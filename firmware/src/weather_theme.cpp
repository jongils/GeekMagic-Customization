#include "weather_theme.h"
#include "clock_theme.h"
#include "display.h"
#include "config.h"
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <FS.h>
#include <LittleFS.h>
#include <time.h>

// ── persisted config ──────────────────────────────────────────────────────────

static char _apiKey[64]  = "";
static char _city[64]    = "Seoul";

void weatherSetConfig(const char *apiKey, const char *city) {
    strncpy(_apiKey, apiKey, sizeof(_apiKey) - 1);
    strncpy(_city,   city,   sizeof(_city)   - 1);

    File f = LittleFS.open("/weather.json", "w");
    if (!f) return;
    JsonDocument doc;
    doc["apiKey"] = _apiKey;
    doc["city"]   = _city;
    serializeJson(doc, f);
    f.close();
}

static void weatherLoadConfig() {
    File f = LittleFS.open("/weather.json", "r");
    if (!f) return;
    JsonDocument doc;
    deserializeJson(doc, f);
    f.close();
    String k = doc["apiKey"] | "";
    String c = doc["city"]   | "Seoul";
    strncpy(_apiKey, k.c_str(), sizeof(_apiKey) - 1);
    strncpy(_city,   c.c_str(), sizeof(_city)   - 1);
}

// ── cached weather data ───────────────────────────────────────────────────────

struct WeatherData {
    char     description[48];
    char     city[48];
    int16_t  tempC;        // Celsius * 1 (integer)
    int16_t  feelsLike;
    uint8_t  humidity;
    uint8_t  windSpeed;    // m/s
    uint16_t weatherId;    // OpenWeatherMap condition code
    bool     valid;
};

static WeatherData _wx = {};
static unsigned long _lastFetchMs = 0;

bool weatherDataValid() { return _wx.valid; }

// ── OpenWeatherMap fetch ──────────────────────────────────────────────────────

static bool fetchWeather() {
    if (strlen(_apiKey) == 0) {
        LOG("weatherFetch: no API key configured\n");
        return false;
    }

    LOG("weatherFetch: fetching for city=%s (heap=%u)\n",
        _city, ESP.getFreeHeap());

    // HTTPS client in a tight scope so BearSSL frees ~20 KB on exit
    bool ok = false;
    {
        BearSSL::WiFiClientSecure client;
        client.setInsecure();   // skip cert validation (saves ~10 KB)
        client.setTimeout(8000);

        if (!client.connect("api.openweathermap.org", 443)) {
            LOG("weatherFetch: connect failed\n");
            return false;
        }

        char path[256];
        snprintf(path, sizeof(path),
            "/data/2.5/weather?q=%s&appid=%s&units=metric&lang=kr",
            _city, _apiKey);

        client.printf("GET %s HTTP/1.1\r\nHost: api.openweathermap.org\r\n"
                      "Connection: close\r\n\r\n", path);

        // Skip HTTP headers
        while (client.connected()) {
            String line = client.readStringUntil('\n');
            if (line == "\r") break;
        }

        // Stream-parse only the fields we need (saves heap vs readString)
        JsonDocument filter;
        filter["name"]                    = true;
        filter["main"]["temp"]            = true;
        filter["main"]["feels_like"]      = true;
        filter["main"]["humidity"]        = true;
        filter["wind"]["speed"]           = true;
        filter["weather"][0]["id"]        = true;
        filter["weather"][0]["description"] = true;

        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, client,
            DeserializationOption::Filter(filter));

        if (err) {
            LOG("weatherFetch: JSON parse error: %s\n", err.c_str());
        } else {
            strncpy(_wx.city,        doc["name"] | _city,              sizeof(_wx.city) - 1);
            strncpy(_wx.description, doc["weather"][0]["description"] | "N/A",
                    sizeof(_wx.description) - 1);
            _wx.tempC     = (int16_t)(doc["main"]["temp"].as<float>());
            _wx.feelsLike = (int16_t)(doc["main"]["feels_like"].as<float>());
            _wx.humidity  = (uint8_t)(doc["main"]["humidity"].as<int>());
            _wx.windSpeed = (uint8_t)(doc["wind"]["speed"].as<float>());
            _wx.weatherId = doc["weather"][0]["id"] | 800;
            _wx.valid = true;
            ok = true;
            LOG("weatherFetch: OK temp=%d desc=%s\n", _wx.tempC, _wx.description);
        }
    } // BearSSL released here

    LOG("weatherFetch: done (heap=%u)\n", ESP.getFreeHeap());
    return ok;
}

void weatherUpdate() {
    if (strlen(_apiKey) == 0) {
        if (_lastFetchMs == 0) weatherLoadConfig();
        return;
    }

    unsigned long now = millis();
    if (_lastFetchMs != 0 && (now - _lastFetchMs) < WEATHER_INTERVAL_MS) return;

    if (fetchWeather()) _lastFetchMs = now;
    else if (_lastFetchMs == 0)  _lastFetchMs = now; // avoid tight retry loop
}

// ── weather icon (geometric, no font dependency) ──────────────────────────────

static void drawWeatherIcon(int16_t cx, int16_t cy, uint16_t id) {
    // id groups: 2xx thunderstorm, 3xx drizzle, 5xx rain, 6xx snow,
    //            7xx atmosphere, 800 clear, 80x clouds
    if (id == 800) {
        // Sun: circle + 8 rays
        tft.fillCircle(cx, cy, 18, TFT_YELLOW);
        for (int i = 0; i < 8; i++) {
            float angle = i * PI / 4.0f;
            int x1 = cx + (int)(cos(angle) * 22);
            int y1 = cy + (int)(sin(angle) * 22);
            int x2 = cx + (int)(cos(angle) * 30);
            int y2 = cy + (int)(sin(angle) * 30);
            tft.drawLine(x1, y1, x2, y2, TFT_YELLOW);
        }
    } else if (id >= 801 && id <= 804) {
        // Cloud
        tft.fillCircle(cx - 8, cy + 4, 12, COL_GREY);
        tft.fillCircle(cx + 8, cy + 4, 10, COL_GREY);
        tft.fillCircle(cx,     cy - 2, 14, COL_WHITE);
        tft.fillRect(cx - 18, cy + 4, 36, 12, COL_GREY);
    } else if (id >= 500 && id < 600) {
        // Rain: cloud + drops
        tft.fillCircle(cx, cy - 8, 14, COL_GREY);
        tft.fillRect(cx - 14, cy - 8, 28, 12, COL_GREY);
        for (int d = 0; d < 3; d++) {
            int rx = cx - 10 + d * 10;
            tft.drawLine(rx, cy + 8, rx - 4, cy + 20, TFT_CYAN);
        }
    } else if (id >= 600 && id < 700) {
        // Snow: cloud + asterisks
        tft.fillCircle(cx, cy - 8, 14, COL_GREY);
        tft.fillRect(cx - 14, cy - 8, 28, 12, COL_GREY);
        for (int d = 0; d < 3; d++) {
            int rx = cx - 10 + d * 10;
            tft.drawLine(rx - 4, cy + 8, rx + 4, cy + 20, COL_WHITE);
            tft.drawLine(rx + 4, cy + 8, rx - 4, cy + 20, COL_WHITE);
        }
    } else if (id >= 200 && id < 300) {
        // Thunderstorm: cloud + bolt
        tft.fillCircle(cx, cy - 8, 14, COL_GREY);
        tft.fillRect(cx - 14, cy - 8, 28, 12, COL_GREY);
        // lightning bolt
        tft.fillTriangle(cx + 4, cy + 5, cx - 8, cy + 18, cx + 2, cy + 18, TFT_YELLOW);
        tft.fillTriangle(cx - 4, cy + 18, cx + 8, cy + 5, cx + 2, cy + 18, TFT_YELLOW);
    } else {
        // Generic: question mark placeholder
        displayTextCentre(cx - 20, cy - 10, 40, "?", COL_GREY, 2);
    }
}

// ── render helpers ────────────────────────────────────────────────────────────

static void drawTempStr(int16_t x, int16_t y, int16_t tempC, uint8_t sz) {
    char buf[8];
    snprintf(buf, sizeof(buf), "%d C", tempC);
    // Replace 'C' with degree symbol workaround (font may lack °)
    displayText(x, y, buf, COL_ORANGE, sz);
}

// ── Theme 1: Weather Clock Today ─────────────────────────────────────────────

static int8_t _lastTheme1Min = -1;

static void renderTheme1() {
    time_t now = time(nullptr);
    struct tm t;
    localtime_r(&now, &t);

    // Full redraw on first call or minute change
    if (t.tm_min != _lastTheme1Min) {
        tft.fillScreen(TFT_BLACK);
        _lastTheme1Min = t.tm_min;

        // City
        displayTextCentre(0, 8, DISPLAY_W, _wx.city, COL_WHITE, 2);
        displayHLine(20, 34, DISPLAY_W - 40, COL_GREY);

        // Time
        char timeBuf[6];
        snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d", t.tm_hour, t.tm_min);
        displayTextCentre(0, 40, DISPLAY_W, timeBuf, COL_CYAN, 4);

        // Weather icon (left half)
        drawWeatherIcon(60, 140, _wx.weatherId);

        // Temperature (right half)
        drawTempStr(130, 118, _wx.tempC, 3);
        char humBuf[12];
        snprintf(humBuf, sizeof(humBuf), "H: %d%%", _wx.humidity);
        displayText(130, 158, humBuf, COL_GREY, 1);
        char windBuf[14];
        snprintf(windBuf, sizeof(windBuf), "W: %dm/s", _wx.windSpeed);
        displayText(130, 174, windBuf, COL_GREY, 1);

        displayHLine(20, 196, DISPLAY_W - 40, COL_GREY);
        displayTextCentre(0, 204, DISPLAY_W, _wx.description, COL_WHITE, 1);
    }
}

// ── Theme 2: Weather Forecast (simplified — current + feels like) ─────────────

static void renderTheme2() {
    tft.fillScreen(TFT_BLACK);
    displayTextCentre(0, 8,  DISPLAY_W, _wx.city,        COL_WHITE, 2);
    displayHLine(20, 32, DISPLAY_W - 40, COL_GREY);

    drawWeatherIcon(120, 90, _wx.weatherId);

    char buf[24];
    snprintf(buf, sizeof(buf), "Temp:   %d C",        _wx.tempC);
    displayText(20, 150, buf, COL_ORANGE, 1);
    snprintf(buf, sizeof(buf), "Feels:  %d C",        _wx.feelsLike);
    displayText(20, 167, buf, COL_GREY, 1);
    snprintf(buf, sizeof(buf), "Humidity: %d%%",      _wx.humidity);
    displayText(20, 184, buf, COL_CYAN, 1);
    snprintf(buf, sizeof(buf), "Wind:   %d m/s",      _wx.windSpeed);
    displayText(20, 201, buf, COL_GREY, 1);

    displayTextCentre(0, 218, DISPLAY_W, _wx.description, COL_WHITE, 1);
}

// ── Theme 7: Simple Weather Clock ────────────────────────────────────────────

static int8_t _lastTheme7Min = -1;

static void renderTheme7() {
    time_t now = time(nullptr);
    struct tm t;
    localtime_r(&now, &t);

    if (t.tm_min != _lastTheme7Min) {
        tft.fillScreen(TFT_BLACK);
        _lastTheme7Min = t.tm_min;

        char timeBuf[6];
        snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d", t.tm_hour, t.tm_min);
        displayTextCentre(0, 88, DISPLAY_W, timeBuf, COL_WHITE, 5);

        char tempBuf[8];
        snprintf(tempBuf, sizeof(tempBuf), "%d C", _wx.tempC);
        displayTextCentre(0, 160, DISPLAY_W, tempBuf, COL_ORANGE, 2);
        displayTextCentre(0, 186, DISPLAY_W, _wx.description, COL_GREY, 1);
    }
}

// ── public entry point ────────────────────────────────────────────────────────

void weatherThemeRender(uint8_t theme) {
    if (!_wx.valid) {
        displayTextCentre(0, 100, DISPLAY_W, "Fetching weather...", COL_GREY, 1);
        displayTextCentre(0, 120, DISPLAY_W, _city, COL_WHITE, 1);
        return;
    }

    switch (theme) {
        case THEME_WEATHER_CLOCK:   renderTheme1(); break;
        case THEME_WEATHER_FORECAST:renderTheme2(); break;
        case THEME_SIMPLE_WEATHER:  renderTheme7(); break;
        default:                    renderTheme1(); break;
    }
}
