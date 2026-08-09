#pragma once

#include <TFT_eSPI.h>
#include <stdint.h>

// Colour helpers (RGB565)
#define COL_BLACK    TFT_BLACK
#define COL_WHITE    TFT_WHITE
#define COL_CYAN     TFT_CYAN
#define COL_YELLOW   TFT_YELLOW
#define COL_RED      TFT_RED
#define COL_GREEN    TFT_GREEN
#define COL_ORANGE   0xFD20   // RGB565
#define COL_GREY     0x8410

extern TFT_eSPI tft;

void displayInit();

// Clears the entire screen with the given colour
void displayFill(uint16_t colour);

// Draws a UTF-8 string at pixel position (x, y)
void displayText(int16_t x, int16_t y, const char *text,
                 uint16_t colour, uint8_t size = 2);

// Draws a right-aligned string whose right edge is at x
void displayTextRight(int16_t x, int16_t y, const char *text,
                      uint16_t colour, uint8_t size = 2);

// Draws a centre-aligned string within width w starting at x
void displayTextCentre(int16_t x, int16_t y, int16_t w, const char *text,
                       uint16_t colour, uint8_t size = 2);

// Draws a horizontal line
void displayHLine(int16_t x, int16_t y, int16_t len, uint16_t colour);

// Draws a filled rectangle
void displayRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t colour);

// Draws a rounded rectangle outline
void displayRoundRect(int16_t x, int16_t y, int16_t w, int16_t h,
                      int16_t r, uint16_t colour);

// Sets backlight brightness (0–255); no-op if TFT_BL == -1
void displaySetBrightness(uint8_t level);

// Boot splash shown at startup
void displayBootSplash();
