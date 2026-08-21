# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2024-present Wired Engine contributors

if(NOT DEFINED BINARY OR NOT EXISTS "${BINARY}")
	message(FATAL_ERROR "BINARY is required")
endif()
execute_process(COMMAND /usr/bin/otool -L "${BINARY}"
	RESULT_VARIABLE RESULT OUTPUT_VARIABLE LINKS ERROR_VARIABLE ERROR_TEXT)
if(NOT RESULT EQUAL 0)
	message(FATAL_ERROR "otool failed: ${ERROR_TEXT}")
endif()
foreach(REQUIRED "Metal.framework" "QuartzCore.framework" "CoreGraphics.framework" "libSDL3")
	string(FIND "${LINKS}" "${REQUIRED}" POS)
	if(POS EQUAL -1)
		message(FATAL_ERROR "wired_metal smoke missing linkage: ${REQUIRED}")
	endif()
endforeach()
foreach(FORBIDDEN "MoltenVK" "Vulkan.framework" "libvulkan")
	string(FIND "${LINKS}" "${FORBIDDEN}" POS)
	if(NOT POS EQUAL -1)
		message(FATAL_ERROR "wired_metal smoke gained compatibility linkage: ${FORBIDDEN}")
	endif()
endforeach()
message(STATUS "wired_metal window link policy: PASS")
