// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "vk_temporal_iqm_geometry.h"

#include <limits.h>
#include <string.h>

static qboolean KeyValid( const vkTemporalIqmGeometryKey_t *key ) {
	return key && key->backend && key->nativeVertexBuffer
		&& key->nativeIndexBuffer
		&& key->nativeVertexBuffer != key->nativeIndexBuffer
		&& key->vertexBytes && key->vertexBytes <= SIZE_MAX
		&& key->indexBytes && key->indexBytes <= SIZE_MAX
		&& key->modelAllocationGeneration && key->geometryGeneration
		&& key->modelAllocationGeneration != UINT32_MAX
		&& key->geometryGeneration != UINT32_MAX
		&& key->contentDigest ? qtrue : qfalse;
}

static qboolean KeyEqual( const vkTemporalIqmGeometryKey_t *a,
		const vkTemporalIqmGeometryKey_t *b ) {
	return a->backend == b->backend
		&& a->nativeVertexBuffer == b->nativeVertexBuffer
		&& a->nativeIndexBuffer == b->nativeIndexBuffer
		&& a->vertexBytes == b->vertexBytes && a->indexBytes == b->indexBytes
		&& a->modelAllocationGeneration == b->modelAllocationGeneration
		&& a->geometryGeneration == b->geometryGeneration
		&& a->contentDigest == b->contentDigest ? qtrue : qfalse;
}

static qboolean ReceiptValid( const vkTemporalIqmGeometryReceipt_t *receipt,
		const vkTemporalIqmGeometryOps_t *ops ) {
	return receipt && receipt->ready == qtrue && KeyValid( &receipt->key )
		&& ops && ops->matchesNative
		&& receipt->vertex && receipt->index
		&& receipt->vertex != receipt->index
		&& (const void *)receipt->vertex != receipt->key.backend
		&& (const void *)receipt->index != receipt->key.backend
		&& (const void *)receipt->vertex != receipt->key.nativeVertexBuffer
		&& (const void *)receipt->vertex != receipt->key.nativeIndexBuffer
		&& (const void *)receipt->index != receipt->key.nativeVertexBuffer
		&& (const void *)receipt->index != receipt->key.nativeIndexBuffer
		&& ops->matchesNative( receipt->vertex,
			receipt->key.nativeVertexBuffer, (size_t)receipt->key.vertexBytes )
		&& ops->matchesNative( receipt->index,
			receipt->key.nativeIndexBuffer, (size_t)receipt->key.indexBytes )
		&& receipt->allocationGeneration
		&& receipt->allocationGeneration != UINT32_MAX ? qtrue : qfalse;
}

static qboolean Protected( const vkTemporalIqmGeometryReceipt_t *live,
		const vkTemporalIqmGeometryKey_t *key, const void *candidate ) {
	if ( !candidate ) return qfalse;
	if ( candidate == key->backend || candidate == key->nativeVertexBuffer
			|| candidate == key->nativeIndexBuffer ) return qtrue;
	return live && ( candidate == live->vertex || candidate == live->index
		|| candidate == live->key.backend
		|| candidate == live->key.nativeVertexBuffer
		|| candidate == live->key.nativeIndexBuffer ) ? qtrue : qfalse;
}

static void DestroyCandidate( const vkTemporalIqmGeometryReceipt_t *live,
		const vkTemporalIqmGeometryKey_t *key, ralBuffer_t *candidate,
		const vkTemporalIqmGeometryOps_t *ops ) {
	if ( candidate && !Protected( live, key, candidate )
			&& ops->candidateOwned( candidate, ops->candidateContext ) )
		ops->destroy( candidate );
}

void VK_TemporalIqmGeometryInit( vkTemporalIqmGeometryOwner_t *owner ) {
	if ( owner ) memset( owner, 0, sizeof( *owner ) );
}

qboolean VK_TemporalIqmGeometryNeedsIdle(
		const vkTemporalIqmGeometryOwner_t *owner,
		const vkTemporalIqmGeometryKey_t *key,
		const vkTemporalIqmGeometryOps_t *ops ) {
	if ( !owner || !KeyValid( key ) || !ReceiptValid( &owner->receipt, ops ) )
		return qfalse;
	return KeyEqual( &owner->receipt.key, key ) ? qfalse : qtrue;
}

qboolean VK_TemporalIqmGeometryEnsureAfterIdle(
		vkTemporalIqmGeometryOwner_t *owner,
		const vkTemporalIqmGeometryKey_t *key, qboolean idleProven,
		const vkTemporalIqmGeometryOps_t *ops ) {
	vkTemporalIqmGeometryReceipt_t candidate, live;
	uint32_t generation;
	if ( !owner || !KeyValid( key ) || !ops || !ops->adopt || !ops->destroy
			|| !ops->candidateOwned || !ops->matchesNative ) return qfalse;
	live = owner->receipt;
	if ( live.ready ) {
		if ( !ReceiptValid( &live, ops ) ) return qfalse;
		if ( KeyEqual( &live.key, key ) ) return qtrue;
		if ( idleProven != qtrue || live.allocationGeneration >= UINT32_MAX - 1u )
			return qfalse;
	}
	generation = live.ready ? live.allocationGeneration + 1u : 1u;
	memset( &candidate, 0, sizeof( candidate ) );
	candidate.key = *key;
	candidate.allocationGeneration = generation;
	candidate.vertex = ops->adopt( key->backend, key->nativeVertexBuffer,
		(size_t)key->vertexBytes, "temporal-iqm-vertex" );
	if ( !candidate.vertex || Protected( &live, key, candidate.vertex )
			|| !ops->candidateOwned( candidate.vertex, ops->candidateContext )
			|| !ops->matchesNative( candidate.vertex, key->nativeVertexBuffer,
				(size_t)key->vertexBytes ) ) {
		DestroyCandidate( &live, key, candidate.vertex, ops );
		return qfalse;
	}
	candidate.index = ops->adopt( key->backend, key->nativeIndexBuffer,
		(size_t)key->indexBytes, "temporal-iqm-index" );
	if ( !candidate.index || candidate.index == candidate.vertex
			|| Protected( &live, key, candidate.index )
			|| !ops->candidateOwned( candidate.index, ops->candidateContext )
			|| !ops->matchesNative( candidate.index, key->nativeIndexBuffer,
				(size_t)key->indexBytes ) ) {
		DestroyCandidate( &live, key, candidate.index, ops );
		if ( candidate.vertex != candidate.index )
			DestroyCandidate( &live, key, candidate.vertex, ops );
		return qfalse;
	}
	candidate.ready = qtrue;
	if ( !ReceiptValid( &candidate, ops ) ) {
		DestroyCandidate( &live, key, candidate.index, ops );
		DestroyCandidate( &live, key, candidate.vertex, ops );
		return qfalse;
	}
	owner->receipt = candidate;
	if ( live.ready ) {
		ops->destroy( live.index );
		ops->destroy( live.vertex );
	}
	return qtrue;
}

qboolean VK_TemporalIqmGeometryGetReceipt(
		const vkTemporalIqmGeometryOwner_t *owner,
		vkTemporalIqmGeometryReceipt_t *outReceipt,
		const vkTemporalIqmGeometryOps_t *ops ) {
	if ( !owner || !outReceipt || !ReceiptValid( &owner->receipt, ops ) ) return qfalse;
	*outReceipt = owner->receipt;
	return qtrue;
}

qboolean VK_TemporalIqmGeometryReleaseAfterIdle(
		vkTemporalIqmGeometryOwner_t *owner, qboolean idleProven,
		const vkTemporalIqmGeometryOps_t *ops ) {
	vkTemporalIqmGeometryReceipt_t live;
	if ( !owner || !ops || !ops->destroy ) return qfalse;
	if ( !owner->receipt.ready ) return qtrue;
	if ( idleProven != qtrue || !ReceiptValid( &owner->receipt, ops ) ) return qfalse;
	live = owner->receipt;
	memset( owner, 0, sizeof( *owner ) );
	ops->destroy( live.index );
	ops->destroy( live.vertex );
	return qtrue;
}
