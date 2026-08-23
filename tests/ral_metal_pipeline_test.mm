// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "ral_metal_pipeline.h"

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

int main( void ) {
	ralMetalCoreCreateInfo_t coreInfo = { 17u };
	ralMetalCore_t *core = NULL;
	ralMetalCoreReceipt_t coreReceipt;
	ralBindEntry_t entries[2] = {
		{ 0u, RAL_BIND_SAMPLED_TEXTURE, 1u, RAL_STAGE_FRAGMENT, RAL_BIND_TEXTURE_VIEW_2D, qfalse },
		{ 32u, RAL_BIND_SAMPLER, 1u, RAL_STAGE_FRAGMENT, RAL_BIND_TEXTURE_VIEW_UNSPECIFIED, qfalse }
	};
	ralBindGroupLayoutCreateInfo_t layoutInfo = { entries, 2u, qfalse, "overlay-fragment" };
	ralMetalBindLayout_t *layout = NULL;
	ralMetalBindLayoutReceipt_t layoutReceipt;
	ralMetalPipelineCreateInfo_t createInfo;
	ralMetalPipeline_t *pipeline = (ralMetalPipeline_t *)(uintptr_t)0x1234u;
	ralMetalPipelineReceipt_t receipt, before, exact;
	CHECK( RalMetal_CoreCreate( &coreInfo, &core, &coreReceipt ) );
	CHECK( RalMetal_BindLayoutCreate( core, &coreReceipt, &layoutInfo, 23u,
		&layout, &layoutReceipt ) );
	memset( &createInfo, 0, sizeof( createInfo ) );
	createInfo.vertexSourcePath = RAL_METAL_TEST_VERTEX_SOURCE;
	createInfo.fragmentSourcePath = RAL_METAL_TEST_FRAGMENT_SOURCE;
	createInfo.vertexLibraryPath = RAL_METAL_TEST_VERTEX_LIBRARY;
	createInfo.fragmentLibraryPath = RAL_METAL_TEST_FRAGMENT_LIBRARY;
	createInfo.expectedVertexSourceDigest = {
		UINT64_C(0x8498bc3a26f377c9), UINT64_C(0x15f4b85f5c921883) };
	createInfo.expectedFragmentSourceDigest = {
		UINT64_C(0xf79c3d7e42e966ad), UINT64_C(0x5df4210a60636729) };
	createInfo.fragmentLayout = layout;
	createInfo.fragmentLayoutReceipt = &layoutReceipt;
	createInfo.colorFormat = RAL_FORMAT_R8G8B8A8_UNORM;
	createInfo.depthFormat = RAL_FORMAT_D32_SFLOAT;
	createInfo.sampleCount = 1u; createInfo.generation = 41u;
	memset( &receipt, 0x5a, sizeof( receipt ) ); before = receipt;
	createInfo.expectedVertexSourceDigest.lane0++;
	CHECK( !RalMetal_PipelineCreate( core, &coreReceipt, &createInfo, &pipeline, &receipt ) );
	CHECK( pipeline == (ralMetalPipeline_t *)(uintptr_t)0x1234u
		&& memcmp( &receipt, &before, sizeof( receipt ) ) == 0 );
	createInfo.expectedVertexSourceDigest.lane0--;
	const char *validVertexLibrary = createInfo.vertexLibraryPath;
	const char *validFragmentLibrary = createInfo.fragmentLibraryPath;
	createInfo.vertexLibraryPath = "/private/tmp/missing-wired.metallib";
	CHECK( !RalMetal_PipelineCreate( core, &coreReceipt, &createInfo, &pipeline, &receipt ) );
	createInfo.vertexLibraryPath = validVertexLibrary;
	createInfo.fragmentLibraryPath = validVertexLibrary;
	CHECK( !RalMetal_PipelineCreate( core, &coreReceipt, &createInfo, &pipeline, &receipt ) );
	createInfo.fragmentLibraryPath = validFragmentLibrary;
	createInfo.colorFormat = RAL_FORMAT_R16G16B16A16_SFLOAT;
	CHECK( !RalMetal_PipelineCreate( core, &coreReceipt, &createInfo, &pipeline, &receipt ) );
	createInfo.colorFormat = RAL_FORMAT_R8G8B8A8_UNORM;
	createInfo.sampleCount = 2u;
	CHECK( !RalMetal_PipelineCreate( core, &coreReceipt, &createInfo, &pipeline, &receipt ) );
	createInfo.sampleCount = 1u;
	createInfo.generation = UINT64_MAX;
	CHECK( !RalMetal_PipelineCreate( core, &coreReceipt, &createInfo, &pipeline, &receipt ) );
	createInfo.generation = 41u;
	ralMetalCoreReceipt_t staleCore = coreReceipt;
	staleCore.generation++;
	CHECK( !RalMetal_PipelineCreate( core, &staleCore, &createInfo, &pipeline, &receipt ) );
	ralMetalBindLayoutReceipt_t staleLayout = layoutReceipt;
	staleLayout.layoutGeneration++;
	createInfo.fragmentLayoutReceipt = &staleLayout;
	CHECK( !RalMetal_PipelineCreate( core, &coreReceipt, &createInfo, &pipeline, &receipt ) );
	createInfo.fragmentLayoutReceipt = &layoutReceipt;
	entries[1].binding = 31u;
	ralMetalBindLayout_t *wrongLayout = NULL;
	ralMetalBindLayoutReceipt_t wrongLayoutReceipt;
	CHECK( RalMetal_BindLayoutCreate( core, &coreReceipt, &layoutInfo, 24u,
		&wrongLayout, &wrongLayoutReceipt ) );
	createInfo.fragmentLayout = wrongLayout; createInfo.fragmentLayoutReceipt = &wrongLayoutReceipt;
	CHECK( !RalMetal_PipelineCreate( core, &coreReceipt, &createInfo, &pipeline, &receipt ) );
	createInfo.fragmentLayout = layout; createInfo.fragmentLayoutReceipt = &layoutReceipt;
	CHECK( pipeline == (ralMetalPipeline_t *)(uintptr_t)0x1234u
		&& memcmp( &receipt, &before, sizeof( receipt ) ) == 0 );
	CHECK( RalMetal_PipelineCreate( core, &coreReceipt, &createInfo, &pipeline, &receipt ) );
	CHECK( receipt.vertexStride == 20u && receipt.vertexInputOffsets[2] == 12u
		&& receipt.fragmentLayoutIdentity == layoutReceipt.layoutIdentity
		&& receipt.vertexLibraryBytes != 0u && receipt.fragmentLibraryBytes != 0u );
	exact = receipt; CHECK( RalMetal_PipelineReceiptExact( &receipt, &exact ) );
#define MUTATE(field) do { exact = receipt; exact.field++; \
	CHECK( !RalMetal_PipelineReceiptExact( &receipt, &exact ) ); } while (0)
	MUTATE( coreGeneration ); MUTATE( pipelineGeneration ); MUTATE( vertexLibraryBytes );
	MUTATE( fragmentLibraryDigest.lane1 ); MUTATE( fragmentLayoutGeneration );
	MUTATE( vertexInputOffsets[2] );
#undef MUTATE
	exact = receipt; exact.pipelineIdentity = exact.fragmentLayoutIdentity;
	CHECK( !RalMetal_PipelineReceiptExact( &exact, &exact ) );
	exact = receipt; exact.colorFormat = RAL_FORMAT_D32_SFLOAT;
	CHECK( !RalMetal_PipelineReceiptExact( &exact, &exact ) );
	exact = receipt; exact.vertexInputs[1].format = RAL_FORMAT_R32G32B32A32_SFLOAT;
	CHECK( !RalMetal_PipelineReceiptExact( &exact, &exact ) );

	RalMetal_PipelineDestroy( pipeline );
	RalMetal_BindLayoutDestroy( wrongLayout );
	RalMetal_BindLayoutDestroy( layout );
	RalMetal_CoreDestroy( core );
	puts( "ral metal pipeline: PASS" );
	return 0;
}
