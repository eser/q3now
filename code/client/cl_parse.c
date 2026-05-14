// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 1999-2005 Id Software, Inc.
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
// cl_parse.c  -- parse a message received from the server

#include "client.h"
#include "../qcommon/wired/net/wn_public.h"
/* Phase 5: log channels */
LOG_DECLARE_CHANNEL( ch_client, "client" );

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
static void CL_DeltaEntity( msg_t *msg, clSnapshot_t *frame, int newnum, const entityState_t *old, qboolean unchanged) {
	entityState_t	*state;

	// save the parsed entity state into the big circular buffer so
	// it can be used as the source for a later delta
	state = &cl.parseEntities[cl.parseEntitiesNum & (MAX_PARSE_ENTITIES-1)];

	if ( unchanged ) {
		// NOLINTNEXTLINE(clang-analyzer-core.NullDereference) — caller contract: unchanged=qtrue implies old is non-NULL (no prior frame ⇒ no "unchanged")
		*state = *old;
	} else {
		MSG_ReadDeltaEntity( msg, old, state, newnum );
	}

	if ( state->number == (MAX_GENTITIES-1) ) {
		return;		// entity was delta removed
	}
	cl.parseEntitiesNum++;
	frame->numEntities++;
}


/*
==================
CL_ParsePacketEntities
==================
*/
static void CL_ParsePacketEntities( msg_t *msg, const clSnapshot_t *oldframe, clSnapshot_t *newframe ) {
	newframe->parseEntitiesNum = cl.parseEntitiesNum;
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
			oldstate = &cl.parseEntities[
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
			CL_DeltaEntity( msg, newframe, oldnum, oldstate, qtrue );

			oldindex++;

			// NOLINTNEXTLINE(clang-analyzer-core.NullDereference) — packet-entity parser invariant: when oldnum != MAX_GENTITIES+1 (sentinel), oldframe is non-NULL
			if ( oldindex >= oldframe->numEntities ) {
				oldnum = MAX_GENTITIES+1;
			} else {
				oldstate = &cl.parseEntities[
					(oldframe->parseEntitiesNum + oldindex) & (MAX_PARSE_ENTITIES-1)];
				oldnum = oldstate->number;
			}
		}
		if (oldnum == newnum) {
			// delta from previous state
			if ( cl_shownet->integer == 3 ) {
				Com_Log( SEV_INFO, LOG_CH(ch_client), "%3i:  delta: %i\n", msg->readcount, newnum);
			}
			CL_DeltaEntity( msg, newframe, newnum, oldstate, qfalse );

			oldindex++;

			// NOLINTNEXTLINE(clang-analyzer-core.NullDereference) — packet-entity parser invariant: when oldnum != MAX_GENTITIES+1 (sentinel), oldframe is non-NULL
			if ( oldindex >= oldframe->numEntities ) {
				oldnum = MAX_GENTITIES+1;
			} else {
				oldstate = &cl.parseEntities[
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
			CL_DeltaEntity( msg, newframe, newnum, &cl.entityBaselines[newnum], qfalse );
			continue;
		}

	}

	// any remaining entities in the old frame are copied over
	while ( oldnum != MAX_GENTITIES+1 ) {
		// one or more entities from the old packet are unchanged
		if ( cl_shownet->integer == 3 ) {
			Com_Log( SEV_INFO, LOG_CH(ch_client), "%3i:  unchanged: %i\n", msg->readcount, oldnum);
		}
		CL_DeltaEntity( msg, newframe, oldnum, oldstate, qtrue );

		oldindex++;

		if ( oldindex >= oldframe->numEntities ) {
			oldnum = MAX_GENTITIES+1;
		} else {
			oldstate = &cl.parseEntities[
				(oldframe->parseEntitiesNum + oldindex) & (MAX_PARSE_ENTITIES-1)];
			oldnum = oldstate->number;
		}
	}
}


/*
================
CL_ParseSnapshot

If the snapshot is parsed properly, it will be copied to
cl.snap and saved in cl.snapshots[].  If the snapshot is invalid
for any reason, no changes to the state will be made at all.
================
*/
static void CL_ParseSnapshot( msg_t *msg ) {
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
	newSnap.serverCommandNum = clc.serverCommandSequence;

	newSnap.serverTime = MSG_ReadLong( msg );

	newSnap.messageNum = clc.serverMessageSequence;

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
		clc.demowaiting = qfalse;	// we can start recording now
	} else {
		old = &cl.snapshots[newSnap.deltaNum & PACKET_MASK];
		if ( !old->valid ) {
			// should never happen
			Com_Log( SEV_INFO, LOG_CH(ch_client), "Delta from invalid frame (not supposed to happen!).\n");
		} else if ( old->messageNum != newSnap.deltaNum ) {
			// The frame that the server did the delta from
			// is too old, so we can't reconstruct it properly.
			Com_Log( SEV_INFO, LOG_CH(ch_client), "Delta frame too old.\n");
		} else if ( cl.parseEntitiesNum - old->parseEntitiesNum > MAX_PARSE_ENTITIES - MAX_SNAPSHOT_ENTITIES ) {
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
	CL_ParsePacketEntities( msg, old, &newSnap );

	// if not valid, dump the entire thing now that it has
	// been properly read
	if ( !newSnap.valid ) {
		return;
	}

	// clear the valid flags of any snapshots between the last
	// received and this one, so if there was a dropped packet
	// it won't look like something valid to delta from next
	// time we wrap around in the buffer
	int			oldMessageNum = cl.snap.messageNum + 1;

	if ( newSnap.messageNum - oldMessageNum >= PACKET_BACKUP ) {
		oldMessageNum = newSnap.messageNum - ( PACKET_BACKUP - 1 );
	}

	for ( int i = 0, n = newSnap.messageNum - oldMessageNum; i < n; i++ ) {
		cl.snapshots[ ( oldMessageNum + i ) & PACKET_MASK ].valid = qfalse;
	}

	// copy to the current good spot
	cl.snap = newSnap;
	cl.snap.ping = 999;
	// calculate ping time
	for ( int i = 0 ; i < PACKET_BACKUP ; i++ ) {
		int packetNum = ( clc.netchan.outgoingSequence - 1 - i ) & PACKET_MASK;
		if ( cl.snap.ps.commandTime - cl.outPackets[packetNum].p_serverTime >= 0 ) {
			cl.snap.ping = cls.realtime - cl.outPackets[ packetNum ].p_realtime;
			break;
		}
	}
	// save the frame off in the backup array for later delta comparisons
	cl.snapshots[cl.snap.messageNum & PACKET_MASK] = cl.snap;

	if (cl_shownet->integer == 3) {
		Com_Log( SEV_INFO, LOG_CH(ch_client), "   snapshot:%i  delta:%i  ping:%i\n", cl.snap.messageNum,
		cl.snap.deltaNum, cl.snap.ping );
	}

	cl.newSnapshots = qtrue;

	clc.eventMask |= EM_SNAPSHOT;
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
void CL_SystemInfoChanged( qboolean onlyGame ) {
	const char		*systemInfo;
	const char		*s, *t;
	char			key[BIG_INFO_KEY];
	char			value[BIG_INFO_VALUE];

	systemInfo = cl.gameState.stringData + cl.gameState.stringOffsets[ CS_SYSTEMINFO ];
	// NOTE TTimo:
	// when the serverId changes, any further messages we send to the server will use this new serverId
	// https://zerowing.idsoftware.com/bugzilla/show_bug.cgi?id=475
	// in some cases, outdated cp commands might get sent with this news serverId
	cl.serverId = atoi( Info_ValueForKey( systemInfo, "sv_serverid" ) );

	// don't set any vars when playing a demo
	if ( clc.demoplaying ) {
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
qboolean CL_GameSwitch( void )
{
	return (cls.gameSwitch && !com_errorEntered);
}


/*
==================
CL_ParseServerInfo
==================
*/
static void CL_ParseServerInfo( void )
{
	const char *serverInfo = cl.gameState.stringData
		+ cl.gameState.stringOffsets[ CS_SERVERINFO ];

	clc.sv_allowDownload = atoi(Info_ValueForKey(serverInfo,
		"sv_allowDownload"));
	Q_strncpyz(clc.sv_dlURL,
		Info_ValueForKey(serverInfo, "sv_dlURL"),
		sizeof(clc.sv_dlURL));

	/* remove ending slash in URLs */
	size_t	len = strlen( clc.sv_dlURL );
	if ( len > 0 &&  clc.sv_dlURL[len-1] == '/' )
		clc.sv_dlURL[len-1] = '\0';
}


/*
==================
CL_ParseGamestate
==================
*/
static void CL_ParseGamestate( msg_t *msg ) {
	entityState_t	nullstate;

	Con_Close();

	clc.connectPacketCount = 0;

	memset( &nullstate, 0, sizeof( nullstate ) );

	// clear old error message
	Com_ClearLastError();

	// wipe local client state
	CL_ClearState();

	// all configstring updates received before new gamestate must be discarded
	for ( int i = 0; i < MAX_RELIABLE_COMMANDS; i++ ) {
		const char *s = clc.serverCommands[ i ];
		if ( !strncmp( s, "cs ", 3 ) || !strncmp( s, "bcs0 ", 5 ) || !strncmp( s, "bcs1 ", 5 ) || !strncmp( s, "bcs2 ", 5 ) ) {
			clc.serverCommandsIgnore[ i ] = qtrue;
		}
	}

	// a gamestate always marks a server command sequence
	clc.serverCommandSequence = MSG_ReadLong( msg );

	// parse all the configstrings and baselines
	cl.gameState.dataCount = 1;	// leave a 0 at the beginning for uninitialized configstrings
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

			if ( len + 1 + cl.gameState.dataCount > MAX_GAMESTATE_CHARS ) {
				Com_Terminate( TERM_CLIENT_DROP, "%s: MAX_GAMESTATE_CHARS exceeded: %i", __func__,
					len + 1 + cl.gameState.dataCount );
			}

			// append it to the gameState string buffer
			cl.gameState.stringOffsets[ i ] = cl.gameState.dataCount;
			memcpy( cl.gameState.stringData + cl.gameState.dataCount, s, len + 1 );
			cl.gameState.dataCount += len + 1;
		} else if ( cmd == svc_baseline ) {
			int			newnum = MSG_ReadEntitynum( msg );

			if ( newnum < 0 ) {
				Com_Terminate( TERM_CLIENT_DROP, "%s: end of message", __func__ );
			}

			if ( newnum >= MAX_GENTITIES ) {
				Com_Terminate( TERM_CLIENT_DROP, "%s: baseline number out of range: %i", __func__, newnum );
			}

			entityState_t *es = &cl.entityBaselines[ newnum ];
			MSG_ReadDeltaEntity( msg, &nullstate, es, newnum );
			cl.baselineUsed[ newnum ] = 1;
		} else {
			Com_Terminate( TERM_CLIENT_DROP, "%s: bad command byte", __func__ );
		}
	}

	clc.eventMask |= EM_GAMESTATE;

	clc.clientNum = MSG_ReadLong(msg);
	// read the checksum feed
	clc.checksumFeed = MSG_ReadLong( msg );

	// save old gamedir
	char			oldGame[ MAX_QPATH ];
	Cvar_VariableStringBuffer( "fs_game", oldGame, sizeof( oldGame ) );

	// parse useful values out of CS_SERVERINFO
	CL_ParseServerInfo();

	// parse serverId and other cvars
	CL_SystemInfoChanged( qtrue );

	// stop recording now so the demo won't have an unnecessary level load at the end.
	if ( cl_autoRecordDemo->integer && clc.demorecording ) {
		if ( !clc.demoplaying ) {
			CL_StopRecord_f();
		}
	}

	qboolean		gamedirModified = ( Cvar_Flags( "fs_game" ) & CVAR_MODIFIED ) ? qtrue : qfalse;

	if ( !cl_oldGameSet && gamedirModified ) {
		cl_oldGameSet = qtrue;
		Q_strncpyz( cl_oldGame, oldGame, sizeof( cl_oldGame ) );
	}

	// try to keep gamestate and connection state during game switch
	cls.gameSwitch = gamedirModified;

	// preserve \cl_reconnectAgrs between online game directory changes
	// so after mod switch \reconnect will not restore old value from config but use new one
	char			reconnectArgs[ MAX_CVAR_VALUE_STRING ];
	if ( gamedirModified ) {
		Cvar_VariableStringBuffer( "cl_reconnectArgs", reconnectArgs, sizeof( reconnectArgs ) );
	}

	// reinitialize the filesystem if the game directory has changed
	FS_ConditionalRestart( clc.checksumFeed, gamedirModified );

	// restore \cl_reconnectAgrs
	if ( gamedirModified ) {
		Cvar_Set( "cl_reconnectArgs", reconnectArgs );
	}

	cls.gameSwitch = qfalse;

	// This used to call CL_StartHunkUsers, but now we enter the download state before loading the cgame
	CL_InitDownloads();

	// make sure the game starts
	Cvar_Set( "cl_paused", "0" );
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
static void CL_HandleDownloadBlock( uint16_t block, int size, const byte *data,
	qboolean hasSize, int downloadSize, const char *errorMessage ) {

	if (!*clc.downloadTempName) {
		Com_Log( SEV_INFO, LOG_CH(ch_client), "Server sending download, but no download was requested\n");
		CL_AddReliableCommand( "stopdl", qfalse );
		return;
	}

	if ( clc.recordfile != FS_INVALID_HANDLE ) {
		CL_StopRecord_f();
	}

	if ( hasSize && !block && !clc.downloadBlock )
	{
		// block zero is special, contains file size
		clc.downloadSize = downloadSize;

		Cvar_SetIntegerValue( "cl_downloadSize", clc.downloadSize );

		if (clc.downloadSize < 0)
		{
			Com_Terminate( TERM_CLIENT_DROP, "%s", errorMessage ? errorMessage : "download error" );
			return;
		}
	}

	if (size < 0 || size > sizeof(data))
	{
		Com_Terminate( TERM_CLIENT_DROP, "CL_ParseDownload: Invalid size %d for download chunk", size);
		return;
	}

	if((clc.downloadBlock & 0xFFFF) != block)
	{
		Com_Log( SEV_DEBUG, LOG_CH(ch_client), "CL_ParseDownload: Expected block %d, got %d\n", (clc.downloadBlock & 0xFFFF), block);
		return;
	}

	// open the file if not opened yet
	if ( clc.download == FS_INVALID_HANDLE )
	{
		if ( !CL_ValidPakSignature( data, size ) )
		{
			COM_WARN( LOG_CH(ch_client), "Invalid pak signature for %s\n", clc.downloadName );
			CL_AddReliableCommand( "stopdl", qfalse );
			CL_NextDownload();
			return;
		}

		clc.download = FS_SV_FOpenFileWrite( clc.downloadTempName );

		if ( clc.download == FS_INVALID_HANDLE )
		{
			Com_Log( SEV_INFO, LOG_CH(ch_client), "Could not create %s\n", clc.downloadTempName );
			CL_AddReliableCommand( "stopdl", qfalse );
			CL_NextDownload();
			return;
		}
	}

	if (size)
		FS_Write( data, size, clc.download );

	CL_AddReliableCommand( va("nextdl %d", clc.downloadBlock), qfalse );
	clc.downloadBlock++;

	clc.downloadCount += size;

	// So UI gets access to it
	Cvar_SetIntegerValue( "cl_downloadCount", clc.downloadCount );

	// update loading screen download progress
	if ( clc.downloadSize > 0 ) {
		cl_loadProgress.download = (float)clc.downloadCount / (float)clc.downloadSize;
	}

	if ( size == 0 ) { // A zero length block means EOF
		if ( clc.download != FS_INVALID_HANDLE ) {
			FS_FCloseFile( clc.download );
			clc.download = FS_INVALID_HANDLE;

			// rename the file
			FS_SV_Rename( clc.downloadTempName, clc.downloadName );
		}

		// send intentions now
		// We need this because without it, we would hold the last nextdl and then start
		// loading right away.  If we take a while to load, the server is happily trying
		// to send us that last block over and over.
		// Write it twice to help make sure we acknowledge the download
		CL_WritePacket( 1 );

		// get another file if needed
		CL_NextDownload();
	}
}

static void CL_ParseDownload( msg_t *msg ) {
	int		size;
	unsigned char data[ MAX_MSGLEN ];
	uint16_t block;
	int         downloadSize = 0;

	// read the data
	block = MSG_ReadShort ( msg );

	if(!block && !clc.downloadBlock)
	{
		downloadSize = MSG_ReadLong ( msg );
	}

	size = MSG_ReadShort ( msg );
	MSG_ReadData(msg, data, size);
	CL_HandleDownloadBlock( block, size, data,
		( !block && !clc.downloadBlock ) ? qtrue : qfalse,
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

static void CL_WiredNetBootstrapResetState( void )
{
	Con_Close();
	clc.connectPacketCount = 0;
	Com_ClearLastError();
	CL_ClearState();
	for ( int i = 0; i < MAX_RELIABLE_COMMANDS; i++ ) {
		const char *s = clc.serverCommands[i];
		if ( !strncmp( s, "cs ", 3 ) || !strncmp( s, "bcs0 ", 5 ) ||
			!strncmp( s, "bcs1 ", 5 ) || !strncmp( s, "bcs2 ", 5 ) ) {
			clc.serverCommandsIgnore[i] = qtrue;
		}
	}
	cl.gameState.dataCount = 1;
	memset( cl.baselineUsed, 0, sizeof( cl.baselineUsed ) );
	memset( cl.entityBaselines, 0, sizeof( cl.entityBaselines ) );
}

static qboolean CL_WiredNetApplyServerCommand( int seq, const char *s )
{
	int index;
	if ( clc.serverCommandSequence - seq >= 0 ) {
		return qtrue;
	}
	clc.serverCommandSequence = seq;
	index = seq & ( MAX_RELIABLE_COMMANDS - 1 );
	Q_strncpyz( clc.serverCommands[index], s, sizeof( clc.serverCommands[index] ) );
	clc.serverCommandsIgnore[index] = qfalse;
	return qtrue;
}

static qboolean CL_WiredNetApplyConfigstring( int index, const char *s )
{
	int slen;
	if ( index < 0 || index >= MAX_CONFIGSTRINGS ) {
		return qfalse;
	}
	slen = (int)strlen( s );
	if ( slen + 1 + cl.gameState.dataCount > MAX_GAMESTATE_CHARS ) {
		return qfalse;
	}
	cl.gameState.stringOffsets[index] = cl.gameState.dataCount;
	memcpy( cl.gameState.stringData + cl.gameState.dataCount, s, (size_t)slen + 1 );
	cl.gameState.dataCount += slen + 1;
	return qtrue;
}

static qboolean CL_WiredNetApplyBaseline( int entityNum, const entityState_t *es )
{
	if ( entityNum < 0 || entityNum >= MAX_GENTITIES ) {
		return qfalse;
	}
	cl.entityBaselines[entityNum] = *es;
	cl.baselineUsed[entityNum] = 1;
	return qtrue;
}

static void CL_WiredNetBootstrapFinalize( int clientNum, int checksumFeed )
{
	char oldGame[MAX_QPATH];
	char reconnectArgs[MAX_CVAR_VALUE_STRING];
	qboolean gamedirModified;
	clc.eventMask |= EM_GAMESTATE;
	clc.clientNum = clientNum;
	clc.checksumFeed = checksumFeed;
	Cvar_VariableStringBuffer( "fs_game", oldGame, sizeof( oldGame ) );
	CL_ParseServerInfo();
	CL_SystemInfoChanged( qtrue );
	if ( cl_autoRecordDemo->integer && clc.demorecording && !clc.demoplaying ) {
		CL_StopRecord_f();
	}
	gamedirModified = ( Cvar_Flags( "fs_game" ) & CVAR_MODIFIED ) ? qtrue : qfalse;
	if ( !cl_oldGameSet && gamedirModified ) {
		cl_oldGameSet = qtrue;
		Q_strncpyz( cl_oldGame, oldGame, sizeof( cl_oldGame ) );
	}
	cls.gameSwitch = gamedirModified;
	if ( gamedirModified ) {
		Cvar_VariableStringBuffer( "cl_reconnectArgs", reconnectArgs, sizeof( reconnectArgs ) );
	}
	FS_ConditionalRestart( clc.checksumFeed, gamedirModified );
	if ( gamedirModified ) {
		Cvar_Set( "cl_reconnectArgs", reconnectArgs );
	}
	cls.gameSwitch = qfalse;
	CL_InitDownloads();
	Cvar_Set( "cl_paused", "0" );
}

static void CL_ParseTypedBootstrap( const byte *buf, int len )
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
	CL_WiredNetBootstrapResetState();
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
		sectionEnd = offset + (int)sectionLen;
		if ( sectionEnd > len ) {
			COM_WARN( LOG_CH(ch_client), "WiredNet bootstrap: truncated section\n" );
			return;
		}
		switch ( sectionType ) {
		case WN_BOOTSTRAP_SEC_ACK:
			if ( sawAck || !CL_WiredNetReadS32( buf, sectionEnd, &offset, &clc.reliableAcknowledge ) ) return;
			sawAck = qtrue;
			break;
		case WN_BOOTSTRAP_SEC_SERVER_CMDS:
			if ( sawCmds || !CL_WiredNetReadU16( buf, sectionEnd, &offset, &count ) ) return;
			while ( count-- ) {
				int seq;
				char cmd[MAX_STRING_CHARS];
				if ( !CL_WiredNetReadS32( buf, sectionEnd, &offset, &seq ) ||
					!CL_WiredNetReadString( buf, sectionEnd, &offset, cmd, sizeof( cmd ) ) ||
					!CL_WiredNetApplyServerCommand( seq, cmd ) ) {
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
					!CL_WiredNetApplyConfigstring( index, value ) ) {
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
					!CL_WiredNetApplyBaseline( entityNum, &es ) ) {
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
	CL_WiredNetBootstrapFinalize( clientNum, checksumFeed );
}

static void CL_ParseTypedDownload( const byte *buf, int len )
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
		if ( block == 0 && clc.downloadBlock == 0 ) {
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
			CL_HandleDownloadBlock( block, size, payload, qtrue, downloadSize, NULL );
			return;
		}
		if ( size > len - 5 ) {
			COM_WARN( LOG_CH(ch_client), "WiredNet download: block payload truncated\n" );
			return;
		}
		CL_HandleDownloadBlock( block, size, payload, qfalse, 0, NULL );
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
static void CL_ParseCommandString( msg_t *msg ) {
	int		seq = MSG_ReadLong( msg );
	const char *s = MSG_ReadString( msg );

	if ( cl_shownet->integer >= 3 )
		Com_Log( SEV_INFO, LOG_CH(ch_client), " %3i(%3i) %s\n", seq, clc.serverCommandSequence, s );

	// see if we have already executed stored it off
	if ( clc.serverCommandSequence - seq >= 0 ) {
		return;
	}
	clc.serverCommandSequence = seq;

	int		index = seq & (MAX_RELIABLE_COMMANDS-1);
	Q_strncpyz( clc.serverCommands[ index ], s, sizeof( clc.serverCommands[ index ] ) );
	clc.serverCommandsIgnore[ index ] = qfalse;

#ifdef USE_CURL
	if ( !clc.cURLUsed )
#endif
	// -EC- : we may stuck on downloading because of non-working cgvm
	// or in "awaiting snapshot..." state so handle "disconnect" here
	if ( ( !cgvm && cls.state == CA_CONNECTED && clc.download != FS_INVALID_HANDLE ) || ( cgvm && cls.state == CA_PRIMED ) ) {
		const char *text;
		Cmd_TokenizeString( s );
		if ( !Q_stricmp( Cmd_Argv(0), "disconnect" ) ) {
			text = ( Cmd_Argc() > 1 ) ? va( "Server disconnected: %s", Cmd_Argv( 1 ) ) : "Server disconnected.";
			Com_SetLastError( "%s", text );
			Com_Log( SEV_INFO, LOG_CH(ch_client), "%s\n", text );
			if ( !CL_Disconnect( qtrue ) ) { // restart client if not done already
				CL_FlushMemory();
			}
			return;
		}
	}

	clc.eventMask |= EM_COMMAND;
}


/*
=====================
CL_ParseServerMessage
=====================
*/
void CL_ParseServerMessage( msg_t *msg ) {
	if ( cl_shownet->integer == 1 ) {
		Com_Log( SEV_INFO, LOG_CH(ch_client), "%i ",msg->cursize );
	} else if ( cl_shownet->integer >= 2 ) {
		Com_Log( SEV_INFO, LOG_CH(ch_client), "------------------\n" );
	}

	clc.eventMask = 0;
	MSG_Bitstream( msg );

	// get the reliable sequence acknowledge number
	{
		int wire_ack = MSG_ReadLong( msg );
		if ( clc.quic_conn != CONN_INVALID ) {
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
			clc.reliableAcknowledge = wire_ack;

			if ( clc.reliableSequence - clc.reliableAcknowledge > MAX_RELIABLE_COMMANDS ) {
				if ( !clc.demoplaying ) {
					COM_WARN( LOG_CH(ch_client), "dropping %i commands from server\n", clc.reliableSequence - clc.reliableAcknowledge );
				}
				clc.reliableAcknowledge = clc.reliableSequence;
			} else if ( clc.reliableSequence - clc.reliableAcknowledge < 0 ) {
				if ( clc.demoplaying ) {
					clc.reliableSequence = clc.reliableAcknowledge;
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
			CL_ParseCommandString( msg );
			break;
		case svc_gamestate:
			CL_ParseGamestate( msg );
			break;
		case svc_snapshot:
			CL_ParseSnapshot( msg );
			break;
		case svc_download:
			if ( clc.demofile != FS_INVALID_HANDLE )
				return;
			CL_ParseDownload( msg );
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
			CL_ParseVoip( msg, !clc.voipEnabled );
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

static snapshot_reassembly_t s_snap_reassembly;

void CL_CheckReliableStreams( void )
{
	byte          buf[MAX_MSGLEN];
	int           len;
	int           rchan;

	/* CHAN_BOOTSTRAP exceeds MAX_MSGLEN; it uses a dedicated large buffer. */
	{
		const byte *bdata;
		int         blen;
		if ( WN_ClientConsumeBootstrap( &bdata, &blen ) ) {
			CL_ParseTypedBootstrap( bdata, blen );
		}
	}

	len = (int)sizeof( buf );
	while ( WN_ClientRecvReliable( &rchan, buf, &len ) ) {
		if ( rchan == CHAN_DOWNLOAD ) {
			CL_ParseTypedDownload( buf, len );
		} else if ( rchan == CHAN_MCP ) {
			/* MCP JSON-RPC push from server via reliable channel.
			 * Primary MCP path is client-initiated bidi streams in wn_main.c;
			 * this handles server-initiated MCP messages if the server uses CHAN_MCP. */
			Com_Log( SEV_DEBUG, LOG_CH(ch_client), "QUIC: CHAN_MCP from server len=%d\n", len );
			/* Future: route to client-side MCP handler */
		} else if ( rchan == CHAN_SNAPSHOT_RELIABLE ) {
			/* Reliable snapshot: [wn_sequence:u32le][delta_base:u32le][snapshot_data...]
			 * Parse identically to a single-datagram snapshot. */
			if ( len > 8 ) {
				uint32_t srv_tick;
				msg_t    rmsg;
				srv_tick = (uint32_t)buf[0]
				         | ( (uint32_t)buf[1] <<  8 )
				         | ( (uint32_t)buf[2] << 16 )
				         | ( (uint32_t)buf[3] << 24 );
				clc.serverMessageSequence = (int)srv_tick;
				MSG_Init( &rmsg, buf + 8, len - 8 );
				rmsg.cursize   = len - 8;
				rmsg.readcount = 0;
				clc.lastPacketTime = cls.realtime;
				CL_ParseServerMessage( &rmsg );
			}
		}
		len = (int)sizeof( buf );
	}
}

/*
==================
CL_CheckSnapshotDatagrams

Poll the QUIC unreliable recv queue for snapshot datagrams each frame.

Single-datagram format:
  [wn_sequence:u32le] [delta_base:u32le] [snapshot_data...]
  delta_base high bit = 0 → this is a complete snapshot.

Fragmented-datagram format (high bit of delta_base set):
  [wn_sequence:u32le] [delta_base|0x80000000:u32le] [frag_total:u8] [frag_index:u8] [fragment_data...]
  All fragments must arrive before the snapshot is assembled and parsed.
  If any fragment is lost the snapshot is silently dropped (Q3 unreliable semantics).
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
		if ( dglen >= 8 ) {
			uint32_t srv_tick  = (uint32_t)dgbuf[0]
			                   | ( (uint32_t)dgbuf[1] <<  8 )
			                   | ( (uint32_t)dgbuf[2] << 16 )
			                   | ( (uint32_t)dgbuf[3] << 24 );
			uint32_t raw_base  = (uint32_t)dgbuf[4]
			                   | ( (uint32_t)dgbuf[5] <<  8 )
			                   | ( (uint32_t)dgbuf[6] << 16 )
			                   | ( (uint32_t)dgbuf[7] << 24 );
			qboolean is_frag   = ( raw_base & 0x80000000u ) != 0;

			if ( !is_frag ) {
				/* Single complete datagram — fast path. */
				if ( dglen > 8 ) {
					msg_t msg;
					clc.serverMessageSequence = (int)srv_tick;
					Com_Log( SEV_TRACE, LOG_CH(ch_client), "[WiredNet] snapshot recv: wn_seq=%u delta_base=%u → serverMessageSequence=%d\n",
						srv_tick, raw_base & 0x7FFFFFFFu, clc.serverMessageSequence );
					MSG_Init( &msg, dgbuf + 8, dglen - 8 );
					msg.cursize   = dglen - 8;
					msg.readcount = 0;
					clc.lastPacketTime = cls.realtime;
					CL_ParseServerMessage( &msg );
				}
			} else if ( dglen >= 10 ) {
				/* Fragment — reassemble before parsing. */
				uint8_t frag_total = dgbuf[8];
				uint8_t frag_index = dgbuf[9];
				int     frag_len   = dglen - 10;
				uint32_t base_tick = raw_base & 0x7FFFFFFFu;
				int      offset;
				uint8_t  all_mask;

				if ( frag_total < 2 || frag_total > 8 || frag_index >= frag_total || frag_len <= 0 ) {
					/* Malformed fragment — discard. */
					dglen = (int)sizeof( dgbuf );
					continue;
				}

				/* If this is for a different snapshot, discard old and start fresh. */
				if ( s_snap_reassembly.wn_sequence != srv_tick ) {
					memset( &s_snap_reassembly, 0, sizeof(s_snap_reassembly) );
					s_snap_reassembly.wn_sequence = srv_tick;
					s_snap_reassembly.delta_base  = base_tick;
					s_snap_reassembly.frag_total  = frag_total;
				}

				offset = frag_index * WN_FRAG_PAYLOAD;
				if ( !( s_snap_reassembly.frag_received_mask & ( 1 << frag_index ) ) &&
				     offset + frag_len <= (int)sizeof(s_snap_reassembly.data) ) {
					memcpy( s_snap_reassembly.data + offset, dgbuf + 10, frag_len );
					s_snap_reassembly.frag_sizes[frag_index] = frag_len;
					s_snap_reassembly.frag_received_mask |= (uint8_t)( 1 << frag_index );
				}

				/* Check if all fragments are in. */
				all_mask = ( frag_total == 8 ) ? 0xFF : (uint8_t)( ( 1 << frag_total ) - 1 );
				if ( s_snap_reassembly.frag_received_mask == all_mask ) {
					int   total_len = 0;
					msg_t msg;
					for ( int i = 0; i < (int)frag_total; i++ )
						total_len += s_snap_reassembly.frag_sizes[i];
					clc.serverMessageSequence = (int)s_snap_reassembly.wn_sequence;
					Com_Log( SEV_TRACE, LOG_CH(ch_client), "[WiredNet] snapshot recv: wn_seq=%u delta_base=%u (reassembled %d bytes) → serverMessageSequence=%d\n",
						s_snap_reassembly.wn_sequence, s_snap_reassembly.delta_base,
						total_len, clc.serverMessageSequence );
					MSG_Init( &msg, s_snap_reassembly.data, total_len );
					msg.cursize   = total_len;
					msg.readcount = 0;
					clc.lastPacketTime = cls.realtime;
					CL_ParseServerMessage( &msg );
					memset( &s_snap_reassembly, 0, sizeof(s_snap_reassembly) );
				}
			}
		}
		dglen = (int)sizeof( dgbuf );
	}
}
