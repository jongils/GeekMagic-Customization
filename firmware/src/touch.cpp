#include "touch.h"
#include "config.h"
#include "clock_theme.h"
#include "display.h"
#include <Arduino.h>

#define TOUCH_PIN    4
#define HOLD_MS      100   // 이 시간 이상 눌러야 유효 (오감지 필터)
#define DEBOUNCE_MS  500   // 연속 전환 방지 간격

static uint8_t  _theme    = THEME_CLOCK_1;
static bool     _prevHigh = false;
static bool     _fired    = false;  // 현재 누름에서 이미 전환했으면 중복 방지
static uint32_t _riseMs   = 0;     // 신호가 HIGH로 바뀐 시각
static uint32_t _lastFire = 0;     // 마지막 전환 시각

uint8_t touchGetTheme() { return _theme; }

void touchInit() {
    pinMode(TOUCH_PIN, INPUT);
}

void touchHandle() {
    bool cur = digitalRead(TOUCH_PIN);
    uint32_t now = millis();

    if (!_prevHigh && cur) {
        // rising edge — 누름 시작 시각 기록
        _riseMs = now;
        _fired  = false;
    }

    if (cur && !_fired
            && (now - _riseMs  >= HOLD_MS)
            && (now - _lastFire > DEBOUNCE_MS)) {
        // HOLD_MS 이상 유지 → 유효 터치
        if      (_theme == THEME_CLOCK_1) _theme = THEME_CLOCK_2;
        else if (_theme == THEME_CLOCK_2) _theme = THEME_CLOCK_3;
        else                              _theme = THEME_CLOCK_1;
        _lastFire = now;
        _fired    = true;
        clockThemeInit();
        displayFill(TFT_BLACK);
    }

    _prevHigh = cur;
}
