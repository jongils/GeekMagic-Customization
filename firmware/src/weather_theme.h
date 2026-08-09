#pragma once

#include <stdint.h>

// Configure weather API credentials (stored to NVS by http_server)
void weatherSetConfig(const char *apiKey, const char *city);

// Fetch fresh weather data if the cache is stale; call from main loop.
// Does nothing while HTTPS + heap are under pressure.
void weatherUpdate();

// Render weather theme; only redraws changed areas.
void weatherThemeRender(uint8_t theme);

// Returns true if valid weather data is cached
bool weatherDataValid();
