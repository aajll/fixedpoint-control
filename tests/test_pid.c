#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "../include/fpc_config.h"
#include "../include/fpc_pid.h"
#include "test_harness.h"

static struct fpc_pid_config
test_config(void)
{
    struct fpc_pid_config cfg = {
        .kp = 1000,
        .ki = 500,
        .kd = 200,
        .dt = 1000,
        .out_min = -100,
        .out_max = 100,
        .integral_min = -50,
        .integral_max = 50,
        .d_filter_alpha = 65536U
    };

    return cfg;
}

TEST_CASE(test_pool_init_required)
{
    struct fpc_pid *ctx = NULL;
    struct fpc_pid_config cfg = test_config();

    TEST_ASSERT(fpc_pid_init(&ctx, &cfg) == FPC_STATUS_NOT_INITIALIZED);
    TEST_ASSERT(ctx == NULL);
}

TEST_CASE(test_init_and_get_config)
{
    struct fpc_pid *ctx = NULL;
    struct fpc_pid_config cfg = test_config();
    struct fpc_pid_config copy;

    TEST_ASSERT(fpc_pid_pool_init() == FPC_STATUS_OK);
    TEST_ASSERT(fpc_pid_init(&ctx, &cfg) == FPC_STATUS_OK);
    TEST_ASSERT(ctx != NULL);
    TEST_ASSERT(fpc_pid_get_config(ctx, &copy) == FPC_STATUS_OK);
    TEST_ASSERT(copy.kp == cfg.kp);
    TEST_ASSERT(copy.ki == cfg.ki);
    TEST_ASSERT(copy.kd == cfg.kd);
    TEST_ASSERT(copy.dt == cfg.dt);
    TEST_ASSERT(copy.out_min == cfg.out_min);
    TEST_ASSERT(copy.out_max == cfg.out_max);
    TEST_ASSERT(copy.integral_min == cfg.integral_min);
    TEST_ASSERT(copy.integral_max == cfg.integral_max);
    TEST_ASSERT(copy.d_filter_alpha == cfg.d_filter_alpha);
    TEST_ASSERT(fpc_pid_deinit(ctx) == FPC_STATUS_OK);
}

TEST_CASE(test_invalid_config_rejected)
{
    struct fpc_pid *ctx = NULL;
    struct fpc_pid_config cfg = test_config();

    TEST_ASSERT(fpc_pid_init(NULL, &cfg) == FPC_STATUS_NULL_PTR);
    TEST_ASSERT(fpc_pid_init(&ctx, NULL) == FPC_STATUS_INVALID_PARAM);

    cfg.dt = 0;
    TEST_ASSERT(fpc_pid_init(&ctx, &cfg) == FPC_STATUS_INVALID_PARAM);

    cfg = test_config();
    cfg.out_min = 10;
    cfg.out_max = 0;
    TEST_ASSERT(fpc_pid_init(&ctx, &cfg) == FPC_STATUS_INVALID_PARAM);

    cfg = test_config();
    cfg.integral_min = 10;
    cfg.integral_max = 0;
    TEST_ASSERT(fpc_pid_init(&ctx, &cfg) == FPC_STATUS_INVALID_PARAM);

    cfg = test_config();
    cfg.d_filter_alpha = 65537U;
    TEST_ASSERT(fpc_pid_init(&ctx, &cfg) == FPC_STATUS_INVALID_PARAM);
}

TEST_CASE(test_saturation_reporting)
{
    struct fpc_pid *ctx = NULL;
    struct fpc_pid_config cfg = test_config();
    int32_t output = 0;

    TEST_ASSERT(fpc_pid_pool_init() == FPC_STATUS_OK);
    TEST_ASSERT(fpc_pid_init(&ctx, &cfg) == FPC_STATUS_OK);
    TEST_ASSERT(fpc_pid_compute(ctx, 10000, 0, &output) == FPC_STATUS_SATURATED);
    TEST_ASSERT(output == 100);
    TEST_ASSERT(fpc_pid_compute(ctx, -10000, 0, &output) == FPC_STATUS_SATURATED);
    TEST_ASSERT(output == -100);
    TEST_ASSERT(fpc_pid_deinit(ctx) == FPC_STATUS_OK);
}

TEST_CASE(test_state_retrieval)
{
    struct fpc_pid *ctx = NULL;
    struct fpc_pid_config cfg = test_config();
    struct fpc_pid_state state;
    int32_t output = 0;

    cfg.out_min = -1000;
    cfg.out_max = 1000;
    cfg.integral_min = -1000;
    cfg.integral_max = 1000;

    TEST_ASSERT(fpc_pid_pool_init() == FPC_STATUS_OK);
    TEST_ASSERT(fpc_pid_init(&ctx, &cfg) == FPC_STATUS_OK);
    TEST_ASSERT(fpc_pid_compute(ctx, 1000, 500, &output) == FPC_STATUS_OK);
    TEST_ASSERT(fpc_pid_get_state(ctx, &state) == FPC_STATUS_OK);
    TEST_ASSERT(state.integral > 0);
    TEST_ASSERT(state.prev_error == 500);
    TEST_ASSERT(state.filtered_derivative == 100);
    TEST_ASSERT(state.last_output == output);
    TEST_ASSERT(state.mode == FPC_PID_MODE_AUTO);
    TEST_ASSERT(fpc_pid_deinit(ctx) == FPC_STATUS_OK);
}

TEST_CASE(test_derivative_filtering)
{
    struct fpc_pid *ctx = NULL;
    struct fpc_pid_config cfg = test_config();
    struct fpc_pid_state state;
    int32_t output = 0;

    cfg.kp = 0;
    cfg.ki = 0;
    cfg.kd = 1000;
    cfg.out_min = -1000;
    cfg.out_max = 1000;
    cfg.integral_min = -1000;
    cfg.integral_max = 1000;
    cfg.d_filter_alpha = 32768U;

    TEST_ASSERT(fpc_pid_pool_init() == FPC_STATUS_OK);
    TEST_ASSERT(fpc_pid_init(&ctx, &cfg) == FPC_STATUS_OK);
    TEST_ASSERT(fpc_pid_compute(ctx, 1000, 0, &output) == FPC_STATUS_OK);
    TEST_ASSERT(output == 500);
    TEST_ASSERT(fpc_pid_get_state(ctx, &state) == FPC_STATUS_OK);
    TEST_ASSERT(state.filtered_derivative == 500);
    TEST_ASSERT(fpc_pid_compute(ctx, 1000, 0, &output) == FPC_STATUS_OK);
    TEST_ASSERT(output == 250);
    TEST_ASSERT(fpc_pid_get_state(ctx, &state) == FPC_STATUS_OK);
    TEST_ASSERT(state.filtered_derivative == 250);
    TEST_ASSERT(fpc_pid_deinit(ctx) == FPC_STATUS_OK);
}

TEST_CASE(test_reset_clears_runtime_state)
{
    struct fpc_pid *ctx = NULL;
    struct fpc_pid_config cfg = test_config();
    struct fpc_pid_state state;
    int32_t output = 0;

    TEST_ASSERT(fpc_pid_pool_init() == FPC_STATUS_OK);
    TEST_ASSERT(fpc_pid_init(&ctx, &cfg) == FPC_STATUS_OK);
    TEST_ASSERT(fpc_pid_compute(ctx, 1000, 0, &output) == FPC_STATUS_SATURATED);
    TEST_ASSERT(fpc_pid_reset(ctx) == FPC_STATUS_OK);
    TEST_ASSERT(fpc_pid_get_state(ctx, &state) == FPC_STATUS_OK);
    TEST_ASSERT(state.integral == 0);
    TEST_ASSERT(state.prev_error == 0);
    TEST_ASSERT(state.filtered_derivative == 0);
    TEST_ASSERT(state.last_output == 0);
    TEST_ASSERT(fpc_pid_deinit(ctx) == FPC_STATUS_OK);
}

TEST_CASE(test_set_config_clamps_state)
{
    struct fpc_pid *ctx = NULL;
    struct fpc_pid_config cfg = test_config();
    struct fpc_pid_config tighter_cfg = test_config();
    struct fpc_pid_state state;
    int32_t output = 0;

    tighter_cfg.out_min = -10;
    tighter_cfg.out_max = 10;
    tighter_cfg.integral_min = -5;
    tighter_cfg.integral_max = 5;

    TEST_ASSERT(fpc_pid_pool_init() == FPC_STATUS_OK);
    TEST_ASSERT(fpc_pid_init(&ctx, &cfg) == FPC_STATUS_OK);
    TEST_ASSERT(fpc_pid_compute(ctx, 1000, 0, &output) == FPC_STATUS_SATURATED);
    TEST_ASSERT(fpc_pid_set_config(ctx, &tighter_cfg) == FPC_STATUS_SATURATED);
    TEST_ASSERT(fpc_pid_get_state(ctx, &state) == FPC_STATUS_OK);
    TEST_ASSERT(state.integral <= 5);
    TEST_ASSERT(state.last_output <= 10);
    TEST_ASSERT(fpc_pid_deinit(ctx) == FPC_STATUS_OK);
}

TEST_CASE(test_pool_exhaustion)
{
    struct fpc_pid *ctx[FPC_MAX_INSTANCES] = {0};
    struct fpc_pid *extra = NULL;
    struct fpc_pid_config cfg = test_config();
    uint16_t i;

    TEST_ASSERT(fpc_pid_pool_init() == FPC_STATUS_OK);
    for (i = 0U; i < FPC_MAX_INSTANCES; ++i) {
        TEST_ASSERT(fpc_pid_init(&ctx[i], &cfg) == FPC_STATUS_OK);
    }

    TEST_ASSERT(fpc_pid_init(&extra, &cfg) == FPC_STATUS_POOL_FULL);
    TEST_ASSERT(extra == NULL);

    for (i = 0U; i < FPC_MAX_INSTANCES; ++i) {
        TEST_ASSERT(fpc_pid_deinit(ctx[i]) == FPC_STATUS_OK);
    }
}

TEST_CASE(test_manual_mode_and_bumpless_return)
{
    struct fpc_pid *ctx = NULL;
    struct fpc_pid_config cfg = test_config();
    struct fpc_pid_state state;
    int32_t output = 0;

    cfg.out_min = -1000;
    cfg.out_max = 1000;
    cfg.integral_min = -1000;
    cfg.integral_max = 1000;
    cfg.ki = 0;
    cfg.kd = 0;

    TEST_ASSERT(fpc_pid_pool_init() == FPC_STATUS_OK);
    TEST_ASSERT(fpc_pid_init(&ctx, &cfg) == FPC_STATUS_OK);
    TEST_ASSERT(fpc_pid_set_mode(ctx, FPC_PID_MODE_MANUAL, 300) == FPC_STATUS_OK);
    TEST_ASSERT(fpc_pid_compute(ctx, 1000, 900, &output) == FPC_STATUS_OK);
    TEST_ASSERT(output == 300);
    TEST_ASSERT(fpc_pid_get_state(ctx, &state) == FPC_STATUS_OK);
    TEST_ASSERT(state.mode == FPC_PID_MODE_MANUAL);
    TEST_ASSERT(state.prev_error == 100);

    TEST_ASSERT(fpc_pid_set_mode(ctx, FPC_PID_MODE_AUTO, 0) == FPC_STATUS_OK);
    TEST_ASSERT(fpc_pid_get_state(ctx, &state) == FPC_STATUS_OK);
    TEST_ASSERT(state.mode == FPC_PID_MODE_AUTO);
    TEST_ASSERT(state.integral == 200);
    TEST_ASSERT(fpc_pid_compute(ctx, 1000, 900, &output) == FPC_STATUS_OK);
    TEST_ASSERT(output == 300);
    TEST_ASSERT(fpc_pid_deinit(ctx) == FPC_STATUS_OK);
}

TEST_CASE(test_manual_mode_clamps_output)
{
    struct fpc_pid *ctx = NULL;
    struct fpc_pid_config cfg = test_config();
    int32_t output = 0;

    TEST_ASSERT(fpc_pid_pool_init() == FPC_STATUS_OK);
    TEST_ASSERT(fpc_pid_init(&ctx, &cfg) == FPC_STATUS_OK);
    TEST_ASSERT(fpc_pid_set_mode(ctx, FPC_PID_MODE_MANUAL, 1000) == FPC_STATUS_SATURATED);
    TEST_ASSERT(fpc_pid_compute(ctx, 0, 0, &output) == FPC_STATUS_OK);
    TEST_ASSERT(output == 100);
    TEST_ASSERT(fpc_pid_deinit(ctx) == FPC_STATUS_OK);
}

TEST_CASE(test_null_and_invalid_runtime_calls)
{
    struct fpc_pid *ctx = NULL;
    struct fpc_pid_config cfg = test_config();
    struct fpc_pid_config copy;
    int32_t output = 0;
    struct fpc_pid_state state;

    TEST_ASSERT(fpc_pid_pool_init() == FPC_STATUS_OK);
    TEST_ASSERT(fpc_pid_reset(NULL) == FPC_STATUS_NULL_PTR);
    TEST_ASSERT(fpc_pid_get_config(NULL, &copy) == FPC_STATUS_NULL_PTR);
    TEST_ASSERT(fpc_pid_get_config(ctx, NULL) == FPC_STATUS_NULL_PTR);
    TEST_ASSERT(fpc_pid_set_mode(NULL, FPC_PID_MODE_MANUAL, 0) == FPC_STATUS_NULL_PTR);
    TEST_ASSERT(fpc_pid_get_state(NULL, &state) == FPC_STATUS_NULL_PTR);
    TEST_ASSERT(fpc_pid_get_state(ctx, NULL) == FPC_STATUS_NULL_PTR);
    TEST_ASSERT(fpc_pid_compute(NULL, 0, 0, &output) == FPC_STATUS_NULL_PTR);
    TEST_ASSERT(fpc_pid_compute(ctx, 0, 0, NULL) == FPC_STATUS_NULL_PTR);

    TEST_ASSERT(fpc_pid_init(&ctx, &cfg) == FPC_STATUS_OK);
    TEST_ASSERT(fpc_pid_deinit(ctx) == FPC_STATUS_OK);
    TEST_ASSERT(fpc_pid_compute(ctx, 1000, 0, &output) == FPC_STATUS_NOT_INITIALIZED);
    TEST_ASSERT(fpc_pid_reset(ctx) == FPC_STATUS_NOT_INITIALIZED);
    TEST_ASSERT(fpc_pid_set_mode(ctx, FPC_PID_MODE_AUTO, 0) == FPC_STATUS_NOT_INITIALIZED);
}

TEST_CASE(test_overflow_detection)
{
    struct fpc_pid *ctx = NULL;
    struct fpc_pid_config cfg = test_config();
    int32_t output = 0;

    TEST_ASSERT(fpc_pid_pool_init() == FPC_STATUS_OK);
    TEST_ASSERT(fpc_pid_init(&ctx, &cfg) == FPC_STATUS_OK);
    TEST_ASSERT(fpc_pid_compute(ctx, INT32_MAX, INT32_MIN, &output) == FPC_STATUS_OVERFLOW);
    TEST_ASSERT(output == 0);
    TEST_ASSERT(fpc_pid_deinit(ctx) == FPC_STATUS_OK);
}

void
run_pid_tests(void)
{
    run_test(test_pool_init_required, "pool_init_required");
    run_test(test_init_and_get_config, "init_and_get_config");
    run_test(test_invalid_config_rejected, "invalid_config_rejected");
    run_test(test_saturation_reporting, "saturation_reporting");
    run_test(test_state_retrieval, "state_retrieval");
    run_test(test_derivative_filtering, "derivative_filtering");
    run_test(test_reset_clears_runtime_state, "reset_clears_runtime_state");
    run_test(test_set_config_clamps_state, "set_config_clamps_state");
    run_test(test_pool_exhaustion, "pool_exhaustion");
    run_test(test_manual_mode_and_bumpless_return, "manual_mode_and_bumpless_return");
    run_test(test_manual_mode_clamps_output, "manual_mode_clamps_output");
    run_test(test_null_and_invalid_runtime_calls, "null_and_invalid_runtime_calls");
    run_test(test_overflow_detection, "overflow_detection");
}
