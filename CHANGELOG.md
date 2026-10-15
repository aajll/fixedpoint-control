# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## [Unreleased]

### Added

- `docs/integration.md`, `docs/api.md` and `docs/design.md`, covering installation and the number format, the API, the discrete control law and its overflow bounds, the modes, the filter forms, the footprint, failure behaviour, the MISRA baseline and verification. The README is now a short landing page with a quick start whose output is checked, and a `fixedpoint-control docs examples` test compiles every C block in the README, `CONTRIBUTING.md` and `docs/`.

### Changed

- Versioning restarts in the 0.x series while the API is under active development: the 1.2.2 and 1.3.2 releases are re-published as 0.2.2 and 0.3.2 on the same commits, and minor releases may contain breaking changes until 1.0.0.
- Added security policy, SPDX headers, cleaned ignore rules and explicit `warning_level=3`.
- **Breaking:** PID, FIR and biquad instances are caller-owned structs (`struct fpc_pid`, `struct fpc_fir`, `struct fpc_biquad`) passed by pointer to `*_init()`, instead of pointers handed out from internal pools. FIR history is a caller-provided buffer of `order` elements.
- **Breaking:** signals, gains and coefficients are `fxp_t` from the `fxp` library, whose header is now part of the public API. `fpc_pid_config` is all `fxp_t`, including `d_filter_alpha`.
- **Breaking:** `dt` multiplies the integral gain, divides the derivative gain and leaves the proportional term unchanged, matching the documented continuous equation. Configurations whose derived gains saturate or round to zero are rejected.
- FIR and biquad outputs narrow through fxp's build-wide rounding policy (round-to-nearest-even by default) instead of an arithmetic right shift, which rounded towards negative infinity. Expected values move by one LSB where the floor lost a fraction: the three-tap moving average of 1000, 2000, 2000 gives 333, 1000, 1667 (previously 333, 999, 1666; exact values 333.328, 999.985, 1666.641), and the same filter fed 900 after a reset gives 300 (previously 299; exact value 299.995).
- The coverage gate is 100% of lines and branches.
- CI cross-compiles the library and fxp with TI C2000 CGT 22.6.2 and 25.11.1 under COFF and EABI, with warnings as errors.
- CI installs its Python tools with uv, checks formatting with the clang-format release pinned in `scripts/format.sh`, and runs a MISRA C:2023 analysis with misch against a recorded baseline. The Meson files and tests carry SPDX headers.

### Fixed

- The PID integral accumulates in a wide `fxp_acc_t` and narrows once per output, so contributions below one LSB per step add up instead of truncating to zero; anti-windup clamps that accumulator.
- FIR and biquad sums saturate and return `FPC_STATUS_SATURATED` on overflow, including with `INT32_MIN` coefficients and samples, instead of overflowing a signed integer.
- No arithmetic path right-shifts a negative value.

### Removed

- `fpc_pid_pool_init()`, `fpc_filter_pool_init()` and the `*_deinit()` functions.
- The `pool-allocator` dependency, `fpc_config.h`, the generated `fpc_conf.h`, and the `fpc_max_instances` and `fpc_filter_max_order` options.
- `FPC_STATUS_POOL_FULL` and `FPC_STATUS_OVERFLOW`; the remaining status values keep their numbers.

## [0.3.2] - 2026-05-29

### Added

- Added TI C2000 support notes, expanded CI gates, platform documentation, threading-contract documentation, and local pre-submit guidance.

### Changed

- Bumped the pool-allocator wrap to v2.x, derived pool slot sizing from `fpc_filter_max_order`, and updated configuration/docs around the new option layout.

### Fixed

- Ensured CI enables tests explicitly instead of letting `meson test` pass vacuously when `build_tests` is disabled.

## [0.2.2] - 2026-04-29

### Added

- Added fixed-point PID, FIR, and biquad APIs using static pools, Q16.16 arithmetic, saturation/error reporting, Meson packaging, and TAP unit tests.

### Fixed

- Fixed pool-allocator subproject configuration and documentation for clean out-of-tree builds.
