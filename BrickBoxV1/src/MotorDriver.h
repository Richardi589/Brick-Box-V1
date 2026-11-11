#ifndef MOTOR_DRIVER_H
#define MOTOR_DRIVER_H

#include <Arduino.h>
#include "driver/mcpwm.h"

class MotorDriver {
private:
    uint8_t pinFI;
    uint8_t pinBI;
    uint8_t chFI;
    uint8_t chBI;
    bool isPwm;
    bool useMcpwm;
    mcpwm_timer_t mcpwmTimer;

public:
    MotorDriver(uint8_t fi, uint8_t bi, uint8_t channelFI, uint8_t channelBI,
                bool pwmEnabled = true, bool mcpwmMode = false, int timerIdx = 0)
        : pinFI(fi), pinBI(bi), chFI(channelFI), chBI(channelBI),
          isPwm(pwmEnabled), useMcpwm(mcpwmMode),
          mcpwmTimer(timerIdx == 0 ? MCPWM_TIMER_0 : MCPWM_TIMER_1) {}

    void begin() {
        if (useMcpwm) {
            mcpwm_io_signals_t sigA = (mcpwmTimer == MCPWM_TIMER_0) ? MCPWM0A : MCPWM1A;
            mcpwm_io_signals_t sigB = (mcpwmTimer == MCPWM_TIMER_0) ? MCPWM0B : MCPWM1B;

            mcpwm_gpio_init(MCPWM_UNIT_0, sigA, pinFI);
            mcpwm_gpio_init(MCPWM_UNIT_0, sigB, pinBI);

            mcpwm_config_t pwm_config;
            pwm_config.frequency = 20000; // Бесшумные 20 кГц
            pwm_config.cmpr_a = 0.0f;
            pwm_config.cmpr_b = 0.0f;
            pwm_config.counter_mode = MCPWM_UP_COUNTER;
            pwm_config.duty_mode = MCPWM_DUTY_MODE_0;
            mcpwm_init(MCPWM_UNIT_0, mcpwmTimer, &pwm_config);
            stop();
        } else if (isPwm) {
            ledcSetup(chFI, 20000, 10); // Для ходовых моторов оставляем бесшумные 20 кГц
            ledcSetup(chBI, 20000, 10);
            ledcAttachPin(pinFI, chFI);
            ledcAttachPin(pinBI, chBI);
            stop();
        } else {
            pinMode(pinFI, OUTPUT);
            pinMode(pinBI, OUTPUT);
            digitalWrite(pinFI, LOW);
            digitalWrite(pinBI, LOW);
        }
    }

    // Установка частоты ШИМ для звуковых эффектов (только для MCPWM)
    void setFrequency(uint32_t freq) {
        if (useMcpwm) {
            mcpwm_set_frequency(MCPWM_UNIT_0, mcpwmTimer, freq);
        }
    }

    void setSpeed(int speed) {
        if (useMcpwm) {
            if (speed > 1023) speed = 1023;
            if (speed < -1023) speed = -1023;

            if (speed > 5) {
                float duty = (speed * 100.0f) / 1023.0f;
                mcpwm_set_duty(MCPWM_UNIT_0, mcpwmTimer, MCPWM_OPR_B, 0.0f);
                mcpwm_set_duty_type(MCPWM_UNIT_0, mcpwmTimer, MCPWM_OPR_B, MCPWM_DUTY_MODE_0);
                mcpwm_set_duty(MCPWM_UNIT_0, mcpwmTimer, MCPWM_OPR_A, duty);
                mcpwm_set_duty_type(MCPWM_UNIT_0, mcpwmTimer, MCPWM_OPR_A, MCPWM_DUTY_MODE_0);
            } else if (speed < -5) {
                float duty = (abs(speed) * 100.0f) / 1023.0f;
                mcpwm_set_duty(MCPWM_UNIT_0, mcpwmTimer, MCPWM_OPR_A, 0.0f);
                mcpwm_set_duty_type(MCPWM_UNIT_0, mcpwmTimer, MCPWM_OPR_A, MCPWM_DUTY_MODE_0);
                mcpwm_set_duty(MCPWM_UNIT_0, mcpwmTimer, MCPWM_OPR_B, duty);
                mcpwm_set_duty_type(MCPWM_UNIT_0, mcpwmTimer, MCPWM_OPR_B, MCPWM_DUTY_MODE_0);
            } else {
                mcpwm_set_duty(MCPWM_UNIT_0, mcpwmTimer, MCPWM_OPR_A, 0.0f);
                mcpwm_set_duty_type(MCPWM_UNIT_0, mcpwmTimer, MCPWM_OPR_A, MCPWM_DUTY_MODE_0);
                mcpwm_set_duty(MCPWM_UNIT_0, mcpwmTimer, MCPWM_OPR_B, 0.0f);
                mcpwm_set_duty_type(MCPWM_UNIT_0, mcpwmTimer, MCPWM_OPR_B, MCPWM_DUTY_MODE_0);
            }
        } else if (isPwm) {
            if (speed > 5) {
                ledcWrite(chBI, 0);
                ledcWrite(chFI, speed);
            } else if (speed < -5) {
                ledcWrite(chFI, 0);
                ledcWrite(chBI, abs(speed));
            } else {
                ledcWrite(chFI, 0);
                ledcWrite(chBI, 0);
            }
        } else {
            if (speed > 0) {
                digitalWrite(pinBI, LOW);
                digitalWrite(pinFI, HIGH);
            } else if (speed < 0) {
                digitalWrite(pinFI, LOW);
                digitalWrite(pinBI, HIGH);
            } else {
                digitalWrite(pinFI, LOW);
                digitalWrite(pinBI, LOW);
            }
        }
    }

    void stop() {
        setSpeed(0);
    }
};

#endif