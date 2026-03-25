/*
===========================================================================
cl_wired_hud_compat.h — Compatibility shim for SuperHUD element migration

This header maps cgame API patterns to client equivalents so that
SuperHUD element files can be moved to the client with MINIMAL changes.
Each element file only needs: #include "cg_local.h" → #include "cl_wired_hud_compat.h"

Provides:
  - cg.* → wiredHud->* field mappings (as a struct with same field names)
  - cgs.* → wiredHud->* or client equivalents
  - trap_R_* → re.* direct renderer calls
  - trap_FS_* → FS_* direct filesystem calls
  - trap_Milliseconds → cls.realtime
  - CG_* helper function mappings
===========================================================================
*/

#ifndef CL_WIRED_HUD_COMPAT_H
#define CL_WIRED_HUD_COMPAT_H

// NOTE: include "client.h" BEFORE this header in your .c file
// This header must NOT include client.h itself to avoid double-include issues

#include "../../game/bg_public.h"
#include "cl_wired_hud.h"
#include "cl_wired_fonts.h"

// ── renderer trap → direct re.* calls ─────────────────────────────────

#define trap_R_SetColor(c)                  re.SetColor(c)
#define trap_R_DrawStretchPic(x,y,w,h,s1,t1,s2,t2,sh) \
        re.DrawStretchPic(x,y,w,h,s1,t1,s2,t2,sh)
#define trap_R_RegisterShader(n)            re.RegisterShader(n)
#define trap_R_RegisterShaderNoMip(n)       re.RegisterShaderNoMip(n)

// ── coordinate scaling ────────────────────────────────────────────────

#define CG_AdjustFrom640(x,y,w,h)  SCR_AdjustFrom640(x,y,w,h)

// ── filesystem trap → direct FS_* calls ───────────────────────────────

#define trap_FS_FOpenFile(n,f,m)    FS_FOpenFileByMode(n,f,m)
#define trap_FS_Read(b,l,f)         FS_Read(b,l,f)
#define trap_FS_FCloseFile(f)       FS_FCloseFile(f)

// ── sound trap ────────────────────────────────────────────────────────

#define trap_S_StartLocalSound(s,c) S_StartLocalSound(s,c)

// ── time ──────────────────────────────────────────────────────────────

#define trap_Milliseconds()         Sys_Milliseconds()
#define trap_RealTime(t)            Com_RealTime(t)

// ── print ─────────────────────────────────────────────────────────────

#define trap_Print(s)               Com_Printf("%s", s)
#define CG_Printf                   Com_Printf
#define CG_Error(...)               Com_Error(ERR_DROP, __VA_ARGS__)

// ── memory ────────────────────────────────────────────────────────────
// Z_Malloc and Z_Free are available in both cgame and client

#ifndef OSP_MEMORY_CHECK
#define OSP_MEMORY_CHECK(p) do { if(!(p)) Com_Error(ERR_DROP, "Out of memory"); } while(0)
#endif

// ── cg.* field access via wiredHudState_t ─────────────────────────────
// We create a struct that mirrors cg_t's layout for the fields elements use.
// This struct reads from wiredHud-> so element code sees the same field names.

typedef struct {
	// Snapshot proxy — elements access cg.snap->ps.*
	struct {
		playerState_t ps;
		int ping;
	} _snapData;
	struct {
		playerState_t ps;
		int ping;
	} *snap;

	int         clientNum;
	int         time;
	int         frametime;
	float       xyspeed;
	int         warmup;
	qboolean    showScores;
	qboolean    demoPlayback;
	qboolean    renderingThirdPerson;
	int         crosshairClientNum;
	int         crosshairClientTime;
	int         lowAmmoWarning;
	int         itemPickup;
	int         itemPickupTime;
	playerState_t predictedPlayerState;

	int         rewardStack;
	int         rewardTime;
	int         rewardCount[WIRED_HUD_MAX_REWARDSTACK];
	qhandle_t   rewardShader[WIRED_HUD_MAX_REWARDSTACK];
	sfxHandle_t rewardSound[WIRED_HUD_MAX_REWARDSTACK];
} wiredCgCompat_t;

// global compat struct — synced from wiredHud each frame
extern wiredCgCompat_t wired_cg;

// Elements use "cg." — redirect to compat struct
#define cg wired_cg

// ── client info type (replaces cgame's clientInfo_t) ──────────────────

typedef struct {
	char        name[MAX_QPATH];
	int         team;
	int         health, armor;
	int         curWeapon;
	int         location;
	int         powerups;
	qboolean    infoValid;
} wiredClientInfoCompat_t;

typedef wiredClientInfoCompat_t clientInfo_t;

// ── cgs.* field access ────────────────────────────────────────────────

typedef struct {
	int         gametype;
	int         scores1, scores2;
	int         fraglimit, capturelimit, timelimit;
	int         maxclients;
	int         levelStartTime;
	int         blueflag, redflag;
	qboolean    voteModified;
	int         voteTime, voteYes, voteNo;
	char        voteString[256];
	glconfig_t  glconfig;

	struct {
		qhandle_t   whiteShader;
		qhandle_t   deferShader;
		qhandle_t   noammoShader;
		qhandle_t   healthIcon;
		qhandle_t   heavyArmorIcon, combatArmorIcon, jacketArmorIcon;
		qhandle_t   redFlagShader[3];
		qhandle_t   blueFlagShader[3];
		sfxHandle_t talkSound;
	} media;

	// clientinfo array — typedef for elements that use clientInfo_t
	wiredClientInfoCompat_t clientinfo[WIRED_HUD_MAX_CLIENTS];
} wiredCgsCompat_t;

extern wiredCgsCompat_t wired_cgs;
#define cgs wired_cgs

// ── weapon icon access ────────────────────────────────────────────────
// Elements use cg_weapons[i].weaponIcon

typedef struct {
	qhandle_t weaponIcon;
	qhandle_t ammoIcon;
} wiredWeaponCompat_t;

extern wiredWeaponCompat_t wired_cg_weapons[MAX_WEAPONS];
#define cg_weapons wired_cg_weapons

// ── helper function compat ────────────────────────────────────────────

// CG_ConfigString — elements use it for server info
#define CG_ConfigString(idx) wired_ConfigString(idx)
const char *wired_ConfigString( int index );

// wired_GetColorForAmount — color based on health value
void wired_GetColorForAmount( int amount, vec4_t hcolor );

// CG_IsFollowing / CG_IsSpectator
qboolean wired_IsFollowing( void );
qboolean wired_IsSpectator( void );
#define CG_IsFollowing()          wired_IsFollowing()
#define CG_IsSpectator()          wired_IsSpectator()

// CG_ModernIsGameTypeFreeze
qboolean wired_IsGameTypeFreeze( void );
#define CG_ModernIsGameTypeFreeze()  wired_IsGameTypeFreeze()

// CG_RegisterItemVisuals — stub (not needed for HUD drawing)
#define CG_RegisterItemVisuals(x)

// CG_ModernDrawFrame — border frame drawing
void WiredFont_DrawFrame( float x, float y, float w, float h, const float *border, const float *borderColor, qboolean filled );
#define CG_ModernDrawFrame  WiredFont_DrawFrame

// ── memory functions ──────────────────────────────────────────────────
// Z_Malloc/Z_Free declared in qcommon.h, available in client

#ifndef OSP_MEMORY_CHECK
#define OSP_MEMORY_CHECK(p) do { if(!(p)) Com_Error(ERR_DROP, "Out of memory"); } while(0)
#endif

// ── cvar stubs from cgame ─────────────────────────────────────────────

extern vmCvar_t wired_MaxlocationWidth;
extern vmCvar_t wired_drawTimer;
extern vmCvar_t wired_drawAmmoWarning;
#define cg_MaxlocationWidth  wired_MaxlocationWidth
#define cg_drawTimer         wired_drawTimer
#define cg_drawAmmoWarning   wired_drawAmmoWarning
extern vmCvar_t wired_drawRewards;
#define cg_drawRewards       wired_drawRewards
extern vmCvar_t wired_drawCrosshair;
extern vmCvar_t wired_drawCrosshairNames;
extern vmCvar_t wired_crosshairHealth;
#define cg_drawCrosshair       wired_drawCrosshair
#define cg_drawCrosshairNames  wired_drawCrosshairNames
#define cg_crosshairHealth     wired_crosshairHealth

// ── item data stub ───────────────────────────────────────────────────

typedef struct {
	qhandle_t icon;
} wiredItemCompat_t;
extern wiredItemCompat_t wired_cg_items[256];
#define cg_items wired_cg_items

// ── team overlay data ─────────────────────────────────────────────────

extern int wired_numSortedTeamPlayers;
extern int wired_sortedTeamPlayers[WIRED_HUD_MAX_TEAMOVERLAY];
#define numSortedTeamPlayers  wired_numSortedTeamPlayers
#define sortedTeamPlayers     wired_sortedTeamPlayers

// Sync compat structs from wiredHudState — called each frame before element routines
void WiredHud_SyncCompat( void );

#endif // CL_WIRED_HUD_COMPAT_H
