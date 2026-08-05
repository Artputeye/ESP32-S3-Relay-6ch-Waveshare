//config.h
#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <stdarg.h>
#include <ESPmDNS.h>
#include <LittleFS.h>
#include <Update.h>
#include "base64.h"
#include <esp_wifi.h>
#include <esp_task_wdt.h>
#include <time.h>
#include <esp_now.h>
#include "rom/rtc.h" 


// --- System Definitions ---
#define WDT_TIMEOUT 120
#define STATUS_LED 38 //Pin 2 for ESP32 Devkit/Pin 23 for ESP32 Relay 4CH//pin 38 for ESP32 Relay 6CH
#define AP_PIN 0

#define FILESYSTEM LittleFS

// --- Global Objects (Extern) ---
extern AsyncWebServer server;
extern WiFiClient espClient;
extern PubSubClient client;

extern File fsUploadFile;

// --- Device Info ---
extern char D_SWVersion[15];
extern char D_Mfac[15];
extern char D_Model[28];

// --- Network Settings ---
extern char DEVICE_NAME[28];
extern char DEVICE_PASS[28];
extern char WIFI_SSID[30];
extern char WIFI_PASS[25];
extern const char *device_id;

// --- Static IP Settings ---
extern char DIVICE_IP[16];
extern char IP_ADDR[16];
extern char SUBNET_MASK[16];
extern char GATEWAY[16];

// --- MQTT Settings ---
extern char MQTT_SERVER[16];
extern char MQTT_USER[28];
extern char MQTT_PASS[28];
extern uint16_t MQTT_PORT ;

// --- MAC : ADDRESS ---
extern char MAC_RECEIVE[18];
extern uint8_t Mac[6];
extern uint8_t MAC_RX[6];
extern String MacAddr; // เพิ่มตัวแปรนี้ไว้เก็บ Mac ในรูปแบบ String จะได้ใช้สะดวก

// --- Auth & Status ---
extern char AUTH_USER[10];
extern char AUTH_PASS[10];

extern bool isWifiApMode;
extern bool isIpConfigStatic;
extern String serialData;
extern String lastSerialMsg;

// --- นิยามพิน Relay ของบอร์ด Waveshare ESP32-S3 Relay 6CH ---
extern const int RELAY_PINS[6];
extern bool relay_states[6];

// --- Project Sub-Programs (Include หลังการประกาศ extern เสมอ) ---
#include "app_main.h"
#include "ha_integration.h"
#include "http_server.h"
#include "logger.h"
#include "network_manager.h"
#include "ota_update.h"
#include "relay.h"
#include "serial_handler.h"
#include "storage_manager.h"
#include "time_sync.h"
#include "ui_indicator.h"
#include "websocket_handler.h"







