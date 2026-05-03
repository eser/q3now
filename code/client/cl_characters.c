/*
===========================================================================
cl_characters.c — Engine-side character manifest loading.

Called from CL_GetValue() with key "char:{name}".
Three-layer merge: characters/_archetypes/_base/main.lua →
  characters/_archetypes/{archetype}/main.lua → characters/{name}/main.lua.
Animations follow the same three-layer chain.

Merge rule: top-level container keys (model, sounds, stats, bot) are
merged field-by-field. Within bot, the sub-containers traits/aim/chats
are also merged field-by-field. Everything else (scalars, lists) is
replaced wholesale. Animations are merged wholesale per named entry.

No trap syscall is added — this extends the existing CG_TRAP_GETVALUE path.
===========================================================================
*/

#include "client.h"
#include "../qcommon/wired/core/scripting/wired_scripting.h"

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

// ── Canonical sound slot names (index matches CSOUND_* defines in cg_public.h) ──

static const char *s_soundSlotName[CM_SOUND_SLOTS] = {
	"death1",   // 0: *death1.opus
	"death2",   // 1: *death2.opus
	"death3",   // 2: *death3.opus
	"jump",     // 3: *jump1.opus
	"pain25",   // 4: *pain25_1.opus
	"pain50",   // 5: *pain50_1.opus
	"pain75",   // 6: *pain75_1.opus
	"pain100",  // 7: *pain100_1.opus
	"falling",  // 8: *falling1.opus
	"gasp",     // 9: *gasp.opus
	"drown",    // 10: *drown.opus
	"fall",     // 11: *fall1.opus
	"taunt",    // 12: *taunt.opus
};

// ── Character skin registry ───────────────────────────────────────────────
// Stores both renderer-facing handles (cmSkin_t) and raw paths needed for
// re-registration after vid_restart.  Path strings are kept so that
// CL_Characters_RegisterShaders() can call re.RegisterShader() fresh each time.

#define MAX_CHARACTER_SKINS 1024

typedef struct {
	// Renderer-facing skin (handles resolved after renderer init).
	cmSkin_t skin;
	// Path strings for re-registration.
	char fallbackPath[MAX_QPATH];
	struct {
		char path[MAX_QPATH];
	} overridePaths[CM_MAX_SURFACE_OVERRIDES];
} clSkinEntry_t;

// Engine-internal parsed skin (path strings only, pre-registration).
typedef struct {
	char name[CM_SKIN_NAME_LEN];
	int  paintable;
	int  singlePath;
	char fallback[MAX_QPATH];
	int  overrideCount;
	struct {
		char surface[CM_SURFACE_NAME_LEN];
		char path[MAX_QPATH];
	} overrides[CM_MAX_SURFACE_OVERRIDES];
} clParsedSkin_t;

static clSkinEntry_t s_characterSkins[MAX_CHARACTER_SKINS];
static int           s_characterSkinCount;

// ── Animation name → enum index mapping ──────────────────────────────────

static const struct { const char *name; int idx; } s_animNames[] = {
	{ "BOTH_DEATH1",       BOTH_DEATH1 },
	{ "BOTH_DEAD1",        BOTH_DEAD1 },
	{ "BOTH_DEATH2",       BOTH_DEATH2 },
	{ "BOTH_DEAD2",        BOTH_DEAD2 },
	{ "BOTH_DEATH3",       BOTH_DEATH3 },
	{ "BOTH_DEAD3",        BOTH_DEAD3 },
	{ "TORSO_GESTURE",     TORSO_GESTURE },
	{ "TORSO_ATTACK",      TORSO_ATTACK },
	{ "TORSO_ATTACK2",     TORSO_ATTACK2 },
	{ "TORSO_DROP",        TORSO_DROP },
	{ "TORSO_RAISE",       TORSO_RAISE },
	{ "TORSO_STAND",       TORSO_STAND },
	{ "TORSO_STAND2",      TORSO_STAND2 },
	{ "LEGS_WALKCR",       LEGS_WALKCR },
	{ "LEGS_WALK",         LEGS_WALK },
	{ "LEGS_RUN",          LEGS_RUN },
	{ "LEGS_BACK",         LEGS_BACK },
	{ "LEGS_SWIM",         LEGS_SWIM },
	{ "LEGS_JUMP",         LEGS_JUMP },
	{ "LEGS_LAND",         LEGS_LAND },
	{ "LEGS_JUMPB",        LEGS_JUMPB },
	{ "LEGS_LANDB",        LEGS_LANDB },
	{ "LEGS_IDLE",         LEGS_IDLE },
	{ "LEGS_IDLECR",       LEGS_IDLECR },
	{ "LEGS_TURN",         LEGS_TURN },
	{ "TORSO_GETFLAG",     TORSO_GETFLAG },
	{ "TORSO_GUARDBASE",   TORSO_GUARDBASE },
	{ "TORSO_PATROL",      TORSO_PATROL },
	{ "TORSO_FOLLOWME",    TORSO_FOLLOWME },
	{ "TORSO_AFFIRMATIVE", TORSO_AFFIRMATIVE },
	{ "TORSO_NEGATIVE",    TORSO_NEGATIVE },
};
static const int s_numAnimNames = (int)(sizeof(s_animNames) / sizeof(s_animNames[0]));

// ── Known manifest keys (for typo warnings) ──────────────────────────────

static const char *s_knownTopKeys[] = {
	"name", "display_name", "nicknames", "bio", "role", "archetype",
	"model", "sounds", "stats", NULL
};
static const char *s_knownModelKeys[] = {
	"parts", "icon", "headoffset", "skins", NULL
};
static const char *s_knownSoundsKeys[] = {
	"footsteps",
	"death1", "death2", "death3", "jump",
	"pain25", "pain50", "pain75", "pain100",
	"falling", "gasp", "drown", "fall", "taunt", NULL
};
static const char *s_knownStatsKeys[] = {
	"health", "speed", NULL
};

// ── Lua helpers ───────────────────────────────────────────────────────────

/* Load a .lua file via the game VFS, execute it, and push its return value
   (expected to be a table) onto the Lua stack.
   Returns qtrue on success (table is on top of stack). */
static qboolean CL_Char_LoadLuaTable( lua_State *L, const char *path ) {
	fileHandle_t f;
	int len;
	char *buf;
	char chunkName[MAX_QPATH + 1];
	int status;

	len = FS_FOpenFileRead( path, &f, qfalse );
	if ( len <= 0 ) {
		return qfalse;
	}

	buf = Z_Malloc( len + 1 );
	FS_Read( buf, len, f );
	buf[len] = '\0';
	FS_FCloseFile( f );

	Com_sprintf( chunkName, sizeof( chunkName ), "@%s", path );
	status = luaL_loadbuffer( L, buf, len, chunkName );
	Z_Free( buf );

	if ( status != 0 ) {
		COM_ERROR( LOG_CAT_CLIENT, "CL_Characters: Lua load error (%s): %s\n",
			path, lua_tostring( L, -1 ) );
		lua_pop( L, 1 );
		return qfalse;
	}

	// Execute with 0 args, expect 1 return value.
	status = lua_pcall( L, 0, 1, 0 );
	if ( status != 0 ) {
		COM_ERROR( LOG_CAT_CLIENT, "CL_Characters: Lua exec error (%s): %s\n",
			path, lua_tostring( L, -1 ) );
		lua_pop( L, 1 );
		return qfalse;
	}

	if ( !lua_istable( L, -1 ) ) {
		COM_ERROR( LOG_CAT_CLIENT, "CL_Characters: '%s' did not return a table\n", path );
		lua_pop( L, 1 );
		return qfalse;
	}

	return qtrue;
}

// ── Merge container key lists ─────────────────────────────────────────────

/* Top-level manifest keys treated as containers (merged field-by-field). */
static const char *s_topContainers[] = { "model", "sounds", "stats", "bot", NULL };

/* Second-level containers inside the "bot" container. */
static const char *s_botSubContainers[] = { "traits", "aim", "chats", NULL };

/* Returns qtrue if key (string at key_abs on the Lua stack) is in list. */
static qboolean CL_Char_KeyInList( lua_State *L, int key_abs, const char **list ) {
	if ( lua_type( L, key_abs ) != LUA_TSTRING ) return qfalse;
	const char *k = lua_tostring( L, key_abs );
	for ( int i = 0; list[i]; i++ ) {
		if ( !Q_stricmp( k, list[i] ) ) return qtrue;
	}
	return qfalse;
}

/* Deep-copy src into dst, copying sub-tables recursively down to depth levels.
   depth=2: copies level-1 sub-tables AND level-2 sub-tables as fresh objects,
   so in-place merges at either level cannot affect the original. */
static void CL_Char_DeepCopy( lua_State *L, int dst, int src, int depth ) {
	lua_pushnil( L );
	while ( lua_next( L, src ) != 0 ) {
		int key = lua_gettop( L ) - 1;
		int val = lua_gettop( L );
		if ( depth > 0 && lua_istable( L, val ) ) {
			lua_newtable( L );
			int sub = lua_gettop( L );
			CL_Char_DeepCopy( L, sub, val, depth - 1 );
			lua_pushvalue( L, key );
			lua_pushvalue( L, sub );
			lua_rawset( L, dst );
			lua_pop( L, 2 );  // pop sub and val
		} else {
			lua_pushvalue( L, key );
			lua_pushvalue( L, val );
			lua_rawset( L, dst );
			lua_pop( L, 1 );  // pop val; key stays for lua_next
		}
	}
}

/* Leaf merge: for every key K in src, assign dst[K] = src[K] wholesale.
   No recursion. Used for terminal containers and animation merging. */
static void CL_Char_MergeLeaf( lua_State *L, int dst, int src ) {
	lua_pushnil( L );
	while ( lua_next( L, src ) != 0 ) {
		int key = lua_gettop( L ) - 1;
		int val = lua_gettop( L );
		lua_pushvalue( L, key );
		lua_pushvalue( L, val );
		lua_rawset( L, dst );
		lua_pop( L, 1 );  // pop val; key stays for lua_next
	}
}

/* Sub-table merge: for every key K in src:
   - If K is in sub_containers AND dst[K] and src[K] are both tables:
       merge src[K] field-by-field into dst[K] (CL_Char_MergeLeaf).
   - Otherwise: dst[K] = src[K] wholesale.
   sub_containers may be NULL (all keys treated as wholesale). */
static void CL_Char_MergeSubTable( lua_State *L, int dst, int src, const char **sub_containers ) {
	lua_pushnil( L );
	while ( lua_next( L, src ) != 0 ) {
		int key = lua_gettop( L ) - 1;
		int val = lua_gettop( L );
		qboolean merged = qfalse;
		if ( sub_containers && lua_istable( L, val ) && CL_Char_KeyInList( L, key, sub_containers ) ) {
			lua_pushvalue( L, key );
			lua_rawget( L, dst );
			if ( lua_istable( L, -1 ) ) {
				CL_Char_MergeLeaf( L, lua_gettop( L ), val );
				merged = qtrue;
			}
			lua_pop( L, 1 );  // pop dst[key]
		}
		if ( !merged ) {
			lua_pushvalue( L, key );
			lua_pushvalue( L, val );
			lua_rawset( L, dst );
		}
		lua_pop( L, 1 );  // pop val; key stays for lua_next
	}
}

/* Two-level manifest merge: for every key K in src:
   - If K is a top container (model/sounds/stats/bot) AND both values are tables:
       merge src[K] into dst[K] field-by-field, with bot's sub-containers
       (traits/aim/chats) receiving one further level of field-by-field merge.
   - Otherwise: dst[K] = src[K] wholesale. */
static void CL_Char_MergeManifests( lua_State *L, int dst, int src ) {
	lua_pushnil( L );
	while ( lua_next( L, src ) != 0 ) {
		int key = lua_gettop( L ) - 1;
		int val = lua_gettop( L );
		qboolean merged = qfalse;
		if ( lua_istable( L, val ) && CL_Char_KeyInList( L, key, s_topContainers ) ) {
			lua_pushvalue( L, key );
			lua_rawget( L, dst );
			if ( lua_istable( L, -1 ) ) {
				int dst_sub = lua_gettop( L );
				qboolean is_bot = ( lua_type( L, key ) == LUA_TSTRING &&
					!Q_stricmp( lua_tostring( L, key ), "bot" ) );
				CL_Char_MergeSubTable( L, dst_sub, val,
					is_bot ? s_botSubContainers : NULL );
				merged = qtrue;
			}
			lua_pop( L, 1 );  // pop dst[key]
		}
		if ( !merged ) {
			lua_pushvalue( L, key );
			lua_pushvalue( L, val );
			lua_rawset( L, dst );
		}
		lua_pop( L, 1 );  // pop val; key stays for lua_next
	}
}

/* Warn about any key in the given table that is not in the null-terminated
   known list.  Prefixes the warning with context for user-facing output. */
static void CL_Char_WarnUnknownKeys( lua_State *L, int tbl_idx, const char **known,
	const char *charName, const char *context ) {
	lua_pushnil( L );
	while ( lua_next( L, tbl_idx ) != 0 ) {
		if ( lua_type( L, -2 ) == LUA_TSTRING ) {
			const char *k = lua_tostring( L, -2 );
			qboolean found = qfalse;
			for ( int i = 0; known[i]; i++ ) {
				if ( !Q_stricmp( k, known[i] ) ) { found = qtrue; break; }
			}
			if ( !found ) {
				COM_WARN( LOG_CAT_CLIENT,
					"CL_Characters: '%s' unknown key '%s' in %s (typo?)\n",
					charName, k, context );
			}
		}
		lua_pop( L, 1 );
	}
}

// ── Skin registration functions ───────────────────────────────────────────

/* Register a parsed skin into the engine registry.
   Resolves shader handles immediately if renderer is up; stores paths for
   re-registration after vid_restart via CL_Characters_RegisterShaders().
   Returns 1-indexed handle; 0 on failure. */
static qhandle_t CL_RegisterCharacterSkin( const clParsedSkin_t *parsed ) {
	clSkinEntry_t *entry;
	int i;

	if ( s_characterSkinCount >= MAX_CHARACTER_SKINS ) {
		COM_WARN( LOG_CAT_CLIENT, "CL_Characters: skin registry full\n" );
		return 0;
	}

	entry = &s_characterSkins[s_characterSkinCount];
	memset( entry, 0, sizeof( *entry ) );

	Q_strncpyz( entry->skin.name, parsed->name, CM_SKIN_NAME_LEN );
	entry->skin.paintable     = parsed->paintable;
	entry->skin.singlePath    = parsed->singlePath;
	entry->skin.overrideCount = parsed->overrideCount;

	if ( parsed->singlePath ) {
		Q_strncpyz( entry->fallbackPath, parsed->fallback, MAX_QPATH );
		if ( re.RegisterShaderLightMap && parsed->fallback[0] )
			entry->skin.fallbackShader = re.RegisterShaderLightMap( parsed->fallback, LIGHTMAP_NONE );
	} else {
		for ( i = 0; i < parsed->overrideCount; i++ ) {
			Q_strncpyz( entry->skin.overrides[i].surfaceName,
				parsed->overrides[i].surface, CM_SURFACE_NAME_LEN );
			Q_strlwr( entry->skin.overrides[i].surfaceName );
			Q_strncpyz( entry->overridePaths[i].path,
				parsed->overrides[i].path, MAX_QPATH );
			if ( re.RegisterShaderLightMap && parsed->overrides[i].path[0] )
				entry->skin.overrides[i].shader =
					re.RegisterShaderLightMap( parsed->overrides[i].path, LIGHTMAP_NONE );
		}
	}

	int handle = ++s_characterSkinCount;
	Com_Log( SEV_DEBUG, LOG_CAT_CLIENT, "[CSK-REG] name=%s handle=%d singlePath=%d fallbackPath=%s fallbackShader=%d overrideCount=%d paintable=%d\n",
		entry->skin.name, handle, entry->skin.singlePath,
		entry->fallbackPath, entry->skin.fallbackShader,
		entry->skin.overrideCount, entry->skin.paintable );
	return handle;
}

/* Re-register shader handles for all skins (called after vid_restart). */
void CL_Characters_RegisterShaders( void ) {
	int i, j;
	for ( i = 0; i < s_characterSkinCount; i++ ) {
		clSkinEntry_t *entry = &s_characterSkins[i];
		if ( entry->skin.singlePath ) {
			if ( entry->fallbackPath[0] )
				entry->skin.fallbackShader = re.RegisterShaderLightMap( entry->fallbackPath, LIGHTMAP_NONE );
			Com_Log( SEV_DEBUG, LOG_CAT_CLIENT, "[CSK-RE] name=%s path=%s -> shader=%d\n",
				entry->skin.name, entry->fallbackPath, entry->skin.fallbackShader );
		} else {
			for ( j = 0; j < entry->skin.overrideCount; j++ ) {
				if ( entry->overridePaths[j].path[0] )
					entry->skin.overrides[j].shader =
						re.RegisterShaderLightMap( entry->overridePaths[j].path, LIGHTMAP_NONE );
			}
		}
	}
}

/* Look up a skin by handle.  Returns NULL for invalid handles. */
const cmSkin_t *CL_GetCharacterSkin( qhandle_t handle ) {
	if ( handle < 1 || handle > s_characterSkinCount )
		return NULL;
	return &s_characterSkins[handle - 1].skin;
}

// ── Skin path resolution ──────────────────────────────────────────────────

/* Resolve a skin path value from the manifest into an absolute VFS path.
   "./foo.tga" → "characters/{charName}/models/foo.tga"
   Anything else is passed through as-is. */
static void CL_Char_ResolveSkinPath( char *out, int outSize,
	const char *charName, const char *val ) {
	if ( val[0] == '.' && val[1] == '/' ) {
		Com_sprintf( out, outSize, "characters/%s/models/%s", charName, val + 2 );
	} else {
		Q_strncpyz( out, val, outSize );
	}
}

/* Parse one entry (key + value on top of Lua stack) from the model.skins table
   into a clParsedSkin_t.  Stack on entry: key at -2, value at -1.
   value may be a string (Case 1/3 via string) or a table (Case 2/3).
   Returns qtrue if a valid skin was parsed. */
static qboolean CL_Char_ParseSkin( lua_State *L, const char *charName,
	const char *skinName, clParsedSkin_t *sk ) {
	memset( sk, 0, sizeof( *sk ) );
	Q_strncpyz( sk->name, skinName, CM_SKIN_NAME_LEN );

	int val = lua_gettop( L );   // value is at top of stack

	if ( lua_isstring( L, val ) ) {
		// Case 1: string → singlePath shader
		sk->singlePath = 1;
		CL_Char_ResolveSkinPath( sk->fallback, sizeof( sk->fallback ),
			charName, lua_tostring( L, val ) );
		return qtrue;
	}

	if ( !lua_istable( L, val ) ) {
		COM_WARN( LOG_CAT_CLIENT,
			"CL_Characters: '%s' skin '%s' value must be a string or table (ignored)\n",
			charName, skinName );
		return qfalse;
	}

	// Table form: read paintable flag and collect surface overrides.
	lua_getfield( L, val, "paintable" );
	sk->paintable = lua_toboolean( L, -1 );
	lua_pop( L, 1 );

	char defaultPath[MAX_QPATH] = { 0 };
	qboolean hasDefault = qfalse;
	int overrideCount = 0;

	lua_pushnil( L );
	while ( lua_next( L, val ) != 0 ) {
		if ( lua_type( L, -2 ) != LUA_TSTRING ) { lua_pop( L, 1 ); continue; }
		const char *k = lua_tostring( L, -2 );
		if ( !Q_stricmp( k, "paintable" ) ) { lua_pop( L, 1 ); continue; }
		if ( !lua_isstring( L, -1 ) )        { lua_pop( L, 1 ); continue; }
		const char *v = lua_tostring( L, -1 );

		if ( !Q_stricmp( k, "default" ) ) {
			hasDefault = qtrue;
			CL_Char_ResolveSkinPath( defaultPath, sizeof( defaultPath ), charName, v );
		} else {
			if ( overrideCount < CM_MAX_SURFACE_OVERRIDES ) {
				Q_strncpyz( sk->overrides[overrideCount].surface, k, CM_SURFACE_NAME_LEN );
				CL_Char_ResolveSkinPath( sk->overrides[overrideCount].path,
					sizeof( sk->overrides[overrideCount].path ), charName, v );
				overrideCount++;
			} else {
				COM_WARN( LOG_CAT_CLIENT,
					"CL_Characters: '%s' skin '%s' exceeds CM_MAX_SURFACE_OVERRIDES=%d; extra surfaces ignored\n",
					charName, skinName, CM_MAX_SURFACE_OVERRIDES );
			}
		}
		lua_pop( L, 1 );
	}

	if ( overrideCount > 0 ) {
		// Case 2: explicit surface overrides
		if ( hasDefault ) {
			COM_WARN( LOG_CAT_CLIENT,
				"CL_Characters: '%s' skin '%s' has both 'default' and explicit surface entries; "
				"'default' ignored — list every surface explicitly\n",
				charName, skinName );
		}
		sk->singlePath = 0;
		sk->overrideCount = overrideCount;
	} else if ( hasDefault ) {
		// Case 3: only a default catch-all → treat as singlePath shader
		sk->singlePath = 1;
		Q_strncpyz( sk->fallback, defaultPath, sizeof( sk->fallback ) );
	} else {
		// Empty table or only paintable flag — no texture, valid but no-op
		sk->singlePath = 1;  // fallback stays empty string; cgame will skip registration
	}
	return qtrue;
}

// ── Animation loading ─────────────────────────────────────────────────────

/* Load the animation data from _archetypes/_base/animations.lua, optionally leaf-merged
   with _archetypes/{archetypeName}/animations.lua and then characters/{charName}/animations.lua
   if they exist, and fill animations[].
   Animation entries (ANIM_NAME → table) are merged wholesale — no sub-table
   recursion needed here since each entry is a flat {first,num,looping,fps} table.
   Three-layer chain:
   - _archetypes/_base/animations.lua      (required)
   - _archetypes/{archetype}/animations.lua (optional)
   - characters/{charName}/animations.lua  (optional)
   Post-processing applied:
   - leg frame offset (LEGS_* adjusted by LEGS_WALKCR - TORSO_GESTURE)
   - TORSO_GETFLAG..TORSO_NEGATIVE filled from TORSO_GESTURE if absent
   - LEGS_BACKCR/LEGS_BACKWALK synthesized from forward anims (reversed=1)
   - FLAG_* set to hardcoded values
*/
static qboolean CL_Char_LoadAnimations( lua_State *L, const char *charName,
	const char *archetypeName, animation_t *animations ) {
	char path[MAX_QPATH];
	qboolean mapped[MAX_ANIMATIONS];
	int i, j;
	int skip;

	if ( !CL_Char_LoadLuaTable( L, "characters/_archetypes/_base/animations.lua" ) ) {
		COM_ERROR( LOG_CAT_CLIENT, "CL_Characters: '%s' failed to load _archetypes/_base/animations.lua\n",
			charName );
		return qfalse;
	}
	int anim_tbl = lua_gettop( L );

	// Optional archetype animation layer (leaf-merge: each named entry replaces wholesale)
	if ( archetypeName && archetypeName[0] ) {
		Com_sprintf( path, sizeof( path ),
			"characters/_archetypes/%s/animations.lua", archetypeName );
		if ( CL_Char_LoadLuaTable( L, path ) ) {
			CL_Char_MergeLeaf( L, anim_tbl, lua_gettop( L ) );
			lua_pop( L, 1 );
		}
	}

	// Optional per-character animation override (leaf-merge: each named entry replaces wholesale)
	Com_sprintf( path, sizeof( path ), "characters/%s/animations.lua", charName );
	if ( CL_Char_LoadLuaTable( L, path ) ) {
		CL_Char_MergeLeaf( L, anim_tbl, lua_gettop( L ) );
		lua_pop( L, 1 );
	}
	memset( animations, 0, sizeof( animation_t ) * MAX_TOTALANIMATIONS );
	memset( mapped, 0, sizeof( mapped ) );

	// Extract named entries for indices 0..MAX_ANIMATIONS-1
	for ( i = 0; i < s_numAnimNames; i++ ) {
		int animIdx = s_animNames[i].idx;
		if ( animIdx >= MAX_ANIMATIONS ) {
			// LEGS_BACKCR, LEGS_BACKWALK, FLAG_* — synthesized below
			continue;
		}

		lua_getfield( L, anim_tbl, s_animNames[i].name );
		if ( !lua_istable( L, -1 ) ) {
			lua_pop( L, 1 );
			continue;
		}

		int entry_tbl = lua_gettop( L );
		lua_getfield( L, entry_tbl, "first" );
		animations[animIdx].firstFrame = lua_tointeger( L, -1 );
		lua_pop( L, 1 );

		lua_getfield( L, entry_tbl, "num" );
		animations[animIdx].numFrames = lua_tointeger( L, -1 );
		lua_pop( L, 1 );

		lua_getfield( L, entry_tbl, "looping" );
		animations[animIdx].loopFrames = lua_tointeger( L, -1 );
		lua_pop( L, 1 );

		lua_getfield( L, entry_tbl, "fps" );
		float fps = (float)lua_tonumber( L, -1 );
		lua_pop( L, 1 );

		if ( fps == 0 ) fps = 1;
		animations[animIdx].frameLerp    = (int)( 1000.0f / fps );
		animations[animIdx].initialLerp  = (int)( 1000.0f / fps );
		animations[animIdx].reversed     = qfalse;
		animations[animIdx].flipflop     = qfalse;

		mapped[animIdx] = qtrue;
		lua_pop( L, 1 );  // pop entry table
	}

	lua_pop( L, 1 );  // pop animation template table

	if ( !mapped[TORSO_GESTURE] || !mapped[LEGS_WALKCR] ) {
		COM_ERROR( LOG_CAT_CLIENT, "CL_Characters: '%s' animations.lua missing required entries\n",
			charName );
		return qfalse;
	}

	// Apply leg frame offset: match CG_ParseAnimationFile behaviour
	skip = animations[LEGS_WALKCR].firstFrame - animations[TORSO_GESTURE].firstFrame;
	for ( i = LEGS_WALKCR; i < TORSO_GETFLAG; i++ ) {
		animations[i].firstFrame -= skip;
	}

	// Fill TORSO_GETFLAG..TORSO_NEGATIVE from TORSO_GESTURE if absent
	for ( i = TORSO_GETFLAG; i <= TORSO_NEGATIVE; i++ ) {
		if ( !mapped[i] ) {
			animations[i].firstFrame   = animations[TORSO_GESTURE].firstFrame;
			animations[i].numFrames    = animations[TORSO_GESTURE].numFrames;
			animations[i].loopFrames   = animations[TORSO_GESTURE].loopFrames;
			animations[i].frameLerp    = animations[TORSO_GESTURE].frameLerp;
			animations[i].initialLerp  = animations[TORSO_GESTURE].initialLerp;
			animations[i].reversed     = qfalse;
			animations[i].flipflop     = qfalse;
		}
	}

	// Synthesize backward animations
	memcpy( &animations[LEGS_BACKCR],   &animations[LEGS_WALKCR], sizeof( animation_t ) );
	animations[LEGS_BACKCR].reversed = qtrue;
	memcpy( &animations[LEGS_BACKWALK], &animations[LEGS_WALK],   sizeof( animation_t ) );
	animations[LEGS_BACKWALK].reversed = qtrue;

	// Hardcoded flag animations (CTF flag carrier)
	animations[FLAG_RUN].firstFrame  = 0;
	animations[FLAG_RUN].numFrames   = 16;
	animations[FLAG_RUN].loopFrames  = 16;
	animations[FLAG_RUN].frameLerp   = 1000 / 15;
	animations[FLAG_RUN].initialLerp = 1000 / 15;
	animations[FLAG_RUN].reversed    = qfalse;

	animations[FLAG_STAND].firstFrame  = 16;
	animations[FLAG_STAND].numFrames   = 5;
	animations[FLAG_STAND].loopFrames  = 0;
	animations[FLAG_STAND].frameLerp   = 1000 / 20;
	animations[FLAG_STAND].initialLerp = 1000 / 20;
	animations[FLAG_STAND].reversed    = qfalse;

	animations[FLAG_STAND2RUN].firstFrame  = 16;
	animations[FLAG_STAND2RUN].numFrames   = 5;
	animations[FLAG_STAND2RUN].loopFrames  = 1;
	animations[FLAG_STAND2RUN].frameLerp   = 1000 / 15;
	animations[FLAG_STAND2RUN].initialLerp = 1000 / 15;
	animations[FLAG_STAND2RUN].reversed    = qtrue;

	return qtrue;
}

// ── Public API ────────────────────────────────────────────────────────────

// ── Archetype registry ─────────────────────────────────────────────────────

#define CL_MAX_ARCHETYPES 32

static char  s_archetypeNames[CL_MAX_ARCHETYPES][64];
static int   s_archetypeCount;

/* Returns qtrue if archetypeName is a known archetype (or empty string for default). */
static qboolean CL_Archetypes_IsValid( const char *name ) {
	int i;
	for ( i = 0; i < s_archetypeCount; i++ ) {
		if ( !Q_stricmp( s_archetypeNames[i], name ) ) return qtrue;
	}
	return qfalse;
}

/* Scan characters/_archetypes/ and populate s_archetypeNames[].
   Skips "_base" (reserved). */
static void CL_Archetypes_Scan( void ) {
	char **dirs;
	int numDirs, i;

	s_archetypeCount = 0;
	dirs = FS_ListDirectories( "characters/_archetypes", &numDirs );
	if ( !dirs || numDirs == 0 ) {
		COM_WARN( LOG_CAT_CLIENT, "CL_Characters: no archetype directories found\n" );
		if ( dirs ) FS_FreeFileList( dirs );
		return;
	}

	for ( i = 0; i < numDirs && s_archetypeCount < CL_MAX_ARCHETYPES; i++ ) {
		if ( !Q_stricmp( dirs[i], "_base" ) ) continue;  // reserved
		Q_strncpyz( s_archetypeNames[s_archetypeCount++], dirs[i],
			sizeof( s_archetypeNames[0] ) );
	}
	FS_FreeFileList( dirs );
	Com_Log( SEV_INFO, LOG_CAT_CLIENT, "CL_Characters: %d archetype(s) found\n", s_archetypeCount );
}

// ── Registry ─────────────────────────────────────────────────────────────────

#define CL_MAX_CHARACTERS 128

static clCharacterEntry_t  s_clCharacters[CL_MAX_CHARACTERS];
static int                 s_clCharacterCount;
static void CL_ReloadCharacters_f( void );  // forward declaration

// Load one character directory into the next free registry slot.
// Uses the provided Lua state (must be valid).  Stack is guarded.
static qboolean CL_Characters_LoadOne( lua_State *L, const char *dirname ) {
	characterManifest_t mf;
	char path[MAX_QPATH];
	clCharacterEntry_t *entry;
	int base;

	if ( s_clCharacterCount >= CL_MAX_CHARACTERS ) {
		COM_WARN( LOG_CAT_CLIENT, "CL_Characters: registry full, skipping '%s'\n", dirname );
		return qfalse;
	}

	base = lua_gettop( L );

	memset( &mf, 0, sizeof( mf ) );

	// ── Step 1: Load _archetypes/_base defaults ─────────────────────────
	if ( !CL_Char_LoadLuaTable( L, "characters/_archetypes/_base/main.lua" ) ) {
		COM_ERROR( LOG_CAT_CLIENT, "CL_Characters: failed to load _archetypes/_base/main.lua\n" );
		lua_settop( L, base );
		return qfalse;
	}
	int defaults_idx = lua_gettop( L );

	// ── Step 2: Load character manifest ──────────────────────────────────
	Com_sprintf( path, sizeof( path ), "characters/%s/main.lua", dirname );
	if ( !CL_Char_LoadLuaTable( L, path ) ) {
		lua_settop( L, base );
		COM_WARN( LOG_CAT_CLIENT, "CL_Characters: no manifest for '%s'\n", dirname );
		return qfalse;
	}
	int char_idx = lua_gettop( L );

	// ── Step 3: Extract and validate archetype field ──────────────────────
	char archetypeBuf[64];
	archetypeBuf[0] = '\0';

	lua_getfield( L, char_idx, "archetype" );
	if ( lua_isstring( L, -1 ) ) {
		const char *s = lua_tostring( L, -1 );
		if ( s && s[0] )
			Q_strncpyz( archetypeBuf, s, sizeof( archetypeBuf ) );
	}
	lua_pop( L, 1 );

	if ( !archetypeBuf[0] ) {
		COM_ERROR( LOG_CAT_CLIENT, "CL_Characters: '%s/main.lua' missing required 'archetype'\n",
			dirname );
		lua_settop( L, base );
		return qfalse;
	}
	const char *archetypeName = archetypeBuf;

	// ── Step 4: Load archetype manifest and merge over _base ──────────────
	char archPath[MAX_QPATH];
	Com_sprintf( archPath, sizeof( archPath ),
		"characters/_archetypes/%s/main.lua", archetypeName );
	int arch_idx = -1;
	if ( CL_Char_LoadLuaTable( L, archPath ) ) {
		arch_idx = lua_gettop( L );
	}

	// ── Step 5: Validate identity field ──────────────────────────────────────
	char mfNameBuf[64];
	const char *mfName = NULL;

	lua_getfield( L, char_idx, "name" );
	if ( lua_isstring( L, -1 ) ) {
		Q_strncpyz( mfNameBuf, lua_tostring( L, -1 ), sizeof( mfNameBuf ) );
		mfName = mfNameBuf;
	} else {
		lua_pop( L, 1 );
		lua_getfield( L, char_idx, "display_name" );
		if ( lua_isstring( L, -1 ) ) {
			Q_strncpyz( mfNameBuf, lua_tostring( L, -1 ), sizeof( mfNameBuf ) );
			Q_CleanStr( mfNameBuf );
			mfName = mfNameBuf;
		} else {
			COM_ERROR( LOG_CAT_CLIENT, "CL_Characters: '%s/main.lua' missing required 'display_name'\n",
				dirname );
			lua_settop( L, base );
			return qfalse;
		}
	}

	if ( Q_stricmp( mfName, dirname ) != 0 ) {
		COM_ERROR( LOG_CAT_CLIENT, "CL_Characters: '%s/main.lua' name '%s' "
			"does not match directory\n", dirname, mfName );
		lua_settop( L, base );
		return qfalse;
	}
	lua_pop( L, 1 );  // pop name or display_name

	// ── Step 4 (was Step 4): Warn on unknown keys ──────────────────────────
	CL_Char_WarnUnknownKeys( L, char_idx, s_knownTopKeys, dirname, "top-level" );

	lua_getfield( L, char_idx, "model" );
	if ( lua_istable( L, -1 ) )
		CL_Char_WarnUnknownKeys( L, lua_gettop( L ), s_knownModelKeys, dirname, "model" );
	lua_pop( L, 1 );

	lua_getfield( L, char_idx, "sounds" );
	if ( lua_istable( L, -1 ) )
		CL_Char_WarnUnknownKeys( L, lua_gettop( L ), s_knownSoundsKeys, dirname, "sounds" );
	lua_pop( L, 1 );

	lua_getfield( L, char_idx, "stats" );
	if ( lua_istable( L, -1 ) )
		CL_Char_WarnUnknownKeys( L, lua_gettop( L ), s_knownStatsKeys, dirname, "stats" );
	lua_pop( L, 1 );

	// ── Step 5: Three-layer merge: _base → archetype → character ──────────
	lua_newtable( L );
	int merged_idx = lua_gettop( L );
	CL_Char_DeepCopy( L, merged_idx, defaults_idx, 2 );
	if ( arch_idx > 0 ) {
		CL_Char_MergeManifests( L, merged_idx, arch_idx );
	}
	CL_Char_MergeManifests( L, merged_idx, char_idx );

	// ── Step 6: Extract identity ──────────────────────────────────────────
	Q_strncpyz( mf.name, dirname, sizeof( mf.name ) );
	Com_sprintf( mf.charRoot, sizeof( mf.charRoot ), "characters/%s/", dirname );
	Q_strncpyz( mf.archetypeName, archetypeName, sizeof( mf.archetypeName ) );

	lua_getfield( L, merged_idx, "display_name" );
	Q_strncpyz( mf.displayName,
		lua_isstring( L, -1 ) ? lua_tostring( L, -1 ) : dirname,
		sizeof( mf.displayName ) );
	lua_pop( L, 1 );

	// ── Step 7: Extract model fields ─────────────────────────────────────
	lua_getfield( L, merged_idx, "model" );
	int model_idx = lua_gettop( L );
	if ( lua_istable( L, model_idx ) ) {
		lua_getfield( L, model_idx, "parts" );
		if ( lua_istable( L, -1 ) ) {
			int parts_tbl = lua_gettop( L );
			mf.partCount = 0;
			for ( int i = 1; i <= CM_MAX_MODEL_PARTS; i++ ) {
				lua_rawgeti( L, parts_tbl, i );
				if ( !lua_isstring( L, -1 ) ) { lua_pop( L, 1 ); break; }
				const char *pname = lua_tostring( L, -1 );
				Q_strncpyz( mf.partNames[mf.partCount], pname, CM_PART_NAME_LEN );
				Com_sprintf( mf.partPaths[mf.partCount], sizeof( mf.partPaths[0] ),
					"characters/%s/models/%s", dirname, pname );
				mf.partCount++;
				lua_pop( L, 1 );
			}
		}
		lua_pop( L, 1 );

		lua_getfield( L, model_idx, "icon" );
		if ( lua_isstring( L, -1 ) )
			Com_sprintf( mf.iconPath, sizeof( mf.iconPath ),
				"%s%s", mf.charRoot, lua_tostring( L, -1 ) );
		lua_pop( L, 1 );

		lua_getfield( L, model_idx, "headoffset" );
		if ( lua_istable( L, -1 ) ) {
			for ( int i = 0; i < 3; i++ ) {
				lua_rawgeti( L, -1, i + 1 );
				mf.headOffset[i] = (float)lua_tonumber( L, -1 );
				lua_pop( L, 1 );
			}
		}
		lua_pop( L, 1 );

		lua_getfield( L, model_idx, "skins" );
		if ( lua_istable( L, -1 ) ) {
			int skins_tbl = lua_gettop( L );
			mf.numSkins = 0;
			lua_pushnil( L );
			while ( lua_next( L, skins_tbl ) != 0 ) {
				if ( lua_type( L, -2 ) == LUA_TSTRING && mf.numSkins < CM_MAX_SKINS ) {
					const char *skinName = lua_tostring( L, -2 );
					clParsedSkin_t parsed;
					if ( CL_Char_ParseSkin( L, dirname, skinName, &parsed ) ) {
						mf.skins[mf.numSkins].skinHandle = CL_RegisterCharacterSkin( &parsed );
						Q_strncpyz( mf.skins[mf.numSkins].name, parsed.name, CM_SKIN_NAME_LEN );
						mf.skins[mf.numSkins].paintable = parsed.paintable;
						mf.numSkins++;
					}
				}
				lua_pop( L, 1 );
			}
		}
		lua_pop( L, 1 );

		if ( !CL_Char_LoadAnimations( L, dirname, archetypeName, mf.animations ) ) {
			COM_WARN( LOG_CAT_CLIENT, "CL_Characters: '%s' animation load failed\n",
				dirname );
		}
	}
	lua_pop( L, 1 );  // pop model table

	// ── Step 8: Extract sounds ────────────────────────────────────────────
	lua_getfield( L, merged_idx, "sounds" );
	int sounds_idx = lua_gettop( L );
	if ( lua_istable( L, sounds_idx ) ) {
		lua_getfield( L, sounds_idx, "footsteps" );
		if ( lua_isstring( L, -1 ) ) {
			const char *ft = lua_tostring( L, -1 );
			if      ( !Q_stricmp( ft, "boot"   ) ) mf.footsteps = FOOTSTEP_BOOT;
			else if ( !Q_stricmp( ft, "flesh"  ) ) mf.footsteps = FOOTSTEP_FLESH;
			else if ( !Q_stricmp( ft, "mech"   ) ) mf.footsteps = FOOTSTEP_MECH;
			else if ( !Q_stricmp( ft, "energy" ) ) mf.footsteps = FOOTSTEP_ENERGY;
			else                                    mf.footsteps = FOOTSTEP_NORMAL;
		}
		lua_pop( L, 1 );

		for ( int i = 0; i < CM_SOUND_SLOTS; i++ ) {
			lua_getfield( L, sounds_idx, s_soundSlotName[i] );
			if ( lua_isstring( L, -1 ) ) {
				Com_sprintf( mf.soundPaths[i], sizeof( mf.soundPaths[i] ),
					"%s%s", mf.charRoot, lua_tostring( L, -1 ) );
			} else {
				Com_sprintf( mf.soundPaths[i], sizeof( mf.soundPaths[i] ),
					"%ssounds/%s.opus", mf.charRoot, s_soundSlotName[i] );
			}
			lua_pop( L, 1 );
		}
	}
	lua_pop( L, 1 );  // pop sounds table

	lua_settop( L, base );  // restore stack regardless of path

	// ── Store in registry ─────────────────────────────────────────────────
	entry = &s_clCharacters[s_clCharacterCount++];
	entry->loaded = qtrue;
	Q_strncpyz( entry->dirname, dirname, sizeof( entry->dirname ) );
	memcpy( &entry->manifest, &mf, sizeof( mf ) );
	entry->iconHandle = 0;   // registered lazily in CL_Characters_RegisterIcons

	return qtrue;
}

// Scan characters/ and (re)populate the registry.
static void CL_Characters_Scan( lua_State *L ) {
	char **dirs;
	int numDirs, i;

	dirs = FS_ListDirectories( "characters", &numDirs );
	if ( !dirs || numDirs == 0 ) {
		COM_WARN( LOG_CAT_CLIENT, "CL_Characters: no character directories found under characters/\n" );
		if ( dirs ) FS_FreeFileList( dirs );
		return;
	}

	for ( i = 0; i < numDirs && s_clCharacterCount < CL_MAX_CHARACTERS; i++ ) {
		if ( dirs[i][0] == '_' ) continue;  // skip _archetypes, _common, etc.
		CL_Characters_LoadOne( L, dirs[i] );
	}
	FS_FreeFileList( dirs );
}

void CL_Characters_Init( void ) {
	lua_State *L;

	memset( s_archetypeNames, 0, sizeof( s_archetypeNames ) );
	s_archetypeCount = 0;
	memset( s_clCharacters, 0, sizeof( s_clCharacters ) );
	s_clCharacterCount = 0;
	memset( s_characterSkins, 0, sizeof( s_characterSkins ) );
	s_characterSkinCount = 0;

	L = WiredScript_GetState();
	if ( !L ) {
		COM_WARN( LOG_CAT_CLIENT, "CL_Characters: WiredScript not available, skipping preload\n" );
		return;
	}

	CL_Archetypes_Scan();
	CL_Characters_Scan( L );
	Com_Log( SEV_INFO, LOG_CAT_CLIENT, "CL_Characters: loaded %d character(s)\n", s_clCharacterCount );

	Cmd_AddCommand( "reload_characters", CL_ReloadCharacters_f );
}

void CL_Characters_Reload( void ) {
	lua_State *L;

	memset( s_archetypeNames, 0, sizeof( s_archetypeNames ) );
	s_archetypeCount = 0;
	memset( s_clCharacters, 0, sizeof( s_clCharacters ) );
	s_clCharacterCount = 0;
	memset( s_characterSkins, 0, sizeof( s_characterSkins ) );
	s_characterSkinCount = 0;

	L = WiredScript_GetState();
	if ( !L ) {
		COM_WARN( LOG_CAT_CLIENT, "CL_Characters: WiredScript not available\n" );
		return;
	}

	CL_Archetypes_Scan();
	CL_Characters_Scan( L );

	// Re-register icons if renderer is available.
	if ( re.RegisterShaderNoMip )
		CL_Characters_RegisterIcons();

	Com_Log( SEV_INFO, LOG_CAT_CLIENT, "CL_Characters: reloaded %d character(s)\n", s_clCharacterCount );
}

void CL_Characters_Shutdown( void ) {
	memset( s_archetypeNames, 0, sizeof( s_archetypeNames ) );
	s_archetypeCount = 0;
	memset( s_clCharacters, 0, sizeof( s_clCharacters ) );
	s_clCharacterCount = 0;
	memset( s_characterSkins, 0, sizeof( s_characterSkins ) );
	s_characterSkinCount = 0;
	Cmd_RemoveCommand( "reload_characters" );
}

// Called after renderer is initialized so icon shaders and skin shaders can be registered.
void CL_Characters_RegisterIcons( void ) {
	int i;
	CL_Characters_RegisterShaders();
	for ( i = 0; i < s_clCharacterCount; i++ ) {
		if ( !s_clCharacters[i].loaded ) continue;
		if ( s_clCharacters[i].iconHandle ) continue;
		if ( s_clCharacters[i].manifest.iconPath[0] )
			s_clCharacters[i].iconHandle =
				re.RegisterShaderNoMip( s_clCharacters[i].manifest.iconPath );
	}
}

// ── Access API ────────────────────────────────────────────────────────────────

const characterManifest_t *CL_Characters_Get( const char *dirname ) {
	int i;
	for ( i = 0; i < s_clCharacterCount; i++ ) {
		if ( s_clCharacters[i].loaded && !Q_stricmp( s_clCharacters[i].dirname, dirname ) )
			return &s_clCharacters[i].manifest;
	}
	return NULL;
}

int CL_Characters_Count( void ) {
	return s_clCharacterCount;
}

const clCharacterEntry_t *CL_Characters_At( int index ) {
	if ( index < 0 || index >= s_clCharacterCount ) return NULL;
	return &s_clCharacters[index];
}

// ── Legacy trap entry point ───────────────────────────────────────────────────
//
// CL_GetValue("char:{name}") → serialises registry entry into characterManifest_t buf.
// No Lua is invoked at call time; this is a pure registry lookup.

qboolean CL_Characters_GetManifest( const char *charName, char *buf, int bufSize ) {
	int i;

	if ( bufSize < (int)sizeof( characterManifest_t ) ) {
		COM_ERROR( LOG_CAT_CLIENT, "CL_Characters: buffer too small for '%s' "
			"(need %d, got %d)\n", charName, (int)sizeof(characterManifest_t), bufSize );
		return qfalse;
	}

	for ( i = 0; i < s_clCharacterCount; i++ ) {
		if ( s_clCharacters[i].loaded && !Q_stricmp( s_clCharacters[i].dirname, charName ) ) {
			memcpy( buf, &s_clCharacters[i].manifest, sizeof( characterManifest_t ) );
			return qtrue;
		}
	}

	COM_WARN( LOG_CAT_CLIENT, "CL_Characters: '%s' not found in registry\n", charName );
	return qfalse;
}

static void CL_ReloadCharacters_f( void ) {
	CL_Characters_Reload();
}
