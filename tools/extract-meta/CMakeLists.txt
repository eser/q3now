# tools/extract-meta — q3now CLI tool for generating .meta and .ent files.
#
# B1 scope: VFS bootstrap + map enumeration only. The actual inventory,
# resolution, and emission logic lands in B2.
#
# Self-contained build per B1.5 retry: this CMakeLists declares its
# OWN static library `qcommon_tool` from a curated subset of qcommon
# sources. The engine's existing `qcommon` (OBJECT) target — which
# the engine binaries actually consume — is not affected. Top-level
# CMakeLists.txt and code/* CMakeLists are intentionally untouched.
#
# Source list is a copy-paste-and-prune from the engine's
# QCOMMON_SRCS / WIRED_*_SRCS / lz4 list. Once copied, the tool's
# source list is self-owned: engine refactors don't silently break
# the tool, and tool experiments don't risk breaking the engine.

# ──────────────────────────────────────────────────────────────────
#  Source subset
# ──────────────────────────────────────────────────────────────────
# Excluded (intentionally) and why:
#   - msg.c                 → references cl_shownet (client cvar)
#   - net_ip.c              → references CL_* (client networking)
#   - cm_*.c                → BotDrawDebugPolygons (botlib coupling);
#                             tool only enumerates maps/*.bsp via FS,
#                             never traces collision geometry.
#   - vm.c, vm_wasm.c       → VM machinery + WAMR SDK
#   - wired/core/events/event.c       → CL_/SV_ event dispatch + Sys_ConsoleInput
#   - wired/core/logging/log_sink_console.c → CL_ConsolePrint
#   - wired/core/scripting/*          → Lua (USE_LUA path)
#   - wired/net/*                     → networking (picoquic + mpack + WN)
#   - code/game/bg_*.c                → game-side; uses VM trap_*
#   - nav/*                           → Recast navmesh

set(QCOMMON_TOOL_SRCS
	# qcommon top-level
	${CMAKE_SOURCE_DIR}/code/qcommon/arena.c
	${CMAKE_SOURCE_DIR}/code/qcommon/asset_load_log.c
	${CMAKE_SOURCE_DIR}/code/qcommon/common.c
	${CMAKE_SOURCE_DIR}/code/qcommon/crash.c
	${CMAKE_SOURCE_DIR}/code/qcommon/help.c
	${CMAKE_SOURCE_DIR}/code/qcommon/history.c
	${CMAKE_SOURCE_DIR}/code/qcommon/huffman.c
	${CMAKE_SOURCE_DIR}/code/qcommon/huffman_static.c
	${CMAKE_SOURCE_DIR}/code/qcommon/puff.c
	${CMAKE_SOURCE_DIR}/code/qcommon/q_color.c
	${CMAKE_SOURCE_DIR}/code/qcommon/q_math.c
	${CMAKE_SOURCE_DIR}/code/qcommon/q_shared.c
	${CMAKE_SOURCE_DIR}/code/qcommon/q_string.c
	${CMAKE_SOURCE_DIR}/code/qcommon/sw3z.c
	${CMAKE_SOURCE_DIR}/code/qcommon/unzip.c

	# maps/* — needed by B2 for BSP_Load + Meta_*. Pulling these in
	# also forces:
	#   - cm_q1.c (CMQ1_Store{Clipnodes,LeafContents} called from
	#     bsp_q1.c). cm_q1.c in turn references CM_Trace,
	#     CMQ3_PointContents, CM_PointLeafnum, CMQ3_BiSphereTrace,
	#     and the global `cm`, all from the rest of cm_*.c.
	#   - nav/nav_coord.c (Nav_QuakeToRecast called from bsp_q1/q3.c).
	# All cm_* sources are self-contained except cm_patch.c, which
	# references BotDrawDebugPolygons (botlib). That single symbol
	# is satisfied by a no-op stub in stubs.c.
	${CMAKE_SOURCE_DIR}/code/qcommon/cm_load.c
	${CMAKE_SOURCE_DIR}/code/qcommon/cm_patch.c
	${CMAKE_SOURCE_DIR}/code/qcommon/cm_polylib.c
	${CMAKE_SOURCE_DIR}/code/qcommon/cm_q1.c
	${CMAKE_SOURCE_DIR}/code/qcommon/cm_test.c
	${CMAKE_SOURCE_DIR}/code/qcommon/cm_trace.c
	${CMAKE_SOURCE_DIR}/code/qcommon/maps/bsp.c
	${CMAKE_SOURCE_DIR}/code/qcommon/maps/bsp_q1.c
	${CMAKE_SOURCE_DIR}/code/qcommon/maps/bsp_q3.c
	${CMAKE_SOURCE_DIR}/code/qcommon/maps/meta_aliases.c
	${CMAKE_SOURCE_DIR}/code/qcommon/maps/meta_parse.c
	${CMAKE_SOURCE_DIR}/code/qcommon/maps/meta_remap.c
	${CMAKE_SOURCE_DIR}/code/qcommon/maps/meta_scan.c
	${CMAKE_SOURCE_DIR}/code/qcommon/maps/meta_write.c
	${CMAKE_SOURCE_DIR}/code/qcommon/nav/nav_coord.c

	# util/
	${CMAKE_SOURCE_DIR}/code/qcommon/util/crypto.c
	${CMAKE_SOURCE_DIR}/code/qcommon/util/hash.c
	${CMAKE_SOURCE_DIR}/code/qcommon/util/math.c
	${CMAKE_SOURCE_DIR}/code/qcommon/util/md4.c
	${CMAKE_SOURCE_DIR}/code/qcommon/util/md5.c
	${CMAKE_SOURCE_DIR}/code/qcommon/util/sort.c
	${CMAKE_SOURCE_DIR}/code/qcommon/util/string.c

	# wired/core/* (excluding events/event.c, logging/log_sink_console.c,
	# core/core.c — the latter calls into events + scripting orchestration
	# which are already excluded; tool stubs WiredCore_Init/Shutdown +
	# WiredScript_PostInit instead)
	${CMAKE_SOURCE_DIR}/code/qcommon/wired/core/cvars/cvar.c
	${CMAKE_SOURCE_DIR}/code/qcommon/wired/core/logging/log.c
	${CMAKE_SOURCE_DIR}/code/qcommon/wired/core/logging/log_buffer.c
	${CMAKE_SOURCE_DIR}/code/qcommon/wired/core/logging/log_channels.c
	${CMAKE_SOURCE_DIR}/code/qcommon/wired/core/logging/log_sink_file.c
	${CMAKE_SOURCE_DIR}/code/qcommon/wired/core/logging/log_sink_tty.c
	${CMAKE_SOURCE_DIR}/code/qcommon/wired/core/shell/cmd.c
	${CMAKE_SOURCE_DIR}/code/qcommon/wired/core/shell/keys.c
	${CMAKE_SOURCE_DIR}/code/qcommon/wired/core/time/time.c
	${CMAKE_SOURCE_DIR}/code/qcommon/wired/core/vfs/files.c

	# Sound codec dispatcher (split TU). Provides S_CodecResolves for
	# offline asset auditing without dragging the codec implementations
	# (snd_codec_opus.c / _wav.c / _ogg.c) or the engine's S_CodecInit
	# (which lives in snd_codec_init.c — NOT in this list). The static
	# codec extension list inside snd_codec.c is what powers the probe.
	${CMAKE_SOURCE_DIR}/code/client/snd_codec.c

	# Image availability probe — same role as snd_codec.c above but
	# for textures. Mirrors R_FindImageFile's extension fallback
	# without dragging the renderer's image loaders (tr_image_*.c).
	${CMAKE_SOURCE_DIR}/code/renderercommon/r_image_probe.c

	# LZ4 (used by sw3z + frame compression)
	${CMAKE_SOURCE_DIR}/src/libs/lz4/lz4.c
	${CMAKE_SOURCE_DIR}/src/libs/lz4/lz4frame.c
	${CMAKE_SOURCE_DIR}/src/libs/lz4/lz4hc.c
	${CMAKE_SOURCE_DIR}/src/libs/lz4/xxhash.c
)

# ──────────────────────────────────────────────────────────────────
#  qcommon_tool — tool-private static archive
# ──────────────────────────────────────────────────────────────────

ADD_LIBRARY(qcommon_tool STATIC ${QCOMMON_TOOL_SRCS})

TARGET_INCLUDE_DIRECTORIES(qcommon_tool PRIVATE
	${CMAKE_SOURCE_DIR}/code/qcommon
	${CMAKE_SOURCE_DIR}/code/qcommon/util
	${CMAKE_SOURCE_DIR}/code/qcommon/wired/net
	${CMAKE_SOURCE_DIR}/src/libs/lz4
)
# Notes on the include dirs:
#  - `code/qcommon/wired/net` is here NOT because the tool uses
#    networking sources (it doesn't), but because several engine
#    sources (logging/log.c, shell/keys.c) use `../../../foo` style
#    relative includes that only resolve correctly when an
#    additional `-I` puts them at the right depth. The engine's
#    qcommon target gets this same `-I` for free via WIREDNET. We
#    keep nothing else from wired/net — its sources are not in
#    QCOMMON_TOOL_SRCS.
#  - Do NOT add `code/qcommon/wired/core/time` (or any
#    wired/core/<x>) as an include dir — its `time.h` would shadow
#    the system `<time.h>`. Wired sub-headers should be included via
#    their full relative path (`wired/core/time/time.h`).

# Mirror engine qcommon's defines, with FEAT_WASM forced off because
# qcommon_tool excludes vm_wasm.c (so WAMR symbols can't link).
TARGET_COMPILE_DEFINITIONS(qcommon_tool PRIVATE
	CHANNEL_SUFFIX="${CHANNEL_SUFFIX}"
	PRODUCT_NAME="${PRODUCT_NAME}"
	Q3NOW_GAMEDATE="${Q3NOW_GAMEDATE}"
	FEAT_WASM=0
	NO_VM_COMPILED
)

# zlib-ng for unzip.c (matches engine qcommon when USE_ZLIB_NG=ON).
IF(USE_ZLIB_NG)
	TARGET_INCLUDE_DIRECTORIES(qcommon_tool PRIVATE
		${CMAKE_SOURCE_DIR}/src/libs/zlib-ng
		${CMAKE_BINARY_DIR}/src/libs/zlib-ng)
	TARGET_LINK_LIBRARIES(qcommon_tool PUBLIC zlibstatic)
ELSE()
	TARGET_COMPILE_DEFINITIONS(qcommon_tool PRIVATE FEAT_LEGACY_ZLIB)
ENDIF()

# OpenSSL is referenced by qcommon's util/crypto.c only via the SHA256
# helpers it implements directly — not via libcrypto. No link needed.

# ──────────────────────────────────────────────────────────────────
#  extract-meta executable
# ──────────────────────────────────────────────────────────────────
#
# EXCLUDE_FROM_ALL: tool builds only on explicit request (`cmake
# --build ... --target extract-meta`) so the engine build pipeline
# stays untouched even while the tool bootstrap is iterated.

ADD_EXECUTABLE(extract-meta EXCLUDE_FROM_ALL
	main.c
	tool_init.c
	stubs.c
	# B2 modules
	category_map.c
	shader_index.c
	bsp_inventory.c
	asset_resolve.c
	meta_emit.c
	ent_emit.c
)

TARGET_LINK_LIBRARIES(extract-meta PRIVATE qcommon_tool)

TARGET_INCLUDE_DIRECTORIES(extract-meta PRIVATE
	${CMAKE_SOURCE_DIR}/code/qcommon
	${CMAKE_SOURCE_DIR}/code/qcommon/util
	${CMAKE_SOURCE_DIR}/code/client          # snd_codec.h (S_CodecResolves)
	${CMAKE_SOURCE_DIR}/code/renderercommon  # r_image_probe.h (R_ImageResolves)
)

# Platform OS link surface (the tool ships its own minimal Sys_*
# stubs — see stubs.c — but the underlying win32 APIs they wrap
# need their import libs).
IF(WIN32)
	TARGET_LINK_LIBRARIES(extract-meta PRIVATE
		kernel32 user32 advapi32 shell32 ole32 bcrypt
		winmm comctl32 ws2_32 pathcch)
ENDIF()
