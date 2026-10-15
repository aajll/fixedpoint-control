# AGENTS.md

---

## 1) Project-specific instructions

**Project:** `fixedpoint-control` (library `fpc`)
**Primary goal:** A C11 fixed-point PID controller and FIR and biquad filter library built on `fxp`, with caller-owned instances, saturating and reported overflow, no heap, and a bounded cost per sample.

### 1.1 Essential commands

#### Configure and build (library only)

```sh
meson setup build --wipe --buildtype=release -Dbuild_tests=false
meson compile -C build
```

#### Configure, build, and run unit tests (matches CI)

```sh
meson setup build --wipe --buildtype=debug -Dbuild_tests=true -Db_sanitize=address,undefined
meson compile -C build
meson test -C build --verbose
```

#### Warnings as errors, cppcheck and Valgrind

```sh
meson setup build_warn --wipe --buildtype=debug -Dbuild_tests=true -Dwerror=true
meson compile -C build_warn

cppcheck --enable=warning,style,performance,portability --error-exitcode=1 --inline-suppr --std=c11 --suppress=missingIncludeSystem --suppress=unusedFunction --suppress='*:subprojects/*' -I include -I subprojects/fxp/include src/ include/

# Build without sanitisers; add -Dc_link_args=-static if the host's dynamic loader lacks symbols.
meson setup build_mem --wipe --buildtype=debug -Dbuild_tests=true
meson compile -C build_mem
meson test -C build_mem --no-suite docs --wrapper='valgrind --error-exitcode=1 --leak-check=full --show-leak-kinds=all --track-origins=yes --errors-for-leak-kinds=all'
```

#### MISRA C:2023 (reads `build/compile_commands.json`)

```sh
env -u MISRA_RULE_TEXTS misch run --baseline
misch deviations
```

#### Formatting (pinned clang-format through `uvx`)

```sh
scripts/format.sh          # check, as CI does
scripts/format.sh --fix    # rewrite in place
```

Coverage commands are in `CONTRIBUTING.md`.

### 1.2 Notes

- `meson setup` generates `fpc_version.h` into the build directory; no header includes it. The build installs `libfpc.a`, headers under `fixedpoint-control/`, and `fixedpoint-control.pc`. The only Meson option is `build_tests`, default `false`.
- fxp is fetched on the first configure, pinned by commit in `subprojects/fxp.wrap`, and built with its tests and conversion helpers disabled. Its `fxp.h` is part of this library's API.
- The `fixedpoint-control docs examples` test needs fxp's headers in `subprojects/fxp/`, so it is registered only when fxp is a subproject.

---

## 2) Non-negotiable design invariants

- **Caller-owned storage.** Every instance is a structure the caller declares. No heap, no global state, no handle.
- **Arithmetic through fxp.** Sums accumulate in `fxp_acc_t` and narrow once through the build-wide rounding policy. Use the `_chk` or `_sat` forms wherever magnitudes are unbounded, and record the bound in a comment wherever an unchecked form is used.
- **Saturate and report.** No path wraps, shifts a negative value or relies on signed overflow. Every clamp reaches the caller as `FPC_STATUS_SATURATED`.
- **Discrete gains on configure.** `ki × dt` and `kd / dt` are derived and checked when a configuration is applied; a per-sample function divides nothing.
- **Anti-windup on the wide accumulator.** The integral bounds clamp `fxp_acc_t` before narrowing.
- **Bounded cost.** The PID step and the biquad step are constant time; the FIR step is linear in its tap count. Nothing else loops.

---

## 3) CI / source of truth

- CI definitions live in `.github/workflows/ci.yml`.
- Prefer running the same commands locally as CI runs (see §1.1 above).
- Install Python tools (Meson, Ninja, gcovr, misch) with `uv tool install`, never `pip`; CI does the same through `astral-sh/setup-uv`. CI installs `cppcheck` for the MISRA job from Homebrew because Ubuntu's 2.13 reports different findings.
- `misch run --baseline` must pass against `analysis/baseline/misra-baseline.json`. Never regenerate the baseline to admit a new finding. Run it with `MISRA_RULE_TEXTS` unset, as CI does.
- The CI jobs are: tests under ASan/UBSan, a release build, warnings as errors, cppcheck, Valgrind, a 100 percent line and branch coverage gate, a TI C2000 build (CGT 22.6.2 and 25.11.1, COFF and EABI, the library and fxp, warnings as errors), `clang-format`, and `misch`.

---

## 4) Docs / commit conventions

### Documentation layout

- `README.md` is a short landing page (under 100 lines). Put detail in `docs/`, never in the README; link to the owning file instead.
- `docs/integration.md` owns requirements, install methods, the number format, building, and cross builds.
- `docs/api.md` owns the API reference, the status codes and recipes. Change it together with the headers; keep signatures in fenced `c` blocks and describe them in prose, not function tables. Check every value an example states against a build.
- `docs/design.md` is the single authoritative design record: the arithmetic, the control law and its overflow bounds, the modes, the filter forms, storage and concurrency, worst-case cost, footprint, failure behaviour, MISRA position, and verification.
- `CONTRIBUTING.md` documents the contributor workflow; `CHANGELOG.md` holds release history (Keep a Changelog); `.github/SECURITY.md` covers vulnerability reporting.
- Code blocks in `README.md`, `CONTRIBUTING.md`, and `docs/` are compiled by the `fixedpoint-control docs examples` test (`tests/docs/check_snippets.py`). Each document is compiled as one source file and includes the headers itself. A `c` fence is file-level code, `<!-- snippet: body -->` wraps statements, and `<!-- snippet: skip -->` excludes pseudo-code or a copy of a definition. Do not use `skip` to silence a broken example.
- Prose uses British spelling. Do not hard-wrap Markdown: write one line per paragraph. The 80-column limit applies to C source, not Markdown.

### Commits

- Use **Conventional Commits** format when asked to commit.
- Keep commits focused; explain *why* in the message body.

---

## 5) C style expectations

### Build & configuration

- Use the Meson build system. Do not introduce CMake, Make, or other systems.
- Update `meson.build`, `src/meson.build` and `tests/meson.build` when adding or removing files.

### Formatting

- `.clang-format` is present and **mandatory**. Run `scripts/format.sh --fix` before committing. It runs the clang-format release pinned in the script through `uvx`. Do not use a system `clang-format`: another release can format the same code differently.
- Do not reformat unrelated code.
- Key settings: 8-space indent, `BreakBeforeBraces: Linux`, column limit 80.

### Style & correctness

- Match conventions in the existing files (indentation, braces, naming).
- Every public identifier uses the `fpc_` or `FPC_` prefix.
- Validate pointer arguments at every public API boundary. Public functions return `enum fpc_status` and write results through out-parameters; no `errno`.
- Use fixed-width types from `<stdint.h>` and `<stdbool.h>`, never plain `int` for a fixed-width field. Use `uint_least8_t` for byte-sized fields, because `uint8_t` does not exist where `CHAR_BIT` is 16.
- A change to an instance layout needs a `CHANGELOG.md` entry.

### MISRA deviations

- Prefer compliant code; never restructure only to hide a finding. Deviate at the point of use with `/* cppcheck-suppress misra-c2012-<rule> ; @deviation <rationale> */`, or project-wide in `analysis/deviations/misra-deviations.txt` for a house-style rule.

### Comment placement (Doxygen)

- Inline trailing annotations (`/**< ... */`) on `enum`/`struct` members are allowed only when the resulting line fits the 80-column limit.
- If any member's annotation would overrun, move **all** of that aggregate's member docs into a single structured Doxygen block above the type, as an `@details` list of `- ::SYMBOL  description` entries.
- Never mix inline and block forms within one aggregate, and never leave a trailing comment that clang-format would wrap onto a second line.
- After editing, run `scripts/format.sh` and an 80-column scan.

### Testing

- Run `meson test -C build` after every change; all tests must pass.
- Tests live in `tests/test_*.c` and run as one executable through `tests/test_main.c`. Do not add Unity or another external framework.
- Expected values are exact integers derived from the arithmetic.
- Add a test case for each bug fix and each new feature.
- Keep `src/` and `include/` at the coverage gate (100 percent line and branch).
