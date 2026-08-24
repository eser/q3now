# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2024-present Wired Engine contributors

# Dynamic UI rendering is a RAL-owned color-only pass.  Legacy Vulkan
# VkRenderPass/VkFramebuffer compatibility objects must not return.

IF(NOT DEFINED SOURCE_ROOT OR NOT IS_DIRECTORY "${SOURCE_ROOT}")
	MESSAGE(FATAL_ERROR "SOURCE_ROOT must name the q3now source tree")
ENDIF()

FILE(READ "${SOURCE_ROOT}/code/render/ral/backends/vulkan/renderer/vk.c" VK)
FILE(READ "${SOURCE_ROOT}/code/render/ral/backends/vulkan/renderer/vk.h" VK_H)

FUNCTION(REQUIRE_TEXT BODY NEEDLE LABEL)
	STRING(FIND "${BODY}" "${NEEDLE}" POS)
	IF(POS EQUAL -1)
		MESSAGE(FATAL_ERROR "${LABEL}: missing '${NEEDLE}'")
	ENDIF()
ENDFUNCTION()

FUNCTION(FORBID_TEXT BODY NEEDLE LABEL)
	STRING(FIND "${BODY}" "${NEEDLE}" POS)
	IF(NOT POS EQUAL -1)
		MESSAGE(FATAL_ERROR "${LABEL}: forbidden '${NEEDLE}'")
	ENDIF()
ENDFUNCTION()

FUNCTION(REQUIRE_COUNT BODY REGEX EXPECTED LABEL)
	STRING(REGEX MATCHALL "${REGEX}" HITS "${BODY}")
	LIST(LENGTH HITS COUNT)
	IF(NOT COUNT EQUAL EXPECTED)
		MESSAGE(FATAL_ERROR "${LABEL}: expected ${EXPECTED}, found ${COUNT}")
	ENDIF()
ENDFUNCTION()

FUNCTION(REQUIRE_ORDER BODY FIRST SECOND LABEL)
	STRING(FIND "${BODY}" "${FIRST}" FIRST_POS)
	STRING(FIND "${BODY}" "${SECOND}" SECOND_POS)
	IF(FIRST_POS EQUAL -1 OR SECOND_POS EQUAL -1 OR NOT FIRST_POS LESS SECOND_POS)
		MESSAGE(FATAL_ERROR "${LABEL}: '${FIRST}' must precede '${SECOND}'")
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

EXTRACT_SPAN("${VK}" "void vk_open_ui_pass( qboolean clear )"
	"vk_scene_depth_copy" UI_PASS)

# The dead native object cohort is absent from declarations, loader ownership,
# storage, lifecycle helpers and callsites.
FOREACH(NEEDLE IN ITEMS
	"qvkCreateRenderPass"
	"qvkDestroyRenderPass"
	"qvkCreateFramebuffer"
	"qvkDestroyFramebuffer"
	"VkRenderPassCreateInfo"
	"VkFramebufferCreateInfo"
	"vk_create_render_passes"
	"vk_destroy_render_passes"
	"vk_create_framebuffers"
	"vk_destroy_framebuffers"
	"vk.render_pass.ui"
	"vk.render_pass.ui_clear"
	"vk.framebuffers.ui"
	"vk.framebuffers.ui_clear")
	FORBID_TEXT("${VK}" "${NEEDLE}" "legacy UI native object ownership")
ENDFOREACH()
FOREACH(NEEDLE IN ITEMS "VkRenderPass ui" "VkRenderPass ui_clear"
	"VkFramebuffer ui" "VkFramebuffer ui_clear")
	FORBID_TEXT("${VK_H}" "${NEEDLE}" "legacy UI native object storage")
ENDFOREACH()

# Pin the exact color-only dynamic recipe and its state publication.  CLEAR and
# LOAD remain one branch of the same RAL pass instead of separate native
# compatibility objects.
FOREACH(NEEDLE IN ITEMS
	"if ( !vk.fboActive )"
	"if ( clear )"
	"vk_end_render_pass();"
	"memset( &ri, 0, sizeof( ri ) );"
	"ri.colorAttachments[0] = vk.ral_tonemapped_image;"
	"ri.colorLoadOps[0]     = clear ? RAL_LOAD_OP_CLEAR : RAL_LOAD_OP_LOAD;"
	"ri.colorStoreOps[0]    = RAL_STORE_OP_STORE;"
	"ri.numColorAttachments = 1;"
	"Ral_BeginRendering( vk.cmd->ral_cmd, &ri );"
	"vk.cmd->open_dynamic_pass = VK_DYN_PASS_UI;"
	"vk.renderPassIndex = RENDER_PASS_MAIN;")
	REQUIRE_TEXT("${UI_PASS}" "${NEEDLE}" "RAL dynamic UI recipe")
ENDFOREACH()
REQUIRE_COUNT("${UI_PASS}" "Ral_BeginRendering[(]" 1
	"RAL dynamic UI begin inventory")
REQUIRE_COUNT("${UI_PASS}" "RAL_LOAD_OP_CLEAR" 1
	"RAL dynamic UI CLEAR branch")
REQUIRE_COUNT("${UI_PASS}" "RAL_LOAD_OP_LOAD" 1
	"RAL dynamic UI LOAD branch")
REQUIRE_ORDER("${UI_PASS}" "if ( clear )" "vk_end_render_pass();"
	"pure-2D prior-pass close")
REQUIRE_ORDER("${UI_PASS}" "vk_end_render_pass();"
	"ri.colorAttachments[0] = vk.ral_tonemapped_image;"
	"UI target publication")
REQUIRE_ORDER("${UI_PASS}"
	"ri.colorAttachments[0] = vk.ral_tonemapped_image;"
	"ri.colorLoadOps[0]     = clear ? RAL_LOAD_OP_CLEAR : RAL_LOAD_OP_LOAD;"
	"UI target before load policy")
REQUIRE_ORDER("${UI_PASS}" "ri.colorStoreOps[0]    = RAL_STORE_OP_STORE;"
	"Ral_BeginRendering( vk.cmd->ral_cmd, &ri );"
	"UI recipe before emission")
REQUIRE_ORDER("${UI_PASS}" "Ral_BeginRendering( vk.cmd->ral_cmd, &ri );"
	"vk.cmd->open_dynamic_pass = VK_DYN_PASS_UI;"
	"UI emission before state publication")
FOREACH(NEEDLE IN ITEMS "qvk" "VkRenderPass" "VkFramebuffer"
	"depthAttachment" "vk.depth_image")
	FORBID_TEXT("${UI_PASS}" "${NEEDLE}" "RAL color-only UI command span")
ENDFOREACH()

MESSAGE(STATUS
	"RAL dynamic UI object policy: color-only CLEAR/LOAD pass exact; legacy VkRenderPass/VkFramebuffer cohort absent")
