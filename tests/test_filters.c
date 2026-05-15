#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "../include/fpc_config.h"
#include "../include/fpc_filters.h"
#include "test_harness.h"

static struct fpc_fir_config
fir_average_config(void)
{
    static const int32_t coeffs[] = {21845, 21845, 21845};
    struct fpc_fir_config cfg = {
        .order = 3U,
        .coeffs = coeffs
    };

    return cfg;
}

static struct fpc_biquad_config
biquad_identity_config(void)
{
    struct fpc_biquad_config cfg = {
        .b0 = 65536,
        .b1 = 0,
        .b2 = 0,
        .a1 = 0,
        .a2 = 0
    };

    return cfg;
}

TEST_CASE(test_filter_pool_init_required)
{
    struct fpc_fir *fir = NULL;
    struct fpc_fir_config cfg = fir_average_config();

    TEST_ASSERT(fpc_fir_init(&fir, &cfg) == FPC_STATUS_NOT_INITIALIZED);
    TEST_ASSERT(fir == NULL);
}

TEST_CASE(test_fir_init_and_process)
{
    struct fpc_fir *fir = NULL;
    struct fpc_fir_config cfg = fir_average_config();
    int32_t output = 0;

    TEST_ASSERT(fpc_filter_pool_init() == FPC_STATUS_OK);
    TEST_ASSERT(fpc_fir_init(&fir, &cfg) == FPC_STATUS_OK);
    TEST_ASSERT(fpc_fir_process(fir, 1000, &output) == FPC_STATUS_OK);
    TEST_ASSERT((output >= 330) && (output <= 340));
    TEST_ASSERT(fpc_fir_process(fir, 2000, &output) == FPC_STATUS_OK);
    TEST_ASSERT((output >= 990) && (output <= 1010));
    TEST_ASSERT(fpc_fir_process(fir, 2000, &output) == FPC_STATUS_OK);
    TEST_ASSERT((output >= 1650) && (output <= 1680));
    TEST_ASSERT(fpc_fir_deinit(fir) == FPC_STATUS_OK);
}

TEST_CASE(test_fir_reset_and_reconfigure)
{
    static const int32_t pass_coeffs[] = {65536};
    struct fpc_fir *fir = NULL;
    struct fpc_fir_config cfg = fir_average_config();
    struct fpc_fir_config pass_cfg = {
        .order = 1U,
        .coeffs = pass_coeffs
    };
    int32_t output = 0;

    TEST_ASSERT(fpc_filter_pool_init() == FPC_STATUS_OK);
    TEST_ASSERT(fpc_fir_init(&fir, &cfg) == FPC_STATUS_OK);
    TEST_ASSERT(fpc_fir_process(fir, 900, &output) == FPC_STATUS_OK);
    TEST_ASSERT(fpc_fir_reset(fir) == FPC_STATUS_OK);
    TEST_ASSERT(fpc_fir_process(fir, 900, &output) == FPC_STATUS_OK);
    TEST_ASSERT((output >= 290) && (output <= 310));
    TEST_ASSERT(fpc_fir_set_config(fir, &pass_cfg) == FPC_STATUS_OK);
    TEST_ASSERT(fpc_fir_process(fir, 1234, &output) == FPC_STATUS_OK);
    TEST_ASSERT(output == 1234);
    TEST_ASSERT(fpc_fir_deinit(fir) == FPC_STATUS_OK);
}

TEST_CASE(test_fir_invalid_usage)
{
    struct fpc_fir *fir = NULL;
    int32_t coeffs[FPC_FILTER_MAX_ORDER + 1] = {0};
    struct fpc_fir_config invalid_cfg = {
        .order = (uint16_t)(FPC_FILTER_MAX_ORDER + 1U),
        .coeffs = coeffs
    };
    int32_t output = 0;

    TEST_ASSERT(fpc_filter_pool_init() == FPC_STATUS_OK);
    TEST_ASSERT(fpc_fir_init(NULL, &invalid_cfg) == FPC_STATUS_NULL_PTR);
    TEST_ASSERT(fpc_fir_init(&fir, NULL) == FPC_STATUS_INVALID_PARAM);
    TEST_ASSERT(fpc_fir_init(&fir, &invalid_cfg) == FPC_STATUS_INVALID_PARAM);
    TEST_ASSERT(fpc_fir_process(NULL, 0, &output) == FPC_STATUS_NULL_PTR);
    TEST_ASSERT(fpc_fir_process(fir, 0, NULL) == FPC_STATUS_NULL_PTR);
}

TEST_CASE(test_fir_pool_exhaustion)
{
    struct fpc_fir *filters[FPC_MAX_INSTANCES] = {0};
    struct fpc_fir *extra = NULL;
    struct fpc_fir_config cfg = fir_average_config();
    uint16_t i;

    TEST_ASSERT(fpc_filter_pool_init() == FPC_STATUS_OK);
    for (i = 0U; i < FPC_MAX_INSTANCES; ++i) {
        TEST_ASSERT(fpc_fir_init(&filters[i], &cfg) == FPC_STATUS_OK);
    }

    TEST_ASSERT(fpc_fir_init(&extra, &cfg) == FPC_STATUS_POOL_FULL);
    TEST_ASSERT(extra == NULL);

    for (i = 0U; i < FPC_MAX_INSTANCES; ++i) {
        TEST_ASSERT(fpc_fir_deinit(filters[i]) == FPC_STATUS_OK);
    }
}

TEST_CASE(test_biquad_identity_and_reset)
{
    struct fpc_biquad *biquad = NULL;
    struct fpc_biquad_config cfg = biquad_identity_config();
    int32_t output = 0;

    TEST_ASSERT(fpc_filter_pool_init() == FPC_STATUS_OK);
    TEST_ASSERT(fpc_biquad_init(&biquad, &cfg) == FPC_STATUS_OK);
    TEST_ASSERT(fpc_biquad_process(biquad, 1000, &output) == FPC_STATUS_OK);
    TEST_ASSERT((output >= 900) && (output <= 1100));
    TEST_ASSERT(fpc_biquad_reset(biquad) == FPC_STATUS_OK);
    TEST_ASSERT(fpc_biquad_process(biquad, 2000, &output) == FPC_STATUS_OK);
    TEST_ASSERT((output >= 1900) && (output <= 2100));
    TEST_ASSERT(fpc_biquad_deinit(biquad) == FPC_STATUS_OK);
}

TEST_CASE(test_biquad_reconfigure)
{
    struct fpc_biquad *biquad = NULL;
    struct fpc_biquad_config cfg = biquad_identity_config();
    struct fpc_biquad_config lowpass_cfg = {
        .b0 = 32768,
        .b1 = 32768,
        .b2 = 0,
        .a1 = 32768,
        .a2 = 0
    };
    int32_t output = 0;

    TEST_ASSERT(fpc_filter_pool_init() == FPC_STATUS_OK);
    TEST_ASSERT(fpc_biquad_init(&biquad, &cfg) == FPC_STATUS_OK);
    TEST_ASSERT(fpc_biquad_set_config(biquad, &lowpass_cfg) == FPC_STATUS_OK);
    TEST_ASSERT(fpc_biquad_process(biquad, 1000, &output) == FPC_STATUS_OK);
    TEST_ASSERT((output >= 450) && (output <= 550));
    TEST_ASSERT(fpc_biquad_process(biquad, 1000, &output) == FPC_STATUS_OK);
    TEST_ASSERT((output >= 700) && (output <= 800));
    TEST_ASSERT(fpc_biquad_deinit(biquad) == FPC_STATUS_OK);
}

TEST_CASE(test_biquad_invalid_usage)
{
    struct fpc_biquad *biquad = NULL;
    struct fpc_biquad_config cfg = biquad_identity_config();
    int32_t output = 0;

    TEST_ASSERT(fpc_filter_pool_init() == FPC_STATUS_OK);
    TEST_ASSERT(fpc_biquad_init(NULL, &cfg) == FPC_STATUS_NULL_PTR);
    TEST_ASSERT(fpc_biquad_init(&biquad, NULL) == FPC_STATUS_INVALID_PARAM);
    TEST_ASSERT(fpc_biquad_process(NULL, 0, &output) == FPC_STATUS_NULL_PTR);
    TEST_ASSERT(fpc_biquad_process(biquad, 0, NULL) == FPC_STATUS_NULL_PTR);
}

TEST_CASE(test_filter_overflow_reporting)
{
    static const int32_t fir_coeffs[] = {INT32_MAX};
    struct fpc_fir *fir = NULL;
    struct fpc_fir_config fir_cfg = {
        .order = 1U,
        .coeffs = fir_coeffs
    };
    struct fpc_biquad *biquad = NULL;
    struct fpc_biquad_config biquad_cfg = {
        .b0 = INT32_MAX,
        .b1 = 0,
        .b2 = 0,
        .a1 = 0,
        .a2 = 0
    };
    int32_t output = 0;

    TEST_ASSERT(fpc_filter_pool_init() == FPC_STATUS_OK);
    TEST_ASSERT(fpc_fir_init(&fir, &fir_cfg) == FPC_STATUS_OK);
    TEST_ASSERT(fpc_fir_process(fir, INT32_MAX, &output) == FPC_STATUS_OVERFLOW);
    TEST_ASSERT(output == INT32_MAX);
    TEST_ASSERT(fpc_fir_deinit(fir) == FPC_STATUS_OK);

    TEST_ASSERT(fpc_biquad_init(&biquad, &biquad_cfg) == FPC_STATUS_OK);
    TEST_ASSERT(fpc_biquad_process(biquad, INT32_MAX, &output) == FPC_STATUS_OVERFLOW);
    TEST_ASSERT(output == INT32_MAX);
    TEST_ASSERT(fpc_biquad_deinit(biquad) == FPC_STATUS_OK);
}

TEST_CASE(test_biquad_pool_exhaustion)
{
    struct fpc_biquad *filters[FPC_MAX_INSTANCES] = {0};
    struct fpc_biquad *extra = NULL;
    struct fpc_biquad_config cfg = biquad_identity_config();
    uint16_t i;

    TEST_ASSERT(fpc_filter_pool_init() == FPC_STATUS_OK);
    for (i = 0U; i < FPC_MAX_INSTANCES; ++i) {
        TEST_ASSERT(fpc_biquad_init(&filters[i], &cfg) == FPC_STATUS_OK);
    }

    TEST_ASSERT(fpc_biquad_init(&extra, &cfg) == FPC_STATUS_POOL_FULL);
    TEST_ASSERT(extra == NULL);

    for (i = 0U; i < FPC_MAX_INSTANCES; ++i) {
        TEST_ASSERT(fpc_biquad_deinit(filters[i]) == FPC_STATUS_OK);
    }
}

void
run_filters_tests(void)
{
    run_test(test_filter_pool_init_required, "filter_pool_init_required");
    run_test(test_fir_init_and_process, "fir_init_and_process");
    run_test(test_fir_reset_and_reconfigure, "fir_reset_and_reconfigure");
    run_test(test_fir_invalid_usage, "fir_invalid_usage");
    run_test(test_fir_pool_exhaustion, "fir_pool_exhaustion");
    run_test(test_biquad_identity_and_reset, "biquad_identity_and_reset");
    run_test(test_biquad_reconfigure, "biquad_reconfigure");
    run_test(test_biquad_invalid_usage, "biquad_invalid_usage");
    run_test(test_filter_overflow_reporting, "filter_overflow_reporting");
    run_test(test_biquad_pool_exhaustion, "biquad_pool_exhaustion");
}
