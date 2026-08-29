// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

/*
===========================================================================
g_behavior.c -- non-client monster behavior tick (the decision layer).

A monster is two stacked non-client systems: it DECIDES (this behavior layer)
and it MOVES (the nav-follower movement layer in g_bot_nav.c). G_RunBehavior is
the upper hook — dispatched each server frame from G_RunFrame when ent->aiThink
is set. A later phase fills it with the actual decision logic (choose a goal,
pick a mode); the goal it sets is then walked by the nav-follower below it.

This is the inert shell: it only runs the entity's scheduled think, exactly as
G_RunMover / G_RunNavFollower do, so a behaving entity's think fires on time.
Nothing sets ent->aiThink yet, so this is dead code until the decision layer
lands — the build is byte-identical with it present.
===========================================================================
*/

#include "g_local.h"
#include "g_behavior.h"

#if FEAT_MONSTER_AI

LOG_DECLARE_CHANNEL( ch_behavior, "game.behavior" );
LOG_DECLARE_CHANNEL( ch_botai, "botlib.ai" );   /* shared bot-nav telemetry stream */

/* ── Per-monster behavior-state pool ──────────────────────────────────────
 *
 * A non-client monster acquires one of these slots into ent->behaviorState to
 * store its behavior-FSM state — the decision-layer analog of the nav-movement
 * pool (navEntityPool in g_bot_nav.c). On demand rather than a flat
 * MAX_GENTITIES array; sized to MAX_MONSTERS, the shared cap that also sizes
 * the nav pool, so a monster that both moves and thinks gets a slot from each
 * without the caps diverging. Acquire/release are balanced; the pool resets
 * each map. */
static behaviorState_t behaviorPool[MAX_MONSTERS];
static qboolean        behaviorPoolUsed[MAX_MONSTERS];

/*
=================
Behavior_ResetPool — free every slot (called on map/game init)
=================
*/
void Behavior_ResetPool( void )
{
	memset( behaviorPool, 0, sizeof( behaviorPool ) );
	memset( behaviorPoolUsed, 0, sizeof( behaviorPoolUsed ) );
}

/*
=================
Behavior_AcquireState — grab a free pool slot for this entity

Zeroes the slot, marks it active, points ent->behaviorState at it. Returns NULL
if the pool is exhausted (the caller must handle having no behavior-state).
=================
*/
struct behaviorState_s *Behavior_AcquireState( gentity_t *ent )
{
	int i;

	if ( !ent ) return NULL;
	if ( ent->behaviorState ) return ent->behaviorState;   /* already holds one */

	for ( i = 0; i < MAX_MONSTERS; i++ ) {
		if ( !behaviorPoolUsed[i] ) {
			behaviorPoolUsed[i] = qtrue;
			memset( &behaviorPool[i], 0, sizeof( behaviorPool[i] ) );
			behaviorPool[i].active = qtrue;
			ent->behaviorState = &behaviorPool[i];
			return ent->behaviorState;
		}
	}

	Com_Log( SEV_WARN, LOG_CH(ch_behavior),
		"behavior-state pool exhausted (%d slots) — entity %d gets no behavior\n",
		MAX_MONSTERS, (int)( ent - g_entities ) );
	return NULL;
}

/*
=================
Behavior_ReleaseState — return this entity's slot to the pool

Idempotent: releasing an entity with no behavior-state (or a pointer outside the
pool) is a no-op. Clears ent->behaviorState.
=================
*/
void Behavior_ReleaseState( gentity_t *ent )
{
	if ( !ent || !ent->behaviorState ) return;

	ptrdiff_t idx = ent->behaviorState - behaviorPool;
	if ( idx >= 0 && idx < MAX_MONSTERS ) {
		behaviorPoolUsed[idx] = qfalse;
	}
	ent->behaviorState = NULL;
}

/*
=================
Behavior_SlotSize / Behavior_SaveSlot / Behavior_LoadSlot — savegame accessors

The pool (behaviorPool / behaviorPoolUsed) is file-static, so the Phase-4
serializer reaches a slot only through these. behaviorState_t is pure data (all
int/float/qboolean; entity refs are ENTITYNUM values, not pointers), so a slot
serializes as a verbatim blob. Slot<->entity association is recovered on load by
re-acquiring a slot for the entity, so the pool's internal used[] map is NOT
serialized — it is rebuilt by the load re-acquiring exactly the saved entities.
=================
*/
size_t Behavior_SlotSize( void )
{
	return sizeof( behaviorState_t );
}

int Behavior_SaveSlot( gentity_t *ent, void *out, size_t slotBytes )
{
	if ( !ent || !ent->behaviorState || slotBytes != sizeof( behaviorState_t ) ) {
		return 0;   // no slot to save (or a size-mismatch guard)
	}
	memcpy( out, ent->behaviorState, sizeof( behaviorState_t ) );
	return 1;
}

int Behavior_LoadSlot( gentity_t *ent, const void *in, size_t slotBytes )
{
	behaviorState_t *bs;

	if ( !ent || slotBytes != sizeof( behaviorState_t ) ) {
		return 0;
	}
	bs = Behavior_AcquireState( ent );   // grabs a free pool slot, links ent->behaviorState
	if ( !bs ) {
		return 0;   // pool exhausted
	}
	memcpy( bs, in, sizeof( behaviorState_t ) );
	bs->active = qtrue;   // the acquired slot is live regardless of the saved flag
	return 1;
}

/* ── The first native behavior states ─────────────────────────────────────
 *
 * Battle / Hunt / TakeCover — a small monster-agnostic combat FSM. The DECIDE
 * (which state) is pure C and throttled: it re-evaluates on a slow tick
 * (BEHAVIOR_THINK_MS), NOT every server frame, so N monsters cost N cheap
 * decides per ~150ms, not per frame. Between decides the current state keeps
 * ACTING every frame (Battle keeps swinging on its own timer; Hunt/TakeCover
 * ride the nav-follower, which advances continuously below). This decide-slow /
 * act-fast split is what makes a crowd of monsters affordable — the behavior
 * analog of the nav-follower's per-leg (not per-frame) re-steer.
 *
 * No Lua here: the decide is 100% native. A monster with a Lua decide-override
 * is a later opt-in; the C FSM is the floor every monster falls to. */

#define BEHAVIOR_THINK_MS      150    /* re-decide cadence (≈6-7 decides/sec)  */
#define BEHAVIOR_ATTACK_MS     600    /* min gap between melee swings          */
#define BEHAVIOR_SIGHT_RANGE   2000.0f /* beyond this the enemy is "lost"      */
#define BEHAVIOR_BATTLE_RANGE  80.0f  /* within this = in melee range          */
#define BEHAVIOR_MELEE_REACH   80.0f  /* melee trace length                    */
#define BEHAVIOR_MELEE_DAMAGE  15     /* per-swing damage                      */
#define BEHAVIOR_EYE_HEIGHT    48.0f  /* monster eye above its origin          */
#define BEHAVIOR_LOWHEALTH     30     /* at/below this health → TakeCover      */
#define BEHAVIOR_COVER_DIST    400.0f /* how far to retreat for cover          */
#define BEHAVIOR_HUNT_GIVEUP   8000   /* ms hunting a cold trail before idle   */
#define BEHAVIOR_COVER_HOLD    8000   /* ms hard cap in cover before re-engage */
#define BEHAVIOR_SIGHT_GRACE   500    /* ms a lost LOS is tolerated before Hunt */
#define BEHAVIOR_COVER_REGEN   10     /* health regained per decide tick in cover */
#define BEHAVIOR_HEALTH_FULL   100    /* absolute health ceiling (full)         */
#define BEHAVIOR_COVER_CAP     50     /* in-cover regen ceiling: recover enough
                                       * to clear the low-health line and re-engage,
                                       * but NEVER back to full — so damage taken
                                       * across fights accumulates and a monster
                                       * under sustained fire stays killable
                                       * (LOWHEALTH < this < HEALTH_FULL)        */

/* ── awareness axis tunables ── */
#define BEHAVIOR_HEAR_RANGE    700.0f /* sense enemy noise within this, no LOS   */
#define BEHAVIOR_HEAR_GRACE    3000   /* ms a heard noise stays "recent"         */
#define BEHAVIOR_QUERY_ARRIVE  64.0f  /* investigate goal reached within this    */
#define BEHAVIOR_QUERY_GIVEUP  5000   /* ms investigating nothing before relax   */
#define BEHAVIOR_ALERT_GIVEUP  10000  /* ms searching (alert) before relax       */
#define BEHAVIOR_WANDER_RADIUS 48.0f  /* patrol wander goal reached within this  */
#define BEHAVIOR_FLEE_DIST     600.0f /* how far to run in a no-cover flee        */

/* bbInt[1] — level.time of this monster's last melee swing (Battle timer). */
#define BB_LAST_ATTACK 1
/* bbInt[2] — level.time the enemy was last actually seen (sight hysteresis). */
#define BB_LAST_SEEN   2
/* bbInt[3] — level.time the enemy was last heard (hearing recency). */
#define BB_LAST_HEARD  3

/* g_skill=3 is exact authored identity.  Easier/harder levels change a small
 * native attribute band, not the number of behavior ticks per server frame:
 * perception, reaction cadence and attack cadence/damage.  The value is
 * snapped at spawn into behaviorState_t, so this adds no cvar work to the hot
 * loop and gives the later character-table bands a stable carrier. */
static int Behavior_SkillIndex( const behaviorState_t *bs )
{
	return Com_Clamp( 1, 5, bs ? bs->difficultySkill : 3 ) - 1;
}

static int Behavior_ThinkInterval( const behaviorState_t *bs )
{
	static const int value[5] = { 220, 180, BEHAVIOR_THINK_MS, 125, 100 };
	return value[Behavior_SkillIndex( bs )];
}

static int Behavior_AttackInterval( const behaviorState_t *bs )
{
	static const int value[5] = { 800, 700, BEHAVIOR_ATTACK_MS, 525, 450 };
	return value[Behavior_SkillIndex( bs )];
}

static int Behavior_MeleeDamage( const behaviorState_t *bs )
{
	static const int value[5] = { 10, 12, BEHAVIOR_MELEE_DAMAGE, 18, 20 };
	return value[Behavior_SkillIndex( bs )];
}

static float Behavior_HearRange( const behaviorState_t *bs )
{
	static const float scale[5] = { 0.75f, 0.875f, 1.0f, 1.125f, 1.25f };
	return BEHAVIOR_HEAR_RANGE * scale[Behavior_SkillIndex( bs )];
}

/*
=================
Behavior_EnemyEnt — resolve the current enemy entity, or NULL

Returns NULL when there is no enemy, the enemy slot is free, or the enemy is
dead — so every state can treat NULL as "no valid target".
=================
*/
static gentity_t *Behavior_EnemyEnt( behaviorState_t *bs )
{
	gentity_t *enemy;

	if ( bs->enemy < 0 || bs->enemy >= MAX_GENTITIES || bs->enemy == ENTITYNUM_NONE ) {
		return NULL;
	}
	enemy = &g_entities[bs->enemy];
	if ( !enemy->inuse || enemy->health <= 0 ) {
		return NULL;
	}
	return enemy;
}

/*
=================
Behavior_CanSeeEnemy — throttled line-of-sight test

Reuses CanDamage (a solid-only multi-sample trace to the target bounds) from the
monster's eye. Cheap: a handful of traces, and only ever called from the decide
tick (BEHAVIOR_THINK_MS), never per frame.
=================
*/
static qboolean Behavior_CanSeeEnemy( gentity_t *ent, gentity_t *enemy )
{
	vec3_t eye;

	if ( !enemy ) return qfalse;

	VectorCopy( ent->r.currentOrigin, eye );
	eye[2] += BEHAVIOR_EYE_HEIGHT;
	return CanDamage( enemy, eye );
}

/*
=================
Behavior_CanHearEnemy — throttled hearing test (the QUERY sense)

A minimal native-C hearing primitive: the monster senses an enemy within
BEHAVIOR_HEAR_RANGE even without line of sight (footsteps/noise carrying around
geometry). Distance-only — one cheap length test on the throttled decide tick,
never per-frame. Parallel to the bot sound-event queue (which is hard
client-slot-keyed and unusable by a non-client monster), so the bot path is
untouched. This is what lets a RELAXED monster escalate to QUERY on a noise
before it ever gets a clean line of sight.
=================
*/
static qboolean Behavior_CanHearEnemy( gentity_t *ent, behaviorState_t *bs,
		gentity_t *enemy )
{
	if ( !enemy ) return qfalse;
	return Distance( ent->r.currentOrigin, enemy->r.currentOrigin ) <= Behavior_HearRange( bs );
}

/*
=================
Behavior_MeleeAttack — one melee swing toward the enemy

No reusable client-free melee helper exists (the gauntlet paths hard-require a
client), so this inlines the minimal trace-then-damage the gauntlet does: a
forward trace from the eye toward the enemy, G_Damage on whatever solid body it
hits. Self-throttled by BEHAVIOR_ATTACK_MS via the blackboard so it swings on
its own timer independent of the decide tick.
=================
*/
static void Behavior_MeleeAttack( gentity_t *ent, behaviorState_t *bs, gentity_t *enemy )
{
	vec3_t    dir, muzzle, end;
	trace_t   tr;
	gentity_t *hit;

	if ( level.time - bs->bbInt[BB_LAST_ATTACK] < Behavior_AttackInterval( bs ) ) {
		return;   /* still in the swing cooldown */
	}

	VectorSubtract( enemy->r.currentOrigin, ent->r.currentOrigin, dir );
	dir[2] = 0.0f;
	if ( VectorNormalize( dir ) < 0.001f ) {
		return;   /* enemy exactly on top of us — no direction */
	}

	VectorCopy( ent->r.currentOrigin, muzzle );
	muzzle[2] += BEHAVIOR_EYE_HEIGHT;
	VectorMA( muzzle, BEHAVIOR_MELEE_REACH, dir, end );

	trap_Trace( &tr, muzzle, NULL, NULL, end, ent->s.number, MASK_SHOT );
	hit = &g_entities[tr.entityNum];

	if ( hit->takedamage ) {
		G_Damage( hit, ent, ent, dir, tr.endpos, Behavior_MeleeDamage( bs ), 0, MOD_GAUNTLET );
	}

	bs->bbInt[BB_LAST_ATTACK] = level.time;
	ent->s.torsoAnim = TORSO_ATTACK;
}

/*
=================
Behavior_RangedAttack — lob an aimed projectile at the enemy

A ranged monster (attackMode == ATTACK_RANGED) fires from a distance instead of a melee
swing. Unlike the melee path it aims in FULL 3D — it keeps dir[2] so a lobbed projectile
leads the enemy's height (the melee path flattens dir[2] because a swing is horizontal).
Self-throttled by the same BB_LAST_ATTACK cooldown so it fires on its own timer.
=================
*/
#define BEHAVIOR_RANGED_SPEED  600.0f  /* lavaball launch speed (arcs under gravity) */

static void Behavior_RangedAttack( gentity_t *ent, behaviorState_t *bs, gentity_t *enemy )
{
	vec3_t dir, muzzle;

	if ( level.time - bs->bbInt[BB_LAST_ATTACK] < Behavior_AttackInterval( bs ) ) {
		return;   /* still in the throw cooldown */
	}

	/* Muzzle at eye height so the ball leaves the model, not the feet. */
	VectorCopy( ent->r.currentOrigin, muzzle );
	muzzle[2] += BEHAVIOR_EYE_HEIGHT;

	/* Aim at the enemy in 3D — keep dir[2] so the lob leads the player's height. */
	VectorSubtract( enemy->r.currentOrigin, muzzle, dir );
	if ( VectorNormalize( dir ) < 0.001f ) {
		return;   /* enemy exactly on the muzzle — no direction */
	}

	fire_q1_lavaball( ent, muzzle, dir, BEHAVIOR_RANGED_SPEED );

	bs->bbInt[BB_LAST_ATTACK] = level.time;
	ent->s.torsoAnim = TORSO_ATTACK;
}

/*
=================
Behavior_FaceEnemy — yaw the monster toward the enemy (visible-state only)

Sets s.apos so the monster body points at its target; no movement.
=================
*/
static void Behavior_FaceEnemy( gentity_t *ent, gentity_t *enemy )
{
	vec3_t dir, ang;

	VectorSubtract( enemy->r.currentOrigin, ent->r.currentOrigin, dir );
	dir[2] = 0.0f;
	if ( VectorNormalize( dir ) < 0.001f ) return;

	vectoangles( dir, ang );
	ent->s.apos.trType = TR_STATIONARY;
	VectorCopy( ang, ent->s.apos.trBase );
	ent->s.angles[YAW] = ang[YAW];
}

/*
=================
Behavior_HuntGoal — the position a Hunt should walk to

The last-known-enemy-pos when we have one, else the enemy's live origin (a
monster that was assigned an enemy it has not yet cleanly seen still knows
roughly where it is). Guards against hunting toward an unseeded (0,0,0) goal.
=================
*/
static void Behavior_HuntGoal( behaviorState_t *bs, gentity_t *enemy, vec3_t out )
{
	if ( bs->bbInt[BB_LAST_SEEN] != 0 ||
	     bs->bbFloat[BB_LKP_X] != 0.0f ||
	     bs->bbFloat[BB_LKP_Y] != 0.0f ||
	     bs->bbFloat[BB_LKP_Z] != 0.0f ) {
		out[0] = bs->bbFloat[BB_LKP_X];
		out[1] = bs->bbFloat[BB_LKP_Y];
		out[2] = bs->bbFloat[BB_LKP_Z];
		return;
	}
	VectorCopy( enemy->r.currentOrigin, out );
}

/*
=================
Behavior_SensePoint — the last-sensed enemy position (Query/Alert investigate)

The blackboard LKP is seeded on both sight and hearing, so it doubles as the
"where I last sensed the threat" point that Query walks toward.
=================
*/
static void Behavior_SensePoint( behaviorState_t *bs, vec3_t out )
{
	out[0] = bs->bbFloat[BB_LKP_X];
	out[1] = bs->bbFloat[BB_LKP_Y];
	out[2] = bs->bbFloat[BB_LKP_Z];
}

/*
=================
Behavior_WanderGoal — pick a random reachable nav point to patrol toward

RELAXED patrol: a random walkable navmesh point (trap_Nav_GetRandomPoint,
NAVPOLY_WALKABLE). Returns qfalse if the navmesh has no point (then RELAXED just
idles). The nav layer owns the RNG, so this is cheap + deterministic.
=================
*/
static qboolean Behavior_WanderGoal( vec3_t out )
{
#if FEAT_RECAST_NAVMESH
	return trap_Nav_GetRandomPoint( NAVPOLY_WALKABLE, out );
#else
	(void)out;
	return qfalse;
#endif
}

/*
=================
Behavior_StopNav — cancel any nav goal this monster handed the follower

The behavior FSM owns the follower for a thinking monster: it starts it (Hunt /
TakeCover) and stops it (Battle, or on give-up). Clearing navFollower returns
the monster to a stand; the nav-state slot stays acquired for the next drive.
=================
*/
static void Behavior_StopNav( gentity_t *ent )
{
	if ( ent->navFollower ) {
		ent->navFollower  = qfalse;
		VectorClear( ent->s.pos.trDelta );
		ent->s.pos.trType = TR_STATIONARY;
		ent->s.pos.trTime = level.time;
	}
}

/*
=================
Behavior_DriveToGoal — send the monster walking to goal via the nav-follower

The seam where the decision layer hands off to the movement layer: it calls
Nav_StartFollower (the same follower the nav dev-command drives), which sets
navFollower + navGoal so G_RunNavFollower walks it there. G_RunBehavior rides
that follower every frame (see below) so the monster keeps thinking while it
moves. Returns qfalse if the nav pool is exhausted.
=================
*/
static qboolean Behavior_DriveToGoal( gentity_t *ent, const vec3_t goal )
{
	return Nav_StartFollower( ent, goal, 0 /* player-size mesh */ );
}

/*
=================
Behavior_Transition — enter a new state, stamping the entry time

Logs the transition (state A→B with the trigger) so the FSM is observable, and
resets stateEntered for the new state's timers.
=================
*/
static const char *Behavior_StateName( int state )
{
	switch ( state ) {
	case BSTATE_IDLE:      return "idle";
	case BSTATE_RELAXED:   return "relaxed";
	case BSTATE_QUERY:     return "query";
	case BSTATE_ALERT:     return "alert";
	case BSTATE_BATTLE:    return "battle";
	case BSTATE_HUNT:      return "hunt";
	case BSTATE_TAKECOVER: return "takecover";
	case BSTATE_FLEE:      return "flee";
	default:               return "?";
	}
}

static void Behavior_Transition( gentity_t *ent, behaviorState_t *bs, int newState, const char *why )
{
	if ( bs->state == newState ) return;

	Com_Log( SEV_INFO, LOG_CH(ch_behavior),
		"monster %d: %s -> %s (%s)\n",
		(int)( ent - g_entities ),
		Behavior_StateName( bs->state ), Behavior_StateName( newState ), why );

	bs->state        = newState;
	bs->stateEntered = level.time;
}

/*
=================
Behavior_KeyToState — map a Lua-returned state-key string onto a state

The Lua decide-fn returns a state-key (the same string vocabulary as the bot
decide oracle). Only the SELECTION comes from Lua; the native C states still
EXECUTE. Recognized keys map to a combat state; anything unknown/empty returns
BSTATE_COUNT so the caller falls back to the C FSM (graceful).
=================
*/
static int Behavior_KeyToState( const char *key )
{
	if ( !key || !key[0] )                       return BSTATE_COUNT;   /* unknown → C */
	/* combat axis */
	if ( !Q_stricmp( key, "battle" ) )           return BSTATE_BATTLE;
	if ( !Q_stricmp( key, "fight" ) )            return BSTATE_BATTLE;
	if ( !Q_stricmp( key, "attack" ) )           return BSTATE_BATTLE;
	if ( !Q_stricmp( key, "hunt" ) )             return BSTATE_HUNT;
	if ( !Q_stricmp( key, "chase" ) )            return BSTATE_HUNT;
	if ( !Q_stricmp( key, "takecover" ) )        return BSTATE_TAKECOVER;
	if ( !Q_stricmp( key, "retreat" ) )          return BSTATE_TAKECOVER;
	if ( !Q_stricmp( key, "cover" ) )            return BSTATE_TAKECOVER;
	if ( !Q_stricmp( key, "flee" ) )             return BSTATE_FLEE;
	if ( !Q_stricmp( key, "panic" ) )            return BSTATE_FLEE;
	/* awareness axis */
	if ( !Q_stricmp( key, "relaxed" ) )          return BSTATE_RELAXED;
	if ( !Q_stricmp( key, "patrol" ) )           return BSTATE_RELAXED;
	if ( !Q_stricmp( key, "wander" ) )           return BSTATE_RELAXED;
	if ( !Q_stricmp( key, "query" ) )            return BSTATE_QUERY;
	if ( !Q_stricmp( key, "investigate" ) )      return BSTATE_QUERY;
	if ( !Q_stricmp( key, "alert" ) )            return BSTATE_ALERT;
	if ( !Q_stricmp( key, "search" ) )           return BSTATE_ALERT;
	if ( !Q_stricmp( key, "idle" ) )             return BSTATE_IDLE;
	if ( !Q_stricmp( key, "roam" ) )             return BSTATE_RELAXED;
	return BSTATE_COUNT;   /* unrecognized → fall to C FSM */
}

/*
=================
Behavior_CoverRegen — the monster's in-cover health recovery, applied once/think

The single source of the TakeCover regen, shared by both the C-FSM decide path and
the Lua act path so the two can never drift or double-apply. A non-client monster
has no other health recovery, and the TakeCover exit keys on health rising back
above the low-health line — so recovery must happen, but it is CAPPED below full:
a monster recovers just enough to clear the low-health line and re-engage, never
back to full. That keeps damage cumulative across fights (the ceiling is below full,
so sustained fire finishes a monster instead of it resetting to full each retreat)
while still letting it legitimately leave cover (the cap sits above the low-health
line, so health always crosses the exit threshold — no livelock).
=================
*/
static void Behavior_CoverRegen( gentity_t *ent )
{
	if ( ent->health < BEHAVIOR_COVER_CAP ) {
		ent->health += BEHAVIOR_COVER_REGEN;
		if ( ent->health > BEHAVIOR_COVER_CAP ) ent->health = BEHAVIOR_COVER_CAP;
	}
}

/*
=================
Behavior_ActState — run the native ACTION for a state, every decide tick

The F2-hybrid ACT half: given the state the decision layer chose, this drives its
native behavior each tick — Battle closes to melee, TakeCover retreats + regens,
Hunt walks/keeps its goal. It performs the MAINTENANCE the C switch does per tick
(nav re-drive, health regen) but takes NO exit decision — leaving a state is the
decision layer's job (Lua on the Lua path). Entering a new state seeds its goal;
staying in one maintains it. Used by the Lua path so a Lua-selected state acts
exactly like a C-selected one, without the C exit-transition logic Lua replaces.
=================
*/
static void Behavior_ActState( gentity_t *ent, behaviorState_t *bs, gentity_t *enemy, int newState )
{
	vec3_t goal, dir;
	qboolean entering = ( newState != bs->state );

	if ( entering ) {
		Behavior_Transition( ent, bs, newState, "lua" );
	}

	switch ( newState ) {
	case BSTATE_BATTLE:
		/* close to melee via nav when out of range, else stand (the per-frame
		 * act path swings). Mirrors the C Battle maintenance. */
		if ( enemy && Distance( ent->r.currentOrigin, enemy->r.currentOrigin ) > BEHAVIOR_BATTLE_RANGE ) {
			if ( !ent->navFollower ) {
				Behavior_DriveToGoal( ent, enemy->r.currentOrigin );
			}
		} else {
			Behavior_StopNav( ent );
		}
		break;
	case BSTATE_HUNT:
	case BSTATE_ALERT:
		/* walk to the last-known/last-sensed enemy pos; (re)issue if not moving.
		 * Alert is Hunt's more-aware sibling (same drive; the decision layer
		 * gives up faster from Alert). */
		if ( !ent->navFollower ) {
			Behavior_HuntGoal( bs, enemy, goal );
			Behavior_DriveToGoal( ent, goal );
		}
		break;
	case BSTATE_QUERY:
		/* investigate the last-sensed point: walk toward where the noise/glimpse
		 * came from. (re)issue the goal if not already moving. */
		if ( !ent->navFollower ) {
			Behavior_SensePoint( bs, goal );
			Behavior_DriveToGoal( ent, goal );
		}
		break;
	case BSTATE_RELAXED:
		/* patrol: wander to a random nav point, pick a fresh one on arrival */
		if ( !ent->navFollower ) {
			if ( Behavior_WanderGoal( goal ) ) {
				Behavior_DriveToGoal( ent, goal );
			}
		}
		break;
	case BSTATE_TAKECOVER:
		/* regen while covering (the monster's only health recovery, capped below
		 * full — see Behavior_CoverRegen), and keep a retreat goal live so it
		 * actually reaches cover */
		Behavior_CoverRegen( ent );
		if ( entering && enemy ) {
			VectorSubtract( ent->r.currentOrigin, enemy->r.currentOrigin, dir );
			dir[2] = 0.0f;
			if ( VectorNormalize( dir ) < 0.001f ) { dir[0] = 1.0f; }
			VectorMA( ent->r.currentOrigin, BEHAVIOR_COVER_DIST, dir, goal );
			Behavior_DriveToGoal( ent, goal );
		}
		break;
	case BSTATE_FLEE:
		/* no-cover panic: run directly away from the enemy (no regen, no hold —
		 * TakeCover is the recover-in-place variant; Flee just keeps running) */
		if ( entering && enemy ) {
			VectorSubtract( ent->r.currentOrigin, enemy->r.currentOrigin, dir );
			dir[2] = 0.0f;
			if ( VectorNormalize( dir ) < 0.001f ) { dir[0] = 1.0f; }
			VectorMA( ent->r.currentOrigin, BEHAVIOR_FLEE_DIST, dir, goal );
			Behavior_DriveToGoal( ent, goal );
		}
		break;
	case BSTATE_IDLE:
	default:
		Behavior_StopNav( ent );
		break;
	}
}

/*
=================
Behavior_LuaDecide — Lua selects the next state; the native C state then acts

Opt-in and throttled: only called when bs->luaDecide and only at the decide tick
(~6.7/sec, never per-frame). Builds the decide context, calls the monster Lua
trap for a state-key, maps it, and runs that state's native action via
Behavior_ActState (so a Lua-selected state moves/attacks/regens exactly like a
C-selected one — the F2-hybrid contract: Lua decides WHICH, native C ACTS).
Returns qfalse (caller runs the full C FSM) when the key is unknown/empty or the
trap declined — an ill-behaved Lua-fn degrades gracefully to native C.
=================
*/
static qboolean Behavior_LuaDecide( gentity_t *ent, behaviorState_t *bs,
                                    gentity_t *enemy, qboolean canSee, qboolean canHear, float dist )
{
	wbDecideCtx_t ctx;
	char          key[64];
	int           entnum = (int)( ent - g_entities );
	int           newState;

	memset( &ctx, 0, sizeof( ctx ) );
	ctx.health       = ent->health;
	ctx.armor        = 0;
	ctx.enemyVisible = canSee ? 1 : 0;
	ctx.enemyDist    = dist;
	ctx.enemyHealth  = enemy ? enemy->health : 0;
	ctx.underFire    = 0;
	ctx.timeLeft     = 0;
	/* Awareness inputs — heard/sensed without sight, and the distance. Lets the
	 * Lua decide-fn drive the RELAXED→QUERY→ALERT escalation. */
	ctx.sensedNoise  = ( canHear && !canSee ) ? 1 : 0;
	ctx.noiseDist    = canHear ? dist : 0.0f;

	key[0] = '\0';
	if ( !trap_MonsterLuaDecide( entnum, &ctx, key, sizeof( key ) ) ) {
		return qfalse;   /* trap declined (unbound / no ctx) → C FSM */
	}

	newState = Behavior_KeyToState( key );
	if ( newState == BSTATE_COUNT ) {
		return qfalse;   /* unrecognized key → C FSM (graceful) */
	}

	Behavior_ActState( ent, bs, enemy, newState );
	return qtrue;
}

/*
=================
Behavior_Decide — the throttled FSM tick: evaluate + transition

Pure C. Runs at most once per BEHAVIOR_THINK_MS. Two composed axes:

  AWARENESS (perception): with no confirmed threat the monster sits on the
    awareness axis — RELAXED (patrol) when nothing is sensed, QUERY (investigate)
    when it HEARS but cannot SEE the enemy, escalating to ALERT/COMBAT on sight.
    It de-escalates back down as the threat goes stale.
  COMBAT (action): once the enemy is SEEN, the monster enters the combat cluster
    — Battle / Hunt / TakeCover — exactly as before.

The sensory primitives (sight = CanDamage, hearing = a distance check) run here on
the throttled tick, never per-frame. A Lua decide-fn (opt-in) can override the
state selection; the pure-C FSM below is the default + the FPS floor.
=================
*/
static void Behavior_Decide( gentity_t *ent, behaviorState_t *bs )
{
	gentity_t *enemy;
	qboolean   canSee, canHear;
	float      dist;
	vec3_t     lkp, cover, dir;
	qboolean   sightRecent, heardRecent;

	enemy = Behavior_EnemyEnt( bs );

	/* No valid enemy at all → relax and patrol (the top of the awareness axis).
	 * A wandering RELAXED monster still re-acquires when a target is assigned. */
	if ( !enemy ) {
		if ( bs->state != BSTATE_RELAXED ) {
			Behavior_StopNav( ent );
			Behavior_Transition( ent, bs, BSTATE_RELAXED, "no threat" );
		}
		Behavior_ActState( ent, bs, NULL, BSTATE_RELAXED );   /* keep patrolling */
		return;
	}

	canSee  = Behavior_CanSeeEnemy( ent, enemy );
	canHear = Behavior_CanHearEnemy( ent, bs, enemy );
	dist    = Distance( ent->r.currentOrigin, enemy->r.currentOrigin );

	/* Seed last-known/sensed pos + stamp the recency timers. Sight seeds a clean
	 * position; hearing seeds a coarse one (the enemy's rough position) so QUERY
	 * has somewhere to investigate. */
	if ( canSee ) {
		bs->bbFloat[BB_LKP_X] = enemy->r.currentOrigin[0];
		bs->bbFloat[BB_LKP_Y] = enemy->r.currentOrigin[1];
		bs->bbFloat[BB_LKP_Z] = enemy->r.currentOrigin[2];
		bs->bbInt[BB_LAST_SEEN] = level.time;
	}
	if ( canHear ) {
		bs->bbFloat[BB_LKP_X] = enemy->r.currentOrigin[0];
		bs->bbFloat[BB_LKP_Y] = enemy->r.currentOrigin[1];
		bs->bbFloat[BB_LKP_Z] = enemy->r.currentOrigin[2];
		bs->bbInt[BB_LAST_HEARD] = level.time;
	}

	/* Recency windows: sight hysteresis (tolerate a single noisy trace) + a
	 * longer hearing window (a noise lingers in memory). */
	sightRecent = ( level.time - bs->bbInt[BB_LAST_SEEN] )  <= BEHAVIOR_SIGHT_GRACE;
	heardRecent = ( level.time - bs->bbInt[BB_LAST_HEARD] ) <= BEHAVIOR_HEAR_GRACE;

	/* Lua-decide opt-in: a monster bound to a character with a Lua "decide"
	 * override lets Lua SELECT the next state (the native C states still act).
	 * Opt-in only — luaDecide is false for every other monster, which then runs
	 * the pure-C FSM below. Same throttled tick as the C FSM (this whole function
	 * fires once per BEHAVIOR_THINK_MS), so a Lua monster makes ~6.7 VM-calls/sec.
	 * A declined/unknown key returns qfalse and falls through to the C FSM. */
	if ( bs->luaDecide && Behavior_LuaDecide( ent, bs, enemy, sightRecent, canHear, dist ) ) {
		return;
	}

	/* ── AWARENESS gate (pure-C default) ────────────────────────────────────
	 * Before the combat cluster: a monster NOT already engaged (i.e. sitting on
	 * the awareness axis — Idle/Relaxed/Query) that has no recent sight sits on
	 * that axis: Query if a noise lingers, else Relaxed patrol, escalating to
	 * Battle on fresh sight. The engaged combat states (Battle/Hunt/TakeCover/
	 * Alert) are excluded — each owns its own de-escalation below. */
	if ( !sightRecent
	     && bs->state != BSTATE_BATTLE && bs->state != BSTATE_HUNT
	     && bs->state != BSTATE_TAKECOVER && bs->state != BSTATE_ALERT ) {
		if ( canSee ) {
			/* fresh sight while relaxed/querying → engage */
			Behavior_StopNav( ent );
			Behavior_Transition( ent, bs, BSTATE_BATTLE, "sighted enemy" );
			/* fall through into the combat switch below */
		} else if ( heardRecent ) {
			/* heard but not seen → investigate */
			if ( bs->state != BSTATE_QUERY ) {
				Behavior_Transition( ent, bs, BSTATE_QUERY, "heard enemy" );
			}
			/* give up querying a cold noise → relax */
			if ( level.time - bs->bbInt[BB_LAST_HEARD] > BEHAVIOR_QUERY_GIVEUP ) {
				Behavior_Transition( ent, bs, BSTATE_RELAXED, "noise faded" );
			}
			Behavior_ActState( ent, bs, enemy, bs->state );
			return;
		} else {
			/* nothing sensed → relax and patrol */
			if ( bs->state != BSTATE_RELAXED ) {
				Behavior_StopNav( ent );
				Behavior_Transition( ent, bs, BSTATE_RELAXED, "threat lost" );
			}
			Behavior_ActState( ent, bs, NULL, BSTATE_RELAXED );
			return;
		}
	}

	/* Rule 1 — low health outranks everything: take cover. */
	if ( ent->health <= BEHAVIOR_LOWHEALTH && bs->state != BSTATE_TAKECOVER ) {
		/* cover point = a spot COVER_DIST away from the enemy, behind us. */
		VectorSubtract( ent->r.currentOrigin, enemy->r.currentOrigin, dir );
		dir[2] = 0.0f;
		if ( VectorNormalize( dir ) < 0.001f ) { dir[0] = 1.0f; }
		VectorMA( ent->r.currentOrigin, BEHAVIOR_COVER_DIST, dir, cover );
		Behavior_StopNav( ent );
		Behavior_Transition( ent, bs, BSTATE_TAKECOVER, "low health" );
		Behavior_DriveToGoal( ent, cover );
		return;
	}

	switch ( bs->state ) {
	case BSTATE_ALERT:
		/* actively searching a known-but-unseen enemy (Hunt's aware sibling):
		 * re-sight → engage; heard again → keep searching; long silence → relax */
		if ( canSee ) {
			Behavior_StopNav( ent );
			Behavior_Transition( ent, bs, BSTATE_BATTLE, "re-sighted enemy" );
		} else if ( level.time - bs->stateEntered > BEHAVIOR_ALERT_GIVEUP ) {
			Behavior_StopNav( ent );
			Behavior_Transition( ent, bs, BSTATE_RELAXED, "search exhausted" );
		} else {
			Behavior_ActState( ent, bs, enemy, BSTATE_ALERT );   /* keep searching */
		}
		break;

	case BSTATE_TAKECOVER:
		/* Recover health while holding cover — this is the whole point of taking
		 * cover, and it is what lets the monster legitimately exit (a non-client
		 * monster has no other health regen). Without recovery a permanently-low
		 * monster would re-trip Rule 1 the instant it left cover and livelock. The
		 * recovery is capped below full (Behavior_CoverRegen) so it clears the
		 * low-health line and re-engages, but does not reset to full every retreat. */
		Behavior_CoverRegen( ent );

		/* Exit once actually recovered above the low-health line → re-engage
		 * (Battle if visible, else Hunt). The hard-cap timer is a give-up: if it
		 * somehow never recovers, forget the enemy and idle rather than flap. */
		if ( ent->health > BEHAVIOR_LOWHEALTH ) {
			Behavior_StopNav( ent );
			if ( canSee ) {
				Behavior_Transition( ent, bs, BSTATE_BATTLE, "recovered, enemy in sight" );
			} else {
				Behavior_Transition( ent, bs, BSTATE_HUNT, "recovered, enemy unseen" );
				Behavior_HuntGoal( bs, enemy, lkp );
				Behavior_DriveToGoal( ent, lkp );
			}
		} else if ( level.time - bs->stateEntered > BEHAVIOR_COVER_HOLD ) {
			/* never recovered → break contact and drop to the awareness axis */
			Behavior_StopNav( ent );
			Behavior_Transition( ent, bs, BSTATE_RELAXED, "cover timed out" );
		}
		break;

	case BSTATE_BATTLE:
		if ( !sightRecent ) {
			/* sustained loss of sight (past the grace window) → hunt the
			 * last-known position */
			Behavior_Transition( ent, bs, BSTATE_HUNT, "lost sight" );
			Behavior_HuntGoal( bs, enemy, lkp );
			Behavior_DriveToGoal( ent, lkp );
		} else if ( dist > BEHAVIOR_BATTLE_RANGE ) {
			/* in-sight but out of melee → close in via nav (still "battle"
			 * intent, drive toward the live enemy pos). Re-issue only when not
			 * already following, so an in-flight leg is not reset each tick. */
			if ( !ent->navFollower ) {
				Behavior_DriveToGoal( ent, enemy->r.currentOrigin );
			}
		} else {
			/* in range → stand and swing */
			Behavior_StopNav( ent );
		}
		break;

	case BSTATE_HUNT:
		if ( canSee ) {
			/* re-sighted → engage */
			Behavior_StopNav( ent );
			Behavior_Transition( ent, bs, BSTATE_BATTLE, "re-sighted enemy" );
		} else if ( level.time - bs->stateEntered > BEHAVIOR_HUNT_GIVEUP ) {
			/* reached the last-known spot and found nothing → de-escalate one
			 * step to ALERT (keep the target, widen the search) rather than
			 * dropping straight to relaxed. The awareness axis keeps the assigned
			 * enemy for re-acquisition; Alert's own give-up relaxes it fully. */
			Behavior_StopNav( ent );
			Behavior_Transition( ent, bs, BSTATE_ALERT, "trail cold, searching" );
			Behavior_HuntGoal( bs, enemy, lkp );
			Behavior_DriveToGoal( ent, lkp );
		}
		/* else keep walking to last-known pos (nav-follower already driving) */
		break;

	case BSTATE_IDLE:
	default:
		/* Idle/relaxed with a target in hand → engage per what we sense. */
		if ( canSee ) {
			Behavior_Transition( ent, bs, BSTATE_BATTLE, "enemy sighted" );
		} else {
			Behavior_Transition( ent, bs, BSTATE_HUNT, "enemy known, unseen" );
			Behavior_HuntGoal( bs, enemy, lkp );
			Behavior_DriveToGoal( ent, lkp );
		}
		break;
	}
}

/*
=================
Behavior_MonsterDie — a behaving monster's death callback

Non-client monsters have no client death path (no respawn queue, no corpse
handling), so a killed monster simply frees its entity. G_FreeEntity returns
both the behavior-state and nav-state pool slots, so nothing is stranded. Without
a die handler an entity with takedamage set would NULL-deref ent->die when killed.
=================
*/
void Behavior_MonsterDie( gentity_t *self, gentity_t *inflictor, gentity_t *attacker,
                          int damage, int mod )
{
	(void)inflictor; (void)damage; (void)mod;

	Com_Log( SEV_INFO, LOG_CH(ch_behavior),
		"monster %d: killed\n", (int)( self - g_entities ) );

	// Gameplay telemetry on the shared bot-nav stream: which killer dropped which
	// monster, plus the running census, so a playthrough harness can count kills.
	// attacker is NULL for world/environmental damage (lava, crusher) → killer -1.
	level.numMonstersKilled++;
	Com_Log( SEV_INFO, LOG_CH(ch_botai),
		"monster %d killed by %d (%d/%d)\n",
		(int)( self - g_entities ),
		attacker ? (int)( attacker - g_entities ) : -1,
		level.numMonstersKilled, level.numMonstersSpawned );

	/* Fire the death event BEFORE the entity frees, so its death block can run a
	 * final verb (e.g. trigger a downstream event). No-op for a non-scripted
	 * monster. The death block cannot outlive the free — it runs its immediate
	 * verbs here; a block that then yields is cut off when the slot releases. */
	Script_FireEvent( self, "death", NULL );
	G_RunScriptDispatcher( self );

	self->aiThink    = qfalse;
	self->takedamage = qfalse;
	Behavior_StopNav( self );

	/* Play the death animation, then free the corpse after it has had time to run.
	 * s.legsAnim = MANIM_DEATH is a code the client resolves to the death frame
	 * range (which plays ONCE — loopFrames 0 — and holds the last frame). The
	 * generic think dispatcher still runs after aiThink clears, so a deferred
	 * G_FreeEntity think reaps the corpse (mirrors the BodySink corpse pattern). */
	self->s.legsAnim = ( ( self->s.legsAnim & ANIM_TOGGLEBIT ) ^ ANIM_TOGGLEBIT ) | MANIM_DEATH;
	self->think     = G_FreeEntity;
	self->nextthink = level.time + 2000;
}

/*
=================
G_RunBehavior — advance one behaving non-client entity this server frame

The monster's single frame owner (the G_RunFrame dispatch routes a thinking
monster here, not to the nav-follower runner, even when it is following — see
the aiThink guard on the nav-follower branch). Two cadences:

  DECIDE (throttled): at most once per BEHAVIOR_THINK_MS, evaluate the FSM and
    transition / re-seed goals (Behavior_Decide).
  ACT (every frame): Battle swings on its own attack timer; Hunt/TakeCover ride
    the nav-follower continuously by delegating to G_RunNavFollower.

So the decision is slow and the action is smooth — a crowd of monsters pays only
the throttled decide per monster, while movement stays per-frame via the nav
layer it shares with the follower dev-command.
=================
*/
/*
=================
Behavior_UpdateAnim — map the behavior state to a monster animation code

Sets ent->s.legsAnim to a monsterAnim_t the client resolves to a frame range. The
mapping is the behavior taxonomy -> the 6 Q1 monster anims: idle states -> STAND,
investigate/approach -> WALK, close/flee -> RUN, in-range battle -> ATTACK. PAIN
and DEATH are event-driven (set on damage/death, not by steady-state), so they are
not produced here. ANIM_TOGGLEBIT is flipped on a code change so the client
restarts the sequence (same convention the player uses).
=================
*/
static void Behavior_UpdateAnim( gentity_t *ent, const behaviorState_t *bs )
{
	int code;

	switch ( bs->state ) {
	case BSTATE_IDLE:
	case BSTATE_RELAXED:
		code = MANIM_STAND;
		break;
	case BSTATE_QUERY:
	case BSTATE_ALERT:
	case BSTATE_HUNT:
		code = MANIM_WALK;
		break;
	case BSTATE_TAKECOVER:
	case BSTATE_FLEE:
		code = MANIM_RUN;
		break;
	case BSTATE_BATTLE:
		/* Closing on the enemy (out of melee, nav-driven) plays locomotion, not the
		 * in-place swing: navFollower is the live "moving this frame" signal — set by
		 * Behavior_Decide's closing-Battle branch when the enemy is out of melee range,
		 * and cleared by Behavior_StopNav / G_RunNavFollower on arrival. Without this a
		 * battling soldier that is still approaching the player translates its origin
		 * (nav) while the legs hold the stationary ATTACK cycle — a frozen-feet slide.
		 * In melee range nav is stopped (navFollower clear) → the ATTACK swing plays. */
		code = ent->navFollower ? MANIM_RUN : MANIM_ATTACK;
		break;
	default:
		code = MANIM_STAND;
		break;
	}

	/* Only re-issue (and flip the toggle) when the code actually changes, so the
	 * client's per-code restart fires once per transition, not every frame. */
	if ( ( ent->s.legsAnim & ~ANIM_TOGGLEBIT ) != code ) {
		ent->s.legsAnim = ( ( ent->s.legsAnim & ANIM_TOGGLEBIT ) ^ ANIM_TOGGLEBIT ) | code;
	}
}

void G_RunBehavior( gentity_t *ent )
{
	behaviorState_t *bs = ent->behaviorState;
	gentity_t       *enemy;

	if ( !bs ) {           /* aiThink set but no state slot — nothing to run */
		G_RunThink( ent );
		return;
	}

	/* ── DECIDE (throttled) ─────────────────────────────────────────────── */
	/* A scripted monster's state comes from the verb-dispatcher, not the FSM:
	 * run the script each frame (its blocking verbs suspend/resume via the int
	 * cursor) and skip Behavior_Decide entirely. An autonomous monster
	 * (scripted == qfalse, the default) takes the exact Fork-2 path below — the
	 * guard is a single branch that never fires on the autonomous hot side. */
	if ( bs->scripted ) {
		G_RunScriptDispatcher( ent );
	} else if ( level.time >= bs->nextThink ) {
		bs->nextThink = level.time + Behavior_ThinkInterval( bs );
		Behavior_Decide( ent, bs );
	}

	/* ── ANIMATE ────────────────────────────────────────────────────────── */
	/* Publish the current behavior state as an animation CODE in s.legsAnim; the
	 * CLIENT resolves it to a frame range and ticks + lerps it (mirroring the
	 * player anim path). The server never touches frame indices. */
	Behavior_UpdateAnim( ent, bs );

	/* ── ACT (every frame) ──────────────────────────────────────────────── */
	enemy = Behavior_EnemyEnt( bs );

	if ( ent->navFollower ) {
		/* Hunt / TakeCover / closing-Battle: ride the nav-follower this frame
		 * (continuous movement). It links the entity + evaluates arrival. */
		/* G_RunNavFollower clears navFollower on arrival; the next decide then
		 * re-plans (it reads ent->navFollower as the live-goal source of truth). */
		G_RunNavFollower( ent );
		return;   /* G_RunNavFollower already ran the think */
	}

	if ( bs->state == BSTATE_BATTLE && enemy ) {
		/* face the enemy, then attack on the timer. A ranged monster lobs a projectile
		 * from any distance (it does not need to close); a melee monster only swings once
		 * it is in reach. The ranged branch is additive — a melee monster (attackMode 0)
		 * takes the exact unchanged range-gated melee path. */
		Behavior_FaceEnemy( ent, enemy );
		if ( ent->attackMode == ATTACK_RANGED ) {
			Behavior_RangedAttack( ent, bs, enemy );
		} else if ( Distance( ent->r.currentOrigin, enemy->r.currentOrigin ) <= BEHAVIOR_BATTLE_RANGE ) {
			Behavior_MeleeAttack( ent, bs, enemy );
		}
	}

	G_RunThink( ent );
}

#endif /* FEAT_MONSTER_AI */
