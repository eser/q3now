// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "vk_temporal_main_activation.h"

#include <limits.h>
#include <string.h>

static qboolean AuthorityValid(
		const vkTemporalMotionRecordingAuthority_t *a ) {
	return a && a->token && a->frameId && a->width && a->height
		&& a->worldIndex >= 0 && a->topologyEpoch && a->planGeneration
		&& a->geometryBufferSize && a->uniformItemSize
		&& a->requiredCapacity > 0
		&& a->requiredCapacity <= TEMPORAL_MOTION_PAYLOAD_MAX_SLOTS
		&& a->rawEntMatCapacity >= a->requiredCapacity
		&& a->geometryBufferSize / a->uniformItemSize == a->requiredCapacity
		&& a->rawEntMatAllocationGeneration
		&& a->rawEntMatAllocationGeneration != UINT32_MAX
		&& a->payloadAllocationGeneration && a->payloadLayoutGeneration
		&& a->targetAllocationGeneration
		&& a->pipelineLayoutAllocationGeneration
		&& a->materializationGeneration && a->pipelineTableGeneration
		&& a->frameIndex < TEMPORAL_MOTION_PAYLOAD_MAX_FRAMES
		? qtrue : qfalse;
}

static qboolean AuthorityEqual(
		const vkTemporalMotionRecordingAuthority_t *a,
		const vkTemporalMotionRecordingAuthority_t *b ) {
	return a && b
		&& a->token == b->token && a->frameId == b->frameId
		&& a->worldIndex == b->worldIndex
		&& a->width == b->width && a->height == b->height
		&& a->topologyEpoch == b->topologyEpoch
		&& a->planGeneration == b->planGeneration
		&& a->geometryBufferSize == b->geometryBufferSize
		&& a->uniformItemSize == b->uniformItemSize
		&& a->requiredCapacity == b->requiredCapacity
		&& a->rawEntMatCapacity == b->rawEntMatCapacity
		&& a->rawEntMatAllocationGeneration == b->rawEntMatAllocationGeneration
		&& a->payloadAllocationGeneration == b->payloadAllocationGeneration
		&& a->payloadLayoutGeneration == b->payloadLayoutGeneration
		&& a->targetAllocationGeneration == b->targetAllocationGeneration
		&& a->pipelineLayoutAllocationGeneration == b->pipelineLayoutAllocationGeneration
		&& a->materializationGeneration == b->materializationGeneration
		&& a->pipelineTableGeneration == b->pipelineTableGeneration
		&& a->frameIndex == b->frameIndex ? qtrue : qfalse;
}

static qboolean PipelineReceiptValid(
		const vkTemporalMainActivationOwner_t *owner,
		uint32_t pipelineSlot,
		const vkTemporalGenericPipelineReceipt_t *r,
		ralPipeline_t *const p[3] ) {
	return owner && r && p
		&& r->tableGeneration == owner->authority.pipelineTableGeneration
		&& r->slot == pipelineSlot
		&& r->entryGeneration && r->recipeOwnerEpoch
		&& r->recipeEntryGeneration && r->factoryAllocationGeneration
		&& r->catalogId && p[0] && p[1] && p[2]
		&& p[0] != p[1] && p[0] != p[2] && p[1] != p[2]
		? qtrue : qfalse;
}

static qboolean PipelineReceiptEqual(
		const vkTemporalGenericPipelineReceipt_t *a,
		const vkTemporalGenericPipelineReceipt_t *b ) {
	return a && b && a->tableGeneration == b->tableGeneration
		&& a->slot == b->slot && a->entryGeneration == b->entryGeneration
		&& a->recipeOwnerEpoch == b->recipeOwnerEpoch
		&& a->recipeEntryGeneration == b->recipeEntryGeneration
		&& a->factoryAllocationGeneration == b->factoryAllocationGeneration
		&& a->catalogId == b->catalogId ? qtrue : qfalse;
}

static qboolean PlanEqual( const vkTemporalMainActivationPlan_t *a,
		const vkTemporalMainActivationPlan_t *b ) {
	return a && b && AuthorityEqual( &a->authority, &b->authority )
		&& a->planSerial == b->planSerial && a->kind == b->kind
		&& a->pipelineSlot == b->pipelineSlot
		&& a->absoluteEntMatSlot == b->absoluteEntMatSlot
		&& PipelineReceiptEqual( &a->pipelineReceipt, &b->pipelineReceipt )
		&& a->pipeline == b->pipeline
		&& a->endOrdinary == b->endOrdinary
		&& a->beginExact3 == b->beginExact3
		&& a->clearAuxiliary == b->clearAuxiliary
		&& a->endExact3 == b->endExact3
		&& a->resumeOrdinary == b->resumeOrdinary ? qtrue : qfalse;
}

static uint64_t TaggedMix( uint64_t state, uint64_t value ) {
	state ^= value + UINT64_C( 0x9e3779b97f4a7c15 ) + ( state << 6 )
		+ ( state >> 2 );
	state ^= state >> 30;
	state *= UINT64_C( 0xbf58476d1ce4e5b9 );
	state ^= state >> 27;
	state *= UINT64_C( 0x94d049bb133111eb );
	return state ^ ( state >> 31 );
}

static qboolean TaggedAppend( vkTemporalMainTaggedSequence_t *sequence,
		vkTemporalMainEventKind_t kind, uint32_t ordinal,
		uint32_t recordIndex, vkTemporalShaderRecipeKind_t recipe,
		uint64_t familyDigest ) {
	vkTemporalMainTaggedSequence_t candidate;
	uint64_t words[7];
	uint32_t i;
	if ( !sequence || sequence->count == UINT32_MAX
			|| sequence->genericCount == UINT32_MAX
			|| sequence->iqmCount == UINT32_MAX
			|| ( kind != VK_TEMPORAL_MAIN_EVENT_GENERIC
				&& kind != VK_TEMPORAL_MAIN_EVENT_IQM ) ) return qfalse;
	words[0] = (uint32_t)kind; words[1] = ordinal; words[2] = recordIndex;
	words[3] = (uint32_t)recipe; words[4] = familyDigest;
	words[5] = familyDigest >> 32; words[6] = sequence->count + 1u;
	candidate = *sequence;
	if ( candidate.count == 0 ) {
		candidate.lane0 = UINT64_C( 0x6a09e667f3bcc909 );
		candidate.lane1 = UINT64_C( 0xbb67ae8584caa73b );
	}
	for ( i = 0; i < 7u; ++i ) {
		candidate.lane0 = TaggedMix( candidate.lane0, words[i] );
		candidate.lane1 = TaggedMix( candidate.lane1,
			words[6u - i] ^ UINT64_C( 0x3c6ef372fe94f82b ) );
	}
	candidate.count++;
	if ( kind == VK_TEMPORAL_MAIN_EVENT_GENERIC ) candidate.genericCount++;
	else candidate.iqmCount++;
	*sequence = candidate;
	return qtrue;
}

static qboolean TaggedExact( const vkTemporalMainTaggedSequence_t *a,
		const vkTemporalMainTaggedSequence_t *b ) {
	return a && b && a->lane0 == b->lane0 && a->lane1 == b->lane1
		&& a->count == b->count && a->genericCount == b->genericCount
		&& a->iqmCount == b->iqmCount ? qtrue : qfalse;
}

static qboolean TaggedValid(
		const vkTemporalMainTaggedSequence_t *sequence ) {
	return sequence && sequence->lane0 && sequence->lane1 && sequence->count
		&& sequence->genericCount <= TEMPORAL_MOTION_PAYLOAD_MAX_SLOTS
		&& sequence->iqmCount <= TEMPORAL_IQM_MAX_DRAWS
		&& sequence->count == sequence->genericCount + sequence->iqmCount
		? qtrue : qfalse;
}

static uint64_t GenericFamilyDigest( temporalMotionOutcome_t outcome,
		uint32_t absoluteEntMatSlot, uint32_t pipelineSlot,
		const vkTemporalGenericPipelineReceipt_t *receipt ) {
	uint64_t digest = UINT64_C( 0x510e527fade682d1 );
	const uint64_t words[10] = {
		(uint32_t)outcome, absoluteEntMatSlot, pipelineSlot,
		receipt ? receipt->tableGeneration : 0,
		receipt ? receipt->slot : 0,
		receipt ? receipt->entryGeneration : 0,
		receipt ? receipt->recipeOwnerEpoch : 0,
		receipt ? receipt->recipeEntryGeneration : 0,
		receipt ? receipt->factoryAllocationGeneration : 0,
		receipt ? receipt->catalogId : 0
	};
	for ( uint32_t i = 0; i < 10u; ++i ) digest = TaggedMix( digest, words[i] );
	return digest;
}

static uint64_t FactoryDigest(
		const vkTemporalIqmExact3FactoryReceipt_t *factory ) {
	uint64_t digest = UINT64_C( 0x1f83d9abfb41bd6b );
	const uint64_t words[] = {
		(uintptr_t)factory->backend, (uintptr_t)factory->device,
		(uintptr_t)factory->payloadOwner, (uintptr_t)factory->payloadLayout,
		(uintptr_t)factory->payloadRawLayout, (uintptr_t)factory->bindlessRawLayout,
		factory->payloadLayoutGeneration, (uintptr_t)factory->bindless.backend,
		(uintptr_t)factory->bindless.layout, (uintptr_t)factory->bindless.setIdentity,
		factory->bindless.setGeneration, (uintptr_t)factory->rawLayout,
		(uintptr_t)factory->adoptedLayout, (uintptr_t)factory->writePipeline,
		(uintptr_t)factory->invalidatePipeline, factory->shaderGeneration,
		factory->pipelineGeneration, factory->topologyGeneration,
		factory->allocationGeneration, (uint32_t)factory->sceneFormat,
		(uint32_t)factory->depthFormat, (uint32_t)factory->reversedDepth
	};
	for ( uint32_t i = 0; i < sizeof( words ) / sizeof( words[0] ); ++i )
		digest = TaggedMix( digest, words[i] );
	return digest;
}

static uint64_t IqmFamilyDigest( uint64_t sequenceDigest,
		const temporalIqmSequenceEntry_t *entry,
		const vkTemporalIqmExact3FactoryReceipt_t *factory ) {
	uint64_t digest = TaggedMix( sequenceDigest, FactoryDigest( factory ) );
	digest = TaggedMix( digest, entry->facts.ordinal );
	digest = TaggedMix( digest, entry->recordIndex );
	digest = TaggedMix( digest, (uint32_t)entry->facts.outcome );
	return digest;
}

static qboolean IqmAuthorityExact( const temporalIqmSequenceAuthority_t *a,
		const temporalIqmSequenceAuthority_t *b ) {
	return a && b && a->token == b->token && a->frameId == b->frameId
		&& a->worldIndex == b->worldIndex && a->commandSlot == b->commandSlot
		&& a->frameCount == b->frameCount ? qtrue : qfalse;
}

static qboolean IqmSummaryEmpty(
		const vkTemporalMainIqmActivationReceipt_t *iqm ) {
	vkTemporalMainIqmActivationReceipt_t empty;
	if ( !iqm ) return qfalse;
	memset( &empty, 0, sizeof( empty ) );
	return memcmp( iqm, &empty, sizeof( empty ) ) == 0 ? qtrue : qfalse;
}

static qboolean IqmSummaryValid(
		const vkTemporalMainIqmActivationReceipt_t *iqm ) {
	return iqm && iqm->ready == qtrue && iqm->authority.token
		&& iqm->authority.frameId && iqm->authority.worldIndex >= 0
		&& iqm->authority.frameCount
		&& iqm->authority.commandSlot < iqm->authority.frameCount
		&& iqm->sequenceDigest && iqm->drawCount
		&& iqm->drawCount <= TEMPORAL_IQM_MAX_DRAWS
		&& iqm->entityCount && iqm->entityCount <= TEMPORAL_IQM_MAX_RECORDS
		&& iqm->prepared == iqm->drawCount
		&& iqm->temporalSegments == iqm->prepared
		&& iqm->temporalSegments == iqm->written + iqm->invalidated
		&& VK_TemporalIqmPayloadContentReceiptExact( &iqm->content,
			&iqm->content )
		&& IqmAuthorityExact( &iqm->authority, &iqm->content.authority )
		&& iqm->content.sequenceDigest == iqm->sequenceDigest
		&& iqm->content.recordCount == iqm->entityCount
		&& VK_TemporalIqmExact3FactoryReceiptExact( &iqm->factory,
			&iqm->factory )
		&& iqm->pass.ready == qtrue && iqm->pass.topologyGeneration
		&& iqm->pass.sceneFormat != RAL_FORMAT_UNDEFINED
		&& iqm->pass.depthFormat != RAL_FORMAT_UNDEFINED
		&& ( iqm->pass.reversedDepth == qfalse
			|| iqm->pass.reversedDepth == qtrue )
		&& iqm->factory.topologyGeneration == iqm->pass.topologyGeneration
		&& iqm->factory.sceneFormat == iqm->pass.sceneFormat
		&& iqm->factory.depthFormat == iqm->pass.depthFormat
		&& iqm->factory.reversedDepth == iqm->pass.reversedDepth ? qtrue : qfalse;
}

static qboolean IqmSummaryExact(
		const vkTemporalMainIqmActivationReceipt_t *a,
		const vkTemporalMainIqmActivationReceipt_t *b ) {
	if ( IqmSummaryEmpty( a ) || IqmSummaryEmpty( b ) )
		return IqmSummaryEmpty( a ) && IqmSummaryEmpty( b );
	return IqmSummaryValid( a ) && IqmSummaryValid( b )
		&& IqmAuthorityExact( &a->authority, &b->authority )
		&& a->sequenceDigest == b->sequenceDigest
		&& a->drawCount == b->drawCount && a->entityCount == b->entityCount
		&& a->prepared == b->prepared
		&& a->temporalSegments == b->temporalSegments
		&& a->written == b->written && a->invalidated == b->invalidated
		&& VK_TemporalIqmPayloadContentReceiptExact( &a->content, &b->content )
		&& VK_TemporalIqmExact3FactoryReceiptExact( &a->factory, &b->factory )
		&& a->pass.topologyGeneration == b->pass.topologyGeneration
		&& a->pass.sceneFormat == b->pass.sceneFormat
		&& a->pass.depthFormat == b->pass.depthFormat
		&& a->pass.reversedDepth == b->pass.reversedDepth
		&& a->pass.ready == b->pass.ready
		? qtrue : qfalse;
}

static qboolean SequenceMatchesIqm(
		const temporalIqmSequence_t *sequence,
		const vkTemporalMainIqmActivationReceipt_t *iqm ) {
	return sequence && iqm && R_TemporalIqmSequenceExact( sequence, sequence )
		&& IqmAuthorityExact( &sequence->authority, &iqm->authority )
		&& sequence->orderedDigest == iqm->sequenceDigest
		&& sequence->drawCount == iqm->drawCount
		&& sequence->entityCount == iqm->entityCount ? qtrue : qfalse;
}

static qboolean IqmPlanEqual( const vkTemporalMainIqmActivationPlan_t *a,
		const vkTemporalMainIqmActivationPlan_t *b ) {
	return a && b && AuthorityEqual( &a->authority, &b->authority )
		&& a->planSerial == b->planSerial && a->ordinal == b->ordinal
		&& a->recordIndex == b->recordIndex
		&& a->sequenceDigest == b->sequenceDigest && a->kind == b->kind
		&& R_TemporalIqmSequenceEntryEqual( &a->entry, &b->entry )
		&& a->payloadGroup == b->payloadGroup
		&& a->pipeline == b->pipeline
		&& VK_TemporalIqmExact3FactoryReceiptExact( &a->factory, &b->factory )
		&& a->endOrdinary == b->endOrdinary
		&& a->beginExact3 == b->beginExact3
		&& a->clearAuxiliary == b->clearAuxiliary
		&& a->endExact3 == b->endExact3
		&& a->resumeOrdinary == b->resumeOrdinary ? qtrue : qfalse;
}

static qboolean GeometryMatchesFacts(
		const vkTemporalIqmGeometryReceipt_t *geometry,
		const temporalIqmDrawFacts_t *facts ) {
	return geometry && facts && geometry->ready == qtrue
		&& geometry->key.backend == (ralBackend_t *)facts->geometryBackend
		&& geometry->key.nativeVertexBuffer == (void *)facts->rawVertexBuffer
		&& geometry->key.nativeIndexBuffer == (void *)facts->rawIndexBuffer
		&& geometry->key.vertexBytes == facts->vertexBufferBytes
		&& geometry->key.indexBytes == facts->indexBufferBytes
		&& geometry->key.modelAllocationGeneration == facts->modelAllocationGeneration
		&& geometry->key.geometryGeneration == facts->geometryGeneration
		&& geometry->key.contentDigest == facts->modelContentDigest
		&& geometry->vertex == (ralBuffer_t *)facts->ralVertexBuffer
		&& geometry->index == (ralBuffer_t *)facts->ralIndexBuffer
		&& geometry->allocationGeneration == facts->geometryAllocationGeneration
		? qtrue : qfalse;
}

static qboolean RevalidateIqmEntry( const temporalIqmSequenceEntry_t *entry,
		void *context, const vkTemporalMainIqmActivationOps_t *ops ) {
	vkTemporalIqmGeometryReceipt_t geometry;
	vkBindlessOrdinaryReceipt_t bindless;
	if ( !entry || !ops || !ops->revalidateGeometry || !ops->revalidateBindless )
		return qfalse;
	memset( &geometry, 0, sizeof( geometry ) );
	memset( &bindless, 0, sizeof( bindless ) );
	return ops->revalidateGeometry( context, &entry->facts, &geometry )
		&& GeometryMatchesFacts( &geometry, &entry->facts )
		&& ops->revalidateBindless( context, &entry->facts, &bindless )
		&& VK_BindlessPublicationReceiptExact( &entry->facts.bindless, &bindless )
		? qtrue : qfalse;
}

void VK_TemporalMainActivationInit(
		vkTemporalMainActivationOwner_t *owner ) {
	if ( owner ) memset( owner, 0, sizeof( *owner ) );
}

qboolean VK_TemporalMainActivationBegin(
		vkTemporalMainActivationOwner_t *owner,
		const vkTemporalMotionRecordingAuthority_t *authority ) {
	if ( !owner || owner->active || owner->submissionPending ) return qfalse;
	memset( &owner->receipt, 0, sizeof( owner->receipt ) );
	memset( &owner->pendingReceipt, 0, sizeof( owner->pendingReceipt ) );
	if ( !AuthorityValid( authority ) ) return qfalse;
	memset( &owner->pendingPlan, 0, sizeof( owner->pendingPlan ) );
	memset( &owner->pendingIqmPlan, 0, sizeof( owner->pendingIqmPlan ) );
	owner->authority = *authority;
	owner->nextPlanSerial = 0;
	owner->prepared = 0;
	owner->temporalSegments = 0;
	owner->preserved = 0;
	owner->written = 0;
	owner->invalidated = 0;
	memset( &owner->drawSequence, 0, sizeof( owner->drawSequence ) );
	memset( &owner->iqm, 0, sizeof( owner->iqm ) );
	memset( &owner->taggedSequence, 0, sizeof( owner->taggedSequence ) );
	memset( &owner->iqmVerifier, 0, sizeof( owner->iqmVerifier ) );
	owner->iqmRevalidateContext = NULL;
	memset( &owner->iqmRevalidateOps, 0, sizeof( owner->iqmRevalidateOps ) );
	owner->iqmRevalidateBound = qfalse;
	owner->auxiliaryInitialized = qfalse;
	owner->requireDepthStencilStore = qtrue;
	owner->pending = qfalse;
	owner->pendingKind = 0;
	owner->submissionPending = qfalse;
	owner->poisoned = qfalse;
	owner->active = qtrue;
	return qtrue;
}

qboolean VK_TemporalMainActivationBindIqm(
		vkTemporalMainActivationOwner_t *owner,
		const temporalIqmSequence_t *sequence,
		const vkTemporalIqmPayloadContentReceipt_t *content,
		const vkTemporalIqmPayloadOwner_t *payloadOwner,
		const vkTemporalIqmExact3FactoryReceipt_t *factory,
		const vkTemporalMainIqmPassReceipt_t *pass,
		void *revalidateContext,
		const vkTemporalMainIqmActivationOps_t *revalidateOps ) {
	vkTemporalMainIqmActivationReceipt_t candidate;
	temporalIqmSequenceVerifier_t verifier;
	if ( !owner || !owner->active || owner->poisoned || owner->pending
			|| owner->prepared || owner->taggedSequence.count
			|| !IqmSummaryEmpty( &owner->iqm ) || !sequence || !content
			|| !payloadOwner || !factory || factory->payloadOwner != payloadOwner
			|| !pass || pass->ready != qtrue || !pass->topologyGeneration
			|| pass->sceneFormat == RAL_FORMAT_UNDEFINED
			|| pass->depthFormat == RAL_FORMAT_UNDEFINED
			|| !R_TemporalIqmSequenceExact( sequence, sequence )
			|| !VK_TemporalIqmPayloadContentReceiptExact( content, content )
			|| !VK_TemporalIqmPayloadContentRevalidate( content, payloadOwner )
			|| !VK_TemporalIqmExact3FactoryReceiptExact( factory, factory )
			|| sequence->authority.token != owner->authority.token
			|| sequence->authority.frameId != owner->authority.frameId
			|| sequence->authority.worldIndex != owner->authority.worldIndex
			|| sequence->authority.commandSlot != owner->authority.frameIndex
			|| !IqmAuthorityExact( &sequence->authority, &content->authority )
			|| sequence->orderedDigest != content->sequenceDigest
			|| sequence->entityCount != content->recordCount
			|| content->payload.commandSlot != sequence->authority.commandSlot
			|| content->payload.frameCount != sequence->authority.frameCount
			|| content->payload.backend != factory->backend
			|| content->payload.layout != factory->payloadLayout
			|| content->payload.ownerAllocationGeneration !=
				factory->payloadLayoutGeneration
			|| factory->topologyGeneration != pass->topologyGeneration
			|| factory->topologyGeneration != owner->authority.topologyEpoch
			|| factory->sceneFormat != pass->sceneFormat
			|| factory->depthFormat != pass->depthFormat
			|| factory->reversedDepth != pass->reversedDepth ) {
		if ( owner && owner->active ) owner->poisoned = qtrue;
		return qfalse;
	}
	memset( &verifier, 0, sizeof( verifier ) );
	if ( !R_TemporalIqmSequenceVerifierBegin( &verifier, sequence ) ) {
		owner->poisoned = qtrue; return qfalse;
	}
	for ( uint32_t i = 0; i < sequence->drawCount; ++i ) {
		const temporalIqmDrawFacts_t *facts = &sequence->entries[i].facts;
		if ( facts->geometryBackend != (uintptr_t)factory->backend
				|| facts->bindless.setIdentity !=
					(uintptr_t)factory->bindless.setIdentity
				|| facts->bindless.setGeneration != factory->bindless.setGeneration ) {
			owner->poisoned = qtrue; return qfalse;
		}
		if ( !RevalidateIqmEntry( &sequence->entries[i], revalidateContext,
				revalidateOps ) ) { owner->poisoned = qtrue; return qfalse; }
	}
	memset( &candidate, 0, sizeof( candidate ) );
	candidate.authority = sequence->authority;
	candidate.sequenceDigest = sequence->orderedDigest;
	candidate.drawCount = sequence->drawCount;
	candidate.entityCount = sequence->entityCount;
	candidate.content = *content; candidate.factory = *factory;
	candidate.pass = *pass;
	// Bound is a candidate until every expected tagged IQM draw commits.
	owner->iqmRevalidateContext = revalidateContext;
	owner->iqmRevalidateOps = *revalidateOps;
	owner->iqmRevalidateBound = qtrue;
	owner->iqmVerifier = verifier;
	owner->iqm = candidate;
	return qtrue;
}

qboolean VK_TemporalMainActivationRequiresDepthStencilStore(
		const vkTemporalMainActivationOwner_t *owner ) {
	return owner && owner->active && !owner->poisoned
		&& owner->requireDepthStencilStore
		? qtrue : qfalse;
}

qboolean VK_TemporalMainActivationPlanDraw(
		vkTemporalMainActivationOwner_t *owner,
		temporalMotionOutcome_t outcome,
		uint32_t absoluteEntMatSlot,
		uint32_t pipelineSlot,
		const ralPipeline_t *ordinaryPipeline,
		void *preflightContext,
		const vkTemporalMainActivationOps_t *ops,
		vkTemporalMainActivationPlan_t *outPlan ) {
	vkTemporalMainActivationPlan_t candidate;
	vkTemporalGenericPipelineReceipt_t pipelineReceipt;
	ralPipeline_t *exactPipelines[3];
	if ( !owner || !outPlan || !owner->active || owner->pending
			|| owner->poisoned || owner->nextPlanSerial == UINT32_MAX ) return qfalse;
	memset( &candidate, 0, sizeof( candidate ) );
	memset( &pipelineReceipt, 0, sizeof( pipelineReceipt ) );
	memset( exactPipelines, 0, sizeof( exactPipelines ) );
	if ( absoluteEntMatSlot >= owner->authority.requiredCapacity ) {
		owner->poisoned = qtrue;
		return qfalse;
	}
	candidate.authority = owner->authority;
	candidate.planSerial = owner->nextPlanSerial + 1u;
	candidate.pipelineSlot = pipelineSlot;
	candidate.absoluteEntMatSlot = absoluteEntMatSlot;
	if ( outcome == TEMPORAL_MOTION_PRESERVE ) {
		candidate.kind = VK_TEMPORAL_SHADER_PRESERVE;
	} else if ( outcome == TEMPORAL_MOTION_WRITE_VALID
			|| outcome == TEMPORAL_MOTION_INVALIDATE_OPAQUE ) {
		if ( !ordinaryPipeline || !ops || !ops->preflight
				|| !ops->preflight( preflightContext, pipelineSlot,
					ordinaryPipeline, &pipelineReceipt, exactPipelines )
				|| !PipelineReceiptValid( owner, pipelineSlot,
					&pipelineReceipt, exactPipelines ) ) {
			owner->poisoned = qtrue;
			return qfalse;
		}
		candidate.pipelineReceipt = pipelineReceipt;
		candidate.kind = outcome == TEMPORAL_MOTION_WRITE_VALID
			? VK_TEMPORAL_SHADER_WRITE : VK_TEMPORAL_SHADER_INVALIDATE;
		candidate.pipeline = exactPipelines[ candidate.kind == VK_TEMPORAL_SHADER_WRITE ? 1 : 2 ];
		candidate.endOrdinary = qtrue;
		candidate.beginExact3 = qtrue;
		candidate.clearAuxiliary = owner->auxiliaryInitialized ? qfalse : qtrue;
		candidate.endExact3 = qtrue;
		candidate.resumeOrdinary = qtrue;
	} else {
		owner->poisoned = qtrue;
		return qfalse;
	}
	owner->nextPlanSerial = candidate.planSerial;
	owner->pendingPlan = candidate;
	owner->pending = qtrue;
	owner->pendingKind = VK_TEMPORAL_MAIN_EVENT_GENERIC;
	*outPlan = candidate;
	return qtrue;
}

qboolean VK_TemporalMainActivationCommitDraw(
		vkTemporalMainActivationOwner_t *owner,
		const vkTemporalMainActivationPlan_t *plan ) {
	vkTemporalMotionDrawSequence_t drawCandidate;
	vkTemporalMainTaggedSequence_t taggedCandidate;
	if ( !owner || !owner->active || owner->poisoned || !owner->pending
			|| owner->pendingKind != VK_TEMPORAL_MAIN_EVENT_GENERIC
			|| !PlanEqual( &owner->pendingPlan, plan ) ) {
		if ( owner && owner->active ) owner->poisoned = qtrue;
		return qfalse;
	}
	if ( owner->prepared == UINT32_MAX
			|| ( plan->kind == VK_TEMPORAL_SHADER_PRESERVE
				&& owner->preserved == UINT32_MAX )
			|| ( plan->kind != VK_TEMPORAL_SHADER_PRESERVE
				&& owner->temporalSegments == UINT32_MAX )
			|| ( plan->kind == VK_TEMPORAL_SHADER_WRITE
				&& owner->written == UINT32_MAX )
			|| ( plan->kind == VK_TEMPORAL_SHADER_INVALIDATE
				&& owner->invalidated == UINT32_MAX ) ) {
		owner->poisoned = qtrue;
		return qfalse;
	}
	drawCandidate = owner->drawSequence;
	taggedCandidate = owner->taggedSequence;
	if ( !VK_TemporalMotionDrawSequenceAppend( &drawCandidate,
			plan->absoluteEntMatSlot, plan->pipelineSlot,
			plan->kind == VK_TEMPORAL_SHADER_PRESERVE
				? TEMPORAL_MOTION_PRESERVE
				: ( plan->kind == VK_TEMPORAL_SHADER_WRITE
					? TEMPORAL_MOTION_WRITE_VALID
					: TEMPORAL_MOTION_INVALIDATE_OPAQUE ),
			plan->kind == VK_TEMPORAL_SHADER_PRESERVE
				? NULL : &plan->pipelineReceipt ) ) {
		owner->poisoned = qtrue;
		return qfalse;
	}
	if ( !TaggedAppend( &taggedCandidate, VK_TEMPORAL_MAIN_EVENT_GENERIC,
			owner->prepared, plan->absoluteEntMatSlot, plan->kind,
			GenericFamilyDigest(
				plan->kind == VK_TEMPORAL_SHADER_PRESERVE
					? TEMPORAL_MOTION_PRESERVE
					: ( plan->kind == VK_TEMPORAL_SHADER_WRITE
						? TEMPORAL_MOTION_WRITE_VALID
						: TEMPORAL_MOTION_INVALIDATE_OPAQUE ),
				plan->absoluteEntMatSlot, plan->pipelineSlot,
				plan->kind == VK_TEMPORAL_SHADER_PRESERVE
					? NULL : &plan->pipelineReceipt ) ) ) {
		owner->poisoned = qtrue;
		return qfalse;
	}
	owner->drawSequence = drawCandidate;
	owner->taggedSequence = taggedCandidate;
	owner->pending = qfalse;
	owner->pendingKind = 0;
	owner->prepared++;
	if ( plan->kind == VK_TEMPORAL_SHADER_PRESERVE ) {
		owner->preserved++;
	} else {
		owner->temporalSegments++;
		owner->auxiliaryInitialized = qtrue;
		if ( plan->kind == VK_TEMPORAL_SHADER_WRITE ) owner->written++;
		else if ( plan->kind == VK_TEMPORAL_SHADER_INVALIDATE ) owner->invalidated++;
		else {
			owner->poisoned = qtrue;
			return qfalse;
		}
	}
	memset( &owner->pendingPlan, 0, sizeof( owner->pendingPlan ) );
	return qtrue;
}

qboolean VK_TemporalMainActivationPlanIqmDraw(
		vkTemporalMainActivationOwner_t *owner,
		const temporalIqmDrawFacts_t *observed,
		vkTemporalMainIqmActivationPlan_t *outPlan ) {
	vkTemporalMainIqmActivationPlan_t candidate;
	temporalIqmSequenceEntry_t entry;
	if ( !owner || !outPlan || !owner->active || owner->pending
			|| owner->poisoned || owner->nextPlanSerial == UINT32_MAX
			|| !observed || owner->iqm.prepared >= owner->iqm.drawCount
			|| owner->iqmVerifier.cursor != owner->iqm.prepared
			|| !owner->iqmRevalidateBound
			|| !R_TemporalIqmSequenceVerifierPeek(
				&owner->iqmVerifier, observed, &entry )
			|| !RevalidateIqmEntry( &entry, owner->iqmRevalidateContext,
				&owner->iqmRevalidateOps )
			|| ( entry.facts.outcome != TEMPORAL_MOTION_WRITE_VALID
				&& entry.facts.outcome != TEMPORAL_MOTION_INVALIDATE_OPAQUE ) ) {
		if ( owner && owner->active ) owner->poisoned = qtrue;
		return qfalse;
	}
	memset( &candidate, 0, sizeof( candidate ) );
	candidate.authority = owner->authority;
	candidate.planSerial = owner->nextPlanSerial + 1u;
	candidate.ordinal = owner->iqm.prepared;
	candidate.recordIndex = entry.recordIndex;
	candidate.sequenceDigest = owner->iqm.sequenceDigest;
	candidate.entry = entry;
	candidate.payloadGroup = owner->iqm.content.payload.group;
	candidate.kind = entry.facts.outcome == TEMPORAL_MOTION_WRITE_VALID
		? VK_TEMPORAL_SHADER_WRITE : VK_TEMPORAL_SHADER_INVALIDATE;
	candidate.factory = owner->iqm.factory;
	candidate.pipeline = candidate.kind == VK_TEMPORAL_SHADER_WRITE
		? candidate.factory.writePipeline : candidate.factory.invalidatePipeline;
	candidate.endOrdinary = qtrue; candidate.beginExact3 = qtrue;
	candidate.clearAuxiliary = owner->auxiliaryInitialized ? qfalse : qtrue;
	candidate.endExact3 = qtrue; candidate.resumeOrdinary = qtrue;
	if ( !candidate.payloadGroup ) {
		owner->poisoned = qtrue; return qfalse;
	}
	owner->nextPlanSerial = candidate.planSerial;
	owner->pendingIqmPlan = candidate;
	owner->pending = qtrue; owner->pendingKind = VK_TEMPORAL_MAIN_EVENT_IQM;
	*outPlan = candidate;
	return qtrue;
}

qboolean VK_TemporalMainActivationCommitIqmDraw(
		vkTemporalMainActivationOwner_t *owner,
		const vkTemporalMainIqmActivationPlan_t *plan ) {
	vkTemporalMainTaggedSequence_t taggedCandidate;
	temporalIqmSequenceVerifier_t verifierCandidate;
	if ( !owner || !owner->active || owner->poisoned || !owner->pending
			|| owner->pendingKind != VK_TEMPORAL_MAIN_EVENT_IQM
			|| !IqmPlanEqual( &owner->pendingIqmPlan, plan ) ) {
		if ( owner && owner->active ) owner->poisoned = qtrue;
		return qfalse;
	}
	if ( owner->iqm.prepared == UINT32_MAX
			|| owner->iqm.temporalSegments == UINT32_MAX
			|| ( plan->kind == VK_TEMPORAL_SHADER_WRITE
				&& owner->iqm.written == UINT32_MAX )
			|| ( plan->kind == VK_TEMPORAL_SHADER_INVALIDATE
				&& owner->iqm.invalidated == UINT32_MAX ) ) {
		owner->poisoned = qtrue; return qfalse;
	}
	taggedCandidate = owner->taggedSequence;
	verifierCandidate = owner->iqmVerifier;
	if ( !R_TemporalIqmSequenceVerifierCommit(
			&verifierCandidate, &plan->entry ) ) {
		owner->poisoned = qtrue; return qfalse;
	}
	if ( !TaggedAppend( &taggedCandidate, VK_TEMPORAL_MAIN_EVENT_IQM,
			plan->ordinal, plan->recordIndex, plan->kind,
			IqmFamilyDigest( plan->sequenceDigest, &plan->entry,
				&plan->factory ) ) ) {
		owner->poisoned = qtrue; return qfalse;
	}
	owner->taggedSequence = taggedCandidate;
	owner->iqmVerifier = verifierCandidate;
	owner->pending = qfalse; owner->pendingKind = 0;
	owner->iqm.prepared++; owner->iqm.temporalSegments++;
	owner->auxiliaryInitialized = qtrue;
	if ( plan->kind == VK_TEMPORAL_SHADER_WRITE ) owner->iqm.written++;
	else if ( plan->kind == VK_TEMPORAL_SHADER_INVALIDATE ) owner->iqm.invalidated++;
	else { owner->poisoned = qtrue; return qfalse; }
	memset( &owner->pendingIqmPlan, 0, sizeof( owner->pendingIqmPlan ) );
	return qtrue;
}

void VK_TemporalMainActivationPoison(
		vkTemporalMainActivationOwner_t *owner ) {
	if ( !owner || !owner->active ) return;
	owner->pending = qfalse;
	owner->pendingKind = 0;
	memset( &owner->pendingPlan, 0, sizeof( owner->pendingPlan ) );
	memset( &owner->pendingIqmPlan, 0, sizeof( owner->pendingIqmPlan ) );
	owner->poisoned = qtrue;
}

static qboolean FinishInternal(
		vkTemporalMainActivationOwner_t *owner,
		const vkTemporalMotionRecordingReceipt_t *recordingReceipt,
		const temporalIqmSequence_t *iqmSequence,
		const vkTemporalIqmPayloadOwner_t *payloadOwner,
		const vkTemporalIqmExact3FactoryReceipt_t *currentFactory,
		qboolean requireIqm ) {
	vkTemporalMainActivationReceipt_t candidate;
	vkTemporalMainIqmActivationReceipt_t iqmCandidate;
	temporalIqmSequenceVerifier_t verifierCandidate;
	qboolean genericValid, iqmValid, valid;
	if ( !owner || !owner->active ) return qfalse;
	memset( &candidate, 0, sizeof( candidate ) );
	memset( &iqmCandidate, 0, sizeof( iqmCandidate ) );
	genericValid = !owner->poisoned && !owner->pending && recordingReceipt
		&& recordingReceipt->ready
		&& AuthorityEqual( &owner->authority, &recordingReceipt->authority )
		&& recordingReceipt->prepared == owner->prepared
		&& recordingReceipt->appended == owner->written + owner->invalidated
		&& recordingReceipt->preserved == owner->preserved
		&& recordingReceipt->invalidated == owner->invalidated
		&& VK_TemporalMotionDrawSequenceEqual( &recordingReceipt->drawSequence,
			&owner->drawSequence )
		&& owner->temporalSegments == owner->written + owner->invalidated;
	if ( requireIqm ) {
		iqmCandidate = owner->iqm;
		verifierCandidate = owner->iqmVerifier;
		iqmCandidate.ready = qtrue;
		iqmValid = owner->iqmVerifier.expected == iqmSequence
			&& R_TemporalIqmSequenceExact(
				owner->iqmVerifier.expected, iqmSequence )
			&& SequenceMatchesIqm( iqmSequence, &owner->iqm )
			&& R_TemporalIqmSequenceVerifierFinish( &verifierCandidate )
			&& owner->iqm.drawCount && owner->iqm.prepared == owner->iqm.drawCount
			&& owner->iqm.temporalSegments == owner->iqm.prepared
			&& owner->iqm.temporalSegments ==
				owner->iqm.written + owner->iqm.invalidated
			&& payloadOwner
			&& VK_TemporalIqmPayloadContentRevalidate(
				&owner->iqm.content, payloadOwner )
			&& currentFactory
			&& currentFactory->payloadOwner == payloadOwner
			&& VK_TemporalIqmExact3FactoryReceiptExact(
				&owner->iqm.factory, currentFactory )
			&& IqmSummaryValid( &iqmCandidate );
	} else {
		iqmValid = IqmSummaryEmpty( &owner->iqm ) && !iqmSequence
			&& !payloadOwner && !currentFactory
			&& !owner->iqmVerifier.active && !owner->iqmVerifier.expected;
	}
	valid = genericValid && iqmValid && TaggedValid( &owner->taggedSequence )
		&& owner->taggedSequence.count ==
			owner->prepared + owner->iqm.prepared
		&& owner->taggedSequence.genericCount == owner->prepared
		&& owner->taggedSequence.iqmCount == owner->iqm.prepared
		&& owner->temporalSegments + owner->iqm.temporalSegments > 0
		&& owner->auxiliaryInitialized;
	if ( valid ) {
		candidate.authority = owner->authority;
		candidate.prepared = owner->prepared;
		candidate.temporalSegments = owner->temporalSegments;
		candidate.preserved = owner->preserved;
		candidate.written = owner->written;
		candidate.invalidated = owner->invalidated;
		candidate.drawSequence = owner->drawSequence;
		candidate.iqm = iqmCandidate;
		candidate.taggedSequence = owner->taggedSequence;
		candidate.auxiliaryCleared = qtrue;
		candidate.depthStoreRequired = owner->requireDepthStencilStore;
		candidate.stencilStoreRequired = owner->requireDepthStencilStore;
		candidate.ready = qtrue;
	}
	owner->active = qfalse;
	owner->pending = qfalse;
	owner->pendingKind = 0;
	owner->pendingReceipt = candidate;
	owner->submissionPending = valid;
	memset( &owner->pendingPlan, 0, sizeof( owner->pendingPlan ) );
	memset( &owner->pendingIqmPlan, 0, sizeof( owner->pendingIqmPlan ) );
	memset( &owner->iqm, 0, sizeof( owner->iqm ) );
	memset( &owner->taggedSequence, 0, sizeof( owner->taggedSequence ) );
	memset( &owner->iqmVerifier, 0, sizeof( owner->iqmVerifier ) );
	return valid;
}

qboolean VK_TemporalMainActivationFinish(
		vkTemporalMainActivationOwner_t *owner,
		const vkTemporalMotionRecordingReceipt_t *recordingReceipt ) {
	return FinishInternal( owner, recordingReceipt, NULL, NULL, NULL, qfalse );
}

qboolean VK_TemporalMainActivationFinishIqm(
		vkTemporalMainActivationOwner_t *owner,
		const vkTemporalMotionRecordingReceipt_t *recordingReceipt,
		const temporalIqmSequence_t *sequence,
		const vkTemporalIqmPayloadOwner_t *payloadOwner,
		const vkTemporalIqmExact3FactoryReceipt_t *currentFactory ) {
	return FinishInternal( owner, recordingReceipt, sequence, payloadOwner,
		currentFactory, qtrue );
}

static qboolean ActivationReceiptValid(
		const vkTemporalMainActivationReceipt_t *r ) {
	qboolean iqmValid;
	if ( !r || r->ready != qtrue || !AuthorityValid( &r->authority )
			|| r->prepared != r->drawSequence.count
			|| r->temporalSegments != r->written + r->invalidated
			|| r->prepared != r->temporalSegments + r->preserved
			|| !r->auxiliaryCleared || !r->depthStoreRequired
			|| !r->stencilStoreRequired ) return qfalse;
	iqmValid = IqmSummaryEmpty( &r->iqm ) || IqmSummaryValid( &r->iqm );
	return iqmValid
		&& r->taggedSequence.count
		&& r->taggedSequence.lane0 && r->taggedSequence.lane1
		&& r->taggedSequence.count == r->prepared + r->iqm.prepared
		&& r->taggedSequence.genericCount == r->prepared
		&& r->taggedSequence.iqmCount == r->iqm.prepared
		&& r->temporalSegments + r->iqm.temporalSegments > 0
		? qtrue : qfalse;
}

qboolean VK_TemporalMainActivationReceiptExact(
		const vkTemporalMainActivationReceipt_t *a,
		const vkTemporalMainActivationReceipt_t *b ) {
	return ActivationReceiptValid( a ) && ActivationReceiptValid( b )
		&& AuthorityEqual( &a->authority, &b->authority )
		&& a->prepared == b->prepared
		&& a->temporalSegments == b->temporalSegments
		&& a->preserved == b->preserved && a->written == b->written
		&& a->invalidated == b->invalidated
		&& VK_TemporalMotionDrawSequenceEqual( &a->drawSequence, &b->drawSequence )
		&& IqmSummaryExact( &a->iqm, &b->iqm )
		&& TaggedExact( &a->taggedSequence, &b->taggedSequence )
		&& a->auxiliaryCleared == b->auxiliaryCleared
		&& a->depthStoreRequired == b->depthStoreRequired
		&& a->stencilStoreRequired == b->stencilStoreRequired
		? qtrue : qfalse;
}

qboolean VK_TemporalMainActivationResolveSubmit(
		vkTemporalMainActivationOwner_t *owner, qboolean submitted ) {
	vkTemporalMainActivationReceipt_t receipt;
	if ( !owner || owner->active || !owner->submissionPending
			|| !owner->pendingReceipt.ready ) return qfalse;
	memset( &receipt, 0, sizeof( receipt ) );
	if ( submitted && ActivationReceiptValid( &owner->pendingReceipt ) )
		receipt = owner->pendingReceipt;
	memset( &owner->pendingReceipt, 0, sizeof( owner->pendingReceipt ) );
	owner->submissionPending = qfalse;
	owner->receipt = receipt;
	return submitted && receipt.ready ? qtrue : qfalse;
}

qboolean VK_TemporalMainActivationPeekPendingReceipt(
		const vkTemporalMainActivationOwner_t *owner,
		vkTemporalMainActivationReceipt_t *outReceipt ) {
	if ( !owner || !outReceipt || owner->active || !owner->submissionPending
			|| !ActivationReceiptValid( &owner->pendingReceipt ) ) return qfalse;
	*outReceipt = owner->pendingReceipt;
	return qtrue;
}

qboolean VK_TemporalMainActivationGetReceipt(
		const vkTemporalMainActivationOwner_t *owner,
		vkTemporalMainActivationReceipt_t *outReceipt ) {
	if ( !owner || !outReceipt
			|| !ActivationReceiptValid( &owner->receipt ) ) return qfalse;
	*outReceipt = owner->receipt;
	return qtrue;
}
