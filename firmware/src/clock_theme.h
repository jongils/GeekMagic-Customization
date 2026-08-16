#pragma once

#include <stdint.h>

// Call once after NTP is synchronised to set up the clock renderer
void clockThemeInit();

// Render clock for the given theme (THEME_CLOCK_1 / _2 / _3).
// Safe to call repeatedly; only redraws changed regions.
void clockThemeRender(uint8_t theme);

// Returns true if NTP time is valid
bool clockTimeValid();

// Smooth left-right icon animation — call every loop() when in clock mode
void clockAnimUpdate();
