#pragma once

// ─────────────────────────────────────────────────────────────────────────────
// Confirmed pin mapping — KAHD154010C10-V3 (ST7789V, 1.54" 240×240 IPS)
//   MOSI=13, SCLK=14 (hardware SPI fixed)
//   CS=15 (display측 미연결, ESP 부팅 스트랩용)
//   DC=0, RST=2, BL=5 (PWM)
// ─────────────────────────────────────────────────────────────────────────────

// Firmware identity (returned by /v.json)
#define FW_MODEL    "SmallTV-Ultra"
#define FW_VERSION  "Custom-V1.0.0"

// AP name shown during first-run WiFi setup
#define WIFI_AP_NAME  "GeekMagic-Setup"

// NTP
#define NTP_SERVER      "pool.ntp.org"
#define NTP_OFFSET_SEC  (9 * 3600)   // KST = UTC+9

// Image paths on LittleFS
#define IMAGE_DIR       "/image"
#define UPLOAD_FILENAME "weather_clock.jpg"
#define UPLOAD_PATH     "/image/weather_clock.jpg"

// Theme numbers (mirrors original firmware)
#define THEME_WEATHER_CLOCK     1
#define THEME_WEATHER_FORECAST  2
#define THEME_PHOTO_ALBUM       3
#define THEME_CLOCK_1           4
#define THEME_CLOCK_2           5
#define THEME_CLOCK_3           6
#define THEME_SIMPLE_WEATHER    7

// Display
#define DISPLAY_W  240
#define DISPLAY_H  240

// Weather cache interval (ms)
#define WEATHER_INTERVAL_MS  (10UL * 60 * 1000)

// Default brightness (0–255)
#define DEFAULT_BRIGHTNESS  200

// Serial debug (set to 0 for release build)
#define DEBUG  1

#if DEBUG
#define LOG(...)  Serial.printf(__VA_ARGS__)
#else
#define LOG(...)
#endif
