#include "display.h"
#include "config.h"
#include <Arduino.h>

TFT_eSPI tft = TFT_eSPI();

void displayInit() {
    tft.init();
    tft.setRotation(0);
    tft.fillScreen(TFT_BLACK);

#if TFT_BL != -1
    pinMode(TFT_BL, OUTPUT);
    displaySetBrightness(DEFAULT_BRIGHTNESS);
#endif

    LOG("TFT init done. Free heap: %u\n", ESP.getFreeHeap());
}

void displayFill(uint16_t colour) {
    tft.fillScreen(colour);
}

void displayText(int16_t x, int16_t y, const char *text,
                 uint16_t colour, uint8_t size) {
    tft.setTextSize(size);
    tft.setTextColor(colour, TFT_BLACK);
    tft.setCursor(x, y);
    tft.print(text);
}

void displayTextRight(int16_t x, int16_t y, const char *text,
                      uint16_t colour, uint8_t size) {
    tft.setTextSize(size);
    int16_t tw = tft.textWidth(text);
    displayText(x - tw, y, text, colour, size);
}

void displayTextCentre(int16_t x, int16_t y, int16_t w, const char *text,
                       uint16_t colour, uint8_t size) {
    tft.setTextSize(size);
    int16_t tw = tft.textWidth(text);
    displayText(x + (w - tw) / 2, y, text, colour, size);
}

void displayHLine(int16_t x, int16_t y, int16_t len, uint16_t colour) {
    tft.drawFastHLine(x, y, len, colour);
}

void displayRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t colour) {
    tft.fillRect(x, y, w, h, colour);
}

void displayRoundRect(int16_t x, int16_t y, int16_t w, int16_t h,
                      int16_t r, uint16_t colour) {
    tft.drawRoundRect(x, y, w, h, r, colour);
}

void displayLine(int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t colour) {
    tft.drawLine(x1, y1, x2, y2, colour);
}

void displayVLine(int16_t x, int16_t y, int16_t len, uint16_t colour) {
    tft.drawFastVLine(x, y, len, colour);
}

void displayCircle(int16_t x, int16_t y, int16_t r, uint16_t colour) {
    tft.drawCircle(x, y, r, colour);
}

void displayCircleFill(int16_t x, int16_t y, int16_t r, uint16_t colour) {
    tft.fillCircle(x, y, r, colour);
}

void displaySetBrightness(uint8_t level) {
#if TFT_BL != -1
    analogWrite(TFT_BL, level);
#endif
}

void displayBootSplash() {
    tft.fillScreen(TFT_BLACK);
    displayTextCentre(SAFE_X,  78, SAFE_W, "GeekMagic",          COL_CYAN,  3);
    displayTextCentre(SAFE_X, 113, SAFE_W, "Custom Firmware",    COL_WHITE, 2);
    displayTextCentre(SAFE_X, 136, SAFE_W, FW_VERSION,           COL_GREY,  1);
    displayHLine(SAFE_X, 155, SAFE_W, COL_GREY);
    displayTextCentre(SAFE_X, 165, SAFE_W, "Connecting WiFi...", COL_GREY,  1);
}
