// serial_handler.cpp
#include "serial_handler.h"

void processSerialCommand(String input)
{
    input.trim();
    lastSerialMsg = input;
    String upperInput = input;
    upperInput.toUpperCase();

    if (upperInput.startsWith("SET:"))
    {
        String payload = upperInput.substring(4); // ตัด "SET:" ออก เหลือ "RELAY1:ON"
        int firstColon = payload.indexOf(':');

        if (firstColon != -1)
        {
            String key = payload.substring(0, firstColon);  // "RELAY1"
            String val = payload.substring(firstColon + 1); // "ON"

            // serialRelay(key, val);

            // Debug ดูค่าที่แยกได้
            LOGI("Command Key: ");
            LOGI("%s", key.c_str());
            LOGI("Command Val: ");
            LOGI("%s", val.c_str());
        }
    }
    else if (input.length() > 0)
    {
        LOGI(">>> [INV] Command format to inverter.");
    }
}
