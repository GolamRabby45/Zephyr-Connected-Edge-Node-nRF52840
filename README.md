# Zephyr Real-Time Edge Telemetry Node (nRF52840)

Real-time firmware platform for `nrf52840dk/nrf52840` using Zephyr RTOS.

## What it does

- Samples sensor data over `I2C` and `SPI` on deterministic timer intervals.
- Uses `k_thread`, `k_timer`, `k_msgq`, `k_sem`, `k_mutex`, and custom `k_work_q` to separate timing-critical work from network transport.
- Streams processed telemetry over `WebSocket` (with TCP fallback) to a gateway.
- Polls a REST endpoint for runtime control commands (e.g., sample period changes).
- Uses ISR-to-thread handoff from button interrupt (`sw0`) into the data pipeline.
- Feeds task watchdog channels for key runtime threads.

## RTOS architecture

- Driver layer: `sensor_bus.c`, `irq_button.c`
- Pipeline layer: `sampler.c`, `processor.c`, `pipeline_math.c`
- Connectivity layer: `net_client.c`, `ws_client.c`, `rest_gateway.c`
- Control layer: `control_plane.c`

## Build and run

### 1) Build firmware

```bash
west build -b nrf52840dk/nrf52840 -p auto .
```

### 2) Flash target

```bash
west flash
```

### 3) Serial logs

```bash
west build -t run
# or use your preferred RTT/UART terminal
```

### 4) Debug with GDB

```bash
west debug
```

## Runtime command format (REST response body)

The REST poller expects plain text command bodies:

- `sample_period_ms=200`
- `reconnect`
- `flush`

## Unit tests (Twister)

```bash
west twister -T tests/unit -p native_sim
```

## Local helper scripts

- `scripts/build.sh`
- `scripts/flash.sh`
- `scripts/debug.sh`
- `scripts/test.sh`
- `scripts/ci-local.sh`

## Outcomes demonstrated by this codebase

- Deterministic sampling remains isolated from network jitter.
- Reusable RTOS architecture with clean layer boundaries.
- Faster debugging via structured logs, ISR event tracing, and watchdog channels.
- Reproducible engineering flow via scripts, Twister tests, Docker scaffold, and CI config.
