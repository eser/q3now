# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2024-present Wired Engine contributors

if(NOT DEFINED ROOT)
	message(FATAL_ERROR "ROOT is required")
endif()

set(HEADER "${ROOT}/code/renderer/ral/ral_transition.h")
file(READ "${HEADER}" TEXT)
file(READ "${ROOT}/code/renderer/ral/ral_command.h" COMMAND_TEXT)
file(READ "${ROOT}/code/renderer/ral_vulkan/ral_vulkan_transition.c" VULKAN_TEXT)
file(READ "${ROOT}/code/renderer/ral_vulkan/ral_vulkan_command.c" LEGACY_COMMAND_TEXT)
file(READ "${ROOT}/code/renderer/ral_vulkan/ral_vulkan_resource.c" RESOURCE_TEXT)
file(READ "${ROOT}/code/renderer/ral_vulkan/ral_vulkan_internal.h" INTERNAL_TEXT)

foreach(FORBIDDEN IN ITEMS
	"VkImageLayout" "VkAccess" "VkPipelineStage" "VkQueue"
	"VK_IMAGE_LAYOUT" "VK_ACCESS" "VK_PIPELINE_STAGE" "queueFamilyIndex")
	string(FIND "${TEXT}" "${FORBIDDEN}" POSITION)
	if(NOT POSITION EQUAL -1)
		message(FATAL_ERROR "portable RAL transition header leaked native Vulkan token: ${FORBIDDEN}")
	endif()
endforeach()

foreach(REQUIRED IN ITEMS
	"buf->portableStateKnown = qfalse"
	"WebGPU requires that access"
	"info->bufferMemoryBarriers[i].buffer->portableStateKnown = qfalse"
	"info->imageMemoryBarriers[i].texture->portableStateKnown = qfalse"
	"src->portableState.usage != RAL_RESOURCE_USAGE_COPY_SOURCE"
	"dst->portableState.usage != RAL_RESOURCE_USAGE_COPY_DESTINATION")
	string(FIND "${RESOURCE_TEXT}${LEGACY_COMMAND_TEXT}" "${REQUIRED}" POSITION)
	if(POSITION EQUAL -1)
		message(FATAL_ERROR "legacy RAL path lost fail-closed portable-state seam: ${REQUIRED}")
	endif()
endforeach()

foreach(REQUIRED IN ITEMS
	"ralResult_t Ral_CmdTransitionResources"
	"const ralResourceTransitionBatch_t *batch"
	"Ral_CmdReleaseBufferOwnership"
	"Ral_CmdAcquireBufferOwnership"
	"Ral_CancelBufferOwnershipTransfer"
	"Ral_CmdReleaseTextureOwnership"
	"Ral_CmdAcquireTextureOwnership"
	"Ral_CancelTextureOwnershipTransfer")
	string(FIND "${COMMAND_TEXT}" "${REQUIRED}" POSITION)
	if(POSITION EQUAL -1)
		message(FATAL_ERROR "portable RAL transition command lost required declaration: ${REQUIRED}")
	endif()
endforeach()

foreach(REQUIRED IN ITEMS
	"cb->renderingActive"
	"VK_QUEUE_FAMILY_IGNORED"
	"return ralUnsupported"
	"cb->backend->vk.CmdPipelineBarrier"
	"texture->portableState = batch->textureTransitions[i].after"
	"Ral_QueueTransferLifecycleRelease( &buffer->queueTransfer"
	"Ral_QueueTransferLifecycleAcquire( &buffer->queueTransfer"
	"Ral_QueueTransferLifecycleRelease( &texture->queueTransfer"
	"Ral_QueueTransferLifecycleAcquire( &texture->queueTransfer"
	"barrier.srcQueueFamilyIndex = cb->backend->queueFamily[transition->sourceQueue]"
	"barrier.dstQueueFamilyIndex = cb->backend->queueFamily[transition->destinationQueue]")
	string(FIND "${VULKAN_TEXT}" "${REQUIRED}" POSITION)
	if(POSITION EQUAL -1)
		message(FATAL_ERROR "Vulkan semantic transition command lost required seam: ${REQUIRED}")
	endif()
endforeach()

foreach(REQUIRED IN ITEMS
	"ralResourceUsage_t" "ralResourceState_t" "ralBufferTransition_t"
	"ralTextureTransition_t" "sourceQueue" "destinationQueue"
	"WebGPU validates encoder/pass usage and relies on implicit transitions"
	"ralQueueTransferReceipt_t" "ralQueueTransferLifecycle_t"
	"Ral_QueueTransferReceiptExact"
	"Ral_QueueTransferLifecycleRelease"
	"Ral_QueueTransferLifecycleAcquire"
	"Ral_QueueTransferLifecycleCancel")
	string(FIND "${TEXT}" "${REQUIRED}" POSITION)
	if(POSITION EQUAL -1)
		message(FATAL_ERROR "portable RAL transition contract lost required seam: ${REQUIRED}")
	endif()
endforeach()

foreach(REQUIRED IN ITEMS
	"!buffer->queueTransfer.pending.ready"
	"buffer->queueTransfer.pending.ready"
	"texture->queueTransfer.pending.ready")
	string(FIND "${VULKAN_TEXT}${RESOURCE_TEXT}${INTERNAL_TEXT}" "${REQUIRED}" POSITION)
	if(POSITION EQUAL -1)
		message(FATAL_ERROR "pending ownership transfer lost fail-closed seam: ${REQUIRED}")
	endif()
endforeach()

message(STATUS "portable RAL transition policy: PASS")
