// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
//
// Vulkan migration bridge.
//
// These entry points expose or wrap backend-native Vulkan objects while the
// legacy renderervk owner is being migrated onto typed RAL resources. They are
// deliberately NOT part of code/renderer/ral/'s portable public surface.
// Only the Vulkan backend and renderervk integration may include this header.

#ifndef WIRED_RAL_VULKAN_BRIDGE_H
#define WIRED_RAL_VULKAN_BRIDGE_H

#include "../ral/ral.h"

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

void *Ral_GetCommandBufferHandle( const ralCommandBuffer_t *cb );
// Renderer-only migration bridge. The current persistent renderer rings still
// use qvkBegin/End/Reset on their RAL-allocated handles. New portable RAL
// consumers keep the default strict lifecycle and must not enable this.
void Ral_SetCommandBufferExternalLifecycle( ralCommandBuffer_t *cb, qboolean enabled );

ralFence_t *Ral_AdoptFence( ralBackend_t *b, void *externalFence, const char *debugName );
void       *Ral_GetFenceHandle( const ralFence_t *fence );
ralSemaphore_t *Ral_AdoptSemaphore( ralBackend_t *b, void *externalSemaphore,
                                    ralSemaphoreType_t type, const char *debugName );
void           *Ral_GetSemaphoreHandle( const ralSemaphore_t *semaphore );

void *Ral_GetSwapchainHandle( const ralSwapchain_t *swapchain );

ralBuffer_t *Ral_AdoptBuffer( ralBackend_t *b, void *externalBuffer,
                              size_t size, const char *debugName );
void *Ral_GetBufferHandle( const ralBuffer_t *buffer );
uint64_t Ral_GetBufferSize( const ralBuffer_t *buffer );

ralTexture_t *Ral_AdoptTexture( ralBackend_t *b, void *externalImage,
                                void *externalView, ralFormat_t format,
                                uint32_t width, uint32_t height, uint32_t aspect,
                                const char *debugName );
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

ralPipelineLayout_t *Ral_AdoptPipelineLayout( ralBackend_t *b,
                                              void *externalLayout,
                                              const char *debugName );
void *Ral_GetPipelineLayoutHandle( const ralPipelineLayout_t *layout );

ralBindGroup_t *Ral_AdoptBindGroup( ralBackend_t *b, void *externalSet,
                                    const ralBindGroupLayout_t *layout,
                                    const char *debugName );
void *Ral_GetBindGroupHandle( const ralBindGroup_t *group );

#ifdef __cplusplus
}
#endif

#endif
