#ifndef STATE_MACHINE_H
#define STATE_MACHINE_H

#include <stdbool.h>
#include <stdint.h>
#include "config.h"

typedef enum { LINK_MVB = 0, LINK_ETH = 1 } link_t;

typedef enum { ROLE_STANDBY = 0, ROLE_MASTER = 1 } role_t;

typedef enum {
    ST_INIT = 0,
    ST_DISCOVERY,
    ST_ELECTION,
    ST_MASTER,
    ST_STANDBY
} state_t;

typedef struct {
    uint32_t seq;
    uint32_t epoch;
    uint32_t sender_id;
    uint32_t sender_priority;
    role_t role;
} heartbeat_t;

typedef struct {
    uint32_t device_id;
    uint32_t priority;

    uint32_t now_ms;
    uint32_t epoch;
    uint32_t hb_seq;

    uint32_t last_rx_hb_mvb_ms;
    uint32_t last_rx_hb_eth_ms;
    uint32_t last_rx_refresh_ms;
    uint32_t last_tx_hb_ms;

    bool mvb_peer_found;
    bool eth_peer_found;
    bool seen_master_announce;
    bool hb_seen_in_standby;

    bool mvb_link_up;
    bool eth_link_up;
    bool critical_fault;

    role_t role;
    state_t state;
} gateway_t;

void gw_init(gateway_t *gw, uint32_t device_id, uint32_t priority);
void gw_tick(gateway_t *self, const gateway_t *peer, const redundancy_config_t *cfg, const char *name);

void gw_receive_heartbeat(gateway_t *self, const heartbeat_t *hb, link_t link);
heartbeat_t gw_build_heartbeat(gateway_t *self);

const char *gw_role_str(role_t role);
const char *gw_state_str(state_t state);
bool gw_should_takeover(const gateway_t *self, const redundancy_config_t *cfg);
bool gw_win_election(const gateway_t *self, const gateway_t *peer);

#endif
