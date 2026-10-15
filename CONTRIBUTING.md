# Contributing to fixedpoint-control

`fixedpoint-control` is a C11 fixed-point PID controller and FIR and biquad filter library built on the `fxp` arithmetic kernel, for deterministic execution in caller-owned memory. An arithmetic change that is off by one LSB, or that wraps instead of saturating, shows only at the extremes of the format, so read [design.md](docs/design.md) before you change a computation.

## Getting started

Install the Python tools with [uv](https://docs.astral.sh/uv/), as CI does. The MISRA analysis also needs a current `cppcheck`; CI installs it from Homebrew because the 2.13 release in Ubuntu 24.04 reports different findings. The same commands CI runs, locally:

```sh
# Python tools
uv tool install meson
uv tool install ninja
uv tool install gcovr
uv tool install misch

# Configure with tests and sanitisers (CI default)
meson setup build --buildtype=debug -Dbuild_tests=true -Db_sanitize=address,undefined
meson compile -C build
meson test -C build --verbose

# Warnings as errors
meson setup build_warn --buildtype=debug -Dbuild_tests=true -Dwerror=true
meson compile -C build_warn

# Coverage (CI gate is 100% line and branch)
meson setup build_cov --buildtype=debug -Dbuild_tests=true -Db_coverage=true
meson compile -C build_cov && meson test -C build_cov
gcovr --root . --filter 'src/' --filter 'include/' --fail-under-line 100 --fail-under-branch 100 --print-summary build_cov

# Valgrind memcheck: a build without sanitisers; the docs test runs the Python checker, so skip it.
# Add -Dc_link_args=-static if the host's dynamic loader lacks symbols.
meson setup build_mem --buildtype=debug -Dbuild_tests=true
meson compile -C build_mem
meson test -C build_mem --no-suite docs --wrapper='valgrind --error-exitcode=1 --leak-check=full --show-leak-kinds=all --track-origins=yes --errors-for-leak-kinds=all'

# cppcheck
cppcheck --enable=warning,style,performance,portability --error-exitcode=1 --inline-suppr --std=c11 --suppress=missingIncludeSystem --suppress=unusedFunction --suppress='*:subprojects/*' -I include -I subprojects/fxp/include src/ include/

# MISRA C:2023 against the baseline (reads build/compile_commands.json)
env -u MISRA_RULE_TEXTS misch run --baseline
misch deviations

# Formatting (pinned clang-format through uvx)
scripts/format.sh --fix
```

A cross build needs a Meson cross file for the target compiler; the `c2000` job in `.github/workflows/ci.yml` writes a working one.

## Source style

- Formatting is mandatory. Run `scripts/format.sh --fix` before submitting, or `scripts/format.sh` to check without writing, as CI does. The script pins the clang-format release and runs it through `uvx`, so an editor formatter gives the same result only when it runs the same release.
- 8-space indent, Linux brace style, 80-column limit. Match the existing conventions and do not reformat unrelated code.
- The Meson build system is the single source of truth. Update `meson.build`, `src/meson.build` and `tests/meson.build` when you add or remove files. No CMake, no Make, and no other build systems.

## C language rules

- C11 only.
- Use fixed-width types from `<stdint.h>` and `<stdbool.h>`; never plain `int` for a fixed-width field. Use `uint_least8_t` for a byte-sized field, because `uint8_t` does not exist where `CHAR_BIT` is 16.
- No heap allocation (`malloc`, `free`, VLAs) and no global state.
- Validate every pointer argument at the public API boundary.
- Public functions return `enum fpc_status` and write results through out-parameters, so a valid result is never mistaken for an error. No `errno`.
- Public identifiers use the `fpc_` or `FPC_` prefix.

## Design rules

- Instances are caller-owned structures. The library keeps no global state, so there is no capacity to configure, no allocator dependency and no handle that outlives its storage. The members are visible only so callers can allocate them; treat them as private.
- All arithmetic goes through `fxp`. Sums accumulate in `fxp_acc_t` and narrow once through the build-wide rounding policy. Use the `_chk` or `_sat` forms wherever operand magnitudes are not bounded, and record the bound in a comment wherever an unchecked form is used.
- Every saturation reaches the caller as `FPC_STATUS_SATURATED`. Loops are bounded by the FIR tap count; nothing else loops.
- A per-sample function divides nothing. Derive anything that needs a division when the configuration is applied.

## MISRA C:2023

The library is analysed with `misch` (cppcheck-backed MISRA C:2023 analysis), configured by `misra.toml`. CI fails on any finding that is not in `analysis/baseline/misra-baseline.json`. Run it from a configured `build/` directory, with `MISRA_RULE_TEXTS` unset, as CI does. If your change introduces a finding:

1. Prefer restructuring the code so the rule is satisfied. Deviate only when compliance would make the code genuinely worse, and never restructure purely to hide a finding from the checker; an honest deviation beats evasion.
2. Suppress at the point of use with `/* cppcheck-suppress misra-c2012-<rule> ; @deviation <rationale> */`, or add a justified project-wide entry to `analysis/deviations/misra-deviations.txt` for a house-style rule.
3. Verify with `misch run --baseline` and `misch deviations`, which requires every suppression to carry a rationale, and justify the deviation in the pull request description.

Do not regenerate the baseline to admit a new finding. It changes only after a deliberate review, with `misch baseline`. `cppcheck` and Valgrind findings must be fixed or silenced with a justified inline `/* cppcheck-suppress <id> */` marker.

## Tests and coverage

- Add a test for every bug fix and every new feature.
- Tests live in `tests/test_*.c` and run as one executable through `tests/test_main.c`. Register a new case with `run_test` in its file.
- Expected values are exact integers derived from the arithmetic, with a comment wherever the derivation is not obvious.
- CI enforces 100 percent line and branch coverage of `src/` and `include/`. Every new branch needs a test that fails when the branch is broken.
- New code must build where `CHAR_BIT` is 16. CI compiles the library and fxp with warnings as errors on two releases of a vendor cross compiler for such a target, under both of its object formats.
- The `fixedpoint-control docs examples` test compiles every C code block in `README.md`, `CONTRIBUTING.md`, and `docs/` through `tests/docs/check_snippets.py`. Each document is one source file and includes the headers itself, so its first block includes them. A block compiles at file scope by default. On the line directly above the fence, put `<!-- snippet: body -->` for a block of statements, or `<!-- snippet: skip -->` for pseudo-code or a copy of a header definition.

## Documentation

| File | Purpose |
| --- | --- |
| `README.md` | Landing page: quick start, description, validated toolchains, and links. Keep it short. |
| `docs/integration.md` | Requirements, installation, the number format, building, and cross builds. |
| `docs/api.md` | Reference for the PID controller, the FIR and biquad filters, the status codes, and recipes. |
| `docs/design.md` | The arithmetic, the control law and its overflow bounds, the modes, the filter forms, storage and concurrency, worst-case cost, the footprint, failure behaviour, MISRA position, and verification. |

Update `docs/api.md` and the header comments together when an interface changes, and check every value an example states against a build. Update `docs/design.md` when a computation, a bound or the verification changes, and the MISRA table when the baseline changes. Keep it standalone: describe how the library works, not how it was built or which other projects use it. Write prose in British English, one line per paragraph.

## API stability

- The function signatures, the configuration structures and the status codes are stable within a major release line.
- Rounding and saturation are observable behaviour. A change that alters an output for the same inputs needs a `CHANGELOG.md` entry, and a major release if it changes a signature.
- The members of an instance are private, but its size is not: callers allocate it, so a layout change needs a `CHANGELOG.md` entry.

## Commits

Use Conventional Commits:

- `feat: ...` new feature
- `fix: ...` bug fix
- `docs: ...` documentation only
- `test: ...` test-only changes
- `chore: ...` build, CI, release work
- `refactor: ...` a change that neither fixes a bug nor adds a feature

Keep the subject under about 70 characters. Use the body to explain why the change is needed.

## Pull requests

- Open an issue first for non-trivial changes, and always for a change to the control law or a filter form, so the design can be agreed before implementation.
- Keep pull requests focused: one feature or one fix.
- All CI checks must pass: tests under ASan and UBSan, Valgrind, warnings as errors, release build, coverage gate, cppcheck, the TI C2000 build, clang-format, and MISRA analysis against the baseline.
- Add a `CHANGELOG.md` entry under `## [Unreleased]` for a user-visible change.
- Flag any new MISRA deviation, and any change to an instance layout, in the description.

## When in doubt

Open an issue. The library's constraints on memory and determinism mean that design decisions have significant implications for its target use cases.
