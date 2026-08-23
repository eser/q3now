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
file(READ "${ROOT}/code/renderer/ral/ral_resource.h" RAL_HEADER)
file(READ "${ROOT}/code/renderer/ral_vulkan/ral_vulkan_resource.c" RAL_VULKAN)
file(READ "${ROOT}/tests/ral_vulkan_bind_group_arena_test.c" RAL_HOST)
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
			Ral_BindGroupSetTextureViewAtBinding Ral_BindGroupSetTextureViewsAt
			Ral_BindGroupSetSamplerAt)
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

# Shipping descriptor publication is RAL-only. Native descriptor writes belong
# to the backend implementation, never to renderervk product code.
foreach(source IN LISTS RENDERER_VK_C)
	file(READ "${source}" product_source)
	string(FIND "${product_source}" "qvkUpdateDescriptorSets(" raw_update)
	if(NOT raw_update EQUAL -1)
		message(FATAL_ERROR "raw descriptor update escaped RAL backend: ${source}")
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
foreach(needle IN ITEMS
	"if ( !Ral_BindGroupSetTextureViewAt( s_ral_bindless_set, slot, view ) )"
	"if ( !Ral_BindGroupSetTextureViewsAt( s_ral_bindless_set, slots, views, count ) )"
	"if ( !Ral_BindGroupSetTextureAt( s_ral_bindless_set, slot, texture ) )"
	"if ( !Ral_BindGroupSetSamplerAt( s_ral_bindless_set, slot, sampler ) )"
	"if ( !Ral_BindGroupSetTextureAt( s_ral_bindless_set, slot, NULL ) )")
	require_text("${TEXTURES}" "${needle}" "write-before-ledger failure gate")
endforeach()
foreach(needle IN ITEMS
	"int Ral_BindGroupSetTextureAt("
	"int Ral_BindGroupSetTextureViewAt("
	"int Ral_BindGroupSetSamplerAt(")
	require_text("${RAL_HEADER}${RAL_VULKAN}" "${needle}" "observable sparse-write result")
endforeach()
foreach(needle IN ITEMS
	"if ( slot >= capacity ) return 0;"
	"tex->backend != ( g ? g->backend : NULL )"
	"view->backend != ( g ? g->backend : NULL )"
	"s->backend != b || s->sampler == VK_NULL_HANDLE"
	"!Ral_BindGroupSetTextureViewAt( &imageGroup, 4096u, NULL )"
	"!Ral_BindGroupSetTextureAt( &imageGroup, 4096u, NULL )"
	"!Ral_BindGroupSetSamplerAt( &imageGroup, 32u, NULL )")
	require_text("${RAL_VULKAN}${RAL_HOST}" "${needle}" "sparse-write mutation gate")
endforeach()
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
string(FIND "${VK}" "void vk_ral_register_image_array( image_t *image )" array_begin)
string(FIND "${VK}" "static void vk_ral_register_screenmap_view" array_end)
if(array_begin EQUAL -1 OR array_end EQUAL -1 OR NOT array_begin LESS array_end)
	message(FATAL_ERROR "binding-2 writer function boundary missing")
endif()
math(EXPR array_len "${array_end}-${array_begin}")
string(SUBSTRING "${VK}" ${array_begin} ${array_len} ARRAY_FN)
require_text("${ARRAY_FN}"
	"Ral_BindGroupSetTextureViewAtBinding( ralSet,\n\t\t\tWIRED_BINDLESS_BIND_ARRAY_IMAGES"
	"binding-2 RAL publication")
string(FIND "${ARRAY_FN}" "vk_ral_bindless_record_" array_ledger_pos)
if(NOT array_ledger_pos EQUAL -1)
	message(FATAL_ERROR "binding-2 writer must not enter the binding-0 ledger")
endif()
foreach(needle IN ITEMS
	"vk_ral_bindless_record_reserved( WIRED_BINDLESS_SCREENMAP_SLOT"
	"vk_ral_bindless_record_reserved( WIRED_BINDLESS_SCENEDEPTH_SLOT"
	"vk_ral_bindless_record_raw_image( tr.blackImage"
	"vk_ral_bindless_record_raw_image( tr.whiteImage")
	require_text("${VK}" "${needle}" "reserved binding-0 publication join")
endforeach()

# Ordinary authoring is direct per-image group -> exact sampler -> exact RAL
# binding-0 view publication, all in one function.
string(FIND "${VK}" "void vk_update_descriptor_set( image_t *image, qboolean mipmap )" ordinary_begin)
string(FIND "${VK}" "void vk_destroy_image_resources" ordinary_end)
if(ordinary_begin EQUAL -1 OR ordinary_end EQUAL -1 OR NOT ordinary_begin LESS ordinary_end)
	message(FATAL_ERROR "ordinary descriptor function boundary missing")
endif()
math(EXPR ordinary_len "${ordinary_end}-${ordinary_begin}")
string(SUBSTRING "${VK}" ${ordinary_begin} ${ordinary_len} ORDINARY)
string(FIND "${ORDINARY}" "vk_ral_refresh_image_descriptor( image, nativeSampler )" combined_pos)
string(FIND "${ORDINARY}" "vk_ral_bindless_publish_sampler" sampler_pos)
string(FIND "${ORDINARY}" "vk_ral_bindless_publish_texture_view( image" image_pos)
string(FIND "${ORDINARY}" "VK_BINDLESS_PUBLICATION_LEGACY_EXACT" receipt_pos)
if(combined_pos EQUAL -1 OR sampler_pos EQUAL -1 OR image_pos EQUAL -1 OR receipt_pos EQUAL -1
		OR NOT combined_pos LESS sampler_pos OR NOT sampler_pos LESS image_pos OR NOT image_pos LESS receipt_pos)
	message(FATAL_ERROR "ordinary combined/sampler/image/receipt order drifted")
endif()
foreach(needle IN ITEMS
	"image->ralDescriptorView"
	"Ral_GetTextureViewHandle( image->ralDescriptorView )"
	"== (void *)image->view")
	require_text("${ORDINARY}" "${needle}" "ordinary exact native-view join")
endforeach()
foreach(retired IN ITEMS qvkUpdateDescriptorSets VkWriteDescriptorSet VkDescriptorImageInfo
	vk_ral_bindless_record_legacy_exact)
	string(FIND "${ORDINARY}" "${retired}" retired_pos)
	if(NOT retired_pos EQUAL -1)
		message(FATAL_ERROR "ordinary publication regained raw/legacy authority: ${retired}")
	endif()
endforeach()
foreach(needle IN ITEMS
	"kind == VK_BINDLESS_PUBLICATION_LEGACY_EXACT"
	"!image->descriptor || image->view == VK_NULL_HANDLE"
	"image->ralDescriptorView != view"
	"nativeView != (const void *)image->view")
	require_text("${TEXTURES}" "${needle}" "LEGACY_EXACT view identity gate")
endforeach()

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
