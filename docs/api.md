# API reference

`fpc_pid.h` declares the PID controller, `fpc_filters.h` the FIR and biquad filters, and `fpc_status.h` the status codes they share. Both component headers include `fxp.h`. The examples in this document compile together as one file, in order.

```c
#include "fpc_filters.h"
#include "fpc_pid.h"
```

## Conventions

- Every instance is a structure the caller declares, with any storage duration, and passes by pointer. The library has no global state, allocates nothing and never blocks. Treat the members as private: read and change an instance only through the functions.
- Every function returns an `enum fpc_status`, and results come back through out-parameters, so a valid result of 0 is never confused with an error.
- Every pointer argument is checked. A NULL argument never dereferences.
- Values are `fxp_t` in the format fxp is built with. [integration.md](integration.md#number-format) describes the format and the constructors `FXP_INT` and `FXP_C`.
- An instance is not synchronised. Serialise every call on one instance, including `fpc_pid_get_config` and `fpc_pid_get_state`; distinct instances are independent.
- There is no teardown call. An instance owns nothing beyond its own storage and the arrays a FIR filter borrows.

## Status codes

<!-- snippet: skip -->
```c
enum fpc_status {
        FPC_STATUS_OK = 0,
        FPC_STATUS_SATURATED = 1,
        FPC_STATUS_NULL_PTR = -1,
        FPC_STATUS_INVALID_PARAM = -2,
        FPC_STATUS_NOT_INITIALIZED = -3
};
```

A negative value is an error, and the call changed nothing unless its description says otherwise. `FPC_STATUS_SATURATED` is a success: the operation completed and its out-parameter holds a clamped result. Test `status < FPC_STATUS_OK` to catch errors only.

## PID controller

### Configuration

<!-- snippet: skip -->
```c
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

#define FPC_PID_D_FILTER_ALPHA_MAX FXP_ONE
```

- `kp`, `ki` and `kd` are the continuous gains. Any sign is accepted, so a reverse-acting loop uses negative gains.
- `dt` is the sample period, in the time unit of `ki` and `kd`. It multiplies the integral gain and divides the derivative gain; the proportional term does not use it. Choose a unit in which `dt` is exact, such as milliseconds for a 1 kHz loop, where `dt` is `FXP_INT(1)`.
- `out_min` and `out_max` bound the output, inclusive.
- `integral_min` and `integral_max` bound the integral term, in output units, inclusive. They apply to the wide accumulator before it is narrowed, so a reversal in error leaves the bound on the next step. Setting both to 0 disables the integral.
- `d_filter_alpha` smooths the derivative term, above 0 and at most `FPC_PID_D_FILTER_ALPHA_MAX` (1.0). The maximum disables smoothing and smaller values smooth more.

A configuration is rejected with `FPC_STATUS_INVALID_PARAM` when `dt` is not positive, a lower bound exceeds its upper bound, `d_filter_alpha` is outside its range, or a discrete gain `ki × dt` or `kd / dt` saturates or rounds to zero from a nonzero gain. [design.md](design.md#the-control-law) gives the discrete law.

### Initialisation and reset

<!-- snippet: skip -->
```c
enum fpc_status fpc_pid_init(struct fpc_pid *pid, const struct fpc_pid_config *cfg);
enum fpc_status fpc_pid_reset(struct fpc_pid *pid);
```

`fpc_pid_init` applies `cfg` and starts the controller in auto mode with the integral, the derivative term, the error history, the output and the manual output all 0. It returns `FPC_STATUS_NULL_PTR` for a NULL `pid`, and `FPC_STATUS_INVALID_PARAM` for a NULL or rejected `cfg`, in which case the controller is left uninitialised. A zero-initialised controller is also uninitialised; every other function returns `FPC_STATUS_NOT_INITIALIZED` for one.

`fpc_pid_reset` clears the integral, the derivative term and the error history, and keeps the configuration and the mode. The last output becomes the held output in manual mode and 0 in auto mode.

```c
static struct fpc_pid pid;

static const struct fpc_pid_config pid_cfg = {
        .kp = FXP_C(1, 2),
        .ki = FXP_C(1, 4),
        .kd = FXP_C(1, 8),
        .dt = FXP_INT(1),
        .out_min = FXP_INT(-100),
        .out_max = FXP_INT(100),
        .integral_min = FXP_INT(-50),
        .integral_max = FXP_INT(50),
        .d_filter_alpha = FXP_C(1, 2),
};
```

<!-- snippet: body -->
```c
if (fpc_pid_init(&pid, &pid_cfg) != FPC_STATUS_OK) {
        /* the configuration is invalid */
}
```

### Changing and reading the configuration

<!-- snippet: skip -->
```c
enum fpc_status fpc_pid_set_config(struct fpc_pid *pid, const struct fpc_pid_config *cfg);
enum fpc_status fpc_pid_get_config(const struct fpc_pid *pid, struct fpc_pid_config *cfg);
```

`fpc_pid_set_config` applies a new configuration and keeps the runtime state. The integral, the last output and the manual output are clamped to the new bounds, and the call returns `FPC_STATUS_SATURATED` when any of them moved. A NULL or rejected `cfg` returns `FPC_STATUS_INVALID_PARAM` and leaves the controller unchanged.

The integral is stored in output units, so changing `ki` or `dt` changes only how fast it grows from the next step; the output does not jump. A change to `kp` moves the output by the change in `kp` times the error. The derivative term keeps its value and moves towards the new gain at the rate `d_filter_alpha` sets.

`fpc_pid_get_config` copies the applied configuration to `*cfg`. It returns `FPC_STATUS_NULL_PTR` for a NULL `cfg`.

<!-- snippet: body -->
```c
struct fpc_pid_config retune;

if (fpc_pid_get_config(&pid, &retune) == FPC_STATUS_OK) {
        retune.ki = FXP_C(1, 8);
        (void)fpc_pid_set_config(&pid, &retune);
}
```

### Modes

<!-- snippet: skip -->
```c
enum fpc_pid_mode {
        FPC_PID_MODE_AUTO = 0,
        FPC_PID_MODE_MANUAL = 1
};

enum fpc_status fpc_pid_set_mode(struct fpc_pid *pid, enum fpc_pid_mode mode, fxp_t manual_output);
```

Switching to manual mode holds `manual_output`, clamped to the output bounds, clears the derivative term and returns `FPC_STATUS_SATURATED` if the output was clamped. A second call in manual mode replaces the held output. While in manual mode, `fpc_pid_compute` returns the held output and tracks the error, and the derivative term stays 0.

Switching back to auto mode sets the integral to `u − kp × e − ki × dt × e`, clamped to the integral bounds, where `u` is the held output and `e` the last error. The next `fpc_pid_compute` adds the integral step `ki × dt × e`, so for an unchanged error it returns `u`. With `kp` 1, `ki` 0.5, `dt` 1, a held output of 300 and an error of 100, the integral becomes 150 and the first auto output is 300. The call returns `FPC_STATUS_SATURATED` if the integral was clamped, and the first auto output then differs from `u`. Switching to auto mode while in auto mode does nothing, and `manual_output` is then ignored.

A `mode` other than the two enumerators returns `FPC_STATUS_INVALID_PARAM`.

<!-- snippet: body -->
```c
(void)fpc_pid_set_mode(&pid, FPC_PID_MODE_MANUAL, FXP_INT(25));
/* ... the operator holds the output at 25 ... */
(void)fpc_pid_set_mode(&pid, FPC_PID_MODE_AUTO, 0);
```

### Computing a step

<!-- snippet: skip -->
```c
enum fpc_status fpc_pid_compute(struct fpc_pid *pid, fxp_t setpoint, fxp_t measurement, fxp_t *output);
```

`fpc_pid_compute` forms the error `setpoint − measurement`, updates the controller and writes the output to `*output`. It returns `FPC_STATUS_SATURATED` when the error, the change in error, the raw derivative, the integral or the output was clamped, and `*output` then holds the clamped result. It returns `FPC_STATUS_NULL_PTR`, without updating the controller, for a NULL `pid` or `output`.

The derivative acts on the error, so the first step after `fpc_pid_init` or `fpc_pid_reset` sees the whole error as a change from 0 and produces a derivative kick. A change of setpoint does the same.

Call it once per sample period: `dt` is fixed in the configuration, not measured.

<!-- snippet: body -->
```c
fxp_t drive = 0;
const fxp_t target = FXP_INT(40);
const fxp_t measured = FXP_INT(38);

if (fpc_pid_compute(&pid, target, measured, &drive) == FPC_STATUS_SATURATED) {
        /* drive is at a bound */
}
```

### Reading the state

<!-- snippet: skip -->
```c
struct fpc_pid_state {
        fxp_t integral;
        fxp_t prev_error;
        fxp_t filtered_derivative;
        fxp_t last_output;
        enum fpc_pid_mode mode;
};

enum fpc_status fpc_pid_get_state(const struct fpc_pid *pid, struct fpc_pid_state *state);
```

`fpc_pid_get_state` copies the runtime state for diagnostics and tuning. `integral` is the wide accumulator rounded to `fxp_t`, `prev_error` the error of the last step, `filtered_derivative` the derivative term, `last_output` the last output, and `mode` the current mode. It returns `FPC_STATUS_NULL_PTR` for a NULL `state`.

## FIR filter

<!-- snippet: skip -->
```c
struct fpc_fir_config {
        const fxp_t *coeffs;
        fxp_t *history;
        uint16_t order;
};

enum fpc_status fpc_fir_init(struct fpc_fir *fir, const struct fpc_fir_config *cfg);
enum fpc_status fpc_fir_reset(struct fpc_fir *fir);
enum fpc_status fpc_fir_set_config(struct fpc_fir *fir, const struct fpc_fir_config *cfg);
enum fpc_status fpc_fir_process(struct fpc_fir *fir, fxp_t sample, fxp_t *output);
```

The filter computes `y[k] = Σ h[i] × x[k − i]` over `order` taps, from 1 to 65 535, where `coeffs[0]` weights the newest sample. Both arrays are borrowed, not copied: each holds `order` elements, they must not overlap, and they must stay valid while the filter uses them. The library writes only to `history`.

`fpc_fir_init` applies `cfg` and clears the history. A NULL `cfg`, a NULL array or an `order` of 0 returns `FPC_STATUS_INVALID_PARAM` and leaves the filter uninitialised, even if it was initialised before. A zero-initialised filter is uninitialised, and the other three functions return `FPC_STATUS_NOT_INITIALIZED` for one.

`fpc_fir_reset` clears the history. `fpc_fir_set_config` replaces the configuration and clears the history; an invalid `cfg` returns `FPC_STATUS_INVALID_PARAM` and leaves the filter unchanged.

`fpc_fir_process` stores `sample` as the newest input and writes `y[k]` to `*output`. It returns `FPC_STATUS_SATURATED` when the sum, or a partial sum, exceeded its range, and `*output` then holds the saturated result. A saturated result can depend on the order of the taps, because the sum is clamped where it overflows.

This four-tap moving average turns an input of 4.0 into 1.0, 2.0, 3.0 and then 4.0:

```c
static const fxp_t average_coeffs[4] = {FXP_C(1, 4), FXP_C(1, 4), FXP_C(1, 4),
                                        FXP_C(1, 4)};
static fxp_t average_history[4];
static struct fpc_fir average;
```

<!-- snippet: body -->
```c
const struct fpc_fir_config fir_cfg = {
        .coeffs = average_coeffs,
        .history = average_history,
        .order = 4U,
};
fxp_t smoothed = 0;

if (fpc_fir_init(&average, &fir_cfg) == FPC_STATUS_OK) {
        for (int i = 0; i < 4; i++) {
                (void)fpc_fir_process(&average, FXP_INT(4), &smoothed);
        }
}
```

## Biquad filter

<!-- snippet: skip -->
```c
struct fpc_biquad_config {
        fxp_t b0;
        fxp_t b1;
        fxp_t b2;
        fxp_t a1;
        fxp_t a2;
};

enum fpc_status fpc_biquad_init(struct fpc_biquad *bq, const struct fpc_biquad_config *cfg);
enum fpc_status fpc_biquad_reset(struct fpc_biquad *bq);
enum fpc_status fpc_biquad_set_config(struct fpc_biquad *bq, const struct fpc_biquad_config *cfg);
enum fpc_status fpc_biquad_process(struct fpc_biquad *bq, fxp_t sample, fxp_t *output);
```

The filter computes `y[k] = b0 × x[k] + b1 × x[k − 1] + b2 × x[k − 2] − a1 × y[k − 1] − a2 × y[k − 2]`, with `a0` normalised to 1. Neither `a1` nor `a2` may be `FXP_MIN`, which no stable filter needs.

`fpc_biquad_init` applies `cfg` and clears the delay elements. A NULL or invalid `cfg` returns `FPC_STATUS_INVALID_PARAM` and leaves the filter zeroed. A zeroed or zero-initialised biquad is a valid filter whose output is always 0, so the biquad functions never return `FPC_STATUS_NOT_INITIALIZED`.

`fpc_biquad_reset` clears the delay elements. `fpc_biquad_set_config` replaces the coefficients and clears the delay elements; an invalid `cfg` returns `FPC_STATUS_INVALID_PARAM` and leaves the filter unchanged.

`fpc_biquad_process` writes `y[k]` to `*output` and keeps it as the next `y[k − 1]`. It returns `FPC_STATUS_SATURATED` when the sum, or a partial sum, exceeded its range; the saturated output is then also the value fed back.

These coefficients are a second-order Butterworth low-pass with its cut-off at a tenth of the sample rate. Rounded to four decimal places, they give a DC gain of about 1.0004, so a step of 1.0 settles at 1.000443 in Q16.16 within 50 samples:

```c
static const struct fpc_biquad_config lowpass_cfg = {
        .b0 = FXP_C(675, 10000),
        .b1 = FXP_C(1349, 10000),
        .b2 = FXP_C(675, 10000),
        .a1 = FXP_C(-11430, 10000),
        .a2 = FXP_C(4128, 10000),
};
static struct fpc_biquad lowpass;
```

<!-- snippet: body -->
```c
fxp_t filtered = 0;

if (fpc_biquad_init(&lowpass, &lowpass_cfg) == FPC_STATUS_OK) {
        (void)fpc_biquad_process(&lowpass, FXP_INT(1), &filtered);
}
```

## Recipes

### A loop at a fixed rate

Run `fpc_pid_compute` from a periodic interrupt or task at the rate `dt` describes, and express `ki` and `kd` in the same time unit. For a 1 kHz loop with gains per second, `dt` is `FXP_C(1, 1000)`, which Q16.16 rounds to 66 LSB, about 0.7 % long. Expressing the gains per millisecond makes `dt` exactly `FXP_INT(1)`.

### A RAM budget

Check the instance sizes where they are declared, so a format change that outgrows a budget stops the build:

```c
_Static_assert(sizeof(struct fpc_pid) <= 96U, "PID exceeds its RAM budget");
```

`struct fpc_pid` is 80 bytes on x86-64 with the default format. `sizeof` counts 16-bit words on a target where `CHAR_BIT` is 16, so express the budget in the target's units. [design.md](design.md#footprint) gives every size.

## Version

`fpc_status.h` defines the release it belongs to, so a copied-in header still identifies itself:

<!-- snippet: skip -->
```c
#define FPC_VERSION_MAJOR 0
#define FPC_VERSION_MINOR 3
#define FPC_VERSION_PATCH 2
#define FPC_VERSION "0.3.2"
```

`FPC_VERSION_NUMBER` is `MAJOR * 10000 + MINOR * 100 + PATCH` as a `long`, so it holds any version where `int` is 16 bits. Compare it in the preprocessor to require a minimum release:

```c
#if FPC_VERSION_NUMBER < 302L
#error "fixedpoint-control 0.3.2 or later is required"
#endif
```
