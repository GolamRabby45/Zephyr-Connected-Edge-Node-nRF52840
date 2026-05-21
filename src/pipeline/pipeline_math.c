#include "pipeline_math.h"

int32_t pipeline_compute_fused(int32_t previous, int16_t i2c_raw, int16_t spi_raw)
{
    int32_t instant = (int32_t)i2c_raw + (int32_t)spi_raw;
    return (previous * 3 + instant) / 4;
}
