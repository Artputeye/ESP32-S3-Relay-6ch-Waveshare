#include "websocket_handler.h"

AsyncWebSocket ws("/ws");
unsigned long lastTimeMonitor = 0;
unsigned long lastPingTime = 0;
unsigned long MonitorDelay = 1000;         // อัปเดตสถานะให้เรียลไทม์ขึ้น (ทุก 1 วินาที)
const unsigned long pingInterval = 25000;   // 25 วินาที
String wsSerial;

String wsAllDataBase64()
{
    JsonDocument doc;

    // --- ข้อมูลระบบทั่วไป ---
    doc["device_name"] = DEVICE_NAME; // เพิ่ม device_name ส่งไปแสดงบนหน้า index.html และ relay.html
    doc["DIVICE_IP"] = DIVICE_IP;
    doc["rssi"] = WiFi.RSSI();
    doc["Serial"] = wsSerial;

    // --- สถานะ Relay ทั้ง 6 ช่อง ---
    JsonObject relays = doc["relays"].to<JsonObject>();
    for (int i = 0; i < 6; i++)
    {
        String key = "Relay" + String(i + 1);
        relays[key] = relay_states[i] ? "ON" : "OFF";
    }

    String jsonOut;
    serializeJson(doc, jsonOut);
    return base64::encode(jsonOut); // แปลงเป็น Base64 ส่งให้ Web UI
}

void notifyClients(const String &base64Msg)
{
    ws.textAll(base64Msg);
}

// -------------------------------------------------------------
// จัดการข้อความ/คำสั่งเปิด-ปิด Relay ที่ส่งมาจาก Web Client
// -------------------------------------------------------------
void handleWebSocketMessage(void *arg, uint8_t *data, size_t len)
{
    AwsFrameInfo *info = (AwsFrameInfo *)arg;
    if (info->final && info->index == 0 && info->len == len &&
        (info->opcode == WS_TEXT || info->opcode == WS_BINARY))
    {
        data[len] = 0; // Null-terminate string
        
        JsonDocument rxDoc;
        DeserializationError error = deserializeJson(rxDoc, (char *)data);

        if (!error)
        {
            if (rxDoc["relay"].is<int>())
            {
                int relayNum = rxDoc["relay"].as<int>(); // 1 - 6
                if (relayNum >= 1 && relayNum <= 6)
                {
                    int index = relayNum - 1;

                    if (rxDoc["state"].is<bool>())
                    {
                        bool state = rxDoc["state"].as<bool>();
                        relay_states[index] = state;
                        digitalWrite(RELAY_PINS[index], state ? HIGH : LOW);
                        LOGD("⚡ WebSocket Command: Relay %d -> %s", relayNum, state ? "ON" : "OFF");
                    }
                    else if (rxDoc["action"] == "toggle")
                    {
                        relay_states[index] = !relay_states[index];
                        digitalWrite(RELAY_PINS[index], relay_states[index] ? HIGH : LOW);
                        LOGD("⚡ WebSocket Command: Relay %d TOGGLED -> %s", relayNum, relay_states[index] ? "ON" : "OFF");
                    }
                }
                else
                {
                    LOGW("⚠️ Invalid Relay Number received: %d", relayNum);
                }
            }
        }
        else
        {
            LOGW("⚠️ WebSocket JSON Parse Failed: %s", error.c_str());
        }

        notifyClients(wsAllDataBase64());
    }
}

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type,
             void *arg, uint8_t *data, size_t len)
{
    switch (type)
    {
    case WS_EVT_CONNECT:
        LOGI("🔌 WebSocket Client #%u connected from %s",
             client->id(), client->remoteIP().toString().c_str());
        // ส่งสถานะปัจจุบันให้ Client ที่เพิ่งเชื่อมต่อทันที
        client->text(wsAllDataBase64());
        break;

    case WS_EVT_DISCONNECT:
        LOGI("🔌 WebSocket Client #%u disconnected", client->id());
        break;

    case WS_EVT_DATA:
        handleWebSocketMessage(arg, data, len);
        break;

    case WS_EVT_PONG:
        LOGD("🏓 WebSocket Client #%u pong", client->id());
        break;

    case WS_EVT_ERROR:
        LOGE("❌ WebSocket Client #%u error (%u): %s", client->id(), *((uint16_t *)arg), (char *)data);
        break;
    }
}

void ws_init()
{
    ws.onEvent(onEvent);
    server.addHandler(&ws);
    LOGI("🌐 WebSocket Server Initialized (/ws)");
}

void wsJsonSerial(const String &msg)
{
    wsSerial = msg;
    if (!wsSerial.isEmpty())
    {
        notifyClients(wsAllDataBase64());
        wsSerial = "";
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

void wsProcess()
{
    if (millis() - lastTimeMonitor > MonitorDelay)
    {
        lastTimeMonitor = millis();
        notifyClients(wsAllDataBase64());
    }

    if (millis() - lastPingTime > pingInterval)
    {
        lastPingTime = millis();
        ws.pingAll();
    }
    ws.cleanupClients();
}