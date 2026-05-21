#ifndef EDGE_PIPELINE_MATH_H_
#define EDGE_PIPELINE_MATH_H_

#include <stdint.h>

int32_t pipeline_compute_fused(int32_t previous, int16_t i2c_raw, int16_t spi_raw);

#endif
