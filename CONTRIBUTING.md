# Contributing to fixedpoint-control

fixedpoint-control is a safety-oriented fixed-point PID and filter library. It is designed for deterministic, static-memory execution in embedded environments.

## Getting started

The same commands CI runs, locally:

```sh
# Configure with tests + sanitisers (CI default)
meson setup build --buildtype=debug -Dbuild_tests=true \
                  -Db_sanitize=address,undefined
meson compile -C build
meson test -C build --verbose

# Override pool geometry
meson setup build-custom -Dbuild_tests=true \
                          -Dfpc_max_instances=16 -Dfpc_filter_max_order=128
meson compile -C build-custom
meson test -C build-custom --verbose

# Warnings-as-errors build
meson setup build-warn --buildtype=debug -Dbuild_tests=true -Dwerror=true
meson compile -C build-warn

# TI C2000 cross build (library only) — requires a user-provided Meson
# cross file describing your TI codegen install. Place it anywhere Meson
# searches for cross files, or pass an absolute path with --cross-file=.
# Not gated in CI (the GitHub Actions runner does not have the TI toolchain).
meson setup build_c2000 --cross-file=ti-c2000.ini -Dbuild_tests=false
meson compile -C build_c2000
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

The library is designed with compliance in mind (e.g. IEC 61508). It is not formally certified. Contributors are encouraged to follow MISRA C:2023 guidelines to maintain the library's auditable nature. New deviations introduced in a PR should be called out in the description with a justification.

- Avoid pointer arithmetic where possible.
- Ensure all loops have deterministic, compile-time or strictly bounded limits.
- Minimise side effects in functions.

### Running the gates locally

The same gates CI runs, in the same shape:

```sh
# Static analysis (cppcheck)
cppcheck --enable=warning,style,performance,portability --error-exitcode=1 \
         --inline-suppr --std=c11 \
         --suppress=missingIncludeSystem --suppress=unusedFunction \
         -I include -I subprojects/pool-allocator/include \
         src/ include/

# Memcheck (debug build with NO sanitizers)
meson setup build_mem --buildtype=debug -Dbuild_tests=true
meson compile -C build_mem
meson test -C build_mem --verbose \
  --wrapper='valgrind --error-exitcode=1 --leak-check=full \
             --show-leak-kinds=all --track-origins=yes \
             --errors-for-leak-kinds=all'

# Warnings-as-errors
meson setup build_warn --buildtype=debug -Dbuild_tests=true -Dwerror=true
meson compile -C build_warn
```

`cppcheck` and `valgrind memcheck` findings must either be fixed or silenced with a justified inline `/* cppcheck-suppress <id> */` marker.

## Tests and coverage

- Add a test for every bug fix.
- Add a test for every new feature.
- Tests are gated by `-Dbuild_tests=true` (default `false` for a clean library build). All tests must pass: `meson test -C build --verbose`.
- New code must build and pass under ASan + UBSan (`-Db_sanitize=address,undefined`).
- New code must also build under the TI C2000 cross configuration (library target only). The GitHub Actions runner does not have the TI toolchain, so this is a **local pre-submit** check — run it before opening a PR if your change touches `src/` or `include/`.
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
