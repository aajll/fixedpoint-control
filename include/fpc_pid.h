/**
 * SPDX-License-Identifier: MIT
 *
 * @file: fpc_pid.h
 *
 * @brief
 *    Safety-oriented fixed-point PID controller API.
 *
 * @par Threading contract
 *    fpc inherits a **single-writer / many-readers** contract from its
 *    backing allocator. For a given controller instance:
 *      - One context may call the mutating functions: @c fpc_pid_init,
 *        @c fpc_pid_deinit, @c fpc_pid_reset, @c fpc_pid_set_config,
 *        @c fpc_pid_set_mode, @c fpc_pid_compute.
 *      - Any number of contexts may call the read-only @c fpc_pid_get_config
 *        and @c fpc_pid_get_state concurrently with the single writer.
 *      - Two mutating contexts that share an instance must be serialised
 *        by the caller.
 */

#ifndef FPC_PID_H_
#define FPC_PID_H_

#include "fpc_status.h"
#include <stdint.h>

struct fpc_pid;

/**
 * Maximum value `fpc_pid_config.d_filter_alpha` may take, and the value
 * that disables derivative smoothing. Numerically `65536` — i.e. the
 * Q16.16 representation of `1.0`. The field itself is `uint32_t`, so the
 * value fits with room to spare.
 */
#define FPC_PID_D_FILTER_ALPHA_MAX (65536U)

/**
 * @struct fpc_pid
 * @brief Opaque handle to a PID controller instance.
 *
 * This structure is managed by an internal allocator and should never be
 * accessed directly. It stores all state required for PID computation.
 */

/**
 * @enum fpc_pid_mode
 * @brief PID runtime operating mode.
 *
 * @note In manual mode, the controller outputs a fixed value regardless of
 *       error. In auto mode, the PID computation is performed on each call
 *       to fpc_pid_compute().
 */
enum fpc_pid_mode {
        FPC_PID_MODE_AUTO = 0,
        FPC_PID_MODE_MANUAL = 1
};

/**
 * @struct fpc_pid_config
 * @brief PID controller configuration.
 *
 * The controller implements the discrete PID algorithm:
 *
 * \f[
 * u(t) = K_p \cdot e(t) + K_i \cdot \sum{e(t)} + K_d \cdot \frac{d e(t)}{d t}
 * \f]
 *
 * The implementation evaluates the proportional, integral, and derivative
 * terms as integer ratios over the configured sample time \f$ \Delta t \f$:
 *
 * \f[
 * u[k] = \frac{K_p}{\Delta t} \cdot e[k] + \frac{K_i}{\Delta t} \cdot
 * \sum_{i=0}^{k} e[i]
 *        + \frac{K_d}{\Delta t} \cdot (e[k] - e[k-1])
 * \f]
 *
 * The derivative term uses exponential smoothing with coefficient \f$ \alpha
 * \f$:
 *
 * \f[
 * D[k] = D[k-1] + \frac{\alpha}{65536} \cdot \left(D_{raw}[k] - D[k-1]\right)
 * \f]
 *
 * where \f$ D_{raw}[k] = \frac{K_d}{\Delta t} \cdot (e[k] - e[k-1]) \f$ and
 * \f$ \alpha \in [0, 65536] \f$ is a Q16.16 smoothing coefficient.
 *
 * @note `kp`, `ki`, `kd`, the integral term, the filtered derivative, and the
 *       output are stored as signed `int32_t` controller units. Only
 *       `d_filter_alpha` is expressed in Q16.16.
 * @note `d_filter_alpha = 65536U` disables smoothing; smaller values apply
 *       stronger low-pass filtering.
 *
 * @post Configuration validation ensures:
 *   - dt > 0 (positive sample time)
 *   - out_min <= out_max (valid output range)
 *   - integral_min <= integral_max (valid integral bounds)
 *   - d_filter_alpha <= 65536 (valid smoothing coefficient)
 */
struct fpc_pid_config {
        int32_t kp; /**< Proportional gain numerator divided by dt at runtime */
        int32_t ki; /**< Integral gain numerator divided by dt at runtime */
        int32_t kd; /**< Derivative gain numerator divided by dt at runtime */
        int32_t dt; /**< Sample time in ticks (must be > 0) */
        int32_t out_min;         /**< Minimum output clamp (inclusive) */
        int32_t out_max;         /**< Maximum output clamp (inclusive) */
        int32_t integral_min;    /**< Minimum integral term (inclusive) */
        int32_t integral_max;    /**< Maximum integral term (inclusive) */
        uint32_t d_filter_alpha; /**< Derivative smoothing coefficient (Q16.16,
                                    0-65536) */
};

/**
 * @struct fpc_pid_state
 * @brief Runtime PID state for diagnostics and tuning.
 *
 * This structure provides access to internal controller state for:
 *   - Online tuning adjustments
 *   - Controller performance monitoring
 *   - Mode transition handling
 *
 * @note The state values are raw controller units returned by the runtime
 *       implementation; they are not Q16.16-encoded values.
 */
struct fpc_pid_state {
        int32_t integral;            /**< Current integral term */
        int32_t prev_error;          /**< Previous error value e[k-1] */
        int32_t filtered_derivative; /**< Smoothed derivative term D[k] */
        int32_t last_output;         /**< Last computed output u[k] */
        enum fpc_pid_mode mode;      /**< Current operating mode */
};

/**
 * @brief Initialize the PID controller pool.
 *
 * @return enum fpc_status Status code indicating success or failure.
 *
 * @pre No pre-initialization required.
 * @post The pool allocator is initialized and ready for fpc_pid_init() calls.
 * @note Must be called before any PID controller instances can be created.
 * @see fpc_pid_init()
 */
enum fpc_status fpc_pid_pool_init(void);

/**
 * @brief Create and initialize a new PID controller instance.
 *
 * @param ctx Pointer to store the controller handle (output).
 * @param cfg Configuration structure with controller gains and bounds.
 *
 * @return enum fpc_status Status code indicating success or failure.
 *
 * @retval FPC_STATUS_OK on success.
 * @retval FPC_STATUS_NULL_PTR if ctx is NULL.
 * @retval FPC_STATUS_INVALID_PARAM if cfg contains invalid parameters.
 * @retval FPC_STATUS_NOT_INITIALIZED if pool initialization failed.
 * @retval FPC_STATUS_POOL_FULL if no free controller slots available.
 *
 * @pre fpc_pid_pool_init() must have been called successfully.
 * @post *ctx is set to a valid controller handle on success.
 * @post Controller is in AUTO mode with integral term initialized to zero.
 * @note The controller inherits configuration immediately; changes to cfg
 *       after return do not affect the controller.
 */
enum fpc_status fpc_pid_init(struct fpc_pid **ctx,
                             const struct fpc_pid_config *cfg);

/**
 * @brief Deinitialize a PID controller instance and return it to the pool.
 *
 * @param ctx Controller handle to deinitialize.
 *
 * @return enum fpc_status Status code indicating success or failure.
 *
 * @retval FPC_STATUS_OK on success.
 * @retval FPC_STATUS_NULL_PTR if ctx is NULL.
 * @retval FPC_STATUS_NOT_INITIALIZED if ctx is not a valid initialized
 * controller.
 *
 * @pre fpc_pid_init() must have been called with ctx.
 * @post ctx is no longer valid and cannot be used after return.
 * @note The controller state is released back to the pool.
 */
enum fpc_status fpc_pid_deinit(struct fpc_pid *ctx);

/**
 * @brief Reset a PID controller to its initial state.
 *
 * @param ctx Controller handle to reset.
 *
 * @return enum fpc_status Status code indicating success or failure.
 *
 * @retval FPC_STATUS_OK on success.
 * @retval FPC_STATUS_NULL_PTR if ctx is NULL.
 * @retval FPC_STATUS_NOT_INITIALIZED if ctx is not a valid initialized
 * controller.
 *
 * @pre fpc_pid_init() must have been called with ctx.
 * @post The integral term, previous error, and filtered derivative are zeroed.
 * @post If in MANUAL mode, last_output retains the manual value.
 * @post If in AUTO mode, last_output is set to zero.
 */
enum fpc_status fpc_pid_reset(struct fpc_pid *ctx);

/**
 * @brief Update a PID controller's configuration parameters.
 *
 * @param ctx Controller handle to update.
 * @param cfg New configuration structure.
 *
 * @return enum fpc_status Status code indicating success or failure.
 *
 * @retval FPC_STATUS_OK on success.
 * @retval FPC_STATUS_NULL_PTR if ctx is NULL.
 * @retval FPC_STATUS_INVALID_PARAM if cfg is NULL or contains invalid
 * parameters.
 * @retval FPC_STATUS_NOT_INITIALIZED if ctx is not a valid initialized
 * controller.
 * @retval FPC_STATUS_SATURATED if internal state was clamped.
 *
 * @pre fpc_pid_init() must have been called with ctx.
 * @post All configuration parameters are updated atomically.
 * @post Integral term is clamped to integral_min..integral_max if out of
 * bounds.
 * @post last_output is clamped to out_min..out_max if out of bounds.
 * @note The controller does not reset on configuration change; state persists.
 */
enum fpc_status fpc_pid_set_config(struct fpc_pid *ctx,
                                   const struct fpc_pid_config *cfg);

/**
 * @brief Copy a PID controller's current configuration.
 *
 * @param ctx Controller handle to read.
 * @param cfg Buffer to store configuration (output).
 *
 * @return enum fpc_status Status code indicating success or failure.
 *
 * @retval FPC_STATUS_OK on success.
 * @retval FPC_STATUS_NULL_PTR if ctx or cfg is NULL.
 * @retval FPC_STATUS_NOT_INITIALIZED if ctx is not a valid initialized
 * controller.
 *
 * @pre fpc_pid_init() must have been called with ctx.
 * @post *cfg contains a snapshot of the current configuration.
 * @note Reflects any configuration changes made via fpc_pid_set_config().
 */
enum fpc_status fpc_pid_get_config(const struct fpc_pid *ctx,
                                   struct fpc_pid_config *cfg);

/**
 * @brief Change PID controller operating mode.
 *
 * @param ctx Controller handle to update.
 * @param mode Desired operating mode (AUTO or MANUAL).
 * @param manual_output Output value to use in MANUAL mode.
 *
 * @return enum fpc_status Status code indicating success or failure.
 *
 * @retval FPC_STATUS_OK on success.
 * @retval FPC_STATUS_NULL_PTR if ctx is NULL.
 * @retval FPC_STATUS_INVALID_PARAM if mode is invalid.
 * @retval FPC_STATUS_NOT_INITIALIZED if ctx is not a valid initialized
 * controller.
 * @retval FPC_STATUS_SATURATED if manual_output was clamped to
 * out_min..out_max.
 *
 * @pre fpc_pid_init() must have been called with ctx.
 * @post When switching from MANUAL to AUTO, the integral term is rebalanced
 *       to maintain output continuity (anti-windup).
 * @post When switching to MANUAL, last_output is set to the clamped
 * manual_output value.
 * @post When switching to AUTO, the integral term is adjusted to account for
 *       the proportional and derivative contributions to the current output.
 */
enum fpc_status fpc_pid_set_mode(struct fpc_pid *ctx, enum fpc_pid_mode mode,
                                 int32_t manual_output);

/**
 * @brief Compute the PID output for a given setpoint and measurement.
 *
 * @param ctx Controller handle to use for computation.
 * @param setpoint Target value (setpoint).
 * @param measurement Current process value (measurement).
 * @param output Computed control output (output).
 *
 * @return enum fpc_status Status code indicating success or failure.
 *
 * @retval FPC_STATUS_OK on success with no clamping.
 * @retval FPC_STATUS_NULL_PTR if ctx or output is NULL.
 * @retval FPC_STATUS_NOT_INITIALIZED if ctx is not a valid initialized
 * controller.
 * @retval FPC_STATUS_OVERFLOW if an intermediate value cannot be represented as
 * int32_t.
 * @retval FPC_STATUS_SATURATED if output or integral term was clamped.
 *
 * @pre fpc_pid_init() must have been called with ctx.
 * @post On `FPC_STATUS_OK` or `FPC_STATUS_SATURATED`, *output contains the
 *       computed PID value clamped to out_min..out_max.
 * @post In AUTO mode with no overflow: computes full PID with proportional,
 *       integral, and derivative terms and updates the controller state.
 * @post In MANUAL mode: returns last_output without computation, updates
 * prev_error, and clears the filtered derivative state.
 * @note Error is computed as: error = setpoint - measurement
 * @note If overflow is detected, the controller holds the previous output and
 *       returns FPC_STATUS_OVERFLOW.
 */
enum fpc_status fpc_pid_compute(struct fpc_pid *ctx, int32_t setpoint,
                                int32_t measurement, int32_t *output);

/**
 * @brief Retrieve the current runtime state of a PID controller.
 *
 * @param ctx Controller handle to read.
 * @param state Buffer to store controller state (output).
 *
 * @return enum fpc_status Status code indicating success or failure.
 *
 * @retval FPC_STATUS_OK on success.
 * @retval FPC_STATUS_NULL_PTR if ctx or state is NULL.
 * @retval FPC_STATUS_NOT_INITIALIZED if ctx is not a valid initialized
 * controller.
 *
 * @pre fpc_pid_init() must have been called with ctx.
 * @post *state contains a snapshot of the current internal state.
 * @note This function is useful for diagnostics, logging, and mode transition
 * logic.
 */
enum fpc_status fpc_pid_get_state(const struct fpc_pid *ctx,
                                  struct fpc_pid_state *state);

#endif /* FPC_PID_H_ */
