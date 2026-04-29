# AGENTS.md

---

## 1) Project-specific instructions

**Project:** `fixedpoint-control`
**Primary goal:** Safety-oriented fixed-point PID controller and discrete filter (FIR / biquad) library for embedded systems — static memory only, no heap allocation.

### 1.1 Essential commands

#### Configure, build, and run unit tests

```sh
meson setup builddir
meson compile -C builddir
meson test -C builddir --verbose
```

#### Override pool geometry

```sh
meson setup builddir-custom -Dfpc_max_instances=16 -Dfpc_pool_item_size=640
```

#### Warnings-as-errors build

```sh
meson setup builddir-warn -Dwerror=true
meson compile -C builddir-warn
```

#### Notes

- `meson setup` generates `fpc_version.h` and `fpc_conf.h` into the **build directory**
- Tests are always compiled; there is no build-time option to disable them
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
- Use `uint32_t`, `uint16_t`, `int16_t`, `bool` from `<stdint.h>` /
  `<stdbool.h>` — never plain `int` for fixed-width fields.

### Error handling

- Public functions return `bool` or validate via early `return`.
- No `errno`; no exceptions.

### Testing

- Run `meson test -C build` after every change.
- Add a test case for each bug fix.
- Tests live in `tests/test_*.c`; all tests must pass.

---
