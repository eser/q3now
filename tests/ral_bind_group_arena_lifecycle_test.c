// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "ral_bind_group_arena.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if ( !(x) ) { \
	fprintf( stderr, "CHECK failed at %s:%d: %s\n", __FILE__, __LINE__, #x ); \
	return 1; } } while ( 0 )

int main( void ) {
	ralBindGroupArenaLifecycle_t lifecycle;
	ralBindGroupArenaReceipt_t first, second, stale, sentinel, untouched;
	const ralBackend_t *backend = (const ralBackend_t *)(uintptr_t)0x10u;
	const ralBindGroupArena_t *arena = (const ralBindGroupArena_t *)(uintptr_t)0x20u;

	memset( &sentinel, 0x5a, sizeof( sentinel ) );
	untouched = sentinel;
	Ral_BindGroupArenaLifecycleInit( &lifecycle, backend, arena, 64u );
	CHECK( Ral_BindGroupArenaLifecycleGetReceipt( &lifecycle, &untouched )
		== ralErrorInvalidArgument );
	CHECK( memcmp( &untouched, &sentinel, sizeof( untouched ) ) == 0 );
	CHECK( Ral_BindGroupArenaLifecyclePublishCreate( &lifecycle, &first )
		== ralSuccess );
	CHECK( Ral_BindGroupArenaReceiptValid( &first )
		&& first.backendIdentity == backend && first.arenaIdentity == arena
		&& first.generation == 1u && first.maxGroups == 64u );
	CHECK( Ral_BindGroupArenaLifecyclePublishCreate( &lifecycle, &untouched )
		== ralErrorInvalidArgument );
	CHECK( Ral_BindGroupArenaLifecyclePublishReset( &lifecycle, &first, &second )
		== ralSuccess );
	CHECK( second.generation == 2u && Ral_BindGroupArenaReceiptExact( &second, &second )
		&& !Ral_BindGroupArenaReceiptExact( &first, &second ) );

	stale = second; stale.generation--;
	untouched = sentinel;
	CHECK( Ral_BindGroupArenaLifecyclePublishReset( &lifecycle, &stale, &untouched )
		== ralErrorInvalidArgument );
	CHECK( memcmp( &untouched, &sentinel, sizeof( untouched ) ) == 0 );
	stale = second; stale.maxGroups++;
	CHECK( !Ral_BindGroupArenaReceiptExact( &stale, &second ) );
	stale = second; stale.backendIdentity = (const ralBackend_t *)(uintptr_t)0x30u;
	CHECK( !Ral_BindGroupArenaReceiptExact( &stale, &second ) );
	stale = second; stale.arenaIdentity = (const ralBindGroupArena_t *)(uintptr_t)0x40u;
	CHECK( !Ral_BindGroupArenaReceiptExact( &stale, &second ) );
	stale = second; stale.ready = qfalse;
	CHECK( !Ral_BindGroupArenaReceiptValid( &stale ) );
	lifecycle.generation = UINT64_MAX - 1u;
	untouched = sentinel;
	CHECK( Ral_BindGroupArenaLifecyclePublishReset( &lifecycle, &second, &untouched )
		== ralErrorInvalidArgument );
	CHECK( memcmp( &untouched, &sentinel, sizeof( untouched ) ) == 0 );

	Ral_BindGroupArenaLifecycleInit( &lifecycle, NULL, arena, 64u );
	CHECK( Ral_BindGroupArenaLifecyclePublishCreate( &lifecycle, &untouched )
		== ralErrorInvalidArgument );
	Ral_BindGroupArenaLifecycleInit( &lifecycle, backend, NULL, 64u );
	CHECK( Ral_BindGroupArenaLifecyclePublishCreate( &lifecycle, &untouched )
		== ralErrorInvalidArgument );
	Ral_BindGroupArenaLifecycleInit( &lifecycle, backend, arena, 0u );
	CHECK( Ral_BindGroupArenaLifecyclePublishCreate( &lifecycle, &untouched )
		== ralErrorInvalidArgument );
	return 0;
}
