#include "logger.h"

#define LOG_FILE_PATH "/system_log.txt"
#define MAX_LOG_SIZE_BYTES 10240 

struct LogMessage
{
    char text[256];
};

static QueueHandle_t logQueue;

void LoggerBegin()
{
    logQueue = xQueueCreate(100, sizeof(LogMessage));

    xTaskCreatePinnedToCore(
        LoggerTask,
        "Logger",
        4096,
        NULL,
        1,
        NULL,
        0);
}

void logPrintf(const char *level, const char *file, int line, const char *fmt, ...)
{
    if (logQueue == NULL) return;

    LogMessage msg;

    int offset = snprintf(msg.text, sizeof(msg.text),
                          "[%10lu][%-5s][%s:%d] ",
                          millis(), level, file, line);

    va_list args;
    va_start(args, fmt);
    vsnprintf(msg.text + offset, sizeof(msg.text) - offset, fmt, args);
    va_end(args);

    xQueueSend(logQueue, &msg, 0); 
}

void LoggerTask(void *pv)
{
    LogMessage msg;

    while (true)
    {
        if (xQueueReceive(logQueue, &msg, portMAX_DELAY))
        {
            Serial.println(msg.text); // พิมพ์ออกเป็นบรรทัดเต็มๆ เสมอ
        }
        vTaskDelay(pdMS_TO_TICKS(1)); 
    }
}

// ฟังก์ชันสำหรับบันทึก Log ลง LittleFS
void writeLog(const String &level, const String &message) {
    if (!LittleFS.begin(true)) return;

    // เช็คขนาดไฟล์
    if (LittleFS.exists(LOG_FILE_PATH)) {
        File checkFile = LittleFS.open(LOG_FILE_PATH, "r");
        if (checkFile && checkFile.size() > MAX_LOG_SIZE_BYTES) {
            checkFile.close();
            LittleFS.remove(LOG_FILE_PATH);
        } else if (checkFile) {
            checkFile.close();
        }
    }

    // ใช้โหมด "a+" เพื่อสร้างไฟล์อัตโนมัติหากยังไม่มี
    File logFile = LittleFS.open(LOG_FILE_PATH, "a+");
    if (!logFile) return;

    String logLine = "[" + String(millis() / 1000) + "s] [" + level + "] " + message + "\n";
    logFile.print(logLine);
    logFile.close();
}

void displayLogs() {
    Serial.println(F("\n====== [ SYSTEM LOG HISTORY ] ======"));
    
    // ตรวจสอบก่อนว่ามีไฟล์หรือไม่ หากไม่มีให้สร้างไฟล์เปล่าไว้ก่อนเลยเพื่อไม่ให้เกิด Error 105
    if (!LittleFS.exists(LOG_FILE_PATH)) {
        File createFile = LittleFS.open(LOG_FILE_PATH, "w");
        if(createFile) createFile.close();
        Serial.println(F("No log history found. System is clean."));
        Serial.println(F("====================================\n"));
        return;
    }

    File logFile = LittleFS.open(LOG_FILE_PATH, "r");
    if (!logFile) {
        Serial.println(F("❌ Failed to open log file for reading."));
        return;
    }

    while (logFile.available()) {
        Serial.write(logFile.read());
    }
    logFile.close();
    Serial.println(F("====================================\n"));
}

void checkAndLogResetReason() {
    RESET_REASON reason_core0 = rtc_get_reset_reason(0);
    RESET_REASON reason_core1 = rtc_get_reset_reason(1);

    if (reason_core0 == RTCWDT_RTC_RESET || reason_core0 == TG0WDT_CPU_RESET || reason_core0 == TG1WDT_CPU_RESET) {
        String wdtMessage = "System crash detected! Reset by Watchdog. Core0 Reason: " + 
                            String(reason_core0) + ", Core1 Reason: " + String(reason_core1);
        writeLog("CRITICAL", wdtMessage);
    } else {
        Serial.println(F("[Logger] Normal boot up sequence."));
    }
}

// ฟังก์ชันสำหรับล้างประวัติ Log ทั้งหมด
void clearLogHistory() {
    if (!LittleFS.begin(true)) {
        LOGE("[Logger] LittleFS not ready, cannot clear log.");
        return;
    }

    if (LittleFS.exists(LOG_FILE_PATH)) {
        if (LittleFS.remove(LOG_FILE_PATH)) {
            LOGI("✅ System log cleared successfully.");
            
            // สร้างไฟล์เปล่าขึ้นมาใหม่ทันที เพื่อป้องกัน Error 105 (no permits for creation) ตอนอ่านครั้งต่อไป
            File f = LittleFS.open(LOG_FILE_PATH, "w");
            if (f) f.close();
        } else {
            LOGE("❌ Failed to remove system log file.");
        }
    } else {
        LOGI("ℹ️ System log file does not exist. Nothing to clear.");
    }
}