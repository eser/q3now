// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "ral_metal_bind_group.h"
#include "ral_metal_internal.h"

#import <Foundation/Foundation.h>
#import <Metal/Metal.h>

#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr,"FAIL %s:%d: %s\n",__FILE__,__LINE__,#x); return 1; } } while (0)

static void FillResources( ralMetalBindResource_t resources[6],
		id<MTLBuffer> uniformBuffer, id<MTLBuffer> storageBuffer,
		id<MTLTexture> sampledA, id<MTLTexture> sampledB,
		id<MTLTexture> storageTexture, id<MTLSamplerState> sampler ) {
	void *native[6] = { (void *)uniformBuffer, (void *)storageBuffer,
		(void *)sampledA, (void *)sampledB, (void *)storageTexture, (void *)sampler };
	uint32_t binding[6] = { 0u, 1u, 3u, 3u, 5u, 7u };
	uint32_t element[6] = { 0u, 0u, 0u, 1u, 0u, 0u };
	ralBindType_t type[6] = { RAL_BIND_UNIFORM_BUFFER, RAL_BIND_STORAGE_BUFFER,
		RAL_BIND_SAMPLED_TEXTURE, RAL_BIND_SAMPLED_TEXTURE,
		RAL_BIND_STORAGE_TEXTURE, RAL_BIND_SAMPLER };
	uint32_t i;
	memset( resources, 0, sizeof( *resources ) * 6u );
	for ( i = 0u; i < 6u; ++i ) {
		resources[i].binding = binding[i]; resources[i].arrayElement = element[i];
		resources[i].type = type[i]; resources[i].nativeResource = native[i];
		resources[i].resourceIdentity = (uintptr_t)native[i];
		resources[i].resourceGeneration = 20u + i;
	}
	resources[0].bufferRange = 256u;
	resources[1].bufferOffset = 64u; resources[1].bufferRange = 512u;
}

int main( void ) {
	ralMetalCoreCreateInfo_t coreInfo = { 3u };
	ralMetalCore_t *core = NULL;
	ralMetalCoreReceipt_t coreReceipt, wrongCore;
	ralBindEntry_t entries[5] = {
		{ 0u, RAL_BIND_UNIFORM_BUFFER, 1u, RAL_STAGE_VERTEX, RAL_BIND_TEXTURE_VIEW_UNSPECIFIED },
		{ 1u, RAL_BIND_STORAGE_BUFFER, 1u, RAL_STAGE_VERTEX | RAL_STAGE_FRAGMENT, RAL_BIND_TEXTURE_VIEW_UNSPECIFIED },
		{ 3u, RAL_BIND_SAMPLED_TEXTURE, 2u, RAL_STAGE_FRAGMENT, RAL_BIND_TEXTURE_VIEW_2D },
		{ 5u, RAL_BIND_STORAGE_TEXTURE, 1u, RAL_STAGE_COMPUTE, RAL_BIND_TEXTURE_VIEW_2D },
		{ 7u, RAL_BIND_SAMPLER, 1u, RAL_STAGE_FRAGMENT, RAL_BIND_TEXTURE_VIEW_UNSPECIFIED }
	};
	ralBindGroupLayoutCreateInfo_t layoutInfo = { entries, 5u, qfalse, "metal-test" };
	ralMetalBindLayout_t *layout = (ralMetalBindLayout_t *)(uintptr_t)0x1234u;
	ralMetalBindLayoutReceipt_t layoutReceipt, beforeLayout, exactLayout;
	ralMetalBindGroup_t *group = (ralMetalBindGroup_t *)(uintptr_t)0x5678u;
	ralMetalBindGroupReceipt_t groupReceipt, beforeGroup, exactGroup;
	ralMetalBindResource_t resources[6], badResources[6];
	id<MTLDevice> device;
	id<MTLBuffer> uniformBuffer, storageBuffer;
	id<MTLTexture> sampledA, sampledB, storageTexture;
	id<MTLSamplerState> sampler;
	MTLTextureDescriptor *textureInfo;
	MTLSamplerDescriptor *samplerInfo;
	CHECK( RalMetal_CoreCreate( &coreInfo, &core, &coreReceipt ) );
	device = RalMetal_CoreNativeDevice( core ); CHECK( device != nil );
	uniformBuffer = [device newBufferWithLength:1024u options:MTLResourceStorageModeShared];
	storageBuffer = [device newBufferWithLength:2048u options:MTLResourceStorageModeShared];
	textureInfo = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
		width:4u height:4u mipmapped:NO];
	textureInfo.usage = MTLTextureUsageShaderRead | MTLTextureUsageShaderWrite;
	sampledA = [device newTextureWithDescriptor:textureInfo];
	sampledB = [device newTextureWithDescriptor:textureInfo];
	storageTexture = [device newTextureWithDescriptor:textureInfo];
	samplerInfo = [[[MTLSamplerDescriptor alloc] init] autorelease];
	sampler = [device newSamplerStateWithDescriptor:samplerInfo];
	CHECK( uniformBuffer && storageBuffer && sampledA && sampledB && storageTexture && sampler );

	memset( &layoutReceipt, 0x5a, sizeof( layoutReceipt ) ); beforeLayout = layoutReceipt;
	entries[0].stageFlags = 0u;
	CHECK( !RalMetal_BindLayoutCreate( core, &coreReceipt, &layoutInfo, 11u, &layout, &layoutReceipt ) );
	CHECK( layout == (ralMetalBindLayout_t *)(uintptr_t)0x1234u );
	CHECK( memcmp( &layoutReceipt, &beforeLayout, sizeof( layoutReceipt ) ) == 0 );
	entries[0].stageFlags = RAL_STAGE_VERTEX;
	entries[2].count = 0u;
	CHECK( !RalMetal_BindLayoutCreate( core, &coreReceipt, &layoutInfo, 11u, &layout, &layoutReceipt ) );
	entries[2].count = 2u;
	layoutInfo.bindless = qtrue;
	CHECK( !RalMetal_BindLayoutCreate( core, &coreReceipt, &layoutInfo, 11u, &layout, &layoutReceipt ) );
	layoutInfo.bindless = qfalse;
	entries[2].textureViewType = RAL_BIND_TEXTURE_VIEW_UNSPECIFIED;
	CHECK( !RalMetal_BindLayoutCreate( core, &coreReceipt, &layoutInfo, 11u, &layout, &layoutReceipt ) );
	entries[2].textureViewType = RAL_BIND_TEXTURE_VIEW_2D;
	entries[3].type = RAL_BIND_COMBINED_TEXTURE_SAMPLER;
	CHECK( !RalMetal_BindLayoutCreate( core, &coreReceipt, &layoutInfo, 11u, &layout, &layoutReceipt ) );
	entries[3].type = RAL_BIND_STORAGE_TEXTURE;
	entries[3].binding = entries[2].binding;
	CHECK( !RalMetal_BindLayoutCreate( core, &coreReceipt, &layoutInfo, 11u, &layout, &layoutReceipt ) );
	entries[3].binding = 5u;
	CHECK( RalMetal_BindLayoutCreate( core, &coreReceipt, &layoutInfo, 11u, &layout, &layoutReceipt ) );
	CHECK( layoutReceipt.entryCount == 5u && layoutReceipt.totalArgumentCount == 6u );
	CHECK( layoutReceipt.entries[2].argumentIndex == 2u
		&& layoutReceipt.entries[3].argumentIndex == 4u );
	exactLayout = layoutReceipt;
	CHECK( RalMetal_BindLayoutReceiptExact( &layoutReceipt, &exactLayout ) );
#define MUTATE_LAYOUT(field) do { exactLayout = layoutReceipt; exactLayout.field++; \
	CHECK( !RalMetal_BindLayoutReceiptExact( &layoutReceipt, &exactLayout ) ); } while (0)
	MUTATE_LAYOUT( coreGeneration ); MUTATE_LAYOUT( layoutGeneration );
	MUTATE_LAYOUT( encodedLength ); MUTATE_LAYOUT( entries[2].argumentIndex );
	exactLayout = layoutReceipt;
	exactLayout.entries[2].textureViewType = RAL_BIND_TEXTURE_VIEW_3D;
	CHECK( !RalMetal_BindLayoutReceiptExact( &layoutReceipt, &exactLayout ) );
#undef MUTATE_LAYOUT

	FillResources( resources, uniformBuffer, storageBuffer, sampledA, sampledB,
		storageTexture, sampler );
	memset( &groupReceipt, 0x5a, sizeof( groupReceipt ) ); beforeGroup = groupReceipt;
	memcpy( badResources, resources, sizeof( resources ) ); badResources[3].arrayElement = 0u;
	CHECK( !RalMetal_BindGroupCreate( core, &coreReceipt, layout, &layoutReceipt,
		badResources, 6u, 31u, &group, &groupReceipt ) );
	CHECK( group == (ralMetalBindGroup_t *)(uintptr_t)0x5678u
		&& memcmp( &groupReceipt, &beforeGroup, sizeof( groupReceipt ) ) == 0 );
	memcpy( badResources, resources, sizeof( resources ) ); badResources[0].type = RAL_BIND_SAMPLER;
	CHECK( !RalMetal_BindGroupCreate( core, &coreReceipt, layout, &layoutReceipt,
		badResources, 6u, 31u, &group, &groupReceipt ) );
	memcpy( badResources, resources, sizeof( resources ) ); badResources[0].bufferRange = 4096u;
	CHECK( !RalMetal_BindGroupCreate( core, &coreReceipt, layout, &layoutReceipt,
		badResources, 6u, 31u, &group, &groupReceipt ) );
	CHECK( !RalMetal_BindGroupCreate( core, &coreReceipt, layout, &layoutReceipt,
		resources, 5u, 31u, &group, &groupReceipt ) );
	memcpy( badResources, resources, sizeof( resources ) );
	badResources[0].resourceGeneration = UINT64_MAX;
	CHECK( !RalMetal_BindGroupCreate( core, &coreReceipt, layout, &layoutReceipt,
		badResources, 6u, 31u, &group, &groupReceipt ) );
	memcpy( badResources, resources, sizeof( resources ) );
	badResources[0].nativeResource = (void *)sampler;
	badResources[0].resourceIdentity = (uintptr_t)(void *)sampler;
	CHECK( !RalMetal_BindGroupCreate( core, &coreReceipt, layout, &layoutReceipt,
		badResources, 6u, 31u, &group, &groupReceipt ) );
	wrongCore = coreReceipt; wrongCore.generation++;
	CHECK( !RalMetal_BindGroupCreate( core, &wrongCore, layout, &layoutReceipt,
		resources, 6u, 31u, &group, &groupReceipt ) );
	CHECK( RalMetal_BindGroupCreate( core, &coreReceipt, layout, &layoutReceipt,
		resources, 6u, 31u, &group, &groupReceipt ) );
	CHECK( groupReceipt.resourceCount == 6u && groupReceipt.argumentBufferBytes == layoutReceipt.encodedLength );
	exactGroup = groupReceipt;
	CHECK( RalMetal_BindGroupReceiptExact( &groupReceipt, &exactGroup ) );
#define MUTATE_GROUP(field) do { exactGroup = groupReceipt; exactGroup.field++; \
	CHECK( !RalMetal_BindGroupReceiptExact( &groupReceipt, &exactGroup ) ); } while (0)
	MUTATE_GROUP( coreGeneration ); MUTATE_GROUP( layoutGeneration );
	MUTATE_GROUP( groupGeneration ); MUTATE_GROUP( argumentBufferBytes );
	MUTATE_GROUP( resources[0].resourceGeneration ); MUTATE_GROUP( resources[3].arrayElement );
	MUTATE_GROUP( resources[1].bufferRange );
#undef MUTATE_GROUP
	exactGroup = groupReceipt; exactGroup.resources[0].type = RAL_BIND_COMBINED_TEXTURE_SAMPLER;
	CHECK( !RalMetal_BindGroupReceiptExact( &exactGroup, &exactGroup ) );
	exactGroup = groupReceipt;
	exactGroup.argumentBufferIdentity = exactGroup.resources[0].resourceIdentity;
	CHECK( !RalMetal_BindGroupReceiptExact( &exactGroup, &exactGroup ) );

	RalMetal_BindGroupDestroy( group );
	RalMetal_BindLayoutDestroy( layout );
	[sampler release]; [storageTexture release]; [sampledB release]; [sampledA release];
	[storageBuffer release]; [uniformBuffer release];
	RalMetal_CoreDestroy( core );
	puts( "ral metal bind group: PASS" );
	return 0;
}
