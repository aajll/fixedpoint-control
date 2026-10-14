#include "../include/fpc_pid.h"
#include "test_harness.h"
#include <stddef.h>
#include <stdint.h>

/* Signals are raw fxp_t words. Expected values are exact under the default
 * round-to-nearest-even policy. */

#define GAIN_0_2 ((fxp_t)13107) /* 0.199997 in Q16.16 */

static struct fpc_pid_config
test_config(void)
{
        struct fpc_pid_config cfg = {.kp = FXP_ONE,
                                     .ki = FXP_ONE / 2,
                                     .kd = GAIN_0_2,
                                     .dt = FXP_ONE,
                                     .out_min = -100,
                                     .out_max = 100,
                                     .integral_min = -50,
                                     .integral_max = 50,
                                     .d_filter_alpha =
                                         FPC_PID_D_FILTER_ALPHA_MAX};

        return cfg;
}

static struct fpc_pid_config
wide_config(void)
{
        struct fpc_pid_config cfg = test_config();

        cfg.out_min = -1000;
        cfg.out_max = 1000;
        cfg.integral_min = -1000;
        cfg.integral_max = 1000;
        return cfg;
}

/* Only one term enabled; wide bounds so nothing clamps. */
static struct fpc_pid_config
term_config(fxp_t kp, fxp_t ki, fxp_t kd, fxp_t dt)
{
        struct fpc_pid_config cfg = {.kp = kp,
                                     .ki = ki,
                                     .kd = kd,
                                     .dt = dt,
                                     .out_min = FXP_MIN,
                                     .out_max = FXP_MAX,
                                     .integral_min = FXP_MIN,
                                     .integral_max = FXP_MAX,
                                     .d_filter_alpha =
                                         FPC_PID_D_FILTER_ALPHA_MAX};

        return cfg;
}

TEST_CASE(test_init_and_get_config)
{
        struct fpc_pid pid;
        struct fpc_pid_config cfg = test_config();
        struct fpc_pid_config copy;
        struct fpc_pid_state state;

        TEST_ASSERT(fpc_pid_init(&pid, &cfg) == FPC_STATUS_OK);
        TEST_ASSERT(fpc_pid_get_config(&pid, &copy) == FPC_STATUS_OK);
        TEST_ASSERT(copy.kp == cfg.kp);
        TEST_ASSERT(copy.ki == cfg.ki);
        TEST_ASSERT(copy.kd == cfg.kd);
        TEST_ASSERT(copy.dt == cfg.dt);
        TEST_ASSERT(copy.out_min == cfg.out_min);
        TEST_ASSERT(copy.out_max == cfg.out_max);
        TEST_ASSERT(copy.integral_min == cfg.integral_min);
        TEST_ASSERT(copy.integral_max == cfg.integral_max);
        TEST_ASSERT(copy.d_filter_alpha == cfg.d_filter_alpha);
        TEST_ASSERT(fpc_pid_get_state(&pid, &state) == FPC_STATUS_OK);
        TEST_ASSERT(state.integral == 0);
        TEST_ASSERT(state.last_output == 0);
        TEST_ASSERT(state.mode == FPC_PID_MODE_AUTO);
}

TEST_CASE(test_invalid_config_rejected)
{
        struct fpc_pid pid;
        struct fpc_pid_config cfg = test_config();

        TEST_ASSERT(fpc_pid_init(NULL, &cfg) == FPC_STATUS_NULL_PTR);
        TEST_ASSERT(fpc_pid_init(&pid, NULL) == FPC_STATUS_INVALID_PARAM);

        cfg.dt = 0;
        TEST_ASSERT(fpc_pid_init(&pid, &cfg) == FPC_STATUS_INVALID_PARAM);
        cfg.dt = -FXP_ONE;
        TEST_ASSERT(fpc_pid_init(&pid, &cfg) == FPC_STATUS_INVALID_PARAM);

        cfg = test_config();
        cfg.out_min = 10;
        cfg.out_max = 0;
        TEST_ASSERT(fpc_pid_init(&pid, &cfg) == FPC_STATUS_INVALID_PARAM);

        cfg = test_config();
        cfg.integral_min = 10;
        cfg.integral_max = 0;
        TEST_ASSERT(fpc_pid_init(&pid, &cfg) == FPC_STATUS_INVALID_PARAM);

        cfg = test_config();
        cfg.d_filter_alpha = FPC_PID_D_FILTER_ALPHA_MAX + 1;
        TEST_ASSERT(fpc_pid_init(&pid, &cfg) == FPC_STATUS_INVALID_PARAM);
        cfg.d_filter_alpha = -1;
        TEST_ASSERT(fpc_pid_init(&pid, &cfg) == FPC_STATUS_INVALID_PARAM);

        /* ki * dt saturates. */
        cfg = term_config(0, FXP_MAX, 0, FXP_INT(2));
        TEST_ASSERT(fpc_pid_init(&pid, &cfg) == FPC_STATUS_INVALID_PARAM);
        /* ki * dt rounds to zero. */
        cfg = term_config(0, FXP_RAW(1), 0, FXP_ONE / 4);
        TEST_ASSERT(fpc_pid_init(&pid, &cfg) == FPC_STATUS_INVALID_PARAM);
        /* kd / dt saturates. */
        cfg = term_config(0, 0, FXP_ONE, FXP_RAW(1));
        TEST_ASSERT(fpc_pid_init(&pid, &cfg) == FPC_STATUS_INVALID_PARAM);
        /* kd / dt rounds to zero. */
        cfg = term_config(0, 0, FXP_RAW(1), FXP_INT(4));
        TEST_ASSERT(fpc_pid_init(&pid, &cfg) == FPC_STATUS_INVALID_PARAM);
        TEST_ASSERT(fpc_pid_reset(&pid) == FPC_STATUS_NOT_INITIALIZED);

        /* Zero gains are valid at any dt. */
        cfg = term_config(0, 0, 0, FXP_RAW(1));
        TEST_ASSERT(fpc_pid_init(&pid, &cfg) == FPC_STATUS_OK);
}

TEST_CASE(test_dt_scales_i_and_d_only)
{
        struct fpc_pid pid;
        struct fpc_pid_config cfg;
        fxp_t output = 0;

        /* P ignores dt. */
        cfg = term_config(FXP_INT(2), 0, 0, FXP_INT(10));
        TEST_ASSERT(fpc_pid_init(&pid, &cfg) == FPC_STATUS_OK);
        TEST_ASSERT(fpc_pid_compute(&pid, FXP_INT(3), 0, &output)
                    == FPC_STATUS_OK);
        TEST_ASSERT(output == FXP_INT(6));

        /* I adds ki * dt * e per step. */
        cfg = term_config(0, FXP_ONE, 0, FXP_INT(2));
        TEST_ASSERT(fpc_pid_init(&pid, &cfg) == FPC_STATUS_OK);
        TEST_ASSERT(fpc_pid_compute(&pid, FXP_INT(3), 0, &output)
                    == FPC_STATUS_OK);
        TEST_ASSERT(output == FXP_INT(6));
        TEST_ASSERT(fpc_pid_compute(&pid, FXP_INT(3), 0, &output)
                    == FPC_STATUS_OK);
        TEST_ASSERT(output == FXP_INT(12));

        /* D is kd / dt times the change in error. */
        cfg = term_config(0, 0, FXP_INT(4), FXP_INT(2));
        TEST_ASSERT(fpc_pid_init(&pid, &cfg) == FPC_STATUS_OK);
        TEST_ASSERT(fpc_pid_compute(&pid, FXP_INT(3), 0, &output)
                    == FPC_STATUS_OK);
        TEST_ASSERT(output == FXP_INT(6));
        TEST_ASSERT(fpc_pid_compute(&pid, FXP_INT(3), 0, &output)
                    == FPC_STATUS_OK);
        TEST_ASSERT(output == 0);
}

TEST_CASE(test_integral_keeps_sub_lsb_contributions)
{
        struct fpc_pid pid;
        /* ki * dt = 60 LSB of gain; with e = 990 LSB each step adds
         * 59400 / 65536 = 0.906 LSB to the integral. */
        struct fpc_pid_config cfg = term_config(0, FXP_RAW(1), 0, FXP_INT(60));
        struct fpc_pid_state state;
        fxp_t output = 0;
        uint32_t i;

        TEST_ASSERT(fpc_pid_init(&pid, &cfg) == FPC_STATUS_OK);
        for (i = 0U; i < 100000U; ++i) {
                TEST_ASSERT(fpc_pid_compute(&pid, 990, 0, &output)
                            == FPC_STATUS_OK);
        }

        /* 100000 * 59400 / 65536 = 90637.2. Narrowing each step would give
         * 0 (truncate) or 100000 (round to nearest). */
        TEST_ASSERT(fpc_pid_get_state(&pid, &state) == FPC_STATUS_OK);
        TEST_ASSERT(state.integral == 90637);
        TEST_ASSERT(output == 90637);
}

TEST_CASE(test_anti_windup_clamps_accumulator)
{
        struct fpc_pid pid;
        struct fpc_pid_config cfg = term_config(0, FXP_ONE, 0, FXP_ONE);
        struct fpc_pid_state state;
        fxp_t output = 0;
        uint32_t i;

        cfg.integral_min = -5;
        cfg.integral_max = 5;
        TEST_ASSERT(fpc_pid_init(&pid, &cfg) == FPC_STATUS_OK);
        for (i = 0U; i < 10U; ++i) {
                TEST_ASSERT(fpc_pid_compute(&pid, 10, 0, &output)
                            == FPC_STATUS_SATURATED);
        }
        TEST_ASSERT(output == 5);

        /* No stored windup: one negative step leaves the bound at once. */
        TEST_ASSERT(fpc_pid_compute(&pid, -1, 0, &output) == FPC_STATUS_OK);
        TEST_ASSERT(output == 4);
        TEST_ASSERT(fpc_pid_get_state(&pid, &state) == FPC_STATUS_OK);
        TEST_ASSERT(state.integral == 4);
}

TEST_CASE(test_saturation_reporting)
{
        struct fpc_pid pid;
        struct fpc_pid_config cfg = test_config();
        fxp_t output = 0;

        TEST_ASSERT(fpc_pid_init(&pid, &cfg) == FPC_STATUS_OK);
        TEST_ASSERT(fpc_pid_compute(&pid, 10000, 0, &output)
                    == FPC_STATUS_SATURATED);
        TEST_ASSERT(output == 100);
        TEST_ASSERT(fpc_pid_compute(&pid, -10000, 0, &output)
                    == FPC_STATUS_SATURATED);
        TEST_ASSERT(output == -100);
}

TEST_CASE(test_extreme_inputs_saturate)
{
        struct fpc_pid pid;
        struct fpc_pid_config cfg = term_config(FXP_MIN, 0, 0, FXP_ONE);
        struct fpc_pid_state state;
        fxp_t output = 0;

        /* The error saturates instead of holding the last output. */
        TEST_ASSERT(fpc_pid_init(&pid, &cfg) == FPC_STATUS_OK);
        TEST_ASSERT(fpc_pid_compute(&pid, FXP_MAX, FXP_MIN, &output)
                    == FPC_STATUS_SATURATED);
        TEST_ASSERT(output == FXP_MIN);
        TEST_ASSERT(fpc_pid_get_state(&pid, &state) == FPC_STATUS_OK);
        TEST_ASSERT(state.prev_error == FXP_MAX);

        /* kp * e = 2^62 narrows out of range. */
        TEST_ASSERT(fpc_pid_compute(&pid, FXP_MIN, 0, &output)
                    == FPC_STATUS_SATURATED);
        TEST_ASSERT(output == FXP_MAX);

        /* The change in error and the raw derivative saturate. */
        cfg = term_config(0, 0, FXP_MIN, FXP_ONE);
        TEST_ASSERT(fpc_pid_init(&pid, &cfg) == FPC_STATUS_OK);
        TEST_ASSERT(fpc_pid_compute(&pid, FXP_MAX, 0, &output)
                    == FPC_STATUS_SATURATED);
        TEST_ASSERT(output == FXP_MIN);
        TEST_ASSERT(fpc_pid_compute(&pid, FXP_MIN, 0, &output)
                    == FPC_STATUS_SATURATED);
        TEST_ASSERT(output == FXP_MAX);
}

TEST_CASE(test_state_retrieval)
{
        struct fpc_pid pid;
        struct fpc_pid_config cfg = wide_config();
        struct fpc_pid_state state;
        fxp_t output = 0;

        /* P = 500, I = 250, D = 0.2 * 500 = 99.998 -> 100. */
        TEST_ASSERT(fpc_pid_init(&pid, &cfg) == FPC_STATUS_OK);
        TEST_ASSERT(fpc_pid_compute(&pid, 1000, 500, &output) == FPC_STATUS_OK);
        TEST_ASSERT(output == 850);
        TEST_ASSERT(fpc_pid_get_state(&pid, &state) == FPC_STATUS_OK);
        TEST_ASSERT(state.integral == 250);
        TEST_ASSERT(state.prev_error == 500);
        TEST_ASSERT(state.filtered_derivative == 100);
        TEST_ASSERT(state.last_output == 850);
        TEST_ASSERT(state.mode == FPC_PID_MODE_AUTO);
}

TEST_CASE(test_derivative_filtering)
{
        struct fpc_pid pid;
        struct fpc_pid_config cfg = term_config(0, 0, FXP_ONE, FXP_ONE);
        struct fpc_pid_state state;
        fxp_t output = 0;

        cfg.d_filter_alpha = FXP_ONE / 2;
        TEST_ASSERT(fpc_pid_init(&pid, &cfg) == FPC_STATUS_OK);
        TEST_ASSERT(fpc_pid_compute(&pid, 1000, 0, &output) == FPC_STATUS_OK);
        TEST_ASSERT(output == 500);
        TEST_ASSERT(fpc_pid_get_state(&pid, &state) == FPC_STATUS_OK);
        TEST_ASSERT(state.filtered_derivative == 500);
        TEST_ASSERT(fpc_pid_compute(&pid, 1000, 0, &output) == FPC_STATUS_OK);
        TEST_ASSERT(output == 250);
        TEST_ASSERT(fpc_pid_get_state(&pid, &state) == FPC_STATUS_OK);
        TEST_ASSERT(state.filtered_derivative == 250);
}

TEST_CASE(test_reset_clears_runtime_state)
{
        struct fpc_pid pid;
        struct fpc_pid_config cfg = test_config();
        struct fpc_pid_state state;
        fxp_t output = 0;

        TEST_ASSERT(fpc_pid_init(&pid, &cfg) == FPC_STATUS_OK);
        TEST_ASSERT(fpc_pid_compute(&pid, 1000, 0, &output)
                    == FPC_STATUS_SATURATED);
        TEST_ASSERT(fpc_pid_reset(&pid) == FPC_STATUS_OK);
        TEST_ASSERT(fpc_pid_get_state(&pid, &state) == FPC_STATUS_OK);
        TEST_ASSERT(state.integral == 0);
        TEST_ASSERT(state.prev_error == 0);
        TEST_ASSERT(state.filtered_derivative == 0);
        TEST_ASSERT(state.last_output == 0);

        /* In manual mode the held output survives a reset. */
        TEST_ASSERT(fpc_pid_set_mode(&pid, FPC_PID_MODE_MANUAL, 30)
                    == FPC_STATUS_OK);
        TEST_ASSERT(fpc_pid_reset(&pid) == FPC_STATUS_OK);
        TEST_ASSERT(fpc_pid_get_state(&pid, &state) == FPC_STATUS_OK);
        TEST_ASSERT(state.last_output == 30);
}

TEST_CASE(test_set_config_clamps_state)
{
        struct fpc_pid pid;
        struct fpc_pid_config cfg = test_config();
        struct fpc_pid_config tighter_cfg = test_config();
        struct fpc_pid_config bad_cfg = test_config();
        struct fpc_pid_state state;
        fxp_t output = 0;

        tighter_cfg.out_min = -10;
        tighter_cfg.out_max = 10;
        tighter_cfg.integral_min = -5;
        tighter_cfg.integral_max = 5;
        bad_cfg.dt = 0;

        TEST_ASSERT(fpc_pid_init(&pid, &cfg) == FPC_STATUS_OK);
        TEST_ASSERT(fpc_pid_compute(&pid, 1000, 0, &output)
                    == FPC_STATUS_SATURATED);
        TEST_ASSERT(fpc_pid_set_config(&pid, &bad_cfg)
                    == FPC_STATUS_INVALID_PARAM);
        TEST_ASSERT(fpc_pid_set_config(&pid, NULL) == FPC_STATUS_INVALID_PARAM);
        TEST_ASSERT(fpc_pid_get_state(&pid, &state) == FPC_STATUS_OK);
        TEST_ASSERT(state.integral == 50);
        TEST_ASSERT(state.last_output == 100);

        TEST_ASSERT(fpc_pid_set_config(&pid, &tighter_cfg)
                    == FPC_STATUS_SATURATED);
        TEST_ASSERT(fpc_pid_get_state(&pid, &state) == FPC_STATUS_OK);
        TEST_ASSERT(state.integral == 5);
        TEST_ASSERT(state.last_output == 10);
        TEST_ASSERT(fpc_pid_set_config(&pid, &tighter_cfg) == FPC_STATUS_OK);

        /* A held manual output is clamped to the new bounds. */
        TEST_ASSERT(fpc_pid_set_config(&pid, &cfg) == FPC_STATUS_OK);
        TEST_ASSERT(fpc_pid_set_mode(&pid, FPC_PID_MODE_MANUAL, 80)
                    == FPC_STATUS_OK);
        TEST_ASSERT(fpc_pid_set_config(&pid, &tighter_cfg)
                    == FPC_STATUS_SATURATED);
        TEST_ASSERT(fpc_pid_compute(&pid, 0, 0, &output) == FPC_STATUS_OK);
        TEST_ASSERT(output == 10);
}

TEST_CASE(test_manual_mode_and_bumpless_return)
{
        struct fpc_pid pid;
        struct fpc_pid_config cfg = wide_config();
        struct fpc_pid_state state;
        fxp_t output = 0;

        cfg.ki = 0;
        cfg.kd = 0;

        TEST_ASSERT(fpc_pid_init(&pid, &cfg) == FPC_STATUS_OK);
        TEST_ASSERT(fpc_pid_set_mode(&pid, FPC_PID_MODE_MANUAL, 300)
                    == FPC_STATUS_OK);
        TEST_ASSERT(fpc_pid_compute(&pid, 1000, 900, &output) == FPC_STATUS_OK);
        TEST_ASSERT(output == 300);
        TEST_ASSERT(fpc_pid_get_state(&pid, &state) == FPC_STATUS_OK);
        TEST_ASSERT(state.mode == FPC_PID_MODE_MANUAL);
        TEST_ASSERT(state.prev_error == 100);

        TEST_ASSERT(fpc_pid_set_mode(&pid, FPC_PID_MODE_AUTO, 0)
                    == FPC_STATUS_OK);
        TEST_ASSERT(fpc_pid_get_state(&pid, &state) == FPC_STATUS_OK);
        TEST_ASSERT(state.mode == FPC_PID_MODE_AUTO);
        TEST_ASSERT(state.integral == 200);
        TEST_ASSERT(fpc_pid_compute(&pid, 1000, 900, &output) == FPC_STATUS_OK);
        TEST_ASSERT(output == 300);

        /* Auto to auto changes nothing. */
        TEST_ASSERT(fpc_pid_set_mode(&pid, FPC_PID_MODE_AUTO, 0)
                    == FPC_STATUS_OK);
        TEST_ASSERT(fpc_pid_get_state(&pid, &state) == FPC_STATUS_OK);
        TEST_ASSERT(state.integral == 200);
}

TEST_CASE(test_bumpless_rebias_clamps)
{
        struct fpc_pid pid;
        struct fpc_pid_config cfg = test_config();
        struct fpc_pid_state state;
        fxp_t output = 0;

        /* Re-biasing needs I = 90 - 10 = 80, above integral_max = 50. */
        TEST_ASSERT(fpc_pid_init(&pid, &cfg) == FPC_STATUS_OK);
        TEST_ASSERT(fpc_pid_set_mode(&pid, FPC_PID_MODE_MANUAL, 90)
                    == FPC_STATUS_OK);
        TEST_ASSERT(fpc_pid_compute(&pid, 10, 0, &output) == FPC_STATUS_OK);
        TEST_ASSERT(fpc_pid_set_mode(&pid, FPC_PID_MODE_AUTO, 0)
                    == FPC_STATUS_SATURATED);
        TEST_ASSERT(fpc_pid_get_state(&pid, &state) == FPC_STATUS_OK);
        TEST_ASSERT(state.integral == 50);
}

TEST_CASE(test_manual_mode_clamps_output)
{
        struct fpc_pid pid;
        struct fpc_pid_config cfg = test_config();
        fxp_t output = 0;

        TEST_ASSERT(fpc_pid_init(&pid, &cfg) == FPC_STATUS_OK);
        TEST_ASSERT(fpc_pid_set_mode(&pid, FPC_PID_MODE_MANUAL, 1000)
                    == FPC_STATUS_SATURATED);
        TEST_ASSERT(fpc_pid_compute(&pid, 0, 0, &output) == FPC_STATUS_OK);
        TEST_ASSERT(output == 100);
        TEST_ASSERT(fpc_pid_compute(&pid, FXP_MAX, FXP_MIN, &output)
                    == FPC_STATUS_SATURATED);
        TEST_ASSERT(output == 100);
}

TEST_CASE(test_null_and_invalid_runtime_calls)
{
        struct fpc_pid pid = {0};
        struct fpc_pid_config cfg = test_config();
        struct fpc_pid_config copy;
        struct fpc_pid_state state;
        fxp_t output = 0;

        TEST_ASSERT(fpc_pid_reset(NULL) == FPC_STATUS_NULL_PTR);
        TEST_ASSERT(fpc_pid_set_config(NULL, &cfg) == FPC_STATUS_NULL_PTR);
        TEST_ASSERT(fpc_pid_get_config(NULL, &copy) == FPC_STATUS_NULL_PTR);
        TEST_ASSERT(fpc_pid_set_mode(NULL, FPC_PID_MODE_MANUAL, 0)
                    == FPC_STATUS_NULL_PTR);
        TEST_ASSERT(fpc_pid_get_state(NULL, &state) == FPC_STATUS_NULL_PTR);
        TEST_ASSERT(fpc_pid_compute(NULL, 0, 0, &output)
                    == FPC_STATUS_NULL_PTR);

        /* A zeroed instance is not initialised. */
        TEST_ASSERT(fpc_pid_reset(&pid) == FPC_STATUS_NOT_INITIALIZED);
        TEST_ASSERT(fpc_pid_set_config(&pid, &cfg)
                    == FPC_STATUS_NOT_INITIALIZED);
        TEST_ASSERT(fpc_pid_get_config(&pid, &copy)
                    == FPC_STATUS_NOT_INITIALIZED);
        TEST_ASSERT(fpc_pid_set_mode(&pid, FPC_PID_MODE_AUTO, 0)
                    == FPC_STATUS_NOT_INITIALIZED);
        TEST_ASSERT(fpc_pid_get_state(&pid, &state)
                    == FPC_STATUS_NOT_INITIALIZED);
        TEST_ASSERT(fpc_pid_compute(&pid, 1000, 0, &output)
                    == FPC_STATUS_NOT_INITIALIZED);

        TEST_ASSERT(fpc_pid_init(&pid, &cfg) == FPC_STATUS_OK);
        TEST_ASSERT(fpc_pid_get_config(&pid, NULL) == FPC_STATUS_NULL_PTR);
        TEST_ASSERT(fpc_pid_get_state(&pid, NULL) == FPC_STATUS_NULL_PTR);
        TEST_ASSERT(fpc_pid_compute(&pid, 0, 0, NULL) == FPC_STATUS_NULL_PTR);
        TEST_ASSERT(fpc_pid_set_mode(&pid, (enum fpc_pid_mode)2, 0)
                    == FPC_STATUS_INVALID_PARAM);
}

void
run_pid_tests(void)
{
        run_test(test_init_and_get_config, "init_and_get_config");
        run_test(test_invalid_config_rejected, "invalid_config_rejected");
        run_test(test_dt_scales_i_and_d_only, "dt_scales_i_and_d_only");
        run_test(test_integral_keeps_sub_lsb_contributions,
                 "integral_keeps_sub_lsb_contributions");
        run_test(test_anti_windup_clamps_accumulator,
                 "anti_windup_clamps_accumulator");
        run_test(test_saturation_reporting, "saturation_reporting");
        run_test(test_extreme_inputs_saturate, "extreme_inputs_saturate");
        run_test(test_state_retrieval, "state_retrieval");
        run_test(test_derivative_filtering, "derivative_filtering");
        run_test(test_reset_clears_runtime_state, "reset_clears_runtime_state");
        run_test(test_set_config_clamps_state, "set_config_clamps_state");
        run_test(test_manual_mode_and_bumpless_return,
                 "manual_mode_and_bumpless_return");
        run_test(test_bumpless_rebias_clamps, "bumpless_rebias_clamps");
        run_test(test_manual_mode_clamps_output, "manual_mode_clamps_output");
        run_test(test_null_and_invalid_runtime_calls,
                 "null_and_invalid_runtime_calls");
}
