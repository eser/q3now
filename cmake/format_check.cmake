# Driver script invoked by `cmake --build <dir> --target format-check`.
# Runs clang-format --dry-run --Werror per file so the command line stays
# under Windows cmd.exe limits even with hundreds of sources. Fails with
# a non-zero exit if any file deviates from .clang-format.

cmake_minimum_required(VERSION 3.16)

IF(NOT DEFINED CLANG_FORMAT)
    MESSAGE(FATAL_ERROR "format_check.cmake requires -DCLANG_FORMAT=<path>")
ENDIF()
IF(NOT DEFINED SOURCE_DIR)
    MESSAGE(FATAL_ERROR "format_check.cmake requires -DSOURCE_DIR=<path>")
ENDIF()

SET(_dirs
    code/botlib
    code/cgame
    code/client
    code/game
    code/qcommon
    code/renderer
    code/renderer2
    code/renderercommon
    code/renderervk
    code/sdl
    code/server
    code/unix
    code/win32
)

SET(_files)
FOREACH(d IN LISTS _dirs)
    FILE(GLOB_RECURSE _f
        "${SOURCE_DIR}/${d}/*.c"
        "${SOURCE_DIR}/${d}/*.h"
    )
    LIST(APPEND _files ${_f})
ENDFOREACH()

LIST(LENGTH _files _n)
MESSAGE(STATUS "format-check: scanning ${_n} files")

SET(_failed 0)
FOREACH(f IN LISTS _files)
    EXECUTE_PROCESS(
        COMMAND "${CLANG_FORMAT}" --dry-run --Werror --style=file "${f}"
        RESULT_VARIABLE _rc
        OUTPUT_VARIABLE _out
        ERROR_VARIABLE  _err
    )
    IF(NOT _rc EQUAL 0)
        MESSAGE(STATUS "  drift: ${f}")
        IF(_err)
            MESSAGE("${_err}")
        ENDIF()
        MATH(EXPR _failed "${_failed} + 1")
    ENDIF()
ENDFOREACH()

IF(_failed GREATER 0)
    MESSAGE(FATAL_ERROR "format-check: ${_failed} file(s) need clang-format")
ENDIF()
MESSAGE(STATUS "format-check: OK")
