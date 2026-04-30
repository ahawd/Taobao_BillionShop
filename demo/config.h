#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>

typedef struct {
    uint32_t hb_period_ms;
    uint32_t hb_timeout_mvb_ms;
    uint32_t hb_timeout_eth_ms;
    uint32_t refresh_period_ms;
    uint32_t refresh_lost_multiplier;
    uint32_t takeover_debounce_ms;
    uint32_t tick_period_ms;
} redundancy_config_t;

#endif
