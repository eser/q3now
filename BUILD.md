## Build Instructions

### First-time setup

After cloning, initialize submodules:

```
git submodule update --init --recursive
```

This pulls in all eight vendored libraries — `luajit`, `mpack`, `picoquic`, `picotls`, `zlib-ng`, `libjpeg-turbo`, `recastnavigation`, and `tools/msdf-atlas-gen`. Skip this and the build will fail with missing headers.

Some of those submodules carry Wired-specific changes that are **not** upstream. They are kept as patch files under `patches/` and applied automatically at CMake configure time — you do not run anything by hand. If you ever need to edit a file under `src/libs/`, read [SUBMODULE-PATCHES.md](SUBMODULE-PATCHES.md) first: a superproject records only a submodule's *commit*, never its *content*, so edits made in place are invisible to `git add -A` and are lost on the next clone.

Two toolchain pieces are needed beyond a C/C++ compiler on every platform:

* **CMake ≥ 3.14 and Ninja** — `make` is a thin wrapper around them, not a standalone build.
* **wasi-sdk** — the deployable mod pack (`pax21.sw3z`) always ships the WASM game modules: `Makefile`'s `PAK_VM_MODULES` lists `gamesv.wasm` / `gamecl.wasm` as unconditional prerequisites. Without wasi-sdk every `make` target fails with `No rule to make target .../gamesv.wasm`.

  Turning WASM off is not a way around this. `USE_WASM` gates *both* the WAMR runtime backend and the compilation of the `.wasm` modules themselves (`cmake/utils/wasm_tools.cmake` returns early when it is off), while the pack's requirement for those files stays unconditional — so `USE_WASM=0` does not drop the requirement, it makes it unsatisfiable. Note also that the CMake option defaults to `OFF` (`CMakeLists.txt`) but the Makefile defaults it to `1`, so the normal `make` path always has it on.

  See [WASM.md](WASM.md) for the pinned version and install locations.

---

### windows/cmake

Wired uses CMake as its build system. **MSYS2 MINGW64 is the canonical Windows toolchain** — it's vendor-neutral, community-owned, and is the profile actively built and verified. The MSVC + Visual Studio cmake generator path is not currently verified; a community contributor wishing to maintain an MSVC build profile is welcome to submit one, but it is not presented here as a supported path until verified.

**Using `make` from MSYS2 MINGW64:**
```
make
```

This wraps `cmake` + `ninja` and produces `wired.x64.exe` and `wired-headless.x64.exe` in `build/release/` along with renderer DLLs and game modules. Copy resulting binaries from the created `build` directory.

**Platform backend:** the window/input/surface backend is SDL3 (`code/sdl`) on every platform; SDL3 is a required build dependency. Audio is handled by miniaudio across all platforms.

---

### windows/msys2

Install the build dependencies:

`MSYS2 MSYS`

* pacman -Syu
* pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake mingw-w64-x86_64-nasm mingw-w64-x86_64-sdl3 mingw-w64-x86_64-go mingw-w64-x86_64-nodejs mingw-w64-x86_64-opus mingw-w64-x86_64-opusfile mingw-w64-x86_64-clang-tools-extra make git

Use `MSYS2 MINGW32` or `MSYS2 MINGW64` depending on your target system, then copy resulting binaries from created `build` directory or use command:

`make install DESTDIR=<path_to_game_files>`

---

### windows/mingw

All build dependencies (libraries, headers) are bundled-in

Build with either `make ARCH=x86` or `make ARCH=x86_64` commands depending on your target system, then copy resulting binaries from created `build` directory or use command:

`make install DESTDIR=<path_to_game_files>`

---

### generic/ubuntu linux/bsd

You may need to run the following commands to install packages (using fresh ubuntu-18.04 installation as example):

* sudo apt install make gcc libcurl4-openssl-dev mesa-common-dev
* sudo apt install libxxf86dga-dev libxrandr-dev libxxf86vm-dev libasound-dev
* sudo apt install libsdl3-dev

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

* apt install libsdl3-dev libxxf86dga-dev libcurl4-openssl-dev

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
