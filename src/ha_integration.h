// ha_integration.h
#ifndef HA_INTEGRATION_H
#define HA_INTEGRATION_H

#pragma once
#include "config.h"

extern const char *discovery_prefix;
extern char availability_topic[128];
extern char state_topic[128];

void init_ha_topics();
void iotHAsetup();
void iotHAloop();
void send_ha_discovery();
void publish_all_states();
void reconnect();

#endif