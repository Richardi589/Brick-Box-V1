#ifndef RGB_LED_H
#define RGB_LED_H

#include <Adafruit_NeoPixel.h>

#define RGB_PIN 21          
#define NUM_LEDS 1          

// Своя яркость NeoPixel: 0..255.
// ВАЖНО: имя НЕ RGB_BRIGHTNESS — оно уже занято ядром ESP32-S3
// (pins_arduino.h, значение 64 для встроенного LED).
#define RGB_LED_BRIGHTNESS 16

extern Adafruit_NeoPixel rgbLed;

void initRGB();
void setRGB(uint8_t r, uint8_t g, uint8_t b);
void clearRGB();

#endif