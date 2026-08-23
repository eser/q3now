# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2024-present Wired Engine contributors

if(NOT DEFINED SOURCE_ROOT OR NOT DEFINED PROBE_ROOT)
	message(FATAL_ERROR "SOURCE_ROOT and PROBE_ROOT are required")
endif()

file(REMOVE_RECURSE "${PROBE_ROOT}")
foreach(dir IN ITEMS code/renderercommon code/renderer/ral_vulkan code/client code/sdl code/renderervk code/qcommon)
	file(MAKE_DIRECTORY "${PROBE_ROOT}/${dir}")
endforeach()
file(COPY "${SOURCE_ROOT}/code/renderercommon/tr_public.h" DESTINATION "${PROBE_ROOT}/code/renderercommon")
file(COPY "${SOURCE_ROOT}/code/client/client.h" DESTINATION "${PROBE_ROOT}/code/client")
file(COPY "${SOURCE_ROOT}/code/sdl/sdl_glimp.c" DESTINATION "${PROBE_ROOT}/code/sdl")
file(COPY "${SOURCE_ROOT}/code/renderer/ral_vulkan/ral_vulkan_caps.c"
	DESTINATION "${PROBE_ROOT}/code/renderer/ral_vulkan")
file(COPY "${SOURCE_ROOT}/code/renderer/ral_vulkan/ral_vulkan_resource.c"
	DESTINATION "${PROBE_ROOT}/code/renderer/ral_vulkan")
file(COPY "${SOURCE_ROOT}/code/renderervk/vk_ral_textures.c"
	"${SOURCE_ROOT}/code/renderervk/vk_ral_buffer_shadow.c"
	"${SOURCE_ROOT}/code/renderervk/vk_ral_buffer_shadow.h"
	"${SOURCE_ROOT}/code/renderervk/vk.c" "${SOURCE_ROOT}/code/renderervk/vk.h"
	DESTINATION "${PROBE_ROOT}/code/renderervk")
string(CONCAT native_type "V" "kBuffer")
file(WRITE "${PROBE_ROOT}/code/qcommon/native_leak.c" "${native_type} escapedBuffer;\n")

execute_process(COMMAND "${CMAKE_COMMAND}"
	-DSOURCE_ROOT=${PROBE_ROOT}
	-P ${SOURCE_ROOT}/cmake/ral_raw_api_boundary_policy_check.cmake
	RESULT_VARIABLE rc OUTPUT_VARIABLE stdout ERROR_VARIABLE stderr)
file(REMOVE_RECURSE "${PROBE_ROOT}")
if(rc EQUAL 0)
	message(FATAL_ERROR "raw API boundary accepted a native type in qcommon")
endif()
string(FIND "${stdout}${stderr}" "raw graphics API escaped backend/platform ownership" receipt)
if(receipt EQUAL -1)
	message(FATAL_ERROR "raw API rejection lacked its causal receipt: ${stdout}${stderr}")
endif()

# The explicit Vulkan renderer adapter may retain native value vocabulary, but
# it may not reacquire loader/PFN/call ownership from the RAL backend.
foreach(dir IN ITEMS code/renderercommon code/renderer/ral_vulkan code/client code/sdl code/renderervk code/qcommon)
	file(MAKE_DIRECTORY "${PROBE_ROOT}/${dir}")
endforeach()
file(COPY "${SOURCE_ROOT}/code/renderercommon/tr_public.h" DESTINATION "${PROBE_ROOT}/code/renderercommon")
file(COPY "${SOURCE_ROOT}/code/client/client.h" DESTINATION "${PROBE_ROOT}/code/client")
file(COPY "${SOURCE_ROOT}/code/sdl/sdl_glimp.c" DESTINATION "${PROBE_ROOT}/code/sdl")
file(COPY "${SOURCE_ROOT}/code/renderer/ral_vulkan/ral_vulkan_caps.c"
	DESTINATION "${PROBE_ROOT}/code/renderer/ral_vulkan")
file(COPY "${SOURCE_ROOT}/code/renderer/ral_vulkan/ral_vulkan_resource.c"
	DESTINATION "${PROBE_ROOT}/code/renderer/ral_vulkan")
file(COPY "${SOURCE_ROOT}/code/renderervk/vk_ral_textures.c"
	"${SOURCE_ROOT}/code/renderervk/vk_ral_buffer_shadow.c"
	"${SOURCE_ROOT}/code/renderervk/vk_ral_buffer_shadow.h"
	"${SOURCE_ROOT}/code/renderervk/vk.c" "${SOURCE_ROOT}/code/renderervk/vk.h"
	DESTINATION "${PROBE_ROOT}/code/renderervk")
file(APPEND "${PROBE_ROOT}/code/renderervk/vk.c"
	"\nvoid ral_raw_boundary_mutation(void) { qvkMapMemory(); }\n")
execute_process(COMMAND "${CMAKE_COMMAND}"
	-DSOURCE_ROOT=${PROBE_ROOT}
	-P ${SOURCE_ROOT}/cmake/ral_raw_api_boundary_policy_check.cmake
	RESULT_VARIABLE rc OUTPUT_VARIABLE stdout ERROR_VARIABLE stderr)
file(REMOVE_RECURSE "${PROBE_ROOT}")
if(rc EQUAL 0)
	message(FATAL_ERROR "raw API boundary accepted renderer-local qvk ownership")
endif()
string(FIND "${stdout}${stderr}" "retired declaration-only entry point qvkMapMemory" receipt)
if(receipt EQUAL -1)
	message(FATAL_ERROR "qvk ownership rejection lacked its causal receipt: ${stdout}${stderr}")
endif()

message(STATUS "RAL raw API boundary mutations: common native handle and renderer qvk ownership rejected")
