#ifndef INVERSION_CONFIG_H
#define INVERSION_CONFIG_H

#include <Arduino.h>
#include <BLEGamepadClient.h>
#include <Preferences.h>
#include "RGBLed.h"

// Режимы движения:
//   0 = основной (автомобильный)
//   1 = дополнительный (M1..M4 синхронно от LT/RT, M5 и сервы — как обычно)
enum DriveMode : uint8_t {
    DRIVE_NORMAL = 0,
    DRIVE_ALT    = 1
};

struct InversionState {
    bool m1 = false;
    bool m2 = false;
    bool geek1 = false;
    bool geek2 = false;
    bool m3 = false;
    bool m4 = false;
    bool m5 = false;
    bool swapped  = false;
    uint8_t driveMode = DRIVE_NORMAL;
};

extern InversionState inv;
extern Preferences prefs;

extern unsigned long lastComboTime;
extern unsigned long menuButtonPressStart;
extern bool menuButtonHeld;

inline void flashLED(uint8_t r, uint8_t g, uint8_t b) {
    for (int i = 0; i < 2; i++) {
        setRGB(r, g, b);
        delay(80);
        setRGB(0, 0, 0);
        delay(80);
    }
}

inline void loadSettings() {
    prefs.begin("robot_settings", true);
    inv.m1       = prefs.getBool("m1",       false);
    inv.m2       = prefs.getBool("m2",       false);
    inv.geek1    = prefs.getBool("geek1",    false);
    inv.geek2    = prefs.getBool("geek2",    false);
    inv.m3       = prefs.getBool("m3",       false);
    inv.m4       = prefs.getBool("m4",       false);
    inv.m5       = prefs.getBool("m5",       false);
    inv.swapped  = prefs.getBool("swapped",  false);
    inv.driveMode = prefs.getUChar("driveMode", DRIVE_NORMAL);
    if (inv.driveMode > DRIVE_ALT) inv.driveMode = DRIVE_NORMAL;
    prefs.end();
}

inline void saveSettings() {
    prefs.begin("robot_settings", false);
    prefs.putBool("m1",       inv.m1);
    prefs.putBool("m2",       inv.m2);
    prefs.putBool("geek1",    inv.geek1);
    prefs.putBool("geek2",    inv.geek2);
    prefs.putBool("m3",       inv.m3);
    prefs.putBool("m4",       inv.m4);
    prefs.putBool("m5",       inv.m5);
    prefs.putBool("swapped",  inv.swapped);
    prefs.putUChar("driveMode", inv.driveMode);
    prefs.end();
}

inline void checkInputCombinations(const XboxControlsState& s) {
    unsigned long now = millis();

    // --- 1. Удержание viewButton 2 сек — циклическая смена режима ---
    if (s.viewButton) {
        if (!menuButtonHeld) {
            menuButtonPressStart = now;
            menuButtonHeld = true;
        } else if (now - menuButtonPressStart >= 2000) {
            inv.driveMode = (inv.driveMode + 1) % 2;
            menuButtonHeld = false;
            saveSettings();

            switch (inv.driveMode) {
                case DRIVE_ALT: flashLED(48, 0, 64); break; // фиолетовый
                default:        flashLED(0, 0, 64);   break; // синий
            }
        }
    } else {
        menuButtonHeld = false;
    }

    // --- 2. Быстрые комбинации с бамперами (работают в обоих режимах) ---
    if (now - lastComboTime < 500) return;

    if (s.leftBumper) {
        if (s.buttonY) {
            inv.m1 = !inv.m1;
            lastComboTime = now;
            saveSettings();
            flashLED(64, 64, 64);           // белый
        }
        else if (s.buttonX) {
            inv.geek1 = !inv.geek1;
            lastComboTime = now;
            saveSettings();
            flashLED(0, 64, 0);             // зелёный
        }
        else if (s.leftStickButton) {
            inv.m3 = !inv.m3;
            lastComboTime = now;
            saveSettings();
            flashLED(64, 0, 0);             // красный
        }
        else if (s.rightBumper) {        // LB + RB -> инверсия M5
            inv.m5 = !inv.m5;
            lastComboTime = now;
            saveSettings();
            flashLED(48, 0, 64);            // фиолетовый
        }
    }

    if (s.rightBumper) {
        if (s.buttonA) {
            inv.m2 = !inv.m2;
            lastComboTime = now;
            saveSettings();
            flashLED(64, 64, 64);           // белый
        }
        else if (s.buttonB) {
            inv.geek2 = !inv.geek2;
            lastComboTime = now;
            saveSettings();
            flashLED(0, 64, 0);             // зелёный
        }
        else if (s.rightStickButton) {
            inv.m4 = !inv.m4;
            lastComboTime = now;
            saveSettings();
            flashLED(64, 0, 0);             // красный
        }
    }
}

#endif