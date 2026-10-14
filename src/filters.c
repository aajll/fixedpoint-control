/**
 * SPDX-License-Identifier: MIT
 *
 * @file: filters.c
 *
 * @brief
 *    Fixed-point FIR and biquad filter implementations.
 *
 * Implements the FIR and biquad (second-order IIR) filter APIs declared in
 * @ref fpc_filters.h. Each output is accumulated in a wide @c fxp_acc_t with
 * overflow detection and narrowed once through the fxp rounding policy.
 *
 * @note Public API functions are documented in the header; internal helpers are
 *       marked @internal.
 */
#include "../include/fpc_filters.h"
#include <stdbool.h>
#include <stddef.h>

/* ================ INTERNAL HELPERS ===================================== */

/** @internal Check that a FIR configuration can be applied. */
static bool
fpc_fir_config_valid(const struct fpc_fir_config *cfg)
{
        return (cfg != NULL) && (cfg->coeffs != NULL) && (cfg->history != NULL)
               && (cfg->order != 0U);
}

/** @internal Check that a biquad configuration can be applied. */
static bool
fpc_biquad_config_valid(const struct fpc_biquad_config *cfg)
{
        return (cfg != NULL) && (cfg->a1 != FXP_MIN) && (cfg->a2 != FXP_MIN);
}

/** @internal Clear a FIR filter's history; @p fir must be initialised. */
static void
fpc_fir_clear(struct fpc_fir *fir)
{
        uint16_t i;

        for (i = 0U; i < fir->order; ++i) {
                fir->history[i] = 0;
        }
        fir->head = 0U;
}

/** @internal Store a validated FIR configuration and clear its history. */
static void
fpc_fir_apply(struct fpc_fir *fir, const struct fpc_fir_config *cfg)
{
        fir->coeffs = cfg->coeffs;
        fir->history = cfg->history;
        fir->order = cfg->order;
        fpc_fir_clear(fir);
}

/** @internal Clear a biquad filter's delay elements. */
static void
fpc_biquad_clear(struct fpc_biquad *bq)
{
        bq->x1 = 0;
        bq->x2 = 0;
        bq->y1 = 0;
        bq->y2 = 0;
}

/** @internal Store a validated biquad configuration and clear its state. */
static void
fpc_biquad_apply(struct fpc_biquad *bq, const struct fpc_biquad_config *cfg)
{
        bq->b0 = cfg->b0;
        bq->b1 = cfg->b1;
        bq->b2 = cfg->b2;
        /* Exact: the config check excludes FXP_MIN. */
        bq->neg_a1 = -cfg->a1;
        bq->neg_a2 = -cfg->a2;
        fpc_biquad_clear(bq);
}

/** @internal Multiply-accumulate, recording any accumulator overflow. */
static fxp_acc_t
fpc_mac(fxp_acc_t acc, fxp_t a, fxp_t b, bool *overflow)
{
        bool clamped = false;

        acc = fxp_acc_mac_chk(acc, a, b, &clamped);
        if (clamped) {
                *overflow = true;
        }
        return acc;
}

/** @internal Narrow an accumulator, folding in earlier overflow. */
static enum fpc_status
fpc_narrow(fxp_acc_t acc, bool overflow, fxp_t *output)
{
        enum fxp_status st = fxp_acc_narrow_chk(acc, output);

        return (overflow || (st != FXP_OK)) ? FPC_STATUS_SATURATED
                                            : FPC_STATUS_OK;
}

/* ================ FIR ================================================== */

enum fpc_status
fpc_fir_init(struct fpc_fir *fir, const struct fpc_fir_config *cfg)
{
        if (fir == NULL) {
                return FPC_STATUS_NULL_PTR;
        }

        fir->order = 0U;
        if (!fpc_fir_config_valid(cfg)) {
                return FPC_STATUS_INVALID_PARAM;
        }

        fpc_fir_apply(fir, cfg);
        return FPC_STATUS_OK;
}

enum fpc_status
fpc_fir_reset(struct fpc_fir *fir)
{
        if (fir == NULL) {
                return FPC_STATUS_NULL_PTR;
        }
        if (fir->order == 0U) {
                return FPC_STATUS_NOT_INITIALIZED;
        }

        fpc_fir_clear(fir);
        return FPC_STATUS_OK;
}

enum fpc_status
fpc_fir_set_config(struct fpc_fir *fir, const struct fpc_fir_config *cfg)
{
        if (fir == NULL) {
                return FPC_STATUS_NULL_PTR;
        }
        if (fir->order == 0U) {
                return FPC_STATUS_NOT_INITIALIZED;
        }
        if (!fpc_fir_config_valid(cfg)) {
                return FPC_STATUS_INVALID_PARAM;
        }

        fpc_fir_apply(fir, cfg);
        return FPC_STATUS_OK;
}

enum fpc_status
fpc_fir_process(struct fpc_fir *fir, fxp_t sample, fxp_t *output)
{
        fxp_acc_t acc = fxp_acc_zero();
        bool overflow = false;
        uint16_t index;
        uint16_t i;

        if ((fir == NULL) || (output == NULL)) {
                return FPC_STATUS_NULL_PTR;
        }
        if (fir->order == 0U) {
                return FPC_STATUS_NOT_INITIALIZED;
        }

        /* The newest sample sits at head; older samples follow it. */
        fir->head = (fir->head == 0U) ? (uint16_t)(fir->order - 1U)
                                      : (uint16_t)(fir->head - 1U);
        fir->history[fir->head] = sample;

        index = fir->head;
        for (i = 0U; i < fir->order; ++i) {
                acc = fpc_mac(acc, fir->coeffs[i], fir->history[index],
                              &overflow);
                index = (index == (uint16_t)(fir->order - 1U))
                            ? 0U
                            : (uint16_t)(index + 1U);
        }

        return fpc_narrow(acc, overflow, output);
}

/* ================ BIQUAD =============================================== */

enum fpc_status
fpc_biquad_init(struct fpc_biquad *bq, const struct fpc_biquad_config *cfg)
{
        static const struct fpc_biquad zero = {0};

        if (bq == NULL) {
                return FPC_STATUS_NULL_PTR;
        }

        *bq = zero;
        if (!fpc_biquad_config_valid(cfg)) {
                return FPC_STATUS_INVALID_PARAM;
        }

        fpc_biquad_apply(bq, cfg);
        return FPC_STATUS_OK;
}

enum fpc_status
fpc_biquad_reset(struct fpc_biquad *bq)
{
        if (bq == NULL) {
                return FPC_STATUS_NULL_PTR;
        }

        fpc_biquad_clear(bq);
        return FPC_STATUS_OK;
}

enum fpc_status
fpc_biquad_set_config(struct fpc_biquad *bq,
                      const struct fpc_biquad_config *cfg)
{
        if (bq == NULL) {
                return FPC_STATUS_NULL_PTR;
        }
        if (!fpc_biquad_config_valid(cfg)) {
                return FPC_STATUS_INVALID_PARAM;
        }

        fpc_biquad_apply(bq, cfg);
        return FPC_STATUS_OK;
}

enum fpc_status
fpc_biquad_process(struct fpc_biquad *bq, fxp_t sample, fxp_t *output)
{
        fxp_acc_t acc = fxp_acc_zero();
        bool overflow = false;
        enum fpc_status status;

        if ((bq == NULL) || (output == NULL)) {
                return FPC_STATUS_NULL_PTR;
        }

        acc = fpc_mac(acc, bq->b0, sample, &overflow);
        acc = fpc_mac(acc, bq->b1, bq->x1, &overflow);
        acc = fpc_mac(acc, bq->b2, bq->x2, &overflow);
        acc = fpc_mac(acc, bq->neg_a1, bq->y1, &overflow);
        acc = fpc_mac(acc, bq->neg_a2, bq->y2, &overflow);

        status = fpc_narrow(acc, overflow, output);

        bq->x2 = bq->x1;
        bq->x1 = sample;
        bq->y2 = bq->y1;
        bq->y1 = *output;
        return status;
}
