// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "ral_texture_stream.h"

#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if ( !(x) ) { fprintf( stderr, "FAIL %s:%d: %s\n", \
	__FILE__, __LINE__, #x ); return 1; } } while ( 0 )

static int Fixture( ralTextureCacheKey_t *key, ralTextureUploadPlan_t *plan ) {
	unsigned char payload[336], artifact[1024];
	const uint64_t levels[3] = { 256u, 64u, 16u };
	ralTextureAssetRequest_t request;
	ralTextureAssetReceipt_t asset;
	ralTextureArtifactReceipt_t artifactReceipt;
	uint32_t i;
	memset( &request, 0, sizeof( request ) );
	request.schemaVersion = RAL_TEXTURE_ASSET_SCHEMA_VERSION;
	request.assetGeneration = 17u; request.provenanceHash = 0x20277u;
	request.dimension = RAL_TEXTURE_ASSET_2D;
	request.width = request.height = 8u; request.depth = request.layers = 1u;
	request.sourceMipLevels = 3u; request.colorEncoding = RAL_TEXTURE_ENCODING_SRGB;
	request.channelSemantic = RAL_TEXTURE_CHANNEL_COLOR;
	request.sourceEncoding = RAL_TEXTURE_SOURCE_UNCOMPRESSED;
	request.mipPolicy = RAL_TEXTURE_MIPS_SOURCE;
	request.residency = RAL_TEXTURE_RESIDENCY_STREAMED;
	request.preferenceCount = 1u;
	request.preferences[0] = RAL_TEXTURE_COMPRESSION_UNCOMPRESSED;
	request.allowUncompressedFallback = qtrue;
	for ( i = 0u; i < sizeof( payload ); i++ ) payload[i] = (unsigned char)( i * 13u );
	return Ral_ResolveTextureAsset( &request, &asset )
		&& Ral_EncodeTextureArtifact( &asset, levels, 3u, payload, sizeof( payload ),
			artifact, sizeof( artifact ), &artifactReceipt )
		&& Ral_BuildTextureArtifactUploadPlan( artifact,
			artifactReceipt.containerByteLength, &asset, plan )
		&& Ral_TextureCacheKeyBuild( &asset, key );
}

static ralTextureResourceReceipt_t Resource( const ralTextureUploadPlan_t *plan,
		ralBackendType_t backend, uintptr_t identity, uint64_t generation ) {
	ralTextureResourceReceipt_t r;
	memset( &r, 0, sizeof( r ) );
	r.schemaVersion = RAL_TEXTURE_RESOURCE_RECEIPT_SCHEMA_VERSION;
	r.backendType = backend; r.textureIdentity = identity;
	r.resourceGeneration = generation; r.type = plan->texture.type;
	r.format = plan->texture.format; r.usage = plan->texture.usage;
	r.width = plan->texture.width; r.height = plan->texture.height;
	r.mipLevels = plan->levelCount; r.arrayLayers = 1u; r.ready = qtrue;
	return r;
}

static int Complete( ralTextureStreamCohort_t *c, uint32_t mip,
		uint32_t beginSerial, uint32_t completeSerial,
		ralTextureStreamUploadReceipt_t *out ) {
	ralTextureStreamUploadReceipt_t begun;
	return Ral_TextureStreamUploadBegin( c, mip, beginSerial, &begun )
		&& Ral_TextureStreamUploadComplete( c, &begun, 1u, completeSerial, out );
}

int main( void ) {
	ralTextureCacheKey_t key, sameKey;
	ralTextureUploadPlan_t plan, samePlan;
	ralTextureStreamCohort_t cohort, before;
	ralTextureResourceReceipt_t resource, badResource;
	ralTextureStreamUploadReceipt_t begun, completed, hostile, beforeOutput;
	uint64_t evicted = UINT64_MAX;
	uint32_t i;
	CHECK( Fixture( &key, &plan ) ); sameKey = key; samePlan = plan;
	memset( &cohort, 0xA5, sizeof( cohort ) );
	CHECK( Ral_TextureStreamCohortInit( &cohort, &key, &plan, 1u, 1u ) );
	CHECK( Ral_TextureStreamCohortValid( &cohort ) && cohort.mipCount == 3u );
	CHECK( cohort.mips[2].id.level == 0u && cohort.mips[1].id.level == 1u
		&& cohort.mips[0].id.level == 2u );
	resource = Resource( &plan, RAL_BACKEND_VULKAN, (uintptr_t)0x1000u, 3u );
	badResource = resource; badResource.mipLevels--;
	before = cohort; CHECK( !Ral_TextureStreamBindResource( &cohort, &badResource )
		&& !memcmp( &cohort, &before, sizeof( cohort ) ) );
	CHECK( Ral_TextureStreamBindResource( &cohort, &resource ) );

	/* A fine mip may be copied early, but cannot become visible without its
	 * direct coarser safety ancestor. Failed publication is output-atomic. */
	CHECK( Ral_TextureStreamUploadBegin( &cohort, 0u, 2u, &begun ) );
	before = cohort; memset( &completed, 0xCC, sizeof( completed ) );
	beforeOutput = completed;
	CHECK( !Ral_TextureStreamUploadComplete( &cohort, &begun, 1u, 3u, &completed )
		&& !memcmp( &cohort, &before, sizeof( cohort ) )
		&& !memcmp( &completed, &beforeOutput, sizeof( completed ) ) );
	CHECK( Ral_TextureStreamUploadCancel( &cohort, &begun, 3u ) );

	CHECK( Ral_TextureStreamUploadBegin( &cohort, 2u, 4u, &begun ) );
	before = cohort; memset( &completed, 0xD1, sizeof( completed ) );
	beforeOutput = completed;
	CHECK( !Ral_TextureStreamUploadComplete( &cohort, &begun, 0u, 5u, &completed )
		&& !memcmp( &cohort, &before, sizeof( cohort ) )
		&& !memcmp( &completed, &beforeOutput, sizeof( completed ) ) );
	hostile = begun; hostile.resource.resourceGeneration++;
	CHECK( !Ral_TextureStreamUploadComplete( &cohort, &hostile, 1u, 5u, &completed )
		&& !memcmp( &cohort, &before, sizeof( cohort ) ) );
	CHECK( Ral_TextureStreamUploadComplete( &cohort, &begun, 1u, 5u, &completed )
		&& completed.imageCount == plan.levels[2].imageCount
		&& completed.payloadByteLength == plan.levels[2].artifactLength );
	before = cohort;
	CHECK( !Ral_TextureStreamUploadComplete( &cohort, &begun, 1u, 6u, &hostile )
		&& !memcmp( &cohort, &before, sizeof( cohort ) ) );
	CHECK( Complete( &cohort, 1u, 6u, 7u, &completed ) );
	CHECK( Complete( &cohort, 0u, 8u, 9u, &completed ) );

	CHECK( Ral_TextureStreamApplyPressure( &cohort, 1u, 10u, &evicted )
		&& evicted == 320u && cohort.mips[0].state == RAL_RESIDENCY_ABSENT
		&& cohort.mips[1].state == RAL_RESIDENCY_ABSENT
		&& cohort.mips[2].state == RAL_RESIDENCY_RESIDENT );
	CHECK( Ral_TextureStreamRequestMip( &cohort, 1u, 11u )
		&& Ral_TextureStreamUploadBegin( &cohort, 1u, 12u, &begun ) );

	/* Device loss drops only physical authority. Cache identity and immutable
	 * upload facts survive, and every mip is requested for deterministic replay. */
	CHECK( Ral_TextureStreamInvalidateDevice( &cohort, 2u, 13u )
		&& !cohort.resourceBound && cohort.streamGeneration == 2u
		&& Ral_TextureCacheKeyExact( &cohort.cacheKey, &sameKey )
		&& Ral_TextureUploadPlanExact( &cohort.uploadPlan, &samePlan ) );
	for ( i = 0u; i < cohort.mipCount; i++ )
		CHECK( cohort.mips[i].state == RAL_RESIDENCY_REQUESTED );
	memset( &hostile, 0xEE, sizeof( hostile ) ); beforeOutput = hostile;
	CHECK( !Ral_TextureStreamUploadComplete( &cohort, &begun, 1u, 14u, &hostile )
		&& !memcmp( &hostile, &beforeOutput, sizeof( hostile ) ) );
	resource = Resource( &plan, RAL_BACKEND_WEBGPU, (uintptr_t)0x2000u, 1u );
	CHECK( Ral_TextureStreamBindResource( &cohort, &resource )
		&& Complete( &cohort, 2u, 14u, 15u, &completed )
		&& completed.streamGeneration == 2u
		&& completed.resource.backendType == RAL_BACKEND_WEBGPU );
	puts( "ral_texture_stream_test: ok" );
	return 0;
}
