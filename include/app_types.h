#ifndef EDGE_APP_TYPES_H_
#define EDGE_APP_TYPES_H_

#include <stdint.h>
#include <zephyr/sys/util.h>

#define EDGE_FLAGS_ISR_EVENT BIT(0)
#define EDGE_FLAGS_I2C_OK    BIT(1)
#define EDGE_FLAGS_SPI_OK    BIT(2)

enum edge_control_cmd_type {
    EDGE_CMD_SET_SAMPLE_PERIOD_MS = 1,
    EDGE_CMD_FORCE_FLUSH = 2,
    EDGE_CMD_FORCE_RECONNECT = 3,
};

enum edge_irq_event_type {
    EDGE_IRQ_BUTTON_PRESS = 1,
};

struct edge_raw_sample {
    uint32_t sequence;
    int64_t ts_ms;
    int16_t i2c_raw;
    int16_t spi_raw;
    uint8_t flags;
};

struct edge_processed_sample {
    uint32_t sequence;
    int64_t ts_ms;
    int16_t i2c_raw;
    int16_t spi_raw;
    int32_t fused_value;
    uint8_t quality;
    uint8_t flags;
};

struct edge_control_cmd {
    enum edge_control_cmd_type type;
    uint32_t value_u32;
};

struct edge_irq_event {
    enum edge_irq_event_type type;
    uint32_t count;
    int64_t ts_ms;
};

#endif
