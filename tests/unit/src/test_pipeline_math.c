#include <zephyr/ztest.h>

#include "pipeline_math.h"

ZTEST(edge_pipeline_math, test_low_pass_fusion)
{
    int32_t prev = 100;
    int16_t i2c = 40;
    int16_t spi = 20;

    int32_t fused = pipeline_compute_fused(prev, i2c, spi);

    zassert_equal(fused, 90, "unexpected fused value: %ld", (long)fused);
}

ZTEST(edge_pipeline_math, test_negative_signal)
{
    int32_t prev = -80;
    int16_t i2c = -20;
    int16_t spi = 10;

    int32_t fused = pipeline_compute_fused(prev, i2c, spi);

    zassert_true(fused < 0, "fused should stay negative");
}

ZTEST_SUITE(edge_pipeline_math, NULL, NULL, NULL, NULL, NULL);
