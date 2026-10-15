/**
 * SPDX-License-Identifier: MIT
 *
 * @file: fpc_pid.h
 *
 * @brief
 *    Safety-oriented fixed-point PID controller API.
 *
 * Gains, signals, bounds and the sample period are @c fxp_t values. The
 * integral is accumulated in a wide @c fxp_acc_t, so contributions smaller
 * than one LSB per step are kept, and every result outside its range
 * saturates and is reported.
 *
 * @par Storage
 *    Controllers are caller-owned. Declare a @c struct fpc_pid with any
 *    storage duration and pass its address to fpc_pid_init(). Its members are
 *    private to the library.
 *
 * @par Threading contract
 *    A controller is not internally synchronised. Calls on the same instance
 *    must be serialised by the caller; distinct instances are independent.
 */

#ifndef FPC_PID_H_
#define FPC_PID_H_

#include "fpc_status.h"
#include "fxp.h"
#include <stdint.h>

/**
 * Maximum value of @c fpc_pid_config.d_filter_alpha, and the value that
 * disables derivative smoothing (1.0).
 */
#define FPC_PID_D_FILTER_ALPHA_MAX FXP_ONE

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
 * The controller implements the continuous law
 *
 * \f[
 * u(t) = K_p e(t) + K_i \int e(t)\,dt + K_d \frac{de(t)}{dt}
 * \f]
 *
 * discretised with sample period \f$ \Delta t \f$:
 *
 * \f[
 * u[k] = K_p e[k] + \sum_{i=0}^{k} (K_i \Delta t)\, e[i] + D[k]
 * \f]
 *
 * The derivative term is smoothed with coefficient \f$ \alpha \f$:
 *
 * \f[
 * D[k] = D[k-1] + \alpha \left(\frac{K_d}{\Delta t}(e[k] - e[k-1])
 *        - D[k-1]\right)
 * \f]
 *
 * The discrete gains \f$ K_i \Delta t \f$ and \f$ K_d / \Delta t \f$ are
 * derived once when a configuration is applied. @c dt is in the time unit of
 * @c ki and @c kd; choose a unit in which @c dt is exactly representable, such
 * as milliseconds for a 1 kHz loop (@c dt = 1.0, @c ki in 1/ms).
 *
 * @details
 *   - ::kp  proportional gain.
 *   - ::ki  integral gain, per unit of @c dt time.
 *   - ::kd  derivative gain, in units of @c dt time.
 *   - ::dt  sample period; must be positive.
 *   - ::out_min, ::out_max  inclusive output bounds.
 *   - ::integral_min, ::integral_max  inclusive bounds on the integral term,
 *     applied to the wide accumulator (anti-windup).
 *   - ::d_filter_alpha  derivative smoothing coefficient in
 *     (0, @ref FPC_PID_D_FILTER_ALPHA_MAX]; the maximum disables smoothing.
 *
 * A configuration is rejected when @c dt is not positive, a lower bound
 * exceeds its upper bound, @c d_filter_alpha is out of range, or a derived
 * gain saturates or rounds to zero from a nonzero gain.
 */
struct fpc_pid_config {
        fxp_t kp;
        fxp_t ki;
        fxp_t kd;
        fxp_t dt;
        fxp_t out_min;
        fxp_t out_max;
        fxp_t integral_min;
        fxp_t integral_max;
        fxp_t d_filter_alpha;
};

/**
 * @struct fpc_pid
 * @brief Caller-owned PID controller instance. Members are private.
 */
struct fpc_pid {
        struct fpc_pid_config cfg; /**< @private Applied configuration. */
        fxp_t ki_dt;               /**< @private ki * dt */
        fxp_t kd_dt;               /**< @private kd / dt */
        fxp_acc_t integral;        /**< @private Integral accumulator. */
        fxp_t prev_error;          /**< @private e[k-1] */
        fxp_t filtered_derivative; /**< @private D[k] */
        fxp_t last_output;         /**< @private u[k] */
        fxp_t manual_output;       /**< @private Manual-mode output. */
        uint_least8_t mode;        /**< @private enum fpc_pid_mode */
};

/**
 * @struct fpc_pid_state
 * @brief Runtime PID state for diagnostics and tuning.
 */
struct fpc_pid_state {
        fxp_t integral;            /**< Integral term, rounded to fxp_t. */
        fxp_t prev_error;          /**< Previous error e[k-1]. */
        fxp_t filtered_derivative; /**< Smoothed derivative term D[k]. */
        fxp_t last_output;         /**< Last output u[k]. */
        enum fpc_pid_mode mode;    /**< Current operating mode. */
};

/**
 * @brief Initialise a PID controller in auto mode with cleared state.
 *
 * @retval FPC_STATUS_OK on success.
 * @retval FPC_STATUS_NULL_PTR if @p pid is NULL.
 * @retval FPC_STATUS_INVALID_PARAM if @p cfg is NULL or invalid (see
 *         @ref fpc_pid_config). @p pid is left uninitialised.
 */
enum fpc_status fpc_pid_init(struct fpc_pid *pid,
                             const struct fpc_pid_config *cfg);

/**
 * @brief Clear the integral, derivative and error history.
 *
 * The last output becomes the manual output in manual mode, and 0 otherwise.
 *
 * @retval FPC_STATUS_OK on success.
 * @retval FPC_STATUS_NULL_PTR if @p pid is NULL.
 * @retval FPC_STATUS_NOT_INITIALIZED if @p pid has not been initialised.
 */
enum fpc_status fpc_pid_reset(struct fpc_pid *pid);

/**
 * @brief Apply a new configuration, keeping runtime state.
 *
 * The integral, last output and manual output are clamped to the new bounds.
 *
 * @retval FPC_STATUS_OK on success.
 * @retval FPC_STATUS_SATURATED if any state was clamped.
 * @retval FPC_STATUS_NULL_PTR if @p pid is NULL.
 * @retval FPC_STATUS_NOT_INITIALIZED if @p pid has not been initialised.
 * @retval FPC_STATUS_INVALID_PARAM if @p cfg is NULL or invalid; the
 *         controller is unchanged.
 */
enum fpc_status fpc_pid_set_config(struct fpc_pid *pid,
                                   const struct fpc_pid_config *cfg);

/**
 * @brief Read the applied configuration.
 *
 * @retval FPC_STATUS_OK on success.
 * @retval FPC_STATUS_NULL_PTR if @p pid or @p cfg is NULL.
 * @retval FPC_STATUS_NOT_INITIALIZED if @p pid has not been initialised.
 */
enum fpc_status fpc_pid_get_config(const struct fpc_pid *pid,
                                   struct fpc_pid_config *cfg);

/**
 * @brief Switch between auto and manual mode.
 *
 * Entering manual mode holds @p manual_output, clamped to the output bounds,
 * and clears the derivative term. Returning to auto mode re-biases the integral
 * so that the next output, for an unchanged error, equals the last output
 * (bumpless transfer).
 *
 * @param pid Controller instance.
 * @param mode Requested mode.
 * @param manual_output Output to hold in manual mode; ignored for auto mode.
 *
 * @retval FPC_STATUS_OK on success.
 * @retval FPC_STATUS_SATURATED if the manual output or the re-biased integral
 *         was clamped.
 * @retval FPC_STATUS_NULL_PTR if @p pid is NULL.
 * @retval FPC_STATUS_NOT_INITIALIZED if @p pid has not been initialised.
 * @retval FPC_STATUS_INVALID_PARAM if @p mode is not a valid mode.
 */
enum fpc_status fpc_pid_set_mode(struct fpc_pid *pid, enum fpc_pid_mode mode,
                                 fxp_t manual_output);

/**
 * @brief Run one controller step.
 *
 * In manual mode, records the error and returns the held output.
 *
 * @param pid Controller instance.
 * @param setpoint Desired value.
 * @param measurement Measured value.
 * @param[out] output Controller output u[k].
 *
 * @retval FPC_STATUS_OK on success.
 * @retval FPC_STATUS_SATURATED if the error, the error change, the raw
 *         derivative, the integral or the output was clamped; @p output
 *         holds the clamped result.
 * @retval FPC_STATUS_NULL_PTR if @p pid or @p output is NULL.
 * @retval FPC_STATUS_NOT_INITIALIZED if @p pid has not been initialised.
 */
enum fpc_status fpc_pid_compute(struct fpc_pid *pid, fxp_t setpoint,
                                fxp_t measurement, fxp_t *output);

/**
 * @brief Read the runtime state.
 *
 * @retval FPC_STATUS_OK on success.
 * @retval FPC_STATUS_NULL_PTR if @p pid or @p state is NULL.
 * @retval FPC_STATUS_NOT_INITIALIZED if @p pid has not been initialised.
 */
enum fpc_status fpc_pid_get_state(const struct fpc_pid *pid,
                                  struct fpc_pid_state *state);

#endif /* FPC_PID_H_ */
