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
EXTRACT_SPAN("${VK}" "void vk_draw_forwardplus( Vk_Depth_Range depth_range )"
	"void vk_begin_main_render_pass( void )" DRAW)
EXTRACT_SPAN("${VK}" "static void vk_resize_geometry_buffer( void )"
	"qboolean vk_temporal_motion_seal_primary( void )" RESIZE)

# Exact portable layout inventory: set0 dynamic UBO, set1 central bindless,
# set2 eight fragment resources. The pipeline must publish this exact vector.
FOREACH(NEEDLE IN ITEMS
	"struct ralBindGroup_s *ral_uniform_descriptor;"
	"struct ralBindGroupLayout_s *ral_fpLitSetLayout;"
	"struct ralBindGroup_s       *ral_fpLitSet[NUM_COMMAND_BUFFERS];")
	REQUIRE_TEXT("${VK_H}" "${NEEDLE}" "Forward+ retained inventory")
ENDFOREACH()
FOREACH(NEEDLE IN ITEMS
	"e.dynamicOffset = qtrue;"
	"vk.ral_bgl_uniform = Ral_AdoptBindGroupLayout( backend, vk.set_layout_uniform"
	"vk.ral_fpLitSetLayout = Ral_AdoptBindGroupLayout( backend,"
	"e[0].type = RAL_BIND_STORAGE_BUFFER;"
	"e[1].type = RAL_BIND_STORAGE_BUFFER;"
	"e[2].type = RAL_BIND_UNIFORM_BUFFER;"
	"e[3].type = RAL_BIND_SAMPLED_TEXTURE;"
	"e[3].textureViewType = RAL_BIND_TEXTURE_VIEW_2D;"
	"e[4].type = RAL_BIND_SAMPLER;"
	"e[5].type = RAL_BIND_UNIFORM_BUFFER;"
	"e[6].type = RAL_BIND_STORAGE_BUFFER;"
	"e[7].type = RAL_BIND_UNIFORM_BUFFER;"
	"Ral_RegisterExternalPipelineBindGroupLayouts("
	"vk.ral_fpLitPipeline, 3u, layouts")
	REQUIRE_TEXT("${VK}" "${NEEDLE}" "Forward+ exact layout ABI")
ENDFOREACH()

# Lazy native set allocation immediately receives one retained wrapper. Every
# invalidation destroys the wrapper before forgetting the pool-owned identity.
FOREACH(NEEDLE IN ITEMS
	"vk.ral_fpLitSet[fi] = Ral_AdoptBindGroup("
	"vk.ral_fpLitSetLayout, \"wired-fp-lit-set2\""
	"static void vk_forwardplus_lit_invalidate_sets( void )"
	"Ral_DestroyBindGroup( vk.ral_fpLitSet[i] );"
	"vk.fpLitSet[i] = VK_NULL_HANDLE;")
	REQUIRE_TEXT("${VK}" "${NEEDLE}" "Forward+ set-2 retained lifecycle")
ENDFOREACH()
REQUIRE_TEXT("${SHUTDOWN}" "vk_forwardplus_lit_invalidate_sets();"
	"Forward+ shutdown child release")
REQUIRE_TEXT("${SHUTDOWN}" "Ral_DestroyBindGroupLayout( vk.ral_fpLitSetLayout )"
	"Forward+ set-2 layout teardown")

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

# The main uniform set is a true dynamic adopted group backed by the currently
# registered geometry buffer, and geometry replacement is child-before-parent.
FOREACH(NEEDLE IN ITEMS
	"vk_ral_refresh_tess_uniform_bindgroup( uint32_t slot )"
	"Ral_RegisterAdoptedBindGroupDynamicBuffer("
	"group, 0u, buffer, 0u, sizeof( vkUniform_t )"
	"DESTROY_RETAINED_BG( vk.tess[i].ral_uniform_descriptor );")
	REQUIRE_TEXT("${ADOPT}" "${NEEDLE}" "tess exact dynamic group lifecycle")
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
# dynamic-buffer authority without opening a native window.
FOREACH(NEEDLE IN ITEMS
	"Ral_RegisterExternalPipelineBindGroupLayouts("
	"!Ral_RegisterExternalPipelineBindGroupLayouts("
	"Ral_RegisterAdoptedBindGroupDynamicBuffer("
	"!Ral_CmdBindBindGroupDynamicExact("
	"bindCalls == 0u")
	REQUIRE_TEXT("${HOST}" "${NEEDLE}" "Forward+ backend mutation host")
ENDFOREACH()

MESSAGE(STATUS "RAL Forward+ exact bind policy: retained set0/set1/set2 cohort PASS")
