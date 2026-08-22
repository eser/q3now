# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 Wired Engine contributors

if(NOT DEFINED ROOT)
	message(FATAL_ERROR "ROOT is required")
endif()
file(READ "${ROOT}/code/renderervk/vk.c" VK)
file(READ "${ROOT}/code/renderervk/vk_ral_textures.c" TEXTURES)
file(READ "${ROOT}/code/renderervk/vk_bindless_publication.c" LEDGER)
file(READ "${ROOT}/code/renderervk/vk_bindless_publication.h" ABI)
file(READ "${ROOT}/code/renderervk/vk_bindless_cohort.c" COHORT)
file(READ "${ROOT}/code/renderervk/vk_bindless_cohort.h" COHORT_ABI)
file(READ "${ROOT}/tests/vk_bindless_cohort_test.c" COHORT_HOST)
file(READ "${ROOT}/code/renderervk/tr_local.h" TR_LOCAL)
file(GLOB RENDERER_VK_C "${ROOT}/code/renderervk/*.c")

function(require_text haystack needle label)
	string(FIND "${haystack}" "${needle}" pos)
	if(pos EQUAL -1)
		message(FATAL_ERROR "bindless publication policy missing ${label}: ${needle}")
	endif()
endfunction()

foreach(source IN LISTS RENDERER_VK_C)
	if(source STREQUAL "${ROOT}/code/renderervk/vk.c"
			OR source STREQUAL "${ROOT}/code/renderervk/vk_ral_textures.c")
		continue()
	endif()
	file(READ "${source}" source_text)
	foreach(writer IN ITEMS Ral_BindGroupSetTextureAt Ral_BindGroupSetTextureViewAt
			Ral_BindGroupSetTextureViewsAt Ral_BindGroupSetSamplerAt)
		string(FIND "${source_text}" "${writer}" escaped_writer)
		if(NOT escaped_writer EQUAL -1)
			message(FATAL_ERROR "global bindless writer escaped allowlisted TUs: ${source}: ${writer}")
		endif()
	endforeach()
	string(FIND "${source_text}" "WIRED_BINDLESS_BIND_IMAGES" escaped_raw_binding0)
	string(FIND "${source_text}" "WIRED_BINDLESS_BIND_ARRAY_IMAGES" escaped_raw_binding2)
	string(FIND "${source_text}" "qvkUpdateDescriptorSets" escaped_raw_update)
	if(NOT escaped_raw_update EQUAL -1 AND
			(NOT escaped_raw_binding0 EQUAL -1 OR NOT escaped_raw_binding2 EQUAL -1))
		message(FATAL_ERROR "raw bindless writer escaped vk.c allowlist: ${source}")
	endif()
endforeach()

foreach(needle IN ITEMS
	"VK_BINDLESS_PUBLICATION_IMAGE_SLOTS 4096u"
	"VK_BINDLESS_PUBLICATION_SAMPLER_SLOTS 32u"
	"VK_BINDLESS_PUBLICATION_LEGACY_EXACT"
	"VK_BINDLESS_PUBLICATION_TOMBSTONE"
	"ordinaryDescriptorIdentity"
	"samplerDefinitionDigest"
	"imageSlotGeneration"
	"samplerSlotGeneration")
	require_text("${ABI}" "${needle}" "fixed receipt ABI")
endforeach()
string(REGEX MATCH "uint64_t[ \t]+bindlessOwnerGeneration;" owner_epoch "${TR_LOCAL}")
if(NOT owner_epoch)
	message(FATAL_ERROR "bindless publication policy missing image owner epoch")
endif()
require_text("${LEDGER}" "image->kind != VK_BINDLESS_PUBLICATION_LEGACY_EXACT" "ordinary exact-kind gate")
require_text("${LEDGER}" "!ReceiptValid( a ) || !ReceiptValid( b )" "two-sided receipt validation")
require_text("${LEDGER}" "VK_BindlessPublicationPoisonSetAfterWrite" "post-writer poison authority")

# The H5 resource parent is an exact set-cohort receipt, not texture residency.
# The product accessor first converges the central ledger, then delegates all
# output-atomic validation/publication to the pure host-mutated builder.
foreach(needle IN ITEMS backend layout set rawLayout rawSet setGeneration ready)
	require_text("${COHORT_ABI}" "${needle}" "bindless cohort receipt field")
endforeach()
foreach(needle IN ITEMS
	"publicationInitialized != qtrue"
	"publication->setIdentity != (uintptr_t)set"
	"publication->setGeneration == UINT64_MAX"
	"*outReceipt = candidate"
	"VK_BindlessCohortReceiptExact")
	require_text("${COHORT}" "${needle}" "bindless cohort exact builder")
endforeach()
foreach(needle IN ITEMS
	"vk_ral_bindless_ledger_activate()"
	"Ral_GetBindGroupLayoutHandle( s_ral_bindless_layout )"
	"Ral_GetBindGroupHandle( s_ral_bindless_set )"
	"VK_BindlessCohortBuild( s_ral_backend")
	require_text("${TEXTURES}" "${needle}" "authoritative bindless cohort accessor")
endforeach()
foreach(needle IN ITEMS
	"MUTATE_RECEIPT(backend)"
	"MUTATE_RECEIPT(layout)"
	"MUTATE_RECEIPT(set)"
	"MUTATE_RECEIPT(rawLayout)"
	"MUTATE_RECEIPT(rawSet)"
	"mutatedLedger.setGeneration=UINT64_MAX"
	"ledger.setGeneration==2u"
	"memcmp(&snapshot,&receipt,sizeof(snapshot))==0")
	require_text("${COHORT_HOST}" "${needle}" "bindless cohort host mutation")
endforeach()
foreach(source IN LISTS RENDERER_VK_C)
	if(source STREQUAL "${ROOT}/code/renderervk/vk_ral_textures.c")
		continue()
	endif()
	file(READ "${source}" cohort_consumer)
	string(FIND "${cohort_consumer}" "vk_ral_bindless_get_cohort(" cohort_call)
	if(source STREQUAL "${ROOT}/code/renderervk/vk.c")
		string(REGEX MATCHALL "vk_ral_bindless_get_cohort[(]" cohort_calls
			"${cohort_consumer}")
		list(LENGTH cohort_calls cohort_count)
		if(NOT cohort_count EQUAL 1)
			message(FATAL_ERROR "bindless cohort product accessor inventory drifted")
		endif()
		continue()
	endif()
	if(NOT cohort_call EQUAL -1)
		message(FATAL_ERROR "bindless cohort escaped definition-only safe seam: ${source}")
	endif()
endforeach()

# Every RAL binding-0/binding-1 writer lives inside the publication wrappers.
string(REGEX MATCHALL "Ral_BindGroupSetTextureViewAt[(]" texture_view_writes "${TEXTURES}")
list(LENGTH texture_view_writes texture_view_count)
if(NOT texture_view_count EQUAL 1)
	message(FATAL_ERROR "TextureViewAt writer inventory drifted: ${texture_view_count}")
endif()
string(REGEX MATCHALL "Ral_BindGroupSetTextureViewsAt[(]" texture_views_writes "${TEXTURES}")
list(LENGTH texture_views_writes texture_views_count)
if(NOT texture_views_count EQUAL 1)
	message(FATAL_ERROR "TextureViewsAt writer inventory drifted: ${texture_views_count}")
endif()
string(REGEX MATCHALL "Ral_BindGroupSetTextureAt[(]" texture_writes "${TEXTURES}")
list(LENGTH texture_writes texture_count)
if(NOT texture_count EQUAL 2)
	message(FATAL_ERROR "TextureAt wrapper/tombstone inventory drifted: ${texture_count}")
endif()
string(REGEX MATCHALL "Ral_BindGroupSetSamplerAt[(]" sampler_writes "${TEXTURES}")
list(LENGTH sampler_writes sampler_count)
if(NOT sampler_count EQUAL 1)
	message(FATAL_ERROR "SamplerAt writer inventory drifted: ${sampler_count}")
endif()
string(REGEX MATCHALL "Ral_BindGroupSetSamplerAt[(]" vk_sampler_writes "${VK}")
list(LENGTH vk_sampler_writes vk_sampler_count)
if(NOT vk_sampler_count EQUAL 0)
	message(FATAL_ERROR "vk.c bypasses the sampler publication wrapper")
endif()
string(REGEX MATCHALL "Ral_BindGroupSetTextureAt[(]" vk_texture_writes "${VK}")
list(LENGTH vk_texture_writes vk_texture_count)
if(NOT vk_texture_count EQUAL 1)
	message(FATAL_ERROR "vk.c reserved TextureAt inventory drifted: ${vk_texture_count}")
endif()
string(FIND "${VK}" "Ral_BindGroupSetTextureAt( ralSet, WIRED_BINDLESS_SCENEDEPTH_SLOT" depth_write_pos)
string(FIND "${VK}" "vk_ral_bindless_record_reserved( WIRED_BINDLESS_SCENEDEPTH_SLOT" depth_record_pos)
if(depth_write_pos EQUAL -1 OR depth_record_pos EQUAL -1 OR NOT depth_write_pos LESS depth_record_pos)
	message(FATAL_ERROR "scene-depth write must precede its adjacent RESERVED receipt")
endif()
require_text("${VK}" "vk_ral_bindless_tombstone(\n\t\t\t\t\tWIRED_BINDLESS_SCENEDEPTH_SLOT )"
	"scene-depth falling-edge tombstone")

# Binding 2 is a separate 2D-array cohort and must never enter binding-0 ledger.
string(REGEX MATCHALL "imgWrite\.dstBinding[ \t]*=[ \t]*WIRED_BINDLESS_BIND_ARRAY_IMAGES" array_writes "${VK}")
list(LENGTH array_writes array_write_count)
if(NOT array_write_count EQUAL 1)
	message(FATAL_ERROR "binding-2 2D-array writer allowlist drifted")
endif()
string(FIND "${VK}" "void vk_ral_register_image_array( image_t *image )" array_begin)
string(FIND "${VK}" "static void vk_ral_register_screenmap_view" array_end)
if(array_begin EQUAL -1 OR array_end EQUAL -1 OR NOT array_begin LESS array_end)
	message(FATAL_ERROR "binding-2 writer function boundary missing")
endif()
math(EXPR array_len "${array_end}-${array_begin}")
string(SUBSTRING "${VK}" ${array_begin} ${array_len} ARRAY_FN)
string(REGEX MATCHALL "imgWrite\.dstBinding[ \t]*=[ \t]*WIRED_BINDLESS_BIND_ARRAY_IMAGES" array_fn_writes "${ARRAY_FN}")
list(LENGTH array_fn_writes array_fn_write_count)
if(NOT array_fn_write_count EQUAL 1)
	message(FATAL_ERROR "binding-2 writer escaped vk_ral_register_image_array")
endif()
string(FIND "${ARRAY_FN}" "vk_ral_bindless_record_" array_ledger_pos)
if(NOT array_ledger_pos EQUAL -1)
	message(FATAL_ERROR "binding-2 writer must not enter the binding-0 ledger")
endif()
string(REGEX MATCHALL "imgWrite\.dstBinding[ \t]*=[ \t]*WIRED_BINDLESS_BIND_IMAGES" raw_image_writes "${VK}")
list(LENGTH raw_image_writes raw_image_write_count)
if(NOT raw_image_write_count EQUAL 4)
	message(FATAL_ERROR "raw binding-0 writer inventory drifted: ${raw_image_write_count}")
endif()
foreach(needle IN ITEMS
	"vk_ral_bindless_record_reserved( WIRED_BINDLESS_SCREENMAP_SLOT"
	"vk_ral_bindless_record_reserved( WIRED_BINDLESS_SCENEDEPTH_SLOT"
	"vk_ral_bindless_record_raw_image( tr.blackImage"
	"vk_ral_bindless_record_raw_image( tr.whiteImage"
	"vk_ral_bindless_record_legacy_exact( image")
	require_text("${VK}" "${needle}" "raw binding-0 publication join")
endforeach()

# Ordinary authoring is legacy combined descriptor -> exact sampler -> raw
# binding-0 view -> LEGACY_EXACT receipt, all in one function.
string(FIND "${VK}" "void vk_update_descriptor_set( image_t *image, qboolean mipmap )" ordinary_begin)
string(FIND "${VK}" "void vk_destroy_image_resources" ordinary_end)
if(ordinary_begin EQUAL -1 OR ordinary_end EQUAL -1 OR NOT ordinary_begin LESS ordinary_end)
	message(FATAL_ERROR "ordinary descriptor function boundary missing")
endif()
math(EXPR ordinary_len "${ordinary_end}-${ordinary_begin}")
string(SUBSTRING "${VK}" ${ordinary_begin} ${ordinary_len} ORDINARY)
string(FIND "${ORDINARY}" "qvkUpdateDescriptorSets( vk.device, 1, &descriptor_write" combined_pos)
string(FIND "${ORDINARY}" "vk_ral_bindless_publish_sampler" sampler_pos)
string(FIND "${ORDINARY}" "qvkUpdateDescriptorSets( vk.device, 1, &imgWrite" image_pos)
string(FIND "${ORDINARY}" "vk_ral_bindless_record_legacy_exact" receipt_pos)
if(combined_pos EQUAL -1 OR sampler_pos EQUAL -1 OR image_pos EQUAL -1 OR receipt_pos EQUAL -1
		OR NOT combined_pos LESS sampler_pos OR NOT sampler_pos LESS image_pos OR NOT image_pos LESS receipt_pos)
	message(FATAL_ERROR "ordinary combined/sampler/image/receipt order drifted")
endif()

string(FIND "${VK}" "void vk_destroy_samplers( void )" sampler_destroy_begin)
string(SUBSTRING "${VK}" ${sampler_destroy_begin} 700 SAMPLER_DESTROY)
string(FIND "${SAMPLER_DESTROY}" "vk_ral_bindless_sampler_pool_invalidate();" pool_invalidate_pos)
string(FIND "${SAMPLER_DESTROY}" "Ral_DestroySampler( vk.samplers.ral_handle[i] );" owner_destroy_pos)
string(FIND "${SAMPLER_DESTROY}" "vk.samplers.ral_handle[i] = NULL;" owner_clear_pos)
if(pool_invalidate_pos EQUAL -1 OR owner_destroy_pos EQUAL -1 OR owner_clear_pos EQUAL -1
		OR NOT pool_invalidate_pos LESS owner_destroy_pos
		OR NOT owner_destroy_pos LESS owner_clear_pos)
	message(FATAL_ERROR "sampler ledger must invalidate before RAL-owner destruction and publication clear")
endif()
require_text("${TEXTURES}"
	"VK_BindlessPublicationInvalidateSet(\n\t\t\t\t&s_bindless_publication, s_ral_bindless_set );\n\t\tRal_DestroyBindGroup( s_ral_bindless_set )"
	"set invalidation before bind-group destruction")

message(STATUS "vk bindless publication source policy PASS")
