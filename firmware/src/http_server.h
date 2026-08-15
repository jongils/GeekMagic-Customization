#pragma once
#include <stdint.h>

// ── Display mode ──────────────────────────────────────────────────────────────

enum DisplayMode : uint8_t {
    MODE_CLOCK = 0,   // Default: 1-second clock render
    MODE_DRAW  = 1,   // Drawing commands active (/draw)
    MODE_JPEG  = 2    // JPEG image displayed (/display, /doUpload)
};

// ── Public API ────────────────────────────────────────────────────────────────

void httpServerInit();
void httpServerHandle();

// Returns the active display mode.
DisplayMode httpGetDisplayMode();

// Call from loop(): if a timeout was set on /draw and it has expired,
// reverts to MODE_CLOCK and returns true so the caller can reinitialise.
bool httpCheckModeTimeout();

// Returns current brightness (0–255).
uint8_t httpGetBrightness();
