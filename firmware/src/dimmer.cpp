#include "dimmer.h"
#include "display.h"
#include "config.h"
#include <Arduino.h>
#include <EEPROM.h>
#include <time.h>

#define DIM_EEPROM_SIZE  16
#define DIM_EEPROM_ADDR   0
#define DIM_MAGIC        0xD8   // bumped: invalidates pre-inversion saved config

static DimmerConfig _cfg    = { false, 22, 7, 10, DEFAULT_BRIGHTNESS };
static bool         _active = false;

static bool inDimPeriod(int h) {
    if (!_cfg.enabled) return false;
    if (_cfg.startHour < _cfg.endHour)           // same-day range e.g. 08:00–20:00
        return h >= _cfg.startHour && h < _cfg.endHour;
    return h >= _cfg.startHour || h < _cfg.endHour;  // wraps midnight e.g. 22:00–07:00
}

static void applyNow(int h) {
    bool dim = inDimPeriod(h);
    displaySetBrightness(dim ? _cfg.dimBrt : _cfg.normalBrt);
    _active = dim;
}

void dimmerInit() {
    EEPROM.begin(DIM_EEPROM_SIZE);
    uint8_t magic = 0;
    EEPROM.get(DIM_EEPROM_ADDR, magic);
    if (magic == DIM_MAGIC) {
        EEPROM.get(DIM_EEPROM_ADDR + 1, _cfg);
        LOG("Dimmer: loaded en=%d %02d:00-%02d:00 dim=%d norm=%d\n",
            _cfg.enabled, _cfg.startHour, _cfg.endHour,
            _cfg.dimBrt, _cfg.normalBrt);
    } else {
        LOG("Dimmer: no saved config, using defaults\n");
    }

    // Apply initial brightness
    time_t now = time(nullptr);
    if (now > 1000000000UL) {
        struct tm t; localtime_r(&now, &t);
        applyNow(t.tm_hour);
    } else {
        displaySetBrightness(_cfg.normalBrt);
    }
}

void dimmerApply(int curHour) {
    bool dim = inDimPeriod(curHour);
    if (dim == _active) return;
    displaySetBrightness(dim ? _cfg.dimBrt : _cfg.normalBrt);
    _active = dim;
    LOG("Dimmer: -> %s h=%d brt=%d\n",
        dim ? "DIM" : "NORMAL", curHour,
        dim ? _cfg.dimBrt : _cfg.normalBrt);
}

DimmerConfig dimmerGetConfig() { return _cfg; }

void dimmerSetConfig(const DimmerConfig &cfg) {
    _cfg = cfg;
    EEPROM.put(DIM_EEPROM_ADDR,     (uint8_t)DIM_MAGIC);
    EEPROM.put(DIM_EEPROM_ADDR + 1, cfg);
    EEPROM.commit();
    // Force immediate re-evaluation regardless of previous _active state
    _active = !inDimPeriod(-1);   // invalidate cache
    time_t now = time(nullptr);
    if (now > 1000000000UL) {
        struct tm t; localtime_r(&now, &t);
        applyNow(t.tm_hour);
    }
    LOG("Dimmer: saved en=%d %02d:00-%02d:00 dim=%d norm=%d\n",
        cfg.enabled, cfg.startHour, cfg.endHour, cfg.dimBrt, cfg.normalBrt);
}

bool dimmerIsActive() { return _active; }
