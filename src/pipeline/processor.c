#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <stdint.h>

#include "app_types.h"
#include "control_plane.h"
#include "pipeline_math.h"
#include "processor.h"
#include "rtos_channels.h"

LOG_MODULE_REGISTER(processor, LOG_LEVEL_INF);

#define EDGE_PIPELINE_STACK_SIZE 2300
#define EDGE_PIPELINE_PRIORITY   4
#define EDGE_WORKQ_STACK_SIZE    2048
#define EDGE_WORKQ_PRIORITY      3

static struct k_work_q g_pipeline_workq;
K_THREAD_STACK_DEFINE(g_pipeline_workq_stack, EDGE_WORKQ_STACK_SIZE);

static struct k_work g_process_work;
static struct k_mutex g_process_lock;
static struct edge_raw_sample g_pending_sample;
static struct edge_processed_sample g_output_sample;
static int32_t g_prev_fused;

K_THREAD_STACK_DEFINE(g_pipeline_stack, EDGE_PIPELINE_STACK_SIZE);
static struct k_thread g_pipeline_thread;

static void process_work_handler(struct k_work *work)
{
    ARG_UNUSED(work);

    struct edge_raw_sample in;
    struct edge_processed_sample out;

    k_mutex_lock(&g_process_lock, K_FOREVER);
    in = g_pending_sample;
    k_mutex_unlock(&g_process_lock);

    /* Simple low-pass style fusion for deterministic lightweight processing. */
    int32_t fused = pipeline_compute_fused(g_prev_fused, in.i2c_raw, in.spi_raw);
    g_prev_fused = fused;

    out.sequence = in.sequence;
    out.ts_ms = in.ts_ms;
    out.i2c_raw = in.i2c_raw;
    out.spi_raw = in.spi_raw;
    out.fused_value = fused;
    out.quality = (uint8_t)(((in.flags & EDGE_FLAGS_I2C_OK) && (in.flags & EDGE_FLAGS_SPI_OK)) ? 100 : 70);
    out.flags = in.flags;

    k_mutex_lock(&g_process_lock, K_FOREVER);
    g_output_sample = out;
    k_mutex_unlock(&g_process_lock);

    if (k_msgq_put(&edge_processed_sample_msgq, &out, K_NO_WAIT) != 0) {
        LOG_WRN("processed sample queue full; dropping sample %u", out.sequence);
    }

    k_sem_give(&edge_work_done_sem);
}

static void pipeline_thread_fn(void *arg1, void *arg2, void *arg3)
{
    ARG_UNUSED(arg1);
    ARG_UNUSED(arg2);
    ARG_UNUSED(arg3);

    int wdt_channel = control_plane_watchdog_register("pipeline", 4500U);
    struct edge_raw_sample incoming;

    while (true) {
        if (k_msgq_get(&edge_raw_sample_msgq, &incoming, K_FOREVER) != 0) {
            continue;
        }

        k_mutex_lock(&g_process_lock, K_FOREVER);
        g_pending_sample = incoming;
        k_mutex_unlock(&g_process_lock);

        (void)k_work_submit_to_queue(&g_pipeline_workq, &g_process_work);
        (void)k_sem_take(&edge_work_done_sem, K_MSEC(50));

        control_plane_watchdog_feed(wdt_channel);
    }
}

int processor_start(void)
{
    k_mutex_init(&g_process_lock);
    k_work_init(&g_process_work, process_work_handler);

    k_work_queue_start(&g_pipeline_workq,
                       g_pipeline_workq_stack,
                       K_THREAD_STACK_SIZEOF(g_pipeline_workq_stack),
                       EDGE_WORKQ_PRIORITY,
                       NULL);
    k_thread_name_set(&g_pipeline_workq.thread, "pipe_workq");

    (void)k_thread_create(&g_pipeline_thread,
                          g_pipeline_stack,
                          K_THREAD_STACK_SIZEOF(g_pipeline_stack),
                          pipeline_thread_fn,
                          NULL,
                          NULL,
                          NULL,
                          EDGE_PIPELINE_PRIORITY,
                          0,
                          K_NO_WAIT);
    k_thread_name_set(&g_pipeline_thread, "pipeline");

    g_prev_fused = 0;
    LOG_INF("pipeline started");
    return 0;
}
