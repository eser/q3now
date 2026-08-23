# SPDX-License-Identifier: GPL-3.0-or-later
# Forward+ lit exact three-set RAL bind and retained lifecycle policy.

IF(NOT DEFINED SOURCE_ROOT OR NOT IS_DIRECTORY "${SOURCE_ROOT}")
	MESSAGE(FATAL_ERROR "SOURCE_ROOT must name the q3now source tree")
ENDIF()

FILE(READ "${SOURCE_ROOT}/code/renderervk/vk.c" VK)
FILE(READ "${SOURCE_ROOT}/code/renderervk/vk.h" VK_H)
FILE(READ "${SOURCE_ROOT}/code/renderervk/vk_ral_textures.c" ADOPT)
FILE(READ "${SOURCE_ROOT}/code/renderervk/vk_ral_textures.h" ADOPT_H)
FILE(READ "${SOURCE_ROOT}/tests/ral_vulkan_dynamic_bind_test.c" HOST)
FILE(READ "${SOURCE_ROOT}/tests/ral_vulkan_bind_group_arena_test.c" ARENA_HOST)

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
		MESSAGE(FATAL_ERROR "${LABEL}: expected ${EXPECTED} matches of '${REGEX}', found ${COUNT}")
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

EXTRACT_SPAN("${VK}" "void vk_forwardplus_lit_init( ralBackend_t *backend )"
	"void vk_forwardplus_build_clusters( void )" INIT)
EXTRACT_SPAN("${VK}" "void vk_forwardplus_lit_shutdown( void )"
	"void vk_forwardplus_lit_init( ralBackend_t *backend )" SHUTDOWN)
EXTRACT_SPAN("${VK}" "void vk_forwardplus_shutdown( void )"
	"void vk_forwardplus_init( ralBackend_t *backend )" PRODUCER_SHUTDOWN)
EXTRACT_SPAN("${VK}" "void vk_forwardplus_build_clusters( void )"
	"#define VK_BRDF_LUT_SIZE" CLUSTER_BUILD)
EXTRACT_SPAN("${VK}" "static qboolean vk_forwardplus_lit_refresh_set( uint32_t slot )
{"
	"void vk_forwardplus_dispatch( void )" REFRESH)
EXTRACT_SPAN("${VK}" "void vk_forwardplus_dispatch( void )"
	"static const void *s_fpClusterBuiltFor" DISPATCH)
EXTRACT_SPAN("${VK}" "static void vk_forwardplus_lit_invalidate_sets( void )
{"
	"void vk_forwardplus_lit_shutdown( void )" INVALIDATE)
EXTRACT_SPAN("${VK}" "static void vk_shadow_release_resources( void )
{"
	"static void vk_dlight_shadow_release_resources( void )" SHADOW_RELEASE)
EXTRACT_SPAN("${VK}" "static void vk_dlight_shadow_release_resources( void )
{"
	"static void vk_dlight_shadow_alloc_resources( void )" DLIGHT_RELEASE)
EXTRACT_SPAN("${VK}" "static void vk_dlight_shadow_alloc_resources( void )
{"
	"static void vk_create_attachments( void )" DLIGHT_ALLOC)
EXTRACT_SPAN("${VK}" "void vk_draw_forwardplus( Vk_Depth_Range depth_range )"
	"void vk_begin_main_render_pass( void )" DRAW)
EXTRACT_SPAN("${VK}" "static void vk_resize_geometry_buffer( void )"
	"qboolean vk_temporal_motion_seal_primary( void )" RESIZE)

# Exact portable layout inventory: set0 dynamic UBO, set1 central bindless,
# set2 eight fragment resources. The pipeline must publish this exact vector.
FOREACH(NEEDLE IN ITEMS
	"struct ralBindGroup_s *ral_uniform_descriptor;"
	"struct ralBindGroupLayout_s *ral_fpLitSetLayout;"
	"struct ralBindGroup_s       *ral_fpLitSet[NUM_COMMAND_BUFFERS];"
	"struct ralTextureView_s     *ral_fpLitShadowView[NUM_COMMAND_BUFFERS];")
	REQUIRE_TEXT("${VK_H}" "${NEEDLE}" "Forward+ retained inventory")
ENDFOREACH()
FOREACH(NEEDLE IN ITEMS
	"e.dynamicOffset = qtrue;"
	"vk.ral_bgl_uniform = Ral_AdoptBindGroupLayout( backend, vk.set_layout_uniform"
	"vk_create_effect_bind_group_layout( e, ARRAY_LEN( e ),"
	"e[0].type = RAL_BIND_STORAGE_BUFFER;"
	"e[1].type = RAL_BIND_STORAGE_BUFFER;"
	"e[2].type = RAL_BIND_UNIFORM_BUFFER;"
	"e[3].type = RAL_BIND_SAMPLED_TEXTURE;"
	"e[3].textureViewType = RAL_BIND_TEXTURE_VIEW_2D;"
	"e[4].type = RAL_BIND_SAMPLER;"
	"e[5].type = RAL_BIND_UNIFORM_BUFFER;"
	"e[6].type = RAL_BIND_STORAGE_BUFFER;"
	"e[7].type = RAL_BIND_UNIFORM_BUFFER;"
	"layouts[0] = vk.ral_bgl_uniform;"
	"layouts[1] = vk_ral_get_bindless_layout();"
	"layouts[2] = vk.ral_fpLitSetLayout;"
	"candidate = Ral_CreatePipelineLayout( vk_ral_get_backend(), &createInfo );"
	"vk.ral_fpLitLayout = candidate;"
	"vk.fpLitLayout = native;")
	REQUIRE_TEXT("${VK}" "${NEEDLE}" "Forward+ exact layout ABI")
ENDFOREACH()
FOREACH(RETIRED IN ITEMS qvkCreateDescriptorSetLayout qvkCreatePipelineLayout
	Ral_AdoptBindGroupLayout vk_ral_adopt_one_pipeline_layout
	Ral_RegisterExternalPipelineBindGroupLayouts)
	FORBID_TEXT("${INIT}" "${RETIRED}"
		"Forward+ direct layout ownership")
ENDFOREACH()
FOREACH(RETIRED IN ITEMS qvkDestroyDescriptorSetLayout qvkDestroyPipelineLayout)
	FORBID_TEXT("${SHUTDOWN}" "${RETIRED}"
		"Forward+ direct layout teardown")
ENDFOREACH()

# Set-2 is one immutable direct RAL arena child over all eight resources.  The
# native VkDescriptorSet field is only a compatibility mirror; no raw allocate,
# update, or post-allocation adoption authority may return to this path.
FOREACH(NEEDLE IN ITEMS
	"Ral_BindGroupArenaReceiptValid( &vk.ral_descriptor_arena_receipt )"
	"vk.ral_descriptor_arena_receipt.backendIdentity != backend"
	"vk.ral_descriptor_arena_receipt.arenaIdentity"
	"tileParams = vk_ral_lookup_buffer( vk.fpTileParamsBuf[slot] );"
	"shadowParams = vk_ral_lookup_buffer( vk.dlightShadow.paramsBuf[slot] );"
	"clusterParams = vk_ral_lookup_buffer( vk.fpClusterParamsBuf );"
	"vk.dlightShadow.ral_image && vk.dlightShadow.view"
	"vk.shadowMap.ral_image && vk.shadowMap.view"
	"tr.whiteImage && tr.whiteImage->ralDescriptorTexture"
	"shadowViewCandidate = Ral_AdoptTextureViewExact( backend,"
	".binding=4u,"
	".binding=5u,"
	".binding=6u,"
	".binding=7u,"
	".binding=8u,"
	".binding=9u,"
	".binding=10u,"
	".binding=11u,"
	"createInfo.numValues = ARRAY_LEN( values );"
	"createInfo.arena = vk.ral_descriptor_arena;"
	"createInfo.arenaReceipt = &vk.ral_descriptor_arena_receipt;"
	"groupCandidate = Ral_CreateBindGroup( backend, &createInfo );"
	"groupRetired = vk.ral_fpLitSet[slot];"
	"shadowViewRetired = vk.ral_fpLitShadowView[slot];"
	"vk.ral_fpLitSet[slot] = groupCandidate;"
	"vk.ral_fpLitShadowView[slot] = shadowViewCandidate;"
	"vk.fpLitSet[slot] = (VkDescriptorSet)rawCandidate;"
	"if ( groupCandidate ) Ral_DestroyBindGroup( groupCandidate );"
	"if ( shadowViewCandidate ) Ral_DestroyTextureView( shadowViewCandidate );")
	REQUIRE_TEXT("${REFRESH}" "${NEEDLE}" "Forward+ direct set-2 cohort")
ENDFOREACH()
REQUIRE_COUNT("${REFRESH}" "Ral_CreateBindGroup[(]" 1
	"Forward+ direct set-2 creation authority")
FOREACH(FORBIDDEN IN ITEMS
	"qvkAllocateDescriptorSets"
	"qvkUpdateDescriptorSets"
	"Ral_AdoptBindGroup("
	"VkWriteDescriptorSet"
	"VkDescriptorBufferInfo"
	"VkDescriptorImageInfo")
	FORBID_TEXT("${REFRESH}" "${FORBIDDEN}" "Forward+ direct set-2 refresh")
	FORBID_TEXT("${DISPATCH}" "${FORBIDDEN}" "Forward+ set-2 dispatch")
ENDFOREACH()
REQUIRE_TEXT("${DISPATCH}" "!vk_forwardplus_lit_refresh_set( fi )"
	"Forward+ fence-safe lazy set-2 publication")

# Publication is candidate-first and retirement is typed child-before-parent.
STRING(FIND "${REFRESH}" "groupCandidate = Ral_CreateBindGroup" CREATE_GROUP_POS)
STRING(FIND "${REFRESH}" "groupRetired = vk.ral_fpLitSet[slot];" CAPTURE_RETIRED_POS)
STRING(FIND "${REFRESH}" "vk.ral_fpLitSet[slot] = groupCandidate;" PUBLISH_GROUP_POS)
STRING(FIND "${REFRESH}" "if ( groupRetired ) Ral_DestroyBindGroup" RETIRE_GROUP_POS)
STRING(FIND "${REFRESH}" "if ( shadowViewRetired ) Ral_DestroyTextureView" RETIRE_VIEW_POS)
IF(CREATE_GROUP_POS EQUAL -1 OR CAPTURE_RETIRED_POS EQUAL -1
		OR PUBLISH_GROUP_POS EQUAL -1 OR RETIRE_GROUP_POS EQUAL -1
		OR RETIRE_VIEW_POS EQUAL -1
		OR NOT CREATE_GROUP_POS LESS CAPTURE_RETIRED_POS
		OR NOT CAPTURE_RETIRED_POS LESS PUBLISH_GROUP_POS
		OR NOT PUBLISH_GROUP_POS LESS RETIRE_GROUP_POS
		OR NOT RETIRE_GROUP_POS LESS RETIRE_VIEW_POS)
	MESSAGE(FATAL_ERROR "Forward+ set-2 lost candidate-first group/view publication order")
ENDIF()

# Invalidation and every shadow producer teardown retire the group before the
# borrowed view/sampler/texture parents. Both raw UBO rings are registered for
# exact reverse lookup and unregistered before native buffer destruction.
STRING(FIND "${INVALIDATE}" "Ral_DestroyBindGroup( vk.ral_fpLitSet[i] )" INVALIDATE_GROUP_POS)
STRING(FIND "${INVALIDATE}" "Ral_DestroyTextureView( vk.ral_fpLitShadowView[i] )" INVALIDATE_VIEW_POS)
IF(INVALIDATE_GROUP_POS EQUAL -1 OR INVALIDATE_VIEW_POS EQUAL -1
		OR NOT INVALIDATE_GROUP_POS LESS INVALIDATE_VIEW_POS)
	MESSAGE(FATAL_ERROR "Forward+ set-2 invalidation lost group-before-view order")
ENDIF()
STRING(FIND "${SHADOW_RELEASE}" "vk_forwardplus_lit_invalidate_sets();" CSM_CHILD_POS)
STRING(FIND "${SHADOW_RELEASE}" "vk_ral_release_engine_resources_bindgroup();" CSM_PARENT_POS)
IF(CSM_CHILD_POS EQUAL -1 OR CSM_PARENT_POS EQUAL -1
		OR NOT CSM_CHILD_POS LESS CSM_PARENT_POS)
	MESSAGE(FATAL_ERROR "Forward+ set-2 no longer precedes CSM parent teardown")
ENDIF()
STRING(FIND "${DLIGHT_RELEASE}" "vk_forwardplus_lit_invalidate_sets();" DLIGHT_CHILD_POS)
STRING(FIND "${DLIGHT_RELEASE}" "if ( vk.dlightShadow.ral_image )" DLIGHT_PARENT_POS)
IF(DLIGHT_CHILD_POS EQUAL -1 OR DLIGHT_PARENT_POS EQUAL -1
		OR NOT DLIGHT_CHILD_POS LESS DLIGHT_PARENT_POS)
	MESSAGE(FATAL_ERROR "Forward+ set-2 no longer precedes dlight-atlas parent teardown")
ENDIF()
FOREACH(NEEDLE IN ITEMS
	"vk_ral_register_buffer( vk.fpTileParamsBuf[i]"
	"vk_ral_register_buffer( vk.fpClusterParamsBuf")
	REQUIRE_TEXT("${INIT}" "${NEEDLE}" "Forward+ raw UBO registry publication")
ENDFOREACH()
REQUIRE_TEXT("${DLIGHT_ALLOC}" "vk_ral_register_buffer( vk.dlightShadow.paramsBuf[i]"
	"Forward+ shadow UBO registry publication")
FOREACH(NEEDLE IN ITEMS
	"vk_ral_unregister_buffer( vk.fpTileParamsBuf[i] );"
	"vk_ral_unregister_buffer( vk.fpClusterParamsBuf );")
	REQUIRE_TEXT("${SHUTDOWN}" "${NEEDLE}" "Forward+ raw UBO registry teardown")
ENDFOREACH()
REQUIRE_TEXT("${DLIGHT_RELEASE}" "vk_ral_unregister_buffer( vk.dlightShadow.paramsBuf[i] );"
	"Forward+ shadow UBO registry teardown")

REQUIRE_TEXT("${SHUTDOWN}" "vk_forwardplus_lit_invalidate_sets();"
	"Forward+ shutdown child release")
REQUIRE_TEXT("${SHUTDOWN}" "Ral_DestroyBindGroupLayout( vk.ral_fpLitSetLayout )"
	"Forward+ set-2 layout teardown")
REQUIRE_TEXT("${SHUTDOWN}" "Ral_DestroyPipelineLayout( vk.ral_fpLitLayout )"
	"Forward+ pipeline-layout teardown")
STRING(FIND "${SHUTDOWN}" "Ral_DestroyPipeline(" FP_PIPE_DESTROY)
STRING(FIND "${SHUTDOWN}" "Ral_DestroyPipelineLayout(" FP_LAYOUT_DESTROY)
STRING(FIND "${SHUTDOWN}" "Ral_DestroyBindGroupLayout(" FP_BGL_DESTROY)
IF(FP_PIPE_DESTROY EQUAL -1 OR FP_LAYOUT_DESTROY EQUAL -1 OR FP_BGL_DESTROY EQUAL -1
		OR NOT FP_PIPE_DESTROY LESS FP_LAYOUT_DESTROY
		OR NOT FP_LAYOUT_DESTROY LESS FP_BGL_DESTROY)
	MESSAGE(FATAL_ERROR "Forward+ pipeline -> pipeline-layout -> bind-group-layout teardown drifted")
ENDIF()

# Set-2 wrappers borrow the light/tile/cluster producer buffers. Every producer
# shutdown or replacement must therefore invalidate the wrapper cohort first.
STRING(FIND "${PRODUCER_SHUTDOWN}" "vk_forwardplus_lit_invalidate_sets();" PRODUCER_INVALIDATE_POS)
STRING(FIND "${PRODUCER_SHUTDOWN}" "Ral_DestroyBuffer( vk.ral_fp_lights[i] )" PRODUCER_DESTROY_POS)
IF(PRODUCER_INVALIDATE_POS EQUAL -1 OR PRODUCER_DESTROY_POS EQUAL -1
		OR NOT PRODUCER_INVALIDATE_POS LESS PRODUCER_DESTROY_POS)
	MESSAGE(FATAL_ERROR "Forward+ set-2 wrappers no longer precede producer teardown")
ENDIF()
REQUIRE_COUNT("${CLUSTER_BUILD}" "vk_forwardplus_lit_invalidate_sets[(][)]" 3
	"Forward+ cluster replacement invalidation inventory")
STRING(FIND "${CLUSTER_BUILD}" "vk_forwardplus_lit_invalidate_sets();" CLUSTER_INVALIDATE_POS)
STRING(FIND "${CLUSTER_BUILD}" "Ral_DestroyBuffer( vk.ral_fp_clustergrid )" CLUSTER_DESTROY_POS)
IF(CLUSTER_INVALIDATE_POS EQUAL -1 OR CLUSTER_DESTROY_POS EQUAL -1
		OR NOT CLUSTER_INVALIDATE_POS LESS CLUSTER_DESTROY_POS)
	MESSAGE(FATAL_ERROR "Forward+ cluster replacement lost child-before-parent order")
ENDIF()

# The main uniform set is a direct arena group backed by the currently
# registered geometry buffer; its layout carries the dynamic-offset ABI and
# geometry replacement remains child-before-parent.
FOREACH(NEEDLE IN ITEMS
	"vk_ral_refresh_tess_uniform_bindgroup( uint32_t slot )"
	"value.type = RAL_BIND_UNIFORM_BUFFER;"
	"value.bufferRange = sizeof( vkUniform_t );"
	"createInfo.arena = vk.ral_descriptor_arena;"
	"createInfo.arenaReceipt = &vk.ral_descriptor_arena_receipt;"
	"vk.tess[slot].ral_uniform_descriptor = Ral_CreateBindGroup("
	"DESTROY_RETAINED_BG( vk.tess[i].ral_uniform_descriptor );")
	REQUIRE_TEXT("${ADOPT}" "${NEEDLE}" "tess exact direct dynamic group lifecycle")
ENDFOREACH()
REQUIRE_TEXT("${ADOPT_H}" "vk_ral_release_tess_uniform_bindgroup( uint32_t slot );"
	"geometry replacement release API")
STRING(FIND "${RESIZE}" "vk_ral_release_tess_uniform_bindgroup" RELEASE_POS)
STRING(FIND "${RESIZE}" "vk_release_geometry_buffers();" RAW_RELEASE_POS)
STRING(FIND "${RESIZE}" "vk_create_geometry_buffers" CREATE_POS)
STRING(FIND "${RESIZE}" "vk_ral_refresh_tess_uniform_bindgroup" REFRESH_POS)
IF(RELEASE_POS EQUAL -1 OR RAW_RELEASE_POS EQUAL -1 OR CREATE_POS EQUAL -1
		OR REFRESH_POS EQUAL -1 OR NOT RELEASE_POS LESS RAW_RELEASE_POS
		OR NOT RAW_RELEASE_POS LESS CREATE_POS OR NOT CREATE_POS LESS REFRESH_POS)
	MESSAGE(FATAL_ERROR "geometry replacement lost exact uniform child/parent order")
ENDIF()

# The draw contains no native descriptor bind. Exactly three checked set binds
# precede the indexed draw, with the only dynamic offset at set0.
FORBID_TEXT("${DRAW}" "qvkCmdBindDescriptorSets" "Forward+ draw")
REQUIRE_COUNT("${DRAW}" "Ral_CmdBindBindGroupDynamicExact[(]" 3
	"Forward+ exact set bind inventory")
FOREACH(NEEDLE IN ITEMS
	"vk.cmd->ral_uniform_descriptor, &uniformOffset, 1u"
	"bindless, NULL, 0u"
	"vk.ral_fpLitSet[vk.cmd_index], NULL, 0u"
	"Ral_CmdBindPipeline( vk.cmd->ral_cmd, NULL );"
	"Ral_CmdDrawIndexed( vk.cmd->ral_cmd")
	REQUIRE_TEXT("${DRAW}" "${NEEDLE}" "Forward+ exact draw operand")
ENDFOREACH()
STRING(FIND "${DRAW}" "Ral_CmdBindPipeline( vk.cmd->ral_cmd, vk.ral_fpLitPipeline )" PIPE_POS)
STRING(FIND "${DRAW}" "vk.cmd->ral_uniform_descriptor, &uniformOffset, 1u" SET0_POS)
STRING(FIND "${DRAW}" "bindless, NULL, 0u" SET1_POS)
STRING(FIND "${DRAW}" "vk.ral_fpLitSet[vk.cmd_index], NULL, 0u" SET2_POS)
STRING(FIND "${DRAW}" "Ral_CmdDrawIndexed( vk.cmd->ral_cmd" DRAW_POS)
IF(PIPE_POS EQUAL -1 OR SET0_POS EQUAL -1 OR SET1_POS EQUAL -1
		OR SET2_POS EQUAL -1 OR DRAW_POS EQUAL -1
		OR NOT PIPE_POS LESS SET0_POS OR NOT SET0_POS LESS SET1_POS
		OR NOT SET1_POS LESS SET2_POS OR NOT SET2_POS LESS DRAW_POS)
	MESSAGE(FATAL_ERROR "Forward+ pipeline/set0/set1/set2/draw order drifted")
ENDIF()

# Existing backend host mutates external layout registration and adopted
# dynamic-buffer authority without opening a native window. The arena host also
# proves current-receipt publication and stale/missing receipt rejection for
# direct groups; neither fixture opens a native window.
FOREACH(NEEDLE IN ITEMS
	"Ral_RegisterExternalPipelineBindGroupLayouts("
	"!Ral_RegisterExternalPipelineBindGroupLayouts("
	"Ral_RegisterAdoptedBindGroupDynamicBuffer("
	"!Ral_CmdBindBindGroupDynamicExact("
	"bindCalls == 0u")
	REQUIRE_TEXT("${HOST}" "${NEEDLE}" "Forward+ backend mutation host")
ENDFOREACH()
FOREACH(NEEDLE IN ITEMS
	"groupInfo.arenaReceipt = &first;"
	"group = Ral_CreateBindGroup( &backend, &groupInfo );"
	"groupInfo.arenaReceipt = &stale;"
	"Ral_CreateBindGroup( &backend, &groupInfo ) == NULL"
	"groupInfo.arenaReceipt = NULL;")
	REQUIRE_TEXT("${ARENA_HOST}" "${NEEDLE}" "Forward+ direct arena mutation host")
ENDFOREACH()

MESSAGE(STATUS "RAL Forward+ exact bind policy: direct arena set-2 cohort PASS")
