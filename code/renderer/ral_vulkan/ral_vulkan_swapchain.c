// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
//
// ral_vulkan_swapchain.c — Vulkan backend: presentation surface + HDR
// metadata.
//
// Phase 7.4c-submit-followup-present-1: real bodies for Create / Destroy /
// SetHdrMetadata; signature-updated stub bodies for AcquireNextImage /
// Present (their real implementations land in -2). RAL adopts the
// renderer-created VkSurfaceKHR via ralSwapchainCreateInfo_t.externalSurface
// — RAL OWNS the VkSwapchainKHR + swapchain images (adopted as ralTexture_t
// wrappers with ownsImage=qfalse since they're owned by the swapchain
// implementation, not by the per-image VkImage allocation).
//
// Phase 7.8b's planned Windows DXGI rewrite happens in the platform layer
// (code/win32/), producing a VkSurfaceKHR (or surface-equivalent) that
// this backend consumes unchanged.

#include "ral_vulkan_internal.h"

// Phase 7.4c-submit-followup-present-2 — phase-tag marker retired. All 5
// swapchain entry points (Create / Destroy / AcquireNextImage / Present /
// SetSwapchainHdrMetadata) have real bodies. Phase 7.8b's planned Windows
// DXGI rewrite happens in the platform layer (code/win32/), producing a
// VkSurfaceKHR-equivalent that this backend adopts unchanged via
// ralSwapchainCreateInfo_t.externalSurface — no further RAL backend changes
// expected for 7.8b.

// ════════════════════════════════════════════════════════════════════════
// format / colorspace / present-mode mapping helpers (ralFormat_t → Vk*)
// ════════════════════════════════════════════════════════════════════════
static VkColorSpaceKHR ralVk_TranslateColorSpace( ralColorSpace_t cs ) {
	switch ( cs ) {
	case RAL_COLORSPACE_EXTENDED_SRGB_LINEAR: return VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT;
	case RAL_COLORSPACE_HDR10_ST2084:         return VK_COLOR_SPACE_HDR10_ST2084_EXT;
	case RAL_COLORSPACE_HDR10_HLG:            return VK_COLOR_SPACE_HDR10_HLG_EXT;
	case RAL_COLORSPACE_DISPLAY_P3:           return VK_COLOR_SPACE_DISPLAY_P3_NONLINEAR_EXT;
	case RAL_COLORSPACE_SRGB_NONLINEAR:
	default:                                   return VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
	}
}

static VkPresentModeKHR ralVk_TranslatePresentMode( ralPresentMode_t pm ) {
	switch ( pm ) {
	case RAL_PRESENT_MAILBOX:   return VK_PRESENT_MODE_MAILBOX_KHR;
	case RAL_PRESENT_IMMEDIATE: return VK_PRESENT_MODE_IMMEDIATE_KHR;
	case RAL_PRESENT_FIFO:
	default:                    return VK_PRESENT_MODE_FIFO_KHR;
	}
}

// ════════════════════════════════════════════════════════════════════════
// Ral_CreateSwapchain — adopt externalSurface, create VkSwapchainKHR +
// adopt each swapchain image as ralTexture_t (ownsImage=qfalse).
// ════════════════════════════════════════════════════════════════════════
ralSwapchain_t *Ral_CreateSwapchain( ralBackend_t *b, const ralSwapchainCreateInfo_t *ci ) {
	VkSwapchainCreateInfoKHR sci;
	ralSwapchain_t          *sc;
	uint32_t                 imageCount = 0;
	VkResult                 r;
	uint32_t                 i;

	if ( !b || !ci || !ci->externalSurface ) {
		ri.Log( SEV_WARN, "[RAL] Ral_CreateSwapchain: bad args (b=%p, ci=%p, externalSurface=%p)\n",
		        (void *)b, (const void *)ci, ci ? ci->externalSurface : NULL );
		return NULL;
	}

	sc = (ralSwapchain_t *)malloc( sizeof( *sc ) );
	if ( !sc ) return NULL;
	RAL_ZERO( *sc );
	sc->backend       = b;
	sc->surface       = (VkSurfaceKHR)ci->externalSurface;
	sc->ownsSurface   = qfalse;
	sc->vkFormat      = ralVk_TranslateFormat   ( ci->format     );
	sc->vkColorSpace  = ralVk_TranslateColorSpace( ci->colorSpace );
	sc->vkPresentMode = ralVk_TranslatePresentMode( ci->presentMode );
	sc->extent.width  = ci->width;
	sc->extent.height = ci->height;

	RAL_ZERO( sci );
	sci.sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	sci.pNext            = ci->backendExtensionChain;   // Phase 7.4c-submit-followup-present-2 — caller-owned extension chain (Win32 HDR FSE on q3now's Windows build)
	sci.surface          = sc->surface;
	sci.minImageCount    = ci->minImageCount ? ci->minImageCount : 2u;
	sci.imageFormat      = sc->vkFormat;
	sci.imageColorSpace  = sc->vkColorSpace;
	sci.imageExtent      = sc->extent;
	sci.imageArrayLayers = 1;
	sci.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	sci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	sci.preTransform     = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
	sci.compositeAlpha   = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	sci.presentMode      = sc->vkPresentMode;
	sci.clipped          = VK_TRUE;
	// Phase 7.4c-submit-followup-present-2 — atomic-handoff support. When the
	// renderer drives a swapchain recreate (r_fbo / r_ext_multisample / r_hdrDisplay
	// toggle, or VK_ERROR_OUT_OF_DATE_KHR recovery), it passes the prior RAL
	// swapchain's VkSwapchainKHR here so the new swapchain transitions atomically
	// (retires the old, no two-non-retired-on-one-surface hazard). NULL on boot.
	sci.oldSwapchain     = (VkSwapchainKHR)ci->oldExternalSwapchain;

	r = b->vk.CreateSwapchainKHR( b->device, &sci, NULL, &sc->swapchain );
	if ( r != VK_SUCCESS ) {
		ri.Log( SEV_ERROR, "[RAL] Ral_CreateSwapchain: vkCreateSwapchainKHR returned %d (format=%d colorSpace=%d presentMode=%d %ux%u)\n",
		        (int)r, (int)sc->vkFormat, (int)sc->vkColorSpace, (int)sc->vkPresentMode, sc->extent.width, sc->extent.height );
		free( sc );
		return NULL;
	}

	// enumerate swapchain images + adopt each as ralTexture_t
	b->vk.GetSwapchainImagesKHR( b->device, sc->swapchain, &imageCount, NULL );
	if ( imageCount > MAX_RAL_SWAPCHAIN_IMAGES ) {
		ri.Log( SEV_WARN, "[RAL] Ral_CreateSwapchain: swapchain image count %u exceeds MAX_RAL_SWAPCHAIN_IMAGES %u; clamping\n",
		        imageCount, MAX_RAL_SWAPCHAIN_IMAGES );
		imageCount = MAX_RAL_SWAPCHAIN_IMAGES;
	}
	sc->imageCount = imageCount;
	b->vk.GetSwapchainImagesKHR( b->device, sc->swapchain, &sc->imageCount, sc->images );

	for ( i = 0; i < sc->imageCount; i++ ) {
		sc->adoptedImages[i] = Ral_AdoptTexture( b,
			(void *)sc->images[i],
			sc->extent.width, sc->extent.height,
			VK_IMAGE_ASPECT_COLOR_BIT,
			"wired-swapchain-image" );
	}

	return sc;
}

// ════════════════════════════════════════════════════════════════════════
// Ral_DestroySwapchain — free adopted-image wrappers, destroy VkSwapchainKHR.
// Does NOT destroy the surface (ownsSurface=qfalse).
// ════════════════════════════════════════════════════════════════════════
void Ral_DestroySwapchain( ralSwapchain_t *sc ) {
	uint32_t i;
	if ( !sc ) return;
	for ( i = 0; i < sc->imageCount; i++ ) {
		if ( sc->adoptedImages[i] ) {
			Ral_DestroyTexture( sc->adoptedImages[i] );   // ownsImage=qfalse → wrapper-only free
			sc->adoptedImages[i] = NULL;
		}
	}
	if ( sc->swapchain != VK_NULL_HANDLE && sc->backend ) {
		sc->backend->vk.DestroySwapchainKHR( sc->backend->device, sc->swapchain, NULL );
		sc->swapchain = VK_NULL_HANDLE;
	}
	// surface stays — renderer's qvkDestroySurfaceKHR teardown path owns it.
	free( sc );
}

// ════════════════════════════════════════════════════════════════════════
// Ral_GetSwapchainHandle — accessor for the underlying VkSwapchainKHR.
// Consumed by the renderer's vk_create_swapchain to mirror vk.swapchain
// from the RAL-owned VkSwapchainKHR (preserves the 100+ existing
// vk.swapchain references in vk.c without refactoring each).
// ════════════════════════════════════════════════════════════════════════
void *Ral_GetSwapchainHandle( const ralSwapchain_t *sc ) {
	return sc ? (void *)sc->swapchain : NULL;
}

// ════════════════════════════════════════════════════════════════════════
// Ral_AcquireNextImage — real body (Phase 7.4c-submit-followup-present-2).
// Drives vkAcquireNextImageKHR; signals caller's semaphore on success;
// translates VkResult to ralResult_t. ralOutOfDate / ralSuboptimal flow
// the renderer into its vk_restart_swapchain recreate path.
// ════════════════════════════════════════════════════════════════════════
ralResult_t Ral_AcquireNextImage( ralSwapchain_t *sc, uint64_t timeoutNs, ralSemaphore_t *signalSem, uint32_t *outImageIndex, ralTexture_t **outImage ) {
	ralBackend_t *b;
	VkSemaphore   vkSem;
	VkResult      res;
	uint32_t      idx = 0;

	if ( sc == NULL || outImageIndex == NULL || outImage == NULL )
		return ralErrorInvalidArgument;
	b = sc->backend;
	if ( b == NULL )
		return ralErrorInvalidArgument;

	vkSem = ( signalSem != NULL ) ? (VkSemaphore)Ral_GetSemaphoreHandle( signalSem ) : VK_NULL_HANDLE;

	res = b->vk.AcquireNextImageKHR( b->device, sc->swapchain, timeoutNs, vkSem, VK_NULL_HANDLE, &idx );

	if ( res == VK_SUCCESS ) {
		*outImageIndex = idx;
		*outImage      = ( idx < sc->imageCount ) ? sc->adoptedImages[ idx ] : NULL;
		return ralSuccess;
	}
	if ( res == VK_SUBOPTIMAL_KHR ) {
		*outImageIndex = idx;
		*outImage      = ( idx < sc->imageCount ) ? sc->adoptedImages[ idx ] : NULL;
		return ralSuboptimal;
	}
	if ( res == VK_ERROR_OUT_OF_DATE_KHR )
		return ralOutOfDate;
	if ( res == VK_TIMEOUT )
		return ralErrorUnknown;

	ri.Log( SEV_ERROR, "[RAL] Ral_AcquireNextImage: vkAcquireNextImageKHR returned %d\n", (int)res );
	return ralErrorDeviceLost;
}

// ════════════════════════════════════════════════════════════════════════
// Ral_Present — real body (Phase 7.4c-submit-followup-present-2). Drives
// vkQueuePresentKHR on the graphics queue. Builds VkPresentInfoKHR from
// the typed ralPresentInfo_t array shape; unwraps adopted semaphores via
// Ral_GetSemaphoreHandle. Caps at MAX_RAL_SWAPCHAIN_IMAGES per scratch
// stack arrays.
// ════════════════════════════════════════════════════════════════════════
ralResult_t Ral_Present( ralBackend_t *b, const ralPresentInfo_t *info ) {
	VkSwapchainKHR    vkSwapchains[ MAX_RAL_SWAPCHAIN_IMAGES ];
	VkSemaphore       vkWaits     [ MAX_RAL_SWAPCHAIN_IMAGES ];
	VkPresentInfoKHR  pi;
	VkResult          res;
	uint32_t          i;

	if ( b == NULL || info == NULL || info->swapchains == NULL
	  || info->numSwapchains == 0 || info->imageIndices == NULL )
		return ralErrorInvalidArgument;
	if ( info->numSwapchains    > MAX_RAL_SWAPCHAIN_IMAGES )
		return ralErrorInvalidArgument;
	if ( info->numWaitSemaphores > MAX_RAL_SWAPCHAIN_IMAGES )
		return ralErrorInvalidArgument;

	for ( i = 0; i < info->numSwapchains; i++ ) {
		if ( info->swapchains[i] == NULL ) return ralErrorInvalidArgument;
		vkSwapchains[i] = info->swapchains[i]->swapchain;
	}
	for ( i = 0; i < info->numWaitSemaphores; i++ ) {
		vkWaits[i] = (VkSemaphore)Ral_GetSemaphoreHandle( info->waitSemaphores[i] );
	}

	RAL_ZERO( pi );
	pi.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	pi.swapchainCount     = info->numSwapchains;
	pi.pSwapchains        = vkSwapchains;
	pi.pImageIndices      = info->imageIndices;
	pi.waitSemaphoreCount = info->numWaitSemaphores;
	pi.pWaitSemaphores    = ( info->numWaitSemaphores > 0 ) ? vkWaits : NULL;
	pi.pResults           = NULL;

	res = b->vk.QueuePresentKHR( b->queues[ RAL_QUEUE_GRAPHICS ], &pi );

	if ( res == VK_SUCCESS )               return ralSuccess;
	if ( res == VK_SUBOPTIMAL_KHR )        return ralSuboptimal;
	if ( res == VK_ERROR_OUT_OF_DATE_KHR ) return ralOutOfDate;
	if ( res == VK_ERROR_DEVICE_LOST )     return ralErrorDeviceLost;

	ri.Log( SEV_ERROR, "[RAL] Ral_Present: vkQueuePresentKHR returned %d\n", (int)res );
	return ralErrorUnknown;
}

// ════════════════════════════════════════════════════════════════════════
// Ral_SetSwapchainHdrMetadata — cache + forward to vkSetHdrMetadataEXT
// when VK_EXT_hdr_metadata is available. ralHdrMetadata_t fields map 1:1
// to VkHdrMetadataEXT (primaries + luminance + maxCLL + maxFALL).
// ════════════════════════════════════════════════════════════════════════
void Ral_SetSwapchainHdrMetadata( ralSwapchain_t *sc, const ralHdrMetadata_t *md ) {
	VkHdrMetadataEXT vk_md;
	if ( !sc || !md ) return;

	RAL_ZERO( vk_md );
	vk_md.sType                       = VK_STRUCTURE_TYPE_HDR_METADATA_EXT;
	vk_md.displayPrimaryRed.x         = md->displayPrimaryRed  [0];
	vk_md.displayPrimaryRed.y         = md->displayPrimaryRed  [1];
	vk_md.displayPrimaryGreen.x       = md->displayPrimaryGreen[0];
	vk_md.displayPrimaryGreen.y       = md->displayPrimaryGreen[1];
	vk_md.displayPrimaryBlue.x        = md->displayPrimaryBlue [0];
	vk_md.displayPrimaryBlue.y        = md->displayPrimaryBlue [1];
	vk_md.whitePoint.x                = md->whitePoint         [0];
	vk_md.whitePoint.y                = md->whitePoint         [1];
	vk_md.maxLuminance                = md->maxLuminance;
	vk_md.minLuminance                = md->minLuminance;
	vk_md.maxContentLightLevel        = md->maxContentLightLevel;
	vk_md.maxFrameAverageLightLevel   = md->maxFrameAverageLightLevel;

	sc->hdrMetadata     = vk_md;
	sc->hasHdrMetadata  = qtrue;

	if ( sc->backend && sc->backend->vk.SetHdrMetadataEXT && sc->swapchain != VK_NULL_HANDLE ) {
		sc->backend->vk.SetHdrMetadataEXT( sc->backend->device, 1, &sc->swapchain, &vk_md );
	}
}
