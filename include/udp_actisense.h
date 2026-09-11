#pragma once

#include <stdbool.h>

void udp_actisense_load_config(void);
bool udp_actisense_begin(void);
void udp_actisense_process(void);
bool udp_actisense_connected(void);