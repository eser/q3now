# WASM VM Backend

Wired runs the server game (`gamesv`) and client game (`gamecl`) modules as
WebAssembly through [WAMR](https://github.com/bytecodealliance/wasm-micro-runtime)
(WebAssembly Micro Runtime). Legacy QVM execution and q3_ui are not part of the
current runtime; menus and HUDs are owned by Wired UI.

## Architecture

```
Engine (Client/Server)
    |
VM_Create() -> requested policy:
    |
+----------------+------------------------+-------------------------+
| 0 VMI_NATIVE   | 1 VMI_BYTECODE         | 2 VMI_COMPILED          |
| DLL/dylib      | .wasm only             | .aot, then .wasm        |
| then 2 fallback| WAMR interpreter       | WAMR AOT/interpreter    |
+----------------+------------------------+-------------------------+

Syscall bridge:
  WASM module imports env.syscall(i32 x 13) -> i32
  Bridge widens to intptr_t and calls vm->systemCall(args)
  vm_t* retrieved via wasm_runtime_get_user_data(exec_env)
```

The same policy applies to `vm_game` and `vm_cgame`. Mode `1` never probes an
AOT file. Mode `2` prefers `.aot` and falls back to `.wasm`; mode `0` first
tries the platform-native module and then uses mode `2` as its fallback. A
live VM records its effective selection policy, so `map_restart` recreates a
mode-1 VM as interpreter-only instead of silently changing it to AOT-first.
For compatibility, integer values above `2` currently follow mode `2`; negative
values are rejected. The public, documented choices remain `0`, `1`, and `2`.

## Building

### Prerequisites

- **wasi-sdk** for compiling game modules to WASM. **Pinned version: 33.0**
  (released 2026-04-30, clang 22.1.0, target `wasm32-unknown-wasip1`) — the
  latest stable release as of 2026-08-10; 34 exists only as release candidates.

  The pin is single-sourced in **`.wasi-sdk-version`** at the repo root: CI's
  three install steps and the Dockerfile all read that file (they used to carry
  independent copies, and had drifted to 32 while this document said 33 —
  meaning CI and developer machines emitted different module bytes, which
  matters whenever a `.wasm` byte-size is used as evidence that a build
  changed). When bumping: edit `.wasi-sdk-version`, update this section's
  version line, and update `docs/health.md`'s "WASM module compilation" entry.

  A cross-compiler toolchain is per-developer state, not system state, so the
  install below needs no `sudo`. CMake auto-detects, in order:
  `$HOME/.local/opt/wasi-sdk`, then `/opt/wasi-sdk` (plus
  `C:/msys64/opt/wasi-sdk` on Windows). An explicit `WASI_SDK_PATH` — env var or
  `-D` — overrides all of them. See `cmake/utils/wasm_tools.cmake`.

  **macOS (arm64):**
  ```bash
  mkdir -p ~/.local/opt && cd /tmp
  curl -LO https://github.com/WebAssembly/wasi-sdk/releases/download/wasi-sdk-33/wasi-sdk-33.0-arm64-macos.tar.gz
  tar xf wasi-sdk-33.0-arm64-macos.tar.gz -C ~/.local/opt/
  ln -sfn ~/.local/opt/wasi-sdk-33.0-arm64-macos ~/.local/opt/wasi-sdk
  ~/.local/opt/wasi-sdk/bin/clang --version     # expect clang 22.1.0-wasi-sdk
  ```

  **Linux (x86_64), e.g., Ubuntu/Debian:**
  ```bash
  mkdir -p ~/.local/opt && cd /tmp
  curl -LO https://github.com/WebAssembly/wasi-sdk/releases/download/wasi-sdk-33/wasi-sdk-33.0-x86_64-linux.tar.gz
  tar xf wasi-sdk-33.0-x86_64-linux.tar.gz -C ~/.local/opt/
  ln -sfn ~/.local/opt/wasi-sdk-33.0-x86_64-linux ~/.local/opt/wasi-sdk
  ```

  A system-wide install under `/opt/wasi-sdk` still works if you prefer it —
  same commands with `sudo` and `-C /opt/`. Existing setups are unaffected.

  **Windows (MSYS2 MINGW64):** `pacman` does not package wasi-sdk (only
  `wasi-libc`, which is just headers). Manual install required:
  ```bash
  cd /c/msys64/opt
  curl -LO https://github.com/WebAssembly/wasi-sdk/releases/download/wasi-sdk-33/wasi-sdk-33.0-x86_64-windows.tar.gz
  tar xzf wasi-sdk-33.0-x86_64-windows.tar.gz
  ln -sf wasi-sdk-33.0-x86_64-windows wasi-sdk   # symlink for consistency
  export WASI_SDK_PATH=C:/msys64/opt/wasi-sdk    # required: Windows-native
                                                 # cmake.exe doesn't resolve
                                                 # POSIX `/opt/wasi-sdk`
  ```
  The `Makefile` forwards `WASI_SDK_PATH` to cmake automatically when set.

  Other distros: download the matching tarball from
  https://github.com/WebAssembly/wasi-sdk/releases/tag/wasi-sdk-33

- **wamrc** (optional) for AOT compilation. Build from
  https://github.com/bytecodealliance/wasm-micro-runtime — not bundled. When
  not on PATH, Wired builds interpreter-mode `.wasm` only (no `.aot`).

### Build commands

```bash
# Build engine + WASM modules (USE_WASM=1 in Makefile, FEAT_WASM=1 in q_feats.h)
make build DEV=1

# Run with the shipped WASM policy (mode 2: .aot then .wasm)
make run-game VM=1

# Run WASM smoke test
make test-wasm
```

### CMake options

| Option | Default | Description |
|--------|---------|-------------|
| `USE_WASM` | `OFF` | Enable WAMR linking and WASM module compilation |

When `USE_WASM=ON`, cmake:
1. Builds WAMR as a static library (`vmlib`)
2. Links it into the engine and headless server
3. Compiles `gamesv` and `gamecl` to `.wasm` via wasi-sdk (output:
   `<build>/<config>/base/vm/{gamesv,gamecl}.wasm`)

### Feature flags

`FEAT_WASM` in `code/game/q_feats.h` controls compile-time inclusion of WASM
code paths. When `0`, all WASM code is compiled out — zero binary impact.

`FEAT_WASM` and `USE_WASM` must match. Mismatch produces a link error (fails
loudly).

Current production builds use native modules and WAMR only. If `FEAT_WASM` is
disabled, modes `1` and `2` cannot load a module; mode `0` can still load the
platform-native module.

## Runtime: WAMR

[WAMR](https://github.com/bytecodealliance/wasm-micro-runtime) — Apache 2.0,
Bytecode Alliance.

- Pure C, designed for embedding
- Interpreter + AOT modes
- Pre-allocated linear memory is exposed through the VM data bounds used by
  the engine syscall bridge
- Vendored at `src/libs/wamr/`

### WAMR build flags

| Flag | Value | Reason |
|------|-------|--------|
| `WAMR_BUILD_INTERP` | 1 | Interpreter mode for `.wasm` files |
| `WAMR_BUILD_AOT` | 1 | AOT mode for `.aot` files |
| `WAMR_BUILD_JIT` | 0 | Not needed (AOT is pre-compiled offline) |
| `WAMR_BUILD_LIBC_BUILTIN` | 0 | Not needed |
| `WAMR_BUILD_LIBC_WASI` | 1 | wasi-sdk libc imports (fd_write, etc.) |
| `WAMR_BUILD_SHARED_MEMORY` | 0 | Not needed |
| `WAMR_BUILD_MULTI_MODULE` | 0 | Each game module is independent |
| `WAMR_BUILD_REF_TYPES` | 1 | Required by wasi-sdk 32+ compiled modules |
| `WAMR_BUILD_SIMD` | 0 | Not needed for game logic |
| `WAMR_BUILD_LOAD_CUSTOM_SECTION` | 1 | For `wired_api` version check |
| `WAMR_DISABLE_HW_BOUND_CHECK` | 1 | **Critical** — see below |

## Design Decisions

### Raw native calling convention

WAMR's default native function invocation (`invokeNative_aarch64.s`) splits
parameters across float (d0-d7) and integer (x0-x7) registers. With 13 i32
syscall parameters, the first 8 values land in float registers which the C
bridge function never reads — causing silent argument corruption.

**Fix**: Register the syscall bridge via `wasm_runtime_register_natives_raw()`
instead of `init_args.native_symbols`. Raw natives receive all parameters in a
`uint64_t *argv` array, bypassing the assembly-level register split entirely.

### HW bound check disabled

WAMR's hardware bounds checking (`OS_ENABLE_HW_BOUND_CHECK`, enabled by default
on macOS/Linux) uses OS signal handlers (SIGSEGV/SIGBUS) and stores the current
exec_env in thread-local storage. This creates a **single-exec-env-per-thread
constraint**: calling `wasm_runtime_call_wasm()` with a different exec_env on
the same thread is rejected with "invalid exec env".

Quake 3 freely calls between game/cgame/ui modules from the same thread —
especially during initialization and syscall handling (e.g., engine calls UI
from within a cgame syscall). This cross-module pattern is fundamental to the
engine and cannot be changed.

**Fix**: `WAMR_DISABLE_HW_BOUND_CHECK=1` falls back to software bounds checking.
Performance impact is negligible for interpreter mode; for AOT, the module
itself contains bounds checks compiled by `wamrc`.

### vmMain argument count

WASM functions have strict arity validation — unlike C varargs or QVM's
stack-based calls. `vmMain(cmd, arg0, arg1, arg2)` always requires exactly 4
i32 parameters. The engine's `VM_Call()` can pass fewer arguments (e.g., 1 for
`GAME_SHUTDOWN`), so `VM_CallWasm` pads to a minimum of 4 with zeros.

### Struct layout compatibility

All shared structures (`refEntity_t`, `playerState_t`, `entityState_t`,
`sharedEntity_t`, etc.) use only fixed-size types (`int`, `float`, `vec3_t`,
`qhandle_t`). No pointer fields in shared structs. This means struct layout is
**identical** between wasm32 and arm64/x86_64, verified at compile time:

| Struct | arm64 | wasm32 |
|--------|-------|--------|
| `refEntity_t` | 140 | 140 |
| `playerState_t` | 468 | 468 |
| `entityState_t` | 208 | 208 |
| `sharedEntity_t` | 516 | 516 |

`gentity_t` differs (952 vs 808) due to pointer fields, but only
`sharedEntity_t` (the prefix) is accessed by the engine — the size difference
is handled correctly as array stride.

### WASM platform block

`q_platform.h` includes a `WASM_MODULE` block that defines `OS_STRING`,
`ID_INLINE`, `ARCH_STRING`, `Q_BIG_ENDIAN`, and path separators for the
wasm32 target. WASM is always little-endian per spec.

### wasi-sdk libc

Game modules are compiled with wasi-sdk's libc (no `-nostdlib`). This provides
standard C functions (`vsnprintf`, `qsort`, `srand`, `atoi`, etc.) that the
game code needs beyond what `bg_lib.c` provides. The WASI imports
(`fd_write`, `proc_exit`, etc.) are resolved by WAMR's built-in WASI support.

## File Map

| File | Role |
|------|------|
| `code/game/q_feats.h` | `FEAT_WASM` flag |
| `code/qcommon/qcommon.h` | `vmInterpret_t` enum |
| `code/qcommon/vm_local.h` | WASM fields in `vm_s`, function declarations |
| `code/qcommon/vm.c` | VM creation/restart, VM_Call dispatch, vminfo, reload_wasm |
| `code/qcommon/vm_interpret_policy.c` | production mode-to-candidate policy |
| `code/qcommon/vm_wasm.c` | Core backend: load, call, destroy, syscall bridge |
| `code/qcommon/q_platform.h` | WASM platform definitions |
| `code/wasm/wasm_bridge.c` | Varargs adapter compiled into WASM modules |
| `src/libs/wamr/` | Vendored WAMR runtime |
| `CMakeLists.txt` | `USE_WASM` option, WAMR linking |
| `cmake/basegame.cmake` | `add_wasm()` calls for game/cgame/ui |
| `cmake/utils/wasm_tools.cmake` | `add_wasm()` cmake function |
| `tests/smoke-wasm.sh` | WASM smoke test |

## Console Commands

| Command | Description |
|---------|-------------|
| `vminfo` | Shows the loaded module type and memory size |
| `reload_wasm` | Diagnostic-only safety command: refuses while any WASM VM is live; otherwise reports a no-op |

## Limitations

- **Interpreter only** — AOT compilation requires `wamrc` which is not bundled.
  AOT produces near-native performance but requires per-platform compilation.
- **No WASM GDB debugging** — deferred until mod authors need it.
- **No multi-language support** — game modules are C-only. Rust/Zig deferred.
- **No live hot reload or state migration** — `/reload_wasm` never unloads a
  live module. Map/server lifecycle transitions remain the only supported VM
  recreation boundary.

## Shipped policy

Production defaults use mode `2`: AOT is preferred when a compatible artifact
is present, otherwise the packaged `.wasm` runs in the WAMR interpreter. Mode
`1` is the explicit interpreter-only policy. Both modes are preserved across
VM recreation; neither implies persistence of WASM linear memory.
