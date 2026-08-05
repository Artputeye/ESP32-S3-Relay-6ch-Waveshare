#include "relay.h"

// ตัวแปรสำหรับเช็คสถานะเดิม เพื่ออัปเดตพินเฉพาะตอนที่มีการเปลี่ยนแปลง
static bool last_relay_states[6] = {false, false, false, false, false, false};

void relay_setup()
{
    for (int i = 0; i < 6; i++)
    {
        pinMode(RELAY_PINS[i], OUTPUT);
        digitalWrite(RELAY_PINS[i], relay_states[i] ? HIGH : LOW);
        last_relay_states[i] = relay_states[i];
    }
}

// ฟังก์ชันนี้จะรันใน app_loop() บน TaskMain (Core 0)
void relay_loop()
{
    for (int i = 0; i < 6; i++)
    {
        // ตรวจสอบว่ามีการสั่งเปลี่ยนสถานะจาก Core อื่นหรือไม่
        if (relay_states[i] != last_relay_states[i])
        {
            last_relay_states[i] = relay_states[i];
            digitalWrite(RELAY_PINS[i], relay_states[i] ? HIGH : LOW);
            LOGI("[Relay] Channel %d -> %s\n", i + 1, relay_states[i] ? "ON" : "OFF");
        }
    }
}

// ฟังก์ชัน helper สำหรับเปลี่ยนสถานะจากส่วนอื่น
void set_relay_state(int index, bool state)
{
    if (index >= 0 && index < 6)
    {
        relay_states[index] = state;
    }
}