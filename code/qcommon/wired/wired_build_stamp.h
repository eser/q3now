// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
#ifndef WIRED_BUILD_STAMP_H
#define WIRED_BUILD_STAMP_H

// Per-build stamp shared by every separately-compiled component (engine client,
// headless server, renderer DLL, gamecl / gamesv VMs — native AND WASM). The
// build system passes WIRED_BUILD_ID (a monotonic counter from the Makefile's
// .build_number) and WIRED_BUILD_DATE (a "%Y-%m-%d-%H-%M" CMake timestamp) as
// compile definitions to each component, so each binary embeds its OWN id+date.
// `sysinfo` prints all five so a stale binary self-reports an older stamp than
// the engine. Fallbacks keep a bare/SDK-less compile (no -D) building.
//
// WIRED_BUILD_TU_DATE is this translation unit's own __DATE__ " " __TIME__ —
// finer than the per-build id when a single component is recompiled in isolation.
//
// The id/date come from a Makefile-GENERATED header (wired_build_stamp_gen.h,
// written by the `_build-stamp` target, .gitignore'd, regenerated every build).
// The header route (vs compiler -D) refreshes every build with no cmake
// reconfigure, avoids the windres/Ninja spaced-string -D escaping trap, and
// reaches the WASI wasm builds for free (same TU). __has_include keeps a bare /
// SDK-less compile (no generated header) building via the fallbacks below.

#if defined(__has_include)
#  if __has_include("wired/wired_build_stamp_gen.h")
#    include "wired/wired_build_stamp_gen.h"
#  elif __has_include("wired_build_stamp_gen.h")
#    include "wired_build_stamp_gen.h"
#  endif
#endif

#ifndef WIRED_BUILD_ID
#define WIRED_BUILD_ID 0
#endif

#ifndef WIRED_BUILD_DATE
#define WIRED_BUILD_DATE "unknown"
#endif

#define WIRED_BUILD_TU_DATE  ( __DATE__ " " __TIME__ )

// WIRED_BUILD_ID arrives as a bare integer token; stringize it for cvar / printf.
#define WIRED_BUILD_STR2(x)  #x
#define WIRED_BUILD_STR(x)   WIRED_BUILD_STR2(x)
#define WIRED_BUILD_ID_STR   WIRED_BUILD_STR(WIRED_BUILD_ID)

#endif // WIRED_BUILD_STAMP_H
