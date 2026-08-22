# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2024-present Wired Engine contributors

if(NOT DEFINED ROOT)
	message(FATAL_ERROR "ROOT is required")
endif()
execute_process(COMMAND "${CMAKE_COMMAND}" -DROOT=${ROOT}
	-DPROBE_TEXT=qvkGetPhysicalDeviceSurfaceFormats2KHR
	-P "${ROOT}/cmake/ral_swapchain_policy_check.cmake"
	RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
if(rc EQUAL 0)
	message(FATAL_ERROR "RAL swapchain policy accepted a renderer-local surface-format query")
endif()
string(FIND "${out}${err}" "retired renderer ownership" receipt)
if(receipt EQUAL -1)
	message(FATAL_ERROR "RAL swapchain rejection lacked causal receipt: ${out}${err}")
endif()
message(STATUS "RAL swapchain policy mutation: retired renderer query rejected")
