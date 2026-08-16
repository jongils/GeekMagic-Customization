#!/usr/bin/env bash
# Phase 0: ESP8266 pin mapping + display chip discovery
# Run BEFORE flashing the custom firmware.
#
# Prerequisites:
#   pip install esptool
#   Connect USB-to-Serial adapter to ESP8266 in flash mode:
#     GPIO0 → GND, then power on (or press FLASH button if available)

set -e

PORT="${1:-/dev/ttyUSB0}"
BAUD=921600
BACKUP="original_firmware.bin"

echo "=== Step 1: Backup original firmware ==="
echo "Port: $PORT"
esptool.py --port "$PORT" --baud "$BAUD" \
    read_flash 0x0 0x400000 "$BACKUP"
echo "Backup saved to $BACKUP"

echo ""
echo "=== Step 2: Search for display driver strings ==="
strings "$BACKUP" | grep -iE \
    "st7789|ili9341|gc9a01|ili9488|tft_|spi_|mosi|miso|sck|clk|dc=|rst=|cs=" \
    | sort -u || true

echo ""
echo "=== Step 3: Search for GPIO pin numbers ==="
# Look for decimal pin assignments near display-related keywords
strings "$BACKUP" | grep -E "GPIO[0-9]|pin[[:space:]]*[0-9]|= [0-9]+" \
    | grep -i "tft\|lcd\|spi\|dc\|rst\|bl\|backlight" \
    | sort -u || true

echo ""
echo "=== Step 4: Common ESP-12F SPI pin candidates ==="
echo "Hardware SPI (fixed):  MOSI=13  MISO=12  SCLK=14"
echo "Likely TFT control:    CS=15  DC=2 or 4  RST=0 or 5  BL=varies"
echo ""
echo "After confirming pins, update firmware/platformio.ini build_flags:"
echo "  -DTFT_MOSI=13 -DTFT_SCLK=14 -DTFT_CS=15 -DTFT_DC=2 -DTFT_RST=0"
echo ""
echo "And update firmware/src/config.h accordingly."
