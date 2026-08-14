// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 1999-2005 Id Software, Inc.
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
// cl_main.c  -- client main loop

#include "client.h"
#include "cl_demo_frame.h"
#include "cl_info_challenge.h"
#include "cl_ping_queue.h"
#include "wired/ui/cl_wired_ui.h"
#include "wired/ui/cl_wired_viewport.h"
#include "wired/ui/cl_wired_compositor.h"
#include "wired/ui/cl_wired_attract.h"
#include "wired/store/cl_wired_store.h"
#include "wired/hud/cl_wired_crosshair.h"
#include "wired/scene/cl_wired_scene_lua.h"
#include "wired/l10n/cl_wired_l10n.h"
#include "../qcommon/util/crypto.h"
#include "../qcommon/maps/meta.h"
#include "../qcommon/wired/net/wn_public.h"
#include <errno.h>
#include <limits.h>
LOG_DECLARE_CHANNEL( ch_client, "client" );
LOG_DECLARE_CHANNEL( ch_renderer, "renderer" );

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
cvar_t	*cl_conXOffset;
cvar_t	*cl_conYOffset;
cvar_t	*cl_conColor;
cvar_t	*cl_inGameVideo;

cvar_t	*cl_serverStatusResendTime;

cvar_t	*cl_lanForcePackets;

cvar_t	*cl_guidServerUniq;

cvar_t	*cl_dlURL;

cvar_t	*cl_reconnectArgs;

/* cl_matchAlerts
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

// Per-app-instance client-state container (server-client decoupling).
// Single-app: only slot 0 is live; clientActiveApp pins to it. Access is explicit
// through clientActiveApp->cl / clientActiveApp->clc (the cl/clc macro aliases
// were retired).
clientApp_t			clientApps[MAX_LOCAL_CGAME_VMS];
clientApp_t			*clientActiveApp = &clientApps[0];
static uint64_t		cl_connectionGenerationIssuer;

static uint64_t CL_NextConnectionGeneration( void ) {
	cl_connectionGenerationIssuer++;
	if ( cl_connectionGenerationIssuer == 0 ) cl_connectionGenerationIssuer++;
	return cl_connectionGenerationIssuer;
}

// The app currently being serviced this frame — the "faulting-app cursor". A
// recoverable error (TERM_CLIENT_DROP/LEAVE/KICK) inside an app's per-frame
// service must disconnect + longjmp THAT app, not the input-focused one. Default
// is the focused app (re-armed at the CL_Frame call boundary in common.c each
// frame); the non-focused per-app loop points it at the iterated app per service,
// then restores it. At N=1 it is always clientApps[0] == clientActiveApp.
clientApp_t			*cl_frameApp = &clientApps[0];

// Is the faulting-app cursor's per-frame recovery point (cl_frameApp->appAbortFrame)
// currently armed? The per-app jmp_buf is only valid between the Q_setjmp at the
// CL_Frame call boundary (common.c) and the end of that protected section; outside
// that window (e.g. during the startup cbuf '+map <bad>' / '+demo <corrupt>' that
// runs in Com_Frame BEFORE the per-app setjmp, or during Com_EventLoop / init) it
// is a zero-initialized buffer and longjmp'ing through it is undefined behaviour.
// Com_Terminate consults this so a recoverable drop raised outside the armed window
// falls back to the process-global abortframe (always armed during a frame / init)
// instead of crashing. Single-app: clientApps[0] only; engine sets/clears it around
// the per-app setjmp section in common.c and in the non-focused per-app service loop.
static qboolean cl_frameAbortArmed = qfalse;

// Accessors so qcommon (Com_Terminate in log.c) can target the faulting app's
// disconnect + per-frame recovery point without seeing the clientApp_t layout.
clientApp_t *CL_FrameApp( void ) {
	return cl_frameApp;
}
jmp_buf *CL_FrameAppAbort( void ) {
	return &cl_frameApp->appAbortFrame;
}
qboolean CL_FrameAbortArmed( void ) {
	return cl_frameAbortArmed;
}
void CL_SetFrameAbortArmed( qboolean armed ) {
	cl_frameAbortArmed = armed;
}

// Active-app accessor — pull-model source for the WUI activation predicates.
// Single-app: the pinned slot 0. clientActiveApp is initialised to &clientApps[0]
// and never cleared, so this is never NULL.
clientApp_t *CL_ActiveApp( void ) {
	return clientActiveApp;
}

// The active app's cgame slot index (its clientApps[] index == its cgameInstance,
// per the slot↔app mapping in cl_cgame.c). Stable regardless of whether the cgame
// VM is currently loaded, so the engine's level-transition teardown can scope its
// VM clear to this app without reaching into clientApp_t internals. Single-app:
// clientActiveApp == &clientApps[0], so this is 0.
int CL_ActiveCgameInstance( void ) {
	return (int)( clientActiveApp - clientApps );
}
clientStatic_t		cls;
clLoadProgress_t	cl_loadProgress;
// The cgame VM handle is no longer a bare global — it lives in the per-app
// container (clientApps[N].cgvm), reached via clientActiveApp->cgvm /
// CL_ActiveApp()->cgvm. Deglobalized (zero-init by the clientApps
// array's static storage).

char				cl_oldGame[ MAX_QPATH ];
qboolean			cl_oldGameSet;
static	qboolean	noGameRestart = qfalse;


// Structure containing functions exported from refresh DLL
refexport_t	re;
#ifdef USE_RENDERER_DLOPEN
static void	*rendererLib;
// Physical path the renderer DLL was dlopen'd from, captured engine-side at load
// (the DLL can't know its own path — NOT routed via the r_buildId cvar). Read by
// the sysinfo "loaded-from" diagnostic via CL_RendererLoadPath(). Cleared on
// CL_ShutdownRef. Always a loose DLL in the install binary dir (never a pak).
static char	s_rendererLoadPath[ MAX_OSPATH ];
// Pointer to the renderer DLL's own static refexport_t (the one returned by
// GetRefAPI). Kept across the BeginRegistration call so the recoverable
// init-failure flag (initFailed) the renderer sets DURING BeginRegistration
// can be polled by re-reading the DLL-side struct. The engine-side `re` is
// a COPY made at CL_InitRef time; without this pointer we'd never observe
// the DLL's late mutation.
static refexport_t *s_re_dll;
// Ordered renderer fallback list — cl_renderer is advanced through this on
// recoverable init failure. Ordering matches CMakeLists.txt's RENDERER_DEFAULT
// comment: vulkan first (current default), opengl2 next, opengl as ultimate
// fallback. Adding a new renderer means appending here.
static const char *s_renderer_fallback_list[] = { "vulkan", "opengl2", "opengl" };
#endif

static ping_t cl_pinglist[MAX_PINGREQUESTS];

#define CL_INFO_CHALLENGE_BYTES 16
#define CL_INFO_CHALLENGE_CHARS ( CL_INFO_CHALLENGE_BYTES * 2 )
#define CL_LOCAL_DISCOVERY_TIMEOUT_MS 3000u
#define CL_MASTER_DISCOVERY_TIMEOUT_MS 3000u

typedef struct {
	char challenge[CL_INFO_CHALLENGE_CHARS + 1];
	unsigned int generation;
	unsigned int start;
	unsigned int timeout;
	qboolean active;
} localDiscovery_t;

typedef struct {
	netadr_t address;
	qboolean extended;
} masterDiscoverySource_t;

typedef struct {
	masterDiscoverySource_t sources[MAX_MASTER_SERVERS];
	unsigned int generation;
	unsigned int start;
	unsigned int timeout;
	int sourceCount;
	qboolean active;
} masterDiscovery_t;

static localDiscovery_t cl_localDiscovery;
static masterDiscovery_t cl_masterDiscovery;
static unsigned int cl_infoChallengeGeneration;
static unsigned int cl_masterDiscoveryGeneration;

static void hash_reset( void );

static void CL_BumpGlobalServerGeneration( void ) {
	cls.globalServerGeneration++;
	if ( cls.globalServerGeneration == 0 ) cls.globalServerGeneration++;
}

static void CL_ClearServerQueryTransactions( void ) {
	/* Discovery and directed-ping authority must not cross a client
	 * shutdown/re-init boundary.  Keep the process-wide generation counter so
	 * the next request still receives a distinct identity. */
	memset( &cl_localDiscovery, 0, sizeof( cl_localDiscovery ) );
	memset( &cl_masterDiscovery, 0, sizeof( cl_masterDiscovery ) );
	memset( cl_pinglist, 0, sizeof( cl_pinglist ) );
	hash_reset();
}

typedef struct serverStatus_s
{
	char string[BIG_INFO_STRING];
	char challenge[24];
	netadr_t address;
	int time, startTime;
	qboolean pending;
	qboolean print;
	qboolean retrieved;
} serverStatus_t;

static serverStatus_t cl_serverStatusList[MAX_SERVERSTATUSREQUESTS];
static unsigned cl_serverStatusChallengeSerial;

static void CL_CheckForResend( void );
static void CL_ShowIP_f( void );
static void CL_ServerStatus_f( void );
static void CL_ServerStatusResponse( const netadr_t *from, msg_t *msg );
static void CL_ServerInfoPacket( const netadr_t *from, msg_t *msg );

static void CL_LocalServers_f( void );
static void CL_GlobalServers_f( void );
static void CL_Ping_f( void );

static void CL_InitRef( void );
static void CL_ShutdownRef( refShutdownCode_t code );
static void CL_InitGLimp_Cvars( void );
static void CL_GpuMemReport( void );
static void CL_RconLogin_f( void );
static void CL_Rcon_f( void );

static void CL_NextDemo( void );
void CL_DownloadsComplete_Tick( void );

static cvar_t *cl_wiredRconPassword;

qboolean CL_DemoPlaying( void ) {
	return clientActiveApp->clc.demoplaying;
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
void CL_AddReliableCommand( clientApp_t *app, const char *cmd, qboolean isDisconnectCmd ) {
	int unacknowledged = app->clc.reliableSequence - app->clc.reliableAcknowledge;

	if ( app->clc.serverAddress.type == NA_BAD )
		return;

	// if we would be losing an old command that hasn't been acknowledged,
	// we must drop the connection
	// also leave one slot open for the disconnect command in this case.

	if ((isDisconnectCmd && unacknowledged > MAX_RELIABLE_COMMANDS) ||
		(!isDisconnectCmd && unacknowledged >= MAX_RELIABLE_COMMANDS))
	{
		if( com_errorEntered )
			return;
		Com_Terminate( TERM_CLIENT_DROP, "Client command overflow" );
	}

	app->clc.reliableSequence++;
	int index = app->clc.reliableSequence & ( MAX_RELIABLE_COMMANDS - 1 );
	Q_strncpyz( app->clc.reliableCommands[ index ], cmd, sizeof( app->clc.reliableCommands[ index ] ) );
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
	int len = clientActiveApp->clc.serverMessageSequence;
	int swlen = LittleLong( len );
	FS_Write( &swlen, 4, clientActiveApp->clc.recordfile );

	// skip the packet sequencing information
	len = msg->cursize - headerBytes;
	swlen = LittleLong(len);
	FS_Write( &swlen, 4, clientActiveApp->clc.recordfile );
	FS_Write( msg->data + headerBytes, len, clientActiveApp->clc.recordfile );
}

static void CL_WriteGamestate( qboolean initial );
static void CL_WriteSnapshot( void );

/*
====================
CL_RecordCommittedSnapshot

The WiredNet receive paths do not expose one legacy packet blob to copy into a
demo.  Once CL_ParseSnapshot has validated and committed a snapshot, re-encode
that authoritative state through the canonical demo writer.  Invalid deltas
and pre-commit packet-entity state never reach the recording.
====================
*/
void CL_RecordCommittedSnapshot( clientApp_t *app ) {
	if ( app != clientActiveApp || !app->clc.demorecording
	  || app->clc.demoplaying
	  || app->clc.recordfile == FS_INVALID_HANDLE ) {
		return;
	}
	app->clc.demowaiting = qfalse;
	CL_WriteSnapshot();
}


/*
====================
CL_StopRecording_f

stop recording a demo
====================
*/
void CL_StopRecord_f( void ) {

	if ( clientActiveApp->clc.recordfile != FS_INVALID_HANDLE ) {
		char tempName[MAX_OSPATH];
		char finalName[MAX_OSPATH];
		int protocol = PROTOCOL_VERSION;

		// finish up
		int len = -1;
		FS_Write( &len, 4, clientActiveApp->clc.recordfile );
		FS_Write( &len, 4, clientActiveApp->clc.recordfile );
		FS_FCloseFile( clientActiveApp->clc.recordfile );
		clientActiveApp->clc.recordfile = FS_INVALID_HANDLE;

		if ( com_protocol->integer != PROTOCOL_VERSION ) {
			protocol = com_protocol->integer;
		}

		Com_sprintf( tempName, sizeof( tempName ), "%s.tmp", clientActiveApp->clc.recordName );

		Com_sprintf( finalName, sizeof( finalName ), "%s.%s%d", clientActiveApp->clc.recordName, DEMOEXT, protocol );

		if ( clientActiveApp->clc.explicitRecordName ) {
			/* Demo file is written via FS_FOpenFileWrite (homepath/fs_gamedir/...);
			 * FS_HomeRemove rebuilds the same path. */
			FS_HomeRemove( finalName );
		} else {
			// add sequence suffix to avoid overwrite
			int sequence = 0;
			while ( FS_FileExists( finalName ) && ++sequence < 1000 ) {
				Com_sprintf( finalName, sizeof( finalName ), "%s-%02d.%s%d",
					clientActiveApp->clc.recordName, sequence, DEMOEXT, protocol );
			}
		}

		FS_Rename( tempName, finalName );
	}

	if ( !clientActiveApp->clc.demorecording ) {
		Com_Log( SEV_INFO, LOG_CH(ch_client), "Not recording a demo.\n" );
	} else {
		Com_Log( SEV_INFO, LOG_CH(ch_client), "Stopped demo recording.\n" );
	}

	clientActiveApp->clc.demorecording = qfalse;
	clientActiveApp->clc.spDemoRecording = qfalse;
}


/*
====================
CL_WriteServerCommands
====================
*/
static void CL_WriteServerCommands( msg_t *msg ) {
	if ( clientActiveApp->clc.serverCommandSequence - clientActiveApp->clc.demoCommandSequence > 0 ) {

		// do not write more than MAX_RELIABLE_COMMANDS
		if ( clientActiveApp->clc.serverCommandSequence - clientActiveApp->clc.demoCommandSequence > MAX_RELIABLE_COMMANDS ) {
			clientActiveApp->clc.demoCommandSequence = clientActiveApp->clc.serverCommandSequence - MAX_RELIABLE_COMMANDS;
		}

		for ( int i = clientActiveApp->clc.demoCommandSequence + 1 ; i <= clientActiveApp->clc.serverCommandSequence; i++ ) {
			MSG_WriteByte( msg, svc_serverCommand );
			MSG_WriteLong( msg, i );
			MSG_WriteString( msg, clientActiveApp->clc.serverCommands[ i & (MAX_RELIABLE_COMMANDS-1) ] );
		}
	}

	clientActiveApp->clc.demoCommandSequence = clientActiveApp->clc.serverCommandSequence;
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
	MSG_WriteLong( &msg, clientActiveApp->clc.reliableSequence );

	if ( initial ) {
		clientActiveApp->clc.demoMessageSequence = 1;
		clientActiveApp->clc.demoCommandSequence = clientActiveApp->clc.serverCommandSequence;
	} else {
		CL_WriteServerCommands( &msg );
	}

	clientActiveApp->clc.demoDeltaNum = 0; // reset delta for next snapshot

	MSG_WriteByte( &msg, svc_gamestate );
	MSG_WriteLong( &msg, clientActiveApp->clc.serverCommandSequence );

	// configstrings
	for ( int i = 0 ; i < MAX_CONFIGSTRINGS ; i++ ) {
		if ( !clientActiveApp->cl.gameState.stringOffsets[i] ) {
			continue;
		}
		char *s = clientActiveApp->cl.gameState.stringData + clientActiveApp->cl.gameState.stringOffsets[i];
		MSG_WriteByte( &msg, svc_configstring );
		MSG_WriteShort( &msg, i );
		MSG_WriteBigString( &msg, s );
	}

	// baselines
	entityState_t nullstate;
	memset( &nullstate, 0, sizeof( nullstate ) );
	for ( int i = 0; i < MAX_GENTITIES ; i++ ) {
		if ( !clientActiveApp->cl.baselineUsed[ i ] )
			continue;
		entityState_t *ent = &clientActiveApp->cl.entityBaselines[ i ];
		MSG_WriteByte( &msg, svc_baseline );
		MSG_WriteDeltaEntity( &msg, &nullstate, ent, qtrue );
	}

	// finalize message
	MSG_WriteByte( &msg, svc_EOF );

	// finished writing the gamestate stuff

	// write the client num
	MSG_WriteLong( &msg, clientActiveApp->clc.clientNum );

	// write the checksum feed
	MSG_WriteLong( &msg, clientActiveApp->clc.checksumFeed );

	// finished writing the client packet
	MSG_WriteByte( &msg, svc_EOF );

	// write it to the demo file
	int len;
	if ( clientActiveApp->clc.demoplaying )
		len = LittleLong( clientActiveApp->clc.demoMessageSequence - 1 );
	else
		len = LittleLong( clientActiveApp->clc.serverMessageSequence - 1 );

	FS_Write( &len, 4, clientActiveApp->clc.recordfile );

	len = LittleLong( msg.cursize );
	FS_Write( &len, 4, clientActiveApp->clc.recordfile );
	FS_Write( msg.data, msg.cursize, clientActiveApp->clc.recordfile );
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
			newent = &clientActiveApp->cl.parseEntities[(to->parseEntitiesNum + newindex) % MAX_PARSE_ENTITIES];
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
			// NOLINTNEXTLINE(clang-analyzer-security.ArrayBound) — newnum < MAX_GENTITIES is enforced upstream by snapshot encoder
			MSG_WriteDeltaEntity (msg, &clientActiveApp->cl.entityBaselines[newnum], newent, qtrue );
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

	clSnapshot_t *snap = &clientActiveApp->cl.snapshots[ clientActiveApp->cl.snap.messageNum & PACKET_MASK ]; // current snapshot
	//if ( !snap->valid ) // should never happen?
	//	return;

	clSnapshot_t *oldSnap;
	if ( clientActiveApp->clc.demoDeltaNum == 0 ) {
		oldSnap = NULL;
	} else {
		oldSnap = &saved_snap;
	}

	MSG_Init( &msg, bufData, MAX_MSGLEN );
	MSG_Bitstream( &msg );

	// NOTE, MRE: all server->client messages now acknowledge
	MSG_WriteLong( &msg, clientActiveApp->clc.reliableSequence );

	// Write all pending server commands
	CL_WriteServerCommands( &msg );

	MSG_WriteByte( &msg, svc_snapshot );
	MSG_WriteLong( &msg, snap->serverTime ); // sv.time
	MSG_WriteByte( &msg, clientActiveApp->clc.demoDeltaNum ); // 0 or 1
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
	if ( clientActiveApp->clc.demoplaying )
		len = LittleLong( clientActiveApp->clc.demoMessageSequence );
	else
		len = LittleLong( clientActiveApp->clc.serverMessageSequence );
	FS_Write( &len, 4, clientActiveApp->clc.recordfile );

	len = LittleLong( msg.cursize );
	FS_Write( &len, 4, clientActiveApp->clc.recordfile );
	FS_Write( msg.data, msg.cursize, clientActiveApp->clc.recordfile );

	// save last sent state so if there any need - we can skip any further incoming messages
	for ( int i = 0; i < snap->numEntities; i++ )
		saved_ents[ i ] = clientActiveApp->cl.parseEntities[ (snap->parseEntitiesNum + i) % MAX_PARSE_ENTITIES ];

	saved_snap = *snap;
	saved_snap.parseEntitiesNum = 0;

	clientActiveApp->clc.demoMessageSequence++;
	clientActiveApp->clc.demoDeltaNum = 1;
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
		Com_Log( SEV_INFO, LOG_CH(ch_client), "record <demoname>\n" );
		return;
	}

	if ( clientActiveApp->clc.demorecording ) {
		if ( !clientActiveApp->clc.spDemoRecording ) {
			Com_Log( SEV_INFO, LOG_CH(ch_client), "Already recording.\n" );
		}
		return;
	}

	if ( clientActiveApp->state != CA_ACTIVE ) {
		Com_Log( SEV_INFO, LOG_CH(ch_client), "You must be in a level to record.\n" );
		return;
	}

	// sync 0 doesn't prevent recording, so not forcing it off .. everyone does g_sync 1 ; record ; g_sync 0 ..
	if ( NET_IsLocalAddress( &clientActiveApp->clc.serverAddress ) && !Cvar_VariableIntegerValue( "g_synchronousClients" ) ) {
		COM_WARN( LOG_CH(ch_client), "WARNING: You should set 'g_synchronousClients 1' for smoother demo recording\n" );
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

		clientActiveApp->clc.explicitRecordName = qtrue;
	} else {
		qtime_t t;
		Com_RealTime( &t );
		/* use YYYY_MM_DD-HH_MM_SS filename format so
		   timestamped demos sort lexicographically. */
		Com_sprintf( name, sizeof( name ), "demos/%04d_%02d_%02d-%02d_%02d_%02d",
			1900 + t.tm_year, 1 + t.tm_mon, t.tm_mday,
			t.tm_hour, t.tm_min, t.tm_sec );

		clientActiveApp->clc.explicitRecordName = qfalse;
	}

	// save desired filename without extension
	Q_strncpyz( clientActiveApp->clc.recordName, name, sizeof( clientActiveApp->clc.recordName ) );

	Com_Log( SEV_INFO, LOG_CH(ch_client), "recording to %s.\n", name );

	// start new record with temporary extension
	{ qstring_t _nm_qs = QS_WrapExisting( name, sizeof( name ) ); QS_Append( &_nm_qs, ".tmp" ); }

	// open the demo file
	clientActiveApp->clc.recordfile = FS_FOpenFileWrite( name );
	if ( clientActiveApp->clc.recordfile == FS_INVALID_HANDLE ) {
		Com_Log( SEV_INFO, LOG_CH(ch_client), "ERROR: couldn't open.\n" );
		clientActiveApp->clc.recordName[0] = '\0';
		return;
	}

	clientActiveApp->clc.demorecording = qtrue;

	Com_TruncateLongString( clientActiveApp->clc.recordNameShort, clientActiveApp->clc.recordName );

	if ( Cvar_VariableIntegerValue( "ui_recordSPDemo" ) ) {
	  clientActiveApp->clc.spDemoRecording = qtrue;
	} else {
	  clientActiveApp->clc.spDemoRecording = qfalse;
	}

	// don't start saving messages until a non-delta compressed message is received
	clientActiveApp->clc.demowaiting = qtrue;

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
		int time = Sys_Milliseconds() - clientActiveApp->clc.timeDemoStart;
		if ( time > 0 ) {
			Com_Log( SEV_INFO, LOG_CH(ch_client), "%i frames, %3.*f seconds: %3.1f fps\n", clientActiveApp->clc.timeDemoFrames,
			time > 10000 ? 1 : 2, time/1000.0, clientActiveApp->clc.timeDemoFrames*1000.0 / time );
		}
	}

	CL_Disconnect( clientActiveApp, qtrue );
	if ( WiredAttract_OnDemoCompleted() ) {
		return; /* attract scheduler handled the advance */
	}
	CL_NextDemo();
}

typedef struct {
	clientApp_t *app;
	fileHandle_t file;
} clDemoFileReadContext_t;

static void CL_ClearDemoReadFault( clientApp_t *app ) {
	app->demoReadFaultArmed = qfalse;
	app->demoReadFaultAfterBytes = 0;
	app->demoReadFaultRemaining = 0;
}

static int CL_DemoFileRead( void *context, void *buffer, size_t length ) {
	clDemoFileReadContext_t *readContext = context;
	clientApp_t *app = readContext->app;
	int count;

	if ( length > (size_t)INT_MAX ) return -1;
	if ( app->demoReadFaultArmed ) {
		if ( app->demoReadFaultRemaining == 0 ) {
			const int afterBytes = app->demoReadFaultAfterBytes;
			CL_ClearDemoReadFault( app );
			Com_Log( SEV_DEBUG, LOG_CH(ch_client),
				"Demo read fault injected after_bytes=%d requested=%zu\n",
				afterBytes, length );
			return -1;
		}
		if ( length > (size_t)app->demoReadFaultRemaining ) {
			length = (size_t)app->demoReadFaultRemaining;
		}
	}

	count = FS_Read( buffer, (int)length, readContext->file );
	if ( count > 0 && app->demoReadFaultArmed ) {
		if ( count > app->demoReadFaultRemaining ) {
			CL_ClearDemoReadFault( app );
			return -1;
		}
		app->demoReadFaultRemaining -= count;
	}
	return count;
}

static void CL_DemoReadFault_f( void ) {
	char *parseEnd = NULL;
	long afterBytes;

	if ( !com_automated || !com_automated->integer ) {
		Com_Log( SEV_WARN, LOG_CH(ch_client),
			"Demo read fault refused automated=0\n" );
		return;
	}
	if ( Cmd_Argc() != 2 ) {
		Com_Log( SEV_INFO, LOG_CH(ch_client),
			"demo_read_fault <after-bytes>\n" );
		return;
	}

	errno = 0;
	afterBytes = strtol( Cmd_Argv( 1 ), &parseEnd, 10 );
	if ( errno == ERANGE || !parseEnd || parseEnd == Cmd_Argv( 1 ) || *parseEnd
	  || afterBytes < 0 || afterBytes > MAX_MSGLEN_BUF ) {
		Com_Log( SEV_WARN, LOG_CH(ch_client),
			"Demo read fault rejected invalid_budget=1\n" );
		return;
	}

	clientActiveApp->demoReadFaultArmed = qtrue;
	clientActiveApp->demoReadFaultAfterBytes = (int)afterBytes;
	clientActiveApp->demoReadFaultRemaining = (int)afterBytes;
	Com_Log( SEV_DEBUG, LOG_CH(ch_client),
		"Demo read fault armed after_bytes=%ld\n", afterBytes );
}

static const char *CL_DemoFailureText( clDemoFrameStatus_t status ) {
	switch ( status ) {
	case CL_DEMO_FRAME_MISSING_TERMINATOR:
		return "The demo ended without its required terminator.";
	case CL_DEMO_FRAME_TRUNCATED_SEQUENCE:
		return "The demo has a truncated message sequence.";
	case CL_DEMO_FRAME_TRUNCATED_LENGTH:
		return "The demo has a truncated message length.";
	case CL_DEMO_FRAME_INVALID_TERMINATOR:
		return "The demo has an invalid end marker.";
	case CL_DEMO_FRAME_INVALID_LENGTH:
		return "The demo has an invalid message length.";
	case CL_DEMO_FRAME_EMPTY_PAYLOAD:
		return "The demo contains an empty message payload.";
	case CL_DEMO_FRAME_OVERSIZE:
		return "The demo contains a message larger than the protocol limit.";
	case CL_DEMO_FRAME_TRUNCATED_PAYLOAD:
		return "The demo has a truncated message payload.";
	case CL_DEMO_FRAME_IO_ERROR:
	default:
		return "The demo could not be read.";
	}
}

static void CL_DemoFailedWithText( const char *reason, const char *message ) {
	char nextDemo[MAX_CVAR_VALUE_STRING];
	qboolean attractOwned;
	qboolean scriptedContinuation;

	Cvar_VariableStringBuffer( "nextdemo", nextDemo, sizeof( nextDemo ) );
	attractOwned = WiredAttract_IsDemoOverlayActive();
	scriptedContinuation = nextDemo[0] != '\0';
	Com_Log( SEV_INFO, LOG_CH(ch_client),
		"Demo playback rejected reason=%s continuation=%d\n",
		reason, ( attractOwned || scriptedContinuation ) ? 1 : 0 );
	Com_SetLastError( "%s", message );
	CL_Disconnect( clientActiveApp, qfalse );

	if ( attractOwned ) {
		Com_ClearLastError();
		if ( WiredAttract_OnDemoFailed() ) return;
		Com_SetLastError( "%s", message );
	}
	if ( scriptedContinuation ) {
		Com_ClearLastError();
		CL_NextDemo();
		return;
	}

	if ( cls.uiStarted ) {
		WiredUI_SetActiveMenu( UIMENU_MAIN );
		WiredUI_PushMenu( "demos", WUI_BG_INTENT_SCENE );
		if ( com_automated && com_automated->integer ) {
			Com_ClearLastError();
			Com_Log( SEV_INFO, LOG_CH(ch_client),
				"Demo playback recovery reason=%s depth=%d popup=0 automated=1\n",
				reason, WiredUI_GetMenuStackDepth() );
		} else {
			CL_WiredUI_ShowError( "Demo Playback Failed", message, qfalse );
			Com_Log( SEV_INFO, LOG_CH(ch_client),
				"Demo playback recovery reason=%s depth=%d popup=1 automated=0\n",
				reason, WiredUI_GetMenuStackDepth() );
		}
	}
}

static void CL_DemoFailed( clDemoFrameStatus_t status ) {
	CL_DemoFailedWithText( CL_DemoFrameStatusName( status ),
		CL_DemoFailureText( status ) );
}


/*
=================
CL_ReadDemoMessage
=================
*/
void CL_ReadDemoMessage( void ) {
	msg_t		buf;
	byte		bufData[ MAX_MSGLEN_BUF ];
	clDemoFrame_t frame;
	clDemoFrameStatus_t frameStatus;
	clDemoFileReadContext_t readContext;

	if ( clientActiveApp->clc.demofile == FS_INVALID_HANDLE ) {
		CL_DemoCompleted();
		return;
	}

	MSG_Init( &buf, bufData, MAX_MSGLEN );
	readContext.app = clientActiveApp;
	readContext.file = clientActiveApp->clc.demofile;
	frameStatus = CL_DemoFrameRead( CL_DemoFileRead,
		&readContext, buf.data, (size_t)buf.maxsize, &frame );
	if ( frameStatus == CL_DEMO_FRAME_END ) {
		CL_DemoCompleted();
		return;
	}
	if ( frameStatus != CL_DEMO_FRAME_MESSAGE ) {
		CL_DemoFailed( frameStatus );
		return;
	}
	clientActiveApp->clc.serverMessageSequence = frame.sequence;
	buf.cursize = (int)frame.payloadLength;

	clientActiveApp->clc.lastPacketTime = cls.realtime;
	buf.readcount = 0;

	clientActiveApp->clc.demoCommandSequence = clientActiveApp->clc.serverCommandSequence;

	// Demo playback parses into the active app. The frame envelope may be valid
	// while its payload contains a bounded typed semantic failure (currently an
	// illegal top-level svc or an oversized snapshot areamask declaration). Arm a
	// file-local recovery boundary only for this parse; live network messages stay
	// on the generic TERM_CLIENT_DROP path in CL_ParseServerMessage.
	clientActiveApp->demoMessageAbortArmed = qtrue;
	clientActiveApp->demoMessageAbortCommand = -1;
	clientActiveApp->demoMessageAbortKind = DEMO_MESSAGE_ABORT_NONE;
	clientActiveApp->demoMessageAbortDetail = -1;
	if ( Q_setjmp( clientActiveApp->demoMessageAbort ) ) {
		const int recoveredCommand = clientActiveApp->demoMessageAbortCommand;
		const demoMessageAbortKind_t recoveredKind = clientActiveApp->demoMessageAbortKind;
		const int recoveredDetail = clientActiveApp->demoMessageAbortDetail;
		clientActiveApp->demoMessageAbortArmed = qfalse;
		clientActiveApp->demoMessageAbortCommand = -1;
		clientActiveApp->demoMessageAbortKind = DEMO_MESSAGE_ABORT_NONE;
		clientActiveApp->demoMessageAbortDetail = -1;
		if ( recoveredKind == DEMO_MESSAGE_ABORT_SNAPSHOT_AREAMASK ) {
			Com_Log( SEV_DEBUG, LOG_CH(ch_client),
				"Demo semantic parse recovered command=%d detail=snapshot-areamask areabytes=%d generic_teardown=0\n",
				recoveredCommand, recoveredDetail );
		} else {
			Com_Log( SEV_DEBUG, LOG_CH(ch_client),
				"Demo semantic parse recovered command=%d generic_teardown=0\n",
				recoveredCommand );
		}
		CL_DemoFailedWithText( "semantic-payload",
			"The demo contains an invalid server message." );
		return;
	}
	CL_ParseServerMessage( clientActiveApp, &buf );
	clientActiveApp->demoMessageAbortArmed = qfalse;
	clientActiveApp->demoMessageAbortCommand = -1;
	clientActiveApp->demoMessageAbortKind = DEMO_MESSAGE_ABORT_NONE;
	clientActiveApp->demoMessageAbortDetail = -1;

	if ( clientActiveApp->clc.demorecording ) {
		// track changes and write new message
		if ( clientActiveApp->clc.eventMask & EM_GAMESTATE ) {
			CL_WriteGamestate( qfalse );
			// nothing should came after gamestate in current message
		} else if ( clientActiveApp->clc.eventMask & (EM_SNAPSHOT|EM_COMMAND) ) {
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
			Com_Log( SEV_INFO, LOG_CH(ch_client), "Demo file: %s\n", name );
			return demo_protocols[ i ];
		}
		Com_Log( SEV_INFO, LOG_CH( ch_client ), "Not found: %s\n", name );
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
CL_ConsoleCloseForConnect

Close the console as part of entering a connection/demo, EXCEPT when an attract
reel is driving the transition. The fullscreen console is a separate top layer
(WUI_LAYER_CONSOLE) the user opens and closes deliberately; a user-initiated
`\demo` or `\connect` should drop it, but attract advancing its own reel must not
steal it (Eser 2026-07-03: "console is a different layer, never interrupted by
attract screen changes"). Attract keeps the console alive by collapsing it
visually while preserving KEYCATCH_CONSOLE — the same idiom the map-change path
(CL_MapLoading) already uses. Single decision point for all connect-time console
closes (CL_PlayDemo_f, CL_ParseGamestate, CL_WiredNetBootstrapResetState).
====================
*/
void CL_ConsoleCloseForConnect( void ) {
	if ( WiredAttract_IsActive() )
		Con_SoftClose();   // reel transition — layer survives, user still owns ~
	else
		Con_Close();        // user-initiated connect/demo — drop the console
}


/*
====================
CL_OnClientStateChanged

The single reaction point for a client connection-state edge. Called by
CL_SetState only for the input-focused app (app == clientActiveApp). Cross-
cutting reactions to "the connection state just changed" belong HERE, not
inlined into the wire-parse / demo-pump functions that happen to cause the edge
(Eser 2026-07-03: those should be side-effect-free).

The session-entry edge = a transition INTO CA_CONNECTING, CA_CONNECTED, or
CA_LOADING. This covers every way a new session/map begins:
  - user \connect         → CA_CONNECTING
  - \demo <file>          → CA_CONNECTED
  - local \map (fresh)    → CA_CONNECTING; (localhost rotation) → CA_CONNECTED
  - download-queued join  → CA_CONNECTED
  - REMOTE server-pushed mid-session map change (no command typed, no download
    needed) → CA_ACTIVE → CA_LOADING, which never touches CA_CONNECTING/CONNECTED.
CA_LOADING is REQUIRED for that last case — without it the remote map rotation
would not hard-close the console (only CL_InitCGame's soft-close would collapse
it, wrongly preserving KEYCATCH_CONSOLE). CA_LOADING is entered ONLY inside the
map-load pipeline (protocol.h: "only during cgame initialization, never during
the main loop"), so keying on it produces no false close. Transitions into
PRIMED / ACTIVE / CINEMATIC / DISCONNECTED are NOT session-entry and must not
close. Because CL_SetState funnels ALL transitions, this fires on the server-
pushed edge exactly as on a typed command — which a command-dispatch owner could
not do (flow d has no command). The oldState!=newState guard in CL_SetState
collapses the double CA_LOADING set (CL_DownloadsComplete then CL_InitCGame) to a
single fire; redundant fires on connect/demo (already closed at CONNECTING/
CONNECTED) are harmless — Con_Close/Con_SoftClose are idempotent.

The attract distinction stays a property of the moment (WiredAttract_IsActive at
transition time): an attract reel advancing a demo soft-closes (console layer
survives), a user connect hard-closes.
====================
*/
static void CL_OnClientStateChanged( clientApp_t *app, connstate_t oldState, connstate_t newState ) {
	(void)app;
	(void)oldState;
	/* A server that has advanced us beyond admission no longer needs browser
	 * retry metadata.  The credential itself was already erased immediately
	 * after transport->connect copied CONNECT userinfo. */
	if ( newState >= CA_CONNECTED && app->clc.joinAttempt.browserOrigin ) {
		Q_SecureZeroMemory( &app->clc.joinAttempt, sizeof( app->clc.joinAttempt ) );
	}
	// Session-entry edge → close the console (attract-aware). Other edges: no-op.
	if ( newState == CA_CONNECTING || newState == CA_CONNECTED || newState == CA_LOADING ) {
		CL_ConsoleCloseForConnect();
	}
}


/*
====================
CL_SetState

The single funnel for client connection-state changes. Previously ~20 sites did
a bare `app->state = CA_XXX;`, so a state transition was invisible — there was no
seam to react to "a connection began." Routing every write through here makes the
edge observable (CL_OnClientStateChanged) without the low-level functions naming
the reaction. Only the input-focused app's transitions notify (a background
in-process client must not drive host-global UI).
====================
*/
void CL_SetState( clientApp_t *app, connstate_t newState ) {
	connstate_t oldState = app->state;
	app->state = newState;
	if ( app == clientActiveApp && oldState != newState ) {
		CL_OnClientStateChanged( app, oldState, newState );
	}
}


/*
====================
CL_DemoOpenFailed

Preserve the historical console/demo-reel behaviour, but give an authored
Demos-menu launch a local recovery when its loose file disappeared after the
feeder snapshot was built.  The UI branch deliberately does not echo the
qpath: it is an inventory race, not a path diagnostic.

====================
*/
static void CL_DemoOpenFailed( qboolean uiOwned, const char *stage,
	qboolean disconnected, const char *name ) {
	char nextDemo[MAX_CVAR_VALUE_STRING];

	CL_ClearDemoReadFault( clientActiveApp );
	Cvar_VariableStringBuffer( "nextdemo", nextDemo, sizeof( nextDemo ) );
	if ( uiOwned ) {
		const char *message = "The selected demo is no longer available.";

		/* A continuation left by a previous/demo-reel owner must not hijack an
		 * authored Demos-menu activation.  UI origin owns its own bounded
		 * recovery; the direct `demo` command retains historical nextdemo. */
		if ( nextDemo[0] ) {
			Cvar_Set( "nextdemo", "" );
		}
		Com_Log( SEV_INFO, LOG_CH(ch_client),
			"Demo playback open rejected origin=demo-ui stage=%s continuation=0 disconnect=%d\n",
			stage, disconnected ? 1 : 0 );
		Com_SetLastError( "%s", message );
		if ( cls.uiStarted ) {
			WiredUI_SetActiveMenu( UIMENU_MAIN );
			WiredUI_PushMenu( "demos", WUI_BG_INTENT_SCENE );
			CL_WiredUI_ShowError( "Demo Playback Failed", message, qfalse );
			Com_Log( SEV_INFO, LOG_CH(ch_client),
				"Demo playback open recovery origin=demo-ui stage=%s depth=%d popup=%d\n",
				stage, WiredUI_GetMenuStackDepth(),
				( com_automated && com_automated->integer ) ? 0 : 1 );
		}
		return;
	}

	// Direct `demo` and any authored `nextdemo` continuation retain their
	// established diagnostic and successor behaviour.
	COM_WARN( LOG_CH(ch_client), "couldn't open %s\n", name );
	CL_NextDemo();
}

/*
====================
CL_PlayDemoCommand

demo <demoname>

====================
*/
static void CL_PlayDemoCommand( qboolean uiOwned ) {
	char		name[MAX_OSPATH];
	qboolean	preserveReadFault;
	int		readFaultAfterBytes;

	if ( Cmd_Argc() != 2 ) {
		Com_Log( SEV_INFO, LOG_CH(ch_client), "demo <demoname>\n" );
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

			Com_Log( SEV_INFO, LOG_CH(ch_client), "Protocol %d not supported for demos\n", protocol );
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
		CL_DemoOpenFailed( uiOwned, "probe", qfalse, name );
		return;
	}

	FS_FCloseFile( hFile );
	hFile = FS_INVALID_HANDLE;

	// make sure a local server is killed
	// 2 means don't force disconnect of local client
	Cvar_Set( "sv_killserver", "2" );

	preserveReadFault = clientActiveApp->demoReadFaultArmed;
	readFaultAfterBytes = clientActiveApp->demoReadFaultAfterBytes;
	CL_Disconnect( clientActiveApp, qtrue );
	/* Preserve the historical FS-generation boundary: disconnect may restore
	 * fs_game/pure state, so the playback handle is acquired only afterwards. */
	FS_BypassPure();
	FS_FOpenFileRead( name, &hFile, qtrue );
	FS_RestorePure();
	if ( hFile == FS_INVALID_HANDLE ) {
		CL_DemoOpenFailed( uiOwned, "playback", qtrue, name );
		return;
	}
	clientActiveApp->clc.demofile = hFile;
	hFile = FS_INVALID_HANDLE;
	if ( preserveReadFault ) {
		clientActiveApp->demoReadFaultArmed = qtrue;
		clientActiveApp->demoReadFaultAfterBytes = readFaultAfterBytes;
		clientActiveApp->demoReadFaultRemaining = readFaultAfterBytes;
	}

	const char *slash, *shortname;
	if ( (slash = strrchr( name, '/' )) != NULL )
		shortname = slash + 1;
	else
		shortname = name;

	Q_strncpyz( clientActiveApp->clc.demoName, shortname, sizeof( clientActiveApp->clc.demoName ) );

	// (Console close is delegated to the CA_CONNECTED transition below via
	//  CL_OnClientStateChanged — playing a demo is not itself a UI action.)

	CL_SetState( clientActiveApp, CA_CONNECTED );
	clientActiveApp->clc.demoplaying = qtrue;
	Q_strncpyz( clientActiveApp->servername, shortname, sizeof( clientActiveApp->servername ) );

	// Read demo messages until primed. The gamestate parsed out of the demo
	// drives the client through CA_CONNECTED -> CA_LOADING (CL_DownloadsComplete
	// kicks off the async load state machine and returns) and only reaches
	// CA_PRIMED once that machine's phases have run. Com_Frame normally pumps
	// CL_DownloadsComplete_Tick once per frame, but this loop runs synchronously
	// inside a single command and never returns to Com_Frame, so we must drive
	// the phase ticks here too — otherwise the load is deferred forever and
	// CA_PRIMED is never reached (demo / timedemo would hang at CA_LOADING).
	while ( clientActiveApp->state >= CA_CONNECTED && clientActiveApp->state < CA_PRIMED ) {
		CL_ReadDemoMessage();
		CL_DownloadsComplete_Tick();
	}

	// don't get the first snapshot this frame, to prevent the long
	// time from the gamestate load from messing causing a time skip
	clientActiveApp->clc.firstDemoFrameSkipped = qfalse;
}

static void CL_PlayDemo_f( void ) {
	CL_PlayDemoCommand( qfalse );
}

static void CL_PlayDemoUI_f( void ) {
	CL_PlayDemoCommand( qtrue );
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
	Com_Log( SEV_DEBUG, LOG_CH(ch_client), "CL_NextDemo: %s\n", v );
	if ( !v[0] ) {
		return;
	}

	Cvar_Set( "nextdemo", "" );
	// A demo can complete or fail synchronously while its `demo` command is
	// itself executing from a cfg. Appending puts the successor behind that
	// cfg's remaining commands (including a possible `quit`), so it may never
	// run. Insert makes the documented continuation the immediate next command;
	// the nested execute then stops naturally at any authored wait.
	Cbuf_InsertText( v );
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
	CL_ShutdownCGame( clientActiveApp );
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
	if ( !com_cl_running->integer ) {
		// no local client subsystem — nothing to tear down
		return;
	}

	// mute level sounds; audio mixer and device stay alive
	S_DisableSounds();

	// cgame VM is level-scoped; shut it down.
	// Wired UI VM is persistent — do NOT call CL_ShutdownUI() here.
	CL_ShutdownCGame( clientActiveApp );

	// The cgame just went away, so drop the LEVEL-lifetime viewport providers
	// it registered (e.g. the world scene). Process-lifetime providers stay.
	// Slot teardown only marks the slot free + clears the struct — providers
	// own no heap here — so this is safe immediately after CG shutdown.
	WiredUI_UnregisterLevelViewportProviders();

	// Release level-scoped renderer resources.  REF_LEVEL_ONLY runs the
	// map-scoped teardown path: level pipelines + per-map images + zone
	// are released, but the renderer's persistent state (RAL backend +
	// VkDevice + descriptor pool + base pipelines + backEndData) stays
	// alive between async spawn phases. Font atlases + map textures are
	// re-uploaded on the next CL_InitRenderer; CPU-side font glyph
	// metadata (BSS) survives unchanged.
	if ( re.Shutdown ) {
		re.Shutdown( REF_LEVEL_ONLY );
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


	// level-scoped teardown first (mutes sounds, kills cgame, frees level geo)
	CL_ShutdownLevel();

	// shutdown remaining persistent VMs — Wired UI VM
	CL_ShutdownUI();

	// CL_ShutdownLevel already called re.Shutdown(REF_LEVEL_ONLY) to release
	// map-scoped resources.  For a game-switch, also destroy the window and
	// GL/Vk context entirely.  For the non-switch path, the REF_LEVEL_ONLY
	// call from CL_ShutdownLevel is sufficient — don't call it again.
	if ( re.Shutdown && CL_GameSwitch( clientActiveApp ) ) {
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

	Map_ClearMapCache();

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
	// A non-headless build always brings up its client, so there is no
	// runtime "dedicated" branch here. If the client subsystem is not running
	// (nothing to attach to the local server), there is nothing to do.
	if ( !com_cl_running->integer ) {
		return;
	}

	// Soft-close: collapse console visually but preserve KEYCATCH_CONSOLE so
	// the user sees the log wall throughout the async spawn phases.
	Con_SoftClose();
	// Fully close the menu stack before loading. The catcher drop below clears
	// KEYCATCH_UI, but a still-populated menu stack (e.g. the main menu, which
	// is now a real depth-1 stack entry) would re-assert KEYCATCH_UI on the
	// next WiredUI frame and trip the cgame assert (KEYCATCH_UI must be 0 at
	// CA_LOADING). Draining the stack keeps that invariant.
	WiredUI_CloseAllMenus();
	// An intentional map start also voids any stale error text. Without this
	// the CL_Disconnect(qtrue) below sees the leftover com_errorMessage and
	// the Plan C hook re-surfaces error_popup over the connect screen
	// (sticky/empty-popup family, qconsole-9 #3/#5).
	Com_ClearLastError();
	// Preserve the console catcher; drop all others (UI, cgame, etc.).
	Key_SetCatcher( Key_GetCatcher() & KEYCATCH_CONSOLE );

	qboolean localReconnect = ( clientActiveApp->state >= CA_CONNECTED && !Q_stricmp( clientActiveApp->servername, "localhost" ) );

	// if we are already connected to the local host, stay connected
	if ( localReconnect ) {
		CL_SetState( clientActiveApp, CA_CONNECTED );		// so the connect screen is drawn
		memset( cls.updateInfoString, 0, sizeof( cls.updateInfoString ) );
		memset( clientActiveApp->clc.serverMessage, 0, sizeof( clientActiveApp->clc.serverMessage ) );
		memset( &clientActiveApp->cl.gameState, 0, sizeof( clientActiveApp->cl.gameState ) );
		clientActiveApp->clc.lastPacketSentTime = cls.realtime - RETRANSMIT_TIMEOUT; // send packet immediately
		/* In-process-queue B4: the in-mem host keeps its connection across this
		 * map→map reconnect (no CL_Disconnect/WN_ConnectApp re-run), so its
		 * client+server rings still hold the previous map's snapshot/usercmd/
		 * reliable datagrams. Drain them so the new map starts clean. Gated on
		 * WN_HasInmemClient → no-op for a QUIC host (byte-identical). */
		if ( WN_HasInmemClient() )
			WN_ResetInmemClientRings( 0 );   /* the integrated host is app slot 0 */
	} else {
		// clear nextmap so the cinematic shutdown doesn't execute it
		Cvar_Set( "nextmap", "" );
		CL_Disconnect( clientActiveApp, qtrue );
		Q_strncpyz( clientActiveApp->servername, "localhost", sizeof(clientActiveApp->servername) );
		CL_SetState( clientActiveApp, CA_CONNECTING );		// so the connect screen is drawn
		WiredUI_SetLoadingMenu( "ui/connect.wui" );  // state→named-UI: show connect
		Key_SetCatcher( Key_GetCatcher() & KEYCATCH_CONSOLE );
	}

	memset( &cl_loadProgress, 0, sizeof( cl_loadProgress ) );
	CL_ResetLoadingScreenState();
	CL_ClearMapInfo();
	CL_ClearMapPreview();

	cl_loadProgress.startTime = cls.realtime ? cls.realtime : 1;
	cl_loadProgress.phase = "initializing";

	// Load map metadata and BSP wireframe preview for the loading screen.
	// mapname comes from SV_SpawnServer parameter (cvar not set yet at this point).
	if ( mapname && mapname[0] ) {
		CL_BuildMapPreview( mapname );
		CL_LoadMapInfo( mapname );
		CL_ApplyLoadingTheme( &cl_mapInfo );
	}

	cls.framecount++;
	SCR_UpdateScreen();

	if ( !localReconnect ) {
		clientActiveApp->clc.connectTime = cls.realtime - RECONNECT_TIMEOUT; // send packet immediately
		NET_StringToAdr( clientActiveApp->servername, &clientActiveApp->clc.serverAddress, NA_UNSPEC );
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
void CL_ClearState( clientApp_t *app ) {

//	S_StopAllSounds();

	memset( &app->cl, 0, sizeof( app->cl ) );
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
		// A running server owns fs_game for its lifetime. A client disconnect
		// must NOT restore fs_game here (that would FS_ConditionalRestart ->
		// Com_GameRestart -> SV_Shutdown, tearing the server down via the
		// client's lifecycle). Defer: leave cl_oldGameSet/cl_oldGame intact so
		// the restore still happens on a later disconnect when no server runs.
		// (The server owns the gamedir; the client does not restore it out
		// from under a live server.)
		if ( com_sv_running && com_sv_running->integer )
			return qfalse;

		cl_oldGameSet = qfalse;
		Cvar_Set( "fs_game", cl_oldGame );
		FS_ConditionalRestart( clientActiveApp->clc.checksumFeed, qtrue );
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
qboolean CL_Disconnect( clientApp_t *app, qboolean showMainMenu ) {
	qboolean cl_restarted = qfalse;
	// Host-global singletons (the screen, audio, key, UI, FS-pure-pak, and
	// loading-screen state are single per process) belong to the input-focused
	// app; a non-focused app's disconnect tears down only its own connection.
	qboolean isFocused = ( app == clientActiveApp );

	// A generic parser failure can leave CL_ReadDemoMessage through the outer
	// per-app error boundary instead of its narrow semantic-payload boundary.
	// Invalidate that stack-owned jump target before every disconnect exit,
	// including uninitialised and re-entrant teardown paths.
	app->demoMessageAbortArmed = qfalse;
	app->demoMessageAbortCommand = -1;
	app->demoMessageAbortKind = DEMO_MESSAGE_ABORT_NONE;
	app->demoMessageAbortDetail = -1;
	CL_ClearDemoReadFault( app );

	if ( !com_cl_running || !com_cl_running->integer ) {
		return cl_restarted;
	}

	// Reentry guard is per-app (was a single process-wide static) so a 2nd
	// app's disconnect does not block the host's, or vice-versa.
	if ( app->disconnecting ) {
		return cl_restarted;
	}

	app->disconnecting = qtrue;

	// Cancel any in-flight chunked-load state machine. If
	// CL_InitCGame in CL_DownloadsComplete_Tick's P2/P3 phase longjmp'd
	// out via Com_Error (e.g. a CG_INIT asset failure), the jump skipped
	// the phase -> DLC_P4_FINALIZE advance, leaving app->dlcomplete.phase
	// stuck at P2/P3. Without this reset CL_DownloadsComplete_Tick would
	// re-fire that failed phase every Com_Frame forever — the empty-
	// mapname CL_InitCGame loop. Any disconnect (error recovery or user)
	// is an unconditional abort of the load.
	app->dlcomplete.phase = DLC_IDLE;

	// Stop demo recording
	if ( app->clc.demorecording ) {
		CL_StopRecord_f();
	}

	// Stop demo playback
	if ( app->clc.demofile != FS_INVALID_HANDLE ) {
		FS_FCloseFile( app->clc.demofile );
		app->clc.demofile = FS_INVALID_HANDLE;
	}

	// Finish downloads
	if ( app->clc.download != FS_INVALID_HANDLE ) {
		FS_FCloseFile( app->clc.download );
		app->clc.download = FS_INVALID_HANDLE;
	}
	*app->clc.downloadTempName = *app->clc.downloadName = '\0';
	if ( isFocused ) {
		Cvar_Set( "cl_downloadName", "" );
	}

	// Stop recording any video (host screen)
	if ( isFocused && CL_VideoRecording() ) {
		// Finish rendering current frame
		cls.framecount++;
		SCR_UpdateScreen();
		CL_CloseAVI( qfalse );
	}

	if ( app->cgvm ) {
		// do that right after we rendered last video frame
		CL_ShutdownCGame( app );
	}

	if ( isFocused ) {
		SCR_StopCinematic();
		S_StopAllSounds();
		Key_ClearStates();

		if ( UI_VM_ACTIVE && showMainMenu ) {
			UI_CALL_SET_ACTIVE( UIMENU_NONE );
		}

		// Remove pure paks (host FS pure/referenced-pak state)
		FS_PureServerSetLoadedPaks( "", "" );
		FS_PureServerSetReferencedPaks( "", "" );

		FS_ClearPakReferences( FS_GENERAL_REF | FS_UI_REF | FS_CGAME_REF );
	}

	if ( CL_GameSwitch( app ) ) {
		// keep current gamestate and connection
		app->disconnecting = qfalse;
		return qfalse;
	}

	// send a disconnect message to the server
	// send it a few times in case one is dropped
	if ( app->state >= CA_CONNECTED && app->state != CA_CINEMATIC && !app->clc.demoplaying ) {
		CL_AddReliableCommand( app, "disconnect", qtrue );
		CL_WritePacket( app, 2 );
	}

	CL_ClearState( app );
	if ( isFocused ) {
		memset( &cl_loadProgress, 0, sizeof( cl_loadProgress ) );
		CL_ResetLoadingScreenState();
		CL_ClearMapInfo();
		CL_ClearMapPreview();
	}

	// wipe the client connection
	// Tear down client QUIC connection before wiping app->clc
	Com_Log( SEV_INFO, LOG_CH(ch_client), "*** CL_Disconnect: state=%d showMainMenu=%d initialized=%d ***\n",
		(int)app->state, (int)showMainMenu,
		(int)( transport && transport->is_connecting && transport->is_connecting() ) );
	/* Route the disconnect through the backend that owns this client's handle.
	 * In-process-queue: the integrated host stores an in-mem client handle
	 * (100+slot) in clc.quic_conn; a QUIC client stores CONN_CLIENT_QUIC (9).
	 * transport_for_handle picks the matching backend. Fall back to the QUIC
	 * client handle when none is stored yet (never-connected teardown).
	 * Per-app: keys on this app's own handle, so it never disturbs another
	 * app's connection. */
	{
		conn_handle_t disc = app->clc.quic_conn != CONN_INVALID
			? app->clc.quic_conn : CONN_CLIENT_QUIC;
		if ( transport_for_handle( disc )->disconnect )
			transport_for_handle( disc )->disconnect( disc, "client disconnect" );
	}

	Q_SecureZeroMemory( &app->clc.joinAttempt, sizeof( app->clc.joinAttempt ) );
	memset( &app->clc, 0, sizeof( app->clc ) );
	app->clc.wiredRconChallenge[0] = '\0';

	CL_SetState( app, CA_DISCONNECTED );

	if ( isFocused ) {
		// not connected to a pure server anymore
		cl_connectedToPureServer = 0;

		CL_UpdateGUID( NULL, 0 );
	}

	// Remove only this app's cgame commands (owner = its cgame VM handle). By
	// this point the CL_ShutdownCGame above already swept + NULL'd app->cgvm, so
	// this matches nothing (a NULL owner matches no command) — an intentional,
	// safe no-op kept for symmetry. Self-scopes by owner; runs unconditionally.
	Cmd_RemoveCgameCommandsByOwner( app->cgvm );

	if ( isFocused ) {
		if ( noGameRestart )
			noGameRestart = qfalse;
		else
			cl_restarted = CL_RestoreOldGame();
	}

	app->disconnecting = qfalse;

#ifndef HEADLESS
	// Plan C hook: surface any ERR_DROP error as a Wired UI dialog.
	// Four guards prevent reentry and false positives:
	//   showMainMenu  — only when we are going back to the menu, not during
	//                   silent disconnects (map change, demo end, etc.)
	//   cls.uiStarted — UI must be running (early-boot disconnects skip this)
	//   !com_errorEntered — ERR_DROP longjmp is still in progress when this
	//                       is set; calling into UI would crash mid-teardown
	//   com_errorMessage  — nothing to show if there's no error text
	// Host-screen dialog — focused app only.
	if ( isFocused ) {
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

	if ( clientActiveApp->clc.demoplaying || clientActiveApp->state < CA_CONNECTED || cmd[0] == '+' ) {
#if FEAT_WIRED_UI
		/* source-attribution: when a wmenu/whud parse is
		 * underway, the unknown command almost certainly came from a
		 * parser fallthrough (botlib PC consumes a keyword the parser
		 * doesn't recognise, then unmatched data lands here via
		 * Cbuf_ExecuteText). Annotate with file + line + menu so the
		 * cause is one log line away. */
		const char *parseFile = WiredUI_ParseContextFile();
		if ( parseFile ) {
			const char *parseMenu = WiredUI_ParseContextMenu();
			const char *parseItem = WiredUI_ParseContextItem();
			int         parseLine = WiredUI_ParseContextLine();
			Com_Log( SEV_INFO, LOG_CH(ch_client),
				"Unknown command \"%s" S_COLOR_WHITE "\" "
				"(leaked from parsing %s line %d, menu '%s'%s%s%s)\n",
				cmd,
				parseFile, parseLine,
				( parseMenu && parseMenu[0] ) ? parseMenu : "(pre-name)",
				parseItem ? ", item '" : "",
				parseItem ? parseItem : "",
				parseItem ? "'" : "" );
		} else
#endif
		{
			Com_Log( SEV_INFO, LOG_CH(ch_client), "Unknown command \"%s" S_COLOR_WHITE "\"\n", cmd );
		}
		return;
	}

	if ( Cmd_Argc() > 1 ) {
		CL_AddReliableCommand( clientActiveApp, string, qfalse );
	} else {
		CL_AddReliableCommand( clientActiveApp, cmd, qfalse );
	}
}


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
	if ( clientActiveApp->state != CA_ACTIVE || clientActiveApp->clc.demoplaying ) {
		Com_Log( SEV_INFO, LOG_CH(ch_client), "Not connected to a server.\n");
		return;
	}

	if ( Cmd_Argc() <= 1 || strcmp( Cmd_Argv( 1 ), "userinfo" ) == 0 )
		return;

	// don't forward the first argument
	CL_AddReliableCommand( clientActiveApp, Cmd_ArgsFrom( 1 ), qfalse );
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
		Com_Log( SEV_INFO, LOG_CH(ch_client), "Usage: rcon_login <password>\n" );
		return;
	}

	if ( clientActiveApp->clc.serverAddress.type == NA_BAD ) {
		Com_Log( SEV_INFO, LOG_CH(ch_client), "Not connected to a server.\n" );
		return;
	}

	clientActiveApp->clc.wiredRconAuthed = qfalse;
	clientActiveApp->clc.wiredRconHasChallenge = qfalse;
	clientActiveApp->clc.wiredRconChallenge[0] = '\0';
	clientActiveApp->clc.wiredRconAddress = clientActiveApp->clc.serverAddress;

	NET_OutOfBandPrint( NS_CLIENT, &clientActiveApp->clc.serverAddress, "rcon_auth" );
	Com_Log( SEV_INFO, LOG_CH(ch_client), "Wired RCON: requesting challenge...\n" );
}

static void CL_Rcon_f( void ) {
	char cmd[2048];

	if ( Cmd_Argc() < 2 ) {
		Com_Log( SEV_INFO, LOG_CH(ch_client), "Usage: rcon <lua code>\n" );
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

	if ( !clientActiveApp->clc.wiredRconAuthed ) {
		Com_Log( SEV_INFO, LOG_CH(ch_client), "Plaintext rcon disabled. Use rcon_login.\n" );
		return;
	}

	Com_sprintf( cmd, sizeof( cmd ), "rcon %s", Cmd_ArgsFrom( 1 ) );
	NET_OutOfBandPrint( NS_CLIENT, &clientActiveApp->clc.wiredRconAddress, "%s", cmd );
}


/*
==================
CL_Disconnect_f
==================
*/
void CL_Disconnect_f( void ) {
	SCR_StopCinematic();
	if ( clientActiveApp->state != CA_DISCONNECTED && clientActiveApp->state != CA_CINEMATIC ) {
		if ( clientActiveApp->cgvm && clientActiveApp->cgvm->callLevel ) {
			Com_Terminate( TERM_CLIENT_LEAVE, "Disconnected from server" );
		} else {
			// clear any previous "server full" type messages
			clientActiveApp->clc.serverMessage[0] = '\0';
			// Disconnect is CLIENT-ONLY. A running local server is left
			// up (sv_running stays 1) — use 'stopserver' to stop it.
			Com_Log( SEV_INFO, LOG_CH(ch_client), "Disconnected from %s\n", clientActiveApp->servername );
			Com_ClearLastError();
			if ( !CL_Disconnect( clientActiveApp, qfalse ) ) { // restart client if not done already
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
	Cbuf_AddText( va( "connect %s\n", cl_reconnectArgs->string ) );
}


/*
================
CL_SpawnHeadlessApp_f

Spawn a runtime-headless same-process client (an additional app slot) over the
in-memory backend — a real, game-visible, kickable bot/MCP client that skips the
render path and the cgame VM at runtime. (This is a runtime mode of a normal
client, not a -DHEADLESS compile, which cannot coexist with the host in one
process.)

Currently an inert stub: the command is registered and the transport-layer drain
+ pump already iterate every client slot, but the handler does not yet allocate a
slot, prime it without the cgame VM, drive its state, or send READY — so it
spawns nothing and the integrated host (slot 0) remains the only live client.
================
*/
static void CL_SpawnHeadlessApp_f( void ) {
	int          slot;
	clientApp_t *app;
	char         info[MAX_INFO_STRING];
	conn_handle_t handle;

	if ( !com_sv_running || !com_sv_running->integer ) {
		Com_Log( SEV_INFO, LOG_CH(ch_client),
			"spawn_headless_client: requires a running local server.\n" );
		return;
	}

	/* Find a free additional slot (slot 0 is the integrated host). */
	for ( slot = 1; slot < MAX_LOCAL_CGAME_VMS; slot++ ) {
		if ( clientApps[slot].state <= CA_DISCONNECTED )
			break;
	}
	if ( slot >= MAX_LOCAL_CGAME_VMS ) {
		Com_Log( SEV_INFO, LOG_CH(ch_client),
			"spawn_headless_client: no free client slot.\n" );
		return;
	}
	app = &clientApps[slot];

	/* Minimal userinfo for a non-rendering bot/MCP client. */
	info[0] = '\0';
	Info_SetValueForKey_s( info, MAX_INFO_STRING, "name", "headless" );
	Info_SetValueForKey_s( info, MAX_INFO_STRING, "rate", "25000" );
	Info_SetValueForKey_s( info, MAX_INFO_STRING, "snaps", "20" );

	/* Connect over the in-memory backend (no picoquic, no UDP). Returns the
	 * client-end handle; the server admits this as a normal kickable client in a
	 * slot>0 (it is not the un-kickable host). */
	handle = WN_ConnectApp( slot, info );
	if ( handle == CONN_INVALID ) {
		Com_Log( SEV_WARN, LOG_CH(ch_client),
			"spawn_headless_client: WN_ConnectApp(%d) failed.\n", slot );
		return;
	}

	memset( &app->clc, 0, sizeof( app->clc ) );
	memset( &app->cl, 0, sizeof( app->cl ) );
	app->clc.quic_conn = handle;
	app->connectionGeneration = CL_NextConnectionGeneration();
	app->cgvm = NULL;                 /* runtime-headless: no cgame VM */
	Q_strncpyz( app->servername, "localhost", sizeof( app->servername ) );
	CL_SetState( app, CA_CONNECTING );

	Com_Log( SEV_INFO, LOG_CH(ch_client),
		"spawn_headless_client: client slot %d connecting (handle %llu); gamestate "
		"ingest + prime + ready run from the per-frame drive.\n",
		slot, (unsigned long long)handle );
}


/*
================
CL_NormalizeServerAddress

Validates one console-safe server-address token, resolves it once, and returns
the canonical numeric address consumed by every UI/console connect path.  The
lexical gate runs before NET_StringToAdr so command separators, quoting,
whitespace, control bytes, and malformed ports never reach either DNS or the
command buffer.
================
*/
qboolean CL_NormalizeServerAddress( const char *input, netadrtype_t family,
	char *normalized, int normalizedSize, netadr_t *address ) {
	char token[sizeof( clientActiveApp->servername )];
	const char *start;
	const char *end;
	const char *port = NULL;
	int colonCount = 0;
	int length;
	netadr_t resolved;

	if ( !input || !normalized || normalizedSize < 1 ) return qfalse;
	normalized[0] = '\0';

	start = input;
	while ( *start == ' ' || *start == '\t' ) start++;
	end = start + strlen( start );
	while ( end > start && ( end[-1] == ' ' || end[-1] == '\t' ) ) end--;
	length = (int)( end - start );
	if ( length <= 0 || length >= (int)sizeof( token ) ) return qfalse;

	for ( int i = 0; i < length; i++ ) {
		const unsigned char ch = (unsigned char)start[i];
		if ( ch >= 'A' && ch <= 'Z' ) continue;
		if ( ch >= 'a' && ch <= 'z' ) continue;
		if ( ch >= '0' && ch <= '9' ) continue;
		if ( ch == '.' || ch == '_' || ch == '-' || ch == ':' ||
		     ch == '[' || ch == ']' || ch == '%' ) continue;
		return qfalse;
	}

	memcpy( token, start, (size_t)length );
	token[length] = '\0';
	for ( int i = 0; i < length; i++ ) {
		if ( token[i] == ':' ) colonCount++;
	}

	if ( token[0] == '[' ) {
		char *close = strchr( token + 1, ']' );
		if ( !close || close == token + 1 || strchr( close + 1, ']' ) ||
		     strchr( token + 1, '[' ) ) return qfalse;
		if ( close[1] ) {
			if ( close[1] != ':' || !close[2] ) return qfalse;
			port = close + 2;
		}
	} else {
		if ( strchr( token, '[' ) || strchr( token, ']' ) ) return qfalse;
		if ( colonCount == 1 ) {
			char *separator = strchr( token, ':' );
			if ( separator == token || !separator[1] ) return qfalse;
			port = separator + 1;
		}
	}

	if ( port ) {
		char *parseEnd = NULL;
		long value;
		for ( const char *p = port; *p; p++ ) {
			if ( *p < '0' || *p > '9' ) return qfalse;
		}
		value = strtol( port, &parseEnd, 10 );
		if ( !parseEnd || *parseEnd || value < 1 || value > 65535 ) return qfalse;
	}

	if ( !NET_StringToAdr( token, &resolved, family ) || resolved.type == NA_BAD ) {
		return qfalse;
	}
	if ( resolved.port == 0 ) resolved.port = BigShort( PORT_SERVER );

	if ( resolved.type == NA_LOOPBACK ) {
		Q_strncpyz( normalized, "localhost", normalizedSize );
	} else {
		Q_strncpyz( normalized, NET_AdrToStringwPort( &resolved ), normalizedSize );
	}
	if ( !normalized[0] ) return qfalse;
	if ( address ) *address = resolved;
	return qtrue;
}


/* Begin a connection from already-normalized, already-resolved authority.
 * Browser callers enter through CL_ConnectBrowserServer; console callers are
 * normalized by CL_Connect_f below. */
static qboolean CL_BeginResolvedConnect( const char *server, const netadr_t *resolved,
	qboolean browserOrigin, const char *joinPassword, int selectionGeneration ) {
	netadr_t addr;
	// save arguments for reconnect
	char args[ sizeof( clientActiveApp->servername ) + MAX_CVAR_VALUE_STRING ];

	if ( !server || !server[0] || !resolved || resolved->type == NA_BAD ) {
		return qfalse;
	}
	addr = *resolved;
	Q_strncpyz( args, server, sizeof( args ) );

	// if running a local server, kill it
	if ( com_sv_running->integer && addr.type == NA_LOOPBACK ) {
		SV_Shutdown( "Server quit" );
	}

	// make sure a local server is killed
	Cvar_Set( "sv_killserver", "1" );
	SV_Frame( 0 );

	noGameRestart = qtrue;
	CL_Disconnect( clientActiveApp, qtrue );
	// (Console close is delegated to the CA_CONNECTING transition below via
	//  CL_OnClientStateChanged — \connect is a connection, not a UI action. This
	//  also fixes the old asymmetry where this bare Con_Close ignored attract.)

	Q_strncpyz( clientActiveApp->servername, server, sizeof( clientActiveApp->servername ) );
	clientActiveApp->clc.serverMessage[0] = '\0';
	if ( browserOrigin ) {
		clientActiveApp->clc.joinAttempt.browserOrigin = qtrue;
		clientActiveApp->clc.joinAttempt.selectionGeneration = selectionGeneration;
		Q_strncpyz( clientActiveApp->clc.joinAttempt.target, server,
			sizeof( clientActiveApp->clc.joinAttempt.target ) );
		if ( joinPassword && joinPassword[0] ) {
			Q_strncpyz( clientActiveApp->clc.joinAttempt.joinPassword, joinPassword,
				sizeof( clientActiveApp->clc.joinAttempt.joinPassword ) );
			clientActiveApp->clc.joinAttempt.credentialPending = qtrue;
		}
		Com_Log( SEV_DEBUG, LOG_CH(ch_client),
			"Browser connect attempt armed target=%s selection_generation=%d credential_present=%d\n",
			server, selectionGeneration,
			clientActiveApp->clc.joinAttempt.credentialPending ? 1 : 0 );
	}

	// copy resolved address
	clientActiveApp->clc.serverAddress = addr;

	if (clientActiveApp->clc.serverAddress.port == 0) {
		clientActiveApp->clc.serverAddress.port = BigShort( PORT_SERVER );
	}

	const char *serverString = NET_AdrToStringwPort( &clientActiveApp->clc.serverAddress );

	Com_Log( SEV_INFO, LOG_CH(ch_client), "%s resolved to %s\n", clientActiveApp->servername, serverString );

	if ( cl_guidServerUniq->integer )
		CL_UpdateGUID( serverString, strlen( serverString ) );
	else
		CL_UpdateGUID( NULL, 0 );

	// QUIC handles auth via TLS; LAN no longer needs the UDP challenge round-trip.
	CL_SetState( clientActiveApp, CA_CONNECTING );
	WiredUI_SetLoadingMenu( "ui/connect.wui" );  // state→named-UI: show connect
	Com_RandomBytes( (byte*)&clientActiveApp->clc.challenge, sizeof( clientActiveApp->clc.challenge ) );

	Key_SetCatcher( 0 );
	clientActiveApp->clc.connectTime = cls.realtime - RECONNECT_TIMEOUT; // CL_CheckForResend() will fire immediately
	clientActiveApp->clc.connectPacketCount = 0;

	Cvar_Set( "cl_reconnectArgs", args );

	// server connection string
	Cvar_Set( "cl_currentServerAddress", server );
	return qtrue;
}

qboolean CL_ConnectBrowserServer( const char *target, const netadr_t *address,
	const char *joinPassword, int selectionGeneration ) {
	char secret[33];
	char normalized[sizeof( clientActiveApp->servername )];
	netadr_t resolved;
	qboolean result;

	memset( secret, 0, sizeof( secret ) );
	if ( joinPassword && joinPassword[0] ) {
		if ( strlen( joinPassword ) > 32 || !Info_ValidateKeyValue( joinPassword ) ) {
			Q_SecureZeroMemory( secret, sizeof( secret ) );
			return qfalse;
		}
		Q_strncpyz( secret, joinPassword, sizeof( secret ) );
	}
	/* Treat the typed address as corroborating evidence, not authority. Resolve
	 * the canonical target again at the CL boundary and require an exact
	 * address+port match before copying anything into connection state. */
	if ( !address || !CL_NormalizeServerAddress( target, NA_UNSPEC, normalized,
		sizeof( normalized ), &resolved ) || !NET_CompareAdr( &resolved, address ) ) {
		Com_Log( SEV_WARN, LOG_CH(ch_client),
			"Browser connect refused stage=target-revalidation address_match=0\n" );
		Q_SecureZeroMemory( secret, sizeof( secret ) );
		return qfalse;
	}
	result = CL_BeginResolvedConnect( normalized, &resolved, qtrue, secret,
		selectionGeneration );
	Q_SecureZeroMemory( secret, sizeof( secret ) );
	return result;
}

/*
================
CL_Connect_f
================
*/
static void CL_Connect_f( void ) {
	int argc = Cmd_Argc();
	netadrtype_t family = NA_UNSPEC;
	const char *server;
	char buffer[ sizeof( clientActiveApp->servername ) ];
	char normalized[sizeof( clientActiveApp->servername )];
	netadr_t addr;
	int len;

	if ( argc != 2 && argc != 3 ) {
		Com_Log( SEV_INFO, LOG_CH(ch_client), "usage: connect [-4|-6] <server>\n");
		return;
	}
	if ( argc == 2 ) {
		server = Cmd_Argv(1);
	} else {
		if ( !strcmp( Cmd_Argv(1), "-4" ) ) {
			family = NA_IP;
#if FEAT_IPV6
		} else if ( !strcmp( Cmd_Argv(1), "-6" ) ) {
			family = NA_IP6;
#endif
		} else {
			COM_WARN( LOG_CH(ch_client), "warning: only -4 or -6 as address type understood.\n" );
			return;
		}
		server = Cmd_Argv(2);
	}

	Q_strncpyz( buffer, server, sizeof( buffer ) );
	len = (int)strlen( buffer );
	if ( len <= 0 ) return;
	if ( buffer[len - 1] == '/' ) buffer[len - 1] = '\0';
	server = buffer;
	if ( !Q_stricmpn( server, "q3a:/", 5 ) ) server += 5;
	while ( *server == '/' ) server++;
	if ( !server[0] ) return;
	if ( !CL_NormalizeServerAddress( server, family, normalized,
		sizeof( normalized ), &addr ) ) {
		COM_WARN( LOG_CH(ch_client), "Bad server address\n" );
		return;
	}
	CL_BeginResolvedConnect( normalized, &addr, qfalse, NULL, 0 );
}


/*
=================
CL_SendPureChecksums
=================
*/
static void CL_SendPureChecksums( void ) {
	char cMsg[ MAX_STRING_CHARS-1 ];

	if ( !cl_connectedToPureServer || clientActiveApp->clc.demoplaying )
		return;

	// if we are pure we need to send back a command with our referenced pk3 checksums
	int len = sprintf( cMsg, "cp %d ", clientActiveApp->cl.serverId );
	strcpy( cMsg + len, FS_ReferencedPakPureChecksums( sizeof( cMsg ) - len - 1 ) );

	CL_AddReliableCommand( clientActiveApp, cMsg, qfalse );
}


/*
=================
CL_ResetPureClientAtServer
=================
*/
static void CL_ResetPureClientAtServer( void ) {
	CL_AddReliableCommand( clientActiveApp, "vdr", qfalse );
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

	if ( clientActiveApp->clc.demorecording )
		CL_StopRecord_f();

	// clear and mute all sounds until next registration
	S_DisableSounds();

	// Teardown window: from here through CL_ShutdownRef the renderer + cgame are
	// mid-tear-down (VMs shut down, the WiredUI compositor deregistered, the
	// renderer about to release GPU resources). Do NOT pump a render frame in this
	// window — SCR_UpdateScreen would reference half-freed render state.
	// shutdown VMs
	CL_ShutdownVMs();

#if FEAT_WIRED_UI
	WiredUI_CompositorUnregisterDevCommands();
	WiredUI_Shutdown();
#endif

	// shutdown the renderer and clear the renderer interface
	CL_ShutdownRef( shutdownCode ); // REF_LEVEL_ONLY, REF_KEEP_WINDOW, REF_DESTROY_WINDOW

	// client is no longer pure until new checksums are sent
	CL_ResetPureClientAtServer();

	// clear pak references
	FS_ClearPakReferences( FS_UI_REF | FS_CGAME_REF );

	// reinitialize the filesystem if the game directory or checksum has changed
	if ( !clientActiveApp->clc.demoplaying ) // -EC-
		FS_ConditionalRestart( clientActiveApp->clc.checksumFeed, qfalse );

	cls.soundRegistered = qfalse;

	// unpause so the cgame definitely gets a snapshot and renders a frame
	Cvar_Set( "cl_paused", "0" );

	CL_ClearMemory();

	// startup all the client stuff
	CL_StartHunkUsers();

	// start the cgame if connected
	if ( ( clientActiveApp->state > CA_CONNECTED && clientActiveApp->state != CA_CINEMATIC ) || clientActiveApp->startCgame ) {
		clientActiveApp->cgameStarted = qtrue;
		CL_InitCGame( clientActiveApp );
		// send pure checksums
		CL_SendPureChecksums();
	}

	clientActiveApp->startCgame = qfalse;
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

	// Sound will be reinitialized by vid_restart.  REF_KEEP_WINDOW
	// destroys the device + recreates it on the !vk.active fallback;
	// the renderer doesn't need full map-scoped preservation for a
	// snd_restart (sound is independent of renderer context).
	CL_Vid_Restart( REF_KEEP_WINDOW );
}


/*
==================
CL_PakList_f
==================
*/
void CL_OpenedPakList_f( void ) {
	Com_Log( SEV_INFO, LOG_CH(ch_client), "Opened Pak Names: %s\n", FS_LoadedPakNames());
}


/*
==================
CL_PureList_f
==================
*/
static void CL_ReferencedPakList_f( void ) {
	Com_Log( SEV_INFO, LOG_CH(ch_client), "Referenced Pak Names: %s\n", FS_ReferencedPakNames() );
}


/*
==================
CL_Configstrings_f
==================
*/
static void CL_Configstrings_f( void ) {
	if ( clientActiveApp->state != CA_ACTIVE ) {
		Com_Log( SEV_INFO, LOG_CH(ch_client), "Not connected to a server.\n");
		return;
	}

	for ( int i = 0 ; i < MAX_CONFIGSTRINGS ; i++ ) {
		int ofs = clientActiveApp->cl.gameState.stringOffsets[ i ];
		if ( !ofs ) {
			continue;
		}
		Com_Log( SEV_INFO, LOG_CH(ch_client), "%4i: %s\n", i, clientActiveApp->cl.gameState.stringData + ofs );
	}
}


/*
==============
CL_Clientinfo_f
==============
*/
static void CL_Clientinfo_f( void ) {
	Com_Log( SEV_INFO, LOG_CH(ch_client), "--------- Client Information ---------\n" );
	Com_Log( SEV_INFO, LOG_CH(ch_client), "state: %i\n", clientActiveApp->state );
	Com_Log( SEV_INFO, LOG_CH(ch_client), "Server: %s\n", clientActiveApp->servername );
	Com_Log( SEV_INFO, LOG_CH(ch_client), "User info settings:\n");
	Info_Print( Cvar_InfoString( CVAR_USERINFO, NULL ) );
	Com_Log( SEV_INFO, LOG_CH(ch_client), "--------------------------------------\n" );
}


/*
==============
CL_Serverinfo_f
==============
*/
static void CL_Serverinfo_f( void ) {
	int ofs = clientActiveApp->cl.gameState.stringOffsets[ CS_SERVERINFO ];
	if ( !ofs )
		return;

	Com_Log( SEV_INFO, LOG_CH(ch_client), "Server info settings:\n" );
	Info_Print( clientActiveApp->cl.gameState.stringData + ofs );
}


/*
===========
CL_Systeminfo_f
===========
*/
static void CL_Systeminfo_f( void ) {
	int ofs = clientActiveApp->cl.gameState.stringOffsets[ CS_SYSTEMINFO ];
	if ( !ofs )
		return;

	Com_Log( SEV_INFO, LOG_CH(ch_client), "System info settings:\n" );
	Info_Print( clientActiveApp->cl.gameState.stringData + ofs );
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

Called when all downloading has been completed.

chunking: the synchronous downloadRestart early-return paths
remain here (they exit without starting the load).  The actual load —
CL_FlushMemory + CL_InitCGame + finalize — runs one phase per Com_Frame
tick out of CL_DownloadsComplete_Tick below, mirroring SV_SpawnServer_Tick
(sv_init.c).  This keeps the console + loading screen responsive between
phases.  The cgame-VM-internal CG_INIT asset loop is still a single
synchronous hitch inside its own phase — the VM cooperative-yield
contract is the architectural follow-up.
=================
*/
static void CL_DownloadsComplete( void ) {


	// if we downloaded files we need to restart the file system
	if ( clientActiveApp->clc.downloadRestart ) {
		clientActiveApp->clc.downloadRestart = qfalse;

		FS_Restart(clientActiveApp->clc.checksumFeed); // We possibly downloaded a pak, restart the file system to load it

		// inform the server so we get new gamestate info
		CL_AddReliableCommand( clientActiveApp, "donedl", qfalse );

		// by sending the donedl command we request a new gamestate
		// so we don't want to load stuff yet
		return;
	}

	// let the client game init and load data
	Com_Log( SEV_INFO, LOG_CH(ch_client),
		"cls.state: -> CA_LOADING (CL_DownloadsComplete: server=%s)\n",
		clientActiveApp->servername[0] ? clientActiveApp->servername : "(local)" );
	CL_SetState( clientActiveApp, CA_LOADING );
	WiredUI_SetLoadingMenu( "ui/loading_screen.wui" );  // state→named-UI: map-load backdrop

	/* Kick off the async load state machine.  Actual work happens
	 * one phase per Com_Frame tick in CL_DownloadsComplete_Tick. */
	clientActiveApp->dlcomplete.phase = DLC_P1_EVENTLOOP;
}


/*
=================
CL_DownloadsComplete_Tick

Drive one phase of the async CL_DownloadsComplete state machine.  Called
from Com_Frame once per tick, before CL_Frame, so the loading screen and
console drawing happen between phases.  Mirrors SV_SpawnServer_Tick.

Phase order:
  P1  Event-loop pump        (Com_EventLoop; abort if state changed)
  P2  Flush client memory    (CL_FlushMemory: CL_ShutdownAll + ClearMemory + StartHunkUsers)
  P3  CGame init             (CL_InitCGame — single bounded VM hitch)
  P4  Finalize               (callvote command, pure checksums, ready, packet)
=================
*/
void CL_DownloadsComplete_Tick( void ) {
	switch ( clientActiveApp->dlcomplete.phase ) {

	case DLC_IDLE:
		return;

	/* ---- Phase 1: Pump the event loop ------------------------------------ */
	case DLC_P1_EVENTLOOP:
		// Pump the loop, this may change gamestate!
		Com_EventLoop();

		// if the gamestate was changed by calling Com_EventLoop
		// then we loaded everything already and we don't want to do it again.
		if ( clientActiveApp->state != CA_LOADING ) {
			clientActiveApp->dlcomplete.phase = DLC_IDLE;
			return;
		}

		clientActiveApp->dlcomplete.phase = DLC_P2_FLUSH_MEMORY;
		return;

	/* ---- Phase 2: Flush memory + CGame init (atomic pair) ---------------- */
	//
	// CL_FlushMemory clears the client hunk, shuts down VMs, and restarts
	// hunk users into a partially-initialised state — the renderer is alive
	// again but the cgame VM is gone.  If we yield to Com_Frame in that
	// window, CL_Frame may try to render a loading screen whose dependencies
	// are mid-rebuild.  Pair them as one atomic phase so CL_InitCGame
	// restores a consistent state before Com_Frame loops back.  (A
	// Com_sprintf buffer overflow once seen in this window was a separate
	// log-sink bug, fixed by sizing the JSON header to
	// LOG_JSON_HEADER_SIZE — the atomic-pair rationale here is architectural
	// and independent of it.)  The VM-internal CG_INIT asset loop is still
	// the architectural hitch — the VM cooperative-yield contract is the
	// follow-up.
	case DLC_P2_FLUSH_MEMORY:
	case DLC_P3_INIT_CGAME:
		CL_FlushMemory();
		clientActiveApp->cgameStarted = qtrue;
		CL_InitCGame( clientActiveApp );

		clientActiveApp->dlcomplete.phase = DLC_P4_FINALIZE;
		return;

	/* ---- Phase 4: Finalize ----------------------------------------------- */
	case DLC_P4_FINALIZE:
		if ( clientActiveApp->clc.demofile == FS_INVALID_HANDLE ) {
			Cmd_AddCommand( "callvote", NULL );
			Cmd_SetCommandCompletionFunc( "callvote", CL_CompleteCallvote );
		}

		// set pure checksums
		CL_SendPureChecksums();
		WN_ClientSendReady( (int)( clientActiveApp - clientApps ) );

		CL_WritePacket( clientActiveApp, 2 );

		clientActiveApp->dlcomplete.phase = DLC_IDLE;
		return;

	default:
		clientActiveApp->dlcomplete.phase = DLC_IDLE;
		return;
	}
}


/*
=================
CL_BeginDownload

Requests a file to download from the server.  Stores it in the current
game directory.
=================
*/
static void CL_BeginDownload( const char *localName, const char *remoteName ) {

	Com_Log( SEV_DEBUG, LOG_CH(ch_client), "***** CL_BeginDownload *****\n"
				"Localname: %s\n"
				"Remotename: %s\n"
				"****************************\n", localName, remoteName);

	Q_strncpyz ( clientActiveApp->clc.downloadName, localName, sizeof(clientActiveApp->clc.downloadName) );
	Com_sprintf( clientActiveApp->clc.downloadTempName, sizeof(clientActiveApp->clc.downloadTempName), "%s.tmp", localName );

	// Set so UI gets access to it
	Cvar_Set( "cl_downloadName", remoteName );
	Cvar_Set( "cl_downloadSize", "0" );
	Cvar_Set( "cl_downloadCount", "0" );
	Cvar_SetIntegerValue( "cl_downloadTime", cls.realtime );

	clientActiveApp->clc.downloadBlock = 0; // Starting new file
	clientActiveApp->clc.downloadCount = 0;

	CL_AddReliableCommand( clientActiveApp, va("download %s", remoteName), qfalse );
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
	if(*clientActiveApp->clc.downloadName)
	{
		const char *zippath = FS_BuildOSPath(Cvar_VariableString("fs_homepath"), clientActiveApp->clc.downloadName, NULL );

		if(!FS_CompareZipChecksum(zippath))
			Com_Terminate( TERM_CLIENT_DROP, "Incorrect checksum for file: %s", clientActiveApp->clc.downloadName);
	}

	*clientActiveApp->clc.downloadTempName = *clientActiveApp->clc.downloadName = '\0';
	Cvar_Set("cl_downloadName", "");

	// We are looking to start a download here
	if (*clientActiveApp->clc.downloadList) {
		char *s = clientActiveApp->clc.downloadList;
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


		if( !useCURL ) {
			if( (cl_allowDownload->integer & DLF_NO_UDP) ) {
				Com_Terminate( TERM_CLIENT_DROP, "UDP Downloads are "
					"disabled on your client. "
					"(cl_allowDownload is %d)",
					cl_allowDownload->integer);
				return;
			}
			CL_BeginDownload( localName, remoteName );
		}
		clientActiveApp->clc.downloadRestart = qtrue;

		// move over the rest
		memmove( clientActiveApp->clc.downloadList, s, strlen(s) + 1 );

		return;
	}

	CL_DownloadsComplete();
}


/*
=================
CL_SetupQuicNetchan

The QUIC migration removed the UDP netchan handshake, but the client still uses the
trimmed netchan state for packet pacing, packet-history bookkeeping, and to
select the QUIC send path in CL_WritePacket. Seed the same fields that the old
Netchan_Setup path used to initialize.

Note: address type is left as-is (NA_IP, NA_IP6, NA_LOOPBACK).  NA_QUIC /
NA_QUIC6 are transport-internal types; use (clientActiveApp->clc.quic_conn != CONN_INVALID) to
test whether we are on a QUIC connection.
=================
*/
static void CL_SetupQuicNetchan( void )
{
	clientActiveApp->clc.netchan.remoteAddress = clientActiveApp->clc.serverAddress;
	clientActiveApp->clc.netchan.incomingSequence = 0;
	clientActiveApp->clc.netchan.outgoingSequence = 1;
	clientActiveApp->clc.netchan.isLANAddress = Sys_IsLANAddress( &clientActiveApp->clc.netchan.remoteAddress );
}


/*
=================
CL_InitDownloads

After receiving a valid game state, we valid the cgame and local zip files here
and determine if we need to download them
=================
*/
void CL_InitDownloads( clientApp_t *app ) {

	/* An additional in-process client shares this process's filesystem with the
	 * integrated host — every pak the server references is already loaded. It
	 * downloads nothing and does not run the cgame-loading flow (CL_DownloadsComplete
	 * → CL_InitCGame → a cgame VM, which a runtime-headless client has no use for).
	 * It just advances to CA_CONNECTED; the headless prime path takes it from
	 * there to CA_PRIMED without a VM. */
	if ( app != clientActiveApp ) {
		CL_SetState( app, CA_CONNECTED );
		return;
	}

	if ( !(cl_allowDownload->integer & DLF_ENABLE) )
	{
		char missingfiles[ MAXPRINTMSG ];

		// autodownload is disabled on the client
		// but it's possible that some referenced files on the server are missing
		if ( FS_ComparePaks( missingfiles, sizeof( missingfiles ), qfalse ) )
		{
			// NOTE TTimo I would rather have that printed as a modal message box
			// but at this point while joining the game we don't know whether we will successfully join or not
			Com_Log( SEV_INFO, LOG_CH(ch_client), "\nWARNING: You are missing some files referenced by the server:\n%s"
				"You might not be able to join the game\n"
				"Go to the setting menu to turn on autodownload, or get the file elsewhere\n\n", missingfiles );
		}
	}
	else if ( FS_ComparePaks( app->clc.downloadList, sizeof( app->clc.downloadList ) , qtrue ) ) {

		Com_Log( SEV_INFO, LOG_CH(ch_client), "Need paks: %s\n", app->clc.downloadList );

		if ( *app->clc.downloadList ) {
			// if autodownloading is not enabled on the server
			CL_SetState( app, CA_CONNECTED );

			*app->clc.downloadTempName = *app->clc.downloadName = '\0';
			Cvar_Set( "cl_downloadName", "" );

			CL_NextDownload();
			return;
		}

	}


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
	if ( clientActiveApp->clc.demoplaying ) {
		return;
	}

	// resend if we haven't gotten a reply yet
	if ( clientActiveApp->state != CA_CONNECTING ) {
		return;
	}

	if ( cls.realtime - clientActiveApp->clc.connectTime < RECONNECT_TIMEOUT ) {
		return;
	}

	clientActiveApp->clc.connectTime = cls.realtime;	// for retransmit requests
	clientActiveApp->clc.connectPacketCount++;

	switch ( clientActiveApp->state ) {
	case CA_CONNECTING:
		// QUIC path: skip the UDP challenge round-trip.
		// Build userinfo with challenge included (so server echoes it back in connectResponse),
		// then initiate QUIC handshake via transport->connect() if not already started.
		// "loopback" address string is handled inside wn_connect → WN_ClientConnect,
		// which maps it to 127.0.0.1 so picoquic can send real UDP datagrams.
		if ( !( transport && transport->is_connecting && transport->is_connecting() ) ) {
			char   info[MAX_INFO_STRING * 2];
			qboolean truncated = qfalse;
			qboolean infoValid;
			qboolean handedOff = qfalse;
			int qport = Cvar_VariableIntegerValue( "net_qport" );
			Q_strncpyz( info, Cvar_InfoString( CVAR_USERINFO, &truncated ), sizeof( info ) );
			infoValid = !truncated && strlen( info ) < MAX_USERINFO_LENGTH;

			/* Browser authentication is target-scoped.  Never inherit the
			 * process-global legacy password into a browser attempt; inject the
			 * one-shot credential under its distinct wire key instead. */
			if ( clientActiveApp->clc.joinAttempt.browserOrigin ) {
				Info_RemoveKey( info, "password" );
				Info_RemoveKey( info, "join_password" );
				Info_RemoveKey( info, "join_auth" );
				if ( infoValid && clientActiveApp->clc.joinAttempt.credentialPending ) {
					infoValid = Info_SetValueForKey_s( info, MAX_USERINFO_LENGTH,
						"join_password", clientActiveApp->clc.joinAttempt.joinPassword );
				}
			}

			// Embed client challenge so server echoes it back in connectResponse.
			if ( infoValid )
				infoValid = Info_SetValueForKey_s( info, MAX_USERINFO_LENGTH,
					"challenge", va( "%i", clientActiveApp->clc.challenge ) );
			if ( infoValid )
				infoValid = Info_SetValueForKey_s( info, MAX_USERINFO_LENGTH,
					"protocol", com_protocol->string );
			if ( infoValid )
				infoValid = Info_SetValueForKey_s( info, MAX_USERINFO_LENGTH,
					"qport", va( "%i", qport ) );
			if ( !infoValid ) {
				static const char productError[] =
					"Unable to connect because the client settings are too large.";
				Com_Log( SEV_WARN, LOG_CH(ch_client),
					"Connect userinfo rejected stage=build truncated=%d browser_origin=%d\n",
					truncated ? 1 : 0,
					clientActiveApp->clc.joinAttempt.browserOrigin ? 1 : 0 );
				Q_SecureZeroMemory( info, sizeof( info ) );
				Q_SecureZeroMemory( clientActiveApp->clc.joinAttempt.joinPassword,
					sizeof( clientActiveApp->clc.joinAttempt.joinPassword ) );
				clientActiveApp->clc.joinAttempt.credentialPending = qfalse;
				Com_SetLastError( "%s", productError );
				CL_Disconnect( clientActiveApp, qfalse );
				if ( cls.uiStarted ) {
					UI_CALL_SET_ACTIVE( UIMENU_MAIN );
					CL_WiredUI_ShowError( "Connection Failed", productError, qtrue );
				} else {
					Q_strncpyz( CL_ActiveApp()->pendingConnectError, productError,
						sizeof( CL_ActiveApp()->pendingConnectError ) );
					CL_FlushMemory();
				}
				return;
			}

			CL_SetupQuicNetchan();
			/* In-process-queue (B2+B3): the integrated host (this client + the
			 * same-process listen server, servername "localhost") connects over
			 * the in-memory backend — a buffer-pass over per-app rings, no
			 * loopback-QUIC. A remote connect stays on QUIC. The returned handle
			 * encodes the backend: 100+ → in-mem (transport_for_handle), else
			 * QUIC. The client-side recv pump (global `transport`) reads
			 * wtcl_array[0] either way, so no recv-side branch is needed. */
			{
				/* The integrated host (listen server) is the ONLY connection that
				 * may use the in-memory backend. The deciding factor is whether an
				 * in-process server is actually being hosted in THIS process — NOT
				 * the servername string. Routing on servername=="localhost" alone is
				 * wrong: a `connect localhost` to a SEPARATE process (a standalone
				 * wired-headless on the same box) also carries servername "localhost"
				 * but has no in-process server to answer the in-mem rings, so it would
				 * hang forever at CA_CONNECTING. That external connect must take the
				 * network (loopback QUIC) path instead.
				 *
				 * com_sv_running alone is NOT sufficient: the integrated host enters
				 * CA_CONNECTING and issues this connect during CL_MapLoading, which
				 * runs inside SV_SpawnServer_Tick's SPAWN_P1 phase BEFORE SV_Startup
				 * sets sv_running=1 (verified: sv_running=0 at this point). So also
				 * accept an in-flight spawn (!SV_IsSpawnIdle()). For an external
				 * `connect localhost` CL_Connect_f has already killed any local server
				 * (sv_killserver), leaving sv_running=0 AND the spawn idle, so neither
				 * condition holds and the connect correctly routes over the network.
				 * The in-mem server end is admitted later by WN_DrainPendingConnects
				 * once the spawn reaches SPAWN_IDLE, so the early in-mem connect is
				 * simply queued — order-independent. */
				qboolean inProcessServer = ( com_sv_running && com_sv_running->integer ) || !SV_IsSpawnIdle();
				qboolean useInmem = inProcessServer && !Q_stricmp( clientActiveApp->servername, "localhost" );
				transport_t *connTransport = useInmem ? &inmem_transport : transport;
				if ( connTransport ) {
					clientActiveApp->clc.quic_conn = connTransport->connect(
						NET_AdrToString( &clientActiveApp->clc.serverAddress ),
						(int)BigShort( clientActiveApp->clc.serverAddress.port ),
						info );
					if ( clientActiveApp->clc.quic_conn != CONN_INVALID ) {
						clientActiveApp->connectionGeneration = CL_NextConnectionGeneration();
					}
					handedOff = qtrue;
				}
			}
			if ( handedOff && clientActiveApp->clc.joinAttempt.browserOrigin ) {
				Com_Log( SEV_DEBUG, LOG_CH(ch_client),
					"Browser connect credential disposed target=%s stage=client-handoff\n",
					clientActiveApp->clc.joinAttempt.target );
				Q_SecureZeroMemory( clientActiveApp->clc.joinAttempt.joinPassword,
					sizeof( clientActiveApp->clc.joinAttempt.joinPassword ) );
				clientActiveApp->clc.joinAttempt.credentialPending = qfalse;
			}
			Q_SecureZeroMemory( info, sizeof( info ) );
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

static qboolean CL_MasterResponseCommandMatches( const msg_t *msg, qboolean extended ) {
	const char *command = extended ? "getserversExtResponse" : "getserversResponse";
	const size_t commandLength = strlen( command );
	const byte *cursor;
	const byte *end;

	if ( !msg || msg->cursize < 4 || (size_t)( msg->cursize - 4 ) <= commandLength ) {
		return qfalse;
	}
	cursor = msg->data + 4;
	end = msg->data + msg->cursize;
	if ( memcmp( cursor, command, commandLength ) != 0 ) return qfalse;
	cursor += commandLength;
	if ( cursor >= end ) return qfalse;
	return *cursor == '\\' || ( extended && *cursor == '/' );
}

static int CL_MasterResponseSource( const netadr_t *from, qboolean extended ) {
	for ( int i = 0; i < cl_masterDiscovery.sourceCount; i++ ) {
		if ( cl_masterDiscovery.sources[i].extended == extended
		  && NET_CompareAdr( from, &cl_masterDiscovery.sources[i].address ) ) {
			return i;
		}
	}
	return -1;
}

static qboolean CL_ParseMasterResponseAddresses( const netadr_t *from,
	const msg_t *msg, qboolean extended, netadr_t *addresses, int *addressCount ) {
	const char *command = extended ? "getserversExtResponse" : "getserversResponse";
	const byte *cursor = msg->data + 4 + strlen( command );
	const byte *end = msg->data + msg->cursize;
	int count = 0;

	memset( addresses, 0, sizeof( *addresses ) * MAX_SERVERSPERPACKET );
	while ( cursor < end ) {
		byte separator = *cursor;
		int addressBytes;

		if ( end - cursor >= 4 && ( separator == '\\' || ( extended && separator == '/' ) )
		  && cursor[1] == 'E' && cursor[2] == 'O' && cursor[3] == 'T' ) {
			cursor += 4;
			while ( cursor < end && ( *cursor == '\0' || *cursor == '\r' || *cursor == '\n' ) ) cursor++;
			if ( cursor != end ) return qfalse;
			*addressCount = count;
			return qtrue;
		}
		if ( count >= MAX_SERVERSPERPACKET ) return qfalse;
		if ( separator == '\\' ) {
			addresses[count].type = NA_IP;
			addressBytes = (int)sizeof( addresses[count].ipv._4 );
		} else if ( extended && separator == '/' ) {
#if FEAT_IPV6
			addresses[count].type = NA_IP6;
			addresses[count].scope_id = from->scope_id;
			addressBytes = (int)sizeof( addresses[count].ipv._6 );
#else
			return qfalse;
#endif
		} else {
			return qfalse;
		}
		cursor++;
		if ( end - cursor < addressBytes + 2 ) return qfalse;
		if ( addresses[count].type == NA_IP ) {
			memcpy( addresses[count].ipv._4, cursor, addressBytes );
		} else {
#if FEAT_IPV6
			memcpy( addresses[count].ipv._6, cursor, addressBytes );
#endif
		}
		cursor += addressBytes;
		{
			unsigned int port = ( (unsigned int)cursor[0] << 8 ) | cursor[1];
			if ( port == 0 ) return qfalse;
			addresses[count].port = BigShort( (short)port );
			cursor += 2;
		}
		count++;
	}
	return qfalse;
}

static qboolean CL_GlobalAddressKnown( const netadr_t *address ) {
	if ( hash_find( address ) ) return qtrue;
	for ( int i = 0; i < cls.numGlobalServerAddresses; i++ ) {
		if ( NET_CompareAdr( address, &cls.globalServerAddresses[i] ) ) return qtrue;
	}
	return qfalse;
}


/*
===================
CL_ServersResponsePacket
===================
*/
static void CL_ServersResponsePacket( const netadr_t* from, msg_t *msg, qboolean extended ) {
	netadr_t addresses[MAX_SERVERSPERPACKET];
	unsigned int elapsed;
	int sourceIndex;
	int numservers = 0;
	int added = 0;

	if ( !cl_masterDiscovery.active ) {
		Com_Log( SEV_DEBUG, LOG_CH(ch_client),
			"Ignored master response reason=inactive source=%s kind=%s\n",
			NET_AdrToStringwPort( from ), extended ? "extended" : "classic" );
		return;
	}
	elapsed = (unsigned int)Sys_Milliseconds() - cl_masterDiscovery.start;
	if ( elapsed >= cl_masterDiscovery.timeout ) {
		cl_masterDiscovery.active = qfalse;
		Com_Log( SEV_DEBUG, LOG_CH(ch_client),
			"Ignored master response reason=expired generation=%u source=%s kind=%s elapsed=%ums\n",
			cl_masterDiscovery.generation, NET_AdrToStringwPort( from ),
			extended ? "extended" : "classic", elapsed );
		return;
	}
	sourceIndex = CL_MasterResponseSource( from, extended );
	if ( sourceIndex < 0 ) {
		Com_Log( SEV_DEBUG, LOG_CH(ch_client),
			"Ignored unauthorized %s from %s\n",
			extended ? "getserversExtResponse" : "getserversResponse",
			NET_AdrToStringwPort( from ) );
		Com_Log( SEV_DEBUG, LOG_CH(ch_client),
			"Ignored master response reason=source-or-kind generation=%u source=%s kind=%s\n",
			cl_masterDiscovery.generation, NET_AdrToStringwPort( from ),
			extended ? "extended" : "classic" );
		return;
	}
	if ( !CL_MasterResponseCommandMatches( msg, extended )
	  || !CL_ParseMasterResponseAddresses( from, msg, extended, addresses, &numservers ) ) {
		Com_Log( SEV_DEBUG, LOG_CH(ch_client),
			"Ignored master response reason=malformed generation=%u source=%s kind=%s\n",
			cl_masterDiscovery.generation, NET_AdrToStringwPort( from ),
			extended ? "extended" : "classic" );
		return;
	}

	for ( int i = 0; i < numservers; i++ ) {
		if ( CL_GlobalAddressKnown( &addresses[i] ) ) continue;
		if ( cls.numglobalservers < MAX_GLOBAL_SERVERS ) {
			hash_insert( &addresses[i] );
			CL_InitServerInfo( &cls.globalServers[cls.numglobalservers], &addresses[i] );
			cls.numglobalservers++;
			added++;
		} else if ( cls.numGlobalServerAddresses < MAX_GLOBAL_SERVERS ) {
			cls.globalServerAddresses[cls.numGlobalServerAddresses++] = addresses[i];
			added++;
		}
	}
	if ( added ) CL_BumpGlobalServerGeneration();
	Com_Log( SEV_INFO, LOG_CH(ch_client),
		"Accepted %s generation=%u address=%s parsed=%d total=%d\n",
		extended ? "getserversExtResponse" : "getserversResponse",
		cl_masterDiscovery.generation, NET_AdrToStringwPort( from ), numservers,
		cls.numglobalservers + cls.numGlobalServerAddresses );
	Com_Log( SEV_INFO, LOG_CH(ch_client),
		"Accepted master response generation=%u source_index=%d source=%s kind=%s parsed=%d added=%d total=%d elapsed=%ums\n",
		cl_masterDiscovery.generation, sourceIndex, NET_AdrToStringwPort( from ),
		extended ? "extended" : "classic", numservers, added,
		cls.numglobalservers + cls.numGlobalServerAddresses, elapsed );
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

	Com_Log( SEV_DEBUG, LOG_CH(ch_client), "CL packet %s: %s\n", NET_AdrToStringwPort( from ), s );

	// challenge from the server we are connecting to
	if ( !Q_stricmp(c, "challengeResponse" ) ) {

		if ( clientActiveApp->state != CA_CONNECTING ) {
			Com_Log( SEV_DEBUG, LOG_CH(ch_client), "Unwanted challenge response received. Ignored.\n" );
			return qfalse;
		}

		c = Cmd_Argv( 2 );
		if ( *c != '\0' )
			challenge = atoi( c );

		s = Cmd_Argv( 3 ); // analyze server protocol version
		if ( *s != '\0' ) {
			int sv_proto = atoi( s );
		}

		if ( *c == '\0' || challenge != clientActiveApp->clc.challenge )
		{
			Com_Log( SEV_INFO, LOG_CH(ch_client), "Bad challenge for challengeResponse. Ignored.\n" );
			return qfalse;
		}

		// start sending connect instead of challenge request packets
		clientActiveApp->clc.challenge = atoi(Cmd_Argv(1));
		CL_SetState( clientActiveApp, CA_CHALLENGING );
		clientActiveApp->clc.connectPacketCount = 0;
		clientActiveApp->clc.connectTime = cls.realtime - RECONNECT_TIMEOUT;

		// take this address as the new server address.  This allows
		// a server proxy to hand off connections to multiple servers
		clientActiveApp->clc.serverAddress = *from;
		Com_Log( SEV_DEBUG, LOG_CH(ch_client), "challengeResponse: %d\n", clientActiveApp->clc.challenge );
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
			Com_Log( SEV_INFO, LOG_CH(ch_client), "Wired RCON: invalid challenge received.\n" );
			return qfalse;
		}

		Q_strncpyz( clientActiveApp->clc.wiredRconChallenge, challengeStr, sizeof( clientActiveApp->clc.wiredRconChallenge ) );
		clientActiveApp->clc.wiredRconHasChallenge = qtrue;
		clientActiveApp->clc.wiredRconAddress = *from;

		Com_HMAC_SHA256_Hex( cl_wiredRconPassword ? cl_wiredRconPassword->string : "", clientActiveApp->clc.wiredRconChallenge, hmacHex );
		NET_OutOfBandPrint( NS_CLIENT, &clientActiveApp->clc.wiredRconAddress, "rcon_verify %s", hmacHex );
		return qfalse;
	}

	if ( !Q_stricmp( c, "rconAuthResult" ) ) {
		const char *result = Cmd_Argv( 1 );
		if ( !Q_stricmp( result, "ok" ) ) {
			clientActiveApp->clc.wiredRconAuthed = qtrue;
			Com_Log( SEV_INFO, LOG_CH(ch_client), "Wired RCON: authenticated.\n" );
		} else {
			clientActiveApp->clc.wiredRconAuthed = qfalse;
			Com_Log( SEV_INFO, LOG_CH(ch_client), "Wired RCON: authentication failed.\n" );
		}
		return qfalse;
	}

	/* "connectResponse" OOB handler removed — QUIC uses TLV ACCEPT on stream 0.
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
		if ( NET_CompareAdr( from, &clientActiveApp->clc.serverAddress ) ) {
			NET_OutOfBandPrint( NS_CLIENT, from, "%s", Cmd_Argv(1) );
			return qtrue;
		}
		return qfalse;
	}

	// legacy "keyAuthorize" packet handler removed — Wired never
	// talks to the id authorize server, so any such packet is unsolicited.

	// print string from server
	if ( !Q_stricmp(c, "print") ) {
		// NOTE: we may have to add exceptions for auth and update servers
		if ( NET_CompareAdr( from, &clientActiveApp->clc.serverAddress ) ) {
			s = MSG_ReadString( msg );
			Q_strncpyz( clientActiveApp->clc.serverMessage, s, sizeof( clientActiveApp->clc.serverMessage ) );
			Com_Log( SEV_INFO, LOG_CH(ch_client), "%s", s );
			return qtrue;
		}
		return qfalse;
	}

	// list of servers sent back by a master server (classic)
	if ( !strncmp( c, "getserversResponse", 18 )
	  && CL_MasterResponseCommandMatches( msg, qfalse ) ) {
		CL_ServersResponsePacket( from, msg, qfalse );
		return qfalse;
	}

	// list of servers sent back by a master server (extended)
	if ( !strncmp( c, "getserversExtResponse", 21 )
	  && CL_MasterResponseCommandMatches( msg, qtrue ) ) {
		CL_ServersResponsePacket( from, msg, qtrue );
		return qfalse;
	}

	Com_Log( SEV_DEBUG, LOG_CH(ch_client), "Unknown connectionless packet command.\n" );
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

	/* Only OOB packets (server browser infoResponse/statusResponse) are handled here.
	   All game traffic flows through QUIC streams and datagrams. */
	if ( *(int *)msg->data == -1 ) {
		if ( CL_ConnectionlessPacket( from, msg ) )
			clientActiveApp->clc.lastPacketTime = cls.realtime;
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
	qboolean isBothPaused = ( CL_CheckPaused() && sv_paused->integer );
	if ( clientActiveApp->timeoutWasBothPaused && !isBothPaused ) {
		clientActiveApp->clc.lastPacketTime = cls.realtime;

		/* Pause→unpause time re-baseline — SYMMETRIC (momentum-safe, 2026-07-09).
		 *
		 * cls.realtime advances during pause while cl.snap.serverTime is frozen,
		 * so on unpause the stale-forward delta makes serverTime leap ~pause-
		 * duration ahead of the snapshot → a late <RESET> lurch. The earlier
		 * "Fix B" re-baselined ONLY serverTimeDelta and left cl.oldServerTime at
		 * its pre-pause value T0; because T0 > snap.serverTime, the backwards-flow
		 * clamp (cl_cgame.c:2630) then PINNED serverTime at T0, every usercmd got
		 * stamped T0, the cgame skipped them all (cg_predict.c:510) → zero forward
		 * Pmove → the "drop straight down" momentum regression.
		 *
		 * The correct re-baseline is SYMMETRIC — set BOTH serverTimeDelta AND
		 * oldServerTime to the snapshot, exactly like the two legitimate re-
		 * baseline sites (CL_FirstSnapshot cl_cgame.c:2456-2457, <RESET>
		 * cl_cgame.c:2384-2386). With oldServerTime moved down to snap.serverTime,
		 * the backwards-flow clamp does NOT fire (serverTime == oldServerTime, not
		 * < it), so there is no pin: serverTime sits exactly on the snapshot and
		 * then advances forward normally, usercmds carry advancing time, Pmove
		 * integrates forward — momentum preserved AND no <RESET> lurch.
		 *
		 * Local single-player only by construction — sv_paused is set only by
		 * SV_CheckPaused for the lone-human integrated host, so a real networked
		 * client (sv_paused never set for it) never takes this edge and keeps its
		 * RTT-based time-sync untouched. Skipped in demo playback (delta fixed). */
		if ( clientActiveApp->cl.snap.valid && !clientActiveApp->clc.demoplaying ) {
			clientActiveApp->cl.serverTimeDelta =
				clientActiveApp->cl.snap.serverTime - cls.realtime;
			clientActiveApp->cl.oldServerTime = clientActiveApp->cl.snap.serverTime;
		}
	}
	clientActiveApp->timeoutWasBothPaused = isBothPaused;

	//
	// check timeout
	//
	if ( ( !CL_CheckPaused() || !sv_paused->integer )
		&& clientActiveApp->state >= CA_CONNECTED && clientActiveApp->state != CA_CINEMATIC
		&& cls.realtime - clientActiveApp->clc.lastPacketTime > cl_timeout->integer * 1000 ) {
		if ( ++clientActiveApp->cl.timeoutcount > 5 ) { // timeoutcount saves debugger
			Com_Log( SEV_INFO, LOG_CH(ch_client), "\nServer connection timed out.\n" );
			Com_SetLastError( "Server connection timed out." );
			if ( !CL_Disconnect( clientActiveApp, qfalse ) ) { // restart client if not done already
				CL_FlushMemory();
			}
			if ( UI_VM_ACTIVE ) {
				UI_CALL_SET_ACTIVE( UIMENU_MAIN );
			}
			return;
		}
	} else {
		clientActiveApp->cl.timeoutcount = 0;
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
	if ( CL_VideoRecording() || ( com_timedemo->integer && clientActiveApp->clc.demofile != FS_INVALID_HANDLE ) )
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
	if ( clientActiveApp->state < CA_CONNECTED )
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
			COM_WARN( LOG_CH(ch_client), "WARNING: oversize userinfo, you might be not able to play on remote server!\n" );
		}

		CL_AddReliableCommand( clientActiveApp, va( "userinfo \"%s\"", info ), qfalse );
	}
}


/*
==================
CL_CheckMatchAlerts

raise OS-level attention (taskbar flash, beep, audio unmute) when a match
is about to start while the window is unfocused or minimized.

We detect the transition by polling the CS_WARMUP configstring:
  - warmup > 0   : countdown running
  - warmup == 0  : waiting for warmup
  - warmup < 0   : match has started (server wrote -|restarttime|)

The engine has no direct "match started" event, so we store the
previous CS_WARMUP value and fire the alert on any warmup -> match or
warmup -> new-warmup-value transition.
==================
*/
// cl_lastWarmupValue / cl_matchAlertExpire relocated to clientApp_t
// (lastWarmupValue / matchAlertExpire) — per-app, in-process-queue L6.

static void CL_CheckMatchAlerts( void )
{
	clientApp_t *app = CL_ActiveApp();
	qboolean fire = qfalse;

	/* clear the s_autoMute override once the alert window closes */
	if ( app->matchAlertExpire > 0 && Sys_Milliseconds() >= app->matchAlertExpire ) {
		app->matchAlertExpire = 0;
		S_SetMuteOverride( qfalse );
	}

	if ( clientActiveApp->state != CA_ACTIVE ) {
		app->lastWarmupValue = 0;
		return;
	}

	if ( cl_matchAlerts == NULL || cl_matchAlerts->integer == 0 )
		return;

	/* read CS_WARMUP from the current gamestate */
	if ( CS_WARMUP < 0 || CS_WARMUP >= MAX_CONFIGSTRINGS )
		return;

	int ofs = clientActiveApp->cl.gameState.stringOffsets[ CS_WARMUP ];
	if ( ofs == 0 )
		return;

	const char *s = clientActiveApp->cl.gameState.stringData + ofs;
	int warmup = atoi( s );

	/* trigger whenever the warmup value transitions — either warmup
	   begins (0 -> positive), a new restart time is scheduled
	   (positive -> new positive), or warmup ends (positive -> 0 or
	   anything -> negative for "match live") */
	if ( warmup != app->lastWarmupValue ) {
		/* only treat transitions that actually represent a match start
		   event — avoid firing on the initial gamestate parse where
		   app->lastWarmupValue is still 0 and warmup is 0 */
		if ( app->lastWarmupValue != 0 || warmup != 0 ) {
			fire = qtrue;
		}
		app->lastWarmupValue = warmup;
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
		app->matchAlertExpire = Sys_Milliseconds() + 5000;
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
// cl_pendingConnectError relocated to clientApp_t.pendingConnectError —
// per-app, in-process-queue L6. Accessed via CL_ActiveApp().

static void CL_CheckConnectError( void )
{
	char msg[512];
	char retryTarget[MAX_OSPATH];
	netConnectErrorKind_t errorKind = NET_CONNECT_ERROR_NONE;
	int retryGeneration = 0;
	qboolean retryAuthentication = qfalse;

	// EB7: guard covers CA_CONNECTING + CA_CHALLENGING and any state
	// between disconnected and fully active.
	if ( clientActiveApp->state <= CA_DISCONNECTED || clientActiveApp->state >= CA_ACTIVE )
		return;
	if ( !( transport && transport->get_error
	     && transport->get_error( msg, sizeof(msg), &errorKind ) ) )
		return;

	retryTarget[0] = '\0';
	if ( errorKind == NET_CONNECT_ERROR_AUTH_REFUSED
	     && clientActiveApp->clc.joinAttempt.browserOrigin ) {
		retryAuthentication = qtrue;
		retryGeneration = clientActiveApp->clc.joinAttempt.selectionGeneration;
		Q_strncpyz( retryTarget, clientActiveApp->clc.joinAttempt.target,
			sizeof( retryTarget ) );
	}

	// Consume before CL_Disconnect so the error slot is clean on retry.
	if ( transport && transport->clear_error )
		transport->clear_error();

	Com_Log( SEV_WARN, LOG_CH(ch_client),
		"Connect failed kind=%d browser_retry=%d\n", (int)errorKind,
		retryAuthentication ? 1 : 0 );
	if ( !retryAuthentication ) {
		Com_SetLastError( "%s", msg );
	}
	CL_Disconnect( clientActiveApp, qfalse );

	if ( retryAuthentication
	     && CL_WiredUI_ShowJoinPasswordRetry( retryTarget, retryGeneration ) ) {
		return;
	}

	if ( cls.uiStarted ) {
		// UI is still up — show the error dialog directly.
		UI_CALL_SET_ACTIVE( UIMENU_MAIN );
		CL_WiredUI_ShowError( "Connection Failed", msg, qtrue );
	} else {
		// Renderer is down (SV_SpawnServer wiped it).  Restart everything
		// and let the deferred check in CL_StartHunkUsers show the dialog.
		Q_strncpyz( CL_ActiveApp()->pendingConnectError, msg, sizeof( CL_ActiveApp()->pendingConnectError ) );
		CL_FlushMemory();
	}
}

/*
==================
CL_Frame
==================
*/
void CL_Frame( int msec, int realMsec ) {


	if ( !com_cl_running->integer ) {
		return;
	}

#if FEAT_WIRED_UI
	CL_PROF(store, WiredStore_BeginFrame());
	/* Keep Clay layout state coherent.
	 * Does NOT yet drive rendering — SCR_DrawScreenField still dispatches
	 * panels below. Later passes take over rendering and retire the legacy
	 * dispatcher (docs/wiredui-compositor-spec.md §13). */
	WiredUI_CompositorFrame( cls.realtime );
#endif

	// save the msec before checking pause
	cls.realFrametime = realMsec;


	/* Boot auto-push retired: at CA_DISCONNECTED the bg_attract layer is
	 * the sole visible surface (policy/bg_attract.c gates only on
	 * clientActiveApp->state). The first non-ESC keypress / mouse click promotes the
	 * user into the main menu (cl_keys.c CL_KeyDownEvent first-input
	 * branch). Error-recovery transitions (timeout, connect-failed,
	 * deferred connect error) still call UI_CALL_SET_ACTIVE(UIMENU_MAIN)
	 * explicitly elsewhere to surface the error dialog over main. */

	// if recording an avi, lock to a fixed fps
	if ( CL_VideoRecording() && msec ) {
		// save the current screen
		if ( clientActiveApp->state == CA_ACTIVE || cl_forceavidemo->integer ) {
			float fps, frameDuration;

			if ( com_timescale->value > 0.0001f )
				fps = MIN( cl_aviFrameRate->value / com_timescale->value, 1000.0f );
			else
				fps = 1000.0f;

			frameDuration = MAX( 1000.0f / fps, 1.0f ) + clientActiveApp->clc.aviVideoFrameRemainder;

			CL_TakeVideoFrame();

			msec = (int)frameDuration;
			clientActiveApp->clc.aviVideoFrameRemainder = frameDuration - msec;

			realMsec = msec; // sync sound duration
		}
	}

	if ( cl_autoRecordDemo->integer && !clientActiveApp->clc.demoplaying ) {
		if ( clientActiveApp->state == CA_ACTIVE && !clientActiveApp->clc.demorecording ) {
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

			Q_strncpyz( serverName, clientActiveApp->servername, MAX_OSPATH );
			// Replace the ":" in the address as it is not a valid
			// file name character
			char *p = strchr( serverName, ':' );
			if ( p ) {
				*p = '.';
			}

			Q_strncpyz( mapName, COM_SkipPath( clientActiveApp->cl.mapname ), sizeof( clientActiveApp->cl.mapname ) );
			COM_StripExtension(mapName, mapName, sizeof(mapName));

			Cbuf_ExecuteText( EXEC_NOW,
					va( "record %s-%s-%s", nowString, serverName, mapName ) );
		}
		else if ( clientActiveApp->state != CA_ACTIVE && clientActiveApp->clc.demorecording ) {
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
	if ( !clientActiveApp->clc.demoplaying ) { CL_PROF(misc, CL_CheckConnectError()); }
	if ( !clientActiveApp->clc.demoplaying ) { CL_PROF(misc, CL_CheckTimeout()); }
	CL_PROF(send,    CL_SendCmd());
	/* Per-app outbound ACK (in-process-queue L5): the input-focused app sent its
	 * acks via CL_SendCmd above (with real usercmds); every OTHER live app must
	 * still ack-send each frame so the server's delta baseline for its connection
	 * does not starve. At N=1 there is no non-focused live app -> zero iterations
	 * -> byte-identical (clientActiveApp is the only connected app). */
	for ( int ai = 0; ai < MAX_LOCAL_CGAME_VMS; ai++ ) {
		clientApp_t *other = &clientApps[ai];
		if ( other == clientActiveApp )
			continue;
		if ( other->state >= CA_CONNECTED ) {
			/* Point the faulting-app cursor at the app being serviced so a
			 * recoverable error here drops THIS app (not the focused one), and
			 * arm THIS app's per-frame recovery point so the TERM_CLIENT_DROP
			 * longjmp (Com_Terminate → cl_frameApp->appAbortFrame) lands in an
			 * armed buffer. A drop during this app's service (e.g. CL_SetCGameTime
			 * → Com_Terminate) tears the app down in Com_Terminate, longjmps here,
			 * and we continue to the next app — the focused app + other apps
			 * survive. N=1: this loop has zero iterations, so EDIT 5 never runs. */
			cl_frameApp = other;
			if ( Q_setjmp( other->appAbortFrame ) ) {
				CL_AbortFrame();                 // this app dropped; teardown already done in Com_Terminate
				cl_frameApp = clientActiveApp;   // restore cursor before next iteration
				continue;                        // survive: skip to the next app
			}
			CL_SendAckOnly( other );
			/* A non-focused client without a cgame VM (a runtime-headless
			 * bot/MCP client) advances its own state machine here — the
			 * input-focused client runs the full CL_SetCGameTime path below.
			 * A non-focused client that DOES have a cgvm (a future split-screen
			 * view) is left to a cgame-aware driver, not this VM-less one. */
			if ( !other->cgvm ) {
				/* Once its gamestate has been ingested (CA_CONNECTED), prime the
				 * headless client to CA_PRIMED without a cgame VM and tell the
				 * server it is ready; from CA_PRIMED, CL_DriveHeadlessApp enters
				 * the world on the first snapshot. */
				if ( other->state == CA_CONNECTED && other->cl.gameState.dataCount > 1 ) {
					CL_PrimeHeadlessApp( other );
					WN_ClientSendReady( (int)( other - clientApps ) );
				}
				CL_DriveHeadlessApp( other );
			} else {
				/* A non-focused client that has its own cgame VM advances its own
				 * snapshot interpolation + world time each frame so its viewport
				 * renders a live scene, without it holding input focus. The render
				 * is pulled by the compositor through this client's registered
				 * viewport provider; here we only advance time/state. */
				CL_SetCGameTime( other );
			}
		}
	}
	/* Restore the cursor to the focused app for the remainder of the frame
	 * (focused CL_SetCGameTime + the render path); the boundary in common.c
	 * also re-arms it to clientActiveApp next frame. */
	cl_frameApp = clientActiveApp;
	CL_PROF(resend,  CL_CheckForResend());
	CL_PROF(cgtime,  CL_SetCGameTime( clientActiveApp ));
	CL_PROF(misc,    CL_CheckMatchAlerts());
	cls.framecount++;
#if FEAT_WIRED_UI
	/* C-14 follow-up A: per-frame state publishers — fire BEFORE
	 * SCR_UpdateScreen so the compositor emit walk (inside
	 * SCR_DrawScreenField) reads current-frame values, eliminating the
	 * one-frame lag from the previous render-time publisher sites at
	 * CL_DrawLoadingScreen top + WiredUI_DrawConnectScreen top.
	 * Each publisher self-gates on clientActiveApp->state range to match the legacy
	 * SCR invocation conditions (CA_CONNECTING through CA_PRIMED). */
	CL_PublishLoadingState();
	CL_PublishConnectState();
#endif
	SCR_UpdateScreen();
	CL_PROF(sound,   S_Update( realMsec ));
	CL_PROF(misc,    SCR_RunCinematic());
	CL_PROF(misc,    Con_RunConsole( ( Key_GetCatcher() & KEYCATCH_CONSOLE ) != 0, cls.realFrametime ));
}


//============================================================================

/*
============
CL_ShutdownRef
============
*/
static void CL_ShutdownRef( refShutdownCode_t code ) {

	// Drop the /meminfo GPU hook before the renderer tears down — re.GetMemoryBudget
	// would dangle once the DLL unloads. Re-registered by the next CL_InitRenderer.
	Com_RegisterGpuMemReport( NULL );

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

	// SCR_Done() drops screen state, then re.Shutdown releases the renderer's GPU
	// resources. Do NOT pump a render frame from here on — the render state is
	// mid-tear-down until the renderer re-initializes; a frame here would touch
	// freed resources.
	SCR_Done();

	CL_ProfileTelemetry_RendererStopping();
	if ( re.Shutdown ) {
		re.Shutdown( code );
	}

#ifdef USE_RENDERER_DLOPEN
	if ( rendererLib ) {
		Sys_UnloadLibrary( rendererLib );
		rendererLib = NULL;
	}
	s_rendererLoadPath[0] = '\0';
#endif

	memset( &re, 0, sizeof( re ) );

	cls.rendererStarted = qfalse;
	cls.wiredUIStarted  = qfalse;
}


#ifdef USE_RENDERER_DLOPEN
/*
============
CL_TryNextRenderer

Advances `cl_renderer` to the next entry in s_renderer_fallback_list and
queues a vid_restart. Called from CL_InitRenderer when the just-loaded
renderer flagged a recoverable init failure (re.initFailed). The cursor
ONLY advances — a renderer that just failed is never retried within the
same fallback chain, so the walk is loop-safe by construction. If the
current value is not in the list, treat as before-head (start the walk
from list[0]). End-of-list ⇒ Com_Terminate(TERM_UNRECOVERABLE).
============
*/
static void CL_TryNextRenderer( const char *failed ) {
	int idx = -1;
	int next;
	for ( int i = 0; i < (int)ARRAY_LEN( s_renderer_fallback_list ); i++ ) {
		if ( failed && Q_stricmp( failed, s_renderer_fallback_list[i] ) == 0 ) {
			idx = i;
			break;
		}
	}
	// idx == -1 (current not in list) ⇒ start the walk from the head; otherwise
	// advance one. next == ARRAY_LEN means the last entry just failed.
	next = ( idx < 0 ) ? 0 : ( idx + 1 );
	if ( next >= (int)ARRAY_LEN( s_renderer_fallback_list ) ) {
		Com_Terminate( TERM_UNRECOVERABLE,
			"renderer fallback exhausted: '%s' (last in list) failed to initialize; no renderer could initialize",
			failed ? failed : "(null)" );
		return;
	}
	Com_Log( SEV_WARN, LOG_CH(ch_client),
		"renderer '%s' failed to initialize; advancing cl_renderer to '%s' and restarting video\n",
		failed ? failed : "(null)", s_renderer_fallback_list[next] );
	Cvar_Set( "cl_renderer", s_renderer_fallback_list[next] );
	// Run the restart inline (not via Cbuf_AddText) so we re-enter
	// CL_InitRenderer with the new cl_renderer BEFORE the outer
	// CL_StartHunkUsers continues — otherwise it would invoke re.RegisterShader
	// / CL_Characters_* on the half-initialised declined renderer (vk dispatch
	// tables intact but vk state never came up). REF_DESTROY_WINDOW; the
	// cl_renderer modificationCount bump inside CL_ShutdownRef escalates it
	// to REF_UNLOAD_DLL so the previous DLL is unloaded.
	CL_Vid_Restart( REF_DESTROY_WINDOW );
}
#endif


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
	cls.glconfigGeneration++;	// signal cgame that glconfig / screen dims may have changed

#ifdef USE_RENDERER_DLOPEN
	// Recoverable init-failure poll. The renderer mutates s_re_dll->initFailed
	// from inside BeginRegistration when a runtime check (e.g. GPU caps)
	// makes it non-viable. The engine-side `re` is a stale COPY made at
	// CL_InitRef time, so the flag must be re-read via s_re_dll. On a
	// recoverable failure, advance cl_renderer + vid_restart and bail before
	// touching re.RegisterShader / SCR_Init / etc.
	if ( s_re_dll != NULL && s_re_dll->initFailed ) {
		re.initFailed = qtrue;
		CL_TryNextRenderer( cl_renderer ? cl_renderer->string : NULL );
		return;
	}
#endif

	// Surface the renderer's GPU memory budget in /meminfo (qcommon owns the
	// command but can't reach the renderer; this client hook bridges it).
	Com_RegisterGpuMemReport( CL_GpuMemReport );

	// load character sets
	cls.whiteShader = re.RegisterShader( "*white" );
	cls.consoleShader = re.RegisterShader( "console" );
	// Phase 7.15.4-a class-B pin: the console background is a persistent, client-
	// cached handle bound every console frame without re-resolving residency, so
	// the texture-LRU must never evict it. (*white is already class-A pinned by
	// its '*' name.) Dark in 7.15.4-a — the pin has no reader yet.
	if ( re.PinShaderImages ) re.PinShaderImages( cls.consoleShader );

	Con_CheckResize();

	// Defensive guard: smallchar_width (the DPI-scaled runtime console cell
	// width, set in wired/ui/elements/console.c) can still be 0 on the map-load
	// path when this recompute runs before the console UI element has
	// initialised — a bare divide there is a div-by-zero crash that kills the
	// loading render. Fall back to the base SMALLCHAR_WIDTH so the field width
	// stays sane. The real init-ordering (why it's 0 here) is the rel-migration's
	// to settle; this is only the minimal crash guard.
	{
		int consoleCharW = ( smallchar_width > 0 ) ? smallchar_width : SMALLCHAR_WIDTH;
		g_console_field_width = ( cls.glconfig.vidWidth / consoleCharW ) - 2;
	}
	g_consoleField.widthInChars = g_console_field_width;

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

	if ( clientActiveApp->state >= CA_LOADING ) {
		// try to apply map-depending configuration from cvar cl_mapConfig_<mapname> cvars
		const char *info = clientActiveApp->cl.gameState.stringData + clientActiveApp->cl.gameState.stringOffsets[ CS_SERVERINFO ];
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
		if ( !WiredUI_Init( clientActiveApp->state >= CA_AUTHORIZING && clientActiveApp->state < CA_ACTIVE ) ) {
			/* V-25/V-26 (2026-05-25): graceful failure policy.
			 * wui_required=1 → fatal (UI unrecoverable from here).
			 * wui_required=0 → SEV_WARN + headless fallback; the engine
			 * continues but WiredUI_RenderFrame's !wui_clay_initialized
			 * gate keeps per-frame work a no-op. */
			if ( Cvar_VariableIntegerValue( "wui_required" ) != 0 ) {
				Com_Terminate( TERM_CLIENT_DROP,
					"WiredUI init failed and wui_required=1" );
			} else {
				Com_Log( SEV_WARN, LOG_CH(ch_client),
					"WiredUI init failed (wui_required=0); continuing "
					"headless without UI mode\n" );
			}
		}
	}
#endif

	if ( !cls.uiStarted ) {
		cls.uiStarted = qtrue;

		// Show any connect error that was deferred across the UI restart
		// triggered by CL_Disconnect() inside CL_CheckConnectError.
		if ( CL_ActiveApp()->pendingConnectError[0] ) {
			char pendingMsg[512];
			Q_strncpyz( pendingMsg, CL_ActiveApp()->pendingConnectError, sizeof( pendingMsg ) );
			CL_ActiveApp()->pendingConnectError[0] = '\0';
			UI_CALL_SET_ACTIVE( UIMENU_MAIN );
			// Publish the message into com_errorMessage — the error_popup.wui
			// dialog binds its text field to that cvar. It was set earlier by
			// Com_SetLastError when the connect failed, but the failure was
			// deferred across a UI restart and intervening errors may have
			// overwritten it, so set it explicitly to the message we are about
			// to show. Without this the dialog appears with empty text.
			Com_SetLastError( "%s", pendingMsg );
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
	clientActiveApp->captureWidth = captureWidth;
	clientActiveApp->captureHeight = captureHeight;
}


/*
============
CL_InitRef
============
*/
static void QDECL RI_Log( log_severity_t severity, const char *fmt, ... ) {
	va_list args;
	va_start( args, fmt );
	Com_Logv( severity, LOG_CH(ch_renderer), fmt, args );
	va_end( args );
}

/*
============
RI_LogCh — channel-aware sink for the rilog-channel-mechanism (Turn A).
The renderer DLL caches the channel id from ri.GetLogChannel (engine-side
Log_GetChannel) per TU; here we just dispatch to Com_Logv with that id.
The legacy RI_Log above stays for the unmigrated ri.Log call sites — they
all route to the `renderer` root channel.
============
*/
static void QDECL RI_LogCh( int channel, log_severity_t severity, const char *fmt, ... ) {
	va_list args;
	va_start( args, fmt );
	Com_Logv( severity, channel, fmt, args );
	va_end( args );
}

/*
=================
CL_RendererLoadPath

Physical path the renderer DLL was dlopen'd from, for the sysinfo "loaded-from"
diagnostic. "" before load / after shutdown. With a statically-linked renderer
(no dlopen) there is no separate file → reports "(static renderer)".
=================
*/
const char *CL_RendererLoadPath( void ) {
#ifdef USE_RENDERER_DLOPEN
	return s_rendererLoadPath;
#else
	return "(static renderer)";
#endif
}

static void CL_InitRef( void ) {
	refimport_t	rimp;
	refexport_t	*ret;
#ifdef USE_RENDERER_DLOPEN
	GetRefAPI_t		GetRefAPI;
	char			dllName[ MAX_OSPATH ], *ospath;
#endif

	CL_InitGLimp_Cvars();

	Com_Log( SEV_INFO, LOG_CH(ch_client), "----- Initializing Renderer ----\n" );

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
	if ( rendererLib )
	{
		// Copy NOW: FS_BuildOSPath returns a rotating static buffer.
		Q_strncpyz( s_rendererLoadPath, ospath, sizeof( s_rendererLoadPath ) );
	}
	if ( !rendererLib )
	{
		Cvar_ForceReset( "cl_renderer" );
		Com_sprintf( dllName, sizeof( dllName ), RENDERER_PREFIX "_%s_" REND_ARCH_STRING DLL_EXT, cl_renderer->string );
		ospath = FS_BuildOSPath( FS_GetInstallBinaryPath(), dllName, NULL );
		rendererLib = Sys_LoadLibrary( ospath );
		if ( rendererLib )
		{
			Q_strncpyz( s_rendererLoadPath, ospath, sizeof( s_rendererLoadPath ) );
		}
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
	// rilog-channel-mechanism Turn A — channel-aware refimport entries.
	// rimp.GetLogChannel lets the renderer DLL resolve named channels
	// (e.g. "renderer.init") and cache the integer id per-TU; rimp.LogCh
	// dispatches to that channel. R_LOG macros in the renderer wrap both.
	rimp.GetLogChannel = Log_GetChannel;
	rimp.LogCh         = RI_LogCh;
	// Floor the `renderer` root channel to WARN by default so the migrated
	// R_LOG(renderer.X, INFO, …) calls (and the still-unmigrated ri.Log
	// INFO sites, which route through the same root via RI_Log above)
	// stay silent in default builds. Inheritance via the dot-hierarchy
	// means every sub-channel that lacks its own override picks up WARN
	// from "renderer". User opts in per channel with `log renderer.init
	// info` (or `log renderer info` for the whole tree).
	{
		int rch = Log_GetChannel( "renderer" );
		if ( rch >= 0 && rch < log_channelCount ) {
			log_channels[ rch ].overrideSev = (int)SEV_WARN;
		}
		// renderer.screenshot overrides the root's WARN floor back to INFO:
		// the `screenshot` command's "Screenshot saved as ..." confirmation
		// is a deliberate, rare, user-facing event that must stay visible.
		// Only screenshot events route to this channel, so INFO floods
		// nothing.
		{
			int rchss = Log_GetChannel( "renderer.screenshot" );
			if ( rchss >= 0 && rchss < log_channelCount ) {
				log_channels[ rchss ].overrideSev = (int)SEV_INFO;
			}
		}
		Log_ResolveAllChannels();
		// Pre-register the renderer sub-channels so they appear in
		// `log channels` output even before any R_LOG call hits them.
		// Lazy resolve would still work, but discoverability matters
		// for the workstream's UX. Each call is idempotent — if a TU's
		// R_LOG_DECLARE_CHANNEL has the same name, both share the id.
		(void)Log_GetChannel( "renderer.init"    );
		(void)Log_GetChannel( "renderer.shaders" );
		(void)Log_GetChannel( "renderer.assets"  );
		(void)Log_GetChannel( "renderer.vk"      );
		(void)Log_GetChannel( "renderer.gl"      );  /* Turn C — GL1/GL2 backend-internal logging */
		(void)Log_GetChannel( "renderer.ral"     );
		(void)Log_GetChannel( "renderer.hdr"     );
		(void)Log_GetChannel( "renderer.fbo"     );
		(void)Log_GetChannel( "renderer.timing"  );
		(void)Log_GetChannel( "renderer.cmd"     );
		(void)Log_GetChannel( "renderer.temporal" );
		(void)Log_GetChannel( "renderer.screenshot" );
	}
	rimp.Terminate = Com_Terminate;
	rimp.Milliseconds = CL_ScaledMilliseconds;
	rimp.Microseconds = Sys_Microseconds;
	rimp.Malloc = CL_RefMalloc;
	rimp.FreeAll = CL_RefFreeAll;
	rimp.Free = Z_Free;

	/* Bridge the engine's qcommon/arena.c API into the
	 * renderer DLL so renderer-side persistent allocations register with
	 * the engine's process-global arena list (visible in /meminfo). */
	rimp.Arena_Create  = Arena_Create;
	rimp.Arena_Destroy = Arena_Destroy;
	rimp.Arena_Alloc   = Arena_Alloc;
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
	rimp.CM_BoxTrace = CM_BoxTrace;	/* sun-mask ray-cast bridge; cm.tracer dispatches q1/q3 */

	rimp.FS_ReadFile = FS_ReadFile;
	rimp.FS_FreeFile = FS_FreeFile;
	rimp.FS_WriteFile = FS_WriteFile;
	rimp.FS_FreeFileList = FS_FreeFileList;
	rimp.FS_ListFiles = FS_ListFiles;
	//rimp.FS_FileIsInPAK = FS_FileIsInPAK;
	rimp.FS_FileExists = FS_FileExists;

	rimp.Map_Load = Map_Load;
	rimp.Map_Free = Map_Free;

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
	rimp.Sys_SetClipboardImagePNG = Sys_SetClipboardImagePNG;
	rimp.Sys_LowPhysicalMemory = Sys_LowPhysicalMemory;
	rimp.Com_RealTime = Com_RealTime;

	rimp.GetCharacterSkin = CL_GetCharacterSkin;
	rimp.AssetLog_Event = AssetLog_Event;
	rimp.MetaRemap_Lookup = MetaRemap_LookupAdapter;

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
	rimp.VK_GetInstanceExtensions = VK_GetInstanceExtensions;
	rimp.VK_CreateSurface = VK_CreateSurface;
#endif

	ret = GetRefAPI( REF_API_VERSION, &rimp );

	Com_Log( SEV_INFO, LOG_CH(ch_client), "-------------------------------\n");

	if ( !ret ) {
		Com_Terminate( TERM_UNRECOVERABLE, "Couldn't initialize refresh" );
	}

	re = *ret;
	CL_ProfileTelemetry_RendererStarted();
#ifdef USE_RENDERER_DLOPEN
	// Keep a pointer to the DLL's static refexport_t so the post-
	// BeginRegistration poll can observe the renderer's recoverable
	// init-failure mutation of initFailed.
	s_re_dll = ret;
#endif

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
		Com_Log( SEV_INFO, LOG_CH(ch_client), "char is set to %s\n", name );
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
		Com_Log( SEV_INFO, LOG_CH(ch_client), "skin is set to %s\n", name );
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

	if( !clientActiveApp->clc.demoplaying )
	{
		Com_Log( SEV_INFO, LOG_CH(ch_client), "The %s command can only be used when playing back demos\n", Cmd_Argv( 0 ) );
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
			COM_ERROR( LOG_CH(ch_client), "ERROR: no free file names to create video\n" );
			return;
		}

		// without extension
		Com_sprintf( filename, sizeof( filename ), "videos/video%04d", i );
	}


	clientActiveApp->clc.aviSoundFrameRemainder = 0.0f;
	clientActiveApp->clc.aviVideoFrameRemainder = 0.0f;

	Q_strncpyz( clientActiveApp->clc.videoName, filename, sizeof( clientActiveApp->clc.videoName ) );
	clientActiveApp->clc.videoIndex = 0;

	CL_OpenAVIForWriting( va( "%s.%s", clientActiveApp->clc.videoName, ext ), pipe, qfalse );
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
	Com_Log( SEV_INFO, LOG_CH(ch_client), "\n" );
	for ( int i = 0; i < s_numVidModes; i++ )
	{
		Com_Log( SEV_INFO, LOG_CH(ch_client), "%s\n", cl_vidModes[ i ].description );
	}
	Com_Log( SEV_INFO, LOG_CH(ch_client), "\n" );
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
	/* 11 */ CVAR_INT(    "r_stencilBits",       "8",               CVAR_ARCHIVE | CVAR_NODEFAULT | CVAR_LATCH,  "Stencil buffer size, required to be 8 for stencil shadows.", 0, 8 ),
	/* 12 */ CVAR_INT(    "r_depthBits",         "0",               CVAR_ARCHIVE | CVAR_NODEFAULT | CVAR_LATCH,  "Sets precision of Z-buffer.", 0, 32 ),
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
CL_RalDump_f

"\ral_dump": dump the renderer's GPU Renderer Abstraction Layer
state (backend probe, capabilities, memory budget). The RAL lives inside the
renderer DLL; resolve and call its exported Ral_Dump(). Developer diagnostic;
prints a note and does nothing if the loaded renderer has no RAL backend
(e.g. the OpenGL renderers, which export no Ral_Dump).
====================
*/
// GPU-memory section for /meminfo. Registered with qcommon (which owns the
// command, but must not depend on the renderer); pulls the budget through the
// re-export interface. No-op when the loaded renderer exposes no budget API
// (the GL renderers leave re.GetMemoryBudget NULL).
static void CL_GpuMemReport( void ) {
	uint64_t dlUsed = 0, dlBudget = 0, hvUsed = 0, hvBudget = 0;
	int      level = 0;
	qboolean real;
	const char *lvlName;

	if ( !re.GetMemoryBudget )
		return;

	real    = re.GetMemoryBudget( &dlUsed, &dlBudget, &hvUsed, &hvBudget, &level );
	lvlName = ( level >= 2 ) ? "CRITICAL" : ( level == 1 ) ? "warning" : "normal";

	Com_Log( SEV_INFO, LOG_CH(ch_client), "\nGPU MEMORY (%s):\n", real ? "reported" : "estimated" );
	Com_Log( SEV_INFO, LOG_CH(ch_client), "  device-local  %6u / %6u MiB   (%u%%)\n",
		(unsigned)( dlUsed >> 20 ), (unsigned)( dlBudget >> 20 ),
		dlBudget ? (unsigned)( ( dlUsed * 100ull ) / dlBudget ) : 0u );
	Com_Log( SEV_INFO, LOG_CH(ch_client), "  host-visible  %6u / %6u MiB   (%u%%)\n",
		(unsigned)( hvUsed >> 20 ), (unsigned)( hvBudget >> 20 ),
		hvBudget ? (unsigned)( ( hvUsed * 100ull ) / hvBudget ) : 0u );
	Com_Log( SEV_INFO, LOG_CH(ch_client), "  pressure      %s\n", lvlName );
}

static void CL_RalDump_f( void ) {
#ifdef USE_RENDERER_DLOPEN
	void ( *ralDump )( void );
	const char *sub;

	if ( !rendererLib ) {
		Com_Log( SEV_INFO, LOG_CH(ch_client), "ral_dump: no renderer module loaded\n" );
		return;
	}

	// "\ral_dump live" inspects the renderer's OWN backend
	// (the imported-mode one shared with vk.device) rather than creating a
	// throwaway. Falls through to the standard Ral_Dump when no subcmd or
	// when the renderer's that old.
	sub = Cmd_Argv( 1 );
	if ( sub && Q_stricmp( sub, "live" ) == 0 ) {
		ralDump = Sys_LoadFunction( rendererLib, "Ral_DumpLive" );
		if ( !ralDump ) {
			Com_Log( SEV_INFO, LOG_CH(ch_client), "ral_dump live: the loaded renderer module exports no Ral_DumpLive (pre-7.4c-pre build?)\n" );
			return;
		}
		ralDump();
		return;
	}

	ralDump = Sys_LoadFunction( rendererLib, "Ral_Dump" );
	if ( !ralDump ) {
		Com_Log( SEV_INFO, LOG_CH(ch_client), "ral_dump: the loaded renderer module exports no RAL backend (try \"cl_renderer vulkan\")\n" );
		return;
	}
	ralDump();
#else
	Com_Log( SEV_INFO, LOG_CH(ch_client), "ral_dump: not available -- renderer is statically linked (rebuild with USE_RENDERER_DLOPEN)\n" );
#endif
}


/*
====================
CL_RalPipelineTest_f

"\ral_pipeline_test": run the renderer's exact offscreen RAL pipeline
exercise (layout sharing, draw/readback, compute dispatch and cache roundtrip).
Resolves Ral_PipelineTest in the renderer DLL; if the loaded renderer doesn't
export it (e.g. OpenGL), prints a note and returns.
====================
*/
/*
====================
CL_WaitForMap_Ready

Predicate registered with Cbuf_RegisterWaitForMapCheck. Returns qtrue when
the local map is fully loaded — i.e. the client has reached CA_ACTIVE and,
if a local server is running, its async spawn machine is back to
SPAWN_IDLE. Used by the /waitForMap command to gate the smoke-test cbuf
on real map-load completion (vs. a fixed +wait N frames, which races the
spawn machine's per-phase Cbuf_Wait calls).
====================
*/
static qboolean CL_WaitForMap_Ready( void ) {
	if ( clientActiveApp->state != CA_ACTIVE ) {
		return qfalse;
	}
	if ( com_sv_running && com_sv_running->integer && !SV_IsSpawnIdle() ) {
		return qfalse;
	}
	return qtrue;
}


/*
====================
CL_WaitForMap_f

Console command — yields Cbuf until CL_WaitForMap_Ready() returns qtrue or
the safety timeout (WAITFORMAP_MAX_FRAMES) expires.
====================
*/
#define WAITFORMAP_MAX_FRAMES 2000
static void CL_WaitForMap_f( void ) {
	Com_Log( SEV_INFO, LOG_CH(ch_client),
		"waitForMap: arming Cbuf gate (timeout %d frames)\n",
		WAITFORMAP_MAX_FRAMES );
	Cbuf_RequestWaitForMap( WAITFORMAP_MAX_FRAMES );
}


static void CL_RalPipelineTest_f( void ) {
#ifdef USE_RENDERER_DLOPEN
	void ( *ralPipelineTest )( void );

	if ( !rendererLib ) {
		Com_Log( SEV_INFO, LOG_CH(ch_client), "ral_pipeline_test: no renderer module loaded\n" );
		return;
	}
	ralPipelineTest = Sys_LoadFunction( rendererLib, "Ral_PipelineTest" );
	if ( !ralPipelineTest ) {
		Com_Log( SEV_INFO, LOG_CH(ch_client), "ral_pipeline_test: the loaded renderer module exports no Ral_PipelineTest (try \"cl_renderer vulkan\")\n" );
		return;
	}
	ralPipelineTest();
#else
	Com_Log( SEV_INFO, LOG_CH(ch_client), "ral_pipeline_test: not available -- renderer is statically linked (rebuild with USE_RENDERER_DLOPEN)\n" );
#endif
}


/*
====================
CL_Init
====================
*/
void CL_Init( void ) {
	Com_Log( SEV_INFO, LOG_CH(ch_client), "----- Client Initialization -----\n" );
	CL_ClearServerQueryTransactions();

	Con_Init();
	Con_InitProjection();   /* UI presentation half (colors, fields, commands, close hook) */

	CL_ClearState( clientActiveApp );
	CL_SetState( clientActiveApp, CA_DISCONNECTED );	// no longer CA_UNINITIALIZED

	CL_ResetOldGame();

	cls.realtime = 0;

	CL_InitInput();
	CL_ProfileTelemetry_Init();

#if FEAT_WIRED_UI
	/* Compositor lifecycle lives at
	 * process scope (CL_Init / CL_Shutdown), NOT WiredUI_Init / Shutdown.
	 * WiredUI_Init runs again every map load via CL_FlushMemory →
	 * CL_ShutdownAll → CL_StartHunkUsers; keeping the compositor here
	 * means the arena, Clay context, font indirection table and measure
	 * callback survive map loads + vid_restart. Per memory-architecture-
	 * handoff §"CL_ShutdownAll Split", CL_Init runs exactly once per
	 * engine session — CL_FlushMemory does not call it. */
	WiredUI_CompositorInit();

	WiredStore_Init();
	WiredStoreLua_Init();
	/* Single entry point: registers all WiredUI Lua globals (load_menu,
	   attract.*) before WiredScript_PostInit runs them against the VM. */
	WiredUI_LuaInit();

	/* Register the `anim` namespace in System + User VMs.
	 * Must run before WiredScript_PostInit (same window as the other Lua
	 * binding registrars above). */
	WiredAnimLua_Init();

	/* Dev utilities — wui_popup_test / wui_anim_test. Gated by
	 * `developer 1` inside the handlers, so cost is zero for end users. */
	WiredUI_CompositorRegisterDevCommands();

	/* Wired Crosshair: procedural per-weapon reticle. Registers
	 * the q3.* Lua table + queues the crosshair script load; both run at
	 * WiredScript_PostInit alongside the store/anim/menu registrars above. */
	WiredCrosshair_Init();

	/* Cinematic scene: registers the scene.play/stop/skip Lua table (System VM,
	 * same PostInit registrar window). The triggers bridge to the cgame
	 * playback state via the sceneplay/scenestop/sceneskip console commands. */
	WiredScene_LuaInit();

	/* Localization table: registers cl_language + l10n_reload and queues the
	 * scripts/l10n/<lang>.lua key->text load at PostInit (same registrar window).
	 * The caption consumer (cgame) bridges its keys through WiredL10n_Get. */
	WiredL10n_Init();
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

	// Cold-menu master queries run before cgame has published its per-game
	// identity.  PRODUCT_NAME is the packaged product authority at that point;
	// cgame may still replace this ROM cvar from GAMENAME_FOR_MASTER at init.
	Cvar_Get( "cl_gamename", PRODUCT_NAME, CVAR_ROM );


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
	Cmd_AddCommand ("demo_ui", CL_PlayDemoUI_f);
	Cmd_AddCommand( "demo_read_fault", CL_DemoReadFault_f );
	Cmd_AddCommand ("cinematic", CL_PlayCinematic_f);
	Cmd_AddCommand ("stoprecord", CL_StopRecord_f);
	Cmd_AddCommand ("connect", CL_Connect_f);
	Cmd_AddCommand ("reconnect", CL_Reconnect_f);
	/* Spawn a runtime-headless same-process client over the in-memory backend.
	 * The handler is currently an inert stub (spawns nothing); the transport
	 * scaffold to drive an additional in-process client already exists. */
	Cmd_AddCommand ("spawn_headless_client", CL_SpawnHeadlessApp_f);
	Cmd_AddCommand ("rcon_login", CL_RconLogin_f);
	Cmd_AddCommand ("rcon", CL_Rcon_f);
	Cmd_AddCommand ("localservers", CL_LocalServers_f);
	Cmd_AddCommand ("globalservers", CL_GlobalServers_f);
	Cmd_AddCommand ("ping", CL_Ping_f );
	Cmd_AddCommand ("serverstatus", CL_ServerStatus_f );
	Cmd_AddCommand ("showip", CL_ShowIP_f );
	Cmd_AddCommand ("fs_openedList", CL_OpenedPakList_f );
	Cmd_AddCommand ("fs_referencedList", CL_ReferencedPakList_f );
	Cmd_AddCommand ("char", CL_SetChar_f );
	Cmd_AddCommand ("skin", CL_SetSkin_f );
	Cmd_AddCommand ("video", CL_Video_f );
	Cmd_AddCommand ("video-pipe", CL_Video_f );
	Cmd_SetCommandCompletionFunc( "video", CL_CompleteVideoName );
	Cmd_AddCommand ("stopvideo", CL_StopVideo_f );
	Cmd_AddCommand ("serverinfo", CL_Serverinfo_f );
	Cmd_AddCommand ("systeminfo", CL_Systeminfo_f );

	Cmd_AddCommand( "modelist", CL_ModeList_f );
	Cmd_AddCommand( "ral_dump", CL_RalDump_f );          // dump renderer RAL backend probe / caps / memory budget
	Cmd_AddCommand( "ral_pipeline_test", CL_RalPipelineTest_f ); // exact offscreen RAL pipeline exercise
	Cmd_AddCommand( "waitForMap", CL_WaitForMap_f );     // yield Cbuf until map fully loaded
	Cbuf_RegisterWaitForMapCheck( CL_WaitForMap_Ready );

#ifndef NDEBUG
	Cmd_AddCommand( "wui_testerror", CL_WuiTestError_f );
	Cvar_Get( "net_forceSendError", "0", CVAR_TEMP );
#endif

#ifdef _DEBUG
	/* Dev/test loading-backdrop capture gate. Integer = frames to hold the
	 * client at CA_PRIMED (deferring CL_FirstSnapshot in CL_SetCGameTime) so
	 * a +screenshot lands on a held loading frame. 0 = inert. Read by name in
	 * cl_cgame.c. Test-only verify tool, removed in release. */
	Cvar_Get( "debug_hold_loading", "0", CVAR_CHEAT | CVAR_TEMP );
#endif

	Cvar_Set( "cl_running", "1" );
	Cvar_Get( "cl_guid", "", CVAR_USERINFO | CVAR_ROM | CVAR_PROTECTED );
	CL_UpdateGUID( NULL, 0 );

	Com_Log( SEV_INFO, LOG_CH(ch_client), "----- Client Initialization Complete -----\n" );
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

	Com_Log( SEV_INFO, LOG_CH(ch_client), "----- Client Shutdown (%s) -----\n", finalmsg );

	if ( recursive ) {
		Com_Log( SEV_INFO, LOG_CH(ch_client), "WARNING: Recursive CL_Shutdown()\n" );
		return;
	}
	recursive = qtrue;

	noGameRestart = quit;
	CL_Disconnect( clientActiveApp, qfalse );

	// clear and mute all sounds until next registration
	S_DisableSounds();

	CL_ShutdownVMs();
	CL_ProfileTelemetry_Shutdown();

	CL_ShutdownRef( quit ? REF_UNLOAD_DLL : REF_DESTROY_WINDOW );

	Con_ShutdownProjection();
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
	Cmd_RemoveCommand ("demo_ui");
	Cmd_RemoveCommand( "demo_read_fault" );
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


	CL_ClearInput();

#if FEAT_WIRED_UI
	WiredStore_Shutdown();
	WiredL10n_Shutdown();   /* free the plain-C localization table */

	/* Tear down the process-scope
	 * compositor arena + Clay context. CL_Shutdown is the only path that
	 * reaches this — map-load reset (CL_FlushMemory → CL_ShutdownAll) does
	 * not, by design. */
	WiredUI_CompositorShutdown();
#endif

	Cvar_Set( "cl_running", "0" );

	recursive = qfalse;

	CL_ClearServerQueryTransactions();
	memset( &cls, 0, sizeof( cls ) );
	// The per-connection tail (state, servername, cgameBsp, …) moved out of cls
	// into clientApps[0]; the cls memset above no longer reaches it.
	// Zero exactly those fields here to preserve the former wholesale-wipe
	// semantics (clientActiveApp->cl/clientActiveApp->clc are intentionally left untouched, as before).
	CL_SetState( clientActiveApp, CA_UNINITIALIZED );
	clientActiveApp->gameSwitch = qfalse;
	clientActiveApp->servername[0] = '\0';
	clientActiveApp->cgameStarted = qfalse;
	clientActiveApp->startCgame = qfalse;
	clientActiveApp->cgameBsp = NULL;
	clientActiveApp->captureWidth = 0;
	clientActiveApp->captureHeight = 0;
	memset( &clientActiveApp->dlcomplete, 0, sizeof( clientActiveApp->dlcomplete ) );
	Key_SetCatcher( 0 );
	CL_Characters_Shutdown();
	Com_Log( SEV_INFO, LOG_CH(ch_client), "-----------------------\n" );
}


static qboolean CL_SetServerInfo( serverInfo_t *server, const char *info, int ping ) {
	serverInfo_t parsed;
	netadr_t address;
	qboolean visible;

	if ( !server ) return qfalse;
	if ( !info ) {
		server->ping = ping;
		return qtrue;
	}
	if ( !CL_ParseServerInfoResponse( info, com_protocol->integer, &parsed ) ) {
		return qfalse;
	}
	address = server->adr;
	visible = server->visible;
	*server = parsed;
	server->adr = address;
	server->visible = visible;
	server->ping = ping;
	return qtrue;
}


static qboolean CL_SetServerInfoByAddressForOwner( const netadr_t *from,
	const char *info, int ping, clPingOwner_t owner ) {
	qboolean globalChanged = qfalse;
	qboolean changed = qfalse;
	int source;

	if ( !from || !CL_PingOwnerBrowserSource( owner, &source ) ) return qfalse;
	if ( source == AS_LOCAL ) {
		for ( int i = 0; i < cls.numlocalservers; i++ ) {
			if ( NET_CompareAdr( from, &cls.localServers[i].adr ) ) {
				changed |= CL_SetServerInfo( &cls.localServers[i], info, ping );
			}
		}
		return changed;
	}

	if ( source == AS_GLOBAL ) {
		for ( int i = 0; i < cls.numglobalservers; i++ ) {
			if ( NET_CompareAdr( from, &cls.globalServers[i].adr ) ) {
				if ( CL_SetServerInfo( &cls.globalServers[i], info, ping ) ) {
					globalChanged = qtrue;
				}
			}
		}
		if ( globalChanged ) CL_BumpGlobalServerGeneration();
		return globalChanged;
	}

	for ( int i = 0; i < cls.numfavoriteservers; i++ ) {
		if ( NET_CompareAdr( from, &cls.favoriteServers[i].adr ) ) {
			changed |= CL_SetServerInfo( &cls.favoriteServers[i], info, ping );
		}
	}
	return changed;
}

static int CL_RetirePingOwner( clPingOwner_t owner, const char *reason ) {
	int retired = 0;
	int retainedDirect = 0;
	unsigned int now = (unsigned int)Sys_Milliseconds();

	for ( int i = 0; i < ARRAY_LEN( cl_pinglist ); i++ ) {
		ping_t *ping = &cl_pinglist[i];
		if ( !ping->adr.port || ping->owner != owner ) continue;
		Com_Log( SEV_DEBUG, LOG_CH(ch_client),
			"Ping transaction retired slot=%d owner=%s generation=%u reason=%s state=%s age=%ums address=%s\n",
			i, CL_PingOwnerName( ping->owner ), ping->generation, reason,
			ping->time > 0 ? "completed" : "pending", now - ping->start,
			NET_AdrToStringwPort( &ping->adr ) );
		memset( ping, 0, sizeof( *ping ) );
		retired++;
	}
	for ( int i = 0; i < ARRAY_LEN( cl_pinglist ); i++ ) {
		if ( cl_pinglist[i].adr.port
		  && cl_pinglist[i].owner == CL_PING_OWNER_MANUAL ) retainedDirect++;
	}
	Com_Log( SEV_DEBUG, LOG_CH(ch_client),
		"Ping owner refresh owner=%s reason=%s retired=%d retained_direct=%d\n",
		CL_PingOwnerName( owner ), reason, retired, retainedDirect );
	return retired;
}

static int CL_ServerInfoNetType( const netadr_t *from ) {
	if ( !from ) return 0;
	switch ( from->type ) {
		case NA_BROADCAST:
		case NA_IP:
			return 1;
#if FEAT_IPV6
		case NA_IP6:
			return 2;
#endif
		default:
			return 0;
	}
}

static unsigned int CL_ElapsedMilliseconds( unsigned int start ) {
	return (unsigned int)Sys_Milliseconds() - start;
}

_Static_assert( AS_LOCAL == CL_PING_BROWSER_SOURCE_LOCAL,
	"local browser source ABI drift" );
_Static_assert( AS_GLOBAL == CL_PING_BROWSER_SOURCE_GLOBAL,
	"global browser source ABI drift" );
_Static_assert( AS_FAVORITES == CL_PING_BROWSER_SOURCE_FAVORITES,
	"favorites browser source ABI drift" );

static void CL_NewInfoChallenge( char challenge[CL_INFO_CHALLENGE_CHARS + 1],
	unsigned int *generation ) {
	byte random[CL_INFO_CHALLENGE_BYTES];
	unsigned int allocatedGeneration;

	cl_infoChallengeGeneration++;
	if ( cl_infoChallengeGeneration == 0 ) cl_infoChallengeGeneration++;
	allocatedGeneration = cl_infoChallengeGeneration;
	Com_RandomBytes( random, sizeof( random ) );
	/* Some platform RNG fallbacks can repeat within one weak, same-second seed.
	 * Mix the monotonic generation into all bytes without reducing entropy. */
	CL_InfoChallengeDerive( random, allocatedGeneration, challenge );
	if ( generation ) *generation = allocatedGeneration;
}


/*
===================
CL_ServerInfoPacket
===================
*/
static void CL_ServerInfoPacket( const netadr_t *from, msg_t *msg ) {
	char	info[MAX_INFO_STRING];
	serverInfo_t parsed;
	ping_t *pingRequest = NULL;
	ping_t *pingFallback = NULL;
	const char *expectedChallenge;
	unsigned int requestGeneration;
	unsigned int elapsed;
	qboolean localChallengeMatch;

	/* Read the complete packet payload before applying the MAX_INFO_STRING
	 * contract; MSG_ReadString would silently return a valid-looking 1023-byte
	 * prefix for an overlong response. */
	const char *infoString = MSG_ReadBigString( msg );
	if ( strlen( infoString ) >= sizeof( info ) ) {
		Com_Log( SEV_DEBUG, LOG_CH(ch_client), "Rejected overlong infoResponse from %s\n",
			NET_AdrToStringwPort( from ) );
		return;
	}
	Q_strncpyz( info, infoString, sizeof( info ) );

	/* Resolve response ownership by its untrusted-but-correlated token before
	 * falling back to address.  A manual ping and local broadcast can target the
	 * same endpoint concurrently; address-first routing would let either one
	 * shadow the other's response. */
	localChallengeMatch = cl_localDiscovery.active && cl_localDiscovery.challenge[0]
		&& CL_ServerInfoChallengeMatches( info, cl_localDiscovery.challenge );
	for ( int i = 0; i < MAX_PINGREQUESTS; i++ ) {
		if ( cl_pinglist[i].adr.port && !cl_pinglist[i].time
		  && NET_CompareAdr( from, &cl_pinglist[i].adr ) ) {
			if ( !pingFallback ) pingFallback = &cl_pinglist[i];
			if ( CL_ServerInfoChallengeMatches( info, cl_pinglist[i].challenge ) ) {
				pingRequest = &cl_pinglist[i];
				break;
			}
		}
	}
	if ( !pingRequest && !localChallengeMatch ) pingRequest = pingFallback;
	if ( pingRequest ) {
		elapsed = CL_ElapsedMilliseconds( pingRequest->start );
		if ( elapsed >= pingRequest->timeout ) {
			Com_Log( SEV_DEBUG, LOG_CH(ch_client),
				"Ignored expired ping infoResponse generation=%u from %s\n",
				pingRequest->generation, NET_AdrToStringwPort( from ) );
			return;
		}
		expectedChallenge = pingRequest->challenge;
		requestGeneration = pingRequest->generation;
	} else {
		if ( !Sys_IsLANAddress( from ) ) {
			Com_Log( SEV_DEBUG, LOG_CH(ch_client),
				"Ignored non-LAN local discovery infoResponse from %s\n",
				NET_AdrToStringwPort( from ) );
			return;
		}
		elapsed = CL_ElapsedMilliseconds( cl_localDiscovery.start );
		if ( !cl_localDiscovery.active ) {
			Com_Log( SEV_DEBUG, LOG_CH(ch_client),
				"Ignored unsolicited infoResponse from %s\n", NET_AdrToStringwPort( from ) );
			return;
		}
		if ( elapsed >= cl_localDiscovery.timeout ) {
			cl_localDiscovery.active = qfalse;
			Com_Log( SEV_DEBUG, LOG_CH(ch_client),
				"Ignored expired local discovery infoResponse generation=%u elapsed=%ums from %s\n",
				cl_localDiscovery.generation, elapsed, NET_AdrToStringwPort( from ) );
			return;
		}
		expectedChallenge = cl_localDiscovery.challenge;
		requestGeneration = cl_localDiscovery.generation;
	}
	if ( !CL_ServerInfoChallengeMatches( info, expectedChallenge ) ) {
		Com_Log( SEV_DEBUG, LOG_CH(ch_client),
			"Ignored infoResponse challenge mismatch generation=%u from %s\n",
			requestGeneration, NET_AdrToStringwPort( from ) );
		return;
	}
	if ( !Info_SetValueForKey( info, "nettype", va( "%d", CL_ServerInfoNetType( from ) ) ) ) {
		Com_Log( SEV_DEBUG, LOG_CH(ch_client), "Rejected full infoResponse from %s\n",
			NET_AdrToStringwPort( from ) );
		return;
	}

	// Reject the complete untrusted row before it can enter any browser cache.
	if ( !CL_ParseServerInfoResponse( info, com_protocol->integer, &parsed ) ) {
		Com_Log( SEV_DEBUG, LOG_CH(ch_client), "Rejected invalid infoResponse from %s\n",
			NET_AdrToStringwPort( from ) );
		return;
	}

	// iterate servers waiting for ping response
	if ( pingRequest ) {
			// calc ping time
			pingRequest->time = (int)elapsed;
			if ( pingRequest->time < 1 )
			{
				pingRequest->time = 1;
			}
			Com_Log( SEV_DEBUG, LOG_CH(ch_client),
				"Accepted ping infoResponse generation=%u time=%dms address=%s\n",
				pingRequest->generation, pingRequest->time, NET_AdrToStringwPort( from ) );

			// save of info
			Q_strncpyz( pingRequest->info, info, sizeof( pingRequest->info ) );

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
		Com_Log( SEV_DEBUG, LOG_CH(ch_client), "MAX_OTHER_SERVERS hit, dropping infoResponse\n" );
		return;
	}

	// add this to the list
	CL_InitServerInfo( &cls.localServers[i], from );
	if ( elapsed < 1 ) elapsed = 1;
	if ( !CL_SetServerInfo( &cls.localServers[i], info, (int)elapsed ) ) {
		memset( &cls.localServers[i], 0, sizeof( cls.localServers[i] ) );
		return;
	}
	cls.numlocalservers = i+1;
	Com_Log( SEV_INFO, LOG_CH(ch_client),
		"Accepted local discovery infoResponse generation=%u address=%s rtt=%ums name=%s map=%s clients=%d/%d gametype=%d\n",
		cl_localDiscovery.generation,
		NET_AdrToStringwPort( from ), elapsed, cls.localServers[i].hostName,
		cls.localServers[i].mapName, cls.localServers[i].clients,
		cls.localServers[i].maxClients, cls.localServers[i].gameType );
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

static void CL_BeginServerStatusRequest( serverStatus_t *serverStatus,
	const netadr_t *to, qboolean print ) {
	serverStatus->address = *to;
	serverStatus->print = print;
	serverStatus->pending = qtrue;
	serverStatus->retrieved = qfalse;
	serverStatus->startTime = Sys_Milliseconds();
	serverStatus->time = 0;
	Com_sprintf( serverStatus->challenge, sizeof( serverStatus->challenge ),
		"%08x%08x", (unsigned)serverStatus->startTime,
		++cl_serverStatusChallengeSerial );
	NET_OutOfBandPrint( NS_CLIENT, to, "getstatus %s", serverStatus->challenge );
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
			memset( &cl_serverStatusList[i], 0, sizeof( cl_serverStatusList[i] ) );
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
		memset( serverStatus, 0, sizeof( *serverStatus ) );
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
		if ( Sys_Milliseconds() - serverStatus->startTime > cl_serverStatusResendTime->integer ) {
			serverStatus->print		= qfalse;
			serverStatus->pending	= qtrue;
			serverStatus->retrieved = qfalse;
			serverStatus->time		= 0;
			serverStatus->startTime = Sys_Milliseconds();
			NET_OutOfBandPrint( NS_CLIENT, &to, "getstatus %s", serverStatus->challenge );
			return qfalse;
		}
	}
	// if retrieved
	else if ( serverStatus->retrieved ) {
		CL_BeginServerStatusRequest( serverStatus, &to, qfalse );
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
		Com_Log( SEV_DEBUG, LOG_CH(ch_client),
			"CL_ServerStatusResponse: ignored unrequested response from %s\n",
			NET_AdrToStringwPort( from ) );
		return;
	}

	const char *s = MSG_ReadStringLine( msg );
	if ( !serverStatus->pending || !serverStatus->challenge[0]
	     || Q_stricmp( Info_ValueForKey( s, "challenge" ), serverStatus->challenge ) ) {
		Com_Log( SEV_DEBUG, LOG_CH(ch_client),
			"CL_ServerStatusResponse: ignored stale challenge from %s\n",
			NET_AdrToStringwPort( from ) );
		return;
	}

	int len = 0;
	Com_sprintf(&serverStatus->string[len], sizeof(serverStatus->string)-len, "%s", s);

	if (serverStatus->print) {
		Com_Log( SEV_INFO, LOG_CH(ch_client), "Server settings:\n");
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
					Com_Log( SEV_INFO, LOG_CH(ch_client), "%s\n", info);
				}
				else {
					Com_Log( SEV_INFO, LOG_CH(ch_client), "%-24s", info);
				}
			}
		}
	}

	len = strlen(serverStatus->string);
	Com_sprintf(&serverStatus->string[len], sizeof(serverStatus->string)-len, "\\");

	if (serverStatus->print) {
		Com_Log( SEV_INFO, LOG_CH(ch_client), "\nPlayers:\n");
		Com_Log( SEV_INFO, LOG_CH(ch_client), "num: score: ping: name:\n");
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
			Com_Log( SEV_INFO, LOG_CH(ch_client), "%-2d   %-3d    %-3d   %s\n", i, score, ping, s );
		}
	}
	len = strlen(serverStatus->string);
	Com_sprintf(&serverStatus->string[len], sizeof(serverStatus->string)-len, "\\");

	serverStatus->time = Sys_Milliseconds();
	serverStatus->address = *from;
	serverStatus->pending = qfalse;
	Com_Log( SEV_DEBUG, LOG_CH(ch_client),
		"CL_ServerStatusResponse: stored response from %s bytes=%d\n",
		NET_AdrToStringwPort( from ), (int)strlen( serverStatus->string ) );
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
	Com_Log( SEV_INFO, LOG_CH(ch_client), "Scanning for servers on the local network...\n");

	// A new local generation owns only local-browser transactions. Manual,
	// global and favorite requests share capacity but are independent lifecycles.
	CL_RetirePingOwner( CL_PING_OWNER_BROWSER_LOCAL, "local-refresh" );
	// reset the list, waiting for response
	cls.numlocalservers = 0;
	cls.pingUpdateSource = AS_LOCAL;
	CL_NewInfoChallenge( cl_localDiscovery.challenge, &cl_localDiscovery.generation );
	cl_localDiscovery.start = (unsigned int)Sys_Milliseconds();
	cl_localDiscovery.timeout = CL_LOCAL_DISCOVERY_TIMEOUT_MS;
	cl_localDiscovery.active = qtrue;
	Com_Log( SEV_DEBUG, LOG_CH(ch_client),
		"Local discovery request generation=%u challenge=%s timeout=%ums\n",
		cl_localDiscovery.generation, cl_localDiscovery.challenge,
		cl_localDiscovery.timeout );

	for (int i = 0; i < MAX_OTHER_SERVERS; i++) {
		qboolean b = cls.localServers[i].visible;
		memset(&cls.localServers[i], 0, sizeof(cls.localServers[i]));
		cls.localServers[i].visible = b;
	}

	netadr_t to;
	memset( &to, 0, sizeof( to ) );

	// send each message twice in case one is dropped
	for ( int i = 0 ; i < 2 ; i++ ) {
		// send a broadcast packet on each server port
		// we support multiple server ports so a single machine
		// can nicely run multiple servers
		for ( int j = 0 ; j < NUM_SERVER_PORTS ; j++ ) {
			to.port = BigShort( (short)(PORT_SERVER + j) );

			to.type = NA_BROADCAST;
			NET_OutOfBandPrint( NS_CLIENT, &to, "getinfo %s", cl_localDiscovery.challenge );
#if FEAT_IPV6
			to.type = NA_MULTICAST6;
			NET_OutOfBandPrint( NS_CLIENT, &to, "getinfo %s", cl_localDiscovery.challenge );
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
static qboolean CL_MasterDecimalArg( const char *text, int maximum, int *value ) {
	int parsed = 0;
	if ( !text || !text[0] || !value || maximum < 0 ) return qfalse;
	for ( const unsigned char *p = (const unsigned char *)text; *p; p++ ) {
		int digit = *p - '0';
		if ( digit < 0 || digit > 9 || parsed > ( maximum - digit ) / 10 ) return qfalse;
		parsed = parsed * 10 + digit;
	}
	*value = parsed;
	return qtrue;
}

static qboolean CL_MasterTokenSafe( const char *text ) {
	if ( !text || !text[0] ) return qfalse;
	for ( const unsigned char *p = (const unsigned char *)text; *p; p++ ) {
		if ( !( ( *p >= 'a' && *p <= 'z' ) || ( *p >= 'A' && *p <= 'Z' )
		     || ( *p >= '0' && *p <= '9' ) || *p == '_' || *p == '-' || *p == '.' ) ) {
			return qfalse;
		}
	}
	return qtrue;
}

static void CL_GlobalServers_f( void ) {
	masterDiscoverySource_t resolved[MAX_MASTER_SERVERS];
	const char *gamename;
	size_t commandLength;
	int count = Cmd_Argc();
	int masterNum;
	int protocol;
	int resolvedCount = 0;

	if ( count < 3
	  || !CL_MasterDecimalArg( Cmd_Argv( 1 ), MAX_MASTER_SERVERS, &masterNum )
	  || !CL_MasterDecimalArg( Cmd_Argv( 2 ), INT_MAX, &protocol ) || protocol <= 0 ) {
		Com_Log( SEV_INFO, LOG_CH(ch_client), "usage: globalservers <master# 0-%d> <protocol> [keywords]\n", MAX_MASTER_SERVERS );
		return;
	}
	gamename = Cvar_VariableString( "cl_gamename" );
	if ( !CL_MasterTokenSafe( gamename ) ) {
		Com_Log( SEV_WARN, LOG_CH(ch_client),
			"CL_GlobalServers_f: invalid or empty cl_gamename; cannot query master.\n" );
		return;
	}
	for ( int i = 3; i < count; i++ ) {
		if ( !CL_MasterTokenSafe( Cmd_Argv( i ) ) ) {
			Com_Log( SEV_WARN, LOG_CH(ch_client),
				"CL_GlobalServers_f: rejected invalid master keyword.\n" );
			return;
		}
	}
	commandLength = strlen( gamename ) + 32;
	for ( int i = 3; i < count; i++ ) commandLength += strlen( Cmd_Argv( i ) ) + 1;
	if ( commandLength >= 1024 ) {
		Com_Log( SEV_WARN, LOG_CH(ch_client),
			"CL_GlobalServers_f: master query command too long; cache preserved.\n" );
		return;
	}
	{
		size_t commandLength = strlen( "getserversExt " ) + strlen( gamename ) + 1
			+ strlen( Cmd_Argv( 2 ) ) + strlen( " ipv6" );
		for ( int i = 3; i < count; i++ ) commandLength += 1 + strlen( Cmd_Argv( i ) );
		if ( commandLength >= 1024 ) {
			Com_Log( SEV_WARN, LOG_CH(ch_client),
				"CL_GlobalServers_f: rejected overlong master query.\n" );
			return;
		}
	}

	memset( resolved, 0, sizeof( resolved ) );
	for ( int number = masterNum ? masterNum : 1;
	      number <= ( masterNum ? masterNum : MAX_MASTER_SERVERS ); number++ ) {
		char cvarName[32];
		const char *masterAddress;
		netadr_t address;
		int result;
		qboolean duplicate = qfalse;

		Com_sprintf( cvarName, sizeof( cvarName ), "sv_master%d", number );
		masterAddress = Cvar_VariableString( cvarName );
		if ( !masterAddress[0] ) continue;
		memset( &address, 0, sizeof( address ) );
		result = NET_StringToAdr( masterAddress, &address, NA_UNSPEC );
		if ( result == 0 ) {
			Com_Log( SEV_WARN, LOG_CH(ch_client),
				"Master discovery resolve failed slot=%d address=%s\n", number, masterAddress );
			continue;
		}
		if ( result == 2 ) address.port = BigShort( PORT_MASTER );
		if ( address.type != NA_IP
#if FEAT_IPV6
		  && address.type != NA_IP6
#endif
		) {
			Com_Log( SEV_WARN, LOG_CH(ch_client),
				"Master discovery resolve rejected slot=%d address=%s\n", number, masterAddress );
			continue;
		}
		for ( int i = 0; i < resolvedCount; i++ ) {
			if ( NET_CompareAdr( &address, &resolved[i].address ) ) {
				duplicate = qtrue;
				break;
			}
		}
		if ( duplicate ) continue;
		resolved[resolvedCount].address = address;
#if FEAT_IPV6
		resolved[resolvedCount].extended = address.type == NA_IP6;
#else
		resolved[resolvedCount].extended = qfalse;
#endif
		resolvedCount++;
	}
	if ( resolvedCount == 0 ) {
		Com_Log( SEV_WARN, LOG_CH(ch_client),
			"CL_GlobalServers_f: no configured master endpoint resolved; cache preserved.\n" );
		return;
	}

	memset( &cl_masterDiscovery, 0, sizeof( cl_masterDiscovery ) );
	memcpy( cl_masterDiscovery.sources, resolved,
		(size_t)resolvedCount * sizeof( resolved[0] ) );
	cl_masterDiscovery.sourceCount = resolvedCount;
	cl_masterDiscoveryGeneration++;
	if ( cl_masterDiscoveryGeneration == 0 ) cl_masterDiscoveryGeneration++;
	cl_masterDiscovery.generation = cl_masterDiscoveryGeneration;
	cl_masterDiscovery.start = (unsigned int)Sys_Milliseconds();
	cl_masterDiscovery.timeout = CL_MASTER_DISCOVERY_TIMEOUT_MS;
	cl_masterDiscovery.active = qtrue;

	memset( cls.globalServers, 0, sizeof( cls.globalServers ) );
	memset( cls.globalServerAddresses, 0, sizeof( cls.globalServerAddresses ) );
	CL_RetirePingOwner( CL_PING_OWNER_BROWSER_GLOBAL, "global-refresh" );
	cls.numglobalservers = 0;
	cls.numGlobalServerAddresses = 0;
	cls.pingUpdateSource = AS_GLOBAL;
	hash_reset();
	CL_BumpGlobalServerGeneration();
	Com_Log( SEV_INFO, LOG_CH(ch_client),
		"Master discovery query generation=%u sources=%d protocol=%d gamename=%s timeout=%ums\n",
		cl_masterDiscovery.generation, resolvedCount, protocol, gamename,
		cl_masterDiscovery.timeout );

	for ( int source = 0; source < resolvedCount; source++ ) {
		char command[1024];
		qstring_t commandString;
		if ( resolved[source].extended ) {
			Com_sprintf( command, sizeof( command ), "getserversExt %s %d", gamename, protocol );
		} else {
			Com_sprintf( command, sizeof( command ), "getservers %s %d", gamename, protocol );
		}
		commandString = QS_WrapExisting( command, sizeof( command ) );
#if FEAT_IPV6
		if ( resolved[source].extended
		  && !( Cvar_VariableIntegerValue( "net_enabled" ) & NET_ENABLEV4 ) ) {
			QS_Append( &commandString, " ipv6" );
		}
#endif
		Com_Log( SEV_INFO, LOG_CH(ch_client),
			"Master query generation=%u address=%s extended=%d timeout=%ums\n",
			cl_masterDiscovery.generation,
			NET_AdrToStringwPort( &resolved[source].address ),
			resolved[source].extended ? 1 : 0, cl_masterDiscovery.timeout );
		for ( int i = 3; i < count; i++ ) {
			QS_AppendChar( &commandString, ' ' );
			QS_Append( &commandString, Cmd_Argv( i ) );
		}
		Com_Log( SEV_INFO, LOG_CH(ch_client),
			"Master discovery send generation=%u source_index=%d endpoint=%s kind=%s command=%s\n",
			cl_masterDiscovery.generation, source,
			NET_AdrToStringwPort( &resolved[source].address ),
			resolved[source].extended ? "extended" : "classic", command );
		/* This is a client browser transaction.  Using NS_SERVER routes the
		 * master's reply back to the server socket, where a GUI-only client never
		 * feeds it through CL_ConnectionlessPacket. */
		NET_OutOfBandPrint( NS_CLIENT, &resolved[source].address, "%s", command );
	}
}


/*
==================
CL_GetPing
==================
*/
static void CL_GetPingForConsumer( int n, char *buf, int buflen, int *pingtime,
	const char *consumer )
{
	const char *outcome = NULL;
	const char *cacheAction = "none";
	qboolean cacheMatched = qfalse;
	clPingCacheAction_t requestedCacheAction;
	ping_t *ping;
	unsigned int elapsed;

	if (n < 0 || n >= MAX_PINGREQUESTS || !cl_pinglist[n].adr.port)
	{
		// empty or invalid slot
		buf[0]    = '\0';
		*pingtime = 0;
		return;
	}
	ping = &cl_pinglist[n];

	const char *str = NET_AdrToStringwPort( &ping->adr );
	Q_strncpyz( buf, str, buflen );

	int time = ping->time;
	if ( time == 0 )
	{
		// check for timeout
		elapsed = CL_ElapsedMilliseconds( ping->start );
		if ( elapsed < ping->timeout )
		{
			// not timed out yet
			time = 0;
		} else {
			time = (int)elapsed;
			outcome = "expired";
			requestedCacheAction = CL_PingOwnerTerminalCacheAction(
				ping->owner, false );
			if ( requestedCacheAction == CL_PING_CACHE_CLEAR ) {
				cacheMatched = CL_SetServerInfoByAddressForOwner(
					&ping->adr, NULL, 0, ping->owner );
				cacheAction = "clear";
			}
		}
	} else {
		outcome = "completed";
		requestedCacheAction = CL_PingOwnerTerminalCacheAction( ping->owner, true );
		if ( requestedCacheAction == CL_PING_CACHE_PUBLISH ) {
			cacheMatched = CL_SetServerInfoByAddressForOwner( &ping->adr,
				ping->info, ping->time, ping->owner );
			cacheAction = "publish";
		}
	}

	*pingtime = time;
	if ( outcome ) {
		Com_Log( SEV_DEBUG, LOG_CH(ch_client),
			"Ping result consumed slot=%d owner=%s generation=%u outcome=%s time=%dms cache_action=%s matched=%d consumer=%s address=%s\n",
			n, CL_PingOwnerName( ping->owner ), ping->generation, outcome,
			time, cacheAction, cacheMatched ? 1 : 0, consumer,
			NET_AdrToStringwPort( &ping->adr ) );
	}
}

void CL_GetPing( int n, char *buf, int buflen, int *pingtime ) {
	CL_GetPingForConsumer( n, buf, buflen, pingtime, "public" );
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
static qboolean CL_ClearPingIdentity( int n, clPingOwner_t expectedOwner,
	unsigned int expectedGeneration )
{
	ping_t *ping;
	unsigned int age;
	const char *state;

	if (n < 0 || n >= MAX_PINGREQUESTS)
		return qfalse;
	ping = &cl_pinglist[n];
	if ( !ping->adr.port || !CL_PingIdentityMatches( ping->owner, ping->generation,
		expectedOwner, expectedGeneration ) ) return qfalse;
	age = CL_ElapsedMilliseconds( ping->start );
	state = ping->time > 0 ? "completed"
		: age >= ping->timeout ? "expired" : "pending";
	Com_Log( SEV_DEBUG, LOG_CH(ch_client),
		"Ping transaction cleared slot=%d owner=%s generation=%u state=%s age=%ums address=%s\n",
		n, CL_PingOwnerName( ping->owner ), ping->generation, state, age,
		NET_AdrToStringwPort( &ping->adr ) );

	memset( ping, 0, sizeof( *ping ) );
	return qtrue;
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
	unsigned int msec = (unsigned int)Sys_Milliseconds();
	clPingQueueEntry_t entries[ARRAY_LEN( cl_pinglist )];
	clPingQueueSelection_t selection;
	ping_t *pingptr;
	const char *reason;
	const char *state;
	char address[MAX_STRING_CHARS];

	for ( int i = 0; i < ARRAY_LEN( cl_pinglist ); i++ ) {
		if ( !CL_PingQueueDescribe( cl_pinglist[i].adr.port != 0,
			cl_pinglist[i].start, cl_pinglist[i].timeout, cl_pinglist[i].time,
			&entries[i] ) ) {
			Com_Log( SEV_WARN, LOG_CH(ch_client),
				"Ping queue allocation failed reason=invalid-result-time slot=%d\n", i );
			return NULL;
		}
	}
	if ( !CL_PingQueueSelect( entries, ARRAY_LEN( entries ), msec, &selection ) ) {
		Com_Log( SEV_WARN, LOG_CH(ch_client),
			"Ping queue allocation failed reason=invalid-state\n" );
		return NULL;
	}

	pingptr = &cl_pinglist[selection.index];
	Q_strncpyz( address, pingptr->adr.port
		? NET_AdrToStringwPort( &pingptr->adr ) : "none", sizeof( address ) );
	switch ( selection.reason ) {
	case CL_PING_QUEUE_REUSE_FREE:
		reason = "free";
		state = "empty";
		break;
	case CL_PING_QUEUE_REUSE_EXPIRED:
		reason = "expired";
		state = "pending";
		break;
	case CL_PING_QUEUE_REUSE_COMPLETED:
		reason = "completed";
		state = "completed";
		break;
	case CL_PING_QUEUE_REUSE_OLDEST_PENDING:
	default:
		reason = "oldest-pending";
		state = "pending";
		break;
	}
	Com_Log( SEV_DEBUG, LOG_CH(ch_client),
		"Ping queue allocation slot=%u reason=%s previous_generation=%u previous_state=%s age=%ums address=%s\n",
		(unsigned int)selection.index, reason, pingptr->generation, state,
		selection.age, address );
	if ( selection.reason == CL_PING_QUEUE_REUSE_EXPIRED
	  || selection.reason == CL_PING_QUEUE_REUSE_OLDEST_PENDING ) {
		CL_SetServerInfoByAddressForOwner( &pingptr->adr, NULL, 0, pingptr->owner );
	} else if ( selection.reason == CL_PING_QUEUE_REUSE_COMPLETED
	       && CL_PingOwnerTerminalCacheAction( pingptr->owner, true )
	          == CL_PING_CACHE_PUBLISH ) {
		qboolean matched = CL_SetServerInfoByAddressForOwner( &pingptr->adr,
			pingptr->info, pingptr->time, pingptr->owner );
		Com_Log( SEV_DEBUG, LOG_CH(ch_client),
			"Ping result consumed slot=%u owner=%s generation=%u outcome=completed time=%dms cache_action=publish matched=%d consumer=capacity address=%s\n",
			(unsigned int)selection.index, CL_PingOwnerName( pingptr->owner ),
			pingptr->generation, pingptr->time, matched ? 1 : 0, address );
	}
	if ( pingptr->adr.port ) {
		Com_Log( SEV_DEBUG, LOG_CH(ch_client),
			"Ping transaction retired slot=%u owner=%s generation=%u reason=capacity-%s state=%s age=%ums address=%s\n",
			(unsigned int)selection.index, CL_PingOwnerName( pingptr->owner ),
			pingptr->generation, reason, state, selection.age, address );
	}
	memset( pingptr, 0, sizeof( *pingptr ) );
	return pingptr;
}

static void CL_BeginPingRequest( ping_t *ping, const netadr_t *address,
	clPingOwner_t owner ) {
	if ( !ping || !address
	  || ( owner != CL_PING_OWNER_MANUAL && !CL_PingOwnerIsBrowser( owner ) ) ) return;
	memset( ping, 0, sizeof( *ping ) );
	ping->adr = *address;
	ping->owner = owner;
	ping->start = (unsigned int)Sys_Milliseconds();
	ping->timeout = (unsigned int)Cvar_VariableIntegerValue( "cl_maxPing" );
	CL_NewInfoChallenge( ping->challenge, &ping->generation );
	NET_OutOfBandPrint( NS_CLIENT, &ping->adr, "getinfo %s", ping->challenge );
	Com_Log( SEV_DEBUG, LOG_CH(ch_client),
		"Ping transaction started owner=%s generation=%u address=%s\n",
		CL_PingOwnerName( ping->owner ), ping->generation,
		NET_AdrToStringwPort( &ping->adr ) );
	Com_Log( SEV_DEBUG, LOG_CH(ch_client),
		"Ping request generation=%u challenge=%s timeout=%ums address=%s\n",
		ping->generation, ping->challenge, ping->timeout,
		NET_AdrToStringwPort( &ping->adr ) );
}

static qboolean CL_ReapTerminalBrowserPings( clPingOwner_t activeOwner ) {
	qboolean activeReaped = qfalse;

	for ( int i = 0; i < MAX_PINGREQUESTS; i++ ) {
		clPingOwner_t expectedOwner;
		unsigned int expectedGeneration;
		char buff[MAX_STRING_CHARS];
		int pingTime;
		ping_t *ping = &cl_pinglist[i];
		if ( !ping->adr.port || !CL_PingOwnerIsBrowser( ping->owner ) ) continue;
		if ( ping->time == 0
		  && CL_ElapsedMilliseconds( ping->start ) < ping->timeout ) continue;
		expectedOwner = ping->owner;
		expectedGeneration = ping->generation;
		CL_GetPingForConsumer( i, buff, sizeof( buff ), &pingTime,
			ping->owner == activeOwner ? "current" : "offscreen" );
		if ( pingTime != 0
		  && CL_ClearPingIdentity( i, expectedOwner, expectedGeneration ) ) {
			if ( expectedOwner == activeOwner ) activeReaped = qtrue;
		}
	}
	return activeReaped;
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
		Com_Log( SEV_INFO, LOG_CH(ch_client), "usage: ping [-4|-6] <server>\n");
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
			Com_Log( SEV_INFO, LOG_CH(ch_client), "warning: only -4 or -6 as address type understood.\n" );
#else
		else
			Com_Log( SEV_INFO, LOG_CH(ch_client), "warning: only -4 as address type understood.\n" );
#endif

		server = Cmd_Argv(2);
	}

	netadr_t to;
	memset( &to, 0, sizeof( to ) );

	if ( !NET_StringToAdr( server, &to, family ) ) {
		return;
	}

	ping_t* pingptr = CL_GetFreePing();
	if ( pingptr ) CL_BeginPingRequest( pingptr, &to, CL_PING_OWNER_MANUAL );
}


/*
==================
CL_UpdateVisiblePings_f
==================
*/
qboolean CL_UpdateVisiblePings_f(int source) {
	qboolean status = qfalse;
	qboolean currentOwnerWork = qfalse;
	clPingOwner_t owner;

	if ( !CL_PingOwnerFromBrowserSource( source, &owner ) ) {
		return qfalse;
	}

	cls.pingUpdateSource = source;

	/* Every browser terminal result has a source-specific cache destination, so
	 * it may be reaped before scheduling the active source without cross-tab
	 * mutation. Direct requests and live browser transactions remain shared
	 * pressure and are never consumed here. */
	status |= CL_ReapTerminalBrowserPings( owner );

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
						Com_Log( SEV_DEBUG, LOG_CH(ch_client),
							"Ping browser scheduling deferred owner=%s reason=capacity-full slots=%d address=%s\n",
							CL_PingOwnerName( owner ), slots,
							NET_AdrToStringwPort( &server[i].adr ) );
						break;
					}
					for (j = 0; j < MAX_PINGREQUESTS; j++) {
						if (!cl_pinglist[j].adr.port) {
							continue;
						}
						if ( cl_pinglist[j].owner == owner
						  && NET_CompareAdr( &cl_pinglist[j].adr, &server[i].adr ) ) {
							// already on the list
							currentOwnerWork = qtrue;
							break;
						}
					}
					if (j >= MAX_PINGREQUESTS) {
						status = qtrue;
						for (j = 0; j < MAX_PINGREQUESTS; j++) {
							if (!cl_pinglist[j].adr.port) {
								CL_BeginPingRequest( &cl_pinglist[j], &server[i].adr, owner );
								slots++;
								currentOwnerWork = qtrue;
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

	if ( currentOwnerWork ) status = qtrue;

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
		if (clientActiveApp->state != CA_ACTIVE || clientActiveApp->clc.demoplaying)
		{
			Com_Log( SEV_INFO, LOG_CH(ch_client), "Not connected to a server.\n" );
#if FEAT_IPV6
			Com_Log( SEV_INFO, LOG_CH(ch_client), "usage: serverstatus [-4|-6] <server>\n" );
#else
			Com_Log( SEV_INFO, LOG_CH(ch_client), "usage: serverstatus <server>\n");
#endif
			return;
		}

		toptr = &clientActiveApp->clc.serverAddress;
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
				Com_Log( SEV_INFO, LOG_CH(ch_client), "warning: only -4 or -6 as address type understood.\n" );
#else
			else
				Com_Log( SEV_INFO, LOG_CH(ch_client), "warning: only -4 as address type understood.\n" );
#endif

			server = Cmd_Argv(2);
		}

		toptr = &to;
		if ( !NET_StringToAdr( server, toptr, family ) )
			return;
	}

	serverStatus_t *serverStatus = CL_GetServerStatus( toptr );
	CL_BeginServerStatusRequest( serverStatus, toptr, qtrue );
}


/*
==================
CL_ShowIP_f
==================
*/
static void CL_ShowIP_f( void ) {
	Sys_ShowIP();
}
