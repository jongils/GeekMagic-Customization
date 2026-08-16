/*
 * Bezel boundary test — GeekMagic SmallTV Ultra
 *
 * 4면 각각 안쪽으로 5px 간격으로 선을 그립니다.
 *  TOP    → CYAN  수평선, y=0 부터 y=120
 *  BOTTOM → MAGENTA 수평선, y=239 부터 y=120
 *  LEFT   → GREEN 수직선, x=0 부터 x=120
 *  RIGHT  → YELLOW 수직선, x=239 부터 x=120
 *
 * 밝은 선(10px 단위)에는 좌표값 레이블을 표시합니다.
 * 장치를 켜고 각 면에서 "처음으로 보이는 선"의 좌표를 기록하세요.
 *
 * 플래시:  pio run -e esp8266_bezel -t upload --upload-port /dev/serial0
 * OTA:     pio run -e esp8266_bezel_ota -t upload
 */

#include <Arduino.h>
#include <TFT_eSPI.h>

static TFT_eSPI tft;

#define W      240
#define H      240
#define BL_PIN   5
#define STEP     5    // 선 간격 (px)
#define DEPTH   25    // 각 면에서 안쪽으로 그릴 선 수 (25 × 5 = 125px)

// 밝은 색 / 어두운 색 쌍 — 짝수 인덱스=밝음, 홀수=어두움
static const uint16_t COL_TOP_HI  = 0x07FF;  // CYAN
static const uint16_t COL_TOP_LO  = 0x0310;  // dim cyan
static const uint16_t COL_BOT_HI  = 0xF81F;  // MAGENTA
static const uint16_t COL_BOT_LO  = 0x4008;  // dim magenta
static const uint16_t COL_LFT_HI  = 0x07E0;  // GREEN
static const uint16_t COL_LFT_LO  = 0x0200;  // dim green
static const uint16_t COL_RGT_HI  = 0xFFE0;  // YELLOW
static const uint16_t COL_RGT_LO  = 0x8400;  // dim yellow

static void label(int x, int y, int val, uint16_t bg) {
    char buf[6];
    snprintf(buf, sizeof(buf), "%d", val);
    tft.setTextColor(TFT_WHITE, bg);
    tft.setTextSize(1);
    tft.setCursor(x, y);
    tft.print(buf);
}

void setup() {
    Serial.begin(115200);
    delay(300);
    Serial.println("\n=== Bezel boundary test ===");

    analogWrite(BL_PIN, 220);
    tft.init();
    tft.setRotation(0);
    tft.fillScreen(TFT_BLACK);

    // ── TOP: horizontal lines y=0..120, labels on LEFT side (x=3) ─────────
    for (int i = 0; i < DEPTH; i++) {
        int y = i * STEP;
        uint16_t c = (i % 2 == 0) ? COL_TOP_HI : COL_TOP_LO;
        tft.drawFastHLine(0, y, W, c);
        if (i % 2 == 0) {   // every 10px
            label(3, y + 1, y, COL_TOP_HI);
        }
    }

    // ── BOTTOM: horizontal lines y=239..115, labels on RIGHT side (x=210) ─
    for (int i = 0; i < DEPTH; i++) {
        int y = H - 1 - i * STEP;
        uint16_t c = (i % 2 == 0) ? COL_BOT_HI : COL_BOT_LO;
        tft.drawFastHLine(0, y, W, c);
        if (i % 2 == 0) {
            label(195, y - 8, y, COL_BOT_HI);
        }
    }

    // ── LEFT: vertical lines x=0..120, labels at y=110 (above center) ────
    for (int i = 0; i < DEPTH; i++) {
        int x = i * STEP;
        uint16_t c = (i % 2 == 0) ? COL_LFT_HI : COL_LFT_LO;
        tft.drawFastVLine(x, 0, H, c);
        if (i % 2 == 0) {
            label(x + 1, 108, x, COL_LFT_HI);
        }
    }

    // ── RIGHT: vertical lines x=239..115, labels at y=125 (below center) ─
    for (int i = 0; i < DEPTH; i++) {
        int x = W - 1 - i * STEP;
        uint16_t c = (i % 2 == 0) ? COL_RGT_HI : COL_RGT_LO;
        tft.drawFastVLine(x, 0, H, c);
        if (i % 2 == 0) {
            int lx = (x >= 18) ? x - 18 : x + 2;
            label(lx, 125, x, COL_RGT_HI);
        }
    }

    // ── 4 corner markers: 4×4 white squares ───────────────────────────────
    tft.fillRect(0,     0,     4, 4, TFT_WHITE);
    tft.fillRect(W - 4, 0,     4, 4, TFT_WHITE);
    tft.fillRect(0,     H - 4, 4, 4, TFT_WHITE);
    tft.fillRect(W - 4, H - 4, 4, 4, TFT_WHITE);

    // ── Center crosshair ───────────────────────────────────────────────────
    tft.drawFastHLine(W/2 - 15, H/2, 30, TFT_WHITE);
    tft.drawFastVLine(W/2, H/2 - 15, 30, TFT_WHITE);
    label(W/2 + 3, H/2 + 3, 120, TFT_BLACK);

    Serial.println("Pattern drawn. Observe device and note first visible line per edge.");
    Serial.println("  TOP    (CYAN)    : read y from LEFT labels");
    Serial.println("  BOTTOM (MAGENTA) : read y from RIGHT labels");
    Serial.println("  LEFT   (GREEN)   : read x from UPPER-CENTER labels");
    Serial.println("  RIGHT  (YELLOW)  : read x from LOWER-CENTER labels");
}

void loop() {
    ESP.wdtFeed();
    delay(5000);
}
