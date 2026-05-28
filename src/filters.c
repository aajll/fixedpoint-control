/**
 * @file filters.c
 * @brief Fixed-point FIR and biquad filter implementations.
 *
 * Implements the FIR and biquad (second‑order IIR) filter APIs declared in
 * @ref fpc_filters.h. All filters use Q16.16 fixed‑point arithmetic and share a
 * common pool allocator configured via @ref fpc_config.h.
 *
 * @note Public API functions are documented in the header; internal helpers are
 *       marked @internal.
 */
#include "../include/fpc_config.h"
#include "../include/fpc_filters.h"
#include "pool.h"
#include <limits.h>
#include <stdbool.h>
#include <string.h>

/** @internal */
struct fpc_fir {
        pool_id_t pool_id;
        uint16_t order;
        uint16_t head;
        uint_least8_t initialized;
        uint_least8_t reserved[1];
        int32_t coeffs[FPC_FILTER_MAX_ORDER];
        int32_t history[FPC_FILTER_MAX_ORDER];
};

struct fpc_biquad {
        pool_id_t pool_id;
        uint_least8_t initialized;
        uint_least8_t reserved[1];
        int32_t b0;
        int32_t b1;
        int32_t b2;
        int32_t a1;
        int32_t a2;
        int32_t x1;
        int32_t x2;
        int32_t y1;
        int32_t y2;
};

#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L)
_Static_assert(sizeof(struct fpc_fir) <= POOL_ITEM_SIZE,
               "POOL_ITEM_SIZE too small for struct fpc_fir");
_Static_assert(sizeof(struct fpc_biquad) <= POOL_ITEM_SIZE,
               "POOL_ITEM_SIZE too small for struct fpc_biquad");
#endif

static struct pool_t fpc_fir_pool_storage = {0};
static pool_handle_t fpc_fir_pool_handle = NULL;

static struct pool_t fpc_biquad_pool_storage = {0};
static pool_handle_t fpc_biquad_pool_handle = NULL;

/**
 * @brief Validate FIR filter configuration parameters.
 *
 * @param cfg Configuration to validate.
 *
 * @return bool true if configuration is valid, false otherwise.
 *
 * @pre cfg may be NULL.
 * @post Returns false if cfg is NULL or if any parameter is invalid.
 * @note Valid configuration requires:
 *   - cfg is non-NULL
 *   - coeffs is non-NULL
 *   - order is in range [1, FPC_FILTER_MAX_ORDER]
 */
static bool
fpc_fir_config_is_valid(const struct fpc_fir_config *cfg)
{
        if (cfg == NULL) {
                return false;
        }

        if (cfg->coeffs == NULL) {
                return false;
        }

        if ((cfg->order == 0U) || (cfg->order > FPC_FILTER_MAX_ORDER)) {
                return false;
        }

        return true;
}

/**
 * @brief Validate biquad filter configuration parameters.
 *
 * @param cfg Configuration to validate.
 *
 * @return bool true if configuration is valid, false otherwise.
 *
 * @pre cfg may be NULL.
 * @post Returns false only if cfg is NULL.
 * @note Biquad filters require no parameter validation beyond NULL checks.
 */
static bool
fpc_biquad_config_is_valid(const struct fpc_biquad_config *cfg)
{
        return (cfg != NULL);
}

/**
 * @brief Validate and retrieve a writable FIR filter pointer.
 *
 * @param ctx Filter handle to validate.
 * @param valid_ctx Pointer to store the validated handle (output).
 *
 * @return enum fpc_status Status code indicating validation result.
 *
 * @retval FPC_STATUS_OK if ctx is valid and initialized.
 * @retval FPC_STATUS_NULL_PTR if ctx or valid_ctx is NULL.
 * @retval FPC_STATUS_NOT_INITIALIZED if pool or filter is not initialized.
 *
 * @pre Pool must be initialized via fpc_filter_pool_init().
 * @post *valid_ctx is set to ctx on success.
 * @note This function ensures the filter handle corresponds to an actual
 *       allocated pool slot and has been initialized.
 */
static enum fpc_status
fpc_fir_get_valid_pointer(struct fpc_fir *ctx, struct fpc_fir **valid_ctx)
{
        void *pool_ptr = NULL;

        if ((ctx == NULL) || (valid_ctx == NULL)) {
                return FPC_STATUS_NULL_PTR;
        }

        if (fpc_fir_pool_handle == NULL) {
                return FPC_STATUS_NOT_INITIALIZED;
        }

        if (ctx->initialized == 0U) {
                return FPC_STATUS_NOT_INITIALIZED;
        }

        pool_ptr = pool_get_pointer(fpc_fir_pool_handle, ctx->pool_id);
        if (pool_ptr != (void *)ctx) {
                return FPC_STATUS_NOT_INITIALIZED;
        }

        *valid_ctx = ctx;
        return FPC_STATUS_OK;
}

/**
 * @brief Validate and retrieve a writable biquad filter pointer.
 *
 * @param ctx Filter handle to validate.
 * @param valid_ctx Pointer to store the validated handle (output).
 *
 * @return enum fpc_status Status code indicating validation result.
 *
 * @retval FPC_STATUS_OK if ctx is valid and initialized.
 * @retval FPC_STATUS_NULL_PTR if ctx or valid_ctx is NULL.
 * @retval FPC_STATUS_NOT_INITIALIZED if pool or filter is not initialized.
 *
 * @pre Pool must be initialized via fpc_filter_pool_init().
 * @post *valid_ctx is set to ctx on success.
 * @note This function ensures the filter handle corresponds to an actual
 *       allocated pool slot and has been initialized.
 */
static enum fpc_status
fpc_biquad_get_valid_pointer(struct fpc_biquad *ctx,
                             struct fpc_biquad **valid_ctx)
{
        void *pool_ptr = NULL;

        if ((ctx == NULL) || (valid_ctx == NULL)) {
                return FPC_STATUS_NULL_PTR;
        }

        if (fpc_biquad_pool_handle == NULL) {
                return FPC_STATUS_NOT_INITIALIZED;
        }

        if (ctx->initialized == 0U) {
                return FPC_STATUS_NOT_INITIALIZED;
        }

        pool_ptr = pool_get_pointer(fpc_biquad_pool_handle, ctx->pool_id);
        if (pool_ptr != (void *)ctx) {
                return FPC_STATUS_NOT_INITIALIZED;
        }

        *valid_ctx = ctx;
        return FPC_STATUS_OK;
}

/**
 * @brief Apply configuration to a FIR filter instance.
 *
 * @param ctx Filter instance to configure.
 * @param cfg Configuration structure with coefficients.
 *
 * @return enum fpc_status Status code indicating success or failure.
 *
 * @pre ctx and cfg must be valid pointers.
 * @post Filter order, coefficient buffer, and history are updated.
 * @note This is an internal helper; configuration validation is done by caller.
 */
static enum fpc_status
fpc_fir_apply_config(struct fpc_fir *ctx, const struct fpc_fir_config *cfg)
{
        if (!fpc_fir_config_is_valid(cfg)) {
                return FPC_STATUS_INVALID_PARAM;
        }

        ctx->order = cfg->order;
        ctx->head = 0U;
        (void)memcpy(ctx->coeffs, cfg->coeffs,
                     (size_t)cfg->order * sizeof(int32_t));
        (void)memset(ctx->history, 0, sizeof(ctx->history));
        return FPC_STATUS_OK;
}

/**
 * @brief Apply configuration to a biquad filter instance.
 *
 * @param ctx Filter instance to configure.
 * @param cfg Configuration structure with coefficients.
 *
 * @return enum fpc_status Status code indicating success or failure.
 *
 * @pre ctx and cfg must be valid pointers.
 * @post All filter coefficients and state variables are updated.
 * @note This is an internal helper; configuration validation is done by caller.
 */
static enum fpc_status
fpc_biquad_apply_config(struct fpc_biquad *ctx,
                        const struct fpc_biquad_config *cfg)
{
        if (!fpc_biquad_config_is_valid(cfg)) {
                return FPC_STATUS_INVALID_PARAM;
        }

        ctx->b0 = cfg->b0;
        ctx->b1 = cfg->b1;
        ctx->b2 = cfg->b2;
        ctx->a1 = cfg->a1;
        ctx->a2 = cfg->a2;
        ctx->x1 = 0;
        ctx->x2 = 0;
        ctx->y1 = 0;
        ctx->y2 = 0;
        return FPC_STATUS_OK;
}

/**
 * @copydoc fpc_filter_pool_init()
 */
enum fpc_status
fpc_filter_pool_init(void)
{
        if (pool_init(&fpc_fir_pool_storage) != POOL_OK) {
                return FPC_STATUS_NOT_INITIALIZED;
        }

        if (pool_init(&fpc_biquad_pool_storage) != POOL_OK) {
                return FPC_STATUS_NOT_INITIALIZED;
        }

        fpc_fir_pool_handle = &fpc_fir_pool_storage;
        fpc_biquad_pool_handle = &fpc_biquad_pool_storage;
        return FPC_STATUS_OK;
}

/**
 * @copydoc fpc_fir_init()
 */
enum fpc_status
fpc_fir_init(struct fpc_fir **ctx, const struct fpc_fir_config *cfg)
{
        pool_id_t pool_id;
        void *pool_ptr = NULL;
        struct fpc_fir *fir = NULL;
        pool_status_t pool_status;
        enum fpc_status status;

        if (ctx == NULL) {
                return FPC_STATUS_NULL_PTR;
        }

        *ctx = NULL;

        if (!fpc_fir_config_is_valid(cfg)) {
                return FPC_STATUS_INVALID_PARAM;
        }

        if (fpc_fir_pool_handle == NULL) {
                return FPC_STATUS_NOT_INITIALIZED;
        }

        pool_status = pool_acquire(fpc_fir_pool_handle, &pool_id);
        if (pool_status == POOL_ERR_FULL) {
                return FPC_STATUS_POOL_FULL;
        }

        if (pool_status != POOL_OK) {
                return FPC_STATUS_NOT_INITIALIZED;
        }

        pool_status =
            pool_get_pointer_checked(fpc_fir_pool_handle, pool_id, &pool_ptr);
        if (pool_status != POOL_OK) {
                (void)pool_release(fpc_fir_pool_handle, pool_id);
                return FPC_STATUS_NOT_INITIALIZED;
        }

        fir = (struct fpc_fir *)pool_ptr;
        fir->pool_id = pool_id;
        fir->initialized = 1U;
        status = fpc_fir_apply_config(fir, cfg);
        if (status != FPC_STATUS_OK) {
                (void)pool_release(fpc_fir_pool_handle, pool_id);
                return status;
        }

        *ctx = fir;
        return FPC_STATUS_OK;
}

/**
 * @copydoc fpc_fir_reset()
 */
enum fpc_status
fpc_fir_reset(struct fpc_fir *ctx)
{
        struct fpc_fir *valid_ctx = NULL;
        enum fpc_status status;

        status = fpc_fir_get_valid_pointer(ctx, &valid_ctx);
        if (status != FPC_STATUS_OK) {
                return status;
        }

        valid_ctx->head = 0U;
        (void)memset(valid_ctx->history, 0, sizeof(valid_ctx->history));
        return FPC_STATUS_OK;
}

/**
 * @copydoc fpc_fir_set_config()
 */
enum fpc_status
fpc_fir_set_config(struct fpc_fir *ctx, const struct fpc_fir_config *cfg)
{
        struct fpc_fir *valid_ctx = NULL;
        enum fpc_status status;

        status = fpc_fir_get_valid_pointer(ctx, &valid_ctx);
        if (status != FPC_STATUS_OK) {
                return status;
        }

        return fpc_fir_apply_config(valid_ctx, cfg);
}

/**
 * @copydoc fpc_fir_process()
 */
enum fpc_status
fpc_fir_process(struct fpc_fir *ctx, int32_t sample, int32_t *output)
{
        struct fpc_fir *valid_ctx = NULL;
        enum fpc_status status;
        int64_t acc = 0;
        uint16_t i;
        uint16_t index;
        int64_t shifted_output;

        if (output == NULL) {
                return FPC_STATUS_NULL_PTR;
        }

        *output = 0;

        status = fpc_fir_get_valid_pointer(ctx, &valid_ctx);
        if (status != FPC_STATUS_OK) {
                return status;
        }

        valid_ctx->history[valid_ctx->head] = sample;
        index = valid_ctx->head;

        for (i = 0U; i < valid_ctx->order; ++i) {
                acc += (int64_t)valid_ctx->coeffs[i]
                       * (int64_t)valid_ctx->history[index];

                if (index == 0U) {
                        index = (uint16_t)(valid_ctx->order - 1U);
                } else {
                        index--;
                }
        }

        valid_ctx->head++;
        if (valid_ctx->head >= valid_ctx->order) {
                valid_ctx->head = 0U;
        }

        shifted_output = acc >> 16;
        if ((shifted_output > (int64_t)INT32_MAX)
            || (shifted_output < (int64_t)INT32_MIN)) {
                *output = (shifted_output > 0) ? INT32_MAX : INT32_MIN;
                return FPC_STATUS_OVERFLOW;
        }

        *output = (int32_t)shifted_output;
        return FPC_STATUS_OK;
}

/**
 * @copydoc fpc_fir_deinit()
 */
enum fpc_status
fpc_fir_deinit(struct fpc_fir *ctx)
{
        struct fpc_fir *valid_ctx = NULL;
        enum fpc_status status;

        status = fpc_fir_get_valid_pointer(ctx, &valid_ctx);
        if (status != FPC_STATUS_OK) {
                return status;
        }

        if (pool_release(fpc_fir_pool_handle, valid_ctx->pool_id) != POOL_OK) {
                return FPC_STATUS_NOT_INITIALIZED;
        }

        return FPC_STATUS_OK;
}

/**
 * @copydoc fpc_biquad_init()
 */
enum fpc_status
fpc_biquad_init(struct fpc_biquad **ctx, const struct fpc_biquad_config *cfg)
{
        pool_id_t pool_id;
        void *pool_ptr = NULL;
        struct fpc_biquad *biquad = NULL;
        pool_status_t pool_status;
        enum fpc_status status;

        if (ctx == NULL) {
                return FPC_STATUS_NULL_PTR;
        }

        *ctx = NULL;

        if (!fpc_biquad_config_is_valid(cfg)) {
                return FPC_STATUS_INVALID_PARAM;
        }

        if (fpc_biquad_pool_handle == NULL) {
                return FPC_STATUS_NOT_INITIALIZED;
        }

        pool_status = pool_acquire(fpc_biquad_pool_handle, &pool_id);
        if (pool_status == POOL_ERR_FULL) {
                return FPC_STATUS_POOL_FULL;
        }

        if (pool_status != POOL_OK) {
                return FPC_STATUS_NOT_INITIALIZED;
        }

        pool_status = pool_get_pointer_checked(fpc_biquad_pool_handle, pool_id,
                                               &pool_ptr);
        if (pool_status != POOL_OK) {
                (void)pool_release(fpc_biquad_pool_handle, pool_id);
                return FPC_STATUS_NOT_INITIALIZED;
        }

        biquad = (struct fpc_biquad *)pool_ptr;
        biquad->pool_id = pool_id;
        biquad->initialized = 1U;
        status = fpc_biquad_apply_config(biquad, cfg);
        if (status != FPC_STATUS_OK) {
                (void)pool_release(fpc_biquad_pool_handle, pool_id);
                return status;
        }

        *ctx = biquad;
        return FPC_STATUS_OK;
}

/**
 * @copydoc fpc_biquad_reset()
 */
enum fpc_status
fpc_biquad_reset(struct fpc_biquad *ctx)
{
        struct fpc_biquad *valid_ctx = NULL;
        enum fpc_status status;

        status = fpc_biquad_get_valid_pointer(ctx, &valid_ctx);
        if (status != FPC_STATUS_OK) {
                return status;
        }

        valid_ctx->x1 = 0;
        valid_ctx->x2 = 0;
        valid_ctx->y1 = 0;
        valid_ctx->y2 = 0;
        return FPC_STATUS_OK;
}

/**
 * @copydoc fpc_biquad_set_config()
 */
enum fpc_status
fpc_biquad_set_config(struct fpc_biquad *ctx,
                      const struct fpc_biquad_config *cfg)
{
        struct fpc_biquad *valid_ctx = NULL;
        enum fpc_status status;

        status = fpc_biquad_get_valid_pointer(ctx, &valid_ctx);
        if (status != FPC_STATUS_OK) {
                return status;
        }

        return fpc_biquad_apply_config(valid_ctx, cfg);
}

/**
 * @copydoc fpc_biquad_process()
 */
enum fpc_status
fpc_biquad_process(struct fpc_biquad *ctx, int32_t sample, int32_t *output)
{
        struct fpc_biquad *valid_ctx = NULL;
        enum fpc_status status;
        int64_t acc;
        int64_t shifted_output;

        if (output == NULL) {
                return FPC_STATUS_NULL_PTR;
        }

        *output = 0;

        status = fpc_biquad_get_valid_pointer(ctx, &valid_ctx);
        if (status != FPC_STATUS_OK) {
                return status;
        }

        acc = ((int64_t)valid_ctx->b0 * (int64_t)sample)
              + ((int64_t)valid_ctx->b1 * (int64_t)valid_ctx->x1)
              + ((int64_t)valid_ctx->b2 * (int64_t)valid_ctx->x2)
              - ((int64_t)valid_ctx->a1 * (int64_t)valid_ctx->y1)
              - ((int64_t)valid_ctx->a2 * (int64_t)valid_ctx->y2);

        shifted_output = acc >> 16;
        if ((shifted_output > (int64_t)INT32_MAX)
            || (shifted_output < (int64_t)INT32_MIN)) {
                *output = (shifted_output > 0) ? INT32_MAX : INT32_MIN;
                return FPC_STATUS_OVERFLOW;
        }

        *output = (int32_t)shifted_output;
        valid_ctx->x2 = valid_ctx->x1;
        valid_ctx->x1 = sample;
        valid_ctx->y2 = valid_ctx->y1;
        valid_ctx->y1 = *output;
        return FPC_STATUS_OK;
}

/**
 * @copydoc fpc_biquad_deinit()
 */
enum fpc_status
fpc_biquad_deinit(struct fpc_biquad *ctx)
{
        struct fpc_biquad *valid_ctx = NULL;
        enum fpc_status status;

        status = fpc_biquad_get_valid_pointer(ctx, &valid_ctx);
        if (status != FPC_STATUS_OK) {
                return status;
        }

        if (pool_release(fpc_biquad_pool_handle, valid_ctx->pool_id)
            != POOL_OK) {
                return FPC_STATUS_NOT_INITIALIZED;
        }

        return FPC_STATUS_OK;
}
