CMAKE_MINIMUM_REQUIRED(VERSION 3.14)

PROJECT(q3now LANGUAGES C CXX ASM)

SET(CNAME "q3now")
SET(DNAME "q3now-ded")

SET(RENDERER_PREFIX ${CNAME})
SET(RENDERER_DEFAULT vulkan) # valid options: opengl, vulkan, opengl2; opengl as ultimate fallback

OPTION(USE_SDL "" ON)
OPTION(USE_CURL "" ON)
OPTION(USE_OPENGL "" ON)
OPTION(USE_VULKAN "" ON)
OPTION(USE_SYSTEM_JPEG "" OFF)
OPTION(USE_RENDERER_DLOPEN "" ON)
OPTION(USE_WASM "Enable WASM VM backend via WAMR" OFF)
OPTION(USE_QUIC "Enable QUIC transport via picoquic (FEAT_QUIC_TRANSPORT)" OFF)
OPTION(USE_QUIC_OBSERVE "Enable QUIC game state observation (FEAT_QUIC_OBSERVE, implies USE_QUIC)" OFF)
OPTION(USE_QUIC_CONTROL "Enable QUIC MCP/command channel (FEAT_QUIC_CONTROL, implies USE_QUIC_OBSERVE)" OFF)
OPTION(USE_QUIC_HTTP "Enable QUIC HTTP endpoints (FEAT_QUIC_HTTP, implies USE_QUIC)" OFF)

SET(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

AUX_SOURCE_DIRECTORY(code/qcommon QCOMMON_SRCS)

# LZ4 decoder (for SW3Z archive reader)
list(APPEND QCOMMON_SRCS code/libs/lz4/lz4.c code/libs/lz4/lz4frame.c code/libs/lz4/xxhash.c code/libs/lz4/lz4hc.c)

# exclude platform-dependent QVM bytecode compilers
list(FILTER QCOMMON_SRCS EXCLUDE REGEX ".*vm_[alxp].*.c")
# When WASM is disabled, override FEAT_WASM=0 so vm_wasm.c compiles to empty
# and vm.c skips WASM code paths (q_feats.h hardcodes FEAT_WASM=1).
IF(NOT USE_WASM)
	ADD_COMPILE_DEFINITIONS(FEAT_WASM=0)
ENDIF()
# add platform-dependent QVM bytecode compilers
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
	list(APPEND QCOMMON_SRCS code/qcommon/vm_x86.c)
ELSEIF(CMAKE_SYSTEM_PROCESSOR MATCHES aarch64|arm64)
	IF(APPLE)
		SET(BINEXT .arm64)
		SET(RENDEXT _arm64)
	ELSE()
		SET(BINEXT .aarch64)
		SET(RENDEXT _aarch64)
	ENDIF()
	list(APPEND QCOMMON_SRCS code/qcommon/vm_aarch64.c)
ELSEIF(CMAKE_SYSTEM_PROCESSOR MATCHES arm*)
	SET(BINEXT .arm)
	SET(RENDEXT _arm)
	list(APPEND QCOMMON_SRCS code/qcommon/vm_armv7l.c)
ELSEIF(CMAKE_SYSTEM_PROCESSOR MATCHES "ppc64le|ppc64")
	IF(CMAKE_SYSTEM_PROCESSOR MATCHES "ppc64le")
		SET(BINEXT .ppc64le)
		SET(RENDEXT _ppc64le)
		list(APPEND QCOMMON_SRCS code/qcommon/vm_powerpc.c)
	ELSE()
		SET(BINEXT .ppc64)
		SET(RENDEXT _ppc64)
		list(APPEND QCOMMON_SRCS code/qcommon/vm_powerpc.c)
	ENDIF()
ELSE()
	TARGET_COMPILE_DEFINITIONS(qcommon PRIVATE -DNO_VM_COMPILED)
	TARGET_COMPILE_DEFINITIONS(qcommon_ded PRIVATE -DNO_VM_COMPILED)
ENDIF()

# botlib
AUX_SOURCE_DIRECTORY(code/botlib BOTLIB_SRCS)
ADD_LIBRARY(botlib OBJECT ${BOTLIB_SRCS})
TARGET_COMPILE_DEFINITIONS(botlib PRIVATE BOTLIB)

AUX_SOURCE_DIRECTORY(code/server SERVER_SRCS)
list(REMOVE_ITEM SERVER_SRCS code/server/sv_rankings.c)

# universal binary
ADD_LIBRARY(qcommon OBJECT ${QCOMMON_SRCS} ${SERVER_SRCS})

# dedicated server
ADD_LIBRARY(qcommon_ded OBJECT ${QCOMMON_SRCS} ${SERVER_SRCS})
TARGET_COMPILE_DEFINITIONS(qcommon_ded PUBLIC DEDICATED)

# LZ4 include path (for SW3Z archive reader)
TARGET_INCLUDE_DIRECTORIES(qcommon PRIVATE code/libs/lz4)
TARGET_INCLUDE_DIRECTORIES(qcommon_ded PRIVATE code/libs/lz4)

# WAMR (WebAssembly Micro Runtime) for WASM VM backend
IF(USE_WASM)
	SET(WAMR_BUILD_INTERP 1)
	SET(WAMR_BUILD_AOT 1)
	SET(WAMR_BUILD_JIT 0)
	SET(WAMR_BUILD_LIBC_BUILTIN 0)
	SET(WAMR_BUILD_LIBC_WASI 1)
	SET(WAMR_BUILD_SHARED_MEMORY 0)
	SET(WAMR_BUILD_MULTI_MODULE 0)
	SET(WAMR_BUILD_REF_TYPES 1)
	SET(WAMR_BUILD_SIMD 0)
	SET(WAMR_DISABLE_HW_BOUND_CHECK 1)
	SET(WAMR_BUILD_LOAD_CUSTOM_SECTION 1)
	ADD_SUBDIRECTORY(code/libs/wamr)
	IF(WIN32)
		# WAMR upstream hardcodes -lm -ldl (line 154) without platform guard.
		# Windows has no libdl — strip it from vmlib's transitive link interface.
		get_target_property(_vmlib_libs vmlib INTERFACE_LINK_LIBRARIES)
		IF(_vmlib_libs)
			list(REMOVE_ITEM _vmlib_libs "-ldl" "-lm")
			set_target_properties(vmlib PROPERTIES INTERFACE_LINK_LIBRARIES "${_vmlib_libs}")
		ENDIF()
	ENDIF()
	TARGET_INCLUDE_DIRECTORIES(qcommon PRIVATE code/libs/wamr/core/iwasm/include)
	TARGET_INCLUDE_DIRECTORIES(qcommon_ded PRIVATE code/libs/wamr/core/iwasm/include)
ENDIF()

IF(NO_VM_COMPILED)
	TARGET_COMPILE_DEFINITIONS(qcommon PRIVATE NO_VM_COMPILED)
	TARGET_COMPILE_DEFINITIONS(qcommon_ded PRIVATE NO_VM_COMPILED)
ENDIF()

# ── QUIC Transport (picoquic) ─────────────────────────────────────
# Layered: USE_QUIC_CONTROL → USE_QUIC_OBSERVE → USE_QUIC
# Each higher layer auto-enables its prerequisites.
IF(USE_QUIC_CONTROL)
	SET(USE_QUIC_OBSERVE ON)
ENDIF()
IF(USE_QUIC_OBSERVE)
	SET(USE_QUIC ON)
ENDIF()
IF(USE_QUIC_HTTP)
	SET(USE_QUIC ON)
ENDIF()

IF(USE_QUIC)
	# Feature flag defines — passed to compiler, override q_feats.h defaults
	ADD_COMPILE_DEFINITIONS(FEAT_QUIC_TRANSPORT=1)
	IF(USE_QUIC_OBSERVE)
		ADD_COMPILE_DEFINITIONS(FEAT_QUIC_OBSERVE=1)
	ENDIF()
	IF(USE_QUIC_CONTROL)
		ADD_COMPILE_DEFINITIONS(FEAT_QUIC_CONTROL=1)
	ENDIF()
	IF(USE_QUIC_HTTP)
		ADD_COMPILE_DEFINITIONS(FEAT_QUIC_HTTP=1)
	ENDIF()

	# picoquic + picotls + mpack sources
	# These are added as git submodules under code/libs/
	FILE(GLOB PICOQUIC_SRCS code/libs/picoquic/picoquic/*.c)

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
		code/libs/picotls/lib/picotls.c
		code/libs/picotls/lib/openssl.c
		code/libs/picotls/lib/pembase64.c
		code/libs/picotls/lib/asn1.c
		code/libs/picotls/lib/hpke.c
	)

	FILE(GLOB MPACK_SRCS    code/libs/mpack/src/mpack/*.c)

	# webtransport module — our integration code
	FILE(GLOB WT_SRCS code/webtransport/*.c)

	SET(QUIC_ALL_SRCS ${PICOQUIC_SRCS} ${PICOTLS_SRCS} ${MPACK_SRCS} ${WT_SRCS})

	# Include paths
	SET(QUIC_INCLUDE_DIRS
		code/libs/picoquic/picoquic
		code/libs/picotls/include
		code/libs/mpack/src/mpack
		code/webtransport
	)

	# OpenSSL (required by picotls for crypto primitives)
	FIND_PACKAGE(OpenSSL REQUIRED)

	# Add to qcommon (client) and qcommon_ded (dedicated server)
	TARGET_SOURCES(qcommon PRIVATE ${QUIC_ALL_SRCS})
	TARGET_INCLUDE_DIRECTORIES(qcommon PRIVATE ${QUIC_INCLUDE_DIRS})
	TARGET_LINK_LIBRARIES(qcommon PRIVATE OpenSSL::Crypto)

	TARGET_SOURCES(qcommon_ded PRIVATE ${QUIC_ALL_SRCS})
	TARGET_INCLUDE_DIRECTORIES(qcommon_ded PRIVATE ${QUIC_INCLUDE_DIRS})
	TARGET_LINK_LIBRARIES(qcommon_ded PRIVATE OpenSSL::Crypto)

	MESSAGE(STATUS "QUIC transport enabled (picoquic)")
	IF(USE_QUIC_OBSERVE)
		MESSAGE(STATUS "  + QUIC observation (game state + events)")
	ENDIF()
	IF(USE_QUIC_CONTROL)
		MESSAGE(STATUS "  + QUIC control (MCP + commands)")
	ENDIF()
	IF(USE_QUIC_HTTP)
		MESSAGE(STATUS "  + QUIC HTTP (/health, /metrics)")
	ENDIF()
ELSE()
	# When QUIC is disabled, override all FEAT_QUIC flags to 0
	# (q_feats.h defaults are already 0, but this ensures consistency)
	ADD_COMPILE_DEFINITIONS(FEAT_QUIC_TRANSPORT=0)
ENDIF()

# client + cURL + Wired UI
AUX_SOURCE_DIRECTORY(code/client CLIENT_SRCS)
AUX_SOURCE_DIRECTORY(code/client/wired CLIENT_SRCS)
# bg_misc.c needed by Wired UI HUD elements (BG_FindItemForPowerup, bg_itemlist)
list(APPEND CLIENT_SRCS code/game/bg_misc.c)
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

# jpeg
IF(USE_SYSTEM_JPEG)
	ADD_LIBRARY(client OBJECT ${CLIENT_SRCS})
	find_package(JPEG REQUIRED)
	TARGET_COMPILE_DEFINITIONS(client PRIVATE -DUSE_SYSTEM_JPEG)
	TARGET_LINK_LIBRARIES(client ${JPEG_LIBRARIES})
ELSE()
	AUX_SOURCE_DIRECTORY(code/libjpeg JPEG_SRCS)
	#ADD_LIBRARY(jpeg ${JPEG_SRCS})
	#TARGET_LINK_LIBRARIES(client jpeg)
	ADD_LIBRARY(client OBJECT ${CLIENT_SRCS} ${JPEG_SRCS})
ENDIF()

# renderers
AUX_SOURCE_DIRECTORY(code/renderercommon RENDERER_COMMON_SRCS)
AUX_SOURCE_DIRECTORY(code/renderer RENDERER_GL_SRCS)
AUX_SOURCE_DIRECTORY(code/renderervk RENDERER_VK_SRCS)

IF(USE_RENDERER_DLOPEN)
	SET(AUX_SRCS
		code/qcommon/q_shared.c
		code/qcommon/puff.c
		code/qcommon/q_math.c
	)
	TARGET_COMPILE_DEFINITIONS(client PRIVATE USE_RENDERER_DLOPEN RENDERER_PREFIX="${RENDERER_PREFIX}" RENDERER_DEFAULT=${RENDERER_DEFAULT})

	ADD_LIBRARY(${RENDERER_PREFIX}_opengl${RENDEXT} SHARED ${RENDERER_GL_SRCS} ${RENDERER_COMMON_SRCS} ${AUX_SRCS})
	TARGET_COMPILE_DEFINITIONS(${RENDERER_PREFIX}_opengl${RENDEXT} PRIVATE USE_RENDERER_DLOPEN)
	SET_TARGET_PROPERTIES(${RENDERER_PREFIX}_opengl${RENDEXT} PROPERTIES PREFIX "")

	ADD_LIBRARY(${RENDERER_PREFIX}_vulkan${RENDEXT} SHARED ${RENDERER_VK_SRCS} ${RENDERER_COMMON_SRCS} ${AUX_SRCS})
	TARGET_COMPILE_DEFINITIONS(${RENDERER_PREFIX}_vulkan${RENDEXT} PRIVATE USE_RENDERER_DLOPEN)
	SET_TARGET_PROPERTIES(${RENDERER_PREFIX}_vulkan${RENDEXT} PROPERTIES PREFIX "")

	# Linux: renderer shared libs need libm for math functions (powf etc.)
	# and are dlopen'd at runtime, so allow host-process symbols to remain undefined.
	IF(UNIX AND NOT APPLE)
		TARGET_LINK_LIBRARIES(${RENDERER_PREFIX}_opengl${RENDEXT} m)
		TARGET_LINK_LIBRARIES(${RENDERER_PREFIX}_vulkan${RENDEXT} m)
		TARGET_LINK_OPTIONS(${RENDERER_PREFIX}_opengl${RENDEXT} PRIVATE "LINKER:--unresolved-symbols=ignore-in-object-files")
		TARGET_LINK_OPTIONS(${RENDERER_PREFIX}_vulkan${RENDEXT} PRIVATE "LINKER:--unresolved-symbols=ignore-in-object-files")
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
		AUX_SOURCE_DIRECTORY(code/sdl Q3_UI_SRCS)
	ELSE()
		SET(Q3_UI_SRCS
			code/unix/linux_glimp.c
			code/unix/linux_qgl.c
			code/unix/linux_snd.c
			code/unix/x11_dga.c
			code/unix/x11_randr.c
			code/unix/x11_vidmode.c)
		IF(USE_VULKAN)
			list(APPEND Q3_UI_SRCS code/unix/linux_qvk.c)
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
	ENDIF()
ELSEIF(WIN32)
	SET(EXE_TYPE WIN32)
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

	SET(Q3_SRCS
		code/win32/win_main.c
		code/win32/win_shared.c
		code/win32/win_syscon.c
		code/win32/win_resource.rc)

	SET(Q3_UI_SRCS
		code/win32/win_gamma.c
		code/win32/win_glimp.c
		code/win32/win_input.c
		code/win32/win_minimize.c
		code/win32/win_qgl.c
		code/win32/win_snd.c
		code/win32/win_wndproc.c)
	IF(USE_VULKAN)
		list(APPEND Q3_UI_SRCS code/win32/win_qvk.c)
	ENDIF()
ENDIF(UNIX)

# Define _DEBUG for Debug builds so signal handlers, backtraces, and
# developer diagnostics compile in (guarded by #ifdef _DEBUG in code).
ADD_COMPILE_DEFINITIONS($<$<CONFIG:Debug>:_DEBUG>)

ADD_LIBRARY(q3ui OBJECT ${Q3_UI_SRCS})

IF(NOT MSVC)
	IF(USE_SDL)
		find_package(SDL3 REQUIRED)
		TARGET_LINK_LIBRARIES(q3ui SDL3::SDL3)
	ELSEIF(UNIX)
		find_package (X11 REQUIRED)
		TARGET_LINK_LIBRARIES(q3ui ${X11_LIBRARIES})
	ENDIF()
ENDIF()

IF(USE_OPENGL)
	TARGET_COMPILE_DEFINITIONS(q3ui PRIVATE USE_OPENGL_API)
	TARGET_COMPILE_DEFINITIONS(client PRIVATE USE_OPENGL_API)
ENDIF()

IF(USE_VULKAN)
	TARGET_COMPILE_DEFINITIONS(q3ui PRIVATE USE_VULKAN_API)
	TARGET_COMPILE_DEFINITIONS(client PRIVATE USE_VULKAN_API)
ENDIF()

# client executable

ADD_EXECUTABLE(${CNAME}${BINEXT} ${EXE_TYPE} ${Q3_SRCS})
TARGET_LINK_LIBRARIES(${CNAME}${BINEXT} qcommon botlib client q3ui)
#TARGET_LINK_OPTIONS(${CNAME} PRIVATE -fno-pie) # v3.13+

IF(NOT USE_RENDERER_DLOPEN)
	IF(USE_VULKAN)
		TARGET_LINK_LIBRARIES(${CNAME}${BINEXT} ${RENDERER_PREFIX}_vulkan)
	ELSE()
		TARGET_LINK_LIBRARIES(${CNAME}${BINEXT} ${RENDERER_PREFIX}_opengl)
	ENDIF()
ENDIF()

# dedicated server executable

ADD_EXECUTABLE(${DNAME}${BINEXT} ${EXE_TYPE} ${Q3_SRCS})
TARGET_LINK_LIBRARIES(${DNAME}${BINEXT} qcommon_ded botlib)

# WAMR library linking
IF(USE_WASM)
	TARGET_LINK_LIBRARIES(${CNAME}${BINEXT} vmlib)
	TARGET_LINK_LIBRARIES(${DNAME}${BINEXT} vmlib)
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
OPTION(BUILD_GAME_QVMS "Build game QVM bytecode (.qvm)" OFF)

IF(BUILD_GAME_LIBRARIES OR BUILD_GAME_QVMS)
	SET(SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/code)
	SET(BASEGAME "baseq3")
	SET(CGAME_MODULE "cgame")
	SET(GAME_MODULE "qagame")
	SET(UI_MODULE "ui")
	IF(WIN32)
		SET(COMMON_LIBRARIES)
	ELSE()
		SET(COMMON_LIBRARIES m ${CMAKE_DL_LIBS})
	ENDIF()
	# PRODUCT_DATE: ioquake3 game code uses this for version stamping
	# Use target_compile_definitions instead of add_compile_definitions to avoid
	# passing the date string (with spaces) to windres on Windows, which breaks.
	STRING(TIMESTAMP PRODUCT_DATE "%b %d %Y" UTC)
	TARGET_COMPILE_DEFINITIONS(qcommon PRIVATE PRODUCT_DATE="${PRODUCT_DATE}")
	TARGET_COMPILE_DEFINITIONS(qcommon_ded PRIVATE PRODUCT_DATE="${PRODUCT_DATE}")
	TARGET_COMPILE_DEFINITIONS(client PRIVATE PRODUCT_DATE="${PRODUCT_DATE}")
	INCLUDE(basegame)
ENDIF()
