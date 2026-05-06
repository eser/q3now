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
// cl_main.c  -- client main loop

#include "client.h"
#include "wired/ui/cl_wired_ui.h"
#include "wired/ui/cl_wired_attract.h"
#include "wired/store/cl_wired_store.h"
#include "../qcommon/util/crypto.h"
#include "../qcommon/wired/net/wn_public.h"
#include <limits.h>

cvar_t	*cl_noprint;
cvar_t	*cl_debugMove;
cvar_t	*cl_motd;

#ifdef USE_RENDERER_DLOPEN
static cvar_t *cl_renderer;
static int    s_cl_renderer_mod = -1;
#endif

cvar_t	*cl_timeout;
cvar_t	*cl_autoNudge;
cvar_t	*cl_timeNudge;
cvar_t	*cl_showTimeDelta;

cvar_t	*cl_shownet;
cvar_t	*cl_autoRecordDemo;
cvar_t	*cl_drawRecording;

cvar_t	*cl_aviFrameRate;
cvar_t	*cl_aviMotionJpeg;
cvar_t	*cl_forceavidemo;
cvar_t	*cl_aviPipeFormat;

cvar_t	*cl_activeAction;

cvar_t	*cl_motdString;

cvar_t	*cl_allowDownload;
#ifdef USE_CURL
cvar_t	*cl_mapAutoDownload;
#endif
cvar_t	*cl_conXOffset;
cvar_t	*cl_conYOffset;
cvar_t	*cl_conColor;
cvar_t	*cl_inGameVideo;

cvar_t	*cl_serverStatusResendTime;

cvar_t	*cl_lanForcePackets;

cvar_t	*cl_guidServerUniq;

cvar_t	*cl_dlURL;

cvar_t	*cl_reconnectArgs;

/* CNQ3 backport: cl_matchAlerts
 *   Bit 1 = trigger alert even when the window is merely unfocused
 *           (without the bit, alerts fire only while minimized)
 *   Bit 2 = flash the taskbar / dock / window manager
 *   Bit 4 = beep (platform attention sound)
 *   Bit 8 = temporarily override s_autoMute while the alert is active
 * Default is 7 (flash + beep + focus-sensitive, no unmute override).
 */
cvar_t	*cl_matchAlerts;

// common cvars for GLimp modules
cvar_t	*vid_xpos;			// X coordinate of window position
cvar_t	*vid_ypos;			// Y coordinate of window position
cvar_t	*r_noborder;

cvar_t *r_allowSoftwareGL;	// don't abort out if the pixelformat claims software
cvar_t *r_swapInterval;
cvar_t *r_glDriver;
cvar_t *r_displayRefresh;
cvar_t *r_fullscreen;
cvar_t *r_mode;
cvar_t *r_modeFullscreen;
cvar_t *r_customwidth;
cvar_t *r_customheight;
cvar_t *r_customPixelAspect;

cvar_t *r_colorbits;
// these also shared with renderers:
cvar_t *cl_stencilbits;
cvar_t *cl_depthbits;
cvar_t *cl_drawBuffer;

clientActive_t		cl;
clientConnection_t	clc;
clientStatic_t		cls;
clLoadProgress_t	cl_loadProgress;
vm_t				*cgvm = NULL;

char				cl_oldGame[ MAX_QPATH ];
qboolean			cl_oldGameSet;
static	qboolean	noGameRestart = qfalse;

#ifdef USE_CURL
download_t			download;
#endif

// Structure containing functions exported from refresh DLL
refexport_t	re;
#ifdef USE_RENDERER_DLOPEN
static void	*rendererLib;
#endif

static ping_t cl_pinglist[MAX_PINGREQUESTS];

typedef struct serverStatus_s
{
	char string[BIG_INFO_STRING];
	netadr_t address;
	int time, startTime;
	qboolean pending;
	qboolean print;
	qboolean retrieved;
} serverStatus_t;

static serverStatus_t cl_serverStatusList[MAX_SERVERSTATUSREQUESTS];

static void CL_CheckForResend( void );
static void CL_ShowIP_f( void );
static void CL_ServerStatus_f( void );
static void CL_ServerStatusResponse( const netadr_t *from, msg_t *msg );
static void CL_ServerInfoPacket( const netadr_t *from, msg_t *msg );

#ifdef USE_CURL
static void CL_Download_f( void );
#endif
static void CL_LocalServers_f( void );
static void CL_GlobalServers_f( void );
static void CL_Ping_f( void );

static void CL_InitRef( void );
static void CL_ShutdownRef( refShutdownCode_t code );
static void CL_InitGLimp_Cvars( void );
static void CL_RconLogin_f( void );
static void CL_Rcon_f( void );

static void CL_NextDemo( void );

static cvar_t *cl_wiredRconPassword;

qboolean CL_DemoPlaying( void ) {
	return clc.demoplaying;
}


/*
=======================================================================

CLIENT RELIABLE COMMAND COMMUNICATION

=======================================================================
*/

/*
======================
CL_AddReliableCommand

The given command will be transmitted to the server, and is guaranteed to
not have future usercmd_t executed before it is executed
======================
*/
void CL_AddReliableCommand( const char *cmd, qboolean isDisconnectCmd ) {
	int unacknowledged = clc.reliableSequence - clc.reliableAcknowledge;

	if ( clc.serverAddress.type == NA_BAD )
		return;

	// if we would be losing an old command that hasn't been acknowledged,
	// we must drop the connection
	// also leave one slot open for the disconnect command in this case.

	if ((isDisconnectCmd && unacknowledged > MAX_RELIABLE_COMMANDS) ||
		(!isDisconnectCmd && unacknowledged >= MAX_RELIABLE_COMMANDS))
	{
		if( com_errorEntered )
			return;
		else
			Com_Terminate( TERM_CLIENT_DROP, "Client command overflow");
	}

	clc.reliableSequence++;
	int index = clc.reliableSequence & ( MAX_RELIABLE_COMMANDS - 1 );
	Q_strncpyz( clc.reliableCommands[ index ], cmd, sizeof( clc.reliableCommands[ index ] ) );
}


/*
=======================================================================

CLIENT SIDE DEMO RECORDING

=======================================================================
*/

/*
====================
CL_WriteDemoMessage

Dumps the current net message, prefixed by the length
====================
*/
static void CL_WriteDemoMessage( msg_t *msg, int headerBytes ) {
	// write the packet sequence
	int len = clc.serverMessageSequence;
	int swlen = LittleLong( len );
	FS_Write( &swlen, 4, clc.recordfile );

	// skip the packet sequencing information
	len = msg->cursize - headerBytes;
	swlen = LittleLong(len);
	FS_Write( &swlen, 4, clc.recordfile );
	FS_Write( msg->data + headerBytes, len, clc.recordfile );
}


/*
====================
CL_StopRecording_f

stop recording a demo
====================
*/
void CL_StopRecord_f( void ) {

	if ( clc.recordfile != FS_INVALID_HANDLE ) {
		char tempName[MAX_OSPATH];
		char finalName[MAX_OSPATH];
		int protocol = PROTOCOL_VERSION;

		// finish up
		int len = -1;
		FS_Write( &len, 4, clc.recordfile );
		FS_Write( &len, 4, clc.recordfile );
		FS_FCloseFile( clc.recordfile );
		clc.recordfile = FS_INVALID_HANDLE;

		if ( com_protocol->integer != PROTOCOL_VERSION ) {
			protocol = com_protocol->integer;
		}

		Com_sprintf( tempName, sizeof( tempName ), "%s.tmp", clc.recordName );

		Com_sprintf( finalName, sizeof( finalName ), "%s.%s%d", clc.recordName, DEMOEXT, protocol );

		if ( clc.explicitRecordName ) {
			/* Demo file is written via FS_FOpenFileWrite (homepath/fs_gamedir/...);
			 * FS_HomeRemove rebuilds the same path. */
			FS_HomeRemove( finalName );
		} else {
			// add sequence suffix to avoid overwrite
			int sequence = 0;
			while ( FS_FileExists( finalName ) && ++sequence < 1000 ) {
				Com_sprintf( finalName, sizeof( finalName ), "%s-%02d.%s%d",
					clc.recordName, sequence, DEMOEXT, protocol );
			}
		}

		FS_Rename( tempName, finalName );
	}

	if ( !clc.demorecording ) {
		Com_Log( SEV_INFO, LOG_CAT_CLIENT, "Not recording a demo.\n" );
	} else {
		Com_Log( SEV_INFO, LOG_CAT_CLIENT, "Stopped demo recording.\n" );
	}

	clc.demorecording = qfalse;
	clc.spDemoRecording = qfalse;
}


/*
====================
CL_WriteServerCommands
====================
*/
static void CL_WriteServerCommands( msg_t *msg ) {
	if ( clc.serverCommandSequence - clc.demoCommandSequence > 0 ) {

		// do not write more than MAX_RELIABLE_COMMANDS
		if ( clc.serverCommandSequence - clc.demoCommandSequence > MAX_RELIABLE_COMMANDS ) {
			clc.demoCommandSequence = clc.serverCommandSequence - MAX_RELIABLE_COMMANDS;
		}

		for ( int i = clc.demoCommandSequence + 1 ; i <= clc.serverCommandSequence; i++ ) {
			MSG_WriteByte( msg, svc_serverCommand );
			MSG_WriteLong( msg, i );
			MSG_WriteString( msg, clc.serverCommands[ i & (MAX_RELIABLE_COMMANDS-1) ] );
		}
	}

	clc.demoCommandSequence = clc.serverCommandSequence;
}


/*
====================
CL_WriteGamestate
====================
*/
static void CL_WriteGamestate( qboolean initial )
{
	byte		bufData[ MAX_MSGLEN_BUF ];
	msg_t		msg;

	// write out the gamestate message
	MSG_Init( &msg, bufData, MAX_MSGLEN );
	MSG_Bitstream( &msg );

	// NOTE, MRE: all server->client messages now acknowledge
	MSG_WriteLong( &msg, clc.reliableSequence );

	if ( initial ) {
		clc.demoMessageSequence = 1;
		clc.demoCommandSequence = clc.serverCommandSequence;
	} else {
		CL_WriteServerCommands( &msg );
	}

	clc.demoDeltaNum = 0; // reset delta for next snapshot

	MSG_WriteByte( &msg, svc_gamestate );
	MSG_WriteLong( &msg, clc.serverCommandSequence );

	// configstrings
	for ( int i = 0 ; i < MAX_CONFIGSTRINGS ; i++ ) {
		if ( !cl.gameState.stringOffsets[i] ) {
			continue;
		}
		char *s = cl.gameState.stringData + cl.gameState.stringOffsets[i];
		MSG_WriteByte( &msg, svc_configstring );
		MSG_WriteShort( &msg, i );
		MSG_WriteBigString( &msg, s );
	}

	// baselines
	entityState_t nullstate;
	memset( &nullstate, 0, sizeof( nullstate ) );
	for ( int i = 0; i < MAX_GENTITIES ; i++ ) {
		if ( !cl.baselineUsed[ i ] )
			continue;
		entityState_t *ent = &cl.entityBaselines[ i ];
		MSG_WriteByte( &msg, svc_baseline );
		MSG_WriteDeltaEntity( &msg, &nullstate, ent, qtrue );
	}

	// finalize message
	MSG_WriteByte( &msg, svc_EOF );

	// finished writing the gamestate stuff

	// write the client num
	MSG_WriteLong( &msg, clc.clientNum );

	// write the checksum feed
	MSG_WriteLong( &msg, clc.checksumFeed );

	// finished writing the client packet
	MSG_WriteByte( &msg, svc_EOF );

	// write it to the demo file
	int len;
	if ( clc.demoplaying )
		len = LittleLong( clc.demoMessageSequence - 1 );
	else
		len = LittleLong( clc.serverMessageSequence - 1 );

	FS_Write( &len, 4, clc.recordfile );

	len = LittleLong( msg.cursize );
	FS_Write( &len, 4, clc.recordfile );
	FS_Write( msg.data, msg.cursize, clc.recordfile );
}


/*
=============
CL_EmitPacketEntities
=============
*/
static void CL_EmitPacketEntities( clSnapshot_t *from, clSnapshot_t *to, msg_t *msg, entityState_t *oldents ) {
	// generate the delta update
	int from_num_entities;
	if ( !from ) {
		from_num_entities = 0;
	} else {
		from_num_entities = from->numEntities;
	}

	entityState_t *newent = NULL;
	entityState_t *oldent = NULL;
	int newindex = 0;
	int oldindex = 0;
	while ( newindex < to->numEntities || oldindex < from_num_entities ) {
		int newnum;
		if ( newindex >= to->numEntities ) {
			newnum = MAX_GENTITIES+1;
		} else {
			newent = &cl.parseEntities[(to->parseEntitiesNum + newindex) % MAX_PARSE_ENTITIES];
			newnum = newent->number;
		}

		int oldnum;
		if ( oldindex >= from_num_entities ) {
			oldnum = MAX_GENTITIES+1;
		} else {
			//oldent = &cl.parseEntities[(from->parseEntitiesNum + oldindex) % MAX_PARSE_ENTITIES];
			oldent = &oldents[ oldindex ];
			oldnum = oldent->number;
		}

		if ( newnum == oldnum ) {
			// delta update from old position
			// because the force parm is qfalse, this will not result
			// in any bytes being emitted if the entity has not changed at all
			MSG_WriteDeltaEntity (msg, oldent, newent, qfalse );
			oldindex++;
			newindex++;
			continue;
		}

		if ( newnum < oldnum ) {
			// this is a new entity, send it from the baseline
			MSG_WriteDeltaEntity (msg, &cl.entityBaselines[newnum], newent, qtrue );
			newindex++;
			continue;
		}

		if ( newnum > oldnum ) {
			// the old entity isn't present in the new message
			MSG_WriteDeltaEntity (msg, oldent, NULL, qtrue );
			oldindex++;
			continue;
		}
	}

	MSG_WriteBits( msg, (MAX_GENTITIES-1), GENTITYNUM_BITS );	// end of packetentities
}


/*
====================
CL_WriteSnapshot
====================
*/
static void CL_WriteSnapshot( void ) {

	static	clSnapshot_t saved_snap;
	static entityState_t saved_ents[ MAX_SNAPSHOT_ENTITIES ];

	byte	bufData[ MAX_MSGLEN_BUF ];
	msg_t	msg;

	clSnapshot_t *snap = &cl.snapshots[ cl.snap.messageNum & PACKET_MASK ]; // current snapshot
	//if ( !snap->valid ) // should never happen?
	//	return;

	clSnapshot_t *oldSnap;
	if ( clc.demoDeltaNum == 0 ) {
		oldSnap = NULL;
	} else {
		oldSnap = &saved_snap;
	}

	MSG_Init( &msg, bufData, MAX_MSGLEN );
	MSG_Bitstream( &msg );

	// NOTE, MRE: all server->client messages now acknowledge
	MSG_WriteLong( &msg, clc.reliableSequence );

	// Write all pending server commands
	CL_WriteServerCommands( &msg );

	MSG_WriteByte( &msg, svc_snapshot );
	MSG_WriteLong( &msg, snap->serverTime ); // sv.time
	MSG_WriteByte( &msg, clc.demoDeltaNum ); // 0 or 1
	MSG_WriteByte( &msg, snap->snapFlags );  // snapFlags
	MSG_WriteByte( &msg, snap->areabytes );  // areabytes
	MSG_WriteData( &msg, snap->areamask, snap->areabytes );
	if ( oldSnap )
		MSG_WriteDeltaPlayerstate( &msg, &oldSnap->ps, &snap->ps );
	else
		MSG_WriteDeltaPlayerstate( &msg, NULL, &snap->ps );

	CL_EmitPacketEntities( oldSnap, snap, &msg, saved_ents );

	// finished writing the client packet
	MSG_WriteByte( &msg, svc_EOF );

	// write it to the demo file
	int len;
	if ( clc.demoplaying )
		len = LittleLong( clc.demoMessageSequence );
	else
		len = LittleLong( clc.serverMessageSequence );
	FS_Write( &len, 4, clc.recordfile );

	len = LittleLong( msg.cursize );
	FS_Write( &len, 4, clc.recordfile );
	FS_Write( msg.data, msg.cursize, clc.recordfile );

	// save last sent state so if there any need - we can skip any further incoming messages
	for ( int i = 0; i < snap->numEntities; i++ )
		saved_ents[ i ] = cl.parseEntities[ (snap->parseEntitiesNum + i) % MAX_PARSE_ENTITIES ];

	saved_snap = *snap;
	saved_snap.parseEntitiesNum = 0;

	clc.demoMessageSequence++;
	clc.demoDeltaNum = 1;
}


/*
====================
CL_Record_f

record <demoname>

Begins recording a demo from the current position
====================
*/
static void CL_Record_f( void ) {
	if ( Cmd_Argc() > 2 ) {
		Com_Log( SEV_INFO, LOG_CAT_CLIENT, "record <demoname>\n" );
		return;
	}

	if ( clc.demorecording ) {
		if ( !clc.spDemoRecording ) {
			Com_Log( SEV_INFO, LOG_CAT_CLIENT, "Already recording.\n" );
		}
		return;
	}

	if ( cls.state != CA_ACTIVE ) {
		Com_Log( SEV_INFO, LOG_CAT_CLIENT, "You must be in a level to record.\n" );
		return;
	}

	// sync 0 doesn't prevent recording, so not forcing it off .. everyone does g_sync 1 ; record ; g_sync 0 ..
	if ( NET_IsLocalAddress( &clc.serverAddress ) && !Cvar_VariableIntegerValue( "g_synchronousClients" ) ) {
		COM_WARN( LOG_CAT_CLIENT, "WARNING: You should set 'g_synchronousClients 1' for smoother demo recording\n" );
	}

	char		demoName[MAX_OSPATH];
	char		name[MAX_OSPATH];
	char		demoExt[16];

	if ( Cmd_Argc() == 2 ) {
		// explicit demo name specified
		Q_strncpyz( demoName, Cmd_Argv( 1 ), sizeof( demoName ) );
		const char *ext = COM_GetExtension( demoName );
		if ( *ext ) {
			// strip demo extension
			sprintf( demoExt, "%s%d", DEMOEXT, PROTOCOL_VERSION );
			if ( Q_stricmp( ext, demoExt ) == 0 ) {
				*(strrchr( demoName, '.' )) = '\0';
			}
		}
		Com_sprintf( name, sizeof( name ), "demos/%s", demoName );

		clc.explicitRecordName = qtrue;
	} else {
		qtime_t t;
		Com_RealTime( &t );
		/* CNQ3 backport: use YYYY_MM_DD-HH_MM_SS filename format so
		   timestamped demos sort lexicographically. */
		Com_sprintf( name, sizeof( name ), "demos/%04d_%02d_%02d-%02d_%02d_%02d",
			1900 + t.tm_year, 1 + t.tm_mon, t.tm_mday,
			t.tm_hour, t.tm_min, t.tm_sec );

		clc.explicitRecordName = qfalse;
	}

	// save desired filename without extension
	Q_strncpyz( clc.recordName, name, sizeof( clc.recordName ) );

	Com_Log( SEV_INFO, LOG_CAT_CLIENT, "recording to %s.\n", name );

	// start new record with temporary extension
	{ qstring_t _nm_qs = QS_WrapExisting( name, sizeof( name ) ); QS_Append( &_nm_qs, ".tmp" ); }

	// open the demo file
	clc.recordfile = FS_FOpenFileWrite( name );
	if ( clc.recordfile == FS_INVALID_HANDLE ) {
		Com_Log( SEV_INFO, LOG_CAT_CLIENT, "ERROR: couldn't open.\n" );
		clc.recordName[0] = '\0';
		return;
	}

	clc.demorecording = qtrue;

	Com_TruncateLongString( clc.recordNameShort, clc.recordName );

	if ( Cvar_VariableIntegerValue( "ui_recordSPDemo" ) ) {
	  clc.spDemoRecording = qtrue;
	} else {
	  clc.spDemoRecording = qfalse;
	}

	// don't start saving messages until a non-delta compressed message is received
	clc.demowaiting = qtrue;

	// write out the gamestate message
	CL_WriteGamestate( qtrue );

	// the rest of the demo file will be copied from net messages
}


/*
====================
CL_CompleteRecordName
====================
*/
static void CL_CompleteRecordName(const char *args, int argNum )
{
	if ( argNum == 2 )
	{
		char demoExt[ 16 ];

		Com_sprintf( demoExt, sizeof( demoExt ), "." DEMOEXT "%d", com_protocol->integer );
		Field_CompleteFilename( "demos", demoExt, qtrue, FS_MATCH_EXTERN | FS_MATCH_STICK );
	}
}


/*
=======================================================================

CLIENT SIDE DEMO PLAYBACK

=======================================================================
*/

/*
=================
CL_DemoCompleted
=================
*/
static void CL_DemoCompleted( void ) {
	if ( com_timedemo->integer ) {
		int time = Sys_Milliseconds() - clc.timeDemoStart;
		if ( time > 0 ) {
			Com_Log( SEV_INFO, LOG_CAT_CLIENT, "%i frames, %3.*f seconds: %3.1f fps\n", clc.timeDemoFrames,
			time > 10000 ? 1 : 2, time/1000.0, clc.timeDemoFrames*1000.0 / time );
		}
	}

	CL_Disconnect( qtrue );
	if ( WiredAttract_OnDemoCompleted() ) {
		return; /* attract scheduler handled the advance */
	}
	CL_NextDemo();
}


/*
=================
CL_ReadDemoMessage
=================
*/
void CL_ReadDemoMessage( void ) {
	msg_t		buf;
	byte		bufData[ MAX_MSGLEN_BUF ];

	if ( clc.demofile == FS_INVALID_HANDLE ) {
		CL_DemoCompleted();
		return;
	}

	// get the sequence number
	int s;
	int r = FS_Read( &s, 4, clc.demofile );
	if ( r != 4 ) {
		CL_DemoCompleted();
		return;
	}
	clc.serverMessageSequence = LittleLong( s );

	// init the message
	MSG_Init( &buf, bufData, MAX_MSGLEN );

	// get the length
	r = FS_Read( &buf.cursize, 4, clc.demofile );
	if ( r != 4 ) {
		CL_DemoCompleted();
		return;
	}
	buf.cursize = LittleLong( buf.cursize );
	if ( buf.cursize < 0 ) {
		CL_DemoCompleted();
		return;
	}
	if ( buf.cursize > buf.maxsize ) {
		Com_Terminate( TERM_CLIENT_DROP, "CL_ReadDemoMessage: demoMsglen > MAX_MSGLEN");
	}
	r = FS_Read( buf.data, buf.cursize, clc.demofile );
	if ( r != buf.cursize ) {
		Com_Log( SEV_INFO, LOG_CAT_CLIENT, "Demo file was truncated.\n");
		CL_DemoCompleted();
		return;
	}

	clc.lastPacketTime = cls.realtime;
	buf.readcount = 0;

	clc.demoCommandSequence = clc.serverCommandSequence;

	CL_ParseServerMessage( &buf );

	if ( clc.demorecording ) {
		// track changes and write new message
		if ( clc.eventMask & EM_GAMESTATE ) {
			CL_WriteGamestate( qfalse );
			// nothing should came after gamestate in current message
		} else if ( clc.eventMask & (EM_SNAPSHOT|EM_COMMAND) ) {
			CL_WriteSnapshot();
		}
	}
}


/*
====================
CL_WalkDemoExt
====================
*/
static int CL_WalkDemoExt( const char *arg, char *name, int name_len, fileHandle_t *handle )
{
	*handle = FS_INVALID_HANDLE;
	int i = 0;

	while ( demo_protocols[ i ] )
	{
		Com_sprintf( name, name_len, "demos/%s.%s%d", arg, DEMOEXT, demo_protocols[ i ] );
		FS_BypassPure();
		FS_FOpenFileRead( name, handle, qtrue );
		FS_RestorePure();
		if ( *handle != FS_INVALID_HANDLE )
		{
			Com_Log( SEV_INFO, LOG_CAT_CLIENT, "Demo file: %s\n", name );
			return demo_protocols[ i ];
		}
		else
			Com_Log( SEV_INFO, LOG_CAT_CLIENT, "Not found: %s\n", name );
		i++;
	}
	return -1;
}


/*
====================
CL_DemoExtCallback
====================
*/
static qboolean CL_DemoNameCallback_f( const char *filename, int length )
{
	const int ext_len = strlen( "." DEMOEXT );
	const int num_len = 2;

	if ( length <= ext_len + num_len || Q_stricmpn( filename + length - (ext_len + num_len), "." DEMOEXT, ext_len ) != 0 )
		return qfalse;

	int version = atoi( filename + length - num_len );
	if ( version == com_protocol->integer )
		return qtrue;

	if ( version > PROTOCOL_VERSION )
		return qfalse;

	return qtrue;
}


/*
====================
CL_CompleteDemoName
====================
*/
static void CL_CompleteDemoName(const char *args, int argNum )
{
	if ( argNum == 2 )
	{
		FS_SetFilenameCallback( CL_DemoNameCallback_f );
		Field_CompleteFilename( "demos", "." DEMOEXT "??", qfalse, FS_MATCH_ANY | FS_MATCH_STICK | FS_MATCH_SUBDIRS );
		FS_SetFilenameCallback( NULL );
	}
}


/*
====================
CL_PlayDemo_f

demo <demoname>

====================
*/
static void CL_PlayDemo_f( void ) {
	char		name[MAX_OSPATH];

	if ( Cmd_Argc() != 2 ) {
		Com_Log( SEV_INFO, LOG_CAT_CLIENT, "demo <demoname>\n" );
		return;
	}

	// open the demo file
	const char *arg = Cmd_Argv( 1 );

	// check for an extension .DEMOEXT_?? (?? is protocol)
	char *ext_test = strrchr(arg, '.');
	int protocol;
	fileHandle_t hFile = FS_INVALID_HANDLE;
	if ( ext_test && !Q_stricmpn(ext_test + 1, DEMOEXT, ARRAY_LEN(DEMOEXT) - 1) )
	{
		protocol = atoi(ext_test + ARRAY_LEN(DEMOEXT));

		int i;
		for ( i = 0; demo_protocols[ i ]; i++ )
		{
			if ( demo_protocols[ i ] == protocol )
				break;
		}

		if ( demo_protocols[ i ] || protocol == com_protocol->integer  )
		{
			Com_sprintf(name, sizeof(name), "demos/%s", arg);
			FS_BypassPure();
			FS_FOpenFileRead( name, &hFile, qtrue );
			FS_RestorePure();
		}
		else
		{
			char retry[MAX_OSPATH];
			size_t len;

			Com_Log( SEV_INFO, LOG_CAT_CLIENT, "Protocol %d not supported for demos\n", protocol );
			len = ext_test - arg;

			if ( len > ARRAY_LEN( retry ) - 1 ) {
				len = ARRAY_LEN( retry ) - 1;
			}

			Q_strncpyz( retry, arg, len + 1);
			retry[len] = '\0';
			protocol = CL_WalkDemoExt( retry, name, sizeof( name ), &hFile );
		}
	}
	else
		protocol = CL_WalkDemoExt( arg, name, sizeof( name ), &hFile );

	if ( hFile == FS_INVALID_HANDLE ) {
		COM_WARN( LOG_CAT_CLIENT, "couldn't open %s\n", name );
		// Honor the "nextdemo" cvar even if the current demo fails to
		// open so that a queued demo reel keeps advancing.
		CL_NextDemo();
		return;
	}

	FS_FCloseFile( hFile );
	hFile = FS_INVALID_HANDLE;

	// make sure a local server is killed
	// 2 means don't force disconnect of local client
	Cvar_Set( "sv_killserver", "2" );

	CL_Disconnect( qtrue );

	// clc.demofile will be closed during CL_Disconnect so reopen it
	if ( FS_FOpenFileRead( name, &clc.demofile, qtrue ) == -1 )
	{
		// drop this time
		COM_WARN( LOG_CAT_CLIENT, "couldn't open %s\n", name );
		// Honor the "nextdemo" cvar so a demo reel can progress past a
		// missing entry rather than tearing down with an ERR_DROP.
		CL_NextDemo();
		return;
	}

	const char *slash, *shortname;
	if ( (slash = strrchr( name, '/' )) != NULL )
		shortname = slash + 1;
	else
		shortname = name;

	Q_strncpyz( clc.demoName, shortname, sizeof( clc.demoName ) );

	Con_Close();

	cls.state = CA_CONNECTED;
	clc.demoplaying = qtrue;
	Q_strncpyz( cls.servername, shortname, sizeof( cls.servername ) );

	// read demo messages until connected
#ifdef USE_CURL
	while ( cls.state >= CA_CONNECTED && cls.state < CA_PRIMED && !Com_DL_InProgress( &download ) ) {
#else
	while ( cls.state >= CA_CONNECTED && cls.state < CA_PRIMED ) {
#endif
		CL_ReadDemoMessage();
	}

	// don't get the first snapshot this frame, to prevent the long
	// time from the gamestate load from messing causing a time skip
	clc.firstDemoFrameSkipped = qfalse;
}


/*
==================
CL_NextDemo

Called when a demo or cinematic finishes
If the "nextdemo" cvar is set, that command will be issued
==================
*/
static void CL_NextDemo( void ) {
	char v[ MAX_CVAR_VALUE_STRING ];

	Cvar_VariableStringBuffer( "nextdemo", v, sizeof( v ) );
	Com_Log( SEV_DEBUG, LOG_CAT_CLIENT, "CL_NextDemo: %s\n", v );
	if ( !v[0] ) {
		return;
	}

	Cvar_Set( "nextdemo", "" );
	Cbuf_AddText( v );
	Cbuf_AddText( "\n" );
	Cbuf_Execute();
}


//======================================================================

/*
=====================
CL_ShutdownVMs
=====================
*/
static void CL_ShutdownVMs( void )
{
	CL_ShutdownCGame();
	CL_ShutdownUI();
}


/*
=====================
CL_ShutdownLevel

Level-scoped client teardown.  Tears down the cgame VM and frees level-scoped
renderer resources (world surfaces, lightmaps, level models, level audio), but
does NOT touch the Wired UI VM, Vulkan device/context, font atlas, console
buffers, or any other persistent subsystem.

Called from SV_SpawnServer P1 (map transition).
Must NOT be called from process-exit paths — use CL_ShutdownAll for those.
=====================
*/
void CL_ShutdownLevel( void ) {
	if ( com_dedicated->integer ) {
		return;
	}

	// mute level sounds; audio mixer and device stay alive
	S_DisableSounds();

	// cgame VM is level-scoped; shut it down.
	// Wired UI VM is persistent — do NOT call CL_ShutdownUI() here.
	CL_ShutdownCGame();

	// Release level-scoped renderer resources.  REF_KEEP_CONTEXT preserves
	// the Vulkan device, GPU textures (font atlas, base shaders), and zone
	// memory so they remain valid between async spawn phases.
	// R_InitImages will destroy and re-register them when CL_InitRenderer
	// runs on client reconnect (P5).
	if ( re.Shutdown ) {
		re.Shutdown( REF_KEEP_CONTEXT );
	}
	// Signal CL_StartHunkUsers → CL_InitRenderer → RE_BeginRegistration so the
	// renderer is properly re-initialized (new backEndData, fresh shaders, new
	// font atlas) for the incoming map.
	cls.rendererStarted = qfalse;
	cls.wiredUIStarted  = qfalse;

	// sounds must be re-registered after each map load
	cls.soundRegistered = qfalse;

	// do NOT call SCR_Done() — screen state must stay alive for inter-phase frames
}


/*
=====================
Called by Com_GameRestart, CL_FlushMemory and engine quit / fatal error paths.

CL_ShutdownAll

Full client teardown.  Calls CL_ShutdownLevel() first for the level-scoped
work, then destroys all persistent state: Wired UI VM, renderer device, font
atlases, console, audio mixer.

Do NOT call this on map transitions — use CL_ShutdownLevel() instead.
=====================
*/
void CL_ShutdownAll( void ) {

#ifdef USE_CURL
	CL_cURL_Shutdown();
#endif

	// level-scoped teardown first (mutes sounds, kills cgame, frees level geo)
	CL_ShutdownLevel();

	// shutdown remaining persistent VMs — Wired UI VM
	CL_ShutdownUI();

	// CL_ShutdownLevel already called re.Shutdown(REF_KEEP_CONTEXT) to release
	// level resources.  For a game-switch, also destroy the window and GL/Vk
	// context entirely.  For the non-switch path, the REF_KEEP_CONTEXT call
	// from CL_ShutdownLevel is sufficient — don't call it again.
	if ( re.Shutdown && CL_GameSwitch() ) {
		CL_ShutdownRef( REF_DESTROY_WINDOW );
	}

	cls.rendererStarted = qfalse;
	cls.wiredUIStarted  = qfalse;
	cls.soundRegistered = qfalse;

	SCR_Done();
}


/*
=================
CL_ClearMemory
=================
*/
void CL_ClearMemory( void ) {
	// if not running a server clear the whole hunk
	if ( !com_sv_running->integer ) {
		// clear the level-scoped hunk (persistent arenas survive)
		Hunk_ClearLevel();
		// clear collision map data
		CM_ClearMap();
	} else {
		// clear all the client data on the hunk
		Hunk_ClearToMark();
	}
}


/*
=================
CL_FlushMemory

Called by CL_Disconnect_f, CL_DownloadsComplete
Also called by Com_Error
=================
*/
void CL_FlushMemory( void ) {

	// shutdown all the client stuff
	CL_ShutdownAll();

	CL_ClearMemory();

	BSP_ClearMapCache();

	CL_StartHunkUsers();
}


/*
=====================
CL_MapLoading

A local server is starting to load a map, so update the
screen to let the user know about it, then dump all client
memory on the hunk from cgame, ui, and renderer
=====================
*/
void CL_MapLoading( const char *mapname ) {
	if ( com_dedicated->integer ) {
		cls.state = CA_DISCONNECTED;
		Key_SetCatcher( KEYCATCH_CONSOLE );
		return;
	}

	if ( !com_cl_running->integer ) {
		return;
	}

	// Soft-close: collapse console visually but preserve KEYCATCH_CONSOLE so
	// the user sees the log wall throughout the async spawn phases (Fix 6.1).
	Con_SoftClose();
	// Preserve the console catcher; drop all others (UI, cgame, etc.).
	Key_SetCatcher( Key_GetCatcher() & KEYCATCH_CONSOLE );

	qboolean localReconnect = ( cls.state >= CA_CONNECTED && !Q_stricmp( cls.servername, "localhost" ) );

	// if we are already connected to the local host, stay connected
	if ( localReconnect ) {
		cls.state = CA_CONNECTED;		// so the connect screen is drawn
		memset( cls.updateInfoString, 0, sizeof( cls.updateInfoString ) );
		memset( clc.serverMessage, 0, sizeof( clc.serverMessage ) );
		memset( &cl.gameState, 0, sizeof( cl.gameState ) );
		clc.lastPacketSentTime = cls.realtime - 9999;  // send packet immediately
	} else {
		// clear nextmap so the cinematic shutdown doesn't execute it
		Cvar_Set( "nextmap", "" );
		CL_Disconnect( qtrue );
		Q_strncpyz( cls.servername, "localhost", sizeof(cls.servername) );
		cls.state = CA_CONNECTING;		// so the connect screen is drawn
		Key_SetCatcher( Key_GetCatcher() & KEYCATCH_CONSOLE );
	}

	memset( &cl_loadProgress, 0, sizeof( cl_loadProgress ) );
	CL_ResetLoadingScreenState();
	CL_ClearMapInfo();
	CL_ClearBspPreview();

	cl_loadProgress.startTime = cls.realtime ? cls.realtime : 1;
	cl_loadProgress.phase = "initializing";

	// Load map metadata and BSP wireframe preview for the loading screen.
	// mapname comes from SV_SpawnServer parameter (cvar not set yet at this point).
	if ( mapname && mapname[0] ) {
		CL_BuildBspPreview( mapname );
		CL_LoadMapInfo( mapname );
		CL_ApplyLoadingTheme( &cl_mapInfo );
	}

	cls.framecount++;
	SCR_UpdateScreen();

	if ( !localReconnect ) {
		clc.connectTime = -RETRANSMIT_TIMEOUT;
		NET_StringToAdr( cls.servername, &clc.serverAddress, NA_UNSPEC );
		// we don't need a challenge on the localhost
		CL_CheckForResend();
	}
}


/*
=====================
CL_ClearState

Called before parsing a gamestate
=====================
*/
void CL_ClearState( void ) {

//	S_StopAllSounds();

	memset( &cl, 0, sizeof( cl ) );
}


/*
====================
CL_UpdateGUID

update cl_guid using cdkey and optional prefix
====================
*/
static void CL_UpdateGUID( const char *prefix, int prefix_len )
{
	Cvar_Set( "cl_guid", Com_MD5Buf( &cl_cdkey[0], sizeof(cl_cdkey), prefix, prefix_len));
}


/*
=====================
CL_ResetOldGame
=====================
*/
void CL_ResetOldGame( void )
{
	cl_oldGameSet = qfalse;
	cl_oldGame[0] = '\0';
}


/*
=====================
CL_RestoreOldGame

change back to previous fs_game
=====================
*/
static qboolean CL_RestoreOldGame( void )
{
	if ( cl_oldGameSet )
	{
		cl_oldGameSet = qfalse;
		Cvar_Set( "fs_game", cl_oldGame );
		FS_ConditionalRestart( clc.checksumFeed, qtrue );
		return qtrue;
	}
	return qfalse;
}


/*
=====================
CL_Disconnect

Called when a connection, demo, or cinematic is being terminated.
Goes from a connected state to either a menu state or a console state
Sends a disconnect message to the server
This is also called on Com_Error and Com_Quit, so it shouldn't cause any errors
=====================
*/
qboolean CL_Disconnect( qboolean showMainMenu ) {
	static qboolean cl_disconnecting = qfalse;
	qboolean cl_restarted = qfalse;

	if ( !com_cl_running || !com_cl_running->integer ) {
		return cl_restarted;
	}

	if ( cl_disconnecting ) {
		return cl_restarted;
	}

	cl_disconnecting = qtrue;

	// Stop demo recording
	if ( clc.demorecording ) {
		CL_StopRecord_f();
	}

	// Stop demo playback
	if ( clc.demofile != FS_INVALID_HANDLE ) {
		FS_FCloseFile( clc.demofile );
		clc.demofile = FS_INVALID_HANDLE;
	}

	// Finish downloads
	if ( clc.download != FS_INVALID_HANDLE ) {
		FS_FCloseFile( clc.download );
		clc.download = FS_INVALID_HANDLE;
	}
	*clc.downloadTempName = *clc.downloadName = '\0';
	Cvar_Set( "cl_downloadName", "" );

	// Stop recording any video
	if ( CL_VideoRecording() ) {
		// Finish rendering current frame
		cls.framecount++;
		SCR_UpdateScreen();
		CL_CloseAVI( qfalse );
	}

	if ( cgvm ) {
		// do that right after we rendered last video frame
		CL_ShutdownCGame();
	}

	SCR_StopCinematic();
	S_StopAllSounds();
	Key_ClearStates();

	if ( UI_VM_ACTIVE && showMainMenu ) {
		UI_CALL_SET_ACTIVE( UIMENU_NONE );
	}

	// Remove pure paks
	FS_PureServerSetLoadedPaks( "", "" );
	FS_PureServerSetReferencedPaks( "", "" );

	FS_ClearPakReferences( FS_GENERAL_REF | FS_UI_REF | FS_CGAME_REF );

	if ( CL_GameSwitch() ) {
		// keep current gamestate and connection
		cl_disconnecting = qfalse;
		return qfalse;
	}

	// send a disconnect message to the server
	// send it a few times in case one is dropped
	if ( cls.state >= CA_CONNECTED && cls.state != CA_CINEMATIC && !clc.demoplaying ) {
		CL_AddReliableCommand( "disconnect", qtrue );
		CL_WritePacket( 2 );
	}

	CL_ClearState();
	memset( &cl_loadProgress, 0, sizeof( cl_loadProgress ) );
	CL_ResetLoadingScreenState();
	CL_ClearMapInfo();
	CL_ClearBspPreview();

	// wipe the client connection
	// Tear down client QUIC connection before wiping clc
	Com_Log( SEV_INFO, LOG_CAT_CLIENT, "*** CL_Disconnect: state=%d showMainMenu=%d initialized=%d ***\n",
		(int)cls.state, (int)showMainMenu, (int)WN_ClientIsConnecting() );
	WN_ClientDisconnect();

	memset( &clc, 0, sizeof( clc ) );
	clc.wiredRconChallenge[0] = '\0';

	cls.state = CA_DISCONNECTED;

	// not connected to a pure server anymore
	cl_connectedToPureServer = 0;

	CL_UpdateGUID( NULL, 0 );

	// Cmd_RemoveCommand( "callvote" );
	Cmd_RemoveCgameCommands();

	if ( noGameRestart )
		noGameRestart = qfalse;
	else
		cl_restarted = CL_RestoreOldGame();

	cl_disconnecting = qfalse;

#ifndef DEDICATED
	// Plan C hook: surface any ERR_DROP error as a Wired UI dialog.
	// Four guards prevent reentry and false positives:
	//   showMainMenu  — only when we are going back to the menu, not during
	//                   silent disconnects (map change, demo end, etc.)
	//   cls.uiStarted — UI must be running (early-boot disconnects skip this)
	//   !com_errorEntered — ERR_DROP longjmp is still in progress when this
	//                       is set; calling into UI would crash mid-teardown
	//   com_errorMessage  — nothing to show if there's no error text
	{
		const char *errMsg = Cvar_VariableString( "com_errorMessage" );
		if ( showMainMenu
		  && cls.uiStarted
		  && !com_errorEntered
		  && errMsg && errMsg[0] ) {
			CL_WiredUI_ShowError( "Disconnected", errMsg, qtrue );
		}
	}
#endif

	return cl_restarted;
}


/*
===================
CL_ForwardCommandToServer

adds the current command line as a clientCommand
things like godmode, noclip, etc, are commands directed to the server,
so when they are typed in at the console, they will need to be forwarded.
===================
*/
void CL_ForwardCommandToServer( const char *string ) {
	const char *cmd = Cmd_Argv( 0 );

	// ignore key up commands
	if ( cmd[0] == '-' ) {
		return;
	}

	// no userinfo updates from command line
	if ( !strcmp( cmd, "userinfo" ) ) {
		return;
	}

	if ( clc.demoplaying || cls.state < CA_CONNECTED || cmd[0] == '+' ) {
		Com_Log( SEV_INFO, LOG_CAT_CLIENT, "Unknown command \"%s" S_COLOR_WHITE "\"\n", cmd );
		return;
	}

	if ( Cmd_Argc() > 1 ) {
		CL_AddReliableCommand( string, qfalse );
	} else {
		CL_AddReliableCommand( cmd, qfalse );
	}
}


/*
===================
CL_RequestMotd

===================
*/
#if 0
static void CL_RequestMotd( void ) {
	char		info[MAX_INFO_STRING];

	if ( !cl_motd->integer ) {
		return;
	}
	Com_Log( SEV_INFO, LOG_CAT_CLIENT, "Resolving %s\n", UPDATE_SERVER_NAME );
	if ( !NET_StringToAdr( UPDATE_SERVER_NAME, &cls.updateServer, NA_IP ) ) {
		Com_Log( SEV_INFO, LOG_CAT_CLIENT, "Couldn't resolve address\n" );
		return;
	}
	cls.updateServer.port = BigShort( PORT_UPDATE );
	Com_Log( SEV_INFO, LOG_CAT_CLIENT, "%s resolved to %i.%i.%i.%i:%i\n", UPDATE_SERVER_NAME,
		cls.updateServer.ip[0], cls.updateServer.ip[1],
		cls.updateServer.ip[2], cls.updateServer.ip[3],
		BigShort( cls.updateServer.port ) );

	info[0] = 0;
	// NOTE TTimo xoring against Com_Milliseconds, otherwise we may not have a true randomization
	// only srand I could catch before here is tr_noise.c l:26 srand(1001)
	// https://zerowing.idsoftware.com/bugzilla/show_bug.cgi?id=382
	// NOTE: the Com_Milliseconds xoring only affects the lower 16-bit word,
	//   but I decided it was enough randomization
	Com_sprintf( cls.updateChallenge, sizeof( cls.updateChallenge ), "%i", ((rand() << 16) ^ rand()) ^ Com_Milliseconds());

	Info_SetValueForKey( info, "challenge", cls.updateChallenge );
	Info_SetValueForKey( info, "renderer", cls.glconfig.renderer_string );
	Info_SetValueForKey( info, "version", com_version->string );

	NET_OutOfBandPrint( NS_CLIENT, &cls.updateServer, "getmotd \"%s\"\n", info );
}
#endif



/*
======================================================================

CONSOLE COMMANDS

======================================================================
*/

/*
==================
CL_ForwardToServer_f
==================
*/
static void CL_ForwardToServer_f( void ) {
	if ( cls.state != CA_ACTIVE || clc.demoplaying ) {
		Com_Log( SEV_INFO, LOG_CAT_CLIENT, "Not connected to a server.\n");
		return;
	}

	if ( Cmd_Argc() <= 1 || strcmp( Cmd_Argv( 1 ), "userinfo" ) == 0 )
		return;

	// don't forward the first argument
	CL_AddReliableCommand( Cmd_ArgsFrom( 1 ), qfalse );
}

static void CL_RconLogin_f( void ) {
	if ( Cmd_Argc() > 1 ) {
		Cvar_Set( "cl_wiredRconPassword", Cmd_Argv( 1 ) );
	}

	if ( !cl_wiredRconPassword ) {
		static const cvarDesc_t d = CVAR_STRING( "cl_wiredRconPassword", "", CVAR_TEMP,
			"Wired RCON password used for challenge-response authentication." );
		cl_wiredRconPassword = Cvar_Register( &d );
	}

	if ( !cl_wiredRconPassword->string[0] ) {
		Com_Log( SEV_INFO, LOG_CAT_CLIENT, "Usage: rcon_login <password>\n" );
		return;
	}

	if ( clc.serverAddress.type == NA_BAD ) {
		Com_Log( SEV_INFO, LOG_CAT_CLIENT, "Not connected to a server.\n" );
		return;
	}

	clc.wiredRconAuthed = qfalse;
	clc.wiredRconHasChallenge = qfalse;
	clc.wiredRconChallenge[0] = '\0';
	clc.wiredRconAddress = clc.serverAddress;

	NET_OutOfBandPrint( NS_CLIENT, &clc.serverAddress, "rcon_auth" );
	Com_Log( SEV_INFO, LOG_CAT_CLIENT, "Wired RCON: requesting challenge...\n" );
}

static void CL_Rcon_f( void ) {
	char cmd[2048];

	if ( Cmd_Argc() < 2 ) {
		Com_Log( SEV_INFO, LOG_CAT_CLIENT, "Usage: rcon <lua code>\n" );
		return;
	}

	if ( Cmd_Argc() >= 2 && !Q_stricmp( Cmd_Argv( 1 ), "login" ) ) {
		if ( Cmd_Argc() >= 3 ) {
			Cbuf_AddText( va( "rcon_login %s\n", Cmd_Argv( 2 ) ) );
		} else {
			Cbuf_AddText( "rcon_login\n" );
		}
		return;
	}

	if ( !clc.wiredRconAuthed ) {
		Com_Log( SEV_INFO, LOG_CAT_CLIENT, "Plaintext rcon disabled. Use rcon_login.\n" );
		return;
	}

	Com_sprintf( cmd, sizeof( cmd ), "rcon %s", Cmd_ArgsFrom( 1 ) );
	NET_OutOfBandPrint( NS_CLIENT, &clc.wiredRconAddress, "%s", cmd );
}


/*
==================
CL_Disconnect_f
==================
*/
void CL_Disconnect_f( void ) {
	SCR_StopCinematic();
	Cvar_Set( "ui_singlePlayerActive", "0" );
	if ( cls.state != CA_DISCONNECTED && cls.state != CA_CINEMATIC ) {
		if ( cgvm && cgvm->callLevel ) {
			Com_Terminate( TERM_CLIENT_LEAVE, "Disconnected from server" );
		} else {
			// clear any previous "server full" type messages
			clc.serverMessage[0] = '\0';
			if ( com_sv_running && com_sv_running->integer ) {
				// if running a local server, kill it
				SV_Shutdown( "Disconnected from server" );
			} else {
				Com_Log( SEV_INFO, LOG_CAT_CLIENT, "Disconnected from %s\n", cls.servername );
			}
			Com_ClearLastError();
			if ( !CL_Disconnect( qfalse ) ) { // restart client if not done already
				CL_FlushMemory();
			}
			if ( UI_VM_ACTIVE ) {
				UI_CALL_SET_ACTIVE( UIMENU_MAIN );
			}
		}
	}
}


/*
================
CL_Reconnect_f
================
*/
static void CL_Reconnect_f( void ) {
	if ( cl_reconnectArgs->string[0] == '\0' || Q_stricmp( cl_reconnectArgs->string, "localhost" ) == 0 )
		return;
	Cvar_Set( "ui_singlePlayerActive", "0" );
	Cbuf_AddText( va( "connect %s\n", cl_reconnectArgs->string ) );
}


/*
================
CL_Connect_f
================
*/
static void CL_Connect_f( void ) {
	int argc = Cmd_Argc();
	netadrtype_t family = NA_UNSPEC;

	if ( argc != 2 && argc != 3 ) {
		Com_Log( SEV_INFO, LOG_CAT_CLIENT, "usage: connect [-4|-6] <server>\n");
		return;
	}

	const char *server;
	if ( argc == 2 ) {
		server = Cmd_Argv(1);
	} else {
		if( !strcmp( Cmd_Argv(1), "-4" ) )
			family = NA_IP;
#if FEAT_IPV6
		else if( !strcmp( Cmd_Argv(1), "-6" ) )
			family = NA_IP6;
		else
			COM_WARN( LOG_CAT_CLIENT, "warning: only -4 or -6 as address type understood.\n" );
#else
			COM_WARN( LOG_CAT_CLIENT, "warning: only -4 as address type understood.\n" );
#endif
		server = Cmd_Argv(2);
	}

	char	buffer[ sizeof( cls.servername ) ];  // same length as cls.servername
	Q_strncpyz( buffer, server, sizeof( buffer ) );
	server = buffer;

	// skip leading "q3a:/" in connection string
	if ( !Q_stricmpn( server, "q3a:/", 5 ) ) {
		server += 5;
	}

	// skip all slash prefixes
	while ( *server == '/' ) {
		server++;
	}

	int len = strlen( server );
	if ( len <= 0 ) {
		return;
	}

	// some programs may add ending slash
	if ( buffer[len-1] == '/' ) {
		buffer[len-1] = '\0';
	}

	if ( !*server ) {
		return;
	}

	// try resolve remote server first
	netadr_t addr;
	if ( !NET_StringToAdr( server, &addr, family ) ) {
		COM_WARN( LOG_CAT_CLIENT, "Bad server address - %s\n", server );
		return;
	}

	// save arguments for reconnect
	char args[ sizeof( cls.servername ) + MAX_CVAR_VALUE_STRING ];
	Q_strncpyz( args, Cmd_ArgsFrom( 1 ), sizeof( args ) );

	Cvar_Set( "ui_singlePlayerActive", "0" );

	// clear any previous "server full" type messages
	clc.serverMessage[0] = '\0';

	// if running a local server, kill it
	if ( com_sv_running->integer && !strcmp( server, "localhost" ) ) {
		SV_Shutdown( "Server quit" );
	}

	// make sure a local server is killed
	Cvar_Set( "sv_killserver", "1" );
	SV_Frame( 0 );

	noGameRestart = qtrue;
	CL_Disconnect( qtrue );
	Con_Close();

	Q_strncpyz( cls.servername, server, sizeof( cls.servername ) );

	// copy resolved address
	clc.serverAddress = addr;

	if (clc.serverAddress.port == 0) {
		clc.serverAddress.port = BigShort( PORT_SERVER );
	}

	const char *serverString = NET_AdrToStringwPort( &clc.serverAddress );

	Com_Log( SEV_INFO, LOG_CAT_CLIENT, "%s resolved to %s\n", cls.servername, serverString );

	if ( cl_guidServerUniq->integer )
		CL_UpdateGUID( serverString, strlen( serverString ) );
	else
		CL_UpdateGUID( NULL, 0 );

	// QUIC handles auth via TLS; LAN no longer needs the UDP challenge round-trip.
	cls.state = CA_CONNECTING;
	Com_RandomBytes( (byte*)&clc.challenge, sizeof( clc.challenge ) );

	Key_SetCatcher( 0 );
	clc.connectTime = -99999;	// CL_CheckForResend() will fire immediately
	clc.connectPacketCount = 0;

	Cvar_Set( "cl_reconnectArgs", args );

	// server connection string
	Cvar_Set( "cl_currentServerAddress", server );
}


/*
=================
CL_SendPureChecksums
=================
*/
static void CL_SendPureChecksums( void ) {
	char cMsg[ MAX_STRING_CHARS-1 ];

	if ( !cl_connectedToPureServer || clc.demoplaying )
		return;

	// if we are pure we need to send back a command with our referenced pk3 checksums
	int len = sprintf( cMsg, "cp %d ", cl.serverId );
	strcpy( cMsg + len, FS_ReferencedPakPureChecksums( sizeof( cMsg ) - len - 1 ) );

	CL_AddReliableCommand( cMsg, qfalse );
}


/*
=================
CL_ResetPureClientAtServer
=================
*/
static void CL_ResetPureClientAtServer( void ) {
	CL_AddReliableCommand( "vdr", qfalse );
}


/*
=================
CL_Vid_Restart

Restart the video subsystem

we also have to reload the UI and CGame because the renderer
doesn't know what graphics to reload
=================
*/
static void CL_Vid_Restart( refShutdownCode_t shutdownCode ) {

	// Settings may have changed so stop recording now
	if ( CL_VideoRecording() )
		CL_CloseAVI( qfalse );

	if ( clc.demorecording )
		CL_StopRecord_f();

	// clear and mute all sounds until next registration
	S_DisableSounds();

	// shutdown VMs
	CL_ShutdownVMs();

#if FEAT_WIRED_UI
	WiredUI_Shutdown();
#endif

	// shutdown the renderer and clear the renderer interface
	CL_ShutdownRef( shutdownCode ); // REF_KEEP_CONTEXT, REF_KEEP_WINDOW, REF_DESTROY_WINDOW

	// client is no longer pure until new checksums are sent
	CL_ResetPureClientAtServer();

	// clear pak references
	FS_ClearPakReferences( FS_UI_REF | FS_CGAME_REF );

	// reinitialize the filesystem if the game directory or checksum has changed
	if ( !clc.demoplaying ) // -EC-
		FS_ConditionalRestart( clc.checksumFeed, qfalse );

	cls.soundRegistered = qfalse;

	// unpause so the cgame definitely gets a snapshot and renders a frame
	Cvar_Set( "cl_paused", "0" );

	CL_ClearMemory();

	// startup all the client stuff
	CL_StartHunkUsers();

	// start the cgame if connected
	if ( ( cls.state > CA_CONNECTED && cls.state != CA_CINEMATIC ) || cls.startCgame ) {
		cls.cgameStarted = qtrue;
		CL_InitCGame();
		// send pure checksums
		CL_SendPureChecksums();
	}

	cls.startCgame = qfalse;
}


/*
=================
CL_Vid_Restart_f

Wrapper for CL_Vid_Restart
=================
*/
static void CL_Vid_Restart_f( void ) {

	if ( Q_stricmp( Cmd_Argv( 1 ), "keep_window" ) == 0 || Q_stricmp( Cmd_Argv( 1 ), "fast" ) == 0 ) {
		// fast path: keep window
		CL_Vid_Restart( REF_KEEP_WINDOW );
	} else {
		if ( cls.lastVidRestart ) {
			if ( abs( cls.lastVidRestart - Sys_Milliseconds() ) < 500 ) {
				// hack: do not allow vid restart right after cgame init
				return;
			}
		}
		CL_Vid_Restart( REF_DESTROY_WINDOW );
	}
}


/*
=================
CL_Snd_Restart_f

Restart the sound subsystem
The cgame and game must also be forced to restart because
handles will be invalid
=================
*/
static void CL_Snd_Restart_f( void )
{
	S_Shutdown();

	// sound will be reinitialized by vid_restart
	CL_Vid_Restart( REF_KEEP_CONTEXT /*REF_KEEP_WINDOW*/ );
}


/*
==================
CL_PK3List_f
==================
*/
void CL_OpenedPK3List_f( void ) {
	Com_Log( SEV_INFO, LOG_CAT_CLIENT, "Opened PK3 Names: %s\n", FS_LoadedPakNames());
}


/*
==================
CL_PureList_f
==================
*/
static void CL_ReferencedPK3List_f( void ) {
	Com_Log( SEV_INFO, LOG_CAT_CLIENT, "Referenced PK3 Names: %s\n", FS_ReferencedPakNames() );
}


/*
==================
CL_Configstrings_f
==================
*/
static void CL_Configstrings_f( void ) {
	if ( cls.state != CA_ACTIVE ) {
		Com_Log( SEV_INFO, LOG_CAT_CLIENT, "Not connected to a server.\n");
		return;
	}

	for ( int i = 0 ; i < MAX_CONFIGSTRINGS ; i++ ) {
		int ofs = cl.gameState.stringOffsets[ i ];
		if ( !ofs ) {
			continue;
		}
		Com_Log( SEV_INFO, LOG_CAT_CLIENT, "%4i: %s\n", i, cl.gameState.stringData + ofs );
	}
}


/*
==============
CL_Clientinfo_f
==============
*/
static void CL_Clientinfo_f( void ) {
	Com_Log( SEV_INFO, LOG_CAT_CLIENT, "--------- Client Information ---------\n" );
	Com_Log( SEV_INFO, LOG_CAT_CLIENT, "state: %i\n", cls.state );
	Com_Log( SEV_INFO, LOG_CAT_CLIENT, "Server: %s\n", cls.servername );
	Com_Log( SEV_INFO, LOG_CAT_CLIENT, "User info settings:\n");
	Info_Print( Cvar_InfoString( CVAR_USERINFO, NULL ) );
	Com_Log( SEV_INFO, LOG_CAT_CLIENT, "--------------------------------------\n" );
}


/*
==============
CL_Serverinfo_f
==============
*/
static void CL_Serverinfo_f( void ) {
	int ofs = cl.gameState.stringOffsets[ CS_SERVERINFO ];
	if ( !ofs )
		return;

	Com_Log( SEV_INFO, LOG_CAT_CLIENT, "Server info settings:\n" );
	Info_Print( cl.gameState.stringData + ofs );
}


/*
===========
CL_Systeminfo_f
===========
*/
static void CL_Systeminfo_f( void ) {
	int ofs = cl.gameState.stringOffsets[ CS_SYSTEMINFO ];
	if ( !ofs )
		return;

	Com_Log( SEV_INFO, LOG_CAT_CLIENT, "System info settings:\n" );
	Info_Print( cl.gameState.stringData + ofs );
}


static void CL_CompleteCallvote(const char *args, int argNum )
{
	if( argNum >= 2 )
	{
		// Skip "callvote "
		const char *p = Com_SkipTokens( args, 1, " " );

		if ( p > args )
			Field_CompleteCommand( p, qtrue, qtrue );
	}
}


//====================================================================

/*
=================
CL_DownloadsComplete

Called when all downloading has been completed
=================
*/
static void CL_DownloadsComplete( void ) {

#ifdef USE_CURL
	// if we downloaded with cURL
	if ( clc.cURLUsed ) {
		clc.cURLUsed = qfalse;
		CL_cURL_Shutdown();
		if ( clc.cURLDisconnected ) {
			if ( clc.downloadRestart ) {
				FS_Restart( clc.checksumFeed );
				clc.downloadRestart = qfalse;
			}
			clc.cURLDisconnected = qfalse;
			CL_Reconnect_f();
			return;
		}
	}
#endif

	// if we downloaded files we need to restart the file system
	if ( clc.downloadRestart ) {
		clc.downloadRestart = qfalse;

		FS_Restart(clc.checksumFeed); // We possibly downloaded a pak, restart the file system to load it

		// inform the server so we get new gamestate info
		CL_AddReliableCommand( "donedl", qfalse );

		// by sending the donedl command we request a new gamestate
		// so we don't want to load stuff yet
		return;
	}

	// let the client game init and load data
	cls.state = CA_LOADING;

	// Pump the loop, this may change gamestate!
	Com_EventLoop();

	// if the gamestate was changed by calling Com_EventLoop
	// then we loaded everything already and we don't want to do it again.
	if ( cls.state != CA_LOADING ) {
		return;
	}

	// flush client memory and start loading stuff
	// this will also (re)load the UI
	// if this is a local client then only the client part of the hunk
	// will be cleared, note that this is done after the hunk mark has been set
	//if ( !com_sv_running->integer )
	CL_FlushMemory();

	// initialize the CGame
	cls.cgameStarted = qtrue;
	CL_InitCGame();

	if ( clc.demofile == FS_INVALID_HANDLE ) {
		Cmd_AddCommand( "callvote", NULL );
		Cmd_SetCommandCompletionFunc( "callvote", CL_CompleteCallvote );
	}

	// set pure checksums
	CL_SendPureChecksums();
	WN_ClientSendReady();

	CL_WritePacket( 2 );
}


/*
=================
CL_BeginDownload

Requests a file to download from the server.  Stores it in the current
game directory.
=================
*/
static void CL_BeginDownload( const char *localName, const char *remoteName ) {

	Com_Log( SEV_DEBUG, LOG_CAT_CLIENT, "***** CL_BeginDownload *****\n"
				"Localname: %s\n"
				"Remotename: %s\n"
				"****************************\n", localName, remoteName);

	Q_strncpyz ( clc.downloadName, localName, sizeof(clc.downloadName) );
	Com_sprintf( clc.downloadTempName, sizeof(clc.downloadTempName), "%s.tmp", localName );

	// Set so UI gets access to it
	Cvar_Set( "cl_downloadName", remoteName );
	Cvar_Set( "cl_downloadSize", "0" );
	Cvar_Set( "cl_downloadCount", "0" );
	Cvar_SetIntegerValue( "cl_downloadTime", cls.realtime );

	clc.downloadBlock = 0; // Starting new file
	clc.downloadCount = 0;

	CL_AddReliableCommand( va("download %s", remoteName), qfalse );
}


/*
=================
CL_NextDownload

A download completed or failed
=================
*/
void CL_NextDownload( void )
{
	// A download has finished, check whether this matches a referenced checksum
	if(*clc.downloadName)
	{
		const char *zippath = FS_BuildOSPath(Cvar_VariableString("fs_homepath"), clc.downloadName, NULL );

		if(!FS_CompareZipChecksum(zippath))
			Com_Terminate( TERM_CLIENT_DROP, "Incorrect checksum for file: %s", clc.downloadName);
	}

	*clc.downloadTempName = *clc.downloadName = '\0';
	Cvar_Set("cl_downloadName", "");

	// We are looking to start a download here
	if (*clc.downloadList) {
		char *s = clc.downloadList;
		char *remoteName, *localName;
		qboolean useCURL = qfalse;

		// format is:
		//  @remotename@localname@remotename@localname, etc.

		if (*s == '@')
			s++;
		remoteName = s;

		if ( (s = strchr(s, '@')) == NULL ) {
			CL_DownloadsComplete();
			return;
		}

		*s++ = '\0';
		localName = s;
		if ( (s = strchr(s, '@')) != NULL )
			*s++ = '\0';
		else
			s = localName + strlen(localName); // point at the null byte

#ifdef USE_CURL
		if(!(cl_allowDownload->integer & DLF_NO_REDIRECT)) {
			if(clc.sv_allowDownload & DLF_NO_REDIRECT) {
				Com_Log( SEV_INFO, LOG_CAT_CLIENT, "WARNING: server does not "
					"allow download redirection "
					"(sv_allowDownload is %d)\n",
					clc.sv_allowDownload);
			}
			else if(!*clc.sv_dlURL) {
				Com_Log( SEV_INFO, LOG_CAT_CLIENT, "WARNING: server allows "
					"download redirection, but does not "
					"have sv_dlURL set\n");
			}
			else if(!CL_cURL_Init()) {
				Com_Log( SEV_INFO, LOG_CAT_CLIENT, "WARNING: could not load "
					"cURL library\n");
			}
			else {
				CL_cURL_BeginDownload(localName, va("%s/%s",
					clc.sv_dlURL, remoteName));
				useCURL = qtrue;
			}
		}
		else if(!(clc.sv_allowDownload & DLF_NO_REDIRECT)) {
			Com_Log( SEV_INFO, LOG_CAT_CLIENT, "WARNING: server allows download "
				"redirection, but it disabled by client "
				"configuration (cl_allowDownload is %d)\n",
				cl_allowDownload->integer);
		}
#endif /* USE_CURL */

		if( !useCURL ) {
		if( (cl_allowDownload->integer & DLF_NO_UDP) ) {
				Com_Terminate( TERM_CLIENT_DROP, "UDP Downloads are "
					"disabled on your client. "
					"(cl_allowDownload is %d)",
					cl_allowDownload->integer);
				return;
			}
			else {
				CL_BeginDownload( localName, remoteName );
			}
		}
		clc.downloadRestart = qtrue;

		// move over the rest
		memmove( clc.downloadList, s, strlen(s) + 1 );

		return;
	}

	CL_DownloadsComplete();
}


/*
=================
CL_SetupQuicNetchan

Phase D removed the UDP netchan handshake, but the client still uses the
trimmed netchan state for packet pacing, packet-history bookkeeping, and to
select the QUIC send path in CL_WritePacket. Seed the same fields that the old
Netchan_Setup path used to initialize.

Note: address type is left as-is (NA_IP, NA_IP6, NA_LOOPBACK).  NA_QUIC /
NA_QUIC6 are transport-internal types; use (clc.quic_conn != CONN_INVALID) to
test whether we are on a QUIC connection.
=================
*/
static void CL_SetupQuicNetchan( void )
{
	clc.netchan.remoteAddress = clc.serverAddress;
	clc.netchan.incomingSequence = 0;
	clc.netchan.outgoingSequence = 1;
	clc.netchan.isLANAddress = Sys_IsLANAddress( &clc.netchan.remoteAddress );
}


/*
=================
CL_InitDownloads

After receiving a valid game state, we valid the cgame and local zip files here
and determine if we need to download them
=================
*/
void CL_InitDownloads( void ) {

	if ( !(cl_allowDownload->integer & DLF_ENABLE) )
	{
		char missingfiles[ MAXPRINTMSG ];

		// autodownload is disabled on the client
		// but it's possible that some referenced files on the server are missing
		if ( FS_ComparePaks( missingfiles, sizeof( missingfiles ), qfalse ) )
		{
			// NOTE TTimo I would rather have that printed as a modal message box
			// but at this point while joining the game we don't know whether we will successfully join or not
			Com_Log( SEV_INFO, LOG_CAT_CLIENT, "\nWARNING: You are missing some files referenced by the server:\n%s"
				"You might not be able to join the game\n"
				"Go to the setting menu to turn on autodownload, or get the file elsewhere\n\n", missingfiles );
		}
	}
	else if ( FS_ComparePaks( clc.downloadList, sizeof( clc.downloadList ) , qtrue ) ) {

		Com_Log( SEV_INFO, LOG_CAT_CLIENT, "Need paks: %s\n", clc.downloadList );

		if ( *clc.downloadList ) {
			// if autodownloading is not enabled on the server
			cls.state = CA_CONNECTED;

			*clc.downloadTempName = *clc.downloadName = '\0';
			Cvar_Set( "cl_downloadName", "" );

			CL_NextDownload();
			return;
		}

	}

#ifdef USE_CURL
	if ( cl_mapAutoDownload->integer && ( !(clc.sv_allowDownload & DLF_ENABLE) || clc.demoplaying ) )
	{
		const char *info, *mapname, *bsp;

		// get map name and BSP file name
		info = cl.gameState.stringData + cl.gameState.stringOffsets[ CS_SERVERINFO ];
		mapname = Info_ValueForKey( info, "mapname" );
		bsp = va( "maps/%s.bsp", mapname );

		if ( FS_FOpenFileRead( bsp, NULL, qfalse ) == -1 )
		{
			if ( CL_Download( "dlmap", mapname, qtrue ) )
			{
				cls.state = CA_CONNECTED; // prevent continue loading and shows the ui download progress screen
				return;
			}
		}
	}
#endif // USE_CURL

	CL_DownloadsComplete();
}


/*
=================
CL_CheckForResend

Resend a connect message if the last one has timed out
=================
*/
static void CL_CheckForResend( void ) {

	// don't send anything if playing back a demo
	if ( clc.demoplaying ) {
		return;
	}

	// resend if we haven't gotten a reply yet
	if ( cls.state != CA_CONNECTING ) {
		return;
	}

	if ( cls.realtime - clc.connectTime < RETRANSMIT_TIMEOUT ) {
		return;
	}

	clc.connectTime = cls.realtime;	// for retransmit requests
	clc.connectPacketCount++;

	switch ( cls.state ) {
	case CA_CONNECTING:
		// QUIC path: skip the UDP challenge round-trip.
		// Build userinfo with challenge included (so server echoes it back in connectResponse),
		// then initiate QUIC handshake via transport->connect() if not already started.
		// "loopback" address string is handled inside wn_connect → WN_ClientConnect,
		// which maps it to 127.0.0.1 so picoquic can send real UDP datagrams.
		if ( !WN_ClientIsConnecting() ) {
			char   info[MAX_INFO_STRING * 2];
			qboolean truncated = qfalse;
			int qport = Cvar_VariableIntegerValue( "net_qport" );
			Q_strncpyz( info, Cvar_InfoString( CVAR_USERINFO, &truncated ), sizeof( info ) );

			// Embed client challenge so server echoes it back in connectResponse.
			Info_SetValueForKey_s( info, MAX_USERINFO_LENGTH, "challenge",
									va( "%i", clc.challenge ) );
			Info_SetValueForKey_s( info, MAX_USERINFO_LENGTH, "protocol",
									com_protocol->string );
			Info_SetValueForKey_s( info, MAX_USERINFO_LENGTH, "qport",
									va( "%i", qport ) );

			CL_SetupQuicNetchan();
			if ( transport ) {
				clc.quic_conn = transport->connect(
					NET_AdrToString( &clc.serverAddress ),
					(int)BigShort( clc.serverAddress.port ),
					info );
			}
		} else {
			// Already connecting — just pump timers (WN_ClientFrame is
			// also called from NET_Event, but belt-and-suspenders here).
			WN_ClientFrame();
		}
		break;

	default:
		break;
	}
}


/*
===================
CL_MotdPacket
===================
*/
static void CL_MotdPacket( const netadr_t *from ) {
	// if not from our server, ignore it
	if ( !NET_CompareAdr( from, &cls.updateServer ) ) {
		return;
	}

	const char *info = Cmd_Argv(1);

	// check challenge
	const char *challenge = Info_ValueForKey( info, "challenge" );
	if ( strcmp( challenge, cls.updateChallenge ) ) {
		return;
	}

	challenge = Info_ValueForKey( info, "motd" );

	Q_strncpyz( cls.updateInfoString, info, sizeof( cls.updateInfoString ) );
	Cvar_Set( "cl_motdString", challenge );
}


/*
===================
CL_InitServerInfo
===================
*/
static void CL_InitServerInfo( serverInfo_t *server, const netadr_t *address ) {
	server->adr = *address;
	server->clients = 0;
	server->hostName[0] = '\0';
	server->mapName[0] = '\0';
	server->maxClients = 0;
	server->maxPing = 0;
	server->minPing = 0;
	server->ping = -1;
	server->game[0] = '\0';
	server->gameType = 0;
	server->netType = 0;
	server->punkbuster = 0;
	server->g_humanplayers = 0;
	server->g_needpass = 0;
}

#define MAX_SERVERSPERPACKET	256

typedef struct hash_chain_s {
	netadr_t             addr;
	struct hash_chain_s *next;
} hash_chain_t;

static hash_chain_t *hash_table[1024];
static hash_chain_t hash_list[MAX_GLOBAL_SERVERS];
static unsigned int hash_count = 0;

static unsigned int hash_func( const netadr_t *addr ) {

	const byte		*ip = NULL;
	unsigned int	size;
	unsigned int	hash = 0;

	switch ( addr->type ) {
		case NA_IP:  ip = addr->ipv._4; size = 4;  break;
#if FEAT_IPV6
		case NA_IP6: ip = addr->ipv._6; size = 16; break;
#endif
		default: size = 0; break;
	}

	for ( unsigned int i = 0; i < size; i++ )
		hash = hash * 101 + (int)( *ip++ );

	hash = hash ^ ( hash >> 16 );

	return (hash & 1023);
}

static void hash_insert( const netadr_t *addr )
{
	hash_chain_t **tab, *cur;
	unsigned int hash;
	if ( hash_count >= MAX_GLOBAL_SERVERS )
		return;
	hash = hash_func( addr );
	tab = &hash_table[ hash ];
	cur = &hash_list[ hash_count++ ];
	cur->addr = *addr;
	if ( cur != *tab )
		cur->next = *tab;
	else
		cur->next = NULL;
	*tab = cur;
}

static void hash_reset( void )
{
	hash_count = 0;
	memset( hash_list, 0, sizeof( hash_list ) );
	memset( hash_table, 0, sizeof( hash_table ) );
}

static hash_chain_t *hash_find( const netadr_t *addr )
{
	hash_chain_t *cur;
	cur = hash_table[ hash_func( addr ) ];
	while ( cur != NULL ) {
		if ( NET_CompareAdr( addr, &cur->addr ) )
			return cur;
		cur = cur->next;
	}
	return NULL;
}


/*
===================
CL_ServersResponsePacket
===================
*/
static void CL_ServersResponsePacket( const netadr_t* from, msg_t *msg, qboolean extended ) {
	netadr_t addresses[MAX_SERVERSPERPACKET];

	//Com_Log( SEV_INFO, LOG_CAT_CLIENT, "CL_ServersResponsePacket\n"); // moved down

	if (cls.numglobalservers == -1) {
		// state to detect lack of servers or lack of response
		cls.numglobalservers = 0;
		cls.numGlobalServerAddresses = 0;
		hash_reset();
	}

	// parse through server response string
	int numservers = 0;
	byte *buffptr  = msg->data;
	byte *buffend  = buffptr + msg->cursize;

	// advance to initial token
	do
	{
		if(*buffptr == '\\' || (extended && *buffptr == '/'))
			break;

		buffptr++;
	} while (buffptr < buffend);

	while (buffptr + 1 < buffend)
	{
		// IPv4 address
		if (*buffptr == '\\')
		{
			buffptr++;

			if (buffend - buffptr < sizeof(addresses[numservers].ipv._4) + sizeof(addresses[numservers].port) + 1)
				break;

			for(int i = 0; i < (int)sizeof(addresses[numservers].ipv._4); i++)
				addresses[numservers].ipv._4[i] = *buffptr++;

			addresses[numservers].type = NA_IP;
		}
#if FEAT_IPV6
		// IPv6 address, if it's an extended response
		else if (extended && *buffptr == '/')
		{
			buffptr++;

			if (buffend - buffptr < sizeof(addresses[numservers].ipv._6) + sizeof(addresses[numservers].port) + 1)
				break;

			for(int i = 0; i < (int)sizeof(addresses[numservers].ipv._6); i++)
				addresses[numservers].ipv._6[i] = *buffptr++;

			addresses[numservers].type = NA_IP6;
			addresses[numservers].scope_id = from->scope_id;
		}
#endif
		else
			// syntax error!
			break;

		// parse out port
		addresses[numservers].port = (*buffptr++) << 8;
		addresses[numservers].port += *buffptr++;
		addresses[numservers].port = BigShort( addresses[numservers].port );

		// syntax check
		if (*buffptr != '\\' && *buffptr != '/')
			break;

		numservers++;
		if (numservers >= MAX_SERVERSPERPACKET)
			break;
	}

	int count = cls.numglobalservers;

	int i;
	for (i = 0; i < numservers && count < MAX_GLOBAL_SERVERS; i++) {

		// Tequila: It's possible to have sent many master server requests. Then
		// we may receive many times the same addresses from the master server.
		// We just avoid to add a server if it is still in the global servers list.
		if ( hash_find( &addresses[i] ) )
			continue;

		hash_insert( &addresses[i] );

		// build net address
		serverInfo_t *server = &cls.globalServers[count];

		CL_InitServerInfo( server, &addresses[i] );
		// advance to next slot
		count++;
	}

	// if getting the global list
	if ( count >= MAX_GLOBAL_SERVERS && cls.numGlobalServerAddresses < MAX_GLOBAL_SERVERS )
	{
		// if we couldn't store the servers in the main list anymore
		for (; i < numservers && cls.numGlobalServerAddresses < MAX_GLOBAL_SERVERS; i++)
		{
			// just store the addresses in an additional list
			cls.globalServerAddresses[cls.numGlobalServerAddresses++] = addresses[i];
		}
	}

	cls.numglobalservers = count;
	int total = count + cls.numGlobalServerAddresses;

	Com_Log( SEV_INFO, LOG_CAT_CLIENT, "getserversResponse:%3d servers parsed (total %d)\n", numservers, total);
}


/*
=================
CL_ConnectionlessPacket

Responses to broadcasts, etc

return true only for commands indicating that our server is alive
or connection sequence is going into the right way
=================
*/
static qboolean CL_ConnectionlessPacket( const netadr_t *from, msg_t *msg ) {
	int challenge = 0;

	MSG_BeginReadingOOB( msg );
	MSG_ReadLong( msg );	// skip the -1

	const char *s = MSG_ReadStringLine( msg );

	Cmd_TokenizeString( s );

	const char *c = Cmd_Argv(0);

	Com_Log( SEV_DEBUG, LOG_CAT_CLIENT, "CL packet %s: %s\n", NET_AdrToStringwPort( from ), s );

	// challenge from the server we are connecting to
	if ( !Q_stricmp(c, "challengeResponse" ) ) {

		if ( cls.state != CA_CONNECTING ) {
			Com_Log( SEV_DEBUG, LOG_CAT_CLIENT, "Unwanted challenge response received. Ignored.\n" );
			return qfalse;
		}

		c = Cmd_Argv( 2 );
		if ( *c != '\0' )
			challenge = atoi( c );

		s = Cmd_Argv( 3 ); // analyze server protocol version
		if ( *s != '\0' ) {
			int sv_proto = atoi( s );
		}

		if ( *c == '\0' || challenge != clc.challenge )
		{
			Com_Log( SEV_INFO, LOG_CAT_CLIENT, "Bad challenge for challengeResponse. Ignored.\n" );
			return qfalse;
		}

		// start sending connect instead of challenge request packets
		clc.challenge = atoi(Cmd_Argv(1));
		cls.state = CA_CHALLENGING;
		clc.connectPacketCount = 0;
		clc.connectTime = -99999;

		// take this address as the new server address.  This allows
		// a server proxy to hand off connections to multiple servers
		clc.serverAddress = *from;
		Com_Log( SEV_DEBUG, LOG_CAT_CLIENT, "challengeResponse: %d\n", clc.challenge );
		return qtrue;
	}

	if ( !Q_stricmp( c, "rconChallenge" ) ) {
		const char *challengeStr = Cmd_Argv( 1 );
		char hmacHex[ COM_SHA256_HEX_LEN + 1 ];

		if ( !cl_wiredRconPassword ) {
			static const cvarDesc_t d = CVAR_STRING( "cl_wiredRconPassword", "", CVAR_TEMP,
				"Wired RCON password used for challenge-response authentication." );
			cl_wiredRconPassword = Cvar_Register( &d );
		}

		if ( !challengeStr || strlen( challengeStr ) != 64 ) {
			Com_Log( SEV_INFO, LOG_CAT_CLIENT, "Wired RCON: invalid challenge received.\n" );
			return qfalse;
		}

		Q_strncpyz( clc.wiredRconChallenge, challengeStr, sizeof( clc.wiredRconChallenge ) );
		clc.wiredRconHasChallenge = qtrue;
		clc.wiredRconAddress = *from;

		Com_HMAC_SHA256_Hex( cl_wiredRconPassword ? cl_wiredRconPassword->string : "", clc.wiredRconChallenge, hmacHex );
		NET_OutOfBandPrint( NS_CLIENT, &clc.wiredRconAddress, "rcon_verify %s", hmacHex );
		return qfalse;
	}

	if ( !Q_stricmp( c, "rconAuthResult" ) ) {
		const char *result = Cmd_Argv( 1 );
		if ( !Q_stricmp( result, "ok" ) ) {
			clc.wiredRconAuthed = qtrue;
			Com_Log( SEV_INFO, LOG_CAT_CLIENT, "Wired RCON: authenticated.\n" );
		} else {
			clc.wiredRconAuthed = qfalse;
			Com_Log( SEV_INFO, LOG_CAT_CLIENT, "Wired RCON: authentication failed.\n" );
		}
		return qfalse;
	}

	/* Phase D: "connectResponse" OOB handler removed — QUIC uses TLV ACCEPT on stream 0.
	   CA_CONNECTED is set by CL_InitDownloads when bootstrap state arrives on
	   the reliable bootstrap channel. */

	// server responding to an info broadcast
	if ( !Q_stricmp(c, "infoResponse") ) {
		CL_ServerInfoPacket( from, msg );
		return qfalse;
	}

	// server responding to a get playerlist
	if ( !Q_stricmp(c, "statusResponse") ) {
		CL_ServerStatusResponse( from, msg );
		return qfalse;
	}

	// echo request from server
	if ( !Q_stricmp(c, "echo") ) {
		// NOTE: we may have to add exceptions for auth and update servers
		if ( NET_CompareAdr( from, &clc.serverAddress ) ) {
			NET_OutOfBandPrint( NS_CLIENT, from, "%s", Cmd_Argv(1) );
			return qtrue;
		}
		return qfalse;
	}

	// Phase 6.4: legacy "keyAuthorize" packet handler removed — q3now never
	// talks to the id authorize server, so any such packet is unsolicited.

	// global MOTD from id
	if ( !Q_stricmp(c, "motd") ) {
		CL_MotdPacket( from );
		return qfalse;
	}

	// print string from server
	if ( !Q_stricmp(c, "print") ) {
		// NOTE: we may have to add exceptions for auth and update servers
		if ( NET_CompareAdr( from, &clc.serverAddress ) ) {
			s = MSG_ReadString( msg );
			Q_strncpyz( clc.serverMessage, s, sizeof( clc.serverMessage ) );
			Com_Log( SEV_INFO, LOG_CAT_CLIENT, "%s", s );
			return qtrue;
		}
		return qfalse;
	}

	// list of servers sent back by a master server (classic)
	if ( !strncmp(c, "getserversResponse", 18) ) {
		CL_ServersResponsePacket( from, msg, qfalse );
		return qfalse;
	}

	// list of servers sent back by a master server (extended)
	if ( !strncmp(c, "getserversExtResponse", 21) ) {
		CL_ServersResponsePacket( from, msg, qtrue );
		return qfalse;
	}

	Com_Log( SEV_DEBUG, LOG_CAT_CLIENT, "Unknown connectionless packet command.\n" );
	return qfalse;
}


/*
=================
CL_PacketEvent

A packet has arrived from the main event loop
=================
*/
void CL_PacketEvent( const netadr_t *from, msg_t *msg ) {
	if ( msg->cursize < 4 )
		return;

	/* Phase D: Only OOB packets (server browser infoResponse/statusResponse) are handled here.
	   All game traffic flows through QUIC streams and datagrams. */
	if ( *(int *)msg->data == -1 ) {
		if ( CL_ConnectionlessPacket( from, msg ) )
			clc.lastPacketTime = cls.realtime;
	}
}


/*
==================
CL_CheckTimeout
==================
*/
static void CL_CheckTimeout( void ) {
	// On the transition from fully-paused (both cl_paused and sv_paused set) back
	// to running, refresh lastPacketTime.  Without this, the ~50 ms gap between
	// menu close and first QUIC snapshot looks like server silence and increments
	// cl.timeoutcount, causing a false disconnect after pauses longer than
	// cl_timeout seconds.  Stock Q3's in-memory loopback has zero transport
	// latency so it never sees this window; QUIC loopback does.
	static qboolean wasBothPaused = qfalse;
	qboolean isBothPaused = ( CL_CheckPaused() && sv_paused->integer );
	if ( wasBothPaused && !isBothPaused ) {
		clc.lastPacketTime = cls.realtime;
	}
	wasBothPaused = isBothPaused;

	//
	// check timeout
	//
	if ( ( !CL_CheckPaused() || !sv_paused->integer )
		&& cls.state >= CA_CONNECTED && cls.state != CA_CINEMATIC
		&& cls.realtime - clc.lastPacketTime > cl_timeout->integer * 1000 ) {
		if ( ++cl.timeoutcount > 5 ) { // timeoutcount saves debugger
			Com_Log( SEV_INFO, LOG_CAT_CLIENT, "\nServer connection timed out.\n" );
			Com_SetLastError( "Server connection timed out." );
			if ( !CL_Disconnect( qfalse ) ) { // restart client if not done already
				CL_FlushMemory();
			}
			if ( UI_VM_ACTIVE ) {
				UI_CALL_SET_ACTIVE( UIMENU_MAIN );
			}
			return;
		}
	} else {
		cl.timeoutcount = 0;
	}
}


/*
==================
CL_CheckPaused
Check whether client has been paused.
==================
*/
qboolean CL_CheckPaused( void )
{
	if(cl_paused->integer)
		return qtrue;

	return qfalse;
}


/*
==================
CL_NoDelay
==================
*/
qboolean CL_NoDelay( void )
{
	if ( CL_VideoRecording() || ( com_timedemo->integer && clc.demofile != FS_INVALID_HANDLE ) )
		return qtrue;

	return qfalse;
}


/*
==================
CL_CheckUserinfo
==================
*/
static void CL_CheckUserinfo( void ) {

	// don't add reliable commands when not yet connected
	if ( cls.state < CA_CONNECTED )
		return;

	// don't overflow the reliable command buffer when paused
	if ( CL_CheckPaused() )
		return;

	// send a reliable userinfo update if needed
	if ( cvar_modifiedFlags & CVAR_USERINFO )
	{
		qboolean infoTruncated = qfalse;
		const char *info;

		cvar_modifiedFlags &= ~CVAR_USERINFO;

		info = Cvar_InfoString( CVAR_USERINFO, &infoTruncated );
		if ( strlen( info ) > MAX_USERINFO_LENGTH || infoTruncated ) {
			COM_WARN( LOG_CAT_CLIENT, "WARNING: oversize userinfo, you might be not able to play on remote server!\n" );
		}

		CL_AddReliableCommand( va( "userinfo \"%s\"", info ), qfalse );
	}
}


/*
==================
CL_CheckMatchAlerts

CNQ3 backport: raise OS-level attention (taskbar flash, beep, audio
unmute) when a match is about to start while the window is unfocused
or minimized.

We detect the transition by polling the CS_WARMUP configstring:
  - warmup > 0   : countdown running
  - warmup == 0  : waiting for warmup
  - warmup < 0   : match has started (server wrote -|restarttime|)

The engine has no direct "match started" event, so we store the
previous CS_WARMUP value and fire the alert on any warmup -> match or
warmup -> new-warmup-value transition.
==================
*/
static int  cl_lastWarmupValue = 0;
static int  cl_matchAlertExpire = 0;

static void CL_CheckMatchAlerts( void )
{
	qboolean fire = qfalse;

	/* clear the s_autoMute override once the alert window closes */
	if ( cl_matchAlertExpire > 0 && Sys_Milliseconds() >= cl_matchAlertExpire ) {
		cl_matchAlertExpire = 0;
		S_SetMuteOverride( qfalse );
	}

	if ( cls.state != CA_ACTIVE ) {
		cl_lastWarmupValue = 0;
		return;
	}

	if ( cl_matchAlerts == NULL || cl_matchAlerts->integer == 0 )
		return;

	/* read CS_WARMUP from the current gamestate */
	if ( CS_WARMUP < 0 || CS_WARMUP >= MAX_CONFIGSTRINGS )
		return;

	int ofs = cl.gameState.stringOffsets[ CS_WARMUP ];
	if ( ofs == 0 )
		return;

	const char *s = cl.gameState.stringData + ofs;
	int warmup = atoi( s );

	/* trigger whenever the warmup value transitions — either warmup
	   begins (0 -> positive), a new restart time is scheduled
	   (positive -> new positive), or warmup ends (positive -> 0 or
	   anything -> negative for "match live") */
	if ( warmup != cl_lastWarmupValue ) {
		/* only treat transitions that actually represent a match start
		   event — avoid firing on the initial gamestate parse where
		   cl_lastWarmupValue is still 0 and warmup is 0 */
		if ( cl_lastWarmupValue != 0 || warmup != 0 ) {
			fire = qtrue;
		}
		cl_lastWarmupValue = warmup;
	}

	if ( !fire )
		return;

	int bits = cl_matchAlerts->integer;

	/* bit 1: require window to be either minimized, or (with bit 1 set)
	   unfocused but not minimized either way we only alert when the
	   user is not actively looking. */
	if ( gw_active && !gw_minimized ) {
		/* user is actively looking at the window — no alert needed */
		return;
	}
	if ( !gw_minimized && !(bits & 1) ) {
		/* unfocused but not minimized, and bit 1 not set → skip */
		return;
	}

	if ( bits & 2 ) {
		Sys_FlashWindow();
	}
	if ( bits & 4 ) {
		Sys_BeepAttention();
	}
	if ( bits & 8 ) {
		S_SetMuteOverride( qtrue );
		cl_matchAlertExpire = Sys_Milliseconds() + 5000;
	}
}


/*
===================
CL_WuiTestError_f
===================
Dev command: `wui_testerror [message]`
Directly exercises the error_popup.wmenu dialog without requiring a real
network failure.  Only compiled in debug builds.
*/
#ifndef NDEBUG
static void CL_WuiTestError_f( void )
{
	const char *msg;
	if ( Cmd_Argc() > 1 )
		msg = Cmd_ArgsFrom( 1 );
	else
		msg = "Synthetic test error — Copy, Retry, Back to menu all work";
	Com_SetLastError( "%s", msg );
	CL_WiredUI_ShowError( "Test Error", msg, qtrue );
}
#endif

/*
=====================
CL_CheckConnectError
=====================
Polls QUIC client for a deferred connect-phase failure and surfaces it as
an error_popup.wmenu modal.  Called every frame just before CL_CheckTimeout,
which only fires at CA_CONNECTED+ and would otherwise never see these.

Two cases after CL_Disconnect(qfalse):

  cls.uiStarted == qtrue  — remote connect, no local server was started.
      The renderer and UI survived.  Call CL_WiredUI_ShowError directly.

  cls.uiStarted == qfalse — SV_SpawnServer called CL_ShutdownAll, which
      shut down the renderer and WiredUI.  The screen is black; nobody will
      call CL_StartHunkUsers unless we do it explicitly.  Store the error
      in cl_pendingConnectError and call CL_FlushMemory() to restart the
      renderer and UI — the deferred check inside CL_StartHunkUsers fires
      and shows the dialog as soon as WiredUI is back up.
*/
static char cl_pendingConnectError[512];

static void CL_CheckConnectError( void )
{
	char msg[512];

	// EB7: guard covers CA_CONNECTING + CA_CHALLENGING and any state
	// between disconnected and fully active.
	if ( cls.state <= CA_DISCONNECTED || cls.state >= CA_ACTIVE )
		return;
	if ( !WN_ClientHasError( msg, sizeof(msg) ) )
		return;

	// Consume before CL_Disconnect so the error slot is clean on retry.
	WN_ClientClearError();

	COM_WARN( LOG_CAT_CLIENT, "Connect failed: %s\n", msg );
	Com_SetLastError( "%s", msg );
	CL_Disconnect( qfalse );

	if ( cls.uiStarted ) {
		// UI is still up — show the error dialog directly.
		UI_CALL_SET_ACTIVE( UIMENU_MAIN );
		CL_WiredUI_ShowError( "Connection Failed", msg, qtrue );
	} else {
		// Renderer is down (SV_SpawnServer wiped it).  Restart everything
		// and let the deferred check in CL_StartHunkUsers show the dialog.
		Q_strncpyz( cl_pendingConnectError, msg, sizeof( cl_pendingConnectError ) );
		CL_FlushMemory();
	}
}

/*
==================
CL_Frame
==================
*/
void CL_Frame( int msec, int realMsec ) {

#ifdef USE_CURL
	if ( download.cURL ) {
		Com_DL_Perform( &download );
	}
#endif

	if ( !com_cl_running->integer ) {
		return;
	}

#if FEAT_WIRED_UI
	CL_PROF(store, WiredStore_BeginFrame());
#endif

	// save the msec before checking pause
	cls.realFrametime = realMsec;

#ifdef USE_CURL
	if ( clc.downloadCURLM ) {
		CL_cURL_PerformDownload();
		// we can't process frames normally when in disconnected
		// download mode since the ui vm expects cls.state to be
		// CA_CONNECTED
		if ( clc.cURLDisconnected ) {
			cls.frametime = msec;
			cls.realtime += msec;
			cls.framecount++;
			SCR_UpdateScreen();
			S_Update( realMsec );
			Con_RunConsole();
			return;
		}
	}
#endif

	if ( cls.state == CA_DISCONNECTED && !( Key_GetCatcher( ) & KEYCATCH_UI )
		&& !com_sv_running->integer && UI_VM_ACTIVE ) {
		// if disconnected, bring up the menu
		S_StopAllSounds();
		UI_CALL_SET_ACTIVE( UIMENU_MAIN );
	}

	// if recording an avi, lock to a fixed fps
	if ( CL_VideoRecording() && msec ) {
		// save the current screen
		if ( cls.state == CA_ACTIVE || cl_forceavidemo->integer ) {
			float fps, frameDuration;

			if ( com_timescale->value > 0.0001f )
				fps = MIN( cl_aviFrameRate->value / com_timescale->value, 1000.0f );
			else
				fps = 1000.0f;

			frameDuration = MAX( 1000.0f / fps, 1.0f ) + clc.aviVideoFrameRemainder;

			CL_TakeVideoFrame();

			msec = (int)frameDuration;
			clc.aviVideoFrameRemainder = frameDuration - msec;

			realMsec = msec; // sync sound duration
		}
	}

	if ( cl_autoRecordDemo->integer && !clc.demoplaying ) {
		if ( cls.state == CA_ACTIVE && !clc.demorecording ) {
			// If not recording a demo, and we should be, start one
			qtime_t	now;
			char		mapName[ MAX_QPATH ];
			char		serverName[ MAX_OSPATH ];

			Com_RealTime( &now );
			const char *nowString = va( "%04d%02d%02d%02d%02d%02d",
					1900 + now.tm_year,
					1 + now.tm_mon,
					now.tm_mday,
					now.tm_hour,
					now.tm_min,
					now.tm_sec );

			Q_strncpyz( serverName, cls.servername, MAX_OSPATH );
			// Replace the ":" in the address as it is not a valid
			// file name character
			char *p = strchr( serverName, ':' );
			if ( p ) {
				*p = '.';
			}

			Q_strncpyz( mapName, COM_SkipPath( cl.mapname ), sizeof( cl.mapname ) );
			COM_StripExtension(mapName, mapName, sizeof(mapName));

			Cbuf_ExecuteText( EXEC_NOW,
					va( "record %s-%s-%s", nowString, serverName, mapName ) );
		}
		else if ( cls.state != CA_ACTIVE && clc.demorecording ) {
			// Recording, but not CA_ACTIVE, so stop recording
			CL_StopRecord_f();
		}
	}

	// decide the simulation time
	cls.frametime = msec;
	cls.realtime += msec;

	if ( cl_timegraph->integer ) {
		SCR_DebugGraph( msec * 0.25f );
	}

	CL_PROF(userinfo, CL_CheckUserinfo());
	if ( !clc.demoplaying ) { CL_PROF(misc, CL_CheckConnectError()); }
	if ( !clc.demoplaying ) { CL_PROF(misc, CL_CheckTimeout()); }
	CL_PROF(send,    CL_SendCmd());
	CL_PROF(resend,  CL_CheckForResend());
	CL_PROF(cgtime,  CL_SetCGameTime());
	CL_PROF(misc,    CL_CheckMatchAlerts());
	cls.framecount++;
	SCR_UpdateScreen();
	CL_PROF(sound,   S_Update( realMsec ));
	CL_PROF(misc,    SCR_RunCinematic());
	CL_PROF(misc,    Con_RunConsole());
}


//============================================================================

/*
============
CL_ShutdownRef
============
*/
static void CL_ShutdownRef( refShutdownCode_t code ) {

#ifdef USE_RENDERER_DLOPEN
	if ( s_cl_renderer_mod != -1 && cl_renderer->modificationCount != s_cl_renderer_mod ) {
		code = REF_UNLOAD_DLL;
	}
#endif

	// clear and mute all sounds until next registration
	// S_DisableSounds();

	if ( code >= REF_DESTROY_WINDOW ) { // +REF_UNLOAD_DLL
		// shutdown sound system before renderer
		// because it may depend from window handle
		S_Shutdown();
	}

	SCR_Done();

	if ( re.Shutdown ) {
		re.Shutdown( code );
	}

#ifdef USE_RENDERER_DLOPEN
	if ( rendererLib ) {
		Sys_UnloadLibrary( rendererLib );
		rendererLib = NULL;
	}
#endif

	memset( &re, 0, sizeof( re ) );

	cls.rendererStarted = qfalse;
	cls.wiredUIStarted  = qfalse;
}


/*
============
CL_InitRenderer
============
*/
static void CL_InitRenderer( void ) {

	// fixup renderer -EC-
	if ( !re.BeginRegistration ) {
		CL_InitRef();
	}

	// this sets up the renderer and calls R_Init
	re.BeginRegistration( &cls.glconfig );

	// load character sets
	cls.whiteShader = re.RegisterShader( "white" );
	cls.consoleShader = re.RegisterShader( "console" );

	Con_CheckResize();

	g_console_field_width = ((cls.glconfig.vidWidth / smallchar_width)) - 2;
	g_consoleField.widthInChars = g_console_field_width;

	// for 640x480 virtualized screen
	cls.biasY = 0;
	cls.biasX = 0;
	if ( cls.glconfig.vidWidth * 480 > cls.glconfig.vidHeight * 640 ) {
		// wide screen, scale by height
		cls.scale = cls.glconfig.vidHeight * (1.0/480.0);
		cls.biasX = 0.5 * ( cls.glconfig.vidWidth - ( cls.glconfig.vidHeight * (640.0/480.0) ) );
	} else {
		// no wide screen, scale by width
		cls.scale = cls.glconfig.vidWidth * (1.0/640.0);
		cls.biasY = 0.5 * ( cls.glconfig.vidHeight - ( cls.glconfig.vidWidth * (480.0/640) ) );
	}

	SCR_Init();
}


/*
============================
CL_StartHunkUsers

After the server has cleared the hunk, these will need to be restarted
This is the only place that any of these functions are called from
============================
*/
void CL_StartHunkUsers( void ) {

	if ( !com_cl_running || !com_cl_running->integer ) {
		return;
	}

	if ( cls.state >= CA_LOADING ) {
		// try to apply map-depending configuration from cvar cl_mapConfig_<mapname> cvars
		const char *info = cl.gameState.stringData + cl.gameState.stringOffsets[ CS_SERVERINFO ];
		const char *mapname = Info_ValueForKey( info, "mapname" );
		if ( mapname && *mapname != '\0' ) {
			const char *fmt = "cl_mapConfig_%s";
			const char *cmd = Cvar_VariableString( va( fmt, mapname ) );
			if ( cmd && *cmd != '\0' ) {
				Cbuf_AddText( cmd );
				Cbuf_AddText( "\n" );
			} else {
				// apply mapname "default" if present
				cmd = Cvar_VariableString( va( fmt, "default" ) );
				if ( cmd && *cmd != '\0' ) {
					Cbuf_AddText( cmd );
					Cbuf_AddText( "\n" );
				}
			}
		}
	}

	if ( !cls.rendererStarted ) {
		cls.rendererStarted = qtrue;
		CL_InitRenderer();
		CL_Characters_RegisterIcons();
	}

	if ( !cls.soundStarted ) {
		cls.soundStarted = qtrue;
		S_Init();
	}

	if ( !cls.soundRegistered ) {
		cls.soundRegistered = qtrue;
		S_BeginRegistration();
	}

#if FEAT_WIRED_UI
	if ( !cls.wiredUIStarted ) {
		cls.wiredUIStarted = qtrue;
		WiredUI_Init( cls.state >= CA_AUTHORIZING && cls.state < CA_ACTIVE );
	}
#endif

	if ( !cls.uiStarted ) {
		cls.uiStarted = qtrue;

		// Show any connect error that was deferred across the UI restart
		// triggered by CL_Disconnect() inside CL_CheckConnectError.
		if ( cl_pendingConnectError[0] ) {
			char pendingMsg[512];
			Q_strncpyz( pendingMsg, cl_pendingConnectError, sizeof( pendingMsg ) );
			cl_pendingConnectError[0] = '\0';
			UI_CALL_SET_ACTIVE( UIMENU_MAIN );
			CL_WiredUI_ShowError( "Connection Failed", pendingMsg, qtrue );
		}
	}
}


/*
============
CL_RefMalloc
============
*/
static void *CL_RefMalloc( size_t size ) {
	return Z_TagMalloc( size, TAG_RENDERER );
}


/*
============
CL_RefFreeAll
============
*/
static void CL_RefFreeAll( void ) {
	Z_FreeTags( TAG_RENDERER );
}


/*
============
CL_ScaledMilliseconds
============
*/
int CL_ScaledMilliseconds( void ) {
	return Sys_Milliseconds()*com_timescale->value;
}


/*
============
CL_IsMinimized
============
*/
static qboolean CL_IsMininized( void ) {
	return gw_minimized;
}


/*
============
CL_SetScaling

Sets console chars height
============
*/
static void CL_SetScaling( float factor, int captureWidth, int captureHeight ) {

	cls.con_factor = factor;

	// set custom capture resolution
	cls.captureWidth = captureWidth;
	cls.captureHeight = captureHeight;
}


/*
============
CL_InitRef
============
*/
static void QDECL RI_Log( log_severity_t severity, const char *fmt, ... ) {
	va_list args;
	va_start( args, fmt );
	Com_Logv( severity, LOG_CAT_RENDERER, fmt, args );
	va_end( args );
}

static void CL_InitRef( void ) {
	refimport_t	rimp;
	refexport_t	*ret;
#ifdef USE_RENDERER_DLOPEN
	GetRefAPI_t		GetRefAPI;
	char			dllName[ MAX_OSPATH ], *ospath;
#endif

	CL_InitGLimp_Cvars();

	Com_Log( SEV_INFO, LOG_CAT_CLIENT, "----- Initializing Renderer ----\n" );

#ifdef USE_RENDERER_DLOPEN

#if defined (__linux__) && defined(__i386__)
#define REND_ARCH_STRING "x86"
#else
#define REND_ARCH_STRING ARCH_STRING
#endif

	Com_sprintf( dllName, sizeof( dllName ), RENDERER_PREFIX "_%s_" REND_ARCH_STRING DLL_EXT, cl_renderer->string );
	// FS_GetInstallBinaryPath returns Contents/MacOS on macOS, fs_installpath
	// elsewhere — covers both the .app bundle and the flat-dir layouts.
	ospath = FS_BuildOSPath( FS_GetInstallBinaryPath(), dllName, NULL );
	rendererLib = Sys_LoadLibrary( ospath );
	if ( !rendererLib )
	{
		Cvar_ForceReset( "cl_renderer" );
		Com_sprintf( dllName, sizeof( dllName ), RENDERER_PREFIX "_%s_" REND_ARCH_STRING DLL_EXT, cl_renderer->string );
		ospath = FS_BuildOSPath( FS_GetInstallBinaryPath(), dllName, NULL );
		rendererLib = Sys_LoadLibrary( ospath );
		if ( !rendererLib )
		{
			Com_Terminate( TERM_UNRECOVERABLE, "Failed to load renderer %s", dllName );
		}
	}

	GetRefAPI = Sys_LoadFunction( rendererLib, "GetRefAPI" );
	if( !GetRefAPI )
	{
		Com_Terminate( TERM_UNRECOVERABLE, "Can't load symbol GetRefAPI" );
		return;
	}

	s_cl_renderer_mod = cl_renderer->modificationCount;
#endif

	memset( &rimp, 0, sizeof( rimp ) );

	rimp.Cmd_AddCommand = Cmd_AddCommand;
	rimp.Cmd_RemoveCommand = Cmd_RemoveCommand;
	rimp.Cmd_Argc = Cmd_Argc;
	rimp.Cmd_Argv = Cmd_Argv;
	rimp.Cmd_ExecuteText = Cbuf_ExecuteText;
	rimp.Log = RI_Log;
	rimp.Terminate = Com_Terminate;
	rimp.Milliseconds = CL_ScaledMilliseconds;
	rimp.Microseconds = Sys_Microseconds;
	rimp.Malloc = CL_RefMalloc;
	rimp.FreeAll = CL_RefFreeAll;
	rimp.Free = Z_Free;
#ifdef HUNK_DEBUG
	rimp.Hunk_AllocDebug = Hunk_AllocDebug;
#else
	rimp.Hunk_Alloc = Hunk_Alloc;
#endif
	rimp.Hunk_AllocateTempMemory = Hunk_AllocateTempMemory;
	rimp.Hunk_FreeTempMemory = Hunk_FreeTempMemory;

	rimp.CM_ClusterPVS = CM_ClusterPVS;
	rimp.CM_PointContents = CM_PointContents;
	rimp.CM_NumBrushes = CM_NumBrushes;
	rimp.CM_GetBrushData = CM_GetBrushData;
	rimp.CM_GetBrushSideData = CM_GetBrushSideData;
	rimp.CM_DrawDebugSurface = CM_DrawDebugSurface;

	rimp.FS_ReadFile = FS_ReadFile;
	rimp.FS_FreeFile = FS_FreeFile;
	rimp.FS_WriteFile = FS_WriteFile;
	rimp.FS_FreeFileList = FS_FreeFileList;
	rimp.FS_ListFiles = FS_ListFiles;
	//rimp.FS_FileIsInPAK = FS_FileIsInPAK;
	rimp.FS_FileExists = FS_FileExists;

	rimp.BSP_Load = BSP_Load;
	rimp.BSP_Free = BSP_Free;

	rimp.Cvar_Get = Cvar_Get;
	rimp.Cvar_Set = Cvar_Set;
	rimp.Cvar_SetValue = Cvar_SetValue;
	rimp.Cvar_CheckRange = Cvar_CheckRange;
	rimp.Cvar_SetDescription = Cvar_SetDescription;
	rimp.Cvar_VariableStringBuffer = Cvar_VariableStringBuffer;
	rimp.Cvar_VariableString = Cvar_VariableString;
	rimp.Cvar_VariableIntegerValue = Cvar_VariableIntegerValue;

	rimp.Cvar_SetGroup = Cvar_SetGroup;
	rimp.Cvar_CheckGroup = Cvar_CheckGroup;
	rimp.Cvar_ResetGroup = Cvar_ResetGroup;

	// cinematic stuff

	rimp.CIN_UploadCinematic = CIN_UploadCinematic;
	rimp.CIN_PlayCinematic = CIN_PlayCinematic;
	rimp.CIN_RunCinematic = CIN_RunCinematic;

	rimp.CL_WriteAVIVideoFrame = CL_WriteAVIVideoFrame;
	rimp.CL_SaveJPGToBuffer = CL_SaveJPGToBuffer;
	rimp.CL_SaveJPG = CL_SaveJPG;
	rimp.CL_LoadJPG = CL_LoadJPG;

	rimp.CL_IsMinimized = CL_IsMininized;
	rimp.CL_SetScaling = CL_SetScaling;

	rimp.Sys_SetClipboardBitmap = Sys_SetClipboardBitmap;
	rimp.Sys_LowPhysicalMemory = Sys_LowPhysicalMemory;
	rimp.Com_RealTime = Com_RealTime;

	rimp.GetCharacterSkin = CL_GetCharacterSkin;
	rimp.AssetLog_Event = AssetLog_Event;

	rimp.GLimp_InitGamma = GLimp_InitGamma;
	rimp.GLimp_SetGamma = GLimp_SetGamma;

	// OpenGL API
#ifdef USE_OPENGL_API
	rimp.GLimp_Init = GLimp_Init;
	rimp.GLimp_Shutdown = GLimp_Shutdown;
	rimp.GL_GetProcAddress = GL_GetProcAddress;
	rimp.GLimp_EndFrame = GLimp_EndFrame;
#endif

	// Vulkan API
#ifdef USE_VULKAN_API
	rimp.VKimp_Init = VKimp_Init;
	rimp.VKimp_Shutdown = VKimp_Shutdown;
	rimp.VK_GetInstanceProcAddr = VK_GetInstanceProcAddr;
	rimp.VK_CreateSurface = VK_CreateSurface;
#endif

	ret = GetRefAPI( REF_API_VERSION, &rimp );

	Com_Log( SEV_INFO, LOG_CAT_CLIENT, "-------------------------------\n");

	if ( !ret ) {
		Com_Terminate( TERM_UNRECOVERABLE, "Couldn't initialize refresh" );
	}

	re = *ret;

	// unpause so the cgame definitely gets a snapshot and renders a frame
	Cvar_Set( "cl_paused", "0" );
}


//===========================================================================================


static void CL_SetChar_f( void ) {
	const char *arg;
	char name[ MAX_CVAR_VALUE_STRING ];
	char *slash;

	arg = Cmd_Argv( 1 );
	if ( arg[0] ) {
		// Support "char visor/gorre" shorthand — split on '/' to set skin too.
		char charBuf[ MAX_CVAR_VALUE_STRING ];
		Q_strncpyz( charBuf, arg, sizeof( charBuf ) );
		slash = strchr( charBuf, '/' );
		if ( slash ) {
			*slash = '\0';
			Cvar_Set( "skin", slash + 1 );
		}
		Cvar_Set( "char", charBuf );
	} else {
		Cvar_VariableStringBuffer( "char", name, sizeof( name ) );
		Com_Log( SEV_INFO, LOG_CAT_CLIENT, "char is set to %s\n", name );
	}
}

static void CL_SetSkin_f( void ) {
	const char *arg;
	char name[ MAX_CVAR_VALUE_STRING ];

	arg = Cmd_Argv( 1 );
	if ( arg[0] ) {
		Cvar_Set( "skin", arg );
	} else {
		Cvar_VariableStringBuffer( "skin", name, sizeof( name ) );
		Com_Log( SEV_INFO, LOG_CAT_CLIENT, "skin is set to %s\n", name );
	}
}


//===========================================================================================


/*
===============
CL_Video_f

video
video [filename]
===============
*/
static void CL_Video_f( void )
{
	char filename[ MAX_OSPATH ];
	const char *ext;
	qboolean pipe;

	if( !clc.demoplaying )
	{
		Com_Log( SEV_INFO, LOG_CAT_CLIENT, "The %s command can only be used when playing back demos\n", Cmd_Argv( 0 ) );
		return;
	}

	pipe = ( Q_stricmp( Cmd_Argv( 0 ), "video-pipe" ) == 0 );

	if ( pipe )
		ext = "mp4";
	else
		ext = "avi";

	if ( Cmd_Argc() == 2 )
	{
		// explicit filename
		Com_sprintf( filename, sizeof( filename ), "videos/%s", Cmd_Argv( 1 ) );

		// override video file extension
		if ( pipe )
		{
			char *sep = strrchr( filename, '/' ); // last path separator
			char *e = strrchr( filename, '.' );

			if ( e && e > sep && *(e+1) != '\0' ) {
				ext = e + 1;
				*e = '\0';
			}
		}
	}
	else
	{
		 // scan for a free filename
		int i;
		for ( i = 0; i <= 9999; i++ )
		{
			Com_sprintf( filename, sizeof( filename ), "videos/video%04d.%s", i, ext );
			if ( !FS_FileExists( filename ) )
				break; // file doesn't exist
		}

		if ( i > 9999 )
		{
			COM_ERROR( LOG_CAT_CLIENT, "ERROR: no free file names to create video\n" );
			return;
		}

		// without extension
		Com_sprintf( filename, sizeof( filename ), "videos/video%04d", i );
	}


	clc.aviSoundFrameRemainder = 0.0f;
	clc.aviVideoFrameRemainder = 0.0f;

	Q_strncpyz( clc.videoName, filename, sizeof( clc.videoName ) );
	clc.videoIndex = 0;

	CL_OpenAVIForWriting( va( "%s.%s", clc.videoName, ext ), pipe, qfalse );
}


/*
===============
CL_StopVideo_f
===============
*/
static void CL_StopVideo_f( void )
{
	CL_CloseAVI( qfalse );
}


/*
====================
CL_CompleteRecordName
====================
*/
static void CL_CompleteVideoName(const char *args, int argNum )
{
	if ( argNum == 2 )
	{
		Field_CompleteFilename( "videos", ".avi", qtrue, FS_MATCH_EXTERN | FS_MATCH_STICK );
	}
}

/*
** CL_GetModeInfo
*/
typedef struct vidmode_s
{
	const char	*description;
	int			width, height;
	float		pixelAspect;		// pixel width / height
} vidmode_t;

static const vidmode_t cl_vidModes[] =
{
	{ "Mode  0: 320x240",			320,	240,	1 },
	{ "Mode  1: 400x300",			400,	300,	1 },
	{ "Mode  2: 512x384",			512,	384,	1 },
	{ "Mode  3: 640x480",			640,	480,	1 },
	{ "Mode  4: 800x600",			800,	600,	1 },
	{ "Mode  5: 960x720",			960,	720,	1 },
	{ "Mode  6: 1024x768",			1024,	768,	1 },
	{ "Mode  7: 1152x864",			1152,	864,	1 },
	{ "Mode  8: 1280x1024 (5:4)",	1280,	1024,	1 },
	{ "Mode  9: 1600x1200",			1600,	1200,	1 },
	{ "Mode 10: 2048x1536",			2048,	1536,	1 },
	{ "Mode 11: 856x480 (wide)",	856,	480,	1 },
	// extra modes:
	{ "Mode 12: 1280x960",			1280,	960,	1 },
	{ "Mode 13: 1280x720",			1280,	720,	1 },
	{ "Mode 14: 1280x800 (16:10)",	1280,	800,	1 },
	{ "Mode 15: 1366x768",			1366,	768,	1 },
	{ "Mode 16: 1440x900 (16:10)",	1440,	900,	1 },
	{ "Mode 17: 1600x900",			1600,	900,	1 },
	{ "Mode 18: 1680x1050 (16:10)",	1680,	1050,	1 },
	{ "Mode 19: 1920x1080",			1920,	1080,	1 },
	{ "Mode 20: 1920x1200 (16:10)",	1920,	1200,	1 },
	{ "Mode 21: 2560x1080 (21:9)",	2560,	1080,	1 },
	{ "Mode 22: 3440x1440 (21:9)",	3440,	1440,	1 },
	{ "Mode 23: 3840x2160",			3840,	2160,	1 },
	{ "Mode 24: 4096x2160 (4K)",	4096,	2160,	1 }
};
static const int s_numVidModes = ARRAY_LEN( cl_vidModes );

qboolean CL_GetModeInfo( int *width, int *height, float *windowAspect, int mode, const char *modeFS, int dw, int dh, qboolean fullscreen )
{
	// set dedicated fullscreen mode
	if ( fullscreen && *modeFS )
		mode = atoi( modeFS );

	if ( mode < -2 )
		return qfalse;

	if ( mode >= s_numVidModes )
		return qfalse;

	// fix unknown desktop resolution
	if ( mode == -2 && (dw == 0 || dh == 0) )
		mode = 3;

	float pixelAspect;
	if ( mode == -2 ) { // desktop resolution
		*width = dw;
		*height = dh;
		pixelAspect = r_customPixelAspect->value;
	} else if ( mode == -1 ) { // custom resolution
		*width = r_customwidth->integer;
		*height = r_customheight->integer;
		pixelAspect = r_customPixelAspect->value;
	} else { // predefined resolution
		const vidmode_t *vm = &cl_vidModes[ mode ];
		*width  = vm->width;
		*height = vm->height;
		pixelAspect = vm->pixelAspect;
	}

	*windowAspect = (float)*width / ( *height * pixelAspect );

	return qtrue;
}


/*
** CL_ModeList_f
*/
static void CL_ModeList_f( void )
{
	Com_Log( SEV_INFO, LOG_CAT_CLIENT, "\n" );
	for ( int i = 0; i < s_numVidModes; i++ )
	{
		Com_Log( SEV_INFO, LOG_CAT_CLIENT, "%s\n", cl_vidModes[ i ].description );
	}
	Com_Log( SEV_INFO, LOG_CAT_CLIENT, "\n" );
}


#ifdef USE_RENDERER_DLOPEN
static qboolean isValidRenderer( const char *s ) {
	while ( *s ) {
		if ( !((*s >= 'a' && *s <= 'z') || (*s >= 'A' && *s <= 'Z') || (*s >= '1' && *s <= '9')) )
			return qfalse;
		++s;
	}
	return qtrue;
}
#endif


static const cvarDesc_t glimpDescs[] = {
	/* 0  */ CVAR_BOOL(   "r_allowSoftwareGL",  "0",               CVAR_LATCH,                   "Toggle the use of the default software OpenGL driver supplied by the Operating System." ),
	/* 1  */ CVAR_INT(    "r_swapInterval",      "0",               CVAR_ARCHIVE | CVAR_NODEFAULT,               "V-blanks to wait before swapping buffers.\n 0: No V-Sync\n 1: Synced to the monitor's refresh rate.", 0, 0 ),
	/* 2  */ CVAR_STRING( "r_glDriver",          OPENGL_DRIVER_NAME, CVAR_ARCHIVE | CVAR_NODEFAULT | CVAR_LATCH, "Specifies the OpenGL driver to use, will revert back to default if driver name set is invalid." ),
	/* 3  */ CVAR_INT(    "r_displayRefresh",    "0",               CVAR_LATCH,                   "Override monitor refresh rate in fullscreen mode:\n   0 - use current monitor refresh rate\n > 0 - use custom refresh rate", 0, 500 ),
	/* 4  */ CVAR_INT(    "vid_xpos",            "3",               CVAR_ARCHIVE,                  "Saves/sets window X-coordinate when windowed, requires \\vid_restart.", 0, 0 ),
	/* 5  */ CVAR_INT(    "vid_ypos",            "22",              CVAR_ARCHIVE,                  "Saves/sets window Y-coordinate when windowed, requires \\vid_restart.", 0, 0 ),
	/* 6  */ CVAR_BOOL(   "r_noborder",          "0",               CVAR_ARCHIVE | CVAR_NODEFAULT | CVAR_LATCH,  "Setting to 1 will remove window borders and title bar in windowed mode, hold ALT to drag & drop it with opened console." ),
	/* 7  */ CVAR_BOOL(   "r_customPixelAspect", "1",               CVAR_ARCHIVE | CVAR_NODEFAULT | CVAR_LATCH,  "Enables custom aspect of the screen, with \\r_mode -1." ),
	/* 8  */ CVAR_INT(    "r_customWidth",       "1600",            CVAR_ARCHIVE | CVAR_LATCH,     "Custom width to use with \\r_mode -1.", 0, 0 ),
	/* 9  */ CVAR_INT(    "r_customHeight",      "1024",            CVAR_ARCHIVE | CVAR_LATCH,     "Custom height to use with \\r_mode -1.", 0, 0 ),
	/* 10 */ CVAR_INT(    "r_colorbits",         "0",               CVAR_ARCHIVE | CVAR_NODEFAULT | CVAR_LATCH,  "Sets color bit depth, set to 0 to use desktop settings.", 0, 32 ),
	/* 11 */ CVAR_INT(    "r_stencilbits",       "8",               CVAR_ARCHIVE | CVAR_NODEFAULT | CVAR_LATCH,  "Stencil buffer size, required to be 8 for stencil shadows.", 0, 8 ),
	/* 12 */ CVAR_INT(    "r_depthbits",         "0",               CVAR_ARCHIVE | CVAR_NODEFAULT | CVAR_LATCH,  "Sets precision of Z-buffer.", 0, 32 ),
	/* 13 */ CVAR_STRING( "r_drawBuffer",        "GL_BACK",         CVAR_CHEAT,                    "Specifies buffer to draw from: GL_FRONT or GL_BACK." ),
};

enum {
	GLIMP_ALLOW_SW_GL, GLIMP_SWAP_INTERVAL, GLIMP_GL_DRIVER, GLIMP_DISPLAY_REFRESH,
	GLIMP_VID_XPOS, GLIMP_VID_YPOS, GLIMP_NOBORDER, GLIMP_CUSTOM_PIXEL_ASPECT,
	GLIMP_CUSTOM_WIDTH, GLIMP_CUSTOM_HEIGHT, GLIMP_COLORBITS, GLIMP_STENCILBITS,
	GLIMP_DEPTHBITS, GLIMP_DRAWBUFFER,
	GLIMP_CVAR_COUNT
};

_Static_assert( ARRAY_LEN( glimpDescs ) == GLIMP_CVAR_COUNT, "glimpDescs/enum mismatch" );
static cvar_t *glimpHandles[GLIMP_CVAR_COUNT];


static void CL_InitGLimp_Cvars( void )
{
	Cvar_RegisterTable( glimpDescs, ARRAY_LEN( glimpDescs ), glimpHandles );
	r_allowSoftwareGL   = glimpHandles[GLIMP_ALLOW_SW_GL];
	r_swapInterval      = glimpHandles[GLIMP_SWAP_INTERVAL];
	r_glDriver          = glimpHandles[GLIMP_GL_DRIVER];
	r_displayRefresh    = glimpHandles[GLIMP_DISPLAY_REFRESH];
	vid_xpos            = glimpHandles[GLIMP_VID_XPOS];
	vid_ypos            = glimpHandles[GLIMP_VID_YPOS];
	r_noborder          = glimpHandles[GLIMP_NOBORDER];
	r_customPixelAspect = glimpHandles[GLIMP_CUSTOM_PIXEL_ASPECT];
	r_customwidth       = glimpHandles[GLIMP_CUSTOM_WIDTH];
	r_customheight      = glimpHandles[GLIMP_CUSTOM_HEIGHT];
	r_colorbits         = glimpHandles[GLIMP_COLORBITS];
	cl_stencilbits      = glimpHandles[GLIMP_STENCILBITS];
	cl_depthbits        = glimpHandles[GLIMP_DEPTHBITS];
	cl_drawBuffer       = glimpHandles[GLIMP_DRAWBUFFER];

	// r_mode: runtime upper bound (s_numVidModes-1) prevents static descriptor
	r_mode = Cvar_Get( "r_mode", "-2", CVAR_ARCHIVE | CVAR_LATCH );
	Cvar_CheckRange( r_mode, "-2", va( "%i", s_numVidModes-1 ), CV_INTEGER );
	Cvar_SetDescription( r_mode, "Set video mode:\n -2 - use current desktop resolution\n -1 - use \\r_customWidth and \\r_customHeight\n  0..N - enter \\modelist for details" );

	{
#ifdef _DEBUG
		static const cvarDesc_t d = CVAR_STRING( "r_modeFullscreen", "", CVAR_ARCHIVE | CVAR_LATCH,
			"Dedicated fullscreen mode, set to \"\" to use \\r_mode in all cases." );
#else
		static const cvarDesc_t d = CVAR_STRING( "r_modeFullscreen", "-2", CVAR_ARCHIVE | CVAR_LATCH,
			"Dedicated fullscreen mode, set to \"\" to use \\r_mode in all cases." );
#endif
		r_modeFullscreen = Cvar_Register( &d );
	}

	{
#ifdef __APPLE__
		// SDL3 fullscreen mode detection is broken on macOS/MoltenVK —
		// default to windowed so first launch succeeds without config.cfg.
		static const cvarDesc_t d = CVAR_BOOL( "r_fullscreen", "0", CVAR_ARCHIVE | CVAR_LATCH,
			"Fullscreen mode. Set to 0 for windowed mode." );
#else
		static const cvarDesc_t d = CVAR_BOOL( "r_fullscreen", "1", CVAR_ARCHIVE | CVAR_LATCH,
			"Fullscreen mode. Set to 0 for windowed mode." );
#endif
		r_fullscreen = Cvar_Register( &d );
	}

#ifdef USE_RENDERER_DLOPEN
	{
#ifdef RENDERER_DEFAULT
		static const cvarDesc_t d = CVAR_STRING( "cl_renderer", XSTRING( RENDERER_DEFAULT ), CVAR_ARCHIVE | CVAR_LATCH,
			"Sets your desired renderer, requires \\vid_restart." );
#else
		static const cvarDesc_t d = CVAR_STRING( "cl_renderer", "opengl", CVAR_ARCHIVE | CVAR_LATCH,
			"Sets your desired renderer, requires \\vid_restart." );
#endif
		cl_renderer = Cvar_Register( &d );
	}

	if ( !isValidRenderer( cl_renderer->string ) ) {
		Cvar_ForceReset( "cl_renderer" );
	}
#endif
}


static const cvarDesc_t clInitDescs[] = {
	/* 0  */ CVAR_BOOL(   "cl_noprint",               "0",   0,                             "Disable printing of information in the console." ),
	/* 1  */ CVAR_BOOL(   "cl_motd",                  "1",   0,                             "Toggle the display of the 'Message of the day'. When Quake 3 Arena starts a map up, it sends the GL_RENDERER string to the Message Of The Day server at id. This responds back with a message of the day to the client." ),
	/* 2  */ CVAR_INT(    "cl_timeout",               "200", 0,                             "Duration of receiving nothing from server for client to decide it must be disconnected (in seconds).", 0, 0 ),
	/* 3  */ CVAR_FLOAT(  "cl_autoNudge",             "1",   CVAR_TEMP,                     "Automatic time nudge that uses your average ping as the time nudge, values:\n  0 - use fixed \\cl_timeNudge\n (0..1] - factor of median average ping to use as timenudge\n", 0, 1 ),
	/* 4  */ CVAR_INT(    "cl_timeNudge",             "0",   CVAR_TEMP,                     "Allows more or less latency to be added in the interest of better smoothness or better responsiveness.", -30, 30 ),
	/* 5  */ CVAR_INT(    "cl_shownet",               "0",   CVAR_TEMP,                     "Toggle the display of current network status.", 0, 0 ),
	/* 6  */ CVAR_BOOL(   "cl_showTimeDelta",         "0",   CVAR_TEMP,                     "Prints the time delta of each packet to the console (the time delta between server updates)." ),
	/* 7  */ CVAR_STRING( "activeAction",             "",    CVAR_TEMP,                     "Contents of this variable will be executed upon first frame of play.\nNote: It is cleared every time it is executed." ),
	/* 8  */ CVAR_BOOL(   "cl_autoRecordDemo",        "0",   CVAR_ARCHIVE,                  "Auto-record demos when starting or joining a game." ),
	/* 9  */ CVAR_BOOL(   "cl_drawRecording",         "1",   CVAR_ARCHIVE,                  "Hide (0) or shorten (1) \"RECORDING\" HUD message when recording demo." ),
	/* 10 */ CVAR_INT(    "cl_aviFrameRate",          "25",  CVAR_ARCHIVE,                  "The framerate used for capturing video.", 1, 1000 ),
	/* 11 */ CVAR_BOOL(   "cl_aviMotionJpeg",         "1",   CVAR_ARCHIVE,                  "Enable/disable the MJPEG codec for avi output." ),
	/* 12 */ CVAR_BOOL(   "cl_forceavidemo",          "0",   0,                             "Forces all demo recording into a sequence of screenshots in TGA format." ),
	/* 13 */ CVAR_STRING( "cl_aviPipeFormat",
		"-preset medium -crf 23 -c:v libx264 -flags +cgop -pix_fmt yuvj420p "
		"-bf 2 -c:a aac -strict -2 -b:a 160k -movflags faststart",
		CVAR_ARCHIVE,                  "Encoder parameters used for \\video-pipe." ),
	/* 14 */ CVAR_INT(    "cl_allowDownload",         "1",   CVAR_ARCHIVE | CVAR_NODEFAULT,               "Enables downloading of content needed in server. Valid bitmask flags:\n 1: Downloading enabled\n 2: Do not use HTTP/FTP downloads\n 4: Do not use UDP downloads", 0, 0 ),
	/* 15 */ CVAR_INT(    "cl_conXOffset",            "0",   0,                             "Console notifications X-offset.", 0, 0 ),
	/* 16 */ CVAR_INT(    "cl_conYOffset",            "0",   0,                             "Console notifications Y-offset.", 0, 0 ),
	/* 17 */ CVAR_STRING( "cl_conColor",              "",    0,                             "Console background color, set as R G B A values from 0-255, use with \\seta to save in config." ),
	/* 18 */ CVAR_INT(    "cl_matchAlerts",           "7",   CVAR_ARCHIVE,
		"Match-start alert bitmask:\n"
		"  1 = alert when the window is unfocused (not just minimized)\n"
		"  2 = flash the taskbar / dock\n"
		"  4 = beep (attention sound)\n"
		"  8 = unmute audio (overrides s_autoMute)", 0, 15 ),
	/* 19 */ CVAR_INT(    "cl_serverStatusResendTime","750", 0,                             "Time between re-sending server status requests if no response is received (in milliseconds).", 0, 0 ),
	/* 20 */ CVAR_STRING( "cl_motdString",            "",    CVAR_ROM,                      "Message of the day string from id's master server, it is a read only variable." ),
	/* 21 */ CVAR_BOOL(   "cl_lanForcePackets",       "1",   CVAR_ARCHIVE | CVAR_NODEFAULT,               "Bypass \\cl_maxpackets for LAN games, send packets every frame." ),
	/* 22 */ CVAR_BOOL(   "cl_guidServerUniq",        "1",   CVAR_ARCHIVE | CVAR_NODEFAULT,               "Makes cl_guid unique for each server." ),
	/* 23 */ CVAR_STRING( "cl_dlURL",                 "http://ws.q3df.org/maps/download/%1", CVAR_ARCHIVE | CVAR_NODEFAULT, "Cvar must point to download location." ),
	/* 25 */ CVAR_STRING( "cl_reconnectArgs",         "",    CVAR_ARCHIVE | CVAR_NODEFAULT | CVAR_NOTABCOMPLETE, NULL ),
	/* 26 */ CVAR_STRING( "cl_wiredRconPassword",     "",    CVAR_TEMP,                     "Wired RCON password used for challenge-response authentication." ),
};

enum {
	CLI_NOPRINT, CLI_MOTD, CLI_TIMEOUT, CLI_AUTONUDGE, CLI_TIMENUDGE,
	CLI_SHOWNET, CLI_SHOWTIMEDELTA, CLI_ACTIVEACTION,
	CLI_AUTORECORDDEMO, CLI_DRAWRECORDING,
	CLI_AVIFRAMERATE, CLI_AVIMOTIONJPEG, CLI_FORCEAVIDEMO, CLI_AVIPIPEFORMAT,
	CLI_ALLOWDOWNLOAD,
	CLI_CONXOFFSET, CLI_CONYOFFSET, CLI_CONCOLOR,
	CLI_MATCHALERTS, CLI_SERVERSTATUSRESENDTIME,
	CLI_MOTDSTRING, CLI_LANFORCEPACKETS, CLI_GUIDSERVERUNIQ,
	CLI_DLURL, CLI_RECONNECTARGS, CLI_WIREDRCONPASSWORD,
	CLI_CVAR_COUNT
};

_Static_assert( ARRAY_LEN( clInitDescs ) == CLI_CVAR_COUNT, "clInitDescs/enum mismatch" );
static cvar_t *clInitHandles[CLI_CVAR_COUNT];


/*
====================
CL_Init
====================
*/
void CL_Init( void ) {
	Com_Log( SEV_INFO, LOG_CAT_CLIENT, "----- Client Initialization -----\n" );

	Con_Init();

	CL_ClearState();
	cls.state = CA_DISCONNECTED;	// no longer CA_UNINITIALIZED

	CL_ResetOldGame();

	cls.realtime = 0;

	CL_InitInput();

#if FEAT_WIRED_UI
	WiredStore_Init();
	WiredStoreLua_Init();
	/* Single entry point: registers all WiredUI Lua globals (load_menu,
	   attract.*) before WiredScript_PostInit runs them against the VM. */
	WiredUI_LuaInit();
#endif

	//
	// register client variables
	//
	Cvar_RegisterTable( clInitDescs, ARRAY_LEN( clInitDescs ), clInitHandles );
	cl_noprint                = clInitHandles[CLI_NOPRINT];
	cl_motd                   = clInitHandles[CLI_MOTD];
	cl_timeout                = clInitHandles[CLI_TIMEOUT];
	cl_autoNudge              = clInitHandles[CLI_AUTONUDGE];
	cl_timeNudge              = clInitHandles[CLI_TIMENUDGE];
	cl_shownet                = clInitHandles[CLI_SHOWNET];
	cl_showTimeDelta          = clInitHandles[CLI_SHOWTIMEDELTA];
	cl_activeAction           = clInitHandles[CLI_ACTIVEACTION];
	cl_autoRecordDemo         = clInitHandles[CLI_AUTORECORDDEMO];
	cl_drawRecording          = clInitHandles[CLI_DRAWRECORDING];
	cl_aviFrameRate           = clInitHandles[CLI_AVIFRAMERATE];
	cl_aviMotionJpeg          = clInitHandles[CLI_AVIMOTIONJPEG];
	cl_forceavidemo           = clInitHandles[CLI_FORCEAVIDEMO];
	cl_aviPipeFormat          = clInitHandles[CLI_AVIPIPEFORMAT];
	cl_allowDownload          = clInitHandles[CLI_ALLOWDOWNLOAD];
	cl_conXOffset             = clInitHandles[CLI_CONXOFFSET];
	cl_conYOffset             = clInitHandles[CLI_CONYOFFSET];
	cl_conColor               = clInitHandles[CLI_CONCOLOR];
	cl_matchAlerts            = clInitHandles[CLI_MATCHALERTS];
	cl_serverStatusResendTime = clInitHandles[CLI_SERVERSTATUSRESENDTIME];
	cl_motdString             = clInitHandles[CLI_MOTDSTRING];
	cl_lanForcePackets        = clInitHandles[CLI_LANFORCEPACKETS];
	cl_guidServerUniq         = clInitHandles[CLI_GUIDSERVERUNIQ];
	cl_dlURL                  = clInitHandles[CLI_DLURL];
	cl_reconnectArgs          = clInitHandles[CLI_RECONNECTARGS];
	cl_wiredRconPassword      = clInitHandles[CLI_WIREDRCONPASSWORD];

#ifdef USE_CURL
	{
		static const cvarDesc_t d = CVAR_BOOL( "cl_mapAutoDownload", "0", CVAR_ARCHIVE | CVAR_NODEFAULT,
			"Automatic map download for play and demo playback (via automatic \\dlmap call)." );
		cl_mapAutoDownload = Cvar_Register( &d );
	}
#ifdef USE_CURL_DLOPEN
	{
		static const cvarDesc_t d = CVAR_STRING( "cl_cURLLib", DEFAULT_CURL_LIB, 0,
			"Filename of cURL library to load." );
		cl_cURLLib = Cvar_Register( &d );
	}
#endif
#endif

	{
#ifdef MACOS_X
		// In game video is REALLY slow in Mac OS X right now due to driver slowness
		static const cvarDesc_t d = CVAR_BOOL( "r_inGameVideo", "0", CVAR_ARCHIVE | CVAR_NODEFAULT,
			"Controls whether in-game video should be drawn." );
#else
		static const cvarDesc_t d = CVAR_BOOL( "r_inGameVideo", "1", CVAR_ARCHIVE | CVAR_NODEFAULT,
			"Controls whether in-game video should be drawn." );
#endif
		cl_inGameVideo = Cvar_Register( &d );
	}

	// init cg_autoswitch so the ui will have it correctly even
	// if the cgame hasn't been started
	Cvar_Get ("cg_autoswitch", "0", CVAR_ARCHIVE);

	{
		static const cvarDesc_t d = CVAR_INT( "cl_maxPing", "800", CVAR_ARCHIVE | CVAR_NODEFAULT,
			"Specify the maximum allowed ping to a server.", 100, 999 );
		Cvar_Register( &d );
	}

	// userinfo
	Cvar_Get ("name", "UnnamedPlayer", CVAR_USERINFO | CVAR_ARCHIVE | CVAR_NODEFAULT );
	Cvar_Get ("rate", "25000", CVAR_USERINFO | CVAR_ARCHIVE );
	Cvar_Get ("snaps", "40", CVAR_USERINFO | CVAR_ARCHIVE );
	Cvar_Get ("char", DEFAULT_MODEL, CVAR_USERINFO | CVAR_ARCHIVE | CVAR_NODEFAULT );
	Cvar_Get ("skin", "default", CVAR_USERINFO | CVAR_ARCHIVE | CVAR_NODEFAULT );
	Cvar_Get ("color1", "4", CVAR_USERINFO | CVAR_ARCHIVE );
	Cvar_Get ("color2", "5", CVAR_USERINFO | CVAR_ARCHIVE );
//	Cvar_Get ("teamtask", "0", CVAR_USERINFO );
	Cvar_Get ("cl_anonymous", "0", CVAR_USERINFO | CVAR_ARCHIVE | CVAR_NODEFAULT );

	Cvar_Get ("password", "", CVAR_USERINFO | CVAR_NORESTART);
	Cvar_Get ("cg_predictItems", "1", CVAR_USERINFO | CVAR_ARCHIVE );


	// Make sure cg_stereoSeparation is zero as that variable is deprecated and should not be used anymore.
	Cvar_Get ("cg_stereoSeparation", "0", CVAR_ROM);

	//
	// register client commands
	//
	Cmd_AddCommand ("cmd", CL_ForwardToServer_f);
	Cmd_AddCommand ("configstrings", CL_Configstrings_f);
	Cmd_AddCommand ("clientinfo", CL_Clientinfo_f);
	Cmd_AddCommand ("snd_restart", CL_Snd_Restart_f);
	Cmd_AddCommand ("vid_restart", CL_Vid_Restart_f);
	Cmd_AddCommand ("disconnect", CL_Disconnect_f);
	Cmd_AddCommand ("record", CL_Record_f);
	Cmd_SetCommandCompletionFunc( "record", CL_CompleteRecordName );
	Cmd_AddCommand ("demo", CL_PlayDemo_f);
	Cmd_SetCommandCompletionFunc( "demo", CL_CompleteDemoName );
	Cmd_AddCommand ("cinematic", CL_PlayCinematic_f);
	Cmd_AddCommand ("stoprecord", CL_StopRecord_f);
	Cmd_AddCommand ("connect", CL_Connect_f);
	Cmd_AddCommand ("reconnect", CL_Reconnect_f);
	Cmd_AddCommand ("rcon_login", CL_RconLogin_f);
	Cmd_AddCommand ("rcon", CL_Rcon_f);
	Cmd_AddCommand ("localservers", CL_LocalServers_f);
	Cmd_AddCommand ("globalservers", CL_GlobalServers_f);
	Cmd_AddCommand ("ping", CL_Ping_f );
	Cmd_AddCommand ("serverstatus", CL_ServerStatus_f );
	Cmd_AddCommand ("showip", CL_ShowIP_f );
	Cmd_AddCommand ("fs_openedList", CL_OpenedPK3List_f );
	Cmd_AddCommand ("fs_referencedList", CL_ReferencedPK3List_f );
	Cmd_AddCommand ("char", CL_SetChar_f );
	Cmd_AddCommand ("skin", CL_SetSkin_f );
	Cmd_AddCommand ("video", CL_Video_f );
	Cmd_AddCommand ("video-pipe", CL_Video_f );
	Cmd_SetCommandCompletionFunc( "video", CL_CompleteVideoName );
	Cmd_AddCommand ("stopvideo", CL_StopVideo_f );
	Cmd_AddCommand ("serverinfo", CL_Serverinfo_f );
	Cmd_AddCommand ("systeminfo", CL_Systeminfo_f );

#ifdef USE_CURL
	Cmd_AddCommand( "download", CL_Download_f );
	Cmd_AddCommand( "dlmap", CL_Download_f );
#endif
	Cmd_AddCommand( "modelist", CL_ModeList_f );

#ifndef NDEBUG
	Cmd_AddCommand( "wui_testerror", CL_WuiTestError_f );
	Cvar_Get( "net_forceSendError", "0", CVAR_TEMP );
#endif

	Cvar_Set( "cl_running", "1" );
	Cvar_Get( "cl_guid", "", CVAR_USERINFO | CVAR_ROM | CVAR_PROTECTED );
	CL_UpdateGUID( NULL, 0 );

	Com_Log( SEV_INFO, LOG_CAT_CLIENT, "----- Client Initialization Complete -----\n" );
}


/*
===============
CL_Shutdown

Called on fatal error, quit and dedicated mode switch
===============
*/
void CL_Shutdown( const char *finalmsg, qboolean quit ) {
	static qboolean recursive = qfalse;

	// check whether the client is running at all.
	if ( !( com_cl_running && com_cl_running->integer ) )
		return;

	Com_Log( SEV_INFO, LOG_CAT_CLIENT, "----- Client Shutdown (%s) -----\n", finalmsg );

	if ( recursive ) {
		Com_Log( SEV_INFO, LOG_CAT_CLIENT, "WARNING: Recursive CL_Shutdown()\n" );
		return;
	}
	recursive = qtrue;

	noGameRestart = quit;
	CL_Disconnect( qfalse );

	// clear and mute all sounds until next registration
	S_DisableSounds();

	CL_ShutdownVMs();

	CL_ShutdownRef( quit ? REF_UNLOAD_DLL : REF_DESTROY_WINDOW );

	Con_Shutdown();

	Cmd_RemoveCommand ("cmd");
	Cmd_RemoveCommand ("configstrings");
	Cmd_RemoveCommand ("userinfo");
	Cmd_RemoveCommand ("clientinfo");
	Cmd_RemoveCommand ("snd_restart");
	Cmd_RemoveCommand ("vid_restart");
	Cmd_RemoveCommand ("disconnect");
	Cmd_RemoveCommand ("record");
	Cmd_RemoveCommand ("demo");
	Cmd_RemoveCommand ("cinematic");
	Cmd_RemoveCommand ("stoprecord");
	Cmd_RemoveCommand ("connect");
	Cmd_RemoveCommand ("reconnect");
	Cmd_RemoveCommand ("rcon_login");
	Cmd_RemoveCommand ("rcon");
	Cmd_RemoveCommand ("localservers");
	Cmd_RemoveCommand ("globalservers");
	Cmd_RemoveCommand ("ping");
	Cmd_RemoveCommand ("serverstatus");
	Cmd_RemoveCommand ("showip");
	Cmd_RemoveCommand ("fs_openedList");
	Cmd_RemoveCommand ("fs_referencedList");
	Cmd_RemoveCommand ("char");
	Cmd_RemoveCommand ("skin");
	Cmd_RemoveCommand ("video");
	Cmd_RemoveCommand ("stopvideo");
	Cmd_RemoveCommand ("serverinfo");
	Cmd_RemoveCommand ("systeminfo");
	Cmd_RemoveCommand ("modelist");

#ifndef NDEBUG
	Cmd_RemoveCommand( "wui_testerror" );
#endif

#ifdef USE_CURL
	Com_DL_Cleanup( &download );

	Cmd_RemoveCommand( "download" );
	Cmd_RemoveCommand( "dlmap" );
#endif

	CL_ClearInput();

#if FEAT_WIRED_UI
	WiredStore_Shutdown();
#endif

	Cvar_Set( "cl_running", "0" );

	recursive = qfalse;

	memset( &cls, 0, sizeof( cls ) );
	Key_SetCatcher( 0 );
	CL_Characters_Shutdown();
	Com_Log( SEV_INFO, LOG_CAT_CLIENT, "-----------------------\n" );
}


static void CL_SetServerInfo(serverInfo_t *server, const char *info, int ping) {
	if (server) {
		if (info) {
			server->clients = atoi(Info_ValueForKey(info, "clients"));
			Q_strncpyz(server->hostName,Info_ValueForKey(info, "hostname"), MAX_HOSTNAME_LENGTH);
			Q_strncpyz(server->mapName, Info_ValueForKey(info, "mapname"), MAX_NAME_LENGTH);
			server->maxClients = atoi(Info_ValueForKey(info, "sv_maxclients"));
			Q_strncpyz(server->game,Info_ValueForKey(info, "game"), MAX_NAME_LENGTH);
			server->gameType = atoi(Info_ValueForKey(info, "gametype"));
			server->netType = atoi(Info_ValueForKey(info, "nettype"));
			server->minPing = atoi(Info_ValueForKey(info, "minping"));
			server->maxPing = atoi(Info_ValueForKey(info, "maxping"));
			server->punkbuster = atoi(Info_ValueForKey(info, "punkbuster"));
			server->g_humanplayers = atoi(Info_ValueForKey(info, "g_humanplayers"));
			server->g_needpass = atoi(Info_ValueForKey(info, "g_needpass"));
		}
		server->ping = ping;
	}
}


static void CL_SetServerInfoByAddress(const netadr_t *from, const char *info, int ping) {
	for (int i = 0; i < MAX_OTHER_SERVERS; i++) {
		if (NET_CompareAdr(from, &cls.localServers[i].adr) ) {
			CL_SetServerInfo(&cls.localServers[i], info, ping);
		}
	}

	for (int i = 0; i < MAX_GLOBAL_SERVERS; i++) {
		if (NET_CompareAdr(from, &cls.globalServers[i].adr)) {
			CL_SetServerInfo(&cls.globalServers[i], info, ping);
		}
	}

	for (int i = 0; i < MAX_OTHER_SERVERS; i++) {
		if (NET_CompareAdr(from, &cls.favoriteServers[i].adr)) {
			CL_SetServerInfo(&cls.favoriteServers[i], info, ping);
		}
	}
}


/*
===================
CL_ServerInfoPacket
===================
*/
static void CL_ServerInfoPacket( const netadr_t *from, msg_t *msg ) {
	char	info[MAX_INFO_STRING];

	const char *infoString = MSG_ReadString( msg );

	// if this isn't the correct protocol version, ignore it
	int prot = atoi( Info_ValueForKey( infoString, "protocol" ) );
	if ( prot != com_protocol->integer ) {
		Com_Log( SEV_DEBUG, LOG_CAT_CLIENT, "Different protocol info packet: %s\n", infoString );
		return;
	}

	// iterate servers waiting for ping response
	for (int i=0; i<MAX_PINGREQUESTS; i++)
	{
		if ( cl_pinglist[i].adr.port && !cl_pinglist[i].time && NET_CompareAdr( from, &cl_pinglist[i].adr ) )
		{
			// calc ping time
			cl_pinglist[i].time = Sys_Milliseconds() - cl_pinglist[i].start;
			if ( cl_pinglist[i].time < 1 )
			{
				cl_pinglist[i].time = 1;
			}
			Com_Log( SEV_DEBUG, LOG_CAT_CLIENT, "ping time %dms from %s\n", cl_pinglist[i].time, NET_AdrToString( from ) );

			// save of info
			Q_strncpyz( cl_pinglist[i].info, infoString, sizeof( cl_pinglist[i].info ) );

			// tack on the net type
			// NOTE: make sure these types are in sync with the netnames strings in the UI
			int type;
			switch (from->type)
			{
				case NA_BROADCAST:
				case NA_IP:
					type = 1;
					break;
#if FEAT_IPV6
				case NA_IP6:
					type = 2;
					break;
#endif
				default:
					type = 0;
					break;
			}

			Info_SetValueForKey( cl_pinglist[i].info, "nettype", va( "%d", type ) );
			CL_SetServerInfoByAddress( from, infoString, cl_pinglist[i].time );

			return;
		}
	}

	// if not just sent a local broadcast or pinging local servers
	if (cls.pingUpdateSource != AS_LOCAL) {
		return;
	}

	int i;
	for ( i = 0 ; i < MAX_OTHER_SERVERS ; i++ ) {
		// empty slot
		if ( cls.localServers[i].adr.port == 0 ) {
			break;
		}

		// avoid duplicate
		if ( NET_CompareAdr( from, &cls.localServers[i].adr ) ) {
			return;
		}
	}

	if ( i == MAX_OTHER_SERVERS ) {
		Com_Log( SEV_DEBUG, LOG_CAT_CLIENT, "MAX_OTHER_SERVERS hit, dropping infoResponse\n" );
		return;
	}

	// add this to the list
	cls.numlocalservers = i+1;
	CL_InitServerInfo( &cls.localServers[i], from );

	Q_strncpyz( info, MSG_ReadString( msg ), sizeof( info ) );
	int len = (int) strlen( info );
	if ( len > 0 ) {
		if ( info[ len-1 ] == '\n' ) {
			info[ len-1 ] = '\0';
		}
		Com_Log( SEV_INFO, LOG_CAT_CLIENT, "%s: %s\n", NET_AdrToStringwPort( from ), info );
	}
}


/*
===================
CL_GetServerStatus
===================
*/
static serverStatus_t *CL_GetServerStatus( const netadr_t *from ) {
	for (int i = 0; i < MAX_SERVERSTATUSREQUESTS; i++) {
		if ( NET_CompareAdr( from, &cl_serverStatusList[i].address ) ) {
			return &cl_serverStatusList[i];
		}
	}
	for (int i = 0; i < MAX_SERVERSTATUSREQUESTS; i++) {
		if ( cl_serverStatusList[i].retrieved ) {
			return &cl_serverStatusList[i];
		}
	}
	int oldest = -1;
	int oldestTime = 0;
	for (int i = 0; i < MAX_SERVERSTATUSREQUESTS; i++) {
		if (oldest == -1 || cl_serverStatusList[i].startTime < oldestTime) {
			oldest = i;
			oldestTime = cl_serverStatusList[i].startTime;
		}
	}
	return &cl_serverStatusList[oldest];
}


/*
===================
CL_ServerStatus
===================
*/
int CL_ServerStatus( const char *serverAddress, char *serverStatusString, int maxLen ) {
	netadr_t	to;

	// if no server address then reset all server status requests
	if ( !serverAddress ) {
		for (int i = 0; i < MAX_SERVERSTATUSREQUESTS; i++) {
			cl_serverStatusList[i].address.port = 0;
			cl_serverStatusList[i].retrieved = qtrue;
		}
		return qfalse;
	}
	// get the address
	if ( !NET_StringToAdr( serverAddress, &to, NA_UNSPEC ) ) {
		return qfalse;
	}
	serverStatus_t *serverStatus = CL_GetServerStatus( &to );
	// if no server status string then reset the server status request for this address
	if ( !serverStatusString ) {
		serverStatus->retrieved = qtrue;
		return qfalse;
	}

	// if this server status request has the same address
	if ( NET_CompareAdr( &to, &serverStatus->address) ) {
		// if we received a response for this server status request
		if (!serverStatus->pending) {
			Q_strncpyz(serverStatusString, serverStatus->string, maxLen);
			serverStatus->retrieved = qtrue;
			serverStatus->startTime = 0;
			return qtrue;
		}
		// resend the request regularly
		else if ( Sys_Milliseconds() - serverStatus->startTime > cl_serverStatusResendTime->integer ) {
			serverStatus->print = qfalse;
			serverStatus->pending = qtrue;
			serverStatus->retrieved = qfalse;
			serverStatus->time = 0;
			serverStatus->startTime = Sys_Milliseconds();
			NET_OutOfBandPrint( NS_CLIENT, &to, "getstatus" );
			return qfalse;
		}
	}
	// if retrieved
	else if ( serverStatus->retrieved ) {
		serverStatus->address = to;
		serverStatus->print = qfalse;
		serverStatus->pending = qtrue;
		serverStatus->retrieved = qfalse;
		serverStatus->startTime = Sys_Milliseconds();
		serverStatus->time = 0;
		NET_OutOfBandPrint( NS_CLIENT, &to, "getstatus" );
		return qfalse;
	}
	return qfalse;
}


/*
===================
CL_ServerStatusResponse
===================
*/
static void CL_ServerStatusResponse( const netadr_t *from, msg_t *msg ) {
	char	info[MAX_INFO_STRING];

	serverStatus_t *serverStatus = NULL;
	for (int i = 0; i < MAX_SERVERSTATUSREQUESTS; i++) {
		if ( NET_CompareAdr( from, &cl_serverStatusList[i].address ) ) {
			serverStatus = &cl_serverStatusList[i];
			break;
		}
	}
	// if we didn't request this server status
	if (!serverStatus) {
		return;
	}

	const char *s = MSG_ReadStringLine( msg );

	int len = 0;
	Com_sprintf(&serverStatus->string[len], sizeof(serverStatus->string)-len, "%s", s);

	if (serverStatus->print) {
		Com_Log( SEV_INFO, LOG_CAT_CLIENT, "Server settings:\n");
		// print cvars
		while (*s) {
			for (int i = 0; i < 2 && *s; i++) {
				if (*s == '\\')
					s++;
				int l = 0;
				while (*s) {
					info[l++] = *s;
					if (l >= MAX_INFO_STRING-1)
						break;
					s++;
					if (*s == '\\') {
						break;
					}
				}
				info[l] = '\0';
				if (i) {
					Com_Log( SEV_INFO, LOG_CAT_CLIENT, "%s\n", info);
				}
				else {
					Com_Log( SEV_INFO, LOG_CAT_CLIENT, "%-24s", info);
				}
			}
		}
	}

	len = strlen(serverStatus->string);
	Com_sprintf(&serverStatus->string[len], sizeof(serverStatus->string)-len, "\\");

	if (serverStatus->print) {
		Com_Log( SEV_INFO, LOG_CAT_CLIENT, "\nPlayers:\n");
		Com_Log( SEV_INFO, LOG_CAT_CLIENT, "num: score: ping: name:\n");
	}
	s = MSG_ReadStringLine( msg );
	for (int i = 0; *s; s = MSG_ReadStringLine( msg ), i++) {

		len = strlen(serverStatus->string);
		Com_sprintf(&serverStatus->string[len], sizeof(serverStatus->string)-len, "\\%s", s);

		if (serverStatus->print) {
			//score = ping = 0;
			//sscanf(s, "%d %d", &score, &ping);
			char buf[64]; char *v[2];
			Q_strncpyz( buf, s, sizeof (buf) );
			Com_Split( buf, v, 2, ' ' );
			int score = atoi( v[0] );
			int ping = atoi( v[1] );
			s = strchr(s, ' ');
			if (s)
				s = strchr(s+1, ' ');
			if (s)
				s++;
			else
				s = "unknown";
			Com_Log( SEV_INFO, LOG_CAT_CLIENT, "%-2d   %-3d    %-3d   %s\n", i, score, ping, s );
		}
	}
	len = strlen(serverStatus->string);
	Com_sprintf(&serverStatus->string[len], sizeof(serverStatus->string)-len, "\\");

	serverStatus->time = Sys_Milliseconds();
	serverStatus->address = *from;
	serverStatus->pending = qfalse;
	if (serverStatus->print) {
		serverStatus->retrieved = qtrue;
	}
}


/*
==================
CL_LocalServers_f
==================
*/
static void CL_LocalServers_f( void ) {
	Com_Log( SEV_INFO, LOG_CAT_CLIENT, "Scanning for servers on the local network...\n");

	// reset the list, waiting for response
	cls.numlocalservers = 0;
	cls.pingUpdateSource = AS_LOCAL;

	for (int i = 0; i < MAX_OTHER_SERVERS; i++) {
		qboolean b = cls.localServers[i].visible;
		memset(&cls.localServers[i], 0, sizeof(cls.localServers[i]));
		cls.localServers[i].visible = b;
	}

	netadr_t to;
	memset( &to, 0, sizeof( to ) );

	// The 'xxx' in the message is a challenge that will be echoed back
	// by the server.  We don't care about that here, but master servers
	// can use that to prevent spoofed server responses from invalid ip
	const char *message = "\377\377\377\377getinfo xxx";
	int n = (int)strlen( message );

	// send each message twice in case one is dropped
	for ( int i = 0 ; i < 2 ; i++ ) {
		// send a broadcast packet on each server port
		// we support multiple server ports so a single machine
		// can nicely run multiple servers
		for ( int j = 0 ; j < NUM_SERVER_PORTS ; j++ ) {
			to.port = BigShort( (short)(PORT_SERVER + j) );

			to.type = NA_BROADCAST;
			NET_SendPacket( NS_CLIENT, n, message, &to );
#if FEAT_IPV6
			to.type = NA_MULTICAST6;
			NET_SendPacket( NS_CLIENT, n, message, &to );
#endif
		}
	}
}


/*
==================
CL_GlobalServers_f

Originally master 0 was Internet and master 1 was MPlayer.
ioquake3 2008; added support for requesting five separate master servers using 0-4.
ioquake3 2017; made master 0 fetch all master servers and 1-5 request a single master server.
==================
*/
static void CL_GlobalServers_f( void ) {
	char		command[1024];

	int count, masterNum;
	if ( (count = Cmd_Argc()) < 3 || (masterNum = atoi(Cmd_Argv(1))) < 0 || masterNum > MAX_MASTER_SERVERS )
	{
		Com_Log( SEV_INFO, LOG_CAT_CLIENT, "usage: globalservers <master# 0-%d> <protocol> [keywords]\n", MAX_MASTER_SERVERS );
		return;
	}

	// request from all master servers
	if ( masterNum == 0 ) {
		int numAddress = 0;

		for ( int i = 1; i <= MAX_MASTER_SERVERS; i++ ) {
			sprintf( command, "sv_master%d", i );
			const char *masteraddress = Cvar_VariableString( command );

			if ( !*masteraddress )
				continue;

			numAddress++;

			Com_sprintf( command, sizeof( command ), "globalservers %d %s %s\n", i, Cmd_Argv( 2 ), Cmd_ArgsFrom( 3 ) );
			Cbuf_AddText( command );
		}

		if ( !numAddress ) {
			Com_Log( SEV_INFO, LOG_CAT_CLIENT, "CL_GlobalServers_f: Error: No master server addresses.\n");
		}
		return;
	}

	sprintf( command, "sv_master%d", masterNum );
	const char *masteraddress = Cvar_VariableString( command );

	if ( !*masteraddress )
	{
		Com_Log( SEV_INFO, LOG_CAT_CLIENT, "CL_GlobalServers_f: Error: No master server address given.\n");
		return;
	}

	// reset the list, waiting for response
	// -1 is used to distinguish a "no response"

	netadr_t to;
	int i = NET_StringToAdr( masteraddress, &to, NA_UNSPEC );

	if ( i == 0 )
	{
		Com_Log( SEV_INFO, LOG_CAT_CLIENT, "CL_GlobalServers_f: Error: could not resolve address of master %s\n", masteraddress );
		return;
	}
	else if ( i == 2 )
		to.port = BigShort( PORT_MASTER );

	Com_Log( SEV_INFO, LOG_CAT_CLIENT, "Requesting servers from %s (%s)...\n", masteraddress, NET_AdrToStringwPort( &to ) );

	cls.numglobalservers = -1;
	cls.pingUpdateSource = AS_GLOBAL;

	// Use the extended query for IPv6 masters
#if FEAT_IPV6
	if ( to.type == NA_IP6 || to.type == NA_MULTICAST6 )
	{
		int v4enabled = Cvar_VariableIntegerValue( "net_enabled" ) & NET_ENABLEV4;

		if ( v4enabled )
		{
			Com_sprintf( command, sizeof( command ), "getserversExt %s %s",
				GAMENAME_FOR_MASTER, Cmd_Argv(2) );
		}
		else
		{
			Com_sprintf( command, sizeof( command ), "getserversExt %s %s ipv6",
				GAMENAME_FOR_MASTER, Cmd_Argv(2) );
		}
	}
	else
#endif
		Com_sprintf( command, sizeof( command ), "getservers %s", Cmd_Argv(2) );

	{
		qstring_t cmd_qs = QS_WrapExisting( command, sizeof( command ) );
		for ( int i = 3; i < count; i++ )
		{
			QS_AppendChar( &cmd_qs, ' ' );
			QS_Append( &cmd_qs, Cmd_Argv( i ) );
		}
	}

	NET_OutOfBandPrint( NS_SERVER, &to, "%s", command );
}


/*
==================
CL_GetPing
==================
*/
void CL_GetPing( int n, char *buf, int buflen, int *pingtime )
{
	if (n < 0 || n >= MAX_PINGREQUESTS || !cl_pinglist[n].adr.port)
	{
		// empty or invalid slot
		buf[0]    = '\0';
		*pingtime = 0;
		return;
	}

	const char *str = NET_AdrToStringwPort( &cl_pinglist[n].adr );
	Q_strncpyz( buf, str, buflen );

	int time = cl_pinglist[n].time;
	if ( time == 0 )
	{
		// check for timeout
		time = Sys_Milliseconds() - cl_pinglist[n].start;
		int maxPing = Cvar_VariableIntegerValue( "cl_maxPing" );
		if ( time < maxPing )
		{
			// not timed out yet
			time = 0;
		}
	}

	CL_SetServerInfoByAddress(&cl_pinglist[n].adr, cl_pinglist[n].info, cl_pinglist[n].time);

	*pingtime = time;
}


/*
==================
CL_GetPingInfo
==================
*/
void CL_GetPingInfo( int n, char *buf, int buflen )
{
	if (n < 0 || n >= MAX_PINGREQUESTS || !cl_pinglist[n].adr.port)
	{
		// empty or invalid slot
		if (buflen)
			buf[0] = '\0';
		return;
	}

	Q_strncpyz( buf, cl_pinglist[n].info, buflen );
}


/*
==================
CL_ClearPing
==================
*/
void CL_ClearPing( int n )
{
	if (n < 0 || n >= MAX_PINGREQUESTS)
		return;

	cl_pinglist[n].adr.port = 0;
}


/*
==================
CL_GetPingQueueCount
==================
*/
int CL_GetPingQueueCount( void )
{
	int count = 0;
	ping_t *pingptr = cl_pinglist;

	for (int i = 0; i < MAX_PINGREQUESTS; i++, pingptr++ ) {
		if (pingptr->adr.port) {
			count++;
		}
	}

	return (count);
}


/*
==================
CL_GetFreePing
==================
*/
static ping_t* CL_GetFreePing( void )
{
	int msec = Sys_Milliseconds();
	ping_t* pingptr = cl_pinglist;
	for ( int i = 0; i < ARRAY_LEN( cl_pinglist ); i++, pingptr++ )
	{
		// find free ping slot
		if ( pingptr->adr.port )
		{
			if ( pingptr->time == 0 )
			{
				if ( msec - pingptr->start < 500 )
				{
					// still waiting for response
					continue;
				}
			}
			else if ( pingptr->time < 500 )
			{
				// results have not been queried
				continue;
			}
		}

		// clear it
		pingptr->adr.port = 0;
		return pingptr;
	}

	// use oldest entry
	pingptr = cl_pinglist;
	ping_t* best = cl_pinglist;
	int oldest = INT_MIN;
	for ( int i = 0; i < ARRAY_LEN( cl_pinglist ); i++, pingptr++ )
	{
		// scan for oldest
		int time = msec - pingptr->start;
		if ( time > oldest )
		{
			oldest = time;
			best   = pingptr;
		}
	}

	return best;
}


/*
==================
CL_Ping_f
==================
*/
static void CL_Ping_f( void ) {
	int argc = Cmd_Argc();
	netadrtype_t	family = NA_UNSPEC;

	if ( argc != 2 && argc != 3 ) {
		Com_Log( SEV_INFO, LOG_CAT_CLIENT, "usage: ping [-4|-6] <server>\n");
		return;
	}

	const char *server;
	if ( argc == 2 )
		server = Cmd_Argv(1);
	else
	{
		if( !strcmp( Cmd_Argv(1), "-4" ) )
			family = NA_IP;
#if FEAT_IPV6
		else if( !strcmp( Cmd_Argv(1), "-6" ) )
			family = NA_IP6;
		else
			Com_Log( SEV_INFO, LOG_CAT_CLIENT, "warning: only -4 or -6 as address type understood.\n" );
#else
		else
			Com_Log( SEV_INFO, LOG_CAT_CLIENT, "warning: only -4 as address type understood.\n" );
#endif

		server = Cmd_Argv(2);
	}

	netadr_t to;
	memset( &to, 0, sizeof( to ) );

	if ( !NET_StringToAdr( server, &to, family ) ) {
		return;
	}

	ping_t* pingptr = CL_GetFreePing();

	memcpy( &pingptr->adr, &to, sizeof (netadr_t) );
	pingptr->start = Sys_Milliseconds();
	pingptr->time  = 0;

	CL_SetServerInfoByAddress( &pingptr->adr, NULL, 0 );

	NET_OutOfBandPrint( NS_CLIENT, &to, "getinfo xxx" );
}


/*
==================
CL_UpdateVisiblePings_f
==================
*/
qboolean CL_UpdateVisiblePings_f(int source) {
	qboolean status = qfalse;

	if (source < 0 || source > AS_FAVORITES) {
		return qfalse;
	}

	cls.pingUpdateSource = source;

	int slots = CL_GetPingQueueCount();
	if (slots < MAX_PINGREQUESTS) {
		serverInfo_t *server = NULL;
		int max;

		switch (source) {
			case AS_LOCAL :
				server = &cls.localServers[0];
				max = cls.numlocalservers;
			break;
			case AS_GLOBAL :
				server = &cls.globalServers[0];
				max = cls.numglobalservers;
			break;
			case AS_FAVORITES :
				server = &cls.favoriteServers[0];
				max = cls.numfavoriteservers;
			break;
			default:
				return qfalse;
		}
		for (int i = 0; i < max; i++) {
			if (server[i].visible) {
				if (server[i].ping == -1) {
					int j;

					if (slots >= MAX_PINGREQUESTS) {
						break;
					}
					for (j = 0; j < MAX_PINGREQUESTS; j++) {
						if (!cl_pinglist[j].adr.port) {
							continue;
						}
						if (NET_CompareAdr( &cl_pinglist[j].adr, &server[i].adr)) {
							// already on the list
							break;
						}
					}
					if (j >= MAX_PINGREQUESTS) {
						status = qtrue;
						for (j = 0; j < MAX_PINGREQUESTS; j++) {
							if (!cl_pinglist[j].adr.port) {
								memcpy(&cl_pinglist[j].adr, &server[i].adr, sizeof(netadr_t));
								cl_pinglist[j].start = Sys_Milliseconds();
								cl_pinglist[j].time = 0;
								NET_OutOfBandPrint(NS_CLIENT, &cl_pinglist[j].adr, "getinfo xxx");
								slots++;
								break;
							}
						}
					}
				}
				// if the server has a ping higher than cl_maxPing or
				// the ping packet got lost
				else if (server[i].ping == 0) {
					// if we are updating global servers
					if (source == AS_GLOBAL) {
						//
						if ( cls.numGlobalServerAddresses > 0 ) {
							// overwrite this server with one from the additional global servers
							cls.numGlobalServerAddresses--;
							CL_InitServerInfo(&server[i], &cls.globalServerAddresses[cls.numGlobalServerAddresses]);
							// NOTE: the server[i].visible flag stays untouched
						}
					}
				}
			}
		}
	}

	if (slots) {
		status = qtrue;
	}
	for (int i = 0; i < MAX_PINGREQUESTS; i++) {
		if (!cl_pinglist[i].adr.port) {
			continue;
		}
		char buff[MAX_STRING_CHARS];
		int pingTime;
		CL_GetPing( i, buff, MAX_STRING_CHARS, &pingTime );
		if (pingTime != 0) {
			CL_ClearPing(i);
			status = qtrue;
		}
	}

	return status;
}


/*
==================
CL_ServerStatus_f
==================
*/
static void CL_ServerStatus_f( void ) {
	int argc = Cmd_Argc();
	netadrtype_t	family = NA_UNSPEC;
	netadr_t	*toptr = NULL;

	if ( argc != 2 && argc != 3 )
	{
		if (cls.state != CA_ACTIVE || clc.demoplaying)
		{
			Com_Log( SEV_INFO, LOG_CAT_CLIENT, "Not connected to a server.\n" );
#if FEAT_IPV6
			Com_Log( SEV_INFO, LOG_CAT_CLIENT, "usage: serverstatus [-4|-6] <server>\n" );
#else
			Com_Log( SEV_INFO, LOG_CAT_CLIENT, "usage: serverstatus <server>\n");
#endif
			return;
		}

		toptr = &clc.serverAddress;
	}

	netadr_t to;
	if ( !toptr )
	{
		memset( &to, 0, sizeof( to ) );

		const char *server;
		if ( argc == 2 )
			server = Cmd_Argv(1);
		else
		{
			if ( !strcmp( Cmd_Argv(1), "-4" ) )
				family = NA_IP;
#if FEAT_IPV6
			else if ( !strcmp( Cmd_Argv(1), "-6" ) )
				family = NA_IP6;
			else
				Com_Log( SEV_INFO, LOG_CAT_CLIENT, "warning: only -4 or -6 as address type understood.\n" );
#else
			else
				Com_Log( SEV_INFO, LOG_CAT_CLIENT, "warning: only -4 as address type understood.\n" );
#endif

			server = Cmd_Argv(2);
		}

		toptr = &to;
		if ( !NET_StringToAdr( server, toptr, family ) )
			return;
	}

	NET_OutOfBandPrint( NS_CLIENT, toptr, "getstatus" );

	serverStatus_t *serverStatus = CL_GetServerStatus( toptr );
	serverStatus->address = *toptr;
	serverStatus->print = qtrue;
	serverStatus->pending = qtrue;
}


/*
==================
CL_ShowIP_f
==================
*/
static void CL_ShowIP_f( void ) {
	Sys_ShowIP();
}


#ifdef USE_CURL

qboolean CL_Download( const char *cmd, const char *pakname, qboolean autoDownload )
{
	if ( cl_dlURL->string[0] == '\0' )
	{
		COM_WARN( LOG_CAT_CLIENT, "cl_dlURL cvar is not set\n" );
		return qfalse;
	}

	// skip leading slashes
	while ( *pakname == '/' || *pakname == '\\' )
		pakname++;

	// skip gamedir
	const char *s = strrchr( pakname, '/' );
	if ( s )
		pakname = s+1;

	if ( !Com_DL_ValidFileName( pakname ) )
	{
		COM_WARN( LOG_CAT_CLIENT, "invalid file name: '%s'.\n", pakname );
		return qfalse;
	}

	if ( !Q_stricmp( cmd, "dlmap" ) )
	{
		char name[MAX_CVAR_VALUE_STRING];
		Q_strncpyz( name, pakname, sizeof( name ) );
		FS_StripExt( name, ".pk3" );
		if ( !name[0] )
			return qfalse;
		char url[MAX_OSPATH];
		s = va( "maps/%s.bsp", name );
		if ( FS_FileIsInPAK( s, NULL, url ) )
		{
			COM_WARN( LOG_CAT_CLIENT, " map %s already exists in %s.pk3\n", name, url );
			return qfalse;
		}
	}

	return Com_DL_Begin( &download, pakname, cl_dlURL->string, autoDownload );
}


/*
==================
CL_Download_f
==================
*/
static void CL_Download_f( void )
{
	if ( Cmd_Argc() < 2 || *Cmd_Argv( 1 ) == '\0' )
	{
		Com_Log( SEV_INFO, LOG_CAT_CLIENT, "usage: %s <mapname>\n", Cmd_Argv( 0 ) );
		return;
	}

	if ( !strcmp( Cmd_Argv(1), "-" ) )
	{
		Com_DL_Cleanup( &download );
		return;
	}

	CL_Download( Cmd_Argv( 0 ), Cmd_Argv( 1 ), qfalse );
}
#endif // USE_CURL
