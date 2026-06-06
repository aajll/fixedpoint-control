/**
 * SPDX-License-Identifier: MIT
 *
 * @file: fpc_config.h
 *
 * @brief
 *    Central configuration header for fixedpoint-control.
 *
 * This header resolves all compile-time configuration for the library.
 * It checks three sources in priority order:
 *
 *   1. A build-system-generated header (Meson `configure_file()` writes
 *      `fpc_conf.h` into the build directory, which is on the include path).
 *   2. A user-supplied `fpc_conf.h` placed on the include path (for
 *      drag-drop / embedded use without a build system).
 *   3. Built-in defaults (defined below).
 *
 * Users may also override individual values from the compiler command line,
 * e.g. `-DFPC_MAX_INSTANCES=4`, because every define is guarded by `#ifndef`.
 *
 * @see config/fpc_conf_template.h for a copy-and-edit starting point.
 */

#ifndef FPC_CONFIG_H_
#define FPC_CONFIG_H_

/* ------------------------------------------------------------------ */
/*  Step 1: Pull in user / build-system configuration (if present)    */
/* ------------------------------------------------------------------ */

#if defined(FPC_CONF_PATH)
/* Explicit path supplied via -DFPC_CONF_PATH=\"...\". The cppcheck
 * suppression below silences a false positive: cppcheck explores both
 * preprocessor branches and, when FPC_CONF_PATH is undefined, the
 * `#include` line looks empty to it. The actual code path is unreachable
 * unless the consumer explicitly defines the macro. */
/* cppcheck-suppress preprocessorErrorDirective */
#include FPC_CONF_PATH

#elif defined(__has_include)
#if __has_include("fpc_conf.h")
#include "fpc_conf.h"
#endif
#endif

/* ------------------------------------------------------------------ */
/*  Step 2: Apply defaults for anything not yet defined               */
/* ------------------------------------------------------------------ */

/** Maximum number of simultaneously active PID / FIR / biquad instances. */
#ifndef FPC_MAX_INSTANCES
#define FPC_MAX_INSTANCES (8U)
#endif

/** Maximum FIR filter order (number of taps). Drives the pool slot size. */
#ifndef FPC_FILTER_MAX_ORDER
#define FPC_FILTER_MAX_ORDER (64U)
#endif

/** Size of each pool slot, derived from the largest user-visible struct
 *  (`struct fpc_fir` ≈ 8 header bytes + 2 * int32_t * order). Rounded up
 *  to a 16-byte boundary, which is the conservative upper bound for
 *  `_Alignof(max_align_t)` on supported targets. Override only if a
 *  consumer-defined struct sharing the pool needs more space; the
 *  `_Static_assert` in the source files is the authoritative check. */
#ifndef FPC_POOL_ITEM_SIZE
#define FPC_POOL_ITEM_SIZE                                                     \
        ((((8U + (8U * FPC_FILTER_MAX_ORDER)) + 15U) / 16U) * 16U)
#endif

/* ------------------------------------------------------------------ */
/*  Step 3: Bridge to pool allocator's expected macro names           */
/* ------------------------------------------------------------------ */

#ifndef POOL_MAX_SLOTS
#define POOL_MAX_SLOTS FPC_MAX_INSTANCES
#endif

#ifndef POOL_ITEM_SIZE
#define POOL_ITEM_SIZE FPC_POOL_ITEM_SIZE
#endif

#endif /* FPC_CONFIG_H_ */
