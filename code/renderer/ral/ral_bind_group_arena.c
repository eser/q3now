// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "ral_bind_group_arena.h"

#include <limits.h>
#include <string.h>

static void ralBindGroupArenaMakeReceipt(
		const ralBindGroupArenaLifecycle_t *lifecycle,
		ralBindGroupArenaReceipt_t *receipt ) {
	memset( receipt, 0, sizeof( *receipt ) );
	receipt->backendIdentity = lifecycle->backendIdentity;
	receipt->arenaIdentity = lifecycle->arenaIdentity;
	receipt->generation = lifecycle->generation;
	receipt->maxGroups = lifecycle->maxGroups;
	receipt->ready = lifecycle->ready;
}

qboolean Ral_BindGroupArenaReceiptValid(
		const ralBindGroupArenaReceipt_t *receipt ) {
	return receipt && receipt->backendIdentity && receipt->arenaIdentity
		&& receipt->generation != 0u && receipt->generation != UINT64_MAX
		&& receipt->maxGroups != 0u && receipt->ready == qtrue;
}

qboolean Ral_BindGroupArenaReceiptExact(
		const ralBindGroupArenaReceipt_t *a,
		const ralBindGroupArenaReceipt_t *b ) {
	return Ral_BindGroupArenaReceiptValid( a )
		&& Ral_BindGroupArenaReceiptValid( b )
		&& a->backendIdentity == b->backendIdentity
		&& a->arenaIdentity == b->arenaIdentity
		&& a->generation == b->generation
		&& a->maxGroups == b->maxGroups && a->ready == b->ready;
}

void Ral_BindGroupArenaLifecycleInit(
		ralBindGroupArenaLifecycle_t *lifecycle,
		const ralBackend_t *backend,
		const ralBindGroupArena_t *arena,
		uint32_t maxGroups ) {
	if ( !lifecycle ) return;
	memset( lifecycle, 0, sizeof( *lifecycle ) );
	if ( !backend || !arena || maxGroups == 0u ) return;
	lifecycle->backendIdentity = backend;
	lifecycle->arenaIdentity = arena;
	lifecycle->maxGroups = maxGroups;
}

ralResult_t Ral_BindGroupArenaLifecyclePublishCreate(
		ralBindGroupArenaLifecycle_t *lifecycle,
		ralBindGroupArenaReceipt_t *outReceipt ) {
	ralBindGroupArenaLifecycle_t candidate;
	ralBindGroupArenaReceipt_t receipt;
	if ( !lifecycle || !outReceipt || !lifecycle->backendIdentity
	  || !lifecycle->arenaIdentity || lifecycle->maxGroups == 0u
	  || lifecycle->ready != qfalse || lifecycle->generation != 0u )
		return ralErrorInvalidArgument;
	candidate = *lifecycle;
	candidate.generation = 1u;
	candidate.ready = qtrue;
	ralBindGroupArenaMakeReceipt( &candidate, &receipt );
	if ( !Ral_BindGroupArenaReceiptValid( &receipt ) )
		return ralErrorInvalidArgument;
	*lifecycle = candidate;
	*outReceipt = receipt;
	return ralSuccess;
}

ralResult_t Ral_BindGroupArenaLifecycleGetReceipt(
		const ralBindGroupArenaLifecycle_t *lifecycle,
		ralBindGroupArenaReceipt_t *outReceipt ) {
	ralBindGroupArenaReceipt_t receipt;
	if ( !lifecycle || !outReceipt || lifecycle->ready != qtrue )
		return ralErrorInvalidArgument;
	ralBindGroupArenaMakeReceipt( lifecycle, &receipt );
	if ( !Ral_BindGroupArenaReceiptValid( &receipt ) )
		return ralErrorInvalidArgument;
	*outReceipt = receipt;
	return ralSuccess;
}

ralResult_t Ral_BindGroupArenaLifecyclePublishReset(
		ralBindGroupArenaLifecycle_t *lifecycle,
		const ralBindGroupArenaReceipt_t *current,
		ralBindGroupArenaReceipt_t *outNext ) {
	ralBindGroupArenaLifecycle_t candidate;
	ralBindGroupArenaReceipt_t live, next;
	if ( !lifecycle || !current || !outNext || lifecycle->ready != qtrue
	  || lifecycle->generation >= UINT64_MAX - 1u )
		return ralErrorInvalidArgument;
	ralBindGroupArenaMakeReceipt( lifecycle, &live );
	if ( !Ral_BindGroupArenaReceiptExact( &live, current ) )
		return ralErrorInvalidArgument;
	candidate = *lifecycle;
	candidate.generation++;
	ralBindGroupArenaMakeReceipt( &candidate, &next );
	if ( !Ral_BindGroupArenaReceiptValid( &next ) )
		return ralErrorInvalidArgument;
	*lifecycle = candidate;
	*outNext = next;
	return ralSuccess;
}
