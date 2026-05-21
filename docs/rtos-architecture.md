# RTOS Architecture Notes

## Thread priorities

- `sampler` (prio 2): deterministic acquisition trigger consumer
- `pipe_workq` (prio 3): compute offload worker thread
- `pipeline` (prio 4): queue bridge and work scheduling
- `ctrl_plane` (prio 5): command and IRQ event coordination
- `ws_telemetry` (prio 7): telemetry streaming
- `rest_poll` (prio 8): gateway command polling

Lower priority numbers represent higher scheduling priority in Zephyr.

## Determinism strategy

- Acquisition is driven by `k_timer` and `k_sem`.
- Network transport runs in separate threads.
- Pipeline compute runs in custom workqueue.
- Shared network resources protected with `k_mutex`.

## Reliability strategy

- Task watchdog channels for sampler/pipeline/control/connectivity.
- Non-blocking queue puts on hot paths to avoid priority inversion.
- Structured logging around queue drops, reconnects, and command changes.
