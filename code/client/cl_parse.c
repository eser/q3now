// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 1999-2005 Id Software, Inc.
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
// cl_parse.c  -- parse a message received from the server

#include "client.h"
#include "../qcommon/wired/net/wn_public.h"
LOG_DECLARE_CHANNEL( ch_client, "client" );
LOG_DECLARE_CHANNEL( ch_network_client, "network.client" );

static const char *svc_strings[] = {
	"svc_bad",
	"svc_nop",
	"svc_gamestate",
	"svc_configstring",
	"svc_baseline",
	"svc_serverCommand",
	"svc_download",
	"svc_snapshot",
	"svc_EOF",
	"svc_voipSpeex", // ioq3 extension
	"svc_voipOpus",  // ioq3 extension
};

static void SHOWNET( msg_t *msg, const char *s ) {
	if ( cl_shownet->integer >= 2) {
		Com_Log( SEV_INFO, LOG_CH(ch_client), "%3i:%s\n", msg->readcount-1, s);
	}
}


/*
=========================================================================

MESSAGE PARSING

=========================================================================
*/

/*
==================
CL_DeltaEntity

Parses deltas from the given base and adds the resulting entity
to the current frame
==================
*/
static void CL_DeltaEntity( clientApp_t *app, msg_t *msg, clSnapshot_t *frame, int newnum, const entityState_t *old, qboolean unchanged) {
	entityState_t	*state;

	// save the parsed entity state into the big circular buffer so
	// it can be used as the source for a later delta
	state = &app->cl.parseEntities[app->cl.parseEntitiesNum & (MAX_PARSE_ENTITIES-1)];

	if ( unchanged ) {
		// NOLINTNEXTLINE(clang-analyzer-core.NullDereference) — caller contract: unchanged=qtrue implies old is non-NULL (no prior frame ⇒ no "unchanged")
		*state = *old;
	} else {
		MSG_ReadDeltaEntity( msg, old, state, newnum );
	}

	if ( state->number == (MAX_GENTITIES-1) ) {
		return;		// entity was delta removed
	}
	app->cl.parseEntitiesNum++;
	frame->numEntities++;
}


/*
==================
CL_ParsePacketEntities
==================
*/
static void CL_ParsePacketEntities( clientApp_t *app, msg_t *msg, const clSnapshot_t *oldframe, clSnapshot_t *newframe ) {
	newframe->parseEntitiesNum = app->cl.parseEntitiesNum;
	newframe->numEntities = 0;

	// delta from the entities present in oldframe
	int	oldindex = 0;
	int	oldnum;
	const entityState_t	*oldstate = NULL;
	if ( !oldframe ) {
		oldnum = MAX_GENTITIES+1;
	} else {
		if ( oldindex >= oldframe->numEntities ) {
			oldnum = MAX_GENTITIES+1;
		} else {
			oldstate = &app->cl.parseEntities[
				(oldframe->parseEntitiesNum + oldindex) & (MAX_PARSE_ENTITIES-1)];
			oldnum = oldstate->number;
		}
	}

	while ( 1 ) {
		// read the entity index number
		int	newnum = MSG_ReadEntitynum( msg );

		if ( newnum < 0 ) {
			Com_Terminate( TERM_CLIENT_DROP, "CL_ParsePacketEntities: end of message" );
		}

		if ( newnum == (MAX_GENTITIES-1) ) {
			break;
		}

		while ( oldnum < newnum ) {
			// one or more entities from the old packet are unchanged
			if ( cl_shownet->integer == 3 ) {
				Com_Log( SEV_INFO, LOG_CH(ch_client), "%3i:  unchanged: %i\n", msg->readcount, oldnum);
			}
			CL_DeltaEntity( app, msg, newframe, oldnum, oldstate, qtrue );

			oldindex++;

			// NOLINTNEXTLINE(clang-analyzer-core.NullDereference) — packet-entity parser invariant: when oldnum != MAX_GENTITIES+1 (sentinel), oldframe is non-NULL
			if ( oldindex >= oldframe->numEntities ) {
				oldnum = MAX_GENTITIES+1;
			} else {
				oldstate = &app->cl.parseEntities[
					(oldframe->parseEntitiesNum + oldindex) & (MAX_PARSE_ENTITIES-1)];
				oldnum = oldstate->number;
			}
		}
		if (oldnum == newnum) {
			// delta from previous state
			if ( cl_shownet->integer == 3 ) {
				Com_Log( SEV_INFO, LOG_CH(ch_client), "%3i:  delta: %i\n", msg->readcount, newnum);
			}
			CL_DeltaEntity( app, msg, newframe, newnum, oldstate, qfalse );

			oldindex++;

			// NOLINTNEXTLINE(clang-analyzer-core.NullDereference) — packet-entity parser invariant: when oldnum != MAX_GENTITIES+1 (sentinel), oldframe is non-NULL
			if ( oldindex >= oldframe->numEntities ) {
				oldnum = MAX_GENTITIES+1;
			} else {
				oldstate = &app->cl.parseEntities[
					(oldframe->parseEntitiesNum + oldindex) & (MAX_PARSE_ENTITIES-1)];
				oldnum = oldstate->number;
			}
			continue;
		}

		if ( oldnum > newnum ) {
			// delta from baseline
			if ( cl_shownet->integer == 3 ) {
				Com_Log( SEV_INFO, LOG_CH(ch_client), "%3i:  baseline: %i\n", msg->readcount, newnum);
			}
			CL_DeltaEntity( app, msg, newframe, newnum, &app->cl.entityBaselines[newnum], qfalse );
			continue;
		}

	}

	// any remaining entities in the old frame are copied over
	while ( oldnum != MAX_GENTITIES+1 ) {
		// one or more entities from the old packet are unchanged
		if ( cl_shownet->integer == 3 ) {
			Com_Log( SEV_INFO, LOG_CH(ch_client), "%3i:  unchanged: %i\n", msg->readcount, oldnum);
		}
		CL_DeltaEntity( app, msg, newframe, oldnum, oldstate, qtrue );

		oldindex++;

		if ( oldindex >= oldframe->numEntities ) {
			oldnum = MAX_GENTITIES+1;
		} else {
			oldstate = &app->cl.parseEntities[
				(oldframe->parseEntitiesNum + oldindex) & (MAX_PARSE_ENTITIES-1)];
			oldnum = oldstate->number;
		}
	}

}


/*
================
CL_ParseSnapshot

If the snapshot is parsed properly, it will be copied to
app->cl.snap and saved in app->cl.snapshots[].  If the snapshot is invalid
for any reason, no changes to the state will be made at all.
================
*/
static void CL_ParseSnapshot( clientApp_t *app, msg_t *msg ) {
	const clSnapshot_t *old;
	clSnapshot_t	newSnap;

	// get the reliable sequence acknowledge number
	// NOTE: now sent with all server to client messages
	//clc.reliableAcknowledge = MSG_ReadLong( msg );

	// read in the new snapshot to a temporary buffer
	// we will only copy to cl.snap if it is valid
	memset (&newSnap, 0, sizeof(newSnap));

	// we will have read any new server commands in this
	// message before we got to svc_snapshot
	newSnap.serverCommandNum = app->clc.serverCommandSequence;

	newSnap.serverTime = MSG_ReadLong( msg );

	newSnap.messageNum = app->clc.serverMessageSequence;

	int			deltaNum = MSG_ReadByte( msg );
	if ( !deltaNum ) {
		newSnap.deltaNum = -1;
	} else {
		newSnap.deltaNum = newSnap.messageNum - deltaNum;
	}
	newSnap.snapFlags = MSG_ReadByte( msg );

	// If the frame is delta compressed from data that we
	// no longer have available, we must suck up the rest of
	// the frame, but not use it, then ask for a non-compressed
	// message
	if ( newSnap.deltaNum <= 0 ) {
		newSnap.valid = qtrue;		// uncompressed frame
		old = NULL;
		app->clc.demowaiting = qfalse;	// we can start recording now
	} else {
		old = &app->cl.snapshots[newSnap.deltaNum & PACKET_MASK];
		if ( !old->valid ) {
			// should never happen
			Com_Log( SEV_INFO, LOG_CH(ch_client), "Delta from invalid frame (not supposed to happen!).\n");
		} else if ( old->messageNum != newSnap.deltaNum ) {
			// The frame that the server did the delta from
			// is too old, so we can't reconstruct it properly.
			Com_Log( SEV_INFO, LOG_CH(ch_client), "Delta frame too old.\n");
		} else if ( app->cl.parseEntitiesNum - old->parseEntitiesNum > MAX_PARSE_ENTITIES - MAX_SNAPSHOT_ENTITIES ) {
			Com_Log( SEV_INFO, LOG_CH(ch_client), "Delta parseEntitiesNum too old.\n");
		} else {
			newSnap.valid = qtrue;	// valid delta parse
		}
	}

	// read areamask
	newSnap.areabytes = MSG_ReadByte( msg );

	if ( newSnap.areabytes > sizeof(newSnap.areamask) )
	{
		Com_Terminate( TERM_CLIENT_DROP,"CL_ParseSnapshot: Invalid size %d for areamask", newSnap.areabytes );
		return;
	}

	MSG_ReadData( msg, &newSnap.areamask, newSnap.areabytes );

	// read playerinfo
	SHOWNET( msg, "playerstate" );
	if ( old ) {
		MSG_ReadDeltaPlayerstate( msg, &old->ps, &newSnap.ps );
	} else {
		MSG_ReadDeltaPlayerstate( msg, NULL, &newSnap.ps );
	}

	// read packet entities
	SHOWNET( msg, "packet entities" );
	CL_ParsePacketEntities( app, msg, old, &newSnap );

	// if not valid, dump the entire thing now that it has
	// been properly read
	if ( !newSnap.valid ) {
		return;
	}

	// clear the valid flags of any snapshots between the last
	// received and this one, so if there was a dropped packet
	// it won't look like something valid to delta from next
	// time we wrap around in the buffer
	int			oldMessageNum = app->cl.snap.messageNum + 1;

	if ( newSnap.messageNum - oldMessageNum >= PACKET_BACKUP ) {
		oldMessageNum = newSnap.messageNum - ( PACKET_BACKUP - 1 );
	}

	for ( int i = 0, n = newSnap.messageNum - oldMessageNum; i < n; i++ ) {
		app->cl.snapshots[ ( oldMessageNum + i ) & PACKET_MASK ].valid = qfalse;
	}

	// copy to the current good spot
	app->cl.snap = newSnap;
	app->cl.snap.ping = 999;
	// calculate ping time
	for ( int i = 0 ; i < PACKET_BACKUP ; i++ ) {
		int packetNum = ( app->clc.netchan.outgoingSequence - 1 - i ) & PACKET_MASK;
		if ( app->cl.snap.ps.commandTime - app->cl.outPackets[packetNum].p_serverTime >= 0 ) {
			app->cl.snap.ping = cls.realtime - app->cl.outPackets[ packetNum ].p_realtime;
			break;
		}
	}
	// save the frame off in the backup array for later delta comparisons
	app->cl.snapshots[app->cl.snap.messageNum & PACKET_MASK] = app->cl.snap;

	if (cl_shownet->integer == 3) {
		Com_Log( SEV_INFO, LOG_CH(ch_client), "   snapshot:%i  delta:%i  ping:%i\n", app->cl.snap.messageNum,
		app->cl.snap.deltaNum, app->cl.snap.ping );
	}

	app->cl.newSnapshots = qtrue;

	app->clc.eventMask |= EM_SNAPSHOT;
	CL_RecordCommittedSnapshot( app );
}


//=====================================================================

int cl_connectedToPureServer;
int cl_connectedToCheatServer;

/*
==================
CL_SystemInfoChanged

The systeminfo configstring has been changed, so parse
new information out of it.  This will happen at every
gamestate, and possibly during gameplay.
==================
*/
void CL_SystemInfoChanged( clientApp_t *app, qboolean onlyGame ) {
	const char		*systemInfo;
	const char		*s, *t;
	char			key[BIG_INFO_KEY];
	char			value[BIG_INFO_VALUE];

	/* The process-global filesystem + pure state (cl_connectedToPureServer,
	 * fs_game, FS_PureServer*, the systeminfo→cvar mirror, cheat flag) is owned by
	 * the input-focused client. An additional in-process client shares the same
	 * process filesystem, which the focused client already configured, so it must
	 * read its own serverId/demo state but NEVER drive these globals. */
	qboolean drivesGlobalState = ( app == clientActiveApp );

	systemInfo = app->cl.gameState.stringData + app->cl.gameState.stringOffsets[ CS_SYSTEMINFO ];
	// NOTE TTimo:
	// when the serverId changes, any further messages we send to the server will use this new serverId
	// https://zerowing.idsoftware.com/bugzilla/show_bug.cgi?id=475
	// in some cases, outdated cp commands might get sent with this news serverId
	app->cl.serverId = atoi( Info_ValueForKey( systemInfo, "sv_serverid" ) );

	// don't set any vars when playing a demo
	if ( app->clc.demoplaying ) {
		return;
	}

	// An additional in-process client does not touch the shared fs/pure globals.
	if ( !drivesGlobalState ) {
		return;
	}

	s = Info_ValueForKey( systemInfo, "sv_pure" );
	cl_connectedToPureServer = atoi( s );

	// parse/update fs_game in first place
	s = Info_ValueForKey( systemInfo, "fs_game" );

	if ( FS_InvalidGameDir( s ) ) {
		COM_WARN( LOG_CH(ch_client), "Server sent invalid fs_game value %s\n", s );
	} else {
		Cvar_Set( "fs_game", s );
	}

	// if game folder should not be set and it is set at the client side
	if ( *s == '\0' && *Cvar_VariableString( "fs_game" ) != '\0' ) {
		Cvar_Set( "fs_game", "" );
	}

	if ( onlyGame && Cvar_Flags( "fs_game" ) & CVAR_MODIFIED ) {
		// game directory change is needed
		// return early to avoid systeminfo-cvar pollution in current fs_game
		return;
	}

	s = Info_ValueForKey( systemInfo, "sv_cheats" );
	cl_connectedToCheatServer = atoi( s );
	if ( !cl_connectedToCheatServer ) {
		Cvar_CheatsWereDisabled();
	}

	if ( com_sv_running->integer ) {
		// no filesystem restrictions for localhost
		FS_PureServerSetLoadedPaks( "", "" );
		FS_PureServerSetReferencedPaks( "", "" );
	} else {
		// check pure server string
		s = Info_ValueForKey( systemInfo, "sv_paks" );
		t = Info_ValueForKey( systemInfo, "sv_pakNames" );
		FS_PureServerSetLoadedPaks( s, t );

		s = Info_ValueForKey( systemInfo, "sv_referencedPaks" );
		t = Info_ValueForKey( systemInfo, "sv_referencedPakNames" );
		FS_PureServerSetReferencedPaks( s, t );
	}

	// scan through all the variables in the systeminfo and locally set cvars to match
	s = systemInfo;
	do {
		int cvar_flags;

		s = Info_NextPair( s, key, value );
		if ( key[0] == '\0' ) {
			break;
		}

		// we don't really need any of these server cvars to be set on client-side
		if ( !Q_stricmp( key, "sv_pure" ) || !Q_stricmp( key, "sv_serverid" ) || !Q_stricmp( key, "sv_fps" ) ) {
			continue;
		}
		if ( !Q_stricmp( key, "sv_paks" ) || !Q_stricmp( key, "sv_pakNames" ) ) {
			continue;
		}
		if ( !Q_stricmp( key, "sv_referencedPaks" ) || !Q_stricmp( key, "sv_referencedPakNames" ) ) {
			continue;
		}

		if ( !Q_stricmp( key, "fs_game" ) ) {
			continue; // already processed
		}

		if ( ( cvar_flags = Cvar_Flags( key ) ) == CVAR_NONEXISTENT )
			Cvar_Get( key, value, CVAR_SERVER_CREATED | CVAR_ROM );
		else
		{
			// If this cvar may not be modified by a server discard the value.
			if ( !(cvar_flags & ( CVAR_SYSTEMINFO | CVAR_SERVER_CREATED | CVAR_USER_CREATED ) ) )
			{
#ifndef STANDALONE
				if ( Q_stricmp( key, "g_synchronousClients" ) && Q_stricmp( key, "pmove_fixed" ) && Q_stricmp( key, "pmove_msec" ) )
#endif
				{
					COM_WARN( LOG_CH(ch_client), "server is not allowed to set %s=%s\n", key, value );
					continue;
				}
			}

			Cvar_SetSafe( key, value );
		}
	}
	while ( *s != '\0' );
}


/*
==================
CL_GameSwitch
==================
*/
qboolean CL_GameSwitch( clientApp_t *app )
{
	return (app->gameSwitch && !com_errorEntered);
}


/*
==================
CL_ParseServerInfo
==================
*/
static void CL_ParseServerInfo( clientApp_t *app )
{
	const char *serverInfo = app->cl.gameState.stringData
		+ app->cl.gameState.stringOffsets[ CS_SERVERINFO ];

	app->clc.sv_allowDownload = atoi(Info_ValueForKey(serverInfo,
		"sv_allowDownload"));
	Q_strncpyz(app->clc.sv_dlURL,
		Info_ValueForKey(serverInfo, "sv_dlURL"),
		sizeof(app->clc.sv_dlURL));

	/* remove ending slash in URLs */
	size_t	len = strlen( app->clc.sv_dlURL );
	if ( len > 0 &&  app->clc.sv_dlURL[len-1] == '/' )
		app->clc.sv_dlURL[len-1] = '\0';
}


/*
==================
CL_ParseGamestate
==================
*/
static void CL_ParseGamestate( clientApp_t *app, msg_t *msg ) {
	entityState_t	nullstate;

	/* The shared console UI is owned by the input-focused client. A non-focused
	 * in-process client's gamestate must not close the focused client's console
	 * (mirrors CL_WiredNetBootstrapResetState / the app==clientActiveApp gating
	 * in CL_WiredNetBootstrapFinalize). */
	// (Console close is no longer done here — parsing a gamestate is a wire
	//  operation, not a UI action. The connection-state edge this parse causes
	//  drives the console close via CL_OnClientStateChanged; see CL_SetState.)

	app->clc.connectPacketCount = 0;

	memset( &nullstate, 0, sizeof( nullstate ) );

	// clear old error message
	Com_ClearLastError();

	// wipe local client state
	CL_ClearState( app );

	// all configstring updates received before new gamestate must be discarded
	for ( int i = 0; i < MAX_RELIABLE_COMMANDS; i++ ) {
		const char *s = app->clc.serverCommands[ i ];
		if ( !strncmp( s, "cs ", 3 ) || !strncmp( s, "bcs0 ", 5 ) || !strncmp( s, "bcs1 ", 5 ) || !strncmp( s, "bcs2 ", 5 ) ) {
			app->clc.serverCommandsIgnore[ i ] = qtrue;
		}
	}

	// a gamestate always marks a server command sequence
	app->clc.serverCommandSequence = MSG_ReadLong( msg );

	// parse all the configstrings and baselines
	app->cl.gameState.dataCount = 1;	// leave a 0 at the beginning for uninitialized configstrings
	while ( 1 ) {
		int cmd = MSG_ReadByte( msg );

		if ( cmd == svc_EOF ) {
			break;
		}

		if ( cmd == svc_configstring ) {
			int		i = MSG_ReadShort( msg );
			if ( i < 0 || i >= MAX_CONFIGSTRINGS ) {
				Com_Terminate( TERM_CLIENT_DROP, "%s: configstring > MAX_CONFIGSTRINGS", __func__ );
			}

			const char *s = MSG_ReadBigString( msg );
			int		len = strlen( s );

			if ( len + 1 + app->cl.gameState.dataCount > MAX_GAMESTATE_CHARS ) {
				Com_Terminate( TERM_CLIENT_DROP, "%s: MAX_GAMESTATE_CHARS exceeded: %i", __func__,
					len + 1 + app->cl.gameState.dataCount );
			}

			// append it to the gameState string buffer
			app->cl.gameState.stringOffsets[ i ] = app->cl.gameState.dataCount;
			memcpy( app->cl.gameState.stringData + app->cl.gameState.dataCount, s, len + 1 );
			app->cl.gameState.dataCount += len + 1;
		} else if ( cmd == svc_baseline ) {
			int			newnum = MSG_ReadEntitynum( msg );

			if ( newnum < 0 ) {
				Com_Terminate( TERM_CLIENT_DROP, "%s: end of message", __func__ );
			}

			if ( newnum >= MAX_GENTITIES ) {
				Com_Terminate( TERM_CLIENT_DROP, "%s: baseline number out of range: %i", __func__, newnum );
			}

			entityState_t *es = &app->cl.entityBaselines[ newnum ];
			MSG_ReadDeltaEntity( msg, &nullstate, es, newnum );
			app->cl.baselineUsed[ newnum ] = 1;
		} else {
			Com_Terminate( TERM_CLIENT_DROP, "%s: bad command byte", __func__ );
		}
	}

	app->clc.eventMask |= EM_GAMESTATE;

	app->clc.clientNum = MSG_ReadLong(msg);
	// read the checksum feed
	app->clc.checksumFeed = MSG_ReadLong( msg );

	// save old gamedir
	char			oldGame[ MAX_QPATH ];
	Cvar_VariableStringBuffer( "fs_game", oldGame, sizeof( oldGame ) );

	// parse useful values out of CS_SERVERINFO — route to THIS gamestate's app
	// (CL_ParseServerInfo is threaded and `app` is in scope here; NOT a pull-escape).
	CL_ParseServerInfo( app );

	// parse serverId and other cvars for this gamestate's client. The shared
	// fs/pure globals are only driven when this is the input-focused client
	// (gated inside CL_SystemInfoChanged on app == clientActiveApp).
	CL_SystemInfoChanged( app, qtrue );

	// The demo-record stop, the fs_game / pure restart, the old-game bookkeeping
	// and the cl_paused reset all act on process-global state owned by the
	// input-focused client. A non-focused in-process client shares the already-
	// configured filesystem, so it skips this block and only finalizes its own
	// download/state via CL_InitDownloads below (mirrors CL_WiredNetBootstrapFinalize).
	if ( app == clientActiveApp ) {
		// stop recording now so the demo won't have an unnecessary level load at the end.
		if ( cl_autoRecordDemo->integer && app->clc.demorecording ) {
			if ( !app->clc.demoplaying ) {
				CL_StopRecord_f();
			}
		}

		qboolean		gamedirModified = ( Cvar_Flags( "fs_game" ) & CVAR_MODIFIED ) ? qtrue : qfalse;

		if ( !cl_oldGameSet && gamedirModified ) {
			cl_oldGameSet = qtrue;
			Q_strncpyz( cl_oldGame, oldGame, sizeof( cl_oldGame ) );
		}

		// try to keep gamestate and connection state during game switch
		app->gameSwitch = gamedirModified;

		// preserve \cl_reconnectAgrs between online game directory changes
		// so after mod switch \reconnect will not restore old value from config but use new one
		char			reconnectArgs[ MAX_CVAR_VALUE_STRING ];
		if ( gamedirModified ) {
			Cvar_VariableStringBuffer( "cl_reconnectArgs", reconnectArgs, sizeof( reconnectArgs ) );
		}

		// reinitialize the filesystem if the game directory has changed
		FS_ConditionalRestart( app->clc.checksumFeed, gamedirModified );

		// restore \cl_reconnectAgrs
		if ( gamedirModified ) {
			Cvar_Set( "cl_reconnectArgs", reconnectArgs );
		}

		app->gameSwitch = qfalse;
	}

	// This used to call CL_StartHunkUsers, but now we enter the download state before loading the cgame
	CL_InitDownloads( app );

	// make sure the game starts
	if ( app == clientActiveApp ) {
		Cvar_Set( "cl_paused", "0" );
	}
}


/*
=====================
CL_ValidPakSignature

checks for valid ZIP signature
returns qtrue for normal and empty archives
=====================
*/
qboolean CL_ValidPakSignature( const byte *data, int len )
{
	// maybe it is not 100% correct to check for file size here
	// because we may receive more data in future packets
	// but situation when server sends fragmented/shortened
	// zip header in first packet - looks pretty suspicious
	if ( len < 22 )
		return qfalse; // minimal ZIP file length is 22 bytes

	if ( data[0] != 'P' || data[1] != 'K' )
		return qfalse;

	if ( data[2] == 0x3 && data[3] == 0x4 )
		return qtrue; // local file header

	if ( data[2] == 0x5 && data[3] == 0x6 )
		return qtrue; // EOCD

	return qfalse;
}

//=====================================================================

/*
=====================
CL_ParseDownload

A download message has been received from the server
=====================
*/
static void CL_HandleDownloadBlock( clientApp_t *app, uint16_t block, int size, const byte *data,
	qboolean hasSize, int downloadSize, const char *errorMessage ) {

	if (!*app->clc.downloadTempName) {
		Com_Log( SEV_INFO, LOG_CH(ch_client), "Server sending download, but no download was requested\n");
		CL_AddReliableCommand( app, "stopdl", qfalse );
		return;
	}

	if ( app->clc.recordfile != FS_INVALID_HANDLE ) {
		CL_StopRecord_f();
	}

	if ( hasSize && !block && !app->clc.downloadBlock )
	{
		// block zero is special, contains file size
		app->clc.downloadSize = downloadSize;

		Cvar_SetIntegerValue( "cl_downloadSize", app->clc.downloadSize );

		if (app->clc.downloadSize < 0)
		{
			Com_Terminate( TERM_CLIENT_DROP, "%s", errorMessage ? errorMessage : "download error" );
			return;
		}
	}

	// NOTE: `data` is a pointer here, so the upper bound is MAX_MSGLEN (the
	// size of the caller's chunk buffer) — NOT sizeof(data), which would be
	// the pointer width and wrongly reject any legitimate block > 8 bytes.
	// The authoritative bound is enforced at the read site before MSG_ReadData;
	// this is a defense-in-depth re-check for all callers of this helper.
	if (size < 0 || size > MAX_MSGLEN)
	{
		Com_Terminate( TERM_CLIENT_DROP, "CL_ParseDownload: Invalid size %d for download chunk", size);
		return;
	}

	if((app->clc.downloadBlock & 0xFFFF) != block)
	{
		Com_Log( SEV_DEBUG, LOG_CH(ch_client), "CL_ParseDownload: Expected block %d, got %d\n", (app->clc.downloadBlock & 0xFFFF), block);
		return;
	}

	// open the file if not opened yet
	if ( app->clc.download == FS_INVALID_HANDLE )
	{
		if ( !CL_ValidPakSignature( data, size ) )
		{
			COM_WARN( LOG_CH(ch_client), "Invalid pak signature for %s\n", app->clc.downloadName );
			CL_AddReliableCommand( app, "stopdl", qfalse );
			CL_NextDownload();
			return;
		}

		app->clc.download = FS_SV_FOpenFileWrite( app->clc.downloadTempName );

		if ( app->clc.download == FS_INVALID_HANDLE )
		{
			Com_Log( SEV_INFO, LOG_CH(ch_client), "Could not create %s\n", app->clc.downloadTempName );
			CL_AddReliableCommand( app, "stopdl", qfalse );
			CL_NextDownload();
			return;
		}
	}

	if (size)
		FS_Write( data, size, app->clc.download );

	CL_AddReliableCommand( app, va("nextdl %d", app->clc.downloadBlock), qfalse );
	app->clc.downloadBlock++;

	app->clc.downloadCount += size;

	// So UI gets access to it
	Cvar_SetIntegerValue( "cl_downloadCount", app->clc.downloadCount );

	// update loading screen download progress
	if ( app->clc.downloadSize > 0 ) {
		cl_loadProgress.download = (float)app->clc.downloadCount / (float)app->clc.downloadSize;
	}

	if ( size == 0 ) { // A zero length block means EOF
		if ( app->clc.download != FS_INVALID_HANDLE ) {
			FS_FCloseFile( app->clc.download );
			app->clc.download = FS_INVALID_HANDLE;

			// rename the file
			FS_SV_Rename( app->clc.downloadTempName, app->clc.downloadName );
		}

		// send intentions now
		// We need this because without it, we would hold the last nextdl and then start
		// loading right away.  If we take a while to load, the server is happily trying
		// to send us that last block over and over.
		// Write it twice to help make sure we acknowledge the download
		CL_WritePacket( app, 1 );

		// get another file if needed
		CL_NextDownload();
	}
}

static void CL_ParseDownload( clientApp_t *app, msg_t *msg ) {
	int		size;
	unsigned char data[ MAX_MSGLEN ];
	uint16_t block;
	int         downloadSize = 0;

	// read the data
	block = MSG_ReadShort ( msg );

	if(!block && !app->clc.downloadBlock)
	{
		downloadSize = MSG_ReadLong ( msg );
	}

	size = MSG_ReadShort ( msg );
	// Bound the chunk size against the destination buffer BEFORE copying:
	// MSG_ReadShort yields a signed 16-bit value (-32768..32767) and
	// MSG_ReadData copies `size` bytes unconditionally, so an out-of-range
	// size from a malicious/corrupt server would overflow `data[MAX_MSGLEN]`.
	if ( size < 0 || size > (int)sizeof( data ) ) {
		Com_Terminate( TERM_CLIENT_DROP, "CL_ParseDownload: invalid download chunk size %d", size );
		return;
	}
	MSG_ReadData(msg, data, size);
	CL_HandleDownloadBlock( app, block, size, data,
		( !block && !app->clc.downloadBlock ) ? qtrue : qfalse,
		downloadSize, NULL );
}

static int CL_WiredNetReadU16( const byte *buf, int len, int *offset, uint16_t *out )
{
	if ( *offset + 2 > len ) {
		return 0;
	}
	*out = (uint16_t)( buf[*offset] | ( (uint16_t)buf[*offset + 1] << 8 ) );
	*offset += 2;
	return 1;
}

static int CL_WiredNetReadU32( const byte *buf, int len, int *offset, uint32_t *out )
{
	if ( *offset + 4 > len ) {
		return 0;
	}
	*out = (uint32_t)buf[*offset]
		| ( (uint32_t)buf[*offset + 1] << 8 )
		| ( (uint32_t)buf[*offset + 2] << 16 )
		| ( (uint32_t)buf[*offset + 3] << 24 );
	*offset += 4;
	return 1;
}

static int CL_WiredNetReadS32( const byte *buf, int len, int *offset, int *out )
{
	uint32_t v;
	if ( !CL_WiredNetReadU32( buf, len, offset, &v ) ) {
		return 0;
	}
	*out = (int)v;
	return 1;
}

static int CL_WiredNetReadBytes( const byte *buf, int len, int *offset, byte *out, int count )
{
	if ( *offset + count > len ) {
		return 0;
	}
	memcpy( out, buf + *offset, (size_t)count );
	*offset += count;
	return 1;
}

static int CL_WiredNetReadString( const byte *buf, int len, int *offset,
	char *out, int outSize )
{
	uint16_t slen;
	if ( !CL_WiredNetReadU16( buf, len, offset, &slen ) ) {
		return 0;
	}
	if ( slen <= 0 || *offset + slen > len || slen > outSize ) {
		return 0;
	}
	memcpy( out, buf + *offset, (size_t)slen );
	*offset += slen;
	if ( out[slen - 1] != '\0' ) {
		return 0;
	}
	return 1;
}

static int CL_WiredNetReadFloat( const byte *buf, int len, int *offset, float *out )
{
	uint32_t bits;
	if ( !CL_WiredNetReadU32( buf, len, offset, &bits ) ) {
		return 0;
	}
	memcpy( out, &bits, sizeof( bits ) );
	return 1;
}

static int CL_WiredNetReadTrajectory( const byte *buf, int len, int *offset, trajectory_t *tr )
{
	int trType;
	if ( !CL_WiredNetReadS32( buf, len, offset, &trType ) ||
		!CL_WiredNetReadS32( buf, len, offset, &tr->trTime ) ||
		!CL_WiredNetReadS32( buf, len, offset, &tr->trDuration ) ) {
		return 0;
	}
	tr->trType = (trType_t)trType;
	for ( int i = 0; i < 3; i++ ) {
		if ( !CL_WiredNetReadFloat( buf, len, offset, &tr->trBase[i] ) ) {
			return 0;
		}
	}
	for ( int i = 0; i < 3; i++ ) {
		if ( !CL_WiredNetReadFloat( buf, len, offset, &tr->trDelta[i] ) ) {
			return 0;
		}
	}
	return 1;
}

static int CL_WiredNetReadEntityState( const byte *buf, int len, int *offset, entityState_t *es )
{
	if ( !CL_WiredNetReadS32( buf, len, offset, &es->number ) ||
		!CL_WiredNetReadS32( buf, len, offset, &es->eType ) ||
		!CL_WiredNetReadS32( buf, len, offset, &es->eFlags ) ||
		!CL_WiredNetReadTrajectory( buf, len, offset, &es->pos ) ||
		!CL_WiredNetReadTrajectory( buf, len, offset, &es->apos ) ||
		!CL_WiredNetReadS32( buf, len, offset, &es->time ) ||
		!CL_WiredNetReadS32( buf, len, offset, &es->time2 ) ) {
		return 0;
	}
	for ( int i = 0; i < 3; i++ ) if ( !CL_WiredNetReadFloat( buf, len, offset, &es->origin[i] ) ) return 0;
	for ( int i = 0; i < 3; i++ ) if ( !CL_WiredNetReadFloat( buf, len, offset, &es->origin2[i] ) ) return 0;
	for ( int i = 0; i < 3; i++ ) if ( !CL_WiredNetReadFloat( buf, len, offset, &es->angles[i] ) ) return 0;
	for ( int i = 0; i < 3; i++ ) if ( !CL_WiredNetReadFloat( buf, len, offset, &es->angles2[i] ) ) return 0;
	if ( !CL_WiredNetReadS32( buf, len, offset, &es->otherEntityNum ) ||
		!CL_WiredNetReadS32( buf, len, offset, &es->otherEntityNum2 ) ||
		!CL_WiredNetReadS32( buf, len, offset, &es->groundEntityNum ) ||
		!CL_WiredNetReadS32( buf, len, offset, &es->constantLight ) ||
		!CL_WiredNetReadS32( buf, len, offset, &es->loopSound ) ||
		!CL_WiredNetReadS32( buf, len, offset, &es->modelindex ) ||
		!CL_WiredNetReadS32( buf, len, offset, &es->modelindex2 ) ||
		!CL_WiredNetReadS32( buf, len, offset, &es->clientNum ) ||
		!CL_WiredNetReadS32( buf, len, offset, &es->frame ) ||
		!CL_WiredNetReadS32( buf, len, offset, &es->solid ) ||
		!CL_WiredNetReadS32( buf, len, offset, &es->event ) ||
		!CL_WiredNetReadS32( buf, len, offset, &es->eventParm ) ||
		!CL_WiredNetReadS32( buf, len, offset, &es->powerups ) ||
		!CL_WiredNetReadS32( buf, len, offset, &es->weapon ) ||
		!CL_WiredNetReadS32( buf, len, offset, &es->legsAnim ) ||
		!CL_WiredNetReadS32( buf, len, offset, &es->torsoAnim ) ||
		!CL_WiredNetReadS32( buf, len, offset, &es->generic1 ) ) {
		return 0;
	}
	return 1;
}

static void CL_WiredNetBootstrapResetState( clientApp_t *app )
{
	/* Console close is no longer done here — resetting bootstrap state is a wire
	 * operation, not a UI action. The connection-state edge this reset causes
	 * (via CL_InitDownloads → CA_LOADING/CA_CONNECTED) drives the console close in
	 * CL_OnClientStateChanged, which is also correctly gated on the focused app. */
	app->clc.connectPacketCount = 0;
	Com_ClearLastError();
	CL_ClearState( app );
	for ( int i = 0; i < MAX_RELIABLE_COMMANDS; i++ ) {
		const char *s = app->clc.serverCommands[i];
		if ( !strncmp( s, "cs ", 3 ) || !strncmp( s, "bcs0 ", 5 ) ||
			!strncmp( s, "bcs1 ", 5 ) || !strncmp( s, "bcs2 ", 5 ) ) {
			app->clc.serverCommandsIgnore[i] = qtrue;
		}
	}
	app->cl.gameState.dataCount = 1;
	memset( app->cl.baselineUsed, 0, sizeof( app->cl.baselineUsed ) );
	memset( app->cl.entityBaselines, 0, sizeof( app->cl.entityBaselines ) );
}

static qboolean CL_WiredNetApplyServerCommand( clientApp_t *app, int seq, const char *s )
{
	int index;
	if ( app->clc.serverCommandSequence - seq >= 0 ) {
		return qtrue;
	}
	app->clc.serverCommandSequence = seq;
	index = seq & ( MAX_RELIABLE_COMMANDS - 1 );
	Q_strncpyz( app->clc.serverCommands[index], s, sizeof( app->clc.serverCommands[index] ) );
	app->clc.serverCommandsIgnore[index] = qfalse;
	return qtrue;
}

static qboolean CL_WiredNetApplyConfigstring( clientApp_t *app, int index, const char *s )
{
	int slen;
	if ( index < 0 || index >= MAX_CONFIGSTRINGS ) {
		return qfalse;
	}
	slen = (int)strlen( s );
	if ( slen + 1 + app->cl.gameState.dataCount > MAX_GAMESTATE_CHARS ) {
		return qfalse;
	}
	app->cl.gameState.stringOffsets[index] = app->cl.gameState.dataCount;
	memcpy( app->cl.gameState.stringData + app->cl.gameState.dataCount, s, (size_t)slen + 1 );
	app->cl.gameState.dataCount += slen + 1;
	return qtrue;
}

static qboolean CL_WiredNetApplyBaseline( clientApp_t *app, int entityNum, const entityState_t *es )
{
	if ( entityNum < 0 || entityNum >= MAX_GENTITIES ) {
		return qfalse;
	}
	app->cl.entityBaselines[entityNum] = *es;
	app->cl.baselineUsed[entityNum] = 1;
	return qtrue;
}

static void CL_WiredNetBootstrapFinalize( clientApp_t *app, int clientNum, int checksumFeed )
{
	char oldGame[MAX_QPATH];
	char reconnectArgs[MAX_CVAR_VALUE_STRING];
	qboolean gamedirModified;
	app->clc.eventMask |= EM_GAMESTATE;
	app->clc.clientNum = clientNum;
	app->clc.checksumFeed = checksumFeed;
	CL_ParseServerInfo( app );
	CL_SystemInfoChanged( app, qtrue );

	/* The fs_game / pure restart, the old-game bookkeeping, the demo-record stop
	 * and the cl_paused reset all act on process-global state owned by the
	 * input-focused client. An additional in-process client shares the already-
	 * configured filesystem, so it skips this block and only finalizes its own
	 * download/state via CL_InitDownloads below. */
	if ( app == clientActiveApp ) {
		Cvar_VariableStringBuffer( "fs_game", oldGame, sizeof( oldGame ) );
		if ( cl_autoRecordDemo->integer && app->clc.demorecording && !app->clc.demoplaying ) {
			CL_StopRecord_f();
		}
		gamedirModified = ( Cvar_Flags( "fs_game" ) & CVAR_MODIFIED ) ? qtrue : qfalse;
		if ( !cl_oldGameSet && gamedirModified ) {
			cl_oldGameSet = qtrue;
			Q_strncpyz( cl_oldGame, oldGame, sizeof( cl_oldGame ) );
		}
		app->gameSwitch = gamedirModified;
		if ( gamedirModified ) {
			Cvar_VariableStringBuffer( "cl_reconnectArgs", reconnectArgs, sizeof( reconnectArgs ) );
		}
		FS_ConditionalRestart( app->clc.checksumFeed, gamedirModified );
		if ( gamedirModified ) {
			Cvar_Set( "cl_reconnectArgs", reconnectArgs );
		}
		app->gameSwitch = qfalse;
	}

	CL_InitDownloads( app );

	if ( app == clientActiveApp ) {
		Cvar_Set( "cl_paused", "0" );
	}
}

static void CL_ParseTypedBootstrap( clientApp_t *app, const byte *buf, int len )
{
	int offset = 0;
	qboolean sawAck = qfalse;
	qboolean sawCmds = qfalse;
	qboolean sawConfig = qfalse;
	qboolean sawBaselines = qfalse;
	qboolean sawClientInfo = qfalse;
	int clientNum = -1;
	int checksumFeed = 0;

	if ( len < 1 || buf[0] != WN_BOOTSTRAP_MSG_STATE ) {
		COM_WARN( LOG_CH(ch_client), "WiredNet bootstrap: invalid message\n" );
		return;
	}
	CL_WiredNetBootstrapResetState( app );
	offset = 1;
	while ( offset < len ) {
		uint32_t sectionLen;
		int sectionEnd;
		int sectionType = buf[offset++];
		uint16_t count;
		if ( !CL_WiredNetReadU32( buf, len, &offset, &sectionLen ) ) {
			COM_WARN( LOG_CH(ch_client), "WiredNet bootstrap: short section header\n" );
			return;
		}
		// sectionLen is an untrusted uint32. Compare it against the remaining
		// bytes in UNSIGNED, overflow-free arithmetic before forming sectionEnd:
		// `offset + (int)sectionLen` would sign-overflow for a large sectionLen
		// (yielding a negative/small sectionEnd that slips past `> len`), making
		// the section bound bogus for every downstream read.
		if ( offset < 0 || offset > len || sectionLen > (uint32_t)( len - offset ) ) {
			COM_WARN( LOG_CH(ch_client), "WiredNet bootstrap: truncated section\n" );
			return;
		}
		sectionEnd = offset + (int)sectionLen;
		switch ( sectionType ) {
		case WN_BOOTSTRAP_SEC_ACK:
			if ( sawAck || !CL_WiredNetReadS32( buf, sectionEnd, &offset, &app->clc.reliableAcknowledge ) ) return;
			sawAck = qtrue;
			break;
		case WN_BOOTSTRAP_SEC_SERVER_CMDS:
			if ( sawCmds || !CL_WiredNetReadU16( buf, sectionEnd, &offset, &count ) ) return;
			while ( count-- ) {
				int seq;
				char cmd[MAX_STRING_CHARS];
				if ( !CL_WiredNetReadS32( buf, sectionEnd, &offset, &seq ) ||
					!CL_WiredNetReadString( buf, sectionEnd, &offset, cmd, sizeof( cmd ) ) ||
					!CL_WiredNetApplyServerCommand( app, seq, cmd ) ) {
					return;
				}
			}
			sawCmds = qtrue;
			break;
		case WN_BOOTSTRAP_SEC_CONFIGSTRINGS:
			if ( sawConfig || !CL_WiredNetReadU16( buf, sectionEnd, &offset, &count ) ) return;
			while ( count-- ) {
				uint16_t index;
				char value[BIG_INFO_STRING];
				if ( !CL_WiredNetReadU16( buf, sectionEnd, &offset, &index ) ||
					!CL_WiredNetReadString( buf, sectionEnd, &offset, value, sizeof( value ) ) ||
					!CL_WiredNetApplyConfigstring( app, index, value ) ) {
					return;
				}
			}
			sawConfig = qtrue;
			break;
		case WN_BOOTSTRAP_SEC_BASELINES:
			if ( sawBaselines || !CL_WiredNetReadU16( buf, sectionEnd, &offset, &count ) ) return;
			while ( count-- ) {
				uint16_t entityNum;
				entityState_t es;
				memset( &es, 0, sizeof( es ) );
				if ( !CL_WiredNetReadU16( buf, sectionEnd, &offset, &entityNum ) ||
					!CL_WiredNetReadEntityState( buf, sectionEnd, &offset, &es ) ||
					!CL_WiredNetApplyBaseline( app, entityNum, &es ) ) {
					return;
				}
			}
			sawBaselines = qtrue;
			break;
		case WN_BOOTSTRAP_SEC_CLIENT_INFO:
			if ( sawClientInfo ||
				!CL_WiredNetReadS32( buf, sectionEnd, &offset, &clientNum ) ||
				!CL_WiredNetReadS32( buf, sectionEnd, &offset, &checksumFeed ) ) {
				return;
			}
			sawClientInfo = qtrue;
			break;
		default:
			COM_WARN( LOG_CH(ch_client), "WiredNet bootstrap: unknown section %d\n", sectionType );
			return;
		}
		if ( offset != sectionEnd ) {
			COM_WARN( LOG_CH(ch_client), "WiredNet bootstrap: section length mismatch\n" );
			return;
		}
	}
	if ( !sawAck || !sawCmds || !sawConfig || !sawBaselines || !sawClientInfo ) {
		COM_WARN( LOG_CH(ch_client), "WiredNet bootstrap: missing required section\n" );
		return;
	}
	CL_WiredNetBootstrapFinalize( app, clientNum, checksumFeed );
}

static void CL_ParseTypedDownload( clientApp_t *app, const byte *buf, int len )
{
	uint16_t block;
	uint16_t size;
	int      downloadSize = 0;
	const byte *payload;

	if ( len < 1 ) {
		COM_WARN( LOG_CH(ch_client), "WiredNet download: short message\n" );
		return;
	}

	switch ( buf[0] ) {
	case WN_DOWNLOAD_MSG_ERROR:
		if ( len < 3 ) {
			COM_WARN( LOG_CH(ch_client), "WiredNet download: short error\n" );
			return;
		}
		{
			int errlen = (int)( buf[1] | ( (uint16_t)buf[2] << 8 ) );
			char reason[1024];
			if ( errlen > len - 3 )
				errlen = len - 3;
			if ( errlen >= (int)sizeof(reason) )
				errlen = (int)sizeof(reason) - 1;
			memcpy( reason, buf + 3, (size_t)errlen );
			reason[errlen] = '\0';
			Com_Terminate( TERM_CLIENT_DROP, "%s", reason );
		}
		return;

	case WN_DOWNLOAD_MSG_BLOCK:
		if ( len < 5 ) {
			COM_WARN( LOG_CH(ch_client), "WiredNet download: short block header\n" );
			return;
		}
		block = (uint16_t)( buf[1] | ( (uint16_t)buf[2] << 8 ) );
		size  = (uint16_t)( buf[3] | ( (uint16_t)buf[4] << 8 ) );
		payload = buf + 5;
		if ( block == 0 && app->clc.downloadBlock == 0 ) {
			if ( len < 9 ) {
				COM_WARN( LOG_CH(ch_client), "WiredNet download: short initial block\n" );
				return;
			}
			downloadSize = (int)( buf[5]
				| ( (uint32_t)buf[6] << 8 )
				| ( (uint32_t)buf[7] << 16 )
				| ( (uint32_t)buf[8] << 24 ) );
			payload = buf + 9;
			if ( size > len - 9 ) {
				COM_WARN( LOG_CH(ch_client), "WiredNet download: block payload truncated\n" );
				return;
			}
			CL_HandleDownloadBlock( app, block, size, payload, qtrue, downloadSize, NULL );
			return;
		}
		if ( size > len - 5 ) {
			COM_WARN( LOG_CH(ch_client), "WiredNet download: block payload truncated\n" );
			return;
		}
		CL_HandleDownloadBlock( app, block, size, payload, qfalse, 0, NULL );
		return;
	default:
		COM_WARN( LOG_CH(ch_client), "WiredNet download: unknown msg type %d\n", buf[0] );
		return;
	}
}


/*
=====================
CL_ParseCommandString

Command strings are just saved off until cgame asks for them
when it transitions a snapshot
=====================
*/
static void CL_ParseCommandString( clientApp_t *app, msg_t *msg ) {
	int		seq = MSG_ReadLong( msg );
	const char *s = MSG_ReadString( msg );

	if ( cl_shownet->integer >= 3 )
		Com_Log( SEV_INFO, LOG_CH(ch_client), " %3i(%3i) %s\n", seq, app->clc.serverCommandSequence, s );

	// see if we have already executed stored it off
	if ( app->clc.serverCommandSequence - seq >= 0 ) {
		return;
	}
	app->clc.serverCommandSequence = seq;

	int		index = seq & (MAX_RELIABLE_COMMANDS-1);
	Q_strncpyz( app->clc.serverCommands[ index ], s, sizeof( app->clc.serverCommands[ index ] ) );
	app->clc.serverCommandsIgnore[ index ] = qfalse;

	// -EC- : we may stuck on downloading because of non-working app->cgvm
	// or in "awaiting snapshot..." state so handle "disconnect" here
	if ( ( !app->cgvm && app->state == CA_CONNECTED && app->clc.download != FS_INVALID_HANDLE ) || ( app->cgvm && app->state == CA_PRIMED ) ) {
		const char *text;
		Cmd_TokenizeString( s );
		if ( !Q_stricmp( Cmd_Argv(0), "disconnect" ) ) {
			text = ( Cmd_Argc() > 1 ) ? va( "Server disconnected: %s", Cmd_Argv( 1 ) ) : "Server disconnected.";
			Com_SetLastError( "%s", text );
			Com_Log( SEV_INFO, LOG_CH(ch_client), "%s\n", text );
			if ( !CL_Disconnect( app, qtrue ) ) { // restart client if not done already
				CL_FlushMemory();
			}
			return;
		}
	}

	app->clc.eventMask |= EM_COMMAND;
}


/*
=====================
CL_ParseServerMessage
=====================
*/
void CL_ParseServerMessage( clientApp_t *app, msg_t *msg ) {
	if ( cl_shownet->integer == 1 ) {
		Com_Log( SEV_INFO, LOG_CH(ch_client), "%i ",msg->cursize );
	} else if ( cl_shownet->integer >= 2 ) {
		Com_Log( SEV_INFO, LOG_CH(ch_client), "------------------\n" );
	}

	app->clc.eventMask = 0;
	MSG_Bitstream( msg );

	// get the reliable sequence acknowledge number
	{
		int wire_ack = MSG_ReadLong( msg );
		if ( app->clc.quic_conn != CONN_INVALID ) {
			/* QUIC client path: reliable commands are delivered via QUIC streams
			 * (send_reliable on CHAN_COMMANDS) and acknowledged LOCALLY the moment
			 * they enter the stream (cl_input.c sets reliableAcknowledge =
			 * reliableSequence after the call). QUIC guarantees per-stream delivery,
			 * so the wire ack the server writes into each snapshot is redundant —
			 * worse, it races: the first snapshot often arrives before the server's
			 * SV_DrainQUICReliableCommands bumps lastClientCommand, making the wire
			 * value stale and clobbering the client's correct local state. Read the
			 * LONG to stay aligned with the Q3 snapshot framing, then discard it. */
			(void)wire_ack;
		} else {
			app->clc.reliableAcknowledge = wire_ack;

			if ( app->clc.reliableSequence - app->clc.reliableAcknowledge > MAX_RELIABLE_COMMANDS ) {
				if ( !app->clc.demoplaying ) {
					COM_WARN( LOG_CH(ch_client), "dropping %i commands from server\n", app->clc.reliableSequence - app->clc.reliableAcknowledge );
				}
				app->clc.reliableAcknowledge = app->clc.reliableSequence;
			} else if ( app->clc.reliableSequence - app->clc.reliableAcknowledge < 0 ) {
				if ( app->clc.demoplaying ) {
					app->clc.reliableSequence = app->clc.reliableAcknowledge;
				} else {
					Com_Terminate( TERM_CLIENT_DROP, "%s: incorrect reliable sequence acknowledge number", __func__ );
				}
			}
		}
	}

	// parse the message
	while ( 1 ) {
		if ( msg->readcount > msg->cursize ) {
			Com_Terminate( TERM_CLIENT_DROP,"%s: read past end of server message", __func__ );
			break;
		}

		int cmd = MSG_ReadByte( msg );

		/* In Huffman mode, MSG_ReadByte returns -1 when the post-decode
		 * readcount crosses cursize — even when the symbol decoded
		 * correctly (e.g. svc_EOF exactly fitting the last byte).
		 * Check -1 first (original Q3 behaviour), then svc_EOF as a
		 * belt-and-suspenders for any future OOB-mode callers. */
		if ( cmd == -1 || cmd == svc_EOF ) {
			SHOWNET( msg, "END OF MESSAGE" );
			break;
		}

		if ( cl_shownet->integer >= 2 ) {
			if ( (unsigned) cmd >= ARRAY_LEN( svc_strings ) ) {
				Com_Log( SEV_INFO, LOG_CH(ch_client), "%3i:BAD CMD %i\n", msg->readcount-1, cmd );
			} else {
				SHOWNET( msg, svc_strings[cmd] );
			}
		}

		// other commands
		switch ( cmd ) {
		default:
			Com_Terminate( TERM_CLIENT_DROP,"%s: Illegible server message", __func__ );
			break;
		case svc_nop:
			break;
		case svc_serverCommand:
			CL_ParseCommandString( app, msg );
			break;
		case svc_gamestate:
			CL_ParseGamestate( app, msg );
			break;
		case svc_snapshot:
			CL_ParseSnapshot( app, msg );
			break;
		case svc_download:
			if ( app->clc.demofile != FS_INVALID_HANDLE )
				return;
			CL_ParseDownload( app, msg );
			break;
		case svc_voipSpeex: // ioq3 extension
#ifdef USE_VOIP
			CL_ParseVoip( msg, qtrue );
			break;
#else
			return;
#endif
		case svc_voipOpus: // ioq3 extension
#ifdef USE_VOIP
			CL_ParseVoip( msg, !app->clc.voipEnabled );
			break;
#else
			return;
#endif
		}
	}
}

/*
==================
CL_CheckReliableStreams

Poll the WiredNet reliable recv queue each frame. `CHAN_BOOTSTRAP` carries
bootstrap state and `CHAN_DOWNLOAD` carries typed reliable download payloads.
Bootstrap currently reuses the standard Quake 3 server-message layout:
  [reliableAck:4][server cmds...][svc_gamestate][reliableSeq:4][configstrings/baselines][svc_EOF][clientNum:4][checksumFeed:4]
so we feed it to CL_ParseServerMessage which strips the header and
 dispatches to CL_ParseGamestate internally.  Client READY is sent later,
 after loading completes and the client has entered CA_PRIMED.
==================
*/

/* ── Fragment reassembly state (one pending snapshot at a time) ─────────────
 * If a new wn_sequence arrives before the previous is complete, the incomplete
 * set is discarded — it's stale. Memory is bounded to MAX_MSGLEN at all times. */
typedef struct {
	uint32_t wn_sequence;          /* snapshot sequence being assembled         */
	uint32_t delta_base;           /* delta baseline (informational, not parsed) */
	uint8_t  frag_total;           /* total fragments expected                   */
	uint8_t  frag_received_mask;   /* bitmask of received fragment indices (≤8)  */
	byte     data[MAX_MSGLEN];     /* reassembly buffer                          */
	int      frag_sizes[8];        /* byte count of each received fragment       */
} snapshot_reassembly_t;

/* Per-app fragment reassembly (in-process-queue L2): one pending-snapshot buffer
 * per local client so two apps' fragment streams never interleave. Indexed by
 * app slot (app - clientApps). At N=1 only slot 0 is touched, byte-identical to
 * the former single static. Kept file-static (type is cl_parse-local) rather
 * than a clientApp_t field to avoid moving the typedef into client.h. */
static snapshot_reassembly_t s_snap_reassembly_arr[MAX_LOCAL_CGAME_VMS];
#define CL_SNAP_REASSEMBLY( app ) ( s_snap_reassembly_arr[ (int)( (app) - clientApps ) ] )

void CL_CheckReliableStreams( void )
{
	byte          buf[MAX_MSGLEN];
	int           len;
	int           rchan;
	int           slot;

	/* Drain the srv->cli reliable + bootstrap rings for every in-process client
	 * and route each to its own clientApps[slot]. The drains take the slot
	 * explicitly (WN_ClientConsumeBootstrap/WN_ClientRecvReliable) so a slot's
	 * traffic only ever lands in its own clientApp_t.
	 *
	 * These are direct calls (NOT transport->recv_reliable): in listen-server mode
	 * the unified recv_reliable shim would also drain the server-side cli->srv
	 * queue here, stealing the local client's own outgoing CHAN_COMMANDS before
	 * sv_client.c consumes them. The slot-scoped client drains read only
	 * wtcl_array[slot].rel_queue / bootstrap (srv->cli), never the server-side
	 * game_conns[].rel_queue, keeping the two directions separate.
	 *
	 * Only slot 0 is connected today; the loop visits it alone and drains it in
	 * the same order as a single-client build. */
	for ( slot = 0; slot < MAX_LOCAL_CGAME_VMS; slot++ ) {
	clientApp_t  *app = &clientApps[slot];
	/* Process any slot with a connection in progress. The gamestate
	 * (CHAN_BOOTSTRAP) arrives while the client is still CA_CONNECTING — gating
	 * any higher would starve it and the client would never get its gamestate.
	 * A disconnected slot (CA_DISCONNECTED/CA_UNINITIALIZED) has nothing to
	 * drain. Slot 0 is always at CA_CONNECTING or beyond once connecting, so it
	 * is processed exactly as the single-client drain was. */
	if ( app->state <= CA_AUTHORIZING )
		continue;

	/* CHAN_BOOTSTRAP exceeds MAX_MSGLEN; it uses a dedicated large buffer. */
	{
		const byte *bdata;
		int         blen;
		if ( WN_ClientConsumeBootstrap( slot, &bdata, &blen ) ) {
			CL_ParseTypedBootstrap( app, bdata, blen );
		}
	}

	len = (int)sizeof( buf );
	while ( WN_ClientRecvReliable( slot, &rchan, buf, &len ) ) {
		if ( rchan == CHAN_DOWNLOAD ) {
			CL_ParseTypedDownload( app, buf, len );
		} else if ( rchan == CHAN_MCP ) {
			/* MCP JSON-RPC push from server via reliable channel.
			 * Primary MCP path is client-initiated bidi streams in wn_main.c;
			 * this handles server-initiated MCP messages if the server uses CHAN_MCP. */
			Com_Log( SEV_DEBUG, LOG_CH(ch_client), "QUIC: CHAN_MCP from server len=%d\n", len );
			/* Future: route to client-side MCP handler */
		} else if ( rchan == CHAN_SNAPSHOT_RELIABLE ) {
			/* Tier-3 reliable snapshot (wire format v2):
			 *   [wn_sequence:u32le] [delta_base:u32le] [flags:u8] [snapshot_data...]
			 * flags bit 0 = 0 (tier-3 never carries fragments). */
			if ( len > 9 ) {
				uint32_t srv_tick;
				uint8_t  flags;
				msg_t    rmsg;
				/* tier-3 reliable snapshot supersedes any in-flight
				 * tier-2 reassembly. Clear stale partial state to prevent cross-tier
				 * corruption (see docs/Q3NETCODE_VS_WIREDNET.md #11). */
				memset( &CL_SNAP_REASSEMBLY(app), 0, sizeof(CL_SNAP_REASSEMBLY(app)) );
				srv_tick = (uint32_t)buf[0]
				         | ( (uint32_t)buf[1] <<  8 )
				         | ( (uint32_t)buf[2] << 16 )
				         | ( (uint32_t)buf[3] << 24 );
				flags    = buf[8];
				if ( flags & 0x01 ) {
					/* Defensive sanity: tier-3 reliable should never carry fragments. */
					Com_Log( SEV_WARN, LOG_CH(ch_network_client),
						"snapshot recv: CHAN_SNAPSHOT_RELIABLE wn_seq=%u with is_fragment flag set — ignoring\n",
						srv_tick );
				} else {
					app->clc.serverMessageSequence = (int)srv_tick;
					MSG_Init( &rmsg, buf + 9, len - 9 );
					rmsg.cursize   = len - 9;
					rmsg.readcount = 0;
					app->clc.lastPacketTime = cls.realtime;
					CL_ParseServerMessage( app, &rmsg );
				}
			}
		}
		len = (int)sizeof( buf );
	}
	}  /* for each in-process client slot */
}

/*
==================
CL_CheckSnapshotDatagrams

Poll the QUIC unreliable recv queue for snapshot datagrams each frame.

Wire format v2:

Tier-1 single-datagram format (9-byte header):
  [wn_sequence:u32le] [delta_base:u32le] [flags:u8] [snapshot_data...]
  flags bit 0 = 0 (is_fragment=false) → this is a complete snapshot.

Tier-2 fragmented-datagram format (11-byte header):
  [wn_sequence:u32le] [delta_base:u32le] [flags:u8] [frag_total:u8] [frag_index:u8] [fragment_data...]
  flags bit 0 = 1 (is_fragment=true). flags bits 1..7 reserved (ignored on recv).
  All fragments must arrive before the snapshot is assembled and parsed.
  If any fragment is lost the snapshot is silently dropped (Q3 unreliable semantics).

delta_base is a pure u32 in v2 — no bit-31 multiplexing.
==================
*/
void CL_CheckSnapshotDatagrams( void )
{
	byte          dgbuf[MAX_MSGLEN_BUF];
	int           dglen;
	conn_handle_t dgconn;

	if ( !transport || !transport->recv_unreliable )
		return;

	dglen = (int)sizeof( dgbuf );
	while ( transport->recv_unreliable( &dgconn, dgbuf, &dglen ) ) {
		/* Route this datagram to its owning app by decoding the conn handle.
		 * At N=1 the only client handle is CONN_CLIENT_QUIC -> slot 0 ->
		 * clientApps[0] (== clientActiveApp), byte-identical to the old path. */
		clientApp_t *app = &clientApps[ WN_AppSlotForConn( dgconn ) ];
		if ( dglen >= 9 ) {
			uint32_t srv_tick  = (uint32_t)dgbuf[0]
			                   | ( (uint32_t)dgbuf[1] <<  8 )
			                   | ( (uint32_t)dgbuf[2] << 16 )
			                   | ( (uint32_t)dgbuf[3] << 24 );
			uint32_t base_tick = (uint32_t)dgbuf[4]
			                   | ( (uint32_t)dgbuf[5] <<  8 )
			                   | ( (uint32_t)dgbuf[6] << 16 )
			                   | ( (uint32_t)dgbuf[7] << 24 );
			uint8_t  flags     = dgbuf[8];
			qboolean is_frag   = ( flags & 0x01 ) != 0;

			if ( !is_frag ) {
				/* Tier-1 single complete datagram — fast path. */
				if ( dglen > 9 ) {
					msg_t msg;
					app->clc.serverMessageSequence = (int)srv_tick;
					Com_Log( SEV_TRACE, LOG_CH(ch_network_client), "snapshot recv: wn_seq=%u delta_base=%u → serverMessageSequence=%d\n",
						srv_tick, base_tick, app->clc.serverMessageSequence );
					MSG_Init( &msg, dgbuf + 9, dglen - 9 );
					msg.cursize   = dglen - 9;
					msg.readcount = 0;
					app->clc.lastPacketTime = cls.realtime;
					CL_ParseServerMessage( app, &msg );
				}
			} else if ( dglen >= 11 ) {
				/* Tier-2 fragment — reassemble before parsing. */
				uint8_t frag_total = dgbuf[9];
				uint8_t frag_index = dgbuf[10];
				int     frag_len   = dglen - 11;
				int      offset;
				uint8_t  all_mask;

				if ( frag_total < 2 || frag_total > 8 || frag_index >= frag_total || frag_len <= 0 ) {
					/* Malformed fragment — discard. */
					dglen = (int)sizeof( dgbuf );
					continue;
				}

				/* If this is for a different snapshot, discard old and start fresh. */
				if ( CL_SNAP_REASSEMBLY(app).wn_sequence != srv_tick ) {
					memset( &CL_SNAP_REASSEMBLY(app), 0, sizeof(CL_SNAP_REASSEMBLY(app)) );
					CL_SNAP_REASSEMBLY(app).wn_sequence = srv_tick;
					CL_SNAP_REASSEMBLY(app).delta_base  = base_tick;
					CL_SNAP_REASSEMBLY(app).frag_total  = frag_total;
				} else if ( CL_SNAP_REASSEMBLY(app).frag_total != frag_total ) {
					/* Later fragment of the SAME sequence disagrees on frag_total
					 * (a buggy/malicious server). all_mask and the completion-sum
					 * loop below both key off frag_total, so a mid-stream change
					 * would desync the mask from the stored frag_sizes[] — discard
					 * the inconsistent fragment rather than mixing the two. */
					dglen = (int)sizeof( dgbuf );
					continue;
				}

				offset = frag_index * WN_FRAG_PAYLOAD;
				if ( !( CL_SNAP_REASSEMBLY(app).frag_received_mask & ( 1 << frag_index ) ) &&
				     offset + frag_len <= (int)sizeof(CL_SNAP_REASSEMBLY(app).data) ) {
					memcpy( CL_SNAP_REASSEMBLY(app).data + offset, dgbuf + 11, frag_len );
					CL_SNAP_REASSEMBLY(app).frag_sizes[frag_index] = frag_len;
					CL_SNAP_REASSEMBLY(app).frag_received_mask |= (uint8_t)( 1 << frag_index );
				}

				/* Check if all fragments are in. */
				all_mask = ( frag_total == 8 ) ? 0xFF : (uint8_t)( ( 1 << frag_total ) - 1 );
				if ( CL_SNAP_REASSEMBLY(app).frag_received_mask == all_mask ) {
					int   total_len = 0;
					msg_t msg;
					for ( int i = 0; i < (int)frag_total; i++ )
						total_len += CL_SNAP_REASSEMBLY(app).frag_sizes[i];
					app->clc.serverMessageSequence = (int)CL_SNAP_REASSEMBLY(app).wn_sequence;
					Com_Log( SEV_TRACE, LOG_CH(ch_network_client), "snapshot recv: wn_seq=%u delta_base=%u (reassembled %d bytes) → serverMessageSequence=%d\n",
						CL_SNAP_REASSEMBLY(app).wn_sequence, CL_SNAP_REASSEMBLY(app).delta_base,
						total_len, app->clc.serverMessageSequence );
					MSG_Init( &msg, CL_SNAP_REASSEMBLY(app).data, total_len );
					msg.cursize   = total_len;
					msg.readcount = 0;
					app->clc.lastPacketTime = cls.realtime;
					CL_ParseServerMessage( app, &msg );
					memset( &CL_SNAP_REASSEMBLY(app), 0, sizeof(CL_SNAP_REASSEMBLY(app)) );
				}
			}
		}
		dglen = (int)sizeof( dgbuf );
	}
}
