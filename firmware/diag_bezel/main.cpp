/*
 * Bezel boundary test — GeekMagic SmallTV Ultra
 *
 * 가장 바깥(0px)부터 5px씩 작아지는 사각형을 그립니다.
 * 4가지 색이 순환 (CYAN→YELLOW→GREEN→MAGENTA).
 * 라벨(size 2)은 10px 간격마다 4개 코너를 돌아가며 표시.
 *
 * 장치를 켜고 "가장 바깥에서 처음으로 완전히 보이는 사각형"의 inset 값을 기록하세요.
 *
 * WiFi 연결 성공 시 /update 엔드포인트로 OTA 복구 가능.
 *
 * 시리얼 플래시:  pio run -e esp8266_bezel -t upload --upload-port /dev/serial0
 * OTA 업데이트:   curl -F "image=@.pio/build/esp8266_bezel/firmware.bin" http://<IP>/update
 */

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>
#include <TFT_eSPI.h>

static TFT_eSPI               tft;
static ESP8266WebServer        server(80);
static ESP8266HTTPUpdateServer updater;

#define W      240
#define H      240
#define BL_PIN   5

static const uint16_t COLS[4] = {
    0x07FF,  // CYAN
    0xFFE0,  // YELLOW
    0x07E0,  // GREEN
    0xF81F,  // MAGENTA
};

// Confirmed bezel margins
#define BZ_TOP     5
#define BZ_LEFT    5
#define BZ_RIGHT   5
#define BZ_BOT    10

static void drawPattern() {
    tft.fillScreen(TFT_BLACK);

    // 안전 영역 안쪽 참조 사각형 (10px 간격, 4색 순환)
    for (int i = 10; i >= 1; i--) {
        int pad = i * 10;
        int x = BZ_LEFT + pad;
        int y = BZ_TOP  + pad;
        int w = W - BZ_LEFT - BZ_RIGHT - pad * 2;
        int h = H - BZ_TOP  - BZ_BOT   - pad * 2;
        if (w > 0 && h > 0)
            tft.drawRect(x, y, w, h, COLS[i % 4]);
    }

    // ── 확정 경계선: 흰색 2px 두께 프레임 ──
    int fx = BZ_LEFT, fy = BZ_TOP;
    int fw = W - BZ_LEFT - BZ_RIGHT;
    int fh = H - BZ_TOP  - BZ_BOT;
    tft.drawRect(fx,     fy,     fw,     fh,     TFT_WHITE);
    tft.drawRect(fx + 1, fy + 1, fw - 2, fh - 2, TFT_WHITE);

    // 각 면 마진 라벨 (size 2)
    tft.setTextSize(2);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);

    // TOP: 경계선 바로 안쪽 상단 중앙
    tft.setCursor(88, fy + 6);
    tft.print("T:5");

    // BOTTOM: 경계선 바로 안쪽 하단 중앙
    tft.setCursor(76, fy + fh - 22);
    tft.print("B:10");

    // LEFT: 경계선 안쪽 좌측 세로 중앙
    tft.setCursor(fx + 4, H / 2 - 8);
    tft.print("L:5");

    // RIGHT: 경계선 안쪽 우측 세로 중앙
    tft.setCursor(fx + fw - 42, H / 2 + 8);
    tft.print("R:5");

    // 중앙 십자선
    tft.drawFastHLine(W / 2 - 10, H / 2, 20, TFT_WHITE);
    tft.drawFastVLine(W / 2, H / 2 - 10, 20, TFT_WHITE);
}

void setup() {
    Serial.begin(115200);
    delay(300);
    Serial.println("\n=== Bezel boundary test (rect mode) ===");

    tft.init();
    tft.setRotation(0);
    tft.fillScreen(TFT_BLACK);
    pinMode(BL_PIN, OUTPUT);
    analogWrite(BL_PIN, 220);

    drawPattern();

    // WiFi — 저장된 크리덴셜로만 연결 (AP 폴백 없음)
    WiFi.mode(WIFI_STA);
    WiFi.begin();
    Serial.print("WiFi connecting");
    unsigned long t = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - t < 10000) {
        delay(300);
        Serial.print('.');
        ESP.wdtFeed();
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("\nIP: %s\n", WiFi.localIP().toString().c_str());
        updater.setup(&server);
        server.on("/", HTTP_GET, []() {
            server.send(200, "text/plain", "bezel-test fw — POST /update to reflash");
        });
        server.begin();

        // IP를 화면 중앙 아래쪽에 표시
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.setTextSize(1);
        tft.setCursor(55, 120);
        tft.print(WiFi.localIP().toString());
    } else {
        Serial.println("\nWiFi not connected");
    }

    Serial.println("Ready.");
    Serial.println("가장 바깥에서 처음으로 완전히 보이는 사각형의 inset 값을 기록하세요.");
    Serial.println("  CYAN=0,20,40...  YELLOW=5,25,45...  GREEN=10,30,50...  MAGENTA=15,35,55...");
}

void loop() {
    server.handleClient();
    ESP.wdtFeed();
    delay(10);
}
