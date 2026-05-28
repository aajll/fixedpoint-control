/**
 * @file fpc_filters.h
 * @brief Safety-oriented FIR and biquad filter APIs.
 *
 * This module provides fixed-point implementations of:
 *   - Finite Impulse Response (FIR) filters
 *   - Biquad (second-order IIR) filters
 *
 * Both filter types use Q16.16 fixed-point arithmetic for efficient computation
 * on embedded systems without floating-point support.
 *
 * @par Threading contract
 *    fpc inherits a **single-writer / many-readers** contract from its
 *    backing allocator. For a given filter instance:
 *      - One context may call the mutating functions: @c fpc_*_init,
 *        @c fpc_*_deinit, @c fpc_*_reset, @c fpc_*_set_config,
 *        @c fpc_*_process.
 *      - Read-only callers may run concurrently with the single writer.
 *      - Two mutating contexts that share an instance must be serialised
 *        by the caller.
 */

#ifndef FPC_FILTERS_H_
#define FPC_FILTERS_H_

#include "fpc_config.h"

#include "fpc_status.h"
#include <stdint.h>

struct fpc_fir;
struct fpc_biquad;

/**
 * @struct fpc_fir
 * @brief Opaque handle to a FIR filter instance.
 *
 * This structure is managed by an internal allocator and stores the filter's
 * coefficients and history buffer for the convolution operation.
 */

/**
 * @struct fpc_biquad
 * @brief Opaque handle to a biquad (IIR) filter instance.
 *
 * This structure is managed by an internal allocator and stores the filter's
 * difference equation coefficients and internal state (delay elements).
 */

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
 * where:
 *   - \f$ N \f$ is the filter order (number of taps)
 *   - \f$ h[i] \f$ are the filter coefficients (Q16.16 format)
 *   - \f$ x[k] \f$ is the input at time step \f$ k \f$
 *
 * @note The filter uses a circular buffer for input history.
 * @note Coefficients are stored in Q16.16 fixed-point format.
 */
struct fpc_fir_config {
        uint16_t order; /**< Filter order (number of taps, 1 to
                           FPC_FILTER_MAX_ORDER) */
        const int32_t
            *coeffs; /**< Pointer to array of Q16.16 filter coefficients */
};

/**
 * @struct fpc_biquad_config
 * @brief Biquad (second-order IIR) filter coefficient set in Q16.16 format.
 *
 * The biquad filter implements the following difference equation:
 *
 * \f[
 * y[k] = b_0 \cdot x[k] + b_1 \cdot x[k-1] + b_2 \cdot x[k-2]
 *       - a_1 \cdot y[k-1] - a_2 \cdot y[k-2]
 * \f]
 *
 * In fixed-point Q16.16 format, this becomes:
 *
 * \f[
 * y[k] = \frac{
 *   b_0 \cdot x[k] + b_1 \cdot x[k-1] + b_2 \cdot x[k-2]
 *   - a_1 \cdot y[k-1] - a_2 \cdot y[k-2]
 * }{2^{16}}
 * \f]
 *
 * where the coefficients are in Q16.16 format and the delayed input/output
 * samples are stored as raw `int32_t` sample values.
 *
 * @note The implementation stores delayed input and output samples explicitly
 *       as `x1`, `x2`, `y1`, and `y2`.
 * @note Coefficients are stored in Q16.16 fixed-point format (1 sign bit, 15
 * integer bits, 16 fractional bits).
 */
struct fpc_biquad_config {
        int32_t b0; /**< Feedforward coefficient for x[k] (Q16.16) */
        int32_t b1; /**< Feedforward coefficient for x[k-1] (Q16.16) */
        int32_t b2; /**< Feedforward coefficient for x[k-2] (Q16.16) */
        int32_t a1; /**< Feedback coefficient for y[k-1] (Q16.16) */
        int32_t a2; /**< Feedback coefficient for y[k-2] (Q16.16) */
};

/**
 * @brief Initialize the filter pool allocator.
 *
 * @return enum fpc_status Status code indicating success or failure.
 *
 * @pre No pre-initialization required.
 * @post The pool allocator is initialized and ready for filter instance
 * creation.
 * @note Must be called before any filter instances can be created.
 */
enum fpc_status fpc_filter_pool_init(void);

/**
 * @brief Create and initialize a new FIR filter instance.
 *
 * @param ctx Pointer to store the filter handle (output).
 * @param cfg Configuration structure with filter order and coefficients.
 *
 * @return enum fpc_status Status code indicating success or failure.
 *
 * @retval FPC_STATUS_OK on success.
 * @retval FPC_STATUS_NULL_PTR if ctx is NULL.
 * @retval FPC_STATUS_INVALID_PARAM if cfg contains invalid parameters (NULL
 * coeffs, invalid order).
 * @retval FPC_STATUS_NOT_INITIALIZED if pool initialization failed.
 * @retval FPC_STATUS_POOL_FULL if no free filter slots available.
 *
 * @pre fpc_filter_pool_init() must have been called successfully.
 * @post *ctx is set to a valid filter handle on success.
 * @post Filter history is cleared (all samples set to zero).
 * @note The filter coefficients are copied internally; changes to the coeffs
 * array after initialization do not affect the filter.
 * @note Filter order must be between 1 and FPC_FILTER_MAX_ORDER (inclusive).
 */
enum fpc_status fpc_fir_init(struct fpc_fir **ctx,
                             const struct fpc_fir_config *cfg);

/**
 * @brief Reset a FIR filter's internal history buffer.
 *
 * @param ctx Filter handle to reset.
 *
 * @return enum fpc_status Status code indicating success or failure.
 *
 * @retval FPC_STATUS_OK on success.
 * @retval FPC_STATUS_NULL_PTR if ctx is NULL.
 * @retval FPC_STATUS_NOT_INITIALIZED if ctx is not a valid initialized filter.
 *
 * @pre fpc_fir_init() must have been called with ctx.
 * @post All history samples are set to zero.
 * @post The circular buffer head pointer is reset to zero.
 */
enum fpc_status fpc_fir_reset(struct fpc_fir *ctx);

/**
 * @brief Update a FIR filter's coefficient configuration.
 *
 * @param ctx Filter handle to update.
 * @param cfg New configuration structure with coefficients.
 *
 * @return enum fpc_status Status code indicating success or failure.
 *
 * @retval FPC_STATUS_OK on success.
 * @retval FPC_STATUS_NULL_PTR if ctx is NULL.
 * @retval FPC_STATUS_INVALID_PARAM if cfg is NULL or contains invalid
 * parameters.
 * @retval FPC_STATUS_NOT_INITIALIZED if ctx is not a valid initialized filter.
 *
 * @pre fpc_fir_init() must have been called with ctx.
 * @post All coefficients are updated atomically.
 * @post Filter history is cleared (all samples set to zero).
 * @note This is useful for time-varying filters or reconfiguration.
 */
enum fpc_status fpc_fir_set_config(struct fpc_fir *ctx,
                                   const struct fpc_fir_config *cfg);

/**
 * @brief Process a single sample through the FIR filter.
 *
 * @param ctx Filter handle to use for processing.
 * @param sample Input sample value.
 * @param output Filtered output sample (output).
 *
 * @return enum fpc_status Status code indicating success or failure.
 *
 * @retval FPC_STATUS_OK on success.
 * @retval FPC_STATUS_NULL_PTR if ctx or output is NULL.
 * @retval FPC_STATUS_NOT_INITIALIZED if ctx is not a valid initialized filter.
 * @retval FPC_STATUS_OVERFLOW if convolution result exceeds int32_t range.
 *
 * @pre fpc_fir_init() must have been called with ctx.
 * @post *output contains the filtered result.
 * @post Internal history buffer is updated with the new sample.
 * @note The filter uses Q16.16 fixed-point arithmetic internally.
 * @note Convolution is computed as: output = (sum(h[i] * x[i])) >> 16
 */
enum fpc_status fpc_fir_process(struct fpc_fir *ctx, int32_t sample,
                                int32_t *output);

/**
 * @brief Deinitialize a FIR filter instance and return it to the pool.
 *
 * @param ctx Filter handle to deinitialize.
 *
 * @return enum fpc_status Status code indicating success or failure.
 *
 * @retval FPC_STATUS_OK on success.
 * @retval FPC_STATUS_NULL_PTR if ctx is NULL.
 * @retval FPC_STATUS_NOT_INITIALIZED if ctx is not a valid initialized filter.
 *
 * @pre fpc_fir_init() must have been called with ctx.
 * @post ctx is no longer valid and cannot be used after return.
 * @note The filter state is released back to the pool.
 */
enum fpc_status fpc_fir_deinit(struct fpc_fir *ctx);

/**
 * @brief Create and initialize a new biquad filter instance.
 *
 * @param ctx Pointer to store the filter handle (output).
 * @param cfg Configuration structure with filter coefficients.
 *
 * @return enum fpc_status Status code indicating success or failure.
 *
 * @retval FPC_STATUS_OK on success.
 * @retval FPC_STATUS_NULL_PTR if ctx is NULL.
 * @retval FPC_STATUS_INVALID_PARAM if cfg is NULL.
 * @retval FPC_STATUS_NOT_INITIALIZED if pool initialization failed.
 * @retval FPC_STATUS_POOL_FULL if no free filter slots available.
 *
 * @pre fpc_filter_pool_init() must have been called successfully.
 * @post *ctx is set to a valid filter handle on success.
 * @post Filter state (x1, x2, y1, y2) is cleared to zero.
 * @note Coefficients must be provided in Q16.16 format.
 */
enum fpc_status fpc_biquad_init(struct fpc_biquad **ctx,
                                const struct fpc_biquad_config *cfg);

/**
 * @brief Reset a biquad filter's internal state (delay elements).
 *
 * @param ctx Filter handle to reset.
 *
 * @return enum fpc_status Status code indicating success or failure.
 *
 * @retval FPC_STATUS_OK on success.
 * @retval FPC_STATUS_NULL_PTR if ctx is NULL.
 * @retval FPC_STATUS_NOT_INITIALIZED if ctx is not a valid initialized filter.
 *
 * @pre fpc_biquad_init() must have been called with ctx.
 * @post All delay elements (x1, x2, y1, y2) are set to zero.
 */
enum fpc_status fpc_biquad_reset(struct fpc_biquad *ctx);

/**
 * @brief Update a biquad filter's coefficient configuration.
 *
 * @param ctx Filter handle to update.
 * @param cfg New configuration structure with coefficients.
 *
 * @return enum fpc_status Status code indicating success or failure.
 *
 * @retval FPC_STATUS_OK on success.
 * @retval FPC_STATUS_NULL_PTR if ctx is NULL.
 * @retval FPC_STATUS_INVALID_PARAM if cfg is NULL.
 * @retval FPC_STATUS_NOT_INITIALIZED if ctx is not a valid initialized filter.
 *
 * @pre fpc_biquad_init() must have been called with ctx.
 * @post All filter coefficients are updated atomically.
 * @post The filter delay state is reset to zero.
 */
enum fpc_status fpc_biquad_set_config(struct fpc_biquad *ctx,
                                      const struct fpc_biquad_config *cfg);

/**
 * @brief Process a single sample through the biquad filter.
 *
 * @param ctx Filter handle to use for processing.
 * @param sample Input sample value.
 * @param output Filtered output sample (output).
 *
 * @return enum fpc_status Status code indicating success or failure.
 *
 * @retval FPC_STATUS_OK on success.
 * @retval FPC_STATUS_NULL_PTR if ctx or output is NULL.
 * @retval FPC_STATUS_NOT_INITIALIZED if ctx is not a valid initialized filter.
 * @retval FPC_STATUS_OVERFLOW if computation result exceeds int32_t range.
 *
 * @pre fpc_biquad_init() must have been called with ctx.
 * @post *output contains the filtered result.
 * @post Internal state (x1, x2, y1, y2) is updated for next sample.
 * @note The filter uses Q16.16 fixed-point arithmetic.
 * @note Implementation evaluates the documented difference equation directly
 *       using delayed input/output samples.
 * @note Output is computed as: output = (acc) >> 16 where acc is the Q32.32
 * intermediate result.
 */
enum fpc_status fpc_biquad_process(struct fpc_biquad *ctx, int32_t sample,
                                   int32_t *output);

/**
 * @brief Deinitialize a biquad filter instance and return it to the pool.
 *
 * @param ctx Filter handle to deinitialize.
 *
 * @return enum fpc_status Status code indicating success or failure.
 *
 * @retval FPC_STATUS_OK on success.
 * @retval FPC_STATUS_NULL_PTR if ctx is NULL.
 * @retval FPC_STATUS_NOT_INITIALIZED if ctx is not a valid initialized filter.
 *
 * @pre fpc_biquad_init() must have been called with ctx.
 * @post ctx is no longer valid and cannot be used after return.
 * @note The filter state is released back to the pool.
 */
enum fpc_status fpc_biquad_deinit(struct fpc_biquad *ctx);

#endif /* FPC_FILTERS_H_ */
