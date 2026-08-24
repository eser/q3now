# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2024-present Wired Engine contributors

if(NOT DEFINED ROOT)
	message(FATAL_ERROR "ROOT is required")
endif()
set(BACKEND "${ROOT}/code/render/ral/backends/webgpu")
foreach(FILE IN ITEMS ral_webgpu_pipeline.h ral_webgpu_pipeline.c)
	if(NOT EXISTS "${BACKEND}/${FILE}")
		message(FATAL_ERROR "missing canonical WebGPU pipeline file: ${FILE}")
	endif()
	file(READ "${BACKEND}/${FILE}" TEXT)
	foreach(FORBIDDEN IN ITEMS "WebGL2" "WEBGL2" "GLES" "glsl300" "vulkan.h" "OpenGL/")
		string(FIND "${TEXT}" "${FORBIDDEN}" POSITION)
		if(NOT POSITION EQUAL -1)
			message(FATAL_ERROR "forbidden fallback/native dependency in ${FILE}: ${FORBIDDEN}")
		endif()
	endforeach()
endforeach()
file(READ "${BACKEND}/ral_webgpu_pipeline.h" HEADER)
file(READ "${BACKEND}/ral_webgpu_pipeline.c" SOURCE)
foreach(NEEDLE IN ITEMS
	"RAL_SHADER_ARTIFACT_WGSL"
	"Ral_ShaderArtifactDigest"
	"createShaderModule"
	"createBindGroupLayout"
	"createPipelineLayout"
	"createPipeline"
	"RalWebGpu_CoreMatchesReceipt")
	string(FIND "${HEADER}${SOURCE}" "${NEEDLE}" POSITION)
	if(POSITION EQUAL -1)
		message(FATAL_ERROR "WebGPU pipeline layer is missing exact lowering authority: ${NEEDLE}")
	endif()
endforeach()
message(STATUS "RAL WebGPU WGSL binding/pipeline ownership policy: PASS")
