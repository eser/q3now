# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2024-present Wired Engine contributors

if(NOT DEFINED ROOT)
	message(FATAL_ERROR "ROOT is required")
endif()
set(H "${ROOT}/code/renderer/ral_metal/ral_metal_render.h")
set(S "${ROOT}/code/renderer/ral_metal/ral_metal_render.mm")
set(T "${ROOT}/tests/ral_metal_render_test.mm")
foreach(path IN ITEMS "${H}" "${S}" "${T}")
	if(NOT EXISTS "${path}")
		message(FATAL_ERROR "missing Metal render contract file: ${path}")
	endif()
endforeach()
file(READ "${H}" HEADER)
file(READ "${S}" SOURCE)
file(READ "${T}" TEST)
foreach(needle IN ITEMS
	"MTLRenderPassDescriptor" "renderCommandEncoderWithDescriptor"
	"MetalLoadAction" "MetalStoreAction" "MTLTextureUsageRenderTarget"
	"RalMetal_CoreBeginCommand" "Ral_CommandLifecyclePublishEnd"
	"RalMetal_CoreMatchesReceipt" "RalMetal_CorePublishSubmission" "waitUntilCompleted"
	"AttachmentInputValid" "RenderReceiptValid")
	string(FIND "${SOURCE}" "${needle}" pos)
	if(pos EQUAL -1)
		message(FATAL_ERROR "Metal render lost native/lifecycle seam: ${needle}")
	endif()
endforeach()
foreach(forbidden IN ITEMS "VkRendering" "VkImage" "WGPURenderPass" "MoltenVK")
	string(FIND "${HEADER}${SOURCE}" "${forbidden}" pos)
	if(NOT pos EQUAL -1)
		message(FATAL_ERROR "Metal render gained forbidden compatibility concept: ${forbidden}")
	endif()
endforeach()
foreach(needle IN ITEMS
	"color.textureGeneration = UINT64_MAX" "color.loadOp = (ralLoadOp_t)99"
	"color.clearValue.color[0] = NAN" "plan.colorAttachmentCount = 2u"
	"NearByte( pixels[0], 64u )" "recording.state == RAL_COMMAND_RECORDING"
	"submission.commands[0].commandIdentity" "RalMetal_CorePublishDeviceLoss")
	string(FIND "${TEST}" "${needle}" pos)
	if(pos EQUAL -1)
		message(FATAL_ERROR "Metal render host lost mutation/native proof: ${needle}")
	endif()
endforeach()
message(STATUS "RAL Metal render policy: PASS")
