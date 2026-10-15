/**
 * SPDX-License-Identifier: MIT
 *
 * @file: pid_controller.c
 *
 * @brief
 *    Fixed-point PID controller implementation.
 *
 * Implements the PID API declared in @ref fpc_pid.h. The integral term is
 * held in a wide accumulator scaled by 2^FXP_FRAC_BITS and is narrowed only
 * when it is combined into an output.
 *
 * The unchecked wide sums cannot overflow in any fxp format. With W-bit
 * words, fxp guarantees FXP_FRAC_BITS <= W - 2, so the integral accumulator
 * and the widened derivative (an fxp_t times FXP_ONE) are each at most
 * 2^(2W-3) in magnitude and any product of two fxp_t is at most 2^(2W-2).
 * Every sum of two such terms and one product stays below 2^(2W-1).
 *
 * @note Public API functions are documented in the header; internal helpers are
 *       marked @internal.
 */
#include "../include/fpc_pid.h"
#include <stdbool.h>
#include <stddef.h>

/* ================ INTERNAL HELPERS ===================================== */

/** @internal Widen an fxp_t to the accumulator scale. */
static fxp_acc_t
fpc_pid_widen(fxp_t value)
{
        return (fxp_acc_t)value * (fxp_acc_t)FXP_ONE;
}

/** @internal Clamp a wide value to the scaled bounds [lo, hi]. */
static fxp_acc_t
fpc_pid_clamp_acc(fxp_acc_t value, fxp_t lo, fxp_t hi, bool *clamped)
{
        const fxp_acc_t wide_lo = fpc_pid_widen(lo);
        const fxp_acc_t wide_hi = fpc_pid_widen(hi);

        if (value < wide_lo) {
                *clamped = true;
                return wide_lo;
        }
        if (value > wide_hi) {
                *clamped = true;
                return wide_hi;
        }
        return value;
}

/** @internal Clamp an fxp_t to [lo, hi]. */
static fxp_t
fpc_pid_clamp(fxp_t value, fxp_t lo, fxp_t hi, bool *clamped)
{
        bool hit = false;
        fxp_t result = fxp_clamp_wide((fxp_wide_t)value, lo, hi, &hit);

        if (hit) {
                *clamped = true;
        }
        return result;
}

/** @internal Check a derived gain: false if it saturated or vanished. */
static bool
fpc_pid_gain_valid(enum fxp_status st, fxp_t gain, fxp_t derived)
{
        return (st == FXP_OK) && ((gain == 0) || (derived != 0));
}

/**
 * @internal Validate @p cfg and derive its discrete gains.
 *
 * @return true if @p cfg can be applied; the outputs are then set.
 */
static bool
fpc_pid_derive(const struct fpc_pid_config *cfg, fxp_t *ki_dt, fxp_t *kd_dt)
{
        enum fxp_status st;

        if ((cfg == NULL) || (cfg->dt <= 0) || (cfg->out_min > cfg->out_max)
            || (cfg->integral_min > cfg->integral_max)
            || (cfg->d_filter_alpha <= 0)
            || (cfg->d_filter_alpha > FPC_PID_D_FILTER_ALPHA_MAX)) {
                return false;
        }

        st = fxp_mul_chk(cfg->ki, cfg->dt, ki_dt);
        if (!fpc_pid_gain_valid(st, cfg->ki, *ki_dt)) {
                return false;
        }

        st = fxp_div_chk(cfg->kd, cfg->dt, kd_dt);
        return fpc_pid_gain_valid(st, cfg->kd, *kd_dt);
}

/** @internal A controller is initialised once a positive dt is applied. */
static bool
fpc_pid_ready(const struct fpc_pid *pid)
{
        return pid->cfg.dt > 0;
}

/** @internal Check the instance pointer and its initialisation. */
static enum fpc_status
fpc_pid_check(const struct fpc_pid *pid)
{
        if (pid == NULL) {
                return FPC_STATUS_NULL_PTR;
        }
        return fpc_pid_ready(pid) ? FPC_STATUS_OK : FPC_STATUS_NOT_INITIALIZED;
}

/* ================ PUBLIC API =========================================== */

enum fpc_status
fpc_pid_init(struct fpc_pid *pid, const struct fpc_pid_config *cfg)
{
        static const struct fpc_pid zero = {0};
        fxp_t ki_dt = 0;
        fxp_t kd_dt = 0;

        if (pid == NULL) {
                return FPC_STATUS_NULL_PTR;
        }

        *pid = zero;
        if (!fpc_pid_derive(cfg, &ki_dt, &kd_dt)) {
                return FPC_STATUS_INVALID_PARAM;
        }

        pid->cfg = *cfg;
        pid->ki_dt = ki_dt;
        pid->kd_dt = kd_dt;
        pid->mode = (uint_least8_t)FPC_PID_MODE_AUTO;
        return FPC_STATUS_OK;
}

enum fpc_status
fpc_pid_reset(struct fpc_pid *pid)
{
        enum fpc_status status = fpc_pid_check(pid);

        if (status != FPC_STATUS_OK) {
                return status;
        }

        pid->integral = 0;
        pid->prev_error = 0;
        pid->filtered_derivative = 0;
        pid->last_output = ((enum fpc_pid_mode)pid->mode == FPC_PID_MODE_MANUAL)
                               ? pid->manual_output
                               : 0;
        return FPC_STATUS_OK;
}

enum fpc_status
fpc_pid_set_config(struct fpc_pid *pid, const struct fpc_pid_config *cfg)
{
        enum fpc_status status = fpc_pid_check(pid);
        fxp_t ki_dt = 0;
        fxp_t kd_dt = 0;
        bool clamped = false;

        if (status != FPC_STATUS_OK) {
                return status;
        }
        if (!fpc_pid_derive(cfg, &ki_dt, &kd_dt)) {
                return FPC_STATUS_INVALID_PARAM;
        }

        pid->cfg = *cfg;
        pid->ki_dt = ki_dt;
        pid->kd_dt = kd_dt;
        pid->integral = fpc_pid_clamp_acc(pid->integral, cfg->integral_min,
                                          cfg->integral_max, &clamped);
        pid->last_output = fpc_pid_clamp(pid->last_output, cfg->out_min,
                                         cfg->out_max, &clamped);
        pid->manual_output = fpc_pid_clamp(pid->manual_output, cfg->out_min,
                                           cfg->out_max, &clamped);
        return clamped ? FPC_STATUS_SATURATED : FPC_STATUS_OK;
}

enum fpc_status
fpc_pid_get_config(const struct fpc_pid *pid, struct fpc_pid_config *cfg)
{
        enum fpc_status status = fpc_pid_check(pid);

        if (cfg == NULL) {
                return FPC_STATUS_NULL_PTR;
        }
        if (status != FPC_STATUS_OK) {
                return status;
        }

        *cfg = pid->cfg;
        return FPC_STATUS_OK;
}

enum fpc_status
fpc_pid_set_mode(struct fpc_pid *pid, enum fpc_pid_mode mode,
                 fxp_t manual_output)
{
        enum fpc_status status = fpc_pid_check(pid);
        bool clamped = false;

        if (status != FPC_STATUS_OK) {
                return status;
        }
        if ((mode != FPC_PID_MODE_AUTO) && (mode != FPC_PID_MODE_MANUAL)) {
                return FPC_STATUS_INVALID_PARAM;
        }

        if (mode == FPC_PID_MODE_MANUAL) {
                pid->mode = (uint_least8_t)FPC_PID_MODE_MANUAL;
                pid->manual_output =
                    fpc_pid_clamp(manual_output, pid->cfg.out_min,
                                  pid->cfg.out_max, &clamped);
                pid->last_output = pid->manual_output;
        } else if ((enum fpc_pid_mode)pid->mode == FPC_PID_MODE_MANUAL) {
                /* I = u - Kp e - D, so the next output repeats u. */
                const fxp_acc_t rebias =
                    fpc_pid_widen(pid->last_output)
                    - ((fxp_acc_t)pid->cfg.kp * (fxp_acc_t)pid->prev_error)
                    - fpc_pid_widen(pid->filtered_derivative);
                pid->integral =
                    fpc_pid_clamp_acc(rebias, pid->cfg.integral_min,
                                      pid->cfg.integral_max, &clamped);
                pid->mode = (uint_least8_t)FPC_PID_MODE_AUTO;
        } else {
                /* Already in auto mode. */
        }

        return clamped ? FPC_STATUS_SATURATED : FPC_STATUS_OK;
}

enum fpc_status
fpc_pid_compute(struct fpc_pid *pid, fxp_t setpoint, fxp_t measurement,
                fxp_t *output)
{
        enum fpc_status status = fpc_pid_check(pid);
        bool clamped = false;
        fxp_t error = 0;
        fxp_t error_delta = 0;
        fxp_t raw_derivative = 0;
        fxp_t narrowed = 0;
        fxp_acc_t acc;

        if (output == NULL) {
                return FPC_STATUS_NULL_PTR;
        }
        if (status != FPC_STATUS_OK) {
                return status;
        }

        if (fxp_sub_chk(setpoint, measurement, &error) != FXP_OK) {
                clamped = true;
        }

        if ((enum fpc_pid_mode)pid->mode == FPC_PID_MODE_MANUAL) {
                pid->prev_error = error;
                pid->filtered_derivative = 0;
                *output = pid->last_output;
                return clamped ? FPC_STATUS_SATURATED : FPC_STATUS_OK;
        }

        if (fxp_sub_chk(error, pid->prev_error, &error_delta) != FXP_OK) {
                clamped = true;
        }
        if (fxp_mul_chk(pid->kd_dt, error_delta, &raw_derivative) != FXP_OK) {
                clamped = true;
        }
        pid->filtered_derivative = fxp_lerp(
            pid->filtered_derivative, raw_derivative, pid->cfg.d_filter_alpha);

        /* Anti-windup acts on the wide accumulator, before narrowing. */
        pid->integral = fpc_pid_clamp_acc(
            fxp_acc_mac(pid->integral, pid->ki_dt, error),
            pid->cfg.integral_min, pid->cfg.integral_max, &clamped);

        acc = fxp_acc_mac(pid->integral, pid->cfg.kp, error);
        acc = fxp_acc_mac(acc, pid->filtered_derivative, FXP_ONE);
        if (fxp_acc_narrow_chk(acc, &narrowed) != FXP_OK) {
                clamped = true;
        }

        pid->last_output = fpc_pid_clamp(narrowed, pid->cfg.out_min,
                                         pid->cfg.out_max, &clamped);
        pid->prev_error = error;
        *output = pid->last_output;
        return clamped ? FPC_STATUS_SATURATED : FPC_STATUS_OK;
}

enum fpc_status
fpc_pid_get_state(const struct fpc_pid *pid, struct fpc_pid_state *state)
{
        enum fpc_status status = fpc_pid_check(pid);

        if (state == NULL) {
                return FPC_STATUS_NULL_PTR;
        }
        if (status != FPC_STATUS_OK) {
                return status;
        }

        state->integral = fxp_acc_narrow_sat(pid->integral);
        state->prev_error = pid->prev_error;
        state->filtered_derivative = pid->filtered_derivative;
        state->last_output = pid->last_output;
        state->mode = (enum fpc_pid_mode)pid->mode;
        return FPC_STATUS_OK;
}
