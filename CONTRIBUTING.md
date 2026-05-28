# Contributing to fixedpoint-control

fixedpoint-control is a safety-oriented fixed-point PID and filter library. It is designed for deterministic, static-memory execution in embedded environments.

## Getting started

The same commands CI runs, locally:

```sh
# Configure, build, and test
meson setup builddir
meson compile -C builddir
meson test -C builddir --verbose

# Override pool geometry
meson setup builddir-custom -Dfpc_max_instances=16 -Dfpc_pool_item_size=640

# Warnings-as-errors build
meson setup builddir-warn -Dwerror=true
meson compile -C builddir-warn
```

## Source style

- `.clang-format` is mandatory. Run `clang-format -i` on every modified `.c` / `.h` file before submitting.
- 8-space indent, Linux brace style, 80-column limit. Match the existing conventions; do not reformat unrelated code.
- The Meson build system is the single source of truth. Update `src/meson.build` or `tests/meson.build` when adding or removing source files.
- Do not introduce CMake, Make, or other build systems.

## C language rules

- C11 only.
- Use fixed-width types from `<stdint.h>` and `<stdbool.h>` (e.g., `uint32_t`, `int16_t`). Never use plain `int` for fixed-width fields.
- No heap allocation (`malloc`, `free`, VLAs).
- Validate pointer arguments at every public API boundary.
- Public functions return `enum fpc_status`. Outputs are written through out-parameters so a valid `FPC_STATUS_OK` result is never ambiguous. No `errno`, no exceptions.

## Compliance & Static Analysis

The library is designed with compliance in mind (e.g., IEC 61508). While the in-repo CI does not currently enforce MISRA checks, contributors are encouraged to follow MISRA C:2012 guidelines to maintain the library's auditable nature.

Recommended additional verification for compliance-focused use:

```sh
# Example warning-focused build
meson setup builddir -Dwerror=true
meson compile -C builddir

# Example external analysis tools
cppcheck --enable=warning,style,performance,portability src include tests
clang --analyze src/pid_controller.c src/filters.c
```

- Avoid pointer arithmetic where possible.
- Ensure all loops have deterministic, compile-time or strictly bounded limits.
- Minimize side effects in functions.

## Tests and coverage

- Add a test for every bug fix.
- Add a test for every new feature.
- All tests must pass: `meson test -C builddir`.
- Tests are always compiled; there is no build-time option to disable them.
- Tests live in `tests/test_*.c`.

## API Stability and Compatibility

This project prioritizes safety and correctness over strict backward compatibility.

- **No automatic backward compatibility**: Unless explicitly requested, breaking changes to the public API are acceptable to improve safety or correctness.
- **Breaking changes**: Major changes should be communicated via the PR description.

## Commits

Use Conventional Commits:

- `feat: ...` new feature
- `fix: ...` bug fix
- `doc: ...` documentation only
- `test: ...` test-only changes
- `chore: ...` build, CI, release work
- `refactor: ...` code change that neither fixes a bug nor adds a feature

Keep the subject under ~70 characters. Use the body to explain _why_ the change is needed, not _what_ the diff already shows.

## Pull requests

- Open an issue first for non-trivial changes so the design can be agreed upon before implementation.
- Keep PRs focused. One feature or one fix per PR.
- The PR description should explain _why_ the change is needed.
- All CI checks must pass.
- Ensure your changes comply with the project's formatting and style requirements.

## When in doubt

Open an issue and discuss before writing code. The library's constraints on memory and determinism mean that design decisions have significant implications for its target use cases.
