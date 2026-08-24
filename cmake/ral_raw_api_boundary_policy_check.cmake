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

set(public_abi_path "${SOURCE_ROOT}/code/render/frontend/tr_public.h")
set(client_path "${SOURCE_ROOT}/code/client/client.h")
set(sdl_path "${SOURCE_ROOT}/code/sdl/sdl_glimp.c")
set(vk_boot_path "${SOURCE_ROOT}/code/render/ral/backends/vulkan/renderer/vk_ral_textures.c")
set(vk_buffer_shadow_path "${SOURCE_ROOT}/code/render/ral/backends/vulkan/renderer/vk_ral_buffer_shadow.c")
set(vk_buffer_shadow_header_path "${SOURCE_ROOT}/code/render/ral/backends/vulkan/renderer/vk_ral_buffer_shadow.h")
set(vk_path "${SOURCE_ROOT}/code/render/ral/backends/vulkan/renderer/vk.c")
set(vk_header_path "${SOURCE_ROOT}/code/render/ral/backends/vulkan/renderer/vk.h")
set(vk_caps_path "${SOURCE_ROOT}/code/render/ral/backends/vulkan/ral_vulkan_caps.c")
set(vk_resource_path "${SOURCE_ROOT}/code/render/ral/backends/vulkan/ral_vulkan_resource.c")
foreach(path IN ITEMS "${public_abi_path}" "${client_path}" "${sdl_path}"
	"${vk_boot_path}" "${vk_path}" "${vk_header_path}" "${vk_caps_path}"
	"${vk_resource_path}" "${vk_buffer_shadow_path}"
	"${vk_buffer_shadow_header_path}")
	if(NOT EXISTS "${path}")
		message(FATAL_ERROR "raw API boundary input missing: ${path}")
	endif()
endforeach()

file(READ "${public_abi_path}" public_abi)
file(READ "${client_path}" client)
file(READ "${sdl_path}" sdl)
file(READ "${vk_boot_path}" vk_boot)
file(READ "${vk_buffer_shadow_path}" vk_buffer_shadow)
file(READ "${vk_buffer_shadow_header_path}" vk_buffer_shadow_header)
file(READ "${vk_path}" vk)
file(READ "${vk_header_path}" vk_header)
file(READ "${vk_caps_path}" vk_caps)
file(READ "${vk_resource_path}" vk_resource)
strip_c_comments("${public_abi}" public_code)
strip_c_comments("${client}" client_code)
strip_c_comments("${vk}" vk_code)
strip_c_comments("${vk_header}" vk_header_code)
strip_c_comments("${vk_buffer_shadow}" vk_buffer_shadow_code)
strip_c_comments("${vk_buffer_shadow_header}" vk_buffer_shadow_header_code)

require_text("${public_abi}" "#define\tREF_API_VERSION\t\t22" "renderer ABI generation")
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
forbid_regex("${vk_code}" "ri[.]VK_GetInstanceProcAddr|PFN_vkGetDeviceProcAddr|qvkGetDeviceProcAddr"
	"renderer-local device entry-point loader")

# All renderer-owned descriptor-set and pipeline-layout creation/destruction is
# direct RAL. Native mirrors may be read from typed owners, but raw ownership
# cannot return to temporal, core, shadow, or effect cohorts.
forbid_regex("${vk_code}"
	"qvk(Create|Destroy)DescriptorSetLayout[ \t\r\n]*\\("
	"renderer-owned descriptor-set-layout operation")
forbid_regex("${vk_code}"
	"qvk(Create|Destroy)PipelineLayout[ \t\r\n]*\\("
	"renderer-owned pipeline-layout operation")
foreach(layout_seam IN ITEMS
	"static void vk_create_direct_pipeline_layout("
	"*owner = Ral_CreatePipelineLayout( vk_ral_get_backend(), &desc );"
	"vk_create_direct_pipeline_layout( ral_layouts,"
	"vk_create_direct_pipeline_layout( shadowSets"
	"vk_create_direct_pipeline_layout( skinnedSets"
	"vk_create_direct_pipeline_layout( atestSets")
	require_text("${vk_code}" "${layout_seam}" "direct RAL layout ownership")
endforeach()
foreach(temporal_layout_seam IN ITEMS
	"ops.create = Ral_CreatePipelineLayout;"
	"ops.getHandle = Ral_GetPipelineLayoutHandle;"
	"ops.destroy = Ral_DestroyPipelineLayout;"
	"ops.createLayout = Ral_CreatePipelineLayout;"
	"ops.getLayoutHandle = Ral_GetPipelineLayoutHandle;"
	"ops.destroyLayout = Ral_DestroyPipelineLayout;"
	"input.borrowedLayouts[0] = vk.ral_bgl_uniform;"
	"input.borrowedLayouts[2] = vk.ral_bgl_engine_resources;")
	require_text("${vk_code}" "${temporal_layout_seam}" "portable temporal layout ownership")
endforeach()

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
forbid_regex("${vk_code}" "RalVulkan_GetImageMemoryRequirements"
	"renderer-side image memory-requirements query")
foreach(retired_image_query IN ITEMS qvkGetImageMemoryRequirements qvkGetImageMemoryRequirements2KHR)
	forbid_regex("${vk_code}" "(^|[^A-Za-z0-9_])${retired_image_query}([^A-Za-z0-9_]|$)"
		"renderer-local image memory-requirements query ${retired_image_query}")
endforeach()
foreach(retired_image_owner IN ITEMS
	qvkAllocateMemory qvkBindImageMemory qvkCreateImage qvkCreateImageView
	qvkDestroyImage qvkDestroyImageView qvkFreeMemory
	PFN_vkAllocateMemory PFN_vkBindImageMemory PFN_vkCreateImage
	PFN_vkCreateImageView PFN_vkDestroyImage PFN_vkDestroyImageView
	PFN_vkFreeMemory)
	forbid_regex("${vk_code}" "(^|[^A-Za-z0-9_])${retired_image_owner}([^A-Za-z0-9_]|$)"
		"retired image_t raw allocation authority ${retired_image_owner}")
endforeach()
foreach(direct_image_seam IN ITEMS
	"qboolean vk_ral_create_image_texture_candidate( image_t *image,"
	"candidate = Ral_CreateTexture( s_ral_backend, &createInfo );"
	"uploadRecorded = vk_ral_stage_texture_copy("
	"image->ral, update, (uint32_t)num_regions,")
	require_text("${vk_code}${vk_boot}" "${direct_image_seam}"
		"direct image_t RAL ownership")
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
	"vk.capture.ral_image = vk_create_attachment_texture( gls.captureWidth,"
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

# Tess geometry and the main entity-matrix ring use the same CPU-shadow owner as
# every other GPU-consumed renderer ring. No renderer-local persistent map helper
# or legacy Ral_MapBuffer/Ral_UnmapBuffer call may return.
foreach(retired_mapped_seam IN ITEMS
	"vk_create_persistent_mapped_buffer"
	"vk_destroy_persistent_mapped_buffer"
	"Ral_MapBuffer("
	"Ral_UnmapBuffer(")
	string(FIND "${vk_code}" "${retired_mapped_seam}" POSITION)
	if(NOT POSITION EQUAL -1)
		message(FATAL_ERROR
			"renderer regained persistent mapped GPU-consumer authority: ${retired_mapped_seam}")
	endif()
endforeach()
foreach(core_shadow_seam IN ITEMS
	"static vkRalBufferShadow_t vk_tess_geometry_shadows[NUM_COMMAND_BUFFERS];"
	"static vkRalBufferShadow_t vk_entmat_shadows[NUM_COMMAND_BUFFERS];"
	"VK_RalBufferShadowEnsure( &candidate[i], vk_ral_get_backend(),"
	"qboolean vk_tess_publish_shadow_range( uint32_t offset, uint32_t size )"
	"static qboolean vk_entmat_publish_shadow_range( uint32_t slotIndex,"
	"vk_entmat_publish_shadow_range( (uint32_t)vk.cmd_index,"
	"VK_RalBufferShadowRelease( &vk_tess_geometry_shadows[i] );"
	"VK_RalBufferShadowRelease( &vk_entmat_shadows[i] );")
	require_text("${vk_code}" "${core_shadow_seam}"
		"portable tess/entity CPU-shadow ownership")
endforeach()
foreach(buffer_shadow_seam IN ITEMS
	"createInfo.usage = (ralBufferUsage_t)( consumerUsage"
	"| RAL_BUFFER_TRANSFER_DST );"
	"candidate.bytes = (unsigned char *)calloc( 1u, (size_t)byteSize );"
	"Ral_BufferWriteImmediate( shadow->buffer, offset,"
	"VK_RalBufferShadowPublish( vkRalBufferShadow_t *shadow,")
	require_text("${vk_buffer_shadow_code}${vk_buffer_shadow_header_code}"
		"${buffer_shadow_seam}" "portable CPU-shadow buffer publication")
endforeach()
require_text("${vk_buffer_shadow_code}"
	"const uint32_t forbidden = RAL_BUFFER_MAP_READ | RAL_BUFFER_MAP_WRITE;"
	"CPU-shadow owner mapped-capability rejection")
require_text("${vk_buffer_shadow_code}"
	"&& ( (uint32_t)usage & forbidden ) == 0u;"
	"CPU-shadow owner mapped-capability gate")
foreach(cohort IN ITEMS msdf effectsUbo smaaRt exposure menubg)
	require_text("${vk_code}" "vk.${cohort}.ral_buffer[i]" "${cohort} RAL buffer ownership")
	forbid_regex("${vk_code}" "vk[.]${cohort}[.](buffer|memory)" "${cohort} raw buffer ownership")
endforeach()
foreach(sprite_buffer_seam IN ITEMS
	"vk.sprite.ral_headers_buffer[i]"
	"buffer = vk.sprite.ral_headers_buffer[slot];")
	require_text("${vk}${vk_boot}" "${sprite_buffer_seam}" "sprite RAL buffer ownership")
endforeach()
forbid_regex("${vk_code}" "vk[.]sprite[.]headers_(buffer|memory)" "sprite raw buffer ownership")
require_text("${vk_header_code}" "struct ralBuffer_s\t*ral_headers_buffer[NUM_COMMAND_BUFFERS];"
	"sprite RAL buffer field")
foreach(ribbon_buffer IN ITEMS points headers)
	require_text("${vk_code}" "vk.ribbon.ral_${ribbon_buffer}_buffer[i]"
		"ribbon ${ribbon_buffer} RAL buffer ownership")
	forbid_regex("${vk_code}" "vk[.]ribbon[.]${ribbon_buffer}_(buffer|memory)"
		"ribbon ${ribbon_buffer} raw buffer ownership")
endforeach()
require_text("${vk_code}" "vk.railRibbon.ral_header_buffer[i]"
	"rail-ribbon RAL buffer ownership")
forbid_regex("${vk_code}" "vk[.]railRibbon[.]header_(buffer|memory)"
	"rail-ribbon raw buffer ownership")
foreach(beam_buffer_seam IN ITEMS
	"vk.beam.ral_header_buffer[i]"
	"vk.ral_primitive_stages_buffer"
	"vk.ral_primitive_stage_counts_buffer"
	"stageBuffer = vk.ral_primitive_stages_buffer;"
	"stageCountBuffer = vk.ral_primitive_stage_counts_buffer;")
	require_text("${vk}${vk_boot}" "${beam_buffer_seam}"
		"beam primitive RAL buffer ownership")
endforeach()
foreach(retired_beam_buffer IN ITEMS
	"vk[.]beam[.]header_(buffer|memory)"
	"vk[.]primitive_(stages|stage_counts)_(buffer|memory)")
	forbid_regex("${vk_code}" "${retired_beam_buffer}"
		"beam primitive raw buffer ownership")
endforeach()
foreach(iqm_buffer_seam IN ITEMS
	"vk.iqmGpu.ral_bone_buffer[i]"
	"buffer = vk.iqmGpu.ral_bone_buffer[slot];")
	require_text("${vk}${vk_boot}" "${iqm_buffer_seam}"
		"IQM RAL bone-buffer ownership")
endforeach()
forbid_regex("${vk_code}" "vk[.]iqmGpu[.]bone_(buffer|memory)"
	"IQM raw bone-buffer ownership")
foreach(fp_param_seam IN ITEMS
	"vk.ral_fpTileParams[i]"
	"vk.ral_fpClusterParams"
	"tileParams = vk.ral_fpTileParams[slot];"
	"clusterParams = vk.ral_fpClusterParams;")
	require_text("${vk_code}" "${fp_param_seam}"
		"Forward+ RAL parameter-buffer ownership")
endforeach()
foreach(retired_fp_param IN ITEMS fpTileParamsBuf fpTileParamsMem fpClusterParamsBuf fpClusterParamsMem)
	forbid_regex("${vk_code}" "(^|[^A-Za-z0-9_])${retired_fp_param}([^A-Za-z0-9_]|$)"
		"Forward+ raw parameter-buffer ownership ${retired_fp_param}")
endforeach()
foreach(dlight_buffer IN ITEMS params faceMvp entMat)
	require_text("${vk_code}" "vk.dlightShadow.ral_${dlight_buffer}Buf"
		"dlight-shadow ${dlight_buffer} RAL buffer ownership")
	forbid_regex("${vk_code}" "vk[.]dlightShadow[.]${dlight_buffer}(Buf|Mem)"
		"dlight-shadow ${dlight_buffer} raw buffer ownership")
endforeach()
require_text("${vk_code}" "static qboolean vk_shadow_refresh_ral_buffer_group("
	"typed shadow bind-group refresh")
foreach(csm_buffer IN ITEMS cascadeMvp bone)
	require_text("${vk_code}" "vk.shadowMap.ral_${csm_buffer}Buf"
		"cascaded-shadow ${csm_buffer} RAL buffer ownership")
	forbid_regex("${vk_code}" "vk[.]shadowMap[.]${csm_buffer}(Buf|Mem)"
		"cascaded-shadow ${csm_buffer} raw buffer ownership")
endforeach()
foreach(entmat_seam IN ITEMS
	"struct ralBuffer_s *ral_entMatBuf;"
	"vk_ral_create_entmat_bindgroup_candidate("
	"VK_RalBufferShadowRelease( &vk_entmat_shadows[i] );"
	"Ral_GetBufferHandle( vk.cmd->ral_entMatBuf )")
	require_text("${vk_header}${vk_boot}${vk}" "${entmat_seam}"
		"growable tess entity-matrix RAL ownership")
endforeach()
forbid_regex("${vk_code}" "vk[.](cmd|tess[^.]*)->?entMat(Buf|Mem)"
	"growable tess entity-matrix raw ownership")
forbid_regex("${vk_header_code}"
	"Vk(Buffer|DeviceMemory)[ \t]+entMat(Buf|Mem)"
	"growable tess entity-matrix raw fields")
foreach(shadow_growable_seam IN ITEMS
	"struct ralBuffer_s *ral_shadowSnapBuf;"
	"struct ralBuffer_s *ral_shadowEntMatBuf;"
	"vk_shadow_create_ral_buffer_group_candidate("
	"(ralBufferUsage_t)( RAL_BUFFER_VERTEX | RAL_BUFFER_INDEX )"
	"Ral_CmdBindVertexBuffer( vk.cmd->ral_cmd, 0,"
	"vk.cmd->ral_shadowEntMatBuf")
	require_text("${vk_header}${vk}" "${shadow_growable_seam}"
		"growable shadow RAL buffer ownership")
endforeach()
foreach(retired_shadow_growable IN ITEMS
	shadowSnapBuf shadowSnapMem shadowEntMatBuf shadowEntMatMem)
	forbid_regex("${vk_code}${vk_header_code}"
		"(^|[^A-Za-z0-9_])${retired_shadow_growable}([^A-Za-z0-9_]|$)"
		"growable shadow raw ownership ${retired_shadow_growable}")
endforeach()
foreach(staging_buffer_seam IN ITEMS
	"struct ralBuffer_s *ral_buffer;"
	"vk.staging_buffer.ral_buffer = candidateBuffer;"
	"createInfo.usage = (ralBufferUsage_t)( RAL_BUFFER_TRANSFER_SRC"
	"| RAL_BUFFER_MAP_WRITE );"
	"candidateBuffer = Ral_CreateBuffer( vk_ral_get_backend(), &createInfo );"
	"Ral_DestroyBuffer( vk.staging_buffer.ral_buffer );"
	"static qboolean vk_ral_write_upload_buffer( ralBuffer_t *buffer,"
	"Ral_BufferMapBegin( buffer, &request, &ticket )"
	"Ral_BufferMapUnmap( buffer, &ticket )")
	require_text("${vk_header}${vk}" "${staging_buffer_seam}"
		"global staging RAL buffer ownership")
endforeach()
forbid_regex("${vk_header_code}"
	"struct staging_buffer_s[^{]*[{][^}]*Vk(Buffer|DeviceMemory)"
	"global staging raw buffer fields")
forbid_regex("${vk_code}" "vk[.]staging_buffer[.](handle|memory)"
	"global staging raw buffer ownership")
forbid_regex("${vk_code}" "vk[.]staging_buffer[.]ptr"
	"persistent-mapped global staging authority")
foreach(caster_buffer IN ITEMS caster casterAtest casterBmodel)
	require_text("${vk_header}${vk}" "ral_${caster_buffer}Buf"
		"shadow ${caster_buffer} RAL geometry ownership")
	forbid_regex("${vk_code}${vk_header_code}"
		"(^|[^A-Za-z0-9_])${caster_buffer}(Buf|Mem)([^A-Za-z0-9_]|$)"
		"shadow ${caster_buffer} raw geometry ownership")
endforeach()
foreach(caster_command_seam IN ITEMS
	"static ralBuffer_t *vk_create_device_local_buffer("
	"static qboolean vk_ral_stage_buffer_copy( ralBuffer_t *destination,"
	"recorded = Ral_CmdCopyBufferExact( command,"
	"Ral_CmdBindVertexBuffer( vk.cmd->ral_cmd, 0,"
	"Ral_CmdBindIndexBuffer( vk.cmd->ral_cmd,")
	require_text("${vk_code}" "${caster_command_seam}"
		"typed shadow caster geometry command path")
endforeach()
foreach(geometry_buffer_seam IN ITEMS
	"struct ralBuffer_s *ral_vertex_buffer;"
	"vk.tess[i].ral_vertex_buffer = candidate[i].buffer;"
	"vk.vbo.ral_vertex_buffer = candidate;"
	"RAL_BUFFER_VERTEX | RAL_BUFFER_INDEX"
	"Ral_CmdBindVertexBuffersExact( vk.cmd->ral_cmd,"
	"void vk_bind_index_buffer( ralBuffer_t *buffer, uint32_t offset )")
	require_text("${vk_header}${vk}" "${geometry_buffer_seam}"
		"portable tess/static-VBO geometry ownership")
endforeach()
foreach(smaa_staging_seam IN ITEMS
	"static qboolean vk_ral_stage_texture_copy( ralTexture_t *destination,"
	"vk_smaa_alloc_resources area LUT"
	"vk_smaa_alloc_resources search LUT"
	"image->ral, update, (uint32_t)num_regions")
	require_text("${vk}" "${smaa_staging_seam}"
		"typed SMAA/general texture upload staging path")
endforeach()
foreach(iqm_geometry_seam IN ITEMS
	"qboolean vk_create_iqm_vbo( ralBuffer_t **outVertBuf,"
	"\"wired-iqm-vertex\""
	"\"wired-iqm-index\""
	"Ral_CmdBindVertexBuffersExact( vk.cmd->ral_cmd, 0, 1,"
	"void vk_shadow_capture_iqm( ralBuffer_t *vertBuffer, ralBuffer_t *idxBuffer,"
	"vk_destroy_iqm_vbo( &data->ral_vertex_buffer,")
	require_text("${vk}" "${iqm_geometry_seam}"
		"typed IQM model geometry ownership")
endforeach()
forbid_regex("${vk_code}"
	"vk_(ral_record_registered_buffer_copy|ral_bind_registered_(vertex_buffers|index_buffer)|staging_buffer_native)"
	"retired native-buffer command adapters")
foreach(retired_buffer_registry IN ITEMS
	"vk_ral_lookup_buffer"
	"vk_ral_register_buffer"
	"vk_ral_unregister_buffer"
	"s_buf_pending"
	"s_buf_active")
	forbid_regex("${vk_code}${vk_boot}" "${retired_buffer_registry}"
		"retired native-buffer adoption registry")
endforeach()
foreach(buffer_diagnostic_seam IN ITEMS
	"ownership              : direct Ral_CreateBuffer/Ral_DestroyBuffer"
	"native adoption registry: retired"
	"accounting authority   : live backend memory budget above")
	require_text("${vk_boot}" "${buffer_diagnostic_seam}"
		"direct RAL buffer diagnostics")
endforeach()
foreach(forwardplus_depth_seam IN ITEMS
	"tci.debugName = \"wired-fp-depth-copy\";"
	"vk.ral_fpDepthImage = Ral_CreateTexture( backend, &tci );"
	"vci.aspect = RAL_TEXTURE_VIEW_ASPECT_DEPTH_ONLY;"
	"vk.ral_fpDepthView = Ral_CreateTextureView( backend, &vci );")
	require_text("${vk}" "${forwardplus_depth_seam}"
		"direct Forward+ depth texture ownership")
endforeach()
forbid_regex("${vk_header_code}"
	"Vk(Image|ImageView|DeviceMemory)[ \t]+fpDepth(Image|View|Memory)"
	"raw Forward+ depth texture fields")
forbid_regex("${vk_code}"
	"vk_ral_adopt_one_texture[(][ \t\r\n]*vk[.]fpDepth"
	"adopted Forward+ depth texture ownership")
foreach(scene_depth_seam IN ITEMS
	"texture_desc.debugName = \"wired-img-scenedepth\";"
	"vk.sceneDepth.ral_image = Ral_CreateTexture( vk_ral_get_backend(),"
	"vk_ral_refresh_texture_sampler_group( vk.sceneDepth.ral_image,"
	"Ral_BindGroupSetTextureViewAtBinding( ralSet,")
	require_text("${vk}" "${scene_depth_seam}"
		"direct shared scene-depth texture/view ownership")
endforeach()
require_text("${vk_header_code}" "struct ralTexture_s *ral_image;"
	"direct shared scene-depth texture field")
forbid_regex("${vk_code}"
	"vk[.]sceneDepth[.](image|view|memory)"
	"raw shared scene-depth texture access")
forbid_regex("${vk_code}"
	"Ral_AdoptTextureExact[(][^;]*sceneDepth"
	"adopted shared scene-depth texture ownership")
foreach(smaa_texture IN ITEMS edges blend input area search)
	require_text("${vk_code}"
		"vk.smaa.ral_${smaa_texture}_image = vk_smaa_create_texture("
		"direct SMAA ${smaa_texture} texture ownership")
endforeach()
foreach(smaa_texture_seam IN ITEMS
	"static ralTexture_t *vk_smaa_create_texture("
	"vk_ral_seed_smaa_intermediate_states( rcmd )"
	"RELEASE_SMAA_TEXTURE( vk.smaa.ral_edges_image );"
	"vk_ral_release_smaa_sampler_cohorts();")
	require_text("${vk_code}" "${smaa_texture_seam}"
		"direct SMAA texture/view lifecycle")
endforeach()
forbid_regex("${vk_code}"
	"vk[.]smaa[.](edges|blend|input|area|search)_(image|view|memory)"
	"raw SMAA texture ownership")
forbid_regex("${vk_code}"
	"(Ral_AdoptTextureExact|vk_ral_adopt_one_texture)[(][^;]*smaa"
	"adopted SMAA texture ownership")
foreach(dlight_texture_seam IN ITEMS
	"texture_desc.debugName = \"wired-dlight-shadow-atlas\";"
	"vk.dlightShadow.ral_image = Ral_CreateTexture( vk_ral_get_backend(),"
	"shadowViewCandidate = Ral_CreateTextureView( backend, &shadowViewInfo );")
	require_text("${vk_code}" "${dlight_texture_seam}"
		"direct dynamic-light shadow texture/view ownership")
endforeach()
forbid_regex("${vk_code}"
	"vk[.]dlightShadow[.](image|view|faceView|memory)"
	"raw dynamic-light shadow texture ownership")
forbid_regex("${vk_code}"
	"(Ral_AdoptTextureExact|vk_ral_adopt_one_texture)[(][^;]*dlightShadow"
	"adopted dynamic-light shadow texture ownership")
foreach(csm_texture_seam IN ITEMS
	"texture_desc.type = RAL_TEXTURE_2D_ARRAY;"
	"texture_desc.debugName = \"ral-shadowmap\";"
	"vk.shadowMap.ral_image = Ral_CreateTexture( vk_ral_get_backend(),")
	require_text("${vk_code}" "${csm_texture_seam}"
		"direct cascaded-shadow array texture ownership")
endforeach()
foreach(array_layer_seam IN ITEMS
	"layerViews = (VkImageView *)calloc( layers, sizeof( *layerViews ) );"
	"tex->layerViews = layerViews;"
	"ralVk_DeferDestroy( b, RAL_RES_IMAGE_VIEW,")
	require_text("${vk_resource}" "${array_layer_seam}"
		"direct RAL array-layer attachment-view lifecycle")
endforeach()
forbid_regex("${vk_code}"
	"vk[.]shadowMap[.](image|view|layerView|memory)"
	"raw cascaded-shadow texture ownership")
forbid_regex("${vk_code}" "Ral_AdoptArrayTexture[(]"
	"adopted cascaded-shadow array texture ownership")
foreach(attachment_texture_seam IN ITEMS
	"static ralTexture_t *vk_create_attachment_texture("
	"createInfo.format = vk_attachment_format_to_ral( format );"
	"vk.ral_color_image = vk_create_attachment_texture("
	"vk.ral_tonemapped_image = vk_create_attachment_texture("
	"vk.ral_depth_image = vk_create_depth_attachment_texture("
	"vk.screenMap.ral_color_image = vk_create_attachment_texture("
	"vk.screenMap.ral_depth_image = vk_create_attachment_texture("
	"vk.capture.ral_image = vk_create_attachment_texture("
	"Ral_DestroyTexture( vk.ral_bloom_image[i] );"
	"vk_ral_release_internal_texture_dependents();")
	require_text("${vk_code}" "${attachment_texture_seam}"
		"direct RAL framebuffer attachment ownership")
endforeach()
forbid_regex("${vk_header_code}"
	"Vk(Image|ImageView|DeviceMemory)[ \t]+(color_image|color_image_view|tonemapped_image|tonemapped_image_view|depth_image|depth_image_view|bloom_image|bloom_image_view|image_memory)"
	"raw framebuffer attachment fields")
forbid_regex("${vk_header_code}"
	"VkImage[ \t]+image;[ \t\r\n]*VkImageView[ \t]+image_view;[ \t\r\n]*struct ralTexture_s [*]ral_image"
	"raw capture attachment fields")
foreach(retired_attachment_owner IN ITEMS
	"vk_attach_desc"
	"vk_alloc_attachments"
	"vk_clear_attachment_pool"
	"create_color_attachment"
	"Ral_AdoptTextureExact[(][^;]*(color_image|tonemapped_image|screenMap|bloom_image|capture)"
	"vk_ral_adopt_one_texture")
	forbid_regex("${vk_code}" "${retired_attachment_owner}"
		"retired raw framebuffer attachment ownership")
endforeach()
forbid_regex("${vk_code}"
	"Vk(Buffer|DeviceMemory)[ \t]+staging_(buf|mem)"
	"raw one-shot staging buffer ownership")
forbid_regex("${vk_code}"
	"vk_ral_copy_buffer_to_texture_exact[(][^)]*VkBuffer"
	"native buffer-to-texture exact-copy seam")
foreach(retired_geometry_owner IN ITEMS
	"vk[.]vbo[.](vertex_buffer|buffer_memory)"
	"vk[.]geometry_buffer_memory"
	"vk[.](cmd|tess[^.]*)[-.>]+vertex_buffer([^_A-Za-z]|$)")
	forbid_regex("${vk_code}" "${retired_geometry_owner}"
		"raw tess/static-VBO geometry ownership")
endforeach()
forbid_regex("${vk_header_code}"
	"VkDeviceMemory[ \t]+(geometry_buffer_memory|buffer_memory)"
	"raw tess/static-VBO memory fields")
forbid_regex("${vk_header_code}"
	"Vk(Buffer|DeviceMemory)[ \t]+(buffer|memory)[[]NUM_COMMAND_BUFFERS[]]"
	"persistent mapped cohort raw fields")

# These renderer-owned PFNs had no callsite left after the corresponding RAL
# lifecycle/command migrations.  Keep declaration-only loader debt from
# silently returning: each native entry point must stay backend-owned or absent.
foreach(retired_entry IN ITEMS
	qvkAllocateDescriptorSets
	qvkGetDeviceProcAddr
	qvkCreateDescriptorSetLayout
	qvkDestroyDescriptorSetLayout
	qvkMapMemory
	qvkUnmapMemory
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
	qvkBindBufferMemory
	qvkCreateBuffer
	qvkDestroyBuffer
	qvkGetBufferMemoryRequirements
	qvkGetBufferMemoryRequirements2KHR
	qvkGetImageMemoryRequirements
	qvkGetImageMemoryRequirements2KHR
	qvkDebugMarkerSetObjectNameEXT
	qvkInvalidateMappedMemoryRanges
	qvkQueueWaitIdle
	qvkQueueSubmit
	qvkUpdateDescriptorSets)
	forbid_regex("${vk_code}" "(^|[^A-Za-z0-9_])${retired_entry}([^A-Za-z0-9_]|$)"
		"retired declaration-only entry point ${retired_entry}")
endforeach()

# Portable/common/game code may not gain native graphics types or SDK includes.
# Backend implementations, the two SDL platform adapters, vendored Vulkan SDK
# headers, legacy GL renderer modules and host-only tools are explicit owners.
# renderervk remains the Vulkan-specific translation product, so Vulkan value
# vocabulary is measured monotonically. Direct qvk entry-point ownership is not
# exempt: its cap is zero and the retired-entry inventory rejects recurrence.
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

	if(rel MATCHES "^code/render/ral/backends/vulkan/renderer/")
		string(REGEX MATCHALL "(^|[^A-Za-z0-9_])Vk[A-Z][A-Za-z0-9_]*" matches "${code}")
		list(LENGTH matches count)
		math(EXPR vk_type_count "${vk_type_count} + ${count}")
		# Portable renderer helpers intentionally use the VK_Ral* namespace. Strip
		# those mixed-case identifiers before counting all-uppercase Vulkan macros;
		# otherwise the prefix-only regex misclassifies every helper as `VK_R`.
		string(REGEX REPLACE "VK_Ral[A-Za-z0-9_]*" "VKPORTABLE" macro_code "${code}")
		string(REGEX MATCHALL "(^|[^A-Za-z0-9_])VK_[A-Z0-9_]+" matches "${macro_code}")
		list(LENGTH matches count)
		math(EXPR vk_macro_count "${vk_macro_count} + ${count}")
		string(REGEX MATCHALL "(^|[^A-Za-z0-9_])qvk[A-Z][A-Za-z0-9_]*" matches "${code}")
		list(LENGTH matches count)
		math(EXPR qvk_call_count "${qvk_call_count} + ${count}")
		continue()
	endif()

	if(rel MATCHES "^code/render/ral/backends/(vulkan|opengl|metal|webgpu)/"
		OR rel MATCHES "^code/render/ral/backends/vulkan/include/vulkan/"
		OR rel MATCHES "^code/renderer2/"
		OR rel MATCHES "^code/renderer/[^/]+\\.(c|h|cpp|hpp|m|mm)$"
		OR rel STREQUAL "code/sdl/sdl_glimp.c"
		OR rel STREQUAL "code/sdl/sdl_ral_presentation.mm"
		# Browser main is the Web platform adapter: it may adopt the abstract
		# WebGPU backend module but owns no shared/frontend graphics ABI.
		OR rel STREQUAL "code/web/web_main.c"
		OR rel STREQUAL "code/web/web_presentation.c"
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

set(vk_type_cap 942)
set(vk_macro_cap 1186)
set(qvk_call_cap 0)
if(DEFINED RAL_RAW_PRINT_COUNTS AND RAL_RAW_PRINT_COUNTS)
	message(WARNING
		"renderervk Vulkan-adapter vocabulary: types=${vk_type_count}, macros=${vk_macro_count}, calls=${qvk_call_count}")
endif()
if(vk_type_count GREATER vk_type_cap OR vk_macro_count GREATER vk_macro_cap
	OR qvk_call_count GREATER qvk_call_cap)
	message(FATAL_ERROR
		"renderervk Vulkan-adapter vocabulary grew: types=${vk_type_count}/${vk_type_cap}, "
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

message(STATUS "RAL raw API boundary: native-free ABI 22; explicit renderervk Vulkan-adapter vocabulary ${vk_type_count}/${vk_macro_count}, direct qvk ownership ${qvk_call_count}")
