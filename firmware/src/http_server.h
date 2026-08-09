#pragma once

#include <stdint.h>

// Initialise and start the HTTP server on port 80.
// Must be called after WiFi is connected.
void httpServerInit();

// Process pending HTTP requests; call from main loop().
void httpServerHandle();

// Returns the currently active theme number (1–7)
uint8_t httpGetTheme();

// Returns the currently configured brightness (0–255)
uint8_t httpGetBrightness();

// Returns true if a new image was just received and rendered;
// clears the flag on read.
bool httpConsumeImageReady();
