# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2024-present Wired Engine contributors

if(NOT DEFINED ROOT)
	message(FATAL_ERROR "ROOT is required")
endif()
set(BACKEND "${ROOT}/code/render/ral/backends/webgpu")
foreach(FILE IN ITEMS ral_webgpu_core.h ral_webgpu_core.c)
	if(NOT EXISTS "${BACKEND}/${FILE}")
		message(FATAL_ERROR "missing canonical WebGPU adapter file: ${FILE}")
	endif()
endforeach()
file(READ "${BACKEND}/ral_webgpu_core.h" HEADER)
file(READ "${BACKEND}/ral_webgpu_core.c" SOURCE)
foreach(NEEDLE IN ITEMS
	"RAL_WEBGPU_REQUEST_PENDING"
	"RAL_WEBGPU_POLL_UNAVAILABLE"
	"beginAdapter"
	"pollAdapter"
	"beginDevice"
	"pollDevice"
	"Ral_CapabilityProfileFromCaps"
	"RAL_BACKEND_WEBGPU"
	"RAL_MEMORY_RECOVERY_RECREATE_BACKEND")
	string(FIND "${HEADER}${SOURCE}" "${NEEDLE}" POSITION)
	if(POSITION EQUAL -1)
		message(FATAL_ERROR "WebGPU core is missing async/capability authority: ${NEEDLE}")
	endif()
endforeach()
foreach(FORBIDDEN IN ITEMS "WebGL2" "WEBGL2" "GLES" "glsl300" "vulkan.h" "OpenGL/")
	string(FIND "${HEADER}${SOURCE}" "${FORBIDDEN}" POSITION)
	if(NOT POSITION EQUAL -1)
		message(FATAL_ERROR "forbidden WebGPU fallback/native dependency: ${FORBIDDEN}")
	endif()
endforeach()
file(GLOB_RECURSE PORTABLE_SOURCES
	"${ROOT}/code/render/frontend/*.[ch]"
	"${ROOT}/code/render/ral/core/*.[ch]")
foreach(FILE IN LISTS PORTABLE_SOURCES)
	file(READ "${FILE}" TEXT)
	if(TEXT MATCHES "#[ \t]*include[ \t]*[<\"][^>\"]*(webgpu|wgpu)")
		message(FATAL_ERROR "native WebGPU include leaked outside adapter: ${FILE}")
	endif()
endforeach()
message(STATUS "RAL WebGPU async core ownership policy: PASS")
