# SPDX-License-Identifier: GPL-3.0-or-later
# Main/MSDF exact RAL bind-group and retained lifecycle policy.

IF(NOT DEFINED SOURCE_ROOT OR NOT IS_DIRECTORY "${SOURCE_ROOT}")
	MESSAGE(FATAL_ERROR "SOURCE_ROOT must name the q3now source tree")
ENDIF()

FILE(READ "${SOURCE_ROOT}/code/renderervk/vk.c" VK)
FILE(READ "${SOURCE_ROOT}/code/renderervk/vk.h" VK_H)
FILE(READ "${SOURCE_ROOT}/code/renderervk/vk_ral_textures.c" ADOPT)
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

EXTRACT_SPAN("${VK}" "static qboolean vk_bind_descriptor_sets( void )"
	"void vk_bind_pipeline( uint32_t pipeline )" BIND)
STRING(FIND "${VK}" "qboolean vk_bloom( void )" BLOOM_POS)
IF(BLOOM_POS EQUAL -1)
	MESSAGE(FATAL_ERROR "missing vk_bloom")
ENDIF()
STRING(SUBSTRING "${VK}" ${BLOOM_POS} -1 BLOOM)
EXTRACT_SPAN("${VK}" "static void vk_entmat_materialize_slot_after_idle("
	"static void vk_entmat_ensure_temporal_ring(" ENTMAT)

# Main and MSDF pipelines must publish the same exact 0/1/2 parents and their
# distinct overloaded set-3 layout before any exact bind can succeed.
FOREACH(NEEDLE IN ITEMS
	"struct ralBindGroupLayout_s *ral_bgl_engine_resources;"
	"struct ralBindGroupLayout_s *ral_bgl_entmat;"
	"struct ralBindGroupLayout_s *ral_bgl_msdf;"
	"layouts[0] = vk.ral_bgl_uniform;"
	"layouts[1] = vk_ral_get_bindless_layout();"
	"layouts[2] = vk.ral_bgl_engine_resources;"
	"layouts[3] = msdf ? vk.ral_bgl_msdf : vk.ral_bgl_entmat;"
	"Ral_RegisterExternalPipelineBindGroupLayouts("
	"vk_ral_register_world_pipeline_bind_abi( pipeline, msdf )")
	REQUIRE_TEXT("${VK}${VK_H}" "${NEEDLE}" "world exact layout ABI")
ENDFOREACH()

# Retained descriptor children must be rebuilt from the current pool/buffer
# identities and destroyed before their descriptor pool or raw buffer parents.
FOREACH(NEEDLE IN ITEMS
	"vk.engineResources.ral_descriptor"
	"vk.msdf.ral_descriptor[i]"
	"Ral_RegisterAdoptedBindGroupDynamicBuffer("
	"vk_ral_refresh_entmat_bindgroup( uint32_t slot )"
	"vk_ral_release_entmat_bindgroup( uint32_t slot )"
	"DESTROY_RETAINED_BG( vk.tess[i].ral_entMatDesc );"
	"DESTROY_RETAINED_BG( vk.msdf.ral_descriptor[i] );")
	REQUIRE_TEXT("${ADOPT}" "${NEEDLE}" "world retained bind-group lifecycle")
ENDFOREACH()
STRING(FIND "${ENTMAT}" "vk_ral_release_entmat_bindgroup" RELEASE_POS)
STRING(FIND "${ENTMAT}" "qvkDestroyBuffer" DESTROY_POS)
STRING(FIND "${ENTMAT}" "vk_ral_refresh_entmat_bindgroup" REFRESH_POS)
IF(RELEASE_POS EQUAL -1 OR DESTROY_POS EQUAL -1 OR REFRESH_POS EQUAL -1
		OR NOT RELEASE_POS LESS DESTROY_POS OR NOT DESTROY_POS LESS REFRESH_POS)
	MESSAGE(FATAL_ERROR "entity-matrix child/parent refresh order drifted")
ENDIF()

# The central binder has no native descriptor command. Dirty-range semantics
# resolve to exact per-set binds, and a mismatch returns before draw issuance.
FORBID_TEXT("${BIND}" "qvkCmdBindDescriptorSets" "central world binder")
REQUIRE_COUNT("${BIND}" "Ral_CmdBindBindGroupDynamicExact[(]" 2
	"central exact bind callsite inventory")
FOREACH(NEEDLE IN ITEMS
	"group = vk.cmd->ral_uniform_descriptor;"
	"group = vk_ral_get_bindless_set();"
	"group = vk.engineResources.ral_descriptor;"
	"group = vk.msdf.ral_descriptor[vk.cmd_index];"
	"WIRED_ENTITY_MAT_SET, vk.cmd->ral_entMatDesc"
	"Ral_CmdBindPipeline( vk.cmd->ral_cmd, NULL );"
	"return ok;")
	REQUIRE_TEXT("${BIND}" "${NEEDLE}" "central exact bind operand")
ENDFOREACH()
REQUIRE_TEXT("${VK}" "if ( !vk_bind_descriptor_sets() ) {"
	"draw rejection on bind mismatch")

# Bloom never restores a stale raw pipeline/set. It invalidates both cache
# identities so the next geometry draw performs the exact RAL transaction.
FORBID_TEXT("${BLOOM}" "qvkCmdBindDescriptorSets" "bloom descriptor restore")
REQUIRE_TEXT("${BLOOM}" "Ral_CmdBindPipeline( vk.cmd->ral_cmd, NULL );"
	"bloom RAL pipeline invalidation")
REQUIRE_TEXT("${BLOOM}" "vk.cmd->last_ral_pipeline = NULL;"
	"bloom RAL cache invalidation")
REQUIRE_TEXT("${BLOOM}" "vk.cmd->last_pipeline_layout = VK_NULL_HANDLE;"
	"bloom layout cache invalidation")

# Backend host mutates layout registration, exact dynamic offsets and verifies
# output atomicity without opening a native window.
FOREACH(NEEDLE IN ITEMS
	"Ral_RegisterExternalPipelineBindGroupLayouts("
	"!Ral_RegisterExternalPipelineBindGroupLayouts("
	"!Ral_CmdBindBindGroupDynamicExact("
	"bindCalls == 0u")
	REQUIRE_TEXT("${HOST}" "${NEEDLE}" "world backend mutation host")
ENDFOREACH()

MESSAGE(STATUS "RAL main/MSDF exact bind policy: retained set0/1/2/3 cohort PASS")
