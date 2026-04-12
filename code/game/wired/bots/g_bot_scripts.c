#include "g_local.h"

#include "../botlib/botlib.h"
#include "../botlib/be_aas.h"
#include "../botlib/be_ea.h"
#include "../botlib/be_ai_char.h"
#include "../botlib/be_ai_chat.h"
#include "../botlib/be_ai_gen.h"
#include "../botlib/be_ai_goal.h"
#include "../botlib/be_ai_move.h"
#include "../botlib/be_ai_weap.h"

#include "ai_main.h"
#include "ai_chat.h"
#include "ai_dmq3.h"
#include "g_bot_scripts.h"
#include "chars.h"
#include "inv.h"

#include <math.h>

#define WB_MAX_NBG_DETOUR 120

typedef struct {
	const char *shortname;
	int weapon;
} wbWeaponMap_t;

static const wbWeaponMap_t s_weaponShortnameMap[] = {
	// weapon-level shortnames (legacy / pick_weapon return value)
	{ "g",   WP_GAUNTLET },
	{ "mg",  WP_MACHINEGUN },
	{ "sg",  WP_SHOTGUN },
	{ "gl",  WP_GRENADE_LAUNCHER },
	{ "rl",  WP_ROCKET_LAUNCHER },
	{ "lg",  WP_LIGHTNING_GUN },
	{ "rg",  WP_RAILGUN },
	{ "pr",  WP_PLASMA_RIFLE },
	{ "pg",  WP_PLASMA_RIFLE },
	// attack-level shortnames (new attacks[] priority list)
	{ "g1",  WP_GAUNTLET },
	{ "g2",  WP_GAUNTLET },
	{ "mg1", WP_MACHINEGUN },
	{ "mg2", WP_MACHINEGUN },
	{ "sg1", WP_SHOTGUN },
	{ "sg2", WP_SHOTGUN },
	{ "gl1", WP_GRENADE_LAUNCHER },
	{ "rl1", WP_ROCKET_LAUNCHER },
	{ "rl2", WP_ROCKET_LAUNCHER },
	{ "lg1", WP_LIGHTNING_GUN },
	{ "lg2", WP_LIGHTNING_GUN },
	{ "rg1", WP_RAILGUN },
	{ "pr1", WP_PLASMA_RIFLE },
};

static float WiredBots_Clamp01( float value ) {
	if ( value < 0.0f ) {
		return 0.0f;
	}
	if ( value > 1.0f ) {
		return 1.0f;
	}
	return value;
}

static float WiredBots_ClampNonNegative( float value ) {
	if ( value < 0.0f ) {
		return 0.0f;
	}
	return value;
}

static int WiredBots_HasWeapon( const bot_state_t *bs, int weapon ) {
	switch ( weapon ) {
		case WP_GAUNTLET:
			return qtrue;
		case WP_MACHINEGUN:
			return bs->inventory[INVENTORY_MACHINEGUN] > 0;
		case WP_SHOTGUN:
			return bs->inventory[INVENTORY_SHOTGUN] > 0;
		case WP_GRENADE_LAUNCHER:
			return bs->inventory[INVENTORY_GRENADE_LAUNCHER] > 0;
		case WP_ROCKET_LAUNCHER:
			return bs->inventory[INVENTORY_ROCKET_LAUNCHER] > 0;
		case WP_LIGHTNING_GUN:
			return bs->inventory[INVENTORY_LIGHTNING_GUN] > 0;
		case WP_RAILGUN:
			return bs->inventory[INVENTORY_RAILGUN] > 0;
		case WP_PLASMA_RIFLE:
			return bs->inventory[INVENTORY_PLASMA_RIFLE] > 0;
		default:
			return qfalse;
	}
}

static int WiredBots_AmmoForWeapon( const bot_state_t *bs, int weapon ) {
	switch ( weapon ) {
		case WP_GAUNTLET:
			return 999;
		case WP_MACHINEGUN:
			return bs->inventory[INVENTORY_BULLETS];
		case WP_SHOTGUN:
			return bs->inventory[INVENTORY_SHELLS];
		case WP_GRENADE_LAUNCHER:
			return bs->inventory[INVENTORY_GRENADES];
		case WP_ROCKET_LAUNCHER:
			return bs->inventory[INVENTORY_ROCKETS];
		case WP_LIGHTNING_GUN:
			return bs->inventory[INVENTORY_LIGHTNING];
		case WP_RAILGUN:
			return bs->inventory[INVENTORY_SLUGS];
		case WP_PLASMA_RIFLE:
			return bs->inventory[INVENTORY_CELLS];
		default:
			return 0;
	}
}

static qboolean WiredBots_WeaponUsable( const bot_state_t *bs, int weapon ) {
	if ( !WiredBots_HasWeapon( bs, weapon ) ) {
		return qfalse;
	}
	if ( weapon == WP_GAUNTLET ) {
		return qtrue;
	}
	return WiredBots_AmmoForWeapon( bs, weapon ) > 0;
}

static const wbWeaponMap_t *WiredBots_WeaponMapFromShortName( const char *shortname ) {
	int i;

	if ( !shortname || !shortname[0] ) {
		return NULL;
	}

	for ( i = 0; i < (int)( sizeof( s_weaponShortnameMap ) / sizeof( s_weaponShortnameMap[0] ) ); i++ ) {
		if ( !Q_stricmp( shortname, s_weaponShortnameMap[i].shortname ) ) {
			return &s_weaponShortnameMap[i];
		}
	}

	return NULL;
}

float WiredBots_ProfileFieldOr( bot_state_t *bs, int field, float fallback ) {
	if ( !bs || !bs->wiredBotsActive || field < 0 || field >= WB_PROFILE_MAX ) {
		return fallback;
	}
	return trap_BotLuaBotProfileField( bs->client, field );
}

// Return the aim height (units above entity origin) baked for the attack the bot
// currently has equipped.  Falls through to 28.0f (center mass) if not Lua-driven
// or no matching attack entry is cached.
float WiredBots_GetCurrentAttackAimHeight( bot_state_t *bs ) {
	if ( !bs || !bs->wiredBotsActive ) {
		return 28.0f;
	}
	return trap_BotLuaBotGetAttackAimHeight( bs->client, bs->weaponnum );
}

static float WiredBots_WeaponBaseScore( int weapon ) {
	switch ( weapon ) {
		case WP_GAUNTLET:
			return 0.10f;
		case WP_MACHINEGUN:
			return 0.45f;
		case WP_SHOTGUN:
			return 0.55f;
		case WP_GRENADE_LAUNCHER:
			return 0.70f;
		case WP_ROCKET_LAUNCHER:
			return 0.85f;
		case WP_LIGHTNING_GUN:
			return 0.90f;
		case WP_RAILGUN:
			return 0.80f;
		case WP_PLASMA_RIFLE:
			return 0.75f;
		default:
			return 0.0f;
	}
}

static float WiredBots_WeaponIdealRange( int weapon ) {
	switch ( weapon ) {
		case WP_GAUNTLET:
			return 64.0f;
		case WP_MACHINEGUN:
			return 700.0f;
		case WP_SHOTGUN:
			return 250.0f;
		case WP_GRENADE_LAUNCHER:
			return 380.0f;
		case WP_ROCKET_LAUNCHER:
			return 500.0f;
		case WP_LIGHTNING_GUN:
			return 480.0f;
		case WP_RAILGUN:
			return 1000.0f;
		case WP_PLASMA_RIFLE:
			return 420.0f;
		default:
			return 300.0f;
	}
}

static float WiredBots_WeaponRangeScore( int weapon, float distance ) {
	float ideal;
	float span;
	float delta;
	float score;

	if ( distance <= 0.0f ) {
		return 0.5f;
	}

	ideal = WiredBots_WeaponIdealRange( weapon );
	span = ideal * 0.75f + 80.0f;
	delta = fabsf( distance - ideal );
	score = 1.0f - ( delta / span );

	return WiredBots_Clamp01( score );
}

static float WiredBots_AmmoScore( int weapon, int ammo ) {
	if ( weapon == WP_GAUNTLET ) {
		return 1.0f;
	}
	if ( ammo <= 0 ) {
		return 0.0f;
	}
	if ( ammo >= 40 ) {
		return 1.0f;
	}
	return 0.25f + 0.75f * ( ammo / 40.0f );
}


static int WiredBots_WeaponFromKey( const char *weaponKey ) {
	const wbWeaponMap_t *map;

	if ( !weaponKey || !weaponKey[0] ) {
		return WP_NONE;
	}

	map = WiredBots_WeaponMapFromShortName( weaponKey );
	if ( map ) {
		return map->weapon;
	}

	return WP_NONE;
}

static void WiredBots_FillCombatCtx( bot_state_t *bs, wbCombatCtx_t *ctx ) {
	int weapon;

	Com_Memset( ctx, 0, sizeof( *ctx ) );
	ctx->enemyDist = fabsf( (float)bs->inventory[ENEMY_HORIZONTAL_DIST] );
	ctx->enemyHealth = -1;
	ctx->selfHealth = bs->inventory[INVENTORY_HEALTH];
	ctx->selfArmor = bs->inventory[INVENTORY_ARMOR];

	for ( weapon = WP_GAUNTLET; weapon < WP_NUM_WEAPONS; weapon++ ) {
		ctx->weapons[weapon] = WiredBots_HasWeapon( bs, weapon ) ? 1 : 0;
		ctx->ammo[weapon] = WiredBots_AmmoForWeapon( bs, weapon );
	}
}

int WiredBots_ChooseWeapon( bot_state_t *bs, int fallbackWeapon ) {
	wbCombatCtx_t ctx;
	char weaponKey[WB_WEAPONKEY_STR];
	int weapon;
	int bestWeapon;
	float bestScore;
	float enemyDistance;
	const char *chosenKey;
	int chosenWeapon;

	if ( !bs || !bs->wiredBotsActive ) {
		return fallbackWeapon;
	}

	WiredBots_FillCombatCtx( bs, &ctx );

	chosenKey = NULL;
	chosenWeapon = 0;

	if ( trap_BotLuaBotPickWeapon( bs->client, &ctx, weaponKey, sizeof( weaponKey ) ) ) {
		weapon = WiredBots_WeaponFromKey( weaponKey );
		if ( WiredBots_WeaponUsable( bs, weapon ) ) {
			chosenKey = weaponKey;
			chosenWeapon = weapon;
			goto debug_print;
		}
	}

	enemyDistance = fabsf( (float)bs->inventory[ENEMY_HORIZONTAL_DIST] );

	bestWeapon = WiredBots_WeaponUsable( bs, fallbackWeapon ) ? fallbackWeapon : WP_GAUNTLET;
	bestScore = -1.0f;

	for ( weapon = WP_GAUNTLET; weapon < WP_NUM_WEAPONS; weapon++ ) {
		float baseScore;
		float rangeScore;
		float ammoScore;
		float score;
		int ammo;
		if ( !WiredBots_WeaponUsable( bs, weapon ) ) {
			continue;
		}

		ammo = WiredBots_AmmoForWeapon( bs, weapon );
		baseScore = WiredBots_WeaponBaseScore( weapon );
		rangeScore = WiredBots_WeaponRangeScore( weapon, enemyDistance );
		ammoScore = WiredBots_AmmoScore( weapon, ammo );

		score = baseScore * 0.35f + rangeScore * 0.40f + ammoScore * 0.25f;

		if ( weapon == bs->cur_ps.weapon && bs->cur_ps.weaponstate == WEAPON_READY ) {
			score += 0.05f;
		}

		if ( score > bestScore ) {
			bestScore = score;
			bestWeapon = weapon;
		}
	}

	chosenKey = "(dps)";
	chosenWeapon = bestWeapon;

debug_print:
	{
		static int s_dbgFrames = 0;
		static char s_dbgPrev[MAX_NAME_LENGTH];
		char dbgTarget[MAX_NAME_LENGTH];
		char botName[MAX_NAME_LENGTH];

		trap_Cvar_VariableStringBuffer( "bot_debug_weapon", dbgTarget, sizeof( dbgTarget ) );
		ClientName( bs->client, botName, sizeof( botName ) );

		if ( dbgTarget[0] && Q_stricmp( dbgTarget, "off" ) != 0 ) {
			if ( Q_stricmpn( dbgTarget, s_dbgPrev, MAX_NAME_LENGTH ) != 0 ) {
				s_dbgFrames = 0;
				Q_strncpyz( s_dbgPrev, dbgTarget, sizeof( s_dbgPrev ) );
			}
			if ( Q_stricmpn( botName, dbgTarget, strlen( dbgTarget ) ) == 0 ) {
				if ( s_dbgFrames < 100 ) {
					int ammo = WiredBots_AmmoForWeapon( bs, chosenWeapon );
					int inBitmask = ( bs->cur_ps.stats[STAT_WEAPONS] >> chosenWeapon ) & 1;
					G_Printf( "^5[bot_debug_weapons] frame=%d bot=%s key=%s wp=%d ammo=%d bitmask=%d\n",
					          s_dbgFrames, botName,
					          chosenKey ? chosenKey : "?",
					          chosenWeapon, ammo, inBitmask );
					s_dbgFrames++;
				} else {
					trap_Cvar_Set( "bot_debug_weapon", "" );
					s_dbgPrev[0] = '\0';
					s_dbgFrames = 0;
					G_Printf( "^5[bot_debug_weapons] reached 100 frames, stopping.\n" );
				}
			}
		} else if ( s_dbgPrev[0] ) {
			s_dbgPrev[0] = '\0';
			s_dbgFrames = 0;
		}
	}

	return chosenWeapon;
}

float WiredBots_Aggression( bot_state_t *bs ) {
	float health;
	float armor;
	float weaponPower;
	float enemyPressure;
	float aggression;
	float traitAggression;

	if ( !bs ) {
		return 0.0f;
	}

	traitAggression = WiredBots_Clamp01( WiredBots_ProfileFieldOr( bs, WB_PROFILE_AGGRESSION, 0.5f ) );
	health = WiredBots_Clamp01( bs->inventory[INVENTORY_HEALTH] / 125.0f );
	armor = WiredBots_Clamp01( bs->inventory[INVENTORY_ARMOR] / 125.0f );
	weaponPower = WiredBots_Clamp01( WiredBots_WeaponBaseScore( bs->weaponnum ) );
	enemyPressure = 1.0f - WiredBots_Clamp01( fabsf( (float)bs->inventory[ENEMY_HORIZONTAL_DIST] ) / 1000.0f );

	aggression = 100.0f * (
		traitAggression * 0.45f +
		health * 0.20f +
		armor * 0.15f +
		weaponPower * 0.15f +
		enemyPressure * 0.05f
	);

	if ( bs->inventory[INVENTORY_QUAD] ) {
		if ( aggression < 85.0f ) {
			aggression = 85.0f;
		}
	}

	if ( bs->inventory[ENEMY_HEIGHT] > 200 ) {
		aggression *= 0.35f;
	}

	if ( aggression < 0.0f ) {
		aggression = 0.0f;
	} else if ( aggression > 100.0f ) {
		aggression = 100.0f;
	}

	return aggression;
}

static int WiredBots_Decide( bot_state_t *bs, char *decision, int decisionSize ) {
	wbDecideCtx_t ctx;

	if ( !bs || !decision || decisionSize <= 0 || !bs->wiredBotsActive ) {
		return qfalse;
	}

	Com_Memset( &ctx, 0, sizeof( ctx ) );
	ctx.health = bs->inventory[INVENTORY_HEALTH];
	ctx.armor = bs->inventory[INVENTORY_ARMOR];
	ctx.enemyVisible = ( bs->enemy >= 0 && BotEntityVisible( bs->entitynum, bs->eye, bs->viewangles, 360, bs->enemy ) > 0.0f ) ? 1 : 0;
	ctx.enemyDist = fabsf( (float)bs->inventory[ENEMY_HORIZONTAL_DIST] );
	ctx.enemyHealth = -1;
	ctx.underFire = ( bs->lastframe_health > bs->inventory[INVENTORY_HEALTH] ) ? 1 : 0;
	ctx.teamScore = level.teamScores[BotTeam( bs )];
	ctx.enemyScore = level.teamScores[( BotTeam( bs ) == TEAM_RED ) ? TEAM_BLUE : TEAM_RED];
	ctx.timeLeft = ( g_timelimit.integer > 0 ) ? ( g_timelimit.integer * 60 - level.time / 1000 ) : 0;

	if ( ctx.timeLeft < 0 ) {
		ctx.timeLeft = 0;
	}

	switch ( gametype ) {
		case GT_DEATHMATCH: Q_strncpyz( ctx.gametype, "ffa", sizeof( ctx.gametype ) ); break;
		case GT_DUEL: Q_strncpyz( ctx.gametype, "duel", sizeof( ctx.gametype ) ); break;
		case GT_TDM: Q_strncpyz( ctx.gametype, "tdm", sizeof( ctx.gametype ) ); break;
		case GT_CTF: Q_strncpyz( ctx.gametype, "ctf", sizeof( ctx.gametype ) ); break;
		case GT_1FCTF: Q_strncpyz( ctx.gametype, "1fctf", sizeof( ctx.gametype ) ); break;
		default: Q_strncpyz( ctx.gametype, "other", sizeof( ctx.gametype ) ); break;
	}

	if ( bs->enemy >= 0 && bs->enemy < MAX_CLIENTS ) {
		ClientName( bs->enemy, ctx.currentEnemy, sizeof( ctx.currentEnemy ) );
	}
	if ( bs->lastkilledby >= 0 && bs->lastkilledby < MAX_CLIENTS ) {
		ClientName( bs->lastkilledby, ctx.lastKiller, sizeof( ctx.lastKiller ) );
	}

	if ( bs->enemy >= 0 ) {
		aas_entityinfo_t entinfo;
		BotEntityInfo( bs->enemy, &entinfo );
		if ( entinfo.valid ) {
			ctx.enemyHealth = g_entities[bs->enemy].health;
			if ( entinfo.weapon > WP_NONE && entinfo.weapon < WP_NUM_WEAPONS && bg_weaponlist[entinfo.weapon].shortname ) {
				Q_strncpyz( ctx.enemyWeapon, bg_weaponlist[entinfo.weapon].shortname, sizeof( ctx.enemyWeapon ) );
			}
		}
	}

	{
		int result = trap_BotLuaBotDecide( bs->client, &ctx, decision, decisionSize );

		if ( trap_Cvar_VariableIntegerValue( "sv_botDebugDecide" ) ) {
			static float s_decideLogTime[MAX_CLIENTS];
			if ( FloatTime() - s_decideLogTime[bs->client] > 2.0f ) {
				s_decideLogTime[bs->client] = FloatTime();
				G_Printf( "^5[Decide] client=%d result='%s'(%d) ev=%d ed=%.0f hp=%d uf=%d\n",
					bs->client, result ? decision : "<none>", result,
					ctx.enemyVisible, ctx.enemyDist, ctx.health, ctx.underFire );
			}
		}

		return result;
	}
}

int WiredBots_WantsToRetreat( bot_state_t *bs ) {
	char decision[32];
	float danger;
	float selfPreserve;

	if ( !bs ) {
		return qfalse;
	}

	// Trust the Lua decide result completely when available.
	// "roam" (no visible enemy) must NOT fall through to the danger formula —
	// that formula returns qtrue for MG+no-armor bots, blocking all combat.
	if ( WiredBots_Decide( bs, decision, sizeof( decision ) ) ) {
		return !Q_stricmp( decision, "retreat" );
	}

	// Lua bot with no decide() — do NOT use the legacy danger formula.
	// The formula yields danger > 1.0 for MG+no-armor, permanently blocking combat.
	if ( bs->wiredBotsActive ) {
		return qfalse;
	}

	selfPreserve = WiredBots_Clamp01( WiredBots_ProfileFieldOr( bs, WB_PROFILE_SELF_PRESERVE, 0.5f ) );
	danger = 1.0f - WiredBots_Clamp01( WiredBots_Aggression( bs ) / 100.0f );

	if ( bs->inventory[INVENTORY_HEALTH] < 40 ) {
		danger += 0.30f;
	} else if ( bs->inventory[INVENTORY_HEALTH] < 70 ) {
		danger += 0.15f;
	}

	if ( bs->inventory[INVENTORY_ARMOR] < 25 ) {
		danger += 0.15f;
	}

	if ( bs->weaponnum == WP_GAUNTLET || bs->weaponnum == WP_MACHINEGUN ) {
		danger += 0.15f;
	}

	danger *= 0.70f + selfPreserve;

	return danger > 0.65f;
}

int WiredBots_WantsToChase( bot_state_t *bs ) {
	char decision[32];
	float chaseDrive;
	float vengefulness;

	if ( !bs ) {
		return qfalse;
	}

	if ( WiredBots_Decide( bs, decision, sizeof( decision ) ) ) {
		if ( !Q_stricmp( decision, "chase" ) ) {
			return qtrue;
		}
		if ( !Q_stricmp( decision, "retreat" ) || !Q_stricmp( decision, "fight" ) || !Q_stricmp( decision, "roam" ) ) {
			return qfalse;
		}
	}

	vengefulness = WiredBots_Clamp01( WiredBots_ProfileFieldOr( bs, WB_PROFILE_VENGEFULNESS, 0.5f ) );
	chaseDrive = WiredBots_Clamp01( WiredBots_Aggression( bs ) / 100.0f );
	chaseDrive *= 0.70f + vengefulness;

	if ( bs->inventory[INVENTORY_HEALTH] < 45 ) {
		chaseDrive -= 0.25f;
	}

	if ( bs->inventory[ENEMY_HORIZONTAL_DIST] > 1200 ) {
		chaseDrive -= 0.10f;
	}

	return chaseDrive > 0.60f;
}

static void WiredBots_ItemTypeName( const gitem_t *item, char *itemType, int itemTypeSize ) {
	if ( !item || !itemType || itemTypeSize <= 0 ) {
		return;
	}

	Q_strncpyz( itemType, "item", itemTypeSize );

	switch ( item->giType ) {
		case IT_HEALTH:
			if ( item->quantity >= 100 ) {
				Q_strncpyz( itemType, "health_mega", itemTypeSize );
			} else if ( item->quantity >= 25 ) {
				Q_strncpyz( itemType, "health_large", itemTypeSize );
			} else if ( item->quantity >= 5 ) {
				Q_strncpyz( itemType, "health_medium", itemTypeSize );
			} else {
				Q_strncpyz( itemType, "health_small", itemTypeSize );
			}
			break;
		case IT_ARMOR:
			if ( item->quantity >= 100 ) {
				Q_strncpyz( itemType, "armor_red", itemTypeSize );
			} else if ( item->quantity >= 50 ) {
				Q_strncpyz( itemType, "armor_yellow", itemTypeSize );
			} else {
				Q_strncpyz( itemType, "armor_shard", itemTypeSize );
			}
			break;
		case IT_WEAPON:
			if ( item->giTag > WP_NONE && item->giTag < WP_NUM_WEAPONS && bg_weaponlist[item->giTag].shortname ) {
				Com_sprintf( itemType, itemTypeSize, "weapon_%s", bg_weaponlist[item->giTag].shortname );
			}
			break;
		case IT_AMMO:
			if ( item->giTag > WP_NONE && item->giTag < WP_NUM_WEAPONS && bg_weaponlist[item->giTag].shortname ) {
				Com_sprintf( itemType, itemTypeSize, "ammo_%s", bg_weaponlist[item->giTag].shortname );
			} else {
				Q_strncpyz( itemType, "ammo", itemTypeSize );
			}
			break;
		case IT_POWERUP:
			switch ( item->giTag ) {
				case PW_QUAD: Q_strncpyz( itemType, "powerup_quad", itemTypeSize ); break;
				case PW_BATTLESUIT: Q_strncpyz( itemType, "powerup_battlesuit", itemTypeSize ); break;
				case PW_HASTE: Q_strncpyz( itemType, "powerup_haste", itemTypeSize ); break;
				case PW_INVIS: Q_strncpyz( itemType, "powerup_invis", itemTypeSize ); break;
				case PW_REGEN: Q_strncpyz( itemType, "powerup_regen", itemTypeSize ); break;
				case PW_FLIGHT: Q_strncpyz( itemType, "powerup_flight", itemTypeSize ); break;
				default: Q_strncpyz( itemType, "powerup", itemTypeSize ); break;
			}
			break;
		case IT_HOLDABLE:
			Q_strncpyz( itemType, "holdable", itemTypeSize );
			break;
		case IT_TEAM:
			Q_strncpyz( itemType, "team", itemTypeSize );
			break;
		default:
			break;
	}
}

static int WiredBots_EvalItemGoal( bot_state_t *bs, const gitem_t *item, const bot_goal_t *goal ) {
	wbItemEvalCtx_t ctx;
	int weapon;

	if ( !bs || !item || !goal ) {
		return 0;
	}

	Com_Memset( &ctx, 0, sizeof( ctx ) );
	WiredBots_ItemTypeName( item, ctx.itemType, sizeof( ctx.itemType ) );
	VectorCopy( goal->origin, ctx.itemOrigin );
	ctx.itemRespawn = 0.0f;
	ctx.health = bs->inventory[INVENTORY_HEALTH];
	ctx.armor = bs->inventory[INVENTORY_ARMOR];
	ctx.hasEnemy = ( bs->enemy >= 0 ) ? 1 : 0;
	ctx.enemyDist = fabsf( (float)bs->inventory[ENEMY_HORIZONTAL_DIST] );

	for ( weapon = WP_GAUNTLET; weapon < WP_NUM_WEAPONS; weapon++ ) {
		ctx.weapons[weapon] = WiredBots_HasWeapon( bs, weapon ) ? 1 : 0;
		ctx.ammo[weapon] = WiredBots_AmmoForWeapon( bs, weapon );
	}

	ctx.powerups[WB_POWERUP_SLOT_QUAD] = bs->inventory[INVENTORY_QUAD] ? 1.0f : 0.0f;
	ctx.powerups[WB_POWERUP_SLOT_BATTLESUIT] = bs->inventory[INVENTORY_ENVIRONMENTSUIT] ? 1.0f : 0.0f;
	ctx.powerups[WB_POWERUP_SLOT_HASTE] = bs->inventory[INVENTORY_HASTE] ? 1.0f : 0.0f;
	ctx.powerups[WB_POWERUP_SLOT_INVIS] = bs->inventory[INVENTORY_INVISIBILITY] ? 1.0f : 0.0f;
	ctx.powerups[WB_POWERUP_SLOT_REGEN] = bs->inventory[INVENTORY_REGEN] ? 1.0f : 0.0f;
	ctx.powerups[WB_POWERUP_SLOT_FLIGHT] = bs->inventory[INVENTORY_FLIGHT] ? 1.0f : 0.0f;

	return trap_BotLuaBotEvalItem( bs->client, &ctx );
}

static int WiredBots_SelectBestItemGoal( bot_state_t *bs, int tfl, const bot_goal_t *ltg, float maxTravelTime, bot_goal_t *bestGoal ) {
	float bestScore;
	int itemNum;
	int directLtgTime;
	int dbgSkipAvoid = 0, dbgSkipNoPath = 0, dbgSkipTooFar = 0, dbgSkipDetour = 0;

	if ( !bs || !bestGoal ) {
		return qfalse;
	}

	bestScore = 0.0f;
	directLtgTime = 0;

	if ( ltg && ltg->areanum > 0 ) {
		directLtgTime = trap_AAS_AreaTravelTimeToGoalArea( bs->areanum, bs->origin, ltg->areanum, tfl );
	}

	for ( itemNum = 1; itemNum < bg_numItems; itemNum++ ) {
		const gitem_t *item;
		int itemIndex;
		bot_goal_t goal;

		item = &bg_itemlist[itemNum];
		if ( !item->pickup_name || !item->pickup_name[0] ) {
			continue;
		}

		itemIndex = -1;
		while ( ( itemIndex = trap_BotGetLevelItemGoal( itemIndex, (char *)item->pickup_name, &goal ) ) >= 0 ) {
			int travelTime;
			float score;

			if ( trap_BotAvoidGoalTime( bs->gs, goal.number ) > 0.0f ) {
				dbgSkipAvoid++;
				continue;
			}

			travelTime = trap_AAS_AreaTravelTimeToGoalArea( bs->areanum, bs->origin, goal.areanum, tfl );
			if ( travelTime <= 0 ) {
				dbgSkipNoPath++;
				continue;
			}

			if ( maxTravelTime > 0.0f && travelTime > maxTravelTime ) {
				dbgSkipTooFar++;
				continue;
			}

			if ( ltg && ltg->areanum > 0 && directLtgTime > 0 ) {
				int viaLtgTime;

				viaLtgTime = trap_AAS_AreaTravelTimeToGoalArea( goal.areanum, goal.origin, ltg->areanum, tfl );
				if ( viaLtgTime > 0 && viaLtgTime > directLtgTime + WB_MAX_NBG_DETOUR ) {
					dbgSkipDetour++;
					continue;
				}
			}

			score = (float)WiredBots_EvalItemGoal( bs, item, &goal );
			score *= 1.0f / ( 1.0f + travelTime / 220.0f );

			if ( score > bestScore ) {
				bestScore = score;
				*bestGoal = goal;
			}
		}
	}

	if ( trap_Cvar_VariableIntegerValue( "sv_botDebugDecide" ) ) {
		static float s_nbgLog[MAX_CLIENTS];
		if ( FloatTime() - s_nbgLog[bs->client] > 2.0f ) {
			s_nbgLog[bs->client] = FloatTime();
			if ( bestScore > 0.0f ) {
				G_Printf( "^2[NBG] cl=%d FOUND goal=%d score=%.1f area=%d\n",
					bs->client, bestGoal->number, bestScore, bestGoal->areanum );
			} else {
				G_Printf( "^3[NBG] cl=%d NONE area=%d maxTT=%.0f "
					"skip: avoid=%d nopath=%d toofar=%d detour=%d\n",
					bs->client, bs->areanum, maxTravelTime,
					dbgSkipAvoid, dbgSkipNoPath, dbgSkipTooFar, dbgSkipDetour );
			}
		}
	}

	return ( bestScore > 0.0f ) ? qtrue : qfalse;
}

int WiredBots_ChooseLTGItem( bot_state_t *bs, int tfl ) {
	bot_goal_t goal;

	if ( !bs || !bs->wiredBotsActive ) {
		return qfalse;
	}

	if ( !WiredBots_SelectBestItemGoal( bs, tfl, NULL, 0.0f, &goal ) ) {
		return qfalse;
	}

	trap_BotPushGoal( bs->gs, &goal );
	return qtrue;
}

int WiredBots_ChooseNBGItem( bot_state_t *bs, int tfl, bot_goal_t *ltg, float range ) {
	bot_goal_t goal;

	if ( !bs || !bs->wiredBotsActive ) {
		return qfalse;
	}

	if ( range <= 0.0f ) {
		range = 150.0f;
	}

	if ( !WiredBots_SelectBestItemGoal( bs, tfl, ltg, range, &goal ) ) {
		return qfalse;
	}

	trap_BotPushGoal( bs->gs, &goal );
	return qtrue;
}

int WiredBots_Chat( bot_state_t *bs, const char *eventName, const wbChatCtx_t *ctx ) {
	char chatText[WB_CHAT_TEXT_STR];

	if ( !bs || !bs->wiredBotsActive || !eventName || !eventName[0] || !ctx ) {
		return qfalse;
	}

	if ( bs->lastchat_time > FloatTime() - TIME_BETWEENCHATTING ) {
		return qfalse;
	}

	if ( !trap_BotLuaBotOnChat( bs->client, eventName, ctx, chatText, sizeof( chatText ) ) ) {
		return qfalse;
	}

	if ( !chatText[0] ) {
		return qfalse;
	}

	if ( TeamPlayIsOn() && ctx->team ) {
		trap_EA_SayTeam( bs->client, chatText );
		bs->chatto = CHAT_TEAM;
	} else {
		trap_EA_Say( bs->client, chatText );
		bs->chatto = CHAT_ALL;
	}

	bs->lastchat_time = FloatTime();
	return qtrue;
}
