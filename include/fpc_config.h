/**
 * @file fpc_config.h
 * @brief Central configuration header for fixedpoint-control.
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
/* Explicit path supplied via -DFPC_CONF_PATH=\"...\" */
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

/** Size in bytes of each pool slot (must be >= largest controller struct
 *  and a multiple of `_Alignof(max_align_t)`, typically 16). */
#ifndef FPC_POOL_ITEM_SIZE
#define FPC_POOL_ITEM_SIZE (528U)
#endif

/** Maximum FIR filter order (number of taps). */
#ifndef FPC_FILTER_MAX_ORDER
#define FPC_FILTER_MAX_ORDER (64U)
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
