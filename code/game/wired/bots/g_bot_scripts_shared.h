#ifndef G_BOT_SCRIPTS_SHARED_H
#define G_BOT_SCRIPTS_SHARED_H

#include "../../../qcommon/q_shared.h"
#include "../../bg_public.h"

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
	WB_PROFILE_ROCKET_JUMP,
	WB_PROFILE_BUNNY_HOP,
	WB_PROFILE_DODGE_ON_FIRE,
	WB_PROFILE_USE_JUMPPADS,
	WB_PROFILE_SWIM,
	WB_PROFILE_FIRETHROTTLE,
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
	int health;
	int armor;
	int hasEnemy;
	float enemyDist;
	int weapons[WP_NUM_WEAPONS];
	int ammo[WP_NUM_WEAPONS];
	float powerups[WB_POWERUP_SLOT_COUNT];
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
