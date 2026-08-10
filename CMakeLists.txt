CMAKE_MINIMUM_REQUIRED(VERSION 3.14)

PROJECT(wired LANGUAGES C CXX ASM)

SET(CMAKE_C_STANDARD 11)
SET(CMAKE_C_STANDARD_REQUIRED ON)
SET(CMAKE_C_EXTENSIONS OFF)

# Apple clang on macOS defaults to C++03 if no standard is set explicitly,
# which causes nav_impl.cpp's `auto` and `override` usage to error out as
# "C++11 extensions" rather than warn.  Pin to C++17: covers our current
# usage (auto, override, range-for, lambdas) plus headroom for future
# constexpr / structured-binding / std::optional adoption without another
# bump.  Apple clang has shipped C++17 since Xcode 9 (2017); no risk.
# Extensions left ON (CMake default) because vendored picoquic/picotls
# C++ touchpoints rely on them.
SET(CMAKE_CXX_STANDARD 17)
SET(CMAKE_CXX_STANDARD_REQUIRED ON)

# Strict -std=c11 hides glibc POSIX/BSD extensions (addrinfo, EAI_NONAME,
# IFF_UP, NI_NUMERICHOST, nanosleep, ...) that net_ip.c, unix_main.c, and
# other platform code rely on. _GNU_SOURCE re-exposes them without raising
# the C standard floor. Future work: incrementally reduce the surface that
# requires _GNU_SOURCE and eventually drop this define. See docs/health.md
# "Linux dev workflow silently broken for 14 days" for motivation.
ADD_COMPILE_DEFINITIONS(_GNU_SOURCE)

# Product / channel branding — used for per-user data layout
# (~/wired/<PRODUCT_NAME><CHANNEL_SUFFIX>/) and OS bundle / install paths.
# Override via -DPRODUCT_NAME=<name> / -DCHANNEL_SUFFIX=-<channel>.
IF(NOT DEFINED PRODUCT_NAME)
	SET(PRODUCT_NAME "q3now")
ENDIF()
IF(NOT DEFINED CHANNEL_SUFFIX)
	SET(CHANNEL_SUFFIX "-preview")
ENDIF()
ADD_COMPILE_DEFINITIONS(PRODUCT_NAME="${PRODUCT_NAME}")
ADD_COMPILE_DEFINITIONS(CHANNEL_SUFFIX="${CHANNEL_SUFFIX}")

SET(CNAME "wired")
SET(DNAME "wired-headless")

SET(RENDERER_PREFIX ${CNAME})
SET(RENDERER_DEFAULT vulkan) # valid options: opengl, vulkan, opengl2; opengl as ultimate fallback

OPTION(USE_CURL "" ON)
OPTION(USE_OPENGL "" ON)
OPTION(USE_VULKAN "" ON)
OPTION(USE_SYSTEM_JPEG "" OFF)
OPTION(USE_JPEG_TURBO "" ON)
OPTION(USE_ZLIB_NG "" ON)
OPTION(USE_RENDERER_DLOPEN "" ON)
OPTION(USE_WASM "Enable WASM VM backend via WAMR" OFF)
OPTION(USE_LUA "Enable LuaJIT scripting" ON)
OPTION(USE_WIRED_UI_CLAY "Vendor Clay layout engine for the WiredUI compositor (client target only)" ON)
OPTION(USE_SSAO "Compile the GTAO/SSAO renderer path in (FEAT_SSAO=1) for the visual-gate verify build; OFF = compiled-out ship path" OFF)

SET(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

AUX_SOURCE_DIRECTORY(code/qcommon QCOMMON_SRCS)
AUX_SOURCE_DIRECTORY(code/qcommon/maps QCOMMON_SRCS)

# Arena allocator (must be explicit — AUX_SOURCE_DIRECTORY is configure-time only)
list(APPEND QCOMMON_SRCS code/qcommon/arena.c)

# stalltrace diagnostic (temporary; cvar-gated, default-off)
list(APPEND QCOMMON_SRCS code/qcommon/wired/stalltrace.c)

# LZ4 decoder (for SW3Z archive reader)
list(APPEND QCOMMON_SRCS src/libs/lz4/lz4.c src/libs/lz4/lz4frame.c src/libs/lz4/xxhash.c src/libs/lz4/lz4hc.c)

# When WASM is disabled, override FEAT_WASM=0 so vm_wasm.c compiles to empty
# and vm.c skips WASM code paths (q_feats.h hardcodes FEAT_WASM=1).
IF(NOT USE_WASM)
	ADD_COMPILE_DEFINITIONS(FEAT_WASM=0)
ENDIF()

# When USE_SSAO is ON, override FEAT_SSAO=1 so the GTAO renderer path compiles in
# for the visual-gate verify build (q_feats.h #ifndef-defaults it to 0 = the ship
# build's compiled-out path). The default ship build leaves USE_SSAO OFF → FEAT_SSAO 0.
IF(USE_SSAO)
	ADD_COMPILE_DEFINITIONS(FEAT_SSAO=1)
ENDIF()

# MinGW: picoquic + picotls use bare `#ifdef _WINDOWS` (MSVC-only auto-define)
# to gate winsock includes. Inject _WINDOWS so they take the Windows code
# path; vendored sources stay untouched. Definition is innocuous for wired's
# own code (wired uses _WIN32 throughout). Applied globally before any target
# is created so every TU that transitively includes picoquic.h sees it.
# See docs/health.md "picoquic _WINDOWS guards do not match MinGW".
IF(WIN32 AND NOT MSVC)
	ADD_COMPILE_DEFINITIONS(_WINDOWS)
ENDIF()
IF(CMAKE_SYSTEM_PROCESSOR MATCHES AMD64|x86|i*86)
	IF(CMAKE_SYSTEM_PROCESSOR MATCHES "AMD64|x86_64")
		IF(WIN32)
			SET(BINEXT .x64)
		ELSE()
			SET(BINEXT .x86_64)
		ENDIF()
		SET(RENDEXT _x86_64)
	ELSE()
		#SET(BINEXT .x86)
		SET(RENDEXT _x86)
	ENDIF()
ELSEIF(CMAKE_SYSTEM_PROCESSOR MATCHES aarch64|arm64)
	IF(APPLE)
		SET(BINEXT .arm64)
		SET(RENDEXT _arm64)
	ELSE()
		SET(BINEXT .aarch64)
		SET(RENDEXT _aarch64)
	ENDIF()
ELSEIF(CMAKE_SYSTEM_PROCESSOR MATCHES arm*)
	SET(BINEXT .arm)
	SET(RENDEXT _arm)
ENDIF()

# botlib
AUX_SOURCE_DIRECTORY(code/botlib BOTLIB_SRCS)
ADD_LIBRARY(botlib OBJECT ${BOTLIB_SRCS})
TARGET_COMPILE_DEFINITIONS(botlib PRIVATE BOTLIB)
# code/qcommon needed so wired/* sub-headers (e.g. wired/con/log.h) can
# resolve q_shared.h and peer headers via the include-dir search path.
TARGET_INCLUDE_DIRECTORIES(botlib PRIVATE code/qcommon)

AUX_SOURCE_DIRECTORY(code/server SERVER_SRCS)
list(REMOVE_ITEM SERVER_SRCS code/server/sv_rankings.c)

# bg_weapons.c, bg_misc.c and bg_gametypes.c go into qcommon so the headless
# server can access bg_weaponlist, bg_itemlist and bg_gametypelist
# (e.g. for MCP tools like game.items, coaching.tick)
list(APPEND QCOMMON_SRCS code/game/bg_weapons.c code/game/bg_misc.c code/game/bg_gametypes.c)

# universal binary
ADD_LIBRARY(qcommon OBJECT ${QCOMMON_SRCS} ${SERVER_SRCS})

# headless server
ADD_LIBRARY(qcommon_ded OBJECT ${QCOMMON_SRCS} ${SERVER_SRCS})
TARGET_COMPILE_DEFINITIONS(qcommon_ded PUBLIC HEADLESS)

# LZ4 include path (for SW3Z archive reader)
TARGET_INCLUDE_DIRECTORIES(qcommon PRIVATE src/libs/lz4)
TARGET_INCLUDE_DIRECTORIES(qcommon_ded PRIVATE src/libs/lz4)

# code/qcommon as explicit include root so wired/* sub-components can resolve
# sibling headers (qcommon.h, q_shared.h, unzip.h, etc.) without relative paths.
TARGET_INCLUDE_DIRECTORIES(qcommon     PRIVATE code/qcommon)
TARGET_INCLUDE_DIRECTORIES(qcommon_ded PRIVATE code/qcommon)

# wired/core/vfs — virtual filesystem
FILE(GLOB WIRED_VFS_SRCS  code/qcommon/wired/core/vfs/*.c)
TARGET_SOURCES(qcommon     PRIVATE ${WIRED_VFS_SRCS})
TARGET_SOURCES(qcommon_ded PRIVATE ${WIRED_VFS_SRCS})

# wired/core/cvars — cvar sub-component
FILE(GLOB WIRED_CVAR_SRCS code/qcommon/wired/core/cvars/*.c)
TARGET_SOURCES(qcommon     PRIVATE ${WIRED_CVAR_SRCS})
TARGET_SOURCES(qcommon_ded PRIVATE ${WIRED_CVAR_SRCS})

# wired/core/shell — keys + cmd
FILE(GLOB WIRED_CON_SRCS  code/qcommon/wired/core/shell/*.c)
TARGET_SOURCES(qcommon     PRIVATE ${WIRED_CON_SRCS})
TARGET_SOURCES(qcommon_ded PRIVATE ${WIRED_CON_SRCS})

# wired/core/console — presentation-agnostic ring buffer + history +
# scrollback + search/mark + severity-as-data + the param-injected geometry
# reflow (Con_Reflow). Turn 3 V-20 extracted all UI-tier touches (glconfig,
# render metrics, color vecs, Key_*/Field_*/cgvm, the toggleconsole/messagemode
# commands, and Con_CheckResize's glconfig half) into the projection at
# code/client/wired/ui/elements/console.c. con_buffer.c now includes ZERO
# client/* headers, so it joins the dedicated-server (qcommon_ded) target too.
FILE(GLOB WIRED_CONSOLE_SRCS code/qcommon/wired/core/console/*.c)
TARGET_SOURCES(qcommon     PRIVATE ${WIRED_CONSOLE_SRCS})
TARGET_SOURCES(qcommon_ded PRIVATE ${WIRED_CONSOLE_SRCS})

# wired/core/logging — log pipeline (console/tty/file sinks, ring buffer)
FILE(GLOB WIRED_LOG_SRCS  code/qcommon/wired/core/logging/*.c)
TARGET_SOURCES(qcommon     PRIVATE ${WIRED_LOG_SRCS})
TARGET_SOURCES(qcommon_ded PRIVATE ${WIRED_LOG_SRCS})

# wired/core/events — event primitive stubs (V2 bodies TBD)
FILE(GLOB WIRED_EVENT_SRCS code/qcommon/wired/core/events/*.c)
TARGET_SOURCES(qcommon     PRIVATE ${WIRED_EVENT_SRCS})
TARGET_SOURCES(qcommon_ded PRIVATE ${WIRED_EVENT_SRCS})

# wired/core/time — time subsystem (Sys_NanoTime, Sys_Milliseconds, Com_RealTime, Com_FormatTimestamp)
FILE(GLOB WIRED_TIME_SRCS  code/qcommon/wired/core/time/*.c)
TARGET_SOURCES(qcommon     PRIVATE ${WIRED_TIME_SRCS})
TARGET_SOURCES(qcommon_ded PRIVATE ${WIRED_TIME_SRCS})

# wired/core/thread — portable thread + atomic primitives (Sys_CreateThread, ...), links
# into both client and headless (unlike SDL threads) for the background navmesh bake
FILE(GLOB WIRED_THREAD_SRCS  code/qcommon/wired/core/thread/*.c)
TARGET_SOURCES(qcommon     PRIVATE ${WIRED_THREAD_SRCS})
TARGET_SOURCES(qcommon_ded PRIVATE ${WIRED_THREAD_SRCS})

# wired/core — orchestration layer (WiredCore_Init / WiredCore_Shutdown)
FILE(GLOB WIRED_CORE_SRCS code/qcommon/wired/core/*.c)
TARGET_SOURCES(qcommon     PRIVATE ${WIRED_CORE_SRCS})
TARGET_SOURCES(qcommon_ded PRIVATE ${WIRED_CORE_SRCS})

# wired/math — pure-CPU math (spline curve core; NOT renderer, NOT RAL).
# Cinematic-scene spline core: RBDOOM Curve.h port (Catmull-Rom + TCB +
# arc-length reparameterization). No Vulkan/RAL dependency — only q_shared.h
# vec macros.
FILE(GLOB WIRED_MATH_SRCS code/qcommon/wired/math/*.c)
TARGET_SOURCES(qcommon     PRIVATE ${WIRED_MATH_SRCS})
TARGET_SOURCES(qcommon_ded PRIVATE ${WIRED_MATH_SRCS})

# wired_curve math-core unit test (cinematic-scene spline core gate).
# Standalone: wired_curve.c compiled with -DWIRED_CURVE_STANDALONE so it uses
# its local vec-macro fallback (no full q_shared.h chain in the test binary).
# Asserts: interpolation-through-control-points, straight-line constant
# tangent, TCB(0,0,0)==Catmull-Rom, SetConstantSpeed linearity, GetTimeForLength
# round-trip, Newton div-by-zero guard.
# Run with: ctest -R wired_curve
ENABLE_TESTING()
ADD_EXECUTABLE(wired_curve_test
	tests/wired_curve_test.c
	code/qcommon/wired/math/wired_curve.c
)
TARGET_COMPILE_DEFINITIONS(wired_curve_test PRIVATE WIRED_CURVE_STANDALONE)
IF(NOT WIN32)
	TARGET_LINK_LIBRARIES(wired_curve_test m)
ENDIF()
ADD_TEST(NAME wired_curve COMMAND wired_curve_test)

# wired_scene evaluator unit test (samples wiredScene_t -> {origin,angles,fov}).
# Standalone: wired_scene_eval.c + wired_curve.c compiled with
# -DWIRED_SCENE_STANDALONE / -DWIRED_CURVE_STANDALONE so they use local vec /
# vectoangles / stricmp fallbacks (no q_shared.h or Lua VM in the test binary).
# The test builds a wiredScene_t directly (WiredCurve_Init + WiredCurve_AddValue,
# events filled in C) — no Lua loader needed. Asserts: origin exact at t=0 / a
# knot / t=end, tangent facing without a target, look-at facing + curTargetIndex
# flip after a TARGET event, authored-vs-event fov precedence, a WAIT holding the
# origin, and FADEOUT/SCENE surfaced as pending signals (not acted on).
# Run with: ctest -R wired_scene_eval
ADD_EXECUTABLE(wired_scene_eval_test
	tests/wired_scene_eval_test.c
	code/qcommon/wired/scene/wired_scene_eval.c
	code/qcommon/wired/math/wired_curve.c
)
TARGET_COMPILE_DEFINITIONS(wired_scene_eval_test PRIVATE WIRED_SCENE_STANDALONE WIRED_CURVE_STANDALONE)
IF(NOT WIN32)
	TARGET_LINK_LIBRARIES(wired_scene_eval_test m)
ENDIF()
ADD_TEST(NAME wired_scene_eval COMMAND wired_scene_eval_test)

# savegame callback registry — Phase-2 hard gate: name<->pointer round-trip +
# completeness across both registry tiers. Compiles the REAL resolver source
# (code/game/g_save_registry.c) against stub callback symbols under
# SG_FUNCS_STANDALONE; the callback name lists (central SG_CALLBACK_LIST +
# per-file SG_LOCAL_CB_*) are the single source shared with the game build.
# Asserts: name->ptr->name and ptr->name->ptr for every registered callback,
# pointer distinctness, the registered count (114 in the default feature config),
# the NULL/""/unknown-name paths (unknown -> NULL, the Phase-6 version signal),
# and that Q3_ReturnToPos1 (the runtime-reassigned mover think) resolves.
# Run with: ctest -R sg_registry
ADD_EXECUTABLE(sg_registry_test
	tests/sg_registry_test.c
	code/game/g_save_registry.c
)
TARGET_INCLUDE_DIRECTORIES(sg_registry_test PRIVATE code/game)
ADD_TEST(NAME sg_registry COMMAND sg_registry_test)

# savegame file primitive — Phase-3 hard gate: version header + RLE codec
# round-trip. Compiles the REAL codec (code/game/g_save_codec.c, the game-type-
# free half of the file primitive) and drives it directly. Asserts: header
# init/validate, strict wrong-magic / wrong-version / NULL rejection, and the
# zero-run RLE codec round-tripping every buffer shape (all-zero, no-zero, sparse,
# alternating, >255 runs, empty) plus capacity and malformed-input guards. The
# trap-using SG_WriteFile / SG_ReadFile (g_save_file.c) wrap this codec in FS
# calls and are covered by the game build compiling them.
# Run with: ctest -R sg_savefile
ADD_EXECUTABLE(sg_savefile_test
	tests/sg_savefile_test.c
	code/game/g_save_codec.c
)
TARGET_INCLUDE_DIRECTORIES(sg_savefile_test PRIVATE code/game)
ADD_TEST(NAME sg_savefile COMMAND sg_savefile_test)

# savegame serializer — Phase-4 hard gate: the field-walker round-trip + index
# relocation. Compiles the REAL walker (g_save_serialize.c) + registry
# (g_save_registry.c) and drives them with a synthetic world carrying every
# field-type the Phase-1 descriptors use. Asserts: byte-exact round-trip of
# scalars/vector/RAW/string, symmetric index relocation (ptr<->index, -1==NULL both
# directions) for entity/client/item, callback round-trip via the registry name,
# and rejection of an out-of-range index + an unknown callback name. The real
# gentity_t walk over g_entities (g_save_world.c) is covered by the game build.
# Run with: ctest -R sg_serialize
ADD_EXECUTABLE(sg_serialize_test
	tests/sg_serialize_test.c
	code/game/g_save_serialize.c
	code/game/g_save_registry.c
)
TARGET_INCLUDE_DIRECTORIES(sg_serialize_test PRIVATE code/game)
ADD_TEST(NAME sg_serialize COMMAND sg_serialize_test)

# savegame .psw persistent subset — Phase-5 hard gate: the cross-map carry
# (player-progression byte ranges + objectives POD) round-trip. Compiles the REAL
# range-walker (g_save_serialize.c SG_WriteRanges/SG_ReadRanges) + the header codec
# (g_save_codec.c) and drives them with a synthetic client + objectives. Asserts:
# progression ranges round-trip byte-exact, SUBSET-ONLY (a non-whitelisted
# transient field does NOT leak into the .psw), objectives POD round-trips, and a
# wrong-version/magic .psw header is REJECTED. The real gclientPersFields walk over
# level.clients (g_save_world.c) is covered by the game build.
# Run with: ctest -R sg_psw
ADD_EXECUTABLE(sg_psw_test
	tests/sg_psw_test.c
	code/game/g_save_serialize.c
	code/game/g_save_codec.c
)
TARGET_INCLUDE_DIRECTORIES(sg_psw_test PRIVATE code/game)
ADD_TEST(NAME sg_psw COMMAND sg_psw_test)

# savegame load fixup — Phase-6 (FINAL) hard gate: the two-phase load relocation.
# Compiles the REAL walker (g_save_serialize.c SG_ReadFields + SG_ResolveIndices) +
# registry (g_save_registry.c) and drives a synthetic world of cross-referencing
# records through Phase-A (load-all into fixed-base slots, pointers = indices) then
# Phase-B (SG_ResolveIndices patches indices -> live pointers). Asserts every
# cross-reference resolves to the CORRECT live slot — forward-references (A->C where
# C loads later), backward, self, and NULL — and strings re-intern via the
# callback. This is the relocation the live SG_LoadWorld resume depends on.
# Run with: ctest -R sg_reloc
ADD_EXECUTABLE(sg_reloc_test
	tests/sg_reloc_test.c
	code/game/g_save_serialize.c
	code/game/g_save_registry.c
)
TARGET_INCLUDE_DIRECTORIES(sg_reloc_test PRIVATE code/game)
ADD_TEST(NAME sg_reloc COMMAND sg_reloc_test)

# Q1 monster animation derivation — Phase A hard gate: the frame-name prefix
# grouping that turns a Q1 .mdl's frame names into animation ranges. Compiles the
# REAL derivation (code/renderercommon/tr_model_mdl_anim.h, MDL_ANIM_TEST_STANDALONE)
# and drives it with the canonical Q1 soldier frame-name sequence. Asserts:
# contiguous same-label frames form one range; id's letter-before-digit variants
# (pain vs painb) stay SEPARATE; all-digit/empty-label frames open no range. This
# is the derivation the renderer's R_GetMDLAnimations + cgame's CG_ParseMDLAnimations
# both build on.
# Run with: ctest -R mdl_anim
ADD_EXECUTABLE(mdl_anim_test
	tests/mdl_anim_test.c
)
ADD_TEST(NAME mdl_anim COMMAND mdl_anim_test)

# wired/scene — cinematic-scene definition + Lua-table loader. Pure CPU +
# Lua load (NOT renderer, NOT RAL): loads a scene .lua that returns a table
# into a wiredScene_t (eye-path spline + named target splines + fov lerp +
# decoded event list) atop the wired/math spline core. Depends on WiredScript
# (the Lua VM) — scene definitions are Lua like the rest of Wired content. The
# luajit include dir + link for qcommon/qcommon_ded are set with WiredScript's.
FILE(GLOB WIRED_SCENE_SRCS code/qcommon/wired/scene/*.c)
TARGET_SOURCES(qcommon     PRIVATE ${WIRED_SCENE_SRCS})
TARGET_SOURCES(qcommon_ded PRIVATE ${WIRED_SCENE_SRCS})

# The wired_scene Lua-loader unit test links LuaJIT, so its target is defined
# in the IF(USE_LUA) block below (after LUAJIT_DIR / the luajit target exist).

# util/ — utility sub-components (crypto, md5, md4; string/sort/hash extraction TBD)
FILE(GLOB UTIL_SRCS code/qcommon/util/*.c)
TARGET_SOURCES(qcommon     PRIVATE ${UTIL_SRCS})
TARGET_SOURCES(qcommon_ded PRIVATE ${UTIL_SRCS})
TARGET_INCLUDE_DIRECTORIES(qcommon     PRIVATE code/qcommon/util)
TARGET_INCLUDE_DIRECTORIES(qcommon_ded PRIVATE code/qcommon/util)
TARGET_INCLUDE_DIRECTORIES(botlib      PRIVATE code/qcommon/util)

# ── zlib-ng (faster inflate for pk3 decompression) ────────────────
# Replaces the embedded zlib in unzip.c via USE_ZLIB_NG guard.
# Submodule: src/libs/zlib-ng  —  cmake -B build -DUSE_ZLIB_NG=ON
IF(USE_ZLIB_NG)
    SET(ZLIB_COMPAT         ON  CACHE BOOL "" FORCE)
    SET(ZLIB_ENABLE_TESTS   OFF CACHE BOOL "" FORCE)
    SET(ZLIB_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    ADD_SUBDIRECTORY(src/libs/zlib-ng EXCLUDE_FROM_ALL)
    TARGET_INCLUDE_DIRECTORIES(qcommon     PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/libs/zlib-ng
        ${CMAKE_CURRENT_BINARY_DIR}/src/libs/zlib-ng)
    TARGET_INCLUDE_DIRECTORIES(qcommon_ded PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/libs/zlib-ng
        ${CMAKE_CURRENT_BINARY_DIR}/src/libs/zlib-ng)
    TARGET_LINK_LIBRARIES(qcommon     PRIVATE zlibstatic)
    TARGET_LINK_LIBRARIES(qcommon_ded PRIVATE zlibstatic)
ELSE()
    TARGET_COMPILE_DEFINITIONS(qcommon     PRIVATE FEAT_LEGACY_ZLIB)
    TARGET_COMPILE_DEFINITIONS(qcommon_ded PRIVATE FEAT_LEGACY_ZLIB)
ENDIF()

# WAMR (WebAssembly Micro Runtime) for WASM VM backend
IF(USE_WASM)
	SET(WAMR_BUILD_INTERP 1)
	SET(WAMR_BUILD_FAST_INTERP 1)
	SET(WAMR_BUILD_AOT 1)
	SET(WAMR_BUILD_JIT 0)
	# Fast JIT: WAMR's own x86_64 JIT (no LLVM). Skip on:
	#   - Non-x86_64 (Fast JIT codegen only targets x86_64)
	#   - Windows (WAMR's runtime_lib.cmake:61-64 force-disables Fast JIT on
	#     Windows; AND its line 51-54 mutex (FAST_JIT=1 forces FAST_INTERP=0)
	#     fires BEFORE the Windows disable, so requesting FAST_JIT=1 on Windows
	#     leaves us with neither Fast JIT nor Fast Interpreter — only the
	#     classic interpreter (slowest mode). On Windows, request FAST_INTERP=1
	#     by setting FAST_JIT=0 here so the mutex never fires.)
	IF(CMAKE_SYSTEM_PROCESSOR MATCHES "AMD64|x86_64" AND NOT WIN32)
		SET(WAMR_BUILD_FAST_JIT 1)
	ELSE()
		SET(WAMR_BUILD_FAST_JIT 0)
	ENDIF()
	SET(WAMR_BUILD_LIBC_BUILTIN 0)
	SET(WAMR_BUILD_LIBC_WASI 1)
	SET(WAMR_BUILD_SHARED_MEMORY 0)
	SET(WAMR_BUILD_MULTI_MODULE 0)
	SET(WAMR_BUILD_REF_TYPES 1)
	SET(WAMR_BUILD_SIMD 0)
	SET(WAMR_DISABLE_HW_BOUND_CHECK 1)
	SET(WAMR_BUILD_LOAD_CUSTOM_SECTION 1)
	ADD_SUBDIRECTORY(src/libs/wamr)
	IF(WIN32)
    	target_compile_definitions(vmlib PRIVATE
            _WIN32_WINNT=0x0A00
            WINVER=0x0A00
        )
		# WAMR upstream hardcodes -lm -ldl (line 154) without platform guard.
		# Windows has no libdl — strip it from vmlib's transitive link interface.
		get_target_property(_vmlib_libs vmlib INTERFACE_LINK_LIBRARIES)
		IF(_vmlib_libs)
			list(REMOVE_ITEM _vmlib_libs "-ldl" "-lm")
			set_target_properties(vmlib PROPERTIES INTERFACE_LINK_LIBRARIES "${_vmlib_libs}")
		ENDIF()
	ENDIF()
	TARGET_INCLUDE_DIRECTORIES(qcommon PRIVATE src/libs/wamr/core/iwasm/include)
	TARGET_INCLUDE_DIRECTORIES(qcommon_ded PRIVATE src/libs/wamr/core/iwasm/include)
ENDIF()

IF(NO_VM_COMPILED)
	TARGET_COMPILE_DEFINITIONS(qcommon PRIVATE NO_VM_COMPILED)
	TARGET_COMPILE_DEFINITIONS(qcommon_ded PRIVATE NO_VM_COMPILED)
ENDIF()

# ── WiredNet ──────────────────────────────────────────────────────
# picoquic + picotls + mpack sources
# These are added as git submodules under src/libs/

# MinGW: picoquic + picotls have a handful of MSVC-only assumptions that
# break under MinGW's CRT (struct timezone redefinition, va_args macros,
# ws2tcpip include order, _dupenv_s missing).  Local patches in
# patches/picoquic-mingw/ fix each.  Apply them idempotently at configure
# time so fresh submodule checkouts (CI) get the same fixes that local
# working trees have.
#
# Idempotency: try `git apply --check --reverse` first — exits 0 only when
# the patch is already cleanly applied, so we skip.  Otherwise apply with
# --ignore-whitespace so CRLF/LF drift on Windows working trees (Git's
# autocrlf converts checked-out files) doesn't fail the patch context
# match.  Both check and apply pass --ignore-whitespace for symmetry.
IF(WIN32)
	FIND_PROGRAM(_GIT_EXE git REQUIRED)
	FILE(GLOB _PICOQUIC_PATCHES "${CMAKE_SOURCE_DIR}/patches/picoquic-mingw/*.patch")
	LIST(SORT _PICOQUIC_PATCHES)
	FOREACH(_patch ${_PICOQUIC_PATCHES})
		GET_FILENAME_COMPONENT(_patch_name "${_patch}" NAME)
		EXECUTE_PROCESS(
			COMMAND "${_GIT_EXE}" apply --check --reverse --ignore-whitespace "${_patch}"
			WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/src/libs/picoquic"
			RESULT_VARIABLE _rc_already
			OUTPUT_QUIET ERROR_QUIET)
		IF(_rc_already EQUAL 0)
			MESSAGE(STATUS "picoquic patch already applied: ${_patch_name}")
		ELSE()
			EXECUTE_PROCESS(
				COMMAND "${_GIT_EXE}" apply --ignore-whitespace "${_patch}"
				WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/src/libs/picoquic"
				RESULT_VARIABLE _rc_apply
				OUTPUT_VARIABLE _out_apply
				ERROR_VARIABLE _err_apply)
			IF(NOT _rc_apply EQUAL 0)
				MESSAGE(FATAL_ERROR
					"Failed to apply ${_patch_name}:\n${_err_apply}\n"
					"This usually means the picoquic submodule has unrelated "
					"local modifications that conflict with the patch hunks.\n"
					"To recover:\n"
					"  git -C src/libs/picoquic reset --hard\n"
					"  rm -rf build/release  # or build/debug\n"
					"  re-run cmake / make")
			ENDIF()
			MESSAGE(STATUS "applied picoquic patch: ${_patch_name}")
		ENDIF()
	ENDFOREACH()
ENDIF()

FILE(GLOB PICOQUIC_SRCS src/libs/picoquic/picoquic/*.c)

# Exclude optional picoquic TLS backend adapters — we only use OpenSSL
list(FILTER PICOQUIC_SRCS EXCLUDE REGEX ".*minicrypto.*")
list(FILTER PICOQUIC_SRCS EXCLUDE REGEX ".*mbedtls.*")

# Exclude picoquic's fusion TLS backend on all platforms — we don't compile
# the picotls fusion source (lib/fusion.c), so the symbols are unresolved.
# Our wt_picoquic_stubs.c provides a no-op stub instead.
list(FILTER PICOQUIC_SRCS EXCLUDE REGEX ".*fusion.*")

# Exclude qlog (optional logging, pulls in file I/O we don't need)
list(FILTER PICOQUIC_SRCS EXCLUDE REGEX ".*qlog.*")

# picotls — only compile the core + OpenSSL backend.
# Skip optional backends (cifra, mbedtls, brotli, fusion, uecc, minicrypto)
# to avoid pulling in dependencies we don't have.
SET(PICOTLS_SRCS
	src/libs/picotls/lib/picotls.c
	src/libs/picotls/lib/openssl.c
	src/libs/picotls/lib/pembase64.c
	src/libs/picotls/lib/asn1.c
	src/libs/picotls/lib/hpke.c
)

# Windows: picoquic+picotls call gettimeofday(), which wincompat.h remaps to
# wintimeofday(). The implementation lives in picotls' Visual-Studio project
# subdirectory (picotlsvs/picotls/wintimeofday.c) and is not in the default
# library source list. MSVC builds compile it via the .vcxproj; we add it
# explicitly here so MinGW + any future MSVC-via-CMake build link cleanly.
IF(WIN32)
	list(APPEND PICOTLS_SRCS src/libs/picotls/picotlsvs/picotls/wintimeofday.c)
ENDIF()

FILE(GLOB MPACK_SRCS    src/libs/mpack/src/mpack/*.c)

# wired/net module — WiredNet transport layer
FILE(GLOB WN_SRCS code/qcommon/wired/net/*.c)

SET(WIREDNET_ALL_SRCS ${PICOQUIC_SRCS} ${PICOTLS_SRCS} ${MPACK_SRCS} ${WN_SRCS})

# MinGW: MinGW's <inttypes.h> defines imaxabs via __CRT_INLINE which under
# strict -std=c11 expands to extern inline (strong symbol per TU) instead of
# the GNU/C89 semantics of "inline-only, no strong definition emitted". Many
# picoquic+picotls TUs include <inttypes.h> transitively, so the linker sees
# multiple definitions of imaxabs. -fgnu-inline restores the GNU inline
# semantics for these TUs only without raising the C standard floor.
# See docs/health.md "picoquic _WINDOWS guards do not match MinGW".
IF(WIN32 AND NOT MSVC)
	set_source_files_properties(${PICOQUIC_SRCS} ${PICOTLS_SRCS}
		PROPERTIES COMPILE_OPTIONS "-fgnu89-inline")
ENDIF()

# Include paths
SET(WIREDNET_INCLUDE_DIRS
	src/libs/picoquic/picoquic
	src/libs/picotls/include
	src/libs/mpack/src/mpack
	code/qcommon/wired/net
)

# OpenSSL (required by picotls for crypto primitives)
FIND_PACKAGE(OpenSSL REQUIRED)

# Add to qcommon (client) and qcommon_ded (headless server)
TARGET_SOURCES(qcommon PRIVATE ${WIREDNET_ALL_SRCS})
TARGET_INCLUDE_DIRECTORIES(qcommon PRIVATE ${WIREDNET_INCLUDE_DIRS})
TARGET_LINK_LIBRARIES(qcommon PRIVATE OpenSSL::Crypto)

TARGET_SOURCES(qcommon_ded PRIVATE ${WIREDNET_ALL_SRCS})
TARGET_INCLUDE_DIRECTORIES(qcommon_ded PRIVATE ${WIREDNET_INCLUDE_DIRS})
TARGET_LINK_LIBRARIES(qcommon_ded PRIVATE OpenSSL::Crypto)


# ── Recast/Detour navmesh (replaces AAS when USE_RECAST_NAVMESH=ON) ──
OPTION(USE_RECAST_NAVMESH "Use Recast/Detour/DetourCrowd navmesh (replaces AAS)" ON)
IF(NOT USE_RECAST_NAVMESH)
	ADD_COMPILE_DEFINITIONS(FEAT_RECAST_NAVMESH=0)
ELSE()
	# Recast / Detour / DetourCrowd sources from the vendored submodule.
	# DetourCrowd is compiled but dormant until Phase 6.
	FILE(GLOB RECAST_SRC    src/libs/recastnavigation/Recast/Source/*.cpp)
	FILE(GLOB DETOUR_SRC    src/libs/recastnavigation/Detour/Source/*.cpp)
	FILE(GLOB DETOURCROWD_SRC src/libs/recastnavigation/DetourCrowd/Source/*.cpp)

	ADD_LIBRARY(recast_nav STATIC
		${RECAST_SRC}
		${DETOUR_SRC}
		${DETOURCROWD_SRC}
	)
	# Expose only the Include dirs that the nav integration layer needs.
	TARGET_INCLUDE_DIRECTORIES(recast_nav PUBLIC
		src/libs/recastnavigation/Recast/Include
		src/libs/recastnavigation/Detour/Include
		src/libs/recastnavigation/DetourCrowd/Include
	)
	# Recast/Detour do not use C++ exceptions or RTTI.
	TARGET_COMPILE_OPTIONS(recast_nav PRIVATE
		$<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-fno-exceptions -fno-rtti>
	)

	SET(RECAST_INCLUDE_DIRS
		src/libs/recastnavigation/Recast/Include
		src/libs/recastnavigation/Detour/Include
		src/libs/recastnavigation/DetourCrowd/Include
	)

	# Nav integration layer: code/qcommon/nav/*.cpp + *.c
	FILE(GLOB NAV_CPP_SRCS code/qcommon/nav/*.cpp)
	FILE(GLOB NAV_C_SRCS   code/qcommon/nav/*.c)

	# Nav CPP files inherit from rcContext which is compiled without RTTI.
	# Apply -fno-rtti to avoid typeinfo mismatch at link time.
	SET_SOURCE_FILES_PROPERTIES(${NAV_CPP_SRCS} PROPERTIES
		COMPILE_OPTIONS "$<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-fno-exceptions;-fno-rtti>")

	TARGET_SOURCES(qcommon     PRIVATE ${NAV_CPP_SRCS} ${NAV_C_SRCS})
	TARGET_SOURCES(qcommon_ded PRIVATE ${NAV_CPP_SRCS} ${NAV_C_SRCS})

	TARGET_INCLUDE_DIRECTORIES(qcommon     PRIVATE ${RECAST_INCLUDE_DIRS})
	TARGET_INCLUDE_DIRECTORIES(qcommon_ded PRIVATE ${RECAST_INCLUDE_DIRS})

	TARGET_LINK_LIBRARIES(qcommon     PRIVATE recast_nav)
	TARGET_LINK_LIBRARIES(qcommon_ded PRIVATE recast_nav)

	# Unit test — Phase 2 hard gate 1: coordinate round-trip identity.
	# Run with: ctest -R nav_coord_round_trip
	ENABLE_TESTING()
	ADD_EXECUTABLE(nav_coord_test
		tests/nav_coord_test.c
		code/qcommon/nav/nav_coord.c
	)
	ADD_TEST(NAME nav_coord_round_trip COMMAND nav_coord_test)
ENDIF()


set(OGG_SRCS src/libs/ogg/src/bitwise.c src/libs/ogg/src/framing.c)
add_library(ogg STATIC ${OGG_SRCS})
target_include_directories(ogg PUBLIC src/libs/ogg/include)

# ── libopus (Opus audio codec) ─────────────────────────────────────
file(GLOB OPUS_CELT_SRCS src/libs/opus/celt/*.c)
file(GLOB OPUS_SILK_SRCS src/libs/opus/silk/*.c)
file(GLOB OPUS_SILK_FLOAT_SRCS src/libs/opus/silk/float/*.c)
set(OPUS_SRC_SRCS
    src/libs/opus/src/opus.c
    src/libs/opus/src/opus_decoder.c
    src/libs/opus/src/opus_encoder.c
    src/libs/opus/src/opus_multistream.c
    src/libs/opus/src/opus_multistream_decoder.c
    src/libs/opus/src/opus_multistream_encoder.c
    src/libs/opus/src/repacketizer.c
    src/libs/opus/src/analysis.c
    src/libs/opus/src/mlp.c
    src/libs/opus/src/mlp_data.c
    src/libs/opus/src/extensions.c
)
# Exclude encoder-only demo/test files from celt
list(FILTER OPUS_CELT_SRCS EXCLUDE REGEX "opus_custom_demo\\.c$")
# Exclude silk/fixed/ entirely (we use FLOATING_POINT)
# Platform-specific SIMD sources — only compile on matching architecture
set(OPUS_ARCH_SRCS "")
IF(CMAKE_SYSTEM_PROCESSOR MATCHES "AMD64|x86_64|x86|i.86")
    file(GLOB OPUS_CELT_X86_SRCS src/libs/opus/celt/x86/*.c)
    file(GLOB OPUS_SILK_X86_SRCS src/libs/opus/silk/x86/*.c)
    file(GLOB OPUS_SILK_FLOAT_X86_SRCS src/libs/opus/silk/float/x86/*.c)
    list(APPEND OPUS_ARCH_SRCS ${OPUS_CELT_X86_SRCS} ${OPUS_SILK_X86_SRCS} ${OPUS_SILK_FLOAT_X86_SRCS})
    # Each SSE/AVX file requires the matching ISA flag; without it GCC/Clang reject the intrinsics
    foreach(_f
        src/libs/opus/celt/x86/pitch_sse2.c
        src/libs/opus/celt/x86/vq_sse2.c
    )
        set_source_files_properties(${CMAKE_SOURCE_DIR}/${_f} PROPERTIES COMPILE_FLAGS "-msse2")
    endforeach()
    foreach(_f
        src/libs/opus/celt/x86/celt_lpc_sse4_1.c
        src/libs/opus/celt/x86/pitch_sse4_1.c
        src/libs/opus/silk/x86/NSQ_del_dec_sse4_1.c
        src/libs/opus/silk/x86/NSQ_sse4_1.c
        src/libs/opus/silk/x86/VAD_sse4_1.c
        src/libs/opus/silk/x86/VQ_WMat_EC_sse4_1.c
    )
        set_source_files_properties(${CMAKE_SOURCE_DIR}/${_f} PROPERTIES COMPILE_FLAGS "-msse4.1")
    endforeach()
    foreach(_f
        src/libs/opus/celt/x86/pitch_avx.c
    )
        set_source_files_properties(${CMAKE_SOURCE_DIR}/${_f} PROPERTIES COMPILE_FLAGS "-mavx")
    endforeach()
    foreach(_f
        src/libs/opus/silk/x86/NSQ_del_dec_avx2.c
        src/libs/opus/silk/float/x86/inner_product_FLP_avx2.c
    )
        set_source_files_properties(${CMAKE_SOURCE_DIR}/${_f} PROPERTIES COMPILE_FLAGS "-mavx2 -mfma")
    endforeach()
ELSEIF(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64|arm")
    file(GLOB OPUS_CELT_ARM_SRCS src/libs/opus/celt/arm/*.c)
    file(GLOB OPUS_SILK_ARM_SRCS src/libs/opus/silk/arm/*.c)
    # Exclude NE10 files (require external NE10 library we don't vendor)
    list(FILTER OPUS_CELT_ARM_SRCS EXCLUDE REGEX "ne10\\.c$")
    list(APPEND OPUS_ARCH_SRCS ${OPUS_CELT_ARM_SRCS} ${OPUS_SILK_ARM_SRCS})
ENDIF()

set(OPUS_ALL_SRCS
    ${OPUS_CELT_SRCS} ${OPUS_SILK_SRCS} ${OPUS_SILK_FLOAT_SRCS}
    ${OPUS_ARCH_SRCS} ${OPUS_SRC_SRCS}
)

add_library(opus STATIC ${OPUS_ALL_SRCS})
target_include_directories(opus PUBLIC src/libs/opus/include)
target_include_directories(opus PRIVATE
    src/libs/opus
    src/libs/opus/include/opus
    src/libs/opus/celt
    src/libs/opus/silk
    src/libs/opus/silk/float
    src/libs/opus/silk/fixed
    src/libs/opus/src
)
target_compile_definitions(opus PRIVATE OPUS_BUILD HAVE_CONFIG_H)

# ── libopusfile (Opus file reader) ─────────────────────────────────
set(OPUSFILE_SRCS
    src/libs/opusfile/src/info.c
    src/libs/opusfile/src/internal.c
    src/libs/opusfile/src/opusfile.c
    src/libs/opusfile/src/stream.c
)
add_library(opusfile STATIC ${OPUSFILE_SRCS})
target_include_directories(opusfile PUBLIC src/libs/opusfile/include)
target_include_directories(opusfile PRIVATE src/libs/opusfile/src)
target_link_libraries(opusfile PUBLIC opus ogg)
# libopusfile needs libogg headers and opus headers (without opus/ prefix)
target_include_directories(opusfile PUBLIC src/libs/ogg/include)
target_include_directories(opusfile PRIVATE src/libs/opus/include/opus)

# client + cURL + Wired UI + miniaudio test
AUX_SOURCE_DIRECTORY(code/client CLIENT_SRCS)
list(FILTER CLIENT_SRCS EXCLUDE REGEX "cl_ui\\.c$")
list(FILTER CLIENT_SRCS EXCLUDE REGEX "snd_miniaudio_test\\.c$")

# snd_miniaudio_test.c is a standalone test runner with its own main();
# it must NOT be linked into the client library or it duplicates _main.
list(FILTER CLIENT_SRCS EXCLUDE REGEX "snd_miniaudio_test\\.c$")
AUX_SOURCE_DIRECTORY(code/client/wired/ui          CLIENT_SRCS)
AUX_SOURCE_DIRECTORY(code/client/wired/store       CLIENT_SRCS)
# V-13 (2026-05-25): code/client/wired/hud/ top-level files relocated to
# code/client/wired/ui/ as cl_wired_ui_hud_*.
# hud-element-relocation (2026-05-31): the 46 cl_wired_hud_elem_*.c element
# modules moved from code/client/wired/hud/elements/ to code/client/wired/ui/
# elements/ — they now compile via the ui/elements glob below; the old
# hud/elements glob is retired. cl_wired_ui_hud_register.c (in ui/) is the
# PERMANENT C-13 HUD registration infra (name→element table + config-bridge
# adapter); it binds the elements by symbol. The table is load-bearing at
# create-time (registry/element create-signature mismatch) — not a retireable
# shim. See that file's header for the rationale.
# Chain 6.0: the procedural-crosshair Lua pipeline (cl_wired_crosshair.c) is a
# top-level wired/hud module — compile the directory back in (it is already on
# the include path below). It stays in code/client/wired/hud/.
AUX_SOURCE_DIRECTORY(code/client/wired/hud          CLIENT_SRCS)
# cinematic-scene Lua trigger surface (scene.play/stop/skip on the System VM;
# bridges to the cgame playback state via the sceneplay/scenestop/sceneskip commands).
AUX_SOURCE_DIRECTORY(code/client/wired/scene       CLIENT_SRCS)
# localization table loader (scripts/l10n/<lang>.lua key->text on the System VM).
AUX_SOURCE_DIRECTORY(code/client/wired/l10n        CLIENT_SRCS)
AUX_SOURCE_DIRECTORY(code/client/wired/ui/panels   CLIENT_SRCS)
AUX_SOURCE_DIRECTORY(code/client/wired/ui/elements CLIENT_SRCS)
AUX_SOURCE_DIRECTORY(code/client/wired/ui/policy   CLIENT_SRCS)
# bg_misc.c, bg_weapons.c and bg_gametypes.c are in QCOMMON_SRCS (shared with headless server); not duplicated here
IF(NOT USE_CURL)
	list(REMOVE_ITEM CLIENT_SRCS code/client/cl_curl.c)
ENDIF()

#ADD_LIBRARY(client OBJECT ${CLIENT_SRCS})

# ── JPEG codec ────────────────────────────────────────────────────
# Default: libjpeg-turbo submodule (src/libs/libjpeg-turbo).
# Override: USE_SYSTEM_JPEG=ON uses the system libjpeg.
# Legacy:   USE_JPEG_TURBO=OFF falls back to the embedded libjpeg in
#           src/libs/jpeg (fenced with FEAT_LEGACY_LIBJPEG; remove later).
IF(USE_SYSTEM_JPEG)
	ADD_LIBRARY(client OBJECT ${CLIENT_SRCS})
	FIND_PACKAGE(JPEG REQUIRED)
	TARGET_COMPILE_DEFINITIONS(client PRIVATE USE_SYSTEM_JPEG)
	TARGET_LINK_LIBRARIES(client PRIVATE ${JPEG_LIBRARIES})
ELSEIF(USE_JPEG_TURBO)
	# Submodule: src/libs/libjpeg-turbo
	SET(ENABLE_SHARED        OFF CACHE BOOL   "" FORCE)
	SET(WITH_TOOLS           OFF CACHE BOOL   "" FORCE)
	SET(WITH_TESTS           OFF CACHE BOOL   "" FORCE)
	SET(WITH_SIMD            ON  CACHE BOOL   "" FORCE)
	SET(REQUIRE_SIMD         OFF CACHE BOOL   "" FORCE)
	# libjpeg-turbo validates these install dirs; set defaults so subdirectory configure passes
	SET(CMAKE_INSTALL_BINDIR     "bin"                     CACHE STRING "" FORCE)
	SET(CMAKE_INSTALL_LIBDIR     "lib"                     CACHE STRING "" FORCE)
	SET(CMAKE_INSTALL_INCLUDEDIR "include"                 CACHE STRING "" FORCE)
	SET(CMAKE_INSTALL_DOCDIR     "share/doc/libjpeg-turbo" CACHE STRING "" FORCE)
	SET(CMAKE_INSTALL_MANDIR     "share/man"               CACHE STRING "" FORCE)
	# libjpeg-turbo upstream refuses add_subdirectory() via a FATAL_ERROR guard.
	# Neutralize the guard at configure time so the vendored submodule builds
	# without maintaining a fork. Idempotent: no write if already patched.
	SET(_JPEGTURBO_CML "${CMAKE_CURRENT_SOURCE_DIR}/src/libs/libjpeg-turbo/CMakeLists.txt")
	FILE(READ "${_JPEGTURBO_CML}" _JPEGTURBO_SRC)
	STRING(REPLACE
		"if(NOT CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)"
		"if(FALSE) # wired: add_subdirectory() guard neutralized"
		_JPEGTURBO_PATCHED "${_JPEGTURBO_SRC}")
	IF(NOT _JPEGTURBO_PATCHED STREQUAL _JPEGTURBO_SRC)
		FILE(WRITE "${_JPEGTURBO_CML}" "${_JPEGTURBO_PATCHED}")
	ENDIF()
	ADD_SUBDIRECTORY(src/libs/libjpeg-turbo EXCLUDE_FROM_ALL)
	ADD_LIBRARY(client OBJECT ${CLIENT_SRCS})
	TARGET_INCLUDE_DIRECTORIES(client PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}/src/libs/libjpeg-turbo/src
		${CMAKE_CURRENT_SOURCE_DIR}/src/libs/libjpeg-turbo
		${CMAKE_CURRENT_BINARY_DIR}/src/libs/libjpeg-turbo)
	TARGET_LINK_LIBRARIES(client PRIVATE jpeg-static)
ELSE()
	# Legacy embedded libjpeg — fenced with FEAT_LEGACY_LIBJPEG; safe to remove
	AUX_SOURCE_DIRECTORY(src/libs/jpeg JPEG_SRCS)
	ADD_LIBRARY(client OBJECT ${CLIENT_SRCS} ${JPEG_SRCS})
	TARGET_COMPILE_DEFINITIONS(client PRIVATE FEAT_LEGACY_LIBJPEG)
	TARGET_INCLUDE_DIRECTORIES(client PRIVATE src/libs/jpeg)
ENDIF()

# Opus codec + wired bucket dirs: bare #include "cl_wired_*.h" works across buckets
target_include_directories(client PRIVATE
    src/libs/opusfile/include
    src/libs/ogg/include
    src/libs/opus/include
    src/libs/opus/include/opus
    code/client/wired/ui
    code/client/wired/hud
    code/client/wired/store
    code/client/wired/scripting
)

# Clay layout engine — WiredUI compositor only. Client target only;
# renderer DLLs / server / VMs MUST NOT see clay.h on their include path
# (per docs/wiredui-compositor-spec.md §5.1 and §10).
IF(USE_WIRED_UI_CLAY)
    target_include_directories(client PRIVATE src/libs/clay)
    target_compile_definitions(client PRIVATE USE_WIRED_UI_CLAY=1)
ENDIF()

# renderers
AUX_SOURCE_DIRECTORY(code/renderercommon RENDERER_COMMON_SRCS)
# r_image_probe.c is an offline-tool-only TU (consumed by extract-meta).
# It calls FS_FOpenFileRead/FS_FCloseFile directly, which the renderer
# DLLs can't satisfy (they go through refimport_t::FS_ReadFile). The
# engine never calls R_ImageResolves, so the file is harmless to omit
# from every renderer DLL link. Two forms because AUX_SOURCE_DIRECTORY
# may emit either depending on CMake version.
LIST(REMOVE_ITEM RENDERER_COMMON_SRCS code/renderercommon/r_image_probe.c)
LIST(REMOVE_ITEM RENDERER_COMMON_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/code/renderercommon/r_image_probe.c)
AUX_SOURCE_DIRECTORY(code/renderer RENDERER_GL_SRCS)
AUX_SOURCE_DIRECTORY(code/renderer2 RENDERER_GL2_SRCS)
AUX_SOURCE_DIRECTORY(code/renderervk RENDERER_VK_SRCS)

# ── Phase 7 RAL — Vulkan backend skeleton ──────────────────────────────────
# code/renderer/ral/        — public RAL surface (headers only)
# code/renderer/ral_vulkan/ — the Vulkan backend (skeleton in Phase 7.1)
# Built as a static lib and linked into the Vulkan renderer DLL alongside the
# renderervk objects (it references the DLL's `ri` global, defined in
# code/renderervk/tr_init.c, so it must only link into a target that also has
# renderervk). FEAT_RAL (default 0; pass -DFEAT_RAL=1 to flip) gates whether
# the *renderer* uses the RAL; the static lib is always built so the surface
# stays link-validated. No file under code/renderervk/ includes ral/*.h while
# FEAT_RAL is 0. The RAL's Ral_Dump() entry point is exported (Q_EXPORT) and
# resolved by the client's "\ral_dump" command via Sys_LoadFunction.
SET(RAL_VK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/code/renderer/ral_vulkan")
SET(RAL_VK_SRCS
	${RAL_VK_DIR}/ral_vulkan_backend.c
	${RAL_VK_DIR}/ral_vulkan_caps.c
	${RAL_VK_DIR}/ral_vulkan_memory.c
	${RAL_VK_DIR}/ral_vulkan_resource.c
	${RAL_VK_DIR}/ral_vulkan_pipeline.c
	${RAL_VK_DIR}/ral_vulkan_command.c
	${RAL_VK_DIR}/ral_vulkan_sync.c
	${RAL_VK_DIR}/ral_vulkan_swapchain.c
	${RAL_VK_DIR}/ral_vulkan_query.c
)
ADD_LIBRARY(ral_vulkan STATIC ${RAL_VK_SRCS})
TARGET_INCLUDE_DIRECTORIES(ral_vulkan PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}/code/renderer/ral
	${CMAKE_CURRENT_SOURCE_DIR}/code/renderercommon
	${CMAKE_CURRENT_SOURCE_DIR}/code/qcommon)
IF(USE_RENDERER_DLOPEN)
	TARGET_COMPILE_DEFINITIONS(ral_vulkan PRIVATE USE_RENDERER_DLOPEN)
ENDIF()
IF(WIN32 AND NOT MSVC)
	# match the relaxation the renderer DLLs get (legacy const-correctness patterns)
	TARGET_COMPILE_OPTIONS(ral_vulkan PRIVATE -Wno-error=incompatible-pointer-types -Wno-error=int-conversion)
ENDIF()
IF(UNIX)
	FIND_PACKAGE(Threads)
	IF(Threads_FOUND)
		TARGET_LINK_LIBRARIES(ral_vulkan Threads::Threads)
	ENDIF()
ENDIF()

# Link ral_vulkan into a renderer target. --whole-archive forces every object
# into the link so Ral_Dump (which nothing else references) is exported from
# the DLL; also puts code/renderer/ral/ on the target's include path.
FUNCTION(WIRED_LINK_RAL_VULKAN _target)
	TARGET_INCLUDE_DIRECTORIES(${_target} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/code/renderer/ral)
	IF(MSVC)
		TARGET_LINK_LIBRARIES(${_target} ral_vulkan)
		TARGET_LINK_OPTIONS(${_target} PRIVATE "/WHOLEARCHIVE:ral_vulkan")
	ELSE()
		TARGET_LINK_LIBRARIES(${_target} -Wl,--whole-archive ral_vulkan -Wl,--no-whole-archive)
	ENDIF()
ENDFUNCTION()

# ── Phase 7.3b — shader translation toolchain ──────────────────────────────
# Offline tool: shader_xlate. Reads SPIR-V, emits MSL / GLSL 4.30 / GLSL ES 300
# via the SPIRV-Cross C++ API (submodule at src/libs/SPIRV-Cross/, tagged
# vulkan-sdk-1.4.341.0 to match the engine's pinned Vulkan SDK 1.4.341.1) and
# WGSL via shelling out to the `naga` CLI (the "Option B2" build-time-tool
# model per docs/phase-7-ral-design.md §16.2 — no Rust links into the engine
# at runtime). Build-time only; the translated outputs aren't consumed yet
# (Phase 7.3c integrates RAL pipeline creation with them).
#
# Default OFF in mainline builds (don't slow renderer-only iteration);
# auto-enabled when FEAT_RAL is on; explicit:
#   cmake -DWIRED_BUILD_SHADER_XLATE=ON -B build/debug
#   ninja -C build/debug shader_xlate
OPTION(WIRED_BUILD_SHADER_XLATE
	"Build the offline GLSL → SPIR-V → {MSL, GLSL, GLSL ES, WGSL} translator (needs src/libs/SPIRV-Cross submodule; WGSL needs the `naga` CLI on PATH — see BUILD-RUST.md)"
	OFF)
IF(FEAT_RAL)
	SET(WIRED_BUILD_SHADER_XLATE ON CACHE BOOL "" FORCE)
ENDIF()

IF(WIRED_BUILD_SHADER_XLATE)
	SET(SPIRV_CROSS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/libs/SPIRV-Cross")
	IF(NOT EXISTS "${SPIRV_CROSS_DIR}/CMakeLists.txt")
		MESSAGE(WARNING
			"shader_xlate requested but src/libs/SPIRV-Cross is not populated. Run:\n"
			"  git submodule add https://github.com/KhronosGroup/SPIRV-Cross.git src/libs/SPIRV-Cross\n"
			"  cd src/libs/SPIRV-Cross && git checkout vulkan-sdk-1.4.341.0 && cd ../../..\n"
			"  git add .gitmodules src/libs/SPIRV-Cross && git commit\n"
			"shader_xlate target skipped this configure.")
	ELSE()
		# CMake 4.x: SPIRV-Cross still declares cmake_minimum_required(VERSION 3.10);
		# pin the policy floor so the build is quiet.
		SET(CMAKE_POLICY_VERSION_MINIMUM 3.10 CACHE STRING "" FORCE)
		# Trim SPIRV-Cross to the three backends the tool uses.
		SET(SPIRV_CROSS_CLI               OFF CACHE BOOL "" FORCE)
		SET(SPIRV_CROSS_ENABLE_TESTS      OFF CACHE BOOL "" FORCE)
		SET(SPIRV_CROSS_ENABLE_HLSL       OFF CACHE BOOL "" FORCE)
		SET(SPIRV_CROSS_ENABLE_CPP        OFF CACHE BOOL "" FORCE)
		SET(SPIRV_CROSS_ENABLE_REFLECT    OFF CACHE BOOL "" FORCE)
		SET(SPIRV_CROSS_ENABLE_C_API      OFF CACHE BOOL "" FORCE)
		SET(SPIRV_CROSS_ENABLE_UTIL       OFF CACHE BOOL "" FORCE)
		SET(SPIRV_CROSS_ENABLE_GLSL       ON  CACHE BOOL "" FORCE)
		SET(SPIRV_CROSS_ENABLE_MSL        ON  CACHE BOOL "" FORCE)
		SET(SPIRV_CROSS_SKIP_INSTALL      ON  CACHE BOOL "" FORCE)
		SET(SPIRV_CROSS_FORCE_PIC         ON  CACHE BOOL "" FORCE)
		SET(SPIRV_CROSS_SHARED            OFF CACHE BOOL "" FORCE)
		SET(SPIRV_CROSS_STATIC            ON  CACHE BOOL "" FORCE)
		ADD_SUBDIRECTORY("${SPIRV_CROSS_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/spirv-cross" EXCLUDE_FROM_ALL)

		ADD_EXECUTABLE(shader_xlate code/tools/shader_xlate/main.cpp)
		TARGET_LINK_LIBRARIES(shader_xlate spirv-cross-msl spirv-cross-glsl spirv-cross-core)
		TARGET_INCLUDE_DIRECTORIES(shader_xlate PRIVATE "${SPIRV_CROSS_DIR}")
		SET_TARGET_PROPERTIES(shader_xlate PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
		IF(MSVC)
			TARGET_COMPILE_OPTIONS(shader_xlate PRIVATE /EHsc)
		ENDIF()

		# Build artefacts: build/translated_shaders/ is where compile_xlate.mjs
		# (Phase 7.3c — runtime integration) will land the per-backend outputs.
		FILE(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/translated_shaders")

		# naga detection — informational at configure time, runtime-checked by the tool.
		FIND_PROGRAM(NAGA_EXECUTABLE NAMES naga)
		IF(NAGA_EXECUTABLE)
			MESSAGE(STATUS "shader_xlate: naga CLI found at ${NAGA_EXECUTABLE} -- WGSL output enabled")
		ELSE()
			MESSAGE(STATUS "shader_xlate: naga CLI not on PATH -- WGSL skipped (Rust + naga-cli setup: see BUILD-RUST.md)")
		ENDIF()
	ENDIF()
ENDIF()

# GL2: stringify is a host tool, not a library source — remove it from the list
LIST(REMOVE_ITEM RENDERER_GL2_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/code/renderer2/stringify.c)

# GL2: build the stringify tool and use it to generate fallback C strings from GLSL files
ADD_EXECUTABLE(stringify code/renderer2/stringify.c)

FILE(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/renderer2_glsl")
FILE(GLOB RENDERER_GL2_GLSL_FILES "${CMAKE_CURRENT_SOURCE_DIR}/code/renderer2/glsl/*.glsl")
# MSDF shaders are already inlined as static strings in tr_glsl.c; skip them here
LIST(FILTER RENDERER_GL2_GLSL_FILES EXCLUDE REGEX "msdf_")

SET(RENDERER_GL2_FALLBACK_SRCS)
FOREACH(GLSL_FILE ${RENDERER_GL2_GLSL_FILES})
	GET_FILENAME_COMPONENT(GLSL_BASE ${GLSL_FILE} NAME_WE)
	SET(GENERATED_C "${CMAKE_CURRENT_BINARY_DIR}/renderer2_glsl/${GLSL_BASE}.c")
	ADD_CUSTOM_COMMAND(
		OUTPUT ${GENERATED_C}
		COMMAND stringify ${GLSL_FILE} ${GENERATED_C}
		DEPENDS stringify ${GLSL_FILE}
		COMMENT "Generating fallbackShader_${GLSL_BASE}"
	)
	LIST(APPEND RENDERER_GL2_FALLBACK_SRCS ${GENERATED_C})
ENDFOREACH()

IF(USE_RENDERER_DLOPEN)
	SET(AUX_SRCS
		code/qcommon/q_shared.c
		code/qcommon/q_string.c
		code/qcommon/puff.c
		code/qcommon/q_math.c
		# util/ functions extracted from q_shared.c — must ship with every renderer module
		code/qcommon/util/math.c
		code/qcommon/util/hash.c
		code/qcommon/util/string.c
	)
	TARGET_COMPILE_DEFINITIONS(client PRIVATE USE_RENDERER_DLOPEN RENDERER_PREFIX="${RENDERER_PREFIX}" RENDERER_DEFAULT=${RENDERER_DEFAULT})

	ADD_LIBRARY(${RENDERER_PREFIX}_opengl${RENDEXT} SHARED ${RENDERER_GL_SRCS} ${RENDERER_COMMON_SRCS} ${AUX_SRCS})
	TARGET_COMPILE_DEFINITIONS(${RENDERER_PREFIX}_opengl${RENDEXT} PRIVATE USE_RENDERER_DLOPEN)
	TARGET_INCLUDE_DIRECTORIES(${RENDERER_PREFIX}_opengl${RENDEXT} PRIVATE code/qcommon)
	SET_TARGET_PROPERTIES(${RENDERER_PREFIX}_opengl${RENDEXT} PROPERTIES PREFIX "")

	ADD_LIBRARY(${RENDERER_PREFIX}_opengl2${RENDEXT} SHARED ${RENDERER_GL2_SRCS} ${RENDERER_GL2_FALLBACK_SRCS} ${RENDERER_COMMON_SRCS} ${AUX_SRCS})
	TARGET_COMPILE_DEFINITIONS(${RENDERER_PREFIX}_opengl2${RENDEXT} PRIVATE USE_RENDERER_DLOPEN)
	TARGET_INCLUDE_DIRECTORIES(${RENDERER_PREFIX}_opengl2${RENDEXT} PRIVATE code/qcommon)
	SET_TARGET_PROPERTIES(${RENDERER_PREFIX}_opengl2${RENDEXT} PROPERTIES PREFIX "")

	ADD_LIBRARY(${RENDERER_PREFIX}_vulkan${RENDEXT} SHARED ${RENDERER_VK_SRCS} ${RENDERER_COMMON_SRCS} ${AUX_SRCS})
	TARGET_COMPILE_DEFINITIONS(${RENDERER_PREFIX}_vulkan${RENDEXT} PRIVATE USE_RENDERER_DLOPEN)
	TARGET_INCLUDE_DIRECTORIES(${RENDERER_PREFIX}_vulkan${RENDEXT} PRIVATE code/qcommon)
	SET_TARGET_PROPERTIES(${RENDERER_PREFIX}_vulkan${RENDEXT} PROPERTIES PREFIX "")
	# Emit a linker map file alongside the DLL — includes every symbol (static
	# functions, compiler-generated helpers, etc.) with its VMA. Built-in
	# objdump/addr2line resolution of caller IPs against the stripped DLL is
	# sparse (statics are omitted), so the map is the authoritative source for
	# symbolicating crash-trace return addresses. The flag is GNU-ld / lld
	# only; if a non-GNU toolchain is ever swapped in here, this line is the
	# one to gate behind `IF(CMAKE_C_COMPILER_ID STREQUAL "GNU" OR …)`.
	TARGET_LINK_OPTIONS(${RENDERER_PREFIX}_vulkan${RENDEXT} PRIVATE "LINKER:-Map=${RENDERER_PREFIX}_vulkan${RENDEXT}.map")
	# Phase 7: pull in the (dormant, FEAT_RAL-gated) RAL Vulkan backend skeleton
	WIRED_LINK_RAL_VULKAN(${RENDERER_PREFIX}_vulkan${RENDEXT})

	# Linux: renderer shared libs need libm for math functions (powf etc.)
	# and are dlopen'd at runtime, so allow host-process symbols to remain undefined.
	IF(UNIX AND NOT APPLE)
		TARGET_LINK_LIBRARIES(${RENDERER_PREFIX}_opengl${RENDEXT} m)
		TARGET_LINK_LIBRARIES(${RENDERER_PREFIX}_opengl2${RENDEXT} m)
		TARGET_LINK_LIBRARIES(${RENDERER_PREFIX}_vulkan${RENDEXT} m)
		TARGET_LINK_OPTIONS(${RENDERER_PREFIX}_opengl${RENDEXT} PRIVATE "LINKER:--unresolved-symbols=ignore-in-object-files")
		TARGET_LINK_OPTIONS(${RENDERER_PREFIX}_opengl2${RENDEXT} PRIVATE "LINKER:--unresolved-symbols=ignore-in-object-files")
		TARGET_LINK_OPTIONS(${RENDERER_PREFIX}_vulkan${RENDEXT} PRIVATE "LINKER:--unresolved-symbols=ignore-in-object-files")
	ENDIF()

	# MinGW: renderer shared libs are created before the global
	# add_compile_options(-Wno-error=incompatible-pointer-types ...) at the WIN32
	# platform-detection block (~line 642), so they don't inherit the relaxation.
	# Apply the same flags explicitly here. Mirrors the legacy char**/const char**
	# patterns the qcommon-side relaxation accommodates.
	IF(WIN32 AND NOT MSVC)
		TARGET_COMPILE_OPTIONS(${RENDERER_PREFIX}_opengl${RENDEXT}  PRIVATE -Wno-error=incompatible-pointer-types -Wno-error=int-conversion)
		TARGET_COMPILE_OPTIONS(${RENDERER_PREFIX}_opengl2${RENDEXT} PRIVATE -Wno-error=incompatible-pointer-types -Wno-error=int-conversion)
		TARGET_COMPILE_OPTIONS(${RENDERER_PREFIX}_vulkan${RENDEXT}  PRIVATE -Wno-error=incompatible-pointer-types -Wno-error=int-conversion)
	ENDIF()
ELSE()
	IF(USE_VULKAN)
		ADD_LIBRARY(${RENDERER_PREFIX}_vulkan OBJECT ${RENDERER_VK_SRCS} ${RENDERER_COMMON_SRCS})
		# Statically-linked Vulkan renderer compiles into the client. Give the
		# OBJECT lib the same include paths the DLOPEN DLL gets (code/qcommon at
		# line ~857) plus code/renderer/ral — the renderervk sources reference the
		# RAL surface (Ral_* in vk.c, vk_ral_textures.c) and need its headers. The
		# whole-archive helper is for the DLL (force-exporting Ral_Dump); the
		# static client resolves only the Ral_* symbols renderervk actually calls,
		# so a plain link of ral_vulkan into the executable suffices (done at the
		# client link list below). Just put the include path on the OBJECT lib here.
		TARGET_INCLUDE_DIRECTORIES(${RENDERER_PREFIX}_vulkan PRIVATE
			code/qcommon
			${CMAKE_CURRENT_SOURCE_DIR}/code/renderer/ral)
	ELSE()
		ADD_LIBRARY(${RENDERER_PREFIX}_opengl OBJECT ${RENDERER_GL_SRCS} ${RENDERER_COMMON_SRCS})
		TARGET_INCLUDE_DIRECTORIES(${RENDERER_PREFIX}_opengl PRIVATE code/qcommon)
	ENDIF()
ENDIF()

# common function, window system
IF(UNIX)
	SET (Q3_SRCS
		code/unix/unix_main.c
		code/unix/unix_shared.c
		code/unix/linux_signals.c)

	# The window/input/surface backend is SDL3 on every platform (code/sdl).
	AUX_SOURCE_DIRECTORY(code/sdl WINDOW_SYSTEM_SRCS)

	IF(APPLE)
		# macOS: build the engine binaries as plain executables. The combined
		# product bundle (containing both wired and wired-headless under
		# Contents/MacOS/) is assembled by a custom target later in this file —
		# see "Combined macOS bundle assembly".
		SET(EXE_TYPE "")
		list(APPEND Q3_SRCS code/macosx/macosx_main.c)
	ENDIF()
ELSEIF(WIN32)
	# Per-target subsystem: client is GUI (game has a window), headless is
	# console (Linux-style stdio so smoke harnesses, NSSM-services, Docker
	# logs, terminal redirection etc. all see stdout/stderr). See
	# docs/health.md "wired-headless Windows GUI-subsystem blocks smoke output capture"
	# and docs/launcher.md (analogous launcher CLI dual-mode pattern).
	SET(EXE_TYPE_CLIENT WIN32) # GUI subsystem (-mwindows / /SUBSYSTEM:WINDOWS)
	SET(EXE_TYPE_DED    "")    # Console subsystem (default, no flag)
	IF(MSVC)
		SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /MANIFEST:NO")
		ADD_COMPILE_DEFINITIONS(_CRT_SECURE_NO_WARNINGS) # v3.12+
	ELSE()
		# MinGW: windres needs the source root to resolve relative paths in .rc files
		SET(CMAKE_RC_FLAGS "${CMAKE_RC_FLAGS} -I${CMAKE_SOURCE_DIR}")
		# GCC 14+ treats incompatible-pointer-types as error; Quake3 codebase uses
		# legacy char**/const char** patterns that are safe but technically incorrect.
		ADD_COMPILE_OPTIONS(-Wno-error=incompatible-pointer-types -Wno-error=int-conversion)
	ENDIF()

	# Q3_SRCS holds files that go into BOTH executables. After phase 3 of
	# the Windows-console modernization (2026-05-05), win_syscon.c is a
	# single stdio shim shared by client and headless — no per-target
	# split needed for it. Client adds win_console_attach.c on top
	# (AttachConsole shim for the GUI-subsystem binary).
	SET(Q3_SRCS
		code/win32/win_main.c
		code/win32/win_shared.c
		code/win32/win_syscon.c
		code/win32/win_resource.rc)

	# Note: audio is handled by code/client/snd_miniaudio.c (vendored
	# miniaudio.h), not by a per-platform SNDDMA backend.
	#
	# The window/input/surface backend is SDL3 (code/sdl). The OS-tier files in
	# Q3_SRCS above (win_main/win_shared/win_syscon/win_resource.rc: registry,
	# CPU, console, dedicated path) are platform-native and stay.
	AUX_SOURCE_DIRECTORY(code/sdl WINDOW_SYSTEM_SRCS)
ENDIF(UNIX)

# Define _DEBUG for Debug builds so signal handlers, backtraces, and
# developer diagnostics compile in (guarded by #ifdef _DEBUG in code).
ADD_COMPILE_DEFINITIONS($<$<CONFIG:Debug>:_DEBUG>)

ADD_LIBRARY(windowSystem OBJECT ${WINDOW_SYSTEM_SRCS})

IF(NOT MSVC)
	find_package(SDL3 REQUIRED)
	TARGET_LINK_LIBRARIES(windowSystem SDL3::SDL3)
ENDIF()

IF(USE_OPENGL)
	TARGET_COMPILE_DEFINITIONS(windowSystem PRIVATE USE_OPENGL_API)
	TARGET_COMPILE_DEFINITIONS(client PRIVATE USE_OPENGL_API)
ENDIF()

IF(USE_VULKAN)
	TARGET_COMPILE_DEFINITIONS(windowSystem PRIVATE USE_VULKAN_API)
	TARGET_COMPILE_DEFINITIONS(client PRIVATE USE_VULKAN_API)
ENDIF()

IF(USE_RENDERER_DLOPEN OR NOT USE_VULKAN)
	TARGET_COMPILE_DEFINITIONS(client PRIVATE USE_OPENGL_API)
ENDIF()

# client executable

# EXE_TYPE differs per platform: Linux/macOS use a single EXE_TYPE
# (set above to MACOSX_BUNDLE on macOS, empty on Linux). Windows uses
# split EXE_TYPE_CLIENT / EXE_TYPE_DED — bridge the two conventions.
IF(WIN32)
	SET(_CLIENT_EXE_TYPE ${EXE_TYPE_CLIENT})
	SET(_DED_EXE_TYPE    ${EXE_TYPE_DED})
ELSE()
	SET(_CLIENT_EXE_TYPE ${EXE_TYPE})
	SET(_DED_EXE_TYPE    ${EXE_TYPE})
ENDIF()

ADD_EXECUTABLE(${CNAME}${BINEXT} ${_CLIENT_EXE_TYPE} ${Q3_SRCS})
TARGET_LINK_LIBRARIES(${CNAME}${BINEXT} qcommon botlib client windowSystem opusfile)
IF(WIN32)
	# Client adds the AttachConsole shim (phase 2 of Windows-console
	# modernization, 2026-05-05) — reattaches stdio to parent console when
	# this GUI-subsystem binary is launched from a terminal. Headless
	# doesn't need it (console subsystem auto-attaches at process start).
	# The shared win_syscon.c is in Q3_SRCS, linked into both targets.
	TARGET_SOURCES(${CNAME}${BINEXT} PRIVATE code/win32/win_console_attach.c)
ENDIF()
#TARGET_LINK_OPTIONS(${CNAME} PRIVATE -fno-pie) # v3.13+

IF(NOT USE_RENDERER_DLOPEN)
	IF(USE_VULKAN)
		# Static Vulkan: the renderervk OBJECT lib references the RAL Vulkan
		# backend (Ral_* in vk.c / vk_ral_textures.c), which lives in the
		# ral_vulkan static lib. The DLOPEN path bundles it into the DLL via
		# WIRED_LINK_RAL_VULKAN; here it must be linked into the executable too,
		# or the static build has unresolved Ral_* symbols. Plain link (no
		# --whole-archive): unlike the DLL, the static client never force-exports
		# Ral_Dump (its "\ral_dump" path is #ifdef USE_RENDERER_DLOPEN, see
		# cl_main.c CL_RalDump_f), so only the referenced objects need pulling.
		TARGET_LINK_LIBRARIES(${CNAME}${BINEXT} ${RENDERER_PREFIX}_vulkan ral_vulkan)
	ELSE()
		TARGET_LINK_LIBRARIES(${CNAME}${BINEXT} ${RENDERER_PREFIX}_opengl)
	ENDIF()
ENDIF()

# headless server executable

ADD_EXECUTABLE(${DNAME}${BINEXT} ${_DED_EXE_TYPE} ${Q3_SRCS})
TARGET_LINK_LIBRARIES(${DNAME}${BINEXT} qcommon_ded botlib)
# Headless uses the same win_syscon.c (in Q3_SRCS) as client; the
# Sys_ConsoleInput stdin reader path is gated #ifdef HEADLESS so only
# the headless build compiles it.

# WAMR library linking
IF(USE_WASM)
	TARGET_LINK_LIBRARIES(${CNAME}${BINEXT} vmlib)
	TARGET_LINK_LIBRARIES(${DNAME}${BINEXT} vmlib)
ENDIF()

# ── LuaJIT scripting ──────────────────────────────────────────────
# LuaJIT has its own Makefile that generates architecture-specific JIT code.
# Build the static library via custom command, then import and link it.
IF(USE_LUA)
	SET(LUAJIT_DIR ${CMAKE_SOURCE_DIR}/src/libs/luajit)
	SET(LUAJIT_LIB ${LUAJIT_DIR}/src/libluajit.a)

	# Platform-specific build flags
	SET(LUAJIT_XCFLAGS "")
	SET(LUAJIT_MAKE_ENV "")
	IF(APPLE)
		SET(LUAJIT_MAKE_ENV ${CMAKE_COMMAND} -E env MACOSX_DEPLOYMENT_TARGET=10.15)
		IF(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
			SET(LUAJIT_XCFLAGS "-DLUAJIT_ENABLE_GC64")
		ENDIF()
	ENDIF()

	add_custom_command(
		OUTPUT ${LUAJIT_LIB}
		COMMAND ${LUAJIT_MAKE_ENV} make -C ${LUAJIT_DIR}/src clean
		COMMAND ${LUAJIT_MAKE_ENV} make -C ${LUAJIT_DIR}/src BUILDMODE=static "XCFLAGS=${LUAJIT_XCFLAGS}"
		COMMENT "Building LuaJIT (static)"
	)
	add_custom_target(luajit_build DEPENDS ${LUAJIT_LIB})

	add_library(luajit STATIC IMPORTED GLOBAL)
	set_target_properties(luajit PROPERTIES IMPORTED_LOCATION ${LUAJIT_LIB})
	add_dependencies(luajit luajit_build)

	# Headless Lua runtime lives in qcommon/wired/core/scripting/ — used by both client and headless server
	AUX_SOURCE_DIRECTORY(code/qcommon/wired/core/scripting QCOMMON_SCRIPTING_SRCS)
	TARGET_SOURCES(qcommon PRIVATE ${QCOMMON_SCRIPTING_SRCS})
	TARGET_SOURCES(qcommon_ded PRIVATE ${QCOMMON_SCRIPTING_SRCS})

	# LuaJIT headers needed by qcommon (both targets) and client
	TARGET_INCLUDE_DIRECTORIES(qcommon PRIVATE ${LUAJIT_DIR}/src)
	TARGET_INCLUDE_DIRECTORIES(qcommon_ded PRIVATE ${LUAJIT_DIR}/src)
	TARGET_INCLUDE_DIRECTORIES(client PRIVATE ${LUAJIT_DIR}/src)

	# Link LuaJIT to both client and headless server executables
	TARGET_LINK_LIBRARIES(${CNAME}${BINEXT} luajit)
	TARGET_LINK_LIBRARIES(${DNAME}${BINEXT} luajit)

	# wired_scene Lua-loader unit test. Spins up a minimal luaL_newstate (links
	# LuaJIT directly) — no engine FS / WiredScript. wired_scene.c + wired_curve.c
	# are built with -DWIRED_SCENE_STANDALONE / -DWIRED_CURVE_STANDALONE so they
	# route diagnostics to stderr and skip the q_shared / WiredScript chain; the
	# test pushes a scene table itself and calls WiredScene_ReadTable. Asserts:
	# eye-path knot count + round-tripped positions, named targets, the fov table
	# (incl. ['end']), each live event verb decoded (wait/target/fov/fadeout/
	# fadein/feather/stop/scene) + unknown-verb skip, the TCB knot triple + its
	# default, missing-field defaults, and malformed-input (non-table) failure.
	# Defined here (not with the other tests) because it needs the luajit target.
	# Run with: ctest -R wired_scene
	ADD_EXECUTABLE(wired_scene_test
		tests/wired_scene_test.c
		code/qcommon/wired/scene/wired_scene.c
		code/qcommon/wired/math/wired_curve.c
	)
	TARGET_COMPILE_DEFINITIONS(wired_scene_test PRIVATE WIRED_SCENE_STANDALONE WIRED_CURVE_STANDALONE)
	TARGET_INCLUDE_DIRECTORIES(wired_scene_test PRIVATE ${LUAJIT_DIR}/src)
	TARGET_LINK_LIBRARIES(wired_scene_test luajit)
	IF(NOT WIN32)
		TARGET_LINK_LIBRARIES(wired_scene_test m dl pthread)
	ENDIF()
	ADD_TEST(NAME wired_scene COMMAND wired_scene_test)

	MESSAGE(STATUS "LuaJIT scripting enabled")
ENDIF()

#TARGET_LINK_OPTIONS(${DNAME} PRIVATE -fno-pie) # v3.13+

IF(WIN32)
	TARGET_LINK_LIBRARIES(${CNAME}${BINEXT} winmm comctl32 ws2_32)
	TARGET_LINK_LIBRARIES(${DNAME}${BINEXT} winmm comctl32 ws2_32)
	IF(USE_WASM)
		# WAMR's WASI libc (win_file.c, random.c) needs pathcch + bcrypt
		TARGET_LINK_LIBRARIES(${CNAME}${BINEXT} pathcch bcrypt)
		TARGET_LINK_LIBRARIES(${DNAME}${BINEXT} pathcch bcrypt)
	ENDIF()
	IF(NOT MSVC)
		# MinGW: statically link runtime libs (libwinpthread, libgcc, libstdc++)
		# so end users don't need MinGW DLLs in PATH
		TARGET_LINK_OPTIONS(${CNAME}${BINEXT} PRIVATE -static)
		TARGET_LINK_OPTIONS(${DNAME}${BINEXT} PRIVATE -static)
	ENDIF()
ELSE()
	TARGET_LINK_LIBRARIES(${CNAME}${BINEXT} m ${CMAKE_DL_LIBS})
	TARGET_LINK_LIBRARIES(${DNAME}${BINEXT} m ${CMAKE_DL_LIBS})

	# pthread is a hard link dep of BOTH binaries on POSIX: wired/core/thread
	# (Sys_CreateThread/Sys_JoinThread for the background navmesh bake) compiles
	# into qcommon AND qcommon_ded, and the bake runs server-side in both builds.
	# Link Threads::Threads to the headless server too (not just the client) —
	# otherwise wired-headless fails to link on any toolchain that needs -pthread
	# (glibc < 2.34, musl). Kept outside the miniaudio block below, which is
	# client-only.
	FIND_PACKAGE(Threads REQUIRED)
	TARGET_LINK_LIBRARIES(${CNAME}${BINEXT} Threads::Threads)
	TARGET_LINK_LIBRARIES(${DNAME}${BINEXT} Threads::Threads)

	# miniaudio platform link dependencies (client only — headless server
	# never compiles snd_miniaudio.c thanks to #ifndef HEADLESS).
	IF(APPLE)
		# miniaudio uses CoreAudio + AudioToolbox on macOS.
		FIND_LIBRARY(COREAUDIO_LIBRARY CoreAudio)
		FIND_LIBRARY(AUDIOTOOLBOX_LIBRARY AudioToolbox)
		FIND_LIBRARY(COREFOUNDATION_LIBRARY CoreFoundation)
		TARGET_LINK_LIBRARIES(${CNAME}${BINEXT}
			${COREAUDIO_LIBRARY}
			${AUDIOTOOLBOX_LIBRARY}
			${COREFOUNDATION_LIBRARY})
	ENDIF()
	# (Linux miniaudio dlopens ALSA/PulseAudio/JACK at runtime — no extra hard deps.)
ENDIF()

# macOS: ARM64 JIT requires entitlements for MAP_JIT and executable memory
IF(APPLE)
	SET_TARGET_PROPERTIES(${CNAME}${BINEXT} PROPERTIES
		XCODE_ATTRIBUTE_CODE_SIGN_ENTITLEMENTS "${CMAKE_SOURCE_DIR}/misc/macos/wired.entitlements"
	)
	# rpath: find bundled dylibs (SDL3, MoltenVK) in the same dir as the executable
	SET(CMAKE_INSTALL_RPATH "@executable_path")
	SET(CMAKE_BUILD_WITH_INSTALL_RPATH ON)
ENDIF()

# ─────────────────────────────────────────────────────────────────────
# Combined macOS bundle assembly
#
# CMake builds the engine binaries (wired, wired-headless) as plain executables on
# macOS. This target assembles both into a single product bundle named after
# PRODUCT_NAME + CHANNEL_SUFFIX, with the engine binaries co-located under
# Contents/MacOS/. This keeps the engine identity ("wired") on the binary while
# the user-facing product identity ("q3now-preview") owns the bundle.
#
# Layout produced:
#   build/<cfg>/${APP_BUNDLE_NAME}${BINEXT}.app/
#     Contents/
#       Info.plist             (configured from misc/macos/Info.plist.in)
#       PkgInfo                ("APPL????")
#       MacOS/
#         wired${BINEXT}       (client engine binary)
#         wired-headless${BINEXT}   (headless server engine binary)
#       Resources/
#         quake3_flat.icns
# ─────────────────────────────────────────────────────────────────────
IF(APPLE)
	SET(APP_BUNDLE_NAME "${PRODUCT_NAME}${CHANNEL_SUFFIX}")
	SET(APP_BUNDLE_DIR  "${CMAKE_BINARY_DIR}/${APP_BUNDLE_NAME}${BINEXT}.app")

	# Info.plist substitution variables (referenced by misc/macos/Info.plist.in).
	SET(WIRED_BUNDLE_NAME       "${APP_BUNDLE_NAME}")
	SET(WIRED_BUNDLE_EXECUTABLE "${CNAME}${BINEXT}")
	SET(WIRED_BUNDLE_VERSION    "${WIRED_ENGINE_VERSION_RAW}")
	IF(NOT WIRED_BUNDLE_VERSION)
		SET(WIRED_BUNDLE_VERSION "0.80")
	ENDIF()
	configure_file(
		"${CMAKE_SOURCE_DIR}/misc/macos/Info.plist.in"
		"${APP_BUNDLE_DIR}/Contents/Info.plist"
		@ONLY
	)

	add_custom_target(macos_app_bundle ALL
		COMMAND ${CMAKE_COMMAND} -E make_directory "${APP_BUNDLE_DIR}/Contents/MacOS"
		COMMAND ${CMAKE_COMMAND} -E make_directory "${APP_BUNDLE_DIR}/Contents/Resources"
		COMMAND ${CMAKE_COMMAND} -E copy
			"$<TARGET_FILE:${CNAME}${BINEXT}>"
			"${APP_BUNDLE_DIR}/Contents/MacOS/${CNAME}${BINEXT}"
		COMMAND ${CMAKE_COMMAND} -E copy
			"$<TARGET_FILE:${DNAME}${BINEXT}>"
			"${APP_BUNDLE_DIR}/Contents/MacOS/${DNAME}${BINEXT}"
		COMMAND ${CMAKE_COMMAND} -E copy
			"${CMAKE_SOURCE_DIR}/code/unix/quake3_flat.icns"
			"${APP_BUNDLE_DIR}/Contents/Resources/quake3_flat.icns"
		COMMAND ${CMAKE_COMMAND} -E echo_append "APPL????"
			> "${APP_BUNDLE_DIR}/Contents/PkgInfo"
		DEPENDS ${CNAME}${BINEXT} ${DNAME}${BINEXT}
		COMMENT "Assembling macOS bundle: ${APP_BUNDLE_NAME}${BINEXT}.app"
		VERBATIM
	)
ENDIF()

# ============================================================
# q3now game modules (gamecl, gamesv, ui)
# ============================================================

OPTION(BUILD_GAME_LIBRARIES "Build game native shared libraries (.dylib/.so)" ON)

IF(BUILD_GAME_LIBRARIES)
	SET(SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/code)
	SET(BASEGAME "base")
	SET(GAMECL_MODULE "gamecl")
	SET(GAMESV_MODULE "gamesv")
	IF(WIN32)
		SET(COMMON_LIBRARIES)
	ELSE()
		SET(COMMON_LIBRARIES m ${CMAKE_DL_LIBS})
	ENDIF()
	# Q3NOW_GAMEDATE: ioquake3 game code uses this for version stamping
	# Use target_compile_definitions instead of add_compile_definitions to avoid
	# passing the date string (with spaces) to windres on Windows, which breaks.
	STRING(TIMESTAMP Q3NOW_GAMEDATE "%b %d %Y" UTC)
	TARGET_COMPILE_DEFINITIONS(qcommon PRIVATE Q3NOW_GAMEDATE="${Q3NOW_GAMEDATE}")
	TARGET_COMPILE_DEFINITIONS(qcommon_ded PRIVATE Q3NOW_GAMEDATE="${Q3NOW_GAMEDATE}")
	TARGET_COMPILE_DEFINITIONS(client PRIVATE Q3NOW_GAMEDATE="${Q3NOW_GAMEDATE}")
	# Per-build stamp values, computed BEFORE basegame so the game VM targets
	# (gamecl/gamesv native + WASM, stamped in basegame.cmake) see the same id/date
	# as the engine. WIRED_BUILD_ID = Makefile counter (-D; default 0); the DATE is
	# computed here (never passed from the Makefile → no windres spaces trap). The
	# engine + renderer targets are stamped in the dedicated block after this IF.
	# CHANNEL_SUFFIX + PRODUCT_NAME compile definitions are propagated globally
	# from the top-of-file branding block (search for "Product / channel branding").
	INCLUDE(basegame)
ENDIF()

# Per-build stamp (WIRED_BUILD_ID / WIRED_BUILD_DATE for `sysinfo`) is NOT a
# compile definition: the Makefile `_build-stamp` target writes a generated header
# (code/qcommon/wired/wired_build_stamp_gen.h) that wired/wired_build_stamp.h
# includes. That refreshes every build with no cmake reconfigure and reaches the
# WASI wasm builds for free — see the Makefile comment + wired_build_stamp.h.

# ============================================================
# Lint & format integration
# ============================================================
# -DENABLE_CLANG_TIDY=ON       run clang-tidy at compile time using .clang-tidy
# --target format-check        CI: fail if any engine source deviates from .clang-format
# (No bulk in-place format target — clang-format is on-demand per project policy.)

OPTION(ENABLE_CLANG_TIDY "Run clang-tidy during compilation (uses .clang-tidy)" OFF)
IF(ENABLE_CLANG_TIDY)
	FIND_PROGRAM(CLANG_TIDY_EXE NAMES clang-tidy)
	IF(CLANG_TIDY_EXE)
		SET(CMAKE_C_CLANG_TIDY "${CLANG_TIDY_EXE};--config-file=${CMAKE_SOURCE_DIR}/.clang-tidy")
		MESSAGE(STATUS "clang-tidy enabled: ${CLANG_TIDY_EXE}")
	ELSE()
		MESSAGE(WARNING "ENABLE_CLANG_TIDY=ON but clang-tidy not found in PATH")
	ENDIF()
ENDIF()

FIND_PROGRAM(CLANG_FORMAT_EXE NAMES clang-format)
IF(CLANG_FORMAT_EXE)
	ADD_CUSTOM_TARGET(format-check
		COMMAND ${CMAKE_COMMAND}
			-DCLANG_FORMAT=${CLANG_FORMAT_EXE}
			-DSOURCE_DIR=${CMAKE_SOURCE_DIR}
			-P ${CMAKE_SOURCE_DIR}/cmake/format_check.cmake
		WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
		COMMENT "Checking clang-format compliance for engine + game sources"
		VERBATIM
	)
ENDIF()

ADD_SUBDIRECTORY(tools/extract-meta)
