#include <Arduino.h>
#include <ESP32Ping.h>
#include <UniversalTelegramBot.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <esp_task_wdt.h>
#include <ld2410.h>

const char* ssid = "Wifi SSID";
const char* password = "Wifi Password";

const IPAddress remote_ip(192, 168, 1, 8);  // IP address to ping

// Telegram settings
#define BOTtoken "XXXXXXXXX:XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX"
#define CHAT_ID "XXXXXXXXXX"

#define MONITOR_SERIAL Serial
#define RADAR_SERIAL Serial1
#define RADAR_RX_PIN 16
#define RADAR_TX_PIN 17
const uint32_t maxDistance = 1000;  // in cm
const uint32_t minEnergy = 50;      // 0-100
const uint32_t readDelay = 10;  // time to wait after message sent in seconds

#define WDT_TIMEOUT 120

WiFiClientSecure client;
UniversalTelegramBot bot(BOTtoken, client);
ld2410 radar;

void setup() {
    Serial.begin(115200);
    RADAR_SERIAL.begin(256000, SERIAL_8N1, RADAR_RX_PIN,
                       RADAR_TX_PIN);  // UART for monitoring the radar
    delay(500);

    esp_task_wdt_init(WDT_TIMEOUT, true);
    esp_task_wdt_add(NULL);

    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    client.setCACert(TELEGRAM_CERTIFICATE_ROOT);

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("");
    Serial.println("WiFi connected");

    MONITOR_SERIAL.print(F("LD2410 radar sensor initialising: "));

    if (radar.begin(RADAR_SERIAL)) {
        MONITOR_SERIAL.println(F("OK"));
        MONITOR_SERIAL.print(F("LD2410 firmware version: "));
        MONITOR_SERIAL.print(radar.firmware_major_version);
        MONITOR_SERIAL.print('.');
        MONITOR_SERIAL.print(radar.firmware_minor_version);
        MONITOR_SERIAL.print('.');
        MONITOR_SERIAL.println(radar.firmware_bugfix_version, HEX);
    } else {
        MONITOR_SERIAL.println(F("not connected"));
    }

    bot.sendMessage(CHAT_ID, "Reinitialized");
}

bool detected = false;

void loop() {
    esp_task_wdt_reset();

    radar.read();
    if (radar.isConnected()) {
        if (radar.presenceDetected()) {
            Serial.println();
            if (radar.stationaryTargetDetected()) {
                Serial.print(F("Stationary target: "));
                Serial.print(radar.stationaryTargetDistance());
                Serial.print(F("cm energy:"));
                Serial.print(radar.stationaryTargetEnergy());
                Serial.print(' ');

                if (radar.stationaryTargetDistance() < maxDistance &&
                    radar.stationaryTargetEnergy() > minEnergy) {
                    if (!Ping.ping(remote_ip)) {
                        detected = true;
                        bot.sendMessage(
                            CHAT_ID,
                            "Stationary target detected, distance: " +
                                String(radar.stationaryTargetDistance()) +
                                "cm, energy: " +
                                String(radar.stationaryTargetEnergy()));
                    }
                }
            }
            if (radar.movingTargetDetected()) {
                Serial.print(F("Moving target: "));
                Serial.print(radar.movingTargetDistance());
                Serial.print(F("cm energy:"));
                Serial.print(radar.movingTargetEnergy());

                if (radar.movingTargetDistance() < maxDistance &&
                    radar.movingTargetEnergy() > minEnergy) {
                    if (!Ping.ping(remote_ip)) {
                        detected = true;
                        bot.sendMessage(
                            CHAT_ID, "Moving target detected, distance: " +
                                         String(radar.movingTargetDistance()) +
                                         "cm, energy: " +
                                         String(radar.movingTargetEnergy()));
                    }
                }
            }
        }
    }

    if (detected) {
        detected = false;
        delay(readDelay * 1000);
    } else {
        delay(1500);
    }
}
