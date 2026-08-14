#include "clock_theme.h"
#include "display.h"
#include "config.h"
#include <Arduino.h>
#include <time.h>

// Cache last-rendered values to minimise redraws
static int8_t _lastHour   = -1;
static int8_t _lastMin    = -1;
static int8_t _lastSec    = -1;
static int8_t _lastWday   = -1;

static const char *WDAY_KR[] = {
    "일요일","월요일","화요일","수요일","목요일","금요일","토요일"
};
static const char *WDAY_EN[] = {
    "SUN","MON","TUE","WED","THU","FRI","SAT"
};

void clockThemeInit() {
    _lastHour = _lastMin = _lastSec = _lastWday = -1;
}

bool clockTimeValid() {
    time_t now = time(nullptr);
    return now > 1000000000UL;   // sanity check: past year 2001
}

// ── helpers ──────────────────────────────────────────────────────────────────

static void drawTwoDigit(int16_t x, int16_t y, int val,
                         uint16_t colour, uint8_t sz) {
    char buf[4];
    snprintf(buf, sizeof(buf), "%02d", val);
    // Erase old number first to avoid ghost pixels
    displayRect(x, y, sz * 12 * 2, sz * 16, TFT_BLACK);
    displayText(x, y, buf, colour, sz);
}

// ── Theme 4: large digital clock + date (default) ────────────────────────────

static void renderTheme4(const struct tm &t) {
    // Hour and minute (large, centre-stage)
    if (t.tm_hour != _lastHour || t.tm_min != _lastMin) {
        char buf[6];
        snprintf(buf, sizeof(buf), "%02d:%02d", t.tm_hour, t.tm_min);
        displayRect(0, 70, DISPLAY_W, 80, TFT_BLACK);
        displayTextCentre(0, 75, DISPLAY_W, buf, COL_CYAN, 5);
        _lastHour = t.tm_hour;
        _lastMin  = t.tm_min;
    }

    // Seconds (smaller, below)
    if (t.tm_sec != _lastSec) {
        char buf[4];
        snprintf(buf, sizeof(buf), ":%02d", t.tm_sec);
        displayRect(0, 158, DISPLAY_W, 28, TFT_BLACK);
        displayTextCentre(0, 158, DISPLAY_W, buf, COL_GREY, 2);
        _lastSec = t.tm_sec;
    }

    // Date line (updates once per day)
    if (t.tm_wday != _lastWday) {
        char dateBuf[32];
        snprintf(dateBuf, sizeof(dateBuf), "%d.%02d.%02d %s",
                 1900 + t.tm_year, t.tm_mon + 1, t.tm_mday,
                 WDAY_KR[t.tm_wday]);
        displayRect(0, 30, DISPLAY_W, 36, TFT_BLACK);
        displayTextCentre(0, 35, DISPLAY_W, dateBuf, COL_WHITE, 1);
        displayHLine(20, 65, DISPLAY_W - 40, COL_GREY);
        _lastWday = t.tm_wday;
    }
}

// ── Theme 5: two-line style ───────────────────────────────────────────────────

static void renderTheme5(const struct tm &t) {
    // HH:MM on line 1
    if (t.tm_hour != _lastHour || t.tm_min != _lastMin) {
        char buf[6];
        snprintf(buf, sizeof(buf), "%02d:%02d", t.tm_hour, t.tm_min);
        displayRect(0, 55, DISPLAY_W, 72, TFT_BLACK);
        displayTextCentre(0, 60, DISPLAY_W, buf, COL_YELLOW, 4);
        _lastHour = t.tm_hour;
        _lastMin  = t.tm_min;
    }

    // SS on line 2
    if (t.tm_sec != _lastSec) {
        char buf[12];
        snprintf(buf, sizeof(buf), "-- %02d --", t.tm_sec);
        displayRect(0, 135, DISPLAY_W, 36, TFT_BLACK);
        displayTextCentre(0, 138, DISPLAY_W, buf, COL_GREY, 2);
        _lastSec = t.tm_sec;
    }

    if (t.tm_wday != _lastWday) {
        char dateBuf[32];
        snprintf(dateBuf, sizeof(dateBuf), "%s  %04d/%02d/%02d",
                 WDAY_EN[t.tm_wday],
                 1900 + t.tm_year, t.tm_mon + 1, t.tm_mday);
        displayRect(0, 178, DISPLAY_W, 28, TFT_BLACK);
        displayTextCentre(0, 182, DISPLAY_W, dateBuf, COL_WHITE, 1);
        _lastWday = t.tm_wday;
    }
}

// ── Theme 6: HH MM SS three separate panels ──────────────────────────────────

static void renderTheme6(const struct tm &t) {
    // Draw static labels on first render (wday change is a good proxy)
    if (t.tm_wday != _lastWday) {
        tft.fillScreen(TFT_BLACK);
        displayTextCentre(0,  10, DISPLAY_W, "HOUR", COL_GREY, 1);
        displayTextCentre(0,  90, DISPLAY_W, "MIN",  COL_GREY, 1);
        displayTextCentre(0, 170, DISPLAY_W, "SEC",  COL_GREY, 1);
        _lastWday = t.tm_wday;
    }

    if (t.tm_hour != _lastHour) {
        drawTwoDigit(84, 22, t.tm_hour, COL_CYAN, 4);
        _lastHour = t.tm_hour;
    }
    if (t.tm_min != _lastMin) {
        drawTwoDigit(84, 102, t.tm_min, COL_YELLOW, 4);
        _lastMin = t.tm_min;
    }
    if (t.tm_sec != _lastSec) {
        drawTwoDigit(84, 182, t.tm_sec, COL_WHITE, 4);
        _lastSec = t.tm_sec;
    }
}

// ── public entry point ────────────────────────────────────────────────────────

void clockThemeRender(uint8_t theme) {
    time_t now = time(nullptr);
    if (now < 1000000000UL) {
        displayTextCentre(0, 110, DISPLAY_W, "NTP syncing...", COL_GREY, 1);
        return;
    }

    struct tm t;
    localtime_r(&now, &t);

    switch (theme) {
        case THEME_CLOCK_1: renderTheme4(t); break;
        case THEME_CLOCK_2: renderTheme5(t); break;
        case THEME_CLOCK_3: renderTheme6(t); break;
        default:            renderTheme4(t); break;
    }
}
