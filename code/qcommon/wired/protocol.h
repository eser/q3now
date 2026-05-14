// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

/*
===========================================================================
wired/protocol.h — engine-side entry point for game/engine protocol-shared
                   constants and types.

PURPOSE
-------
Wired is the engine; q3now is one game built on it. The network protocol
contract — entityState_t / playerState_t / usercmd_t (already in q_shared.h
since they're engine-owned), plus the enums and registries below that
both engine code (snapshot encoding, MCP diagnostics, observer JSON, HUD
compat) and game/cgame modules need — lives here.

DEPENDENCY DIRECTION
--------------------
The architectural rule is one-way: engine knows nothing about specific
games. Engine code includes wired/protocol.h. Game/cgame includes
bg_public.h, which includes wired/protocol.h transitively, so game-side
sees the same types under the same names with no source change.

CONTENTS
--------
This is the single source of truth for the wired-engine ↔ game protocol
contract. bg_public.h `#include`s this file near its top so game/cgame
see all the names; engine code includes this file directly.

  - Configstring slot indices (CS_*) — server→client string-table layout
  - Content masks (MASK_*)
  - pmtype_t (PM_*) — playerState_t.pm_type values
  - persEnum_t (PERS_*) — playerState_t.persistant[] indexes
  - statIndex_t (STAT_*) — playerState_t.stats[] indexes
  - entityType_t (ET_*) — entityState_t.eType values
  - weapon_t (WP_*), powerup_t (PW_*), holdable_t (HI_*),
    meansOfDeath_t (MOD_*), projectileType_t (PROJ_*), team_t (TEAM_*),
    gametype_t (GT_*) — all the wire-protocol enums
  - itemType_t / gitem_t / bg_itemlist[] / bg_numItems
  - attackType_t / gattack_t / bg_attacklist[]
  - gweapon_t / bg_weaponlist[]
  - ggametype_t / bg_gametypelist[]
  - Function decls: BG_ModShortName, BG_AttackByShortname

Items that remain in bg_public.h (game-private, engine doesn't see):
gameplay constants (DEFAULT_GRAVITY, MAX_HEALTH, …), weaponstate_t,
gender_t, EV_* event enum, animation_t, the pmove_t struct, BG_Gametype*
helpers, etc.
===========================================================================
*/

#ifndef WIRED_PROTOCOL_H
#define WIRED_PROTOCOL_H

/* core.h supplies the foundational primitives (vec3_t, qboolean, byte,
 * MAX_QPATH, MAX_OSPATH, fileHandle_t, …) every wired module needs. */
#include "core.h"

/* surfaceflags.h supplies the CONTENTS_* bit flags that the MASK_* trace
 * masks below combine. */
#include "../surfaceflags.h"

/* FEAT_* macros gate optional enum members (e.g. STAT_FROZENSTATE under
 * FEAT_FREEZETAG). q_feats.h is the single source of truth. */
#include "../q_feats.h"


/* ── Protocol-network-level limits ────────────────────────────────────── */

#define	SNAPFLAG_RATE_DELAYED	1
#define	SNAPFLAG_NOT_ACTIVE		2	// snapshot used during connection and for zombies
#define SNAPFLAG_SERVERCOUNT	4	// toggled every map_restart so transitions can be detected

#define	ANGLE2SHORT(x)	((int)((x)*65536/360) & 65535)
#define	SHORT2ANGLE(x)	((x)*(360.0/65536))

// per-level limits
#define	MAX_CLIENTS			64		// absolute limit
#define MAX_LOCATIONS		64

#define	GENTITYNUM_BITS		10		// don't need to send any more
#define	MAX_GENTITIES		(1<<GENTITYNUM_BITS)

// entitynums are communicated with GENTITY_BITS, so any reserved
// values that are going to be communcated over the net need to
// also be in this range
#define	ENTITYNUM_NONE		(MAX_GENTITIES-1)
#define	ENTITYNUM_WORLD		(MAX_GENTITIES-2)
#define	ENTITYNUM_MAX_NORMAL	(MAX_GENTITIES-2)

#define	MAX_MODELS			256		// these are sent over the net as 8 bits
#define	MAX_SOUNDS			256		// so they cannot be blindly increased

#define	MAX_CONFIGSTRINGS	1024

// these are the only configstrings that the system reserves, all the
// other ones are strictly for servergame to clientgame communication
#define	CS_SERVERINFO		0		// an info string with all the serverinfo cvars
#define	CS_SYSTEMINFO		1		// an info string for server system to client system configuration (timescale, etc)
#define	RESERVED_CONFIGSTRINGS	2	// game can't modify below this, only the system can

#define	MAX_GAMESTATE_CHARS	16000
typedef struct gameState_s {
	int			stringOffsets[MAX_CONFIGSTRINGS];
	char		stringData[MAX_GAMESTATE_CHARS];
	int			dataCount;
} gameState_t;

// player_state bit-field limits
#define	MAX_STATS				16
#define	MAX_PERSISTANT			24		// increased from 20 for observer/web stat slots
#define	MAX_POWERUPS			16
#define	MAX_WEAPONS				16
#define	MAX_ATTACKS				32
#define	MAX_PS_EVENTS			2

#define PS_PMOVEFRAMECOUNTBITS	6


/* ── Collision / trace primitives ─────────────────────────────────────── */

// plane_t structure
// !!! if this is changed, it must be changed in asm code too !!!
typedef struct cplane_s {
	vec3_t	normal;
	float	dist;
	byte	type;			// for fast side tests: 0,1,2 = axial, 3 = nonaxial
	byte	signbits;		// signx + (signy<<1) + (signz<<2), used as lookup during collision
	byte	pad[2];
} cplane_t;

// trace type for collision detection
typedef enum {
	TT_NONE,
	TT_AABB,
	TT_CAPSULE,
	TT_BISPHERE
} traceType_t;

// a trace is returned when a box is swept through the world
typedef struct trace_s {
	qboolean	allsolid;	// if true, plane is not valid
	qboolean	startsolid;	// if true, the initial point was in a solid area
	float		fraction;	// time completed, 1.0 = didn't hit anything
	vec3_t		endpos;		// final position
	cplane_t	plane;		// surface normal at impact, transformed to world space
	int			surfaceFlags;	// surface hit
	int			contents;	// contents on other side of surface hit
	int			entityNum;	// entity the contacted surface is a part of
} trace_t;
// trace->entityNum can also be 0 to (MAX_GENTITIES-1)
// or ENTITYNUM_NONE, ENTITYNUM_WORLD

// markfragments are returned by R_MarkFragments()
typedef struct markFragment_s {
	int		firstPoint;
	int		numPoints;
} markFragment_t;

typedef struct orientation_s {
	vec3_t		origin;
	vec3_t		axis[3];
} orientation_t;


/* ── Key-catcher and sound-channel enums ──────────────────────────────── */

// in order from highest priority to lowest
// if none of the catchers are active, bound key strings will be executed
#define KEYCATCH_CONSOLE    0x0001
#define KEYCATCH_UI         0x0002
#define KEYCATCH_MESSAGE    0x0004
#define KEYCATCH_CGAME      0x0008

// sound channels
// channel 0 never willingly overrides
// other channels will always override a playing sound on that channel
typedef enum {
	CHAN_AUTO,
	CHAN_LOCAL,		// menu sounds, etc
	CHAN_WEAPON,
	CHAN_VOICE,
	CHAN_ITEM,
	CHAN_BODY,
	CHAN_LOCAL_SOUND,	// chat messages, etc
	CHAN_ANNOUNCER		// announcer voices, etc
} soundChannel_t;


/* ── Configstring slot indices ──────────────────────────────────────────
 * Server → all clients string-table slots. CS_SERVERINFO and CS_SYSTEMINFO
 * are in q_shared.h (engine-owned). Game-specific extensions follow. */

#define	CS_MUSIC				2
#define	CS_MESSAGE				3		// from the map worldspawn's message field
#define	CS_MOTD					4		// g_motd string for server message of the day
#define	CS_WARMUP				5		// server time when the match will be restarted
#define	CS_SCORES1				6
#define	CS_SCORES2				7
#define CS_VOTE_TIME			8
#define CS_VOTE_STRING			9
#define	CS_VOTE_YES				10
#define	CS_VOTE_NO				11

#define CS_TEAMVOTE_TIME		12
#define CS_TEAMVOTE_STRING		14
#define	CS_TEAMVOTE_YES			16
#define	CS_TEAMVOTE_NO			18

#define	CS_GAME_VERSION			20
#define	CS_LEVEL_START_TIME		21		// so the timer only shows the current level
#define	CS_INTERMISSION			22		// when 1, scorelimit/timelimit has been hit and intermission will start in a second or two
#define CS_FLAGSTATUS			23		// string indicating flag status in CTF
#define CS_SHADERSTATE			24
#define CS_BOTINFO				25

#define	CS_ITEMS				27		// string of 0's and 1's that tell which items are present

#define	CS_MODELS				32
#define	CS_SOUNDS				(CS_MODELS+MAX_MODELS)
#define	CS_PLAYERS				(CS_SOUNDS+MAX_SOUNDS)
#define CS_BOTDIRECTIVES		(CS_PLAYERS+MAX_CLIENTS)	// one slot per client; server→client directive display
#define CS_LOCATIONS			(CS_BOTDIRECTIVES+MAX_CLIENTS)
#define CS_PARTICLES			(CS_LOCATIONS+MAX_LOCATIONS)

#define CS_MAX					(CS_PARTICLES+MAX_LOCATIONS)

#if (CS_MAX) > MAX_CONFIGSTRINGS
#error overflow: (CS_MAX) > MAX_CONFIGSTRINGS
#endif

#if FEAT_SCREENSHOT_TOOLS
#define CS_STOPTIME				(MAX_CONFIGSTRINGS-1)	// spectator bullet-time freeze timestamp
#endif

#if FEAT_GAME_MEETING
#define CS_MEETING				(CS_MAX+0)				// "1" while pre-match lobby is active
#define CS_CLIENTS_READY		(CS_MAX+1)				// packed bitmask of ready players
#if (CS_CLIENTS_READY) >= MAX_CONFIGSTRINGS
#error overflow: CS_CLIENTS_READY >= MAX_CONFIGSTRINGS
#endif
#endif

// Lightstyle pattern strings (game → renderer channel).
// 64 slots: slot i = style i, covering styles 0-63.
//   Styles  0-10: vanilla Q1 animated pattern strings (e.g. "mmnmmommommnonmmonqnmmo")
//   Styles 11-31: "m" (constant baseline, full brightness)
//   Styles 32-63: "" (off) or "m" (on) for trigger-controlled switchable lights
// Value stored as a pattern string; renderer animates at 10Hz from character sequence.
#define CS_LIGHTSTYLES		(CS_MAX+2)
#define CS_MAX_LIGHTSTYLES	64
#define LIGHTSTYLE_PATTERN_MAX	64
#if (CS_LIGHTSTYLES + CS_MAX_LIGHTSTYLES - 1) >= MAX_CONFIGSTRINGS
#error overflow: CS_LIGHTSTYLES range exceeds MAX_CONFIGSTRINGS
#endif


/* ── Content masks ──────────────────────────────────────────────────────
 * Trace mask combinations of CONTENTS_* flags from q_shared.h. Used by
 * sv_*, cm_*, bg_pmove and game/cgame logic alike. */
#define	MASK_ALL				(-1)
#define	MASK_SOLID				(CONTENTS_SOLID)
#define	MASK_PLAYERSOLID		(CONTENTS_SOLID|CONTENTS_PLAYERCLIP|CONTENTS_BODY)
#define	MASK_DEADSOLID			(CONTENTS_SOLID|CONTENTS_PLAYERCLIP)
#define	MASK_WATER				(CONTENTS_WATER|CONTENTS_LAVA|CONTENTS_SLIME)
#define	MASK_OPAQUE				(CONTENTS_SOLID|CONTENTS_SLIME|CONTENTS_LAVA)
#define	MASK_SHOT				(CONTENTS_SOLID|CONTENTS_BODY|CONTENTS_CORPSE)


/* ── pmtype_t ───────────────────────────────────────────────────────────
 * Player movement type. Embedded in playerState_t.pm_type by the pmove
 * code; engine reads it for spectator/dead-state checks. */
typedef enum {
	PM_NORMAL,		// can accelerate and turn
	PM_NOCLIP,		// noclip movement
	PM_SPECTATOR,	// still run into walls
	PM_DEAD,		// no acceleration or turning, but free falling
	PM_FREEZE,		// stuck in place with no control
	PM_INTERMISSION,
#if FEAT_GAME_MEETING
	PM_MEETING,		// frozen in pre-match lobby until all players ready
#endif
} pmtype_t;


/* ── meansOfDeath_t / projectileType_t / BG_ModShortName ────────────────
 * Death-cause classification used in obituary events and engine telemetry
 * (wn_events.c calls BG_ModShortName for log strings). */

typedef enum {
	MOD_UNKNOWN,
	MOD_SHOTGUN,
	MOD_GAUNTLET,
	MOD_MACHINEGUN,
	MOD_GRENADE,
	MOD_GRENADE_SPLASH,
	MOD_ROCKET,
	MOD_ROCKET_SPLASH,
	MOD_PLASMA,
	MOD_RAILGUN,
	MOD_LIGHTNING,
	MOD_LIGHTNING_DISCHARGE,
	// alt-fire means of death
	MOD_GAUNTLET_LUNGE,
	MOD_MACHINEGUN_BURST,
	MOD_SHOTGUN_DOUBLE_BLAST,
	MOD_ROCKET_MORTAR,
	MOD_ROCKET_MORTAR_SPLASH,
	MOD_LIGHTNING_CHAIN_ARC,
	MOD_WATER,
	MOD_SLIME,
	MOD_LAVA,
	MOD_CRUSH,
	MOD_TELEFRAG,
	MOD_FALLING,
	MOD_SUICIDE,
	MOD_TARGET_LASER,
	MOD_TRIGGER_HURT,
	MOD_KAMIKAZE,
	MOD_GRAPPLE,
	MOD_LAVABALL,	/* Q1 misc_fireball projectile */
	MOD_NAIL		/* Q1 trap_spikeshooter spike/superspike/laser */
} meansOfDeath_t;

typedef enum {
	PROJ_NONE,        /* default / generic explosion fallback */
	PROJ_ROCKET,      /* rocket launcher missile */
	PROJ_GRENADE,     /* grenade launcher projectile */
	PROJ_PLASMA,      /* plasma rifle bolt */
	PROJ_SPIKE,       /* Q1 nail/spike */
	PROJ_LASER,       /* Q1 enforcer laser */
	PROJ_LAVABALL,    /* Q1 fireball/lavaball */
	PROJ_SHOTGUN,     /* shotgun pellet impact (hitscan direct caller) */
	PROJ_MACHINEGUN,  /* machinegun bullet impact (hitscan direct caller) */
	PROJ_RAILGUN,     /* railgun slug impact (hitscan direct caller) */
	PROJ_NUM_TYPES
} projectileType_t;

const char *BG_ModShortName( meansOfDeath_t mod );


/* ── powerup_t / holdable_t ─────────────────────────────────────────────
 * Slot indices into playerState_t.powerups[] / .stats[STAT_HOLDABLE_BITS]. */

// NOTE: may not have more than 16
typedef enum {
	PW_NONE,

	PW_QUAD,
	PW_BERSERK,
	PW_BATTLESUIT,
	PW_HASTE,
	PW_INVIS,
	PW_REGEN,
	PW_FLIGHT,
	PW_DEFLECTOR,

	PW_REDFLAG,
	PW_BLUEFLAG,
	PW_NEUTRALFLAG,

	PW_KING,

	PW_NUM_POWERUPS

} powerup_t;

typedef enum {
	HI_NONE,

	HI_TELEPORTER,
	HI_MEDKIT,
	HI_KAMIKAZE,
	HI_PORTAL,
	HI_DEFLECTOR,

	HI_KEY_GOLD,
	HI_KEY_SILVER,

	HI_NUM_HOLDABLE
} holdable_t;


/* ── attackType_t / gattack_t / bg_attacklist ───────────────────────────
 * Per-weapon primary/alt-fire attack registry. The struct's function
 * pointers take pmove_t which stays game-internal; forward declared here. */

struct pmove_s;

typedef enum {
	ATT_NONE,

	ATT_GAUNTLET_PRIMARY,
	ATT_GAUNTLET_LUNGE,
	ATT_MACHINEGUN_PRIMARY,
	ATT_MACHINEGUN_BURST,
	ATT_SHOTGUN_PRIMARY,
	ATT_SHOTGUN_DOUBLE_BLAST,
	ATT_GRENADE_LAUNCHER_PRIMARY,
	ATT_ROCKET_LAUNCHER_PRIMARY,
	ATT_ROCKET_LAUNCHER_MORTAR,
	ATT_LIGHTNING_GUN_PRIMARY,
	ATT_LIGHTNING_GUN_CHAIN_ARC,
	ATT_RAILGUN_PRIMARY,
	ATT_PLASMA_RIFLE_PRIMARY,

	ATT_NUM_ATTACKS
} attackType_t;

typedef struct gattack_s {
	char		*name;
	char		 shortname[16];	// Lua bot shortname (e.g. "lg1", "rl2")
	int			 weapon;
	float		 maxDamageDistance;
	qboolean	 armorPiercing;
	float		 knockbackScale;
	float		 selfKnockbackScale;
	float		 recoilKick;
	int			 reloadTime;
	int			 meansOfDeath;			// MOD_ value for direct hit kills
	int			 splashMeansOfDeath;	// MOD_ value for splash kills (0 if no splash)

	// Alt-fire callbacks (NULL = use default fire behavior). pmove_t is
	// forward-declared above; full definition is game-side.
	qboolean	 (*onAltFireStart)(struct pmove_s *pm);
	qboolean	 (*onAltFireThink)(struct pmove_s *pm);
	void		 (*onAltFireRelease)(struct pmove_s *pm);
} gattack_t;

extern	gattack_t	bg_attacklist[];
int BG_AttackByShortname( const char *shortname );	// returns ATT_NONE if not found


/* ── gweapon_t / bg_weaponlist ──────────────────────────────────────────
 * Per-weapon configuration registry. */
typedef struct gweapon_s {
	char		*name;
	char		*shortname;

	vec_t		*color;
	qboolean	 switchOnCycle;
	qboolean	 switchOnOutOfAmmo;

	qboolean	 tossOnDeath;

	int			 ammoBox;
	int			 minAmmunition;
	int			 maxAmmunition;

	qboolean	 spawnWeapon;
	int			 spawnAmmunition;

	int			 attack;		// attackType_t index into bg_attacklist[]
	int			 attackAlt;		// ATT_NONE if no alt-fire

	float		 weight;
} gweapon_t;

extern	gweapon_t	bg_weaponlist[];


/* ── playerState_t->persistant[] indexes ─────────────────────────────────
 * These fields are the only part of player_state that isn't cleared on
 * respawn. NOTE: may not have more than 16. PERS_SCORE must remain at
 * index 0 — server and game both reference it positionally. */
typedef enum {
	PERS_SCORE,						// !!! MUST NOT CHANGE, SERVER AND GAME BOTH REFERENCE !!!
	PERS_HITS,						// total points damage inflicted so damage beeps can sound on change
	PERS_RANK,						// player rank or team rank
	PERS_TEAM,						// player team
	PERS_SPAWN_COUNT,				// incremented every respawn
	PERS_PLAYEREVENTS,				// 16 bits that can be flipped for events
	PERS_LAST_ATTACKER,				// clientnum of last damage inflicter
	PERS_KILLED,					// count of the number of times you died
	// player awards tracking
	PERS_IMPRESSIVE_COUNT,			// two railgun hits in a row
	PERS_EXCELLENT_COUNT,			// two successive kills in a short amount of time
	PERS_DEFEND_COUNT,				// defend awards
	PERS_ASSIST_COUNT,				// assist awards
	PERS_GAUNTLET_FRAG_COUNT,		// kills with the guantlet
	PERS_CAPTURES,					// captures
	PERS_KILLING_SPREE_COUNT,		// 5-kill streaks
	PERS_RAMPAGE_COUNT,				// 10-kill streaks
	PERS_MASSACRE_COUNT,			// 15-kill streaks
	PERS_UNSTOPPABLE_COUNT,			// 20-kill streaks
	// observer/web stats (synced each frame by ClientEndFrame)
	PERS_TOTAL_SHOTS,				// total shots fired across all weapons
	PERS_TOTAL_HITS,				// total hits across all weapons
	PERS_TOTAL_DAMAGE,				// total damage dealt
} persEnum_t;


/* ── playerState_t->stats[] indexes ─────────────────────────────────────
 * NOTE: may not have more than 16. */
typedef enum {
	STAT_HEALTH,					// 16 bit fields
	STAT_ARMOR,
	STAT_ARMORCLASS,
	STAT_WEAPONS,
	STAT_HOLDABLE_ITEM,				// deprecated: single-slot index (kept for bot AI compat)
	STAT_HOLDABLE_BITS,				// bitmask: bit N set means player owns HI_* type N
	STAT_DEAD_YAW,					// look this direction when dead (FIXME: get rid of?)
	STAT_CLIENTS_READY,				// bit mask of clients wishing to exit the intermission (FIXME: configstring?)

	STAT_JUMPTIME,
	STAT_RAILTIME,					// PM: Added for allowchange

	STAT_WALLJUMPS,
#if FEAT_FREEZETAG
	STAT_FROZENSTATE,				// 0=normal, 1=frozen, 2=thawing (7A)
#endif
	STAT_CAMPER,					// 0-255: camping punishment darkness level (11C)
#if FEAT_MOVEMENT_KEYS
	STAT_KEYS						// bitmask of movement keys (spectator follow only)
#endif
} statIndex_t;


/* ── weapon_t ───────────────────────────────────────────────────────────
 * Weapon identifiers used in playerState/usercmd/entityState across the
 * wire. WP_NUM_WEAPONS is the array-size sentinel. */
typedef enum {
	WP_NONE,

	WP_GAUNTLET,
	WP_MACHINEGUN,
	WP_SHOTGUN,
	WP_GRENADE_LAUNCHER,
	WP_ROCKET_LAUNCHER,
	WP_LIGHTNING_GUN,
	WP_RAILGUN,
	WP_PLASMA_RIFLE,

	WP_NUM_WEAPONS
} weapon_t;


/* ── team_t ─────────────────────────────────────────────────────────────
 * Player-team identifier. TEAM_NUM_TEAMS is the array-size sentinel.
 * Distinct from "say-team" chat constants and from the per-stat slots. */
typedef enum {
	TEAM_FREE,
	TEAM_RED,
	TEAM_BLUE,
	TEAM_SPECTATOR,
	TEAM_4,
	TEAM_5,
	TEAM_6,
	TEAM_7,

	TEAM_NUM_TEAMS
} team_t;


/* ── entityType_t ───────────────────────────────────────────────────────
 * entityState_t->eType. Values >= ET_EVENTS are event-carriers — see
 * comment on ET_EVENTS. */
typedef enum {
	ET_GENERAL,
	ET_PLAYER,
	ET_ITEM,
	ET_MISSILE,
	ET_MOVER,
	ET_BEAM,
	ET_PORTAL,
	ET_SPEAKER,
	ET_PUSH_TRIGGER,
	ET_TELEPORT_TRIGGER,
	ET_INVISIBLE,
	ET_GRAPPLE,				// grapple hooked on wall
	ET_TEAM,

	ET_EVENTS				// any of the EV_* events can be added freestanding
							// by setting eType to ET_EVENTS + eventNum
							// this avoids having to set eFlags and eventNum
} entityType_t;


/* ── gametype_t / ggametype_t / bg_gametypelist ─────────────────────────
 * Wire-side gametype identifier and its metadata table. Engine uses
 * GT_MAX_GAME_TYPE for bounds checks (see wn_mcp.c). Game-side helper
 * functions (BG_GametypeBits, BG_IsTeamGametype, …) stay in bg_public.h
 * since engine doesn't call them. */

typedef enum {
	GT_DEATHMATCH,		// deathmatch
	GT_DUEL,			// one on one tournament

	GT_KINGOFTHEHILL,
	GT_LASTMANSTANDING,

	//-- team games go after this --

	GT_TDM,			// team deathmatch
	GT_CTF,				// capture the flag
	GT_1FCTF,
	GT_OBELISK,
	GT_HARVESTER,
#if FEAT_FREEZETAG
	GT_FREEZETAG,
#endif
	GT_MAX_GAME_TYPE
} gametype_t;

#define GF_CAMPAIGN         0x01

// Gametype metadata — indexed by gametype_t
typedef struct ggametype_s {
	char		*name;
	char		*shortname;
	char		**parseTokens;		// NULL-terminated token list for arena/BSP matching
	char		*hudToken;			// ModernHUD visibility token (e.g. "gt_ffa"), "" if none
} ggametype_t;

extern	ggametype_t	bg_gametypelist[];


/* ── itemType_t / gitem_t / bg_itemlist ─────────────────────────────────
 * Item registry. Engine snapshot code, MCP diagnostics, and HUD compat
 * need to read entries from bg_itemlist, so the type and registry both
 * live here. The data itself (the bg_itemlist[] array body) is defined
 * once in code/game/bg_misc.c — that file is game-side. */

// gitem_t->type
typedef enum {
	IT_BAD,
	IT_WEAPON,				// EFX: rotate + upscale + minlight
	IT_AMMO,				// EFX: rotate
	IT_ARMOR,				// EFX: rotate + minlight
	IT_HEALTH,				// EFX: static external sphere + rotating internal
	IT_POWERUP,				// instant on, timer based
							// EFX: rotate + external ring that rotates
	IT_HOLDABLE,			// single use, holdable item
							// EFX: rotate + bob
	IT_TEAM
} itemType_t;

#define MAX_ITEM_MODELS     4

typedef struct gitem_s {
	char		*classname;	// spawning name
	char		*pickup_sound;
	char		*world_model[MAX_ITEM_MODELS];
	float		q1OriginZOffset;	// Z added to entity origin when the item came from a Q1 BSP

	char		*icon;
	char		*pickup_name;	// for printing on pickup

	int			quantity;		// for ammo how much, or duration of powerup
	itemType_t	giType;			// IT_* flags

	int			giTag;

	char		*precaches;		// string of all models and images this item will use
	char		*sounds;		// string of all sounds this item will use
} gitem_t;

// included in both the game dll and the client
extern	gitem_t	bg_itemlist[];
extern	int		bg_numItems;


/* ── Wire-protocol structs ────────────────────────────────────────────── */

// usercmd_t->button bits, many of which are generated by the client system,
// so they aren't game/cgame only definitions
#define	BUTTON_ATTACK_PRI	1
#define	BUTTON_ATTACK_SEC	2			// secondary fire (bit 1, slot next to +attack)
#define	BUTTON_TALK			4096		// displays talk balloon and disables actions
#define	BUTTON_USE_HOLDABLE	4
#define	BUTTON_GESTURE		8
#define	BUTTON_WALKING		16			// walking can't just be inferred from MOVE_RUN
										// because a key pressed late in the frame will
										// only generate a small move value for that frame
										// walking will use different animations and
										// won't generate footsteps
#define BUTTON_AFFIRMATIVE	32
#define	BUTTON_NEGATIVE		64

#define BUTTON_GETFLAG		128
#define BUTTON_GUARDBASE	256
#define BUTTON_PATROL		512
#define BUTTON_FOLLOWME		1024

#define	BUTTON_ANY			2048		// any key whatsoever

#define	MOVE_RUN			120			// if forwardmove or rightmove are >= MOVE_RUN,
										// then BUTTON_WALKING should be set

// usercmd_t is sent to the server each client frame
typedef struct usercmd_s {
	int				serverTime;
	int				angles[3];
	int 			buttons;
	byte			weapon;
	signed char		forwardmove, rightmove, upmove;
} usercmd_t;

// if entityState->solid == SOLID_BMODEL, modelindex is an inline model number
#define	SOLID_BMODEL	0xffffff

typedef enum {
	TR_STATIONARY,
	TR_INTERPOLATE,				// non-parametric, but interpolate between snapshots
	TR_LINEAR,
	TR_LINEAR_STOP,
	TR_SINE,					// value = base + sin( time / duration ) * delta
	TR_GRAVITY,
	// wired: additional trajectory types used by bg_misc.c
	TR_GRAVITY_DOUBLE,			// double strength gravity
	TR_ACCELERATE,				// accelerating linear
	TR_SMALL_GRAVITY,			// half strength gravity
	TR_ORBITAL					// gravity + centripetal
} trType_t;

typedef struct trajectory_s {
	trType_t	trType;
	int		trTime;
	int		trDuration;			// if non 0, trTime + trDuration = stop time
	vec3_t	trBase;
	vec3_t	trDelta;			// velocity, etc
} trajectory_t;

// playerState_t is the information needed by both the client and server
// to predict player motion and actions
// nothing outside of pmove should modify these, or some degree of prediction error
// will occur
//
// playerState_t is a full superset of entityState_t as it is used by players,
// so if a playerState_t is transmitted, the entityState_t can be fully derived
// from it.
typedef struct playerState_s {
	int			commandTime;	// cmd->serverTime of last executed command
	int			pm_type;
	int			bobCycle;		// for view bobbing and footstep generation
	int			pm_flags;		// ducked, jump_held, etc
	int			pm_time;

	vec3_t		origin;
	vec3_t		velocity;
	int			weaponTime;
	int			gravity;
	int			speed;
	int			delta_angles[3];	// add to command angles to get view direction
									// changed by spawns, rotating objects, and teleporters

	int			groundEntityNum;// ENTITYNUM_NONE = in air

	int			legsTimer;		// don't change low priority animations until this runs out
	int			legsAnim;		// mask off ANIM_TOGGLEBIT

	int			torsoTimer;		// don't change low priority animations until this runs out
	int			torsoAnim;		// mask off ANIM_TOGGLEBIT

	int			movementDir;	// a number 0 to 7 that represents the relative angle
								// of movement to the view angle (axial and diagonals)
								// when at rest, the value will remain unchanged
								// used to twist the legs during strafing

	vec3_t		grapplePoint;	// location of grapple to pull towards if PMF_GRAPPLE_PULL

	int			eFlags;			// copied to entityState_t->eFlags

	int			eventSequence;	// pmove generated events
	int			events[MAX_PS_EVENTS];
	int			eventParms[MAX_PS_EVENTS];

	int			externalEvent;	// events set on player from another source
	int			externalEventParm;
	int			externalEventTime;

	int			clientNum;		// ranges from 0 to MAX_CLIENTS-1
	int			weapon;			// copied to entityState_t->weapon
	int			weaponstate;
	int			burstRoundsRemaining;	// rounds left in current burst fire
	int			chargeStartTime;		// time when alt-fire charge began (0 = not charging)
	int			cooldownEndTime;		// time when weapon cooldown ends (0 = no cooldown)
	int			doubleBlastState;		// state for shotgun double-blast (0 = idle, 1 = first blast fired, waiting for second)

	vec3_t		viewangles;		// for fixed views
	int			viewheight;

	// damage feedback
	int			damageEvent;	// when it changes, latch the other parms
	int			damageYaw;
	int			damagePitch;
	int			damageCount;

	int			stats[MAX_STATS];
	int			persistant[MAX_PERSISTANT];	// stats that aren't cleared on death
	int			powerups[MAX_POWERUPS];	// level.time that the powerup runs out
	int			ammo[MAX_WEAPONS];

	int			generic1;
	int			loopSound;
	int			jumppad_ent;	// jumppad entity hit this frame

	// not communicated over the net at all
	int			ping;			// server to game info for scoreboard
	int			pmove_framecount;	// FIXME: don't transmit over the network
	int			jumppad_frame;
	int			entityEventSequence;
} playerState_t;

// entityState_t is the information conveyed from the server
// in an update message about entities that the client will
// need to render in some way
// Different eTypes may use the information in different ways
// The messages are delta compressed, so it doesn't really matter if
// the structure size is fairly large
typedef struct entityState_s {
	int		number;			// entity index
	int		eType;			// entityType_t
	int		eFlags;

	trajectory_t	pos;	// for calculating position
	trajectory_t	apos;	// for calculating angles

	int		time;
	int		time2;

	vec3_t	origin;
	vec3_t	origin2;

	vec3_t	angles;
	vec3_t	angles2;

	int		otherEntityNum;	// shotgun sources, etc
	int		otherEntityNum2;

	int		groundEntityNum;	// ENTITYNUM_NONE = in air

	int		constantLight;	// r + (g<<8) + (b<<16) + (intensity<<24)
	int		loopSound;		// constantly loop this sound

	int		modelindex;
	int		modelindex2;
	int		clientNum;		// 0 to (MAX_CLIENTS - 1), for players and corpses
	int		frame;

	int		solid;			// for client side prediction, trap_linkentity sets this properly

	int		event;			// impulse events -- muzzle flashes, footsteps, etc
	int		eventParm;

	// for players
	int		powerups;		// bit flags
	int		weapon;			// determines weapon and flash model, etc
	int		pType;			// projectileType_t — ET_MISSILE visual discriminator
	int		legsAnim;		// mask off ANIM_TOGGLEBIT
	int		torsoAnim;		// mask off ANIM_TOGGLEBIT

	int		generic1;
} entityState_t;

typedef enum {
	CA_UNINITIALIZED,
	CA_DISCONNECTED, 	// not talking to a server
	CA_AUTHORIZING,		// not used any more, was checking cd key
	CA_CONNECTING,		// sending request packets to the server
	CA_CHALLENGING,		// sending challenge packets to the server
	CA_CONNECTED,		// netchan_t established, getting gamestate
	CA_LOADING,			// only during cgame initialization, never during main loop
	CA_PRIMED,			// got gamestate, waiting for first frame
	CA_ACTIVE,			// game views should be displayed
	CA_CINEMATIC		// playing a cinematic or a static pic, not connected to a server
} connstate_t;


#endif /* WIRED_PROTOCOL_H */
