#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>
#include <ArduinoJson.h>
#include "http_server.h"
#include "config.h"
#include "display.h"
#include "draw_cmd.h"
#include "filesystem.h"
#include "jpeg_display.h"
#include "clock_theme.h"

static ESP8266WebServer      server(80);
static ESP8266HTTPUpdateServer updater;

// ── display mode state ────────────────────────────────────────────────────────

static DisplayMode    _mode          = MODE_CLOCK;
static unsigned long  _modeSetAt     = 0;
static unsigned long  _modeTimeoutMs = 0;   // 0 = no timeout
static uint8_t        _brightness    = DEFAULT_BRIGHTNESS;

DisplayMode httpGetDisplayMode() { return _mode; }
uint8_t     httpGetBrightness()  { return _brightness; }

bool httpCheckModeTimeout() {
    if (_mode != MODE_CLOCK && _modeTimeoutMs > 0) {
        if (millis() - _modeSetAt >= _modeTimeoutMs) {
            _mode          = MODE_CLOCK;
            _modeTimeoutMs = 0;
            return true;
        }
    }
    return false;
}

static void setMode(DisplayMode m, uint32_t timeoutSec = 0) {
    _mode          = m;
    _modeSetAt     = millis();
    _modeTimeoutMs = timeoutSec > 0 ? timeoutSec * 1000UL : 0;
}

// ── helpers ───────────────────────────────────────────────────────────────────

static void sendJSON(int code, const char *body) {
    server.send(code, "application/json", body);
}

// ── GET / — status ────────────────────────────────────────────────────────────

static void handleRoot() {
    const char *modeStr = (_mode == MODE_DRAW) ? "draw"
                        : (_mode == MODE_JPEG) ? "jpeg"
                        : "clock";
    char buf[128];
    snprintf(buf, sizeof(buf),
        "{\"fw\":\"%s\",\"mode\":\"%s\",\"brt\":%d,\"heap\":%u}",
        FW_VERSION, modeStr, _brightness, ESP.getFreeHeap());
    sendJSON(200, buf);
}

// ── GET /mode[?set=clock|draw] ────────────────────────────────────────────────

static void handleMode() {
    if (server.hasArg("set")) {
        String s = server.arg("set");
        if (s == "clock") {
            setMode(MODE_CLOCK);
            clockThemeInit();
            displayFill(TFT_BLACK);
            sendJSON(200, "{\"mode\":\"clock\"}");
        } else {
            sendJSON(400, "{\"error\":\"unknown mode\"}");
        }
        return;
    }
    // GET without param: return current mode
    handleRoot();
}

// ── POST /draw ────────────────────────────────────────────────────────────────
//
// Body (JSON):
// {
//   "clear":   true,          // optional; default true
//   "bg":      "black",       // optional background colour
//   "timeout": 30,            // optional seconds → revert to clock
//   "elements": [
//     {"type":"text",   "x":120,"y":100,"text":"Hi","size":3,"color":"cyan","align":"center"},
//     {"type":"rect",   "x":10,"y":10,"w":100,"h":50,"color":"blue","fill":true},
//     {"type":"line",   "x1":0,"y1":75,"x2":240,"y2":75,"color":"grey"},
//     {"type":"hline",  "x":20,"y":180,"len":200,"color":"white"},
//     {"type":"vline",  "x":120,"y":80,"len":80,"color":"white"},
//     {"type":"circle", "x":120,"y":120,"r":40,"color":"yellow","fill":false}
//   ]
// }

static void handleDraw() {
    if (server.method() != HTTP_POST) {
        server.send(405, "text/plain", "POST only");
        return;
    }
    String body = server.arg("plain");
    if (body.isEmpty()) {
        sendJSON(400, "{\"error\":\"empty body\"}");
        return;
    }

    // Peek at timeout before executing (drawExecute consumes the JSON)
    JsonDocument meta;
    deserializeJson(meta, body);
    uint32_t timeoutSec = meta["timeout"] | 0;

    if (!drawExecute(body)) {
        sendJSON(400, "{\"error\":\"JSON parse error\"}");
        return;
    }

    setMode(MODE_DRAW, timeoutSec);
    sendJSON(200, "{\"ok\":true}");
}

// ── POST /display  — raw JPEG body ───────────────────────────────────────────
//   (Phase 2: multipart via /doUpload also accepted for compat)

static void handleDisplay() {
    server.send(200, "text/plain", "OK");
}

static void handleDisplayData() {
    HTTPUpload &upload = server.upload();
    static uint8_t *_buf = nullptr;
    static size_t   _len = 0;

    if (upload.status == UPLOAD_FILE_START) {
        _len = 0;
        free(_buf); _buf = nullptr;
        LOG("JPEG upload START (heap=%u)\n", ESP.getFreeHeap());

    } else if (upload.status == UPLOAD_FILE_WRITE) {
        uint8_t *tmp = (uint8_t *)realloc(_buf, _len + upload.currentSize);
        if (!tmp) { LOG("JPEG realloc failed\n"); return; }
        _buf = tmp;
        memcpy(_buf + _len, upload.buf, upload.currentSize);
        _len += upload.currentSize;

    } else if (upload.status == UPLOAD_FILE_END) {
        bool ok = false;
        if (_buf && _len > 0) {
            // Save + display
            ok = fsSaveUpload(_buf, _len);
            if (ok) jpegDisplayFile(UPLOAD_PATH);
        }
        free(_buf); _buf = nullptr; _len = 0;
        if (ok) setMode(MODE_JPEG);
        LOG("JPEG upload END: %s (heap=%u)\n", ok ? "OK" : "FAIL", ESP.getFreeHeap());
    }
}

// ── GET /set?brt=N ────────────────────────────────────────────────────────────

static void handleSet() {
    if (server.hasArg("brt")) {
        _brightness = (uint8_t)server.arg("brt").toInt();
        displaySetBrightness(_brightness);
        LOG("Brightness -> %d\n", _brightness);
    }
    sendJSON(200, "{\"ok\":true}");
}

// ── public init ───────────────────────────────────────────────────────────────

void httpServerInit() {
    server.on("/",        HTTP_GET,  handleRoot);
    server.on("/mode",    HTTP_GET,  handleMode);
    server.on("/draw",    HTTP_POST, handleDraw);
    server.on("/set",     HTTP_GET,  handleSet);

    // JPEG push: /display (new) and /doUpload (legacy compat)
    server.on("/display",  HTTP_POST, handleDisplay,  handleDisplayData);
    server.on("/doUpload", HTTP_POST, handleDisplay,  handleDisplayData);

    updater.setup(&server, "/update");
    server.begin();
    LOG("HTTP server started\n");
}

void httpServerHandle() {
    server.handleClient();
}
