# SPDX-License-Identifier: GPL-3.0-or-later
# Exact ordinary GPU-IQM bind-group migration policy.

IF(NOT DEFINED SOURCE_ROOT OR NOT IS_DIRECTORY "${SOURCE_ROOT}")
	MESSAGE(FATAL_ERROR "SOURCE_ROOT must name the q3now source tree")
ENDIF()

FILE(READ "${SOURCE_ROOT}/code/renderer/ral/ral_command.h" RAL_HEADER)
FILE(READ "${SOURCE_ROOT}/code/renderer/ral_vulkan/ral_vulkan_bridge.h" BRIDGE_HEADER)
FILE(READ "${SOURCE_ROOT}/code/renderer/ral_vulkan/ral_vulkan_dynamic_bind.c" BIND_CORE)
FILE(READ "${SOURCE_ROOT}/code/renderer/ral_vulkan/ral_vulkan_pipeline.c" PIPELINE_CORE)
FILE(READ "${SOURCE_ROOT}/code/renderervk/vk.c" VK_SOURCE)
FILE(READ "${SOURCE_ROOT}/code/renderervk/vk.h" VK_HEADER)
FILE(READ "${SOURCE_ROOT}/code/renderervk/vk_ral_textures.c" RAL_TEXTURES)
FILE(READ "${SOURCE_ROOT}/code/renderervk/tr_model_iqm.c" IQM_MODEL)
FILE(READ "${SOURCE_ROOT}/tests/ral_vulkan_dynamic_bind_test.c" HOST)

FUNCTION(REQUIRE_TEXT BODY NEEDLE MESSAGE_TEXT)
	STRING(FIND "${BODY}" "${NEEDLE}" POS)
	IF(POS EQUAL -1)
		MESSAGE(FATAL_ERROR "${MESSAGE_TEXT}: ${NEEDLE}")
	ENDIF()
ENDFUNCTION()

FUNCTION(REQUIRE_COUNT BODY NEEDLE EXPECTED MESSAGE_TEXT)
	STRING(REGEX REPLACE "([][+.*()^$?\\|])" "\\\\\\1" ESCAPED "${NEEDLE}")
	STRING(REGEX MATCHALL "${ESCAPED}" MATCHES "${BODY}")
	LIST(LENGTH MATCHES ACTUAL)
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

REQUIRE_TEXT("${RAL_HEADER}" "qboolean Ral_CmdBindBindGroupDynamicExact("
	"portable exact bind-group declaration missing")
REQUIRE_TEXT("${BRIDGE_HEADER}" "Ral_RegisterExternalPipelineBindGroupLayouts("
	"external pipeline set ABI registration missing")
FOREACH(NEEDLE IN ITEMS
	"pipeline->bindGroupLayoutsRegistered"
	"setIndex < pipeline->numSetLayouts"
	"pipeline->setLayouts[setIndex] == group->layout->layout"
	"cb->state != RAL_VK_CMD_RECORDING"
	"cb->lifecycle.state != RAL_COMMAND_RECORDING"
	"group->set == VK_NULL_HANDLE"
	"ralVk_CmdBindBindGroupDynamicCore( cb, setIndex, group,"
	"dynamicOffsets, dynamicOffsetCount, qtrue )")
	REQUIRE_TEXT("${BIND_CORE}" "${NEEDLE}"
		"exact bind-group validation seam missing")
ENDFOREACH()

FOREACH(NEEDLE IN ITEMS
	"pipeline->layoutCacheIndex != 0xFFFFFFFFu"
	"count == 0u || count > RAL_VK_MAX_PIPELINE_SETS"
	"bindGroupLayouts[i]->backend != pipeline->backend"
	"candidate[i] = bindGroupLayouts[i]->layout"
	"pipeline->numSetLayouts == count"
	"memcmp( pipeline->setLayouts, candidate"
	"pipeline->bindGroupLayoutsRegistered = qtrue;")
	REQUIRE_TEXT("${PIPELINE_CORE}" "${NEEDLE}"
		"output-atomic external pipeline ABI registration missing")
ENDFOREACH()

REQUIRE_TEXT("${VK_HEADER}" "ral_bone_descriptor[NUM_COMMAND_BUFFERS]"
	"retained IQM bone group inventory missing")
REQUIRE_TEXT("${VK_HEADER}" "struct ralBindGroup_s *textureGroup"
	"ordinary IQM signature still exposes a native descriptor set")
FOREACH(NEEDLE IN ITEMS
	"ee.dynamicOffset = qtrue;"
	"Ral_RegisterExternalPipelineBindGroupLayouts("
	"vk.iqmGpu.ral_pipeline, 2u, layouts"
	"vk.iqmGpu.ral_bgl_bones"
	"vk.ral_bgl_sampler")
	REQUIRE_TEXT("${VK_SOURCE}" "${NEEDLE}"
		"IQM pipeline set ABI publication missing")
ENDFOREACH()
FOREACH(NEEDLE IN ITEMS
	"vk.iqmGpu.ral_bone_descriptor[i]"
	"vk_ral_lookup_buffer("
	"vk.iqmGpu.bone_buffer[i]"
	"Ral_RegisterAdoptedBindGroupDynamicBuffer("
	"group, 0u, buffer, 0u, item")
	REQUIRE_TEXT("${RAL_TEXTURES}" "${NEEDLE}"
		"IQM dynamic bone group adoption missing")
ENDFOREACH()

EXTRACT_SPAN("${VK_SOURCE}" "void vk_draw_iqm_gpu("
	"#ifdef USE_VBO" IQM_DRAW)
STRING(FIND "${IQM_DRAW}" "qvkCmdBindDescriptorSets" RAW_BIND)
IF(NOT RAW_BIND EQUAL -1)
	MESSAGE(FATAL_ERROR "ordinary GPU-IQM regained raw descriptor binding")
ENDIF()
REQUIRE_COUNT("${IQM_DRAW}" "Ral_CmdBindBindGroupDynamicExact(" 2
	"ordinary IQM exact bind count drifted")
REQUIRE_COUNT("${IQM_DRAW}" "Ral_CmdBindPipeline( vk.cmd->ral_cmd, NULL )" 2
	"ordinary IQM fail-closed pipeline invalidation drifted")
FOREACH(NEEDLE IN ITEMS
	"vk.iqmGpu.ral_bone_descriptor[frameIdx], &iqmOff, 1u"
	"textureGroup, NULL, 0u"
	"vk_ral_bind_registered_vertex_buffers("
	"vk_ral_bind_registered_index_buffer("
	"Ral_CmdDrawIndexed( vk.cmd->ral_cmd")
	REQUIRE_TEXT("${IQM_DRAW}" "${NEEDLE}"
		"ordinary IQM exact command operand missing")
ENDFOREACH()
STRING(FIND "${IQM_DRAW}" "Ral_CmdBindPipeline( vk.cmd->ral_cmd, vk.iqmGpu.ral_pipeline )" PIPE_POS)
STRING(FIND "${IQM_DRAW}" "vk.iqmGpu.ral_bone_descriptor[frameIdx], &iqmOff, 1u" BONE_POS)
STRING(FIND "${IQM_DRAW}" "textureGroup, NULL, 0u" TEXTURE_POS)
STRING(FIND "${IQM_DRAW}" "vk_ral_bind_registered_vertex_buffers(" VERTEX_POS)
STRING(FIND "${IQM_DRAW}" "Ral_CmdDrawIndexed( vk.cmd->ral_cmd" DRAW_POS)
IF(PIPE_POS EQUAL -1 OR BONE_POS EQUAL -1 OR TEXTURE_POS EQUAL -1
		OR VERTEX_POS EQUAL -1 OR DRAW_POS EQUAL -1
		OR NOT PIPE_POS LESS BONE_POS OR NOT BONE_POS LESS TEXTURE_POS
		OR NOT TEXTURE_POS LESS VERTEX_POS OR NOT VERTEX_POS LESS DRAW_POS)
	MESSAGE(FATAL_ERROR "ordinary IQM exact bind/draw order drifted")
ENDIF()

REQUIRE_TEXT("${IQM_MODEL}" "ordinaryImage->ralDescriptor"
	"ordinary IQM material did not pass the retained RAL group")
REQUIRE_TEXT("${VK_SOURCE}"
	"Ral_DestroyBindGroup( vk.iqmGpu.ral_bone_descriptor[i] );"
	"IQM descriptor child teardown missing")

FOREACH(NEEDLE IN ITEMS
	"!Ral_CmdBindBindGroupDynamicExact( &command, 3u, &group, offsets, 2u )"
	"Ral_RegisterExternalPipelineBindGroupLayouts("
	"otherLayout.layout = (VkDescriptorSetLayout)(uintptr_t)0x61u"
	"command.boundBindGroups[3] == &sentinel"
	"!Ral_RegisterExternalPipelineBindGroupLayouts("
	"Ral_CmdBindBindGroupDynamicExact( &command, 3u, &group, offsets, 2u )")
	REQUIRE_TEXT("${HOST}" "${NEEDLE}"
		"exact bind-group mutation coverage missing")
ENDFOREACH()

MESSAGE(STATUS "RAL ordinary GPU-IQM exact bind-group policy: set ABI and two binds pinned")
