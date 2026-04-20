#include "server.h"
#include "sv_lua.h"

#include "../game/chars.h"

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#define SV_WB_MAX_CHARACTERS 256
#define SV_WB_MAX_INDEX 64
#define SV_WB_MAX_ATTACKS 16

typedef struct {
	int index;
	const char *key;
} svLuaStringField_t;

typedef struct {
	int   attackIndex; // index into bg_attacklist[], resolved at load time
	float aimHeight;   // units above entity origin to aim (0=feet, 28=center mass, 56=top)
} svLuaCachedAttack_t;

typedef struct {
	qboolean inuse;
	char name[MAX_QPATH];
	float skillNormalized;
	int profileRef;
	int botRef;
	qboolean hasDecideFn;   // qtrue if character's own bot table defines a 'decide' function
	char stringValues[SV_WB_MAX_INDEX][MAX_QPATH];
	float floatValues[SV_WB_MAX_INDEX];
	qboolean floatValid[SV_WB_MAX_INDEX];
	svLuaCachedAttack_t cachedAttacks[SV_WB_MAX_ATTACKS];
	int cachedAttackCount;
} svLuaCharacter_t;

typedef struct {
	qboolean bound;
	int characterHandle;
	float profileValues[WB_PROFILE_MAX];
} svLuaBotBinding_t;

// Canonical float-valued characteristic name -> CHARACTERISTIC_* index table.
// Injected into Lua as q3now.characteristics at init.
// String-valued indices (0/3/21/22/23/40) and undefined gaps are excluded.
static const struct { const char *name; int index; } s_characteristicNames[] = {
	{ "attack_skill",                 CHARACTERISTIC_ATTACK_SKILL },
	{ "view_factor",                  CHARACTERISTIC_VIEW_FACTOR },
	{ "view_maxchange",               CHARACTERISTIC_VIEW_MAXCHANGE },
	{ "reaction_time",                CHARACTERISTIC_REACTIONTIME },
	{ "accuracy",                   CHARACTERISTIC_AIM_ACCURACY },
	{ "accuracy_machinegun",        CHARACTERISTIC_AIM_ACCURACY_MACHINEGUN },
	{ "accuracy_shotgun",           CHARACTERISTIC_AIM_ACCURACY_SHOTGUN },
	{ "accuracy_rocket_launcher",   CHARACTERISTIC_AIM_ACCURACY_ROCKETLAUNCHER },
	{ "accuracy_grenade_launcher",  CHARACTERISTIC_AIM_ACCURACY_GRENADELAUNCHER },
	{ "accuracy_lightning_gun",     CHARACTERISTIC_AIM_ACCURACY_LIGHTNING },
	{ "accuracy_plasma_rifle",      CHARACTERISTIC_AIM_ACCURACY_PLASMAGUN },
	{ "accuracy_railgun",           CHARACTERISTIC_AIM_ACCURACY_RAILGUN },
	{ "skill",                   CHARACTERISTIC_AIM_SKILL },
	{ "skill_machinegun",        CHARACTERISTIC_AIM_SKILL_MACHINEGUN },
	{ "skill_shotgun",           CHARACTERISTIC_AIM_SKILL_SHOTGUN },
	{ "skill_grenade_launcher",  CHARACTERISTIC_AIM_SKILL_GRENADELAUNCHER },
	{ "skill_rocket_launcher",   CHARACTERISTIC_AIM_SKILL_ROCKETLAUNCHER },
	{ "skill_lightning_gun",     CHARACTERISTIC_AIM_SKILL_LIGHTNING },
	{ "skill_railgun",           CHARACTERISTIC_AIM_SKILL_RAILGUN },
	{ "skill_plasma_rifle",      CHARACTERISTIC_AIM_SKILL_PLASMAGUN },
	{ "insult",                   CHARACTERISTIC_CHAT_INSULT },
	{ "misc",                     CHARACTERISTIC_CHAT_MISC },
	{ "startendlevel",            CHARACTERISTIC_CHAT_STARTENDLEVEL },
	{ "enterexitgame",            CHARACTERISTIC_CHAT_ENTEREXITGAME },
	{ "kill",                     CHARACTERISTIC_CHAT_KILL },
	{ "death",                    CHARACTERISTIC_CHAT_DEATH },
	{ "enemysuicide",             CHARACTERISTIC_CHAT_ENEMYSUICIDE },
	{ "hittalking",               CHARACTERISTIC_CHAT_HITTALKING },
	{ "hitnodeath",               CHARACTERISTIC_CHAT_HITNODEATH },
	{ "hitnokill",                CHARACTERISTIC_CHAT_HITNOKILL },
	{ "random",                   CHARACTERISTIC_CHAT_RANDOM },
	{ "reply",                    CHARACTERISTIC_CHAT_REPLY },
	{ "croucher",                     CHARACTERISTIC_CROUCHER },
	{ "jumper",                       CHARACTERISTIC_JUMPER },
	{ "weaponjumping",                CHARACTERISTIC_WEAPONJUMPING },
	{ "grapple_user",                 CHARACTERISTIC_GRAPPLE_USER },
	{ "aggression",                   CHARACTERISTIC_AGGRESSION },
	{ "selfpreservation",             CHARACTERISTIC_SELFPRESERVATION },
	{ "vengefulness",                 CHARACTERISTIC_VENGEFULNESS },
	{ "camper",                       CHARACTERISTIC_CAMPER },
	{ "easy_fragger",                 CHARACTERISTIC_EASY_FRAGGER },
	{ "alertness",                    CHARACTERISTIC_ALERTNESS },
	{ "firethrottle",                 CHARACTERISTIC_FIRETHROTTLE },
	{ "walker",                       CHARACTERISTIC_WALKER },
	{ NULL, 0 }
};

// Per-frame error throttle: suppress duplicate errors for 10 seconds.
// Key is a cheap hash of (clientNum << 8 | methodIndex).
#define SV_WB_ERR_THROTTLE_SLOTS 64
#define SV_WB_ERR_THROTTLE_MS    10000

typedef enum {
	WB_METHOD_DECIDE = 0,
	WB_METHOD_ON_CHAT,
	WB_METHOD_COUNT
} svLuaMethodIndex_t;

typedef struct {
	int  clientNum;
	int  methodIndex;
	int  lastPrintTime;
} svLuaErrThrottle_t;

static lua_State *s_botLua = NULL;
static svLuaCharacter_t s_characters[SV_WB_MAX_CHARACTERS];
static svLuaBotBinding_t s_bots[MAX_CLIENTS];
static svLuaErrThrottle_t s_errThrottle[SV_WB_ERR_THROTTLE_SLOTS];

// Returns qtrue if this error should be printed (not yet suppressed).
// Prints once, then suppresses identical (client, method) pairs for 10 seconds.
static qboolean SV_Lua_ShouldPrintError( int clientNum, int methodIndex ) {
	int slot;
	int now;
	int i;

	slot = -1;
	now = sv.time;

	for ( i = 0; i < SV_WB_ERR_THROTTLE_SLOTS; i++ ) {
		if ( s_errThrottle[i].clientNum == clientNum && s_errThrottle[i].methodIndex == methodIndex ) {
			if ( now - s_errThrottle[i].lastPrintTime < SV_WB_ERR_THROTTLE_MS ) {
				return qfalse;
			}
			s_errThrottle[i].lastPrintTime = now;
			return qtrue;
		}
		if ( slot < 0 && s_errThrottle[i].clientNum < 0 ) {
			slot = i;
		}
	}

	// evict oldest slot if table is full
	if ( slot < 0 ) {
		int oldest = 0;
		for ( i = 1; i < SV_WB_ERR_THROTTLE_SLOTS; i++ ) {
			if ( s_errThrottle[i].lastPrintTime < s_errThrottle[oldest].lastPrintTime ) {
				oldest = i;
			}
		}
		slot = oldest;
	}

	s_errThrottle[slot].clientNum = clientNum;
	s_errThrottle[slot].methodIndex = methodIndex;
	s_errThrottle[slot].lastPrintTime = now;
	return qtrue;
}

static void SV_Lua_BotSetDefaultProfile( float *profileValues ) {
	if ( !profileValues ) {
		return;
	}

	memset( profileValues, 0, sizeof( float ) * WB_PROFILE_MAX );

	profileValues[WB_PROFILE_REACTION_TIME] = 0.3f;
	profileValues[WB_PROFILE_FOV] = 150.0f;
	profileValues[WB_PROFILE_AGGRESSION] = 0.5f;
	profileValues[WB_PROFILE_SELF_PRESERVE] = 0.5f;
	profileValues[WB_PROFILE_VENGEFULNESS] = 0.5f;
	profileValues[WB_PROFILE_CAMP_TENDENCY] = 0.0f;
	profileValues[WB_PROFILE_OPPORTUNISM] = 0.5f;
	profileValues[WB_PROFILE_TRACKING] = 0.5f;
	profileValues[WB_PROFILE_ACCURACY] = 0.5f;
	profileValues[WB_PROFILE_LEAD_SKILL] = 0.5f;
	profileValues[WB_PROFILE_STRAFE_JUMP] = 0.0f;
	profileValues[WB_PROFILE_ROCKET_JUMP] = 0.0f;
	profileValues[WB_PROFILE_BUNNY_HOP] = 0.0f;
	profileValues[WB_PROFILE_DODGE_ON_FIRE] = 0.5f;
	profileValues[WB_PROFILE_USE_JUMPPADS] = 1.0f;
	profileValues[WB_PROFILE_SWIM] = 1.0f;
	profileValues[WB_PROFILE_FIRETHROTTLE] = 0.5f;
}

static float SV_Lua_BotClamp01( float value ) {
	if ( value < 0.0f ) {
		return 0.0f;
	}
	if ( value > 1.0f ) {
		return 1.0f;
	}
	return value;
}

// Resolve a Lua value (number or {lo,hi} table) to a float via skill lerp.
// If the value is a plain number it is used as-is.  If it is a two-element
// array table {lo, hi} the result is lo + (hi-lo)*skill.  Otherwise the
// caller-supplied defaultValue is returned.
static float SV_Lua_ResolveFloat( lua_State *L, int valueIndex, float skill, float defaultValue ) {
	if ( valueIndex < 0 ) {
		valueIndex = lua_gettop( L ) + valueIndex + 1;
	}
	if ( lua_type( L, valueIndex ) == LUA_TNUMBER ) {
		return (float)lua_tonumber( L, valueIndex );
	}
	if ( lua_type( L, valueIndex ) == LUA_TTABLE ) {
		float lo, hi;
		lua_rawgeti( L, valueIndex, 1 );
		lua_rawgeti( L, valueIndex, 2 );
		lo = lua_isnumber( L, -2 ) ? (float)lua_tonumber( L, -2 ) : defaultValue;
		hi = lua_isnumber( L, -1 ) ? (float)lua_tonumber( L, -1 ) : defaultValue;
		lua_pop( L, 2 );
		return lo + ( hi - lo ) * skill;
	}
	return defaultValue;
}

// Resolve a Lua value as a threshold boolean:
//   bool   → use directly
//   number → true when skill >= threshold
//   nil    → return defaultValue
static qboolean SV_Lua_ResolveThreshold( lua_State *L, int valueIndex, float skill, qboolean defaultValue ) {
	int t;
	if ( valueIndex < 0 ) {
		valueIndex = lua_gettop( L ) + valueIndex + 1;
	}
	t = lua_type( L, valueIndex );
	if ( t == LUA_TBOOLEAN ) {
		return lua_toboolean( L, valueIndex ) ? qtrue : qfalse;
	}
	if ( t == LUA_TNUMBER ) {
		return skill >= (float)lua_tonumber( L, valueIndex ) ? qtrue : qfalse;
	}
	return defaultValue;
}

static int SV_Lua_BotPushMethod( lua_State *L, int clientNum, const char *method ) {
	int characterHandle;

	if ( !L || !method || !method[0] ) {
		return qfalse;
	}

	if ( clientNum < 0 || clientNum >= MAX_CLIENTS ) {
		return qfalse;
	}

	if ( !s_bots[clientNum].bound ) {
		return qfalse;
	}

	characterHandle = s_bots[clientNum].characterHandle;
	if ( characterHandle <= 0 || characterHandle >= SV_WB_MAX_CHARACTERS ) {
		return qfalse;
	}

	if ( !s_characters[characterHandle].inuse || s_characters[characterHandle].botRef == LUA_NOREF ) {
		return qfalse;
	}

	lua_settop( L, 0 );
	lua_rawgeti( L, LUA_REGISTRYINDEX, s_characters[characterHandle].botRef );
	if ( !lua_istable( L, -1 ) ) {
		lua_settop( L, 0 );
		return qfalse;
	}

	lua_getfield( L, -1, method );
	if ( !lua_isfunction( L, -1 ) ) {
		lua_settop( L, 0 );
		return qfalse;
	}

	lua_pushvalue( L, -2 );
	return qtrue;
}

static int SV_Lua_LoadFileChunk( lua_State *L, const char *qpath );

static int SV_Lua_LoadBotFromPath( lua_State *L, const char *path, int *outRef ) {
	int status;

	status = SV_Lua_LoadFileChunk( L, path );
	if ( status != 0 ) {
		return status;
	}

	status = lua_pcall( L, 0, 1, 0 );
	if ( status != 0 ) {
		return status;
	}

	if ( !lua_istable( L, -1 ) ) {
		lua_pop( L, 1 );
		lua_pushfstring( L, "BotLua: %s must return a table", path );
		return LUA_ERRRUN;
	}

	if ( outRef ) {
		*outRef = luaL_ref( L, LUA_REGISTRYINDEX );
	} else {
		lua_pop( L, 1 );
	}

	return 0;
}

static void SV_Lua_BotInitCachedProfile( int clientNum ) {
	lua_State *L;
	int characterHandle;
	float skill;
	float *profile;
	int botTableIndex;
	int subTableIndex;

	if ( !s_botLua ) {
		return;
	}

	if ( clientNum < 0 || clientNum >= MAX_CLIENTS ) {
		return;
	}

	if ( !s_bots[clientNum].bound ) {
		return;
	}

	characterHandle = s_bots[clientNum].characterHandle;
	if ( characterHandle <= 0 || characterHandle >= SV_WB_MAX_CHARACTERS ) {
		return;
	}

	if ( !s_characters[characterHandle].inuse || s_characters[characterHandle].botRef == LUA_NOREF ) {
		return;
	}

	profile = s_bots[clientNum].profileValues;
	SV_Lua_BotSetDefaultProfile( profile );

	L = s_botLua;
	skill = s_characters[characterHandle].skillNormalized;

	lua_settop( L, 0 );
	lua_rawgeti( L, LUA_REGISTRYINDEX, s_characters[characterHandle].botRef );
	if ( !lua_istable( L, -1 ) ) {
		lua_settop( L, 0 );
		return;
	}
	botTableIndex = lua_gettop( L );

	// --- traits → WB_PROFILE_* ---
	lua_getfield( L, botTableIndex, "traits" );
	if ( lua_istable( L, -1 ) ) {
		subTableIndex = lua_gettop( L );

		lua_getfield( L, subTableIndex, "reaction_time" );
		profile[WB_PROFILE_REACTION_TIME] = SV_Lua_ResolveFloat( L, -1, skill, profile[WB_PROFILE_REACTION_TIME] );
		lua_pop( L, 1 );

		lua_getfield( L, subTableIndex, "view_factor" );
		profile[WB_PROFILE_FOV] = SV_Lua_ResolveFloat( L, -1, skill, profile[WB_PROFILE_FOV] );
		lua_pop( L, 1 );

		lua_getfield( L, subTableIndex, "aggression" );
		profile[WB_PROFILE_AGGRESSION] = SV_Lua_BotClamp01( SV_Lua_ResolveFloat( L, -1, skill, profile[WB_PROFILE_AGGRESSION] ) );
		lua_pop( L, 1 );

		lua_getfield( L, subTableIndex, "selfpreservation" );
		profile[WB_PROFILE_SELF_PRESERVE] = SV_Lua_BotClamp01( SV_Lua_ResolveFloat( L, -1, skill, profile[WB_PROFILE_SELF_PRESERVE] ) );
		lua_pop( L, 1 );

		lua_getfield( L, subTableIndex, "vengefulness" );
		profile[WB_PROFILE_VENGEFULNESS] = SV_Lua_BotClamp01( SV_Lua_ResolveFloat( L, -1, skill, profile[WB_PROFILE_VENGEFULNESS] ) );
		lua_pop( L, 1 );

		lua_getfield( L, subTableIndex, "camper" );
		profile[WB_PROFILE_CAMP_TENDENCY] = SV_Lua_BotClamp01( SV_Lua_ResolveFloat( L, -1, skill, profile[WB_PROFILE_CAMP_TENDENCY] ) );
		lua_pop( L, 1 );

		lua_getfield( L, subTableIndex, "easy_fragger" );
		profile[WB_PROFILE_OPPORTUNISM] = SV_Lua_BotClamp01( SV_Lua_ResolveFloat( L, -1, skill, profile[WB_PROFILE_OPPORTUNISM] ) );
		lua_pop( L, 1 );

		lua_getfield( L, subTableIndex, "firethrottle" );
		profile[WB_PROFILE_FIRETHROTTLE] = SV_Lua_BotClamp01( SV_Lua_ResolveFloat( L, -1, skill, profile[WB_PROFILE_FIRETHROTTLE] ) );
		lua_pop( L, 1 );
	}
	lua_pop( L, 1 );

	// --- aim → WB_PROFILE_* ---
	lua_getfield( L, botTableIndex, "aim" );
	if ( lua_istable( L, -1 ) ) {
		subTableIndex = lua_gettop( L );

		lua_getfield( L, subTableIndex, "skill" );
		profile[WB_PROFILE_TRACKING] = SV_Lua_BotClamp01( SV_Lua_ResolveFloat( L, -1, skill, profile[WB_PROFILE_TRACKING] ) );
		profile[WB_PROFILE_LEAD_SKILL] = profile[WB_PROFILE_TRACKING];
		lua_pop( L, 1 );

		lua_getfield( L, subTableIndex, "accuracy" );
		profile[WB_PROFILE_ACCURACY] = SV_Lua_BotClamp01( SV_Lua_ResolveFloat( L, -1, skill, profile[WB_PROFILE_ACCURACY] ) );
		lua_pop( L, 1 );
	}
	lua_pop( L, 1 );

	// --- movement → WB_PROFILE_* ---
	// Threshold convention: number means "enable at skill >= N"; true/false = always on/off.
	// dodge_on_fire is a numeric lerp, not a threshold.
	lua_getfield( L, botTableIndex, "movement" );
	if ( lua_istable( L, -1 ) ) {
		subTableIndex = lua_gettop( L );

		lua_getfield( L, subTableIndex, "strafe_jump" );
		profile[WB_PROFILE_STRAFE_JUMP] = SV_Lua_ResolveThreshold( L, -1, skill, profile[WB_PROFILE_STRAFE_JUMP] > 0.5f ) ? 1.0f : 0.0f;
		lua_pop( L, 1 );

		lua_getfield( L, subTableIndex, "rocket_jump" );
		profile[WB_PROFILE_ROCKET_JUMP] = SV_Lua_ResolveThreshold( L, -1, skill, profile[WB_PROFILE_ROCKET_JUMP] > 0.5f ) ? 1.0f : 0.0f;
		lua_pop( L, 1 );

		lua_getfield( L, subTableIndex, "bunny_hop" );
		profile[WB_PROFILE_BUNNY_HOP] = SV_Lua_ResolveThreshold( L, -1, skill, profile[WB_PROFILE_BUNNY_HOP] > 0.5f ) ? 1.0f : 0.0f;
		lua_pop( L, 1 );

		lua_getfield( L, subTableIndex, "dodge_on_fire" );
		profile[WB_PROFILE_DODGE_ON_FIRE] = SV_Lua_BotClamp01( SV_Lua_ResolveFloat( L, -1, skill, profile[WB_PROFILE_DODGE_ON_FIRE] ) );
		lua_pop( L, 1 );

		lua_getfield( L, subTableIndex, "use_jumppads" );
		profile[WB_PROFILE_USE_JUMPPADS] = SV_Lua_ResolveThreshold( L, -1, skill, profile[WB_PROFILE_USE_JUMPPADS] > 0.5f ) ? 1.0f : 0.0f;
		lua_pop( L, 1 );

		lua_getfield( L, subTableIndex, "swim" );
		profile[WB_PROFILE_SWIM] = SV_Lua_ResolveThreshold( L, -1, skill, profile[WB_PROFILE_SWIM] > 0.5f ) ? 1.0f : 0.0f;
		lua_pop( L, 1 );
	}
	lua_pop( L, 1 );

	lua_settop( L, 0 );
}

static int SV_Lua_LoadFileChunk( lua_State *L, const char *qpath ) {
	void *buffer = NULL;
	int len;
	int status;

	if ( !qpath || !qpath[0] ) {
		lua_pushstring( L, "BotLua: missing script path" );
		return LUA_ERRFILE;
	}

	len = FS_ReadFile( qpath, &buffer );
	if ( !buffer || len <= 0 ) {
		if ( buffer ) {
			FS_FreeFile( buffer );
		}
		lua_pushfstring( L, "BotLua: failed reading %s", qpath );
		return LUA_ERRFILE;
	}

	status = luaL_loadbuffer( L, (const char *)buffer, len, qpath );
	FS_FreeFile( buffer );
	return status;
}

static int SV_Lua_Q3NowLoad( lua_State *L ) {
	const char *qpath = luaL_checkstring( L, 1 );
	char pathbuf[MAX_QPATH];
	int status;
	int pathlen;

	// append .lua if the caller omitted the extension (spec: q3now.load("path/module"))
	pathlen = (int)strlen( qpath );
	if ( pathlen < 4 || Q_stricmp( qpath + pathlen - 4, ".lua" ) != 0 ) {
		Com_sprintf( pathbuf, sizeof( pathbuf ), "%s.lua", qpath );
		qpath = pathbuf;
	}

	lua_settop( L, 0 );

	status = SV_Lua_LoadFileChunk( L, qpath );
	if ( status != 0 ) {
		return lua_error( L );
	}

	status = lua_pcall( L, 0, LUA_MULTRET, 0 );
	if ( status != 0 ) {
		return lua_error( L );
	}

	return lua_gettop( L );
}

static int SV_Lua_Q3NowPrint( lua_State *L ) {
	int i;
	int nargs = lua_gettop( L );
	QS_LOCAL(message, 1024);

	for ( i = 1; i <= nargs; i++ ) {
		const char *argText;

		lua_getglobal( L, "tostring" );
		lua_pushvalue( L, i );
		if ( lua_pcall( L, 1, 1, 0 ) != 0 ) {
			lua_pop( L, 1 );
			argText = "<tostring error>";
		} else {
			argText = lua_tostring( L, -1 );
			if ( !argText ) {
				argText = "<non-string>";
			}
		}
		if ( i > 1 ) {
			QS_AppendChar( &message, '\t' );
		}
		QS_Append( &message, argText );
		if ( lua_gettop( L ) > nargs ) {
			lua_pop( L, 1 );
		}
	}

	Com_Printf( "BotLua: %s\n", QS_CStr(&message) );
	return 0;
}

static int SV_Lua_Q3NowTime( lua_State *L ) {
	lua_pushnumber( L, (lua_Number)sv.time );
	return 1;
}

// String fields read from the identity profile table (main.lua).
// Chat strings (chat_name, chat_file) are read from the bot table's chats section.
static const svLuaStringField_t s_stringFields[] = {
	{ CHARACTERISTIC_NAME, "name" }
};

static int SV_Lua_ProfileSetStringField( lua_State *L, int tableIndex, const char *key, char *dst, int dstSize ) {
	const char *value;

	lua_getfield( L, tableIndex, key );
	if ( !lua_isstring( L, -1 ) ) {
		lua_pop( L, 1 );
		return qfalse;
	}

	value = lua_tostring( L, -1 );
	if ( value && value[0] ) {
		Q_strncpyz( dst, value, dstSize );
		lua_pop( L, 1 );
		return qtrue;
	}

	lua_pop( L, 1 );
	return qfalse;
}

static float SV_Lua_DefaultCharacteristicNormalized( int index, float skillNormalized ) {
	switch ( index ) {
		case CHARACTERISTIC_REACTIONTIME:
		case CHARACTERISTIC_CHAT_INSULT:
		case CHARACTERISTIC_CHAT_MISC:
		case CHARACTERISTIC_CHAT_STARTENDLEVEL:
		case CHARACTERISTIC_CHAT_ENTEREXITGAME:
		case CHARACTERISTIC_CHAT_KILL:
		case CHARACTERISTIC_CHAT_DEATH:
		case CHARACTERISTIC_CHAT_ENEMYSUICIDE:
		case CHARACTERISTIC_CHAT_HITTALKING:
		case CHARACTERISTIC_CHAT_HITNODEATH:
		case CHARACTERISTIC_CHAT_HITNOKILL:
		case CHARACTERISTIC_CHAT_RANDOM:
		case CHARACTERISTIC_CHAT_REPLY:
		case CHARACTERISTIC_WALKER:
			return 1.0f - skillNormalized;
		default:
			return skillNormalized;
	}
}

static void SV_Lua_InitCharacterStrings( svLuaCharacter_t *character, const char *characterName ) {
	int i;

	for ( i = 0; i < SV_WB_MAX_INDEX; i++ ) {
		character->stringValues[i][0] = '\0';
	}

	Q_strncpyz( character->stringValues[CHARACTERISTIC_NAME], characterName, sizeof( character->stringValues[CHARACTERISTIC_NAME] ) );
	Q_strncpyz( character->stringValues[CHARACTERISTIC_CHAT_NAME], characterName, sizeof( character->stringValues[CHARACTERISTIC_CHAT_NAME] ) );
}

static void SV_Lua_ReleaseCharacterProfileRef( int characterHandle ) {
	if ( !s_botLua ) {
		return;
	}

	if ( characterHandle <= 0 || characterHandle >= SV_WB_MAX_CHARACTERS ) {
		return;
	}

	if ( s_characters[characterHandle].profileRef != LUA_NOREF ) {
		luaL_unref( s_botLua, LUA_REGISTRYINDEX, s_characters[characterHandle].profileRef );
		s_characters[characterHandle].profileRef = LUA_NOREF;
	}
}

static void SV_Lua_ReleaseCharacterBotRef( int characterHandle ) {
	if ( !s_botLua ) {
		return;
	}

	if ( characterHandle <= 0 || characterHandle >= SV_WB_MAX_CHARACTERS ) {
		return;
	}

	if ( s_characters[characterHandle].botRef != LUA_NOREF ) {
		luaL_unref( s_botLua, LUA_REGISTRYINDEX, s_characters[characterHandle].botRef );
		s_characters[characterHandle].botRef = LUA_NOREF;
	}
}

static void SV_Lua_ResetState( void ) {
	int i;

	for ( i = 0; i < SV_WB_MAX_CHARACTERS; i++ ) {
		s_characters[i].inuse = qfalse;
		s_characters[i].name[0] = '\0';
		s_characters[i].skillNormalized = 0.0f;
		s_characters[i].profileRef = LUA_NOREF;
		s_characters[i].botRef = LUA_NOREF;
		memset( s_characters[i].stringValues, 0, sizeof( s_characters[i].stringValues ) );
		memset( s_characters[i].floatValues, 0, sizeof( s_characters[i].floatValues ) );
		memset( s_characters[i].floatValid, 0, sizeof( s_characters[i].floatValid ) );
		s_characters[i].cachedAttackCount = 0;
	}

	memset( s_bots, 0, sizeof( s_bots ) );
	for ( i = 0; i < MAX_CLIENTS; i++ ) {
		SV_Lua_BotSetDefaultProfile( s_bots[i].profileValues );
	}

	// reset error throttle table: clientNum = -1 means free slot
	for ( i = 0; i < SV_WB_ERR_THROTTLE_SLOTS; i++ ) {
		s_errThrottle[i].clientNum = -1;
		s_errThrottle[i].methodIndex = 0;
		s_errThrottle[i].lastPrintTime = 0;
	}
}

static int SV_Lua_GetOrCreateCharacterHandle( const char *characterName, float skillNormalized ) {
	int i;

	if ( !characterName || !characterName[0] ) {
		return 0;
	}

	for ( i = 1; i < SV_WB_MAX_CHARACTERS; i++ ) {
		if ( s_characters[i].inuse && !Q_stricmp( s_characters[i].name, characterName ) ) {
			s_characters[i].skillNormalized = skillNormalized;
			memset( s_characters[i].floatValues, 0, sizeof( s_characters[i].floatValues ) );
			memset( s_characters[i].floatValid, 0, sizeof( s_characters[i].floatValid ) );
			s_characters[i].cachedAttackCount = 0;
			SV_Lua_InitCharacterStrings( &s_characters[i], characterName );
			return i;
		}
	}

	for ( i = 1; i < SV_WB_MAX_CHARACTERS; i++ ) {
		if ( !s_characters[i].inuse ) {
			s_characters[i].inuse = qtrue;
			Q_strncpyz( s_characters[i].name, characterName, sizeof( s_characters[i].name ) );
			s_characters[i].skillNormalized = skillNormalized;
			s_characters[i].profileRef = LUA_NOREF;
			s_characters[i].botRef = LUA_NOREF;
			memset( s_characters[i].floatValues, 0, sizeof( s_characters[i].floatValues ) );
			memset( s_characters[i].floatValid, 0, sizeof( s_characters[i].floatValid ) );
			s_characters[i].cachedAttackCount = 0;
			SV_Lua_InitCharacterStrings( &s_characters[i], characterName );
			return i;
		}
	}

	Com_Printf( S_COLOR_YELLOW "BotLua: character table full, cannot register '%s'\n", characterName );
	return 0;
}

void SV_Lua_Init( void ) {
	int i;
	int status;

	if ( s_botLua ) {
		SV_Lua_Shutdown();
	}

	SV_Lua_ResetState();

	s_botLua = luaL_newstate();
	if ( !s_botLua ) {
		Com_Printf( S_COLOR_YELLOW "BotLua: failed to initialize Lua state\n" );
		return;
	}

	luaL_openlibs( s_botLua );

	lua_newtable( s_botLua );
	lua_pushcfunction( s_botLua, SV_Lua_Q3NowLoad );
	lua_setfield( s_botLua, -2, "load" );
	lua_pushcfunction( s_botLua, SV_Lua_Q3NowPrint );
	lua_setfield( s_botLua, -2, "print" );
	lua_pushcfunction( s_botLua, SV_Lua_Q3NowTime );
	lua_setfield( s_botLua, -2, "time" );

	// inject q3now.characteristics: name -> CHARACTERISTIC_* index
	// single source of truth — derived from chars.h #defines, zero drift
	lua_newtable( s_botLua );
	for ( i = 0; s_characteristicNames[i].name; i++ ) {
		lua_pushinteger( s_botLua, s_characteristicNames[i].index );
		lua_setfield( s_botLua, -2, s_characteristicNames[i].name );
	}
	lua_setfield( s_botLua, -2, "characteristics" );

	lua_setglobal( s_botLua, "q3now" );

	// load _base/main.lua — installs q3now.load_character trampoline as side effect
	lua_getglobal( s_botLua, "q3now" );
	lua_getfield( s_botLua, -1, "load" );
	lua_remove( s_botLua, -2 );
	lua_pushstring( s_botLua, "characters/_base/main" );
	status = lua_pcall( s_botLua, 1, 1, 0 );
	if ( status != 0 ) {
		const char *err = lua_tostring( s_botLua, -1 );
		Com_Printf( S_COLOR_YELLOW "BotLua: failed loading _base/main.lua (%s)\n", err ? err : "unknown" );
	}
	lua_settop( s_botLua, 0 );

	Com_Printf( "BotLua: initialized\n" );
}

void SV_Lua_Shutdown( void ) {
	if ( s_botLua ) {
		lua_close( s_botLua );
		s_botLua = NULL;
	}
	SV_Lua_ResetState();
}

// Look up a string key in s_characteristicNames[], returning the CHARACTERISTIC_* index
// or -1 if not found.
static int SV_Lua_FindCharacteristicIndex( const char *key ) {
	int i;
	for ( i = 0; s_characteristicNames[i].name; i++ ) {
		if ( Q_stricmp( s_characteristicNames[i].name, key ) == 0 ) {
			return s_characteristicNames[i].index;
		}
	}
	return -1;
}

// Walk one sub-table of the bot data table (e.g. "traits", "aim", "chats"),
// resolve each float-valued key via s_characteristicNames[], lerp {lo,hi} pairs at skill.
static void SV_Lua_LoadBotSubTable( lua_State *L, int botTableIndex, const char *section, int handle ) {
	float skill = s_characters[handle].skillNormalized;

	lua_getfield( L, botTableIndex, section );
	if ( !lua_istable( L, -1 ) ) {
		lua_pop( L, 1 );
		return;
	}

	lua_pushnil( L );
	while ( lua_next( L, -2 ) ) {
		if ( lua_type( L, -2 ) == LUA_TSTRING ) {
			const char *key = lua_tostring( L, -2 );
			int idx = SV_Lua_FindCharacteristicIndex( key );
			if ( idx >= 0 && idx < SV_WB_MAX_INDEX ) {
				s_characters[handle].floatValues[idx] = SV_Lua_ResolveFloat( L, -1, skill, 0.0f );
				s_characters[handle].floatValid[idx] = qtrue;
			}
		}
		lua_pop( L, 1 );
	}
	lua_pop( L, 1 );
}

// Walk bot/main.lua attacks[] at character load time and pre-resolve each
// entry to a bg_attacklist[] index + aim height.  Entries may be plain strings
// (default aim_height=28) or tables { "shortname", aim_height = N }.
// Stored in cachedAttacks[] so BotPickWeapon/GetAttackAimHeight do zero Lua ops per frame.
static void SV_Lua_CacheAttacks( lua_State *L, int botTableIndex, int handle ) {
	int n, i, idx, count;
	const char *shortname;
	float aimHeight;

	s_characters[handle].cachedAttackCount = 0;
	lua_getfield( L, botTableIndex, "attacks" );
	if ( !lua_istable( L, -1 ) ) {
		lua_pop( L, 1 );
		return;
	}

	n = (int)lua_objlen( L, -1 );
	for ( i = 1; i <= n && s_characters[handle].cachedAttackCount < SV_WB_MAX_ATTACKS; i++ ) {
		lua_rawgeti( L, -1, i );
		shortname = NULL;
		aimHeight = 28.0f;

		if ( lua_type( L, -1 ) == LUA_TSTRING ) {
			// plain string entry — use default aim height
			shortname = lua_tostring( L, -1 );
		} else if ( lua_type( L, -1 ) == LUA_TTABLE ) {
			// table entry: { "shortname", aim_height = N }
			lua_rawgeti( L, -1, 1 );
			if ( lua_type( L, -1 ) == LUA_TSTRING ) {
				shortname = lua_tostring( L, -1 );
			}
			lua_pop( L, 1 );
			lua_getfield( L, -1, "aim_height" );
			if ( lua_isnumber( L, -1 ) ) {
				aimHeight = (float)lua_tonumber( L, -1 );
			}
			lua_pop( L, 1 );
		}

		if ( shortname ) {
			idx = BG_AttackByShortname( shortname );
			if ( idx > 0 ) {  // 0 == ATT_NONE, skip unknowns
				count = s_characters[handle].cachedAttackCount;
				s_characters[handle].cachedAttacks[count].attackIndex = idx;
				s_characters[handle].cachedAttacks[count].aimHeight   = aimHeight;
				s_characters[handle].cachedAttackCount++;
			}
		}
		lua_pop( L, 1 );
	}
	lua_pop( L, 1 );
}

// Read all float characteristics from the bot data table.
// traits, aim, and chats sub-tables are walked; keys are matched against
// s_characteristicNames[] to get CHARACTERISTIC_* indices.  {lo,hi} pairs
// are resolved to a single float at the character's current skillNormalized.
static void SV_Lua_LoadCharacterFloatValues( lua_State *L, int botTableIndex, int handle ) {
	memset( s_characters[handle].floatValues, 0, sizeof( s_characters[handle].floatValues ) );
	memset( s_characters[handle].floatValid, 0, sizeof( s_characters[handle].floatValid ) );

	SV_Lua_LoadBotSubTable( L, botTableIndex, "traits", handle );
	SV_Lua_LoadBotSubTable( L, botTableIndex, "aim",    handle );
	SV_Lua_LoadBotSubTable( L, botTableIndex, "chats",  handle );
}

int SV_Lua_LoadCharacter( const char *characterName, float skillNormalized ) {
	lua_State *L;
	int status;
	int i;
	int profileTableIndex;
	int profileRef;
	int botRef;
	char botPath[MAX_QPATH];
	const char *baseBotPath = "characters/_base/bot/main.lua";
	int handle;

	if ( !s_botLua ) {
		return 0;
	}

	if ( !characterName || !characterName[0] ) {
		return 0;
	}

	if ( skillNormalized < 0.0f ) {
		skillNormalized = 0.0f;
	} else if ( skillNormalized > 1.0f ) {
		skillNormalized = 1.0f;
	}

	handle = SV_Lua_GetOrCreateCharacterHandle( characterName, skillNormalized );
	if ( !handle ) {
		return 0;
	}

	Com_sprintf( botPath, sizeof( botPath ), "characters/%s/bot/main.lua", characterName );

	L = s_botLua;
	lua_settop( L, 0 );

	// call q3now.load_character(name, skillNormalized) — trampoline installed by _base/main.lua
	lua_getglobal( L, "q3now" );
	lua_getfield( L, -1, "load_character" );
	lua_remove( L, -2 );
	if ( !lua_isfunction( L, -1 ) ) {
		Com_Printf( S_COLOR_YELLOW "BotLua: q3now.load_character not available\n" );
		lua_settop( L, 0 );
		SV_Lua_FreeCharacter( handle );
		return 0;
	}
	lua_pushstring( L, characterName );
	lua_pushnumber( L, (double)skillNormalized );
	status = lua_pcall( L, 2, 1, 0 );
	if ( status != 0 ) {
		const char *err = lua_tostring( L, -1 );
		Com_Printf( S_COLOR_YELLOW "BotLua: failed loading character '%s' (%s)\n", characterName, err ? err : "unknown" );
		lua_settop( L, 0 );
		SV_Lua_FreeCharacter( handle );
		return 0;
	}

	if ( !lua_istable( L, -1 ) ) {
		Com_Printf( S_COLOR_YELLOW "BotLua: load_character('%s') must return a table\n", characterName );
		lua_settop( L, 0 );
		SV_Lua_FreeCharacter( handle );
		return 0;
	}

	profileTableIndex = lua_gettop( L );

	lua_pushnumber( L, skillNormalized );
	lua_setfield( L, profileTableIndex, "skill" );
	lua_pushinteger( L, handle );
	lua_setfield( L, profileTableIndex, "characterHandle" );

	for ( i = 0; i < (int)( sizeof( s_stringFields ) / sizeof( s_stringFields[0] ) ); i++ ) {
		SV_Lua_ProfileSetStringField(
			L,
			profileTableIndex,
			s_stringFields[i].key,
			s_characters[handle].stringValues[s_stringFields[i].index],
			sizeof( s_characters[handle].stringValues[s_stringFields[i].index] )
		);
	}

	if ( s_characters[handle].stringValues[CHARACTERISTIC_NAME][0] == '\0' ) {
		Q_strncpyz(
			s_characters[handle].stringValues[CHARACTERISTIC_NAME],
			characterName,
			sizeof( s_characters[handle].stringValues[CHARACTERISTIC_NAME] )
		);
	}

	// nicknames[1] is the primary botlib-facing name (chat matching, addbot, /char).
	// All entries in the array are valid aliases; botlib queries CHARACTERISTIC_CHAT_NAME → nicknames[1].
	lua_getfield( L, profileTableIndex, "nicknames" );
	if ( lua_istable( L, -1 ) ) {
		lua_rawgeti( L, -1, 1 );
		if ( lua_isstring( L, -1 ) ) {
			const char *nick = lua_tostring( L, -1 );
			if ( nick && nick[0] ) {
				Q_strncpyz(
					s_characters[handle].stringValues[CHARACTERISTIC_CHAT_NAME],
					nick,
					sizeof( s_characters[handle].stringValues[CHARACTERISTIC_CHAT_NAME] )
				);
			}
		}
		lua_pop( L, 1 );
	}
	lua_pop( L, 1 );

	if ( s_characters[handle].stringValues[CHARACTERISTIC_CHAT_NAME][0] == '\0' ) {
		Q_strncpyz(
			s_characters[handle].stringValues[CHARACTERISTIC_CHAT_NAME],
			s_characters[handle].stringValues[CHARACTERISTIC_NAME],
			sizeof( s_characters[handle].stringValues[CHARACTERISTIC_CHAT_NAME] )
		);
	}

	SV_Lua_ReleaseCharacterProfileRef( handle );
	SV_Lua_ReleaseCharacterBotRef( handle );
	lua_pushvalue( L, profileTableIndex );
	profileRef = luaL_ref( L, LUA_REGISTRYINDEX );
	s_characters[handle].profileRef = profileRef;

	// Always load bot brain from bot/main.lua; fall back to _base/bot/main.lua.
	// The old profile.bot embedded table pattern is no longer supported.
	botRef = LUA_NOREF;
	status = SV_Lua_LoadBotFromPath( L, botPath, &botRef );
	if ( status != 0 ) {
		lua_settop( L, 0 );
		status = SV_Lua_LoadBotFromPath( L, baseBotPath, &botRef );
	} else {
		// Character has its own bot table. Wire up __index → _base so that methods
		// defined there (decide, on_chat, etc.) are inherited without duplication.
		// lua_getfield follows __index, so BotPushMethod picks them up automatically.
		int baseBotRef = LUA_NOREF;
		lua_settop( L, 0 );
		if ( SV_Lua_LoadBotFromPath( L, baseBotPath, &baseBotRef ) == 0 && baseBotRef != LUA_NOREF ) {
			lua_rawgeti( L, LUA_REGISTRYINDEX, botRef );      // [1] char bot table
			lua_createtable( L, 0, 1 );                        // [2] new metatable
			lua_rawgeti( L, LUA_REGISTRYINDEX, baseBotRef );   // [3] base bot table
			lua_setfield( L, -2, "__index" );                  // mt.__index = base; pops [3]
			lua_setmetatable( L, -2 );                         // setmetatable(char, mt); pops [2]
			lua_settop( L, 0 );
			// Registry ref no longer needed — base table stays alive via char's metatable.
			luaL_unref( L, LUA_REGISTRYINDEX, baseBotRef );
		} else {
			lua_settop( L, 0 );
		}
	}

	if ( status != 0 ) {
		const char *err = lua_tostring( L, -1 );
		Com_Printf( S_COLOR_YELLOW "BotLua: failed loading brain for %s (%s)\n", characterName, err ? err : "unknown" );
		lua_settop( L, 0 );
		SV_Lua_FreeCharacter( handle );
		return 0;
	}

	s_characters[handle].botRef = botRef;

	// Detect per-character Lua decide override. rawget bypasses __index so we only
	// flag characters whose own bot table defines the function, not inherited ones.
	{
		lua_settop( L, 0 );
		lua_rawgeti( L, LUA_REGISTRYINDEX, botRef );
		lua_pushstring( L, "decide" );
		lua_rawget( L, -2 );
		s_characters[handle].hasDecideFn = lua_isfunction( L, -1 ) ? qtrue : qfalse;
		lua_settop( L, 0 );
	}

	// Read float characteristics and chat strings from the bot data table.
	lua_rawgeti( L, LUA_REGISTRYINDEX, botRef );
	{
		int botTableIndex = lua_gettop( L );

		// chat_file comes from bot table's chats sub-table
		lua_getfield( L, botTableIndex, "chats" );
		if ( lua_istable( L, -1 ) ) {
			int chatsIdx = lua_gettop( L );
			SV_Lua_ProfileSetStringField( L, chatsIdx, "file",
				s_characters[handle].stringValues[CHARACTERISTIC_CHAT_FILE],
				sizeof( s_characters[handle].stringValues[CHARACTERISTIC_CHAT_FILE] ) );
		}
		lua_pop( L, 1 );

		SV_Lua_LoadCharacterFloatValues( L, botTableIndex, handle );
		SV_Lua_CacheAttacks( L, botTableIndex, handle );
	}

	lua_settop( L, 0 );
	return handle;
}

void SV_Lua_FreeCharacter( int characterHandle ) {
	int i;

	if ( characterHandle <= 0 || characterHandle >= SV_WB_MAX_CHARACTERS ) {
		return;
	}

	SV_Lua_ReleaseCharacterProfileRef( characterHandle );
	SV_Lua_ReleaseCharacterBotRef( characterHandle );

	for ( i = 0; i < MAX_CLIENTS; i++ ) {
		if ( s_bots[i].bound && s_bots[i].characterHandle == characterHandle ) {
			s_bots[i].bound = qfalse;
			s_bots[i].characterHandle = 0;
			SV_Lua_BotSetDefaultProfile( s_bots[i].profileValues );
		}
	}

	memset( &s_characters[characterHandle], 0, sizeof( s_characters[characterHandle] ) );
	s_characters[characterHandle].profileRef = LUA_NOREF;
}

float SV_Lua_CharacteristicBFloat( int characterHandle, int index, float min, float max ) {
	float t;

	if ( max < min ) {
		const float swap = min;
		min = max;
		max = swap;
	}

	if ( characterHandle <= 0
	     || characterHandle >= SV_WB_MAX_CHARACTERS
	     || !s_characters[characterHandle].inuse
	     || index < 0 || index >= SV_WB_MAX_INDEX ) {
		return min + ( max - min ) * 0.5f;
	}

	if ( s_characters[characterHandle].floatValid[index] ) {
		t = s_characters[characterHandle].floatValues[index];
	} else {
		t = SV_Lua_DefaultCharacteristicNormalized( index, s_characters[characterHandle].skillNormalized );
	}

	// dual-semantic: normalized [0,1] -> lerp; raw engineering value -> clamp to [min,max]
	if ( t >= 0.0f && t <= 1.0f ) {
		return min + ( max - min ) * t;
	}
	if ( t < min ) return min;
	if ( t > max ) return max;
	return t;
}

void SV_Lua_CharacteristicString( int characterHandle, int index, char *buf, int size ) {
	if ( !buf || size <= 0 ) {
		return;
	}

	buf[0] = '\0';

	if ( characterHandle <= 0 || characterHandle >= SV_WB_MAX_CHARACTERS ) {
		return;
	}

	if ( !s_characters[characterHandle].inuse ) {
		return;
	}

	if ( index < 0 || index >= SV_WB_MAX_INDEX ) {
		return;
	}

	if ( s_characters[characterHandle].stringValues[index][0] ) {
		Q_strncpyz( buf, s_characters[characterHandle].stringValues[index], size );
	}
}

int SV_Lua_BindBot( int clientNum, int characterHandle ) {
	if ( !s_botLua ) {
		return qfalse;
	}

	if ( clientNum < 0 || clientNum >= MAX_CLIENTS ) {
		return qfalse;
	}

	if ( characterHandle <= 0 || characterHandle >= SV_WB_MAX_CHARACTERS || !s_characters[characterHandle].inuse ) {
		return qfalse;
	}

	s_bots[clientNum].bound = qtrue;
	s_bots[clientNum].characterHandle = characterHandle;
	SV_Lua_BotSetDefaultProfile( s_bots[clientNum].profileValues );
	SV_Lua_BotInitCachedProfile( clientNum );
	return qtrue;
}

int SV_Lua_BotThink( int clientNum, float thinktime ) {
	lua_State *L;
	int status;

	if ( !s_botLua ) {
		return qfalse;
	}

	if ( clientNum < 0 || clientNum >= MAX_CLIENTS ) {
		return qfalse;
	}

	if ( !s_bots[clientNum].bound ) {
		return qfalse;
	}

	L = s_botLua;
	lua_settop( L, 0 );
	lua_getglobal( L, "q3now_bot_think" );
	if ( !lua_isfunction( L, -1 ) ) {
		lua_settop( L, 0 );
		return qtrue;
	}

	lua_pushinteger( L, clientNum );
	lua_pushnumber( L, thinktime );
	status = lua_pcall( L, 2, 0, 0 );
	if ( status != 0 ) {
		const char *err = lua_tostring( L, -1 );
		Com_Printf( S_COLOR_YELLOW "BotLua: think error for client %d (%s)\n", clientNum, err ? err : "unknown" );
		lua_settop( L, 0 );
		return qfalse;
	}

	lua_settop( L, 0 );

	return qtrue;
}

float SV_Lua_BotProfileField( int clientNum, int field ) {
	float defaults[WB_PROFILE_MAX];

	SV_Lua_BotSetDefaultProfile( defaults );

	if ( field < 0 || field >= WB_PROFILE_MAX ) {
		return 0.0f;
	}

	if ( clientNum < 0 || clientNum >= MAX_CLIENTS || !s_bots[clientNum].bound ) {
		return defaults[field];
	}

	return s_bots[clientNum].profileValues[field];
}

// Walk the pre-cached attack index list and return the first attack whose weapon
// is present and has ammo.  Gauntlet attacks are always usable regardless of
// inventory.  No Lua state access at runtime — all resolution happened at load.
int SV_Lua_BotPickWeapon( int clientNum, const wbCombatCtx_t *ctx, char *weaponKey, int weaponKeySize ) {
	int characterHandle;
	int i;
	int idx;
	int weapon;

	if ( weaponKey && weaponKeySize > 0 ) {
		Q_strncpyz( weaponKey, "mg", weaponKeySize );
	}

	if ( !ctx ) {
		return qfalse;
	}

	if ( clientNum < 0 || clientNum >= MAX_CLIENTS || !s_bots[clientNum].bound ) {
		return qfalse;
	}

	characterHandle = s_bots[clientNum].characterHandle;
	if ( characterHandle <= 0 || characterHandle >= SV_WB_MAX_CHARACTERS ) {
		return qfalse;
	}

	if ( !s_characters[characterHandle].inuse ) {
		return qfalse;
	}

	{
		int debugWeapon = Cvar_VariableIntegerValue( "sv_botDebugWeapon" );
		if ( debugWeapon ) {
			Com_Printf( "^3[BotPickWeapon] client=%d cachedAttackCount=%d\n",
				clientNum, s_characters[characterHandle].cachedAttackCount );
		}

		for ( i = 0; i < s_characters[characterHandle].cachedAttackCount; i++ ) {
			idx    = s_characters[characterHandle].cachedAttacks[i].attackIndex;
			weapon = bg_attacklist[idx].weapon;

			if ( debugWeapon ) {
				Com_Printf( "  [%d] shortname='%s' weapon=%d aim_height=%.0f has_weapon=%d has_ammo=%d\n",
					i,
					bg_attacklist[idx].shortname,
					weapon,
					s_characters[characterHandle].cachedAttacks[i].aimHeight,
					( weapon == WP_GAUNTLET ) ? 1 : ( weapon > 0 && weapon < WP_NUM_WEAPONS ? ctx->weapons[weapon] : -1 ),
					( weapon == WP_GAUNTLET ) ? 1 : ( weapon > 0 && weapon < WP_NUM_WEAPONS ? ctx->ammo[weapon] : -1 ) );
			}

			if ( weapon == WP_GAUNTLET ) {
				// gauntlet: always usable, no inventory check
				if ( debugWeapon ) {
					Com_Printf( "  ^2=> selected '%s' (gauntlet fallback)\n", bg_attacklist[idx].shortname );
				}
				if ( weaponKey && weaponKeySize > 0 ) {
					Q_strncpyz( weaponKey, bg_attacklist[idx].shortname, weaponKeySize );
				}
				return qtrue;
			}

			if ( ctx->weapons[weapon] && ctx->ammo[weapon] > 0 ) {
				if ( debugWeapon ) {
					Com_Printf( "  ^2=> selected '%s' weapon=%d\n", bg_attacklist[idx].shortname, weapon );
				}
				if ( weaponKey && weaponKeySize > 0 ) {
					Q_strncpyz( weaponKey, bg_attacklist[idx].shortname, weaponKeySize );
				}
				return qtrue;
			}
		}
	}

	if ( Cvar_VariableIntegerValue( "sv_botDebugWeapon" ) ) {
		Com_Printf( "  ^1=> no attack selected (fallback to default 'mg')\n" );
	}

	return qfalse;
}

// Return the aimHeight baked for the first cached attack that uses weaponNum.
// Called from BotAimAtEnemy via a syscall so the C aim code stays Lua-free at runtime.
// Returns 28.0f (center mass) when no matching attack is found or the bot is not Lua-driven.
float SV_Lua_BotGetAttackAimHeight( int clientNum, int weaponNum ) {
	int characterHandle;
	int i, idx;

	if ( clientNum < 0 || clientNum >= MAX_CLIENTS || !s_bots[clientNum].bound ) {
		return 28.0f;
	}

	characterHandle = s_bots[clientNum].characterHandle;
	if ( characterHandle <= 0 || characterHandle >= SV_WB_MAX_CHARACTERS ) {
		return 28.0f;
	}

	if ( !s_characters[characterHandle].inuse ) {
		return 28.0f;
	}

	for ( i = 0; i < s_characters[characterHandle].cachedAttackCount; i++ ) {
		idx = s_characters[characterHandle].cachedAttacks[i].attackIndex;
		if ( bg_attacklist[idx].weapon == weaponNum ) {
			return s_characters[characterHandle].cachedAttacks[i].aimHeight;
		}
	}

	return 28.0f;
}

// Walk the bot's items[] priority list and score ctx->itemType by its position.
// Position 1 → score 100, position n → score ~(100/n).  Items not in the list
// return 0 (bot has no interest).  The caller applies a travel-time discount on
// top of this base score, so higher position = stronger pull even at distance.
int SV_Lua_BotEvalItem( int clientNum, const wbItemEvalCtx_t *ctx ) {
	lua_State *L;
	int characterHandle;
	int i;
	int n;
	const char *shortname;

	if ( !ctx || !ctx->itemType[0] || !s_botLua ) {
		return 0;
	}

	if ( clientNum < 0 || clientNum >= MAX_CLIENTS || !s_bots[clientNum].bound ) {
		return 0;
	}

	characterHandle = s_bots[clientNum].characterHandle;
	if ( characterHandle <= 0 || characterHandle >= SV_WB_MAX_CHARACTERS ) {
		return 0;
	}

	if ( !s_characters[characterHandle].inuse || s_characters[characterHandle].botRef == LUA_NOREF ) {
		return 0;
	}

	L = s_botLua;
	lua_settop( L, 0 );
	lua_rawgeti( L, LUA_REGISTRYINDEX, s_characters[characterHandle].botRef );
	if ( !lua_istable( L, -1 ) ) {
		lua_settop( L, 0 );
		return 0;
	}

	lua_getfield( L, -1, "items" );
	if ( !lua_istable( L, -1 ) ) {
		lua_settop( L, 0 );
		return 0;
	}

	n = (int)lua_objlen( L, -1 );
	if ( n <= 0 ) {
		lua_settop( L, 0 );
		return 0;
	}

	for ( i = 1; i <= n; i++ ) {
		lua_rawgeti( L, -1, i );
		if ( lua_type( L, -1 ) == LUA_TSTRING ) {
			shortname = lua_tostring( L, -1 );
			if ( Q_stricmp( shortname, ctx->itemType ) == 0 ) {
				int score = 100 - ( (i - 1) * 100 / n );
				lua_settop( L, 0 );
				return score > 0 ? score : 1;
			}
		}
		lua_pop( L, 1 );
	}

	lua_settop( L, 0 );
	return 0;
}

int SV_Lua_BotDecide( int clientNum, const wbDecideCtx_t *ctx, char *decision, int decisionSize ) {
	lua_State *L;
	int status;
	int characterHandle;

	if ( decision && decisionSize > 0 ) {
		Q_strncpyz( decision, "roam", decisionSize );
	}

	if ( !ctx ) {
		return qfalse;
	}

	if ( clientNum < 0 || clientNum >= MAX_CLIENTS || !s_bots[clientNum].bound ) {
		return qfalse;
	}

	characterHandle = s_bots[clientNum].characterHandle;

	// Route: character-specific Lua decide override (rare — only explicitly defined functions).
	if ( characterHandle > 0 && characterHandle < SV_WB_MAX_CHARACTERS &&
	     s_characters[characterHandle].inuse && s_characters[characterHandle].hasDecideFn &&
	     s_botLua ) {
		L = s_botLua;
		if ( SV_Lua_BotPushMethod( L, clientNum, "decide" ) ) {
			lua_createtable( L, 0, 12 );
			lua_pushnumber( L, ctx->health );
			lua_setfield( L, -2, "health" );
			lua_pushnumber( L, ctx->armor );
			lua_setfield( L, -2, "armor" );
			lua_pushboolean( L, ctx->enemyVisible ? 1 : 0 );
			lua_setfield( L, -2, "enemy_visible" );
			lua_pushnumber( L, ctx->enemyDist );
			lua_setfield( L, -2, "enemy_dist" );
			lua_pushnumber( L, ctx->enemyHealth );
			lua_setfield( L, -2, "enemy_health" );
			lua_pushstring( L, ctx->enemyWeapon );
			lua_setfield( L, -2, "enemy_weapon" );
			lua_pushboolean( L, ctx->underFire ? 1 : 0 );
			lua_setfield( L, -2, "under_fire" );
			lua_pushstring( L, ctx->lastKiller );
			lua_setfield( L, -2, "last_killer" );
			lua_pushstring( L, ctx->currentEnemy );
			lua_setfield( L, -2, "current_enemy" );
			lua_pushnumber( L, ctx->teamScore );
			lua_setfield( L, -2, "team_score" );
			lua_pushnumber( L, ctx->enemyScore );
			lua_setfield( L, -2, "enemy_score" );
			lua_pushstring( L, ctx->gametype );
			lua_setfield( L, -2, "gametype" );
			lua_pushnumber( L, ctx->timeLeft );
			lua_setfield( L, -2, "time_left" );

			status = lua_pcall( L, 2, 1, 0 );
			if ( status != 0 ) {
				if ( SV_Lua_ShouldPrintError( clientNum, WB_METHOD_DECIDE ) ) {
					const char *err = lua_tostring( L, -1 );
					Com_Printf( S_COLOR_YELLOW "BotLua: decide failed for client %d (%s)\n", clientNum, err ? err : "unknown" );
				}
				lua_settop( L, 0 );
				// Fall through to C logic on Lua error.
			} else if ( lua_isstring( L, -1 ) && decision && decisionSize > 0 ) {
				Q_strncpyz( decision, lua_tostring( L, -1 ), decisionSize );
				lua_settop( L, 0 );
				return qtrue;
			} else {
				lua_settop( L, 0 );
			}
		}
		// Fall through to C logic if method not found or returned non-string.
	}

	// C-driven decision from cached traits — zero Lua calls for bots without a decide override.
	{
		float *pv      = s_bots[clientNum].profileValues;
		float aggr     = pv[WB_PROFILE_AGGRESSION];
		float selfPres = pv[WB_PROFILE_SELF_PRESERVE];
		float camp     = pv[WB_PROFILE_CAMP_TENDENCY];

		if ( !ctx->enemyVisible ) {
			if ( camp > 0.5f && decision && decisionSize > 0 ) {
				Q_strncpyz( decision, "ambush", decisionSize );
			}
			// else default "roam" already written above
			return qtrue;
		}

		// Low health + high self-preservation outweighs aggression → retreat
		if ( ctx->health < 30 && selfPres > aggr * 0.5f ) {
			if ( decision && decisionSize > 0 ) {
				Q_strncpyz( decision, "retreat", decisionSize );
			}
			return qtrue;
		}

		// Enemy far away + enough aggression → chase
		if ( ctx->enemyDist > 1000.0f && aggr > 0.3f ) {
			if ( decision && decisionSize > 0 ) {
				Q_strncpyz( decision, "chase", decisionSize );
			}
			return qtrue;
		}

		// Default: enemy visible, fight
		if ( decision && decisionSize > 0 ) {
			Q_strncpyz( decision, "fight", decisionSize );
		}
		return qtrue;
	}
}

int SV_Lua_BotOnChat( int clientNum, const char *eventName, const wbChatCtx_t *ctx, char *outChat, int outChatSize ) {
	lua_State *L;
	int status;

	if ( outChat && outChatSize > 0 ) {
		outChat[0] = '\0';
	}

	if ( !ctx || !eventName || !eventName[0] || !s_botLua ) {
		return qfalse;
	}

	L = s_botLua;
	if ( !SV_Lua_BotPushMethod( L, clientNum, "on_chat" ) ) {
		return qfalse;
	}

	lua_pushstring( L, eventName );

	lua_createtable( L, 0, 11 );
	lua_pushstring( L, ctx->victim );
	lua_setfield( L, -2, "victim" );
	lua_pushstring( L, ctx->killer );
	lua_setfield( L, -2, "killer" );
	lua_pushstring( L, ctx->weapon );
	lua_setfield( L, -2, "weapon" );
	lua_pushnumber( L, ctx->distance );
	lua_setfield( L, -2, "distance" );
	lua_pushnumber( L, ctx->count );
	lua_setfield( L, -2, "count" );
	lua_pushboolean( L, ctx->won ? 1 : 0 );
	lua_setfield( L, -2, "won" );
	lua_pushnumber( L, ctx->score );
	lua_setfield( L, -2, "score" );
	lua_pushboolean( L, ctx->team ? 1 : 0 );
	lua_setfield( L, -2, "team" );
	lua_pushstring( L, ctx->sender );
	lua_setfield( L, -2, "sender" );
	lua_pushstring( L, ctx->text );
	lua_setfield( L, -2, "text" );
	lua_pushstring( L, ctx->map );
	lua_setfield( L, -2, "map" );
	lua_pushstring( L, ctx->gametype );
	lua_setfield( L, -2, "gametype" );

	status = lua_pcall( L, 3, 1, 0 );
	if ( status != 0 ) {
		if ( SV_Lua_ShouldPrintError( clientNum, WB_METHOD_ON_CHAT ) ) {
			const char *err = lua_tostring( L, -1 );
			Com_Printf( S_COLOR_YELLOW "BotLua: on_chat failed for client %d (%s)\n", clientNum, err ? err : "unknown" );
		}
		lua_settop( L, 0 );
		return qfalse;
	}

	if ( lua_isstring( L, -1 ) && outChat && outChatSize > 0 ) {
		Q_strncpyz( outChat, lua_tostring( L, -1 ), outChatSize );
		lua_settop( L, 0 );
		return outChat[0] ? qtrue : qfalse;
	}

	lua_settop( L, 0 );
	return qfalse;
}

// ─── Bot verification tools ──────────────────────────────────────────────────

// Characteristics that changed scale between old Q3 and new Lua system:
//   VIEW_MAXCHANGE: old [1,360] degrees → new normalized [0,1]
//   REACTIONTIME:  old [0,5] seconds   → new normalized [0,1]
// These are shown in output but excluded from the mismatch count.
static qboolean SV_BotVerifyIsLegacyScale( int idx ) {
	return ( idx == CHARACTERISTIC_VIEW_MAXCHANGE ||
	         idx == CHARACTERISTIC_REACTIONTIME ) ? qtrue : qfalse;
}

// Map "CHARACTERISTIC_X" string → CHARACTERISTIC_* index.
// Strips the 15-char prefix, lowercases, looks up in s_characteristicNames[].
// Returns -1 for unknown / string-only fields.
static int SV_BotVerifyCharNameToIndex( const char *name ) {
	char lower[64];
	int i, j;
	const char *src;

	if ( Q_stricmpn( name, "CHARACTERISTIC_", 15 ) != 0 ) {
		return -1;
	}
	src = name + 15;
	for ( j = 0; j < 63 && src[j]; j++ ) {
		lower[j] = tolower( (unsigned char)src[j] );
	}
	lower[j] = '\0';

	for ( i = 0; s_characteristicNames[i].name; i++ ) {
		if ( strcmp( s_characteristicNames[i].name, lower ) == 0 ) {
			return s_characteristicNames[i].index;
		}
	}
	return -1;
}

// Parse old _c.c botfile and interpolate values for the requested skill (1-5).
// Returns qtrue if the file was found and parsed.
static qboolean SV_BotVerifyParseOldFile(
    const char *charName,
    int requestedSkill,
    float outVals[SV_WB_MAX_INDEX],
    qboolean outValid[SV_WB_MAX_INDEX]
) {
	char path[MAX_QPATH];
	char *fileData;
	int fileLen;
	const char *p;
	const char *tok;
	int curSkill, inBlock, idx, i;

	float blocks[5][SV_WB_MAX_INDEX];
	qboolean bValid[5][SV_WB_MAX_INDEX];
	qboolean bPresent[5];

	Com_sprintf( path, sizeof( path ), "botfiles/bots/%s_c.c", charName );
	fileLen = FS_ReadFile( path, (void **)&fileData );
	if ( fileLen <= 0 || !fileData ) {
		return qfalse;
	}

	memset( blocks, 0, sizeof( blocks ) );
	memset( bValid, 0, sizeof( bValid ) );
	memset( bPresent, 0, sizeof( bPresent ) );

	p = fileData;
	curSkill = -1;
	inBlock = 0;

	ComParser parser = { 0 };
	while ( *(tok = COM_ParseExt( &parser, &p, qtrue )) ) {
		// Skip preprocessor directives (#include etc.)
		if ( tok[0] == '#' ) {
			COM_ParseExt( &parser, &p, qfalse );
			continue;
		}

		if ( !strcmp( tok, "{" ) ) {
			inBlock = 1;
			continue;
		}
		if ( !strcmp( tok, "}" ) ) {
			inBlock = 0;
			curSkill = -1;
			continue;
		}

		if ( !inBlock && !strcmp( tok, "skill" ) ) {
			const char *numStr = COM_ParseExt( &parser, &p, qfalse );
			if ( numStr && *numStr ) {
				int sn = atoi( numStr );
				if ( sn >= 1 && sn <= 5 ) {
					curSkill = sn;
					bPresent[sn - 1] = qtrue;
				}
			}
			continue;
		}

		if ( inBlock && curSkill >= 1 && curSkill <= 5 ) {
			idx = SV_BotVerifyCharNameToIndex( tok );
			// Read value token (COM_ParseExt strips quotes from strings)
			const char *valStr = COM_ParseExt( &parser, &p, qfalse );
			if ( !valStr || !*valStr ) {
				break;
			}
			if ( idx >= 0 && idx < SV_WB_MAX_INDEX ) {
				char fc = valStr[0];
				// Numeric values start with digit, minus, or dot
				if ( (fc >= '0' && fc <= '9') || fc == '-' || fc == '.' ) {
					blocks[curSkill - 1][idx] = (float)atof( valStr );
					bValid[curSkill - 1][idx] = qtrue;
				}
			}
			continue;
		}
	}

	FS_FreeFile( fileData );

	// Interpolate between the two adjacent skill blocks
	{
		int lowerSkill = -1, upperSkill = -1;
		float t;

		for ( i = 1; i <= 5; i++ ) {
			if ( bPresent[i - 1] ) {
				if ( i <= requestedSkill ) lowerSkill = i;
				if ( i >= requestedSkill && upperSkill == -1 ) upperSkill = i;
			}
		}

		if ( lowerSkill == -1 && upperSkill == -1 ) {
			FS_FreeFile( fileData );
			return qfalse;
		}
		if ( lowerSkill == -1 ) lowerSkill = upperSkill;
		if ( upperSkill == -1 ) upperSkill = lowerSkill;

		t = ( lowerSkill == upperSkill ) ? 0.0f
		    : (float)( requestedSkill - lowerSkill ) / (float)( upperSkill - lowerSkill );

		memset( outVals, 0, sizeof( float ) * SV_WB_MAX_INDEX );
		memset( outValid, 0, sizeof( qboolean ) * SV_WB_MAX_INDEX );

		for ( idx = 0; idx < SV_WB_MAX_INDEX; idx++ ) {
			qboolean inLo = bValid[lowerSkill - 1][idx];
			qboolean inHi = bValid[upperSkill - 1][idx];
			float loVal, hiVal;

			if ( !inLo && !inHi ) continue;

			outValid[idx] = qtrue;
			loVal = inLo ? blocks[lowerSkill - 1][idx] : 0.0f;
			hiVal = inHi ? blocks[upperSkill - 1][idx] : 0.0f;
			outVals[idx] = loVal + ( hiVal - loVal ) * t;
		}
	}

	return qtrue;
}

// Load Lua characteristic_values for charName at skillNorm directly from Lua,
// without touching the s_characters[] cache (so live bots are unaffected).
static qboolean SV_BotVerifyLoadLuaValues(
    const char *charName,
    float skillNorm,
    float newVals[SV_WB_MAX_INDEX],
    qboolean newValid[SV_WB_MAX_INDEX]
) {
	lua_State *L = s_botLua;
	int status;

	if ( !L ) {
		return qfalse;
	}

	lua_settop( L, 0 );
	lua_getglobal( L, "q3now" );
	lua_getfield( L, -1, "load_character" );
	lua_remove( L, -2 );
	if ( !lua_isfunction( L, -1 ) ) {
		lua_settop( L, 0 );
		return qfalse;
	}
	lua_pushstring( L, charName );
	lua_pushnumber( L, (double)skillNorm );
	status = lua_pcall( L, 2, 1, 0 );
	if ( status != 0 ) {
		const char *err = lua_tostring( L, -1 );
		Com_Printf( S_COLOR_YELLOW "bot_verify_character: Lua error for '%s': %s\n",
		            charName, err ? err : "unknown" );
		lua_settop( L, 0 );
		return qfalse;
	}

	if ( !lua_istable( L, -1 ) ) {
		lua_settop( L, 0 );
		return qfalse;
	}

	memset( newVals, 0, sizeof( float ) * SV_WB_MAX_INDEX );
	memset( newValid, 0, sizeof( qboolean ) * SV_WB_MAX_INDEX );

	lua_getfield( L, -1, "characteristic_values" );
	if ( lua_istable( L, -1 ) ) {
		lua_pushnil( L );
		while ( lua_next( L, -2 ) ) {
			if ( lua_type( L, -2 ) == LUA_TNUMBER && lua_type( L, -1 ) == LUA_TNUMBER ) {
				int key = (int)lua_tointeger( L, -2 );
				if ( key >= 0 && key < SV_WB_MAX_INDEX ) {
					newVals[key] = (float)lua_tonumber( L, -1 );
					newValid[key] = qtrue;
				}
			}
			lua_pop( L, 1 );
		}
	}
	lua_settop( L, 0 );
	return qtrue;
}

// bot_verify_character <name> <skill>
// Parses the legacy botfiles/bots/<name>_c.c, loads the Lua character values,
// and prints a side-by-side comparison flagging any mismatch > 0.01.
void SV_BotVerifyCharacter_f( void ) {
	const char *charName;
	int skill, i;
	float skillNorm;
	float oldVals[SV_WB_MAX_INDEX], newVals[SV_WB_MAX_INDEX];
	qboolean oldValid[SV_WB_MAX_INDEX], newValid[SV_WB_MAX_INDEX];
	int matchCount = 0, mismatchCount = 0, skipCount = 0;

	if ( Cmd_Argc() < 3 ) {
		Com_Printf( "Usage: bot_verify_character <name> <skill>\n"
		            "  name  - character name (e.g., sarge, daemia, grunt)\n"
		            "  skill - integer skill level 1-5\n" );
		return;
	}

	charName = Cmd_Argv( 1 );
	skill = atoi( Cmd_Argv( 2 ) );

	if ( skill < 1 || skill > 5 ) {
		Com_Printf( "bot_verify_character: skill must be 1-5\n" );
		return;
	}

	skillNorm = (float)( skill - 1 ) * 0.25f;

	if ( !SV_BotVerifyParseOldFile( charName, skill, oldVals, oldValid ) ) {
		Com_Printf( "bot_verify_character: no legacy botfile for '%s' "
		            "(botfiles/bots/%s_c.c not found)\n",
		            charName, charName );
		Com_Printf( "  Expected for new characters (keel, anarki, crash).\n" );
		return;
	}

	if ( !SV_BotVerifyLoadLuaValues( charName, skillNorm, newVals, newValid ) ) {
		Com_Printf( "bot_verify_character: failed to load Lua character '%s'\n", charName );
		return;
	}

	Com_Printf( "\n^3bot_verify_character: %s  skill=%d (norm=%.2f)\n\n",
	            charName, skill, skillNorm );
	Com_Printf( "%-34s  %-8s  %-8s  %s\n",
	            "characteristic", "old", "new", "status" );
	Com_Printf( "%-34s  %-8s  %-8s  ------\n",
	            "----------------------------------", "--------", "--------" );

	for ( i = 0; s_characteristicNames[i].name; i++ ) {
		int idx = s_characteristicNames[i].index;
		const char *cname = s_characteristicNames[i].name;
		float oldVal, newVal, diff;

		if ( !oldValid[idx] && !newValid[idx] ) {
			continue;
		}

		oldVal = oldValid[idx] ? oldVals[idx] : 0.0f;
		newVal = newValid[idx] ? newVals[idx] : 0.0f;
		diff = oldVal - newVal;
		if ( diff < 0.0f ) diff = -diff;

		if ( SV_BotVerifyIsLegacyScale( idx ) ) {
			Com_Printf( "%-34s  %-8.4f  %-8.4f  ^6SCALE_DIFF (legacy range)\n",
			            cname, oldVal, newVal );
			skipCount++;
		} else if ( oldValid[idx] && !newValid[idx] ) {
			float def = SV_Lua_DefaultCharacteristicNormalized( idx, skillNorm );
			float defDiff = oldVal - def;
			if ( defDiff < 0.0f ) defDiff = -defDiff;
			if ( defDiff > 0.01f ) {
				Com_Printf( "%-34s  %-8.4f  ^1(undef)^7   ^1MISSING (def=%.4f, diff=%.4f)\n",
				            cname, oldVal, def, defDiff );
				mismatchCount++;
			} else {
				Com_Printf( "%-34s  %-8.4f  %-8s  ^3MISSING (def~=old)\n",
				            cname, oldVal, "(undef)" );
				matchCount++;
			}
		} else if ( !oldValid[idx] && newValid[idx] ) {
			Com_Printf( "%-34s  %-8s  %-8.4f  ^2NEW_ONLY\n",
			            cname, "(none)", newVal );
			matchCount++;
		} else if ( diff > 0.01f ) {
			Com_Printf( "%-34s  %-8.4f  %-8.4f  ^1MISMATCH (diff=%.4f)\n",
			            cname, oldVal, newVal, diff );
			mismatchCount++;
		} else {
			Com_Printf( "%-34s  %-8.4f  %-8.4f  ^2OK\n",
			            cname, oldVal, newVal );
			matchCount++;
		}
	}

	Com_Printf( "\n^3Summary: %s skill=%d -- %d match, %d mismatch, %d scale-skipped\n",
	            charName, skill, matchCount, mismatchCount, skipCount );
	if ( mismatchCount == 0 ) {
		Com_Printf( "^2All comparable values match.\n\n" );
	} else {
		Com_Printf( "^1Fix %d mismatch(es) in modfiles/characters/%s/main.lua\n\n",
		            mismatchCount, charName );
	}
}

// bot_debug_weapons <botname|off>
// Sets the cvar that game code reads to enable per-frame weapon debug output.
void SV_BotDebugWeapons_f( void ) {
	if ( Cmd_Argc() < 2 ) {
		Com_Printf( "Usage: bot_debug_weapons <botname|off>\n"
		            "  botname - track weapon selection for this bot (100 frames)\n"
		            "  off     - disable weapon debug output\n" );
		return;
	}

	if ( !Q_stricmp( Cmd_Argv( 1 ), "off" ) ) {
		Cvar_Set( "bot_debug_weapon", "" );
		Com_Printf( "bot_debug_weapons: disabled\n" );
	} else {
		Cvar_Set( "bot_debug_weapon", Cmd_Argv( 1 ) );
		Com_Printf( "bot_debug_weapons: tracking '%s' for up to 100 frames\n", Cmd_Argv( 1 ) );
	}
}
