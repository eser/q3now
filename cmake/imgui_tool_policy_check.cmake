# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 Wired Engine contributors

FOREACH(_required SOURCE_ROOT COMPILE_COMMANDS BUILD_NINJA)
	IF(NOT DEFINED ${_required})
		MESSAGE(FATAL_ERROR "${_required} is required")
	ENDIF()
ENDFOREACH()
IF(NOT EXISTS "${COMPILE_COMMANDS}" OR NOT EXISTS "${BUILD_NINJA}")
	MESSAGE(FATAL_ERROR "ImGui tool policy inputs are missing")
ENDIF()

SET(_expected_relative
	code/tools/profile_imgui/wired_profile_imgui.cpp
	code/tools/profile_imgui/wired_profile_imgui_sdl3.cpp
	src/libs/recastnavigation/RecastDemo/Contrib/imgui/imgui.cpp
	src/libs/recastnavigation/RecastDemo/Contrib/imgui/imgui_draw.cpp
	src/libs/recastnavigation/RecastDemo/Contrib/imgui/imgui_tables.cpp
	src/libs/recastnavigation/RecastDemo/Contrib/imgui/imgui_widgets.cpp
)
SET(_expected_absolute)
FOREACH(_relative IN LISTS _expected_relative)
	LIST(APPEND _expected_absolute "${SOURCE_ROOT}/${_relative}")
ENDFOREACH()

FILE(READ "${COMPILE_COMMANDS}" _commands)
STRING(JSON _command_count LENGTH "${_commands}")
SET(_seen)
MATH(EXPR _last "${_command_count} - 1")
FOREACH(_index RANGE 0 ${_last})
	STRING(JSON _file GET "${_commands}" ${_index} file)
	LIST(FIND _expected_absolute "${_file}" _owner_index)
	IF(_owner_index GREATER -1)
		STRING(JSON _output GET "${_commands}" ${_index} output)
		STRING(JSON _command GET "${_commands}" ${_index} command)
		IF(NOT _output MATCHES "CMakeFiles/wired_profile_imgui\\.dir/")
			MESSAGE(FATAL_ERROR "ImGui tool source leaked outside its adapter target: ${_file} -> ${_output}")
		ENDIF()
		IF(_command MATCHES "(^| )-DHEADLESS(=| |$)")
			MESSAGE(FATAL_ERROR "HEADLESS product policy leaked into tool-only ImGui source: ${_file}")
		ENDIF()
		LIST(APPEND _seen "${_file}")
	ENDIF()
ENDFOREACH()
LIST(SORT _expected_absolute)
LIST(SORT _seen)
IF(NOT _seen STREQUAL _expected_absolute)
	MESSAGE(FATAL_ERROR "ImGui adapter compile ownership mismatch")
ENDIF()

# In the testing graph exactly one consumer may link the adapter: the isolated
# host contract. Shipping GUI/headless must not gain a transitive ImGui link.
FILE(STRINGS "${BUILD_NINJA}" _imgui_link_lines
	REGEX "^[ \\t]*LINK_LIBRARIES = .*libwired_profile_imgui\\.a")
LIST(LENGTH _imgui_link_lines _imgui_link_count)
IF(NOT _imgui_link_count EQUAL 1)
	MESSAGE(FATAL_ERROR
		"tool-only ImGui archive must have exactly one test consumer, got ${_imgui_link_count}")
ENDIF()

MESSAGE(STATUS "ImGui tool policy PASS: 6 sources isolated; shipping links absent")
