# SPDX-License-Identifier: GPL-3.0-or-later
# Shadow caster/dlight exact RAL bind-group migration policy.

IF(NOT DEFINED SOURCE_ROOT OR NOT IS_DIRECTORY "${SOURCE_ROOT}")
	MESSAGE(FATAL_ERROR "SOURCE_ROOT must name the q3now source tree")
ENDIF()

FILE(READ "${SOURCE_ROOT}/code/renderervk/vk.c" VK)
FILE(READ "${SOURCE_ROOT}/code/renderervk/vk.h" VK_H)
FILE(READ "${SOURCE_ROOT}/tests/ral_vulkan_dynamic_bind_test.c" HOST)

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

EXTRACT_SPAN("${VK}" "static qboolean vk_shadow_register_pipeline_layout_abis( void )"
	"vk_shadow_alloc_resources" ABI)
EXTRACT_SPAN("${VK}" "static qboolean vk_shadow_refresh_buffer_group("
	"static qboolean vk_shadow_register_pipeline_layout_abis( void )" MVP_HELPER)
EXTRACT_SPAN("${VK}" "void vk_render_shadow_map( void )"
	"vk_render_dlight_shadow —" CSM)
EXTRACT_SPAN("${VK}" "void vk_render_dlight_shadow( void )"
	"#endif // FEAT_SHADOW_MAPPING" DLIGHT)
EXTRACT_SPAN("${VK}" "static void vk_shadow_release_resources( void )"
	"vk_dlight_shadow_release_resources / vk_dlight_shadow_alloc_resources" RELEASE)

FORBID_TEXT("${VK}" "qvkCmdBindDescriptorSets" "renderer shadow/native bind ownership")
FORBID_TEXT("${VK}" "vkCmdBindDescriptorSets" "renderer shadow/native entry-point ownership")
FORBID_TEXT("${VK}${VK_H}" "shadowMap.descriptor"
	"retired standalone shadow sampler descriptor")

FOREACH(NEEDLE IN ITEMS
	"ral_shadowEntMatDesc"
	"ral_bgl_entmat"
	"ral_bgl_cascademvp"
	"ral_cascadeMvpDesc[NUM_COMMAND_BUFFERS]"
	"ral_bgl_bones"
	"ral_boneDesc[NUM_COMMAND_BUFFERS]"
	"ral_faceMvpDesc[NUM_COMMAND_BUFFERS]"
	"ral_entMatDesc[NUM_COMMAND_BUFFERS]")
	REQUIRE_TEXT("${VK_H}" "${NEEDLE}" "retained shadow inventory")
ENDFOREACH()

REQUIRE_COUNT("${ABI}" "Ral_RegisterExternalPipelineBindGroupLayouts[(]" 3
	"shadow pipeline set-layout ABI inventory")
FOREACH(NEEDLE IN ITEMS
	"vk.shadowMap.ral_depthPipeline, 2u, staticLayouts"
	"vk.shadowMap.ral_depthPipelineSkinned, 3u, skinnedLayouts"
	"vk.shadowMap.ral_depthPipelineAtest, 3u, atestLayouts"
	"atestLayouts[2] = vk_ral_get_bindless_layout();")
	REQUIRE_TEXT("${ABI}" "${NEEDLE}" "shadow pipeline ABI operand")
ENDFOREACH()

FOREACH(NEEDLE IN ITEMS
	"Ral_AdoptOwnedPipelineLayout("
	"vk.shadowMap.ral_bgl_entmat = Ral_AdoptBindGroupLayout("
	"vk.shadowMap.ral_bgl_cascademvp = Ral_AdoptBindGroupLayout("
	"vk.shadowMap.ral_bgl_bones = Ral_AdoptBindGroupLayout("
	"vk_ral_register_buffer( vk.shadowMap.cascadeMvpBuf[ci], cmBytes"
	"vk_ral_register_buffer( vk.shadowMap.boneBuf[bi], ringBytes"
	"vk_ral_register_buffer( vk.dlightShadow.faceMvpBuf[i], mvpSz"
	"vk_ral_register_buffer( vk.dlightShadow.entMatBuf[i], 64u"
	"vk_shadow_descriptor_pool_invalidate();")
	REQUIRE_TEXT("${VK}" "${NEEDLE}" "shadow retained lifecycle")
ENDFOREACH()
REQUIRE_TEXT("${VK}" "vk_ral_unregister_buffer( vk.dlightShadow.entMatBuf[i] )"
	"dlight entity buffer registry teardown")
FORBID_TEXT("${VK}" "static qboolean vk_shadow_adopt_group("
	"shadow bind-group adoption helper retirement")

FOREACH(NEEDLE IN ITEMS
	"Ral_GetBufferHandle( buffer ) != (void *)nativeBuffer"
	"bufferRange > Ral_GetBufferSize( buffer )"
	"type != RAL_BIND_UNIFORM_BUFFER"
	"type != RAL_BIND_STORAGE_BUFFER"
	"value.type = type;"
	"value.bufferRange = bufferRange;"
	"createInfo.arena = vk.ral_descriptor_arena;"
	"createInfo.arenaReceipt = &vk.ral_descriptor_arena_receipt;"
	"candidate = Ral_CreateBindGroup( vk_ral_get_backend(), &createInfo );"
	"*nativeMirror = (VkDescriptorSet)Ral_GetBindGroupHandle( candidate );")
	REQUIRE_TEXT("${MVP_HELPER}" "${NEEDLE}" "shadow arena-owned MVP helper")
ENDFOREACH()
FOREACH(RETIRED IN ITEMS Ral_AdoptBindGroup Ral_RegisterAdoptedBindGroupDynamicBuffer)
	FORBID_TEXT("${MVP_HELPER}" "${RETIRED}" "shadow MVP adoption fallback")
ENDFOREACH()
FOREACH(RETIRED IN ITEMS
	"qvkAllocateDescriptorSets( vk.device, &da, &vk.shadowMap.cascadeMvpDesc"
	"qvkAllocateDescriptorSets( vk.device, &da, &vk.dlightShadow.faceMvpDesc"
	"qvkAllocateDescriptorSets( vk.device, &dsAlloc, &vk.cmd->shadowEntMatDesc"
	"qvkAllocateDescriptorSets( vk.device, &ba, &vk.shadowMap.boneDesc"
	"qvkAllocateDescriptorSets( vk.device, &da, &vk.dlightShadow.entMatDesc")
	FORBID_TEXT("${VK}" "${RETIRED}" "shadow MVP raw/adoption authority")
ENDFOREACH()
FOREACH(NEEDLE IN ITEMS
	"vk_shadow_refresh_buffer_group(\n\t\t\t\t\t&vk.shadowMap.ral_cascadeMvpDesc[cmd0]"
	"&vk.shadowMap.cascadeMvpDesc[cmd0]"
	"vk_shadow_refresh_buffer_group(\n\t\t\t\t&vk.dlightShadow.ral_faceMvpDesc[vk.cmd_index]"
	"&vk.dlightShadow.faceMvpDesc[vk.cmd_index]"
	"vk_shadow_refresh_buffer_group( &vk.cmd->ral_shadowEntMatDesc"
	"RAL_BIND_STORAGE_BUFFER, vk.cmd->shadowEntMatBuf"
	"vk_shadow_refresh_buffer_group( &vk.shadowMap.ral_boneDesc[fi]"
	"RAL_BIND_UNIFORM_BUFFER, vk.shadowMap.boneBuf[fi]"
	"vk_shadow_refresh_buffer_group(\n\t\t\t\t&vk.dlightShadow.ral_entMatDesc[vk.cmd_index]"
	"RAL_BIND_STORAGE_BUFFER,\n\t\t\t\tvk.dlightShadow.entMatBuf[vk.cmd_index], 64u")
	REQUIRE_TEXT("${VK}" "${NEEDLE}" "shadow arena-owned MVP callsite")
ENDFOREACH()

REQUIRE_COUNT("${CSM}" "Ral_CmdBindBindGroupDynamicExact[(]" 8
	"CSM exact bind callsite inventory")
REQUIRE_COUNT("${DLIGHT}" "Ral_CmdBindBindGroupDynamicExact[(]" 2
	"dlight exact bind callsite inventory")
FOREACH(NEEDLE IN ITEMS
	"vk.cmd->ral_shadowEntMatDesc, NULL, 0u"
	"vk.shadowMap.ral_cascadeMvpDesc[vk.cmd_index]"
	"vk.shadowMap.ral_cascadeMvpDesc[fi], &cmvpOff, 1u"
	"vk.shadowMap.ral_boneDesc[fi], &boneOff, 1u"
	"bindlessGroup, NULL, 0u"
	"Ral_EndRendering( vk.cmd->ral_cmd );")
	REQUIRE_TEXT("${CSM}" "${NEEDLE}" "CSM exact bind/fail-closed operand")
ENDFOREACH()
FOREACH(NEEDLE IN ITEMS
	"vk.dlightShadow.ral_entMatDesc[vk.cmd_index], NULL, 0u"
	"vk.dlightShadow.ral_faceMvpDesc[vk.cmd_index]"
	"&mvpOff, 1u"
	"Ral_EndRendering( vk.cmd->ral_cmd );")
	REQUIRE_TEXT("${DLIGHT}" "${NEEDLE}" "dlight exact bind/fail-closed operand")
ENDFOREACH()

STRING(FIND "${RELEASE}" "vk_shadow_descriptor_pool_invalidate();" GROUP_POS)
STRING(FIND "${RELEASE}" "Ral_DestroyPipeline( vk.shadowMap.ral_depthPipeline )" PIPE_POS)
STRING(FIND "${RELEASE}" "Ral_DestroyPipelineLayout( vk.shadowMap.ral_depthLayout )" RAL_LAYOUT_POS)
STRING(FIND "${RELEASE}" "Ral_DestroyBindGroupLayout( vk.shadowMap.ral_bgl_entmat )" BGL_POS)
STRING(FIND "${RELEASE}" "qvkDestroyDescriptorSetLayout( vk.device, vk.shadowMap.set_layout_entmat" RAW_BGL_POS)
IF(GROUP_POS EQUAL -1 OR PIPE_POS EQUAL -1 OR RAL_LAYOUT_POS EQUAL -1
		OR BGL_POS EQUAL -1 OR RAW_BGL_POS EQUAL -1
		OR NOT GROUP_POS LESS PIPE_POS OR NOT PIPE_POS LESS RAL_LAYOUT_POS
		OR NOT RAL_LAYOUT_POS LESS BGL_POS
		OR NOT BGL_POS LESS RAW_BGL_POS)
	MESSAGE(FATAL_ERROR "shadow child-before-parent teardown order drifted")
ENDIF()
FORBID_TEXT("${RELEASE}" "qvkDestroyPipelineLayout"
	"shadow pipeline-layout destroy escaped RAL ownership")

FOREACH(NEEDLE IN ITEMS
	"Ral_ValidateBindGroupDynamicExact("
	"command.boundBindGroups[3] == &sentinel"
	"offsets[0] = 1u;"
	"uniformBuffer.size = 300u;"
	"group.layout = &otherLayout;"
	"bindCalls == 0u")
	REQUIRE_TEXT("${HOST}" "${NEEDLE}" "headless exact-bind mutation host")
ENDFOREACH()
FOREACH(NEEDLE IN ITEMS
	"Ral_AdoptOwnedPipelineLayout("
	"destroyPipelineLayoutCalls == 0u"
	"destroyPipelineLayoutCalls == 1u"
	"capturedDestroyedPipelineLayout == (VkPipelineLayout)(uintptr_t)0x71u")
	REQUIRE_TEXT("${HOST}" "${NEEDLE}" "owned pipeline-layout lifecycle host")
ENDFOREACH()

MESSAGE(STATUS "RAL shadow exact bind-group policy: arena-owned buffer cohorts + CSM 8 + dlight 2 callsites, retained lifecycle PASS")
