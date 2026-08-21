// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
//
// ral_resource.h — buffers, textures, samplers, bind groups.
// Part of the Wired RAL v1 surface (docs/phase-7-ral-design.md §3.2-§3.5, §4).

#ifndef WIRED_RAL_RESOURCE_H
#define WIRED_RAL_RESOURCE_H

#include "ral_types.h"
#include "ral_buffer_map.h"
#include "ral_transfer.h"

#ifdef __cplusplus
extern "C" {
#endif

// ════════════════════════════════════════════════════════════════════════
// Buffers (§3.2)
// ════════════════════════════════════════════════════════════════════════
typedef enum {
	RAL_BUFFER_VERTEX       = 1 << 0,
	RAL_BUFFER_INDEX        = 1 << 1,
	RAL_BUFFER_UNIFORM      = 1 << 2,
	RAL_BUFFER_STORAGE      = 1 << 3,
	RAL_BUFFER_INDIRECT     = 1 << 4,
	RAL_BUFFER_TRANSFER_SRC = 1 << 5,
	RAL_BUFFER_TRANSFER_DST = 1 << 6,
	// Portable/WebGPU staging capabilities: MAP_READ is paired with
	// TRANSFER_DST; MAP_WRITE is paired with TRANSFER_SRC. Do not use these as
	// persistent mappings for vertex/uniform/storage resources.
	RAL_BUFFER_MAP_READ     = 1 << 7,
	RAL_BUFFER_MAP_WRITE    = 1 << 8
} ralBufferUsage_t;

typedef enum {
	RAL_MEMORY_DEVICE_LOCAL,    // GPU-only, fastest GPU access
	RAL_MEMORY_HOST_VISIBLE,    // mappable, slower GPU access (needs flush)
	RAL_MEMORY_HOST_COHERENT,   // mappable + no flush needed
	RAL_MEMORY_LAZY_ALLOC       // tile-GPU transient, no host backing
} ralMemoryType_t;

typedef struct {
	uint64_t         size;
	ralBufferUsage_t usage;     // bitmask of RAL_BUFFER_*
	ralMemoryType_t  memory;
	const char      *debugName;
} ralBufferCreateInfo_t;

ralBuffer_t *Ral_CreateBuffer ( ralBackend_t *b, const ralBufferCreateInfo_t *ci );
void         Ral_DestroyBuffer( ralBuffer_t *buf );

// Mapping — host-visible memory only. Returns a persistent pointer; the
// backend handles coherent vs. explicit flush per memory type.
void *Ral_MapBuffer  ( ralBuffer_t *buf );
void  Ral_UnmapBuffer( ralBuffer_t *buf );
void  Ral_FlushBuffer( ralBuffer_t *buf, uint64_t offset, uint64_t size );

// Typed mapping is the portable path. Vulkan completes Begin immediately;
// WebGPU may return a PENDING ticket and later expose READY through Poll.
ralResult_t Ral_BufferMapBegin( ralBuffer_t *buf,
	                            const ralBufferMapRequest_t *request,
	                            ralBufferMapTicket_t *outTicket );
ralResult_t Ral_BufferMapPoll( ralBuffer_t *buf,
	                           const ralBufferMapTicket_t *authority,
	                           ralBufferMapTicket_t *outTicket );
ralResult_t Ral_BufferMapUnmap( ralBuffer_t *buf,
	                            const ralBufferMapTicket_t *readyTicket );
ralResult_t Ral_BufferMapCancel( ralBuffer_t *buf,
	                             const ralBufferMapTicket_t *pendingTicket );

// Async upload — first-class in v1. Submits to the transfer queue (§10);
// returns a fence the caller can wait on / poll. The renderer never writes
// the staging-buffer / submit / fence dance itself.
ralFence_t *Ral_BufferUploadAsync( ralBuffer_t *buf, uint64_t offset,
                                   const void *data, uint64_t size );

// ════════════════════════════════════════════════════════════════════════
// Textures (§3.3) — same shape as buffers + format / extent / mips / views.
// ════════════════════════════════════════════════════════════════════════
typedef enum {
	RAL_TEXTURE_1D,
	RAL_TEXTURE_2D,
	RAL_TEXTURE_3D,
	RAL_TEXTURE_CUBE,
	RAL_TEXTURE_2D_ARRAY,
	RAL_TEXTURE_CUBE_ARRAY
} ralTextureType_t;

typedef enum {
	RAL_TEXTURE_USAGE_SAMPLED                  = 1 << 0,
	RAL_TEXTURE_USAGE_STORAGE                  = 1 << 1,
	RAL_TEXTURE_USAGE_COLOR_ATTACHMENT         = 1 << 2,
	RAL_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT = 1 << 3,
	RAL_TEXTURE_USAGE_TRANSFER_SRC             = 1 << 4,
	RAL_TEXTURE_USAGE_TRANSFER_DST             = 1 << 5
} ralTextureUsage_t;

typedef struct {
	ralTextureType_t  type;
	ralFormat_t       format;
	uint32_t          width;
	uint32_t          height;             // 1 for 1D
	uint32_t          depthOrArrayLayers; // 3D depth, or array layer count (cube = 6 * faces)
	uint32_t          mipLevels;          // 0 → full chain
	uint32_t          sampleCount;        // 1 = no MSAA
	ralTextureUsage_t usage;              // bitmask of RAL_TEXTURE_USAGE_*
	ralMemoryType_t   memory;
	const char       *debugName;
	// async-compute: when qtrue, the image is created with concurrent
	// queue sharing across the graphics + compute families (VK_SHARING_MODE_CONCURRENT
	// on Vulkan), so a resource written by an async-compute pass and read by graphics
	// (or vice-versa) needs only a semaphore for execution ordering — no per-frame
	// queue-family-ownership-transfer barriers. Ignored when the device has no
	// dedicated compute family (the backend falls back to EXCLUSIVE, single-queue).
	// Default qfalse = EXCLUSIVE (the historical behaviour for every other texture).
	qboolean          concurrentGraphicsCompute;
	// async-transfer: when qtrue, the image is created with concurrent queue sharing
	// across the graphics + transfer families, so an upload copy submitted on the
	// transfer queue and sampled by graphics needs only a semaphore for ordering — no
	// queue-family-ownership-transfer barrier. Ignored when there is no dedicated
	// transfer family (the backend falls back to EXCLUSIVE). Default qfalse = EXCLUSIVE.
	qboolean          concurrentGraphicsTransfer;
} ralTextureCreateInfo_t;

typedef struct {
	const ralTexture_t *texture;
	ralTextureType_t    viewType;
	ralFormat_t         format;           // RAL_FORMAT_UNDEFINED → inherit texture format
	uint32_t            baseMipLevel;
	uint32_t            mipLevelCount;     // 0 → all remaining
	uint32_t            baseArrayLayer;
	uint32_t            arrayLayerCount;   // 0 → all remaining
} ralTextureViewCreateInfo_t;

// One mip/layer slice for Ral_TextureUploadAsync. regionWidth/regionHeight
// default (== 0) means "the full mip extent"; non-zero values let the caller
// upload a sub-rectangle of the mip — used by the bindless mirror in
// vk_upload_image_data so the per-tile merged-lightmap atlas uploads can be
// propagated into the RAL texture without re-uploading the whole atlas.
// Sub-region uploads (regionWidth/Height != 0) suppress the automatic GPU
// mip-gen path; the caller is responsible for providing every mip.
typedef struct {
	uint32_t    mipLevel;
	uint32_t    arrayLayer;
	const void *data;
	uint64_t    dataSize;
	uint32_t    offsetX, offsetY;
	uint32_t    regionWidth, regionHeight;
	// Full-mip uploads at mip 0 historically generate the remaining chain.
	// Set this only when the caller is independently streaming one mip and must
	// preserve the already-resident parent chain.  Zero keeps legacy behavior.
	qboolean    suppressMipGeneration;
} ralTextureUploadDesc_t;

ralTexture_t     *Ral_CreateTexture     ( ralBackend_t *b, const ralTextureCreateInfo_t *ci );
// Reports whether an optimal-tiling image of `format` supports every usage
// bit the Vulkan backend will actually place on the image. The backend's
// default sampled/upload/readback expansion is included, so this query is
// never more optimistic than Ral_CreateTexture's VkImageCreateInfo.
qboolean          Ral_TextureFormatSupports( ralBackend_t *b, ralFormat_t format,
	                                          ralTextureUsage_t usage );
void              Ral_DestroyTexture    ( ralTexture_t *tex );
uint32_t          Ral_GetTextureMipLevelCount( const ralTexture_t *tex );
ralTextureView_t *Ral_CreateTextureView ( ralBackend_t *b, const ralTextureViewCreateInfo_t *ci );
void              Ral_DestroyTextureView( ralTextureView_t *view );
ralFence_t       *Ral_TextureUploadAsync( ralTexture_t *tex, const ralTextureUploadDesc_t *region );

// Residency ticket returned by Ral_TextureUploadBegin. `fence` is the CPU poll
// authority. A dedicated-transfer upload additionally returns a binary
// readySemaphore plus its exact mip/layer range; pass the ticket through
// Ral_TextureAcquireBatchToGraphics before publishing it to graphics. A
// synchronous or graphics-queue ticket needs no acquire semaphore.
typedef struct {
	ralFence_t     *fence;
	ralSemaphore_t *readySemaphore;
	ralTexture_t   *texture;
	uint32_t        baseMipLevel;
	uint32_t        mipLevelCount;
	uint32_t        baseArrayLayer;
	uint32_t        arrayLayerCount;
	qboolean        synchronous;
	qboolean        graphicsAcquireRequired;
	ralTransferReceipt_t transfer;
} ralUploadTicket_t;

// Begins an upload of `region` into `tex` and returns a residency ticket. The
// caller binds a placeholder until the ticket reports resident (synchronous ==
// qtrue, or Ral_FenceSignaled(fence) == qtrue), then swaps the real texture in.
// Coexists with Ral_TextureUploadAsync (which the sub-region / mip-gen / depth
// callers keep using). The returned fence is owned by the caller (destroy it
// once residency is observed).
ralUploadTicket_t Ral_TextureUploadBegin( ralTexture_t *tex, const ralTextureUploadDesc_t *region );
qboolean Ral_TextureUploadTicketComplete( ralUploadTicket_t *ticket );
qboolean Ral_TextureUploadTicketGetReceipt( const ralUploadTicket_t *ticket,
	ralTransferReceipt_t *out );

// Makes a batch of transfer-upload tickets visible to graphics sampling. The
// acquire waits each ticket's binary readySemaphore and applies a precise
// SHADER_READ_ONLY barrier to only that ticket's mip/layer range. The caller
// retains ticket ownership and destroys fence/semaphore after a successful
// return. Tickets that do not require a graphics acquire are ignored.
qboolean Ral_TextureAcquireBatchToGraphics( ralBackend_t *b, const ralUploadTicket_t *tickets, uint32_t count );

// ════════════════════════════════════════════════════════════════════════
// Samplers (§3.4)
// ════════════════════════════════════════════════════════════════════════
typedef enum { RAL_FILTER_NEAREST, RAL_FILTER_LINEAR } ralFilter_t;
typedef enum { RAL_MIPMAP_NEAREST, RAL_MIPMAP_LINEAR } ralMipmapMode_t;
typedef enum {
	RAL_ADDRESS_REPEAT,
	RAL_ADDRESS_MIRRORED_REPEAT,
	RAL_ADDRESS_CLAMP_TO_EDGE,
	RAL_ADDRESS_CLAMP_TO_BORDER
} ralAddressMode_t;

typedef struct {
	ralFilter_t      minFilter;
	ralFilter_t      magFilter;
	ralMipmapMode_t  mipmapMode;
	ralAddressMode_t addressU, addressV, addressW;
	float            maxAnisotropy;   // 1 = off
	qboolean         compareEnable;   // shadow sampler
	ralCompareOp_t   compareOp;
	float            minLod, maxLod;
	const char      *debugName;
} ralSamplerCreateInfo_t;

ralSampler_t *Ral_CreateSampler ( ralBackend_t *b, const ralSamplerCreateInfo_t *ci );
void          Ral_DestroySampler( ralSampler_t *s );

// ════════════════════════════════════════════════════════════════════════
// BindGroupLayout / BindGroup (§3.5, §4) — the bindless-native abstraction.
// ════════════════════════════════════════════════════════════════════════
typedef enum {
	RAL_BIND_UNIFORM_BUFFER,
	RAL_BIND_STORAGE_BUFFER,
	RAL_BIND_SAMPLED_TEXTURE,
	RAL_BIND_STORAGE_TEXTURE,
	RAL_BIND_SAMPLER,
	RAL_BIND_TEXTURE_ARRAY,     // bindless sampled-texture array (count == 0 → unbounded)
	RAL_BIND_COMBINED_TEXTURE_SAMPLER
} ralBindType_t;

// Portable texture-view dimensionality carried by bind-layout entries.  The
// explicit UNSPECIFIED value keeps legacy initializers source-compatible while
// allowing WebGPU/Metal backends to reject ambiguous shader interfaces.
typedef enum {
	RAL_BIND_TEXTURE_VIEW_UNSPECIFIED = 0,
	RAL_BIND_TEXTURE_VIEW_1D,
	RAL_BIND_TEXTURE_VIEW_2D,
	RAL_BIND_TEXTURE_VIEW_2D_ARRAY,
	RAL_BIND_TEXTURE_VIEW_CUBE,
	RAL_BIND_TEXTURE_VIEW_CUBE_ARRAY,
	RAL_BIND_TEXTURE_VIEW_3D
} ralBindTextureViewType_t;

typedef struct {
	uint32_t      binding;
	ralBindType_t type;
	uint32_t      count;        // 1 = single; >1 = fixed array; 0 = unbounded (bindless layout only)
	uint32_t      stageFlags;   // bitmask of RAL_STAGE_*
	ralBindTextureViewType_t textureViewType; // required for texture entries on portable backends
} ralBindEntry_t;

typedef struct {
	const ralBindEntry_t *entries;
	uint32_t              numEntries;
	qboolean              bindless;   // true → entries with count == 0 are unbounded
	const char           *debugName;
} ralBindGroupLayoutCreateInfo_t;

ralBindGroupLayout_t *Ral_CreateBindGroupLayout ( ralBackend_t *b, const ralBindGroupLayoutCreateInfo_t *ci );

void                  Ral_DestroyBindGroupLayout( ralBindGroupLayout_t *layout );

// ════════════════════════════════════════════════════════════════════════
// ralPipelineLayout_t foundation.
//
void                 Ral_DestroyPipelineLayout( ralPipelineLayout_t *pl );

// One slot's value for Ral_CreateBindGroup. Read the fields that match `type`:
//   *_BUFFER          → buffer, bufferOffset, bufferRange
//   SAMPLED/STORAGE_TEXTURE → textureView
//   SAMPLER           → sampler
//   COMBINED_TEXTURE_SAMPLER → textureView + sampler
//   TEXTURE_ARRAY     → textureArray[0..textureArrayCount)
typedef struct {
	uint32_t                       binding;
	ralBindType_t                  type;
	const ralBuffer_t             *buffer;
	uint64_t                       bufferOffset;
	uint64_t                       bufferRange;     // 0 → whole buffer
	const ralTextureView_t        *textureView;
	const ralSampler_t            *sampler;
	const ralTextureView_t *const *textureArray;
	uint32_t                       textureArrayCount;
} ralBindingValue_t;

typedef struct {
	const ralBindGroupLayout_t *layout;
	const ralBindingValue_t    *values;
	uint32_t                    numValues;
	const char                 *debugName;
} ralBindGroupCreateInfo_t;

ralBindGroup_t *Ral_CreateBindGroup ( ralBackend_t *b, const ralBindGroupCreateInfo_t *ci );
void            Ral_DestroyBindGroup( ralBindGroup_t *g );

// Sparse update of a bindless table — per-frame, adds a newly-resident
// texture without recreating the BindGroup.
void Ral_BindGroupSetTextureAt( ralBindGroup_t *g, uint32_t slot, ralTexture_t *tex );

// View-aware sparse update for residency systems. Unlike the whole-texture
// helper above, this preserves the caller's base-mip / mip-count restriction,
// so a coarse parent view can remain bound while finer child pages stream.
// NULL view is the same intentional no-op clear as NULL texture.
void Ral_BindGroupSetTextureViewAt( ralBindGroup_t *g, uint32_t slot, ralTextureView_t *view );
// Atomically publishes a coherence group into one sampled-image array with a
// single backend descriptor update. All entries are validated before any write;
// NULL inputs or an invalid slot reject the whole batch.
int Ral_BindGroupSetTextureViewsAt( ralBindGroup_t *g, const uint32_t *slots,
		ralTextureView_t *const *views, uint32_t count );

// Sparse update of a bindless sampler-array binding — written as new
// VkSamplers enter the dedup pool. Mirrors Ral_BindGroupSetTextureAt; the
// layout must declare a SAMPLER binding (any count) for the write to find
// a destination. NULL sampler is a no-op clear (PARTIALLY_BOUND lets the
// stale slot persist; explicit-null writes are invalid in Vulkan).
void Ral_BindGroupSetSamplerAt( ralBindGroup_t *g, uint32_t slot, ralSampler_t *s );

#ifdef __cplusplus
}
#endif

#endif // WIRED_RAL_RESOURCE_H
