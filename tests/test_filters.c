/* SPDX-License-Identifier: MIT */

#include "../include/fpc_filters.h"
#include "test_harness.h"
#include <stddef.h>
#include <stdint.h>

/* Samples are raw fxp_t words. Expected outputs are exact under the default
 * round-to-nearest-even policy. */

#define FIR_THIRD ((fxp_t)21845) /* 0.333328 in Q16.16 */

static const fxp_t average_coeffs[3] = {FIR_THIRD, FIR_THIRD, FIR_THIRD};

static struct fpc_fir_config
fir_average_config(fxp_t *history)
{
        struct fpc_fir_config cfg = {
            .coeffs = average_coeffs, .history = history, .order = 3U};

        return cfg;
}

TEST_CASE(test_fir_init_and_process)
{
        fxp_t history[3];
        struct fpc_fir fir;
        struct fpc_fir_config cfg = fir_average_config(history);
        fxp_t output = 0;

        TEST_ASSERT(fpc_fir_init(&fir, &cfg) == FPC_STATUS_OK);
        TEST_ASSERT(fpc_fir_process(&fir, 1000, &output) == FPC_STATUS_OK);
        TEST_ASSERT(output == 333); /* 333.328 */
        TEST_ASSERT(fpc_fir_process(&fir, 2000, &output) == FPC_STATUS_OK);
        TEST_ASSERT(output == 1000); /* 999.985 */
        TEST_ASSERT(fpc_fir_process(&fir, 2000, &output) == FPC_STATUS_OK);
        TEST_ASSERT(output == 1667); /* 1666.641 */
}

TEST_CASE(test_fir_tap_order)
{
        static const fxp_t coeffs[3] = {FXP_ONE, 2 * FXP_ONE, 3 * FXP_ONE};
        static const fxp_t samples[5] = {1, 10, 100, 1000, 10000};
        static const fxp_t expected[5] = {1, 12, 123, 1230, 12300};
        fxp_t history[3];
        struct fpc_fir fir;
        struct fpc_fir_config cfg = {
            .coeffs = coeffs, .history = history, .order = 3U};
        fxp_t output = 0;
        size_t i;

        /* h[0] weights the newest sample, across several buffer wraps. */
        TEST_ASSERT(fpc_fir_init(&fir, &cfg) == FPC_STATUS_OK);
        for (i = 0U; i < 5U; ++i) {
                TEST_ASSERT(fpc_fir_process(&fir, samples[i], &output)
                            == FPC_STATUS_OK);
                TEST_ASSERT(output == expected[i]);
        }
}

TEST_CASE(test_fir_reset_and_reconfigure)
{
        static const fxp_t pass_coeffs[1] = {FXP_ONE};
        fxp_t history[3];
        fxp_t pass_history[1];
        struct fpc_fir fir;
        struct fpc_fir_config cfg = fir_average_config(history);
        struct fpc_fir_config pass_cfg = {
            .coeffs = pass_coeffs, .history = pass_history, .order = 1U};
        fxp_t output = 0;

        TEST_ASSERT(fpc_fir_init(&fir, &cfg) == FPC_STATUS_OK);
        TEST_ASSERT(fpc_fir_process(&fir, 900, &output) == FPC_STATUS_OK);
        TEST_ASSERT(fpc_fir_reset(&fir) == FPC_STATUS_OK);
        TEST_ASSERT(fpc_fir_process(&fir, 900, &output) == FPC_STATUS_OK);
        TEST_ASSERT(output == 300); /* 299.995 */
        TEST_ASSERT(fpc_fir_set_config(&fir, &pass_cfg) == FPC_STATUS_OK);
        TEST_ASSERT(fpc_fir_process(&fir, 1234, &output) == FPC_STATUS_OK);
        TEST_ASSERT(output == 1234);
}

TEST_CASE(test_fir_invalid_usage)
{
        fxp_t history[3];
        struct fpc_fir fir = {0};
        struct fpc_fir_config cfg = fir_average_config(history);
        struct fpc_fir_config bad = cfg;
        fxp_t output = 7;

        /* A zeroed instance is not initialised. */
        TEST_ASSERT(fpc_fir_process(&fir, 0, &output)
                    == FPC_STATUS_NOT_INITIALIZED);
        TEST_ASSERT(fpc_fir_reset(&fir) == FPC_STATUS_NOT_INITIALIZED);
        TEST_ASSERT(fpc_fir_set_config(&fir, &cfg)
                    == FPC_STATUS_NOT_INITIALIZED);

        TEST_ASSERT(fpc_fir_init(NULL, &cfg) == FPC_STATUS_NULL_PTR);
        TEST_ASSERT(fpc_fir_init(&fir, NULL) == FPC_STATUS_INVALID_PARAM);
        bad.coeffs = NULL;
        TEST_ASSERT(fpc_fir_init(&fir, &bad) == FPC_STATUS_INVALID_PARAM);
        bad = cfg;
        bad.history = NULL;
        TEST_ASSERT(fpc_fir_init(&fir, &bad) == FPC_STATUS_INVALID_PARAM);
        bad = cfg;
        bad.order = 0U;
        TEST_ASSERT(fpc_fir_init(&fir, &bad) == FPC_STATUS_INVALID_PARAM);
        TEST_ASSERT(fpc_fir_process(&fir, 0, &output)
                    == FPC_STATUS_NOT_INITIALIZED);

        /* A failed init leaves a previously valid instance unusable. */
        TEST_ASSERT(fpc_fir_init(&fir, &cfg) == FPC_STATUS_OK);
        TEST_ASSERT(fpc_fir_init(&fir, &bad) == FPC_STATUS_INVALID_PARAM);
        TEST_ASSERT(fpc_fir_reset(&fir) == FPC_STATUS_NOT_INITIALIZED);

        /* A rejected configuration leaves the filter unchanged. */
        TEST_ASSERT(fpc_fir_init(&fir, &cfg) == FPC_STATUS_OK);
        TEST_ASSERT(fpc_fir_process(&fir, 1000, &output) == FPC_STATUS_OK);
        TEST_ASSERT(fpc_fir_set_config(&fir, NULL) == FPC_STATUS_INVALID_PARAM);
        TEST_ASSERT(fpc_fir_set_config(&fir, &bad) == FPC_STATUS_INVALID_PARAM);
        TEST_ASSERT(fpc_fir_process(&fir, 2000, &output) == FPC_STATUS_OK);
        TEST_ASSERT(output == 1000);

        TEST_ASSERT(fpc_fir_reset(NULL) == FPC_STATUS_NULL_PTR);
        TEST_ASSERT(fpc_fir_set_config(NULL, &cfg) == FPC_STATUS_NULL_PTR);
        TEST_ASSERT(fpc_fir_process(NULL, 0, &output) == FPC_STATUS_NULL_PTR);
        TEST_ASSERT(fpc_fir_process(&fir, 0, NULL) == FPC_STATUS_NULL_PTR);
}

TEST_CASE(test_fir_rounds_negative_to_nearest)
{
        static const fxp_t half[1] = {FXP_ONE / 2};
        fxp_t history[1];
        struct fpc_fir fir;
        struct fpc_fir_config cfg = {
            .coeffs = half, .history = history, .order = 1U};
        fxp_t output = 0;

        /* An arithmetic right shift would floor these to -3 and -1. */
        TEST_ASSERT(fpc_fir_init(&fir, &cfg) == FPC_STATUS_OK);
        TEST_ASSERT(fpc_fir_process(&fir, -5, &output) == FPC_STATUS_OK);
        TEST_ASSERT(output == -2); /* -2.5 */
        TEST_ASSERT(fpc_fir_process(&fir, -1, &output) == FPC_STATUS_OK);
        TEST_ASSERT(output == 0); /* -0.5 */
}

TEST_CASE(test_fir_saturates)
{
        static const fxp_t max_coeff[1] = {FXP_MAX};
        static const fxp_t min_coeffs[3] = {FXP_MIN, FXP_MIN, FXP_MIN};
        fxp_t history[3];
        struct fpc_fir fir;
        struct fpc_fir_config one = {
            .coeffs = max_coeff, .history = history, .order = 1U};
        struct fpc_fir_config three = {
            .coeffs = min_coeffs, .history = history, .order = 3U};
        fxp_t output = 0;

        /* The narrowed sum exceeds the fxp_t range. */
        TEST_ASSERT(fpc_fir_init(&fir, &one) == FPC_STATUS_OK);
        TEST_ASSERT(fpc_fir_process(&fir, FXP_MAX, &output)
                    == FPC_STATUS_SATURATED);
        TEST_ASSERT(output == FXP_MAX);
        TEST_ASSERT(fpc_fir_process(&fir, FXP_MIN, &output)
                    == FPC_STATUS_SATURATED);
        TEST_ASSERT(output == FXP_MIN);

        /* Two products of 2^62 overflow the 64-bit accumulator itself. */
        TEST_ASSERT(fpc_fir_init(&fir, &three) == FPC_STATUS_OK);
        TEST_ASSERT(fpc_fir_process(&fir, FXP_MIN, &output)
                    == FPC_STATUS_SATURATED);
        TEST_ASSERT(fpc_fir_process(&fir, FXP_MIN, &output)
                    == FPC_STATUS_SATURATED);
        TEST_ASSERT(output == FXP_MAX);
        TEST_ASSERT(fpc_fir_process(&fir, FXP_MIN, &output)
                    == FPC_STATUS_SATURATED);
        TEST_ASSERT(output == FXP_MAX);
        TEST_ASSERT(fpc_fir_reset(&fir) == FPC_STATUS_OK);
        TEST_ASSERT(fpc_fir_process(&fir, FXP_MAX, &output)
                    == FPC_STATUS_SATURATED);
        TEST_ASSERT(fpc_fir_process(&fir, FXP_MAX, &output)
                    == FPC_STATUS_SATURATED);
        TEST_ASSERT(fpc_fir_process(&fir, FXP_MAX, &output)
                    == FPC_STATUS_SATURATED);
        TEST_ASSERT(output == FXP_MIN);
}

TEST_CASE(test_biquad_identity_and_reset)
{
        struct fpc_biquad bq;
        struct fpc_biquad_config cfg = {.b0 = FXP_ONE};
        fxp_t output = 0;

        TEST_ASSERT(fpc_biquad_init(&bq, &cfg) == FPC_STATUS_OK);
        TEST_ASSERT(fpc_biquad_process(&bq, 1000, &output) == FPC_STATUS_OK);
        TEST_ASSERT(output == 1000);
        TEST_ASSERT(fpc_biquad_reset(&bq) == FPC_STATUS_OK);
        TEST_ASSERT(fpc_biquad_process(&bq, 2000, &output) == FPC_STATUS_OK);
        TEST_ASSERT(output == 2000);
}

TEST_CASE(test_biquad_difference_equation)
{
        struct fpc_biquad bq;
        struct fpc_biquad_config identity = {.b0 = FXP_ONE};
        struct fpc_biquad_config lowpass = {
            .b0 = FXP_ONE / 2, .b1 = FXP_ONE / 2, .a1 = FXP_ONE / 2};
        struct fpc_biquad_config all_taps = {.b0 = FXP_ONE,
                                             .b1 = 2 * FXP_ONE,
                                             .b2 = 3 * FXP_ONE,
                                             .a1 = -FXP_ONE,
                                             .a2 = FXP_ONE / 4};
        fxp_t output = 0;

        TEST_ASSERT(fpc_biquad_init(&bq, &identity) == FPC_STATUS_OK);
        TEST_ASSERT(fpc_biquad_set_config(&bq, &lowpass) == FPC_STATUS_OK);
        TEST_ASSERT(fpc_biquad_process(&bq, 1000, &output) == FPC_STATUS_OK);
        TEST_ASSERT(output == 500);
        TEST_ASSERT(fpc_biquad_process(&bq, 1000, &output) == FPC_STATUS_OK);
        TEST_ASSERT(output == 750); /* 500 + 500 - 250 */

        /* y = x + 2x1 + 3x2 + y1 - y2/4, starting from cleared state. */
        TEST_ASSERT(fpc_biquad_set_config(&bq, &all_taps) == FPC_STATUS_OK);
        TEST_ASSERT(fpc_biquad_process(&bq, 4, &output) == FPC_STATUS_OK);
        TEST_ASSERT(output == 4);
        TEST_ASSERT(fpc_biquad_process(&bq, 0, &output) == FPC_STATUS_OK);
        TEST_ASSERT(output == 12); /* 8 + 4 */
        TEST_ASSERT(fpc_biquad_process(&bq, 0, &output) == FPC_STATUS_OK);
        TEST_ASSERT(output == 23); /* 12 + 12 - 1 */
}

TEST_CASE(test_biquad_invalid_usage)
{
        struct fpc_biquad bq;
        struct fpc_biquad_config cfg = {.b0 = FXP_ONE};
        struct fpc_biquad_config bad_a1 = {.b0 = FXP_ONE, .a1 = FXP_MIN};
        struct fpc_biquad_config bad_a2 = {.b0 = FXP_ONE, .a2 = FXP_MIN};
        fxp_t output = 7;

        TEST_ASSERT(fpc_biquad_init(NULL, &cfg) == FPC_STATUS_NULL_PTR);
        TEST_ASSERT(fpc_biquad_init(&bq, NULL) == FPC_STATUS_INVALID_PARAM);
        TEST_ASSERT(fpc_biquad_init(&bq, &bad_a1) == FPC_STATUS_INVALID_PARAM);

        /* A failed init leaves the zero filter. */
        TEST_ASSERT(fpc_biquad_process(&bq, 1000, &output) == FPC_STATUS_OK);
        TEST_ASSERT(output == 0);

        /* A rejected configuration leaves the filter unchanged. */
        TEST_ASSERT(fpc_biquad_init(&bq, &cfg) == FPC_STATUS_OK);
        TEST_ASSERT(fpc_biquad_set_config(&bq, &bad_a2)
                    == FPC_STATUS_INVALID_PARAM);
        TEST_ASSERT(fpc_biquad_process(&bq, 1000, &output) == FPC_STATUS_OK);
        TEST_ASSERT(output == 1000);

        TEST_ASSERT(fpc_biquad_reset(NULL) == FPC_STATUS_NULL_PTR);
        TEST_ASSERT(fpc_biquad_set_config(NULL, &cfg) == FPC_STATUS_NULL_PTR);
        TEST_ASSERT(fpc_biquad_set_config(&bq, NULL)
                    == FPC_STATUS_INVALID_PARAM);
        TEST_ASSERT(fpc_biquad_process(NULL, 0, &output)
                    == FPC_STATUS_NULL_PTR);
        TEST_ASSERT(fpc_biquad_process(&bq, 0, NULL) == FPC_STATUS_NULL_PTR);
}

TEST_CASE(test_biquad_zero_instance)
{
        struct fpc_biquad bq = {0};
        fxp_t output = 7;

        TEST_ASSERT(fpc_biquad_process(&bq, FXP_MAX, &output) == FPC_STATUS_OK);
        TEST_ASSERT(output == 0);
}

TEST_CASE(test_biquad_saturates)
{
        struct fpc_biquad bq;
        struct fpc_biquad_config max_gain = {.b0 = FXP_MAX};
        struct fpc_biquad_config min_taps = {
            .b0 = FXP_MIN, .b1 = FXP_MIN, .b2 = FXP_MIN};
        struct fpc_biquad_config feedback = {.b0 = FXP_ONE, .a1 = FXP_MIN + 1};
        fxp_t output = 0;

        TEST_ASSERT(fpc_biquad_init(&bq, &max_gain) == FPC_STATUS_OK);
        TEST_ASSERT(fpc_biquad_process(&bq, FXP_MAX, &output)
                    == FPC_STATUS_SATURATED);
        TEST_ASSERT(output == FXP_MAX);

        /* Three products of 2^62 overflow the 64-bit accumulator. */
        TEST_ASSERT(fpc_biquad_init(&bq, &min_taps) == FPC_STATUS_OK);
        TEST_ASSERT(fpc_biquad_process(&bq, FXP_MIN, &output)
                    == FPC_STATUS_SATURATED);
        TEST_ASSERT(fpc_biquad_process(&bq, FXP_MIN, &output)
                    == FPC_STATUS_SATURATED);
        TEST_ASSERT(fpc_biquad_process(&bq, FXP_MIN, &output)
                    == FPC_STATUS_SATURATED);
        TEST_ASSERT(output == FXP_MAX);

        /* The largest negatable a1 feeds a saturated output back. */
        TEST_ASSERT(fpc_biquad_init(&bq, &feedback) == FPC_STATUS_OK);
        TEST_ASSERT(fpc_biquad_process(&bq, 2 * FXP_ONE, &output)
                    == FPC_STATUS_OK);
        TEST_ASSERT(output == 2 * FXP_ONE);
        TEST_ASSERT(fpc_biquad_process(&bq, 0, &output)
                    == FPC_STATUS_SATURATED);
        TEST_ASSERT(output == FXP_MAX);
}

/* The version number agrees with its parts. */
TEST_CASE(test_version_macros)
{
        TEST_ASSERT(FPC_VERSION_NUMBER
                    == ((FPC_VERSION_MAJOR * 10000L)
                        + (FPC_VERSION_MINOR * 100L) + FPC_VERSION_PATCH));
        TEST_ASSERT(FPC_VERSION[0] != '\0');
}

void
run_filters_tests(void)
{
        run_test(test_fir_init_and_process, "fir_init_and_process");
        run_test(test_fir_tap_order, "fir_tap_order");
        run_test(test_fir_reset_and_reconfigure, "fir_reset_and_reconfigure");
        run_test(test_fir_invalid_usage, "fir_invalid_usage");
        run_test(test_fir_rounds_negative_to_nearest,
                 "fir_rounds_negative_to_nearest");
        run_test(test_fir_saturates, "fir_saturates");
        run_test(test_biquad_identity_and_reset, "biquad_identity_and_reset");
        run_test(test_biquad_difference_equation, "biquad_difference_equation");
        run_test(test_biquad_invalid_usage, "biquad_invalid_usage");
        run_test(test_biquad_zero_instance, "biquad_zero_instance");
        run_test(test_biquad_saturates, "biquad_saturates");
        run_test(test_version_macros, "version_macros");
}
