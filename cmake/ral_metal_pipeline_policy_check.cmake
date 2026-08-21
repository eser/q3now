# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2024-present Wired Engine contributors

if(NOT DEFINED ROOT)
	message(FATAL_ERROR "ROOT is required")
endif()
set(CMAKE_SOURCE "${ROOT}/CMakeLists.txt")
set(H "${ROOT}/code/renderer/ral_metal/ral_metal_pipeline.h")
set(S "${ROOT}/code/renderer/ral_metal/ral_metal_pipeline.mm")
set(T "${ROOT}/tests/ral_metal_pipeline_test.mm")
foreach(path IN ITEMS "${H}" "${S}" "${T}")
	if(NOT EXISTS "${path}")
		message(FATAL_ERROR "missing Metal pipeline contract file: ${path}")
	endif()
endforeach()
file(READ "${CMAKE_SOURCE}" CMAKE_TEXT)
file(READ "${H}" HEADER)
file(READ "${S}" SOURCE)
file(READ "${T}" TEST)
foreach(needle IN ITEMS
	"xcrun -sdk macosx metal -x metal -c" "xcrun -sdk macosx metallib"
	"overlay_vert_spv.msl" "overlay_frag_spv.msl")
	string(FIND "${CMAKE_TEXT}" "${needle}" pos)
	if(pos EQUAL -1)
		message(FATAL_ERROR "Metal pipeline lost offline toolchain seam: ${needle}")
	endif()
endforeach()
foreach(needle IN ITEMS
	"newLibraryWithURL" "newFunctionWithName:@\"main0\""
	"newRenderPipelineStateWithDescriptor" "MTLVertexFormatUChar4Normalized"
	"MTLBlendFactorSourceAlpha" "LayoutIsCanonicalOverlay"
	"Ral_ShaderArtifactDigest" "RalMetal_CoreMatchesReceipt"
	"RalMetal_BindLayoutMatchesReceipt")
	string(FIND "${SOURCE}" "${needle}" pos)
	if(pos EQUAL -1)
		message(FATAL_ERROR "Metal pipeline lost native/ABI seam: ${needle}")
	endif()
endforeach()
foreach(forbidden IN ITEMS "newLibraryWithSource" "VkPipeline" "MoltenVK" "WGPUShaderModule")
	string(FIND "${HEADER}${SOURCE}" "${forbidden}" pos)
	if(NOT pos EQUAL -1)
		message(FATAL_ERROR "Metal pipeline gained forbidden runtime/compatibility seam: ${forbidden}")
	endif()
endforeach()
foreach(needle IN ITEMS
	"expectedVertexSourceDigest.lane0++" "missing-wired.metallib"
	"fragmentLibraryPath = validVertexLibrary" "staleCore.generation++"
	"staleLayout.layoutGeneration++" "sampleCount = 2u"
	"entries[1].binding = 31u" "vertexInputOffsets[2]"
	"fragmentLibraryDigest.lane1" "pipelineIdentity = exact.fragmentLayoutIdentity"
	"PipelineDestroy( pipeline )")
	string(FIND "${TEST}" "${needle}" pos)
	if(pos EQUAL -1)
		message(FATAL_ERROR "Metal pipeline host lost mutation/ownership coverage: ${needle}")
	endif()
endforeach()
message(STATUS "RAL Metal pipeline policy: PASS")
