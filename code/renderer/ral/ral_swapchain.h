// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
//
// ral_swapchain.h — presentation surface, HDR metadata.
// Part of the Wired RAL v1 surface (docs/phase-7-ral-design.md §3.9, §7.8b).
//
// The platform surface comes from the renderer via ralSwapchainCreateInfo_t.
// externalSurface; RAL adopts the VkSurfaceKHR without taking ownership of
// its lifecycle. Surface creation/destruction stays in the platform layer
// (ri.VK_CreateSurface / vkDestroySurfaceKHR). HDR10 / scRGB colour spaces
// depend on caps.hdr10Swapchain / caps.scRGBSwapchain; SDR
// (RAL_COLORSPACE_SRGB_NONLINEAR) is always available and the default.

#ifndef WIRED_RAL_SWAPCHAIN_H
#define WIRED_RAL_SWAPCHAIN_H

#include "ral_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
	uint32_t         width;
	uint32_t         height;
	ralFormat_t      format;           // B8G8R8A8_UNORM / A2B10G10R10_UNORM / R16G16B16A16_SFLOAT
	ralColorSpace_t  colorSpace;
	ralPresentMode_t presentMode;      // FIFO / MAILBOX / IMMEDIATE
	uint32_t         minImageCount;    // 0 → backend picks (typ. 2-3)
	void            *externalSurface;  // Phase 7.4c-submit-followup-present-1 — VkSurfaceKHR on Vulkan; RAL adopts without taking ownership (renderer's ri.VK_CreateSurface retains lifecycle)
	// Phase 7.4c-submit-followup-present-2 — atomic-handoff swapchain
	// recreation hint. When non-NULL, passed into
	// VkSwapchainCreateInfoKHR.oldSwapchain so the new swapchain is created
	// "in place of" the old. The old VkSwapchainKHR is retired by
	// vkCreateSwapchainKHR; the caller MUST destroy the old ralSwapchain_t
	// wrapper AFTER successful create (can no longer use the old handle for
	// anything except vkDestroySwapchainKHR). NULL on initial boot create.
	void            *oldExternalSwapchain;
	// Phase 7.4c-submit-followup-present-2 — backend-extension pass-through
	// for the swapchain create info struct's extension chain. On Vulkan, this
	// is the VkSwapchainCreateInfoKHR.pNext pointer — used by the renderer's
	// existing Windows-HDR full-screen-exclusive (FSE) chain at vk.c's
	// _WIN32 + hdr_display_active branch. NULL on platforms / modes that
	// don't need an extension chain. Caller-owned (no lifecycle transfer);
	// must outlive the Ral_CreateSwapchain call.
	const void      *backendExtensionChain;
} ralSwapchainCreateInfo_t;

ralSwapchain_t *Ral_CreateSwapchain ( ralBackend_t *b, const ralSwapchainCreateInfo_t *ci );
void            Ral_DestroySwapchain( ralSwapchain_t *sc );

// Phase 7.4c-submit-followup-present-2 — accessor for the underlying
// VkSwapchainKHR (on Vulkan). Used by the renderer to mirror its legacy
// vk.swapchain field from the RAL-owned swapchain so the 100+ existing
// vk.swapchain references in vk.c work transparently. Returns NULL on
// NULL arg or pre-create state. Consumer casts back to VkSwapchainKHR.
void *Ral_GetSwapchainHandle( const ralSwapchain_t *sc );

// Phase 7.4c-submit-followup-present-1 — typed Present-info shape (mirrors
// ralSubmitInfo_t's array convention). Renderer builds in-place and passes
// to Ral_Present. The single-swapchain case sets numSwapchains=1.
typedef struct {
	ralSwapchain_t   **swapchains;       // array of swapchains to present (typ. 1)
	uint32_t           numSwapchains;
	const uint32_t    *imageIndices;     // parallel to swapchains[]; one image index per swapchain
	ralSemaphore_t   **waitSemaphores;   // semaphores Present must wait on (typically render-finished per swapchain image)
	uint32_t           numWaitSemaphores;
} ralPresentInfo_t;

// Phase 7.4c-submit-followup-present-1 — typed Acquire signature (replaces
// Phase 7.1 stub's outAcquireSem pattern). Caller owns the signal semaphore
// (per-frame ring's ralSemaphore_t) — RAL signals it but doesn't manage
// lifecycle. Returns ralSuccess on normal acquire; ralOutOfDate /
// ralSuboptimal indicate the renderer should drive a swapchain recreate
// via its existing vk_restart_swapchain path.
ralResult_t Ral_AcquireNextImage( ralSwapchain_t *sc,
                                  uint64_t        timeoutNs,
                                  ralSemaphore_t *signalSem,
                                  uint32_t       *outImageIndex,
                                  ralTexture_t  **outImage );

// Phase 7.4c-submit-followup-present-1 — typed Present (replaces Phase 7.1
// stub's flat 2-arg signature). Returns ralSuccess on normal present;
// ralOutOfDate / ralSuboptimal indicate the renderer should drive a
// swapchain recreate.
ralResult_t Ral_Present( ralBackend_t *b, const ralPresentInfo_t *info );

// HDR static metadata for HDR10 swapchains (set per Phase 7.8b). Coordinates
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
