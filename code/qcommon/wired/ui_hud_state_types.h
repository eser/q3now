// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
//
// qcommon/wired/ui_hud_state_types.h — HUD state bridge ABI types.
//
// cgame fills wiredHudState_t and pushes it with trap_WiredUI_PushHudState
// (code/cgame/cg_public.h); the client's WiredUI HUD reads the same struct.
// To keep the engine free of cgame headers (AGENTS.md one-way dependency rule)
// the types live here, like ui_viewport_types.h and ui_store_types.h: both
// sides include this header, neither includes the other's. The struct crosses
// the VM boundary, so a layout change is an ABI change (cgame + client rebuilt
// together).

#ifndef QCOMMON_WIRED_UI_HUD_STATE_TYPES_H
#define QCOMMON_WIRED_UI_HUD_STATE_TYPES_H

#include "../q_shared.h"
#include "protocol.h"

// ── Wired UI: HUD state bridge struct ────────────────────────────────
// cgame fills this each frame and pushes to client via CG_WIREDUI_PUSH_HUD_STATE.
// Client-side HUD element code reads from this instead of cg.*/cgs.*.

#if FEAT_WIRED_UI

#define WIRED_HUD_MAX_CLIENTS      128
#define WIRED_HUD_MAX_REWARDSTACK  10
#define WIRED_HUD_MAX_ITEMS        64
#define WIRED_HUD_MAX_TEAMOVERLAY 32
#define WIRED_HUD_MAX_BINDINGS    16
#define WIRED_HUD_MAX_SCORES      64
#define WIRED_LAG_SAMPLES          128

// lagometer data (ring buffers from cgame — frame timing + snapshot health)
typedef struct {
	int     frameSamples[WIRED_LAG_SAMPLES];     // cg.time - cg.latestSnapshotTime per frame
	int     frameCount;                           // frame ring buffer index
	int     snapshotFlags[WIRED_LAG_SAMPLES];    // SNAPFLAG_RATE_DELAYED per snapshot
	int     snapshotSamples[WIRED_LAG_SAMPLES];  // ping per snapshot, -1 = dropped
	int     snapshotCount;                        // snapshot ring buffer index
} wiredLagometer_t;

// per-weapon stat counters (for scoreboard weapon breakdown)
#define WIRED_MAX_WEAPONS  16  // covers all weapon_t values

typedef struct {
	int     hits;
	int     shots;
	int     kills;
	int     deaths;
} wiredWeaponStats_t;

// scoreboard entry (mirrors score_t from cg_local.h, pre-sorted by server)
typedef struct {
	int         client;
	int         score;
	int         ping;
	int         time;
	int         scoreFlags;
	int         powerUps;
	int         accuracy;
	int         impressiveCount;
	int         excellentCount;
	int         guantletCount;
	int         defendCount;
	int         assistCount;
	int         captures;
	int         deaths;
	int         team;
	qboolean    perfect;
	int         killingSpreeCount;
	int         rampageCount;
	int         massacreCount;
	int         unstoppableCount;
	int         totalDamage;        // sum of all weapon damage (pre-computed by cgame)
	int         bestAttack;         // attack with most kills (ATT_* enum, ATT_NONE if none)

	// tournament / duel stats
	int         wins;               // tournament wins
	int         losses;             // tournament losses

	// damage dealt / received
	int         damageDone;         // total damage dealt
	int         damageTaken;        // total damage received

	// per-weapon breakdown
	wiredWeaponStats_t weaponStats[WIRED_MAX_WEAPONS];
} wiredHudScore_t;

// data binding: named stat bundle pushed from cgame, consumed by generic HUD elements
typedef struct {
	char        name[32];       // "health", "armor", "ammo"
	char        text[64];       // "14", "114", "20"
	vec4_t      color;          // pre-computed RGBA
	qhandle_t   icon;           // shader handle (0 = none)
	float       percent;        // 0.0-1.0 for bars
	qboolean    visible;        // hide element when false
} wiredHudBinding_t;

typedef struct {
	// player state
	int         stats[MAX_STATS];
	int         persistant[MAX_PERSISTANT];
	int         powerups[MAX_POWERUPS];
	int         ammo[MAX_WEAPONS];
	int         weapon, weaponstate;
	int         clientNum, ping;
	float       xyspeed;
	int         health, armor;              // convenience copies
	int         effectiveHealth;            // BG_GetEffectiveHealth (accounts for armor)
	vec4_t      healthColor;               // pre-computed color for health display
	vec4_t      armorColor;                // pre-computed color for armor display

	// timing
	int         time, frametime, realtime;

	// match state
	int         gametype;
	int         scores1, scores2;
	int         scorelimit, timelimit;
	int         maxclients;
	int         warmup, levelStartTime;
	qboolean    showScores, demoPlayback, intermission;
	qboolean    connectionInterrupted; // client commands fell past CMD_BACKUP without ack
	qboolean    hud2DHidden;           // cg_draw2D == 0: hide WiredUI HUD as well as legacy 2D
	qboolean    sceneHudHidden;        // cinematic director active + HUD off: hide the whole game HUD

	// team
	int         ourTeam;
	int         ourActiveTeam;     // resolved team (follows spectator target)
	qboolean    isOurTeamBlue;     // ourActiveTeam == TEAM_BLUE
	qboolean    isSpectator;       // ourTeam == TEAM_SPECTATOR
	qboolean    isTeamGame;        // gametype >= GT_TDM
	qboolean    isDuel;            // gametype == GT_DUEL
	int         ownTeamCount;      // alive players on our team
	int         enemyTeamCount;    // alive players on enemy team
	char        gametypeName[32];  // bg_gametypelist[gametype].name
	int         blueflag, redflag;

	// crosshair
	int         crosshairClientNum;
	int         crosshairClientTime;

	// crosshair (pre-computed by cgame — client just draws)
	struct {
		vec4_t  color;          // final RGBA (health-based or custom)
		float   size;           // final size (with item pickup pulse)
		float   x, y;           // screen offset
		int     shaderIndex;    // which crosshair (0-9), -1 = hidden
	} crosshair;

	// player extras
	int         lowAmmoWarning;         // cg.lowAmmoWarning
	qboolean    renderingThirdPerson;   // cg.renderingThirdPerson
	playerState_t predictedPlayerState; // cg.predictedPlayerState (full copy)

	// rewards
	int         rewardStack;
	int         rewardTime;
	int         rewardCount[WIRED_HUD_MAX_REWARDSTACK];
	qhandle_t   rewardShader[WIRED_HUD_MAX_REWARDSTACK];
	sfxHandle_t rewardSound[WIRED_HUD_MAX_REWARDSTACK];

	// vote
	int         voteTime, voteYes, voteNo;
	qboolean    voteModified;
	char        voteString[256];
	// team vote — mirrors the regular vote block above, but only the local
	// player's own team slot is published (cgs.teamVote*[] is indexed by
	// 0=TEAM_RED / 1=TEAM_BLUE; cgame resolves the slot so the client never
	// has to know the team mapping). teamVoteTime == 0 means "no team vote".
	int         teamVoteTime, teamVoteYes, teamVoteNo;
	qboolean    teamVoteModified;
	char        teamVoteString[256];
	char        killerName[MAX_QPATH];

	// client info
	struct {
		char        name[MAX_QPATH];
		int         team, health, armor, armorClass, weapon, location;
		int         curWeapon;
		qboolean    infoValid;
	} clients[WIRED_HUD_MAX_CLIENTS];

	// team overlay (server-sorted player list)
	int         numSortedTeamPlayers;
	int         sortedTeamPlayers[WIRED_HUD_MAX_TEAMOVERLAY];

	// scoreboard (pre-sorted by server rank order)
	int               numScores;
	wiredHudScore_t   scores[WIRED_HUD_MAX_SCORES];

	// events
	int         itemPickup, itemPickupTime;

	// media handles (registered by cgame, passed to client)
	qhandle_t   whiteShader;
	qhandle_t   deferShader;
	qhandle_t   noammoShader;
	qhandle_t   healthIcon;
	qhandle_t   heavyArmorIcon, combatArmorIcon, jacketArmorIcon;
	qhandle_t   redFlagShader[3];   // [status 0-2]
	qhandle_t   blueFlagShader[3];
	qhandle_t   weaponIcons[MAX_WEAPONS];
	qhandle_t   attackIcons[MAX_ATTACKS];
	qhandle_t   ammoIcons[MAX_WEAPONS];
	qhandle_t   itemIcons[WIRED_HUD_MAX_ITEMS];
	qhandle_t   headIcons[WIRED_HUD_MAX_CLIENTS];
	sfxHandle_t talkSound;

	// config strings (for CG_ConfigString compat)
	// Elements use CG_ConfigString(CS_SERVERINFO) etc. — we cache needed ones
	char        hostname[MAX_QPATH];

	// data bindings (named stat bundles — generic elements look these up by name)
	int               numBindings;
	wiredHudBinding_t bindings[WIRED_HUD_MAX_BINDINGS];

	// per-attack stats for local player (for SBA accuracy elements)
	struct {
		int hits, shots, kills, deaths, damage;
	} attackStats[ATT_NUM_ATTACKS];

	// lagometer (ring buffers for frame timing + snapshot health graph)
	wiredLagometer_t lagometer;
	qboolean    localServer;        // cgs.localServer — skip lagometer on local

	qboolean    valid;

	// TA compat: CG_SHOW_* / UI_SHOW_* display flags for ownerdrawFlag evaluation
	unsigned int cgShowFlags;   // CG_SHOW_* bitmask (set by cgame each frame)
	unsigned int uiShowFlags;   // UI_SHOW_* bitmask (set by cgame each frame)

	/* ── pre-computed weapon list (cgame fills, client iterates) ──── */
	int         weaponListCount;                  /* number of entries */
	int         weaponSelectTime;                 /* cg.time of last weapon switch — carousel show/fade gate */
	struct {
		int       id;          /* weapon_t value */
		qhandle_t icon;        /* weapon icon handle */
		qhandle_t ammoIcon;    /* ammo icon handle */
		int       ammo;        /* current ammo count */
		qboolean  selected;    /* qtrue if currently held */
	} weaponList[WIRED_MAX_WEAPONS];

	/* ── pre-computed active powerups (cgame fills, client iterates) ─ */
	int         activePowerupCount;
	struct {
		qhandle_t icon;        /* powerup icon handle (pre-registered) */
		int       timeLeft;    /* seconds remaining */
		qboolean  isHoldable;  /* qtrue if holdable item, not a timed powerup */
	} activePowerups[8];

	/* ── holdable inventory list (cgame fills, client iterates) ─────── */
	int         holdableListCount;
	struct {
		holdable_t id;         /* HI_* enum value */
		qhandle_t  icon;       /* item icon handle (0 = no asset) */
		qboolean   selected;   /* qtrue if STAT_HOLDABLE_ITEM points to this holdable */
		char       label[16];  /* fallback text label (used when icon == 0) */
	} holdableList[HI_NUM_HOLDABLE];

	/* ── pre-computed scoreboard weapon totals per score entry ──────── */
	struct {
		int totalKills;
		int totalShots;
		int totalHits;
	} scoreWeaponTotals[WIRED_HUD_MAX_SCORES];
} wiredHudState_t;

#endif // FEAT_WIRED_UI

#endif // QCOMMON_WIRED_UI_HUD_STATE_TYPES_H
