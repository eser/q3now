# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2024-present Wired Engine contributors

if(NOT DEFINED SOURCE_ROOT OR NOT IS_DIRECTORY "${SOURCE_ROOT}")
	message(FATAL_ERROR "SOURCE_ROOT must name the q3now source tree")
endif()

function(strip_c_comments input output)
	set(clean "${input}")
	# Remove line comments first so disabled-code sentinels such as `//*` and
	# `//*/` cannot masquerade as unterminated block comments.
	string(REGEX REPLACE "//[^\r\n]*" "" clean "${clean}")
	while(TRUE)
		string(FIND "${clean}" "/*" comment_begin)
		if(comment_begin EQUAL -1)
			break()
		endif()
		string(SUBSTRING "${clean}" ${comment_begin} -1 comment_tail)
		string(FIND "${comment_tail}" "*/" comment_end_relative)
		if(comment_end_relative EQUAL -1)
			# Historical id code intentionally comments disabled tails through EOF.
			string(SUBSTRING "${clean}" 0 ${comment_begin} clean)
			break()
		endif()
		math(EXPR comment_end "${comment_begin} + ${comment_end_relative}")
		string(SUBSTRING "${clean}" 0 ${comment_begin} before)
		math(EXPR after_begin "${comment_end} + 2")
		string(SUBSTRING "${clean}" ${after_begin} -1 after)
		set(clean "${before}${after}")
	endwhile()
	set(${output} "${clean}" PARENT_SCOPE)
endfunction()

function(require_text body needle why)
	string(FIND "${body}" "${needle}" hit)
	if(hit EQUAL -1)
		message(FATAL_ERROR "raw API boundary lost ${why}: ${needle}")
	endif()
endfunction()

function(forbid_regex body pattern why)
	string(REGEX MATCH "${pattern}" hit "${body}")
	if(hit)
		message(FATAL_ERROR "raw API boundary leaked ${why}: ${hit}")
	endif()
endfunction()

set(public_abi_path "${SOURCE_ROOT}/code/renderercommon/tr_public.h")
set(client_path "${SOURCE_ROOT}/code/client/client.h")
set(sdl_path "${SOURCE_ROOT}/code/sdl/sdl_glimp.c")
set(vk_boot_path "${SOURCE_ROOT}/code/renderervk/vk_ral_textures.c")
set(vk_path "${SOURCE_ROOT}/code/renderervk/vk.c")
set(vk_caps_path "${SOURCE_ROOT}/code/renderer/ral_vulkan/ral_vulkan_caps.c")
foreach(path IN ITEMS "${public_abi_path}" "${client_path}" "${sdl_path}"
	"${vk_boot_path}" "${vk_path}" "${vk_caps_path}")
	if(NOT EXISTS "${path}")
		message(FATAL_ERROR "raw API boundary input missing: ${path}")
	endif()
endforeach()

file(READ "${public_abi_path}" public_abi)
file(READ "${client_path}" client)
file(READ "${sdl_path}" sdl)
file(READ "${vk_boot_path}" vk_boot)
file(READ "${vk_path}" vk)
file(READ "${vk_caps_path}" vk_caps)
strip_c_comments("${public_abi}" public_code)
strip_c_comments("${client}" client_code)
strip_c_comments("${vk}" vk_code)

require_text("${public_abi}" "#define\tREF_API_VERSION\t\t21" "renderer ABI generation")
require_text("${public_abi}" "(*VK_GetInstanceProcAddr)( void *nativeInstance, const char *name )" "opaque proc-loader callback")
require_text("${public_abi}" "(*VK_CreateSurface)( void *nativeInstance, uint64_t *outNativeSurface )" "fixed-width surface callback")
require_text("${client}" "VK_GetInstanceProcAddr( void *nativeInstance, const char *name )" "client opaque proc-loader declaration")
require_text("${client}" "VK_CreateSurface( void *nativeInstance, uint64_t *outNativeSurface )" "client fixed-width surface declaration")
foreach(common_code IN ITEMS "${public_code}" "${client_code}")
	forbid_regex("${common_code}" "(^|[^A-Za-z0-9_])Vk[A-Z][A-Za-z0-9_]*" "Vulkan handle type across renderer ABI")
	forbid_regex("${common_code}" "#[ \t]*include[ \t]*[<\"][^>\"]*vulkan" "Vulkan header across renderer ABI")
endforeach()

require_text("${sdl}" "VK_GetInstanceProcAddr( void *nativeInstance, const char *name )" "SDL opaque proc-loader adapter")
require_text("${sdl}" "qvkGetInstanceProcAddr( (VkInstance)nativeInstance, name )" "SDL-only instance conversion")
require_text("${sdl}" "VK_CreateSurface( void *nativeInstance, uint64_t *outNativeSurface )" "SDL fixed-width surface adapter")
require_text("${sdl}" "SDL_Vulkan_CreateSurface( SDL_window, (VkInstance)nativeInstance," "SDL-only surface conversion")
require_text("${sdl}" "*outNativeSurface = (uint64_t)(uintptr_t)surface;" "SDL surface publication")
require_text("${vk_boot}" "ri.VK_GetInstanceProcAddr( nativeInstance, name )" "native-free RAL host proc forwarding")
require_text("${vk_boot}" "ri.VK_CreateSurface( nativeInstance, outNativeSurface )" "native-free RAL host surface forwarding")
require_text("${vk}" "ri.VK_GetInstanceProcAddr((void *)vk.instance, #func)" "Vulkan migration-boundary proc call")

# Renderer boot consumes the backend-neutral caps snapshot rather than
# reacquiring VkPhysicalDeviceProperties. Pin both the renderer operands and
# the Vulkan backend mappings so WebGPU/Metal can publish the same authority.
foreach(caps_seam IN ITEMS
	"caps = Ral_GetCaps( vk_ral_get_backend() );"
	"vk.uniform_alignment = (uint32_t)caps->minUniformBufferAlignment;"
	"vk.maxAnisotropy = caps->maxSamplerAnisotropy;"
	"vk.timestampPeriodNs  = caps->timestampPeriodNs;"
	"glConfig.maxTextureSize = MIN( caps->maxTextureDimension2D"
	"glConfig.numTextureUnits = caps->maxSampledTexturesPerShaderStage;"
	"vk.maxBoundDescriptorSets = caps->maxBindGroups;"
	"Com_sprintf( glConfig.version_string, sizeof( glConfig.version_string ),"
	"vk.offscreenRender = caps->offscreenPresentation;"
	"if ( caps->deviceLocalMemoryBytes != 0u ) {"
	"if ( caps->hostVisibleDeviceLocalMemoryBytes != 0u ) {"
	"Q_strncpyz( glConfig.vendor_string, caps->vendorName,"
	"Q_strncpyz( glConfig.renderer_string, renderer_name( caps ),")
	require_text("${vk_code}" "${caps_seam}" "RAL caps renderer authority")
endforeach()
foreach(caps_mapping IN ITEMS
	"c->timestampPeriodNs         = L->timestampComputeAndGraphics"
	"? L->timestampPeriod : 0.0f;"
	"c->maxSampledTexturesPerShaderStage = L->maxPerStageDescriptorSamplers;"
	"c->maxBindGroups             = L->maxBoundDescriptorSets;"
	"ralVk_FillDriverIdentity( &b->physProps, c );"
	"ralVk_FillMemoryCapacity( &b->memProps, c );"
	"caps->hostVisibleDeviceLocalMemoryBytes = bytes;"
	"c->offscreenPresentation = p->vendorID == 0x10DE ? qfalse : qtrue;")
	require_text("${vk_caps}" "${caps_mapping}" "Vulkan caps publication")
endforeach()
require_text("${vk_code}"
	"Ral_TextureFormatSupports( backend, formats[i].portable,"
	"RAL depth-format capability selection")
foreach(memory_type_seam IN ITEMS
	"RalVulkan_FindMemoryType( vk_ral_get_backend(), memory_type_bits, (uint32_t)properties,"
	"&memory_type, NULL )"
	"&memory_type, &actual_properties )")
	require_text("${vk_code}" "${memory_type_seam}" "backend-owned memory-type selection")
endforeach()
forbid_regex("${vk_code}" "(^|[^A-Za-z0-9_])qvkGetPhysicalDeviceMemoryProperties([^A-Za-z0-9_]|$)"
	"renderer-local physical memory-properties query")
foreach(image_requirements_seam IN ITEMS
	"RalVulkan_GetImageMemoryRequirements("
	"vk_ral_get_backend(), (void *)(uintptr_t)image, &requirements )"
	"memory_requirements->memoryTypeBits = requirements.memoryTypeBits;")
	require_text("${vk_code}" "${image_requirements_seam}" "backend-owned image memory-requirements query")
endforeach()
foreach(retired_image_query IN ITEMS qvkGetImageMemoryRequirements qvkGetImageMemoryRequirements2KHR)
	forbid_regex("${vk_code}" "(^|[^A-Za-z0-9_])${retired_image_query}([^A-Za-z0-9_]|$)"
		"renderer-local image memory-requirements query ${retired_image_query}")
endforeach()
require_text("${vk_code}"
	"RalVulkan_SetObjectName( vk_ral_get_backend(), obj, objType, objName );"
	"backend-owned Vulkan object naming")
foreach(retired_debug_marker IN ITEMS
	qvkDebugMarkerSetObjectNameEXT
	VkDebugMarkerObjectNameInfoEXT
	VK_EXT_DEBUG_MARKER_EXTENSION_NAME
	debugMarkers)
	forbid_regex("${vk_code}" "(^|[^A-Za-z0-9_])${retired_debug_marker}([^A-Za-z0-9_]|$)"
		"retired legacy debug-marker ownership ${retired_debug_marker}")
endforeach()
foreach(hdr_format_seam IN ITEMS
	"const ralTextureFormatFeatures_t required ="
	"RAL_TEXTURE_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND"
	"RAL_TEXTURE_FORMAT_FEATURE_FILTER_LINEAR;"
	"vk_hdr_state.sfloat_supported = Ral_TextureFormatSupportsFeatures( backend,"
	"RAL_FORMAT_R16G16B16A16_SFLOAT, required );")
	require_text("${vk_code}" "${hdr_format_seam}" "RAL HDR format-feature authority")
endforeach()
string(REGEX MATCHALL "qvkGetPhysicalDeviceFormatProperties" raw_format_queries "${vk_code}")
list(LENGTH raw_format_queries raw_format_query_count)
if(NOT raw_format_query_count EQUAL 0)
	message(FATAL_ERROR
		"renderer reacquired raw format-property ownership: ${raw_format_query_count}")
endif()
foreach(capture_format_seam IN ITEMS
	"static void setup_surface_formats( void )"
	"vk.capture_format = VK_FORMAT_R8G8B8A8_UNORM;"
	"vk_set_rpfmt( RPFMT_CAPTURE,       captureFmt,"
	"create_color_attachment( gls.captureWidth, gls.captureHeight, VK_SAMPLE_COUNT_1_BIT, vk.capture_format,"
	"ralRpfmt  = RPFMT_CAPTURE;"
	"if ( program_index == 3 )"
	"frag_spec_data.srgb_swapchain = 0;"
	"hdr_display_active && program_index != 3"
	"srcFormat = vk.capture_format;"
	"sourceReceipt.format != vk_attachment_format_to_ral( srcFormat )")
	require_text("${vk_code}" "${capture_format_seam}" "shader-rendered R8 capture continuity")
endforeach()
foreach(retired_capture_token IN ITEMS "vk_blit_enabled" "blitEnabled")
	forbid_regex("${vk_code}" "(^|[^A-Za-z0-9_])${retired_capture_token}([^A-Za-z0-9_]|$)"
		"retired capture-blit authority ${retired_capture_token}")
endforeach()

# Draw issuance is backend-neutral even while adjacent legacy bindings are still
# being migrated.  Keep the product renderer from reacquiring raw draw entry
# points, and pin the operand-bearing seams whose values must survive the RAL
# forwarding unchanged (including WebGPU's firstInstance/vertexOffset shape).
forbid_regex("${vk_code}" "(^|[^A-Za-z0-9_])qvkCmdDraw(Indexed)?([^A-Za-z0-9_]|$)" "raw draw command ownership")
foreach(draw_seam IN ITEMS
	"Ral_CmdDrawIndexed( vk.cmd->ral_cmd, numIndexes, 1, firstIndex, 0, 0 );"
	"Ral_CmdDrawIndexed( vk.cmd->ral_cmd, indexCount, 1, firstIndex, 0, firstInstance );"
	"Ral_CmdDrawIndexed( vk.cmd->ral_cmd, vk.cmd->num_indexes, 1,"
	"Ral_CmdDraw( vk.cmd->ral_cmd, tess.numVertexes, 1,"
	"Ral_CmdDrawIndexed( vk.cmd->ral_cmd, rng->indexCount, 1, rng->firstIndex, 0, slot );"
	"Ral_CmdDrawIndexed( vk.cmd->ral_cmd, sl->indexCount, 1, sl->firstIndex, sl->vertexOffset, slot );"
	"Ral_CmdDrawIndexed( vk.cmd->ral_cmd, c->numIndexes, 1, c->firstIndex, 0, slot );")
	require_text("${vk_code}" "${draw_seam}" "portable draw operand seam")
endforeach()
string(REGEX MATCHALL "Ral_CmdDrawIndexed[(]" ral_indexed_draws "${vk_code}")
list(LENGTH ral_indexed_draws ral_indexed_draw_count)
string(REGEX MATCHALL "Ral_CmdDraw[(]" ral_draws "${vk_code}")
list(LENGTH ral_draws ral_draw_count)
if(NOT ral_indexed_draw_count EQUAL 11 OR NOT ral_draw_count EQUAL 17)
	message(FATAL_ERROR
		"portable draw inventory changed: indexed=${ral_indexed_draw_count}/11, draw=${ral_draw_count}/17")
endif()

forbid_regex("${vk_code}" "(^|[^A-Za-z0-9_])qvkCmdSet(Viewport|Scissor)([^A-Za-z0-9_]|$)"
	"raw viewport/scissor command ownership")
foreach(dynamic_seam IN ITEMS
	"Ral_CmdSetViewport( vk.cmd->ral_cmd, &viewport );"
	"Ral_CmdSetScissor( vk.cmd->ral_cmd, &scissor );"
	"portable_scissor.x = scissor_rect.offset.x;"
	"portable_scissor.height = scissor_rect.extent.height;"
	"portable_viewport.minDepth = viewport.minDepth;"
	"portable_viewport.maxDepth = viewport.maxDepth;"
	"Ral_CmdSetViewport( vk.cmd->ral_cmd, &portable_viewport );")
	require_text("${vk_code}" "${dynamic_seam}" "portable viewport/scissor operand seam")
endforeach()
string(REGEX MATCHALL "Ral_CmdSetViewport[(]" ral_viewports "${vk_code}")
list(LENGTH ral_viewports ral_viewport_count)
string(REGEX MATCHALL "Ral_CmdSetScissor[(]" ral_scissors "${vk_code}")
list(LENGTH ral_scissors ral_scissor_count)
if(NOT ral_viewport_count EQUAL 20 OR NOT ral_scissor_count EQUAL 21)
	message(FATAL_ERROR
		"portable dynamic-state inventory changed: viewport=${ral_viewport_count}/20, scissor=${ral_scissor_count}/21")
endif()

# These renderer-owned PFNs had no callsite left after the corresponding RAL
# lifecycle/command migrations.  Keep declaration-only loader debt from
# silently returning: each native entry point must stay backend-owned or absent.
foreach(retired_entry IN ITEMS
	qvkCreateDevice
	qvkEnumerateDeviceExtensionProperties
	qvkGetPhysicalDeviceFeatures
	qvkGetPhysicalDeviceFeatures2
	qvkGetPhysicalDeviceFormatProperties
	qvkGetPhysicalDeviceMemoryProperties
	qvkGetPhysicalDeviceProperties
	qvkGetPhysicalDeviceQueueFamilyProperties
	qvkGetPhysicalDeviceSurfaceSupportKHR
	qvkGetPhysicalDeviceSurfaceCapabilitiesKHR
	qvkGetPhysicalDeviceSurfacePresentModesKHR
	qvkGetPhysicalDeviceSurfaceFormatsKHR
	qvkGetPhysicalDeviceSurfaceFormats2KHR
	qvkCmdBlitImage
	qvkCmdBindPipeline
	qvkCmdBindDescriptorSets
	qvkCmdBindIndexBuffer
	qvkCmdBindVertexBuffers
	qvkCmdClearAttachments
	qvkCmdCopyBuffer
	qvkCmdCopyImage
	qvkCmdCopyImageToBuffer
	qvkCmdPipelineBarrier
	qvkCreateFence
	qvkCreateSemaphore
	qvkCreateShaderModule
	qvkDestroyFence
	qvkDestroySemaphore
	qvkDestroyShaderModule
	qvkResetFences
	qvkWaitForFences
	qvkCmdDispatch
	qvkCmdSetDepthBias
	qvkBeginCommandBuffer
	qvkEndCommandBuffer
	qvkResetCommandBuffer
	qvkAllocateCommandBuffers
	qvkCreateCommandPool
	qvkCreateComputePipelines
	qvkCreateFramebuffer
	qvkCreateGraphicsPipelines
	qvkCreatePipelineCache
	qvkCreateRenderPass
	qvkCreateSampler
	qvkDestroyCommandPool
	qvkDestroyFramebuffer
	qvkDestroyPipeline
	qvkDestroyPipelineCache
	qvkDestroyRenderPass
	qvkDestroySampler
	qvkDeviceWaitIdle
	qvkFlushMappedMemoryRanges
	qvkFreeCommandBuffers
	qvkFreeDescriptorSets
	qvkGetImageSubresourceLayout
	qvkGetBufferMemoryRequirements2KHR
	qvkGetImageMemoryRequirements
	qvkGetImageMemoryRequirements2KHR
	qvkDebugMarkerSetObjectNameEXT
	qvkInvalidateMappedMemoryRanges
	qvkQueueWaitIdle
	qvkQueueSubmit)
	forbid_regex("${vk_code}" "(^|[^A-Za-z0-9_])${retired_entry}([^A-Za-z0-9_]|$)"
		"retired declaration-only entry point ${retired_entry}")
endforeach()

# Portable/common/game code may not gain native graphics types or SDK includes.
# Backend implementations, the two SDL platform adapters, vendored Vulkan SDK
# headers, legacy GL renderer modules and host-only tools are explicit owners.
# renderervk is the one bounded migration-debt owner; the aggregate caps below
# can only decrease as TASK-202.5 moves code behind RAL.
file(GLOB_RECURSE production_sources
	"${SOURCE_ROOT}/code/*.c" "${SOURCE_ROOT}/code/*.h"
	"${SOURCE_ROOT}/code/*.cpp" "${SOURCE_ROOT}/code/*.hpp"
	"${SOURCE_ROOT}/code/*.m" "${SOURCE_ROOT}/code/*.mm")
set(vk_type_count 0)
set(vk_macro_count 0)
set(qvk_call_count 0)
foreach(path IN LISTS production_sources)
	file(RELATIVE_PATH rel "${SOURCE_ROOT}" "${path}")
	file(READ "${path}" body)
	strip_c_comments("${body}" code)

	if(rel MATCHES "^code/renderervk/")
		string(REGEX MATCHALL "(^|[^A-Za-z0-9_])Vk[A-Z][A-Za-z0-9_]*" matches "${code}")
		list(LENGTH matches count)
		math(EXPR vk_type_count "${vk_type_count} + ${count}")
		string(REGEX MATCHALL "(^|[^A-Za-z0-9_])VK_[A-Z0-9_]+" matches "${code}")
		list(LENGTH matches count)
		math(EXPR vk_macro_count "${vk_macro_count} + ${count}")
		string(REGEX MATCHALL "(^|[^A-Za-z0-9_])qvk[A-Z][A-Za-z0-9_]*" matches "${code}")
		list(LENGTH matches count)
		math(EXPR qvk_call_count "${qvk_call_count} + ${count}")
		continue()
	endif()

	if(rel MATCHES "^code/renderer/ral_(vulkan|metal|webgpu)/"
		OR rel MATCHES "^code/renderercommon/vulkan/"
		OR rel MATCHES "^code/renderer2/"
		OR rel MATCHES "^code/renderer/[^/]+\\.(c|h|cpp|hpp|m|mm)$"
		OR rel STREQUAL "code/sdl/sdl_glimp.c"
		OR rel STREQUAL "code/sdl/sdl_ral_presentation.mm"
		OR rel MATCHES "^code/tools/")
		continue()
	endif()

	foreach(pattern IN ITEMS
		"#[ \t]*include[ \t]*[<\"][^>\"]*(vulkan|Metal/Metal|webgpu|SDL_opengl|OpenGL/)"
		"(^|[^A-Za-z0-9_])Vk[A-Z][A-Za-z0-9_]*"
		"(^|[^A-Za-z0-9_])qvk[A-Z][A-Za-z0-9_]*"
		"(^|[^A-Za-z0-9_])(MTL[A-Z][A-Za-z0-9_]*|WGPU[A-Z][A-Za-z0-9_]*)"
		"(^|[^A-Za-z0-9_])(GLenum|GLuint|GLint|GLsizei|GLboolean|GLbitfield|GLfloat|GLsync|GLchar|GLintptr|GLsizeiptr)([^A-Za-z0-9_]|$)")
		string(REGEX MATCH "${pattern}" escaped "${code}")
		if(escaped)
			message(FATAL_ERROR "raw graphics API escaped backend/platform ownership in ${rel}: ${escaped}")
		endif()
	endforeach()
endforeach()

set(vk_type_cap 1544)
set(vk_macro_cap 1212)
set(qvk_call_cap 589)
if(DEFINED RAL_RAW_PRINT_COUNTS AND RAL_RAW_PRINT_COUNTS)
	message(WARNING
		"renderervk raw debt measurement: types=${vk_type_count}, macros=${vk_macro_count}, calls=${qvk_call_count}")
endif()
if(vk_type_count GREATER vk_type_cap OR vk_macro_count GREATER vk_macro_cap
	OR qvk_call_count GREATER qvk_call_cap)
	message(FATAL_ERROR
		"renderervk raw debt grew: types=${vk_type_count}/${vk_type_cap}, "
		"macros=${vk_macro_count}/${vk_macro_cap}, calls=${qvk_call_count}/${qvk_call_cap}")
endif()

set(cmake_path "${SOURCE_ROOT}/CMakeLists.txt")
set(readme_path "${SOURCE_ROOT}/tests/README.md")
if(EXISTS "${cmake_path}" AND EXISTS "${readme_path}")
	file(READ "${cmake_path}" cmake_source)
	file(READ "${readme_path}" readme)
	require_text("${cmake_source}" "ral_raw_api_boundary_source_policy_contract" "registered positive policy")
	require_text("${cmake_source}" "ral_raw_api_boundary_reintroduction_rejected" "registered negative mutation")
	require_text("${readme}" "ral_raw_api_boundary_source_policy_contract" "test inventory row")
endif()

message(STATUS "RAL raw API boundary: native-free ABI 21; renderervk debt ${vk_type_count}/${vk_macro_count}/${qvk_call_count} within monotonic caps")
