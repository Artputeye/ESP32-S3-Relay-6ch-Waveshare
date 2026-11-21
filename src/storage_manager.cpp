// storage_manager.cpp
#include "storage_manager.h"

const char *targetDirectory = "/";
String energyFile = "/energy.json";
unsigned long lastSaveTime = 0;
unsigned long lastClearEnergy = 0;
const unsigned long saveInterval = 1UL * 60UL * 1000UL;

// สร้างตัวแปร Global สำหรับเก็บค่า Friendly Names 6 ช่อง
String friendlyNames[6] = {
    "Relay Channel 1",
    "Relay Channel 2",
    "Relay Channel 3",
    "Relay Channel 4",
    "Relay Channel 5",
    "Relay Channel 6"
};

void listAllFiles(const char *dirname, uint8_t levels)
{
    LOGI("Listing directory: %s\n", dirname);

    File root = LittleFS.open(dirname);
    if (!root || !root.isDirectory())
    {
        LOGI("❌ Failed to open directory or not a directory");
        return;
    }

    File file = root.openNextFile();
    while (file)
    {
        if (file.isDirectory())
        {
            LOGI("  DIR : %s\n", file.name());
            if (levels > 0)
            {
                listAllFiles(file.path(), levels - 1);
            }
        }
        else
        {
            LOGI("  FILE: %-20s  SIZE: %u bytes\n", file.name(), file.size());
        }
        file = root.openNextFile();
    }
}

void formatFS()
{
    LOGI("⚠️ Formatting LittleFS...");
    if (LittleFS.format())
    {
        LOGI("✅ Format successful");
    }
    else
    {
        LOGI("❌ Format failed");
    }
}

bool loadJsonFile(const char *filename, JsonDocument &doc)
{
    if (!LittleFS.exists(filename))
    {
        LOGI("⚠️ File not found: %s\n", filename);
        return false;
    }

    File file = LittleFS.open(filename, "r");
    if (!file || file.size() == 0)
    {
        if (file) file.close();
        return false;
    }

    DeserializationError error = deserializeJson(doc, file);
    file.close();

    if (error)
    {
        LOGI("❌ JSON Parse Error (%s): %s\n", filename, error.c_str());
        return false;
    }
    return true;
}

bool saveJsonFile(const char *filename, const JsonDocument &doc)
{
    File file = LittleFS.open(filename, "w");
    if (!file)
    {
        LOGI("❌ Failed to open %s for writing\n", filename);
        return false;
    }

    if (serializeJson(doc, file) == 0)
    {
        LOGI("❌ Failed to write to %s\n", filename);
        file.close();
        return false;
    }

    file.close();
    LOGI("✅ Saved: %s\n", filename);
    return true;
}

bool loadAllSettings()
{
    LOGD("--- [FS] Loading General Settings (/setting.json) ---");
    JsonDocument doc;
    
    if (loadJsonFile("/setting.json", doc))
    {
        // 1. อ่านค่า friendly_name_1 ถึง friendly_name_6
        for (int i = 0; i < 6; i++) {
            String key = "friendly_name_" + String(i + 1);
            
            // เช็คว่ามี Key และค่าไม่เป็น String ว่างเปล่า
            if (doc[key].is<String>() && !doc[key].as<String>().isEmpty()) {
                friendlyNames[i] = doc[key].as<String>();
            }
            // ถ้าว่างหรือไม่มี Key จะคงค่า friendlyNames[i] เดิมเอาไว้
        }

        // 2. เช็คเงื่อนไขคีย์ device_name
        if (doc["device_name"].is<const char*>() && strlen(doc["device_name"].as<const char*>()) > 0) {
            strlcpy(DEVICE_NAME, doc["device_name"].as<const char*>(), sizeof(DEVICE_NAME));
            LOGD("  Device Name loaded: %s", DEVICE_NAME);
        } else {
            strlcpy(DEVICE_NAME, "ESP32-S3-Relay-6CH", sizeof(DEVICE_NAME));
            LOGW("⚠️ Key 'device_name' empty or not found in setting.json (Using default: %s)", DEVICE_NAME);
        }

        LOGI("✅ General settings loaded successfully");
        for (int i = 0; i < 6; i++) {
            LOGD("   - Friendly Name %d: %s", i + 1, friendlyNames[i].c_str());
        }
    }
    else
    {
        LOGW("⚠️ Failed to load /setting.json (Using default names)");
    }

    return true;
}

bool saveWifiModeSetting()
{
    JsonDocument doc;
    loadJsonFile("/networkconfig.json", doc);
    doc["wifi_mode"] = isWifiApMode;
    return saveJsonFile("/networkconfig.json", doc);
}