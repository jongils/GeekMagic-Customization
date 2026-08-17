/*
 * Color Evaluation Test — GeekMagic SmallTV Ultra
 *
 * 웹 UI에서 색상을 선택해 TFT에 즉시 표시. 색 비교 목적.
 *
 * HTTP:
 *   GET  /              → 웹 컨트롤 패널 (컬러 피커 + 프리셋 버튼)
 *   GET  /color?c=RRGGBB → 24-bit hex 색상을 TFT에 표시
 *   GET  /compare       → 프리셋 전체를 TFT에 스트립으로 비교 표시
 *   GET  /page?n=1-5    → 기존 테스트 페이지 (그레이스케일 등)
 *   POST /update        → OTA 펌웨어 업데이트
 *
 * Build: pio run -e esp8266_color_diag
 * OTA:   curl -F "image=@.pio/build/esp8266_color_diag/firmware.bin" http://<IP>/update
 */

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>
#include <TFT_eSPI.h>

static TFT_eSPI                tft;
static ESP8266WebServer         server(80);
static ESP8266HTTPUpdateServer  updater;

#define W       240
#define H       240
#define BL_PIN    5
#define PAGES     5

#define SX   5
#define SY   5
#define SW  230
#define SH  225

static int     currentPage = 0;          // 0 = 색상 표시 모드
static uint8_t curR = 127, curG = 64, curB = 0;  // 현재 TFT 색 (기본: 현재 게 색)

// ── Helper ────────────────────────────────────────────────────────────────────

static uint16_t c565(uint8_t r, uint8_t g, uint8_t b) {
    return tft.color565(r, g, b);
}

static uint16_t contrast(uint16_t col) {
    uint8_t r = (col >> 11) << 3;
    uint8_t g = ((col >> 5) & 0x3F) << 2;
    uint8_t b = (col & 0x1F) << 3;
    uint32_t luma = (r * 299UL + g * 587UL + b * 114UL) / 1000;
    return luma > 128 ? TFT_BLACK : TFT_WHITE;
}

// 24-bit hex 문자열 "FF8000" → r,g,b 분해
static bool parseHex(const String &s, uint8_t &r, uint8_t &g, uint8_t &b) {
    if (s.length() < 6) return false;
    r = (uint8_t)strtol(s.substring(0, 2).c_str(), nullptr, 16);
    g = (uint8_t)strtol(s.substring(2, 4).c_str(), nullptr, 16);
    b = (uint8_t)strtol(s.substring(4, 6).c_str(), nullptr, 16);
    return true;
}

static void drawTitle(const char *txt) {
    tft.fillRect(0, 0, W, SY + 14, TFT_BLACK);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(1);
    tft.setCursor(SX, SY);
    tft.print(txt);
    tft.drawFastHLine(SX, SY + 12, SW, TFT_DARKGREY);
}

static void drawFooter() {
    if (WiFi.status() != WL_CONNECTED) return;
    tft.setTextColor(0x4208, TFT_BLACK);
    tft.setTextSize(1);
    tft.setCursor(W - 96, H - 10);
    tft.print(WiFi.localIP().toString());
}

// ── 색상 표시 모드 ────────────────────────────────────────────────────────────
// 선택한 색을 TFT 대부분 영역에 채우고 RGB / RGB565 정보 표시

static void showColor(uint8_t r, uint8_t g, uint8_t b) {
    curR = r; curG = g; curB = b;
    currentPage = 0;

    uint16_t col = c565(r, g, b);
    uint16_t fg  = contrast(col);

    tft.fillScreen(TFT_BLACK);

    // 큰 색상 스와치
    tft.fillRect(0, 0, W, H - 50, col);

    // 정보 패널
    tft.fillRect(0, H - 50, W, 50, TFT_BLACK);
    tft.drawFastHLine(0, H - 50, W, TFT_DARKGREY);

    char buf[40];
    tft.setTextColor(TFT_WHITE, TFT_BLACK);

    // RGB
    tft.setTextSize(2);
    snprintf(buf, sizeof(buf), "R%3d G%3d B%3d", r, g, b);
    tft.setCursor(SX, H - 46);
    tft.print(buf);

    // RGB565 hex
    tft.setTextSize(2);
    snprintf(buf, sizeof(buf), "RGB565: 0x%04X", col);
    tft.setCursor(SX, H - 24);
    tft.print(buf);

    // 색상 위에 hex 라벨
    tft.setTextColor(fg, col);
    tft.setTextSize(3);
    snprintf(buf, sizeof(buf), "#%02X%02X%02X", r, g, b);
    tft.setCursor(W/2 - strlen(buf)*9, (H-50)/2 - 12);
    tft.print(buf);

    drawFooter();
}

// ── 비교 모드: 프리셋 전체를 수평 스트립으로 표시 ──────────────────────────
struct Preset { uint8_t r, g, b; const char *name; };

static const Preset PRESETS[] = {
    // 게 후보
    {197, 105,  66, "Terracotta"},
    {191,  96,   0, "Org 75%"},
    {255, 128,   0, "Orange"},
    {127,  64,   0, "Org 50%*"},
    // 기본색
    {255,   0,   0, "Red"},
    {  0, 255,   0, "Green"},
    {  0,   0, 255, "Blue"},
    {  0, 255, 255, "Cyan"},
    {255,   0, 255, "Magenta"},
    {255, 255,   0, "Yellow"},
    {255, 255, 255, "White"},
    {128, 128, 128, "Gray"},
};
static const int N_PRESETS = sizeof(PRESETS) / sizeof(PRESETS[0]);

static void showCompare() {
    currentPage = -1;
    tft.fillScreen(TFT_BLACK);
    drawTitle("COMPARE — all presets");

    int y0   = SY + 16;
    int bh   = (SH - 18) / N_PRESETS;

    for (int i = 0; i < N_PRESETS; i++) {
        uint16_t col = c565(PRESETS[i].r, PRESETS[i].g, PRESETS[i].b);
        int y = y0 + i * bh;
        tft.fillRect(SX, y, SW, bh - 1, col);

        tft.setTextColor(contrast(col), col);
        tft.setTextSize(1);
        tft.setCursor(SX + 3, y + bh/2 - 4);

        char buf[40];
        snprintf(buf, sizeof(buf), "%-12s R%3d G%3d B%3d",
                 PRESETS[i].name, PRESETS[i].r, PRESETS[i].g, PRESETS[i].b);
        tft.print(buf);
    }
    drawFooter();
}

// ── 기존 테스트 페이지들 ──────────────────────────────────────────────────────

static void drawPage1() {
    tft.fillScreen(TFT_BLACK);
    drawTitle("1/5  GRAYSCALE  16 steps");
    const int STEPS = 16;
    int bw = SW / STEPS, y0 = SY + 16, bh = SH - 30;
    for (int i = 0; i < STEPS; i++) {
        uint8_t v = (uint8_t)(i * 255 / (STEPS - 1));
        uint16_t c = c565(v, v, v);
        int x = SX + i * bw;
        tft.fillRect(x, y0, bw, bh, c);
        if (i % 4 == 0) {
            char buf[5]; snprintf(buf, sizeof(buf), "%d", v);
            tft.setTextColor(contrast(c), c);
            tft.setTextSize(1); tft.setCursor(x + 1, y0 + 4); tft.print(buf);
        }
    }
    drawFooter();
}

static void drawPage2() {
    tft.fillScreen(TFT_BLACK);
    drawTitle("2/5  RGB RAMPS  8 steps each");
    const int STEPS = 8;
    int y0 = SY + 18, bh = (SH - 20) / 3, xOff = 20, bw = (SW - xOff) / STEPS;
    struct { uint8_t r,g,b; uint16_t lbl; char ch; } rows[3] = {
        {1,0,0,TFT_RED,'R'},{0,1,0,TFT_GREEN,'G'},{0,0,1,0x07FF,'B'}
    };
    for (int row = 0; row < 3; row++) {
        int y = y0 + row * bh;
        tft.setTextColor(rows[row].lbl, TFT_BLACK); tft.setTextSize(2);
        tft.setCursor(SX, y + bh/2 - 8); tft.print(rows[row].ch);
        for (int i = 0; i < STEPS; i++) {
            uint8_t v = (uint8_t)((i+1) * 255 / STEPS);
            uint16_t col = c565(v*rows[row].r, v*rows[row].g, v*rows[row].b);
            int x = SX + xOff + i * bw;
            tft.fillRect(x, y, bw-1, bh-2, col);
            char buf[5]; snprintf(buf, sizeof(buf), "%d", v);
            tft.setTextColor(contrast(col), col); tft.setTextSize(1);
            tft.setCursor(x+1, y+2); tft.print(buf);
        }
    }
    drawFooter();
}

static void drawPage3() {
    tft.fillScreen(TFT_BLACK);
    drawTitle("3/5  TFT COLOR PALETTE");
    struct { uint16_t col; const char *name; } colors[16] = {
        {TFT_RED,"RED"},{TFT_GREEN,"GREEN"},{TFT_BLUE,"BLUE"},{TFT_CYAN,"CYAN"},
        {TFT_MAGENTA,"MAGENTA"},{TFT_YELLOW,"YELLOW"},{TFT_WHITE,"WHITE"},{TFT_ORANGE,"ORANGE"},
        {TFT_DARKGREY,"D.GREY"},{TFT_LIGHTGREY,"L.GREY"},{TFT_NAVY,"NAVY"},{TFT_DARKGREEN,"D.GREEN"},
        {TFT_MAROON,"MAROON"},{TFT_PURPLE,"PURPLE"},{TFT_OLIVE,"OLIVE"},{TFT_PINK,"PINK"},
    };
    int bw = SW/4, bh = (SH-18)/4;
    for (int i = 0; i < 16; i++) {
        int x = SX+(i%4)*bw, y = SY+18+(i/4)*bh;
        tft.fillRect(x, y, bw-1, bh-1, colors[i].col);
        tft.setTextColor(contrast(colors[i].col), colors[i].col);
        tft.setTextSize(1); tft.setCursor(x+2, y+2); tft.print(colors[i].name);
    }
    drawFooter();
}

static void drawPage4() {
    tft.fillScreen(TFT_BLACK);
    drawTitle("4/5  CRAB COLOR COMPARE");
    struct { uint16_t col; const char *tag; const char *rgb; bool cur; } sw[4] = {
        {0xC348,"Terracotta  0xC348","R197 G105 B 66",false},
        {0xBB00,"Orange 75%  0xBB00","R191 G 96 B  0",false},
        {0xFC00,"Orange      0xFC00","R255 G128 B  0",false},
        {0x7A00,"Orange 50%  0x7A00","R127 G 64 B  0",true },
    };
    int y0 = SY+18, bh = (SH-20)/4;
    for (int i = 0; i < 4; i++) {
        int y = y0 + i*bh;
        tft.fillRect(SX, y, SW, bh-2, sw[i].col);
        uint16_t fg = contrast(sw[i].col);
        tft.setTextColor(fg, sw[i].col); tft.setTextSize(1);
        tft.setCursor(SX+4, y+4);  tft.print(sw[i].tag);
        tft.setCursor(SX+4, y+14); tft.print(sw[i].rgb);
        if (sw[i].cur) {
            tft.drawRect(SX, y, SW, bh-2, TFT_WHITE);
            tft.drawRect(SX+1, y+1, SW-2, bh-4, TFT_WHITE);
            tft.setTextColor(TFT_WHITE, sw[i].col);
            tft.setCursor(SX+SW-58, y+4); tft.print("<<CURRENT");
        }
    }
    drawFooter();
}

static void drawPage5() {
    tft.fillScreen(TFT_BLACK);
    drawTitle("5/5  WARM-COOL SPECTRUM");
    int y0 = SY+16, gh = 55;
    for (int x = 0; x < SW; x++) {
        float t = (float)x/(SW-1); uint8_t r,g,b;
        if      (t < 0.25f) { float u=t/0.25f;      r=200;g=(uint8_t)(80+175*u);b=0; }
        else if (t < 0.50f) { float u=(t-0.25f)/0.25f; r=200;g=255;b=(uint8_t)(255*u); }
        else if (t < 0.75f) { float u=(t-0.50f)/0.25f; r=(uint8_t)(200*(1-u));g=255;b=255; }
        else                 { float u=(t-0.75f)/0.25f; r=0;g=(uint8_t)(255*(1-u));b=255; }
        tft.drawFastVLine(SX+x, y0, gh, c565(r,g,b));
    }
    const char *lbls[]={"WARM","YEL","WHITE","CYAN","COOL"};
    int pos[]={0,SW/4-10,SW/2-15,3*SW/4-10,SW-28};
    tft.setTextColor(TFT_WHITE,TFT_BLACK); tft.setTextSize(1);
    for (int i=0;i<5;i++){tft.setCursor(SX+pos[i],y0+gh+3);tft.print(lbls[i]);}
    int y1=y0+gh+16; int bw=SW/16;
    tft.setTextColor(TFT_LIGHTGREY,TFT_BLACK); tft.setCursor(SX,y1-10);
    tft.print("Orange brightness ramp:");
    for (int i=0;i<16;i++){
        float t=(float)(i+1)/16;
        uint16_t col=c565((uint8_t)(127*t),(uint8_t)(64*t),0);
        tft.fillRect(SX+i*bw,y1,bw-1,28,col);
        if(i==7) tft.drawRect(SX+i*bw,y1,bw-1,28,TFT_WHITE);
    }
    drawFooter();
}

static void showPage(int n) {
    currentPage = n;
    switch (n) {
        case 1: drawPage1(); break;
        case 2: drawPage2(); break;
        case 3: drawPage3(); break;
        case 4: drawPage4(); break;
        case 5: drawPage5(); break;
    }
}

// ── 웹 컨트롤 패널 HTML ───────────────────────────────────────────────────────

static void handleRoot() {
    // 현재 색 hex 문자열
    char curHex[8];
    snprintf(curHex, sizeof(curHex), "%02X%02X%02X", curR, curG, curB);

    String html = F("<!doctype html><html><head>"
        "<meta charset=utf-8>"
        "<meta name='viewport' content='width=device-width,initial-scale=1'>"
        "<title>Color Eval</title>"
        "<style>"
        "*{box-sizing:border-box;margin:0;padding:0}"
        "body{font:14px sans-serif;background:#111;color:#eee;"
        "padding:16px;display:flex;flex-direction:column;gap:14px;}"
        "h2{color:#FC8000;font-size:17px}"
        "section{background:#1a1a1a;border-radius:8px;padding:12px;}"
        "h3{font-size:12px;color:#888;margin-bottom:8px;letter-spacing:.5px;}"
        ".row{display:flex;flex-wrap:wrap;gap:7px;align-items:center;}"
        "input[type=color]{width:48px;height:48px;border:none;"
        "border-radius:6px;cursor:pointer;padding:2px;background:transparent;}"
        "button{padding:8px 12px;border-radius:6px;border:1px solid #333;"
        "background:#222;color:#ccc;cursor:pointer;font-size:12px;}"
        "button:hover{border-color:#FC8000;color:#FC8000;}"
        "button.active{background:#FC8000;color:#000;border-color:#FC8000;}"
        ".swatch{display:inline-flex;flex-direction:column;align-items:center;"
        "gap:3px;cursor:pointer;}"
        ".swatch div{width:48px;height:36px;border-radius:5px;"
        "border:2px solid #333;}"
        ".swatch div:hover{border-color:#fff;}"
        ".swatch span{font-size:9px;color:#888;text-align:center;}"
        "#applyBtn{background:#FC8000;color:#000;font-weight:bold;padding:10px 20px;}"
        "#curSwatch{width:64px;height:48px;border-radius:6px;border:2px solid #555;}"
        "#curInfo{font-size:11px;color:#888;}"
        "hr{border:none;border-top:1px solid #2a2a2a;}"
        "</style></head><body>"
        "<h2>🦀 Color Eval Test</h2>");

    // ── 색상 피커 섹션 ──
    html += F("<section><h3>COLOR PICKER</h3><div class='row'>"
        "<input type='color' id='picker' value='#");
    html += curHex;
    html += F("'>"
        "<button id='applyBtn' onclick='applyPicker()'>▶ TFT에 표시</button>"
        "<div id='curSwatch' style='background:#");
    html += curHex;
    html += F(";'></div>"
        "<span id='curInfo'>#");
    html += curHex;
    html += F("<br>RGB565: 0x");
    char rgb565buf[8];
    snprintf(rgb565buf, sizeof(rgb565buf), "%04X", c565(curR, curG, curB));
    html += rgb565buf;
    html += F("</span></div></section>");

    // ── 프리셋: 게 후보 ──
    html += F("<section><h3>🦀 CRAB CANDIDATES</h3><div class='row'>");
    struct { uint8_t r,g,b; const char *name; const char *hex565; } crab[] = {
        {197,105, 66,"Terracotta","C348"},
        {191, 96,  0,"Org 75%",  "BB00"},
        {255,128,  0,"Orange",   "FC00"},
        {127, 64,  0,"Org 50%",  "7A00"},
    };
    for (auto &p : crab) {
        char hex[8], style[32];
        snprintf(hex,   sizeof(hex),   "%02X%02X%02X", p.r, p.g, p.b);
        snprintf(style, sizeof(style), "background:#%s", hex);
        html += "<div class='swatch' onclick='sendColor(\"";
        html += hex;
        html += "\")'><div style='";
        html += style;
        html += ";'></div><span>";
        html += p.name;
        html += "<br>0x";
        html += p.hex565;
        html += "</span></div>";
    }
    html += F("</div></section>");

    // ── 프리셋: 기본색 ──
    html += F("<section><h3>BASIC COLORS</h3><div class='row'>");
    struct { uint8_t r,g,b; const char *name; } basic[] = {
        {255,  0,  0,"Red"},   {  0,255,  0,"Green"}, {  0,  0,255,"Blue"},
        {  0,255,255,"Cyan"},  {255,  0,255,"Magenta"},{255,255,  0,"Yellow"},
        {255,255,255,"White"},  {128,128,128,"Gray"},   {  0,  0,  0,"Black"},
        {255,128,  0,"Orange"}, {255,165,  0,"Gold"},   { 75,  0,130,"Indigo"},
    };
    for (auto &p : basic) {
        char hex[8], style[40], border[40];
        snprintf(hex,    sizeof(hex),    "%02X%02X%02X", p.r, p.g, p.b);
        snprintf(style,  sizeof(style),  "background:#%s", hex);
        bool isBlack = (p.r == 0 && p.g == 0 && p.b == 0);
        snprintf(border, sizeof(border), "%s;border-color:%s",
                 style, isBlack ? "#555" : hex);
        html += "<div class='swatch' onclick='sendColor(\"";
        html += hex;
        html += "\")'><div style='";
        html += border;
        html += ";'></div><span>";
        html += p.name;
        html += "</span></div>";
    }
    html += F("</div></section>");

    // ── 비교 / 테스트 페이지 버튼 ──
    html += F("<section><h3>TEST PAGES</h3><div class='row'>");
    html += F("<button onclick=\"location='/compare'\">⬛ Compare All</button>");
    const char *pgNames[] = {"Grayscale","RGB Ramps","TFT Palette","Crab Colors","Warm-Cool"};
    for (int i = 1; i <= PAGES; i++) {
        html += "<button onclick=\"location='/page?n=";
        html += i;
        html += "'\">";
        html += i;
        html += " · ";
        html += pgNames[i-1];
        html += "</button>";
    }
    html += F("</div></section>");

    // ── JS ──
    html += F("<script>"
        "const pk=document.getElementById('picker');"
        "const sw=document.getElementById('curSwatch');"
        "const info=document.getElementById('curInfo');"
        "function sendColor(hex){"
        "  fetch('/color?c='+hex).then(r=>r.text()).then(t=>{"
        "    sw.style.background='#'+hex;"
        "    info.innerHTML=t;"
        "    pk.value='#'+hex;"
        "  });"
        "}"
        "function applyPicker(){"
        "  sendColor(pk.value.replace('#',''));"
        "}"
        "pk.addEventListener('input',()=>{"
        "  sw.style.background=pk.value;"
        "});"
        "</script>"
        "</body></html>");

    server.send(200, "text/html", html);
}

// ── /color 핸들러 ─────────────────────────────────────────────────────────────
static void handleColor() {
    if (!server.hasArg("c")) { server.send(400, "text/plain", "missing ?c=RRGGBB"); return; }
    uint8_t r, g, b;
    if (!parseHex(server.arg("c"), r, g, b)) {
        server.send(400, "text/plain", "bad hex"); return;
    }
    showColor(r, g, b);

    // JS에 표시할 정보 텍스트 반환
    char buf[60];
    snprintf(buf, sizeof(buf), "#%02X%02X%02X&lt;br&gt;R%d G%d B%d&lt;br&gt;RGB565: 0x%04X",
             r, g, b, r, g, b, c565(r, g, b));
    server.send(200, "text/plain", buf);
}

// ── setup ─────────────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    delay(300);
    Serial.println("\n=== Color Eval Test ===");

    tft.init();
    tft.setRotation(0);
    tft.fillScreen(TFT_BLACK);
    pinMode(BL_PIN, OUTPUT);
    analogWrite(BL_PIN, 220);

    showColor(curR, curG, curB);   // 초기 화면: 현재 게 색

    WiFi.mode(WIFI_STA);
    WiFi.begin();
    Serial.print("WiFi connecting");
    unsigned long t0 = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - t0 < 10000) {
        delay(300); Serial.print('.'); ESP.wdtFeed();
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("\nIP: %s\n", WiFi.localIP().toString().c_str());

        updater.setup(&server);
        server.on("/",        HTTP_GET, handleRoot);
        server.on("/color",   HTTP_GET, handleColor);
        server.on("/compare", HTTP_GET, []() { showCompare(); server.sendHeader("Location","/"); server.send(302,"text/plain",""); });
        server.on("/page",    HTTP_GET, []() {
            int n = server.hasArg("n") ? server.arg("n").toInt() : 1;
            if (n < 1 || n > PAGES) { server.send(400,"text/plain","n=1-5"); return; }
            showPage(n);
            server.sendHeader("Location","/"); server.send(302,"text/plain","");
        });
        server.begin();

        showColor(curR, curG, curB);   // IP 포함 재표시
    } else {
        Serial.println("\nWiFi not connected");
    }
    Serial.println("Ready.");
}

// ── loop ──────────────────────────────────────────────────────────────────────
void loop() {
    server.handleClient();
    ESP.wdtFeed();
    delay(10);
}
