// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "tr_local.h"
#include "tr_temporal_projection.h"
#include "tr_temporal_history.h"
#include "vk_ral_textures.h"
#include "../renderer/ral/ral_temporal.h"
#include "../renderercommon/r_log.h"

#include <string.h>
#include <stdio.h>

R_LOG_DECLARE_CHANNEL( rch_temporal, "renderer.temporal" );

typedef struct {
	qboolean valid;
	qboolean queued;
	qboolean committed;
	uint32_t width;
	uint32_t height;
	float projectionBefore[2];
	float projectionAfter[2];
	float ndcOffset[2];
	uint32_t resourceGeneration;
	qboolean resourcesReady;
	qboolean historyRecorded;
	qboolean previousSlotRead;
	uint32_t entityReceiptAttempts;
	uint32_t entityReceiptScans;
	uint32_t entityDrawSurfs;
	uint32_t entityVisibleTemporal;
	uint32_t entityAccepted;
	uint32_t entityPrevious;
	uint32_t entityRejected;
	qboolean entityReceiptsRecorded;
	qboolean entityCommitted;
	temporalEntityPoseReceipt_t entitySample;
	temporalCameraPoseReceipt_t cameraReceipt;
	ralTemporalFramePlan_t plan;
} temporalProjectionDiagnostic_t;

static ralTemporalState_t s_temporalStates[ MAX_RENDER_WORLDS ];
static uint32_t s_temporalTopologyEpochs[ MAX_RENDER_WORLDS ];
static temporalProjectionDiagnostic_t s_temporalDiagnostics[ MAX_RENDER_WORLDS ];
static temporalHistoryResources_t s_temporalHistory[ MAX_RENDER_WORLDS ];
static qboolean s_temporalCameraCutPending[ MAX_RENDER_WORLDS ];
static int s_temporalBackendWorld = -1;
static uint64_t s_temporalBackendFrame;

void R_TemporalHistoryShutdown( void ) {
	R_TemporalEntityCacheResetAll();
	memset( s_temporalCameraCutPending, 0, sizeof( s_temporalCameraCutPending ) );
	for ( int i = 0; i < MAX_RENDER_WORLDS; ++i ) {
		R_TemporalHistoryRelease( &s_temporalHistory[i] );
		R_TemporalHistoryInit( &s_temporalHistory[i] );
	}
}

void R_TemporalMarkCameraCut( int worldIndex ) {
	if ( worldIndex < 0 || worldIndex >= MAX_RENDER_WORLDS ) worldIndex = 0;
	s_temporalCameraCutPending[worldIndex] = qtrue;
}

void R_TemporalWorldLoaded( int worldIndex ) {
	if ( worldIndex < 0 || worldIndex >= MAX_RENDER_WORLDS ) {
		worldIndex = 0;
	}
	if ( s_temporalStates[ worldIndex ].pending ) {
		Ral_TemporalCancelFrame( &s_temporalStates[ worldIndex ],
			s_temporalStates[ worldIndex ].pendingPlan.frameId );
	}
	vk_temporal_history_store_shutdown();
	R_TemporalHistoryRelease( &s_temporalHistory[ worldIndex ] );
	if ( s_temporalTopologyEpochs[ worldIndex ] == UINT32_MAX ) {
		Ral_TemporalInit( &s_temporalStates[ worldIndex ] );
		s_temporalTopologyEpochs[ worldIndex ] = 1u;
	} else {
		s_temporalTopologyEpochs[ worldIndex ]++;
		if ( !s_temporalTopologyEpochs[ worldIndex ] ) {
			s_temporalTopologyEpochs[ worldIndex ] = 1u;
		}
	}
	memset( &s_temporalDiagnostics[ worldIndex ], 0,
		sizeof( s_temporalDiagnostics[ worldIndex ] ) );
	s_temporalCameraCutPending[worldIndex] = qfalse;
	R_TemporalEntityCacheResetWorld( worldIndex );
}

uint64_t R_TemporalProjectionPrepare( viewParms_t *view ) {
	ralTemporalFrameInput_t input;
	ralTemporalFramePlan_t plan;
	temporalProjectionDiagnostic_t diagnostic;
	ralTemporalState_t *state;
	uint64_t frameId;
	int worldIndex;

	if ( !view || !( tr.refdef.rdflags & RDF_TEMPORAL_PRIMARY )
			|| view->portalView != PV_NONE
			|| ( tr.refdef.rdflags & RDF_NOWORLDMODEL )
			|| view->stereoFrame != STEREO_CENTER
			|| view->viewportX != 0 || view->viewportY != 0
			|| view->viewportWidth != glConfig.vidWidth
			|| view->viewportHeight != glConfig.vidHeight ) {
		return 0;
	}
	worldIndex = view->temporalWorldIndex;
	if ( worldIndex < 0 || worldIndex >= MAX_RENDER_WORLDS ) {
		return 0;
	}
	state = &s_temporalStates[ worldIndex ];
	if ( ( !r_temporalInputTest || !r_temporalInputTest->integer )
			&& !state->active ) {
		return 0;
	}

	frameId = (uint64_t)(uint32_t)tr.frameCount + 1u;
	if ( state->pending || ( state->lastCommittedFrame
			&& frameId <= state->lastCommittedFrame ) ) {
		return 0;
	}

	memset( &input, 0, sizeof( input ) );
	input.frameId = frameId;
	input.width = (uint32_t)view->viewportWidth;
	input.height = (uint32_t)view->viewportHeight;
	input.viewId = (uint32_t)worldIndex + 1u;
	input.topologyEpoch = s_temporalTopologyEpochs[ worldIndex ];
	input.enabled = (uint8_t)( r_temporalInputTest && r_temporalInputTest->integer );
	input.cameraCut = (uint8_t)s_temporalCameraCutPending[worldIndex];
	if ( !Ral_TemporalBeginFrame( state, &input, &plan ) ) {
		return 0;
	}

	memset( &diagnostic, 0, sizeof( diagnostic ) );
	diagnostic.valid = qtrue;
	diagnostic.width = input.width;
	diagnostic.height = input.height;
	diagnostic.plan = plan;
	if ( plan.enabled ) {
		if ( s_temporalHistory[ worldIndex ].ready
				&& ( s_temporalHistory[ worldIndex ].width != input.width
					|| s_temporalHistory[ worldIndex ].height != input.height
					|| s_temporalHistory[ worldIndex ].topologyEpoch
						!= input.topologyEpoch ) ) {
			vk_temporal_history_store_shutdown();
		}
		if ( !R_TemporalHistoryEnsure( &s_temporalHistory[ worldIndex ],
				vk_ral_get_backend(), input.width, input.height,
				input.topologyEpoch ) ) {
			Ral_TemporalCancelFrame( state, frameId );
			return 0;
		}
	} else {
		R_TemporalHistoryRelease( &s_temporalHistory[ worldIndex ] );
	}
	if ( !R_TemporalEntityCacheBegin( worldIndex, input.topologyEpoch,
			plan.generation, frameId ) ) {
		Ral_TemporalCancelFrame( state, frameId );
		return 0;
	}
	{
		temporalCameraPose_t camera;
		memset( &camera, 0, sizeof( camera ) );
		memcpy( camera.projection, view->projectionMatrix, sizeof( camera.projection ) );
		memcpy( camera.worldModel, view->world.modelMatrix, sizeof( camera.worldModel ) );
		memcpy( camera.origin, view->or.origin, sizeof( camera.origin ) );
		memcpy( camera.axis, view->or.axis, sizeof( camera.axis ) );
		if ( !R_TemporalEntityCacheStageCamera( worldIndex, frameId, &camera,
				&diagnostic.cameraReceipt ) ) {
			R_TemporalEntityCacheFinish( worldIndex, frameId, qfalse );
			Ral_TemporalCancelFrame( state, frameId );
			return 0;
		}
	}
	diagnostic.resourceGeneration = s_temporalHistory[ worldIndex ].allocationGeneration;
	diagnostic.resourcesReady = s_temporalHistory[ worldIndex ].ready;
	diagnostic.projectionBefore[0] = view->projectionMatrix[8];
	diagnostic.projectionBefore[1] = view->projectionMatrix[9];
	if ( plan.enabled && !R_TemporalProjectionApply( view->projectionMatrix,
			input.width, input.height, plan.sceneJitterPixels,
			diagnostic.ndcOffset ) ) {
		R_TemporalEntityCacheFinish( worldIndex, frameId, qfalse );
		Ral_TemporalCancelFrame( state, frameId );
		return 0;
	}
	diagnostic.projectionAfter[0] = view->projectionMatrix[8];
	diagnostic.projectionAfter[1] = view->projectionMatrix[9];
	s_temporalDiagnostics[ worldIndex ] = diagnostic;
	view->temporalFrameId = frameId;
	view->temporalCameraReceipt = diagnostic.cameraReceipt;
	return frameId;
}

void R_TemporalProjectionFinish( int worldIndex, uint64_t frameId,
		qboolean queued ) {
	temporalProjectionDiagnostic_t *diagnostic;
	temporalBatchRequest_t request;

	if ( !frameId || worldIndex < 0 || worldIndex >= MAX_RENDER_WORLDS ) {
		return;
	}
	diagnostic = &s_temporalDiagnostics[ worldIndex ];
	diagnostic->queued = queued;
	if ( queued && diagnostic->valid && diagnostic->plan.frameId == frameId
			&& s_temporalStates[worldIndex].pending
			&& s_temporalStates[worldIndex].pendingPlan.frameId == frameId
			&& s_temporalStates[worldIndex].pendingPlan.generation
				== diagnostic->plan.generation
			&& s_temporalStates[worldIndex].pendingPlan.enabled
				== diagnostic->plan.enabled
			&& s_temporalStates[worldIndex].pendingWidth == diagnostic->width
			&& s_temporalStates[worldIndex].pendingHeight == diagnostic->height ) {
		memset( &request, 0, sizeof( request ) );
		request.state = TEMPORAL_BATCH_REQUEST_EXACT;
		request.token = backEndData->commands.temporalRequest.token;
		request.worldIndex = worldIndex;
		request.frameId = frameId;
		request.width = diagnostic->width;
		request.height = diagnostic->height;
		request.topologyEpoch =
			s_temporalStates[worldIndex].pendingTopologyEpoch;
		request.planGeneration = diagnostic->plan.generation;
		request.enabled = diagnostic->plan.enabled ? 1u : 0u;
		(void)R_TemporalBatchRequestPublish(
			&backEndData->commands.temporalRequest, &request );
	}
	if ( !queued ) Ral_TemporalCancelFrame( &s_temporalStates[ worldIndex ], frameId );
	if ( !queued ) R_TemporalEntityCacheFinish( worldIndex, frameId, qfalse );
	diagnostic->committed = qfalse;
}

void R_TemporalBackendRecorded( int worldIndex, uint64_t frameId ) {
	if ( !frameId || worldIndex < 0 || worldIndex >= MAX_RENDER_WORLDS ) return;
	if ( !s_temporalStates[ worldIndex ].pending
			|| s_temporalStates[ worldIndex ].pendingPlan.frameId != frameId ) return;
	if ( !R_TemporalEntityCacheBegin( worldIndex,
			s_temporalStates[worldIndex].pendingTopologyEpoch,
			s_temporalStates[worldIndex].pendingPlan.generation, frameId ) ) return;
	s_temporalBackendWorld = worldIndex;
	s_temporalBackendFrame = frameId;
}

void R_TemporalBackendEntityReceipts( uint32_t drawSurfs,
		uint32_t visibleTemporal, uint32_t accepted, uint32_t previous, uint32_t rejected,
		const temporalEntityPoseReceipt_t *sample ) {
	if ( s_temporalBackendWorld < 0 ) return;
	s_temporalDiagnostics[s_temporalBackendWorld].entityDrawSurfs += drawSurfs;
	s_temporalDiagnostics[s_temporalBackendWorld].entityVisibleTemporal += visibleTemporal;
	s_temporalDiagnostics[s_temporalBackendWorld].entityAccepted += accepted;
	s_temporalDiagnostics[s_temporalBackendWorld].entityPrevious += previous;
	s_temporalDiagnostics[s_temporalBackendWorld].entityRejected += rejected;
	if ( sample && sample->valid ) {
		s_temporalDiagnostics[s_temporalBackendWorld].entitySample = *sample;
	}
}

qboolean R_TemporalBackendEntityReceiptsBegin( void ) {
	temporalProjectionDiagnostic_t *diagnostic;
	if ( s_temporalBackendWorld < 0 ) return qfalse;
	diagnostic = &s_temporalDiagnostics[s_temporalBackendWorld];
	diagnostic->entityReceiptAttempts++;
	if ( diagnostic->entityReceiptsRecorded ) return qfalse;
	diagnostic->entityReceiptsRecorded = qtrue;
	diagnostic->entityReceiptScans++;
	return qtrue;
}

static void R_TemporalAppendU32( char *out, size_t capacity, size_t *used,
		uint32_t value ) {
	if ( *used < capacity ) {
		int wrote = snprintf( out + *used, capacity - *used, "%08x", value );
		if ( wrote > 0 ) *used += (size_t)wrote;
	}
}

static void R_TemporalAppendU64( char *out, size_t capacity, size_t *used,
		uint64_t value ) {
	if ( *used < capacity ) {
		int wrote = snprintf( out + *used, capacity - *used, "%016llx",
			(unsigned long long)value );
		if ( wrote > 0 ) *used += (size_t)wrote;
	}
}

static void R_TemporalAppendFloat( char *out, size_t capacity, size_t *used,
		float value ) {
	uint32_t bits;
	memcpy( &bits, &value, sizeof( bits ) );
	R_TemporalAppendU32( out, capacity, used, bits );
}

static void R_TemporalCameraBits( const temporalCameraPose_t *pose,
		char *out, size_t capacity ) {
	size_t used = 0;
	const float *fields[] = { pose->projection, pose->worldModel,
		pose->origin, pose->axis };
	const size_t counts[] = { 16, 16, 3, 9 };
	out[0] = '\0';
	for ( size_t group = 0; group < ARRAY_LEN( fields ); ++group ) {
		for ( size_t i = 0; i < counts[group]; ++i ) {
			R_TemporalAppendFloat( out, capacity, &used, fields[group][i] );
		}
	}
}

static void R_TemporalEntityBits( const temporalEntityPose_t *pose,
		char *out, size_t capacity ) {
	size_t used = 0;
	out[0] = '\0';
	R_TemporalAppendU32( out, capacity, &used, (uint32_t)pose->hModel );
	R_TemporalAppendU64( out, capacity, &used, (uint64_t)pose->modelToken );
	R_TemporalAppendU64( out, capacity, &used, (uint64_t)pose->modelDataToken );
	R_TemporalAppendU32( out, capacity, &used, pose->modelType );
	R_TemporalAppendU32( out, capacity, &used, pose->modelTopology );
	R_TemporalAppendU32( out, capacity, &used, (uint32_t)pose->frame );
	R_TemporalAppendU32( out, capacity, &used, (uint32_t)pose->oldframe );
	R_TemporalAppendFloat( out, capacity, &used, pose->backlerp );
	for ( size_t i = 0; i < ARRAY_LEN( pose->origin ); ++i )
		R_TemporalAppendFloat( out, capacity, &used, pose->origin[i] );
	for ( size_t i = 0; i < ARRAY_LEN( pose->axis ); ++i )
		R_TemporalAppendFloat( out, capacity, &used, pose->axis[i] );
	R_TemporalAppendU32( out, capacity, &used, pose->nonNormalizedAxes );
}

qboolean R_TemporalBackendGetPending( int worldIndex, uint64_t frameId,
		const ralTemporalFramePlan_t **plan, temporalHistoryResources_t **history ) {
	if ( plan ) *plan = NULL;
	if ( history ) *history = NULL;
	if ( !frameId || worldIndex < 0 || worldIndex >= MAX_RENDER_WORLDS
			|| !s_temporalStates[worldIndex].pending
			|| s_temporalStates[worldIndex].pendingPlan.frameId != frameId
			|| !s_temporalDiagnostics[worldIndex].queued
			|| s_temporalDiagnostics[worldIndex].historyRecorded
			|| !s_temporalDiagnostics[worldIndex].resourcesReady ) return qfalse;
	if ( plan ) *plan = &s_temporalStates[worldIndex].pendingPlan;
	if ( history ) *history = &s_temporalHistory[worldIndex];
	return qtrue;
}

void R_TemporalBackendMarkHistoryRecorded( int worldIndex, uint64_t frameId,
		qboolean previousSlotRead ) {
	if ( worldIndex < 0 || worldIndex >= MAX_RENDER_WORLDS
			|| s_temporalBackendWorld != worldIndex
			|| s_temporalBackendFrame != frameId ) return;
	s_temporalDiagnostics[worldIndex].historyRecorded = qtrue;
	s_temporalDiagnostics[worldIndex].previousSlotRead = previousSlotRead;
}

void R_TemporalBackendSubmitted( qboolean submitted ) {
	temporalProjectionDiagnostic_t *diagnostic;
	int committed;
	if ( s_temporalBackendWorld < 0 || s_temporalBackendWorld >= MAX_RENDER_WORLDS
			|| !s_temporalBackendFrame ) return;
	diagnostic = &s_temporalDiagnostics[ s_temporalBackendWorld ];
	committed = submitted && ( !diagnostic->plan.enabled || diagnostic->historyRecorded )
		? Ral_TemporalCommitFrame( &s_temporalStates[ s_temporalBackendWorld ], s_temporalBackendFrame )
		: Ral_TemporalCancelFrame( &s_temporalStates[ s_temporalBackendWorld ], s_temporalBackendFrame );
	diagnostic->committed = (qboolean)( submitted
		&& ( !diagnostic->plan.enabled || diagnostic->historyRecorded ) && committed );
	diagnostic->entityCommitted = R_TemporalEntityCacheFinish(
		s_temporalBackendWorld, s_temporalBackendFrame, diagnostic->committed );
	if ( diagnostic->committed ) {
		s_temporalCameraCutPending[s_temporalBackendWorld] = qfalse;
	}
	s_temporalBackendWorld = -1;
	s_temporalBackendFrame = 0;
}

void R_TemporalCancelQueuedFrames( void ) {
	if ( s_temporalBackendWorld >= 0 && s_temporalBackendFrame ) {
		R_TemporalEntityCacheFinish( s_temporalBackendWorld,
			s_temporalBackendFrame, qfalse );
	}
	for ( int worldIndex = 0; worldIndex < MAX_RENDER_WORLDS; ++worldIndex ) {
		if ( s_temporalStates[worldIndex].pending ) {
			R_TemporalEntityCacheFinish( worldIndex,
				s_temporalStates[worldIndex].pendingPlan.frameId, qfalse );
			Ral_TemporalCancelFrame( &s_temporalStates[worldIndex],
				s_temporalStates[worldIndex].pendingPlan.frameId );
			s_temporalDiagnostics[worldIndex].committed = qfalse;
		}
	}
	s_temporalBackendWorld = -1;
	s_temporalBackendFrame = 0;
}

void R_TemporalProjectionDump( void ) {
	int found = 0;
	for ( int worldIndex = 0; worldIndex < MAX_RENDER_WORLDS; ++worldIndex ) {
		const temporalProjectionDiagnostic_t *d =
			&s_temporalDiagnostics[ worldIndex ];
		if ( !d->valid ) {
			continue;
		}
		found = 1;
		R_LOG( rch_temporal, SEV_INFO,
			"temporal-projection world=%d frame=%llu enabled=%u queued=%d recorded=%d previous-read=%d committed=%d camera-valid=%d camera-previous=%d entities=%u/%u previous=%u rejected=%u entity-commit=%d generation=%u reset=0x%x history-valid=%u read=%u write=%u phase=%u extent=%ux%u resources=%s resource-generation=%u color-slots=%u depth-slots=%u jitter-px=%.6f,%.6f jitter-uv=%.9f,%.9f ui-jitter=%.1f,%.1f ndc=%.9f,%.9f projection=%.9f,%.9f->%.9f,%.9f\n",
			worldIndex, (unsigned long long)d->plan.frameId,
			(unsigned)d->plan.enabled, (int)d->queued,
			(int)d->historyRecorded, (int)d->previousSlotRead,
			(int)d->committed,
			(int)d->cameraReceipt.valid, (int)d->cameraReceipt.previousValid,
			d->entityAccepted, d->entityVisibleTemporal, d->entityPrevious,
			d->entityRejected, (int)d->entityCommitted,
			d->plan.generation, d->plan.resetMask,
			(unsigned)d->plan.historyValid,
			(unsigned)d->plan.historyReadIndex,
			(unsigned)d->plan.historyWriteIndex,
			(unsigned)d->plan.jitterPhase, d->width, d->height,
			d->resourcesReady ? "ready" : "none", d->resourceGeneration,
			d->resourcesReady ? 2u : 0u, d->resourcesReady ? 2u : 0u,
			d->plan.sceneJitterPixels[0], d->plan.sceneJitterPixels[1],
			d->plan.sceneJitterUv[0], d->plan.sceneJitterUv[1],
			d->plan.uiJitterPixels[0], d->plan.uiJitterPixels[1],
			d->ndcOffset[0], d->ndcOffset[1],
			d->projectionBefore[0], d->projectionBefore[1],
			d->projectionAfter[0], d->projectionAfter[1] );
		{
			char cameraCurrent[353], cameraPrevious[353];
			char entityCurrent[193], entityPrevious[193];
			R_TemporalCameraBits( &d->cameraReceipt.current,
				cameraCurrent, sizeof( cameraCurrent ) );
			R_TemporalCameraBits( &d->cameraReceipt.previous,
				cameraPrevious, sizeof( cameraPrevious ) );
			R_TemporalEntityBits( &d->entitySample.current,
				entityCurrent, sizeof( entityCurrent ) );
			R_TemporalEntityBits( &d->entitySample.previous,
				entityPrevious, sizeof( entityPrevious ) );
		R_LOG( rch_temporal, SEV_INFO,
			"temporal-continuity schema=1 world=%d frame=%llu committed=%d camera-valid=%d camera-previous=%d camera-previous-frame=%llu camera-current=%s camera-previous-fields=%s entity-valid=%d entity-owner=%u entity-generation=%u entity-role=%u entity-previous=%d entity-previous-frame=%llu entity-current=%s entity-previous-fields=%s entity-committed=%d attempts=%u scans=%u drawsurfs=%u visible-temporal=%u accepted=%u rejected=%u\n",
			worldIndex, (unsigned long long)d->plan.frameId, (int)d->committed,
			(int)d->cameraReceipt.valid, (int)d->cameraReceipt.previousValid,
			(unsigned long long)d->cameraReceipt.previousFrameId,
			cameraCurrent, cameraPrevious,
			(int)d->entitySample.valid, d->entitySample.identity.ownerId,
			d->entitySample.identity.generation, d->entitySample.identity.role,
			(int)d->entitySample.previousValid,
			(unsigned long long)d->entitySample.previousFrameId,
			entityCurrent, entityPrevious,
			(int)d->entityCommitted, d->entityReceiptAttempts,
			d->entityReceiptScans, d->entityDrawSurfs, d->entityVisibleTemporal,
			d->entityAccepted, d->entityRejected );
		}
	}
	if ( !found ) {
		R_LOG( rch_temporal, SEV_INFO,
			"temporal-projection status=unavailable\n" );
	}
}
