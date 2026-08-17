#pragma once
#include <stdint.h>

struct DimmerConfig {
    bool    enabled;
    uint8_t startHour;   // 0-23: backlight dims at this hour
    uint8_t endHour;     // 0-23: backlight returns to normal at this hour
    uint8_t dimBrt;      // brightness during dim period (0-255)
    uint8_t normalBrt;   // brightness during normal period (0-255)
};

void         dimmerInit();
void         dimmerApply(int curHour);       // call each hour change
DimmerConfig dimmerGetConfig();
void         dimmerSetConfig(const DimmerConfig &cfg);  // persists + applies now
bool         dimmerIsActive();
