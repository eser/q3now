#ifndef G_BOT_SCRIPTS_SHARED_H
#define G_BOT_SCRIPTS_SHARED_H

#include "../../../qcommon/q_shared.h"
// This header is consumed by both game-module bot AI code AND by the
// engine-side server bot-scripting bridge (sv_lua.h). To keep the engine
// free of any game-side header dependency, source the protocol-shared
// types (WP_NUM_WEAPONS, gitem_t, etc.) from wired/protocol.h directly.
#include "../../../qcommon/wired/protocol.h"

#define WB_WEAPONKEY_STR 16
#define WB_ITEMTYPE_STR 32
#define WB_NAME_STR 64
#define WB_GAMETYPE_STR 16
#define WB_CHAT_TEXT_STR 256

typedef enum {
	WB_POWERUP_SLOT_QUAD = 0,
	WB_POWERUP_SLOT_BATTLESUIT,
	WB_POWERUP_SLOT_HASTE,
	WB_POWERUP_SLOT_INVIS,
	WB_POWERUP_SLOT_REGEN,
	WB_POWERUP_SLOT_FLIGHT,
	WB_POWERUP_SLOT_COUNT
} wbPowerupSlot_t;

typedef enum {
	WB_PROFILE_REACTION_TIME = 0,
	WB_PROFILE_FOV,
	WB_PROFILE_AGGRESSION,
	WB_PROFILE_SELF_PRESERVE,
	WB_PROFILE_VENGEFULNESS,
	WB_PROFILE_CAMP_TENDENCY,
	WB_PROFILE_OPPORTUNISM,
	WB_PROFILE_TRACKING,
	WB_PROFILE_ACCURACY,
	WB_PROFILE_LEAD_SKILL,
	WB_PROFILE_STRAFE_JUMP,
	WB_PROFILE_WEAPON_JUMPING,
	WB_PROFILE_JUMPER,
	WB_PROFILE_DODGING,
	WB_PROFILE_USE_JUMPPADS,
	WB_PROFILE_SWIM,
	WB_PROFILE_FIRETHROTTLE,
	WB_PROFILE_GRAPPLE,    // from traits.grapple_user
	WB_PROFILE_NAVIGATION, // from movement.navigation_skill
	// B.1: per-weapon score bias for the C fallback scorer (from aim.weapon_bias_<key>)
	WB_PROFILE_BIAS_MG,
	WB_PROFILE_BIAS_SG,
	WB_PROFILE_BIAS_GL,
	WB_PROFILE_BIAS_RL,
	WB_PROFILE_BIAS_LG,
	WB_PROFILE_BIAS_RG,
	WB_PROFILE_BIAS_PG,
	// B.2: per-weapon aim skill (from aim.skill_<weapon>)
	WB_PROFILE_SKILL_MG,
	WB_PROFILE_SKILL_SG,
	WB_PROFILE_SKILL_GL,
	WB_PROFILE_SKILL_RL,
	WB_PROFILE_SKILL_LG,
	WB_PROFILE_SKILL_RG,
	WB_PROFILE_SKILL_PG,
	WB_PROFILE_ALERTNESS,      // from traits.alertness
	WB_PROFILE_ATTACK_SKILL,   // from traits.attack_skill
	WB_PROFILE_VIEW_MAXCHANGE, // from traits.view_maxchange (deg/s, not clamped to [0,1])
	// C.1: per-weapon accuracy per slot (aim.accuracy_<weapon>_pri and aim.accuracy_<weapon>_sec).
	// Sentinel: -1.0f = "not set in Lua; inherit from fallback".
	// Fallback chain: slot 1 → slot 0 → WB_PROFILE_ACCURACY → 0.5.
	// Entries are in WP_* index order; S0 (primary) precedes S1 (alt) per weapon.
	WB_PROFILE_ACCURACY_MACHINEGUN_S0,
	WB_PROFILE_ACCURACY_MACHINEGUN_S1,
	WB_PROFILE_ACCURACY_SHOTGUN_S0,
	WB_PROFILE_ACCURACY_SHOTGUN_S1,
	WB_PROFILE_ACCURACY_GRENADE_LAUNCHER_S0,
	WB_PROFILE_ACCURACY_GRENADE_LAUNCHER_S1,
	WB_PROFILE_ACCURACY_ROCKET_LAUNCHER_S0,
	WB_PROFILE_ACCURACY_ROCKET_LAUNCHER_S1,
	WB_PROFILE_ACCURACY_LIGHTNING_GUN_S0,
	WB_PROFILE_ACCURACY_LIGHTNING_GUN_S1,
	WB_PROFILE_ACCURACY_RAILGUN_S0,
	WB_PROFILE_ACCURACY_RAILGUN_S1,
	WB_PROFILE_ACCURACY_PLASMA_RIFLE_S0,
	WB_PROFILE_ACCURACY_PLASMA_RIFLE_S1,
	WB_PROFILE_CHAT_INSULT, // from chats.insult — gate for kill_insult/death_insult sub-selection
	WB_PROFILE_MAX
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
	char itemType[WB_ITEMTYPE_STR];
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
	float powerups[WB_POWERUP_SLOT_COUNT];
	int giType;  /* gitem_t.giType (IT_HEALTH, IT_ARMOR, IT_WEAPON, ...) */
	int giTag;   /* gitem_t.giTag  (weapon index, PW_* powerup, etc.)    */
} wbItemEvalCtx_t;

typedef struct {
	int health;
	int armor;
	int enemyVisible;
	float enemyDist;
	int enemyHealth;
	char enemyWeapon[WB_WEAPONKEY_STR];
	int underFire;
	char lastKiller[WB_NAME_STR];
	char currentEnemy[WB_NAME_STR];
	int teamScore;
	int enemyScore;
	char gametype[WB_GAMETYPE_STR];
	int timeLeft;
} wbDecideCtx_t;

typedef struct {
	char victim[WB_NAME_STR];
	char killer[WB_NAME_STR];
	char weapon[WB_WEAPONKEY_STR];
	int distance;
	int count;
	int won;
	int score;
	int team;
	char sender[WB_NAME_STR];
	char text[WB_CHAT_TEXT_STR];
	char map[WB_NAME_STR];
	char gametype[WB_GAMETYPE_STR];
} wbChatCtx_t;

#endif
