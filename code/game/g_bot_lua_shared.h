#ifndef G_BOT_LUA_SHARED_H
#define G_BOT_LUA_SHARED_H

#include "../qcommon/q_shared.h"

#define BOTLUA_WEAPONKEY_STR 16
#define BOTLUA_ITEMTYPE_STR 32
#define BOTLUA_NAME_STR 64
#define BOTLUA_GAMETYPE_STR 16
#define BOTLUA_CHAT_TEXT_STR 256

typedef enum {
	BOTLUA_WEAPON_SLOT_MG = 0,
	BOTLUA_WEAPON_SLOT_SG,
	BOTLUA_WEAPON_SLOT_GL,
	BOTLUA_WEAPON_SLOT_RL,
	BOTLUA_WEAPON_SLOT_LG,
	BOTLUA_WEAPON_SLOT_RG,
	BOTLUA_WEAPON_SLOT_PG,
	BOTLUA_WEAPON_SLOT_COUNT
} botLuaWeaponSlot_t;

typedef enum {
	BOTLUA_POWERUP_SLOT_QUAD = 0,
	BOTLUA_POWERUP_SLOT_BATTLESUIT,
	BOTLUA_POWERUP_SLOT_HASTE,
	BOTLUA_POWERUP_SLOT_INVIS,
	BOTLUA_POWERUP_SLOT_REGEN,
	BOTLUA_POWERUP_SLOT_FLIGHT,
	BOTLUA_POWERUP_SLOT_COUNT
} botLuaPowerupSlot_t;

typedef enum {
	BOTLUA_PROFILE_REACTION_TIME = 0,
	BOTLUA_PROFILE_FOV,
	BOTLUA_PROFILE_AGGRESSION,
	BOTLUA_PROFILE_SELF_PRESERVE,
	BOTLUA_PROFILE_VENGEFULNESS,
	BOTLUA_PROFILE_CAMP_TENDENCY,
	BOTLUA_PROFILE_OPPORTUNISM,
	BOTLUA_PROFILE_TRACKING,
	BOTLUA_PROFILE_ACCURACY,
	BOTLUA_PROFILE_LEAD_SKILL,
	BOTLUA_PROFILE_STRAFE_JUMP,
	BOTLUA_PROFILE_ROCKET_JUMP,
	BOTLUA_PROFILE_BUNNY_HOP,
	BOTLUA_PROFILE_DODGE_ON_FIRE,
	BOTLUA_PROFILE_USE_JUMPPADS,
	BOTLUA_PROFILE_SWIM,
	BOTLUA_PROFILE_MAX
} botLuaProfileField_t;

typedef struct {
	float enemyDist;
	int enemyHealth;
	int selfHealth;
	int selfArmor;
	int ammo[BOTLUA_WEAPON_SLOT_COUNT];
	int weapons[BOTLUA_WEAPON_SLOT_COUNT];
} botLuaCombatCtx_t;

typedef struct {
	char itemType[BOTLUA_ITEMTYPE_STR];
	vec3_t itemOrigin;
	float itemRespawn;
	int health;
	int armor;
	int hasEnemy;
	float enemyDist;
	int weapons[BOTLUA_WEAPON_SLOT_COUNT];
	int ammo[BOTLUA_WEAPON_SLOT_COUNT];
	float powerups[BOTLUA_POWERUP_SLOT_COUNT];
} botLuaItemEvalCtx_t;

typedef struct {
	int health;
	int armor;
	int enemyVisible;
	float enemyDist;
	int enemyHealth;
	char enemyWeapon[BOTLUA_WEAPONKEY_STR];
	int underFire;
	char lastKiller[BOTLUA_NAME_STR];
	char currentEnemy[BOTLUA_NAME_STR];
	int teamScore;
	int enemyScore;
	char gametype[BOTLUA_GAMETYPE_STR];
	int timeLeft;
} botLuaDecideCtx_t;

typedef struct {
	char victim[BOTLUA_NAME_STR];
	char killer[BOTLUA_NAME_STR];
	char weapon[BOTLUA_WEAPONKEY_STR];
	int distance;
	int count;
	int won;
	int score;
	int team;
	char sender[BOTLUA_NAME_STR];
	char text[BOTLUA_CHAT_TEXT_STR];
	char map[BOTLUA_NAME_STR];
	char gametype[BOTLUA_GAMETYPE_STR];
} botLuaChatCtx_t;

#endif
