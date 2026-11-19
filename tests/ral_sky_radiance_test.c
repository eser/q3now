// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "ral_sky_radiance.h"

#include <stdio.h>
#include <string.h>
#include <math.h>

#define CHECK( x ) do { if ( !( x ) ) { fprintf( stderr, "FAIL %s:%d: %s\n", \
	__FILE__, __LINE__, #x ); return 1; } } while ( 0 )

static ralSkyRadianceRequest_t Request( ralBackendType_t backend, uint64_t frame )
{
	ralSkyRadianceRequest_t request;
	memset( &request, 0, sizeof( request ) );
	request.schemaVersion = RAL_SKY_RADIANCE_SCHEMA_VERSION;
	request.backendType = backend;
	request.frameGeneration = frame;
	request.sourceRevision = 41u;
	request.mode = RAL_SKY_ENVIRONMENT_PHYSICAL;
	request.specularMipCount = 4u;
	request.maxSlicesPerFrame = 4u;
	request.deltaSeconds = 0.1f;
	request.crossfadeSeconds = 0.5f;
	return request;
}

static int ProductionMipChain( ralBackendType_t backend )
{
	ralSkyRadianceState_t state;
	ralSkyRadianceRequest_t request = Request( backend, 1u );
	ralSkyRadianceReceipt_t receipt;
	const uint32_t required = RAL_SKY_RADIANCE_CAPTURE_MASK
		| RAL_SKY_RADIANCE_AMBIENT_SH_MASK
		| ( ( ( 1u << WIRED_SKY_SPECULAR_MIP_COUNT ) - 1u ) << 15u );
	CHECK( WIRED_SKY_SOURCE_SIZE == 128u && WIRED_SKY_IRRADIANCE_SIZE == 32u );
	CHECK( WIRED_SKY_SPECULAR_SIZE == 128u && WIRED_SKY_SPECULAR_MIP_COUNT == 8u );
	CHECK( WIRED_SKY_SPECULAR_MIP_COUNT <= RAL_SKY_RADIANCE_MAX_SPECULAR_MIPS );
	CHECK( ( WIRED_SKY_SPECULAR_SIZE >> ( WIRED_SKY_SPECULAR_MIP_COUNT - 1u ) ) == 1u );
	request.specularMipCount = WIRED_SKY_SPECULAR_MIP_COUNT;
	CHECK( Ral_SkyRadianceInit( backend, &state ) );
	CHECK( Ral_SkyRadianceAdvance( &state, &request, &receipt ) );
	for ( unsigned step = 0u; !receipt.targetReady && step < 32u; ++step ) {
		CHECK( receipt.admittedSliceCount <= RAL_SKY_RADIANCE_MAX_SLICES_PER_FRAME );
		CHECK( !receipt.targetPublished && state.currentProductRevision == 0u );
		request.frameGeneration++;
		request.completedProductRevision = state.targetProductRevision;
		request.completedSliceMask = state.inFlightSliceMask;
		CHECK( Ral_SkyRadianceAdvance( &state, &request, &receipt ) );
	}
	CHECK( receipt.targetReady && receipt.targetPublished );
	CHECK( state.completedSliceMask == required );
	CHECK( ( state.completedSliceMask & ( 3u << 21u ) ) == ( 3u << 21u ) );
	return 0;
}

/* The adapters double-buffer SH/irradiance/specular: slot 1 is BOTH the
 * previous sampled product and the next writable target. A soft update must
 * not admit writes while that slot participates in a crossfade. */
static int CrossfadeOwnership( ralBackendType_t backend, uint32_t event )
{
	ralSkyRadianceState_t state;
	ralSkyRadianceRequest_t request = Request( backend, 1u );
	ralSkyRadianceReceipt_t receipt;
	ralSkyRadianceConsumerReceipt_t consumer;
	CHECK( Ral_SkyRadianceInit( backend, &state ) );
	for ( unsigned product = 0u; product < 2u; ++product ) {
		request.sourceRevision++;
		request.eventMask = RAL_SKY_EVENT_SOURCE_CHANGE;
		request.completedSliceMask = 0u;
		request.completedProductRevision = 0u;
		CHECK( Ral_SkyRadianceAdvance( &state, &request, &receipt ) );
		request.eventMask = 0u;
		for ( unsigned step = 0u; !receipt.targetReady && step < 32u; ++step ) {
			request.frameGeneration++;
			request.completedSliceMask = state.inFlightSliceMask;
			request.completedProductRevision = state.targetProductRevision;
			CHECK( Ral_SkyRadianceAdvance( &state, &request, &receipt ) );
		}
		CHECK( receipt.targetPublished && receipt.targetReady );
		request.frameGeneration++;
	}
	const uint64_t current = receipt.currentProductRevision;
	const uint64_t previous = receipt.previousProductRevision;
	CHECK( previous && receipt.crossfadeAlpha == 0.0f );
	request.completedSliceMask = 0u;
	request.completedProductRevision = 0u;
	request.deltaSeconds = 0.0f;
	/* A different requested duration belongs to the NEXT transition. */
	request.crossfadeSeconds = 0.0f;
	for ( unsigned change = 0u; change < 3u; ++change ) {
		request.sourceRevision++;
		request.eventMask = event | RAL_SKY_EVENT_SOURCE_CHANGE;
		CHECK( Ral_SkyRadianceAdvance( &state, &request, &receipt ) );
		CHECK( Ral_SkyRadianceReceiptValid( &receipt ) );
		CHECK( receipt.currentProductRevision == current
			&& receipt.previousProductRevision == previous
			&& receipt.crossfadeAlpha == 0.0f && !receipt.targetReady
			&& !receipt.admittedSliceMask && !receipt.inFlightSliceMask
			&& receipt.pendingSliceMask == receipt.requiredSliceMask );
		CHECK( Ral_SkyRadianceConsumerPlan( &receipt,
			RAL_SKY_RADIANCE_PRODUCT_ALL, RAL_SKY_RADIANCE_PRODUCT_ALL,
			&consumer ) && consumer.usePreviousProduct
			&& consumer.previousToCurrentBlend == 0.0f );
		request.frameGeneration++;
	}
	{
		ralSkyRadianceReceipt_t forged = receipt;
		forged.pendingSliceMask &= ~0x0fu;
		forged.pendingSliceCount -= 4u;
		forged.inFlightSliceMask = forged.admittedSliceMask = 0x0fu;
		forged.inFlightSliceCount = forged.admittedSliceCount = 4u;
		CHECK( !Ral_SkyRadianceReceiptValid( &forged ) );
	}
	/* Map/device invalidation is a hard boundary: it drops both sampled
	 * identities and may immediately reuse the target, even while paused. */
	const uint32_t hardEvents[] = { RAL_SKY_EVENT_MAP_TRANSITION,
		RAL_SKY_EVENT_DEVICE_RECREATE };
	for ( unsigned i = 0u; i < 2u; ++i ) {
		ralSkyRadianceState_t hardState = state;
		ralSkyRadianceRequest_t hardRequest = request;
		ralSkyRadianceReceipt_t hardReceipt;
		hardRequest.eventMask = hardEvents[i];
		CHECK( Ral_SkyRadianceAdvance( &hardState, &hardRequest, &hardReceipt ) );
		CHECK( !hardReceipt.currentProductRevision
			&& !hardReceipt.previousProductRevision
			&& hardReceipt.admittedSliceMask == 0x0fu
			&& Ral_SkyRadianceReceiptValid( &hardReceipt ) );
	}
	request.eventMask = 0u;
	request.deltaSeconds = 0.25f;
	CHECK( Ral_SkyRadianceAdvance( &state, &request, &receipt ) );
	CHECK( receipt.crossfadeAlpha == 0.5f && receipt.previousProductValid
		&& !receipt.admittedSliceMask && !receipt.inFlightSliceMask );
	request.frameGeneration++;
	CHECK( Ral_SkyRadianceAdvance( &state, &request, &receipt ) );
	CHECK( !receipt.previousProductValid && receipt.crossfadeAlpha == 1.0f
		&& receipt.currentProductRevision == current
		&& receipt.sourceRevision == request.sourceRevision
		&& receipt.admittedSliceMask == 0x0fu );
	/* Once the sampled previous product retires, the latest queued revision
	 * resumes under the unchanged four-slice budget and publishes normally. */
	for ( unsigned step = 0u; !receipt.targetReady && step < 32u; ++step ) {
		request.frameGeneration++;
		request.completedSliceMask = state.inFlightSliceMask;
		request.completedProductRevision = state.targetProductRevision;
		CHECK( Ral_SkyRadianceAdvance( &state, &request, &receipt ) );
		CHECK( receipt.admittedSliceCount <= 4u );
	}
	CHECK( receipt.targetPublished && receipt.targetReady && receipt.zeroWork
		&& receipt.currentProductRevision != current );
	return 0;
}

static int AnimatedRefresh( ralBackendType_t backend ) {
	ralSkyRadianceState_t state;
	ralSkyRadianceReceipt_t receipt, beforeReceipt;
	ralSkyRadianceRequest_t request = Request( backend, 1u );
	CHECK( Ral_SkyRadianceInit( backend, &state ) );
	request.refreshIntervalSeconds = 0.5f;
	request.maxSlicesPerFrame = 2u;
	request.deltaSeconds = 0.05f;
	CHECK( Ral_SkyRadianceAdvance( &state, &request, &receipt ) );
	const uint64_t initial = receipt.targetProductRevision;
	/* Slow GPU: elapsed refresh interval must never cancel this capture. */
	for ( unsigned frame = 0; frame < 50; ++frame ) {
		request.frameGeneration++;
		CHECK( Ral_SkyRadianceAdvance( &state, &request, &receipt ) );
		CHECK( !receipt.refreshStarted && !receipt.cancelledSliceMask
			&& receipt.targetProductRevision == initial );
	}
	unsigned refreshCount = 0, publicationCount = 0;
	for ( unsigned frame = 0; frame < 400; ++frame ) {
		request.frameGeneration++;
		request.completedSliceMask = state.inFlightSliceMask;
		request.completedProductRevision = state.targetProductRevision;
		CHECK( Ral_SkyRadianceAdvance( &state, &request, &receipt ) );
		CHECK( Ral_SkyRadianceReceiptValid( &receipt ) );
		CHECK( receipt.admittedSliceCount <= 2 && !receipt.cancelledSliceMask );
		if ( receipt.refreshStarted ) {
			refreshCount++;
			CHECK( !receipt.sourceChanged && !receipt.previousProductValid
				&& receipt.currentProductRevision && receipt.admittedSliceMask == 3u );
		}
		if ( receipt.targetPublished ) publicationCount++;
		/* A paused reader keeps its slot; no timed refresh, even after a stall. */
		if ( receipt.previousProductValid && receipt.crossfadeAlpha == 0.0f ) {
			const uint64_t current = receipt.currentProductRevision;
			request.deltaSeconds = 0;
			request.completedSliceMask = 0;
			for ( unsigned paused = 0; paused < 20; ++paused ) {
				request.frameGeneration++;
				CHECK( Ral_SkyRadianceAdvance( &state, &request, &receipt ) );
				CHECK( !receipt.refreshStarted && !receipt.admittedSliceMask
					&& receipt.currentProductRevision == current && receipt.crossfadeAlpha == 0.0f );
			}
			request.deltaSeconds = 0.05f;
		}
	}
	CHECK( refreshCount > 5 && publicationCount > 5 && refreshCount < 40 );
	/* Turning periodic work off drains any admitted cohort and stays idle. */
	request.refreshIntervalSeconds = 0;
	for ( unsigned frame = 0; frame < 100; ++frame ) {
		request.frameGeneration++;
		request.completedSliceMask = state.inFlightSliceMask;
		request.completedProductRevision = state.targetProductRevision;
		CHECK( Ral_SkyRadianceAdvance( &state, &request, &receipt ) );
		CHECK( !receipt.refreshStarted );
	}
	CHECK( receipt.targetReady && receipt.zeroWork && !receipt.previousProductValid );
	const float invalid[] = { -1, NAN, INFINITY };
	for ( unsigned i = 0; i < 3; ++i ) {
		const ralSkyRadianceState_t before = state;
		beforeReceipt = receipt;
		request.frameGeneration++;
		request.refreshIntervalSeconds = invalid[i];
		CHECK( !Ral_SkyRadianceAdvance( &state, &request, &receipt )
			&& !memcmp( &state, &before, sizeof( state ) )
			&& !memcmp( &receipt, &beforeReceipt, sizeof( receipt ) ) );
	}
	return 0;
}

int main( void )
{
	/* Q3 face order and camera handedness are shared by capture adapters. */
	const float forwards[6][3] = {
		{ 1, 0, 0 }, { 0, 1, 0 }, { -1, 0, 0 },
		{ 0, -1, 0 }, { 0, 0, 1 }, { 0, 0, -1 }
	};
	float axes[3][3], beforeAxes[3][3];
	for ( uint32_t face = 0; face < 6; ++face ) {
		CHECK( Ral_SkyRadianceCaptureAxes( face, axes ) );
		CHECK( !memcmp( axes[0], forwards[face], sizeof( forwards[face] ) ) );
		for ( unsigned i = 0; i < 3; ++i ) for ( unsigned j = 0; j < 3; ++j ) {
			float dot = 0;
			for ( unsigned c = 0; c < 3; ++c ) dot += axes[i][c] * axes[j][c];
			CHECK( dot == ( i == j ? 1.0f : 0.0f ) );
		}
		for ( unsigned c = 0; c < 3; ++c ) {
			unsigned a = ( c + 1 ) % 3, b = ( c + 2 ) % 3;
			CHECK( axes[0][a] * axes[1][b] - axes[0][b] * axes[1][a] == axes[2][c] );
		}
	}
	memcpy( beforeAxes, axes, sizeof( axes ) );
	CHECK( !Ral_SkyRadianceCaptureAxes( 6, axes )
		&& !memcmp( axes, beforeAxes, sizeof( axes ) ) );
	CHECK( !Ral_SkyRadianceCaptureAxes( UINT32_MAX, axes )
		&& !memcmp( axes, beforeAxes, sizeof( axes ) ) );
	CHECK( !Ral_SkyRadianceCaptureAxes( 0, NULL ) );
	const uint32_t softEvents[] = { RAL_SKY_EVENT_CAMERA_CUT,
		RAL_SKY_EVENT_SOURCE_CHANGE, RAL_SKY_EVENT_TIME_CHANGE,
		RAL_SKY_EVENT_WEATHER_CHANGE };
	for ( int backend = RAL_BACKEND_VULKAN; backend < RAL_BACKEND_COUNT; ++backend )
		CHECK( !AnimatedRefresh( (ralBackendType_t)backend ) );
	for ( int backend = RAL_BACKEND_VULKAN; backend < RAL_BACKEND_COUNT; ++backend )
		for ( unsigned event = 0u; event < sizeof( softEvents ) / sizeof( softEvents[0] ); ++event )
			CHECK( !CrossfadeOwnership( (ralBackendType_t)backend, softEvents[event] ) );
	/* The same validated upload payload is used by all four adapters.
	 * Clear weather is identity; only the sun lobe receives cloud attenuation. */
	for ( int backend = RAL_BACKEND_VULKAN; backend < RAL_BACKEND_COUNT; ++backend ) {
		ralSkyEnvironmentPlanRequest_t request;
		ralSkyEnvironmentPlanReceipt_t plan;
		ralSkyRadianceSourceParams_t params, before;
		const float coverage[] = { 0.0f, 0.5f, 1.0f };
		memset( &request, 0, sizeof( request ) );
		request.schemaVersion = RAL_SKY_ENVIRONMENT_SCHEMA_VERSION;
		request.backendType = (ralBackendType_t)backend;
		request.frameGeneration = 1u;
		request.sourceRevision = 1u;
		request.requestedTier = RAL_ATMOSPHERE_TIER_FULL;
		request.mode = RAL_SKY_ENVIRONMENT_PHYSICAL;
		request.capabilities.physicalLuts = qtrue;
		request.capabilities.radianceCapture = qtrue;
		request.capabilities.convolution = qtrue;
		request.sunDirection[2] = 1.0f;
		request.sunRadiance[0] = 2.0f;
		request.sunRadiance[1] = 1.0f;
		request.ambientSh[0] = 0.25f;
		request.skyLuminance = 0.75f;
		request.exposure = 1.25f;
		for ( unsigned strength = 0u; strength < 3u; ++strength ) {
			request.cloudSunOcclusion = coverage[strength];
			CHECK( Ral_SkyEnvironmentPlan( &request, &plan ) );
			for ( unsigned cover = 0u; cover < 3u; ++cover ) {
				for ( int cube = 0; cube < 3; ++cube ) {
					CHECK( Ral_SkyRadianceSourceParamsBuild( &plan, coverage[cover],
						(ralSkyRadianceSourceType_t)cube, &params ) );
					CHECK( sizeof( params ) == 64u && params.meta[0] == 0u
						&& params.meta[3] == (uint32_t)( cube != 0 ) );
					CHECK( params.sunDirectionIntensity[3]
						== ( cube == 2 ? 0.0f : 1.0f - coverage[cover] * coverage[strength] ) );
					CHECK( params.authoredRadiance[0] == 0.25f
						&& params.authoredRadiance[3] == 0.75f
						&& params.sunRadianceExposure[0] == ( cube == 2 ? 0.0f : 2.0f )
						&& params.sunRadianceExposure[3] == ( cube == 2 ? 1.0f : 1.25f ) );
				}
			}
		}
		before = params;
		const float invalid[] = { -0.01f, 1.01f, INFINITY, NAN };
		for ( unsigned i = 0u; i < sizeof( invalid ) / sizeof( invalid[0] ); ++i ) {
			CHECK( !Ral_SkyRadianceSourceParamsBuild( &plan, invalid[i], RAL_SKY_SOURCE_AUTHORED_FACES, &params )
				&& !memcmp( &params, &before, sizeof( params ) ) );
		}
		CHECK( !Ral_SkyRadianceSourceParamsBuild( NULL, 0.0f, RAL_SKY_SOURCE_AUTHORED_FACES, &params )
			&& !memcmp( &params, &before, sizeof( params ) ) );
		CHECK( !Ral_SkyRadianceSourceParamsBuild( &plan, 0.0f, (ralSkyRadianceSourceType_t)3, &params )
			&& !memcmp( &params, &before, sizeof( params ) ) );
	}
	for ( int backend = RAL_BACKEND_VULKAN; backend < RAL_BACKEND_COUNT; ++backend ) {
		CHECK( ProductionMipChain( (ralBackendType_t)backend ) == 0 );
		ralSkyRadianceState_t state;
		ralSkyRadianceRequest_t request = Request( (ralBackendType_t)backend,
			100u + (uint64_t)backend * 32u );
		ralSkyRadianceReceipt_t receipt, exact, before;
		ralSkyRadianceConsumerReceipt_t consumer, consumerExact, consumerBefore;
		ralSkyRadianceState_t stateBefore;
		uint64_t frame = request.frameGeneration;
		CHECK( Ral_SkyRadianceInit( (ralBackendType_t)backend, &state ) );
		CHECK( Ral_SkyRadianceAdvance( &state, &request, &receipt ) );
		CHECK( receipt.admittedSliceMask == 0x0fu &&
			receipt.admittedSliceCount == 4u &&
			receipt.pendingSliceCount == 15u &&
			receipt.inFlightSliceCount == 4u &&
			receipt.completedSliceCount == 0u &&
			( receipt.admittedSliceMask & ~RAL_SKY_RADIANCE_CAPTURE_MASK ) == 0u &&
			!receipt.targetReady && !receipt.targetPublished );
		exact = receipt;
		CHECK( Ral_SkyRadianceReceiptValid( &receipt ) );
		CHECK( Ral_SkyRadianceReceiptExact( &receipt, &exact ) );
		exact.admittedSliceMask ^= 1u;
		CHECK( !Ral_SkyRadianceReceiptExact( &receipt, &exact ) );
		exact = receipt;
		exact.crossfadeAlpha = 0.0f / 0.0f;
		CHECK( !Ral_SkyRadianceReceiptValid( &exact ) &&
			!Ral_SkyRadianceReceiptExact( &exact, &exact ) );
		exact = receipt;
		exact.pendingSliceMask |= exact.inFlightSliceMask;
		CHECK( !Ral_SkyRadianceReceiptValid( &exact ) &&
			!Ral_SkyRadianceReceiptExact( &exact, &exact ) );

		request.frameGeneration = ++frame;
		request.completedProductRevision = state.targetProductRevision;
		request.completedSliceMask = state.inFlightSliceMask;
		CHECK( Ral_SkyRadianceAdvance( &state, &request, &receipt ) &&
			receipt.acceptedCompletionMask == 0x0fu &&
			receipt.acceptedCompletionCount == 4u &&
			receipt.admittedSliceMask == 0x30u &&
			( receipt.admittedSliceMask & ~RAL_SKY_RADIANCE_CAPTURE_MASK ) == 0u );
		request.frameGeneration = ++frame;
		request.completedSliceMask = state.inFlightSliceMask;
		CHECK( Ral_SkyRadianceAdvance( &state, &request, &receipt ) &&
			( receipt.completedSliceMask & RAL_SKY_RADIANCE_CAPTURE_MASK ) ==
				RAL_SKY_RADIANCE_CAPTURE_MASK &&
			receipt.admittedSliceMask == ( 0x0fu << 6u ) );

		while ( !state.targetReady ) {
			request.frameGeneration = ++frame;
			request.completedProductRevision = state.targetProductRevision;
			request.completedSliceMask = state.inFlightSliceMask;
			CHECK( Ral_SkyRadianceAdvance( &state, &request, &receipt ) );
		}
		CHECK( receipt.targetPublished && receipt.targetReady && receipt.zeroWork &&
			state.currentProductRevision != 0u && state.pendingSliceMask == 0u );
		CHECK( Ral_SkyRadianceConsumerPlan( &receipt,
			RAL_SKY_RADIANCE_PRODUCT_ALL, 0u,
			&consumer ) && consumer.ready && consumer.currentProductReadable &&
			!consumer.previousProductReadable && !consumer.usePreviousProduct &&
			consumer.previousToCurrentBlend == 1.0f &&
			consumer.currentProductRevision == receipt.currentProductRevision );
		consumerExact = consumer;
		CHECK( Ral_SkyRadianceConsumerReceiptExact( &consumer,
			&consumerExact ) );
		consumerBefore = consumer;
		/* Identical admission on every backend: a completed product is not
		 * permission to light OFF/analytic frames. Withdrawing/re-enabling
		 * must leave its immutable completion identity untouched. */
		for ( unsigned tier = RAL_ATMOSPHERE_TIER_OFF;
				tier <= RAL_ATMOSPHERE_TIER_FULL; ++tier ) {
			ralSkyTierBudgetReceipt_t budget, corrupt;
			ralSkyRadianceConsumerReceipt_t admitted, empty = {0}, invalid;
			CHECK( Ral_SkyTierBudget( (ralAtmosphereTier_t)tier, &budget ) );
			CHECK( Ral_SkyRadianceConsumerAdmit( &budget, &consumer, &admitted ) );
			CHECK( !memcmp( &admitted, tier >= RAL_ATMOSPHERE_TIER_WEATHER
				? &consumer : &empty, sizeof( admitted ) ) );
			CHECK( !memcmp( &consumer, &consumerBefore, sizeof( consumer ) ) );
			CHECK( Ral_SkyRadianceConsumerAdmit( &budget, &admitted, &admitted ) );
			invalid = admitted; invalid.ready = (qboolean)2;
			CHECK( !Ral_SkyRadianceConsumerAdmit( &budget, &invalid, &admitted ) );
			corrupt = budget; corrupt.maxRadianceSlicesPerFrame++;
			invalid = admitted;
			CHECK( !Ral_SkyRadianceConsumerAdmit( &corrupt, &consumer, &admitted )
				&& !memcmp( &admitted, &invalid, sizeof( admitted ) ) );
			CHECK( Ral_SkyRadianceConsumerAdmit( &budget, &empty, &admitted )
				&& !memcmp( &admitted, &empty, sizeof( admitted ) ) );
		}
		CHECK( consumer.requiredProductMask == RAL_SKY_RADIANCE_PRODUCT_ALL &&
			consumer.currentReadableProductMask ==
				RAL_SKY_RADIANCE_PRODUCT_ALL &&
			consumer.previousReadableProductMask == 0u );
		CHECK( !Ral_SkyRadianceConsumerPlan( &receipt,
			RAL_SKY_RADIANCE_PRODUCT_ALL &
				~RAL_SKY_RADIANCE_PRODUCT_AMBIENT_SH, 0u,
			&consumer ) && !memcmp( &consumer, &consumerBefore,
				sizeof( consumer ) ) );
		CHECK( !Ral_SkyRadianceConsumerPlan( &receipt,
			RAL_SKY_RADIANCE_PRODUCT_ALL, RAL_SKY_RADIANCE_PRODUCT_ALL,
			&consumer ) && !memcmp( &consumer, &consumerBefore,
				sizeof( consumer ) ) );

		request.frameGeneration = ++frame;
		request.sourceRevision = 42u;
		request.eventMask = RAL_SKY_EVENT_SOURCE_CHANGE;
		request.completedSliceMask = 0u;
		request.completedProductRevision = 0u;
		CHECK( Ral_SkyRadianceAdvance( &state, &request, &receipt ) &&
			receipt.sourceChanged && state.currentProductRevision != 0u &&
			!state.targetReady && receipt.previousProductRevision == 0u );
		while ( !state.targetReady ) {
			request.frameGeneration = ++frame;
			request.eventMask = 0u;
			request.completedProductRevision = state.targetProductRevision;
			request.completedSliceMask = state.inFlightSliceMask;
			CHECK( Ral_SkyRadianceAdvance( &state, &request, &receipt ) );
		}
		CHECK( receipt.targetPublished && receipt.previousProductValid &&
			receipt.crossfadeAlpha == 0.0f );
		CHECK( Ral_SkyRadianceConsumerPlan( &receipt,
			RAL_SKY_RADIANCE_PRODUCT_ALL, RAL_SKY_RADIANCE_PRODUCT_ALL,
			&consumer ) && consumer.currentProductReadable &&
			consumer.previousProductReadable && consumer.usePreviousProduct &&
			consumer.previousToCurrentBlend == 0.0f &&
			consumer.previousProductRevision ==
				receipt.previousProductRevision );
		consumerBefore = consumer;
		CHECK( !Ral_SkyRadianceConsumerPlan( &receipt,
			RAL_SKY_RADIANCE_PRODUCT_ALL,
			RAL_SKY_RADIANCE_PRODUCT_ALL &
				~RAL_SKY_RADIANCE_PRODUCT_SPECULAR_RADIANCE,
			&consumer ) && !memcmp( &consumer, &consumerBefore,
				sizeof( consumer ) ) );
		consumerExact = consumer;
		consumerExact.previousToCurrentBlend = 2.0f;
		CHECK( !Ral_SkyRadianceConsumerReceiptExact( &consumer,
			&consumerExact ) );
		request.frameGeneration = ++frame;
		request.completedSliceMask = 0u;
		request.completedProductRevision = 0u;
		CHECK( Ral_SkyRadianceAdvance( &state, &request, &receipt ) &&
			receipt.previousProductValid && receipt.crossfadeAlpha > 0.19f &&
			receipt.crossfadeAlpha < 0.21f );
		{
			const uint64_t currentRevision = state.currentProductRevision;
			const uint64_t oldTargetRevision = state.targetProductRevision;
			request.frameGeneration = ++frame;
			request.eventMask = RAL_SKY_EVENT_CAMERA_CUT;
			request.deltaSeconds = 0.0f;
			CHECK( Ral_SkyRadianceAdvance( &state, &request, &receipt ) &&
				!receipt.sourceChanged &&
				state.currentProductRevision == currentRevision &&
				state.targetProductRevision != oldTargetRevision &&
				!state.targetReady && receipt.previousProductValid &&
				receipt.crossfadeAlpha > 0.19f &&
				receipt.crossfadeAlpha < 0.21f &&
				receipt.admittedSliceMask == 0u );
			/* Finish the old transition before exercising in-flight cancellation. */
			request.deltaSeconds = 1.0f;
		}

		request.frameGeneration = ++frame;
		request.sourceRevision = 43u;
		request.eventMask = RAL_SKY_EVENT_SOURCE_CHANGE;
		CHECK( Ral_SkyRadianceAdvance( &state, &request, &receipt ) );
		{
			const uint64_t staleRevision = state.targetProductRevision;
			const uint32_t staleMask = state.inFlightSliceMask;
			request.frameGeneration = ++frame;
			request.sourceRevision = 44u;
			request.eventMask = RAL_SKY_EVENT_SOURCE_CHANGE;
			request.completedProductRevision = staleRevision;
			request.completedSliceMask = staleMask;
			CHECK( Ral_SkyRadianceAdvance( &state, &request, &receipt ) &&
				receipt.staleCompletionMask == staleMask &&
				receipt.staleCompletionCount == 4u &&
				receipt.cancelledSliceMask == staleMask &&
				receipt.cancelledSliceCount == 4u &&
				state.completedSliceMask == 0u );
		}
		request.frameGeneration = ++frame;
		request.eventMask = RAL_SKY_EVENT_MAP_TRANSITION;
		request.completedSliceMask = 0u;
		request.completedProductRevision = 0u;
		CHECK( Ral_SkyRadianceAdvance( &state, &request, &receipt ) &&
			state.currentProductRevision == 0u && state.previousProductRevision == 0u );
		request.frameGeneration = ++frame;
		request.eventMask = RAL_SKY_EVENT_DEVICE_RECREATE;
		CHECK( Ral_SkyRadianceAdvance( &state, &request, &receipt ) &&
			state.currentProductRevision == 0u && state.previousProductRevision == 0u &&
			receipt.cancelledSliceMask != 0u );

		before = receipt;
		stateBefore = state;
		request.eventMask = 0u;
		request.frameGeneration = state.lastFrameGeneration;
		CHECK( !Ral_SkyRadianceAdvance( &state, &request, &receipt ) &&
			!memcmp( &state, &stateBefore, sizeof( state ) ) &&
			!memcmp( &receipt, &before, sizeof( receipt ) ) );
		request.frameGeneration = ++frame;
		request.eventMask = 0u;
		request.maxSlicesPerFrame = RAL_SKY_RADIANCE_MAX_SLICES_PER_FRAME + 1u;
		CHECK( !Ral_SkyRadianceAdvance( &state, &request, &receipt ) &&
			!memcmp( &receipt, &before, sizeof( receipt ) ) );
	}
	return 0;
}
