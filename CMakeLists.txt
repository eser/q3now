CMAKE_MINIMUM_REQUIRED(VERSION 3.14)

PROJECT(q3now LANGUAGES C CXX ASM)

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

SET(CNAME "q3now")
SET(DNAME "q3now-ded")

SET(RENDERER_PREFIX ${CNAME})
SET(RENDERER_DEFAULT vulkan) # valid options: opengl, vulkan, opengl2; opengl as ultimate fallback

OPTION(USE_SDL "" ON)
OPTION(USE_CURL "" ON)
OPTION(USE_OPENGL "" ON)
OPTION(USE_VULKAN "" ON)
OPTION(USE_SYSTEM_JPEG "" OFF)
OPTION(USE_JPEG_TURBO "" ON)
OPTION(USE_ZLIB_NG "" ON)
OPTION(USE_RENDERER_DLOPEN "" ON)
OPTION(USE_WASM "Enable WASM VM backend via WAMR" OFF)
OPTION(USE_LUA "Enable LuaJIT scripting" ON)

SET(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

AUX_SOURCE_DIRECTORY(code/qcommon QCOMMON_SRCS)
AUX_SOURCE_DIRECTORY(code/qcommon/maps QCOMMON_SRCS)

# Arena allocator (must be explicit — AUX_SOURCE_DIRECTORY is configure-time only)
list(APPEND QCOMMON_SRCS code/qcommon/arena.c)

# LZ4 decoder (for SW3Z archive reader)
list(APPEND QCOMMON_SRCS src/libs/lz4/lz4.c src/libs/lz4/lz4frame.c src/libs/lz4/xxhash.c src/libs/lz4/lz4hc.c)

# When WASM is disabled, override FEAT_WASM=0 so vm_wasm.c compiles to empty
# and vm.c skips WASM code paths (q_feats.h hardcodes FEAT_WASM=1).
IF(NOT USE_WASM)
	ADD_COMPILE_DEFINITIONS(FEAT_WASM=0)
ENDIF()

# MinGW: picoquic + picotls use bare `#ifdef _WINDOWS` (MSVC-only auto-define)
# to gate winsock includes. Inject _WINDOWS so they take the Windows code
# path; vendored sources stay untouched. Definition is innocuous for q3now's
# own code (q3now uses _WIN32 throughout). Applied globally before any target
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

# bg_weapons.c, bg_misc.c and bg_gametypes.c go into qcommon so the dedicated
# server can access bg_weaponlist, bg_itemlist and bg_gametypelist
# (e.g. for MCP tools like game.items, coaching.tick)
list(APPEND QCOMMON_SRCS code/game/bg_weapons.c code/game/bg_misc.c code/game/bg_gametypes.c)

# universal binary
ADD_LIBRARY(qcommon OBJECT ${QCOMMON_SRCS} ${SERVER_SRCS})

# dedicated server
ADD_LIBRARY(qcommon_ded OBJECT ${QCOMMON_SRCS} ${SERVER_SRCS})
TARGET_COMPILE_DEFINITIONS(qcommon_ded PUBLIC DEDICATED)

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

# wired/core/shell — log pipeline (console/tty/file sinks)
FILE(GLOB WIRED_CON_SRCS  code/qcommon/wired/core/shell/*.c)
TARGET_SOURCES(qcommon     PRIVATE ${WIRED_CON_SRCS})
TARGET_SOURCES(qcommon_ded PRIVATE ${WIRED_CON_SRCS})

# wired/core/events — event primitive stubs (V2 bodies TBD)
FILE(GLOB WIRED_EVENT_SRCS code/qcommon/wired/core/events/*.c)
TARGET_SOURCES(qcommon     PRIVATE ${WIRED_EVENT_SRCS})
TARGET_SOURCES(qcommon_ded PRIVATE ${WIRED_EVENT_SRCS})

# wired/core/time — time subsystem (Sys_NanoTime, Sys_Milliseconds, Com_RealTime, Com_FormatTimestamp)
FILE(GLOB WIRED_TIME_SRCS  code/qcommon/wired/core/time/*.c)
TARGET_SOURCES(qcommon     PRIVATE ${WIRED_TIME_SRCS})
TARGET_SOURCES(qcommon_ded PRIVATE ${WIRED_TIME_SRCS})

# wired/core — orchestration layer (WiredCore_Init / WiredCore_Shutdown)
FILE(GLOB WIRED_CORE_SRCS code/qcommon/wired/core/*.c)
TARGET_SOURCES(qcommon     PRIVATE ${WIRED_CORE_SRCS})
TARGET_SOURCES(qcommon_ded PRIVATE ${WIRED_CORE_SRCS})

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

# Add to qcommon (client) and qcommon_ded (dedicated server)
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
AUX_SOURCE_DIRECTORY(code/client/wired/hud         CLIENT_SRCS)
AUX_SOURCE_DIRECTORY(code/client/wired/hud/elements CLIENT_SRCS)
AUX_SOURCE_DIRECTORY(code/client/wired/ui/panels   CLIENT_SRCS)
# bg_misc.c, bg_weapons.c and bg_gametypes.c are in QCOMMON_SRCS (shared with dedicated server); not duplicated here
IF(NOT USE_CURL)
	list(REMOVE_ITEM CLIENT_SRCS code/client/cl_curl.c)
ENDIF()

# x86_64 sound mixing ASM (GAS syntax — works with GCC on both Linux and MinGW)
IF(CMAKE_SYSTEM_PROCESSOR MATCHES "AMD64|x86_64")
	IF(NOT MSVC)
		list(APPEND CLIENT_SRCS code/asm/snd_mix_x86_64.s)
		# The .s file uses C preprocessor macros (#ifdef, C()) and needs
		# preprocessing. -DELF avoids underscore prefix (correct for x86_64 ABI).
		set_source_files_properties(code/asm/snd_mix_x86_64.s PROPERTIES
			COMPILE_FLAGS "-x assembler-with-cpp -DELF")
	ENDIF()
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
		"if(FALSE) # q3now: add_subdirectory() guard neutralized"
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

# renderers
AUX_SOURCE_DIRECTORY(code/renderercommon RENDERER_COMMON_SRCS)
AUX_SOURCE_DIRECTORY(code/renderer RENDERER_GL_SRCS)
AUX_SOURCE_DIRECTORY(code/renderer2 RENDERER_GL2_SRCS)
AUX_SOURCE_DIRECTORY(code/renderervk RENDERER_VK_SRCS)

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
	ELSE()
		ADD_LIBRARY(${RENDERER_PREFIX}_opengl OBJECT ${RENDERER_GL_SRCS} ${RENDERER_COMMON_SRCS})
	ENDIF()
ENDIF()

# common function, window system
IF(UNIX)
	SET (Q3_SRCS
		code/unix/unix_main.c
		code/unix/unix_shared.c
		code/unix/linux_signals.c)

	IF(USE_SDL)
		AUX_SOURCE_DIRECTORY(code/sdl WINDOW_SYSTEM_SRCS)
	ELSE()
		# Note: audio is now handled by code/client/snd_miniaudio.c (vendored
		# miniaudio.h), not by a per-platform SNDDMA backend. linux_snd.c was
		# deleted along with sdl_snd.c and win_snd.c.
		SET(WINDOW_SYSTEM_SRCS
			code/unix/linux_glimp.c
			code/unix/linux_qgl.c
			code/unix/x11_dga.c
			code/unix/x11_randr.c
			code/unix/x11_vidmode.c)
		IF(USE_VULKAN)
			list(APPEND WINDOW_SYSTEM_SRCS code/unix/linux_qvk.c)
		ENDIF()
	ENDIF()

	IF(APPLE)
		SET(EXE_TYPE MACOSX_BUNDLE)
		list(APPEND Q3_SRCS code/unix/quake3_flat.icns)
		set_source_files_properties(code/unix/quake3_flat.icns PROPERTIES
			MACOSX_PACKAGE_LOCATION Resources)
		SET(MACOSX_BUNDLE_ICON_FILE quake3_flat.icns)
		SET(MACOSX_BUNDLE_GUI_IDENTIFIER "com.eser.q3now")
		SET(MACOSX_BUNDLE_BUNDLE_NAME "q3now")
		list(APPEND Q3_SRCS code/macosx/macosx_main.c)
	ENDIF()
ELSEIF(WIN32)
	# Per-target subsystem: client is GUI (game has a window), dedicated is
	# console (Linux-style stdio so smoke harnesses, NSSM-services, Docker
	# logs, terminal redirection etc. all see stdout/stderr). See
	# docs/health.md "q3now-ded Windows GUI-subsystem blocks smoke output capture"
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
	# single stdio shim shared by client and dedicated — no per-target
	# split needed for it. Client adds win_console_attach.c on top
	# (AttachConsole shim for the GUI-subsystem binary).
	SET(Q3_SRCS
		code/win32/win_main.c
		code/win32/win_shared.c
		code/win32/win_syscon.c
		code/win32/win_resource.rc)

	# Note: audio is now handled by code/client/snd_miniaudio.c (vendored
	# miniaudio.h), not by a per-platform SNDDMA backend. win_snd.c was
	# deleted along with sdl_snd.c and linux_snd.c.
	SET(WINDOW_SYSTEM_SRCS
		code/win32/win_gamma.c
		code/win32/win_glimp.c
		code/win32/win_input.c
		code/win32/win_minimize.c
		code/win32/win_qgl.c
		code/win32/win_wndproc.c)
	IF(USE_VULKAN)
		list(APPEND WINDOW_SYSTEM_SRCS code/win32/win_qvk.c)
	ENDIF()
ENDIF(UNIX)

# Define _DEBUG for Debug builds so signal handlers, backtraces, and
# developer diagnostics compile in (guarded by #ifdef _DEBUG in code).
ADD_COMPILE_DEFINITIONS($<$<CONFIG:Debug>:_DEBUG>)

ADD_LIBRARY(windowSystem OBJECT ${WINDOW_SYSTEM_SRCS})

IF(NOT MSVC)
	IF(USE_SDL)
		find_package(SDL3 REQUIRED)
		TARGET_LINK_LIBRARIES(windowSystem SDL3::SDL3)
	ELSEIF(UNIX)
		find_package (X11 REQUIRED)
		TARGET_LINK_LIBRARIES(windowSystem ${X11_LIBRARIES})
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
TARGET_LINK_LIBRARIES(${CNAME}${BINEXT} qcommon botlib client windowSystem opusfile)
IF(WIN32)
	# Client adds the AttachConsole shim (phase 2 of Windows-console
	# modernization, 2026-05-05) — reattaches stdio to parent console when
	# this GUI-subsystem binary is launched from a terminal. Dedicated
	# doesn't need it (console subsystem auto-attaches at process start).
	# The shared win_syscon.c is in Q3_SRCS, linked into both targets.
	TARGET_SOURCES(${CNAME}${BINEXT} PRIVATE code/win32/win_console_attach.c)
ENDIF()
#TARGET_LINK_OPTIONS(${CNAME} PRIVATE -fno-pie) # v3.13+

IF(NOT USE_RENDERER_DLOPEN)
	IF(USE_VULKAN)
		TARGET_LINK_LIBRARIES(${CNAME}${BINEXT} ${RENDERER_PREFIX}_vulkan)
	ELSE()
		TARGET_LINK_LIBRARIES(${CNAME}${BINEXT} ${RENDERER_PREFIX}_opengl)
	ENDIF()
ENDIF()

# dedicated server executable

ADD_EXECUTABLE(${DNAME}${BINEXT} ${_DED_EXE_TYPE} ${Q3_SRCS})
TARGET_LINK_LIBRARIES(${DNAME}${BINEXT} qcommon_ded botlib)
# Dedicated uses the same win_syscon.c (in Q3_SRCS) as client; the
# Sys_ConsoleInput stdin reader path is gated #ifdef DEDICATED so only
# the dedicated build compiles it.

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

	# Headless Lua runtime lives in qcommon/wired/core/scripting/ — used by both client and dedicated server
	AUX_SOURCE_DIRECTORY(code/qcommon/wired/core/scripting QCOMMON_SCRIPTING_SRCS)
	TARGET_SOURCES(qcommon PRIVATE ${QCOMMON_SCRIPTING_SRCS})
	TARGET_SOURCES(qcommon_ded PRIVATE ${QCOMMON_SCRIPTING_SRCS})

	# LuaJIT headers needed by qcommon (both targets) and client
	TARGET_INCLUDE_DIRECTORIES(qcommon PRIVATE ${LUAJIT_DIR}/src)
	TARGET_INCLUDE_DIRECTORIES(qcommon_ded PRIVATE ${LUAJIT_DIR}/src)
	TARGET_INCLUDE_DIRECTORIES(client PRIVATE ${LUAJIT_DIR}/src)

	# Link LuaJIT to both client and dedicated server executables
	TARGET_LINK_LIBRARIES(${CNAME}${BINEXT} luajit)
	TARGET_LINK_LIBRARIES(${DNAME}${BINEXT} luajit)

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

	# miniaudio platform link dependencies (client only — dedicated server
	# never compiles snd_miniaudio.c thanks to #ifndef DEDICATED).
	IF(APPLE)
		# miniaudio uses CoreAudio + AudioToolbox on macOS.
		FIND_LIBRARY(COREAUDIO_LIBRARY CoreAudio)
		FIND_LIBRARY(AUDIOTOOLBOX_LIBRARY AudioToolbox)
		FIND_LIBRARY(COREFOUNDATION_LIBRARY CoreFoundation)
		TARGET_LINK_LIBRARIES(${CNAME}${BINEXT}
			${COREAUDIO_LIBRARY}
			${AUDIOTOOLBOX_LIBRARY}
			${COREFOUNDATION_LIBRARY})
	ELSE()
		# miniaudio dlopens ALSA / PulseAudio / JACK at runtime, so the
		# only hard link deps on Linux are pthread, dl (already via
		# CMAKE_DL_LIBS above), and m (already linked above).
		FIND_PACKAGE(Threads REQUIRED)
		TARGET_LINK_LIBRARIES(${CNAME}${BINEXT} Threads::Threads)
	ENDIF()
ENDIF()

# macOS: ARM64 JIT requires entitlements for MAP_JIT and executable memory
IF(APPLE)
	SET_TARGET_PROPERTIES(${CNAME}${BINEXT} PROPERTIES
		XCODE_ATTRIBUTE_CODE_SIGN_ENTITLEMENTS "${CMAKE_SOURCE_DIR}/misc/macos/q3now.entitlements"
	)
	# rpath: find bundled dylibs (SDL3, MoltenVK) in the same dir as the executable
	SET(CMAKE_INSTALL_RPATH "@executable_path")
	SET(CMAKE_BUILD_WITH_INSTALL_RPATH ON)
ENDIF()

# ============================================================
# q3now game modules (cgame, qagame, ui)
# ============================================================

OPTION(BUILD_GAME_LIBRARIES "Build game native shared libraries (.dylib/.so)" ON)

IF(BUILD_GAME_LIBRARIES)
	SET(SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/code)
	SET(BASEGAME "baseq3")
	SET(CGAME_MODULE "cgame")
	SET(GAME_MODULE "qagame")
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
	# Release channel suffix (e.g. "-preview", "-canary", or "" for public)
	IF(DEFINED CHANNEL_SUFFIX)
		ADD_COMPILE_DEFINITIONS(CHANNEL_SUFFIX="${CHANNEL_SUFFIX}")
	ELSE()
		ADD_COMPILE_DEFINITIONS(CHANNEL_SUFFIX="-preview")
	ENDIF()
	INCLUDE(basegame)
ENDIF()
