## Build Instructions

### First-time setup

After cloning, initialize submodules:

```
git submodule update --init --recursive
```

This pulls in `src/libs/luajit`, `src/libs/mpack`, `src/libs/picoquic`, and `src/libs/picotls`. Skip this and the build will fail with missing headers.

---

### windows/cmake

q3now uses CMake as its build system. **MSYS2 MINGW64 is the canonical Windows toolchain** — it's vendor-neutral, community-owned, and is the profile actively built and verified. The MSVC + Visual Studio cmake generator path is not currently verified; a community contributor wishing to maintain an MSVC build profile is welcome to submit one, but it is not presented here as a supported path until verified.

**Using `make` from MSYS2 MINGW64:**
```
make
```

This wraps `cmake` + `ninja` and produces `q3now.x64.exe` and `q3now-ded.x64.exe` in `build/release/` along with renderer DLLs and game modules. Copy resulting binaries from the created `build` directory.

**USE_SDL profile note:** `USE_SDL=ON` is the default and recommended profile on Windows. `USE_SDL=OFF` uses the native Win32 windowing/input subsystem (`code/win32/win_glimp.c`, `win_input.c`, `win_wndproc.c`); audio is handled by miniaudio across all platforms regardless of this flag. Supported but less tested.

---

### windows/msys2

Install the build dependencies:

`MSYS2 MSYS`

* pacman -Syu
* pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake mingw-w64-x86_64-nasm mingw-w64-x86_64-sdl3 mingw-w64-x86_64-go mingw-w64-x86_64-nodejs mingw-w64-x86_64-opus mingw-w64-x86_64-opusfile make git

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

* `brew install sdl3 molten-vk` (SDL3 required; MoltenVK needed for Vulkan renderer)
* `git submodule update --init --recursive` (required on first clone)

Build with: `make`

Copy the resulting binaries from created `build` directory

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

`BUILD_SERVER=1` - build dedicated server executable, enabled by default

`USE_SDL=0`- use SDL3 backend for video, audio, input subsystems, enabled by default, enforced for macos

`USE_VULKAN=1` - build vulkan modular renderer, enabled by default

`USE_OPENGL=1` - build opengl modular renderer, enabled by default

`USE_OPENGL2=0` - build opengl2 modular renderer, disabled by default

`USE_RENDERER_DLOPEN=1` - do not link single renderer into client binary, compile all enabled renderers as dynamic libraries and allow to switch them on the fly via `\cl_renderer` cvar, enabled by default

`RENDERER_DEFAULT=opengl` - set default value for `\cl_renderer` cvar or use selected renderer for static build for `USE_RENDERER_DLOPEN=0`, valid options are `opengl`, `opengl2`, `vulkan`

`USE_SYSTEM_JPEG=0` - use current system JPEG library, disabled by default

Example:

`make BUILD_SERVER=0 USE_RENDERER_DLOPEN=0 RENDERER_DEFAULT=vulkan` - which means do not build dedicated binary, build client with single static vulkan renderer
