// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
//
// ral_vulkan_resource.c — Vulkan backend: buffers, textures, samplers, bind
// groups (phase-7-ral-design.md §3.2-§3.5, §7). Phase 7.2:
//   * resources are real (VkBuffer / VkImage / VkImageView / VkSampler /
//     VkDescriptorSetLayout / VkDescriptorSet), backed by the suballocator;
//   * GPU mipmap generation via vkCmdBlitImage is a core Texture-upload
//     feature (Ral_TextureUploadAsync);
//   * uploads are SYNCHRONOUS (transient staging buffer + one-shot command +
//     fence wait) — they return an already-signaled fence. The async transfer
//     queue lands in Phase 7.3;
//   * Ral_Destroy* does vkDeviceWaitIdle + immediate destroy — the deferred-
//     destroy queue (§7.2) lands in Phase 7.3;
//   * descriptor sets come from one big backend-owned pool and are not freed
//     individually — Ral_DestroyBindGroup is a no-op until 7.3.

#include "ral_vulkan_internal.h"

// Practical bindless-array size used for layouts whose entry count is 0
// (unbounded). The device cap (caps.maxBindlessTextures) can be far larger;
// 7.2 picks a portable fixed size. The descriptor pool is sized to match.
#define RAL_VK_BINDLESS_LAYOUT_COUNT  4096u
#define RAL_VK_BINDLESS_POOL_SETS     4u

// scratch sizes for Ral_CreateBindGroup
#define RAL_VK_MAX_BG_WRITES   64u
#define RAL_VK_MAX_BG_IMAGES   256u
#define RAL_VK_MAX_BG_BUFFERS  64u

// ════════════════════════════════════════════════════════════════════════
// translation helpers
// ════════════════════════════════════════════════════════════════════════
VkFormat ralVk_TranslateFormat( ralFormat_t f ) {
	switch ( f ) {
	case RAL_FORMAT_R8_UNORM:             return VK_FORMAT_R8_UNORM;
	case RAL_FORMAT_R8G8_UNORM:           return VK_FORMAT_R8G8_UNORM;
	case RAL_FORMAT_R8G8B8A8_UNORM:       return VK_FORMAT_R8G8B8A8_UNORM;
	case RAL_FORMAT_R8G8B8A8_SRGB:        return VK_FORMAT_R8G8B8A8_SRGB;
	case RAL_FORMAT_B8G8R8A8_UNORM:       return VK_FORMAT_B8G8R8A8_UNORM;
	case RAL_FORMAT_B8G8R8A8_SRGB:        return VK_FORMAT_B8G8R8A8_SRGB;
	case RAL_FORMAT_A2B10G10R10_UNORM:    return VK_FORMAT_A2B10G10R10_UNORM_PACK32;
	case RAL_FORMAT_R16_UNORM:            return VK_FORMAT_R16_UNORM;
	case RAL_FORMAT_R16_SFLOAT:           return VK_FORMAT_R16_SFLOAT;
	case RAL_FORMAT_R16G16_SFLOAT:        return VK_FORMAT_R16G16_SFLOAT;
	case RAL_FORMAT_R16G16B16A16_SFLOAT:  return VK_FORMAT_R16G16B16A16_SFLOAT;
	case RAL_FORMAT_R11G11B10_UFLOAT:     return VK_FORMAT_B10G11R11_UFLOAT_PACK32;
	case RAL_FORMAT_R32_SFLOAT:           return VK_FORMAT_R32_SFLOAT;
	case RAL_FORMAT_R32G32B32_SFLOAT:     return VK_FORMAT_R32G32B32_SFLOAT;
	case RAL_FORMAT_R32G32B32A32_SFLOAT:  return VK_FORMAT_R32G32B32A32_SFLOAT;
	case RAL_FORMAT_D16_UNORM:            return VK_FORMAT_D16_UNORM;
	case RAL_FORMAT_D24_UNORM_S8_UINT:    return VK_FORMAT_D24_UNORM_S8_UINT;
	case RAL_FORMAT_D32_SFLOAT:           return VK_FORMAT_D32_SFLOAT;
	case RAL_FORMAT_D32_SFLOAT_S8_UINT:   return VK_FORMAT_D32_SFLOAT_S8_UINT;
	case RAL_FORMAT_BC1_RGBA_UNORM:       return VK_FORMAT_BC1_RGBA_UNORM_BLOCK;
	case RAL_FORMAT_BC1_RGBA_SRGB:        return VK_FORMAT_BC1_RGBA_SRGB_BLOCK;
	case RAL_FORMAT_BC3_UNORM:            return VK_FORMAT_BC3_UNORM_BLOCK;
	case RAL_FORMAT_BC3_SRGB:             return VK_FORMAT_BC3_SRGB_BLOCK;
	case RAL_FORMAT_BC4_UNORM:            return VK_FORMAT_BC4_UNORM_BLOCK;
	case RAL_FORMAT_BC5_UNORM:            return VK_FORMAT_BC5_UNORM_BLOCK;
	case RAL_FORMAT_BC6H_UFLOAT:          return VK_FORMAT_BC6H_UFLOAT_BLOCK;
	case RAL_FORMAT_BC7_UNORM:            return VK_FORMAT_BC7_UNORM_BLOCK;
	case RAL_FORMAT_BC7_SRGB:             return VK_FORMAT_BC7_SRGB_BLOCK;
	case RAL_FORMAT_ASTC_4x4_UNORM:       return VK_FORMAT_ASTC_4x4_UNORM_BLOCK;
	case RAL_FORMAT_ASTC_4x4_SRGB:        return VK_FORMAT_ASTC_4x4_SRGB_BLOCK;
	case RAL_FORMAT_ETC2_R8G8B8A8_UNORM:  return VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK;
	case RAL_FORMAT_ETC2_R8G8B8A8_SRGB:   return VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK;
	default:                              return VK_FORMAT_UNDEFINED;
	}
}

static VkImageAspectFlags ralVk_FormatAspect( ralFormat_t f ) {
	switch ( f ) {
	case RAL_FORMAT_D16_UNORM:
	case RAL_FORMAT_D32_SFLOAT:           return VK_IMAGE_ASPECT_DEPTH_BIT;
	case RAL_FORMAT_D24_UNORM_S8_UINT:
	case RAL_FORMAT_D32_SFLOAT_S8_UINT:   return VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
	default:                              return VK_IMAGE_ASPECT_COLOR_BIT;
	}
}

static qboolean ralVk_FormatIsDepthOrInteger( ralFormat_t f ) {
	switch ( f ) {
	case RAL_FORMAT_D16_UNORM: case RAL_FORMAT_D24_UNORM_S8_UINT:
	case RAL_FORMAT_D32_SFLOAT: case RAL_FORMAT_D32_SFLOAT_S8_UINT:
		return qtrue;
	default:
		return qfalse;
	}
}

static uint32_t ralVk_FormatBPP( ralFormat_t f ) {   // uncompressed only; bytes per pixel
	switch ( f ) {
	case RAL_FORMAT_R8_UNORM:                                  return 1;
	case RAL_FORMAT_R8G8_UNORM: case RAL_FORMAT_R16_UNORM: case RAL_FORMAT_R16_SFLOAT: case RAL_FORMAT_D16_UNORM:
		return 2;
	case RAL_FORMAT_R8G8B8A8_UNORM: case RAL_FORMAT_R8G8B8A8_SRGB: case RAL_FORMAT_B8G8R8A8_UNORM:
	case RAL_FORMAT_B8G8R8A8_SRGB: case RAL_FORMAT_A2B10G10R10_UNORM: case RAL_FORMAT_R16G16_SFLOAT:
	case RAL_FORMAT_R11G11B10_UFLOAT: case RAL_FORMAT_R32_SFLOAT: case RAL_FORMAT_D32_SFLOAT: case RAL_FORMAT_D24_UNORM_S8_UINT:
		return 4;
	case RAL_FORMAT_R16G16B16A16_SFLOAT:                       return 8;
	case RAL_FORMAT_R32G32B32_SFLOAT:                          return 12;   // vec3 — vertex-attribute-only on most hw (no optimal-tiling colour rendering)
	case RAL_FORMAT_R32G32B32A32_SFLOAT:                       return 16;
	default:                                                   return 4;   // best-effort fallback
	}
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

static VkImageUsageFlags ralVk_TextureUsage( ralTextureUsage_t u ) {
	// SAMPLED + TRANSFER are always on (default view sampling, upload, mip-gen blit, readback).
	VkImageUsageFlags v = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	if ( u & RAL_TEXTURE_USAGE_STORAGE                  )  v |= VK_IMAGE_USAGE_STORAGE_BIT;
	if ( u & RAL_TEXTURE_USAGE_COLOR_ATTACHMENT         )  v |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	if ( u & RAL_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT )  v |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
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
static VkDescriptorType ralVk_DescType( ralBindType_t t ) {
	switch ( t ) {
	case RAL_BIND_UNIFORM_BUFFER:  return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	case RAL_BIND_STORAGE_BUFFER:  return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	case RAL_BIND_STORAGE_TEXTURE: return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	case RAL_BIND_SAMPLER:         return VK_DESCRIPTOR_TYPE_SAMPLER;
	case RAL_BIND_SAMPLED_TEXTURE:
	case RAL_BIND_TEXTURE_ARRAY:
	default:                       return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
	}
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
// one-shot upload command helpers (Phase 7.3: per-queue, vkQueueSubmit2)
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
static void ralVk_SubmitUploadCmdAndWait( ralBackend_t *b, ralQueueType_t q, VkCommandBuffer cb, VkFence *keepFence ) {
	VkCommandBufferSubmitInfo cbi;
	VkSubmitInfo2             si2;
	VkFenceCreateInfo         fi;
	VkFence                   fence = VK_NULL_HANDLE;
	b->vk.EndCommandBuffer( cb );
	RAL_ZERO( fi ); fi.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	b->vk.CreateFence( b->device, &fi, NULL, &fence );
	RAL_ZERO( cbi ); cbi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO; cbi.commandBuffer = cb;
	RAL_ZERO( si2 ); si2.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2; si2.commandBufferInfoCount = 1; si2.pCommandBufferInfos = &cbi;
	ralVk_QueueSubmit2( b, q, &si2, fence );
	if ( fence != VK_NULL_HANDLE ) b->vk.WaitForFences( b->device, 1, &fence, VK_TRUE, ~0ull );
	else                           b->vk.DeviceWaitIdle( b->device );   // fence creation failed — fall back
	ralVk_QueueLock( b, q ); b->vk.FreeCommandBuffers( b->device, b->cmdPools[q], 1, &cb ); ralVk_QueueUnlock( b, q );
	if ( keepFence ) *keepFence = fence;
	else if ( fence != VK_NULL_HANDLE ) b->vk.DestroyFence( b->device, fence, NULL );
}

// Wraps a (real) VkFence in a ralFence_t the caller owns. fence may be
// VK_NULL_HANDLE (fence creation failed) → reports signaled.
//
// Phase 7.4c-submit-followup-present-2-fix3 — set ownsFence=qtrue. Both
// callers (Ral_BufferUploadAsync at line 403, Ral_TextureUploadAsync at
// line 719) pass a VkFence that was just created by
// ralVk_SubmitUploadCmdAndWait's CreateFence call + handed back via the
// keepFence out-param. Without ownsFence=qtrue, Ral_DestroyFence's
// ownership gate at sync.c:38 skipped the underlying VkFence destroy,
// leaking N fences per session (~15 textures × 1 fence each on arena1).
// Surfaced at vkDestroyDevice once fix3 unblocked the shutdown crash.
static ralFence_t *ralVk_WrapFence( ralBackend_t *b, VkFence fence ) {
	ralFence_t *f = (ralFence_t *)malloc( sizeof( *f ) );
	if ( !f ) { if ( fence != VK_NULL_HANDLE ) b->vk.DestroyFence( b->device, fence, NULL ); return NULL; }
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
	if ( !b || !ci || ci->size == 0 ) return NULL;

	buf = (ralBuffer_t *)malloc( sizeof( *buf ) );
	if ( !buf ) return NULL;
	RAL_ZERO( *buf );
	buf->header.refCount = 1;
	buf->backend         = b;
	buf->size            = ci->size;
	buf->memoryType      = ci->memory;

	RAL_ZERO( bci );
	bci.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bci.size        = ci->size;
	bci.usage       = ralVk_BufferUsage( ci->usage );
	bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	if ( b->vk.CreateBuffer( b->device, &bci, NULL, &buf->buffer ) != VK_SUCCESS ) {
		ri.Log( SEV_WARN, "[RAL] Ral_CreateBuffer: vkCreateBuffer failed (%llu bytes)\n", (unsigned long long)ci->size );
		free( buf ); return NULL;
	}
	b->vk.GetBufferMemoryRequirements( b->device, buf->buffer, &req );
	buf->alloc = ralVk_Alloc( b, req, ralVk_MemProps( ci->memory ) );
	if ( !buf->alloc ) { b->vk.DestroyBuffer( b->device, buf->buffer, NULL ); free( buf ); return NULL; }
	if ( b->vk.BindBufferMemory( b->device, buf->buffer, buf->alloc->memory, 0 ) != VK_SUCCESS ) {
		ri.Log( SEV_WARN, "[RAL] Ral_CreateBuffer: vkBindBufferMemory failed\n" );
		ralVk_Free( b, buf->alloc ); b->vk.DestroyBuffer( b->device, buf->buffer, NULL ); free( buf ); return NULL;
	}
	buf->hostVisible = ( buf->alloc->propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT  ) ? qtrue : qfalse;
	buf->coherent    = ( buf->alloc->propertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT ) ? qtrue : qfalse;
	ralVk_SetObjectName( b, (uint64_t)buf->buffer, VK_OBJECT_TYPE_BUFFER, ci->debugName );
	return buf;
}

void Ral_DestroyBuffer( ralBuffer_t *buf ) {
	ralBackend_t *b;
	if ( !buf ) return;
	if ( buf->header.refCount > 1 ) { buf->header.refCount--; return; }
	b = buf->backend;
	ralVk_DeferDestroy( b, RAL_RES_BUFFER, RAL_VK_H2U( buf->buffer ), 0, buf->alloc );
	free( buf );
}

void *Ral_MapBuffer( ralBuffer_t *buf ) {
	if ( !buf ) return NULL;
	if ( !buf->hostVisible ) { ri.Log( SEV_WARN, "[RAL] Ral_MapBuffer: buffer is not host-visible\n" ); return NULL; }
	return ralVk_Map( buf->alloc );
}
void Ral_UnmapBuffer( ralBuffer_t *buf ) { if ( buf ) ralVk_Unmap( buf->alloc ); }
void Ral_FlushBuffer( ralBuffer_t *buf, uint64_t offset, uint64_t size ) {
	if ( !buf || buf->coherent ) return;   // coherent → no flush needed
	ralVk_Flush( buf->alloc, (VkDeviceSize)offset, (VkDeviceSize)size );
}

ralFence_t *Ral_BufferUploadAsync( ralBuffer_t *buf, uint64_t offset, const void *data, uint64_t size ) {
	ralBackend_t        *b;
	ralBufferCreateInfo_t sci;
	ralBuffer_t          *staging;
	void                 *mapped;
	VkCommandBuffer       cb;
	VkBufferCopy          region;
	VkFence               fence = VK_NULL_HANDLE;
	ralQueueType_t        q;
	if ( !buf || !data || size == 0 ) return NULL;
	b = buf->backend;
	q = ralVk_UploadQueue( b );
	RAL_NOTE_ONCE( "[RAL] Ral_BufferUploadAsync runs on the transfer queue when available but still waits internally before returning -- async streaming (no internal wait) is Phase 7.15\n" );

	RAL_ZERO( sci ); sci.size = size; sci.usage = RAL_BUFFER_TRANSFER_SRC; sci.memory = RAL_MEMORY_HOST_COHERENT; sci.debugName = "ral-staging-buf-upload";
	staging = Ral_CreateBuffer( b, &sci );
	if ( !staging ) return NULL;
	mapped = Ral_MapBuffer( staging );
	if ( !mapped ) { Ral_DestroyBuffer( staging ); return NULL; }
	memcpy( mapped, data, (size_t)size );
	Ral_UnmapBuffer( staging );

	if ( !ralVk_BeginUploadCmd( b, q, &cb ) ) { Ral_DestroyBuffer( staging ); return NULL; }
	RAL_ZERO( region ); region.srcOffset = 0; region.dstOffset = offset; region.size = size;
	b->vk.CmdCopyBuffer( cb, staging->buffer, buf->buffer, 1, &region );
	// Queue-ownership release: if this ran on a dedicated transfer family, hand
	// `buf` to the graphics family. The matching acquire is the consumer's job
	// (renderer migration, Phase 7.4+) — until then nothing uses RAL buffers on
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
	ralVk_SubmitUploadCmdAndWait( b, q, cb, &fence );   // submits, waits, keeps the fence (signaled) for the caller
	Ral_DestroyBuffer( staging );
	return ralVk_WrapFence( b, fence );
}

// ════════════════════════════════════════════════════════════════════════
// Texture
// ════════════════════════════════════════════════════════════════════════
ralTexture_t *Ral_CreateTexture( ralBackend_t *b, const ralTextureCreateInfo_t *ci ) {
	VkImageCreateInfo     ici;
	VkImageViewCreateInfo vci;
	VkMemoryRequirements  req;
	ralTexture_t         *tex;
	uint32_t              layers, depth3d, mips;
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
	tex->width                = ci->width;
	tex->height               = ci->height ? ci->height : 1;
	tex->depthOrArrayLayers   = ci->depthOrArrayLayers ? ci->depthOrArrayLayers : 1;
	tex->mipLevels            = mips;
	tex->arrayLayers          = layers;
	tex->sampleCount          = ci->sampleCount ? ci->sampleCount : 1;
	tex->aspect               = ralVk_FormatAspect( ci->format );
	tex->currentLayout        = VK_IMAGE_LAYOUT_UNDEFINED;
	tex->ownsImage            = qtrue;   // Phase 7.4c-submit-A4: native RAL allocation owns the VkImage; Ral_AdoptTexture flips this to qfalse for adopted handles.
	if ( tex->vkFormat == VK_FORMAT_UNDEFINED ) {
		ri.Log( SEV_WARN, "[RAL] Ral_CreateTexture: unsupported ralFormat %d\n", (int)ci->format );
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
	ici.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	if ( b->vk.CreateImage( b->device, &ici, NULL, &tex->image ) != VK_SUCCESS ) {
		ri.Log( SEV_WARN, "[RAL] Ral_CreateTexture: vkCreateImage failed (%ux%u, %u mips, fmt %d)\n", tex->width, tex->height, mips, (int)ci->format );
		free( tex ); return NULL;
	}
	b->vk.GetImageMemoryRequirements( b->device, tex->image, &req );
	tex->alloc = ralVk_Alloc( b, req, ralVk_MemProps( ci->memory ) );
	if ( !tex->alloc ) { b->vk.DestroyImage( b->device, tex->image, NULL ); free( tex ); return NULL; }
	if ( b->vk.BindImageMemory( b->device, tex->image, tex->alloc->memory, 0 ) != VK_SUCCESS ) {
		ri.Log( SEV_WARN, "[RAL] Ral_CreateTexture: vkBindImageMemory failed\n" );
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
		ri.Log( SEV_WARN, "[RAL] Ral_CreateTexture: vkCreateImageView failed\n" );
		ralVk_Free( b, tex->alloc ); b->vk.DestroyImage( b->device, tex->image, NULL ); free( tex ); return NULL;
	}
	if ( ci->debugName ) {
		ralVk_SetObjectName( b, (uint64_t)tex->image,       VK_OBJECT_TYPE_IMAGE,      ci->debugName );
		ralVk_SetObjectName( b, (uint64_t)tex->defaultView, VK_OBJECT_TYPE_IMAGE_VIEW, ci->debugName );
	}
	return tex;
}

void Ral_DestroyTexture( ralTexture_t *tex ) {
	ralBackend_t *b;
	if ( !tex ) return;
	if ( tex->header.refCount > 1 ) { tex->header.refCount--; return; }
	b = tex->backend;
	// Phase 7.4c-submit-A4: ownsImage=qfalse on Ral_AdoptTexture-created wrappers
	// (the renderer's existing qvkCreateImage/qvkAllocateMemory/qvkCreateImageView
	// retain lifetime ownership of the underlying VkImage/VkImageView/VkDeviceMemory).
	// Free only the wrapper struct; defer-destroy of the backend objects belongs
	// to the caller's existing teardown path.
	if ( tex->ownsImage ) {
		ralVk_DeferDestroy( b, RAL_RES_IMAGE_AND_VIEW, RAL_VK_H2U( tex->image ), RAL_VK_H2U( tex->defaultView ), tex->alloc );
	}
	free( tex );
}

// Phase 7.4c-submit-A4 — adoption helper. Wraps an existing VkImage in a
// ralTexture_t with ownsImage=qfalse. `aspect` is the caller's
// VkImageAspectFlags bitmask (VK_IMAGE_ASPECT_COLOR_BIT or
// VK_IMAGE_ASPECT_DEPTH_BIT, etc) — stored verbatim on tex->aspect so
// ralImageMemoryBarrier_t builders can echo it. defaultView is left
// VK_NULL_HANDLE because adopted-texture consumers in 7.4c-submit-A4 only
// reference the VkImage; if a future site needs a view it can populate
// tex->defaultView directly or use Ral_CreateTextureView on the wrapper.
ralTexture_t *Ral_AdoptTexture( ralBackend_t *b,
                                void *externalImage,
                                uint32_t width, uint32_t height,
                                uint32_t aspect,
                                const char *debugName )
{
	ralTexture_t *tex;
	if ( !b || !externalImage ) return NULL;
	tex = (ralTexture_t *)malloc( sizeof( *tex ) );
	if ( !tex ) return NULL;
	RAL_ZERO( *tex );
	tex->header.refCount = 1;
	tex->backend         = b;
	tex->image           = (VkImage)externalImage;
	tex->alloc           = NULL;
	tex->defaultView     = VK_NULL_HANDLE;
	tex->vkFormat        = VK_FORMAT_UNDEFINED;
	tex->ralFormat       = RAL_FORMAT_UNDEFINED;
	tex->type            = RAL_TEXTURE_2D;
	tex->width           = width;
	tex->height          = height ? height : 1;
	tex->depthOrArrayLayers = 1;
	tex->mipLevels       = 1;
	tex->arrayLayers     = 1;
	tex->sampleCount     = 1;
	tex->aspect          = (VkImageAspectFlags)aspect;
	tex->currentLayout   = VK_IMAGE_LAYOUT_UNDEFINED;
	tex->ownsImage       = qfalse;
	if ( debugName ) ralVk_SetObjectName( b, (uint64_t)tex->image, VK_OBJECT_TYPE_IMAGE, debugName );
	return tex;
}

void *Ral_GetTextureImageHandle( const ralTexture_t *tex ) {
	return tex ? (void *)tex->image : NULL;
}

ralTextureView_t *Ral_CreateTextureView( ralBackend_t *b, const ralTextureViewCreateInfo_t *ci ) {
	VkImageViewCreateInfo vci;
	ralTextureView_t     *view;
	const ralTexture_t   *tex;
	uint32_t              levelCount, layerCount;
	if ( !b || !ci || !ci->texture ) return NULL;
	tex = ci->texture;
	levelCount = ci->mipLevelCount   ? ci->mipLevelCount   : ( tex->mipLevels   - ci->baseMipLevel  );
	layerCount = ci->arrayLayerCount ? ci->arrayLayerCount : ( tex->arrayLayers - ci->baseArrayLayer );

	view = (ralTextureView_t *)malloc( sizeof( *view ) );
	if ( !view ) return NULL;
	RAL_ZERO( *view );
	view->header.refCount = 1;
	view->backend         = b;
	view->texture         = tex;

	RAL_ZERO( vci );
	vci.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	vci.image                           = tex->image;
	vci.viewType                        = ralVk_ViewType( ci->viewType );
	vci.format                          = ( ci->format != RAL_FORMAT_UNDEFINED ) ? ralVk_TranslateFormat( ci->format ) : tex->vkFormat;
	vci.subresourceRange.aspectMask     = tex->aspect;
	vci.subresourceRange.baseMipLevel   = ci->baseMipLevel;
	vci.subresourceRange.levelCount     = levelCount;
	vci.subresourceRange.baseArrayLayer = ci->baseArrayLayer;
	vci.subresourceRange.layerCount     = layerCount;
	if ( b->vk.CreateImageView( b->device, &vci, NULL, &view->view ) != VK_SUCCESS ) {
		ri.Log( SEV_WARN, "[RAL] Ral_CreateTextureView: vkCreateImageView failed\n" );
		free( view ); return NULL;
	}
	return view;
}

void Ral_DestroyTextureView( ralTextureView_t *view ) {
	ralBackend_t *b;
	if ( !view ) return;
	if ( view->header.refCount > 1 ) { view->header.refCount--; return; }
	b = view->backend;
	ralVk_DeferDestroy( b, RAL_RES_IMAGE_VIEW, RAL_VK_H2U( view->view ), 0, NULL );
	free( view );
}

// Ral_TextureUploadAsync — sync upload of `region`, plus (when region is mip 0
// of a multi-mip texture whose format supports it) GPU mipmap generation via
// vkCmdBlitImage. Returns an already-signaled fence.
ralFence_t *Ral_TextureUploadAsync( ralTexture_t *tex, const ralTextureUploadDesc_t *region ) {
	ralBackend_t        *b;
	ralBufferCreateInfo_t sci;
	ralBuffer_t          *staging;
	void                 *mapped;
	VkCommandBuffer       cb;
	VkBufferImageCopy     bic;
	VkFence               fence = VK_NULL_HANDLE;
	ralQueueType_t        q;
	uint32_t              mip0w, mip0h, level;
	qboolean              genMips, blitColor;
	VkFilter              blitFilter;
	if ( !tex || !region || !region->data || region->dataSize == 0 ) return NULL;
	b = tex->backend;
	RAL_NOTE_ONCE( "[RAL] Ral_TextureUploadAsync runs on the transfer queue when no GPU mip-gen is needed (mip-gen uses vkCmdBlitImage, which stays on graphics); still waits internally before returning -- async streaming is Phase 7.15\n" );

	mip0w = tex->width  >> region->mipLevel; if ( mip0w == 0 ) mip0w = 1;
	mip0h = tex->height >> region->mipLevel; if ( mip0h == 0 ) mip0h = 1;

	genMips   = ( region->mipLevel == 0 && region->arrayLayer == 0 && tex->mipLevels > 1
	              && tex->aspect == VK_IMAGE_ASPECT_COLOR_BIT
	              && b->formatBlitGen[ tex->ralFormat ] && tex->arrayLayers == 1 ) ? qtrue : qfalse;
	blitColor = !ralVk_FormatIsDepthOrInteger( tex->ralFormat );
	blitFilter = blitColor ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
	if ( !genMips && region->mipLevel == 0 && tex->mipLevels > 1 && !b->formatBlitGen[ tex->ralFormat ] )
		RAL_NOTE_ONCE( "[RAL] Ral_TextureUploadAsync: format lacks BLIT support -- GPU mip-gen skipped; caller must supply all mips\n" );
	// blit (mip-gen) must stay on graphics — transfer queues don't support BLIT.
	q = genMips ? RAL_QUEUE_GRAPHICS : ralVk_UploadQueue( b );

	RAL_ZERO( sci ); sci.size = region->dataSize; sci.usage = RAL_BUFFER_TRANSFER_SRC; sci.memory = RAL_MEMORY_HOST_COHERENT; sci.debugName = "ral-staging-tex-upload";
	staging = Ral_CreateBuffer( b, &sci );
	if ( !staging ) return NULL;
	mapped = Ral_MapBuffer( staging );
	if ( !mapped ) { Ral_DestroyBuffer( staging ); return NULL; }
	memcpy( mapped, region->data, (size_t)region->dataSize );
	Ral_UnmapBuffer( staging );

	if ( !ralVk_BeginUploadCmd( b, q, &cb ) ) { Ral_DestroyBuffer( staging ); return NULL; }

	// mip `region->mipLevel`: UNDEFINED → TRANSFER_DST, copy from staging.
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
	} else {
		// single mip uploaded: TRANSFER_DST → SHADER_READ_ONLY.
		// dstStage depends on the recording queue: FRAGMENT_SHADER_BIT is
		// invalid on transfer queues (they only support TRANSFER + SPARSE).
		// For transfer-queue uploads, use BOTTOM_OF_PIPE here; the consumer's
		// graphics-queue acquire-barrier (Phase 7.4+ renderer migration)
		// re-makes the layout visible to fragment-sampling. For graphics-
		// queue uploads (mip-gen / depth-format paths), keep FRAGMENT_SHADER.
		VkPipelineStageFlags dstStage = ( q == RAL_QUEUE_TRANSFER ) ? VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT : VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		VkAccessFlags        dstAcc   = ( q == RAL_QUEUE_TRANSFER ) ? 0u                                    : VK_ACCESS_SHADER_READ_BIT;
		ralVk_ImgBarrier( b, cb, tex->image, tex->aspect, region->mipLevel, 1, region->arrayLayer, 1,
		                  VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		                  VK_PIPELINE_STAGE_TRANSFER_BIT, dstStage,
		                  VK_ACCESS_TRANSFER_WRITE_BIT, dstAcc );
		tex->currentLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;   // approximation when not all mips written
	}
	// TODO 7.4: when `q == RAL_QUEUE_TRANSFER` and the transfer family differs
	// from graphics, emit a queue-ownership release of the image here (paired
	// acquire on the graphics queue is the consumer's job). 7.3 has no
	// consumer of transfer-uploaded textures, so the release is omitted.
	ralVk_SubmitUploadCmdAndWait( b, q, cb, &fence );
	Ral_DestroyBuffer( staging );
	return ralVk_WrapFence( b, fence );
}

// ════════════════════════════════════════════════════════════════════════
// Sampler
// ════════════════════════════════════════════════════════════════════════
ralSampler_t *Ral_CreateSampler( ralBackend_t *b, const ralSamplerCreateInfo_t *ci ) {
	VkSamplerCreateInfo  sci;
	ralSampler_t        *s;
	if ( !b || !ci ) return NULL;
	s = (ralSampler_t *)malloc( sizeof( *s ) );
	if ( !s ) return NULL;
	RAL_ZERO( *s );
	s->header.refCount = 1; s->backend = b;
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
	sci.borderColor      = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
	if ( b->vk.CreateSampler( b->device, &sci, NULL, &s->sampler ) != VK_SUCCESS ) {
		ri.Log( SEV_WARN, "[RAL] Ral_CreateSampler: vkCreateSampler failed\n" );
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
	ralVk_DeferDestroy( b, RAL_RES_SAMPLER, RAL_VK_H2U( s->sampler ), 0, NULL );
	free( s );
}

// ════════════════════════════════════════════════════════════════════════
// BindGroupLayout / BindGroup
// ════════════════════════════════════════════════════════════════════════
ralBindGroupLayout_t *Ral_CreateBindGroupLayout( ralBackend_t *b, const ralBindGroupLayoutCreateInfo_t *ci ) {
	VkDescriptorSetLayoutBinding          binds[ RAL_VK_MAX_LAYOUT_ENTRIES ];
	VkDescriptorBindingFlags              bflags[ RAL_VK_MAX_LAYOUT_ENTRIES ];
	VkDescriptorSetLayoutBindingFlagsCreateInfo fci;
	VkDescriptorSetLayoutCreateInfo       lci;
	ralBindGroupLayout_t                 *L;
	uint32_t                              i, lastUnbounded = 0xFFFFFFFFu;
	qboolean                              anyUpdateAfterBind = qfalse;
	if ( !b || !ci || ci->numEntries == 0 || ci->numEntries > RAL_VK_MAX_LAYOUT_ENTRIES ) {
		ri.Log( SEV_WARN, "[RAL] Ral_CreateBindGroupLayout: bad/too-many entries (%u)\n", ci ? ci->numEntries : 0u );
		return NULL;
	}
	if ( ci->bindless ) {
		for ( i = 0; i < ci->numEntries; i++ ) if ( ci->entries[i].count == 0 ) { lastUnbounded = i; }
		if ( lastUnbounded != 0xFFFFFFFFu && !b->caps.bindlessTextures ) {
			ri.Log( SEV_WARN, "[RAL] Ral_CreateBindGroupLayout: bindless layout requested but caps.bindlessTextures is false\n" );
			return NULL;   // ralUnsupported equivalent for a handle-returning call
		}
	}

	L = (ralBindGroupLayout_t *)malloc( sizeof( *L ) );
	if ( !L ) return NULL;
	RAL_ZERO( *L );
	L->header.refCount = 1; L->backend = b; L->bindless = ci->bindless; L->numEntries = ci->numEntries;
	L->ownsLayout = qtrue;   // Phase 7.4c-bindgroup-pre: standalone create owns the VkDescriptorSetLayout.

	for ( i = 0; i < ci->numEntries; i++ ) {
		const ralBindEntry_t *e = &ci->entries[i];
		uint32_t cnt = ( e->count == 0 ) ? RAL_VK_BINDLESS_LAYOUT_COUNT : e->count;
		if ( e->count == 0 && cnt > b->caps.maxBindlessTextures && b->caps.maxBindlessTextures > 0 ) cnt = b->caps.maxBindlessTextures;
		RAL_ZERO( binds[i] );
		binds[i].binding         = e->binding;
		binds[i].descriptorType  = ralVk_DescType( e->type );
		binds[i].descriptorCount = cnt;
		binds[i].stageFlags      = ralVk_StageFlags( e->stageFlags );
		bflags[i] = 0;
		if ( ci->bindless && e->count == 0 ) {
			bflags[i] = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;
			anyUpdateAfterBind = qtrue;
		}
		L->entries[i].binding        = e->binding;
		L->entries[i].vkType         = binds[i].descriptorType;
		L->entries[i].count          = e->count;
		L->entries[i].effectiveCount = cnt;
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
		ri.Log( SEV_WARN, "[RAL] Ral_CreateBindGroupLayout: vkCreateDescriptorSetLayout failed\n" );
		free( L ); return NULL;
	}
	ralVk_SetObjectName( b, (uint64_t)L->layout, VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT, ci->debugName );
	return L;
}

// Phase 7.4c-bindgroup-pre — wrap a caller-owned VkDescriptorSetLayout.
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
	if ( !b || !externalLayout || numEntries > RAL_VK_MAX_LAYOUT_ENTRIES ) {
		ri.Log( SEV_WARN, "[RAL] Ral_AdoptBindGroupLayout: bad args (externalLayout=%p, numEntries=%u, max=%u)\n",
		        externalLayout, numEntries, (unsigned)RAL_VK_MAX_LAYOUT_ENTRIES );
		return NULL;
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
		L->entries[i].vkType         = ralVk_DescType( entries[i].type );
		L->entries[i].count          = entries[i].count;
		L->entries[i].effectiveCount = ( entries[i].count == 0 ) ? 1u : entries[i].count;
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
	// Phase 7.4c-bindgroup-pre: only defer-destroy the VkDescriptorSetLayout
	// when RAL created it. Adopted layouts (ownsLayout=qfalse) belong to the
	// caller — destroying them here would double-free on the renderer's own
	// vkDestroyDescriptorSetLayout call.
	if ( layout->ownsLayout ) {
		ralVk_DeferDestroy( b, RAL_RES_DESC_SET_LAYOUT, RAL_VK_H2U( layout->layout ), 0, NULL );
	}
	(void)b;
	free( layout );
}

ralBindGroup_t *Ral_CreateBindGroup( ralBackend_t *b, const ralBindGroupCreateInfo_t *ci ) {
	VkDescriptorSetAllocateInfo dai;
	VkWriteDescriptorSet        writes[ RAL_VK_MAX_BG_WRITES ];
	VkDescriptorImageInfo       imgs[ RAL_VK_MAX_BG_IMAGES ];
	VkDescriptorBufferInfo      bufs[ RAL_VK_MAX_BG_BUFFERS ];
	uint32_t                    nw = 0, ni = 0, nb = 0, v;
	ralBindGroup_t             *bg;
	VkResult                    r;
	if ( !b || !ci || !ci->layout ) return NULL;

	bg = (ralBindGroup_t *)malloc( sizeof( *bg ) );
	if ( !bg ) return NULL;
	RAL_ZERO( *bg );
	bg->header.refCount = 1; bg->backend = b; bg->layout = ci->layout;
	bg->ownsSet = qtrue;   // Phase 7.4c-bindgroup: native RAL allocation owns the set (Ral_AdoptBindGroup flips this to qfalse for adopted handles).

	RAL_ZERO( dai );
	dai.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	dai.descriptorPool     = b->descriptorPool;
	dai.descriptorSetCount = 1;
	dai.pSetLayouts        = &ci->layout->layout;
	r = b->vk.AllocateDescriptorSets( b->device, &dai, &bg->set );
	if ( r != VK_SUCCESS ) {
		ri.Log( SEV_WARN, "[RAL] Ral_CreateBindGroup: vkAllocateDescriptorSets failed (VkResult %d -- pool may be exhausted)\n", (int)r );
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
		w->descriptorType  = ralVk_DescType( val->type );
		switch ( val->type ) {
		case RAL_BIND_UNIFORM_BUFFER:
		case RAL_BIND_STORAGE_BUFFER:
			if ( nb >= RAL_VK_MAX_BG_BUFFERS || !val->buffer ) continue;
			RAL_ZERO( bufs[nb] );
			bufs[nb].buffer = val->buffer->buffer;
			bufs[nb].offset = val->bufferOffset;
			bufs[nb].range  = ( val->bufferRange == 0 ) ? VK_WHOLE_SIZE : val->bufferRange;
			w->descriptorCount = 1; w->pBufferInfo = &bufs[nb]; nb++;
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
		case RAL_BIND_TEXTURE_ARRAY: {
			uint32_t k, base = ni;
			if ( !val->textureArray || val->textureArrayCount == 0 ) continue;
			for ( k = 0; k < val->textureArrayCount && ni < RAL_VK_MAX_BG_IMAGES; k++ ) {
				if ( !val->textureArray[k] ) break;
				RAL_ZERO( imgs[ni] ); imgs[ni].imageView = val->textureArray[k]->view; imgs[ni].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL; ni++;
			}
			if ( ni == base ) continue;
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
	// Phase 7.4c-bindgroup: only defer-destroy the underlying VkDescriptorSet
	// when RAL owns it. Adopted sets (ownsSet=qfalse) belong to the renderer's
	// own descriptor pool — vkFreeDescriptorSets would race with / double-free
	// against the existing vkResetDescriptorPool / vk_destroy_descriptor_pools
	// path. Wrapper struct is freed in both cases.
	if ( g->ownsSet ) {
		// 7.3: the pool has FREE_DESCRIPTOR_SET_BIT — the set is returned to the
		// pool via vkFreeDescriptorSets after the deferred-destroy delay.
		ralVk_DeferDestroy( b, RAL_RES_DESC_SET, RAL_VK_H2U( g->set ), 0, NULL );
	}
	(void)b;
	free( g );
}


// Phase 7.4c-bindgroup: native-handle accessor for parallel-paths bind-side
// callers that need to reverse-look up adopted wrappers by VkDescriptorSet.
void *Ral_GetBindGroupHandle( const ralBindGroup_t *g ) {
	return g ? (void *)g->set : NULL;
}


// Phase 7.4c-bindgroup. Mirrors Ral_AdoptBindGroupLayout's contract: the
// caller's descriptor pool owns the lifetime of `externalSet`, the wrapper
// is just metadata + a stable RAL handle that the renderer can pass to
// Ral_CmdBindBindGroup. ownsSet=qfalse so Ral_DestroyBindGroup skips the
// vkFreeDescriptorSets call. layout must be a previously-adopted (or RAL-
// created) ralBindGroupLayout_t whose binding shape matches the writes the
// caller already did against externalSet.
// ════════════════════════════════════════════════════════════════════════
// Phase 7.4c-submit-A2 — ralPipelineLayout_t / ralRenderPass_t /
// ralFramebuffer_t adoption helpers. See ral_resource.h docblocks for
// the parallel-paths lifetime rationale. ownsHandle=qfalse on adopted
// wrappers; Ral_Destroy* frees only the wrapper, the underlying Vk handle
// stays owned by the renderer's existing teardown path.
// ════════════════════════════════════════════════════════════════════════

ralPipelineLayout_t *Ral_AdoptPipelineLayout( ralBackend_t *b, void *externalLayout, const char *debugName ) {
	ralPipelineLayout_t *pl;
	if ( !b || !externalLayout ) return NULL;
	pl = (ralPipelineLayout_t *)malloc( sizeof( *pl ) );
	if ( !pl ) return NULL;
	RAL_ZERO( *pl );
	pl->backend    = b;
	pl->vkHandle   = (VkPipelineLayout)externalLayout;
	pl->ownsHandle = qfalse;
	if ( debugName ) ralVk_SetObjectName( b, (uint64_t)pl->vkHandle, VK_OBJECT_TYPE_PIPELINE_LAYOUT, debugName );
	return pl;
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

ralRenderPass_t *Ral_AdoptRenderPass( ralBackend_t *b, void *externalRenderPass, const char *debugName ) {
	ralRenderPass_t *rp;
	if ( !b || !externalRenderPass ) return NULL;
	rp = (ralRenderPass_t *)malloc( sizeof( *rp ) );
	if ( !rp ) return NULL;
	RAL_ZERO( *rp );
	rp->backend    = b;
	rp->vkHandle   = (VkRenderPass)externalRenderPass;
	rp->ownsHandle = qfalse;
	if ( debugName ) ralVk_SetObjectName( b, (uint64_t)rp->vkHandle, VK_OBJECT_TYPE_RENDER_PASS, debugName );
	return rp;
}

void Ral_DestroyRenderPass( ralRenderPass_t *rp ) {
	if ( !rp ) return;
	// Adopted-only path: ownsHandle is always qfalse for renderer adoption.
	// Future native RAL ownership would call b->vk.DestroyRenderPass here.
	free( rp );
}

void *Ral_GetRenderPassHandle( const ralRenderPass_t *rp ) {
	return rp ? (void *)rp->vkHandle : NULL;
}

ralFramebuffer_t *Ral_AdoptFramebuffer( ralBackend_t *b, void *externalFramebuffer, const char *debugName ) {
	ralFramebuffer_t *fb;
	if ( !b || !externalFramebuffer ) return NULL;
	fb = (ralFramebuffer_t *)malloc( sizeof( *fb ) );
	if ( !fb ) return NULL;
	RAL_ZERO( *fb );
	fb->backend    = b;
	fb->vkHandle   = (VkFramebuffer)externalFramebuffer;
	fb->ownsHandle = qfalse;
	if ( debugName ) ralVk_SetObjectName( b, (uint64_t)fb->vkHandle, VK_OBJECT_TYPE_FRAMEBUFFER, debugName );
	return fb;
}

void Ral_DestroyFramebuffer( ralFramebuffer_t *fb ) {
	if ( !fb ) return;
	free( fb );
}

void *Ral_GetFramebufferHandle( const ralFramebuffer_t *fb ) {
	return fb ? (void *)fb->vkHandle : NULL;
}


ralBindGroup_t *Ral_AdoptBindGroup( ralBackend_t *b,
                                    void *externalSet,
                                    const ralBindGroupLayout_t *layout,
                                    const char *debugName ) {
	ralBindGroup_t *g;
	if ( !b || !externalSet || !layout ) {
		ri.Log( SEV_WARN, "[RAL] Ral_AdoptBindGroup: bad args (b=%p, externalSet=%p, layout=%p)\n",
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
	if ( debugName ) {
		ralVk_SetObjectName( b, (uint64_t)g->set, VK_OBJECT_TYPE_DESCRIPTOR_SET, debugName );
	}
	return g;
}

// Phase 7.4a: `tex == NULL` is a no-op clear. Vulkan doesn't have an
// "unwrite descriptor" operation; writing VK_NULL_HANDLE as the imageView is
// invalid even with PARTIALLY_BOUND (which only protects UNINITIALIZED
// descriptors, not explicit-null writes). The bind-group is destroyed on
// teardown either way, and PARTIALLY_BOUND + UPDATE_AFTER_BIND mean a stale
// descriptor (left pointing at a now-destroyed VkImageView) is OK provided
// no shader accesses it — which is enforced in 7.4a by the bindless table
// being unused. A real "evict + reuse slot" path lands with 7.4c when the
// renderer starts consuming the bindless set and needs slot-recycling
// semantics.
void Ral_BindGroupSetTextureAt( ralBindGroup_t *g, uint32_t slot, ralTexture_t *tex ) {
	VkWriteDescriptorSet  w;
	VkDescriptorImageInfo img;
	ralBackend_t         *b;
	uint32_t              i, binding = 0xFFFFFFFFu;
	if ( !g || !tex ) return;     // NULL tex: leave the slot stale (see comment above)
	b = g->backend;
	for ( i = 0; g->layout && i < g->layout->numEntries; i++ )
		if ( g->layout->entries[i].vkType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE ) { binding = g->layout->entries[i].binding; break; }
	if ( binding == 0xFFFFFFFFu ) { ri.Log( SEV_WARN, "[RAL] Ral_BindGroupSetTextureAt: layout has no SAMPLED_IMAGE binding\n" ); return; }
	RAL_ZERO( img );
	img.imageView   = tex->defaultView;
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
}

// Phase 7.4a interop bridge — renderervk needs raw VkImage / VkImageView /
// VkDevice handles for the parallel-paths migration model. These are
// backend-internal accessors (NOT part of the public RAL surface), exposed
// only to code statically linked against the ral_vulkan static lib (the
// renderer DLL). They go away in 7.4c+ when descriptor binding migrates onto
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
	dpi.flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT          // Phase 7.3: per-set free path
	                  | ( b->haveDescriptorIndexing ? VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT : 0u );
	dpi.maxSets       = 4096u;
	dpi.poolSizeCount = (uint32_t)( sizeof( sizes ) / sizeof( sizes[0] ) );
	dpi.pPoolSizes    = sizes;
	if ( b->vk.CreateDescriptorPool( b->device, &dpi, NULL, &b->descriptorPool ) != VK_SUCCESS ) {
		ri.Log( SEV_WARN, "[RAL] ralVk_InitResourceLayer: vkCreateDescriptorPool failed\n" );
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
		ralVkAllocation_t *a = b->allocations;
		ri.Log( SEV_WARN, "[RAL] ralVk_ShutdownResourceLayer: %u allocation(s) still live -- freeing\n", b->numAllocations );
		while ( a ) { ralVkAllocation_t *n = a->next; if ( a->mapped ) b->vk.UnmapMemory( b->device, a->memory ); b->vk.FreeMemory( b->device, a->memory, NULL ); free( a ); a = n; }
		b->allocations = NULL; b->numAllocations = 0; b->ralDeviceLocalBytes = 0; b->ralHostVisibleBytes = 0;
	}
}

// ════════════════════════════════════════════════════════════════════════
// \ral_dump resource — exercise the resource layer end-to-end (PART G)
// ════════════════════════════════════════════════════════════════════════
// Copies a small clamped region (≤ 4×4 texels) of `mip` into `out` so we can
// inspect a few pixels without a mip-sized staging buffer.
static void ralVk_ReadbackMip( ralBackend_t *b, ralTexture_t *tex, uint32_t mip, byte *out, uint32_t outSize ) {
	ralBufferCreateInfo_t bci;
	ralBuffer_t          *rb;
	VkCommandBuffer       cb;
	VkBufferImageCopy     bic;
	void                 *mapped;
	uint32_t              mw  = ( tex->width  >> mip ) ? ( tex->width  >> mip ) : 1u;
	uint32_t              mh  = ( tex->height >> mip ) ? ( tex->height >> mip ) : 1u;
	uint32_t              bpp = ralVk_FormatBPP( tex->ralFormat );
	uint32_t              ew  = ( mw < 4u ) ? mw : 4u;
	uint32_t              eh  = ( mh < 4u ) ? mh : 4u;
	uint32_t              copyBytes = ew * eh * bpp;
	uint32_t              bufBytes  = ( copyBytes > outSize ) ? copyBytes : outSize;
	if ( bufBytes < 64u ) bufBytes = 64u;
	memset( out, 0, outSize );
	RAL_ZERO( bci ); bci.size = bufBytes; bci.usage = RAL_BUFFER_TRANSFER_DST; bci.memory = RAL_MEMORY_HOST_COHERENT; bci.debugName = "ral-test-readback";
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
	ralVk_ImgBarrier( b, cb, tex->image, tex->aspect, 0, tex->mipLevels, 0, tex->arrayLayers,
	                  VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
	                  VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
	                  VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_SHADER_READ_BIT );
	ralVk_SubmitUploadCmdAndWait( b, RAL_QUEUE_GRAPHICS, cb, NULL );
	tex->currentLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	mapped = Ral_MapBuffer( rb );
	if ( mapped ) memcpy( out, mapped, ( copyBytes < outSize ) ? copyBytes : outSize );
	Ral_UnmapBuffer( rb );
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

	ri.Log( SEV_INFO, "===== RAL resource test (Phase 7.2) =====\n" );
	Ral_QueryMemoryBudget( b, &before );

	RAL_ZERO( tci );
	tci.type = RAL_TEXTURE_2D; tci.format = RAL_FORMAT_R8G8B8A8_UNORM;
	tci.width = W; tci.height = H; tci.depthOrArrayLayers = 1; tci.mipLevels = 0; tci.sampleCount = 1;
	tci.usage = RAL_TEXTURE_USAGE_SAMPLED; tci.memory = RAL_MEMORY_DEVICE_LOCAL; tci.debugName = "ral-test-tex";
	tex = Ral_CreateTexture( b, &tci );
	if ( !tex ) { ri.Log( SEV_WARN, "  Ral_CreateTexture failed\n" ); ri.Log( SEV_INFO, "===== end RAL resource test =====\n" ); return; }
	ri.Log( SEV_INFO, "  texture: %ux%u RGBA8_UNORM, %u mip levels, GPU-mip-gen=%s\n",
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
	if ( f ) { ri.Log( SEV_INFO, "  Ral_TextureUploadAsync: ok (mip 0 uploaded%s)\n", tex->mipLevels > 1 ? " + GPU mips generated" : "" ); Ral_DestroyFence( f ); }
	else     { ri.Log( SEV_WARN, "  Ral_TextureUploadAsync failed\n" ); }
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
		ri.Log( SEV_INFO, "  mip %u (%ux%u) pixel[0,0] = R%u G%u B%u A%u%s\n",
		        m, mw, mh, px[0], px[1], px[2], px[3],
		        ( idx == 0 ) ? "  (expect ~127/0/~127/255 = averaged red+blue)" : "" );
	}

	// bindless BindGroup
	RAL_ZERO( be ); be.binding = 0; be.type = RAL_BIND_TEXTURE_ARRAY; be.count = 0; be.stageFlags = RAL_STAGE_FRAGMENT;
	RAL_ZERO( lci ); lci.entries = &be; lci.numEntries = 1; lci.bindless = qtrue; lci.debugName = "ral-test-bindless-layout";
	bgl = Ral_CreateBindGroupLayout( b, &lci );
	if ( !bgl ) {
		ri.Log( SEV_INFO, "  bindless: skipped (caps.bindlessTextures=%s)\n", b->caps.bindlessTextures ? "yes-but-layout-failed" : "no" );
	} else {
		ralBindGroupCreateInfo_t bci2;
		RAL_ZERO( bci2 ); bci2.layout = bgl; bci2.values = NULL; bci2.numValues = 0; bci2.debugName = "ral-test-bindless-bg";
		bg = Ral_CreateBindGroup( b, &bci2 );
		if ( !bg ) {
			ri.Log( SEV_WARN, "  bindless BindGroup creation failed\n" );
		} else {
			Ral_BindGroupSetTextureAt( bg, 0, tex );
			Ral_BindGroupSetTextureAt( bg, 1, tex );
			ri.Log( SEV_INFO, "  bindless BindGroup: %u-slot SAMPLED_IMAGE array created; slots 0+1 populated with the test texture\n", RAL_VK_BINDLESS_LAYOUT_COUNT );
		}
	}

	ri.Log( SEV_INFO, "  memory: vk-reported device-local used: before=%u MiB  after=%u MiB\n",
	        (unsigned)( before.deviceLocalUsed >> 20 ), (unsigned)( after.deviceLocalUsed >> 20 ) );
	ri.Log( SEV_INFO, "  memory: RAL-tracked device-local footprint = %u KiB across %u allocation(s) (texture + mip chain still live)\n",
	        (unsigned)( b->ralDeviceLocalBytes >> 10 ), b->numAllocations );

	// teardown
	if ( bg )  Ral_DestroyBindGroup( bg );
	if ( bgl ) Ral_DestroyBindGroupLayout( bgl );
	Ral_DestroyTexture( tex );
	Ral_QueryMemoryBudget( b, &teardown );
	ri.Log( SEV_INFO, "  teardown: RAL-tracked device-local footprint = %u KiB across %u allocation(s) [back to baseline]; vk-reported device-local used = %u MiB\n",
	        (unsigned)( b->ralDeviceLocalBytes >> 10 ), b->numAllocations, (unsigned)( teardown.deviceLocalUsed >> 20 ) );
	ri.Log( SEV_INFO, "===== end RAL resource test =====\n" );
}
