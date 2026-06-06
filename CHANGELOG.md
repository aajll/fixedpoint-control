# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## [Unreleased]

### Changed

- Added security policy, SPDX headers, cleaned ignore rules, explicit `warning_level=3`, and a standard coverage gate while preserving the existing pool-allocator integration.

## [1.3.2] - 2026-05-29

### Added

- Added TI C2000 support notes, expanded CI gates, platform documentation, threading-contract documentation, and local pre-submit guidance.

### Changed

- Bumped the pool-allocator wrap to v2.x, derived pool slot sizing from `fpc_filter_max_order`, and updated configuration/docs around the new option layout.

### Fixed

- Ensured CI enables tests explicitly instead of letting `meson test` pass vacuously when `build_tests` is disabled.

## [1.2.2] - 2026-04-29

### Added

- Added fixed-point PID, FIR, and biquad APIs using static pools, Q16.16 arithmetic, saturation/error reporting, Meson packaging, and TAP unit tests.

### Fixed

- Fixed pool-allocator subproject configuration and documentation for clean out-of-tree builds.
