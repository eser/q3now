// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "ral_lighting_composition.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if ( !(x) ) { fprintf( stderr, "FAIL %d: %s\n", \
	__LINE__, #x ); return 1; } } while ( 0 )

static ralLightingCompositionRequest_t Modern( void ) {
	ralLightingCompositionRequest_t request;
	memset( &request, 0, sizeof( request ) );
	request.schemaVersion = RAL_LIGHTING_COMPOSITION_SCHEMA_VERSION;
	request.frameGeneration = 7u;
	request.surfaceId = 11u;
	request.diffuseAuthority = RAL_LIGHTING_DIFFUSE_AUTHORITY_DIRECTIONAL_STATIC;
	request.availableTermMask = RAL_LIGHTING_TERM_DYNAMIC_DIRECT
		| RAL_LIGHTING_TERM_STATIC_INDIRECT | RAL_LIGHTING_TERM_SPECULAR_IBL;
	request.directionalStaticBound = qtrue;
	request.dynamicDirectBound = qtrue;
	request.specularIblBound = qtrue;
	request.staticProductDiffuseIndirectOnly = qtrue;
	return request;
}

static ralLightingRuntimePlan_t ModernPlan( void ) {
	ralLightingRuntimePlan_t plan;
	memset( &plan, 0, sizeof( plan ) );
	plan.schemaVersion = RAL_LIGHTING_RUNTIME_PLAN_SCHEMA_VERSION;
	plan.backendType = RAL_BACKEND_OPENGL;
	plan.frameGeneration = 7u;
	plan.artifactGeneration = 9u;
	plan.artifactHash = plan.manifestHash = 10u;
	plan.encoding = RAL_STATIC_LIGHTING_ENCODING_RGB9E5_OCT8;
	plan.planeCount = 2u;
	plan.staticDiffuseIndirectOnly = qtrue;
	plan.ready = qtrue;
	for ( uint32_t i = 0u; i < plan.planeCount; ++i ) {
		plan.planes[i].role = i ? RAL_LIGHTING_PAYLOAD_DIRECTION
			: RAL_LIGHTING_PAYLOAD_RADIANCE;
		plan.planes[i].texture.type = RAL_TEXTURE_2D_ARRAY;
		plan.planes[i].texture.format = i ? RAL_FORMAT_R8G8_UNORM
			: RAL_FORMAT_E5B9G9R9_UFLOAT;
		plan.planes[i].texture.width = 128u;
		plan.planes[i].texture.height = 128u;
		plan.planes[i].texture.depthOrArrayLayers = 3u;
		plan.planes[i].texture.mipLevels = 1u;
		plan.planes[i].texture.sampleCount = 1u;
		plan.planes[i].texture.usage = RAL_TEXTURE_USAGE_SAMPLED
			| RAL_TEXTURE_USAGE_TRANSFER_DST;
		plan.planes[i].texture.memory = RAL_MEMORY_DEVICE_LOCAL;
		plan.planes[i].texture.concurrentGraphicsTransfer = qtrue;
		plan.planes[i].artifactOffset = 240u + i * 64u;
		plan.planes[i].byteLength = 64u;
		plan.planes[i].payloadHash = 20u + i;
		plan.planes[i].tightBytesPerRow = i ? 256u : 512u;
		plan.planes[i].rowsPerImage = 128u;
	}
	return plan;
}

int main( void ) {
	ralLightingCompositionRequest_t request = Modern();
	ralLightingCompositionReceipt_t receipt, untouched;
	const int32_t terms[RAL_LIGHTING_COMPOSITION_TERM_COUNT][3] = {
		{ 1, 2, 3 }, { 10, 20, 30 }, { 100, 200, 300 }, { 1000, 2000, 3000 }
	};
	int32_t rgb[3] = { -1, -1, -1 };

	CHECK( Ral_LightingCompositionBuild( &request, &receipt ) );
	CHECK( receipt.activeTermMask == request.availableTermMask );
	CHECK( receipt.visibleTermMask == request.availableTermMask );
	CHECK( receipt.compositionWriteCount[0] == 1u );
	CHECK( receipt.compositionWriteCount[1] == 1u );
	CHECK( receipt.compositionWriteCount[2] == 0u );
	CHECK( receipt.compositionWriteCount[3] == 1u );
	CHECK( Ral_LightingCompositionEvaluateQ16( &receipt, terms, rgb ) );
	CHECK( rgb[0] == 1011 && rgb[1] == 2022 && rgb[2] == 3033 );

	request.debugView = RAL_LIGHTING_DEBUG_STATIC_INDIRECT;
	CHECK( Ral_LightingCompositionBuild( &request, &receipt ) );
	CHECK( receipt.visibleTermMask == RAL_LIGHTING_TERM_STATIC_INDIRECT );
	CHECK( Ral_LightingCompositionEvaluateQ16( &receipt, terms, rgb ) );
	CHECK( rgb[0] == 10 && rgb[1] == 20 && rgb[2] == 30 );
	request.debugView = RAL_LIGHTING_DEBUG_DYNAMIC_DIRECT;
	CHECK( Ral_LightingCompositionBuild( &request, &receipt ) );
	CHECK( receipt.visibleTermMask == RAL_LIGHTING_TERM_DYNAMIC_DIRECT );
	CHECK( Ral_LightingCompositionEvaluateQ16( &receipt, terms, rgb ) );
	CHECK( rgb[0] == 1 && rgb[1] == 2 && rgb[2] == 3 );
	request.debugView = RAL_LIGHTING_DEBUG_SPECULAR_IBL;
	CHECK( Ral_LightingCompositionBuild( &request, &receipt ) );
	CHECK( receipt.visibleTermMask == RAL_LIGHTING_TERM_SPECULAR_IBL );
	CHECK( Ral_LightingCompositionEvaluateQ16( &receipt, terms, rgb ) );
	CHECK( rgb[0] == 1000 && rgb[1] == 2000 && rgb[2] == 3000 );
	request.debugView = RAL_LIGHTING_DEBUG_LOCAL_SH;
	memset( &untouched, 0x5a, sizeof( untouched ) );
	CHECK( !Ral_LightingCompositionBuild( &request, &untouched ) );
	CHECK( ((const unsigned char *)&untouched)[0] == 0x5au );

	request = Modern();
	request.legacyLightmapBound = qtrue;
	CHECK( !Ral_LightingCompositionBuild( &request, &untouched ) );
	request = Modern();
	request.staticProductDiffuseIndirectOnly = qfalse;
	CHECK( !Ral_LightingCompositionBuild( &request, &untouched ) );

	request = Modern();
	request.diffuseAuthority = RAL_LIGHTING_DIFFUSE_AUTHORITY_LEGACY_LIGHTMAP;
	request.legacyLightmapBound = qtrue;
	request.directionalStaticBound = qfalse;
	request.staticProductDiffuseIndirectOnly = qfalse;
	CHECK( Ral_LightingCompositionBuild( &request, &receipt ) );

	request = Modern();
	request.diffuseAuthority = RAL_LIGHTING_DIFFUSE_AUTHORITY_LOCAL_SH;
	request.availableTermMask = RAL_LIGHTING_TERM_DYNAMIC_DIRECT
		| RAL_LIGHTING_TERM_LOCAL_SH | RAL_LIGHTING_TERM_SPECULAR_IBL;
	request.directionalStaticBound = qfalse;
	request.localShBound = qtrue;
	request.staticProductDiffuseIndirectOnly = qfalse;
	request.debugView = RAL_LIGHTING_DEBUG_LOCAL_SH;
	CHECK( Ral_LightingCompositionBuild( &request, &receipt ) );
	CHECK( receipt.visibleTermMask == RAL_LIGHTING_TERM_LOCAL_SH );
	CHECK( Ral_LightingCompositionEvaluateQ16( &receipt, terms, rgb ) );
	CHECK( rgb[0] == 100 && rgb[1] == 200 && rgb[2] == 300 );

	request = Modern();
	request.debugView = RAL_LIGHTING_DEBUG_FINAL;
	CHECK( Ral_LightingCompositionBuild( &request, &receipt ) );
	{
		int32_t overflow[RAL_LIGHTING_COMPOSITION_TERM_COUNT][3] = { { 0 } };
		overflow[0][0] = INT32_MAX;
		overflow[1][0] = 1;
		rgb[0] = rgb[1] = rgb[2] = -7;
		CHECK( !Ral_LightingCompositionEvaluateQ16( &receipt, overflow, rgb ) );
	CHECK( rgb[0] == -7 && rgb[1] == -7 && rgb[2] == -7 );
	}
	{
		ralLightingRuntimePlan_t plan = ModernPlan();
		ralLightingSurfaceBindingRequest_t binding = {
			RAL_LIGHTING_SURFACE_BINDING_SCHEMA_VERSION, 7u, 11u, 2, qtrue
		};
		ralLightingSurfaceBindingReceipt_t modern, legacy, guard;
		CHECK( Ral_LightingRuntimePlanValid( &plan ) );
		CHECK( Ral_LightingSurfaceBindingBuild( &binding, &plan, &modern ) );
		CHECK( modern.diffuseAuthority
			== RAL_LIGHTING_DIFFUSE_AUTHORITY_DIRECTIONAL_STATIC );
		CHECK( modern.arrayLayer == 2u && modern.radiancePlane == 0u
			&& modern.directionPlane == 1u );
		CHECK( modern.directionalStaticBound && !modern.legacyLightmapBound );
		CHECK( Ral_LightingSurfaceBindingReceiptExact( &modern, &modern ) );
		plan.frameGeneration = 5u;
		CHECK( Ral_LightingSurfaceBindingBuild( &binding, &plan, &modern ) );
		CHECK( Ral_LightingSurfaceBindingBuild( &binding, NULL, &legacy ) );
		CHECK( legacy.diffuseAuthority
			== RAL_LIGHTING_DIFFUSE_AUTHORITY_LEGACY_LIGHTMAP );
		CHECK( legacy.legacyLightmapBound && !legacy.directionalStaticBound );
		binding.lightmapIndex = 3;
		memset( &guard, 0x5a, sizeof( guard ) );
		CHECK( !Ral_LightingSurfaceBindingBuild( &binding, &plan, &guard ) );
		CHECK( ((const unsigned char *)&guard)[0] == 0x5au );
		binding.lightmapIndex = 1;
		binding.legacyLightmapAvailable = qfalse;
		CHECK( Ral_LightingSurfaceBindingBuild( &binding, &plan, &modern ) );
		CHECK( !Ral_LightingSurfaceBindingBuild( &binding, NULL, &guard ) );
		plan.planes[1].role = RAL_LIGHTING_PAYLOAD_RADIANCE;
		CHECK( !Ral_LightingSurfaceBindingBuild( &binding, &plan, &guard ) );
	}
	puts( "ral_lighting_composition_test: ok" );
	return 0;
}
