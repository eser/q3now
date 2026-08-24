# SPDX-License-Identifier: GPL-3.0-or-later
# Exact portable mid-pass attachment-clear migration policy.

IF(NOT DEFINED SOURCE_ROOT OR NOT IS_DIRECTORY "${SOURCE_ROOT}")
	MESSAGE(FATAL_ERROR "SOURCE_ROOT must name the q3now source tree")
ENDIF()

FILE(READ "${SOURCE_ROOT}/code/render/ral/core/ral_command.h" RAL_HEADER)
FILE(READ "${SOURCE_ROOT}/code/render/ral/backends/vulkan/ral_vulkan_command.c" COMMAND)
FILE(READ "${SOURCE_ROOT}/code/render/ral/backends/vulkan/renderer/vk.c" VK_SOURCE)
FILE(READ "${SOURCE_ROOT}/tests/ral_vulkan_dynamic_bind_test.c" HOST)

FUNCTION(REQUIRE_TEXT BODY NEEDLE MESSAGE_TEXT)
	STRING(FIND "${BODY}" "${NEEDLE}" POS)
	IF(POS EQUAL -1)
		MESSAGE(FATAL_ERROR "${MESSAGE_TEXT}: ${NEEDLE}")
	ENDIF()
ENDFUNCTION()

FUNCTION(FORBID_TEXT BODY NEEDLE MESSAGE_TEXT)
	STRING(FIND "${BODY}" "${NEEDLE}" POS)
	IF(NOT POS EQUAL -1)
		MESSAGE(FATAL_ERROR "${MESSAGE_TEXT}: ${NEEDLE}")
	ENDIF()
ENDFUNCTION()

FUNCTION(REQUIRE_COUNT BODY NEEDLE EXPECTED MESSAGE_TEXT)
	STRING(LENGTH "${NEEDLE}" NEEDLE_LENGTH)
	STRING(LENGTH "${BODY}" BEFORE_LENGTH)
	STRING(REPLACE "${NEEDLE}" "" REMAINDER "${BODY}")
	STRING(LENGTH "${REMAINDER}" AFTER_LENGTH)
	MATH(EXPR ACTUAL "(${BEFORE_LENGTH} - ${AFTER_LENGTH}) / ${NEEDLE_LENGTH}")
	IF(NOT ACTUAL EQUAL EXPECTED)
		MESSAGE(FATAL_ERROR "${MESSAGE_TEXT}: ${NEEDLE} count ${ACTUAL}, expected ${EXPECTED}")
	ENDIF()
ENDFUNCTION()

FUNCTION(EXTRACT_SPAN BODY BEGIN END OUT)
	STRING(FIND "${BODY}" "${BEGIN}" BEGIN_POS)
	IF(BEGIN_POS EQUAL -1)
		MESSAGE(FATAL_ERROR "missing span begin: ${BEGIN}")
	ENDIF()
	STRING(SUBSTRING "${BODY}" ${BEGIN_POS} -1 TAIL)
	STRING(FIND "${TAIL}" "${END}" END_POS)
	IF(END_POS EQUAL -1)
		MESSAGE(FATAL_ERROR "missing span end: ${END}")
	ENDIF()
	STRING(SUBSTRING "${TAIL}" 0 ${END_POS} SPAN)
	SET(${OUT} "${SPAN}" PARENT_SCOPE)
ENDFUNCTION()

REQUIRE_TEXT("${RAL_HEADER}" "ralTextureAspectFlags_t aspectMask;"
	"clear attachment regained native aspect storage")
REQUIRE_TEXT("${RAL_HEADER}" "qboolean Ral_CmdClearAttachmentsExact("
	"exact portable clear declaration missing")

FOREACH(NEEDLE IN ITEMS
	"!ralVk_CommandRecordsBufferBindings( cb ) || !cb->renderingActive"
	"!cb->backend->vk.CmdClearAttachments"
	"attachmentCount > RAL_MAX_COLOR_ATTACHMENTS"
	"rectCount > RAL_MAX_COLOR_ATTACHMENTS"
	"aspects & ~( RAL_TEXTURE_ASPECT_COLOR"
	"aspects != RAL_TEXTURE_ASPECT_COLOR"
	"attachments[i].colorAttachment >= RAL_MAX_COLOR_ATTACHMENTS"
	"rects[i].rect.x < 0 || rects[i].rect.y < 0"
	"rects[i].rect.width == 0u || rects[i].rect.height == 0u"
	"rects[i].baseArrayLayer > UINT32_MAX - rects[i].layerCount"
	"nativeAspects |= VK_IMAGE_ASPECT_COLOR_BIT"
	"nativeAspects |= VK_IMAGE_ASPECT_DEPTH_BIT"
	"nativeAspects |= VK_IMAGE_ASPECT_STENCIL_BIT"
	"cb->backend->vk.CmdClearAttachments( cb->cb, attachmentCount"
	"return qtrue;")
	REQUIRE_TEXT("${COMMAND}" "${NEEDLE}"
		"exact clear validation/lowering seam missing")
ENDFOREACH()
FORBID_TEXT("${COMMAND}" "(const VkClearAttachment *)attachments"
	"clear lowering regained struct-layout cast")
FORBID_TEXT("${COMMAND}" "(const VkClearRect *)rects"
	"clear lowering regained rect-layout cast")

EXTRACT_SPAN("${VK_SOURCE}" "void vk_clear_color(" "void vk_clear_depth(" COLOR_SPAN)
EXTRACT_SPAN("${VK_SOURCE}" "void vk_clear_depth(" "void vk_update_mvp(" DEPTH_SPAN)
FOREACH(NEEDLE IN ITEMS
	"ralClearAttachment_t attachment;"
	"attachment.aspectMask = RAL_TEXTURE_ASPECT_COLOR;"
	"Ral_CmdClearAttachmentsExact( vk.cmd->ral_cmd"
	"TERM_UNRECOVERABLE"
	"exact RAL color attachment clear failed")
	REQUIRE_TEXT("${COLOR_SPAN}" "${NEEDLE}"
		"product color clear migration seam missing")
ENDFOREACH()
FOREACH(NEEDLE IN ITEMS
	"ralClearAttachment_t attachment;"
	"attachment.clearValue.depthStencil.depth = 0.0f;"
	"attachment.clearValue.depthStencil.depth = 1.0f;"
	"RAL_TEXTURE_ASPECT_DEPTH | RAL_TEXTURE_ASPECT_STENCIL"
	"attachment.aspectMask = RAL_TEXTURE_ASPECT_DEPTH;"
	"Ral_CmdClearAttachmentsExact( vk.cmd->ral_cmd"
	"TERM_UNRECOVERABLE"
	"exact RAL depth attachment clear failed")
	REQUIRE_TEXT("${DEPTH_SPAN}" "${NEEDLE}"
		"product depth clear migration seam missing")
ENDFOREACH()
REQUIRE_COUNT("${VK_SOURCE}" "Ral_CmdClearAttachmentsExact( vk.cmd->ral_cmd" 2
	"product exact clear inventory changed")

FILE(GLOB_RECURSE PRODUCT_SOURCES
	"${SOURCE_ROOT}/code/render/ral/backends/vulkan/renderer/*.c" "${SOURCE_ROOT}/code/render/ral/backends/vulkan/renderer/*.h")
FOREACH(PATH IN LISTS PRODUCT_SOURCES)
	FILE(READ "${PATH}" BODY)
	FORBID_TEXT("${BODY}" "qvkCmdClearAttachments"
		"raw attachment clear escaped Vulkan backend")
ENDFOREACH()

FOREACH(NEEDLE IN ITEMS
	"CaptureClearAttachments("
	"capturedClearAttachment.aspectMask == VK_IMAGE_ASPECT_COLOR_BIT"
	"VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT"
	"command.lifecycle.state = RAL_COMMAND_RECORDING;"
	"command.renderingActive = qfalse;"
	"clearAttachment.aspectMask = 0u;"
	"clearAttachment.aspectMask = 1u << 7;"
	"RAL_TEXTURE_ASPECT_COLOR | RAL_TEXTURE_ASPECT_DEPTH"
	"clearAttachment.colorAttachment = RAL_MAX_COLOR_ATTACHMENTS;"
	"clearRect.rect.x = -1;"
	"clearRect.rect.width = 0u;"
	"clearRect.layerCount = 0u;"
	"clearRect.baseArrayLayer = UINT32_MAX;"
	"backend.vk.CmdClearAttachments = NULL;"
	"CHECK( clearCalls == 0u );")
	REQUIRE_TEXT("${HOST}" "${NEEDLE}"
		"clear mutation host coverage missing")
ENDFOREACH()

MESSAGE(STATUS "RAL attachment clear policy: exact portable color/depth/stencil transaction; raw renderer ownership absent")
