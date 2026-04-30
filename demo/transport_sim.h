#ifndef TRANSPORT_SIM_H
#define TRANSPORT_SIM_H

#include "state_machine.h"

void transport_send_hb(gateway_t *from, gateway_t *to, const redundancy_config_t *cfg, const char *name);

#endif
