# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## [1.2.2] - 2026-03-14

### Added

- Fixed-point PID controller (`fpc_pid_*`) with proportional, integral, and derivative terms in Q16.16 arithmetic
- Integral windup protection via configurable `integral_min` / `integral_max` bounds
- Derivative term exponential smoothing with configurable Q16.16 alpha coefficient
- Manual/auto mode switching with bumpless transfer (automatic integral rebias on return to auto)
- FIR filter (`fpc_fir_*`) using circular buffer convolution in fixed-point arithmetic
- Biquad IIR filter (`fpc_biquad_*`) implementing second-order section difference equation
- Static pool allocator integration — all instances allocated from compile-time bounded pools (no `malloc` / `free`)
- Separate pool namespaces for PID, FIR, and biquad instances via `FPC_MAX_INSTANCES` per type
- Unified configuration dispatch header (`fpc_config.h`) supporting Meson subproject, pkg-config install, and drag-drop consumption
- Compile-time `_Static_assert` guards ensuring pool slot sizes accommodate all pooled structs
- TAP protocol unit test suite (23 tests) covering initialization, computation, saturation reporting, overflow detection, pool exhaustion, mode switching, and filter correctness — run under ASAN / UBSAN in CI
- Meson build system with static library export, header installation, subproject/wrap consumption, and pkg-config generation

### Fixed

- Pool-allocator subproject configuration for clean out-of-tree builds
- Documentation: corrected meson pool build options and added FIR/biquad examples to README
