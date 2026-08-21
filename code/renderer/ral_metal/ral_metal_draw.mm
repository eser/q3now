// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "ral_metal_draw.h"
#include "ral_metal_internal.h"

#import <Foundation/Foundation.h>
#import <Metal/Metal.h>

#include <limits.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
	float position[2];
	uint8_t color[4];
	float uv[2];
} overlayVertex_t;

static const overlayVertex_t kVertices[4] = {
	{ { -1.0f, -1.0f }, { 255u, 255u, 255u, 255u }, { 0.0f, 1.0f } },
	{ { -1.0f,  1.0f }, { 255u, 255u, 255u, 255u }, { 0.0f, 0.0f } },
	{ {  1.0f,  1.0f }, { 255u, 255u, 255u, 255u }, { 1.0f, 0.0f } },
	{ {  1.0f, -1.0f }, { 255u, 255u, 255u, 255u }, { 1.0f, 1.0f } }
};
static const uint16_t kIndices[6] = { 0u, 1u, 2u, 0u, 2u, 3u };
static const uint8_t kExpectedRgba[4] = { 68u, 136u, 204u, 255u };

static uint64_t DigestBytes( const uint8_t *bytes, uint64_t count ) {
	uint64_t digest = UINT64_C(1469598103934665603), i;
	for ( i = 0u; i < count; ++i ) {
		digest ^= bytes[i];
		digest *= UINT64_C(1099511628211);
	}
	return digest;
}

static qboolean BufferValid( const ralMetalDrawBuffer_t *buffer,
		uint64_t requiredBytes ) {
	id resource;
	if ( !buffer || !buffer->nativeBuffer
			|| buffer->bufferIdentity != (uintptr_t)buffer->nativeBuffer
			|| buffer->bufferGeneration == 0u || buffer->bufferGeneration == UINT64_MAX
			|| buffer->byteSize == 0u || requiredBytes > buffer->byteSize ) return qfalse;
	resource = (id)buffer->nativeBuffer;
	if ( ![resource conformsToProtocol:@protocol(MTLBuffer)] ) return qfalse;
	id<MTLBuffer> nativeBuffer = (id<MTLBuffer>)resource;
	return ( buffer->byteOffset <= (uint64_t)nativeBuffer.length
		&& buffer->byteSize <= (uint64_t)nativeBuffer.length - buffer->byteOffset
		&& nativeBuffer.storageMode == MTLStorageModeShared ) ? qtrue : qfalse;
}

static qboolean CanonicalGroupValid( ralMetalBindGroup_t *group,
		const ralMetalBindGroupReceipt_t *receipt ) {
	id texture, sampler;
	if ( !RalMetal_BindGroupMatchesReceipt( group, receipt )
			|| receipt->resourceCount != 2u
			|| receipt->resources[0].binding != 0u
			|| receipt->resources[0].arrayElement != 0u
			|| receipt->resources[0].type != RAL_BIND_SAMPLED_TEXTURE
			|| receipt->resources[1].binding != 32u
			|| receipt->resources[1].arrayElement != 0u
			|| receipt->resources[1].type != RAL_BIND_SAMPLER ) return qfalse;
	texture = RalMetal_BindGroupNativeResource( group, 0u );
	sampler = RalMetal_BindGroupNativeResource( group, 1u );
	if ( ![texture conformsToProtocol:@protocol(MTLTexture)]
			|| ![sampler conformsToProtocol:@protocol(MTLSamplerState)] ) return qfalse;
	id<MTLTexture> nativeTexture = (id<MTLTexture>)texture;
	if ( nativeTexture.textureType != MTLTextureType2D
			|| nativeTexture.pixelFormat != MTLPixelFormatRGBA8Unorm
			|| nativeTexture.width != 2u || nativeTexture.height != 2u
			|| nativeTexture.storageMode != MTLStorageModeShared
			|| ( nativeTexture.usage & MTLTextureUsageShaderRead ) == 0u ) return qfalse;
	uint8_t texels[16];
	[nativeTexture getBytes:texels bytesPerRow:8u
		fromRegion:MTLRegionMake2D( 0u, 0u, 2u, 2u ) mipmapLevel:0u];
	for ( uint32_t i = 0u; i < 4u; ++i )
		if ( memcmp( texels + i * 4u, kExpectedRgba, 4u ) != 0 ) return qfalse;
	return qtrue;
}

static qboolean DrawInfoValid( ralMetalCore_t *core,
		const ralMetalCoreReceipt_t *coreReceipt,
		const ralMetalIndexedDrawInfo_t *info ) {
	const ralMetalRenderAttachment_t *color, *depth;
	uint64_t vertexBytes = sizeof( kVertices ), indexBytes = sizeof( kIndices );
	if ( !core || !coreReceipt || !info || !info->pipelineReceipt
			|| !info->fragmentGroupReceipt || !info->renderPlan
			|| info->drawGeneration == 0u || info->drawGeneration == UINT64_MAX
			|| info->vertexCount != 4u || info->indexCount != 6u
			|| info->firstIndex != 0u || info->instanceCount != 1u
			|| info->firstInstance != 0u
			|| !RalMetal_CoreMatchesReceipt( core, coreReceipt )
			|| !RalMetal_PipelineMatchesReceipt( info->pipeline, info->pipelineReceipt )
			|| !CanonicalGroupValid( info->fragmentGroup, info->fragmentGroupReceipt )
			|| info->pipelineReceipt->coreGeneration != coreReceipt->generation
			|| info->fragmentGroupReceipt->coreGeneration != coreReceipt->generation
			|| info->pipelineReceipt->fragmentLayoutIdentity
				!= info->fragmentGroupReceipt->layoutIdentity
			|| info->pipelineReceipt->fragmentLayoutGeneration
				!= info->fragmentGroupReceipt->layoutGeneration
			|| !BufferValid( &info->vertexBuffer, vertexBytes )
			|| !BufferValid( &info->indexBuffer, indexBytes )
			|| info->vertexBuffer.bufferIdentity == info->indexBuffer.bufferIdentity
			|| info->vertexBuffer.bufferIdentity
				== info->fragmentGroupReceipt->argumentBufferIdentity
			|| info->indexBuffer.bufferIdentity
				== info->fragmentGroupReceipt->argumentBufferIdentity
			|| !RalMetal_RenderPlanValid( info->renderPlan )
			|| info->renderPlan->colorAttachmentCount != 1u
			|| !info->renderPlan->depthAttachment ) return qfalse;
	color = &info->renderPlan->colorAttachments[0];
	depth = info->renderPlan->depthAttachment;
	if ( color->format != info->pipelineReceipt->colorFormat
			|| depth->format != info->pipelineReceipt->depthFormat
			|| color->sampleCount != info->pipelineReceipt->sampleCount
			|| depth->sampleCount != info->pipelineReceipt->sampleCount
			|| color->storeOp != RAL_STORE_OP_STORE
			|| color->loadOp != RAL_LOAD_OP_CLEAR
			|| depth->loadOp != RAL_LOAD_OP_CLEAR
			|| color->width > 64u || color->height > 64u
			|| ( (id<MTLTexture>)color->nativeTexture ).storageMode != MTLStorageModeShared
			|| color->textureIdentity == info->fragmentGroupReceipt->resources[0].resourceIdentity
			|| depth->textureIdentity == info->fragmentGroupReceipt->resources[0].resourceIdentity )
		return qfalse;
	if ( memcmp( (const uint8_t *)[(id<MTLBuffer>)info->vertexBuffer.nativeBuffer contents]
			+ info->vertexBuffer.byteOffset, kVertices, sizeof( kVertices ) ) != 0
			|| memcmp( (const uint8_t *)[(id<MTLBuffer>)info->indexBuffer.nativeBuffer contents]
			+ info->indexBuffer.byteOffset, kIndices, sizeof( kIndices ) ) != 0 ) return qfalse;
	return qtrue;
}

static qboolean DrawReceiptValid( const ralMetalIndexedDrawReceipt_t *r ) {
	return ( r && r->schemaVersion == RAL_METAL_DRAW_SCHEMA_VERSION
		&& r->backendType == RAL_BACKEND_METAL
		&& r->coreGeneration != 0u && r->coreGeneration != UINT64_MAX
		&& r->drawGeneration != 0u && r->drawGeneration != UINT64_MAX
		&& r->pipelineIdentity != (uintptr_t)0 && r->pipelineGeneration != 0u
		&& r->pipelineGeneration != UINT64_MAX
		&& r->layoutIdentity != (uintptr_t)0 && r->layoutGeneration != 0u
		&& r->layoutGeneration != UINT64_MAX
		&& r->groupIdentity != (uintptr_t)0 && r->groupGeneration != 0u
		&& r->groupGeneration != UINT64_MAX && r->argumentBufferIdentity != (uintptr_t)0
		&& r->sampledTextureIdentity != (uintptr_t)0
		&& r->sampledTextureGeneration != 0u && r->sampledTextureGeneration != UINT64_MAX
		&& r->samplerIdentity != (uintptr_t)0
		&& r->samplerGeneration != 0u && r->samplerGeneration != UINT64_MAX
		&& r->vertexBufferIdentity != (uintptr_t)0 && r->vertexBufferGeneration != 0u
		&& r->vertexBufferGeneration != UINT64_MAX && r->vertexBufferBytes >= sizeof( kVertices )
		&& r->indexBufferIdentity != (uintptr_t)0 && r->indexBufferGeneration != 0u
		&& r->indexBufferGeneration != UINT64_MAX && r->indexBufferBytes >= sizeof( kIndices )
		&& r->vertexBufferIdentity != r->indexBufferIdentity
		&& r->vertexBufferIdentity != r->argumentBufferIdentity
		&& r->indexBufferIdentity != r->argumentBufferIdentity
		&& r->colorTextureIdentity != (uintptr_t)0 && r->colorTextureGeneration != 0u
		&& r->colorTextureGeneration != UINT64_MAX
		&& r->depthTextureIdentity != (uintptr_t)0 && r->depthTextureGeneration != 0u
		&& r->depthTextureGeneration != UINT64_MAX
		&& r->sampledTextureIdentity != r->colorTextureIdentity
		&& r->sampledTextureIdentity != r->depthTextureIdentity
		&& r->colorTextureIdentity != r->depthTextureIdentity
		&& r->pipelineIdentity != r->layoutIdentity && r->pipelineIdentity != r->groupIdentity
		&& r->layoutIdentity != r->groupIdentity
		&& r->width != 0u && r->height != 0u && r->width <= 64u && r->height <= 64u
		&& r->vertexCount == 4u && r->indexCount == 6u && r->firstIndex == 0u
		&& r->instanceCount == 1u && r->firstInstance == 0u
		&& Ral_CommandReceiptValid( &r->recording )
		&& Ral_CommandReceiptValid( &r->executable )
		&& Ral_SubmissionReceiptValid( &r->submission )
		&& r->recording.state == RAL_COMMAND_RECORDING
		&& r->executable.state == RAL_COMMAND_EXECUTABLE
		&& r->recording.generation == r->drawGeneration
		&& r->executable.generation == r->drawGeneration
		&& r->recording.backendIdentity == r->executable.backendIdentity
		&& r->recording.commandIdentity == r->executable.commandIdentity
		&& r->submission.commandCount == 1u
		&& r->submission.backendIdentity == r->executable.backendIdentity
		&& r->submission.queue == r->executable.queue
		&& r->submission.commands[0].commandIdentity == r->executable.commandIdentity
		&& r->submission.commands[0].generation == r->drawGeneration
		&& r->submission.commands[0].state == RAL_COMMAND_SUBMITTED
		&& r->completionGeneration == r->submission.generation
		&& r->pixelCount == (uint64_t)r->width * r->height && r->pixelDigest != 0u
		&& !memcmp( r->expectedRgba, kExpectedRgba, sizeof( kExpectedRgba ) )
		&& r->ready == qtrue ) ? qtrue : qfalse;
}

qboolean RalMetal_IndexedDrawReceiptExact( const ralMetalIndexedDrawReceipt_t *a,
		const ralMetalIndexedDrawReceipt_t *b ) {
	return ( DrawReceiptValid( a ) && DrawReceiptValid( b )
		&& a->backendType == b->backendType && a->coreGeneration == b->coreGeneration
		&& a->drawGeneration == b->drawGeneration
		&& a->pipelineIdentity == b->pipelineIdentity
		&& a->pipelineGeneration == b->pipelineGeneration
		&& a->layoutIdentity == b->layoutIdentity && a->layoutGeneration == b->layoutGeneration
		&& a->groupIdentity == b->groupIdentity && a->groupGeneration == b->groupGeneration
		&& a->argumentBufferIdentity == b->argumentBufferIdentity
		&& a->sampledTextureIdentity == b->sampledTextureIdentity
		&& a->sampledTextureGeneration == b->sampledTextureGeneration
		&& a->samplerIdentity == b->samplerIdentity
		&& a->samplerGeneration == b->samplerGeneration
		&& a->vertexBufferIdentity == b->vertexBufferIdentity
		&& a->vertexBufferGeneration == b->vertexBufferGeneration
		&& a->vertexBufferBytes == b->vertexBufferBytes
		&& a->indexBufferIdentity == b->indexBufferIdentity
		&& a->indexBufferGeneration == b->indexBufferGeneration
		&& a->indexBufferBytes == b->indexBufferBytes
		&& a->colorTextureIdentity == b->colorTextureIdentity
		&& a->colorTextureGeneration == b->colorTextureGeneration
		&& a->depthTextureIdentity == b->depthTextureIdentity
		&& a->depthTextureGeneration == b->depthTextureGeneration
		&& a->width == b->width && a->height == b->height
		&& a->vertexCount == b->vertexCount && a->indexCount == b->indexCount
		&& a->firstIndex == b->firstIndex && a->instanceCount == b->instanceCount
		&& a->firstInstance == b->firstInstance
		&& Ral_CommandReceiptExact( &a->recording, &b->recording )
		&& Ral_CommandReceiptExact( &a->executable, &b->executable )
		&& Ral_SubmissionReceiptExact( &a->submission, &b->submission )
		&& a->completionGeneration == b->completionGeneration
		&& a->pixelCount == b->pixelCount && a->pixelDigest == b->pixelDigest
		&& !memcmp( a->expectedRgba, b->expectedRgba, sizeof( a->expectedRgba ) )
		&& a->ready == b->ready ) ? qtrue : qfalse;
}

qboolean RalMetal_DrawIndexedOverlay( ralMetalCore_t *core,
		const ralMetalCoreReceipt_t *coreReceipt,
		const ralMetalIndexedDrawInfo_t *info,
		ralMetalIndexedDrawReceipt_t *outReceipt ) {
	ralMetalIndexedDrawReceipt_t receipt;
	ralCommandLifecycle_t lifecycle;
	ralCommandReceipt_t recording, executable;
	id<MTLCommandQueue> queue;
	uint8_t *pixels = NULL;
	uint64_t pixelCount, pixelBytes, i;
	if ( !outReceipt || !DrawInfoValid( core, coreReceipt, info ) ) return qfalse;
	pixelCount = (uint64_t)info->renderPlan->width * info->renderPlan->height;
	pixelBytes = pixelCount * 4u;
	if ( pixelBytes == 0u || pixelBytes > SIZE_MAX ) return qfalse;
	pixels = (uint8_t *)malloc( (size_t)pixelBytes );
	if ( !pixels ) return qfalse;
	queue = RalMetal_CoreNativeQueue( core );
	memset( &receipt, 0, sizeof( receipt ) );
	receipt.schemaVersion = RAL_METAL_DRAW_SCHEMA_VERSION;
	receipt.backendType = RAL_BACKEND_METAL;
	receipt.coreGeneration = coreReceipt->generation;
	receipt.drawGeneration = info->drawGeneration;
	receipt.pipelineIdentity = info->pipelineReceipt->pipelineIdentity;
	receipt.pipelineGeneration = info->pipelineReceipt->pipelineGeneration;
	receipt.layoutIdentity = info->fragmentGroupReceipt->layoutIdentity;
	receipt.layoutGeneration = info->fragmentGroupReceipt->layoutGeneration;
	receipt.groupIdentity = info->fragmentGroupReceipt->groupIdentity;
	receipt.groupGeneration = info->fragmentGroupReceipt->groupGeneration;
	receipt.argumentBufferIdentity = info->fragmentGroupReceipt->argumentBufferIdentity;
	receipt.sampledTextureIdentity = info->fragmentGroupReceipt->resources[0].resourceIdentity;
	receipt.sampledTextureGeneration = info->fragmentGroupReceipt->resources[0].resourceGeneration;
	receipt.samplerIdentity = info->fragmentGroupReceipt->resources[1].resourceIdentity;
	receipt.samplerGeneration = info->fragmentGroupReceipt->resources[1].resourceGeneration;
	receipt.vertexBufferIdentity = info->vertexBuffer.bufferIdentity;
	receipt.vertexBufferGeneration = info->vertexBuffer.bufferGeneration;
	receipt.vertexBufferBytes = info->vertexBuffer.byteSize;
	receipt.indexBufferIdentity = info->indexBuffer.bufferIdentity;
	receipt.indexBufferGeneration = info->indexBuffer.bufferGeneration;
	receipt.indexBufferBytes = info->indexBuffer.byteSize;
	receipt.colorTextureIdentity = info->renderPlan->colorAttachments[0].textureIdentity;
	receipt.colorTextureGeneration = info->renderPlan->colorAttachments[0].textureGeneration;
	receipt.depthTextureIdentity = info->renderPlan->depthAttachment->textureIdentity;
	receipt.depthTextureGeneration = info->renderPlan->depthAttachment->textureGeneration;
	receipt.width = info->renderPlan->width; receipt.height = info->renderPlan->height;
	receipt.vertexCount = info->vertexCount; receipt.indexCount = info->indexCount;
	receipt.firstIndex = info->firstIndex; receipt.instanceCount = info->instanceCount;
	receipt.firstInstance = info->firstInstance;
	memcpy( receipt.expectedRgba, kExpectedRgba, sizeof( kExpectedRgba ) );
	@autoreleasepool {
		MTLRenderPassDescriptor *pass = [MTLRenderPassDescriptor renderPassDescriptor];
		const ralMetalRenderAttachment_t *color = &info->renderPlan->colorAttachments[0];
		const ralMetalRenderAttachment_t *depth = info->renderPlan->depthAttachment;
		pass.colorAttachments[0].texture = (id<MTLTexture>)color->nativeTexture;
		pass.colorAttachments[0].loadAction = MTLLoadActionClear;
		pass.colorAttachments[0].storeAction = MTLStoreActionStore;
		pass.colorAttachments[0].clearColor = MTLClearColorMake( color->clearValue.color[0],
			color->clearValue.color[1], color->clearValue.color[2], color->clearValue.color[3] );
		pass.depthAttachment.texture = (id<MTLTexture>)depth->nativeTexture;
		pass.depthAttachment.loadAction = MTLLoadActionClear;
		pass.depthAttachment.storeAction = depth->storeOp == RAL_STORE_OP_STORE
			? MTLStoreActionStore : MTLStoreActionDontCare;
		pass.depthAttachment.clearDepth = depth->clearValue.depthStencil.depth;
		id<MTLCommandBuffer> command = [queue commandBuffer];
		if ( !command || !RalMetal_CoreBeginCommand( core, info->drawGeneration,
				&lifecycle, &recording ) ) { free( pixels ); return qfalse; }
		id<MTLRenderCommandEncoder> encoder = [command renderCommandEncoderWithDescriptor:pass];
		if ( !encoder ) { free( pixels ); return qfalse; }
		[encoder setViewport:(MTLViewport){ 0.0, 0.0, (double)info->renderPlan->width,
			(double)info->renderPlan->height, 0.0, 1.0 }];
		[encoder setScissorRect:(MTLScissorRect){ 0u, 0u, info->renderPlan->width,
			info->renderPlan->height }];
		[encoder setRenderPipelineState:RalMetal_PipelineNativeState( info->pipeline )];
		[encoder setDepthStencilState:RalMetal_PipelineNativeDepthState( info->pipeline )];
		[encoder setVertexBuffer:(id<MTLBuffer>)info->vertexBuffer.nativeBuffer
			offset:(NSUInteger)info->vertexBuffer.byteOffset atIndex:0u];
		[encoder setFragmentBuffer:RalMetal_BindGroupNativeArgumentBuffer( info->fragmentGroup )
			offset:0u atIndex:0u];
		RalMetal_BindGroupUseFragmentResources( info->fragmentGroup, encoder );
		[encoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle
			indexCount:info->indexCount indexType:MTLIndexTypeUInt16
			indexBuffer:(id<MTLBuffer>)info->indexBuffer.nativeBuffer
			indexBufferOffset:(NSUInteger)( info->indexBuffer.byteOffset
				+ (uint64_t)info->firstIndex * sizeof( uint16_t ) )
			instanceCount:info->instanceCount baseVertex:0 baseInstance:info->firstInstance];
		[encoder endEncoding];
		if ( Ral_CommandLifecyclePublishEnd( &lifecycle, &recording,
				&executable ) != ralSuccess ) { free( pixels ); return qfalse; }
		[command commit]; [command waitUntilCompleted];
		if ( command.status != MTLCommandBufferStatusCompleted
				|| !RalMetal_CorePublishSubmission( core, &lifecycle, &executable,
					&receipt.submission ) ) { free( pixels ); return qfalse; }
	}
	[(id<MTLTexture>)info->renderPlan->colorAttachments[0].nativeTexture
		getBytes:pixels bytesPerRow:(NSUInteger)info->renderPlan->width * 4u
		fromRegion:MTLRegionMake2D( 0u, 0u, info->renderPlan->width,
			info->renderPlan->height ) mipmapLevel:0u];
	for ( i = 0u; i < pixelCount; ++i ) if ( memcmp( pixels + i * 4u,
			kExpectedRgba, sizeof( kExpectedRgba ) ) != 0 ) { free( pixels ); return qfalse; }
	receipt.recording = recording; receipt.executable = executable;
	receipt.completionGeneration = receipt.submission.generation;
	receipt.pixelCount = pixelCount; receipt.pixelDigest = DigestBytes( pixels, pixelBytes );
	receipt.ready = qtrue;
	free( pixels );
	if ( !DrawReceiptValid( &receipt ) ) return qfalse;
	*outReceipt = receipt;
	return qtrue;
}
