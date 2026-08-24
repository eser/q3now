// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
//
// ral_vulkan_resource.c — Vulkan backend: buffers, textures, samplers, bind
// groups (phase-7-ral-design.md §3.2-§3.5, §7). Current state:
//   * resources are real (VkBuffer / VkImage / VkImageView / VkSampler /
//     VkDescriptorSetLayout / VkDescriptorSet), backed by the suballocator;
//   * GPU mipmap generation via vkCmdBlitImage is a core Texture-upload
//     feature (Ral_TextureUploadAsync);
//   * uploads are SYNCHRONOUS (transient staging buffer + one-shot command +
//     fence wait) — they return an already-signaled fence. The async transfer
//     queue lands later;
//   * Ral_Destroy* does vkDeviceWaitIdle + immediate destroy — the deferred-
//     destroy queue (§7.2) lands later;
//   * descriptor sets come from one big backend-owned pool and are not freed
//     individually — Ral_DestroyBindGroup is a no-op for now.

#include "ral_vulkan_internal.h"

// Practical bindless-array size used for layouts whose entry count is 0
// (unbounded). The device cap (caps.maxBindlessTextures) can be far larger;
// this picks a portable fixed size. The descriptor pool is sized to match.
#define RAL_VK_BINDLESS_LAYOUT_COUNT  4096u
#define RAL_VK_BINDLESS_POOL_SETS     4u

// scratch sizes for Ral_CreateBindGroup
#define RAL_VK_MAX_BG_WRITES   64u
#define RAL_VK_MAX_BG_IMAGES   256u
#define RAL_VK_MAX_BG_BUFFERS  64u

static VkImageAspectFlags ralVk_FormatAspect( ralFormat_t f ) {
	switch ( f ) {
	case RAL_FORMAT_D16_UNORM:
	case RAL_FORMAT_X8_D24_UNORM:         // X8 padding is NOT a stencil aspect → depth-only
	case RAL_FORMAT_D32_SFLOAT:           return VK_IMAGE_ASPECT_DEPTH_BIT;
	case RAL_FORMAT_D24_UNORM_S8_UINT:
	case RAL_FORMAT_D16_UNORM_S8_UINT:
	case RAL_FORMAT_D32_SFLOAT_S8_UINT:   return VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
	default:                              return VK_IMAGE_ASPECT_COLOR_BIT;
	}
}

static qboolean ralVk_FormatIsDepthOrInteger( ralFormat_t f ) {
	switch ( f ) {
	case RAL_FORMAT_D16_UNORM: case RAL_FORMAT_D24_UNORM_S8_UINT:
	case RAL_FORMAT_D32_SFLOAT: case RAL_FORMAT_D32_SFLOAT_S8_UINT:
	case RAL_FORMAT_D16_UNORM_S8_UINT: case RAL_FORMAT_X8_D24_UNORM:
		return qtrue;
	default:
		return qfalse;
	}
}

uint32_t ralVk_FormatBPP( ralFormat_t f ) {   // uncompressed only; zero rejects compressed readback
	switch ( f ) {
	case RAL_FORMAT_R8_UNORM:                                  return 1;
	case RAL_FORMAT_R8G8_UNORM: case RAL_FORMAT_R16_UNORM: case RAL_FORMAT_R16_SFLOAT: case RAL_FORMAT_D16_UNORM:
	case RAL_FORMAT_B5G6R5_UNORM: case RAL_FORMAT_R5G6B5_UNORM:
	case RAL_FORMAT_B4G4R4A4_UNORM: case RAL_FORMAT_A1R5G5B5_UNORM:
		return 2;
	case RAL_FORMAT_R8G8B8_UNORM:                            return 3;
	case RAL_FORMAT_R8G8B8A8_UNORM: case RAL_FORMAT_R8G8B8A8_SRGB: case RAL_FORMAT_B8G8R8A8_UNORM:
	case RAL_FORMAT_B8G8R8A8_SRGB: case RAL_FORMAT_A2B10G10R10_UNORM: case RAL_FORMAT_A2R10G10B10_UNORM: case RAL_FORMAT_R16G16_SFLOAT:
	case RAL_FORMAT_R11G11B10_UFLOAT: case RAL_FORMAT_R32_SFLOAT: case RAL_FORMAT_D32_SFLOAT: case RAL_FORMAT_D24_UNORM_S8_UINT:
	case RAL_FORMAT_D16_UNORM_S8_UINT: case RAL_FORMAT_X8_D24_UNORM: case RAL_FORMAT_R8G8B8A8_UINT:
		return 4;
	case RAL_FORMAT_R16G16B16A16_SFLOAT: case RAL_FORMAT_R16G16B16A16_UNORM:
	case RAL_FORMAT_R32G32_SFLOAT: case RAL_FORMAT_D32_SFLOAT_S8_UINT: return 8;
	case RAL_FORMAT_R32G32B32_SFLOAT:                          return 12;   // vec3 — vertex-attribute-only on most hw (no optimal-tiling colour rendering)
	case RAL_FORMAT_R32G32B32A32_SFLOAT:                       return 16;
	default:                                                   return 0;
	}
}

qboolean ralVk_FormatCopyFootprint( ralFormat_t format,
		uint32_t *blockWidth, uint32_t *blockHeight,
		uint32_t *bytesPerBlock ) {
	uint32_t width = 1u, height = 1u, bytes = ralVk_FormatBPP( format );
	if ( !blockWidth || !blockHeight || !bytesPerBlock ) return qfalse;
	if ( bytes == 0u ) {
		width = height = 4u;
		switch ( format ) {
		case RAL_FORMAT_BC1_RGBA_UNORM:
		case RAL_FORMAT_BC1_RGBA_SRGB:
		case RAL_FORMAT_BC1_RGB_UNORM:
		case RAL_FORMAT_BC1_RGB_SRGB:
		case RAL_FORMAT_BC4_UNORM:
		case RAL_FORMAT_BC4_SNORM:
			bytes = 8u;
			break;
		case RAL_FORMAT_BC2_UNORM:
		case RAL_FORMAT_BC2_SRGB:
		case RAL_FORMAT_BC3_UNORM:
		case RAL_FORMAT_BC3_SRGB:
		case RAL_FORMAT_BC5_UNORM:
		case RAL_FORMAT_BC5_SNORM:
		case RAL_FORMAT_BC6H_UFLOAT:
		case RAL_FORMAT_BC6H_SFLOAT:
		case RAL_FORMAT_BC7_UNORM:
		case RAL_FORMAT_BC7_SRGB:
		case RAL_FORMAT_ASTC_4x4_UNORM:
		case RAL_FORMAT_ASTC_4x4_SRGB:
		case RAL_FORMAT_ETC2_R8G8B8A8_UNORM:
		case RAL_FORMAT_ETC2_R8G8B8A8_SRGB:
			bytes = 16u;
			break;
		default:
			return qfalse;
		}
	}
	*blockWidth = width;
	*blockHeight = height;
	*bytesPerBlock = bytes;
	return qtrue;
}

static VkBufferUsageFlags ralVk_BufferUsage( ralBufferUsage_t u ) {
	VkBufferUsageFlags v = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;  // every RAL buffer is up/down-loadable
	if ( u & RAL_BUFFER_VERTEX  )  v |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
	if ( u & RAL_BUFFER_INDEX   )  v |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
	if ( u & RAL_BUFFER_UNIFORM )  v |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
	if ( u & RAL_BUFFER_STORAGE )  v |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
	if ( u & RAL_BUFFER_INDIRECT)  v |= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
	return v;
}

static VkMemoryPropertyFlags ralVk_MemProps( ralMemoryType_t m ) {
	switch ( m ) {
	case RAL_MEMORY_HOST_VISIBLE:  return VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
	case RAL_MEMORY_HOST_COHERENT: return VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
	case RAL_MEMORY_LAZY_ALLOC:    return VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT;   // allocator falls back to DEVICE_LOCAL
	case RAL_MEMORY_DEVICE_LOCAL:
	default:                       return VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
	}
}

static VkImageType     ralVk_ImageType( ralTextureType_t t ) { return ( t == RAL_TEXTURE_1D ) ? VK_IMAGE_TYPE_1D : ( t == RAL_TEXTURE_3D ) ? VK_IMAGE_TYPE_3D : VK_IMAGE_TYPE_2D; }
static VkImageViewType ralVk_ViewType ( ralTextureType_t t ) {
	switch ( t ) {
	case RAL_TEXTURE_1D:         return VK_IMAGE_VIEW_TYPE_1D;
	case RAL_TEXTURE_3D:         return VK_IMAGE_VIEW_TYPE_3D;
	case RAL_TEXTURE_CUBE:       return VK_IMAGE_VIEW_TYPE_CUBE;
	case RAL_TEXTURE_2D_ARRAY:   return VK_IMAGE_VIEW_TYPE_2D_ARRAY;
	case RAL_TEXTURE_CUBE_ARRAY: return VK_IMAGE_VIEW_TYPE_CUBE_ARRAY;
	case RAL_TEXTURE_2D:
	default:                     return VK_IMAGE_VIEW_TYPE_2D;
	}
}

static VkFilter            ralVk_Filter  ( ralFilter_t f )      { return ( f == RAL_FILTER_LINEAR ) ? VK_FILTER_LINEAR : VK_FILTER_NEAREST; }
static VkSamplerMipmapMode ralVk_MipMode ( ralMipmapMode_t m )  { return ( m == RAL_MIPMAP_LINEAR ) ? VK_SAMPLER_MIPMAP_MODE_LINEAR : VK_SAMPLER_MIPMAP_MODE_NEAREST; }
static VkSamplerAddressMode ralVk_AddrMode( ralAddressMode_t a ) {
	switch ( a ) {
	case RAL_ADDRESS_MIRRORED_REPEAT: return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
	case RAL_ADDRESS_CLAMP_TO_EDGE:   return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	case RAL_ADDRESS_CLAMP_TO_BORDER: return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
	case RAL_ADDRESS_REPEAT:
	default:                          return VK_SAMPLER_ADDRESS_MODE_REPEAT;
	}
}
static VkCompareOp ralVk_CompareOp( ralCompareOp_t c ) {
	switch ( c ) {
	case RAL_COMPARE_LESS:          return VK_COMPARE_OP_LESS;
	case RAL_COMPARE_EQUAL:         return VK_COMPARE_OP_EQUAL;
	case RAL_COMPARE_LESS_EQUAL:    return VK_COMPARE_OP_LESS_OR_EQUAL;
	case RAL_COMPARE_GREATER:       return VK_COMPARE_OP_GREATER;
	case RAL_COMPARE_NOT_EQUAL:     return VK_COMPARE_OP_NOT_EQUAL;
	case RAL_COMPARE_GREATER_EQUAL: return VK_COMPARE_OP_GREATER_OR_EQUAL;
	case RAL_COMPARE_ALWAYS:        return VK_COMPARE_OP_ALWAYS;
	case RAL_COMPARE_NEVER:
	default:                        return VK_COMPARE_OP_NEVER;
	}
}
static VkBorderColor ralVk_BorderColor( ralBorderColor_t color ) {
	switch ( color ) {
	case RAL_BORDER_OPAQUE_BLACK: return VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
	case RAL_BORDER_OPAQUE_WHITE: return VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
	case RAL_BORDER_TRANSPARENT_BLACK:
	default:                      return VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
	}
}
static VkDescriptorType ralVk_DescType( ralBindType_t t ) {
	switch ( t ) {
	case RAL_BIND_UNIFORM_BUFFER:  return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	case RAL_BIND_STORAGE_BUFFER:  return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	case RAL_BIND_STORAGE_TEXTURE: return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	case RAL_BIND_SAMPLER:         return VK_DESCRIPTOR_TYPE_SAMPLER;
	case RAL_BIND_COMBINED_TEXTURE_SAMPLER: return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	case RAL_BIND_SAMPLED_TEXTURE:
	case RAL_BIND_TEXTURE_ARRAY:
	default:                       return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
	}
}
static VkDescriptorType ralVk_DescTypeForEntry( const ralBindEntry_t *entry ) {
	VkDescriptorType type;
	if ( !entry ) return VK_DESCRIPTOR_TYPE_MAX_ENUM;
	type = ralVk_DescType( entry->type );
	if ( !entry->dynamicOffset ) return type;
	if ( type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER )
		return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
	if ( type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER )
		return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
	return VK_DESCRIPTOR_TYPE_MAX_ENUM;
}

static qboolean ralVk_DynamicEntryValid( const ralBindEntry_t *entry ) {
	if ( !entry || ( entry->dynamicOffset != qfalse && entry->dynamicOffset != qtrue ) )
		return qfalse;
	if ( !entry->dynamicOffset ) return qtrue;
	return entry->count == 1u
		&& ( entry->type == RAL_BIND_UNIFORM_BUFFER
		  || entry->type == RAL_BIND_STORAGE_BUFFER );
}

static const ralVkBindEntry_t *ralVk_FindLayoutEntry(
	const ralBindGroupLayout_t *layout, uint32_t binding ) {
	uint32_t i;
	if ( !layout ) return NULL;
	for ( i = 0; i < layout->numEntries; ++i )
		if ( layout->entries[i].binding == binding ) return &layout->entries[i];
	return NULL;
}

static void ralVk_InitDynamicBindings( ralBindGroup_t *group,
	const ralBindGroupLayout_t *layout ) {
	uint32_t i, count = 0;
	if ( !group || !layout ) return;
	for ( i = 0; i < layout->numEntries; ++i ) {
		const ralVkBindEntry_t *entry = &layout->entries[i];
		uint32_t pos;
		if ( !entry->dynamicOffset || count >= RAL_VK_MAX_DYNAMIC_OFFSETS ) continue;
		pos = count;
		while ( pos > 0 && group->dynamicBindings[pos - 1].binding > entry->binding ) {
			group->dynamicBindings[pos] = group->dynamicBindings[pos - 1];
			--pos;
		}
		RAL_ZERO( group->dynamicBindings[pos] );
		group->dynamicBindings[pos].binding = entry->binding;
		group->dynamicBindings[pos].vkType = entry->vkType;
		++count;
	}
	group->dynamicOffsetCount = count;
}

static qboolean ralVk_RegisterDynamicBuffer( ralBindGroup_t *group,
	uint32_t binding, const ralBuffer_t *buffer, uint64_t baseOffset, uint64_t range ) {
	uint32_t i, tracked;
	if ( !group || !buffer || buffer->backend != group->backend || range == 0
	  || baseOffset > buffer->size || range > buffer->size - baseOffset ) return qfalse;
	for ( i = 0; i < group->dynamicOffsetCount; ++i ) {
		ralVkDynamicBufferBinding_t *dynamic = &group->dynamicBindings[i];
		if ( dynamic->binding != binding ) continue;
		if ( dynamic->registered ) return qfalse;
		for ( tracked = 0; tracked < group->bufferCount; ++tracked )
			if ( group->buffers[tracked] == buffer ) break;
		if ( tracked == group->bufferCount
		  && group->bufferCount >= RAL_VK_MAX_TRACKED_BIND_GROUP_BUFFERS ) return qfalse;
		dynamic->buffer = buffer;
		dynamic->baseOffset = baseOffset;
		dynamic->range = range;
		dynamic->registered = qtrue;
		if ( tracked == group->bufferCount ) {
			group->buffers[group->bufferCount++] = buffer;
		}
		return qtrue;
	}
	return qfalse;
}
static VkShaderStageFlags ralVk_StageFlags( uint32_t s ) {
	VkShaderStageFlags v = 0;
	if ( s & RAL_STAGE_VERTEX   )  v |= VK_SHADER_STAGE_VERTEX_BIT;
	if ( s & RAL_STAGE_FRAGMENT )  v |= VK_SHADER_STAGE_FRAGMENT_BIT;
	if ( s & RAL_STAGE_COMPUTE  )  v |= VK_SHADER_STAGE_COMPUTE_BIT;
	if ( v == 0 ) v = VK_SHADER_STAGE_ALL;
	return v;
}

static uint32_t ralVk_FullMipChain( uint32_t w, uint32_t h, uint32_t d ) {
	uint32_t m = ( w > h ) ? w : h; if ( d > m ) m = d;
	uint32_t levels = 1;
	while ( m > 1 ) { m >>= 1; levels++; }
	return levels;
}

void ralVk_SetObjectName( ralBackend_t *b, uint64_t handle, VkObjectType type, const char *name ) {
	VkDebugUtilsObjectNameInfoEXT info;
	if ( !b->haveDebugUtils || !b->vk.SetDebugUtilsObjectNameEXT || !name || !handle ) return;
	RAL_ZERO( info );
	info.sType        = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
	info.objectType   = type;
	info.objectHandle = handle;
	info.pObjectName  = name;
	b->vk.SetDebugUtilsObjectNameEXT( b->device, &info );
}

// ════════════════════════════════════════════════════════════════════════
// one-shot upload command helpers (per-queue, vkQueueSubmit2)
// ════════════════════════════════════════════════════════════════════════
static qboolean ralVk_BeginUploadCmd( ralBackend_t *b, ralQueueType_t q, VkCommandBuffer *out ) {
	VkCommandBufferAllocateInfo  ai;
	VkCommandBufferBeginInfo     bi;
	*out = VK_NULL_HANDLE;
	RAL_ZERO( ai );
	ai.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	ai.commandPool        = b->cmdPools[q];
	ai.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	ai.commandBufferCount = 1;
	ralVk_QueueLock( b, q );
	if ( b->vk.AllocateCommandBuffers( b->device, &ai, out ) != VK_SUCCESS ) *out = VK_NULL_HANDLE;
	ralVk_QueueUnlock( b, q );
	if ( *out == VK_NULL_HANDLE ) return qfalse;
	RAL_ZERO( bi );
	bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	if ( b->vk.BeginCommandBuffer( *out, &bi ) != VK_SUCCESS ) {
		ralVk_QueueLock( b, q ); b->vk.FreeCommandBuffers( b->device, b->cmdPools[q], 1, out ); ralVk_QueueUnlock( b, q );
		*out = VK_NULL_HANDLE;
		return qfalse;
	}
	return qtrue;
}

// Ends + submits `cb` on queue `q` with a fence, waits for completion, then
// frees `cb`. If `keepFence` is non-NULL the fence (now signaled) is written
// there and NOT destroyed — the caller owns it; otherwise it is destroyed.
static qboolean ralVk_SubmitUploadCmdAndWait( ralBackend_t *b, ralQueueType_t q, VkCommandBuffer cb, VkFence *keepFence ) {
	VkCommandBufferSubmitInfo cbi;
	VkSubmitInfo2             si2;
	VkFenceCreateInfo         fi;
	VkFence                   fence = VK_NULL_HANDLE;
	if ( b->vk.EndCommandBuffer( cb ) != VK_SUCCESS ) goto fail;
	RAL_ZERO( fi ); fi.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	if ( b->vk.CreateFence( b->device, &fi, NULL, &fence ) != VK_SUCCESS ) goto fail;
	RAL_ZERO( cbi ); cbi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO; cbi.commandBuffer = cb;
	RAL_ZERO( si2 ); si2.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2; si2.commandBufferInfoCount = 1; si2.pCommandBufferInfos = &cbi;
	if ( ralVk_QueueSubmit2( b, q, &si2, fence ) != ralSuccess ) goto fail;
	if ( b->vk.WaitForFences( b->device, 1, &fence, VK_TRUE, ~0ull ) != VK_SUCCESS ) goto fail;
	ralVk_QueueLock( b, q ); b->vk.FreeCommandBuffers( b->device, b->cmdPools[q], 1, &cb ); ralVk_QueueUnlock( b, q );
	if ( keepFence ) *keepFence = fence;
	else if ( fence != VK_NULL_HANDLE ) b->vk.DestroyFence( b->device, fence, NULL );
	return qtrue;
fail:
	if ( fence != VK_NULL_HANDLE ) b->vk.DestroyFence( b->device, fence, NULL );
	ralVk_QueueLock( b, q ); b->vk.FreeCommandBuffers( b->device, b->cmdPools[q], 1, &cb ); ralVk_QueueUnlock( b, q );
	if ( keepFence ) *keepFence = VK_NULL_HANDLE;
	return qfalse;
}

// Ends + submits `cb` on queue `q` with a fresh fence and returns WITHOUT waiting.
// The fence (in-flight) is written to *outFence for the caller to poll. The command
// buffer is registered for deferred free at the next frame boundary; by the time the
// frame-in-flight window has elapsed the submission has completed, so freeing it then
// is safe. Used by the async-transfer upload path (no blocking wait on submit).
static qboolean ralVk_SubmitUploadCmdNoWait( ralBackend_t *b, ralQueueType_t q, VkCommandBuffer cb,
	VkSemaphore signalSemaphore, VkFence *outFence ) {
	VkCommandBufferSubmitInfo cbi;
	VkSemaphoreSubmitInfo     signalInfo;
	VkSubmitInfo2             si2;
	VkFenceCreateInfo         fi;
	VkFence                   fence = VK_NULL_HANDLE;
	if ( outFence ) *outFence = VK_NULL_HANDLE;
	if ( b->vk.EndCommandBuffer( cb ) != VK_SUCCESS ) goto fail;
	RAL_ZERO( fi ); fi.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	if ( b->vk.CreateFence( b->device, &fi, NULL, &fence ) != VK_SUCCESS ) goto fail;
	RAL_ZERO( cbi ); cbi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO; cbi.commandBuffer = cb;
	RAL_ZERO( si2 ); si2.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2; si2.commandBufferInfoCount = 1; si2.pCommandBufferInfos = &cbi;
	if ( signalSemaphore != VK_NULL_HANDLE ) {
		RAL_ZERO( signalInfo );
		signalInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
		signalInfo.semaphore = signalSemaphore;
		signalInfo.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
		si2.signalSemaphoreInfoCount = 1;
		si2.pSignalSemaphoreInfos = &signalInfo;
	}
	if ( ralVk_QueueSubmit2( b, q, &si2, fence ) != ralSuccess ) goto fail;
	// Defer the command-buffer free to the frame-in-flight boundary; the submission
	// is complete well within that window, so this never frees an in-flight buffer.
	ralVk_DeferDestroy( b, RAL_RES_CMD_BUFFER, RAL_VK_H2U( cb ), (uint64_t)q, NULL );
	if ( outFence ) *outFence = fence;
	return qtrue;
fail:
	if ( fence != VK_NULL_HANDLE ) b->vk.DestroyFence( b->device, fence, NULL );
	ralVk_QueueLock( b, q ); b->vk.FreeCommandBuffers( b->device, b->cmdPools[q], 1, &cb ); ralVk_QueueUnlock( b, q );
	return qfalse;
}

// Wraps a (real) VkFence in a ralFence_t the caller owns. fence may be
// VK_NULL_HANDLE (fence creation failed) → reports signaled.
//
// Set ownsFence=qtrue. Both
// callers (Ral_BufferUploadAsync at line 403, Ral_TextureUploadAsync at
// line 719) pass a VkFence that was just created by
// ralVk_SubmitUploadCmdAndWait's CreateFence call + handed back via the
// keepFence out-param. Without ownsFence=qtrue, Ral_DestroyFence's
// ownership gate at sync.c:38 skipped the underlying VkFence destroy,
// leaking N fences per session (~15 textures × 1 fence each on arena1).
// Surfaced at vkDestroyDevice once fix3 unblocked the shutdown crash.
static ralFence_t *ralVk_WrapFence( ralBackend_t *b, VkFence fence ) {
	ralFence_t *f = (ralFence_t *)malloc( sizeof( *f ) );
	if ( !f ) {
		if ( fence != VK_NULL_HANDLE ) {
			(void)b->vk.WaitForFences( b->device, 1, &fence, VK_TRUE, ~0ull );
			b->vk.DestroyFence( b->device, fence, NULL );
		}
		return NULL;
	}
	RAL_ZERO( *f );
	f->backend     = b;
	f->fence       = fence;
	f->preSignaled = ( fence == VK_NULL_HANDLE ) ? qtrue : qfalse;
	f->ownsFence   = ( fence != VK_NULL_HANDLE ) ? qtrue : qfalse;
	return f;
}

// Choose the queue for an upload op: the dedicated transfer queue when
// available, else graphics. Used by the buffer-upload path; the texture path
// overrides this to graphics whenever it must run vkCmdBlitImage (mip-gen).
static ralQueueType_t ralVk_UploadQueue( ralBackend_t *b ) {
	return b->caps.asyncTransfer ? RAL_QUEUE_TRANSFER : RAL_QUEUE_GRAPHICS;
}

// Convenience image-barrier emitter (subresource: all aspect, [baseMip,levelCount) × [baseLayer,layerCount)).
static void ralVk_ImgBarrier( ralBackend_t *b, VkCommandBuffer cb, VkImage img, VkImageAspectFlags aspect,
                              uint32_t baseMip, uint32_t mipCount, uint32_t baseLayer, uint32_t layerCount,
                              VkImageLayout oldL, VkImageLayout newL,
                              VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage,
                              VkAccessFlags srcAccess, VkAccessFlags dstAccess ) {
	VkImageMemoryBarrier bar;
	RAL_ZERO( bar );
	bar.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	bar.srcAccessMask                   = srcAccess;
	bar.dstAccessMask                   = dstAccess;
	bar.oldLayout                       = oldL;
	bar.newLayout                       = newL;
	bar.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
	bar.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
	bar.image                           = img;
	bar.subresourceRange.aspectMask     = aspect;
	bar.subresourceRange.baseMipLevel   = baseMip;
	bar.subresourceRange.levelCount     = mipCount;
	bar.subresourceRange.baseArrayLayer = baseLayer;
	bar.subresourceRange.layerCount     = layerCount;
	b->vk.CmdPipelineBarrier( cb, srcStage, dstStage, 0, 0, NULL, 0, NULL, 1, &bar );
}

// ════════════════════════════════════════════════════════════════════════
// Buffer
// ════════════════════════════════════════════════════════════════════════
ralBuffer_t *Ral_CreateBuffer( ralBackend_t *b, const ralBufferCreateInfo_t *ci ) {
	VkBufferCreateInfo    bci;
	VkMemoryRequirements  req;
	ralBuffer_t          *buf;
	ralAllocationClass_t allocationClass;
	if ( !b || !ci || ci->size == 0 ) return NULL;

	buf = (ralBuffer_t *)malloc( sizeof( *buf ) );
	if ( !buf ) return NULL;
	RAL_ZERO( *buf );
	buf->header.refCount = 1;
	buf->backend         = b;
	buf->size            = ci->size;
	buf->memoryType      = ci->memory;
	buf->usage           = ci->usage;
	buf->portableStateKnown = qtrue;
	buf->portableState.usage = RAL_RESOURCE_USAGE_UNDEFINED;
	buf->portableOwnerQueue = RAL_QUEUE_GRAPHICS;
	Ral_QueueTransferLifecycleInit( &buf->queueTransfer );
	Ral_BufferMapLifecycleInit( &buf->mapLifecycle );

	RAL_ZERO( bci );
	bci.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bci.size        = ci->size;
	bci.usage       = ralVk_BufferUsage( ci->usage );
	bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	if ( b->vk.CreateBuffer( b->device, &bci, NULL, &buf->buffer ) != VK_SUCCESS ) {
		RAL_VK_LOG( SEV_WARN, "Ral_CreateBuffer: vkCreateBuffer failed (%llu bytes)\n", (unsigned long long)ci->size );
		free( buf ); return NULL;
	}
	b->vk.GetBufferMemoryRequirements( b->device, buf->buffer, &req );
	allocationClass = ci->memory == RAL_MEMORY_DEVICE_LOCAL ? RAL_ALLOCATION_DEVICE_LOCAL
		: ((ci->usage & RAL_BUFFER_MAP_READ) && (ci->usage & RAL_BUFFER_TRANSFER_DST))
			? RAL_ALLOCATION_READBACK : RAL_ALLOCATION_UPLOAD;
	buf->alloc = ralVk_Alloc( b, req, ralVk_MemProps( ci->memory ), allocationClass,
		RAL_ALLOCATION_RESIDENCY_PERMANENT, (uintptr_t)buf,
		RAL_VK_ALLOC_RESOURCE_BUFFER );
	if ( !buf->alloc ) { b->vk.DestroyBuffer( b->device, buf->buffer, NULL ); free( buf ); return NULL; }
	if ( b->vk.BindBufferMemory( b->device, buf->buffer, buf->alloc->memory,
			buf->alloc->offset ) != VK_SUCCESS ) {
		RAL_VK_LOG( SEV_WARN, "Ral_CreateBuffer: vkBindBufferMemory failed\n" );
		ralVk_Free( b, buf->alloc ); b->vk.DestroyBuffer( b->device, buf->buffer, NULL ); free( buf ); return NULL;
	}
	buf->hostVisible = ( buf->alloc->propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT  ) ? qtrue : qfalse;
	buf->coherent    = ( buf->alloc->propertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT ) ? qtrue : qfalse;
	buf->ownsBuffer  = qtrue;   // RAL created the VkBuffer; RAL destroys it.
	ralVk_SetObjectName( b, (uint64_t)buf->buffer, VK_OBJECT_TYPE_BUFFER, ci->debugName );
	return buf;
}

static ralBuffer_t *ralVk_AdoptBuffer( ralBackend_t *b, void *vkBuffer,
		uint64_t size, ralBufferUsage_t usage, ralMemoryType_t memory,
		const char *debugName ) {
	ralBuffer_t *buf;
	buf = (ralBuffer_t *)malloc( sizeof( *buf ) );
	if ( !buf ) return NULL;
	RAL_ZERO( *buf );
	buf->header.refCount = 1;
	buf->backend         = b;
	buf->buffer          = (VkBuffer)vkBuffer;
	buf->alloc           = NULL;          // engine-owned memory; RAL never frees it
	buf->size            = (VkDeviceSize)size;
	buf->usage           = usage;
	buf->memoryType      = memory;
	// An adopted wrapper has no RAL-owned allocation to map or flush. Retaining
	// the memory class is exact creation metadata, not mapping authority.
	buf->hostVisible     = qfalse;
	buf->coherent        = qfalse;
	buf->ownsBuffer      = qfalse;
	// The external producer owns the native state before adoption. A portable
	// transition may not guess it; a later explicit import-state API will make
	// adopted resources eligible without leaking native state upward.
	buf->portableStateKnown = qfalse;
	buf->portableOwnerQueue = RAL_QUEUE_GRAPHICS;
	Ral_QueueTransferLifecycleInit( &buf->queueTransfer );
	Ral_BufferMapLifecycleInit( &buf->mapLifecycle );
	if ( debugName ) {
		ralVk_SetObjectName( b, (uint64_t)buf->buffer, VK_OBJECT_TYPE_BUFFER, debugName );
	}
	return buf;
}

// Wrap an existing engine-owned VkBuffer without taking ownership. This
// compatibility helper intentionally publishes no portable usage metadata;
// new migration code should use Ral_AdoptBufferExact.
ralBuffer_t *Ral_AdoptBuffer( ralBackend_t *b, void *vkBuffer, size_t size,
		const char *debugName ) {
	if ( !b || vkBuffer == NULL || size == 0u ) {
		RAL_VK_LOG( SEV_WARN, "Ral_AdoptBuffer: bad args (b=%p, vkBuffer=%p, size=%llu)\n",
		        (void *)b, vkBuffer, (unsigned long long)size );
		return NULL;
	}
	return ralVk_AdoptBuffer( b, vkBuffer, size, (ralBufferUsage_t)0,
		RAL_MEMORY_DEVICE_LOCAL, debugName );
}

ralBuffer_t *Ral_AdoptBufferExact( ralBackend_t *b, void *vkBuffer,
		const ralBufferCreateInfo_t *ci ) {
	const uint32_t knownUsage = RAL_BUFFER_VERTEX | RAL_BUFFER_INDEX
		| RAL_BUFFER_UNIFORM | RAL_BUFFER_STORAGE | RAL_BUFFER_INDIRECT
		| RAL_BUFFER_TRANSFER_SRC | RAL_BUFFER_TRANSFER_DST
		| RAL_BUFFER_MAP_READ | RAL_BUFFER_MAP_WRITE;
	if ( !b || vkBuffer == NULL || !ci || ci->size == 0u
			|| ci->usage == 0 || ( (uint32_t)ci->usage & ~knownUsage ) != 0u
			|| ci->memory < RAL_MEMORY_DEVICE_LOCAL
			|| ci->memory > RAL_MEMORY_LAZY_ALLOC ) {
		RAL_VK_LOG( SEV_WARN, "Ral_AdoptBufferExact: invalid native buffer metadata\n" );
		return NULL;
	}
	return ralVk_AdoptBuffer( b, vkBuffer, ci->size, ci->usage, ci->memory,
		ci->debugName );
}

qboolean Ral_PublishAdoptedBufferState( ralBuffer_t *buffer,
		const ralResourceState_t *state, ralQueueType_t ownerQueue ) {
	if ( !buffer || !state || buffer->ownsBuffer || buffer->portableStateKnown
			|| buffer->queueTransfer.pending.ready || buffer->legacyMapped
			|| ownerQueue < RAL_QUEUE_GRAPHICS || ownerQueue > RAL_QUEUE_TRANSFER
			|| !Ral_BufferMapLifecycleGpuUseAllowed( &buffer->mapLifecycle )
			|| !Ral_ResourceStateValidForBuffer( state ) ) return qfalse;
	buffer->portableState = *state;
	buffer->portableOwnerQueue = ownerQueue;
	buffer->portableStateKnown = qtrue;
	return qtrue;
}

void *Ral_GetBufferHandle( const ralBuffer_t *buf ) {
	return buf ? (void *)buf->buffer : NULL;
}

uint64_t Ral_GetBufferSize( const ralBuffer_t *buf ) {
	return buf ? (uint64_t)buf->size : 0u;
}

ralBufferUsage_t Ral_GetBufferUsage( const ralBuffer_t *buf ) {
	return buf ? buf->usage : (ralBufferUsage_t)0;
}

ralMemoryType_t Ral_GetBufferMemoryType( const ralBuffer_t *buf ) {
	return buf ? buf->memoryType : RAL_MEMORY_DEVICE_LOCAL;
}

void Ral_DestroyBuffer( ralBuffer_t *buf ) {
	ralBackend_t *b;
	if ( !buf ) return;
	if ( buf->header.refCount > 1 ) { buf->header.refCount--; return; }
	b = buf->backend;
	if ( buf->immediateMapped ) {
		ralVk_Unmap( buf->alloc );
		buf->immediateMapped = NULL;
	}
	// Adopted buffers (ownsBuffer=qfalse) are engine-owned: free only the
	// wrapper, never the VkBuffer/memory.
	if ( buf->ownsBuffer ) {
		ralVk_DeferDestroy( b, RAL_RES_BUFFER, RAL_VK_H2U( buf->buffer ), 0, buf->alloc );
	}
	free( buf );
}

void *Ral_MapBuffer( ralBuffer_t *buf ) {
	ralBackend_t *b;
	void *mapped;
	if ( !buf ) return NULL;
	b = buf->backend;
	if ( !buf->hostVisible ) { RAL_VK_LOG( SEV_WARN, "Ral_MapBuffer: buffer is not host-visible\n" ); return NULL; }
	if ( buf->immediateMapped || buf->legacyMapped || buf->alloc->mapped ) return NULL;
	if ( !Ral_BufferMapLifecycleGpuUseAllowed( &buf->mapLifecycle ) ) return NULL;
	mapped = ralVk_Map( buf->alloc );
	// The legacy API does not say whether the mapping is read- or write-only.
	// Do not invent a HOST_READ/HOST_WRITE state: WebGPU requires that access
	// mode to be explicit. A future typed map/unmap surface can re-enter portable
	// tracking with the correct semantic state.
	if ( mapped ) {
		buf->portableStateKnown = qfalse;
		buf->legacyMapped = qtrue;
		buf->backend->gpuExcludedBufferMapCount++;
	}
	return mapped;
}
void Ral_UnmapBuffer( ralBuffer_t *buf ) {
	if ( buf && buf->legacyMapped ) {
		ralVk_Unmap( buf->alloc );
		buf->legacyMapped = qfalse;
		if ( buf->backend->gpuExcludedBufferMapCount > 0u )
			buf->backend->gpuExcludedBufferMapCount--;
	}
}
void Ral_FlushBuffer( ralBuffer_t *buf, uint64_t offset, uint64_t size ) {
	if ( !buf || buf->coherent ) return;   // coherent → no flush needed
	ralVk_Flush( buf->alloc, (VkDeviceSize)offset, (VkDeviceSize)size );
}

static qboolean ralVk_WriteStagingBuffer( ralBuffer_t *buffer,
	                                      const void *data, uint64_t size ) {
	ralBufferMapRequest_t request;
	ralBufferMapTicket_t ticket;
	if ( !buffer || !data || size == 0 ) return qfalse;
	RAL_ZERO( request );
	request.mode = RAL_MAP_WRITE;
	request.size = size;
	if ( Ral_BufferMapBegin( buffer, &request, &ticket ) != ralSuccess ) return qfalse;
	memcpy( ticket.mappedRange, data, (size_t)size );
	return Ral_BufferMapUnmap( buffer, &ticket ) == ralSuccess ? qtrue : qfalse;
}

ralFence_t *Ral_BufferUploadAsync( ralBuffer_t *buf, uint64_t offset, const void *data, uint64_t size ) {
	ralBackend_t        *b;
	ralBufferCreateInfo_t sci;
	ralBuffer_t          *staging;
	VkCommandBuffer       cb;
	VkBufferCopy          region;
	VkFence               fence = VK_NULL_HANDLE;
	ralQueueType_t        q;
	if ( !buf || !data || size == 0 ) return NULL;
	b = buf->backend;
	q = ralVk_UploadQueue( b );
	RAL_NOTE_ONCE( "Ral_BufferUploadAsync runs on the transfer queue when available but still waits internally before returning -- async streaming (no internal wait) is Phase 7.15\n" );

	RAL_ZERO( sci ); sci.size = size; sci.usage = RAL_BUFFER_TRANSFER_SRC | RAL_BUFFER_MAP_WRITE; sci.memory = RAL_MEMORY_HOST_COHERENT; sci.debugName = "ral-staging-buf-upload";
	staging = Ral_CreateBuffer( b, &sci );
	if ( !staging ) return NULL;
	if ( !ralVk_WriteStagingBuffer( staging, data, size ) ) { Ral_DestroyBuffer( staging ); return NULL; }

	if ( !ralVk_BeginUploadCmd( b, q, &cb ) ) { Ral_DestroyBuffer( staging ); return NULL; }
	RAL_ZERO( region ); region.srcOffset = 0; region.dstOffset = offset; region.size = size;
	b->vk.CmdCopyBuffer( cb, staging->buffer, buf->buffer, 1, &region );
	buf->portableStateKnown = qfalse;
	// Queue-ownership release: if this ran on a dedicated transfer family, hand
	// `buf` to the graphics family. The matching acquire is the consumer's job
	// (renderer migration, future) — until then nothing uses RAL buffers on
	// the graphics queue, so the unacquired release is harmless.
	if ( q == RAL_QUEUE_TRANSFER && b->queueFamily[RAL_QUEUE_TRANSFER] != b->queueFamily[RAL_QUEUE_GRAPHICS] ) {
		VkBufferMemoryBarrier bar;
		RAL_ZERO( bar );
		bar.sType               = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
		bar.srcAccessMask       = VK_ACCESS_TRANSFER_WRITE_BIT;
		bar.dstAccessMask       = 0;
		bar.srcQueueFamilyIndex = b->queueFamily[RAL_QUEUE_TRANSFER];
		bar.dstQueueFamilyIndex = b->queueFamily[RAL_QUEUE_GRAPHICS];
		bar.buffer              = buf->buffer;
		bar.offset              = 0;
		bar.size                = VK_WHOLE_SIZE;
		b->vk.CmdPipelineBarrier( cb, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, NULL, 1, &bar, 0, NULL );
	}
	if ( !ralVk_SubmitUploadCmdAndWait( b, q, cb, &fence ) ) { Ral_DestroyBuffer( staging ); return NULL; }
	Ral_DestroyBuffer( staging );
	return ralVk_WrapFence( b, fence );
}

// ════════════════════════════════════════════════════════════════════════
static qboolean ralVk_BufferTransferPrepared( ralBuffer_t *buffer,
		uint64_t offset, const void *data, uint64_t size,
		ralQueueType_t queue, uint64_t generation,
		ralTransferReceipt_t *out ) {
	ralAllocationReceipt_t allocation;
	ralTransferRequest_t request;
	if ( !buffer || !data || size == 0u || offset > buffer->size
			|| size > buffer->size - offset
			|| !( buffer->usage & RAL_BUFFER_TRANSFER_DST )
			|| !ralVk_BufferGpuUseAllowed( buffer )
			|| !Ral_BufferGetAllocationReceipt( buffer, &allocation ) ) return qfalse;
	RAL_ZERO( request );
	request.backendType = RAL_BACKEND_VULKAN;
	request.direction = RAL_TRANSFER_UPLOAD;
	request.resourceKind = RAL_TRANSFER_BUFFER;
	request.resourceIdentity = (uintptr_t)buffer;
	request.resourceGeneration = allocation.allocationGeneration;
	request.byteOffset = offset;
	request.byteSize = size;
	request.byteBudget = allocation.committedSize;
	request.queue = queue;
	return Ral_TransferPrepare( &request, generation, out );
}

static ralFence_t *ralVk_BufferUploadNoWait( ralBuffer_t *buffer,
		uint64_t offset, const void *data, uint64_t size,
		ralQueueType_t queue, ralSemaphore_t **outReadySemaphore ) {
	ralBackend_t *backend = buffer->backend;
	ralBufferCreateInfo_t stagingInfo;
	ralBuffer_t *staging;
	ralSemaphore_t *ready = NULL;
	VkCommandBuffer commandBuffer;
	VkBufferCopy copy;
	VkFence fence = VK_NULL_HANDLE;
	if ( outReadySemaphore ) *outReadySemaphore = NULL;
	RAL_ZERO( stagingInfo );
	stagingInfo.size = size;
	stagingInfo.usage = RAL_BUFFER_TRANSFER_SRC | RAL_BUFFER_MAP_WRITE;
	stagingInfo.memory = RAL_MEMORY_HOST_COHERENT;
	stagingInfo.debugName = "ral-staging-buffer-upload-ticket";
	staging = Ral_CreateBuffer( backend, &stagingInfo );
	if ( !staging ) return NULL;
	if ( queue == RAL_QUEUE_TRANSFER ) {
		ready = Ral_CreateSemaphore( backend, RAL_SEMAPHORE_BINARY );
		if ( !ready ) { Ral_DestroyBuffer( staging ); return NULL; }
	}
	if ( !ralVk_WriteStagingBuffer( staging, data, size )
			|| !ralVk_BeginUploadCmd( backend, queue, &commandBuffer ) ) {
		if ( ready ) Ral_DestroySemaphore( ready );
		Ral_DestroyBuffer( staging );
		return NULL;
	}
	RAL_ZERO( copy );
	copy.dstOffset = offset;
	copy.size = size;
	backend->vk.CmdCopyBuffer( commandBuffer, staging->buffer, buffer->buffer,
		1u, &copy );
	if ( queue == RAL_QUEUE_TRANSFER
			&& backend->queueFamily[RAL_QUEUE_TRANSFER]
				!= backend->queueFamily[RAL_QUEUE_GRAPHICS] ) {
		VkBufferMemoryBarrier release;
		RAL_ZERO( release );
		release.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
		release.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		release.srcQueueFamilyIndex = backend->queueFamily[RAL_QUEUE_TRANSFER];
		release.dstQueueFamilyIndex = backend->queueFamily[RAL_QUEUE_GRAPHICS];
		release.buffer = buffer->buffer;
		release.offset = offset;
		release.size = size;
		backend->vk.CmdPipelineBarrier( commandBuffer,
			VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
			0u, 0u, NULL, 1u, &release, 0u, NULL );
	} else if ( queue == RAL_QUEUE_GRAPHICS ) {
		VkBufferMemoryBarrier visible;
		RAL_ZERO( visible );
		visible.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
		visible.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		visible.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
		visible.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		visible.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		visible.buffer = buffer->buffer;
		visible.offset = offset;
		visible.size = size;
		backend->vk.CmdPipelineBarrier( commandBuffer,
			VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
			0u, 0u, NULL, 1u, &visible, 0u, NULL );
	}
	if ( !ralVk_SubmitUploadCmdNoWait( backend, queue, commandBuffer,
			ready ? ready->sem : VK_NULL_HANDLE, &fence ) ) {
		if ( ready ) Ral_DestroySemaphore( ready );
		Ral_DestroyBuffer( staging );
		return NULL;
	}
	buffer->portableStateKnown = qfalse;
	buffer->portableOwnerQueue = queue;
	Ral_DestroyBuffer( staging );
	if ( outReadySemaphore ) *outReadySemaphore = ready;
	return ralVk_WrapFence( backend, fence );
}

ralBufferUploadTicket_t Ral_BufferUploadBegin( ralBuffer_t *buffer,
		uint64_t offset, const void *data, uint64_t size ) {
	ralBufferUploadTicket_t ticket;
	ralTransferReceipt_t prepared, published;
	ralBackend_t *backend;
	ralQueueType_t queue;
	uint64_t generation;
	RAL_ZERO( ticket );
	if ( !buffer || !data || size == 0u ) return ticket;
	backend = buffer->backend;
	if ( !backend || backend->nextTransferGeneration >= UINT64_MAX - 1u ) return ticket;
	queue = ( backend->allowAsyncTextureUploads && backend->caps.asyncTransfer )
		? RAL_QUEUE_TRANSFER : RAL_QUEUE_GRAPHICS;
	generation = backend->nextTransferGeneration + 1u;
	if ( !ralVk_BufferTransferPrepared( buffer, offset, data, size, queue,
			generation, &prepared )
			|| !Ral_TransferPublish( &prepared, RAL_TRANSFER_OUTCOME_NATIVE_ASYNC,
				generation, &published ) ) return ticket;
	ticket.fence = ralVk_BufferUploadNoWait( buffer, offset, data, size, queue,
		&ticket.readySemaphore );
	if ( !ticket.fence ) {
		if ( ticket.readySemaphore ) Ral_DestroySemaphore( ticket.readySemaphore );
		RAL_ZERO( ticket );
		return ticket;
	}
	ticket.buffer = buffer;
	ticket.offset = offset;
	ticket.size = size;
	ticket.synchronous = qfalse;
	ticket.graphicsAcquireRequired = queue == RAL_QUEUE_TRANSFER ? qtrue : qfalse;
	ticket.graphicsAcquired = queue == RAL_QUEUE_GRAPHICS ? qtrue : qfalse;
	ticket.transfer = published;
	backend->nextTransferGeneration = generation;
	return ticket;
}

qboolean Ral_BufferWriteImmediate( ralBuffer_t *buffer, uint64_t offset,
		const void *data, uint64_t size,
		ralBufferUploadReceipt_t *outReceipt ) {
	ralBackend_t *backend;
	ralTransferReceipt_t prepared, completed;
	ralBufferUploadReceipt_t candidate;
	void *mapped;
	uint64_t generation;
	if ( !buffer || !data || !outReceipt || size == 0u
			|| !buffer->backend || !buffer->alloc || !buffer->hostVisible
			|| buffer->legacyMapped
			|| ( buffer->alloc->mapped && !buffer->immediateMapped )
			|| !buffer->portableStateKnown
			|| !ralVk_BufferGpuUseAllowed( buffer )
			|| !( buffer->usage & RAL_BUFFER_TRANSFER_DST ) ) return qfalse;
	backend = buffer->backend;
	if ( backend->nextTransferGeneration >= UINT64_MAX - 1u ) return qfalse;
	generation = backend->nextTransferGeneration + 1u;
	if ( !ralVk_BufferTransferPrepared( buffer, offset, data, size,
			RAL_QUEUE_GRAPHICS, generation, &prepared )
			|| !Ral_TransferPublish( &prepared,
				RAL_TRANSFER_OUTCOME_SYNCHRONOUS, generation, &completed )
			|| !Ral_BufferUploadReceiptBuild( &completed, generation,
				&candidate ) ) return qfalse;
	mapped = buffer->immediateMapped;
	if ( !mapped ) {
		mapped = ralVk_Map( buffer->alloc );
		if ( !mapped ) return qfalse;
		buffer->immediateMapped = mapped;
	}
	memcpy( (unsigned char *)mapped + offset, data, (size_t)size );
	ralVk_Flush( buffer->alloc, (VkDeviceSize)offset, (VkDeviceSize)size );
	buffer->portableStateKnown = qtrue;
	buffer->portableState.usage = RAL_RESOURCE_USAGE_HOST_WRITE;
	buffer->portableState.shaderStages = 0u;
	buffer->portableOwnerQueue = RAL_QUEUE_GRAPHICS;
	backend->nextTransferGeneration = generation;
	*outReceipt = candidate;
	return qtrue;
}

static qboolean ralVk_BufferUploadTicketValid(
		const ralBufferUploadTicket_t *ticket ) {
	ralAllocationReceipt_t allocation;
	const ralTransferRequest_t *request;
	if ( !ticket || !ticket->buffer || !ticket->fence
			|| ticket->synchronous != qfalse
			|| ( ticket->graphicsAcquireRequired != qfalse
				&& ticket->graphicsAcquireRequired != qtrue )
			|| ( ticket->graphicsAcquired != qfalse
				&& ticket->graphicsAcquired != qtrue )
			|| !ticket->buffer->backend
			|| ticket->fence->backend != ticket->buffer->backend
			|| !Ral_TransferReceiptExact( &ticket->transfer,
				&ticket->transfer )
			|| ( ticket->transfer.state != RAL_TRANSFER_SUBMITTED
				&& ticket->transfer.state != RAL_TRANSFER_COMPLETED )
			|| !Ral_BufferGetAllocationReceipt( ticket->buffer, &allocation ) ) {
		return qfalse;
	}
	request = &ticket->transfer.request;
	if ( request->direction != RAL_TRANSFER_UPLOAD
			|| request->resourceKind != RAL_TRANSFER_BUFFER
			|| request->resourceIdentity != (uintptr_t)ticket->buffer
			|| request->resourceGeneration != allocation.allocationGeneration
			|| request->byteOffset != ticket->offset
			|| request->byteSize != ticket->size
			|| request->byteBudget != allocation.committedSize
			|| ticket->size == 0u || ticket->offset > ticket->buffer->size
			|| ticket->size > ticket->buffer->size - ticket->offset ) {
		return qfalse;
	}
	if ( request->queue == RAL_QUEUE_GRAPHICS ) {
		return ticket->readySemaphore == NULL
			&& ticket->graphicsAcquireRequired == qfalse
			&& ticket->graphicsAcquired == qtrue;
	}
	return request->queue == RAL_QUEUE_TRANSFER
		&& ticket->readySemaphore
		&& ticket->readySemaphore->backend == ticket->buffer->backend
		&& ticket->graphicsAcquireRequired == qtrue;
}

qboolean Ral_BufferUploadTicketComplete( ralBufferUploadTicket_t *ticket ) {
	ralTransferReceipt_t completed;
	if ( !ralVk_BufferUploadTicketValid( ticket ) ) return qfalse;
	if ( ticket->transfer.state == RAL_TRANSFER_COMPLETED ) return qtrue;
	if ( ticket->transfer.state != RAL_TRANSFER_SUBMITTED
			|| !Ral_FenceSignaled( ticket->fence ) ) return qfalse;
	if ( !Ral_TransferComplete( &ticket->transfer,
			ticket->transfer.submissionGeneration, qtrue, &completed ) ) return qfalse;
	ticket->transfer = completed;
	return qtrue;
}

qboolean Ral_BufferUploadTicketGetReceipt(
	const ralBufferUploadTicket_t *ticket, ralBufferUploadReceipt_t *out ) {
	ralBufferUploadReceipt_t candidate;
	if ( !out || !ralVk_BufferUploadTicketValid( ticket )
			|| ticket->graphicsAcquired != qtrue
			|| ticket->transfer.state != RAL_TRANSFER_COMPLETED ) return qfalse;
	if ( !Ral_BufferUploadReceiptBuild( &ticket->transfer,
			ticket->transfer.completionGeneration, &candidate ) ) return qfalse;
	*out = candidate;
	return qtrue;
}

qboolean Ral_BufferAcquireBatchToGraphics( ralBackend_t *backend,
		ralBufferUploadTicket_t *tickets, uint32_t count ) {
	VkCommandBuffer commandBuffer;
	VkCommandBufferSubmitInfo commandInfo;
	VkSemaphoreSubmitInfo *waits;
	VkSubmitInfo2 submit;
	uint32_t i, waitCount = 0u;
	if ( !backend || !tickets || count == 0u
			|| count > RAL_BUFFER_UPLOAD_MAX_BATCH ) return qfalse;
	for ( i = 0u; i < count; i++ ) {
		const ralBufferUploadTicket_t *ticket = &tickets[i];
		if ( !ralVk_BufferUploadTicketValid( ticket )
				|| ticket->transfer.state != RAL_TRANSFER_COMPLETED ) return qfalse;
		if ( ticket->graphicsAcquireRequired == qfalse ) {
			if ( ticket->graphicsAcquired != qtrue ) return qfalse;
			continue;
		}
		if ( ticket->graphicsAcquireRequired != qtrue
				|| ticket->graphicsAcquired != qfalse ) return qfalse;
		if ( ticket->fence->backend != backend
				|| ticket->readySemaphore->backend != backend
				|| ticket->buffer->backend != backend
				|| ticket->transfer.request.queue != RAL_QUEUE_TRANSFER ) return qfalse;
		waitCount++;
	}
	if ( waitCount == 0u ) return qtrue;
	waits = (VkSemaphoreSubmitInfo *)calloc( waitCount, sizeof( *waits ) );
	if ( !waits ) return qfalse;
	if ( !ralVk_BeginUploadCmd( backend, RAL_QUEUE_GRAPHICS, &commandBuffer ) ) {
		free( waits );
		return qfalse;
	}
	waitCount = 0u;
	for ( i = 0u; i < count; i++ ) {
		const ralBufferUploadTicket_t *ticket = &tickets[i];
		VkBufferMemoryBarrier acquire;
		if ( !ticket->graphicsAcquireRequired ) continue;
		RAL_ZERO( waits[waitCount] );
		waits[waitCount].sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
		waits[waitCount].semaphore = ticket->readySemaphore->sem;
		waits[waitCount].stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
		waitCount++;
		RAL_ZERO( acquire );
		acquire.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
		acquire.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
		acquire.srcQueueFamilyIndex = backend->queueFamily[RAL_QUEUE_TRANSFER]
			== backend->queueFamily[RAL_QUEUE_GRAPHICS]
			? VK_QUEUE_FAMILY_IGNORED : backend->queueFamily[RAL_QUEUE_TRANSFER];
		acquire.dstQueueFamilyIndex = backend->queueFamily[RAL_QUEUE_TRANSFER]
			== backend->queueFamily[RAL_QUEUE_GRAPHICS]
			? VK_QUEUE_FAMILY_IGNORED : backend->queueFamily[RAL_QUEUE_GRAPHICS];
		acquire.buffer = ticket->buffer->buffer;
		acquire.offset = ticket->offset;
		acquire.size = ticket->size;
		backend->vk.CmdPipelineBarrier( commandBuffer,
			VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
			0u, 0u, NULL, 1u, &acquire, 0u, NULL );
	}
	if ( backend->vk.EndCommandBuffer( commandBuffer ) != VK_SUCCESS ) {
		free( waits );
		ralVk_QueueLock( backend, RAL_QUEUE_GRAPHICS );
		backend->vk.FreeCommandBuffers( backend->device,
			backend->cmdPools[RAL_QUEUE_GRAPHICS], 1u, &commandBuffer );
		ralVk_QueueUnlock( backend, RAL_QUEUE_GRAPHICS );
		return qfalse;
	}
	RAL_ZERO( commandInfo );
	commandInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
	commandInfo.commandBuffer = commandBuffer;
	RAL_ZERO( submit );
	submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
	submit.waitSemaphoreInfoCount = waitCount;
	submit.pWaitSemaphoreInfos = waits;
	submit.commandBufferInfoCount = 1u;
	submit.pCommandBufferInfos = &commandInfo;
	if ( ralVk_QueueSubmit2( backend, RAL_QUEUE_GRAPHICS, &submit,
			VK_NULL_HANDLE ) != ralSuccess ) {
		free( waits );
		ralVk_QueueLock( backend, RAL_QUEUE_GRAPHICS );
		backend->vk.FreeCommandBuffers( backend->device,
			backend->cmdPools[RAL_QUEUE_GRAPHICS], 1u, &commandBuffer );
		ralVk_QueueUnlock( backend, RAL_QUEUE_GRAPHICS );
		return qfalse;
	}
	ralVk_DeferDestroy( backend, RAL_RES_CMD_BUFFER,
		RAL_VK_H2U( commandBuffer ), (uint64_t)RAL_QUEUE_GRAPHICS, NULL );
	for ( i = 0u; i < count; i++ ) {
		if ( tickets[i].graphicsAcquireRequired != qtrue ) continue;
		tickets[i].buffer->portableOwnerQueue = RAL_QUEUE_GRAPHICS;
		tickets[i].buffer->portableStateKnown = qfalse;
		tickets[i].graphicsAcquired = qtrue;
	}
	free( waits );
	return qtrue;
}

// Texture
// ════════════════════════════════════════════════════════════════════════
ralTexture_t *Ral_CreateTexture( ralBackend_t *b, const ralTextureCreateInfo_t *ci ) {
	VkImageCreateInfo     ici;
	VkImageViewCreateInfo vci;
	VkMemoryRequirements  req;
	ralTexture_t         *tex;
	VkImageView          *layerViews = NULL;
	uint32_t              layers, depth3d, mips;
	uint32_t              qfamShare[3];   // concurrent-sharing family list (function-scoped for pQueueFamilyIndices validity)
	uint32_t              qfamShareCount;
	if ( !b || !ci || ci->width == 0 ) return NULL;

	switch ( ci->type ) {
	case RAL_TEXTURE_CUBE:       layers = 6;                              depth3d = 1; break;
	case RAL_TEXTURE_CUBE_ARRAY: layers = 6 * ( ci->depthOrArrayLayers ? ci->depthOrArrayLayers : 1 ); depth3d = 1; break;
	case RAL_TEXTURE_2D_ARRAY:   layers = ci->depthOrArrayLayers ? ci->depthOrArrayLayers : 1;          depth3d = 1; break;
	case RAL_TEXTURE_3D:         layers = 1;                              depth3d = ci->depthOrArrayLayers ? ci->depthOrArrayLayers : 1; break;
	default:                     layers = 1;                              depth3d = 1; break;
	}
	mips = ci->mipLevels ? ci->mipLevels : ralVk_FullMipChain( ci->width, ci->height ? ci->height : 1, depth3d );

	tex = (ralTexture_t *)malloc( sizeof( *tex ) );
	if ( !tex ) return NULL;
	RAL_ZERO( *tex );
	tex->header.refCount      = 1;
	tex->backend              = b;
	tex->vkFormat             = ralVk_TranslateFormat( ci->format );
	tex->ralFormat            = ci->format;
	tex->type                 = ci->type;
	tex->usage                = ci->usage;
	tex->width                = ci->width;
	tex->height               = ci->height ? ci->height : 1;
	tex->depthOrArrayLayers   = ci->depthOrArrayLayers ? ci->depthOrArrayLayers : 1;
	tex->mipLevels            = mips;
	tex->arrayLayers          = layers;
	tex->sampleCount          = ci->sampleCount ? ci->sampleCount : 1;
	tex->aspect               = ralVk_FormatAspect( ci->format );
	tex->currentLayout        = VK_IMAGE_LAYOUT_UNDEFINED;
	tex->ownsImage            = qtrue;   // native RAL allocation owns the VkImage; Ral_AdoptTexture flips this to qfalse for adopted handles.
	tex->portableStateKnown   = qtrue;
	tex->portableState.usage  = RAL_RESOURCE_USAGE_UNDEFINED;
	tex->portableOwnerQueue   = RAL_QUEUE_GRAPHICS;
	Ral_QueueTransferLifecycleInit( &tex->queueTransfer );
	if ( tex->vkFormat == VK_FORMAT_UNDEFINED ) {
		RAL_VK_LOG( SEV_WARN, "Ral_CreateTexture: unsupported ralFormat %d\n", (int)ci->format );
		free( tex ); return NULL;
	}

	RAL_ZERO( ici );
	ici.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	if ( ci->type == RAL_TEXTURE_CUBE || ci->type == RAL_TEXTURE_CUBE_ARRAY ) ici.flags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
	ici.imageType     = ralVk_ImageType( ci->type );
	ici.format        = tex->vkFormat;
	ici.extent.width  = tex->width;
	ici.extent.height = tex->height;
	ici.extent.depth  = depth3d;
	ici.mipLevels     = mips;
	ici.arrayLayers   = layers;
	ici.samples       = (VkSampleCountFlagBits)( tex->sampleCount );
	ici.tiling        = VK_IMAGE_TILING_OPTIMAL;
	ici.usage         = ralVk_TextureUsage( ci->usage );
	ici.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
	// Concurrent sharing lets a resource written on one queue family and read on
	// another use only a semaphore for execution ordering — no per-frame queue-
	// family-ownership transfer. concurrentGraphicsCompute adds the compute family
	// (an async-compute pass writes, graphics reads); concurrentGraphicsTransfer adds
	// the transfer family (an upload copy submits on transfer, graphics samples). Each
	// family is only added when it actually differs from graphics; CONCURRENT with a
	// single family is a spec violation, so a degenerate list stays EXCLUSIVE. qfamShare
	// is function-scoped so pQueueFamilyIndices stays valid through CreateImage.
	qfamShareCount = 0;
	if ( ( ci->concurrentGraphicsCompute  && b->computeFamily  != b->graphicsFamily ) ||
	     ( ci->concurrentGraphicsTransfer && b->transferFamily != b->graphicsFamily ) ) {
		qfamShare[ qfamShareCount++ ] = b->graphicsFamily;
		if ( ci->concurrentGraphicsCompute && b->computeFamily != b->graphicsFamily )
			qfamShare[ qfamShareCount++ ] = b->computeFamily;
		if ( ci->concurrentGraphicsTransfer && b->transferFamily != b->graphicsFamily && b->transferFamily != b->computeFamily )
			qfamShare[ qfamShareCount++ ] = b->transferFamily;
	}
	if ( qfamShareCount >= 2 ) {
		ici.sharingMode           = VK_SHARING_MODE_CONCURRENT;
		ici.queueFamilyIndexCount = qfamShareCount;
		ici.pQueueFamilyIndices   = qfamShare;
	}
	// The transfer family is in the concurrent set → an upload copy on the transfer
	// queue can hand the image to graphics without an ownership-transfer barrier.
	tex->concurrentTransfer = ( ci->concurrentGraphicsTransfer && b->transferFamily != b->graphicsFamily && ici.sharingMode == VK_SHARING_MODE_CONCURRENT ) ? qtrue : qfalse;
	ici.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	if ( b->vk.CreateImage( b->device, &ici, NULL, &tex->image ) != VK_SUCCESS ) {
		RAL_VK_LOG( SEV_WARN, "Ral_CreateTexture: vkCreateImage failed (%ux%u, %u mips, fmt %d)\n", tex->width, tex->height, mips, (int)ci->format );
		free( tex ); return NULL;
	}
	b->vk.GetImageMemoryRequirements( b->device, tex->image, &req );
	tex->alloc = ralVk_Alloc( b, req, ralVk_MemProps( ci->memory ),
		ci->memory == RAL_MEMORY_LAZY_ALLOC ? RAL_ALLOCATION_TRANSIENT : RAL_ALLOCATION_DEVICE_LOCAL,
		ci->memory == RAL_MEMORY_LAZY_ALLOC ? RAL_ALLOCATION_RESIDENCY_TRANSIENT
			: RAL_ALLOCATION_RESIDENCY_PERMANENT, (uintptr_t)tex,
		RAL_VK_ALLOC_RESOURCE_IMAGE );
	if ( !tex->alloc ) { b->vk.DestroyImage( b->device, tex->image, NULL ); free( tex ); return NULL; }
	tex->resourceGeneration = tex->alloc->receipt.allocationGeneration;
	if ( b->vk.BindImageMemory( b->device, tex->image, tex->alloc->memory,
			tex->alloc->offset ) != VK_SUCCESS ) {
		RAL_VK_LOG( SEV_WARN, "Ral_CreateTexture: vkBindImageMemory failed\n" );
		ralVk_Free( b, tex->alloc ); b->vk.DestroyImage( b->device, tex->image, NULL ); free( tex ); return NULL;
	}

	RAL_ZERO( vci );
	vci.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	vci.image                           = tex->image;
	vci.viewType                        = ralVk_ViewType( ci->type );
	vci.format                          = tex->vkFormat;
	vci.subresourceRange.aspectMask     = tex->aspect;
	vci.subresourceRange.baseMipLevel   = 0;
	vci.subresourceRange.levelCount      = mips;
	vci.subresourceRange.baseArrayLayer = 0;
	vci.subresourceRange.layerCount     = layers;
	if ( b->vk.CreateImageView( b->device, &vci, NULL, &tex->defaultView ) != VK_SUCCESS ) {
		RAL_VK_LOG( SEV_WARN, "Ral_CreateTexture: vkCreateImageView failed\n" );
		ralVk_Free( b, tex->alloc ); b->vk.DestroyImage( b->device, tex->image, NULL ); free( tex ); return NULL;
	}
	// Dynamic rendering selects an array slice through the attachment view, not
	// through VkRenderingInfo. Direct 2D-array attachment textures therefore own
	// one single-layer 2D view per slice, matching adopted-array semantics.
	if ( ci->type == RAL_TEXTURE_2D_ARRAY && layers > 1u
			&& ( ci->usage & ( RAL_TEXTURE_USAGE_COLOR_ATTACHMENT
				| RAL_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT ) ) ) {
		uint32_t i;
		layerViews = (VkImageView *)calloc( layers, sizeof( *layerViews ) );
		if ( !layerViews ) goto layer_view_fail;
		vci.viewType = VK_IMAGE_VIEW_TYPE_2D;
		vci.subresourceRange.levelCount = 1u;
		vci.subresourceRange.layerCount = 1u;
		for ( i = 0u; i < layers; ++i ) {
			vci.subresourceRange.baseArrayLayer = i;
			if ( b->vk.CreateImageView( b->device, &vci, NULL,
					&layerViews[i] ) != VK_SUCCESS ) goto layer_view_fail;
		}
		tex->layerViews = layerViews;
		tex->numLayerViews = layers;
	}
	if ( ci->debugName ) {
		ralVk_SetObjectName( b, (uint64_t)tex->image,       VK_OBJECT_TYPE_IMAGE,      ci->debugName );
		ralVk_SetObjectName( b, (uint64_t)tex->defaultView, VK_OBJECT_TYPE_IMAGE_VIEW, ci->debugName );
	}
	return tex;

layer_view_fail:
	if ( layerViews ) {
		uint32_t i;
		for ( i = 0u; i < layers; ++i )
			if ( layerViews[i] )
				b->vk.DestroyImageView( b->device, layerViews[i], NULL );
		free( layerViews );
	}
	b->vk.DestroyImageView( b->device, tex->defaultView, NULL );
	ralVk_Free( b, tex->alloc );
	b->vk.DestroyImage( b->device, tex->image, NULL );
	free( tex );
	return NULL;
}

void Ral_DestroyTexture( ralTexture_t *tex ) {
	ralBackend_t *b;
	if ( !tex ) return;
	// Aliased images and their shared allocation are one cohort-owned lifetime.
	// Releasing an individual wrapper would leave the cohort with a dangling
	// child and could retire shared memory before its sibling images.
	if ( tex->transientCohortOwned ) return;
	if ( tex->header.refCount > 1 ) { tex->header.refCount--; return; }
	b = tex->backend;
	// ownsImage=qfalse on Ral_AdoptTexture-created wrappers
	// (the renderer's existing qvkCreateImage/qvkAllocateMemory/qvkCreateImageView
	// retain lifetime ownership of the underlying VkImage/VkImageView/VkDeviceMemory).
	// Free only the wrapper struct; defer-destroy of the backend objects belongs
	// to the caller's existing teardown path.
	if ( tex->ownsImage ) {
		if ( tex->layerViews ) {
			uint32_t i;
			for ( i = 0u; i < tex->numLayerViews; ++i )
				if ( tex->layerViews[i] )
					ralVk_DeferDestroy( b, RAL_RES_IMAGE_VIEW,
						RAL_VK_H2U( tex->layerViews[i] ), 0u, NULL );
			free( (void *)tex->layerViews );
		}
		ralVk_DeferDestroy( b, RAL_RES_IMAGE_AND_VIEW, RAL_VK_H2U( tex->image ), RAL_VK_H2U( tex->defaultView ), tex->alloc );
	}
	free( tex );
}

// Adoption helper. Wraps an existing VkImage + its VkImageView + format in a
// ralTexture_t with ownsImage=qfalse. `aspect` is the caller's
// VkImageAspectFlags bitmask (VK_IMAGE_ASPECT_COLOR_BIT / _DEPTH_BIT, …) stored
// verbatim on tex->aspect so ralImageMemoryBarrier_t builders can echo it.
// `externalView` (a VkImageView; NULL if the caller has no view) becomes the
// wrapper's defaultView, so the adopted texture can serve as a dynamic-rendering
// attachment (Ral_BeginRendering reads tex->defaultView). `fmt` records the
// texture's format on tex->vkFormat / tex->ralFormat so pipelines + views built
// from the wrapper carry the right format. currentLayout stays UNDEFINED — it is
// runtime state, re-synced by the consumer via Ral_SetTextureLayout.
ralTexture_t *Ral_AdoptTextureResourceExact( ralBackend_t *b,
		void *externalImage, void *externalView, uint32_t aspect,
		const ralTextureCreateInfo_t *ci ) {
	ralTexture_t *tex;
	uint64_t generation;
	uint32_t layers, depth, fullMips;
	VkFormat vkFormat;
	if ( !b || !externalImage || !ci || ci->width == 0u || ci->height == 0u
			|| ci->mipLevels == 0u || ci->sampleCount == 0u || ci->usage == 0u
			|| ci->format <= RAL_FORMAT_UNDEFINED || ci->format >= RAL_FORMAT_COUNT
			|| ci->type < RAL_TEXTURE_1D || ci->type > RAL_TEXTURE_CUBE_ARRAY
			|| b->nextTextureGeneration >= UINT64_MAX - 1u ) return NULL;
	vkFormat = ralVk_TranslateFormat( ci->format );
	if ( vkFormat == VK_FORMAT_UNDEFINED
			|| aspect == 0u || (VkImageAspectFlags)aspect != ralVk_FormatAspect( ci->format ) )
		return NULL;
	switch ( ci->type ) {
	case RAL_TEXTURE_1D:
		if ( ci->height != 1u || ci->depthOrArrayLayers > 1u ) return NULL;
		layers = 1u; depth = 1u;
		break;
	case RAL_TEXTURE_2D:
		if ( ci->depthOrArrayLayers > 1u ) return NULL;
		layers = 1u; depth = 1u;
		break;
	case RAL_TEXTURE_3D:
		depth = ci->depthOrArrayLayers;
		if ( depth == 0u ) return NULL;
		layers = 1u;
		break;
	case RAL_TEXTURE_CUBE:
		if ( ci->width != ci->height || ci->depthOrArrayLayers > 1u ) return NULL;
		layers = 6u; depth = 1u;
		break;
	case RAL_TEXTURE_2D_ARRAY:
		layers = ci->depthOrArrayLayers;
		if ( layers == 0u ) return NULL;
		depth = 1u;
		break;
	case RAL_TEXTURE_CUBE_ARRAY:
		if ( ci->width != ci->height || ci->depthOrArrayLayers == 0u
				|| ci->depthOrArrayLayers > UINT32_MAX / 6u ) return NULL;
		layers = ci->depthOrArrayLayers * 6u; depth = 1u;
		break;
	default:
		return NULL;
	}
	fullMips = ralVk_FullMipChain( ci->width, ci->height, depth );
	if ( ci->mipLevels > fullMips ) return NULL;
	generation = b->nextTextureGeneration + 1u;
	tex = (ralTexture_t *)malloc( sizeof( *tex ) );
	if ( !tex ) return NULL;
	RAL_ZERO( *tex );
	tex->header.refCount = 1;
	tex->backend         = b;
	tex->image           = (VkImage)externalImage;
	tex->alloc           = NULL;
	tex->defaultView     = (VkImageView)externalView;
	tex->ralFormat       = ci->format;
	tex->vkFormat        = vkFormat;
	tex->type            = ci->type;
	tex->usage           = ci->usage;
	tex->width           = ci->width;
	tex->height          = ci->height;
	tex->depthOrArrayLayers = ci->depthOrArrayLayers ? ci->depthOrArrayLayers : 1u;
	tex->mipLevels       = ci->mipLevels;
	tex->arrayLayers     = layers;
	tex->sampleCount     = ci->sampleCount;
	tex->resourceGeneration = generation;
	tex->aspect          = (VkImageAspectFlags)aspect;
	tex->currentLayout   = VK_IMAGE_LAYOUT_UNDEFINED;
	tex->ownsImage       = qfalse;
	tex->portableStateKnown = qfalse;
	tex->portableOwnerQueue = RAL_QUEUE_GRAPHICS;
	Ral_QueueTransferLifecycleInit( &tex->queueTransfer );
	if ( ci->debugName ) ralVk_SetObjectName( b, (uint64_t)tex->image,
		VK_OBJECT_TYPE_IMAGE, ci->debugName );
	b->nextTextureGeneration = generation;
	return tex;
}

ralTexture_t *Ral_AdoptTextureExact( ralBackend_t *b,
		void *externalImage, void *externalView, ralFormat_t fmt,
		uint32_t width, uint32_t height, uint32_t aspect,
		ralTextureUsage_t usage, const char *debugName ) {
	ralTextureCreateInfo_t ci;
	RAL_ZERO( ci );
	ci.type = RAL_TEXTURE_2D;
	ci.format = fmt;
	ci.width = width;
	ci.height = height ? height : 1u;
	ci.depthOrArrayLayers = 1u;
	ci.mipLevels = 1u;
	ci.sampleCount = 1u;
	ci.usage = usage;
	ci.memory = RAL_MEMORY_DEVICE_LOCAL;
	ci.debugName = debugName;
	return Ral_AdoptTextureResourceExact( b, externalImage, externalView,
		aspect, &ci );
}

ralTexture_t *Ral_AdoptTexture( ralBackend_t *b,
                                void *externalImage,
                                void *externalView,
                                ralFormat_t fmt,
                                uint32_t width, uint32_t height,
                                uint32_t aspect,
                                const char *debugName ) {
	return Ral_AdoptTextureExact( b, externalImage, externalView, fmt,
		width, height, aspect, 0, debugName );
}

qboolean Ral_PublishAdoptedTextureState( ralTexture_t *tex,
		const ralResourceState_t *state, ralQueueType_t ownerQueue ) {
	return ralVk_PublishAdoptedTextureResourceState( tex, state, ownerQueue );
}

// Array-adoption helper. Like Ral_AdoptTexture but for a 2D-ARRAY image rendered
// one layer at a time via dynamic rendering. `defaultView` is the full-array
// sampling view (viewType 2D_ARRAY, baseArrayLayer 0, layerCount layerCount) — it
// becomes tex->defaultView for sampling. `layerViews[]` are the caller-owned
// per-layer single-layer attachment views (each viewType 2D, baseArrayLayer=i,
// layerCount 1); Ral_BeginRendering selects layerViews[depthAttachmentLayerIndex]
// as the depth imageView (VkRenderingInfo has no baseArrayLayer, so the layer
// offset must live in the bound view). arrayLayers is set to the real layerCount
// so ralVk_RenderTargetTransition covers ALL layers (baseArrayLayer 0,
// layerCount arrayLayers) — the first render transitions the whole array once and
// currentLayout-tracking early-returns the rest. ownsImage=qfalse: the renderer
// retains lifetime of the image + every view (default + per-layer); the wrapper
// references layerViews caller memory and frees nothing on Ral_DestroyTexture.
ralTexture_t *Ral_AdoptArrayTexture( ralBackend_t *b,
                                     void *externalImage,
                                     void *externalDefaultView,
                                     const void *const *layerViews,
                                     uint32_t layerCount,
                                     ralFormat_t fmt,
                                     uint32_t width, uint32_t height,
                                     uint32_t aspect,
                                     const char *debugName )
{
	ralTexture_t *tex;
	uint64_t generation;
	if ( !b || !externalImage || !layerViews || layerCount == 0
			|| b->nextTextureGeneration >= UINT64_MAX - 1u ) return NULL;
	generation = b->nextTextureGeneration + 1u;
	tex = (ralTexture_t *)malloc( sizeof( *tex ) );
	if ( !tex ) return NULL;
	RAL_ZERO( *tex );
	tex->header.refCount = 1;
	tex->backend         = b;
	tex->image           = (VkImage)externalImage;
	tex->alloc           = NULL;
	tex->defaultView     = (VkImageView)externalDefaultView;
	tex->ralFormat       = fmt;
	tex->vkFormat        = ( fmt != RAL_FORMAT_UNDEFINED ) ? ralVk_TranslateFormat( fmt ) : VK_FORMAT_UNDEFINED;
	tex->type            = RAL_TEXTURE_2D_ARRAY;
	tex->width           = width;
	tex->height          = height ? height : 1;
	tex->depthOrArrayLayers = layerCount;
	tex->mipLevels       = 1;
	tex->arrayLayers     = layerCount;
	tex->sampleCount     = 1;
	tex->resourceGeneration = generation;
	tex->aspect          = (VkImageAspectFlags)aspect;
	tex->currentLayout   = VK_IMAGE_LAYOUT_UNDEFINED;
	tex->ownsImage       = qfalse;
	tex->portableStateKnown = qfalse;
	tex->portableOwnerQueue = RAL_QUEUE_GRAPHICS;
	Ral_QueueTransferLifecycleInit( &tex->queueTransfer );
	tex->layerViews      = (const VkImageView *)layerViews;
	tex->numLayerViews   = layerCount;
	if ( debugName ) ralVk_SetObjectName( b, (uint64_t)tex->image, VK_OBJECT_TYPE_IMAGE, debugName );
	b->nextTextureGeneration = generation;
	return tex;
}

void *Ral_GetTextureImageHandle( const ralTexture_t *tex ) {
	return tex ? (void *)tex->image : NULL;
}

void *Ral_GetTextureDefaultViewHandle( const ralTexture_t *tex ) {
	return tex ? (void *)tex->defaultView : NULL;
}

uint32_t Ral_GetTextureMipLevelCount( const ralTexture_t *tex ) {
	return tex ? tex->mipLevels : 0u;
}

static qboolean ralVk_TextureResourceReceiptValid(
		const ralTextureResourceReceipt_t *receipt ) {
	return receipt
		&& receipt->schemaVersion == RAL_TEXTURE_RESOURCE_RECEIPT_SCHEMA_VERSION
		&& receipt->backendType == RAL_BACKEND_VULKAN
		&& receipt->textureIdentity != 0u
		&& receipt->resourceGeneration > 0u
		&& receipt->resourceGeneration < UINT64_MAX
		&& receipt->type >= RAL_TEXTURE_1D
		&& receipt->type <= RAL_TEXTURE_CUBE_ARRAY
		&& receipt->format > RAL_FORMAT_UNDEFINED
		&& receipt->format < RAL_FORMAT_COUNT
		&& receipt->width > 0u && receipt->height > 0u
		&& receipt->mipLevels > 0u && receipt->arrayLayers > 0u
		&& ( receipt->imported == qfalse || receipt->imported == qtrue )
		&& receipt->ready == qtrue;
}

qboolean Ral_TextureGetResourceReceipt( const ralTexture_t *tex,
		ralTextureResourceReceipt_t *out ) {
	ralTextureResourceReceipt_t candidate;
	if ( !tex || !out || !tex->backend || tex->resourceGeneration == 0u
			|| tex->resourceGeneration == UINT64_MAX ) return qfalse;
	RAL_ZERO( candidate );
	candidate.schemaVersion = RAL_TEXTURE_RESOURCE_RECEIPT_SCHEMA_VERSION;
	candidate.backendType = tex->backend->type;
	candidate.textureIdentity = (uintptr_t)tex;
	candidate.resourceGeneration = tex->resourceGeneration;
	candidate.type = tex->type;
	candidate.format = tex->ralFormat;
	candidate.usage = tex->usage;
	candidate.width = tex->width;
	candidate.height = tex->height;
	candidate.mipLevels = tex->mipLevels;
	candidate.arrayLayers = tex->arrayLayers;
	candidate.imported = tex->ownsImage ? qfalse : qtrue;
	candidate.ready = qtrue;
	if ( !ralVk_TextureResourceReceiptValid( &candidate ) ) return qfalse;
	*out = candidate;
	return qtrue;
}

qboolean Ral_TextureResourceReceiptExact(
		const ralTextureResourceReceipt_t *a,
		const ralTextureResourceReceipt_t *b ) {
	return ralVk_TextureResourceReceiptValid( a )
		&& ralVk_TextureResourceReceiptValid( b )
		&& !memcmp( a, b, sizeof( *a ) );
}

void *Ral_GetTextureViewHandle( const ralTextureView_t *view ) {
	return view ? (void *)view->view : NULL;
}

ralTextureView_t *Ral_AdoptTextureViewExact( ralBackend_t *b,
		const ralTexture_t *texture, void *externalView ) {
	ralTextureView_t *view;
	if ( !b || !texture || texture->backend != b || !externalView ) return NULL;
	view = (ralTextureView_t *)malloc( sizeof( *view ) );
	if ( !view ) return NULL;
	RAL_ZERO( *view );
	view->header.refCount = 1;
	view->backend = b;
	view->view = (VkImageView)externalView;
	view->texture = texture;
	view->ownsView = qfalse;
	return view;
}

void *Ral_GetSamplerHandle( const ralSampler_t *s ) {
	return s ? (void *)s->sampler : NULL;
}

void Ral_SetTextureLayout( ralTexture_t *tex, uint32_t vkLayout ) {
	if ( !tex ) return;
	tex->currentLayout = (VkImageLayout)vkLayout;
	tex->portableStateKnown = qfalse;
}

ralTextureView_t *Ral_CreateTextureView( ralBackend_t *b, const ralTextureViewCreateInfo_t *ci ) {
	VkImageViewCreateInfo vci;
	ralTextureView_t     *view;
	const ralTexture_t   *tex;
	uint32_t              levelCount, layerCount;
	VkImageAspectFlags    viewAspects;
	if ( !b || !ci || !ci->texture ) return NULL;
	tex = ci->texture;
	if ( tex->backend != b
	  || !ralVk_TranslateTextureViewAspect( ci->aspect, tex->aspect,
			&viewAspects ) ) return NULL;
	if ( ci->baseMipLevel >= tex->mipLevels || ci->baseArrayLayer >= tex->arrayLayers ) {
		RAL_VK_LOG( SEV_WARN,
		       "Ral_CreateTextureView: base range out of bounds (mip %u/%u, layer %u/%u)\n",
		       ci->baseMipLevel, tex->mipLevels, ci->baseArrayLayer, tex->arrayLayers );
		return NULL;
	}
	levelCount = ci->mipLevelCount   ? ci->mipLevelCount   : ( tex->mipLevels   - ci->baseMipLevel  );
	layerCount = ci->arrayLayerCount ? ci->arrayLayerCount : ( tex->arrayLayers - ci->baseArrayLayer );
	if ( levelCount > tex->mipLevels - ci->baseMipLevel ||
	     layerCount > tex->arrayLayers - ci->baseArrayLayer ) {
		RAL_VK_LOG( SEV_WARN,
		       "Ral_CreateTextureView: range exceeds texture (mips %u+%u/%u, layers %u+%u/%u)\n",
		       ci->baseMipLevel, levelCount, tex->mipLevels,
		       ci->baseArrayLayer, layerCount, tex->arrayLayers );
		return NULL;
	}

	view = (ralTextureView_t *)malloc( sizeof( *view ) );
	if ( !view ) return NULL;
	RAL_ZERO( *view );
	view->header.refCount = 1;
	view->backend         = b;
	view->texture         = tex;
	view->ownsView        = qtrue;

	RAL_ZERO( vci );
	vci.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	vci.image                           = tex->image;
	vci.viewType                        = ralVk_ViewType( ci->viewType );
	vci.format                          = ( ci->format != RAL_FORMAT_UNDEFINED ) ? ralVk_TranslateFormat( ci->format ) : tex->vkFormat;
	vci.subresourceRange.aspectMask     = viewAspects;
	vci.subresourceRange.baseMipLevel   = ci->baseMipLevel;
	vci.subresourceRange.levelCount     = levelCount;
	vci.subresourceRange.baseArrayLayer = ci->baseArrayLayer;
	vci.subresourceRange.layerCount     = layerCount;
	if ( b->vk.CreateImageView( b->device, &vci, NULL, &view->view ) != VK_SUCCESS ) {
		RAL_VK_LOG( SEV_WARN, "Ral_CreateTextureView: vkCreateImageView failed\n" );
		free( view ); return NULL;
	}
	return view;
}

void Ral_DestroyTextureView( ralTextureView_t *view ) {
	ralBackend_t *b;
	if ( !view ) return;
	if ( view->header.refCount > 1 ) { view->header.refCount--; return; }
	b = view->backend;
	if ( view->ownsView )
		ralVk_DeferDestroy( b, RAL_RES_IMAGE_VIEW,
			RAL_VK_H2U( view->view ), 0, NULL );
	free( view );
}

// Ral_TextureUploadAsync — sync upload of `region`, plus (when region is mip 0
// of a multi-mip texture whose format supports it) GPU mipmap generation via
// vkCmdBlitImage. Returns an already-signaled fence.
ralFence_t *Ral_TextureUploadAsync( ralTexture_t *tex, const ralTextureUploadDesc_t *region ) {
	ralBackend_t        *b;
	ralBufferCreateInfo_t sci;
	ralBuffer_t          *staging;
	VkCommandBuffer       cb;
	VkBufferImageCopy     bic;
	VkFence               fence = VK_NULL_HANDLE;
	ralQueueType_t        q;
	uint32_t              mip0w, mip0h, level;
	qboolean              genMips, blitColor;
	VkFilter              blitFilter;
	uint32_t              releaseBaseMip, releaseMipCount, releaseBaseLayer, releaseLayerCount;
	if ( !tex || !region || !region->data || region->dataSize == 0 ) return NULL;
	b = tex->backend;
	RAL_NOTE_ONCE( "Ral_TextureUploadAsync runs on the transfer queue when no GPU mip-gen is needed (mip-gen uses vkCmdBlitImage, which stays on graphics); still waits internally before returning -- async streaming is Phase 7.15\n" );

	mip0w = tex->width  >> region->mipLevel; if ( mip0w == 0 ) mip0w = 1;
	mip0h = tex->height >> region->mipLevel; if ( mip0h == 0 ) mip0h = 1;

	// Sub-region uploads (regionWidth/Height != 0) are the bindless-lightmap
	// mirror path: copy a rectangle, never auto-gen mips (caller controls every
	// mip in that case).
	qboolean isSubRegion = ( region->regionWidth != 0 && region->regionHeight != 0 ) ? qtrue : qfalse;
	genMips   = ( !region->suppressMipGeneration && !isSubRegion && region->mipLevel == 0 && region->arrayLayer == 0 && tex->mipLevels > 1
	              && tex->aspect == VK_IMAGE_ASPECT_COLOR_BIT
	              && b->formatBlitGen[ tex->ralFormat ] && tex->arrayLayers == 1 ) ? qtrue : qfalse;
	blitColor = !ralVk_FormatIsDepthOrInteger( tex->ralFormat );
	blitFilter = blitColor ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
	if ( !genMips && region->mipLevel == 0 && tex->mipLevels > 1 && !b->formatBlitGen[ tex->ralFormat ] )
		RAL_NOTE_ONCE( "Ral_TextureUploadAsync: format lacks BLIT support -- GPU mip-gen skipped; caller must supply all mips\n" );
	// blit (mip-gen) must stay on graphics — transfer queues don't support BLIT.
	// Sub-region uploads must also stay on graphics so the FRAGMENT_SHADER →
	// TRANSFER barrier (which preserves prior tiles) is legal — transfer queues
	// don't support FRAGMENT_SHADER stage.
	// A streamed full-mip overwrite preserves the other mips and therefore must
	// stay on graphics unless the texture was explicitly created for concurrent
	// transfer ownership.  The no-wait residency path below uses graphics too.
	q = ( genMips || isSubRegion || region->suppressMipGeneration ) ? RAL_QUEUE_GRAPHICS : ralVk_UploadQueue( b );

	RAL_ZERO( sci ); sci.size = region->dataSize; sci.usage = RAL_BUFFER_TRANSFER_SRC | RAL_BUFFER_MAP_WRITE; sci.memory = RAL_MEMORY_HOST_COHERENT; sci.debugName = "ral-staging-tex-upload";
	staging = Ral_CreateBuffer( b, &sci );
	if ( !staging ) return NULL;
	if ( !ralVk_WriteStagingBuffer( staging, region->data, region->dataSize ) ) { Ral_DestroyBuffer( staging ); return NULL; }

	if ( !ralVk_BeginUploadCmd( b, q, &cb ) ) { Ral_DestroyBuffer( staging ); return NULL; }

	// Initial barrier into TRANSFER_DST. Sub-region uploads must preserve
	// previously-written tiles, so transition FROM currentLayout (typically
	// SHADER_READ_ONLY after the first call); full uploads keep the legacy
	// UNDEFINED behavior so the driver can discard the old contents we're
	// about to overwrite anyway. Sub-region path is always on GRAPHICS queue
	// (see the q-selection above), so FRAGMENT_SHADER as srcStage is legal
	// there; the full-upload path uses UNDEFINED + TOP_OF_PIPE which is legal
	// on any queue.
	{
		VkImageLayout        srcLayout = isSubRegion ? tex->currentLayout : VK_IMAGE_LAYOUT_UNDEFINED;
		VkPipelineStageFlags srcStage  = isSubRegion ? VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT : VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		VkAccessFlags        srcAcc    = isSubRegion ? VK_ACCESS_SHADER_READ_BIT             : 0u;
		ralVk_ImgBarrier( b, cb, tex->image, tex->aspect, region->mipLevel, 1, region->arrayLayer, 1,
		                  srcLayout, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		                  srcStage,  VK_PIPELINE_STAGE_TRANSFER_BIT,
		                  srcAcc,    VK_ACCESS_TRANSFER_WRITE_BIT );
	}
	RAL_ZERO( bic );
	bic.bufferOffset                    = 0;
	bic.imageSubresource.aspectMask     = tex->aspect;
	bic.imageSubresource.mipLevel       = region->mipLevel;
	bic.imageSubresource.baseArrayLayer = region->arrayLayer;
	bic.imageSubresource.layerCount     = 1;
	bic.imageOffset.x                   = (int32_t)region->offsetX;
	bic.imageOffset.y                   = (int32_t)region->offsetY;
	bic.imageOffset.z                   = 0;
	bic.imageExtent.width               = ( region->regionWidth  != 0 ) ? region->regionWidth  : mip0w;
	bic.imageExtent.height              = ( region->regionHeight != 0 ) ? region->regionHeight : mip0h;
	bic.imageExtent.depth               = ( tex->type == RAL_TEXTURE_3D ) ? ( tex->depthOrArrayLayers >> region->mipLevel ? tex->depthOrArrayLayers >> region->mipLevel : 1 ) : 1;
	b->vk.CmdCopyBufferToImage( cb, staging->buffer, tex->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &bic );

	if ( genMips ) {
		// mip 0: TRANSFER_DST → TRANSFER_SRC (it becomes the blit source)
		ralVk_ImgBarrier( b, cb, tex->image, tex->aspect, 0, 1, 0, 1,
		                  VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		                  VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
		                  VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT );
		for ( level = 1; level < tex->mipLevels; level++ ) {
			uint32_t sw = tex->width  >> ( level - 1 ); if ( sw == 0 ) sw = 1;
			uint32_t sh = tex->height >> ( level - 1 ); if ( sh == 0 ) sh = 1;
			uint32_t dw = tex->width  >> level;          if ( dw == 0 ) dw = 1;
			uint32_t dh = tex->height >> level;          if ( dh == 0 ) dh = 1;
			VkImageBlit blit;
			// dst mip: UNDEFINED → TRANSFER_DST
			ralVk_ImgBarrier( b, cb, tex->image, tex->aspect, level, 1, 0, 1,
			                  VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			                  VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
			                  0, VK_ACCESS_TRANSFER_WRITE_BIT );
			RAL_ZERO( blit );
			blit.srcSubresource.aspectMask = tex->aspect;
			blit.srcSubresource.mipLevel   = level - 1;
			blit.srcSubresource.layerCount = 1;
			blit.srcOffsets[1].x = (int32_t)sw; blit.srcOffsets[1].y = (int32_t)sh; blit.srcOffsets[1].z = 1;
			blit.dstSubresource.aspectMask = tex->aspect;
			blit.dstSubresource.mipLevel   = level;
			blit.dstSubresource.layerCount = 1;
			blit.dstOffsets[1].x = (int32_t)dw; blit.dstOffsets[1].y = (int32_t)dh; blit.dstOffsets[1].z = 1;
			b->vk.CmdBlitImage( cb, tex->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			                        tex->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, blitFilter );
			// this mip now becomes the next iteration's source
			ralVk_ImgBarrier( b, cb, tex->image, tex->aspect, level, 1, 0, 1,
			                  VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			                  VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
			                  VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT );
		}
		// all mips TRANSFER_SRC → SHADER_READ_ONLY
		ralVk_ImgBarrier( b, cb, tex->image, tex->aspect, 0, tex->mipLevels, 0, 1,
		                  VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		                  VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
		                  VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_SHADER_READ_BIT );
		tex->currentLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		tex->portableStateKnown = qfalse;
		releaseBaseMip = 0; releaseMipCount = tex->mipLevels; releaseBaseLayer = 0; releaseLayerCount = 1;
	} else {
		// single mip uploaded: TRANSFER_DST → SHADER_READ_ONLY.
		// dstStage depends on the recording queue: FRAGMENT_SHADER_BIT is
		// invalid on transfer queues (they only support TRANSFER + SPARSE).
		// For transfer-queue uploads, use BOTTOM_OF_PIPE here; the consumer's
		// graphics-queue acquire-barrier (renderer migration)
		// re-makes the layout visible to fragment-sampling. For graphics-
		// queue uploads (mip-gen / depth-format paths), keep FRAGMENT_SHADER.
		VkPipelineStageFlags dstStage = ( q == RAL_QUEUE_TRANSFER ) ? VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT : VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		VkAccessFlags        dstAcc   = ( q == RAL_QUEUE_TRANSFER ) ? 0u                                    : VK_ACCESS_SHADER_READ_BIT;
		ralVk_ImgBarrier( b, cb, tex->image, tex->aspect, region->mipLevel, 1, region->arrayLayer, 1,
		                  VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		                  VK_PIPELINE_STAGE_TRANSFER_BIT, dstStage,
		                  VK_ACCESS_TRANSFER_WRITE_BIT, dstAcc );
		tex->currentLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;   // approximation when not all mips written
		tex->portableStateKnown = qfalse;
		releaseBaseMip = region->mipLevel; releaseMipCount = 1; releaseBaseLayer = region->arrayLayer; releaseLayerCount = 1;
	}
	// Queue-ownership release: if this ran on a dedicated transfer family, hand
	// `tex` (now in SHADER_READ_ONLY_OPTIMAL) to the graphics family. Mirrors the
	// buffer-upload release above. The layout transition was already performed by
	// the IGNORED-family barriers in the branches above (legal on the transfer
	// queue for the SHADER_READ_ONLY target); this barrier is a pure ownership
	// release (oldLayout == newLayout), so it adds no second layout change. The
	// matching acquire on the graphics queue is the consumer's job (renderer
	// migration, future) — until then nothing samples transfer-uploaded
	// textures on the graphics queue, so the unacquired release is harmless.
	if ( q == RAL_QUEUE_TRANSFER && b->queueFamily[RAL_QUEUE_TRANSFER] != b->queueFamily[RAL_QUEUE_GRAPHICS] ) {
		VkImageMemoryBarrier bar;
		RAL_ZERO( bar );
		bar.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		bar.srcAccessMask                   = VK_ACCESS_TRANSFER_WRITE_BIT;
		bar.dstAccessMask                   = 0;
		bar.oldLayout                       = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		bar.newLayout                       = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		bar.srcQueueFamilyIndex             = b->queueFamily[RAL_QUEUE_TRANSFER];
		bar.dstQueueFamilyIndex             = b->queueFamily[RAL_QUEUE_GRAPHICS];
		bar.image                           = tex->image;
		bar.subresourceRange.aspectMask     = tex->aspect;
		bar.subresourceRange.baseMipLevel   = releaseBaseMip;
		bar.subresourceRange.levelCount     = releaseMipCount;
		bar.subresourceRange.baseArrayLayer = releaseBaseLayer;
		bar.subresourceRange.layerCount     = releaseLayerCount;
		b->vk.CmdPipelineBarrier( cb, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, NULL, 0, NULL, 1, &bar );
	}
	if ( !ralVk_SubmitUploadCmdAndWait( b, q, cb, &fence ) ) { Ral_DestroyBuffer( staging ); return NULL; }
	Ral_DestroyBuffer( staging );
	return ralVk_WrapFence( b, fence );
}

// A single-mip copy of `region` into `tex` on the dedicated transfer queue that
// submits WITHOUT waiting and returns the in-flight fence. Only valid for a plain
// upload (no GPU mip-gen, no sub-region): mip-gen needs vkCmdBlitImage and sub-region
// needs a FRAGMENT_SHADER barrier, neither of which the transfer queue supports, so
// those keep the synchronous graphics path. The image is CONCURRENT graphics+transfer
// (tex->concurrentTransfer), so no queue-family-ownership-release barrier is emitted —
// the graphics-side layout acquire (making the SHADER_READ_ONLY layout visible to
// graphics sampling) is issued at residency-swap time by Ral_TextureAcquireBatchTo-
// Graphics. The staging buffer is freed at the frame-in-flight boundary, after the copy
// completes. Returns NULL on failure (the caller falls back to the synchronous path).
static ralFence_t *ralVk_TextureUploadTransferNoWait( ralTexture_t *tex, const ralTextureUploadDesc_t *region,
	ralSemaphore_t **outReadySemaphore ) {
	ralBackend_t         *b = tex->backend;
	ralBufferCreateInfo_t  sci;
	ralBuffer_t           *staging;
	VkCommandBuffer        cb;
	VkBufferImageCopy      bic;
	VkFence                fence = VK_NULL_HANDLE;
	ralSemaphore_t        *ready = NULL;
	uint32_t               mip0w, mip0h;
	const ralQueueType_t   q = RAL_QUEUE_TRANSFER;
	if ( outReadySemaphore ) *outReadySemaphore = NULL;

	mip0w = tex->width  >> region->mipLevel; if ( mip0w == 0 ) mip0w = 1;
	mip0h = tex->height >> region->mipLevel; if ( mip0h == 0 ) mip0h = 1;
	{
		static qboolean loggedQueue = qfalse;
		if ( !loggedQueue ) {
			loggedQueue = qtrue;
			RAL_VK_LOG( SEV_INFO, "async texture upload uses transfer queue family %u (graphics family %u)\n",
			        b->queueFamily[ RAL_QUEUE_TRANSFER ], b->queueFamily[ RAL_QUEUE_GRAPHICS ] );
		}
	}

	RAL_ZERO( sci ); sci.size = region->dataSize; sci.usage = RAL_BUFFER_TRANSFER_SRC | RAL_BUFFER_MAP_WRITE; sci.memory = RAL_MEMORY_HOST_COHERENT; sci.debugName = "ral-staging-tex-upload-async";
	staging = Ral_CreateBuffer( b, &sci );
	if ( !staging ) return NULL;
	ready = Ral_CreateSemaphore( b, RAL_SEMAPHORE_BINARY );
	if ( !ready ) { Ral_DestroyBuffer( staging ); return NULL; }
	if ( !ralVk_WriteStagingBuffer( staging, region->data, region->dataSize ) ) { Ral_DestroySemaphore( ready ); Ral_DestroyBuffer( staging ); return NULL; }

	if ( !ralVk_BeginUploadCmd( b, q, &cb ) ) { Ral_DestroySemaphore( ready ); Ral_DestroyBuffer( staging ); return NULL; }

	// UNDEFINED → TRANSFER_DST (full-upload discard semantics; legal on any queue).
	ralVk_ImgBarrier( b, cb, tex->image, tex->aspect, region->mipLevel, 1, region->arrayLayer, 1,
	                  VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
	                  VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
	                  0, VK_ACCESS_TRANSFER_WRITE_BIT );
	RAL_ZERO( bic );
	bic.bufferOffset                    = 0;
	bic.imageSubresource.aspectMask     = tex->aspect;
	bic.imageSubresource.mipLevel       = region->mipLevel;
	bic.imageSubresource.baseArrayLayer = region->arrayLayer;
	bic.imageSubresource.layerCount     = 1;
	bic.imageExtent.width               = mip0w;
	bic.imageExtent.height              = mip0h;
	bic.imageExtent.depth               = 1;
	b->vk.CmdCopyBufferToImage( cb, staging->buffer, tex->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &bic );

	// TRANSFER_DST → SHADER_READ_ONLY. dstStage is BOTTOM_OF_PIPE because FRAGMENT_SHADER
	// is invalid on the transfer queue; the graphics-side first-use makes the layout
	// visible to sampling at swap time. CONCURRENT sharing means no ownership-release
	// barrier is needed (and emitting one would be wrong on a CONCURRENT image).
	ralVk_ImgBarrier( b, cb, tex->image, tex->aspect, region->mipLevel, 1, region->arrayLayer, 1,
	                  VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
	                  VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
	                  VK_ACCESS_TRANSFER_WRITE_BIT, 0 );
	tex->currentLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	tex->portableStateKnown = qfalse;

	if ( !ralVk_SubmitUploadCmdNoWait( b, q, cb, ready->sem, &fence ) ) {
		Ral_DestroySemaphore( ready );Ral_DestroyBuffer( staging );return NULL;
	}
	// The staging buffer must outlive the in-flight copy: Ral_DestroyBuffer defers the
	// VkBuffer destroy to the frame-in-flight boundary (the same window the command
	// buffer is freed in), by which time the transfer has completed.
	Ral_DestroyBuffer( staging );
	if ( outReadySemaphore ) *outReadySemaphore = ready;
	return ralVk_WrapFence( b, fence );
}

// Full-mip, no-mipgen upload on the graphics queue.  This is the safe first
// page-streaming primitive for an already-sampled texture: queue order drains
// prior readers, only the addressed mip changes layout, parent mips stay
// SHADER_READ_ONLY, and the returned fence lets the renderer retain its parent
// view until the child copy has actually completed.  No queue-family acquire is
// needed because copy, barriers and subsequent sampling share one queue.
static ralFence_t *ralVk_TextureUploadGraphicsNoWait( ralTexture_t *tex, const ralTextureUploadDesc_t *region ) {
	ralBackend_t          *b = tex->backend;
	ralBufferCreateInfo_t  sci;
	ralBuffer_t           *staging;
	VkCommandBuffer        cb;
	VkBufferImageCopy      bic;
	VkFence                fence = VK_NULL_HANDLE;
	uint32_t               mipw, miph;

	mipw = tex->width >> region->mipLevel; if ( mipw == 0 ) mipw = 1;
	miph = tex->height >> region->mipLevel; if ( miph == 0 ) miph = 1;
	RAL_ZERO( sci );
	sci.size = region->dataSize;
	sci.usage = RAL_BUFFER_TRANSFER_SRC | RAL_BUFFER_MAP_WRITE;
	sci.memory = RAL_MEMORY_HOST_COHERENT;
	sci.debugName = "ral-staging-tex-mip-stream";
	staging = Ral_CreateBuffer( b, &sci );
	if ( !staging ) return NULL;
	if ( !ralVk_WriteStagingBuffer( staging, region->data, region->dataSize ) ) { Ral_DestroyBuffer( staging ); return NULL; }

	if ( !ralVk_BeginUploadCmd( b, RAL_QUEUE_GRAPHICS, &cb ) ) {
		Ral_DestroyBuffer( staging );
		return NULL;
	}
	ralVk_ImgBarrier( b, cb, tex->image, tex->aspect,
	                  region->mipLevel, 1, region->arrayLayer, 1,
	                  tex->currentLayout, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
	                  VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
	                  VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_TRANSFER_WRITE_BIT );
	RAL_ZERO( bic );
	bic.bufferOffset = 0;
	bic.imageSubresource.aspectMask = tex->aspect;
	bic.imageSubresource.mipLevel = region->mipLevel;
	bic.imageSubresource.baseArrayLayer = region->arrayLayer;
	bic.imageSubresource.layerCount = 1;
	bic.imageExtent.width = mipw;
	bic.imageExtent.height = miph;
	bic.imageExtent.depth = ( tex->type == RAL_TEXTURE_3D )
	                      ? ( tex->depthOrArrayLayers >> region->mipLevel ? tex->depthOrArrayLayers >> region->mipLevel : 1 )
	                      : 1;
	b->vk.CmdCopyBufferToImage( cb, staging->buffer, tex->image,
	                            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &bic );
	ralVk_ImgBarrier( b, cb, tex->image, tex->aspect,
	                  region->mipLevel, 1, region->arrayLayer, 1,
	                  VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
	                  VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
	                  VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT );
	tex->currentLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	tex->portableStateKnown = qfalse;
	if ( !ralVk_SubmitUploadCmdNoWait( b, RAL_QUEUE_GRAPHICS, cb, VK_NULL_HANDLE, &fence ) ) {
		Ral_DestroyBuffer( staging );return NULL;
	}
	// Deferred buffer destruction uses the same frame-in-flight retirement as
	// other no-wait uploads, so staging outlives the GPU copy.
	Ral_DestroyBuffer( staging );
	return ralVk_WrapFence( b, fence );
}

// Ral_TextureUploadBegin — residency-ticket entry point. With r_asyncTextureUpload on
// (the default), a transfer queue present, and an upload that needs no GPU mip-gen
// or sub-region handling, it pipelines the copy on the transfer queue and returns
// before the copy completes (synchronous=qfalse); the caller polls Ral_FenceSignaled
// for residency. With the cvar off or no dedicated transfer queue, it runs the
// synchronous upload (Ral_TextureUploadAsync, which submits, waits, and returns an
// already-signaled fence) and reports the texture resident-on-return — byte-identical
// to the historical path. Uploads that need mip-gen / sub-region always take the
// synchronous path (the transfer queue can't run those).
static qboolean ralVk_TextureTransferPrepared( ralTexture_t *tex,
		const ralTextureUploadDesc_t *region, ralQueueType_t queue,
		uint64_t generation, ralTransferReceipt_t *out ) {
	ralAllocationReceipt_t allocation;
	ralTransferRequest_t request;
	uint32_t width, height, depth;
	if ( !tex || !region || !region->data || region->dataSize == 0u
			|| region->mipLevel >= tex->mipLevels || region->arrayLayer >= tex->arrayLayers
			|| !Ral_TextureGetAllocationReceipt( tex, &allocation ) ) return qfalse;
	width = region->regionWidth ? region->regionWidth : tex->width >> region->mipLevel;
	height = region->regionHeight ? region->regionHeight : tex->height >> region->mipLevel;
	if ( width == 0u ) width = 1u;
	if ( height == 0u ) height = 1u;
	depth = tex->type == RAL_TEXTURE_3D ? tex->depthOrArrayLayers >> region->mipLevel : 1u;
	if ( depth == 0u ) depth = 1u;
	if ( region->offsetX > UINT32_MAX - width || region->offsetY > UINT32_MAX - height
			|| region->offsetX + width > ( tex->width >> region->mipLevel ? tex->width >> region->mipLevel : 1u )
			|| region->offsetY + height > ( tex->height >> region->mipLevel ? tex->height >> region->mipLevel : 1u ) ) return qfalse;
	RAL_ZERO( request );
	request.backendType = RAL_BACKEND_VULKAN;
	request.direction = RAL_TRANSFER_UPLOAD;
	request.resourceKind = RAL_TRANSFER_TEXTURE;
	request.resourceIdentity = (uintptr_t)tex;
	request.resourceGeneration = allocation.allocationGeneration;
	request.byteSize = region->dataSize;
	request.byteBudget = allocation.committedSize;
	request.mipLevel = region->mipLevel;
	request.arrayLayer = region->arrayLayer;
	request.offsetX = region->offsetX;
	request.offsetY = region->offsetY;
	request.width = width;
	request.height = height;
	request.depth = depth;
	request.queue = queue;
	return Ral_TransferPrepare( &request, generation, out );
}

ralUploadTicket_t Ral_TextureUploadBegin( ralTexture_t *tex, const ralTextureUploadDesc_t *region ) {
	ralUploadTicket_t ticket;
	ralTransferReceipt_t prepared, published;
	uint64_t generation;
	RAL_ZERO( ticket );
	ticket.texture       = tex;
	ticket.synchronous   = qtrue;
	ticket.fence         = NULL;
	ticket.readySemaphore = NULL;
	ticket.baseMipLevel = region ? region->mipLevel : 0;
	ticket.mipLevelCount = 1;
	ticket.baseArrayLayer = region ? region->arrayLayer : 0;
	ticket.arrayLayerCount = 1;
	ticket.graphicsAcquireRequired = qfalse;
	if ( tex && region && region->data && region->dataSize != 0 ) {
		ralBackend_t *b           = tex->backend;
		qboolean      isSubRegion = ( region->regionWidth != 0 && region->regionHeight != 0 ) ? qtrue : qfalse;
		qboolean      needsMipGen = ( !region->suppressMipGeneration && !isSubRegion && region->mipLevel == 0 && region->arrayLayer == 0 && tex->mipLevels > 1
		                              && tex->aspect == VK_IMAGE_ASPECT_COLOR_BIT
		                              && b->formatBlitGen[ tex->ralFormat ] && tex->arrayLayers == 1 ) ? qtrue : qfalse;
		qboolean      wantAsync   = b->allowAsyncTextureUploads;
		if ( b->nextTransferGeneration >= UINT64_MAX - 1u ) return ticket;
		generation = b->nextTransferGeneration + 1u;
		if ( wantAsync && region->suppressMipGeneration && !isSubRegion ) {
			ralFence_t *f = NULL;
			if ( ralVk_TextureTransferPrepared( tex, region, RAL_QUEUE_GRAPHICS, generation, &prepared )
					&& Ral_TransferPublish( &prepared, RAL_TRANSFER_OUTCOME_NATIVE_ASYNC,
						generation, &published ) ) f = ralVk_TextureUploadGraphicsNoWait( tex, region );
			if ( f ) { ticket.fence = f; ticket.synchronous = qfalse; ticket.transfer = published;
				b->nextTransferGeneration = generation; return ticket; }
			// fall through to the synchronous graphics path on failure
		}
		if ( wantAsync && b->caps.asyncTransfer && tex->concurrentTransfer && !needsMipGen && !isSubRegion ) {
			ralSemaphore_t *ready = NULL;
			ralFence_t *f = NULL;
			if ( ralVk_TextureTransferPrepared( tex, region, RAL_QUEUE_TRANSFER, generation, &prepared )
					&& Ral_TransferPublish( &prepared, RAL_TRANSFER_OUTCOME_NATIVE_ASYNC,
						generation, &published ) ) f = ralVk_TextureUploadTransferNoWait( tex, region, &ready );
			if ( f ) {
				ticket.fence = f;
				ticket.readySemaphore = ready;
				ticket.synchronous = qfalse;
				ticket.graphicsAcquireRequired = qtrue;
				ticket.transfer = published;
				b->nextTransferGeneration = generation;
				return ticket;
			}
			if ( ready ) Ral_DestroySemaphore( ready );
			// fall through to the synchronous path on failure
		}
		{
			ralQueueType_t queue = ( needsMipGen || isSubRegion || region->suppressMipGeneration )
				? RAL_QUEUE_GRAPHICS : ralVk_UploadQueue( b );
			if ( !ralVk_TextureTransferPrepared( tex, region, queue, generation, &prepared )
					|| !Ral_TransferPublish( &prepared, RAL_TRANSFER_OUTCOME_SYNCHRONOUS,
						generation, &published ) ) return ticket;
			ticket.fence = Ral_TextureUploadAsync( tex, region );
			if ( ticket.fence ) { ticket.transfer = published;b->nextTransferGeneration = generation; }
		}
	}
	return ticket;
}

qboolean Ral_TextureUploadTicketComplete( ralUploadTicket_t *ticket ) {
	ralTransferReceipt_t completed;
	if ( !ticket || !Ral_TransferReceiptExact( &ticket->transfer, &ticket->transfer ) ) return qfalse;
	if ( ticket->transfer.state == RAL_TRANSFER_COMPLETED ) return qtrue;
	if ( ticket->transfer.state != RAL_TRANSFER_SUBMITTED || !ticket->fence
			|| !Ral_FenceSignaled( ticket->fence ) ) return qfalse;
	if ( !Ral_TransferComplete( &ticket->transfer, ticket->transfer.submissionGeneration,
			qtrue, &completed ) ) return qfalse;
	ticket->transfer = completed;
	return qtrue;
}

qboolean Ral_TextureUploadTicketGetReceipt( const ralUploadTicket_t *ticket,
		ralTransferReceipt_t *out ) {
	ralTransferReceipt_t candidate;
	if ( !ticket || !out || !Ral_TransferReceiptExact( &ticket->transfer, &ticket->transfer ) ) return qfalse;
	candidate = ticket->transfer;
	*out = candidate;
	return qtrue;
}

// Make a batch of async-uploaded ranges visible to graphics/compute sampling. Each
// image range was copied + transitioned to SHADER_READ_ONLY on the transfer queue.
// This records ONE graphics-queue command buffer with a precise same-layout barrier
// for every ticket and submits it while waiting each ticket's binary ready semaphore.
// The semaphore is the cross-queue memory dependency; the barrier publishes the exact
// mip/layer range to shader reads. One submit per batch, none once the residency list
// is empty. Tickets produced on the graphics/synchronous paths need no acquire.
qboolean Ral_TextureAcquireBatchToGraphics( ralBackend_t *b, const ralUploadTicket_t *tickets, uint32_t count ) {
	VkCommandBuffer       cb;
	VkCommandBufferSubmitInfo cbi;
	VkSemaphoreSubmitInfo *waits;
	VkSubmitInfo2          si2;
	uint32_t               i, waitCount = 0;
	if ( !b || !tickets || count == 0 ) return qfalse;
	for ( i = 0; i < count; i++ ) {
		const ralUploadTicket_t *ticket = &tickets[i];
		const ralTexture_t *t = ticket->texture;
		ralAllocationReceipt_t allocation;
		if ( !ticket->graphicsAcquireRequired ) continue;
		if ( !ticket->readySemaphore || !t || t->backend != b
			|| !Ral_TransferReceiptExact( &ticket->transfer, &ticket->transfer )
			|| ticket->transfer.state != RAL_TRANSFER_COMPLETED
			|| ticket->transfer.request.direction != RAL_TRANSFER_UPLOAD
			|| ticket->transfer.request.resourceKind != RAL_TRANSFER_TEXTURE
			|| ticket->transfer.request.resourceIdentity != (uintptr_t)t
			|| ticket->transfer.request.queue != RAL_QUEUE_TRANSFER
			|| ticket->transfer.request.mipLevel != ticket->baseMipLevel
			|| ticket->transfer.request.arrayLayer != ticket->baseArrayLayer
			|| !Ral_TextureGetAllocationReceipt( t, &allocation )
			|| ticket->transfer.request.resourceGeneration != allocation.allocationGeneration
			|| ticket->mipLevelCount == 0 || ticket->arrayLayerCount == 0 ||
		     ticket->baseMipLevel >= t->mipLevels || ticket->mipLevelCount > t->mipLevels - ticket->baseMipLevel ||
		     ticket->baseArrayLayer >= t->arrayLayers || ticket->arrayLayerCount > t->arrayLayers - ticket->baseArrayLayer ) {
			RAL_VK_LOG( SEV_WARN, "Ral_TextureAcquireBatchToGraphics: invalid upload ticket range\n" );
			return qfalse;
		}
		waitCount++;
	}
	if ( waitCount == 0 ) return qtrue;
	waits = (VkSemaphoreSubmitInfo *)calloc( count, sizeof( *waits ) );
	if ( !waits ) return qfalse;
	if ( !ralVk_BeginUploadCmd( b, RAL_QUEUE_GRAPHICS, &cb ) ) { free( waits ); return qfalse; }
	waitCount = 0;
	for ( i = 0; i < count; i++ ) {
		const ralUploadTicket_t *ticket = &tickets[i];
		ralTexture_t *t = ticket->texture;
		if ( !ticket->graphicsAcquireRequired || !ticket->readySemaphore || !t || t->image == VK_NULL_HANDLE ) continue;
		RAL_ZERO( waits[waitCount] );
		waits[waitCount].sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
		waits[waitCount].semaphore = ticket->readySemaphore->sem;
		waits[waitCount].stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
		waitCount++;
		ralVk_ImgBarrier( b, cb, t->image, t->aspect,
		                  ticket->baseMipLevel, ticket->mipLevelCount,
		                  ticket->baseArrayLayer, ticket->arrayLayerCount,
		                  VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		                  VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
		                  0, VK_ACCESS_SHADER_READ_BIT );
		t->currentLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		t->portableStateKnown = qfalse;
	}
	b->vk.EndCommandBuffer( cb );
	RAL_ZERO( cbi ); cbi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO; cbi.commandBuffer = cb;
	RAL_ZERO( si2 );
	si2.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
	si2.waitSemaphoreInfoCount = waitCount;
	si2.pWaitSemaphoreInfos = waits;
	si2.commandBufferInfoCount = 1;
	si2.pCommandBufferInfos = &cbi;
	ralVk_QueueSubmit2( b, RAL_QUEUE_GRAPHICS, &si2, VK_NULL_HANDLE );
	ralVk_DeferDestroy( b, RAL_RES_CMD_BUFFER, RAL_VK_H2U( cb ), (uint64_t)RAL_QUEUE_GRAPHICS, NULL );
	free( waits );
	return qtrue;
}

// ════════════════════════════════════════════════════════════════════════
// Sampler
// ════════════════════════════════════════════════════════════════════════
ralSampler_t *Ral_CreateSampler( ralBackend_t *b, const ralSamplerCreateInfo_t *ci ) {
	VkSamplerCreateInfo  sci;
	ralSampler_t        *s;
	if ( !b || !Ral_SamplerCreateInfoValid( ci ) ) return NULL;
	s = (ralSampler_t *)malloc( sizeof( *s ) );
	if ( !s ) return NULL;
	RAL_ZERO( *s );
	s->header.refCount = 1; s->backend = b; s->ownsSampler = qtrue;
	RAL_ZERO( sci );
	sci.sType         = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	sci.magFilter     = ralVk_Filter( ci->magFilter );
	sci.minFilter     = ralVk_Filter( ci->minFilter );
	sci.mipmapMode    = ralVk_MipMode( ci->mipmapMode );
	sci.addressModeU  = ralVk_AddrMode( ci->addressU );
	sci.addressModeV  = ralVk_AddrMode( ci->addressV );
	sci.addressModeW  = ralVk_AddrMode( ci->addressW );
	if ( b->haveSamplerAnisotropy && ci->maxAnisotropy > 1.0f ) {
		float a = ci->maxAnisotropy;
		if ( a > b->caps.maxSamplerAnisotropy ) a = b->caps.maxSamplerAnisotropy;
		sci.anisotropyEnable = VK_TRUE;
		sci.maxAnisotropy    = a;
	} else {
		sci.anisotropyEnable = VK_FALSE;
		sci.maxAnisotropy    = 1.0f;
	}
	sci.compareEnable    = ci->compareEnable ? VK_TRUE : VK_FALSE;
	sci.compareOp        = ralVk_CompareOp( ci->compareOp );
	sci.minLod           = ci->minLod;
	sci.maxLod           = ( ci->maxLod > 0.0f ) ? ci->maxLod : VK_LOD_CLAMP_NONE;
	sci.borderColor      = ralVk_BorderColor( ci->borderColor );
	if ( b->vk.CreateSampler( b->device, &sci, NULL, &s->sampler ) != VK_SUCCESS ) {
		RAL_VK_LOG( SEV_WARN, "Ral_CreateSampler: vkCreateSampler failed\n" );
		free( s ); return NULL;
	}
	ralVk_SetObjectName( b, (uint64_t)s->sampler, VK_OBJECT_TYPE_SAMPLER, ci->debugName );
	return s;
}

void Ral_DestroySampler( ralSampler_t *s ) {
	ralBackend_t *b;
	if ( !s ) return;
	if ( s->header.refCount > 1 ) { s->header.refCount--; return; }
	b = s->backend;
	if ( s->ownsSampler ) {
		ralVk_DeferDestroy( b, RAL_RES_SAMPLER, RAL_VK_H2U( s->sampler ), 0, NULL );
	}
	free( s );
}

// Adoption helper — wraps an existing VkSampler in a ralSampler_t with
// ownsSampler=qfalse. Mirrors Ral_AdoptTexture / Ral_AdoptBindGroup:
// teardown only frees the wrapper struct; the caller's create site
// retains lifetime ownership.
ralSampler_t *Ral_AdoptSampler( ralBackend_t *b, void *externalSampler, const char *debugName ) {
	ralSampler_t *s;
	if ( !b || !externalSampler ) return NULL;
	s = (ralSampler_t *)malloc( sizeof( *s ) );
	if ( !s ) return NULL;
	RAL_ZERO( *s );
	s->header.refCount = 1;
	s->backend         = b;
	s->sampler         = (VkSampler)externalSampler;
	s->ownsSampler     = qfalse;
	if ( debugName ) ralVk_SetObjectName( b, (uint64_t)s->sampler, VK_OBJECT_TYPE_SAMPLER, debugName );
	return s;
}

// ════════════════════════════════════════════════════════════════════════
// Bind-group arena / layout / group
// ════════════════════════════════════════════════════════════════════════
ralBindGroupArena_t *Ral_CreateBindGroupArena(
		ralBackend_t *backend,
		const ralBindGroupArenaCreateInfo_t *createInfo,
		ralBindGroupArenaReceipt_t *outReceipt ) {
	VkDescriptorPoolSize sizes[ RAL_MAX_BIND_GROUP_ARENA_ENTRIES ];
	VkDescriptorPoolCreateInfo nativeInfo;
	ralBindGroupArena_t *arena;
	ralBindGroupArenaReceipt_t receipt;
	uint32_t i;
	if ( !backend || !createInfo || !outReceipt || !createInfo->entries
	  || !backend->vk.CreateDescriptorPool
	  || !backend->vk.ResetDescriptorPool
	  || !backend->vk.DestroyDescriptorPool
	  || createInfo->numEntries == 0u
	  || createInfo->numEntries > RAL_MAX_BIND_GROUP_ARENA_ENTRIES
	  || createInfo->maxGroups == 0u ) return NULL;
	for ( i = 0u; i < createInfo->numEntries; ++i ) {
		const ralBindGroupArenaEntry_t *entry = &createInfo->entries[i];
		VkDescriptorType type;
		uint32_t j;
		if ( entry->type < RAL_BIND_UNIFORM_BUFFER
		  || entry->type > RAL_BIND_COMBINED_TEXTURE_SAMPLER
		  || entry->count == 0u
		  || ( entry->dynamicOffset != qfalse && entry->dynamicOffset != qtrue )
		  || ( entry->dynamicOffset
		    && entry->type != RAL_BIND_UNIFORM_BUFFER
		    && entry->type != RAL_BIND_STORAGE_BUFFER ) ) return NULL;
		type = ralVk_DescType( entry->type );
		if ( entry->dynamicOffset )
			type = entry->type == RAL_BIND_UNIFORM_BUFFER
				? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC
				: VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
		for ( j = 0u; j < i; ++j )
			if ( sizes[j].type == type ) return NULL;
		sizes[i].type = type;
		sizes[i].descriptorCount = entry->count;
	}
	arena = (ralBindGroupArena_t *)malloc( sizeof( *arena ) );
	if ( !arena ) return NULL;
	RAL_ZERO( *arena );
	arena->backend = backend;
	RAL_ZERO( nativeInfo );
	nativeInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	nativeInfo.maxSets = createInfo->maxGroups;
	nativeInfo.poolSizeCount = createInfo->numEntries;
	nativeInfo.pPoolSizes = sizes;
	if ( backend->vk.CreateDescriptorPool( backend->device, &nativeInfo, NULL,
			&arena->pool ) != VK_SUCCESS ) {
		free( arena );
		return NULL;
	}
	if ( createInfo->debugName )
		ralVk_SetObjectName( backend, (uint64_t)arena->pool,
			VK_OBJECT_TYPE_DESCRIPTOR_POOL, createInfo->debugName );
	Ral_BindGroupArenaLifecycleInit( &arena->lifecycle, backend, arena,
		createInfo->maxGroups );
	if ( Ral_BindGroupArenaLifecyclePublishCreate( &arena->lifecycle,
			&receipt ) != ralSuccess ) {
		backend->vk.DestroyDescriptorPool( backend->device, arena->pool, NULL );
		free( arena );
		return NULL;
	}
	*outReceipt = receipt;
	return arena;
}

ralResult_t Ral_GetBindGroupArenaReceipt(
		const ralBindGroupArena_t *arena,
		ralBindGroupArenaReceipt_t *outReceipt ) {
	if ( !arena ) return ralErrorInvalidArgument;
	return Ral_BindGroupArenaLifecycleGetReceipt( &arena->lifecycle, outReceipt );
}

ralResult_t Ral_ResetBindGroupArenaExact(
		ralBindGroupArena_t *arena,
		const ralBindGroupArenaReceipt_t *current,
		ralBindGroupArenaReceipt_t *outNext ) {
	ralBindGroupArenaReceipt_t live;
	VkResult result;
	if ( !arena || !current || !outNext
	  || arena->lifecycle.generation >= UINT64_MAX - 1u
	  || Ral_BindGroupArenaLifecycleGetReceipt( &arena->lifecycle, &live ) != ralSuccess
	  || !Ral_BindGroupArenaReceiptExact( &live, current ) )
		return ralErrorInvalidArgument;
	result = arena->backend->vk.ResetDescriptorPool(
		arena->backend->device, arena->pool, 0u );
	if ( result == VK_ERROR_DEVICE_LOST ) return ralErrorDeviceLost;
	if ( result != VK_SUCCESS ) return ralErrorUnknown;
	return Ral_BindGroupArenaLifecyclePublishReset( &arena->lifecycle,
		current, outNext );
}

void Ral_DestroyBindGroupArena( ralBindGroupArena_t *arena ) {
	if ( !arena ) return;
	if ( arena->pool != VK_NULL_HANDLE )
		arena->backend->vk.DestroyDescriptorPool(
			arena->backend->device, arena->pool, NULL );
	free( arena );
}

void *Ral_GetBindGroupArenaHandle( const ralBindGroupArena_t *arena ) {
	return arena ? (void *)arena->pool : NULL;
}

ralBindGroupLayout_t *Ral_CreateBindGroupLayout( ralBackend_t *b, const ralBindGroupLayoutCreateInfo_t *ci ) {
	VkDescriptorSetLayoutBinding          binds[ RAL_VK_MAX_LAYOUT_ENTRIES ];
	VkDescriptorBindingFlags              bflags[ RAL_VK_MAX_LAYOUT_ENTRIES ];
	VkDescriptorSetLayoutBindingFlagsCreateInfo fci;
	VkDescriptorSetLayoutCreateInfo       lci;
	ralBindGroupLayout_t                 *L;
	uint32_t                              i, lastUnbounded = 0xFFFFFFFFu;
	qboolean                              anyUpdateAfterBind = qfalse;
	if ( !b || !ci || !ci->entries || ci->numEntries == 0 || ci->numEntries > RAL_VK_MAX_LAYOUT_ENTRIES ) {
		RAL_VK_LOG( SEV_WARN, "Ral_CreateBindGroupLayout: bad/too-many entries (%u)\n", ci ? ci->numEntries : 0u );
		return NULL;
	}
	for ( i = 0; i < ci->numEntries; ++i ) {
		uint32_t j;
		if ( !ralVk_DynamicEntryValid( &ci->entries[i] ) ) {
			RAL_VK_LOG( SEV_WARN, "Ral_CreateBindGroupLayout: invalid dynamic binding %u\n",
				ci->entries[i].binding );
			return NULL;
		}
		for ( j = 0; j < i; ++j )
			if ( ci->entries[j].binding == ci->entries[i].binding ) return NULL;
	}
	if ( ci->bindless ) {
		for ( i = 0; i < ci->numEntries; i++ ) if ( ci->entries[i].count == 0 ) { lastUnbounded = i; }
		if ( lastUnbounded != 0xFFFFFFFFu && !b->caps.bindlessTextures ) {
			RAL_VK_LOG( SEV_WARN, "Ral_CreateBindGroupLayout: bindless layout requested but caps.bindlessTextures is false\n" );
			return NULL;   // ralUnsupported equivalent for a handle-returning call
		}
	}

	L = (ralBindGroupLayout_t *)malloc( sizeof( *L ) );
	if ( !L ) return NULL;
	RAL_ZERO( *L );
	L->header.refCount = 1; L->backend = b; L->bindless = ci->bindless; L->numEntries = ci->numEntries;
	L->ownsLayout = qtrue;   // standalone create owns the VkDescriptorSetLayout.

	for ( i = 0; i < ci->numEntries; i++ ) {
		const ralBindEntry_t *e = &ci->entries[i];
		uint32_t cnt = ( e->count == 0 ) ? RAL_VK_BINDLESS_LAYOUT_COUNT : e->count;
		if ( e->count == 0 && cnt > b->caps.maxBindlessTextures && b->caps.maxBindlessTextures > 0 ) cnt = b->caps.maxBindlessTextures;
		RAL_ZERO( binds[i] );
		binds[i].binding         = e->binding;
		binds[i].descriptorType  = ralVk_DescTypeForEntry( e );
		binds[i].descriptorCount = cnt;
		binds[i].stageFlags      = ralVk_StageFlags( e->stageFlags );
		bflags[i] = 0;
		// In a bindless layout every entry fills in lazily (per-image VkImageView writes for the
		// texture array, per-VkSampler-dedup-pool writes for the sampler array) and is written
		// while command buffers may already reference the set — both bindings need
		// PARTIALLY_BOUND + UPDATE_AFTER_BIND regardless of whether the array is bounded
		// (count > 1) or unbounded (count == 0).
		if ( ci->bindless ) {
			bflags[i] = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;
			anyUpdateAfterBind = qtrue;
		}
		L->entries[i].binding        = e->binding;
		L->entries[i].vkType         = binds[i].descriptorType;
		L->entries[i].count          = e->count;
		L->entries[i].effectiveCount = cnt;
		L->entries[i].dynamicOffset  = e->dynamicOffset;
		if ( e->dynamicOffset ) L->dynamicOffsetCount++;
	}

	RAL_ZERO( fci );
	fci.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
	fci.bindingCount  = ci->numEntries;
	fci.pBindingFlags = bflags;
	RAL_ZERO( lci );
	lci.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	lci.bindingCount = ci->numEntries;
	lci.pBindings    = binds;
	if ( anyUpdateAfterBind ) {
		lci.pNext = &fci;
		lci.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
	}
	if ( b->vk.CreateDescriptorSetLayout( b->device, &lci, NULL, &L->layout ) != VK_SUCCESS ) {
		RAL_VK_LOG( SEV_WARN, "Ral_CreateBindGroupLayout: vkCreateDescriptorSetLayout failed\n" );
		free( L ); return NULL;
	}
	ralVk_SetObjectName( b, (uint64_t)L->layout, VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT, ci->debugName );
	return L;
}

// Wrap a caller-owned VkDescriptorSetLayout.
// Used by renderervk so the q3now descriptor set layouts (vk.set_layout_*)
// can be referenced by RAL pipelines created via Ral_CreateGraphicsPipeline
// without RAL re-creating + double-owning them. The `entries[]` description
// is informational (RAL pipeline-layout creation uses the VkDescriptorSetLayout
// handle directly), but stored on the wrapper so diagnostics + future RAL
// BindGroup creation against an adopted layout have the binding shape.
ralBindGroupLayout_t *Ral_AdoptBindGroupLayout( ralBackend_t *b,
                                                void *externalLayout,
                                                uint32_t numEntries,
                                                const ralBindEntry_t *entries,
                                                const char *debugName ) {
	ralBindGroupLayout_t *L;
	uint32_t              i;
	if ( !b || !externalLayout || ( numEntries > 0 && !entries )
	  || numEntries > RAL_VK_MAX_LAYOUT_ENTRIES ) {
		RAL_VK_LOG( SEV_WARN, "Ral_AdoptBindGroupLayout: bad args (externalLayout=%p, numEntries=%u, max=%u)\n",
		        externalLayout, numEntries, (unsigned)RAL_VK_MAX_LAYOUT_ENTRIES );
		return NULL;
	}
	for ( i = 0; i < numEntries; ++i ) {
		uint32_t j;
		if ( !ralVk_DynamicEntryValid( &entries[i] ) ) return NULL;
		for ( j = 0; j < i; ++j )
			if ( entries[j].binding == entries[i].binding ) return NULL;
	}
	L = (ralBindGroupLayout_t *)malloc( sizeof( *L ) );
	if ( !L ) return NULL;
	RAL_ZERO( *L );
	L->header.refCount = 1;
	L->backend         = b;
	L->layout          = (VkDescriptorSetLayout)externalLayout;
	L->bindless        = qfalse;        // adopted q3now layouts are not bindless
	L->ownsLayout      = qfalse;        // caller owns the VkDescriptorSetLayout
	L->numEntries      = numEntries;
	for ( i = 0; i < numEntries; i++ ) {
		L->entries[i].binding        = entries[i].binding;
		L->entries[i].vkType         = ralVk_DescTypeForEntry( &entries[i] );
		L->entries[i].count          = entries[i].count;
		L->entries[i].effectiveCount = ( entries[i].count == 0 ) ? 1u : entries[i].count;
		L->entries[i].dynamicOffset  = entries[i].dynamicOffset;
		if ( entries[i].dynamicOffset ) L->dynamicOffsetCount++;
	}
	if ( debugName ) {
		// We don't re-name the underlying VkDescriptorSetLayout (caller picked
		// its name); the debugName is recorded informationally via debug-utils
		// only when the caller didn't already name it. Single SetObjectName
		// call is idempotent and cheap.
		ralVk_SetObjectName( b, (uint64_t)L->layout, VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT, debugName );
	}
	return L;
}

void Ral_DestroyBindGroupLayout( ralBindGroupLayout_t *layout ) {
	ralBackend_t *b;
	if ( !layout ) return;
	if ( layout->header.refCount > 1 ) { layout->header.refCount--; return; }
	b = layout->backend;
	// only defer-destroy the VkDescriptorSetLayout
	// when RAL created it. Adopted layouts (ownsLayout=qfalse) belong to the
	// caller — destroying them here would double-free on the renderer's own
	// vkDestroyDescriptorSetLayout call.
	if ( layout->ownsLayout ) {
		ralVk_DeferDestroy( b, RAL_RES_DESC_SET_LAYOUT, RAL_VK_H2U( layout->layout ), 0, NULL );
	}
	(void)b;
	free( layout );
}

void *Ral_GetBindGroupLayoutHandle( const ralBindGroupLayout_t *layout ) {
	return layout ? (void *)layout->layout : NULL;
}

ralBindGroup_t *Ral_CreateBindGroup( ralBackend_t *b, const ralBindGroupCreateInfo_t *ci ) {
	VkDescriptorSetAllocateInfo dai;
	VkWriteDescriptorSet        writes[ RAL_VK_MAX_BG_WRITES ];
	VkDescriptorImageInfo       imgs[ RAL_VK_MAX_BG_IMAGES ];
	VkDescriptorBufferInfo      bufs[ RAL_VK_MAX_BG_BUFFERS ];
	uint32_t                    nw = 0, ni = 0, nb = 0, v, imageValueCount = 0;
	ralBindGroup_t             *bg;
	VkResult                    r;
	VkDescriptorPool           pool;
	ralBindGroupArenaReceipt_t arenaLive;
	if ( !b || !ci || !ci->layout || ci->layout->backend != b
	  || ( ci->numValues > 0u && !ci->values )
	  || ( ( ci->arena == NULL ) != ( ci->arenaReceipt == NULL ) ) ) return NULL;
	pool = b->descriptorPool;
	if ( ci->arena ) {
		if ( ci->arena->backend != b
		  || Ral_GetBindGroupArenaReceipt( ci->arena, &arenaLive ) != ralSuccess
		  || !Ral_BindGroupArenaReceiptExact( &arenaLive, ci->arenaReceipt ) )
			return NULL;
		pool = ci->arena->pool;
	}
	for ( v = 0; v < ci->layout->numEntries; ++v ) {
		const ralVkBindEntry_t *entry = &ci->layout->entries[v];
		uint32_t valueIndex;
		if ( !entry->dynamicOffset ) continue;
		for ( valueIndex = 0; valueIndex < ci->numValues; ++valueIndex ) {
			const ralBindingValue_t *value = &ci->values[valueIndex];
			uint64_t range;
			if ( value->binding != entry->binding ) continue;
			if ( !value->buffer || value->buffer->backend != b ) return NULL;
			if ( ( entry->vkType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC
			    && value->type != RAL_BIND_UNIFORM_BUFFER )
			  || ( entry->vkType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC
			    && value->type != RAL_BIND_STORAGE_BUFFER ) ) return NULL;
			if ( value->bufferOffset > value->buffer->size ) return NULL;
			range = value->bufferRange == 0
				? value->buffer->size - value->bufferOffset : value->bufferRange;
			if ( range == 0
			  || range > value->buffer->size - value->bufferOffset ) return NULL;
			break;
		}
		if ( valueIndex == ci->numValues ) return NULL;
	}
	for ( v = 0; v < ci->numValues; v++ ) {
		const ralBindingValue_t *val = &ci->values[v];
		switch ( val->type ) {
		case RAL_BIND_UNIFORM_BUFFER:
		case RAL_BIND_STORAGE_BUFFER:
			if ( val->buffer && val->buffer->backend != b ) return NULL;
			break;
		case RAL_BIND_SAMPLER:
			if ( val->sampler && val->sampler->backend != b ) return NULL;
			if ( ++imageValueCount > RAL_VK_MAX_BG_IMAGES ) return NULL;
			break;
		case RAL_BIND_SAMPLED_TEXTURE:
		case RAL_BIND_STORAGE_TEXTURE:
			if ( val->textureView && val->textureView->backend != b ) return NULL;
			if ( ++imageValueCount > RAL_VK_MAX_BG_IMAGES ) return NULL;
			break;
		case RAL_BIND_COMBINED_TEXTURE_SAMPLER:
			if ( ( val->textureView && val->textureView->backend != b )
			  || ( val->sampler && val->sampler->backend != b ) ) return NULL;
			if ( ++imageValueCount > RAL_VK_MAX_BG_IMAGES ) return NULL;
			break;
		case RAL_BIND_TEXTURE_ARRAY:
		{
			const ralVkBindEntry_t *entry = ralVk_FindLayoutEntry( ci->layout, val->binding );
			if ( !entry || !val->textureArray || val->textureArrayCount == 0u
					|| val->textureArrayCount > entry->effectiveCount
					|| val->textureArrayCount > RAL_VK_MAX_BG_IMAGES ) return NULL;
			if ( val->textureArrayCount > RAL_VK_MAX_BG_IMAGES - imageValueCount )
				return NULL;
			imageValueCount += val->textureArrayCount;
			if ( entry->vkType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER ) {
				if ( !val->sampler || val->sampler->backend != b ) return NULL;
			} else if ( entry->vkType != VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE ) return NULL;
			for ( uint32_t k = 0; k < val->textureArrayCount; k++ )
				if ( !val->textureArray[k]
						|| val->textureArray[k]->backend != b ) return NULL;
			break;
		}
		default: break;
		}
	}

	// Refuse rather than silently truncate: the write loop caps at
	// RAL_VK_MAX_BG_WRITES and would otherwise return a descriptor set with
	// bindings past the cap left UNwritten (uninitialized), which the caller
	// has no way to detect. Mirror Ral_Submit's over-capacity hard-fail.
	if ( ci->numValues > RAL_VK_MAX_BG_WRITES ) {
		RAL_VK_LOG( SEV_ERROR, "Ral_CreateBindGroup: over-capacity (values %u > max %u) — refusing (would truncate)\n",
			ci->numValues, (unsigned)RAL_VK_MAX_BG_WRITES );
		return NULL;
	}

	bg = (ralBindGroup_t *)malloc( sizeof( *bg ) );
	if ( !bg ) return NULL;
	RAL_ZERO( *bg );
	bg->header.refCount = 1; bg->backend = b; bg->layout = ci->layout;
	bg->ownsSet = qtrue;   // native RAL allocation owns the set (Ral_AdoptBindGroup flips this to qfalse for adopted handles).
	bg->arena = ci->arena;
	if ( ci->arenaReceipt ) bg->arenaReceipt = *ci->arenaReceipt;
	bg->bufferTrackingComplete = qtrue;
	ralVk_InitDynamicBindings( bg, ci->layout );

	RAL_ZERO( dai );
	dai.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	dai.descriptorPool     = pool;
	dai.descriptorSetCount = 1;
	dai.pSetLayouts        = &ci->layout->layout;
	r = b->vk.AllocateDescriptorSets( b->device, &dai, &bg->set );
	if ( r != VK_SUCCESS ) {
		RAL_VK_LOG( SEV_WARN, "Ral_CreateBindGroup: vkAllocateDescriptorSets failed (VkResult %d -- pool may be exhausted)\n", (int)r );
		free( bg ); return NULL;
	}

	for ( v = 0; v < ci->numValues && nw < RAL_VK_MAX_BG_WRITES; v++ ) {
		const ralBindingValue_t *val = &ci->values[v];
		VkWriteDescriptorSet *w = &writes[nw];
		RAL_ZERO( *w );
		w->sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		w->dstSet          = bg->set;
		w->dstBinding      = val->binding;
		w->dstArrayElement = 0;
		{
			const ralVkBindEntry_t *entry = ralVk_FindLayoutEntry( ci->layout, val->binding );
			w->descriptorType = entry ? entry->vkType : ralVk_DescType( val->type );
		}
		switch ( val->type ) {
		case RAL_BIND_UNIFORM_BUFFER:
		case RAL_BIND_STORAGE_BUFFER:
			if ( nb >= RAL_VK_MAX_BG_BUFFERS || !val->buffer ) continue;
			{
				uint32_t tracked;
				for ( tracked = 0; tracked < bg->bufferCount; ++tracked )
					if ( bg->buffers[tracked] == val->buffer ) break;
				if ( tracked == bg->bufferCount
				  && bg->bufferCount < RAL_VK_MAX_TRACKED_BIND_GROUP_BUFFERS )
					bg->buffers[bg->bufferCount++] = val->buffer;
			}
			RAL_ZERO( bufs[nb] );
			bufs[nb].buffer = val->buffer->buffer;
			bufs[nb].offset = val->bufferOffset;
			bufs[nb].range  = ( val->bufferRange == 0 ) ? VK_WHOLE_SIZE : val->bufferRange;
			w->descriptorCount = 1; w->pBufferInfo = &bufs[nb]; nb++;
			{
				const ralVkBindEntry_t *entry = ralVk_FindLayoutEntry( ci->layout, val->binding );
				if ( entry && entry->dynamicOffset ) {
					uint64_t range = val->bufferRange == 0
						? val->buffer->size - val->bufferOffset : val->bufferRange;
					if ( !ralVk_RegisterDynamicBuffer( bg, val->binding, val->buffer,
						val->bufferOffset, range ) ) {
						if ( !bg->arena )
							b->vk.FreeDescriptorSets( b->device, b->descriptorPool, 1, &bg->set );
						free( bg ); return NULL;
					}
				}
			}
			break;
		case RAL_BIND_SAMPLER:
			if ( ni >= RAL_VK_MAX_BG_IMAGES || !val->sampler ) continue;
			RAL_ZERO( imgs[ni] ); imgs[ni].sampler = val->sampler->sampler;
			w->descriptorCount = 1; w->pImageInfo = &imgs[ni]; ni++;
			break;
		case RAL_BIND_STORAGE_TEXTURE:
			if ( ni >= RAL_VK_MAX_BG_IMAGES || !val->textureView ) continue;
			RAL_ZERO( imgs[ni] ); imgs[ni].imageView = val->textureView->view; imgs[ni].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
			w->descriptorCount = 1; w->pImageInfo = &imgs[ni]; ni++;
			break;
		case RAL_BIND_SAMPLED_TEXTURE:
			if ( ni >= RAL_VK_MAX_BG_IMAGES || !val->textureView ) continue;
			RAL_ZERO( imgs[ni] ); imgs[ni].imageView = val->textureView->view; imgs[ni].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			w->descriptorCount = 1; w->pImageInfo = &imgs[ni]; ni++;
			break;
		case RAL_BIND_COMBINED_TEXTURE_SAMPLER:
			if ( ni >= RAL_VK_MAX_BG_IMAGES || !val->textureView || !val->sampler ) continue;
			RAL_ZERO( imgs[ni] );
			imgs[ni].imageView = val->textureView->view;
			imgs[ni].sampler = val->sampler->sampler;
			imgs[ni].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			w->descriptorCount = 1; w->pImageInfo = &imgs[ni]; ni++;
			break;
		case RAL_BIND_TEXTURE_ARRAY: {
			uint32_t k, base = ni;
			const ralVkBindEntry_t *entry = ralVk_FindLayoutEntry( ci->layout, val->binding );
			for ( k = 0; k < val->textureArrayCount; k++ ) {
				RAL_ZERO( imgs[ni] );
				imgs[ni].imageView = val->textureArray[k]->view;
				imgs[ni].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				if ( entry && entry->vkType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER )
					imgs[ni].sampler = val->sampler->sampler;
				ni++;
			}
			w->descriptorCount = ni - base; w->pImageInfo = &imgs[base];
			break; }
		default: continue;
		}
		nw++;
	}
	if ( nw > 0 ) b->vk.UpdateDescriptorSets( b->device, nw, writes, 0, NULL );
	ralVk_SetObjectName( b, (uint64_t)bg->set, VK_OBJECT_TYPE_DESCRIPTOR_SET, ci->debugName );
	return bg;
}

void Ral_DestroyBindGroup( ralBindGroup_t *g ) {
	ralBackend_t *b;
	if ( !g ) return;
	if ( g->header.refCount > 1 ) { g->header.refCount--; return; }
	b = g->backend;
	// only defer-destroy the underlying VkDescriptorSet
	// when RAL owns it. Adopted sets (ownsSet=qfalse) belong to the renderer's
	// own descriptor pool — vkFreeDescriptorSets would race with / double-free
	// against the existing vkResetDescriptorPool / vk_destroy_descriptor_pools
	// path. Wrapper struct is freed in both cases.
	if ( g->ownsSet && !g->arena ) {
		// the pool has FREE_DESCRIPTOR_SET_BIT — the set is returned to the
		// pool via vkFreeDescriptorSets after the deferred-destroy delay.
		ralVk_DeferDestroy( b, RAL_RES_DESC_SET, RAL_VK_H2U( g->set ), 0, NULL );
	}
	(void)b;
	free( g );
}


// native-handle accessor for parallel-paths bind-side
// callers that need to reverse-look up adopted wrappers by VkDescriptorSet.
void *Ral_GetBindGroupHandle( const ralBindGroup_t *g ) {
	return g ? (void *)g->set : NULL;
}


// Mirrors Ral_AdoptBindGroupLayout's contract: the
// caller's descriptor pool owns the lifetime of `externalSet`, the wrapper
// is just metadata + a stable RAL handle that the renderer can pass to
// Ral_CmdBindBindGroup. ownsSet=qfalse so Ral_DestroyBindGroup skips the
// vkFreeDescriptorSets call. layout must be a previously-adopted (or RAL-
// created) ralBindGroupLayout_t whose binding shape matches the writes the
// caller already did against externalSet.
// ════════════════════════════════════════════════════════════════════════
// ralPipelineLayout_t adoption helper. See ral_resource.h for the
// parallel-paths lifetime rationale. ownsHandle=qfalse on adopted wrappers;
// destroy frees only the wrapper and leaves the renderer-owned handle intact.
// ════════════════════════════════════════════════════════════════════════

ralPipelineLayout_t *Ral_CreatePipelineLayout( ralBackend_t *b,
		const ralPipelineLayoutCreateInfo_t *ci ) {
	VkDescriptorSetLayout nativeLayouts[ RAL_MAX_PIPELINE_BIND_GROUP_LAYOUTS ];
	VkPushConstantRange pushRange;
	VkPipelineLayoutCreateInfo nativeInfo;
	ralPipelineLayout_t *candidate;
	uint32_t i;
	if ( !b || !ci || !b->vk.CreatePipelineLayout || !b->vk.DestroyPipelineLayout
			|| ci->numBindGroupLayouts > RAL_MAX_PIPELINE_BIND_GROUP_LAYOUTS
			|| ( ci->numBindGroupLayouts > 0u && !ci->bindGroupLayouts )
			|| ( b->caps.maxBindGroups > 0u
				&& ci->numBindGroupLayouts > b->caps.maxBindGroups )
			|| ( ci->pushConstantSize == 0u ) != ( ci->pushConstantStages == 0u )
			|| ( ci->pushConstantSize == 0u && ci->pushConstantOffset != 0u )
			|| ( ci->pushConstantOffset & 3u ) != 0u
			|| ( ci->pushConstantSize & 3u ) != 0u
			|| ( ci->pushConstantStages & ~RAL_STAGE_ALL ) != 0u
			|| ci->pushConstantOffset > b->caps.maxPushConstantSize
			|| ci->pushConstantSize > b->caps.maxPushConstantSize
				- ci->pushConstantOffset ) return NULL;
	for ( i = 0u; i < ci->numBindGroupLayouts; ++i ) {
		const ralBindGroupLayout_t *layout = ci->bindGroupLayouts[i];
		if ( !layout || layout->backend != b || layout->layout == VK_NULL_HANDLE )
			return NULL;
		nativeLayouts[i] = layout->layout;
	}
	candidate = (ralPipelineLayout_t *)malloc( sizeof( *candidate ) );
	if ( !candidate ) return NULL;
	RAL_ZERO( *candidate );
	RAL_ZERO( pushRange );
	pushRange.stageFlags = ralVk_StageFlags( ci->pushConstantStages );
	pushRange.offset = ci->pushConstantOffset;
	pushRange.size = ci->pushConstantSize;
	RAL_ZERO( nativeInfo );
	nativeInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	nativeInfo.setLayoutCount = ci->numBindGroupLayouts;
	nativeInfo.pSetLayouts = ci->numBindGroupLayouts ? nativeLayouts : NULL;
	nativeInfo.pushConstantRangeCount = ci->pushConstantSize ? 1u : 0u;
	nativeInfo.pPushConstantRanges = ci->pushConstantSize ? &pushRange : NULL;
	if ( b->vk.CreatePipelineLayout( b->device, &nativeInfo, NULL,
			&candidate->vkHandle ) != VK_SUCCESS
			|| candidate->vkHandle == VK_NULL_HANDLE ) {
		if ( candidate->vkHandle != VK_NULL_HANDLE )
			b->vk.DestroyPipelineLayout( b->device, candidate->vkHandle, NULL );
		free( candidate );
		return NULL;
	}
	candidate->backend = b;
	candidate->ownsHandle = qtrue;
	candidate->portableShapeKnown = qtrue;
	candidate->numBindGroupLayouts = ci->numBindGroupLayouts;
	if ( ci->numBindGroupLayouts > 0u )
		memcpy( candidate->bindGroupLayouts, nativeLayouts,
			ci->numBindGroupLayouts * sizeof( nativeLayouts[0] ) );
	candidate->pushConstantSize = ci->pushConstantSize;
	candidate->pushConstantStages = ci->pushConstantStages;
	if ( ci->pushConstantSize > 0u ) {
		candidate->externalPushRangeCount = 1u;
		candidate->externalPushRanges[0].stageFlags = ci->pushConstantStages;
		candidate->externalPushRanges[0].offset = ci->pushConstantOffset;
		candidate->externalPushRanges[0].size = ci->pushConstantSize;
	}
	ralVk_SetObjectName( b, (uint64_t)candidate->vkHandle,
		VK_OBJECT_TYPE_PIPELINE_LAYOUT, ci->debugName );
	return candidate;
}

static ralPipelineLayout_t *AdoptPipelineLayoutExact( ralBackend_t *b,
		void *externalLayout, qboolean ownsHandle, const char *debugName ) {
	ralPipelineLayout_t *pl;
	if ( !b || !externalLayout || ( ownsHandle && !b->vk.DestroyPipelineLayout ) )
		return NULL;
	pl = (ralPipelineLayout_t *)malloc( sizeof( *pl ) );
	if ( !pl ) return NULL;
	RAL_ZERO( *pl );
	pl->backend    = b;
	pl->vkHandle   = (VkPipelineLayout)externalLayout;
	pl->ownsHandle = ownsHandle;
	if ( debugName ) ralVk_SetObjectName( b, (uint64_t)pl->vkHandle, VK_OBJECT_TYPE_PIPELINE_LAYOUT, debugName );
	return pl;
}

ralPipelineLayout_t *Ral_AdoptPipelineLayout( ralBackend_t *b,
		void *externalLayout, const char *debugName ) {
	return AdoptPipelineLayoutExact( b, externalLayout, qfalse, debugName );
}

ralPipelineLayout_t *Ral_AdoptOwnedPipelineLayout( ralBackend_t *b,
		void *externalLayout, const char *debugName ) {
	return AdoptPipelineLayoutExact( b, externalLayout, qtrue, debugName );
}

void Ral_DestroyPipelineLayout( ralPipelineLayout_t *pl ) {
	if ( !pl ) return;
	if ( pl->ownsHandle && pl->backend && pl->vkHandle != VK_NULL_HANDLE ) {
		pl->backend->vk.DestroyPipelineLayout( pl->backend->device, pl->vkHandle, NULL );
	}
	free( pl );
}

void *Ral_GetPipelineLayoutHandle( const ralPipelineLayout_t *pl ) {
	return pl ? (void *)pl->vkHandle : NULL;
}

qboolean Ral_RegisterExternalPipelineLayoutPushRange(
		ralPipelineLayout_t *layout, uint32_t stageFlags,
		uint32_t offset, uint32_t size ) {
	uint32_t i;
	uint32_t limit;
	if ( !layout || !layout->backend || layout->vkHandle == VK_NULL_HANDLE
			|| stageFlags == 0u || ( stageFlags & ~RAL_STAGE_ALL ) != 0u
			|| size == 0u || ( offset & 3u ) != 0u || ( size & 3u ) != 0u )
		return qfalse;
	limit = layout->backend->caps.maxPushConstantSize;
	if ( limit == 0u || size > limit || offset > limit - size ) return qfalse;
	for ( i = 0; i < layout->externalPushRangeCount; ++i ) {
		const uint32_t priorEnd = layout->externalPushRanges[i].offset
			+ layout->externalPushRanges[i].size;
		const uint32_t candidateEnd = offset + size;
		if ( layout->externalPushRanges[i].stageFlags == stageFlags
				&& layout->externalPushRanges[i].offset == offset
				&& layout->externalPushRanges[i].size == size ) return qtrue;
		if ( ( layout->externalPushRanges[i].stageFlags & stageFlags ) != 0u
				&& offset < priorEnd
				&& layout->externalPushRanges[i].offset < candidateEnd ) return qfalse;
	}
	if ( layout->externalPushRangeCount >= RAL_VK_MAX_EXTERNAL_PUSH_RANGES )
		return qfalse;
	i = layout->externalPushRangeCount;
	layout->externalPushRanges[i].stageFlags = stageFlags;
	layout->externalPushRanges[i].offset = offset;
	layout->externalPushRanges[i].size = size;
	layout->externalPushRangeCount = i + 1u;
	return qtrue;
}

ralBindGroup_t *Ral_AdoptBindGroup( ralBackend_t *b,
                                    void *externalSet,
                                    const ralBindGroupLayout_t *layout,
                                    const char *debugName ) {
	ralBindGroup_t *g;
	if ( !b || !externalSet || !layout ) {
		RAL_VK_LOG( SEV_WARN, "Ral_AdoptBindGroup: bad args (b=%p, externalSet=%p, layout=%p)\n",
		        (void *)b, externalSet, (const void *)layout );
		return NULL;
	}
	g = (ralBindGroup_t *)malloc( sizeof( *g ) );
	if ( !g ) return NULL;
	RAL_ZERO( *g );
	g->header.refCount = 1;
	g->backend         = b;
	g->set             = (VkDescriptorSet)externalSet;
	g->layout          = layout;
	g->ownsSet         = qfalse;
	ralVk_InitDynamicBindings( g, layout );
	if ( debugName ) {
		ralVk_SetObjectName( b, (uint64_t)g->set, VK_OBJECT_TYPE_DESCRIPTOR_SET, debugName );
	}
	return g;
}

qboolean Ral_RegisterAdoptedBindGroupDynamicBuffer( ralBindGroup_t *group,
	uint32_t binding, const ralBuffer_t *buffer, uint64_t baseOffset, uint64_t range ) {
	uint32_t i;
	if ( !group || group->ownsSet || !ralVk_RegisterDynamicBuffer(
		group, binding, buffer, baseOffset, range ) ) return qfalse;
	for ( i = 0; i < group->dynamicOffsetCount; ++i )
		if ( !group->dynamicBindings[i].registered ) return qtrue;
	group->bufferTrackingComplete = qtrue;
	return qtrue;
}

// `tex == NULL` is a no-op clear. Vulkan doesn't have an
// "unwrite descriptor" operation; writing VK_NULL_HANDLE as the imageView is
// invalid even with PARTIALLY_BOUND (which only protects UNINITIALIZED
// descriptors, not explicit-null writes). Callers that can destroy a formerly
// published view must overwrite its slot with a live, dimension-compatible
// fallback or rebuild the bind group before destroying that view.
static int ralVk_BindGroupSetImageViewAt( ralBindGroup_t *g, uint32_t slot, VkImageView imageView, const char *caller ) {
	VkWriteDescriptorSet  w;
	VkDescriptorImageInfo img;
	ralBackend_t         *b;
	uint32_t              i, binding = 0xFFFFFFFFu, capacity = 0u;
	if ( !g || !ralVk_BindGroupArenaLive( g ) ) return 0;
	b = g->backend;
	for ( i = 0; g->layout && i < g->layout->numEntries; i++ )
		if ( g->layout->entries[i].vkType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE ) {
			binding = g->layout->entries[i].binding;
			capacity = g->layout->entries[i].effectiveCount;
			break;
		}
	if ( binding == 0xFFFFFFFFu ) { RAL_VK_LOG( SEV_WARN, "%s: layout has no SAMPLED_IMAGE binding\n", caller ); return 0; }
	if ( slot >= capacity ) return 0;
	// NULL is an intentional no-op clear for a PARTIALLY_BOUND table. The
	// validated slot remains stale until the publication ledger tombstones it.
	if ( imageView == VK_NULL_HANDLE ) return 1;
	RAL_ZERO( img );
	img.imageView   = imageView;
	img.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	RAL_ZERO( w );
	w.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	w.dstSet          = g->set;
	w.dstBinding      = binding;
	w.dstArrayElement = slot;
	w.descriptorCount = 1;
	w.descriptorType  = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
	w.pImageInfo      = &img;
	b->vk.UpdateDescriptorSets( b->device, 1, &w, 0, NULL );
	return 1;
}

int Ral_BindGroupSetTextureAt( ralBindGroup_t *g, uint32_t slot, ralTexture_t *tex ) {
	if ( tex && ( tex->backend != ( g ? g->backend : NULL )
			|| tex->defaultView == VK_NULL_HANDLE ) ) return 0;
	return ralVk_BindGroupSetImageViewAt( g, slot,
		tex ? tex->defaultView : VK_NULL_HANDLE, "Ral_BindGroupSetTextureAt" );
}

int Ral_BindGroupSetTextureViewAt( ralBindGroup_t *g, uint32_t slot, ralTextureView_t *view ) {
	if ( view && ( view->backend != ( g ? g->backend : NULL )
			|| view->view == VK_NULL_HANDLE ) ) return 0;
	return ralVk_BindGroupSetImageViewAt( g, slot,
		view ? view->view : VK_NULL_HANDLE, "Ral_BindGroupSetTextureViewAt" );
}

int Ral_BindGroupSetTextureViewAtBinding( ralBindGroup_t *g, uint32_t binding,
		uint32_t slot, ralTextureView_t *view ) {
	VkWriteDescriptorSet  write;
	VkDescriptorImageInfo image;
	ralBackend_t          *backend;
	uint32_t               i;
	const ralVkBindEntry_t *entry = NULL;
	if ( !g || !view || !ralVk_BindGroupArenaLive( g )
			|| view->backend != g->backend || view->view == VK_NULL_HANDLE ) return 0;
	backend = g->backend;
	for ( i = 0; g->layout && i < g->layout->numEntries; ++i ) {
		if ( g->layout->entries[i].binding == binding ) {
			entry = &g->layout->entries[i];
			break;
		}
	}
	if ( !entry || entry->vkType != VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE
			|| slot >= entry->effectiveCount ) return 0;
	RAL_ZERO( image );
	image.imageView = view->view;
	image.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	RAL_ZERO( write );
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.dstSet = g->set;
	write.dstBinding = binding;
	write.dstArrayElement = slot;
	write.descriptorCount = 1u;
	write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
	write.pImageInfo = &image;
	backend->vk.UpdateDescriptorSets( backend->device, 1u, &write, 0u, NULL );
	return 1;
}

int Ral_BindGroupSetTextureViewsAt( ralBindGroup_t *g, const uint32_t *slots,
		ralTextureView_t *const *views, uint32_t count ) {
	VkWriteDescriptorSet *writes;
	VkDescriptorImageInfo *images;
	ralBackend_t *b;
	uint32_t i, binding = 0xFFFFFFFFu, capacity = 0;
	if ( !g || !slots || !views || count == 0
	  || !ralVk_BindGroupArenaLive( g ) ) return 0;
	b = g->backend;
	for ( i = 0; g->layout && i < g->layout->numEntries; ++i ) {
		if ( g->layout->entries[i].vkType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE ) {
			binding = g->layout->entries[i].binding;
			capacity = g->layout->entries[i].effectiveCount;
			break;
		}
	}
	if ( binding == 0xFFFFFFFFu || count > capacity ) return 0;
	for ( i = 0; i < count; ++i ) {
		uint32_t j;
		if ( !views[i] || views[i]->backend != b ||
		     views[i]->view == VK_NULL_HANDLE || slots[i] >= capacity ) return 0;
		for ( j = 0; j < i; ++j ) if ( slots[j] == slots[i] ) return 0;
	}
	writes = (VkWriteDescriptorSet *)calloc( count, sizeof( *writes ) );
	images = (VkDescriptorImageInfo *)calloc( count, sizeof( *images ) );
	if ( !writes || !images ) { free( writes ); free( images ); return 0; }
	for ( i = 0; i < count; ++i ) {
		images[i].imageView = views[i]->view;
		images[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[i].dstSet = g->set;
		writes[i].dstBinding = binding;
		writes[i].dstArrayElement = slots[i];
		writes[i].descriptorCount = 1;
		writes[i].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
		writes[i].pImageInfo = &images[i];
	}
	b->vk.UpdateDescriptorSets( b->device, count, writes, 0, NULL );
	free( images );
	free( writes );
	return 1;
}

// Mirror of Ral_BindGroupSetTextureAt for the SAMPLER binding. NULL sampler
// is a no-op (see the SAMPLED_IMAGE comment above for the rationale).
int Ral_BindGroupSetSamplerAt( ralBindGroup_t *g, uint32_t slot, ralSampler_t *s ) {
	VkWriteDescriptorSet  w;
	VkDescriptorImageInfo img;
	ralBackend_t         *b;
	uint32_t              i, binding = 0xFFFFFFFFu, capacity = 0u;
	if ( !g || !ralVk_BindGroupArenaLive( g ) ) return 0;
	b = g->backend;
	for ( i = 0; g->layout && i < g->layout->numEntries; i++ )
		if ( g->layout->entries[i].vkType == VK_DESCRIPTOR_TYPE_SAMPLER ) {
			binding = g->layout->entries[i].binding;
			capacity = g->layout->entries[i].effectiveCount;
			break;
		}
	if ( binding == 0xFFFFFFFFu ) { RAL_VK_LOG( SEV_WARN, "Ral_BindGroupSetSamplerAt: layout has no SAMPLER binding\n" ); return 0; }
	if ( slot >= capacity ) return 0;
	if ( !s ) return 1;
	if ( s->backend != b || s->sampler == VK_NULL_HANDLE ) return 0;
	RAL_ZERO( img );
	img.sampler = s->sampler;
	RAL_ZERO( w );
	w.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	w.dstSet          = g->set;
	w.dstBinding      = binding;
	w.dstArrayElement = slot;
	w.descriptorCount = 1;
	w.descriptorType  = VK_DESCRIPTOR_TYPE_SAMPLER;
	w.pImageInfo      = &img;
	b->vk.UpdateDescriptorSets( b->device, 1, &w, 0, NULL );
	return 1;
}

// interop bridge — renderervk needs raw VkImage / VkImageView /
// VkDevice handles for the parallel-paths migration model. These are
// backend-internal accessors (NOT part of the public RAL surface), exposed
// only to code statically linked against the ral_vulkan static lib (the
// renderer DLL). They go away later when descriptor binding migrates onto
// the RAL surface and the renderer stops needing native Vulkan handles.
VkImage     ralVk_GetTextureNativeImage    ( const ralTexture_t *tex ) { return tex ? tex->image       : VK_NULL_HANDLE; }
VkImageView ralVk_GetTextureNativeImageView( const ralTexture_t *tex ) { return tex ? tex->defaultView : VK_NULL_HANDLE; }
VkDevice    ralVk_GetBackendNativeDevice   ( const ralBackend_t *b   ) { return b   ? b->device        : VK_NULL_HANDLE; }

// ════════════════════════════════════════════════════════════════════════
// resource-layer init / shutdown (called from ral_vulkan_backend.c)
// ════════════════════════════════════════════════════════════════════════
qboolean ralVk_InitResourceLayer( ralBackend_t *b ) {
	VkDescriptorPoolSize       sizes[6];
	VkDescriptorPoolCreateInfo dpi;
	uint32_t                   i, sampledCount;

	sampledCount = RAL_VK_BINDLESS_LAYOUT_COUNT * RAL_VK_BINDLESS_POOL_SETS + 1024u;
	sizes[0].type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;          sizes[0].descriptorCount = sampledCount;
	sizes[1].type = VK_DESCRIPTOR_TYPE_SAMPLER;                sizes[1].descriptorCount = 256u;
	sizes[2].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; sizes[2].descriptorCount = 1024u;
	sizes[3].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;         sizes[3].descriptorCount = 4096u;
	sizes[4].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;         sizes[4].descriptorCount = 4096u;
	sizes[5].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;          sizes[5].descriptorCount = 256u;
	RAL_ZERO( dpi );
	dpi.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	dpi.flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT          // per-set free path
	                  | ( b->haveDescriptorIndexing ? VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT : 0u );
	dpi.maxSets       = 4096u;
	dpi.poolSizeCount = (uint32_t)( sizeof( sizes ) / sizeof( sizes[0] ) );
	dpi.pPoolSizes    = sizes;
	if ( b->vk.CreateDescriptorPool( b->device, &dpi, NULL, &b->descriptorPool ) != VK_SUCCESS ) {
		RAL_VK_LOG( SEV_WARN, "ralVk_InitResourceLayer: vkCreateDescriptorPool failed\n" );
		return qfalse;
	}

	// per-format GPU-mip-gen capability cache (BLIT_SRC + BLIT_DST + SAMPLED, optimal tiling)
	for ( i = 0; i < RAL_FORMAT_COUNT; i++ ) {
		VkFormat vf = ralVk_TranslateFormat( (ralFormat_t)i );
		VkFormatProperties fp;
		b->formatBlitGen[i] = 0;
		if ( vf == VK_FORMAT_UNDEFINED || !b->vk.GetPhysicalDeviceFormatProperties ) continue;
		b->vk.GetPhysicalDeviceFormatProperties( b->physicalDevice, vf, &fp );
		if ( ( fp.optimalTilingFeatures & ( VK_FORMAT_FEATURE_BLIT_SRC_BIT | VK_FORMAT_FEATURE_BLIT_DST_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT ) )
		     == ( VK_FORMAT_FEATURE_BLIT_SRC_BIT | VK_FORMAT_FEATURE_BLIT_DST_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT ) )
			b->formatBlitGen[i] = 1;
	}
	return qtrue;
}

void ralVk_ShutdownResourceLayer( ralBackend_t *b ) {
	if ( b->descriptorPool != VK_NULL_HANDLE ) { b->vk.DestroyDescriptorPool( b->device, b->descriptorPool, NULL ); b->descriptorPool = VK_NULL_HANDLE; }
	if ( b->allocations ) {
		RAL_VK_LOG( SEV_WARN, "ralVk_ShutdownResourceLayer: %u allocation(s) still live -- freeing\n", b->numAllocations );
		while ( b->allocations ) ralVk_Free( b, b->allocations );
	}
	if ( b->memoryBlocks )
		RAL_VK_LOG( SEV_WARN, "ralVk_ShutdownResourceLayer: reusable memory block leak detected -- freeing\n" );
	while ( b->memoryBlocks ) {
		ralVkMemoryBlock_t *block = b->memoryBlocks;
		b->memoryBlocks = block->next;
		if ( block->mappedBase ) b->vk.UnmapMemory( b->device, block->memory );
		b->vk.FreeMemory( b->device, block->memory, NULL );
		free( block );
	}
	b->ralDeviceLocalBytes = 0;
	b->ralHostVisibleBytes = 0;
}

// ════════════════════════════════════════════════════════════════════════
// \ral_dump resource — exercise the resource layer end-to-end (PART G)
// ════════════════════════════════════════════════════════════════════════
// Copies a small clamped region (≤ 4×4 texels) of `mip` into `out` so we can
// inspect a few pixels without a mip-sized staging buffer.
static void ralVk_ReadbackMip( ralBackend_t *b, ralTexture_t *tex, uint32_t mip, byte *out, uint32_t outSize ) {
	ralBufferCreateInfo_t bci;
	ralBuffer_t          *rb;
	ralBufferMapRequest_t mapRequest;
	ralBufferMapTicket_t  mapTicket;
	VkCommandBuffer       cb;
	VkBufferImageCopy     bic;
	VkBufferMemoryBarrier hostBarrier;
	uint32_t              mw  = ( tex->width  >> mip ) ? ( tex->width  >> mip ) : 1u;
	uint32_t              mh  = ( tex->height >> mip ) ? ( tex->height >> mip ) : 1u;
	uint32_t              bpp = ralVk_FormatBPP( tex->ralFormat );
	uint32_t              ew  = ( mw < 4u ) ? mw : 4u;
	uint32_t              eh  = ( mh < 4u ) ? mh : 4u;
	uint32_t              copyBytes = ew * eh * bpp;
	uint32_t              bufBytes  = ( copyBytes > outSize ) ? copyBytes : outSize;
	if ( bufBytes < 64u ) bufBytes = 64u;
	memset( out, 0, outSize );
	RAL_ZERO( bci ); bci.size = bufBytes; bci.usage = RAL_BUFFER_TRANSFER_DST | RAL_BUFFER_MAP_READ; bci.memory = RAL_MEMORY_HOST_COHERENT; bci.debugName = "ral-test-readback";
	rb = Ral_CreateBuffer( b, &bci );
	if ( !rb ) return;
	if ( !ralVk_BeginUploadCmd( b, RAL_QUEUE_GRAPHICS, &cb ) ) { Ral_DestroyBuffer( rb ); return; }   // graphics queue — same as the upload/mip-gen that produced the data
	ralVk_ImgBarrier( b, cb, tex->image, tex->aspect, 0, tex->mipLevels, 0, tex->arrayLayers,
	                  tex->currentLayout, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
	                  VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
	                  VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_TRANSFER_READ_BIT );
	RAL_ZERO( bic );
	bic.imageSubresource.aspectMask = tex->aspect;
	bic.imageSubresource.mipLevel   = mip;
	bic.imageSubresource.layerCount = 1;
	bic.imageExtent.width  = ew; bic.imageExtent.height = eh; bic.imageExtent.depth = 1;
	b->vk.CmdCopyImageToBuffer( cb, tex->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, rb->buffer, 1, &bic );
	RAL_ZERO( hostBarrier );
	hostBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
	hostBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	hostBarrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
	hostBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	hostBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	hostBarrier.buffer = rb->buffer;
	hostBarrier.offset = 0;
	hostBarrier.size = rb->size;
	b->vk.CmdPipelineBarrier( cb, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
		0, 0, NULL, 1, &hostBarrier, 0, NULL );
	ralVk_ImgBarrier( b, cb, tex->image, tex->aspect, 0, tex->mipLevels, 0, tex->arrayLayers,
	                  VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
	                  VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
	                  VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_SHADER_READ_BIT );
	ralVk_SubmitUploadCmdAndWait( b, RAL_QUEUE_GRAPHICS, cb, NULL );
	tex->currentLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	tex->portableStateKnown = qfalse;
	rb->portableStateKnown = qtrue;
	rb->portableState.usage = RAL_RESOURCE_USAGE_HOST_READ;
	rb->portableState.shaderStages = 0;
	rb->portableOwnerQueue = RAL_QUEUE_GRAPHICS;
	RAL_ZERO( mapRequest ); mapRequest.mode = RAL_MAP_READ; mapRequest.size = rb->size;
	if ( Ral_BufferMapBegin( rb, &mapRequest, &mapTicket ) == ralSuccess ) {
		memcpy( out, mapTicket.mappedRange, ( copyBytes < outSize ) ? copyBytes : outSize );
		(void)Ral_BufferMapUnmap( rb, &mapTicket );
	}
	Ral_DestroyBuffer( rb );
}

void ralVk_RunResourceTest( ralBackend_t *b ) {
	const uint32_t        W = 256, H = 256;
	ralTextureCreateInfo_t tci;
	ralTexture_t          *tex;
	ralTextureUploadDesc_t up;
	ralFence_t            *f;
	byte                  *checker;
	ralMemoryBudget_t      before, after, teardown;
	uint32_t               x, y, m, mips[3], nMips, idx;
	ralBindEntry_t         be;
	ralBindGroupLayoutCreateInfo_t lci;
	ralBindGroup_t        *bg = NULL;
	ralBindGroupLayout_t  *bgl = NULL;

	RAL_VK_LOG( SEV_INFO, "===== RAL resource test (Phase 7.2) =====\n" );
	Ral_QueryMemoryBudget( b, &before );

	RAL_ZERO( tci );
	tci.type = RAL_TEXTURE_2D; tci.format = RAL_FORMAT_R8G8B8A8_UNORM;
	tci.width = W; tci.height = H; tci.depthOrArrayLayers = 1; tci.mipLevels = 0; tci.sampleCount = 1;
	tci.usage = RAL_TEXTURE_USAGE_SAMPLED; tci.memory = RAL_MEMORY_DEVICE_LOCAL; tci.debugName = "ral-test-tex";
	tex = Ral_CreateTexture( b, &tci );
	if ( !tex ) { RAL_VK_LOG( SEV_WARN, "  Ral_CreateTexture failed\n" ); RAL_VK_LOG( SEV_INFO, "===== end RAL resource test =====\n" ); return; }
	RAL_VK_LOG( SEV_INFO, "  texture: %ux%u RGBA8_UNORM, %u mip levels, GPU-mip-gen=%s\n",
	        W, H, tex->mipLevels, b->formatBlitGen[ RAL_FORMAT_R8G8B8A8_UNORM ] ? "yes" : "no" );

	// 1px red/blue checkerboard as mip 0
	checker = (byte *)malloc( (size_t)W * H * 4u );
	for ( y = 0; y < H; y++ ) for ( x = 0; x < W; x++ ) {
		byte *p = checker + ( (size_t)y * W + x ) * 4u;
		if ( ( x ^ y ) & 1u ) { p[0] = 255; p[1] = 0;   p[2] = 0;   p[3] = 255; }   // red
		else                  { p[0] = 0;   p[1] = 0;   p[2] = 255; p[3] = 255; }   // blue
	}
	RAL_ZERO( up ); up.mipLevel = 0; up.arrayLayer = 0; up.data = checker; up.dataSize = (uint64_t)W * H * 4u;
	f = Ral_TextureUploadAsync( tex, &up );
	if ( f ) { RAL_VK_LOG( SEV_INFO, "  Ral_TextureUploadAsync: ok (mip 0 uploaded%s)\n", tex->mipLevels > 1 ? " + GPU mips generated" : "" ); Ral_DestroyFence( f ); }
	else     { RAL_VK_LOG( SEV_WARN, "  Ral_TextureUploadAsync failed\n" ); }
	free( checker );

	Ral_QueryMemoryBudget( b, &after );

	// readback mip 1, mip 4 (if present), and the smallest mip
	mips[0] = 1; nMips = 1;
	if ( tex->mipLevels > 4 ) { mips[nMips++] = 4; }
	mips[nMips++] = tex->mipLevels - 1;
	for ( idx = 0; idx < nMips; idx++ ) {
		byte px[64]; uint32_t mw, mh; m = mips[idx];
		if ( m == 0 || m >= tex->mipLevels ) continue;
		mw = ( W >> m ) ? ( W >> m ) : 1u; mh = ( H >> m ) ? ( H >> m ) : 1u;
		ralVk_ReadbackMip( b, tex, m, px, sizeof( px ) );
		RAL_VK_LOG( SEV_INFO, "  mip %u (%ux%u) pixel[0,0] = R%u G%u B%u A%u%s\n",
		        m, mw, mh, px[0], px[1], px[2], px[3],
		        ( idx == 0 ) ? "  (expect ~127/0/~127/255 = averaged red+blue)" : "" );
	}

	// bindless BindGroup
	RAL_ZERO( be ); be.binding = 0; be.type = RAL_BIND_TEXTURE_ARRAY; be.count = 0; be.stageFlags = RAL_STAGE_FRAGMENT;
	RAL_ZERO( lci ); lci.entries = &be; lci.numEntries = 1; lci.bindless = qtrue; lci.debugName = "ral-test-bindless-layout";
	bgl = Ral_CreateBindGroupLayout( b, &lci );
	if ( !bgl ) {
		RAL_VK_LOG( SEV_INFO, "  bindless: skipped (caps.bindlessTextures=%s)\n", b->caps.bindlessTextures ? "yes-but-layout-failed" : "no" );
	} else {
		ralBindGroupCreateInfo_t bci2;
		RAL_ZERO( bci2 ); bci2.layout = bgl; bci2.values = NULL; bci2.numValues = 0; bci2.debugName = "ral-test-bindless-bg";
		bg = Ral_CreateBindGroup( b, &bci2 );
		if ( !bg ) {
			RAL_VK_LOG( SEV_WARN, "  bindless BindGroup creation failed\n" );
		} else {
			Ral_BindGroupSetTextureAt( bg, 0, tex );
			Ral_BindGroupSetTextureAt( bg, 1, tex );
			RAL_VK_LOG( SEV_INFO, "  bindless BindGroup: %u-slot SAMPLED_IMAGE array created; slots 0+1 populated with the test texture\n", RAL_VK_BINDLESS_LAYOUT_COUNT );
		}
	}

	RAL_VK_LOG( SEV_INFO, "  memory: vk-reported device-local used: before=%u MiB  after=%u MiB\n",
	        (unsigned)( before.deviceLocalUsed >> 20 ), (unsigned)( after.deviceLocalUsed >> 20 ) );
	RAL_VK_LOG( SEV_INFO, "  memory: RAL-tracked device-local footprint = %u KiB across %u allocation(s) (texture + mip chain still live)\n",
	        (unsigned)( b->ralDeviceLocalBytes >> 10 ), b->numAllocations );

	// teardown
	if ( bg )  Ral_DestroyBindGroup( bg );
	if ( bgl ) Ral_DestroyBindGroupLayout( bgl );
	Ral_DestroyTexture( tex );
	Ral_QueryMemoryBudget( b, &teardown );
	RAL_VK_LOG( SEV_INFO, "  teardown: RAL-tracked device-local footprint = %u KiB across %u allocation(s) [back to baseline]; vk-reported device-local used = %u MiB\n",
	        (unsigned)( b->ralDeviceLocalBytes >> 10 ), b->numAllocations, (unsigned)( teardown.deviceLocalUsed >> 20 ) );
	RAL_VK_LOG( SEV_INFO, "===== end RAL resource test =====\n" );
}
