# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2024-present Wired Engine contributors

IF(NOT DEFINED SOURCE_ROOT OR NOT IS_DIRECTORY "${SOURCE_ROOT}")
	MESSAGE(FATAL_ERROR "SOURCE_ROOT must name the q3now source tree")
ENDIF()
IF(NOT DEFINED PROBE_ROOT OR PROBE_ROOT STREQUAL "")
	MESSAGE(FATAL_ERROR "PROBE_ROOT is required")
ENDIF()

FILE(REMOVE_RECURSE "${PROBE_ROOT}")
STRING(CONCAT _retired_arg "-D" "FEAT_" "RAL=1")
EXECUTE_PROCESS(
	COMMAND "${CMAKE_COMMAND}" -S "${SOURCE_ROOT}" -B "${PROBE_ROOT}"
		"${_retired_arg}" -DBUILD_TESTING=OFF
	RESULT_VARIABLE _rc
	OUTPUT_VARIABLE _stdout
	ERROR_VARIABLE _stderr
)
FILE(REMOVE_RECURSE "${PROBE_ROOT}")
SET(_combined "${_stdout}\n${_stderr}")
IF(_rc EQUAL 0)
	MESSAGE(FATAL_ERROR "retired RAL feature cache input was accepted")
ENDIF()
STRING(FIND "${_combined}" "is retired: Vulkan RAL is mandatory" _receipt)
IF(_receipt EQUAL -1)
	MESSAGE(FATAL_ERROR "retired RAL feature rejection lacked exact policy receipt:\n${_combined}")
ENDIF()
MESSAGE(STATUS "RAL retired-feature rejection contract: PASS")
