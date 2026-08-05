#ifndef RELAY_H
#define RELAY_H

#include "config.h"

void relay_setup();
void relay_loop();
void set_relay_state(int index, bool state);

#endif // RELAY_H