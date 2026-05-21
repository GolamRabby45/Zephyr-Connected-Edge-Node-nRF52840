#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

#include <errno.h>
#include <stdint.h>
#include <string.h>

#include "sensor_bus.h"

LOG_MODULE_REGISTER(sensor_bus, LOG_LEVEL_INF);

#define EDGE_I2C_SENSOR_ADDR 0x68
#define EDGE_I2C_REG_WHOAMI  0x75

static const struct device *g_i2c_dev;
static const struct device *g_spi_dev;
static struct spi_config g_spi_cfg;
static uint32_t g_fallback_counter;

static int read_i2c_byte(int16_t *out_val)
{
    uint8_t reg = EDGE_I2C_REG_WHOAMI;
    uint8_t value = 0U;

    if (g_i2c_dev == NULL) {
        return -ENODEV;
    }

    int rc = i2c_write_read(g_i2c_dev, EDGE_I2C_SENSOR_ADDR, &reg, sizeof(reg), &value, sizeof(value));
    if (rc < 0) {
        return rc;
    }

    *out_val = (int16_t)value;
    return 0;
}

static int read_spi_byte(int16_t *out_val)
{
    uint8_t cmd = 0x9FU;
    uint8_t tx_pad[1] = {0U};
    uint8_t rx_dummy = 0U;
    uint8_t rx_data = 0U;

    struct spi_buf tx_bufs[2] = {
        { .buf = &cmd, .len = 1U },
        { .buf = tx_pad, .len = 1U },
    };

    struct spi_buf rx_bufs[2] = {
        { .buf = &rx_dummy, .len = 1U },
        { .buf = &rx_data, .len = 1U },
    };

    struct spi_buf_set tx = {
        .buffers = tx_bufs,
        .count = ARRAY_SIZE(tx_bufs),
    };

    struct spi_buf_set rx = {
        .buffers = rx_bufs,
        .count = ARRAY_SIZE(rx_bufs),
    };

    if (g_spi_dev == NULL) {
        return -ENODEV;
    }

    int rc = spi_transceive(g_spi_dev, &g_spi_cfg, &tx, &rx);
    if (rc < 0) {
        return rc;
    }

    *out_val = (int16_t)rx_data;
    return 0;
}

int sensor_bus_init(void)
{
    g_i2c_dev = DEVICE_DT_GET(DT_NODELABEL(i2c0));
    g_spi_dev = DEVICE_DT_GET(DT_NODELABEL(spi1));

    if (!device_is_ready(g_i2c_dev)) {
        LOG_WRN("i2c0 not ready; fallback sample mode will be used");
        g_i2c_dev = NULL;
    }

    if (!device_is_ready(g_spi_dev)) {
        LOG_WRN("spi1 not ready; fallback sample mode will be used");
        g_spi_dev = NULL;
    }

    memset(&g_spi_cfg, 0, sizeof(g_spi_cfg));
    g_spi_cfg.frequency = 1000000U;
    g_spi_cfg.operation = SPI_WORD_SET(8) | SPI_TRANSFER_MSB;
    g_spi_cfg.slave = 0U;

    g_fallback_counter = 0U;

    return 0;
}

int sensor_bus_read(struct edge_raw_sample *sample)
{
    int rc_i2c;
    int rc_spi;
    int16_t i2c_val = 0;
    int16_t spi_val = 0;

    if (sample == NULL) {
        return -EINVAL;
    }

    rc_i2c = read_i2c_byte(&i2c_val);
    if (rc_i2c == 0) {
        sample->flags |= EDGE_FLAGS_I2C_OK;
    } else {
        i2c_val = (int16_t)(k_cycle_get_32() & 0xFF);
    }

    rc_spi = read_spi_byte(&spi_val);
    if (rc_spi == 0) {
        sample->flags |= EDGE_FLAGS_SPI_OK;
    } else {
        spi_val = (int16_t)((k_cycle_get_32() >> 8) & 0xFF);
    }

    if (rc_i2c < 0 && rc_spi < 0) {
        g_fallback_counter++;
        i2c_val = (int16_t)(g_fallback_counter & 0x3FF);
        spi_val = (int16_t)((g_fallback_counter * 3U) & 0x3FF);
    }

    sample->i2c_raw = i2c_val;
    sample->spi_raw = spi_val;

    return 0;
}
