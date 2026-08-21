// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "ral_metal_draw.h"
#include "ral_metal_internal.h"

#import <Foundation/Foundation.h>
#import <Metal/Metal.h>

#include <stdio.h>
#include <string.h>

#ifndef RAL_METAL_TEST_VERTEX_SOURCE
#error RAL_METAL_TEST_VERTEX_SOURCE is required
#endif
#ifndef RAL_METAL_TEST_FRAGMENT_SOURCE
#error RAL_METAL_TEST_FRAGMENT_SOURCE is required
#endif
#ifndef RAL_METAL_TEST_VERTEX_LIBRARY
#error RAL_METAL_TEST_VERTEX_LIBRARY is required
#endif
#ifndef RAL_METAL_TEST_FRAGMENT_LIBRARY
#error RAL_METAL_TEST_FRAGMENT_LIBRARY is required
#endif

#define CHECK(x) do { if (!(x)) { fprintf(stderr,"FAIL %s:%d: %s\n",__FILE__,__LINE__,#x); return 1; } } while (0)

typedef struct {
	float position[2];
	uint8_t color[4];
	float uv[2];
} testOverlayVertex_t;

static const testOverlayVertex_t vertices[4] = {
	{ { -1.0f, -1.0f }, { 255u, 255u, 255u, 255u }, { 0.0f, 1.0f } },
	{ { -1.0f,  1.0f }, { 255u, 255u, 255u, 255u }, { 0.0f, 0.0f } },
	{ {  1.0f,  1.0f }, { 255u, 255u, 255u, 255u }, { 1.0f, 0.0f } },
	{ {  1.0f, -1.0f }, { 255u, 255u, 255u, 255u }, { 1.0f, 1.0f } }
};
static const uint16_t indices[6] = { 0u, 1u, 2u, 0u, 2u, 3u };
static const uint8_t textureBytes[16] = {
	68u, 136u, 204u, 255u, 68u, 136u, 204u, 255u,
	68u, 136u, 204u, 255u, 68u, 136u, 204u, 255u
};

int main( void ) {
	ralMetalCoreCreateInfo_t coreInfo = { 51u };
	ralMetalCore_t *core = NULL;
	ralMetalCoreReceipt_t coreReceipt, staleCore;
	ralBindEntry_t entries[2] = {
		{ 0u, RAL_BIND_SAMPLED_TEXTURE, 1u, RAL_STAGE_FRAGMENT, RAL_BIND_TEXTURE_VIEW_2D },
		{ 32u, RAL_BIND_SAMPLER, 1u, RAL_STAGE_FRAGMENT, RAL_BIND_TEXTURE_VIEW_UNSPECIFIED }
	};
	ralBindGroupLayoutCreateInfo_t layoutInfo = { entries, 2u, qfalse, "overlay-draw" };
	ralMetalBindLayout_t *layout = NULL;
	ralMetalBindLayoutReceipt_t layoutReceipt;
	ralMetalPipelineCreateInfo_t pipelineInfo;
	ralMetalPipeline_t *pipeline = NULL;
	ralMetalPipelineReceipt_t pipelineReceipt;
	ralMetalBindResource_t resources[2];
	ralMetalBindGroup_t *group = NULL;
	ralMetalBindGroupReceipt_t groupReceipt;
	ralMetalRenderAttachment_t color, depth;
	ralMetalRenderPlan_t renderPlan;
	ralMetalIndexedDrawInfo_t drawInfo, badDraw;
	ralMetalIndexedDrawReceipt_t receipt, exact, before;
	id<MTLDevice> device;
	id<MTLBuffer> vertexBuffer, indexBuffer;
	id<MTLTexture> sampledTexture, colorTexture, depthTexture;
	id<MTLSamplerState> sampler;
	MTLTextureDescriptor *textureInfo;
	MTLSamplerDescriptor *samplerInfo;
	CHECK( sizeof( testOverlayVertex_t ) == 20u );
	CHECK( RalMetal_CoreCreate( &coreInfo, &core, &coreReceipt ) );
	device = RalMetal_CoreNativeDevice( core ); CHECK( device != nil );
	CHECK( RalMetal_BindLayoutCreate( core, &coreReceipt, &layoutInfo, 52u,
		&layout, &layoutReceipt ) );
	memset( &pipelineInfo, 0, sizeof( pipelineInfo ) );
	pipelineInfo.vertexSourcePath = RAL_METAL_TEST_VERTEX_SOURCE;
	pipelineInfo.fragmentSourcePath = RAL_METAL_TEST_FRAGMENT_SOURCE;
	pipelineInfo.vertexLibraryPath = RAL_METAL_TEST_VERTEX_LIBRARY;
	pipelineInfo.fragmentLibraryPath = RAL_METAL_TEST_FRAGMENT_LIBRARY;
	pipelineInfo.expectedVertexSourceDigest = {
		UINT64_C(0x8498bc3a26f377c9), UINT64_C(0x15f4b85f5c921883) };
	pipelineInfo.expectedFragmentSourceDigest = {
		UINT64_C(0xf79c3d7e42e966ad), UINT64_C(0x5df4210a60636729) };
	pipelineInfo.fragmentLayout = layout;
	pipelineInfo.fragmentLayoutReceipt = &layoutReceipt;
	pipelineInfo.colorFormat = RAL_FORMAT_R8G8B8A8_UNORM;
	pipelineInfo.depthFormat = RAL_FORMAT_D32_SFLOAT;
	pipelineInfo.sampleCount = 1u; pipelineInfo.generation = 53u;
	CHECK( RalMetal_PipelineCreate( core, &coreReceipt, &pipelineInfo,
		&pipeline, &pipelineReceipt ) );
	vertexBuffer = [device newBufferWithBytes:vertices length:sizeof( vertices )
		options:MTLResourceStorageModeShared];
	indexBuffer = [device newBufferWithBytes:indices length:sizeof( indices )
		options:MTLResourceStorageModeShared];
	textureInfo = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
		width:2u height:2u mipmapped:NO];
	textureInfo.storageMode = MTLStorageModeShared; textureInfo.usage = MTLTextureUsageShaderRead;
	sampledTexture = [device newTextureWithDescriptor:textureInfo];
	[sampledTexture replaceRegion:MTLRegionMake2D( 0u, 0u, 2u, 2u ) mipmapLevel:0u
		withBytes:textureBytes bytesPerRow:8u];
	samplerInfo = [[[MTLSamplerDescriptor alloc] init] autorelease];
	samplerInfo.minFilter = MTLSamplerMinMagFilterNearest;
	samplerInfo.magFilter = MTLSamplerMinMagFilterNearest;
	samplerInfo.sAddressMode = MTLSamplerAddressModeClampToEdge;
	samplerInfo.tAddressMode = MTLSamplerAddressModeClampToEdge;
	sampler = [device newSamplerStateWithDescriptor:samplerInfo];
	CHECK( vertexBuffer && indexBuffer && sampledTexture && sampler );
	memset( resources, 0, sizeof( resources ) );
	resources[0].binding = 0u; resources[0].type = RAL_BIND_SAMPLED_TEXTURE;
	resources[0].resourceIdentity = (uintptr_t)(void *)sampledTexture;
	resources[0].resourceGeneration = 54u; resources[0].nativeResource = (void *)sampledTexture;
	resources[1].binding = 32u; resources[1].type = RAL_BIND_SAMPLER;
	resources[1].resourceIdentity = (uintptr_t)(void *)sampler;
	resources[1].resourceGeneration = 55u; resources[1].nativeResource = (void *)sampler;
	CHECK( RalMetal_BindGroupCreate( core, &coreReceipt, layout, &layoutReceipt,
		resources, 2u, 56u, &group, &groupReceipt ) );
	textureInfo = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
		width:8u height:8u mipmapped:NO];
	textureInfo.storageMode = MTLStorageModeShared;
	textureInfo.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
	colorTexture = [device newTextureWithDescriptor:textureInfo];
	textureInfo = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatDepth32Float
		width:8u height:8u mipmapped:NO];
	textureInfo.storageMode = MTLStorageModePrivate; textureInfo.usage = MTLTextureUsageRenderTarget;
	depthTexture = [device newTextureWithDescriptor:textureInfo];
	CHECK( colorTexture && depthTexture );
	memset( &color, 0, sizeof( color ) );
	color.nativeTexture = (void *)colorTexture;
	color.textureIdentity = (uintptr_t)(void *)colorTexture;
	color.textureGeneration = 57u; color.format = RAL_FORMAT_R8G8B8A8_UNORM;
	color.width = 8u; color.height = 8u; color.sampleCount = 1u;
	color.loadOp = RAL_LOAD_OP_CLEAR; color.storeOp = RAL_STORE_OP_STORE;
	color.clearValue.color[3] = 1.0f;
	memset( &depth, 0, sizeof( depth ) );
	depth.nativeTexture = (void *)depthTexture;
	depth.textureIdentity = (uintptr_t)(void *)depthTexture;
	depth.textureGeneration = 58u; depth.format = RAL_FORMAT_D32_SFLOAT;
	depth.width = 8u; depth.height = 8u; depth.sampleCount = 1u;
	depth.loadOp = RAL_LOAD_OP_CLEAR; depth.storeOp = RAL_STORE_OP_DONT_CARE;
	depth.clearValue.depthStencil.depth = 1.0f;
	renderPlan.colorAttachments = &color; renderPlan.colorAttachmentCount = 1u;
	renderPlan.depthAttachment = &depth; renderPlan.width = 8u; renderPlan.height = 8u;
	memset( &drawInfo, 0, sizeof( drawInfo ) );
	drawInfo.pipeline = pipeline; drawInfo.pipelineReceipt = &pipelineReceipt;
	drawInfo.fragmentGroup = group; drawInfo.fragmentGroupReceipt = &groupReceipt;
	drawInfo.vertexBuffer = { (void *)vertexBuffer, (uintptr_t)(void *)vertexBuffer,
		59u, 0u, sizeof( vertices ) };
	drawInfo.indexBuffer = { (void *)indexBuffer, (uintptr_t)(void *)indexBuffer,
		60u, 0u, sizeof( indices ) };
	drawInfo.renderPlan = &renderPlan; drawInfo.vertexCount = 4u;
	drawInfo.indexCount = 6u; drawInfo.instanceCount = 1u;
	drawInfo.drawGeneration = 61u;
	memset( &receipt, 0x5a, sizeof( receipt ) ); before = receipt;
	badDraw = drawInfo; badDraw.indexBuffer.byteSize = 10u;
	CHECK( !RalMetal_DrawIndexedOverlay( core, &coreReceipt, &badDraw, &receipt ) );
	badDraw = drawInfo; badDraw.vertexBuffer = badDraw.indexBuffer;
	CHECK( !RalMetal_DrawIndexedOverlay( core, &coreReceipt, &badDraw, &receipt ) );
	badDraw = drawInfo; badDraw.indexCount = 5u;
	CHECK( !RalMetal_DrawIndexedOverlay( core, &coreReceipt, &badDraw, &receipt ) );
	staleCore = coreReceipt; staleCore.generation++;
	CHECK( !RalMetal_DrawIndexedOverlay( core, &staleCore, &drawInfo, &receipt ) );
	ralMetalPipelineReceipt_t stalePipeline = pipelineReceipt;
	stalePipeline.pipelineGeneration++;
	badDraw = drawInfo; badDraw.pipelineReceipt = &stalePipeline;
	CHECK( !RalMetal_DrawIndexedOverlay( core, &coreReceipt, &badDraw, &receipt ) );
	ralMetalBindGroupReceipt_t staleGroup = groupReceipt;
	staleGroup.resources[0].resourceGeneration++;
	badDraw = drawInfo; badDraw.fragmentGroupReceipt = &staleGroup;
	CHECK( !RalMetal_DrawIndexedOverlay( core, &coreReceipt, &badDraw, &receipt ) );
	color.textureGeneration = UINT64_MAX;
	CHECK( !RalMetal_DrawIndexedOverlay( core, &coreReceipt, &drawInfo, &receipt ) );
	color.textureGeneration = 57u;
	uint8_t badTexel[4] = { 1u, 2u, 3u, 4u };
	[sampledTexture replaceRegion:MTLRegionMake2D( 0u, 0u, 1u, 1u ) mipmapLevel:0u
		withBytes:badTexel bytesPerRow:4u];
	CHECK( !RalMetal_DrawIndexedOverlay( core, &coreReceipt, &drawInfo, &receipt ) );
	[sampledTexture replaceRegion:MTLRegionMake2D( 0u, 0u, 2u, 2u ) mipmapLevel:0u
		withBytes:textureBytes bytesPerRow:8u];
	CHECK( memcmp( &receipt, &before, sizeof( receipt ) ) == 0 );
	CHECK( RalMetal_DrawIndexedOverlay( core, &coreReceipt, &drawInfo, &receipt ) );
	CHECK( receipt.pixelCount == 64u && receipt.pixelDigest != 0u
		&& !memcmp( receipt.expectedRgba, textureBytes, 4u )
		&& receipt.submission.commands[0].state == RAL_COMMAND_SUBMITTED );
	exact = receipt; CHECK( RalMetal_IndexedDrawReceiptExact( &receipt, &exact ) );
#define MUTATE(field) do { exact = receipt; exact.field++; \
	CHECK( !RalMetal_IndexedDrawReceiptExact( &receipt, &exact ) ); } while (0)
	MUTATE( coreGeneration ); MUTATE( drawGeneration ); MUTATE( pipelineGeneration );
	MUTATE( layoutGeneration ); MUTATE( groupGeneration );
	MUTATE( sampledTextureGeneration ); MUTATE( samplerGeneration );
	MUTATE( vertexBufferGeneration ); MUTATE( indexBufferBytes );
	MUTATE( colorTextureGeneration ); MUTATE( depthTextureGeneration );
	MUTATE( indexCount ); MUTATE( submission.generation ); MUTATE( pixelDigest );
#undef MUTATE
	exact = receipt; exact.indexBufferIdentity = exact.vertexBufferIdentity;
	CHECK( !RalMetal_IndexedDrawReceiptExact( &exact, &exact ) );
	exact = receipt; exact.colorTextureIdentity = exact.sampledTextureIdentity;
	CHECK( !RalMetal_IndexedDrawReceiptExact( &exact, &exact ) );
	exact = receipt; exact.expectedRgba[2]++;
	CHECK( !RalMetal_IndexedDrawReceiptExact( &exact, &exact ) );

	RalMetal_BindGroupDestroy( group );
	RalMetal_PipelineDestroy( pipeline );
	RalMetal_BindLayoutDestroy( layout );
	[depthTexture release]; [colorTexture release]; [sampler release];
	[sampledTexture release]; [indexBuffer release]; [vertexBuffer release];
	RalMetal_CoreDestroy( core );
	puts( "ral metal indexed draw: PASS" );
	return 0;
}
