#include "http_server.h"
#include "config.h"
#include "filesystem.h"
#include "jpeg_display.h"
#include "display.h"
#include "weather_theme.h"
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>
#include <Preferences.h>

static ESP8266WebServer   server(80);
static ESP8266HTTPUpdateServer updater;

static uint8_t _theme      = THEME_WEATHER_CLOCK;
static uint8_t _brightness = DEFAULT_BRIGHTNESS;
static char    _imgPath[64] = UPLOAD_PATH;
static bool    _imageReady  = false;

// ── state accessors ───────────────────────────────────────────────────────────

uint8_t httpGetTheme()      { return _theme; }
uint8_t httpGetBrightness() { return _brightness; }

bool httpConsumeImageReady() {
    bool v = _imageReady;
    _imageReady = false;
    return v;
}

// ── persistence ───────────────────────────────────────────────────────────────

static void saveState() {
    Preferences prefs;
    prefs.begin("state", false);
    prefs.putUChar("theme",  _theme);
    prefs.putUChar("brt",    _brightness);
    prefs.end();
}

static void loadState() {
    Preferences prefs;
    prefs.begin("state", true);
    _theme      = prefs.getUChar("theme", THEME_WEATHER_CLOCK);
    _brightness = prefs.getUChar("brt",   DEFAULT_BRIGHTNESS);
    prefs.end();
}

// ── response helpers ──────────────────────────────────────────────────────────

static void sendJSON(const char *body) {
    server.send(200, "application/json", body);
}

// ── route handlers ────────────────────────────────────────────────────────────

// GET /
static void handleRoot() {
    server.send(200, "text/plain", "GeekMagic Custom Firmware OK");
}

// GET /v.json  →  {"m":"SmallTV-Ultra","v":"Custom-V1.0.0"}
static void handleVersion() {
    char buf[80];
    snprintf(buf, sizeof(buf), "{\"m\":\"%s\",\"v\":\"%s\"}", FW_MODEL, FW_VERSION);
    sendJSON(buf);
}

// GET /app.json  →  {"theme":N}
static void handleApp() {
    char buf[24];
    snprintf(buf, sizeof(buf), "{\"theme\":%d}", _theme);
    sendJSON(buf);
}

// GET /img.json  →  {"img":"/image//weather_clock.jpg"}
static void handleImg() {
    char buf[80];
    snprintf(buf, sizeof(buf), "{\"img\":\"%s\"}", _imgPath);
    sendJSON(buf);
}

// GET /brt.json  →  {"brt":"N"}
static void handleBrt() {
    char buf[24];
    snprintf(buf, sizeof(buf), "{\"brt\":\"%d\"}", _brightness);
    sendJSON(buf);
}

// GET /set?theme=N  — switch theme
//     /set?img=PATH — set image (only effective in theme 3)
//     /set?brt=N    — set brightness
static void handleSet() {
    bool changed = false;

    if (server.hasArg("theme")) {
        uint8_t t = (uint8_t)server.arg("theme").toInt();
        if (t >= 1 && t <= 7) {
            _theme = t;
            changed = true;
            LOG("Theme -> %d\n", _theme);

            // If switching to photo album, display the current image immediately
            if (_theme == THEME_PHOTO_ALBUM && fsExists(_imgPath)) {
                displayFill(TFT_BLACK);
                jpegDisplayFile(_imgPath);
            }
        }
    }

    if (server.hasArg("img")) {
        String p = server.arg("img");
        strncpy(_imgPath, p.c_str(), sizeof(_imgPath) - 1);
        LOG("img -> %s\n", _imgPath);

        if (_theme == THEME_PHOTO_ALBUM && fsExists(_imgPath)) {
            displayFill(TFT_BLACK);
            jpegDisplayFile(_imgPath);
            _imageReady = true;
        }
    }

    if (server.hasArg("brt")) {
        _brightness = (uint8_t)server.arg("brt").toInt();
        displaySetBrightness(_brightness);
        changed = true;
        LOG("Brightness -> %d\n", _brightness);
    }

    if (changed) saveState();
    server.send(200, "text/plain", "OK");
}

// GET /delete?f=PATH
static void handleDelete() {
    if (!server.hasArg("f")) {
        server.send(400, "text/plain", "missing f");
        return;
    }
    String path = server.arg("f");
    bool ok = fsDelete(path.c_str());
    server.send(200, "text/plain", ok ? "OK" : "NOT FOUND");
}

// POST /doUpload?dir=/image/
// multipart/form-data; field name "file"
static void handleUpload() {
    server.send(200, "text/plain", "OK");
}

static void handleUploadData() {
    HTTPUpload &upload = server.upload();

    // Accumulate chunks in a static buffer (JPEG is typically < 50 KB)
    static uint8_t *_uploadBuf = nullptr;
    static size_t   _uploadLen = 0;

    if (upload.status == UPLOAD_FILE_START) {
        _uploadLen = 0;
        if (_uploadBuf) { free(_uploadBuf); _uploadBuf = nullptr; }
        LOG("Upload START: %s (heap=%u)\n", upload.filename.c_str(), ESP.getFreeHeap());

    } else if (upload.status == UPLOAD_FILE_WRITE) {
        size_t newLen = _uploadLen + upload.currentSize;
        uint8_t *tmp = (uint8_t *)realloc(_uploadBuf, newLen);
        if (!tmp) {
            LOG("Upload realloc failed at %u bytes\n", newLen);
            return;
        }
        _uploadBuf = tmp;
        memcpy(_uploadBuf + _uploadLen, upload.buf, upload.currentSize);
        _uploadLen = newLen;

    } else if (upload.status == UPLOAD_FILE_END) {
        bool ok = (_uploadBuf && _uploadLen > 0)
                  ? fsSaveUpload(_uploadBuf, _uploadLen)
                  : false;
        if (_uploadBuf) { free(_uploadBuf); _uploadBuf = nullptr; }
        _uploadLen = 0;
        LOG("Upload END: %s (%u bytes)\n", ok ? "saved" : "FAILED", upload.totalSize);
    }
}

// POST /config  — JSON body: {"apiKey":"...","city":"..."}
static void handleConfig() {
    if (server.method() != HTTP_POST) {
        server.send(405, "text/plain", "POST only");
        return;
    }
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, server.arg("plain"));
    if (err) {
        server.send(400, "text/plain", "JSON parse error");
        return;
    }
    const char *key  = doc["apiKey"] | "";
    const char *city = doc["city"]   | "Seoul";
    weatherSetConfig(key, city);
    server.send(200, "application/json", "{\"ok\":true}");
}

// ── public init ───────────────────────────────────────────────────────────────

void httpServerInit() {
    loadState();

    server.on("/",          HTTP_GET,  handleRoot);
    server.on("/v.json",    HTTP_GET,  handleVersion);
    server.on("/app.json",  HTTP_GET,  handleApp);
    server.on("/img.json",  HTTP_GET,  handleImg);
    server.on("/brt.json",  HTTP_GET,  handleBrt);
    server.on("/set",       HTTP_GET,  handleSet);
    server.on("/delete",    HTTP_GET,  handleDelete);
    server.on("/config",    HTTP_POST, handleConfig);

    // File upload: handler fires after multipart is complete; data handler streams chunks
    server.on("/doUpload", HTTP_POST,
        handleUpload,
        handleUploadData);

    // OTA update page + binary upload
    updater.setup(&server, "/update");

    server.begin();
    LOG("HTTP server started\n");
}

void httpServerHandle() {
    server.handleClient();
}
