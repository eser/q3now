// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

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
#include "g_belief.h"
#include "chars.h"
#include "inv.h"

#include <math.h>
LOG_DECLARE_CHANNEL( ch_botai, "botlib.ai" );

#define WI_MAX_NBG_DETOUR 120

/* Item-selection instrument (measurement only; no behavioural effect).  File-scope
 * rather than function-static so the LTG failure BRANCH in WiredIntel_ChooseLTGItem
 * can be counted alongside the selection results it is supposed to follow from. */
static int s_ltgOk, s_ltgFail, s_nbgOk, s_nbgFail, s_ltgFailBranch;
static int s_skipNoPath, s_skipTooFar, s_skipDetour;

/* STEP 2 state: the item score that used to be destroyed at SelectBestItemGoal's
 * return boundary.  Published, never consulted for a decision in this pass. */
static float    s_lastItemScore;
static qboolean s_lastItemScoreLtg;

static void WiredIntel_LtgFailBranchCount( void ) { s_ltgFailBranch++; }

/* STEP 1 — atomic emit.
 *
 * This print previously lived INSIDE WiredIntel_SelectBestItemGoal, before its
 * return.  s_ltgFail increments there; s_ltgFailBranch increments in the CALLER,
 * after that return.  So the pair could never be sampled together: prb2 printed
 * `ltg ok=2 fail=1 branch=0`, which looks like a contradiction but is just a
 * counter pair read between its two increments.  Both counters were honest.
 *
 * Emitting from the caller — after BOTH increments have run for this evaluation —
 * makes the pair atomic.  The counters themselves are unchanged and still
 * unconditional; only the sampling POINT moved.  Behaviour is byte-identical: this
 * function reads state and logs, and is called from a site that already ran. */
static void WiredIntel_ActSelReport( const bot_state_t *bs ) {
	if ( !trap_Cvar_VariableIntegerValue( "nav_botdebug" ) ) {
		return;
	}
	{
		static float s_selLog;
		if ( FloatTime() - s_selLog <= 5.0f ) {
			return;
		}
		s_selLog = FloatTime();
	}
	Com_Log( SEV_INFO, LOG_CH(ch_botai),
		"[ACTSEL] ltg ok=%d fail=%d branch=%d | nbg ok=%d fail=%d"
		" | last skip: nopath=%d toofar=%d detour=%d\n",
		s_ltgOk, s_ltgFail, s_ltgFailBranch, s_nbgOk, s_nbgFail,
		s_skipNoPath, s_skipTooFar, s_skipDetour );
	WiredIntel_ExitChainReport();
	WiredIntel_CandReport( s_lastItemScore, s_lastItemScoreLtg,
	                       bs ? bs->origin : NULL );
}

/*
 * Item-goal database node.  A superset of the botlib bot_goal_t: the eight
 * bot_goal_t fields (origin, areanum, mins, maxs, entitynum, number, flags,
 * iteminfo) are carried verbatim in the embedded `goal` member, so a plain
 * struct assignment (`*out = node->goal;`) copies out a byte-exact bot_goal_t
 * slice for trap_BotPushGoal with zero ABI coupling — mapGoal_t never crosses
 * the botlib boundary.  On top of the slice it makes item availability a
 * first-class, queryable field:
 *   want       — the WiredIntel_EvalItemGoal result (the "do I want this?"
 *                score) for the bot that last queried this node.
 *   wantClient — which bot the cached `want` belongs to.
 *   wantTime   — level.time (ms) when `want` was computed.
 * `want` is per-bot and time-sensitive, so wantClient/wantTime stamp it and a
 * stale cross-bot value is never read: the querying bot recomputes `want` for
 * itself at selection time.  This type lives at file scope here (game-side,
 * alongside the WiredIntel bot code) rather than in be_ai_goal.h so no botlib
 * ABI header changes; it is used only by the static selection code below and
 * is deliberately kept out of the sv_lua-shared header.
 */
typedef struct mapGoal_s {
	bot_goal_t goal;   /* the 8-field bot_goal_t slice, copied out verbatim on push */
	float want;        /* availability/desirability score for wantClient */
	int wantClient;    /* client the cached `want` belongs to */
	int wantTime;      /* level.time (ms) when `want` was computed */
} mapGoal_t;

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

static float WiredIntel_Clamp01( float value ) {
	if ( value < 0.0f ) {
		return 0.0f;
	}
	if ( value > 1.0f ) {
		return 1.0f;
	}
	return value;
}

static float WiredIntel_ClampNonNegative( float value ) {
	if ( value < 0.0f ) {
		return 0.0f;
	}
	return value;
}

static int WiredIntel_HasWeapon( const bot_state_t *bs, int weapon ) {
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

static int WiredIntel_AmmoForWeapon( const bot_state_t *bs, int weapon ) {
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

static qboolean WiredIntel_WeaponUsable( const bot_state_t *bs, int weapon ) {
	if ( !WiredIntel_HasWeapon( bs, weapon ) ) {
		return qfalse;
	}
	if ( weapon == WP_GAUNTLET ) {
		return qtrue;
	}
	return WiredIntel_AmmoForWeapon( bs, weapon ) > 0;
}

static const wbWeaponMap_t *WiredIntel_WeaponMapFromShortName( const char *shortname ) {
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

float WiredIntel_ProfileFieldOr( bot_state_t *bs, int field, float fallback ) {
	if ( !bs || !bs->wiredIntelActive || field < 0 || field >= WI_PROFILE_MAX ) {
		return fallback;
	}
	return trap_BotLuaBotProfileField( bs->client, field );
}

// Return the aim height (units above entity origin) baked for the attack the bot
// currently has equipped.  Falls through to 28.0f (center mass) if not Lua-driven
// or no matching attack entry is cached.
float WiredIntel_GetCurrentAttackAimHeight( bot_state_t *bs ) {
	if ( !bs || !bs->wiredIntelActive ) {
		return 28.0f;
	}
	return trap_BotLuaBotGetAttackAimHeight( bs->client, bs->weaponnum );
}

// Returns effective bot skill in [BOT_SKILL_MIN..BOT_SKILL_MAX].
// Uses autoskill when active, otherwise settings.skill.
// Single source of truth for the inline ternary pattern.
float WiredIntel_EffectiveSkill( bot_state_t *bs ) {
	return bs->autoskill > 0.0f ? bs->autoskill : bs->settings.skill;
}

// Returns skill normalized to [0..1]: BOT_SKILL_MIN→0.0, BOT_SKILL_MAX→1.0.
// Use for new code that needs a clean 0..1 factor.
// Existing Q3-style sites used skill/5.0f ([0.2..1.0]) — use WiredIntel_ResolveAbility for those.
float WiredIntel_SkillFraction( bot_state_t *bs ) {
	float skill = WiredIntel_EffectiveSkill( bs );
	return ( skill - BOT_SKILL_MIN ) / ( BOT_SKILL_MAX - BOT_SKILL_MIN );
}

// Returns lerp(min, max) across the bot's skill range.
// Drop-in replacement for legacy magic-number patterns:
//   skill/5.0f       → WiredIntel_ResolveAbility(bs, 0.2f, 1.0f)
//   (6-skill)/5.0f   → WiredIntel_ResolveAbility(bs, 1.0f, 0.2f)
float WiredIntel_ResolveAbility( bot_state_t *bs, float min, float max ) {
	float fraction = WiredIntel_SkillFraction( bs );
	return min + fraction * ( max - min );
}

// Maps (weapon, slot) → wbProfileField_t. WP_NONE(0) and WP_GAUNTLET(1) have no accuracy field.
// Indexed [weapon][slot] following WP_* enum order; -1 = no per-weapon entry for this weapon.
static const int s_accuracyField[WP_NUM_WEAPONS][NUM_ATTACK_SLOTS] = {
	{ -1, -1 },  // WP_NONE (0)
	{ -1, -1 },  // WP_GAUNTLET (1) — melee, no accuracy noise field
	{ WI_PROFILE_ACCURACY_MACHINEGUN_S0,       WI_PROFILE_ACCURACY_MACHINEGUN_S1       },
	{ WI_PROFILE_ACCURACY_SHOTGUN_S0,          WI_PROFILE_ACCURACY_SHOTGUN_S1          },
	{ WI_PROFILE_ACCURACY_GRENADE_LAUNCHER_S0, WI_PROFILE_ACCURACY_GRENADE_LAUNCHER_S1 },
	{ WI_PROFILE_ACCURACY_ROCKET_LAUNCHER_S0,  WI_PROFILE_ACCURACY_ROCKET_LAUNCHER_S1  },
	{ WI_PROFILE_ACCURACY_LIGHTNING_GUN_S0,    WI_PROFILE_ACCURACY_LIGHTNING_GUN_S1    },
	{ WI_PROFILE_ACCURACY_RAILGUN_S0,          WI_PROFILE_ACCURACY_RAILGUN_S1          },
	{ WI_PROFILE_ACCURACY_PLASMA_RIFLE_S0,     WI_PROFILE_ACCURACY_PLASMA_RIFLE_S1     },
};

// Returns per-(weapon, slot) accuracy. Values of -1.0f are sentinels: "not set in Lua".
// Fallback chain (first non-sentinel value wins):
//   slot=1, slot1=unset, slot0=0.7  → 0.7
//   slot=0, slot0=unset, global=0.4 → 0.4
//   slot=0, slot0=0.8, global=0.4   → 0.8
//   slot=1, slot1=0.9, slot0=0.7    → 0.9
//   all unset                        → 0.5 (hardcoded default)
float WiredIntel_AttackAccuracy( bot_state_t *bs, int weapon, int slot ) {
	float val = -1.0f;

	if ( weapon > WP_NONE && weapon < WP_NUM_WEAPONS
	     && slot >= 0 && slot < NUM_ATTACK_SLOTS ) {
		int field = s_accuracyField[weapon][slot];
		if ( field >= 0 ) {
			val = WiredIntel_ProfileFieldOr( bs, field, -1.0f );
		}
		if ( val < 0.0f && slot == 1 ) {
			// slot 1 not set: fall through to primary
			int field0 = s_accuracyField[weapon][0];
			if ( field0 >= 0 ) {
				val = WiredIntel_ProfileFieldOr( bs, field0, -1.0f );
			}
		}
	}

	if ( val < 0.0f ) {
		val = WiredIntel_ProfileFieldOr( bs, WI_PROFILE_ACCURACY, -1.0f );
	}
	return val < 0.0f ? 0.5f : val;
}

static float WiredIntel_WeaponBaseScore( int weapon ) {
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

static float WiredIntel_WeaponIdealRange( int weapon ) {
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

static float WiredIntel_WeaponRangeScore( int weapon, float distance ) {
	float ideal;
	float span;
	float delta;
	float score;

	if ( distance <= 0.0f ) {
		return 0.5f;
	}

	ideal = WiredIntel_WeaponIdealRange( weapon );
	span = ideal * 0.75f + 80.0f;
	delta = fabsf( distance - ideal );
	score = 1.0f - ( delta / span );

	return WiredIntel_Clamp01( score );
}

static float WiredIntel_AmmoScore( int weapon, int ammo ) {
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


static int WiredIntel_WeaponFromKey( const char *weaponKey ) {
	const wbWeaponMap_t *map;

	if ( !weaponKey || !weaponKey[0] ) {
		return WP_NONE;
	}

	map = WiredIntel_WeaponMapFromShortName( weaponKey );
	if ( map ) {
		return map->weapon;
	}

	return WP_NONE;
}

static void WiredIntel_FillCombatCtx( bot_state_t *bs, wbCombatCtx_t *ctx ) {
	int weapon;

	memset( ctx, 0, sizeof( *ctx ) );
	ctx->enemyDist = fabsf( (float)bs->inventory[ENEMY_HORIZONTAL_DIST] );
	ctx->enemyHealth = ( bs->enemy >= 0 && bs->enemy < MAX_GENTITIES ) ? g_entities[bs->enemy].health : -1;
	ctx->selfHealth = bs->inventory[INVENTORY_HEALTH];
	ctx->selfArmor = bs->inventory[INVENTORY_ARMOR];

	for ( weapon = WP_GAUNTLET; weapon < WP_NUM_WEAPONS; weapon++ ) {
		ctx->weapons[weapon] = WiredIntel_HasWeapon( bs, weapon ) ? 1 : 0;
		ctx->ammo[weapon] = WiredIntel_AmmoForWeapon( bs, weapon );
	}
}

static void WiredIntel_WeaponPersonalityScores( int clientNum, int weapon, float *outBias, float *outSkill ) {
	wbProfileField_t biasField = WI_PROFILE_MAX;
	wbProfileField_t skillField = WI_PROFILE_MAX;
	switch ( weapon ) {
		case WP_MACHINEGUN:       biasField = WI_PROFILE_BIAS_MG; skillField = WI_PROFILE_SKILL_MG; break;
		case WP_SHOTGUN:          biasField = WI_PROFILE_BIAS_SG; skillField = WI_PROFILE_SKILL_SG; break;
		case WP_GRENADE_LAUNCHER: biasField = WI_PROFILE_BIAS_GL; skillField = WI_PROFILE_SKILL_GL; break;
		case WP_ROCKET_LAUNCHER:  biasField = WI_PROFILE_BIAS_RL; skillField = WI_PROFILE_SKILL_RL; break;
		case WP_LIGHTNING_GUN:    biasField = WI_PROFILE_BIAS_LG; skillField = WI_PROFILE_SKILL_LG; break;
		case WP_RAILGUN:          biasField = WI_PROFILE_BIAS_RG; skillField = WI_PROFILE_SKILL_RG; break;
		case WP_PLASMA_RIFLE:     biasField = WI_PROFILE_BIAS_PG; skillField = WI_PROFILE_SKILL_PG; break;
		default: break;
	}
	*outBias  = ( biasField  < WI_PROFILE_MAX ) ? trap_BotLuaBotProfileField( clientNum, biasField )  : 1.0f;
	*outSkill = ( skillField < WI_PROFILE_MAX ) ? trap_BotLuaBotProfileField( clientNum, skillField ) : 0.5f;
}

int WiredIntel_ChooseWeapon( bot_state_t *bs, int fallbackWeapon ) {
	wbCombatCtx_t ctx;
	char weaponKey[WI_WEAPONKEY_STR];
	int weapon;
	int bestWeapon;
	float bestScore;
	float enemyDistance;
	const char *chosenKey;
	int chosenWeapon;

	if ( !bs || !bs->wiredIntelActive ) {
		return fallbackWeapon;
	}

	WiredIntel_FillCombatCtx( bs, &ctx );

	chosenKey = NULL;
	chosenWeapon = 0;

	if ( trap_BotLuaBotPickWeapon( bs->client, &ctx, weaponKey, sizeof( weaponKey ) ) ) {
		weapon = WiredIntel_WeaponFromKey( weaponKey );
		if ( WiredIntel_WeaponUsable( bs, weapon ) ) {
			chosenKey = weaponKey;
			chosenWeapon = weapon;
			goto debug_print;
		}
	}

	enemyDistance = fabsf( (float)bs->inventory[ENEMY_HORIZONTAL_DIST] );

	bestWeapon = WiredIntel_WeaponUsable( bs, fallbackWeapon ) ? fallbackWeapon : WP_GAUNTLET;
	bestScore = -1.0f;

	for ( weapon = WP_GAUNTLET; weapon < WP_NUM_WEAPONS; weapon++ ) {
		float baseScore;
		float rangeScore;
		float ammoScore;
		float score;
		float bias, skillVal;
		int ammo;
		if ( !WiredIntel_WeaponUsable( bs, weapon ) ) {
			continue;
		}

		ammo = WiredIntel_AmmoForWeapon( bs, weapon );

		if ( weapon != WP_GAUNTLET && ammo < 2 ) {
			continue;
		}

		baseScore = WiredIntel_WeaponBaseScore( weapon );
		rangeScore = WiredIntel_WeaponRangeScore( weapon, enemyDistance );
		ammoScore = WiredIntel_AmmoScore( weapon, ammo );

		score = baseScore * 0.35f + rangeScore * 0.40f + ammoScore * 0.25f;

		WiredIntel_WeaponPersonalityScores( bs->client, weapon, &bias, &skillVal );
		score *= bias;
		score += ( skillVal - 0.5f ) * 0.10f;

		if ( ctx.enemyHealth > 0 && ctx.enemyHealth <= 50 ) {
			if      ( weapon == WP_RAILGUN )   score += 0.15f;
			else if ( weapon == WP_LIGHTNING_GUN ) score += 0.10f;
			else if ( weapon == WP_SHOTGUN )   score += 0.08f;
		}

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
					int ammo = WiredIntel_AmmoForWeapon( bs, chosenWeapon );
					int inBitmask = ( bs->cur_ps.stats[STAT_WEAPONS] >> chosenWeapon ) & 1;
					Com_Log( SEV_INFO, LOG_CH(ch_botai), "frame=%d bot=%s key=%s wp=%d ammo=%d bitmask=%d\n",
					          s_dbgFrames, botName,
					          chosenKey ? chosenKey : "?",
					          chosenWeapon, ammo, inBitmask );
					s_dbgFrames++;
				} else {
					trap_Cvar_Set( "bot_debug_weapon", "" );
					s_dbgPrev[0] = '\0';
					s_dbgFrames = 0;
					Com_Log( SEV_INFO, LOG_CH(ch_botai), "reached 100 frames, stopping.\n" );
				}
			}
		} else if ( s_dbgPrev[0] ) {
			s_dbgPrev[0] = '\0';
			s_dbgFrames = 0;
		}
	}

	return chosenWeapon;
}

float WiredIntel_Aggression( bot_state_t *bs ) {
	float health;
	float armor;
	float weaponPower;
	float enemyPressure;
	float aggression;
	float traitAggression;

	if ( !bs ) {
		return 0.0f;
	}

	traitAggression = WiredIntel_Clamp01( WiredIntel_ProfileFieldOr( bs, WI_PROFILE_AGGRESSION, 0.5f ) );
	health = WiredIntel_Clamp01( bs->inventory[INVENTORY_HEALTH] / 125.0f );
	armor = WiredIntel_Clamp01( bs->inventory[INVENTORY_ARMOR] / 125.0f );
	weaponPower = WiredIntel_Clamp01( WiredIntel_WeaponBaseScore( bs->weaponnum ) );
	enemyPressure = 1.0f - WiredIntel_Clamp01( fabsf( (float)bs->inventory[ENEMY_HORIZONTAL_DIST] ) / 1000.0f );

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

static int WiredIntel_Decide( bot_state_t *bs, char *decision, int decisionSize ) {
	wbDecideCtx_t ctx;

	if ( !bs || !decision || decisionSize <= 0 || !bs->wiredIntelActive ) {
		return qfalse;
	}

	memset( &ctx, 0, sizeof( ctx ) );
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

		if ( trap_Cvar_VariableIntegerValue( "bot_debug" ) >= 1 ) {
			static float s_decideLogTime[MAX_CLIENTS];
			if ( FloatTime() - s_decideLogTime[bs->client] > 2.0f ) {
				s_decideLogTime[bs->client] = FloatTime();
				Com_Log( SEV_INFO, LOG_CH(ch_botai), "client=%d result='%s'(%d) ev=%d ed=%.0f hp=%d uf=%d\n",
					bs->client, result ? decision : "<none>", result,
					ctx.enemyVisible, ctx.enemyDist, ctx.health, ctx.underFire );
			}
		}

		return result;
	}
}

int WiredIntel_WantsToRetreat( bot_state_t *bs ) {
	char decision[32];
	float danger;
	float selfPreserve;

	if ( !bs ) {
		return qfalse;
	}

	// NOTE (Lua-decide role): unlike WiredIntel_WantsToChase — where the Lua
	// decision is a HINT with the C formula as fallback for any unrecognized or
	// absent decision — here a PRESENT Lua decision is the SOLE authority: when
	// WiredIntel_Decide returns a decision, we answer purely from it
	// ("retreat" -> qtrue, anything else -> qfalse) and never consult the C
	// danger formula. The C formula below is a fallback ONLY for the "Lua
	// produced no decision" case. This is deliberate ("roam" with no visible
	// enemy must NOT fall through to the danger formula, which returns qtrue for
	// MG+no-armor bots and would block all combat), but it means the C code is
	// NOT the arbitration authority on this path the way the hint model assumes.
	if ( WiredIntel_Decide( bs, decision, sizeof( decision ) ) ) {
		return !Q_stricmp( decision, "retreat" );
	}

	selfPreserve = WiredIntel_Clamp01( WiredIntel_ProfileFieldOr( bs, WI_PROFILE_SELF_PRESERVE, 0.5f ) );
	danger = 1.0f - WiredIntel_Clamp01( WiredIntel_Aggression( bs ) / 100.0f );

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
	danger = Com_Clamp( 0.0f, 1.0f, danger );  // cap: low-weapon + no-armor sums can exceed 1.0

	return danger > 0.65f;
}

int WiredIntel_WantsToChase( bot_state_t *bs ) {
	char decision[32];
	float chaseDrive;
	float vengefulness;

	if ( !bs ) {
		return qfalse;
	}

	// Lua decide is a HINT feeding the C arbitration, not the authority: only a
	// RECOGNIZED Lua decision ("chase"/"fight" -> engage, "retreat"/"roam" ->
	// don't) is honored; an unknown decision, or no decision at all, falls
	// through to the C chaseDrive formula below (ret stays -1). The C formula is
	// always the fallback, so the Lua string can never be the sole authority here.
	if ( WiredIntel_Decide( bs, decision, sizeof( decision ) ) ) {
		int ret = -1; // -1 = unknown decision, fall through to chaseDrive formula
		// "fight" and "chase" both mean engage — pursue if briefly out of sight
		if ( !Q_stricmp( decision, "chase" ) || !Q_stricmp( decision, "fight" ) ) {
			ret = qtrue;
		} else if ( !Q_stricmp( decision, "retreat" ) || !Q_stricmp( decision, "roam" ) ) {
			ret = qfalse;
		}
		if ( trap_Cvar_VariableIntegerValue( "bot_debug" ) >= 1 )
			Com_Log( SEV_INFO, LOG_CH(ch_botai), "cl=%d decision='%s' ret=%d\n",
				bs->client, decision, ret );
		if ( ret >= 0 ) return ret;
	}

	vengefulness = WiredIntel_Clamp01( WiredIntel_ProfileFieldOr( bs, WI_PROFILE_VENGEFULNESS, 0.5f ) );
	chaseDrive = WiredIntel_Clamp01( WiredIntel_Aggression( bs ) / 100.0f );
	chaseDrive *= 0.70f + vengefulness;

	if ( bs->inventory[INVENTORY_HEALTH] < 45 ) {
		chaseDrive -= 0.25f;
	}

	if ( bs->inventory[ENEMY_HORIZONTAL_DIST] > 1200 ) {
		chaseDrive -= 0.10f;
	}

	return chaseDrive > 0.60f;
}

static void WiredIntel_ItemTypeName( const gitem_t *item, char *itemType, int itemTypeSize ) {
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

static int WiredIntel_EvalItemGoal( bot_state_t *bs, const gitem_t *item, const bot_goal_t *goal ) {
	wbItemEvalCtx_t ctx;
	int weapon;

	if ( !bs || !item || !goal ) {
		return 0;
	}

	memset( &ctx, 0, sizeof( ctx ) );
	WiredIntel_ItemTypeName( item, ctx.itemType, sizeof( ctx.itemType ) );
	VectorCopy( goal->origin, ctx.itemOrigin );
	ctx.itemRespawn = 0.0f;
	ctx.health = bs->inventory[INVENTORY_HEALTH];
	ctx.armor = bs->inventory[INVENTORY_ARMOR];
	ctx.hasEnemy = ( bs->enemy >= 0 ) ? 1 : 0;
	ctx.enemyDist = fabsf( (float)bs->inventory[ENEMY_HORIZONTAL_DIST] );
	ctx.giType = item->giType;
	ctx.giTag  = item->giTag;

	for ( weapon = WP_GAUNTLET; weapon < WP_NUM_WEAPONS; weapon++ ) {
		ctx.weapons[weapon] = WiredIntel_HasWeapon( bs, weapon ) ? 1 : 0;
		ctx.ammo[weapon] = WiredIntel_AmmoForWeapon( bs, weapon );
	}

	ctx.powerups[WI_POWERUP_SLOT_QUAD] = bs->inventory[INVENTORY_QUAD] ? 1.0f : 0.0f;
	ctx.powerups[WI_POWERUP_SLOT_BATTLESUIT] = bs->inventory[INVENTORY_ENVIRONMENTSUIT] ? 1.0f : 0.0f;
	ctx.powerups[WI_POWERUP_SLOT_HASTE] = bs->inventory[INVENTORY_HASTE] ? 1.0f : 0.0f;
	ctx.powerups[WI_POWERUP_SLOT_INVIS] = bs->inventory[INVENTORY_INVISIBILITY] ? 1.0f : 0.0f;
	ctx.powerups[WI_POWERUP_SLOT_REGEN] = bs->inventory[INVENTORY_REGEN] ? 1.0f : 0.0f;
	ctx.powerups[WI_POWERUP_SLOT_FLIGHT] = bs->inventory[INVENTORY_FLIGHT] ? 1.0f : 0.0f;

	// C.5: distances bot→item and enemy→item for contested-item scoring in Lua
	{
		vec3_t diff;
		VectorSubtract( goal->origin, bs->origin, diff );
		ctx.itemBotDist = VectorLength( diff );
		if ( bs->enemy >= 0 && bs->enemy < MAX_GENTITIES ) {
			VectorSubtract( goal->origin, g_entities[bs->enemy].s.origin, diff );
			ctx.itemEnemyDist = VectorLength( diff );
		} else {
			ctx.itemEnemyDist = -1.0f;
		}
	}

	// C.2/C.3: populate respawn time and apply per-item window (no navmesh dependency)
	if ( goal->entitynum > 0 && goal->entitynum < MAX_GENTITIES ) {
		const gentity_t *gent = &g_entities[goal->entitynum];
		if ( !gent->r.linked ) {
			int windowMs;
			int respawnMs = gent->nextthink - level.time;
			if      ( strcmp( ctx.itemType, "health_mega" ) == 0 ) windowMs = 15000;
			else if ( strcmp( ctx.itemType, "armor_red"   ) == 0 ) windowMs = 10000;
			else if ( item->giType == IT_POWERUP )                  windowMs = 20000;
			else                                                     windowMs =  5000;
			ctx.itemRespawn = ( respawnMs > 0 ) ? (float)respawnMs / 1000.0f : 0.0f;
			if ( respawnMs > windowMs ) {
				return 0;
			}
			if ( respawnMs > 0 ) {
				int rawScore = trap_BotLuaBotEvalItem( bs->client, &ctx );
				return (int)( rawScore * (float)( windowMs - respawnMs ) / (float)windowMs );
			}
		}
	}

	return trap_BotLuaBotEvalItem( bs->client, &ctx );
}

/*
 * Item-goal database.  s_mapGoalDb is a fixed array (one node per live item
 * entity); s_mapGoalCount is how many nodes are currently maintained.
 *
 * Maintenance strategy: EVENT-DRIVEN.  The node SET is maintained incrementally
 * at the two item lifecycle events — a node is inserted when an item entity
 * finishes entering the world and removed when the entity is freed.  It is NOT
 * rebuilt per query.  A fixed array (rather than the botlib levelitem_t
 * intrusive freelist) is still the right container: MAX_GENTITIES is the hard
 * ceiling on distinct item entities so it can never overflow, insert appends in
 * O(1), and remove is an O(count) swap-with-last-and-shrink (count is tiny —
 * one entry per map item), keeping the container allocation-free and
 * cache-linear with no per-node prev/next bookkeeping.
 *
 * s_mapGoalIndexByEnt[entitynum] is a reverse index: the slot holding that
 * entity's node, or -1 if the entity has no node.  It makes remove O(1)-lookup
 * and makes "does this entity already have a node?" a single array read, so a
 * double insert (belt-and-suspenders against a spawn hook firing twice) is
 * cheaply idempotent.
 *
 * CRITICAL SEPARATION — node-SET vs. availability:
 *   The events maintain only the node SET (which item entities exist as
 *   candidates).  Item AVAILABILITY (is it grabbable now, respawn window, the
 *   per-bot `want` score) is NOT event-driven: it is computed at QUERY time in
 *   WiredIntel_SelectBestItemGoal / WiredIntel_EvalItemGoal from ent->r.linked
 *   and the respawn timer.  Pickup and respawn do NOT free the entity, so they
 *   are NOT node-set events — the node persists; only its query-time
 *   availability flips.  `want` is never cached across queries (see below).
 */
static mapGoal_t s_mapGoalDb[MAX_GENTITIES];
static int       s_mapGoalCount;
static int       s_mapGoalIndexByEnt[MAX_GENTITIES];

/*
 * The item-list index (iteminfo) for an entity that carries a gitem_t, or 0 if
 * the entity is not a valid item goal candidate.  This is exactly the predicate
 * and derivation the retired full-rebuild scan applied per entity:
 *   inuse && item && (item - bg_itemlist) in [1, bg_numItems) && pickup_name[0]
 * pickup_name is unique across bg_itemlist, so gent->item - bg_itemlist is the
 * same iteminfo the scan produced — keeping the maintained set byte-identical in
 * membership to what the rebuild would have derived for this entity.
 */
static int WiredIntel_MapGoalIteminfo( const gentity_t *gent ) {
	int iteminfo;

	if ( !gent || !gent->inuse || !gent->item ) {
		return 0;
	}
	iteminfo = (int)( gent->item - bg_itemlist );
	if ( iteminfo < 1 || iteminfo >= bg_numItems ) {
		return 0;
	}
	if ( !gent->item->pickup_name || !gent->item->pickup_name[0] ) {
		return 0;
	}
	return iteminfo;
}

/*
 * Reset the DB to empty.  Called at level init (G_InitGame) before entities
 * spawn, so no node from a previous map survives a map transition.  The retired
 * per-tick rebuild implicitly discarded stale state every call; the maintained
 * DB must be cleared explicitly once per level.
 */
void WiredIntel_ClearMapGoalDb( void ) {
	int i;
	s_mapGoalCount = 0;
	for ( i = 0; i < MAX_GENTITIES; i++ ) {
		s_mapGoalIndexByEnt[i] = -1;
	}
}

/*
 * Insert a node for an item entity that has just entered the world as a live
 * goal candidate.  Called from the two spawn-completion choke points that
 * cover every item entity exactly once:
 *   - FinishSpawningItem  (map-placed items, after drop-to-floor settles origin)
 *   - LaunchItem          (dropped items: weapon/flag drops, "give" fallthrough)
 * These are the only two sites that assign gent->item, so together they account
 * for every item entity.  Idempotent: if the entity already has a node (hook
 * fired twice, or a re-spawn re-enters the same choke point), the existing node
 * is refreshed in place rather than duplicated.  Static fields (mins/maxs,
 * iteminfo, flags) are captured here; the mutable origin is re-synced from the
 * live entity at query time (see WiredIntel_SelectBestItemGoal), so a dropped
 * item that physics-settles or a map item riding a mover never diverges from
 * the entity.  Entities that are not valid item goals (invalid iteminfo, empty
 * pickup_name) are ignored — matching the rebuild's per-entity filter.
 */
void WiredIntel_MapGoalOnSpawn( gentity_t *gent ) {
	int eNum;
	int iteminfo;
	int slot;
	mapGoal_t *node;

	iteminfo = WiredIntel_MapGoalIteminfo( gent );
	if ( !iteminfo ) {
		return;
	}

	eNum = (int)( gent - g_entities );
	if ( eNum < 0 || eNum >= MAX_GENTITIES ) {
		return;
	}

	slot = s_mapGoalIndexByEnt[eNum];
	if ( slot < 0 ) {
		slot = s_mapGoalCount++;
		s_mapGoalIndexByEnt[eNum] = slot;
	}

	node = &s_mapGoalDb[slot];
	memset( &node->goal, 0, sizeof( node->goal ) );
	VectorCopy( gent->r.currentOrigin, node->goal.origin );
	VectorCopy( gent->r.mins, node->goal.mins );
	VectorCopy( gent->r.maxs, node->goal.maxs );
	node->goal.entitynum = eNum;
	node->goal.number    = eNum;
	node->goal.flags     = GFL_ITEM;
	node->goal.iteminfo  = iteminfo;

	/* want is per-bot/time-sensitive; leave it unstamped so the querying bot
	 * always recomputes it for itself (no stale cross-bot value). */
	node->want       = 0.0f;
	node->wantClient = -1;
	node->wantTime   = -1;
}

/*
 * Remove the node for an entity being freed (G_FreeEntity: dropped items
 * expiring, team teardown, nodrop-volume removal, map transition).  A no-op if
 * the entity had no item node (most freed entities aren't items).  Removal is a
 * swap-with-last-and-shrink so the array stays dense; the reverse index of the
 * moved node is fixed up.  Pickup does NOT free the entity, so pickup never
 * reaches here — the node correctly persists across a pickup, and only its
 * query-time availability (r.linked / respawn timer) flips.
 */
void WiredIntel_MapGoalOnFree( gentity_t *gent ) {
	int eNum;
	int slot;
	int last;

	if ( !gent ) {
		return;
	}
	eNum = (int)( gent - g_entities );
	if ( eNum < 0 || eNum >= MAX_GENTITIES ) {
		return;
	}

	slot = s_mapGoalIndexByEnt[eNum];
	if ( slot < 0 ) {
		return; /* entity had no item node */
	}

	last = s_mapGoalCount - 1;
	if ( slot != last ) {
		s_mapGoalDb[slot] = s_mapGoalDb[last];
		s_mapGoalIndexByEnt[ s_mapGoalDb[slot].goal.entitynum ] = slot;
	}
	s_mapGoalCount = last;
	s_mapGoalIndexByEnt[eNum] = -1;
}

/*
 * Item-belief view over the shared item-goal DB.  The belief store does NOT
 * copy the item set into itself: s_mapGoalDb stays the single, event-driven,
 * map-shared maintained set (one per map, not per brain).  These two accessors
 * expose it read-only as the belief API's item-belief view, so the goal tree
 * and coroutine layer address item knowledge through the same seam as the other
 * belief fields without the set being duplicated.  Belief_ItemBelief out-copies
 * the i-th node's bot_goal_t slice verbatim (the same slice trap_BotPushGoal
 * consumes); availability/`want` stays query-time in the selection path and is
 * intentionally not surfaced here.  Declared in g_belief.h; implemented here
 * because the DB is file-static to this translation unit.
 */
int Belief_ItemBeliefCount( void ) {
	return s_mapGoalCount;
}

qboolean Belief_ItemBelief( int index, bot_goal_t *out ) {
	if ( !out || index < 0 || index >= s_mapGoalCount ) {
		return qfalse;
	}
	*out = s_mapGoalDb[index].goal;   /* verbatim bot_goal_t slice */
	return qtrue;
}

/*
 * Tie-break identical to the pre-change scan.  The scan iterated
 * (itemNum ascending, then entitynum ascending) and used strict `>`, so on an
 * exact score tie it kept the candidate encountered first — the one with the
 * lower (iteminfo, entitynum).  Selection below iterates the DB in entitynum
 * order, so this comparator restores that ordering: a candidate beats the
 * incumbent if its score is strictly greater, or equal and its (iteminfo,
 * entitynum) key is lower.  This makes the DB path pick the same winner the
 * scan did, ties included.
 */
static qboolean WiredIntel_MapGoalBeatsBest( float score, const bot_goal_t *cand,
		float bestScore, const bot_goal_t *best, qboolean haveBest ) {
	if ( score > bestScore ) {
		return qtrue;
	}
	if ( !haveBest || score < bestScore ) {
		return qfalse;
	}
	/* score == bestScore: prefer lower iteminfo, then lower entitynum. */
	if ( cand->iteminfo != best->iteminfo ) {
		return ( cand->iteminfo < best->iteminfo ) ? qtrue : qfalse;
	}
	return ( cand->entitynum < best->entitynum ) ? qtrue : qfalse;
}

static int WiredIntel_SelectBestItemGoal( bot_state_t *bs, int tfl, const bot_goal_t *ltg, float maxTravelTime, bot_goal_t *bestGoal ) {
	float bestScore;
	int directLtgTime;
	int nodeIdx;
	qboolean haveBest;
	int dbgSkipNoPath = 0, dbgSkipTooFar = 0, dbgSkipDetour = 0;

	if ( !bs || !bestGoal ) {
		return qfalse;
	}

	bestScore = 0.0f;
	haveBest = qfalse;
	directLtgTime = 0;

	/* AAS is disabled when Recast navmesh is active.  Use distance / 300 ups
	   as a travel-time proxy so goal scoring still penalises far items. */
	if ( ltg ) {
		vec3_t delta;
		VectorSubtract( ltg->origin, bs->origin, delta );
		directLtgTime = (int)( VectorLength( delta ) * 1000.0f / 300.0f ) + 1;
	}

	/* Read the maintained item-goal DB directly — NO per-tick rebuild.  The node
	 * SET is maintained event-driven at item spawn/free (WiredIntel_MapGoalOnSpawn
	 * / WiredIntel_MapGoalOnFree).  The botlib item database is empty under Recast
	 * (BotInitLevelItems early-returns when AAS_Loaded()==false), so this DB is the
	 * sole item-goal source.  Availability/`want` stays query-time: below, each
	 * node's origin is re-synced from the live entity (drift-proof for dropped
	 * items settling / map items on movers) and `want` is recomputed for this bot,
	 * so the maintained DB yields the same candidate set / best goal the retired
	 * full rebuild would have for the same world state at this selection instant. */
	for ( nodeIdx = 0; nodeIdx < s_mapGoalCount; nodeIdx++ ) {
		mapGoal_t *node = &s_mapGoalDb[nodeIdx];
		const gitem_t *item = &bg_itemlist[node->goal.iteminfo];
		const gentity_t *gent = &g_entities[node->goal.entitynum];
		int travelTime;
		float score;

		/* Re-sync the mutable static field (origin) from the live entity so the
		 * node never diverges from a physics-settling dropped item or a mover-
		 * borne map item; matches the r.currentOrigin the rebuild read. */
		VectorCopy( gent->r.currentOrigin, node->goal.origin );

		{
			vec3_t delta;
			VectorSubtract( node->goal.origin, bs->origin, delta );
			travelTime = (int)( VectorLength( delta ) * 1000.0f / 300.0f ) + 1;
		}

		/* Distance pre-reject stays FIRST: it is the cheap filter, and running it
		 * before the corridor query keeps the expensive test off the majority of
		 * candidates (measured: it rejects 1530 of 1666, so the corridor query runs
		 * on 8.2% of what it otherwise would). */
		if ( maxTravelTime > 0.0f && travelTime > maxTravelTime ) {
			dbgSkipTooFar++;
			continue;
		}

		/* ── CANDIDATE REACHABILITY ───────────────────────────────────────────
		   CLASS RULE: a goal candidate that cannot be reached from the agent's
		   current position must not be selected, regardless of its straight-line
		   distance.

		   The travel-time above is a STRAIGHT-LINE proxy — the stand-in AAS
		   retirement left behind when AAS_AreaTravelTime (graph distance) was
		   replaced by |delta|/300ups.  Distance says nothing about connectivity, so
		   the filter happily accepts a candidate on the far side of a severed floor:
		   measured on the e1m1 specimen, 34 of 136 accepted candidates (25.0%) had
		   no corridor from the bot's position.  Committing to one of those is what
		   produces a goal the follower can never make progress toward.

		   The predicate is the SAME degenerate-corridor test the held-goal retire
		   uses (<=2 waypoints AND partial): origin and goal with nothing between
		   them.  It is used here as a SINGLE-SHOT test rather than the sustained
		   form, and the difference is deliberate: at the retention seam a transient
		   degenerate reading must not abandon a goal the bot is already committed
		   to (a gate shut ahead reads identically), whereas at the SELECTION seam
		   there is nothing yet committed — declining a candidate this tick costs
		   nothing, because selection re-runs and the candidate returns the moment a
		   corridor exists.  Same mechanism, and the seam decides whether it needs
		   persistence.

		   This is where dbgSkipNoPath finally increments: the counter has existed
		   and read 0 since AAS retirement because nothing could set it. */
		{
			static navPath_t s_reachProbe;
			int n = trap_Nav_FindPath( bs->origin, node->goal.origin,
			                           NAV_AGENT_PLAYER, &s_reachProbe );
			if ( NAV_FINDPATH_COUNT( n ) <= 2 && ( n & NAV_FINDPATH_PARTIAL ) ) {
				dbgSkipNoPath++;
				continue;
			}
		}

		/* want is a first-class field on the node: compute it for the querying
		 * bot and stamp whose/when so no stale cross-bot value is ever served. */
		node->want       = (float)WiredIntel_EvalItemGoal( bs, item, &node->goal );
		node->wantClient = bs->client;
		node->wantTime   = level.time;

		score = node->want;
		score *= 1.0f / ( 1.0f + travelTime / 220.0f );

		if ( WiredIntel_MapGoalBeatsBest( score, &node->goal, bestScore, bestGoal, haveBest ) ) {
			bestScore = score;
			*bestGoal = node->goal;
			haveBest  = qtrue;
		}
	}

	if ( trap_Cvar_VariableIntegerValue( "bot_debug" ) >= 1 ) {
		static float s_nbgLog[MAX_CLIENTS];
		if ( FloatTime() - s_nbgLog[bs->client] > 2.0f ) {
			s_nbgLog[bs->client] = FloatTime();
			if ( bestScore > 0.0f ) {
				// NOLINTNEXTLINE(clang-analyzer-core.CallAndMessage) — bestGoal is written via *bestGoal=node->goal when a node beats the best (bestScore>0); analyzer can't see the through-pointer init via the parameter
				Com_Log( SEV_INFO, LOG_CH(ch_botai), "cl=%d FOUND goal=%d score=%.1f area=%d\n",
					bs->client, bestGoal->number, bestScore, bestGoal->areanum );
			} else {
				Com_Log( SEV_INFO, LOG_CH(ch_botai), "cl=%d NONE area=%d maxTT=%.0f skip:"
					" nopath=%d toofar=%d detour=%d\n",
					bs->client, bs->areanum, maxTravelTime,
					dbgSkipNoPath, dbgSkipTooFar, dbgSkipDetour );
			}
		}
	}

	/* ACTIVATION-CHAIN INSTRUMENT (measurement only, no behavioural effect).
	 *
	 * The FOUND/NONE log above cannot answer "how often does item selection fail",
	 * for two reasons.  It is throttled to one line per client per 2 s, and BOTH
	 * call paths share that one throttle (`s_nbgLog`): the NBG path runs far more
	 * often, consumes the window, and the LTG path's own result is almost never
	 * printed.  Measured over five pinned runs: 83 emitted lines carried maxTT=150
	 * (NBG) and exactly 1 carried maxTT=0 (LTG).  That 1-vs-83 is a property of the
	 * THROTTLE, not of the call rate — a counter behind the condition it measures.
	 *
	 * These counters are therefore UNCONDITIONAL: they increment on every call,
	 * before and independently of any debug cvar or throttle.  Only the periodic
	 * EMIT below is gated (on nav_botdebug, read through the same trap accessor the
	 * bot_debug logging above uses), which is the trap-18 rule — gate the
	 * print, never the count.  Split by call path because only the LTG path
	 * (maxTravelTime == 0) can reach WiredIntel_TrySetExitGoal, which is the sole
	 * entry to the exit/door/button progression. */
	{
		const qboolean isLtg = ( maxTravelTime <= 0.0f );
		const qboolean ok    = ( bestScore > 0.0f );
		if ( isLtg ) { if ( ok ) s_ltgOk++; else s_ltgFail++; }
		else         { if ( ok ) s_nbgOk++; else s_nbgFail++; }
		/* The last-call skip locals are function-scoped, so snapshot them here for
		 * the emit that now runs in the CALLER (see WiredIntel_ActSelReport). */
		s_skipNoPath = dbgSkipNoPath;
		s_skipTooFar = dbgSkipTooFar;
		s_skipDetour = dbgSkipDetour;
	}

	/* STEP 2 — expose the score instead of destroying it.
	 *
	 * The return value is UNCHANGED: `bestScore > 0.0f`, exactly as before.  But the
	 * float itself was previously discarded at this boundary, so no caller could ever
	 * compare an item's utility against another candidate's — the arbitration seam
	 * (WiredIntel_MapGoalBeatsBest) exists and is generic, yet nothing outside this
	 * function can feed it.  Publishing the score costs nothing and decides nothing;
	 * it is read only by telemetry in this pass. */
	s_lastItemScore    = bestScore;
	s_lastItemScoreLtg = ( maxTravelTime <= 0.0f ) ? qtrue : qfalse;

	return ( bestScore > 0.0f ) ? qtrue : qfalse;
}

int WiredIntel_ChooseLTGItem( bot_state_t *bs, int tfl ) {
	bot_goal_t goal;

	if ( !bs || !bs->wiredIntelActive ) {
		return qfalse;
	}

	if ( !WiredIntel_SelectBestItemGoal( bs, tfl, NULL, 0.0f, &goal ) ) {
		/* No worthwhile item to fetch — this is the "nothing better to do" point
		   that otherwise leads to random roaming. Before roaming, aim the bot at
		   the level's exit trigger so it makes directed forward progress. This
		   sets an UNLOCKED long-term goal (LTG_DEFENDKEYAREA toward the exit),
		   which BotGetLongTermGoal picks up next frame; combat still preempts it.
		   Returns qfalse either way: no item goal was pushed to the stack, and on
		   a map with no exit the bot roams exactly as before. */

		/* BRANCH PROBE (instrument only).  s_ltgFail (incremented inside
		 * SelectBestItemGoal on the maxTravelTime<=0 path) and s_exitEntered
		 * (incremented at TrySetExitGoal's top) MUST move together: this branch is
		 * the only LTG caller, the !bs||!wiredIntelActive guard above already
		 * passed, and TrySetExitGoal is called unconditionally on the next line with
		 * that same bs.  A run recorded ltgFail=1 with entered=0, which those two
		 * descriptions make impossible — so one counter is not measuring what its
		 * name says.  This counter sits ON the branch itself and arbitrates:
		 * branch==ltgFail isolates the fault to TrySetExitGoal's entry;
		 * branch<ltgFail isolates it to s_ltgFail's own increment site.
		 * Unconditional; only the [ACTSEL] print that reports it is gated. */
		WiredIntel_LtgFailBranchCount();

		WiredIntel_TrySetExitGoal( bs );
		/* Emit AFTER both the fail counter and the branch counter have moved for this
		 * evaluation, and after TrySetExitGoal has had its chance to increment the
		 * spine counters — so [ACTSEL] and [EXITCHAIN] describe the same instant. */
		WiredIntel_ActSelReport( bs );
		return qfalse;
	}

	/* STEP 4 — THE COMPARISON REPLACES THE COUPLING.
	 *
	 * Until now an item goal SUPPRESSED the exit goal: the exit was reachable only on
	 * the `!SelectBestItemGoal` branch above, so a fallback chain decided by position
	 * rather than by value.  Measured: 703 LTG calls, 702 successes, TrySetExitGoal
	 * entered 0 times, 0 doors, 0 buttons, exit reached 0/N.  One candidate starved
	 * the other permanently because a chain cannot compare.
	 *
	 * Both candidates now produce a score on the shipped [0,100] item scale and the
	 * shipped comparator decides.  WiredIntel_MapGoalBeatsBest is already generic —
	 * it takes (score, cand, bestScore, best, haveBest) and breaks ties on
	 * (iteminfo, entitynum) — so this generalises the shipped mechanism rather than
	 * adding one.
	 *
	 * SCOPE: item vs exit ONLY.  Door stays out (its magnitude needs the mutating
	 * openGap probe), ride stays out (a forced read has no deliverFrontier).
	 * Combat remains a hard preempt and the directive lock remains an authority —
	 * both OUTSIDE this comparison, untouched.
	 *
	 * ARENA SAFETY: WiredIntel_ExitCandidateScore returns < 0 when FindExitOrigin
	 * fails, i.e. the exit is ABSENT rather than a zero-scoring present candidate,
	 * so on a map with no changelevel the comparison sees exactly one candidate class
	 * and the item goal is pushed exactly as before.
	 *
	 * The weight and the exit's distance divisor are NOT invented here: both are read
	 * from the sweep cvars, which have no shipped defaults baked in — absent a sweep
	 * value the exit scores < 0 and behaviour is byte-identical to today. */
	{
		/* The exit is not a bot_goal_t candidate with an iteminfo/entitynum, so the
		 * comparator's tie-break fields do not apply to it.  A strict `>` on the two
		 * scores is exactly what MapGoalBeatsBest's first clause does, and using that
		 * clause directly keeps the item-vs-item path (which DOES need the tie-break)
		 * completely untouched. */
		float exitScore = WiredIntel_ExitCandidateScore( bs );
		if ( exitScore > 0.0f && exitScore > s_lastItemScore ) {
			WiredIntel_TrySetExitGoal( bs );
			WiredIntel_ActSelReport( bs );
			return qfalse;   /* exit won: no item goal pushed this evaluation */
		}
	}

	trap_BotPushGoal( bs->gs, &goal );
	WiredIntel_ActSelReport( bs );
	return qtrue;
}

int WiredIntel_ChooseNBGItem( bot_state_t *bs, int tfl, bot_goal_t *ltg, float range ) {
	bot_goal_t goal;

	if ( !bs || !bs->wiredIntelActive ) {
		return qfalse;
	}

	if ( range <= 0.0f ) {
		range = 150.0f;
	}

	if ( !WiredIntel_SelectBestItemGoal( bs, tfl, ltg, range, &goal ) ) {
		WiredIntel_ActSelReport( bs );
		return qfalse;
	}

	trap_BotPushGoal( bs->gs, &goal );
	WiredIntel_ActSelReport( bs );
	return qtrue;
}

int WiredIntel_Chat( bot_state_t *bs, const char *eventName, const wbChatCtx_t *ctx ) {
	char chatText[WI_CHAT_TEXT_STR];

	if ( !bs || !bs->wiredIntelActive || !eventName || !eventName[0] || !ctx ) {
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
