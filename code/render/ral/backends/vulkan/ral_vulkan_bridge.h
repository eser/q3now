// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
//
// Vulkan migration bridge.
//
// These entry points expose or wrap backend-native Vulkan objects while the
// legacy renderervk owner is being migrated onto typed RAL resources. They are
// deliberately NOT part of code/render/ral/core/'s portable public surface.
// Only the Vulkan backend and renderervk integration may include this header.

#ifndef WIRED_RAL_VULKAN_BRIDGE_H
#define WIRED_RAL_VULKAN_BRIDGE_H

#include "../../core/ral.h"

#ifdef __cplusplus
extern "C" {
#endif

void    *Ral_GetInstanceHandle( const ralBackend_t *b );
void    *Ral_GetPhysicalDeviceHandle( const ralBackend_t *b );
void    *Ral_GetSurfaceHandle( const ralBackend_t *b );
void    *Ral_GetDeviceHandle( const ralBackend_t *b );
void    *Ral_GetQueueHandle( const ralBackend_t *b, ralQueueType_t q );
uint32_t Ral_GetQueueFamily( const ralBackend_t *b, ralQueueType_t q );
void     Ral_GetEnabledDeviceExtensions( const ralBackend_t *b,
                                         const char *const **out,
                                         uint32_t *outCount );

// Renderer-only allocation migration bridge. Vulkan memory-type indices and
// property bits have no portable/WebGPU meaning, so this deliberately stays
// outside ral/*.h. It queries the backend-owned immutable memory snapshot and
// never calls the physical device from the renderer.
qboolean RalVulkan_FindMemoryType( const ralBackend_t *b, uint32_t typeBits,
	uint32_t requiredProperties, uint32_t *outTypeIndex,
	uint32_t *outActualProperties );

typedef struct {
	uint64_t size;
	uint64_t alignment;
	uint32_t memoryTypeBits;
} ralVulkanMemoryRequirements_t;

// Renderer-only image-allocation migration bridge. The portable RAL never
// exposes native memory requirements; legacy Vulkan images use this narrow
// receipt until their allocation ownership moves behind typed RAL textures.
qboolean RalVulkan_GetImageMemoryRequirements( const ralBackend_t *b,
	void *imageIdentity, ralVulkanMemoryRequirements_t *outRequirements );

typedef enum {
	RAL_VULKAN_OBJECT_ROLE_DEVICE = 0,
	RAL_VULKAN_OBJECT_ROLE_DEVICE_MEMORY,
	RAL_VULKAN_OBJECT_ROLE_FENCE,
	RAL_VULKAN_OBJECT_ROLE_SEMAPHORE,
	RAL_VULKAN_OBJECT_ROLE_BUFFER,
	RAL_VULKAN_OBJECT_ROLE_IMAGE,
	RAL_VULKAN_OBJECT_ROLE_IMAGE_VIEW,
	RAL_VULKAN_OBJECT_ROLE_SHADER_MODULE,
	RAL_VULKAN_OBJECT_ROLE_PIPELINE_LAYOUT,
	RAL_VULKAN_OBJECT_ROLE_SAMPLER,
	RAL_VULKAN_OBJECT_ROLE_DESCRIPTOR_SET_LAYOUT,
	RAL_VULKAN_OBJECT_ROLE_DESCRIPTOR_SET,
	RAL_VULKAN_OBJECT_ROLE_COMMAND_BUFFER,
	RAL_VULKAN_OBJECT_ROLE_COUNT
} ralVulkanObjectRole_t;

// Renderer-only naming migration bridge. Roles are bounded here rather than
// exposing VkObjectType through portable RAL. A backend without debug-utils
// accepts the request as an optional no-op, matching debug-name semantics.
qboolean RalVulkan_SetObjectName( ralBackend_t *b, uint64_t objectIdentity,
	ralVulkanObjectRole_t role, const char *debugName );

// Legacy renderervk shader modules remain native until every adjacent pipeline
// path consumes SPIR-V directly. The bridge owns an exact per-backend registry
// so only modules created here can be destroyed here; portable RAL APIs expose
// neither VkShaderModule nor this temporary identity.
qboolean RalVulkan_CreateShaderModule( ralBackend_t *b,
	const uint8_t *bytes, uint32_t byteCount, void **outIdentity );
qboolean RalVulkan_DestroyShaderModule( ralBackend_t *b, void *identity );

// Legacy raw images that have not yet entered the typed texture registry still
// need exact layout handoffs during upload. Keep native layout/aspect/stage
// values inside this Vulkan-only bridge; portable textures must use
// Ral_CmdTransitionResources instead.
qboolean RalVulkan_RecordLegacyImageTransition( ralBackend_t *b,
	void *commandIdentity, void *imageIdentity, uint32_t aspectMask,
	uint32_t oldLayout, uint32_t newLayout,
	uint32_t srcStageOverride, uint32_t dstStageOverride );

void *Ral_GetCommandBufferHandle( const ralCommandBuffer_t *cb );

ralFence_t *Ral_AdoptFence( ralBackend_t *b, void *externalFence, const char *debugName );
void       *Ral_GetFenceHandle( const ralFence_t *fence );
ralSemaphore_t *Ral_AdoptSemaphore( ralBackend_t *b, void *externalSemaphore,
                                    ralSemaphoreType_t type, const char *debugName );
void           *Ral_GetSemaphoreHandle( const ralSemaphore_t *semaphore );

void *Ral_GetSwapchainHandle( const ralSwapchain_t *swapchain );
qboolean RalVulkan_SurfaceFormatToNative( ralSurfaceFormat_t format,
	uint32_t *outFormat, uint32_t *outColorSpace );

ralBuffer_t *Ral_AdoptBuffer( ralBackend_t *b, void *externalBuffer,
                              size_t size, const char *debugName );
// Exact renderer-migration import. The wrapper borrows the native buffer but
// retains its portable creation capabilities so later RAL commands operate on
// the real renderer-owned bytes rather than a parallel allocation.
ralBuffer_t *Ral_AdoptBufferExact( ralBackend_t *b, void *externalBuffer,
                              const ralBufferCreateInfo_t *createInfo );
// One-time semantic handoff for a freshly adopted external buffer. Later
// state changes must use Ral_CmdTransitionResources.
qboolean Ral_PublishAdoptedBufferState( ralBuffer_t *buffer,
	const ralResourceState_t *state, ralQueueType_t ownerQueue );
void *Ral_GetBufferHandle( const ralBuffer_t *buffer );
uint64_t Ral_GetBufferSize( const ralBuffer_t *buffer );
ralBufferUsage_t Ral_GetBufferUsage( const ralBuffer_t *buffer );
ralMemoryType_t Ral_GetBufferMemoryType( const ralBuffer_t *buffer );

ralTexture_t *Ral_AdoptTexture( ralBackend_t *b, void *externalImage,
                                void *externalView, ralFormat_t format,
                                uint32_t width, uint32_t height, uint32_t aspect,
                                const char *debugName );
// Exact import variant for creation-time usage authority. Prefer this for new
// external/surface resources; the compatibility helper above publishes no
// usage capabilities and therefore cannot enter portable transfer paths.
ralTexture_t *Ral_AdoptTextureExact( ralBackend_t *b, void *externalImage,
                                void *externalView, ralFormat_t format,
                                uint32_t width, uint32_t height, uint32_t aspect,
                                ralTextureUsage_t usage, const char *debugName );
// Full-shape import used by renderer migration. The wrapper borrows the native
// image/view but preserves type, format, extent, mip, sample and capability
// facts required by portable buffer-to-texture validation.
ralTexture_t *Ral_AdoptTextureResourceExact( ralBackend_t *b,
		void *externalImage, void *externalView, uint32_t aspect,
		const ralTextureCreateInfo_t *createInfo );
// Renderer-migration import for an already-created native view. The returned
// wrapper borrows both `texture` and `externalView`; destroying it releases
// only the wrapper and never the caller-owned VkImageView.
ralTextureView_t *Ral_AdoptTextureViewExact( ralBackend_t *b,
		const ralTexture_t *texture, void *externalView );
// One-time semantic handoff for a freshly adopted external image. The caller
// owns proof of the external image's current state; later state changes must go
// through portable Ral_CmdTransitionResources.
qboolean Ral_PublishAdoptedTextureState( ralTexture_t *texture,
	const ralResourceState_t *state, ralQueueType_t ownerQueue );
ralTexture_t *Ral_AdoptArrayTexture( ralBackend_t *b, void *externalImage,
                                     void *externalDefaultView,
                                     const void *const *layerViews,
                                     uint32_t layerCount, ralFormat_t format,
                                     uint32_t width, uint32_t height, uint32_t aspect,
                                     const char *debugName );
void *Ral_GetTextureImageHandle( const ralTexture_t *texture );
void *Ral_GetTextureDefaultViewHandle( const ralTexture_t *texture );
void *Ral_GetTextureViewHandle( const ralTextureView_t *view );
void  Ral_SetTextureLayout( ralTexture_t *texture, uint32_t vkLayout );

ralSampler_t *Ral_AdoptSampler( ralBackend_t *b, void *externalSampler,
                                const char *debugName );
void *Ral_GetSamplerHandle( const ralSampler_t *sampler );

ralBindGroupLayout_t *Ral_AdoptBindGroupLayout( ralBackend_t *b,
                                                void *externalLayout,
                                                uint32_t numEntries,
                                                const ralBindEntry_t *entries,
                                                const char *debugName );
void *Ral_GetBindGroupLayoutHandle( const ralBindGroupLayout_t *layout );
void *Ral_GetBindGroupArenaHandle( const ralBindGroupArena_t *arena );

ralPipelineLayout_t *Ral_AdoptPipelineLayout( ralBackend_t *b,
                                              void *externalLayout,
                                              const char *debugName );
// Migration ownership transfer: the wrapper becomes the sole owner of the
// supplied native pipeline layout and destroys it through the backend dispatch.
// Use only for a freshly-created handle with no remaining external destructor.
ralPipelineLayout_t *Ral_AdoptOwnedPipelineLayout( ralBackend_t *b,
                                                   void *externalLayout,
                                                   const char *debugName );
void *Ral_GetPipelineLayoutHandle( const ralPipelineLayout_t *layout );
qboolean Ral_RegisterExternalPipelineBindGroupLayouts(
	ralPipeline_t *pipeline, uint32_t count,
	ralBindGroupLayout_t *const *bindGroupLayouts );
// Publish one exact portable push-constant range from a renderer-owned native
// pipeline layout. Re-registration is idempotent; overlapping same-stage drift
// and capacity/range overflow reject without changing prior authority.
qboolean Ral_RegisterExternalPipelineLayoutPushRange(
	ralPipelineLayout_t *layout, uint32_t stageFlags,
	uint32_t offset, uint32_t size );

ralBindGroup_t *Ral_AdoptBindGroup( ralBackend_t *b, void *externalSet,
                                    const ralBindGroupLayout_t *layout,
                                    const char *debugName );
void *Ral_GetBindGroupHandle( const ralBindGroup_t *group );
// Migration-only provenance for an adopted dynamic descriptor set. The
// renderer still owns the native descriptor write; this registers the exact
// portable buffer/base/range facts needed by command-time validation.
qboolean Ral_RegisterAdoptedBindGroupDynamicBuffer( ralBindGroup_t *group,
	uint32_t binding, const ralBuffer_t *buffer, uint64_t baseOffset, uint64_t range );

#ifdef __cplusplus
}
#endif

#endif
