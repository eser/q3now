# WASM VM Backend

q3now supports WebAssembly (WASM) as a game module execution backend via
[WAMR](https://github.com/bytecodealliance/wasm-micro-runtime) (WebAssembly
Micro Runtime). Game modules (qagame, cgame, ui) can run as `.wasm` files
alongside or instead of traditional QVM bytecode.

## Architecture

```
Engine (Client/Server)
    |
VM_Create() -> auto-detect priority (vm_game 2):
    |
+------------+----------+----------+------------+------------+
| VMI_NATIVE | WASM     | WASM     | VMI_COMPILED| VMI_BYTECODE|
| (DLL/SO)   | (.aot)   | (.wasm)  | (QVM JIT)  | (QVM interp)|
+------------+----------+----------+------------+------------+
              ^-- FEAT_WASM -------^  ^-- FEAT_LEGACY_QVM --^

Syscall bridge:
  WASM module imports env.syscall(i32 x 13) -> i32
  Bridge widens to intptr_t and calls vm->systemCall(args)
  vm_t* retrieved via wasm_runtime_get_user_data(exec_env)
```

With `vm_game 2` (auto-detect), the engine tries in order: `.aot` > `.wasm` > `.qvm`.
If `.wasm` is not found, it silently falls back to `.qvm`.

## Building

### Prerequisites

- **wasi-sdk** (v32+) for compiling game modules to WASM:
  ```bash
  # macOS arm64
  cd /tmp
  curl -LO https://github.com/WebAssembly/wasi-sdk/releases/download/wasi-sdk-32/wasi-sdk-32.0-arm64-macos.tar.gz
  sudo tar xf wasi-sdk-32.0-arm64-macos.tar.gz -C /opt/
  sudo ln -sf /opt/wasi-sdk-32.0-arm64-macos /opt/wasi-sdk
  ```
  For other platforms, download from https://github.com/WebAssembly/wasi-sdk/releases

- **wamrc** (optional) for AOT compilation. Build from https://github.com/bytecodealliance/wasm-micro-runtime

### Build commands

```bash
# Build engine + WASM modules (USE_WASM=1 in Makefile, FEAT_WASM=1 in q_feats.h)
make build-debug

# Run with WASM modules (auto-detect: prefers .wasm over .qvm)
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
2. Links it into the engine and dedicated server
3. Compiles all three game modules to `.wasm` via wasi-sdk

### Feature flags

`FEAT_WASM` in `code/game/q_feats.h` controls compile-time inclusion of WASM
code paths. When `0`, all WASM code is compiled out — zero binary impact.

`FEAT_WASM` and `USE_WASM` must match. Mismatch produces a link error (fails
loudly).

`FEAT_LEGACY_QVM` controls the legacy QVM bytecode interpreter and JIT compiler.
Set to `0` to remove all QVM support — the engine then only runs WASM and/or
native DLL modules.

| FEAT_LEGACY_QVM | FEAT_WASM | Result |
|-----------------|-----------|--------|
| 1 | 1 | Both backends, auto-detect prefers WASM |
| 0 | 1 | WASM only, ~6500 lines of QVM code compiled out |
| 1 | 0 | QVM only (original behavior) |
| 0 | 0 | Native DLL only, no VM sandboxing |

## Runtime: WAMR

[WAMR](https://github.com/bytecodealliance/wasm-micro-runtime) — Apache 2.0,
Bytecode Alliance.

- Pure C, designed for embedding
- Interpreter + AOT modes
- Pre-allocated linear memory maps to QVM's `dataMask` model
- Vendored at `code/libs/wamr/`

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
| `WAMR_BUILD_LOAD_CUSTOM_SECTION` | 1 | For `q3now_api` version check |
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
`ID_INLINE`, `ARCH_STRING`, `Q3_LITTLE_ENDIAN`, and path separators for the
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
| `code/qcommon/vm.c` | VM_Create auto-detect, VM_Call dispatch, vminfo, reload_wasm |
| `code/qcommon/vm_wasm.c` | Core backend: load, call, destroy, syscall bridge |
| `code/qcommon/q_platform.h` | WASM platform definitions |
| `code/wasm/wasm_bridge.c` | Varargs adapter compiled into WASM modules |
| `code/libs/wamr/` | Vendored WAMR runtime |
| `CMakeLists.txt` | `USE_WASM` option, WAMR linking |
| `cmake/basegame.cmake` | `add_wasm()` calls for game/cgame/ui |
| `cmake/utils/wasm_tools.cmake` | `add_wasm()` cmake function |
| `tests/smoke-wasm.sh` | WASM smoke test |

## Console Commands

| Command | Description |
|---------|-------------|
| `vminfo` | Shows module type (QVM JIT / WASM Interp / WASM AOT), memory size |
| `reload_wasm` | Force-unload WASM modules (reload on next map) |

## Limitations

- **Interpreter only** — AOT compilation requires `wamrc` which is not bundled.
  AOT produces near-native performance but requires per-platform compilation.
- **No WASM GDB debugging** — deferred until mod authors need it.
- **No multi-language support** — game modules are C-only. Rust/Zig deferred.
- **No hot-reload file watcher** — `/reload_wasm` is manual. Automatic
  file-watching hot-reload is a future TODO.

## Migration Roadmap

| Phase | Default | FEAT_WASM | Status |
|-------|---------|-----------|--------|
| **Ship** | QVM (`vm_*=2`) | `0` | WASM testing, auto-detect finds .wasm |
| **Stabilize** | QVM (`vm_*=2`) | `1` | WASM promoted to mature |
| **Flip** | WASM preferred | `1` | .wasm ships in pak files |
| **Deprecate** | WASM only | `1` | QVM legacy, eventually removed |
