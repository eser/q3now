// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
//
// cg_creature.c — render a behavior monster as a character.
//
// A behavior monster is an ET_GENERAL entity (no clientNum), so it cannot ride the
// player render path (CG_Player is hard-coupled to cgs.clientinfo[clientNum] — it
// terminates on an out-of-range clientNum). Instead a creature borrows the character
// DATA — models/skins loaded into a clientInfo_t by CG_LoadCharacter — and renders it
// through the shared single-mesh core CG_CharacterMesh (the same body draw the player
// IQM path uses), fed from a small per-character cache that does NOT occupy a player
// clientinfo[] slot.
//
// The server names the character by writing "characters/<name>/tag" into the entity's
// modelindex2 (a spare CS_MODELS slot); the primary modelindex still points at the Q1
// .mdl so a creature with no loadable character falls back to the CG_General .mdl
// render. This file resolves that name, caches the character clientInfo_t, and draws
// the body. Land-1 renders a static frame (frame 0); creature animation is a follow-up.

#include "cg_local.h"

LOG_DECLARE_CHANNEL( ch_cgame, "cgame" );

// The creature render-identity sentinel: the server sets a behavior monster's
// modelindex2 to this path so the client can derive the character slug. It is NOT a
// real model file — cg_servercmds skips registering it (see CG_IsCreatureTagModel).
// The tag helpers are format-only (no IQM dependency) so they compile regardless of
// FEAT_IQM — the server writes modelindex2 whenever a behavior monster spawns, so the
// registration-skip must always be available even in a build with no creature render.
#define CREATURE_TAG_PREFIX "characters/"
#define CREATURE_TAG_SUFFIX "/tag"

// ── CG_ParseCreatureTag — extract the character slug from a "characters/<name>/tag"
// sentinel string. Returns qtrue + fills name[] on a well-formed tag, qfalse otherwise.
static qboolean CG_ParseCreatureTag( const char *path, char *name, int nameSize ) {
	const char *slug, *slash;
	int         n;
	size_t      len;

	if ( !path || strncmp( path, CREATURE_TAG_PREFIX, strlen( CREATURE_TAG_PREFIX ) ) != 0 ) {
		return qfalse;
	}
	slug  = path + strlen( CREATURE_TAG_PREFIX );
	slash = strchr( slug, '/' );
	if ( !slash ) {
		return qfalse;                       // must have the "/tag" segment
	}
	// the segment after the slug must be exactly "/tag" (the sentinel suffix)
	if ( strcmp( slash, CREATURE_TAG_SUFFIX ) != 0 ) {
		return qfalse;
	}
	n = (int)( slash - slug );
	len = (size_t)n;
	if ( n <= 0 || len >= (size_t)nameSize ) {
		return qfalse;
	}
	memcpy( name, slug, len );
	name[n] = '\0';
	return qtrue;
}

// ── CG_IsCreatureTagModel — is this CS_MODELS string the creature render-identity
// sentinel (not a real model to register)? Used by cg_servercmds to skip registration.
qboolean CG_IsCreatureTagModel( const char *str ) {
	char scratch[MAX_QPATH];
	return CG_ParseCreatureTag( str, scratch, sizeof( scratch ) );
}

#if FEAT_IQM

// ── CG_CreatureCharName — resolve the character slug a creature entity should render
// as, from its modelindex2 CS_MODELS path ("characters/<name>/tag"). Returns qtrue and
// fills name[] if the entity carries a character slug, qfalse otherwise (a plain
// monster with no character → caller renders via CG_General). ────────────────────────
static qboolean CG_CreatureCharName( const centity_t *cent, char *name, int nameSize ) {
	int         idx = cent->currentState.modelindex2;

	if ( idx <= 0 ) {
		return qfalse;
	}
	return CG_ParseCreatureTag( CG_ConfigString( CS_MODELS + idx ), name, nameSize );
}

// ── CG_CreatureInfo — get (loading on first use) the cached character clientInfo_t for
// a creature slug. Returns NULL if the character has no loadable body (caller then
// falls back to CG_General so the .mdl still shows). One entry per distinct character;
// the cache lives in cgs and is zeroed on map load by CG_Init's memset. ───────────────
static clientInfo_t *CG_CreatureInfo( const char *charName ) {
	int           i, slot;
	clientInfo_t *ci;

	// hit?
	for ( i = 0; i < cgs.numCreatureChars; i++ ) {
		if ( !Q_stricmp( cgs.creatureCharName[i], charName ) ) {
			ci = &cgs.creatureInfo[i];
			return ci->infoValid ? ci : NULL;   // load-failed slot stays cached as invalid
		}
	}

	// miss — claim a slot (record the name either way so a failed load isn't retried
	// every frame; a full cache just renders via fallback).
	if ( cgs.numCreatureChars >= MAX_CREATURE_CHARS ) {
		return NULL;
	}
	slot = cgs.numCreatureChars++;
	Q_strncpyz( cgs.creatureCharName[slot], charName, MAX_QPATH );

	ci = &cgs.creatureInfo[slot];
	memset( ci, 0, sizeof( *ci ) );
	// Neutral creature: TEAM_FREE avoids the team paintable-skin override in
	// CG_LoadCharacter and respects skinName. CG_LoadCharacter does not touch infoValid,
	// so we set it ourselves on success (it is also our cache "slot loaded" flag).
	ci->team = TEAM_FREE;
	Q_strncpyz( ci->skinName, "default", sizeof( ci->skinName ) );

	if ( CG_LoadCharacter( ci, charName ) ) {
		ci->infoValid = qtrue;
		return ci;
	}
	// Load failed (e.g. no body mesh) — leave infoValid=qfalse; caller falls back.
	return NULL;
}

// ── CG_CreatureBody — the single resolver used by BOTH the router predicate and the
// draw: return the cached clientInfo_t this creature should render as, or NULL if it has
// no single-mesh body to draw (so the router routes it to CG_General for the .mdl
// fallback). The predicate MUST match the draw condition exactly — a character that
// loads only 3-part MD3 handles (no single-mesh bodyModel) is NOT drawable by this path,
// so it returns NULL and falls back rather than rendering invisibly. The gate is
// bodyModel (set for an .iqm OR an MD3-shaped .mdl single-mesh body), matching the
// player single-mesh gate in CG_Player. ──────────────────────────────────────────────
static clientInfo_t *CG_CreatureBody( centity_t *cent ) {
	char          name[MAX_QPATH];
	clientInfo_t *ci;

	if ( !CG_CreatureCharName( cent, name, sizeof( name ) ) ) {
		return NULL;
	}
	ci = CG_CreatureInfo( name );
	if ( !ci || !ci->bodyModel ) {
		return NULL;
	}
	return ci;
}

// ── CG_CreatureRenders — router go/no-go: qtrue only when this entity resolves to a
// drawable single-mesh character body (else route to CG_General for the .mdl fallback).
qboolean CG_CreatureRenders( centity_t *cent ) {
	return CG_CreatureBody( cent ) != NULL;
}

// ── CG_Creature — render a behavior monster as its character. Draws the character body
// mesh (from the cached clientInfo_t) through the shared single-mesh core, animated by
// the monster's derived .mdl anim ranges (CG_MonsterAnimation resolves the server's
// MANIM code in s.legsAnim to a frame + backlerp). No weapon, no powerups (player-only).
// No-op if the entity has no drawable body — but the router only calls us when
// CG_CreatureRenders() already confirmed one, so that guard is belt-and-braces. ───
void CG_Creature( centity_t *cent ) {
	clientInfo_t *ci = CG_CreatureBody( cent );
	refEntity_t   body;
	vec3_t        axis[3];
	int           frame, oldframe;
	float         backlerp;

	if ( !ci ) {
		return;
	}

	// Drive the single mesh from the monster's derived .mdl animation table (the same
	// source CG_General uses for the .mdl fallback). Falls back to a static frame 0 if
	// the model has no derived ranges (a non-monster single-mesh character).
	//
	// The frame indices are derived from s.modelindex (the .mdl the server spawned),
	// while the mesh drawn is ci->bodyModel (the character's body). These are the SAME
	// underlying .mdl for the soldier — its body part re-homes the exact soldier.mdl —
	// so the frame space matches. A future character whose body .mdl has a DIFFERENT
	// frame layout than s.modelindex would need its anim derived from ci->bodyModel
	// instead; that is a per-body anim-table keyed by the render handle, deferred until a
	// creature with a distinct body model exists.
	if ( !CG_MonsterAnimation( cent, &frame, &oldframe, &backlerp ) ) {
		frame = oldframe = 0;
		backlerp = 0.0f;
	}

	// full-body orientation from the entity's lerped angles
	AnglesToAxis( cent->lerpAngles, axis );

	CG_CharacterMesh( cent, &body, ci->bodyModel,
		ci->bodyShader, ci->bodyShader ? 0 : ci->bodySkin,
		frame, oldframe, backlerp, axis,
		0 /*renderfx*/, 0 /*alpha*/, TEAM_FREE );
}

#endif // FEAT_IQM
