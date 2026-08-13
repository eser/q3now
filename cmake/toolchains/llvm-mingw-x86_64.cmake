# ─────────────────────────────────────────────────────────────────────────────
# Wired — cross-compile toolchain: macOS (arm64/x86_64) host → Windows x86_64
# target, using mstorsjo/llvm-mingw (clang 22 + lld + compiler-rt + libunwind
# + libc++ + mingw-w64/UCRT).
#
# This mirrors the CI Windows job's OUTPUTS (wired.x64.exe, wired-headless.x64
# .exe, wired_{opengl,opengl2,vulkan}_x86_64.dll, game modules, pak, zip) but
# NOT its toolchain: CI is MSYS2 MINGW64 = GCC + msvcrt (.github/workflows/
# build.yml:371-381). llvm-mingw's macOS build is UCRT-only. See BUILD.md §
# windows/cmake for why MSYS2 is the canonical Windows profile.
#
#   cmake -S . -B build/cross-windows -G Ninja \
#     -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/llvm-mingw-x86_64.cmake \
#     -DLLVM_MINGW_ROOT=$HOME/.local/opt/llvm-mingw \
#     -DWIRED_CROSS_PREFIX=$HOME/.local/opt/wired-win64 \
#     -DCMAKE_BUILD_TYPE=Release -DUSE_WASM=ON -DBUILD_TESTING=OFF \
#     -DWIRED_HOST_STRINGIFY=$PWD/build/host-tools/stringify
# ─────────────────────────────────────────────────────────────────────────────

# Guard against the double-inclusion CMake does for try_compile projects.
if(DEFINED WIRED_LLVM_MINGW_TOOLCHAIN_INCLUDED)
	return()
endif()
set(WIRED_LLVM_MINGW_TOOLCHAIN_INCLUDED TRUE)

# ── 1. Target description ────────────────────────────────────────────────────
set(CMAKE_SYSTEM_NAME      Windows)
set(CMAKE_SYSTEM_VERSION   10.0)   # matches _WIN32_WINNT=0x0A00 (CMakeLists.txt:987)
# "x86_64" (not "AMD64") is what MSYS2's cmake reports on the CI runner, and it
# is what CMakeLists.txt:191-198 (BINEXT .x64 / RENDEXT _x86_64), the libopus
# SIMD cohort (CMakeLists.txt:1288) and WAMR's target probe
# (src/libs/wamr/CMakeLists.txt:33-43) all key off. Keep the two in step.
set(CMAKE_SYSTEM_PROCESSOR x86_64)

# ── 2. Toolchain location ────────────────────────────────────────────────────
if(NOT LLVM_MINGW_ROOT)
	if(DEFINED ENV{LLVM_MINGW_ROOT})
		set(LLVM_MINGW_ROOT "$ENV{LLVM_MINGW_ROOT}")
	else()
		set(LLVM_MINGW_ROOT "$ENV{HOME}/.local/opt/llvm-mingw")
	endif()
endif()
set(LLVM_MINGW_ROOT "${LLVM_MINGW_ROOT}" CACHE PATH "llvm-mingw install root")

set(_wired_triple  "x86_64-w64-mingw32")
set(_wired_bin     "${LLVM_MINGW_ROOT}/bin")
set(_wired_sysroot "${LLVM_MINGW_ROOT}/${_wired_triple}")

if(NOT EXISTS "${_wired_bin}/${_wired_triple}-clang")
	message(FATAL_ERROR
		"llvm-mingw not found at ${LLVM_MINGW_ROOT}.\n"
		"Install it user-locally (no sudo), mirroring WASM.md's wasi-sdk recipe:\n"
		"  mkdir -p ~/.local/opt && cd /tmp\n"
		"  curl -LO https://github.com/mstorsjo/llvm-mingw/releases/download/20260616/llvm-mingw-20260616-ucrt-macos-universal.tar.xz\n"
		"  tar xf llvm-mingw-20260616-ucrt-macos-universal.tar.xz -C ~/.local/opt/\n"
		"  ln -sfn ~/.local/opt/llvm-mingw-20260616-ucrt-macos-universal ~/.local/opt/llvm-mingw\n"
		"Or pass -DLLVM_MINGW_ROOT=<path>.")
endif()

# ── 3. Compilers ─────────────────────────────────────────────────────────────
# The *-mingw32-* wrappers are clang-target-wrapper shims that inject
#   -target x86_64-w64-mingw32 -rtlib=compiler-rt -unwindlib=libunwind
#   -stdlib=libc++ -fuse-ld=lld
# (wrappers/x86_64-w64-windows-gnu.cfg + wrappers/mingw32-common.cfg), so no
# --target/--sysroot needs to be repeated here. SEH is the default on x86_64.
set(CMAKE_C_COMPILER   "${_wired_bin}/${_wired_triple}-clang"   CACHE FILEPATH "")
set(CMAKE_CXX_COMPILER "${_wired_bin}/${_wired_triple}-clang++" CACHE FILEPATH "")
set(CMAKE_ASM_COMPILER "${_wired_bin}/${_wired_triple}-clang"   CACHE FILEPATH "")

set(CMAKE_C_COMPILER_TARGET   ${_wired_triple})
set(CMAKE_CXX_COMPILER_TARGET ${_wired_triple})
set(CMAKE_ASM_COMPILER_TARGET ${_wired_triple})

# ── 4. Resource compiler (code/win32/win_resource.rc) ────────────────────────
# Prefer a plain `windres` entry point when the install provides one: CMake
# sets CMAKE_RC_OUTPUT_EXTENSION to .obj only when NAME_WE == "windres"
# (Modules/CMakeDetermineRCCompiler.cmake), otherwise .res. Either links
# (lld dispatches on file magic), but .obj matches CI byte-for-byte.
# NOTE: that helper must be a shell WRAPPER that execs the prefixed binary —
# llvm-windres derives its target from argv[0], so a bare symlink would drop
# the arch prefix. The install steps create it.
if(EXISTS "${_wired_bin}/windres")
	set(CMAKE_RC_COMPILER "${_wired_bin}/windres" CACHE FILEPATH "")
else()
	set(CMAKE_RC_COMPILER "${_wired_bin}/${_wired_triple}-windres" CACHE FILEPATH "")
endif()
# Belt and braces: pin the output architecture regardless of how windres was
# invoked. Harmless when the prefix already selected it.
set(CMAKE_RC_FLAGS_INIT "--target=pe-x86-64")

# ── 5. Binutils (LLVM equivalents; never the host's cctools) ─────────────────
set(CMAKE_AR        "${_wired_bin}/${_wired_triple}-ar"        CACHE FILEPATH "")
set(CMAKE_RANLIB    "${_wired_bin}/${_wired_triple}-ranlib"    CACHE FILEPATH "")
set(CMAKE_NM        "${_wired_bin}/${_wired_triple}-nm"        CACHE FILEPATH "")
set(CMAKE_STRIP     "${_wired_bin}/${_wired_triple}-strip"     CACHE FILEPATH "")
set(CMAKE_OBJCOPY   "${_wired_bin}/${_wired_triple}-objcopy"   CACHE FILEPATH "")
set(CMAKE_OBJDUMP   "${_wired_bin}/${_wired_triple}-objdump"   CACHE FILEPATH "")
set(CMAKE_ADDR2LINE "${_wired_bin}/${_wired_triple}-addr2line" CACHE FILEPATH "")
set(CMAKE_DLLTOOL   "${_wired_bin}/${_wired_triple}-dlltool"   CACHE FILEPATH "")
set(CMAKE_LINKER    "${_wired_bin}/ld.lld"                     CACHE FILEPATH "")
# CMake would otherwise inherit Apple's ar/ranlib argument conventions.
set(CMAKE_C_COMPILER_AR       "${CMAKE_AR}")
set(CMAKE_CXX_COMPILER_AR     "${CMAKE_AR}")
set(CMAKE_C_COMPILER_RANLIB   "${CMAKE_RANLIB}")
set(CMAKE_CXX_COMPILER_RANLIB "${CMAKE_RANLIB}")

# ── 6. NASM (libjpeg-turbo SIMD) — a HOST program emitting a WIN64 object ────
# CMakeLists.txt:1416 forces WITH_SIMD=ON; src/libs/libjpeg-turbo/simd/
# CMakeLists.txt:53-59 degrades gracefully if nasm is missing (REQUIRE_SIMD is
# OFF at CMakeLists.txt:1417), but for parity with CI install `brew install nasm`.
set(CMAKE_ASM_NASM_OBJECT_FORMAT win64)

# ── 7. Find-root policy: target artefacts ONLY, host programs NEVER ──────────
set(CMAKE_FIND_ROOT_PATH "${_wired_sysroot}")
if(WIRED_CROSS_PREFIX)
	# Cross-built dependency prefix: SDL3 (find_package at CMakeLists.txt:337,
	# 1800) and OpenSSL (FIND_PACKAGE(OpenSSL REQUIRED) at CMakeLists.txt:1166).
	list(APPEND CMAKE_FIND_ROOT_PATH "${WIRED_CROSS_PREFIX}")
	list(APPEND CMAKE_PREFIX_PATH    "${WIRED_CROSS_PREFIX}")
	set(OPENSSL_ROOT_DIR "${WIRED_CROSS_PREFIX}" CACHE PATH "")
	set(OPENSSL_USE_STATIC_LIBS ON CACHE BOOL "")
endif()

# PROGRAM=NEVER is essential and deliberate: git (CMakeLists.txt:1035),
# clang-format/clang-tidy (:2209, :2218), naga (:1616), wamrc
# (cmake/utils/wasm_tools.cmake:70), nasm, ninja and the wasi-sdk clang are all
# HOST executables and must be found on the host PATH, not inside the sysroot.
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# Hard block on the host's library trees. Without this a Homebrew SDL3 or
# openssl@3 (BUILD.md:125 installs both) can still be picked up through
# CMAKE_SYSTEM_PREFIX_PATH and blow up the link with Mach-O vs COFF errors.
list(APPEND CMAKE_SYSTEM_IGNORE_PATH
	/usr /usr/local /usr/local/lib /usr/local/include
	/opt/homebrew /opt/homebrew/lib /opt/homebrew/include
	/opt/local /Library/Frameworks /System/Library/Frameworks /sw)

# ── 8. pkg-config: keep the host's .pc tree completely out of the build ──────
# Nothing in CMakeLists.txt calls pkg_check_modules directly, but SDL3's and
# other dependencies' CMake configs may, and Homebrew's PKG_CONFIG_PATH is
# commonly exported in an interactive shell.
set(ENV{PKG_CONFIG_PATH} "")
set(ENV{PKG_CONFIG_SYSROOT_DIR} "")
if(WIRED_CROSS_PREFIX)
	set(ENV{PKG_CONFIG_LIBDIR}
		"${WIRED_CROSS_PREFIX}/lib/pkgconfig:${WIRED_CROSS_PREFIX}/share/pkgconfig:${_wired_sysroot}/lib/pkgconfig")
else()
	set(ENV{PKG_CONFIG_LIBDIR} "${_wired_sysroot}/lib/pkgconfig")
endif()
set(PKG_CONFIG_USE_CMAKE_PREFIX_PATH ON)
# Use a triple-prefixed pkg-config if one exists; otherwise the host binary is
# fine because PKG_CONFIG_LIBDIR above already re-roots its search.
find_program(PKG_CONFIG_EXECUTABLE
	NAMES ${_wired_triple}-pkg-config pkg-config pkgconf
	NO_CMAKE_FIND_ROOT_PATH)

# ── 9. Neutralise Apple-host defaults ────────────────────────────────────────
# CMAKE_SYSTEM_NAME=Windows already makes APPLE false, but a stale cache or an
# environment CFLAGS can still smuggle -isysroot / -arch through.
set(CMAKE_OSX_SYSROOT            "" CACHE INTERNAL "" FORCE)
set(CMAKE_OSX_DEPLOYMENT_TARGET  "" CACHE INTERNAL "" FORCE)
set(CMAKE_OSX_ARCHITECTURES      "" CACHE INTERNAL "" FORCE)
set(CMAKE_MACOSX_BUNDLE          OFF CACHE INTERNAL "" FORCE)
set(CMAKE_MACOSX_RPATH           OFF CACHE INTERNAL "" FORCE)

# ── 10. No emulator ──────────────────────────────────────────────────────────
# Build-time tools must be built for the HOST, not run under wine. The one such
# tool in this tree is `stringify` (CMakeLists.txt:1629, executed by the custom
# commands at :1640-1645); pass -DWIRED_HOST_STRINGIFY=<host binary> and let the
# CMakeLists import it. Setting CMAKE_CROSSCOMPILING_EMULATOR=wine64 would
# "work" but adds an unversioned, unpinned dependency to the build graph.
set(CMAKE_CROSSCOMPILING_EMULATOR "")

# ── 11. try_compile hygiene ──────────────────────────────────────────────────
# Full executables link fine against the mingw sysroot, so the default
# EXECUTABLE target type is correct; propagate the vars try_compile needs.
list(APPEND CMAKE_TRY_COMPILE_PLATFORM_VARIABLES
	LLVM_MINGW_ROOT WIRED_CROSS_PREFIX)

message(STATUS "Wired cross toolchain: ${_wired_triple} via ${LLVM_MINGW_ROOT}")
if(WIRED_CROSS_PREFIX)
	message(STATUS "Wired cross dependency prefix: ${WIRED_CROSS_PREFIX}")
endif()
