#include "RGBLed.h"

Adafruit_NeoPixel rgbLed(NUM_LEDS, RGB_PIN, NEO_GRB + NEO_KHZ800);

// Кэш последнего установленного цвета — чтобы не вызывать show() зря
static uint8_t lastR = 255, lastG = 255, lastB = 255;

void initRGB() {
    rgbLed.begin();
    rgbLed.setBrightness(RGB_LED_BRIGHTNESS);   // <- новое имя, без конфликта с ядром
    rgbLed.clear();
    rgbLed.show();
    lastR = lastG = lastB = 0;
}

void setRGB(uint8_t r, uint8_t g, uint8_t b) {
    // Если цвет не изменился — не трогаем NeoPixel (экономия тока и CPU)
    if (r == lastR && g == lastG && b == lastB) return;

    lastR = r; lastG = g; lastB = b;
    rgbLed.setPixelColor(0, rgbLed.Color(r, g, b));
    rgbLed.show();
}

void clearRGB() {
    if (lastR == 0 && lastG == 0 && lastB == 0) return;
    lastR = lastG = lastB = 0;
    rgbLed.clear();
    rgbLed.show();
}