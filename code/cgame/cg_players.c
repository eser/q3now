/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.
Copyright (C) 2024 Wired engine contributors

This file is part of the Wired Engine (derived from idTech 3 & 4 source
code and community around it). It is free software released under the terms
of the GNU General Public License version 2 or (at your option) any later
version.

Quake III Arena, q3now, Wired Engine and the rest are licensed under the
**GNU General Public License, version 2 or later (GPL-2.0-or-later)**.
The full license text is in `LICENSE` and `THIRD_PARTY_LICENSES.md` at the
repository root.
===========================================================================
*/
//
// cg_players.c -- handle the media and animation for player entities
#include "cg_local.h"
/* Phase 5: log channels */
LOG_DECLARE_CHANNEL( ch_cgame, "cgame" );

// Canonical sound slot names — order matches CSOUND_* defines in cg_public.h
static const char *cg_soundSlotNames[CM_SOUND_SLOTS] = {
	"death1", "death2", "death3", "jump1",
	"pain25", "pain50", "pain75", "pain100",
	"falling1", "gasp1", "drown1", "fall1", "taunt1",
};


/*
=============================================================================

CLIENT INFO

=============================================================================
*/

#if FEAT_IQM
/*
======================
IQM embedded animation name-to-enum mapping table.
Content creators name their IQM animations with these strings.
======================
*/
static const struct {
	const char	*name;
	int		anim;
} iqmAnimNames[] = {
	{ "BOTH_DEATH1",		BOTH_DEATH1 },
	{ "BOTH_DEAD1",		BOTH_DEAD1 },
	{ "BOTH_DEATH2",		BOTH_DEATH2 },
	{ "BOTH_DEAD2",		BOTH_DEAD2 },
	{ "BOTH_DEATH3",		BOTH_DEATH3 },
	{ "BOTH_DEAD3",		BOTH_DEAD3 },
	{ "TORSO_GESTURE",		TORSO_GESTURE },
	{ "TORSO_ATTACK",		TORSO_ATTACK },
	{ "TORSO_ATTACK2",		TORSO_ATTACK2 },
	{ "TORSO_DROP",			TORSO_DROP },
	{ "TORSO_RAISE",		TORSO_RAISE },
	{ "TORSO_STAND",		TORSO_STAND },
	{ "TORSO_STAND2",		TORSO_STAND2 },
	{ "LEGS_WALKCR",		LEGS_WALKCR },
	{ "LEGS_WALK",			LEGS_WALK },
	{ "LEGS_RUN",			LEGS_RUN },
	{ "LEGS_BACK",			LEGS_BACK },
	{ "LEGS_SWIM",			LEGS_SWIM },
	{ "LEGS_JUMP",			LEGS_JUMP },
	{ "LEGS_LAND",			LEGS_LAND },
	{ "LEGS_JUMPB",			LEGS_JUMPB },
	{ "LEGS_LANDB",			LEGS_LANDB },
	{ "LEGS_IDLE",			LEGS_IDLE },
	{ "LEGS_IDLECR",		LEGS_IDLECR },
	{ "LEGS_TURN",			LEGS_TURN },
	{ "TORSO_GETFLAG",		TORSO_GETFLAG },
	{ "TORSO_GUARDBASE",		TORSO_GUARDBASE },
	{ "TORSO_PATROL",		TORSO_PATROL },
	{ "TORSO_FOLLOWME",		TORSO_FOLLOWME },
	{ "TORSO_AFFIRMATIVE",		TORSO_AFFIRMATIVE },
	{ "TORSO_NEGATIVE",		TORSO_NEGATIVE },
	{ "LEGS_BACKCR",		LEGS_BACKCR },
	{ "LEGS_BACKWALK",		LEGS_BACKWALK },
	{ "FLAG_RUN",			FLAG_RUN },
	{ "FLAG_STAND",			FLAG_STAND },
	{ "FLAG_STAND2RUN",		FLAG_STAND2RUN },
};
static const int numIqmAnimNames = ARRAY_LEN( iqmAnimNames );


/*
======================
CG_ParseIQMAnimations

Map embedded IQM animation names to Q3 animation enums.
Returns qtrue if at least one animation was successfully mapped.
======================
*/
static qboolean CG_ParseIQMAnimations( const char *modelName, clientInfo_t *ci ) {
	iqmAnimInfo_t	iqmAnims[MAX_IQM_ANIMS];
	int		numAnims;
	int		i, j;
	animation_t	*animations;
	qboolean	mapped[MAX_TOTALANIMATIONS];
	int		mappedCount;

	numAnims = trap_R_GetIQMAnimations( ci->bodyModel, iqmAnims, MAX_IQM_ANIMS );
	if ( numAnims <= 0 ) {
		return qfalse;
	}

	animations = ci->animations;
	memset( mapped, 0, sizeof( mapped ) );
	mappedCount = 0;

	// set sensible defaults for all animations
	VectorClear( ci->headOffset );
	ci->gender = GENDER_NEUTER;
	ci->fixedlegs = qfalse;
	ci->fixedtorso = qfalse;

	for ( i = 0; i < numAnims; i++ ) {
		qboolean found = qfalse;
		for ( j = 0; j < numIqmAnimNames; j++ ) {
			if ( !Q_stricmp( iqmAnims[i].name, iqmAnimNames[j].name ) ) {
				int animIdx = iqmAnimNames[j].anim;
				float fps;

				animations[animIdx].firstFrame = iqmAnims[i].first_frame;
				animations[animIdx].numFrames = iqmAnims[i].num_frames;
				animations[animIdx].loopFrames = ( iqmAnims[i].flags & 1 ) ? iqmAnims[i].num_frames : 0;  // IQM_LOOP = 1<<0
				fps = iqmAnims[i].framerate;
				if ( fps == 0 ) {
					fps = 1;
				}
				animations[animIdx].frameLerp = (int)( 1000.0f / fps );
				animations[animIdx].initialLerp = (int)( 1000.0f / fps );
				animations[animIdx].reversed = qfalse;
				animations[animIdx].flipflop = qfalse;

				mapped[animIdx] = qtrue;
				mappedCount++;
				found = qtrue;
				break;
			}
		}
		if ( !found ) {
			Com_Log( SEV_INFO, LOG_CH(ch_cgame), "IQM %s: unmapped animation '%s'\n", modelName, iqmAnims[i].name );
		}
	}

	if ( mappedCount == 0 ) {
		return qfalse;
	}

	// warn about unmapped Q3 animation enums (developer-level)
	for ( i = 0; i < MAX_ANIMATIONS; i++ ) {
		if ( !mapped[i] ) {
			Com_Log( SEV_INFO, LOG_CH(ch_cgame), "IQM %s: missing Q3 animation enum %d\n", modelName, i );
		}
	}

	// synthesize backward animations from forward ones if not explicitly mapped
	if ( !mapped[LEGS_BACKCR] && mapped[LEGS_WALKCR] ) {
		memcpy( &animations[LEGS_BACKCR], &animations[LEGS_WALKCR], sizeof(animation_t) );
		animations[LEGS_BACKCR].reversed = qtrue;
	}
	if ( !mapped[LEGS_BACKWALK] && mapped[LEGS_WALK] ) {
		memcpy( &animations[LEGS_BACKWALK], &animations[LEGS_WALK], sizeof(animation_t) );
		animations[LEGS_BACKWALK].reversed = qtrue;
	}

	// set default flag animations if not mapped
	if ( !mapped[FLAG_RUN] ) {
		animations[FLAG_RUN].firstFrame = 0;
		animations[FLAG_RUN].numFrames = 16;
		animations[FLAG_RUN].loopFrames = 16;
		animations[FLAG_RUN].frameLerp = 1000 / 15;
		animations[FLAG_RUN].initialLerp = 1000 / 15;
		animations[FLAG_RUN].reversed = qfalse;
	}
	if ( !mapped[FLAG_STAND] ) {
		animations[FLAG_STAND].firstFrame = 16;
		animations[FLAG_STAND].numFrames = 5;
		animations[FLAG_STAND].loopFrames = 0;
		animations[FLAG_STAND].frameLerp = 1000 / 20;
		animations[FLAG_STAND].initialLerp = 1000 / 20;
		animations[FLAG_STAND].reversed = qfalse;
	}
	if ( !mapped[FLAG_STAND2RUN] ) {
		animations[FLAG_STAND2RUN].firstFrame = 16;
		animations[FLAG_STAND2RUN].numFrames = 5;
		animations[FLAG_STAND2RUN].loopFrames = 1;
		animations[FLAG_STAND2RUN].frameLerp = 1000 / 15;
		animations[FLAG_STAND2RUN].initialLerp = 1000 / 15;
		animations[FLAG_STAND2RUN].reversed = qtrue;
	}

	Com_Log( SEV_INFO, LOG_CH(ch_cgame), "IQM %s: mapped %d/%d animations from embedded data\n",
		modelName, mappedCount, numAnims );

	return qtrue;
}
#endif // FEAT_IQM


/*
======================
CG_ParseAnimationFile

Read a configuration file containing animation counts and rates
characters/visor/models/animation.cfg, etc
======================
*/
static qboolean	CG_ParseAnimationFile( const char *filename, clientInfo_t *ci ) {
	const char	*text_p, *prev;
	int			len;
	int			i;
	const char	*token;
	float		fps;
	int			skip;
	char		text[20000];
	fileHandle_t	f;
	animation_t *animations;
	ComParser   parser = { 0 };

	animations = ci->animations;

	// load the file
	len = trap_FS_FOpenFile( filename, &f, FS_READ );
	if ( len <= 0 ) {
		return qfalse;
	}
	if ( len >= sizeof( text ) - 1 ) {
		Com_Log( SEV_INFO, LOG_CH(ch_cgame), "File %s too long\n", filename );
		trap_FS_FCloseFile( f );
		return qfalse;
	}
	trap_FS_Read( text, len, f );
	text[len] = 0;
	trap_FS_FCloseFile( f );

	// parse the text
	text_p = text;
	skip = 0;	// quite the compiler warning

	VectorClear( ci->headOffset );
	ci->gender = GENDER_NEUTER;
	ci->fixedlegs = qfalse;
	ci->fixedtorso = qfalse;

	// read optional parameters
	while ( 1 ) {
		prev = text_p;	// so we can unget
		token = COM_Parse( &parser, &text_p );
		if ( !token[0] ) {
			break;
		}
		if ( !Q_stricmp( token, "headoffset" ) ) {
			for ( i = 0 ; i < 3 ; i++ ) {
				token = COM_Parse( &parser, &text_p );
				if ( !token[0] ) {
					break;
				}
				ci->headOffset[i] = atof( token );
			}
			continue;
		}
		if ( !Q_stricmp( token, "fixedlegs" ) ) {
			ci->fixedlegs = qtrue;
			continue;
		}
		if ( !Q_stricmp( token, "fixedtorso" ) ) {
			ci->fixedtorso = qtrue;
			continue;
		}
		if ( !Q_stricmp( token, "sex" ) ) {
			token = COM_Parse( &parser, &text_p );
			if ( !token[0] ) {
				break;
			}
			if ( token[0] == 'f' || token[0] == 'F' ) {
				ci->gender = GENDER_FEMALE;
			} else if ( token[0] == 'n' || token[0] == 'N' ) {
				ci->gender = GENDER_NEUTER;
			} else {
				ci->gender = GENDER_MALE;
			}
			continue;
		}

		// if it is a number, start parsing animations
		if ( token[0] >= '0' && token[0] <= '9' ) {
			text_p = prev;	// unget the token
			break;
		}
		Com_Log( SEV_INFO, LOG_CH(ch_cgame), "unknown token '%s' in %s\n", token, filename );
	}

	// read information for each frame
	for ( i = 0 ; i < MAX_ANIMATIONS ; i++ ) {

		token = COM_Parse( &parser, &text_p );
		if ( !token[0] ) {
			if( i >= TORSO_GETFLAG && i <= TORSO_NEGATIVE ) {
				animations[i].firstFrame = animations[TORSO_GESTURE].firstFrame;
				animations[i].frameLerp = animations[TORSO_GESTURE].frameLerp;
				animations[i].initialLerp = animations[TORSO_GESTURE].initialLerp;
				animations[i].loopFrames = animations[TORSO_GESTURE].loopFrames;
				animations[i].numFrames = animations[TORSO_GESTURE].numFrames;
				animations[i].reversed = qfalse;
				animations[i].flipflop = qfalse;
				continue;
			}
			break;
		}
		animations[i].firstFrame = atoi( token );
		// leg only frames are adjusted to not count the upper body only frames
		if ( i == LEGS_WALKCR ) {
			skip = animations[LEGS_WALKCR].firstFrame - animations[TORSO_GESTURE].firstFrame;
		}
		if ( i >= LEGS_WALKCR && i<TORSO_GETFLAG) {
			animations[i].firstFrame -= skip;
		}

		token = COM_Parse( &parser, &text_p );
		if ( !token[0] ) {
			break;
		}
		animations[i].numFrames = atoi( token );

		animations[i].reversed = qfalse;
		animations[i].flipflop = qfalse;
		// if numFrames is negative the animation is reversed
		if (animations[i].numFrames < 0) {
			animations[i].numFrames = -animations[i].numFrames;
			animations[i].reversed = qtrue;
		}

		token = COM_Parse( &parser, &text_p );
		if ( !token[0] ) {
			break;
		}
		animations[i].loopFrames = atoi( token );

		token = COM_Parse( &parser, &text_p );
		if ( !token[0] ) {
			break;
		}
		fps = atof( token );
		if ( fps == 0 ) {
			fps = 1;
		}
		animations[i].frameLerp = 1000 / fps;
		animations[i].initialLerp = 1000 / fps;
	}

	if ( i != MAX_ANIMATIONS ) {
		Com_Log( SEV_INFO, LOG_CH(ch_cgame), "Error parsing animation file: %s\n", filename );
		return qfalse;
	}

	// crouch backward animation
	memcpy(&animations[LEGS_BACKCR], &animations[LEGS_WALKCR], sizeof(animation_t));
	animations[LEGS_BACKCR].reversed = qtrue;
	// walk backward animation
	memcpy(&animations[LEGS_BACKWALK], &animations[LEGS_WALK], sizeof(animation_t));
	animations[LEGS_BACKWALK].reversed = qtrue;
	// flag moving fast
	animations[FLAG_RUN].firstFrame = 0;
	animations[FLAG_RUN].numFrames = 16;
	animations[FLAG_RUN].loopFrames = 16;
	animations[FLAG_RUN].frameLerp = 1000 / 15;
	animations[FLAG_RUN].initialLerp = 1000 / 15;
	animations[FLAG_RUN].reversed = qfalse;
	// flag not moving or moving slowly
	animations[FLAG_STAND].firstFrame = 16;
	animations[FLAG_STAND].numFrames = 5;
	animations[FLAG_STAND].loopFrames = 0;
	animations[FLAG_STAND].frameLerp = 1000 / 20;
	animations[FLAG_STAND].initialLerp = 1000 / 20;
	animations[FLAG_STAND].reversed = qfalse;
	// flag speeding up
	animations[FLAG_STAND2RUN].firstFrame = 16;
	animations[FLAG_STAND2RUN].numFrames = 5;
	animations[FLAG_STAND2RUN].loopFrames = 1;
	animations[FLAG_STAND2RUN].frameLerp = 1000 / 15;
	animations[FLAG_STAND2RUN].initialLerp = 1000 / 15;
	animations[FLAG_STAND2RUN].reversed = qtrue;
	//
	// new anims changes
	//
//	animations[TORSO_GETFLAG].flipflop = qtrue;
//	animations[TORSO_GUARDBASE].flipflop = qtrue;
//	animations[TORSO_PATROL].flipflop = qtrue;
//	animations[TORSO_AFFIRMATIVE].flipflop = qtrue;
//	animations[TORSO_NEGATIVE].flipflop = qtrue;
	//
	return qtrue;
}

/*
==========================
CG_FileExists
==========================
*/
static qboolean	CG_FileExists(const char *filename) {
	int len = trap_FS_FOpenFile( filename, NULL, FS_READ );
	if (len>0) {
		return qtrue;
	}
	return qfalse;
}

/*
==========================
CG_FindClientModelFile
==========================
*/
static qboolean	CG_FindClientModelFile( char *filename, int length, clientInfo_t *ci, const char *characterName, const char *skinName, const char *base, const char *ext ) {
	char *team;

	if ( cgs.gametypeIsTeamGame ) {
		switch ( ci->team ) {
			case TEAM_BLUE: {
				team = "blue";
				break;
			}
			default: {
				team = "red";
				break;
			}
		}
	}
	else {
		team = "default";
	}

	Com_sprintf( filename, length, "characters/%s/models/%s_%s_%s.%s", characterName, base, skinName, team, ext );

	if ( CG_FileExists( filename ) ) {
		return qtrue;
	}

	if ( cgs.gametypeIsTeamGame ) {
		Com_sprintf( filename, length, "characters/%s/models/%s_%s.%s", characterName, base, team, ext );
	}
	else {
		Com_sprintf( filename, length, "characters/%s/models/%s_%s.%s", characterName, base, skinName, ext );
	}
	if ( CG_FileExists( filename ) ) {
		return qtrue;
	}

	return qfalse;
}

/*
==========================
CG_RegisterClientModelname
==========================
*/
static qboolean CG_RegisterClientModelname( clientInfo_t *ci, const char *characterName, const char *skinName ) {
	char	filename[MAX_QPATH];

#if FEAT_IQM
	// Try body.iqm first for single-mesh IQM player model
	Com_sprintf( filename, sizeof( filename ), "characters/%s/models/body.iqm", characterName );
	ci->bodyModel = trap_R_RegisterModel( filename );
	if ( ci->bodyModel ) {
		ci->iqmModel = qtrue;

		// For IQM, the single model serves as legs, torso, and head
		ci->legsModel = ci->bodyModel;
		ci->torsoModel = ci->bodyModel;
		ci->headModel = ci->bodyModel;

		// Load skin (no .skin file fallback — skins are manifest-driven)
		Com_sprintf( filename, sizeof( filename ), "characters/%s/models/body_%s.skin", characterName, skinName );
		ci->bodySkin = trap_R_RegisterSkin( filename );
		ci->legsSkin = ci->bodySkin;
		ci->torsoSkin = ci->bodySkin;
		ci->headSkin = ci->bodySkin;

		// load animations: try embedded IQM anims first, fall back to animation.cfg
		if ( !CG_ParseIQMAnimations( characterName, ci ) ) {
			Com_sprintf( filename, sizeof( filename ), "characters/%s/models/animation.cfg", characterName );
			if ( !CG_ParseAnimationFile( filename, ci ) ) {
				Com_Log( SEV_INFO, LOG_CH(ch_cgame), "Failed to load animation file %s\n", filename );
				return qfalse;
			}
		}

		// load icon
		if ( CG_FindClientModelFile( filename, sizeof(filename), ci, characterName, skinName, "icon", "skin" ) ) {
			ci->modelIcon = trap_R_RegisterShaderNoMip( filename );
		}
		else if ( CG_FindClientModelFile( filename, sizeof(filename), ci, characterName, skinName, "icon", "png" ) ) {
			ci->modelIcon = trap_R_RegisterShaderNoMip( filename );
		}
		else if ( CG_FindClientModelFile( filename, sizeof(filename), ci, characterName, skinName, "icon", "jpg" ) ) {
			ci->modelIcon = trap_R_RegisterShaderNoMip( filename );
		}
#if FEAT_LEGACY_FORMATS_IMAGE
		else if ( CG_FindClientModelFile( filename, sizeof(filename), ci, characterName, skinName, "icon", "tga" ) ) {
			ci->modelIcon = trap_R_RegisterShaderNoMip( filename );
		}
#endif

		return qtrue;
	}
	ci->iqmModel = qfalse;

#endif // FEAT_IQM

	Com_sprintf( filename, sizeof( filename ), "characters/%s/models/lower.md3", characterName );
	ci->legsModel = trap_R_RegisterModel( filename );
	if ( !ci->legsModel ) {
		Com_Log( SEV_INFO, LOG_CH(ch_cgame), "Failed to load model file %s\n", filename );
		return qfalse;
	}

	Com_sprintf( filename, sizeof( filename ), "characters/%s/models/upper.md3", characterName );
	ci->torsoModel = trap_R_RegisterModel( filename );
	if ( !ci->torsoModel ) {
		Com_Log( SEV_INFO, LOG_CH(ch_cgame), "Failed to load model file %s\n", filename );
		return qfalse;
	}

	Com_sprintf( filename, sizeof( filename ), "characters/%s/models/head.md3", characterName );
	ci->headModel = trap_R_RegisterModel( filename );
	if ( !ci->headModel ) {
		Com_Log( SEV_INFO, LOG_CH(ch_cgame), "Failed to load model file %s\n", filename );
		return qfalse;
	}

	// load the animations
	Com_sprintf( filename, sizeof( filename ), "characters/%s/models/animation.cfg", characterName );
	if ( !CG_ParseAnimationFile( filename, ci ) ) {
		Com_Log( SEV_INFO, LOG_CH(ch_cgame), "Failed to load animation file %s\n", filename );
		return qfalse;
	}

	if ( CG_FindClientModelFile( filename, sizeof(filename), ci, characterName, skinName, "icon", "skin" ) ) {
		ci->modelIcon = trap_R_RegisterShaderNoMip( filename );
	}
	else if ( CG_FindClientModelFile( filename, sizeof(filename), ci, characterName, skinName, "icon", "png" ) ) {
		ci->modelIcon = trap_R_RegisterShaderNoMip( filename );
	}
	else if ( CG_FindClientModelFile( filename, sizeof(filename), ci, characterName, skinName, "icon", "jpg" ) ) {
		ci->modelIcon = trap_R_RegisterShaderNoMip( filename );
	}
#if FEAT_LEGACY_FORMATS_IMAGE
	else if ( CG_FindClientModelFile( filename, sizeof(filename), ci, characterName, skinName, "icon", "tga" ) ) {
		ci->modelIcon = trap_R_RegisterShaderNoMip( filename );
	}
#endif

	if ( !ci->modelIcon ) {
		return qfalse;
	}

	return qtrue;
}

/*
====================
CG_ColorFromString
====================
*/
static void CG_ColorFromString( const char *v, vec3_t color ) {
	VectorClear( color );

	int val = atoi( v );

	if ( val < 1 || val > 7 ) {
		VectorSet( color, 1, 1, 1 );
		return;
	}

	if ( val & 1 ) {
		color[2] = 1.0f;
	}
	if ( val & 2 ) {
		color[1] = 1.0f;
	}
	if ( val & 4 ) {
		color[0] = 1.0f;
	}
}

/*
===================
CG_LoadClientInfo

Load it now, taking the disk hits.
This will usually be deferred to a safe time
===================
*/
static void CG_LoadClientInfo( int clientNum, clientInfo_t *ci ) {
	const char	*dir, *fallback;
	int			i, modelloaded;

	modelloaded = qtrue;

	// Try the new character manifest system first.
	if ( CG_LoadCharacter( ci, ci->characterName ) ) {
		ci->deferred = qfalse;
		// The player entity for client N lives at cg_entities[N] — direct access
		// instead of scanning all MAX_GENTITIES.
		if ( clientNum >= 0 && clientNum < MAX_CLIENTS
			&& cg_entities[clientNum].currentState.clientNum == clientNum
			&& cg_entities[clientNum].currentState.eType == ET_PLAYER ) {
			CG_ResetPlayerEntity( &cg_entities[clientNum] );
		}
		return;
	}

	if ( !CG_RegisterClientModelname( ci, ci->characterName, ci->skinName ) ) {
		if ( cg_buildScript.integer ) {
			Com_Terminate( TERM_CLIENT_DROP, "CG_RegisterClientModelname( %s, %s ) failed", ci->characterName, ci->skinName);
		}

		// fall back to default team name
		if( cgs.gametypeIsTeamGame ) {
			// Build team prefix with trailing "/" so CG_FindClientModelFile constructs
			// "characters/visor/models/Pagans/lower_blue.skin" not "Paganslower_blue.skin".
			if ( !CG_RegisterClientModelname( ci, DEFAULT_MODEL, ci->skinName ) ) {
				// Team-specific skin absent — retry with bare default (no team prefix).
				if ( !CG_RegisterClientModelname( ci, DEFAULT_MODEL, "default" ) ) {
					Com_Terminate( TERM_CLIENT_DROP, "DEFAULT_MODEL / skin (%s/%s) failed to register", DEFAULT_MODEL, ci->skinName );
				}
			}
		} else {
			if ( !CG_RegisterClientModelname( ci, DEFAULT_MODEL, "default" ) ) {
				Com_Terminate( TERM_CLIENT_DROP, "DEFAULT_MODEL (%s) failed to register", DEFAULT_MODEL );
			}
		}
		modelloaded = qfalse;
	}

	ci->newAnims = qfalse;
	if ( ci->torsoModel ) {
		orientation_t tag;
		// if the torso model has the "tag_flag"
		if ( trap_R_LerpTag( &tag, ci->torsoModel, 0, 0, 1, "tag_flag" ) ) {
			ci->newAnims = qtrue;
		}
	}

	// sounds — convention path: characters/{name}/sounds/{slot}.opus
	dir = ci->characterName;
	fallback = DEFAULT_MODEL;

	for ( i = 0; i < CM_SOUND_SLOTS; i++ ) {
		ci->sounds[i] = 0;
		if ( modelloaded ) {
			ci->sounds[i] = trap_S_RegisterSound(
				va( "characters/%s/sounds/%s.opus", dir, cg_soundSlotNames[i] ), qfalse );
		}
		if ( !ci->sounds[i] ) {
			ci->sounds[i] = trap_S_RegisterSound(
				va( "characters/%s/sounds/%s.opus", fallback, cg_soundSlotNames[i] ), qfalse );
		}
	}

	ci->deferred = qfalse;

	// Reset the player entity if it's currently live for this client slot —
	// the new model frames may not match the previous one. Player entities are
	// always at cg_entities[clientNum]; no need to walk MAX_GENTITIES.
	if ( clientNum >= 0 && clientNum < MAX_CLIENTS
		&& cg_entities[clientNum].currentState.clientNum == clientNum
		&& cg_entities[clientNum].currentState.eType == ET_PLAYER ) {
		CG_ResetPlayerEntity( &cg_entities[clientNum] );
	}
}

/*
======================
CG_CopyClientInfoModel
======================
*/
static void CG_CopyClientInfoModel( clientInfo_t *from, clientInfo_t *to ) {
	VectorCopy( from->headOffset, to->headOffset );
	to->gender = from->gender;

	to->legsModel = from->legsModel;
	to->legsSkin = from->legsSkin;
	to->torsoModel = from->torsoModel;
	to->torsoSkin = from->torsoSkin;
	to->headModel = from->headModel;
	to->headSkin = from->headSkin;
	to->skinHandle = from->skinHandle;
	to->modelIcon = from->modelIcon;

	to->newAnims = from->newAnims;

	memcpy( to->animations, from->animations, sizeof( to->animations ) );
	memcpy( to->sounds, from->sounds, sizeof( to->sounds ) );
	memcpy( &to->effects, &from->effects, sizeof( to->effects ) );
	memcpy( to->footstepSounds, from->footstepSounds, sizeof( to->footstepSounds ) );
}

/*
======================
CG_FNV1aLower

Case-insensitive FNV-1a over a NUL-terminated string. Used to build the
clientInfo_t.infoHash signature so the scan loops below can reject mismatches
without string compares.
======================
*/
static unsigned int CG_FNV1aLower( unsigned int h, const char *s ) {
	while ( *s ) {
		unsigned char c = (unsigned char)*s++;
		if ( c >= 'A' && c <= 'Z' ) c += 'a' - 'A';
		h ^= c;
		h *= 16777619u;
	}
	h ^= '\0';
	h *= 16777619u;
	return h;
}

/*
======================
CG_HashClientInfo

Build the fast-reject signature: characterName | skinName | team. Loops in
CG_ScanForExistingClientInfo and CG_SetDeferredClientInfo compare this first
and only fall through to Q_stricmp on a hash hit.
======================
*/
static unsigned int CG_HashClientInfo( const clientInfo_t *ci ) {
	unsigned int h = 2166136261u; // FNV-1a offset basis
	h = CG_FNV1aLower( h, ci->characterName );
	h = CG_FNV1aLower( h, ci->skinName );
	h ^= (unsigned int)ci->team;
	h *= 16777619u;
	return h;
}

/*
======================
CG_ScanForExistingClientInfo
======================
*/
static qboolean CG_ScanForExistingClientInfo( clientInfo_t *ci ) {
	clientInfo_t	*match;
	const unsigned int wantHash = CG_HashClientInfo( ci );

	for ( int i = 0 ; i < cgs.maxclients ; i++ ) {
		match = &cgs.clientinfo[ i ];
		if ( !match->infoValid || match->deferred ) {
			continue;
		}
		// Fast-reject on the cached signature before any string compare.
		if ( match->infoHash != wantHash ) {
			continue;
		}
		// Hash hit — full verification (also covers blueTeam/redTeam which
		// aren't in the signature) and the team check.
		if ( !Q_stricmp( ci->characterName, match->characterName )
			&& !Q_stricmp( ci->skinName, match->skinName )
			&& !Q_stricmp( ci->blueTeam, match->blueTeam )
			&& !Q_stricmp( ci->redTeam, match->redTeam )
			&& (!cgs.gametypeIsTeamGame || ci->team == match->team) ) {
			// this clientinfo is identical, so use its handles

			ci->deferred = qfalse;

			CG_CopyClientInfoModel( match, ci );

			return qtrue;
		}
	}

	// nothing matches, so defer the load
	return qfalse;
}

/*
======================
CG_SetDeferredClientInfo

We aren't going to load it now, so grab some other
client's info to use until we have some spare time.
======================
*/
static void CG_SetDeferredClientInfo( int clientNum, clientInfo_t *ci ) {
	int		i;
	clientInfo_t	*match;
	const unsigned int wantHash = CG_HashClientInfo( ci );

	// if someone else is already the same models and skins we
	// can just load the client info
	for ( i = 0 ; i < cgs.maxclients ; i++ ) {
		match = &cgs.clientinfo[ i ];
		if ( !match->infoValid || match->deferred ) {
			continue;
		}
		// Fast-reject via hash signature (characterName + skinName + team) —
		// matches the field set this loop checks. blueTeam/redTeam aren't part
		// of the signature, but this loop doesn't check them either.
		if ( match->infoHash != wantHash ) {
			continue;
		}
		if ( Q_stricmp( ci->skinName, match->skinName ) ||
			 Q_stricmp( ci->characterName, match->characterName ) ||
			 (cgs.gametypeIsTeamGame && ci->team != match->team) ) {
			continue;
		}
		// just load the real info cause it uses the same models and skins
		CG_LoadClientInfo( clientNum, ci );
		return;
	}

	// if we are in teamplay, only grab a model if the skin is correct
	if ( cgs.gametypeIsTeamGame ) {
		for ( i = 0 ; i < cgs.maxclients ; i++ ) {
			match = &cgs.clientinfo[ i ];
			if ( !match->infoValid || match->deferred ) {
				continue;
			}
			if ( Q_stricmp( ci->skinName, match->skinName ) ||
				(cgs.gametypeIsTeamGame && ci->team != match->team) ) {
				continue;
			}
			ci->deferred = qtrue;
			CG_CopyClientInfoModel( match, ci );
			return;
		}
		// load the full model, because we don't ever want to show
		// an improper team skin.  This will cause a hitch for the first
		// player, when the second enters.  Combat shouldn't be going on
		// yet, so it shouldn't matter
		CG_LoadClientInfo( clientNum, ci );
		return;
	}

	// find the first valid clientinfo and grab its stuff
	for ( i = 0 ; i < cgs.maxclients ; i++ ) {
		match = &cgs.clientinfo[ i ];
		if ( !match->infoValid ) {
			continue;
		}

		ci->deferred = qtrue;
		CG_CopyClientInfoModel( match, ci );
		return;
	}

	// we should never get here...
	Com_Log( SEV_INFO, LOG_CH(ch_cgame), "CG_SetDeferredClientInfo: no valid clients!\n" );

	CG_LoadClientInfo( clientNum, ci );
}


/*
======================
CG_NewClientInfo
======================
*/
void CG_NewClientInfo( int clientNum ) {
	clientInfo_t *ci;
	clientInfo_t newInfo;
	const char	*configstring;
	const char	*v;

	ci = &cgs.clientinfo[clientNum];

	configstring = CG_ConfigString( clientNum + CS_PLAYERS );
	if ( !configstring[0] ) {
		memset( ci, 0, sizeof( *ci ) );
		return;		// player just left
	}

	// build into a temp buffer so the defer checks can use
	// the old value
	memset( &newInfo, 0, sizeof( newInfo ) );

	// isolate the player's name
	v = Info_ValueForKey(configstring, "n");
	Q_strncpyz( newInfo.name, v, sizeof( newInfo.name ) );

	// colors
	v = Info_ValueForKey( configstring, "c1" );
	CG_ColorFromString( v, newInfo.color1 );

	newInfo.c1RGBA[0] = 255 * newInfo.color1[0];
	newInfo.c1RGBA[1] = 255 * newInfo.color1[1];
	newInfo.c1RGBA[2] = 255 * newInfo.color1[2];
	newInfo.c1RGBA[3] = 255;

	v = Info_ValueForKey( configstring, "c2" );
	CG_ColorFromString( v, newInfo.color2 );

	newInfo.c2RGBA[0] = 255 * newInfo.color2[0];
	newInfo.c2RGBA[1] = 255 * newInfo.color2[1];
	newInfo.c2RGBA[2] = 255 * newInfo.color2[2];
	newInfo.c2RGBA[3] = 255;

	// bot skill
	v = Info_ValueForKey( configstring, "skill" );
	newInfo.botSkill = atoi( v );

	// wins
	v = Info_ValueForKey( configstring, "w" );
	newInfo.wins = atoi( v );

	// losses
	v = Info_ValueForKey( configstring, "l" );
	newInfo.losses = atoi( v );

	// team
	v = Info_ValueForKey( configstring, "t" );
	newInfo.team = atoi( v );

	// team task
	v = Info_ValueForKey( configstring, "tt" );
	newInfo.teamTask = atoi(v);

	// team leader
	v = Info_ValueForKey( configstring, "tl" );
	newInfo.teamLeader = atoi(v);

	// model
	// character name and skin
	if ( cg_forceSameCharacter.integer ) {
		// force same character makes everyone render as the local player's character
		char charBuf[MAX_QPATH];
		char skinBuf[MAX_QPATH];

		if( cgs.gametypeIsTeamGame ) {
			Q_strncpyz( newInfo.characterName, DEFAULT_MODEL, sizeof( newInfo.characterName ) );
			Q_strncpyz( newInfo.skinName, "default", sizeof( newInfo.skinName ) );
		} else {
			trap_Cvar_VariableStringBuffer( "char", charBuf, sizeof( charBuf ) );
			trap_Cvar_VariableStringBuffer( "skin", skinBuf, sizeof( skinBuf ) );
			if ( !charBuf[0] ) {
				Q_strncpyz( charBuf, DEFAULT_MODEL, sizeof( charBuf ) );
			}
			if ( !skinBuf[0] ) {
				Q_strncpyz( skinBuf, "default", sizeof( skinBuf ) );
			}
			Q_strncpyz( newInfo.characterName, charBuf, sizeof( newInfo.characterName ) );
			Q_strncpyz( newInfo.skinName, skinBuf, sizeof( newInfo.skinName ) );
		}
	} else {
		v = Info_ValueForKey( configstring, "char" );
		Q_strncpyz( newInfo.characterName, v[0] ? v : DEFAULT_MODEL, sizeof( newInfo.characterName ) );

		v = Info_ValueForKey( configstring, "skin" );
		Q_strncpyz( newInfo.skinName, v[0] ? v : "default", sizeof( newInfo.skinName ) );
	}

	// scan for an existing clientinfo that matches this modelname
	// so we can avoid loading checks if possible
	if ( !CG_ScanForExistingClientInfo( &newInfo ) ) {
		qboolean	forceDefer;

		forceDefer = trap_MemoryRemaining() < 4000000;

		// if we are defering loads, just have it pick the first valid
		if ( forceDefer || (cg_deferPlayers.integer && !cg_buildScript.integer && !cg.loading ) ) {
			// keep whatever they had if it won't violate team skins
			CG_SetDeferredClientInfo( clientNum, &newInfo );
			// if we are low on memory, leave them with this model
			if ( forceDefer ) {
				Com_Log( SEV_INFO, LOG_CH(ch_cgame), "Memory is low. Using deferred model.\n" );
				newInfo.deferred = qfalse;
			}
		} else {
			CG_LoadClientInfo( clientNum, &newInfo );
		}
	}

	// replace whatever was there with the new one
	newInfo.infoValid = qtrue;
	newInfo.infoHash = CG_HashClientInfo( &newInfo );
	*ci = newInfo;
}



/*
======================
CG_LoadDeferredPlayers

Called each frame when a player is dead
and the scoreboard is up
so deferred players can be loaded
======================
*/
void CG_LoadDeferredPlayers( void ) {
	int		i;
	clientInfo_t	*ci;

	// scan for a deferred player to load
	for ( i = 0, ci = cgs.clientinfo ; i < cgs.maxclients ; i++, ci++ ) {
		if ( ci->infoValid && ci->deferred ) {
			// if we are low on memory, leave it deferred
			if ( trap_MemoryRemaining() < 4000000 ) {
				Com_Log( SEV_INFO, LOG_CH(ch_cgame), "Memory is low. Using deferred model.\n" );
				ci->deferred = qfalse;
				continue;
			}
			CG_LoadClientInfo( i, ci );
//			break;
		}
	}
}

/*
=============================================================================

PLAYER ANIMATION

=============================================================================
*/


/*
===============
CG_SetLerpFrameAnimation

may include ANIM_TOGGLEBIT
===============
*/
static void CG_SetLerpFrameAnimation( clientInfo_t *ci, lerpFrame_t *lf, int newAnimation ) {
	animation_t	*anim;

	lf->animationNumber = newAnimation;
	newAnimation &= ~ANIM_TOGGLEBIT;

	if ( newAnimation < 0 || newAnimation >= MAX_TOTALANIMATIONS ) {
		Com_Terminate( TERM_CLIENT_DROP, "Bad animation number: %i", newAnimation );
	}

	anim = &ci->animations[ newAnimation ];

	lf->animation = anim;
	lf->animationTime = lf->frameTime + anim->initialLerp;

	if ( cg_debugAnim.integer ) {
		Com_Log( SEV_INFO, LOG_CH(ch_cgame), "Anim: %i\n", newAnimation );
	}
}

/*
===============
CG_RunLerpFrame

Sets cg.snap, cg.oldFrame, and cg.backlerp
cg.time should be between oldFrameTime and frameTime after exit
===============
*/
static void CG_RunLerpFrame( clientInfo_t *ci, lerpFrame_t *lf, int newAnimation, float speedScale ) {
	int			f, numFrames;
	animation_t	*anim;

	// debugging tool to get no animations
	if ( cg_animSpeed.integer == 0 ) {
		lf->oldFrame = lf->frame = lf->backlerp = 0;
		return;
	}

#if FEAT_SPEED_SCALING
	lf->clock += (int)( cg.frametime * speedScale );
	const int t = lf->clock;
#else
	const int t = cg.time;
#endif

	// see if the animation sequence is switching
	if ( newAnimation != lf->animationNumber || !lf->animation ) {
		CG_SetLerpFrameAnimation( ci, lf, newAnimation );
	}

	// if we have passed the current frame, move it to
	// oldFrame and calculate a new frame
	if ( t >= lf->frameTime ) {
		lf->oldFrame = lf->frame;
		lf->oldFrameTime = lf->frameTime;

		// get the next frame based on the animation
		anim = lf->animation;
		if ( !anim->frameLerp ) {
			return;		// shouldn't happen
		}
		if ( t < lf->animationTime ) {
			lf->frameTime = lf->animationTime;		// initial lerp
		} else {
			lf->frameTime = lf->oldFrameTime + anim->frameLerp;
		}
		f = ( lf->frameTime - lf->animationTime ) / anim->frameLerp;
#if !FEAT_SPEED_SCALING
		f *= speedScale;		// adjust for haste, etc
#endif

		numFrames = anim->numFrames;
		if (anim->flipflop) {
			numFrames *= 2;
		}
		if ( f >= numFrames ) {
			f -= numFrames;
			if ( anim->loopFrames ) {
				f %= anim->loopFrames;
				f += anim->numFrames - anim->loopFrames;
			} else {
				f = numFrames - 1;
				// the animation is stuck at the end, so it
				// can immediately transition to another sequence
				lf->frameTime = t;
			}
		}
		if ( anim->reversed ) {
			lf->frame = anim->firstFrame + anim->numFrames - 1 - f;
		}
		else if (anim->flipflop && f>=anim->numFrames) {
			lf->frame = anim->firstFrame + anim->numFrames - 1 - (f%anim->numFrames);
		}
		else {
			lf->frame = anim->firstFrame + f;
		}
		if ( t > lf->frameTime ) {
			lf->frameTime = t;
			if ( cg_debugAnim.integer ) {
				Com_Log( SEV_INFO, LOG_CH(ch_cgame), "Clamp lf->frameTime\n");
			}
		}
	}

	if ( lf->frameTime > t + 200 ) {
		lf->frameTime = t;
	}

	if ( lf->oldFrameTime > t ) {
		lf->oldFrameTime = t;
	}
	// calculate current lerp value
	if ( lf->frameTime == lf->oldFrameTime ) {
		lf->backlerp = 0;
	} else {
		lf->backlerp = 1.0 - (float)( t - lf->oldFrameTime ) / ( lf->frameTime - lf->oldFrameTime );
	}
}


/*
===============
CG_ClearLerpFrame
===============
*/
static void CG_ClearLerpFrame( clientInfo_t *ci, lerpFrame_t *lf, int animationNumber ) {
#if FEAT_SPEED_SCALING
	lf->clock = cg.time;
	lf->frameTime = lf->oldFrameTime = lf->clock;
#else
	lf->frameTime = lf->oldFrameTime = cg.time;
#endif
	CG_SetLerpFrameAnimation( ci, lf, animationNumber );
	lf->oldFrame = lf->frame = lf->animation->firstFrame;
}


/*
===============
CG_PlayerAnimation
===============
*/
static void CG_PlayerAnimation( centity_t *cent, int *legsOld, int *legs, float *legsBackLerp,
						int *torsoOld, int *torso, float *torsoBackLerp ) {
	clientInfo_t	*ci;
	int				clientNum;
	float			speedScale;

	clientNum = cent->currentState.clientNum;

	if ( cg_noPlayerAnims.integer ) {
		*legsOld = *legs = *torsoOld = *torso = 0;
		return;
	}

#if FEAT_SPEED_SCALING
	{
		int legsAnim = cent->currentState.legsAnim & ~ANIM_TOGGLEBIT;

		speedScale = 1.0f;
		if ( legsAnim == LEGS_RUN || legsAnim == LEGS_BACK ||
			 legsAnim == LEGS_WALK || legsAnim == LEGS_WALKCR ||
			 legsAnim == LEGS_BACKWALK || legsAnim == LEGS_BACKCR ) {
			float vel = VectorLength( cent->currentState.pos.trDelta );
			speedScale = vel / 320.0f;
			if ( speedScale < 0.2f ) speedScale = 0.2f;
			if ( speedScale > 2.0f ) speedScale = 2.0f;
			if ( legsAnim == LEGS_WALK || legsAnim == LEGS_WALKCR ) {
				speedScale *= 2.0f;
			}
		}
	}
#else
	if ( cent->currentState.powerups & ( 1 << PW_HASTE ) ) {
		speedScale = 1.5;
	} else {
		speedScale = 1;
	}
#endif

	ci = &cgs.clientinfo[ clientNum ];

#if FEAT_SCREENSHOT_TOOLS
	if ( cg.stopTime ) speedScale = 0;
#endif

	// do the shuffle turn frames locally
	if ( cent->pe.legs.yawing && ( cent->currentState.legsAnim & ~ANIM_TOGGLEBIT ) == LEGS_IDLE ) {
		CG_RunLerpFrame( ci, &cent->pe.legs, LEGS_TURN, speedScale );
	} else {
		CG_RunLerpFrame( ci, &cent->pe.legs, cent->currentState.legsAnim, speedScale );
	}

	*legsOld = cent->pe.legs.oldFrame;
	*legs = cent->pe.legs.frame;
	*legsBackLerp = cent->pe.legs.backlerp;

#if FEAT_SPEED_SCALING
	CG_RunLerpFrame( ci, &cent->pe.torso, cent->currentState.torsoAnim, 1 );
#else
	CG_RunLerpFrame( ci, &cent->pe.torso, cent->currentState.torsoAnim, speedScale );
#endif

	*torsoOld = cent->pe.torso.oldFrame;
	*torso = cent->pe.torso.frame;
	*torsoBackLerp = cent->pe.torso.backlerp;
}

/*
=============================================================================

PLAYER ANGLES

=============================================================================
*/

/*
==================
CG_SwingAngles
==================
*/
static void CG_SwingAngles( float destination, float swingTolerance, float clampTolerance,
					float speed, float *angle, qboolean *swinging ) {
	float	swing;
	float	move;
	float	scale;

	if ( !*swinging ) {
		// see if a swing should be started
		swing = AngleSubtract( *angle, destination );
		if ( swing > swingTolerance || swing < -swingTolerance ) {
			*swinging = qtrue;
		}
	}

	if ( !*swinging ) {
		return;
	}

	// modify the speed depending on the delta
	// so it doesn't seem so linear
	swing = AngleSubtract( destination, *angle );
	scale = fabs( swing );
	if ( scale < swingTolerance * 0.5 ) {
		scale = 0.5;
	} else if ( scale < swingTolerance ) {
		scale = 1.0;
	} else {
		scale = 2.0;
	}

	// swing towards the destination angle
	if ( swing >= 0 ) {
		move = cg.frametime * scale * speed;
		if ( move >= swing ) {
			move = swing;
			*swinging = qfalse;
		}
		*angle = AngleMod( *angle + move );
	} else if ( swing < 0 ) {
		move = cg.frametime * scale * -speed;
		if ( move <= swing ) {
			move = swing;
			*swinging = qfalse;
		}
		*angle = AngleMod( *angle + move );
	}

	// clamp to no more than tolerance
	swing = AngleSubtract( destination, *angle );
	if ( swing > clampTolerance ) {
		*angle = AngleMod( destination - (clampTolerance - 1) );
	} else if ( swing < -clampTolerance ) {
		*angle = AngleMod( destination + (clampTolerance - 1) );
	}
}

/*
=================
CG_AddPainTwitch
=================
*/
static void CG_AddPainTwitch( centity_t *cent, vec3_t torsoAngles ) {
	int		t;
	float	f;

	t = cg.time - cent->pe.painTime;
	if ( t >= PAIN_TWITCH_TIME ) {
		return;
	}

	f = 1.0 - (float)t / PAIN_TWITCH_TIME;

	if ( cent->pe.painDirection ) {
		torsoAngles[ROLL] += 20 * f;
	} else {
		torsoAngles[ROLL] -= 20 * f;
	}
}


/*
===============
CG_PlayerAngles

Handles separate torso motion

  legs pivot based on direction of movement

  head always looks exactly at cent->lerpAngles

  if motion < 20 degrees, show in head only
  if < 45 degrees, also show in torso
===============
*/
static void CG_PlayerAngles( centity_t *cent, vec3_t legs[3], vec3_t torso[3], vec3_t head[3] ) {
	vec3_t		legsAngles, torsoAngles, headAngles;
	float		dest;
	static	int	movementOffsets[8] = { 0, 22, 45, -22, 0, 22, -45, -22 };
	vec3_t		velocity;
	float		speed;
	int			dir, clientNum;
	clientInfo_t	*ci;

	VectorCopy( cent->lerpAngles, headAngles );
	headAngles[YAW] = AngleMod( headAngles[YAW] );
	VectorClear( legsAngles );
	VectorClear( torsoAngles );

	// --------- yaw -------------

	// allow yaw to drift a bit
	if ( ( cent->currentState.legsAnim & ~ANIM_TOGGLEBIT ) != LEGS_IDLE
		|| ((cent->currentState.torsoAnim & ~ANIM_TOGGLEBIT) != TORSO_STAND
		&& (cent->currentState.torsoAnim & ~ANIM_TOGGLEBIT) != TORSO_STAND2)) {
		// if not standing still, always point all in the same direction
		cent->pe.torso.yawing = qtrue;	// always center
		cent->pe.torso.pitching = qtrue;	// always center
		cent->pe.legs.yawing = qtrue;	// always center
	}

	// adjust legs for movement dir
	if ( cent->currentState.eFlags & EF_DEAD ) {
		// don't let dead bodies twitch
		dir = 0;
	} else {
		dir = cent->currentState.angles2[YAW];
		if ( dir < 0 || dir > 7 ) {
			Com_Terminate( TERM_CLIENT_DROP, "Bad player movement angle" );
		}
	}
	legsAngles[YAW] = headAngles[YAW] + movementOffsets[ dir ];
	torsoAngles[YAW] = headAngles[YAW] + 0.25 * movementOffsets[ dir ];

	// torso
	CG_SwingAngles( torsoAngles[YAW], 25, 90, cg_swingSpeed.value, &cent->pe.torso.yawAngle, &cent->pe.torso.yawing );
	CG_SwingAngles( legsAngles[YAW], 40, 90, cg_swingSpeed.value, &cent->pe.legs.yawAngle, &cent->pe.legs.yawing );

	torsoAngles[YAW] = cent->pe.torso.yawAngle;
	legsAngles[YAW] = cent->pe.legs.yawAngle;


	// --------- pitch -------------

	// only show a fraction of the pitch angle in the torso
	if ( headAngles[PITCH] > 180 ) {
		dest = (-360 + headAngles[PITCH]) * 0.75f;
	} else {
		dest = headAngles[PITCH] * 0.75f;
	}
	CG_SwingAngles( dest, 15, 30, 0.1f, &cent->pe.torso.pitchAngle, &cent->pe.torso.pitching );
	torsoAngles[PITCH] = cent->pe.torso.pitchAngle;

	//
	clientNum = cent->currentState.clientNum;
	if ( clientNum >= 0 && clientNum < MAX_CLIENTS ) {
		ci = &cgs.clientinfo[ clientNum ];
		if ( ci->fixedtorso ) {
			torsoAngles[PITCH] = 0.0f;
		}
	}

	// --------- roll -------------


	// lean towards the direction of travel
	VectorCopy( cent->currentState.pos.trDelta, velocity );
	speed = VectorNormalize( velocity );
	if ( speed ) {
		vec3_t	axis[3];
		float	side;

		speed *= 0.05f;

		AnglesToAxis( legsAngles, axis );
		side = speed * DotProduct( velocity, axis[1] );
		legsAngles[ROLL] -= side;

		side = speed * DotProduct( velocity, axis[0] );
		legsAngles[PITCH] += side;
	}

	//
	clientNum = cent->currentState.clientNum;
	if ( clientNum >= 0 && clientNum < MAX_CLIENTS ) {
		ci = &cgs.clientinfo[ clientNum ];
		if ( ci->fixedlegs ) {
			legsAngles[YAW] = torsoAngles[YAW];
			legsAngles[PITCH] = 0.0f;
			legsAngles[ROLL] = 0.0f;
		}
	}

	// pain twitch
	CG_AddPainTwitch( cent, torsoAngles );

	// pull the angles back out of the hierarchial chain
	AnglesSubtract( headAngles, torsoAngles, headAngles );
	AnglesSubtract( torsoAngles, legsAngles, torsoAngles );
	AnglesToAxis( legsAngles, legs );
	AnglesToAxis( torsoAngles, torso );
	AnglesToAxis( headAngles, head );
}


//==========================================================================

/*
===============
CG_HasteTrail
===============
*/
static void CG_HasteTrail( centity_t *cent ) {
	localEntity_t	*smoke;
	vec3_t			origin;
	int				anim;

	if ( cent->trailTime > cg.time ) {
		return;
	}
	anim = cent->pe.legs.animationNumber & ~ANIM_TOGGLEBIT;
	if ( anim != LEGS_RUN && anim != LEGS_BACK ) {
		return;
	}

	cent->trailTime += 100;
	if ( cent->trailTime < cg.time ) {
		cent->trailTime = cg.time;
	}

	VectorCopy( cent->lerpOrigin, origin );
	origin[2] -= 16;

	smoke = CG_SmokePuff( origin, vec3_origin,
				  8,
				  1, 1, 1, 1,
				  500,
				  cg.time,
				  0,
				  0,
				  cgs.media.hastePuffShader );

	// use the optimized local entity add
	smoke->leType = LE_SCALE_FADE;
}

/*
===============
CG_BreathPuffs
===============
*/
static void CG_BreathPuffs( centity_t *cent, refEntity_t *head) {
	clientInfo_t *ci;
	vec3_t up, origin;

	ci = &cgs.clientinfo[ cent->currentState.number ];

	if (cg_envTemperature.integer > 0) {
		return;
	}
	if ( cent->currentState.number == cg.snap->ps.clientNum && !cg.renderingThirdPerson) {
		return;
	}
	if ( cent->currentState.eFlags & EF_DEAD ) {
		return;
	}
	int contents = CG_PointContents( head->origin, 0 );
	if ( contents & ( CONTENTS_WATER | CONTENTS_SLIME | CONTENTS_LAVA ) ) {
		return;
	}
	if ( ci->breathPuffTime > cg.time ) {
		return;
	}

	VectorSet( up, 0, 0, 8 );
	VectorMA(head->origin, 8, head->axis[0], origin);
	VectorMA(origin, -4, head->axis[2], origin);
	CG_SmokePuff( origin, up, 16, 1, 1, 1, 0.66f, 1500, cg.time, cg.time + 400, LEF_PUFF_DONT_SCALE, cgs.media.shotgunSmokePuffShader );
	ci->breathPuffTime = cg.time + 2000;
}

/*
===============
CG_BubblePuffs

Underwater bubble emission from player's head. Player-driven cosmetic.
Visible in third-person and on remote players; suppressed in first-person
for the local player to avoid HUD clutter.
===============
*/
static void CG_BubblePuffs( centity_t *cent, refEntity_t *head ) {
	clientInfo_t  *ci;
	vec3_t         origin;
	int            contents;
	float          speed;
	int            interval;
	localEntity_t *le;
	refEntity_t   *re;

	ci = &cgs.clientinfo[ cent->currentState.number ];

	if ( cent->currentState.number == cg.snap->ps.clientNum && !cg.renderingThirdPerson ) {
		return;
	}
	if ( cent->currentState.eFlags & EF_DEAD ) {
		return;
	}

	contents = CG_PointContents( head->origin, 0 );
	if ( !( contents & ( CONTENTS_WATER | CONTENTS_SLIME | CONTENTS_LAVA ) ) ) {
		return;
	}

	if ( ci->bubblePuffTime > cg.time ) {
		return;
	}

	/* movement-rate-driven cadence: idle ~1500ms, sprinting ~400ms */
	speed = VectorLength( cent->currentState.pos.trDelta );
	interval = (int)( 1500.0f - speed * 3.5f );
	if ( interval < 400 )  interval = 400;
	if ( interval > 1500 ) interval = 1500;
	interval += ( rand() % 200 ) - 100;

	le = CG_AllocLocalEntity();
	le->leFlags  = LEF_PUFF_DONT_SCALE;
	le->leType   = LE_MOVE_SCALE_FADE;
	le->startTime = cg.time;
	le->endTime   = cg.time + 1000 + ( rand() % 250 );
	le->lifeRate  = 1.0f / ( le->endTime - le->startTime );

	re = &le->refEntity;
	re->shaderTime.f  = cg.time / 1000.0f;
	re->reType        = RT_SPRITE;
	re->rotation      = 0;
	re->radius        = 3;
	re->customShader  = cgs.media.waterBubbleShader;
	re->shader.rgba[0] = 0xff;
	re->shader.rgba[1] = 0xff;
	re->shader.rgba[2] = 0xff;
	re->shader.rgba[3] = 0xff;

	le->color[3] = 1.0f;

	VectorMA( head->origin, 4, head->axis[0], origin );
	VectorMA( origin, 4, head->axis[2], origin );

	le->pos.trType    = TR_LINEAR;
	le->pos.trTime    = cg.time;
	VectorCopy( origin, le->pos.trBase );
	le->pos.trDelta[0] = crandom() * 4;
	le->pos.trDelta[1] = crandom() * 4;
	le->pos.trDelta[2] = 12 + crandom() * 4;

	ci->bubblePuffTime = cg.time + interval;
}

/*
===============
CG_DustTrail
===============
*/
static void CG_DustTrail( centity_t *cent ) {
	int				anim;
	vec3_t end, vel;
	trace_t tr;

	if (!cg_envGroundDusty.integer)
		return;

	if ( cent->dustTrailTime > cg.time ) {
		return;
	}

	anim = cent->pe.legs.animationNumber & ~ANIM_TOGGLEBIT;
	if ( anim != LEGS_LANDB && anim != LEGS_LAND ) {
		return;
	}

	cent->dustTrailTime += 40;
	if ( cent->dustTrailTime < cg.time ) {
		cent->dustTrailTime = cg.time;
	}

	VectorCopy(cent->currentState.pos.trBase, end);
	end[2] -= 64;
	CG_Trace( &tr, cent->currentState.pos.trBase, NULL, NULL, end, cent->currentState.number, MASK_PLAYERSOLID );

	if ( !(tr.surfaceFlags & SURF_DUST) )
		return;

	VectorCopy( cent->currentState.pos.trBase, end );
	end[2] -= 16;

	VectorSet(vel, 0, 0, -30);
	CG_SmokePuff( end, vel,
				  24,
				  .8f, .8f, 0.7f, 0.33f,
				  500,
				  cg.time,
				  0,
				  0,
				  cgs.media.dustPuffShader );
}

/*
===============
CG_TrailItem
===============
*/
static void CG_TrailItem( centity_t *cent, qhandle_t hModel ) {
	refEntity_t		ent;
	vec3_t			angles;
	vec3_t			axis[3];

	VectorCopy( cent->lerpAngles, angles );
	angles[PITCH] = 0;
	angles[ROLL] = 0;
	AnglesToAxis( angles, axis );

	memset( &ent, 0, sizeof( ent ) );
	VectorMA( cent->lerpOrigin, -16, axis[0], ent.origin );
	ent.origin[2] += 16;
	angles[YAW] += 90;
	AnglesToAxis( angles, ent.axis );

	ent.hModel = hModel;
	trap_R_AddRefEntityToScene( &ent );
}


/*
===============
CG_PlayerFlag
===============
*/
static void CG_PlayerFlag( centity_t *cent, qhandle_t hSkin, refEntity_t *torso ) {
	clientInfo_t	*ci;
	refEntity_t	pole;
	refEntity_t	flag;
	vec3_t		angles, dir;
	int			legsAnim, flagAnim, updateangles;
	float		angle, d;

	// show the flag pole model
	memset( &pole, 0, sizeof(pole) );
	pole.hModel = cgs.media.flagPoleModel;
	VectorCopy( torso->lightingOrigin, pole.lightingOrigin );
	pole.shadowPlane = torso->shadowPlane;
	pole.renderfx = torso->renderfx;
	CG_PositionEntityOnTag( &pole, torso, torso->hModel, "tag_flag" );
	trap_R_AddRefEntityToScene( &pole );

	// show the flag model
	memset( &flag, 0, sizeof(flag) );
	flag.hModel = cgs.media.flagFlapModel;
	flag.customSkin = hSkin;
	VectorCopy( torso->lightingOrigin, flag.lightingOrigin );
	flag.shadowPlane = torso->shadowPlane;
	flag.renderfx = torso->renderfx;

	VectorClear(angles);

	updateangles = qfalse;
	legsAnim = cent->currentState.legsAnim & ~ANIM_TOGGLEBIT;
	if( legsAnim == LEGS_IDLE || legsAnim == LEGS_IDLECR ) {
		flagAnim = FLAG_STAND;
	} else if ( legsAnim == LEGS_WALK || legsAnim == LEGS_WALKCR ) {
		flagAnim = FLAG_STAND;
		updateangles = qtrue;
	} else {
		flagAnim = FLAG_RUN;
		updateangles = qtrue;
	}

	if ( updateangles ) {

		VectorCopy( cent->currentState.pos.trDelta, dir );
		// add gravity
		dir[2] += 100;
		VectorNormalize( dir );
		d = DotProduct(pole.axis[2], dir);
		// if there is enough movement orthogonal to the flag pole
		if (fabs(d) < 0.9) {
			//
			d = DotProduct(pole.axis[0], dir);
			if (d > 1.0f) {
				d = 1.0f;
			}
			else if (d < -1.0f) {
				d = -1.0f;
			}
			angle = acos(d);

			d = DotProduct(pole.axis[1], dir);
			if (d < 0) {
				angles[YAW] = 360 - angle * 180 / M_PI;
			}
			else {
				angles[YAW] = angle * 180 / M_PI;
			}
			if (angles[YAW] < 0)
				angles[YAW] += 360;
			if (angles[YAW] > 360)
				angles[YAW] -= 360;

			//vectoangles( cent->currentState.pos.trDelta, tmpangles );
			//angles[YAW] = tmpangles[YAW] + 45 - cent->pe.torso.yawAngle;
			// change the yaw angle
			CG_SwingAngles( angles[YAW], 25, 90, 0.15f, &cent->pe.flag.yawAngle, &cent->pe.flag.yawing );
		}

		/*
		d = DotProduct(pole.axis[2], dir);
		angle = Q_acos(d);

		d = DotProduct(pole.axis[1], dir);
		if (d < 0) {
			angle = 360 - angle * 180 / M_PI;
		}
		else {
			angle = angle * 180 / M_PI;
		}
		if (angle > 340 && angle < 20) {
			flagAnim = FLAG_RUNUP;
		}
		if (angle > 160 && angle < 200) {
			flagAnim = FLAG_RUNDOWN;
		}
		*/
	}

	// set the yaw angle
	angles[YAW] = cent->pe.flag.yawAngle;
	// lerp the flag animation frames
	ci = &cgs.clientinfo[ cent->currentState.clientNum ];
	CG_RunLerpFrame( ci, &cent->pe.flag, flagAnim, 1 );
	flag.oldframe = cent->pe.flag.oldFrame;
	flag.frame = cent->pe.flag.frame;
	flag.backlerp = cent->pe.flag.backlerp;

	AnglesToAxis( angles, flag.axis );
	CG_PositionRotatedEntityOnTag( &flag, &pole, pole.hModel, "tag_flag" );

	trap_R_AddRefEntityToScene( &flag );
}


#if FEAT_HARVESTER
/*
===============
CG_PlayerTokens
===============
*/
static void CG_PlayerTokens( centity_t *cent, int renderfx ) {
	int			tokens, i, j;
	float		angle;
	refEntity_t	ent;
	vec3_t		dir, origin;
	skulltrail_t *trail;
	if ( cent->currentState.number >= MAX_CLIENTS ) {
		return;
	}
	trail = &cg.skulltrails[cent->currentState.number];
	tokens = cent->currentState.generic1;
	if ( !tokens ) {
		trail->numpositions = 0;
		return;
	}

	if ( tokens > MAX_SKULLTRAIL ) {
		tokens = MAX_SKULLTRAIL;
	}

	// add skulls if there are more than last time
	for (i = 0; i < tokens - trail->numpositions; i++) {
		for (j = trail->numpositions; j > 0; j--) {
			VectorCopy(trail->positions[j-1], trail->positions[j]);
		}
		VectorCopy(cent->lerpOrigin, trail->positions[0]);
	}
	trail->numpositions = tokens;

	// move all the skulls along the trail
	VectorCopy(cent->lerpOrigin, origin);
	for (i = 0; i < trail->numpositions; i++) {
		VectorSubtract(trail->positions[i], origin, dir);
		if (VectorNormalize(dir) > 30) {
			VectorMA(origin, 30, dir, trail->positions[i]);
		}
		VectorCopy(trail->positions[i], origin);
	}

	memset( &ent, 0, sizeof( ent ) );
	if( cgs.clientinfo[ cent->currentState.clientNum ].team == TEAM_BLUE ) {
		ent.hModel = cgs.media.redCubeModel;
	} else {
		ent.hModel = cgs.media.blueCubeModel;
	}
	ent.renderfx = renderfx;

	VectorCopy(cent->lerpOrigin, origin);
	for (i = 0; i < trail->numpositions; i++) {
		VectorSubtract(origin, trail->positions[i], ent.axis[0]);
		ent.axis[0][2] = 0;
		VectorNormalize(ent.axis[0]);
		VectorSet(ent.axis[2], 0, 0, 1);
		CrossProduct(ent.axis[0], ent.axis[2], ent.axis[1]);

		VectorCopy(trail->positions[i], ent.origin);
		angle = (((cg.time + 500 * MAX_SKULLTRAIL - 500 * i) / 16) & 255) * (M_PI * 2) / 255;
		ent.origin[2] += sin(angle) * 10;
		trap_R_AddRefEntityToScene( &ent );
		VectorCopy(trail->positions[i], origin);
	}
}
#endif


/*
===============
CG_PlayerPowerups
===============
*/
static void CG_PlayerPowerups( centity_t *cent, refEntity_t *torso ) {
	int		powerups;
	clientInfo_t	*ci;

	powerups = cent->currentState.powerups;
	if ( !powerups ) {
		return;
	}

	// quad gives a dlight
	if ( powerups & ( 1 << PW_QUAD ) ) {
		trap_R_AddLightToScene( cent->lerpOrigin, 200 + (rand()&31), 0.2f, 0.2f, 1 );
	}

	// berserk gives a dlight
	if ( powerups & ( 1 << PW_BERSERK ) ) {
		trap_R_AddLightToScene( cent->lerpOrigin, 200 + (rand()&31), 1, 0.0f, 0.0f );
	}

	// flight plays a looped sound
	if ( powerups & ( 1 << PW_FLIGHT ) ) {
		trap_S_AddLoopingSound( cent->currentState.number, cent->lerpOrigin, vec3_origin, cgs.media.flightSound );
	}

	ci = &cgs.clientinfo[ cent->currentState.clientNum ];
	// redflag
	if ( powerups & ( 1 << PW_REDFLAG ) ) {
		if (ci->newAnims) {
			CG_PlayerFlag( cent, cgs.media.redFlagFlapSkin, torso );
		}
		else {
			CG_TrailItem( cent, cgs.media.redFlagModel );
		}
		trap_R_AddLightToScene( cent->lerpOrigin, 200 + (rand()&31), 1.0, 0.2f, 0.2f );
	}

	// blueflag
	if ( powerups & ( 1 << PW_BLUEFLAG ) ) {
		if (ci->newAnims){
			CG_PlayerFlag( cent, cgs.media.blueFlagFlapSkin, torso );
		}
		else {
			CG_TrailItem( cent, cgs.media.blueFlagModel );
		}
		trap_R_AddLightToScene( cent->lerpOrigin, 200 + (rand()&31), 0.2f, 0.2f, 1.0 );
	}

	// neutralflag
	if ( powerups & ( 1 << PW_NEUTRALFLAG ) ) {
		if (ci->newAnims) {
			CG_PlayerFlag( cent, cgs.media.neutralFlagFlapSkin, torso );
		}
		else {
			CG_TrailItem( cent, cgs.media.neutralFlagModel );
		}
		trap_R_AddLightToScene( cent->lerpOrigin, 200 + (rand()&31), 1.0, 1.0, 1.0 );
	}

	// haste leaves smoke trails
	if ( powerups & ( 1 << PW_HASTE ) ) {
		CG_HasteTrail( cent );
	}
}


/*
===============
CG_PlayerFloatSprite

Float a sprite over the player's head
===============
*/
static void CG_PlayerFloatSprite( centity_t *cent, qhandle_t shader ) {
	int				rf;
	refEntity_t		ent;

	if ( cent->currentState.number == cg.snap->ps.clientNum && !cg.renderingThirdPerson ) {
		rf = RF_THIRD_PERSON;		// only show in mirrors
	} else {
		rf = 0;
	}

	memset( &ent, 0, sizeof( ent ) );
	VectorCopy( cent->lerpOrigin, ent.origin );
	ent.origin[2] += 48;
	ent.reType = RT_SPRITE;
	ent.customShader = shader;
	ent.radius = 10;
	ent.renderfx = rf | RF_DEPTHHACK;
	ent.shaderRGBA[0] = 255;
	ent.shaderRGBA[1] = 255;
	ent.shaderRGBA[2] = 255;
	ent.shaderRGBA[3] = 255;
	trap_R_AddRefEntityToScene( &ent );
}

static qhandle_t CG_GetPlayerSpriteShader(centity_t *cent) {
	qhandle_t *shaderarr = cgs.media.friendColorShaders;
	int effectiveHealth;

	if (cgs.clientinfo[cent->currentState.number].infoValid) {
		effectiveHealth = BG_GetEffectiveHealth(
			cgs.clientinfo[cent->currentState.number].health,
			cgs.clientinfo[cent->currentState.number].armorClass,
			cgs.clientinfo[cent->currentState.number].armor
		);
	} else {
		effectiveHealth = 100;
	}

	if (effectiveHealth > 300) {
		return shaderarr[0];
	}
	if ( effectiveHealth > 200 ) {
		return shaderarr[1];
	}
	if ( effectiveHealth > 150 ) {
		return shaderarr[2];
	}
	if ( effectiveHealth > 100 ) {
		return shaderarr[3];
	}
	if ( effectiveHealth > 80 ) {
		return shaderarr[4];
	}
	return shaderarr[5];
}

static qboolean CG_FriendVisible(centity_t *cent) {
	vec3_t start;
	trace_t trace;

	VectorCopy( cg.refdef.vieworg, start );
	CG_Trace(&trace, start, vec3_origin, vec3_origin, cent->lerpOrigin,
			cg.snap->ps.clientNum,
			CONTENTS_SOLID );

	if (trace.fraction == 1.0) {
		return qtrue;
	}
	return qfalse;

	//VectorCopy( cg.refdef.vieworg, start );
	//CG_Trace(&trace, start, vec3_origin, vec3_origin, cent->lerpOrigin,
	//		cg.snap->ps.clientNum,
	//		CONTENTS_SOLID |CONTENTS_BODY );

	//if (trace.entityNum <= MAX_CLIENTS && trace.entityNum == cent->currentState.clientNum) {
	//	return qtrue;
	//}
	//return qfalse;
}

static void CG_FriendFlagIndicator(centity_t *cent) {
	int powerups = cent->currentState.powerups;
	qhandle_t shader = 0;
	refEntity_t ent;

	if (powerups & ( 1 << PW_REDFLAG)) {
		shader = cgs.media.friendFlagShaderRed;
	} else if (powerups & ( 1 << PW_BLUEFLAG)) {
		shader = cgs.media.friendFlagShaderBlue;
	} else if (powerups & (1 << PW_NEUTRALFLAG)) {
		shader = cgs.media.friendFlagShaderNeutral;
	} else {
		return;
	}

	//if (CG_FriendVisible(cent)) {
	//	return;
	//}

	int rf;
	if ( cent->currentState.number == cg.snap->ps.clientNum && !cg.renderingThirdPerson ) {
		rf = RF_THIRD_PERSON;		// only show in mirrors
	} else {
		rf = 0;
	}

	memset( &ent, 0, sizeof( ent ) );
	VectorCopy( cent->lerpOrigin, ent.origin );
	ent.origin[2] += 0;
	ent.reType = RT_SPRITE;
	ent.customShader = shader;
	ent.radius = 32;
	ent.renderfx = rf;
	ent.shaderRGBA[0] = 255;
	ent.shaderRGBA[1] = 255;
	ent.shaderRGBA[2] = 255;
	ent.shaderRGBA[3] = 255;
	trap_R_AddRefEntityToScene( &ent );
}

qboolean CG_OnSameTeam( centity_t *cent ) {
	if (cent->currentState.number == cg.snap->ps.clientNum) {
        return qfalse;
    }

	if ( cgs.gametypeIsTeamGame ) {
		if ( cg.snap->ps.persistant[PERS_TEAM] != cgs.clientinfo[ cent->currentState.clientNum ].team ) {
			return qfalse;
		}

		return qtrue;
	}

    if (cgs.gametype == GT_KINGOFTHEHILL) {
		if ( cg.snap->ps.powerups[PW_KING] ) {
			return qfalse;
		}

        if (cent->currentState.powerups & (1 << PW_KING)) {
            return qfalse;
        }

		return qtrue;
    }

	return qfalse;
}

/*
===============
CG_WorldToScreen

Projects a 3D world point into virtual 640x480 screen coordinates.
Returns qfalse if the point is behind the camera.
===============
*/
qboolean CG_WorldToScreen( vec3_t point, float *x, float *y ) {
	vec3_t	trans;
	float	px, py, z;

	px = (float)tan( cg.refdef.fov_x * M_PI / 360.0 );
	py = (float)tan( cg.refdef.fov_y * M_PI / 360.0 );

	VectorSubtract( point, cg.refdef.vieworg, trans );
	z = DotProduct( trans, cg.refdef.viewaxis[0] );
	if ( z <= 0.001f ) {
		return qfalse;
	}

	*x = 320.0f - DotProduct( trans, cg.refdef.viewaxis[1] ) * 320.0f / ( z * px );
	*y = 240.0f - DotProduct( trans, cg.refdef.viewaxis[2] ) * 240.0f / ( z * py );
	return qtrue;
}

/*
================
CG_Draw2DBotDirectives

2D pass: called from CG_Draw2D (AFTER trap_R_RenderScene) so that
trap_R_DrawTextNorm is called while the renderer is in 2D mode.
Iterates all clients, projects their lerpOrigin to screen, draws directive text.
lerpOrigins are already updated by CG_AddPacketEntities before this runs.
================
*/
void CG_Draw2DBotDirectives( void ) {
	botDirectiveDisplay_t	*bd;
	centity_t			*cent;
	vec3_t				origin;
	float				x, y, dist;
	char				text[128];
	vec4_t				color;

	if ( !cg.snap ) {
		return;
	}

	for ( int i = 0; i < MAX_CLIENTS; i++ ) {
		// only bots
		if ( !cgs.clientinfo[i].infoValid || !cgs.clientinfo[i].botSkill ) {
			continue;
		}

		bd = &cg_botDirectives[i];
		if ( !bd->type ) {
			continue;
		}

		cent = &cg_entities[i];

		// skip dead entities
		if ( cent->currentState.eFlags & EF_DEAD ) {
			continue;
		}

		// distance cull — beyond 1500 units text is too small to read
		dist = Distance( cg.refdef.vieworg, cent->lerpOrigin );
		if ( dist > 1500.0f ) {
			continue;
		}

		// project origin above head
		VectorCopy( cent->lerpOrigin, origin );
		origin[2] += 56;	// slightly above where sprites float (48)

		if ( !CG_WorldToScreen( origin, &x, &y ) ) {
			continue;
		}

		// build display text; color prefix encodes directive category
		switch ( bd->type ) {
			case 5:		// DIR_SEEK_ITEM
				Com_sprintf( text, sizeof( text ), "^3> %s", bd->targetName );
				break;
			case 9:		// DIR_KILL_TARGET
				Com_sprintf( text, sizeof( text ), "^1X %s", bd->targetName );
				break;
			case 2:		// DIR_DEFEND_AREA
				Com_sprintf( text, sizeof( text ), "^5# DEFEND" );
				break;
			case 3:		// DIR_CAMP_SPOT
				Com_sprintf( text, sizeof( text ), "^5# CAMP" );
				break;
			case 4:		// DIR_PATROL
				Com_sprintf( text, sizeof( text ), "^5# PATROL" );
				break;
			case 6:		// DIR_RUSH_BASE
				Com_sprintf( text, sizeof( text ), "^2< RUSH BASE" );
				break;
			case 7:		// DIR_RETURN_FLAG
				Com_sprintf( text, sizeof( text ), "^2< RETURN FLAG" );
				break;
			case 8:		// DIR_ATTACK_BASE
				Com_sprintf( text, sizeof( text ), "^1X ATTACK" );
				break;
			case 10:	// DIR_HARVEST
				Com_sprintf( text, sizeof( text ), "^3# HARVEST" );
				break;
			case 1:		// DIR_FOLLOW
				Com_sprintf( text, sizeof( text ), "^6@ %s", bd->targetName );
				break;
			default:
				continue;
		}

		Vector4Set( color, 1.0f, 1.0f, 1.0f, 0.85f );

		trap_R_DrawTextNorm(
			text,
			x * NORM_HSCALE,
			y * NORM_VSCALE,
			FONT_UI,
			(float)SMALLCHAR_HEIGHT * NORM_VSCALE,
			color,
			TEXT_ALIGN_CENTER,
			TEXT_DROPSHADOW
		);
	}
}

/*
===============
CG_PlayerSprites

Float sprites over the player's head
===============
*/
static void CG_PlayerSprites( centity_t *cent ) {
	if (cent->currentState.number == cg.snap->ps.clientNum) {
        return;
    }

	if ( CG_OnSameTeam( cent ) && !CG_FriendVisible( cent ) ) {
		qhandle_t shader;

		CG_FriendFlagIndicator( cent );

		shader = CG_GetPlayerSpriteShader(cent);
		if ( shader ) {
			CG_PlayerFloatSprite( cent, shader );
			return;
		}
	}

	if ( cent->currentState.eFlags & EF_CONNECTION ) {
		CG_PlayerFloatSprite( cent, cgs.media.connectionShader );
		return;
	}

	if ( cent->currentState.eFlags & EF_TALK ) {
		CG_PlayerFloatSprite( cent, cgs.media.balloonShader );
		return;
	}

    if ((cent->currentState.powerups & (1 << PW_INVIS)) || (cent->currentState.eFlags & EF_CLOAK)) {
    	return;
    }

	if ( cent->currentState.eFlags & EF_AWARD_IMPRESSIVE ) {
		CG_PlayerFloatSprite( cent, cgs.media.medalImpressive );
		return;
	}

	if ( cent->currentState.eFlags & EF_AWARD_EXCELLENT ) {
		CG_PlayerFloatSprite( cent, cgs.media.medalExcellent );
		return;
	}

	if ( cent->currentState.eFlags & EF_AWARD_GAUNTLET ) {
		CG_PlayerFloatSprite( cent, cgs.media.medalGauntlet );
		return;
	}

	if ( cent->currentState.eFlags & EF_AWARD_DEFEND ) {
		CG_PlayerFloatSprite( cent, cgs.media.medalDefend );
		return;
	}

	if ( cent->currentState.eFlags & EF_AWARD_ASSIST ) {
		CG_PlayerFloatSprite( cent, cgs.media.medalAssist );
		return;
	}

	if ( cent->currentState.eFlags & EF_AWARD_CAP ) {
		CG_PlayerFloatSprite( cent, cgs.media.medalCapture );
		return;
	}

    if (cgs.gametype == GT_KINGOFTHEHILL && !(cent->currentState.eFlags & EF_DEAD)) {
        if ( cent->currentState.powerups & (1 << PW_KING) ) {
            CG_PlayerFloatSprite(cent, cgs.media.medalExcellent);
            return;
        }

        if ( cg.snap->ps.powerups[PW_KING] && !CG_IsPlayerInvisible(cent) ) {
            CG_PlayerFloatSprite(cent, cgs.media.friendShader);
            return;
        }
    }

	if ( !(cent->currentState.eFlags & EF_DEAD) &&
		cg.snap->ps.persistant[PERS_TEAM] == cgs.clientinfo[ cent->currentState.clientNum ].team &&
		cgs.gametypeIsTeamGame) {
		if (cg_drawFriend.integer) {
			CG_PlayerFloatSprite( cent, cgs.media.friendShader );
		}
		return;
	}
}

/*
===============
CG_PlayerShadow

Returns the Z component of the surface being shadowed

  should it return a full plane instead of a Z?
===============
*/
#define	SHADOW_DISTANCE		128
static qboolean CG_PlayerShadow( centity_t *cent, float *shadowPlane ) {
	vec3_t		end, mins = {-15, -15, 0}, maxs = {15, 15, 2};
	trace_t		trace;
	float		alpha;

	*shadowPlane = 0;

	if ( cg_shadows.integer == 0 ) {
		return qfalse;
	}

	// no shadows when invisible
	if ( CG_IsPlayerInvisible(cent) ) {
		return qfalse;
	}

	// send a trace down from the player to the ground
	VectorCopy( cent->lerpOrigin, end );
	end[2] -= SHADOW_DISTANCE;

	trap_CM_BoxTrace( &trace, cent->lerpOrigin, end, mins, maxs, 0, MASK_PLAYERSOLID );

	// no shadow if too high
	if ( trace.fraction == 1.0 || trace.startsolid || trace.allsolid ) {
		return qfalse;
	}

	*shadowPlane = trace.endpos[2] + 1;

	if ( cg_shadows.integer != 1 ) {	// no mark for stencil or projection shadows
		return qtrue;
	}

	// fade the shadow out with height
	alpha = 1.0 - trace.fraction;

	// hack / FPE - bogus planes?
	//assert( DotProduct( trace.plane.normal, trace.plane.normal ) != 0.0f )

	// add the mark as a temporary, so it goes directly to the renderer
	// without taking a spot in the cg_marks array
	CG_ImpactMark( cgs.media.shadowMarkShader, trace.endpos, trace.plane.normal,
		cent->pe.legs.yawAngle, alpha,alpha,alpha,1, qfalse, 24, qtrue );

	return qtrue;
}


/*
===============
CG_PlayerSplash

Draw a mark at the water surface
===============
*/
static void CG_PlayerSplash( centity_t *cent ) {
	vec3_t		start, end;
	trace_t		trace;
	int			contents;
	polyVert_t	verts[4];

	if ( !cg_shadows.integer ) {
		return;
	}

	VectorCopy( cent->lerpOrigin, end );
	end[2] -= 24;

	// if the feet aren't in liquid, don't make a mark
	// this won't handle moving water brushes, but they wouldn't draw right anyway...
	contents = CG_PointContents( end, 0 );
	if ( !( contents & ( CONTENTS_WATER | CONTENTS_SLIME | CONTENTS_LAVA ) ) ) {
		return;
	}

	VectorCopy( cent->lerpOrigin, start );
	start[2] += 32;

	// if the head isn't out of liquid, don't make a mark
	contents = CG_PointContents( start, 0 );
	if ( contents & ( CONTENTS_SOLID | CONTENTS_WATER | CONTENTS_SLIME | CONTENTS_LAVA ) ) {
		return;
	}

	// trace down to find the surface
	trap_CM_BoxTrace( &trace, start, end, NULL, NULL, 0, ( CONTENTS_WATER | CONTENTS_SLIME | CONTENTS_LAVA ) );

	if ( trace.fraction == 1.0 ) {
		return;
	}

	// create a mark polygon
	VectorCopy( trace.endpos, verts[0].xyz );
	verts[0].xyz[0] -= 32;
	verts[0].xyz[1] -= 32;
	verts[0].st[0] = 0;
	verts[0].st[1] = 0;
	verts[0].modulate.rgba[0] = 255;
	verts[0].modulate.rgba[1] = 255;
	verts[0].modulate.rgba[2] = 255;
	verts[0].modulate.rgba[3] = 255;

	VectorCopy( trace.endpos, verts[1].xyz );
	verts[1].xyz[0] -= 32;
	verts[1].xyz[1] += 32;
	verts[1].st[0] = 0;
	verts[1].st[1] = 1;
	verts[1].modulate.rgba[0] = 255;
	verts[1].modulate.rgba[1] = 255;
	verts[1].modulate.rgba[2] = 255;
	verts[1].modulate.rgba[3] = 255;

	VectorCopy( trace.endpos, verts[2].xyz );
	verts[2].xyz[0] += 32;
	verts[2].xyz[1] += 32;
	verts[2].st[0] = 1;
	verts[2].st[1] = 1;
	verts[2].modulate.rgba[0] = 255;
	verts[2].modulate.rgba[1] = 255;
	verts[2].modulate.rgba[2] = 255;
	verts[2].modulate.rgba[3] = 255;

	VectorCopy( trace.endpos, verts[3].xyz );
	verts[3].xyz[0] += 32;
	verts[3].xyz[1] -= 32;
	verts[3].st[0] = 1;
	verts[3].st[1] = 0;
	verts[3].modulate.rgba[0] = 255;
	verts[3].modulate.rgba[1] = 255;
	verts[3].modulate.rgba[2] = 255;
	verts[3].modulate.rgba[3] = 255;

	trap_R_AddPolyToScene( cgs.media.wakeMarkShader, 4, verts );
}



/*
===============
CG_AddRefEntityWithPowerups

Adds a piece with modifications or duplications for powerups
Also called by CG_Missile for quad rockets, but nobody can tell...
===============
*/
void CG_AddRefEntityWithPowerups( centity_t *cent, refEntity_t *ent, entityState_t *state, qboolean isPlayerPart, int team ) {

	if ( isPlayerPart && CG_IsPlayerInvisible(cent) ) {
		ent->customShader = cgs.media.invisShader;
		trap_R_AddRefEntityToScene( ent );
	} else {
		/*
		if ( state->eFlags & EF_KAMIKAZE ) {
			if (team == TEAM_BLUE)
				ent->customShader = cgs.media.blueKamikazeShader;
			else
				ent->customShader = cgs.media.redKamikazeShader;
			trap_R_AddRefEntityToScene( ent );
		}
		else {*/
			trap_R_AddRefEntityToScene( ent );
		//}

		if ( state->powerups & ( 1 << PW_QUAD ) )
		{
			if (team == TEAM_RED)
				ent->customShader = cgs.media.redQuadShader;
			else
				ent->customShader = cgs.media.quadShader;
			trap_R_AddRefEntityToScene( ent );
		}
		if ( state->powerups & ( 1 << PW_BERSERK ) )
		{
			ent->customShader = cgs.media.berserkShader;
			trap_R_AddRefEntityToScene( ent );
		}
		if ( state->powerups & ( 1 << PW_REGEN ) ) {
			if ( ( ( cg.time / 100 ) % 10 ) == 1 ) {
				ent->customShader = cgs.media.regenShader;
				trap_R_AddRefEntityToScene( ent );
			}
		}
		if ( state->powerups & ( 1 << PW_BATTLESUIT ) ) {
			ent->customShader = cgs.media.battleSuitShader;
			trap_R_AddRefEntityToScene( ent );
		}
		if ( state->eFlags & EF_SPAWN_PROTECT ) {
			ent->customShader = cgs.media.spawnProtectShader;
			trap_R_AddRefEntityToScene( ent );
		}
#if FEAT_FREEZETAG
		// freezetag (7A): blue-white ice shell on frozen players
		if ( state->eFlags & EF_FROZEN ) {
			ent->customShader = cgs.media.spawnProtectShader;
			ent->shaderRGBA[0] = 0x80; ent->shaderRGBA[1] = 0xc0; ent->shaderRGBA[2] = 0xff;
			ent->shaderRGBA[3] = 0xc0;
			trap_R_AddRefEntityToScene( ent );
		}
#endif
#if FEAT_SPECTATOR_OUTLINES
		// spectator outlines (8A): colored shell visible through walls
		if ( cg.snap->ps.persistant[PERS_TEAM] == TEAM_SPECTATOR
			 && cg_specOutlines.integer ) {
			ent->renderfx |= RF_DEPTHHACK;
			ent->customShader = cgs.media.spawnProtectShader;
			if ( team == TEAM_RED ) {
				ent->shaderRGBA[0] = 0xff; ent->shaderRGBA[1] = 0x40; ent->shaderRGBA[2] = 0x40;
			} else if ( team == TEAM_BLUE ) {
				ent->shaderRGBA[0] = 0x40; ent->shaderRGBA[1] = 0x40; ent->shaderRGBA[2] = 0xff;
			} else {
				ent->shaderRGBA[0] = 0x40; ent->shaderRGBA[1] = 0xff; ent->shaderRGBA[2] = 0x40;
			}
			ent->shaderRGBA[3] = 0x80;
			trap_R_AddRefEntityToScene( ent );
			ent->renderfx &= ~RF_DEPTHHACK;
		}
#endif
	}
}

/*
=================
CG_LightVerts
=================
*/
int CG_LightVerts( vec3_t normal, int numVerts, polyVert_t *verts )
{
	int				i, j;
	float			incoming;
	vec3_t			ambientLight;
	vec3_t			lightDir;
	vec3_t			directedLight;

	trap_R_LightForPoint( verts[0].xyz, ambientLight, directedLight, lightDir );

	for (i = 0; i < numVerts; i++) {
		incoming = DotProduct (normal, lightDir);
		if ( incoming <= 0 ) {
			verts[i].modulate.rgba[0] = ambientLight[0];
			verts[i].modulate.rgba[1] = ambientLight[1];
			verts[i].modulate.rgba[2] = ambientLight[2];
			verts[i].modulate.rgba[3] = 255;
			continue;
		}
		j = ( ambientLight[0] + incoming * directedLight[0] );
		if ( j > 255 ) {
			j = 255;
		}
		verts[i].modulate.rgba[0] = j;

		j = ( ambientLight[1] + incoming * directedLight[1] );
		if ( j > 255 ) {
			j = 255;
		}
		verts[i].modulate.rgba[1] = j;

		j = ( ambientLight[2] + incoming * directedLight[2] );
		if ( j > 255 ) {
			j = 255;
		}
		verts[i].modulate.rgba[2] = j;

		verts[i].modulate.rgba[3] = 255;
	}
	return qtrue;
}

/*
===============
CG_Player
===============
*/
void CG_Player( centity_t *cent ) {
	clientInfo_t	*ci;
	refEntity_t		legs;
	refEntity_t		torso;
	refEntity_t		head;
	int				clientNum;
	int				renderfx;
#if FEAT_THIRD_PERSON
	int				thirdPersonAlpha = 255;
#endif
	qboolean		shadow;
	float			shadowPlane;
	refEntity_t		skull;
	refEntity_t		powerup;
	int				t;
	float			c;
	float			angle;
	vec3_t			dir, angles;

	// the client number is stored in clientNum.  It can't be derived
	// from the entity number, because a single client may have
	// multiple corpses on the level using the same clientinfo
	clientNum = cent->currentState.clientNum;
	if ( clientNum < 0 || clientNum >= MAX_CLIENTS ) {
		Com_Terminate( TERM_CLIENT_DROP, "Bad clientNum on player entity");
	}
	ci = &cgs.clientinfo[ clientNum ];

	// it is possible to see corpses from disconnected players that may
	// not have valid clientinfo
	if ( !ci->infoValid ) {
		return;
	}

#if FEAT_ZNUDGE
	// znudge: extrapolate other players forward by ping
	if ( cg_znudge.integer && clientNum != cg.clientNum ) {
		float nudge = ZN_GetNudge();
		if ( nudge > 0.0f ) {
			ZN_PredictPlayer( cent, nudge, cent->lerpOrigin );
		}
	}
#endif

	// get the player model information
	renderfx = 0;
	if ( cent->currentState.number == cg.snap->ps.clientNum) {
		if (!cg.renderingThirdPerson) {
			renderfx = RF_THIRD_PERSON;			// only draw in mirrors
		} else {
			if (cg_cameraMode.integer) {
				return;
			}
		}
	}

#if FEAT_THIRD_PERSON
	#define TP_ALPHA_FAR    255
	#define TP_ALPHA_CLOSE   10
	if ( cent->currentState.number == cg.snap->ps.clientNum && cg.renderingThirdPerson ) {
		vec3_t diff;
		float dist, fadeStart, fadeEnd, frac;

		VectorSubtract( cent->lerpOrigin, cg.refdef.vieworg, diff );
		dist = VectorLength( diff );

		fadeStart = cg_thirdPersonFadeStart.value;
		fadeEnd   = cg_thirdPersonFadeEnd.value;
		if ( fadeStart <= fadeEnd ) {
			fadeStart = fadeEnd + 1.0f;
		}

		if ( dist >= fadeStart ) {
			thirdPersonAlpha = TP_ALPHA_FAR;
		} else if ( dist <= fadeEnd ) {
			thirdPersonAlpha = TP_ALPHA_CLOSE;
		} else {
			frac = (dist - fadeEnd) / (fadeStart - fadeEnd);
			thirdPersonAlpha = TP_ALPHA_CLOSE + (int)((TP_ALPHA_FAR - TP_ALPHA_CLOSE) * frac);
		}

		if ( cg_thirdPersonAlpha.integer >= 0 && cg_thirdPersonAlpha.integer <= 255 ) {
			thirdPersonAlpha = cg_thirdPersonAlpha.integer;
		}
	}
#endif

	memset( &legs, 0, sizeof(legs) );
	memset( &torso, 0, sizeof(torso) );
	memset( &head, 0, sizeof(head) );

	// get the rotation information
	CG_PlayerAngles( cent, legs.axis, torso.axis, head.axis );

	// get the animation state (after rotation, to allow feet shuffle)
	CG_PlayerAnimation( cent, &legs.oldframe, &legs.frame, &legs.backlerp,
		 &torso.oldframe, &torso.frame, &torso.backlerp );

	// add the talk baloon or disconnect icon
	CG_PlayerSprites( cent );

	// add the shadow
	shadow = CG_PlayerShadow( cent, &shadowPlane );

	// add a water splash if partially in and out of water
	CG_PlayerSplash( cent );

	if ( cg_shadows.integer == 3 && shadow ) {
		renderfx |= RF_SHADOW_PLANE;
	}
	renderfx |= RF_LIGHTING_ORIGIN;			// use the same origin for all
#if FEAT_HARVESTER
	if( cgs.gametype == GT_HARVESTER ) {
		CG_PlayerTokens( cent, renderfx );
	}
#endif
#if FEAT_IQM
	//
	// IQM single-mesh player model rendering
	//
	if ( ci->iqmModel ) {
		refEntity_t body;

		memset( &body, 0, sizeof(body) );

		body.hModel = ci->bodyModel;
		if ( ci->bodyShader ) body.customShader = ci->bodyShader;
		else if ( ci->bodySkin ) body.customSkin = ci->bodySkin;

		VectorCopy( cent->lerpOrigin, body.origin );
		VectorCopy( cent->lerpOrigin, body.lightingOrigin );
		body.shadowPlane = shadowPlane;
		body.renderfx = renderfx;
		VectorCopy( body.origin, body.oldorigin );

		// use legs animation frames for the single-mesh model
		body.frame = legs.frame;
		body.oldframe = legs.oldframe;
		body.backlerp = legs.backlerp;

		// copy rotation from legs (full-body orientation)
		AxisCopy( legs.axis, body.axis );

#if FEAT_THIRD_PERSON
#if FEAT_FORCE_ENTITY_VERTEX_ALPHA
		if ( thirdPersonAlpha < 255 ) {
			body.renderfx |= RF_FORCE_ENT_ALPHA;
			body.shaderRGBA[3] = thirdPersonAlpha;
		}
#endif
#endif

		CG_AddRefEntityWithPowerups( cent, &body, &cent->currentState, qtrue, ci->team );

		if ( !body.hModel ) {
			return;
		}

		// attach weapon to tag_weapon joint in the IQM model
		CG_AddPlayerWeapon( &body, NULL, cent, ci->team );

		// add powerups floating behind the player
		CG_PlayerPowerups( cent, &body );
		return;
	}
#endif // FEAT_IQM

	//
	// add the legs
	//
	legs.hModel = ci->legsModel;
	legs.characterSkin = ci->skinHandle;
	if ( !ci->skinHandle && ci->legsSkin ) legs.customSkin = ci->legsSkin;

	VectorCopy( cent->lerpOrigin, legs.origin );

	VectorCopy( cent->lerpOrigin, legs.lightingOrigin );
	legs.shadowPlane = shadowPlane;
	legs.renderfx = renderfx;
	VectorCopy (legs.origin, legs.oldorigin);	// don't positionally lerp at all

#if FEAT_THIRD_PERSON
#if FEAT_FORCE_ENTITY_VERTEX_ALPHA
	if ( thirdPersonAlpha < 255 ) {
		legs.renderfx |= RF_FORCE_ENT_ALPHA;
		legs.shaderRGBA[3] = thirdPersonAlpha;
	}
#endif
#endif
	{
		static qboolean legs_printed = qfalse;
		if (cg_debugCharacterSkin.integer && !legs_printed) { legs_printed = qtrue;
		Com_Log( SEV_INFO, LOG_CH(ch_cgame), "[ENT-legs] characterSkin=%d shaderRGBA=%d,%d,%d,%d renderfx=%d customShader=%d\n",
			legs.characterSkin, legs.shaderRGBA[0], legs.shaderRGBA[1], legs.shaderRGBA[2], legs.shaderRGBA[3],
			legs.renderfx, legs.customShader); }
	}
	CG_AddRefEntityWithPowerups( cent, &legs, &cent->currentState, qtrue, ci->team );

	// if the model failed, allow the default nullmodel to be displayed
	if (!legs.hModel) {
		return;
	}

	//
	// add the torso
	//
	torso.hModel = ci->torsoModel;
	if (!torso.hModel) {
		return;
	}

	torso.customSkin = 0;
	torso.characterSkin = ci->skinHandle;
	if ( !ci->skinHandle && ci->torsoSkin ) torso.customSkin = ci->torsoSkin;

	VectorCopy( cent->lerpOrigin, torso.lightingOrigin );

	CG_PositionRotatedEntityOnTag( &torso, &legs, ci->legsModel, "tag_torso");

	torso.shadowPlane = shadowPlane;
	torso.renderfx = renderfx;

#if FEAT_THIRD_PERSON
#if FEAT_FORCE_ENTITY_VERTEX_ALPHA
	if ( thirdPersonAlpha < 255 ) {
		torso.renderfx |= RF_FORCE_ENT_ALPHA;
		torso.shaderRGBA[3] = thirdPersonAlpha;
	}
#endif
#endif
	{
		static qboolean torso_printed = qfalse;
		if (cg_debugCharacterSkin.integer && !torso_printed) { torso_printed = qtrue;
		Com_Log( SEV_INFO, LOG_CH(ch_cgame), "[ENT-torso] characterSkin=%d shaderRGBA=%d,%d,%d,%d renderfx=%d customShader=%d\n",
			torso.characterSkin, torso.shaderRGBA[0], torso.shaderRGBA[1], torso.shaderRGBA[2], torso.shaderRGBA[3],
			torso.renderfx, torso.customShader); }
	}
	CG_AddRefEntityWithPowerups( cent, &torso, &cent->currentState, qtrue, ci->team );

	if ( cent->currentState.eFlags & EF_KAMIKAZE ) {

		memset( &skull, 0, sizeof(skull) );

		VectorCopy( cent->lerpOrigin, skull.lightingOrigin );
		skull.shadowPlane = shadowPlane;
		skull.renderfx = renderfx;

		if ( cent->currentState.eFlags & EF_DEAD ) {
			// one skull bobbing above the dead body
			angle = ((cg.time / 7) & 255) * (M_PI * 2) / 255;
			if (angle > M_PI * 2)
				angle -= (float)M_PI * 2;
			dir[0] = sin(angle) * 20;
			dir[1] = cos(angle) * 20;
			angle = ((cg.time / 4) & 255) * (M_PI * 2) / 255;
			dir[2] = 15 + sin(angle) * 8;
			VectorAdd(torso.origin, dir, skull.origin);

			dir[2] = 0;
			VectorCopy(dir, skull.axis[1]);
			VectorNormalize(skull.axis[1]);
			VectorSet(skull.axis[2], 0, 0, 1);
			CrossProduct(skull.axis[1], skull.axis[2], skull.axis[0]);

			skull.hModel = cgs.media.kamikazeHeadModel;
			trap_R_AddRefEntityToScene( &skull );
			skull.hModel = cgs.media.kamikazeHeadTrail;
			trap_R_AddRefEntityToScene( &skull );
		}
		else {
			// three skulls spinning around the player
			angle = ((cg.time / 4) & 255) * (M_PI * 2) / 255;
			dir[0] = cos(angle) * 20;
			dir[1] = sin(angle) * 20;
			dir[2] = cos(angle) * 20;
			VectorAdd(torso.origin, dir, skull.origin);

			angles[0] = sin(angle) * 30;
			angles[1] = (angle * 180 / M_PI) + 90;
			if (angles[1] > 360)
				angles[1] -= 360;
			angles[2] = 0;
			AnglesToAxis( angles, skull.axis );

			/*
			dir[2] = 0;
			VectorInverse(dir);
			VectorCopy(dir, skull.axis[1]);
			VectorNormalize(skull.axis[1]);
			VectorSet(skull.axis[2], 0, 0, 1);
			CrossProduct(skull.axis[1], skull.axis[2], skull.axis[0]);
			*/

			skull.hModel = cgs.media.kamikazeHeadModel;
			trap_R_AddRefEntityToScene( &skull );
			// flip the trail because this skull is spinning in the other direction
			VectorInverse(skull.axis[1]);
			skull.hModel = cgs.media.kamikazeHeadTrail;
			trap_R_AddRefEntityToScene( &skull );

			angle = ((cg.time / 4) & 255) * (M_PI * 2) / 255 + M_PI;
			if (angle > M_PI * 2)
				angle -= (float)M_PI * 2;
			dir[0] = sin(angle) * 20;
			dir[1] = cos(angle) * 20;
			dir[2] = cos(angle) * 20;
			VectorAdd(torso.origin, dir, skull.origin);

			angles[0] = cos(angle - 0.5 * M_PI) * 30;
			angles[1] = 360 - (angle * 180 / M_PI);
			if (angles[1] > 360)
				angles[1] -= 360;
			angles[2] = 0;
			AnglesToAxis( angles, skull.axis );

			/*
			dir[2] = 0;
			VectorCopy(dir, skull.axis[1]);
			VectorNormalize(skull.axis[1]);
			VectorSet(skull.axis[2], 0, 0, 1);
			CrossProduct(skull.axis[1], skull.axis[2], skull.axis[0]);
			*/

			skull.hModel = cgs.media.kamikazeHeadModel;
			trap_R_AddRefEntityToScene( &skull );
			skull.hModel = cgs.media.kamikazeHeadTrail;
			trap_R_AddRefEntityToScene( &skull );

			angle = ((cg.time / 3) & 255) * (M_PI * 2) / 255 + 0.5 * M_PI;
			if (angle > M_PI * 2)
				angle -= (float)M_PI * 2;
			dir[0] = sin(angle) * 20;
			dir[1] = cos(angle) * 20;
			dir[2] = 0;
			VectorAdd(torso.origin, dir, skull.origin);

			VectorCopy(dir, skull.axis[1]);
			VectorNormalize(skull.axis[1]);
			VectorSet(skull.axis[2], 0, 0, 1);
			CrossProduct(skull.axis[1], skull.axis[2], skull.axis[0]);

			skull.hModel = cgs.media.kamikazeHeadModel;
			trap_R_AddRefEntityToScene( &skull );
			skull.hModel = cgs.media.kamikazeHeadTrail;
			trap_R_AddRefEntityToScene( &skull );
		}
	}

	if ( cent->currentState.powerups & ( 1 << PW_DEFLECTOR ) ) {
		if ( !ci->deflectorStartTime ) {
			ci->deflectorStartTime = cg.time;
		}
		ci->deflectorStopTime = cg.time;
	}
	else {
		ci->deflectorStartTime = 0;
	}

	if ( (cent->currentState.powerups & ( 1 << PW_DEFLECTOR ) ) ||
		cg.time - ci->deflectorStopTime < 250 ) {

		memcpy(&powerup, &torso, sizeof(torso));
		powerup.hModel = cgs.media.deflectorPowerupModel;
		powerup.frame = 0;
		powerup.oldframe = 0;
		powerup.customSkin = 0;
		powerup.characterSkin = 0;
		// always draw
		powerup.renderfx &= ~RF_THIRD_PERSON;
		VectorCopy(cent->lerpOrigin, powerup.origin);

		if ( cg.time - ci->deflectorStartTime < 250 ) {
			c = (float) (cg.time - ci->deflectorStartTime) / 250;
		}
		else if (cg.time - ci->deflectorStopTime < 250 ) {
			c = (float) (250 - (cg.time - ci->deflectorStopTime)) / 250;
		}
		else {
			c = 1;
		}
		VectorSet( powerup.axis[0], c, 0, 0 );
		VectorSet( powerup.axis[1], 0, c, 0 );
		VectorSet( powerup.axis[2], 0, 0, c );
		trap_R_AddRefEntityToScene( &powerup );
	}

	t = cg.time - ci->medkitUsageTime;
	if ( ci->medkitUsageTime && t < 500 ) {
		memcpy(&powerup, &torso, sizeof(torso));
		powerup.hModel = cgs.media.medkitUsageModel;
		powerup.frame = 0;
		powerup.oldframe = 0;
		powerup.customSkin = 0;
		powerup.customShader = 0;
		powerup.characterSkin = 0;
		// always draw
		powerup.renderfx &= ~RF_THIRD_PERSON;
		VectorClear(angles);
		AnglesToAxis(angles, powerup.axis);
		VectorCopy(cent->lerpOrigin, powerup.origin);
		powerup.origin[2] += -24 + (float) t * 80 / 500;
		if ( t > 400 ) {
			c = (float) (t - 400) * 0xff / 100;
			powerup.shaderRGBA[0] = 0xff - c;
			powerup.shaderRGBA[1] = 0xff - c;
			powerup.shaderRGBA[2] = 0xff - c;
			powerup.shaderRGBA[3] = 0xff - c;
		}
		else {
			powerup.shaderRGBA[0] = 0xff;
			powerup.shaderRGBA[1] = 0xff;
			powerup.shaderRGBA[2] = 0xff;
			powerup.shaderRGBA[3] = 0xff;
		}
		trap_R_AddRefEntityToScene( &powerup );
	}

	//
	// add the head
	//
	head.hModel = ci->headModel;
	if (!head.hModel) {
		return;
	}
	head.customSkin = 0;
	head.characterSkin = ci->skinHandle;
	if ( !ci->skinHandle && ci->headSkin ) head.customSkin = ci->headSkin;

	VectorCopy( cent->lerpOrigin, head.lightingOrigin );

	CG_PositionRotatedEntityOnTag( &head, &torso, ci->torsoModel, "tag_head");

	head.shadowPlane = shadowPlane;
	head.renderfx = renderfx;

#if FEAT_THIRD_PERSON
#if FEAT_FORCE_ENTITY_VERTEX_ALPHA
	if ( thirdPersonAlpha < 255 ) {
		head.renderfx |= RF_FORCE_ENT_ALPHA;
		head.shaderRGBA[3] = thirdPersonAlpha;
	}
#endif
#endif
	{
		static qboolean head_printed = qfalse;
		if (cg_debugCharacterSkin.integer && !head_printed) { head_printed = qtrue;
		Com_Log( SEV_INFO, LOG_CH(ch_cgame), "[ENT-head] characterSkin=%d shaderRGBA=%d,%d,%d,%d renderfx=%d customShader=%d\n",
			head.characterSkin, head.shaderRGBA[0], head.shaderRGBA[1], head.shaderRGBA[2], head.shaderRGBA[3],
			head.renderfx, head.customShader); }
	}
	CG_AddRefEntityWithPowerups( cent, &head, &cent->currentState, qtrue, ci->team );

	CG_BreathPuffs(cent, &head);
	CG_BubblePuffs(cent, &head);

	CG_DustTrail(cent);

	//
	// add the gun / barrel / flash
	//
	CG_AddPlayerWeapon( &torso, NULL, cent, ci->team );

	// add powerups floating behind the player
	CG_PlayerPowerups( cent, &torso );

	// gauntlet charge glow (local player only)
	if ( cent->currentState.number == cg.snap->ps.clientNum
		&& cg.predictedPlayerState.chargeStartTime > 0
		&& cg.snap->ps.weapon == WP_GAUNTLET ) {
		float chargeProgress = (float)( cg.time - cg.predictedPlayerState.chargeStartTime ) / GAUNTLET_CHARGE_TIME;
		if ( chargeProgress > 1.0f ) chargeProgress = 1.0f;
		if ( chargeProgress < 0.0f ) chargeProgress = 0.0f;
		// growing orange-red glow during charge
		trap_R_AddLightToScene( cent->lerpOrigin,
			100 + (int)( chargeProgress * 150 ),
			1.0f, 0.6f - chargeProgress * 0.4f, 0.0f );
	}
}


//=====================================================================

/*
===============
CG_ResetPlayerEntity

A player just came into view or teleported, so reset all animation info
===============
*/
void CG_ResetPlayerEntity( centity_t *cent ) {
	cent->errorTime = -99999;		// guarantee no error decay added
	cent->extrapolated = qfalse;

	CG_ClearLerpFrame( &cgs.clientinfo[ cent->currentState.clientNum ], &cent->pe.legs, cent->currentState.legsAnim );
	CG_ClearLerpFrame( &cgs.clientinfo[ cent->currentState.clientNum ], &cent->pe.torso, cent->currentState.torsoAnim );

	BG_EvaluateTrajectory( &cent->currentState.pos, cg.time, cent->lerpOrigin );
	BG_EvaluateTrajectory( &cent->currentState.apos, cg.time, cent->lerpAngles );

	VectorCopy( cent->lerpOrigin, cent->rawOrigin );
	VectorCopy( cent->lerpAngles, cent->rawAngles );

	memset( &cent->pe.legs, 0, sizeof( cent->pe.legs ) );
	cent->pe.legs.yawAngle = cent->rawAngles[YAW];
	cent->pe.legs.yawing = qfalse;
	cent->pe.legs.pitchAngle = 0;
	cent->pe.legs.pitching = qfalse;

	memset( &cent->pe.torso, 0, sizeof( cent->pe.torso ) );
	cent->pe.torso.yawAngle = cent->rawAngles[YAW];
	cent->pe.torso.yawing = qfalse;
	cent->pe.torso.pitchAngle = cent->rawAngles[PITCH];
	cent->pe.torso.pitching = qfalse;

	if ( cg_debugPosition.integer ) {
		Com_Log( SEV_INFO, LOG_CH(ch_cgame), "%i ResetPlayerEntity yaw=%f\n", cent->currentState.number, cent->pe.torso.yawAngle );
	}
}
