// DC=GPIO2 confirmed. Now identify driver: RED=GC9A01, GREEN=ST7789
// BL=GPIO16, RST=GPIO5, DC=GPIO2, hold 20s each
#include <Arduino.h>
#include <SPI.h>

#define CS_PIN  15
#define RST_PIN  5
#define BL_PIN  16
#define DC_PIN   2

static void spiCmd(uint8_t c) {
    digitalWrite(DC_PIN, LOW); digitalWrite(CS_PIN, LOW);
    SPI.transfer(c);
    digitalWrite(CS_PIN, HIGH);
}
static void spiData(uint8_t d) {
    digitalWrite(DC_PIN, HIGH); digitalWrite(CS_PIN, LOW);
    SPI.transfer(d);
    digitalWrite(CS_PIN, HIGH);
}
static void hwReset() {
    digitalWrite(RST_PIN, HIGH); delay(5);
    digitalWrite(RST_PIN, LOW);  delay(20);
    digitalWrite(RST_PIN, HIGH); delay(200);
}

static void gc9a01Init() {
    spiCmd(0xEF);
    spiCmd(0xEB); spiData(0x14);
    spiCmd(0xFE); spiCmd(0xEF);
    spiCmd(0xEB); spiData(0x14);
    spiCmd(0x84); spiData(0x40);
    spiCmd(0x85); spiData(0xFF); spiCmd(0x86); spiData(0xFF);
    spiCmd(0x87); spiData(0xFF); spiCmd(0x88); spiData(0x0A);
    spiCmd(0x89); spiData(0x21); spiCmd(0x8A); spiData(0x00);
    spiCmd(0x8B); spiData(0x80); spiCmd(0x8C); spiData(0x01);
    spiCmd(0x8D); spiData(0x01); spiCmd(0x8E); spiData(0xFF);
    spiCmd(0x8F); spiData(0xFF);
    spiCmd(0xB6); spiData(0x00); spiData(0x20);
    spiCmd(0x3A); spiData(0x05);
    spiCmd(0x90); spiData(0x08); spiData(0x08); spiData(0x08); spiData(0x08);
    spiCmd(0xBD); spiData(0x06); spiCmd(0xBC); spiData(0x00);
    spiCmd(0xFF); spiData(0x60); spiData(0x01); spiData(0x04);
    spiCmd(0xC3); spiData(0x13); spiCmd(0xC4); spiData(0x13);
    spiCmd(0xC9); spiData(0x22); spiCmd(0xBE); spiData(0x11);
    spiCmd(0xE1); spiData(0x10); spiData(0x0E);
    spiCmd(0xDF); spiData(0x21); spiData(0x0C); spiData(0x02);
    spiCmd(0xF0); spiData(0x45); spiData(0x09); spiData(0x08); spiData(0x08); spiData(0x26); spiData(0x2A);
    spiCmd(0xF1); spiData(0x43); spiData(0x70); spiData(0x72); spiData(0x36); spiData(0x37); spiData(0x6F);
    spiCmd(0xF2); spiData(0x45); spiData(0x09); spiData(0x08); spiData(0x08); spiData(0x26); spiData(0x2A);
    spiCmd(0xF3); spiData(0x43); spiData(0x70); spiData(0x72); spiData(0x36); spiData(0x37); spiData(0x6F);
    spiCmd(0xED); spiData(0x1B); spiData(0x0B);
    spiCmd(0xAE); spiData(0x77); spiCmd(0xCD); spiData(0x63);
    spiCmd(0x70); spiData(0x07); spiData(0x07); spiData(0x04); spiData(0x0E);
                  spiData(0x0F); spiData(0x09); spiData(0x07); spiData(0x08); spiData(0x03);
    spiCmd(0xE8); spiData(0x34);
    spiCmd(0x62); spiData(0x18); spiData(0x0D); spiData(0x71); spiData(0xED);
                  spiData(0x70); spiData(0x70); spiData(0x18); spiData(0x0F);
                  spiData(0x71); spiData(0xEF); spiData(0x70); spiData(0x70);
    spiCmd(0x63); spiData(0x18); spiData(0x11); spiData(0x71); spiData(0xF1);
                  spiData(0x70); spiData(0x70); spiData(0x18); spiData(0x13);
                  spiData(0x71); spiData(0xF3); spiData(0x70); spiData(0x70);
    spiCmd(0x64); spiData(0x28); spiData(0x29); spiData(0xF1); spiData(0x01); spiData(0xF1); spiData(0x00); spiData(0x07);
    spiCmd(0x66); spiData(0x3C); spiData(0x00); spiData(0xCD); spiData(0x67);
                  spiData(0x45); spiData(0x45); spiData(0x10); spiData(0x00); spiData(0x00); spiData(0x00);
    spiCmd(0x67); spiData(0x00); spiData(0x3C); spiData(0x00); spiData(0x00);
                  spiData(0x00); spiData(0x01); spiData(0x54); spiData(0x10); spiData(0x32); spiData(0x98);
    spiCmd(0x74); spiData(0x10); spiData(0x85); spiData(0x80); spiData(0x00); spiData(0x00); spiData(0x4E); spiData(0x00);
    spiCmd(0x98); spiData(0x3E); spiData(0x07);
    spiCmd(0x35); spiCmd(0x21);
    spiCmd(0x11); delay(120);
    spiCmd(0x29); delay(20);
}

static void st7789Init() {
    spiCmd(0x01); delay(200);
    spiCmd(0x11); delay(500);
    spiCmd(0x3A); spiData(0x55);
    spiCmd(0x36); spiData(0x00);
    spiCmd(0x2A); spiData(0); spiData(0); spiData(0); spiData(0xEF);
    spiCmd(0x2B); spiData(0); spiData(0); spiData(0); spiData(0xEF);
    spiCmd(0x21); spiCmd(0x13);
    spiCmd(0x29); delay(100);
}

static void fillColor(uint16_t color) {
    spiCmd(0x2A); spiData(0); spiData(0); spiData(0); spiData(0xEF);
    spiCmd(0x2B); spiData(0); spiData(0); spiData(0); spiData(0xEF);
    spiCmd(0x2C);
    digitalWrite(DC_PIN, HIGH); digitalWrite(CS_PIN, LOW);
    uint8_t hi = color >> 8, lo = color & 0xFF;
    for (uint32_t i = 0; i < 240UL*240UL; i++) { SPI.transfer(hi); SPI.transfer(lo); }
    digitalWrite(CS_PIN, HIGH);
}

static void hold(int seconds, const char* msg) {
    Serial.printf("%s — holding %ds\n", msg, seconds);
    for (int s = seconds; s > 0; s--) {
        Serial.printf("  %2ds\n", s);
        delay(1000); ESP.wdtFeed();
    }
}

void setup() {
    Serial.begin(115200); delay(300);
    Serial.println("\n=== Driver confirm: DC=GPIO2 BL=GPIO16 RST=GPIO5 ===");
    pinMode(BL_PIN, OUTPUT); digitalWrite(BL_PIN, HIGH);
    pinMode(CS_PIN, OUTPUT); digitalWrite(CS_PIN, HIGH);
    pinMode(RST_PIN, OUTPUT);
    pinMode(DC_PIN, OUTPUT); digitalWrite(DC_PIN, HIGH);
    SPI.begin();
    SPI.setFrequency(10000000);
    SPI.setDataMode(SPI_MODE0);
    SPI.setBitOrder(MSBFIRST);
}

void loop() {
    // GC9A01 → RED
    Serial.println("\n[GC9A01] filling RED...");
    hwReset(); gc9a01Init(); fillColor(0xF800);
    hold(20, "GC9A01 RED");

    // ST7789 → GREEN
    Serial.println("\n[ST7789] filling GREEN...");
    hwReset(); st7789Init(); fillColor(0x07E0);
    hold(20, "ST7789 GREEN");

    // Also try: no inversion for GC9A01
    Serial.println("\n[GC9A01 no-inv] filling BLUE...");
    hwReset();
    // Same init but 0x20 (INVOFF) instead of 0x21
    spiCmd(0xEF); spiCmd(0xEB); spiData(0x14);
    spiCmd(0xFE); spiCmd(0xEF);
    spiCmd(0xB6); spiData(0x00); spiData(0x20);
    spiCmd(0x3A); spiData(0x05);
    spiCmd(0x35); spiCmd(0x20); // INVOFF
    spiCmd(0x11); delay(120); spiCmd(0x29); delay(20);
    fillColor(0x001F);
    hold(20, "GC9A01-noINV BLUE");

    Serial.println("\n=== cycle done ===\n");
}
