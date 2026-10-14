# fixedpoint-control

[![CI](https://github.com/aajll/fixedpoint-control/actions/workflows/ci.yml/badge.svg)](https://github.com/aajll/fixedpoint-control/actions/workflows/ci.yml)

Fixed-point PID controller and discrete filter library in C for embedded systems, built on the [fxp](https://github.com/aajll/fxp) arithmetic kernel.

## Features

- **Caller-owned storage** — every PID, FIR and biquad instance is a struct the caller declares; the library has no global state and no heap allocation.
- **Wide accumulation** — sums run in fxp's `fxp_acc_t` and narrow once per output, so sub-LSB contributions such as a slow integral are kept rather than truncated.
- **Saturating arithmetic** — overflow saturates and is reported as `FPC_STATUS_SATURATED`; no path relies on signed overflow or right shifts of negative values.
- **Explicit status handling** — APIs return `enum fpc_status`; outputs are written through out-parameters so a valid `0` result is never ambiguous.
- **PID runtime controls** — anti-windup on the integral accumulator, derivative smoothing, and bumpless manual/automatic mode switching.

## Installation

### Meson subproject

Add this repository as a subproject or wrap. The exported dependency carries the public headers and the fxp dependency, whose header is part of the API.

```meson
fpc_dep = dependency(
  'fixedpoint-control',
  fallback : ['fixedpoint-control', 'fpc_dep']
)
```

### Installed dependency

The project installs a static library, its public headers and a pkg-config file:

```meson
fpc_dep = dependency('fixedpoint-control')
```

## Requirements

- C11 compiler
- Meson and Ninja
- Network access on first configure when Meson fetches the `fxp` wrap

## Number format

Every signal, gain and coefficient is an `fxp_t`. The format and rounding policy are fxp's build-wide settings (`FXP_WORD_BITS`, `FXP_FRAC_BITS`, `FXP_ROUNDING`), which default to Q16.16 with round-to-nearest-even. Use fxp's constructors, such as `FXP_INT(3)` or `FXP_C(1, 5)`, to write values independently of the format.

## Quick Start

### PID controller

```c
#include "fpc_pid.h"

int main(void)
{
        static struct fpc_pid pid;
        const struct fpc_pid_config cfg = {
                .kp = FXP_C(1, 1),
                .ki = FXP_C(1, 2),
                .kd = FXP_C(1, 5),
                .dt = FXP_C(1, 1),
                .out_min = FXP_INT(-100),
                .out_max = FXP_INT(100),
                .integral_min = FXP_INT(-50),
                .integral_max = FXP_INT(50),
                .d_filter_alpha = FPC_PID_D_FILTER_ALPHA_MAX,
        };
        fxp_t output = 0;

        if (fpc_pid_init(&pid, &cfg) != FPC_STATUS_OK) {
                return 1;
        }
        if (fpc_pid_compute(&pid, FXP_INT(10), FXP_INT(9), &output)
            < FPC_STATUS_OK) {
                return 1;
        }
        return 0;
}
```

`dt` multiplies the integral gain and divides the derivative gain; the proportional term does not depend on it. Pick a time unit in which `dt` is exact, such as milliseconds for a 1 kHz loop. The integral is accumulated wide, so `integral_min` and `integral_max` clamp the accumulator itself and a reversal in error leaves the bound on the next step.

### FIR filter

The coefficient and history arrays are borrowed: both hold `order` elements and must outlive the filter.

```c
#include "fpc_filters.h"

int main(void)
{
        static const fxp_t coeffs[3] = {FXP_C(1, 3), FXP_C(1, 3),
                                        FXP_C(1, 3)};
        static fxp_t history[3];
        static struct fpc_fir fir;
        const struct fpc_fir_config cfg = {
                .coeffs = coeffs,
                .history = history,
                .order = 3U,
        };
        fxp_t output = 0;

        if (fpc_fir_init(&fir, &cfg) != FPC_STATUS_OK) {
                return 1;
        }
        if (fpc_fir_process(&fir, FXP_INT(1), &output) < FPC_STATUS_OK) {
                return 1;
        }
        return 0;
}
```

### Biquad filter

```c
#include "fpc_filters.h"

int main(void)
{
        static struct fpc_biquad bq;
        const struct fpc_biquad_config cfg = {
                .b0 = FXP_C(1, 1),
                .b1 = 0,
                .b2 = 0,
                .a1 = 0,
                .a2 = 0,
        };
        fxp_t output = 0;

        if (fpc_biquad_init(&bq, &cfg) != FPC_STATUS_OK) {
                return 1;
        }
        if (fpc_biquad_process(&bq, FXP_INT(1), &output) < FPC_STATUS_OK) {
                return 1;
        }
        return 0;
}
```

## Lifecycle

- `*_init()` validates a configuration and clears the instance state. On failure the instance is left uninitialised, and calls on it return `FPC_STATUS_NOT_INITIALIZED`.
- A zero-initialised PID or FIR instance is uninitialised. A zero-initialised biquad is a valid filter whose output is always zero.
- `*_set_config()` applies a new configuration at runtime; an invalid one leaves the instance unchanged.
- `*_reset()` clears runtime state and keeps the configuration.
- There is no teardown call: an instance owns no resources beyond its own storage.

## Building

```sh
# Library only (release)
meson setup build --buildtype=release -Dbuild_tests=false
meson compile -C build

# With unit tests
meson setup build --buildtype=debug -Dbuild_tests=true
meson compile -C build
meson test -C build --verbose

# Install into a staging directory
meson install -C build --destdir staging
```

## Platform Support

The library works on any C11 toolchain whose `CHAR_BIT` is 8 or 16, with no chip-specific code paths.

| Toolchain                       | Target                | Status                   |
| ------------------------------- | --------------------- | ------------------------ |
| GCC, Clang                      | x86_64 Linux          | Host tests (CI)          |
| GCC, Clang                      | aarch64 Linux         | Compiles via cross       |
| TI C2000 CGT 22.6 and 25.11 LTS | 16-bit-MAU DSP family | Library cross-build (CI) |

CI cross-compiles the library and fxp with warnings as errors under both the COFF and EABI object formats. Byte-sized fields use `uint_least8_t` rather than `uint8_t`, which C11 does not provide when `CHAR_BIT == 16`.

## Notes

- **Thread safety** — instances share no state, so different instances may be used from different contexts freely. Calls on one instance must be serialised by the caller, including the read-only `*_get_config()` and `*_get_state()`.
- **Biquad feedback** — `a1` and `a2` must not be `FXP_MIN`, so that their negation is exact.
- **FIR saturation** — the FIR sum saturates when the accumulator would overflow; once saturated, the result can depend on tap order.
- **Version header** — `fpc_version.h` is generated into the build directory from `config/fpc_version.h.in` and is not source-controlled.
- **Licence** — MIT, see `LICENSE`.
