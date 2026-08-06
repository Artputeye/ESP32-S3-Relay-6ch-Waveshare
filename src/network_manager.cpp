// network_manager.cpp
#include "network_manager.h"

const unsigned long HOLD_MS = 5000;

// State variables
static unsigned long pressStart = 0;
static bool pressed = false;

void apModeCheck()
{
    bool isPressed = (digitalRead(AP_PIN) == LOW);

    if (isPressed && !pressed)
    {
        pressed = true;
        pressStart = millis();
    }
    else if (!isPressed && pressed)
    {
        pressed = false;
    }

    if (pressed && (millis() - pressStart >= HOLD_MS))
    {
        LOGW("⚠️ Long press detected! Switching to AP Mode...");
        isWifiApMode = 0;      // Set to AP
        saveWifiModeSetting(); // สมมติว่ามี function นี้ใน globals
        vTaskDelay(pdMS_TO_TICKS(1000));
        ESP.restart();
    }
}

void network_setup()
{
    if (!LittleFS.begin(true))
    {
        LOGE("❌ LittleFS Mount Failed");
    }
    else
    {
        LOGI("✅ LittleFS Mounted");
    }
    vTaskDelay(pdMS_TO_TICKS(100));

    WiFi.onEvent(WiFiEvent);
    readNetworkConfig();

    if (isWifiApMode == 1)
    { // STA Mode
        setupIPConfig();
    }

    setupWiFiMode();

    initWebRoutes();
    vTaskDelay(pdMS_TO_TICKS(100));

    ws_init();
    vTaskDelay(pdMS_TO_TICKS(100));

    setupOTAManagement();
    vTaskDelay(pdMS_TO_TICKS(100));

    server.begin();
    LOGI("🌐 HTTP Server Started");
}

void mac_config()
{
    uint8_t mac[6];
    WiFi.macAddress(mac); // ดึงค่า MAC Address มาเก็บในอาเรย์ mac

    char tempMac[18];
    snprintf(tempMac, sizeof(tempMac), "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    MacAddr = String(tempMac); // เก็บลงตัวแปร Global String

    // รวมเป็นข้อความบรรทัดเดียว ป้องกันข้อความใน Serial แตกกระจาย
    LOGI("Device MAC Address: %s", tempMac);
}

bool parseMacAddress(const char *macStr, uint8_t mac[6])
{
    if (!macStr)
        return false;

    int values[6];

    if (sscanf(macStr, "%x:%x:%x:%x:%x:%x",
               &values[0], &values[1], &values[2],
               &values[3], &values[4], &values[5]) != 6)
    {
        return false;
    }

    for (int i = 0; i < 6; i++)
        mac[i] = (uint8_t)values[i];

    return true;
}

void readNetworkConfig()
{
    LOGD("--- [FS] Starting Network Config Load ---");

    if (!LittleFS.begin(false))
    {
        LOGE("❌ FS Error: LittleFS not mounted. Check setup()!");
        return;
    }

    if (!LittleFS.exists("/networkconfig.json"))
    {
        LOGW("⚠️ FS Warning: /networkconfig.json NOT FOUND on Flash");

        File root = LittleFS.open("/");
        File file = root.openNextFile();
        LOGD("Files currently on FS:");
        while (file)
        {
            LOGD("  - %s (%d bytes)", file.name(), file.size());
            file = root.openNextFile();
        }
        return;
    }

    File file = LittleFS.open("/networkconfig.json", "r");
    if (!file)
    {
        LOGE("❌ FS Error: Could not open file for reading");
        return;
    }

    LOGD("📂 File opened. Size: %d bytes", file.size());

    // =================================================================
    // ✅ จุดที่แก้ไข: อ่านและพิมพ์ข้อความดิบจากไฟล์ลงมอนิเตอร์เพื่อตรวจสอบ
    // =================================================================
    String rawContent = file.readString();
    //LOGD("📄 Raw File Content in Flash: %s", rawContent.c_str());
    file.seek(0); // 👈 ย้อน Pointer กลับไปจุดเริ่มต้นไฟล์ เพื่อให้ deserializeJson อ่านต่อได้

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, file);
    file.close();

    if (error)
    {
        LOGE("❌ JSON Parsing Error: %s", error.c_str());
        return;
    }

    // ดึงเป็น int ตรงๆ (รองรับทั้งกรณีหน้าเว็บส่ง "1" หรือ 1 มา)
    isWifiApMode = doc["wifi_mode"].as<int>();
    isIpConfigStatic = doc["ip_config"].as<int>();

    strlcpy(WIFI_SSID, doc["wifi_ssid"] | "", sizeof(WIFI_SSID));
    strlcpy(WIFI_PASS, doc["wifi_pass"] | "", sizeof(WIFI_PASS));
    strlcpy(MAC_RECEIVE, doc["mac_receive"] | "", sizeof(MAC_RECEIVE));

    strlcpy(IP_ADDR, doc["ip_address"] | "", sizeof(IP_ADDR));
    strlcpy(SUBNET_MASK, doc["subnet_mask"] | "", sizeof(SUBNET_MASK));
    strlcpy(GATEWAY, doc["gateway"] | "", sizeof(GATEWAY));

    strlcpy(MQTT_SERVER, doc["mqtt_server"] | "", sizeof(MQTT_SERVER));
    strlcpy(MQTT_USER, doc["mqtt_user"] | "", sizeof(MQTT_USER));
    strlcpy(MQTT_PASS, doc["mqtt_pass"] | "", sizeof(MQTT_PASS));
    strlcpy(HOST_NAME, doc["host_name"] | "relay", sizeof(HOST_NAME));


    MQTT_PORT = atoi(doc["mqtt_port"] | "1883");

    // ================== DEBUG AFTER MAPPING ==================
    LOGD("✅ Config Loaded Successfully:");
    LOGD("  WIFI_SSID    : %s", WIFI_SSID);
    LOGD("  WIFI_MODE    : %s (%d)", (isWifiApMode == 1 ? "STA" : "AP"), isWifiApMode);
    LOGD("  IP_CONFIG    : %s (%d)", (isIpConfigStatic == 1 ? "STATIC" : "DHCP"), isIpConfigStatic);
    LOGD("  MAC_RECEIVE  : %s", MAC_RECEIVE);
    LOGD("  IP_ADDRESS   : %s", IP_ADDR);
    LOGD("  SUBNET_MASK  : %s", SUBNET_MASK);
    LOGD("  GATEWAY      : %s", GATEWAY);
    LOGD("  MQTT_SERVER  : %s:%d", MQTT_SERVER, MQTT_PORT);


    // ================== MAC Parse ==================
    if (parseMacAddress(MAC_RECEIVE, MAC_RX))
    {
        LOGD("  MAC_RX Parsed: %02X:%02X:%02X:%02X:%02X:%02X",
             MAC_RX[0], MAC_RX[1], MAC_RX[2], MAC_RX[3], MAC_RX[4], MAC_RX[5]);
    }
    else
    {
        LOGW("⚠️ Invalid MAC_RECEIVE format in JSON: '%s'", MAC_RECEIVE);
    }
}

void setupWiFiMode()
{
    if (isWifiApMode == 0)
    { // AP MODE
        LOGI("📡 Starting AP Mode...");
        ledMode = LED_AP_MODE;

        // ใน Core 3.x การล้างค่า WiFi ให้ใช้การตั้ง Mode โดยตรง
        WiFi.persistent(false);
        WiFi.mode(WIFI_AP);

        // กำหนด IP สำหรับ AP Mode ชัดเจน
        IPAddress apIP(192, 168, 4, 1);
        IPAddress apSubnet(255, 255, 255, 0);
        WiFi.softAPConfig(apIP, apIP, apSubnet);

        // เช็คว่า DEVICE_NAME มีค่าจริงไหม หากไม่มีให้ใช้ชื่อ Default
        const char *apSSID = (DEVICE_NAME && strlen(DEVICE_NAME) > 0) ? DEVICE_NAME : "ESP32S3 WAVESHARE";

        bool apCreated = WiFi.softAP(
            apSSID,
            "12345678",
            6, // channel
            0, // hidden (0 = visible)
            4  // max client
        );

        if (apCreated)
        {
            LOGI("✅ AP Started | SSID: %s | IP: %s",
                 WiFi.softAPSSID().c_str(),
                 WiFi.softAPIP().toString().c_str());
        }
        else
        {
            LOGE("❌ Failed to start AP Mode!");
        }
    }
    else
    { // STA MODE
        LOGI("📡 Connecting to WiFi: %s", WIFI_SSID);
        ledMode = LED_DISCONNECTED;
        WiFi.mode(WIFI_STA);
        WiFi.setSleep(false);
        WiFi.begin(WIFI_SSID, WIFI_PASS);
        mac_config();

        int timeoutCounter = 0;
        while (WiFi.status() != WL_CONNECTED && timeoutCounter < 20) // รอนานสุด 10 วินาที
        {
            vTaskDelay(pdMS_TO_TICKS(500)); // เปลี่ยน delay เป็น vTaskDelay เพื่อไม่บล็อก Task อื่น
            timeoutCounter++;
        }

        if (WiFi.status() == WL_CONNECTED)
        {
            ledMode = LED_CONNECTED;
            LOGI("✅ WiFi Connected | SSID: %s | IP: %s | RSSI: %d dBm",
                 WiFi.SSID().c_str(),
                 WiFi.localIP().toString().c_str(),
                 WiFi.RSSI());
        }
        else
        {
            LOGE("❌ WiFi Connection Failed (Timeout)");
        }
    }
}

void setupIPConfig()
{
    if (isIpConfigStatic == 1)
    {
        LOGI("🌐 Setting Static IP: %s", IP_ADDR);
        IPAddress local_IP = parseIP(IP_ADDR);
        IPAddress subnet = parseIP(SUBNET_MASK);
        IPAddress gateway = parseIP(GATEWAY);

        if (!WiFi.config(local_IP, gateway, subnet))
        {
            LOGE("❌ Static IP Configuration Failed");
        }
    }
}

IPAddress parseIP(const char *ipStr)
{
    IPAddress ip;
    if (ip.fromString(ipStr))
        return ip;
    return IPAddress(0, 0, 0, 0);
}

void WiFiEvent(WiFiEvent_t event)
{
    switch (event)
    {
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
        LOGI("✅ GOT IP Event: %s", WiFi.localIP().toString().c_str());
        snprintf(DIVICE_IP, sizeof(DIVICE_IP), "%s", WiFi.localIP().toString().c_str());
        ledMode = LED_CONNECTED;

        // NTP Sync
        configTime(7 * 3600, 0, "time.google.com", "pool.ntp.org");

        if (ntpTaskHandle == NULL)
        {
            xTaskCreatePinnedToCore(TaskNTP, "NTP", 4096, NULL, 1, &ntpTaskHandle, 1);
        }
        break;

    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
        ledMode = LED_DISCONNECTED;
        LOGW("⚠️ WiFi Lost Connection.");
        // ใน Core 3.x ไม่จำเป็นต้องสั่ง WiFi.reconnect() ในนี้เพื่อป้องกัน Event Loop
        break;

    default:
        break;
    }
}

void keepWiFiAlive()
{
    if (isWifiApMode == 1)
    {
        static unsigned long lastReconnectCheck = 0;
        if (millis() - lastReconnectCheck > 20000)
        {
            if (WiFi.status() != WL_CONNECTED)
            {
                LOGW("🔄 KeepAlive: WiFi disconnected. Retrying connection...");
                WiFi.disconnect();
                WiFi.begin(WIFI_SSID, WIFI_PASS);
            }
            lastReconnectCheck = millis();
        }
    }
}

void showAPClients()
{
    if (isWifiApMode != 0)
        return;

    wifi_sta_list_t wifi_sta_list;
    esp_netif_sta_list_t netif_sta_list;

    if (esp_wifi_ap_get_sta_list(&wifi_sta_list) == ESP_OK)
    {
        if (esp_netif_get_sta_list(&wifi_sta_list, &netif_sta_list) == ESP_OK)
        {
            LOGD("👥 Connected AP Clients: %d", netif_sta_list.num);
            for (int i = 0; i < netif_sta_list.num; i++)
            {
                esp_netif_sta_info_t station = netif_sta_list.sta[i];
                LOGD(" - [%d] MAC: %02X:%02X:%02X:%02X:%02X:%02X | IP: %s",
                     i + 1,
                     station.mac[0], station.mac[1], station.mac[2],
                     station.mac[3], station.mac[4], station.mac[5],
                     ip4addr_ntoa((ip4_addr_t *)&station.ip.addr));
            }
        }
    }
}