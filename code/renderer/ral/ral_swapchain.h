// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
//
// ral_swapchain.h — presentation surface, HDR metadata.
// Part of the Wired RAL v1 surface (docs/phase-7-ral-design.md §3.9, §7.8b).
//
// The presentation surface is backend-owned and was created through
// ralHostImports_t::createSurface.  Callers describe ordered preferences and
// hard usage requirements; the backend queries the surface and publishes the
// selected, renderable swapchain through ralSwapchainInfo_t.

#ifndef WIRED_RAL_SWAPCHAIN_H
#define WIRED_RAL_SWAPCHAIN_H

#include "ral_types.h"
#include "ral_resource.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
	ralFormat_t     format;
	ralColorSpace_t colorSpace;
} ralSurfaceFormat_t;

typedef struct {
	uint32_t                  desiredWidth;
	uint32_t                  desiredHeight;
	const ralSurfaceFormat_t *formatPreferences;      // ordered, exact format+colour-space pairs
	uint32_t                  formatPreferenceCount;
	const ralPresentMode_t   *presentModePreferences; // ordered; include every acceptable fallback
	uint32_t                  presentModePreferenceCount;
	uint32_t                  desiredImageCount;      // 0 → backend default, clamped to surface caps
	ralTextureUsage_t         requiredUsage;          // exact hard requirements; unsupported bits fail
	// Backend-extension pass-through
	// for the swapchain create info struct's extension chain. On Vulkan, this
	// is the VkSwapchainCreateInfoKHR.pNext pointer — used by the renderer's
	// existing Windows-HDR full-screen-exclusive (FSE) chain at vk.c's
	// _WIN32 + hdr_display_active branch. NULL on platforms / modes that
	// don't need an extension chain. Caller-owned (no lifecycle transfer);
	// must outlive the Ral_CreateOrRecreateSwapchain call.
	const void      *backendExtensionChain;
} ralSwapchainCreateInfo_t;

typedef struct {
	uint64_t          generation;
	uint32_t          width;
	uint32_t          height;
	ralFormat_t       format;
	ralColorSpace_t   colorSpace;
	ralPresentMode_t  presentMode;
	uint32_t          imageCount;
	ralTextureUsage_t usage;
} ralSwapchainInfo_t;

// Create or recreate an output-authoritative swapchain.  On recreate, Vulkan
// receives the typed old native handle. Failures discovered during portable
// preflight (capability/policy selection, zero extent, or wait-idle) preserve
// *inOut and the old generation. Once vkCreateSwapchainKHR is attempted the old
// native swapchain may be retired even when that call fails, so the handoff is
// deliberately one-way: native-attempt/materialisation failure destroys the
// old wrapper and leaves *inOut NULL. There is no post-attempt rollback.
ralResult_t     Ral_CreateOrRecreateSwapchain( ralBackend_t *b,
	                                            const ralSwapchainCreateInfo_t *ci,
	                                            ralSwapchain_t **inOut );
void            Ral_DestroySwapchain( ralSwapchain_t *sc );

qboolean        Ral_GetSwapchainInfo( const ralSwapchain_t *sc, ralSwapchainInfo_t *outInfo );
ralTexture_t   *Ral_GetSwapchainImage( const ralSwapchain_t *sc, uint32_t imageIndex );

// Cached swapchain image extent (surface currentExtent at create time = physical
// swapchain pixels). The correct render-target size for the final present-blit
// pass — distinct from the SDL-reported window size (half on hi-DPI) and from the
// renderer's virtual vidWidth (diverges under render-scale / supersample). Writes
// 0 on NULL arg / pre-create. The render-target size lives in the RAL tier; the
// renderer asks via this accessor rather than reaching into the swapchain.
void Ral_GetSwapchainExtent( const ralSwapchain_t *sc, uint32_t *width, uint32_t *height );

// Typed Present-info shape (mirrors
// ralSubmitInfo_t's array convention). Renderer builds in-place and passes
// to Ral_Present. The single-swapchain case sets numSwapchains=1.
typedef struct {
	ralSwapchain_t   **swapchains;       // array of swapchains to present (typ. 1)
	uint32_t           numSwapchains;
	const uint32_t    *imageIndices;     // parallel to swapchains[]; one image index per swapchain
	ralSemaphore_t   **waitSemaphores;   // semaphores Present must wait on (typically render-finished per swapchain image)
	uint32_t           numWaitSemaphores;
} ralPresentInfo_t;

// Typed Acquire signature (replaces
// the earlier stub's outAcquireSem pattern). Caller owns the signal semaphore
// (per-frame ring's ralSemaphore_t) — RAL signals it but doesn't manage
// lifecycle. Returns ralSuccess on normal acquire; ralOutOfDate /
// ralSuboptimal indicate the renderer should drive a swapchain recreate
// via its existing vk_restart_swapchain path. ralSurfaceLost requires a
// surface/backend rebuild; retrying the same swapchain surface is invalid.
ralResult_t Ral_AcquireNextImage( ralSwapchain_t *sc,
                                  uint64_t        timeoutNs,
                                  ralSemaphore_t *signalSem,
                                  uint32_t       *outImageIndex,
                                  ralTexture_t  **outImage );

// Typed Present (replaces the earlier
// stub's flat 2-arg signature). Returns ralSuccess on normal present;
// ralOutOfDate / ralSuboptimal indicate the renderer should drive a
// swapchain recreate. ralSurfaceLost requires a surface/backend rebuild.
ralResult_t Ral_Present( ralBackend_t *b, const ralPresentInfo_t *info );

// HDR static metadata for HDR10 swapchains. Coordinates
// are CIE 1931 xy; luminance in cd/m². No-op on SDR swapchains.
typedef struct {
	float displayPrimaryRed[2];
	float displayPrimaryGreen[2];
	float displayPrimaryBlue[2];
	float whitePoint[2];
	float maxLuminance;
	float minLuminance;
	float maxContentLightLevel;
	float maxFrameAverageLightLevel;
} ralHdrMetadata_t;

void Ral_SetSwapchainHdrMetadata( ralSwapchain_t *sc, const ralHdrMetadata_t *md );

#ifdef __cplusplus
}
#endif

#endif // WIRED_RAL_SWAPCHAIN_H
