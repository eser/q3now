# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2024-present Wired Engine contributors

IF(NOT DEFINED SOURCE_ROOT OR NOT IS_DIRECTORY "${SOURCE_ROOT}")
	MESSAGE(FATAL_ERROR "SOURCE_ROOT must name the q3now source tree")
ENDIF()

# The old flag asserted that RAL could be compiled out even though the live
# Vulkan renderer always built and consumed it. Keep the retired token absent
# from code/docs/build comments so that false optionality cannot creep back in.
STRING(CONCAT _retired "FEAT_" "RAL")
FILE(GLOB_RECURSE _policy_files
	"${SOURCE_ROOT}/code/*.c"
	"${SOURCE_ROOT}/code/*.h"
	"${SOURCE_ROOT}/code/*.md"
	"${SOURCE_ROOT}/tools/*.mjs"
)
LIST(APPEND _policy_files
	"${SOURCE_ROOT}/CMakeLists.txt"
	"${SOURCE_ROOT}/Makefile"
)
FOREACH(_path IN LISTS _policy_files)
	FILE(READ "${_path}" _body)
	STRING(FIND "${_body}" "${_retired}" _hit)
	IF(NOT _hit EQUAL -1)
		FILE(RELATIVE_PATH _rel "${SOURCE_ROOT}" "${_path}")
		MESSAGE(FATAL_ERROR "retired RAL feature token remains in ${_rel}")
	ENDIF()
ENDFOREACH()

MESSAGE(STATUS "RAL feature policy contract: mandatory Vulkan layer; retired optional flag absent")
