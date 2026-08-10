// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

/*
===========================================================================
wired/core/thread/thread.c — portable thread + atomic primitives

Win32 (CreateThread / WaitForSingleObject) and POSIX (pthread_create /
pthread_join), plus a release/acquire atomic-int handoff. Links into both the
client and the headless server (no SDL, no external dep) — the navmesh bake
runs server-side in both builds and needs a thread primitive that reaches the
headless binary.
===========================================================================
*/

#include "thread.h"
#include "../../../qcommon.h"   /* Z_Malloc / Z_Free */

#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#  undef WIN32_LEAN_AND_MEAN
#else
#  include <pthread.h>
#endif

/* ── Thread handle ─────────────────────────────────────────────────────────
 * A tiny heap struct wrapping the platform handle plus the trampoline state,
 * so the caller holds one opaque pointer regardless of platform. */
struct sysThread_s {
	sysThreadFn_t fn;
	void         *arg;
#ifdef _WIN32
	HANDLE        handle;
#else
	pthread_t     handle;
#endif
};

#ifdef _WIN32
static DWORD WINAPI Sys_ThreadTrampoline( LPVOID p ) {
	sysThread_t *t = (sysThread_t *)p;
	t->fn( t->arg );
	return 0;
}
#else
static void *Sys_ThreadTrampoline( void *p ) {
	sysThread_t *t = (sysThread_t *)p;
	t->fn( t->arg );
	return NULL;
}
#endif

sysThread_t *Sys_CreateThread( sysThreadFn_t fn, void *arg ) {
	sysThread_t *t;

	if ( !fn ) {
		return NULL;
	}
	t = (sysThread_t *)Z_Malloc( sizeof( *t ) );
	t->fn  = fn;
	t->arg = arg;

#ifdef _WIN32
	t->handle = CreateThread( NULL, 0, Sys_ThreadTrampoline, t, 0, NULL );
	if ( !t->handle ) {
		Z_Free( t );
		return NULL;
	}
#else
	if ( pthread_create( &t->handle, NULL, Sys_ThreadTrampoline, t ) != 0 ) {
		Z_Free( t );
		return NULL;
	}
#endif
	return t;
}

void Sys_JoinThread( sysThread_t *t ) {
	if ( !t ) {
		return;
	}
#ifdef _WIN32
	WaitForSingleObject( t->handle, INFINITE );
	CloseHandle( t->handle );
#else
	pthread_join( t->handle, NULL );
#endif
	Z_Free( t );
}

/* ── Atomics ───────────────────────────────────────────────────────────────
 * GCC/Clang __atomic builtins cover both MinGW-Windows and POSIX (the shipping
 * compilers). The MSVC path uses Interlocked + an explicit barrier. */
#if defined(__GNUC__) || defined(__clang__)

void Sys_AtomicStore( sysAtomic32_t *a, int value ) {
	__atomic_store_n( a, value, __ATOMIC_RELEASE );
}
int Sys_AtomicLoad( const sysAtomic32_t *a ) {
	return __atomic_load_n( a, __ATOMIC_ACQUIRE );
}

#elif defined(_WIN32)

void Sys_AtomicStore( sysAtomic32_t *a, int value ) {
	InterlockedExchange( (volatile LONG *)a, (LONG)value );  /* full barrier */
}
int Sys_AtomicLoad( const sysAtomic32_t *a ) {
	int v = *a;
	MemoryBarrier();
	return v;
}

#else
#  error "no atomic primitive available for this compiler"
#endif
