#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/task_wdt/task_wdt.h>

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "app_config.h"
#include "app_types.h"
#include "control_plane.h"
#include "rtos_channels.h"
#include "sampler.h"

LOG_MODULE_REGISTER(control_plane, LOG_LEVEL_INF);

#define EDGE_CONTROL_STACK_SIZE 2300
#define EDGE_CONTROL_PRIORITY   5
#define EDGE_MAX_WDT_OWNERS     8

struct wdt_owner {
    int channel;
    char owner[20];
};

static struct wdt_owner g_wdt_owners[EDGE_MAX_WDT_OWNERS];
static uint32_t g_wdt_owner_count;
static struct k_mutex g_wdt_lock;
static atomic_t g_irq_latch;

K_THREAD_STACK_DEFINE(g_control_stack, EDGE_CONTROL_STACK_SIZE);
static struct k_thread g_control_thread;

static void apply_control_command(const struct edge_control_cmd *cmd)
{
    if (cmd == NULL) {
        return;
    }

    switch (cmd->type) {
    case EDGE_CMD_SET_SAMPLE_PERIOD_MS:
        app_config_set_sample_period_ms(cmd->value_u32);
        sampler_update_period_ms(cmd->value_u32);
        LOG_INF("Updated sample period to %u ms", cmd->value_u32);
        break;
    case EDGE_CMD_FORCE_FLUSH:
        LOG_INF("Flush requested by gateway");
        break;
    case EDGE_CMD_FORCE_RECONNECT:
        app_config_set_websocket_enabled(false);
        k_msleep(50);
        app_config_set_websocket_enabled(true);
        LOG_INF("Reconnect requested by gateway");
        break;
    default:
        LOG_WRN("Unknown command type %d", cmd->type);
        break;
    }
}

static void control_thread_fn(void *arg1, void *arg2, void *arg3)
{
    ARG_UNUSED(arg1);
    ARG_UNUSED(arg2);
    ARG_UNUSED(arg3);

    int control_wdt = control_plane_watchdog_register("control", 6000U);
    struct edge_control_cmd cmd;
    struct edge_irq_event irq_evt;

    while (true) {
        while (k_msgq_get(&edge_irq_event_msgq, &irq_evt, K_NO_WAIT) == 0) {
            atomic_set(&g_irq_latch, 1);
            LOG_INF("IRQ event count=%u ts=%lld", irq_evt.count, (long long)irq_evt.ts_ms);
        }

        if (k_msgq_get(&edge_control_cmd_msgq, &cmd, K_MSEC(250)) == 0) {
            apply_control_command(&cmd);
        }

        control_plane_watchdog_feed(control_wdt);
    }
}

int control_plane_start(void)
{
    int rc;

    k_mutex_init(&g_wdt_lock);
    g_wdt_owner_count = 0U;
    atomic_set(&g_irq_latch, 0);

    rc = task_wdt_init(NULL);
    if (rc < 0 && rc != -EALREADY) {
        LOG_ERR("task_wdt_init failed: %d", rc);
        return rc;
    }

    (void)k_thread_create(&g_control_thread,
                          g_control_stack,
                          K_THREAD_STACK_SIZEOF(g_control_stack),
                          control_thread_fn,
                          NULL,
                          NULL,
                          NULL,
                          EDGE_CONTROL_PRIORITY,
                          0,
                          K_NO_WAIT);

    k_thread_name_set(&g_control_thread, "ctrl_plane");
    return 0;
}

bool control_plane_consume_irq_latch(void)
{
    return atomic_cas(&g_irq_latch, 1, 0);
}

int control_plane_watchdog_register(const char *owner, uint32_t reload_ms)
{
    int channel;

    channel = task_wdt_add(reload_ms, NULL, NULL);
    if (channel < 0) {
        LOG_WRN("task_wdt_add failed for %s (%d)", owner ? owner : "unknown", channel);
        return channel;
    }

    k_mutex_lock(&g_wdt_lock, K_FOREVER);
    if (g_wdt_owner_count < EDGE_MAX_WDT_OWNERS) {
        g_wdt_owners[g_wdt_owner_count].channel = channel;
        if (owner != NULL) {
            strncpy(g_wdt_owners[g_wdt_owner_count].owner, owner,
                    sizeof(g_wdt_owners[g_wdt_owner_count].owner) - 1U);
            g_wdt_owners[g_wdt_owner_count].owner[sizeof(g_wdt_owners[g_wdt_owner_count].owner) - 1U] = '\0';
        } else {
            strcpy(g_wdt_owners[g_wdt_owner_count].owner, "unknown");
        }
        g_wdt_owner_count++;
    }
    k_mutex_unlock(&g_wdt_lock);

    return channel;
}

void control_plane_watchdog_feed(int channel_id)
{
    if (channel_id < 0) {
        return;
    }

    int rc = task_wdt_feed(channel_id);
    if (rc < 0) {
        LOG_DBG("task_wdt_feed failed channel=%d rc=%d", channel_id, rc);
    }
}
