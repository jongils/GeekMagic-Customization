#include "touch.h"
#include "config.h"
#include "clock_theme.h"
#include "display.h"
#include <Arduino.h>

#define TOUCH_PIN    4
#define DEBOUNCE_MS  300

static uint8_t  _theme    = THEME_CLOCK_1;
static bool     _prevHigh = false;
static uint32_t _lastMs   = 0;

uint8_t touchGetTheme() { return _theme; }

void touchInit() {
    pinMode(TOUCH_PIN, INPUT);
}

void touchHandle() {
    bool cur = digitalRead(TOUCH_PIN);
    uint32_t now = millis();
    if (cur && !_prevHigh && (now - _lastMs > DEBOUNCE_MS)) {
        if      (_theme == THEME_CLOCK_1) _theme = THEME_CLOCK_2;
        else if (_theme == THEME_CLOCK_2) _theme = THEME_CLOCK_3;
        else                              _theme = THEME_CLOCK_1;
        _lastMs = now;
        clockThemeInit();
        displayFill(TFT_BLACK);
    }
    _prevHigh = cur;
}
