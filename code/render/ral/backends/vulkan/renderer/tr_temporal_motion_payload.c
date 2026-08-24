// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "tr_temporal_motion_payload.h"
#include "../../../core/ral_sync.h"

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static qboolean FiniteMatrices( const temporalMotionMatrices_t *matrices ) {
	uint32_t i;
	if ( !matrices ) return qfalse;
	for ( i = 0; i < 16; ++i ) {
		if ( !isfinite( matrices->currentMvp[i] )
				|| !isfinite( matrices->previousMvp[i] ) ) return qfalse;
	}
	return qtrue;
}

static ralBindGroupLayout_t *CreateCompositeLayout( ralBackend_t *backend ) {
	ralBindEntry_t entries[2];
	ralBindGroupLayoutCreateInfo_t ci;
	memset( entries, 0, sizeof( entries ) );
	entries[0].binding = 0;
	entries[0].type = RAL_BIND_STORAGE_BUFFER;
	entries[0].count = 1;
	entries[0].stageFlags = RAL_STAGE_VERTEX;
	entries[1].binding = 1;
	entries[1].type = RAL_BIND_STORAGE_BUFFER;
	entries[1].count = 1;
	entries[1].stageFlags = RAL_STAGE_VERTEX;
	memset( &ci, 0, sizeof( ci ) );
	ci.entries = entries;
	ci.numEntries = 2;
	ci.bindless = qfalse;
	ci.debugName = "wired-temporal-motion-payload-layout";
	return Ral_CreateBindGroupLayout( backend, &ci );
}

static ralBindGroup_t *CreateCompositeGroup( ralBackend_t *backend,
		const ralBindGroupLayout_t *layout, const ralBuffer_t *entityBuffer,
		const ralBuffer_t *temporalBuffer ) {
	ralBindingValue_t values[2];
	ralBindGroupCreateInfo_t ci;
	memset( values, 0, sizeof( values ) );
	values[0].binding = 0;
	values[0].type = RAL_BIND_STORAGE_BUFFER;
	values[0].buffer = entityBuffer;
	values[0].bufferRange = 0;
	values[1].binding = 1;
	values[1].type = RAL_BIND_STORAGE_BUFFER;
	values[1].buffer = temporalBuffer;
	values[1].bufferRange = 0;
	memset( &ci, 0, sizeof( ci ) );
	ci.layout = layout;
	ci.values = values;
	ci.numValues = 2;
	ci.debugName = "wired-temporal-motion-payload-group";
	return Ral_CreateBindGroup( backend, &ci );
}

void R_TemporalMotionPayloadInit( temporalMotionPayloadOwner_t *owner ) {
	if ( owner ) memset( owner, 0, sizeof( *owner ) );
}

qboolean R_TemporalMotionPayloadEnsure( temporalMotionPayloadOwner_t *owner,
		ralBackend_t *backend, uint32_t frameCount, uint32_t frameIndex,
		uint32_t capacity, const ralBuffer_t *entityBuffer,
		uint32_t entityAllocationGeneration ) {
	temporalMotionPayloadFrame_t *frame;
	ralBindGroupLayout_t *candidateLayout = NULL;
	ralBuffer_t *candidateBuffer = NULL;
	ralBindGroup_t *candidateGroup = NULL;
	byte *candidateShadow = NULL;
	uint64_t bytes;
	uint32_t nextGeneration;
	uint32_t nextLayoutGeneration = 0;
	qboolean replaceBuffer;
	qboolean replacementResetAuthority;
	uint32_t i;

	if ( !owner || !backend || !entityBuffer || !entityAllocationGeneration
			|| frameCount == 0
			|| frameCount > TEMPORAL_MOTION_PAYLOAD_MAX_FRAMES
			|| frameIndex >= frameCount || capacity == 0
			|| capacity > TEMPORAL_MOTION_PAYLOAD_MAX_SLOTS ) return qfalse;
	bytes = (uint64_t)capacity * (uint64_t)sizeof( temporalMotionGpuPayload_t );
	if ( owner->ready && ( owner->backend != backend
			|| owner->frameCount != frameCount || !owner->layout ) ) return qfalse;

	frame = &owner->frames[frameIndex];
	for ( i = 0; i < owner->frameCount; ++i ) {
		if ( owner->frames[i].ready && entityBuffer == owner->frames[i].buffer )
			return qfalse;
		if ( i != frameIndex && owner->frames[i].ready
				&& entityBuffer == owner->frames[i].entityBuffer ) return qfalse;
	}
	if ( frame->ready && frame->capacity >= capacity
			&& frame->entityBuffer == entityBuffer
			&& frame->entityAllocationGeneration == entityAllocationGeneration
			&& frame->bindGroup ) return qtrue;
	if ( frame->ready && ( !frame->resetAfterFence || frame->begun ) ) return qfalse;
	if ( frame->allocationGeneration == UINT32_MAX ) return qfalse;
	nextGeneration = frame->allocationGeneration + 1u;
	replaceBuffer = ( !frame->ready || frame->capacity < capacity ) ? qtrue : qfalse;
	// First materialization has no prior in-flight payload allocation.  The
	// caller reached Ensure from the completed-fence seam, so publish the same
	// one-shot begin authority that a reused frame receives via ResetAfterFence.
	replacementResetAuthority = frame->ready ? frame->resetAfterFence : qtrue;
	if ( !owner->layout ) {
		if ( owner->layoutAllocationGeneration == UINT32_MAX ) return qfalse;
		nextLayoutGeneration = owner->layoutAllocationGeneration + 1u;
		candidateLayout = CreateCompositeLayout( backend );
		if ( !candidateLayout ) goto fail;
	}
	if ( replaceBuffer ) {
		ralBufferCreateInfo_t bci;
		memset( &bci, 0, sizeof( bci ) );
		bci.size = bytes;
		bci.usage = RAL_BUFFER_STORAGE | RAL_BUFFER_TRANSFER_DST;
		bci.memory = RAL_MEMORY_DEVICE_LOCAL;
		bci.debugName = "wired-temporal-motion-payload";
		candidateBuffer = Ral_CreateBuffer( backend, &bci );
		if ( !candidateBuffer ) goto fail;
		if ( candidateBuffer == entityBuffer ) {
			candidateBuffer = NULL;
			goto fail;
		}
		for ( i = 0; i < owner->frameCount; ++i ) {
			if ( owner->frames[i].ready
					&& ( candidateBuffer == owner->frames[i].buffer
						|| candidateBuffer == owner->frames[i].entityBuffer ) ) {
				candidateBuffer = NULL;
				goto fail;
			}
		}
		candidateShadow = (byte *)calloc( 1, (size_t)bytes );
		if ( !candidateShadow ) goto fail;
	} else {
		candidateBuffer = frame->buffer;
		candidateShadow = frame->cpuShadow;
	}
	candidateGroup = CreateCompositeGroup( backend,
		candidateLayout ? candidateLayout : owner->layout,
		entityBuffer, candidateBuffer );
	if ( !candidateGroup ) goto fail;
	for ( i = 0; i < owner->frameCount; ++i ) {
		if ( owner->frames[i].ready
				&& candidateGroup == owner->frames[i].bindGroup ) {
			candidateGroup = NULL;
			goto fail;
		}
	}

	if ( frame->bindGroup ) Ral_DestroyBindGroup( frame->bindGroup );
	if ( replaceBuffer && frame->buffer ) {
		free( frame->cpuShadow );
		Ral_DestroyBuffer( frame->buffer );
	}
	if ( candidateLayout ) {
		owner->layout = candidateLayout;
		owner->layoutAllocationGeneration = nextLayoutGeneration;
		candidateLayout = NULL;
	}
	frame->buffer = candidateBuffer;
	frame->bindGroup = candidateGroup;
	frame->cpuShadow = candidateShadow;
	frame->entityBuffer = entityBuffer;
	frame->entityAllocationGeneration = entityAllocationGeneration;
	if ( replaceBuffer ) frame->capacity = capacity;
	frame->allocationGeneration = nextGeneration;
	frame->lastSlot = 0;
	frame->hasAppends = qfalse;
	frame->resetAfterFence = replacementResetAuthority;
	frame->begun = qfalse;
	frame->ready = qtrue;
	owner->backend = backend;
	owner->frameCount = frameCount;
	owner->ready = qtrue;
	return qtrue;

fail:
	if ( candidateGroup && candidateGroup != frame->bindGroup )
		Ral_DestroyBindGroup( candidateGroup );
	if ( replaceBuffer && candidateBuffer && candidateBuffer != frame->buffer
			&& candidateBuffer != entityBuffer ) {
		free( candidateShadow );
		Ral_DestroyBuffer( candidateBuffer );
	}
	if ( candidateLayout ) Ral_DestroyBindGroupLayout( candidateLayout );
	return qfalse;
}

qboolean R_TemporalMotionPayloadResetAfterFence(
		temporalMotionPayloadOwner_t *owner, uint32_t frameIndex ) {
	temporalMotionPayloadFrame_t *frame;
	if ( !owner || !owner->ready || frameIndex >= owner->frameCount ) return qfalse;
	frame = &owner->frames[frameIndex];
	if ( !frame->ready ) return qfalse;
	frame->lastSlot = 0;
	frame->hasAppends = qfalse;
	frame->begun = qfalse;
	frame->resetAfterFence = qtrue;
	return qtrue;
}

qboolean R_TemporalMotionPayloadBeginFrame(
		temporalMotionPayloadOwner_t *owner, uint32_t frameIndex ) {
	temporalMotionPayloadFrame_t *frame;
	if ( !owner || !owner->ready || frameIndex >= owner->frameCount ) return qfalse;
	frame = &owner->frames[frameIndex];
	if ( !frame->ready || !frame->bindGroup || !frame->entityBuffer
			|| !frame->entityAllocationGeneration || !frame->resetAfterFence
			|| frame->begun ) return qfalse;
	frame->resetAfterFence = qfalse;
	frame->begun = qtrue;
	return qtrue;
}

qboolean R_TemporalMotionPayloadDetachEntityBuffer(
		temporalMotionPayloadOwner_t *owner, uint32_t frameIndex,
		const ralBuffer_t *entityBuffer, uint32_t entityAllocationGeneration ) {
	temporalMotionPayloadFrame_t *frame;
	if ( !owner || !owner->ready || !entityBuffer || !entityAllocationGeneration
			|| frameIndex >= owner->frameCount ) return qfalse;
	frame = &owner->frames[frameIndex];
	if ( !frame->ready || !frame->resetAfterFence || frame->begun
			|| frame->entityBuffer != entityBuffer
			|| frame->entityAllocationGeneration != entityAllocationGeneration
			|| !frame->bindGroup ) return qfalse;
	Ral_DestroyBindGroup( frame->bindGroup );
	frame->bindGroup = NULL;
	frame->entityBuffer = NULL;
	frame->entityAllocationGeneration = 0;
	frame->lastSlot = 0;
	frame->hasAppends = qfalse;
	return qtrue;
}

qboolean R_TemporalMotionPayloadAppendAt(
		temporalMotionPayloadOwner_t *owner, uint32_t frameIndex,
		uint32_t absoluteEntMatSlot, temporalMotionOutcome_t outcome,
		const temporalMotionMatrices_t *matrices, uint32_t *outSlot ) {
	temporalMotionPayloadFrame_t *frame;
	temporalMotionGpuPayload_t candidate;
	uint64_t offset;
	ralFence_t *upload;
	if ( !owner || !owner->ready || !outSlot || frameIndex >= owner->frameCount )
		return qfalse;
	frame = &owner->frames[frameIndex];
	if ( !frame->ready || !frame->begun || !frame->bindGroup || !frame->cpuShadow
			|| absoluteEntMatSlot >= frame->capacity
			|| ( frame->hasAppends && absoluteEntMatSlot <= frame->lastSlot ) ) {
		return qfalse;
	}
	memset( &candidate, 0, sizeof( candidate ) );
	switch ( outcome ) {
	case TEMPORAL_MOTION_WRITE_VALID:
		if ( !FiniteMatrices( matrices ) ) return qfalse;
		memcpy( candidate.currentMvp, matrices->currentMvp,
			sizeof( candidate.currentMvp ) );
		memcpy( candidate.previousMvp, matrices->previousMvp,
			sizeof( candidate.previousMvp ) );
		break;
	case TEMPORAL_MOTION_INVALIDATE_OPAQUE:
		if ( matrices ) return qfalse;
		break;
	default:
		return qfalse;
	}
	candidate.outcome = (uint32_t)outcome;
	offset = (uint64_t)absoluteEntMatSlot
		* (uint64_t)sizeof( temporalMotionGpuPayload_t );
	upload = Ral_BufferUploadAsync( frame->buffer, offset,
		&candidate, sizeof( candidate ) );
	if ( !upload ) return qfalse;
	Ral_WaitFence( upload, ~(uint64_t)0 );
	Ral_DestroyFence( upload );
	memcpy( frame->cpuShadow + offset, &candidate, sizeof( candidate ) );
	frame->lastSlot = absoluteEntMatSlot;
	frame->hasAppends = qtrue;
	*outSlot = absoluteEntMatSlot;
	return qtrue;
}

ralBindGroup_t *R_TemporalMotionPayloadGetBindGroup(
		const temporalMotionPayloadOwner_t *owner, uint32_t frameIndex ) {
	if ( !owner || !owner->ready || frameIndex >= owner->frameCount
			|| !owner->frames[frameIndex].ready ) return NULL;
	return owner->frames[frameIndex].bindGroup;
}

qboolean R_TemporalMotionPayloadGetLayout(
		const temporalMotionPayloadOwner_t *owner,
		const ralBindGroupLayout_t **outLayout, uint32_t *outGeneration ) {
	if ( !owner || !owner->ready || !owner->layout
			|| !owner->layoutAllocationGeneration || !outLayout || !outGeneration )
		return qfalse;
	*outLayout = owner->layout;
	*outGeneration = owner->layoutAllocationGeneration;
	return qtrue;
}

void R_TemporalMotionPayloadRelease( temporalMotionPayloadOwner_t *owner ) {
	uint32_t i;
	uint32_t layoutGeneration;
	if ( !owner ) return;
	layoutGeneration = owner->layoutAllocationGeneration;
	for ( i = 0; i < TEMPORAL_MOTION_PAYLOAD_MAX_FRAMES; ++i ) {
		if ( owner->frames[i].bindGroup ) {
			Ral_DestroyBindGroup( owner->frames[i].bindGroup );
			owner->frames[i].bindGroup = NULL;
		}
	}
	for ( i = 0; i < TEMPORAL_MOTION_PAYLOAD_MAX_FRAMES; ++i ) {
		if ( owner->frames[i].buffer ) {
			free( owner->frames[i].cpuShadow );
			Ral_DestroyBuffer( owner->frames[i].buffer );
		}
		memset( &owner->frames[i], 0, sizeof( owner->frames[i] ) );
	}
	if ( owner->layout ) Ral_DestroyBindGroupLayout( owner->layout );
	memset( owner, 0, sizeof( *owner ) );
	owner->layoutAllocationGeneration = layoutGeneration;
}
