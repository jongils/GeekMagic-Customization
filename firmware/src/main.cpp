#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WiFiManager.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <Ticker.h>

#include "config.h"
#include "display.h"
#include "filesystem.h"
#include "http_server.h"
#include "jpeg_display.h"
#include "clock_theme.h"

// ── NTP ───────────────────────────────────────────────────────────────────────

static WiFiUDP   _ntpUDP;
static NTPClient _ntp(_ntpUDP, NTP_SERVER, NTP_OFFSET_SEC, 60000);

static void syncPosixTime() {
    _ntp.update();
    unsigned long epoch = _ntp.getEpochTime();
    if (epoch > 1000000000UL) {
        struct timeval tv = { (time_t)epoch, 0 };
        settimeofday(&tv, nullptr);
    }
}

// ── 1-second ticker ───────────────────────────────────────────────────────────

static Ticker         _clockTicker;
static volatile bool  _clockTick = false;
static void IRAM_ATTR onClockTick() { _clockTick = true; }

// ── WiFi setup ────────────────────────────────────────────────────────────────

static void wifiSetup() {
    displayFill(TFT_BLACK);
    displayTextCentre(0, 110, DISPLAY_W, "WiFi Setup...", COL_GREY, 1);

    WiFiManager wm;
    wm.setConfigPortalTimeout(120);
    wm.setAPCallback([](WiFiManager *) {
        displayFill(TFT_BLACK);
        displayTextCentre(SAFE_X,  80, SAFE_W, "Connect to WiFi:", COL_WHITE, 1);
        displayTextCentre(SAFE_X, 100, SAFE_W, WIFI_AP_NAME,       COL_CYAN,  2);
        displayTextCentre(SAFE_X, 138, SAFE_W, "then open",        COL_GREY,  1);
        displayTextCentre(SAFE_X, 155, SAFE_W, "192.168.4.1",      COL_WHITE, 1);
    });

    if (!wm.autoConnect(WIFI_AP_NAME)) {
        displayTextCentre(SAFE_X, 110, SAFE_W, "WiFi failed!", COL_RED, 2);
        delay(3000);
        ESP.restart();
    }

    displayFill(TFT_BLACK);
    displayTextCentre(SAFE_X,  95, SAFE_W, "WiFi OK",      COL_GREEN, 2);
    displayTextCentre(SAFE_X, 121, SAFE_W, WiFi.localIP().toString().c_str(), COL_WHITE, 1);
    delay(1500);
}

// ── setup ─────────────────────────────────────────────────────────────────────

void setup() {
#if DEBUG
    Serial.begin(115200);
    delay(200);
    LOG("\n\n=== GeekMagic Custom FW %s ===\n", FW_VERSION);
    LOG("Free heap at boot: %u\n", ESP.getFreeHeap());
#endif

    displayInit();
    displayBootSplash();

    if (!fsInit()) {
        displayTextCentre(0, 150, DISPLAY_W, "FS error!", COL_RED, 1);
        delay(3000);
    }

    wifiSetup();

    _ntp.begin();
    syncPosixTime();

    httpServerInit();
    _clockTicker.attach(1.0f, onClockTick);
    clockThemeInit();
    displayFill(TFT_BLACK);

    LOG("Setup done. IP=%s  heap=%u\n",
        WiFi.localIP().toString().c_str(), ESP.getFreeHeap());
}

// ── loop ──────────────────────────────────────────────────────────────────────

static DisplayMode _lastMode = MODE_CLOCK;

void loop() {
    httpServerHandle();
    ESP.wdtFeed();
    syncPosixTime();

    DisplayMode mode = httpGetDisplayMode();

    // Detect revert to clock → reinitialise so it redraws immediately
    if (mode == MODE_CLOCK && _lastMode != MODE_CLOCK) {
        clockThemeInit();
        displayFill(TFT_BLACK);
    }
    _lastMode = mode;

    // Auto-revert when a timed /draw command expires
    if (httpCheckModeTimeout()) {
        // httpCheckModeTimeout() already set mode back to MODE_CLOCK;
        // next iteration will trigger the reinit above
    }

    if (_clockTick) {
        _clockTick = false;
        if (mode == MODE_CLOCK) {
            clockThemeRender(THEME_CLOCK_1);
        }
        // MODE_DRAW / MODE_JPEG: display managed by /draw or /display
    }

    if (mode == MODE_CLOCK) {
        clockAnimUpdate();   // smooth icon animation (~60 fps, millis-based)
    }

    yield();
}
