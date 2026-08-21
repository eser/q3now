// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "ral_metal_internal.h"
#include "ral_metal_render.h"

#import <Foundation/Foundation.h>
#import <Metal/Metal.h>

#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr,"FAIL %s:%d: %s\n",__FILE__,__LINE__,#x); return 1; } } while (0)

static qboolean NearByte( uint8_t value, uint8_t expected ) {
	return value + 1u >= expected && value <= expected + 1u ? qtrue : qfalse;
}

int main( void ) {
	ralMetalCoreCreateInfo_t coreInfo = { 9u };
	ralMetalCore_t *core = NULL;
	ralMetalCoreReceipt_t coreReceipt, badCore;
	ralMetalRenderAttachment_t color, depth, colors[2];
	ralMetalRenderPlan_t plan;
	ralMetalRenderReceipt_t receipt, exact, before;
	ralMemoryFailureEvent_t lossEvent;
	ralMemoryFailureReceipt_t lossReceipt;
	id<MTLDevice> device;
	id<MTLTexture> colorTexture, depthTexture;
	MTLTextureDescriptor *textureInfo;
	uint8_t pixels[4u * 4u * 4u];
	CHECK( RalMetal_CoreCreate( &coreInfo, &core, &coreReceipt ) );
	device = RalMetal_CoreNativeDevice( core ); CHECK( device != nil );
	textureInfo = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
		width:4u height:4u mipmapped:NO];
	textureInfo.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
	textureInfo.storageMode = MTLStorageModeShared;
	colorTexture = [device newTextureWithDescriptor:textureInfo];
	textureInfo = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatDepth32Float
		width:4u height:4u mipmapped:NO];
	textureInfo.usage = MTLTextureUsageRenderTarget;
	textureInfo.storageMode = MTLStorageModePrivate;
	depthTexture = [device newTextureWithDescriptor:textureInfo];
	CHECK( colorTexture && depthTexture );
	memset( &color, 0, sizeof( color ) );
	color.nativeTexture = (void *)colorTexture;
	color.textureIdentity = (uintptr_t)(void *)colorTexture;
	color.textureGeneration = 3u; color.format = RAL_FORMAT_R8G8B8A8_UNORM;
	color.width = 4u; color.height = 4u; color.sampleCount = 1u;
	color.loadOp = RAL_LOAD_OP_CLEAR; color.storeOp = RAL_STORE_OP_STORE;
	color.clearValue.color[0] = 0.25f; color.clearValue.color[1] = 0.5f;
	color.clearValue.color[2] = 0.75f; color.clearValue.color[3] = 1.0f;
	memset( &depth, 0, sizeof( depth ) );
	depth.nativeTexture = (void *)depthTexture;
	depth.textureIdentity = (uintptr_t)(void *)depthTexture;
	depth.textureGeneration = 4u; depth.format = RAL_FORMAT_D32_SFLOAT;
	depth.width = 4u; depth.height = 4u; depth.sampleCount = 1u;
	depth.loadOp = RAL_LOAD_OP_CLEAR; depth.storeOp = RAL_STORE_OP_DONT_CARE;
	depth.clearValue.depthStencil.depth = 0.625f;
	memset( &plan, 0, sizeof( plan ) );
	plan.colorAttachments = &color; plan.colorAttachmentCount = 1u;
	plan.depthAttachment = &depth; plan.width = 4u; plan.height = 4u;
	memset( &receipt, 0x5a, sizeof( receipt ) ); before = receipt;
	color.width = 3u;
	CHECK( !RalMetal_RenderClearPass( core, &coreReceipt, &plan, 13u, &receipt ) );
	CHECK( memcmp( &receipt, &before, sizeof( receipt ) ) == 0 );
	color.width = 4u; color.textureGeneration = UINT64_MAX;
	CHECK( !RalMetal_RenderClearPass( core, &coreReceipt, &plan, 13u, &receipt ) );
	color.textureGeneration = 3u; color.format = RAL_FORMAT_D32_SFLOAT;
	CHECK( !RalMetal_RenderClearPass( core, &coreReceipt, &plan, 13u, &receipt ) );
	color.format = RAL_FORMAT_R8G8B8A8_UNORM;
	color.loadOp = (ralLoadOp_t)99;
	CHECK( !RalMetal_RenderClearPass( core, &coreReceipt, &plan, 13u, &receipt ) );
	color.loadOp = RAL_LOAD_OP_CLEAR; color.clearValue.color[0] = NAN;
	CHECK( !RalMetal_RenderClearPass( core, &coreReceipt, &plan, 13u, &receipt ) );
	color.clearValue.color[0] = 0.25f;
	colors[0] = color; colors[1] = color; plan.colorAttachments = colors;
	plan.colorAttachmentCount = 2u;
	CHECK( !RalMetal_RenderClearPass( core, &coreReceipt, &plan, 13u, &receipt ) );
	plan.colorAttachments = &color; plan.colorAttachmentCount = 1u;
	badCore = coreReceipt; badCore.generation++;
	CHECK( !RalMetal_RenderClearPass( core, &badCore, &plan, 13u, &receipt ) );
	CHECK( RalMetal_RenderClearPass( core, &coreReceipt, &plan, 13u, &receipt ) );
	CHECK( receipt.recording.state == RAL_COMMAND_RECORDING
		&& receipt.executable.state == RAL_COMMAND_EXECUTABLE
		&& receipt.submission.commands[0].state == RAL_COMMAND_SUBMITTED );
	memset( pixels, 0, sizeof( pixels ) );
	[colorTexture getBytes:pixels bytesPerRow:16u
		fromRegion:MTLRegionMake2D( 0u, 0u, 4u, 4u ) mipmapLevel:0u];
	CHECK( NearByte( pixels[0], 64u ) && NearByte( pixels[1], 128u )
		&& NearByte( pixels[2], 191u ) && pixels[3] == 255u );
	exact = receipt;
	CHECK( RalMetal_RenderReceiptExact( &receipt, &exact ) );
#define MUTATE(field) do { exact = receipt; exact.field++; \
	CHECK( !RalMetal_RenderReceiptExact( &receipt, &exact ) ); } while (0)
	MUTATE( coreGeneration ); MUTATE( renderGeneration ); MUTATE( completionGeneration );
	MUTATE( colorAttachments[0].textureGeneration ); MUTATE( colorAttachments[0].width );
	MUTATE( recording.generation ); MUTATE( executable.generation );
	MUTATE( submission.generation );
#undef MUTATE
	exact = receipt; exact.colorAttachments[0].format = RAL_FORMAT_D32_SFLOAT;
	CHECK( !RalMetal_RenderReceiptExact( &exact, &exact ) );
	exact = receipt; exact.submission.commands[0].commandIdentity
		= (const ralCommandBuffer_t *)(uintptr_t)0x1234u;
	CHECK( !RalMetal_RenderReceiptExact( &exact, &exact ) );
	exact = receipt; exact.depthAttachment.textureIdentity
		= exact.colorAttachments[0].textureIdentity;
	CHECK( !RalMetal_RenderReceiptExact( &exact, &exact ) );

	memset( &lossEvent, 0, sizeof( lossEvent ) );
	lossEvent.backendType = RAL_BACKEND_METAL;
	lossEvent.cause = RAL_MEMORY_FAILURE_DEVICE_LOST;
	lossEvent.memoryClass = RAL_ALLOCATION_DEVICE_LOCAL;
	lossEvent.residency = RAL_ALLOCATION_RESIDENCY_PERMANENT;
	lossEvent.criticality = RAL_MEMORY_CRITICALITY_REQUIRED;
	lossEvent.requestedBytes = 64u; lossEvent.attempt = 1u;
	lossEvent.maxAttempts = 1u; lossEvent.liveParent = qtrue;
	CHECK( RalMetal_CorePublishDeviceLoss( core, &lossEvent, &lossReceipt ) );
	before = receipt;
	CHECK( !RalMetal_RenderClearPass( core, &coreReceipt, &plan, 14u, &receipt ) );
	CHECK( memcmp( &receipt, &before, sizeof( receipt ) ) == 0 );

	[depthTexture release]; [colorTexture release]; RalMetal_CoreDestroy( core );
	puts( "ral metal render: PASS" );
	return 0;
}
