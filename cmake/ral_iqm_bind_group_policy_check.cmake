# SPDX-License-Identifier: GPL-3.0-or-later
# Exact ordinary GPU-IQM bind-group migration policy.

IF(NOT DEFINED SOURCE_ROOT OR NOT IS_DIRECTORY "${SOURCE_ROOT}")
	MESSAGE(FATAL_ERROR "SOURCE_ROOT must name the q3now source tree")
ENDIF()

FILE(READ "${SOURCE_ROOT}/code/render/ral/core/ral_command.h" RAL_HEADER)
FILE(READ "${SOURCE_ROOT}/code/render/ral/backends/vulkan/ral_vulkan_bridge.h" BRIDGE_HEADER)
FILE(READ "${SOURCE_ROOT}/code/render/ral/backends/vulkan/ral_vulkan_dynamic_bind.c" BIND_CORE)
FILE(READ "${SOURCE_ROOT}/code/render/ral/backends/vulkan/ral_vulkan_pipeline.c" PIPELINE_CORE)
FILE(READ "${SOURCE_ROOT}/code/render/ral/backends/vulkan/renderer/vk.c" VK_SOURCE)
FILE(READ "${SOURCE_ROOT}/code/render/ral/backends/vulkan/renderer/vk.h" VK_HEADER)
FILE(READ "${SOURCE_ROOT}/code/render/ral/backends/vulkan/renderer/vk_ral_textures.c" RAL_TEXTURES)
FILE(READ "${SOURCE_ROOT}/code/render/ral/backends/vulkan/renderer/tr_model_iqm.c" IQM_MODEL)
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

FUNCTION(FORBID_TEXT BODY NEEDLE MESSAGE_TEXT)
	STRING(FIND "${BODY}" "${NEEDLE}" POS)
	IF(NOT POS EQUAL -1)
		MESSAGE(FATAL_ERROR "${MESSAGE_TEXT}: ${NEEDLE}")
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
	"boneEntry.dynamicOffset = qtrue;"
	"vk_create_effect_bind_group_layout( &boneEntry, 1u,"
	"layouts[0] = vk.iqmGpu.ral_bgl_bones;"
	"layouts[1] = vk.ral_bgl_sampler;"
	"candidate = Ral_CreatePipelineLayout( vk_ral_get_backend(), &createInfo );"
	"vk.iqmGpu.ral_pipeline_layout = candidate;"
	"vk.iqmGpu.pipeline_layout = native;")
	REQUIRE_TEXT("${VK_SOURCE}" "${NEEDLE}"
		"IQM direct layout ownership missing")
ENDFOREACH()
STRING(FIND "${VK_SOURCE}" "vk_ral_textures_init();" RAL_INIT_POS)
STRING(FIND "${VK_SOURCE}"
	"&vk.ral_bgl_sampler,\n\t\t&vk.set_layout_sampler, \"wired-set-layout-sampler\" );" SAMPLER_CREATE_POS)
STRING(FIND "${VK_SOURCE}" "vk_init_iqm_gpu_skinning();" IQM_INIT_CALL_POS)
IF(RAL_INIT_POS EQUAL -1 OR SAMPLER_CREATE_POS EQUAL -1
		OR IQM_INIT_CALL_POS EQUAL -1
		OR NOT SAMPLER_CREATE_POS LESS RAL_INIT_POS
		OR NOT RAL_INIT_POS LESS IQM_INIT_CALL_POS)
	MESSAGE(FATAL_ERROR
		"IQM init must follow RAL backend init and direct shared sampler-layout creation")
ENDIF()
EXTRACT_SPAN("${VK_SOURCE}" "void vk_init_iqm_gpu_skinning( void )"
	"void vk_shutdown_iqm_gpu_skinning( void )" IQM_INIT)
FOREACH(RETIRED IN ITEMS qvkCreateDescriptorSetLayout qvkCreatePipelineLayout
	Ral_AdoptBindGroupLayout vk_ral_adopt_one_pipeline_layout)
	FORBID_TEXT("${IQM_INIT}" "${RETIRED}"
		"IQM init regained create-then-adopt authority")
ENDFOREACH()
EXTRACT_SPAN("${VK_SOURCE}" "void vk_shutdown_iqm_gpu_skinning( void )"
	"vk_create_iqm_vbo" IQM_SHUTDOWN)
FOREACH(RETIRED IN ITEMS qvkDestroyDescriptorSetLayout qvkDestroyPipelineLayout)
	FORBID_TEXT("${IQM_SHUTDOWN}" "${RETIRED}"
		"IQM shutdown regained raw layout destruction")
ENDFOREACH()
STRING(FIND "${IQM_SHUTDOWN}" "Ral_DestroyPipeline(" IQM_PIPE_DESTROY)
STRING(FIND "${IQM_SHUTDOWN}" "Ral_DestroyPipelineLayout(" IQM_LAYOUT_DESTROY)
STRING(FIND "${IQM_SHUTDOWN}" "Ral_DestroyBindGroupLayout(" IQM_BGL_DESTROY)
IF(IQM_PIPE_DESTROY EQUAL -1 OR IQM_LAYOUT_DESTROY EQUAL -1 OR IQM_BGL_DESTROY EQUAL -1
		OR NOT IQM_PIPE_DESTROY LESS IQM_LAYOUT_DESTROY
		OR NOT IQM_LAYOUT_DESTROY LESS IQM_BGL_DESTROY)
	MESSAGE(FATAL_ERROR "IQM pipeline -> pipeline-layout -> bind-group-layout teardown drifted")
ENDIF()
FOREACH(RETIRED IN ITEMS
	"ADOPT_PL( vk.iqmGpu.pipeline_layout"
	"KILL_PL( vk.iqmGpu.ral_pipeline_layout"
	"KILL_BGL( vk.iqmGpu.ral_bgl_bones"
	"Ral_AdoptBindGroupLayout( backend, vk.iqmGpu.set_layout_bones")
	FORBID_TEXT("${RAL_TEXTURES}${VK_SOURCE}" "${RETIRED}"
		"IQM direct owner leaked into central adoption lifecycle")
ENDFOREACH()
REQUIRE_TEXT("${RAL_TEXTURES}" "vk_shutdown_iqm_gpu_skinning();"
	"full shutdown does not retire IQM direct layout owners")
EXTRACT_SPAN("${RAL_TEXTURES}" "qboolean vk_ral_refresh_iqm_bone_bindgroup"
	"void vk_ral_release_entmat_bindgroup" IQM_BONE_OWNER)
FOREACH(NEEDLE IN ITEMS
	"vk.iqmGpu.ral_bone_descriptor[slot]"
	"buffer = vk.iqmGpu.ral_bone_buffer[slot];"
	"Ral_GetBufferSize( buffer ) != vk.iqmGpu.ring_size"
	"value.type = RAL_BIND_UNIFORM_BUFFER"
	"value.bufferRange = item"
	"createInfo.arena = vk.ral_descriptor_arena"
	"createInfo.arenaReceipt = &vk.ral_descriptor_arena_receipt"
	"Ral_CreateBindGroup(")
	REQUIRE_TEXT("${IQM_BONE_OWNER}" "${NEEDLE}"
		"IQM dynamic bone direct group missing")
ENDFOREACH()
FOREACH(RETIRED IN ITEMS Ral_AdoptBindGroup
	Ral_RegisterAdoptedBindGroupDynamicBuffer qvkAllocateDescriptorSets
	qvkUpdateDescriptorSets)
	STRING(FIND "${IQM_BONE_OWNER}" "${RETIRED}" POS)
	IF(NOT POS EQUAL -1)
		MESSAGE(FATAL_ERROR "IQM bone owner regained retired authority: ${RETIRED}")
	ENDIF()
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
	"Ral_CmdBindVertexBuffersExact( vk.cmd->ral_cmd, 0, 1,"
	"Ral_CmdBindIndexBufferExact( vk.cmd->ral_cmd, idxBuffer, 0,"
	"Ral_CmdDrawIndexed( vk.cmd->ral_cmd")
	REQUIRE_TEXT("${IQM_DRAW}" "${NEEDLE}"
		"ordinary IQM exact command operand missing")
ENDFOREACH()
STRING(FIND "${IQM_DRAW}" "Ral_CmdBindPipeline( vk.cmd->ral_cmd, vk.iqmGpu.ral_pipeline )" PIPE_POS)
STRING(FIND "${IQM_DRAW}" "vk.iqmGpu.ral_bone_descriptor[frameIdx], &iqmOff, 1u" BONE_POS)
STRING(FIND "${IQM_DRAW}" "textureGroup, NULL, 0u" TEXTURE_POS)
STRING(FIND "${IQM_DRAW}" "Ral_CmdBindVertexBuffersExact( vk.cmd->ral_cmd" VERTEX_POS)
STRING(FIND "${IQM_DRAW}" "Ral_CmdDrawIndexed( vk.cmd->ral_cmd" DRAW_POS)
IF(PIPE_POS EQUAL -1 OR BONE_POS EQUAL -1 OR TEXTURE_POS EQUAL -1
		OR VERTEX_POS EQUAL -1 OR DRAW_POS EQUAL -1
		OR NOT PIPE_POS LESS BONE_POS OR NOT BONE_POS LESS TEXTURE_POS
		OR NOT TEXTURE_POS LESS VERTEX_POS OR NOT VERTEX_POS LESS DRAW_POS)
	MESSAGE(FATAL_ERROR "ordinary IQM exact bind/draw order drifted")
ENDIF()

REQUIRE_TEXT("${IQM_MODEL}" "ordinaryImage->ralDescriptor"
	"ordinary IQM material did not pass the retained RAL group")
REQUIRE_TEXT("${RAL_TEXTURES}"
	"Ral_DestroyBindGroup( vk.iqmGpu.ral_bone_descriptor[slot] );"
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
