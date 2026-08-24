# SPDX-License-Identifier: GPL-3.0-or-later
# Main/MSDF exact RAL bind-group and retained lifecycle policy.

IF(NOT DEFINED SOURCE_ROOT OR NOT IS_DIRECTORY "${SOURCE_ROOT}")
	MESSAGE(FATAL_ERROR "SOURCE_ROOT must name the q3now source tree")
ENDIF()

FILE(READ "${SOURCE_ROOT}/code/render/ral/backends/vulkan/renderer/vk.c" VK)
FILE(READ "${SOURCE_ROOT}/code/render/ral/backends/vulkan/renderer/vk.h" VK_H)
FILE(READ "${SOURCE_ROOT}/code/render/ral/backends/vulkan/renderer/vk_ral_textures.c" ADOPT)
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
EXTRACT_SPAN("${VK}" "static qboolean vk_entmat_materialize_slot_after_idle("
	"static qboolean vk_entmat_ensure_temporal_ring(" ENTMAT)
EXTRACT_SPAN("${ADOPT}" "ralBindGroup_t *vk_ral_create_entmat_bindgroup_candidate("
	"void vk_ral_release_sprite_bindgroup( uint32_t slot )" ENTMAT_GROUP)

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

# Retained descriptor children are direct current-arena groups. Native
# descriptor fields are compatibility mirrors only; group children are
# destroyed before their arena or raw buffer parents.
FOREACH(NEEDLE IN ITEMS
	"vk.engineResources.ral_descriptor"
	"vk.msdf.ral_descriptor[i]"
	"vk_ral_refresh_entmat_bindgroup( uint32_t slot )"
	"vk_ral_release_entmat_bindgroup( uint32_t slot )"
	"vk_ral_release_entmat_bindgroup( i );"
	"DESTROY_RETAINED_BG( vk.msdf.ral_descriptor[i] );")
	REQUIRE_TEXT("${ADOPT}" "${NEEDLE}" "world retained bind-group lifecycle")
ENDFOREACH()
STRING(FIND "${ENTMAT}" "candidateGroup = vk_ral_create_entmat_bindgroup_candidate" CREATE_POS)
STRING(FIND "${ENTMAT}" "*owner = candidateShadow;" PUBLISH_POS)
STRING(FIND "${ENTMAT}" "if ( retiredGroup ) Ral_DestroyBindGroup( retiredGroup );" GROUP_DESTROY_POS)
STRING(FIND "${ENTMAT}" "VK_RalBufferShadowRelease( &retiredShadow );" BUFFER_DESTROY_POS)
IF(CREATE_POS EQUAL -1 OR PUBLISH_POS EQUAL -1 OR GROUP_DESTROY_POS EQUAL -1
		OR BUFFER_DESTROY_POS EQUAL -1 OR NOT CREATE_POS LESS PUBLISH_POS
		OR NOT PUBLISH_POS LESS GROUP_DESTROY_POS
		OR NOT GROUP_DESTROY_POS LESS BUFFER_DESTROY_POS)
	MESSAGE(FATAL_ERROR "entity-matrix candidate-first child-before-parent refresh order drifted")
ENDIF()
FOREACH(RETIRED IN ITEMS qvkAllocateDescriptorSets qvkUpdateDescriptorSets)
	FORBID_TEXT("${ENTMAT}" "${RETIRED}" "entity-matrix raw descriptor authority")
ENDFOREACH()
FOREACH(NEEDLE IN ITEMS
	"Ral_BindGroupArenaReceiptValid("
	"vk.ral_descriptor_arena_receipt.backendIdentity"
	"vk.ral_descriptor_arena_receipt.arenaIdentity"
	"value.type = RAL_BIND_STORAGE_BUFFER;"
	"value.bufferRange = bufferSize;"
	"createInfo.layout = vk.ral_bgl_entmat;"
	"createInfo.arena = vk.ral_descriptor_arena;"
	"createInfo.arenaReceipt = &vk.ral_descriptor_arena_receipt;"
	"candidate = Ral_CreateBindGroup( s_ral_backend, &createInfo );"
	"vk.tess[slot].ral_entMatDesc = candidate;"
	"vk.tess[slot].entMatDesc = rawCandidate;"
	"if ( retired ) Ral_DestroyBindGroup( retired );")
	REQUIRE_TEXT("${ENTMAT_GROUP}" "${NEEDLE}" "direct entity-matrix RAL cohort")
ENDFOREACH()
FOREACH(RETIRED IN ITEMS Ral_AdoptBindGroup qvkAllocateDescriptorSets qvkUpdateDescriptorSets)
	FORBID_TEXT("${ENTMAT_GROUP}" "${RETIRED}" "entity-matrix adoption/raw fallback")
ENDFOREACH()
STRING(FIND "${ENTMAT_GROUP}" "vk.tess[slot].ral_entMatDesc = candidate;" PUBLISH_GROUP)
STRING(FIND "${ENTMAT_GROUP}" "vk.tess[slot].entMatDesc = rawCandidate;" PUBLISH_RAW)
STRING(FIND "${ENTMAT_GROUP}" "if ( retired ) Ral_DestroyBindGroup( retired );" RETIRE_GROUP)
IF(PUBLISH_GROUP EQUAL -1 OR PUBLISH_RAW LESS PUBLISH_GROUP
		OR RETIRE_GROUP LESS PUBLISH_RAW)
	MESSAGE(FATAL_ERROR "entity-matrix candidate publication/retirement order drifted")
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
