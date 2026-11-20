#include "ha_integration.h"
#include <WiFi.h>

const char *discovery_prefix = "homeassistant";
char availability_topic[128] = "";
char state_topic[128] = "";

unsigned long lastMsg = 0;

// ฟังก์ชันช่วยดึง MAC Address แบบไม่มีเครื่องหมาย : (เช่น F412FA47A300)
String getMacClean()
{
  String mac = WiFi.macAddress();
  mac.replace(":", "");
  return mac;
}

void mqtt_callback(char *topic, byte *payload, unsigned int length);

void init_ha_topics()
{
  snprintf(availability_topic, sizeof(availability_topic), "homeassistant/switch/%s/availability", DEVICE_NAME);
  snprintf(state_topic, sizeof(state_topic), "homeassistant/switch/%s/state", DEVICE_NAME);

  LOGI("MQTT Topics Initialized:");
  LOGI("  Availability : %s", availability_topic);
  LOGI("  State        : %s", state_topic);
}

void iotHAsetup()
{
  loadAllSettings();
  init_ha_topics();

  client.setServer(MQTT_SERVER, MQTT_PORT);
  client.setCallback(mqtt_callback);
  client.setBufferSize(4096);
  client.setKeepAlive(60);
  client.setSocketTimeout(60);

  LOGI("[MQTT] Diagnostic enabled: broker=%s:%u, username=%s, keepalive=60s, socket_timeout=60s",
       MQTT_SERVER, MQTT_PORT, MQTT_USER);
}

// แปลรหัสสถานะจาก PubSubClient เพื่อใช้วิเคราะห์สาเหตุที่ MQTT หลุด
const char *mqttStateName(int state)
{
  switch (state)
  {
  case MQTT_CONNECTION_TIMEOUT: return "connection timeout";
  case MQTT_CONNECTION_LOST: return "connection lost";
  case MQTT_CONNECT_FAILED: return "connect failed";
  case MQTT_DISCONNECTED: return "disconnected";
  case MQTT_CONNECTED: return "connected";
  case MQTT_CONNECT_BAD_PROTOCOL: return "bad protocol";
  case MQTT_CONNECT_BAD_CLIENT_ID: return "bad client ID";
  case MQTT_CONNECT_UNAVAILABLE: return "broker unavailable";
  case MQTT_CONNECT_BAD_CREDENTIALS: return "bad credentials";
  case MQTT_CONNECT_UNAUTHORIZED: return "unauthorized";
  default: return "unknown MQTT state";
  }
}

void iotHAloop()
{
  static bool wasConnected = false;
  const bool connectedBeforeLoop = client.connected();

  // บันทึกเฉพาะตอนเปลี่ยนจาก Connected เป็น Disconnected ไม่พิมพ์ซ้ำทุก loop
  if (wasConnected && !connectedBeforeLoop)
  {
    const int state = client.state();
    LOGW("[MQTT] Disconnected. state=%d (%s), WiFi=%s, IP=%s, RSSI=%d dBm, uptime=%lu ms",
         state, mqttStateName(state),
         WiFi.status() == WL_CONNECTED ? "connected" : "disconnected",
         WiFi.localIP().toString().c_str(), WiFi.RSSI(),
         static_cast<unsigned long>(millis()));
  }

  if (!connectedBeforeLoop)
  {
    reconnect();
  }

  client.loop();
  wasConnected = client.connected();

  unsigned long now = millis();
  if (now - lastMsg > 3000)
  {
    lastMsg = now;
    publish_all_states();
  }
}

void send_switch_config(int relay_num)
{
  String mac = getMacClean();
  String object_id = "relay_" + String(relay_num);
  String config_topic = String(discovery_prefix) + "/switch/" + DEVICE_NAME + "/" + object_id + "/config";
  String command_topic = String(discovery_prefix) + "/switch/" + DEVICE_NAME + "/" + object_id + "/set";

  JsonDocument doc;
  
  int index = relay_num - 1;
  if (index >= 0 && index < 6 && !friendlyNames[index].isEmpty()) {
    doc["name"] = friendlyNames[index];
  } else {
    doc["name"] = "Relay Channel " + String(relay_num);
  }

  doc["state_topic"] = state_topic;
  doc["command_topic"] = command_topic;

  String value_template = String("{{ value_json.") + object_id + " }}";
  doc["value_template"] = value_template.c_str();

  // ใช้ MAC + Object ID เพื่อป้องกัน Unique ID ใน HA ซ้ำกัน
  String unique_id = mac + "_" + object_id;
  doc["unique_id"] = unique_id.c_str();

  doc["payload_on"] = "ON";
  doc["payload_off"] = "OFF";
  doc["icon"] = "mdi:relay";

  doc["availability_topic"] = availability_topic;
  doc["payload_available"] = "online";
  doc["payload_not_available"] = "offline";

  JsonObject dev = doc["device"].to<JsonObject>();
  JsonArray ids = dev["identifiers"].to<JsonArray>();
  ids.add(mac); // ใช้ MAC Address เป็น Device Identifier หลัก
  dev["name"] = (DEVICE_NAME && strlen(DEVICE_NAME) > 0) ? DEVICE_NAME : "ESP32-S3 6CH Relay WAVESHARE";
  dev["sw_version"] = D_SWVersion;
  dev["manufacturer"] = D_Mfac;
  dev["model"] = D_Model;

  char buffer[1024];
  serializeJson(doc, buffer, sizeof(buffer));
  client.publish(config_topic.c_str(), buffer, true);
  
  client.subscribe(command_topic.c_str());
  client.loop();
  vTaskDelay(pdMS_TO_TICKS(10));
}

void send_sensor_config(const char *object_id, const char *name, const char *unit, const char *device_class, const char *icon, const char *category)
{
  String mac = getMacClean();
  String config_topic = String(discovery_prefix) + "/sensor/" + DEVICE_NAME + "/" + object_id + "/config";

  JsonDocument doc;
  doc["name"] = name;
  doc["state_topic"] = state_topic;

  String value_template = String("{{ value_json.") + object_id + " }}";
  doc["value_template"] = value_template.c_str();

  // แก้ไขจุดบั๊กเดิม: เพิ่ม mac นำหน้า unique_id
  String unique_id = mac + "_" + object_id;
  doc["unique_id"] = unique_id.c_str();

  if (unit && strlen(unit) > 0) doc["unit_of_measurement"] = unit;
  if (device_class && strlen(device_class) > 0) doc["device_class"] = device_class;
  if (icon && strlen(icon) > 0) doc["icon"] = icon;
  if (category && strlen(category) > 0) doc["entity_category"] = category;

  doc["availability_topic"] = availability_topic;
  doc["payload_available"] = "online";
  doc["payload_not_available"] = "offline";

  JsonObject dev = doc["device"].to<JsonObject>();
  JsonArray ids = dev["identifiers"].to<JsonArray>();
  ids.add(mac); // ใช้ MAC Address เป็น Device Identifier
  dev["name"] = (DEVICE_NAME && strlen(DEVICE_NAME) > 0) ? DEVICE_NAME : "ESP32-S3 6CH Relay WAVESHARE";
  dev["sw_version"] = D_SWVersion;
  dev["manufacturer"] = D_Mfac;
  dev["model"] = D_Model;

  char buffer[1024];
  serializeJson(doc, buffer, sizeof(buffer));
  client.publish(config_topic.c_str(), buffer, true);
  client.loop();
  vTaskDelay(pdMS_TO_TICKS(10));
}

void send_ha_discovery()
{
  LOGI("Sending HA Discovery Configurations...");
  for (int i = 1; i <= 6; i++)
  {
    send_switch_config(i);
  }

  send_sensor_config("ip_address", "IP Address", "", "", "mdi:ip-network", "diagnostic");
  send_sensor_config("mac_address", "MAC Address", "", "", "mdi:lan-connect", "diagnostic");
  send_sensor_config("uptime", "Uptime", "", "timestamp", "mdi:clock", "diagnostic");
  send_sensor_config("rssi", "WiFi Signal", "dBm", "signal_strength", "mdi:wifi", "diagnostic");

  LOGI("All HA Discovery Configs sent!");
}

void publish_all_states()
{
  JsonDocument doc;

  doc["ip_address"] = WiFi.localIP().toString();
  doc["mac_address"] = WiFi.macAddress();
  doc["rssi"] = WiFi.RSSI();

  time_t nowSec;
  time(&nowSec);

  if (nowSec > 1577836800)
  {
    time_t bootTime = nowSec - (millis() / 1000);
    char bootTimeStr[25];
    struct tm *timeinfo = gmtime(&bootTime);
    strftime(bootTimeStr, sizeof(bootTimeStr), "%Y-%m-%dT%H:%M:%SZ", timeinfo);

    doc["uptime"] = bootTimeStr;
  }

  for (int i = 0; i < 6; i++)
  {
    String key = "relay_" + String(i + 1);
    doc[key] = relay_states[i] ? "ON" : "OFF";
  }

  char buffer[1024];
  serializeJson(doc, buffer, sizeof(buffer));

  client.publish(state_topic, buffer, true);
  client.publish(availability_topic, "online", true);
  client.loop();
}

void mqtt_callback(char *topic, byte *payload, unsigned int length)
{
  String message;
  for (unsigned int i = 0; i < length; i++)
  {
    message += (char)payload[i];
  }

  String topicStr = String(topic);
  LOGI("MQTT Message arrived [%s]: %s\n", topic, message.c_str());

  for (int i = 1; i <= 6; i++)
  {
    String expectedTopic = String(discovery_prefix) + "/switch/" + DEVICE_NAME + "/relay_" + String(i) + "/set";
    if (topicStr == expectedTopic)
    {
      if (message == "ON")
      {
        relay_states[i - 1] = true;
      }
      else if (message == "OFF")
      {
        relay_states[i - 1] = false;
      }
      
      publish_all_states();
      break;
    }
  }
}

void reconnect()
{
  static unsigned long lastReconnectAttempt = 0;
  unsigned long now = millis();

  if (!client.connected())
  {
    if (now - lastReconnectAttempt > 5000)
    {
      lastReconnectAttempt = now;

      // ใช้ Device Name ร่วมกับ MAC เพื่อให้ Client ID ไม่ซ้ำกันในแต่ละอุปกรณ์
      const char *deviceName = (DEVICE_NAME[0] != '\0') ? DEVICE_NAME : "ESP32_Relay";
      String client_id = String(deviceName) + "_" + getMacClean();
      LOGI("[MQTT] Connect attempt: server=%s:%u, client_id=%s, mac=%s, ip=%s, WiFi=%s, RSSI=%d dBm, heap=%u",
           MQTT_SERVER, MQTT_PORT, client_id.c_str(), WiFi.macAddress().c_str(),
           WiFi.localIP().toString().c_str(),
           WiFi.status() == WL_CONNECTED ? "connected" : "disconnected",
           WiFi.RSSI(), ESP.getFreeHeap());

      if (client.connect(client_id.c_str(), MQTT_USER, MQTT_PASS, availability_topic, 0, true, "offline"))
      {
        LOGI("[MQTT] Connected: client_id=%s, broker=%s:%u, ip=%s",
             client_id.c_str(), MQTT_SERVER, MQTT_PORT, WiFi.localIP().toString().c_str());
        send_ha_discovery();
        client.publish(availability_topic, "online", true);
        publish_all_states();
      }
      else
      {
        const int state = client.state();
        LOGW("[MQTT] Connect failed: state=%d (%s), WiFi=%s, IP=%s, RSSI=%d dBm, heap=%u",
             state, mqttStateName(state),
             WiFi.status() == WL_CONNECTED ? "connected" : "disconnected",
             WiFi.localIP().toString().c_str(), WiFi.RSSI(), ESP.getFreeHeap());
      }
    }
  }
}