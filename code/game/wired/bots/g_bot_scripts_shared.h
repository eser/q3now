// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#ifndef G_BOT_SCRIPTS_SHARED_H
#define G_BOT_SCRIPTS_SHARED_H

#include "../../../qcommon/q_shared.h"
// This header is consumed by both game-module bot AI code AND by the
// engine-side server bot-scripting bridge (sv_lua.h). To keep the engine
// free of any game-side header dependency, source the protocol-shared
// types (WP_NUM_WEAPONS, gitem_t, etc.) from wired/protocol.h directly.
#include "../../../qcommon/wired/protocol.h"

#define WI_WEAPONKEY_STR 16
#define WI_ITEMTYPE_STR 32
#define WI_NAME_STR 64
#define WI_GAMETYPE_STR 16
#define WI_CHAT_TEXT_STR 256

typedef enum {
	WI_POWERUP_SLOT_QUAD = 0,
	WI_POWERUP_SLOT_BATTLESUIT,
	WI_POWERUP_SLOT_HASTE,
	WI_POWERUP_SLOT_INVIS,
	WI_POWERUP_SLOT_REGEN,
	WI_POWERUP_SLOT_FLIGHT,
	WI_POWERUP_SLOT_COUNT
} wbPowerupSlot_t;

typedef enum {
	WI_PROFILE_REACTION_TIME = 0,
	WI_PROFILE_FOV,
	WI_PROFILE_AGGRESSION,
	WI_PROFILE_SELF_PRESERVE,
	WI_PROFILE_VENGEFULNESS,
	WI_PROFILE_CAMP_TENDENCY,
	WI_PROFILE_OPPORTUNISM,
	WI_PROFILE_TRACKING,
	WI_PROFILE_ACCURACY,
	WI_PROFILE_LEAD_SKILL,
	WI_PROFILE_STRAFE_JUMP,
	WI_PROFILE_WEAPON_JUMPING,
	WI_PROFILE_JUMPER,
	WI_PROFILE_DODGING,
	WI_PROFILE_USE_JUMPPADS,
	WI_PROFILE_SWIM,
	WI_PROFILE_FIRETHROTTLE,
	WI_PROFILE_GRAPPLE,    // from traits.grapple_user
	WI_PROFILE_NAVIGATION, // from movement.navigation_skill
	// B.1: per-weapon score bias for the C fallback scorer (from aim.weapon_bias_<key>)
	WI_PROFILE_BIAS_MG,
	WI_PROFILE_BIAS_SG,
	WI_PROFILE_BIAS_GL,
	WI_PROFILE_BIAS_RL,
	WI_PROFILE_BIAS_LG,
	WI_PROFILE_BIAS_RG,
	WI_PROFILE_BIAS_PG,
	// B.2: per-weapon aim skill (from aim.skill_<weapon>)
	WI_PROFILE_SKILL_MG,
	WI_PROFILE_SKILL_SG,
	WI_PROFILE_SKILL_GL,
	WI_PROFILE_SKILL_RL,
	WI_PROFILE_SKILL_LG,
	WI_PROFILE_SKILL_RG,
	WI_PROFILE_SKILL_PG,
	WI_PROFILE_ALERTNESS,      // from traits.alertness
	WI_PROFILE_ATTACK_SKILL,   // from traits.attack_skill
	WI_PROFILE_VIEW_MAXCHANGE, // from traits.view_maxchange (deg/s, not clamped to [0,1])
	// C.1: per-weapon accuracy per slot (aim.accuracy_<weapon>_pri and aim.accuracy_<weapon>_sec).
	// Sentinel: -1.0f = "not set in Lua; inherit from fallback".
	// Fallback chain: slot 1 → slot 0 → WI_PROFILE_ACCURACY → 0.5.
	// Entries are in WP_* index order; S0 (primary) precedes S1 (alt) per weapon.
	WI_PROFILE_ACCURACY_MACHINEGUN_S0,
	WI_PROFILE_ACCURACY_MACHINEGUN_S1,
	WI_PROFILE_ACCURACY_SHOTGUN_S0,
	WI_PROFILE_ACCURACY_SHOTGUN_S1,
	WI_PROFILE_ACCURACY_GRENADE_LAUNCHER_S0,
	WI_PROFILE_ACCURACY_GRENADE_LAUNCHER_S1,
	WI_PROFILE_ACCURACY_ROCKET_LAUNCHER_S0,
	WI_PROFILE_ACCURACY_ROCKET_LAUNCHER_S1,
	WI_PROFILE_ACCURACY_LIGHTNING_GUN_S0,
	WI_PROFILE_ACCURACY_LIGHTNING_GUN_S1,
	WI_PROFILE_ACCURACY_RAILGUN_S0,
	WI_PROFILE_ACCURACY_RAILGUN_S1,
	WI_PROFILE_ACCURACY_PLASMA_RIFLE_S0,
	WI_PROFILE_ACCURACY_PLASMA_RIFLE_S1,
	WI_PROFILE_CHAT_INSULT, // from chats.insult — gate for kill_insult/death_insult sub-selection
	WI_PROFILE_MAX
} wbProfileField_t;

typedef struct {
	float enemyDist;
	int enemyHealth;
	int selfHealth;
	int selfArmor;
	int ammo[WP_NUM_WEAPONS];
	int weapons[WP_NUM_WEAPONS];
} wbCombatCtx_t;

typedef struct {
	char itemType[WI_ITEMTYPE_STR];
	vec3_t itemOrigin;
	float itemRespawn;
	float itemBotDist;   /* distance from bot to item origin */
	float itemEnemyDist; /* distance from nearest enemy to item origin (-1 if no enemy) */
	int health;
	int armor;
	int hasEnemy;
	float enemyDist;
	int weapons[WP_NUM_WEAPONS];
	int ammo[WP_NUM_WEAPONS];
	float powerups[WI_POWERUP_SLOT_COUNT];
	int giType;  /* gitem_t.giType (IT_HEALTH, IT_ARMOR, IT_WEAPON, ...) */
	int giTag;   /* gitem_t.giTag  (weapon index, PW_* powerup, etc.)    */
} wbItemEvalCtx_t;

typedef struct {
	int health;
	int armor;
	int enemyVisible;
	float enemyDist;
	int enemyHealth;
	char enemyWeapon[WI_WEAPONKEY_STR];
	int underFire;
	char lastKiller[WI_NAME_STR];
	char currentEnemy[WI_NAME_STR];
	int teamScore;
	int enemyScore;
	char gametype[WI_GAMETYPE_STR];
	int timeLeft;
	/* Awareness inputs (monster behavior layer; zero for bots — trailing,
	 * memset-zero-safe, not on any net/protocol boundary). sensedNoise = the
	 * monster heard/sensed the enemy this tick without a clean line of sight;
	 * noiseDist = distance to that sensed source. Lets a Lua decide-fn drive the
	 * RELAXED→QUERY→ALERT awareness escalation. */
	int   sensedNoise;
	float noiseDist;
} wbDecideCtx_t;

typedef struct {
	char victim[WI_NAME_STR];
	char killer[WI_NAME_STR];
	char weapon[WI_WEAPONKEY_STR];
	int distance;
	int count;
	int won;
	int score;
	int team;
	char sender[WI_NAME_STR];
	char text[WI_CHAT_TEXT_STR];
	char map[WI_NAME_STR];
	char gametype[WI_GAMETYPE_STR];
} wbChatCtx_t;

#endif
