#ifndef WIFI_WEB_SERVER_H
#define WIFI_WEB_SERVER_H

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <WebSocketsServer.h>

// Структура текущего состояния органов управления и настроек, полученных с телефона
struct PhoneControls {
    int8_t   throttle  = 0;     // Газ/назад (-100..100)
    int8_t   steer     = 0;     // Руль / правый стик (-100..100)
    int8_t   aux1      = 0;     // Мотор M3 (-100..100)
    int8_t   aux2      = 0;     // Мотор M4 (-100..100)
    int8_t   spotTurn  = 0;     // Канал M5 (-100..100)
    bool     driveAlt  = false; // Режим 4x4 (ALT): M1..M4 синхронно от газа
    bool     horn      = false; // Флаг запроса звукового сигнала ("БИП")
    bool     invM1 = false, invM2 = false, invM3 = false,
             invM4 = false, invM5 = false,
             geek1 = false, geek2 = false, swapped = false; // Флаги инверсий и свапа стика
    uint32_t lastUpdate = 0;    // Время последнего пакета от клиента (мс)
};

// Класс управления точкой доступа Wi-Fi, HTTP-сервером (интерфейс пульта),
// Captive Portal (DNS) и WebSocket-сервером для мгновенной передачи команд.
class WifiWebServer {
public:
    PhoneControls state;

    // Запускает точку доступа Wi-Fi, DNS-сервер, HTTP и WebSocket серверы
    void begin(const char* ssid, const char* password = nullptr);

    // Обрабатывает входящие WebSocket-сообщения, DNS и HTTP запросы (вызывать в loop)
    void loop();

    // Возвращает true, если к пульту подключён хотя бы один браузер по WebSocket
    bool isConnected() const { return clientCount_ > 0; }

private:
    WebServer        http_{80};
    WebSocketsServer ws_{81};
    DNSServer        dns_;
    volatile uint8_t clientCount_ = 0;

    // Разбирает текстовую команду от веб-интерфейса и обновляет структуру state
    void handleLine(const String& line);

    // Колбэк событий WebSocket (подключение, отключение, приём текстового пакета)
    void onWsEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length);
};

#endif
