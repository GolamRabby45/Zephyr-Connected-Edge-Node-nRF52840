#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/socket.h>
#include <zephyr/net/websocket.h>

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "app_config.h"
#include "app_types.h"
#include "control_plane.h"
#include "net_client.h"
#include "rtos_channels.h"
#include "ws_client.h"

LOG_MODULE_REGISTER(ws_client, LOG_LEVEL_INF);

#define EDGE_WS_STACK_SIZE 4096
#define EDGE_WS_PRIORITY   7

K_THREAD_STACK_DEFINE(g_ws_stack, EDGE_WS_STACK_SIZE);
static struct k_thread g_ws_thread;
static struct edge_ws_handle g_ws_handle;

static int send_tcp_fallback(const struct edge_runtime_config *cfg, const uint8_t *payload, uint32_t len)
{
    int sock = -1;
    int rc = net_client_tcp_connect(cfg->telemetry_host, cfg->telemetry_port, &sock);
    if (rc < 0) {
        return rc;
    }

    rc = net_client_send_all(sock, payload, (int)len);
    if (rc == 0) {
        static const uint8_t newline = '\n';
        (void)net_client_send_all(sock, &newline, 1);
    }

    net_client_close(&sock);
    return rc;
}

int ws_client_connect(struct edge_ws_handle *handle, const char *host, uint16_t port, const char *url)
{
    struct websocket_request req = {0};
    int rc;

    if (handle == NULL || host == NULL || url == NULL) {
        return -EINVAL;
    }

    memset(handle, 0, sizeof(*handle));
    handle->tcp_sock = -1;
    handle->ws_sock = -1;

    rc = net_client_tcp_connect(host, port, &handle->tcp_sock);
    if (rc < 0) {
        return rc;
    }

    req.host = host;
    req.url = url;
    req.tmp_buf = handle->tmp_buf;
    req.tmp_buf_len = sizeof(handle->tmp_buf);

    handle->ws_sock = websocket_connect(handle->tcp_sock, &req, 4000, NULL);
    if (handle->ws_sock < 0) {
        net_client_close(&handle->tcp_sock);
        return handle->ws_sock;
    }

    handle->connected = true;
    return 0;
}

int ws_client_send_text(struct edge_ws_handle *handle, const uint8_t *payload, uint32_t payload_len)
{
    int rc;

    if (handle == NULL || !handle->connected || payload == NULL || payload_len == 0U) {
        return -EINVAL;
    }

    rc = websocket_send_msg(handle->ws_sock,
                            payload,
                            payload_len,
                            WEBSOCKET_OPCODE_DATA_TEXT,
                            true,
                            true,
                            2000);

    if (rc < 0) {
        return rc;
    }

    return 0;
}

void ws_client_disconnect(struct edge_ws_handle *handle)
{
    if (handle == NULL) {
        return;
    }

    if (handle->connected) {
        (void)websocket_disconnect(handle->ws_sock);
    }

    if (handle->ws_sock >= 0) {
        (void)zsock_close(handle->ws_sock);
    }

    net_client_close(&handle->tcp_sock);

    handle->ws_sock = -1;
    handle->connected = false;
}

static void ws_thread_fn(void *arg1, void *arg2, void *arg3)
{
    ARG_UNUSED(arg1);
    ARG_UNUSED(arg2);
    ARG_UNUSED(arg3);

    int wdt_channel = control_plane_watchdog_register("telemetry", 5000U);
    struct edge_runtime_config cfg;
    struct edge_processed_sample sample;

    memset(&g_ws_handle, 0, sizeof(g_ws_handle));
    g_ws_handle.tcp_sock = -1;
    g_ws_handle.ws_sock = -1;

    while (true) {
        if (k_msgq_get(&edge_processed_sample_msgq, &sample, K_FOREVER) != 0) {
            continue;
        }

        app_config_get(&cfg);

        char payload[220];
        int n = snprintf(payload,
                         sizeof(payload),
                         "{\"node\":\"%s\",\"seq\":%u,\"ts_ms\":%lld,\"i2c\":%d,\"spi\":%d,\"fused\":%ld,\"q\":%u,\"flags\":%u}",
                         cfg.node_id,
                         sample.sequence,
                         (long long)sample.ts_ms,
                         sample.i2c_raw,
                         sample.spi_raw,
                         (long)sample.fused_value,
                         sample.quality,
                         sample.flags);

        if (n <= 0) {
            continue;
        }

        k_mutex_lock(&edge_network_mutex, K_FOREVER);

        int rc = -1;
        if (cfg.websocket_enabled) {
            if (!g_ws_handle.connected) {
                rc = ws_client_connect(&g_ws_handle,
                                       cfg.telemetry_host,
                                       cfg.telemetry_port,
                                       cfg.ws_url);
                if (rc == 0) {
                    LOG_INF("WebSocket connected to %s:%u%s",
                            cfg.telemetry_host,
                            cfg.telemetry_port,
                            cfg.ws_url);
                }
            } else {
                rc = 0;
            }

            if (rc == 0) {
                rc = ws_client_send_text(&g_ws_handle, (const uint8_t *)payload, (uint32_t)n);
            }

            if (rc < 0) {
                LOG_WRN("WebSocket send failed (%d), switching to TCP fallback", rc);
                ws_client_disconnect(&g_ws_handle);
                rc = send_tcp_fallback(&cfg, (const uint8_t *)payload, (uint32_t)n);
            }
        } else {
            ws_client_disconnect(&g_ws_handle);
            rc = send_tcp_fallback(&cfg, (const uint8_t *)payload, (uint32_t)n);
        }

        k_mutex_unlock(&edge_network_mutex);

        if (rc < 0) {
            LOG_DBG("telemetry delivery failed rc=%d", rc);
        }

        control_plane_watchdog_feed(wdt_channel);
    }
}

int ws_telemetry_start(void)
{
    (void)k_thread_create(&g_ws_thread,
                          g_ws_stack,
                          K_THREAD_STACK_SIZEOF(g_ws_stack),
                          ws_thread_fn,
                          NULL,
                          NULL,
                          NULL,
                          EDGE_WS_PRIORITY,
                          0,
                          K_NO_WAIT);

    k_thread_name_set(&g_ws_thread, "ws_telemetry");
    return 0;
}
