# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2024-present Wired Engine contributors
if(NOT DEFINED ROOT)
	message(FATAL_ERROR "ROOT is required")
endif()
set(BACKEND "${ROOT}/code/render/ral/backends/webgpu")
file(READ "${BACKEND}/ral_webgpu_runtime.c" SOURCE)
foreach(NEEDLE IN ITEMS
	"while ( runtime->pipelineCount )"
	"RalWebGpu_PipelineDestroy"
	"RalWebGpu_ResourcesDestroy"
	"RalWebGpu_CommandDestroy"
	"RalWebGpu_PresentationDestroy"
	"RalWebGpu_CoreDestroy")
	string(FIND "${SOURCE}" "${NEEDLE}" POSITION)
	if(POSITION EQUAL -1)
		message(FATAL_ERROR "WebGPU runtime is missing aggregate teardown authority: ${NEEDLE}")
	endif()
endforeach()
string(FIND "${SOURCE}" "RalWebGpu_ResourcesDestroy" RESOURCE_POSITION)
string(FIND "${SOURCE}" "RalWebGpu_CoreDestroy" CORE_POSITION)
if(RESOURCE_POSITION GREATER CORE_POSITION)
	message(FATAL_ERROR "WebGPU resources must be destroyed before core")
endif()
foreach(FORBIDDEN IN ITEMS "WebGL2" "WEBGL2" "GLES" "SDL_Window")
	string(FIND "${SOURCE}" "${FORBIDDEN}" POSITION)
	if(NOT POSITION EQUAL -1)
		message(FATAL_ERROR "forbidden fallback/window dependency: ${FORBIDDEN}")
	endif()
endforeach()
message(STATUS "RAL WebGPU aggregate runtime ownership policy: PASS")
