/**
 * SPDX-License-Identifier: MIT
 *
 * @file: fpc_filters.h
 *
 * @brief
 *    Safety-oriented FIR and biquad filter APIs.
 *
 * This module provides fixed-point implementations of:
 *   - Finite Impulse Response (FIR) filters
 *   - Biquad (second-order IIR) filters
 *
 * Coefficients and samples are @c fxp_t values. Each output is accumulated in
 * a wide @c fxp_acc_t and narrowed once using the build-wide fxp rounding
 * policy; a result outside the @c fxp_t range saturates and is reported.
 *
 * @par Storage
 *    Filter instances are caller-owned. Declare a @c struct fpc_fir or
 *    @c struct fpc_biquad with any storage duration and pass its address to
 *    the init function. Their members are private to the library.
 *
 * @par Threading contract
 *    An instance is not internally synchronised. Calls on the same instance
 *    must be serialised by the caller; distinct instances are independent.
 */

#ifndef FPC_FILTERS_H_
#define FPC_FILTERS_H_

#include "fpc_status.h"
#include "fxp.h"
#include <stdint.h>

/**
 * @struct fpc_fir_config
 * @brief FIR filter configuration.
 *
 * A FIR filter computes the output as the weighted sum of recent input samples:
 *
 * \f[
 * y[k] = \sum_{i=0}^{N-1} h[i] \cdot x[k-i]
 * \f]
 *
 * where \f$ N \f$ is the filter order (number of taps), \f$ h[i] \f$ are the
 * coefficients and \f$ x[k] \f$ is the input at time step \f$ k \f$.
 *
 * Both arrays are borrowed, not copied: they must hold @c order elements, stay
 * valid while the filter uses them, and must not overlap. The library writes
 * only to @c history. @c coeffs[0] weights the newest sample.
 */
struct fpc_fir_config {
        const fxp_t *coeffs; /**< Coefficients h[0..order-1]. */
        fxp_t *history;      /**< Caller-owned input history buffer. */
        uint16_t order;      /**< Number of taps (at least 1). */
};

/**
 * @struct fpc_fir
 * @brief Caller-owned FIR filter instance. Members are private.
 */
struct fpc_fir {
        const fxp_t *coeffs; /**< @private */
        fxp_t *history;      /**< @private */
        uint16_t order;      /**< @private 0 until initialised. */
        uint16_t head;       /**< @private Index of the newest sample. */
};

/**
 * @struct fpc_biquad_config
 * @brief Biquad (second-order IIR) filter coefficients.
 *
 * The biquad filter implements the direct form I difference equation:
 *
 * \f[
 * y[k] = b_0 \cdot x[k] + b_1 \cdot x[k-1] + b_2 \cdot x[k-2]
 *       - a_1 \cdot y[k-1] - a_2 \cdot y[k-2]
 * \f]
 *
 * @note @c a1 and @c a2 must not be @c FXP_MIN, so that their negation is
 *       exact. No stable biquad needs a coefficient at that bound.
 */
struct fpc_biquad_config {
        fxp_t b0; /**< Feedforward coefficient for x[k]. */
        fxp_t b1; /**< Feedforward coefficient for x[k-1]. */
        fxp_t b2; /**< Feedforward coefficient for x[k-2]. */
        fxp_t a1; /**< Feedback coefficient for y[k-1]. */
        fxp_t a2; /**< Feedback coefficient for y[k-2]. */
};

/**
 * @struct fpc_biquad
 * @brief Caller-owned biquad filter instance. Members are private.
 *
 * A zero-initialised instance is a valid filter whose output is always zero.
 */
struct fpc_biquad {
        fxp_t b0;     /**< @private */
        fxp_t b1;     /**< @private */
        fxp_t b2;     /**< @private */
        fxp_t neg_a1; /**< @private -a1 */
        fxp_t neg_a2; /**< @private -a2 */
        fxp_t x1;     /**< @private x[k-1] */
        fxp_t x2;     /**< @private x[k-2] */
        fxp_t y1;     /**< @private y[k-1] */
        fxp_t y2;     /**< @private y[k-2] */
};

/**
 * @brief Initialise a FIR filter and clear its history.
 *
 * @param fir Filter instance to initialise.
 * @param cfg Configuration; its arrays are borrowed (see @ref fpc_fir_config).
 *
 * @retval FPC_STATUS_OK on success.
 * @retval FPC_STATUS_NULL_PTR if @p fir is NULL.
 * @retval FPC_STATUS_INVALID_PARAM if @p cfg is NULL, either array is NULL, or
 *         @c order is 0. @p fir is left uninitialised.
 */
enum fpc_status fpc_fir_init(struct fpc_fir *fir,
                             const struct fpc_fir_config *cfg);

/**
 * @brief Clear a FIR filter's input history.
 *
 * @retval FPC_STATUS_OK on success.
 * @retval FPC_STATUS_NULL_PTR if @p fir is NULL.
 * @retval FPC_STATUS_NOT_INITIALIZED if @p fir has not been initialised.
 */
enum fpc_status fpc_fir_reset(struct fpc_fir *fir);

/**
 * @brief Replace a FIR filter's configuration and clear its history.
 *
 * @retval FPC_STATUS_OK on success.
 * @retval FPC_STATUS_NULL_PTR if @p fir is NULL.
 * @retval FPC_STATUS_NOT_INITIALIZED if @p fir has not been initialised.
 * @retval FPC_STATUS_INVALID_PARAM if @p cfg is invalid; the filter is
 *         unchanged.
 */
enum fpc_status fpc_fir_set_config(struct fpc_fir *fir,
                                   const struct fpc_fir_config *cfg);

/**
 * @brief Push one sample through a FIR filter.
 *
 * @param fir Filter instance.
 * @param sample Input sample x[k].
 * @param[out] output Filter output y[k].
 *
 * @retval FPC_STATUS_OK on success.
 * @retval FPC_STATUS_SATURATED if the sum, or an intermediate partial sum,
 *         exceeded its range; @p output holds the saturated result.
 * @retval FPC_STATUS_NULL_PTR if @p fir or @p output is NULL.
 * @retval FPC_STATUS_NOT_INITIALIZED if @p fir has not been initialised.
 */
enum fpc_status fpc_fir_process(struct fpc_fir *fir, fxp_t sample,
                                fxp_t *output);

/**
 * @brief Initialise a biquad filter and clear its delay elements.
 *
 * @retval FPC_STATUS_OK on success.
 * @retval FPC_STATUS_NULL_PTR if @p bq is NULL.
 * @retval FPC_STATUS_INVALID_PARAM if @p cfg is NULL or @c a1 or @c a2 is
 *         @c FXP_MIN. @p bq is zeroed.
 */
enum fpc_status fpc_biquad_init(struct fpc_biquad *bq,
                                const struct fpc_biquad_config *cfg);

/**
 * @brief Clear a biquad filter's delay elements.
 *
 * @retval FPC_STATUS_OK on success.
 * @retval FPC_STATUS_NULL_PTR if @p bq is NULL.
 */
enum fpc_status fpc_biquad_reset(struct fpc_biquad *bq);

/**
 * @brief Replace a biquad filter's coefficients and clear its delay elements.
 *
 * @retval FPC_STATUS_OK on success.
 * @retval FPC_STATUS_NULL_PTR if @p bq is NULL.
 * @retval FPC_STATUS_INVALID_PARAM if @p cfg is invalid; the filter is
 *         unchanged.
 */
enum fpc_status fpc_biquad_set_config(struct fpc_biquad *bq,
                                      const struct fpc_biquad_config *cfg);

/**
 * @brief Push one sample through a biquad filter.
 *
 * @param bq Filter instance.
 * @param sample Input sample x[k].
 * @param[out] output Filter output y[k], also stored as the next y[k-1].
 *
 * @retval FPC_STATUS_OK on success.
 * @retval FPC_STATUS_SATURATED if the sum, or an intermediate partial sum,
 *         exceeded its range; @p output holds the saturated result.
 * @retval FPC_STATUS_NULL_PTR if @p bq or @p output is NULL.
 */
enum fpc_status fpc_biquad_process(struct fpc_biquad *bq, fxp_t sample,
                                   fxp_t *output);

#endif /* FPC_FILTERS_H_ */
