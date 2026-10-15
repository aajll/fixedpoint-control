# Design

This document records how the controller and the filters compute their outputs, what they guarantee, and how that is checked. [api.md](api.md) describes the interface.

## Goals

- Deterministic arithmetic in a fixed-point format chosen at build time, with no floating point and no undefined behaviour on any input.
- Every overflow saturates and is reported, never wraps.
- Caller-owned instances with no heap, no global state and a size known at compile time.
- A bounded cost per sample: constant for the PID controller and the biquad, linear in the number of taps for the FIR filter.
- The same source on hosts and on targets where `CHAR_BIT` is 16.

## Arithmetic

Every operation goes through fxp. Values are `fxp_t`; products and sums of products are kept in `fxp_acc_t`, a signed integer twice the width of `fxp_t`, scaled by `FXP_ONE` squared. A product of two `fxp_t` is exact in `fxp_acc_t`, so a sum of products loses nothing until it is narrowed back to `fxp_t`.

Narrowing rounds by the policy fxp is built with, `FXP_ROUNDING`, and saturates to `[FXP_MIN, FXP_MAX]`. Each operation uses fxp's checked form, which writes the clamped result and reports the clamp, so every saturation reaches the caller as `FPC_STATUS_SATURATED`. No code path shifts a negative value or relies on signed overflow.

## The control law

The controller implements the continuous law `u(t) = Kp e(t) + Ki ∫ e dt + Kd de/dt`, sampled every `dt`:

- `e[k] = setpoint − measurement`
- `I[k] = clamp(I[k − 1] + (Ki dt) e[k], integral_min, integral_max)`
- `R[k] = (Kd / dt) (e[k] − e[k − 1])`
- `D[k] = D[k − 1] + α (R[k] − D[k − 1])`
- `u[k] = clamp(I[k] + Kp e[k] + D[k], out_min, out_max)`

where `α` is `d_filter_alpha`, in (0, 1.0].

### Discrete gains

The discrete gains `Ki dt` and `Kd / dt` are computed once when a configuration is applied, so the step itself has no division. A configuration is rejected if either saturates, or if either rounds to zero from a nonzero gain, because the term would then have no effect. The proportional gain is used as given.

### Integral and anti-windup

The integral is held in `fxp_acc_t` and never rounded, so a contribution below one LSB per step accumulates instead of vanishing. It stores the integral term in output units, not the sum of errors, so a change of `Ki` or `dt` changes only its future growth.

The integral bounds clamp the wide accumulator itself, before narrowing. Once the error reverses, the integral leaves the bound on the next step, with no stored excess to unwind. The output bounds are separate; an integral that is not bounded tighter than the output can still wind up to its own bounds while the output is saturated.

The unchecked wide sums cannot overflow in any fxp format. With W-bit words, fxp guarantees `FXP_FRAC_BITS ≤ W − 2`, so the integral and the widened derivative are each at most 2^(2W−3) in magnitude, and a product of two `fxp_t` at most 2^(2W−2). A sum of two such terms and one product stays within the 2W-bit accumulator, so `I + Kp e + D`, the integral step and the mode-switch bias are each formed without a check and narrowed once.

### Derivative

The derivative acts on the error, so a step in the setpoint, and the first step after initialisation or reset, when `e[k − 1]` is 0, produce a derivative kick. `R[k]` is narrowed and checked; `D[k]` is formed by `fxp_lerp`, a first-order low-pass that rounds once per step. With `α` at 1.0 the derivative term is the raw derivative. A configuration with `α` at 0 is rejected, because the term would then never change from its value when the configuration was applied.

### Modes

Manual mode holds an output, clamped to the output bounds. Each step in manual mode records the error and sets `D` to 0, so the derivative restarts from the tracked error.

Returning to auto mode sets `I = u − Kp e[k − 1] − D`, clamped to the integral bounds, where `u` is the held output. The bias omits the integral step that the next `fpc_pid_compute` adds before forming its output, so for an unchanged error the first auto output is `u + (Ki dt) e` rather than `u`. When `Ki` is 0 the transfer is exact.

## FIR filter

The filter keeps its input history in a caller-provided array used as a ring. Each sample moves the head back one position and overwrites the oldest input there, so `coeffs[0]` always meets the newest sample and the history is never shifted. The output is one checked multiply-accumulate per tap in `fxp_acc_t`, narrowed once.

The accumulator saturates at the wide bound when a partial sum overflows and stays flagged, so the result is reported as saturated even if later taps bring the sum back into range. A saturated result therefore depends on where in the sum the overflow happened, which depends on the order of the taps. With the default format the accumulator holds about ±2^31, and one product at most 2^30, so an overflow needs several taps near full scale.

The arrays are borrowed. The filter keeps their addresses and the tap count, so a configuration can switch arrays without copying.

## Biquad filter

The biquad is direct form I: it keeps the last two inputs and the last two outputs. It stores `−a1` and `−a2`, negated when the configuration is applied, so the step is five multiply-accumulates of one sign. The negation is exact because `FXP_MIN` is rejected.

The saturated output is what is stored as `y[k − 1]`, so a filter driven into saturation feeds back the clamped value. That bounds the stored state, at the cost of a response that differs from the unbounded filter until the saturation clears. Direct form I needs no internal headroom beyond the output, because the only stored values are inputs and outputs.

## Storage and concurrency

Each instance is a plain structure. The library keeps no state outside it, so separate instances can be used from separate contexts with no coordination. Calls on one instance must be serialised by the caller, because a step reads and writes several members.

A FIR filter borrows its arrays. The caller must keep them valid, must not write `history` while the filter is in use, and may share one `coeffs` array between filters.

## Worst-case cost

| Function | Cost |
| --- | --- |
| `fpc_pid_compute` | Constant: two subtractions, one multiply, one interpolation, three multiply-accumulates, one narrow and two clamps. No loops and no division. |
| `fpc_pid_init`, `fpc_pid_set_config` | Constant, with one multiply and one division to derive the gains. |
| `fpc_fir_process` | One multiply-accumulate per tap. |
| `fpc_fir_init`, `fpc_fir_reset`, `fpc_fir_set_config` | One store per tap to clear the history. |
| `fpc_biquad_process` | Constant: five multiply-accumulates and one narrow. |

A multiply-accumulate is a multiply of two `fxp_t` into `fxp_acc_t`. With the default 32-bit word that is a 64-bit product, which a target without a native one computes with a runtime library call.

## Footprint

Sizes with the default Q16.16 format:

| Type | x86-64 | Target where `CHAR_BIT` is 16 |
| --- | --- | --- |
| `fxp_t`, `fxp_acc_t` | 4 and 8 bytes | 2 and 4 words |
| `struct fpc_pid_config` | 36 bytes | 18 words |
| `struct fpc_pid` | 80 bytes: 36 of configuration, 8 of derived gains, 8 of integral, 16 of state, 1 of mode, 11 of padding | 36 words: 18 of configuration, 4 of derived gains, 4 of integral, 8 of state, 1 of mode, 1 of padding |
| `struct fpc_fir` | 24 bytes: two pointers, two `uint16_t` and 4 of padding | 6 words: two pointers and two `uint16_t` |
| `struct fpc_biquad` | 36 bytes | 18 words |

A FIR filter also uses its two arrays, each `order` elements of `fxp_t`. On a target where `CHAR_BIT` is 16, `sizeof` counts 16-bit words. The target figures are the same for both compiler releases and both object formats in CI.

## Failure behaviour

| Condition | Result |
| --- | --- |
| NULL instance or out-parameter | `FPC_STATUS_NULL_PTR`; nothing changes. |
| NULL or rejected configuration to `init` | `FPC_STATUS_INVALID_PARAM`; a PID or FIR instance is left uninitialised, a biquad zeroed. |
| NULL or rejected configuration to `set_config` | `FPC_STATUS_INVALID_PARAM`; the instance is unchanged. |
| Any call but `init` on an uninitialised PID or FIR instance | `FPC_STATUS_NOT_INITIALIZED`; nothing changes. |
| Invalid mode to `fpc_pid_set_mode` | `FPC_STATUS_INVALID_PARAM`; nothing changes. |
| Overflow anywhere in a step | The step completes with the clamped value and returns `FPC_STATUS_SATURATED`. |
| State clamped by `fpc_pid_set_config` or `fpc_pid_set_mode` | The call completes and returns `FPC_STATUS_SATURATED`. |

A PID controller is initialised once its stored `dt` is positive, and a FIR filter once its stored order is nonzero, so a zero-initialised instance is uninitialised. A zeroed biquad is a valid filter with all coefficients 0, so the biquad has no uninitialised state. The library does not detect a FIR array that has been freed or overwritten.

## MISRA C:2023

The library is analysed with [`misch`](https://pypi.org/project/misch/), a cppcheck-based MISRA C:2023 checker, over `src/` and `include/`. fxp is outside the scope, but its macros expand into the analysed code. The baseline of accepted findings is `analysis/baseline/misra-baseline.json`, and CI fails on any finding outside it. The version macros in `fpc_status.h` carry an inline deviation of Rule 2.5 (unused macro), because they exist for consumers and the library does not use them.

| Rule | Findings | Cause |
| --- | --- | --- |
| 15.5 | 34 | Early returns on NULL arguments, uninitialised instances and rejected configurations. |
| 8.7 | 15 | Public functions with external linkage that the analysed code does not call from another file. |
| 10.1 | 5 | `FXP_ONE` and `FXP_MIN`, whose fxp definitions shift and negate signed operands. |
| 17.8 | 1 | The multiply-accumulate helper in `filters.c` updates its accumulator parameter. |

The total is 55, 21 in `filters.c` and 34 in `pid_controller.c`. The baseline records the findings as they stand. Formal compliance needs a certified analyser and a deviation record from the consuming project.

## Verification

The tests run on the host:

| Test | What it checks |
| --- | --- |
| `fixedpoint-control unit tests` | 15 PID cases: configuration checks, the effect of `dt` on each term, sub-LSB integration, anti-windup on the accumulator, saturation reporting at the extremes, derivative smoothing, state readback, reset, retuning, manual mode and the return to auto. 11 filter cases: FIR sums, tap order, rounding of negative values, reset and reconfiguration, saturation and invalid use, and the biquad difference equation, a zeroed instance, saturation and invalid use. |
| `fixedpoint-control docs examples` | Every C block in the README, `CONTRIBUTING.md` and `docs/` compiles against the headers. |

The bumpless-transfer test uses `Ki` 0, where the transfer is exact.

CI runs the unit tests under AddressSanitizer and UndefinedBehaviorSanitizer and again under Valgrind, builds with warnings as errors and in release mode, gates line and branch coverage of `src/` and `include/` at 100 percent, runs cppcheck with its warning, style, performance and portability checks, checks formatting, and runs the MISRA analysis. It also compiles the library and fxp with warnings as errors under TI C2000 CGT 22.6.2 and 25.11.1, in both the COFF and EABI formats.
