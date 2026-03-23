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

#include "g_local.h"
#include "bg_promode.h" // CPM

// PRODUCT_DATE is injected by cmake for native builds; QVM builds fall back to unknown
#ifndef PRODUCT_DATE
#define PRODUCT_DATE "unknown"
#endif

level_locals_t	level;

typedef struct {
	vmCvar_t	*vmCvar;
	char		*cvarName;
	char		*defaultString;
	int			cvarFlags;
	int			modificationCount;  // for tracking changes
	qboolean	trackChange;	    // track this variable, and announce if changed
  qboolean teamShader;        // track and if changed, update shader state
} cvarTable_t;

gentity_t		g_entities[MAX_GENTITIES];
gclient_t		g_clients[MAX_CLIENTS];

vmCvar_t	g_gametype;
vmCvar_t	g_dmflags;
vmCvar_t	g_kothflags;
vmCvar_t	g_fraglimit;
vmCvar_t	g_timelimit;
vmCvar_t	g_capturelimit;
vmCvar_t	g_friendlyFire;
vmCvar_t	g_password;
vmCvar_t	g_needpass;
vmCvar_t	g_maxclients;
vmCvar_t	g_maxGameClients;
vmCvar_t	g_dedicated;
vmCvar_t	g_gravity;
vmCvar_t	g_cheats;
vmCvar_t	g_forceRespawn;
vmCvar_t	g_inactivity;
vmCvar_t	g_debugMove;
vmCvar_t	g_debugDamage;
vmCvar_t	g_debugAlloc;
vmCvar_t	g_motd;
vmCvar_t	g_synchronousClients;
vmCvar_t	g_warmup;
vmCvar_t	g_restarted;
vmCvar_t	g_logfile;
vmCvar_t	g_logfileSync;
vmCvar_t	g_blood;
vmCvar_t	g_allowVote;
vmCvar_t	g_teamAutoJoin;
vmCvar_t	g_teamForceBalance;
vmCvar_t	g_banIPs;
vmCvar_t	g_filterBan;
vmCvar_t	g_smoothClients;
vmCvar_t	pmove_fixed;
vmCvar_t	pmove_msec;
vmCvar_t	pmove_overbounce;
vmCvar_t	g_rankings;
vmCvar_t	g_listEntity;
vmCvar_t	g_localTeamPref;
#ifdef MISSIONPACK
vmCvar_t	g_obeliskRespawnDelay;
vmCvar_t	g_redteam;
vmCvar_t	g_blueteam;
vmCvar_t	g_enableDust;
vmCvar_t	g_enableBreath;
#endif

#define Q3NOW_VERSION "1.0"

vmCvar_t	g_grapple;
vmCvar_t	g_spawnWeapons;
vmCvar_t	g_instagib;
vmCvar_t	g_excessive;
vmCvar_t	g_q3now;
vmCvar_t	g_singlePlayer;
#if FEAT_UNLAGGED
vmCvar_t	g_unlagged;
#endif
#if FEAT_SPAWN_PROTECTION
vmCvar_t	g_spawnProtect;
#endif
#if FEAT_READY_UP
vmCvar_t	g_startWhenReady;
#endif
#if FEAT_FREEZETAG
vmCvar_t	g_freeze;
vmCvar_t	g_thawTime;
vmCvar_t	g_thawRadius;
#endif
#if FEAT_JSON_STATS
vmCvar_t	g_exportStats;
#endif
#if FEAT_MAP_ROTATION
vmCvar_t	g_maprotation;
vmCvar_t	g_maprotationMode;
#endif
#if FEAT_FAST_WEAPON_SWITCH
vmCvar_t	g_fastWeaponSwitch;
#endif
#if FEAT_OVERTIME
vmCvar_t	g_overtime;
#endif
#if FEAT_AUTO_DEMO
vmCvar_t	g_autoDemo;
#endif
#if FEAT_CTF_SCORING
vmCvar_t	g_ctfScoring;
#endif
#if FEAT_TOURNAMENT_PAUSE
vmCvar_t	g_allowTimeout;
#endif
#if FEAT_PROJECTILE_BOUNCE
vmCvar_t	g_projectileBounce;
#endif
#if FEAT_ATMOSPHERIC
vmCvar_t	g_weather;
#endif
#if FEAT_TEAM_AUTOBALANCE
vmCvar_t	g_teamBalance;
vmCvar_t	g_teamBalanceDelay;
#endif
#if FEAT_ELIMINATION
vmCvar_t	g_elimination;
vmCvar_t	g_eliminationRoundTime;
#endif
// eser - admin mode
vmCvar_t	g_adminPassword;
// eser - admin mode
#if FEAT_RANKED_QUEUE
vmCvar_t	g_ranked;
vmCvar_t	g_rankedMinPlayers;
#endif
#if FEAT_CLAN_ARENA
vmCvar_t	g_clanArena;
#endif
#if FEAT_RTF
vmCvar_t	g_rtf;
#endif
#if FEAT_TEAM_LEADERSHIP
vmCvar_t	g_ptl;
#endif
#if FEAT_CRON_JOBS
typedef struct {
	int		intervalSec;
	char	command[256];
} cronJob_t;
#define MAX_CRON_JOBS 32
static cronJob_t	cronJobs[MAX_CRON_JOBS];
static int			numCronJobs = 0;
static int			cronLastCheck = 0;
#endif
// eser - camp-detection
vmCvar_t	g_campDetectionTime;
vmCvar_t	g_campDetectionRadius;
// eser - camp-detection
#if FEAT_DROP_ITEMS
vmCvar_t	g_dropEnable;
#endif


static cvarTable_t		gameCvarTable[] = {
	// don't override the cheat state set by the system
	{ &g_cheats, "sv_cheats", "", 0, 0, qfalse },

	// noset vars
	{ NULL, "gamename", GAMEVERSION , CVAR_SERVERINFO | CVAR_ROM, 0, qfalse  },
	{ NULL, "gamedate", PRODUCT_DATE , CVAR_ROM, 0, qfalse  },
	{ &g_restarted, "g_restarted", "0", CVAR_ROM, 0, qfalse  },

	// latched vars
	{ &g_gametype, "g_gametype", "0", CVAR_SERVERINFO | CVAR_USERINFO | CVAR_LATCH, 0, qfalse  },

	{ &g_maxclients, "sv_maxclients", "8", CVAR_SERVERINFO | CVAR_LATCH | CVAR_ARCHIVE, 0, qfalse  },
	{ &g_maxGameClients, "g_maxGameClients", "0", CVAR_SERVERINFO | CVAR_LATCH | CVAR_ARCHIVE, 0, qfalse  },

	// change anytime vars
	{ &g_dmflags, "dmflags", "0", CVAR_SERVERINFO | CVAR_ARCHIVE, 0, qtrue },
	{ &g_kothflags, "kothflags", "0", CVAR_SERVERINFO | CVAR_ARCHIVE, 0, qtrue },
	{ &g_fraglimit, "fraglimit", "20", CVAR_SERVERINFO | CVAR_ARCHIVE | CVAR_NORESTART, 0, qtrue },
	{ &g_timelimit, "timelimit", "0", CVAR_SERVERINFO | CVAR_ARCHIVE | CVAR_NORESTART, 0, qtrue },
	{ &g_capturelimit, "capturelimit", "8", CVAR_SERVERINFO | CVAR_ARCHIVE | CVAR_NORESTART, 0, qtrue },

	{ &g_synchronousClients, "g_synchronousClients", "0", CVAR_SYSTEMINFO, 0, qfalse  },

	{ &g_friendlyFire, "g_friendlyFire", "0", CVAR_ARCHIVE, 0, qtrue  },

	{ &g_teamAutoJoin, "g_teamAutoJoin", "0", CVAR_ARCHIVE  },
	{ &g_teamForceBalance, "g_teamForceBalance", "0", CVAR_ARCHIVE  },

	{ &g_warmup, "g_warmup", "0", CVAR_ARCHIVE, 0, qtrue  },
	{ &g_logfile, "g_log", "games.log", CVAR_ARCHIVE, 0, qfalse  },
	{ &g_logfileSync, "g_logsync", "0", CVAR_ARCHIVE, 0, qfalse  },

	{ &g_password, "g_password", "", CVAR_USERINFO, 0, qfalse  },

	{ &g_banIPs, "g_banIPs", "", CVAR_ARCHIVE, 0, qfalse  },
	{ &g_filterBan, "g_filterBan", "1", CVAR_ARCHIVE, 0, qfalse  },

	{ &g_needpass, "g_needpass", "0", CVAR_SERVERINFO | CVAR_ROM, 0, qfalse },

	{ &g_dedicated, "dedicated", "0", 0, 0, qfalse  },

	{ &g_gravity, "g_gravity", "800", 0, 0, qtrue  },
	{ &g_forceRespawn, "g_forceRespawn", "0", 0, 0, qtrue },
	{ &g_inactivity, "g_inactivity", "0", 0, 0, qtrue },
	{ &g_debugMove, "g_debugMove", "0", 0, 0, qfalse },
	{ &g_debugDamage, "g_debugDamage", "0", 0, 0, qfalse },
	{ &g_debugAlloc, "g_debugAlloc", "0", 0, 0, qfalse },
	{ &g_motd, "g_motd", "", 0, 0, qfalse },
	{ &g_blood, "com_blood", "1", 0, 0, qfalse },

	{ &g_allowVote, "g_allowVote", "1", CVAR_ARCHIVE, 0, qfalse },
	{ &g_listEntity, "g_listEntity", "0", 0, 0, qfalse },

#ifdef MISSIONPACK
	{ &g_obeliskRespawnDelay, "g_obeliskRespawnDelay", "10", CVAR_SERVERINFO, 0, qfalse },

	{ &g_redteam, "g_redteam", "Stroggs", CVAR_ARCHIVE | CVAR_SERVERINFO | CVAR_USERINFO , 0, qtrue, qtrue },
	{ &g_blueteam, "g_blueteam", "Pagans", CVAR_ARCHIVE | CVAR_SERVERINFO | CVAR_USERINFO , 0, qtrue, qtrue  },

	{ &g_enableDust, "g_enableDust", "0", CVAR_SERVERINFO, 0, qtrue, qfalse },
	{ &g_enableBreath, "g_enableBreath", "0", CVAR_SERVERINFO, 0, qtrue, qfalse },
#endif
	{ &g_smoothClients, "g_smoothClients", "1", 0, 0, qfalse},
	{ &pmove_fixed, "pmove_fixed", "0", CVAR_SYSTEMINFO, 0, qfalse},
	{ &pmove_msec, "pmove_msec", "8", CVAR_SYSTEMINFO, 0, qfalse},
	{ &pmove_overbounce, "pmove_overbounce", "1", CVAR_SYSTEMINFO, 0, qfalse},

	{ &g_rankings, "g_rankings", "0", 0, 0, qfalse},
	{ &g_localTeamPref, "g_localTeamPref", "", 0, 0, qfalse },

    { &g_grapple, "g_grapple", "0", CVAR_SERVERINFO | CVAR_LATCH, 0, qtrue },
    { &g_spawnWeapons, "g_spawnWeapons", "0", CVAR_SERVERINFO | CVAR_LATCH, 0, qtrue },
    { &g_instagib, "g_instagib", "0", CVAR_SERVERINFO | CVAR_LATCH, 0, qtrue },
	{ &g_excessive, "g_excessive", "0", CVAR_SERVERINFO | CVAR_LATCH, 0, qtrue },
	{ &g_q3now, "g_q3now", Q3NOW_VERSION, CVAR_SERVERINFO | CVAR_ROM, 0, qfalse },

    { &g_singlePlayer, "g_singlePlayer", "0", CVAR_SERVERINFO | CVAR_LATCH, 0, qfalse, qfalse },
#if FEAT_UNLAGGED
    { &g_unlagged, "g_unlagged", "1", CVAR_SERVERINFO | CVAR_ARCHIVE, 0, qfalse },
#endif
#if FEAT_SPAWN_PROTECTION
    { &g_spawnProtect,            "g_spawnProtect",            "2", CVAR_SERVERINFO | CVAR_ARCHIVE, 0, qfalse },
#endif
#if FEAT_READY_UP
    { &g_startWhenReady,          "g_startWhenReady",          "0", CVAR_SERVERINFO | CVAR_ARCHIVE, 0, qfalse },
#endif
#if FEAT_FREEZETAG
    { &g_freeze,                  "g_freeze",                  "0", CVAR_SERVERINFO | CVAR_ARCHIVE | CVAR_LATCH, 0, qfalse },
    { &g_thawTime,                "g_thawTime",                "3000", CVAR_ARCHIVE, 0, qfalse },
    { &g_thawRadius,              "g_thawRadius",              "128", CVAR_ARCHIVE, 0, qfalse },
#endif
#if FEAT_JSON_STATS
    { &g_exportStats,             "g_exportStats",             "0", CVAR_ARCHIVE, 0, qfalse },
#endif
#if FEAT_MAP_ROTATION
    { &g_maprotation,             "g_maprotation",             "", CVAR_ARCHIVE, 0, qfalse },
    { &g_maprotationMode,         "g_maprotationMode",         "0", CVAR_ARCHIVE, 0, qfalse },
#endif
#if FEAT_FAST_WEAPON_SWITCH
    { &g_fastWeaponSwitch,        "g_fastWeaponSwitch",        "1", CVAR_SERVERINFO | CVAR_ARCHIVE, 0, qfalse },
#endif
#if FEAT_OVERTIME
    { &g_overtime,                "g_overtime",                "0", CVAR_SERVERINFO | CVAR_ARCHIVE, 0, qfalse },
#endif
#if FEAT_AUTO_DEMO
    { &g_autoDemo,                "g_autoDemo",                "0", CVAR_SERVERINFO | CVAR_ARCHIVE, 0, qfalse },
#endif
#if FEAT_CTF_SCORING
    { &g_ctfScoring,              "g_ctfScoring",              "0", CVAR_SERVERINFO | CVAR_ARCHIVE, 0, qfalse },
#endif
#if FEAT_TOURNAMENT_PAUSE
    { &g_allowTimeout,            "g_allowTimeout",            "1", CVAR_ARCHIVE, 0, qfalse },
#endif
#if FEAT_PROJECTILE_BOUNCE
    { &g_projectileBounce,        "g_projectileBounce",        "0", CVAR_SERVERINFO | CVAR_ARCHIVE, 0, qfalse },
#endif
#if FEAT_ATMOSPHERIC
    { &g_weather,                 "g_weather",                 "", CVAR_SERVERINFO | CVAR_ARCHIVE, 0, qtrue },
#endif
#if FEAT_TEAM_AUTOBALANCE
    { &g_teamBalance,             "g_teamBalance",             "0", CVAR_SERVERINFO | CVAR_ARCHIVE, 0, qfalse },
    { &g_teamBalanceDelay,        "g_teamBalanceDelay",        "5", CVAR_ARCHIVE, 0, qfalse },
#endif
#if FEAT_ELIMINATION
    { &g_elimination,             "g_elimination",             "0", CVAR_SERVERINFO | CVAR_ARCHIVE | CVAR_LATCH, 0, qfalse },
    { &g_eliminationRoundTime,    "g_eliminationRoundTime",    "120", CVAR_ARCHIVE, 0, qfalse },
#endif
// eser - admin mode
    { &g_adminPassword,           "g_adminPassword",           "", CVAR_ARCHIVE, 0, qfalse },
// eser - admin mode
#if FEAT_RANKED_QUEUE
    { &g_ranked,                  "g_ranked",                  "0", CVAR_SERVERINFO | CVAR_ARCHIVE, 0, qfalse },
    { &g_rankedMinPlayers,        "g_rankedMinPlayers",        "2", CVAR_ARCHIVE, 0, qfalse },
#endif
#if FEAT_CLAN_ARENA
    { &g_clanArena,               "g_clanArena",               "0", CVAR_SERVERINFO | CVAR_ARCHIVE | CVAR_LATCH, 0, qfalse },
#endif
#if FEAT_RTF
    { &g_rtf,                     "g_rtf",                     "0", CVAR_SERVERINFO | CVAR_ARCHIVE | CVAR_LATCH, 0, qfalse },
#endif
#if FEAT_TEAM_LEADERSHIP
    { &g_ptl,                     "g_ptl",                     "0", CVAR_SERVERINFO | CVAR_ARCHIVE | CVAR_LATCH, 0, qfalse },
#endif
	// eser - camp-detection
    { &g_campDetectionTime,            "g_campDetectionTime",            "30",  CVAR_SERVERINFO | CVAR_ARCHIVE, 0, qfalse },
    { &g_campDetectionRadius,          "g_campDetectionRadius",          "300", CVAR_ARCHIVE, 0, qfalse },
	// eser - camp-detection
#if FEAT_DROP_ITEMS
    { &g_dropEnable,              "g_dropEnable",              "0", CVAR_SERVERINFO | CVAR_ARCHIVE, 0, qfalse },
#endif
};

static int gameCvarTableSize = ARRAY_LEN( gameCvarTable );


void G_InitGame( int levelTime, int randomSeed, int restart );
void G_RunFrame( int levelTime );
void G_ShutdownGame( int restart );
void CheckExitRules( void );


/*
================
vmMain

This is the only way control passes into the module.
This must be the very first function compiled into the .q3vm file
================
*/
Q_EXPORT intptr_t vmMain( int command, int arg0, int arg1, int arg2, int arg3, int arg4, int arg5, int arg6, int arg7, int arg8, int arg9, int arg10, int arg11  ) {
	switch ( command ) {
	case GAME_INIT:
		G_InitGame( arg0, arg1, arg2 );
		return 0;
	case GAME_SHUTDOWN:
		G_ShutdownGame( arg0 );
		return 0;
	case GAME_CLIENT_CONNECT:
		return (intptr_t)ClientConnect( arg0, arg1, arg2 );
	case GAME_CLIENT_THINK:
		ClientThink( arg0 );
		return 0;
	case GAME_CLIENT_USERINFO_CHANGED:
		ClientUserinfoChanged( arg0 );
		return 0;
	case GAME_CLIENT_DISCONNECT:
		ClientDisconnect( arg0 );
		return 0;
	case GAME_CLIENT_BEGIN:
		ClientBegin( arg0 );
		return 0;
	case GAME_CLIENT_COMMAND:
		ClientCommand( arg0 );
		return 0;
	case GAME_RUN_FRAME:
		G_RunFrame( arg0 );
		return 0;
	case GAME_CONSOLE_COMMAND:
		return ConsoleCommand();
	case BOTAI_START_FRAME:
		return BotAIStartFrame( arg0 );
	}

	return -1;
}


void QDECL G_Printf( const char *fmt, ... ) {
	va_list		argptr;
	char		text[1024];

	va_start (argptr, fmt);
	Q_vsnprintf (text, sizeof(text), fmt, argptr);
	va_end (argptr);

	trap_Print( text );
}

void QDECL G_Error( const char *fmt, ... ) {
	va_list		argptr;
	char		text[1024];

	va_start (argptr, fmt);
	Q_vsnprintf (text, sizeof(text), fmt, argptr);
	va_end (argptr);

	trap_Error( text );
}

/*
================
G_FindTeams

Chain together all entities with a matching team field.
Entity teams are used for item groups and multi-entity mover groups.

All but the first will have the FL_TEAMMEMBER flag set and teammaster field set
All but the last will have the teamchain field set to the next one
================
*/
void G_FindTeams( void ) {
	gentity_t	*e, *e2;
	int		i, j;
	int		c, c2;

	c = 0;
	c2 = 0;
	for ( i=MAX_CLIENTS, e=g_entities+i ; i < level.num_entities ; i++,e++ ) {
		if (!e->inuse)
			continue;
		if (!e->team)
			continue;
		if (e->flags & FL_TEAMMEMBER)
			continue;
		e->teammaster = e;
		c++;
		c2++;
		for (j=i+1, e2=e+1 ; j < level.num_entities ; j++,e2++)
		{
			if (!e2->inuse)
				continue;
			if (!e2->team)
				continue;
			if (e2->flags & FL_TEAMMEMBER)
				continue;
			if (!strcmp(e->team, e2->team))
			{
				c2++;
				e2->teamchain = e->teamchain;
				e->teamchain = e2;
				e2->teammaster = e;
				e2->flags |= FL_TEAMMEMBER;

				// make sure that targets only point at the master
				if ( e2->targetname ) {
					e->targetname = e2->targetname;
					e2->targetname = NULL;
				}
			}
		}
	}

	G_Printf ("%i teams with %i entities\n", c, c2);
}

void G_RemapTeamShaders( void ) {
#ifdef MISSIONPACK
	char string[1024];
	float f = level.time * 0.001;
	Com_sprintf( string, sizeof(string), "team_icon/%s_red", g_redteam.string );
	AddRemap("textures/ctf2/redteam01", string, f); 
	AddRemap("textures/ctf2/redteam02", string, f); 
	Com_sprintf( string, sizeof(string), "team_icon/%s_blue", g_blueteam.string );
	AddRemap("textures/ctf2/blueteam01", string, f); 
	AddRemap("textures/ctf2/blueteam02", string, f); 
	trap_SetConfigstring(CS_SHADERSTATE, BuildShaderStateConfig());
#endif
}


/*
=================
G_RegisterCvars
=================
*/
void G_RegisterCvars( void ) {
	int			i;
	cvarTable_t	*cv;
	qboolean remapped = qfalse;

	for ( i = 0, cv = gameCvarTable ; i < gameCvarTableSize ; i++, cv++ ) {
		trap_Cvar_Register( cv->vmCvar, cv->cvarName,
			cv->defaultString, cv->cvarFlags );
		if ( cv->vmCvar )
			cv->modificationCount = cv->vmCvar->modificationCount;

		if (cv->teamShader) {
			remapped = qtrue;
		}
	}

	if (remapped) {
		G_RemapTeamShaders();
	}

	// check some things
	if ( g_gametype.integer < 0 || g_gametype.integer >= GT_MAX_GAME_TYPE ) {
		G_Printf( "g_gametype %i is out of range, defaulting to 0\n", g_gametype.integer );
		trap_Cvar_Set( "g_gametype", "0" );
		trap_Cvar_Update( &g_gametype );
	}

	level.warmupModificationCount = g_warmup.modificationCount;
}

/*
=================
G_UpdateCvars
=================
*/
void G_UpdateCvars( void ) {
	int			i;
	cvarTable_t	*cv;
	qboolean remapped = qfalse;

	for ( i = 0, cv = gameCvarTable ; i < gameCvarTableSize ; i++, cv++ ) {
		if ( cv->vmCvar ) {
			trap_Cvar_Update( cv->vmCvar );

			if ( cv->modificationCount != cv->vmCvar->modificationCount ) {
				cv->modificationCount = cv->vmCvar->modificationCount;

				if ( cv->trackChange ) {
					trap_SendServerCommand( -1, va("print \"Server: %s changed to %s\n\"", 
						cv->cvarName, cv->vmCvar->string ) );
				}

				if (cv->teamShader) {
					remapped = qtrue;
				}
			}
		}
	}

	if (remapped) {
		G_RemapTeamShaders();
	}
}

/*
============
G_InitGame

============
*/
void G_InitGame( int levelTime, int randomSeed, int restart ) {
	int					i;

	G_Printf ("------- Game Initialization -------\n");
	G_Printf ("gamename: %s\n", GAMEVERSION);
	G_Printf ("gamedate: %s\n", PRODUCT_DATE);

	srand( randomSeed );

	G_RegisterCvars();

	G_ProcessIPBans();

	G_InitMemory();

    // CPM: Initialize
    // Update all settings
    CPM_UpdateSettings(g_gametype.integer);

    // q3now: print mod version and active feature flags
    {
        char features[256];
        features[0] = '\0';
        if ( g_instagib.integer )    Q_strcat( features, sizeof(features), " instagib" );
        if ( g_excessive.integer )   Q_strcat( features, sizeof(features), " excessive" );
        if ( g_grapple.integer )     Q_strcat( features, sizeof(features), " grapple" );
        G_Printf( "q3now | active:%s\n", features[0] ? features : " (none)" );
    }

	// set some level globals
	memset( &level, 0, sizeof( level ) );
	level.time = levelTime;
	level.startTime = levelTime;

	level.snd_fry = G_SoundIndex("sound/player/fry.wav");	// FIXME standing in lava / slime

	if ( g_logfile.string[0] ) {
		if ( g_logfileSync.integer ) {
			trap_FS_FOpenFile( g_logfile.string, &level.logFile, FS_APPEND_SYNC );
		} else {
			trap_FS_FOpenFile( g_logfile.string, &level.logFile, FS_APPEND );
		}
		if ( !level.logFile ) {
			G_Printf( "WARNING: Couldn't open logfile: %s\n", g_logfile.string );
		} else {
			char	serverinfo[MAX_INFO_STRING];

			trap_GetServerinfo( serverinfo, sizeof( serverinfo ) );

			G_LogPrintf("------------------------------------------------------------\n" );
			G_LogPrintf("InitGame: %s\n", serverinfo );
		}
	} else {
		G_Printf( "Not logging to disk.\n" );
	}

	G_InitWorldSession();

#if FEAT_CRON_JOBS
	// cron jobs (11P): parse crontab.txt at init
	{
		fileHandle_t	f;
		int				len;
		char			buf[4096];

		numCronJobs = 0;
		cronLastCheck = 0;
		len = trap_FS_FOpenFile( "crontab.txt", &f, FS_READ );
		if ( f && len > 0 && len < (int)sizeof(buf) ) {
			char *p, *line;
			trap_FS_Read( buf, len, f );
			buf[len] = '\0';
			p = buf;
			while ( numCronJobs < MAX_CRON_JOBS ) {
				// find next line
				line = p;
				while ( *p && *p != '\n' && *p != '\r' ) p++;
				if ( *p ) { *p = '\0'; p++; }
				while ( *p == '\n' || *p == '\r' ) p++;
				// skip empty / comment lines
				if ( !line[0] || line[0] == '#' ) {
					if ( !*p ) break;
					continue;
				}
				// parse: <seconds> <command>
				{
					int sec = atoi( line );
					char *cmd = line;
					while ( *cmd && *cmd != ' ' && *cmd != '\t' ) cmd++;
					while ( *cmd == ' ' || *cmd == '\t' ) cmd++;
					if ( sec > 0 && cmd[0] ) {
						cronJobs[numCronJobs].intervalSec = sec;
						Q_strncpyz( cronJobs[numCronJobs].command, cmd, sizeof(cronJobs[0].command) );
						numCronJobs++;
					}
				}
				if ( !*p ) break;
			}
			G_Printf( "Cron: loaded %d jobs from crontab.txt\n", numCronJobs );
		}
		if ( f ) {
			trap_FS_FCloseFile( f );
		}
	}
#endif

	// initialize all entities for this game
	memset( g_entities, 0, MAX_GENTITIES * sizeof(g_entities[0]) );
	level.gentities = g_entities;

	// initialize all clients for this game
	level.maxclients = g_maxclients.integer;
	memset( g_clients, 0, MAX_CLIENTS * sizeof(g_clients[0]) );
	level.clients = g_clients;

	// set client fields on player ents
	for ( i=0 ; i<level.maxclients ; i++ ) {
		g_entities[i].client = level.clients + i;
	}

	// always leave room for the max number of clients,
	// even if they aren't all used, so numbers inside that
	// range are NEVER anything but clients
	level.num_entities = MAX_CLIENTS;

	for ( i=0 ; i<MAX_CLIENTS ; i++ ) {
		g_entities[i].classname = "clientslot";
	}

	// let the server system know where the entites are
	trap_LocateGameData( level.gentities, level.num_entities, sizeof( gentity_t ), 
		&level.clients[0].ps, sizeof( level.clients[0] ) );

	// reserve some spots for dead player bodies
	InitBodyQue();

	ClearRegisteredItems();

	// parse the key/value pairs and spawn gentities
	G_SpawnEntitiesFromString();

	// general initialization
	G_FindTeams();

    if (g_gametype.integer == GT_LASTMANSTANDING) {
        level.initialFraglimit = g_fraglimit.integer;

        if (level.initialFraglimit < 1) {
            level.initialFraglimit = 1;
        }
    }

#if FEAT_CLAN_ARENA
	// clan arena (11K): remove all pickups (weapons, ammo, armor, health)
	if ( g_clanArena.integer && g_elimination.integer ) {
		gentity_t *e;
		int j;
		for ( j = MAX_CLIENTS, e = &g_entities[j]; j < level.num_entities; j++, e++ ) {
			if ( !e->inuse || !e->item ) continue;
			if ( e->item->giType == IT_WEAPON || e->item->giType == IT_AMMO ||
				 e->item->giType == IT_ARMOR  || e->item->giType == IT_HEALTH ||
				 e->item->giType == IT_POWERUP ) {
				G_FreeEntity( e );
			}
		}
		G_Printf( "Clan Arena: items removed\n" );
	}
#endif

	// make sure we have flags for CTF, etc
	if( g_gametype.integer >= GT_TEAM ) {
		G_CheckTeamItems();
	}

	SaveRegisteredItems();

	G_Printf ("-----------------------------------\n");

	if ( trap_Cvar_VariableIntegerValue( "bot_enable" ) ) {
		BotAISetup( restart );
		BotAILoadMap( restart );
		G_InitBots( restart );
	}

	G_RemapTeamShaders();

	trap_SetConfigstring( CS_INTERMISSION, "" );

#if FEAT_AUTO_DEMO
	// auto-demo (10K): start recording when match begins (warmup ended)
	if ( g_autoDemo.integer && g_restarted.integer ) {
		trap_SendServerCommand( -1, "record\n" );
	}
#endif

#if FEAT_MAP_ROTATION
	// map rotation: set nextmap cvar so vstr nextmap / callvote nextmap work
	{
		const char *next = G_PeekRotationMap();
		if ( next ) {
			trap_Cvar_Set( "nextmap", va( "map %s", next ) );
			G_Printf( "Map rotation: next map is %s\n", next );
		}
	}
#endif
}



/*
=================
G_ShutdownGame
=================
*/
void G_ShutdownGame( int restart ) {
	G_Printf ("==== ShutdownGame ====\n");

	if ( level.logFile ) {
		G_LogPrintf("ShutdownGame:\n" );
		G_LogPrintf("------------------------------------------------------------\n" );
		trap_FS_FCloseFile( level.logFile );
		level.logFile = 0;
	}

	// write all the client session data so we can get it back
	G_WriteSessionData();

	if ( trap_Cvar_VariableIntegerValue( "bot_enable" ) ) {
		BotAIShutdown( restart );
	}
}



//===================================================================

void QDECL Com_Error ( errorParm_t logLevel, const char *error, ... ) {
	va_list		argptr;
	char		text[1024];

	va_start (argptr, error);
	Q_vsnprintf (text, sizeof(text), error, argptr);
	va_end (argptr);

	trap_Error( text );
}

void QDECL Com_Printf( const char *msg, ... ) {
	va_list		argptr;
	char		text[1024];

	va_start (argptr, msg);
	Q_vsnprintf (text, sizeof(text), msg, argptr);
	va_end (argptr);

	trap_Print( text );
}

/*
========================================================================

PLAYER COUNTING / SCORE SORTING

========================================================================
*/

/*
=============
AddTournamentPlayer

If there are less than two tournament players, put a
spectator in the game and restart
=============
*/
void AddTournamentPlayer( void ) {
	int			i;
	gclient_t	*client;
	gclient_t	*nextInLine;

	if ( level.numPlayingClients >= 2 ) {
		return;
	}

	// never change during intermission
	if ( level.intermissiontime ) {
		return;
	}

	nextInLine = NULL;

	for ( i = 0 ; i < level.maxclients ; i++ ) {
		client = &level.clients[i];
		if ( client->pers.connected != CON_CONNECTED ) {
			continue;
		}
		if ( client->sess.sessionTeam != TEAM_SPECTATOR ) {
			continue;
		}
		// never select the dedicated follow or scoreboard clients
		if ( client->sess.spectatorState == SPECTATOR_SCOREBOARD || 
			client->sess.spectatorClient < 0  ) {
			continue;
		}

		if(!nextInLine || client->sess.spectatorNum > nextInLine->sess.spectatorNum)
			nextInLine = client;
	}

	if ( !nextInLine ) {
		return;
	}

	level.warmupTime = -1;

	// set them to free-for-all team
	SetTeam( &g_entities[ nextInLine - level.clients ], "f" );
}

/*
=======================
AddTournamentQueue

Add client to end of tournament queue
=======================
*/

void AddTournamentQueue(gclient_t *client)
{
	int index;
	gclient_t *curclient;
	
	for(index = 0; index < level.maxclients; index++)
	{
		curclient = &level.clients[index];
		
		if(curclient->pers.connected != CON_DISCONNECTED)
		{
			if(curclient == client)
				curclient->sess.spectatorNum = 0;
			else if(curclient->sess.sessionTeam == TEAM_SPECTATOR)
				curclient->sess.spectatorNum++;
		}
	}
}

/*
=======================
RemoveTournamentLoser

Make the loser a spectator at the back of the line
=======================
*/
void RemoveTournamentLoser( void ) {
	int			clientNum;

	if ( level.numPlayingClients != 2 ) {
		return;
	}

	clientNum = level.sortedClients[1];

	if ( level.clients[ clientNum ].pers.connected != CON_CONNECTED ) {
		return;
	}

	// make them a spectator
	SetTeam( &g_entities[ clientNum ], "s" );
}

/*
=======================
RemoveTournamentWinner
=======================
*/
void RemoveTournamentWinner( void ) {
	int			clientNum;

	if ( level.numPlayingClients != 2 ) {
		return;
	}

	clientNum = level.sortedClients[0];

	if ( level.clients[ clientNum ].pers.connected != CON_CONNECTED ) {
		return;
	}

	// make them a spectator
	SetTeam( &g_entities[ clientNum ], "s" );
}

/*
=======================
AdjustTournamentScores
=======================
*/
void AdjustTournamentScores( void ) {
	int			clientNum;

	clientNum = level.sortedClients[0];
	if ( level.clients[ clientNum ].pers.connected == CON_CONNECTED ) {
		level.clients[ clientNum ].sess.wins++;
		ClientUserinfoChanged( clientNum );
	}

	clientNum = level.sortedClients[1];
	if ( level.clients[ clientNum ].pers.connected == CON_CONNECTED ) {
		level.clients[ clientNum ].sess.losses++;
		ClientUserinfoChanged( clientNum );
	}

}

/*
=============
SortRanks

=============
*/
int QDECL SortRanks( const void *a, const void *b ) {
	gclient_t	*ca, *cb;

	ca = &level.clients[*(int *)a];
	cb = &level.clients[*(int *)b];

	// sort special clients last
	if ( ca->sess.spectatorState == SPECTATOR_SCOREBOARD || ca->sess.spectatorClient < 0 ) {
		return 1;
	}
	if ( cb->sess.spectatorState == SPECTATOR_SCOREBOARD || cb->sess.spectatorClient < 0  ) {
		return -1;
	}

	// then connecting clients
	if ( ca->pers.connected == CON_CONNECTING ) {
		return 1;
	}
	if ( cb->pers.connected == CON_CONNECTING ) {
		return -1;
	}


	// then spectators
	if ( ca->sess.sessionTeam == TEAM_SPECTATOR && cb->sess.sessionTeam == TEAM_SPECTATOR ) {
		if ( ca->sess.spectatorNum > cb->sess.spectatorNum ) {
			return -1;
		}
		if ( ca->sess.spectatorNum < cb->sess.spectatorNum ) {
			return 1;
		}
		return 0;
	}
	if ( ca->sess.sessionTeam == TEAM_SPECTATOR ) {
		return 1;
	}
	if ( cb->sess.sessionTeam == TEAM_SPECTATOR ) {
		return -1;
	}

	// then sort by score
	if ( ca->ps.persistant[PERS_SCORE]
		> cb->ps.persistant[PERS_SCORE] ) {
		return -1;
	}
	if ( ca->ps.persistant[PERS_SCORE]
		< cb->ps.persistant[PERS_SCORE] ) {
		return 1;
	}
	return 0;
}

/*
============
CalculateRanks

Recalculates the score ranks of all players
This will be called on every client connect, begin, disconnect, death,
and team change.
============
*/
void CalculateRanks( void ) {
	int		i;
	int		rank;
	int		score;
	int		newScore;
	gclient_t	*cl;

	level.follow1 = -1;
	level.follow2 = -1;
	level.numConnectedClients = 0;
	level.numNonSpectatorClients = 0;
	level.numPlayingClients = 0;
	level.numVotingClients = 0;		// don't count bots

	for (i = 0; i < ARRAY_LEN(level.numteamVotingClients); i++)
		level.numteamVotingClients[i] = 0;

	for ( i = 0 ; i < level.maxclients ; i++ ) {
		if ( level.clients[i].pers.connected != CON_DISCONNECTED ) {
			level.sortedClients[level.numConnectedClients] = i;
			level.numConnectedClients++;

			if ( level.clients[i].sess.sessionTeam != TEAM_SPECTATOR ) {
				level.numNonSpectatorClients++;
			
				// decide if this should be auto-followed
				if ( level.clients[i].pers.connected == CON_CONNECTED ) {
					level.numPlayingClients++;
					if ( !(g_entities[i].r.svFlags & SVF_BOT) ) {
						level.numVotingClients++;
						if ( level.clients[i].sess.sessionTeam == TEAM_RED )
							level.numteamVotingClients[0]++;
						else if ( level.clients[i].sess.sessionTeam == TEAM_BLUE )
							level.numteamVotingClients[1]++;
					}
					if ( level.follow1 == -1 ) {
						level.follow1 = i;
					} else if ( level.follow2 == -1 ) {
						level.follow2 = i;
					}
				}
			}
		}
	}

	qsort( level.sortedClients, level.numConnectedClients, 
		sizeof(level.sortedClients[0]), SortRanks );

	// set the rank value for all clients that are connected and not spectators
	if ( g_gametype.integer >= GT_TEAM ) {
		// in team games, rank is just the order of the teams, 0=red, 1=blue, 2=tied
		for ( i = 0;  i < level.numConnectedClients; i++ ) {
			cl = &level.clients[ level.sortedClients[i] ];
			if ( level.teamScores[TEAM_RED] == level.teamScores[TEAM_BLUE] ) {
				cl->ps.persistant[PERS_RANK] = 2;
			} else if ( level.teamScores[TEAM_RED] > level.teamScores[TEAM_BLUE] ) {
				cl->ps.persistant[PERS_RANK] = 0;
			} else {
				cl->ps.persistant[PERS_RANK] = 1;
			}
		}
	} else {	
		rank = -1;
		score = 0;
		for ( i = 0;  i < level.numPlayingClients; i++ ) {
			cl = &level.clients[ level.sortedClients[i] ];
			newScore = cl->ps.persistant[PERS_SCORE];
			if ( i == 0 || newScore != score ) {
				rank = i;
				// assume we aren't tied until the next client is checked
				level.clients[ level.sortedClients[i] ].ps.persistant[PERS_RANK] = rank;
			} else {
				// we are tied with the previous client
				level.clients[ level.sortedClients[i-1] ].ps.persistant[PERS_RANK] = rank | RANK_TIED_FLAG;
				level.clients[ level.sortedClients[i] ].ps.persistant[PERS_RANK] = rank | RANK_TIED_FLAG;
			}
			score = newScore;
			if ( g_singlePlayer.integer && level.numPlayingClients == 1 ) {
				level.clients[ level.sortedClients[i] ].ps.persistant[PERS_RANK] = rank | RANK_TIED_FLAG;
			}
		}
	}

	// set the CS_SCORES1/2 configstrings, which will be visible to everyone
	if ( g_gametype.integer >= GT_TEAM ) {
		trap_SetConfigstring( CS_SCORES1, va("%i", level.teamScores[TEAM_RED] ) );
		trap_SetConfigstring( CS_SCORES2, va("%i", level.teamScores[TEAM_BLUE] ) );
	} else {
		if ( level.numConnectedClients == 0 ) {
			trap_SetConfigstring( CS_SCORES1, va("%i", SCORE_NOT_PRESENT) );
			trap_SetConfigstring( CS_SCORES2, va("%i", SCORE_NOT_PRESENT) );
		} else if ( level.numConnectedClients == 1 ) {
			trap_SetConfigstring( CS_SCORES1, va("%i", level.clients[ level.sortedClients[0] ].ps.persistant[PERS_SCORE] ) );
			trap_SetConfigstring( CS_SCORES2, va("%i", SCORE_NOT_PRESENT) );
		} else {
			trap_SetConfigstring( CS_SCORES1, va("%i", level.clients[ level.sortedClients[0] ].ps.persistant[PERS_SCORE] ) );
			trap_SetConfigstring( CS_SCORES2, va("%i", level.clients[ level.sortedClients[1] ].ps.persistant[PERS_SCORE] ) );
		}
	}

	// see if it is time to end the level
	CheckExitRules();

	// if we are at the intermission, send the new info to everyone
	if ( level.intermissiontime ) {
		SendScoreboardMessageToAllClients();
	}
}


/*
========================================================================

MAP CHANGING

========================================================================
*/

/*
========================
SendScoreboardMessageToAllClients

Do this at BeginIntermission time and whenever ranks are recalculated
due to enters/exits/forced team changes
========================
*/
void SendScoreboardMessageToAllClients( void ) {
	int		i;

	for ( i = 0 ; i < level.maxclients ; i++ ) {
		if ( level.clients[ i ].pers.connected == CON_CONNECTED ) {
			DeathmatchScoreboardMessage( g_entities + i );
		}
	}
}

/*
========================
MoveClientToIntermission

When the intermission starts, this will be called for all players.
If a new client connects, this will be called after the spawn function.
========================
*/
void MoveClientToIntermission( gentity_t *ent ) {
	// take out of follow mode if needed
	if ( ent->client->sess.spectatorState == SPECTATOR_FOLLOW ) {
		StopFollowing( ent );
	}

	FindIntermissionPoint();
	// move to the spot
	VectorCopy( level.intermission_origin, ent->s.origin );
	VectorCopy( level.intermission_origin, ent->client->ps.origin );
	VectorCopy (level.intermission_angle, ent->client->ps.viewangles);
	ent->client->ps.pm_type = PM_INTERMISSION;

	// clean up powerup info
	memset( ent->client->ps.powerups, 0, sizeof(ent->client->ps.powerups) );

	ent->client->ps.eFlags = 0;
	ent->s.eFlags = 0;
	ent->s.eType = ET_GENERAL;
	ent->s.modelindex = 0;
	ent->s.loopSound = 0;
	ent->s.event = 0;
	ent->r.contents = 0;
}

/*
==================
FindIntermissionPoint

This is also used for spectator spawns
==================
*/
void FindIntermissionPoint( void ) {
	gentity_t	*ent, *target;
	vec3_t		dir;

	// find the intermission spot
	ent = G_Find (NULL, FOFS(classname), "info_player_intermission");
	if ( !ent ) {	// the map creator forgot to put in an intermission point...
		{
			vec3_t avoidOrigin = { 0, 0, 0 };
			SelectSpawnPoint( avoidOrigin, level.intermission_origin, level.intermission_angle, qfalse );
		}
	} else {
		VectorCopy (ent->s.origin, level.intermission_origin);
		VectorCopy (ent->s.angles, level.intermission_angle);
		// if it has a target, look towards it
		if ( ent->target ) {
			target = G_PickTarget( ent->target );
			if ( target ) {
				VectorSubtract( target->s.origin, level.intermission_origin, dir );
				vectoangles( dir, level.intermission_angle );
			}
		}
	}

}

/*
==================
BeginIntermission
==================
*/
void BeginIntermission( void ) {
	int			i;
	gentity_t	*client;

	if ( level.intermissiontime ) {
		return;		// already active
	}

	// if in tournement mode, change the wins / losses
	if ( g_gametype.integer == GT_TOURNAMENT ) {
		AdjustTournamentScores();
	}

	level.intermissiontime = level.time;
	// move all clients to the intermission point
	for (i=0 ; i< level.maxclients ; i++) {
		client = g_entities + i;
		if (!client->inuse)
			continue;
		// respawn if dead
		if (client->health <= 0) {
			ClientRespawn(client);
		}
		MoveClientToIntermission( client );
	}

    if (g_singlePlayer.integer) {
		UpdateTournamentInfo();
	}

    // send the current scoring to all clients
	SendScoreboardMessageToAllClients();

#if FEAT_AUTO_DEMO
	// auto-demo (10K): stop recording when intermission starts
	if ( g_autoDemo.integer ) {
		trap_SendServerCommand( -1, "stoprecord\n" );
	}
#endif
}


/*
=============
ExitLevel

When the intermission has been exited, the server is either killed
or moved to a new level based on the "nextmap" cvar 

=============
*/
void ExitLevel (void) {
	int		i;
	gclient_t *cl;
	char nextmap[MAX_STRING_CHARS];
	char d1[MAX_STRING_CHARS];

#if FEAT_JSON_STATS
	G_WriteStatsJSON();
#endif
#if FEAT_ELO_TRACKING
	G_CalculateEloChanges();
#endif

	//bot interbreeding
	BotInterbreedEndMatch();

	// if we are running a tournement map, kick the loser to spectator status,
	// which will automatically grab the next spectator and restart
	if ( g_gametype.integer == GT_TOURNAMENT  ) {
		if ( !level.restarted ) {
			RemoveTournamentLoser();
			trap_SendConsoleCommand( EXEC_APPEND, "map_restart 0\n" );
			level.restarted = qtrue;
			level.changemap = NULL;
			level.intermissiontime = 0;
		}
		return;	
	}

	// standard Q3 nextmap flow — rotation index is advanced in G_InitGame, not here — nextmap cvar was set by G_PeekRotationMap at init
	trap_Cvar_VariableStringBuffer( "nextmap", nextmap, sizeof(nextmap) );
	trap_Cvar_VariableStringBuffer( "d1", d1, sizeof(d1) );

	if( !Q_stricmp( nextmap, "map_restart 0" ) && Q_stricmp( d1, "" ) ) {
		trap_Cvar_Set( "nextmap", "vstr d2" );
		trap_SendConsoleCommand( EXEC_APPEND, "vstr d1\n" );
	} else {
		trap_SendConsoleCommand( EXEC_APPEND, "vstr nextmap\n" );
	}

	level.changemap = NULL;
	level.intermissiontime = 0;

	// reset all the scores so we don't enter the intermission again
	level.teamScores[TEAM_RED] = 0;
	level.teamScores[TEAM_BLUE] = 0;
	for ( i=0 ; i< g_maxclients.integer ; i++ ) {
		cl = level.clients + i;
		if ( cl->pers.connected != CON_CONNECTED ) {
			continue;
		}
		cl->ps.persistant[PERS_SCORE] = 0;
	}

	// we need to do this here before changing to CON_CONNECTING
	G_WriteSessionData();

	// change all client states to connecting, so the early players into the
	// next level will know the others aren't done reconnecting
	for (i=0 ; i< g_maxclients.integer ; i++) {
		if ( level.clients[i].pers.connected == CON_CONNECTED ) {
			level.clients[i].pers.connected = CON_CONNECTING;
		}
	}

}

/*
=================
G_LogPrintf

Print to the logfile with a time stamp if it is open
=================
*/
void QDECL G_LogPrintf( const char *fmt, ... ) {
	va_list		argptr;
	char		string[1024];
	int			min, tens, sec;

	sec = ( level.time - level.startTime ) / 1000;

	min = sec / 60;
	sec -= min * 60;
	tens = sec / 10;
	sec -= tens * 10;

	Com_sprintf( string, sizeof(string), "%3i:%i%i ", min, tens, sec );

	va_start( argptr, fmt );
	Q_vsnprintf(string + 7, sizeof(string) - 7, fmt, argptr);
	va_end( argptr );

	if ( g_dedicated.integer ) {
		G_Printf( "%s", string + 7 );
	}

	if ( !level.logFile ) {
		return;
	}

	trap_FS_Write( string, strlen( string ), level.logFile );
}

/*
================
LogExit

Append information about this game to the log file
================
*/
void LogExit( const char *string ) {
	int				i, numSorted;
	gclient_t		*cl;
	team_t			team = TEAM_RED;
	qboolean won = qtrue;
	G_LogPrintf( "Exit: %s\n", string );

	level.intermissionQueued = level.time;

	// this will keep the clients from playing any voice sounds
	// that will get cut off when the queued intermission starts
	trap_SetConfigstring( CS_INTERMISSION, "1" );

	// don't send more than 32 scores (FIXME?)
	numSorted = level.numConnectedClients;
	if ( numSorted > 32 ) {
		numSorted = 32;
	}

	if ( g_gametype.integer >= GT_TEAM ) {
		G_LogPrintf( "red:%i  blue:%i\n",
			level.teamScores[TEAM_RED], level.teamScores[TEAM_BLUE] );
	}

	for (i=0 ; i < numSorted ; i++) {
		int		ping;

		cl = &level.clients[level.sortedClients[i]];

		if ( cl->sess.sessionTeam == TEAM_SPECTATOR ) {
			continue;
		}
		if ( cl->pers.connected == CON_CONNECTING ) {
			continue;
		}

		ping = cl->ps.ping < 999 ? cl->ps.ping : 999;

		G_LogPrintf( "score: %i  ping: %i  client: %i %s\n", cl->ps.persistant[PERS_SCORE], ping, level.sortedClients[i],	cl->pers.netname );
		if (g_singlePlayer.integer && g_gametype.integer < GT_TEAM) {
			if (g_entities[cl - level.clients].r.svFlags & SVF_BOT && cl->ps.persistant[PERS_RANK] == 0) {
				won = qfalse;
			}
		}

	}

	if (g_singlePlayer.integer) {
		if (g_gametype.integer >= GT_TEAM) {
			if (team == TEAM_BLUE) {
				won = level.teamScores[TEAM_BLUE] > level.teamScores[TEAM_RED];
			} else {
				won = level.teamScores[TEAM_RED] > level.teamScores[TEAM_BLUE];
			}
		}
		trap_SendConsoleCommand( EXEC_APPEND, (won) ? "spWin\n" : "spLose\n" );
	}


}


/*
=================
CheckIntermissionExit

The level will stay at the intermission for a minimum of 5 seconds
If all players wish to continue, the level will then exit.
If one or more players have not acknowledged the continue, the game will
wait 10 seconds before going on.
=================
*/
void CheckIntermissionExit( void ) {
	int			ready, notReady, playerCount;
	int			i;
	gclient_t	*cl;
	int			readyMask;

	if ( g_singlePlayer.integer ) {
		return;
	}

	// see which players are ready
	ready = 0;
	notReady = 0;
	readyMask = 0;
	playerCount = 0;
	for (i=0 ; i< g_maxclients.integer ; i++) {
		cl = level.clients + i;
		if ( cl->pers.connected != CON_CONNECTED ) {
			continue;
		}
		if ( g_entities[i].r.svFlags & SVF_BOT ) {
			continue;
		}

		playerCount++;
		if ( cl->readyToExit ) {
			ready++;
			if ( i < 16 ) {
				readyMask |= 1 << i;
			}
		} else {
			notReady++;
		}
	}

	// copy the readyMask to each player's stats so
	// it can be displayed on the scoreboard
	for (i=0 ; i< g_maxclients.integer ; i++) {
		cl = level.clients + i;
		if ( cl->pers.connected != CON_CONNECTED ) {
			continue;
		}
		cl->ps.stats[STAT_CLIENTS_READY] = readyMask;
	}

	// never exit in less than five seconds
	if ( level.time < level.intermissiontime + 5000 ) {
		return;
	}

	// only test ready status when there are real players present
	if ( playerCount > 0 ) {
		// if nobody wants to go, clear timer
		if ( !ready ) {
			level.readyToExit = qfalse;
			return;
		}

		// if everyone wants to go, go now
		if ( !notReady ) {
			ExitLevel();
			return;
		}
	}

	// the first person to ready starts the ten second timeout
	if ( !level.readyToExit ) {
		level.readyToExit = qtrue;
		level.exitTime = level.time;
	}

	// if we have waited ten seconds since at least one player
	// wanted to exit, go ahead
	if ( level.time < level.exitTime + 10000 ) {
		return;
	}

	ExitLevel();
}

/*
=============
ScoreIsTied
=============
*/
qboolean ScoreIsTied( void ) {
	int		a, b;

	if ( level.numPlayingClients < 2 ) {
		return qfalse;
	}
	
	if ( g_gametype.integer >= GT_TEAM ) {
		return level.teamScores[TEAM_RED] == level.teamScores[TEAM_BLUE];
	}

	a = level.clients[level.sortedClients[0]].ps.persistant[PERS_SCORE];
	b = level.clients[level.sortedClients[1]].ps.persistant[PERS_SCORE];

	return a == b;
}

/*
=================
CheckExitRules

There will be a delay between the time the exit is qualified for
and the time everyone is moved to the intermission spot, so you
can see the last frag.
=================
*/
void CheckExitRules( void ) {
 	int			i;
	gclient_t	*cl;
    int numClients;
    int aliveClients;

	// if at the intermission, wait for all non-bots to
	// signal ready, then go to next level
	if ( level.intermissiontime ) {
		CheckIntermissionExit ();
		return;
	}

	if ( level.intermissionQueued ) {

        if (level.time - level.intermissionQueued >= INTERMISSION_DELAY_TIME) {
			level.intermissionQueued = 0;
			BeginIntermission();
		}

        return;
	}

	// check for sudden death
	if ( ScoreIsTied() ) {
		// always wait for sudden death
		return;
	}

    if (level.warmupTime != 0) {
        return;
    }

	if ( g_timelimit.integer < 0 || g_timelimit.integer > INT_MAX / 60000 ) {
		G_Printf( "timelimit %i is out of range, defaulting to 0\n", g_timelimit.integer );
		trap_Cvar_Set( "timelimit", "0" );
		trap_Cvar_Update( &g_timelimit );
	}

	if ( g_timelimit.integer ) {
		if ( level.time - level.startTime >= g_timelimit.integer*60000 ) {
#if FEAT_OVERTIME
			// overtime (10D): extend timelimit when score is tied
			if ( g_overtime.integer > 0 && ScoreIsTied() ) {
				level.overtimeCount++;
				trap_Cvar_Set( "timelimit", va( "%i", g_timelimit.integer + g_overtime.integer ) );
				trap_Cvar_Update( &g_timelimit );
				trap_SendServerCommand( -1, va( "print \"Score tied! Overtime #%d (%d min).\n\"",
					level.overtimeCount, g_overtime.integer ) );
				return;
			}
#endif
			trap_SendServerCommand( -1, "print \"Timelimit hit.\n\"");
			LogExit( "Timelimit hit." );
			return;
		}
	}

	if ( g_fraglimit.integer < 0 ) {
		G_Printf( "fraglimit %i is out of range, defaulting to 0\n", g_fraglimit.integer );
		trap_Cvar_Set( "fraglimit", "0" );
		trap_Cvar_Update( &g_fraglimit );
	}

	if ( g_gametype.integer < GT_CTF && g_fraglimit.integer ) {
		if ( level.teamScores[TEAM_RED] >= g_fraglimit.integer ) {
			trap_SendServerCommand( -1, "print \"Red hit the fraglimit.\n\"" );
			LogExit( "Fraglimit hit." );
			return;
		}

		if ( level.teamScores[TEAM_BLUE] >= g_fraglimit.integer ) {
			trap_SendServerCommand( -1, "print \"Blue hit the fraglimit.\n\"" );
			LogExit( "Fraglimit hit." );
			return;
		}

        numClients = 0;
        aliveClients = 0;
		for ( i=0 ; i< g_maxclients.integer ; i++ ) {
			cl = level.clients + i;
			if ( cl->pers.connected != CON_CONNECTED ) {
				continue;
			}
			if ( cl->sess.sessionTeam != TEAM_FREE ) {
				continue;
			}

            numClients++;

            if (g_gametype.integer == GT_LASTMANSTANDING) {
                if (cl->ps.persistant[PERS_SCORE] > 0) {
                    aliveClients++;
                }
            }
            else {
                if (cl->ps.persistant[PERS_SCORE] >= g_fraglimit.integer) {
                    LogExit("Fraglimit hit.");
                    trap_SendServerCommand(-1, va("print \"" S_COLOR_GREEN "%s" S_COLOR_WHITE " hit the fraglimit.\n\"",
                        cl->pers.netname));
                    return;
                }
            }
		}

        if (g_gametype.integer == GT_LASTMANSTANDING && numClients >= 2 && aliveClients <= 1) {
            for (i = 0; i < g_maxclients.integer; i++) {
                cl = level.clients + i;
                if (cl->pers.connected != CON_CONNECTED) {
                    continue;
                }

                if (cl->sess.sessionTeam == TEAM_FREE && cl->ps.persistant[PERS_SCORE] > 0) {
                    trap_SendServerCommand(-1, va("print \"" S_COLOR_GREEN "%s" S_COLOR_WHITE " hit the fraglimit.\n\"",
                        cl->pers.netname));

                    cl->sess.wins++;
                }
                else {
                    cl->sess.losses++;
                }
            }

            // FIXME draw
            LogExit("Fraglimit hit.");
            return;
        }
	}

	if ( g_capturelimit.integer < 0 ) {
		G_Printf( "capturelimit %i is out of range, defaulting to 0\n", g_capturelimit.integer );
		trap_Cvar_Set( "capturelimit", "0" );
		trap_Cvar_Update( &g_capturelimit );
	}

	if ( g_gametype.integer >= GT_CTF && g_capturelimit.integer ) {

		if ( level.teamScores[TEAM_RED] >= g_capturelimit.integer ) {
			trap_SendServerCommand( -1, "print \"Red hit the capturelimit.\n\"" );
			LogExit( "Capturelimit hit." );
			return;
		}

		if ( level.teamScores[TEAM_BLUE] >= g_capturelimit.integer ) {
			trap_SendServerCommand( -1, "print \"Blue hit the capturelimit.\n\"" );
			LogExit( "Capturelimit hit." );
			return;
		}
	}
}



/*
========================================================================

FUNCTIONS CALLED EVERY FRAME

========================================================================
*/

/*
==================
FindTheKing
==================
*/
gentity_t *FindTheKing() {
    gentity_t *ent;
    int i;

    if (g_gametype.integer != GT_KINGOFTHEHILL) {
        return NULL;
    }

    for (i = 0; i< g_maxclients.integer; i++) {
        ent = g_entities + i;
        if (!ent->client) {
            continue;
        }

        if (ent->client->pers.connected != CON_CONNECTED) {
            continue;
        }

        if (ent->client->sess.sessionTeam != TEAM_FREE) {
            continue;
        }

        if (ent->health <= 0) {
            continue;
        }

        if (ent->client->ps.powerups[PW_KING]) {
            return ent;
        }
    }

    return NULL;
}

/*
==================
HonorAsKing
==================
*/
qboolean HonorAsKing(gentity_t *ent) {
    int i;

    if (g_gametype.integer != GT_KINGOFTHEHILL) {
        return qfalse;
    }

    if (!ent->client) {
        return qfalse;
    }

    if (ent->client->pers.connected != CON_CONNECTED) {
        return qfalse;
    }

    if (ent->client->sess.sessionTeam != TEAM_FREE) {
        return qfalse;
    }



    if (ent->health <= 0) {
        return qfalse;
    }

    ent->client->ps.powerups[PW_KING] = -1;

    if (ent->client->ps.stats[STAT_HEALTH] < MAX_HEALTH) {
        ent->health = ent->client->ps.stats[STAT_HEALTH] = MAX_HEALTH;
    }
    ent->client->ps.stats[STAT_ARMOR] = MAX_ARMOR;
    ent->client->ps.stats[STAT_ARMORCLASS] = ARM_HEAVY;

    for (i = WP_NONE + 1; i < WP_NUM_WEAPONS; i++) {
        if (!bg_weaponlist[i].spawnWeapon && !(level.mapWeapons & (1 << i)) && g_spawnWeapons.integer <= 1) {
            continue;
        }

        ent->client->ps.stats[STAT_WEAPONS] |= (1 << i);
        ent->client->ps.ammo[i] = bg_weaponlist[i].maxAmmunition;
    }

    trap_SendServerCommand(-1, va("cp \"" S_COLOR_GREEN "%s" S_COLOR_WHITE " is the new king\n\"", ent->client->pers.netname));

    return qtrue;
}

/*
==================
AssignAKing
==================
*/
gentity_t *AssignAKing(gentity_t *preferred) {
    gentity_t *ent;
    int i;

    if (HonorAsKing(preferred)) {
        return preferred;
    }

    for (i = 0; i< g_maxclients.integer; i++) {
        ent = g_entities + i;
        if (ent == preferred) {
            continue;
        }

        if (HonorAsKing(ent)) {
            return ent;
        }
    }

    return NULL;
}

void CheckKingOfTheHill(void) {
    //if(!FindTheKing()) {
    //	AssignAKing( NULL );
    //}
}

/*
=============
CheckTournament

Once a frame, check for changes in tournement player state
=============
*/
void CheckTournament( void ) {
	// check because we run 3 game frames before calling Connect and/or ClientBegin
	// for clients on a map_restart
	if ( level.numPlayingClients == 0 ) {
		return;
	}

	if ( g_gametype.integer == GT_TOURNAMENT ) {

		// pull in a spectator if needed
		if ( level.numPlayingClients < 2 ) {
			AddTournamentPlayer();
		}

		// if we don't have two players, go back to "waiting for players"
		if ( level.numPlayingClients != 2 ) {
			if ( level.warmupTime != -1 ) {
				level.warmupTime = -1;
				trap_SetConfigstring( CS_WARMUP, va("%i", level.warmupTime) );
				G_LogPrintf( "Warmup:\n" );
			}
			return;
		}

		if ( level.warmupTime == 0 ) {
			return;
		}

		// if the warmup is changed at the console, restart it
		if ( g_warmup.modificationCount != level.warmupModificationCount ) {
			level.warmupModificationCount = g_warmup.modificationCount;
			level.warmupTime = -1;
		}

		// if all players have arrived, start the countdown
		if ( level.warmupTime < 0 ) {
			if ( level.numPlayingClients == 2 ) {
#if FEAT_READY_UP
				// ready-up (4E): wait for all players to be ready
				if ( g_startWhenReady.integer && !G_AllPlayersReady() ) {
					return;
				}
#endif
				// fudge by -1 to account for extra delays
				if ( g_warmup.integer > 1 ) {
					level.warmupTime = level.time + ( g_warmup.integer - 1 ) * 1000;
				} else {
					level.warmupTime = 0;
				}

				trap_SetConfigstring( CS_WARMUP, va("%i", level.warmupTime) );
			}
			return;
		}

		// if the warmup time has counted down, restart
		if ( level.time > level.warmupTime ) {
			level.warmupTime += 10000;
			trap_Cvar_Set( "g_restarted", "1" );
			trap_SendConsoleCommand( EXEC_APPEND, "map_restart 0\n" );
			level.restarted = qtrue;
			return;
		}
	} else if ( !g_singlePlayer.integer && level.warmupTime != 0 ) {
		int		counts[TEAM_NUM_TEAMS];
		qboolean	notEnough = qfalse;

		if ( g_gametype.integer >= GT_TEAM ) {
			counts[TEAM_BLUE] = TeamCount( -1, TEAM_BLUE );
			counts[TEAM_RED] = TeamCount( -1, TEAM_RED );

			if (counts[TEAM_RED] < 1 || counts[TEAM_BLUE] < 1) {
				notEnough = qtrue;
			}
		} else if ( level.numPlayingClients < 2 ) {
			notEnough = qtrue;
		}

		if ( notEnough ) {
			if ( level.warmupTime != -1 ) {
				level.warmupTime = -1;
				trap_SetConfigstring( CS_WARMUP, va("%i", level.warmupTime) );
				G_LogPrintf( "Warmup:\n" );
			}
			return; // still waiting for team members
		}

		if ( level.warmupTime == 0 ) {
			return;
		}

		// if the warmup is changed at the console, restart it
		if ( g_warmup.modificationCount != level.warmupModificationCount ) {
			level.warmupModificationCount = g_warmup.modificationCount;
			level.warmupTime = -1;
		}

		// if all players have arrived, start the countdown
		if ( level.warmupTime < 0 ) {
#if FEAT_READY_UP
			// ready-up (4E): wait for all players to be ready
			if ( g_startWhenReady.integer && !G_AllPlayersReady() ) {
				return;
			}
#endif
			// fudge by -1 to account for extra delays
			if ( g_warmup.integer > 1 ) {
				level.warmupTime = level.time + ( g_warmup.integer - 1 ) * 1000;
			} else {
				level.warmupTime = 0;
			}

			trap_SetConfigstring( CS_WARMUP, va("%i", level.warmupTime) );
			return;
		}

		// if the warmup time has counted down, restart
		if ( level.time > level.warmupTime ) {
			level.warmupTime += 10000;
			trap_Cvar_Set( "g_restarted", "1" );
			trap_SendConsoleCommand( EXEC_APPEND, "map_restart 0\n" );
			level.restarted = qtrue;
			return;
		}
	}
}


/*
==================
CheckVote
==================
*/
void CheckVote( void ) {
	if ( level.voteExecuteTime && level.voteExecuteTime < level.time ) {
		level.voteExecuteTime = 0;

		// eser - team shuffle command
		if ( !Q_stricmp( level.voteString, "shuffle" ) ) {
			G_ShuffleTeams();
		} else

		// eser - team shuffle command
		trap_SendConsoleCommand( EXEC_APPEND, va("%s\n", level.voteString ) );
	}

	if ( !level.voteTime ) {
		return;
	}

	if ( level.time - level.voteTime >= VOTE_TIME ) {
		trap_SendServerCommand( -1, "print \"Vote failed.\n\"" );
	} else {
		// ATVI Q3 1.32 Patch #9, WNF
		if ( level.voteYes > level.numVotingClients/2 ) {
			// execute the command, then remove the vote
			trap_SendServerCommand( -1, "print \"Vote passed.\n\"" );
			level.voteExecuteTime = level.time + 3000;
		} else if ( level.voteNo >= level.numVotingClients/2 ) {
			// same behavior as a timeout
			trap_SendServerCommand( -1, "print \"Vote failed.\n\"" );
		} else {
			// still waiting for a majority
			return;
		}
	}
	level.voteTime = 0;
	trap_SetConfigstring( CS_VOTE_TIME, "" );

}

#if FEAT_MAP_ROTATION
/*
==================
G_NextRotationMap

Returns the next map from g_maprotation. Cycles through the space-separated
list. Mode 0 = sequential, mode 1 = random. (6D)
==================
*/
/*
 * Rotation data flow:
 *
 *   G_InitGame → G_PeekRotationMap() → sets nextmap cvar + advances index
 *                                            ↓
 *   vstr nextmap (ExitLevel / callvote / manual) → loads the next map
 *                                            ↓
 *   G_InitGame → G_PeekRotationMap() → sets nextmap to NEXT map + advances
 */

/*
==================
G_PeekRotationMap

Returns the next map name WITHOUT advancing the rotation index.
Used at map init to set the nextmap cvar.
==================
*/
const char *G_PeekRotationMap( void ) {
	static char mapname[MAX_QPATH];
	static char mapBuf[64][MAX_QPATH];
	char rotation[MAX_INFO_STRING];
	int numMaps, i;
	char *p;
	const char *token;

	trap_Cvar_VariableStringBuffer( "g_maprotation", rotation, sizeof( rotation ) );
	if ( !rotation[0] ) {
		return NULL;
	}

	/* tokenize the space-separated map list, copying each token
	   into a persistent buffer (COM_Parse uses a static buffer
	   that gets overwritten on each call) */
	numMaps = 0;
	p = rotation;
	while ( numMaps < 64 ) {
		token = COM_Parse( (const char **)&p );
		if ( !token[0] ) break;
		Q_strncpyz( mapBuf[numMaps], token, MAX_QPATH );
		numMaps++;
	}

	if ( numMaps == 0 ) {
		return NULL;
	}

	if ( g_maprotationMode.integer == 1 ) {
		i = rand() % numMaps;
	} else {
		/* g_maprotationIndex holds the CURRENT map's index; +1 gives the NEXT map */
		int idx = trap_Cvar_VariableIntegerValue( "g_maprotationIndex" );
		i = ( idx + 1 ) % numMaps;
		/* advance: this map load moves us to the next position */
		trap_Cvar_Set( "g_maprotationIndex", va( "%d", i ) );
	}

	Q_strncpyz( mapname, mapBuf[i], sizeof( mapname ) );
	return mapname;
}


/*
==================
G_AdvanceRotation

Advance the rotation index. Called from ExitLevel when the map actually changes.
==================
*/
void G_AdvanceRotation( void ) {
	int idx = trap_Cvar_VariableIntegerValue( "g_maprotationIndex" );
	trap_Cvar_Set( "g_maprotationIndex", va( "%d", idx + 1 ) );
}
#endif

// eser - team shuffle command
/*
==================
G_ShuffleTeams

Randomly redistributes all non-spectator players across red and blue teams,
then restarts the map. (7D)
==================
*/
void G_ShuffleTeams( void ) {
	int i, count = 0;
	int players[MAX_CLIENTS];
	gclient_t *cl;

	// collect non-spectator players
	for ( i = 0; i < level.maxclients; i++ ) {
		cl = level.clients + i;
		if ( cl->pers.connected != CON_CONNECTED ) continue;
		if ( cl->sess.sessionTeam == TEAM_SPECTATOR ) continue;
		players[count++] = i;
	}

	// Fisher-Yates shuffle
	for ( i = count - 1; i > 0; i-- ) {
		int j = rand() % ( i + 1 );
		int tmp = players[i];
		players[i] = players[j];
		players[j] = tmp;
	}

	// assign alternating teams
	for ( i = 0; i < count; i++ ) {
		cl = level.clients + players[i];
		cl->sess.sessionTeam = ( i % 2 ) ? TEAM_BLUE : TEAM_RED;
		ClientUserinfoChanged( players[i] );
	}

	trap_SendServerCommand( -1, "print \"Teams have been shuffled!\n\"" );
	trap_SendConsoleCommand( EXEC_APPEND, "map_restart 0\n" );
}
// eser - team shuffle command

/*
==================
PrintTeam
==================
*/
void PrintTeam(int team, char *message) {
	int i;

	for ( i = 0 ; i < level.maxclients ; i++ ) {
		if (level.clients[i].sess.sessionTeam != team)
			continue;
		trap_SendServerCommand( i, message );
	}
}

#if FEAT_READY_UP
/*
==================
G_AllPlayersReady

Returns qtrue if all non-spectator players are ready. (4E)
==================
*/
qboolean G_AllPlayersReady( void ) {
	int i;
	gclient_t *cl;

	for ( i = 0; i < level.maxclients; i++ ) {
		cl = level.clients + i;
		if ( cl->pers.connected != CON_CONNECTED ) {
			continue;
		}
		if ( cl->sess.sessionTeam == TEAM_SPECTATOR ) {
			continue;
		}
		// bots are always ready
		if ( g_entities[i].r.svFlags & SVF_BOT ) {
			continue;
		}
		if ( !cl->ready ) {
			return qfalse;
		}
	}
	return qtrue;
}

/*
==================
G_SendReadymask

Broadcasts ready-state bitmask to all clients via STAT_CLIENTS_READY. (4E)
==================
*/
void G_SendReadymask( int clientNum ) {
	int i, mask = 0;
	gclient_t *cl;

	for ( i = 0; i < level.maxclients; i++ ) {
		cl = level.clients + i;
		if ( cl->pers.connected != CON_CONNECTED ) {
			continue;
		}
		if ( cl->sess.sessionTeam == TEAM_SPECTATOR ) {
			continue;
		}
		if ( cl->ready ) {
			mask |= ( 1 << i );
		}
	}
	level.readyMask = mask;

	// broadcast via STAT_CLIENTS_READY (already network-synced in playerState_t)
	for ( i = 0; i < level.maxclients; i++ ) {
		cl = level.clients + i;
		if ( cl->pers.connected == CON_CONNECTED ) {
			cl->ps.stats[STAT_CLIENTS_READY] = mask;
		}
	}
}
#endif

/*
==================
SetLeader
==================
*/
void SetLeader(int team, int client) {
	int i;

	if ( level.clients[client].pers.connected == CON_DISCONNECTED ) {
        PrintTeam(team, va("print \"" S_COLOR_GREEN "%s" S_COLOR_WHITE " is not connected\n\"", level.clients[client].pers.netname));
		return;
	}
	if (level.clients[client].sess.sessionTeam != team) {
        PrintTeam(team, va("print \"" S_COLOR_GREEN "%s" S_COLOR_WHITE " is not on the team anymore\n\"", level.clients[client].pers.netname));
		return;
	}
	for ( i = 0 ; i < level.maxclients ; i++ ) {
		if (level.clients[i].sess.sessionTeam != team)
			continue;
		if (level.clients[i].sess.teamLeader) {
			level.clients[i].sess.teamLeader = qfalse;
			ClientUserinfoChanged(i);
		}
	}
	level.clients[client].sess.teamLeader = qtrue;
	ClientUserinfoChanged( client );
    PrintTeam(team, va("print \"" S_COLOR_GREEN "%s" S_COLOR_WHITE " is the new team leader\n\"", level.clients[client].pers.netname));
}

/*
==================
CheckTeamLeader
==================
*/
void CheckTeamLeader( int team ) {
	int i;

	for ( i = 0 ; i < level.maxclients ; i++ ) {
		if (level.clients[i].sess.sessionTeam != team)
			continue;
		if (level.clients[i].sess.teamLeader)
			break;
	}
	if (i >= level.maxclients) {
		for ( i = 0 ; i < level.maxclients ; i++ ) {
			if (level.clients[i].sess.sessionTeam != team)
				continue;
			if (!(g_entities[i].r.svFlags & SVF_BOT)) {
				level.clients[i].sess.teamLeader = qtrue;
				break;
			}
		}

		if (i >= level.maxclients) {
			for ( i = 0 ; i < level.maxclients ; i++ ) {
				if (level.clients[i].sess.sessionTeam != team)
					continue;
				level.clients[i].sess.teamLeader = qtrue;
				break;
			}
		}
	}
}

/*
==================
CheckTeamVote
==================
*/
void CheckTeamVote( int team ) {
	int cs_offset;

	if ( team == TEAM_RED )
		cs_offset = 0;
	else if ( team == TEAM_BLUE )
		cs_offset = 1;
	else
		return;

	if ( !level.teamVoteTime[cs_offset] ) {
		return;
	}
	if ( level.time - level.teamVoteTime[cs_offset] >= VOTE_TIME ) {
		trap_SendServerCommand( -1, "print \"Team vote failed.\n\"" );
	} else {
		if ( level.teamVoteYes[cs_offset] > level.numteamVotingClients[cs_offset]/2 ) {
			// execute the command, then remove the vote
			trap_SendServerCommand( -1, "print \"Team vote passed.\n\"" );
			//
			if ( !Q_strncmp( "leader", level.teamVoteString[cs_offset], 6) ) {
				//set the team leader
				SetLeader(team, atoi(level.teamVoteString[cs_offset] + 7));
			}
			else {
				trap_SendConsoleCommand( EXEC_APPEND, va("%s\n", level.teamVoteString[cs_offset] ) );
			}
		} else if ( level.teamVoteNo[cs_offset] >= level.numteamVotingClients[cs_offset]/2 ) {
			// same behavior as a timeout
			trap_SendServerCommand( -1, "print \"Team vote failed.\n\"" );
		} else {
			// still waiting for a majority
			return;
		}
	}
	level.teamVoteTime[cs_offset] = 0;
	trap_SetConfigstring( CS_TEAMVOTE_TIME + cs_offset, "" );

}


/*
==================
CheckCvars
==================
*/
void CheckCvars( void ) {
	static int lastMod = -1;

	if ( g_password.modificationCount != lastMod ) {
		lastMod = g_password.modificationCount;
		if ( *g_password.string && Q_stricmp( g_password.string, "none" ) ) {
			trap_Cvar_Set( "g_needpass", "1" );
		} else {
			trap_Cvar_Set( "g_needpass", "0" );
		}
	}
}

/*
=============
G_RunThink

Runs thinking code for this frame if necessary
=============
*/
void G_RunThink (gentity_t *ent) {
	int	thinktime;

	thinktime = ent->nextthink;
	if (thinktime <= 0) {
		return;
	}
	if (thinktime > level.time) {
		return;
	}
	
	ent->nextthink = 0;
	if (!ent->think) {
		G_Error ( "NULL ent->think");
	}
	ent->think (ent);
}

#if FEAT_TEAM_AUTOBALANCE
/*
================
G_CheckTeamBalance

Called from G_RunFrame periodically. If teams differ by 2+,
move the last-joined player on the larger team to the smaller team.
================
*/
void G_CheckTeamBalance( void ) {
	int redCount, blueCount, i;
	int latestTime;
	int latestClient;
	gclient_t *cl;
	team_t bigTeam, smallTeam;

	if ( !g_teamBalance.integer ) {
		return;
	}
	if ( g_gametype.integer < GT_TEAM ) {
		return;
	}
	// only check every g_teamBalanceDelay seconds
	if ( level.time - level.lastBalanceCheck < g_teamBalanceDelay.integer * 1000 ) {
		return;
	}
	level.lastBalanceCheck = level.time;

	redCount = TeamCount( -1, TEAM_RED );
	blueCount = TeamCount( -1, TEAM_BLUE );

	if ( abs( redCount - blueCount ) < 2 ) {
		return;
	}

	if ( redCount > blueCount ) {
		bigTeam = TEAM_RED;
		smallTeam = TEAM_BLUE;
	} else {
		bigTeam = TEAM_BLUE;
		smallTeam = TEAM_RED;
	}

	// find the last-joined player on the larger team
	latestTime = 0;
	latestClient = -1;
	for ( i = 0; i < level.maxclients; i++ ) {
		cl = level.clients + i;
		if ( cl->pers.connected != CON_CONNECTED ) {
			continue;
		}
		if ( cl->sess.sessionTeam != bigTeam ) {
			continue;
		}
#if FEAT_FREEZETAG
		// don't swap frozen players
		if ( cl->ps.stats[STAT_FROZENSTATE] != FROZENSTATE_NORMAL ) {
			continue;
		}
#endif
		if ( cl->pers.enterTime > latestTime ) {
			latestTime = cl->pers.enterTime;
			latestClient = i;
		}
	}

	if ( latestClient < 0 ) {
		return;
	}

	// move the player
	SetTeam( &g_entities[latestClient], smallTeam == TEAM_RED ? "red" : "blue" );
	trap_SendServerCommand( -1, "cp \"Teams auto-balanced\n\"" );
	G_LogPrintf( "TeamAutoBalance: moved client %i to %s\n", latestClient,
		smallTeam == TEAM_RED ? "red" : "blue" );
}
#endif // FEAT_TEAM_AUTOBALANCE

#if FEAT_ELIMINATION
/*
================
G_CheckElimination

Round-based elimination modifier. Called from G_RunFrame.
================
*/
void G_CheckElimination( void ) {
	int redAlive, blueAlive, i;
	gclient_t *cl;

	if ( !g_elimination.integer ) {
		return;
	}
	if ( g_gametype.integer < GT_TEAM ) {
		return;
	}
	if ( level.intermissiontime ) {
		return;
	}

	switch ( level.roundState ) {
	case ROUND_WARMUP:
		// wait 5 seconds then go live
		if ( level.time - level.roundStartTime >= 5000 ) {
			level.roundState = ROUND_LIVE;
			level.roundStartTime = level.time;
			level.roundNumber++;
			trap_SendServerCommand( -1, va( "cp \"Round %i - FIGHT!\n\"", level.roundNumber ) );
		}
		break;

	case ROUND_LIVE:
		// count alive players per team
		redAlive = 0;
		blueAlive = 0;
		for ( i = 0; i < level.maxclients; i++ ) {
			cl = level.clients + i;
			if ( cl->pers.connected != CON_CONNECTED ) {
				continue;
			}
			if ( cl->sess.sessionTeam == TEAM_RED && cl->ps.pm_type != PM_DEAD ) {
				redAlive++;
			} else if ( cl->sess.sessionTeam == TEAM_BLUE && cl->ps.pm_type != PM_DEAD ) {
				blueAlive++;
			}
		}

		if ( redAlive == 0 && blueAlive > 0 ) {
			// blue wins the round
			level.roundWins[TEAM_BLUE]++;
			level.teamScores[TEAM_BLUE]++;
			level.roundState = ROUND_END;
			level.roundStartTime = level.time;
			trap_SendServerCommand( -1, "cp \"Blue team wins the round!\n\"" );
		} else if ( blueAlive == 0 && redAlive > 0 ) {
			// red wins the round
			level.roundWins[TEAM_RED]++;
			level.teamScores[TEAM_RED]++;
			level.roundState = ROUND_END;
			level.roundStartTime = level.time;
			trap_SendServerCommand( -1, "cp \"Red team wins the round!\n\"" );
		} else if ( level.time - level.roundStartTime >= g_eliminationRoundTime.integer * 1000 ) {
			// time expired, draw
			level.roundState = ROUND_END;
			level.roundStartTime = level.time;
			trap_SendServerCommand( -1, "cp \"Round draw - time expired!\n\"" );
		}
		break;

	case ROUND_END:
		// 3 second delay then reset
		if ( level.time - level.roundStartTime >= 3000 ) {
			G_ResetRound();
			level.roundState = ROUND_WARMUP;
			level.roundStartTime = level.time;
		}
		break;
	}
}

/*
================
G_ResetRound

Respawn all dead players, reset health/ammo for all players.
================
*/
void G_ResetRound( void ) {
	int i;
	gentity_t *ent;

	for ( i = 0; i < level.maxclients; i++ ) {
		ent = &g_entities[i];
		if ( !ent->inuse || !ent->client ) {
			continue;
		}
		if ( ent->client->pers.connected != CON_CONNECTED ) {
			continue;
		}
		if ( ent->client->sess.sessionTeam == TEAM_SPECTATOR ) {
			continue;
		}
		// respawn at a new spawn point with full health/ammo
		ClientSpawn( ent );
	}
	trap_SendServerCommand( -1, "cp \"New round starting...\n\"" );
}
#endif // FEAT_ELIMINATION

#if FEAT_ELO_TRACKING
/*
================
G_CalculateEloChanges

At end of match, calculate ELO changes.
For team games: average team ELO, then apply change to each player.
For FFA: winner vs each other player.
K = 32.
================
*/
// QVM-safe 10^x approximation for ELO (no powf in QVM)
static float ELO_Pow10( float x ) {
	// 10^x ≈ e^(x * ln10) ≈ polynomial for |x| <= 1
	// For ELO: x = ratingDiff/400, range ~ [-1, 1]
	float ln10x = x * 2.302585f;  // x * ln(10)
	float abs_ln10x = ln10x < 0 ? -ln10x : ln10x;
	float result;
	// Taylor: e^t ≈ 1 + t + t²/2 + t³/6 + t⁴/24
	result = 1.0f + ln10x + ln10x * ln10x * 0.5f
		+ ln10x * ln10x * ln10x * (1.0f/6.0f)
		+ ln10x * ln10x * ln10x * ln10x * (1.0f/24.0f);
	if ( result < 0.01f ) result = 0.01f;
	return result;
}

void G_CalculateEloChanges( void ) {
	int i;
	gclient_t *cl;

	if ( g_gametype.integer >= GT_TEAM ) {
		// team game: compare average ELOs
		int redElo = 0, blueElo = 0, redCount = 0, blueCount = 0;
		float avgRed, avgBlue, expectedRed, change;

		for ( i = 0; i < level.maxclients; i++ ) {
			cl = level.clients + i;
			if ( cl->pers.connected != CON_CONNECTED ) continue;
			if ( cl->sess.sessionTeam == TEAM_RED ) {
				redElo += cl->elo;
				redCount++;
			} else if ( cl->sess.sessionTeam == TEAM_BLUE ) {
				blueElo += cl->elo;
				blueCount++;
			}
		}
		if ( redCount == 0 || blueCount == 0 ) return;

		avgRed = (float)redElo / redCount;
		avgBlue = (float)blueElo / blueCount;

		// expected score for red
		expectedRed = 1.0f / ( 1.0f + ELO_Pow10( ( avgBlue - avgRed ) / 400.0f ) );

		if ( level.teamScores[TEAM_RED] > level.teamScores[TEAM_BLUE] ) {
			change = 32.0f * ( 1.0f - expectedRed );
		} else if ( level.teamScores[TEAM_BLUE] > level.teamScores[TEAM_RED] ) {
			change = 32.0f * ( 0.0f - expectedRed );
		} else {
			change = 32.0f * ( 0.5f - expectedRed );
		}

		for ( i = 0; i < level.maxclients; i++ ) {
			cl = level.clients + i;
			if ( cl->pers.connected != CON_CONNECTED ) continue;
			if ( cl->sess.sessionTeam == TEAM_RED ) {
				cl->elo += (int)change;
			} else if ( cl->sess.sessionTeam == TEAM_BLUE ) {
				cl->elo -= (int)change;
			}
		}
	} else {
		// FFA: winner gains vs all others
		if ( level.numPlayingClients < 2 ) return;

		{
			int winnerNum = level.sortedClients[0];
			gclient_t *winner = level.clients + winnerNum;

			for ( i = 0; i < level.numPlayingClients; i++ ) {
				int otherNum = level.sortedClients[i];
				gclient_t *other = level.clients + otherNum;
				float expected;
				int gain;

				if ( otherNum == winnerNum ) continue;
				if ( other->pers.connected != CON_CONNECTED ) continue;
				if ( other->sess.sessionTeam == TEAM_SPECTATOR ) continue;

				expected = 1.0f / ( 1.0f + ELO_Pow10( ( (float)other->elo - (float)winner->elo ) / 400.0f ) );
				gain = (int)( 32.0f * ( 1.0f - expected ) );
				winner->elo += gain;
				other->elo -= gain;
			}
		}
	}

	G_LogPrintf( "EloUpdate: ratings updated for %i players\n", level.numPlayingClients );
}
#endif // FEAT_ELO_TRACKING

#if FEAT_RANKED_QUEUE
/*
================
G_CheckRankedQueue

When enough queued players exist, auto-start match.
================
*/
void G_CheckRankedQueue( void ) {
	int queuedCount, i;
	gclient_t *cl;

	if ( !g_ranked.integer ) {
		return;
	}

	queuedCount = 0;
	for ( i = 0; i < level.maxclients; i++ ) {
		cl = level.clients + i;
		if ( cl->pers.connected != CON_CONNECTED ) continue;
		if ( cl->queued ) queuedCount++;
	}

	if ( queuedCount >= g_rankedMinPlayers.integer ) {
		// move all queued players to teams, clear queue
		for ( i = 0; i < level.maxclients; i++ ) {
			cl = level.clients + i;
			if ( cl->pers.connected != CON_CONNECTED ) continue;
			if ( !cl->queued ) continue;
			cl->queued = qfalse;
			if ( g_gametype.integer >= GT_TEAM ) {
				SetTeam( &g_entities[i], "" ); // auto-pick team
			}
		}
		trap_SendServerCommand( -1, "cp \"Ranked match starting!\n\"" );
		trap_SendConsoleCommand( EXEC_APPEND, "map_restart 0\n" );
	}
}
#endif // FEAT_RANKED_QUEUE

/*
================
G_RunFrame

Advances the non-player objects in the world
================
*/
void G_RunFrame( int levelTime ) {
	int			i;
	gentity_t	*ent;

	// if we are waiting for the level to restart, do nothing
	if ( level.restarted ) {
		return;
	}

	level.framenum++;
	level.previousTime = level.time;
	level.time = levelTime;

	// get any cvar changes
	G_UpdateCvars();

	//
	// go through all allocated objects
	//
	ent = &g_entities[0];
	for (i=0 ; i<level.num_entities ; i++, ent++) {
		if ( !ent->inuse ) {
			continue;
		}

		// clear events that are too old
		if ( level.time - ent->eventTime > EVENT_VALID_MSEC ) {
			if ( ent->s.event ) {
				ent->s.event = 0;	// &= EV_EVENT_BITS;
				if ( ent->client ) {
					ent->client->ps.externalEvent = 0;
					// predicted events should never be set to zero
					//ent->client->ps.events[0] = 0;
					//ent->client->ps.events[1] = 0;
				}
			}
			if ( ent->freeAfterEvent ) {
				// tempEntities or dropped items completely go away after their event
				G_FreeEntity( ent );
				continue;
			} else if ( ent->unlinkAfterEvent ) {
				// items that will respawn will hide themselves after their pickup event
				ent->unlinkAfterEvent = qfalse;
				trap_UnlinkEntity( ent );
			}
		}

		// temporary entities don't think
		if ( ent->freeAfterEvent ) {
			continue;
		}

		if ( !ent->r.linked && ent->neverFree ) {
			continue;
		}

		if ( ent->s.eType == ET_MISSILE ) {
			G_RunMissile( ent );
			continue;
		}

		if ( ent->s.eType == ET_ITEM || ent->physicsObject ) {
			G_RunItem( ent );
			continue;
		}

		if ( ent->s.eType == ET_MOVER ) {
			G_RunMover( ent );
			continue;
		}

		if ( i < MAX_CLIENTS ) {
			G_RunClient( ent );
			continue;
		}

		G_RunThink( ent );
	}

	// perform final fixups on the players
	ent = &g_entities[0];
	for (i=0 ; i < level.maxclients ; i++, ent++ ) {
		if ( ent->inuse ) {
			ClientEndFrame( ent );
		}
	}

	// see if it is time to do a tournement restart
	CheckTournament();

	// see if it is time to end the level
	CheckExitRules();

	// update to team status?
	CheckTeamStatus();

	// cancel vote if timed out
	CheckVote();

	// check team votes
	CheckTeamVote( TEAM_RED );
	CheckTeamVote( TEAM_BLUE );

	// for tracking changes
	CheckCvars();

#if FEAT_TEAM_AUTOBALANCE
	G_CheckTeamBalance();
#endif
#if FEAT_ELIMINATION
	G_CheckElimination();
#endif
#if FEAT_RANKED_QUEUE
	G_CheckRankedQueue();
#endif
#if FEAT_CRON_JOBS
	// cron jobs (11P): execute commands at specified intervals
	if ( numCronJobs > 0 ) {
		int elapsed = ( level.time - level.startTime ) / 1000; // seconds since map start
		if ( elapsed > cronLastCheck ) {
			int j;
			for ( j = 0; j < numCronJobs; j++ ) {
				if ( elapsed % cronJobs[j].intervalSec == 0 ) {
					trap_SendConsoleCommand( EXEC_APPEND, va( "%s\n", cronJobs[j].command ) );
				}
			}
			cronLastCheck = elapsed;
		}
	}
#endif

	if (g_listEntity.integer) {
		for (i = 0; i < MAX_GENTITIES; i++) {
			G_Printf("%4i: %s\n", i, g_entities[i].classname);
		}
		trap_Cvar_Set("g_listEntity", "0");
	}
}
