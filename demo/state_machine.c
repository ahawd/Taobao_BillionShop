#include "state_machine.h"

#include <stdio.h>

static void log_event(uint32_t t, const char *name, const char *msg) {
    printf("[%6ums] %-5s | %s\n", t, name, msg);
}

const char *gw_role_str(role_t role) { return role == ROLE_MASTER ? "MASTER" : "STANDBY"; }

const char *gw_state_str(state_t state) {
    switch (state) {
    case ST_INIT: return "INIT";
    case ST_DISCOVERY: return "DISCOVERY";
    case ST_ELECTION: return "ELECTION";
    case ST_MASTER: return "MASTER";
    case ST_STANDBY: return "STANDBY";
    default: return "UNKNOWN";
    }
}

void gw_init(gateway_t *gw, uint32_t device_id, uint32_t priority) {
    *gw = (gateway_t){0};
    gw->device_id = device_id;
    gw->priority = priority;
    gw->state = ST_INIT;
    gw->role = ROLE_STANDBY;
    gw->mvb_link_up = true;
    gw->eth_link_up = true;
}

bool gw_win_election(const gateway_t *self, const gateway_t *peer) {
    if (self->priority != peer->priority) return self->priority > peer->priority;
    return self->device_id > peer->device_id;
}

heartbeat_t gw_build_heartbeat(gateway_t *self) {
    self->hb_seq++;
    return (heartbeat_t){
        .seq = self->hb_seq,
        .epoch = self->epoch,
        .sender_id = self->device_id,
        .sender_priority = self->priority,
        .role = self->role
    };
}

void gw_receive_heartbeat(gateway_t *self, const heartbeat_t *hb, link_t link) {
    if (hb->role == ROLE_MASTER) self->seen_master_announce = true;

    if (link == LINK_MVB) self->last_rx_hb_mvb_ms = self->now_ms;
    if (link == LINK_ETH) self->last_rx_hb_eth_ms = self->now_ms;
    self->last_rx_refresh_ms = self->now_ms;
    self->hb_seen_in_standby = true;

    if (hb->epoch > self->epoch) self->epoch = hb->epoch;
    if (self->role == ROLE_MASTER && hb->role == ROLE_MASTER) {
        bool peer_wins = false;
        if (hb->epoch > self->epoch) peer_wins = true;
        else if (hb->epoch == self->epoch) {
            if (hb->sender_priority != self->priority) peer_wins = hb->sender_priority > self->priority;
            else peer_wins = hb->sender_id > self->device_id;
        }
        if (peer_wins) {
            self->role = ROLE_STANDBY;
            self->state = ST_STANDBY;
        }
    }
}

static bool timeout(uint32_t now, uint32_t last, uint32_t limit) { return (now - last) >= limit; }

bool gw_should_takeover(const gateway_t *self, const redundancy_config_t *cfg) {
    if (!self->hb_seen_in_standby) return false;
    bool mvb_timeout = timeout(self->now_ms, self->last_rx_hb_mvb_ms, cfg->hb_timeout_mvb_ms);
    bool eth_timeout = timeout(self->now_ms, self->last_rx_hb_eth_ms, cfg->hb_timeout_eth_ms);
    bool refresh_timeout = timeout(self->now_ms, self->last_rx_refresh_ms,
        cfg->refresh_lost_multiplier * cfg->refresh_period_ms);
    return (mvb_timeout && eth_timeout) || refresh_timeout;
}

void gw_tick(gateway_t *self, const gateway_t *peer, const redundancy_config_t *cfg, const char *name) {
    (void)cfg;
    switch (self->state) {
    case ST_INIT:
        self->state = ST_DISCOVERY;
        log_event(self->now_ms, name, "INIT -> DISCOVERY");
        break;
    case ST_DISCOVERY:
        self->mvb_peer_found = peer->mvb_link_up;
        self->eth_peer_found = peer->eth_link_up;
        if (self->mvb_peer_found && self->eth_peer_found) {
            self->state = ST_ELECTION;
            log_event(self->now_ms, name, "DISCOVERY -> ELECTION");
        }
        break;
    case ST_ELECTION:
        if (self->seen_master_announce) {
            self->role = ROLE_STANDBY;
            self->state = ST_STANDBY;
            self->hb_seen_in_standby = false;
            log_event(self->now_ms, name, "ELECTION -> STANDBY (master exists)");
        } else if (gw_win_election(self, peer)) {
            self->epoch = (self->epoch > peer->epoch ? self->epoch : peer->epoch) + 1;
            self->role = ROLE_MASTER;
            self->state = ST_MASTER;
            log_event(self->now_ms, name, "ELECTION -> MASTER");
        } else {
            self->role = ROLE_STANDBY;
            self->state = ST_STANDBY;
            self->hb_seen_in_standby = false;
            log_event(self->now_ms, name, "ELECTION -> STANDBY");
        }
        break;
    case ST_MASTER:
        if (self->critical_fault) {
            self->mvb_link_up = false;
            self->eth_link_up = false;
            self->role = ROLE_STANDBY;
            self->state = ST_DISCOVERY;
            log_event(self->now_ms, name, "MASTER fault -> DISCOVERY");
        }
        break;
    case ST_STANDBY:
        if (gw_should_takeover(self, cfg)) {
            self->epoch = (self->epoch > peer->epoch ? self->epoch : peer->epoch) + 1;
            self->role = ROLE_MASTER;
            self->state = ST_MASTER;
            self->seen_master_announce = false;
            log_event(self->now_ms, name, "STANDBY -> MASTER (TAKEOVER)");
        }
        break;
    }
}
