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
#include "weather_theme.h"

// ── NTP ───────────────────────────────────────────────────────────────────────

static WiFiUDP   _ntpUDP;
static NTPClient _ntp(_ntpUDP, NTP_SERVER, NTP_OFFSET_SEC, 60000);

// Sync POSIX time from NTPClient so time() / localtime_r() work correctly
static void syncPosixTime() {
    _ntp.update();
    unsigned long epoch = _ntp.getEpochTime();
    if (epoch > 1000000000UL) {
        struct timeval tv = { (time_t)epoch, 0 };
        settimeofday(&tv, nullptr);
    }
}

// ── clock ticker (1-second interrupt) ────────────────────────────────────────

static Ticker    _clockTicker;
static volatile bool _clockTick = false;

static void IRAM_ATTR onClockTick() { _clockTick = true; }

// ── WiFi setup ────────────────────────────────────────────────────────────────

static void wifiSetup() {
    displayFill(TFT_BLACK);
    displayTextCentre(0, 110, DISPLAY_W, "WiFi Setup...", COL_GREY, 1);

    WiFiManager wm;
    wm.setConfigPortalTimeout(120);

    // Called while config portal is active — show AP instructions
    wm.setAPCallback([](WiFiManager *wm) {
        displayFill(TFT_BLACK);
        displayTextCentre(0,  50, DISPLAY_W, "Connect to WiFi:", COL_WHITE, 1);
        displayTextCentre(0,  72, DISPLAY_W, WIFI_AP_NAME,       COL_CYAN,  2);
        displayTextCentre(0, 110, DISPLAY_W, "Then open:",       COL_GREY,  1);
        displayTextCentre(0, 130, DISPLAY_W, "192.168.4.1",     COL_WHITE, 1);
    });

    if (!wm.autoConnect(WIFI_AP_NAME)) {
        displayTextCentre(0, 120, DISPLAY_W, "WiFi failed!", COL_RED, 2);
        delay(3000);
        ESP.restart();
    }

    displayFill(TFT_BLACK);
    displayTextCentre(0, 100, DISPLAY_W, "WiFi OK",      COL_GREEN, 2);
    displayTextCentre(0, 126, DISPLAY_W, WiFi.localIP().toString().c_str(),
                      COL_WHITE, 1);
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

    // NTP
    _ntp.begin();
    syncPosixTime();

    // HTTP server
    httpServerInit();

    // Clock ticker: fires every 1 second
    _clockTicker.attach(1.0f, onClockTick);

    clockThemeInit();

    // Restore last display state
    uint8_t theme = httpGetTheme();
    if (theme == THEME_PHOTO_ALBUM && fsExists(UPLOAD_PATH)) {
        jpegDisplayFile(UPLOAD_PATH);
    } else {
        displayFill(TFT_BLACK);
    }

    LOG("Setup complete. Theme=%d  Free heap=%u\n", theme, ESP.getFreeHeap());
}

// ── loop ──────────────────────────────────────────────────────────────────────

void loop() {
    httpServerHandle();
    ESP.wdtFeed();

    // NTP re-sync every 60 s (NTPClient handles the interval internally)
    syncPosixTime();

    // Weather data refresh (non-blocking; skips if cache is fresh)
    uint8_t theme = httpGetTheme();
    bool isWeatherTheme = (theme == THEME_WEATHER_CLOCK    ||
                           theme == THEME_WEATHER_FORECAST  ||
                           theme == THEME_SIMPLE_WEATHER);
    if (isWeatherTheme) weatherUpdate();

    // Per-second render for clock/weather themes
    if (_clockTick) {
        _clockTick = false;

        switch (theme) {
            case THEME_CLOCK_1:
            case THEME_CLOCK_2:
            case THEME_CLOCK_3:
                clockThemeRender(theme);
                break;

            case THEME_WEATHER_CLOCK:
            case THEME_WEATHER_FORECAST:
            case THEME_SIMPLE_WEATHER:
                if (weatherDataValid()) weatherThemeRender(theme);
                else clockThemeRender(THEME_CLOCK_1); // fallback until data arrives
                break;

            case THEME_PHOTO_ALBUM:
                // Image updates are driven by HTTP push; no periodic render needed
                break;
        }
    }

    yield();
}
