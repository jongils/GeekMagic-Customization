#include "draw_cmd.h"
#include "display.h"
#include "config.h"
#include <ArduinoJson.h>

// ── colour parsing ────────────────────────────────────────────────────────────

static uint16_t parseColor(const char *s) {
    if (!s || *s == '\0') return TFT_WHITE;

    // Hex "#RRGGBB"
    if (*s == '#' && strlen(s) >= 7) {
        uint32_t rgb = strtoul(s + 1, nullptr, 16);
        return tft.color565((rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF);
    }

    if (strcmp(s, "white")   == 0) return TFT_WHITE;
    if (strcmp(s, "black")   == 0) return TFT_BLACK;
    if (strcmp(s, "red")     == 0) return TFT_RED;
    if (strcmp(s, "green")   == 0) return TFT_GREEN;
    if (strcmp(s, "blue")    == 0) return TFT_BLUE;
    if (strcmp(s, "cyan")    == 0) return TFT_CYAN;
    if (strcmp(s, "yellow")  == 0) return TFT_YELLOW;
    if (strcmp(s, "orange")  == 0) return COL_ORANGE;
    if (strcmp(s, "magenta") == 0) return TFT_MAGENTA;
    if (strcmp(s, "grey")    == 0) return COL_GREY;
    if (strcmp(s, "gray")    == 0) return COL_GREY;
    return TFT_WHITE;
}

// ── element renderers ─────────────────────────────────────────────────────────

static void renderText(JsonObject &el) {
    const char *text  = el["text"]  | "";
    int16_t     y     = el["y"]     | 0;
    uint8_t     size  = el["size"]  | 1;
    uint16_t    color = parseColor(el["color"] | "white");
    const char *align = el["align"] | "left";

    if (strcmp(align, "center") == 0) {
        displayTextCentre(0, y, DISPLAY_W, text, color, size);
    } else if (strcmp(align, "right") == 0) {
        displayTextRight(el["x"] | DISPLAY_W, y, text, color, size);
    } else {
        displayText(el["x"] | 0, y, text, color, size);
    }
}

static void renderRect(JsonObject &el) {
    int16_t  x     = el["x"] | 0;
    int16_t  y     = el["y"] | 0;
    int16_t  w     = el["w"] | 0;
    int16_t  h     = el["h"] | 0;
    uint16_t color = parseColor(el["color"] | "white");
    bool     fill  = el["fill"] | true;

    if (fill) displayRect(x, y, w, h, color);
    else      tft.drawRect(x, y, w, h, color);
}

static void renderCircle(JsonObject &el) {
    int16_t  x     = el["x"] | 0;
    int16_t  y     = el["y"] | 0;
    int16_t  r     = el["r"] | 0;
    uint16_t color = parseColor(el["color"] | "white");
    bool     fill  = el["fill"] | false;

    if (fill) displayCircleFill(x, y, r, color);
    else      displayCircle(x, y, r, color);
}

// ── public entry point ────────────────────────────────────────────────────────

bool drawExecute(const String &json) {
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, json);
    if (err) {
        LOG("drawExecute JSON error: %s\n", err.c_str());
        return false;
    }

    // Optional clear (defaults to true)
    if (doc["clear"] | true) {
        displayFill(parseColor(doc["bg"] | "black"));
    }

    JsonArray elements = doc["elements"].as<JsonArray>();
    if (elements.isNull()) return true;  // clear-only is valid

    for (JsonObject el : elements) {
        const char *type  = el["type"] | "";
        uint16_t    color = parseColor(el["color"] | "white");

        if      (strcmp(type, "text")   == 0) { renderText(el); }
        else if (strcmp(type, "rect")   == 0) { renderRect(el); }
        else if (strcmp(type, "circle") == 0) { renderCircle(el); }
        else if (strcmp(type, "line")   == 0) {
            displayLine(el["x1"]|0, el["y1"]|0, el["x2"]|0, el["y2"]|0, color);
        }
        else if (strcmp(type, "hline")  == 0) {
            displayHLine(el["x"]|0, el["y"]|0, el["len"]|0, color);
        }
        else if (strcmp(type, "vline")  == 0) {
            displayVLine(el["x"]|0, el["y"]|0, el["len"]|0, color);
        }
        // Unknown types are silently skipped
    }

    LOG("drawExecute: %d elements (heap=%u)\n", elements.size(), ESP.getFreeHeap());
    return true;
}
