include_guard(GLOBAL)

if(NOT USE_WASM)
    return()
endif()

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
            message(FATAL_ERROR
                "WASM is enabled but wasi-sdk was not found. Tried: "
                "${_wasi_candidates}. Set WASI_SDK_PATH (env var or -D) to override.")
        endif()
    endif()
endif()

# wasi-sdk clang is a HOST tool: it runs on the build machine regardless of
# the target platform. CMAKE_HOST_WIN32 (not WIN32) is the correct guard —
# plain WIN32 is also TRUE when CROSS-compiling FOR Windows from macOS/Linux
# and would look for clang.exe inside a native wasi-sdk.
if(CMAKE_HOST_WIN32)
    set(WASI_CC "${WASI_SDK_PATH}/bin/clang.exe")
else()
    set(WASI_CC "${WASI_SDK_PATH}/bin/clang")
endif()
if(NOT EXISTS "${WASI_CC}")
    message(FATAL_ERROR
        "WASM is enabled but the wasi-sdk compiler was not found: ${WASI_CC}")
endif()

message(STATUS "WASM: wasi-sdk found at ${WASI_SDK_PATH}")

# Detect AOT target
if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
    set(WASM_AOT_TARGET "aarch64")
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "riscv64")
    set(WASM_AOT_TARGET "riscv64")
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

    if(CMAKE_CONFIGURATION_TYPES)
        set(WASM_CONFIG_DIR /$<CONFIG>)
    else()
        set(WASM_CONFIG_DIR "")
    endif()
    set(WASM_OUTPUT_DIR ${CMAKE_BINARY_DIR}${WASM_CONFIG_DIR})
    if(ARG_OUTPUT_DIRECTORY)
        set(WASM_OUTPUT_DIR ${WASM_OUTPUT_DIR}/${ARG_OUTPUT_DIRECTORY})
    endif()

    set(WASM_OUT ${WASM_OUTPUT_DIR}/${ARG_OUTPUT_NAME}.wasm)
    set(AOT_OUT  ${WASM_OUTPUT_DIR}/${ARG_OUTPUT_NAME}.aot)
    set(WASM_OBJECT_DIR ${CMAKE_BINARY_DIR}${WASM_CONFIG_DIR}/wasm-obj/${MODULE_NAME})

    set(WASM_COMPILE_FLAGS
        -DWASM_MODULE
        # Mirror the engine's Debug _DEBUG define (CMakeLists ADD_COMPILE_DEFINITIONS,
        # which the WASI custom-command does NOT inherit) so _DEBUG-gated code — e.g.
        # the typed-IPC PoC handshake (docs/vm-typed-ipc-design.md) — compiles into the
        # module in Debug to match the engine. Release WASM stays clean.
        $<$<CONFIG:Debug>:-D_DEBUG>
        -O2
        --target=wasm32-wasip1
        ${ARG_DEFINITIONS}
    )

    set(WASM_LINK_FLAGS
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
    )

    # Add include directories
    foreach(dir ${ARG_INCLUDE_DIRECTORIES})
        list(APPEND WASM_COMPILE_FLAGS -I${dir})
    endforeach()

    # Compile every translation unit independently. Object identity includes
    # both the module and repo-relative source path, so equal basenames in
    # different directories — and the bridge compiled once into each module —
    # cannot collide. Clang's depfile is the header authority: an included new
    # header is discovered without reconfiguring, while unrelated headers no
    # longer rebuild the entire module.
    set(_wasm_sources ${ARG_SOURCES} ${CMAKE_SOURCE_DIR}/code/wasm/wasm_bridge.c)
    list(REMOVE_DUPLICATES _wasm_sources)
    set(_wasm_objects "")
    set(_wasm_rsp_lines "")

    foreach(_src IN LISTS _wasm_sources)
        get_filename_component(_src_abs "${_src}" ABSOLUTE
            BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
        file(RELATIVE_PATH _src_rel "${CMAKE_SOURCE_DIR}" "${_src_abs}")
        if(_src_rel MATCHES "^\\.\\.")
            string(SHA256 _src_hash "${_src_abs}")
            set(_src_rel "external/${_src_hash}")
        endif()
        string(REPLACE ":" "_" _src_rel "${_src_rel}")

        set(_obj "${WASM_OBJECT_DIR}/${_src_rel}.o")
        set(_dep "${_obj}.d")
        get_filename_component(_obj_dir "${_obj}" DIRECTORY)

        add_custom_command(
            OUTPUT "${_obj}"
            COMMAND ${CMAKE_COMMAND} -E make_directory "${_obj_dir}"
            COMMAND "${WASI_CC}" ${WASM_COMPILE_FLAGS}
                    -MMD -MF "${_dep}" -MT "${_obj}"
                    -c "${_src_abs}" -o "${_obj}"
            DEPENDS "${_src_abs}" "${WASI_CC}"
            DEPFILE "${_dep}"
            COMMENT "Building WASM object: ${ARG_OUTPUT_NAME} / ${_src_rel}"
            VERBATIM
        )

        list(APPEND _wasm_objects "${_obj}")
        string(APPEND _wasm_rsp_lines "\"${_obj}\"\n")
    endforeach()

    # A response file avoids Windows command-line limits while preserving the
    # authored source order used by the previous single clang invocation.
    set(_wasm_rsp "${WASM_OBJECT_DIR}/objects.rsp")
    file(GENERATE OUTPUT "${_wasm_rsp}" CONTENT "${_wasm_rsp_lines}")

    add_custom_command(
        OUTPUT "${WASM_OUT}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${WASM_OUTPUT_DIR}"
        COMMAND "${WASI_CC}" ${WASM_LINK_FLAGS} -o "${WASM_OUT}" "@${_wasm_rsp}"
        DEPENDS ${_wasm_objects} "${_wasm_rsp}" "${WASI_CC}"
        COMMENT "Linking WASM module: ${ARG_OUTPUT_NAME}.wasm"
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
