# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2024-present Wired Engine contributors

if(NOT DEFINED ROOT)
	message(FATAL_ERROR "ROOT is required")
endif()
set(BACKEND "${ROOT}/code/render/ral/backends/webgpu")
foreach(FILE IN ITEMS ral_webgpu_resource.h ral_webgpu_resource.c)
	if(NOT EXISTS "${BACKEND}/${FILE}")
		message(FATAL_ERROR "missing canonical WebGPU resource file: ${FILE}")
	endif()
	file(READ "${BACKEND}/${FILE}" TEXT)
	foreach(FORBIDDEN IN ITEMS "WebGL2" "WEBGL2" "GLES" "glsl300" "vulkan.h" "OpenGL/")
		string(FIND "${TEXT}" "${FORBIDDEN}" POSITION)
		if(NOT POSITION EQUAL -1)
			message(FATAL_ERROR "forbidden fallback/native dependency in ${FILE}: ${FORBIDDEN}")
		endif()
	endforeach()
endforeach()
file(READ "${BACKEND}/ral_webgpu_resource.h" HEADER)
file(READ "${BACKEND}/ral_webgpu_resource.c" SOURCE)
foreach(NEEDLE IN ITEMS
	"RalWebGpu_WriteBuffer"
	"RalWebGpu_WriteTexture"
	"RalWebGpu_WriteReceiptExact"
	"beginRoundTrip"
	"pollRoundTrip"
	"RAL_WEBGPU_ASYNC_PENDING"
	"RAL_TRANSFER_OUTCOME_NATIVE_ASYNC"
	"Ral_BackendConformanceBuild"
	"RalWebGpu_CoreMatchesReceipt")
	string(FIND "${HEADER}${SOURCE}" "${NEEDLE}" POSITION)
	if(POSITION EQUAL -1)
		message(FATAL_ERROR "WebGPU resource layer is missing async authority: ${NEEDLE}")
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
message(STATUS "RAL WebGPU resource/async transfer ownership policy: PASS")
