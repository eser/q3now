// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 1999-2005 Id Software, Inc.
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

/*
===========================================================================
g_script_verbs.c -- scripted set-piece verb dispatcher (the drive layer).

A monster can be driven two ways. Autonomously, its behaviorState FSM decides
its own state each frame (g_behavior.c). Or by SCRIPT: a set-piece author gives
it an ordered list of verbs — spawncast, gotomarker, wait, playersight, attack —
and the monster executes them in order, ignoring the FSM. This file is that
script drive.

The runner is a re-entrant flag machine, NOT a coroutine. A script is a list of
{verb,args} items with an integer cursor (bs->scriptCursor) as its instruction
pointer. Each frame G_RunScriptDispatcher walks the list from the cursor and
calls each verb's C handler:

    handler returns qtrue  -> done: advance the cursor, run the next verb
    handler returns qfalse -> not done: SUSPEND — leave the cursor put and
                              re-enter this same handler next frame.

A blocking verb (wait / playersight / nosight / hold) simply returns qfalse
until its wake-condition is met, then qtrue. There is no saved context and no
yield: the "resume point" is the cursor, and re-entry re-runs the same handler,
which re-polls its wake-condition. `wait` is the canonical form — a bare
time-compare (scriptChangeTime + ms < level.time).

The verbs run game-side and call game-side symbols directly (G_Spawn, navGoal,
CanDamage, health) — no trap round-trip, no Lua closure resident per frame. The
script FILE is loaded once as data (a verb-list text block), parsed into the
item array, and thereafter it is pure C.
===========================================================================
*/

#include "g_local.h"
#include "g_behavior.h"

#if FEAT_MONSTER_AI

/* G_CallSpawn spawns an entity from its classname; defined in g_spawn.c but not
 * declared in a header — the generic `spawn` verb needs it. */
qboolean G_CallSpawn( gentity_t *ent );

LOG_DECLARE_CHANNEL( ch_script, "game.script" );

/* ── Script data model ────────────────────────────────────────────────────
 * A parsed script is a set of event-keyed action blocks; each block holds its
 * verbs (each a verb id + up to SCRIPT_MAX_ARGS string args, tokenized at load).
 * The cursor, timing, and accum slots live on behaviorState_t (pool-managed,
 * released with the monster); the parsed blocks live in a small game-private
 * pool keyed by the driven entity. Firing an event selects a block and runs it.
 * A bare flat verb-list (no `event {}` structure) is wrapped into a default
 * spawn block, so pre-migration scripts keep working unchanged. */
#define SCRIPT_MAX_ARGS   3
#define SCRIPT_ARG_LEN    64
#define SCRIPT_POOL_SIZE  MAX_MONSTERS   /* co-sized with the behavior pool */

typedef struct {
	int  verb;                              /* index into s_scriptVerbs[] */
	char arg[SCRIPT_MAX_ARGS][SCRIPT_ARG_LEN];
	int  argc;
} scriptItem_t;

/* An event-keyed action block: `event <name> [param] { verbs }`. A script is a
 * set of these; firing a matching event selects a block and runs its verbs from
 * the cursor. The paramless events (spawn/pain/death…) always match; a param
 * (e.g. a trigger name) is a string-equal predicate. */
#define SCRIPT_MAX_BLOCKS          16   /* event blocks per script            */
#define SCRIPT_MAX_ITEMS_PER_BLOCK 32   /* verbs per block                    */

typedef struct {
	int          eventNum;                  /* index into s_scriptEvents[] */
	char         param[SCRIPT_ARG_LEN];     /* "" = always match, else string-equal */
	int          itemCount;
	scriptItem_t items[SCRIPT_MAX_ITEMS_PER_BLOCK];
} scriptEventBlock_t;

typedef struct {
	qboolean           used;
	int                entityNum;           /* the monster this script drives */
	int                activeBlock;         /* index into blocks[], or -1 = idle */
	int                blockCount;
	qboolean           firedSight;          /* enemysight already fired (fire-once) */
	int                nextSightPoll;        /* level.time to next throttled sight check */
	scriptEventBlock_t blocks[SCRIPT_MAX_BLOCKS];
} scriptState_t;

static scriptState_t s_scripts[SCRIPT_POOL_SIZE];

/* Level-global accum: a shared integer buffer (one per level, not per cast) that
 * multiple scripted monsters read/write to coordinate — e.g. "3 guards dead ->
 * open the door". Same op-vocabulary as the per-cast accum. Reset each map in
 * Script_ResetPool. */
#define SCRIPT_GLOBAL_ACCUM_SLOTS 8
static int s_scriptGlobalAccum[SCRIPT_GLOBAL_ACCUM_SLOTS];

/* The survival wave counter: a single level-global int the `wave` verb mutates.
 * Its gameplay CONSUMER is the (deferred) survival fork — this ships the counter
 * + the op so set-pieces can drive it; nothing reads it for spawning yet. */
static int s_scriptWaveCount;

/* ── Event vocabulary ──────────────────────────────────────────────────────
 * The events a scripted monster reacts to. Each is a real fire site (no dead
 * vocabulary): spawn (at bind), enemysight/sight (on first player-sight), pain
 * (on damage), death (on kill), trigger (a named cross-script signal). An event
 * with a param uses a string-equal predicate on the block's param; the rest
 * always match. Firing an event selects the matching block and runs it. */
typedef struct { const char *name; } scriptEventDef_t;
static const scriptEventDef_t s_scriptEvents[] = {
	{ "spawn"      },
	{ "enemysight" },
	{ "sight"      },
	{ "pain"       },
	{ "death"      },
	{ "trigger"    },
};
static const int s_scriptEventCount = (int)( sizeof( s_scriptEvents ) / sizeof( s_scriptEvents[0] ) );

static int Script_FindEvent( const char *name )
{
	int i;
	for ( i = 0; i < s_scriptEventCount; i++ ) {
		if ( !Q_stricmp( name, s_scriptEvents[i].name ) ) return i;
	}
	return -1;
}

/* Named-trigger latch: `trigger <name>` sets a bit; a script waiting on that
 * name (not wired into a verb yet — reserved for the trigger verb + future
 * event verbs) reads it. Kept minimal for the skeleton. */
#define SCRIPT_MAX_TRIGGERS 16
static char s_firedTriggers[SCRIPT_MAX_TRIGGERS][SCRIPT_ARG_LEN];
static int  s_firedTriggerCount;

/* Deterministic RNG seed for `accum random`. Advanced by Q_random (a seeded LCG,
 * NOT the wall-clock rand() macro) so a replay/demo and the nav-gate stay
 * reproducible. Re-seeded each map in Script_ResetPool so runs are identical. */
static int s_scriptRandSeed;
#define SCRIPT_RAND_SEED_INIT 0x53637270   /* 'Scrp' */

/* ── Verb registry ─────────────────────────────────────────────────────────
 * A verb is a C handler in the re-entry contract: qtrue = done (advance cursor),
 * qfalse = suspend (re-enter next frame). The registry is a static name->fn
 * table; the runner looks a verb up once at parse time and stores the index. */
typedef qboolean (*scriptVerbFn)( gentity_t *ent, behaviorState_t *bs, scriptItem_t *item );

typedef struct {
	const char   *name;
	scriptVerbFn  fn;
} scriptVerbDef_t;

/* forward decls for the handlers (defined below the runner) */
static qboolean Verb_SpawnCast   ( gentity_t *ent, behaviorState_t *bs, scriptItem_t *it );
static qboolean Verb_Spawn       ( gentity_t *ent, behaviorState_t *bs, scriptItem_t *it );
static qboolean Verb_Hold        ( gentity_t *ent, behaviorState_t *bs, scriptItem_t *it );
static qboolean Verb_NoSight     ( gentity_t *ent, behaviorState_t *bs, scriptItem_t *it );
static qboolean Verb_GotoMarker  ( gentity_t *ent, behaviorState_t *bs, scriptItem_t *it );
static qboolean Verb_WalkToMarker( gentity_t *ent, behaviorState_t *bs, scriptItem_t *it );
static qboolean Verb_Trigger     ( gentity_t *ent, behaviorState_t *bs, scriptItem_t *it );
static qboolean Verb_WaitForTrigger( gentity_t *ent, behaviorState_t *bs, scriptItem_t *it );
static qboolean Verb_PlayerSight ( gentity_t *ent, behaviorState_t *bs, scriptItem_t *it );
static qboolean Verb_Attack      ( gentity_t *ent, behaviorState_t *bs, scriptItem_t *it );
static qboolean Verb_NoAttack    ( gentity_t *ent, behaviorState_t *bs, scriptItem_t *it );
static qboolean Verb_Accum       ( gentity_t *ent, behaviorState_t *bs, scriptItem_t *it );
static qboolean Verb_Wait        ( gentity_t *ent, behaviorState_t *bs, scriptItem_t *it );
static qboolean Verb_Print       ( gentity_t *ent, behaviorState_t *bs, scriptItem_t *it );
static qboolean Verb_SetHealth   ( gentity_t *ent, behaviorState_t *bs, scriptItem_t *it );
static qboolean Verb_GiveWeapon  ( gentity_t *ent, behaviorState_t *bs, scriptItem_t *it );
static qboolean Verb_SelectWeapon( gentity_t *ent, behaviorState_t *bs, scriptItem_t *it );
static qboolean Verb_ChangeLevel ( gentity_t *ent, behaviorState_t *bs, scriptItem_t *it );
static qboolean Verb_FaceAngles  ( gentity_t *ent, behaviorState_t *bs, scriptItem_t *it );
static qboolean Verb_ObjectiveAdd     ( gentity_t *ent, behaviorState_t *bs, scriptItem_t *it );
static qboolean Verb_ObjectiveComplete( gentity_t *ent, behaviorState_t *bs, scriptItem_t *it );
static qboolean Verb_ObjectiveFail    ( gentity_t *ent, behaviorState_t *bs, scriptItem_t *it );
static qboolean Verb_ObjectiveClear   ( gentity_t *ent, behaviorState_t *bs, scriptItem_t *it );
static qboolean Verb_PlayScene        ( gentity_t *ent, behaviorState_t *bs, scriptItem_t *it );
static qboolean Verb_StopScene        ( gentity_t *ent, behaviorState_t *bs, scriptItem_t *it );
static qboolean Verb_WaitScene        ( gentity_t *ent, behaviorState_t *bs, scriptItem_t *it );
static qboolean Verb_GlobalAccum      ( gentity_t *ent, behaviorState_t *bs, scriptItem_t *it );
static qboolean Verb_Wave             ( gentity_t *ent, behaviorState_t *bs, scriptItem_t *it );
static qboolean Verb_WaitPlayerRadius ( gentity_t *ent, behaviorState_t *bs, scriptItem_t *it );
static qboolean Verb_Invuln           ( gentity_t *ent, behaviorState_t *bs, scriptItem_t *it );
static qboolean Verb_Shock             ( gentity_t *ent, behaviorState_t *bs, scriptItem_t *it );
static int      Script_ResolveCharacter( const char *name );

/* The core set-piece + scene-control verbs. The full ~135-verb vocabulary —
 * CINEMATIC / SURVIVAL / the remaining aicast verbs — is added by category in a
 * later phase; the MISSION category (objective_*) drives the in-memory
 * g_objectives system. */
static const scriptVerbDef_t s_scriptVerbs[] = {
	{ "spawncast",      Verb_SpawnCast      },
	{ "spawn",          Verb_Spawn          },
	{ "hold",           Verb_Hold           },
	{ "nosight",        Verb_NoSight        },
	{ "gotomarker",     Verb_GotoMarker     },
	{ "walktomarker",   Verb_WalkToMarker   },
	{ "trigger",        Verb_Trigger        },
	{ "waitfortrigger", Verb_WaitForTrigger },
	{ "playersight",    Verb_PlayerSight    },
	{ "waitplayerradius", Verb_WaitPlayerRadius },
	{ "attack",         Verb_Attack         },
	{ "noattack",       Verb_NoAttack       },
	{ "invuln",         Verb_Invuln         },
	{ "shock",          Verb_Shock          },
	{ "accum",          Verb_Accum          },
	{ "wait",           Verb_Wait           },
	{ "print",          Verb_Print          },
	{ "sethealth",      Verb_SetHealth      },
	{ "giveweapon",     Verb_GiveWeapon     },
	{ "selectweapon",   Verb_SelectWeapon   },
	{ "changelevel",    Verb_ChangeLevel    },
	{ "faceangles",     Verb_FaceAngles     },
	/* MISSION category — drive the in-memory objectives system (g_objectives.c). */
	{ "objective_add",      Verb_ObjectiveAdd      },
	{ "objective_complete", Verb_ObjectiveComplete },
	{ "objective_fail",     Verb_ObjectiveFail     },
	{ "objective_clear",    Verb_ObjectiveClear    },
	/* CINEMATIC category — trigger the client-side WiredScene director. */
	{ "playscene",          Verb_PlayScene         },
	{ "stopscene",          Verb_StopScene         },
	{ "waitscene",          Verb_WaitScene         },
	/* accum families — level-global slots + the survival wave counter. */
	{ "globalaccum",        Verb_GlobalAccum       },
	{ "wave",               Verb_Wave              },
};
static const int s_scriptVerbCount = (int)( sizeof( s_scriptVerbs ) / sizeof( s_scriptVerbs[0] ) );

/* Look a verb name up in the registry; -1 if unknown. */
static int Script_FindVerb( const char *name )
{
	int i;
	for ( i = 0; i < s_scriptVerbCount; i++ ) {
		if ( !Q_stricmp( name, s_scriptVerbs[i].name ) ) {
			return i;
		}
	}
	return -1;
}

/* ── Pool lifecycle ────────────────────────────────────────────────────────
 * Script slots are drawn on demand, keyed by the driven entity, and returned
 * when that entity dies/frees (Script_ReleaseForEntity, called from
 * G_FreeEntity) or at map init (Script_ResetPool). Balanced with the behavior
 * pool it rides alongside. */
void Script_ResetPool( void )
{
	memset( s_scripts, 0, sizeof( s_scripts ) );
	memset( s_firedTriggers, 0, sizeof( s_firedTriggers ) );
	memset( s_scriptGlobalAccum, 0, sizeof( s_scriptGlobalAccum ) );
	s_firedTriggerCount = 0;
	s_scriptWaveCount   = 0;
	s_scriptRandSeed    = SCRIPT_RAND_SEED_INIT;   /* deterministic each map */
}

static scriptState_t *Script_AcquireFor( int entityNum )
{
	int i;
	for ( i = 0; i < SCRIPT_POOL_SIZE; i++ ) {
		if ( !s_scripts[i].used ) {
			memset( &s_scripts[i], 0, sizeof( s_scripts[i] ) );
			s_scripts[i].used      = qtrue;
			s_scripts[i].entityNum = entityNum;
			return &s_scripts[i];
		}
	}
	Com_Log( SEV_WARN, LOG_CH(ch_script),
		"script pool exhausted (%d slots) — entity %d gets no script\n",
		SCRIPT_POOL_SIZE, entityNum );
	return NULL;
}

static scriptState_t *Script_FindFor( int entityNum )
{
	int i;
	for ( i = 0; i < SCRIPT_POOL_SIZE; i++ ) {
		if ( s_scripts[i].used && s_scripts[i].entityNum == entityNum ) {
			return &s_scripts[i];
		}
	}
	return NULL;
}

void Script_ReleaseForEntity( gentity_t *ent )
{
	scriptState_t *sc;
	if ( !ent ) return;
	sc = Script_FindFor( (int)( ent - g_entities ) );
	if ( sc ) {
		sc->used = qfalse;
	}
}

/* ── Named triggers ────────────────────────────────────────────────────────
 * `trigger <name>` latches a name; the skeleton keeps a small fired-list a
 * future wait-for-trigger verb can poll. Duplicate names are a no-op. */
static void Script_FireTrigger( const char *name )
{
	int i;
	if ( !name || !name[0] ) return;
	for ( i = 0; i < s_firedTriggerCount; i++ ) {
		if ( !Q_stricmp( s_firedTriggers[i], name ) ) return;   /* already fired */
	}
	if ( s_firedTriggerCount < SCRIPT_MAX_TRIGGERS ) {
		Q_strncpyz( s_firedTriggers[s_firedTriggerCount], name, SCRIPT_ARG_LEN );
		s_firedTriggerCount++;
	}
}

/* Has the named trigger fired this map? The waitfortrigger verb polls this to
 * decide when to resume — the consumer half of the trigger/waitfortrigger loop. */
static qboolean Script_TriggerFired( const char *name )
{
	int i;
	if ( !name || !name[0] ) return qfalse;
	for ( i = 0; i < s_firedTriggerCount; i++ ) {
		if ( !Q_stricmp( s_firedTriggers[i], name ) ) return qtrue;
	}
	return qfalse;
}

/* ── Event dispatch ────────────────────────────────────────────────────────
 * Fire an event on a scripted monster: select the first block whose event
 * matches AND whose param matches (empty param = always), reset the cursor, and
 * let the runner walk it next frame. Only touches scripted monsters; an
 * autonomous monster or bot never reaches a fire site. Called both internally
 * (sight poll) and from the game-side hooks (pain in G_Damage, death in the die
 * callback, trigger from the trigger verb). Declared in g_local.h so the hooks
 * in other TUs can reach it. */
void Script_FireEvent( gentity_t *ent, const char *eventName, const char *param )
{
	behaviorState_t *bs;
	scriptState_t   *sc;
	int              ev, i;

	if ( !ent || !ent->behaviorState || !ent->behaviorState->scripted ) return;
	bs = ent->behaviorState;
	sc = Script_FindFor( (int)( ent - g_entities ) );
	if ( !sc ) return;

	ev = Script_FindEvent( eventName );
	if ( ev < 0 ) return;
	{
		wiredEntityEvent_t event;
		memset( &event, 0, sizeof( event ) );
		event.schemaVersion = WIRED_ENTITY_EVENT_SCHEMA_VERSION;
		event.stableEventId = WiredEntityEvent_NameId( eventName );
		event.gameTime = level.time;
		event.entityNum = (int)( ent - g_entities );
		event.sourceEntityNum = event.entityNum;
		event.valueType = ( param && param[0] ) ? WIRED_ENTITY_EVENT_VALUE_STRING :
			WIRED_ENTITY_EVENT_VALUE_NONE;
		Q_strncpyz( event.name, eventName, sizeof( event.name ) );
		if ( param && param[0] ) Q_strncpyz( event.textValue, param, sizeof( event.textValue ) );
		event.ready = qtrue;
		(void)trap_EntityEventEnqueue( &event );
	}

	for ( i = 0; i < sc->blockCount; i++ ) {
		scriptEventBlock_t *blk = &sc->blocks[i];
		if ( blk->eventNum != ev ) continue;
		/* param match: empty block-param always matches; else string-equal */
		if ( blk->param[0] && ( !param || Q_stricmp( blk->param, param ) != 0 ) ) continue;

		sc->activeBlock      = i;
		bs->scriptCursor     = 0;
		bs->scriptChangeTime = level.time;
		/* Anchor the scene timer to block entry so a `waitscene` in a block that has
		 * no `playscene` of its own waits from now (fail-closed) rather than passing
		 * instantly on a stale stamp left by an earlier block. A `playscene` in this
		 * block re-stamps it to its own fire time. */
		bs->sceneStartTime   = level.time;
		if ( g_cheats.integer ) {
			Com_Log( SEV_DEBUG, LOG_CH(ch_script), "monster %d event %s%s%s -> block %d\n",
				(int)( ent - g_entities ), eventName,
				( param && param[0] ) ? " " : "", ( param && param[0] ) ? param : "", i );
		}
		return;
	}
	/* no matching block — the event is simply ignored for this monster */
}

/* Poll the sensory events a scripted monster fires for itself. A scripted
 * monster skips Behavior_Decide, so the runner polls sight here (throttled) and
 * fires enemysight/sight once when it first sees a live player. */
static void Script_PollEvents( gentity_t *ent, behaviorState_t *bs, scriptState_t *sc )
{
	int    i;
	vec3_t eye;

	if ( sc->firedSight ) return;                 /* fire-once */
	if ( level.time < sc->nextSightPoll ) return; /* throttle */
	sc->nextSightPoll = level.time + 150;
	(void)bs;

	VectorCopy( ent->r.currentOrigin, eye );
	eye[2] += 48.0f;                              /* monster eye height */
	for ( i = 0; i < level.maxclients; i++ ) {
		gentity_t *pl = &g_entities[i];
		if ( !pl->inuse || !pl->client || pl->health <= 0 ) continue;
		if ( CanDamage( pl, eye ) ) {
			sc->firedSight = qtrue;
			bs->enemy = pl->s.number;              /* latch the seen player */
			Script_FireEvent( ent, "enemysight", NULL );
			Script_FireEvent( ent, "sight", NULL );  /* alias; no-op if no block */
			return;
		}
	}
}

/* ── Parser ────────────────────────────────────────────────────────────────
 * A script is a set of event blocks:  event <name> [param] { verb args ... }.
 * Tokens are whitespace/newline-separated; `//` runs to end of line; `{` and `}`
 * are standalone tokens. A bare flat verb-list (the first meaningful token is a
 * VERB, not an event-block opener) is wrapped into an implicit spawn block so
 * pre-migration scripts keep working unchanged. */

/* Pull one token from the whole-buffer stream into out; advance *pp. Skips
 * whitespace, newlines, and `//`-to-end-of-line comments. Returns `{` and `}` as
 * single-char tokens. qfalse at end of stream. */
static qboolean Script_StreamToken( const char **pp, char *out, int outSize )
{
	const char *p = *pp;
	int         n = 0;

	for ( ;; ) {
		while ( *p == ' ' || *p == '\t' || *p == '\r' || *p == '\n' ) p++;
		if ( p[0] == '/' && p[1] == '/' ) {              /* line comment */
			while ( *p && *p != '\n' ) p++;
			continue;
		}
		break;
	}
	if ( *p == '\0' ) { *pp = p; return qfalse; }

	if ( *p == '{' || *p == '}' ) {                      /* brace = its own token */
		out[0] = *p; out[1] = '\0';
		*pp = p + 1;
		return qtrue;
	}
	while ( *p && *p != ' ' && *p != '\t' && *p != '\r' && *p != '\n'
	        && *p != '{' && *p != '}' && n < outSize - 1 ) {
		out[n++] = *p++;
	}
	out[n] = '\0';
	*pp = p;
	return ( n > 0 );
}

/* Peek without consuming. */
static qboolean Script_PeekToken( const char *p, char *out, int outSize )
{
	return Script_StreamToken( &p, out, outSize );
}

/* Parse the verbs of one block until a closing `}` (or end of stream). */
static void Script_ParseBlockVerbs( scriptEventBlock_t *blk, const char **pp )
{
	char tok[SCRIPT_ARG_LEN];

	blk->itemCount = 0;
	while ( Script_StreamToken( pp, tok, sizeof( tok ) ) ) {
		int verb;
		scriptItem_t *it;

		if ( !Q_stricmp( tok, "}" ) ) return;            /* block end */

		verb = Script_FindVerb( tok );
		if ( verb < 0 ) {
			Com_Log( SEV_WARN, LOG_CH(ch_script), "unknown script verb '%s' (skipped)\n", tok );
			/* skip this verb's args up to the next verb/brace by peeking */
			continue;
		}
		if ( blk->itemCount >= SCRIPT_MAX_ITEMS_PER_BLOCK ) {
			Com_Log( SEV_WARN, LOG_CH(ch_script), "block full (%d verbs) — extra dropped\n",
				SCRIPT_MAX_ITEMS_PER_BLOCK );
			continue;
		}
		it = &blk->items[blk->itemCount++];
		it->verb = verb;
		it->argc = 0;
		/* an arg token is anything that is not `{`/`}` and not the next verb.
		 * we greedily take up to SCRIPT_MAX_ARGS tokens that are NOT known verbs
		 * or braces — but since verbs and args share the token space, we stop at
		 * a `{`/`}` or when the next token is a recognized verb. */
		for ( ;; ) {
			char peek[SCRIPT_ARG_LEN];
			const char *save = *pp;
			if ( !Script_StreamToken( pp, peek, sizeof( peek ) ) ) break;
			if ( !Q_stricmp( peek, "{" ) || !Q_stricmp( peek, "}" ) ||
			     Script_FindVerb( peek ) >= 0 ) {
				*pp = save;   /* not an arg — put it back */
				break;
			}
			if ( it->argc < SCRIPT_MAX_ARGS ) {
				Q_strncpyz( it->arg[it->argc], peek, SCRIPT_ARG_LEN );
				it->argc++;
			}
			/* if we've hit the arg cap but more non-verb tokens follow, they are
			 * still this verb's (over-cap) args — consume + drop to stay aligned */
		}
	}
}

static qboolean Script_Parse( scriptState_t *sc, const char *text )
{
	const char *p = text;
	char        first[SCRIPT_ARG_LEN];

	sc->blockCount  = 0;
	sc->activeBlock = -1;

	if ( !Script_PeekToken( p, first, sizeof( first ) ) ) {
		return qfalse;   /* empty */
	}

	/* Backward-compat: decide flat vs. event-keyed STRUCTURALLY, not by name —
	 * `spawn` and `trigger` are both event names AND verbs, so a name-table test
	 * mis-fires. The distinguishing signal is the brace: an event block is
	 * `<event> [param] {`; a flat verb-list is a bare `verb args ...` with no
	 * top-level `{`. Look ahead: if the first token names an event and a `{`
	 * follows it (with at most one param token between), the file is event-keyed;
	 * otherwise wrap the whole file into an implicit spawn block so every
	 * pre-migration script runs unchanged. */
	{
		qboolean eventKeyed = qfalse;
		if ( Script_FindEvent( first ) >= 0 ) {
			const char *q = p;
			char        t[SCRIPT_ARG_LEN];
			Script_StreamToken( &q, t, sizeof( t ) );        /* consume the event name */
			if ( Script_StreamToken( &q, t, sizeof( t ) ) ) {
				if ( !Q_stricmp( t, "{" ) )                  /* `<event> {` */
					eventKeyed = qtrue;
				else if ( Script_StreamToken( &q, t, sizeof( t ) )
				          && !Q_stricmp( t, "{" ) )          /* `<event> param {` */
					eventKeyed = qtrue;
			}
		}
		if ( eventKeyed ) goto event_keyed;
	}
	{
		scriptEventBlock_t *blk = &sc->blocks[sc->blockCount++];
		blk->eventNum = Script_FindEvent( "spawn" );
		blk->param[0] = '\0';
		blk->itemCount = 0;
		/* parse the whole stream as this block's verbs (no braces) */
		{
			char tok[SCRIPT_ARG_LEN];
			while ( Script_StreamToken( &p, tok, sizeof( tok ) ) ) {
				int verb;
				scriptItem_t *it;
				if ( !Q_stricmp( tok, "{" ) || !Q_stricmp( tok, "}" ) ) continue;
				verb = Script_FindVerb( tok );
				if ( verb < 0 ) {
					Com_Log( SEV_WARN, LOG_CH(ch_script), "unknown script verb '%s' (skipped)\n", tok );
					continue;
				}
				if ( blk->itemCount >= SCRIPT_MAX_ITEMS_PER_BLOCK ) continue;
				it = &blk->items[blk->itemCount++];
				it->verb = verb;
				it->argc = 0;
				for ( ;; ) {
					char peek[SCRIPT_ARG_LEN];
					const char *save = p;
					if ( !Script_StreamToken( &p, peek, sizeof( peek ) ) ) break;
					if ( Script_FindVerb( peek ) >= 0 ) { p = save; break; }
					if ( it->argc < SCRIPT_MAX_ARGS ) {
						Q_strncpyz( it->arg[it->argc], peek, SCRIPT_ARG_LEN );
						it->argc++;
					}
				}
			}
		}
		return ( blk->itemCount > 0 );
	}

	/* Event-keyed parse: a sequence of  <event> [param] { verbs } blocks. */
event_keyed:
	{
		char tok[SCRIPT_ARG_LEN];
		while ( Script_StreamToken( &p, tok, sizeof( tok ) ) ) {
			int ev = Script_FindEvent( tok );
			scriptEventBlock_t *blk;
			char next[SCRIPT_ARG_LEN];
			const char *save;

			if ( ev < 0 ) {
				Com_Log( SEV_WARN, LOG_CH(ch_script), "unknown script event '%s' (block skipped)\n", tok );
				/* skip to the matching close brace */
				{ int depth = 0;
				  while ( Script_StreamToken( &p, next, sizeof( next ) ) ) {
					if ( !Q_stricmp( next, "{" ) ) depth++;
					else if ( !Q_stricmp( next, "}" ) ) { if ( --depth <= 0 ) break; }
				  } }
				continue;
			}
			if ( sc->blockCount >= SCRIPT_MAX_BLOCKS ) {
				Com_Log( SEV_WARN, LOG_CH(ch_script), "too many event blocks (%d) — extra dropped\n",
					SCRIPT_MAX_BLOCKS );
				break;
			}
			blk = &sc->blocks[sc->blockCount];
			blk->eventNum = ev;
			blk->param[0] = '\0';

			/* optional param before the `{` */
			save = p;
			if ( !Script_StreamToken( &p, next, sizeof( next ) ) ) break;
			if ( Q_stricmp( next, "{" ) != 0 ) {
				Q_strncpyz( blk->param, next, SCRIPT_ARG_LEN );   /* param token */
				if ( !Script_StreamToken( &p, next, sizeof( next ) ) ) break;
			}
			if ( Q_stricmp( next, "{" ) != 0 ) {
				Com_Log( SEV_WARN, LOG_CH(ch_script), "event '%s' missing '{' (skipped)\n", tok );
				p = save;
				continue;
			}
			Script_ParseBlockVerbs( blk, &p );
			sc->blockCount++;
		}
	}

	return ( sc->blockCount > 0 );
}

/* ── Runner ────────────────────────────────────────────────────────────────
 * Walk the ACTIVE event block's verbs from the cursor. Advance on qtrue, suspend
 * on qfalse (re-entered next frame). When the cursor passes the block's last
 * verb, the block is done → go idle (activeBlock = -1): the monster keeps its
 * final scripted state and waits for the next event to fire a new block (it does
 * NOT fall back to the autonomous FSM — a set-piece author owns it). */
void G_RunScriptDispatcher( gentity_t *ent )
{
	behaviorState_t    *bs = ent->behaviorState;
	scriptState_t      *sc;
	scriptEventBlock_t *blk;

	if ( !bs || !bs->scripted ) return;

	sc = Script_FindFor( (int)( ent - g_entities ) );
	if ( !sc ) return;                              /* no parsed script */

	Script_PollEvents( ent, bs, sc );               /* fire sight-driven events */

	if ( sc->activeBlock < 0 || sc->activeBlock >= sc->blockCount ) {
		return;                                     /* idle — waiting for an event */
	}
	blk = &sc->blocks[sc->activeBlock];

	while ( bs->scriptCursor < blk->itemCount ) {
		scriptItem_t *it = &blk->items[bs->scriptCursor];
		qboolean      firstFrame = ( bs->scriptChangeTime == level.time );
		int           activeBlockBefore = sc->activeBlock;

		if ( g_cheats.integer && firstFrame ) {
			Com_Log( SEV_DEBUG, LOG_CH(ch_script),
				"monster %d [%s] verb[%d] %s\n", (int)( ent - g_entities ),
				s_scriptEvents[blk->eventNum].name, bs->scriptCursor,
				s_scriptVerbs[it->verb].name );
		}

		if ( !s_scriptVerbs[it->verb].fn( ent, bs, it ) ) {
			/* not done — suspend at this cursor, re-enter next frame */
			return;
		}

		/* A verb may have re-entered the dispatcher on the SAME entity (e.g. a lethal
		 * verb whose damage fires the death event, which runs the death block here and
		 * to completion). That nested run owns the shared cursor/activeBlock, so this
		 * outer run must NOT keep walking its stale block — the entity's script has moved
		 * on (or ended). Bail instead of advancing into corrupted state. */
		if ( sc->activeBlock != activeBlockBefore ) {
			return;
		}

		/* done — advance */
		bs->scriptCursor++;
		bs->scriptChangeTime = level.time;
	}

	/* block exhausted → idle, wait for the next event */
	sc->activeBlock = -1;
}

/* ════════════════════════════════════════════════════════════════════════
   Verb handlers — qtrue = done (advance), qfalse = suspend (re-enter)
   ════════════════════════════════════════════════════════════════════════ */

/* resolve a nav-marker name to its world origin; qfalse if not found */
static qboolean Script_MarkerOrigin( const char *name, vec3_t out )
{
	gentity_t *m = G_Find( NULL, FOFS( targetname ), (char *)name );
	if ( !m ) return qfalse;
	VectorCopy( m->r.currentOrigin, out );
	return qtrue;
}

/* -- spawn ---------------------------------------------------------------- */

/* spawncast <archetype> <marker>: spawn a script-driven behavior monster of the
 * named character archetype at the marker. The archetype is a characters/<name>/
 * directory (e.g. `visor`); its main.lua is loaded + bound so the monster carries
 * that character's identity/model (and its Lua "decide" override if it has one).
 * If the archetype can't be loaded the monster still spawns pure-C (never wedge).
 * A bare `spawncast <archetype>` with no marker is a no-op WARN. */
static qboolean Verb_SpawnCast( gentity_t *ent, behaviorState_t *bs, scriptItem_t *it )
{
	vec3_t     org;
	gentity_t *mob;
	int        characterHandle = 0;
	(void)ent; (void)bs;

	if ( it->argc < 2 || !Script_MarkerOrigin( it->arg[1], org ) ) {
		Com_Log( SEV_WARN, LOG_CH(ch_script), "spawncast: bad args or unknown marker\n" );
		return qtrue;   /* skip — don't wedge the script */
	}

	/* Resolve arg[0] as a character archetype (empty/unloadable → generic pure-C). */
	characterHandle = Script_ResolveCharacter( it->arg[0] );

	mob = G_SpawnBehaviorMonster( org, ENTITYNUM_NONE, 100, characterHandle, it->arg[0] );
	if ( mob && mob->behaviorState ) {
		mob->behaviorState->scripted = qtrue;   /* a spawned cast is script-driven */
	}
	Com_Log( SEV_DEBUG, LOG_CH(ch_script), "spawncast '%s' at %s -> monster %d\n",
		it->arg[0], it->arg[1], mob ? (int)( mob - g_entities ) : -1 );
	return qtrue;
}

static qboolean Verb_Spawn( gentity_t *ent, behaviorState_t *bs, scriptItem_t *it )
{
	vec3_t     org;
	gentity_t *e;
	(void)ent; (void)bs;

	if ( it->argc < 2 || !Script_MarkerOrigin( it->arg[1], org ) ) {
		Com_Log( SEV_WARN, LOG_CH(ch_script), "spawn: bad args or unknown marker\n" );
		return qtrue;
	}
	e = G_Spawn();
	if ( !e ) return qtrue;
	e->classname = G_NewString( it->arg[0] );
	VectorCopy( org, e->s.origin );
	VectorCopy( org, e->r.currentOrigin );
	if ( !G_CallSpawn( e ) ) {
		G_FreeEntity( e );
	}
	return qtrue;
}

/* -- blocking: hold / nosight / playersight / wait ------------------------ */

/* hold: suspend until the monster's current move finishes (nav goal reached). */
static qboolean Verb_Hold( gentity_t *ent, behaviorState_t *bs, scriptItem_t *it )
{
	(void)bs; (void)it;
	/* still following a nav goal → not done */
	return ( ent->navFollower ) ? qfalse : qtrue;
}

/* nosight: suspend until the monster can NO LONGER see its enemy. */
static qboolean Verb_NoSight( gentity_t *ent, behaviorState_t *bs, scriptItem_t *it )
{
	gentity_t *enemy;
	vec3_t     eye;
	(void)it;

	if ( bs->enemy < 0 || bs->enemy >= MAX_GENTITIES ) return qtrue;  /* no enemy → done */
	enemy = &g_entities[bs->enemy];
	if ( !enemy->inuse ) return qtrue;

	VectorCopy( ent->r.currentOrigin, eye );
	eye[2] += 48.0f;   /* monster eye height (mirrors the behavior sight test) */
	/* still visible → keep waiting; lost sight → done */
	return CanDamage( enemy, eye ) ? qfalse : qtrue;
}

/* playersight: suspend until a player enters the monster's sight; on sight,
 * latch that player as the monster's enemy so a following attack has a target. */
static qboolean Verb_PlayerSight( gentity_t *ent, behaviorState_t *bs, scriptItem_t *it )
{
	int    i;
	vec3_t eye;
	(void)it;

	VectorCopy( ent->r.currentOrigin, eye );
	eye[2] += 48.0f;

	for ( i = 0; i < level.maxclients; i++ ) {
		gentity_t *pl = &g_entities[i];
		if ( !pl->inuse || !pl->client ) continue;
		if ( pl->health <= 0 ) continue;
		if ( CanDamage( pl, eye ) ) {
			bs->enemy = pl->s.number;   /* seen — target it */
			return qtrue;               /* done */
		}
	}
	return qfalse;   /* no player in sight yet — keep waiting */
}

/* waitplayerradius <units>: BLOCKING — suspend until a live player is within
 * <units> of the monster running this verb, then latch that player as the enemy
 * and advance. The proximity twin of playersight: same maxclients scan + qfalse/
 * qtrue re-entry, but a straight-line distance test instead of a sight cone, so
 * an approaching player triggers the encounter through cover. This is how a
 * scripted set-piece gates on player-approach without a map trigger-volume. */
static qboolean Verb_WaitPlayerRadius( gentity_t *ent, behaviorState_t *bs, scriptItem_t *it )
{
	int   i;
	float r;

	if ( it->argc < 1 ) return qtrue;
	r = (float)atof( it->arg[0] );
	if ( r <= 0.0f ) return qtrue;   /* no radius → nothing to wait for */

	for ( i = 0; i < level.maxclients; i++ ) {
		gentity_t *pl = &g_entities[i];
		if ( !pl->inuse || !pl->client ) continue;
		if ( pl->health <= 0 ) continue;
		if ( Distance( pl->r.currentOrigin, ent->r.currentOrigin ) <= r ) {
			bs->enemy = pl->s.number;   /* in range — target it */
			return qtrue;               /* done */
		}
	}
	return qfalse;   /* no player in range yet — keep waiting */
}

/* wait <ms>: the canonical blocking form — a bare time-compare against the time
 * the cursor last advanced. qfalse (suspend) until the duration elapses. */
static qboolean Verb_Wait( gentity_t *ent, behaviorState_t *bs, scriptItem_t *it )
{
	int ms;
	(void)ent;
	if ( it->argc < 1 ) return qtrue;
	ms = atoi( it->arg[0] );
	return ( bs->scriptChangeTime + ms < level.time ) ? qtrue : qfalse;
}

/* -- nav-intent: gotomarker / walktomarker -------------------------------- */

/* A scripted walk gait — half the follower's default run speed, for a slow
 * cutscene approach. gotomarker uses the default (0 = run). */
#define SCRIPT_WALK_SPEED 100.0f

static qboolean Verb_GotoMarker( gentity_t *ent, behaviorState_t *bs, scriptItem_t *it )
{
	vec3_t goal;
	if ( it->argc < 1 || !Script_MarkerOrigin( it->arg[0], goal ) ) {
		Com_Log( SEV_WARN, LOG_CH(ch_script), "gotomarker: unknown marker '%s'\n",
			it->argc ? it->arg[0] : "" );
		return qtrue;
	}
	ent->navSpeed = 0.0f;                   /* run (default follower speed) */
	bs->state = BSTATE_HUNT;               /* HUNT native-executes a nav walk */
	Nav_StartFollower( ent, goal, 0 );     /* set navGoal + navFollower */
	return qtrue;   /* intent issued; the following `hold` blocks on arrival */
}

static qboolean Verb_WalkToMarker( gentity_t *ent, behaviorState_t *bs, scriptItem_t *it )
{
	vec3_t goal;
	if ( it->argc < 1 || !Script_MarkerOrigin( it->arg[0], goal ) ) {
		Com_Log( SEV_WARN, LOG_CH(ch_script), "walktomarker: unknown marker '%s'\n",
			it->argc ? it->arg[0] : "" );
		return qtrue;
	}
	bs->state = BSTATE_HUNT;
	Nav_StartFollower( ent, goal, 0 );
	ent->navSpeed = SCRIPT_WALK_SPEED;     /* slower gait than gotomarker's run */
	return qtrue;
}

/* -- control: trigger / accum / print ------------------------------------- */

static qboolean Verb_Trigger( gentity_t *ent, behaviorState_t *bs, scriptItem_t *it )
{
	int i;
	(void)ent; (void)bs;
	if ( it->argc < 1 ) return qtrue;

	Script_FireTrigger( it->arg[0] );   /* latch — polled by waitfortrigger */

	/* Also fire the `trigger` event on any scripted monster carrying a matching
	 * `trigger <name> { ... }` block — the event-driven half of the same signal. */
	for ( i = 0; i < SCRIPT_POOL_SIZE; i++ ) {
		if ( !s_scripts[i].used ) continue;
		Script_FireEvent( &g_entities[s_scripts[i].entityNum], "trigger", it->arg[0] );
	}
	return qtrue;
}

/* waitfortrigger <name>: BLOCKING — the consumer half of `trigger`. Suspends
 * (qfalse, re-entered each frame) until the named trigger has fired, then
 * advances. Lets one scripted monster gate on another's `trigger`. */
static qboolean Verb_WaitForTrigger( gentity_t *ent, behaviorState_t *bs, scriptItem_t *it )
{
	(void)ent; (void)bs;
	if ( it->argc < 1 ) return qtrue;
	return Script_TriggerFired( it->arg[0] ) ? qtrue : qfalse;
}

/* Abort the CURRENT event block: fast-forward the cursor to the block's end so
 * the runner's post-advance leaves it out of the block's walk loop, ending only
 * this block (not the whole script — the monster then idles and waits for the
 * next event). Mirrors RealRTCW's castScriptStackHead = event.stack.numItems.
 * The abort_if_* ops use this as their forward-branch primitive. */
static void Script_Abort( gentity_t *ent, behaviorState_t *bs )
{
	scriptState_t *sc = Script_FindFor( (int)( ent - g_entities ) );
	if ( sc && sc->activeBlock >= 0 && sc->activeBlock < sc->blockCount ) {
		bs->scriptCursor = sc->blocks[sc->activeBlock].itemCount;
	}
}

/* accum <slot> <op> <val>: per-monster integer arithmetic + conditional
 * script-end, the branch primitive set-pieces need before goto/label land.
 * All ops are immediate (return qtrue). Unknown op → WARN + skip (never wedge).
 *   set/inc/dec <v>            arithmetic
 *   random <max>              slot = deterministic 0..max-1
 *   bitset/bitreset <bit>     set/clear a bit in the slot
 *   abort_if_less/greater/equal/not_equal <v>   abort the current block if so */
static qboolean Verb_Accum( gentity_t *ent, behaviorState_t *bs, scriptItem_t *it )
{
	int   slot, val;
	int  *acc;
	const char *op;

	if ( it->argc < 3 ) return qtrue;
	slot = atoi( it->arg[0] );
	op   = it->arg[1];
	val  = atoi( it->arg[2] );
	if ( slot < 0 || slot >= SCRIPT_ACCUM_SLOTS ) {
		Com_Log( SEV_WARN, LOG_CH(ch_script), "accum: slot %d out of range\n", slot );
		return qtrue;
	}
	acc = &bs->scriptAccum[slot];

	if      ( !Q_stricmp( op, "set" ) )      *acc  = val;
	else if ( !Q_stricmp( op, "inc" ) )      *acc += val;
	else if ( !Q_stricmp( op, "dec" ) )      *acc -= val;
	else if ( !Q_stricmp( op, "random" ) )   *acc  = ( val > 0 ) ? (int)( Q_random( &s_scriptRandSeed ) * val ) : 0;
	else if ( !Q_stricmp( op, "bitset" ) )   *acc |= ( 1 << val );
	else if ( !Q_stricmp( op, "bitreset" ) ) *acc &= ~( 1 << val );
	else if ( !Q_stricmp( op, "abort_if_less" ) )      { if ( *acc <  val ) Script_Abort( ent, bs ); }
	else if ( !Q_stricmp( op, "abort_if_greater" ) )   { if ( *acc >  val ) Script_Abort( ent, bs ); }
	else if ( !Q_stricmp( op, "abort_if_equal" ) )     { if ( *acc == val ) Script_Abort( ent, bs ); }
	else if ( !Q_stricmp( op, "abort_if_not_equal" ) ) { if ( *acc != val ) Script_Abort( ent, bs ); }
	else Com_Log( SEV_WARN, LOG_CH(ch_script), "accum: unknown op '%s'\n", op );

	return qtrue;
}

/* globalaccum <slot> <op> <val>: identical to accum but on the LEVEL-GLOBAL slot
 * buffer (shared across all scripted monsters), so a set-piece can coordinate
 * several casts through one counter. Same op-set as accum; abort_if_* still aborts
 * the CURRENT block of the monster that ran the verb. Unknown op → WARN + skip. */
static qboolean Verb_GlobalAccum( gentity_t *ent, behaviorState_t *bs, scriptItem_t *it )
{
	int   slot, val;
	int  *acc;
	const char *op;

	if ( it->argc < 3 ) return qtrue;
	slot = atoi( it->arg[0] );
	op   = it->arg[1];
	val  = atoi( it->arg[2] );
	if ( slot < 0 || slot >= SCRIPT_GLOBAL_ACCUM_SLOTS ) {
		Com_Log( SEV_WARN, LOG_CH(ch_script), "globalaccum: slot %d out of range\n", slot );
		return qtrue;
	}
	acc = &s_scriptGlobalAccum[slot];

	if      ( !Q_stricmp( op, "set" ) )      *acc  = val;
	else if ( !Q_stricmp( op, "inc" ) )      *acc += val;
	else if ( !Q_stricmp( op, "dec" ) )      *acc -= val;
	else if ( !Q_stricmp( op, "random" ) )   *acc  = ( val > 0 ) ? (int)( Q_random( &s_scriptRandSeed ) * val ) : 0;
	else if ( !Q_stricmp( op, "bitset" ) )   *acc |= ( 1 << val );
	else if ( !Q_stricmp( op, "bitreset" ) ) *acc &= ~( 1 << val );
	else if ( !Q_stricmp( op, "abort_if_less" ) )      { if ( *acc <  val ) Script_Abort( ent, bs ); }
	else if ( !Q_stricmp( op, "abort_if_greater" ) )   { if ( *acc >  val ) Script_Abort( ent, bs ); }
	else if ( !Q_stricmp( op, "abort_if_equal" ) )     { if ( *acc == val ) Script_Abort( ent, bs ); }
	else if ( !Q_stricmp( op, "abort_if_not_equal" ) ) { if ( *acc != val ) Script_Abort( ent, bs ); }
	else { Com_Log( SEV_WARN, LOG_CH(ch_script), "globalaccum: unknown op '%s'\n", op ); return qtrue; }

	Com_Log( SEV_DEBUG, LOG_CH(ch_script), "globalaccum[%d] %s %d -> %d\n", slot, op, val, *acc );
	return qtrue;
}

/* wave <op> <val>: the single level-global survival-wave counter. Ops: set / inc /
 * dec, plus abort_if_less/greater/equal (aborts the current block). No slot index
 * (there is exactly one counter). The gameplay consumer is the deferred survival
 * fork; today this only mutates the counter + supports the branch. Unknown op →
 * WARN + skip. */
static qboolean Verb_Wave( gentity_t *ent, behaviorState_t *bs, scriptItem_t *it )
{
	int val;
	const char *op;

	if ( it->argc < 2 ) return qtrue;
	op  = it->arg[0];
	val = atoi( it->arg[1] );

	if      ( !Q_stricmp( op, "set" ) )   s_scriptWaveCount  = val;
	else if ( !Q_stricmp( op, "inc" ) )   s_scriptWaveCount += val;
	else if ( !Q_stricmp( op, "dec" ) )   s_scriptWaveCount -= val;
	else if ( !Q_stricmp( op, "abort_if_less" ) )    { if ( s_scriptWaveCount <  val ) Script_Abort( ent, bs ); }
	else if ( !Q_stricmp( op, "abort_if_greater" ) ) { if ( s_scriptWaveCount >  val ) Script_Abort( ent, bs ); }
	else if ( !Q_stricmp( op, "abort_if_equal" ) )   { if ( s_scriptWaveCount == val ) Script_Abort( ent, bs ); }
	else { Com_Log( SEV_WARN, LOG_CH(ch_script), "wave: unknown op '%s'\n", op ); return qtrue; }

	Com_Log( SEV_DEBUG, LOG_CH(ch_script), "wave %s %d -> %d\n", op, val, s_scriptWaveCount );
	return qtrue;
}

static qboolean Verb_Print( gentity_t *ent, behaviorState_t *bs, scriptItem_t *it )
{
	(void)bs;
	Com_Log( SEV_DEBUG, LOG_CH(ch_script), "monster %d: %s\n",
		(int)( ent - g_entities ), it->argc ? it->arg[0] : "" );
	return qtrue;
}

/* -- act: attack / noattack ----------------------------------------------- */

static qboolean Verb_Attack( gentity_t *ent, behaviorState_t *bs, scriptItem_t *it )
{
	(void)ent; (void)it;
	bs->state = BSTATE_BATTLE;   /* native Battle: face + swing at bs->enemy */
	return qtrue;
}

static qboolean Verb_NoAttack( gentity_t *ent, behaviorState_t *bs, scriptItem_t *it )
{
	(void)ent; (void)it;
	bs->state = BSTATE_IDLE;     /* clear attack intent — stand */
	return qtrue;
}

/* invuln <0|1>: toggle the monster's damage immunity via FL_GODMODE (honored in
 * G_Damage). `invuln 1` before a cutscene keeps a boss unkillable during its own
 * intro (so a following objective_add always arms before the boss can die);
 * `invuln 0` at battle-start drops it. Immediate. FL_GODMODE is cleared with the
 * whole entity on free, so a re-acquired slot never inherits a stale flag. */
static qboolean Verb_Invuln( gentity_t *ent, behaviorState_t *bs, scriptItem_t *it )
{
	(void)bs;
	if ( it->argc >= 1 && atoi( it->arg[0] ) ) {
		ent->flags |= FL_GODMODE;
	} else {
		ent->flags &= ~FL_GODMODE;
	}
	Com_Log( SEV_DEBUG, LOG_CH(ch_script), "monster %d invuln %s\n",
		(int)( ent - g_entities ), ( ent->flags & FL_GODMODE ) ? "on" : "off" );
	return qtrue;
}

/* shock [damage]: deliver a protection-piercing hit to the monster — the shockwave that
 * kills a boss made invulnerable to normal fire (invuln 1 / FL_GODMODE). It uses
 * DAMAGE_NO_PROTECTION, which G_Damage applies through godmode/armor (the same vehicle a
 * map's trigger_hurt with the NO_PROTECTION spawnflag uses). Default damage is large
 * enough to kill in one hit; a caller may pass a smaller value. This is how a scripted
 * boss dies on its own timeline while shrugging off the player's weapons. */
static qboolean Verb_Shock( gentity_t *ent, behaviorState_t *bs, scriptItem_t *it )
{
	int dmg;
	(void)bs;
	dmg = ( it->argc >= 1 ) ? atoi( it->arg[0] ) : 100000;
	if ( dmg < 1 ) dmg = 1;
	G_Damage( ent, ent, ent, NULL, NULL, dmg, DAMAGE_NO_PROTECTION, MOD_TRIGGER_HURT );
	Com_Log( SEV_DEBUG, LOG_CH(ch_script), "monster %d shock %d (protection-piercing)\n",
		(int)( ent - g_entities ), dmg );
	return qtrue;
}

/* -- cast-tuning: sethealth / giveweapon / selectweapon ------------------- */

static qboolean Verb_SetHealth( gentity_t *ent, behaviorState_t *bs, scriptItem_t *it )
{
	(void)bs;
	if ( it->argc >= 1 ) ent->health = atoi( it->arg[0] );
	return qtrue;
}

/* Resolve a weapon argument to a weapon_t. A bare positive integer < the weapon
 * count is used directly (compat). Otherwise the arg is a NAME, resolved against
 * the shared item table by pickup_name or classname (the readable, author-facing
 * form). Returns WP_NONE (0) when nothing matches. */
static int Script_ResolveWeapon( const char *arg )
{
	gitem_t *it;
	int      n;

	if ( !arg || !arg[0] ) return WP_NONE;

	/* numeric index (compat) */
	n = atoi( arg );
	if ( n > WP_NONE && n < WP_NUM_WEAPONS ) {
		/* only if the whole token was digits — else "weapon_railgun" atoi's to 0 */
		const char *c;
		qboolean allDigits = qtrue;
		for ( c = arg; *c; c++ ) { if ( *c < '0' || *c > '9' ) { allDigits = qfalse; break; } }
		if ( allDigits ) return n;
	}

	/* name: match pickup_name, else classname, on the weapon items */
	it = BG_FindItem( arg );
	if ( it && it->giType == IT_WEAPON ) return it->giTag;
	for ( it = bg_itemlist + 1; it->classname; it++ ) {
		if ( it->giType == IT_WEAPON && !Q_stricmp( it->classname, arg ) ) return it->giTag;
	}
	return WP_NONE;
}

static qboolean Verb_GiveWeapon( gentity_t *ent, behaviorState_t *bs, scriptItem_t *it )
{
	int w;
	(void)bs;
	if ( it->argc < 1 ) return qtrue;
	w = Script_ResolveWeapon( it->arg[0] );
	if ( w <= WP_NONE || w >= WP_NUM_WEAPONS ) {
		Com_Log( SEV_WARN, LOG_CH(ch_script), "giveweapon: unknown weapon '%s'\n", it->arg[0] );
		return qtrue;
	}
	if ( ent->client ) {
		ent->client->ps.stats[STAT_WEAPONS] |= ( 1 << w );
	}
	ent->s.weapon = w;   /* a non-client monster carries its weapon in s.weapon */
	return qtrue;
}

static qboolean Verb_SelectWeapon( gentity_t *ent, behaviorState_t *bs, scriptItem_t *it )
{
	int w;
	(void)bs;
	if ( it->argc < 1 ) return qtrue;
	w = Script_ResolveWeapon( it->arg[0] );
	if ( w <= WP_NONE || w >= WP_NUM_WEAPONS ) {
		Com_Log( SEV_WARN, LOG_CH(ch_script), "selectweapon: unknown weapon '%s'\n", it->arg[0] );
		return qtrue;
	}
	ent->s.weapon = w;
	if ( ent->client ) ent->client->ps.weapon = w;
	return qtrue;
}

/* -- world-trigger / choreography: changelevel / faceangles --------------- */

static qboolean Verb_ChangeLevel( gentity_t *ent, behaviorState_t *bs, scriptItem_t *it )
{
	(void)ent; (void)bs;
	if ( it->argc >= 1 ) {
		trap_SendConsoleCommand( EXEC_APPEND, va( "map %s\n", it->arg[0] ) );
	}
	return qtrue;
}

static qboolean Verb_FaceAngles( gentity_t *ent, behaviorState_t *bs, scriptItem_t *it )
{
	vec3_t ang;
	(void)bs;
	if ( it->argc < 2 ) return qtrue;
	VectorClear( ang );
	ang[YAW]   = atof( it->arg[0] );
	ang[PITCH] = atof( it->arg[1] );
	ent->s.angles[YAW]   = ang[YAW];
	ent->s.apos.trType   = TR_STATIONARY;
	VectorCopy( ang, ent->s.apos.trBase );
	if ( ent->client ) {
		SetClientViewAngle( ent, ang );
	}
	return qtrue;
}

/* -- MISSION: objective_add / _complete / _fail / _clear ------------------
 * Drive the in-memory objectives system (g_objectives.c) directly game-side —
 * the same-tier direct-call pattern as sethealth, no trap round-trip. All four
 * are immediate (mutate level state, return qtrue). A bad index WARNs and is a
 * no-op — never wedge the script. In-session only: this state is not persisted. */

static qboolean Verb_ObjectiveAdd( gentity_t *ent, behaviorState_t *bs, scriptItem_t *it )
{
	qboolean required = qtrue;   /* a bare `objective_add key` is required */
	int      idx;
	(void)ent; (void)bs;
	if ( it->argc < 1 ) return qtrue;
	if ( it->argc >= 2 ) required = ( atoi( it->arg[1] ) != 0 );   /* `required` 0 = optional */
	idx = G_Objectives_Add( it->arg[0], required );
	if ( idx < 0 )
		Com_Log( SEV_WARN, LOG_CH(ch_script), "objective_add: rejected '%s' (empty, duplicate, or full)\n", it->arg[0] );
	else
		Com_Log( SEV_DEBUG, LOG_CH(ch_script), "objective_add: [%d] '%s' (%s)\n",
			idx, it->arg[0], required ? "required" : "optional" );
	return qtrue;
}

static qboolean Verb_ObjectiveComplete( gentity_t *ent, behaviorState_t *bs, scriptItem_t *it )
{
	int idx;
	(void)ent; (void)bs;
	if ( it->argc < 1 ) return qtrue;
	idx = atoi( it->arg[0] );
	if ( !G_Objectives_Complete( idx ) )   /* publishes + re-evals the gate on change */
		Com_Log( SEV_WARN, LOG_CH(ch_script), "objective_complete: [%d] no-op (bad index or already done/failed)\n", idx );
	else
		Com_Log( SEV_DEBUG, LOG_CH(ch_script), "objective_complete: [%d]\n", idx );
	return qtrue;
}

static qboolean Verb_ObjectiveFail( gentity_t *ent, behaviorState_t *bs, scriptItem_t *it )
{
	int idx;
	(void)ent; (void)bs;
	if ( it->argc < 1 ) return qtrue;
	idx = atoi( it->arg[0] );
	if ( !G_Objectives_Fail( idx ) )   /* publishes + re-evals the gate on change */
		Com_Log( SEV_WARN, LOG_CH(ch_script), "objective_fail: [%d] no-op (bad index, already failed, or already completed)\n", idx );
	else
		Com_Log( SEV_DEBUG, LOG_CH(ch_script), "objective_fail: [%d]\n", idx );
	return qtrue;
}

static qboolean Verb_ObjectiveClear( gentity_t *ent, behaviorState_t *bs, scriptItem_t *it )
{
	(void)ent; (void)bs; (void)it;
	G_Objectives_Clear();   /* publishes the now-empty list */
	Com_Log( SEV_DEBUG, LOG_CH(ch_script), "objective_clear\n" );
	return qtrue;
}

/* -- CINEMATIC: playscene / stopscene -------------------------------------
 * Trigger the client-side WiredScene director via the "scene" server-command
 * crossing — the game VM cannot reach cgame directly, so it emits a command
 * string (the same channel as `cp`). The scene DEFINITION (camera spline, events,
 * targets) lives in scripts/scene/<name>.lua and loads client-side via
 * trap_WiredSceneLoad; the string carries only the scene NAME + the live entity
 * numbers to bind to the scene's look-at target slots. Both verbs are immediate
 * (fire the command, return qtrue) — the cutscene plays client-side.
 *
 * Broadcast (clientNum -1) so every connected client plays it: SP has one client;
 * an MP-observed cutscene reaches all viewers (mirrors how `cp` broadcasts). */

static qboolean Verb_PlayScene( gentity_t *ent, behaviorState_t *bs, scriptItem_t *it )
{
	char args[MAX_STRING_CHARS];
	char actorList[128];
	int  i;
	size_t len = 0;

	if ( it->argc < 1 ) {
		Com_Log( SEV_WARN, LOG_CH(ch_script), "playscene: missing scene name\n" );
		return qtrue;
	}

	/* Stamp the fire time so a following `waitscene <ms>` can block for the scene's
	 * (author-known) duration — the server-side timer that stands in for a real
	 * client→game scene-done signal (which the VM split does not allow). */
	bs->sceneStartTime = level.time;

	/* Assemble the actor entityNums into a space-separated list. An actor arg is
	 * either a numeric entityNum or the token `self` (the scripted monster running
	 * this verb — the common case: a scene that follows the entity that triggered
	 * it). Capped by the actorList buffer; extra args are silently dropped. */
	actorList[0] = '\0';
	for ( i = 1; i < it->argc; i++ ) {
		int e = ( !Q_stricmp( it->arg[i], "self" ) )
		        ? (int)( ent - g_entities ) : atoi( it->arg[i] );
		Com_sprintf( actorList + len, (int)( sizeof( actorList ) - len ), " %d", e );
		len = strlen( actorList );
		if ( len >= sizeof( actorList ) - 12 ) break;   /* leave room for one more " %d" */
	}

	/* "scene <name> [actorNum ...]" — the crossing string (see CG_ServerCommand). */
	Com_sprintf( args, sizeof( args ), "scene %s%s", it->arg[0], actorList );
	trap_SendServerCommand( -1, args );
	Com_Log( SEV_DEBUG, LOG_CH(ch_script), "playscene -> serverCommand '%s'\n", args );
	return qtrue;
}

static qboolean Verb_StopScene( gentity_t *ent, behaviorState_t *bs, scriptItem_t *it )
{
	(void)ent; (void)bs; (void)it;
	trap_SendServerCommand( -1, "scene stop" );
	Com_Log( SEV_DEBUG, LOG_CH(ch_script), "stopscene\n" );
	return qtrue;
}

/* waitscene <ms>: BLOCKING — suspend until the last playscene has been running for
 * <ms> milliseconds, then advance. Lets a set-piece sequence AFTER a cutscene
 * (playscene X ; waitscene 3000 ; attack). The scene "done" state is client-side
 * and the game VM has no client→game channel to read it, so this is a SERVER-SIDE
 * timer keyed on the author-known scene duration: approximate but SP-adequate. It
 * is the timer-block twin of `wait`, but anchored on sceneStartTime (set by
 * playscene) rather than the cursor-advance time. With no prior playscene
 * (sceneStartTime 0) it still resolves once level.time passes <ms>. */
static qboolean Verb_WaitScene( gentity_t *ent, behaviorState_t *bs, scriptItem_t *it )
{
	int ms;
	(void)ent;
	if ( it->argc < 1 ) return qtrue;
	ms = atoi( it->arg[0] );
	if ( ms < 0 ) ms = 0;   /* a negative duration is not a wait */
	/* strict `<`, byte-identical to Verb_Wait's timer test (its exact twin). */
	return ( bs->sceneStartTime + ms < level.time ) ? qtrue : qfalse;
}

/* Resolve a character archetype name (a characters/<name>/ directory) to a
 * positive monster-bind handle, or 0 if the name is empty or unloadable.
 * trap_BotLoadCharacter returns a NEGATIVE handle for a Lua character; the
 * monster bind wants the raw positive handle, so negate it. */
static int Script_ResolveCharacter( const char *name )
{
	char path[MAX_QPATH];
	int  botChar;

	if ( !name || !name[0] ) return 0;
	Com_sprintf( path, sizeof( path ), "characters/%s/main.lua", name );
	botChar = trap_BotLoadCharacter( path, 3.0f );
	if ( botChar < 0 ) return -botChar;
	Com_Log( SEV_WARN, LOG_CH(ch_script),
		"could not load character '%s' (spawning pure-C)\n", name );
	return 0;
}

/* ── Script load + spawn ───────────────────────────────────────────────────
 * Load a set-piece script FILE as data (a verb-list text block), parse it into
 * an item list, spawn one scripted monster, and bind the script to it. The
 * file is read once; from here the drive is pure C. `characterName` (may be NULL)
 * binds a character archetype so a scripted boss carries a real identity/model
 * while still running its event blocks. Returns the driven entity. */
struct gentity_s *Script_SpawnDriven( const char *scriptPath, const vec3_t origin, int enemyNum,
                                      const char *characterName )
{
	fileHandle_t  f;
	int           len;
	char          buf[8192];
	gentity_t    *mob;
	scriptState_t *sc;

	len = trap_FS_FOpenFile( scriptPath, &f, FS_READ );
	if ( len <= 0 || len >= (int)sizeof( buf ) ) {
		Com_Log( SEV_WARN, LOG_CH(ch_script), "could not open script '%s'\n", scriptPath );
		if ( f ) trap_FS_FCloseFile( f );
		return NULL;
	}
	trap_FS_Read( buf, len, f );
	buf[len] = '\0';
	trap_FS_FCloseFile( f );

	mob = G_SpawnBehaviorMonster( origin, enemyNum, 100, Script_ResolveCharacter( characterName ), characterName );
	if ( !mob || !mob->behaviorState ) {
		return NULL;
	}

	sc = Script_AcquireFor( mob->s.number );
	if ( !sc || !Script_Parse( sc, buf ) ) {
		if ( sc ) sc->used = qfalse;
		Com_Log( SEV_WARN, LOG_CH(ch_script), "script '%s' parsed to zero verbs\n", scriptPath );
		return NULL;
	}

	mob->behaviorState->scripted         = qtrue;
	mob->behaviorState->scriptTarget     = ENTITYNUM_NONE;
	mob->behaviorState->scriptCursor     = 0;
	mob->behaviorState->scriptChangeTime = level.time;
	mob->behaviorState->stateEntered     = level.time;

	Com_Log( SEV_DEBUG, LOG_CH(ch_script),
		"spawned scripted monster %d from '%s' (%d blocks)\n",
		mob->s.number, scriptPath, sc->blockCount );

	/* Fire the spawn event — runs the spawn block (or the wrapped flat script). */
	Script_FireEvent( mob, "spawn", NULL );
	return mob;
}

#endif /* FEAT_MONSTER_AI */
