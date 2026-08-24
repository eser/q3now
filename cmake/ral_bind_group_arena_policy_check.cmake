# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2024-present Wired Engine contributors

if(NOT DEFINED ROOT)
	message(FATAL_ERROR "ROOT is required")
endif()

file(READ "${ROOT}/code/render/ral/core/ral_bind_group_arena.h" HEADER)
file(READ "${ROOT}/code/render/ral/core/ral_bind_group_arena.c" CORE)
file(READ "${ROOT}/code/render/ral/core/ral_resource.h" RESOURCE_HEADER)
file(READ "${ROOT}/code/render/ral/backends/vulkan/ral_vulkan_resource.c" VULKAN)
file(READ "${ROOT}/code/render/ral/backends/vulkan/ral_vulkan_bridge.h" BRIDGE)
file(READ "${ROOT}/code/render/ral/backends/vulkan/renderer/vk.c" PRODUCT)
file(READ "${ROOT}/code/render/ral/backends/vulkan/renderer/vk.h" PRODUCT_HEADER)
file(READ "${ROOT}/code/render/ral/backends/vulkan/renderer/vk_ral_textures.c" PRODUCT_BINDINGS)
file(READ "${ROOT}/code/render/ral/backends/vulkan/renderer/tr_local.h" PRODUCT_LOCAL)
file(READ "${ROOT}/code/render/ral/backends/vulkan/renderer/tr_image.c" PRODUCT_IMAGE)
file(READ "${ROOT}/code/render/ral/backends/vulkan/renderer/shaders/ribbon.frag" RIBBON_FRAGMENT)
file(READ "${ROOT}/code/render/ral/backends/vulkan/renderer/shaders/beam.frag" BEAM_FRAGMENT)
file(READ "${ROOT}/code/render/ral/backends/vulkan/renderer/shaders/portable/ribbon_frag_spv.wgsl" RIBBON_WGSL)
file(READ "${ROOT}/code/render/ral/backends/vulkan/renderer/shaders/portable/beam_frag_spv.wgsl" BEAM_WGSL)
file(READ "${ROOT}/code/render/ral/backends/vulkan/renderer/shaders/portable/ribbon_frag_spv.msl" RIBBON_MSL)
file(READ "${ROOT}/code/render/ral/backends/vulkan/renderer/shaders/portable/beam_frag_spv.msl" BEAM_MSL)
file(READ "${ROOT}/tests/ral_bind_group_arena_lifecycle_test.c" PORTABLE_HOST)
file(READ "${ROOT}/tests/ral_vulkan_bind_group_arena_test.c" VULKAN_HOST)

function(require_text HAYSTACK NEEDLE LABEL)
	string(FIND "${${HAYSTACK}}" "${NEEDLE}" POS)
	if(POS EQUAL -1)
		message(FATAL_ERROR "missing ${LABEL}: ${NEEDLE}")
	endif()
endfunction()

function(require_count HAYSTACK REGEX EXPECTED LABEL)
	string(REGEX MATCHALL "${REGEX}" HITS "${${HAYSTACK}}")
	list(LENGTH HITS COUNT)
	if(NOT COUNT EQUAL EXPECTED)
		message(FATAL_ERROR "${LABEL}: expected ${EXPECTED}, found ${COUNT}")
	endif()
endfunction()

foreach(NATIVE IN ITEMS VkDescriptorPool VkDevice WGPUDevice WGPUBindGroup)
	string(FIND "${HEADER}${CORE}${RESOURCE_HEADER}" "${NATIVE}" POS)
	if(NOT POS EQUAL -1)
		message(FATAL_ERROR "native token escaped portable bind-group arena: ${NATIVE}")
	endif()
endforeach()

foreach(NEEDLE IN ITEMS
	"const ralBackend_t *backendIdentity;"
	"const ralBindGroupArena_t *arenaIdentity;"
	"uint64_t generation;"
	"uint32_t maxGroups;"
	"Ral_BindGroupArenaLifecyclePublishCreate"
	"Ral_BindGroupArenaLifecyclePublishReset")
	require_text(HEADER "${NEEDLE}" "portable arena lifecycle")
endforeach()

# Per-image combined sampler descriptors are arena-native RAL children. The
# VkDescriptorSet on image_t is a compatibility mirror, never an independent
# allocation/update/adoption authority.
string(FIND "${PRODUCT_BINDINGS}" "qboolean vk_ral_refresh_image_descriptor" IMAGE_OWNER_BEGIN)
string(FIND "${PRODUCT_BINDINGS}" "void vk_ral_release_image_descriptor" IMAGE_OWNER_END)
if(IMAGE_OWNER_BEGIN EQUAL -1 OR IMAGE_OWNER_END EQUAL -1
		OR IMAGE_OWNER_END LESS IMAGE_OWNER_BEGIN)
	message(FATAL_ERROR "cannot isolate per-image direct RAL descriptor owner")
endif()
math(EXPR IMAGE_OWNER_LEN "${IMAGE_OWNER_END} - ${IMAGE_OWNER_BEGIN}")
string(SUBSTRING "${PRODUCT_BINDINGS}" ${IMAGE_OWNER_BEGIN} ${IMAGE_OWNER_LEN} IMAGE_OWNER)
foreach(NEEDLE IN ITEMS
	"Ral_BindGroupArenaReceiptValid("
	"Ral_TextureGetResourceReceipt( image->ral, &textureReceipt )"
	"|| textureReceipt.imported"
	"|| textureReceipt.type != textureInfo.type"
	"|| textureReceipt.format != textureInfo.format"
	"|| textureReceipt.usage != textureInfo.usage"
	"viewInfo.texture = image->ral"
	"viewCandidate = Ral_CreateTextureView( s_ral_backend, &viewInfo )"
	"value.type = RAL_BIND_COMBINED_TEXTURE_SAMPLER"
	"value.sampler = sampler"
	"createInfo.arena = vk.ral_descriptor_arena"
	"createInfo.arenaReceipt = &vk.ral_descriptor_arena_receipt"
	"groupCandidate = Ral_CreateBindGroup( s_ral_backend, &createInfo )"
	"image->ralDescriptor = groupCandidate"
	"image->descriptor = rawCandidate"
	"groupCandidateOwned = qfalse"
	"viewCandidateOwned = qfalse"
	"if ( groupCandidate && groupCandidateOwned )"
	"if ( viewCandidate && viewCandidateOwned )"
	"if ( groupRetired ) Ral_DestroyBindGroup( groupRetired )"
	"if ( viewRetired ) Ral_DestroyTextureView( viewRetired )")
	require_text(IMAGE_OWNER "${NEEDLE}" "per-image direct RAL owner")
endforeach()
require_count(IMAGE_OWNER "Ral_CreateBindGroup[(]" 1
	"per-image bind-group create inventory")
require_count(IMAGE_OWNER "Ral_CreateTextureView[(]" 1
	"per-image direct texture-view inventory")
require_count(IMAGE_OWNER "Ral_TextureGetResourceReceipt[(]" 1
	"per-image texture receipt inventory")
foreach(RETIRED IN ITEMS Ral_AdoptBindGroup Ral_AdoptTextureResourceExact
	Ral_AdoptTextureViewExact qvkAllocateDescriptorSets qvkUpdateDescriptorSets
	VkWriteDescriptorSet)
	string(FIND "${IMAGE_OWNER}" "${RETIRED}" POS)
	if(NOT POS EQUAL -1)
		message(FATAL_ERROR "per-image direct owner regained retired authority: ${RETIRED}")
	endif()
endforeach()
foreach(NEEDLE IN ITEMS
	"struct ralBindGroup_s *ralDescriptor"
	"struct ralTextureView_s *ralDescriptorView"
	"struct ralSampler_s *ralDescriptorSampler"
	"struct ralTexture_s *ral;")
	require_text(PRODUCT_LOCAL "${NEEDLE}" "per-image ownership cohort")
endforeach()

# Bindless tables may expose more than one sampled-texture binding (2D at 0,
# 2D-array at 2). Pin the portable binding-aware publication surface and the
# three product call sites (array image, screenmap and shared scene depth).
foreach(NEEDLE IN ITEMS
	"int Ral_BindGroupSetTextureViewAtBinding( ralBindGroup_t *g, uint32_t binding,"
	"g->layout->entries[i].binding == binding"
	"entry->vkType != VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE"
	"slot >= entry->effectiveCount"
	"write.dstBinding = binding;"
	"write.dstArrayElement = slot;")
	require_text(VULKAN "${NEEDLE}" "binding-aware sampled-texture publication")
endforeach()
require_text(RESOURCE_HEADER
	"int Ral_BindGroupSetTextureViewAtBinding( ralBindGroup_t *g, uint32_t binding,"
	"portable binding-aware sampled-texture API")
foreach(NEEDLE IN ITEMS
	"Ral_BindGroupSetTextureViewAtBinding( &imageGroup, 2u, 17u, &imageView )"
	"capturedWriteBinding == 2u"
	"capturedWriteElement == 17u"
	"Ral_BindGroupSetTextureViewAtBinding( &imageGroup, 2u, 256u, &imageView )")
	require_text(VULKAN_HOST "${NEEDLE}" "binding-aware mutation host")
endforeach()

# A portable texture-array value may also target a combined-image-sampler
# layout entry. One shared sampler is expanded across every image element;
# invalid/missing sampler and capacity cases reject before allocation.
foreach(NEEDLE IN ITEMS
	"entry->vkType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER"
	"!val->sampler || val->sampler->backend != b"
	"val->textureArrayCount > RAL_VK_MAX_BG_IMAGES - imageValueCount"
	"imgs[ni].sampler = val->sampler->sampler")
	require_text(VULKAN "${NEEDLE}" "combined texture-array binding")
endforeach()
foreach(NEEDLE IN ITEMS
	"arrayLayout.entries[0].vkType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER"
	"value.textureArrayCount = 3u"
	"value.sampler = &arraySampler"
	"capturedDescriptorCount == 3u"
	"capturedFirstSampler == arraySampler.sampler"
	"capturedLastSampler == arraySampler.sampler"
	"value.sampler = NULL"
	"Ral_CreateBindGroup( &backend, &groupInfo ) == NULL")
	require_text(VULKAN_HOST "${NEEDLE}" "combined texture-array host")
endforeach()
require_count(PRODUCT "Ral_BindGroupSetTextureViewAtBinding[(]" 3
	"product binding-aware bindless publication inventory")

string(FIND "${PRODUCT}" "void vk_ral_register_image_array" BINDLESS_ARRAY_BEGIN)
string(FIND "${PRODUCT}" "static qboolean vk_ral_register_depthfade_view" BINDLESS_ARRAY_END)
if(BINDLESS_ARRAY_BEGIN EQUAL -1 OR BINDLESS_ARRAY_END EQUAL -1
		OR BINDLESS_ARRAY_END LESS BINDLESS_ARRAY_BEGIN)
	message(FATAL_ERROR "cannot isolate array/screenmap bindless publication")
endif()
math(EXPR BINDLESS_ARRAY_LEN "${BINDLESS_ARRAY_END} - ${BINDLESS_ARRAY_BEGIN}")
string(SUBSTRING "${PRODUCT}" ${BINDLESS_ARRAY_BEGIN}
	${BINDLESS_ARRAY_LEN} BINDLESS_ARRAY)
foreach(NEEDLE IN ITEMS
	"WIRED_BINDLESS_BIND_ARRAY_IMAGES, (uint32_t)slot,"
	"image->ralDescriptorView"
	"WIRED_BINDLESS_BIND_IMAGES, WIRED_BINDLESS_SCREENMAP_SLOT,"
	"vk.screenMap.ral_color_view")
	require_text(BINDLESS_ARRAY "${NEEDLE}" "array/screenmap RAL publication")
endforeach()
string(FIND "${BINDLESS_ARRAY}" "qvkUpdateDescriptorSets" POS)
if(NOT POS EQUAL -1)
	message(FATAL_ERROR "array/screenmap bindless publication regained raw descriptor write")
endif()

string(FIND "${PRODUCT}" "void vk_register_black_sentinel" BINDLESS_SENTINEL_BEGIN)
string(FIND "${PRODUCT}" "static ralSampler_t *vk_ral_sampler_for_definition( const Vk_Sampler_Def *definition ) {" BINDLESS_SENTINEL_END)
math(EXPR BINDLESS_SENTINEL_LEN "${BINDLESS_SENTINEL_END} - ${BINDLESS_SENTINEL_BEGIN}")
string(SUBSTRING "${PRODUCT}" ${BINDLESS_SENTINEL_BEGIN}
	${BINDLESS_SENTINEL_LEN} BINDLESS_SENTINELS)
foreach(NEEDLE IN ITEMS
	"vk_ral_bindless_publish_texture_view( tr.blackImage,"
	"WIRED_BINDLESS_BLACK_TEX_SENTINEL,"
	"tr.blackImage->ralDescriptorView"
	"vk_ral_bindless_publish_texture_view( tr.whiteImage,"
	"WIRED_BINDLESS_WHITE_TEX_SENTINEL,"
	"tr.whiteImage->ralDescriptorView")
	require_text(BINDLESS_SENTINELS "${NEEDLE}" "reserved sentinel RAL publication")
endforeach()
string(FIND "${BINDLESS_SENTINELS}" "qvkUpdateDescriptorSets" POS)
if(NOT POS EQUAL -1)
	message(FATAL_ERROR "reserved sentinel publication regained raw descriptor write")
endif()

string(FIND "${PRODUCT}" "void vk_update_descriptor_set" IMAGE_UPDATE_BEGIN)
string(FIND "${PRODUCT}"
	"static void set_shader_stage_desc(VkPipelineShaderStageCreateInfo *desc, VkShaderStageFlagBits stage, VkShaderModule shader_module, const char *entry) {"
	IMAGE_UPDATE_END)
if(IMAGE_UPDATE_BEGIN EQUAL -1 OR IMAGE_UPDATE_END EQUAL -1
		OR IMAGE_UPDATE_END LESS IMAGE_UPDATE_BEGIN)
	message(FATAL_ERROR "cannot isolate per-image descriptor publication")
endif()
math(EXPR IMAGE_UPDATE_LEN "${IMAGE_UPDATE_END} - ${IMAGE_UPDATE_BEGIN}")
string(SUBSTRING "${PRODUCT}" ${IMAGE_UPDATE_BEGIN} ${IMAGE_UPDATE_LEN} IMAGE_UPDATE)
foreach(NEEDLE IN ITEMS
	"nativeSampler = (void *)vk_find_sampler( &sampler_def )"
	"vk_ral_refresh_image_descriptor( image, nativeSampler )"
	"vk_ral_bindless_publish_sampler"
	"vk_ral_bindless_publish_texture_view( image"
	"VK_BINDLESS_PUBLICATION_LEGACY_EXACT")
	require_text(IMAGE_UPDATE "${NEEDLE}" "per-image/direct-bindless publication")
endforeach()
foreach(RETIRED IN ITEMS "descriptor_write.dstSet = image->descriptor"
	"qvkAllocateDescriptorSets" "vk_ral_adopt_image_descriptor")
	string(FIND "${IMAGE_UPDATE}" "${RETIRED}" POS)
	if(NOT POS EQUAL -1)
		message(FATAL_ERROR "per-image publication regained raw/adopt path: ${RETIRED}")
	endif()
endforeach()

string(FIND "${PRODUCT_IMAGE}" "void GL_TextureMode" IMAGE_FILTER_BEGIN)
string(FIND "${PRODUCT_IMAGE}" "int R_SumOfUsedImages" IMAGE_FILTER_END)
if(IMAGE_FILTER_BEGIN EQUAL -1 OR IMAGE_FILTER_END EQUAL -1
		OR IMAGE_FILTER_END LESS IMAGE_FILTER_BEGIN)
	message(FATAL_ERROR "cannot isolate texture filter reload")
endif()
math(EXPR IMAGE_FILTER_LEN "${IMAGE_FILTER_END} - ${IMAGE_FILTER_BEGIN}")
string(SUBSTRING "${PRODUCT_IMAGE}" ${IMAGE_FILTER_BEGIN} ${IMAGE_FILTER_LEN} IMAGE_FILTER)
string(FIND "${IMAGE_FILTER}" "vk_ral_release_image_descriptor( tr.images[i] )" IMAGE_FILTER_RELEASE)
string(FIND "${IMAGE_FILTER}" "vk_destroy_samplers();" IMAGE_FILTER_DESTROY)
string(FIND "${IMAGE_FILTER}" "vk_update_descriptor_set( img," IMAGE_FILTER_REBUILD)
if(IMAGE_FILTER_RELEASE EQUAL -1 OR IMAGE_FILTER_DESTROY EQUAL -1
		OR IMAGE_FILTER_REBUILD EQUAL -1
		OR NOT IMAGE_FILTER_RELEASE LESS IMAGE_FILTER_DESTROY
		OR NOT IMAGE_FILTER_DESTROY LESS IMAGE_FILTER_REBUILD)
	message(FATAL_ERROR "texture filter reload lost group -> sampler -> all-group order")
endif()

# The renderer-global set 2 cohort (shadow array, sparse IBL views and GTAO
# visibility) is also a direct arena child. The native set is a mirror; neither
# attachment refresh nor the boot adoption sweep may allocate, update or adopt
# an independent VkDescriptorSet.
string(FIND "${PRODUCT_BINDINGS}"
	"qboolean vk_ral_refresh_engine_resources_bindgroup" ENGINE_OWNER_BEGIN)
string(FIND "${PRODUCT_BINDINGS}"
	"void vk_ral_release_sprite_bindgroup" ENGINE_OWNER_END)
if(ENGINE_OWNER_BEGIN EQUAL -1 OR ENGINE_OWNER_END EQUAL -1
		OR ENGINE_OWNER_END LESS ENGINE_OWNER_BEGIN)
	message(FATAL_ERROR "cannot isolate direct engine-resources RAL owner")
endif()
math(EXPR ENGINE_OWNER_LEN "${ENGINE_OWNER_END} - ${ENGINE_OWNER_BEGIN}")
string(SUBSTRING "${PRODUCT_BINDINGS}" ${ENGINE_OWNER_BEGIN}
	${ENGINE_OWNER_LEN} ENGINE_OWNER)
foreach(NEEDLE IN ITEMS
	"Ral_BindGroupArenaReceiptValid("
	"ralBindingValue_t values[5];"
	"viewInfo.texture = vk.shadowMap.ral_image;"
	"shadowViewCandidate = Ral_CreateTextureView( s_ral_backend, &viewInfo );"
	"WIRED_ENGINE_RES_BIND_SHADOWMAP"
	"WIRED_ENGINE_RES_BIND_BRDF_LUT"
	"WIRED_ENGINE_RES_BIND_IRRADIANCE"
	"WIRED_ENGINE_RES_BIND_RADIANCE"
	"WIRED_ENGINE_RES_BIND_GTAO"
	"tr.whiteImage->ralDescriptorView"
	"createInfo.arena = vk.ral_descriptor_arena;"
	"createInfo.arenaReceipt = &vk.ral_descriptor_arena_receipt;"
	"groupCandidate = Ral_CreateBindGroup( s_ral_backend, &createInfo );"
	"vk.engineResources.ral_descriptor = groupCandidate;"
	"vk.engineResources.ral_shadow_view = shadowViewCandidate;"
	"vk.engineResources.descriptor = rawCandidate;"
	"if ( groupRetired ) Ral_DestroyBindGroup( groupRetired );"
	"if ( shadowViewRetired ) Ral_DestroyTextureView( shadowViewRetired );"
	"if ( groupCandidate && groupOwned ) Ral_DestroyBindGroup( groupCandidate );"
	"if ( shadowViewCandidate && shadowViewOwned )")
	require_text(ENGINE_OWNER "${NEEDLE}" "engine-resources direct RAL owner")
endforeach()
require_count(ENGINE_OWNER "Ral_CreateBindGroup[(]" 1
	"engine-resources bind-group create inventory")
require_count(ENGINE_OWNER "Ral_CreateTextureView[(]" 1
	"engine-resources direct shadow-view inventory")
foreach(RETIRED IN ITEMS Ral_AdoptBindGroup Ral_AdoptTextureViewExact
	qvkAllocateDescriptorSets qvkUpdateDescriptorSets VkWriteDescriptorSet
	VkDescriptorImageInfo)
	string(FIND "${ENGINE_OWNER}" "${RETIRED}" POS)
	if(NOT POS EQUAL -1)
		message(FATAL_ERROR
			"engine-resources owner regained raw/adopt authority: ${RETIRED}")
	endif()
endforeach()
foreach(NEEDLE IN ITEMS
	"VkDescriptorSet descriptor;"
	"struct ralBindGroup_s *ral_descriptor;"
	"struct ralTextureView_s *ral_shadow_view;")
	require_text(PRODUCT_HEADER "${NEEDLE}" "engine-resources owner inventory")
endforeach()

string(FIND "${PRODUCT}" "void vk_update_attachment_descriptors" ENGINE_UPDATE_BEGIN)
string(FIND "${PRODUCT}" "void vk_init_descriptors" ENGINE_UPDATE_END)
if(ENGINE_UPDATE_BEGIN EQUAL -1 OR ENGINE_UPDATE_END EQUAL -1
		OR ENGINE_UPDATE_END LESS ENGINE_UPDATE_BEGIN)
	message(FATAL_ERROR "cannot isolate attachment descriptor refresh")
endif()
math(EXPR ENGINE_UPDATE_LEN "${ENGINE_UPDATE_END} - ${ENGINE_UPDATE_BEGIN}")
string(SUBSTRING "${PRODUCT}" ${ENGINE_UPDATE_BEGIN}
	${ENGINE_UPDATE_LEN} ENGINE_UPDATE)
require_count(ENGINE_UPDATE "vk_ral_refresh_engine_resources_bindgroup[(]" 1
	"attachment engine-resources refresh inventory")
string(FIND "${ENGINE_UPDATE}" "vk.engineResources.descriptor" POS)
if(NOT POS EQUAL -1)
	message(FATAL_ERROR
		"attachment refresh regained raw engine-resources descriptor authority")
endif()

string(FIND "${PRODUCT}" "void vk_init_descriptors" ENGINE_INIT_BEGIN)
string(FIND "${PRODUCT}" "static void vk_release_geometry_buffers" ENGINE_INIT_END)
if(ENGINE_INIT_BEGIN EQUAL -1 OR ENGINE_INIT_END EQUAL -1
		OR ENGINE_INIT_END LESS ENGINE_INIT_BEGIN)
	message(FATAL_ERROR "cannot isolate descriptor initialization")
endif()
math(EXPR ENGINE_INIT_LEN "${ENGINE_INIT_END} - ${ENGINE_INIT_BEGIN}")
string(SUBSTRING "${PRODUCT}" ${ENGINE_INIT_BEGIN} ${ENGINE_INIT_LEN} ENGINE_INIT)
require_count(ENGINE_INIT "vk_ral_refresh_engine_resources_bindgroup[(]" 1
	"initial engine-resources refresh inventory")
foreach(RETIRED IN ITEMS "erAlloc" "&vk.engineResources.descriptor"
	"engine-resources set (shadowMap)")
	string(FIND "${ENGINE_INIT}" "${RETIRED}" POS)
	if(NOT POS EQUAL -1)
		message(FATAL_ERROR
			"descriptor initialization regained raw engine-resources allocation: ${RETIRED}")
	endif()
endforeach()

string(FIND "${PRODUCT_BINDINGS}" "void vk_ral_adopt_static_bindgroups" ENGINE_ADOPT_BEGIN)
string(FIND "${PRODUCT_BINDINGS}"
	"static qboolean vk_ral_image_texture_info( const image_t *image,"
	ENGINE_ADOPT_END)
if(ENGINE_ADOPT_BEGIN EQUAL -1 OR ENGINE_ADOPT_END EQUAL -1
		OR ENGINE_ADOPT_END LESS ENGINE_ADOPT_BEGIN)
	message(FATAL_ERROR "cannot isolate static bind-group compatibility sweep")
endif()
math(EXPR ENGINE_ADOPT_LEN "${ENGINE_ADOPT_END} - ${ENGINE_ADOPT_BEGIN}")
string(SUBSTRING "${PRODUCT_BINDINGS}" ${ENGINE_ADOPT_BEGIN}
	${ENGINE_ADOPT_LEN} ENGINE_ADOPT)
require_count(ENGINE_ADOPT "vk_ral_refresh_engine_resources_bindgroup[(]" 1
	"static-sweep direct engine-resources refresh inventory")
string(FIND "${ENGINE_ADOPT}" "RETAIN_ADOPT( vk.engineResources" POS)
if(NOT POS EQUAL -1)
	message(FATAL_ERROR "static sweep reintroduced engine-resources adoption")
endif()

string(FIND "${PRODUCT}" "static void vk_shadow_release_resources( void )\n{" SHADOW_RELEASE_BEGIN)
string(FIND "${PRODUCT}" "static void vk_dlight_shadow_release_resources( void )\n{" SHADOW_RELEASE_END)
math(EXPR SHADOW_RELEASE_LEN "${SHADOW_RELEASE_END} - ${SHADOW_RELEASE_BEGIN}")
string(SUBSTRING "${PRODUCT}" ${SHADOW_RELEASE_BEGIN}
	${SHADOW_RELEASE_LEN} SHADOW_RELEASE)
string(FIND "${SHADOW_RELEASE}"
	"vk_ral_release_engine_resources_bindgroup();" ENGINE_CHILD_RELEASE)
string(FIND "${SHADOW_RELEASE}"
	"Ral_DestroyTexture( vk.shadowMap.ral_image );" SHADOW_PARENT_RELEASE)
if(ENGINE_CHILD_RELEASE EQUAL -1 OR SHADOW_PARENT_RELEASE EQUAL -1
		OR NOT ENGINE_CHILD_RELEASE LESS SHADOW_PARENT_RELEASE)
	message(FATAL_ERROR "shadow teardown lost engine-group -> texture-parent order")
endif()
string(FIND "${PRODUCT_BINDINGS}"
	"void vk_ral_release_internal_texture_dependents( void )"
	INTERNAL_RELEASE_BEGIN)
string(FIND "${PRODUCT_BINDINGS}"
	"static void vk_ral_destroy_adopted_pipeline_layouts( void )"
	INTERNAL_RELEASE_END)
if(INTERNAL_RELEASE_BEGIN EQUAL -1 OR INTERNAL_RELEASE_END EQUAL -1
		OR INTERNAL_RELEASE_END LESS INTERNAL_RELEASE_BEGIN)
	message(FATAL_ERROR "cannot isolate internal texture dependent teardown")
endif()
math(EXPR INTERNAL_RELEASE_LEN "${INTERNAL_RELEASE_END} - ${INTERNAL_RELEASE_BEGIN}")
string(SUBSTRING "${PRODUCT_BINDINGS}" ${INTERNAL_RELEASE_BEGIN}
	${INTERNAL_RELEASE_LEN} INTERNAL_RELEASE)
string(FIND "${INTERNAL_RELEASE}"
	"vk_ral_release_engine_resources_bindgroup();" ENGINE_INTERNAL_RELEASE)
string(FIND "${INTERNAL_RELEASE}" "vk_brdf_lut_shutdown();" ENGINE_BRDF_PARENT)
string(FIND "${INTERNAL_RELEASE}" "vk_ibl_probes_shutdown();" ENGINE_IBL_PARENT)
string(FIND "${INTERNAL_RELEASE}" "vk_gtao_shutdown();" ENGINE_GTAO_PARENT)
if(ENGINE_INTERNAL_RELEASE EQUAL -1 OR ENGINE_BRDF_PARENT EQUAL -1
		OR ENGINE_IBL_PARENT EQUAL -1 OR ENGINE_GTAO_PARENT EQUAL -1
		OR NOT ENGINE_INTERNAL_RELEASE LESS ENGINE_BRDF_PARENT
		OR NOT ENGINE_INTERNAL_RELEASE LESS ENGINE_IBL_PARENT
		OR NOT ENGINE_INTERNAL_RELEASE LESS ENGINE_GTAO_PARENT)
	message(FATAL_ERROR "engine-resources group outlives an IBL/GTAO parent")
endif()
foreach(NEEDLE IN ITEMS
	"receipt->generation != 0u && receipt->generation != UINT64_MAX"
	"lifecycle->generation >= UINT64_MAX - 1u"
	"Ral_BindGroupArenaReceiptExact( &live, current )"
	"*lifecycle = candidate;"
	"*outNext = next;")
	require_text(CORE "${NEEDLE}" "output-atomic arena epoch")
endforeach()
foreach(NEEDLE IN ITEMS
	"RAL_MAX_BIND_GROUP_ARENA_ENTRIES 16u"
	"ralBindType_t type;"
	"qboolean dynamicOffset;"
	"ralBindGroupArena_t         *arena;"
	"const ralBindGroupArenaReceipt_t *arenaReceipt;"
	"Ral_CreateBindGroupArena("
	"Ral_ResetBindGroupArenaExact(")
	require_text(RESOURCE_HEADER "${NEEDLE}" "portable arena API")
endforeach()

foreach(NEEDLE IN ITEMS
	"entry->count == 0u"
	"entry->dynamicOffset != qfalse && entry->dynamicOffset != qtrue"
	"sizes[j].type == type"
	"VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC"
	"VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC"
	"backend->vk.CreateDescriptorPool"
	"VK_OBJECT_TYPE_DESCRIPTOR_POOL, createInfo->debugName"
	"arena->lifecycle.generation >= UINT64_MAX - 1u"
	"arena->backend->vk.ResetDescriptorPool"
	"result == VK_ERROR_DEVICE_LOST"
	"( ci->arena == NULL ) != ( ci->arenaReceipt == NULL )"
	"ci->arena->backend != b"
	"Ral_BindGroupArenaReceiptExact( &arenaLive, ci->arenaReceipt )"
	"dai.descriptorPool     = pool;"
	"if ( g->ownsSet && !g->arena )"
	"Ral_BindGroupArenaLifecyclePublishReset")
	require_text(VULKAN "${NEEDLE}" "Vulkan arena lowering")
endforeach()
require_text(BRIDGE "Ral_GetBindGroupArenaHandle" "bounded legacy native mirror")
file(READ "${ROOT}/code/render/ral/backends/vulkan/ral_vulkan_dynamic_bind.c" DYNAMIC_BIND)
require_text(DYNAMIC_BIND "!ralVk_BindGroupArenaLive( group )"
	"stale arena group command rejection")

foreach(RETIRED IN ITEMS qvkCreateDescriptorPool qvkResetDescriptorPool qvkDestroyDescriptorPool)
	string(FIND "${PRODUCT}${PRODUCT_HEADER}" "${RETIRED}" POS)
	if(NOT POS EQUAL -1)
		message(FATAL_ERROR "renderer retained raw descriptor-pool authority: ${RETIRED}")
	endif()
endforeach()
foreach(NEEDLE IN ITEMS
	"vk.ral_descriptor_arena = Ral_CreateBindGroupArena("
	"vk.ral_descriptor_arena_receipt = next;"
	"vk_ral_reset_descriptor_arena( \"vk_update_attachment_descriptors\" );"
	"vk_ral_reset_descriptor_arena( \"vk_release_resources\" );"
	"Ral_DestroyBindGroupArena( vk.ral_descriptor_arena );"
	"vk.descriptor_pool = (VkDescriptorPool)Ral_GetBindGroupArenaHandle(")
	require_text(PRODUCT "${NEEDLE}" "product arena ownership")
endforeach()
require_text(PRODUCT
	"+ 1 /* vk.blueNoise.descriptor — RAL-owned gamma dither LUT */"
	"blue-noise combined-sampler arena budget")

string(FIND "${PRODUCT_HEADER}" "// Blue-noise dither tile" BLUE_HEADER_BEGIN)
string(FIND "${PRODUCT_HEADER}" "// screenMap" BLUE_HEADER_END)
if(BLUE_HEADER_BEGIN EQUAL -1 OR BLUE_HEADER_END EQUAL -1
		OR BLUE_HEADER_END LESS BLUE_HEADER_BEGIN)
	message(FATAL_ERROR "cannot isolate blue-noise ownership inventory")
endif()
math(EXPR BLUE_HEADER_LEN "${BLUE_HEADER_END} - ${BLUE_HEADER_BEGIN}")
string(SUBSTRING "${PRODUCT_HEADER}" ${BLUE_HEADER_BEGIN} ${BLUE_HEADER_LEN} BLUE_HEADER)
foreach(NEEDLE IN ITEMS
	"struct ralTexture_s *ral_texture;"
	"struct ralTextureView_s *ral_view;"
	"struct ralSampler_s *ral_sampler;"
	"VkDescriptorSet descriptor;"
	"struct ralBindGroup_s *ral_descriptor;")
	require_text(BLUE_HEADER "${NEEDLE}" "blue-noise typed owner inventory")
endforeach()
foreach(RETIRED IN ITEMS "VkImage " "VkImageView " "VkDeviceMemory " "VkSampler ")
	string(FIND "${BLUE_HEADER}" "${RETIRED}" POS)
	if(NOT POS EQUAL -1)
		message(FATAL_ERROR "blue-noise inventory retained raw resource owner: ${RETIRED}")
	endif()
endforeach()

string(FIND "${PRODUCT}" "// Create + upload the baked blue-noise dither tile" BLUE_BEGIN)
string(FIND "${PRODUCT}" "Inverse of vk_smaa_alloc_resources" BLUE_END)
if(BLUE_BEGIN EQUAL -1 OR BLUE_END EQUAL -1 OR BLUE_END LESS BLUE_BEGIN)
	message(FATAL_ERROR "cannot isolate blue-noise RAL-owned cohort")
endif()
math(EXPR BLUE_LEN "${BLUE_END} - ${BLUE_BEGIN}")
string(SUBSTRING "${PRODUCT}" ${BLUE_BEGIN} ${BLUE_LEN} BLUE)
foreach(NEEDLE IN ITEMS
	"candidateTexture = Ral_CreateTexture( backend, &textureInfo );"
	"textureInfo.format = RAL_FORMAT_R8_UNORM;"
	"textureInfo.usage = RAL_TEXTURE_USAGE_SAMPLED"
	"| RAL_TEXTURE_USAGE_TRANSFER_DST;"
	"uploadInfo.data = blue_noise_tex;"
	"uploadInfo.dataSize = texSize;"
	"upload = Ral_TextureUploadAsync( candidateTexture, &uploadInfo );"
	"candidateView = Ral_CreateTextureView( backend, &viewInfo );"
	"candidateSampler = Ral_CreateSampler( backend, &samplerInfo );"
	"value.type = RAL_BIND_COMBINED_TEXTURE_SAMPLER;"
	"groupInfo.arena = vk.ral_descriptor_arena;"
	"groupInfo.arenaReceipt = &vk.ral_descriptor_arena_receipt;"
	"candidateGroup = Ral_CreateBindGroup( backend, &groupInfo );"
	"vk.blueNoise.descriptor = (VkDescriptorSet)Ral_GetBindGroupHandle( candidateGroup );"
	"if ( candidateSampler ) Ral_DestroySampler( candidateSampler );"
	"if ( candidateView ) Ral_DestroyTextureView( candidateView );"
	"if ( candidateTexture ) Ral_DestroyTexture( candidateTexture );")
	require_text(BLUE "${NEEDLE}" "blue-noise RAL-owned cohort")
endforeach()
foreach(RETIRED IN ITEMS
	qvkAllocateDescriptorSets qvkUpdateDescriptorSets Ral_AdoptBindGroup
	qvkCreateBuffer qvkAllocateMemory qvkCmdCopyBufferToImage
	record_image_layout_transition vk_smaa_create_image_dedicated)
	string(FIND "${BLUE}" "${RETIRED}" POS)
	if(NOT POS EQUAL -1)
		message(FATAL_ERROR "blue-noise cohort retained raw/adopted authority: ${RETIRED}")
	endif()
endforeach()
foreach(NEEDLE IN ITEMS
	"DESTROY_RETAINED_BG( vk.blueNoise.ral_descriptor );"
	"vk.blueNoise.descriptor = VK_NULL_HANDLE;")
	require_text(PRODUCT_BINDINGS "${NEEDLE}" "blue-noise arena reset teardown")
endforeach()

string(FIND "${PRODUCT}" "// Resolution-independent blue-noise resources" BLUE_RELEASE_BEGIN)
string(FIND "${PRODUCT}" "__cleanup:" BLUE_RELEASE_END)
if(BLUE_RELEASE_BEGIN EQUAL -1 OR BLUE_RELEASE_END EQUAL -1
		OR BLUE_RELEASE_END LESS BLUE_RELEASE_BEGIN)
	message(FATAL_ERROR "cannot isolate blue-noise backend-child teardown")
endif()
math(EXPR BLUE_RELEASE_LEN "${BLUE_RELEASE_END} - ${BLUE_RELEASE_BEGIN}")
string(SUBSTRING "${PRODUCT}" ${BLUE_RELEASE_BEGIN} ${BLUE_RELEASE_LEN} BLUE_RELEASE)
foreach(NEEDLE IN ITEMS
	"Ral_DestroyBindGroup( vk.blueNoise.ral_descriptor );"
	"Ral_DestroyTextureView( vk.blueNoise.ral_view );"
	"Ral_DestroySampler( vk.blueNoise.ral_sampler );"
	"Ral_DestroyTexture( vk.blueNoise.ral_texture );")
	require_text(BLUE_RELEASE "${NEEDLE}" "blue-noise child-before-backend teardown")
endforeach()

# Mutable attachment sampling is no longer allocated/written through raw Vulkan
# descriptors. Adopted textures mint portable views, then create exact arena-
# owned combined sampler groups; raw VkDescriptorSet fields are mirrors.
foreach(NEEDLE IN ITEMS
	"struct ralTextureView_s *ral_color_view;"
	"struct ralTextureView_s *ral_tonemapped_view;"
	"struct ralTextureView_s *ral_bloom_image_view[1+VK_NUM_BLOOM_PASSES];"
	"struct ralTextureView_s *ral_view;")
	require_text(PRODUCT_HEADER "${NEEDLE}" "attachment portable view inventory")
endforeach()
foreach(NEEDLE IN ITEMS
	"candidateView = Ral_CreateTextureView( backend, &viewInfo );"
	"|| Ral_GetSamplerHandle( sampler ) == NULL"
	"value.type = RAL_BIND_COMBINED_TEXTURE_SAMPLER;"
	"groupInfo.arena = vk.ral_descriptor_arena;"
	"groupInfo.arenaReceipt = &vk.ral_descriptor_arena_receipt;"
	"candidateGroup = Ral_CreateBindGroup( backend, &groupInfo );"
	"*publishedView = candidateView;"
	"*publishedGroup = candidateGroup;"
	"vk.color_descriptor = Ral_GetBindGroupHandle( vk.ral_color_descriptor );"
	"vk.tonemapped_descriptor = Ral_GetBindGroupHandle("
	"vk.screenMap.color_descriptor = Ral_GetBindGroupHandle("
	"vk.bloom_image_descriptor[i] = Ral_GetBindGroupHandle("
	"vk.sceneDepth.descriptor = Ral_GetBindGroupHandle("
	"vk_ral_refresh_internal_texture_dependents();")
	require_text(PRODUCT "${NEEDLE}" "attachment RAL sampler cohort")
endforeach()
foreach(NEEDLE IN ITEMS
	"vk_ral_release_attachment_sampler_cohorts();"
	"vk_update_attachment_descriptors();")
	require_text(PRODUCT_BINDINGS "${NEEDLE}" "attachment child-before-parent lifecycle")
endforeach()
string(FIND "${PRODUCT}"
	"static ralTexture_t *vk_create_attachment_texture( uint32_t width,"
	ATTACH_CREATE_BEGIN)
string(FIND "${PRODUCT}"
	"static ralTexture_t *vk_create_depth_attachment_texture( uint32_t width,"
	ATTACH_CREATE_END)
if(ATTACH_CREATE_BEGIN EQUAL -1 OR ATTACH_CREATE_END EQUAL -1
		OR ATTACH_CREATE_END LESS ATTACH_CREATE_BEGIN)
	message(FATAL_ERROR "cannot isolate direct attachment creation")
endif()
math(EXPR ATTACH_CREATE_LEN
	"${ATTACH_CREATE_END} - ${ATTACH_CREATE_BEGIN}")
string(SUBSTRING "${PRODUCT}" ${ATTACH_CREATE_BEGIN}
	${ATTACH_CREATE_LEN} ATTACH_CREATE)
foreach(NEEDLE IN ITEMS
	"createInfo.type = RAL_TEXTURE_2D;"
	"createInfo.usage = usage;"
	"createInfo.memory = RAL_MEMORY_DEVICE_LOCAL;"
	"texture = Ral_CreateTexture( vk_ral_get_backend(), &createInfo );")
	require_text(ATTACH_CREATE "${NEEDLE}" "direct attachment texture owner")
endforeach()
string(FIND "${PRODUCT}" "static void vk_destroy_attachments( void )\n{" RAW_ATTACHMENT_DESTROY_BEGIN)
string(FIND "${PRODUCT}" "static void vk_destroy_pipelines( qboolean resetCounter )\n{" RAW_ATTACHMENT_DESTROY_END)
if(RAW_ATTACHMENT_DESTROY_BEGIN EQUAL -1 OR RAW_ATTACHMENT_DESTROY_END EQUAL -1
		OR RAW_ATTACHMENT_DESTROY_END LESS RAW_ATTACHMENT_DESTROY_BEGIN)
	message(FATAL_ERROR "cannot isolate direct attachment teardown")
endif()
math(EXPR RAW_ATTACHMENT_DESTROY_LEN
	"${RAW_ATTACHMENT_DESTROY_END} - ${RAW_ATTACHMENT_DESTROY_BEGIN}")
string(SUBSTRING "${PRODUCT}" ${RAW_ATTACHMENT_DESTROY_BEGIN}
	${RAW_ATTACHMENT_DESTROY_LEN} RAW_ATTACHMENT_DESTROY)
string(FIND "${RAW_ATTACHMENT_DESTROY}"
	"vk_ral_release_internal_texture_dependents();" RAW_CHILD_RELEASE)
string(FIND "${RAW_ATTACHMENT_DESTROY}"
	"Ral_DestroyTexture( vk.ral_color_image );" RAW_PARENT_RELEASE)
string(FIND "${RAW_ATTACHMENT_DESTROY}"
	"Ral_DestroyTexture( vk.sceneDepth.ral_image );" RAW_DEPTH_PARENT_RELEASE)
if(RAW_CHILD_RELEASE EQUAL -1 OR RAW_PARENT_RELEASE EQUAL -1
		OR RAW_DEPTH_PARENT_RELEASE EQUAL -1
		OR NOT RAW_CHILD_RELEASE LESS RAW_PARENT_RELEASE
		OR NOT RAW_CHILD_RELEASE LESS RAW_DEPTH_PARENT_RELEASE)
	message(FATAL_ERROR
		"attachment teardown lost portable-child-before-parent order")
endif()
foreach(RETIRED IN ITEMS
	"vk_ral_adopt_static_internal_textures"
	"vk_ral_destroy_adopted_internal_textures"
	"Ral_AdoptTextureExact( s_ral_backend"
	"&vk.color_descriptor )"
	"&vk.tonemapped_descriptor )"
	"&vk.screenMap.color_descriptor )"
	"&vk.bloom_image_descriptor[i] )"
	"&vk.sceneDepth.descriptor )"
	"desc.dstSet = vk.color_descriptor;"
	"desc.dstSet = vk.tonemapped_descriptor;"
	"desc.dstSet = vk.screenMap.color_descriptor;"
	"desc.dstSet = vk.bloom_image_descriptor[i];"
	"desc.dstSet = vk.sceneDepth.descriptor;"
	"RETAIN_ADOPT( vk.ral_color_descriptor"
	"RETAIN_ADOPT( vk.ral_tonemapped_descriptor"
	"RETAIN_ADOPT( vk.screenMap.ral_color_descriptor"
	"RETAIN_ADOPT( vk.ral_bloom_image_descriptor"
	"RETAIN_ADOPT( vk.sceneDepth.ral_descriptor")
	string(FIND "${PRODUCT}${PRODUCT_BINDINGS}" "${RETIRED}" POS)
	if(NOT POS EQUAL -1)
		message(FATAL_ERROR
			"attachment sampler cohort retained raw/adopted authority: ${RETIRED}")
	endif()
endforeach()

string(FIND "${PRODUCT}" "static qboolean vk_ral_register_depthfade_view" DEPTH_GROUP_BEGIN)
string(FIND "${PRODUCT}" "// Write the black sun-mask decline sentinel" DEPTH_GROUP_END)
if(DEPTH_GROUP_BEGIN EQUAL -1 OR DEPTH_GROUP_END EQUAL -1
		OR DEPTH_GROUP_END LESS DEPTH_GROUP_BEGIN)
	message(FATAL_ERROR "cannot isolate scene-depth direct RAL sampler cohort")
endif()
math(EXPR DEPTH_GROUP_LEN "${DEPTH_GROUP_END} - ${DEPTH_GROUP_BEGIN}")
string(SUBSTRING "${PRODUCT}" ${DEPTH_GROUP_BEGIN} ${DEPTH_GROUP_LEN} DEPTH_GROUP)
foreach(NEEDLE IN ITEMS
	"vk_ral_refresh_texture_sampler_group( vk.sceneDepth.ral_image,"
	"&vk.sceneDepth.ral_view, &vk.sceneDepth.ral_descriptor,"
	"vk.sceneDepth.ral_sampler, \"wired-scenedepth-bg\" )"
	"vk.sceneDepth.descriptor = Ral_GetBindGroupHandle(")
	require_text(DEPTH_GROUP "${NEEDLE}" "scene-depth direct RAL sampler cohort")
endforeach()
foreach(RETIRED IN ITEMS qvkAllocateDescriptorSets qvkUpdateDescriptorSets
	Ral_AdoptBindGroup Ral_AdoptTextureExact)
	string(FIND "${DEPTH_GROUP}" "${RETIRED}" POS)
	if(NOT POS EQUAL -1)
		message(FATAL_ERROR "scene-depth sampler retained raw/adopted authority: ${RETIRED}")
	endif()
endforeach()

string(FIND "${PRODUCT}" "// Shared effects per-draw UBO ring." EFFECTS_BEGIN)
string(FIND "${PRODUCT}" "// SMAA rtMetrics per-frame UBO." EFFECTS_END)
if(EFFECTS_BEGIN EQUAL -1 OR EFFECTS_END EQUAL -1 OR EFFECTS_END LESS EFFECTS_BEGIN)
	message(FATAL_ERROR "cannot isolate effects arena-owned bind-group span")
endif()
math(EXPR EFFECTS_LEN "${EFFECTS_END} - ${EFFECTS_BEGIN}")
string(SUBSTRING "${PRODUCT}" ${EFFECTS_BEGIN} ${EFFECTS_LEN} EFFECTS)
foreach(NEEDLE IN ITEMS
	"fxCreate.arena = vk.ral_descriptor_arena;"
	"fxCreate.arenaReceipt = &vk.ral_descriptor_arena_receipt;"
	"vk.effectsUbo.ral_descriptor[i] = Ral_CreateBindGroup("
	"vk.effectsUbo.descriptor[i] = (VkDescriptorSet)Ral_GetBindGroupHandle(")
	require_text(EFFECTS "${NEEDLE}" "effects arena-owned bind group")
endforeach()
foreach(RETIRED IN ITEMS qvkAllocateDescriptorSets qvkUpdateDescriptorSets)
	string(FIND "${EFFECTS}" "${RETIRED}" POS)
	if(NOT POS EQUAL -1)
		message(FATAL_ERROR "effects cohort retained raw descriptor authority: ${RETIRED}")
	endif()
endforeach()
require_text(PRODUCT_BINDINGS
	"Vulkan: effects RAL bind-group cohort drifted at slot %u"
	"effects adoption fallback retirement")

# Sprite set 0 is the first procedural-effects resource group whose complete
# shape is already portable: one exact STORAGE_BUFFER binding. Allocation and
# publication must therefore come only from the current RAL arena generation;
# the VkDescriptorSet field is a compatibility mirror, never an owner.
string(FIND "${PRODUCT_BINDINGS}"
	"void vk_ral_release_sprite_bindgroup" SPRITE_GROUP_BEGIN)
string(FIND "${PRODUCT_BINDINGS}"
	"void vk_ral_adopt_static_bindgroups" SPRITE_GROUP_END)
if(SPRITE_GROUP_BEGIN EQUAL -1 OR SPRITE_GROUP_END EQUAL -1
		OR SPRITE_GROUP_END LESS SPRITE_GROUP_BEGIN)
	message(FATAL_ERROR "cannot isolate sprite direct RAL bind-group owner")
endif()
math(EXPR SPRITE_GROUP_LEN "${SPRITE_GROUP_END} - ${SPRITE_GROUP_BEGIN}")
string(SUBSTRING "${PRODUCT_BINDINGS}" ${SPRITE_GROUP_BEGIN}
	${SPRITE_GROUP_LEN} SPRITE_GROUP)
foreach(NEEDLE IN ITEMS
	"Ral_DestroyBindGroup( vk.sprite.ral_descriptor[slot] );"
	"value.binding = 0u;"
	"value.type = RAL_BIND_STORAGE_BUFFER;"
	"value.bufferRange = bytes;"
	"createInfo.arena = vk.ral_descriptor_arena;"
	"createInfo.arenaReceipt = &vk.ral_descriptor_arena_receipt;"
	"vk.sprite.ral_descriptor[slot] = Ral_CreateBindGroup("
	"vk.sprite.descriptor[slot] = (VkDescriptorSet)Ral_GetBindGroupHandle(")
	require_text(SPRITE_GROUP "${NEEDLE}" "sprite direct RAL bind group")
endforeach()
foreach(RETIRED IN ITEMS qvkAllocateDescriptorSets qvkUpdateDescriptorSets
	Ral_AdoptBindGroup)
	string(FIND "${SPRITE_GROUP}" "${RETIRED}" POS)
	if(NOT POS EQUAL -1)
		message(FATAL_ERROR
			"sprite direct owner retained raw/adopted authority: ${RETIRED}")
	endif()
endforeach()
foreach(NEEDLE IN ITEMS
	"if ( vk.sprite.available && !vk_ral_refresh_sprite_bindgroup( i ) )"
	"vk_ral_release_sprite_bindgroup( i );")
	require_text(PRODUCT_BINDINGS "${NEEDLE}" "sprite static lifecycle")
endforeach()
foreach(RETIRED IN ITEMS
	"RETAIN_ADOPT( vk.sprite.ral_descriptor"
	"qvkAllocateDescriptorSets( vk.device, &dsAlloc, &vk.sprite.descriptor"
	"qvkUpdateDescriptorSets( vk.device, 1, writes, 0, NULL );")
	if(RETIRED STREQUAL "qvkUpdateDescriptorSets( vk.device, 1, writes, 0, NULL );")
		continue()
	endif()
	string(FIND "${PRODUCT}${PRODUCT_BINDINGS}" "${RETIRED}" POS)
	if(NOT POS EQUAL -1)
		message(FATAL_ERROR "sprite cohort retained legacy ownership: ${RETIRED}")
	endif()
endforeach()
string(FIND "${PRODUCT}" "void vk_init_sprite( void )" SPRITE_INIT_BEGIN)
string(FIND "${PRODUCT}" "void vk_shutdown_sprite( void )" SPRITE_INIT_END)
if(SPRITE_INIT_BEGIN EQUAL -1 OR SPRITE_INIT_END EQUAL -1
		OR SPRITE_INIT_END LESS SPRITE_INIT_BEGIN)
	message(FATAL_ERROR "cannot isolate sprite initialization")
endif()
math(EXPR SPRITE_INIT_LEN "${SPRITE_INIT_END} - ${SPRITE_INIT_BEGIN}")
string(SUBSTRING "${PRODUCT}" ${SPRITE_INIT_BEGIN} ${SPRITE_INIT_LEN} SPRITE_INIT)
foreach(RETIRED IN ITEMS qvkAllocateDescriptorSets qvkUpdateDescriptorSets)
	string(FIND "${SPRITE_INIT}" "${RETIRED}" POS)
	if(NOT POS EQUAL -1)
		message(FATAL_ERROR "sprite init retained raw descriptor authority: ${RETIRED}")
	endif()
endforeach()
string(FIND "${PRODUCT}" "void vk_shutdown_sprite( void )" SPRITE_SHUTDOWN_BEGIN)
string(FIND "${PRODUCT}" "void RB_DrawSprites( void )" SPRITE_SHUTDOWN_END)
math(EXPR SPRITE_SHUTDOWN_LEN "${SPRITE_SHUTDOWN_END} - ${SPRITE_SHUTDOWN_BEGIN}")
string(SUBSTRING "${PRODUCT}" ${SPRITE_SHUTDOWN_BEGIN}
	${SPRITE_SHUTDOWN_LEN} SPRITE_SHUTDOWN)
string(FIND "${SPRITE_SHUTDOWN}"
	"vk_ral_release_sprite_bindgroup( i );" SPRITE_GROUP_RELEASE)
string(FIND "${SPRITE_SHUTDOWN}"
	"VK_RalBufferShadowRelease( &vk_sprite_header_shadows[i] );"
	SPRITE_BUFFER_RELEASE)
if(SPRITE_GROUP_RELEASE EQUAL -1 OR SPRITE_BUFFER_RELEASE EQUAL -1
		OR NOT SPRITE_GROUP_RELEASE LESS SPRITE_BUFFER_RELEASE)
	message(FATAL_ERROR "sprite teardown lost bind group -> buffer order")
endif()

# Particle compute set 0 is the next complete portable cohort: one exact frame
# UBO plus read-pool, write-pool, and class SSBO bindings. Both ping-pong slots
# must be built candidate-first from the resource-owner receipts and current
# arena epoch; render set 0 remains legacy until its sampler arrays are split.
string(FIND "${PRODUCT}"
	"void vk_ral_release_particle_compute_bindgroups" PARTICLE_GROUP_BEGIN)
string(FIND "${PRODUCT}"
	"qboolean vk_particle_shadow_get_class" PARTICLE_GROUP_END)
if(PARTICLE_GROUP_BEGIN EQUAL -1 OR PARTICLE_GROUP_END EQUAL -1
		OR PARTICLE_GROUP_END LESS PARTICLE_GROUP_BEGIN)
	message(FATAL_ERROR "cannot isolate particle compute direct RAL owner")
endif()
math(EXPR PARTICLE_GROUP_LEN
	"${PARTICLE_GROUP_END} - ${PARTICLE_GROUP_BEGIN}")
string(SUBSTRING "${PRODUCT}" ${PARTICLE_GROUP_BEGIN}
	${PARTICLE_GROUP_LEN} PARTICLE_GROUP)
foreach(NEEDLE IN ITEMS
	"VK_RalFrameUniformGetResources( &vk_particle_frame,"
	"VK_RalFrameUniformResourcesReceiptExact("
	"VK_RalShadowStorageGetResources("
	"VK_RalShadowStorageResourcesReceiptExact("
	"Ral_BindGroupArenaReceiptValid("
	"vk.ral_descriptor_arena_receipt.backendIdentity != backend"
	"vk.ral_descriptor_arena_receipt.arenaIdentity"
	"frameReceipt.partialOffset != vk_particle_frame_config.partialOffset"
	"poolReceipt.elementSize != PARTICLE_BYTES"
	"classReceipt.elementSize != PARTICLE_CLASS_GPU_BYTES"
	"nativeIdentities[i] = Ral_GetBufferHandle("
	"identities[i] == identities[j]"
	"nativeIdentities[i] == nativeIdentities[j]"
	"values[0].type = RAL_BIND_UNIFORM_BUFFER;"
	"values[0].buffer = frameReceipt.buffers[i];"
	"values[1].type = RAL_BIND_STORAGE_BUFFER;"
	"values[1].buffer = poolReceipt.buffers[i];"
	"values[2].buffer = poolReceipt.buffers[writePool];"
	"values[3].buffer = classReceipt.buffers[0];"
	"values[0].bufferRange = frameBytes;"
	"values[1].bufferRange = poolBytes;"
	"values[3].bufferRange = classBytes;"
	"createInfo.arena = vk.ral_descriptor_arena;"
	"createInfo.arenaReceipt = &vk.ral_descriptor_arena_receipt;"
	"candidates[i] = Ral_CreateBindGroup( backend, &createInfo );"
	"rawCandidates[i] = Ral_GetBindGroupHandle( candidates[i] );"
	"vk.particle.ral_compute_descriptor[i] = candidates[i];"
	"vk.particle.compute_descriptor[i] = rawCandidates[i];"
	"if ( candidates[i] ) Ral_DestroyBindGroup( candidates[i] );")
	require_text(PARTICLE_GROUP "${NEEDLE}"
		"particle compute direct RAL bind group")
endforeach()
foreach(RETIRED IN ITEMS qvkAllocateDescriptorSets qvkUpdateDescriptorSets
	Ral_AdoptBindGroup)
	string(FIND "${PARTICLE_GROUP}" "${RETIRED}" POS)
	if(NOT POS EQUAL -1)
		message(FATAL_ERROR
			"particle compute owner retained raw/adopted authority: ${RETIRED}")
	endif()
endforeach()
foreach(NEEDLE IN ITEMS
	"Vulkan: particle compute RAL bind-group cohort drifted"
	"vk_ral_release_particle_compute_bindgroups();")
	require_text(PRODUCT_BINDINGS "${NEEDLE}"
		"particle compute static lifecycle")
endforeach()
foreach(RETIRED IN ITEMS
	"RETAIN_ADOPT( vk.particle.ral_compute_descriptor"
	"DESTROY_RETAINED_BG( vk.particle.ral_compute_descriptor"
	"qvkAllocateDescriptorSets( vk.device, &dsAlloc, &vk.particle.compute_descriptor"
	"dstSet          = vk.particle.compute_descriptor")
	string(FIND "${PRODUCT}${PRODUCT_BINDINGS}" "${RETIRED}" POS)
	if(NOT POS EQUAL -1)
		message(FATAL_ERROR
			"particle compute cohort retained legacy ownership: ${RETIRED}")
	endif()
endforeach()

# Atmospheric compute+render set 0 is one four-group current-arena cohort.
# It borrows exact pool/frame/heightgrid parents, creates a retained heightgrid
# view, validates scene-depth provenance and publishes only after all four RAL
# groups exist. No native descriptor allocation/update/adoption may reappear.
string(FIND "${PRODUCT}"
	"void vk_atmospheric_write_descriptors( void )" ATM_GROUP_BEGIN)
string(FIND "${PRODUCT}"
	"static void vk_init_atmospheric_compute_ral_pipeline( void );"
	ATM_GROUP_END)
if(ATM_GROUP_BEGIN EQUAL -1 OR ATM_GROUP_END EQUAL -1
		OR ATM_GROUP_END LESS ATM_GROUP_BEGIN)
	message(FATAL_ERROR "cannot isolate atmospheric direct RAL owner")
endif()
math(EXPR ATM_GROUP_LEN "${ATM_GROUP_END} - ${ATM_GROUP_BEGIN}")
string(SUBSTRING "${PRODUCT}" ${ATM_GROUP_BEGIN} ${ATM_GROUP_LEN} ATM_GROUP)
foreach(NEEDLE IN ITEMS
	"VK_AtmosphericPoolGetReceipt( &vk_atmospheric_pool, &poolReceipt )"
	"VK_AtmosphericPoolReceiptExact( &poolReceipt, &poolReceipt )"
	"VK_AtmosphericFrameGetResources( &vk_atmospheric_frame,"
	"VK_AtmosphericFrameResourcesReceiptExact("
	"VK_AtmosphericHeightgridGetReceipt("
	"VK_AtmosphericHeightgridReceiptExact("
	"Ral_BindGroupArenaReceiptValid("
	"vk.ral_descriptor_arena_receipt.backendIdentity != backend"
	"bufferIdentities[2] = poolReceipt.buffers[0];"
	"nativeBufferIdentities[i] = Ral_GetBufferHandle("
	"Ral_GetBufferSize( frameReceipt.buffers[i] ) != frameBytes"
	"Ral_GetBufferSize( poolReceipt.buffers[i] ) != poolBytes"
	"heightgridViewCandidate = Ral_CreateTextureView( backend, &viewInfo );"
	"values[0].type = RAL_BIND_UNIFORM_BUFFER;"
	"values[1].type = RAL_BIND_STORAGE_BUFFER;"
	"values[3].type = RAL_BIND_COMBINED_TEXTURE_SAMPLER;"
	"values[3].textureView = heightgridViewCandidate;"
	"values[2].textureView = vk.sceneDepth.ral_view;"
	"createInfo.arena = vk.ral_descriptor_arena;"
	"createInfo.arenaReceipt = &vk.ral_descriptor_arena_receipt;"
	"computeCandidates[i] = Ral_CreateBindGroup( backend, &createInfo );"
	"renderCandidates[i] = Ral_CreateBindGroup( backend, &createInfo );"
	"vk.atm.ral_compute_descriptor[i] = computeCandidates[i];"
	"vk.atm.ral_render_descriptor[i] = renderCandidates[i];"
	"vk.atm.ral_heightgrid_view = heightgridViewCandidate;"
	"if ( oldCompute[i] ) Ral_DestroyBindGroup( oldCompute[i] );"
	"if ( oldHeightgridView ) Ral_DestroyTextureView( oldHeightgridView );"
	"if ( !oldComplete ) vk.atm.available = qfalse;")
	require_text(ATM_GROUP "${NEEDLE}" "atmospheric direct RAL bind group")
endforeach()
foreach(RETIRED IN ITEMS qvkAllocateDescriptorSets qvkUpdateDescriptorSets
	Ral_AdoptBindGroup VkDescriptorBufferInfo VkDescriptorImageInfo
	VkWriteDescriptorSet)
	string(FIND "${ATM_GROUP}" "${RETIRED}" POS)
	if(NOT POS EQUAL -1)
		message(FATAL_ERROR
			"atmospheric cohort retained raw/adopted authority: ${RETIRED}")
	endif()
endforeach()
string(REGEX MATCHALL "Ral_CreateBindGroup[(]" ATM_CREATE_CALLS "${ATM_GROUP}")
list(LENGTH ATM_CREATE_CALLS ATM_CREATE_COUNT)
if(NOT ATM_CREATE_COUNT EQUAL 2)
	message(FATAL_ERROR
		"atmospheric cohort must have exactly two candidate group call sites")
endif()
string(FIND "${ATM_GROUP}"
	"vk.atm.ral_heightgrid_view = heightgridViewCandidate;" ATM_PUBLISH)
string(FIND "${ATM_GROUP}"
	"if ( oldCompute[i] ) Ral_DestroyBindGroup( oldCompute[i] );" ATM_RETIRE)
if(ATM_PUBLISH EQUAL -1 OR ATM_RETIRE EQUAL -1
		OR NOT ATM_PUBLISH LESS ATM_RETIRE)
	message(FATAL_ERROR
		"atmospheric cohort lost publish-before-retire ordering")
endif()
foreach(NEEDLE IN ITEMS
	"vk.atm.compute_descriptor[i] = VK_NULL_HANDLE;"
	"vk.atm.render_descriptor[i] = VK_NULL_HANDLE;")
	require_text(PRODUCT_BINDINGS "${NEEDLE}"
		"atmospheric arena-reset raw mirror retirement")
endforeach()
require_text(PRODUCT
	"+ 8 /* vk.atm heightgrid + scene-depth samplers"
	"atmospheric combined-sampler arena capacity")
foreach(RETIRED IN ITEMS
	"Ral_AdoptBindGroup(\n\t\t\tvk_ral_get_backend(), vk.atm.compute_descriptor"
	"Ral_AdoptBindGroup(\n\t\t\tvk_ral_get_backend(), vk.atm.render_descriptor")
	string(FIND "${PRODUCT}" "${RETIRED}" POS)
	if(NOT POS EQUAL -1)
		message(FATAL_ERROR
			"atmospheric cohort retained legacy adoption: ${RETIRED}")
	endif()
endforeach()

# Cold boot must not materialize atmospheric render groups before the shared
# scene-depth view exists. vk_init_descriptors first publishes attachment views,
# then creates the four-group cohort from the current arena generation.
string(FIND "${PRODUCT}" "void vk_init_atmospheric( void )" ATM_INIT_BEGIN)
string(FIND "${PRODUCT}"
	"static void vk_ral_release_atmospheric_render_bindgroups( void )"
	ATM_INIT_END)
if(ATM_INIT_BEGIN EQUAL -1 OR ATM_INIT_END EQUAL -1
		OR ATM_INIT_END LESS ATM_INIT_BEGIN)
	message(FATAL_ERROR "cannot isolate atmospheric resource initialization")
endif()
math(EXPR ATM_INIT_LEN "${ATM_INIT_END} - ${ATM_INIT_BEGIN}")
string(SUBSTRING "${PRODUCT}" ${ATM_INIT_BEGIN} ${ATM_INIT_LEN} ATM_INIT)
string(FIND "${ATM_INIT}" "vk_atmospheric_write_descriptors();"
	ATM_EARLY_GROUP_CREATE)
if(NOT ATM_EARLY_GROUP_CREATE EQUAL -1)
	message(FATAL_ERROR
		"atmospheric cohort materialized before scene-depth view publication")
endif()
require_text(ATM_INIT
	"Defer the\n\t// four-group transaction to that descriptor-init boundary"
	"atmospheric cold-boot deferral")

string(FIND "${PRODUCT}" "void vk_init_descriptors( void )"
	ATM_DESCRIPTOR_INIT_BEGIN)
string(FIND "${PRODUCT}" "static void vk_release_geometry_buffers( void )"
	ATM_DESCRIPTOR_INIT_END)
if(ATM_DESCRIPTOR_INIT_BEGIN EQUAL -1 OR ATM_DESCRIPTOR_INIT_END EQUAL -1
		OR ATM_DESCRIPTOR_INIT_END LESS ATM_DESCRIPTOR_INIT_BEGIN)
	message(FATAL_ERROR "cannot isolate descriptor initialization")
endif()
math(EXPR ATM_DESCRIPTOR_INIT_LEN
	"${ATM_DESCRIPTOR_INIT_END} - ${ATM_DESCRIPTOR_INIT_BEGIN}")
string(SUBSTRING "${PRODUCT}" ${ATM_DESCRIPTOR_INIT_BEGIN}
	${ATM_DESCRIPTOR_INIT_LEN} ATM_DESCRIPTOR_INIT)
string(FIND "${ATM_DESCRIPTOR_INIT}" "vk_update_attachment_descriptors();"
	ATM_DEPTH_VIEW_PUBLISH)
string(FIND "${ATM_DESCRIPTOR_INIT}" "vk_atmospheric_write_descriptors();"
	ATM_GROUP_CREATE)
if(ATM_DEPTH_VIEW_PUBLISH EQUAL -1 OR ATM_GROUP_CREATE EQUAL -1
		OR NOT ATM_DEPTH_VIEW_PUBLISH LESS ATM_GROUP_CREATE)
	message(FATAL_ERROR
		"atmospheric cohort must follow scene-depth view publication")
endif()

foreach(NEEDLE IN ITEMS
	"Particle RAL compute bind-group rebuild failed"
	"Particle RAL compute bind-group creation failed")
	require_text(PRODUCT "${NEEDLE}" "particle compute materialization")
endforeach()
string(FIND "${PRODUCT}" "void vk_shutdown_particle( void )"
	PARTICLE_SHUTDOWN_BEGIN)
string(FIND "${PRODUCT}" "// Stand up the GPU decal projector"
	PARTICLE_SHUTDOWN_END)
if(PARTICLE_SHUTDOWN_BEGIN EQUAL -1 OR PARTICLE_SHUTDOWN_END EQUAL -1
		OR PARTICLE_SHUTDOWN_END LESS PARTICLE_SHUTDOWN_BEGIN)
	message(FATAL_ERROR "cannot isolate particle shutdown")
endif()
math(EXPR PARTICLE_SHUTDOWN_LEN
	"${PARTICLE_SHUTDOWN_END} - ${PARTICLE_SHUTDOWN_BEGIN}")
string(SUBSTRING "${PRODUCT}" ${PARTICLE_SHUTDOWN_BEGIN}
	${PARTICLE_SHUTDOWN_LEN} PARTICLE_SHUTDOWN)
string(FIND "${PARTICLE_SHUTDOWN}"
	"vk_ral_release_particle_compute_bindgroups();" PARTICLE_RELEASE)
string(FIND "${PARTICLE_SHUTDOWN}"
	"VK_RalFrameUniformRelease( &vk_particle_frame );" PARTICLE_FRAME_RELEASE)
string(FIND "${PARTICLE_SHUTDOWN}"
	"VK_RalShadowStorageRelease( &vk_particle_pool_storage );"
	PARTICLE_POOL_RELEASE)
if(PARTICLE_RELEASE EQUAL -1 OR PARTICLE_FRAME_RELEASE EQUAL -1
		OR PARTICLE_POOL_RELEASE EQUAL -1
		OR NOT PARTICLE_RELEASE LESS PARTICLE_FRAME_RELEASE
		OR NOT PARTICLE_FRAME_RELEASE LESS PARTICLE_POOL_RELEASE)
	message(FATAL_ERROR
		"particle teardown lost compute groups -> buffer owners order")
endif()

string(FIND "${PRODUCT}" "// MSDF per-draw UBO ring." MSDF_BEGIN)
string(FIND "${PRODUCT}" "// Shared effects per-draw UBO ring." MSDF_END)
if(MSDF_BEGIN EQUAL -1 OR MSDF_END EQUAL -1 OR MSDF_END LESS MSDF_BEGIN)
	message(FATAL_ERROR "cannot isolate MSDF arena-owned bind-group span")
endif()
math(EXPR MSDF_LEN "${MSDF_END} - ${MSDF_BEGIN}")
string(SUBSTRING "${PRODUCT}" ${MSDF_BEGIN} ${MSDF_LEN} MSDF)
foreach(NEEDLE IN ITEMS
	"msdfCreate.arena = vk.ral_descriptor_arena;"
	"msdfCreate.arenaReceipt = &vk.ral_descriptor_arena_receipt;"
	"vk.msdf.ral_descriptor[i] = Ral_CreateBindGroup("
	"vk.msdf.descriptor[i] = (VkDescriptorSet)Ral_GetBindGroupHandle(")
	require_text(MSDF "${NEEDLE}" "MSDF arena-owned bind group")
endforeach()
foreach(RETIRED IN ITEMS qvkAllocateDescriptorSets qvkUpdateDescriptorSets)
	string(FIND "${MSDF}" "${RETIRED}" POS)
	if(NOT POS EQUAL -1)
		message(FATAL_ERROR "MSDF cohort retained raw descriptor authority: ${RETIRED}")
	endif()
endforeach()
require_text(PRODUCT_BINDINGS
	"Vulkan: MSDF RAL bind-group cohort drifted at slot %u"
	"MSDF adoption fallback retirement")

string(FIND "${PRODUCT}" "// SMAA rtMetrics per-frame UBO." SMAA_RT_BEGIN)
string(FIND "${PRODUCT}" "// End arena-owned SMAA rtMetrics cohort." SMAA_RT_END)
if(SMAA_RT_BEGIN EQUAL -1 OR SMAA_RT_END EQUAL -1
		OR SMAA_RT_END LESS SMAA_RT_BEGIN)
	message(FATAL_ERROR "cannot isolate SMAA rtMetrics arena-owned bind-group span")
endif()
math(EXPR SMAA_RT_LEN "${SMAA_RT_END} - ${SMAA_RT_BEGIN}")
string(SUBSTRING "${PRODUCT}" ${SMAA_RT_BEGIN} ${SMAA_RT_LEN} SMAA_RT)
foreach(NEEDLE IN ITEMS
	"rtCreate.arena = vk.ral_descriptor_arena;"
	"rtCreate.arenaReceipt = &vk.ral_descriptor_arena_receipt;"
	"vk.smaaRt.ral_descriptor[i] = Ral_CreateBindGroup("
	"vk.smaaRt.descriptor[i] = (VkDescriptorSet)Ral_GetBindGroupHandle(")
	require_text(SMAA_RT "${NEEDLE}" "SMAA rtMetrics arena-owned bind group")
endforeach()
foreach(RETIRED IN ITEMS qvkAllocateDescriptorSets qvkUpdateDescriptorSets)
	string(FIND "${SMAA_RT}" "${RETIRED}" POS)
	if(NOT POS EQUAL -1)
		message(FATAL_ERROR "SMAA rtMetrics retained raw descriptor authority: ${RETIRED}")
	endif()
endforeach()

string(FIND "${PRODUCT}" "// Per-frame scene-exposure UBO" EXPOSURE_BEGIN)
string(FIND "${PRODUCT}" "// Per-frame WiredUI SCENE backdrop UBO" EXPOSURE_END)
if(EXPOSURE_BEGIN EQUAL -1 OR EXPOSURE_END EQUAL -1
		OR EXPOSURE_END LESS EXPOSURE_BEGIN)
	message(FATAL_ERROR "cannot isolate exposure arena-owned bind-group span")
endif()
math(EXPR EXPOSURE_LEN "${EXPOSURE_END} - ${EXPOSURE_BEGIN}")
string(SUBSTRING "${PRODUCT}" ${EXPOSURE_BEGIN} ${EXPOSURE_LEN} EXPOSURE)
foreach(NEEDLE IN ITEMS
	"expCreate.arena = vk.ral_descriptor_arena;"
	"expCreate.arenaReceipt = &vk.ral_descriptor_arena_receipt;"
	"vk.exposure.ral_descriptor[i] = Ral_CreateBindGroup("
	"vk.exposure.descriptor[i] = (VkDescriptorSet)Ral_GetBindGroupHandle(")
	require_text(EXPOSURE "${NEEDLE}" "exposure arena-owned bind group")
endforeach()
foreach(RETIRED IN ITEMS qvkAllocateDescriptorSets qvkUpdateDescriptorSets)
	string(FIND "${EXPOSURE}" "${RETIRED}" POS)
	if(NOT POS EQUAL -1)
		message(FATAL_ERROR "exposure cohort retained raw descriptor authority: ${RETIRED}")
	endif()
endforeach()

string(FIND "${PRODUCT}" "// Per-frame WiredUI SCENE backdrop UBO" MENUBG_BEGIN)
string(FIND "${PRODUCT}" "// End arena-owned per-frame non-dynamic UBO cohorts." MENUBG_END)
if(MENUBG_BEGIN EQUAL -1 OR MENUBG_END EQUAL -1
		OR MENUBG_END LESS MENUBG_BEGIN)
	message(FATAL_ERROR "cannot isolate menu-backdrop arena-owned bind-group span")
endif()
math(EXPR MENUBG_LEN "${MENUBG_END} - ${MENUBG_BEGIN}")
string(SUBSTRING "${PRODUCT}" ${MENUBG_BEGIN} ${MENUBG_LEN} MENUBG)
foreach(NEEDLE IN ITEMS
	"mbgCreate.arena = vk.ral_descriptor_arena;"
	"mbgCreate.arenaReceipt = &vk.ral_descriptor_arena_receipt;"
	"vk.menubg.ral_descriptor[i] = Ral_CreateBindGroup("
	"vk.menubg.descriptor[i] = (VkDescriptorSet)Ral_GetBindGroupHandle(")
	require_text(MENUBG "${NEEDLE}" "menu-backdrop arena-owned bind group")
endforeach()
foreach(RETIRED IN ITEMS qvkAllocateDescriptorSets qvkUpdateDescriptorSets)
	string(FIND "${MENUBG}" "${RETIRED}" POS)
	if(NOT POS EQUAL -1)
		message(FATAL_ERROR "menu-backdrop cohort retained raw descriptor authority: ${RETIRED}")
	endif()
endforeach()

foreach(NEEDLE IN ITEMS
	"Vulkan: exposure RAL bind-group cohort drifted at slot %u"
	"Vulkan: menubg RAL bind-group cohort drifted at slot %u")
	require_text(PRODUCT_BINDINGS "${NEEDLE}" "non-dynamic UBO adoption fallback retirement")
endforeach()
foreach(RETIRED IN ITEMS
	"vk.exposure.ral_descriptor[i] = Ral_AdoptBindGroup"
	"vk.menubg.ral_descriptor[i] = Ral_AdoptBindGroup"
	"RETAIN_ADOPT( vk.smaaRt.ral_descriptor[i]")
	string(FIND "${PRODUCT_BINDINGS}" "${RETIRED}" POS)
	if(NOT POS EQUAL -1)
		message(FATAL_ERROR "non-dynamic UBO cohort retained adoption fallback: ${RETIRED}")
	endif()
endforeach()
foreach(NEEDLE IN ITEMS
	"vk.smaaRt.ral_buffer[i]"
	"vk.exposure.ral_buffer[i]"
	"vk.menubg.ral_buffer[i]")
	require_text(PRODUCT_BINDINGS "${NEEDLE}" "non-dynamic UBO wrapper teardown")
endforeach()

string(FIND "${PRODUCT}" "// Recreate the set-0 dynamic uniform groups" TESS_PRODUCT_BEGIN)
string(FIND "${PRODUCT}" "// MSDF per-draw UBO ring." TESS_PRODUCT_END)
if(TESS_PRODUCT_BEGIN EQUAL -1 OR TESS_PRODUCT_END EQUAL -1
		OR TESS_PRODUCT_END LESS TESS_PRODUCT_BEGIN)
	message(FATAL_ERROR "cannot isolate tess arena-owned allocation span")
endif()
math(EXPR TESS_PRODUCT_LEN "${TESS_PRODUCT_END} - ${TESS_PRODUCT_BEGIN}")
string(SUBSTRING "${PRODUCT}" ${TESS_PRODUCT_BEGIN} ${TESS_PRODUCT_LEN} TESS_PRODUCT)
require_text(TESS_PRODUCT "vk_ral_refresh_tess_uniform_bindgroup( i )"
	"tess arena-owned allocation call")
foreach(RETIRED IN ITEMS qvkAllocateDescriptorSets qvkUpdateDescriptorSets)
	string(FIND "${TESS_PRODUCT}" "${RETIRED}" POS)
	if(NOT POS EQUAL -1)
		message(FATAL_ERROR "tess cohort retained raw descriptor authority: ${RETIRED}")
	endif()
endforeach()

string(FIND "${PRODUCT_BINDINGS}" "qboolean vk_ral_refresh_tess_uniform_bindgroup" TESS_HELPER_BEGIN)
string(FIND "${PRODUCT_BINDINGS}" "void vk_ral_release_entmat_bindgroup" TESS_HELPER_END)
if(TESS_HELPER_BEGIN EQUAL -1 OR TESS_HELPER_END EQUAL -1
		OR TESS_HELPER_END LESS TESS_HELPER_BEGIN)
	message(FATAL_ERROR "cannot isolate tess arena-owned helper")
endif()
math(EXPR TESS_HELPER_LEN "${TESS_HELPER_END} - ${TESS_HELPER_BEGIN}")
string(SUBSTRING "${PRODUCT_BINDINGS}" ${TESS_HELPER_BEGIN} ${TESS_HELPER_LEN} TESS_HELPER)
foreach(NEEDLE IN ITEMS
	"createInfo.arena = vk.ral_descriptor_arena;"
	"createInfo.arenaReceipt = &vk.ral_descriptor_arena_receipt;"
	"vk.tess[slot].ral_uniform_descriptor = Ral_CreateBindGroup("
	"vk.tess[slot].uniform_descriptor = (VkDescriptorSet)Ral_GetBindGroupHandle(")
	require_text(TESS_HELPER "${NEEDLE}" "tess arena-owned helper")
endforeach()
string(FIND "${TESS_HELPER}" "Ral_AdoptBindGroup(" TESS_ADOPT)
if(NOT TESS_ADOPT EQUAL -1)
	message(FATAL_ERROR "tess helper retained bind-group adoption fallback")
endif()
string(FIND "${PRODUCT}" "static void vk_create_geometry_buffers( VkDeviceSize size )" GEOMETRY_CREATE_BEGIN)
string(FIND "${PRODUCT}" "static qboolean vk_create_effect_bind_group_layout" GEOMETRY_CREATE_END)
if(GEOMETRY_CREATE_BEGIN EQUAL -1 OR GEOMETRY_CREATE_END EQUAL -1
		OR GEOMETRY_CREATE_END LESS GEOMETRY_CREATE_BEGIN)
	message(FATAL_ERROR "cannot isolate geometry replacement bind-group lifecycle")
endif()
math(EXPR GEOMETRY_CREATE_LEN "${GEOMETRY_CREATE_END} - ${GEOMETRY_CREATE_BEGIN}")
string(SUBSTRING "${PRODUCT}" ${GEOMETRY_CREATE_BEGIN} ${GEOMETRY_CREATE_LEN} GEOMETRY_CREATE)
foreach(NEEDLE IN ITEMS
	"vk_ral_release_tess_uniform_bindgroup( (uint32_t)i );"
	"vk_release_geometry_buffers();")
	require_text(GEOMETRY_CREATE "${NEEDLE}" "tess geometry replacement lifecycle")
endforeach()
string(FIND "${GEOMETRY_CREATE}" "vk_ral_release_tess_uniform_bindgroup( (uint32_t)i );" GEOMETRY_GROUP_RELEASE)
string(FIND "${GEOMETRY_CREATE}" "vk_release_geometry_buffers();" GEOMETRY_BUFFER_RELEASE)
if(GEOMETRY_GROUP_RELEASE GREATER GEOMETRY_BUFFER_RELEASE)
	message(FATAL_ERROR "tess bind groups must be released before their geometry buffers")
endif()

string(FIND "${PRODUCT}" "static void vk_resize_geometry_buffer( void )" RESIZE_BEGIN)
string(FIND "${PRODUCT}" "qboolean vk_temporal_motion_seal_primary" RESIZE_END)
if(RESIZE_BEGIN EQUAL -1 OR RESIZE_END EQUAL -1 OR RESIZE_END LESS RESIZE_BEGIN)
	message(FATAL_ERROR "cannot isolate geometry resize bind-group lifecycle")
endif()
math(EXPR RESIZE_LEN "${RESIZE_END} - ${RESIZE_BEGIN}")
string(SUBSTRING "${PRODUCT}" ${RESIZE_BEGIN} ${RESIZE_LEN} RESIZE)
foreach(NEEDLE IN ITEMS
	"vk_create_geometry_buffers( vk.geometry_buffer_size_new );"
	"vk_ral_refresh_tess_uniform_bindgroup( (uint32_t)i )")
	require_text(RESIZE "${NEEDLE}" "tess geometry-resize lifecycle")
endforeach()
string(FIND "${RESIZE}" "vk_create_geometry_buffers( vk.geometry_buffer_size_new );" GEOMETRY_CREATE_CALL)
string(FIND "${RESIZE}" "vk_ral_refresh_tess_uniform_bindgroup( (uint32_t)i )" GEOMETRY_GROUP_REFRESH)
if(GEOMETRY_CREATE_CALL GREATER GEOMETRY_GROUP_REFRESH)
	message(FATAL_ERROR "tess bind groups must refresh after geometry replacement")
endif()
foreach(RETIRED IN ITEMS vk_update_uniform_descriptor qvkUpdateDescriptorSets)
	string(FIND "${GEOMETRY_CREATE}${RESIZE}" "${RETIRED}" POS)
	if(NOT POS EQUAL -1)
		message(FATAL_ERROR "geometry resize retained mutable raw descriptor path: ${RETIRED}")
	endif()
endforeach()

# IQM set 0 is a dynamic UBO group created once per command slot from the
# current exact arena generation. Neither cold boot nor pool-reset rebuild may
# retain a parallel raw descriptor allocation/update path.
string(FIND "${PRODUCT}" "// Recreate the IQM set-0 dynamic UBO groups" IQM_RESET_BEGIN)
string(FIND "${PRODUCT}" "// Sprite and primitive ribbon/rail/beam set 0 cohorts" IQM_RESET_END)
if(IQM_RESET_BEGIN EQUAL -1 OR IQM_RESET_END EQUAL -1
		OR IQM_RESET_END LESS IQM_RESET_BEGIN)
	message(FATAL_ERROR "cannot isolate IQM descriptor reset span")
endif()
math(EXPR IQM_RESET_LEN "${IQM_RESET_END} - ${IQM_RESET_BEGIN}")
string(SUBSTRING "${PRODUCT}" ${IQM_RESET_BEGIN} ${IQM_RESET_LEN} IQM_RESET)
require_text(IQM_RESET "vk_ral_refresh_iqm_bone_bindgroup( j )"
	"IQM arena-owned reset call")
foreach(RETIRED IN ITEMS qvkAllocateDescriptorSets qvkUpdateDescriptorSets)
	string(FIND "${IQM_RESET}" "${RETIRED}" POS)
	if(NOT POS EQUAL -1)
		message(FATAL_ERROR "IQM reset retained raw descriptor authority: ${RETIRED}")
	endif()
endforeach()

string(FIND "${PRODUCT}" "void vk_init_iqm_gpu_skinning( void )" IQM_INIT_BEGIN)
string(FIND "${PRODUCT}" "void vk_shutdown_iqm_gpu_skinning( void )" IQM_INIT_END)
if(IQM_INIT_BEGIN EQUAL -1 OR IQM_INIT_END EQUAL -1
		OR IQM_INIT_END LESS IQM_INIT_BEGIN)
	message(FATAL_ERROR "cannot isolate IQM GPU initialization span")
endif()
math(EXPR IQM_INIT_LEN "${IQM_INIT_END} - ${IQM_INIT_BEGIN}")
string(SUBSTRING "${PRODUCT}" ${IQM_INIT_BEGIN} ${IQM_INIT_LEN} IQM_INIT)
foreach(RETIRED IN ITEMS qvkAllocateDescriptorSets qvkUpdateDescriptorSets)
	string(FIND "${IQM_INIT}" "${RETIRED}" POS)
	if(NOT POS EQUAL -1)
		message(FATAL_ERROR "IQM cold boot retained raw descriptor authority: ${RETIRED}")
	endif()
endforeach()

string(FIND "${PRODUCT_BINDINGS}" "qboolean vk_ral_refresh_iqm_bone_bindgroup" IQM_HELPER_BEGIN)
string(FIND "${PRODUCT_BINDINGS}" "void vk_ral_release_entmat_bindgroup" IQM_HELPER_END)
if(IQM_HELPER_BEGIN EQUAL -1 OR IQM_HELPER_END EQUAL -1
		OR IQM_HELPER_END LESS IQM_HELPER_BEGIN)
	message(FATAL_ERROR "cannot isolate IQM arena-owned helper")
endif()
math(EXPR IQM_HELPER_LEN "${IQM_HELPER_END} - ${IQM_HELPER_BEGIN}")
string(SUBSTRING "${PRODUCT_BINDINGS}" ${IQM_HELPER_BEGIN} ${IQM_HELPER_LEN} IQM_HELPER)
foreach(NEEDLE IN ITEMS
	"Ral_GetBufferSize( buffer ) != vk.iqmGpu.ring_size"
	"value.type = RAL_BIND_UNIFORM_BUFFER;"
	"value.bufferRange = item;"
	"createInfo.arena = vk.ral_descriptor_arena;"
	"createInfo.arenaReceipt = &vk.ral_descriptor_arena_receipt;"
	"vk.iqmGpu.ral_bone_descriptor[slot] = Ral_CreateBindGroup("
	"vk.iqmGpu.bone_descriptor[slot] = (VkDescriptorSet)Ral_GetBindGroupHandle(")
	require_text(IQM_HELPER "${NEEDLE}" "IQM arena-owned helper")
endforeach()
foreach(RETIRED IN ITEMS Ral_AdoptBindGroup Ral_RegisterAdoptedBindGroupDynamicBuffer)
	string(FIND "${IQM_HELPER}" "${RETIRED}" POS)
	if(NOT POS EQUAL -1)
		message(FATAL_ERROR "IQM helper retained adoption fallback: ${RETIRED}")
	endif()
endforeach()
require_text(PRODUCT_BINDINGS "&& !vk_ral_refresh_iqm_bone_bindgroup( i )"
	"IQM static-cohort refresh")
require_text(PRODUCT_BINDINGS "vk_ral_release_iqm_bone_bindgroup( i );"
	"IQM arena teardown")
require_text(PRODUCT "vk_ral_release_iqm_bone_bindgroup( (uint32_t)i );"
	"IQM shutdown child-before-buffer teardown")
foreach(RETIRED IN ITEMS
	"vk.iqmGpu.ral_bone_descriptor[i] = Ral_AdoptBindGroup"
	"Ral_RegisterAdoptedBindGroupDynamicBuffer(\n\t\t\t\t\t\tgroup, 0u, buffer")
	string(FIND "${PRODUCT_BINDINGS}" "${RETIRED}" POS)
	if(NOT POS EQUAL -1)
		message(FATAL_ERROR "IQM static cohort retained adoption fallback: ${RETIRED}")
	endif()
endforeach()

# Main entity-matrix set 3 is a direct, candidate-first current-arena group.
# Its VkDescriptorSet field is only the native mirror consumed by legacy
# receipts; neither materialization path may allocate/update/adopt a set.
string(FIND "${PRODUCT_BINDINGS}" "void vk_ral_release_entmat_bindgroup" ENTMAT_HELPER_BEGIN)
string(FIND "${PRODUCT_BINDINGS}" "void vk_ral_release_sprite_bindgroup" ENTMAT_HELPER_END)
if(ENTMAT_HELPER_BEGIN EQUAL -1 OR ENTMAT_HELPER_END EQUAL -1
		OR ENTMAT_HELPER_END LESS ENTMAT_HELPER_BEGIN)
	message(FATAL_ERROR "cannot isolate entity-matrix arena-owned helper")
endif()
math(EXPR ENTMAT_HELPER_LEN "${ENTMAT_HELPER_END} - ${ENTMAT_HELPER_BEGIN}")
string(SUBSTRING "${PRODUCT_BINDINGS}" ${ENTMAT_HELPER_BEGIN}
	${ENTMAT_HELPER_LEN} ENTMAT_HELPER)
foreach(NEEDLE IN ITEMS
	"Ral_BindGroupArenaReceiptValid("
	"Ral_GetBufferSize( buffer ) != bufferSize"
	"value.type = RAL_BIND_STORAGE_BUFFER;"
	"value.bufferRange = bufferSize;"
	"createInfo.arena = vk.ral_descriptor_arena;"
	"createInfo.arenaReceipt = &vk.ral_descriptor_arena_receipt;"
	"candidate = Ral_CreateBindGroup( s_ral_backend, &createInfo );"
	"rawCandidate = (VkDescriptorSet)Ral_GetBindGroupHandle( candidate );"
	"vk.tess[slot].ral_entMatDesc = candidate;"
	"vk.tess[slot].entMatDesc = rawCandidate;"
	"if ( retired ) Ral_DestroyBindGroup( retired );"
	"vk.tess[slot].entMatDesc = VK_NULL_HANDLE;")
	require_text(ENTMAT_HELPER "${NEEDLE}" "entity-matrix direct arena helper")
endforeach()
foreach(RETIRED IN ITEMS Ral_AdoptBindGroup qvkAllocateDescriptorSets qvkUpdateDescriptorSets)
	string(FIND "${ENTMAT_HELPER}" "${RETIRED}" POS)
	if(NOT POS EQUAL -1)
		message(FATAL_ERROR "entity-matrix helper retained raw/adopt fallback: ${RETIRED}")
	endif()
endforeach()
string(FIND "${PRODUCT}" "static qboolean vk_entmat_materialize_slot_after_idle(" ENTMAT_PRODUCT_BEGIN)
string(FIND "${PRODUCT}" "void vk_entmat_begin_frame( void )" ENTMAT_PRODUCT_END)
if(ENTMAT_PRODUCT_BEGIN EQUAL -1 OR ENTMAT_PRODUCT_END EQUAL -1
		OR ENTMAT_PRODUCT_END LESS ENTMAT_PRODUCT_BEGIN)
	message(FATAL_ERROR "cannot isolate entity-matrix product lifecycle")
endif()
math(EXPR ENTMAT_PRODUCT_LEN "${ENTMAT_PRODUCT_END} - ${ENTMAT_PRODUCT_BEGIN}")
string(SUBSTRING "${PRODUCT}" ${ENTMAT_PRODUCT_BEGIN}
	${ENTMAT_PRODUCT_LEN} ENTMAT_PRODUCT)
require_count(ENTMAT_PRODUCT "vk_ral_refresh_entmat_bindgroup[(]" 2
	"entity-matrix direct refresh inventory")
foreach(RETIRED IN ITEMS qvkAllocateDescriptorSets qvkUpdateDescriptorSets)
	string(FIND "${ENTMAT_PRODUCT}" "${RETIRED}" POS)
	if(NOT POS EQUAL -1)
		message(FATAL_ERROR "entity-matrix product retained raw descriptor authority: ${RETIRED}")
	endif()
endforeach()
foreach(NEEDLE IN ITEMS
	"if ( !vk_entmat_materialize_slot_after_idle("
	"vk_temporal_entmat_ring.ready = qfalse;"
	"return qfalse;"
	"entity-matrix RAL bind-group refresh deferred")
	require_text(ENTMAT_PRODUCT "${NEEDLE}" "entity-matrix fail-closed lifecycle")
endforeach()

# SMAA owns five sampled texture cohorts directly through RAL. Raw descriptor
# fields are Vulkan compatibility mirrors only; allocation, update and retained
# adoption may not return. Arena refresh retires group -> view while the direct
# texture parents survive until the SMAA resource owner tears them down.
string(FIND "${PRODUCT}" "void vk_ral_release_smaa_sampler_cohorts" SMAA_SAMPLE_BEGIN)
string(FIND "${PRODUCT}" "void vk_update_attachment_descriptors" SMAA_SAMPLE_END)
if(SMAA_SAMPLE_BEGIN EQUAL -1 OR SMAA_SAMPLE_END EQUAL -1
		OR SMAA_SAMPLE_END LESS SMAA_SAMPLE_BEGIN)
	message(FATAL_ERROR "cannot isolate direct SMAA sampler cohorts")
endif()
math(EXPR SMAA_SAMPLE_LEN "${SMAA_SAMPLE_END} - ${SMAA_SAMPLE_BEGIN}")
string(SUBSTRING "${PRODUCT}" ${SMAA_SAMPLE_BEGIN} ${SMAA_SAMPLE_LEN} SMAA_SAMPLE)
foreach(NEEDLE IN ITEMS
	"Ral_DestroyBindGroup( (group) );"
	"Ral_DestroyTextureView( (view) );"
	"vk_ral_refresh_texture_sampler_group( vk.smaa.ral_edges_image,"
	"vk_ral_refresh_texture_sampler_group( vk.smaa.ral_blend_image,"
	"vk_ral_refresh_texture_sampler_group( vk.smaa.ral_input_image,"
	"vk_ral_refresh_texture_sampler_group( vk.smaa.ral_area_image,"
	"vk_ral_refresh_texture_sampler_group( vk.smaa.ral_search_image,"
	"vk.smaa.ral_point_sampler, \"wired-smaa-edges-bg\""
	"vk.smaa.ral_linear_sampler, \"wired-smaa-blend-bg\""
	"vk.smaa.ral_linear_sampler, \"wired-smaa-input-bg\""
	"vk.smaa.ral_linear_sampler, \"wired-smaa-area-bg\""
	"vk.smaa.ral_point_sampler, \"wired-smaa-search-bg\""
	"vk.smaa.edges_descriptor = Ral_GetBindGroupHandle("
	"vk.smaa.search_descriptor = Ral_GetBindGroupHandle(")
	require_text(SMAA_SAMPLE "${NEEDLE}" "direct SMAA sampler cohort")
endforeach()
foreach(NEEDLE IN ITEMS
	"struct ralTexture_s *ral_area_image;"
	"struct ralTextureView_s *ral_area_view;"
	"struct ralTexture_s *ral_search_image;"
	"struct ralTextureView_s *ral_search_view;"
	"struct ralTextureView_s *ral_edges_view;"
	"struct ralTextureView_s *ral_blend_view;"
	"struct ralTextureView_s *ral_input_view;"
	"void vk_ral_release_smaa_sampler_cohorts( void );")
	require_text(PRODUCT_HEADER "${NEEDLE}" "SMAA direct RAL inventory")
endforeach()
foreach(RETIRED IN ITEMS
	"qvkAllocateDescriptorSets( vk.device, &alloc, &vk.smaa.edges_descriptor )"
	"qvkAllocateDescriptorSets( vk.device, &alloc, &vk.smaa.blend_descriptor )"
	"qvkAllocateDescriptorSets( vk.device, &alloc, &vk.smaa.input_descriptor )"
	"qvkAllocateDescriptorSets( vk.device, &alloc, &vk.smaa.area_descriptor )"
	"qvkAllocateDescriptorSets( vk.device, &alloc, &vk.smaa.search_descriptor )"
	"desc.dstSet = vk.smaa.edges_descriptor;"
	"desc.dstSet = vk.smaa.blend_descriptor;"
	"desc.dstSet = vk.smaa.input_descriptor;"
	"desc.dstSet = vk.smaa.area_descriptor;"
	"desc.dstSet = vk.smaa.search_descriptor;"
	"RETAIN_ADOPT( vk.smaa.ral_edges_descriptor"
	"RETAIN_ADOPT( vk.smaa.ral_blend_descriptor"
	"RETAIN_ADOPT( vk.smaa.ral_input_descriptor"
	"RETAIN_ADOPT( vk.smaa.ral_area_descriptor"
	"RETAIN_ADOPT( vk.smaa.ral_search_descriptor")
	string(FIND "${PRODUCT}${PRODUCT_BINDINGS}" "${RETIRED}" POS)
	if(NOT POS EQUAL -1)
		message(FATAL_ERROR "SMAA sampler retained raw/adopted authority: ${RETIRED}")
	endif()
endforeach()
string(FIND "${SMAA_SAMPLE}" "Ral_DestroyBindGroup( (group) );" SMAA_GROUP_RELEASE)
string(FIND "${SMAA_SAMPLE}" "Ral_DestroyTextureView( (view) );" SMAA_VIEW_RELEASE)
if(SMAA_GROUP_RELEASE EQUAL -1 OR SMAA_VIEW_RELEASE EQUAL -1
		OR NOT SMAA_GROUP_RELEASE LESS SMAA_VIEW_RELEASE)
	message(FATAL_ERROR "SMAA sampler lost group -> view release order")
endif()
string(FIND "${PRODUCT}" "static void vk_smaa_release_resources" SMAA_RELEASE_BEGIN)
string(FIND "${PRODUCT}" "static void vk_shadow_destroy_group" SMAA_RELEASE_END)
if(SMAA_RELEASE_BEGIN EQUAL -1 OR SMAA_RELEASE_END EQUAL -1
		OR SMAA_RELEASE_END LESS SMAA_RELEASE_BEGIN)
	message(FATAL_ERROR "cannot isolate SMAA texture-parent release")
endif()
math(EXPR SMAA_RELEASE_LEN "${SMAA_RELEASE_END} - ${SMAA_RELEASE_BEGIN}")
string(SUBSTRING "${PRODUCT}" ${SMAA_RELEASE_BEGIN} ${SMAA_RELEASE_LEN} SMAA_RELEASE)
require_text(SMAA_RELEASE "vk_ral_release_smaa_sampler_cohorts();"
	"direct SMAA child release")
require_text(SMAA_RELEASE "Ral_DestroyTexture( (texture) );"
	"direct SMAA texture-parent release")
string(FIND "${SMAA_RELEASE}" "vk_ral_release_smaa_sampler_cohorts();" SMAA_CHILD_RELEASE)
string(FIND "${SMAA_RELEASE}" "Ral_DestroyTexture( (texture) );" SMAA_PARENT_RELEASE)
if(SMAA_CHILD_RELEASE GREATER SMAA_PARENT_RELEASE)
	message(FATAL_ERROR "SMAA texture parents must be released after bind-group/view children")
endif()

# Ribbon, rail-ribbon and beam share one WebGPU-shaped primitive sampling ABI:
# a fixed texture-view array plus one family sampler. Their per-frame groups are
# created candidate-first from the current RAL arena; native descriptor handles
# are compatibility mirrors only.
foreach(NEEDLE IN ITEMS
	"layout(set = 0, binding = 2) uniform texture2D shaderImages[PRIMITIVE_SHADER_IMAGE_MAX];"
	"layout(set = 0, binding = 3) uniform sampler shaderImageSampler;"
	"sampler2D( shaderImages[slot], shaderImageSampler )")
	require_text(RIBBON_FRAGMENT "${NEEDLE}" "ribbon split texture/sampler ABI")
endforeach()
foreach(NEEDLE IN ITEMS
	"layout(set = 0, binding = 1) uniform texture2D shaderImages[PRIMITIVE_SHADER_IMAGE_MAX];"
	"layout(set = 0, binding = 4) uniform sampler shaderImageSampler;"
	"sampler2D( shaderImages[slot], shaderImageSampler )")
	require_text(BEAM_FRAGMENT "${NEEDLE}" "beam split texture/sampler ABI")
endforeach()
foreach(PAIR IN ITEMS RIBBON_WGSL BEAM_WGSL)
	require_text(${PAIR} "binding_array<texture_2d<f32>, 64>" "portable primitive texture array")
	require_text(${PAIR} "var shaderImageSampler: sampler;" "portable primitive scalar sampler")
	string(FIND "${${PAIR}}" "binding_array<sampler" PRIMITIVE_SAMPLER_ARRAY)
	if(NOT PRIMITIVE_SAMPLER_ARRAY EQUAL -1)
		message(FATAL_ERROR "portable primitive shader regained a sampler binding array")
	endif()
endforeach()
require_text(RIBBON_WGSL "@group(0) @binding(3)" "ribbon WGSL sampler binding")
require_text(BEAM_WGSL "@group(0) @binding(4)" "beam WGSL sampler binding")
foreach(PAIR IN ITEMS RIBBON_MSL BEAM_MSL)
	require_text(${PAIR} "sampler shaderImageSampler [[id(64)]];" "Metal primitive scalar sampler")
	string(FIND "${${PAIR}}" "array<sampler" PRIMITIVE_MSL_SAMPLER_ARRAY)
	if(NOT PRIMITIVE_MSL_SAMPLER_ARRAY EQUAL -1)
		message(FATAL_ERROR "Metal primitive shader regained a sampler array")
	endif()
endforeach()

string(FIND "${PRODUCT_BINDINGS}" "void vk_ral_release_primitive_bindgroups" PRIMITIVE_HELPER_BEGIN)
string(FIND "${PRODUCT_BINDINGS}" "void vk_ral_adopt_static_bindgroups" PRIMITIVE_HELPER_END)
if(PRIMITIVE_HELPER_BEGIN EQUAL -1 OR PRIMITIVE_HELPER_END EQUAL -1
		OR PRIMITIVE_HELPER_END LESS PRIMITIVE_HELPER_BEGIN)
	message(FATAL_ERROR "cannot isolate primitive direct-RAL owner")
endif()
math(EXPR PRIMITIVE_HELPER_LEN "${PRIMITIVE_HELPER_END} - ${PRIMITIVE_HELPER_BEGIN}")
string(SUBSTRING "${PRODUCT_BINDINGS}" ${PRIMITIVE_HELPER_BEGIN} ${PRIMITIVE_HELPER_LEN} PRIMITIVE_HELPER)
foreach(NEEDLE IN ITEMS
	"ralBindGroup_t *ribbon[NUM_COMMAND_BUFFERS] = { NULL };"
	"ralBindGroup_t *rail[NUM_COMMAND_BUFFERS] = { NULL };"
	"ralBindGroup_t *beam[NUM_COMMAND_BUFFERS] = { NULL };"
	"Ral_BindGroupArenaReceiptExact( &vk.ral_descriptor_arena_receipt,"
	".type=RAL_BIND_TEXTURE_ARRAY"
	".textureArrayCount=PRIMITIVE_SHADER_IMAGE_MAX"
	".type=RAL_BIND_SAMPLER"
	"Ral_GetBufferSize( stageBuffer ) != stageBytes"
	"Ral_GetBufferSize( stageCountBuffer ) != stageCountBytes"
	"Ral_GetBufferSize( points ) != ribbonPointsBytes"
	"Ral_GetBufferSize( headers ) != ribbonHeadersBytes"
	"ribbon[i] = Ral_CreateBindGroup("
	"rail[i] = Ral_CreateBindGroup("
	"beam[i] = Ral_CreateBindGroup("
	"if ( nativeGroups[i] == nativeGroups[j] ) goto fail;"
	"ralBindGroup_t *oldRibbon = vk.ribbon.ral_descriptor[i];"
	"if ( oldRibbon ) Ral_DestroyBindGroup( oldRibbon );")
	require_text(PRIMITIVE_HELPER "${NEEDLE}" "primitive candidate-first RAL owner")
endforeach()
string(REGEX MATCHALL "Ral_CreateBindGroup[(]" PRIMITIVE_CREATE_CALLS "${PRIMITIVE_HELPER}")
list(LENGTH PRIMITIVE_CREATE_CALLS PRIMITIVE_CREATE_COUNT)
if(NOT PRIMITIVE_CREATE_COUNT EQUAL 3)
	message(FATAL_ERROR "primitive RAL create inventory drifted: ${PRIMITIVE_CREATE_COUNT}/3")
endif()
foreach(RETIRED IN ITEMS Ral_AdoptBindGroup qvkAllocateDescriptorSets qvkUpdateDescriptorSets)
	string(FIND "${PRIMITIVE_HELPER}" "${RETIRED}" POS)
	if(NOT POS EQUAL -1)
		message(FATAL_ERROR "primitive direct owner regained retired authority: ${RETIRED}")
	endif()
endforeach()

string(FIND "${PRODUCT}" "void vk_init_primitive_shader_images( void )" PRIMITIVE_PRODUCT_BEGIN)
string(FIND "${PRODUCT}" "int vk_alloc_primitive_shader_image_slot" PRIMITIVE_PRODUCT_END)
if(PRIMITIVE_PRODUCT_BEGIN EQUAL -1 OR PRIMITIVE_PRODUCT_END EQUAL -1
		OR PRIMITIVE_PRODUCT_END LESS PRIMITIVE_PRODUCT_BEGIN)
	message(FATAL_ERROR "cannot isolate primitive product publication")
endif()
math(EXPR PRIMITIVE_PRODUCT_LEN "${PRIMITIVE_PRODUCT_END} - ${PRIMITIVE_PRODUCT_BEGIN}")
string(SUBSTRING "${PRODUCT}" ${PRIMITIVE_PRODUCT_BEGIN} ${PRIMITIVE_PRODUCT_LEN} PRIMITIVE_PRODUCT)
foreach(NEEDLE IN ITEMS
	"vk_ral_refresh_primitive_bindgroups()"
	"vk_ral_release_primitive_bindgroups();"
	"Ral_BindGroupSetTextureViewAt(\n\t\t\tvk.ribbon.ral_descriptor[j]"
	"Ral_BindGroupSetTextureViewAt(\n\t\t\tvk.railRibbon.ral_descriptor[j]"
	"Ral_BindGroupSetTextureViewAt(\n\t\t\tvk.beam.ral_descriptor[j]")
	require_text(PRIMITIVE_PRODUCT "${NEEDLE}" "primitive product publication")
endforeach()
foreach(RETIRED IN ITEMS qvkAllocateDescriptorSets qvkUpdateDescriptorSets VkWriteDescriptorSet)
	string(FIND "${PRIMITIVE_PRODUCT}" "${RETIRED}" POS)
	if(NOT POS EQUAL -1)
		message(FATAL_ERROR "primitive product publication regained raw descriptor authority: ${RETIRED}")
	endif()
endforeach()
math(EXPR SMAA_RELEASE_LEN "${SMAA_RELEASE_END} - ${SMAA_RELEASE_BEGIN}")
string(SUBSTRING "${PRODUCT}" ${SMAA_RELEASE_BEGIN} ${SMAA_RELEASE_LEN} SMAA_RELEASE)
string(FIND "${SMAA_RELEASE}" "vk_ral_release_smaa_sampler_cohorts();" SMAA_CHILD_RELEASE)
string(FIND "${SMAA_RELEASE}" "Ral_DestroyTexture( (texture) );" SMAA_TEXTURE_RELEASE)
if(SMAA_CHILD_RELEASE EQUAL -1 OR SMAA_TEXTURE_RELEASE EQUAL -1
		OR NOT SMAA_CHILD_RELEASE LESS SMAA_TEXTURE_RELEASE)
	message(FATAL_ERROR
		"SMAA teardown lost RAL children -> direct texture-parent order")
endif()

string(REGEX MATCHALL "Ral_CreateBindGroupArena[(]" PRODUCT_CREATE_CALLS "${PRODUCT}")
list(LENGTH PRODUCT_CREATE_CALLS PRODUCT_CREATE_COUNT)
string(REGEX MATCHALL "vk_ral_reset_descriptor_arena[(]" PRODUCT_RESET_CALLS "${PRODUCT}")
list(LENGTH PRODUCT_RESET_CALLS PRODUCT_RESET_COUNT)
string(REGEX MATCHALL "Ral_DestroyBindGroupArena[(]" PRODUCT_DESTROY_CALLS "${PRODUCT}")
list(LENGTH PRODUCT_DESTROY_CALLS PRODUCT_DESTROY_COUNT)
if(NOT PRODUCT_CREATE_COUNT EQUAL 1 OR NOT PRODUCT_RESET_COUNT EQUAL 3
		OR NOT PRODUCT_DESTROY_COUNT EQUAL 1)
	message(FATAL_ERROR
		"product arena inventory drifted: create=${PRODUCT_CREATE_COUNT}/1 reset=${PRODUCT_RESET_COUNT}/3 destroy=${PRODUCT_DESTROY_COUNT}/1")
endif()
require_text(PRODUCT_HEADER "ralBindGroupArenaReceipt_t ral_descriptor_arena_receipt;"
	"product generation receipt")

foreach(NEEDLE IN ITEMS
	"first.generation == 1u"
	"second.generation == 2u"
	"stale.generation--"
	"lifecycle.generation = UINT64_MAX - 1u"
	"memcmp( &untouched, &sentinel")
	require_text(PORTABLE_HOST "${NEEDLE}" "portable/WebGPU-shaped mutation host")
endforeach()
string(FIND "${PORTABLE_HOST}" "Vk" NATIVE_HOST)
if(NOT NATIVE_HOST EQUAL -1)
	message(FATAL_ERROR "Vulkan token escaped portable/WebGPU-shaped arena host")
endif()
foreach(NEEDLE IN ITEMS
	"capturedInfo.flags == 0u"
	"VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC"
	"VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC"
	"resetResult = VK_ERROR_DEVICE_LOST"
	"resetResult = VK_ERROR_UNKNOWN"
	"capturedAllocationPool == (VkDescriptorPool)(uintptr_t)0x55u"
	"Ral_BindGroupArenaReceiptExact( &group->arenaReceipt, &first )"
	"!ralVk_BindGroupArenaLive( group )"
	"freeCalls == 0u"
	"arena->lifecycle.generation = UINT64_MAX - 1u"
	"createResult = VK_ERROR_OUT_OF_HOST_MEMORY"
	"destroyCalls == 1u")
	require_text(VULKAN_HOST "${NEEDLE}" "Vulkan arena mutation host")
endforeach()

message(STATUS "RAL bind-group arena policy: PASS")
