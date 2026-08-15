# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 Wired Engine contributors

FOREACH(_required SOURCE_ROOT COMPILE_COMMANDS BUILD_NINJA TOOLS_ENABLED)
	IF(NOT DEFINED ${_required})
		MESSAGE(FATAL_ERROR "${_required} is required")
	ENDIF()
ENDFOREACH()
IF(NOT EXISTS "${COMPILE_COMMANDS}" OR NOT EXISTS "${BUILD_NINJA}")
	MESSAGE(STATUS "SKIP: ImGui ownership policy requires a Ninja build with compile_commands.json")
	RETURN()
ENDIF()
FILE(READ "${BUILD_NINJA}" _ninja)
CMAKE_PATH(CONVERT "${SOURCE_ROOT}" TO_CMAKE_PATH_LIST _source_root NORMALIZE)

SET(_expected_relative
	code/tools/profile_imgui/wired_profile_imgui.cpp
	code/tools/profile_imgui/wired_profile_imgui_ral.cpp
	code/tools/profile_imgui/wired_profile_imgui_sdl3.cpp
	src/libs/recastnavigation/RecastDemo/Contrib/imgui/imgui.cpp
	src/libs/recastnavigation/RecastDemo/Contrib/imgui/imgui_draw.cpp
	src/libs/recastnavigation/RecastDemo/Contrib/imgui/imgui_tables.cpp
	src/libs/recastnavigation/RecastDemo/Contrib/imgui/imgui_widgets.cpp
)
SET(_expected_absolute)
FOREACH(_relative IN LISTS _expected_relative)
	LIST(APPEND _expected_absolute "${_source_root}/${_relative}")
ENDFOREACH()

FILE(READ "${COMPILE_COMMANDS}" _commands)
STRING(JSON _command_count LENGTH "${_commands}")
SET(_seen)
MATH(EXPR _last "${_command_count} - 1")
FOREACH(_index RANGE 0 ${_last})
	STRING(JSON _file GET "${_commands}" ${_index} file)
	CMAKE_PATH(CONVERT "${_file}" TO_CMAKE_PATH_LIST _file NORMALIZE)
	LIST(FIND _expected_absolute "${_file}" _owner_index)
	IF(_owner_index GREATER -1)
		STRING(JSON _output GET "${_commands}" ${_index} output)
		CMAKE_PATH(CONVERT "${_output}" TO_CMAKE_PATH_LIST _output NORMALIZE)
		STRING(JSON _command GET "${_commands}" ${_index} command)
		IF(_output MATCHES "CMakeFiles/wired_profile_imgui_ral_test\\.dir/")
			# The focused fake-RAL contract recompiles production adapter + ImGui
			# against recorder symbols; it is not a shipping/archive consumer.
			CONTINUE()
		ENDIF()
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

# The isolated CPU contract always consumes the archive; the standalone tool
# is the only additional tools-on consumer. Shipping GUI/headless must never
# gain a transitive ImGui link.
FILE(STRINGS "${BUILD_NINJA}" _imgui_link_lines
	REGEX "^[ \\t]*LINK_LIBRARIES = .*(lib)?wired_profile_imgui\\.(a|lib)")
LIST(LENGTH _imgui_link_lines _imgui_link_count)
IF(TOOLS_ENABLED)
	SET(_expected_consumers 2)
ELSE()
	SET(_expected_consumers 1)
ENDIF()
IF(NOT _imgui_link_count EQUAL _expected_consumers)
	MESSAGE(FATAL_ERROR
		"tool-only ImGui archive consumer mismatch: expected ${_expected_consumers}, got ${_imgui_link_count}")
ENDIF()
IF(NOT _ninja MATCHES "build ral_profile_imgui_test(\\.exe)?:[^\n]*(lib)?wired_profile_imgui\\.(a|lib)")
	MESSAGE(FATAL_ERROR "ImGui CPU contract is not an exact archive consumer")
ENDIF()
IF(TOOLS_ENABLED)
	IF(NOT _ninja MATCHES "build wired_profile_host(\\.exe)?:[^\n]*(lib)?wired_profile_imgui\\.(a|lib)")
		MESSAGE(FATAL_ERROR "tools-on profile host is not an exact ImGui archive consumer")
	ENDIF()
ELSEIF(_ninja MATCHES "build wired_profile_host:")
	MESSAGE(FATAL_ERROR "tools-off graph contains the profile host")
ENDIF()
IF(_ninja MATCHES "build (wired[.](arm64|x64)([.]exe)?|wired-headless[.](arm64|x64)([.]exe)?|wired_(opengl|opengl2|vulkan)_(arm64|x86_64)[.](dylib|dll)):[^\n]*(lib)?wired_profile_imgui[.](a|lib)")
	MESSAGE(FATAL_ERROR "tool-only ImGui archive leaked into a shipping target")
ENDIF()

MESSAGE(STATUS "ImGui tool policy PASS: 7 sources isolated; exact tool/test consumers")
