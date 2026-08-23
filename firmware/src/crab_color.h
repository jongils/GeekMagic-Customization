#pragma once
#include <stdint.h>

struct CrabColorConfig {
    bool     enabled;    // true = temperature-driven color
    uint8_t  minTemp;    // °C — coldColor applied here
    uint8_t  midTemp;    // °C — baseColor applied here
    uint8_t  maxTemp;    // °C — hotColor applied here
    uint16_t coldColor;  // RGB565 at minTemp
    uint16_t baseColor;  // RGB565 at midTemp (and when no temp data)
    uint16_t hotColor;   // RGB565 at maxTemp
};

void             crabColorInit();
void             crabColorUpdate();          // recalculate from current temp
void             crabSetTemp(float tempC);   // push external temperature (e.g. Pi CPU)
uint16_t         crabColorGet();             // current crab color (RGB565)
float            crabTempC();                // last received temp in °C (-999 if none)
bool             crabTempValid();            // true if temp data has been received
CrabColorConfig  crabColorGetConfig();
void             crabColorSetConfig(const CrabColorConfig &cfg);
