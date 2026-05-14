// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
//
// ral_resource.h — buffers, textures, samplers, bind groups.
// Part of the Wired RAL v1 surface (docs/phase-7-ral-design.md §3.2-§3.5, §4).

#ifndef WIRED_RAL_RESOURCE_H
#define WIRED_RAL_RESOURCE_H

#include "ral_types.h"

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
	RAL_BUFFER_TRANSFER_DST = 1 << 6
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

// One mip/layer slice for Ral_TextureUploadAsync.
typedef struct {
	uint32_t    mipLevel;
	uint32_t    arrayLayer;
	const void *data;
	uint64_t    dataSize;
} ralTextureUploadDesc_t;

ralTexture_t     *Ral_CreateTexture     ( ralBackend_t *b, const ralTextureCreateInfo_t *ci );
void              Ral_DestroyTexture    ( ralTexture_t *tex );
ralTextureView_t *Ral_CreateTextureView ( ralBackend_t *b, const ralTextureViewCreateInfo_t *ci );
void              Ral_DestroyTextureView( ralTextureView_t *view );
ralFence_t       *Ral_TextureUploadAsync( ralTexture_t *tex, const ralTextureUploadDesc_t *region );

// Phase 7.4c-submit-A4 — adopt-style wrapper around an existing backend
// VkImage (caller's qvkCreateImage retains lifetime ownership of the image +
// memory + default view). Mirrors Ral_AdoptPipelineLayout / Ral_AdoptRenderPass:
// ownsImage=qfalse so Ral_DestroyTexture frees only the wrapper struct, the
// underlying VkImage stays alive past the wrapper. `width` / `height` are the
// image's mip-0 extent (the wrapper reports them via the standard fields so
// reverse-lookup consumers can construct ralImageMemoryBarrier_t / ralImageCopy_t
// extents without poking the Vk image). `aspect` is the VkImageAspectFlags
// bitmask the caller's image was created with (COLOR / DEPTH / DEPTH+STENCIL) —
// passed through to ralImageMemoryBarrier_t builders. The wrapper does NOT
// carry an image view (defaultView = VK_NULL_HANDLE) — adopted textures are
// referenced only by their VkImage handle in the 7.4c-submit-A4 callsite set
// (CopyImage / PipelineBarrierFull image-memory barriers).
//
// Used at the 6 renderer-owned internal-image sites (depth_image, color_image,
// tonemapped_image, smaa.input_image, smaa.edges_image, smaa.blend_image)
// adopted into ralTexture_t* sibling fields by vk_ral_adopt_static_internal_textures.
ralTexture_t *Ral_AdoptTexture( ralBackend_t *b,
                                void *externalImage,
                                uint32_t width, uint32_t height,
                                uint32_t aspect,
                                const char *debugName );

// Phase 7.4c-submit-A4: read the backend-native VkImage from an adopted (or
// owned) ralTexture_t. Mirrors Ral_GetPipelineLayoutHandle / Ral_GetBindGroupHandle.
// Returns NULL on bad arg. Consumers cast back to VkImage.
void *Ral_GetTextureImageHandle( const ralTexture_t *tex );

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
	RAL_BIND_TEXTURE_ARRAY      // bindless sampled-texture array (count == 0 → unbounded)
} ralBindType_t;

typedef struct {
	uint32_t      binding;
	ralBindType_t type;
	uint32_t      count;        // 1 = single; >1 = fixed array; 0 = unbounded (bindless layout only)
	uint32_t      stageFlags;   // bitmask of RAL_STAGE_*
} ralBindEntry_t;

typedef struct {
	const ralBindEntry_t *entries;
	uint32_t              numEntries;
	qboolean              bindless;   // true → entries with count == 0 are unbounded
	const char           *debugName;
} ralBindGroupLayoutCreateInfo_t;

ralBindGroupLayout_t *Ral_CreateBindGroupLayout ( ralBackend_t *b, const ralBindGroupLayoutCreateInfo_t *ci );

// Phase 7.4c-bindgroup-pre — wrap a pre-existing backend descriptor-set
// layout (e.g. the renderer's own VkDescriptorSetLayout) in a
// ralBindGroupLayout_t WITHOUT creating a new one. `externalLayout` is the
// backend-specific handle cast to void* (VkDescriptorSetLayout / id<MTLBindGroupLayout>
// / GPUBindGroupLayout etc.). The caller retains ownership of the handle —
// Ral_DestroyBindGroupLayout on an adopted wrapper frees the wrapper struct
// but does NOT destroy the underlying backend layout. `entries[]` describes
// the layout's binding shape so RAL pipeline-layout creation can populate
// VkPipelineLayoutCreateInfo correctly; it must match the bindings the
// caller used when creating the layout.
ralBindGroupLayout_t *Ral_AdoptBindGroupLayout( ralBackend_t *b,
                                                void *externalLayout,
                                                uint32_t numEntries,
                                                const ralBindEntry_t *entries,
                                                const char *debugName );

void                  Ral_DestroyBindGroupLayout( ralBindGroupLayout_t *layout );

// ════════════════════════════════════════════════════════════════════════
// Phase 7.4c-submit-A2 — ralPipelineLayout_t foundation.
//
// Adopt-style wrapper around an existing backend pipeline layout
// (VkPipelineLayout on Vulkan). Mirrors the Ral_AdoptBindGroupLayout pattern
// from 7.4c-bindgroup-pre: ownsLayout=qfalse so Ral_DestroyPipelineLayout
// skips vkDestroyPipelineLayout (the renderer's existing qvkCreatePipelineLayout
// site retains lifetime ownership). Wrapper carries the bind-group-layout
// references that the layout was created with — currently informational only;
// future RAL pipeline-layout creation could rebuild VkPipelineLayoutCreateInfo
// from the array.
//
// The renderer adopts each of its ~13 VkPipelineLayout objects at the matching
// qvkCreatePipelineLayout site (vk.pipeline_layout, vk.pipeline_layout_smaa,
// vk.shadowMap.depthLayout, etc.) into a sibling ral_pipeline_layout field,
// then passes the typed wrapper to the new Ral_CmdPushConstants / Ral_CmdBind*
// surface added in this turn.
ralPipelineLayout_t *Ral_AdoptPipelineLayout( ralBackend_t *b,
                                              void *externalLayout,
                                              const char *debugName );
void                 Ral_DestroyPipelineLayout( ralPipelineLayout_t *pl );

// Phase 7.4c-submit-A2: read the backend-native handle for a ralPipelineLayout_t.
// On Vulkan this is the VkPipelineLayout the wrapper carries. Mirrors
// Ral_GetBindGroupHandle. Returns NULL on bad arg. Consumers cast back to the
// platform-native handle type.
void *Ral_GetPipelineLayoutHandle( const ralPipelineLayout_t *pl );

// Phase 7.4c-submit-A2 — opaque wrappers for VkRenderPass / VkFramebuffer
// adoption. The renderer's render passes + framebuffers stay owned by the
// existing qvkCreate{RenderPass,Framebuffer} sites; these wrappers are
// metadata + a stable RAL handle for the typed Ral_CmdBeginRenderPass surface.
// Lifetime: ownsHandle=qfalse so Ral_Destroy* skips vkDestroy*.
ralRenderPass_t  *Ral_AdoptRenderPass ( ralBackend_t *b, void *externalRenderPass,  const char *debugName );
ralFramebuffer_t *Ral_AdoptFramebuffer( ralBackend_t *b, void *externalFramebuffer, const char *debugName );
void              Ral_DestroyRenderPass ( ralRenderPass_t  *rp );
void              Ral_DestroyFramebuffer( ralFramebuffer_t *fb );
void             *Ral_GetRenderPassHandle ( const ralRenderPass_t  *rp );
void             *Ral_GetFramebufferHandle( const ralFramebuffer_t *fb );

// One slot's value for Ral_CreateBindGroup. Read the fields that match `type`:
//   *_BUFFER          → buffer, bufferOffset, bufferRange
//   SAMPLED/STORAGE_TEXTURE → textureView
//   SAMPLER           → sampler
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

// Phase 7.4c-bindgroup: adopt an EXISTING VkDescriptorSet (or platform
// equivalent) into a ralBindGroup_t wrapper. Mirrors Ral_AdoptBindGroupLayout:
// the caller's pool retains lifetime ownership of the underlying set; the
// wrapper carries ownsSet=qfalse, so Ral_DestroyBindGroup frees only the
// wrapper struct and does NOT call vkFreeDescriptorSets. The `layout` is the
// matching ralBindGroupLayout_t from a prior Ral_CreateBindGroupLayout or
// Ral_AdoptBindGroupLayout call. The descriptor SET CONTENT is whatever the
// caller already wrote — this wrapper does no Update.
ralBindGroup_t *Ral_AdoptBindGroup( ralBackend_t *b,
                                    void *externalSet,
                                    const ralBindGroupLayout_t *layout,
                                    const char *debugName );

// Phase 7.4c-bindgroup: read the backend-native handle for an adopted (or
// owned) ralBindGroup_t. On Vulkan this is the VkDescriptorSet that the
// wrapper carries. The renderer uses this for VkDescriptorSet→ralBindGroup_t
// reverse-lookup at parallel bind-call sites during the parallel-paths era.
// Returns NULL on bad args. void * because the public surface stays platform-
// neutral; consumer casts back to VkDescriptorSet.
void *Ral_GetBindGroupHandle( const ralBindGroup_t *g );

// Sparse update of a bindless table — per-frame, adds a newly-resident
// texture without recreating the BindGroup.
void Ral_BindGroupSetTextureAt( ralBindGroup_t *g, uint32_t slot, ralTexture_t *tex );

#ifdef __cplusplus
}
#endif

#endif // WIRED_RAL_RESOURCE_H
