#include "crab_color.h"
#include "config.h"
#include <Arduino.h>
#include <EEPROM.h>

// EEPROM layout (shared with dimmer which uses addr 0-7)
#define CRAB_EEPROM_TOTAL  32
#define CRAB_EEPROM_ADDR   16
#define CRAB_MAGIC         0xC1

// Defaults: cold=#0060FF  base=#A80000  hot=#FFA000 (RGB565)
static CrabColorConfig _cfg = {
    true,
    40,      // minTemp °C
    50,      // midTemp °C
    60,      // maxTemp °C
    0x031F,  // coldColor  #0060FF (blue)
    0xA800,  // baseColor  #A80000 (dark red)
    0xFD00   // hotColor   #FFA000 (amber)
};

static uint16_t _color    = 0xA800;
static float    _lastTemp = -999.0f;   // -999 = not yet received

// ── RGB565 color helpers ──────────────────────────────────────────────────────

static uint16_t lerpColor(uint16_t a, uint16_t b, float t) {
    if (t <= 0.0f) return a;
    if (t >= 1.0f) return b;
    int ar = (a >> 11) & 0x1F, ag = (a >> 5) & 0x3F, ab = a & 0x1F;
    int br = (b >> 11) & 0x1F, bg = (b >> 5) & 0x3F, bb = b & 0x1F;
    int r  = ar + (int)((br - ar) * t + 0.5f);
    int g  = ag + (int)((bg - ag) * t + 0.5f);
    int bl = ab + (int)((bb - ab) * t + 0.5f);
    return ((r & 0x1F) << 11) | ((g & 0x3F) << 5) | (bl & 0x1F);
}

static uint16_t tempToColor(float tempC) {
    float mn = _cfg.minTemp, mid = _cfg.midTemp, mx = _cfg.maxTemp;
    if (tempC <= mn)  return _cfg.coldColor;
    if (tempC >= mx)  return _cfg.hotColor;
    if (tempC < mid) {
        return lerpColor(_cfg.coldColor, _cfg.baseColor,
                         (tempC - mn) / (mid - mn));
    }
    return lerpColor(_cfg.baseColor, _cfg.hotColor,
                     (tempC - mid) / (mx - mid));
}

// ── Public API ────────────────────────────────────────────────────────────────

void crabColorInit() {
    EEPROM.begin(CRAB_EEPROM_TOTAL);
    uint8_t magic = 0;
    EEPROM.get(CRAB_EEPROM_ADDR, magic);
    if (magic == CRAB_MAGIC) {
        EEPROM.get(CRAB_EEPROM_ADDR + 1, _cfg);
        LOG("CrabColor: loaded en=%d %d-%d-%d°C\n",
            _cfg.enabled, _cfg.minTemp, _cfg.midTemp, _cfg.maxTemp);
    } else {
        LOG("CrabColor: no saved config, using defaults\n");
    }
    _color = _cfg.baseColor;   // show base color until first temp arrives
}

void crabColorUpdate() {
    // Recalculate color from current temp — called from animation loop
    if (!_cfg.enabled || _lastTemp < -900.0f) {
        _color = _cfg.baseColor;
    } else {
        _color = tempToColor(_lastTemp);
    }
}

void crabSetTemp(float tempC) {
    _lastTemp = tempC;
    crabColorUpdate();
    LOG("CrabColor: temp=%.1f°C -> 0x%04X\n", tempC, _color);
}

uint16_t crabColorGet() { return _color; }
float    crabTempC()    { return _lastTemp; }
bool     crabTempValid(){ return _lastTemp > -900.0f; }

CrabColorConfig crabColorGetConfig() { return _cfg; }

void crabColorSetConfig(const CrabColorConfig &cfg) {
    _cfg = cfg;
    EEPROM.put(CRAB_EEPROM_ADDR,     (uint8_t)CRAB_MAGIC);
    EEPROM.put(CRAB_EEPROM_ADDR + 1, cfg);
    EEPROM.commit();
    crabColorUpdate();
    LOG("CrabColor: saved en=%d %d-%d-%d°C\n",
        cfg.enabled, cfg.minTemp, cfg.midTemp, cfg.maxTemp);
}
