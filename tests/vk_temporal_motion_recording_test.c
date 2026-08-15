// SPDX-License-Identifier: GPL-3.0-or-later

#include "vk_temporal_motion_recording.h"

#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL temporal recording %d: %s\n", __LINE__, #x); return 1; } } while (0)

static vkTemporalEntMatRuntimeFrameReceipt_t s_runtimeReceipt;
static temporalMotionOutcome_t s_outcome;
static qboolean s_buildOk = qtrue, s_preflightOk = qtrue, s_appendOk = qtrue;
static qboolean s_beginMismatch;
static int s_receiptMutation;
static int s_peeks, s_begins, s_preflights, s_appends;
static uint32_t s_appendSlot, s_appendOutcome;

qboolean VK_TemporalEntMatRuntimePeekFrameReceipt(
		const vkTemporalEntMatRuntime_t *runtime, uint32_t frameIndex,
		vkTemporalEntMatRuntimeFrameReceipt_t *outReceipt ) {
	(void)runtime; s_peeks++;
	if ( frameIndex != s_runtimeReceipt.frameIndex ) return qfalse;
	*outReceipt = s_runtimeReceipt; return qtrue;
}
qboolean VK_TemporalEntMatRuntimeBeginFrame(
		vkTemporalEntMatRuntime_t *runtime, uint32_t frameIndex,
		vkTemporalEntMatRuntimeFrameReceipt_t *outReceipt ) {
	(void)runtime; s_begins++;
	if ( frameIndex != s_runtimeReceipt.frameIndex ) return qfalse;
	*outReceipt = s_runtimeReceipt;
	if ( s_beginMismatch ) outReceipt->payloadAllocationGeneration++;
	return qtrue;
}
qboolean VK_TemporalEntMatRuntimeAppendAt(
		vkTemporalEntMatRuntime_t *runtime,
		const vkTemporalEntMatRuntimeFrameReceipt_t *receipt,
		uint32_t slot, temporalMotionOutcome_t outcome,
		const temporalMotionMatrices_t *matrices, uint32_t *outSlot ) {
	(void)runtime; (void)receipt; s_appends++; s_appendSlot = slot;
	s_appendOutcome = (uint32_t)outcome;
	if ( outcome == TEMPORAL_MOTION_WRITE_VALID && !matrices ) return qfalse;
	if ( outcome == TEMPORAL_MOTION_INVALIDATE_OPAQUE && matrices ) return qfalse;
	if ( !s_appendOk ) return qfalse;
	*outSlot = slot; return qtrue;
}
qboolean VK_TemporalGenericPipelineTablePreflightSlot(
		const vkTemporalGenericPipelineTable_t *owner, uint32_t slot,
		const ralPipeline_t *ordinary,
		vkTemporalGenericPipelineReceipt_t *outReceipt,
		ralPipeline_t *outPipelines[3] ) {
	(void)owner; s_preflights++;
	if ( !s_preflightOk || !ordinary ) return qfalse;
	memset( outReceipt, 0, sizeof( *outReceipt ) );
	outReceipt->tableGeneration = 11; outReceipt->slot = slot;
	outReceipt->entryGeneration = 1; outReceipt->recipeOwnerEpoch = 2;
	outReceipt->recipeEntryGeneration = 3;
	outReceipt->factoryAllocationGeneration = 4; outReceipt->catalogId = 5;
	outPipelines[0] = (ralPipeline_t *)(uintptr_t)11;
	outPipelines[1] = (ralPipeline_t *)(uintptr_t)12;
	outPipelines[2] = (ralPipeline_t *)(uintptr_t)13;
	switch ( s_receiptMutation ) {
	case 1: outReceipt->tableGeneration = 7; break;
	case 2: outReceipt->slot = slot + 1u; break;
	case 3: outReceipt->entryGeneration = 0; break;
	case 4: outReceipt->recipeOwnerEpoch = 0; break;
	case 5: outReceipt->recipeEntryGeneration = 0; break;
	case 6: outReceipt->factoryAllocationGeneration = 0; break;
	case 7: outReceipt->catalogId = 0; break;
	case 8: outPipelines[1] = NULL; break;
	case 9: outPipelines[2] = outPipelines[0]; break;
	default: break;
	}
	return qtrue;
}
temporalMotionOutcome_t R_TemporalMotionClassify(
		const temporalMotionDrawFacts_t *facts ) { (void)facts; return s_outcome; }
qboolean R_TemporalMotionBuildMatrices( temporalMotionGeometry_t geometry,
		const temporalCameraPoseReceipt_t *camera,
		const temporalEntityPoseReceipt_t *entity,
		temporalMotionMatrices_t *outMatrices ) {
	(void)geometry; (void)camera; (void)entity;
	if ( !s_buildOk ) return qfalse;
	memset( outMatrices, 0, sizeof( *outMatrices ) );
	outMatrices->currentMvp[0] = outMatrices->previousMvp[0] = 1.0f;
	return qtrue;
}

static vkTemporalMotionRecordingAuthority_t Authority( void ) {
	vkTemporalMotionRecordingAuthority_t a;
	memset( &a, 0, sizeof( a ) );
	a.token=1; a.frameId=2; a.worldIndex=0; a.width=1280; a.height=720;
	a.topologyEpoch=3; a.planGeneration=4; a.geometryBufferSize=4096;
	a.uniformItemSize=128; a.requiredCapacity=32; a.rawEntMatCapacity=64;
	a.rawEntMatAllocationGeneration=5; a.payloadAllocationGeneration=6;
	a.payloadLayoutGeneration=7; a.targetAllocationGeneration=8;
	a.pipelineLayoutAllocationGeneration=9; a.materializationGeneration=10;
	a.pipelineTableGeneration=11; a.frameIndex=1; return a;
}

int main( void ) {
	vkTemporalMotionRecordingOwner_t owner, before;
	vkTemporalEntMatRuntime_t runtime;
	vkTemporalMotionRecordingAuthority_t authority = Authority();
	vkTemporalMotionMaterializationReceipt_t materialization;
	vkTemporalGenericPipelineTable_t table;
	vkTemporalPipelineLayoutOwner_t layout;
	temporalMotionDrawFacts_t facts;
	vkTemporalMotionRecordingReceipt_t receipt, receiptBefore;
	vkTemporalEntMatSlotPlan_t slotPlan, slotBefore;
	memset( &runtime, 0, sizeof( runtime ) );
	memset( &materialization, 0, sizeof( materialization ) );
	memset( &table, 0, sizeof( table ) ); memset( &layout, 0, sizeof( layout ) );
	materialization.worldIndex=0; materialization.width=1280; materialization.height=720;
	materialization.topologyEpoch=3; materialization.planGeneration=4;
	materialization.payloadLayoutGeneration=7; materialization.targetAllocationGeneration=8;
	materialization.pipelineLayoutAllocationGeneration=9;
	materialization.allocationGeneration=10; materialization.ready=qtrue;
	table.slots=(vkTemporalGenericPipelineSlot_t *)(uintptr_t)1; table.capacity=64;
	table.allocationGeneration=11; table.layoutOwner=&layout;
	table.layoutAllocationGeneration=9; table.guardLease=qtrue;
	s_runtimeReceipt=(vkTemporalEntMatRuntimeFrameReceipt_t){1,5,6,7,32};
	memset( &facts, 0, sizeof( facts ) ); facts.geometry=TEMPORAL_MOTION_GEOMETRY_WORLD_STATIC;
	CHECK(VK_TemporalEntMatPlanUniformSlot(64,UINT32_MAX,0,32,&slotPlan));
	CHECK(slotPlan.fresh && slotPlan.slot==0);
	CHECK(VK_TemporalEntMatPlanUniformSlot(64,64,1,32,&slotPlan));
	CHECK(!slotPlan.fresh && slotPlan.slot==0);
	slotBefore=(vkTemporalEntMatSlotPlan_t){99,qtrue};slotPlan=slotBefore;
	CHECK(!VK_TemporalEntMatPlanUniformSlot(UINT32_MAX,64,1,32,&slotPlan));
	CHECK(memcmp(&slotPlan,&slotBefore,sizeof(slotPlan))==0);
	CHECK(!VK_TemporalEntMatPlanUniformSlot(128,64,32,32,&slotPlan));

	VK_TemporalMotionRecordingInit( &owner ); before=owner;
#define BAD_AUTH(field, value) do { \
	vkTemporalMotionRecordingAuthority_t bad=authority; int beginBefore=s_begins; \
	bad.field=(value); CHECK(!VK_TemporalMotionRecordingBegin(&owner,&runtime,&bad,&materialization,&table)); \
	CHECK(s_begins==beginBefore && memcmp(&owner,&before,sizeof(owner))==0); \
} while(0)
	BAD_AUTH(token,0); BAD_AUTH(frameId,0); BAD_AUTH(worldIndex,-1);
	BAD_AUTH(width,1279); BAD_AUTH(height,719); BAD_AUTH(topologyEpoch,0);
	BAD_AUTH(planGeneration,0); BAD_AUTH(geometryBufferSize,4095);
	BAD_AUTH(uniformItemSize,0); BAD_AUTH(requiredCapacity,31);
	BAD_AUTH(rawEntMatCapacity,31);
	BAD_AUTH(rawEntMatAllocationGeneration,UINT32_MAX);
	BAD_AUTH(payloadAllocationGeneration,7); BAD_AUTH(payloadLayoutGeneration,8);
	BAD_AUTH(targetAllocationGeneration,9);
	BAD_AUTH(pipelineLayoutAllocationGeneration,10);
	BAD_AUTH(materializationGeneration,11); BAD_AUTH(pipelineTableGeneration,12);
	BAD_AUTH(frameIndex,2);
#undef BAD_AUTH
	{ uint32_t saved=table.layoutAllocationGeneration; table.layoutAllocationGeneration++;
		CHECK(!VK_TemporalMotionRecordingBegin(&owner,&runtime,&authority,&materialization,&table));
		CHECK(s_begins==0); table.layoutAllocationGeneration=saved; }
	{ int beginBefore=s_begins; s_beginMismatch=qtrue;
		CHECK(!VK_TemporalMotionRecordingBegin(&owner,&runtime,&authority,&materialization,&table));
		CHECK(s_begins==beginBefore+1 && !VK_TemporalMotionRecordingIsActive(&owner));
		s_beginMismatch=qfalse; }
	CHECK(VK_TemporalMotionRecordingBegin(&owner,&runtime,&authority,&materialization,&table));
	CHECK(s_peeks>0 && s_begins==2 && VK_TemporalMotionRecordingIsActive(&owner));
	CHECK(VK_TemporalMotionRecordingBeginPrimaryCommand(&owner));

	s_outcome=TEMPORAL_MOTION_PRESERVE;
	CHECK(VK_TemporalMotionRecordingPrepareDraw(&owner,&table,2,(ralPipeline_t*)(uintptr_t)20,&facts,NULL,NULL));
	CHECK(VK_TemporalMotionRecordingConsumeAt(&owner,&runtime,3));
	CHECK(s_appends==0);
	s_outcome=TEMPORAL_MOTION_DEFER_ATEST;
	CHECK(VK_TemporalMotionRecordingPrepareDraw(&owner,&table,2,(ralPipeline_t*)(uintptr_t)20,&facts,NULL,NULL));
	CHECK(VK_TemporalMotionRecordingConsumeAt(&owner,&runtime,4));
	CHECK(s_appends==0 && !owner.poisoned);
	s_outcome=TEMPORAL_MOTION_WRITE_VALID;
	CHECK(VK_TemporalMotionRecordingPrepareDraw(&owner,&table,2,(ralPipeline_t*)(uintptr_t)20,&facts,NULL,NULL));
	{
		temporalMotionOutcome_t pendingOutcome;
		uint32_t pendingSlot;
		CHECK(VK_TemporalMotionRecordingPeekPendingDraw(&owner,&pendingOutcome,&pendingSlot));
		CHECK(pendingOutcome==TEMPORAL_MOTION_WRITE_VALID && pendingSlot==2);
	}
	CHECK(VK_TemporalMotionRecordingConsumeAt(&owner,&runtime,5));
	{
		temporalMotionOutcome_t pendingOutcome=TEMPORAL_MOTION_WRITE_VALID;
		uint32_t pendingSlot=99;
		CHECK(!VK_TemporalMotionRecordingPeekPendingDraw(&owner,&pendingOutcome,&pendingSlot));
		CHECK(pendingOutcome==TEMPORAL_MOTION_WRITE_VALID && pendingSlot==99);
	}
	CHECK(s_preflights==1 && s_appends==1 && s_appendSlot==5
		&& s_appendOutcome==TEMPORAL_MOTION_WRITE_VALID);
	s_buildOk=qfalse;
	CHECK(VK_TemporalMotionRecordingPrepareDraw(&owner,&table,2,(ralPipeline_t*)(uintptr_t)20,&facts,NULL,NULL));
	CHECK(VK_TemporalMotionRecordingConsumeAt(&owner,&runtime,6));
	CHECK(s_appendOutcome==TEMPORAL_MOTION_INVALIDATE_OPAQUE);
	VK_TemporalMotionRecordingEndPrimaryCommand(&owner);
	VK_TemporalMotionRecordingFinish(&owner);
	CHECK(VK_TemporalMotionRecordingGetReceipt(&owner,&receipt));
	CHECK(receipt.ready && receipt.appended==2 && receipt.preserved==2
		&& receipt.deferred==1 && receipt.authority.requiredCapacity==32
		&& receipt.drawSequence.count==4
		&& receipt.drawSequence.lane0 && receipt.drawSequence.lane1);

	CHECK(VK_TemporalMotionRecordingBegin(&owner,&runtime,&authority,&materialization,&table));
	CHECK(VK_TemporalMotionRecordingBeginPrimaryCommand(&owner));
	s_buildOk=qtrue; s_preflightOk=qfalse; s_outcome=TEMPORAL_MOTION_WRITE_VALID;
	CHECK(!VK_TemporalMotionRecordingPrepareDraw(&owner,&table,2,(ralPipeline_t*)(uintptr_t)20,&facts,NULL,NULL));
	CHECK(owner.poisoned); VK_TemporalMotionRecordingEndPrimaryCommand(&owner);
	VK_TemporalMotionRecordingFinish(&owner);
	memset(&receiptBefore,0x5a,sizeof(receiptBefore)); receipt=receiptBefore;
	CHECK(!VK_TemporalMotionRecordingGetReceipt(&owner,&receipt));
	CHECK(memcmp(&receipt,&receiptBefore,sizeof(receipt))==0);

	// An exact3-ready admitted draw whose append fails poisons publication.
	s_preflightOk=qtrue; s_appendOk=qfalse;
	CHECK(VK_TemporalMotionRecordingBegin(&owner,&runtime,&authority,&materialization,&table));
	CHECK(VK_TemporalMotionRecordingBeginPrimaryCommand(&owner));
	CHECK(VK_TemporalMotionRecordingPrepareDraw(&owner,&table,2,(ralPipeline_t*)(uintptr_t)20,&facts,NULL,NULL));
	CHECK(!VK_TemporalMotionRecordingConsumeAt(&owner,&runtime,9) && owner.poisoned);
	VK_TemporalMotionRecordingEndPrimaryCommand(&owner);
	VK_TemporalMotionRecordingFinish(&owner);
	CHECK(!VK_TemporalMotionRecordingGetReceipt(&owner,&receipt));

	// PRESERVE still participates in the exact absolute-slot sequence and must
	// not accept a slot outside the command-wide capacity.
	s_appendOk=qtrue; s_outcome=TEMPORAL_MOTION_PRESERVE;
	CHECK(VK_TemporalMotionRecordingBegin(&owner,&runtime,&authority,&materialization,&table));
	CHECK(VK_TemporalMotionRecordingBeginPrimaryCommand(&owner));
	CHECK(VK_TemporalMotionRecordingPrepareDraw(&owner,&table,2,
		(ralPipeline_t*)(uintptr_t)20,&facts,NULL,NULL));
	CHECK(!VK_TemporalMotionRecordingConsumeAt(&owner,&runtime,
		authority.requiredCapacity) && owner.poisoned);
	VK_TemporalMotionRecordingEndPrimaryCommand(&owner);
	VK_TemporalMotionRecordingFinish(&owner);
	CHECK(!VK_TemporalMotionRecordingGetReceipt(&owner,&receipt));

	// Product treats a failed uniform-slot plan (~0/capacity exhaustion) as a
	// command poison rather than silently omitting an otherwise eligible draw.
	s_appendOk=qtrue;
	CHECK(VK_TemporalMotionRecordingBegin(&owner,&runtime,&authority,&materialization,&table));
	CHECK(VK_TemporalMotionRecordingBeginPrimaryCommand(&owner));
	CHECK(!VK_TemporalEntMatPlanUniformSlot(UINT32_MAX,UINT32_MAX,0,32,&slotPlan));
	VK_TemporalMotionRecordingPoison(&owner);
	VK_TemporalMotionRecordingEndPrimaryCommand(&owner);
	VK_TemporalMotionRecordingFinish(&owner);
	CHECK(!VK_TemporalMotionRecordingGetReceipt(&owner,&receipt));

	// Every identity field and all three distinct exact3 candidates are part of
	// the per-slot atomic preflight authority.
	s_preflightOk=qtrue; s_appendOk=qtrue; s_outcome=TEMPORAL_MOTION_WRITE_VALID;
	for ( s_receiptMutation=1; s_receiptMutation<=9; ++s_receiptMutation ) {
		CHECK(VK_TemporalMotionRecordingBegin(&owner,&runtime,&authority,&materialization,&table));
		CHECK(VK_TemporalMotionRecordingBeginPrimaryCommand(&owner));
		CHECK(!VK_TemporalMotionRecordingPrepareDraw(&owner,&table,2,
			(ralPipeline_t*)(uintptr_t)20,&facts,NULL,NULL));
		CHECK(owner.poisoned && !owner.pending);
		VK_TemporalMotionRecordingEndPrimaryCommand(&owner);
		VK_TemporalMotionRecordingFinish(&owner);
		CHECK(!VK_TemporalMotionRecordingGetReceipt(&owner,&receipt));
	}
	s_receiptMutation=0;
	puts("temporal motion inert recording contract: PASS"); return 0;
}
