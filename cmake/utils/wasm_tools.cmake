include_guard(GLOBAL)

if(NOT USE_WASM)
    return()
endif()

# No-op fallback so basegame.cmake doesn't crash when wasi-sdk is absent
function(add_wasm MODULE_NAME)
    message(STATUS "WASM: Skipping ${MODULE_NAME} (wasi-sdk not available)")
endfunction()

# Find wasi-sdk. Auto-detect at platform-canonical paths so each cmake
# configure (Release AND Debug) finds the toolchain without requiring
# the caller to forward WASI_SDK_PATH every time. Env var still wins
# if set.
if(NOT DEFINED WASI_SDK_PATH)
    if(DEFINED ENV{WASI_SDK_PATH})
        set(WASI_SDK_PATH "$ENV{WASI_SDK_PATH}")
    else()
        # User-local install is tried first. A cross-compiler toolchain is
        # per-developer state, not system state, so setting up a build should
        # not require sudo. A deliberate system-wide install under /opt still
        # works as a fallback, and an explicit WASI_SDK_PATH still wins over
        # both.
        set(_wasi_candidates "")
        foreach(_home_var HOME USERPROFILE)
            if(DEFINED ENV{${_home_var}})
                list(APPEND _wasi_candidates "$ENV{${_home_var}}/.local/opt/wasi-sdk")
            endif()
        endforeach()
        list(APPEND _wasi_candidates "/opt/wasi-sdk")
        if(WIN32)
            # MSYS2 mounts its /opt under C:/msys64/opt. cmake on Windows
            # doesn't see "/opt/wasi-sdk" as the same path, so list the
            # native form explicitly.
            list(APPEND _wasi_candidates "C:/msys64/opt/wasi-sdk")
        endif()
        list(REMOVE_DUPLICATES _wasi_candidates)
        foreach(_candidate IN LISTS _wasi_candidates)
            if(EXISTS "${_candidate}")
                set(WASI_SDK_PATH "${_candidate}")
                break()
            endif()
        endforeach()
        if(NOT DEFINED WASI_SDK_PATH)
            message(STATUS "WASM: wasi-sdk not found — WASM module compilation disabled")
            message(STATUS "  Tried: ${_wasi_candidates}")
            message(STATUS "  Set WASI_SDK_PATH (env var or -D) to override")
            return()
        endif()
    endif()
endif()

if(WIN32)
    set(WASI_CC "${WASI_SDK_PATH}/bin/clang.exe")
else()
    set(WASI_CC "${WASI_SDK_PATH}/bin/clang")
endif()
if(NOT EXISTS "${WASI_CC}")
    message(STATUS "WASM: ${WASI_CC} not found — WASM module compilation disabled")
    return()
endif()

message(STATUS "WASM: wasi-sdk found at ${WASI_SDK_PATH}")

# Detect AOT target
if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
    set(WASM_AOT_TARGET "aarch64")
else()
    set(WASM_AOT_TARGET "x86_64")
endif()

# Find wamrc (optional, for AOT compilation)
find_program(WAMRC wamrc)
if(WAMRC)
    message(STATUS "WASM: wamrc found at ${WAMRC} (AOT compilation enabled)")
else()
    message(STATUS "WASM: wamrc not found (AOT compilation disabled)")
endif()

#
# add_wasm(MODULE_NAME
#     SOURCES src1.c src2.c ...
#     [OUTPUT_NAME name]
#     [OUTPUT_DIRECTORY dir]
#     [DEFINITIONS -DFOO -DBAR]
#     [INCLUDE_DIRECTORIES dir1 dir2 ...]
# )
#
# Compiles C sources to a .wasm module via wasi-sdk, then optionally
# runs wamrc to produce an .aot file for near-native AOT execution.
#
function(add_wasm MODULE_NAME)
    cmake_parse_arguments(ARG "" "OUTPUT_DIRECTORY;OUTPUT_NAME"
        "DEFINITIONS;SOURCES;INCLUDE_DIRECTORIES" ${ARGN})

    # Output name defaults to MODULE_NAME
    if(NOT ARG_OUTPUT_NAME)
        set(ARG_OUTPUT_NAME ${MODULE_NAME})
    endif()

    set(WASM_OUTPUT_DIR ${CMAKE_BINARY_DIR}/$<CONFIG>)
    if(ARG_OUTPUT_DIRECTORY)
        set(WASM_OUTPUT_DIR ${WASM_OUTPUT_DIR}/${ARG_OUTPUT_DIRECTORY})
    endif()

    set(WASM_OUT ${WASM_OUTPUT_DIR}/${ARG_OUTPUT_NAME}.wasm)
    set(AOT_OUT  ${WASM_OUTPUT_DIR}/${ARG_OUTPUT_NAME}.aot)

    set(WASM_CFLAGS
        -DWASM_MODULE
        # Mirror the engine's Debug _DEBUG define (CMakeLists ADD_COMPILE_DEFINITIONS,
        # which the WASI custom-command does NOT inherit) so _DEBUG-gated code — e.g.
        # the typed-IPC PoC handshake (docs/vm-typed-ipc-design.md) — compiles into the
        # module in Debug to match the engine. Release WASM stays clean.
        $<$<CONFIG:Debug>:-D_DEBUG>
        -O2
        --target=wasm32-wasip1
        -Wl,--no-entry
        -Wl,--export=vmMain
        # Initial linear memory (BSS + data floor). Raised 16->32 MiB: the game
        # module's static arrays (g_entities[MAX_GENTITIES] et al.) already sit near
        # ~15.4 MiB, and the savegame working buffers (g_save_file.c I/O scratch +
        # g_save_world.c payload, gamesv-only) need headroom above that. 32 MiB
        # (512 * 64 KiB pages) clears the ~22 MiB static requirement with margin.
        # Link-time floor only — no runtime/gameplay effect; the host sets no
        # conflicting max (WASM_HEAP_SIZE=0, no --max-memory, system allocator).
        -Wl,--initial-memory=33554432
        ${ARG_DEFINITIONS}
    )

    # Add include directories
    foreach(dir ${ARG_INCLUDE_DIRECTORIES})
        list(APPEND WASM_CFLAGS -I${dir})
    endforeach()

    # Header dependencies. The module is one multi-input clang invocation, so
    # there is no per-TU depfile — without explicit header deps a touched
    # header silently did NOT rebuild the .wasm (the stale-VM-in-pak class the
    # Makefile's PAK_OUT machinery exists to prevent). Approximate the true
    # dependency set by globbing every header reachable through the include
    # dirs and the source dirs; over-approximation only costs an occasional
    # extra rebuild, under-approximation ships a stale module. Snapshot is
    # taken at configure time: a header ADDED later is picked up at the next
    # cmake reconfigure, same as the project's other source globs.
    set(_wasm_dep_dirs ${ARG_INCLUDE_DIRECTORIES} ${CMAKE_SOURCE_DIR}/code/wasm)
    foreach(_src ${ARG_SOURCES})
        get_filename_component(_d ${_src} DIRECTORY)
        list(APPEND _wasm_dep_dirs ${_d})
    endforeach()
    list(REMOVE_DUPLICATES _wasm_dep_dirs)
    set(_wasm_hdr_deps "")
    foreach(_d ${_wasm_dep_dirs})
        file(GLOB_RECURSE _h ${_d}/*.h)
        list(APPEND _wasm_hdr_deps ${_h})
    endforeach()

    add_custom_command(
        OUTPUT ${WASM_OUT}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${WASM_OUTPUT_DIR}
        COMMAND ${WASI_CC} ${WASM_CFLAGS} -o ${WASM_OUT}
                ${ARG_SOURCES} ${CMAKE_SOURCE_DIR}/code/wasm/wasm_bridge.c
        DEPENDS ${ARG_SOURCES} ${CMAKE_SOURCE_DIR}/code/wasm/wasm_bridge.c
                ${_wasm_hdr_deps}
        COMMENT "Building WASM module: ${ARG_OUTPUT_NAME}.wasm"
        VERBATIM
    )

    if(WAMRC)
        add_custom_command(
            OUTPUT ${AOT_OUT}
            COMMAND ${WAMRC} --target=${WASM_AOT_TARGET} -o ${AOT_OUT} ${WASM_OUT}
            DEPENDS ${WASM_OUT}
            COMMENT "AOT compiling: ${ARG_OUTPUT_NAME}.aot (${WASM_AOT_TARGET})"
            VERBATIM
        )
        add_custom_target(${MODULE_NAME} ALL DEPENDS ${WASM_OUT} ${AOT_OUT})
    else()
        add_custom_target(${MODULE_NAME} ALL DEPENDS ${WASM_OUT})
    endif()
endfunction()
