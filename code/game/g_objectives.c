// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 1999-2005 Id Software, Inc.
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
//
// g_objectives.c -- in-memory mission objectives + completion gate.
//
// A per-session objective list: each objective carries an l10n key (resolved to
// localized text on the cgame side for the objectives HUD), a `required` flag,
// and a `completed` flag. State lives in level_locals (reset on map load). The
// list is published to cgame via the CS_OBJECTIVES configstring as a string of
// "key,required,completed" triples joined by ';'.
//
// The completion gate is a popcount-style predicate: once every REQUIRED
// objective is complete, the in-session mission-complete condition latches (a
// flag + a log line). This is IN-SESSION ONLY — it is NOT serialized, NOT
// savegame-persisted, and NOT wired to cross-map campaign progression (that is
// the separate objectives-binding work, which attaches persistence to this state
// later).

#include "g_local.h"

// Rebuild CS_OBJECTIVES from the current list so cgame can resolve + render it.
// Each objective is a "key,required,completed,failed;" quadruple; the cgame
// resolves the key to localized text and forwards completed+failed to the HUD
// (the required flag is a server-side gate concern and is not forwarded).
static void G_Objectives_Publish( void ) {
	char	buf[MAX_STRING_CHARS];
	int		i;
	int		len = 0;

	buf[0] = '\0';
	for ( i = 0; i < level.numObjectives; i++ ) {
		const missionObjective_t *o = &level.objectives[i];
		char triple[OBJECTIVE_KEY_LEN + 12];
		int  tlen;

		Com_sprintf( triple, sizeof( triple ), "%s,%d,%d,%d;",
			o->key, o->required ? 1 : 0, o->completed ? 1 : 0, o->failed ? 1 : 0 );
		tlen = (int)strlen( triple );
		if ( len + tlen >= (int)sizeof( buf ) )
			break;   // out of room — publish what fits
		memcpy( buf + len, triple, tlen );
		len += tlen;
		buf[len] = '\0';
	}
	trap_SetConfigstring( CS_OBJECTIVES, buf );
}

// Completion gate: true once every REQUIRED objective is complete. A required
// objective that has FAILED can never be completed, so the gate can never latch
// (a required-but-failed objective blocks it permanently). A failed OPTIONAL
// objective has no gate effect. With no required objectives the gate is not
// considered met (nothing to complete).
static qboolean G_Objectives_AllRequiredComplete( void ) {
	int	i;
	int	requiredCount = 0;

	for ( i = 0; i < level.numObjectives; i++ ) {
		if ( !level.objectives[i].required )
			continue;
		requiredCount++;
		if ( level.objectives[i].failed )
			return qfalse;   // a failed required objective blocks the gate forever
		if ( !level.objectives[i].completed )
			return qfalse;
	}
	return ( requiredCount > 0 ) ? qtrue : qfalse;
}

// Re-evaluate the gate after any change; latch + announce once.
static void G_Objectives_CheckGate( void ) {
	if ( level.objectivesComplete )
		return;
	if ( G_Objectives_AllRequiredComplete() ) {
		level.objectivesComplete = qtrue;
		trap_Print( "objectives: all required objectives complete — mission objectives met\n" );
	}
}

// Add an objective by l10n key. `required` counts toward the gate. Duplicate
// keys and overflow are rejected. Returns the new index or -1. Republishes the
// list on success so the HUD reflects it — callers (verb or console command)
// need only this one call.
int G_Objectives_Add( const char *key, qboolean required ) {
	int	i;

	if ( !key || !key[0] )
		return -1;
	for ( i = 0; i < level.numObjectives; i++ ) {
		if ( !Q_stricmp( level.objectives[i].key, key ) )
			return -1;   // already present
	}
	if ( level.numObjectives >= MAX_MISSION_OBJECTIVES )
		return -1;

	i = level.numObjectives++;
	Q_strncpyz( level.objectives[i].key, key, sizeof( level.objectives[i].key ) );
	level.objectives[i].required  = required;
	level.objectives[i].completed = qfalse;
	level.objectives[i].failed    = qfalse;
	G_Objectives_Publish();
	return i;
}

// Mark objective `index` complete. Returns qtrue if it changed state. On a real
// change, republishes + re-evaluates the completion gate. A failed objective
// cannot be completed (failure is terminal).
qboolean G_Objectives_Complete( int index ) {
	if ( index < 0 || index >= level.numObjectives )
		return qfalse;
	if ( level.objectives[index].completed || level.objectives[index].failed )
		return qfalse;
	level.objectives[index].completed = qtrue;
	G_Objectives_Publish();
	G_Objectives_CheckGate();
	return qtrue;
}

// Mark objective `index` failed. Returns qtrue if it changed state. Completion
// and failure are mutually exclusive terminal states: an already-completed OR
// already-failed objective is a no-op (symmetric with Complete, which refuses a
// failed one). Because a completed objective can never later fail, a latched
// completion gate can never be retroactively invalidated — the one-way latch in
// CheckGate stays consistent. On a real change, republishes + re-evals the gate.
qboolean G_Objectives_Fail( int index ) {
	if ( index < 0 || index >= level.numObjectives )
		return qfalse;
	if ( level.objectives[index].failed || level.objectives[index].completed )
		return qfalse;
	level.objectives[index].failed = qtrue;
	G_Objectives_Publish();
	G_Objectives_CheckGate();
	return qtrue;
}

// Drop the whole list (and the gate latch), then republish the now-empty list.
void G_Objectives_Clear( void ) {
	level.numObjectives      = 0;
	level.objectivesComplete = qfalse;
	memset( level.objectives, 0, sizeof( level.objectives ) );
	G_Objectives_Publish();
}

/*
=================
G_Objectives_Command

Server console command `objective`:
  objective add <l10n-key> [optional]   register (default required; "optional" flag = not required)
  objective complete <index>            mark objective N complete
  objective clear                       drop the whole list

The simplest mechanism that proves the in-memory list + gate + HUD; a map-entity
or scene-verb hook could drive the same G_Objectives_* calls later.
=================
*/
qboolean G_Objectives_Command( void ) {
	char	sub[MAX_TOKEN_CHARS];

	trap_Argv( 1, sub, sizeof( sub ) );

	if ( !Q_stricmp( sub, "add" ) ) {
		char key[OBJECTIVE_KEY_LEN];
		char opt[16];
		qboolean required;
		int idx;

		trap_Argv( 2, key, sizeof( key ) );
		trap_Argv( 3, opt, sizeof( opt ) );
		required = ( Q_stricmp( opt, "optional" ) != 0 );
		idx = G_Objectives_Add( key, required );   // publishes on success
		if ( idx < 0 ) {
			trap_Print( va( "objective add: rejected '%s' (empty, duplicate, or full)\n", key  ) );
			return qtrue;
		}
		trap_Print( va( "objective add: [%d] '%s' (%s)\n", idx, key, required ? "required" : "optional"  ) );
		return qtrue;
	}

	if ( !Q_stricmp( sub, "complete" ) ) {
		char idxStr[16];
		int  idx;

		trap_Argv( 2, idxStr, sizeof( idxStr ) );
		idx = atoi( idxStr );
		if ( !G_Objectives_Complete( idx ) ) {   // publishes + checks gate on change
			trap_Print( va( "objective complete: [%d] not completed (bad index or already done)\n", idx  ) );
			return qtrue;
		}
		trap_Print( va( "objective complete: [%d] '%s' done\n", idx, level.objectives[idx].key  ) );
		return qtrue;
	}

	if ( !Q_stricmp( sub, "fail" ) ) {
		char idxStr[16];
		int  idx;

		trap_Argv( 2, idxStr, sizeof( idxStr ) );
		idx = atoi( idxStr );
		if ( !G_Objectives_Fail( idx ) ) {   // publishes + checks gate on change
			trap_Print( va( "objective fail: [%d] not failed (bad index, already failed, or already completed)\n", idx  ) );
			return qtrue;
		}
		trap_Print( va( "objective fail: [%d] '%s' failed\n", idx, level.objectives[idx].key  ) );
		return qtrue;
	}

	if ( !Q_stricmp( sub, "clear" ) ) {
		G_Objectives_Clear();   // publishes the now-empty list
		trap_Print( "objective clear: list emptied\n" );
		return qtrue;
	}

	trap_Print( "usage: objective add <l10n-key> [optional] | complete <index> | fail <index> | clear\n" );
	return qtrue;
}
