// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 1999-2005 Id Software, Inc.
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
//
// cg_monster.c — Q1 monster animation (client side).
//
// A Q1 monster is a single-mesh model whose animation is driven exactly like the
// player's: the SERVER sets an animation CODE (a monsterAnim_t) into
// entityState_t.legsAnim from the behaviorState FSM; the CLIENT resolves the code
// to a frame range and ticks + interpolates it. The frame ranges are DERIVED from
// the model's Q1 .mdl frame names (carried into md3Frame_t.name by the MDL loader)
// via the trap_R_GetMDLAnimations query, then mapped label -> monsterAnim_t code
// through the alias table below. This mirrors the IQM named-animation path
// (CG_ParseIQMAnimations / CG_RunLerpFrame) exactly; the only monster-specific
// pieces are the single animation axis (no torso/legs split) and the label->code
// alias table (Q1's lettered pain/death/attack variants collapse to one code).

#include "cg_local.h"
#include "../renderercommon/tr_model_mdl.h"   // mdlAnimRange_t

LOG_DECLARE_CHANNEL( ch_cgame, "cgame" );

// ── the alias table: Q1 frame-name label -> monster anim code ────────────────
// Derived ranges are keyed by the raw Q1 label (digits stripped: "walk", "painb",
// "atta"). id Quake puts the variant letter BEFORE the digits, so the labels are
// stable; here we collapse the variants (pain/painb/painc -> PAIN, atta/attb/attc
// / swing/smash -> ATTACK, death/deathc/fdeath -> DEATH). First match wins, so a
// more specific label must precede a prefix of it — but Q1 labels don't nest, so
// exact-match is sufficient and safe. This ~1-page table is the ONLY hand-authored
// data; the ranges themselves auto-derive.
static const struct {
	const char		*label;
	monsterAnim_t	anim;
} cg_mdlAnimAlias[] = {
	// idle / stand
	{ "stand",   MANIM_STAND },
	{ "standing",MANIM_STAND },
	{ "kneel",   MANIM_STAND },
	{ "hover",   MANIM_STAND },   // wizard idle
	{ "cruc_",   MANIM_STAND },   // zombie crucified idle
	// walk
	{ "walk",    MANIM_WALK },
	{ "swim",    MANIM_WALK },    // fish
	{ "fly",     MANIM_WALK },    // wizard/scrag cruise
	// prowl_ is deliberately NOT mapped: it is the soldier's crouch/aim/stalk pose
	// (24 stooping frames), not a locomotion cycle. The soldier has no walk cycle at
	// all — it RUNS to approach — so its WALK intent resolves to RUN via the empty-
	// walk fallback in CG_MonsterAnimation. A future scripted "aim/crouch" verb can
	// drive prowl_ explicitly.
	// run
	{ "run",     MANIM_RUN },
	{ "runb",    MANIM_RUN },
	{ "leap",    MANIM_RUN },     // dog/demon lunge approach
	// attack (Q1's many attack variants all collapse to one code for now)
	{ "attack",  MANIM_ATTACK },
	{ "atta",    MANIM_ATTACK },  // zombie throw a
	{ "attb",    MANIM_ATTACK },  // zombie throw b
	{ "attc",    MANIM_ATTACK },  // zombie throw c
	{ "attacka", MANIM_ATTACK },  // demon melee
	{ "attackb", MANIM_ATTACK },  // knight stationary sword swing
	{ "shoot",   MANIM_ATTACK },  // soldier/ogre ranged
	{ "swing",   MANIM_ATTACK },  // ogre/shambler swing
	{ "swingr",  MANIM_ATTACK },  // shambler swing right
	{ "swingl",  MANIM_ATTACK },  // shambler swing left
	{ "smash",   MANIM_ATTACK },  // ogre/shambler overhead
	{ "magic",   MANIM_ATTACK },  // shambler lightning
	{ "magatt",  MANIM_ATTACK },  // wizard magic attack
	{ "runattack",MANIM_ATTACK }, // knight running slash
	{ "pull",    MANIM_ATTACK },  // ogre chainsaw pull-back
	{ "shocka",  MANIM_ATTACK },  // boss (Chthon) shockwave a
	{ "shockb",  MANIM_ATTACK },  // boss (Chthon) shockwave b
	{ "shockc",  MANIM_ATTACK },  // boss (Chthon) shockwave c
	// pain (all lettered variants -> one code)
	{ "pain",    MANIM_PAIN },
	{ "paina",   MANIM_PAIN },
	{ "painb",   MANIM_PAIN },
	{ "painc",   MANIM_PAIN },
	{ "paind",   MANIM_PAIN },
	{ "paine",   MANIM_PAIN },
	// death (all variants -> one code)
	{ "death",   MANIM_DEATH },
	{ "deathb",  MANIM_DEATH },
	{ "deathc",  MANIM_DEATH },
	{ "deathe",  MANIM_DEATH },
	{ "bdeath",  MANIM_DEATH },   // ogre backward death
	{ "fdeath",  MANIM_DEATH },   // enforcer fiery death
};
static const int cg_numMdlAnimAlias = ARRAY_LEN( cg_mdlAnimAlias );

// Map a raw Q1 label to a monster anim code, or -1 if unmapped.
static int CG_MDLLabelToAnim( const char *label ) {
	int i;
	for ( i = 0; i < cg_numMdlAnimAlias; i++ ) {
		if ( !Q_stricmp( label, cg_mdlAnimAlias[i].label ) ) {
			return cg_mdlAnimAlias[i].anim;
		}
	}
	return -1;
}

// ── CG_ParseMDLAnimations — derive a model's monster anim table (clone of the
// IQM path CG_ParseIQMAnimations). Queries the derived ranges, maps each label to
// a monster anim code, fills the animation_t range table. Returns qtrue if at
// least one anim mapped (i.e. this model is a usable monster model). ───────────
static qboolean CG_ParseMDLAnimations( qhandle_t model, monsterAnimSet_t *set ) {
	mdlAnimRange_t	ranges[MAX_MDL_ANIMS];
	int				numRanges, i, mapped = 0;

	numRanges = trap_R_GetMDLAnimations( model, ranges, MAX_MDL_ANIMS );
	if ( numRanges <= 0 ) {
		return qfalse;
	}

	// Sensible defaults (a monster that's missing an anim falls back to STAND, or
	// frame 0 if even that is absent). 10 fps is the Q1-authentic default.
	for ( i = 0; i < MANIM_COUNT; i++ ) {
		set->anims[i].firstFrame  = 0;
		set->anims[i].numFrames   = 1;
		set->anims[i].loopFrames  = 1;
		set->anims[i].frameLerp   = 100;   // 10 fps (Q1)
		set->anims[i].initialLerp = 100;
		set->anims[i].reversed    = qfalse;
		set->anims[i].flipflop    = qfalse;
	}

	for ( i = 0; i < numRanges; i++ ) {
		int anim = CG_MDLLabelToAnim( ranges[i].label );
		if ( anim < 0 ) {
			// A label with no alias entry derives no range. Surfacing it at load
			// (cg_debugAnim) makes a new model's unmapped locomotion/attack visible
			// immediately instead of as a silent frozen-feet slide. Benign labels
			// (reload/pouch/rise) also print — the author decides which to alias.
			if ( cg_debugAnim.integer ) {
				Com_Log( SEV_INFO, LOG_CH(ch_cgame),
					"monster anim: unmapped frame prefix '%s' (%d frames) — no alias\n",
					ranges[i].label, ranges[i].num_frames );
			}
			continue;
		}
		// First mapping wins for a given code (e.g. "pain" beats "painb" — both map to
		// PAIN; keep the first/base variant, in frame order, as the representative range).
		// A code is "already set" once it holds a real (>1-frame) range; STAND starts as
		// the default 1-frame placeholder, so a real range is still allowed to replace it.
		if ( set->anims[anim].numFrames > 1 && anim != MANIM_STAND ) {
			continue;   // earlier variant already claimed this code — keep it
		}
		set->anims[anim].firstFrame = ranges[i].first_frame;
		set->anims[anim].numFrames  = ranges[i].num_frames;
		// Locomotion + idle loop; attack/pain/death play once (death especially).
		if ( anim == MANIM_STAND || anim == MANIM_WALK || anim == MANIM_RUN ) {
			set->anims[anim].loopFrames = ranges[i].num_frames;
		} else {
			set->anims[anim].loopFrames = 0;
		}
		mapped++;
	}

	set->numAnims = mapped;
	return ( mapped > 0 );
}

// Get (deriving on first use) the monster anim table for a model's configstring
// slot. Returns NULL if the model isn't a usable monster model.
static monsterAnimSet_t *CG_MonsterAnimSet( int modelIndex ) {
	monsterAnimSet_t *set;

	if ( modelIndex <= 0 || modelIndex >= MAX_MODELS ) {
		return NULL;
	}
	set = &cgs.monsterAnims[modelIndex];
	if ( !set->derived ) {
		set->derived = qtrue;
		CG_ParseMDLAnimations( cgs.gameModels[modelIndex], set );
	}
	return ( set->numAnims > 0 ) ? set : NULL;
}

// ── CG_RunMonsterLerpFrame — the single-axis tick+lerp (the self-contained core
// of CG_RunLerpFrame, reading the monster's own animation_t table rather than a
// clientInfo). Advances lf->frame/oldFrame and computes backlerp. ─────────────
static void CG_RunMonsterLerpFrame( lerpFrame_t *lf, const animation_t *anim, int newAnimation ) {
	int f, numFrames;
	const int t = cg.time;

	if ( cg_animSpeed.integer == 0 ) {
		lf->oldFrame = lf->frame = lf->backlerp = 0;
		return;
	}

	// switching sequence?
	if ( newAnimation != lf->animationNumber || !lf->animation ) {
		lf->animationNumber = newAnimation;
		lf->animation       = (animation_t *)anim;
		lf->animationTime   = t + anim->initialLerp;
		lf->oldFrame        = lf->frame = anim->firstFrame;
		lf->oldFrameTime    = lf->frameTime = lf->animationTime;
	}

	if ( t >= lf->frameTime ) {
		lf->oldFrame     = lf->frame;
		lf->oldFrameTime = lf->frameTime;

		if ( !anim->frameLerp ) {
			return;
		}
		if ( t < lf->animationTime ) {
			lf->frameTime = lf->animationTime;
		} else {
			lf->frameTime = lf->oldFrameTime + anim->frameLerp;
		}
		f = ( lf->frameTime - lf->animationTime ) / anim->frameLerp;

		numFrames = anim->numFrames;
		if ( f >= numFrames ) {
			f -= numFrames;
			if ( anim->loopFrames ) {
				f %= anim->loopFrames;
				f += anim->numFrames - anim->loopFrames;
			} else {
				f = numFrames - 1;
				lf->frameTime = t;   // stuck at end (attack/pain/death play once)
			}
		}
		lf->frame = anim->firstFrame + f;
		if ( t > lf->frameTime ) {
			lf->frameTime = t;
		}
	}

	// compute backlerp for smooth interpolation
	if ( lf->frameTime == lf->oldFrameTime ) {
		lf->backlerp = 0;
	} else {
		lf->backlerp = 1.0f - (float)( t - lf->oldFrameTime ) / ( lf->frameTime - lf->oldFrameTime );
		if ( lf->backlerp < 0 ) lf->backlerp = 0;
		if ( lf->backlerp > 1 ) lf->backlerp = 1;
	}
}

// ── CG_IsMonsterModel — public predicate: is the model at modelIndex a Q1 monster
// model (one whose .mdl frame names derived at least one monster anim)? The eType
// router uses this to distinguish a monster ET_GENERAL from a plain generic entity.
qboolean CG_IsMonsterModel( int modelIndex ) {
	return CG_MonsterAnimSet( modelIndex ) != NULL;
}

// ── CG_MonsterAnimation — the public entry: drive a monster centity's animation
// from its entityState.legsAnim (the code the server set from behaviorState).
// Fills *frame / *oldFrame / *backLerp for the render. Returns qtrue if the
// monster has a derived anim table (else the caller falls back to s.frame). ────
qboolean CG_MonsterAnimation( centity_t *cent, int *frame, int *oldFrame, float *backLerp ) {
	monsterAnimSet_t *set;
	lerpFrame_t      *lf = &cent->pe.legs;   // single axis reuses pe.legs
	int               code = cent->currentState.legsAnim & ~ANIM_TOGGLEBIT;

	set = CG_MonsterAnimSet( cent->currentState.modelindex );
	if ( !set ) {
		return qfalse;   // not a monster model with derived anims — caller snaps s.frame
	}
	if ( code < 0 || code >= MANIM_COUNT ) {
		code = MANIM_STAND;
	}

	// Locomotion fallback: a monster may carry only ONE movement cycle. Q1 soldiers
	// RUN to approach (they have `run`, no `walk`); some monsters are the reverse.
	// The server's HUNT/ALERT intent is "locomote" (MANIM_WALK); resolve it to
	// whatever movement the model actually has. An unmapped anim derives an empty
	// range (numFrames <= 1, the default), so an empty WALK falls back to RUN and an
	// empty RUN to WALK. Without this the soldier's empty WALK would freeze (or, with
	// the earlier wrong prowl_ alias, stoop) while sliding on server motion.
	if ( code == MANIM_WALK && set->anims[MANIM_WALK].numFrames <= 1
	     && set->anims[MANIM_RUN].numFrames > 1 ) {
		code = MANIM_RUN;
	} else if ( code == MANIM_RUN && set->anims[MANIM_RUN].numFrames <= 1
	            && set->anims[MANIM_WALK].numFrames > 1 ) {
		code = MANIM_WALK;
	}

	CG_RunMonsterLerpFrame( lf, &set->anims[code], code );

	*frame    = lf->frame;
	*oldFrame = lf->oldFrame;
	*backLerp = lf->backlerp;
	return qtrue;
}
