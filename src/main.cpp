// main.cpp
#include "config.h"
#include <esp_task_wdt.h>

// --- Function Prototypes ---
void TaskMain(void *pvParameters);
void TaskSub(void *pvParameters);
void TaskLED(void *pvParameters);

WiFiClient espClient;
PubSubClient client(espClient);

// Boot messages are written in order after the USB CDC host is ready.
static void bootMark(const char *message)
{
    Serial.printf("[BOOT] %s | uptime=%lu ms | heap=%u\r\n",
                  message,
                  static_cast<unsigned long>(millis()),
                  ESP.getFreeHeap());
}

void setup()
{
    pinMode(STATUS_LED, OUTPUT);
    digitalWrite(STATUS_LED, HIGH);
    pinMode(AP_PIN, INPUT_PULLUP);

    // Native USB CDC: wait briefly for the Serial Monitor to reconnect after reset.
    // This bounded wait makes the boot history visible when the monitor is open,
    // while still allowing normal startup if no host is connected.
    Serial.begin(115200);
    const uint32_t usbWaitStarted = millis();
    while (!Serial && (millis() - usbWaitStarted < 5000))
    {
        delay(10);
    }
    // LittleFS
    if (!LittleFS.begin(true))
    {
        Serial.println("[BOOT][ERROR] LittleFS mount failed");
        Serial.flush();
    }
    else
    {
        bootMark("LittleFS mounted");
        loadAllSettings();
        bootMark("Settings loaded");
    }

    LOGI("Booting Process Started...");
    for (int i = 0; i < 3; i++)
    {
        LOGI(".");
        vTaskDelay(pdMS_TO_TICKS(200));
    }

    displayLogs();
    checkAndLogResetReason();
    bootMark("Boot log history displayed");

    // Start asynchronous logger only after the history has been printed,
    // preventing LoggerTask output from interleaving with the history.
    LoggerBegin();
    bootMark("LoggerBegin returned");
    LOGI("[Logger] Normal boot up sequence.");

    // Network and Home Assistant
    network_setup();
    bootMark("Network setup returned");
    vTaskDelay(pdMS_TO_TICKS(200));

    iotHAsetup();
    bootMark("Home Assistant setup returned");

    if (WiFi.status() == WL_CONNECTED)
    {
        NTPbegin();
        bootMark("NTP setup returned");
    }

    vTaskDelay(pdMS_TO_TICKS(200));
    if (MDNS.begin(HOST_NAME))
    {
        LOGI("[System] mDNS Started: %s.local", HOST_NAME);
        bootMark("mDNS started");
    }
    else
    {
        bootMark("mDNS start failed");
    }

    app_setup();
    bootMark("Application setup returned");
    vTaskDelay(pdMS_TO_TICKS(200));

    // Watchdog configuration (ESP-IDF 5.x / Arduino Core 3.x)
#if defined(ESP_IDF_VERSION_MAJOR) && (ESP_IDF_VERSION_MAJOR >= 5)
    esp_task_wdt_config_t twdt_config = {
        .timeout_ms = WDT_TIMEOUT * 1000,
        .idle_core_mask = (1 << portNUM_PROCESSORS) - 1,
        .trigger_panic = true
    };
    esp_err_t wdtInitResult = esp_task_wdt_init(&twdt_config);
#else
    esp_err_t wdtInitResult = esp_task_wdt_init(WDT_TIMEOUT, true);
#endif

    if (wdtInitResult == ESP_OK)
    {
        bootMark("Watchdog initialized");
    }
    else if (wdtInitResult == ESP_ERR_INVALID_STATE)
    {
        Serial.println("[BOOT][WARN] Watchdog was already initialized; keeping existing configuration.");
        Serial.flush();
    }
    else
    {
        Serial.printf("[BOOT][ERROR] Watchdog init failed: %s (0x%X)\r\n",
                      esp_err_to_name(wdtInitResult),
                      static_cast<unsigned int>(wdtInitResult));
        Serial.flush();
    }

    // FreeRTOS task creation: report every result directly to USB CDC.
    BaseType_t taskResult = xTaskCreatePinnedToCore(TaskMain, "Main", 4096, NULL, 3, NULL, 0);
    if (taskResult == pdPASS)
        bootMark("Main Task created");
    else
        Serial.printf("[BOOT][ERROR] Main Task creation failed: %d\r\n", static_cast<int>(taskResult));

    taskResult = xTaskCreatePinnedToCore(TaskSub, "Sub", 8192, NULL, 1, NULL, 1);
    if (taskResult == pdPASS)
        bootMark("Sub Task created");
    else
        Serial.printf("[BOOT][ERROR] Sub Task creation failed: %d\r\n", static_cast<int>(taskResult));

    taskResult = xTaskCreatePinnedToCore(TaskLED, "LED", 2048, NULL, 1, NULL, 1);
    if (taskResult == pdPASS)
        bootMark("LED Task created");
    else
        Serial.printf("[BOOT][ERROR] LED Task creation failed: %d\r\n", static_cast<int>(taskResult));

    bootMark("Setup complete");
}

void loop()
{
    vTaskDelete(NULL);
}

// ---------------------------------------------------------
// TaskMain: main application processing (Core 0)
// ---------------------------------------------------------
void TaskMain(void *pvParameters)
{
    esp_err_t wdtResult = esp_task_wdt_add(NULL);
    if (wdtResult != ESP_OK)
    {
        Serial.printf("[WDT][Main] registration failed: %s (0x%X)\r\n",
                      esp_err_to_name(wdtResult),
                      static_cast<unsigned int>(wdtResult));
    }

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
// TaskSub: network communication (Core 1)
// ---------------------------------------------------------
void TaskSub(void *pvParameters)
{
    esp_err_t wdtResult = esp_task_wdt_add(NULL);
    if (wdtResult != ESP_OK)
    {
        Serial.printf("[WDT][Sub] registration failed: %s (0x%X)\r\n",
                      esp_err_to_name(wdtResult),
                      static_cast<unsigned int>(wdtResult));
    }

    unsigned long lastStatus = 0;
    unsigned long lastHeartbeat = 0;
    for (;;)
    {
        uint32_t haDuration = 0;
        uint32_t wsDuration = 0;
        uint32_t apDuration = 0;
        uint32_t wifiDuration = 0;

        esp_task_wdt_reset();

        uint32_t stepStarted = millis();
        iotHAloop();
        haDuration = millis() - stepStarted;

        stepStarted = millis();
        wsProcess();
        wsDuration = millis() - stepStarted;

        stepStarted = millis();
        apModeCheck();
        apDuration = millis() - stepStarted;

        stepStarted = millis();
        keepWiFiAlive();
        wifiDuration = millis() - stepStarted;

        // Direct USB CDC heartbeat, deliberately bypassing the Logger queue.
        if (millis() - lastHeartbeat >= 5000)
        {
            lastHeartbeat = millis();
            Serial.printf("[HEARTBEAT] uptime=%lu ms heap=%u wifi=%s mqtt=%s\r\n",
                          static_cast<unsigned long>(millis()),
                          ESP.getFreeHeap(),
                          WiFi.status() == WL_CONNECTED ? "connected" : "disconnected",
                          client.connected() ? "connected" : "disconnected");
        }

        if (millis() - lastStatus >= 10000)
        {
            lastStatus = millis();
            LOGI("[TaskSub] alive | MQTT=%s(state=%d) | WiFi=%s | RSSI=%d | heap=%u | HA=%lu ms WS=%lu ms AP=%lu ms WiFi=%lu ms",
                 client.connected() ? "connected" : "disconnected",
                 client.connected() ? MQTT_CONNECTED : client.state(),
                 WiFi.status() == WL_CONNECTED ? "connected" : "disconnected",
                 WiFi.status() == WL_CONNECTED ? WiFi.RSSI() : 0,
                 ESP.getFreeHeap(),
                 static_cast<unsigned long>(haDuration),
                 static_cast<unsigned long>(wsDuration),
                 static_cast<unsigned long>(apDuration),
                 static_cast<unsigned long>(wifiDuration));
            showAPClients();
        }

        if (haDuration > 1000 || wsDuration > 1000 || apDuration > 1000 || wifiDuration > 1000)
        {
            LOGW("[TaskSub] slow call detected | HA=%lu ms WS=%lu ms AP=%lu ms WiFi=%lu ms",
                 static_cast<unsigned long>(haDuration),
                 static_cast<unsigned long>(wsDuration),
                 static_cast<unsigned long>(apDuration),
                 static_cast<unsigned long>(wifiDuration));
        }

        esp_task_wdt_reset();
        vTaskDelay(pdMS_TO_TICKS(30));
    }
}

// ---------------------------------------------------------
// TaskLED: status LED pattern (Core 1)
// ---------------------------------------------------------
void TaskLED(void *pvParameters)
{
    for (;;)
    {
        ledPatternSelect();
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}
