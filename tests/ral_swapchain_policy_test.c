// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "ral_vulkan_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(expr) do { if ( !(expr) ) { \
	fprintf( stderr, "CHECK failed at %s:%d: %s\n", __FILE__, __LINE__, #expr ); \
	return 1; \
} } while ( 0 )
#define HANDLE(type, value) ((type)(uintptr_t)(value))

static VkSurfaceCapabilitiesKHR g_caps;
static VkSurfaceFormatKHR g_formats[2];
static VkPresentModeKHR g_modes[2];
static VkImage g_images[9];
static VkSwapchainCreateInfoKHR g_lastCreate;
static VkResult g_createResult;
static VkResult g_acquireResult;
static uint32_t g_acquireIndex;
static uint32_t g_createSerial;
static uint32_t g_waitIdleCalls;
static uint32_t g_presentCalls;
static uint32_t g_destroyTextureCalls;
static uint32_t g_destroyViewCalls;
static uint32_t g_destroySwapchainCalls;
static VkSwapchainKHR g_lastDestroyedSwapchain;
static uint32_t g_imageCount;
static int32_t g_failViewAt;
static int32_t g_failAdoptAt;
static uint32_t g_adoptCalls;
static VkResult g_imageFillResult;
static VkResult g_waitIdleResult;
static VkResult g_presentResult;

void ralVk_Logf( const ralBackend_t *b, ralLogSeverity_t severity, const char *fmt, ... ) {
	(void)b; (void)severity; (void)fmt;
}

VkFormat ralVk_TranslateFormat( ralFormat_t format ) {
	switch ( format ) {
	case RAL_FORMAT_B8G8R8A8_UNORM: return VK_FORMAT_B8G8R8A8_UNORM;
	case RAL_FORMAT_A2B10G10R10_UNORM: return VK_FORMAT_A2B10G10R10_UNORM_PACK32;
	case RAL_FORMAT_R16G16B16A16_SFLOAT: return VK_FORMAT_R16G16B16A16_SFLOAT;
	default: return VK_FORMAT_UNDEFINED;
	}
}

VkColorSpaceKHR ralVk_TranslateColorSpace( ralColorSpace_t colorSpace ) {
	switch ( colorSpace ) {
	case RAL_COLORSPACE_EXTENDED_SRGB_LINEAR: return VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT;
	case RAL_COLORSPACE_HDR10_ST2084: return VK_COLOR_SPACE_HDR10_ST2084_EXT;
	default: return VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
	}
}

VkPresentModeKHR ralVk_TranslatePresentMode( ralPresentMode_t mode ) {
	switch ( mode ) {
	case RAL_PRESENT_MAILBOX: return VK_PRESENT_MODE_MAILBOX_KHR;
	case RAL_PRESENT_IMMEDIATE: return VK_PRESENT_MODE_IMMEDIATE_KHR;
	default: return VK_PRESENT_MODE_FIFO_KHR;
	}
}

ralTexture_t *Ral_AdoptTexture( ralBackend_t *b, void *image, void *view,
	                            ralFormat_t format, uint32_t width, uint32_t height,
	                            uint32_t aspect, const char *debugName ) {
	ralTexture_t *texture;
	(void)debugName;
	if ( g_failAdoptAt >= 0 && (int32_t)g_adoptCalls++ == g_failAdoptAt ) return NULL;
	texture = (ralTexture_t *)calloc( 1, sizeof( *texture ) );
	if ( !texture ) return NULL;
	texture->backend = b;
	texture->image = (VkImage)image;
	texture->defaultView = (VkImageView)view;
	texture->ralFormat = format;
	texture->width = width;
	texture->height = height;
	texture->aspect = aspect;
	texture->mipLevels = 1;
	texture->arrayLayers = 1;
	return texture;
}

void Ral_DestroyTexture( ralTexture_t *texture ) {
	if ( !texture ) return;
	g_destroyTextureCalls++;
	free( texture );
}

static VKAPI_ATTR VkResult VKAPI_CALL fakeGetCaps(
	VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, VkSurfaceCapabilitiesKHR *caps ) {
	(void)physicalDevice; (void)surface; *caps = g_caps; return VK_SUCCESS;
}

static VKAPI_ATTR VkResult VKAPI_CALL fakeGetFormats(
	VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, uint32_t *count, VkSurfaceFormatKHR *formats ) {
	(void)physicalDevice; (void)surface;
	if ( !formats ) { *count = 2; return VK_SUCCESS; }
	memcpy( formats, g_formats, sizeof( g_formats ) ); *count = 2; return VK_SUCCESS;
}

static VKAPI_ATTR VkResult VKAPI_CALL fakeGetModes(
	VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, uint32_t *count, VkPresentModeKHR *modes ) {
	(void)physicalDevice; (void)surface;
	if ( !modes ) { *count = 2; return VK_SUCCESS; }
	memcpy( modes, g_modes, sizeof( g_modes ) ); *count = 2; return VK_SUCCESS;
}

static VKAPI_ATTR VkResult VKAPI_CALL fakeCreateSwapchain(
	VkDevice device, const VkSwapchainCreateInfoKHR *ci, const VkAllocationCallbacks *allocator,
	VkSwapchainKHR *swapchain ) {
	(void)device; (void)allocator; g_lastCreate = *ci;
	if ( g_createResult != VK_SUCCESS ) { *swapchain = VK_NULL_HANDLE; return g_createResult; }
	*swapchain = HANDLE( VkSwapchainKHR, 0x100 + ++g_createSerial );
	return VK_SUCCESS;
}

static VKAPI_ATTR void VKAPI_CALL fakeDestroySwapchain(
	VkDevice device, VkSwapchainKHR swapchain, const VkAllocationCallbacks *allocator ) {
	(void)device; (void)allocator; g_destroySwapchainCalls++; g_lastDestroyedSwapchain = swapchain;
}

static VKAPI_ATTR VkResult VKAPI_CALL fakeGetImages(
	VkDevice device, VkSwapchainKHR swapchain, uint32_t *count, VkImage *images ) {
	(void)device; (void)swapchain;
	if ( !images ) { *count = g_imageCount; return VK_SUCCESS; }
	if ( g_imageFillResult != VK_SUCCESS ) return g_imageFillResult;
	memcpy( images, g_images, sizeof( *images ) * g_imageCount ); *count = g_imageCount; return VK_SUCCESS;
}

static VKAPI_ATTR VkResult VKAPI_CALL fakeCreateView(
	VkDevice device, const VkImageViewCreateInfo *ci, const VkAllocationCallbacks *allocator,
	VkImageView *view ) {
	static uint32_t viewCalls;
	(void)device; (void)allocator;
	if ( ci->image == g_images[0] ) viewCalls = 0;
	if ( g_failViewAt >= 0 && (int32_t)viewCalls++ == g_failViewAt ) return VK_ERROR_INITIALIZATION_FAILED;
	*view = HANDLE( VkImageView, 0x500 + (uintptr_t)ci->image ); return VK_SUCCESS;
}

static VKAPI_ATTR void VKAPI_CALL fakeDestroyView(
	VkDevice device, VkImageView view, const VkAllocationCallbacks *allocator ) {
	(void)device; (void)view; (void)allocator; g_destroyViewCalls++;
}

static VKAPI_ATTR VkResult VKAPI_CALL fakeDeviceWaitIdle( VkDevice device ) {
	(void)device; g_waitIdleCalls++; return g_waitIdleResult;
}

static VKAPI_ATTR VkResult VKAPI_CALL fakeAcquire(
	VkDevice device, VkSwapchainKHR swapchain, uint64_t timeout, VkSemaphore semaphore,
	VkFence fence, uint32_t *imageIndex ) {
	(void)device; (void)swapchain; (void)timeout; (void)semaphore; (void)fence;
	*imageIndex = g_acquireIndex; return g_acquireResult;
}

static VKAPI_ATTR VkResult VKAPI_CALL fakePresent( VkQueue queue, const VkPresentInfoKHR *info ) {
	(void)queue; CHECK( info->swapchainCount == 1 ); g_presentCalls++; return g_presentResult;
}

static void setupBackend( ralBackend_t *b ) {
	uint32_t i;
	memset( b, 0, sizeof( *b ) );
	b->physicalDevice = HANDLE( VkPhysicalDevice, 1 );
	b->device = HANDLE( VkDevice, 2 );
	b->surface = HANDLE( VkSurfaceKHR, 3 );
	b->queues[RAL_QUEUE_GRAPHICS] = HANDLE( VkQueue, 4 );
	b->vk.GetPhysicalDeviceSurfaceCapabilitiesKHR = fakeGetCaps;
	b->vk.GetPhysicalDeviceSurfaceFormatsKHR = fakeGetFormats;
	b->vk.GetPhysicalDeviceSurfacePresentModesKHR = fakeGetModes;
	b->vk.CreateSwapchainKHR = fakeCreateSwapchain;
	b->vk.DestroySwapchainKHR = fakeDestroySwapchain;
	b->vk.GetSwapchainImagesKHR = fakeGetImages;
	b->vk.CreateImageView = fakeCreateView;
	b->vk.DestroyImageView = fakeDestroyView;
	b->vk.DeviceWaitIdle = fakeDeviceWaitIdle;
	b->vk.AcquireNextImageKHR = fakeAcquire;
	b->vk.QueuePresentKHR = fakePresent;

	memset( &g_caps, 0, sizeof( g_caps ) );
	g_caps.minImageCount = 2;
	g_caps.maxImageCount = 4;
	g_caps.currentExtent.width = UINT32_MAX;
	g_caps.currentExtent.height = UINT32_MAX;
	g_caps.minImageExtent.width = 320;
	g_caps.minImageExtent.height = 200;
	g_caps.maxImageExtent.width = 1920;
	g_caps.maxImageExtent.height = 1080;
	g_caps.currentTransform = VK_SURFACE_TRANSFORM_ROTATE_90_BIT_KHR;
	g_caps.supportedCompositeAlpha = VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR | VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;
	g_caps.supportedUsageFlags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	g_formats[0].format = VK_FORMAT_R16G16B16A16_SFLOAT;
	g_formats[0].colorSpace = VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT;
	g_formats[1].format = VK_FORMAT_B8G8R8A8_UNORM;
	g_formats[1].colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
	g_modes[0] = VK_PRESENT_MODE_FIFO_KHR;
	g_modes[1] = VK_PRESENT_MODE_MAILBOX_KHR;
	for ( i = 0; i < 9; i++ ) g_images[i] = HANDLE( VkImage, 0x20 + i );
	g_createResult = VK_SUCCESS;
	g_acquireResult = VK_SUCCESS;
	g_acquireIndex = 8;
	g_imageCount = 9;
	g_failViewAt = -1;
	g_failAdoptAt = -1;
	g_adoptCalls = 0;
	g_imageFillResult = VK_SUCCESS;
	g_waitIdleResult = VK_SUCCESS;
	g_presentResult = VK_SUCCESS;
}

int main( void ) {
	ralBackend_t backend;
	ralSwapchain_t *swapchain = NULL;
	ralSurfaceFormat_t formatPreferences[] = {
		{ RAL_FORMAT_A2B10G10R10_UNORM, RAL_COLORSPACE_HDR10_ST2084 },
		{ RAL_FORMAT_B8G8R8A8_UNORM, RAL_COLORSPACE_SRGB_NONLINEAR }
	};
	ralPresentMode_t modePreferences[] = {
		RAL_PRESENT_IMMEDIATE, RAL_PRESENT_MAILBOX, RAL_PRESENT_FIFO
	};
	ralSwapchainCreateInfo_t ci;
	ralSwapchainInfo_t info;
	ralTexture_t *image = NULL;
	uint32_t imageIndex = 7;
	VkSwapchainKHR firstHandle;
	ralPresentInfo_t present;
	ralSemaphore_t acquireSemaphore;
	ralSwapchain_t *presentSwapchains[1];
	uint32_t presentIndices[1];
	uint32_t destroyedTextures, destroyedViews, destroyedSwapchains, createSerial;

	setupBackend( &backend );
	memset( &ci, 0, sizeof( ci ) );
	ci.desiredWidth = 100;
	ci.desiredHeight = 2000;
	ci.formatPreferences = formatPreferences;
	ci.formatPreferenceCount = 2;
	ci.presentModePreferences = modePreferences;
	ci.presentModePreferenceCount = 3;
	ci.desiredImageCount = 10;
	ci.requiredUsage = RAL_TEXTURE_USAGE_COLOR_ATTACHMENT | RAL_TEXTURE_USAGE_TRANSFER_DST;
	ci.backendExtensionChain = (const void *)(uintptr_t)0xCAFE;

	CHECK( Ral_CreateOrRecreateSwapchain( &backend, &ci, &swapchain ) == ralSuccess );
	CHECK( swapchain != NULL );
	CHECK( g_lastCreate.imageFormat == VK_FORMAT_B8G8R8A8_UNORM );
	CHECK( g_lastCreate.presentMode == VK_PRESENT_MODE_MAILBOX_KHR );
	CHECK( g_lastCreate.imageExtent.width == 320 && g_lastCreate.imageExtent.height == 1080 );
	CHECK( g_lastCreate.minImageCount == 4 );
	CHECK( g_lastCreate.imageUsage == ( VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT ) );
	CHECK( g_lastCreate.preTransform == VK_SURFACE_TRANSFORM_ROTATE_90_BIT_KHR );
	CHECK( g_lastCreate.compositeAlpha == VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR );
	CHECK( g_lastCreate.pNext == ci.backendExtensionChain );
	CHECK( Ral_GetSwapchainInfo( swapchain, &info ) );
	CHECK( info.generation == 1 );
	CHECK( info.imageCount == 9 && info.width == 320 && info.height == 1080 );
	CHECK( Ral_GetSwapchainImage( swapchain, 8 ) != NULL );

	memset( &acquireSemaphore, 0, sizeof( acquireSemaphore ) );
	acquireSemaphore.backend = &backend;
	acquireSemaphore.sem = HANDLE( VkSemaphore, 0x88 );
	CHECK( Ral_AcquireNextImage( swapchain, 1, NULL, &imageIndex, &image ) == ralErrorInvalidArgument );
	CHECK( imageIndex == UINT32_MAX && image == NULL );
	CHECK( Ral_AcquireNextImage( swapchain, 1, &acquireSemaphore, &imageIndex, &image ) == ralSuccess );
	CHECK( imageIndex == 8 && image == Ral_GetSwapchainImage( swapchain, 8 ) );
	CHECK( swapchain->imageStates[8] == RAL_VK_SWAPCHAIN_IMAGE_ACQUIRED );
	g_acquireResult = VK_TIMEOUT; imageIndex = 7; image = (ralTexture_t *)(uintptr_t)1;
	CHECK( Ral_AcquireNextImage( swapchain, 1, &acquireSemaphore, &imageIndex, &image ) == ralTimeout );
	CHECK( imageIndex == UINT32_MAX && image == NULL );
	g_acquireResult = VK_SUCCESS; g_acquireIndex = 99;
	CHECK( Ral_AcquireNextImage( swapchain, 1, &acquireSemaphore, &imageIndex, &image ) == ralErrorUnknown );
	CHECK( imageIndex == UINT32_MAX && image == NULL );

	presentSwapchains[0] = swapchain;
	presentIndices[0] = 9;
	memset( &present, 0, sizeof( present ) );
	present.swapchains = presentSwapchains;
	present.numSwapchains = 1;
	present.imageIndices = presentIndices;
	CHECK( Ral_Present( &backend, &present ) == ralErrorInvalidArgument );
	CHECK( g_presentCalls == 0 );
	presentIndices[0] = 8;
	CHECK( Ral_Present( &backend, &present ) == ralErrorInvalidArgument );
	CHECK( g_presentCalls == 0 );
	swapchain->imageStates[8] = RAL_VK_SWAPCHAIN_IMAGE_PREPARED;
	CHECK( Ral_Present( &backend, &present ) == ralSuccess );
	CHECK( g_presentCalls == 1 );
	CHECK( swapchain->imageStates[8] == RAL_VK_SWAPCHAIN_IMAGE_AVAILABLE );

	// Surface loss is not an ordinary out-of-date result: the caller must
	// rebuild the backend-owned surface. Both acquire and present preserve that
	// distinct result and leave public acquire outputs fail-closed.
	g_acquireResult = VK_ERROR_SURFACE_LOST_KHR;
	imageIndex = 7; image = (ralTexture_t *)(uintptr_t)1;
	CHECK( Ral_AcquireNextImage( swapchain, 1, &acquireSemaphore, &imageIndex, &image ) == ralSurfaceLost );
	CHECK( imageIndex == UINT32_MAX && image == NULL );
	swapchain->imageStates[8] = RAL_VK_SWAPCHAIN_IMAGE_PREPARED;
	g_presentResult = VK_ERROR_SURFACE_LOST_KHR;
	CHECK( Ral_Present( &backend, &present ) == ralSurfaceLost );
	CHECK( swapchain->imageStates[8] == RAL_VK_SWAPCHAIN_IMAGE_AVAILABLE );
	g_presentResult = VK_SUCCESS;

	// Recreate preflight is output-atomic: a minimized/zero desired extent or
	// wait-idle failure is rejected before native create is attempted, so the
	// old typed generation remains published and usable.
	{
		ralSwapchain_t *old = swapchain;
		uint32_t serial = g_createSerial;
		uint32_t waits = g_waitIdleCalls;
		ci.desiredWidth = 0;
		CHECK( Ral_CreateOrRecreateSwapchain( &backend, &ci, &swapchain ) == ralOutOfDate );
		CHECK( swapchain == old && g_createSerial == serial && g_waitIdleCalls == waits );
		ci.desiredWidth = 640;
		g_waitIdleResult = VK_ERROR_DEVICE_LOST;
		CHECK( Ral_CreateOrRecreateSwapchain( &backend, &ci, &swapchain ) == ralErrorDeviceLost );
		CHECK( swapchain == old && g_createSerial == serial && g_waitIdleCalls == waits + 1 );
		g_waitIdleResult = VK_SUCCESS;
	}

	firstHandle = swapchain->swapchain;
	g_createResult = VK_ERROR_DEVICE_LOST;
	g_acquireResult = VK_SUCCESS;
	CHECK( Ral_CreateOrRecreateSwapchain( &backend, &ci, &swapchain ) == ralErrorDeviceLost );
	CHECK( swapchain == NULL );
	CHECK( g_lastCreate.oldSwapchain == firstHandle );
	CHECK( g_waitIdleCalls == 2 );
	CHECK( g_lastDestroyedSwapchain == firstHandle );
	CHECK( g_destroyTextureCalls == 9 && g_destroyViewCalls == 9 );

	// VK_FORMAT_UNDEFINED is a wildcard for the caller's exact requested
	// format, empty mode preferences select required FIFO, and maxImageCount=0
	// means unlimited rather than zero.
	g_createResult = VK_SUCCESS;
	g_caps.maxImageCount = 0;
	g_formats[0].format = VK_FORMAT_UNDEFINED;
	g_formats[0].colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
	g_modes[0] = VK_PRESENT_MODE_FIFO_KHR;
	g_modes[1] = VK_PRESENT_MODE_FIFO_KHR;
	formatPreferences[0].format = RAL_FORMAT_A2B10G10R10_UNORM;
	formatPreferences[0].colorSpace = RAL_COLORSPACE_SRGB_NONLINEAR;
	ci.formatPreferenceCount = 1;
	ci.presentModePreferences = NULL;
	ci.presentModePreferenceCount = 0;
	ci.desiredImageCount = 6;
	CHECK( Ral_CreateOrRecreateSwapchain( &backend, &ci, &swapchain ) == ralSuccess );
	CHECK( Ral_GetSwapchainInfo( swapchain, &info ) && info.generation == 2 );
	CHECK( g_lastCreate.imageFormat == VK_FORMAT_A2B10G10R10_UNORM_PACK32 );
	CHECK( g_lastCreate.presentMode == VK_PRESENT_MODE_FIFO_KHR );
	CHECK( g_lastCreate.minImageCount == 6 );
	Ral_DestroySwapchain( swapchain ); swapchain = NULL;

	// Zero variable extent is a recoverable minimized-window state and never
	// attempts native creation. Unsupported usage is rejected the same way.
	createSerial = g_createSerial;
	ci.desiredWidth = 0;
	CHECK( Ral_CreateOrRecreateSwapchain( &backend, &ci, &swapchain ) == ralOutOfDate );
	CHECK( swapchain == NULL && g_createSerial == createSerial );
	ci.desiredWidth = 640;
	ci.requiredUsage |= RAL_TEXTURE_USAGE_TRANSFER_SRC;
	CHECK( Ral_CreateOrRecreateSwapchain( &backend, &ci, &swapchain ) == ralUnsupported );
	CHECK( swapchain == NULL && g_createSerial == createSerial );
	ci.requiredUsage &= ~RAL_TEXTURE_USAGE_TRANSFER_SRC;

	// Partial view construction tears down wrappers/views in reverse ownership
	// order and destroys the candidate native swapchain.
	g_imageCount = 3;
	g_failViewAt = 2;
	destroyedTextures = g_destroyTextureCalls;
	destroyedViews = g_destroyViewCalls;
	destroyedSwapchains = g_destroySwapchainCalls;
	CHECK( Ral_CreateOrRecreateSwapchain( &backend, &ci, &swapchain ) == ralErrorInitFailed );
	CHECK( swapchain == NULL );
	CHECK( g_destroyTextureCalls - destroyedTextures == 2 );
	CHECK( g_destroyViewCalls - destroyedViews == 2 );
	CHECK( g_destroySwapchainCalls - destroyedSwapchains == 1 );

	// Adoption failure owns the just-created view but no wrapper for that slot.
	g_failViewAt = -1;
	g_failAdoptAt = 1;
	g_adoptCalls = 0;
	destroyedTextures = g_destroyTextureCalls;
	destroyedViews = g_destroyViewCalls;
	destroyedSwapchains = g_destroySwapchainCalls;
	CHECK( Ral_CreateOrRecreateSwapchain( &backend, &ci, &swapchain ) == ralErrorOutOfMemory );
	CHECK( swapchain == NULL );
	CHECK( g_destroyTextureCalls - destroyedTextures == 1 );
	CHECK( g_destroyViewCalls - destroyedViews == 2 );
	CHECK( g_destroySwapchainCalls - destroyedSwapchains == 1 );
	g_failAdoptAt = -1;

	// A post-native materialization failure after recreation has the same
	// one-way contract as native-create failure: candidate + old die, output NULL.
	CHECK( Ral_CreateOrRecreateSwapchain( &backend, &ci, &swapchain ) == ralSuccess );
	firstHandle = swapchain->swapchain;
	g_imageFillResult = VK_ERROR_DEVICE_LOST;
	destroyedSwapchains = g_destroySwapchainCalls;
	CHECK( Ral_CreateOrRecreateSwapchain( &backend, &ci, &swapchain ) == ralErrorDeviceLost );
	CHECK( swapchain == NULL );
	CHECK( g_lastCreate.oldSwapchain == firstHandle );
	CHECK( g_destroySwapchainCalls - destroyedSwapchains == 2 );
	g_imageFillResult = VK_SUCCESS;

	puts( "ral swapchain policy contract: PASS" );
	return 0;
}
