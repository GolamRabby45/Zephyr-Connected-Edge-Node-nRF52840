#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <errno.h>
#include <stdint.h>

#include "app_types.h"
#include "irq_button.h"
#include "rtos_channels.h"

LOG_MODULE_REGISTER(irq_button, LOG_LEVEL_INF);

#define SW0_NODE DT_ALIAS(sw0)

#if !DT_NODE_HAS_STATUS(SW0_NODE, okay)
#error "sw0 alias is required for ISR-to-thread handoff demo"
#endif

static const struct gpio_dt_spec g_button = GPIO_DT_SPEC_GET(SW0_NODE, gpios);
static struct gpio_callback g_button_cb;
static uint32_t g_button_count;

static void button_isr(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    ARG_UNUSED(dev);
    ARG_UNUSED(cb);
    ARG_UNUSED(pins);

    struct edge_irq_event evt = {
        .type = EDGE_IRQ_BUTTON_PRESS,
        .count = ++g_button_count,
        .ts_ms = k_uptime_get(),
    };

    (void)k_msgq_put(&edge_irq_event_msgq, &evt, K_NO_WAIT);
}

int irq_button_init(void)
{
    int rc;

    if (!device_is_ready(g_button.port)) {
        LOG_ERR("button gpio not ready");
        return -ENODEV;
    }

    rc = gpio_pin_configure_dt(&g_button, GPIO_INPUT);
    if (rc < 0) {
        LOG_ERR("gpio_pin_configure_dt failed (%d)", rc);
        return rc;
    }

    rc = gpio_pin_interrupt_configure_dt(&g_button, GPIO_INT_EDGE_TO_ACTIVE);
    if (rc < 0) {
        LOG_ERR("gpio_pin_interrupt_configure_dt failed (%d)", rc);
        return rc;
    }

    gpio_init_callback(&g_button_cb, button_isr, BIT(g_button.pin));
    rc = gpio_add_callback(g_button.port, &g_button_cb);
    if (rc < 0) {
        LOG_ERR("gpio_add_callback failed (%d)", rc);
        return rc;
    }

    g_button_count = 0U;
    LOG_INF("ISR button handoff enabled on pin %u", g_button.pin);
    return 0;
}
