// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
//
// g_save_registry.c — the savegame callback registry engine (Phase-2).
//
// This is the name<->pointer resolution core: the central TIER-1 table, the
// TIER-2 file-local sub-table registry, and the two resolvers. It is deliberately
// GAME-TYPE-FREE — it includes only <string.h>, <assert.h>, <stddef.h> and
// g_save_funcs.h, NOT g_local.h. That has two payoffs:
//   1. The round-trip/completeness unit test compiles this exact source (the
//      shipping resolver code, not a copy) against stub symbols — no drift.
//   2. The registry logic carries zero dependency on the game ABI or the VM
//      traps, so it is trivially WASM-portable.
//
// The central table takes &Fn for the 65 extern-linkage callbacks; their
// prototypes come from g_save_funcs.h (active when SG_FUNCS_STANDALONE is unset).
// The 58 file-static callbacks are published by their own translation units via
// the SG_Register_<file>() hooks, walked once on first resolver use.
//
// Phase-2 is byte-identical: nothing calls a resolver yet (Phase-4 is the first
// caller), so the lazy registration never runs in a shipping frame.

#include <stddef.h>
#include <string.h>
#include <assert.h>

// This TU builds the central { name, &fn } table, so it needs the void-void
// prototypes for the 65 central callbacks — enabled by SG_REGISTRY_IMPL. (The 9
// game files that also include g_save_funcs.h must NOT define this: they include
// g_local.h with the real gentity_t prototypes, and a void-void redeclaration
// there would conflict.)
#define SG_REGISTRY_IMPL
#include "g_save_funcs.h"

// TIER 1 — the central { name, &fn } table (external-linkage callbacks).
static const sgCallback_t sg_centralCallbacks[] = {
	SG_CALLBACK_LIST( SG_CALLBACK_ROW )
};
static const size_t sg_numCentralCallbacks =
	sizeof( sg_centralCallbacks ) / sizeof( sg_centralCallbacks[0] );

// The registered TIER-2 sub-tables. Small fixed cap — there are 9 hooks today;
// the slack is generous and the array is file-static (no allocation).
#define SG_MAX_LOCAL_TABLES 16
static sgCallbackTable_t sg_localTables[SG_MAX_LOCAL_TABLES];
static size_t            sg_numLocalTables;
static int               sg_registryBuilt;

// The registration sink handed to each SG_Register_<file>() hook: it appends the
// file's sub-table to sg_localTables. The cap is a build-time invariant (9 hooks
// « 16), so an overflow is a programming error — assert, don't limp on.
static void SG_AddLocalTable( const sgCallback_t *table, size_t count ) {
	assert( sg_numLocalTables < SG_MAX_LOCAL_TABLES &&
		"SG_AddLocalTable: raise SG_MAX_LOCAL_TABLES" );
	if ( sg_numLocalTables >= SG_MAX_LOCAL_TABLES ) {
		return;
	}
	sg_localTables[sg_numLocalTables].table = table;
	sg_localTables[sg_numLocalTables].count = count;
	sg_numLocalTables++;
}

// The explicit init-list — every static-holding file's hook, invoked once. Adding
// a new static-holding file = add its SG_Register_<file> decl to g_save_funcs.h
// and one line here. Explicit (not a static/global constructor): C gives no
// cross-TU init-order guarantee and the WASM build has no constructor support.
static void SG_BuildRegistry( void ) {
	sg_numLocalTables = 0;
	SG_Register_g_trigger_q1( SG_AddLocalTable );
	SG_Register_g_mover_q1( SG_AddLocalTable );
	SG_Register_g_misc_q3( SG_AddLocalTable );
	SG_Register_g_misc_q1( SG_AddLocalTable );
	SG_Register_g_team( SG_AddLocalTable );
	SG_Register_g_target_q3( SG_AddLocalTable );
	SG_Register_g_mover_q3( SG_AddLocalTable );
	SG_Register_g_weapon( SG_AddLocalTable );
	SG_Register_g_missile( SG_AddLocalTable );
	sg_registryBuilt = 1;
}

static void SG_EnsureRegistry( void ) {
	if ( !sg_registryBuilt ) {
		SG_BuildRegistry();
	}
}

// SG_FunctionToName — callback address -> registered name. A NULL callback (a
// field with no handler) serializes as the empty token ""; an unknown non-NULL
// address (a save-time surprise) likewise yields "" rather than a stale address.
const char *SG_FunctionToName( void *ptr ) {
	size_t i, t;

	if ( ptr == NULL ) {
		return "";
	}
	SG_EnsureRegistry();

	for ( i = 0; i < sg_numCentralCallbacks; i++ ) {
		if ( sg_centralCallbacks[i].ptr == ptr ) {
			return sg_centralCallbacks[i].name;
		}
	}
	for ( t = 0; t < sg_numLocalTables; t++ ) {
		const sgCallback_t *tab = sg_localTables[t].table;
		for ( i = 0; i < sg_localTables[t].count; i++ ) {
			if ( tab[i].ptr == ptr ) {
				return tab[i].name;
			}
		}
	}
	return "";
}

// SG_NameToFunction — registered name -> live callback address. NULL/"" -> NULL
// (the "no callback" token). An UNKNOWN name -> NULL: a save referencing a
// callback this build lacks. On the LOAD path (Phase-6) that is a version/
// corruption signal and must HALT — Phase-2 only returns NULL here and leaves the
// HALT to the load-path caller (there is none yet).
void *SG_NameToFunction( const char *name ) {
	size_t i, t;

	if ( name == NULL || name[0] == '\0' ) {
		return NULL;
	}
	SG_EnsureRegistry();

	for ( i = 0; i < sg_numCentralCallbacks; i++ ) {
		if ( strcmp( sg_centralCallbacks[i].name, name ) == 0 ) {
			return sg_centralCallbacks[i].ptr;
		}
	}
	for ( t = 0; t < sg_numLocalTables; t++ ) {
		const sgCallback_t *tab = sg_localTables[t].table;
		for ( i = 0; i < sg_localTables[t].count; i++ ) {
			if ( strcmp( tab[i].name, name ) == 0 ) {
				return tab[i].ptr;
			}
		}
	}
	return NULL;
}
