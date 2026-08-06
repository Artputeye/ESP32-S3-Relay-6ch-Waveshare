#include "ha_integration.h"

const char *discovery_prefix = "homeassistant";
char availability_topic[128] = "";
char state_topic[128] = "";

unsigned long lastMsg = 0;

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
  // โหลดค่า setting.json ก่อนเริ่มตั้งค่า MQTT
  loadAllSettings();
  init_ha_topics();

  client.setServer(MQTT_SERVER, MQTT_PORT);
  client.setCallback(mqtt_callback);
  client.setBufferSize(4096);
  client.setKeepAlive(60);
  client.setSocketTimeout(60);
}

void iotHAloop()
{
  if (!client.connected())
  {
    reconnect();
  }
  client.loop();

  unsigned long now = millis();
  if (now - lastMsg > 3000)
  {
    lastMsg = now;
    publish_all_states();
  }
}

void send_switch_config(int relay_num)
{
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

  String unique_id = String(DEVICE_NAME) + "_" + object_id;
  doc["unique_id"] = unique_id.c_str();

  doc["payload_on"] = "ON";
  doc["payload_off"] = "OFF";
  doc["icon"] = "mdi:relay";

  doc["availability_topic"] = availability_topic;
  doc["payload_available"] = "online";
  doc["payload_not_available"] = "offline";

  JsonObject dev = doc["device"].to<JsonObject>();
  JsonArray ids = dev["identifiers"].to<JsonArray>();
  ids.add(DEVICE_NAME);
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
  String config_topic = String(discovery_prefix) + "/sensor/" + DEVICE_NAME + "/" + object_id + "/config";

  JsonDocument doc;
  doc["name"] = name;
  doc["state_topic"] = state_topic;

  String value_template = String("{{ value_json.") + object_id + " }}";
  doc["value_template"] = value_template.c_str();

  String unique_id = String() + "_" + object_id;
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
  ids.add(DEVICE_NAME);
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

      if (client.connect("ESP32_S3_Relay_6CH", MQTT_USER, MQTT_PASS, availability_topic, 0, true, "offline"))
      {
        LOGI("connected");
        send_ha_discovery();
        client.publish(availability_topic, "online", true);
        publish_all_states();
      }
    }
  }
}