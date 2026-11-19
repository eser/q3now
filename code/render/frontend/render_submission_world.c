// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "render_submission.h"
#include "maps/map_format_registry.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define DISTANT_DRAW_FNV_OFFSET UINT64_C( 14695981039346656037 )
#define DISTANT_DRAW_FNV_PRIME  UINT64_C( 1099511628211 )

static uint64_t DistantDrawHashU32( uint64_t hash, uint32_t value ) {
	for ( uint32_t byteIndex = 0u; byteIndex < 4u; ++byteIndex ) {
		hash ^= (uint8_t)( value >> ( byteIndex * 8u ) );
		hash *= DISTANT_DRAW_FNV_PRIME;
	}
	return hash;
}

static uint64_t DistantDrawIdentity( uint64_t contentIdentity,
		const renderWorldBatch_t *batch, uint32_t fallbackIndex ) {
	uint64_t identity;
	if ( !contentIdentity || !batch ) return 0u;
	identity = DistantDrawHashU32( DISTANT_DRAW_FNV_OFFSET,
		(uint32_t)( contentIdentity ^ ( contentIdentity >> 32u ) ) );
	identity = DistantDrawHashU32( identity, batch->sourceSurfaceIndex );
	identity = DistantDrawHashU32( identity, batch->firstIndex );
	identity = DistantDrawHashU32( identity, batch->indexCount );
	identity = DistantDrawHashU32( identity, (uint32_t)batch->shaderIndex );
	if ( !identity || identity == UINT64_MAX )
		identity = (uint64_t)fallbackIndex + 1u;
	return identity;
}

static qboolean SubmissionFrameReadable( const renderSubmissionState_t *state ) {
	return state && state->initialized
		&& ( state->frameOpen || state->frameSealed ) ? qtrue : qfalse;
}

static uint64_t SubmissionReadableGeneration(
		const renderSubmissionState_t *state ) {
	uint64_t generation = 0u;
	return RenderSubmission_ReadableFrameGeneration( state, &generation )
		? generation : 0u;
}

qboolean RenderSubmission_WorldSnapshot( const renderSubmissionState_t *state,
		renderWorldSnapshot_t *outSnapshot ) {
	if ( !state || !outSnapshot || !state->initialized
			|| ( !state->frameOpen && !state->frameSealed )
			|| !state->worldSnapshot.ready || !state->sceneRendered ) return qfalse;
	*outSnapshot = state->worldSnapshot;
	return qtrue;
}

qboolean RenderSubmission_ViewSnapshot( const renderSubmissionState_t *state,
		renderWorldSnapshot_t *outSnapshot ) {
	if ( !state || !outSnapshot || !state->initialized
			|| ( !state->frameOpen && !state->frameSealed )
			|| !state->sceneRendered ) return qfalse;
	*outSnapshot = state->worldSnapshot;
	return qtrue;
}

qboolean RenderSubmission_ViewCameraCut( const renderSubmissionState_t *state ) {
	renderWorldSnapshot_t view;
	return RenderSubmission_ViewSnapshot( state, &view )
		&& ( view.rdflags & ( RDF_CAMERA_CUT | RDF_HYPERSPACE ) ) ? qtrue : qfalse;
}

qboolean RenderSubmission_ResidentWorldSnapshot(
		const renderSubmissionState_t *state, int worldIndex,
		uint64_t lifecycleGeneration, renderWorldSnapshot_t *outSnapshot ) {
	const renderSubmissionWorldSlot_t *slot;
	if ( !state || !state->initialized || !outSnapshot || !lifecycleGeneration
			|| worldIndex < 0 || worldIndex >= MAX_RENDER_WORLDS ) return qfalse;
	slot = &state->worlds[worldIndex];
	if ( !slot->loaded || !slot->snapshot.ready
			|| slot->lifecycleGeneration != lifecycleGeneration ) return qfalse;
	*outSnapshot = slot->snapshot;
	return qtrue;
}

qboolean RenderSubmission_WorldContentIdentity(
		const renderSubmissionState_t *state, int worldIndex,
		uint64_t *outIdentity ) {
	if ( !state || !state->initialized || !outIdentity || worldIndex < 0
			|| worldIndex >= MAX_RENDER_WORLDS
			|| !state->worlds[worldIndex].loaded
			|| !state->worlds[worldIndex].contentIdentity ) return qfalse;
	*outIdentity = state->worlds[worldIndex].contentIdentity;
	return qtrue;
}

qboolean RenderSubmission_WorldLifecycleGeneration(
		const renderSubmissionState_t *state, int worldIndex,
		uint64_t *outGeneration ) {
	if ( !state || !state->initialized || !outGeneration || worldIndex < 0
			|| worldIndex >= MAX_RENDER_WORLDS
			|| !state->worlds[worldIndex].loaded
			|| !state->worlds[worldIndex].lifecycleGeneration ) return qfalse;
	*outGeneration = state->worlds[worldIndex].lifecycleGeneration;
	return qtrue;
}

qboolean RenderSubmission_UpdateWorldVisibilityForView(
		renderSubmissionState_t *state, int worldIndex, const refdef_t *view ) {
	renderSubmissionWorldSlot_t *slot;
	const mapFile_t *bsp;
	renderWorldBatch_t *batches;
	const byte *clusterVisibility = NULL;
	int nodeIndex = 0, viewLeaf = -1, viewCluster = -1;
	if ( !state || !state->initialized || !view || worldIndex < 0
			|| worldIndex >= MAX_RENDER_WORLDS ) return qfalse;
	slot = &state->worlds[worldIndex];
	if ( !slot->loaded || !slot->map || !slot->snapshot.ready ) return qfalse;
	bsp = slot->map;
	batches = (renderWorldBatch_t *)slot->snapshot.batches;
	if ( slot->snapshot.batchCount && !batches ) return qfalse;
	for ( uint32_t batch = 0u; batch < slot->snapshot.batchCount; ++batch )
		batches[batch].visible = qtrue;
	if ( !slot->surfaceVisibility || !slot->surfaceVisibilityBytes
			|| bsp->numNodes <= 0 || !bsp->nodes || bsp->numLeafs <= 0
			|| !bsp->leafs || bsp->numLeafSurfaces <= 0 || !bsp->leafSurfaces )
		return qtrue;
	while ( nodeIndex >= 0 ) {
		const dnode_t *node;
		const dplane_t *plane;
		float distance;
		if ( nodeIndex >= bsp->numNodes ) return qfalse;
		node = &bsp->nodes[nodeIndex];
		if ( node->planeNum < 0 || node->planeNum >= bsp->numPlanes
				|| !bsp->planes ) return qfalse;
		plane = &bsp->planes[node->planeNum];
		distance = view->vieworg[0] * plane->normal[0]
			+ view->vieworg[1] * plane->normal[1]
			+ view->vieworg[2] * plane->normal[2] - plane->dist;
		nodeIndex = node->children[distance >= 0.0f ? 0 : 1];
	}
	viewLeaf = -1 - nodeIndex;
	if ( viewLeaf < 0 || viewLeaf >= bsp->numLeafs ) return qfalse;
	viewCluster = bsp->leafs[viewLeaf].cluster;
	if ( viewCluster >= 0 && viewCluster < bsp->numClusters
			&& bsp->visibility && bsp->clusterBytes > 0
			&& bsp->visibilityLength >= bsp->clusterBytes * bsp->numClusters )
		clusterVisibility = bsp->visibility
			+ (size_t)viewCluster * (size_t)bsp->clusterBytes;
	memset( slot->surfaceVisibility, 0, slot->surfaceVisibilityBytes );
	for ( int leafIndex = 0; leafIndex < bsp->numLeafs; ++leafIndex ) {
		const dleaf_t *leaf = &bsp->leafs[leafIndex];
		if ( leaf->firstLeafSurface < 0 || leaf->numLeafSurfaces < 0
				|| leaf->firstLeafSurface
					> bsp->numLeafSurfaces - leaf->numLeafSurfaces ) return qfalse;
		if ( leaf->area >= 0 && leaf->area < MAX_MAP_AREA_BYTES * 8
				&& ( view->areamask[leaf->area >> 3]
					& ( 1u << ( leaf->area & 7 ) ) ) ) continue;
		if ( clusterVisibility && ( leaf->cluster < 0
				|| leaf->cluster >= bsp->numClusters
				|| !( clusterVisibility[leaf->cluster >> 3]
					& ( 1u << ( leaf->cluster & 7 ) ) ) ) ) continue;
		for ( int offset = 0; offset < leaf->numLeafSurfaces; ++offset ) {
			int surface = bsp->leafSurfaces[leaf->firstLeafSurface + offset];
			if ( surface >= 0 && surface < bsp->numSurfaces )
				slot->surfaceVisibility[(uint32_t)surface >> 3]
					|= (byte)( 1u << ( (uint32_t)surface & 7u ) );
		}
	}
	for ( uint32_t batch = 0u; batch < slot->snapshot.batchCount; ++batch ) {
		uint32_t surface = batches[batch].sourceSurfaceIndex;
		batches[batch].visible = surface < (uint32_t)bsp->numSurfaces
			&& ( slot->surfaceVisibility[surface >> 3]
				& ( 1u << ( surface & 7u ) ) ) ? qtrue : qfalse;
	}
	return qtrue;
}

static qboolean DistantViewValid( const refdef_t *view ) {
	if ( !view || !isfinite( view->fov_x ) || !isfinite( view->fov_y )
			|| view->fov_x <= 1.0f || view->fov_x >= 179.0f
			|| view->fov_y <= 1.0f || view->fov_y >= 179.0f ) return qfalse;
	for ( uint32_t axis = 0u; axis < 3u; ++axis ) {
		float lengthSquared = 0.0f;
		if ( !isfinite( view->vieworg[axis] ) ) return qfalse;
		for ( uint32_t component = 0u; component < 3u; ++component ) {
			const float value = view->viewaxis[axis][component];
			if ( !isfinite( value ) ) return qfalse;
			lengthSquared += value * value;
		}
		if ( lengthSquared < 0.25f || lengthSquared > 4.0f ) return qfalse;
	}
	return qtrue;
}

qboolean RenderSubmission_DistantWorldBuildMetrics(
		renderSubmissionState_t *state, int worldIndex,
		const ralSkyEnvironmentPlanReceipt_t *skyEnvironment,
		const refdef_t *mainView, uint32_t lodLevelCount,
		renderDistantWorldDrawMetric_t *outMetrics, uint32_t metricCapacity,
		uint32_t *outMetricCount ) {
	renderSubmissionWorldSlot_t *slot;
	refdef_t distantView;
	float tanHalfX, tanHalfY, minTan;
	if ( !SubmissionFrameReadable( state ) || !skyEnvironment
			|| !outMetricCount || worldIndex < 0 || worldIndex >= MAX_RENDER_WORLDS
			|| worldIndex == state->activeWorldIndex || !DistantViewValid( mainView )
			|| !Ral_SkyEnvironmentPlanReceiptExact( skyEnvironment, skyEnvironment )
			|| !skyEnvironment->distantSceneActive
			|| lodLevelCount == 0u
			|| lodLevelCount > RAL_SKY_DISTANT_MAX_LOD_LEVELS ) return qfalse;
	slot = &state->worlds[worldIndex];
	if ( !slot->loaded || !slot->snapshot.ready || !slot->map
			|| !slot->contentIdentity
			|| slot->contentIdentity != skyEnvironment->distantSceneIdentity
			|| skyEnvironment->frameGeneration != SubmissionReadableGeneration( state )
			|| slot->snapshot.batchCount > RAL_SKY_DISTANT_MAX_CANDIDATES
			|| slot->snapshot.batchCount > metricCapacity
			|| ( slot->snapshot.batchCount && !outMetrics ) ) return qfalse;
	for ( uint32_t index = 0u; index < slot->snapshot.batchCount; ++index ) {
		const renderWorldBatch_t *batch = &slot->snapshot.batches[index];
		if ( !isfinite( batch->boundsCenter[0] )
				|| !isfinite( batch->boundsCenter[1] )
				|| !isfinite( batch->boundsCenter[2] )
				|| !isfinite( batch->boundsRadius ) || batch->boundsRadius < 0.0f )
			return qfalse;
	}
	distantView = *mainView;
	for ( uint32_t axis = 0u; axis < 3u; ++axis )
		distantView.vieworg[axis] = skyEnvironment->distantSceneOrigin[axis]
			+ mainView->vieworg[axis] / skyEnvironment->distantSceneScale;
	/* Main-world area portals do not describe a distinct distant BSP. Its own
	 * PVS remains active, but no unrelated main-world areamask is imported. */
	memset( distantView.areamask, 0, sizeof( distantView.areamask ) );
	if ( !RenderSubmission_UpdateWorldVisibilityForView(
			state, worldIndex, &distantView ) ) return qfalse;
	tanHalfX = tanf( distantView.fov_x * 0.008726646259971648f );
	tanHalfY = tanf( distantView.fov_y * 0.008726646259971648f );
	if ( !isfinite( tanHalfX ) || !isfinite( tanHalfY )
			|| tanHalfX <= 0.0f || tanHalfY <= 0.0f ) return qfalse;
	minTan = tanHalfX < tanHalfY ? tanHalfX : tanHalfY;
	for ( uint32_t index = 0u; index < slot->snapshot.batchCount; ++index ) {
		const renderWorldBatch_t *batch = &slot->snapshot.batches[index];
		renderDistantWorldDrawMetric_t metric;
		float delta[3], depth = 0.0f, horizontal = 0.0f, vertical = 0.0f;
		memset( &metric, 0, sizeof( metric ) );
		metric.lodLevelCount = lodLevelCount;
		for ( uint32_t axis = 0u; axis < 3u; ++axis )
			delta[axis] = batch->boundsCenter[axis] - distantView.vieworg[axis];
		for ( uint32_t axis = 0u; axis < 3u; ++axis ) {
			depth += delta[axis] * distantView.viewaxis[0][axis];
			horizontal += delta[axis] * distantView.viewaxis[1][axis];
			vertical += delta[axis] * distantView.viewaxis[2][axis];
		}
		if ( batch->visible && !batch->noDraw && !batch->sky
				&& depth + batch->boundsRadius > 0.001f
				&& fabsf( horizontal ) <= fmaxf( depth, 0.0f ) * tanHalfX
					+ batch->boundsRadius
				&& fabsf( vertical ) <= fmaxf( depth, 0.0f ) * tanHalfY
					+ batch->boundsRadius ) {
			float coverage;
			metric.visible = qtrue;
			if ( batch->boundsRadius <= 0.0f ) coverage = 1.0f / 65536.0f;
			else if ( depth <= batch->boundsRadius ) coverage = 1.0f;
			else {
				const float projectedRadius = batch->boundsRadius / ( depth * minTan );
				coverage = projectedRadius * projectedRadius;
				if ( coverage > 1.0f ) coverage = 1.0f;
				if ( coverage < 1.0f / 65536.0f ) coverage = 1.0f / 65536.0f;
			}
			metric.projectedCoverageQ16 = (uint32_t)ceilf( coverage * 65536.0f );
			if ( metric.projectedCoverageQ16 > 65536u )
				metric.projectedCoverageQ16 = 65536u;
		}
		outMetrics[index] = metric;
	}
	*outMetricCount = slot->snapshot.batchCount;
	return qtrue;
}

qboolean RenderSubmission_DistantWorldDrawPlan(
		const renderSubmissionState_t *state, int worldIndex,
		const ralSkyEnvironmentPlanReceipt_t *skyEnvironment,
		const renderDistantWorldDrawMetric_t *metrics, uint32_t metricCount,
		uint32_t maxVisibleDraws, ralSkyDistantDrawCandidate_t *candidateScratch,
		uint32_t candidateCapacity, ralSkyDistantDrawPlanReceipt_t *outReceipt ) {
	const renderSubmissionWorldSlot_t *slot;
	ralSkyDistantDrawPlanRequest_t request;
	if ( !SubmissionFrameReadable( state ) || !skyEnvironment
			|| !outReceipt || worldIndex < 0 || worldIndex >= MAX_RENDER_WORLDS
			|| worldIndex == state->activeWorldIndex ) return qfalse;
	slot = &state->worlds[worldIndex];
	if ( !slot->loaded || !slot->snapshot.ready || !slot->contentIdentity
			|| slot->contentIdentity != skyEnvironment->distantSceneIdentity
			|| skyEnvironment->frameGeneration != SubmissionReadableGeneration( state )
			|| slot->snapshot.batchCount != metricCount
			|| metricCount > RAL_SKY_DISTANT_MAX_CANDIDATES
			|| ( metricCount && ( !metrics || !candidateScratch ) )
			|| candidateCapacity < metricCount ) return qfalse;
	for ( uint32_t index = 0u; index < metricCount; ++index ) {
		const renderDistantWorldDrawMetric_t *metric = &metrics[index];
		if ( ( metric->visible != qfalse && metric->visible != qtrue )
				|| metric->projectedCoverageQ16 > 65536u
				|| ( metric->visible && !metric->projectedCoverageQ16 )
				|| !metric->lodLevelCount
				|| metric->lodLevelCount > RAL_SKY_DISTANT_MAX_LOD_LEVELS )
			return qfalse;
	}
	for ( uint32_t index = 0u; index < metricCount; ++index ) {
		const renderWorldBatch_t *batch = &slot->snapshot.batches[index];
		uint64_t identity = DistantDrawIdentity( slot->contentIdentity,
			batch, index );
		candidateScratch[index].drawIdentity = identity;
		candidateScratch[index].sourceDrawIndex = index;
		candidateScratch[index].projectedCoverageQ16 =
			metrics[index].projectedCoverageQ16;
		candidateScratch[index].lodLevelCount = metrics[index].lodLevelCount;
		candidateScratch[index].visible = metrics[index].visible &&
			batch->visible && !batch->noDraw && !batch->sky ? qtrue : qfalse;
		if ( !candidateScratch[index].visible )
			candidateScratch[index].projectedCoverageQ16 = 0u;
	}
	memset( &request, 0, sizeof( request ) );
	request.schemaVersion = RAL_SKY_DISTANT_DRAW_SCHEMA_VERSION;
	request.skyEnvironment = *skyEnvironment;
	request.candidates = candidateScratch;
	request.candidateCount = metricCount;
	request.maxVisibleDraws = maxVisibleDraws;
	return Ral_SkyDistantDrawPlan( &request, outReceipt );
}

qboolean RenderSubmission_DistantWorldSubmissionExact(
		const renderDistantWorldSubmission_t *a,
		const renderDistantWorldSubmission_t *b ) {
	const renderWorldSnapshot_t *world;
	if ( !a || !b || a->schemaVersion !=
			RENDER_DISTANT_WORLD_SUBMISSION_SCHEMA_VERSION
			|| !Ral_BackendTypeValid( a->backendType )
			|| !a->frameGeneration || a->frameGeneration == UINT64_MAX
			|| !a->contentIdentity || a->contentIdentity == UINT64_MAX
			|| !Ral_SkyDistantDrawPlanReceiptExact(
				&a->drawPlan, &a->drawPlan )
			|| a->drawPlan.backendType != a->backendType
			|| a->drawPlan.frameGeneration != a->frameGeneration
			|| a->drawPlan.distantSceneIdentity != a->contentIdentity
			|| a->ready != qtrue ) return qfalse;
	world = &a->world;
	if ( world->ready != qtrue || !world->vertices || !world->indices
			|| !world->batches || !world->vertexCount || !world->indexCount
			|| !world->batchCount
			|| world->vertexCount > RENDER_SUBMISSION_MAX_WORLD_VERTICES
			|| world->indexCount > RENDER_SUBMISSION_MAX_WORLD_INDICES
			|| world->batchCount > RENDER_SUBMISSION_MAX_WORLD_BATCHES
			|| a->drawPlan.candidateCount != world->batchCount ) return qfalse;
	for ( uint32_t index = 0u; index < a->drawPlan.admittedCount; ++index ) {
		const ralSkyDistantDrawSelection_t *selection =
			&a->drawPlan.selections[index];
		const renderWorldBatch_t *batch;
		if ( selection->sourceDrawIndex >= world->batchCount ) return qfalse;
		batch = &world->batches[selection->sourceDrawIndex];
		if ( !batch->indexCount || batch->indexCount % 3u
				|| batch->indexCount > world->indexCount
				|| batch->firstIndex > world->indexCount - batch->indexCount
				|| !batch->visible || batch->noDraw || batch->sky
				|| selection->drawIdentity != DistantDrawIdentity(
					a->contentIdentity, batch, selection->sourceDrawIndex ) )
			return qfalse;
	}
	if ( !DistantViewValid( &(refdef_t){
		.fov_x = world->fovX, .fov_y = world->fovY,
		.vieworg = { world->viewOrigin[0], world->viewOrigin[1],
			world->viewOrigin[2] },
		.viewaxis = {
			{ world->viewAxis[0][0], world->viewAxis[0][1], world->viewAxis[0][2] },
			{ world->viewAxis[1][0], world->viewAxis[1][1], world->viewAxis[1][2] },
			{ world->viewAxis[2][0], world->viewAxis[2][1], world->viewAxis[2][2] }
		}
	} ) ) return qfalse;
	return !memcmp( a, b, sizeof( *a ) ) ? qtrue : qfalse;
}

qboolean RenderSubmission_DistantWorldSubmissionBuild(
		const renderSubmissionState_t *state, int worldIndex,
		const refdef_t *mainView,
		const ralSkyDistantDrawPlanReceipt_t *drawPlan,
		renderDistantWorldSubmission_t *outSubmission ) {
	renderDistantWorldSubmission_t submission;
	const renderSubmissionWorldSlot_t *slot;
	if ( !SubmissionFrameReadable( state ) || !mainView
			|| !drawPlan || !outSubmission || worldIndex < 0
			|| worldIndex >= MAX_RENDER_WORLDS
			|| worldIndex == state->activeWorldIndex
			|| !DistantViewValid( mainView )
			|| !Ral_SkyDistantDrawPlanReceiptExact( drawPlan, drawPlan )
			|| drawPlan->frameGeneration != SubmissionReadableGeneration( state ) )
		return qfalse;
	slot = &state->worlds[worldIndex];
	if ( !slot->loaded || !slot->snapshot.ready || !slot->contentIdentity
			|| slot->contentIdentity != drawPlan->distantSceneIdentity
			|| slot->snapshot.batchCount != drawPlan->candidateCount ) return qfalse;
	memset( &submission, 0, sizeof( submission ) );
	submission.schemaVersion = RENDER_DISTANT_WORLD_SUBMISSION_SCHEMA_VERSION;
	submission.backendType = drawPlan->backendType;
	submission.frameGeneration = drawPlan->frameGeneration;
	submission.contentIdentity = slot->contentIdentity;
	submission.world = slot->snapshot;
	for ( uint32_t axis = 0u; axis < 3u; ++axis )
		submission.world.viewOrigin[axis] = drawPlan->distantSceneOrigin[axis]
			+ mainView->vieworg[axis] / drawPlan->distantSceneScale;
	memcpy( submission.world.viewAxis, mainView->viewaxis,
		sizeof( submission.world.viewAxis ) );
	submission.world.fovX = mainView->fov_x;
	submission.world.fovY = mainView->fov_y;
	submission.world.viewportX = mainView->x;
	submission.world.viewportY = mainView->y;
	submission.world.viewportWidth = mainView->width > 0
		? (uint32_t)mainView->width : 1u;
	submission.world.viewportHeight = mainView->height > 0
		? (uint32_t)mainView->height : 1u;
	submission.world.uiPrimitiveInsertionIndex = 0u;
	submission.world.rdflags = 0u;
	submission.world.timeMs = mainView->time;
	submission.drawPlan = *drawPlan;
	submission.ready = qtrue;
	if ( !RenderSubmission_DistantWorldSubmissionExact(
			&submission, &submission ) ) return qfalse;
	*outSubmission = submission;
	return qtrue;
}

qboolean RenderSubmission_DistantWorldSubmissionForActiveView(
		renderSubmissionState_t *state,
		const ralSkyEnvironmentPlanReceipt_t *skyEnvironment,
		uint32_t lodLevelCount, uint32_t maxVisibleDraws,
		renderDistantWorldDrawMetric_t *metricScratch, uint32_t metricCapacity,
		ralSkyDistantDrawCandidate_t *candidateScratch,
		uint32_t candidateCapacity,
		renderDistantWorldSubmission_t *outSubmission ) {
	renderDistantWorldSubmission_t submission;
	ralSkyDistantDrawPlanReceipt_t drawPlan;
	renderWorldSnapshot_t mainSnapshot;
	refdef_t mainView;
	uint32_t metricCount = 0u;
	int distantWorldIndex;
	if ( !SubmissionFrameReadable( state ) || !skyEnvironment || !outSubmission
			|| state->activeWorldIndex < 0
			|| state->activeWorldIndex >= MAX_RENDER_APP_WORLDS
			|| !Ral_SkyEnvironmentPlanReceiptExact(
				skyEnvironment, skyEnvironment )
			|| !skyEnvironment->distantSceneActive
			|| skyEnvironment->frameGeneration != SubmissionReadableGeneration( state )
			|| !RenderSubmission_ViewSnapshot( state, &mainSnapshot ) ) return qfalse;
	distantWorldIndex = RENDER_DISTANT_WORLD_BASE + state->activeWorldIndex;
	memset( &mainView, 0, sizeof( mainView ) );
	memcpy( mainView.vieworg, mainSnapshot.viewOrigin,
		sizeof( mainView.vieworg ) );
	memcpy( mainView.viewaxis, mainSnapshot.viewAxis,
		sizeof( mainView.viewaxis ) );
	mainView.fov_x = mainSnapshot.fovX;
	mainView.fov_y = mainSnapshot.fovY;
	mainView.x = mainSnapshot.viewportX;
	mainView.y = mainSnapshot.viewportY;
	mainView.width = (int)mainSnapshot.viewportWidth;
	mainView.height = (int)mainSnapshot.viewportHeight;
	mainView.rdflags = (int)mainSnapshot.rdflags;
	mainView.time = mainSnapshot.timeMs;
	if ( !RenderSubmission_DistantWorldBuildMetrics( state, distantWorldIndex,
			skyEnvironment, &mainView, lodLevelCount, metricScratch,
			metricCapacity, &metricCount )
			|| !RenderSubmission_DistantWorldDrawPlan( state, distantWorldIndex,
				skyEnvironment, metricScratch, metricCount, maxVisibleDraws,
				candidateScratch, candidateCapacity, &drawPlan )
			|| !RenderSubmission_DistantWorldSubmissionBuild( state,
				distantWorldIndex, &mainView, &drawPlan, &submission ) ) return qfalse;
	*outSubmission = submission;
	return qtrue;
}

qboolean RenderSubmission_LightmapMaterialName( char outName[MAX_QPATH],
		uint32_t checksum, int lightmapIndex ) {
	int written;
	if ( !outName || lightmapIndex < 0 ) return qfalse;
	written = snprintf( outName, MAX_QPATH, "*lightmap/%08x/%d",
		checksum, lightmapIndex );
	return written > 0 && written < MAX_QPATH ? qtrue : qfalse;
}
