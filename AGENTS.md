# AGENTS.md

---

## 1) Project-specific instructions

**Project:** `fixedpoint-control`
**Primary goal:** Safety-oriented fixed-point PID controller and discrete filter (FIR / biquad) library for embedded systems — static memory only, no heap allocation.

### 1.1 Essential commands

#### Configure, build, and run unit tests (host, with sanitisers — matches CI)

```sh
meson setup build --wipe --buildtype=debug -Dbuild_tests=true \
                  -Db_sanitize=address,undefined
meson compile -C build
meson test -C build --verbose
```

#### Override pool geometry

```sh
meson setup build-custom --wipe -Dbuild_tests=true \
                          -Dfpc_max_instances=16 -Dfpc_filter_max_order=128
```

#### Warnings-as-errors build

```sh
meson setup build-warn --wipe --buildtype=debug -Dbuild_tests=true -Dwerror=true
meson compile -C build-warn
```

#### Static analysis (cppcheck) and memory check (Valgrind)

```sh
# cppcheck (CI gate)
cppcheck --enable=warning,style,performance,portability --error-exitcode=1 \
         --inline-suppr --std=c11 \
         --suppress=missingIncludeSystem --suppress=unusedFunction \
         -I include -I subprojects/pool-allocator/include \
         src/ include/

# Valgrind memcheck — build WITHOUT sanitizers, then wrap the test run
meson setup build_mem --wipe --buildtype=debug -Dbuild_tests=true
meson compile -C build_mem
meson test -C build_mem --verbose \
  --wrapper='valgrind --error-exitcode=1 --leak-check=full \
             --show-leak-kinds=all --track-origins=yes \
             --errors-for-leak-kinds=all'
```

#### TI C2000 cross build (library only)

```sh
meson setup build_c2000 --wipe --cross-file=ti-c2000.ini -Dbuild_tests=false
meson compile -C build_c2000
```

The cross file is user-provided. Not gated in CI (no TI toolchain on the runner).

#### Notes

- `meson setup` generates `fpc_version.h` and `fpc_conf.h` into the **build directory**
- Tests are gated by `-Dbuild_tests=true`; default is `false` for a clean library build
- Pool allocator subproject (`pool-allocator`) is fetched automatically on first configure

---

## 2) CI / source of truth

- CI definitions live in `.github/workflows/ci.yml`.
- Prefer running the same commands locally as CI runs (see §1.1 above).
- If `pre-commit` is configured, run `pre-commit run --all-files` before
  committing.

---

## 3) Docs / commit conventions

- Use **Conventional Commits** format when asked to commit.
- Keep commits focused; explain _why_ in the message body.
- **NEVER** commit unless user asks
- Documentation should be forward thinking
- Documentation should not contain references to stale/dead code

---

## 4) C style expectations

- Do **NOT** preserve backward compatibility unless the user explicitly asks for it

### Build & configuration

- Use the Meson build system. Do not introduce CMake, Make, or other systems.
- Update `src/meson.build` when adding or removing source files.

### Formatting

- `.clang-format` is present and **mandatory**. Run `clang-format -i` on all
  modified `.c` / `.h` files before committing.
- Do not reformat unrelated code.
- Key settings: 8-space indent, `BreakBeforeBraces: Linux`, column limit 80.

### Style & correctness

- Match conventions in the existing files (indentation, braces, naming).
- Validate pointer arguments at every public API boundary.
- No heap allocation (`malloc` / `free` / VLAs).
- Use fixed-width types from `<stdint.h>` / `<stdbool.h>` — never plain `int`
  for fixed-width fields. For byte-sized scalar fields use `uint_least8_t`
  (not `uint8_t`) so the code stays portable to targets where `CHAR_BIT == 16`
  (the C11 standard requires `uint8_t` to be exactly 8 bits; on the TI C2000
  it is not defined).

### Error handling

- Public functions return `enum fpc_status`. Outputs are written through
  out-parameters so a valid `FPC_STATUS_OK` is never ambiguous.
- No `errno`; no exceptions.

### Testing

- Run `meson test -C build` after every change.
- Add a test case for each bug fix.
- Tests live in `tests/test_*.c`; all tests must pass.

---
