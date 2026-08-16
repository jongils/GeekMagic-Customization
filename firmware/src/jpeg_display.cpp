#include "jpeg_display.h"
#include "display.h"
#include "config.h"
#include <FS.h>
#include <LittleFS.h>
#include <JPEGDEC.h>

static JPEGDEC jpeg;

// Callback: each decoded block is pushed directly to TFT (no framebuffer)
static int jpegDraw(JPEGDRAW *pDraw) {
    tft.pushImage(pDraw->x, pDraw->y,
                  pDraw->iWidth, pDraw->iHeight,
                  pDraw->pPixels);
    return 1;
}

bool jpegDisplayFile(const char *path) {
    File f = LittleFS.open(path, "r");
    if (!f) {
        LOG("jpegDisplayFile: not found: %s\n", path);
        return false;
    }

    // Read entire file into a heap buffer (JPEGDEC needs random access)
    size_t sz = f.size();
    uint8_t *buf = (uint8_t *)malloc(sz);
    if (!buf) {
        LOG("jpegDisplayFile: malloc %u failed (heap=%u)\n",
            sz, ESP.getFreeHeap());
        f.close();
        return false;
    }
    f.read(buf, sz);
    f.close();

    bool ok = false;
    if (jpeg.openRAM(buf, sz, jpegDraw)) {
        jpeg.setPixelType(RGB565_BIG_ENDIAN);
        ok = jpeg.decode(0, 0, 0);
        jpeg.close();
    } else {
        LOG("jpegDisplayFile: JPEGDEC open failed\n");
    }

    free(buf);
    LOG("jpegDisplayFile: %s -> %s (heap=%u)\n",
        path, ok ? "OK" : "FAIL", ESP.getFreeHeap());
    return ok;
}
