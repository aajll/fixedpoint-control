# Integration guide

The library is two C sources and three public headers built on [fxp](https://github.com/aajll/fxp), whose `fxp_t` type is part of the API. The examples in this document compile together as one file, in order.

## Requirements

- A C11 compiler. The sources assume no `uint8_t` and build where `CHAR_BIT` is 16.
- Meson and Ninja.
- Network access on the first configure, when Meson fetches the fxp wrap pinned in `subprojects/fxp.wrap`. A system fxp found through pkg-config is used instead when one is installed.

## Meson subproject

Clone or wrap this repository into the consumer's `subprojects/` and request the dependency with a fallback:

```meson
fpc_dep = dependency(
  'fixedpoint-control',
  fallback: ['fixedpoint-control', 'fpc_dep'],
)
executable('app', 'main.c', dependencies: fpc_dep)
```

`fpc_dep` carries the include paths and links both `libfpc.a` and fxp, so the consumer includes `fpc_pid.h` or `fpc_filters.h` directly. Meson resolves fxp through the wrap inside this project, so the consumer does not need its own `fxp.wrap`. A consumer that also uses fxp directly should declare the same fxp revision, because a build holds one copy of it. The project calls `meson.override_dependency('fixedpoint-control', fpc_dep)`, so other subprojects that request the dependency by name get the same one.

fxp is configured with its tests and its floating-point conversion helpers disabled.

## Installed package

`meson install` installs `libfpc.a` and the public headers under `include/fixedpoint-control/`, and `fixedpoint-control.pc`. When fxp was built as a subproject it installs alongside, with its headers under `include/fxp/` and `fxp.pc`.

```sh
meson setup build --buildtype=release
meson compile -C build
meson install -C build --destdir staging
```

The pkg-config file adds `include/fixedpoint-control/` to the include path and requires `fxp`, so `pkg-config --cflags --libs fixedpoint-control` gives both include directories and `-lfpc -lfxp`. A Meson consumer writes `dependency('fixedpoint-control')`. `fpc_status.h`, which both public headers include, defines the version macros described in [api.md](api.md#version). `fpc_version.h` is installed as a deprecated header that includes it.

## Number format

Every signal, gain, coefficient and bound is an `fxp_t`. Its format and rounding policy are fxp's build-wide settings:

| Macro | Default | Effect |
| --- | --- | --- |
| `FXP_WORD_BITS` | `32` | Storage width, 16 or 32 bits. |
| `FXP_FRAC_BITS` | `16` | Fractional bits, from 1 to `FXP_WORD_BITS - 2`. |
| `FXP_ROUNDING` | `FXP_ROUND_NEAREST_EVEN` | Rounding on every narrowing step; also `FXP_ROUND_TRUNCATE` or `FXP_ROUND_NEAREST_UP`. |

The defaults give Q16.16 with a resolution of 2⁻¹⁶ and a range of about ±32768. Override a setting for the whole build with the top-level `c_args`, which reaches fxp, this library and the consumer alike:

```sh
meson setup build -Dc_args=-DFXP_FRAC_BITS=24
```

Every translation unit must see the same values, because `fxp_t` and the structures change with them. Write constants with fxp's constructors so they hold in any format: `FXP_INT(n)` for an integer, `FXP_C(num, den)` for a ratio rounded at compile time, and `FXP_ONE` for 1.0.

```c
#include "fpc_filters.h"
#include "fpc_pid.h"

static const fxp_t gain = FXP_C(3, 4);   /* 0.75 */
static const fxp_t limit = FXP_INT(100); /* 100.0 */
```

## Build and test

```sh
# Library only
meson setup build --buildtype=release
meson compile -C build

# All tests under ASan and UBSan
meson setup build --buildtype=debug -Dbuild_tests=true -Db_sanitize=address,undefined
meson compile -C build
meson test -C build --verbose
```

The only Meson option is `build_tests`, which defaults to `false`. The test suite runs the unit tests and compiles every C block in the documentation. [CONTRIBUTING.md](../CONTRIBUTING.md) lists the coverage, Valgrind, MISRA and formatting commands.

## Cross builds

The consuming firmware owns its cross file, linker command file, and memory placement. No cross file ships with this project.

CI compiles the library and fxp with warnings as errors using two releases of a vendor C11 compiler for a 16-bit-MAU DSP target, under both of its object formats. This covers the source dialect and type assumptions only; linking and execution remain checks for the consumer.

On such a target `CHAR_BIT` is 16 and `sizeof` counts 16-bit words. With the default format, `fxp_t` is two words and `fxp_acc_t` four. [design.md](design.md#footprint) gives the size of each instance there.
