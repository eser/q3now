// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "vk_temporal_iqm_payload_authoring.h"
#include "../../../core/ral_sync.h"

#include <math.h>
#include <string.h>

#define TEMPORAL_IQM_AUTHOR_FNV_OFFSET UINT64_C(1469598103934665603)
#define TEMPORAL_IQM_AUTHOR_FNV_PRIME UINT64_C(1099511628211)

static uint64_t FoldBytes( uint64_t hash, const void *data, size_t size ) {
	const unsigned char *bytes = (const unsigned char *)data;
	for ( size_t i = 0; i < size; ++i ) {
		hash ^= bytes[i]; hash *= TEMPORAL_IQM_AUTHOR_FNV_PRIME;
	}
	return hash;
}

static qboolean Finite( const float *values, size_t count ) {
	if ( !values ) return qfalse;
	for ( size_t i = 0; i < count; ++i ) if ( !isfinite( values[i] ) ) return qfalse;
	return qtrue;
}

static qboolean AuthorityExact( const temporalIqmSequenceAuthority_t *a,
		const temporalIqmSequenceAuthority_t *b ) {
	return a && b && a->token == b->token && a->frameId == b->frameId
		&& a->worldIndex == b->worldIndex && a->commandSlot == b->commandSlot
		&& a->frameCount == b->frameCount ? qtrue : qfalse;
}

static qboolean CameraExact( const temporalCameraPoseReceipt_t *a,
		const temporalCameraPoseReceipt_t *b ) {
	return a && b && a->frameId == b->frameId
		&& a->previousFrameId == b->previousFrameId
		&& memcmp( &a->current, &b->current, sizeof( a->current ) ) == 0
		&& memcmp( &a->previous, &b->previous, sizeof( a->previous ) ) == 0
		&& a->valid == b->valid && a->previousValid == b->previousValid
		? qtrue : qfalse;
}

static qboolean CameraValid( const temporalCameraPoseReceipt_t *camera ) {
	if ( !camera || camera->valid != qtrue || !camera->frameId
			|| !Finite( camera->current.projection, 16 )
			|| !Finite( camera->current.worldModel, 16 ) ) return qfalse;
	if ( camera->previousValid == qtrue )
		return camera->previousFrameId
			&& camera->frameId == camera->previousFrameId + 1u
			&& Finite( camera->previous.projection, 16 )
			&& Finite( camera->previous.worldModel, 16 ) ? qtrue : qfalse;
	return camera->previousValid == qfalse ? qtrue : qfalse;
}

static qboolean PoseExact( const temporalEntityPose_t *a,
		const temporalEntityPose_t *b ) {
	return a && b && a->hModel == b->hModel && a->modelToken == b->modelToken
		&& a->modelDataToken == b->modelDataToken && a->modelType == b->modelType
		&& a->modelTopology == b->modelTopology
		&& a->modelAllocationGeneration == b->modelAllocationGeneration
		&& a->modelContentDigest == b->modelContentDigest
		&& a->frame == b->frame && a->oldframe == b->oldframe
		&& memcmp( &a->backlerp, &b->backlerp, sizeof( a->backlerp ) ) == 0
		&& memcmp( a->origin, b->origin, sizeof( a->origin ) ) == 0
		&& memcmp( a->axis, b->axis, sizeof( a->axis ) ) == 0
		&& a->nonNormalizedAxes == b->nonNormalizedAxes ? qtrue : qfalse;
}

static const temporalIqmSequenceEntry_t *FirstRecordEntry(
		const temporalIqmSequence_t *sequence, uint32_t recordIndex ) {
	if ( !sequence || recordIndex >= sequence->entityCount ) return NULL;
	for ( uint32_t i = 0; i < sequence->drawCount; ++i )
		if ( sequence->entries[i].recordIndex == recordIndex ) return &sequence->entries[i];
	return NULL;
}

static uint64_t HashRecords( const void *mapped, uint32_t count ) {
	const unsigned char *bytes = (const unsigned char *)mapped;
	uint64_t hash = TEMPORAL_IQM_AUTHOR_FNV_OFFSET;
	if ( !mapped || !count || count > TEMPORAL_IQM_MAX_RECORDS ) return 0;
	for ( uint32_t i = 0; i < count; ++i ) {
		hash = FoldBytes( hash, &i, sizeof( i ) );
		hash = FoldBytes( hash, bytes + (size_t)i * TEMPORAL_IQM_RECORD_SIZE,
			TEMPORAL_IQM_RECORD_SIZE );
	}
	return hash;
}

static qboolean Fresh( const vkTemporalIqmPayloadAuthor_t *author ) {
	vkTemporalIqmPayloadAuthor_t fresh;
	if ( !author || author->initialized != qtrue ) return qfalse;
	memset( &fresh, 0, sizeof( fresh ) ); fresh.initialized = qtrue;
	return memcmp( author, &fresh, sizeof( fresh ) ) == 0 ? qtrue : qfalse;
}

void VK_TemporalIqmPayloadAuthorInit( vkTemporalIqmPayloadAuthor_t *author ) {
	if ( author ) { memset( author, 0, sizeof( *author ) ); author->initialized = qtrue; }
}

qboolean VK_TemporalIqmPayloadAuthorBegin(
		vkTemporalIqmPayloadAuthor_t *author,
		const temporalIqmSequence_t *sequence,
		const vkTemporalIqmPayloadReceipt_t *payload,
		const temporalCameraPoseReceipt_t *camera,
		const float jitteredProjection[16] ) {
	vkTemporalIqmPayloadAuthor_t candidate;
	if ( !Fresh( author ) || !sequence || !payload || !camera
			|| !R_TemporalIqmSequenceExact( sequence, sequence )
			|| !sequence->entityCount
			|| !VK_TemporalIqmPayloadReceiptExact( payload, payload )
			|| sequence->authority.commandSlot != payload->commandSlot
			|| sequence->authority.frameCount != payload->frameCount
			|| !CameraValid( camera )
			|| camera->frameId != sequence->authority.frameId
			|| !Finite( jitteredProjection, 16 ) ) return qfalse;
	memset( &candidate, 0, sizeof( candidate ) );
	candidate.sequence = *sequence; candidate.payload = *payload;
	candidate.camera = *camera;
	memcpy( candidate.jitteredProjection, jitteredProjection,
		sizeof( candidate.jitteredProjection ) );
	candidate.contentDigest = TEMPORAL_IQM_AUTHOR_FNV_OFFSET;
	candidate.initialized = qtrue; candidate.active = qtrue;
	*author = candidate; return qtrue;
}

qboolean VK_TemporalIqmPayloadAuthorWrite(
		vkTemporalIqmPayloadAuthor_t *author, uint32_t recordIndex,
		const temporalIqmModelView_t *model ) {
	const temporalIqmSequenceEntry_t *entry;
	temporalEntityPoseReceipt_t entity;
	temporalIqmGpuRecord_t record;
	qboolean previousValid;
	unsigned char *destination;
	if ( !author || author->initialized != qtrue || author->active != qtrue
			|| author->poisoned || author->sealed
			|| recordIndex != author->nextRecordIndex ) {
		if ( author && author->active == qtrue ) author->poisoned = qtrue;
		return qfalse;
	}
	entry = FirstRecordEntry( &author->sequence, recordIndex );
	if ( !entry || !model ) {
		author->poisoned = qtrue; return qfalse;
	}
	if ( model->modelDataToken != entry->facts.currentPose.modelDataToken
			|| model->contentDigest != entry->facts.modelContentDigest
			|| model->topologyGeneration != entry->facts.modelTopologyGeneration
			|| model->modelAllocationGeneration
				!= entry->facts.modelAllocationGeneration ) {
		author->poisoned = qtrue; return qfalse;
	}
	memset( &entity, 0, sizeof( entity ) );
	entity.frameId = author->sequence.authority.frameId;
	entity.previousFrameId = entry->facts.previousValid
		? author->sequence.authority.frameId - 1u : 0u;
	entity.identity = entry->facts.identity;
	entity.current = entry->facts.currentPose;
	entity.previous = entry->facts.previousPose;
	entity.valid = qtrue; entity.previousValid = entry->facts.previousValid;
	if ( !PoseExact( &entity.current, &entry->facts.currentPose )
			|| !PoseExact( &entity.previous, &entry->facts.previousPose )
			|| !R_TemporalIqmBuildGpuRecord( model, &author->camera, &entity,
				author->jitteredProjection, &record, &previousValid )
			|| previousValid != entry->facts.previousValid ) {
		author->poisoned = qtrue; return qfalse;
	}
	destination = (unsigned char *)author->payload.cpuShadowIdentity
		+ (size_t)recordIndex * TEMPORAL_IQM_RECORD_SIZE;
	memcpy( destination, &record, sizeof( record ) );
	author->contentDigest = FoldBytes( author->contentDigest,
		&recordIndex, sizeof( recordIndex ) );
	author->contentDigest = FoldBytes( author->contentDigest, &record, sizeof( record ) );
	author->nextRecordIndex++;
	return qtrue;
}

static qboolean ContentValid(
		const vkTemporalIqmPayloadContentReceipt_t *receipt ) {
	return receipt && receipt->ready == qtrue
		&& receipt->authority.token && receipt->authority.frameId
		&& receipt->authority.worldIndex >= 0 && receipt->authority.frameCount
		&& receipt->authority.commandSlot < receipt->authority.frameCount
		&& VK_TemporalIqmPayloadReceiptExact( &receipt->payload, &receipt->payload )
		&& receipt->payload.commandSlot == receipt->authority.commandSlot
		&& receipt->payload.frameCount == receipt->authority.frameCount
		&& CameraValid( &receipt->camera )
		&& receipt->camera.frameId == receipt->authority.frameId
		&& Finite( receipt->jitteredProjection, 16 )
		&& receipt->sequenceDigest && receipt->contentDigest
		&& receipt->recordCount && receipt->recordCount <= TEMPORAL_IQM_MAX_RECORDS
		? qtrue : qfalse;
}

qboolean VK_TemporalIqmPayloadAuthorSeal(
		vkTemporalIqmPayloadAuthor_t *author,
		vkTemporalIqmPayloadContentReceipt_t *outReceipt ) {
	vkTemporalIqmPayloadContentReceipt_t candidate;
	ralFence_t *upload;
	if ( !author || !outReceipt || author->initialized != qtrue
			|| author->active != qtrue || author->poisoned || author->sealed
			|| author->nextRecordIndex != author->sequence.entityCount
			|| author->contentDigest != HashRecords( author->payload.cpuShadowIdentity,
				author->sequence.entityCount ) ) {
		if ( author && author->active == qtrue ) author->poisoned = qtrue;
		return qfalse;
	}
	upload = Ral_BufferUploadAsync( author->payload.buffer, 0,
		author->payload.cpuShadowIdentity,
		(uint64_t)author->sequence.entityCount * TEMPORAL_IQM_RECORD_SIZE );
	if ( !upload ) { author->poisoned = qtrue; return qfalse; }
	Ral_WaitFence( upload, ~(uint64_t)0 );
	Ral_DestroyFence( upload );
	memset( &candidate, 0, sizeof( candidate ) );
	candidate.authority = author->sequence.authority;
	candidate.payload = author->payload; candidate.camera = author->camera;
	memcpy( candidate.jitteredProjection, author->jitteredProjection,
		sizeof( candidate.jitteredProjection ) );
	candidate.sequenceDigest = author->sequence.orderedDigest;
	candidate.contentDigest = author->contentDigest;
	candidate.recordCount = author->sequence.entityCount; candidate.ready = qtrue;
	if ( !ContentValid( &candidate ) ) { author->poisoned = qtrue; return qfalse; }
	author->active = qfalse; author->sealed = qtrue; *outReceipt = candidate;
	return qtrue;
}

qboolean VK_TemporalIqmPayloadContentReceiptExact(
		const vkTemporalIqmPayloadContentReceipt_t *a,
		const vkTemporalIqmPayloadContentReceipt_t *b ) {
	return ContentValid( a ) && ContentValid( b )
		&& AuthorityExact( &a->authority, &b->authority )
		&& VK_TemporalIqmPayloadReceiptExact( &a->payload, &b->payload )
		&& CameraExact( &a->camera, &b->camera )
		&& memcmp( a->jitteredProjection, b->jitteredProjection,
			sizeof( a->jitteredProjection ) ) == 0
		&& a->sequenceDigest == b->sequenceDigest
		&& a->contentDigest == b->contentDigest
		&& a->recordCount == b->recordCount && a->ready == b->ready
		? qtrue : qfalse;
}

qboolean VK_TemporalIqmPayloadContentRevalidate(
		const vkTemporalIqmPayloadContentReceipt_t *receipt,
		const vkTemporalIqmPayloadOwner_t *payloadOwner ) {
	vkTemporalIqmPayloadReceipt_t currentPayload;
	if ( !ContentValid( receipt ) || !payloadOwner
			|| !VK_TemporalIqmPayloadGetReceipt( payloadOwner,
				receipt->authority.commandSlot, &currentPayload ) ) return qfalse;
	return VK_TemporalIqmPayloadReceiptExact( &receipt->payload, &currentPayload )
		&& HashRecords( currentPayload.cpuShadowIdentity, receipt->recordCount )
			== receipt->contentDigest ? qtrue : qfalse;
}

qboolean VK_TemporalIqmPayloadAuthorCancel(
		vkTemporalIqmPayloadAuthor_t *author ) {
	if ( !author || author->initialized != qtrue ) return qfalse;
	VK_TemporalIqmPayloadAuthorInit( author ); return qtrue;
}
