# Floor 3.25: CMakePresets.json schema v6 and add_subdirectory(... SYSTEM)
# both need it, and every supported platform ships well past it (Ubuntu 24.04:
# 3.28, MSYS2/Homebrew: 4.x). The 3.14 floor was already false — the file used
# post-3.14 features. Ceiling 4.4 sets the policy level actually tested.
CMAKE_MINIMUM_REQUIRED(VERSION 3.25...4.4)

PROJECT(wired LANGUAGES C CXX ASM)

IF(APPLE)
	ENABLE_LANGUAGE(OBJCXX)
	SET(CMAKE_OBJCXX_STANDARD 17)
	SET(CMAKE_OBJCXX_STANDARD_REQUIRED ON)
	SET(CMAKE_OBJCXX_EXTENSIONS OFF)
ENDIF()

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

OPTION(USE_OPENGL "" ON)
OPTION(USE_VULKAN "" ON)
OPTION(USE_SYSTEM_JPEG "" OFF)
OPTION(USE_JPEG_TURBO "" ON)
OPTION(USE_ZLIB_NG "" ON)
OPTION(USE_RENDERER_DLOPEN "" ON)
OPTION(USE_WASM "Enable WASM VM backend via WAMR" OFF)
OPTION(USE_LUA "Enable LuaJIT scripting" ON)
OPTION(USE_WIRED_UI_CLAY "Vendor Clay layout engine for the WiredUI compositor (client target only)" ON)
OPTION(USE_SSAO "Compile the GTAO/SSAO renderer path (current product default ON; OFF is an explicit compiled-out permutation)" ON)
OPTION(WIRED_BUILD_IMGUI_TOOLS "Build tool-only Dear ImGui profiling/editor adapters" OFF)

# RAL is a mandatory part of the Vulkan renderer, not an optional feature.
# The historical RAL feature cache/header switch never controlled renderer
# compilation and could therefore advertise a false disabled state. Reject it
# fail-closed; shader translation remains the explicit independent option below.
STRING(CONCAT _retired_ral_feature "FEAT_" "RAL")
IF(DEFINED ${_retired_ral_feature})
	MESSAGE(FATAL_ERROR
		"${_retired_ral_feature} is retired: Vulkan RAL is mandatory. "
		"Use WIRED_BUILD_SHADER_XLATE=ON to build the offline translators.")
ENDIF()
UNSET(_retired_ral_feature)

# cmake/ only — the cmake_modules/ entry pointed at a directory that has never
# existed in this repository, leaving half the module search path dead.
SET(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

AUX_SOURCE_DIRECTORY(code/qcommon QCOMMON_SRCS)
AUX_SOURCE_DIRECTORY(code/qcommon/maps QCOMMON_SRCS)

# Arena allocator (must be explicit — AUX_SOURCE_DIRECTORY is configure-time only)
list(APPEND QCOMMON_SRCS code/qcommon/arena.c)

# VM candidate policy (explicit for already-configured build graphs).
list(APPEND QCOMMON_SRCS code/qcommon/vm_interpret_policy.c)

# stalltrace diagnostic (temporary; cvar-gated, default-off)
list(APPEND QCOMMON_SRCS code/qcommon/wired/stalltrace.c)

# LZ4 decoder (for SW3Z archive reader). These vendor translation units are
# independent of qcommon headers and the HEADLESS compile definition, so build
# them once for both engine variants. qcommon_tool intentionally keeps its own
# curated copy: its bootstrap/link contract remains tool-private.
SET(QCOMMON_VENDOR_SHARED_SRCS
	src/libs/lz4/lz4.c
	src/libs/lz4/lz4frame.c
	src/libs/lz4/xxhash.c
	src/libs/lz4/lz4hc.c
)

# The only qcommon/server translation units whose generated code depends on
# HEADLESS. Everything else is compiled once in qcommon_engine_shared and its
# objects are injected directly into both final executables. Keep this list
# explicit and fail-closed: the graph contract verifies every listed source is
# present in both variants and every other engine source is shared exactly once.
SET(QCOMMON_VARIANT_ALLOWLIST
	code/qcommon/common.c
	code/qcommon/crash.c
	code/qcommon/history.c
	code/qcommon/msg.c
	code/qcommon/net_ip.c
	code/qcommon/vm.c
	code/qcommon/wired/core/console/con_buffer.c
	code/qcommon/wired/core/cvars/cvar.c
	code/qcommon/wired/core/events/event.c
	code/qcommon/wired/core/logging/log.c
	code/qcommon/wired/core/logging/log_sink_console.c
	code/qcommon/wired/core/shell/cmd.c
	code/qcommon/wired/core/vfs/files.c
	code/qcommon/wired/net/wn_bg_stubs.c
	code/qcommon/wired/net/wn_inmem.c
	code/qcommon/wired/net/wn_main.c
	code/qcommon/wired/net/wn_transport.c
	code/server/sv_client.c
	code/server/sv_init.c
	code/server/sv_main.c
)

FUNCTION(WIRED_PARTITION_QCOMMON_SOURCES shared_out variant_out)
	SET(_shared)
	SET(_variant)
	FOREACH(_src IN LISTS ARGN)
		IF(IS_ABSOLUTE "${_src}")
			FILE(RELATIVE_PATH _key "${CMAKE_SOURCE_DIR}" "${_src}")
		ELSE()
			SET(_key "${_src}")
		ENDIF()
		LIST(FIND QCOMMON_VARIANT_ALLOWLIST "${_key}" _variant_index)
		IF(_variant_index GREATER -1)
			LIST(APPEND _variant "${_src}")
		ELSE()
			LIST(APPEND _shared "${_src}")
		ENDIF()
	ENDFOREACH()
	LIST(REMOVE_DUPLICATES _shared)
	LIST(REMOVE_DUPLICATES _variant)
	SET(${shared_out} "${_shared}" PARENT_SCOPE)
	SET(${variant_out} "${_variant}" PARENT_SCOPE)
ENDFUNCTION()

FUNCTION(WIRED_ROUTE_QCOMMON_SOURCES)
	WIRED_PARTITION_QCOMMON_SOURCES(_shared _variant ${ARGN})
	IF(_shared)
		TARGET_SOURCES(qcommon_engine_shared PRIVATE ${_shared})
		SET_PROPERTY(GLOBAL APPEND PROPERTY QCOMMON_SHARED_SOURCE_INVENTORY ${_shared})
	ENDIF()
	IF(_variant)
		TARGET_SOURCES(qcommon PRIVATE ${_variant})
		TARGET_SOURCES(qcommon_ded PRIVATE ${_variant})
		SET_PROPERTY(GLOBAL APPEND PROPERTY QCOMMON_VARIANT_SOURCE_INVENTORY ${_variant})
	ENDIF()
ENDFUNCTION()

# When WASM is disabled, override FEAT_WASM=0 so vm_wasm.c compiles to empty
# and vm.c skips WASM code paths (q_feats.h hardcodes FEAT_WASM=1).
IF(NOT USE_WASM)
	ADD_COMPILE_DEFINITIONS(FEAT_WASM=0)
ENDIF()

# CMake is the build-policy authority for GTAO.  Define the macro in both
# branches so USE_SSAO=OFF is a real compiled-out permutation rather than being
# silently overridden by q_feats.h's product default.  Default ON preserves the
# current assembled product behavior.
IF(USE_SSAO)
	ADD_COMPILE_DEFINITIONS(FEAT_SSAO=1)
	SET(WIRED_SSAO_COMPILE_VALUE 1)
ELSE()
	ADD_COMPILE_DEFINITIONS(FEAT_SSAO=0)
	SET(WIRED_SSAO_COMPILE_VALUE 0)
ENDIF()

# MinGW: picoquic + picotls use bare `#ifdef _WINDOWS` (MSVC-only auto-define)
# to gate winsock includes. Inject _WINDOWS so they take the Windows code
# path; vendored sources stay untouched. Definition is innocuous for wired's
# own code (wired uses _WIN32 throughout). Applied globally before any target
# is created so every TU that transitively includes picoquic.h sees it.
# See docs/health.md "picoquic _WINDOWS guards do not match MinGW".
IF(WIN32)
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
ELSEIF(CMAKE_SYSTEM_PROCESSOR MATCHES riscv64)
	SET(BINEXT .riscv64)
	SET(RENDEXT _riscv64)
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

# Partition the initial qcommon + server inventory before target creation.
WIRED_PARTITION_QCOMMON_SOURCES(
	_QCOMMON_INITIAL_SHARED
	_QCOMMON_INITIAL_VARIANT
	${QCOMMON_SRCS}
	${SERVER_SRCS}
)

# universal binary
ADD_LIBRARY(qcommon_engine_shared OBJECT
	${QCOMMON_VENDOR_SHARED_SRCS}
	${_QCOMMON_INITIAL_SHARED}
)
TARGET_INCLUDE_DIRECTORIES(qcommon_engine_shared PRIVATE src/libs/lz4)
SET_PROPERTY(GLOBAL APPEND PROPERTY QCOMMON_SHARED_SOURCE_INVENTORY
	${QCOMMON_VENDOR_SHARED_SRCS}
	${_QCOMMON_INITIAL_SHARED}
)
SET_PROPERTY(GLOBAL APPEND PROPERTY QCOMMON_VARIANT_SOURCE_INVENTORY
	${_QCOMMON_INITIAL_VARIANT}
)

ADD_LIBRARY(qcommon OBJECT ${_QCOMMON_INITIAL_VARIANT})

# headless server
ADD_LIBRARY(qcommon_ded OBJECT ${_QCOMMON_INITIAL_VARIANT})
TARGET_COMPILE_DEFINITIONS(qcommon_ded PUBLIC HEADLESS)

# LZ4 include path (for SW3Z archive reader)
TARGET_INCLUDE_DIRECTORIES(qcommon PRIVATE src/libs/lz4)
TARGET_INCLUDE_DIRECTORIES(qcommon_ded PRIVATE src/libs/lz4)

# code/qcommon as explicit include root so wired/* sub-components can resolve
# sibling headers (qcommon.h, q_shared.h, unzip.h, etc.) without relative paths.
TARGET_INCLUDE_DIRECTORIES(qcommon     PRIVATE code/qcommon)
TARGET_INCLUDE_DIRECTORIES(qcommon_ded PRIVATE code/qcommon)
TARGET_INCLUDE_DIRECTORIES(qcommon_engine_shared PRIVATE code/qcommon)

# wired/core/vfs — virtual filesystem
FILE(GLOB WIRED_VFS_SRCS  code/qcommon/wired/core/vfs/*.c)
WIRED_ROUTE_QCOMMON_SOURCES(${WIRED_VFS_SRCS})

# wired/core/cvars — cvar sub-component
FILE(GLOB WIRED_CVAR_SRCS code/qcommon/wired/core/cvars/*.c)
WIRED_ROUTE_QCOMMON_SOURCES(${WIRED_CVAR_SRCS})

# wired/core/shell — keys + cmd
FILE(GLOB WIRED_CON_SRCS  code/qcommon/wired/core/shell/*.c)
WIRED_ROUTE_QCOMMON_SOURCES(${WIRED_CON_SRCS})

# wired/core/console — presentation-agnostic ring buffer + history +
# scrollback + search/mark + severity-as-data + the param-injected geometry
# reflow (Con_Reflow). Turn 3 V-20 extracted all UI-tier touches (glconfig,
# render metrics, color vecs, Key_*/Field_*/cgvm, the toggleconsole/messagemode
# commands, and Con_CheckResize's glconfig half) into the projection at
# code/client/wired/ui/elements/console.c. con_buffer.c now includes ZERO
# client/* headers, so it joins the dedicated-server (qcommon_ded) target too.
FILE(GLOB WIRED_CONSOLE_SRCS code/qcommon/wired/core/console/*.c)
WIRED_ROUTE_QCOMMON_SOURCES(${WIRED_CONSOLE_SRCS})

# wired/core/logging — log pipeline (console/tty/file sinks, ring buffer)
FILE(GLOB WIRED_LOG_SRCS  code/qcommon/wired/core/logging/*.c)
WIRED_ROUTE_QCOMMON_SOURCES(${WIRED_LOG_SRCS})

# wired/core/events — event primitive stubs (V2 bodies TBD)
FILE(GLOB WIRED_EVENT_SRCS code/qcommon/wired/core/events/*.c)
WIRED_ROUTE_QCOMMON_SOURCES(${WIRED_EVENT_SRCS})

# wired/core/time — time subsystem (Sys_NanoTime, Sys_Milliseconds, Com_RealTime, Com_FormatTimestamp)
FILE(GLOB WIRED_TIME_SRCS  code/qcommon/wired/core/time/*.c)
WIRED_ROUTE_QCOMMON_SOURCES(${WIRED_TIME_SRCS})

# wired/core/thread — portable thread + atomic primitives (Sys_CreateThread, ...), links
# into both client and headless (unlike SDL threads) for the background navmesh bake
FILE(GLOB WIRED_THREAD_SRCS  code/qcommon/wired/core/thread/*.c)
WIRED_ROUTE_QCOMMON_SOURCES(${WIRED_THREAD_SRCS})

# wired/core — orchestration layer (WiredCore_Init / WiredCore_Shutdown)
FILE(GLOB WIRED_CORE_SRCS code/qcommon/wired/core/*.c)
WIRED_ROUTE_QCOMMON_SOURCES(${WIRED_CORE_SRCS})

# wired/math — pure-CPU math (spline curve core; NOT renderer, NOT RAL).
# Cinematic-scene spline core: RBDOOM Curve.h port (Catmull-Rom + TCB +
# arc-length reparameterization). No Vulkan/RAL dependency — only q_shared.h
# vec macros.
FILE(GLOB WIRED_MATH_SRCS code/qcommon/wired/math/*.c)
WIRED_ROUTE_QCOMMON_SOURCES(${WIRED_MATH_SRCS})

# wired_curve math-core unit test (cinematic-scene spline core gate).
# Production configuration: wired_curve.c consumes q_shared.h exactly as the
# engine targets do; the standalone vec-macro fallback is not enabled here.
# Asserts: interpolation-through-control-points, straight-line constant
# tangent, TCB(0,0,0)==Catmull-Rom, SetConstantSpeed linearity, GetTimeForLength
# round-trip, Newton div-by-zero guard.
# Run with: ctest -R wired_curve
INCLUDE(CTest)
OPTION(WIRED_HOST_SANITIZERS
	"Build host-only subsystem tests with AddressSanitizer and UndefinedBehaviorSanitizer"
	OFF)
SET(WIRED_IMGUI_DIR "${CMAKE_SOURCE_DIR}/src/libs/recastnavigation/RecastDemo/Contrib/imgui")
IF(WIRED_BUILD_IMGUI_TOOLS OR BUILD_TESTING)
	IF(NOT TARGET SDL3::SDL3)
		FIND_PACKAGE(SDL3 REQUIRED)
	ENDIF()
	FOREACH(_imgui_required imgui.h imgui.cpp imgui_draw.cpp imgui_tables.cpp imgui_widgets.cpp)
		IF(NOT EXISTS "${WIRED_IMGUI_DIR}/${_imgui_required}")
			MESSAGE(FATAL_ERROR
				"WIRED_BUILD_IMGUI_TOOLS requires the vendored Recast Dear ImGui source: ${_imgui_required}")
		ENDIF()
	ENDFOREACH()
	ADD_LIBRARY(wired_profile_imgui STATIC
		code/tools/profile_imgui/wired_profile_imgui.cpp
		code/tools/profile_imgui/wired_profile_imgui_ral.cpp
		code/tools/profile_imgui/wired_profile_imgui_sdl3.cpp
		${WIRED_IMGUI_DIR}/imgui.cpp
		${WIRED_IMGUI_DIR}/imgui_draw.cpp
		${WIRED_IMGUI_DIR}/imgui_tables.cpp
		${WIRED_IMGUI_DIR}/imgui_widgets.cpp
	)
	TARGET_INCLUDE_DIRECTORIES(wired_profile_imgui PUBLIC
		code/tools/profile_imgui
		code/renderer/ral
		${WIRED_IMGUI_DIR}
	)
	TARGET_LINK_LIBRARIES(wired_profile_imgui PUBLIC SDL3::SDL3)
	IF(WIRED_HOST_SANITIZERS)
		IF(NOT CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
			MESSAGE(FATAL_ERROR
				"WIRED_HOST_SANITIZERS requires GCC or Clang; C++ compiler is ${CMAKE_CXX_COMPILER_ID}")
		ENDIF()
		TARGET_COMPILE_OPTIONS(wired_profile_imgui PRIVATE
			-fsanitize=address,undefined
			-fno-omit-frame-pointer
			-fno-sanitize-recover=all)
	ENDIF()
ENDIF()
IF(BUILD_TESTING)
ADD_CUSTOM_TARGET(host_tests)

# ── GoogleTest ──────────────────────────────────────────────────────────────
# Vendored as a pinned submodule (src/libs/googletest, v1.17.0), consumed the
# same way as every other third-party tree here: ADD_SUBDIRECTORY + EXCLUDE_FROM_ALL,
# no FetchContent. FetchContent appears nowhere in this repo and would make
# `cmake` reach the network at configure time, so an offline or air-gapped
# configure would start failing — the vendoring is deliberate, not incidental.
#
# It lives INSIDE IF(BUILD_TESTING): with testing off the subdirectory is never
# added, so gtest is not built, not linked, and cannot reach a shipping target.
#
# The BUILD_TESTING save/restore mirrors the zlib-ng block below (see its comment
# for the full rationale): googletest reads BUILD_TESTING for its OWN test suite,
# and ours is ON, so leaving it alone would build gtest's internal tests as part
# of our tree.
IF(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/src/libs/googletest/CMakeLists.txt)
	SET(_WIRED_SAVED_BUILD_TESTING ${BUILD_TESTING})
	SET(BUILD_TESTING OFF)
	SET(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
	# Windows: match the CRT the rest of the tree uses instead of gtest's default
	# static CRT, otherwise test targets fail to link with mismatched runtimes.
	SET(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
	ADD_SUBDIRECTORY(src/libs/googletest EXCLUDE_FROM_ALL)
	SET(BUILD_TESTING ${_WIRED_SAVED_BUILD_TESTING})
	UNSET(_WIRED_SAVED_BUILD_TESTING)
	INCLUDE(GoogleTest)
	SET(WIRED_HAVE_GTEST ON)
ELSE()
	SET(WIRED_HAVE_GTEST OFF)
	MESSAGE(STATUS "googletest submodule absent — gtest suites skipped "
		"(git submodule update --init src/libs/googletest)")
ENDIF()

# Build-graph contract for the first qcommon dedup cohort. The engine variants
# must consume one shared vendor-material archive while extract-meta retains
# its curated, tool-private LZ4 compilation. Reading compile_commands makes an
# accidental source re-addition to qcommon/qcommon_ded a test failure rather
# than a silent cost regression.
ADD_TEST(NAME qcommon_shared_graph_contract
	COMMAND ${CMAKE_COMMAND}
		-DCOMPILE_COMMANDS=${CMAKE_BINARY_DIR}/compile_commands.json
		-DSHARED_MANIFEST=${CMAKE_BINARY_DIR}/qcommon_engine_shared_sources.txt
		-DVARIANT_MANIFEST=${CMAKE_BINARY_DIR}/qcommon_engine_variant_sources.txt
		-DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
		-P ${CMAKE_SOURCE_DIR}/cmake/qcommon_shared_graph_check.cmake
)
SET_TESTS_PROPERTIES(qcommon_shared_graph_contract PROPERTIES
	LABELS "host-only;infrastructure;owner.build;failure.build-graph"
	TIMEOUT 90
)

# Release/package provenance is owned by the production Makefile.  Exercise its
# three platform recipe branches without building: recursive bundle makes must
# retain the outer source/date/version tuple, final summaries must name an exact
# fail-closed artifact, and macOS staging must reject runtime-generated files.
ADD_TEST(NAME release_provenance_contract
	COMMAND bash ${CMAKE_SOURCE_DIR}/tests/release-provenance-check.sh
		--self-test ${CMAKE_SOURCE_DIR}
)
SET_TESTS_PROPERTIES(release_provenance_contract PROPERTIES
	LABELS "host-only;infrastructure;owner.build;failure.package-provenance"
	TIMEOUT 30
)

# HiDPI (#4) analyzer contract.  The live wiredui-layout-checks gate needs a real
# window to CAPTURE a layout dump, but the DPI DECISION it makes is a pure
# function of the dump: dpiScale must equal vidWidthPx/logicalWidth.  This runs
# the same analyzer over synthetic dumps, so the analyzer half is covered with no
# display, no GPU and no pak set — including the Retina dpiScale=4 case that made
# the live gate reject a CORRECT engine before d00a6afd.  The capture half stays
# owner-gated; this pins the half that actually decides pass/fail.
ADD_TEST(NAME wiredui_dpi_analyzer_contract
	COMMAND bash ${CMAKE_SOURCE_DIR}/tests/wiredui-layout-checks.sh
		--dpi-self-test
)
SET_TESTS_PROPERTIES(wiredui_dpi_analyzer_contract PROPERTIES
	LABELS "host-only;infrastructure;owner.q3now-client;failure.contract;feature.always"
	TIMEOUT 60
)

# rem root-scale (#8) analyzer contract.  Same split as the DPI contract above:
# capturing a dump needs a window, but deciding whether the root scale is sound
# is a pure function of the dump.  The fixture that earns this test is
# expectation-missed — a dump whose rootScale does not match what the caller
# set, i.e. ui_rootSize never reached the resolvers.  That is the failure mode a
# root scale has and a DPI ratio does not: nothing else on screen changes to
# give it away, so a silently-ignored setting would otherwise read as a pass.
ADD_TEST(NAME wiredui_rem_analyzer_contract
	COMMAND bash ${CMAKE_SOURCE_DIR}/tests/wiredui-layout-checks.sh
		--rem-self-test
)
SET_TESTS_PROPERTIES(wiredui_rem_analyzer_contract PROPERTIES
	LABELS "host-only;infrastructure;owner.q3now-client;failure.contract;feature.always"
	TIMEOUT 60
)

# Second-map Z_Free (#96) analyzer contract.  The live gate needs a BSP-bearing
# content pak (licensed asset pipeline output), but the crash CLASSIFIER is a
# pure function of the log: it must reject every zone-consistency terminate
# (common.c), the "Server crashed" funnel (log.c:878), a FATAL record, a
# truncated map chain and a non-zero exit.  This runs that classifier over
# synthetic logs — no engine, no packs, no display — so a gate that could not
# catch #96 fails the build.
# Test scripts must parse under macOS's system bash 3.2, which the macOS CI
# runner uses. A developer machine with Homebrew has bash 5 and will not notice
# the difference until CI does — and the error bash 3.2 reports points at the
# end of the file rather than the cause. See the script for the full story.
ADD_TEST(NAME shell_portability_contract
	COMMAND bash ${CMAKE_SOURCE_DIR}/tests/shell-portability-check.sh
)
SET_TESTS_PROPERTIES(shell_portability_contract PROPERTIES
	LABELS "host-only;infrastructure;owner.q3now-server;failure.contract;feature.always"
	TIMEOUT 60
)

# The analyzer half of the independent-tool marker receipt. The live capture
# needs a RenderDoc host (Windows/Linux — RenderDoc has no macOS build), but the
# parsing contract is platform-free, so it runs everywhere and keeps the readout
# schema honest even on machines that can never take the capture.
ADD_TEST(NAME ral_profile_renderdoc_analyzer_contract
	COMMAND bash ${CMAKE_SOURCE_DIR}/tests/ral-profile-renderdoc-check.sh --self-test
)
SET_TESTS_PROPERTIES(ral_profile_renderdoc_analyzer_contract PROPERTIES
	LABELS "host-only;subsystem;owner.ral;owner.tools;failure.contract;feature.always"
	TIMEOUT 120
)

ADD_TEST(NAME headless_map_transition_analyzer_contract
	COMMAND bash ${CMAKE_SOURCE_DIR}/tests/headless-map-transition-zonecheck.sh
		--self-test
)
SET_TESTS_PROPERTIES(headless_map_transition_analyzer_contract PROPERTIES
	LABELS "host-only;infrastructure;owner.q3now-server;failure.contract;feature.always"
	TIMEOUT 60
)

# wired_playtest.jsonl consumer contract (TASK-120 #3). The map-race harness is
# the artefact's first real consumer, and what it must do is decide whether a
# session can be reconstructed from the evidence file alone: v1 envelope on
# every record, intact seq/time order, one session, each map bracketed
# load->loaded, and drop accounting that agrees with what the file actually
# carries. Like the analyzer above this is a pure function of the artefact, so
# it runs over synthetic files with no engine, no packs and no display — an
# evidence gate that could not reject corrupt evidence would certify every
# artefact, including the ones that quietly lost records.
ADD_TEST(NAME playtest_evidence_analyzer_contract
	COMMAND bash ${CMAKE_SOURCE_DIR}/tests/headless-map-transition-zonecheck.sh
		--playtest-self-test
)
SET_TESTS_PROPERTIES(playtest_evidence_analyzer_contract PROPERTIES
	LABELS "host-only;infrastructure;owner.q3now-server;failure.contract;feature.always"
	TIMEOUT 60
)

# The same discipline for crash evidence: this runs the validator's self-test,
# not a real crash. It builds artefacts with one planted defect each — missing
# build identity, an absolute path in the cvar block, a truncated minidump — and
# requires every one to be refused. A validator that cannot reject is a validator
# that certifies corrupt evidence, and privacy defects in particular are silent:
# a leaked home directory reads as a perfectly valid report.
ADD_TEST(NAME crash_evidence_analyzer_contract
	COMMAND bash ${CMAKE_SOURCE_DIR}/tests/crash-evidence-check.sh --self-test
)
SET_TESTS_PROPERTIES(crash_evidence_analyzer_contract PROPERTIES
	LABELS "host-only;infrastructure;owner.build;failure.security-contract;feature.always"
	TIMEOUT 60
)

# Screenshots come from vk.capture.image when supersampling is on and from the
# swapchain otherwise; only the former is display-independent, so on a HiDPI
# display the two disagree about size.  The gate also refuses a blank frame —
# during bring-up every macOS capture came back correctly sized and completely
# black, and the size alone was briefly mistaken for success.  Self-test only:
# analyzer over synthetic PNGs, no engine and no display.
ADD_TEST(NAME visual_supersample_parity_contract
	COMMAND bash ${CMAKE_SOURCE_DIR}/tests/visual-supersample-parity.sh
		--self-test
)
SET_TESTS_PROPERTIES(visual_supersample_parity_contract PROPERTIES
	LABELS "host-only;infrastructure;owner.q3now-client;failure.contract;feature.always"
	TIMEOUT 60
)

ADD_TEST(NAME ral_feature_policy_contract
	COMMAND ${CMAKE_COMMAND}
		-DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
		-P ${CMAKE_SOURCE_DIR}/cmake/ral_feature_policy_check.cmake
)
SET_TESTS_PROPERTIES(ral_feature_policy_contract PROPERTIES
	LABELS "host-only;infrastructure;owner.ral;failure.contract"
	TIMEOUT 30
)

# W-103: every DEFAULT resolution is 16:9. A 4:3 capture does not match the
# player's framing, so HUD/FOV evidence read off one is false evidence. Scoped
# to cvar defaults, the SDL headless fallback, and harness extent expectations
# -- NOT the virtual 640x480 UI space or the user-selectable mode list.
ADD_TEST(NAME widescreen_default_policy_contract
	COMMAND ${CMAKE_COMMAND}
		-DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
		-P ${CMAKE_SOURCE_DIR}/cmake/widescreen_default_policy_check.cmake
)
SET_TESTS_PROPERTIES(widescreen_default_policy_contract PROPERTIES
	LABELS "host-only;infrastructure;owner.renderer;failure.contract"
	TIMEOUT 30
)

ADD_TEST(NAME ssao_feature_policy_contract
	COMMAND ${CMAKE_COMMAND}
		-DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
		-DCOMPILE_COMMANDS=${CMAKE_BINARY_DIR}/compile_commands.json
		-DEXPECTED_SSAO=${WIRED_SSAO_COMPILE_VALUE}
		-P ${CMAKE_SOURCE_DIR}/cmake/ssao_feature_policy_check.cmake
)
SET_TESTS_PROPERTIES(ssao_feature_policy_contract PROPERTIES
	LABELS "host-only;infrastructure;owner.ral;failure.contract;feature.ssao"
	TIMEOUT 30
)

ADD_TEST(NAME ral_retired_feature_rejected
	COMMAND ${CMAKE_COMMAND}
		-DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
		-DPROBE_ROOT=${CMAKE_BINARY_DIR}/ral-retired-feature-probe
		-P ${CMAKE_SOURCE_DIR}/cmake/ral_retired_feature_reject_check.cmake
)
SET_TESTS_PROPERTIES(ral_retired_feature_rejected PROPERTIES
	LABELS "host-only;infrastructure;owner.ral;failure.configuration"
	# 300s, not 30: this check EXECUTE_PROCESSes a full cmake configure of the
	# tree (compiler probing included) before the retired-feature gate can
	# reject — the Windows CI runner needs well over 30s just for the probes.
	TIMEOUT 300
)

ADD_TEST(NAME ral_public_surface_contract
	COMMAND ${CMAKE_COMMAND}
		-DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
		-P ${CMAKE_SOURCE_DIR}/cmake/ral_public_surface_check.cmake
)
SET_TESTS_PROPERTIES(ral_public_surface_contract PROPERTIES
	LABELS "host-only;infrastructure;owner.ral;failure.contract"
	TIMEOUT 30
)

ADD_TEST(NAME ral_public_surface_reintroduction_rejected
	COMMAND ${CMAKE_COMMAND}
		-DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
		-DPROBE_ROOT=${CMAKE_BINARY_DIR}/ral-public-surface-probe
		-P ${CMAKE_SOURCE_DIR}/cmake/ral_public_surface_reject_check.cmake
)
SET_TESTS_PROPERTIES(ral_public_surface_reintroduction_rejected PROPERTIES
	LABELS "host-only;infrastructure;owner.ral;failure.contract"
	TIMEOUT 30
)

ADD_TEST(NAME ral_host_symbol_policy_contract
	COMMAND ${CMAKE_COMMAND}
		-DARCHIVE=$<TARGET_FILE:ral_vulkan>
		-DNM_TOOL=${CMAKE_NM}
		-P ${CMAKE_SOURCE_DIR}/cmake/ral_host_symbol_policy_check.cmake
)
SET_TESTS_PROPERTIES(ral_host_symbol_policy_contract PROPERTIES
	LABELS "host-only;infrastructure;owner.ral;failure.link-policy"
	TIMEOUT 30
)

FUNCTION(WIRED_CONFIGURE_HOST_TEST target test_name)
	ADD_DEPENDENCIES(host_tests ${target})
	IF(WIRED_HOST_SANITIZERS)
		IF(NOT CMAKE_C_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
			MESSAGE(FATAL_ERROR
				"WIRED_HOST_SANITIZERS requires GCC or Clang; compiler is ${CMAKE_C_COMPILER_ID}")
		ENDIF()
		TARGET_COMPILE_OPTIONS(${target} PRIVATE
			-fsanitize=address,undefined
			-fno-omit-frame-pointer
			-fno-sanitize-recover=all)
		TARGET_LINK_OPTIONS(${target} PRIVATE
			-fsanitize=address,undefined
			-fno-omit-frame-pointer
			-fno-sanitize-recover=all)
		SET_PROPERTY(TEST ${test_name} APPEND PROPERTY LABELS "sanitizer.asan-ubsan")
		SET_PROPERTY(TEST ${test_name} PROPERTY ENVIRONMENT
			"ASAN_OPTIONS=abort_on_error=1:detect_leaks=0;UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1")
	ENDIF()
ENDFUNCTION()

# GoogleTest counterpart of WIRED_CONFIGURE_HOST_TEST.
#
#   WIRED_ADD_GTEST(<target> <ctest_prefix> <labels> SOURCES <src>...)
#
# The two cannot share one function because gtest_discover_tests registers its
# CTest entries at BUILD time — one per TEST() macro, named by the framework —
# whereas WIRED_CONFIGURE_HOST_TEST writes properties onto a single name known at
# configure time. So the work is split: per-test properties (LABELS, TIMEOUT) go
# through gtest_discover_tests' own PROPERTIES argument, which applies them to
# every discovered case, while the target-level half (host_tests dependency,
# sanitizer compile/link flags) is done here directly.
#
# The target name must end in _test: the Windows CI job does not run ctest at all,
# it globs *_test.exe and checks exit codes (.github/workflows/build.yml). A gtest
# binary satisfies that — it returns non-zero on failure — but only if it is named
# so the glob finds it.
# WIRED_LINK_MATH(<target>...) — link libm where it is a separate library.
#
# On macOS and Windows the math functions live in the C runtime, so a target
# calling floorf/sinf/acosf links without asking. On Linux they are in libm and
# it does not. The result is a whole class of failure that is INVISIBLE on the
# machine most of this is developed on and only appears in CI, one target at a
# time: fix six, push, discover a seventh.
#
# This exists so the answer is written once. Adding a target that does maths
# means adding it here, not rediscovering the rule from a linker error.
FUNCTION(WIRED_LINK_MATH)
	IF(WIN32)
		RETURN()
	ENDIF()
	FOREACH(_t IN LISTS ARGN)
		IF(TARGET ${_t})
			TARGET_LINK_LIBRARIES(${_t} m)
		ENDIF()
	ENDFOREACH()
ENDFUNCTION()

FUNCTION(WIRED_ADD_GTEST target ctest_prefix labels)
	CMAKE_PARSE_ARGUMENTS(ARG "" "" "SOURCES" ${ARGN})
	IF(NOT WIRED_HAVE_GTEST)
		RETURN()
	ENDIF()

	ADD_EXECUTABLE(${target} ${ARG_SOURCES})
	TARGET_INCLUDE_DIRECTORIES(${target} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/code/qcommon)
	# gtest_main supplies main(); GTest::gtest is linked to THIS TARGET ONLY, never
	# to an engine or shipping target — that is TASK-122 #8's actual requirement.
	TARGET_LINK_LIBRARIES(${target} PRIVATE GTest::gtest GTest::gtest_main)
	TARGET_COMPILE_FEATURES(${target} PRIVATE cxx_std_17)

	# gtest_discover_tests RUNS the binary at build time (--gtest_list_tests) to
	# enumerate its cases. That is fine when host and target match and impossible
	# when they do not: the llvm-mingw cross build produces Windows .exe files
	# and the Linux builder tries to execute them, which fails as
	# "Syntax error: word unexpected" — the shell attempting to interpret a PE
	# binary. Registering the tests would be pointless there anyway, since ctest
	# on that host could not run them either.
	#
	# The target still BUILDS under cross-compilation, which is the part worth
	# keeping: a gtest target that stops compiling for Windows should fail the
	# cross build.
	IF(CMAKE_CROSSCOMPILING)
		MESSAGE(STATUS "${target}: built but not registered (cross-compiling; "
			"the host cannot execute target binaries)")
	ELSE()
		gtest_discover_tests(${target}
			TEST_PREFIX "${ctest_prefix}."
			PROPERTIES
				LABELS "${labels}"
				TIMEOUT 30
			DISCOVERY_TIMEOUT 30)
	ENDIF()

	ADD_DEPENDENCIES(host_tests ${target})
	IF(WIRED_HOST_SANITIZERS)
		IF(NOT CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
			MESSAGE(FATAL_ERROR
				"WIRED_HOST_SANITIZERS requires GCC or Clang; compiler is ${CMAKE_CXX_COMPILER_ID}")
		ENDIF()
		TARGET_COMPILE_OPTIONS(${target} PRIVATE
			-fsanitize=address,undefined
			-fno-omit-frame-pointer
			-fno-sanitize-recover=all)
		TARGET_LINK_OPTIONS(${target} PRIVATE
			-fsanitize=address,undefined
			-fno-omit-frame-pointer
			-fno-sanitize-recover=all)
	ENDIF()
ENDFUNCTION()

IF(WIRED_HOST_SANITIZERS)
	ADD_EXECUTABLE(sanitizer_fault_probe tests/sanitizer_fault_probe.c)
	ADD_TEST(NAME sanitizer_runtime_contract
		COMMAND bash ${CMAKE_SOURCE_DIR}/tests/sanitizer-runtime-check.sh
			$<TARGET_FILE:sanitizer_fault_probe>)
	SET_TESTS_PROPERTIES(sanitizer_runtime_contract PROPERTIES
		LABELS "host-only;infrastructure;owner.build;failure.sanitizer-runtime"
		TIMEOUT 30)
	WIRED_CONFIGURE_HOST_TEST(sanitizer_fault_probe sanitizer_runtime_contract)
ENDIF()

ADD_EXECUTABLE(ral_vulkan_translate_test
	tests/ral_vulkan_translate_test.c
	code/renderer/ral/ral_transition.c
	code/renderer/ral_vulkan/ral_vulkan_translate.c
)
TARGET_INCLUDE_DIRECTORIES(ral_vulkan_translate_test PRIVATE
	code/qcommon
	code/renderer/ral
	code/renderer/ral_vulkan
	code/renderercommon
)
ADD_TEST(NAME ral_vulkan_translate_contract COMMAND ral_vulkan_translate_test)
SET_TESTS_PROPERTIES(ral_vulkan_translate_contract PROPERTIES
	LABELS "host-only;subsystem;owner.ral;failure.contract;feature.always"
	TIMEOUT 30
)
WIRED_CONFIGURE_HOST_TEST(ral_vulkan_translate_test ral_vulkan_translate_contract)
ADD_TEST(NAME ral_transition_source_policy_contract
	COMMAND ${CMAKE_COMMAND} -DROOT=${CMAKE_CURRENT_SOURCE_DIR}
		-P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/ral_transition_policy_check.cmake)
SET_TESTS_PROPERTIES(ral_transition_source_policy_contract PROPERTIES
	LABELS "host-only;subsystem;owner.ral;failure.contract;feature.always"
	TIMEOUT 30
)

ADD_EXECUTABLE(ral_transition_command_test
	tests/ral_transition_command_test.c
	code/renderer/ral/ral_buffer_map.c
	code/renderer/ral/ral_transition.c
	code/renderer/ral_vulkan/ral_vulkan_translate.c
	code/renderer/ral_vulkan/ral_vulkan_transition.c
)
TARGET_INCLUDE_DIRECTORIES(ral_transition_command_test PRIVATE
	code/qcommon
	code/renderer/ral
	code/renderer/ral_vulkan
	code/renderercommon
)
ADD_TEST(NAME ral_transition_command_contract COMMAND ral_transition_command_test)
SET_TESTS_PROPERTIES(ral_transition_command_contract PROPERTIES
	LABELS "host-only;subsystem;owner.ral;failure.contract;feature.always"
	TIMEOUT 30
)
WIRED_CONFIGURE_HOST_TEST(ral_transition_command_test ral_transition_command_contract)

ADD_EXECUTABLE(ral_command_lifecycle_test
	tests/ral_command_lifecycle_test.c
	code/renderer/ral/ral_command_lifecycle.c
)
TARGET_INCLUDE_DIRECTORIES(ral_command_lifecycle_test PRIVATE
	code/qcommon
	code/renderer/ral
	code/renderercommon
)
ADD_TEST(NAME ral_command_lifecycle_contract COMMAND ral_command_lifecycle_test)
SET_TESTS_PROPERTIES(ral_command_lifecycle_contract PROPERTIES
	LABELS "host-only;subsystem;owner.ral;failure.contract;feature.always"
	TIMEOUT 30
)
WIRED_CONFIGURE_HOST_TEST(ral_command_lifecycle_test ral_command_lifecycle_contract)
ADD_EXECUTABLE(ral_webgpu_command_lifecycle_test
	tests/ral_webgpu_command_lifecycle_test.c
	code/renderer/ral/ral_command_lifecycle.c
)
TARGET_INCLUDE_DIRECTORIES(ral_webgpu_command_lifecycle_test PRIVATE
	code/qcommon
	code/renderer/ral
	code/renderercommon
)
ADD_TEST(NAME ral_webgpu_command_lifecycle_contract COMMAND ral_webgpu_command_lifecycle_test)
SET_TESTS_PROPERTIES(ral_webgpu_command_lifecycle_contract PROPERTIES
	LABELS "host-only;subsystem;owner.ral;failure.contract;feature.always"
	TIMEOUT 30
)
WIRED_CONFIGURE_HOST_TEST(ral_webgpu_command_lifecycle_test ral_webgpu_command_lifecycle_contract)
ADD_TEST(NAME ral_command_lifecycle_source_policy_contract
	COMMAND ${CMAKE_COMMAND} -DROOT=${CMAKE_CURRENT_SOURCE_DIR}
		-P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/ral_command_lifecycle_policy_check.cmake)
SET_TESTS_PROPERTIES(ral_command_lifecycle_source_policy_contract PROPERTIES
	LABELS "host-only;subsystem;owner.ral;failure.contract;feature.always"
	TIMEOUT 30
)

ADD_EXECUTABLE(ral_capability_profile_test
	tests/ral_capability_profile_test.c
	code/renderer/ral/ral_capability.c)
TARGET_INCLUDE_DIRECTORIES(ral_capability_profile_test PRIVATE
	code/qcommon code/renderer/ral code/renderercommon)
ADD_TEST(NAME ral_capability_profile_contract COMMAND ral_capability_profile_test)
SET_TESTS_PROPERTIES(ral_capability_profile_contract PROPERTIES
	LABELS "host-only;subsystem;owner.ral;failure.contract;feature.always" TIMEOUT 30)
WIRED_CONFIGURE_HOST_TEST(ral_capability_profile_test ral_capability_profile_contract)
ADD_EXECUTABLE(ral_webgpu_capability_profile_test
	tests/ral_webgpu_capability_profile_test.c
	code/renderer/ral/ral_capability.c)
TARGET_INCLUDE_DIRECTORIES(ral_webgpu_capability_profile_test PRIVATE
	code/qcommon code/renderer/ral code/renderercommon)
ADD_TEST(NAME ral_webgpu_capability_profile_contract COMMAND ral_webgpu_capability_profile_test)
SET_TESTS_PROPERTIES(ral_webgpu_capability_profile_contract PROPERTIES
	LABELS "host-only;subsystem;owner.ral;failure.contract;feature.always" TIMEOUT 30)
WIRED_CONFIGURE_HOST_TEST(ral_webgpu_capability_profile_test ral_webgpu_capability_profile_contract)
ADD_TEST(NAME ral_capability_profile_source_policy_contract
	COMMAND ${CMAKE_COMMAND} -DROOT=${CMAKE_CURRENT_SOURCE_DIR}
		-P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/ral_capability_profile_policy_check.cmake)
SET_TESTS_PROPERTIES(ral_capability_profile_source_policy_contract PROPERTIES
	LABELS "host-only;infrastructure;owner.ral;failure.contract;feature.always" TIMEOUT 30)
ADD_TEST(NAME ral_backend_conformance_source_policy_contract
	COMMAND ${CMAKE_COMMAND} -DROOT=${CMAKE_CURRENT_SOURCE_DIR}
		-P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/ral_backend_conformance_policy_check.cmake)
SET_TESTS_PROPERTIES(ral_backend_conformance_source_policy_contract PROPERTIES
	LABELS "host-only;infrastructure;owner.ral;failure.contract;backend.vulkan;backend.metal"
	TIMEOUT 30)

ADD_EXECUTABLE(ral_frame_shell_test
	tests/ral_frame_shell_test.c code/renderer/ral/ral_frame_shell.c)
TARGET_INCLUDE_DIRECTORIES(ral_frame_shell_test PRIVATE
	code/qcommon code/renderer/ral code/renderercommon)
TARGET_COMPILE_OPTIONS(ral_frame_shell_test PRIVATE -Wall -Wextra -Werror)
ADD_TEST(NAME ral_frame_shell_contract COMMAND ral_frame_shell_test)
SET_TESTS_PROPERTIES(ral_frame_shell_contract PROPERTIES
	LABELS "host-only;subsystem;owner.ral;failure.contract;feature.always" TIMEOUT 30)
WIRED_CONFIGURE_HOST_TEST(ral_frame_shell_test ral_frame_shell_contract)

ADD_EXECUTABLE(ral_presentation_host_test
	tests/ral_presentation_host_test.c
	code/renderer/ral/ral_presentation_host.c)
TARGET_INCLUDE_DIRECTORIES(ral_presentation_host_test PRIVATE
	code/qcommon code/renderer/ral code/renderercommon)
TARGET_COMPILE_OPTIONS(ral_presentation_host_test PRIVATE -Wall -Wextra -Werror)
ADD_TEST(NAME ral_presentation_host_contract COMMAND ral_presentation_host_test)
SET_TESTS_PROPERTIES(ral_presentation_host_contract PROPERTIES
	LABELS "host-only;subsystem;owner.ral;failure.contract;feature.always" TIMEOUT 30)
WIRED_CONFIGURE_HOST_TEST(ral_presentation_host_test ral_presentation_host_contract)
ADD_TEST(NAME ral_frame_shell_source_policy_contract
	COMMAND ${CMAKE_COMMAND} -DROOT=${CMAKE_CURRENT_SOURCE_DIR}
		-P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/ral_frame_shell_policy_check.cmake)
SET_TESTS_PROPERTIES(ral_frame_shell_source_policy_contract PROPERTIES
	LABELS "host-only;infrastructure;owner.ral;failure.contract;feature.always" TIMEOUT 30)

ADD_TEST(NAME ral_metal_module_source_policy_contract
	COMMAND ${CMAKE_COMMAND} -DROOT=${CMAKE_CURRENT_SOURCE_DIR}
		-P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/ral_metal_module_policy_check.cmake)
SET_TESTS_PROPERTIES(ral_metal_module_source_policy_contract PROPERTIES
	LABELS "host-only;infrastructure;owner.ral;failure.contract;backend.metal;module.renderer"
	TIMEOUT 30)

ADD_TEST(NAME ral_presentation_host_source_policy_contract
	COMMAND ${CMAKE_COMMAND} -DROOT=${CMAKE_CURRENT_SOURCE_DIR}
		-P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/ral_presentation_host_policy_check.cmake)
SET_TESTS_PROPERTIES(ral_presentation_host_source_policy_contract PROPERTIES
	LABELS "host-only;infrastructure;owner.ral;failure.contract;feature.always;interop.platform"
	TIMEOUT 30)

ADD_TEST(NAME ral_metal_core_source_policy_contract
	COMMAND ${CMAKE_COMMAND} -DROOT=${CMAKE_CURRENT_SOURCE_DIR}
		-P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/ral_metal_core_policy_check.cmake)
SET_TESTS_PROPERTIES(ral_metal_core_source_policy_contract PROPERTIES
	LABELS "host-only;infrastructure;owner.ral;failure.contract;backend.metal" TIMEOUT 30)

ADD_TEST(NAME ral_metal_bind_group_source_policy_contract
	COMMAND ${CMAKE_COMMAND} -DROOT=${CMAKE_CURRENT_SOURCE_DIR}
		-P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/ral_metal_bind_group_policy_check.cmake)
SET_TESTS_PROPERTIES(ral_metal_bind_group_source_policy_contract PROPERTIES
	LABELS "host-only;infrastructure;owner.ral;failure.contract;backend.metal" TIMEOUT 30)

ADD_TEST(NAME ral_metal_render_source_policy_contract
	COMMAND ${CMAKE_COMMAND} -DROOT=${CMAKE_CURRENT_SOURCE_DIR}
		-P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/ral_metal_render_policy_check.cmake)
SET_TESTS_PROPERTIES(ral_metal_render_source_policy_contract PROPERTIES
	LABELS "host-only;infrastructure;owner.ral;failure.contract;backend.metal" TIMEOUT 30)

ADD_TEST(NAME ral_metal_pipeline_source_policy_contract
	COMMAND ${CMAKE_COMMAND} -DROOT=${CMAKE_CURRENT_SOURCE_DIR}
		-P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/ral_metal_pipeline_policy_check.cmake)
SET_TESTS_PROPERTIES(ral_metal_pipeline_source_policy_contract PROPERTIES
	LABELS "host-only;infrastructure;owner.ral;failure.contract;backend.metal" TIMEOUT 30)

ADD_TEST(NAME ral_metal_draw_source_policy_contract
	COMMAND ${CMAKE_COMMAND} -DROOT=${CMAKE_CURRENT_SOURCE_DIR}
		-P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/ral_metal_draw_policy_check.cmake)
SET_TESTS_PROPERTIES(ral_metal_draw_source_policy_contract PROPERTIES
	LABELS "host-only;infrastructure;owner.ral;failure.contract;backend.metal" TIMEOUT 30)

ADD_TEST(NAME ral_metal_present_source_policy_contract
	COMMAND ${CMAKE_COMMAND} -DROOT=${CMAKE_CURRENT_SOURCE_DIR}
		-P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/ral_metal_present_policy_check.cmake)
SET_TESTS_PROPERTIES(ral_metal_present_source_policy_contract PROPERTIES
	LABELS "host-only;infrastructure;owner.ral;failure.contract;backend.metal" TIMEOUT 30)

ADD_TEST(NAME ral_metal_sdl_source_policy_contract
	COMMAND ${CMAKE_COMMAND} -DROOT=${CMAKE_CURRENT_SOURCE_DIR}
		-P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/ral_metal_sdl_policy_check.cmake)
SET_TESTS_PROPERTIES(ral_metal_sdl_source_policy_contract PROPERTIES
	LABELS "host-only;infrastructure;owner.ral;failure.contract;backend.metal;platform.sdl3" TIMEOUT 30)

ADD_TEST(NAME ral_sdl_presentation_host_source_policy_contract
	COMMAND ${CMAKE_COMMAND} -DROOT=${CMAKE_CURRENT_SOURCE_DIR}
		-P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/ral_sdl_presentation_host_policy_check.cmake)
SET_TESTS_PROPERTIES(ral_sdl_presentation_host_source_policy_contract PROPERTIES
	LABELS "host-only;infrastructure;owner.ral;failure.contract;backend.metal;platform.sdl3"
	TIMEOUT 30)

IF(APPLE)
	FIND_LIBRARY(WIRED_METAL_FRAMEWORK Metal REQUIRED)
	FIND_LIBRARY(WIRED_FOUNDATION_FRAMEWORK Foundation REQUIRED)
	FIND_LIBRARY(WIRED_QUARTZCORE_FRAMEWORK QuartzCore REQUIRED)
	FIND_LIBRARY(WIRED_COREGRAPHICS_FRAMEWORK CoreGraphics REQUIRED)
	ADD_LIBRARY(ral_metal STATIC
		code/renderer/ral_metal/ral_metal_core.mm
		code/renderer/ral_metal/ral_metal_bind_group.mm
		code/renderer/ral_metal/ral_metal_render.mm
		code/renderer/ral_metal/ral_metal_pipeline.mm
		code/renderer/ral_metal/ral_metal_draw.mm
		code/renderer/ral_metal/ral_metal_present.mm
		code/renderer/ral/ral_allocation.c
		code/renderer/ral/ral_backend_conformance.c
		code/renderer/ral/ral_capability.c
		code/renderer/ral/ral_command_lifecycle.c
		code/renderer/ral/ral_frame_shell.c
		code/renderer/ral/ral_presentation_host.c
		code/renderer/ral/ral_memory_recovery.c
		code/renderer/ral/ral_shader_abi.c
		code/renderer/ral/ral_transfer.c)
	TARGET_INCLUDE_DIRECTORIES(ral_metal PUBLIC
		code/qcommon code/renderer/ral code/renderer/ral_metal code/renderercommon)
	TARGET_LINK_LIBRARIES(ral_metal PUBLIC
		${WIRED_METAL_FRAMEWORK} ${WIRED_FOUNDATION_FRAMEWORK}
		${WIRED_QUARTZCORE_FRAMEWORK} ${WIRED_COREGRAPHICS_FRAMEWORK})
	TARGET_COMPILE_OPTIONS(ral_metal PRIVATE -Wall -Wextra -Werror -fblocks)
	ADD_LIBRARY(ral_metal_sdl STATIC code/sdl/sdl_metal_ral.mm)
	TARGET_INCLUDE_DIRECTORIES(ral_metal_sdl PUBLIC
		code/qcommon code/renderer/ral code/renderer/ral_metal
		code/renderercommon code/sdl)
	TARGET_LINK_LIBRARIES(ral_metal_sdl PUBLIC ral_metal SDL3::SDL3)
	TARGET_COMPILE_OPTIONS(ral_metal_sdl PRIVATE -Wall -Wextra -Werror)
	ADD_LIBRARY(ral_sdl_presentation_host STATIC
		code/sdl/sdl_ral_presentation.mm)
	TARGET_INCLUDE_DIRECTORIES(ral_sdl_presentation_host PUBLIC
		code/qcommon code/renderer/ral code/renderer/ral_metal
		code/renderercommon code/sdl)
	TARGET_LINK_LIBRARIES(ral_sdl_presentation_host PUBLIC ral_metal SDL3::SDL3)
	TARGET_COMPILE_OPTIONS(ral_sdl_presentation_host PRIVATE -Wall -Wextra -Werror)

	ADD_EXECUTABLE(ral_metal_core_test tests/ral_metal_core_test.mm)
	TARGET_LINK_LIBRARIES(ral_metal_core_test PRIVATE ral_metal)
	TARGET_COMPILE_OPTIONS(ral_metal_core_test PRIVATE -Wall -Wextra -Werror)
	ADD_TEST(NAME ral_metal_core_contract COMMAND ral_metal_core_test)
	SET_TESTS_PROPERTIES(ral_metal_core_contract PROPERTIES
		LABELS "host-only;subsystem;owner.ral;failure.contract;backend.metal" TIMEOUT 30)
	WIRED_CONFIGURE_HOST_TEST(ral_metal_core_test ral_metal_core_contract)
	ADD_EXECUTABLE(ral_sdl_presentation_host_test
		tests/ral_sdl_presentation_host_test.c)
	TARGET_LINK_LIBRARIES(ral_sdl_presentation_host_test PRIVATE
		ral_sdl_presentation_host)
	TARGET_COMPILE_OPTIONS(ral_sdl_presentation_host_test PRIVATE
		-Wall -Wextra -Werror)
	ADD_TEST(NAME ral_sdl_presentation_host_contract
		COMMAND ral_sdl_presentation_host_test)
	SET_TESTS_PROPERTIES(ral_sdl_presentation_host_contract PROPERTIES
		LABELS "host-only;subsystem;owner.ral;failure.contract;backend.metal;platform.sdl3"
		TIMEOUT 30)
	WIRED_CONFIGURE_HOST_TEST(ral_sdl_presentation_host_test
		ral_sdl_presentation_host_contract)
	ADD_EXECUTABLE(ral_metal_bind_group_test tests/ral_metal_bind_group_test.mm)
	TARGET_LINK_LIBRARIES(ral_metal_bind_group_test PRIVATE ral_metal)
	TARGET_COMPILE_OPTIONS(ral_metal_bind_group_test PRIVATE -Wall -Wextra -Werror)
	ADD_TEST(NAME ral_metal_bind_group_contract COMMAND ral_metal_bind_group_test)
	SET_TESTS_PROPERTIES(ral_metal_bind_group_contract PROPERTIES
		LABELS "host-only;subsystem;owner.ral;failure.contract;backend.metal" TIMEOUT 30)
	WIRED_CONFIGURE_HOST_TEST(ral_metal_bind_group_test ral_metal_bind_group_contract)
	ADD_EXECUTABLE(ral_metal_render_test tests/ral_metal_render_test.mm)
	TARGET_LINK_LIBRARIES(ral_metal_render_test PRIVATE ral_metal)
	TARGET_COMPILE_OPTIONS(ral_metal_render_test PRIVATE -Wall -Wextra -Werror)
	ADD_TEST(NAME ral_metal_render_contract COMMAND ral_metal_render_test)
	SET_TESTS_PROPERTIES(ral_metal_render_contract PROPERTIES
		LABELS "host-only;subsystem;owner.ral;failure.contract;backend.metal" TIMEOUT 30)
	WIRED_CONFIGURE_HOST_TEST(ral_metal_render_test ral_metal_render_contract)

	SET(RAL_METAL_SHADER_BUILD_DIR "${CMAKE_CURRENT_BINARY_DIR}/metal-shaders")
	SET(RAL_METAL_OVERLAY_VERT_SOURCE
		"${CMAKE_CURRENT_SOURCE_DIR}/code/renderervk/shaders/portable/overlay_vert_spv.msl")
	SET(RAL_METAL_OVERLAY_FRAG_SOURCE
		"${CMAKE_CURRENT_SOURCE_DIR}/code/renderervk/shaders/portable/overlay_frag_spv.msl")
	SET(RAL_METAL_OVERLAY_VERT_AIR "${RAL_METAL_SHADER_BUILD_DIR}/overlay_vert_spv.air")
	SET(RAL_METAL_OVERLAY_FRAG_AIR "${RAL_METAL_SHADER_BUILD_DIR}/overlay_frag_spv.air")
	SET(RAL_METAL_OVERLAY_VERT_LIBRARY "${RAL_METAL_SHADER_BUILD_DIR}/overlay_vert_spv.metallib")
	SET(RAL_METAL_OVERLAY_FRAG_LIBRARY "${RAL_METAL_SHADER_BUILD_DIR}/overlay_frag_spv.metallib")
	ADD_CUSTOM_COMMAND(
		OUTPUT ${RAL_METAL_OVERLAY_VERT_LIBRARY} ${RAL_METAL_OVERLAY_FRAG_LIBRARY}
		BYPRODUCTS ${RAL_METAL_OVERLAY_VERT_AIR} ${RAL_METAL_OVERLAY_FRAG_AIR}
		COMMAND ${CMAKE_COMMAND} -E make_directory ${RAL_METAL_SHADER_BUILD_DIR}
		COMMAND xcrun -sdk macosx metal -x metal -c ${RAL_METAL_OVERLAY_VERT_SOURCE}
			-o ${RAL_METAL_OVERLAY_VERT_AIR}
		COMMAND xcrun -sdk macosx metallib ${RAL_METAL_OVERLAY_VERT_AIR}
			-o ${RAL_METAL_OVERLAY_VERT_LIBRARY}
		COMMAND xcrun -sdk macosx metal -x metal -c ${RAL_METAL_OVERLAY_FRAG_SOURCE}
			-o ${RAL_METAL_OVERLAY_FRAG_AIR}
		COMMAND xcrun -sdk macosx metallib ${RAL_METAL_OVERLAY_FRAG_AIR}
			-o ${RAL_METAL_OVERLAY_FRAG_LIBRARY}
		DEPENDS ${RAL_METAL_OVERLAY_VERT_SOURCE} ${RAL_METAL_OVERLAY_FRAG_SOURCE}
		VERBATIM)
	ADD_CUSTOM_TARGET(ral_metal_overlay_metallib DEPENDS
		${RAL_METAL_OVERLAY_VERT_LIBRARY} ${RAL_METAL_OVERLAY_FRAG_LIBRARY})
	ADD_EXECUTABLE(ral_metal_pipeline_test tests/ral_metal_pipeline_test.mm)
	ADD_DEPENDENCIES(ral_metal_pipeline_test ral_metal_overlay_metallib)
	TARGET_LINK_LIBRARIES(ral_metal_pipeline_test PRIVATE ral_metal)
	TARGET_COMPILE_OPTIONS(ral_metal_pipeline_test PRIVATE -Wall -Wextra -Werror)
	TARGET_COMPILE_DEFINITIONS(ral_metal_pipeline_test PRIVATE
		RAL_METAL_TEST_VERTEX_SOURCE="${RAL_METAL_OVERLAY_VERT_SOURCE}"
		RAL_METAL_TEST_FRAGMENT_SOURCE="${RAL_METAL_OVERLAY_FRAG_SOURCE}"
		RAL_METAL_TEST_VERTEX_LIBRARY="${RAL_METAL_OVERLAY_VERT_LIBRARY}"
		RAL_METAL_TEST_FRAGMENT_LIBRARY="${RAL_METAL_OVERLAY_FRAG_LIBRARY}")
	ADD_TEST(NAME ral_metal_pipeline_contract COMMAND ral_metal_pipeline_test)
	SET_TESTS_PROPERTIES(ral_metal_pipeline_contract PROPERTIES
		LABELS "host-only;subsystem;owner.ral;failure.contract;backend.metal" TIMEOUT 30)
	WIRED_CONFIGURE_HOST_TEST(ral_metal_pipeline_test ral_metal_pipeline_contract)
	ADD_EXECUTABLE(ral_metal_draw_test tests/ral_metal_draw_test.mm)
	ADD_DEPENDENCIES(ral_metal_draw_test ral_metal_overlay_metallib)
	TARGET_LINK_LIBRARIES(ral_metal_draw_test PRIVATE ral_metal)
	TARGET_COMPILE_OPTIONS(ral_metal_draw_test PRIVATE -Wall -Wextra -Werror)
	TARGET_COMPILE_DEFINITIONS(ral_metal_draw_test PRIVATE
		RAL_METAL_TEST_VERTEX_SOURCE="${RAL_METAL_OVERLAY_VERT_SOURCE}"
		RAL_METAL_TEST_FRAGMENT_SOURCE="${RAL_METAL_OVERLAY_FRAG_SOURCE}"
		RAL_METAL_TEST_VERTEX_LIBRARY="${RAL_METAL_OVERLAY_VERT_LIBRARY}"
		RAL_METAL_TEST_FRAGMENT_LIBRARY="${RAL_METAL_OVERLAY_FRAG_LIBRARY}")
	ADD_TEST(NAME ral_metal_draw_contract COMMAND ral_metal_draw_test)
	SET_TESTS_PROPERTIES(ral_metal_draw_contract PROPERTIES
		LABELS "host-only;subsystem;owner.ral;failure.contract;backend.metal" TIMEOUT 30)
	WIRED_CONFIGURE_HOST_TEST(ral_metal_draw_test ral_metal_draw_contract)
	ADD_EXECUTABLE(ral_metal_present_test tests/ral_metal_present_test.mm)
	TARGET_LINK_LIBRARIES(ral_metal_present_test PRIVATE ral_metal)
	TARGET_COMPILE_OPTIONS(ral_metal_present_test PRIVATE -Wall -Wextra -Werror)
	ADD_TEST(NAME ral_metal_present_contract COMMAND ral_metal_present_test)
	SET_TESTS_PROPERTIES(ral_metal_present_contract PROPERTIES
		LABELS "host-only;subsystem;owner.ral;failure.contract;backend.metal" TIMEOUT 30)
	WIRED_CONFIGURE_HOST_TEST(ral_metal_present_test ral_metal_present_contract)
	ADD_EXECUTABLE(wired_metal_smoke tests/wired_metal_smoke.mm)
	TARGET_LINK_LIBRARIES(wired_metal_smoke PRIVATE ral_metal_sdl)
	TARGET_COMPILE_OPTIONS(wired_metal_smoke PRIVATE -Wall -Wextra -Werror)
	ADD_TEST(NAME wired_metal_window_smoke COMMAND wired_metal_smoke)
	SET_TESTS_PROPERTIES(wired_metal_window_smoke PROPERTIES
		LABELS "host-only;subsystem;owner.ral;failure.contract;backend.metal;platform.sdl3;smoke"
		TIMEOUT 30)
	WIRED_CONFIGURE_HOST_TEST(wired_metal_smoke wired_metal_window_smoke)
	ADD_TEST(NAME ral_metal_link_policy_contract
		COMMAND ${CMAKE_COMMAND} -DBINARY=$<TARGET_FILE:ral_metal_core_test>
			-P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/ral_metal_link_policy_check.cmake)
	SET_TESTS_PROPERTIES(ral_metal_link_policy_contract PROPERTIES
		LABELS "host-only;infrastructure;owner.ral;failure.link-policy;backend.metal" TIMEOUT 30)
	ADD_TEST(NAME wired_metal_window_link_policy_contract
		COMMAND ${CMAKE_COMMAND} -DBINARY=$<TARGET_FILE:wired_metal_smoke>
			-P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/ral_metal_window_link_policy_check.cmake)
	SET_TESTS_PROPERTIES(wired_metal_window_link_policy_contract PROPERTIES
		LABELS "host-only;infrastructure;owner.ral;failure.link-policy;backend.metal;platform.sdl3"
		TIMEOUT 30)
ENDIF()

ADD_EXECUTABLE(ral_allocation_receipt_test
	tests/ral_allocation_receipt_test.c
	code/renderer/ral/ral_allocation.c)
TARGET_INCLUDE_DIRECTORIES(ral_allocation_receipt_test PRIVATE
	code/qcommon code/renderer/ral code/renderercommon)
ADD_TEST(NAME ral_allocation_receipt_contract COMMAND ral_allocation_receipt_test)
SET_TESTS_PROPERTIES(ral_allocation_receipt_contract PROPERTIES
	LABELS "host-only;subsystem;owner.ral;failure.contract;feature.always" TIMEOUT 30)
WIRED_CONFIGURE_HOST_TEST(ral_allocation_receipt_test ral_allocation_receipt_contract)
ADD_EXECUTABLE(ral_webgpu_allocation_receipt_test
	tests/ral_webgpu_allocation_receipt_test.c
	code/renderer/ral/ral_allocation.c)
TARGET_INCLUDE_DIRECTORIES(ral_webgpu_allocation_receipt_test PRIVATE
	code/qcommon code/renderer/ral code/renderercommon)
ADD_TEST(NAME ral_webgpu_allocation_receipt_contract COMMAND ral_webgpu_allocation_receipt_test)
SET_TESTS_PROPERTIES(ral_webgpu_allocation_receipt_contract PROPERTIES
	LABELS "host-only;subsystem;owner.ral;failure.contract;feature.always" TIMEOUT 30)
WIRED_CONFIGURE_HOST_TEST(ral_webgpu_allocation_receipt_test ral_webgpu_allocation_receipt_contract)
ADD_TEST(NAME ral_allocation_receipt_source_policy_contract
	COMMAND ${CMAKE_COMMAND} -DROOT=${CMAKE_CURRENT_SOURCE_DIR}
		-P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/ral_allocation_receipt_policy_check.cmake)
SET_TESTS_PROPERTIES(ral_allocation_receipt_source_policy_contract PROPERTIES
	LABELS "host-only;infrastructure;owner.ral;failure.contract;feature.always" TIMEOUT 30)

ADD_EXECUTABLE(ral_transient_plan_test
	tests/ral_transient_plan_test.c
	code/renderer/ral/ral_transient.c)
TARGET_INCLUDE_DIRECTORIES(ral_transient_plan_test PRIVATE
	code/qcommon code/renderer/ral code/renderercommon)
ADD_TEST(NAME ral_transient_plan_contract COMMAND ral_transient_plan_test)
SET_TESTS_PROPERTIES(ral_transient_plan_contract PROPERTIES
	LABELS "host-only;subsystem;owner.ral;failure.contract;feature.always" TIMEOUT 30)
WIRED_CONFIGURE_HOST_TEST(ral_transient_plan_test ral_transient_plan_contract)
ADD_EXECUTABLE(ral_webgpu_transient_plan_test
	tests/ral_webgpu_transient_plan_test.c
	code/renderer/ral/ral_transient.c)
TARGET_INCLUDE_DIRECTORIES(ral_webgpu_transient_plan_test PRIVATE
	code/qcommon code/renderer/ral code/renderercommon)
ADD_TEST(NAME ral_webgpu_transient_plan_contract COMMAND ral_webgpu_transient_plan_test)
SET_TESTS_PROPERTIES(ral_webgpu_transient_plan_contract PROPERTIES
	LABELS "host-only;subsystem;owner.ral;failure.contract;feature.always" TIMEOUT 30)
WIRED_CONFIGURE_HOST_TEST(ral_webgpu_transient_plan_test ral_webgpu_transient_plan_contract)
ADD_TEST(NAME ral_transient_source_policy_contract
	COMMAND ${CMAKE_COMMAND} -DROOT=${CMAKE_CURRENT_SOURCE_DIR}
		-P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/ral_transient_policy_check.cmake)
SET_TESTS_PROPERTIES(ral_transient_source_policy_contract PROPERTIES
	LABELS "host-only;infrastructure;owner.ral;failure.contract;feature.always" TIMEOUT 30)

ADD_EXECUTABLE(ral_transient_texture_cohort_test
	tests/ral_transient_texture_cohort_test.c
	code/renderer/ral/ral_transient_texture.c
	code/renderer/ral/ral_transient.c
	code/renderer/ral/ral_allocation.c)
TARGET_INCLUDE_DIRECTORIES(ral_transient_texture_cohort_test PRIVATE
	code/qcommon code/renderer/ral code/renderercommon)
ADD_TEST(NAME ral_transient_texture_cohort_contract COMMAND ral_transient_texture_cohort_test)
SET_TESTS_PROPERTIES(ral_transient_texture_cohort_contract PROPERTIES
	LABELS "host-only;subsystem;owner.ral;failure.contract;feature.always" TIMEOUT 30)
WIRED_CONFIGURE_HOST_TEST(ral_transient_texture_cohort_test ral_transient_texture_cohort_contract)
ADD_TEST(NAME ral_transient_texture_source_policy_contract
	COMMAND ${CMAKE_COMMAND} -DROOT=${CMAKE_CURRENT_SOURCE_DIR}
		-P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/ral_transient_texture_policy_check.cmake)
SET_TESTS_PROPERTIES(ral_transient_texture_source_policy_contract PROPERTIES
	LABELS "host-only;infrastructure;owner.ral;failure.contract;feature.always" TIMEOUT 30)

ADD_EXECUTABLE(ral_shader_abi_test
	tests/ral_shader_abi_test.c
	code/renderer/ral/ral_shader_abi.c
)

ADD_EXECUTABLE(ral_transfer_lifecycle_test
	tests/ral_transfer_lifecycle_test.c code/renderer/ral/ral_transfer.c)
TARGET_INCLUDE_DIRECTORIES(ral_transfer_lifecycle_test PRIVATE
	code/qcommon code/renderer/ral code/renderercommon)
ADD_TEST(NAME ral_transfer_lifecycle_contract COMMAND ral_transfer_lifecycle_test)
SET_TESTS_PROPERTIES(ral_transfer_lifecycle_contract PROPERTIES
	LABELS "host-only;subsystem;owner.ral;failure.contract;feature.always" TIMEOUT 30)
WIRED_CONFIGURE_HOST_TEST(ral_transfer_lifecycle_test ral_transfer_lifecycle_contract)
ADD_EXECUTABLE(ral_webgpu_transfer_lifecycle_test
	tests/ral_webgpu_transfer_lifecycle_test.c code/renderer/ral/ral_transfer.c)
TARGET_INCLUDE_DIRECTORIES(ral_webgpu_transfer_lifecycle_test PRIVATE
	code/qcommon code/renderer/ral code/renderercommon)
ADD_TEST(NAME ral_webgpu_transfer_lifecycle_contract COMMAND ral_webgpu_transfer_lifecycle_test)
SET_TESTS_PROPERTIES(ral_webgpu_transfer_lifecycle_contract PROPERTIES
	LABELS "host-only;subsystem;owner.ral;failure.contract;feature.always" TIMEOUT 30)
WIRED_CONFIGURE_HOST_TEST(ral_webgpu_transfer_lifecycle_test ral_webgpu_transfer_lifecycle_contract)
ADD_TEST(NAME ral_transfer_source_policy_contract
	COMMAND ${CMAKE_COMMAND} -DROOT=${CMAKE_CURRENT_SOURCE_DIR}
		-P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/ral_transfer_policy_check.cmake)
SET_TESTS_PROPERTIES(ral_transfer_source_policy_contract PROPERTIES
	LABELS "host-only;infrastructure;owner.ral;failure.contract;feature.always" TIMEOUT 30)
ADD_EXECUTABLE(ral_readback_owner_test
	tests/ral_readback_owner_test.c
	code/renderer/ral/ral_readback.c
	code/renderer/ral/ral_transfer.c
	code/renderer/ral/ral_allocation.c
	code/renderer/ral/ral_buffer_map.c)
TARGET_INCLUDE_DIRECTORIES(ral_readback_owner_test PRIVATE
	code/qcommon code/renderer/ral code/renderercommon)
ADD_TEST(NAME ral_readback_owner_contract COMMAND ral_readback_owner_test)
SET_TESTS_PROPERTIES(ral_readback_owner_contract PROPERTIES
	LABELS "host-only;subsystem;owner.ral;failure.contract;feature.always" TIMEOUT 30)
ADD_TEST(NAME ral_webgpu_readback_owner_contract COMMAND ral_readback_owner_test)
SET_TESTS_PROPERTIES(ral_webgpu_readback_owner_contract PROPERTIES
	LABELS "host-only;subsystem;owner.ral;failure.contract;feature.always;backend.webgpu" TIMEOUT 30)
WIRED_CONFIGURE_HOST_TEST(ral_readback_owner_test ral_readback_owner_contract)
ADD_TEST(NAME ral_readback_source_policy_contract
	COMMAND ${CMAKE_COMMAND} -DROOT=${CMAKE_CURRENT_SOURCE_DIR}
		-P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/ral_readback_policy_check.cmake)
SET_TESTS_PROPERTIES(ral_readback_source_policy_contract PROPERTIES
	LABELS "host-only;infrastructure;owner.ral;failure.contract;feature.always" TIMEOUT 30)
ADD_EXECUTABLE(ral_memory_recovery_test
	tests/ral_memory_recovery_test.c code/renderer/ral/ral_memory_recovery.c)
TARGET_INCLUDE_DIRECTORIES(ral_memory_recovery_test PRIVATE
	code/qcommon code/renderer/ral code/renderercommon)
ADD_TEST(NAME ral_memory_recovery_contract COMMAND ral_memory_recovery_test)
SET_TESTS_PROPERTIES(ral_memory_recovery_contract PROPERTIES
	LABELS "host-only;subsystem;owner.ral;failure.contract;feature.always" TIMEOUT 30)
WIRED_CONFIGURE_HOST_TEST(ral_memory_recovery_test ral_memory_recovery_contract)
ADD_TEST(NAME ral_memory_recovery_source_policy_contract
	COMMAND ${CMAKE_COMMAND} -DROOT=${CMAKE_CURRENT_SOURCE_DIR}
		-P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/ral_memory_recovery_policy_check.cmake)
SET_TESTS_PROPERTIES(ral_memory_recovery_source_policy_contract PROPERTIES
	LABELS "host-only;infrastructure;owner.ral;failure.contract;feature.always" TIMEOUT 30)
ADD_EXECUTABLE(ral_suballocation_test
	tests/ral_suballocation_test.c code/renderer/ral/ral_suballocation.c)
TARGET_INCLUDE_DIRECTORIES(ral_suballocation_test PRIVATE
	code/qcommon code/renderer/ral code/renderercommon)
ADD_TEST(NAME ral_suballocation_contract COMMAND ral_suballocation_test)
SET_TESTS_PROPERTIES(ral_suballocation_contract PROPERTIES
	LABELS "host-only;subsystem;owner.ral;failure.contract;feature.always" TIMEOUT 30)
WIRED_CONFIGURE_HOST_TEST(ral_suballocation_test ral_suballocation_contract)
ADD_TEST(NAME ral_suballocation_source_policy_contract
	COMMAND ${CMAKE_COMMAND} -DROOT=${CMAKE_CURRENT_SOURCE_DIR}
		-P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/ral_suballocation_policy_check.cmake)
SET_TESTS_PROPERTIES(ral_suballocation_source_policy_contract PROPERTIES
	LABELS "host-only;infrastructure;owner.ral;failure.contract;feature.always" TIMEOUT 30)
ADD_EXECUTABLE(ral_vulkan_suballocation_test
	tests/ral_vulkan_suballocation_test.c
	code/renderer/ral/ral_allocation.c
	code/renderer/ral/ral_memory_recovery.c
	code/renderer/ral/ral_suballocation.c
	code/renderer/ral_vulkan/ral_vulkan_memory.c)
TARGET_INCLUDE_DIRECTORIES(ral_vulkan_suballocation_test PRIVATE
	code/qcommon code/renderer/ral code/renderer/ral_vulkan code/renderercommon)
TARGET_LINK_LIBRARIES(ral_vulkan_suballocation_test PRIVATE Threads::Threads)
ADD_TEST(NAME ral_vulkan_suballocation_contract COMMAND ral_vulkan_suballocation_test)
SET_TESTS_PROPERTIES(ral_vulkan_suballocation_contract PROPERTIES
	LABELS "host-only;subsystem;owner.ral;failure.contract;feature.always;backend.vulkan" TIMEOUT 30)
WIRED_CONFIGURE_HOST_TEST(ral_vulkan_suballocation_test ral_vulkan_suballocation_contract)
TARGET_INCLUDE_DIRECTORIES(ral_shader_abi_test PRIVATE
	code/qcommon
	code/renderer/ral
	code/renderercommon
)
ADD_TEST(NAME ral_shader_abi_contract COMMAND ral_shader_abi_test)
SET_TESTS_PROPERTIES(ral_shader_abi_contract PROPERTIES
	LABELS "host-only;subsystem;owner.ral;failure.contract;feature.always"
	TIMEOUT 30
)
WIRED_CONFIGURE_HOST_TEST(ral_shader_abi_test ral_shader_abi_contract)
ADD_EXECUTABLE(ral_webgpu_shader_abi_test
	tests/ral_webgpu_shader_abi_test.c
	code/renderer/ral/ral_shader_abi.c
)
TARGET_INCLUDE_DIRECTORIES(ral_webgpu_shader_abi_test PRIVATE
	code/qcommon
	code/renderer/ral
	code/renderercommon
)
ADD_TEST(NAME ral_webgpu_shader_abi_contract COMMAND ral_webgpu_shader_abi_test)
SET_TESTS_PROPERTIES(ral_webgpu_shader_abi_contract PROPERTIES
	LABELS "host-only;subsystem;owner.ral;failure.contract;feature.always"
	TIMEOUT 30
)
WIRED_CONFIGURE_HOST_TEST(ral_webgpu_shader_abi_test ral_webgpu_shader_abi_contract)
ADD_TEST(NAME ral_shader_abi_source_policy_contract
	COMMAND ${CMAKE_COMMAND} -DROOT=${CMAKE_CURRENT_SOURCE_DIR}
		-P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/ral_shader_abi_policy_check.cmake)
SET_TESTS_PROPERTIES(ral_shader_abi_source_policy_contract PROPERTIES
	LABELS "host-only;subsystem;owner.ral;failure.contract;feature.always"
	TIMEOUT 30
)
FIND_PROGRAM(WIRED_NODE_EXECUTABLE NAMES node nodejs)
IF(WIRED_NODE_EXECUTABLE)
	ADD_TEST(NAME ral_shader_artifact_catalog_contract
		COMMAND ${WIRED_NODE_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/ral_shader_artifact_catalog_test.mjs)
	SET_TESTS_PROPERTIES(ral_shader_artifact_catalog_contract PROPERTIES
		LABELS "host-only;subsystem;owner.ral;failure.contract;feature.always"
		TIMEOUT 30
	)
ENDIF()
ADD_TEST(NAME ral_shader_artifact_catalog_source_policy_contract
	COMMAND ${CMAKE_COMMAND} -DROOT=${CMAKE_CURRENT_SOURCE_DIR}
		-P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/ral_shader_artifact_catalog_policy_check.cmake)
SET_TESTS_PROPERTIES(ral_shader_artifact_catalog_source_policy_contract PROPERTIES
	LABELS "host-only;subsystem;owner.ral;failure.contract;feature.always"
	TIMEOUT 30
)
IF(WIRED_NODE_EXECUTABLE)
	ADD_TEST(NAME ral_shader_translation_driver_contract
		COMMAND ${WIRED_NODE_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/ral_shader_translation_driver_test.mjs)
	SET_TESTS_PROPERTIES(ral_shader_translation_driver_contract PROPERTIES
		LABELS "host-only;subsystem;owner.ral;failure.contract;feature.always"
		TIMEOUT 30
	)
ENDIF()
ADD_TEST(NAME ral_shader_translation_source_policy_contract
	COMMAND ${CMAKE_COMMAND} -DROOT=${CMAKE_CURRENT_SOURCE_DIR}
		-P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/ral_shader_translation_policy_check.cmake)
SET_TESTS_PROPERTIES(ral_shader_translation_source_policy_contract PROPERTIES
	LABELS "host-only;subsystem;owner.ral;failure.contract;feature.always"
	TIMEOUT 30
)
IF(WIRED_NODE_EXECUTABLE)
	ADD_TEST(NAME ral_shader_reflection_abi_contract
		COMMAND ${WIRED_NODE_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/ral_shader_reflection_abi_test.mjs)
	SET_TESTS_PROPERTIES(ral_shader_reflection_abi_contract PROPERTIES
		LABELS "host-only;subsystem;owner.ral;failure.contract;feature.always"
		TIMEOUT 30
	)
	ADD_TEST(NAME ral_shader_reflection_catalog_contract
		COMMAND ${WIRED_NODE_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/ral_shader_reflection_catalog_test.mjs)
	SET_TESTS_PROPERTIES(ral_shader_reflection_catalog_contract PROPERTIES
		LABELS "host-only;subsystem;owner.ral;failure.contract;feature.always"
		TIMEOUT 30
	)
ENDIF()
ADD_TEST(NAME ral_shader_reflection_source_policy_contract
	COMMAND ${CMAKE_COMMAND} -DROOT=${CMAKE_CURRENT_SOURCE_DIR}
		-P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/ral_shader_reflection_policy_check.cmake)
SET_TESTS_PROPERTIES(ral_shader_reflection_source_policy_contract PROPERTIES
	LABELS "host-only;subsystem;owner.ral;failure.contract;feature.always"
	TIMEOUT 30
)
IF(WIRED_NODE_EXECUTABLE)
	ADD_TEST(NAME ral_shader_manifest_compose_contract
		COMMAND ${WIRED_NODE_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/ral_shader_manifest_compose_test.mjs)
	SET_TESTS_PROPERTIES(ral_shader_manifest_compose_contract PROPERTIES
		LABELS "host-only;subsystem;owner.ral;failure.contract;feature.always"
		TIMEOUT 30
	)
	ADD_TEST(NAME ral_shader_portable_manifest_contract
		COMMAND ${WIRED_NODE_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/ral_shader_portable_manifest_test.mjs)
	SET_TESTS_PROPERTIES(ral_shader_portable_manifest_contract PROPERTIES
		LABELS "host-only;subsystem;owner.ral;failure.contract;feature.always"
		TIMEOUT 60
	)
	ADD_TEST(NAME ral_webgpu_shader_catalog_contract
		COMMAND ${WIRED_NODE_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/ral_webgpu_shader_catalog_test.mjs)
	SET_TESTS_PROPERTIES(ral_webgpu_shader_catalog_contract PROPERTIES
		LABELS "host-only;subsystem;owner.ral;failure.contract;feature.always"
		TIMEOUT 60
	)
	ADD_TEST(NAME ral_metal_shader_catalog_contract
		COMMAND ${WIRED_NODE_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/ral_metal_shader_catalog_test.mjs)
	SET_TESTS_PROPERTIES(ral_metal_shader_catalog_contract PROPERTIES
		LABELS "host-only;subsystem;owner.ral;failure.contract;backend.metal"
		TIMEOUT 60
	)
ENDIF()
ADD_TEST(NAME ral_metal_shader_catalog_source_policy_contract
	COMMAND ${CMAKE_COMMAND} -DROOT=${CMAKE_CURRENT_SOURCE_DIR}
		-P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/ral_metal_shader_catalog_policy_check.cmake)
SET_TESTS_PROPERTIES(ral_metal_shader_catalog_source_policy_contract PROPERTIES
	LABELS "host-only;infrastructure;owner.ral;failure.contract;backend.metal"
	TIMEOUT 30
)
ADD_EXECUTABLE(ral_shader_manifest_schema_test
	tests/ral_shader_manifest_schema_test.c
	code/renderer/ral/ral_shader_abi.c
)
TARGET_INCLUDE_DIRECTORIES(ral_shader_manifest_schema_test PRIVATE
	code/qcommon
	code/renderer/ral
	code/renderercommon
)
ADD_TEST(NAME ral_shader_manifest_schema_contract COMMAND ral_shader_manifest_schema_test)
SET_TESTS_PROPERTIES(ral_shader_manifest_schema_contract PROPERTIES
	LABELS "host-only;subsystem;owner.ral;failure.contract;feature.always"
	TIMEOUT 30
)
WIRED_CONFIGURE_HOST_TEST(ral_shader_manifest_schema_test ral_shader_manifest_schema_contract)
ADD_TEST(NAME ral_shader_manifest_compose_source_policy_contract
	COMMAND ${CMAKE_COMMAND} -DROOT=${CMAKE_CURRENT_SOURCE_DIR}
		-P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/ral_shader_manifest_compose_policy_check.cmake)
SET_TESTS_PROPERTIES(ral_shader_manifest_compose_source_policy_contract PROPERTIES
	LABELS "host-only;subsystem;owner.ral;failure.contract;feature.always"
	TIMEOUT 30
)
ADD_TEST(NAME ral_shader_portable_catalog_source_policy_contract
	COMMAND ${CMAKE_COMMAND} -DROOT=${CMAKE_CURRENT_SOURCE_DIR}
		-P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/ral_shader_portable_catalog_policy_check.cmake)
SET_TESTS_PROPERTIES(ral_shader_portable_catalog_source_policy_contract PROPERTIES
	LABELS "host-only;subsystem;owner.ral;failure.contract;feature.always"
	TIMEOUT 30
)

ADD_EXECUTABLE(ral_buffer_map_state_test
	tests/ral_buffer_map_state_test.c
	code/renderer/ral/ral_buffer_map.c
)
TARGET_INCLUDE_DIRECTORIES(ral_buffer_map_state_test PRIVATE
	code/qcommon
	code/renderer/ral
	code/renderercommon
)
ADD_TEST(NAME ral_buffer_map_state_contract COMMAND ral_buffer_map_state_test)
SET_TESTS_PROPERTIES(ral_buffer_map_state_contract PROPERTIES
	LABELS "host-only;subsystem;owner.ral;failure.contract;feature.always"
	TIMEOUT 30
)
WIRED_CONFIGURE_HOST_TEST(ral_buffer_map_state_test ral_buffer_map_state_contract)
ADD_EXECUTABLE(ral_vulkan_buffer_map_test
	tests/ral_vulkan_buffer_map_test.c
	code/renderer/ral/ral_buffer_map.c
	code/renderer/ral_vulkan/ral_vulkan_map.c
)
TARGET_INCLUDE_DIRECTORIES(ral_vulkan_buffer_map_test PRIVATE
	code/qcommon
	code/renderer/ral
	code/renderer/ral_vulkan
	code/renderercommon
)
ADD_TEST(NAME ral_vulkan_buffer_map_contract COMMAND ral_vulkan_buffer_map_test)
SET_TESTS_PROPERTIES(ral_vulkan_buffer_map_contract PROPERTIES
	LABELS "host-only;subsystem;owner.ral;failure.contract;feature.always"
	TIMEOUT 30
)
WIRED_CONFIGURE_HOST_TEST(ral_vulkan_buffer_map_test ral_vulkan_buffer_map_contract)
ADD_EXECUTABLE(ral_webgpu_buffer_map_contract_test
	tests/ral_webgpu_buffer_map_contract_test.c
	code/renderer/ral/ral_buffer_map.c
)
TARGET_INCLUDE_DIRECTORIES(ral_webgpu_buffer_map_contract_test PRIVATE
	code/qcommon
	code/renderer/ral
	code/renderercommon
)
ADD_TEST(NAME ral_webgpu_buffer_map_contract COMMAND ral_webgpu_buffer_map_contract_test)
SET_TESTS_PROPERTIES(ral_webgpu_buffer_map_contract PROPERTIES
	LABELS "host-only;subsystem;owner.ral;failure.contract;feature.always"
	TIMEOUT 30
)
WIRED_CONFIGURE_HOST_TEST(ral_webgpu_buffer_map_contract_test ral_webgpu_buffer_map_contract)
ADD_TEST(NAME ral_buffer_map_source_policy_contract
	COMMAND ${CMAKE_COMMAND} -DROOT=${CMAKE_CURRENT_SOURCE_DIR}
		-P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/ral_buffer_map_policy_check.cmake)
SET_TESTS_PROPERTIES(ral_buffer_map_source_policy_contract PROPERTIES
	LABELS "host-only;subsystem;owner.ral;failure.contract;feature.always"
	TIMEOUT 30
)

ADD_EXECUTABLE(ral_mrt_contract_test
	tests/ral_mrt_contract_test.c
	code/renderer/ral_vulkan/ral_vulkan_caps.c
	code/renderer/ral_vulkan/ral_vulkan_mrt_policy.c
	code/renderer/ral_vulkan/ral_vulkan_translate.c
)
TARGET_INCLUDE_DIRECTORIES(ral_mrt_contract_test PRIVATE
	code/qcommon
	code/renderer/ral
	code/renderer/ral_vulkan
	code/renderercommon
)
ADD_TEST(NAME ral_mrt_contract COMMAND ral_mrt_contract_test)
SET_TESTS_PROPERTIES(ral_mrt_contract PROPERTIES
	LABELS "host-only;subsystem;owner.ral;failure.contract;feature.always"
	TIMEOUT 30
)
WIRED_CONFIGURE_HOST_TEST(ral_mrt_contract_test ral_mrt_contract)

ADD_EXECUTABLE(vk_ral_attachment_translate_test
	tests/vk_ral_attachment_translate_test.c
	code/renderervk/vk_ral_attachment_translate.c
)
TARGET_INCLUDE_DIRECTORIES(vk_ral_attachment_translate_test PRIVATE
	code/qcommon
	code/renderer/ral
	code/renderercommon
	code/renderervk
)
ADD_TEST(NAME vk_ral_attachment_translate_contract
	COMMAND vk_ral_attachment_translate_test)
SET_TESTS_PROPERTIES(vk_ral_attachment_translate_contract PROPERTIES
	LABELS "host-only;subsystem;owner.renderer;owner.ral;failure.contract;feature.always"
	TIMEOUT 30)
WIRED_CONFIGURE_HOST_TEST(vk_ral_attachment_translate_test
	vk_ral_attachment_translate_contract)

ADD_TEST(NAME vk_ral_attachment_translate_source_policy_contract
	COMMAND ${CMAKE_COMMAND} -DROOT=${CMAKE_SOURCE_DIR}
		-P ${CMAKE_SOURCE_DIR}/cmake/vk_ral_attachment_translate_policy_check.cmake)
SET_TESTS_PROPERTIES(vk_ral_attachment_translate_source_policy_contract PROPERTIES
	LABELS "host-only;infrastructure;owner.renderer;owner.ral;failure.contract;feature.always"
	TIMEOUT 30)

ADD_TEST(NAME ral_mrt_source_policy_contract
	COMMAND ${CMAKE_COMMAND} -DROOT=${CMAKE_SOURCE_DIR}
		-P ${CMAKE_SOURCE_DIR}/cmake/ral_mrt_contract_policy_check.cmake)
SET_TESTS_PROPERTIES(ral_mrt_source_policy_contract PROPERTIES
	LABELS "host-only;infrastructure;owner.ral;failure.contract;feature.always"
	TIMEOUT 30)

ADD_EXECUTABLE(ral_swapchain_policy_test
	tests/ral_swapchain_policy_test.c
	code/renderer/ral_vulkan/ral_vulkan_swapchain.c
)

ADD_EXECUTABLE(vk_temporal_pipeline_cohort_test
	tests/vk_temporal_pipeline_cohort_test.c
	code/renderervk/vk_temporal_pipeline_cohort.c
)
TARGET_INCLUDE_DIRECTORIES(vk_temporal_pipeline_cohort_test PRIVATE
	code/qcommon
	code/renderer/ral
	code/renderervk
)
ADD_TEST(NAME vk_temporal_pipeline_cohort_contract
	COMMAND vk_temporal_pipeline_cohort_test)
SET_TESTS_PROPERTIES(vk_temporal_pipeline_cohort_contract PROPERTIES
	LABELS "host-only;subsystem;owner.renderer;owner.ral;failure.contract;feature.always"
	TIMEOUT 30)
WIRED_CONFIGURE_HOST_TEST(vk_temporal_pipeline_cohort_test
	vk_temporal_pipeline_cohort_contract)

ADD_EXECUTABLE(vk_temporal_shader_cohort_test
	tests/vk_temporal_shader_cohort_test.c
	code/renderervk/vk_temporal_shader_cohort.c
)
TARGET_INCLUDE_DIRECTORIES(vk_temporal_shader_cohort_test PRIVATE
	code/qcommon
	code/renderer/ral
	code/renderervk
)
ADD_TEST(NAME vk_temporal_shader_cohort_contract COMMAND vk_temporal_shader_cohort_test)
SET_TESTS_PROPERTIES(vk_temporal_shader_cohort_contract PROPERTIES
	LABELS "host-only;subsystem;owner.renderer;owner.ral;failure.contract;feature.always" TIMEOUT 30)
WIRED_CONFIGURE_HOST_TEST(vk_temporal_shader_cohort_test vk_temporal_shader_cohort_contract)

ADD_EXECUTABLE(vk_temporal_pipeline_factory_test
	tests/vk_temporal_pipeline_factory_test.c
	code/renderervk/vk_generic_specialization_contract.c
	code/renderer/ral/ral_legacy_material.c
	code/renderervk/vk_temporal_pipeline_factory.c
	code/renderervk/vk_temporal_generic_catalog.c
	code/renderervk/vk_temporal_shader_cohort.c)
TARGET_INCLUDE_DIRECTORIES(vk_temporal_pipeline_factory_test PRIVATE
	code/qcommon code/renderer/ral code/renderercommon code/renderervk)
TARGET_COMPILE_DEFINITIONS(vk_temporal_pipeline_factory_test PRIVATE
	WIRED_TEMPORAL_REPRESENTATIVE_FACTORY_TEST_ONLY=1)
ADD_TEST(NAME vk_temporal_pipeline_factory_contract COMMAND vk_temporal_pipeline_factory_test)
SET_TESTS_PROPERTIES(vk_temporal_pipeline_factory_contract PROPERTIES
	LABELS "host-only;subsystem;owner.renderer;owner.ral;failure.contract;feature.always" TIMEOUT 30)
WIRED_CONFIGURE_HOST_TEST(vk_temporal_pipeline_factory_test vk_temporal_pipeline_factory_contract)

ADD_EXECUTABLE(vk_generic_specialization_contract_test
	tests/vk_generic_specialization_contract_test.c
	code/renderervk/vk_generic_specialization_contract.c
	code/renderer/ral/ral_legacy_material.c)
TARGET_INCLUDE_DIRECTORIES(vk_generic_specialization_contract_test PRIVATE
	code/qcommon code/renderercommon code/renderervk)
ADD_TEST(NAME vk_generic_specialization_contract
	COMMAND vk_generic_specialization_contract_test)
SET_TESTS_PROPERTIES(vk_generic_specialization_contract PROPERTIES
	LABELS "host-only;subsystem;owner.renderer;failure.contract;feature.always" TIMEOUT 30)
WIRED_CONFIGURE_HOST_TEST(vk_generic_specialization_contract_test
	vk_generic_specialization_contract)

ADD_EXECUTABLE(ral_legacy_material_test
	tests/ral_legacy_material_test.c
	code/renderer/ral/ral_legacy_material.c)
TARGET_INCLUDE_DIRECTORIES(ral_legacy_material_test PRIVATE
	code/qcommon code/renderer/ral)
ADD_TEST(NAME ral_legacy_material_contract COMMAND ral_legacy_material_test)
SET_TESTS_PROPERTIES(ral_legacy_material_contract PROPERTIES
	LABELS "host-only;subsystem;owner.ral;failure.contract;feature.always" TIMEOUT 30)
WIRED_CONFIGURE_HOST_TEST(ral_legacy_material_test ral_legacy_material_contract)
ADD_TEST(NAME ral_legacy_material_source_policy_contract
	COMMAND ${CMAKE_COMMAND} -DROOT=${CMAKE_SOURCE_DIR}
		-P ${CMAKE_SOURCE_DIR}/cmake/ral_legacy_material_policy_check.cmake)
SET_TESTS_PROPERTIES(ral_legacy_material_source_policy_contract PROPERTIES
	LABELS "host-only;infrastructure;owner.ral;failure.contract;feature.always" TIMEOUT 30)

ADD_TEST(NAME vk_generic_specialization_source_policy_contract
	COMMAND ${CMAKE_COMMAND} -DROOT=${CMAKE_SOURCE_DIR}
		-P ${CMAKE_SOURCE_DIR}/cmake/vk_generic_specialization_contract_policy_check.cmake)
SET_TESTS_PROPERTIES(vk_generic_specialization_source_policy_contract PROPERTIES
	LABELS "host-only;infrastructure;owner.renderer;failure.contract;feature.always" TIMEOUT 30)

ADD_TEST(NAME vk_temporal_pipeline_factory_source_policy_contract
	COMMAND ${CMAKE_COMMAND} -DROOT=${CMAKE_SOURCE_DIR}
		-P ${CMAKE_SOURCE_DIR}/cmake/vk_temporal_pipeline_factory_policy_check.cmake)
SET_TESTS_PROPERTIES(vk_temporal_pipeline_factory_source_policy_contract PROPERTIES
	LABELS "host-only;infrastructure;owner.renderer;owner.ral;failure.contract;feature.always" TIMEOUT 30)

ADD_TEST(NAME vk_temporal_shader_reflection_contract
	COMMAND sh ${CMAKE_SOURCE_DIR}/tests/vk-temporal-shader-check.sh ${CMAKE_SOURCE_DIR})
SET_TESTS_PROPERTIES(vk_temporal_shader_reflection_contract PROPERTIES
	LABELS "host-only;infrastructure;owner.renderer;owner.ral;failure.contract;feature.always"
	SKIP_RETURN_CODE 77 TIMEOUT 180)

ADD_TEST(NAME vk_temporal_shader_source_policy_contract
	COMMAND ${CMAKE_COMMAND} -DROOT=${CMAKE_SOURCE_DIR}
		-P ${CMAKE_SOURCE_DIR}/cmake/vk_temporal_shader_policy_check.cmake)
SET_TESTS_PROPERTIES(vk_temporal_shader_source_policy_contract PROPERTIES
	LABELS "host-only;infrastructure;owner.renderer;owner.ral;failure.contract;feature.always" TIMEOUT 30)

ADD_EXECUTABLE(vk_temporal_generic_catalog_test
	tests/vk_temporal_generic_catalog_test.c
	code/renderervk/vk_temporal_generic_catalog.c)
TARGET_INCLUDE_DIRECTORIES(vk_temporal_generic_catalog_test PRIVATE
	code/qcommon code/renderer/ral code/renderervk)
ADD_TEST(NAME vk_temporal_generic_catalog_contract COMMAND vk_temporal_generic_catalog_test)
SET_TESTS_PROPERTIES(vk_temporal_generic_catalog_contract PROPERTIES
	LABELS "host-only;subsystem;owner.renderer;owner.ral;failure.contract;feature.always" TIMEOUT 30)
WIRED_CONFIGURE_HOST_TEST(vk_temporal_generic_catalog_test vk_temporal_generic_catalog_contract)

ADD_EXECUTABLE(vk_temporal_generic_recipe_table_test
	tests/vk_temporal_generic_recipe_table_test.c
	code/renderervk/vk_temporal_generic_recipe_table.c
	code/renderervk/vk_generic_specialization_contract.c
	code/renderer/ral/ral_legacy_material.c
	code/renderervk/vk_temporal_generic_catalog.c)
TARGET_INCLUDE_DIRECTORIES(vk_temporal_generic_recipe_table_test PRIVATE
	code/qcommon code/renderer/ral code/renderercommon code/renderervk)
ADD_TEST(NAME vk_temporal_generic_recipe_table_contract
	COMMAND vk_temporal_generic_recipe_table_test)
SET_TESTS_PROPERTIES(vk_temporal_generic_recipe_table_contract PROPERTIES
	LABELS "host-only;subsystem;owner.renderer;failure.contract;feature.always" TIMEOUT 30)
WIRED_CONFIGURE_HOST_TEST(vk_temporal_generic_recipe_table_test
	vk_temporal_generic_recipe_table_contract)

ADD_EXECUTABLE(vk_temporal_generic_pipeline_table_test
	tests/vk_temporal_generic_pipeline_table_test.c
	code/renderervk/vk_temporal_generic_pipeline_table.c
	code/renderervk/vk_temporal_generic_recipe_table.c
	code/renderervk/vk_temporal_pipeline_factory.c
	code/renderervk/vk_temporal_shader_cohort.c
	code/renderervk/vk_temporal_generic_catalog.c
	code/renderervk/vk_generic_specialization_contract.c
	code/renderer/ral/ral_legacy_material.c)
TARGET_INCLUDE_DIRECTORIES(vk_temporal_generic_pipeline_table_test PRIVATE
	code/qcommon code/renderer/ral code/renderercommon code/renderervk)
ADD_TEST(NAME vk_temporal_generic_pipeline_table_contract
	COMMAND vk_temporal_generic_pipeline_table_test)
SET_TESTS_PROPERTIES(vk_temporal_generic_pipeline_table_contract PROPERTIES
	LABELS "host-only;subsystem;owner.renderer;owner.ral;failure.contract;feature.always"
	TIMEOUT 30)
WIRED_CONFIGURE_HOST_TEST(vk_temporal_generic_pipeline_table_test
	vk_temporal_generic_pipeline_table_contract)
ADD_TEST(NAME vk_temporal_generic_pipeline_table_source_policy_contract
	COMMAND ${CMAKE_COMMAND} -DROOT=${CMAKE_SOURCE_DIR}
		-P ${CMAKE_SOURCE_DIR}/cmake/vk_temporal_generic_pipeline_table_policy_check.cmake)
SET_TESTS_PROPERTIES(vk_temporal_generic_pipeline_table_source_policy_contract PROPERTIES
	LABELS "host-only;infrastructure;owner.renderer;owner.ral;failure.contract;feature.always"
	TIMEOUT 30)

ADD_TEST(NAME vk_temporal_generic_recipe_table_source_policy_contract
	COMMAND ${CMAKE_COMMAND} -DROOT=${CMAKE_SOURCE_DIR}
		-P ${CMAKE_SOURCE_DIR}/cmake/vk_temporal_generic_recipe_table_policy_check.cmake)
SET_TESTS_PROPERTIES(vk_temporal_generic_recipe_table_source_policy_contract PROPERTIES
	LABELS "host-only;infrastructure;owner.renderer;failure.contract;feature.always" TIMEOUT 30)
TARGET_INCLUDE_DIRECTORIES(ral_swapchain_policy_test PRIVATE
	code/qcommon
	code/renderer/ral
	code/renderer/ral_vulkan
	code/renderercommon
)
ADD_TEST(NAME ral_swapchain_policy_contract COMMAND ral_swapchain_policy_test)
SET_TESTS_PROPERTIES(ral_swapchain_policy_contract PROPERTIES
	LABELS "host-only;subsystem;owner.ral;failure.contract;feature.always"
	TIMEOUT 30
)
WIRED_CONFIGURE_HOST_TEST(ral_swapchain_policy_test ral_swapchain_policy_contract)

ADD_EXECUTABLE(ral_submission_lifecycle_test
	tests/ral_submission_lifecycle_test.c
)
TARGET_INCLUDE_DIRECTORIES(ral_submission_lifecycle_test PRIVATE
	code/qcommon
	code/renderer/ral
	code/renderer/ral_vulkan
	code/renderercommon
)
TARGET_LINK_LIBRARIES(ral_submission_lifecycle_test PRIVATE ral_vulkan)
ADD_TEST(NAME ral_submission_lifecycle_contract COMMAND ral_submission_lifecycle_test)
SET_TESTS_PROPERTIES(ral_submission_lifecycle_contract PROPERTIES
	LABELS "host-only;subsystem;owner.ral;failure.contract;feature.always"
	TIMEOUT 30
)
WIRED_CONFIGURE_HOST_TEST(ral_submission_lifecycle_test ral_submission_lifecycle_contract)

ADD_EXECUTABLE(wired_profile_host_lifecycle_test
	tests/wired_profile_host_lifecycle_test.cpp
	code/tools/profile_imgui/wired_profile_host_lifecycle.cpp
)
TARGET_INCLUDE_DIRECTORIES(wired_profile_host_lifecycle_test PRIVATE
	code/tools/profile_imgui
	code/renderer/ral
)
ADD_TEST(NAME wired_profile_host_lifecycle_contract COMMAND wired_profile_host_lifecycle_test)
SET_TESTS_PROPERTIES(wired_profile_host_lifecycle_contract PROPERTIES
	LABELS "host-only;subsystem;owner.ral;owner.tools;failure.contract;feature.imgui"
	TIMEOUT 30
)
WIRED_CONFIGURE_HOST_TEST(wired_profile_host_lifecycle_test wired_profile_host_lifecycle_contract)

ADD_EXECUTABLE(ral_host_import_test
	tests/ral_host_import_test.c
	code/renderer/ral/ral_host.c
)
TARGET_INCLUDE_DIRECTORIES(ral_host_import_test PRIVATE
	code/qcommon
	code/renderer/ral
)
ADD_TEST(NAME ral_host_import_contract COMMAND ral_host_import_test)
SET_TESTS_PROPERTIES(ral_host_import_contract PROPERTIES
	LABELS "host-only;subsystem;owner.ral;failure.contract;feature.always"
	TIMEOUT 30
)
WIRED_CONFIGURE_HOST_TEST(ral_host_import_test ral_host_import_contract)

ADD_EXECUTABLE(ral_residency_policy_test
	tests/ral_residency_policy_test.c
	code/renderer/ral/ral_residency.c
)
TARGET_INCLUDE_DIRECTORIES(ral_residency_policy_test PRIVATE
	code/renderer/ral
)
ADD_TEST(NAME ral_residency_policy_contract COMMAND ral_residency_policy_test)
SET_TESTS_PROPERTIES(ral_residency_policy_contract PROPERTIES
	LABELS "host-only;subsystem;owner.ral;failure.contract;feature.always"
	TIMEOUT 30
)
WIRED_CONFIGURE_HOST_TEST(ral_residency_policy_test ral_residency_policy_contract)

ADD_EXECUTABLE(ral_profile_test
	tests/ral_profile_test.c
	code/renderer/ral/ral_profile.c
)
TARGET_INCLUDE_DIRECTORIES(ral_profile_test PRIVATE
	code/renderer/ral
)
ADD_TEST(NAME ral_profile_contract COMMAND ral_profile_test)
SET_TESTS_PROPERTIES(ral_profile_contract PROPERTIES
	LABELS "host-only;subsystem;owner.ral;failure.contract;feature.always"
	TIMEOUT 30
)
WIRED_CONFIGURE_HOST_TEST(ral_profile_test ral_profile_contract)

ADD_EXECUTABLE(ral_temporal_test
	tests/ral_temporal_test.c
	code/renderer/ral/ral_temporal.c
	code/renderervk/tr_temporal_projection.c
	code/renderervk/tr_temporal_history.c
)
TARGET_INCLUDE_DIRECTORIES(ral_temporal_test PRIVATE
	code/renderer/ral
	code/renderervk
)
ADD_TEST(NAME ral_temporal_contract COMMAND ral_temporal_test)
SET_TESTS_PROPERTIES(ral_temporal_contract PROPERTIES
	LABELS "host-only;subsystem;owner.ral;failure.contract;feature.always"
	TIMEOUT 30
)
WIRED_CONFIGURE_HOST_TEST(ral_temporal_test ral_temporal_contract)

ADD_EXECUTABLE(tr_temporal_motion_targets_test
	tests/tr_temporal_motion_targets_test.c
	code/renderervk/tr_temporal_motion_targets.c
)
TARGET_INCLUDE_DIRECTORIES(tr_temporal_motion_targets_test PRIVATE
	code/qcommon
	code/renderer/ral
	code/renderervk
)
ADD_TEST(NAME tr_temporal_motion_targets_contract
	COMMAND tr_temporal_motion_targets_test)
SET_TESTS_PROPERTIES(tr_temporal_motion_targets_contract PROPERTIES
	LABELS "host-only;subsystem;owner.renderer;owner.ral;failure.contract;feature.always"
	TIMEOUT 30)
WIRED_CONFIGURE_HOST_TEST(tr_temporal_motion_targets_test
	tr_temporal_motion_targets_contract)

ADD_EXECUTABLE(tr_temporal_motion_payload_test
	tests/tr_temporal_motion_payload_test.c
	code/renderervk/tr_temporal_motion_payload.c
)
TARGET_INCLUDE_DIRECTORIES(tr_temporal_motion_payload_test PRIVATE
	code/qcommon
	code/renderer/ral
	code/renderercommon
	code/renderervk
)
ADD_TEST(NAME tr_temporal_motion_payload_contract
	COMMAND tr_temporal_motion_payload_test)
SET_TESTS_PROPERTIES(tr_temporal_motion_payload_contract PROPERTIES
	LABELS "host-only;subsystem;owner.renderer;owner.ral;failure.contract;feature.always"
	TIMEOUT 30)
WIRED_CONFIGURE_HOST_TEST(tr_temporal_motion_payload_test
	tr_temporal_motion_payload_contract)

ADD_TEST(NAME tr_temporal_motion_payload_source_policy_contract
	COMMAND ${CMAKE_COMMAND} -DROOT=${CMAKE_SOURCE_DIR}
		-P ${CMAKE_SOURCE_DIR}/cmake/tr_temporal_motion_payload_policy_check.cmake)
SET_TESTS_PROPERTIES(tr_temporal_motion_payload_source_policy_contract PROPERTIES
	LABELS "host-only;infrastructure;owner.renderer;owner.ral;failure.contract;feature.always"
	TIMEOUT 30)

ADD_EXECUTABLE(vk_temporal_entmat_adoption_test
	tests/vk_temporal_entmat_adoption_test.c
	code/renderervk/vk_temporal_entmat_adoption.c
)
TARGET_INCLUDE_DIRECTORIES(vk_temporal_entmat_adoption_test PRIVATE
	code/qcommon
	code/renderer/ral
	code/renderervk
)
ADD_TEST(NAME vk_temporal_entmat_adoption_contract
	COMMAND vk_temporal_entmat_adoption_test)
SET_TESTS_PROPERTIES(vk_temporal_entmat_adoption_contract PROPERTIES
	LABELS "host-only;subsystem;owner.renderer;owner.ral;failure.contract;feature.always"
	TIMEOUT 30)
WIRED_CONFIGURE_HOST_TEST(vk_temporal_entmat_adoption_test
	vk_temporal_entmat_adoption_contract)

ADD_EXECUTABLE(tr_temporal_batch_request_test
	tests/tr_temporal_batch_request_test.c
	code/renderervk/tr_temporal_batch_request.c
)
TARGET_INCLUDE_DIRECTORIES(tr_temporal_batch_request_test PRIVATE
	code/qcommon
	code/renderervk
)
ADD_TEST(NAME tr_temporal_batch_request_contract
	COMMAND tr_temporal_batch_request_test)
SET_TESTS_PROPERTIES(tr_temporal_batch_request_contract PROPERTIES
	LABELS "host-only;subsystem;owner.renderer;failure.contract;feature.always"
	TIMEOUT 30)
WIRED_CONFIGURE_HOST_TEST(tr_temporal_batch_request_test
	tr_temporal_batch_request_contract)

ADD_EXECUTABLE(vk_temporal_entmat_runtime_test
	tests/vk_temporal_entmat_runtime_test.c
	code/renderervk/vk_temporal_entmat_runtime.c
	code/renderervk/vk_temporal_entmat_adoption.c
	code/renderervk/tr_temporal_motion_payload.c
)
TARGET_INCLUDE_DIRECTORIES(vk_temporal_entmat_runtime_test PRIVATE
	code/qcommon
	code/renderer/ral
	code/renderercommon
	code/renderervk
)
ADD_TEST(NAME vk_temporal_entmat_runtime_contract
	COMMAND vk_temporal_entmat_runtime_test)
SET_TESTS_PROPERTIES(vk_temporal_entmat_runtime_contract PROPERTIES
	LABELS "host-only;subsystem;owner.renderer;owner.ral;failure.contract;feature.always"
	TIMEOUT 30)
WIRED_CONFIGURE_HOST_TEST(vk_temporal_entmat_runtime_test
	vk_temporal_entmat_runtime_contract)

ADD_EXECUTABLE(vk_temporal_motion_recording_test
	tests/vk_temporal_motion_recording_test.c
	code/renderervk/vk_temporal_motion_sequence.c
	code/renderervk/vk_temporal_motion_recording.c)
TARGET_INCLUDE_DIRECTORIES(vk_temporal_motion_recording_test PRIVATE
	code/qcommon code/renderer/ral code/renderercommon code/renderervk)
ADD_TEST(NAME vk_temporal_motion_recording_contract
	COMMAND vk_temporal_motion_recording_test)
SET_TESTS_PROPERTIES(vk_temporal_motion_recording_contract PROPERTIES
	LABELS "host-only;subsystem;owner.renderer;owner.ral;failure.contract;feature.always"
	TIMEOUT 30)
WIRED_CONFIGURE_HOST_TEST(vk_temporal_motion_recording_test
	vk_temporal_motion_recording_contract)

ADD_EXECUTABLE(vk_temporal_motion_sequence_test
	tests/vk_temporal_motion_sequence_test.c
	code/renderervk/vk_temporal_motion_sequence.c)
TARGET_INCLUDE_DIRECTORIES(vk_temporal_motion_sequence_test PRIVATE
	code/qcommon code/renderer/ral code/renderercommon code/renderervk)
ADD_TEST(NAME vk_temporal_motion_sequence_contract
	COMMAND vk_temporal_motion_sequence_test)
SET_TESTS_PROPERTIES(vk_temporal_motion_sequence_contract PROPERTIES
	LABELS "host-only;subsystem;owner.renderer;owner.ral;failure.contract;feature.always"
	TIMEOUT 30)
WIRED_CONFIGURE_HOST_TEST(vk_temporal_motion_sequence_test
	vk_temporal_motion_sequence_contract)
ADD_TEST(NAME vk_temporal_motion_recording_source_policy_contract
	COMMAND ${CMAKE_COMMAND} -DROOT=${CMAKE_SOURCE_DIR}
		-P ${CMAKE_SOURCE_DIR}/cmake/vk_temporal_motion_recording_policy_check.cmake)
SET_TESTS_PROPERTIES(vk_temporal_motion_recording_source_policy_contract PROPERTIES
	LABELS "host-only;infrastructure;owner.renderer;owner.ral;failure.contract;feature.always"
	TIMEOUT 30)

ADD_EXECUTABLE(vk_temporal_main_activation_test
	tests/vk_temporal_main_activation_test.c
	code/renderervk/vk_temporal_main_activation.c
	code/renderervk/vk_temporal_motion_sequence.c
	code/renderervk/vk_temporal_iqm_payload_authoring.c
	code/renderervk/vk_temporal_iqm_payload.c
	code/renderervk/tr_temporal_iqm_motion.c
	code/renderervk/tr_temporal_motion.c
	code/renderervk/vk_bindless_publication.c)
TARGET_INCLUDE_DIRECTORIES(vk_temporal_main_activation_test PRIVATE
	code/qcommon code/renderer/ral code/renderercommon code/renderervk)
# tr_temporal_iqm_motion.c calls acosf/sinf. On macOS libm lives inside libc so
# this links without asking; on Linux it does not, and the target failed there
# with "undefined reference to `acosf'" while building clean locally. Same
# IF(NOT WIN32) guard the other math-using test targets use.
IF(NOT WIN32)
	TARGET_LINK_LIBRARIES(vk_temporal_main_activation_test m)
ENDIF()
ADD_TEST(NAME vk_temporal_main_activation_contract
	COMMAND vk_temporal_main_activation_test)
SET_TESTS_PROPERTIES(vk_temporal_main_activation_contract PROPERTIES
	LABELS "host-only;subsystem;owner.renderer;owner.ral;failure.contract;feature.always"
	TIMEOUT 30)
WIRED_CONFIGURE_HOST_TEST(vk_temporal_main_activation_test
	vk_temporal_main_activation_contract)

ADD_EXECUTABLE(vk_temporal_motion_readback_test
	tests/vk_temporal_motion_readback_test.c
	code/renderervk/vk_temporal_motion_readback.c
	code/renderervk/vk_temporal_motion_sequence.c)
TARGET_INCLUDE_DIRECTORIES(vk_temporal_motion_readback_test PRIVATE
	code/qcommon code/renderer/ral code/renderercommon code/renderervk)
ADD_TEST(NAME vk_temporal_motion_readback_contract
	COMMAND vk_temporal_motion_readback_test)
SET_TESTS_PROPERTIES(vk_temporal_motion_readback_contract PROPERTIES
	LABELS "host-only;subsystem;owner.renderer;owner.ral;failure.contract;feature.always"
	TIMEOUT 30)
WIRED_CONFIGURE_HOST_TEST(vk_temporal_motion_readback_test
	vk_temporal_motion_readback_contract)

ADD_EXECUTABLE(vk_temporal_history_consume_test
	tests/vk_temporal_history_consume_test.c
	code/renderervk/vk_temporal_history_consume.c)
TARGET_INCLUDE_DIRECTORIES(vk_temporal_history_consume_test PRIVATE
	code/qcommon code/renderer/ral code/renderercommon code/renderervk)
ADD_TEST(NAME vk_temporal_history_consume_contract
	COMMAND vk_temporal_history_consume_test)
SET_TESTS_PROPERTIES(vk_temporal_history_consume_contract PROPERTIES
	LABELS "host-only;subsystem;owner.renderer;owner.ral;failure.contract;feature.always"
	TIMEOUT 30)
WIRED_CONFIGURE_HOST_TEST(vk_temporal_history_consume_test
	vk_temporal_history_consume_contract)

ADD_EXECUTABLE(vk_temporal_history_store_test
	tests/vk_temporal_history_store_test.c
	code/renderervk/vk_temporal_history_store.c)
TARGET_INCLUDE_DIRECTORIES(vk_temporal_history_store_test PRIVATE
	code/qcommon code/renderer code/renderer/ral code/renderercommon code/renderervk)
ADD_TEST(NAME vk_temporal_history_store_contract
	COMMAND vk_temporal_history_store_test)
SET_TESTS_PROPERTIES(vk_temporal_history_store_contract PROPERTIES
	LABELS "host-only;subsystem;owner.renderer;owner.ral;failure.contract;feature.always"
	TIMEOUT 30)
WIRED_CONFIGURE_HOST_TEST(vk_temporal_history_store_test
	vk_temporal_history_store_contract)
ADD_EXECUTABLE(tr_temporal_submit_test
	tests/tr_temporal_submit_test.c
	code/renderervk/tr_temporal_submit.c
	code/renderervk/tr_temporal_batch_request.c)
TARGET_INCLUDE_DIRECTORIES(tr_temporal_submit_test PRIVATE
	code/qcommon code/renderer code/renderer/ral code/renderercommon code/renderervk)
ADD_TEST(NAME tr_temporal_submit_contract COMMAND tr_temporal_submit_test)
SET_TESTS_PROPERTIES(tr_temporal_submit_contract PROPERTIES
	LABELS "host-only;subsystem;owner.renderer;owner.ral;failure.contract;feature.always"
	TIMEOUT 30)
WIRED_CONFIGURE_HOST_TEST(tr_temporal_submit_test tr_temporal_submit_contract)
ADD_TEST(NAME vk_temporal_history_store_shader_reflection_contract
	COMMAND bash ${CMAKE_SOURCE_DIR}/tests/vk-temporal-history-store-shader-check.sh
		${CMAKE_SOURCE_DIR})
SET_TESTS_PROPERTIES(vk_temporal_history_store_shader_reflection_contract PROPERTIES
	LABELS "host-only;subsystem;owner.renderer;owner.ral;failure.contract;feature.always"
	SKIP_RETURN_CODE 77 TIMEOUT 60)
ADD_TEST(NAME vk_temporal_history_consume_source_policy_contract
	COMMAND ${CMAKE_COMMAND} -DROOT=${CMAKE_SOURCE_DIR}
		-P ${CMAKE_SOURCE_DIR}/cmake/vk_temporal_history_consume_policy_check.cmake)
SET_TESTS_PROPERTIES(vk_temporal_history_consume_source_policy_contract PROPERTIES
	LABELS "host-only;infrastructure;owner.renderer;owner.ral;failure.contract;feature.always"
	TIMEOUT 30)

ADD_TEST(NAME vk_temporal_motion_readback_source_policy_contract
	COMMAND ${CMAKE_COMMAND} -DROOT=${CMAKE_SOURCE_DIR}
		-P ${CMAKE_SOURCE_DIR}/cmake/vk_temporal_motion_readback_policy_check.cmake)
SET_TESTS_PROPERTIES(vk_temporal_motion_readback_source_policy_contract PROPERTIES
	LABELS "host-only;infrastructure;owner.renderer;owner.ral;failure.contract;feature.always"
	TIMEOUT 30)

ADD_EXECUTABLE(vk_temporal_main_rendering_test
	tests/vk_temporal_main_rendering_test.c
	code/renderervk/vk_temporal_main_rendering.c)
TARGET_INCLUDE_DIRECTORIES(vk_temporal_main_rendering_test PRIVATE
	code/qcommon code/renderer/ral code/renderercommon code/renderervk)
ADD_TEST(NAME vk_temporal_main_rendering_contract
	COMMAND vk_temporal_main_rendering_test)
SET_TESTS_PROPERTIES(vk_temporal_main_rendering_contract PROPERTIES
	LABELS "host-only;subsystem;owner.renderer;owner.ral;failure.contract;feature.always"
	TIMEOUT 30)
WIRED_CONFIGURE_HOST_TEST(vk_temporal_main_rendering_test
	vk_temporal_main_rendering_contract)
ADD_TEST(NAME vk_temporal_main_activation_source_policy_contract
	COMMAND ${CMAKE_COMMAND} -DROOT=${CMAKE_SOURCE_DIR}
		-P ${CMAKE_SOURCE_DIR}/cmake/vk_temporal_main_activation_policy_check.cmake)
SET_TESTS_PROPERTIES(vk_temporal_main_activation_source_policy_contract PROPERTIES
	LABELS "host-only;infrastructure;owner.renderer;owner.ral;failure.contract;feature.always"
	TIMEOUT 30)
ADD_TEST(NAME vk_temporal_main_rendering_source_policy_contract
	COMMAND ${CMAKE_COMMAND} -DROOT=${CMAKE_SOURCE_DIR}
		-P ${CMAKE_SOURCE_DIR}/cmake/vk_temporal_main_rendering_policy_check.cmake)
SET_TESTS_PROPERTIES(vk_temporal_main_rendering_source_policy_contract PROPERTIES
	LABELS "host-only;infrastructure;owner.renderer;owner.ral;failure.contract;feature.always"
	TIMEOUT 30)

ADD_EXECUTABLE(vk_temporal_motion_materialization_test
	tests/vk_temporal_motion_materialization_test.c
	code/renderervk/vk_temporal_motion_materialization.c
	code/renderervk/tr_temporal_motion_targets.c
)
TARGET_INCLUDE_DIRECTORIES(vk_temporal_motion_materialization_test PRIVATE
	code/qcommon
	code/renderer/ral
	code/renderercommon
	code/renderervk
)
ADD_TEST(NAME vk_temporal_motion_materialization_contract
	COMMAND vk_temporal_motion_materialization_test)
SET_TESTS_PROPERTIES(vk_temporal_motion_materialization_contract PROPERTIES
	LABELS "host-only;subsystem;owner.renderer;owner.ral;failure.contract;feature.always"
	TIMEOUT 30)
WIRED_CONFIGURE_HOST_TEST(vk_temporal_motion_materialization_test
	vk_temporal_motion_materialization_contract)

ADD_TEST(NAME vk_temporal_motion_materialization_source_policy_contract
	COMMAND ${CMAKE_COMMAND}
		-DROOT=${CMAKE_CURRENT_SOURCE_DIR}
		-P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/vk_temporal_motion_materialization_policy_check.cmake)
SET_TESTS_PROPERTIES(vk_temporal_motion_materialization_source_policy_contract PROPERTIES
	LABELS "host-only;infrastructure;owner.renderer;owner.ral;failure.contract;feature.always"
	TIMEOUT 30)

ADD_EXECUTABLE(vk_temporal_resolved_hdr_test
	tests/vk_temporal_resolved_hdr_test.c
	code/renderervk/vk_temporal_resolved_hdr.c)
TARGET_INCLUDE_DIRECTORIES(vk_temporal_resolved_hdr_test PRIVATE
	code/qcommon code/renderer/ral code/renderercommon code/renderervk)
ADD_TEST(NAME vk_temporal_resolved_hdr_contract
	COMMAND vk_temporal_resolved_hdr_test)
SET_TESTS_PROPERTIES(vk_temporal_resolved_hdr_contract PROPERTIES
	LABELS "host-only;subsystem;owner.renderer;owner.ral;failure.contract;feature.always"
	TIMEOUT 30)
WIRED_CONFIGURE_HOST_TEST(vk_temporal_resolved_hdr_test
	vk_temporal_resolved_hdr_contract)
ADD_TEST(NAME vk_temporal_resolved_hdr_source_policy_contract
	COMMAND ${CMAKE_COMMAND} -DROOT=${CMAKE_SOURCE_DIR}
		-P ${CMAKE_SOURCE_DIR}/cmake/vk_temporal_resolved_hdr_policy_check.cmake)
SET_TESTS_PROPERTIES(vk_temporal_resolved_hdr_source_policy_contract PROPERTIES
	LABELS "host-only;infrastructure;owner.renderer;owner.ral;failure.contract;feature.always"
	TIMEOUT 30)

ADD_EXECUTABLE(vk_temporal_resolve_authority_test
	tests/vk_temporal_resolve_authority_test.c
	code/renderervk/vk_temporal_resolve_authority.c)
TARGET_INCLUDE_DIRECTORIES(vk_temporal_resolve_authority_test PRIVATE
	code/qcommon code/renderer/ral code/renderercommon code/renderervk)
ADD_TEST(NAME vk_temporal_resolve_authority_contract
	COMMAND vk_temporal_resolve_authority_test)
SET_TESTS_PROPERTIES(vk_temporal_resolve_authority_contract PROPERTIES
	LABELS "host-only;subsystem;owner.renderer;owner.ral;failure.contract;feature.always"
	TIMEOUT 30)
WIRED_CONFIGURE_HOST_TEST(vk_temporal_resolve_authority_test
	vk_temporal_resolve_authority_contract)
ADD_TEST(NAME vk_temporal_resolve_authority_source_policy_contract
	COMMAND ${CMAKE_COMMAND} -DROOT=${CMAKE_SOURCE_DIR}
		-P ${CMAKE_SOURCE_DIR}/cmake/vk_temporal_resolve_authority_policy_check.cmake)
SET_TESTS_PROPERTIES(vk_temporal_resolve_authority_source_policy_contract PROPERTIES
	LABELS "host-only;infrastructure;owner.renderer;owner.ral;failure.contract;feature.always"
	TIMEOUT 30)

ADD_EXECUTABLE(vk_temporal_resolve_test
	tests/vk_temporal_resolve_test.c
	code/renderervk/vk_temporal_resolve.c
	code/renderervk/vk_temporal_resolved_hdr.c)
TARGET_INCLUDE_DIRECTORIES(vk_temporal_resolve_test PRIVATE
	code/qcommon code/renderer code/renderer/ral code/renderercommon code/renderervk)
ADD_TEST(NAME vk_temporal_resolve_contract COMMAND vk_temporal_resolve_test)
SET_TESTS_PROPERTIES(vk_temporal_resolve_contract PROPERTIES
	LABELS "host-only;subsystem;owner.renderer;owner.ral;failure.contract;feature.always"
	TIMEOUT 30)
WIRED_CONFIGURE_HOST_TEST(vk_temporal_resolve_test vk_temporal_resolve_contract)
ADD_EXECUTABLE(vk_temporal_resolve_readback_test
	tests/vk_temporal_resolve_readback_test.c
	code/renderervk/vk_temporal_resolve_readback.c)
TARGET_INCLUDE_DIRECTORIES(vk_temporal_resolve_readback_test PRIVATE
	code/qcommon code/renderer code/renderer/ral code/renderercommon code/renderervk)
ADD_TEST(NAME vk_temporal_resolve_readback_contract
	COMMAND vk_temporal_resolve_readback_test)
SET_TESTS_PROPERTIES(vk_temporal_resolve_readback_contract PROPERTIES
	LABELS "host-only;subsystem;owner.renderer;owner.ral;failure.contract;feature.always"
	TIMEOUT 30)
WIRED_CONFIGURE_HOST_TEST(vk_temporal_resolve_readback_test
	vk_temporal_resolve_readback_contract)
ADD_TEST(NAME vk_temporal_resolve_readback_source_policy_contract
	COMMAND ${CMAKE_COMMAND} -DROOT=${CMAKE_SOURCE_DIR}
		-P ${CMAKE_SOURCE_DIR}/cmake/vk_temporal_resolve_readback_policy_check.cmake)
SET_TESTS_PROPERTIES(vk_temporal_resolve_readback_source_policy_contract PROPERTIES
	LABELS "host-only;infrastructure;owner.renderer;owner.ral;failure.contract;feature.always"
	TIMEOUT 30)
ADD_TEST(NAME vk_temporal_resolve_source_policy_contract
	COMMAND ${CMAKE_COMMAND} -DROOT=${CMAKE_SOURCE_DIR}
		-P ${CMAKE_SOURCE_DIR}/cmake/vk_temporal_resolve_policy_check.cmake)
SET_TESTS_PROPERTIES(vk_temporal_resolve_source_policy_contract PROPERTIES
	LABELS "host-only;infrastructure;owner.renderer;owner.ral;failure.contract;feature.always"
	TIMEOUT 30)
ADD_TEST(NAME vk_temporal_resolve_shader_reflection_contract
	COMMAND bash ${CMAKE_SOURCE_DIR}/tests/vk-temporal-resolve-shader-check.sh
		${CMAKE_SOURCE_DIR})
SET_TESTS_PROPERTIES(vk_temporal_resolve_shader_reflection_contract PROPERTIES
	LABELS "host-only;subsystem;owner.renderer;owner.ral;failure.contract;feature.always"
	SKIP_RETURN_CODE 77 TIMEOUT 60)

ADD_TEST(NAME vk_temporal_entmat_runtime_source_policy_contract
	COMMAND ${CMAKE_COMMAND} -DROOT=${CMAKE_SOURCE_DIR}
		-P ${CMAKE_SOURCE_DIR}/cmake/vk_temporal_entmat_runtime_policy_check.cmake)
SET_TESTS_PROPERTIES(vk_temporal_entmat_runtime_source_policy_contract PROPERTIES
	LABELS "host-only;infrastructure;owner.renderer;owner.ral;failure.contract;feature.always"
	TIMEOUT 30)

ADD_TEST(NAME vk_temporal_entmat_adoption_source_policy_contract
	COMMAND ${CMAKE_COMMAND} -DROOT=${CMAKE_SOURCE_DIR}
		-P ${CMAKE_SOURCE_DIR}/cmake/vk_temporal_entmat_adoption_policy_check.cmake)
SET_TESTS_PROPERTIES(vk_temporal_entmat_adoption_source_policy_contract PROPERTIES
	LABELS "host-only;infrastructure;owner.renderer;owner.ral;failure.contract;feature.always"
	TIMEOUT 30)

ADD_TEST(NAME tr_temporal_motion_structural_source_policy_contract
	COMMAND ${CMAKE_COMMAND} -DROOT=${CMAKE_SOURCE_DIR}
		-P ${CMAKE_SOURCE_DIR}/cmake/tr_temporal_motion_structural_policy_check.cmake)
SET_TESTS_PROPERTIES(tr_temporal_motion_structural_source_policy_contract PROPERTIES
	LABELS "host-only;infrastructure;owner.renderer;owner.ral;failure.contract;feature.always"
	TIMEOUT 30)

ADD_EXECUTABLE(ral_temporal_entity_cache_test
	tests/ral_temporal_entity_cache_test.c
	code/renderervk/tr_temporal_entity_cache.c
)
TARGET_INCLUDE_DIRECTORIES(ral_temporal_entity_cache_test PRIVATE
	code/qcommon
	code/renderercommon
	code/renderervk
)
ADD_TEST(NAME ral_temporal_entity_cache_contract
	COMMAND ral_temporal_entity_cache_test)
SET_TESTS_PROPERTIES(ral_temporal_entity_cache_contract PROPERTIES
	LABELS "host-only;subsystem;owner.renderer;failure.contract;feature.always"
	TIMEOUT 30)
WIRED_CONFIGURE_HOST_TEST(ral_temporal_entity_cache_test
	ral_temporal_entity_cache_contract)

ADD_EXECUTABLE(tr_temporal_motion_test
	tests/tr_temporal_motion_test.c
	code/renderervk/tr_temporal_motion.c
)
TARGET_INCLUDE_DIRECTORIES(tr_temporal_motion_test PRIVATE
	code/qcommon
	code/renderercommon
	code/renderervk
)
ADD_TEST(NAME tr_temporal_motion_contract
	COMMAND tr_temporal_motion_test)
SET_TESTS_PROPERTIES(tr_temporal_motion_contract PROPERTIES
	LABELS "host-only;subsystem;owner.renderer;failure.contract;feature.always"
	TIMEOUT 30)
WIRED_CONFIGURE_HOST_TEST(tr_temporal_motion_test
	tr_temporal_motion_contract)

ADD_EXECUTABLE(tr_temporal_iqm_motion_test
	tests/tr_temporal_iqm_motion_test.c
	code/renderervk/tr_temporal_iqm_motion.c
	code/renderervk/tr_temporal_motion.c
	code/renderervk/vk_bindless_publication.c
)
TARGET_INCLUDE_DIRECTORIES(tr_temporal_iqm_motion_test PRIVATE
	code/qcommon
	code/renderercommon
	code/renderervk
)
IF(NOT WIN32)
	TARGET_LINK_LIBRARIES(tr_temporal_iqm_motion_test m)
ENDIF()
ADD_TEST(NAME tr_temporal_iqm_motion_contract
	COMMAND tr_temporal_iqm_motion_test)
SET_TESTS_PROPERTIES(tr_temporal_iqm_motion_contract PROPERTIES
	LABELS "host-only;subsystem;owner.renderer;failure.contract;feature.always"
	TIMEOUT 30)
WIRED_CONFIGURE_HOST_TEST(tr_temporal_iqm_motion_test
	tr_temporal_iqm_motion_contract)

ADD_EXECUTABLE(vk_temporal_iqm_geometry_test
	tests/vk_temporal_iqm_geometry_test.c
	code/renderervk/vk_temporal_iqm_geometry.c
)
TARGET_INCLUDE_DIRECTORIES(vk_temporal_iqm_geometry_test PRIVATE
	code/qcommon
	code/renderercommon
	code/renderer/ral
	code/renderervk
)
ADD_TEST(NAME vk_temporal_iqm_geometry_contract
	COMMAND vk_temporal_iqm_geometry_test)
SET_TESTS_PROPERTIES(vk_temporal_iqm_geometry_contract PROPERTIES
	LABELS "host-only;subsystem;owner.renderer;owner.ral;failure.contract;feature.always"
	TIMEOUT 30)
WIRED_CONFIGURE_HOST_TEST(vk_temporal_iqm_geometry_test
	vk_temporal_iqm_geometry_contract)

ADD_EXECUTABLE(vk_bindless_publication_test
	tests/vk_bindless_publication_test.c
	code/renderervk/vk_bindless_publication.c
)
TARGET_INCLUDE_DIRECTORIES(vk_bindless_publication_test PRIVATE
	code/qcommon
	code/renderervk
)
ADD_TEST(NAME vk_bindless_publication_contract
	COMMAND vk_bindless_publication_test)
SET_TESTS_PROPERTIES(vk_bindless_publication_contract PROPERTIES
	LABELS "host-only;subsystem;owner.renderer;failure.contract;feature.always"
	TIMEOUT 30)
WIRED_CONFIGURE_HOST_TEST(vk_bindless_publication_test
	vk_bindless_publication_contract)

ADD_TEST(NAME vk_bindless_publication_source_policy_contract
	COMMAND ${CMAKE_COMMAND} -DROOT=${CMAKE_SOURCE_DIR}
		-P ${CMAKE_SOURCE_DIR}/cmake/vk_bindless_publication_policy_check.cmake)
SET_TESTS_PROPERTIES(vk_bindless_publication_source_policy_contract PROPERTIES
	LABELS "host-only;infrastructure;owner.renderer;failure.contract;feature.always"
	TIMEOUT 30)

ADD_EXECUTABLE(vk_bindless_cohort_test
	tests/vk_bindless_cohort_test.c
	code/renderervk/vk_bindless_cohort.c
	code/renderervk/vk_bindless_publication.c)
TARGET_INCLUDE_DIRECTORIES(vk_bindless_cohort_test PRIVATE
	code/qcommon code/renderervk)
ADD_TEST(NAME vk_bindless_cohort_contract
	COMMAND vk_bindless_cohort_test)
SET_TESTS_PROPERTIES(vk_bindless_cohort_contract PROPERTIES
	LABELS "host-only;subsystem;owner.renderer;failure.contract;feature.always"
	TIMEOUT 30)
WIRED_CONFIGURE_HOST_TEST(vk_bindless_cohort_test
	vk_bindless_cohort_contract)

ADD_EXECUTABLE(vk_temporal_iqm_payload_test
	tests/vk_temporal_iqm_payload_test.c
	code/renderervk/vk_temporal_iqm_payload.c
	code/renderervk/tr_temporal_iqm_motion.c
	code/renderervk/tr_temporal_motion.c
	code/renderervk/vk_bindless_publication.c)
TARGET_INCLUDE_DIRECTORIES(vk_temporal_iqm_payload_test PRIVATE
	code/qcommon code/renderercommon code/renderervk code/renderer/ral)
IF(NOT WIN32)
	TARGET_LINK_LIBRARIES(vk_temporal_iqm_payload_test m)
ENDIF()
ADD_TEST(NAME vk_temporal_iqm_payload_contract
	COMMAND vk_temporal_iqm_payload_test)
SET_TESTS_PROPERTIES(vk_temporal_iqm_payload_contract PROPERTIES
	LABELS "host-only;subsystem;owner.renderer;failure.contract;feature.always"
	TIMEOUT 30)
WIRED_CONFIGURE_HOST_TEST(vk_temporal_iqm_payload_test
	vk_temporal_iqm_payload_contract)

ADD_TEST(NAME vk_temporal_iqm_payload_source_policy_contract
	COMMAND ${CMAKE_COMMAND} -DROOT=${CMAKE_SOURCE_DIR}
		-P ${CMAKE_SOURCE_DIR}/cmake/vk_temporal_iqm_payload_policy_check.cmake)
SET_TESTS_PROPERTIES(vk_temporal_iqm_payload_source_policy_contract PROPERTIES
	LABELS "host-only;infrastructure;owner.renderer;failure.contract;feature.always"
	TIMEOUT 30)

ADD_EXECUTABLE(vk_temporal_iqm_payload_authoring_test
	tests/vk_temporal_iqm_payload_authoring_test.c
	code/renderervk/vk_temporal_iqm_payload_authoring.c
	code/renderervk/vk_temporal_iqm_payload.c
	code/renderervk/tr_temporal_iqm_motion.c
	code/renderervk/tr_temporal_motion.c
	code/renderervk/vk_bindless_publication.c)
TARGET_INCLUDE_DIRECTORIES(vk_temporal_iqm_payload_authoring_test PRIVATE
	code/qcommon code/renderercommon code/renderervk code/renderer/ral)
IF(NOT WIN32)
	TARGET_LINK_LIBRARIES(vk_temporal_iqm_payload_authoring_test m)
ENDIF()
ADD_TEST(NAME vk_temporal_iqm_payload_authoring_contract
	COMMAND vk_temporal_iqm_payload_authoring_test)
SET_TESTS_PROPERTIES(vk_temporal_iqm_payload_authoring_contract PROPERTIES
	LABELS "host-only;subsystem;owner.renderer;failure.contract;feature.always"
	TIMEOUT 30)
WIRED_CONFIGURE_HOST_TEST(vk_temporal_iqm_payload_authoring_test
	vk_temporal_iqm_payload_authoring_contract)

ADD_TEST(NAME vk_temporal_iqm_payload_authoring_source_policy_contract
	COMMAND ${CMAKE_COMMAND} -DROOT=${CMAKE_SOURCE_DIR}
		-P ${CMAKE_SOURCE_DIR}/cmake/vk_temporal_iqm_payload_authoring_policy_check.cmake)
SET_TESTS_PROPERTIES(vk_temporal_iqm_payload_authoring_source_policy_contract PROPERTIES
	LABELS "host-only;infrastructure;owner.renderer;failure.contract;feature.always"
	TIMEOUT 30)

ADD_EXECUTABLE(vk_temporal_iqm_exact3_factory_test
	tests/vk_temporal_iqm_exact3_factory_test.c
	code/renderervk/vk_temporal_iqm_exact3_factory.c
	code/renderervk/vk_temporal_iqm_payload.c
	code/renderervk/tr_temporal_iqm_motion.c
	code/renderervk/tr_temporal_motion.c
	code/renderervk/vk_bindless_publication.c)
TARGET_INCLUDE_DIRECTORIES(vk_temporal_iqm_exact3_factory_test PRIVATE
	code/qcommon code/renderercommon code/renderervk code/renderer/ral)
IF(NOT WIN32)
	TARGET_LINK_LIBRARIES(vk_temporal_iqm_exact3_factory_test m)
ENDIF()
ADD_TEST(NAME vk_temporal_iqm_exact3_factory_contract
	COMMAND vk_temporal_iqm_exact3_factory_test)
SET_TESTS_PROPERTIES(vk_temporal_iqm_exact3_factory_contract PROPERTIES
	LABELS "host-only;subsystem;owner.renderer;owner.ral;failure.contract;feature.always"
	TIMEOUT 30)
WIRED_CONFIGURE_HOST_TEST(vk_temporal_iqm_exact3_factory_test
	vk_temporal_iqm_exact3_factory_contract)

ADD_EXECUTABLE(vk_temporal_iqm_command_test
	tests/vk_temporal_iqm_command_test.c
	code/renderervk/vk_temporal_iqm_command.c
	code/renderervk/vk_temporal_main_rendering.c
	code/renderervk/tr_temporal_iqm_motion.c
	code/renderervk/tr_temporal_motion.c
	code/renderervk/vk_bindless_publication.c)
TARGET_INCLUDE_DIRECTORIES(vk_temporal_iqm_command_test PRIVATE
	code/qcommon code/renderercommon code/renderervk code/renderer/ral)
IF(NOT WIN32)
	TARGET_LINK_LIBRARIES(vk_temporal_iqm_command_test m)
ENDIF()
ADD_TEST(NAME vk_temporal_iqm_command_contract
	COMMAND vk_temporal_iqm_command_test)
SET_TESTS_PROPERTIES(vk_temporal_iqm_command_contract PROPERTIES
	LABELS "host-only;subsystem;owner.renderer;owner.ral;failure.contract;feature.always"
	TIMEOUT 30)
WIRED_CONFIGURE_HOST_TEST(vk_temporal_iqm_command_test
	vk_temporal_iqm_command_contract)

ADD_TEST(NAME vk_temporal_iqm_exact3_source_policy_contract
	COMMAND ${CMAKE_COMMAND} -DROOT=${CMAKE_SOURCE_DIR}
		-P ${CMAKE_SOURCE_DIR}/cmake/vk_temporal_iqm_exact3_policy_check.cmake)
SET_TESTS_PROPERTIES(vk_temporal_iqm_exact3_source_policy_contract PROPERTIES
	LABELS "host-only;infrastructure;owner.renderer;owner.ral;failure.contract;feature.always"
	TIMEOUT 30)

ADD_TEST(NAME vk_temporal_iqm_exact3_shader_reflection_contract
	COMMAND sh ${CMAKE_SOURCE_DIR}/tests/vk-temporal-iqm-exact3-shader-check.sh
		${CMAKE_SOURCE_DIR})
SET_TESTS_PROPERTIES(vk_temporal_iqm_exact3_shader_reflection_contract PROPERTIES
	LABELS "host-only;infrastructure;owner.renderer;owner.ral;failure.contract;feature.always"
	SKIP_RETURN_CODE 77 TIMEOUT 180)

ADD_TEST(NAME tr_temporal_iqm_motion_source_policy_contract
	COMMAND ${CMAKE_COMMAND} -DROOT=${CMAKE_SOURCE_DIR}
		-P ${CMAKE_SOURCE_DIR}/cmake/tr_temporal_iqm_motion_policy_check.cmake)
SET_TESTS_PROPERTIES(tr_temporal_iqm_motion_source_policy_contract PROPERTIES
	LABELS "host-only;infrastructure;owner.renderer;failure.contract;feature.always"
	TIMEOUT 30)

ADD_TEST(NAME vk_temporal_iqm_resource_product_source_policy_contract
	COMMAND ${CMAKE_COMMAND} -DROOT=${CMAKE_SOURCE_DIR}
		-P ${CMAKE_SOURCE_DIR}/cmake/vk_temporal_iqm_resource_product_policy_check.cmake)
SET_TESTS_PROPERTIES(vk_temporal_iqm_resource_product_source_policy_contract PROPERTIES
	LABELS "host-only;infrastructure;owner.renderer;owner.ral;failure.contract;feature.always"
	TIMEOUT 30)

ADD_TEST(NAME tr_temporal_motion_source_policy_contract
	COMMAND ${CMAKE_COMMAND} -DROOT=${CMAKE_SOURCE_DIR}
		-P ${CMAKE_SOURCE_DIR}/cmake/tr_temporal_motion_policy_check.cmake)
SET_TESTS_PROPERTIES(tr_temporal_motion_source_policy_contract PROPERTIES
	LABELS "host-only;infrastructure;owner.renderer;failure.contract;feature.always"
	TIMEOUT 30)

ADD_TEST(NAME ral_temporal_entity_source_policy_contract
	COMMAND ${CMAKE_COMMAND} -DROOT=${CMAKE_SOURCE_DIR}
		-P ${CMAKE_SOURCE_DIR}/cmake/ral_temporal_entity_policy_check.cmake)
SET_TESTS_PROPERTIES(ral_temporal_entity_source_policy_contract PROPERTIES
	LABELS "host-only;infrastructure;owner.renderer;failure.contract;feature.always"
	TIMEOUT 30)

ADD_EXECUTABLE(ref_entity_motion_test tests/ref_entity_motion_test.c)
TARGET_INCLUDE_DIRECTORIES(ref_entity_motion_test PRIVATE
	code/cgame
	code/qcommon
	code/renderercommon
)
ADD_TEST(NAME ref_entity_motion_contract COMMAND ref_entity_motion_test)
SET_TESTS_PROPERTIES(ref_entity_motion_contract PROPERTIES
	LABELS "host-only;subsystem;owner.renderer;failure.contract;feature.always"
	TIMEOUT 30
)
WIRED_CONFIGURE_HOST_TEST(ref_entity_motion_test ref_entity_motion_contract)

ADD_EXECUTABLE(cg_temporal_identity_test
	tests/cg_temporal_identity_test.c
	code/cgame/cg_temporal_identity.c)
TARGET_INCLUDE_DIRECTORIES(cg_temporal_identity_test PRIVATE
	code/cgame
	code/qcommon
	code/renderercommon)
ADD_TEST(NAME cg_temporal_identity_contract COMMAND cg_temporal_identity_test)
SET_TESTS_PROPERTIES(cg_temporal_identity_contract PROPERTIES
	LABELS "host-only;subsystem;owner.q3now-cgame;failure.contract;feature.always"
	TIMEOUT 30)
WIRED_CONFIGURE_HOST_TEST(cg_temporal_identity_test cg_temporal_identity_contract)

ADD_TEST(NAME cg_temporal_identity_source_policy_contract
	COMMAND ${CMAKE_COMMAND} -DROOT=${CMAKE_SOURCE_DIR}
		-P ${CMAKE_SOURCE_DIR}/cmake/cg_temporal_identity_policy_check.cmake)
SET_TESTS_PROPERTIES(cg_temporal_identity_source_policy_contract PROPERTIES
	LABELS "host-only;infrastructure;owner.q3now-cgame;failure.contract;feature.always"
	TIMEOUT 30)

ADD_TEST(NAME ref_entity_motion_source_policy_contract
	COMMAND ${CMAKE_COMMAND} -DROOT=${CMAKE_SOURCE_DIR}
		-P ${CMAKE_SOURCE_DIR}/cmake/ref_entity_motion_policy_check.cmake)
SET_TESTS_PROPERTIES(ref_entity_motion_source_policy_contract PROPERTIES
	LABELS "host-only;infrastructure;owner.renderer;failure.contract;feature.always"
	TIMEOUT 30
)

ADD_EXECUTABLE(r_profile_telemetry_codec_test
	tests/r_profile_telemetry_codec_test.c
	code/renderercommon/r_profile_telemetry.c)
TARGET_INCLUDE_DIRECTORIES(r_profile_telemetry_codec_test PRIVATE code/renderercommon)
ADD_TEST(NAME r_profile_telemetry_codec_contract COMMAND r_profile_telemetry_codec_test)
SET_TESTS_PROPERTIES(r_profile_telemetry_codec_contract PROPERTIES
	LABELS "host-only;subsystem;owner.renderer;owner.tools;failure.contract" TIMEOUT 30)
WIRED_CONFIGURE_HOST_TEST(r_profile_telemetry_codec_test r_profile_telemetry_codec_contract)

ADD_EXECUTABLE(wired_profile_telemetry_udp_test
	tests/wired_profile_telemetry_udp_test.cpp
	code/tools/profile_imgui/wired_profile_telemetry_udp.cpp
	code/renderercommon/r_profile_telemetry.c
	code/renderer/ral/ral_profile.c)
TARGET_INCLUDE_DIRECTORIES(wired_profile_telemetry_udp_test PRIVATE
	code/tools/profile_imgui code/renderer/ral code/renderercommon)
TARGET_COMPILE_FEATURES(wired_profile_telemetry_udp_test PRIVATE cxx_std_17)
IF(WIN32)
	# winsock: socket/htons/WSAStartup live in ws2_32 on Windows
	TARGET_LINK_LIBRARIES(wired_profile_telemetry_udp_test PRIVATE ws2_32)
ENDIF()
ADD_TEST(NAME wired_profile_telemetry_udp_contract COMMAND wired_profile_telemetry_udp_test)
SET_TESTS_PROPERTIES(wired_profile_telemetry_udp_contract PROPERTIES
	LABELS "host-only;subsystem;owner.tools;failure.contract" TIMEOUT 30)
WIRED_CONFIGURE_HOST_TEST(wired_profile_telemetry_udp_test wired_profile_telemetry_udp_contract)

ADD_EXECUTABLE(ral_profile_imgui_test tests/ral_profile_imgui_test.cpp)
TARGET_LINK_LIBRARIES(ral_profile_imgui_test PRIVATE wired_profile_imgui)
ADD_TEST(NAME ral_profile_imgui_contract COMMAND ral_profile_imgui_test)
SET_TESTS_PROPERTIES(ral_profile_imgui_contract PROPERTIES
	LABELS "host-only;subsystem;owner.ral;owner.tools;failure.contract;feature.imgui"
	TIMEOUT 30
)
WIRED_CONFIGURE_HOST_TEST(ral_profile_imgui_test ral_profile_imgui_contract)

ADD_EXECUTABLE(wired_profile_imgui_ral_test
	tests/wired_profile_imgui_ral_test.cpp
	code/tools/profile_imgui/wired_profile_imgui_ral.cpp
	${WIRED_IMGUI_DIR}/imgui.cpp
	${WIRED_IMGUI_DIR}/imgui_draw.cpp
	${WIRED_IMGUI_DIR}/imgui_tables.cpp
	${WIRED_IMGUI_DIR}/imgui_widgets.cpp
)
TARGET_INCLUDE_DIRECTORIES(wired_profile_imgui_ral_test PRIVATE
	code/tools/profile_imgui code/renderer/ral ${WIRED_IMGUI_DIR})
ADD_TEST(NAME wired_profile_imgui_ral_contract COMMAND wired_profile_imgui_ral_test)
SET_TESTS_PROPERTIES(wired_profile_imgui_ral_contract PROPERTIES
	LABELS "host-only;subsystem;owner.ral;owner.tools;failure.contract;feature.imgui"
	TIMEOUT 30)
WIRED_CONFIGURE_HOST_TEST(wired_profile_imgui_ral_test wired_profile_imgui_ral_contract)

ADD_TEST(NAME imgui_tool_policy_contract
	COMMAND ${CMAKE_COMMAND}
		-DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
		-DCOMPILE_COMMANDS=${CMAKE_BINARY_DIR}/compile_commands.json
		-DBUILD_NINJA=${CMAKE_BINARY_DIR}/build.ninja
		-DTOOLS_ENABLED=${WIRED_BUILD_IMGUI_TOOLS}
		-P ${CMAKE_SOURCE_DIR}/cmake/imgui_tool_policy_check.cmake
)
SET_TESTS_PROPERTIES(imgui_tool_policy_contract PROPERTIES
	LABELS "host-only;infrastructure;owner.ral;owner.tools;failure.build-graph;feature.imgui"
	SKIP_REGULAR_EXPRESSION "SKIP: ImGui ownership policy requires a Ninja build"
	TIMEOUT 30
)

ADD_TEST(NAME ral_profile_host_policy_contract
	COMMAND ${CMAKE_COMMAND}
		-DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
		-DCOMPILE_COMMANDS=${CMAKE_BINARY_DIR}/compile_commands.json
		-DBUILD_NINJA=${CMAKE_BINARY_DIR}/build.ninja
		-DTOOLS_ENABLED=${WIRED_BUILD_IMGUI_TOOLS}
		-P ${CMAKE_SOURCE_DIR}/cmake/ral_profile_host_policy_check.cmake
)
SET_TESTS_PROPERTIES(ral_profile_host_policy_contract PROPERTIES
	LABELS "host-only;infrastructure;owner.ral;owner.tools;failure.build-graph;feature.imgui"
	SKIP_REGULAR_EXPRESSION "SKIP: RAL profile host ownership policy requires a Ninja build"
	# 120s, not 30: the check scans build.ninja + compile_commands.json and the
	# Windows CI runner's filesystem pushed it past 30s (content passes there
	# once given time — Linux/mac finish well under the old limit).
	TIMEOUT 120
)

ADD_EXECUTABLE(wired_curve_test
	tests/wired_curve_test.c
	code/qcommon/wired/math/wired_curve.c
)
TARGET_INCLUDE_DIRECTORIES(wired_curve_test PRIVATE code/qcommon)
IF(NOT WIN32)
	TARGET_LINK_LIBRARIES(wired_curve_test m)
ENDIF()
ADD_TEST(NAME wired_curve COMMAND wired_curve_test)
SET_TESTS_PROPERTIES(wired_curve PROPERTIES
	LABELS "host-only;subsystem;owner.wired-math;failure.contract;feature.always"
	TIMEOUT 30
)
WIRED_CONFIGURE_HOST_TEST(wired_curve_test wired_curve)

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
SET_TESTS_PROPERTIES(wired_scene_eval PROPERTIES
	LABELS "host-only;subsystem;owner.wired-scene;failure.contract;feature.always"
	TIMEOUT 30
)
WIRED_CONFIGURE_HOST_TEST(wired_scene_eval_test wired_scene_eval)

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
SET_TESTS_PROPERTIES(sg_registry PROPERTIES
	LABELS "host-only;subsystem;owner.q3now-save;failure.contract;feature.always"
	TIMEOUT 30
)
WIRED_CONFIGURE_HOST_TEST(sg_registry_test sg_registry)

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
SET_TESTS_PROPERTIES(sg_savefile PROPERTIES
	LABELS "host-only;subsystem;owner.q3now-save;failure.contract;feature.always"
	TIMEOUT 30
)
WIRED_CONFIGURE_HOST_TEST(sg_savefile_test sg_savefile)

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
SET_TESTS_PROPERTIES(sg_serialize PROPERTIES
	LABELS "host-only;subsystem;owner.q3now-save;failure.contract;feature.always"
	TIMEOUT 30
)
WIRED_CONFIGURE_HOST_TEST(sg_serialize_test sg_serialize)

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
SET_TESTS_PROPERTIES(sg_psw PROPERTIES
	LABELS "host-only;subsystem;owner.q3now-save;failure.contract;feature.always"
	TIMEOUT 30
)
WIRED_CONFIGURE_HOST_TEST(sg_psw_test sg_psw)

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
SET_TESTS_PROPERTIES(sg_reloc PROPERTIES
	LABELS "host-only;subsystem;owner.q3now-save;failure.contract;feature.always"
	TIMEOUT 30
)
WIRED_CONFIGURE_HOST_TEST(sg_reloc_test sg_reloc)

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
SET_TESTS_PROPERTIES(mdl_anim PROPERTIES
	LABELS "host-only;subsystem;owner.wired-renderer-model;failure.contract;feature.always"
	TIMEOUT 30
)
WIRED_CONFIGURE_HOST_TEST(mdl_anim_test mdl_anim)

# Browser getinfo/infoResponse ingress contract. Compiles the exact production
# parser as a dependency-free host leaf and drives malicious numeric/text rows
# that previously reached the server-list cache through atoi/truncation.
ADD_EXECUTABLE(server_info_parse_test
	tests/server_info_parse_test.c
	code/client/cl_server_info.c
)
ADD_TEST(NAME server_info_parse_contract COMMAND server_info_parse_test)
SET_TESTS_PROPERTIES(server_info_parse_contract PROPERTIES
	LABELS "host-only;subsystem;owner.q3now-client;failure.security-contract;feature.always"
	TIMEOUT 30
)
WIRED_CONFIGURE_HOST_TEST(server_info_parse_test server_info_parse_contract)

# Browser challenge derivation.  Compiles the exact production helper and pins
# generation freshness even when the platform RNG repeats identical bytes.
ADD_EXECUTABLE(cl_info_challenge_test
	tests/cl_info_challenge_test.c
	code/client/cl_info_challenge.c
)
TARGET_INCLUDE_DIRECTORIES(cl_info_challenge_test PRIVATE code/client)
ADD_TEST(NAME cl_info_challenge_contract COMMAND cl_info_challenge_test)
SET_TESTS_PROPERTIES(cl_info_challenge_contract PROPERTIES
	LABELS "host-only;subsystem;owner.q3now-client;failure.security-contract;feature.always"
	TIMEOUT 30
)
WIRED_CONFIGURE_HOST_TEST(cl_info_challenge_test cl_info_challenge_contract)

# Directed-ping queue reuse policy. Compiles the exact dependency-free
# production selector used by the client allocator: a >=500ms valid completed
# result stays retained while a free slot exists, is reclaimed before a live
# request under saturation, and an all-live saturated queue evicts oldest.
ADD_EXECUTABLE(cl_ping_queue_test
	tests/cl_ping_queue_test.c
	code/client/cl_ping_queue.c
)
TARGET_INCLUDE_DIRECTORIES(cl_ping_queue_test PRIVATE code/client)
ADD_TEST(NAME cl_ping_queue_contract COMMAND cl_ping_queue_test)
SET_TESTS_PROPERTIES(cl_ping_queue_contract PROPERTIES
	LABELS "host-only;subsystem;owner.q3now-client;failure.contract;feature.always"
	TIMEOUT 30
)
WIRED_CONFIGURE_HOST_TEST(cl_ping_queue_test cl_ping_queue_contract)

# Ping transaction ownership. Compiles the exact dependency-free production
# seam that keeps direct/local/global/favorites consumers and cache effects
# source-scoped while the existing queue capacity remains shared.
ADD_EXECUTABLE(cl_ping_owner_test
	tests/cl_ping_owner_test.c
	code/client/cl_ping_owner.c
)
TARGET_INCLUDE_DIRECTORIES(cl_ping_owner_test PRIVATE code/client)
ADD_TEST(NAME cl_ping_owner_contract COMMAND cl_ping_owner_test)
SET_TESTS_PROPERTIES(cl_ping_owner_contract PROPERTIES
	LABELS "host-only;subsystem;owner.q3now-client;failure.contract;feature.always"
	TIMEOUT 30
)
WIRED_CONFIGURE_HOST_TEST(cl_ping_owner_test cl_ping_owner_contract)

# Current-protocol demo framing.  The exact production reader distinguishes
# the canonical <-1,-1> terminator from truncated/invalid envelopes and never
# publishes a partial frame on rejection.
ADD_EXECUTABLE(cl_demo_frame_test
	tests/cl_demo_frame_test.c
	code/client/cl_demo_frame.c
	code/qcommon/huffman_static.c
)
TARGET_INCLUDE_DIRECTORIES(cl_demo_frame_test PRIVATE code/client code/qcommon)
ADD_TEST(NAME cl_demo_frame_contract COMMAND cl_demo_frame_test)
SET_TESTS_PROPERTIES(cl_demo_frame_contract PROPERTIES
	LABELS "host-only;subsystem;owner.q3now-client;failure.security-contract;feature.always"
	TIMEOUT 30
)
WIRED_CONFIGURE_HOST_TEST(cl_demo_frame_test cl_demo_frame_contract)

# Console ownership: only the user (`~`) or a user-initiated session entry may
# take the console away. Compiles the exact dependency-free decision the
# CL_SetState transition observer consumes — attract reels and local \map loads
# soft-close (KEYCATCH_CONSOLE preserved), typed \connect/\demo and remote
# server-pushed map changes hard-close, non-session edges do nothing.
ADD_EXECUTABLE(cl_console_close_policy_test
	tests/cl_console_close_policy_test.c
	code/client/cl_console_close_policy.c
)
TARGET_INCLUDE_DIRECTORIES(cl_console_close_policy_test PRIVATE code/client)
ADD_TEST(NAME cl_console_close_policy_contract COMMAND cl_console_close_policy_test)
SET_TESTS_PROPERTIES(cl_console_close_policy_contract PROPERTIES
	LABELS "host-only;subsystem;owner.q3now-client;failure.contract;feature.always"
	TIMEOUT 30
)
WIRED_CONFIGURE_HOST_TEST(cl_console_close_policy_test cl_console_close_policy_contract)

# Attract-state first-input promotion. The console-close policy above is right
# to leave KEYCATCH_CONSOLE with the user after a soft close, but the promotion
# test in cl_keys.c used to read that catcher as "console is in the way" — so
# opening and closing the console once over attract killed keyboard input to the
# menu for good. A second clause required !com_sv_running, which a local map
# leaves set. Both were silent: nothing logged, keys just stopped working.
ADD_EXECUTABLE(attract_input_promotion_test
	tests/attract-input-promotion-test.c
)
ADD_TEST(NAME attract_input_promotion_contract COMMAND attract_input_promotion_test)
SET_TESTS_PROPERTIES(attract_input_promotion_contract PROPERTIES
	LABELS "host-only;subsystem;owner.q3now-client;failure.contract;feature.always"
	TIMEOUT 30
)
WIRED_CONFIGURE_HOST_TEST(attract_input_promotion_test attract_input_promotion_contract)

# Background preset evaluator. A menu declares what it wants behind it and the
# preset table decides which layers answer; menus never name a layer, so the
# mapping lives in one place and adding a layer touches no .wui file. Compiles
# the production TU so the real table is under test.
#
# The decisive case is pop-re-application. The model this replaces wrote an
# intent into the menu-stack slot at push time, so any path that showed a menu
# without pushing left it unwritten — coming back from a map put the main menu
# up on black. Evaluating from the stack top every frame has no write to skip.
ADD_EXECUTABLE(bg_preset_eval_test
	tests/bg-preset-eval-test.c
	code/client/wired/ui/policy/wui_bg_preset.c
)
TARGET_INCLUDE_DIRECTORIES(bg_preset_eval_test PRIVATE code/client/wired/ui)
ADD_TEST(NAME bg_preset_eval_contract COMMAND bg_preset_eval_test)
SET_TESTS_PROPERTIES(bg_preset_eval_contract PROPERTIES
	LABELS "host-only;subsystem;owner.q3now-client;failure.contract;feature.always"
	TIMEOUT 30
)
WIRED_CONFIGURE_HOST_TEST(bg_preset_eval_test bg_preset_eval_contract)

# Background z-plane pinning. Every background quad is a Clay floating
# attach-to-root element, so Clay sorts each one as its own tree root by zIndex
# and only preserves emit order within a single zIndex. The v2 layers routed
# through wui_bg_emit_rect and carried WUI_BG_SCENE_ZINDEX; the legacy six
# opened CLAY() inline and left .zIndex at its 0 default, so the opaque legacy
# base fill — emitted FIRST — sorted to the FRONT of the whole -10 stack and
# repainted the animated backdrop to near-black. ~190 quads drew every frame
# underneath one opaque quad, which reads as "the background does not render".
#
# Nothing at runtime can catch that: the zIndex is a literal at each call site,
# a mis-pinned quad still draws, and the result is a picture rather than an
# error. So this reads the production source and requires every `.floating`
# block in it to name the constant.
# wired_playtest.jsonl v1 evidence envelope (TASK-120 #1). The artefact is
# consumed by PROGRAMS — the alpha map-race harness dispatches on `ev` — so the
# schema is an interface, and an unpinned interface drifts silently: a renamed
# envelope field or a reordered event-name table still compiles, still runs,
# and still writes a plausible-looking file that means something different to
# every reader. Reads the production source rather than linking it, because
# playtest.c reaches the filesystem, the cvar registry and the mutex layer;
# everything worth pinning (frozen version, envelope fields, dictionary order,
# drop accounting, opt-in default) is statically present in the text. Same
# tactic and same reason as bg_layer_zorder_test below.
ADD_EXECUTABLE(playtest_envelope_test
	tests/playtest-envelope-test.c
)
TARGET_COMPILE_DEFINITIONS(playtest_envelope_test PRIVATE
	PLAYTEST_SOURCE="${CMAKE_CURRENT_SOURCE_DIR}/code/qcommon/wired/core/logging/playtest.c"
	PLAYTEST_HEADER="${CMAKE_CURRENT_SOURCE_DIR}/code/qcommon/wired/core/logging/playtest.h"
)
ADD_TEST(NAME playtest_envelope_contract COMMAND playtest_envelope_test)
SET_TESTS_PROPERTIES(playtest_envelope_contract PROPERTIES
	LABELS "host-only;subsystem;owner.wired-logging;failure.contract;feature.always"
	TIMEOUT 30
)
WIRED_CONFIGURE_HOST_TEST(playtest_envelope_test playtest_envelope_contract)

ADD_EXECUTABLE(bg_layer_zorder_test
	tests/bg-layer-zorder-test.c
)
TARGET_COMPILE_DEFINITIONS(bg_layer_zorder_test PRIVATE
	WUI_BG_SOURCE="${CMAKE_CURRENT_SOURCE_DIR}/code/client/wired/ui/cl_wired_bg.c"
	WUI_BG_HEADER="${CMAKE_CURRENT_SOURCE_DIR}/code/client/wired/ui/cl_wired_bg.h"
)
ADD_TEST(NAME bg_layer_zorder_contract COMMAND bg_layer_zorder_test)
SET_TESTS_PROPERTIES(bg_layer_zorder_contract PROPERTIES
	LABELS "host-only;subsystem;owner.q3now-client;failure.contract;feature.always"
	TIMEOUT 30
)
WIRED_CONFIGURE_HOST_TEST(bg_layer_zorder_test bg_layer_zorder_contract)

# WHICH emitter the ANIMATED background layer reaches. The composed backdrop was
# rewritten from ~200 Clay rects (WUI_BG_DEMO_BACKDROP) into one procedural pass
# (menubg.frag, via WUI_DrawBackgroundScene), and the rect stack was left behind
# as dead code. When the background layers were given their own content, the
# ANIMATED emit site was wired to the RETIRED constant — both call the same
# public API, both compile, both dispatch. But the rect stack has no clock: its
# sky/fog/floor are static bands and only 14 ember quads move, at alpha ~4/255
# over a near-black underpaint. Every `backdrop animated` menu (preferences plus
# the ten settings panels that take the same default preset) drew a backdrop that
# was neither animated nor visible — 0.1% warm coverage against 99.8% once the
# scene pass is emitted.
#
# Nothing at runtime catches it: both paths draw, and the failure is "the screen
# is dark", not an error. So this reads the production source and requires the
# ANIMATED arm to reach the scene emitter and the retired constant to have no
# emitter left. It also pins menubg.frag's sRGB->linear palette decode, whose
# absence is equally invisible (the backdrop just washes out).
ADD_EXECUTABLE(bg_animated_emitter_test
	tests/bg-animated-emitter-test.c
)
TARGET_COMPILE_DEFINITIONS(bg_animated_emitter_test PRIVATE
	WUI_CLAY_SOURCE="${CMAKE_CURRENT_SOURCE_DIR}/code/client/wired/ui/cl_wired_clay.c"
	WUI_BG_SOURCE="${CMAKE_CURRENT_SOURCE_DIR}/code/client/wired/ui/cl_wired_bg.c"
	WUI_BG_HEADER="${CMAKE_CURRENT_SOURCE_DIR}/code/client/wired/ui/cl_wired_bg.h"
	WUI_MENUBG_SHADER="${CMAKE_CURRENT_SOURCE_DIR}/code/renderervk/shaders/menubg.frag"
)
ADD_TEST(NAME bg_animated_emitter_contract COMMAND bg_animated_emitter_test)
SET_TESTS_PROPERTIES(bg_animated_emitter_contract PROPERTIES
	LABELS "host-only;subsystem;owner.q3now-client;failure.contract;feature.always"
	TIMEOUT 30
)
WIRED_CONFIGURE_HOST_TEST(bg_animated_emitter_test bg_animated_emitter_contract)

# How MUCH the background draws, as opposed to where it sorts. Every layer is a
# stack of quads whose count is a compile-time literal — the sky is a 64-band
# gradient, the fog 16, the floor fade 24, plus fixed dot and particle counts —
# and the composed ANIMATED preset comes to ~194 quads a frame. Raising one of
# those literals, or adding another band stack, costs frame time and produces no
# runtime complaint whatsoever: the frame just draws more. This reads the
# production constants and holds the composed total to a budget, with a separate
# bound on the sky because a gradient is the easiest layer to quietly enlarge.
ADD_EXECUTABLE(bg_emit_budget_test
	tests/bg-emit-budget-test.c
)
TARGET_COMPILE_DEFINITIONS(bg_emit_budget_test PRIVATE
	WUI_BG_SOURCE="${CMAKE_CURRENT_SOURCE_DIR}/code/client/wired/ui/cl_wired_bg.c"
	WUI_BG_HEADER="${CMAKE_CURRENT_SOURCE_DIR}/code/client/wired/ui/cl_wired_bg.h"
)
ADD_TEST(NAME bg_emit_budget_contract COMMAND bg_emit_budget_test)
SET_TESTS_PROPERTIES(bg_emit_budget_contract PROPERTIES
	LABELS "host-only;subsystem;owner.q3now-client;failure.contract;feature.always"
	TIMEOUT 30
)
WIRED_CONFIGURE_HOST_TEST(bg_emit_budget_test bg_emit_budget_contract)

# Re-arming a cached anim id. `if ( id == 0 )` cannot tell a live id from one
# WUI_AnimStopAll wiped — the pool is cleared but the caller's tracker still
# holds the old number — so the re-arm never fires and the value it drove stays
# frozen. The menu background's parallax died to this after every hot reload,
# silently.
ADD_EXECUTABLE(anim_rearm_test
	tests/anim-rearm-test.c
)
ADD_TEST(NAME anim_rearm_contract COMMAND anim_rearm_test)
SET_TESTS_PROPERTIES(anim_rearm_contract PROPERTIES
	LABELS "host-only;subsystem;owner.q3now-client;failure.contract;feature.always"
	TIMEOUT 30
)
WIRED_CONFIGURE_HOST_TEST(anim_rearm_test anim_rearm_contract)


# Local bot-slot allocation identity. Compiles the exact production parser and
# match seam so a queued removal cannot authorize a replacement bot in a reused
# physical client slot.
ADD_EXECUTABLE(bot_slot_identity_test
	tests/bot_slot_identity_test.c
	code/server/sv_bot_identity.c
)
TARGET_INCLUDE_DIRECTORIES(bot_slot_identity_test PRIVATE code/server)
ADD_TEST(NAME bot_slot_identity_contract COMMAND bot_slot_identity_test)
SET_TESTS_PROPERTIES(bot_slot_identity_contract PROPERTIES
	LABELS "host-only;subsystem;owner.q3now-server;failure.security-contract;feature.always"
	TIMEOUT 30
)
WIRED_CONFIGURE_HOST_TEST(bot_slot_identity_test bot_slot_identity_contract)

# Entity "gametype" key filtering. Compiles the production gametype table and
# BG_GametypeBits so a map entity tagged with a legacy alias ("oneflag", which is
# how id/Team Arena content spells One Flag CTF) cannot be freed at spawn again,
# and so matching stays whole-token rather than the substring test that let "ctf"
# match "1fctf".
ADD_EXECUTABLE(gametype_entity_filter_test
	tests/gametype_entity_filter_test.c
	code/game/bg_gametypes.c
)
TARGET_INCLUDE_DIRECTORIES(gametype_entity_filter_test PRIVATE code/qcommon code/game)
ADD_TEST(NAME gametype_entity_filter_contract COMMAND gametype_entity_filter_test)
SET_TESTS_PROPERTIES(gametype_entity_filter_contract PROPERTIES
	LABELS "host-only;subsystem;owner.q3now-game;failure.contract;feature.always"
	TIMEOUT 30
)
WIRED_CONFIGURE_HOST_TEST(gametype_entity_filter_test gametype_entity_filter_contract)

# Public VM interpretation semantics. Compiles the exact dependency-free
# production policy consumed by VM_Create: BYTECODE is .wasm-only, COMPILED is
# .aot then .wasm, and NATIVE is DLL then compiled-WASM fallback.
ADD_EXECUTABLE(vm_interpret_policy_test
	tests/vm_interpret_policy_test.c
	code/qcommon/vm_interpret_policy.c
)
TARGET_INCLUDE_DIRECTORIES(vm_interpret_policy_test PRIVATE code/qcommon)
ADD_TEST(NAME vm_interpret_policy_contract COMMAND vm_interpret_policy_test)
SET_TESTS_PROPERTIES(vm_interpret_policy_contract PROPERTIES
	LABELS "host-only;subsystem;owner.wired-vm;failure.contract;feature.always"
	TIMEOUT 30
)
WIRED_CONFIGURE_HOST_TEST(vm_interpret_policy_test vm_interpret_policy_contract)

# Typed cvar lexical/range ingress. Compiles the exact dependency-free
# production seam used by qcommon and qcommon_ded; rejection must be byte-atomic.
ADD_EXECUTABLE(cvar_value_test
	tests/cvar_value_test.c
	code/qcommon/wired/core/cvars/cvar_value.c
)
ADD_TEST(NAME cvar_value_contract COMMAND cvar_value_test)
SET_TESTS_PROPERTIES(cvar_value_contract PROPERTIES
	LABELS "host-only;subsystem;owner.wired-cvars;failure.security-contract;feature.always"
	TIMEOUT 30
)
WIRED_CONFIGURE_HOST_TEST(cvar_value_test cvar_value_contract)

# Same seam, widened: per-input CTest entries plus the boundary families the C
# contract does not reach — inclusive range ends, huge magnitudes that must not
# wrap back into range, and nan/inf, which strtof accepts and which defeat every
# naive min/max comparison once they are in a cvar.
WIRED_ADD_GTEST(cvar_value_gtest cvar_value
	"host-only;subsystem;owner.wired-cvars;failure.security-contract;feature.always"
	SOURCES
		tests/cvar_value_gtest.cpp
		code/qcommon/wired/core/cvars/cvar_value.c
)

# Command tokenizer + registration/dispatch. Unlike the seams above, cmd.c has
# never been carved out for testing, so this links a stub TU for the console,
# cvar, filesystem and game-dispatch symbols it references — see
# tests/cmd_gtest_stubs.c for why that tradeoff was taken here and why it is not
# the default pattern. q_shared.c is the REAL implementation, not a stub: the
# tokenizer's behaviour depends on its string helpers, and substituting them
# would test the substitute.
WIRED_ADD_GTEST(cmd_gtest cmd
	"host-only;subsystem;owner.wired-shell;failure.contract;feature.always"
	SOURCES
		tests/cmd_gtest.cpp
		tests/cmd_gtest_stubs.c
		code/qcommon/wired/core/shell/cmd.c
		code/qcommon/q_shared.c
		code/qcommon/q_string.c
		code/qcommon/q_math.c
		code/qcommon/util/string.c
)

# VFS qpaths must reject exact parent-directory components before any handle
# allocation or host-filesystem call. Compiles the exact production lexical
# seam used by both qcommon targets; symlink/junction containment is out of scope.
# Particle curve table + parameter evaluator. Compiles the exact production
# TU: particle_curve.c is deliberately pure (no cgame, no traps, no cvars) so
# this test needs no stubs — the alternative would be reaching into
# cg_local.h and stubbing the cgame surface to test two functions.
ADD_EXECUTABLE(particle_curve_test
	tests/particle_curve_test.c
	code/qcommon/wired/render/particle_curve.c
)
TARGET_COMPILE_DEFINITIONS(particle_curve_test PRIVATE
	SHADER_DIR="${CMAKE_SOURCE_DIR}/code/renderervk/shaders"
)
ADD_TEST(NAME particle_curve_contract COMMAND particle_curve_test)
SET_TESTS_PROPERTIES(particle_curve_contract PROPERTIES
	LABELS "host-only;subsystem;owner.renderer;failure.contract;feature.always"
	TIMEOUT 30
)

ADD_EXECUTABLE(fs_qpath_test
	tests/fs_qpath_test.c
	code/qcommon/wired/core/vfs/fs_qpath.c
)
ADD_TEST(NAME fs_qpath_contract COMMAND fs_qpath_test)
SET_TESTS_PROPERTIES(fs_qpath_contract PROPERTIES
	LABELS "host-only;subsystem;owner.wired-vfs;failure.security-contract;feature.always"
	TIMEOUT 30
)
WIRED_CONFIGURE_HOST_TEST(fs_qpath_test fs_qpath_contract)

# Same seam, parameterised: one CTest entry per input instead of one for the
# whole set, so a regression names the offending path. Kept ALONGSIDE the C
# contract rather than replacing it — the C one is the dependency-free spelling
# that runs even when the googletest submodule is not checked out, and losing
# that would make a security contract conditional on a vendored library.
WIRED_ADD_GTEST(fs_qpath_gtest fs_qpath
	"host-only;subsystem;owner.wired-vfs;failure.security-contract;feature.always"
	SOURCES
		tests/fs_qpath_gtest.cpp
		code/qcommon/wired/core/vfs/fs_qpath.c
)

# Recyclable game-connection handles must be paired with their monotonic
# allocation identity before a pending CONNECT/READY event can be consumed.
ADD_EXECUTABLE(wn_identity_test tests/wn_identity_test.c)
ADD_TEST(NAME wired_connection_identity_contract COMMAND wn_identity_test)
SET_TESTS_PROPERTIES(wired_connection_identity_contract PROPERTIES
	LABELS "host-only;subsystem;owner.wired-network;failure.security-contract;feature.always"
	TIMEOUT 30
)
WIRED_CONFIGURE_HOST_TEST(wn_identity_test wired_connection_identity_contract)

# ─────────────────────────────────────────────────────────────────────────────
# PROCESS-LEVEL RELEASE SMOKES  (TASK-122 #10, #11)
#
# Everything above this line is `host-only`: pure, in-process, no engine
# launch. The registrations below LAUNCH REAL PROCESSES, so they carry the
# `process` label instead — a different layer with different failure modes and
# a different cost profile. Keeping them off `host-only` is deliberate: the
# existing `make test-host` gate (`ctest -L host-only`) must keep meaning
# exactly what it meant before, and CI reports the two layers separately
# (TASK-122 #12).
#
# 🔴 THE SKIP CONTRACT. A process smoke depends on artefacts CMake does not
# build: launcher-produced content paks, an installed launcher binary. Those
# are absent on an asset-free CI runner. Each script therefore exits 77 when
# its prerequisites are missing and every registration sets SKIP_RETURN_CODE
# 77, so a runner without content reports NOTASRUN rather than red. The
# scripts SKIP only on missing prerequisites — never to dodge a real defect;
# a script that finds its prerequisites and then sees a broken engine exits 1.
#
# Single command that satisfies #10 (launcher + network + map transition):
#     ctest --test-dir <build> -L process
#
# ── the map-transition binary is a DEBUG-build dependency ────────────────────
# The map-transition smoke is fail-closed against a release binary: the
# ZONEID zone-consistency assertion it needs is compiled only under _DEBUG
# (code/qcommon/common.c:409-412), so pointing it at a release build would
# manufacture a vacuous green and the script refuses with exit 1, not 77.
# That means it can only be REGISTERED in a Debug tree. In a Release tree it
# is registered in its engine-free `--self-test` form (the analyzer mutation
# suite), which is where the gate's teeth live and which needs neither a
# debug binary nor content.
SET(WIRED_PROCESS_SMOKE_TIMEOUT 300)

# launcher — CLI dispatch + the fail-closed EULA licence gate. No display, no
# network, no content: the assertion is that `assets download` REFUSES.
ADD_TEST(NAME launcher_process_smoke
	COMMAND bash ${CMAKE_SOURCE_DIR}/tests/smoke-launcher.sh
)
SET_TESTS_PROPERTIES(launcher_process_smoke PROPERTIES
	LABELS "process;owner.launcher;failure.smoke;feature.always"
	SKIP_RETURN_CODE 77
	TIMEOUT 120
)

# network — QUIC game transport must reach the listening state. The smoke
# requires the wn_main.c:582 "WiredNet: listening on port N (IPv4), ALPN:"
# line, which is reachable only after picoquic_create succeeded.
#
# 🔴 NO $<TARGET_FILE:...> HERE, DELIBERATELY. The freshly linked binary in the
# build tree is NOT a runnable install on macOS: FS_GetInstallResourcePath()
# resolves paks under <root>/Contents/Resources (qcommon.h:945-956), so a flat
# `build/release/base/pax21.sw3z` is never searched and the engine aborts with
# "Couldn't load default.cfg" before the transport initialises. Passing the
# build-tree target therefore produces a red that says nothing about QUIC.
# The scripts resolve the ASSEMBLED install through tests/lib/wired_paths.sh
# (`make copy-all` output) and SKIP 77 when it is absent.
ADD_TEST(NAME network_quic_process_smoke
	COMMAND bash ${CMAKE_SOURCE_DIR}/tests/smoke-quic-game.sh
)
SET_TESTS_PROPERTIES(network_quic_process_smoke PROPERTIES
	LABELS "process;owner.wired-network;failure.smoke;feature.always"
	SKIP_RETURN_CODE 77
	TIMEOUT ${WIRED_PROCESS_SMOKE_TIMEOUT}
	# Binds a UDP port. Serialised against the other port-binding smokes via a
	# shared resource lock so a parallel `ctest -j` cannot make them collide.
	RESOURCE_LOCK wired_net_port
)

# engine bring-up — headless server loads a map with a bot and shuts down
# clean. Needs a BSP-bearing content pak; exits 77 without one.
# Same reason as above for not passing $<TARGET_FILE:...>: the assembled
# install is the only layout the engine can resolve its paks from on macOS.
ADD_TEST(NAME headless_engine_process_smoke
	COMMAND bash ${CMAKE_SOURCE_DIR}/tests/smoke.sh
)
SET_TESTS_PROPERTIES(headless_engine_process_smoke PROPERTIES
	LABELS "process;owner.q3now-server;failure.smoke;feature.always"
	SKIP_RETURN_CODE 77
	TIMEOUT ${WIRED_PROCESS_SMOKE_TIMEOUT}
	# No Q3DIR here on purpose: the script resolves the install root and the
	# engine home through tests/lib/wired_paths.sh, which derives them from
	# PRODUCT_NAME/CHANNEL_SUFFIX rather than from a literal baked in twice.
	RESOURCE_LOCK wired_net_port
)

# map transition — N-run lifecycle/race gate (#11).
#
# WHY N=10 AND NOT 1: the target class is lifecycle/race, which a single run
# cannot bound. Each iteration is a fresh process over an IDENTICAL map chain
# and identical cvars; what varies is engine-side nondeterminism only (RNG
# re-seed, ASLR, the background navmesh bake's join point against the main
# thread's map load, async QUIC bring-up). Holding the stimulus fixed makes a
# divergence between iterations attributable to state lifetime rather than to
# a changed input. Every iteration is scored by the same analyzer the
# single-shot mode uses, so repeats cannot pass on a weaker gate.
SET(WIRED_MAP_TRANSITION_REPEATS 10 CACHE STRING
	"Iterations for the map-transition lifecycle/race smoke (TASK-122 #11; must be >= 10)")
IF(WIRED_MAP_TRANSITION_REPEATS LESS 10)
	MESSAGE(FATAL_ERROR
		"WIRED_MAP_TRANSITION_REPEATS must be >= 10 — TASK-122 #11 requires at least "
		"10 repeats for the lifecycle/race class to be exercised at all "
		"(got ${WIRED_MAP_TRANSITION_REPEATS}).")
ENDIF()

IF(CMAKE_BUILD_TYPE STREQUAL "Debug")
	# Debug tree only: the gate needs the _DEBUG-only ZONEID assertion.
	# Each iteration loads 3 maps with +wait gating, so the budget scales
	# with the repeat count rather than being a flat number.
	MATH(EXPR WIRED_MAP_TRANSITION_TIMEOUT "${WIRED_MAP_TRANSITION_REPEATS} * 180")
	# $<TARGET_FILE:...> IS right here, unlike the two smokes above: this
	# script does not launch the binary in place. It assembles its own scratch
	# fs_homepath from the discovered pax21 + pax01 and points the engine at
	# it, so the bundle-vs-flat layout difference does not apply — and it
	# specifically needs THIS tree's debug binary, not whatever is installed.
	ADD_TEST(NAME map_transition_process_smoke
		COMMAND bash ${CMAKE_SOURCE_DIR}/tests/headless-map-transition-zonecheck.sh
			--repeat ${WIRED_MAP_TRANSITION_REPEATS}
			$<TARGET_FILE:${DNAME}${BINEXT}>
	)
	SET_TESTS_PROPERTIES(map_transition_process_smoke PROPERTIES
		LABELS "process;owner.q3now-server;failure.lifecycle;feature.always"
		SKIP_RETURN_CODE 77
		TIMEOUT ${WIRED_MAP_TRANSITION_TIMEOUT}
		# The BSP-bearing pak (pax01) is launcher-produced and lands in the
		# engine HOME dir, not in the build tree or the install (GAME-DATA.md
		# §2). Point the script's content probe there; without it the script
		# SKIPs 77 rather than running a chain it has no maps for.
		ENVIRONMENT "WIRED_CONTENT_ROOT=$ENV{HOME}/wired/${PRODUCT_NAME}${CHANNEL_SUFFIX}"
		RESOURCE_LOCK wired_net_port
	)
ELSE()
	# Release tree: register the engine-free analyzer mutation suite under the
	# `process` label so `ctest -L process` still reports on the map-transition
	# gate here — as the teeth check it can honestly run, not as a vacuous
	# pass of a gate whose assertion is compiled out.
	ADD_TEST(NAME map_transition_analyzer_teeth
		COMMAND bash ${CMAKE_SOURCE_DIR}/tests/headless-map-transition-zonecheck.sh
			--self-test
	)
	SET_TESTS_PROPERTIES(map_transition_analyzer_teeth PROPERTIES
		LABELS "process;owner.q3now-server;failure.contract;feature.always"
		SKIP_RETURN_CODE 77
		TIMEOUT 120
	)
ENDIF()
ENDIF()

# wired/scene — cinematic-scene definition + Lua-table loader. Pure CPU +
# Lua load (NOT renderer, NOT RAL): loads a scene .lua that returns a table
# into a wiredScene_t (eye-path spline + named target splines + fov lerp +
# decoded event list) atop the wired/math spline core. Depends on WiredScript
# (the Lua VM) — scene definitions are Lua like the rest of Wired content. The
# luajit include dir + link for qcommon/qcommon_ded are set with WiredScript's.
FILE(GLOB WIRED_SCENE_SRCS code/qcommon/wired/scene/*.c)
WIRED_ROUTE_QCOMMON_SOURCES(${WIRED_SCENE_SRCS})

# The wired_scene Lua-loader unit test links LuaJIT, so its target is defined
# in the IF(USE_LUA) block below (after LUAJIT_DIR / the luajit target exist).

# util/ — utility sub-components (crypto, md5, md4; string/sort/hash extraction TBD)
FILE(GLOB UTIL_SRCS code/qcommon/util/*.c)
WIRED_ROUTE_QCOMMON_SOURCES(${UTIL_SRCS})
TARGET_INCLUDE_DIRECTORIES(qcommon     PRIVATE code/qcommon/util)
TARGET_INCLUDE_DIRECTORIES(qcommon_ded PRIVATE code/qcommon/util)
TARGET_INCLUDE_DIRECTORIES(qcommon_engine_shared PRIVATE code/qcommon/util)
TARGET_INCLUDE_DIRECTORIES(botlib      PRIVATE code/qcommon/util)

# ── zlib-ng (faster inflate for pk3 decompression) ────────────────
# Replaces the embedded zlib in unzip.c via USE_ZLIB_NG guard.
# Submodule: src/libs/zlib-ng  —  cmake -B build -DUSE_ZLIB_NG=ON
IF(USE_ZLIB_NG)
    SET(ZLIB_COMPAT         ON  CACHE BOOL "" FORCE)
    SET(ZLIB_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    # zlib-ng deprecated ZLIB_ENABLE_TESTS in favour of BUILD_TESTING and warns
    # on every configure when it is defined. BUILD_TESTING cannot simply be left
    # alone, though: it is a GLOBAL option and ours is ON, which would switch
    # zlib-ng's own test and gtest targets back on (zlib-ng/CMakeLists.txt:56).
    # Shadow it with a directory-scoped variable for the subdirectory only, then
    # restore, so our own BUILD_TESTING keeps its value for everything after.
    SET(_WIRED_SAVED_BUILD_TESTING ${BUILD_TESTING})
    SET(BUILD_TESTING OFF)
    ADD_SUBDIRECTORY(src/libs/zlib-ng EXCLUDE_FROM_ALL)
    SET(BUILD_TESTING ${_WIRED_SAVED_BUILD_TESTING})
    UNSET(_WIRED_SAVED_BUILD_TESTING)
    TARGET_INCLUDE_DIRECTORIES(qcommon     PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/libs/zlib-ng
        ${CMAKE_CURRENT_BINARY_DIR}/src/libs/zlib-ng)
    TARGET_INCLUDE_DIRECTORIES(qcommon_ded PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/libs/zlib-ng
        ${CMAKE_CURRENT_BINARY_DIR}/src/libs/zlib-ng)
    TARGET_INCLUDE_DIRECTORIES(qcommon_engine_shared PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/libs/zlib-ng
        ${CMAKE_CURRENT_BINARY_DIR}/src/libs/zlib-ng)
    TARGET_LINK_LIBRARIES(qcommon     PRIVATE zlibstatic)
    TARGET_LINK_LIBRARIES(qcommon_ded PRIVATE zlibstatic)
    TARGET_LINK_LIBRARIES(qcommon_engine_shared PRIVATE zlibstatic)
ELSE()
    TARGET_COMPILE_DEFINITIONS(qcommon     PRIVATE FEAT_LEGACY_ZLIB)
    TARGET_COMPILE_DEFINITIONS(qcommon_ded PRIVATE FEAT_LEGACY_ZLIB)
    TARGET_COMPILE_DEFINITIONS(qcommon_engine_shared PRIVATE FEAT_LEGACY_ZLIB)
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
	# WAMR's CMakeLists defaults WAMR_BUILD_PLATFORM from CMAKE_HOST_SYSTEM_NAME,
	# which is wrong when cross-compiling — pin it to the TARGET system.
	IF(CMAKE_CROSSCOMPILING AND NOT DEFINED WAMR_BUILD_PLATFORM)
		STRING(TOLOWER ${CMAKE_SYSTEM_NAME} WAMR_BUILD_PLATFORM)
	ENDIF()
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
	TARGET_INCLUDE_DIRECTORIES(qcommon_engine_shared PRIVATE src/libs/wamr/core/iwasm/include)
ENDIF()

IF(NO_VM_COMPILED)
	TARGET_COMPILE_DEFINITIONS(qcommon PRIVATE NO_VM_COMPILED)
	TARGET_COMPILE_DEFINITIONS(qcommon_ded PRIVATE NO_VM_COMPILED)
	TARGET_COMPILE_DEFINITIONS(qcommon_engine_shared PRIVATE NO_VM_COMPILED)
ENDIF()

# ── WiredNet ──────────────────────────────────────────────────────
# picoquic + picotls + mpack sources
# These are added as git submodules under src/libs/

# ── Vendored-submodule patches ────────────────────────────────────────────────
# Some submodules need fixes that are not upstream.  Rather than forking each
# one, the fix is kept as a patch under patches/<name>/ and applied to the
# submodule working tree at configure time.
#
# Why this matters: a superproject records only a submodule's COMMIT, never its
# working-tree content.  Edits made directly inside a submodule are invisible to
# `git add -A` in the superproject and are silently lost on a fresh clone — the
# Detour extensions were stranded on one machine exactly this way (2026-08-10).
# Anything a submodule needs beyond its pinned commit MUST live here as a patch.
# See SUBMODULE-PATCHES.md.
#
# Idempotency: try `git apply --check --reverse` first — exits 0 only when
# the patch is already cleanly applied, so we skip.  Otherwise apply with
# --ignore-whitespace so CRLF/LF drift on Windows working trees (Git's
# autocrlf converts checked-out files) doesn't fail the patch context
# match.  Both check and apply pass --ignore-whitespace for symmetry.
# Umbrella target the per-submodule verifiers hang off. Engine targets depend on
# this, so a missing patch stops the build instead of producing a quiet binary.
ADD_CUSTOM_TARGET(wired_verify_patches)

FUNCTION(WIRED_APPLY_SUBMODULE_PATCHES _label _submodule_dir _patch_dir)
	FILE(GLOB _patches "${CMAKE_SOURCE_DIR}/${_patch_dir}/*.patch")
	IF(NOT _patches)
		RETURN()
	ENDIF()
	FIND_PROGRAM(_GIT_EXE git REQUIRED)
	# Pick the apply mode by asking git, not by probing for a .git entry.
	# A Docker build context is the trap: .dockerignore strips the root .git/
	# but each submodule still carries its .git GITFILE ("gitdir: ../../../
	# .git/modules/...") pointing at metadata that no longer exists — any git
	# command run INSIDE the submodule dies with "fatal: not a git
	# repository". When that happens (and the sources are present), apply
	# from the source root in plain-tree mode via --directory: git apply
	# works outside a repository as a pure file operation.
	EXECUTE_PROCESS(
		COMMAND "${_GIT_EXE}" -C "${CMAKE_SOURCE_DIR}/${_submodule_dir}" rev-parse --is-inside-work-tree
		RESULT_VARIABLE _rc_repo
		OUTPUT_QUIET ERROR_QUIET)
	IF(_rc_repo EQUAL 0)
		SET(_apply_cwd "${CMAKE_SOURCE_DIR}/${_submodule_dir}")
		SET(_dir_flag "")
	ELSE()
		IF(NOT EXISTS "${CMAKE_SOURCE_DIR}/${_submodule_dir}/CMakeLists.txt"
		   AND NOT EXISTS "${CMAKE_SOURCE_DIR}/${_submodule_dir}/Makefile")
			MESSAGE(FATAL_ERROR
				"${_label}: ${_patch_dir} carries patches, but the submodule "
				"${_submodule_dir} is not checked out.\n"
				"Run:  git submodule update --init --recursive")
		ENDIF()
		MESSAGE(STATUS "${_label}: git metadata unavailable (container build context) — plain-tree apply")
		SET(_apply_cwd "${CMAKE_SOURCE_DIR}")
		SET(_dir_flag "--directory=${_submodule_dir}")
	ENDIF()
	LIST(SORT _patches)
	# Two attempts. A patch-series EDIT (a hunk folded into an earlier patch,
	# a patch renumbered) leaves previously-patched working trees in a state
	# where neither the already-applied check nor a fresh apply can succeed.
	# The series is the single source of truth for local submodule content
	# (see the policy note above), so in repo mode the recovery is mechanical:
	# reset the submodule tree and re-run the series once from scratch.
	# Plain-tree mode (container) has no git metadata to reset — it always
	# starts from a pristine checkout anyway, so a failure there is real.
	FOREACH(_attempt RANGE 1)
		SET(_series_failed FALSE)
		FOREACH(_patch ${_patches})
			GET_FILENAME_COMPONENT(_patch_name "${_patch}" NAME)
			EXECUTE_PROCESS(
				COMMAND "${_GIT_EXE}" apply ${_dir_flag} --check --reverse --ignore-whitespace "${_patch}"
				WORKING_DIRECTORY "${_apply_cwd}"
				RESULT_VARIABLE _rc_already
				OUTPUT_QUIET ERROR_QUIET)
			IF(_rc_already EQUAL 0)
				MESSAGE(STATUS "${_label} patch already applied: ${_patch_name}")
			ELSE()
				EXECUTE_PROCESS(
					COMMAND "${_GIT_EXE}" apply ${_dir_flag} --ignore-whitespace "${_patch}"
					WORKING_DIRECTORY "${_apply_cwd}"
					RESULT_VARIABLE _rc_apply
					OUTPUT_VARIABLE _out_apply
					ERROR_VARIABLE _err_apply)
				IF(NOT _rc_apply EQUAL 0)
					IF(_attempt EQUAL 0 AND _dir_flag STREQUAL "")
						MESSAGE(STATUS
							"${_label}: ${_patch_name} does not fit the current working "
							"tree (patch-series drift) — resetting the submodule tree "
							"and reapplying the series from scratch")
						EXECUTE_PROCESS(
							COMMAND "${_GIT_EXE}" checkout -- .
							WORKING_DIRECTORY "${_apply_cwd}")
						SET(_series_failed TRUE)
						BREAK()
					ENDIF()
					MESSAGE(FATAL_ERROR
						"Failed to apply ${_patch_name}:\n${_err_apply}\n"
						"This usually means the ${_label} submodule has unrelated "
						"local modifications that conflict with the patch hunks.\n"
						"To recover:\n"
						"  git -C ${_submodule_dir} reset --hard\n"
						"  rm -rf build/release  # or build/debug\n"
						"  re-run cmake / make")
				ENDIF()
				MESSAGE(STATUS "applied ${_label} patch: ${_patch_name}")
			ENDIF()
		ENDFOREACH()
		IF(NOT _series_failed)
			BREAK()
		ENDIF()
	ENDFOREACH()

	# Re-verify at BUILD time, not just here.
	#
	# Patching is a configure-time side effect, and ninja's dependency graph has
	# no edge from a submodule source file to the patch that rewrites it. So once
	# the tree is configured, a plain build never re-checks: after any
	# `git -C src/libs/<sub> checkout/reset` — which SUBMODULE-PATCHES.md §3
	# tells you to do while working on a patch, and which `git submodule update`
	# does on its own — the build succeeds, the binary runs, and the patch is
	# simply absent. Nothing warns. For the sentry patch that means the crash
	# handler is silently dropped again, i.e. the exact defect the patch fixes,
	# reappearing with no signal.
	#
	# The check is the same `--check --reverse` test used above: exit 0 means
	# "already applied". Cheap (one git call per patch) and it fails the BUILD
	# rather than printing a warning nobody reads, because a silently unpatched
	# binary is the failure mode this whole directory exists to prevent.
	STRING(REPLACE "/" "_" _verify_target "wired_verify_patches_${_label}")
	ADD_CUSTOM_TARGET(${_verify_target}
		COMMAND "${CMAKE_COMMAND}"
			-D "GIT_EXE=${_GIT_EXE}"
			-D "LABEL=${_label}"
			-D "APPLY_CWD=${_apply_cwd}"
			-D "DIR_FLAG=${_dir_flag}"
			-D "PATCHES=${_patches}"
			-P "${CMAKE_SOURCE_DIR}/cmake/VerifySubmodulePatches.cmake"
		COMMENT "Verifying ${_label} submodule patches are still applied"
		VERBATIM)
	ADD_DEPENDENCIES(wired_verify_patches ${_verify_target})
ENDFUNCTION()

# MinGW: picoquic + picotls have a handful of MSVC-only assumptions that
# break under MinGW's CRT (struct timezone redefinition, va_args macros,
# ws2tcpip include order, _dupenv_s missing).  Local patches in
# patches/picoquic-mingw/ fix each.  Windows-only — the hunks target MinGW
# CRT workarounds that would not apply against a POSIX build.
IF(WIN32)
	WIRED_APPLY_SUBMODULE_PATCHES(picoquic src/libs/picoquic patches/picoquic-mingw)
	WIRED_APPLY_SUBMODULE_PATCHES(picotls src/libs/picotls patches/picotls-mingw)
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
# wintimeofday(). The implementation lives in picotls' Visual-Studio-era
# subdirectory (picotlsvs/picotls/wintimeofday.c) and is not in the default
# library source list; add it explicitly so the MinGW link resolves.
IF(WIN32)
	list(APPEND PICOTLS_SRCS src/libs/picotls/picotlsvs/picotls/wintimeofday.c)
ENDIF()

FILE(GLOB MPACK_SRCS    src/libs/mpack/src/mpack/*.c)

# wired/net module — WiredNet transport layer
FILE(GLOB WN_SRCS code/qcommon/wired/net/*.c)

SET(WIREDNET_MATERIAL_SRCS ${PICOQUIC_SRCS} ${PICOTLS_SRCS} ${MPACK_SRCS})

# MinGW: MinGW's <inttypes.h> defines imaxabs via __CRT_INLINE which under
# strict -std=c11 expands to extern inline (strong symbol per TU) instead of
# the GNU/C89 semantics of "inline-only, no strong definition emitted". Many
# picoquic+picotls TUs include <inttypes.h> transitively, so the linker sees
# multiple definitions of imaxabs. -fgnu-inline restores the GNU inline
# semantics for these TUs only without raising the C standard floor.
# See docs/health.md "picoquic _WINDOWS guards do not match MinGW".
IF(WIN32)
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

# Vendor transport material has no qcommon or HEADLESS dependency. Compile it
# once beside LZ4; keep WiredNet's WN_* integration translation units variant-
# local because they do consume client/server compile-time policy.
WIRED_ROUTE_QCOMMON_SOURCES(${WIREDNET_MATERIAL_SRCS})
TARGET_INCLUDE_DIRECTORIES(qcommon_engine_shared PRIVATE ${WIREDNET_INCLUDE_DIRS})
TARGET_LINK_LIBRARIES(qcommon_engine_shared PUBLIC OpenSSL::Crypto)

# Add the WiredNet integration layer to qcommon (client) and qcommon_ded
# (headless server).
WIRED_ROUTE_QCOMMON_SOURCES(${WN_SRCS})
TARGET_INCLUDE_DIRECTORIES(qcommon PRIVATE ${WIREDNET_INCLUDE_DIRS})
TARGET_LINK_LIBRARIES(qcommon PRIVATE OpenSSL::Crypto)

TARGET_INCLUDE_DIRECTORIES(qcommon_ded PRIVATE ${WIREDNET_INCLUDE_DIRS})
TARGET_LINK_LIBRARIES(qcommon_ded PRIVATE OpenSSL::Crypto)


# ── Crash capture backend (sentry-native) ───────────────────────────────────
# Out-of-process crash capture with real minidumps, replacing three divergent
# per-platform paths (POSIX signal + backtrace, Windows SEH + MiniDumpWriteDump)
# with one contract. See wiki crash-evidence-pipeline / TASK-81.
#
# Why sentry-native's "native" backend and not Crashpad directly:
#   * No crashpad/breakpad submodule — those stay unfetched, so the licence
#     surface is sentry-native's MIT alone. Crashpad would add Apache-2.0, which
#     escalates against GPL-2.0-only consumers.
#   * SENTRY_TRANSPORT=none compiles out HTTP entirely: no libcurl, no WinHTTP,
#     nothing that can phone home. Crash evidence stays local, which is the
#     policy this project wants and also the smallest attack surface.
#   * Mach exception ports on macOS. This is the load-bearing reason. On
#     Ventura+/Apple Silicon a POSIX signal handler is not a reliable crash hook:
#     backtrace() inside one no longer yields a usable trace, and for some faults
#     the signal is never delivered at all. Consolidating the existing
#     hand-rolled signal path would therefore have bought very little on macOS.
#   * Out-of-process by construction (a small `sentry-crash` daemon), so the
#     "handler must not deadlock or recurse into allocation" requirement is
#     satisfied structurally rather than by careful coding in a fault handler.
#
# Measured before adopting: builds offline from the pinned submodule with no
# network access at configure time, on macOS arm64 (libsentry.dylib 325 KB +
# sentry-crash 336 KB) and on Debian arm64 under Docker (libsentry.so +
# sentry-crash, 4 MB build tree).
#
# Default ON. It was OFF while the engine-side adapter was being built, so a
# half-wired backend could never displace the working per-platform handlers.
# Both conditions that kept it off are now met: the adapter is in place, and
# the vendor patch below stops sentry from dropping our own handler out of the
# chain — so enabling it ADDS a minidump rather than trading the JSON report
# and the playtest timeline away for one.
#
# This is what makes minidump production uniform. Windows used to carry its own
# in-engine MiniDumpWriteDump path while macOS and Linux had none, so "is there
# a dump?" had a different answer per platform and per configuration. That
# writer is gone; sentry is the single producer everywhere.
#
# Cost, measured on macOS arm64 (headless, arena7, each figure repeated):
#   engine RSS   88.5 MB with sentry vs 90.0 MB without — no increase; the
#                difference is noise, and the 0.99 MB crash context is SHARED
#                memory that lands mostly on the daemon's side
#   daemon RSS   ~6 MB, in a SEPARATE process, so the engine's own memory
#                budget (arenas, VM, GC) is untouched
#   footprint    531 KB macOS / 1.2 MB Windows, most of it the daemon binary
#   network      none: SENTRY_TRANSPORT=none, dumps are local files only
OPTION(USE_SENTRY_CRASH "Use sentry-native for crash capture" ON)
IF(NOT USE_SENTRY_CRASH)
	ADD_COMPILE_DEFINITIONS(FEAT_SENTRY_CRASH=0)
ELSEIF(NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/src/libs/sentry-native/CMakeLists.txt)
	MESSAGE(FATAL_ERROR
		"USE_SENTRY_CRASH=ON but the submodule is absent: "
		"git submodule update --init src/libs/sentry-native")
ELSE()
	ADD_COMPILE_DEFINITIONS(FEAT_SENTRY_CRASH=1)

	# sentry's own signal handler saves the previously installed one and then
	# never calls it, terminating via raise() instead. Enabling sentry therefore
	# SILENTLY DISABLED our handler: the JSON crash report and the playtest
	# timeline both stopped being written, leaving only a minidump, and nothing
	# in the log said why. Measured on macOS, Linux and Windows before the patch.
	# The patch chains to the saved handler immediately before raise().
	# See SUBMODULE-PATCHES.md.
	WIRED_APPLY_SUBMODULE_PATCHES(sentry-native src/libs/sentry-native patches/sentry-native)

	SET(SENTRY_BACKEND        "native" CACHE STRING "" FORCE)
	SET(SENTRY_TRANSPORT      "none"   CACHE STRING "" FORCE)
	SET(SENTRY_BUILD_TESTS    OFF      CACHE BOOL   "" FORCE)
	SET(SENTRY_BUILD_EXAMPLES OFF      CACHE BOOL   "" FORCE)
	# Same BUILD_TESTING shadowing as zlib-ng and googletest: sentry-native reads
	# the global option for its own suite.
	SET(_WIRED_SAVED_BUILD_TESTING ${BUILD_TESTING})
	SET(BUILD_TESTING OFF)
	ADD_SUBDIRECTORY(src/libs/sentry-native EXCLUDE_FROM_ALL)
	SET(BUILD_TESTING ${_WIRED_SAVED_BUILD_TESTING})
	UNSET(_WIRED_SAVED_BUILD_TESTING)

	# The umbrella dependency on qcommon_engine_shared only orders THAT target
	# against the verifier; sibling targets built from the same patched sources
	# still run in parallel, so a failed verification can leave stale unpatched
	# objects behind even though the build as a whole stops. Ordering sentry's
	# own targets too keeps the failure clean rather than merely loud.
	FOREACH(_sentry_tgt sentry sentry-crash)
		IF(TARGET ${_sentry_tgt})
			ADD_DEPENDENCIES(${_sentry_tgt} wired_verify_patches_sentry-native)
		ENDIF()
	ENDFOREACH()

	TARGET_LINK_LIBRARIES(qcommon                PRIVATE sentry)
	TARGET_LINK_LIBRARIES(qcommon_ded            PRIVATE sentry)
	TARGET_LINK_LIBRARIES(qcommon_engine_shared  PRIVATE sentry)
ENDIF()

# ── Recast/Detour navmesh (replaces AAS when USE_RECAST_NAVMESH=ON) ──
OPTION(USE_RECAST_NAVMESH "Use Recast/Detour/DetourCrowd navmesh (replaces AAS)" ON)
IF(NOT USE_RECAST_NAVMESH)
	ADD_COMPILE_DEFINITIONS(FEAT_RECAST_NAVMESH=0)
ELSE()
	# Detour carries Wired-specific extensions that nav_impl.cpp calls directly
	# (reconnectUnlinkedOffMeshStarts, reconnectSplitFloorSeams,
	# isPreferredWalkInFloor, getReachablePolyCount, finalizeOmcEndpoints).
	# They are kept as a patch, not a fork — see SUBMODULE-PATCHES.md.
	# Unlike the picoquic set this is NOT platform-gated: without it the
	# engine does not compile anywhere.
	WIRED_APPLY_SUBMODULE_PATCHES(recastnavigation src/libs/recastnavigation patches/recastnavigation)

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

	WIRED_ROUTE_QCOMMON_SOURCES(${NAV_CPP_SRCS} ${NAV_C_SRCS})

	TARGET_INCLUDE_DIRECTORIES(qcommon     PRIVATE ${RECAST_INCLUDE_DIRS})
	TARGET_INCLUDE_DIRECTORIES(qcommon_ded PRIVATE ${RECAST_INCLUDE_DIRS})
	TARGET_INCLUDE_DIRECTORIES(qcommon_engine_shared PRIVATE ${RECAST_INCLUDE_DIRS})

	TARGET_LINK_LIBRARIES(qcommon     PRIVATE recast_nav)
	TARGET_LINK_LIBRARIES(qcommon_ded PRIVATE recast_nav)
	TARGET_LINK_LIBRARIES(qcommon_engine_shared PRIVATE recast_nav)

	IF(BUILD_TESTING)
		# Unit test — direct axis/handedness contract plus round-trip identity.
		# Run with: ctest -R nav_coord_contract
		ADD_EXECUTABLE(nav_coord_test
			tests/nav_coord_test.c
			code/qcommon/nav/nav_coord.c
		)
		ADD_TEST(NAME nav_coord_contract COMMAND nav_coord_test)
		SET_TESTS_PROPERTIES(nav_coord_contract PROPERTIES
			LABELS "host-only;subsystem;owner.wired-nav;failure.contract;feature.recast"
			TIMEOUT 30
		)
		WIRED_CONFIGURE_HOST_TEST(nav_coord_test nav_coord_contract)
	ENDIF()
ENDIF()

# Wire the patch verification into the engine's own build. Placed AFTER every
# WIRED_APPLY_SUBMODULE_PATCHES call so the umbrella target has collected all of
# them; ADD_DEPENDENCIES resolves late, so attaching it to a library declared
# far earlier is fine.
#
# qcommon_engine_shared is the attachment point because both engine binaries
# link it, so there is no configuration in which the check is skipped. Without
# an edge like this the verifier would only run when asked for by name, which is
# precisely the "you have to remember" failure this replaces.
ADD_DEPENDENCIES(qcommon_engine_shared wired_verify_patches)

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

# client + Wired UI + miniaudio test
AUX_SOURCE_DIRECTORY(code/client CLIENT_SRCS)
list(FILTER CLIENT_SRCS EXCLUDE REGEX "cl_ui\\.c$")

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
# cURL support was removed 2026-08-10: the USE_CURL macro was never defined in
# the CMake era (cl_curl.c compiled to an empty TU), and in-game file download
# runs over WiredNet's reliable CHAN_DOWNLOAD QUIC channel — the HTTP-redirect
# path curl served is superseded (gamedata bootstrap belongs to the launcher).

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
	# CMake 4.4 added CMP0219 (macro arguments keep their backslashes). The
	# vendored libjpeg-turbo predates it and feeds a C snippet containing "\\n"
	# to check_c_source_runs(), so configure prints a long policy warning about
	# a file we do not maintain. OLD is also the behaviour that snippet was
	# written for — it wants the escapes interpreted — so pinning it keeps the
	# right-shift probe correct rather than merely silencing the message.
	# CMAKE_POLICY_DEFAULT_*, not CMAKE_POLICY(SET): the submodule calls
	# cmake_minimum_required() itself, which resets policies to that version's
	# defaults, so a SET here is discarded at the subdirectory boundary. The
	# DEFAULT_ variable survives it. Unset afterwards to keep the scope tight.
	SET(CMAKE_POLICY_DEFAULT_CMP0219 OLD)
	ADD_SUBDIRECTORY(src/libs/libjpeg-turbo EXCLUDE_FROM_ALL)
	UNSET(CMAKE_POLICY_DEFAULT_CMP0219)
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

# Pointer-free profile telemetry codec is shared by the GUI client's sender
# and the standalone tool receiver. It is deliberately not part of any
# renderer DLL; renderers expose only the in-process sample POD.
TARGET_SOURCES(client PRIVATE code/renderercommon/r_profile_telemetry.c)

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
LIST(REMOVE_ITEM RENDERER_COMMON_SRCS code/renderercommon/r_profile_telemetry.c)
LIST(REMOVE_ITEM RENDERER_COMMON_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/code/renderercommon/r_profile_telemetry.c)
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

# ── RAL — mandatory Vulkan renderer backend layer ──────────────────────────
# code/renderer/ral/        — public RAL surface (headers only)
# code/renderer/ral_vulkan/ — the active Vulkan backend
# Built as a standalone-linkable static lib and linked into the Vulkan renderer
# DLL alongside the renderervk host adapter. The archive itself has no `ri` or
# qcommon runtime imports; loader/surface/log services arrive through
# ralHostImports_t. RAL is not feature-gated: the static lib and live renderer
# consumers always build together. The renderer-owned Ral_Dump() adapter is
# exported and resolved by the client's command via Sys_LoadFunction.
SET(RAL_VK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/code/renderer/ral_vulkan")
SET(RAL_VK_SRCS
	${CMAKE_CURRENT_SOURCE_DIR}/code/renderer/ral/ral_buffer_map.c
	${CMAKE_CURRENT_SOURCE_DIR}/code/renderer/ral/ral_allocation.c
	${CMAKE_CURRENT_SOURCE_DIR}/code/renderer/ral/ral_backend_conformance.c
	${CMAKE_CURRENT_SOURCE_DIR}/code/renderer/ral/ral_capability.c
	${CMAKE_CURRENT_SOURCE_DIR}/code/renderer/ral/ral_command_lifecycle.c
	${CMAKE_CURRENT_SOURCE_DIR}/code/renderer/ral/ral_legacy_material.c
	${CMAKE_CURRENT_SOURCE_DIR}/code/renderer/ral/ral_memory_recovery.c
	${CMAKE_CURRENT_SOURCE_DIR}/code/renderer/ral/ral_shader_abi.c
	${CMAKE_CURRENT_SOURCE_DIR}/code/renderer/ral/ral_suballocation.c
	${CMAKE_CURRENT_SOURCE_DIR}/code/renderer/ral/ral_transient.c
	${CMAKE_CURRENT_SOURCE_DIR}/code/renderer/ral/ral_transient_texture.c
	${CMAKE_CURRENT_SOURCE_DIR}/code/renderer/ral/ral_transfer.c
	${CMAKE_CURRENT_SOURCE_DIR}/code/renderer/ral/ral_readback.c
	${CMAKE_CURRENT_SOURCE_DIR}/code/renderer/ral/ral_host.c
	${CMAKE_CURRENT_SOURCE_DIR}/code/renderer/ral/ral_profile.c
	${CMAKE_CURRENT_SOURCE_DIR}/code/renderer/ral/ral_residency.c
	${CMAKE_CURRENT_SOURCE_DIR}/code/renderer/ral/ral_temporal.c
	${CMAKE_CURRENT_SOURCE_DIR}/code/renderer/ral/ral_transition.c
	${RAL_VK_DIR}/ral_vulkan_backend.c
	${RAL_VK_DIR}/ral_vulkan_caps.c
	${RAL_VK_DIR}/ral_vulkan_memory.c
	${RAL_VK_DIR}/ral_vulkan_map.c
	${RAL_VK_DIR}/ral_vulkan_readback.c
	${RAL_VK_DIR}/ral_vulkan_translate.c
	${RAL_VK_DIR}/ral_vulkan_mrt_policy.c
	${RAL_VK_DIR}/ral_vulkan_resource.c
	${RAL_VK_DIR}/ral_vulkan_pipeline.c
	${RAL_VK_DIR}/ral_vulkan_command.c
	${RAL_VK_DIR}/ral_vulkan_transition.c
	${RAL_VK_DIR}/ral_vulkan_transient.c
	${RAL_VK_DIR}/ral_vulkan_sync.c
	${RAL_VK_DIR}/ral_vulkan_swapchain.c
	${RAL_VK_DIR}/ral_vulkan_query.c
)
ADD_LIBRARY(ral_vulkan STATIC ${RAL_VK_SRCS})
IF(BUILD_TESTING)
	# The symbol-policy CTest consumes the finished archive directly.  Keep the
	# aggregate host_tests target fail-closed even in a fresh build directory
	# where no renderer target has caused ral_vulkan to build yet.
	ADD_DEPENDENCIES(host_tests ral_vulkan)
ENDIF()
# This archive is force-loaded into the renderer SHARED library, so its
# objects must be PIC. macOS compiles everything PIC by default, which is
# why this was invisible locally — Linux ld fails the .so link without it
# ("recompile with -fPIC", first caught by the branch's first CI run).
SET_TARGET_PROPERTIES(ral_vulkan PROPERTIES POSITION_INDEPENDENT_CODE ON)
TARGET_INCLUDE_DIRECTORIES(ral_vulkan PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}/code/renderer/ral
	${CMAKE_CURRENT_SOURCE_DIR}/code/renderercommon
	${CMAKE_CURRENT_SOURCE_DIR}/code/qcommon)
IF(USE_RENDERER_DLOPEN)
	TARGET_COMPILE_DEFINITIONS(ral_vulkan PRIVATE USE_RENDERER_DLOPEN)
ENDIF()
IF(WIN32)
	# match the relaxation the renderer DLLs get (legacy const-correctness patterns)
	TARGET_COMPILE_OPTIONS(ral_vulkan PRIVATE -Wno-error=incompatible-pointer-types -Wno-error=int-conversion)
ENDIF()
IF(UNIX)
	FIND_PACKAGE(Threads)
	IF(Threads_FOUND)
		TARGET_LINK_LIBRARIES(ral_vulkan Threads::Threads)
	ENDIF()
ENDIF()

IF(APPLE AND BUILD_TESTING)
	ADD_EXECUTABLE(ral_backend_conformance_test
		tests/ral_backend_conformance_test.mm)
	TARGET_INCLUDE_DIRECTORIES(ral_backend_conformance_test PRIVATE
		code/qcommon code/renderer/ral code/renderer/ral_metal
		code/renderer/ral_vulkan code/renderercommon)
	TARGET_LINK_LIBRARIES(ral_backend_conformance_test PRIVATE
		ral_vulkan ral_metal SDL3::SDL3)
	TARGET_COMPILE_OPTIONS(ral_backend_conformance_test PRIVATE
		-Wall -Wextra -Werror)
	ADD_TEST(NAME ral_backend_conformance_contract
		COMMAND ral_backend_conformance_test)
	SET_TESTS_PROPERTIES(ral_backend_conformance_contract PROPERTIES
		LABELS "host-only;subsystem;owner.ral;failure.contract;backend.vulkan;backend.metal"
		TIMEOUT 30)
	WIRED_CONFIGURE_HOST_TEST(ral_backend_conformance_test
		ral_backend_conformance_contract)
ENDIF()

# Standalone Phase 7.12 presentation host. It renders the fixture-backed
# profile ImGui draw stream through the portable RAL while retaining the
# physical resize/minimize lifecycle gate. The target is developer-only and
# never enters the shipping GUI/headless/package graph.
IF(WIRED_BUILD_IMGUI_TOOLS)
	ADD_EXECUTABLE(wired_profile_host
		code/tools/profile_imgui/wired_profile_host_lifecycle.cpp
		code/tools/profile_imgui/wired_profile_host_main.cpp
		code/tools/profile_imgui/wired_profile_telemetry_udp.cpp
		code/renderercommon/r_profile_telemetry.c
	)
	TARGET_INCLUDE_DIRECTORIES(wired_profile_host PRIVATE
		code/tools/profile_imgui
		code/renderer/ral
		code/renderercommon
	)
	TARGET_LINK_LIBRARIES(wired_profile_host PRIVATE wired_profile_imgui ral_vulkan SDL3::SDL3)
	TARGET_COMPILE_FEATURES(wired_profile_host PRIVATE cxx_std_17)
ENDIF()

# Link ral_vulkan into a renderer target. --whole-archive forces every object
# into the link so Ral_Dump (which nothing else references) is exported from
# the DLL; also puts code/renderer/ral/ on the target's include path.
FUNCTION(WIRED_LINK_RAL_VULKAN _target)
	TARGET_INCLUDE_DIRECTORIES(${_target} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/code/renderer/ral)
	IF(APPLE)
		# Apple ld64 has no --whole-archive; -force_load takes the archive path
		# directly and is the exact equivalent for a single archive.
		TARGET_LINK_LIBRARIES(${_target} ral_vulkan)
		TARGET_LINK_OPTIONS(${_target} PRIVATE "LINKER:-force_load,$<TARGET_FILE:ral_vulkan>")
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
# explicitly enabled when the offline translators are required:
#   cmake -DWIRED_BUILD_SHADER_XLATE=ON -B build/debug
#   ninja -C build/debug shader_xlate
OPTION(WIRED_BUILD_SHADER_XLATE
	"Build the offline GLSL → SPIR-V → {MSL, GLSL, GLSL ES, WGSL} translator (needs src/libs/SPIRV-Cross submodule; WGSL needs the `naga` CLI on PATH — see BUILD-RUST.md)"
	OFF)
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
		SET(SPIRV_CROSS_ENABLE_REFLECT    ON  CACHE BOOL "" FORCE)
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
		TARGET_LINK_LIBRARIES(shader_xlate spirv-cross-msl spirv-cross-glsl spirv-cross-reflect spirv-cross-core)
		TARGET_INCLUDE_DIRECTORIES(shader_xlate PRIVATE "${SPIRV_CROSS_DIR}")
		TARGET_COMPILE_DEFINITIONS(shader_xlate PRIVATE
			WIRED_SPIRV_CROSS_REVISION="ad1118591a3150a2a871bf1de66884ddccbffd1d"
			WIRED_NAGA_VERSION="30.0.0+wired-portable-v1")
		SET_TARGET_PROPERTIES(shader_xlate PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)

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

# GL2: build the stringify tool and use it to generate fallback C strings from
# GLSL files. stringify RUNS ON THE BUILD MACHINE, so when cross-compiling it
# must be built with a host compiler — the target-built .exe cannot execute.
IF(CMAKE_CROSSCOMPILING)
	SET(STRINGIFY_BIN "${CMAKE_CURRENT_BINARY_DIR}/stringify_host")
	FIND_PROGRAM(WIRED_HOST_CC NAMES cc gcc clang REQUIRED
		DOC "host C compiler for build-machine tools in cross builds")
	ADD_CUSTOM_COMMAND(
		OUTPUT ${STRINGIFY_BIN}
		COMMAND ${WIRED_HOST_CC} -O2 ${CMAKE_CURRENT_SOURCE_DIR}/code/renderer2/stringify.c -o ${STRINGIFY_BIN}
		DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/code/renderer2/stringify.c
		COMMENT "Building host stringify (cross build)"
	)
	ADD_CUSTOM_TARGET(stringify_host_build DEPENDS ${STRINGIFY_BIN})
	SET(STRINGIFY_DEP stringify_host_build)
ELSE()
	ADD_EXECUTABLE(stringify code/renderer2/stringify.c)
	SET(STRINGIFY_BIN stringify)
	SET(STRINGIFY_DEP stringify)
ENDIF()

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
		COMMAND ${STRINGIFY_BIN} ${GLSL_FILE} ${GENERATED_C}
		DEPENDS ${STRINGIFY_DEP} ${GLSL_FILE}
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
	#
	# That case has arrived: Apple ld64 rejects -Map outright (its own map flag
	# is `-map <file>`, and the format it emits is not what the symbolication
	# tooling reads). Skip the map on Apple rather than emit one nothing can
	# consume; macOS crash traces symbolicate via atos/dSYM instead.
	IF(NOT APPLE)
		TARGET_LINK_OPTIONS(${RENDERER_PREFIX}_vulkan${RENDEXT} PRIVATE "LINKER:-Map=${RENDERER_PREFIX}_vulkan${RENDEXT}.map")
	ENDIF()
	# Mandatory Vulkan RAL backend used by the live renderer consumers.
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
	IF(WIN32)
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
		code/unix/unix_shared.c)

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
	SET(EXE_TYPE_CLIENT WIN32) # GUI subsystem (-mwindows)
	SET(EXE_TYPE_DED    "")    # Console subsystem (default, no flag)
	# MinGW: windres needs the source root to resolve relative paths in .rc files
	SET(CMAKE_RC_FLAGS "${CMAKE_RC_FLAGS} -I${CMAKE_SOURCE_DIR}")
	# GCC 14+ treats incompatible-pointer-types as error; Quake3 codebase uses
	# legacy char**/const char** patterns that are safe but technically incorrect.
	ADD_COMPILE_OPTIONS(-Wno-error=incompatible-pointer-types -Wno-error=int-conversion)

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

# Standalone SDL↔Metal host adapters are linked only into focused Metal
# hosts. The product owns its main SDL window through sdl_glimp.c and must
# never compile either private-window adapter into the engine executable.
LIST(FILTER WINDOW_SYSTEM_SRCS EXCLUDE REGEX
	"sdl_(metal_ral|ral_presentation)\\.mm$")
ADD_LIBRARY(windowSystem OBJECT ${WINDOW_SYSTEM_SRCS})
TARGET_SOURCES(windowSystem PRIVATE code/renderer/ral/ral_presentation_host.c)

IF(NOT TARGET SDL3::SDL3)
	find_package(SDL3 REQUIRED)
ENDIF()
TARGET_LINK_LIBRARIES(windowSystem SDL3::SDL3)

IF(APPLE AND USE_RENDERER_DLOPEN)
	IF(NOT TARGET ral_metal)
		FIND_LIBRARY(WIRED_METAL_FRAMEWORK Metal REQUIRED)
		FIND_LIBRARY(WIRED_FOUNDATION_FRAMEWORK Foundation REQUIRED)
		FIND_LIBRARY(WIRED_QUARTZCORE_FRAMEWORK QuartzCore REQUIRED)
		FIND_LIBRARY(WIRED_COREGRAPHICS_FRAMEWORK CoreGraphics REQUIRED)
		ADD_LIBRARY(ral_metal STATIC
			code/renderer/ral_metal/ral_metal_core.mm
			code/renderer/ral_metal/ral_metal_bind_group.mm
			code/renderer/ral_metal/ral_metal_render.mm
			code/renderer/ral_metal/ral_metal_pipeline.mm
			code/renderer/ral_metal/ral_metal_draw.mm
			code/renderer/ral_metal/ral_metal_present.mm
			code/renderer/ral/ral_allocation.c
			code/renderer/ral/ral_backend_conformance.c
			code/renderer/ral/ral_capability.c
			code/renderer/ral/ral_command_lifecycle.c
			code/renderer/ral/ral_frame_shell.c
			code/renderer/ral/ral_presentation_host.c
			code/renderer/ral/ral_memory_recovery.c
			code/renderer/ral/ral_shader_abi.c
			code/renderer/ral/ral_transfer.c)
		TARGET_INCLUDE_DIRECTORIES(ral_metal PUBLIC code/qcommon code/renderer/ral
			code/renderer/ral_metal code/renderercommon)
		TARGET_LINK_LIBRARIES(ral_metal PUBLIC ${WIRED_METAL_FRAMEWORK}
			${WIRED_FOUNDATION_FRAMEWORK} ${WIRED_QUARTZCORE_FRAMEWORK}
			${WIRED_COREGRAPHICS_FRAMEWORK})
		TARGET_COMPILE_OPTIONS(ral_metal PRIVATE -Wall -Wextra -Werror -fblocks)
	ENDIF()
	IF(NOT TARGET ral_metal_sdl)
		ADD_LIBRARY(ral_metal_sdl STATIC code/sdl/sdl_metal_ral.mm)
		TARGET_INCLUDE_DIRECTORIES(ral_metal_sdl PUBLIC code/qcommon code/renderer/ral
			code/renderer/ral_metal code/renderercommon code/sdl)
		TARGET_LINK_LIBRARIES(ral_metal_sdl PUBLIC ral_metal SDL3::SDL3)
		TARGET_COMPILE_OPTIONS(ral_metal_sdl PRIVATE -Wall -Wextra -Werror)
	ENDIF()
	IF(NOT TARGET ral_sdl_presentation_host)
		ADD_LIBRARY(ral_sdl_presentation_host STATIC
			code/sdl/sdl_ral_presentation.mm)
		TARGET_INCLUDE_DIRECTORIES(ral_sdl_presentation_host PUBLIC code/qcommon
			code/renderer/ral code/renderer/ral_metal code/renderercommon code/sdl)
		TARGET_LINK_LIBRARIES(ral_sdl_presentation_host PUBLIC ral_metal SDL3::SDL3)
		TARGET_COMPILE_OPTIONS(ral_sdl_presentation_host PRIVATE -Wall -Wextra -Werror)
	ENDIF()
	ADD_LIBRARY(${RENDERER_PREFIX}_metal${RENDEXT} SHARED
		code/renderer/ral_metal/ral_metal_module.mm)
	# The renderer boundary itself is a native-free C ABI. Metal and SDL
	# ownership remain in their Objective-C++ libraries below that boundary.
	SET_SOURCE_FILES_PROPERTIES(code/renderer/ral_metal/ral_metal_module.mm
		PROPERTIES LANGUAGE C)
	TARGET_INCLUDE_DIRECTORIES(${RENDERER_PREFIX}_metal${RENDEXT} PRIVATE
		code/qcommon code/renderer/ral code/renderer/ral_metal
		code/renderercommon code/sdl)
	TARGET_LINK_LIBRARIES(${RENDERER_PREFIX}_metal${RENDEXT} PRIVATE ral_metal)
	TARGET_COMPILE_DEFINITIONS(${RENDERER_PREFIX}_metal${RENDEXT} PRIVATE
		USE_RENDERER_DLOPEN)
	TARGET_COMPILE_OPTIONS(${RENDERER_PREFIX}_metal${RENDEXT} PRIVATE
		-Wall -Wextra -Werror)
	SET_TARGET_PROPERTIES(${RENDERER_PREFIX}_metal${RENDEXT} PROPERTIES PREFIX "")
	IF(WIRED_HOST_SANITIZERS)
		FOREACH(_ral_metal_sanitized_target ral_metal ral_metal_sdl
			ral_sdl_presentation_host
			${RENDERER_PREFIX}_metal${RENDEXT})
			TARGET_COMPILE_OPTIONS(${_ral_metal_sanitized_target} PRIVATE
				-fsanitize=address,undefined -fno-omit-frame-pointer
				-fno-sanitize-recover=all)
		ENDFOREACH()
		TARGET_LINK_OPTIONS(${RENDERER_PREFIX}_metal${RENDEXT} PRIVATE
			-fsanitize=address,undefined -fno-omit-frame-pointer
			-fno-sanitize-recover=all)
	ENDIF()
	IF(BUILD_TESTING)
		ADD_EXECUTABLE(ral_metal_module_test tests/ral_metal_module_test.mm)
		SET_SOURCE_FILES_PROPERTIES(tests/ral_metal_module_test.mm
			PROPERTIES LANGUAGE C)
		ADD_DEPENDENCIES(ral_metal_module_test ${RENDERER_PREFIX}_metal${RENDEXT})
		TARGET_INCLUDE_DIRECTORIES(ral_metal_module_test PRIVATE code/qcommon
			code/renderer/ral code/renderer/ral_metal code/renderercommon code/sdl)
		TARGET_LINK_LIBRARIES(ral_metal_module_test PRIVATE
			ral_sdl_presentation_host)
		TARGET_COMPILE_DEFINITIONS(ral_metal_module_test PRIVATE USE_RENDERER_DLOPEN
			RAL_METAL_TEST_MODULE="$<TARGET_FILE:${RENDERER_PREFIX}_metal${RENDEXT}>")
		TARGET_COMPILE_OPTIONS(ral_metal_module_test PRIVATE -Wall -Wextra -Werror)
		ADD_TEST(NAME ral_metal_module_contract COMMAND ral_metal_module_test)
		SET_TESTS_PROPERTIES(ral_metal_module_contract PROPERTIES
			LABELS "host-only;subsystem;owner.ral;failure.contract;backend.metal;module.renderer"
			TIMEOUT 30)
		WIRED_CONFIGURE_HOST_TEST(ral_metal_module_test ral_metal_module_contract)
		ADD_TEST(NAME ral_metal_module_link_policy_contract
			COMMAND ${CMAKE_COMMAND}
				-DBINARY=$<TARGET_FILE:${RENDERER_PREFIX}_metal${RENDEXT}>
				-P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/ral_metal_module_link_policy_check.cmake)
		SET_TESTS_PROPERTIES(ral_metal_module_link_policy_contract PROPERTIES
			LABELS "host-only;infrastructure;owner.ral;failure.link-policy;backend.metal;module.renderer"
			TIMEOUT 30)
	ENDIF()
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
TARGET_SOURCES(${CNAME}${BINEXT} PRIVATE $<TARGET_OBJECTS:qcommon_engine_shared>)
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
TARGET_SOURCES(${DNAME}${BINEXT} PRIVATE $<TARGET_OBJECTS:qcommon_engine_shared>)
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
	# Built in a per-build-dir COPY of the tree, not in the submodule itself.
	# The old in-source build made libluajit.a a shared mutable artifact: every
	# build dir (release/debug/...) linked the same file, and the `make clean`
	# each build started with destroyed it for all of them. The copy also lets
	# us drop the clean (LuaJIT's own make is mtime-incremental) and build -j —
	# the in-source form was the slowest serial edge in the whole build (~11s).
	SET(LUAJIT_BUILD_DIR ${CMAKE_BINARY_DIR}/luajit)
	SET(LUAJIT_LIB ${LUAJIT_BUILD_DIR}/src/libluajit.a)

	# Platform-specific build flags
	SET(LUAJIT_XCFLAGS "")
	SET(LUAJIT_MAKE_ENV "")
	IF(APPLE)
		SET(LUAJIT_MAKE_ENV ${CMAKE_COMMAND} -E env MACOSX_DEPLOYMENT_TARGET=10.15)
		IF(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
			SET(LUAJIT_XCFLAGS "-DLUAJIT_ENABLE_GC64")
		ENDIF()
	ENDIF()
	# Cross-compile (llvm-mingw): LuaJIT builds host minilua/buildvm with
	# HOST_CC and the target library with $(CROSS)gcc; TARGET_SYS overrides
	# the uname-based host detection (which otherwise demands
	# MACOSX_DEPLOYMENT_TARGET on a mac host).
	SET(LUAJIT_CROSS_ARGS "")
	IF(CMAKE_CROSSCOMPILING AND CMAKE_SYSTEM_NAME STREQUAL "Windows")
		SET(LUAJIT_MAKE_ENV ${CMAKE_COMMAND} -E env MACOSX_DEPLOYMENT_TARGET=11.0)
		SET(LUAJIT_CROSS_ARGS HOST_CC=cc TARGET_SYS=Windows "CROSS=${LLVM_MINGW_ROOT}/bin/x86_64-w64-mingw32-")
	ENDIF()

	INCLUDE(ProcessorCount)
	PROCESSORCOUNT(_LUAJIT_JOBS)
	IF(_LUAJIT_JOBS EQUAL 0)
		SET(_LUAJIT_JOBS 4)
	ENDIF()

	# DEPENDS on the LuaJIT sources so a submodule bump or a patch actually
	# rebuilds the library — the old command had no inputs at all and only
	# ever fired when the .a was missing. copy_directory only refreshes files
	# whose content changed, so LuaJIT's own mtime-based make stays incremental
	# across reruns.
	# HAZARD: never run make IN-TREE under src/libs/luajit — leftover host .o
	# files get copied here and poison other-platform builds (lld: "unknown
	# file type"). If that happens: git -C src/libs/luajit clean -fdx.
	FILE(GLOB_RECURSE LUAJIT_SOURCES
		${LUAJIT_DIR}/src/*.c ${LUAJIT_DIR}/src/*.h
		${LUAJIT_DIR}/src/*.lua ${LUAJIT_DIR}/src/Makefile ${LUAJIT_DIR}/Makefile)
	add_custom_command(
		OUTPUT ${LUAJIT_LIB}
		COMMAND ${CMAKE_COMMAND} -E copy_directory ${LUAJIT_DIR} ${LUAJIT_BUILD_DIR}
		COMMAND ${LUAJIT_MAKE_ENV} make -j${_LUAJIT_JOBS} -C ${LUAJIT_BUILD_DIR}/src BUILDMODE=static "XCFLAGS=${LUAJIT_XCFLAGS}" ${LUAJIT_CROSS_ARGS}
		DEPENDS ${LUAJIT_SOURCES}
		COMMENT "Building LuaJIT (static, -j${_LUAJIT_JOBS})"
	)
	add_custom_target(luajit_build DEPENDS ${LUAJIT_LIB})

	add_library(luajit STATIC IMPORTED GLOBAL)
	set_target_properties(luajit PROPERTIES IMPORTED_LOCATION ${LUAJIT_LIB})
	add_dependencies(luajit luajit_build)

	# Headless Lua runtime lives in qcommon/wired/core/scripting/ — used by both client and headless server
	AUX_SOURCE_DIRECTORY(code/qcommon/wired/core/scripting QCOMMON_SCRIPTING_SRCS)
	WIRED_ROUTE_QCOMMON_SOURCES(${QCOMMON_SCRIPTING_SRCS})

	# LuaJIT headers needed by qcommon (both targets) and client
	TARGET_INCLUDE_DIRECTORIES(qcommon PRIVATE ${LUAJIT_DIR}/src)
	TARGET_INCLUDE_DIRECTORIES(qcommon_ded PRIVATE ${LUAJIT_DIR}/src)
	TARGET_INCLUDE_DIRECTORIES(qcommon_engine_shared PRIVATE ${LUAJIT_DIR}/src)
	TARGET_INCLUDE_DIRECTORIES(client PRIVATE ${LUAJIT_DIR}/src)

	# Link LuaJIT to both client and headless server executables
	TARGET_LINK_LIBRARIES(${CNAME}${BINEXT} luajit)
	TARGET_LINK_LIBRARIES(${DNAME}${BINEXT} luajit)

	IF(BUILD_TESTING)
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
	SET_TESTS_PROPERTIES(wired_scene PROPERTIES
		LABELS "host-only;subsystem;owner.wired-scene;failure.contract;feature.lua"
		TIMEOUT 30
	)
	WIRED_CONFIGURE_HOST_TEST(wired_scene_test wired_scene)
	ENDIF()

	MESSAGE(STATUS "LuaJIT scripting enabled")
ENDIF()

#TARGET_LINK_OPTIONS(${DNAME} PRIVATE -fno-pie) # v3.13+

IF(WIN32)
	TARGET_LINK_LIBRARIES(${CNAME}${BINEXT} winmm comctl32 ws2_32 iphlpapi)
	TARGET_LINK_LIBRARIES(${DNAME}${BINEXT} winmm comctl32 ws2_32 iphlpapi)
	IF(USE_WASM)
		# WAMR's WASI libc (win_file.c, random.c) needs pathcch + bcrypt
		TARGET_LINK_LIBRARIES(${CNAME}${BINEXT} pathcch bcrypt)
		TARGET_LINK_LIBRARIES(${DNAME}${BINEXT} pathcch bcrypt)
	ENDIF()
	# MinGW: statically link runtime libs (libwinpthread, libgcc, libstdc++)
	# so end users don't need MinGW DLLs in PATH
	TARGET_LINK_OPTIONS(${CNAME}${BINEXT} PRIVATE -static)
	TARGET_LINK_OPTIONS(${DNAME}${BINEXT} PRIVATE -static)
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
	# rpath: find bundled dylibs (SDL3, MoltenVK, libcrypto) in the same dir as
	# the executable. Must be set as TARGET properties: the CMAKE_INSTALL_RPATH
	# variable only seeds targets created AFTER it, and both executables already
	# exist by this point in the file — the variable form was silently a no-op
	# (verified: the shipped binary's only LC_RPATH was /opt/homebrew/lib).
	SET_TARGET_PROPERTIES(${CNAME}${BINEXT} PROPERTIES
		INSTALL_RPATH "@executable_path"
		BUILD_WITH_INSTALL_RPATH ON
	)
	IF(TARGET ${DNAME}${BINEXT})
		SET_TARGET_PROPERTIES(${DNAME}${BINEXT} PROPERTIES
			INSTALL_RPATH "@executable_path"
			BUILD_WITH_INSTALL_RPATH ON
		)
	ENDIF()
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

	# OUTPUT-based command with a stamp file so ninja can reach a true no-op.
	# The old form was an ALL custom target with no OUTPUT: ninja considered it
	# perpetually dirty, re-assembled the bundle on every build, and a settled
	# tree could never answer "did the build do work?" — the same
	# signal-integrity failure class as make's unreliable exit code.
	SET(_BUNDLE_STAMP "${CMAKE_BINARY_DIR}/CMakeFiles/macos_app_bundle.stamp")
	file(GLOB_RECURSE _BUNDLE_LICENSE_FILES CONFIGURE_DEPENDS
		"${CMAKE_SOURCE_DIR}/LICENSES/*")
	add_custom_command(
		OUTPUT "${_BUNDLE_STAMP}"
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
		COMMAND ${CMAKE_COMMAND} -E copy
			"${CMAKE_SOURCE_DIR}/LICENSE"
			"${APP_BUNDLE_DIR}/Contents/Resources/LICENSE"
		COMMAND ${CMAKE_COMMAND} -E copy
			"${CMAKE_SOURCE_DIR}/THIRD_PARTY_LICENSES.md"
			"${APP_BUNDLE_DIR}/Contents/Resources/THIRD_PARTY_LICENSES.md"
		COMMAND ${CMAKE_COMMAND} -E rm -rf
			"${APP_BUNDLE_DIR}/Contents/Resources/LICENSES"
		COMMAND ${CMAKE_COMMAND} -E copy_directory
			"${CMAKE_SOURCE_DIR}/LICENSES"
			"${APP_BUNDLE_DIR}/Contents/Resources/LICENSES"
		COMMAND ${CMAKE_COMMAND} -E echo_append "APPL????"
			> "${APP_BUNDLE_DIR}/Contents/PkgInfo"
		DEPENDS ${CNAME}${BINEXT} ${DNAME}${BINEXT}
			"$<TARGET_FILE:${CNAME}${BINEXT}>"
			"$<TARGET_FILE:${DNAME}${BINEXT}>"
			"${CMAKE_SOURCE_DIR}/code/unix/quake3_flat.icns"
			"${CMAKE_SOURCE_DIR}/LICENSE"
			"${CMAKE_SOURCE_DIR}/THIRD_PARTY_LICENSES.md"
			${_BUNDLE_LICENSE_FILES}
		COMMENT "Assembling macOS bundle: ${APP_BUNDLE_NAME}${BINEXT}.app"
		VERBATIM
	)
	IF(USE_RENDERER_DLOPEN)
		# The client resolves renderer modules beside the executable. Make them
		# first-class bundle inputs: otherwise the OUTPUT stamp can stay newer than
		# a rebuilt dylib and the app silently runs an obsolete renderer.
		add_custom_command(
			OUTPUT "${_BUNDLE_STAMP}"
			APPEND
			COMMAND ${CMAKE_COMMAND} -E copy
				"$<TARGET_FILE:${RENDERER_PREFIX}_opengl${RENDEXT}>"
				"${APP_BUNDLE_DIR}/Contents/MacOS/$<TARGET_FILE_NAME:${RENDERER_PREFIX}_opengl${RENDEXT}>"
			COMMAND ${CMAKE_COMMAND} -E copy
				"$<TARGET_FILE:${RENDERER_PREFIX}_opengl2${RENDEXT}>"
				"${APP_BUNDLE_DIR}/Contents/MacOS/$<TARGET_FILE_NAME:${RENDERER_PREFIX}_opengl2${RENDEXT}>"
			COMMAND ${CMAKE_COMMAND} -E copy
				"$<TARGET_FILE:${RENDERER_PREFIX}_vulkan${RENDEXT}>"
				"${APP_BUNDLE_DIR}/Contents/MacOS/$<TARGET_FILE_NAME:${RENDERER_PREFIX}_vulkan${RENDEXT}>"
			DEPENDS
				${RENDERER_PREFIX}_opengl${RENDEXT}
				${RENDERER_PREFIX}_opengl2${RENDEXT}
				${RENDERER_PREFIX}_vulkan${RENDEXT}
				"$<TARGET_FILE:${RENDERER_PREFIX}_opengl${RENDEXT}>"
				"$<TARGET_FILE:${RENDERER_PREFIX}_opengl2${RENDEXT}>"
				"$<TARGET_FILE:${RENDERER_PREFIX}_vulkan${RENDEXT}>"
		)
	ENDIF()
	# Publish the stamp only after every required bundle input has copied.  If a
	# renderer copy fails, leaving an older/missing stamp forces the next build to
	# retry instead of accepting a partially assembled app as up to date.
	add_custom_command(
		OUTPUT "${_BUNDLE_STAMP}"
		APPEND
		COMMAND ${CMAKE_COMMAND} -E touch "${_BUNDLE_STAMP}"
	)
	add_custom_target(macos_app_bundle ALL DEPENDS "${_BUNDLE_STAMP}")
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
	TARGET_COMPILE_DEFINITIONS(qcommon_engine_shared PRIVATE Q3NOW_GAMEDATE="${Q3NOW_GAMEDATE}")
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

# Materialize the exact engine partition after every conditional source cohort
# has been routed. qcommon_shared_graph_contract consumes these configure-time
# manifests and rejects missing, duplicated, or mis-owned compile edges.
GET_PROPERTY(_QCOMMON_SHARED_MANIFEST GLOBAL PROPERTY QCOMMON_SHARED_SOURCE_INVENTORY)
GET_PROPERTY(_QCOMMON_VARIANT_MANIFEST GLOBAL PROPERTY QCOMMON_VARIANT_SOURCE_INVENTORY)
LIST(REMOVE_DUPLICATES _QCOMMON_SHARED_MANIFEST)
LIST(REMOVE_DUPLICATES _QCOMMON_VARIANT_MANIFEST)
FOREACH(_manifest_kind SHARED VARIANT)
	SET(_manifest_values ${_QCOMMON_${_manifest_kind}_MANIFEST})
	IF(_manifest_kind STREQUAL "SHARED")
		SET(_manifest_path "${CMAKE_BINARY_DIR}/qcommon_engine_shared_sources.txt")
	ELSE()
		SET(_manifest_path "${CMAKE_BINARY_DIR}/qcommon_engine_variant_sources.txt")
	ENDIF()
	FILE(WRITE "${_manifest_path}" "")
	FOREACH(_manifest_src IN LISTS _manifest_values)
		IF(IS_ABSOLUTE "${_manifest_src}")
			SET(_manifest_abs "${_manifest_src}")
		ELSE()
			SET(_manifest_abs "${CMAKE_SOURCE_DIR}/${_manifest_src}")
		ENDIF()
		FILE(APPEND "${_manifest_path}" "${_manifest_abs}\n")
	ENDFOREACH()
ENDFOREACH()

ADD_SUBDIRECTORY(tools/extract-meta)

# Targets whose sources call into libm. Collected here, after every target
# exists, so the list is one place rather than a rule rediscovered per target
# from a Linux-only linker error. See WIRED_LINK_MATH for why this is not
# needed on macOS or Windows.
WIRED_LINK_MATH(
	ral_temporal_test
	vk_temporal_resolve_authority_test
	vk_temporal_resolve_readback_test
	tr_temporal_motion_test
	particle_curve_test
	nav_coord_test
)
