# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2024-present Wired Engine contributors

cmake_minimum_required(VERSION 3.16)
if(NOT DEFINED ROOT)
	message(FATAL_ERROR "ROOT required")
endif()
file(READ "${ROOT}/code/renderer/ral_vulkan/ral_vulkan_bridge.h" HEADER)
file(READ "${ROOT}/code/renderer/ral_vulkan/ral_vulkan_bridge.c" CORE)
file(READ "${ROOT}/code/renderervk/vk.c" VK)
file(READ "${ROOT}/code/renderervk/vk.h" VK_HEADER)
file(READ "${ROOT}/code/renderervk/vk_ral_textures.c" BOOT)
file(READ "${ROOT}/tests/ral_vulkan_bridge_test.c" HOST)
file(READ "${ROOT}/CMakeLists.txt" CMAKE_TEXT)

function(require_text variable needle label)
	string(FIND "${${variable}}" "${needle}" position)
	if(position EQUAL -1)
		message(FATAL_ERROR "RAL Vulkan bridge policy lost ${label}: ${needle}")
	endif()
endfunction()

function(forbid_text variable needle label)
	string(FIND "${${variable}}" "${needle}" position)
	if(NOT position EQUAL -1)
		message(FATAL_ERROR "RAL Vulkan bridge policy regained ${label}: ${needle}")
	endif()
endfunction()

require_text(HEADER "RAL_VULKAN_OBJECT_ROLE_COUNT" "bounded object-role vocabulary")
require_text(HEADER "qboolean RalVulkan_SetObjectName(" "isolated naming bridge")
require_text(HEADER "qboolean RalVulkan_CreateShaderModule(" "shader-module creation bridge")
require_text(HEADER "qboolean RalVulkan_DestroyShaderModule(" "shader-module teardown bridge")
require_text(HEADER "qboolean RalVulkan_RecordLegacyImageTransition(" "legacy image-transition bridge")
foreach(mapping IN ITEMS
	"RAL_VULKAN_OBJECT_ROLE_DEVICE: candidate = VK_OBJECT_TYPE_DEVICE"
	"RAL_VULKAN_OBJECT_ROLE_DEVICE_MEMORY: candidate = VK_OBJECT_TYPE_DEVICE_MEMORY"
	"RAL_VULKAN_OBJECT_ROLE_FENCE: candidate = VK_OBJECT_TYPE_FENCE"
	"RAL_VULKAN_OBJECT_ROLE_SEMAPHORE: candidate = VK_OBJECT_TYPE_SEMAPHORE"
	"RAL_VULKAN_OBJECT_ROLE_BUFFER: candidate = VK_OBJECT_TYPE_BUFFER"
	"RAL_VULKAN_OBJECT_ROLE_IMAGE: candidate = VK_OBJECT_TYPE_IMAGE"
	"RAL_VULKAN_OBJECT_ROLE_IMAGE_VIEW: candidate = VK_OBJECT_TYPE_IMAGE_VIEW"
	"RAL_VULKAN_OBJECT_ROLE_SHADER_MODULE: candidate = VK_OBJECT_TYPE_SHADER_MODULE"
	"RAL_VULKAN_OBJECT_ROLE_PIPELINE_LAYOUT: candidate = VK_OBJECT_TYPE_PIPELINE_LAYOUT"
	"RAL_VULKAN_OBJECT_ROLE_SAMPLER: candidate = VK_OBJECT_TYPE_SAMPLER"
	"RAL_VULKAN_OBJECT_ROLE_DESCRIPTOR_SET_LAYOUT: candidate = VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT"
	"RAL_VULKAN_OBJECT_ROLE_DESCRIPTOR_SET: candidate = VK_OBJECT_TYPE_DESCRIPTOR_SET"
	"RAL_VULKAN_OBJECT_ROLE_COMMAND_BUFFER: candidate = VK_OBJECT_TYPE_COMMAND_BUFFER")
	require_text(CORE "${mapping}" "exact object-role mapping")
endforeach()
require_text(CORE "if ( !b->haveDebugUtils ) return qtrue;" "optional naming no-op")
require_text(CORE "b->vk.SetDebugUtilsObjectNameEXT( b->device, &info ) == VK_SUCCESS" "backend-owned debug-utils dispatch")
require_text(CORE "magic != RAL_VK_SPIRV_MAGIC" "SPIR-V magic validation")
require_text(CORE "byteCount < 20u || ( byteCount & 3u ) != 0u" "SPIR-V byte validation")
require_text(CORE "node->next = b->legacyShaderModules;" "per-backend ownership registry")
require_text(CORE "b->vk.DestroyShaderModule( b->device, candidate, NULL );" "failed-create candidate cleanup")
require_text(CORE "*outIdentity = (void *)(uintptr_t)RAL_VK_H2U( candidate );" "output-atomic publication")
require_text(CORE "for ( link = &b->legacyShaderModules; *link; link = &( *link )->next )" "owned teardown lookup")
require_text(CORE "( aspectMask & ~supportedAspects ) != 0u" "bounded legacy image aspects")
require_text(CORE "srcStageOverride != VK_PIPELINE_STAGE_HOST_BIT" "bounded source-stage override")
require_text(CORE "case VK_IMAGE_LAYOUT_UNDEFINED:" "legacy undefined source mapping")
require_text(CORE "case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:" "legacy sampled-state mapping")
require_text(CORE "b->vk.CmdPipelineBarrier( commandBuffer, srcStage, dstStage, 0u," "backend-owned image barrier dispatch")
require_text(VK "RalVulkan_SetObjectName( vk_ral_get_backend(), obj, objType, objName );" "renderer bridge call")
require_text(VK "RalVulkan_CreateShaderModule( vk_ral_get_backend()," "renderer shader creation bridge")
require_text(VK "RalVulkan_DestroyShaderModule( vk_ral_get_backend()," "renderer shader teardown bridge")
require_text(VK "RalVulkan_RecordLegacyImageTransition( vk_ral_get_backend()," "renderer image transition bridge")
require_text(HOST "nameCalls == RAL_VULKAN_OBJECT_ROLE_COUNT" "all-role positive fixture")
require_text(HOST "backend.type = RAL_BACKEND_WEBGPU" "wrong-backend mutation")
require_text(HOST "optional-noop" "optional unavailable fixture")
require_text(HOST "missing-dispatch" "missing dispatch mutation")
require_text(HOST "dispatch-failure" "backend failure mutation")
require_text(HOST "invalidSpirv" "invalid SPIR-V mutation")
require_text(HOST "nextShaderCreateResult = VK_ERROR_DEVICE_LOST;" "shader driver failure")
require_text(HOST "backend.legacyShaderModules == NULL" "shader registry cleanup")
require_text(HOST "FakeCmdPipelineBarrier" "image transition dispatch fixture")
require_text(HOST "VK_IMAGE_ASPECT_COLOR_BIT | VK_IMAGE_ASPECT_DEPTH_BIT" "mixed-aspect rejection mutation")
require_text(HOST "VK_PIPELINE_STAGE_VERTEX_SHADER_BIT" "invalid stage mutation")
require_text(HOST "barrierCalls == 0u" "rejected transition command-inertness")
require_text(HOST "capturedImageBarrier.dstAccessMask == VK_ACCESS_TRANSFER_WRITE_BIT" "exact upload transition fixture")
foreach(body_name IN ITEMS VK VK_HEADER BOOT)
	foreach(retired IN ITEMS
		"qvkDebugMarkerSetObjectNameEXT"
		"VkDebugMarkerObjectNameInfoEXT"
		"VK_EXT_DEBUG_MARKER_EXTENSION_NAME"
		"debugMarkers")
		forbid_text(${body_name} "${retired}" "legacy debug-marker ownership")
	endforeach()
endforeach()
foreach(retired_shader_call IN ITEMS qvkCreateShaderModule qvkDestroyShaderModule)
	forbid_text(VK "${retired_shader_call}" "renderer-local shader-module dispatch")
endforeach()
forbid_text(VK "qvkCmdPipelineBarrier" "renderer-local image-transition dispatch")
require_text(CMAKE_TEXT "ral_vulkan_bridge_test" "bridge host registration")
require_text(CMAKE_TEXT "ral_vulkan_bridge_source_policy_contract" "bridge policy registration")
message(STATUS "RAL Vulkan bridge policy: naming, shader modules and legacy image transitions are backend-owned")
