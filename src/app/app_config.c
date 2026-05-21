#include <zephyr/kernel.h>

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "app_config.h"

static struct edge_runtime_config g_cfg;
static struct k_mutex g_cfg_lock;

static void copy_bounded(char *dst, size_t dst_len, const char *src)
{
    if (dst_len == 0U) {
        return;
    }

    if (src == NULL) {
        dst[0] = '\0';
        return;
    }

    strncpy(dst, src, dst_len - 1U);
    dst[dst_len - 1U] = '\0';
}

void app_config_init(void)
{
    k_mutex_init(&g_cfg_lock);

    copy_bounded(g_cfg.node_id, sizeof(g_cfg.node_id), CONFIG_EDGE_NODE_ID);
    copy_bounded(g_cfg.telemetry_host, sizeof(g_cfg.telemetry_host), CONFIG_EDGE_TELEMETRY_HOST);
    copy_bounded(g_cfg.ws_url, sizeof(g_cfg.ws_url), CONFIG_EDGE_WS_URL);
    copy_bounded(g_cfg.rest_url, sizeof(g_cfg.rest_url), CONFIG_EDGE_REST_URL);

    g_cfg.telemetry_port = (uint16_t)CONFIG_EDGE_TELEMETRY_PORT;
    g_cfg.rest_port = (uint16_t)CONFIG_EDGE_REST_PORT;
    g_cfg.sample_period_ms = (uint32_t)CONFIG_EDGE_SAMPLE_PERIOD_MS;
    g_cfg.rest_poll_ms = (uint32_t)CONFIG_EDGE_REST_POLL_MS;
    g_cfg.websocket_enabled = true;
}

void app_config_get(struct edge_runtime_config *out)
{
    if (out == NULL) {
        return;
    }

    k_mutex_lock(&g_cfg_lock, K_FOREVER);
    *out = g_cfg;
    k_mutex_unlock(&g_cfg_lock);
}

uint32_t app_config_get_sample_period_ms(void)
{
    uint32_t period;

    k_mutex_lock(&g_cfg_lock, K_FOREVER);
    period = g_cfg.sample_period_ms;
    k_mutex_unlock(&g_cfg_lock);

    return period;
}

void app_config_set_sample_period_ms(uint32_t period_ms)
{
    if (period_ms < 10U) {
        period_ms = 10U;
    }

    k_mutex_lock(&g_cfg_lock, K_FOREVER);
    g_cfg.sample_period_ms = period_ms;
    k_mutex_unlock(&g_cfg_lock);
}

void app_config_set_websocket_enabled(bool enabled)
{
    k_mutex_lock(&g_cfg_lock, K_FOREVER);
    g_cfg.websocket_enabled = enabled;
    k_mutex_unlock(&g_cfg_lock);
}
