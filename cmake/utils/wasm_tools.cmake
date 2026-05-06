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
        set(_wasi_candidates "/opt/wasi-sdk")
        if(WIN32)
            # MSYS2 mounts its /opt under C:/msys64/opt. cmake on Windows
            # doesn't see "/opt/wasi-sdk" as the same path, so list the
            # native form explicitly.
            list(APPEND _wasi_candidates "C:/msys64/opt/wasi-sdk")
        endif()
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
        -O2
        --target=wasm32-wasip1
        -Wl,--no-entry
        -Wl,--export=vmMain
        -Wl,--initial-memory=16777216
        ${ARG_DEFINITIONS}
    )

    # Add include directories
    foreach(dir ${ARG_INCLUDE_DIRECTORIES})
        list(APPEND WASM_CFLAGS -I${dir})
    endforeach()

    add_custom_command(
        OUTPUT ${WASM_OUT}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${WASM_OUTPUT_DIR}
        COMMAND ${WASI_CC} ${WASM_CFLAGS} -o ${WASM_OUT}
                ${ARG_SOURCES} ${CMAKE_SOURCE_DIR}/code/wasm/wasm_bridge.c
        DEPENDS ${ARG_SOURCES} ${CMAKE_SOURCE_DIR}/code/wasm/wasm_bridge.c
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
