# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026-present Wired Engine contributors

if(NOT DEFINED SOURCE_ROOT OR NOT IS_DIRECTORY "${SOURCE_ROOT}")
	message(FATAL_ERROR "SOURCE_ROOT must name the q3now source tree")
endif()

function(require_text body needle why)
	string(FIND "${body}" "${needle}" hit)
	if(hit EQUAL -1)
		message(FATAL_ERROR "RAL buffer-texture copy lost ${why}: ${needle}")
	endif()
endfunction()

function(forbid_regex body pattern why)
	string(REGEX MATCH "${pattern}" hit "${body}")
	if(hit)
		message(FATAL_ERROR "RAL buffer-texture copy leaked ${why}: ${hit}")
	endif()
endfunction()

function(require_count body pattern expected why)
	string(REGEX MATCHALL "${pattern}" hits "${body}")
	list(LENGTH hits count)
	if(NOT count EQUAL expected)
		message(FATAL_ERROR "RAL buffer-texture copy ${why}: got ${count}, want ${expected}")
	endif()
endfunction()

set(header_path "${SOURCE_ROOT}/code/render/ral/core/ral_command.h")
set(types_path "${SOURCE_ROOT}/code/render/ral/core/ral_types.h")
set(bridge_path "${SOURCE_ROOT}/code/render/ral/backends/vulkan/ral_vulkan_bridge.h")
set(command_path "${SOURCE_ROOT}/code/render/ral/backends/vulkan/ral_vulkan_command.c")
set(resource_path "${SOURCE_ROOT}/code/render/ral/backends/vulkan/ral_vulkan_resource.c")
set(translate_path "${SOURCE_ROOT}/code/render/ral/backends/vulkan/ral_vulkan_translate.c")
set(product_path "${SOURCE_ROOT}/code/render/ral/backends/vulkan/renderer/vk.c")
set(image_header_path "${SOURCE_ROOT}/code/render/ral/backends/vulkan/renderer/tr_local.h")
set(image_owner_path "${SOURCE_ROOT}/code/render/ral/backends/vulkan/renderer/vk_ral_textures.c")
set(image_lifecycle_path "${SOURCE_ROOT}/code/render/ral/backends/vulkan/renderer/tr_image.c")
set(host_path "${SOURCE_ROOT}/tests/ral_vulkan_texture_copy_test.c")
set(receipt_host_path "${SOURCE_ROOT}/tests/ral_texture_resource_receipt_test.c")
foreach(path IN ITEMS ${header_path} ${types_path} ${bridge_path} ${command_path}
		${resource_path} ${translate_path} ${product_path} ${image_header_path}
		${image_owner_path} ${image_lifecycle_path} ${host_path} ${receipt_host_path})
	if(NOT EXISTS "${path}")
		message(FATAL_ERROR "RAL buffer-texture copy input missing: ${path}")
	endif()
endforeach()
file(READ "${header_path}" header)
file(READ "${types_path}" types)
file(READ "${bridge_path}" bridge)
file(READ "${command_path}" command)
file(READ "${resource_path}" resource)
file(READ "${translate_path}" translate)
file(READ "${product_path}" product)
file(READ "${image_header_path}" image_header)
file(READ "${image_owner_path}" image_owner)
file(READ "${image_lifecycle_path}" image_lifecycle)
file(READ "${host_path}" host)
file(READ "${receipt_host_path}" receipt_host)

foreach(needle IN ITEMS
		"uint32_t  arrayLayerCount;"
		"uint32_t  imageZ;"
		"uint32_t  imageDepth;"
		"qboolean Ral_CmdCopyBufferToTextureRegionsExact( ralCommandBuffer_t *cb,"
		"uint32_t regionCount,"
		"const ralBufferTextureCopy_t *regions );")
	require_text("${header}" "${needle}" "portable public ABI")
endforeach()
forbid_regex("${header}" "ralBufferImageCopy_t|Ral_CmdCopyBufferToImage" "Vk-layout buffer-image API")

foreach(needle IN ITEMS
		"RAL_VK_BUFFER_TEXTURE_COPY_MAX_REGIONS 96u"
		"src->backend != cb->backend || dst->backend != cb->backend"
		"cb->state != RAL_VK_CMD_RECORDING"
		"cb->lifecycle.state != RAL_COMMAND_RECORDING"
		"cb->renderingActive"
		"!( src->usage & RAL_BUFFER_TRANSFER_SRC )"
		"!( dst->usage & RAL_TEXTURE_USAGE_TRANSFER_DST )"
		"src->portableState.usage != RAL_RESOURCE_USAGE_COPY_SOURCE"
		"dst->portableState.usage != RAL_RESOURCE_USAGE_COPY_DESTINATION"
		"dst->currentLayout != VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL"
		"dst->sampleCount != 1u"
		"ralVk_FormatCopyFootprint( dst->ralFormat"
		"region->arrayLayer != 0u || layerCount != 1u"
		"region->imageZ != 0u || depth != 1u"
		"region->bufferOffset % bytesPerBlock != 0u"
		"region->imageZ > INT32_MAX"
		"requiredBytes > (uint64_t)src->size - region->bufferOffset"
		"cb->backend->vk.CmdCopyBufferToImage( cb->cb, src->buffer, dst->image,"
		"VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, regionCount, nativeRegions );"
		"(void)Ral_CmdCopyBufferToTextureRegionsExact( cb, src, dst, 1u, region );")
	require_text("${command}" "${needle}" "validation/emission seam")
endforeach()
require_count("${command}" "CmdCopyBufferToImage[(] cb->cb" 1
	"must emit exactly one native call from the portable owner")

foreach(needle IN ITEMS
		"RAL_FORMAT_R8G8B8_UNORM"
		"RAL_FORMAT_B4G4R4A4_UNORM"
		"RAL_FORMAT_A1R5G5B5_UNORM")
	require_text("${types}" "${needle}" "exact legacy upload format ABI")
endforeach()
foreach(needle IN ITEMS
		"case RAL_FORMAT_R8G8B8_UNORM: return VK_FORMAT_R8G8B8_UNORM;"
		"case RAL_FORMAT_B4G4R4A4_UNORM: return VK_FORMAT_B4G4R4A4_UNORM_PACK16;"
		"case RAL_FORMAT_A1R5G5B5_UNORM: return VK_FORMAT_A1R5G5B5_UNORM_PACK16;")
	require_text("${translate}" "${needle}" "exact Vulkan format lowering")
endforeach()

foreach(needle IN ITEMS
		"qboolean Ral_PublishAdoptedBufferState( ralBuffer_t *buffer,"
		"ralTexture_t *Ral_AdoptTextureResourceExact( ralBackend_t *b,")
	require_text("${bridge}" "${needle}" "adopted-resource authority")
endforeach()
foreach(needle IN ITEMS
		"qboolean ralVk_FormatCopyFootprint( ralFormat_t format,"
		"case RAL_FORMAT_BC1_RGBA_UNORM:"
		"case RAL_FORMAT_BC7_SRGB:"
		"qboolean Ral_PublishAdoptedBufferState( ralBuffer_t *buffer,"
		"ralTexture_t *Ral_AdoptTextureResourceExact( ralBackend_t *b,"
		"tex->type            = ci->type;"
		"tex->mipLevels       = ci->mipLevels;"
		"tex->arrayLayers     = layers;")
	require_text("${resource}" "${needle}" "format/adoption implementation")
endforeach()

forbid_regex("${product}" "qvkCmdCopyBufferToImage|PFN_vkCmdCopyBufferToImage|VkBufferImageCopy"
	"renderer-local raw buffer-image command authority")
require_count("${product}" "vk_ral_copy_buffer_to_texture_exact[(]" 2
	"exact helper definition/call inventory drifted")
require_count("${product}" "vk_ral_stage_texture_copy[(]" 6
	"portable staging texture-copy inventory drifted")
foreach(needle IN ITEMS
		"static qboolean vk_ral_stage_texture_copy( ralTexture_t *destination,"
		"vk_ral_write_upload_buffer( vk.staging_buffer.ral_buffer,"
		"RAL_RESOURCE_USAGE_HOST_WRITE,"
		"RAL_RESOURCE_USAGE_COPY_SOURCE )"
		"Ral_CmdTransitionResources( command, &batch )"
		"Ral_CmdCopyBufferToTextureRegionsExact( command, source,"
		"destination, regionCount, regions )"
		"transition.before = copyState;"
		"transition.after = sampledState;"
		"uploadRecorded = vk_ral_stage_texture_copy("
		"vk_smaa_alloc_resources area LUT"
		"vk_smaa_alloc_resources search LUT"
		"portable compressed upload transaction rejected image"
		"portable 3D upload transaction rejected image"
		"SMAA LUT portable upload declined; disabling SMAA resources")
	require_text("${product}" "${needle}" "product transaction")
endforeach()
foreach(needle IN ITEMS
		"case VK_FORMAT_R8G8B8_UNORM:             return RAL_FORMAT_R8G8B8_UNORM;"
		"case VK_FORMAT_B4G4R4A4_UNORM_PACK16:    return RAL_FORMAT_B4G4R4A4_UNORM;"
		"case VK_FORMAT_A1R5G5B5_UNORM_PACK16:    return RAL_FORMAT_A1R5G5B5_UNORM;"
		"case VK_FORMAT_BC1_RGB_UNORM_BLOCK:       return RAL_FORMAT_BC1_RGB_UNORM;"
		"case VK_FORMAT_BC7_SRGB_BLOCK:            return RAL_FORMAT_BC7_SRGB;")
	require_text("${product}" "${needle}" "product upload format mapping")
endforeach()
require_text("${image_header}" "uint32_t\tmipLevelCount;" "exact image mip inventory")
require_text("${image_header}" "struct ralTexture_s *ral;"
	"sole image_t texture owner")
foreach(retired_image_field IN ITEMS
	"VkImage[ \t]+handle;" "VkImageView[ \t]+view;" "ralDescriptorTexture")
	forbid_regex("${image_header}" "${retired_image_field}"
		"retired image_t parallel/native field")
endforeach()
foreach(retired_image_owner IN ITEMS
	qvkAllocateMemory qvkBindImageMemory qvkCreateImage qvkCreateImageView
	qvkDestroyImage qvkDestroyImageView qvkFreeMemory
	PFN_vkAllocateMemory PFN_vkBindImageMemory PFN_vkCreateImage
	PFN_vkCreateImageView PFN_vkDestroyImage PFN_vkDestroyImageView
	PFN_vkFreeMemory image_chunks image_chunk_size ImageChunk)
	forbid_regex("${product}" "(^|[^A-Za-z0-9_])${retired_image_owner}([^A-Za-z0-9_]|$)"
		"retired renderer image allocation authority ${retired_image_owner}")
endforeach()
foreach(needle IN ITEMS
		"vk_ral_image_texture_info"
		"candidate.type = RAL_TEXTURE_3D;"
		"candidate.type = RAL_TEXTURE_2D_ARRAY;"
		"candidate.type = RAL_TEXTURE_CUBE;"
		"candidate.mipLevels = image->mipLevelCount;"
		"candidate.usage = RAL_TEXTURE_USAGE_SAMPLED"
		"candidate = Ral_CreateTexture( s_ral_backend, &createInfo );"
		"textureReceipt.imported")
	require_text("${image_owner}" "${needle}" "direct rich product texture ownership")
endforeach()
forbid_regex("${image_owner}" "Ral_AdoptTexture(ResourceExact|ViewExact)"
	"image_t create-then-adopt ownership")
foreach(needle IN ITEMS
		"vk_ral_create_image_texture_candidate( image, &candidate )"
		"Candidate-first replacement: the old descriptor/residency children"
		"vk_ral_release_image_descriptor( image );"
		"image->ral = candidate;"
		"vk_ral_release_image_descriptor( victim );"
		"vk_ral_unregister_image( victim );"
		"vk_ral_release_static_bindgroups();"
		"vk_ral_unregister_image( img );")
	require_text("${product}${image_lifecycle}" "${needle}"
		"candidate-first/child-before-parent image lifecycle")
endforeach()

foreach(needle IN ITEMS
		"Ral_CmdCopyBufferToTextureRegionsExact( &command, &buffer,"
		"texture.type = RAL_TEXTURE_2D_ARRAY;"
		"texture.ralFormat = RAL_FORMAT_BC1_RGBA_UNORM;"
		"texture.ralFormat = RAL_FORMAT_R8G8B8_UNORM;"
		"texture.type = RAL_TEXTURE_3D;"
		"REJECT_UPLOAD( badUpload.imageRect.x = -1 );"
		"REJECT_UPLOAD( badUpload.bufferOffset = 2u );"
		"REJECT_UPLOAD( badUpload.imageZ = 7u; badUpload.imageDepth = 2u );"
		"REJECT_UPLOAD( badUpload.imageZ = UINT32_MAX );")
	require_text("${host}" "${needle}" "host mutation coverage")
endforeach()
foreach(needle IN ITEMS
		"volumeInfo.format = RAL_FORMAT_BC7_SRGB;"
		"compressed->vkFormat == VK_FORMAT_BC7_SRGB_BLOCK"
		"volumeInfo.format = RAL_FORMAT_R8G8B8_UNORM;"
		"rgb->vkFormat == VK_FORMAT_R8G8B8_UNORM")
	require_text("${receipt_host}" "${needle}" "rich adoption host coverage")
endforeach()

message(STATUS "RAL portable multi-region buffer-texture copy policy: PASS")
