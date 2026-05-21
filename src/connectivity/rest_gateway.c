#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/http/client.h>
#include <zephyr/net/socket.h>

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app_config.h"
#include "app_types.h"
#include "control_plane.h"
#include "net_client.h"
#include "rest_gateway.h"
#include "rtos_channels.h"

LOG_MODULE_REGISTER(rest_gateway, LOG_LEVEL_INF);

#define EDGE_REST_STACK_SIZE 3584
#define EDGE_REST_PRIORITY   8

struct rest_response_ctx {
    char body[256];
    size_t len;
};

K_THREAD_STACK_DEFINE(g_rest_stack, EDGE_REST_STACK_SIZE);
static struct k_thread g_rest_thread;

static int rest_response_cb(struct http_response *rsp, enum http_final_call final_data, void *user_data)
{
    struct rest_response_ctx *ctx = user_data;

    ARG_UNUSED(final_data);

    if (rsp == NULL || ctx == NULL) {
        return -EINVAL;
    }

    if (rsp->body_frag_start == NULL || rsp->body_frag_len == 0U) {
        return 0;
    }

    size_t available = sizeof(ctx->body) - 1U - ctx->len;
    size_t to_copy = rsp->body_frag_len;
    if (to_copy > available) {
        to_copy = available;
    }

    memcpy(&ctx->body[ctx->len], rsp->body_frag_start, to_copy);
    ctx->len += to_copy;
    ctx->body[ctx->len] = '\0';

    return 0;
}

static void trim_inplace(char *s)
{
    size_t len;

    if (s == NULL) {
        return;
    }

    while (*s != '\0' && isspace((unsigned char)*s)) {
        memmove(s, s + 1, strlen(s));
    }

    len = strlen(s);
    while (len > 0U && isspace((unsigned char)s[len - 1U])) {
        s[len - 1U] = '\0';
        len--;
    }
}

static int parse_command(const char *body, struct edge_control_cmd *out)
{
    if (body == NULL || out == NULL) {
        return -EINVAL;
    }

    if (strncmp(body, "sample_period_ms=", 17) == 0) {
        long value = strtol(body + 17, NULL, 10);
        if (value < 10L || value > 60000L) {
            return -EINVAL;
        }
        out->type = EDGE_CMD_SET_SAMPLE_PERIOD_MS;
        out->value_u32 = (uint32_t)value;
        return 0;
    }

    if (strcmp(body, "reconnect") == 0) {
        out->type = EDGE_CMD_FORCE_RECONNECT;
        out->value_u32 = 0U;
        return 0;
    }

    if (strcmp(body, "flush") == 0) {
        out->type = EDGE_CMD_FORCE_FLUSH;
        out->value_u32 = 0U;
        return 0;
    }

    return -ENOMSG;
}

int rest_gateway_poll_command(struct edge_control_cmd *cmd_out)
{
    static uint8_t recv_buf[512];
    static const char *headers[] = {
        "Connection: close\r\n",
        "Accept: text/plain\r\n",
        NULL,
    };

    struct edge_runtime_config cfg;
    struct rest_response_ctx ctx = {0};
    struct http_request req = {0};
    char port_str[8];
    int sock = -1;
    int rc;

    if (cmd_out == NULL) {
        return -EINVAL;
    }

    app_config_get(&cfg);

    rc = net_client_tcp_connect(cfg.telemetry_host, cfg.rest_port, &sock);
    if (rc < 0) {
        return rc;
    }

    (void)snprintf(port_str, sizeof(port_str), "%u", cfg.rest_port);

    req.method = HTTP_GET;
    req.url = cfg.rest_url;
    req.host = cfg.telemetry_host;
    req.port = port_str;
    req.protocol = "HTTP/1.1";
    req.header_fields = headers;
    req.response = rest_response_cb;
    req.recv_buf = recv_buf;
    req.recv_buf_len = sizeof(recv_buf);

    rc = http_client_req(sock, &req, 4000, &ctx);
    net_client_close(&sock);

    if (rc < 0) {
        return rc;
    }

    trim_inplace(ctx.body);
    if (ctx.len == 0U || ctx.body[0] == '\0') {
        return -ENOMSG;
    }

    return parse_command(ctx.body, cmd_out);
}

static void rest_thread_fn(void *arg1, void *arg2, void *arg3)
{
    ARG_UNUSED(arg1);
    ARG_UNUSED(arg2);
    ARG_UNUSED(arg3);

    int wdt_channel = control_plane_watchdog_register("rest", 8000U);

    while (true) {
        struct edge_runtime_config cfg;
        struct edge_control_cmd cmd;

        k_mutex_lock(&edge_network_mutex, K_FOREVER);
        int rc = rest_gateway_poll_command(&cmd);
        k_mutex_unlock(&edge_network_mutex);

        if (rc == 0) {
            (void)k_msgq_put(&edge_control_cmd_msgq, &cmd, K_NO_WAIT);
            LOG_INF("REST command queued (type=%d, value=%u)", cmd.type, cmd.value_u32);
        }

        app_config_get(&cfg);
        control_plane_watchdog_feed(wdt_channel);
        k_msleep(cfg.rest_poll_ms);
    }
}

int rest_gateway_start(void)
{
    (void)k_thread_create(&g_rest_thread,
                          g_rest_stack,
                          K_THREAD_STACK_SIZEOF(g_rest_stack),
                          rest_thread_fn,
                          NULL,
                          NULL,
                          NULL,
                          EDGE_REST_PRIORITY,
                          0,
                          K_NO_WAIT);

    k_thread_name_set(&g_rest_thread, "rest_poll");
    return 0;
}
