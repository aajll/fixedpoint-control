/**
 * SPDX-License-Identifier: MIT
 *
 * @file: fpc_conf.h
 *
 * @brief
 *    User configuration file for fixedpoint-control.
 *
 * Copy this file next to your project's include path and rename it to
 * `fpc_conf.h`.  Set the first `#if 0` to `#if 1` to enable its content,
 * then adjust the values below to match your application.
 *
 * Three ways to provide this configuration (in priority order):
 *
 *  1. Pass `-DFPC_CONF_PATH="path/to/your/fpc_conf.h"` on the compiler
 *     command line.  The path is used verbatim inside an `#include`.
 *  2. Place an `fpc_conf.h` file so that `#include "fpc_conf.h"` resolves
 *     (e.g. next to the `fixedpoint-control/` include directory, or via
 *     a `-I` flag).
 *  3. Do nothing — sensible defaults are compiled in.
 *
 * When building with Meson, the build system generates the configuration
 * automatically from `meson_options.txt`; you do not need this file.
 */

/* clang-format off */
#if 0 /* Set this to "1" to enable content */

#ifndef FPC_CONF_H_
#define FPC_CONF_H_

/*====================
 * POOL CONFIGURATION
 *====================*/

/**
 * Maximum number of simultaneously active PID / FIR / biquad instances.
 * Each instance occupies one slot in the internal pool allocator.
 */
#define FPC_MAX_INSTANCES       8

/*====================
 * FILTER CONFIGURATION
 *====================*/

/**
 * Maximum FIR filter order (number of taps). Drives the derived pool slot
 * size — `struct fpc_fir`'s coefficient and history arrays are sized by
 * this value.
 */
#define FPC_FILTER_MAX_ORDER    64U

/*====================
 * ADVANCED OVERRIDE
 *====================*/

/**
 * Size of each pool slot. Derived from FPC_FILTER_MAX_ORDER by
 * `include/fpc_config.h`; the `_Static_assert` in the source files is the
 * authoritative check.
 *
 * Override **only** if a consumer-defined struct sharing the pool needs more
 * space than the default formula provides. Leave undefined under normal use.
 */
/* #define FPC_POOL_ITEM_SIZE   1024U */

#endif /* FPC_CONF_H_ */
#endif
/* clang-format on */
