//storage_manager.h
#ifndef STORAGE_MANAGER_H
#define STORAGE_MANAGER_H
#include "config.h"

// ตัวแปรสำหรับเก็บค่า Friendly Names (6 ช่อง)
extern String friendlyNames[6]; 

// หรือหากต้องการดึงเป็นตัวแปรเดี่ยวๆ สามารถเปิดใช้ได้ครับ
// extern String friendlyName1, friendlyName2, friendlyName3, friendlyName4, friendlyName5, friendlyName6;

extern bool isWifiApMode; // สมมติว่ามีจากระบบ Networkเดิม

// ฟังก์ชันสำหรับ Storage Management
void listAllFiles(const char *dirname, uint8_t levels);
void formatFS();
bool loadJsonFile(const char *filename, JsonDocument &doc);
bool saveJsonFile(const char *filename, const JsonDocument &doc);
bool loadAllSettings();
bool saveWifiModeSetting();

#endif