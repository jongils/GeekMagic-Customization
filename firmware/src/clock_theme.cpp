#include "clock_theme.h"
#include "display.h"
#include "config.h"
#include <Arduino.h>
#include <time.h>

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

// ── Theme 4: 메인 시계 ────────────────────────────────────────────────────────
//
// 안전 영역 (SAFE_X=5, SAFE_Y=5, SAFE_W=230, SAFE_H=225) 기준 레이아웃
//   y= 34: date "2026.08.17  MON"  size 2, COL_GREY
//   y= 56: separator line
//   y= 70: HH:MM                   size 6, COL_CYAN   [48px → ends y=118]
//   y=127: :SS                     size 3, COL_GREY   [24px → ends y=151]
//   y=170: separator line
//   y=184: weekday full name        size 2, COL_WHITE  [16px → ends y=200]
//
// 상하 여백 각 29px → 수직 중앙 정렬

static void renderTheme4(const struct tm &t) {
    // ── Day/date strip — redraws only when day changes ───────────────────────
    if (t.tm_wday != _lastWday) {
        // Date
        char dateBuf[20];
        snprintf(dateBuf, sizeof(dateBuf), "%04d.%02d.%02d  %s",
                 1900 + t.tm_year, t.tm_mon + 1, t.tm_mday,
                 WDAY_EN[t.tm_wday]);
        displayRect(SAFE_X, 34, SAFE_W, 16, TFT_BLACK);
        displayTextCentre(SAFE_X, 34, SAFE_W, dateBuf, COL_GREY, 2);

        // Separators
        displayHLine(SAFE_X, 56, SAFE_W, COL_GREY);
        displayHLine(SAFE_X, 170, SAFE_W, COL_GREY);

        // Weekday full name
        displayRect(SAFE_X, 184, SAFE_W, 16, TFT_BLACK);
        displayTextCentre(SAFE_X, 184, SAFE_W, WDAY_FULL[t.tm_wday], COL_WHITE, 2);

        _lastWday = t.tm_wday;
    }

    // ── HH:MM — size 6 (48px) ────────────────────────────────────────────────
    if (t.tm_hour != _lastHour || t.tm_min != _lastMin) {
        char buf[6];
        snprintf(buf, sizeof(buf), "%02d:%02d", t.tm_hour, t.tm_min);
        displayRect(SAFE_X, 70, SAFE_W, 48, TFT_BLACK);
        displayTextCentre(SAFE_X, 70, SAFE_W, buf, COL_CYAN, 6);
        _lastHour = t.tm_hour;
        _lastMin  = t.tm_min;
    }

    // ── :SS — size 3 (24px) ──────────────────────────────────────────────────
    if (t.tm_sec != _lastSec) {
        char buf[4];
        snprintf(buf, sizeof(buf), ":%02d", t.tm_sec);
        displayRect(SAFE_X, 127, SAFE_W, 24, TFT_BLACK);
        displayTextCentre(SAFE_X, 127, SAFE_W, buf, COL_GREY, 3);
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

// ── 🦀 Crab icon animation (Claude CLI mascot) ────────────────────────────────
//
// Drawn with TFT primitives (built-in fonts cannot render emoji).
// Bounding box: ±14 px wide, ±9 px tall → 29×19 px total
//   ICON_Y = 215  (gap below weekday label; legs end at y=224 ≤ SAFE_Y2=229)
//
// X range: SAFE_X+16 … SAFE_X2-16  (= 21 … 218)
// Period : 6 s per full round-trip

#define ICON_Y    215
#define ICON_HW    14   // half-width  for erase rect
#define ICON_HH     9   // half-height for erase rect

static void iconErase(int16_t cx) {
    tft.fillRect(cx - ICON_HW - 1, ICON_Y - ICON_HH - 1,
                 (ICON_HW + 1) * 2, (ICON_HH + 1) * 2, TFT_BLACK);
}

static void iconDraw(int16_t cx) {
    const int16_t y   = ICON_Y;
    const uint16_t R  = 0xF800;   // red
    const uint16_t W  = TFT_WHITE;
    const uint16_t B  = TFT_BLACK;

    // Body
    tft.fillCircle(cx, y, 5, R);

    // Eyes: white circle + black pupil
    tft.fillCircle(cx - 3, y - 5, 2, W);
    tft.fillCircle(cx + 3, y - 5, 2, W);
    tft.fillCircle(cx - 3, y - 5, 1, B);
    tft.fillCircle(cx + 3, y - 5, 1, B);

    // Left claw: arm + two pincers
    tft.drawLine(cx - 5,  y - 1, cx - 10, y - 4, R);
    tft.drawLine(cx - 10, y - 4, cx - 13, y - 7, R);
    tft.drawLine(cx - 10, y - 4, cx - 13, y - 1, R);

    // Right claw (mirror)
    tft.drawLine(cx + 5,  y - 1, cx + 10, y - 4, R);
    tft.drawLine(cx + 10, y - 4, cx + 13, y - 7, R);
    tft.drawLine(cx + 10, y - 4, cx + 13, y - 1, R);

    // Legs — 3 per side
    tft.drawLine(cx - 4, y + 3, cx - 8, y + 7, R);
    tft.drawLine(cx - 3, y + 4, cx - 6, y + 9, R);
    tft.drawLine(cx - 2, y + 4, cx - 4, y + 9, R);

    tft.drawLine(cx + 4, y + 3, cx + 8, y + 7, R);
    tft.drawLine(cx + 3, y + 4, cx + 6, y + 9, R);
    tft.drawLine(cx + 2, y + 4, cx + 4, y + 9, R);
}

void clockAnimUpdate() {
    static uint32_t _lastMs = 0;
    uint32_t now = millis();
    if (now - _lastMs < 16) return;   // ~60 fps cap
    _lastMs = now;

    // Triangle wave 0.0 → 1.0 → 0.0 over PERIOD ms
    const uint32_t PERIOD = 6000;
    uint32_t phase = now % PERIOD;
    float frac = phase < PERIOD / 2
                 ? (float)phase / (PERIOD / 2)
                 : 1.0f - (float)(phase - PERIOD / 2) / (PERIOD / 2);

    int16_t minX = SAFE_X  + ICON_HW + 2;
    int16_t maxX = SAFE_X2 - ICON_HW - 2;
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
