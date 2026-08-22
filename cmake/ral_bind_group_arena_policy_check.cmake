# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2024-present Wired Engine contributors

if(NOT DEFINED ROOT)
	message(FATAL_ERROR "ROOT is required")
endif()

file(READ "${ROOT}/code/renderer/ral/ral_bind_group_arena.h" HEADER)
file(READ "${ROOT}/code/renderer/ral/ral_bind_group_arena.c" CORE)
file(READ "${ROOT}/code/renderer/ral/ral_resource.h" RESOURCE_HEADER)
file(READ "${ROOT}/code/renderer/ral_vulkan/ral_vulkan_resource.c" VULKAN)
file(READ "${ROOT}/code/renderer/ral_vulkan/ral_vulkan_bridge.h" BRIDGE)
file(READ "${ROOT}/code/renderervk/vk.c" PRODUCT)
file(READ "${ROOT}/code/renderervk/vk.h" PRODUCT_HEADER)
file(READ "${ROOT}/tests/ral_bind_group_arena_lifecycle_test.c" PORTABLE_HOST)
file(READ "${ROOT}/tests/ral_vulkan_bind_group_arena_test.c" VULKAN_HOST)

function(require_text HAYSTACK NEEDLE LABEL)
	string(FIND "${${HAYSTACK}}" "${NEEDLE}" POS)
	if(POS EQUAL -1)
		message(FATAL_ERROR "missing ${LABEL}: ${NEEDLE}")
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
file(READ "${ROOT}/code/renderer/ral_vulkan/ral_vulkan_dynamic_bind.c" DYNAMIC_BIND)
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
