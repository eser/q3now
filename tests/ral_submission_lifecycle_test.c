// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "ral_vulkan_internal.h"

#include <stdio.h>
#include <string.h>

#define CHECK(expr) do { if ( !(expr) ) { \
	fprintf( stderr, "CHECK failed at %s:%d: %s\n", __FILE__, __LINE__, #expr ); \
	return 1; \
} } while ( 0 )
#define HANDLE(type, value) ((type)(uintptr_t)(value))

static VkResult g_submitResult;
static uint32_t g_submitCalls;
static uint32_t g_barrierCalls;
static VkImageLayout g_barrierOldLayout;
static VkImageLayout g_barrierNewLayout;
static int g_callbackFailed;

static VKAPI_ATTR VkResult VKAPI_CALL fakeQueueSubmit2(
	VkQueue queue, uint32_t submitCount, const VkSubmitInfo2 *submits, VkFence fence ) {
	(void)queue; (void)fence;
	CHECK( submitCount == 1 );
	CHECK( submits != NULL );
	g_submitCalls++;
	return g_submitResult;
}

static VKAPI_ATTR void VKAPI_CALL fakeCmdPipelineBarrier(
	VkCommandBuffer commandBuffer,
	VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask,
	VkDependencyFlags dependencyFlags,
	uint32_t memoryBarrierCount, const VkMemoryBarrier *memoryBarriers,
	uint32_t bufferMemoryBarrierCount, const VkBufferMemoryBarrier *bufferMemoryBarriers,
	uint32_t imageMemoryBarrierCount, const VkImageMemoryBarrier *imageBarriers ) {
	(void)commandBuffer; (void)srcStageMask; (void)dstStageMask;
	(void)dependencyFlags; (void)memoryBarrierCount; (void)memoryBarriers;
	(void)bufferMemoryBarrierCount; (void)bufferMemoryBarriers;
	if ( imageMemoryBarrierCount != 1 || imageBarriers == NULL ) {
		g_callbackFailed = 1;
		return;
	}
	g_barrierCalls++;
	g_barrierOldLayout = imageBarriers[0].oldLayout;
	g_barrierNewLayout = imageBarriers[0].newLayout;
}

static void setupBackend( ralBackend_t *b ) {
	memset( b, 0, sizeof( *b ) );
	b->device = HANDLE( VkDevice, 1 );
	b->queues[RAL_QUEUE_GRAPHICS] = HANDLE( VkQueue, 2 );
	b->vk.QueueSubmit2 = fakeQueueSubmit2;
	b->vk.CmdPipelineBarrier = fakeCmdPipelineBarrier;
}

int main( void ) {
	ralBackend_t backend, otherBackend;
	ralCommandBuffer_t cb;
	ralCommandBuffer_t *cbs[1];
	ralSubmitInfo_t submit;
	ralSwapchain_t swapchain;
	ralTexture_t texture;
	ralTexture_t *images[1];
	uint8_t states[1];
	static const VkImageLayout layouts[] = {
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_IMAGE_LAYOUT_GENERAL,
		VK_IMAGE_LAYOUT_UNDEFINED
	};
	uint32_t i;

	setupBackend( &backend );
	setupBackend( &otherBackend );
	CHECK( ralVk_InitQueueMutexes( &backend ) );

	memset( &cb, 0, sizeof( cb ) );
	cb.backend = &backend;
	cb.cb = HANDLE( VkCommandBuffer, 3 );
	cb.queue = RAL_QUEUE_GRAPHICS;
	cb.state = RAL_VK_CMD_PENDING_SUBMIT;
	cbs[0] = &cb;
	memset( &submit, 0, sizeof( submit ) );
	submit.commandBuffers = cbs;
	submit.numCommandBuffers = 1;

	g_submitResult = VK_SUCCESS;
	CHECK( Ral_Submit( &backend, RAL_QUEUE_GRAPHICS, &submit ) == ralSuccess );
	CHECK( g_submitCalls == 1 && cb.state == RAL_VK_CMD_SUBMITTED );
	cb.state = RAL_VK_CMD_PENDING_SUBMIT;
	g_submitResult = VK_ERROR_DEVICE_LOST;
	CHECK( Ral_Submit( &backend, RAL_QUEUE_GRAPHICS, &submit ) == ralErrorDeviceLost );
	CHECK( g_submitCalls == 2 && cb.state == RAL_VK_CMD_PENDING_SUBMIT );
	cb.backend = &otherBackend;
	CHECK( Ral_Submit( &backend, RAL_QUEUE_GRAPHICS, &submit ) == ralErrorInvalidArgument );
	CHECK( g_submitCalls == 2 );
	cb.backend = &backend;
	cb.state = RAL_VK_CMD_IDLE;
	CHECK( Ral_Submit( &backend, RAL_QUEUE_GRAPHICS, &submit ) == ralErrorInvalidArgument );
	CHECK( g_submitCalls == 2 );
	Ral_SetCommandBufferExternalLifecycle( &cb, qtrue );
	CHECK( Ral_Submit( &backend, RAL_QUEUE_GRAPHICS, &submit ) == ralErrorDeviceLost );
	CHECK( g_submitCalls == 3 && cb.state == RAL_VK_CMD_IDLE );
	Ral_SetCommandBufferExternalLifecycle( &cb, qfalse );

	memset( &swapchain, 0, sizeof( swapchain ) );
	memset( &texture, 0, sizeof( texture ) );
	swapchain.backend = &backend;
	swapchain.imageCount = 1;
	swapchain.adoptedImages = images;
	swapchain.imageStates = states;
	images[0] = &texture;
	texture.backend = &backend;
	texture.image = HANDLE( VkImage, 4 );
	texture.aspect = VK_IMAGE_ASPECT_COLOR_BIT;
	texture.mipLevels = 1;
	texture.arrayLayers = 1;
	cb.state = RAL_VK_CMD_RECORDING;

	for ( i = 0; i < sizeof( layouts ) / sizeof( layouts[0] ); i++ ) {
		states[0] = RAL_VK_SWAPCHAIN_IMAGE_ACQUIRED;
		texture.currentLayout = layouts[i];
		g_barrierCalls = 0;
		CHECK( Ral_PrepareSwapchainImageForPresent( &cb, &swapchain, 0 ) == ralSuccess );
		CHECK( g_barrierCalls == 1 );
		CHECK( !g_callbackFailed );
		CHECK( g_barrierOldLayout == layouts[i] );
		CHECK( g_barrierNewLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR );
		CHECK( texture.currentLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR );
		CHECK( states[0] == RAL_VK_SWAPCHAIN_IMAGE_PREPARED );
	}

	states[0] = RAL_VK_SWAPCHAIN_IMAGE_ACQUIRED;
	texture.currentLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	g_barrierCalls = 0;
	CHECK( Ral_PrepareSwapchainImageForPresent( &cb, &swapchain, 0 ) == ralSuccess );
	CHECK( g_barrierCalls == 0 && states[0] == RAL_VK_SWAPCHAIN_IMAGE_PREPARED );

	states[0] = RAL_VK_SWAPCHAIN_IMAGE_AVAILABLE;
	texture.currentLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	CHECK( Ral_PrepareSwapchainImageForPresent( &cb, &swapchain, 0 ) == ralErrorInvalidArgument );
	CHECK( states[0] == RAL_VK_SWAPCHAIN_IMAGE_AVAILABLE );
	states[0] = RAL_VK_SWAPCHAIN_IMAGE_ACQUIRED;
	texture.currentLayout = (VkImageLayout)0x7fffffff;
	CHECK( Ral_PrepareSwapchainImageForPresent( &cb, &swapchain, 0 ) == ralErrorInvalidArgument );
	CHECK( states[0] == RAL_VK_SWAPCHAIN_IMAGE_ACQUIRED );
	cb.state = RAL_VK_CMD_IDLE;
	texture.currentLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	CHECK( Ral_PrepareSwapchainImageForPresent( &cb, &swapchain, 0 ) == ralErrorInvalidArgument );

	ralVk_DestroyQueueMutexes( &backend );
	puts( "ral submission lifecycle contract: PASS" );
	return 0;
}
