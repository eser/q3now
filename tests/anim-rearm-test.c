// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026-present Wired Engine contributors
//
// Contract for re-arming a cached animation id after the pool is wiped.
//
// Callers hold an anim id across frames and re-arm lazily. The obvious
// predicate — `if ( id == 0 ) create()` — cannot tell a live id from one that
// WUI_AnimStopAll cleared: the pool is memset, but the caller's own tracker
// still holds the old number, so the re-arm never fires and whatever the anim
// drove stays frozen for the rest of the session.
//
// That is not hypothetical. The menu background's parallax died to exactly
// this after every menu hot-reload, and it failed silently — nothing logged,
// the value simply stopped advancing. The comment in cl_wired_bg.c even
// claimed StopAll "clears the registration"; it never did.
//
// The pool is a fixed array of {id, ...} slots, so liveness is a lookup rather
// than something to remember. This models that lookup and the caller-side
// re-arm around it.

#include <stdio.h>
#include <string.h>

#define POOL_MAX 8

typedef struct { int id; } slot_t;

static slot_t pool[ POOL_MAX ];
static int    nextId = 1;

static int PoolCreate( void )
{
	int i;
	for ( i = 0; i < POOL_MAX; i++ ) {
		if ( pool[ i ].id == 0 ) {
			pool[ i ].id = nextId++;
			return pool[ i ].id;
		}
	}
	return 0;
}

static void PoolStopAll( void )
{
	/* Mirrors WUI_AnimStopAll: wipes the pool, and cannot reach any tracker
	 * a caller keeps in its own translation unit. */
	memset( pool, 0, sizeof( pool ) );
}

/* Mirrors WUI_AnimIsLive. */
static int PoolIsLive( int id )
{
	int i;
	if ( id == 0 ) return 0;
	for ( i = 0; i < POOL_MAX; i++ )
		if ( pool[ i ].id == id ) return 1;
	return 0;
}

/* The caller-side re-arm, written the way cl_wired_bg.c writes it. */
static void ReArm( int *tracker )
{
	if ( !PoolIsLive( *tracker ) )
		*tracker = PoolCreate();
}

static int failures = 0;

static void Check( const char *name, int got, int want )
{
	if ( got == want ) {
		printf( "  ok   %-46s -> %d\n", name, got );
	} else {
		printf( "  FAIL %-46s -> %d, want %d\n", name, got, want );
		failures++;
	}
}

int main( void )
{
	int tracker = 0;
	int first, second;

	memset( pool, 0, sizeof( pool ) );
	nextId = 1;

	/* Cold start: nothing cached, so arming allocates. */
	ReArm( &tracker );
	first = tracker;
	Check( "cold start arms", first != 0, 1 );
	Check( "arming registered it", PoolIsLive( first ), 1 );

	/* Steady state: re-arming an already-live id must not allocate again,
	 * or every frame would burn a slot. */
	ReArm( &tracker );
	Check( "re-arm on a live id is a no-op", tracker == first, 1 );

	/* 🔴 The regression. A hot reload wipes the pool; the tracker still holds
	 * the old number. Under `id == 0` this looked live and nothing re-armed. */
	PoolStopAll();
	Check( "tracker still holds the wiped id", tracker == first, 1 );
	Check( "but that id is no longer live", PoolIsLive( tracker ), 0 );

	ReArm( &tracker );
	second = tracker;
	Check( "re-arm after a wipe allocates again", second != 0, 1 );
	Check( "and the new id is live", PoolIsLive( second ), 1 );

	/* Two wipes in a row — the reload case that actually happens. */
	PoolStopAll();
	PoolStopAll();
	ReArm( &tracker );
	Check( "survives repeated wipes", PoolIsLive( tracker ), 1 );

	/* A tracker that was never armed is not live, and zero is never a live
	 * id — otherwise a fresh caller would skip its first arm. */
	Check( "zero is never live", PoolIsLive( 0 ), 0 );

	/* An id from a previous generation must not alias a recycled slot: after
	 * a wipe the pool hands out fresh numbers, so the old one stays dead. */
	Check( "the pre-wipe id stays dead", PoolIsLive( first ), 0 );

	if ( failures ) {
		printf( "==> FAIL: %d case(s)\n", failures );
		return 1;
	}
	printf( "==> PASS: anim re-arm contract\n" );
	return 0;
}
