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
// cl_cgame.c  -- client system interaction with client game

#include "client.h"
#include "wired/hud/cl_wired_hud.h"
#include "wired/ui/cl_wired_text.h"
#include "wired/store/cl_wired_store.h"

#include "../botlib/botlib.h"
/* Phase 5: log channels */
LOG_DECLARE_CHANNEL( ch_cgame, "cgame" );
LOG_DECLARE_CHANNEL( ch_client, "client" );

extern	botlib_export_t	*botlib_export;

//extern qboolean loadCamera(const char *name);
//extern void startCamera(int time);
//extern qboolean getCameraInfo(int time, vec3_t *origin, vec3_t *angles);

/*
====================
CL_GetGameState
====================
*/
static void CL_GetGameState( gameState_t *gs ) {
	*gs = cl.gameState;
}


/*
====================
CL_GetGlconfig
====================
*/
static void CL_GetGlconfig( glconfig_t *glconfig ) {
	*glconfig = cls.glconfig;
}


/*
====================
CL_GetUserCmd
====================
*/
static qboolean CL_GetUserCmd( int cmdNumber, usercmd_t *ucmd ) {
	// cmds[cmdNumber] is the last properly generated command

	// can't return anything that we haven't created yet
	if ( cl.cmdNumber - cmdNumber < 0 ) {
		Com_Terminate( TERM_CLIENT_DROP, "CL_GetUserCmd: cmdNumber (%i) > cl.cmdNumber (%i)", cmdNumber, cl.cmdNumber );
	}

	// the usercmd has been overwritten in the wrapping
	// buffer because it is too far out of date
	if ( cl.cmdNumber - cmdNumber >= CMD_BACKUP ) {
		return qfalse;
	}

	*ucmd = cl.cmds[ cmdNumber & CMD_MASK ];

	return qtrue;
}


/*
====================
CL_GetCurrentCmdNumber
====================
*/
static int CL_GetCurrentCmdNumber( void ) {
	return cl.cmdNumber;
}


/*
====================
CL_GetCurrentSnapshotNumber
====================
*/
static void CL_GetCurrentSnapshotNumber( int *snapshotNumber, int *serverTime ) {
	*snapshotNumber = cl.snap.messageNum;
	*serverTime = cl.snap.serverTime;
}


/*
====================
CL_GetSnapshot
====================
*/
static qboolean CL_GetSnapshot( int snapshotNumber, snapshot_t *snapshot ) {
	clSnapshot_t	*clSnap;

	if ( cl.snap.messageNum - snapshotNumber < 0 ) {
		Com_Terminate( TERM_CLIENT_DROP, "CL_GetSnapshot: snapshotNumber (%i) > cl.snapshot.messageNum (%i)", snapshotNumber, cl.snap.messageNum );
	}

	// if the frame has fallen out of the circular buffer, we can't return it
	if ( cl.snap.messageNum - snapshotNumber >= PACKET_BACKUP ) {
		return qfalse;
	}

	// if the frame is not valid, we can't return it
	clSnap = &cl.snapshots[snapshotNumber & PACKET_MASK];
	if ( !clSnap->valid ) {
		return qfalse;
	}

	// if the entities in the frame have fallen out of their
	// circular buffer, we can't return it
	if ( cl.parseEntitiesNum - clSnap->parseEntitiesNum >= MAX_PARSE_ENTITIES ) {
		return qfalse;
	}

	// write the snapshot
	snapshot->snapFlags = clSnap->snapFlags;
	snapshot->serverCommandSequence = clSnap->serverCommandNum;
	snapshot->ping = clSnap->ping;
	snapshot->serverTime = clSnap->serverTime;
	memcpy( snapshot->areamask, clSnap->areamask, sizeof( snapshot->areamask ) );
	snapshot->ps = clSnap->ps;
	int count = clSnap->numEntities;
	if ( count > MAX_ENTITIES_IN_SNAPSHOT ) {
		Com_Log( SEV_DEBUG, LOG_CH(ch_client), "CL_GetSnapshot: truncated %i entities to %i\n", count, MAX_ENTITIES_IN_SNAPSHOT );
		count = MAX_ENTITIES_IN_SNAPSHOT;
	}
	snapshot->numEntities = count;
	for ( int i = 0 ; i < count ; i++ ) {
		snapshot->entities[i] =
			cl.parseEntities[ ( clSnap->parseEntitiesNum + i ) & (MAX_PARSE_ENTITIES-1) ];
	}

	// FIXME: configstring changes and server commands!!!

	return qtrue;
}


/*
=====================
CL_SetUserCmdValue
=====================
*/
static void CL_SetUserCmdValue( int userCmdValue, float sensitivityScale ) {
	cl.cgameUserCmdValue = userCmdValue;
	cl.cgameSensitivity = sensitivityScale;
}


/*
=====================
CL_AddCgameCommand
=====================
*/
static void CL_AddCgameCommand( const char *cmdName ) {
	Cmd_AddCgameCommand( cmdName );
}


/*
=====================
CL_ConfigstringModified
=====================
*/
static void CL_ConfigstringModified( void ) {
	const char	*old, *s;
	const char	*dup;
	gameState_t	oldGs;

	int index = atoi( Cmd_Argv(1) );
	if ( (unsigned) index >= MAX_CONFIGSTRINGS ) {
		Com_Terminate( TERM_CLIENT_DROP, "%s: bad configstring index %i", __func__, index );
	}
	// get everything after "cs <num>"
	s = Cmd_ArgsFrom(2);

	old = cl.gameState.stringData + cl.gameState.stringOffsets[ index ];
	if ( !strcmp( old, s ) ) {
		return;		// unchanged
	}

	// build the new gameState_t
	oldGs = cl.gameState;

	memset( &cl.gameState, 0, sizeof( cl.gameState ) );

	// leave the first 0 for uninitialized strings
	cl.gameState.dataCount = 1;

	for ( int i = 0; i < MAX_CONFIGSTRINGS; i++ ) {
		if ( i == index ) {
			dup = s;
		} else {
			dup = oldGs.stringData + oldGs.stringOffsets[ i ];
		}
		if ( !dup[0] ) {
			continue;		// leave with the default empty string
		}

		int len = strlen( dup );

		if ( len + 1 + cl.gameState.dataCount > MAX_GAMESTATE_CHARS ) {
			Com_Terminate( TERM_CLIENT_DROP, "%s: MAX_GAMESTATE_CHARS exceeded", __func__ );
		}

		// append it to the gameState string buffer
		cl.gameState.stringOffsets[ i ] = cl.gameState.dataCount;
		memcpy( cl.gameState.stringData + cl.gameState.dataCount, dup, len + 1 );
		cl.gameState.dataCount += len + 1;
	}

	if ( index == CS_SYSTEMINFO ) {
		// parse serverId and other cvars
		CL_SystemInfoChanged( qfalse );
	}
}


/*
===================
CL_GetServerCommand

Set up argc/argv for the given command
===================
*/
static qboolean CL_GetServerCommand( int serverCommandNumber ) {
	const char *s;
	const char *cmd;
	static char bigConfigString[BIG_INFO_STRING];
	int argc, index;

	// if we have irretrievably lost a reliable command, drop the connection
	if ( clc.serverCommandSequence - serverCommandNumber >= MAX_RELIABLE_COMMANDS ) {
		// when a demo record was started after the client got a whole bunch of
		// reliable commands then the client never got those first reliable commands
		if ( clc.demoplaying ) {
			Cmd_Clear();
			return qfalse;
		}
		Com_Terminate( TERM_CLIENT_DROP, "CL_GetServerCommand: a reliable command was cycled out" );
		return qfalse;
	}

	if ( clc.serverCommandSequence - serverCommandNumber < 0 ) {
		Com_Terminate( TERM_CLIENT_DROP, "CL_GetServerCommand: requested a command not received" );
		return qfalse;
	}

	index = serverCommandNumber & ( MAX_RELIABLE_COMMANDS - 1 );
	s = clc.serverCommands[ index ];
	clc.lastExecutedServerCommand = serverCommandNumber;

	if ( clc.serverCommandsIgnore[ index ] ) {
		Cmd_Clear();
		return qfalse;
	}

rescan:
	Cmd_TokenizeString( s );
	cmd = Cmd_Argv(0);
	argc = Cmd_Argc();

	if ( !strcmp( cmd, "disconnect" ) ) {
		// https://zerowing.idsoftware.com/bugzilla/show_bug.cgi?id=552
		// allow server to indicate why they were disconnected
		if ( argc >= 2 )
			Com_Terminate( TERM_SERVER_KICK, "Server disconnected - %s", Cmd_Argv( 1 ) );
		else
			Com_Terminate( TERM_SERVER_KICK, "Server disconnected" );
	}

	if ( !strcmp( cmd, "bcs0" ) ) {
		Com_sprintf( bigConfigString, BIG_INFO_STRING, "cs %s \"%s", Cmd_Argv(1), Cmd_Argv(2) );
		return qfalse;
	}

	if ( !strcmp( cmd, "bcs1" ) ) {
		s = Cmd_Argv(2);
		if( strlen(bigConfigString) + strlen(s) >= BIG_INFO_STRING ) {
			Com_Terminate( TERM_CLIENT_DROP, "bcs exceeded BIG_INFO_STRING" );
		}
		strcat( bigConfigString, s );
		return qfalse;
	}

	if ( !strcmp( cmd, "bcs2" ) ) {
		s = Cmd_Argv(2);
		if( strlen(bigConfigString) + strlen(s) + 1 >= BIG_INFO_STRING ) {
			Com_Terminate( TERM_CLIENT_DROP, "bcs exceeded BIG_INFO_STRING" );
		}
		strcat( bigConfigString, s );
		strcat( bigConfigString, "\"" );
		s = bigConfigString;
		goto rescan;
	}

	if ( !strcmp( cmd, "cs" ) ) {
		CL_ConfigstringModified();
		// reparse the string, because CL_ConfigstringModified may have done another Cmd_TokenizeString()
		Cmd_TokenizeString( s );
		return qtrue;
	}

	if ( !strcmp( cmd, "map_restart" ) ) {
		// clear notify lines and outgoing commands before passing
		// the restart to the cgame
		Con_ClearNotify();
		// reparse the string, because Con_ClearNotify() may have done another Cmd_TokenizeString()
		Cmd_TokenizeString( s );
		memset( cl.cmds, 0, sizeof( cl.cmds ) );
		cls.lastVidRestart = Sys_Milliseconds(); // vid_restart hack
		return qtrue;
	}

	// the clientLevelShot command is used during development
	// to generate 128*128 screenshots from the intermission
	// point of levels for the menu system to use
	// we pass it along to the cgame to make appropriate adjustments,
	// but we also clear the console and notify lines here
	if ( !strcmp( cmd, "clientLevelShot" ) ) {
		// don't do it if we aren't running the server locally,
		// otherwise malicious remote servers could overwrite
		// the existing thumbnails
		if ( !com_sv_running->integer ) {
			return qfalse;
		}
		// close the console
		Con_Close();
		// take a special screenshot next frame
		Cbuf_AddText( "wait ; wait ; wait ; wait ; screenshot levelshot\n" );
		return qtrue;
	}

	// we may want to put a "connect to other server" command here

	// cgame can now act on the command
	return qtrue;
}


/*
====================
CL_CM_LoadMap

Just adds default parameters that cgame doesn't need to know about
====================
*/
static void CL_CM_LoadMap( const char *mapname ) {
	int		checksum;
	bspFile_t	*bsp;

	CM_LoadMap( mapname, qtrue, &checksum );

	if ( cls.cgameBsp ) {
		BSP_Free( cls.cgameBsp );
		cls.cgameBsp = NULL;
	}

	if ( !BSP_Load( mapname, &bsp, BSP_LOAD_FLAGS_NONE ) ) {
		Com_Terminate( TERM_CLIENT_DROP, "%s: couldn't load %s", __func__, mapname );
	}
	cls.cgameBsp = bsp;
}


/*
====================
CL_ShutdonwCGame

====================
*/
void CL_ShutdownCGame( void ) {

	Key_SetCatcher( Key_GetCatcher( ) & ~KEYCATCH_CGAME );
	cls.cgameStarted = qfalse;

	if ( !cgvm ) {
		return;
	}

	re.VertexLighting( qfalse );

	VM_Call( cgvm, 0, CG_SHUTDOWN );
	VM_Free( cgvm );
	cgvm = NULL;
	FS_VM_CloseFiles( H_CGAME );

	// Remove commands that cgame registered via CG_ADDCOMMAND so that
	// they stop showing up in auto-completion / console help once the
	// cgame QVM is gone. These are tracked internally as "function==NULL"
	// stub entries installed by CL_AddCgameCommand.
	Cmd_RemoveCgameCommands();

	if ( cls.cgameBsp ) {
		BSP_Free( cls.cgameBsp );
		cls.cgameBsp = NULL;
	}
}


static int FloatAsInt( float f ) {
	floatint_t fi;
	fi.f = f;
	return fi.i;
}


static void *VM_ArgPtr( intptr_t intValue ) {

	if ( !intValue || cgvm == NULL )
	  return NULL;

	if ( cgvm->entryPoint )
		return (void *)(intValue);
	return (void *)( cgvm->dataBase + ( intValue & cgvm->dataMask ) );
}


static qboolean CL_GetValue( char* value, int valueSize, const char* key ) {

	if ( !Q_stricmp( key, "trap_R_AddRefEntityToScene2" ) ) {
		Com_sprintf( value, valueSize, "%i", CG_R_ADDREFENTITYTOSCENE2 );
		return qtrue;
	}

	if ( !Q_stricmp( key, "trap_R_ForceFixedDLights" ) ) {
		Com_sprintf( value, valueSize, "%i", CG_R_FORCEFIXEDDLIGHTS );
		return qtrue;
	}

	if ( !Q_stricmp( key, "trap_R_AddLinearLightToScene_Q3E" ) && re.AddLinearLightToScene ) {
		Com_sprintf( value, valueSize, "%i", CG_R_ADDLINEARLIGHTTOSCENE );
		return qtrue;
	}

	if ( !Q_stricmp( key, "trap_IsRecordingDemo" ) ) {
		Com_sprintf( value, valueSize, "%i", CG_IS_RECORDING_DEMO );
		return qtrue;
	}

	if ( !Q_stricmp( key, "trap_Cvar_SetDescription_Q3E" ) ) {
		Com_sprintf( value, valueSize, "%i", CG_CVAR_SETDESCRIPTION );
		return qtrue;
	}

	if ( strncmp( key, "char:", 5 ) == 0 ) {
		return CL_Characters_GetManifest( key + 5, value, valueSize );
	}

	return qfalse;
}


static void CL_ForceFixedDlights( void ) {
	cvar_t *cv;

	cv = Cvar_Get( "r_dlightMode", "1", 0 );
	if ( cv ) {
		Cvar_CheckRange( cv, "1", "2", CV_INTEGER );
	}
}


// Frametime tracking for loading yields — advances console animation during CA_LOADING.
static int cl_loadYieldLastTime;

/*
====================
CL_LoadingYield

Cooperative yield during loading — renders a loading screen frame
and pumps OS events to keep the application responsive.
Must ONLY be called between discrete, safe operations (never mid-shader
or mid-texture-upload).
====================
*/
void CL_LoadingYield( const char *phaseName ) {
	cl_loadProgress.phase = phaseName;
	// Recompute weighted overall: geometry 25%, shaders 40%, audio 15%, download 20%
	cl_loadProgress.overall =
		cl_loadProgress.geometry * 0.25f +
		cl_loadProgress.shaders * 0.40f +
		cl_loadProgress.audio   * 0.15f +
		cl_loadProgress.download * 0.20f;

	// Advance realFrametime so console slide animation ticks during loading.
	// CL_Frame is not running here, so this is the only update path.
	// Does NOT touch cls.realtime or cls.frametime — those are CL_Frame's accumulators.
	{
		int now = Sys_Milliseconds();
		cls.realFrametime = now - cl_loadYieldLastTime;
		cl_loadYieldLastTime = now;
	}

	SCR_UpdateScreen();

	// Drain OS events and dispatch them. CL_MapLoading sets only KEYCATCH_CONSOLE
	// before entering CA_LOADING, so dispatch goes exclusively through Console_Key /
	// CL_CharEvent. KEYCATCH_CGAME and KEYCATCH_UI are both 0 — no VM reentry paths
	// are reachable. ESC with console closed hits cls.state != CA_DISCONNECTED and
	// calls CL_Disconnect, cancelling the load as expected.
#ifdef _DEBUG
	assert( !( Key_GetCatcher() & KEYCATCH_CGAME ) );
	assert( !( Key_GetCatcher() & KEYCATCH_UI ) );
#endif
	Sys_SendKeyEvents();
	Com_EventLoop();
}


// Loading yield counters — track registrations during CG_INIT for cooperative yielding
static int loadYield_modelCount;
static int loadYield_shaderCount;
static int loadYield_soundCount;

#define LOADING_YIELD_MODELS   20   // yield every N model registrations
#define LOADING_YIELD_SHADERS  50   // yield every N shader registrations
#define LOADING_YIELD_SOUNDS   30   // yield every N sound registrations

// Estimated totals for progress computation (approximate — exact counts not critical)
#define LOADING_EST_MODELS    100
#define LOADING_EST_SHADERS   200
#define LOADING_EST_SOUNDS    100


/*
====================
CL_CgameSystemCalls

The cgame module is making a system call
====================
*/
static intptr_t CL_CgameSystemCalls( intptr_t *args ) {
	switch( args[0] ) {
	case CG_PRINT:
		Com_Log( SEV_INFO, LOG_CH(ch_cgame), "%s", (const char*)VMA(1) );
		return 0;
	case CG_ERROR:
		Com_Terminate( TERM_CLIENT_DROP, "%s", (const char*)VMA(1) );
		return 0;
	case CG_LOG:
		Com_Log( (log_severity_t)args[1], LOG_CH(ch_cgame), "%s", (const char*)VMA(2) );
		return 0;
	case CG_TERMINATE:
		Com_Terminate( (terminationReason_t)args[1], "%s", (const char*)VMA(2) );
		return 0;
	case CG_MILLISECONDS:
		return Sys_Milliseconds();
	case CG_CVAR_REGISTER:
		Cvar_VM_Register( VMA(1), VMA(2), VMA(3), args[4], cgvm->privateFlag );
		return 0;
	case CG_CVAR_UPDATE:
		Cvar_Update( VMA(1), cgvm->privateFlag );
		return 0;
	case CG_CVAR_SET:
		Cvar_SetSafe( VMA(1), VMA(2) );
		return 0;
	case CG_CVAR_VARIABLESTRINGBUFFER:
		VM_CHECKBOUNDS( cgvm, args[2], args[3] );
		Cvar_VariableStringBufferSafe( VMA(1), VMA(2), args[3], CVAR_PRIVATE );
		return 0;
	case CG_ARGC:
		return Cmd_Argc();
	case CG_ARGV:
		VM_CHECKBOUNDS( cgvm, args[2], args[3] );
		Cmd_ArgvBuffer( args[1], VMA(2), args[3] );
		return 0;
	case CG_ARGS:
		VM_CHECKBOUNDS( cgvm, args[1], args[2] );
		Cmd_ArgsBuffer( VMA(1), args[2] );
		return 0;

	case CG_FS_FOPENFILE:
		return FS_VM_OpenFile( VMA(1), VMA(2), args[3], H_CGAME );
	case CG_FS_READ:
		VM_CHECKBOUNDS( cgvm, args[1], args[2] );
		FS_VM_ReadFile( VMA(1), args[2], args[3], H_CGAME );
		return 0;
	case CG_FS_WRITE:
		VM_CHECKBOUNDS( cgvm, args[1], args[2] );
		FS_VM_WriteFile( VMA(1), args[2], args[3], H_CGAME );
		return 0;
	case CG_FS_FCLOSEFILE:
		FS_VM_CloseFile( args[1], H_CGAME );
		return 0;
	case CG_FS_SEEK:
		return FS_VM_SeekFile( args[1], args[2], args[3], H_CGAME );

	case CG_SENDCONSOLECOMMAND: {
		const char *cmd = VMA(1);
		Cbuf_NestedAdd( cmd );
		return 0;
	}
	case CG_ADDCOMMAND:
		CL_AddCgameCommand( VMA(1) );
		return 0;
	case CG_REMOVECOMMAND:
		Cmd_RemoveCommandSafe( VMA(1) );
		return 0;
	case CG_SENDCLIENTCOMMAND:
		CL_AddReliableCommand( VMA(1), qfalse );
		return 0;
	case CG_UPDATESCREEN:
		// this is used during lengthy level loading, so pump message loop
		// Com_EventLoop();	// FIXME: if a server restarts here, BAD THINGS HAPPEN!
		// We can't call Com_EventLoop here, a restart will crash and this _does_ happen
		// if there is a map change while we are downloading at pk3.
		// ZOID
		SCR_UpdateScreen();
		return 0;
	case CG_CM_LOADMAP:
		CL_CM_LoadMap( VMA(1) );
		cl_loadProgress.geometry = 1.0f;
		CL_LoadingYield( "loading geometry" );
		return 0;
	case CG_CM_NUMINLINEMODELS:
		return CM_NumInlineModels();
	case CG_CM_INLINEMODEL:
		return CM_InlineModel( args[1] );
	case CG_CM_TEMPBOXMODEL:
		return CM_TempBoxModel( VMA(1), VMA(2), /*int capsule*/ qfalse );
	case CG_CM_TEMPCAPSULEMODEL:
		return CM_TempBoxModel( VMA(1), VMA(2), /*int capsule*/ qtrue );
	case CG_CM_POINTCONTENTS:
		return CM_PointContents( VMA(1), args[2] );
	case CG_CM_TRANSFORMEDPOINTCONTENTS:
		return CM_TransformedPointContents( VMA(1), args[2], VMA(3), VMA(4) );
	case CG_CM_BOXTRACE:
		CM_BoxTrace( VMA(1), VMA(2), VMA(3), VMA(4), VMA(5), args[6], args[7], /*int capsule*/ qfalse );
		return 0;
	case CG_CM_CAPSULETRACE:
		CM_BoxTrace( VMA(1), VMA(2), VMA(3), VMA(4), VMA(5), args[6], args[7], /*int capsule*/ qtrue );
		return 0;
	case CG_CM_TRANSFORMEDBOXTRACE:
		CM_TransformedBoxTrace( VMA(1), VMA(2), VMA(3), VMA(4), VMA(5), args[6], args[7], VMA(8), VMA(9), /*int capsule*/ qfalse );
		return 0;
	case CG_CM_TRANSFORMEDCAPSULETRACE:
		CM_TransformedBoxTrace( VMA(1), VMA(2), VMA(3), VMA(4), VMA(5), args[6], args[7], VMA(8), VMA(9), /*int capsule*/ qtrue );
		return 0;
	case CG_CM_MARKFRAGMENTS:
		VM_CHECKBOUNDS3( cgvm, args[2], args[1], sizeof( vec3_t ) );
		VM_CHECKBOUNDS3( cgvm, args[5], args[4], sizeof( vec3_t ) );
		VM_CHECKBOUNDS3( cgvm, args[7], args[6], sizeof( markFragment_t ) );
		return re.MarkFragments( args[1], VMA(2), VMA(3), args[4], VMA(5), args[6], VMA(7) );
	case CG_S_STARTSOUND:
		S_StartSound( VMA(1), args[2], args[3], args[4] );
		return 0;
	case CG_S_STARTLOCALSOUND:
		S_StartLocalSound( args[1], args[2] );
		return 0;
	case CG_S_CLEARLOOPINGSOUNDS:
		S_ClearLoopingSounds(args[1]);
		return 0;
	case CG_S_ADDLOOPINGSOUND:
		S_AddLoopingSound( args[1], VMA(2), VMA(3), args[4] );
		return 0;
	case CG_S_ADDREALLOOPINGSOUND:
		S_AddRealLoopingSound( args[1], VMA(2), VMA(3), args[4] );
		return 0;
	case CG_S_STOPLOOPINGSOUND:
		S_StopLoopingSound( args[1] );
		return 0;
	case CG_S_UPDATEENTITYPOSITION:
		S_UpdateEntityPosition( args[1], VMA(2) );
		return 0;
	case CG_S_RESPATIALIZE:
		S_Respatialize( args[1], VMA(2), VMA(3), args[4] );
		return 0;
	case CG_S_REGISTERSOUND: {
		sfxHandle_t h = S_RegisterSound( VMA(1), args[2] );
		if ( cls.state == CA_LOADING ) {
			loadYield_soundCount++;
			cl_loadProgress.audio = (float)loadYield_soundCount / LOADING_EST_SOUNDS;
			if ( cl_loadProgress.audio > 1.0f ) cl_loadProgress.audio = 1.0f;
			if ( loadYield_soundCount % LOADING_YIELD_SOUNDS == 0 ) {
				CL_LoadingYield( "loading audio" );
			}
		}
		return h;
	}
	case CG_S_SOUNDDURATION:
		// Phase 6.2: returns sound length in milliseconds (0 if invalid).
		return S_SoundDuration( args[1] );
	case CG_S_STARTBACKGROUNDTRACK:
		S_StartBackgroundTrack( VMA(1), VMA(2) );
		return 0;
	case CG_R_LOADWORLDMAP:
		if ( !cls.cgameBsp ) {
			Com_Terminate( TERM_CLIENT_DROP, "%s: world BSP was not loaded", __func__ );
		}
		re.LoadWorld( cls.cgameBsp );
		CL_LoadingYield( "loading BSP" );
		return 0;
	case CG_R_REGISTERMODEL: {
		qhandle_t h = re.RegisterModel( VMA(1) );
		if ( cls.state == CA_LOADING ) {
			loadYield_modelCount++;
			if ( loadYield_modelCount % LOADING_YIELD_MODELS == 0 ) {
				CL_LoadingYield( "loading models" );
			}
		}
		return h;
	}
	case CG_R_REGISTERSKIN:
		return re.RegisterSkin( VMA(1) );
	case CG_R_REGISTERSHADER: {
		qhandle_t h = re.RegisterShader( VMA(1) );
		if ( cls.state == CA_LOADING ) {
			loadYield_shaderCount++;
			cl_loadProgress.shaders = (float)loadYield_shaderCount / LOADING_EST_SHADERS;
			if ( cl_loadProgress.shaders > 1.0f ) cl_loadProgress.shaders = 1.0f;
			if ( loadYield_shaderCount % LOADING_YIELD_SHADERS == 0 ) {
				CL_LoadingYield( "compiling shaders" );
			}
		}
		return h;
	}
	case CG_R_REGISTERSHADERNOMIP: {
		qhandle_t h = re.RegisterShaderNoMip( VMA(1) );
		if ( cls.state == CA_LOADING ) {
			loadYield_shaderCount++;
			cl_loadProgress.shaders = (float)loadYield_shaderCount / LOADING_EST_SHADERS;
			if ( cl_loadProgress.shaders > 1.0f ) cl_loadProgress.shaders = 1.0f;
			if ( loadYield_shaderCount % LOADING_YIELD_SHADERS == 0 ) {
				CL_LoadingYield( "compiling shaders" );
			}
		}
		return h;
	}
	case CG_R_REGISTERPRIMITIVESHADER: {
		// Like RegisterShader but also writes the shader's image into
		// vk_primitive_shader_images[] for ribbon / beam consumers.
		// Loading-progress increment matches the regular path so the
		// progress bar stays consistent regardless of which API the
		// cgame uses.
		qhandle_t h = re.RegisterPrimitiveShader ? re.RegisterPrimitiveShader( VMA(1) ) : 0;
		if ( cls.state == CA_LOADING ) {
			loadYield_shaderCount++;
			cl_loadProgress.shaders = (float)loadYield_shaderCount / LOADING_EST_SHADERS;
			if ( cl_loadProgress.shaders > 1.0f ) cl_loadProgress.shaders = 1.0f;
			if ( loadYield_shaderCount % LOADING_YIELD_SHADERS == 0 ) {
				CL_LoadingYield( "compiling shaders" );
			}
		}
		return h;
	}
	case CG_R_REGISTERFONT:
		re.RegisterFont( VMA(1), args[2], VMA(3));
		return 0;
	case CG_R_CLEARSCENE:
		re.ClearScene();
		return 0;
	case CG_R_ADDREFENTITYTOSCENE:
		re.AddRefEntityToScene( VMA(1), qfalse );
		return 0;
	case CG_R_ADDPOLYTOSCENE:
		re.AddPolyToScene( args[1], args[2], VMA(3), 1 );
		return 0;
	case CG_R_ADDPOLYSTOSCENE:
		re.AddPolyToScene( args[1], args[2], VMA(3), args[4] );
		return 0;
	case CG_R_LIGHTFORPOINT:
		return re.LightForPoint( VMA(1), VMA(2), VMA(3), VMA(4) );
	case CG_R_ADDLIGHTTOSCENE:
		re.AddLightToScene( VMA(1), VMF(2), VMF(3), VMF(4), VMF(5) );
		return 0;
	case CG_R_ADDADDITIVELIGHTTOSCENE:
		re.AddAdditiveLightToScene( VMA(1), VMF(2), VMF(3), VMF(4), VMF(5) );
		return 0;
	case CG_R_ADDRIBBONTOSCENE:
		if ( re.AddRibbonToScene ) {
			// Rebuild the host-side ribbonDesc_t from individual syscall
			// args. Cannot pass the descriptor as a struct over the VM
			// boundary because it contains a pointer field — see traps.h.
			ribbonDesc_t desc;
			desc.points    = VMA(1);
			desc.numPoints = args[2];
			desc.shader    = args[3];
			desc.flags     = args[4];
			re.AddRibbonToScene( &desc );
		}
		return 0;
	case CG_R_ADDBEAMTOSCENE:
		if ( re.AddBeamToScene )
			re.AddBeamToScene( VMA(1) );
		return 0;
	case CG_R_ADDSPRITETOSCENE:
		if ( re.AddSpriteToScene )
			re.AddSpriteToScene( VMA(1) );
		return 0;
	case CG_R_EMITPARTICLES:
		if ( re.EmitParticles )
			re.EmitParticles( VMA(1) );
		return 0;
	case CG_R_ADDDECALTOSCENE:
		if ( re.AddDecalToScene )
			re.AddDecalToScene( VMA(1) );
		return 0;
	case CG_R_REGISTERPARTICLECLASS:
		if ( re.RegisterParticleClass )
			re.RegisterParticleClass( args[1], VMA(2) );
		return 0;
	case CG_R_RENDERSCENE:
		re.RenderScene( VMA(1) );
		return 0;
	case CG_R_SETCOLOR:
		re.SetColor( VMA(1) );
		return 0;
	case CG_R_DRAWSTRETCHPIC:
		/* removed — all callers migrated to CG_R_DRAWSTRETCHPICNORM */
		COM_WARN( LOG_CH(ch_client), "WARNING: CG_R_DRAWSTRETCHPIC is removed, use CG_R_DRAWSTRETCHPICNORM\n" );
		return 0;
	case CG_R_DRAWSTRETCHPICNORM: {
		float x = VMF(1) * cls.glconfig.vidWidth;
		float y = VMF(2) * cls.glconfig.vidHeight;
		float w = VMF(3) * cls.glconfig.vidWidth;
		float h = VMF(4) * cls.glconfig.vidHeight;
		re.DrawStretchPic( x, y, w, h, VMF(5), VMF(6), VMF(7), VMF(8), args[9] );
		return 0;
	}
	case CG_R_MODELBOUNDS:
		re.ModelBounds( args[1], VMA(2), VMA(3) );
		return 0;
	case CG_R_LERPTAG:
		return re.LerpTag( VMA(1), args[2], args[3], args[4], VMF(5), VMA(6) );
	case CG_GETGLCONFIG:
		VM_CHECKBOUNDS( cgvm, args[1], sizeof( glconfig_t ) );
		CL_GetGlconfig( VMA(1) );
		return 0;
	case CG_GETGAMESTATE:
		VM_CHECKBOUNDS( cgvm, args[1], sizeof( gameState_t ) );
		CL_GetGameState( VMA(1) );
		return 0;
	case CG_GETCURRENTSNAPSHOTNUMBER:
		CL_GetCurrentSnapshotNumber( VMA(1), VMA(2) );
		return 0;
	case CG_GETSNAPSHOT:
		return CL_GetSnapshot( args[1], VMA(2) );
	case CG_GETSERVERCOMMAND:
		return CL_GetServerCommand( args[1] );
	case CG_GETCURRENTCMDNUMBER:
		return CL_GetCurrentCmdNumber();
	case CG_GETUSERCMD:
		return CL_GetUserCmd( args[1], VMA(2) );
	case CG_SETUSERCMDVALUE:
		CL_SetUserCmdValue( args[1], VMF(2) );
		return 0;
	case CG_MEMORY_REMAINING:
		return Hunk_MemoryRemaining();
	case CG_KEY_ISDOWN:
		return Key_IsDown( args[1] );
	case CG_KEY_GETCATCHER:
		return Key_GetCatcher();
	case CG_KEY_SETCATCHER:
		// Don't allow the cgame module to close the console
		Key_SetCatcher( args[1] | ( Key_GetCatcher( ) & KEYCATCH_CONSOLE ) );
		return 0;
	case CG_KEY_GETKEY:
		return Key_GetKey( VMA(1) );

	// shared syscalls
	case TRAP_MEMSET:
		VM_CHECKBOUNDS( cgvm, args[1], args[3] );
		memset( VMA(1), args[2], args[3] );
		return args[1];
	case TRAP_MEMCPY:
		VM_CHECKBOUNDS2( cgvm, args[1], args[2], args[3] );
		memcpy( VMA(1), VMA(2), args[3] );
		return args[1];
	case TRAP_STRNCPY:
		VM_CHECKBOUNDS( cgvm, args[1], args[3] );
		Q_strncpy( VMA(1), VMA(2), args[3] );
		return args[1];
	case TRAP_SIN:
		return FloatAsInt( sin( VMF(1) ) );
	case TRAP_COS:
		return FloatAsInt( cos( VMF(1) ) );
	case TRAP_ATAN2:
		return FloatAsInt( atan2( VMF(1), VMF(2) ) );
	case TRAP_SQRT:
		return FloatAsInt( sqrt( VMF(1) ) );

	case CG_FLOOR:
		return FloatAsInt( floor( VMF(1) ) );
	case CG_CEIL:
		return FloatAsInt( ceil( VMF(1) ) );
	case CG_TESTPRINTINT:
		return sprintf( VMA(1), "%i", (int)args[2] );
	case CG_TESTPRINTFLOAT:
		return sprintf( VMA(1), "%f", VMF(2) );
	case CG_ACOS:
		return FloatAsInt( Q_acos( VMF(1) ) );

	case CG_PC_ADD_GLOBAL_DEFINE:
		return botlib_export->PC_AddGlobalDefine( VMA(1) );
	case CG_PC_LOAD_SOURCE:
		return botlib_export->PC_LoadSourceHandle( VMA(1) );
	case CG_PC_FREE_SOURCE:
		return botlib_export->PC_FreeSourceHandle( args[1] );
	case CG_PC_READ_TOKEN:
		return botlib_export->PC_ReadTokenHandle( args[1], VMA(2) );
	case CG_PC_SOURCE_FILE_AND_LINE:
		return botlib_export->PC_SourceFileAndLine( args[1], VMA(2), VMA(3) );

	case CG_S_STOPBACKGROUNDTRACK:
		S_StopBackgroundTrack();
		return 0;

	case CG_REAL_TIME:
		return Com_RealTime( VMA(1) );
	case CG_SNAPVECTOR:
		Sys_SnapVector( VMA(1) );
		return 0;

	case CG_CIN_PLAYCINEMATIC:
		return CIN_PlayCinematic(VMA(1), args[2], args[3], args[4], args[5], args[6]);

	case CG_CIN_STOPCINEMATIC:
		return CIN_StopCinematic(args[1]);

	case CG_CIN_RUNCINEMATIC:
		return CIN_RunCinematic(args[1]);

	case CG_CIN_DRAWCINEMATIC:
		CIN_DrawCinematic(args[1]);
		return 0;

	case CG_CIN_SETEXTENTS:
		CIN_SetExtents(args[1], args[2], args[3], args[4], args[5]);
		return 0;

	case CG_R_REMAP_SHADER:
		re.RemapShader( VMA(1), VMA(2), VMA(3) );
		return 0;

/*
	case CG_LOADCAMERA:
		return loadCamera(VMA(1));

	case CG_STARTCAMERA:
		startCamera(args[1]);
		return 0;

	case CG_GETCAMERAINFO:
		return getCameraInfo(args[1], VMA(2), VMA(3));
*/
	case CG_GET_ENTITY_TOKEN:
		VM_CHECKBOUNDS( cgvm, args[1], args[2] );
		return re.GetEntityToken( VMA(1), args[2] );

	case CG_R_INPVS:
		return re.inPVS( VMA(1), VMA(2) );

	// engine extensions
	case CG_R_ADDREFENTITYTOSCENE2:
		re.AddRefEntityToScene( VMA(1), qtrue );
		return 0;

	case CG_R_ADDLINEARLIGHTTOSCENE:
		re.AddLinearLightToScene( VMA(1), VMA(2), VMF(3), VMF(4), VMF(5), VMF(6) );
		return 0;

	case CG_R_FORCEFIXEDDLIGHTS:
		CL_ForceFixedDlights();
		return 0;

	case CG_IS_RECORDING_DEMO:
		return clc.demorecording;

	case CG_CVAR_SETDESCRIPTION:
		Cvar_SetDescription2( (const char*)VMA(1), (const char*)VMA(2) );
		return 0;

#if FEAT_IQM
	case CG_R_GETIQMANIMS:
		if ( re.GetIQMAnimations )
			return re.GetIQMAnimations( args[1], VMA(2), args[3] );
		return 0;
#endif // FEAT_IQM

	case CG_TRAP_GETVALUE:
		VM_CHECKBOUNDS( cgvm, args[1], args[2] );
		return CL_GetValue( VMA(1), args[2], VMA(3) );

#if FEAT_WIRED_UI
	case CG_WIREDUI_PUSH_HUD_STATE:
		WiredHud_ReceiveState( VMA(1) );
		return 0;
	case CG_WIREDUI_PUSH_EVENT:
		WiredHud_ReceiveEvent( args[1], VMA(2) );
		return 0;
	case CG_R_DRAWTEXTNORM:
		/* normalized coords (0.0-1.0) — scale to real pixels */
		{
			float x = VMF(2) * cls.glconfig.vidWidth;
			float y = VMF(3) * cls.glconfig.vidHeight;
			float size = VMF(5) * cls.glconfig.vidHeight;
			Text_Draw( VMA(1), x, y, args[4], size, VMA(6), args[7], args[8] );
		}
		return 0;
	case CG_R_MEASURETEXTNORM:
		/* normalized size (0.0-1.0) — scale to real pixels, measure, scale result back */
		{
			float realSize = VMF(3) * cls.glconfig.vidHeight;
			float realWidth = Text_Measure( VMA(1), args[2], realSize );
			return FloatAsInt( realWidth / (float)cls.glconfig.vidWidth );
		}
	case CG_WUI_STORE_PUSH_BATCH:
		{
			const wuiStagedEntry_t *entries = VMA(1);
			int count = args[2];
			for ( int i = 0; i < count; i++ ) {
				wuiStoreEntry_t *e = WiredStore_Set( entries[i].key );
				if ( !e ) continue;
				if ( entries[i].fields & WUI_STAGED_TEXT ) {
					Q_strncpyz( e->text, entries[i].text, sizeof( e->text ) );
				}
				if ( entries[i].fields & WUI_STAGED_COLOR ) {
					Vector4Copy( entries[i].color, e->color );
				}
				if ( entries[i].fields & WUI_STAGED_ICON ) {
					e->icon = entries[i].icon;
				}
				if ( entries[i].fields & WUI_STAGED_VALUE ) {
					e->value = entries[i].value;
				}
				if ( entries[i].fields & WUI_STAGED_STATE ) {
					Q_strncpyz( e->state, entries[i].state, sizeof( e->state ) );
				}
				e->flags |= WUI_STORE_FLAG_DIRTY;
			}
		}
		return 0;
	case CG_WUI_STORE_DELETE:
		WiredStore_Delete( VMA(1) );
		return 0;
	case CG_WUI_STORE_CLEAR:
		WiredStore_Clear();
		return 0;
#endif

	case CG_R_SETLIGHTSTYLEPATTERN:
		if ( re.SetLightstylePattern )
			re.SetLightstylePattern( args[1], VMA(2) );
		return 0;

	default:
		Com_Terminate( TERM_CLIENT_DROP, "Bad cgame system trap: %ld", (long int) args[0] );
	}
	return 0;
}


/*
====================
CL_DllSyscall
====================
*/
static intptr_t QDECL CL_DllSyscall( intptr_t arg, ... ) {
#if !id386 || defined __clang__
	intptr_t	args[10]; // max.count for cgame
	va_list	ap;

	args[0] = arg;
	va_start( ap, arg );
	for (int i = 1; i < ARRAY_LEN( args ); i++ )
		args[ i ] = va_arg( ap, intptr_t );
	va_end( ap );

	return CL_CgameSystemCalls( args );
#else
	return CL_CgameSystemCalls( &arg );
#endif
}


/*
====================
CL_InitCGame

Should only be called by CL_StartHunkUsers
====================
*/
void CL_InitCGame( void ) {
	const char			*info;
	const char			*mapname;

	Cbuf_NestedReset();

	int t1 = Sys_Milliseconds();

	// Reset loading progress tracking
	memset( &cl_loadProgress, 0, sizeof( cl_loadProgress ) );
	cl_loadProgress.startTime = cls.realtime;
	cl_loadProgress.phase = "initializing";
	loadYield_modelCount = 0;
	loadYield_shaderCount = 0;
	loadYield_soundCount = 0;

	// Soft-close the console: collapse visually but preserve KEYCATCH_CONSOLE
	// so the user does not have to re-open it after the level finishes loading.
	Con_SoftClose();

	// find the current mapname
	info = cl.gameState.stringData + cl.gameState.stringOffsets[ CS_SERVERINFO ];
	mapname = Info_ValueForKey( info, "mapname" );
	Com_sprintf( cl.mapname, sizeof( cl.mapname ), "maps/%s.bsp", mapname );

	// allow vertex lighting for in-game elements
	re.VertexLighting( qtrue );

	// load native DLL or WASM module per vm_cgame cvar (0=native, 1=WASM interp, 2=WASM AOT)
	vmInterpret_t interpret = Cvar_VariableIntegerValue( "vm_cgame" );
	if ( cl_connectedToPureServer )
	{
		// pure server: force WASM (native DLLs bypass pak integrity checks)
		if ( interpret != VMI_COMPILED && interpret != VMI_BYTECODE )
			interpret = VMI_COMPILED;
	}

	cgvm = VM_Create( VM_CGAME, CL_CgameSystemCalls, CL_DllSyscall, interpret );
	if ( !cgvm ) {
		Com_Terminate( TERM_CLIENT_DROP, "VM_Create on cgame failed" );
	}
	cls.state = CA_LOADING;
	cl_loadYieldLastTime = Sys_Milliseconds();

	// init for this gamestate
	// use the lastExecutedServerCommand instead of the serverCommandSequence
	// otherwise server commands sent just before a gamestate are dropped
	VM_Call( cgvm, 3, CG_INIT, clc.serverMessageSequence, clc.lastExecutedServerCommand, clc.clientNum );

	// we will send a usercmd this frame, which
	// will cause the server to send us the first snapshot
	cls.state = CA_PRIMED;

	int t2 = Sys_Milliseconds();

	Com_Log( SEV_INFO, LOG_CH(ch_client), "CL_InitCGame: %5.2f seconds\n", (t2-t1)/1000.0 );

	// have the renderer touch all its images, so they are present
	// on the card even if the driver does deferred loading
	re.EndRegistration();

	// Final yield — all assets are loaded and finalized
	cl_loadProgress.geometry = 1.0f;
	cl_loadProgress.shaders = 1.0f;
	cl_loadProgress.audio = 1.0f;
	CL_LoadingYield( "finalizing" );

	// make sure everything is paged in
	if (!Sys_LowPhysicalMemory()) {
		Com_TouchMemory();
	}

	// clear anything that got printed
	Con_ClearNotify ();

	// do not allow vid_restart for first time
	cls.lastVidRestart = Sys_Milliseconds();
}


/*
====================
CL_GameCommand

See if the current console command is claimed by the cgame
====================
*/

qboolean CL_GameCommand( void ) {
	qboolean bRes;

	if ( !cgvm ) {
		return qfalse;
	}

	bRes = (qboolean)VM_Call( cgvm, 0, CG_CONSOLE_COMMAND );

	Cbuf_NestedReset();

	return bRes;
}


/*
=====================
CL_CGameRendering
=====================
*/
void CL_CGameRendering( stereoFrame_t stereo ) {
	VM_Call( cgvm, 3, CG_DRAW_ACTIVE_FRAME, cl.serverTime, stereo, clc.demoplaying );
#ifdef DEBUG
	VM_Debug( 0 );
#endif
}


/*
=================
CL_AdjustTimeDelta

Adjust the clients view of server time.

We attempt to have cl.serverTime exactly equal the server's view
of time plus the timeNudge, but with variable latencies over
the internet it will often need to drift a bit to match conditions.

Our ideal time would be to have the adjusted time approach, but not pass,
the very latest snapshot.

Adjustments are only made when a new snapshot arrives with a rational
latency, which keeps the adjustment process framerate independent and
prevents massive overadjustment during times of significant packet loss
or bursted delayed packets.
=================
*/

#define	RESET_TIME	500

static void CL_AdjustTimeDelta( void ) {
	cl.newSnapshots = qfalse;

	// the delta never drifts when replaying a demo
	if ( clc.demoplaying ) {
		return;
	}

	int newDelta = cl.snap.serverTime - cls.realtime;
	int deltaDelta = abs( newDelta - cl.serverTimeDelta );

	if ( deltaDelta > RESET_TIME ) {
		cl.serverTimeDelta = newDelta;
		cl.oldServerTime = cl.snap.serverTime;	// FIXME: is this a problem for cgame?
		cl.serverTime = cl.snap.serverTime;
		if ( cl_showTimeDelta->integer ) {
			Com_Log( SEV_INFO, LOG_CH(ch_client), "<RESET> " );
		}
	} else if ( deltaDelta > 100 ) {
		// fast adjust, cut the difference in half
		if ( cl_showTimeDelta->integer ) {
			Com_Log( SEV_INFO, LOG_CH(ch_client), "<FAST> " );
		}
		cl.serverTimeDelta = ( cl.serverTimeDelta + newDelta ) >> 1;
	} else {
		// slow drift adjust, only move 1 or 2 msec

		// if any of the frames between this and the previous snapshot
		// had to be extrapolated, nudge our sense of time back a little
		// the granularity of +1 / -2 is too high for timescale modified frametimes
		if ( com_timescale->value == 0 || com_timescale->value == 1 ) {
			if ( cl.extrapolatedSnapshot ) {
				cl.extrapolatedSnapshot = qfalse;
				cl.serverTimeDelta -= 2;
			} else {
				// otherwise, move our sense of time forward to minimize total latency
				cl.serverTimeDelta++;
			}
		}
	}

	if ( cl_showTimeDelta->integer ) {
		Com_Log( SEV_INFO, LOG_CH(ch_client), "%i ", cl.serverTimeDelta );
	}
}


/*
==================
CL_FirstSnapshot
==================
*/
static void CL_FirstSnapshot( void ) {
	// ignore snapshots that don't have entities
	if ( cl.snap.snapFlags & SNAPFLAG_NOT_ACTIVE ) {
		return;
	}
	cls.state = CA_ACTIVE;

	// clear old game so we will not switch back to old mod on disconnect
	CL_ResetOldGame();

	// set the timedelta so we are exactly on this first frame
	cl.serverTimeDelta = cl.snap.serverTime - cls.realtime;
	cl.oldServerTime = cl.snap.serverTime;

	clc.timeDemoBaseTime = cl.snap.serverTime;

	// if this is the first frame of active play,
	// execute the contents of activeAction now
	// this is to allow scripting a timedemo to start right
	// after loading
	if ( cl_activeAction->string[0] ) {
		Cbuf_AddText( cl_activeAction->string );
		Cbuf_AddText( "\n" );
		Cvar_Set( "activeAction", "" );
	}

	Sys_BeginProfiling();
}


/*
==================
CL_AvgPing

Calculates Average Ping from snapshots in buffer. Used by AutoNudge.
==================
*/
static float CL_AvgPing( void ) {
	int ping[PACKET_BACKUP];
	int count = 0;

	for ( int i = 0; i < PACKET_BACKUP; i++ ) {
		if ( cl.snapshots[i].ping > 0 && cl.snapshots[i].ping < 999 ) {
			ping[count] = cl.snapshots[i].ping;
			count++;
		}
	}

	if ( count == 0 )
		return 0;

	// sort ping array
	for ( int i = count - 1; i > 0; --i ) {
		for ( int j = 0; j < i; ++j ) {
			if (ping[j] > ping[j + 1]) {
				int iTemp = ping[j];
				ping[j] = ping[j + 1];
				ping[j + 1] = iTemp;
			}
		}
	}

	// use median average ping
	float result;
	// NOLINTBEGIN(bugprone-integer-division) — `count / 2` is the median array index; integer division is correct here
	if ( (count % 2) == 0 )
		result = (ping[count / 2] + ping[(count / 2) - 1]) / 2.0f;
	else
		result = ping[count / 2];
	// NOLINTEND(bugprone-integer-division)

	return result;
}


/*
==================
CL_TimeNudge

Returns either auto-nudge or cl_timeNudge value.
==================
*/
static int CL_TimeNudge( void ) {
	float autoNudge = cl_autoNudge->value;

	if ( autoNudge != 0.0f )
		return (int)((CL_AvgPing() * autoNudge) + 0.5f) * -1;
	return cl_timeNudge->integer;
}


/*
==================
CL_SetCGameTime
==================
*/
void CL_SetCGameTime( void ) {
	qboolean demoFreezed;

	// getting a valid frame message ends the connection process
	if ( cls.state != CA_ACTIVE ) {
		if ( cls.state != CA_PRIMED ) {
			return;
		}
		if ( clc.demoplaying ) {
			// we shouldn't get the first snapshot on the same frame
			// as the gamestate, because it causes a bad time skip
			if ( !clc.firstDemoFrameSkipped ) {
				clc.firstDemoFrameSkipped = qtrue;
				return;
			}
			CL_ReadDemoMessage();
		}
		if ( cl.newSnapshots ) {
			cl.newSnapshots = qfalse;
			CL_FirstSnapshot();
		}
		if ( cls.state != CA_ACTIVE ) {
			return;
		}
	}

	// if we have gotten to this point, cl.snap is guaranteed to be valid
	if ( !cl.snap.valid ) {
		Com_Terminate( TERM_CLIENT_DROP, "CL_SetCGameTime: !cl.snap.valid" );
	}

	// allow pause in single player
	if ( sv_paused->integer && CL_CheckPaused() && com_sv_running->integer ) {
		// paused
		return;
	}

	if ( cl.snap.serverTime - cl.oldFrameServerTime < 0 ) {
		Com_Terminate( TERM_CLIENT_DROP, "cl.snap.serverTime < cl.oldFrameServerTime" );
	}
	cl.oldFrameServerTime = cl.snap.serverTime;

	// get our current view of time
	demoFreezed = clc.demoplaying && com_timescale->value == 0.0f;
	if ( demoFreezed ) {
		// \timescale 0 is used to lock a demo in place for single frame advances
		cl.serverTimeDelta -= cls.frametime;
	} else {
		// cl_timeNudge is a user adjustable cvar that allows more
		// or less latency to be added in the interest of better
		// smoothness or better responsiveness.
		cl.serverTime = cls.realtime + cl.serverTimeDelta - CL_TimeNudge();

		// guarantee that time will never flow backwards, even if
		// serverTimeDelta made an adjustment or cl_timeNudge was changed
		if ( cl.serverTime - cl.oldServerTime < 0 ) {
			cl.serverTime = cl.oldServerTime;
		}
		cl.oldServerTime = cl.serverTime;

		// note if we are almost past the latest frame (without timeNudge),
		// so we will try and adjust back a bit when the next snapshot arrives
		//if ( cls.realtime + cl.serverTimeDelta >= cl.snap.serverTime - 5 ) {
		if ( cls.realtime + cl.serverTimeDelta - cl.snap.serverTime >= -5 ) {
			cl.extrapolatedSnapshot = qtrue;
		}
	}

	// if we have gotten new snapshots, drift serverTimeDelta
	// don't do this every frame, or a period of packet loss would
	// make a huge adjustment
	if ( cl.newSnapshots ) {
		CL_AdjustTimeDelta();
	}

	if ( !clc.demoplaying ) {
		return;
	}

	// if we are playing a demo back, we can just keep reading
	// messages from the demo file until the cgame definitely
	// has valid snapshots to interpolate between

	// a timedemo will always use a deterministic set of time samples
	// no matter what speed machine it is run on,
	// while a normal demo may have different time samples
	// each time it is played back
	if ( com_timedemo->integer ) {
		if ( !clc.timeDemoStart ) {
			clc.timeDemoStart = Sys_Milliseconds();
		}
		clc.timeDemoFrames++;
		cl.serverTime = clc.timeDemoBaseTime + clc.timeDemoFrames * 50;
	}

	//while ( cl.serverTime >= cl.snap.serverTime ) {
	while ( cl.serverTime - cl.snap.serverTime >= 0 ) {
		// feed another message, which should change
		// the contents of cl.snap
		CL_ReadDemoMessage();
		if ( cls.state != CA_ACTIVE ) {
			return; // end of demo
		}
	}
}
