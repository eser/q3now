// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "ral_atmosphere_frame_graph.h"

#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL %s:%d: %s\n", \
	__FILE__, __LINE__, #x); return 1; } } while (0)

static ralAtmospherePlanReceipt_t Receipt( ralBackendType_t backend,
		qboolean clouds ) {
	ralAtmospherePlanRequest_t request;
	ralAtmospherePlanReceipt_t receipt;
	memset( &request, 0, sizeof( request ) );
	request.schemaVersion = RAL_ATMOSPHERE_PLAN_SCHEMA_VERSION;
	request.backendType = backend; request.frameGeneration = 1u;
	request.width = 1280u; request.height = 720u;
	request.requestedTier = RAL_ATMOSPHERE_TIER_FULL;
	request.maxFroxelCount = 262144u;
	request.maxLocalVolumes = 64u; request.maxLights = 256u;
	request.maxShadowedLights = 16u; request.mediaActive = qtrue;
	request.capabilities.analyticComposite = qtrue;
	request.capabilities.compute = qtrue;
	request.capabilities.storageBuffers = qtrue;
	request.capabilities.temporalHistory = qtrue;
	request.capabilities.volumetricShadows = qtrue;
	request.cloudsRequested = clouds;
	request.capabilities.fullClouds = clouds;
	if ( !Ral_AtmospherePlan( &request, &receipt ) )
		memset( &receipt, 0, sizeof( receipt ) );
	return receipt;
}

int main( void ) {
	for ( int backend = RAL_BACKEND_VULKAN; backend < RAL_BACKEND_COUNT; ++backend ) {
		ralAtmosphereFrameGraphRequest_t request, bad;
		ralFrameGraphPlan_t plan, exact, untouched, before;
		memset( &request, 0, sizeof( request ) );
		request.schemaVersion = RAL_ATMOSPHERE_FRAME_GRAPH_SCHEMA_VERSION;
		request.generation = 7u; request.resourceIdentityBase = 0x1000u;
		request.transientBudgetBytes = 16u * 1024u * 1024u;
		request.atmosphere = Receipt( (ralBackendType_t)backend, qtrue );
		CHECK( Ral_AtmosphereFrameGraphBuild( &request, &plan )
			&& plan.ready && plan.description.resourceCount == 9u
			&& plan.description.passCount == 5u
			&& plan.description.useCount == 13u
			&& plan.topologicalPassIds[0] == RAL_ATMOSPHERE_PASS_ID_MEDIA_INJECT
			&& plan.topologicalPassIds[1] == RAL_ATMOSPHERE_PASS_ID_LIGHT_INJECT
			&& plan.topologicalPassIds[2] == RAL_ATMOSPHERE_PASS_ID_CLOUD_INJECT
			&& plan.topologicalPassIds[3] == RAL_ATMOSPHERE_PASS_ID_INTEGRATE
			&& plan.topologicalPassIds[4] == RAL_ATMOSPHERE_PASS_ID_COMPOSITE
			&& plan.transientRequestCount == 4u
			&& plan.transientPlan.slotCount
				== ( backend == RAL_BACKEND_WEBGPU ? 4u : 2u ) );
		exact = plan;
		CHECK( RalFrameGraph_PlanExact( &plan, &exact ) );
		request.atmosphere = Receipt( (ralBackendType_t)backend, qfalse );
		CHECK( Ral_AtmosphereFrameGraphBuild( &request, &exact )
			&& exact.description.resourceCount == 8u
			&& exact.description.passCount == 4u
			&& exact.description.useCount == 11u
			&& exact.transientRequestCount == 3u );
		request.atmosphere = Receipt( (ralBackendType_t)backend, qtrue );
		memset( &untouched, 0x5a, sizeof( untouched ) ); before = untouched;
		bad = request; bad.atmosphere.selectedTier = RAL_ATMOSPHERE_TIER_WEATHER;
		CHECK( !Ral_AtmosphereFrameGraphBuild( &bad, &untouched )
			&& !memcmp( &untouched, &before, sizeof( untouched ) ) );
		bad = request; bad.transientBudgetBytes = request.atmosphere.froxelCount * 16u;
		CHECK( !Ral_AtmosphereFrameGraphBuild( &bad, &untouched )
			&& !memcmp( &untouched, &before, sizeof( untouched ) ) );
	}
	puts( "RAL atmosphere frame graph: PASS" );
	return 0;
}
