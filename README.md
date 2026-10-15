# fixedpoint-control

[![CI](https://github.com/aajll/fixedpoint-control/actions/workflows/ci.yml/badge.svg)](https://github.com/aajll/fixedpoint-control/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)

**A C11 fixed-point PID controller and FIR and biquad filters for embedded systems, built on the [fxp](https://github.com/aajll/fxp) arithmetic kernel, with caller-owned instances, saturating arithmetic and no heap.**

## Quick start

Add the repository as a Meson subproject; the [integration guide](docs/integration.md) shows how. Meson fetches fxp on the first configure.

```c
#include <stdio.h>

#include "fpc_pid.h"

int
main(void)
{
        static struct fpc_pid pid;
        const struct fpc_pid_config cfg = {
                .kp = FXP_C(1, 2),
                .ki = FXP_C(1, 4),
                .kd = 0,
                .dt = FXP_INT(1),
                .out_min = FXP_INT(-22),
                .out_max = FXP_INT(22),
                .integral_min = FXP_INT(-50),
                .integral_max = FXP_INT(50),
                .d_filter_alpha = FPC_PID_D_FILTER_ALPHA_MAX,
        };
        fxp_t output = 0;

        if (fpc_pid_init(&pid, &cfg) != FPC_STATUS_OK) {
                return 1;
        }

        /* Prints "step 1: 15 (0)", "step 2: 20 (0)", then "step 3: 22 (1)",
         * FPC_STATUS_SATURATED, because 25 exceeds out_max. */
        for (int step = 1; step <= 3; step++) {
                const enum fpc_status st =
                    fpc_pid_compute(&pid, FXP_INT(100), FXP_INT(80), &output);

                printf("step %d: %ld (%d)\n", step, (long)(output / FXP_ONE),
                       (int)st);
        }
        return 0;
}
```

Each step adds `ki × dt × error` = 5 to the integral, and the proportional term adds `kp × error` = 10.

## Description

Every signal, gain and coefficient is an `fxp_t`, in the format and rounding policy fxp is built with (Q16.16 and round-to-nearest-even by default). Each instance is a structure the caller declares and passes by pointer, so the library keeps no global state.

- A PID controller with anti-windup on the integral, derivative smoothing, and manual and automatic modes, whose discrete gains `ki × dt` and `kd / dt` are derived and checked once per configuration
- FIR filters of up to 65 535 taps over caller-provided coefficient and history arrays, and direct form I biquads
- Sums accumulated in fxp's wide `fxp_acc_t` and rounded once per output, so contributions below one LSB per step are kept
- Overflow saturates and returns `FPC_STATUS_SATURATED`; no path relies on signed overflow or right-shifts a negative value
- Tests under ASan, UBSan and Valgrind with 100% line and branch coverage, and MISRA C:2023 analysis with [`misch`](https://pypi.org/project/misch/)

## Validated toolchains

| Toolchain               | Target                        | Validation                                 |
| ----------------------- | ----------------------------- | ------------------------------------------ |
| GCC                     | x86_64 Linux                  | All tests                                  |
| DSP vendor C11 compiler | 16-bit MAU (`CHAR_BIT == 16`) | Library and fxp compile, two object formats |

Target linking and execution belong to the consuming firmware. See [cross builds](docs/integration.md#cross-builds).

## Documentation

- [Integration guide](docs/integration.md) — Meson subproject, installed package, the number format, building, and cross builds
- [API reference](docs/api.md) — the PID controller, the FIR and biquad filters, status codes, and recipes
- [Design](docs/design.md) — the discrete PID law, the derived gains, accumulation and saturation, mode switching, the filter forms, the footprint, failure behaviour, and verification
- [Changelog](CHANGELOG.md) — release history

## Contributing

Read [CONTRIBUTING.md](CONTRIBUTING.md) before you open a pull request. Report security issues as described in [SECURITY.md](.github/SECURITY.md).

## License

MIT. See [LICENSE](LICENSE).
