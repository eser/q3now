// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

/*
===========================================================================
Crowd promotion (game side of the crowd hand-off, GAME_API_VERSION 11).

The engine simulates a large population of cheap agents and sends them to clients
as a sprite channel. The few that come within reach of a player are offered to the
game. Here a promoted agent becomes a VISUAL-ONLY entity: a model that is drawn
in place of the sprite, with no collision, no damage and no gameplay effect. It
keeps walking its goal; when no player is near any more it is handed back to the
crowd with its current state.

Nothing here touches Q3 mechanics: the entity has no contents, takes no damage,
is not a target and is never solid.

  g_crowdPromote 1       turn the promotion on (default 0)
  g_crowdModel <path>    the model that stands in for a promoted agent
  g_crowdProbe 1         diagnostic: promote/demote round trip, no entity
  g_crowdPromoteAim 1    diagnostic: pin the first promoted entity 96 units in front of
                         the nearest player, so a screenshot can show that it is drawn
===========================================================================
*/

#include "g_local.h"

LOG_DECLARE_CHANNEL( ch_game_crowd, "game" );

#define GC_MAX_VISUALS     8        /* upper bound on simultaneously promoted agents   */
#define GC_DEMOTE_RADIUS   576.0f   /* 1.5 x the engine's 384 unit promotion radius    */
#define GC_ARRIVE_RADIUS   24.0f
#define GC_DEFAULT_MODEL   "models/powerups/armor/armor_red.md3"

typedef struct {
	qboolean   used;
	gentity_t *ent;
	int        userId;
	vec3_t     origin;
	float      vx, vy;
	float      goalX, goalY, goalSpeed;
} gcVisual_t;

static gcVisual_t gc_visuals[GC_MAX_VISUALS];
static int        gc_lastTime;
static int        gc_modelIndex;
static int        gc_promotedTotal, gc_releasedTotal, gc_nextReport;

static int gc_nextProbe;   /* reset by G_CrowdInit: a reused VM must not carry the old level's clock */

void G_CrowdInit( void ) {
	memset( gc_visuals, 0, sizeof( gc_visuals ) );
	gc_lastTime = 0;
	gc_modelIndex = 0;
	gc_nextProbe = 0;
	gc_promotedTotal = gc_releasedTotal = gc_nextReport = 0;
}

static int GC_UsedCount( void );

static void GC_Release_All( void );

/* Level shutdown or map_restart: give every promoted agent back so the engine's promoted
 * count and its crowd stay consistent. The engine may already have cleared its crowd on a
 * map change, in which case the hand-back is refused and that is fine (quiet). */
void G_CrowdShutdown( void ) {
	const int live = GC_UsedCount();
	GC_Release_All();
	if ( live > 0 ) {
		Com_Log( SEV_INFO, LOG_CH( ch_game_crowd ), "crowd visuals: shutdown, %d handed back\n", live );
	}
}

static int GC_UsedCount( void ) {
	int n = 0;
	for ( int i = 0; i < GC_MAX_VISUALS; ++i ) n += gc_visuals[i].used ? 1 : 0;
	return n;
}

/* Distance to the nearest connected human or bot, in the plane; -1 when nobody is there. */
static float GC_NearestPlayerDistance( const vec3_t at ) {
	float best = -1.0f;
	for ( int i = 0; i < level.maxclients; ++i ) {
		const gentity_t *p = &g_entities[i];
		if ( !p->inuse || !p->client || p->client->pers.connected != CON_CONNECTED
		     || p->client->sess.sessionTeam == TEAM_SPECTATOR ) continue;
		{
			const float dx = p->client->ps.origin[0] - at[0], dy = p->client->ps.origin[1] - at[1];
			const float d = sqrtf( dx * dx + dy * dy );
			if ( best < 0.0f || d < best ) best = d;
		}
	}
	return best;
}

static void GC_FillState( const gcVisual_t *v, crowdAgentState_t *s ) {
	memset( s, 0, sizeof( *s ) );
	s->userId = v->userId;
	VectorCopy( v->origin, s->origin );
	s->velocity[0] = v->vx; s->velocity[1] = v->vy;
	s->goal[0] = v->goalX; s->goal[1] = v->goalY; s->goalSpeed = v->goalSpeed;
}

static void GC_Release( gcVisual_t *v, qboolean quiet ) {
	crowdAgentState_t s;
	GC_FillState( v, &s );
	if ( !trap_Crowd_Demote( &s ) && !quiet ) {
		Com_Log( SEV_WARN, LOG_CH( ch_game_crowd ), "crowd: agent %d could not be handed back\n", v->userId );
	}
	if ( v->ent ) G_FreeEntity( v->ent );
	memset( v, 0, sizeof( *v ) );
	++gc_releasedTotal;
}

static void GC_Release_All( void ) {
	for ( int i = 0; i < GC_MAX_VISUALS; ++i ) if ( gc_visuals[i].used ) GC_Release( &gc_visuals[i], qtrue );
}

static void GC_ChooseGoal( gcVisual_t *v, int levelTime ) {
	const unsigned h = ( (unsigned)v->userId * 2654435761u ) ^ ( (unsigned)levelTime * 40503u );
	v->goalX = v->origin[0] + (float)( (int)( h & 0x7ffu ) - 1024 );
	v->goalY = v->origin[1] + (float)( (int)( ( h >> 11 ) & 0x7ffu ) - 1024 );
	v->goalSpeed = 40.0f + (float)( ( h >> 22 ) & 0x7fu );
}

/* Walk toward the goal and stay on the ground. */
static void GC_Advance( gcVisual_t *v, float dt, int levelTime ) {
	float dx = v->goalX - v->origin[0], dy = v->goalY - v->origin[1];
	const float dist = sqrtf( dx * dx + dy * dy );
	trace_t tr;
	vec3_t from, to, next;
	if ( dist < GC_ARRIVE_RADIUS || v->goalSpeed <= 0.0f ) {
		GC_ChooseGoal( v, levelTime );
		dx = v->goalX - v->origin[0]; dy = v->goalY - v->origin[1];
	}
	{
		const float len = sqrtf( dx * dx + dy * dy );
		if ( len > 0.001f ) { v->vx = dx / len * v->goalSpeed; v->vy = dy / len * v->goalSpeed; }
		else { v->vx = v->vy = 0.0f; }
	}
	VectorSet( next, v->origin[0] + v->vx * dt, v->origin[1] + v->vy * dt, v->origin[2] );
	/* The ground under the next step; a wall or a drop turns the agent to a new goal instead. */
	VectorSet( from, next[0], next[1], next[2] + 24.0f );
	VectorSet( to, next[0], next[1], next[2] - 64.0f );
	trap_Trace( &tr, from, NULL, NULL, to, ENTITYNUM_NONE, MASK_SOLID );
	if ( tr.startsolid || tr.fraction >= 1.0f ) {
		v->vx = v->vy = 0.0f;
		GC_ChooseGoal( v, levelTime + 977 );
		return;
	}
	next[2] = tr.endpos[2];
	if ( next[2] > v->origin[2] + 18.0f ) {   /* too high a step */
		v->vx = v->vy = 0.0f;
		GC_ChooseGoal( v, levelTime + 977 );
		return;
	}
	VectorCopy( next, v->origin );
}

/* Diagnostic placement for g_crowdPromoteAim: in front of the player nearest to the agent. */
static void GC_AimAtPlayer( gcVisual_t *v ) {
	const gentity_t *best = NULL;
	float bestDist = 0.0f;
	for ( int i = 0; i < level.maxclients; ++i ) {
		const gentity_t *p = &g_entities[i];
		float d;
		if ( !p->inuse || !p->client || p->client->pers.connected != CON_CONNECTED ) continue;
		d = Distance( p->client->ps.origin, v->origin );
		if ( !best || d < bestDist ) { best = p; bestDist = d; }
	}
	if ( best ) {
		vec3_t forward;
		AngleVectors( best->client->ps.viewangles, forward, NULL, NULL );
		VectorMA( best->client->ps.origin, 96.0f, forward, v->origin );
		v->origin[2] = best->client->ps.origin[2] - 24.0f;
		v->vx = v->vy = 0.0f;
	}
}

static void GC_Publish( gcVisual_t *v, int levelTime ) {
	gentity_t *e = v->ent;
	VectorCopy( v->origin, e->s.pos.trBase );
	VectorSet( e->s.pos.trDelta, v->vx, v->vy, 0.0f );
	e->s.pos.trType = ( v->vx != 0.0f || v->vy != 0.0f ) ? TR_LINEAR : TR_STATIONARY;
	e->s.pos.trTime = levelTime;
	VectorCopy( v->origin, e->r.currentOrigin );
	if ( v->vx != 0.0f || v->vy != 0.0f ) {
		e->s.angles[YAW] = RAD2DEG( atan2f( v->vy, v->vx ) );
		VectorCopy( e->s.angles, e->r.currentAngles );
	}
	trap_LinkEntity( e );
}

static gcVisual_t *GC_Spawn( const crowdAgentState_t *s ) {
	gcVisual_t *v = NULL;
	gentity_t *e;
	for ( int i = 0; i < GC_MAX_VISUALS; ++i ) if ( !gc_visuals[i].used ) { v = &gc_visuals[i]; break; }
	if ( !v ) return NULL;
	e = G_Spawn();
	e->classname = "crowd_visual";
	e->s.eType = ET_GENERAL;
	e->s.modelindex = gc_modelIndex;
	e->r.contents = 0;
	e->clipmask = 0;
	e->takedamage = qfalse;
	VectorSet( e->r.mins, -16, -16, 0 );
	VectorSet( e->r.maxs, 16, 16, 56 );
	memset( v, 0, sizeof( *v ) );
	v->used = qtrue;
	v->ent = e;
	v->userId = s->userId;
	VectorCopy( s->origin, v->origin );
	v->vx = s->velocity[0]; v->vy = s->velocity[1];
	v->goalX = s->goal[0]; v->goalY = s->goal[1]; v->goalSpeed = s->goalSpeed;
	return v;
}

/*
Diagnostic for the boundary: promote the nearest agent, check a second promotion of the
same id fails, hand it back. Spawns nothing.
*/
static void GC_Probe( int levelTime ) {
	crowdAgentRef_t   refs[4];
	crowdAgentState_t state, again;
	int               n;
	if ( levelTime < gc_nextProbe ) return;
	gc_nextProbe = levelTime + 2000;
	if ( !trap_Cvar_VariableIntegerValue( "g_crowdProbe" ) ) return;
	n = trap_Crowd_Promotable( refs, 4 );
	if ( n <= 0 ) {
		Com_Log( SEV_INFO, LOG_CH( ch_game_crowd ), "crowd probe: no promotable agents (budget %d)\n", trap_Crowd_PromotionBudget() );
		return;
	}
	if ( !trap_Crowd_Promote( refs[0].userId, &state ) ) {
		Com_Log( SEV_INFO, LOG_CH( ch_game_crowd ), "crowd probe: promote of agent %d refused (budget %d)\n", refs[0].userId, trap_Crowd_PromotionBudget() );
		return;
	}
	if ( trap_Crowd_Promote( refs[0].userId, &again ) ) {
		Com_Log( SEV_INFO, LOG_CH( ch_game_crowd ), "crowd probe: FAIL agent %d promoted twice\n", refs[0].userId );
	}
	Com_Log( SEV_INFO, LOG_CH( ch_game_crowd ), "crowd probe: promoted agent %d at (%.0f %.0f %.0f), %d candidates, budget now %d\n",
		state.userId, state.origin[0], state.origin[1], state.origin[2], n, trap_Crowd_PromotionBudget() );
	if ( trap_Crowd_Demote( &state ) ) {
		Com_Log( SEV_INFO, LOG_CH( ch_game_crowd ), "crowd probe: agent %d demoted back\n", state.userId );
	} else {
		Com_Log( SEV_INFO, LOG_CH( ch_game_crowd ), "crowd probe: FAIL agent %d could not be demoted\n", state.userId );
	}
}

void G_CrowdFrame( int levelTime ) {
	float dt;
	int aimSlot, firstLive = -1;
	GC_Probe( levelTime );
	if ( !trap_Cvar_VariableIntegerValue( "g_crowdPromote" ) ) {
		for ( int i = 0; i < GC_MAX_VISUALS; ++i ) if ( gc_visuals[i].used ) GC_Release( &gc_visuals[i], qfalse );
		gc_lastTime = levelTime;
		return;
	}
	dt = gc_lastTime ? (float)( levelTime - gc_lastTime ) * 0.001f : 0.0f;
	gc_lastTime = levelTime;
	if ( dt > 0.25f ) dt = 0.25f;

	if ( !gc_modelIndex ) {
		char model[MAX_QPATH];
		trap_Cvar_VariableStringBuffer( "g_crowdModel", model, sizeof( model ) );
		gc_modelIndex = G_ModelIndex( model[0] ? model : GC_DEFAULT_MODEL );
	}

	for ( int i = 0; i < GC_MAX_VISUALS; ++i ) if ( gc_visuals[i].used ) { firstLive = i; break; }
	aimSlot = trap_Cvar_VariableIntegerValue( "g_crowdPromoteAim" ) ? firstLive : -1;
	/* Move what is promoted; give back what no player is near any more. */
	for ( int i = 0; i < GC_MAX_VISUALS; ++i ) {
		gcVisual_t *v = &gc_visuals[i];
		float near;
		if ( !v->used ) continue;
		near = GC_NearestPlayerDistance( v->origin );
		if ( near < 0.0f || near > GC_DEMOTE_RADIUS ) { GC_Release( v, qfalse ); continue; }
		GC_Advance( v, dt, levelTime );
		if ( i == aimSlot ) GC_AimAtPlayer( v );
		GC_Publish( v, levelTime );
	}

	if ( levelTime >= gc_nextReport ) {
		gc_nextReport = levelTime + 10000;
		Com_Log( SEV_INFO, LOG_CH( ch_game_crowd ), "crowd visuals: %d live, %d promoted, %d handed back\n",
			GC_UsedCount(), gc_promotedTotal, gc_releasedTotal );
	}

	/* Promote at most one agent per frame, inside the engine's budget and our own cap. */
	if ( GC_UsedCount() < GC_MAX_VISUALS && trap_Crowd_PromotionBudget() > 0 ) {
		crowdAgentRef_t ref;
		if ( trap_Crowd_Promotable( &ref, 1 ) > 0 ) {
			crowdAgentState_t s;
			if ( trap_Crowd_Promote( ref.userId, &s ) ) {
				gcVisual_t *v = GC_Spawn( &s );
				if ( v ) {
					if ( gc_promotedTotal++ == 0 ) {
						Com_Log( SEV_INFO, LOG_CH( ch_game_crowd ), "crowd visuals: first promotion (agent %d, entity %d, model %d)\n", v->userId, (int)( v->ent - g_entities ), v->ent->s.modelindex );
					}
					GC_Publish( v, levelTime );
					if ( gc_promotedTotal == 1 ) {
						Com_Log( SEV_INFO, LOG_CH( ch_game_crowd ), "crowd visuals: entity %d linked=%d at (%.0f %.0f %.0f)\n",
							(int)( v->ent - g_entities ), v->ent->r.linked, v->origin[0], v->origin[1], v->origin[2] );
					}
				} else {
					trap_Crowd_Demote( &s );   /* no room on our side: give it straight back */
				}
			}
		}
	}
}
