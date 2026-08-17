#include "clock_theme.h"
#include "display.h"
#include "config.h"
#include <Arduino.h>
#include <time.h>
#include "Font_NotoSansBold36.h"
#include "Font_NotoSansBold15.h"
#include "Font_NotoSansMono20.h"
#include "Font_Unicode72.h"

static int8_t  _lastHour = -1;
static int8_t  _lastMin  = -1;
static int8_t  _lastSec  = -1;
static int8_t  _lastWday = -1;
static int16_t _iconX    = -1;   // current icon x centre (-1 = not drawn yet)

static const char *WDAY_EN[]   = { "SUN","MON","TUE","WED","THU","FRI","SAT" };
static const char *WDAY_FULL[] = { "SUNDAY","MONDAY","TUESDAY","WEDNESDAY","THURSDAY","FRIDAY","SATURDAY" };

void clockThemeInit() {
    _lastHour = _lastMin = _lastSec = _lastWday = -1;
    _iconX = -1;   // force full redraw; screen is cleared by caller
}

bool clockTimeValid() {
    return time(nullptr) > 1000000000UL;
}

// ── Theme 4: 메인 시계 (Smooth Font) ─────────────────────────────────────────
//
// Font_Unicode72   (72px, ascent=52/descent=14) for HH:MM
// NotoSansMono20   (20px, ascent=16/descent= 5) for date / :SS
// NotoSansBold15   (15px, ascent=12/descent= 4) for weekday
//
// 레이아웃 (SAFE_X=5, CX=120):
//   y= 15: date "2026.08.17  MON"  NotoSansMono20, COL_GREY   [cell 21px → ends ~36]
//   y= 40: separator line
//   y= 48: HH:MM                   Font_Unicode72, COL_CYAN   [cell 66px → ends ~114]
//   y=120: :SS                     NotoSansMono20, COL_GREY   [cell 21px → ends ~141]
//   y=148: separator line
//   y=156: weekday full name        NotoSansBold15, COL_WHITE  [cell 16px → ends ~172]
//   y=202: 🦀 crab animation

#define CX  (SAFE_X + SAFE_W / 2)   // 120

static void renderTheme4(const struct tm &t) {
    // ── Day/date strip — redraws only when day changes ───────────────────────
    if (t.tm_wday != _lastWday) {
        char dateBuf[24];
        snprintf(dateBuf, sizeof(dateBuf), "%04d.%02d.%02d  %s",
                 1900 + t.tm_year, t.tm_mon + 1, t.tm_mday,
                 WDAY_EN[t.tm_wday]);

        tft.loadFont(Font_NotoSansMono20);
        tft.setTextDatum(TC_DATUM);
        tft.setTextColor(COL_GREY, TFT_BLACK);
        tft.fillRect(SAFE_X, 15, SAFE_W, 23, TFT_BLACK);
        tft.drawString(dateBuf, CX, 15);
        tft.unloadFont();

        displayHLine(SAFE_X, 40, SAFE_W, COL_GREY);

        // Weekday full name
        tft.loadFont(Font_NotoSansBold15);
        tft.setTextDatum(TC_DATUM);
        tft.fillRect(SAFE_X, 156, SAFE_W, 18, TFT_BLACK);
        tft.setTextColor(COL_WHITE, TFT_BLACK);
        tft.drawString(WDAY_FULL[t.tm_wday], CX, 156);
        tft.unloadFont();
        displayHLine(SAFE_X, 148, SAFE_W, COL_GREY);

        _lastWday = t.tm_wday;
    }

    // ── HH:MM — Font_Unicode72 (72px, cell 66px) ─────────────────────────────
    if (t.tm_hour != _lastHour || t.tm_min != _lastMin) {
        char buf[6];
        snprintf(buf, sizeof(buf), "%02d:%02d", t.tm_hour, t.tm_min);

        tft.loadFont(Font_Unicode72);
        tft.setTextDatum(TC_DATUM);
        tft.setTextColor(COL_CYAN, TFT_BLACK);
        tft.fillRect(SAFE_X, 48, SAFE_W, 68, TFT_BLACK);
        tft.drawString(buf, CX, 48);
        tft.unloadFont();

        _lastHour = t.tm_hour;
        _lastMin  = t.tm_min;
    }

    // ── :SS — NotoSansMono20 (20px) ──────────────────────────────────────────
    if (t.tm_sec != _lastSec) {
        char buf[4];
        snprintf(buf, sizeof(buf), ":%02d", t.tm_sec);

        tft.loadFont(Font_NotoSansMono20);
        tft.setTextDatum(TC_DATUM);
        tft.setTextColor(COL_GREY, TFT_BLACK);
        tft.fillRect(SAFE_X, 120, SAFE_W, 23, TFT_BLACK);
        tft.drawString(buf, CX, 120);
        tft.unloadFont();

        _lastSec = t.tm_sec;
    }
}

// ── Theme 5: 두 줄 스타일 ────────────────────────────────────────────────────
//
//   y= 25: date "2026.08.17"  size 1, COL_GREY
//   y= 40: separator
//   y= 55: HH:MM              size 5, COL_YELLOW  [40px → ends y=95]
//   y=105: "-- SS --"         size 2, COL_GREY    [16px → ends y=121]
//   y=140: separator
//   y=155: "WED  2026/08/17"  size 1, COL_WHITE

static void renderTheme5(const struct tm &t) {
    if (t.tm_wday != _lastWday) {
        char dateBuf[24];
        snprintf(dateBuf, sizeof(dateBuf), "%s  %04d/%02d/%02d",
                 WDAY_EN[t.tm_wday],
                 1900 + t.tm_year, t.tm_mon + 1, t.tm_mday);
        displayRect(SAFE_X, 25, SAFE_W, 8, TFT_BLACK);
        displayTextCentre(SAFE_X, 25, SAFE_W, dateBuf, COL_GREY, 1);
        displayHLine(SAFE_X, 40, SAFE_W, COL_GREY);
        displayHLine(SAFE_X, 140, SAFE_W, COL_GREY);
        char wdayBuf[12];
        snprintf(wdayBuf, sizeof(wdayBuf), "%04d.%02d.%02d",
                 1900 + t.tm_year, t.tm_mon + 1, t.tm_mday);
        displayRect(SAFE_X, 155, SAFE_W, 8, TFT_BLACK);
        displayTextCentre(SAFE_X, 155, SAFE_W, wdayBuf, COL_WHITE, 1);
        _lastWday = t.tm_wday;
    }

    if (t.tm_hour != _lastHour || t.tm_min != _lastMin) {
        char buf[6];
        snprintf(buf, sizeof(buf), "%02d:%02d", t.tm_hour, t.tm_min);
        displayRect(SAFE_X, 55, SAFE_W, 40, TFT_BLACK);
        displayTextCentre(SAFE_X, 55, SAFE_W, buf, COL_YELLOW, 5);
        _lastHour = t.tm_hour;
        _lastMin  = t.tm_min;
    }

    if (t.tm_sec != _lastSec) {
        char buf[12];
        snprintf(buf, sizeof(buf), "-- %02d --", t.tm_sec);
        displayRect(SAFE_X, 105, SAFE_W, 16, TFT_BLACK);
        displayTextCentre(SAFE_X, 105, SAFE_W, buf, COL_GREY, 2);
        _lastSec = t.tm_sec;
    }
}

// ── Theme 6: HOUR / MIN / SEC 3단 패널 ───────────────────────────────────────
//
// 안전 높이 225px → 3등분 = 75px each
//   HOUR label y=13,  value y=25   (size 4, 32px)
//   MIN  label y=88,  value y=100  (size 4, 32px)
//   SEC  label y=163, value y=175  (size 4, 32px)  → ends y=207 ≤ SAFE_Y2=229

static void renderTheme6(const struct tm &t) {
    if (t.tm_wday != _lastWday) {
        tft.fillScreen(TFT_BLACK);
        displayTextCentre(SAFE_X,  13, SAFE_W, "HOUR", COL_GREY, 1);
        displayTextCentre(SAFE_X,  88, SAFE_W, "MIN",  COL_GREY, 1);
        displayTextCentre(SAFE_X, 163, SAFE_W, "SEC",  COL_GREY, 1);
        // Thin divider lines
        displayHLine(SAFE_X, 82, SAFE_W, COL_GREY);
        displayHLine(SAFE_X, 157, SAFE_W, COL_GREY);
        _lastWday = t.tm_wday;
    }

    auto drawVal = [](int16_t y, int val, uint16_t col) {
        char buf[4];
        snprintf(buf, sizeof(buf), "%02d", val);
        displayRect(SAFE_X, y, SAFE_W, 32, TFT_BLACK);
        displayTextCentre(SAFE_X, y, SAFE_W, buf, col, 4);
    };

    if (t.tm_hour != _lastHour) { drawVal(25,  t.tm_hour, COL_CYAN);   _lastHour = t.tm_hour; }
    if (t.tm_min  != _lastMin)  { drawVal(100, t.tm_min,  COL_YELLOW); _lastMin  = t.tm_min;  }
    if (t.tm_sec  != _lastSec)  { drawVal(175, t.tm_sec,  COL_WHITE);  _lastSec  = t.tm_sec;  }
}

// ── 🦀 Crab icon animation (Claude CLI mascot — pixel-art style) ──────────────
//
//   Body  : cx-24, y-16, 48×32 px
//   Claws : cx-32/cx+24, y+0, 8×8 px
//   Eyes  : cx-16/cx+12, y-8,  4×8 px (TFT_BLACK)
//   Legs  : cx-20/cx-12/cx+8/cx+16, y+16, 4×8 px
//
// ICON_Y = body vertical centre
//   ICON_Y=202 → body top y=186, leg bottom y=225 ≤ SAFE_Y2=229
// ICON_HW=32 (left claw: cx-32), ICON_HH=23 (leg bottom: y+23)
// X range: SAFE_X+33 … SAFE_X2-33  (= 38 … 201)

#define ICON_Y    202
#define ICON_HW    32
#define ICON_HH    23

// Dark red RGB(168,0,0) = #A80000 → tft.color565 = 0xA800
#define COL_CLAUDE  0xA800

static void iconErase(int16_t cx) {
    tft.fillRect(cx - ICON_HW - 1, ICON_Y - ICON_HH - 1,
                 (ICON_HW + 1) * 2, (ICON_HH + 1) * 2, TFT_BLACK);
}

static void iconDraw(int16_t cx) {
    const int16_t  y = ICON_Y;
    const uint16_t C = COL_CLAUDE;

    // Body (48×32 px)
    tft.fillRect(cx - 24, y - 16, 48, 32, C);

    // Claws (8×8 px)
    tft.fillRect(cx - 32, y,       8,  8, C);   // left
    tft.fillRect(cx + 24, y,       8,  8, C);   // right

    // Eyes (4×8 px, black)
    tft.fillRect(cx - 16, y -  8,  4,  8, TFT_BLACK);   // left
    tft.fillRect(cx + 12, y -  8,  4,  8, TFT_BLACK);   // right

    // Legs (4×8 px)
    tft.fillRect(cx - 20, y + 16,  4,  8, C);   // leg 1
    tft.fillRect(cx - 12, y + 16,  4,  8, C);   // leg 2
    tft.fillRect(cx +  8, y + 16,  4,  8, C);   // leg 3
    tft.fillRect(cx + 16, y + 16,  4,  8, C);   // leg 4
}

void clockAnimUpdate() {
    static uint32_t _lastMs = 0;
    uint32_t now = millis();
    if (now - _lastMs < 16) return;   // ~60 fps cap
    _lastMs = now;

    // Triangle wave 0.0 → 1.0 → 0.0 over PERIOD ms
    const uint32_t PERIOD = 10000;   // 10 s round-trip (slower)
    uint32_t phase = now % PERIOD;
    float frac = phase < PERIOD / 2
                 ? (float)phase / (PERIOD / 2)
                 : 1.0f - (float)(phase - PERIOD / 2) / (PERIOD / 2);

    int16_t minX = SAFE_X  + ICON_HW + 1;
    int16_t maxX = SAFE_X2 - ICON_HW - 1;
    int16_t newX = minX + (int16_t)(frac * (maxX - minX));

    if (newX == _iconX) return;

    if (_iconX >= 0) iconErase(_iconX);
    iconDraw(newX);
    _iconX = newX;
}

// ── public entry point ────────────────────────────────────────────────────────

void clockThemeRender(uint8_t theme) {
    time_t now = time(nullptr);
    if (now < 1000000000UL) {
        displayRect(SAFE_X, SAFE_Y + 100, SAFE_W, 8, TFT_BLACK);
        displayTextCentre(SAFE_X, SAFE_Y + 100, SAFE_W, "NTP syncing...", COL_GREY, 1);
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
