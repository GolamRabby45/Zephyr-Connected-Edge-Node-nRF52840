#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <stdint.h>

#include "app_config.h"
#include "app_types.h"
#include "control_plane.h"
#include "rtos_channels.h"
#include "sampler.h"
#include "sensor_bus.h"

LOG_MODULE_REGISTER(sampler, LOG_LEVEL_INF);

#define EDGE_SAMPLER_STACK_SIZE 2048
#define EDGE_SAMPLER_PRIORITY   2

static struct k_timer g_sample_timer;
static uint32_t g_sequence;

K_THREAD_STACK_DEFINE(g_sampler_stack, EDGE_SAMPLER_STACK_SIZE);
static struct k_thread g_sampler_thread;

static void sample_timer_fn(struct k_timer *timer)
{
    ARG_UNUSED(timer);
    k_sem_give(&edge_sample_trigger_sem);
}

static void sampler_thread_fn(void *arg1, void *arg2, void *arg3)
{
    ARG_UNUSED(arg1);
    ARG_UNUSED(arg2);
    ARG_UNUSED(arg3);

    int wdt_channel = control_plane_watchdog_register("sampler", 3000U);
    g_sequence = 0U;

    while (true) {
        k_sem_take(&edge_sample_trigger_sem, K_FOREVER);

        struct edge_raw_sample sample = {
            .sequence = ++g_sequence,
            .ts_ms = k_uptime_get(),
            .i2c_raw = 0,
            .spi_raw = 0,
            .flags = 0,
        };

        if (control_plane_consume_irq_latch()) {
            sample.flags |= EDGE_FLAGS_ISR_EVENT;
        }

        (void)sensor_bus_read(&sample);

        if (k_msgq_put(&edge_raw_sample_msgq, &sample, K_NO_WAIT) != 0) {
            LOG_WRN("raw sample queue full; dropping sample %u", sample.sequence);
        }

        control_plane_watchdog_feed(wdt_channel);
    }
}

int sampler_start(void)
{
    const uint32_t sample_period_ms = app_config_get_sample_period_ms();

    k_timer_init(&g_sample_timer, sample_timer_fn, NULL);
    k_timer_start(&g_sample_timer, K_MSEC(sample_period_ms), K_MSEC(sample_period_ms));

    (void)k_thread_create(&g_sampler_thread,
                          g_sampler_stack,
                          K_THREAD_STACK_SIZEOF(g_sampler_stack),
                          sampler_thread_fn,
                          NULL,
                          NULL,
                          NULL,
                          EDGE_SAMPLER_PRIORITY,
                          0,
                          K_NO_WAIT);

    k_thread_name_set(&g_sampler_thread, "sampler");
    LOG_INF("sampler started (%u ms period)", sample_period_ms);
    return 0;
}

void sampler_update_period_ms(uint32_t period_ms)
{
    k_timer_stop(&g_sample_timer);
    k_timer_start(&g_sample_timer, K_MSEC(period_ms), K_MSEC(period_ms));
}
