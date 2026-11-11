#ifndef MK_SERVO_ESP32_H
#define MK_SERVO_ESP32_H

#include <Arduino.h>
#include <math.h>

// Порт класса MKServo из pink0D/TechnicPF под ESP32 ledc.
// Вход: rotation в диапазоне -1.0 .. +1.0 (как в оригинале).
// Аппаратно: N-MOSFET разрывает GND сервы, подтяжка 10k на затворе.
//
// Неблокирующая машина состояний:
//   - перед движением включаем MOSFET и ждём POWER_ON_DELAY_MS
//   - после команды "стоп" даём серве POWER_OFF_DELAY_MS встать в центр
//     (пауза отсчитывается от МОМЕНТА КОМАНДЫ "СТОП", а не от включения питания)
//   - затем снимаем питание

class MKServoESP32 {
private:
    uint8_t pinC1;      // FI драйвера SS6285L
    uint8_t pinC2;      // BI драйвера SS6285L
    uint8_t chC1;       // ledc-канал
    uint8_t chC2;       // ledc-канал
    uint8_t pinMosfet;  // затвор N-MOSFET (GND сервы)

    // Состояние
    float    lastRotation;
    bool     isPowered;
    bool     pendingMove;       // есть команда, которую надо применить после включения питания
    float    pendingRotation;
    uint32_t powerOnTime;       // millis() момента включения MOSFET

    // Состояние возврата в центр
    uint32_t stopCommandTime;   // millis() момента прихода команды "стоп"
    bool     stopPending;       // команда "стоп" уже принята, ждём возврата в центр

    // Параметры (как в ServoBrick / TechnicPF)
    static constexpr uint32_t POWER_ON_DELAY_MS  = 60;
    static constexpr uint32_t POWER_OFF_DELAY_MS = 200;
    static constexpr float    DEADZONE           = 0.04f;   // аналог ±40 из -1023..1023
    static constexpr uint32_t PWM_FREQ           = 980;
    static constexpr uint8_t  PWM_RES            = 8;

    static inline float clampf(float v, float lo, float hi) {
        return v < lo ? lo : (v > hi ? hi : v);
    }

    void applyPwm(float rotation) {
        int pwmVal = (int)(fabsf(rotation) * 255.0f + 0.5f);

        if (rotation > DEADZONE) {
            ledcWrite(chC2, 0);        // сначала гасим противоположное плечо
            ledcWrite(chC1, pwmVal);
        } else if (rotation < -DEADZONE) {
            ledcWrite(chC1, 0);
            ledcWrite(chC2, pwmVal);
        } else {
            ledcWrite(chC1, 0);
            ledcWrite(chC2, 0);
        }
    }

    void powerOn() {
        digitalWrite(pinMosfet, HIGH);
        isPowered   = true;
        powerOnTime = millis();
    }

    void powerOff() {
        ledcWrite(chC1, 0);
        ledcWrite(chC2, 0);
        digitalWrite(pinMosfet, LOW);
        isPowered = false;
    }

public:
    MKServoESP32(uint8_t p_c1, uint8_t p_c2,
                 uint8_t ch_c1, uint8_t ch_c2, uint8_t p_mosfet)
        : pinC1(p_c1), pinC2(p_c2), chC1(ch_c1), chC2(ch_c2),
          pinMosfet(p_mosfet), lastRotation(0.0f), isPowered(false),
          pendingMove(false), pendingRotation(0.0f), powerOnTime(0),
          stopCommandTime(0), stopPending(false) {}

    void begin() {
        pinMode(pinMosfet, OUTPUT);
        digitalWrite(pinMosfet, LOW);  // серва обесточена

        ledcSetup(chC1, PWM_FREQ, PWM_RES);
        ledcSetup(chC2, PWM_FREQ, PWM_RES);
        ledcAttachPin(pinC1, chC1);
        ledcAttachPin(pinC2, chC2);

        powerOff();
    }

    // Главный API — как в TechnicPF: rotation ∈ -1.0 .. +1.0
    // ВАЖНО: вызывать каждый цикл loop().
    void update_motor(double rotation) {
        float r = clampf((float)rotation, -1.0f, 1.0f);

        // --- 1. Команда "стоп" ---
        if (fabsf(r) <= DEADZONE) {
            if (isPowered) {
                // Снимаем ШИМ, но питание НЕ рубим — даём серве доехать до центра
                ledcWrite(chC1, 0);
                ledcWrite(chC2, 0);

                // Фиксируем момент первой команды "стоп"
                if (!stopPending) {
                    stopCommandTime = millis();
                    stopPending     = true;
                }

                // Ждём POWER_OFF_DELAY_MS именно с момента "стоп"
                if (millis() - stopCommandTime >= POWER_OFF_DELAY_MS) {
                    powerOff();
                    lastRotation = 0.0f;
                    stopPending  = false;
                }
            }
            pendingMove = false;
            return;
        }

        // --- 2. Есть команда движения ---
        stopPending = false;   // сброс, если снова начали двигать

        // Питание выключено — включаем и запоминаем команду
        if (!isPowered) {
            powerOn();
            pendingMove     = true;
            pendingRotation = r;
            return;
        }

        // Питание включено, но чип ещё грузится — ждём
        if (pendingMove) {
            if (millis() - powerOnTime < POWER_ON_DELAY_MS) {
                pendingRotation = r;  // обновляем последнюю команду
                return;
            }
            pendingMove = false;
        }

        // --- 3. Штатное пропорциональное управление ---
        applyPwm(r);
        lastRotation = r;
    }

    // Совместимость со старым API (геймпад отдаёт -1023..1023)
    void setSpeed(int speed) {
        update_motor((double)speed / 1023.0);
    }

    // Принудительный стоп (дисконнект, танковый режим) — питание снимается сразу
    void stop() {
        ledcWrite(chC1, 0);
        ledcWrite(chC2, 0);
        digitalWrite(pinMosfet, LOW);
        isPowered       = false;
        pendingMove     = false;
        stopPending     = false;
        lastRotation    = 0.0f;
    }
};

#endif