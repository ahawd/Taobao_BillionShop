# Dual-Gateway Redundancy C Demo

## Build & Run

```bash
make
make run
```

## Architecture

- `main.c`: simulation driver, fault injection, recovery scenario.
- `state_machine.c/.h`: gateway redundancy finite-state machine and takeover logic.
- `transport_sim.c/.h`: simulated MVB/ETH heartbeat transport.
- `config.h`: configurable timing parameters.

## Behavior shown

1. Startup discovery and election.
2. Single-master heartbeat publishing.
3. Standby takeover after heartbeat/refresh timeout.
4. Non-revertive behavior after original master recovery.
