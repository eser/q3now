# SPDX-License-Identifier: GPL-3.0-or-later
# Generic MAIN/SCREENMAP exact RAL pipeline ownership policy.

IF(NOT DEFINED SOURCE_ROOT OR NOT IS_DIRECTORY "${SOURCE_ROOT}")
	MESSAGE(FATAL_ERROR "SOURCE_ROOT must name the q3now source tree")
ENDIF()

FILE(READ "${SOURCE_ROOT}/code/renderervk/vk.c" VK)
FILE(READ "${SOURCE_ROOT}/code/renderervk/vk.h" VK_H)
FILE(READ "${SOURCE_ROOT}/code/renderervk/vk_ral_textures.c" VK_RAL_TEXTURES)

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

EXTRACT_SPAN("${VK}" "static void create_pipeline( const Vk_Pipeline_Def *def,
		renderPass_t renderPassIndex, uint32_t def_index ) {"
	"static qboolean vk_temporal_cached_main_needs_recipe" CREATE)
EXTRACT_SPAN("${VK}" "static ralPipeline_t *vk_gen_pipeline( uint32_t index )"
	"uint32_t vk_find_pipeline_ext" GENERATE)
EXTRACT_SPAN("${VK}" "void vk_bind_pipeline( uint32_t pipeline )"
	"qboolean vk_entmat_active" BIND)
EXTRACT_SPAN("${VK}" "static void vk_destroy_pipelines( qboolean resetCounter )"
	"// tonemap default + variant pipelines." DESTROY)
EXTRACT_SPAN("${VK}" "// vk_destroy_samplers();"
	"vk_forwardplus_lit_invalidate_sets();" REWIND)
EXTRACT_SPAN("${VK}" "void vk_begin_post_bloom_render_pass( void )"
	"vk_tonemap  (Block 8 / Delta 2" POST_BLOOM)

FORBID_TEXT("${VK}" "qvkCmdBindPipeline" "renderer raw pipeline bind ownership")
FORBID_TEXT("${VK}" "vkCmdBindPipeline" "renderer native pipeline entry point")
FORBID_TEXT("${VK_H}" "VkPipeline handle[ RENDER_PASS_COUNT ]"
	"generic raw pipeline handle cache")
FORBID_TEXT("${VK_H}" "last_pipeline;"
	"generic raw command pipeline cache")
FOREACH(NEEDLE IN ITEMS "qvkCreatePipelineCache" "qvkDestroyPipelineCache"
	"VkPipelineCacheCreateInfo" "vk.pipelineCache")
	FORBID_TEXT("${VK}" "${NEEDLE}" "renderer-local native pipeline cache")
ENDFOREACH()
FORBID_TEXT("${VK_H}" "VkPipelineCache pipelineCache"
	"renderer-local native pipeline cache storage")
REQUIRE_TEXT("${VK}" "Ral_LoadPipelineCache( backend, cachePath );"
	"RAL pipeline-cache load authority")
REQUIRE_TEXT("${VK_RAL_TEXTURES}"
	"Ral_SavePipelineCache( s_ral_backend, cachePath );"
	"RAL pipeline-cache save authority")

FOREACH(NEEDLE IN ITEMS
	"!vk_is_world_render_pass( renderPassIndex )"
	"def_index >= vk.pipelines_count"
	"vk_ral_create_pipeline_from_gpinfo( &create_info, ralLayout,"
	"vk.pipelines[ def_index ].ral_handle[ renderPassIndex ]"
	"vk.pipeline_create_count++;"
	"vk_temporal_capture_generic_main_recipe( def_index, &create_info );")
	REQUIRE_TEXT("${CREATE}" "${NEEDLE}" "generic RAL pipeline creation")
ENDFOREACH()
FOREACH(NEEDLE IN ITEMS
	"qvkCreateGraphicsPipelines"
	"qvkDestroyPipeline"
	"VkPipeline pipeline")
	FORBID_TEXT("${CREATE}" "${NEEDLE}" "generic raw pipeline fallback")
ENDFOREACH()

FOREACH(NEEDLE IN ITEMS
	"if ( !vk_is_world_render_pass( pass ) )"
	"if ( !pipeline->ral_handle[ pass ] )"
	"create_pipeline( &pipeline->def, pass, index );"
	"return pipeline->ral_handle[ pass ];")
	REQUIRE_TEXT("${GENERATE}" "${NEEDLE}" "generic RAL cache authority")
ENDFOREACH()

FOREACH(NEEDLE IN ITEMS
	"ralpipe = vk_gen_pipeline( pipeline );"
	"if ( !ralpipe ) return;"
	"if ( ralpipe != vk.cmd->last_ral_pipeline )"
	"Ral_CmdBindPipeline( vk.cmd->ral_cmd, ralpipe );"
	"vk.cmd->last_ral_pipeline = ralpipe;")
	REQUIRE_TEXT("${BIND}" "${NEEDLE}" "generic RAL pipeline bind")
ENDFOREACH()
REQUIRE_COUNT("${BIND}" "Ral_CmdBindPipeline[(]" 1
	"generic RAL pipeline bind inventory")
FORBID_TEXT("${BIND}" "VkPipeline vkpipe" "generic native bind operand")

REQUIRE_TEXT("${DESTROY}"
	"Ral_DestroyPipeline( vk.pipelines[i].ral_handle[j] );"
	"generic full teardown ownership")
REQUIRE_TEXT("${DESTROY}" "vk.pipeline_create_count--;"
	"generic full teardown accounting")
REQUIRE_TEXT("${REWIND}"
	"Ral_DestroyPipeline( vk.pipelines[i].ral_handle[j] );"
	"generic rewind teardown ownership")
REQUIRE_TEXT("${REWIND}" "vk.pipeline_create_count--;"
	"generic rewind teardown accounting")
FORBID_TEXT("${DESTROY}" "qvkDestroyPipeline" "generic raw pipeline teardown")
FORBID_TEXT("${REWIND}" "qvkDestroyPipeline" "generic raw pipeline rewind")

FOREACH(NEEDLE IN ITEMS
	"vk.renderPassIndex = RENDER_PASS_POST_BLOOM;"
	"Ral_BeginRendering( vk.cmd->ral_cmd, &ri );")
	REQUIRE_TEXT("${POST_BLOOM}" "${NEEDLE}" "dedicated post-bloom RAL pass")
ENDFOREACH()
FORBID_TEXT("${POST_BLOOM}" "vk_bind_pipeline("
	"post-bloom generic cache entry")

MESSAGE(STATUS "RAL generic pipeline policy: MAIN/SCREENMAP exact RAL ownership, raw bind/cache absent")
