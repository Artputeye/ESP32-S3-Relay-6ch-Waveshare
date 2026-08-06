// main.cpp
#include "config.h"
#include <esp_task_wdt.h> // ต้อง include header WDT ของ IDF 5.x

// --- Function Prototypes ---
void TaskMain(void *pvParameters);
void TaskSub(void *pvParameters);
void TaskLED(void *pvParameters);


WiFiClient espClient;
PubSubClient client(espClient);

void setup()
{
    // 1. Initial Hardware Pins
    pinMode(STATUS_LED, OUTPUT);
    digitalWrite(STATUS_LED, HIGH);
    pinMode(AP_PIN, INPUT_PULLUP);

    // 2. Setup Serial & Native USB CDC
    // ตั้งค่าไม่ให้ Serial.print ค้างรอ (Non-blocking) เมื่อบัฟเฟอร์ USB เต็ม
    Serial.setTxTimeoutMs(0); 
    Serial.begin(921600);

    // หน่วงเวลารอให้ USB CDC ของ ESP32-S3 สถาปนาการเชื่อมต่อกับคอมพิวเตอร์
    // ป้องกันปัญหา Log ใน setup() หล่นหายช่วงบูท
    unsigned long startUsb = millis();
    while (!Serial && (millis() - startUsb < 3500))
    {
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    vTaskDelay(pdMS_TO_TICKS(500)); // ให้เวลา Terminal ปรับตัวเล็กน้อย

    Serial.println();
    Serial.println("==================================================");
    Serial.println("🚀 ESP32-S3 System Booting...");
    Serial.println("==================================================");

    // 3. Mount LittleFS File System
    if (!LittleFS.begin(true))
    {
        Serial.println("❌ LittleFS Mount Failed!");
    }
    else
    {
        Serial.println("✅ LittleFS Mounted Successfully.");
        loadAllSettings();
    }

    // เริ่มต้นระบบ Logger และพิมพ์สถานะ
    LoggerBegin();

    LOGI("Booting Process Started...");
    for (int i = 0; i < 3; i++)
    {
        LOGI(".");
        vTaskDelay(pdMS_TO_TICKS(200));
    }

    displayLogs();
    checkAndLogResetReason();
    vTaskDelay(pdMS_TO_TICKS(200));

    // 4. Network & Home Assistant Setup
    network_setup();
    vTaskDelay(pdMS_TO_TICKS(200));

    // ต้องเรียก iotHAsetup() เสมอเพื่อ new Heap Memory ให้ Pointer เซนเซอร์
    iotHAsetup();

    if (WiFi.status() == WL_CONNECTED)
    {
        NTPbegin();
    }
    vTaskDelay(pdMS_TO_TICKS(200));

    if (MDNS.begin(HOST_NAME))
    {
        LOGI("[System] mDNS Started: %s.local", HOST_NAME);
    }
    vTaskDelay(pdMS_TO_TICKS(200));

    app_setup();
    vTaskDelay(pdMS_TO_TICKS(200));

    // =========================================================
    // 5. Watchdog Configuration (รองรับ Core 3.x / ESP-IDF v5.x)
    // =========================================================
#if defined(ESP_IDF_VERSION_MAJOR) && (ESP_IDF_VERSION_MAJOR >= 5)
    // สำหรับ Core 3.x (ESP-IDF v5.x)
    esp_task_wdt_config_t twdt_config = {
        .timeout_ms = WDT_TIMEOUT * 1000,
        .idle_core_mask = (1 << portNUM_PROCESSORS) - 1, // เฝ้าระวังทุก Core
        .trigger_panic = true
    };
    esp_task_wdt_init(&twdt_config);
#else
    // สำหรับ Core 2.x (ESP-IDF v4.x)
    esp_task_wdt_init(WDT_TIMEOUT, true);
    esp_task_wdt_delete(NULL);
#endif

    // =========================================================
    // 6. FreeRTOS Task Creation
    // =========================================================
    // Core 0: งานประมวลผลคำนวณหลัก
    // Core 1: งานสื่อสารและเครือข่ายทั้งหมด (WiFi, MQTT, Web)
    xTaskCreatePinnedToCore(TaskMain, "Main", 4096, NULL, 3, NULL, 0);
    xTaskCreatePinnedToCore(TaskSub, "Sub", 8192, NULL, 1, NULL, 1);
    xTaskCreatePinnedToCore(TaskLED, "LED", 2048, NULL, 1, NULL, 1);

    LOGI("[System] Setup Complete");
}

void loop()
{
    // ใน Core 3.x การสั่ง vTaskDelete(NULL) ใน loop() จะคืนทรัพยากรให้ Idle Task สมบูรณ์
    vTaskDelete(NULL);
}

// ---------------------------------------------------------
// TaskMain: งานประมวลผลหลัก (Core 0)
// ---------------------------------------------------------
void TaskMain(void *pvParameters)
{
    esp_task_wdt_add(NULL); // ลงทะเบียน Task นี้เข้า WDT
    unsigned long lastMain = 0;

    for (;;)
    {
        esp_task_wdt_reset();
        app_loop();

        if (millis() - lastMain > 60000)
        {
            lastMain = millis();
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// ---------------------------------------------------------
// TaskSub: งานสื่อสาร Network (Core 1)
// ---------------------------------------------------------
void TaskSub(void *pvParameters)
{
    esp_task_wdt_add(NULL); // ลงทะเบียน Task นี้เข้า WDT
    unsigned long lastStatus = 0;

    for (;;)
    {
        esp_task_wdt_reset();
        iotHAloop();
        wsProcess();
        apModeCheck();
        keepWiFiAlive();

        if (millis() - lastStatus > 10000)
        {
            lastStatus = millis();
            showAPClients();
        }
        vTaskDelay(pdMS_TO_TICKS(30));
    }
}

// ---------------------------------------------------------
// TaskLED: การแสดงผลไฟสถานะ (Core 1)
// ---------------------------------------------------------
void TaskLED(void *pvParameters)
{
    for (;;)
    {
        ledPatternSelect();
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}
