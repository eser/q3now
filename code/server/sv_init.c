/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.
Copyright (C) 2024 Wired engine contributors

This file is part of the Wired Engine (derived from idTech 3 & 4 source
code and community around it). It is free software released under the terms
of the GNU General Public License version 2 or (at your option) any later
version.

Quake III Arena, q3now, Wired Engine and the rest are licensed under the
**GNU General Public License, version 2 or later (GPL-2.0-or-later)**.
The full license text is in `LICENSE` and `THIRD_PARTY_LICENSES.md` at the
repository root.
===========================================================================
*/

#include "server.h"
#include "../qcommon/maps/bsp.h"
#include "../qcommon/maps/meta.h"
#include "../qcommon/wired/core/scripting/user_vm.h"
#include "../qcommon/q_feats.h"

#include "../qcommon/wired/net/wn_public.h"
/* Phase 5: log channels */
LOG_DECLARE_CHANNEL( ch_server, "server" );

// Active per-server map metadata. Points into maps_list[] (owned by
// Maps_Arena), so survives Hunk_ClearLevel and remains valid for the
// whole match. NULL between matches.
static const map_meta_t *sv_currentMeta = NULL;

#if FEAT_RECAST_NAVMESH
#include "../qcommon/nav/nav_public.h"
#endif


/*
===============
SV_SendConfigstring

Creates and sends the server command necessary to update the CS index for the
given client
===============
*/
static void SV_SendConfigstring(client_t *client, int index)
{
	int maxChunkSize = MAX_STRING_CHARS - 24;
	int len = strlen(sv.configstrings[index]);

	if( len >= maxChunkSize ) {
		int		sent = 0;
		int		remaining = len;
		const char	*cmd;
		char	buf[MAX_STRING_CHARS];

		while (remaining > 0 ) {
			if ( sent == 0 ) {
				cmd = "bcs0";
			}
			else if( remaining < maxChunkSize ) {
				cmd = "bcs2";
			}
			else {
				cmd = "bcs1";
			}
			Q_strncpyz( buf, &sv.configstrings[index][sent],
				maxChunkSize );

			SV_SendServerCommand( client, "%s %i \"%s\"", cmd,
				index, buf );

			sent += (maxChunkSize - 1);
			remaining -= (maxChunkSize - 1);
		}
	} else {
		// standard cs, just send it
		SV_SendServerCommand( client, "cs %i \"%s\"", index,
			sv.configstrings[index] );
	}
}

/*
===============
SV_UpdateConfigstrings

Called when a client goes from CS_PRIMED to CS_ACTIVE.  Updates all
Configstring indexes that have changed while the client was in CS_PRIMED
===============
*/
void SV_UpdateConfigstrings(client_t *client)
{
	for( int index = 0; index < MAX_CONFIGSTRINGS; index++ ) {
		// if the CS hasn't changed since we went to CS_PRIMED, ignore
		if(!client->csUpdated[index])
			continue;

		// do not always send server info to all clients
		if ( index == CS_SERVERINFO && ( SV_GentityNum( client - svs.clients )->r.svFlags & SVF_NOSERVERINFO ) ) {
			continue;
		}

		SV_SendConfigstring(client, index);
		client->csUpdated[index] = qfalse;
	}
}

/*
===============
SV_SetConfigstring

===============
*/
void SV_SetConfigstring (int index, const char *val) {
	if ( index < 0 || index >= MAX_CONFIGSTRINGS ) {
		Com_Terminate( TERM_CLIENT_DROP, "SV_SetConfigstring: bad index %i", index);
	}

	if ( !val ) {
		val = "";
	}

	// don't bother broadcasting an update if no change
	if ( !strcmp( val, sv.configstrings[ index ] ) ) {
		return;
	}

	// change the string in sv
	Z_Free( sv.configstrings[index] );
	sv.configstrings[index] = CopyString( val );

	// send it to all the clients if we aren't
	// spawning a new server
	if ( sv.state == SS_GAME || sv.restarting ) {

		// send the data to all relevant clients
		client_t *client = svs.clients;
		for (int i = 0; i < sv.maxclients; i++, client++) {
			if ( client->state < CS_ACTIVE ) {
				if ( client->state == CS_PRIMED || client->state == CS_CONNECTED ) {
					// track CS_CONNECTED clients as well to optimize gamestate acknowledge after downloading/retransmission
					client->csUpdated[index] = qtrue;
				}
				continue;
			}
			// do not always send server info to all clients
			if ( index == CS_SERVERINFO && ( SV_GentityNum( i )->r.svFlags & SVF_NOSERVERINFO ) ) {
				continue;
			}

			SV_SendConfigstring(client, index);
		}
	}
}


/*
===============
SV_GetConfigstring
===============
*/
void SV_GetConfigstring( int index, char *buffer, int bufferSize ) {
	if ( bufferSize < 1 ) {
		Com_Terminate( TERM_CLIENT_DROP, "SV_GetConfigstring: bufferSize == %i", bufferSize );
	}
	if ( index < 0 || index >= MAX_CONFIGSTRINGS ) {
		Com_Terminate( TERM_CLIENT_DROP, "SV_GetConfigstring: bad index %i", index);
	}
	if ( !sv.configstrings[index] ) {
		buffer[0] = '\0';
		return;
	}

	Q_strncpyz( buffer, sv.configstrings[index], bufferSize );
}


/*
===============
SV_SetUserinfo

===============
*/
void SV_SetUserinfo( int index, const char *val ) {
	if ( index < 0 || index >= sv.maxclients ) {
		Com_Terminate( TERM_CLIENT_DROP, "%s: bad index %i", __func__, index );
	}

	if ( !val ) {
		val = "";
	}

	Q_strncpyz( svs.clients[index].userinfo, val, sizeof( svs.clients[ index ].userinfo ) );
	Q_strncpyz( svs.clients[index].name, Info_ValueForKey( val, "name" ), sizeof(svs.clients[index].name) );
}



/*
===============
SV_GetUserinfo

===============
*/
void SV_GetUserinfo( int index, char *buffer, int bufferSize ) {
	if ( bufferSize < 1 ) {
		Com_Terminate( TERM_CLIENT_DROP, "%s: bufferSize == %i", __func__, bufferSize );
	}
	if ( index < 0 || index >= sv.maxclients ) {
		Com_Terminate( TERM_CLIENT_DROP, "%s: bad index %i", __func__, index );
	}
	Q_strncpyz( buffer, svs.clients[ index ].userinfo, bufferSize );
}


/*
================
SV_CreateBaseline

Entity baselines are used to compress non-delta messages
to the clients -- only the fields that differ from the
baseline will be transmitted
================
*/
static void SV_CreateBaseline( void ) {
	for ( int entnum = 0; entnum < sv.num_entities ; entnum++ ) {
		sharedEntity_t *ent = SV_GentityNum( entnum );
		if ( !ent->r.linked ) {
			continue;
		}
		ent->s.number = entnum;

		//
		// take current state as baseline
		//
		sv.svEntities[ entnum ].baseline = ent->s;
		sv.baselineUsed[ entnum ] = 1;
	}
}


/*
===============
SV_BoundMaxClients
===============
*/
static int SV_BoundMaxClients( int minimum ) {
	// get the current maxclients value
	Cvar_Get( "sv_maxclients", "8", 0 );

	if ( sv_maxclients->integer < minimum ) {
		Cvar_SetIntegerValue( "sv_maxclients", minimum );
		return minimum;
	}

	return sv_maxclients->integer;
}


/*
===============
SV_SetSnapshotParams
===============
*/
static void SV_SetSnapshotParams( void )
{
	// PACKET_BACKUP frames is just about 6.67MB so use that even on listen servers
	svs.numSnapshotEntities = PACKET_BACKUP * MAX_GENTITIES;
}


/*
===============
SV_AllocClients
===============
*/
static void SV_AllocClients( int count )
{
	svs.clients = Z_TagMalloc( count * sizeof( client_t ), TAG_CLIENTS );
	memset( svs.clients, 0x0, count * sizeof( client_t ) );
	sv.maxclients = count;
	SV_SetSnapshotParams();
}


/*
===============
SV_Startup

Called when a host starts a map when it wasn't running
one before.  Successive map or map_restart commands will
NOT cause this to be called, unless the game is exited to
the menu system first.
===============
*/
static void SV_Startup( void ) {
	if ( svs.initialized ) {
		Com_Terminate( TERM_UNRECOVERABLE, "SV_Startup: svs.initialized" );
	}

	SV_AllocClients( sv_maxclients->integer );

	svs.initialized = qtrue;

	// Don't respect sv_killserver unless a server is actually running
	if ( sv_killserver->integer ) {
		Cvar_Set( "sv_killserver", "0" );
	}

	Cvar_Set( "sv_running", "1" );

	// Join the ipv6 multicast group now that a map is running so clients can scan for us on the local network.
#if FEAT_IPV6
	NET_JoinMulticast6();
#endif

	SV_Lua_EnsureInit();
	WN_Init();
}


/*
==================
SV_ChangeMaxClients
==================
*/
static void SV_ChangeMaxClients( void ) {
	// get the highest client number in use
	int count = 0;
	for ( int i = 0; i < sv.maxclients; i++ ) {
		if ( svs.clients[i].state >= CS_CONNECTED ) {
			if ( i > count ) {
				count = i;
			}
		}
	}
	count++;

	// never go below the highest client number in use
	int maxclients = SV_BoundMaxClients( count );

	// if still the same
	if ( maxclients == sv.maxclients ) {
		return;
	}

	client_t *oldClients = Hunk_AllocateTempMemory( count * sizeof(client_t) );
	// copy the clients to hunk memory
	for ( int i = 0; i < count; i++ ) {
		if ( svs.clients[i].state >= CS_CONNECTED ) {
			oldClients[i] = svs.clients[i];
		} else {
			memset(&oldClients[i], 0, sizeof(client_t));
		}
	}

	// free old clients arrays
	Z_Free( svs.clients );

	// allocate new clients
	SV_AllocClients( maxclients );

	// copy the clients over
	for ( int i = 0; i < count; i++ ) {
		if ( oldClients[i].state >= CS_CONNECTED ) {
			svs.clients[i] = oldClients[i];
		}
	}

	// free the old clients on the hunk
	Hunk_FreeTempMemory( oldClients );
}


/*
================
SV_ClearServer
================
*/
static void SV_ClearServer( void ) {
	int i;

	for ( i = 0; i < MAX_CONFIGSTRINGS; i++ ) {
		if ( sv.configstrings[i] ) {
			Z_Free( sv.configstrings[i] );
		}
	}

	if ( !sv_levelTimeReset->integer ) {
		i = sv.time;
		memset( &sv, 0, sizeof( sv ) );
		sv.time = i;
	} else {
		memset( &sv, 0, sizeof( sv ) );
	}
}


/*
================
SV_SpawnServer

Change the server to a new map, taking all connected
clients along with it.
This is NOT called for map_restart
================
*/
/*
=================
SV_SpawnServer_Tick

Drive one phase of the async spawn state machine.  Called from Com_Frame
once per tick, before SV_Frame.  Returns immediately after completing one
phase so Com_Frame can run CL_Frame -> SCR_UpdateScreen between phases,
keeping the console live during map load.

Phase order:
  P1  Teardown + setup   (ShutdownGameProgs, CL_ShutdownLevel, Hunk_ClearLevel)
  P2  BSP load           (CM_LoadMap — single bounded hitch, acceptable)
  P3  Game VM init       (SV_InitGameProgs — single bounded hitch, acceptable)
  P4  Settle + baseline  (3x GAME_RUN_FRAME, SV_CreateBaseline)
  P5  Reconnect + final  (client reconnect loop, pak touch, SS_GAME)
=================
*/
void SV_SpawnServer_Tick( void ) {
	switch ( svs.spawn.phase ) {

	case SPAWN_IDLE:
		return;

	/* ---- Phase 1: Teardown & Setup ----------------------------------------- */
	case SPAWN_P1_TEARDOWN_SETUP:
	{
		const char *mapname = svs.spawn.mapname;

		SV_ShutdownGameProgs();

		Com_Log( SEV_INFO, LOG_CH(ch_server), "------ Server Initialization ------\n" );
		Com_Log( SEV_INFO, LOG_CH(ch_server), "Server: %s\n", mapname );

		Sys_SetStatus( "Initializing server..." );

#ifndef DEDICATED
		CL_MapLoading( mapname );
		CL_ShutdownLevel();
#endif

		Hunk_ClearLevel();
		CM_ClearMap();

		Cvar_CheckRange( com_timescale, "0.001", NULL, CV_FLOAT );

		if ( !Cvar_VariableIntegerValue( "sv_running" ) ) {
			SV_Startup();
		} else {
			{
				static int s_maxclients_mod = -1;
				if ( s_maxclients_mod == -1 ) {
					s_maxclients_mod = sv_maxclients->modificationCount;
				} else if ( sv_maxclients->modificationCount != s_maxclients_mod ) {
					s_maxclients_mod = sv_maxclients->modificationCount;
					SV_ChangeMaxClients();
				}
			}
		}

#ifndef DEDICATED
		FS_PureServerSetLoadedPaks( "", "" );
		FS_PureServerSetReferencedPaks( "", "" );
#endif

		FS_ClearPakReferences( 0 );

		svs.snapshotEntities = Hunk_Alloc( sizeof(entityState_t)*svs.numSnapshotEntities, h_high );
		SV_InitSnapshotStorage();

		svs.snapFlagServerBit ^= SNAPFLAG_SERVERCOUNT;

		Cvar_Set( "nextmap", "map_restart 0" );

		if ( !sv_levelTimeReset->integer && !sv.restartTime ) {
			int i;
			for ( i = 0; i < sv.maxclients; i++ ) {
				if ( svs.clients[i].state >= CS_CONNECTED ) {
					break;
				}
			}
			if ( i == sv.maxclients ) {
				sv.time = 0;
			}
		}

		for ( int i = 0; i < sv.maxclients; i++ ) {
			if ( svs.clients[i].state >= CS_CONNECTED && sv_levelTimeReset->integer ) {
				svs.clients[i].oldServerTime = sv.time;
			} else {
				svs.clients[i].oldServerTime = 0;
			}
		}

		int i = sv.maxclients;
		SV_ClearServer();
		sv.maxclients = i;
		for ( int i = 0; i < MAX_CONFIGSTRINGS; i++ ) {
			sv.configstrings[i] = CopyString("");
		}

		svs.spawn.phase = SPAWN_P2_BSP_LOAD;
		return;
	}

	/* ---- Phase 2: BSP Load ------------------------------------------------- */
	case SPAWN_P2_BSP_LOAD:
	{
		int checksum;
		const char *mapname = svs.spawn.mapname;

#ifndef DEDICATED
		Cvar_Set( "cl_paused", "0" );
#endif

		sv_pure = Cvar_Get( "sv_pure", "1", CVAR_SYSTEMINFO | CVAR_LATCH );
		sv.pure = sv_pure->integer;

		srand( Com_Milliseconds() );
		Com_RandomBytes( (byte*)&sv.checksumFeed, sizeof( sv.checksumFeed ) );
		FS_Restart( sv.checksumFeed );

		// Phase 5 (q3now meta): resolve per-map metadata BEFORE CM_LoadMap.
		// CM_LoadMap triggers RE_RegisterShader on the client, and the
		// renderer's R_FindShader fallback consults the active remap; the
		// remap pointer must be live before any shader resolution starts.
		// FS_Restart above may have just rescanned the map roster, so the
		// fast path is to look the map up in maps_list[]. If the map is
		// missing (e.g., custom map dropped in after the last scan),
		// Maps_AddOrRefresh is the targeted re-parse for a single name.
		sv_currentMeta = Maps_FindByName( mapname );
		if ( !sv_currentMeta ) {
			Maps_AddOrRefresh( mapname );
			sv_currentMeta = Maps_FindByName( mapname );
		}
		R_SetActiveRemapSet( sv_currentMeta ? &sv_currentMeta->remap : NULL );

		Sys_SetStatus( "Loading map %s", mapname );
		CM_LoadMap( va( "maps/%s.bsp", mapname ), qfalse, &checksum );
#if FEAT_RECAST_NAVMESH
		Nav_LoadMap( mapname );
#endif

		Cvar_Set( "mapname", mapname );
		Cvar_SetIntegerValue( "sv_mapChecksum", checksum );

		sv.serverId = com_frameTime;
		sv.restartedServerId = sv.serverId;
		Cvar_SetIntegerValue( "sv_serverid", sv.serverId );

		SV_ClearWorld();

		sv.state = SS_LOADING;

		svs.spawn.phase = SPAWN_P3_GAME_VM_INIT;
		return;
	}

	/* ---- Phase 3: Game VM Init --------------------------------------------- */
	case SPAWN_P3_GAME_VM_INIT:
	{
		SV_InitGameProgs();

		SV_SyncReloadTracker();

		svs.spawn.phase = SPAWN_P4_SETTLE_BASELINE;
		return;
	}

	/* ---- Phase 4: Settle + Baseline ---------------------------------------- */
	case SPAWN_P4_SETTLE_BASELINE:
	{
		for ( int i = 0; i < 3; i++ ) {
			Cbuf_Wait();
			sv.time += 100;
			VM_Call( gvm, 1, GAME_RUN_FRAME, sv.time );
			SV_BotFrame( sv.time );
		}

		SV_CreateBaseline();

		svs.spawn.phase = SPAWN_P5_RECONNECT_FINALIZE;
		return;
	}

	/* ---- Phase 5: Reconnect & Finalize ------------------------------------- */
	case SPAWN_P5_RECONNECT_FINALIZE:
	{
		qboolean killBots = svs.spawn.killBots;
		qboolean isBot;
		const char *p;

		for ( int i = 0; i < sv.maxclients; i++ ) {
			if ( svs.clients[i].state >= CS_CONNECTED ) {
				const char *denied;

				if ( svs.clients[i].netchan.remoteAddress.type == NA_BOT ) {
					if ( killBots ) {
						SV_DropClient( &svs.clients[i], "was kicked" );
						continue;
					}
					isBot = qtrue;
				} else {
					isBot = qfalse;
				}

				denied = GVM_ArgPtr( VM_Call( gvm, 3, GAME_CLIENT_CONNECT, i, qfalse, isBot ) ); // firstTime = qfalse
				if ( denied ) {
					SV_DropClient( &svs.clients[i], denied );
				} else {
					if ( !isBot ) {
						svs.clients[i].gamestateAck = GSA_INIT;
						svs.clients[i].state = CS_CONNECTED;
						svs.clients[i].gentity = NULL;
					} else {
						SV_ClientEnterWorld( &svs.clients[i] );
					}
				}
			}
		}

		Cbuf_Wait();
		sv.time += 100;
		VM_Call( gvm, 1, GAME_RUN_FRAME, sv.time );
		SV_BotFrame( sv.time );
		svs.time += 100;

		FS_TouchFileInPak( "vm/cgame.wasm" );

		p = FS_ReferencedPakNames();
		if ( FS_ExcludeReference() ) {
			FS_TouchFileInPak( "vm/cgame.wasm" );
			p = FS_ReferencedPakNames();
		}
		Cvar_Set( "sv_referencedPakNames", p );

		p = FS_ReferencedPakChecksums();
		Cvar_Set( "sv_referencedPaks", p );

		Cvar_Set( "sv_paks", "" );
		Cvar_Set( "sv_pakNames", "" );

		if ( sv.pure != 0 ) {
			int freespace, pakslen, infolen;
			qboolean overflowed = qfalse;
			qboolean infoTruncated = qfalse;

			p = FS_LoadedPakChecksums( &overflowed );

			pakslen = strlen( p ) + 9;
			freespace = SV_RemainingGameState();
			infolen = strlen( Cvar_InfoString_Big( CVAR_SYSTEMINFO, &infoTruncated ) );

			if ( infoTruncated ) {
				COM_WARN( LOG_CH(ch_server), "truncated systeminfo!\n" );
			}

			if ( pakslen > freespace || infolen + pakslen >= BIG_INFO_STRING || overflowed ) {
				Com_Log( SEV_DEBUG, LOG_CH(ch_server), S_COLOR_YELLOW "WARNING: skipping sv_paks setup to avoid gamestate overflow\n" );
			} else {
				Cvar_Set( "sv_paks", p );
				if ( *p == '\0' ) {
					COM_WARN( LOG_CH(ch_server), "sv_pure set but no PK3 files loaded\n" );
				}
			}
		}

		SV_SetConfigstring( CS_SYSTEMINFO, Cvar_InfoString_Big( CVAR_SYSTEMINFO, NULL ) );
		cvar_modifiedFlags &= ~CVAR_SYSTEMINFO;

		SV_SetConfigstring( CS_SERVERINFO, Cvar_InfoString( CVAR_SERVERINFO, NULL ) );
		cvar_modifiedFlags &= ~CVAR_SERVERINFO;

		{
			static int s_sv_cheats_mod = -1;
			if ( s_sv_cheats_mod == -1 ) {
				s_sv_cheats_mod = sv_cheats->modificationCount;
			} else if ( sv_cheats->modificationCount != s_sv_cheats_mod ) {
				s_sv_cheats_mod = sv_cheats->modificationCount;
				if ( !sv_cheats->integer ) {
					Cvar_CheatsWereDisabled();
				}
			}
		}

		sv.state = SS_GAME;

		SV_Heartbeat_f();

		Hunk_SetMark();

		Com_Log( SEV_INFO, LOG_CH(ch_server), "-----------------------------------\n" );

		Sys_SetStatus( "Running map %s", svs.spawn.mapname );

		Com_FrameInit();

		/* Spawn complete — return to normal frame processing */
		svs.spawn.phase = SPAWN_IDLE;
		return;
	}

	default:
		svs.spawn.phase = SPAWN_IDLE;
		return;
	}
}


/*
=================
SV_SpawnServer

Kickoff shim for the async spawn state machine.  Records mapname and
killBots into svs.spawn, sets phase to P1, and returns immediately.
The actual work is driven one phase per Com_Frame tick by SV_SpawnServer_Tick.
=================
*/
void SV_SpawnServer( const char *mapname, qboolean killBots ) {
	Q_strncpyz( svs.spawn.mapname, mapname, sizeof( svs.spawn.mapname ) );
	svs.spawn.killBots = killBots;
	svs.spawn.phase = SPAWN_P1_TEARDOWN_SETUP;
}


/* ──────────────────────────────────────────────────────────────────────────
 * Server cvar descriptor table — owned by this file.
 * Non-owner cvars (g_*, sv_master[], write-time PAK ROM cvars) remain on
 * Cvar_Get inside SV_Init.
 * "min only" legacy CheckRange cases (sv_maxclientsPerIP, sv_clientTLD,
 * sv_timeout, sv_zombietime, sv_padPackets, sv_killserver) are typed INT
 * with min=max=0 (integer type check, no range enforcement).
 * ────────────────────────────────────────────────────────────────────────── */
static const cvarDesc_t svDescs[] = {
	/* serverinfo */
	CVAR_STRING( "mapname",              "nomap",        CVAR_SERVERINFO | CVAR_ROM,
	             "Display the name of the current map being used on a server." ),
	CVAR_INT(    "sv_privateClients",    "0",            CVAR_SERVERINFO,
	             "The number of spots, out of sv_maxclients, reserved for players with the server password (sv_privatePassword).",
	             0, MAX_CLIENTS - 1 ),
	CVAR_STRING( "sv_hostname",          "noname",       CVAR_SERVERINFO | CVAR_ARCHIVE,
	             "Sets the name of the server." ),
	CVAR_INT(    "sv_maxclients",        "8",            CVAR_SERVERINFO | CVAR_LATCH,
	             "Maximum number of people allowed to join the server.",
	             1, MAX_CLIENTS ),
	CVAR_INT(    "sv_maxclientsPerIP",   "3",            CVAR_ARCHIVE,
	             "Limits number of simultaneous connections from the same IP address.",
	             0, 0 ),
	CVAR_INT(    "sv_clientTLD",         "0",            CVAR_ARCHIVE | CVAR_NODEFAULT,
	             "Client country detection code.",
	             0, 0 ),
	CVAR_INT(    "sv_minRate",           "0",            CVAR_ARCHIVE | CVAR_NODEFAULT | CVAR_SERVERINFO,
	             "Minimum server bandwidth (in bit per second) a client can use.",
	             0, 0 ),
	CVAR_INT(    "sv_maxRate",           "0",            CVAR_ARCHIVE | CVAR_NODEFAULT | CVAR_SERVERINFO,
	             "Maximum server bandwidth (in bit per second) a client can use.",
	             0, 0 ),
	CVAR_INT(    "sv_dlRate",            "100",          CVAR_ARCHIVE | CVAR_SERVERINFO,
	             "Bandwidth allotted to PK3 file downloads via UDP, in kbyte/s.",
	             0, 500 ),
	CVAR_BOOL(   "sv_floodProtect",      "1",            CVAR_ARCHIVE | CVAR_SERVERINFO,
	             "Toggle server flood protection to keep players from bringing the server down." ),
	/* systeminfo */
	CVAR_INT(    "sv_serverid",          "0",            CVAR_SYSTEMINFO | CVAR_ROM,
	             NULL, 0, 0 ),
	CVAR_BOOL(   "sv_pure",              "1",            CVAR_SYSTEMINFO | CVAR_LATCH,
	             "Requires clients to only get data from pk3 files the server is using." ),
	CVAR_BOOL(   "sv_cheats",            "0",            CVAR_SYSTEMINFO | CVAR_LATCH,
	             "Cheats!" ),
	CVAR_STRING( "sv_referencedPakNames","",             CVAR_SYSTEMINFO | CVAR_ROM,
	             "Variable holds a list of all the pk3 files the server loaded data from." ),
	/* server vars */
	CVAR_STRING( "sv_privatePassword",   "",             CVAR_TEMP,
	             "Set password for private clients to login with." ),
	CVAR_INT(    "sv_fps",               "20",           CVAR_TEMP,
	             "Set the max frames per second the server sends the client.",
	             10, 250 ),
	CVAR_BOOL(   "sv_snapshotTransport", "0",            CVAR_ARCHIVE,
	             "Snapshot delivery mode: 0=datagram fragmentation (unreliable, Q3 semantics), 1=reliable stream for oversize snapshots." ),
	CVAR_INT(    "sv_timeout",           "200",          CVAR_TEMP,
	             "Seconds without any message before automatic client disconnect.",
	             0, 0 ),
	CVAR_INT(    "sv_zombietime",        "2",            CVAR_TEMP,
	             "Seconds to sink messages after disconnect.",
	             0, 0 ),
	CVAR_BOOL(   "sv_allowDownload",     "1",            CVAR_SERVERINFO,
	             "Toggle the ability for clients to download files maps etc. from server." ),
	CVAR_INT(    "sv_reconnectlimit",    "3",            0,
	             "Number of seconds a disconnected client should wait before next reconnect.",
	             0, 12 ),
	CVAR_INT(    "sv_padPackets",        "0",            CVAR_CHEAT,
	             "Adds padding bytes to network packets for rate debugging.",
	             0, 0 ),
	CVAR_INT(    "sv_killserver",        "0",            0,
	             "Internal flag to manage server state.",
	             0, 0 ),
	CVAR_STRING( "sv_mapChecksum",       "",             CVAR_ROM,
	             "Allows check for client server map to match." ),
	CVAR_BOOL(   "sv_lanForceRate",      "1",            CVAR_ARCHIVE | CVAR_NODEFAULT,
	             "Forces LAN clients to the maximum rate instead of accepting client setting." ),
	CVAR_BOOL(   "sv_levelTimeReset",    "0",            CVAR_ARCHIVE | CVAR_NODEFAULT,
	             "Whether or not to reset leveltime after new map loads." ),
	CVAR_INT(    "sv_minRestartDelay",   "2",            CVAR_ARCHIVE | CVAR_NODEFAULT,
	             "Schedule an automatic server process restart after this many hours of uptime.\n"
	             "The actual restart waits for all human clients to disconnect before firing.\n"
	             "Range 2-48 hours, default 2.",
	             2, 48 ),
	CVAR_STRING( "sv_filter",            "filter.txt",   CVAR_ARCHIVE,
	             "Cvar that point on filter file, if it is \"\" then filtering will be disabled." ),
#ifdef USE_BANS
	CVAR_STRING( "sv_banFile",           "serverbans.dat", CVAR_ARCHIVE,
	             "Name of the file that is used for storing the server bans." ),
#endif
};

enum {
	SV_MAPNAME,
	SV_PRIVATECLIENTS,
	SV_HOSTNAME,
	SV_MAXCLIENTS,
	SV_MAXCLIENTS_PER_IP,
	SV_CLIENT_TLD,
	SV_MIN_RATE,
	SV_MAX_RATE,
	SV_DL_RATE,
	SV_FLOOD_PROTECT,
	SV_SERVERID,
	SV_PURE,
	SV_CHEATS,
	SV_REFERENCED_PAK_NAMES,
	SV_PRIVATE_PASSWORD,
	SV_FPS,
	SV_SNAPSHOT_TRANSPORT,
	SV_TIMEOUT,
	SV_ZOMBIETIME,
	SV_ALLOW_DOWNLOAD,
	SV_RECONNECT_LIMIT,
	SV_PAD_PACKETS,
	SV_KILLSERVER,
	SV_MAP_CHECKSUM,
	SV_LAN_FORCE_RATE,
	SV_LEVEL_TIME_RESET,
	SV_MIN_RESTART_DELAY,
	SV_FILTER,
#ifdef USE_BANS
	SV_BAN_FILE,
#endif
	SV_CVAR_COUNT
};

_Static_assert( ARRAY_LEN( svDescs ) == SV_CVAR_COUNT, "svDescs/enum mismatch" );

static cvar_t *svHandles[SV_CVAR_COUNT];


/*
===============
SV_Init

Only called at main exe startup, not for each game
===============
*/
void SV_Init( void )
{
	SV_AddOperatorCommands();
	SV_BotAwareness_Init();

	if ( com_dedicated->integer )
		SV_AddDedicatedCommands();

	// serverinfo vars — game-owned; server seeds them with serverinfo flags
	Cvar_Get( "g_noFootsteps", "0",  CVAR_SERVERINFO );
	Cvar_Get( "g_scorelimit",  "20", CVAR_SERVERINFO );
	Cvar_Get( "g_timelimit",   "0",  CVAR_SERVERINFO );
	{
		static const cvarDesc_t d = CVAR_INT( "g_gametype", "0", CVAR_SERVERINFO | CVAR_LATCH,
			"Set the gametype to mod.", 0, 0 );
		sv_gametype = Cvar_Register( &d );
	}
	Cvar_Get( "sv_keywords", "", CVAR_SERVERINFO );
	//Cvar_Get ("protocol", va("%i", PROTOCOL_VERSION), CVAR_SERVERINFO | CVAR_ROM);

	// ROM pak-list cvars — written at runtime by the filesystem
	Cvar_Get( "sv_paks",           "", CVAR_SYSTEMINFO | CVAR_ROM );
	Cvar_Get( "sv_pakNames",       "", CVAR_SYSTEMINFO | CVAR_ROM );
	Cvar_Get( "sv_referencedPaks", "", CVAR_SYSTEMINFO | CVAR_ROM );

	// master servers — dynamic names, cannot be in a static descriptor table
	for ( int index = 0; index < MAX_MASTER_SERVERS; index++ )
		sv_master[ index ] = Cvar_Get( va( "sv_master%d", index + 1 ), "", CVAR_ARCHIVE | CVAR_NODEFAULT );

	// per-game identity — populated by the game module from bg_public.h's
	// GAMENAME_FOR_MASTER / HEARTBEAT_FOR_MASTER during G_InitGame. Engine reads
	// the cvars and never includes the game-side header. Heartbeat is skipped
	// while either cvar is empty (i.e., before game init completes).
	Cvar_Get( "sv_gamename",  "", CVAR_SERVERINFO | CVAR_ROM );
	Cvar_Get( "sv_heartbeat", "", CVAR_ROM );

	// transient cvars with no stored handle
	Cvar_Get( "nextmap",  "", CVAR_TEMP );
	Cvar_Get( "sv_dlURL", "", CVAR_SERVERINFO | CVAR_ARCHIVE );

	// owned cvars — registered via typed descriptor table
	Cvar_RegisterTable( svDescs, SV_CVAR_COUNT, svHandles );
	sv_mapname            = svHandles[SV_MAPNAME];
	sv_privateClients     = svHandles[SV_PRIVATECLIENTS];
	sv_hostname           = svHandles[SV_HOSTNAME];
	sv_maxclients         = svHandles[SV_MAXCLIENTS];
	sv_maxclientsPerIP    = svHandles[SV_MAXCLIENTS_PER_IP];
	sv_clientTLD          = svHandles[SV_CLIENT_TLD];
	sv_minRate            = svHandles[SV_MIN_RATE];
	sv_maxRate            = svHandles[SV_MAX_RATE];
	sv_dlRate             = svHandles[SV_DL_RATE];
	sv_floodProtect       = svHandles[SV_FLOOD_PROTECT];
	sv_serverid           = svHandles[SV_SERVERID];
	sv_pure               = svHandles[SV_PURE];
	sv_cheats             = svHandles[SV_CHEATS];
	sv_referencedPakNames = svHandles[SV_REFERENCED_PAK_NAMES];
	sv_privatePassword    = svHandles[SV_PRIVATE_PASSWORD];
	sv_fps                = svHandles[SV_FPS];
	sv_snapshotTransport  = svHandles[SV_SNAPSHOT_TRANSPORT];
	sv_timeout            = svHandles[SV_TIMEOUT];
	sv_zombietime         = svHandles[SV_ZOMBIETIME];
	sv_allowDownload      = svHandles[SV_ALLOW_DOWNLOAD];
	sv_reconnectlimit     = svHandles[SV_RECONNECT_LIMIT];
	sv_padPackets         = svHandles[SV_PAD_PACKETS];
	sv_killserver         = svHandles[SV_KILLSERVER];
	sv_mapChecksum        = svHandles[SV_MAP_CHECKSUM];
	sv_lanForceRate       = svHandles[SV_LAN_FORCE_RATE];
	sv_levelTimeReset     = svHandles[SV_LEVEL_TIME_RESET];
	sv_minRestartDelay    = svHandles[SV_MIN_RESTART_DELAY];
	sv_filter             = svHandles[SV_FILTER];
#ifdef USE_BANS
	sv_banFile            = svHandles[SV_BAN_FILE];
#endif

	// Record server start time for the scheduled-restart logic.
	if ( sv_startRealTime == 0 ) {
		sv_startRealTime = Sys_Milliseconds();
	}
	sv_restartPending = qfalse;

	// initialize bot cvars so they are listed and can be set before loading the botlib
	SV_BotInitCvars();

	// init the botlib here because we need the pre-compiler in the UI
	SV_BotInitBotLib();

#ifdef USE_BANS
	// Load saved bans
	Cbuf_AddText("rehashbans\n");
#endif

	// track group cvar changes
	Cvar_SetGroup( sv_lanForceRate, CVG_SERVER );
	Cvar_SetGroup( sv_minRate, CVG_SERVER );
	Cvar_SetGroup( sv_maxRate, CVG_SERVER );
	Cvar_SetGroup( sv_fps, CVG_SERVER );

	// force initial check
	SV_TrackCvarChanges();

	memset( svs.rconSessions, 0, sizeof( svs.rconSessions ) );
	UserVM_Init();
	SV_RconLua_Init();
	SV_Lua_Init();

	WN_Init();
	WN_RegisterCommands();

	/* Wire the accept callback now that transport is published and SV_OnPlayerConnect
	 * is defined.  WN_DrainPendingConnects() checks this pointer every frame. */
	if ( transport ) {
		transport->accept_callback = SV_OnPlayerConnect;
		transport->ready_callback  = SV_OnPlayerReady;
		transport->drain_usercmds  = SV_DrainUsercmds_Impl;
	}

#if FEAT_RECAST_NAVMESH
	Nav_Init();
#endif
}


/*
==================
SV_FinalMessage

Used by SV_Shutdown to send a final message to all
connected clients before the server goes down.  The messages are sent immediately,
not just stuck on the outgoing message list, because the server is going
to totally exit after returning from this function.
==================
*/
static void SV_FinalMessage( const char *message ) {
	// send it twice, ignoring rate
	for ( int j = 0 ; j < 2 ; j++ ) {
		client_t *cl = svs.clients;
		for ( int i = 0; i < sv.maxclients; i++, cl++) {
			if (cl->state >= CS_CONNECTED ) {
				// don't send a disconnect to a local client
				if ( cl->netchan.remoteAddress.type != NA_LOOPBACK ) {
					SV_SendServerCommand( cl, "print \"%s\n\"\n", message );
					SV_SendServerCommand( cl, "disconnect \"%s\"", message );
				}
				// force a snapshot to be sent
				cl->lastSnapshotTime = svs.time - 9999; // generate a snapshot immediately
				cl->state = CS_ZOMBIE; // skip delta generation
				SV_SendClientSnapshot( cl );
			}
		}
	}

	NET_FlushPacketQueue( 99999 );
}


/*
================
SV_Shutdown

Called when each game quits,
before Sys_Quit or Sys_Error
================
*/
void SV_Shutdown( const char *finalmsg ) {
	if ( !com_sv_running || !com_sv_running->integer ) {
		return;
	}

	Com_Log( SEV_INFO, LOG_CH(ch_server), "----- Server Shutdown (%s) -----\n", finalmsg );

	WN_Shutdown();

#if FEAT_IPV6
	NET_LeaveMulticast6();
#endif

	if ( svs.clients && !com_errorEntered ) {
		SV_FinalMessage( finalmsg );
	}

	SV_RemoveOperatorCommands();
	SV_MasterShutdown();
	SV_ShutdownGameProgs();
	SV_Lua_Shutdown();
	SV_RconLua_Shutdown();
	UserVM_Shutdown();
#if FEAT_RECAST_NAVMESH
	Nav_Shutdown();
#endif

	// Clear the active remap set; the tables themselves live in
	// Maps_Arena and get reclaimed on the next FS_Restart / shutdown.
	R_SetActiveRemapSet( NULL );
	sv_currentMeta = NULL;

	// free current level
	SV_ClearServer();

	SV_FreeIP4DB();

	// free server static data
	if ( svs.clients ) {
		for ( int index = 0; index < sv.maxclients; index++ )
			SV_FreeClient( &svs.clients[ index ] );

		Z_Free( svs.clients );
	}
	memset( &svs, 0, sizeof( svs ) );
	sv.time = 0;

	Cvar_Set( "sv_running", "0" );

	// allow setting timescale 0 for demo playback
	Cvar_CheckRange( com_timescale, "0", NULL, CV_FLOAT );

#ifndef DEDICATED
	Cvar_Set( "ui_singlePlayerActive", "0" );
#endif

	Com_Log( SEV_INFO, LOG_CH(ch_server), "---------------------------\n" );

#ifndef DEDICATED
	// disconnect any local clients
	if ( sv_killserver->integer != 2 )
		CL_Disconnect( qfalse );
#endif

	// clean some server cvars
	Cvar_Set( "sv_referencedPaks", "" );
	Cvar_Set( "sv_referencedPakNames", "" );
	Cvar_Set( "sv_mapChecksum", "" );
	Cvar_Set( "sv_serverid", "0" );

	BSP_ClearMapCache();

	Sys_SetStatus( "Server is not running" );
}
