#include "filesystem.h"
#include "config.h"
#include <LittleFS.h>

bool fsInit() {
    if (!LittleFS.begin()) {
        LOG("LittleFS mount failed — formatting...\n");
        LittleFS.format();
        if (!LittleFS.begin()) {
            LOG("LittleFS format failed\n");
            return false;
        }
    }

    if (!LittleFS.exists(IMAGE_DIR)) {
        LittleFS.mkdir(IMAGE_DIR);
    }

    LOG("LittleFS OK. Free: %u bytes\n",
        (unsigned)LittleFS.totalBytes() - (unsigned)LittleFS.usedBytes());
    return true;
}

bool fsSaveUpload(const uint8_t *data, size_t len) {
    File f = LittleFS.open(UPLOAD_PATH, "w");
    if (!f) {
        LOG("fsSaveUpload: cannot open %s for writing\n", UPLOAD_PATH);
        return false;
    }
    size_t written = f.write(data, len);
    f.close();
    LOG("fsSaveUpload: wrote %u/%u bytes to %s\n", written, len, UPLOAD_PATH);
    return written == len;
}

bool fsDelete(const char *path) {
    if (!LittleFS.exists(path)) return false;
    return LittleFS.remove(path);
}

bool fsExists(const char *path) {
    return LittleFS.exists(path);
}

int32_t fsFileSize(const char *path) {
    File f = LittleFS.open(path, "r");
    if (!f) return -1;
    int32_t sz = f.size();
    f.close();
    return sz;
}
