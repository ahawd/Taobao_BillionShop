#include "state_machine.h"

#include <stdio.h>

void transport_send_hb(gateway_t *from, gateway_t *to, const redundancy_config_t *cfg, const char *name) {
    if (from->role != ROLE_MASTER) return;
    if ((from->now_ms - from->last_tx_hb_ms) < cfg->hb_period_ms) return;

    from->last_tx_hb_ms = from->now_ms;
    heartbeat_t hb = gw_build_heartbeat(from);

    if (from->mvb_link_up && to->mvb_link_up) gw_receive_heartbeat(to, &hb, LINK_MVB);
    if (from->eth_link_up && to->eth_link_up) gw_receive_heartbeat(to, &hb, LINK_ETH);

    printf("[%6ums] %-5s | tx hb seq=%u epoch=%u\n", from->now_ms, name, hb.seq, hb.epoch);
}
