#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <limits.h>

#include "app_config.h"
#include "control_plane.h"
#include "irq_button.h"
#include "processor.h"
#include "rest_gateway.h"
#include "rtos_channels.h"
#include "sampler.h"
#include "sensor_bus.h"
#include "ws_client.h"

LOG_MODULE_REGISTER(edge_main, LOG_LEVEL_INF);

K_MSGQ_DEFINE(edge_raw_sample_msgq,
              sizeof(struct edge_raw_sample),
              CONFIG_EDGE_PIPELINE_QUEUE_DEPTH,
              4);

K_MSGQ_DEFINE(edge_processed_sample_msgq,
              sizeof(struct edge_processed_sample),
              CONFIG_EDGE_PIPELINE_QUEUE_DEPTH,
              4);

K_MSGQ_DEFINE(edge_control_cmd_msgq,
              sizeof(struct edge_control_cmd),
              16,
              4);

K_MSGQ_DEFINE(edge_irq_event_msgq,
              sizeof(struct edge_irq_event),
              16,
              4);

K_SEM_DEFINE(edge_sample_trigger_sem, 0, UINT_MAX);
K_SEM_DEFINE(edge_work_done_sem, 0, UINT_MAX);
K_MUTEX_DEFINE(edge_network_mutex);

int main(void)
{
    int rc;

    LOG_INF("Zephyr Real-Time Edge Telemetry Node booting");

    app_config_init();

    rc = sensor_bus_init();
    if (rc < 0) {
        LOG_ERR("sensor_bus_init failed (%d)", rc);
    }

    rc = irq_button_init();
    if (rc < 0) {
        LOG_WRN("irq_button_init failed (%d), continuing", rc);
    }

    rc = control_plane_start();
    if (rc < 0) {
        LOG_ERR("control_plane_start failed (%d)", rc);
        return rc;
    }

    rc = processor_start();
    if (rc < 0) {
        LOG_ERR("processor_start failed (%d)", rc);
        return rc;
    }

    rc = sampler_start();
    if (rc < 0) {
        LOG_ERR("sampler_start failed (%d)", rc);
        return rc;
    }

    rc = ws_telemetry_start();
    if (rc < 0) {
        LOG_ERR("ws_telemetry_start failed (%d)", rc);
        return rc;
    }

    rc = rest_gateway_start();
    if (rc < 0) {
        LOG_ERR("rest_gateway_start failed (%d)", rc);
        return rc;
    }

    while (true) {
        struct edge_runtime_config cfg;
        app_config_get(&cfg);

        LOG_INF("alive: sample_period=%ums rest_poll=%ums ws=%s host=%s:%u rest_port=%u",
                cfg.sample_period_ms,
                cfg.rest_poll_ms,
                cfg.websocket_enabled ? "on" : "off",
                cfg.telemetry_host,
                cfg.telemetry_port,
                cfg.rest_port);

        k_sleep(K_SECONDS(10));
    }

    return 0;
}
