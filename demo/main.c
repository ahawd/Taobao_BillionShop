#include <stdio.h>

#include "config.h"
#include "state_machine.h"
#include "transport_sim.h"

int main(void) {
    redundancy_config_t cfg = {
        .hb_period_ms = 100,
        .hb_timeout_mvb_ms = 300,
        .hb_timeout_eth_ms = 300,
        .refresh_period_ms = 50,
        .refresh_lost_multiplier = 6,
        .takeover_debounce_ms = 50,
        .tick_period_ms = 10,
    };

    gateway_t a, b;
    gw_init(&a, 1001, 100); // A high priority, becomes initial master.
    gw_init(&b, 1002, 90);

    puts("[     0ms] SYS   | start simulation");

    for (uint32_t t = 0; t <= 6000; t += cfg.tick_period_ms) {
        a.now_ms = t;
        b.now_ms = t;

        if (t == 2500 && a.role == ROLE_MASTER) {
            a.critical_fault = true;
            puts("[  2500ms] A     | fault injected");
        }

        gw_tick(&a, &b, &cfg, "A");
        gw_tick(&b, &a, &cfg, "B");

        transport_send_hb(&a, &b, &cfg, "A");
        transport_send_hb(&b, &a, &cfg, "B");

        if (t == 4200) {
            a.critical_fault = false;
            a.mvb_link_up = true;
            a.eth_link_up = true;
            a.state = ST_DISCOVERY;
            a.role = ROLE_STANDBY;
            a.seen_master_announce = (b.role == ROLE_MASTER);
            puts("[  4200ms] A     | recovered; rejoin as STANDBY (non-revertive)");
        }
    }

    printf("\nFinal Roles: A=%s, B=%s\n", gw_role_str(a.role), gw_role_str(b.role));
    printf("Final States: A=%s, B=%s\n", gw_state_str(a.state), gw_state_str(b.state));
    return 0;
}
