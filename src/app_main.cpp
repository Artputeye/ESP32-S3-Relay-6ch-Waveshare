// app_main.cpp
#include "app_main.h"

void app_setup()
{
    relay_setup(); // ตั้งค่า GPIO ของ Relay
}

void app_loop()
{
    relay_loop(); // ทำงานบน Core 0 อย่างต่อเนื่อง
}

void updateSystemStatus()
{
}