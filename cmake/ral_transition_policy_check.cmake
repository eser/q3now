# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2024-present Wired Engine contributors

if(NOT DEFINED ROOT)
	message(FATAL_ERROR "ROOT is required")
endif()

set(HEADER "${ROOT}/code/render/ral/core/ral_transition.h")
file(READ "${HEADER}" TEXT)
file(READ "${ROOT}/code/render/ral/core/ral_command.h" COMMAND_TEXT)
file(READ "${ROOT}/code/render/ral/core/ral_resource.h" RESOURCE_HEADER_TEXT)
file(READ "${ROOT}/code/render/ral/backends/vulkan/ral_vulkan_transition.c" VULKAN_TEXT)
file(READ "${ROOT}/code/render/ral/backends/vulkan/ral_vulkan_command.c" LEGACY_COMMAND_TEXT)
file(READ "${ROOT}/code/render/ral/backends/vulkan/ral_vulkan_resource.c" RESOURCE_TEXT)
file(READ "${ROOT}/code/render/ral/backends/vulkan/ral_vulkan_internal.h" INTERNAL_TEXT)
file(READ "${ROOT}/code/render/ral/backends/vulkan/ral_vulkan_translate.h" TRANSLATE_HEADER_TEXT)
file(READ "${ROOT}/code/render/ral/backends/vulkan/ral_vulkan_bridge.h" BRIDGE_TEXT)
file(READ "${ROOT}/code/render/ral/backends/vulkan/ral_vulkan_translate.c" TRANSLATE_TEXT)
file(READ "${ROOT}/code/render/ral/backends/vulkan/renderer/vk.c" PRODUCT_TEXT)
file(READ "${ROOT}/code/render/ral/backends/vulkan/renderer/vk.h" PRODUCT_HEADER_TEXT)
file(READ "${ROOT}/code/render/ral/backends/vulkan/renderer/vk_ral_textures.c" PRODUCT_TEXTURE_TEXT)
file(READ "${ROOT}/code/render/ral/backends/vulkan/renderer/vk_ral_textures.h" PRODUCT_TEXTURE_HEADER_TEXT)
file(READ "${ROOT}/code/render/ral/backends/vulkan/renderer/tr_image.c" PRODUCT_IMAGE_TEXT)
file(READ "${ROOT}/code/render/ral/backends/vulkan/renderer/tr_local.h" PRODUCT_LOCAL_TEXT)
file(READ "${ROOT}/tests/ral_vulkan_translate_test.c" TRANSLATE_TEST_TEXT)
file(READ "${ROOT}/tests/ral_transition_command_test.c" TRANSITION_COMMAND_TEST_TEXT)

function(extract_between body begin end out)
	string(FIND "${body}" "${begin}" begin_pos)
	if(begin_pos EQUAL -1)
		message(FATAL_ERROR "portable RAL transition policy missing span begin: ${begin}")
	endif()
	string(SUBSTRING "${body}" ${begin_pos} -1 tail)
	string(FIND "${tail}" "${end}" end_pos)
	if(end_pos EQUAL -1)
		message(FATAL_ERROR "portable RAL transition policy missing span end: ${end}")
	endif()
	string(SUBSTRING "${tail}" 0 ${end_pos} span)
	set(${out} "${span}" PARENT_SCOPE)
endfunction()

foreach(FORBIDDEN IN ITEMS
	"VkImageLayout" "VkAccess" "VkPipelineStage" "VkQueue"
	"VK_IMAGE_LAYOUT" "VK_ACCESS" "VK_PIPELINE_STAGE" "queueFamilyIndex")
	string(FIND "${TEXT}" "${FORBIDDEN}" POSITION)
	if(NOT POSITION EQUAL -1)
		message(FATAL_ERROR "portable RAL transition header leaked native Vulkan token: ${FORBIDDEN}")
	endif()
endforeach()

foreach(REQUIRED IN ITEMS
	"buf->portableStateKnown = qfalse"
	"WebGPU requires that access"
	"info->bufferMemoryBarriers[i].buffer->portableStateKnown = qfalse"
	"info->imageMemoryBarriers[i].texture->portableStateKnown = qfalse"
	"src->portableState.usage != RAL_RESOURCE_USAGE_COPY_SOURCE"
	"dst->portableState.usage != RAL_RESOURCE_USAGE_COPY_DESTINATION")
	string(FIND "${RESOURCE_TEXT}${LEGACY_COMMAND_TEXT}" "${REQUIRED}" POSITION)
	if(POSITION EQUAL -1)
		message(FATAL_ERROR "legacy RAL path lost fail-closed portable-state seam: ${REQUIRED}")
	endif()
endforeach()

foreach(REQUIRED IN ITEMS
	"ralTextureAspectFlags_t aspects;"
	"GPUImageCopyTexture.aspect"
	"qboolean Ral_CmdCopyTextureToBuffer"
	"ralVk_TranslateTextureCopyAspect"
	"( candidate & ( candidate - 1u ) ) != 0u"
	"bic.imageSubresource.aspectMask     = aspect;")
	string(FIND "${COMMAND_TEXT}${TRANSLATE_HEADER_TEXT}${TRANSLATE_TEXT}${LEGACY_COMMAND_TEXT}"
		"${REQUIRED}" POSITION)
	if(POSITION EQUAL -1)
		message(FATAL_ERROR "portable buffer-texture aspect contract lost: ${REQUIRED}")
	endif()
endforeach()

foreach(REQUIRED IN ITEMS
	"RAL_BARRIER_COLOR_ATTACHMENT_TO_FRAGMENT"
	"case RAL_BARRIER_COLOR_ATTACHMENT_TO_FRAGMENT:"
	"out.srcStage  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT"
	"out.dstStage  = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT"
	"out.srcAccess = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT"
	"out.dstAccess = VK_ACCESS_SHADER_READ_BIT")
	string(FIND "${COMMAND_TEXT}${TRANSLATE_TEXT}" "${REQUIRED}" POSITION)
	if(POSITION EQUAL -1)
		message(FATAL_ERROR "color-attachment visibility scope lost: ${REQUIRED}")
	endif()
endforeach()
extract_between("${PRODUCT_TEXT}"
	"// MoltenVK/TBDR: flush tile cache so gamma pass sees writes"
	"// gamma is now a thin display-encode pass" APPLE_GAMMA_VISIBILITY_TEXT)
extract_between("${PRODUCT_TEXT}"
	"// MoltenVK/TBDR: subpass external deps alone don't flush tile cache on Apple Silicon."
	"// bloom extraction." APPLE_BLOOM_VISIBILITY_TEXT)
foreach(SPAN_NAME IN ITEMS APPLE_GAMMA_VISIBILITY_TEXT APPLE_BLOOM_VISIBILITY_TEXT)
	foreach(REQUIRED IN ITEMS
		"r_vkApplePinkBarrier->integer"
		"Ral_CmdPipelineBarrier( vk.cmd->ral_cmd"
		"RAL_BARRIER_COLOR_ATTACHMENT_TO_FRAGMENT")
		string(FIND "${${SPAN_NAME}}" "${REQUIRED}" POSITION)
		if(POSITION EQUAL -1)
			message(FATAL_ERROR "${SPAN_NAME} lost portable Apple visibility barrier: ${REQUIRED}")
		endif()
	endforeach()
	foreach(FORBIDDEN IN ITEMS "VkImageMemoryBarrier" "qvkCmdPipelineBarrier"
		"VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT"
		"VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT"
		"VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT" "VK_ACCESS_SHADER_READ_BIT")
		string(FIND "${${SPAN_NAME}}" "${FORBIDDEN}" POSITION)
		if(NOT POSITION EQUAL -1)
			message(FATAL_ERROR "${SPAN_NAME} regained native visibility authority: ${FORBIDDEN}")
		endif()
	endforeach()
endforeach()
string(REGEX MATCHALL "RAL_BARRIER_COLOR_ATTACHMENT_TO_FRAGMENT"
	APPLE_VISIBILITY_CALLS "${APPLE_GAMMA_VISIBILITY_TEXT}${APPLE_BLOOM_VISIBILITY_TEXT}")
list(LENGTH APPLE_VISIBILITY_CALLS APPLE_VISIBILITY_CALL_COUNT)
if(NOT APPLE_VISIBILITY_CALL_COUNT EQUAL 2)
	message(FATAL_ERROR
		"Apple visibility scope inventory changed: expected 2, got ${APPLE_VISIBILITY_CALL_COUNT}")
endif()
foreach(REQUIRED IN ITEMS
	"VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT"
	"VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT"
	"VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT"
	"VK_ACCESS_SHADER_READ_BIT")
	string(FIND "${TRANSLATE_TEST_TEXT}" "${REQUIRED}" POSITION)
	if(POSITION EQUAL -1)
		message(FATAL_ERROR "color-attachment visibility lowering coverage lost: ${REQUIRED}")
	endif()
endforeach()
foreach(REQUIRED IN ITEMS
	"ralVk_TranslateTextureCopyAspect("
	"RAL_TEXTURE_ASPECT_DEPTH, combined, &out"
	"RAL_TEXTURE_ASPECT_STENCIL, combined, &out"
	"0u, VK_IMAGE_ASPECT_COLOR_BIT, &out"
	"RAL_TEXTURE_ASPECT_DEPTH | RAL_TEXTURE_ASPECT_STENCIL"
	"RAL_TEXTURE_ASPECT_COLOR, combined, &out"
	"1u << 7")
	string(FIND "${TRANSLATE_TEST_TEXT}" "${REQUIRED}" POSITION)
	if(POSITION EQUAL -1)
		message(FATAL_ERROR "buffer-texture aspect mutation coverage lost: ${REQUIRED}")
	endif()
endforeach()

foreach(REQUIRED IN ITEMS
	"RAL_TEXTURE_VIEW_ASPECT_ALL"
	"RAL_TEXTURE_VIEW_ASPECT_DEPTH_ONLY"
	"RAL_TEXTURE_VIEW_ASPECT_STENCIL_ONLY"
	"ralVk_TranslateTextureViewAspect"
	"Ral_PublishAdoptedTextureState"
	"ralVk_PublishAdoptedTextureResourceState"
	"texture->portableStateKnown = qtrue")
	string(FIND "${RESOURCE_HEADER_TEXT}${TRANSLATE_HEADER_TEXT}${TRANSLATE_TEXT}${BRIDGE_TEXT}${RESOURCE_TEXT}"
		"${REQUIRED}" POSITION)
	if(POSITION EQUAL -1)
		message(FATAL_ERROR "adopted depth resource/view-state contract lost: ${REQUIRED}")
	endif()
endforeach()
foreach(REQUIRED IN ITEMS
	"RAL_TEXTURE_VIEW_ASPECT_DEPTH_ONLY, combined"
	"out == VK_IMAGE_ASPECT_DEPTH_BIT"
	"ralVk_PublishAdoptedTextureResourceState"
	"texture.portableStateKnown"
	"texture.ownsImage = qtrue")
	string(FIND "${TRANSLATE_TEST_TEXT}" "${REQUIRED}" POSITION)
	if(POSITION EQUAL -1)
		message(FATAL_ERROR "adopted depth resource/view mutation coverage lost: ${REQUIRED}")
	endif()
endforeach()

foreach(REQUIRED IN ITEMS
	"ralVk_PublishAttachmentResourceState"
	"candidate.usage = RAL_RESOURCE_USAGE_COLOR_ATTACHMENT"
	"candidate.usage = RAL_RESOURCE_USAGE_DEPTH_STENCIL_WRITE"
	"command->queue != RAL_QUEUE_GRAPHICS"
	"texture->portableOwnerQueue = command->queue"
	"texture->portableStateKnown = qtrue")
	string(FIND "${TRANSLATE_HEADER_TEXT}${TRANSLATE_TEXT}" "${REQUIRED}" POSITION)
	if(POSITION EQUAL -1)
		message(FATAL_ERROR "attachment state publication lost exact seam: ${REQUIRED}")
	endif()
endforeach()
foreach(REQUIRED IN ITEMS
	"ralVk_PublishAttachmentResourceState( cb, tex, newLayout )"
	"ri_->resolveAttachments[i]"
	"texture.portableState.usage == RAL_RESOURCE_USAGE_COLOR_ATTACHMENT"
	"texture.portableState.usage == RAL_RESOURCE_USAGE_DEPTH_STENCIL_WRITE"
	"VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL")
	string(FIND "${LEGACY_COMMAND_TEXT}${TRANSLATE_TEST_TEXT}" "${REQUIRED}" POSITION)
	if(POSITION EQUAL -1)
		message(FATAL_ERROR "attachment state publication lost mutation coverage: ${REQUIRED}")
	endif()
endforeach()

extract_between("${PRODUCT_TEXT}" "void vk_tonemap( void )"
	"void vk_open_ui_pass" TONEMAP_TEXT)
foreach(REQUIRED IN ITEMS
	"Ral_EndRendering( vk.cmd->ral_cmd )"
	"textureTransition.before.usage = RAL_RESOURCE_USAGE_COLOR_ATTACHMENT"
	"textureTransition.after.usage = RAL_RESOURCE_USAGE_SAMPLED_TEXTURE"
	"textureTransition.after.shaderStages = RAL_STAGE_FRAGMENT"
	"textureTransition.sourceQueue = RAL_QUEUE_GRAPHICS"
	"textureTransition.destinationQueue = RAL_QUEUE_GRAPHICS"
	"Ral_CmdTransitionResources( vk.cmd->ral_cmd"
	"TERM_UNRECOVERABLE")
	string(FIND "${TONEMAP_TEXT}" "${REQUIRED}" POSITION)
	if(POSITION EQUAL -1)
		message(FATAL_ERROR "tonemap portable attachment transition lost: ${REQUIRED}")
	endif()
endforeach()
foreach(FORBIDDEN IN ITEMS
	"VkImageMemoryBarrier"
	"qvkCmdPipelineBarrier"
	"Ral_SetTextureLayout")
	string(FIND "${TONEMAP_TEXT}" "${FORBIDDEN}" POSITION)
	if(NOT POSITION EQUAL -1)
		message(FATAL_ERROR "tonemap regained raw/manual image transition: ${FORBIDDEN}")
	endif()
endforeach()

extract_between("${PRODUCT_TEXT}"
	"if ( vk.cmd->open_dynamic_pass == VK_DYN_PASS_MAIN ) {"
	"if ( vk.cmd->open_dynamic_pass == VK_DYN_PASS_TEMPORAL_POST_BLOOM ) {"
	MAIN_HANDOFF_TEXT)
foreach(REQUIRED IN ITEMS
	"textureTransition.before.usage = RAL_RESOURCE_USAGE_COLOR_ATTACHMENT"
	"textureTransition.after.usage = RAL_RESOURCE_USAGE_SAMPLED_TEXTURE"
	"Ral_CmdTransitionResources( vk.cmd->ral_cmd"
	"portable MAIN color handoff failed")
	string(FIND "${MAIN_HANDOFF_TEXT}" "${REQUIRED}" POSITION)
	if(POSITION EQUAL -1)
		message(FATAL_ERROR "MAIN portable attachment handoff lost: ${REQUIRED}")
	endif()
endforeach()

extract_between("${PRODUCT_TEXT}"
	"static qboolean vk_publish_color_attachment_handoff"
	"void vk_end_render_pass( void )" COLOR_ATTACHMENT_HANDOFF_HELPER_TEXT)
foreach(REQUIRED IN ITEMS
	"transition.aspects = RAL_TEXTURE_ASPECT_COLOR"
	"transition.mipLevelCount = Ral_GetTextureMipLevelCount( texture )"
	"transition.before.usage = RAL_RESOURCE_USAGE_COLOR_ATTACHMENT"
	"transition.after.usage = afterUsage"
	"transition.after.shaderStages = afterShaderStages"
	"transition.sourceQueue = RAL_QUEUE_GRAPHICS"
	"transition.destinationQueue = RAL_QUEUE_GRAPHICS"
	"Ral_CmdTransitionResources( vk.cmd->ral_cmd")
	string(FIND "${COLOR_ATTACHMENT_HANDOFF_HELPER_TEXT}" "${REQUIRED}" POSITION)
	if(POSITION EQUAL -1)
		message(FATAL_ERROR "dynamic color-attachment handoff helper lost: ${REQUIRED}")
	endif()
endforeach()

extract_between("${PRODUCT_TEXT}" "void vk_end_render_pass( void )"
	"static qboolean vk_find_screenmap_drawsurfs" DYNAMIC_PASS_HANDOFF_TEXT)
foreach(REQUIRED IN ITEMS
	"vk_publish_color_attachment_handoff( postRoute->attachment"
	"RAL_STAGE_FRAGMENT | RAL_STAGE_COMPUTE"
	"vk_publish_color_attachment_handoff( vk.ral_tonemapped_image"
	"vk_publish_color_attachment_handoff( vk.screenMap.ral_color_image"
	"vk_publish_color_attachment_handoff( vk.capture.ral_image"
	"RAL_RESOURCE_USAGE_COPY_SOURCE, 0u"
	"vk_publish_color_attachment_handoff( vk.ral_bloom_image[ dst ]"
	"temporal post-bloom portable handoff failed"
	"UI portable handoff failed"
	"screenmap portable handoff failed"
	"capture portable handoff failed"
	"bloom portable handoff failed")
	string(FIND "${DYNAMIC_PASS_HANDOFF_TEXT}" "${REQUIRED}" POSITION)
	if(POSITION EQUAL -1)
		message(FATAL_ERROR "dynamic-pass portable attachment handoff lost: ${REQUIRED}")
	endif()
endforeach()
string(REGEX MATCHALL "vk_publish_color_attachment_handoff[(]"
	DYNAMIC_HANDOFF_CALLS "${DYNAMIC_PASS_HANDOFF_TEXT}")
list(LENGTH DYNAMIC_HANDOFF_CALLS DYNAMIC_HANDOFF_COUNT)
if(NOT DYNAMIC_HANDOFF_COUNT EQUAL 5)
	message(FATAL_ERROR
		"dynamic-pass handoff call inventory changed: expected 5, got ${DYNAMIC_HANDOFF_COUNT}")
endif()
foreach(FORBIDDEN IN ITEMS "VkImageMemoryBarrier" "qvkCmdPipelineBarrier"
	"Ral_SetTextureLayout" "Ral_CmdTransitionTexture")
	string(FIND "${DYNAMIC_PASS_HANDOFF_TEXT}" "${FORBIDDEN}" POSITION)
	if(NOT POSITION EQUAL -1)
		message(FATAL_ERROR "dynamic-pass exit regained raw/native-shaped transition: ${FORBIDDEN}")
	endif()
endforeach()
foreach(REQUIRED IN ITEMS
	"RAL_RESOURCE_USAGE_COLOR_ATTACHMENT"
	"RAL_RESOURCE_USAGE_SAMPLED_TEXTURE"
	"RAL_RESOURCE_USAGE_COPY_SOURCE"
	"VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL"
	"VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL"
	"VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT"
	"VK_ACCESS_SHADER_READ_BIT"
	"VK_ACCESS_TRANSFER_READ_BIT")
	string(FIND "${TRANSITION_COMMAND_TEST_TEXT}" "${REQUIRED}" POSITION)
	if(POSITION EQUAL -1)
		message(FATAL_ERROR "dynamic-pass handoff mutation coverage lost: ${REQUIRED}")
	endif()
endforeach()

# Combined depth-stencil resources may be rendered through depth-only views,
# but their whole-resource layout authority still includes stencil.  Pin the
# format-derived distinction in the backend and the two shipping shadow exits.
foreach(REQUIRED IN ITEMS
	"static ralTextureAspectFlags_t ralVk_TextureResourceAspects"
	"VkImageAspectFlags aspects = texture->aspect"
	"texture->vkFormat == VK_FORMAT_D24_UNORM_S8_UINT"
	"aspects |= VK_IMAGE_ASPECT_STENCIL_BIT"
	"availableAspects = ralVk_TextureResourceAspects( texture )")
	string(FIND "${VULKAN_TEXT}" "${REQUIRED}" POSITION)
	if(POSITION EQUAL -1)
		message(FATAL_ERROR "depth resource/view aspect split lost: ${REQUIRED}")
	endif()
endforeach()

extract_between("${PRODUCT_TEXT}"
	"static qboolean vk_publish_shadow_sample_handoff"
	"void vk_end_render_pass( void )" SHADOW_HANDOFF_HELPER_TEXT)
foreach(REQUIRED IN ITEMS
	"arrayLayerCount == 0u"
	"transition.aspects = RAL_TEXTURE_ASPECT_DEPTH"
	"RAL_TEXTURE_ASPECT_STENCIL"
	"transition.arrayLayerCount = arrayLayerCount"
	"transition.before.usage = RAL_RESOURCE_USAGE_DEPTH_STENCIL_WRITE"
	"transition.after.usage = RAL_RESOURCE_USAGE_SAMPLED_TEXTURE"
	"transition.after.shaderStages = RAL_STAGE_FRAGMENT"
	"Ral_CmdTransitionResources( vk.cmd->ral_cmd")
	string(FIND "${SHADOW_HANDOFF_HELPER_TEXT}" "${REQUIRED}" POSITION)
	if(POSITION EQUAL -1)
		message(FATAL_ERROR "portable shadow handoff helper lost: ${REQUIRED}")
	endif()
endforeach()

extract_between("${PRODUCT_TEXT}"
	"// Post-loop hand-off: the lit pass samples the full cascade array."
	"// On exit no render pass is open." CSM_SAMPLE_HANDOFF_TEXT)
extract_between("${PRODUCT_TEXT}"
	"// Hand the complete depth-stencil resource to the lit fragment sample."
	"// r_dlightShadowProfile:" DLIGHT_SAMPLE_HANDOFF_TEXT)
foreach(REQUIRED IN ITEMS
	"vk_publish_shadow_sample_handoff( vk.shadowMap.ral_image"
	"SHADOWMAP_MAX_CASCADES"
	"CSM portable sample handoff failed")
	string(FIND "${CSM_SAMPLE_HANDOFF_TEXT}" "${REQUIRED}" POSITION)
	if(POSITION EQUAL -1)
		message(FATAL_ERROR "CSM portable sample handoff lost: ${REQUIRED}")
	endif()
endforeach()
foreach(REQUIRED IN ITEMS
	"vk_publish_shadow_sample_handoff( vk.dlightShadow.ral_image, 1u )"
	"dlight shadow portable sample handoff failed")
	string(FIND "${DLIGHT_SAMPLE_HANDOFF_TEXT}" "${REQUIRED}" POSITION)
	if(POSITION EQUAL -1)
		message(FATAL_ERROR "dlight portable sample handoff lost: ${REQUIRED}")
	endif()
endforeach()
string(REGEX MATCHALL "vk_publish_shadow_sample_handoff[(]"
	SHADOW_HANDOFF_CALLS "${CSM_SAMPLE_HANDOFF_TEXT}${DLIGHT_SAMPLE_HANDOFF_TEXT}")
list(LENGTH SHADOW_HANDOFF_CALLS SHADOW_HANDOFF_COUNT)
if(NOT SHADOW_HANDOFF_COUNT EQUAL 2)
	message(FATAL_ERROR
		"shadow sample handoff call inventory changed: expected 2, got ${SHADOW_HANDOFF_COUNT}")
endif()
foreach(SPAN_NAME IN ITEMS CSM_SAMPLE_HANDOFF_TEXT DLIGHT_SAMPLE_HANDOFF_TEXT)
	foreach(FORBIDDEN IN ITEMS "VkImageMemoryBarrier" "qvkCmdPipelineBarrier"
		"Ral_SetTextureLayout" "VK_IMAGE_LAYOUT" "VK_ACCESS" "VK_PIPELINE_STAGE")
		string(FIND "${${SPAN_NAME}}" "${FORBIDDEN}" POSITION)
		if(NOT POSITION EQUAL -1)
			message(FATAL_ERROR "${SPAN_NAME} regained raw shadow transition authority: ${FORBIDDEN}")
		endif()
	endforeach()
endforeach()
foreach(REQUIRED IN ITEMS
	"shadow.vkFormat = VK_FORMAT_D24_UNORM_S8_UINT"
	"shadow.aspect = VK_IMAGE_ASPECT_DEPTH_BIT"
	"transition.aspects = RAL_TEXTURE_ASPECT_DEPTH | RAL_TEXTURE_ASPECT_STENCIL"
	"capture.image.subresourceRange.layerCount == 4u"
	"shadow.aspect == VK_IMAGE_ASPECT_DEPTH_BIT"
	"transition.aspects = RAL_TEXTURE_ASPECT_DEPTH"
	"== ralErrorInvalidArgument")
	string(FIND "${TRANSITION_COMMAND_TEST_TEXT}" "${REQUIRED}" POSITION)
	if(POSITION EQUAL -1)
		message(FATAL_ERROR "depth resource/view transition mutation coverage lost: ${REQUIRED}")
	endif()
endforeach()

extract_between("${PRODUCT_TEXT}" "static void vk_scene_depth_copy_internal"
	"void vk_scene_depth_copy( void )" SCENE_DEPTH_COPY_TEXT)
extract_between("${PRODUCT_TEXT}" "void vk_forwardplus_depth_copy( void )"
	"void vk_smaa( void )" FORWARDPLUS_DEPTH_COPY_TEXT)
foreach(SPAN_NAME IN ITEMS MAIN_HANDOFF_TEXT SCENE_DEPTH_COPY_TEXT FORWARDPLUS_DEPTH_COPY_TEXT)
	foreach(FORBIDDEN IN ITEMS "VkImageMemoryBarrier" "qvkCmdPipelineBarrier"
		"qvkCmdCopyImage" "Ral_SetTextureLayout")
		string(FIND "${${SPAN_NAME}}" "${FORBIDDEN}" POSITION)
		if(NOT POSITION EQUAL -1)
			message(FATAL_ERROR "${SPAN_NAME} regained raw/manual image authority: ${FORBIDDEN}")
		endif()
	endforeach()
endforeach()
foreach(REQUIRED IN ITEMS
	"RAL_TEXTURE_ASPECT_DEPTH"
	"RAL_TEXTURE_ASPECT_STENCIL"
	"RAL_RESOURCE_USAGE_DEPTH_STENCIL_WRITE"
	"RAL_RESOURCE_USAGE_COPY_SOURCE"
	"RAL_RESOURCE_USAGE_COPY_DESTINATION"
	"RAL_RESOURCE_USAGE_SAMPLED_TEXTURE"
	"RAL_STAGE_FRAGMENT | RAL_STAGE_COMPUTE"
	"Ral_CmdTransitionResources( vk.cmd->ral_cmd"
	"Ral_CmdCopyImage( vk.cmd->ral_cmd"
	"Ral_TextureGetResourceState( vk.sceneDepth.ral_image"
	"Ral_PublishAdoptedTextureState( vk.sceneDepth.ral_image"
	"vk.gtaoSnapshotValid"
	"transitions[1].before = retainedDepthState"
	"TERM_UNRECOVERABLE")
	string(FIND "${SCENE_DEPTH_COPY_TEXT}${FORWARDPLUS_DEPTH_COPY_TEXT}" "${REQUIRED}" POSITION)
	if(POSITION EQUAL -1)
		message(FATAL_ERROR "portable depth-copy contract lost: ${REQUIRED}")
	endif()
endforeach()
string(FIND "${PRODUCT_TEXT}"
	"Ral_CmdTransitionTexture( vk.cmd->ral_cmd, vk.sceneDepth.ral_image"
	LEGACY_SCENE_DEPTH_TRANSITION)
if(NOT LEGACY_SCENE_DEPTH_TRANSITION EQUAL -1)
	message(FATAL_ERROR "sceneDepth regained native-shaped transition authority")
endif()

extract_between("${PRODUCT_TEXT}" "static qboolean vk_smaa_transition_copy_pair"
	"static qboolean vk_smaa_publish_attachment_sampled" SMAA_COPY_TRANSITION_TEXT)
extract_between("${PRODUCT_TEXT}" "static qboolean vk_smaa_publish_attachment_sampled"
	"void vk_smaa( void )" SMAA_ATTACHMENT_TRANSITION_TEXT)
extract_between("${PRODUCT_TEXT}" "void vk_smaa( void )"
	"void vk_begin_bloom_extract_render_pass" SMAA_TEXT)
foreach(REQUIRED IN ITEMS
	"RAL_RESOURCE_USAGE_SAMPLED_TEXTURE"
	"RAL_RESOURCE_USAGE_COPY_SOURCE"
	"RAL_RESOURCE_USAGE_COPY_DESTINATION"
	"RAL_STAGE_FRAGMENT"
	"Ral_CmdTransitionResources( vk.cmd->ral_cmd")
	string(FIND "${SMAA_COPY_TRANSITION_TEXT}" "${REQUIRED}" POSITION)
	if(POSITION EQUAL -1)
		message(FATAL_ERROR "SMAA portable copy transition lost: ${REQUIRED}")
	endif()
endforeach()
foreach(REQUIRED IN ITEMS
	"RAL_RESOURCE_USAGE_COLOR_ATTACHMENT"
	"RAL_RESOURCE_USAGE_SAMPLED_TEXTURE"
	"Ral_CmdTransitionResources( vk.cmd->ral_cmd")
	string(FIND "${SMAA_ATTACHMENT_TRANSITION_TEXT}" "${REQUIRED}" POSITION)
	if(POSITION EQUAL -1)
		message(FATAL_ERROR "SMAA portable attachment transition lost: ${REQUIRED}")
	endif()
endforeach()
foreach(REQUIRED IN ITEMS
	"vk_smaa_transition_copy_pair( qtrue )"
	"Ral_CmdCopyImage( vk.cmd->ral_cmd, vk.ral_tonemapped_image"
	"vk_smaa_transition_copy_pair( qfalse )"
	"vk_smaa_publish_attachment_sampled( vk.smaa.ral_edges_image )"
	"vk_smaa_publish_attachment_sampled( vk.smaa.ral_blend_image )"
	"vk_smaa_publish_attachment_sampled( vk.ral_tonemapped_image )"
	"TERM_UNRECOVERABLE")
	string(FIND "${SMAA_TEXT}" "${REQUIRED}" POSITION)
	if(POSITION EQUAL -1)
		message(FATAL_ERROR "SMAA portable command chain lost: ${REQUIRED}")
	endif()
endforeach()
foreach(REQUIRED IN ITEMS
	"Ral_CmdBindBindGroup( vk.cmd->ral_cmd, 0,\n\t\t\tvk.smaa.ral_input_descriptor )"
	"Ral_CmdBindBindGroup( vk.cmd->ral_cmd, 0,\n\t\t\tvk.smaa.ral_edges_descriptor )"
	"Ral_CmdBindBindGroup( vk.cmd->ral_cmd, 1,\n\t\t\tvk.smaa.ral_area_descriptor )"
	"Ral_CmdBindBindGroup( vk.cmd->ral_cmd, 2,\n\t\t\tvk.smaa.ral_search_descriptor )"
	"Ral_CmdBindBindGroup( vk.cmd->ral_cmd, 1,\n\t\t\tvk.smaa.ral_blend_descriptor )"
	"Ral_CmdBindBindGroup( vk.cmd->ral_cmd, 2,\n\t\t\tvk.smaa.ral_blend_descriptor )"
	"Ral_CmdBindBindGroup( vk.cmd->ral_cmd, 3,\n\t\t\tvk.smaaRt.ral_descriptor[ vk.cmd_index ] )")
	string(FIND "${SMAA_TEXT}" "${REQUIRED}" POSITION)
	if(POSITION EQUAL -1)
		message(FATAL_ERROR "SMAA retained bind-group command lost: ${REQUIRED}")
	endif()
endforeach()
string(REGEX MATCHALL "Ral_CmdBindBindGroup[(]" SMAA_BINDS "${SMAA_TEXT}")
list(LENGTH SMAA_BINDS SMAA_BIND_COUNT)
if(NOT SMAA_BIND_COUNT EQUAL 10)
	message(FATAL_ERROR "SMAA must issue exactly 10 retained bind-group commands, got ${SMAA_BIND_COUNT}")
endif()
foreach(FORBIDDEN IN ITEMS "VkImageMemoryBarrier" "qvkCmdPipelineBarrier"
	"qvkCmdCopyImage" "qvkCmdBindDescriptorSets" "Ral_SetTextureLayout")
	string(FIND "${SMAA_TEXT}" "${FORBIDDEN}" POSITION)
	if(NOT POSITION EQUAL -1)
		message(FATAL_ERROR "SMAA regained raw/manual image authority: ${FORBIDDEN}")
	endif()
endforeach()
foreach(REQUIRED IN ITEMS
	"struct ralBindGroupLayout_s *ral_bgl_smaa_rtmetrics"
	"struct ralBindGroup_s *ral_descriptor[NUM_COMMAND_BUFFERS]"
	"struct ralBindGroup_s *ral_area_descriptor"
	"struct ralBindGroup_s *ral_search_descriptor"
	"struct ralBindGroup_s *ral_edges_descriptor"
	"struct ralBindGroup_s *ral_blend_descriptor"
	"struct ralBindGroup_s *ral_input_descriptor")
	string(FIND "${PRODUCT_HEADER_TEXT}" "${REQUIRED}" POSITION)
	if(POSITION EQUAL -1)
		message(FATAL_ERROR "SMAA retained bind-group inventory lost: ${REQUIRED}")
	endif()
endforeach()
foreach(REQUIRED IN ITEMS
	"vk_ral_refresh_smaa_sampler_cohorts"
	"vk_ral_release_smaa_sampler_cohorts"
	"Ral_CreateBindGroup( backend, &createInfo )"
	"vk_ral_smaa_bindgroups_ready"
	"vk_ral_destroy_smaa_bindgroups"
	"void vk_ral_release_static_bindgroups( void )")
	string(FIND "${PRODUCT_TEXTURE_TEXT}${PRODUCT_TEXT}" "${REQUIRED}" POSITION)
	if(POSITION EQUAL -1)
		message(FATAL_ERROR "SMAA retained bind-group lifecycle lost: ${REQUIRED}")
	endif()
endforeach()
foreach(REQUIRED IN ITEMS
	"&vk.ral_bgl_smaa_rtmetrics, &vk.set_layout_smaa_rtmetrics,"
	"\"wired-set-layout-smaa-rtmetrics\" );"
	"vk_ral_release_static_bindgroups();\n\t\t{\n\t\t\tint sl6d;"
	"vk_ral_release_static_bindgroups();\n\tRal_DestroyBindGroupArena"
	"vk_ral_release_static_bindgroups();\n\t{\n\t\tint sl6d;"
	"Ral_DestroyBindGroupLayout( vk.ral_bgl_smaa_rtmetrics )")
	string(FIND "${PRODUCT_TEXT}" "${REQUIRED}" POSITION)
	if(POSITION EQUAL -1)
		message(FATAL_ERROR "SMAA descriptor-pool/layout ordering lost: ${REQUIRED}")
	endif()
endforeach()
string(FIND "${PRODUCT_TEXTURE_HEADER_TEXT}"
	"void     vk_ral_release_static_bindgroups( void )" POSITION)
if(POSITION EQUAL -1)
	message(FATAL_ERROR "descriptor-pool reset lost public retained-bindgroup release seam")
endif()

extract_between("${PRODUCT_TEXT}" "static void vk_replay_overlay_quads"
	"// Energy-compensation factor" OVERLAY_TEXT)
foreach(REQUIRED IN ITEMS
	"textureGroup = img ? img->ralDescriptor : NULL"
	"if ( !textureGroup )"
	"Ral_GetBindGroupHandle( textureGroup ) != (void *)img->descriptor"
	"Ral_CmdBindBindGroupDynamic( vk.cmd->ral_cmd, 0,"
	"textureGroup, NULL, 0 )")
	string(FIND "${OVERLAY_TEXT}" "${REQUIRED}" POSITION)
	if(POSITION EQUAL -1)
		message(FATAL_ERROR "overlay retained image bind-group command lost: ${REQUIRED}")
	endif()
endforeach()
string(REGEX MATCHALL "Ral_CmdBindBindGroupDynamic[(]" OVERLAY_BINDS "${OVERLAY_TEXT}")
list(LENGTH OVERLAY_BINDS OVERLAY_BIND_COUNT)
if(NOT OVERLAY_BIND_COUNT EQUAL 1)
	message(FATAL_ERROR "overlay must issue exactly one typed bind-group command, got ${OVERLAY_BIND_COUNT}")
endif()
string(FIND "${OVERLAY_TEXT}" "qvkCmdBindDescriptorSets" POSITION)
if(NOT POSITION EQUAL -1)
	message(FATAL_ERROR "overlay regained raw descriptor binding")
endif()

foreach(REQUIRED IN ITEMS
	"struct ralBindGroup_s *ralDescriptor"
	"struct ralTexture_s *ral"
	"struct ralTextureView_s *ralDescriptorView"
	"struct ralSampler_s *ralDescriptorSampler"
	"qboolean vk_ral_refresh_image_descriptor( image_t *image,"
	"void     vk_ral_release_image_descriptor( image_t *image )")
	string(FIND "${PRODUCT_LOCAL_TEXT}${PRODUCT_TEXTURE_HEADER_TEXT}" "${REQUIRED}" POSITION)
	if(POSITION EQUAL -1)
		message(FATAL_ERROR "per-image retained descriptor surface lost: ${REQUIRED}")
	endif()
endforeach()
extract_between("${PRODUCT_TEXTURE_TEXT}"
	"qboolean vk_ral_refresh_image_descriptor"
	"void vk_ral_release_image_descriptor" IMAGE_DESCRIPTOR_OWNER)
foreach(REQUIRED IN ITEMS
	"Ral_BindGroupArenaReceiptValid("
	"vk_ral_lookup_sampler( nativeSampler )"
	"Ral_TextureGetResourceReceipt( image->ral, &textureReceipt )"
	"viewCandidate = Ral_CreateTextureView( s_ral_backend, &viewInfo )"
	"value.type = RAL_BIND_COMBINED_TEXTURE_SAMPLER"
	"createInfo.arena = vk.ral_descriptor_arena"
	"createInfo.arenaReceipt = &vk.ral_descriptor_arena_receipt"
	"groupCandidate = Ral_CreateBindGroup( s_ral_backend, &createInfo )"
	"image->ralDescriptor = groupCandidate"
	"image->descriptor = rawCandidate"
	"groupCandidateOwned = qfalse"
	"viewCandidateOwned = qfalse"
	"if ( groupRetired ) Ral_DestroyBindGroup( groupRetired )"
	"if ( viewRetired ) Ral_DestroyTextureView( viewRetired )")
	string(FIND "${IMAGE_DESCRIPTOR_OWNER}" "${REQUIRED}" POSITION)
	if(POSITION EQUAL -1)
		message(FATAL_ERROR "per-image direct RAL descriptor lifecycle lost: ${REQUIRED}")
	endif()
endforeach()
foreach(RETIRED IN ITEMS Ral_AdoptBindGroup qvkAllocateDescriptorSets
	qvkUpdateDescriptorSets VkWriteDescriptorSet)
	string(FIND "${IMAGE_DESCRIPTOR_OWNER}" "${RETIRED}" POSITION)
	if(NOT POSITION EQUAL -1)
		message(FATAL_ERROR "per-image direct owner regained retired authority: ${RETIRED}")
	endif()
endforeach()
foreach(REQUIRED IN ITEMS
	"for ( i = 0; i < (uint32_t)tr.numImages; i++ )"
	"vk_ral_release_image_descriptor( image )"
	"vk_update_descriptor_set( image,")
	extract_between("${PRODUCT_TEXT}" "void vk_init_descriptors"
		"static void vk_release_geometry_buffers" IMAGE_ARENA_RESET)
	string(FIND "${IMAGE_ARENA_RESET}" "${REQUIRED}" POSITION)
	if(POSITION EQUAL -1)
		message(FATAL_ERROR "image descriptor pool-reset rebuild lost: ${REQUIRED}")
	endif()
endforeach()
foreach(RETIRED IN ITEMS imageAlloc "&image->descriptor"
	"vk_ral_adopt_image_descriptor")
	string(FIND "${IMAGE_ARENA_RESET}" "${RETIRED}" POSITION)
	if(NOT POSITION EQUAL -1)
		message(FATAL_ERROR "image arena rebuild regained raw/adopt authority: ${RETIRED}")
	endif()
endforeach()
foreach(REQUIRED IN ITEMS
	"image->ralDescriptor = NULL"
	"image->ralDescriptorView = NULL"
	"image->ralDescriptorSampler = NULL"
	"image->ral = NULL")
	string(FIND "${PRODUCT_IMAGE_TEXT}" "${REQUIRED}" POSITION)
	if(POSITION EQUAL -1)
		message(FATAL_ERROR "image descriptor init/teardown ordering lost: ${REQUIRED}")
	endif()
endforeach()
foreach(FIELD IN ITEMS ralDescriptor ralDescriptorView
	ralDescriptorSampler)
	string(REGEX MATCHALL "image->${FIELD} = NULL" IMAGE_DESCRIPTOR_INITS
		"${PRODUCT_IMAGE_TEXT}")
	list(LENGTH IMAGE_DESCRIPTOR_INITS IMAGE_DESCRIPTOR_INIT_COUNT)
	if(NOT IMAGE_DESCRIPTOR_INIT_COUNT EQUAL 3)
		message(FATAL_ERROR "all three Vulkan image constructors must initialize ${FIELD}, got ${IMAGE_DESCRIPTOR_INIT_COUNT}")
	endif()
endforeach()
string(REGEX MATCHALL "image->ral = NULL" IMAGE_TEXTURE_INITS
	"${PRODUCT_IMAGE_TEXT}")
list(LENGTH IMAGE_TEXTURE_INITS IMAGE_TEXTURE_INIT_COUNT)
if(NOT IMAGE_TEXTURE_INIT_COUNT EQUAL 1)
	message(FATAL_ERROR "canonical direct image texture initialization drifted: ${IMAGE_TEXTURE_INIT_COUNT}/1")
endif()
foreach(REQUIRED IN ITEMS
	"ralTextureView_t *Ral_AdoptTextureViewExact"
	"view->ownsView = qfalse"
	"view->ownsView        = qtrue"
	"if ( view->ownsView )")
	string(FIND "${BRIDGE_TEXT}${RESOURCE_TEXT}" "${REQUIRED}" POSITION)
	if(POSITION EQUAL -1)
		message(FATAL_ERROR "borrowed native image-view bridge lost: ${REQUIRED}")
	endif()
endforeach()
foreach(REQUIRED IN ITEMS
	"vk.smaa.ral_input_image = vk_smaa_create_texture("
	"vk.smaa.ral_edges_image = vk_smaa_create_texture("
	"vk.smaa.ral_blend_image = vk_smaa_create_texture("
	"RAL_TEXTURE_USAGE_TRANSFER_DST | RAL_TEXTURE_USAGE_SAMPLED")
	string(FIND "${PRODUCT_TEXTURE_TEXT}${PRODUCT_TEXT}" "${REQUIRED}" POSITION)
	if(POSITION EQUAL -1)
		message(FATAL_ERROR "SMAA direct texture ownership lost: ${REQUIRED}")
	endif()
endforeach()

foreach(REQUIRED IN ITEMS
	"ralResult_t Ral_CmdTransitionResources"
	"const ralResourceTransitionBatch_t *batch"
	"Ral_CmdReleaseBufferOwnership"
	"Ral_CmdAcquireBufferOwnership"
	"Ral_CancelBufferOwnershipTransfer"
	"Ral_CmdReleaseTextureOwnership"
	"Ral_CmdAcquireTextureOwnership"
	"Ral_CancelTextureOwnershipTransfer")
	string(FIND "${COMMAND_TEXT}" "${REQUIRED}" POSITION)
	if(POSITION EQUAL -1)
		message(FATAL_ERROR "portable RAL transition command lost required declaration: ${REQUIRED}")
	endif()
endforeach()

foreach(REQUIRED IN ITEMS
	"cb->renderingActive"
	"VK_QUEUE_FAMILY_IGNORED"
	"return ralUnsupported"
	"cb->backend->vk.CmdPipelineBarrier"
	"texture->portableState = batch->textureTransitions[i].after"
	"Ral_QueueTransferLifecycleRelease( &buffer->queueTransfer"
	"Ral_QueueTransferLifecycleAcquire( &buffer->queueTransfer"
	"Ral_QueueTransferLifecycleRelease( &texture->queueTransfer"
	"Ral_QueueTransferLifecycleAcquire( &texture->queueTransfer"
	"barrier.srcQueueFamilyIndex = cb->backend->queueFamily[transition->sourceQueue]"
	"barrier.dstQueueFamilyIndex = cb->backend->queueFamily[transition->destinationQueue]")
	string(FIND "${VULKAN_TEXT}" "${REQUIRED}" POSITION)
	if(POSITION EQUAL -1)
		message(FATAL_ERROR "Vulkan semantic transition command lost required seam: ${REQUIRED}")
	endif()
endforeach()

foreach(REQUIRED IN ITEMS
	"ralResourceUsage_t" "ralResourceState_t" "ralBufferTransition_t"
	"ralTextureTransition_t" "sourceQueue" "destinationQueue"
	"WebGPU validates encoder/pass usage and relies on implicit transitions"
	"ralQueueTransferReceipt_t" "ralQueueTransferLifecycle_t"
	"Ral_QueueTransferReceiptExact"
	"Ral_QueueTransferLifecycleRelease"
	"Ral_QueueTransferLifecycleAcquire"
	"Ral_QueueTransferLifecycleCancel")
	string(FIND "${TEXT}" "${REQUIRED}" POSITION)
	if(POSITION EQUAL -1)
		message(FATAL_ERROR "portable RAL transition contract lost required seam: ${REQUIRED}")
	endif()
endforeach()

foreach(REQUIRED IN ITEMS
	"!buffer->queueTransfer.pending.ready"
	"buffer->queueTransfer.pending.ready"
	"texture->queueTransfer.pending.ready")
	string(FIND "${VULKAN_TEXT}${RESOURCE_TEXT}${INTERNAL_TEXT}" "${REQUIRED}" POSITION)
	if(POSITION EQUAL -1)
		message(FATAL_ERROR "pending ownership transfer lost fail-closed seam: ${REQUIRED}")
	endif()
endforeach()

message(STATUS "portable RAL transition policy: PASS")
