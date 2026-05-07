/*
===========================================================================
cg_characters.c — cgame-side character manifest loading.

CG_LoadCharacter() asks the engine for a serialized characterManifest_t via
trap_GetValue("char:{name}", ...), then registers all models/skins/sounds/
animations into the supplied clientInfo_t.
===========================================================================
*/

#include "cg_local.h"
#include "cg_public.h"

// ── Warn-once dedup (replaces per-clientInfo_t soundMissingWarnedMask) ──────
// djb2-based hash set. Collisions silently suppress a duplicate; acceptable.
#define MISS_WARN_MAX 512
static unsigned int s_missWarnHashes[MISS_WARN_MAX];
static int          s_missWarnCount;

static qboolean MissWarnOnce( const char *a, const char *b ) {
	unsigned int h = 5381;
	const char  *p;
	int          i;
	for ( p = a; *p; p++ ) h = (h << 5) + h + (unsigned char)*p;
	h = (h << 5) + h; /* separator */
	for ( p = b; *p; p++ ) h = (h << 5) + h + (unsigned char)*p;
	for ( i = 0; i < s_missWarnCount; i++ )
		if ( s_missWarnHashes[i] == h ) return qfalse;
	if ( s_missWarnCount < MISS_WARN_MAX )
		s_missWarnHashes[s_missWarnCount++] = h;
	return qtrue;
}

// Sound slot names — order matches CSOUND_* defines in cg_public.h
static const char *s_soundSlotName[CM_SOUND_SLOTS] = {
	"death1", "death2", "death3", "jump1",
	"pain25", "pain50", "pain75", "pain100",
	"falling1", "gasp1", "drown1", "fall1", "taunt1",
};

// ── CG_LoadCharacter ──────────────────────────────────────────────────────

/*
====================
CG_LoadCharacter

Attempts to load the character named charName (directory name, e.g. "visor").
Fills *ci with registered models, skins, sounds, animations, and metadata.
Returns qtrue on success; caller must fall back to legacy CG_RegisterClientModelname
on failure.
====================
*/
qboolean CG_LoadCharacter( clientInfo_t *ci, const char *charName ) {
	characterManifest_t mf;
	char key[64];
	int i;

	memset( &mf, 0, sizeof( mf ) );
	Com_sprintf( key, sizeof( key ), "char:%s", charName );
	if ( !trap_GetValue( (char *)&mf, sizeof( mf ), key ) ) {
		return qfalse;
	}

	// ── Model parts ──────────────────────────────────────────────────────────
	// Each part entry in mf.partPaths is a path prefix without extension.
	// Try {path}.iqm first (IQM single-mesh), then fall back to {path}.md3.
	for ( i = 0; i < mf.partCount; i++ ) {
		const char *pname = mf.partNames[i];
		const char *prefix = mf.partPaths[i];
		qhandle_t handle = 0;
		qboolean  isIQM  = qfalse;

		// Try IQM first.
		char tryPath[MAX_VFS_PATH];
		Com_sprintf( tryPath, sizeof(tryPath), "%s.iqm", prefix );
		handle = trap_R_RegisterModel( tryPath );
		if ( handle ) isIQM = qtrue;

		if ( !handle ) {
			Com_sprintf( tryPath, sizeof(tryPath), "%s.md3", prefix );
			handle = trap_R_RegisterModel( tryPath );
		}

		if ( !handle ) {
			Com_Log( SEV_INFO, LOG_CAT_CGAME, "^3CG_LoadCharacter: '%s' part '%s' not found (tried .iqm/.md3)\n",
				charName, pname );
			// Non-fatal for non-body parts; fatal for the primary mesh.
			if ( !Q_stricmp( pname, "lower" ) || !Q_stricmp( pname, "legs" ) ||
				 !Q_stricmp( pname, "body"  ) ) {
				return qfalse;
			}
			continue;
		}

		// Map part name → clientInfo_t slot(s).
		if ( !Q_stricmp( pname, "head" ) ) {
			ci->headModel = handle;
		} else if ( !Q_stricmp( pname, "upper" ) || !Q_stricmp( pname, "torso" ) ) {
			ci->torsoModel = handle;
		} else if ( !Q_stricmp( pname, "lower" ) || !Q_stricmp( pname, "legs" ) ) {
			ci->legsModel = handle;
		} else if ( !Q_stricmp( pname, "body" ) ) {
			// Shared body mesh: assign to both torso and legs slots.
			ci->torsoModel = handle;
			ci->legsModel  = handle;
#if FEAT_IQM
			if ( isIQM ) {
				ci->bodyModel = handle;
				ci->iqmModel  = qtrue;
			}
#endif
		} else {
			Com_Log( SEV_INFO, LOG_CAT_CGAME, "^3CG_LoadCharacter: '%s' unknown part name '%s' (ignored)\n",
				charName, pname );
		}
	}

	// Require at least a legs/torso model.
	if ( !ci->legsModel && !ci->torsoModel ) {
		Com_Log( SEV_INFO, LOG_CAT_CGAME, "^3CG_LoadCharacter: '%s' no usable body model found\n", charName );
		return qfalse;
	}
	// Fall back: if only one of torso/legs was loaded, share the handle.
	if ( ci->legsModel  && !ci->torsoModel ) ci->torsoModel = ci->legsModel;
	if ( ci->torsoModel && !ci->legsModel  ) ci->legsModel  = ci->torsoModel;
	// Head falls back to torso if absent.
	if ( !ci->headModel ) ci->headModel = ci->torsoModel;

	ci->modelIcon = trap_R_RegisterShaderNoMip( mf.iconPath );

	// ── Skin ────────────────────────────────────────────────────────────────
	// Select the skin whose name matches ci->skinName; fall back to index 0.
	// In team gamemodes, prefer a paintable skin.
	if ( mf.numSkins > 0 ) {
		int chosen = 0;

		if ( cgs.gametypeIsTeamGame && ci->team != TEAM_FREE ) {
			// Find if the requested skin is paintable; also find first paintable.
			int firstPaintable = -1;
			qboolean requestedIsPaintable = qfalse;

			for ( i = 0; i < mf.numSkins; i++ ) {
				if ( mf.skins[i].paintable ) {
					if ( firstPaintable < 0 ) firstPaintable = i;
					if ( !Q_stricmp( mf.skins[i].name, ci->skinName ) ) {
						requestedIsPaintable = qtrue;
						chosen = i;
					}
				}
			}

			if ( firstPaintable >= 0 ) {
				if ( !requestedIsPaintable ) {
					// Override with the first paintable skin.
					chosen = firstPaintable;
					Q_strncpyz( ci->skinName, mf.skins[firstPaintable].name, sizeof( ci->skinName ) );
				}
			} else {
				// No paintable skins — warn once per character.
				static char warnedChars[CM_MAX_SKINS][64];
				static int  warnedCount;
				qboolean already = qfalse;
				int w;
				for ( w = 0; w < warnedCount; w++ ) {
					if ( !Q_stricmp( warnedChars[w], charName ) ) { already = qtrue; break; }
				}
				if ( !already && warnedCount < CM_MAX_SKINS ) {
					Com_Log( SEV_INFO, LOG_CAT_CGAME, "^3Warning: character '%s' has no paintable skin; using non-tinted default in team play\n", charName );
					Q_strncpyz( warnedChars[warnedCount++], charName, sizeof( warnedChars[0] ) );
				}
				// Fall through to normal skin selection (chosen stays 0).
				for ( i = 0; i < mf.numSkins; i++ ) {
					if ( !Q_stricmp( mf.skins[i].name, ci->skinName ) ) {
						chosen = i; break;
					}
				}
			}
		} else {
			for ( i = 0; i < mf.numSkins; i++ ) {
				if ( !Q_stricmp( mf.skins[i].name, ci->skinName ) ) {
					chosen = i; break;
				}
			}
		}
		const cmManifestSkin_t *sk = &mf.skins[chosen];
		ci->skinHandle = sk->skinHandle;
#if FEAT_IQM
		if ( ci->iqmModel && !sk->paintable && sk->skinHandle == 0 ) {
			/* IQM body with no skin handle: keep bodyShader/bodySkin unset — MD3 fallbacks apply. */
		}
#endif
	}
	// If no skins defined, MD3-embedded default textures apply — no error.

	// ── Head offset ─────────────────────────────────────────────────────────
	for ( i = 0; i < 3; i++ ) {
		ci->headOffset[i] = mf.headOffset[i];
	}

	// ── Animations ──────────────────────────────────────────────────────────
	memcpy( ci->animations, mf.animations, sizeof( ci->animations ) );

	ci->newAnims = qfalse;
	if ( ci->torsoModel ) {
		orientation_t tag;
		if ( trap_R_LerpTag( &tag, ci->torsoModel, 0, 0, 1, "tag_flag" ) ) {
			ci->newAnims = qtrue;
		}
	}

	// ── Sounds ──────────────────────────────────────────────────────────────
	// Fallback chain: character → archetype → _base (hard fail + warning if all miss)
	const char *archetype = mf.archetypeName;   // guaranteed non-empty: validator enforces this
	for ( i = 0; i < CM_SOUND_SLOTS && i < MAX_CUSTOM_SOUNDS; i++ ) {
		ci->sounds[i] = 0;
		if ( mf.soundPaths[i][0] ) {
			ci->sounds[i] = trap_S_RegisterSound( mf.soundPaths[i], qfalse );
		}
		if ( !ci->sounds[i] ) {
			// Archetype fallback
			ci->sounds[i] = trap_S_RegisterSound(
				va( "characters/_archetypes/%s/sounds/%s.opus", archetype, s_soundSlotName[i] ),
				qfalse );
		}
		if ( !ci->sounds[i] ) {
			// _base fallback  [Fix A]
			ci->sounds[i] = trap_S_RegisterSound(
				va( "characters/_archetypes/_base/sounds/%s.opus", s_soundSlotName[i] ), qfalse );
		}
		// Warn once per (charName, slot) if all three tiers failed
		if ( !ci->sounds[i] && MissWarnOnce( charName, s_soundSlotName[i] ) ) {
			Com_Log( SEV_INFO, LOG_CAT_CGAME, "^3characters/{%s,_archetypes/%s,_archetypes/_base}/sounds/%s.opus not found, using default\n",
				charName, archetype, s_soundSlotName[i] );
		}
	}

	// ── Effects sounds ────────────────────────────────────────────────────
	// Convention: characters/{name}/sounds/{slot}.opus, archetype fallback.
	static const struct { const char *slot; size_t offset; } s_effectSlots[] = {
		{ "gib_split1", offsetof( clientInfo_t, effects.gibSound )    },
		{ "land1",      offsetof( clientInfo_t, effects.landSound )   },
		{ "water_in1",  offsetof( clientInfo_t, effects.watrInSound ) },
		{ "water_out1", offsetof( clientInfo_t, effects.watrOutSound) },
		{ "water_un1",  offsetof( clientInfo_t, effects.watrUnSound ) },
	};
	for ( i = 0; i < (int)(sizeof(s_effectSlots)/sizeof(s_effectSlots[0])); i++ ) {
		sfxHandle_t *dest = (sfxHandle_t *)( (char *)ci + s_effectSlots[i].offset );
		*dest = trap_S_RegisterSound(
			va( "characters/%s/sounds/%s.opus", charName, s_effectSlots[i].slot ), qfalse );
		if ( !*dest ) {
			*dest = trap_S_RegisterSound(
				va( "characters/_archetypes/%s/sounds/%s.opus", archetype, s_effectSlots[i].slot ),
				qfalse );
		}
		// Fix B: _base tier
		if ( !*dest ) {
			*dest = trap_S_RegisterSound(
				va( "characters/_archetypes/_base/sounds/%s.opus", s_effectSlots[i].slot ), qfalse );
		}
		// Warn once per (charName, slot) if all three tiers failed
		if ( !*dest && MissWarnOnce( charName, s_effectSlots[i].slot ) ) {
			Com_Log( SEV_INFO, LOG_CAT_CGAME, "^3characters/{%s,_archetypes/%s,_archetypes/_base}/sounds/%s.opus not found, using default\n",
				charName, archetype, s_effectSlots[i].slot );
		}
	}

	// ── Footstep sounds ───────────────────────────────────────────────────
	// Convention: characters/_archetypes/{archetype}/footsteps/{type}{1,2,3,4}.opus
	// Character override: characters/{name}/footsteps/{type}{1,2,3,4}.opus
	static const char *s_footstepTypes[] = {
		"normal", "metal", "splash"
	};
	for ( int ft = 0; ft < FOOTSTEP_TOTAL; ft++ ) {
		for ( int v = 0; v < 4; v++ ) {
			// Character-level override first
			sfxHandle_t h = trap_S_RegisterSound(
				va( "characters/%s/sounds/footsteps/%s%d.opus", charName, s_footstepTypes[ft], v + 1 ),
				qfalse );
			if ( !h ) {
				h = trap_S_RegisterSound(
					va( "characters/_archetypes/%s/sounds/footsteps/%s%d.opus",
						archetype, s_footstepTypes[ft], v + 1 ), qfalse );
			}
			// Fix C: _base tier
			if ( !h ) {
				h = trap_S_RegisterSound(
					va( "characters/_archetypes/_base/sounds/footsteps/%s%d.opus",
						s_footstepTypes[ft], v + 1 ), qfalse );
			}
			// Warn once per (charName, footstepType) if all three tiers failed
			if ( !h && v == 0 && MissWarnOnce( charName, s_footstepTypes[ft] ) ) {
				Com_Log( SEV_INFO, LOG_CAT_CGAME, "^3characters/{%s,_archetypes/%s,_archetypes/_base}/sounds/footsteps/%s{1,2,3,4}.opus not found, using default\n",
					charName, archetype, s_footstepTypes[ft] );
			}
			ci->footstepSounds[ft][v] = h;
		}
	}

	return qtrue;
}
