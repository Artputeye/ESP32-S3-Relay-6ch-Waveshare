//config.cpp
#include "config.h"

// --- Global Objects ---
AsyncWebServer server(80);
File fsUploadFile;

// --- Device Info ---
char D_SWVersion[15] = "1.0.0"; //1.2.8
char D_Mfac[15] = "Waveshare"; //ARTTECH
char D_Model[28] = "ARTTECH"; //ESP32-S3-Relay-6CH

// --- Network Settings ---
char DEVICE_NAME[28] = "Hibay Zone 1"; // ESP32-S3 6CH Relay
char DEVICE_PASS[28] = "12345678"; //12345678
char WIFI_SSID[30] = "";
char WIFI_PASS[25] = "";
const char *device_id = "hibay1_relay"; // Unique ID ใหม่สำหรับอุปกรณ์

// --- Static IP Settings ---
char DIVICE_IP[16];
char IP_ADDR[16] = "0.0.0.0"; //0.0.0.0
char SUBNET_MASK[16] = "255.255.255.0"; //255.255.255.0
char GATEWAY[16] = "0.0.0.0"; //0.0.0.0

// --- MQTT Settings ---
char MQTT_SERVER[16] = ""; //
char MQTT_USER[28] = ""; // 
char MQTT_PASS[28] = ""; //
uint16_t MQTT_PORT = 1883; //1883

// --- MAC : ADDRESS ---
char MAC_RECEIVE[18];
uint8_t Mac[6];
uint8_t MAC_RX[6];
String MacAddr;

// --- Auth & Status ---
char AUTH_USER[10] = "admin"; //admin
char AUTH_PASS[10] = "12345678"; //12345678

bool isWifiApMode = false;
bool isIpConfigStatic = false;
String serialData = "";
String lastSerialMsg = ""; //ค่าที่เข้ามาจาก Serial Input

const int RELAY_PINS[6] = {1, 2, 41, 42, 45, 46};
bool relay_states[6] = {false, false, false, false, false, false};