#include <Arduino.h>
#include <BLEGamepadClient.h>
#include <NimBLEDevice.h>
#include "MotorDriver.h"
#include "MKServoESP32.h"
#include "RGBLed.h"
#include "InversionConfig.h"
#include "WifiWebServer.h"

// ---------- Пины ----------
#define M1_FI 9
#define M1_BI 8
#define M2_FI 7
#define M2_BI 6
#define M3_FI 11
#define M3_BI 10
#define M4_FI 13
#define M4_BI 12
#define M5_FI 5
#define M5_BI 4
#define MOSFET_PIN 1

#define GEEK_SERVO_1_PIN 14
#define GEEK_SERVO_2_PIN 15

// ---------- Константы ----------
const int   MOTOR_MIN_PWM       = 350;
const float TRIGGER_DEADZONE    = 0;
const float STICK_M5_DEADZONE   = 0.15f;
const float GEEK_STICK_DEADZONE = 0.15f;

// Таймаут ожидания геймпада при старте в режиме AUTO (15 секунд)
const uint32_t GAMEPAD_WAIT_TIMEOUT_MS = 15000;

// Плавный разгон M1..M2 за 0.5 сек (500 мс) от 0 до 1023
const uint32_t RAMP_UP_M12_MS       = 350;
const float    RAMP_RATE_M12_PER_MS = 1023.0f / (float)RAMP_UP_M12_MS;

// Плавный разгон M3..M4 в обычном режиме за 1.0 сек (1000 мс) от 0 до 1023
const uint32_t RAMP_UP_M34_MS       = 350;
const float    RAMP_RATE_M34_PER_MS = 1023.0f / (float)RAMP_UP_M34_MS;

// Плавный разгон M5 за 1.0 сек (1000 мс) от 0.0 до 1.0
const uint32_t RAMP_UP_M5_MS        = 0;
const float    RAMP_RATE_M5_PER_MS  = 1.0f / (float)RAMP_UP_M5_MS;
const float    M5_MIN_START         = 0.15f;

// ---------- Имя и пароль Wi-Fi ----------
static const char* DEVICE_NAME   = "Brick Box V1";
static const char* WIFI_PASSWORD = "legolego"; // Минимум 8 символов

// ---------- Драйверы ----------
// M1, M2 работают на каналах LEDC 0..3.
// M3, M4 используют аппаратный MCPWM (таймеры 0 и 1), чтобы не занимать каналы LEDC.
// M5 (МК-серва) использует каналы LEDC 6..7.
MotorDriver  m1(M1_FI, M1_BI, 0, 1, true);
MotorDriver  m2(M2_FI, M2_BI, 2, 3, true);
MotorDriver  m3(M3_FI, M3_BI, 0, 0, true, true, 0);
MotorDriver  m4(M4_FI, M4_BI, 0, 0, true, true, 1);
MKServoESP32 m5(M5_FI, M5_BI, 6, 7, MOSFET_PIN);

XboxController xbox;

const int servoCh1 = 4;
const int servoCh2 = 5;

const int SERVO_CENTER      = 76;
const int SERVO_DELTA_LEFT  = 26;
const int SERVO_DELTA_RIGHT = 26;

// ---------- Глобальное состояние ----------
InversionState inv;
Preferences    prefs;
unsigned long  lastComboTime        = 0;
unsigned long  menuButtonPressStart = 0;
bool           menuButtonHeld       = false;

unsigned long  bothSticksPressStart = 0;
bool           bothSticksHeld       = false;
const unsigned long BOTH_STICKS_HOLD_MS = 700;

// Текущие сглаженные значения ШИМ для M1..M4 и float для M5
float curPwmM1 = 0.0f;
float curPwmM2 = 0.0f;
float curPwmM3 = 0.0f;
float curPwmM4 = 0.0f;
float curValM5 = 0.0f;
uint32_t lastRampMs = 0;

// ---------- Состояние звукового сигнала ("бип-бип" через M3) ----------
uint8_t  hornPhase      = 0;     // 0=выкл, 1=первый гудок, 2=пауза, 3=второй гудок
uint32_t hornPhaseStart = 0;
uint32_t hornLastToggle = 0;
bool     hornPolarity   = false;
bool     lastMenuBtn    = false;
bool     lastShareBtn   = false;

// ---------- Режим управления ----------
enum ControlMode : uint8_t {
    MODE_UNDECIDED = 0,
    MODE_GAMEPAD   = 1,
    MODE_PHONE     = 2
};

static const uint8_t CFG_AUTO  = 0;
static const uint8_t CFG_PAD   = 1;
static const uint8_t CFG_PHONE = 2;

ControlMode    controlMode = MODE_UNDECIDED;
WifiWebServer  webPhone;

// ---------- Forward-декларации ----------
void loopGamepad();
void loopPhone();
void checkStickSwap(const XboxControlsState& s);

// =====================================================================
//         Звуковой сигнал "бип-бип" через мотор M3 (двухполярный)
// =====================================================================

// Запускает последовательность двойного гудка, если сигнал ещё не проигрывается
static void triggerHorn() {
    if (hornPhase == 0) {
        hornPhase      = 1;
        hornPhaseStart = millis();
        hornLastToggle = micros();
        hornPolarity   = false;
    }
}

// Выполняет неблокирующее воспроизведение двухполярного меандра на моторе M3:
// Фаза 1: 650 Гц (120 мс) -> Фаза 2: пауза (80 мс) -> Фаза 3: 850 Гц (150 мс).
// Возвращает true, пока гудок активен (чтобы не перезаписывать скорость M3).
static bool updateHornM3(int requestedSpeedM3) {
    // Если водитель нажал кнопку вращения M3, тяга имеет приоритет над гудком
    if (requestedSpeedM3 != 0) {
        if (hornPhase != 0) {
            hornPhase = 0;
            m3.stop();
        }
        return false;
    }

    if (hornPhase == 0) return false;

    uint32_t nowMs = millis();
    uint32_t nowUs = micros();
    uint32_t elapsed = nowMs - hornPhaseStart;

    if (hornPhase == 1) {
        // Первый "бип": частота ~650 Гц (полупериод 770 мкс)
        if (elapsed >= 120) {
            hornPhase = 2;
            hornPhaseStart = nowMs;
            m3.stop();
        } else if (nowUs - hornLastToggle >= 770) {
            hornLastToggle = nowUs;
            hornPolarity = !hornPolarity;
            m3.setSpeed(hornPolarity ? 1023 : -1023);
        }
    } else if (hornPhase == 2) {
        // Пауза между гудками (80 мс)
        if (elapsed >= 80) {
            hornPhase = 3;
            hornPhaseStart = nowMs;
            hornLastToggle = nowUs;
        }
    } else if (hornPhase == 3) {
        // Второй "бип": частота ~850 Гц (полупериод 588 мкс)
        if (elapsed >= 150) {
            hornPhase = 0;
            m3.stop();
            return false;
        } else if (nowUs - hornLastToggle >= 588) {
            hornLastToggle = nowUs;
            hornPolarity = !hornPolarity;
            m3.setSpeed(hornPolarity ? 1023 : -1023);
        }
    }

    return true;
}

// =====================================================================
//               Плавный разгон / торможение моторов M1..M4
// =====================================================================

// Плавно изменяет текущее значение ШИМ (current) в сторону целевого (target)
// с ограничением шага maxStep. При старте с нуля сразу выставляет порог страгивания MOTOR_MIN_PWM.
static int applyMotorRamp(float& current, int target, float maxStep) {
    if (target == 0) {
        current = 0.0f;
        return 0;
    }
    // При резкой смене направления сначала сбрасываем в 0
    if ((current > 0.0f && target < 0) || (current < 0.0f && target > 0)) {
        current = 0.0f;
    }
    // Если мотор стартует с места, начинаем с минимального порога страгивания
    if (current == 0.0f) {
        current = (target > 0) ? (float)MOTOR_MIN_PWM : -(float)MOTOR_MIN_PWM;
        if (abs(target) <= MOTOR_MIN_PWM) {
            current = (float)target;
            return target;
        }
    }

    float diff = (float)target - current;
    if (fabsf(diff) <= maxStep) {
        current = (float)target;
    } else {
        current += (diff > 0.0f) ? maxStep : -maxStep;
    }
    return (int)current;
}

// =====================================================================
//               Плавный разгон / торможение канала M5 (-1.0 .. +1.0)
// =====================================================================

// Аналог applyMotorRamp для канала M5, работающего с числами с плавающей точкой (-1.0..+1.0).
static float applyFloatRamp(float& current, float target, float maxStep) {
    if (fabsf(target) < 0.001f) {
        current = 0.0f;
        return 0.0f;
    }
    if ((current > 0.0f && target < 0.0f) || (current < 0.0f && target > 0.0f)) {
        current = 0.0f;
    }
    if (current == 0.0f) {
        current = (target > 0.0f) ? M5_MIN_START : -M5_MIN_START;
        if (fabsf(target) <= M5_MIN_START) {
            current = target;
            return target;
        }
    }

    float diff = target - current;
    if (fabsf(diff) <= maxStep) {
        current = target;
    } else {
        current += (diff > 0.0f) ? maxStep : -maxStep;
    }
    return current;
}

// Сбрасывает все накопители плавного разгона в ноль (при потере связи или перезапуске)
static void resetMotorRamps() {
    curPwmM1 = curPwmM2 = curPwmM3 = curPwmM4 = 0.0f;
    curValM5 = 0.0f;
    lastRampMs = millis();
}

// =====================================================================
//                          NVS: выбор режима
// =====================================================================

// Читает из энергонезависимой памяти NVS режим запуска (AUTO / только геймпад / только телефон)
static uint8_t loadCtrlMode() {
    prefs.begin("robot_settings", true);
    uint8_t m = prefs.getUChar("ctlMode", CFG_AUTO);
    prefs.end();
    if (m > CFG_PHONE) m = CFG_AUTO;
    return m;
}

// Сохраняет режим запуска в энергонезависимую память NVS
static void saveCtrlMode(uint8_t m) {
    prefs.begin("robot_settings", false);
    prefs.putUChar("ctlMode", m);
    prefs.end();
}

// =====================================================================
//              checkStickSwap — свап стиков (L3+R3, 700 мс)
// =====================================================================

// Отслеживает одновременное удержание нажатий на левый и правый стики геймпада (L3+R3)
// в течение 700 мс для переключения стиков управления рулём и каналом M5.
void checkStickSwap(const XboxControlsState& s) {
    unsigned long now = millis();

    if (s.leftStickButton && s.rightStickButton) {
        if (!bothSticksHeld) {
            bothSticksPressStart = now;
            bothSticksHeld = true;
        } else if (now - bothSticksPressStart >= BOTH_STICKS_HOLD_MS) {
            inv.swapped = !inv.swapped;
            bothSticksHeld = false;
            saveSettings();
            if (inv.swapped) flashLED(0, 64, 0);
            else             flashLED(64, 0, 0);
        }
    } else {
        bothSticksHeld = false;
    }
}

// =====================================================================
//                                  SETUP
// =====================================================================

// Начальная инициализация периферии ESP32-S3:
// - настройка частоты CPU, адресного светодиода, драйверов M1..M5 и сервоприводов;
// - загрузка сохранённых инверсий из NVS;
// - поиск BLE-геймпада в течение 15 секунд, при отсутствии — запуск Wi-Fi пульта.
void setup() {
    setCpuFrequencyMhz(160);

    initRGB();
    setRGB(0, 0, 64);

    m1.begin(); m2.begin(); m3.begin(); m4.begin(); m5.begin();
    resetMotorRamps();

    ledcSetup(servoCh1, 50, 10);
    ledcSetup(servoCh2, 50, 10);
    ledcAttachPin(GEEK_SERVO_1_PIN, servoCh1);
    ledcAttachPin(GEEK_SERVO_2_PIN, servoCh2);
    ledcWrite(servoCh1, SERVO_CENTER);
    ledcWrite(servoCh2, SERVO_CENTER);

    loadSettings();

    uint8_t cfg = loadCtrlMode();

    if (cfg == CFG_PAD) {
        NimBLEDevice::init(DEVICE_NAME);
        NimBLEDevice::setDeviceName(DEVICE_NAME);
        xbox.begin();
        controlMode = MODE_GAMEPAD;
        flashLED(0, 64, 0);
        return;
    }

    if (cfg == CFG_PHONE) {
        webPhone.begin(DEVICE_NAME, WIFI_PASSWORD);
        controlMode = MODE_PHONE;
        flashLED(48, 0, 64);
        return;
    }

    // --- AUTO: ждём подключение геймпада в течение 15 секунд ---
    xbox.begin();

    unsigned long start = millis();
    bool blinkState = false;
    unsigned long lastBlink = 0;
    while (millis() - start < GAMEPAD_WAIT_TIMEOUT_MS) {
        if (xbox.isConnected()) {
            controlMode = MODE_GAMEPAD;
            flashLED(0, 64, 0);
            return;
        }
        if (millis() - lastBlink > 250) {
            blinkState = !blinkState;
            setRGB(0, blinkState ? 64 : 0, 0);
            lastBlink = millis();
        }
        delay(20);
        yield();
    }

    // Геймпад не найден — отключаем BLE и поднимаем Wi-Fi точку доступа
    if (NimBLEDevice::getScan()->isScanning()) {
        NimBLEDevice::getScan()->stop();
    }
    NimBLEDevice::deinit(true);

    controlMode = MODE_PHONE;
    flashLED(48, 0, 64);
    webPhone.begin(DEVICE_NAME, WIFI_PASSWORD);
}

// =====================================================================
//                                  LOOP
// =====================================================================

// Главный цикл программы: делегирует выполнение либо обработчику геймпада, либо Wi-Fi пульта.
void loop() {
    if (controlMode == MODE_GAMEPAD)      loopGamepad();
    else if (controlMode == MODE_PHONE)   loopPhone();
    delay(2);
}

// =====================================================================
//                              ХЕЛПЕРЫ
// =====================================================================

// Преобразует нажатие аналогового курка геймпада (0.0..1.0) в ШИМ мотора (MOTOR_MIN_PWM..1023)
static inline int triggerToSpeed(float value) {
    if (value <= TRIGGER_DEADZONE) return 0;
    float norm = (value - TRIGGER_DEADZONE) / (1.0f - TRIGGER_DEADZONE);
    return MOTOR_MIN_PWM + (int)(norm * (1023 - MOTOR_MIN_PWM));
}

// Преобразует отклонение стика (-1.0..1.0) в длительность импульса для GeekServo
static inline int servoPulseFromX(float x) {
    if (fabsf(x) <= GEEK_STICK_DEADZONE) return SERVO_CENTER;
    float n = (x > 0)
        ? (x - GEEK_STICK_DEADZONE) / (1.0f - GEEK_STICK_DEADZONE)
        : (x + GEEK_STICK_DEADZONE) / (1.0f - GEEK_STICK_DEADZONE);
    int d = (int)(n * (x > 0 ? SERVO_DELTA_RIGHT : SERVO_DELTA_LEFT));
    return SERVO_CENTER + d;
}

// Переводит проценты с веб-интерфейса (-100..100) в значение ШИМ (-1023..1023) с учётом порога страгивания
static inline int scalePercentToPwm(int8_t v) {
    if (v == 0) return 0;
    int a = (int)abs(v) * 1023 / 100;
    if (a < MOTOR_MIN_PWM) a = MOTOR_MIN_PWM;
    return (v > 0) ? a : -a;
}

// =====================================================================
//                            РЕЖИМ ГЕЙМПАДА
// =====================================================================

// Обрабатывает ввод с Bluetooth-геймпада Xbox, рассчитывает плавный разгон M1..M5,
// управляет сервоприводами и клаксоном M3.
void loopGamepad() {
    uint32_t now = millis();
    uint32_t dt = now - lastRampMs;
    if (dt > 100) dt = 100;
    lastRampMs = now;

    float maxStepM12 = RAMP_RATE_M12_PER_MS * (float)dt;
    // В режиме ALT (4x4) M3 и M4 разгоняются с тем же темпом, что и M1..M2 для полной синхронности
    float maxStepM34 = (inv.driveMode == DRIVE_ALT) ? maxStepM12 : (RAMP_RATE_M34_PER_MS * (float)dt);
    float maxStepM5  = RAMP_RATE_M5_PER_MS  * (float)dt;

    if (!xbox.isConnected()) {
        resetMotorRamps();
        hornPhase = 0;
        m1.stop(); m2.stop(); m3.stop(); m4.stop(); m5.stop();
        static unsigned long lastBlink = 0;
        if (now - lastBlink > 1000) {
            static bool toggle = false;
            toggle = !toggle;
            setRGB(0, 0, toggle ? 64 : 0);
            lastBlink = now;
        }
        return;
    }

    XboxControlsState s;
    xbox.read(&s);
    checkInputCombinations(s);
    checkStickSwap(s);

    // Запуск звукового сигнала при нажатии кнопки Меню или Share
    if ((s.menuButton && !lastMenuBtn) || (s.shareButton && !lastShareBtn)) {
        triggerHorn();
    }
    lastMenuBtn  = s.menuButton;
    lastShareBtn = s.shareButton;

    float lt = s.leftTrigger;
    float rt = s.rightTrigger;

    float leftStickX  = s.leftStickX;
    float rightStickX = s.rightStickX;
    if (inv.swapped) {
        float tmp   = leftStickX;
        leftStickX  = rightStickX;
        rightStickX = tmp;
    }
    float spotTurnStick = rightStickX;

    int baseSpeed = 0;
    if (rt > TRIGGER_DEADZONE)      baseSpeed =  triggerToSpeed(rt);
    else if (lt > TRIGGER_DEADZONE) baseSpeed = -triggerToSpeed(lt);

    int targetSpeedM1 = baseSpeed;
    int targetSpeedM2 = baseSpeed;

    if (inv.m1) targetSpeedM1 = -targetSpeedM1;
    if (inv.m2) targetSpeedM2 = -targetSpeedM2;
    m1.setSpeed(applyMotorRamp(curPwmM1, targetSpeedM1, maxStepM12));
    m2.setSpeed(applyMotorRamp(curPwmM2, targetSpeedM2, maxStepM12));

    int speedM3 = 0;
    int speedM4 = 0;
    if (inv.driveMode == DRIVE_ALT) {
        speedM3 = baseSpeed;
        speedM4 = baseSpeed;
    } else {
        if (s.dpadUp)        speedM3 =  1023;
        else if (s.dpadDown) speedM3 = -1023;
        if (s.buttonY)       speedM4 =  1023;
        else if (s.buttonA)  speedM4 = -1023;
    }
    if (inv.m3) speedM3 = -speedM3;
    if (inv.m4) speedM4 = -speedM4;

    // Если активен гудок на M3, не перезаписываем его скорость обычным управлением
    if (!updateHornM3(speedM3)) {
        m3.setSpeed(applyMotorRamp(curPwmM3, speedM3, maxStepM34));
    }
    m4.setSpeed(applyMotorRamp(curPwmM4, speedM4, maxStepM34));

    float targetM5 = 0.0f;
    if (abs(spotTurnStick) > STICK_M5_DEADZONE) {
        float n = (spotTurnStick > 0)
            ? (spotTurnStick - STICK_M5_DEADZONE) / (1.0f - STICK_M5_DEADZONE)
            : (spotTurnStick + STICK_M5_DEADZONE) / (1.0f - STICK_M5_DEADZONE);
        targetM5 = n;
    }
    if (inv.m5) targetM5 = -targetM5;
    m5.update_motor(applyFloatRamp(curValM5, targetM5, maxStepM5));

    float stickX1 = leftStickX;
    float stickX2 = leftStickX;
    if (inv.geek1) stickX1 = -stickX1;
    if (inv.geek2) stickX2 = -stickX2;
    ledcWrite(servoCh1, servoPulseFromX(stickX1));
    ledcWrite(servoCh2, servoPulseFromX(stickX2));

    if (hornPhase != 0)                  setRGB(64, 48, 0);
    else if (inv.driveMode == DRIVE_ALT) setRGB(48, 0, 64);
    else if (inv.m1 || inv.m2)           setRGB(64, 0, 0);
    else                                 setRGB(0, 0, 64);
}

// =====================================================================
//                         РЕЖИМ ТЕЛЕФОНА (WI-FI)
// =====================================================================

// Обрабатывает команды из веб-интерфейса смартфона:
// - поддерживает свап правого стика на управление M5 вместо сервоприводов;
// - использует XOR (!=) для инверсий, чтобы переключатели в телефоне всегда работали поверх NVS;
// - управляет моторами M1..M5 и воспроизводит гудок M3 при нажатии кнопки в пульте.
void loopPhone() {
    webPhone.loop();

    uint32_t now = millis();
    uint32_t dt = now - lastRampMs;
    if (dt > 100) dt = 100;
    lastRampMs = now;

    PhoneControls& p = webPhone.state;

    float maxStepM12 = RAMP_RATE_M12_PER_MS * (float)dt;
    // В режиме ALT (4x4) M3 и M4 разгоняются синхронно с M1 и M2
    float maxStepM34 = p.driveAlt ? maxStepM12 : (RAMP_RATE_M34_PER_MS * (float)dt);
    float maxStepM5  = RAMP_RATE_M5_PER_MS  * (float)dt;

    if (!webPhone.isConnected()) {
        resetMotorRamps();
        hornPhase = 0;
        m1.stop(); m2.stop(); m3.stop(); m4.stop(); m5.stop();
        static unsigned long lastBlink = 0;
        if (now - lastBlink > 1000) {
            static bool toggle = false;
            toggle = !toggle;
            setRGB(toggle ? 48 : 0, 0, toggle ? 64 : 0);
            lastBlink = now;
        }
        return;
    }

    // Если из веб-пульта пришёл запрос на сигнал клаксона — запускаем и сбрасываем флаг
    if (p.horn) {
        p.horn = false;
        triggerHorn();
    }

    bool stale = (now - p.lastUpdate > 1000);

    int base = stale ? 0 : scalePercentToPwm(p.throttle);

    // Инверсия через != (XOR), чтобы галочка в веб-интерфейсе всегда меняла направление
    // независимо от того, что ранее было сохранено в NVS с геймпада.
    bool inv1  = (p.invM1 != inv.m1);
    bool inv2  = (p.invM2 != inv.m2);
    bool inv3  = (p.invM3 != inv.m3);
    bool inv4  = (p.invM4 != inv.m4);
    bool inv5  = (p.invM5 != inv.m5);
    bool invG1 = (p.geek1 != inv.geek1);
    bool invG2 = (p.geek2 != inv.geek2);

    int tM1 = inv1 ? -base : base;
    int tM2 = inv2 ? -base : base;
    m1.setSpeed(applyMotorRamp(curPwmM1, tM1, maxStepM12));
    m2.setSpeed(applyMotorRamp(curPwmM2, tM2, maxStepM12));

    int tM3 = stale ? 0 : scalePercentToPwm(p.aux1);
    int tM4 = stale ? 0 : scalePercentToPwm(p.aux2);
    if (p.driveAlt) {
        tM3 = base;
        tM4 = base;
    }
    if (inv3) tM3 = -tM3;
    if (inv4) tM4 = -tM4;

    // Управление M3: если гудок не играет, подаём целевую скорость от кнопок M3+/M3- или ALT
    if (!updateHornM3(tM3)) {
        m3.setSpeed(applyMotorRamp(curPwmM3, tM3, maxStepM34));
    }
    m4.setSpeed(applyMotorRamp(curPwmM4, tM4, maxStepM34));

    // Обработка правого стика: либо управление сервами, либо свап на канал M5
    float st = stale ? 0.0f : (p.steer / 100.0f);
    float spot = stale ? 0.0f : (p.spotTurn / 100.0f);

    if (p.swapped) {
        // Правый стик управляет M5 с мёртвой зоной, сервы стоят по центру
        if (fabsf(st) > STICK_M5_DEADZONE) {
            spot = (st > 0)
                ? (st - STICK_M5_DEADZONE) / (1.0f - STICK_M5_DEADZONE)
                : (st + STICK_M5_DEADZONE) / (1.0f - STICK_M5_DEADZONE);
        } else if (fabsf(spot) < 0.01f) {
            spot = 0.0f;
        }
        ledcWrite(servoCh1, SERVO_CENTER);
        ledcWrite(servoCh2, SERVO_CENTER);
    } else {
        // Обычный режим: правый стик управляет GeekServo 1 и 2
        ledcWrite(servoCh1, servoPulseFromX(invG1 ? -st : st));
        ledcWrite(servoCh2, servoPulseFromX(invG2 ? -st : st));
    }

    if (inv5) spot = -spot;
    m5.update_motor(applyFloatRamp(curValM5, spot, maxStepM5));

    if (hornPhase != 0)         setRGB(64, 48, 0);
    else if (p.driveAlt)        setRGB(48, 0, 64);
    else if (inv1 || inv2)      setRGB(64, 0, 0);
    else                        setRGB(0, 0, 64);
}