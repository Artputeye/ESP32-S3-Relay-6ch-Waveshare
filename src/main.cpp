// main.cpp
#include "config.h"

// --- Function Prototypes ---
void TaskMain(void *pvParameters);
void TaskSub(void *pvParameters);
void TaskLED(void *pvParameters);
void TaskSerialReader(void *pvParameters);

WiFiClient espClient;
PubSubClient client(espClient);

void setup()
{
    pinMode(STATUS_LED, OUTPUT);
    digitalWrite(STATUS_LED, HIGH);
    pinMode(AP_PIN, INPUT_PULLUP);

    Serial.begin(115200);
    delay(500); // ให้เวลาฮาร์ดแวร์ UART นิ่งก่อน
    Serial.flush(); // ล้างข้อมูลค้างใน Buffer
    // รอให้ Serial พร้อมทำงาน (สูงสุดไม่เกิน 3 วินาที)
    unsigned long start = millis();
    while (!Serial && (millis() - start < 3000));

    // 1. Mount LittleFS ก่อนเสมอ
    if (!LittleFS.begin(true))
    {
        Serial.println("❌ LittleFS Mount Failed!");
    }
    else
    {
        Serial.println("✅ LittleFS Mounted Successfully.");
        loadAllSettings();
    }

    LoggerBegin();

    unsigned long startSerial = millis();
    while (!Serial && millis() - startSerial < 3000)
        ;

    LOGI("Booting");
    for (int i = 0; i < 5; i++)
    {
        LOGI(".");
        delay(300);
    }
    Serial.println();
    delay(500);

    displayLogs(); // 2. เรียกใช้งานหลัง Mount LittleFS แล้ว
    checkAndLogResetReason();
    delay(500);

    network_setup();
    delay(500);

    if (WiFi.status() == WL_CONNECTED)
    {
        iotHAsetup();
        NTPbegin();
    }
    delay(500);

    if (MDNS.begin(device_id))
    {
        LOGI("[System] mDNS Started: %s.local\n", device_id);
    }
    delay(500);

    app_setup();
    delay(500);

    // Watchdog Configuration (ESP32-IDF Style)
    // หมายเหตุ: หากใช้ ESP32 Core 3.x ขึ้นไป อาจต้องปรับ syntax ตามที่แจ้งในรอบก่อน
    esp_task_wdt_init(WDT_TIMEOUT, true);
    esp_task_wdt_delete(NULL); // ไม่ใช้ Loop() หลักคุม WDT

    // Task Creation (แบ่งโหลดความสำคัญ)
    // Core 0: งานหลัก (Processing/Operation)
    // Core 1: งานสื่อสาร (WiFi/MQTT/Web) และ UI (LED)
    xTaskCreatePinnedToCore(TaskMain, "Main", 4096, NULL, 3, NULL, 0);
    xTaskCreatePinnedToCore(TaskSub, "Sub", 8192, NULL, 1, NULL, 1);
    xTaskCreatePinnedToCore(TaskLED, "LED", 2048, NULL, 1, NULL, 1);
    xTaskCreatePinnedToCore(TaskSerialReader, "SerialReader", 4096, NULL, 1, NULL, 0);

    LOGI(("[System] Setup Complete"));
}

void loop()
{
    // ปล่อยว่างไว้ เพราะเราใช้ TaskManagement (FreeRTOS)
    vTaskDelete(NULL);
}

// ---------------------------------------------------------
// TaskMain: งานประมวลผลหลัก (Core 0)
// ---------------------------------------------------------
void TaskMain(void *pvParameters)
{
    esp_task_wdt_add(NULL); // เพิ่มตัวมันเองลงใน Watchdog
    unsigned long lastMain = 0;
    for (;;)
    {
        esp_task_wdt_reset();
        app_loop();

        if (millis() - lastMain > 60000)
        {
            lastMain = millis();
            // ตรวจสอบ Stack เพื่อป้องกันโปรแกรมแฮงค์ (Debug)
            // LOGI("[Debug] Main Stack: %u\n", uxTaskGetStackHighWaterMark(NULL));
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// ---------------------------------------------------------
// TaskSub: งานสื่อสาร Network (Core 1)
// ---------------------------------------------------------
void TaskSub(void *pvParameters)
{
    esp_task_wdt_add(NULL);
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
        vTaskDelay(pdMS_TO_TICKS(30)); // ปรับเป็น 30ms ช่วยคืน CPU ให้ WebServer
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

// ---------------------------------------------------------
// Task สำหรับดักฟัง Serial (Core 0)
// ---------------------------------------------------------
void TaskSerialReader(void *pvParameters)
{
    static String buffer = "";
    for (;;)
    {
        while (Serial.available() > 0)
        {
            char c = Serial.read();
            if (c == '\n') // อ่านจนจบบรรทัด
            {
                buffer.trim();
                if (buffer.length() > 0)
                {
                    LOGI("Received: ");
                    LOGI("%s", buffer.c_str());
                    processSerialCommand(buffer);
                }
                buffer = ""; // เคลียร์บัฟเฟอร์
            }
            else if (c != '\r')
            {
                buffer += c;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(20)); // หน่วงเวลาให้ Task อื่นทำงานได้
    }
}