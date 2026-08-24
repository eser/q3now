# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2024-present Wired Engine contributors

if(NOT DEFINED ROOT)
	message(FATAL_ERROR "ROOT is required")
endif()
set(H "${ROOT}/code/render/ral/backends/metal/ral_metal_draw.h")
set(S "${ROOT}/code/render/ral/backends/metal/ral_metal_draw.mm")
set(T "${ROOT}/tests/ral_metal_draw_test.mm")
foreach(path IN ITEMS "${H}" "${S}" "${T}")
	if(NOT EXISTS "${path}")
		message(FATAL_ERROR "missing Metal draw contract file: ${path}")
	endif()
endforeach()
file(READ "${H}" HEADER)
file(READ "${S}" SOURCE)
file(READ "${T}" TEST)
foreach(needle IN ITEMS
	"RalMetal_CoreMatchesReceipt" "RalMetal_PipelineMatchesReceipt"
	"RalMetal_BindGroupMatchesReceipt" "RalMetal_RenderPlanValid"
	"setRenderPipelineState" "setFragmentBuffer" "setVertexBuffer"
	"drawIndexedPrimitives:MTLPrimitiveTypeTriangle" "MTLIndexTypeUInt16"
	"RalMetal_BindGroupUseFragmentResources" "RalMetal_CorePublishSubmission"
	"getBytes:pixels" "pixelDigest = DigestBytes")
	string(FIND "${SOURCE}" "${needle}" pos)
	if(pos EQUAL -1)
		message(FATAL_ERROR "Metal draw lost validation/emission seam: ${needle}")
	endif()
endforeach()
foreach(forbidden IN ITEMS "CAMetalLayer" "nextDrawable" "VkCommandBuffer" "MoltenVK"
	"newLibraryWithSource")
	string(FIND "${SOURCE}" "${forbidden}" pos)
	if(NOT pos EQUAL -1)
		message(FATAL_ERROR "Metal draw gained forbidden presentation/runtime seam: ${forbidden}")
	endif()
endforeach()
foreach(needle IN ITEMS
	"badDraw.indexBuffer.byteSize = 10u" "badDraw.vertexBuffer = badDraw.indexBuffer"
	"staleCore.generation++" "stalePipeline.pipelineGeneration++"
	"staleGroup.resources[0].resourceGeneration++" "color.textureGeneration = UINT64_MAX"
	"withBytes:badTexel bytesPerRow:4u"
	"receipt.pixelCount == 64u" "indexBufferIdentity = exact.vertexBufferIdentity"
	"colorTextureIdentity = exact.sampledTextureIdentity" "expectedRgba[2]++")
	string(FIND "${TEST}" "${needle}" pos)
	if(pos EQUAL -1)
		message(FATAL_ERROR "Metal draw host lost mutation/pixel coverage: ${needle}")
	endif()
endforeach()
message(STATUS "RAL Metal indexed draw policy: PASS")
