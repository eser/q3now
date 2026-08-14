// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
//
// ral_vulkan_swapchain.c — Vulkan backend: presentation surface + HDR
// metadata.
//
// RAL queries the backend-owned surface and owns VkSwapchainKHR plus dynamic
// image/view storage. Each image has one canonical borrowed ralTexture_t
// wrapper (ownsImage=qfalse).
//
// The planned Windows DXGI rewrite happens in the platform layer
// (code/win32/), producing a VkSurfaceKHR (or surface-equivalent) that
// this backend consumes unchanged.

#include "ral_vulkan_internal.h"

static ralResult_t ralVk_SwapchainResult( VkResult result ) {
	switch ( result ) {
	case VK_SUCCESS:                    return ralSuccess;
	case VK_SUBOPTIMAL_KHR:             return ralSuboptimal;
	case VK_ERROR_OUT_OF_DATE_KHR:      return ralOutOfDate;
	case VK_ERROR_SURFACE_LOST_KHR:     return ralSurfaceLost;
	case VK_ERROR_OUT_OF_HOST_MEMORY:
	case VK_ERROR_OUT_OF_DEVICE_MEMORY: return ralErrorOutOfMemory;
	case VK_ERROR_DEVICE_LOST:          return ralErrorDeviceLost;
	case VK_TIMEOUT:
	case VK_NOT_READY:                  return ralTimeout;
	default:                            return ralErrorInitFailed;
	}
}

static uint32_t ralVk_ClampU32( uint32_t value, uint32_t minValue, uint32_t maxValue ) {
	if ( value < minValue ) return minValue;
	if ( value > maxValue ) return maxValue;
	return value;
}

static VkImageUsageFlags ralVk_SwapchainUsageToVk( ralTextureUsage_t usage ) {
	VkImageUsageFlags result = 0;
	if ( usage & RAL_TEXTURE_USAGE_SAMPLED )                  result |= VK_IMAGE_USAGE_SAMPLED_BIT;
	if ( usage & RAL_TEXTURE_USAGE_STORAGE )                  result |= VK_IMAGE_USAGE_STORAGE_BIT;
	if ( usage & RAL_TEXTURE_USAGE_COLOR_ATTACHMENT )         result |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	if ( usage & RAL_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT ) result |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
	if ( usage & RAL_TEXTURE_USAGE_TRANSFER_SRC )             result |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	if ( usage & RAL_TEXTURE_USAGE_TRANSFER_DST )             result |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	return result;
}

static VkCompositeAlphaFlagBitsKHR ralVk_SelectCompositeAlpha( VkCompositeAlphaFlagsKHR supported ) {
	static const VkCompositeAlphaFlagBitsKHR preference[] = {
		VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
		VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
		VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
		VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR
	};
	uint32_t i;
	for ( i = 0; i < sizeof( preference ) / sizeof( preference[0] ); i++ )
		if ( supported & preference[i] ) return preference[i];
	return (VkCompositeAlphaFlagBitsKHR)0;
}

static ralResult_t ralVk_QuerySurfaceFormats( ralBackend_t *b, VkSurfaceKHR surface,
	                                         VkSurfaceFormatKHR **outFormats, uint32_t *outCount ) {
	uint32_t attempt;
	*outFormats = NULL;
	*outCount = 0;
	for ( attempt = 0; attempt < 4; attempt++ ) {
		VkSurfaceFormatKHR *formats;
		uint32_t count = 0;
		VkResult result = b->vk.GetPhysicalDeviceSurfaceFormatsKHR( b->physicalDevice, surface, &count, NULL );
		if ( result != VK_SUCCESS ) return ralVk_SwapchainResult( result );
		if ( count == 0 ) return ralUnsupported;
		formats = (VkSurfaceFormatKHR *)malloc( sizeof( *formats ) * count );
		if ( !formats ) return ralErrorOutOfMemory;
		result = b->vk.GetPhysicalDeviceSurfaceFormatsKHR( b->physicalDevice, surface, &count, formats );
		if ( result == VK_SUCCESS ) {
			*outFormats = formats;
			*outCount = count;
			return ralSuccess;
		}
		free( formats );
		if ( result != VK_INCOMPLETE ) return ralVk_SwapchainResult( result );
	}
	return ralErrorInitFailed;
}

static ralResult_t ralVk_QueryPresentModes( ralBackend_t *b, VkSurfaceKHR surface,
	                                       VkPresentModeKHR **outModes, uint32_t *outCount ) {
	uint32_t attempt;
	*outModes = NULL;
	*outCount = 0;
	for ( attempt = 0; attempt < 4; attempt++ ) {
		VkPresentModeKHR *modes;
		uint32_t count = 0;
		VkResult result = b->vk.GetPhysicalDeviceSurfacePresentModesKHR( b->physicalDevice, surface, &count, NULL );
		if ( result != VK_SUCCESS ) return ralVk_SwapchainResult( result );
		if ( count == 0 ) return ralUnsupported;
		modes = (VkPresentModeKHR *)malloc( sizeof( *modes ) * count );
		if ( !modes ) return ralErrorOutOfMemory;
		result = b->vk.GetPhysicalDeviceSurfacePresentModesKHR( b->physicalDevice, surface, &count, modes );
		if ( result == VK_SUCCESS ) {
			*outModes = modes;
			*outCount = count;
			return ralSuccess;
		}
		free( modes );
		if ( result != VK_INCOMPLETE ) return ralVk_SwapchainResult( result );
	}
	return ralErrorInitFailed;
}

static qboolean ralVk_SelectSurfaceFormat( const ralSwapchainCreateInfo_t *ci,
	                                      const VkSurfaceFormatKHR *available, uint32_t availableCount,
	                                      ralSurfaceFormat_t *outRal, VkSurfaceFormatKHR *outVk ) {
	uint32_t p, a;
	if ( !ci->formatPreferences || ci->formatPreferenceCount == 0 ) return qfalse;
	for ( p = 0; p < ci->formatPreferenceCount; p++ ) {
		VkSurfaceFormatKHR wanted;
		wanted.format = ralVk_TranslateFormat( ci->formatPreferences[p].format );
		wanted.colorSpace = ralVk_TranslateColorSpace( ci->formatPreferences[p].colorSpace );
		if ( wanted.format == VK_FORMAT_UNDEFINED ) continue;
		for ( a = 0; a < availableCount; a++ ) {
			if ( ( available[a].format == wanted.format || available[a].format == VK_FORMAT_UNDEFINED )
			  && available[a].colorSpace == wanted.colorSpace ) {
				*outRal = ci->formatPreferences[p];
				*outVk = wanted;
				return qtrue;
			}
		}
	}
	return qfalse;
}

static qboolean ralVk_SelectPresentMode( const ralSwapchainCreateInfo_t *ci,
	                                    const VkPresentModeKHR *available, uint32_t availableCount,
	                                    ralPresentMode_t *outRal, VkPresentModeKHR *outVk ) {
	ralPresentMode_t fifo = RAL_PRESENT_FIFO;
	const ralPresentMode_t *preferences = ci->presentModePreferences;
	uint32_t preferenceCount = ci->presentModePreferenceCount;
	uint32_t p, a;
	if ( !preferences || preferenceCount == 0 ) {
		preferences = &fifo;
		preferenceCount = 1;
	}
	for ( p = 0; p < preferenceCount; p++ ) {
		VkPresentModeKHR wanted = ralVk_TranslatePresentMode( preferences[p] );
		for ( a = 0; a < availableCount; a++ ) {
			if ( available[a] == wanted ) {
				*outRal = preferences[p];
				*outVk = wanted;
				return qtrue;
			}
		}
	}
	return qfalse;
}

// Deterministic surface-policy selection and resource materialization.
// ════════════════════════════════════════════════════════════════════════
// ════════════════════════════════════════════════════════════════════════
// ════════════════════════════════════════════════════════════════════════
// ════════════════════════════════════════════════════════════════════════
static void ralVk_DestroySwapchainContents( ralSwapchain_t *sc ) {
	uint32_t i;
	if ( !sc ) return;
	for ( i = 0; i < sc->imageCount; i++ ) {
		if ( sc->adoptedImages && sc->adoptedImages[i] ) {
			Ral_DestroyTexture( sc->adoptedImages[i] );
			sc->adoptedImages[i] = NULL;
		}
		if ( sc->imageViews && sc->imageViews[i] != VK_NULL_HANDLE && sc->backend
		  && sc->backend->vk.DestroyImageView ) {
			sc->backend->vk.DestroyImageView( sc->backend->device, sc->imageViews[i], NULL );
			sc->imageViews[i] = VK_NULL_HANDLE;
		}
	}
	if ( sc->swapchain != VK_NULL_HANDLE && sc->backend && sc->backend->vk.DestroySwapchainKHR ) {
		sc->backend->vk.DestroySwapchainKHR( sc->backend->device, sc->swapchain, NULL );
		sc->swapchain = VK_NULL_HANDLE;
	}
	free( sc->adoptedImages );
	free( sc->imageViews );
	free( sc->images );
	free( sc->imageStates );
	sc->adoptedImages = NULL;
	sc->imageViews = NULL;
	sc->images = NULL;
	sc->imageStates = NULL;
	sc->imageCount = 0;
}

static ralResult_t ralVk_MaterializeSwapchainImages( ralSwapchain_t *sc ) {
	uint32_t attempt;
	for ( attempt = 0; attempt < 4; attempt++ ) {
		uint32_t count = 0;
		VkResult result = sc->backend->vk.GetSwapchainImagesKHR(
			sc->backend->device, sc->swapchain, &count, NULL );
		if ( result != VK_SUCCESS ) return ralVk_SwapchainResult( result );
		if ( count == 0 ) return ralErrorInitFailed;
		sc->images = (VkImage *)calloc( count, sizeof( *sc->images ) );
		if ( !sc->images ) return ralErrorOutOfMemory;
		result = sc->backend->vk.GetSwapchainImagesKHR(
			sc->backend->device, sc->swapchain, &count, sc->images );
		if ( result == VK_SUCCESS ) {
			sc->imageCount = count;
			break;
		}
		free( sc->images );
		sc->images = NULL;
		if ( result != VK_INCOMPLETE ) return ralVk_SwapchainResult( result );
	}
	if ( sc->imageCount == 0 ) return ralErrorInitFailed;
	sc->imageViews = (VkImageView *)calloc( sc->imageCount, sizeof( *sc->imageViews ) );
	sc->adoptedImages = (ralTexture_t **)calloc( sc->imageCount, sizeof( *sc->adoptedImages ) );
	sc->imageStates = (uint8_t *)calloc( sc->imageCount, sizeof( *sc->imageStates ) );
	if ( !sc->imageViews || !sc->adoptedImages || !sc->imageStates ) return ralErrorOutOfMemory;
	for ( attempt = 0; attempt < sc->imageCount; attempt++ ) {
		VkImageViewCreateInfo vci;
		RAL_ZERO( vci );
		vci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		vci.image = sc->images[attempt];
		vci.viewType = VK_IMAGE_VIEW_TYPE_2D;
		vci.format = sc->vkFormat;
		vci.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		vci.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		vci.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		vci.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
		vci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		vci.subresourceRange.levelCount = 1;
		vci.subresourceRange.layerCount = 1;
		if ( sc->backend->vk.CreateImageView( sc->backend->device, &vci, NULL,
		                                     &sc->imageViews[attempt] ) != VK_SUCCESS )
			return ralErrorInitFailed;
		sc->adoptedImages[attempt] = Ral_AdoptTexture(
			sc->backend, (void *)sc->images[attempt], (void *)sc->imageViews[attempt],
			sc->format, sc->extent.width, sc->extent.height, VK_IMAGE_ASPECT_COLOR_BIT,
			"wired-swapchain-image" );
		if ( !sc->adoptedImages[attempt] ) return ralErrorOutOfMemory;
	}
	return ralSuccess;
}

static ralResult_t ralVk_RecreateFailure( ralSwapchain_t *old,
	                                      ralSwapchain_t **inOut,
	                                      ralResult_t result ) {
	if ( inOut ) *inOut = NULL;
	if ( old ) {
		ralVk_DestroySwapchainContents( old );
		free( old );
	}
	return result;
}

ralResult_t Ral_CreateOrRecreateSwapchain( ralBackend_t *b,
	                                       const ralSwapchainCreateInfo_t *ci,
	                                       ralSwapchain_t **inOut ) {
	VkSwapchainCreateInfoKHR sci;
	ralSwapchain_t *sc;
	VkSurfaceCapabilitiesKHR caps;
	VkSurfaceFormatKHR *formats = NULL, selectedFormat;
	VkPresentModeKHR *modes = NULL, selectedMode;
	ralSurfaceFormat_t selectedRalFormat;
	ralPresentMode_t selectedRalMode;
	VkCompositeAlphaFlagBitsKHR compositeAlpha;
	VkImageUsageFlags requiredVkUsage;
	VkResult r;
	ralResult_t result;
	uint32_t formatCount = 0, modeCount = 0, desiredCount;
	ralSwapchain_t *old;
	qboolean recreate;

	if ( !b || !ci || !inOut || !b->surface || !ci->formatPreferences
	  || ci->formatPreferenceCount == 0
	  || !( ci->requiredUsage & RAL_TEXTURE_USAGE_COLOR_ATTACHMENT ) )
		return ralErrorInvalidArgument;
	old = *inOut;
	recreate = old != NULL;
	if ( recreate && ( old->backend != b || old->surface != b->surface ) )
		return ralErrorInvalidArgument;
	if ( !b->vk.GetPhysicalDeviceSurfaceCapabilitiesKHR || !b->vk.GetPhysicalDeviceSurfaceFormatsKHR
	  || !b->vk.GetPhysicalDeviceSurfacePresentModesKHR || !b->vk.CreateSwapchainKHR
	  || !b->vk.DestroySwapchainKHR || !b->vk.GetSwapchainImagesKHR
	  || !b->vk.CreateImageView || !b->vk.DestroyImageView ) return ralUnsupported;

	r = b->vk.GetPhysicalDeviceSurfaceCapabilitiesKHR( b->physicalDevice, b->surface, &caps );
	if ( r != VK_SUCCESS ) return ralVk_SwapchainResult( r );
	result = ralVk_QuerySurfaceFormats( b, b->surface, &formats, &formatCount );
	if ( result != ralSuccess ) return result;
	result = ralVk_QueryPresentModes( b, b->surface, &modes, &modeCount );
	if ( result != ralSuccess ) { free( formats ); return result; }
	if ( !ralVk_SelectSurfaceFormat( ci, formats, formatCount, &selectedRalFormat, &selectedFormat )
	  || !ralVk_SelectPresentMode( ci, modes, modeCount, &selectedRalMode, &selectedMode ) ) {
		free( modes ); free( formats ); return ralUnsupported;
	}
	free( modes ); free( formats );
	requiredVkUsage = ralVk_SwapchainUsageToVk( ci->requiredUsage );
	if ( ( requiredVkUsage & caps.supportedUsageFlags ) != requiredVkUsage ) return ralUnsupported;
	compositeAlpha = ralVk_SelectCompositeAlpha( caps.supportedCompositeAlpha );
	if ( !compositeAlpha ) return ralUnsupported;

	sc = (ralSwapchain_t *)calloc( 1, sizeof( *sc ) );
	if ( !sc ) return ralErrorOutOfMemory;
	sc->backend = b;
	sc->surface = b->surface;
	sc->format = selectedRalFormat.format;
	sc->colorSpace = selectedRalFormat.colorSpace;
	sc->presentMode = selectedRalMode;
	sc->vkFormat = selectedFormat.format;
	sc->vkColorSpace = selectedFormat.colorSpace;
	sc->vkPresentMode = selectedMode;
	sc->usage = ci->requiredUsage;
	if ( caps.currentExtent.width != UINT32_MAX ) sc->extent = caps.currentExtent;
	else {
		if ( ci->desiredWidth == 0 || ci->desiredHeight == 0 ) {
			free( sc );
			return ralOutOfDate; /* minimized/suspended: keep current generation */
		}
		sc->extent.width = ralVk_ClampU32( ci->desiredWidth, caps.minImageExtent.width, caps.maxImageExtent.width );
		sc->extent.height = ralVk_ClampU32( ci->desiredHeight, caps.minImageExtent.height, caps.maxImageExtent.height );
	}
	if ( sc->extent.width == 0 || sc->extent.height == 0 ) {
		free( sc );
		return ralOutOfDate;
	}
	desiredCount = ci->desiredImageCount ? ci->desiredImageCount : 2u;
	desiredCount = ralVk_ClampU32( desiredCount, caps.minImageCount,
	                             caps.maxImageCount ? caps.maxImageCount : UINT32_MAX );

	RAL_ZERO( sci );
	sci.sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	sci.pNext            = ci->backendExtensionChain;
	sci.surface          = sc->surface;
	sci.minImageCount    = desiredCount;
	sci.imageFormat      = sc->vkFormat;
	sci.imageColorSpace  = sc->vkColorSpace;
	sci.imageExtent      = sc->extent;
	sci.imageArrayLayers = 1;
	sci.imageUsage       = requiredVkUsage;
	sci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	sci.preTransform     = caps.currentTransform;
	sci.compositeAlpha   = compositeAlpha;
	sci.presentMode      = sc->vkPresentMode;
	sci.clipped          = VK_TRUE;
	sci.oldSwapchain     = old ? old->swapchain : VK_NULL_HANDLE;

	// Vulkan's oldSwapchain handoff is one-way: as soon as native create is
	// attempted the old generation may no longer be acquired, even when create
	// fails. Unpublish it before the call and make every post-attempt failure
	// converge on an explicit no-current-swapchain state.
	if ( recreate ) {
		if ( !b->vk.DeviceWaitIdle ) { free( sc ); return ralUnsupported; }
		r = b->vk.DeviceWaitIdle( b->device );
		if ( r != VK_SUCCESS ) { free( sc ); return ralVk_SwapchainResult( r ); }
		*inOut = NULL;
	}
	r = b->vk.CreateSwapchainKHR( b->device, &sci, NULL, &sc->swapchain );
	if ( r != VK_SUCCESS ) {
		RAL_VK_LOG( SEV_ERROR, "Ral_CreateOrRecreateSwapchain: vkCreateSwapchainKHR returned %d (format=%d colorSpace=%d presentMode=%d %ux%u)\n",
		        (int)r, (int)sc->vkFormat, (int)sc->vkColorSpace, (int)sc->vkPresentMode, sc->extent.width, sc->extent.height );
		ralVk_DestroySwapchainContents( sc );
		free( sc );
		return recreate
		     ? ralVk_RecreateFailure( old, inOut, ralVk_SwapchainResult( r ) )
		     : ralVk_SwapchainResult( r );
	}
	result = ralVk_MaterializeSwapchainImages( sc );
	if ( result != ralSuccess ) {
		ralVk_DestroySwapchainContents( sc );
		free( sc );
		return recreate ? ralVk_RecreateFailure( old, inOut, result ) : result;
	}
	if ( old && old->hasHdrMetadata ) {
		sc->hasHdrMetadata = qtrue;
		sc->hdrMetadata = old->hdrMetadata;
		if ( b->vk.SetHdrMetadataEXT )
			b->vk.SetHdrMetadataEXT( b->device, 1, &sc->swapchain, &sc->hdrMetadata );
	}
	if ( old ) { ralVk_DestroySwapchainContents( old ); free( old ); }
	sc->generation = ++b->nextSwapchainGeneration;
	if ( sc->generation == 0 ) sc->generation = ++b->nextSwapchainGeneration;
	*inOut = sc;
	return ralSuccess;
}

// ════════════════════════════════════════════════════════════════════════
// Ral_DestroySwapchain — free adopted-image wrappers, destroy VkSwapchainKHR.
// The surface remains backend-owned.
// ════════════════════════════════════════════════════════════════════════
void Ral_DestroySwapchain( ralSwapchain_t *sc ) {
	if ( !sc ) return;
	ralVk_DestroySwapchainContents( sc );
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

qboolean Ral_GetSwapchainInfo( const ralSwapchain_t *sc, ralSwapchainInfo_t *outInfo ) {
	if ( !outInfo ) return qfalse;
	RAL_ZERO( *outInfo );
	if ( !sc || sc->swapchain == VK_NULL_HANDLE ) return qfalse;
	outInfo->generation = sc->generation;
	outInfo->width = sc->extent.width;
	outInfo->height = sc->extent.height;
	outInfo->format = sc->format;
	outInfo->colorSpace = sc->colorSpace;
	outInfo->presentMode = sc->presentMode;
	outInfo->imageCount = sc->imageCount;
	outInfo->usage = sc->usage;
	return qtrue;
}

ralTexture_t *Ral_GetSwapchainImage( const ralSwapchain_t *sc, uint32_t imageIndex ) {
	if ( !sc || !sc->adoptedImages || imageIndex >= sc->imageCount ) return NULL;
	return sc->adoptedImages[imageIndex];
}

// Ral_GetSwapchainExtent — the cached swapchain image extent (sc->extent, set
// from the surface currentExtent at create time = physical present pixels). The
// correct render-target size for the gamma present-blit; writes 0 on NULL.
void Ral_GetSwapchainExtent( const ralSwapchain_t *sc, uint32_t *width, uint32_t *height ) {
	if ( width )  *width  = sc ? sc->extent.width  : 0;
	if ( height ) *height = sc ? sc->extent.height : 0;
}

// ════════════════════════════════════════════════════════════════════════
// Ral_AcquireNextImage — real body.
// Drives vkAcquireNextImageKHR; signals caller's semaphore on success;
// translates VkResult to ralResult_t. ralOutOfDate / ralSuboptimal flow
// the renderer into its vk_restart_swapchain recreate path.
// ════════════════════════════════════════════════════════════════════════
ralResult_t Ral_AcquireNextImage( ralSwapchain_t *sc, uint64_t timeoutNs, ralSemaphore_t *signalSem, uint32_t *outImageIndex, ralTexture_t **outImage ) {
	ralBackend_t *b;
	VkSemaphore   vkSem = VK_NULL_HANDLE;
	VkResult      res;
	uint32_t      idx = UINT32_MAX;

	if ( outImageIndex == NULL || outImage == NULL ) return ralErrorInvalidArgument;
	*outImageIndex = UINT32_MAX;
	*outImage = NULL;
	if ( sc == NULL || sc->swapchain == VK_NULL_HANDLE || signalSem == NULL )
		return ralErrorInvalidArgument;
	b = sc->backend;
	if ( b == NULL || !b->vk.AcquireNextImageKHR ) return ralErrorInvalidArgument;
	if ( signalSem->backend != b || signalSem->sem == VK_NULL_HANDLE )
		return ralErrorInvalidArgument;
	vkSem = signalSem->sem;

	res = b->vk.AcquireNextImageKHR( b->device, sc->swapchain, timeoutNs, vkSem, VK_NULL_HANDLE, &idx );

	if ( res == VK_SUCCESS || res == VK_SUBOPTIMAL_KHR ) {
		if ( idx >= sc->imageCount || !sc->adoptedImages || !sc->adoptedImages[idx]
		  || sc->adoptedImages[idx]->defaultView == VK_NULL_HANDLE || !sc->imageStates
		  || sc->imageStates[idx] != RAL_VK_SWAPCHAIN_IMAGE_AVAILABLE )
			return ralErrorUnknown;
		sc->imageStates[idx] = RAL_VK_SWAPCHAIN_IMAGE_ACQUIRED;
		*outImageIndex = idx;
		*outImage = sc->adoptedImages[idx];
		return res == VK_SUCCESS ? ralSuccess : ralSuboptimal;
	}
	return ralVk_SwapchainResult( res );
}

// ════════════════════════════════════════════════════════════════════════
// Ral_Present: validated dynamic batch with no fixed image-count ceiling.
// ════════════════════════════════════════════════════════════════════════
ralResult_t Ral_Present( ralBackend_t *b, const ralPresentInfo_t *info ) {
	VkSwapchainKHR   *vkSwapchains;
	VkSemaphore      *vkWaits = NULL;
	VkPresentInfoKHR  pi;
	VkResult          res;
	uint32_t          i;

	if ( b == NULL || info == NULL || info->swapchains == NULL
	  || info->numSwapchains == 0 || info->imageIndices == NULL
	  || !b->vk.QueuePresentKHR
	  || ( info->numWaitSemaphores > 0 && info->waitSemaphores == NULL ) )
		return ralErrorInvalidArgument;
	vkSwapchains = (VkSwapchainKHR *)malloc( sizeof( *vkSwapchains ) * info->numSwapchains );
	if ( !vkSwapchains ) return ralErrorOutOfMemory;
	if ( info->numWaitSemaphores ) {
		vkWaits = (VkSemaphore *)malloc( sizeof( *vkWaits ) * info->numWaitSemaphores );
		if ( !vkWaits ) { free( vkSwapchains ); return ralErrorOutOfMemory; }
	}

	for ( i = 0; i < info->numSwapchains; i++ ) {
		if ( info->swapchains[i] == NULL || info->swapchains[i]->backend != b
		  || info->swapchains[i]->swapchain == VK_NULL_HANDLE
		  || info->imageIndices[i] >= info->swapchains[i]->imageCount
		  || !info->swapchains[i]->imageStates
		  || info->swapchains[i]->imageStates[info->imageIndices[i]] != RAL_VK_SWAPCHAIN_IMAGE_PREPARED ) {
			free( vkWaits ); free( vkSwapchains ); return ralErrorInvalidArgument;
		}
		vkSwapchains[i] = info->swapchains[i]->swapchain;
	}
	for ( i = 0; i < info->numWaitSemaphores; i++ ) {
		if ( !info->waitSemaphores[i] || info->waitSemaphores[i]->backend != b
		  || info->waitSemaphores[i]->sem == VK_NULL_HANDLE ) {
			free( vkWaits ); free( vkSwapchains ); return ralErrorInvalidArgument;
		}
		vkWaits[i] = info->waitSemaphores[i]->sem;
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
	for ( i = 0; i < info->numSwapchains; i++ )
		info->swapchains[i]->imageStates[info->imageIndices[i]] = RAL_VK_SWAPCHAIN_IMAGE_AVAILABLE;
	free( vkWaits );
	free( vkSwapchains );
	return ralVk_SwapchainResult( res );
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
