## Build Instructions

### First-time setup

After cloning, initialize submodules:

```
git submodule update --init --recursive
```

This pulls in all eight vendored libraries — `luajit`, `mpack`, `picoquic`, `picotls`, `zlib-ng`, `libjpeg-turbo`, `recastnavigation`, and `tools/msdf-atlas-gen`. Skip this and the build will fail with missing headers.

Some of those submodules carry Wired-specific changes that are **not** upstream. They are kept as patch files under `patches/` and applied automatically at CMake configure time — you do not run anything by hand. If you ever need to edit a file under `src/libs/`, read [SUBMODULE-PATCHES.md](SUBMODULE-PATCHES.md) first: a superproject records only a submodule's *commit*, never its *content*, so edits made in place are invisible to `git add -A` and are lost on the next clone.

Two toolchain pieces are needed beyond a C/C++ compiler on every platform:

* **CMake ≥ 3.25 and Ninja** — `make` is a thin wrapper around them, not a standalone build.
* **wasi-sdk** — the deployable mod pack (`pax21.sw3z`) always ships the WASM game modules: `Makefile`'s `PAK_VM_MODULES` lists `gamesv.wasm` / `gamecl.wasm` as unconditional prerequisites. Without wasi-sdk every `make` target fails with `No rule to make target .../gamesv.wasm`.

  Turning WASM off is not a way around this. `USE_WASM` gates *both* the WAMR runtime backend and the compilation of the `.wasm` modules themselves (`cmake/utils/wasm_tools.cmake` returns early when it is off), while the pack's requirement for those files stays unconditional — so `USE_WASM=0` does not drop the requirement, it makes it unsatisfiable. Note also that the CMake option defaults to `OFF` (`CMakeLists.txt`) but the Makefile defaults it to `1`, so the normal `make` path always has it on.

  See [WASM.md](WASM.md) for the pinned version and install locations.

---

### windows/cmake

Wired uses CMake as its build system, and **clang is the only compiler family across every platform**: Apple clang on macOS, clang on Linux, and llvm-mingw clang for Windows targets. MSVC is not supported.

**Windows binaries are cross-compiled from Linux.** The shipped `wired.x64.exe` comes from the `cross-windows` CI job (llvm-mingw inside a Debian container), not from compiling on a Windows machine. The Windows CI job consumes that artifact: it runs the cross-built tests, then builds only the launcher and packages the zip. See [Cross-compiling Windows binaries](#cross-compiling-windows-binaries-linux-windows) for the exact command.

**Building on Windows directly** remains supported for local development via MSYS2 CLANG64 (see [windows/msys2](#windowsmsys2)):
```
make
```

This wraps `cmake` + `ninja` and produces `wired.x64.exe` and `wired-headless.x64.exe` in `build/release/` along with renderer DLLs and game modules. Copy resulting binaries from the created `build` directory. This path is not the one CI seals — if your results differ from a release build, reproduce with the cross container before filing.

**Platform backend:** the window/input/surface backend is SDL3 (`code/sdl`) on every platform; SDL3 is a required build dependency. Audio is handled by miniaudio across all platforms.

---

### Cross-compiling Windows binaries (Linux → Windows)

The canonical path for shipping Windows artifacts. Needs only Docker:

```
docker build -t wired-cross-windows docker/cross-windows
docker run --rm -v "$PWD:/src" wired-cross-windows /src/docker/cross-windows/build.sh
```

Output lands in `build/cross-windows-docker/`: `wired.x64.exe`, `wired-headless.x64.exe`, the three renderer DLLs, `gamecl.wasm` / `gamesv.wasm`, and the `*_test.exe` suite (the container configures with `BUILD_TESTING=ON`).

The image pins llvm-mingw (UCRT flavour) and wasi-sdk; submodule patches under `patches/` are applied automatically at configure time. The toolchain file is `cmake/toolchains/llvm-mingw-x86_64.cmake`.

**A cross build proves compilation, not execution** — the resulting `.exe` cannot run on the build host. Runtime verification happens in the Windows CI job, which downloads the cross artifacts and runs the test executables *before* packaging, so a binary whose tests fail never reaches the zip.

**UCRT vs msvcrt:** llvm-mingw links against UCRT. This is the only meaningful runtime difference from the historical MSYS2-gcc builds, and it is a non-issue on Windows 10 and later.

---

### windows/msys2

Use the **CLANG64** environment, not MINGW64 — the single-toolchain policy (2026-08-14) means every
lane compiles with clang. The `clang64` repo carries the same library set under the
`mingw-w64-clang-x86_64-*` prefix, and the environment provides `cc`/`gcc` shims that point at clang.

Install the build dependencies from an `MSYS2 MSYS` shell:

* pacman -Syu
* pacman -S mingw-w64-clang-x86_64-toolchain mingw-w64-clang-x86_64-cmake mingw-w64-clang-x86_64-sdl3 mingw-w64-clang-x86_64-openssl mingw-w64-clang-x86_64-opus mingw-w64-clang-x86_64-opusfile mingw-w64-clang-x86_64-pkg-config make git

The set above mirrors what CI installs. A local engine build additionally needs `nasm`, and the
launcher needs `go` and `nodejs` — install those from the same `mingw-w64-clang-x86_64-*` prefix.

Then build from the `MSYS2 CLANG64` shell and copy the resulting binaries from the created `build`
directory, or install them directly:

`make install DESTDIR=<path_to_game_files>`

This produces a locally compiled engine. Release artifacts come from the cross container instead —
see [Cross-compiling Windows binaries](#cross-compiling-windows-binaries-linux-windows).

---

### generic/ubuntu linux/bsd

You may need to run the following commands to install packages (using a fresh Ubuntu installation as example):

* sudo apt install make clang cmake ninja-build libssl-dev mesa-common-dev
* sudo apt install libxxf86dga-dev libxrandr-dev libxxf86vm-dev libasound-dev
* sudo apt install libsdl3-dev

CI builds this lane with clang (installed from apt.llvm.org, matching the pinned clang-tidy major).
gcc is not exercised by the build system.

Build with: `make`

Copy the resulting binaries from created `build` directory or use command:

`make install DESTDIR=<path_to_game_files>`

---

### Arch Linux

The package `q3now-git` can either be installed through your favourite AUR helper, or manually using these commands:

Download the snapshot from AUR:

`curl -O https://aur.archlinux.org/cgit/aur.git/snapshot/q3now-git.tar.gz`

Extract the snapshot:

`tar xfz q3now-git.tar.gz`

Enter the extracted directory:

`cd q3now-git`

Build and install `q3now-git`:

`makepkg -risc`

---

### raspberry pi os

Install the build dependencies:

* apt install libsdl3-dev libxxf86dga-dev libssl-dev

Build with: `make`

Copy the resulting binaries from created `build` directory or use command:

`make install DESTDIR=<path_to_game_files>`

---

### macos

Apple Silicon (arm64) is the profile these steps were walked through on: macOS 26.6, Xcode command-line toolchain (Apple clang 21), Homebrew at `/opt/homebrew`. Intel Macs are not currently verified.

**1 — Toolchain and libraries**

```
brew install cmake ninja pkg-config sdl3 molten-vk openssl@3 opus opusfile
```

`openssl@3` is a hard configure requirement (`FIND_PACKAGE(OpenSSL REQUIRED)` — picoquic's TLS backend); it is easy to miss because CI's macOS runners preinstall it.

`SDL3` is the only library CMake resolves from the system (`find_package(SDL3 REQUIRED)`); everything else heavy — zlib-ng, libjpeg-turbo, LuaJIT, picoquic/picotls, Recast/Detour — is a submodule built in-tree. MoltenVK is a *runtime* dependency of the Vulkan renderer: `code/sdl/sdl_glimp.c` points SDL at a MoltenVK dylib before `SDL_Init`, preferring the copy next to the binary (`make copy-libs` places it there via `otool -L` discovery) and falling back to the Homebrew one. There is no `libvulkan.dylib` loader in this setup and none is needed.

**2 — Submodules**

```
git submodule update --init --recursive
```

**3 — wasi-sdk** (required — see *First-time setup* above)

Install the pinned release for `arm64-macos` under `/opt/wasi-sdk`, or point `WASI_SDK_PATH` at it. Verify with `/opt/wasi-sdk/bin/clang --version`.

**4 — Build**

```
make
```

`make` wraps `cmake` + `ninja`. Invoking CMake directly is also supported via presets — `cmake --preset release && cmake --build --preset release` — which carry the same configuration `make` uses (a bare `cmake -B build` does **not**: it would silently configure without WASM). On macOS CMake assembles a single product bundle rather than loose binaries:

```
build/release/q3now-preview.arm64.app/Contents/MacOS/{wired.arm64, wired-headless.arm64}
build/release/wired_{opengl,vulkan}_arm64.dylib
build/release/base/pax21.sw3z
```

**Verifying the build actually succeeded.** `make`'s exit status is not a reliable health signal — a failing `ninja` sub-command can still leave a zero exit at the top of a pipeline. Check for the artefacts instead:

```
test -x build/release/q3now-preview.arm64.app/Contents/MacOS/wired.arm64 && echo OK
```

To collect *all* compile errors rather than stopping at the first, run `ninja -k 0` inside `build/release/`.

**Per-user state.** Config, screenshots and the `qconsole.jsonl` structured log live under `~/wired/<PRODUCT_NAME><CHANNEL_SUFFIX>/` — by default `~/wired/q3now-preview/`. The names come from `PRODUCT_NAME` / `CHANNEL_SUFFIX` at the top of `CMakeLists.txt`.

**Running the test harness.** Several scripts under `tests/` call GNU `timeout` and use `mapfile`, neither of which exists in the macOS base system (BSD userland, bash 3.2). Install `brew install coreutils bash` and put `/opt/homebrew/opt/coreutils/libexec/gnubin` ahead of `/usr/bin` on `PATH`.

---

### ppc64le / ppc64 (PowerPC 64-bit)

Install the build dependencies (same as generic linux above), then build with:

`make`

The JIT compiler (`vm_powerpc.c`) supports optional ISA-level optimizations that are enabled automatically based on compiler target flags:

* **ISA 2.07 (POWER8)**: Uses direct-move instructions (`mtvsrwa`, `mfvsrwz`, `xscvdpsxws`) to eliminate memory round-trips in float/int conversions (`OP_CVIF`, `OP_CVFI`)
* **ISA 3.0 (POWER9)**: Uses hardware modulo instructions (`modsw`, `moduw`) to replace 3-instruction sequences for `OP_MODI` and `OP_MODU`

To enable these optimizations, pass the appropriate `-mcpu` flag:

`make CFLAGS='-mcpu=power8'` - enable ISA 2.07 optimizations

`make CFLAGS='-mcpu=power9'` - enable ISA 2.07 + ISA 3.0 optimizations

`make CFLAGS='-mcpu=native'` - auto-detect based on build machine (note: resulting binary may not be portable to older hardware)

Without explicit `-mcpu`, the optimizations depend on the compiler/distro defaults. The JIT gracefully falls back to baseline instruction sequences when the target ISA level is not available.

---

Several Makefile options are available for linux/mingw/macos builds:

`BUILD_CLIENT=1` - build unified client/server executable, enabled by default

`BUILD_HEADLESS=1` - build headless server executable, enabled by default

`USE_VULKAN=1` - build vulkan modular renderer, enabled by default

`USE_OPENGL=1` - build opengl modular renderer, enabled by default

`USE_OPENGL2=0` - build opengl2 modular renderer, disabled by default

`USE_RENDERER_DLOPEN=1` - do not link single renderer into client binary, compile all enabled renderers as dynamic libraries and allow to switch them on the fly via `\cl_renderer` cvar, enabled by default

`RENDERER_DEFAULT=opengl` - set default value for `\cl_renderer` cvar or use selected renderer for static build for `USE_RENDERER_DLOPEN=0`, valid options are `opengl`, `opengl2`, `vulkan`

`USE_SYSTEM_JPEG=0` - use current system JPEG library, disabled by default

Example:

`make BUILD_HEADLESS=0 USE_RENDERER_DLOPEN=0 RENDERER_DEFAULT=vulkan` - which means do not build headless binary, build client with single static vulkan renderer
