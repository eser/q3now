#ifndef CG_PUBLIC_H
#define CG_PUBLIC_H
/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
//


#define	CMD_BACKUP			64	
#define	CMD_MASK			(CMD_BACKUP - 1)
// allow a lot of command backups for very fast systems
// multiple commands may be combined into a single packet, so this
// needs to be larger than PACKET_BACKUP


#define	MAX_ENTITIES_IN_SNAPSHOT	256

// snapshots are a view of the server at a given time

// Snapshots are generated at regular time intervals by the server,
// but they may not be sent if a client's rate level is exceeded, or
// they may be dropped by the network.
typedef struct {
	int				snapFlags;			// SNAPFLAG_RATE_DELAYED, etc
	int				ping;

	int				serverTime;		// server time the message is valid for (in msec)

	byte			areamask[MAX_MAP_AREA_BYTES];		// portalarea visibility bits

	playerState_t	ps;						// complete information about the current player at this time

	int				numEntities;			// all of the entities that need to be presented
	entityState_t	entities[MAX_ENTITIES_IN_SNAPSHOT];	// at the time of this snapshot

	int				numServerCommands;		// text based server commands to execute when this
	int				serverCommandSequence;	// snapshot becomes current
} snapshot_t;

enum {
  CGAME_EVENT_NONE,
  CGAME_EVENT_TEAMMENU,
  CGAME_EVENT_SCOREBOARD,
  CGAME_EVENT_EDITHUD
};


/*
==================================================================

functions imported from the main executable

==================================================================
*/

#define	CGAME_IMPORT_API_VERSION	4

typedef enum {
	CG_PRINT,
	CG_ERROR,
	CG_MILLISECONDS,
	CG_CVAR_REGISTER,
	CG_CVAR_UPDATE,
	CG_CVAR_SET,
	CG_CVAR_VARIABLESTRINGBUFFER,
	CG_ARGC,
	CG_ARGV,
	CG_ARGS,
	CG_FS_FOPENFILE,
	CG_FS_READ,
	CG_FS_WRITE,
	CG_FS_FCLOSEFILE,
	CG_SENDCONSOLECOMMAND,
	CG_ADDCOMMAND,
	CG_SENDCLIENTCOMMAND,
	CG_UPDATESCREEN,
	CG_CM_LOADMAP,
	CG_CM_NUMINLINEMODELS,
	CG_CM_INLINEMODEL,
	CG_CM_LOADMODEL,
	CG_CM_TEMPBOXMODEL,
	CG_CM_POINTCONTENTS,
	CG_CM_TRANSFORMEDPOINTCONTENTS,
	CG_CM_BOXTRACE,
	CG_CM_TRANSFORMEDBOXTRACE,
	CG_CM_MARKFRAGMENTS,
	CG_S_STARTSOUND,
	CG_S_STARTLOCALSOUND,
	CG_S_CLEARLOOPINGSOUNDS,
	CG_S_ADDLOOPINGSOUND,
	CG_S_UPDATEENTITYPOSITION,
	CG_S_RESPATIALIZE,
	CG_S_REGISTERSOUND,
	CG_S_STARTBACKGROUNDTRACK,
	CG_R_LOADWORLDMAP,
	CG_R_REGISTERMODEL,
	CG_R_REGISTERSKIN,
	CG_R_REGISTERSHADER,
	CG_R_CLEARSCENE,
	CG_R_ADDREFENTITYTOSCENE,
	CG_R_ADDPOLYTOSCENE,
	CG_R_ADDLIGHTTOSCENE,
	CG_R_RENDERSCENE,
	CG_R_SETCOLOR,
	CG_R_DRAWSTRETCHPIC,
	CG_R_MODELBOUNDS,
	CG_R_LERPTAG,
	CG_GETGLCONFIG,
	CG_GETGAMESTATE,
	CG_GETCURRENTSNAPSHOTNUMBER,
	CG_GETSNAPSHOT,
	CG_GETSERVERCOMMAND,
	CG_GETCURRENTCMDNUMBER,
	CG_GETUSERCMD,
	CG_SETUSERCMDVALUE,
	CG_R_REGISTERSHADERNOMIP,
	CG_MEMORY_REMAINING,
	CG_R_REGISTERFONT,
	CG_KEY_ISDOWN,
	CG_KEY_GETCATCHER,
	CG_KEY_SETCATCHER,
	CG_KEY_GETKEY,
 	CG_PC_ADD_GLOBAL_DEFINE,
	CG_PC_LOAD_SOURCE,
	CG_PC_FREE_SOURCE,
	CG_PC_READ_TOKEN,
	CG_PC_SOURCE_FILE_AND_LINE,
	CG_S_STOPBACKGROUNDTRACK,
	CG_REAL_TIME,
	CG_SNAPVECTOR,
	CG_REMOVECOMMAND,
	CG_R_LIGHTFORPOINT,
	CG_CIN_PLAYCINEMATIC,
	CG_CIN_STOPCINEMATIC,
	CG_CIN_RUNCINEMATIC,
	CG_CIN_DRAWCINEMATIC,
	CG_CIN_SETEXTENTS,
	CG_R_REMAP_SHADER,
	CG_S_ADDREALLOOPINGSOUND,
	CG_S_STOPLOOPINGSOUND,

	CG_CM_TEMPCAPSULEMODEL,
	CG_CM_CAPSULETRACE,
	CG_CM_TRANSFORMEDCAPSULETRACE,
	CG_R_ADDADDITIVELIGHTTOSCENE,
	CG_GET_ENTITY_TOKEN,
	CG_R_ADDPOLYSTOSCENE,
	CG_R_INPVS,
	// 1.32
	CG_FS_SEEK,

/*
	CG_LOADCAMERA,
	CG_STARTCAMERA,
	CG_GETCAMERAINFO,
*/

	CG_FLOOR = 107,
	CG_CEIL,
	CG_TESTPRINTINT,
	CG_TESTPRINTFLOAT,
	CG_ACOS,

	// engine extensions
	CG_R_ADDREFENTITYTOSCENE2,
	CG_R_FORCEFIXEDDLIGHTS,
	CG_R_ADDLINEARLIGHTTOSCENE,
	CG_R_ADDRAILTRAILPARAMS,
	CG_IS_RECORDING_DEMO,
	CG_CVAR_SETDESCRIPTION,
	// ── Wired UI: HUD state bridge (Phase 3) ────────────────────────
	CG_WIREDUI_PUSH_HUD_STATE = 200,
	// void trap_WiredUI_PushHudState( wiredHudState_t *state )
	CG_WIREDUI_PUSH_EVENT = 201,
	// void trap_WiredUI_PushEvent( int type, const char *data )

	CG_TRAP_GETVALUE = COM_TRAP_GETVALUE,

} cgameImport_t;

// ── Wired UI: HUD state bridge struct ────────────────────────────────
// cgame fills this each frame and pushes to client via CG_WIREDUI_PUSH_HUD_STATE.
// Client-side HUD element code reads from this instead of cg.*/cgs.*.

#if FEAT_WIRED_UI

#define WIRED_HUD_MAX_CLIENTS      64
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
	int         bestWeapon;         // weapon with most kills (WP_* enum, WP_NONE if none)
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
	int         fraglimit, capturelimit, timelimit;
	int         maxclients;
	int         warmup, levelStartTime;
	qboolean    showScores, demoPlayback, intermission;

	// team
	int         ourTeam;
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

	// client info
	struct {
		char        name[MAX_QPATH];
		int         team, health, armor, weapon, location;
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
	qhandle_t   ammoIcons[MAX_WEAPONS];
	qhandle_t   itemIcons[WIRED_HUD_MAX_ITEMS];
	sfxHandle_t talkSound;

	// config strings (for CG_ConfigString compat)
	// Elements use CG_ConfigString(CS_SERVERINFO) etc. — we cache needed ones
	char        hostname[MAX_QPATH];

	// data bindings (named stat bundles — generic elements look these up by name)
	int               numBindings;
	wiredHudBinding_t bindings[WIRED_HUD_MAX_BINDINGS];

	// per-weapon stats for local player (for SBA accuracy elements)
	struct {
		int hits, shots, kills, deaths, damage;
	} weaponStats[MAX_WEAPONS];

	// lagometer (ring buffers for frame timing + snapshot health graph)
	wiredLagometer_t lagometer;
	qboolean    localServer;        // cgs.localServer — skip lagometer on local

	qboolean    wiredUIActive;  // cg_wiredUI.integer — client uses this to enable/disable rendering
	qboolean    valid;
} wiredHudState_t;

void trap_WiredUI_PushHudState( wiredHudState_t *state );

// HUD event types (for CG_WIREDUI_PUSH_EVENT)
#define WIRED_EVENT_CHAT       0
#define WIRED_EVENT_TEAMCHAT   1
#define WIRED_EVENT_FRAG       2
#define WIRED_EVENT_RANK       3
#define WIRED_EVENT_AWARD      4
#define WIRED_EVENT_FRAG_RANK  5   // "frag_text|rank_text" combined atomic pair
#define WIRED_EVENT_CENTERPRINT 6  // center print text (routed through message queue)
#define WIRED_EVENT_OBITUARY   7  // "attacker|target|mod|unfrozen" kill event
#define WIRED_EVENT_TEMPACC    8  // "weapon|accuracy" recent weapon accuracy

void trap_WiredUI_PushEvent( int type, const char *data );

#endif // FEAT_WIRED_UI


/*
==================================================================

functions exported to the main executable

==================================================================
*/

typedef enum {
	CG_INIT,
//	void CG_Init( int serverMessageNum, int serverCommandSequence, int clientNum )
	// called when the level loads or when the renderer is restarted
	// all media should be registered at this time
	// cgame will display loading status by calling SCR_Update, which
	// will call CG_DrawInformation during the loading process
	// reliableCommandSequence will be 0 on fresh loads, but higher for
	// demos, tourney restarts, or vid_restarts

	CG_SHUTDOWN,
//	void (*CG_Shutdown)( void );
	// opportunity to flush and close any open files

	CG_CONSOLE_COMMAND,
//	qboolean (*CG_ConsoleCommand)( void );
	// a console command has been issued locally that is not recognized by the
	// main game system.
	// use Cmd_Argc() / Cmd_Argv() to read the command, return qfalse if the
	// command is not known to the game

	CG_DRAW_ACTIVE_FRAME,
//	void (*CG_DrawActiveFrame)( int serverTime, stereoFrame_t stereoView, qboolean demoPlayback );
	// Generates and draws a game scene and status information at the given time.
	// If demoPlayback is set, local movement prediction will not be enabled

	CG_CROSSHAIR_PLAYER,
//	int (*CG_CrosshairPlayer)( void );

	CG_LAST_ATTACKER,
//	int (*CG_LastAttacker)( void );

	CG_KEY_EVENT, 
//	void	(*CG_KeyEvent)( int key, qboolean down );

	CG_MOUSE_EVENT,
//	void	(*CG_MouseEvent)( int dx, int dy );

	CG_EVENT_HANDLING,
//	void (*CG_EventHandling)(int type);

	CG_EXPORT_LAST,
} cgameExport_t;

//----------------------------------------------
#endif // CG_PUBLIC_H
