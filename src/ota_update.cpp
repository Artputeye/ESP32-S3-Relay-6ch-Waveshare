//ota_update.CPP
#include "ota_update.h"

void setupOTAManagement() {
    handleFirmwareUpload();
    handleFileSystemUpload();
    handleFileList();
    handleFileDelete();
}

// --- 1. Firmware & Partition Update ---
static volatile bool firmwareUploadComplete = false;
static volatile bool firmwareUploadFailed = false;

static void restartAfterFirmwareOTA(void *parameter)
{
    // Give AsyncWebServer time to flush the HTTP response to the browser.
    vTaskDelay(pdMS_TO_TICKS(2500));
    ESP.restart();
    vTaskDelete(NULL);
}

void handleFirmwareUpload()
{
    server.on("/otafirmware", HTTP_POST,
        [](AsyncWebServerRequest *request)
        {
            if (firmwareUploadComplete && !firmwareUploadFailed && !Update.hasError())
            {
                BaseType_t taskCreated = xTaskCreate(
                    restartAfterFirmwareOTA,
                    "OTA_Restart",
                    2048,
                    NULL,
                    1,
                    NULL);

                if (taskCreated == pdPASS)
                {
                    request->send(200, "text/plain", "Firmware uploaded successfully. Device restarting.");
                }
                else
                {
                    LOGE("[OTA] Could not create restart task.");
                    request->send(500, "text/plain", "Firmware uploaded, but restart task could not be created. Please restart the device manually.");
                }
            }
            else
            {
                ledMode = LED_DISCONNECTED;
                request->send(500, "text/plain", "Firmware update failed. Device was not restarted.");
                LOGE("[OTA] Upload failed; device will remain running.");
            }
        },
        [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final)
        {
            if (index == 0)
            {
                firmwareUploadComplete = false;
                firmwareUploadFailed = false;
                ledMode = LED_BUSY;
                LOGI("[OTA] Start: %s", filename.c_str());

                if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH))
                {
                    firmwareUploadFailed = true;
                    Update.printError(Serial);
                }
            }

            if (!firmwareUploadFailed && !Update.hasError())
            {
                if (Update.write(data, len) != len)
                {
                    firmwareUploadFailed = true;
                    Update.printError(Serial);
                }
            }

            if (final)
            {
                if (!firmwareUploadFailed && Update.end(true))
                {
                    firmwareUploadComplete = true;
                    LOGI("[OTA] Success: %u bytes", static_cast<unsigned int>(index + len));
                }
                else
                {
                    firmwareUploadFailed = true;
                    firmwareUploadComplete = false;
                    ledMode = LED_DISCONNECTED;
                    Update.printError(Serial);
                    LOGE("[OTA] Finalizing firmware failed.");
                }
            }
        });
}

// --- 2. Upload File to LittleFS ---
void handleFileSystemUpload() {
    server.on("/otalittlefs", HTTP_POST, 
        [](AsyncWebServerRequest *request) { request->send(200); },
        [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
            if (!filename.startsWith("/")) filename = "/" + filename;

            if (index == 0) {
                // ตรวจสอบและสร้าง Directory อัตโนมัติ
                String path = filename;
                int lastSlash = path.lastIndexOf('/');
                if (lastSlash > 0) {
                    String dirPath = path.substring(0, lastSlash);
                    if (!FILESYSTEM.exists(dirPath)) {
                        FILESYSTEM.mkdir(dirPath);
                    }
                }
                
                LOGI("[FS] Uploading: %s\n", filename.c_str());
                fsUploadFile = FILESYSTEM.open(filename, "w");
            }

            if (fsUploadFile) {
                fsUploadFile.write(data, len);
            }

            if (final) {
                if (fsUploadFile) fsUploadFile.close();
                LOGI("[FS] Upload Complete: %u bytes\n", index + len);
            }
        }
    );
}

// --- 3. List Files (JSON) ---
void handleFileList() {
    server.on("/list", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (!request->hasParam("dir")) {
            request->send(400, "text/plain", "Missing 'dir' param");
            return;
        }

        String path = request->getParam("dir")->value();
        File root = FILESYSTEM.open(path);
        
        String output = "[";
        if (root && root.isDirectory()) {
            File file = root.openNextFile();
            while (file) {
                if (output != "[") output += ",";
                
                output += "{\"type\":\"";
                output += (file.isDirectory() ? "dir" : "file");
                output += "\",\"name\":\"";
                output += String(file.path());
                output += "\",\"size\":";
                output += String(file.size());
                output += "}";
                
                file = root.openNextFile();
            }
        }
        output += "]";
        request->send(200, "application/json", output);
    });
}

// --- 4. Delete File ---
void handleFileDelete() {
    server.on("/delete", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (!request->hasParam("file")) {
            request->send(400, "text/plain", "Missing 'file' param");
            return;
        }

        String path = request->getParam("file")->value();
        
        // ป้องกันการลบ Root หรือไฟล์ที่ไม่มีอยู่จริง
        if (path == "/" || !FILESYSTEM.exists(path)) {
            request->send(404, "text/plain", "Invalid Path");
            return;
        }

        if (FILESYSTEM.remove(path)) {
            LOGI("[FS] Deleted: %s\n", path.c_str());
            request->send(200, "text/plain", "Deleted");
        } else {
            request->send(500, "text/plain", "Delete Failed");
        }
    });
}