// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "ral_temporal.h"
#include "tr_temporal_projection.h"
#include "tr_temporal_history.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if ( !(x) ) { fprintf( stderr, "FAIL RAL temporal line %d: %s\n", __LINE__, #x ); return 1; } } while ( 0 )
#define CLOSE(a,b) ( fabsf( (a) - (b) ) < 0.000001f )

static int textureCreates, viewCreates, textureDestroys, viewDestroys;
static int failTextureCreate, failViewCreate;
static int aliasTextureCreate;
static ralTexture_t *aliasTexture;
static ralCaps_t fakeCaps = { .maxTextureDimension2D = 8192 };
struct ralTexture_s { int id; };
struct ralTextureView_s { int id; };
static struct ralTexture_s fakeTextures[32];
static struct ralTextureView_s fakeViews[32];

ralTexture_t *Ral_CreateTexture( ralBackend_t *b, const ralTextureCreateInfo_t *ci ) {
	(void)b; (void)ci;
	textureCreates++;
	if ( textureCreates == failTextureCreate ) return NULL;
	if ( textureCreates == aliasTextureCreate ) return aliasTexture;
	fakeTextures[textureCreates].id = textureCreates;
	return &fakeTextures[textureCreates];
}
ralTextureView_t *Ral_CreateTextureView( ralBackend_t *b, const ralTextureViewCreateInfo_t *ci ) {
	(void)b; (void)ci;
	viewCreates++;
	if ( viewCreates == failViewCreate ) return NULL;
	fakeViews[viewCreates].id = viewCreates;
	return &fakeViews[viewCreates];
}
void Ral_DestroyTexture( ralTexture_t *t ) { if ( t ) textureDestroys++; }
void Ral_DestroyTextureView( ralTextureView_t *v ) { if ( v ) viewDestroys++; }
const ralCaps_t *Ral_GetCaps( ralBackend_t *b ) { return b ? &fakeCaps : NULL; }
qboolean Ral_TextureFormatSupports( ralBackend_t *b, ralFormat_t format,
		ralTextureUsage_t usage ) {
	const ralTextureUsage_t exact = RAL_TEXTURE_USAGE_SAMPLED
		| RAL_TEXTURE_USAGE_STORAGE | RAL_TEXTURE_USAGE_TRANSFER_SRC
		| RAL_TEXTURE_USAGE_TRANSFER_DST;
	return b && ( format == RAL_FORMAT_R16G16B16A16_SFLOAT
		|| format == RAL_FORMAT_R32_SFLOAT ) && usage == exact ? qtrue : qfalse;
}

static ralTemporalFrameInput_t Input( uint64_t frameId, uint32_t width,
		uint32_t height, uint32_t viewId, uint32_t topologyEpoch,
		int enabled, int cameraCut ) {
	ralTemporalFrameInput_t input;
	memset( &input, 0, sizeof( input ) );
	input.frameId = frameId;
	input.width = width;
	input.height = height;
	input.viewId = viewId;
	input.topologyEpoch = topologyEpoch;
	input.enabled = (uint8_t)enabled;
	input.cameraCut = (uint8_t)cameraCut;
	return input;
}

int main( void ) {
	ralTemporalState_t state, before;
	ralTemporalFramePlan_t plan, untouched;
	ralTemporalFrameInput_t input;
	float firstJitter[2];
	float projection[16], projectionBefore[16], ndc[2], ndcBefore[2];
	temporalHistoryResources_t history, historyBefore;
	temporalHistoryCommittedReceipt_t committed, committedBefore;
	temporalHistoryFrameView_t frameView, frameViewBefore;
	temporalHistoryFeedbackSource_t feedbackSource;
	temporalHistoryPendingWriteReceipt_t pendingWrite, pendingBefore;
	ralBackend_t *fakeBackend = (ralBackend_t *)(uintptr_t)1;

	R_TemporalHistoryInit( &history );
	CHECK( R_TemporalHistoryEnsure( &history, fakeBackend, 1280, 720, 1 ) );
	CHECK( history.ready && history.allocationGeneration == 1 );
	CHECK( textureCreates == 4 && viewCreates == 4 );
	historyBefore = history;
	CHECK( R_TemporalHistoryEnsure( &history, fakeBackend, 1280, 720, 1 ) );
	CHECK( memcmp( &history, &historyBefore, sizeof( history ) ) == 0 );
	CHECK( textureCreates == 4 && viewCreates == 4 );
	CHECK( R_TemporalHistoryEnsure( &history, fakeBackend, 1600, 900, 1 ) );
	CHECK( history.allocationGeneration == 2 && history.width == 1600 );
	CHECK( textureDestroys == 4 && viewDestroys == 4 );
	historyBefore = history;
	failTextureCreate = textureCreates + 2;
	CHECK( !R_TemporalHistoryEnsure( &history, fakeBackend, 1600, 900, 2 ) );
	CHECK( memcmp( &history, &historyBefore, sizeof( history ) ) == 0 );
	CHECK( textureDestroys == 5 );
	failTextureCreate = 0;
	CHECK( R_TemporalHistoryEnsure( &history, fakeBackend, 1600, 900, 2 ) );
	CHECK( history.allocationGeneration == 3 && history.topologyEpoch == 2 );
	memset( &plan, 0, sizeof( plan ) );
	plan.enabled = 1; plan.frameId = 20; plan.generation = 7;
	plan.historyValid = 1; plan.historyReadIndex = 0; plan.historyWriteIndex = 1;
	memset( &committed, 0, sizeof( committed ) );
	committed.valid = qtrue; committed.frameId = 19; committed.worldIndex = 3;
	committed.planGeneration = plan.generation;
	committed.allocationGeneration = history.allocationGeneration;
	committed.historyIndex = 0; committed.width = history.width;
	committed.height = history.height; committed.topologyEpoch = history.topologyEpoch;
	committed.color = history.color[0]; committed.colorView = history.colorView[0];
	committed.depth = history.depth[0]; committed.depthView = history.depthView[0];
	committed.source.backend = fakeBackend;
	committed.source.sourceSceneColor = &fakeTextures[29];
	committed.source.sourcePostprocessGroup = (ralBindGroup_t *)&fakeViews[26];
	committed.source.sourceHistogramGroup = (ralBindGroup_t *)&fakeViews[27];
	committed.source.sourceColor = &fakeTextures[30];
	committed.source.sourceColorView = &fakeViews[30];
	committed.source.postprocessGroup = (ralBindGroup_t *)&fakeViews[28];
	committed.source.histogramGroup = (ralBindGroup_t *)&fakeViews[29];
	committed.source.batchToken = 99;
	committed.source.frameId = committed.frameId;
	committed.source.contentSerial = 199;
	committed.source.commandSlot = 0;
	committed.source.frameCount = 3;
	committed.source.worldIndex = 3;
	committed.source.width = history.width;
	committed.source.height = history.height;
	committed.source.topologyEpoch = history.topologyEpoch;
	committed.source.planGeneration = plan.generation;
	committed.source.sceneColorAttachmentGeneration = 8;
	committed.source.targetAllocationGeneration = 9;
	committed.source.resolveOwnerAllocationGeneration = 6;
	committed.source.storeOwnerAllocationGeneration = 5;
	committed.source.sceneFormat = RAL_FORMAT_R16G16B16A16_SFLOAT;
	committed.source.producer = TEMPORAL_HISTORY_WRITE_RESOLVED_FEEDBACK;
	CHECK( R_TemporalHistoryBuildFrameView( &history, &plan, &committed, 3, &frameView ) );
	CHECK( frameView.historyValid && frameView.readIndex == 0 && frameView.writeIndex == 1 );
	CHECK( frameView.readColor == committed.color && frameView.writeColor == history.color[1] );
	frameViewBefore = frameView; committedBefore = committed;
	committed.color = history.color[1];
	CHECK( !R_TemporalHistoryBuildFrameView( &history, &plan, &committed, 3, &frameView ) );
	CHECK( memcmp( &frameView, &frameViewBefore, sizeof( frameView ) ) == 0 );
	committed = committedBefore; plan.historyValid = 0;
	memset( &frameView, 0xa5, sizeof( frameView ) );
	CHECK( R_TemporalHistoryBuildFrameView( &history, &plan, NULL, 3, &frameView ) );
	CHECK( !frameView.historyValid && !frameView.committed.valid
		&& frameView.readIndex != frameView.writeIndex );
	memset( &feedbackSource, 0, sizeof( feedbackSource ) );
	feedbackSource.backend = fakeBackend;
	feedbackSource.sourceSceneColor = &fakeTextures[29];
	feedbackSource.sourcePostprocessGroup = (ralBindGroup_t *)&fakeViews[26];
	feedbackSource.sourceHistogramGroup = (ralBindGroup_t *)&fakeViews[27];
	feedbackSource.sourceColor = &fakeTextures[30];
	feedbackSource.sourceColorView = &fakeViews[30];
	feedbackSource.postprocessGroup = (ralBindGroup_t *)&fakeViews[28];
	feedbackSource.histogramGroup = (ralBindGroup_t *)&fakeViews[29];
	feedbackSource.batchToken = 100;
	feedbackSource.frameId = plan.frameId;
	feedbackSource.contentSerial = 200;
	feedbackSource.commandSlot = 1;
	feedbackSource.frameCount = 3;
	feedbackSource.worldIndex = 3;
	feedbackSource.width = history.width;
	feedbackSource.height = history.height;
	feedbackSource.topologyEpoch = history.topologyEpoch;
	feedbackSource.planGeneration = plan.generation;
	feedbackSource.sceneColorAttachmentGeneration = 8;
	feedbackSource.targetAllocationGeneration = 9;
	feedbackSource.storeOwnerAllocationGeneration = 5;
	feedbackSource.sceneFormat = RAL_FORMAT_R16G16B16A16_SFLOAT;
	feedbackSource.producer = TEMPORAL_HISTORY_WRITE_CURRENT_SEED;
	memset( &pendingWrite, 0xa5, sizeof( pendingWrite ) );
	CHECK( R_TemporalHistoryBuildPendingWrite( &history, &plan, 3,
		&feedbackSource, &pendingWrite ) );
	CHECK( pendingWrite.valid && pendingWrite.write.valid
		&& pendingWrite.write.frameId == plan.frameId
		&& pendingWrite.write.historyIndex == plan.historyWriteIndex
		&& pendingWrite.write.color == history.color[plan.historyWriteIndex]
		&& pendingWrite.write.depth == history.depth[plan.historyWriteIndex]
		&& pendingWrite.write.source.producer ==
			TEMPORAL_HISTORY_WRITE_CURRENT_SEED );
	pendingBefore = pendingWrite;
	CHECK( R_TemporalHistoryPendingWriteEqualExact( &pendingWrite, &pendingBefore ) );
	feedbackSource.resolveOwnerAllocationGeneration = 7;
	CHECK( !R_TemporalHistoryBuildPendingWrite( &history, &plan, 3,
		&feedbackSource, &pendingWrite ) );
	CHECK( memcmp( &pendingWrite, &pendingBefore, sizeof( pendingWrite ) ) == 0 );
	feedbackSource.resolveOwnerAllocationGeneration = 0;
	feedbackSource.sourceColor = history.color[0];
	CHECK( !R_TemporalHistoryBuildPendingWrite( &history, &plan, 3,
		&feedbackSource, &pendingWrite ) );
	CHECK( memcmp( &pendingWrite, &pendingBefore, sizeof( pendingWrite ) ) == 0 );
	feedbackSource.sourceColor = &fakeTextures[30];
	feedbackSource.commandSlot = feedbackSource.frameCount;
	CHECK( !R_TemporalHistoryBuildPendingWrite( &history, &plan, 3,
		&feedbackSource, &pendingWrite ) );
	CHECK( memcmp( &pendingWrite, &pendingBefore, sizeof( pendingWrite ) ) == 0 );
	feedbackSource.commandSlot = 1;
	plan.historyValid = 1;
	feedbackSource.producer = TEMPORAL_HISTORY_WRITE_RESOLVED_FEEDBACK;
	feedbackSource.resolveOwnerAllocationGeneration = 7;
	CHECK( R_TemporalHistoryBuildPendingWrite( &history, &plan, 3,
		&feedbackSource, &pendingWrite ) );
	CHECK( pendingWrite.write.source.resolveOwnerAllocationGeneration == 7
		&& pendingWrite.write.source.producer ==
			TEMPORAL_HISTORY_WRITE_RESOLVED_FEEDBACK );
	pendingBefore = pendingWrite;
	feedbackSource.producer = TEMPORAL_HISTORY_WRITE_CURRENT_SEED;
	CHECK( !R_TemporalHistoryBuildPendingWrite( &history, &plan, 3,
		&feedbackSource, &pendingWrite ) );
	CHECK( memcmp( &pendingWrite, &pendingBefore, sizeof( pendingWrite ) ) == 0 );
	feedbackSource.producer = TEMPORAL_HISTORY_WRITE_RESOLVED_FEEDBACK;
	feedbackSource.resolveOwnerAllocationGeneration = 0;
	CHECK( !R_TemporalHistoryBuildPendingWrite( &history, &plan, 3,
		&feedbackSource, &pendingWrite ) );
	CHECK( memcmp( &pendingWrite, &pendingBefore, sizeof( pendingWrite ) ) == 0 );
	pendingWrite.write.source.contentSerial++;
	CHECK( !R_TemporalHistoryPendingWriteEqualExact( &pendingWrite, &pendingBefore ) );
	fakeCaps.maxTextureDimension2D = 1024;
	historyBefore = history;
	CHECK( !R_TemporalHistoryEnsure( &history, fakeBackend, 1600, 901, 2 ) );
	CHECK( memcmp( &history, &historyBefore, sizeof( history ) ) == 0 );
	fakeCaps.maxTextureDimension2D = 8192;
	historyBefore = history;
	aliasTexture = history.color[0]; aliasTextureCreate = textureCreates + 1;
	{
		const int beforeTextureDestroys = textureDestroys;
		const int beforeViewDestroys = viewDestroys;
		CHECK( !R_TemporalHistoryEnsure( &history, fakeBackend, 1600, 900, 3 ) );
		CHECK( memcmp( &history, &historyBefore, sizeof( history ) ) == 0 );
		CHECK( textureDestroys == beforeTextureDestroys
			&& viewDestroys == beforeViewDestroys );
	}
	aliasTextureCreate = 0; aliasTexture = NULL;
	R_TemporalHistoryRelease( &history );
	CHECK( !history.ready && history.allocationGeneration == 3 );
	CHECK( !R_TemporalHistoryEnsure( &history, NULL, 1, 1, 1 ) );

	Ral_TemporalInit( &state );
	memset( &untouched, 0x5a, sizeof( untouched ) );
	plan = untouched;
	input = Input( 0, 1920, 1080, 7, 1, 1, 0 );
	CHECK( !Ral_TemporalBeginFrame( &state, &input, &plan ) );
	CHECK( memcmp( &plan, &untouched, sizeof( plan ) ) == 0 );
	input = Input( 1, 0, 1080, 7, 1, 1, 0 );
	CHECK( !Ral_TemporalBeginFrame( &state, &input, &plan ) );
	CHECK( memcmp( &plan, &untouched, sizeof( plan ) ) == 0 );

	input = Input( 1, 1920, 1080, 7, 1, 0, 0 );
	CHECK( Ral_TemporalBeginFrame( &state, &input, &plan ) );
	CHECK( !plan.enabled && !plan.historyValid && plan.resetMask == 0 );
	CHECK( CLOSE( plan.sceneJitterPixels[0], 0.0f ) );
	CHECK( CLOSE( plan.uiJitterPixels[0], 0.0f ) && CLOSE( plan.uiJitterPixels[1], 0.0f ) );
	CHECK( Ral_TemporalCommitFrame( &state, 1 ) );

	input = Input( 2, 1920, 1080, 7, 1, 1, 0 );
	CHECK( Ral_TemporalBeginFrame( &state, &input, &plan ) );
	CHECK( plan.enabled && !plan.historyValid );
	CHECK( plan.resetMask == RAL_TEMPORAL_RESET_ACTIVATED );
	CHECK( plan.generation == 1 && plan.jitterPhase == 0 );
	CHECK( plan.historyWriteIndex == 0 );
	CHECK( CLOSE( plan.sceneJitterPixels[0], 0.0f ) );
	CHECK( CLOSE( plan.sceneJitterPixels[1], -1.0f / 6.0f ) );
	CHECK( CLOSE( plan.sceneJitterUv[0], 0.0f ) );
	CHECK( CLOSE( plan.sceneJitterUv[1], plan.sceneJitterPixels[1] / 1080.0f ) );
	firstJitter[0] = plan.sceneJitterPixels[0];
	firstJitter[1] = plan.sceneJitterPixels[1];
	before = state;
	untouched = plan;
	CHECK( !Ral_TemporalBeginFrame( &state, &input, &plan ) );
	CHECK( memcmp( &state, &before, sizeof( state ) ) == 0 );
	CHECK( memcmp( &plan, &untouched, sizeof( plan ) ) == 0 );
	CHECK( !Ral_TemporalCommitFrame( &state, 99 ) );
	CHECK( memcmp( &state, &before, sizeof( state ) ) == 0 );
	CHECK( Ral_TemporalCommitFrame( &state, 2 ) );
	CHECK( !Ral_TemporalCommitFrame( &state, 2 ) );

	input = Input( 3, 1920, 1080, 7, 1, 1, 0 );
	CHECK( Ral_TemporalBeginFrame( &state, &input, &plan ) );
	CHECK( plan.historyValid && plan.resetMask == 0 && plan.generation == 1 );
	CHECK( plan.historyReadIndex == 0 && plan.historyWriteIndex == 1 );
	CHECK( plan.jitterPhase == 1 );
	CHECK( CLOSE( plan.sceneJitterPixels[0], -0.25f ) );
	CHECK( CLOSE( plan.sceneJitterPixels[1], 1.0f / 6.0f ) );
	CHECK( CLOSE( plan.previousJitterPixels[0], firstJitter[0] ) );
	CHECK( CLOSE( plan.previousJitterPixels[1], firstJitter[1] ) );
	CHECK( Ral_TemporalCancelFrame( &state, 3 ) );
	CHECK( !state.pending && state.nextJitterPhase == 1 );
	CHECK( Ral_TemporalBeginFrame( &state, &input, &plan ) );
	CHECK( plan.jitterPhase == 1 && plan.historyWriteIndex == 1 );
	CHECK( Ral_TemporalCommitFrame( &state, 3 ) );

	memset( projection, 0, sizeof( projection ) );
	projection[8] = 0.125f;
	projection[9] = -0.25f;
	// The fixture extent is arbitrary, so it uses the canonical 16:9
	// baseline (1280x720) rather than a 4:3 one.
	CHECK( R_TemporalProjectionApply( projection, 1280, 720,
		(float[2]){ 0.25f, -0.5f }, ndc ) );
	CHECK( CLOSE( ndc[0], 0.000390625f ) );
	CHECK( CLOSE( ndc[1], 1.0f / 720.0f ) );
	CHECK( CLOSE( projection[8], 0.125f - ndc[0] ) );
	CHECK( CLOSE( projection[9], -0.25f - ndc[1] ) );
	memcpy( projectionBefore, projection, sizeof( projection ) );
	ndcBefore[0] = ndc[0];
	ndcBefore[1] = ndc[1];
	CHECK( !R_TemporalProjectionApply( projection, 0, 720,
		(float[2]){ 0.25f, -0.5f }, ndc ) );
	CHECK( memcmp( projection, projectionBefore, sizeof( projection ) ) == 0 );
	CHECK( memcmp( ndc, ndcBefore, sizeof( ndc ) ) == 0 );
	CHECK( !R_TemporalProjectionApply( projection, 1280, 720,
		(float[2]){ NAN, 0.0f }, ndc ) );
	CHECK( memcmp( projection, projectionBefore, sizeof( projection ) ) == 0 );
	CHECK( memcmp( ndc, ndcBefore, sizeof( ndc ) ) == 0 );

	input = Input( 4, 1280, 720, 8, 2, 1, 1 );
	CHECK( Ral_TemporalBeginFrame( &state, &input, &plan ) );
	CHECK( !plan.historyValid && plan.generation == 2 );
	CHECK( plan.resetMask == ( RAL_TEMPORAL_RESET_EXTENT
		| RAL_TEMPORAL_RESET_VIEW | RAL_TEMPORAL_RESET_TOPOLOGY
		| RAL_TEMPORAL_RESET_CAMERA_CUT ) );
	CHECK( plan.jitterPhase == 0 && plan.historyWriteIndex == 0 );
	CHECK( Ral_TemporalCommitFrame( &state, 4 ) );

	input = Input( 6, 1280, 720, 8, 2, 1, 0 );
	CHECK( Ral_TemporalBeginFrame( &state, &input, &plan ) );
	CHECK( plan.resetMask == RAL_TEMPORAL_RESET_FRAME_GAP );
	CHECK( !plan.historyValid && plan.generation == 3 && plan.jitterPhase == 0 );
	CHECK( Ral_TemporalCommitFrame( &state, 6 ) );

	input = Input( 7, 1280, 720, 8, 2, 0, 0 );
	CHECK( Ral_TemporalBeginFrame( &state, &input, &plan ) );
	CHECK( plan.resetMask == RAL_TEMPORAL_RESET_DISABLED );
	CHECK( !plan.historyValid && plan.generation == 4 );
	CHECK( Ral_TemporalCommitFrame( &state, 7 ) );
	CHECK( !state.active && !state.historyValid && state.nextJitterPhase == 0 );
	before = state;
	plan = untouched;
	input = Input( 7, 1280, 720, 8, 2, 1, 0 );
	CHECK( !Ral_TemporalBeginFrame( &state, &input, &plan ) );
	CHECK( memcmp( &state, &before, sizeof( state ) ) == 0 );
	CHECK( memcmp( &plan, &untouched, sizeof( plan ) ) == 0 );

	input = Input( 8, 1280, 720, 8, 2, 1, 0 );
	CHECK( Ral_TemporalBeginFrame( &state, &input, &plan ) );
	CHECK( plan.resetMask == RAL_TEMPORAL_RESET_ACTIVATED );
	CHECK( plan.generation == 5 && plan.jitterPhase == 0 );
	CHECK( Ral_TemporalCancelFrame( &state, 8 ) );
	before = state;
	CHECK( !Ral_TemporalCancelFrame( &state, 8 ) );
	CHECK( memcmp( &state, &before, sizeof( state ) ) == 0 );

	Ral_TemporalInit( &state );
	state.generation = UINT32_MAX;
	before = state;
	plan = untouched;
	input = Input( 1, 1280, 720, 1, 1, 1, 0 );
	CHECK( !Ral_TemporalBeginFrame( &state, &input, &plan ) );
	CHECK( memcmp( &state, &before, sizeof( state ) ) == 0 );
	CHECK( memcmp( &plan, &untouched, sizeof( plan ) ) == 0 );

	Ral_TemporalInit( &state );
	for ( uint64_t frame = 1; frame <= RAL_TEMPORAL_JITTER_PHASES + 1u; ++frame ) {
		input = Input( frame, 1024, 512, 1, 1, 1, 0 );
		CHECK( Ral_TemporalBeginFrame( &state, &input, &plan ) );
		CHECK( plan.sceneJitterPixels[0] >= -0.5f && plan.sceneJitterPixels[0] < 0.5f );
		CHECK( plan.sceneJitterPixels[1] >= -0.5f && plan.sceneJitterPixels[1] < 0.5f );
		if ( frame == 1 ) {
			firstJitter[0] = plan.sceneJitterPixels[0];
			firstJitter[1] = plan.sceneJitterPixels[1];
		} else if ( frame == RAL_TEMPORAL_JITTER_PHASES + 1u ) {
			CHECK( CLOSE( plan.sceneJitterPixels[0], firstJitter[0] ) );
			CHECK( CLOSE( plan.sceneJitterPixels[1], firstJitter[1] ) );
		}
		CHECK( Ral_TemporalCommitFrame( &state, frame ) );
	}

	puts( "PASS RAL temporal frame contract" );
	return 0;
}
