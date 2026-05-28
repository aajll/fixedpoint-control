# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## [1.3.2]

### Added

- TI C2000 family is now a first-class supported target. The library compiles cleanly with the TI C2000 CGT codegen (validated against 25.11 LTS) using the auto-selected `volatile` atomicity path inherited from pool-allocator. The GitHub Actions runner does not ship the TI toolchain, so the C2000 cross-build is a documented local pre-submit check rather than a CI gate.
- "Platform Support" section in `README.md` listing validated toolchains, targets, and the 16-bit MAU portability story.
- "Threading contract" file-level docblock in `include/fpc_pid.h` and `include/fpc_filters.h` documenting the inherited single-writer / many-readers contract.
- CI workflow expansion to match pool-allocator's quality bar. New jobs / axes: `test` matrix over `OS × fpc_filter_max_order` (Ubuntu + macOS, default and a larger order) with `-Db_sanitize=address,undefined`; `release-build` matrix on the same axes without sanitizers; `werror` gate (`-Dwerror=true`); `cppcheck` gate (`warning + style + performance + portability`, `--error-exitcode=1`, inline-suppression handling); `valgrind memcheck` gate (`--leak-check=full --errors-for-leak-kinds=all`, built without sanitizers because ASan and Valgrind are incompatible).
- `CONTRIBUTING.md` "Running the gates locally" subsection mirroring CI commands verbatim, plus a TI C2000 local pre-submit note.

### Changed (BREAKING)

- Pool-allocator wrap bumped to **v2.0.0** (was v1.x). fpc inherits pool-allocator's breaking changes: per-slot status flags are `_Atomic uint_least8_t` (or `volatile` on toolchains without `<stdatomic.h>`); the pool's `struct pool_t` layout changes between `c11` and `volatile` modes; consumers must rebuild against the new headers. See `subprojects/pool-allocator/CHANGELOG.md` for the full pool 2.0 details.
- Meson option layout reshaped. `fpc_pool_item_size` is **removed**; `fpc_filter_max_order` is **added**. The pool slot size is now derived from `fpc_filter_max_order` via `ceil_to_align(8 + 8 * order, 16)`, so the Meson and C views of the slot size are kept in lock-step from a single source. Downstream consumers passing `-Dfpc_pool_item_size=N` on the CLI will get an unknown-option error and must switch to `-Dfpc_filter_max_order=N` (or, if a consumer-defined struct needs more space, override `FPC_POOL_ITEM_SIZE` on the compiler command line directly).
- `FPC_POOL_ITEM_SIZE` is derived in `include/fpc_config.h` from `FPC_FILTER_MAX_ORDER` using the same formula. The `#ifndef`-guard remains, so consumers can still override it for the rare advanced case where a consumer-defined struct needs more space than the formula provides; the `_Static_assert` in the source files is the authoritative check.
- Internal scalar fields in `struct fpc_pid`, `struct fpc_fir`, and `struct fpc_biquad` that previously used `uint8_t` now use `uint_least8_t`. Layout is bit-identical on byte-addressable hosts (host tests pass unchanged); on TI C2000 the fields occupy one 16-bit MAU each, which is the only option the platform offers. C11 requires `uint8_t` to be exactly 8 bits, so the type is not provided when `CHAR_BIT == 16`.
- README, CONTRIBUTING, AGENTS, and `config/fpc_conf_template.h` brought in line with the new option layout. Stale references to `fpc_pool_item_size`, the "tests are always compiled" claim, and the incorrect "public functions return `bool`" line in AGENTS are all corrected.

### Fixed

- `.github/workflows/ci.yml` previously ran `meson setup builddir` without `-Dbuild_tests=true`. `build_tests` defaults to `false`, so `tests/meson.build` was skipped entirely and `meson test` was exiting 0 vacuously. The new workflow enables tests and sanitizers across the matrix.

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
