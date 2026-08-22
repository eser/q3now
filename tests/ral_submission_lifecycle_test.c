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
static VkResult g_commandResetResult;
static uint32_t g_submitCalls;
static uint32_t g_beginCalls, g_endCalls, g_resetCalls;
static uint32_t g_barrierCalls;
static VkImageLayout g_barrierOldLayout;
static VkImageLayout g_barrierNewLayout;
static int g_callbackFailed;
static VkResult g_semaphoreCreateResult;
static uint32_t g_semaphoreCreateCalls;
static uint32_t g_semaphoreDestroyCalls;
static uint32_t g_deviceIdleCalls;
static VkResult g_fenceCreateResult;
static uint32_t g_fenceCreateCalls;
static uint32_t g_fenceDestroyCalls;
static VkResult g_fenceWaitResult;
static VkResult g_fenceResetResult;
static uint32_t g_fenceWaitCalls;
static uint32_t g_fenceResetCalls;
static uint64_t g_fenceWaitTimeout;
static VkBool32 g_fenceWaitAll;

static VKAPI_ATTR VkResult VKAPI_CALL fakeBeginCommandBuffer(
	VkCommandBuffer commandBuffer, const VkCommandBufferBeginInfo *beginInfo ) {
	(void)commandBuffer; (void)beginInfo; g_beginCalls++; return VK_SUCCESS;
}

static VKAPI_ATTR VkResult VKAPI_CALL fakeEndCommandBuffer( VkCommandBuffer commandBuffer ) {
	(void)commandBuffer; g_endCalls++; return VK_SUCCESS;
}

static VKAPI_ATTR VkResult VKAPI_CALL fakeResetCommandBuffer(
	VkCommandBuffer commandBuffer, VkCommandBufferResetFlags flags ) {
	(void)commandBuffer; (void)flags; g_resetCalls++; return g_commandResetResult;
}

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

static VKAPI_ATTR VkResult VKAPI_CALL fakeCreateSemaphore(
		VkDevice device, const VkSemaphoreCreateInfo *info,
		const VkAllocationCallbacks *allocator, VkSemaphore *outSemaphore ) {
	(void)device; (void)allocator;
	if ( !info || info->sType != VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO
			|| info->pNext != NULL || !outSemaphore ) return VK_ERROR_UNKNOWN;
	g_semaphoreCreateCalls++;
	if ( g_semaphoreCreateResult != VK_SUCCESS ) return g_semaphoreCreateResult;
	*outSemaphore = HANDLE( VkSemaphore, 0x900u + g_semaphoreCreateCalls );
	return VK_SUCCESS;
}

static VKAPI_ATTR void VKAPI_CALL fakeDestroySemaphore(
		VkDevice device, VkSemaphore semaphore,
		const VkAllocationCallbacks *allocator ) {
	(void)device; (void)allocator;
	if ( semaphore != VK_NULL_HANDLE ) g_semaphoreDestroyCalls++;
}

static VKAPI_ATTR VkResult VKAPI_CALL fakeCreateFence(
		VkDevice device, const VkFenceCreateInfo *info,
		const VkAllocationCallbacks *allocator, VkFence *outFence ) {
	(void)device; (void)allocator;
	if ( !info || info->sType != VK_STRUCTURE_TYPE_FENCE_CREATE_INFO
			|| info->pNext != NULL || info->flags != 0u || !outFence )
		return VK_ERROR_UNKNOWN;
	g_fenceCreateCalls++;
	if ( g_fenceCreateResult != VK_SUCCESS ) return g_fenceCreateResult;
	*outFence = HANDLE( VkFence, 0xA00u + g_fenceCreateCalls );
	return VK_SUCCESS;
}

static VKAPI_ATTR void VKAPI_CALL fakeDestroyFence(
		VkDevice device, VkFence fence,
		const VkAllocationCallbacks *allocator ) {
	(void)device; (void)allocator;
	if ( fence != VK_NULL_HANDLE ) g_fenceDestroyCalls++;
}

static VKAPI_ATTR VkResult VKAPI_CALL fakeWaitForFences(
		VkDevice device, uint32_t fenceCount, const VkFence *fences,
		VkBool32 waitAll, uint64_t timeout ) {
	(void)device;
	if ( fenceCount != 1u || !fences || fences[0] == VK_NULL_HANDLE )
		return VK_ERROR_UNKNOWN;
	g_fenceWaitCalls++;
	g_fenceWaitTimeout = timeout;
	g_fenceWaitAll = waitAll;
	return g_fenceWaitResult;
}

static VKAPI_ATTR VkResult VKAPI_CALL fakeResetFences(
		VkDevice device, uint32_t fenceCount, const VkFence *fences ) {
	(void)device;
	if ( fenceCount != 1u || !fences || fences[0] == VK_NULL_HANDLE )
		return VK_ERROR_UNKNOWN;
	g_fenceResetCalls++;
	return g_fenceResetResult;
}

static VKAPI_ATTR VkResult VKAPI_CALL fakeDeviceWaitIdle( VkDevice device ) {
	(void)device;
	g_deviceIdleCalls++;
	return VK_SUCCESS;
}

static void setupBackend( ralBackend_t *b ) {
	uint32_t q;
	memset( b, 0, sizeof( *b ) );
	b->device = HANDLE( VkDevice, 1 );
	b->queues[RAL_QUEUE_GRAPHICS] = HANDLE( VkQueue, 2 );
	b->vk.QueueSubmit2 = fakeQueueSubmit2;
	b->vk.BeginCommandBuffer = fakeBeginCommandBuffer;
	b->vk.EndCommandBuffer = fakeEndCommandBuffer;
	b->vk.ResetCommandBuffer = fakeResetCommandBuffer;
	b->vk.CmdPipelineBarrier = fakeCmdPipelineBarrier;
	b->vk.CreateSemaphore = fakeCreateSemaphore;
	b->vk.DestroySemaphore = fakeDestroySemaphore;
	b->vk.CreateFence = fakeCreateFence;
	b->vk.DestroyFence = fakeDestroyFence;
	b->vk.WaitForFences = fakeWaitForFences;
	b->vk.ResetFences = fakeResetFences;
	b->vk.DeviceWaitIdle = fakeDeviceWaitIdle;
	for ( q = RAL_QUEUE_GRAPHICS; q <= RAL_QUEUE_TRANSFER; ++q )
		Ral_SubmissionLifecycleInit( &b->submissionLifecycle[q], b, (ralQueueType_t)q );
}

int main( void ) {
	ralBackend_t backend, otherBackend;
	ralCommandBuffer_t cb;
	ralCommandBuffer_t *cbs[1];
	ralSubmitInfo_t submit;
	ralCommandReceipt_t recording, executable, staleCommand;
	ralSubmissionReceipt_t submitted, untouched, sentinel;
	ralSwapchain_t swapchain;
	ralTexture_t texture;
	ralTexture_t *images[1];
	uint8_t states[1];
	ralSemaphore_t *binarySemaphore;
	ralFence_t *ownedFence;
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
	g_commandResetResult = VK_SUCCESS;
	CHECK( ralVk_InitQueueMutexes( &backend ) );
	g_semaphoreCreateResult = VK_ERROR_OUT_OF_HOST_MEMORY;
	CHECK( Ral_CreateSemaphore( &backend, RAL_SEMAPHORE_BINARY ) == NULL );
	CHECK( g_semaphoreCreateCalls == 1u && g_semaphoreDestroyCalls == 0u );
	g_semaphoreCreateResult = VK_SUCCESS;
	binarySemaphore = Ral_CreateSemaphore( &backend, RAL_SEMAPHORE_BINARY );
	CHECK( binarySemaphore != NULL && binarySemaphore->ownsSemaphore == qtrue );
	CHECK( Ral_GetSemaphoreHandle( binarySemaphore )
		== (void *)HANDLE( VkSemaphore, 0x902u ) );
	Ral_DestroySemaphore( binarySemaphore );
	CHECK( g_deviceIdleCalls == 1u && g_semaphoreDestroyCalls == 1u );
	g_fenceCreateResult = VK_ERROR_OUT_OF_DEVICE_MEMORY;
	CHECK( Ral_CreateFence( &backend ) == NULL );
	CHECK( g_fenceCreateCalls == 1u && g_fenceDestroyCalls == 0u );
	g_fenceCreateResult = VK_SUCCESS;
	ownedFence = Ral_CreateFence( &backend );
	CHECK( ownedFence != NULL && ownedFence->ownsFence == qtrue );
	CHECK( Ral_GetFenceHandle( ownedFence ) == (void *)HANDLE( VkFence, 0xA02u ) );
	CHECK( Ral_WaitFenceExact( NULL, 0u ) == ralErrorInvalidArgument );
	g_fenceWaitResult = VK_SUCCESS;
	CHECK( Ral_WaitFenceExact( ownedFence, 0u ) == ralSuccess );
	CHECK( g_fenceWaitCalls == 1u && g_fenceWaitTimeout == 0u
		&& g_fenceWaitAll == VK_TRUE );
	g_fenceWaitResult = VK_TIMEOUT;
	CHECK( Ral_WaitFenceExact( ownedFence, 1234u ) == ralTimeout );
	CHECK( g_fenceWaitTimeout == 1234u );
	g_fenceWaitResult = VK_NOT_READY;
	CHECK( Ral_WaitFenceExact( ownedFence, RAL_TIMEOUT_INFINITE ) == ralTimeout );
	CHECK( g_fenceWaitTimeout == RAL_TIMEOUT_INFINITE );
	g_fenceWaitResult = VK_ERROR_DEVICE_LOST;
	CHECK( Ral_WaitFenceExact( ownedFence, 1u ) == ralErrorDeviceLost );
	g_fenceWaitResult = VK_ERROR_UNKNOWN;
	CHECK( Ral_WaitFenceExact( ownedFence, 1u ) == ralErrorUnknown );
	g_fenceResetResult = VK_SUCCESS;
	CHECK( Ral_ResetFenceExact( ownedFence ) == ralSuccess );
	g_fenceResetResult = VK_ERROR_DEVICE_LOST;
	CHECK( Ral_ResetFenceExact( ownedFence ) == ralErrorDeviceLost );
	g_fenceResetResult = VK_ERROR_UNKNOWN;
	CHECK( Ral_ResetFenceExact( ownedFence ) == ralErrorUnknown );
	ownedFence->preSignaled = qtrue;
	CHECK( Ral_WaitFenceExact( ownedFence, 0u ) == ralSuccess
		&& Ral_ResetFenceExact( ownedFence ) == ralSuccess );
	ownedFence->preSignaled = qfalse;
	backend.vk.WaitForFences = NULL;
	CHECK( Ral_WaitFenceExact( ownedFence, 0u ) == ralErrorInvalidArgument );
	backend.vk.WaitForFences = fakeWaitForFences;
	backend.vk.ResetFences = NULL;
	CHECK( Ral_ResetFenceExact( ownedFence ) == ralErrorInvalidArgument );
	backend.vk.ResetFences = fakeResetFences;
	backend.device = VK_NULL_HANDLE;
	CHECK( Ral_WaitFenceExact( ownedFence, 0u ) == ralErrorInvalidArgument
		&& Ral_ResetFenceExact( ownedFence ) == ralErrorInvalidArgument );
	backend.device = HANDLE( VkDevice, 1 );
	Ral_DestroyFence( ownedFence );
	CHECK( g_deviceIdleCalls == 2u && g_fenceDestroyCalls == 1u );

	memset( &cb, 0, sizeof( cb ) );
	cb.backend = &backend;
	cb.cb = HANDLE( VkCommandBuffer, 3 );
	cb.queue = RAL_QUEUE_GRAPHICS;
	cb.state = RAL_VK_CMD_PENDING_SUBMIT;
	cb.state = RAL_VK_CMD_IDLE;
	Ral_CommandLifecycleInit( &cb.lifecycle, &backend, &cb, RAL_QUEUE_GRAPHICS );
	CHECK( Ral_BeginCommandBufferExact( &cb, &recording ) == ralSuccess );
	CHECK( g_beginCalls == 1 && cb.state == RAL_VK_CMD_RECORDING );
	CHECK( Ral_EndCommandBufferExact( &cb, &recording, &executable ) == ralSuccess );
	CHECK( g_endCalls == 1 && cb.state == RAL_VK_CMD_PENDING_SUBMIT );
	CHECK( Ral_CancelCommandBuffer( &cb, &executable ) == ralSuccess );
	CHECK( g_resetCalls == 1 && cb.state == RAL_VK_CMD_IDLE );
	CHECK( Ral_BeginCommandBufferExact( &cb, &recording ) == ralSuccess );
	CHECK( recording.generation == 2u );
	CHECK( Ral_EndCommandBufferExact( &cb, &recording, &executable ) == ralSuccess );
	cbs[0] = &cb;
	memset( &submit, 0, sizeof( submit ) );
	submit.commandBuffers = cbs;
	submit.numCommandBuffers = 1;

	g_submitResult = VK_SUCCESS;
	CHECK( Ral_SubmitExact( &backend, RAL_QUEUE_GRAPHICS, &submit,
	                      &executable, &submitted ) == ralSuccess );
	CHECK( g_submitCalls == 1 && cb.state == RAL_VK_CMD_SUBMITTED );
	CHECK( Ral_SubmissionReceiptValid( &submitted )
	    && submitted.commands[0].generation == executable.generation );
	CHECK( Ral_SubmitExact( &backend, RAL_QUEUE_GRAPHICS, &submit,
	                      &executable, &submitted ) == ralErrorInvalidArgument );
	CHECK( g_submitCalls == 1 );
	staleCommand = submitted.commands[0]; staleCommand.generation++;
	CHECK( Ral_RecycleCommandBufferExact( &cb, &staleCommand ) == ralErrorInvalidArgument );
	CHECK( g_resetCalls == 1 && cb.state == RAL_VK_CMD_SUBMITTED );
	g_commandResetResult = VK_ERROR_DEVICE_LOST;
	CHECK( Ral_RecycleCommandBufferExact( &cb, &submitted.commands[0] ) == ralErrorDeviceLost );
	CHECK( g_resetCalls == 2 && cb.state == RAL_VK_CMD_SUBMITTED );
	g_commandResetResult = VK_SUCCESS;
	CHECK( Ral_RecycleCommandBufferExact( &cb, &submitted.commands[0] ) == ralSuccess );
	CHECK( g_resetCalls == 3 && cb.state == RAL_VK_CMD_IDLE );
	CHECK( Ral_BeginCommandBufferExact( &cb, &recording ) == ralSuccess );
	CHECK( recording.generation == 3u );
	CHECK( Ral_CancelCommandBuffer( &cb, &recording ) == ralSuccess );
	CHECK( g_resetCalls == 4 && cb.state == RAL_VK_CMD_IDLE );

	Ral_CommandLifecycleInit( &cb.lifecycle, &backend, &cb, RAL_QUEUE_GRAPHICS );
	CHECK( Ral_CommandLifecyclePublishBegin( &cb.lifecycle, &recording ) == ralSuccess );
	CHECK( Ral_CommandLifecyclePublishEnd( &cb.lifecycle, &recording, &executable ) == ralSuccess );
	cb.state = RAL_VK_CMD_PENDING_SUBMIT;
	g_submitResult = VK_ERROR_DEVICE_LOST;
	memset( &sentinel, 0x5a, sizeof( sentinel ) ); untouched = sentinel;
	CHECK( Ral_SubmitExact( &backend, RAL_QUEUE_GRAPHICS, &submit,
	                      &executable, &untouched ) == ralErrorDeviceLost );
	CHECK( g_submitCalls == 2 && cb.state == RAL_VK_CMD_PENDING_SUBMIT );
	CHECK( cb.lifecycle.state == RAL_COMMAND_EXECUTABLE );
	CHECK( memcmp( &untouched, &sentinel, sizeof( untouched ) ) == 0 );
	cb.backend = &otherBackend;
	CHECK( Ral_SubmitExact( &backend, RAL_QUEUE_GRAPHICS, &submit,
	                      &executable, &untouched ) == ralErrorInvalidArgument );
	CHECK( g_submitCalls == 2 );
	cb.backend = &backend;
	cb.state = RAL_VK_CMD_IDLE;
	CHECK( Ral_SubmitExact( &backend, RAL_QUEUE_GRAPHICS, &submit,
	                      &executable, &untouched ) == ralErrorInvalidArgument );
	CHECK( g_submitCalls == 2 );
	CHECK( Ral_Submit( &backend, RAL_QUEUE_GRAPHICS, &submit ) == ralErrorInvalidArgument );
	CHECK( g_submitCalls == 2 && cb.state == RAL_VK_CMD_IDLE );

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
	cb.lifecycle.state = RAL_COMMAND_RECORDING;

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
