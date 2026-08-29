// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 1999-2005 Id Software, Inc.
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
// cl_cgame.c  -- client system interaction with client game

#include "client.h"
#include "cl_wired_fx.h"
#include "wired/ui/cl_wired_ui.h"          /* WiredUI_SetLoadingMenu (state→named-UI) */
#include "wired/ui/cl_wired_ui_hud_state.h"
#include "wired/ui/cl_wired_text.h"
#include "wired/ui/cl_wired_viewport.h"   /* V-16 viewport provider syscalls */
#include "wired/store/cl_wired_store.h"
#include "wired/l10n/cl_wired_l10n.h"      /* WiredL10n_Get (caption key → text) */
#include "../qcommon/wired/scene/wired_scene.h"   /* WiredScene_LoadFromFile + POD wiredScene_t */

#include "../botlib/botlib.h"
#include "../qcommon/vm_typed_syscall.h"   /* typed VM-IPC: hot-subset cgame syscalls */
LOG_DECLARE_CHANNEL( ch_cgame, "cgame" );
LOG_DECLARE_CHANNEL( ch_client, "client" );

extern	botlib_export_t	*botlib_export;

//extern qboolean loadCamera(const char *name);
//extern void startCamera(int time);
//extern qboolean getCameraInfo(int time, vec3_t *origin, vec3_t *angles);

/* The on-screen pixel rect of the viewport currently being rendered. Set around
 * the world-scene VM_Call (CL_RenderCGameViewport) and read by the scene-render
 * syscall handler, which intersects the cgame's (fullscreen) refdef with it so a
 * non-full-screen viewport clips to its panel. A full-screen viewport's rect
 * contains the refdef, so the intersection is a no-op (the world fills the
 * screen as it always did). Inactive outside a viewport scene render. */
static struct {
	int      x, y, w, h;
	qboolean active;
} s_viewportClip;

/*
====================
CL_AppForActiveCgame

Resolve the client-app whose cgame VM is currently executing a syscall. The
in-syscall data handlers must read/write the EXECUTING app's connection state,
not the input-focused app's — otherwise a non-focused app's cgame would touch
the focused app's data. The executing VM is VM_ActiveNativeVM() (set around the
vmMain call on both backends); its slot index maps to clientApps[].

Single-app: the only cgame VM is clientApps[0].cgvm, slot 0, so this returns
&clientApps[0] == clientActiveApp — identical to reading clientActiveApp directly.

A cgame syscall reached with no executing VM is a control-flow invariant
violation (every handler is entered via VM_Call, which sets the token); fail
loudly rather than silently serving slot 0.
====================
*/
static clientApp_t *CL_AppForActiveCgame( void ) {
	vm_t *vm = VM_ActiveNativeVM();
	int   slot;
	if ( !vm ) {
		Com_Terminate( TERM_CLIENT_DROP, "CL_AppForActiveCgame: cgame syscall with no executing VM" );
		return clientActiveApp;   /* unreached — Com_Terminate longjmps */
	}
	slot = VM_CgameInstance( vm );
	if ( slot < 0 || slot >= MAX_LOCAL_CGAME_VMS ) {
		// Fail loud rather than silently serving slot 0's connection/snapshot
		// state for a corrupted instance index (matches the invariant
		// VM_SlotFor enforces at VM creation and the sibling viewport guards).
		Com_Terminate( TERM_CLIENT_DROP, "CL_AppForActiveCgame: cgame VM has bad instance %i", slot );
		return clientActiveApp;   /* unreached — Com_Terminate longjmps */
	}
	return &clientApps[ slot ];
}


/*
====================
CL_AnyViewportAppRenderable

True if any app that owns a registered viewport provider is in a world-rendering
state. The viewport owner token is that app's cgame VM handle (captured at
register time), which resolves to the app's clientApps[] slot. A client renders
its world when it is in-game (CA_ACTIVE) or when it has a cgame VM and is primed
(the loading-screen backdrop draws under the loading UI during the primed state).

This decouples the world-viewport layer from input focus: the layer activates
because SOME app with a viewport is renderable, not because the focused app is.
Single-client: the only owner is clientApps[0] (== the focused app), so this
returns exactly what the focused-app state check returned.
====================
*/
static qboolean cl_viewportOwnerRenderable( const void *owner ) {
	vm_t        *vm = (vm_t *)owner;
	int          slot;
	clientApp_t *app;
	if ( !vm )
		return qfalse;
	slot = VM_CgameInstance( vm );
	if ( slot < 0 || slot >= MAX_LOCAL_CGAME_VMS )
		return qfalse;
	app = &clientApps[ slot ];
	return ( app->state == CA_ACTIVE
	      || ( app->cgvm && app->state == CA_PRIMED ) ) ? qtrue : qfalse;
}

qboolean CL_AnyViewportAppRenderable( void ) {
#if FEAT_WIRED_UI
	return WiredUI_AnyViewportOwner( cl_viewportOwnerRenderable );
#else
	return qfalse;
#endif
}


/*
====================
CL_ViewportProviderId

Map the cgame-supplied viewport-provider id to the effective registry id,
namespaced by the executing app's cgame slot. The viewport registry is single-
occupancy per id, so without namespacing a second client app registering the
same hardcoded id would evict the first app's provider. Slot 0 (the input-
focused app) keeps the bare id verbatim so its menu-authored lookups still
resolve; slot i>0 gets "<id>#<i>". Returns the bare id pointer for slot 0, or
`buf` (filled) for slot>0. Both the register and unregister syscalls route
through this with the same slot so the two stay symmetric.
====================
*/
static const char *CL_ViewportProviderId( const char *id, int slot, char *buf, int bufSize ) {
	if ( slot <= 0 )
		return id;   /* bare id verbatim — the focused app's lookups resolve this */
	Com_sprintf( buf, bufSize, "%s#%d", id, slot );
	return buf;
}


/*
====================
CL_GetGameState
====================
*/
static void CL_GetGameState( gameState_t *gs ) {
	clientApp_t *app = CL_AppForActiveCgame();
	*gs = app->cl.gameState;
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
CL_GetSceneFrameContext

V-20 (2026-05-31): fill the per-frame scene context the world-viewport
provider pulls via the CG_GET_SCENE_FRAME_CONTEXT syscall. These are the
exact three inputs the (now-retired) engine-side direct CL_CGameRendering
call passed to CG_DRAW_ACTIVE_FRAME — clientActiveApp->cl.serverTime, STEREO_CENTER (the
main viewport is always centre; stereo eyes are a future per-viewport
concern), and clientActiveApp->clc.demoplaying. Engine pushes nothing down the render
contract; the cgame pulls.
====================
*/
static void CL_GetSceneFrameContext( wuiSceneFrameCtx_t *out ) {
	clientApp_t *app = CL_AppForActiveCgame();
	out->serverTime   = app->cl.serverTime;
	out->stereo       = STEREO_CENTER;
	out->demoPlayback = app->clc.demoplaying;
}


/*
====================
CL_WiredSceneLoad

Load a cinematic-scene .lua definition engine-side (FS + System Lua VM) into a
host-layout wiredScene_t, then marshal it into the cgame's `out` buffer by raw
memcpy. The struct is pointer-free fixed arrays, so its wasm32 and x64 layouts
are byte-identical (asserted at compile time in wired_scene.h consumers) and
the copy is safe. This is the one load-time crossing; the cgame runs the
evaluator locally thereafter (no per-frame trap). Returns qtrue on success.
====================
*/
/* POD-ship layout guard: the raw memcpy across wasm32<->x64 is only safe because
   wiredScene_t is pointer-free fixed arrays with identical layout on both. This
   size must match the same assert in the cgame trap stub; if the struct grows,
   both fail to compile until the number is re-synced on both sides. */
_Static_assert( sizeof( wiredScene_t ) == 21936, "wiredScene_t POD size changed — re-verify wasm32<->x64 layout parity" );

static qboolean CL_WiredSceneLoad( const char *name, wiredScene_t *out ) {
	wiredScene_t def;

	if ( !name || !*name || !out ) {
		return qfalse;
	}
	if ( !WiredScene_LoadFromFile( &def, name ) ) {
		return qfalse;
	}
	memcpy( out, &def, sizeof( def ) );
	return qtrue;
}


/*
====================
CL_GetUserCmd
====================
*/
static qboolean CL_GetUserCmd( int cmdNumber, usercmd_t *ucmd ) {
	clientApp_t *app = CL_AppForActiveCgame();
	// cmds[cmdNumber] is the last properly generated command

	// can't return anything that we haven't created yet
	if ( app->cl.cmdNumber - cmdNumber < 0 ) {
		Com_Terminate( TERM_CLIENT_DROP, "CL_GetUserCmd: cmdNumber (%i) > cl.cmdNumber (%i)", cmdNumber, app->cl.cmdNumber );
	}

	// the usercmd has been overwritten in the wrapping
	// buffer because it is too far out of date
	if ( app->cl.cmdNumber - cmdNumber >= CMD_BACKUP ) {
		return qfalse;
	}

	*ucmd = app->cl.cmds[ cmdNumber & CMD_MASK ];

	return qtrue;
}


/*
====================
CL_GetCurrentCmdNumber
====================
*/
static int CL_GetCurrentCmdNumber( void ) {
	return CL_AppForActiveCgame()->cl.cmdNumber;
}


/*
====================
CL_GetCurrentSnapshotNumber
====================
*/
static void CL_GetCurrentSnapshotNumber( int *snapshotNumber, int *serverTime ) {
	clientApp_t *app = CL_AppForActiveCgame();
	*snapshotNumber = app->cl.snap.messageNum;
	*serverTime = app->cl.snap.serverTime;
}


/*
====================
CL_GetSnapshot
====================
*/
static qboolean CL_GetSnapshot( int snapshotNumber, snapshot_t *snapshot ) {
	clientApp_t     *app = CL_AppForActiveCgame();
	clSnapshot_t	*clSnap;

	if ( app->cl.snap.messageNum - snapshotNumber < 0 ) {
		Com_Terminate( TERM_CLIENT_DROP, "CL_GetSnapshot: snapshotNumber (%i) > cl.snapshot.messageNum (%i)", snapshotNumber, app->cl.snap.messageNum );
	}

	// if the frame has fallen out of the circular buffer, we can't return it
	if ( app->cl.snap.messageNum - snapshotNumber >= PACKET_BACKUP ) {
		return qfalse;
	}

	// if the frame is not valid, we can't return it
	clSnap = &app->cl.snapshots[snapshotNumber & PACKET_MASK];
	if ( !clSnap->valid ) {
		return qfalse;
	}

	// if the entities in the frame have fallen out of their
	// circular buffer, we can't return it
	if ( app->cl.parseEntitiesNum - clSnap->parseEntitiesNum >= MAX_PARSE_ENTITIES ) {
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
			app->cl.parseEntities[ ( clSnap->parseEntitiesNum + i ) & (MAX_PARSE_ENTITIES-1) ];
	}

	// FIXME: configstring changes and server commands!!!

	return qtrue;
}


/*
=====================
CL_SetUserCmdValue
=====================
*/
static void CL_SetUserCmdValue( int userCmdValue, float sensitivityScale, int freezeMove ) {
	clientApp_t *app = CL_AppForActiveCgame();
	app->cl.cgameUserCmdValue = userCmdValue;
	app->cl.cgameSensitivity = sensitivityScale;
	app->cl.cgameFreezeMove = freezeMove;
}

static void CL_SetUserCmdAim( int mode, int pitchShort, int yawShort ) {
	clientApp_t *app = CL_AppForActiveCgame();

	if ( mode != UCMD_AIM_THIRD_PERSON_CENTER ) {
		app->cl.cgameAimMode = UCMD_AIM_NONE;
		app->cl.cgameAimAngles[PITCH] = 0;
		app->cl.cgameAimAngles[YAW] = 0;
		return;
	}

	app->cl.cgameAimMode = mode;
	app->cl.cgameAimAngles[PITCH] = pitchShort & 0xffff;
	app->cl.cgameAimAngles[YAW] = yawShort & 0xffff;
}


/*
=====================
CL_AddCgameCommand
=====================
*/
static void CL_AddCgameCommand( const char *cmdName ) {
	// Owner = the executing cgame VM handle (live inside the CG_ADDCOMMAND
	// syscall), so this command is attributed to the registering app and only
	// that app's teardown removes it. Same token the viewport registry uses.
	Cmd_AddCgameCommand( cmdName, VM_ActiveNativeVM() );
}


/*
=====================
CL_ConfigstringModified
=====================
*/
static void CL_ConfigstringModified( void ) {
	clientApp_t	*app = CL_AppForActiveCgame();
	const char	*old, *s;
	const char	*dup;
	gameState_t	oldGs;

	int index = atoi( Cmd_Argv(1) );
	if ( (unsigned) index >= MAX_CONFIGSTRINGS ) {
		Com_Terminate( TERM_CLIENT_DROP, "%s: bad configstring index %i", __func__, index );
	}
	// get everything after "cs <num>"
	s = Cmd_ArgsFrom(2);

	old = app->cl.gameState.stringData + app->cl.gameState.stringOffsets[ index ];
	if ( !strcmp( old, s ) ) {
		return;		// unchanged
	}

	// build the new gameState_t
	oldGs = app->cl.gameState;

	memset( &app->cl.gameState, 0, sizeof( app->cl.gameState ) );

	// leave the first 0 for uninitialized strings
	app->cl.gameState.dataCount = 1;

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

		if ( len + 1 + app->cl.gameState.dataCount > MAX_GAMESTATE_CHARS ) {
			Com_Terminate( TERM_CLIENT_DROP, "%s: MAX_GAMESTATE_CHARS exceeded", __func__ );
		}

		// append it to the gameState string buffer
		app->cl.gameState.stringOffsets[ i ] = app->cl.gameState.dataCount;
		memcpy( app->cl.gameState.stringData + app->cl.gameState.dataCount, dup, len + 1 );
		app->cl.gameState.dataCount += len + 1;
	}

	if ( index == CS_SYSTEMINFO ) {
		// parse serverId and other cvars for the executing client
		CL_SystemInfoChanged( app, qfalse );
	}
}


/*
===================
CL_GetServerCommand

Set up argc/argv for the given command
===================
*/
static qboolean CL_GetServerCommand( int serverCommandNumber ) {
	clientApp_t *app = CL_AppForActiveCgame();
	const char *s;
	const char *cmd;
	static char bigConfigString[BIG_INFO_STRING];
	int argc, index;

	// if we have irretrievably lost a reliable command, drop the connection
	if ( app->clc.serverCommandSequence - serverCommandNumber >= MAX_RELIABLE_COMMANDS ) {
		// when a demo record was started after the client got a whole bunch of
		// reliable commands then the client never got those first reliable commands
		if ( app->clc.demoplaying ) {
			Cmd_Clear();
			return qfalse;
		}
		Com_Terminate( TERM_CLIENT_DROP, "CL_GetServerCommand: a reliable command was cycled out" );
		return qfalse;
	}

	if ( app->clc.serverCommandSequence - serverCommandNumber < 0 ) {
		Com_Terminate( TERM_CLIENT_DROP, "CL_GetServerCommand: requested a command not received" );
		return qfalse;
	}

	index = serverCommandNumber & ( MAX_RELIABLE_COMMANDS - 1 );
	s = app->clc.serverCommands[ index ];
	app->clc.lastExecutedServerCommand = serverCommandNumber;

	if ( app->clc.serverCommandsIgnore[ index ] ) {
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
		memset( app->cl.cmds, 0, sizeof( app->cl.cmds ) );
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
	clientApp_t	*app = CL_AppForActiveCgame();
	int		checksum;
	mapFile_t	*bsp;

	CM_LoadMap( mapname, qtrue, &checksum );

	if ( app->cgameBsp ) {
		Map_Free( app->cgameBsp );
		app->cgameBsp = NULL;
	}

	if ( !Map_Load( mapname, &bsp, MAP_LOAD_FLAGS_NONE ) ) {
		Com_Terminate( TERM_CLIENT_DROP, "%s: couldn't load %s", __func__, mapname );
	}
	app->cgameBsp = bsp;
}


/*
====================
CL_ShutdownCGame

====================
*/
void CL_ShutdownCGame( clientApp_t *app ) {
	// Teardown twin of CL_InitCGame: the input-focused app owns the host-global
	// singletons (key catcher, the H_CGAME file-VM table); a non-focused app's
	// teardown does the VM work but must not touch those.
	qboolean isFocused = ( app == clientActiveApp );

	if ( isFocused ) {
		Key_SetCatcher( Key_GetCatcher( ) & ~KEYCATCH_CGAME );
	}
	app->cgameStarted = qfalse;
	/* The aim channel is owned by the live cgame frame. Never let a staged
	 * third-person direction survive disconnect, map teardown, or VM restart. */
	app->cl.cgameAimMode = UCMD_AIM_NONE;
	app->cl.cgameAimAngles[PITCH] = 0;
	app->cl.cgameAimAngles[YAW] = 0;

	if ( !app->cgvm ) {
		return;
	}

	re.VertexLighting( qfalse );

	vm_t *shuttingDown = app->cgvm;

	VM_Call( shuttingDown, 0, CG_SHUTDOWN );

	// The cgame-VM resources this used to sweep manually — viewport providers
	// (owner-keyed), the focused-app H_CGAME file table, and the cgame commands
	// (owner-keyed) — are now freed by VM_Free via the teardown callbacks
	// registered in CL_InitCGame. CG_SHUTDOWN (above) still unregisters main_scene
	// cgame-side first; the owner-keyed viewport callback then catches non-LEVEL
	// leftovers (idempotent — skips inactive slots — so it never double-frees the
	// LIFETIME_LEVEL path swept separately by CL_ShutdownLevel). The file callback
	// keeps the focused-only gate (registered only when this app was focused).
	VM_Free( shuttingDown );
	app->cgvm = NULL;

	if ( app->cgameBsp ) {
		Map_Free( app->cgameBsp );
		app->cgameBsp = NULL;
	}
}


static int FloatAsInt( float f ) {
	floatint_t fi;
	fi.f = f;
	return fi.i;
}

typedef struct {
	uint32_t glconfigGeneration;
	qboolean initialized;
	qboolean capabilityLogged;
	uint32_t slotGeneration[MAX_GENTITIES][REF_ENTITY_MOTION_ROLE_COUNT];
} clTemporalIngressReceiptState_t;

static clTemporalIngressReceiptState_t s_temporalIngressReceipts;

static void CL_TemporalIngressReceiptGeneration( void ) {
	uint32_t generation = (uint32_t)cls.glconfigGeneration;
	if ( !s_temporalIngressReceipts.initialized
			|| s_temporalIngressReceipts.glconfigGeneration != generation ) {
		memset( &s_temporalIngressReceipts, 0, sizeof( s_temporalIngressReceipts ) );
		s_temporalIngressReceipts.initialized = qtrue;
		s_temporalIngressReceipts.glconfigGeneration = generation;
	}
}


static void *VM_ArgPtr( intptr_t intValue ) {

	if ( !intValue || VM_ActiveNativeVM() == NULL )
	  return NULL;

	if ( VM_ActiveNativeVM()->entryPoint )
		return (void *)(intValue);
	return (void *)( VM_ActiveNativeVM()->dataBase + ( intValue & VM_ActiveNativeVM()->dataMask ) );
}


static qboolean CL_GetValue( char* value, int valueSize, const char* key ) {
	if ( !Q_stricmp( key, "trap_R_AddRefEntityToSceneTemporal" )
			&& re.AddRefEntityToSceneTemporal ) {
		CL_TemporalIngressReceiptGeneration();
		Com_sprintf( value, valueSize, "%i", CG_R_ADDREFENTITYTOSCENETEMPORAL );
		if ( !s_temporalIngressReceipts.capabilityLogged ) {
			s_temporalIngressReceipts.capabilityLogged = qtrue;
			Com_Log( SEV_INFO, LOG_CH(ch_cgame),
				"temporal-entity-capability glconfig-generation=%u key=trap_R_AddRefEntityToSceneTemporal expected=232 discovered=232 route=native-syscall export=1\n",
				s_temporalIngressReceipts.glconfigGeneration );
		}
		return qtrue;
	}

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
	// No-op: this used to force the retired r_dlightMode into its 1..2 (per-pixel)
	// range to keep clients off the VQ3 'fake' dynamic-light tier. That tier is gone —
	// r_dynamiclight is per-pixel-only now (0 off / 1 world / 2 world+models) — so there
	// is no fake mode to force away from. The cgame syscall binding is kept so older
	// cgame modules that still request it resolve harmlessly.
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
Typed-IPC descriptor catalogue — cgame (docs/vm-typed-ipc-design.md, shape A)

Mirrors the game catalogue (sv_game.c) for the hot cgame syscalls. Same flat wire,
same VM_UnmarshalTyped helper; the only boundary difference is that the cgame VM is
resolved per-app via VM_ActiveNativeVM() (not a global gvm), so the argptr adapter
and the unmarshal pass that VM.
====================
*/

// CG_LOG( severity:int, channel:ptr, text:ptr )
static const vmSyscallDesc_t cl_desc_CG_LOG = {
	CG_LOG, "CG_LOG", 3, { VARG_INT, VARG_VMPTR, VARG_VMPTR }
};
// CG_PRINT( text:ptr )
static const vmSyscallDesc_t cl_desc_CG_PRINT = {
	CG_PRINT, "CG_PRINT", 1, { VARG_VMPTR }
};
// CG_ERROR( text:ptr )
static const vmSyscallDesc_t cl_desc_CG_ERROR = {
	CG_ERROR, "CG_ERROR", 1, { VARG_VMPTR }
};
// CG_MILLISECONDS( void ) takes no args → no descriptor (typed unmarshal would be a
// no-op; bare Sys_Milliseconds() handler stays hot-path-cheap, W-41).

// ── cvar subsystem (mirrors sv_game.c; cgame has no integer-value variant) ────
// CG_CVAR_REGISTER( vmCvar*:ptr, varName:ptr, default:ptr, flags:int ) — vmCvar is
// a VM-memory write-back struct ptr (plain VMPTR, may be NULL), per the game side.
static const vmSyscallDesc_t cl_desc_CG_CVAR_REGISTER = {
	CG_CVAR_REGISTER, "CG_CVAR_REGISTER", 4, { VARG_VMPTR, VARG_VMPTR, VARG_VMPTR, VARG_INT }
};
// CG_CVAR_UPDATE( vmCvar*:ptr )
static const vmSyscallDesc_t cl_desc_CG_CVAR_UPDATE = {
	CG_CVAR_UPDATE, "CG_CVAR_UPDATE", 1, { VARG_VMPTR }
};
// CG_CVAR_SET( var_name:ptr, value:ptr )
static const vmSyscallDesc_t cl_desc_CG_CVAR_SET = {
	CG_CVAR_SET, "CG_CVAR_SET", 2, { VARG_VMPTR, VARG_VMPTR }
};
// CG_CVAR_VARIABLESTRINGBUFFER( var_name:ptr, buffer:ptr[bufsize], bufsize:int ) —
// the sized output ptr (VARG_VMPTR_SIZED bounds-checks against bufsize).
static const vmSyscallDesc_t cl_desc_CG_CVAR_VARIABLESTRINGBUFFER = {
	CG_CVAR_VARIABLESTRINGBUFFER, "CG_CVAR_VARIABLESTRINGBUFFER", 3,
	{ VARG_VMPTR, VARG_VMPTR_SIZED, VARG_INT }
};
// CG_L10N_GET( key:ptr, buffer:ptr[bufsize], bufsize:int ) — resolve an l10n key
// to localized text (WiredL10n_Get) into the sized output buffer (VARG_VMPTR_SIZED
// bounds-checks against bufsize). Same shape as CG_CVAR_VARIABLESTRINGBUFFER.
static const vmSyscallDesc_t cl_desc_CG_L10N_GET = {
	CG_L10N_GET, "CG_L10N_GET", 3,
	{ VARG_VMPTR, VARG_VMPTR_SIZED, VARG_INT }
};
// CG_CVAR_SETDESCRIPTION( var_name:ptr, description:ptr )
static const vmSyscallDesc_t cl_desc_CG_CVAR_SETDESCRIPTION = {
	CG_CVAR_SETDESCRIPTION, "CG_CVAR_SETDESCRIPTION", 2, { VARG_VMPTR, VARG_VMPTR }
};

// ── cmd / args subsystem ─────────────────────────────────────────────────────
// CG_ARGC( void ) stays bare (0-arg, no descriptor; W-41).
// CG_ARGV( n:int, buffer:ptr[bufferLength], bufferLength:int ) — sized output buffer.
static const vmSyscallDesc_t cl_desc_CG_ARGV = {
	CG_ARGV, "CG_ARGV", 3, { VARG_INT, VARG_VMPTR_SIZED, VARG_INT }
};
// CG_ARGS( buffer:ptr[bufsize], bufsize:int ) — the full args string into a sized
// buffer (Cmd_ArgsBuffer), NOT a return-pointer; VARG_VMPTR_SIZED reads bufsize as len.
static const vmSyscallDesc_t cl_desc_CG_ARGS = {
	CG_ARGS, "CG_ARGS", 2, { VARG_VMPTR_SIZED, VARG_INT }
};
// CG_SENDCONSOLECOMMAND( text:ptr ) — single text ptr (no leading int, unlike the
// game's G_SEND_CONSOLE_COMMAND); Cbuf_NestedAdd reads the VM's command string.
static const vmSyscallDesc_t cl_desc_CG_SENDCONSOLECOMMAND = {
	CG_SENDCONSOLECOMMAND, "CG_SENDCONSOLECOMMAND", 1, { VARG_VMPTR }
};
// CG_SENDCLIENTCOMMAND( text:ptr )
static const vmSyscallDesc_t cl_desc_CG_SENDCLIENTCOMMAND = {
	CG_SENDCLIENTCOMMAND, "CG_SENDCLIENTCOMMAND", 1, { VARG_VMPTR }
};

// ── fs subsystem (mirrors sv_game.c; cgame has no GETFILELIST, and CG_FS_READ has
// no UrT early-return guard and returns 0 rather than the byte count) ─────────────
// CG_FS_FOPENFILE( qpath:ptr, file:ptr write-back, mode:int )
static const vmSyscallDesc_t cl_desc_CG_FS_FOPENFILE = {
	CG_FS_FOPENFILE, "CG_FS_FOPENFILE", 3, { VARG_VMPTR, VARG_VMPTR, VARG_INT }
};
// CG_FS_READ( buffer:ptr[len], len:int, f:int )
static const vmSyscallDesc_t cl_desc_CG_FS_READ = {
	CG_FS_READ, "CG_FS_READ", 3, { VARG_VMPTR_SIZED, VARG_INT, VARG_INT }
};
// CG_FS_WRITE( buffer:ptr[len], len:int, f:int )
static const vmSyscallDesc_t cl_desc_CG_FS_WRITE = {
	CG_FS_WRITE, "CG_FS_WRITE", 3, { VARG_VMPTR_SIZED, VARG_INT, VARG_INT }
};
// CG_FS_FCLOSEFILE( f:int )
static const vmSyscallDesc_t cl_desc_CG_FS_FCLOSEFILE = {
	CG_FS_FCLOSEFILE, "CG_FS_FCLOSEFILE", 1, { VARG_INT }
};
// CG_FS_SEEK( f:int, offset:int, origin:int ) — long offset passed as one i32, see game side.
static const vmSyscallDesc_t cl_desc_CG_FS_SEEK = {
	CG_FS_SEEK, "CG_FS_SEEK", 3, { VARG_INT, VARG_INT, VARG_INT }
};

// ── collision-model subsystem ────────────────────────────────────────────────
// As on the game side, vec3_t coordinates cross as VM POINTERS (VMA), the trace_t*
// result is a VMPTR write-back, and clipHandle/contentmask/passEnt are ints — no
// VARG_FLOAT. (CG_CM_MARKFRAGMENTS uses VARG_VMPTR_COUNTED for its 3 count×size arrays
// — see its descriptor below.)
// CG_CM_LOADMAP( mapname:ptr )
static const vmSyscallDesc_t cl_desc_CG_CM_LOADMAP = {
	CG_CM_LOADMAP, "CG_CM_LOADMAP", 1, { VARG_VMPTR }
};
// CG_CM_INLINEMODEL( index:int )
static const vmSyscallDesc_t cl_desc_CG_CM_INLINEMODEL = {
	CG_CM_INLINEMODEL, "CG_CM_INLINEMODEL", 1, { VARG_INT }
};
// CG_CM_TEMPBOXMODEL/CAPSULE( mins:ptr, maxs:ptr )
static const vmSyscallDesc_t cl_desc_CG_CM_TEMPBOXMODEL = {
	CG_CM_TEMPBOXMODEL, "CG_CM_TEMPBOXMODEL", 2, { VARG_VMPTR, VARG_VMPTR }
};
static const vmSyscallDesc_t cl_desc_CG_CM_TEMPCAPSULEMODEL = {
	CG_CM_TEMPCAPSULEMODEL, "CG_CM_TEMPCAPSULEMODEL", 2, { VARG_VMPTR, VARG_VMPTR }
};
// CG_CM_POINTCONTENTS( point:ptr, model:int )
static const vmSyscallDesc_t cl_desc_CG_CM_POINTCONTENTS = {
	CG_CM_POINTCONTENTS, "CG_CM_POINTCONTENTS", 2, { VARG_VMPTR, VARG_INT }
};
// CG_CM_TRANSFORMEDPOINTCONTENTS( point:ptr, model:int, origin:ptr, angles:ptr )
static const vmSyscallDesc_t cl_desc_CG_CM_TRANSFORMEDPOINTCONTENTS = {
	CG_CM_TRANSFORMEDPOINTCONTENTS, "CG_CM_TRANSFORMEDPOINTCONTENTS", 4,
	{ VARG_VMPTR, VARG_INT, VARG_VMPTR, VARG_VMPTR }
};
// CG_CM_BOXTRACE/CAPSULE( results:ptr, start:ptr, end:ptr, mins:ptr, maxs:ptr, model:int, mask:int )
static const vmSyscallDesc_t cl_desc_CG_CM_BOXTRACE = {
	CG_CM_BOXTRACE, "CG_CM_BOXTRACE", 7,
	{ VARG_VMPTR, VARG_VMPTR, VARG_VMPTR, VARG_VMPTR, VARG_VMPTR, VARG_INT, VARG_INT }
};
static const vmSyscallDesc_t cl_desc_CG_CM_CAPSULETRACE = {
	CG_CM_CAPSULETRACE, "CG_CM_CAPSULETRACE", 7,
	{ VARG_VMPTR, VARG_VMPTR, VARG_VMPTR, VARG_VMPTR, VARG_VMPTR, VARG_INT, VARG_INT }
};
// CG_CM_TRANSFORMEDBOXTRACE/CAPSULE( results, start, end, mins, maxs, model:int, mask:int, origin, angles )
static const vmSyscallDesc_t cl_desc_CG_CM_TRANSFORMEDBOXTRACE = {
	CG_CM_TRANSFORMEDBOXTRACE, "CG_CM_TRANSFORMEDBOXTRACE", 9,
	{ VARG_VMPTR, VARG_VMPTR, VARG_VMPTR, VARG_VMPTR, VARG_VMPTR, VARG_INT, VARG_INT, VARG_VMPTR, VARG_VMPTR }
};
static const vmSyscallDesc_t cl_desc_CG_CM_TRANSFORMEDCAPSULETRACE = {
	CG_CM_TRANSFORMEDCAPSULETRACE, "CG_CM_TRANSFORMEDCAPSULETRACE", 9,
	{ VARG_VMPTR, VARG_VMPTR, VARG_VMPTR, VARG_VMPTR, VARG_VMPTR, VARG_INT, VARG_INT, VARG_VMPTR, VARG_VMPTR }
};
// CG_CM_MARKFRAGMENTS: re.MarkFragments( numPoints, points[numPoints], projection,
// maxPoints, pointBuffer[maxPoints], numFragments, fragmentBuffer[numFragments] ).
// Three VARG_VMPTR_COUNTED arrays (the count×size case): points = vec3×numPoints
// (count from arg 1), pointBuffer = vec3×maxPoints (count from arg 4), fragmentBuffer
// = markFragment_t×numFragments (count from arg 6). The bounds-checks (VM_CheckBounds3)
// move into VM_UnmarshalTyped; the projection is a single vec3 (plain VARG_VMPTR).
static const vmSyscallDesc_t cl_desc_CG_CM_MARKFRAGMENTS = {
	CG_CM_MARKFRAGMENTS, "CG_CM_MARKFRAGMENTS", 7,
	{ VARG_INT, VARG_VMPTR_COUNTED, VARG_VMPTR, VARG_INT, VARG_VMPTR_COUNTED, VARG_INT, VARG_VMPTR_COUNTED },
	{ [1] = { sizeof( vec3_t ), 1 },          // points: count = args[1] (numPoints)
	  [4] = { sizeof( vec3_t ), 4 },          // pointBuffer: count = args[4] (maxPoints)
	  [6] = { sizeof( markFragment_t ), 6 } } // fragmentBuffer: count = args[6] (numFragments)
};

// ── sound (CG_S_*) + render (CG_R_*) subsystem ───────────────────────────────
// vec3 origins/velocity/axis are VM POINTERS (cm precedent), struct args
// (refEntity_t/refdef_t/fontInfo_t/poly verts) are plain VARG_VMPTR (write-back
// included — the translation, not the direction). CG_R_* is the second VARG_FLOAT
// user: light intensity/colour and normalized draw coords pass as spread VMF floats.
// ── sound ──
static const vmSyscallDesc_t cl_desc_CG_S_STARTSOUND = {  // origin may be NULL
	CG_S_STARTSOUND, "CG_S_STARTSOUND", 4, { VARG_VMPTR, VARG_INT, VARG_INT, VARG_INT }
};
static const vmSyscallDesc_t cl_desc_CG_S_ADDLOOPINGSOUND = {
	CG_S_ADDLOOPINGSOUND, "CG_S_ADDLOOPINGSOUND", 4, { VARG_INT, VARG_VMPTR, VARG_VMPTR, VARG_INT }
};
static const vmSyscallDesc_t cl_desc_CG_S_ADDREALLOOPINGSOUND = {
	CG_S_ADDREALLOOPINGSOUND, "CG_S_ADDREALLOOPINGSOUND", 4, { VARG_INT, VARG_VMPTR, VARG_VMPTR, VARG_INT }
};
static const vmSyscallDesc_t cl_desc_CG_S_UPDATEENTITYPOSITION = {
	CG_S_UPDATEENTITYPOSITION, "CG_S_UPDATEENTITYPOSITION", 2, { VARG_INT, VARG_VMPTR }
};
static const vmSyscallDesc_t cl_desc_CG_S_RESPATIALIZE = {
	CG_S_RESPATIALIZE, "CG_S_RESPATIALIZE", 4, { VARG_INT, VARG_VMPTR, VARG_VMPTR, VARG_INT }
};
static const vmSyscallDesc_t cl_desc_CG_S_REGISTERSOUND = {
	CG_S_REGISTERSOUND, "CG_S_REGISTERSOUND", 2, { VARG_VMPTR, VARG_INT }
};
static const vmSyscallDesc_t cl_desc_CG_S_STARTBACKGROUNDTRACK = {
	CG_S_STARTBACKGROUNDTRACK, "CG_S_STARTBACKGROUNDTRACK", 2, { VARG_VMPTR, VARG_VMPTR }
};
// ── render ──
static const vmSyscallDesc_t cl_desc_CG_R_REGISTERMODEL = {
	CG_R_REGISTERMODEL, "CG_R_REGISTERMODEL", 1, { VARG_VMPTR }
};
static const vmSyscallDesc_t cl_desc_CG_R_REGISTERSKIN = {
	CG_R_REGISTERSKIN, "CG_R_REGISTERSKIN", 1, { VARG_VMPTR }
};
static const vmSyscallDesc_t cl_desc_CG_R_REGISTERSHADER = {
	CG_R_REGISTERSHADER, "CG_R_REGISTERSHADER", 1, { VARG_VMPTR }
};
static const vmSyscallDesc_t cl_desc_CG_R_REGISTERSHADERNOMIP = {
	CG_R_REGISTERSHADERNOMIP, "CG_R_REGISTERSHADERNOMIP", 1, { VARG_VMPTR }
};
static const vmSyscallDesc_t cl_desc_CG_R_REGISTERPRIMITIVESHADER = {
	CG_R_REGISTERPRIMITIVESHADER, "CG_R_REGISTERPRIMITIVESHADER", 1, { VARG_VMPTR }
};
static const vmSyscallDesc_t cl_desc_CG_R_REGISTERFONT = {  // fontInfo_t* (arg 3) write-back
	CG_R_REGISTERFONT, "CG_R_REGISTERFONT", 3, { VARG_VMPTR, VARG_INT, VARG_VMPTR }
};
static const vmSyscallDesc_t cl_desc_CG_R_ADDREFENTITYTOSCENE = {
	CG_R_ADDREFENTITYTOSCENE, "CG_R_ADDREFENTITYTOSCENE", 1, { VARG_VMPTR }
};
static const vmSyscallDesc_t cl_desc_CG_R_ADDREFENTITYTOSCENE2 = {
	CG_R_ADDREFENTITYTOSCENE2, "CG_R_ADDREFENTITYTOSCENE2", 1, { VARG_VMPTR }
};
static const vmSyscallDesc_t cl_desc_CG_R_ADDREFENTITYTOSCENETEMPORAL = {
	CG_R_ADDREFENTITYTOSCENETEMPORAL, "CG_R_ADDREFENTITYTOSCENETEMPORAL", 2,
	{ VARG_VMPTR, VARG_VMPTR }
};
static const vmSyscallDesc_t cl_desc_CG_R_ADDPOLYTOSCENE = {
	CG_R_ADDPOLYTOSCENE, "CG_R_ADDPOLYTOSCENE", 3, { VARG_INT, VARG_INT, VARG_VMPTR }
};
static const vmSyscallDesc_t cl_desc_CG_R_ADDPOLYSTOSCENE = {
	CG_R_ADDPOLYSTOSCENE, "CG_R_ADDPOLYSTOSCENE", 4, { VARG_INT, VARG_INT, VARG_VMPTR, VARG_INT }
};
static const vmSyscallDesc_t cl_desc_CG_R_LIGHTFORPOINT = {
	CG_R_LIGHTFORPOINT, "CG_R_LIGHTFORPOINT", 4, { VARG_VMPTR, VARG_VMPTR, VARG_VMPTR, VARG_VMPTR }
};
static const vmSyscallDesc_t cl_desc_CG_R_ADDLIGHTTOSCENE = {   // VMF(2..5) — VARG_FLOAT
	CG_R_ADDLIGHTTOSCENE, "CG_R_ADDLIGHTTOSCENE", 5, { VARG_VMPTR, VARG_FLOAT, VARG_FLOAT, VARG_FLOAT, VARG_FLOAT }
};
static const vmSyscallDesc_t cl_desc_CG_R_ADDADDITIVELIGHTTOSCENE = {  // VMF(2..5)
	CG_R_ADDADDITIVELIGHTTOSCENE, "CG_R_ADDADDITIVELIGHTTOSCENE", 5, { VARG_VMPTR, VARG_FLOAT, VARG_FLOAT, VARG_FLOAT, VARG_FLOAT }
};
static const vmSyscallDesc_t cl_desc_CG_R_ADDLINEARLIGHTTOSCENE = {  // VMF(3..6)
	CG_R_ADDLINEARLIGHTTOSCENE, "CG_R_ADDLINEARLIGHTTOSCENE", 6,
	{ VARG_VMPTR, VARG_VMPTR, VARG_FLOAT, VARG_FLOAT, VARG_FLOAT, VARG_FLOAT }
};
static const vmSyscallDesc_t cl_desc_CG_R_ADDBEAMTOSCENE = {
	CG_R_ADDBEAMTOSCENE, "CG_R_ADDBEAMTOSCENE", 1, { VARG_VMPTR }
};
static const vmSyscallDesc_t cl_desc_CG_R_ADDRAILRIBBONTOSCENE = {
	CG_R_ADDRAILRIBBONTOSCENE, "CG_R_ADDRAILRIBBONTOSCENE", 1, { VARG_VMPTR }
};
static const vmSyscallDesc_t cl_desc_CG_R_ADDSPRITETOSCENE = {
	CG_R_ADDSPRITETOSCENE, "CG_R_ADDSPRITETOSCENE", 1, { VARG_VMPTR }
};
static const vmSyscallDesc_t cl_desc_CG_R_EMITPARTICLES = {
	CG_R_EMITPARTICLES, "CG_R_EMITPARTICLES", 1, { VARG_VMPTR }
};
static const vmSyscallDesc_t cl_desc_CG_R_ADDDECALTOSCENE = {
	CG_R_ADDDECALTOSCENE, "CG_R_ADDDECALTOSCENE", 1, { VARG_VMPTR }
};
// lensSourceDesc_t is flat (no pointer field) → crosses as a plain bounds-checked
// struct ptr, like decalDesc_t / atmosphericDesc_t.
static const vmSyscallDesc_t cl_desc_CG_R_ADDLENSSOURCETOSCENE = {
	CG_R_ADDLENSSOURCETOSCENE, "CG_R_ADDLENSSOURCETOSCENE", 1, { VARG_VMPTR }
};
// id in, a single float written back through outVis (plain VARG_VMPTR write-back,
// like the vec3/refEntity out-params; the renderer writes *outVis in VM memory).
static const vmSyscallDesc_t cl_desc_CG_R_GETLENSVISIBILITY = {
	CG_R_GETLENSVISIBILITY, "CG_R_GETLENSVISIBILITY", 2, { VARG_INT, VARG_VMPTR }
};
// haloDesc_t is flat (no pointer field) → one bounds-checked struct ptr; the
// dispatch case unpacks it into the renderer's scalar AddHaloToScene signature.
#if FEAT_HALO
static const vmSyscallDesc_t cl_desc_CG_R_ADDHALOTOSCENE = {
	CG_R_ADDHALOTOSCENE, "CG_R_ADDHALOTOSCENE", 1, { VARG_VMPTR }
};
#endif
static const vmSyscallDesc_t cl_desc_CG_R_REGISTERPARTICLECLASS = {
	CG_R_REGISTERPARTICLECLASS, "CG_R_REGISTERPARTICLECLASS", 2, { VARG_INT, VARG_VMPTR }
};
// atmosphericDesc_t is flat (no pointer field) so it crosses as a plain struct ptr.
static const vmSyscallDesc_t cl_desc_CG_R_SETATMOSPHERE = {
	CG_R_SETATMOSPHERE, "CG_R_SETATMOSPHERE", 1, { VARG_VMPTR }
};
// The heightgrid ships as a counted float array: the grid pointer is the first
// arg (args[1]) and its element count is the second (args[2]). VM_UnmarshalTyped
// bounds-checks the array against VM memory before translating — the only safe way
// to hand the renderer a cgame-owned buffer (a struct-nested pointer would NOT be
// address-translated). The grid is the VARG_VMPTR_COUNTED at arg-index 0; its count
// is read from wire args[2], so countArg is 2 (contrast MarkFragments, whose count
// int precedes each counted array).
static const vmSyscallDesc_t cl_desc_CG_R_SETATMOSPHEREHEIGHTGRID = {
	CG_R_SETATMOSPHEREHEIGHTGRID, "CG_R_SETATMOSPHEREHEIGHTGRID", 2, { VARG_VMPTR_COUNTED, VARG_INT },
	{ [0] = { sizeof( float ), 2 } }   // grid: count = args[2] (float count)
};
static const vmSyscallDesc_t cl_desc_CG_R_ADDATMOSPHEREEMITTER = {
	CG_R_ADDATMOSPHEREEMITTER, "CG_R_ADDATMOSPHEREEMITTER", 1, { VARG_VMPTR }
};
static const vmSyscallDesc_t cl_desc_CG_R_REGISTERATMOSPHEREEFFECTPROFILE = {
	CG_R_REGISTERATMOSPHEREEFFECTPROFILE,
	"CG_R_REGISTERATMOSPHEREEFFECTPROFILE", 2, { VARG_INT, VARG_VMPTR }
};
static const vmSyscallDesc_t cl_desc_CG_R_ADDATMOSPHERESURFACEEVENT = {
	CG_R_ADDATMOSPHERESURFACEEVENT,
	"CG_R_ADDATMOSPHERESURFACEEVENT", 1, { VARG_VMPTR }
};
static const vmSyscallDesc_t cl_desc_CG_R_ADDATMOSPHEREMEDIAVOLUME = {
	CG_R_ADDATMOSPHEREMEDIAVOLUME,
	"CG_R_ADDATMOSPHEREMEDIAVOLUME", 1, { VARG_VMPTR }
};
static const vmSyscallDesc_t cl_desc_CG_WIRED_FX_EMIT_EVENT = {
	CG_WIRED_FX_EMIT_EVENT, "CG_WIRED_FX_EMIT_EVENT", 1, { VARG_VMPTR }
};
static const vmSyscallDesc_t cl_desc_CG_R_REGISTERPARTICLECLASSNAMED = {
	CG_R_REGISTERPARTICLECLASSNAMED, "CG_R_REGISTERPARTICLECLASSNAMED", 3,
	{ VARG_INT, VARG_VMPTR, VARG_VMPTR }
};
static const vmSyscallDesc_t cl_desc_CG_R_RENDERSCENE = {   // refdef_t* (write-back: clip applied in place)
	CG_R_RENDERSCENE, "CG_R_RENDERSCENE", 1, { VARG_VMPTR }
};
static const vmSyscallDesc_t cl_desc_CG_R_SETCOLOR = {     // rgba float[4] passed as a pointer
	CG_R_SETCOLOR, "CG_R_SETCOLOR", 1, { VARG_VMPTR }
};
static const vmSyscallDesc_t cl_desc_CG_R_DRAWSTRETCHPICNORM = {  // VMF(1..8) — VARG_FLOAT
	CG_R_DRAWSTRETCHPICNORM, "CG_R_DRAWSTRETCHPICNORM", 9,
	{ VARG_FLOAT, VARG_FLOAT, VARG_FLOAT, VARG_FLOAT, VARG_FLOAT, VARG_FLOAT, VARG_FLOAT, VARG_FLOAT, VARG_INT }
};
static const vmSyscallDesc_t cl_desc_CG_R_MODELBOUNDS = {  // mins/maxs out-vecs (write-back VMPTR)
	CG_R_MODELBOUNDS, "CG_R_MODELBOUNDS", 3, { VARG_INT, VARG_VMPTR, VARG_VMPTR }
};
static const vmSyscallDesc_t cl_desc_CG_R_LERPTAG = {     // VMF(5) frac
	CG_R_LERPTAG, "CG_R_LERPTAG", 6, { VARG_VMPTR, VARG_INT, VARG_INT, VARG_INT, VARG_FLOAT, VARG_VMPTR }
};
static const vmSyscallDesc_t cl_desc_CG_R_REMAP_SHADER = {
	CG_R_REMAP_SHADER, "CG_R_REMAP_SHADER", 3, { VARG_VMPTR, VARG_VMPTR, VARG_VMPTR }
};
static const vmSyscallDesc_t cl_desc_CG_R_INPVS = {
	CG_R_INPVS, "CG_R_INPVS", 2, { VARG_VMPTR, VARG_VMPTR }
};
static const vmSyscallDesc_t cl_desc_CG_R_GETIQMANIMS = {
	CG_R_GETIQMANIMS, "CG_R_GETIQMANIMS", 3, { VARG_INT, VARG_VMPTR, VARG_INT }
};
static const vmSyscallDesc_t cl_desc_CG_R_GETMDLANIMS = {
	CG_R_GETMDLANIMS, "CG_R_GETMDLANIMS", 3, { VARG_INT, VARG_VMPTR, VARG_INT }
};
static const vmSyscallDesc_t cl_desc_CG_R_SETLIGHTSTYLEPATTERN = {
	CG_R_SETLIGHTSTYLEPATTERN, "CG_R_SETLIGHTSTYLEPATTERN", 2, { VARG_INT, VARG_VMPTR }
};
static const vmSyscallDesc_t cl_desc_CG_R_ADDRIBBONTOSCENE = {  // points VMPTR + 3 ints
	CG_R_ADDRIBBONTOSCENE, "CG_R_ADDRIBBONTOSCENE", 4, { VARG_VMPTR, VARG_INT, VARG_INT, VARG_INT }
};
static const vmSyscallDesc_t cl_desc_CG_R_DRAWTEXTNORM = {  // VMF(2,3,5) coords/size
	CG_R_DRAWTEXTNORM, "CG_R_DRAWTEXTNORM", 8,
	{ VARG_VMPTR, VARG_FLOAT, VARG_FLOAT, VARG_INT, VARG_FLOAT, VARG_VMPTR, VARG_INT, VARG_INT }
};
static const vmSyscallDesc_t cl_desc_CG_R_MEASURETEXTNORM = {  // VMF(3) size
	CG_R_MEASURETEXTNORM, "CG_R_MEASURETEXTNORM", 3, { VARG_VMPTR, VARG_INT, VARG_FLOAT }
};
// CG_GET_ENTITY_TOKEN: a renderer call (re.GetEntityToken) with a sized output buffer.
static const vmSyscallDesc_t cl_desc_CG_GET_ENTITY_TOKEN = {
	CG_GET_ENTITY_TOKEN, "CG_GET_ENTITY_TOKEN", 2, { VARG_VMPTR_SIZED, VARG_INT }
};

// Adapter so VM_UnmarshalTyped can call the file-static VM_ArgPtr translator.
static void *CL_TypedArgPtr( intptr_t v ) {
	return VM_ArgPtr( v );
}

// Unmarshal a migrated cgame syscall into `out` via its descriptor; a validation
// failure is a hard error. The shipping path uses the typed values directly.
static void CL_UnmarshalCgame( const vmSyscallDesc_t *desc, intptr_t *args, vmTypedArg_t *out ) {
	if ( !VM_UnmarshalTyped( VM_ActiveNativeVM(), desc, args, CL_TypedArgPtr, out ) ) {
		Com_Terminate( TERM_CLIENT_DROP, "%s: bad typed syscall args", desc->name );
	}
}

#if defined(_DEBUG)
// Generic descriptor-driven parity for syscalls whose args are plain VARG_VMPTR /
// VARG_INT / VARG_FLOAT (no special semantics) — walks the typed args by type. Used
// for the collision-model queries. (Mirrors SV_TypedParityWalk on the game side.)
static void CL_TypedParityWalk( int argc, intptr_t *args, const vmTypedArg_t *t, const char *name ) {
	int a;
	for ( a = 0; a < argc; a++ ) {
		const intptr_t raw = args[ a + 1 ];
		switch ( t[a].type ) {
		case VARG_INT:
			if ( t[a].i != raw )
				Com_Terminate( TERM_CLIENT_DROP, "%s typed/opaque mismatch (int arg %d)", name, a );
			break;
		case VARG_VMPTR:
			if ( t[a].p != (void *)VM_ArgPtr( raw ) )
				Com_Terminate( TERM_CLIENT_DROP, "%s typed/opaque mismatch (ptr arg %d)", name, a );
			break;
		case VARG_FLOAT: {
			floatint_t fi; fi.i = (int)raw;
			if ( t[a].f != fi.f )
				Com_Terminate( TERM_CLIENT_DROP, "%s typed/opaque mismatch (float arg %d)", name, a );
			break;
		}
		case VARG_VMPTR_SIZED:
			if ( t[a].p != (void *)VM_ArgPtr( raw ) || t[a].len != (unsigned)args[ a + 2 ] )
				Com_Terminate( TERM_CLIENT_DROP, "%s typed/opaque mismatch (sized arg %d)", name, a );
			break;
		case VARG_VMPTR_COUNTED:
			// The generic walk has no descriptor (so no elemSize/countArg); verify the
			// pointer translation here — the byte-length (.len == count*elemSize) parity
			// is asserted per-syscall in CL_TypedParityCheck where the elemSize is known.
			if ( t[a].p != (void *)VM_ArgPtr( raw ) )
				Com_Terminate( TERM_CLIENT_DROP, "%s typed/opaque mismatch (counted arg %d)", name, a );
			break;
		default:
			break;
		}
	}
}

// Parity check (Debug only): assert the typed unmarshal reproduces the hand-written
// VMA(x)/cast for each migrated cgame syscall. Compiles out in Release.
static void CL_TypedParityCheck( int call, intptr_t *args, const vmTypedArg_t *t ) {
	switch ( call ) {
	case CG_LOG:
		if ( t[0].i != args[1] ||
		     t[1].p != (void *)VM_ArgPtr( args[2] ) ||
		     t[2].p != (void *)VM_ArgPtr( args[3] ) )
			Com_Terminate( TERM_CLIENT_DROP, "CG_LOG typed/opaque mismatch" );
		break;
	case CG_PRINT:
		if ( t[0].p != (void *)VM_ArgPtr( args[1] ) )
			Com_Terminate( TERM_CLIENT_DROP, "CG_PRINT typed/opaque mismatch" );
		break;
	case CG_ERROR:
		if ( t[0].p != (void *)VM_ArgPtr( args[1] ) )
			Com_Terminate( TERM_CLIENT_DROP, "CG_ERROR typed/opaque mismatch" );
		break;
	case CG_CVAR_REGISTER:
		if ( t[0].p != (void *)VM_ArgPtr( args[1] ) ||   // vmCvar* (may be NULL)
		     t[1].p != (void *)VM_ArgPtr( args[2] ) ||   // varName
		     t[2].p != (void *)VM_ArgPtr( args[3] ) ||   // default
		     t[3].i != args[4] )                          // flags
			Com_Terminate( TERM_CLIENT_DROP, "CG_CVAR_REGISTER typed/opaque mismatch" );
		break;
	case CG_CVAR_UPDATE:
		if ( t[0].p != (void *)VM_ArgPtr( args[1] ) )
			Com_Terminate( TERM_CLIENT_DROP, "CG_CVAR_UPDATE typed/opaque mismatch" );
		break;
	case CG_CVAR_SET:
	case CG_CVAR_SETDESCRIPTION:
		if ( t[0].p != (void *)VM_ArgPtr( args[1] ) ||
		     t[1].p != (void *)VM_ArgPtr( args[2] ) )
			Com_Terminate( TERM_CLIENT_DROP, "CG_CVAR_* (2-ptr) typed/opaque mismatch" );
		break;
	case CG_CVAR_VARIABLESTRINGBUFFER:
		if ( t[0].p != (void *)VM_ArgPtr( args[1] ) ||
		     t[1].p != (void *)VM_ArgPtr( args[2] ) ||
		     t[1].len != (unsigned)args[3] ||
		     t[2].i != args[3] )
			Com_Terminate( TERM_CLIENT_DROP, "CG_CVAR_VARIABLESTRINGBUFFER typed/opaque mismatch" );
		break;
	case CG_L10N_GET:
		if ( t[0].p != (void *)VM_ArgPtr( args[1] ) ||
		     t[1].p != (void *)VM_ArgPtr( args[2] ) ||
		     t[1].len != (unsigned)args[3] ||
		     t[2].i != args[3] )
			Com_Terminate( TERM_CLIENT_DROP, "CG_L10N_GET typed/opaque mismatch" );
		break;
	case CG_ARGV:
		// n INT, buffer VMPTR_SIZED (ptr+len at args[2]/args[3]), bufferLength INT.
		if ( t[0].i != args[1] ||
		     t[1].p != (void *)VM_ArgPtr( args[2] ) ||
		     t[1].len != (unsigned)args[3] ||
		     t[2].i != args[3] )
			Com_Terminate( TERM_CLIENT_DROP, "CG_ARGV typed/opaque mismatch" );
		break;
	case CG_ARGS:
		// buffer VMPTR_SIZED (ptr+len at args[1]/args[2]), bufsize INT.
		if ( t[0].p != (void *)VM_ArgPtr( args[1] ) ||
		     t[0].len != (unsigned)args[2] ||
		     t[1].i != args[2] )
			Com_Terminate( TERM_CLIENT_DROP, "CG_ARGS typed/opaque mismatch" );
		break;
	case CG_SENDCONSOLECOMMAND:
	case CG_SENDCLIENTCOMMAND:
		if ( t[0].p != (void *)VM_ArgPtr( args[1] ) )
			Com_Terminate( TERM_CLIENT_DROP, "CG_SEND*COMMAND typed/opaque mismatch" );
		break;
	case CG_FS_FOPENFILE:
		// qpath VMPTR, fileHandle_t* write-back VMPTR, mode INT.
		if ( t[0].p != (void *)VM_ArgPtr( args[1] ) ||
		     t[1].p != (void *)VM_ArgPtr( args[2] ) ||
		     t[2].i != args[3] )
			Com_Terminate( TERM_CLIENT_DROP, "CG_FS_FOPENFILE typed/opaque mismatch" );
		break;
	case CG_FS_READ:
	case CG_FS_WRITE:
		// buffer VMPTR_SIZED (ptr+len at args[1]/args[2]), len INT, handle INT.
		if ( t[0].p != (void *)VM_ArgPtr( args[1] ) ||
		     t[0].len != (unsigned)args[2] ||
		     t[1].i != args[2] ||
		     t[2].i != args[3] )
			Com_Terminate( TERM_CLIENT_DROP, "CG_FS_READ/WRITE typed/opaque mismatch" );
		break;
	case CG_FS_FCLOSEFILE:
		if ( t[0].i != args[1] )
			Com_Terminate( TERM_CLIENT_DROP, "CG_FS_FCLOSEFILE typed/opaque mismatch" );
		break;
	case CG_FS_SEEK:
		if ( t[0].i != args[1] || t[1].i != args[2] || t[2].i != args[3] )
			Com_Terminate( TERM_CLIENT_DROP, "CG_FS_SEEK typed/opaque mismatch" );
		break;
	case CG_CM_MARKFRAGMENTS:
		// 3 VARG_VMPTR_COUNTED arrays + 3 INT counts + 1 VMPTR projection. Verify each
		// counted ptr translates AND its byte length == count*elemSize (the exact
		// VM_CheckBounds3 the hand path used: points/pointBuffer = vec3, frags = markFragment_t).
		if ( t[0].i != args[1] ||                                          // numPoints
		     t[1].p != (void *)VM_ArgPtr( args[2] ) ||                     // points ptr
		     t[1].len != (unsigned)args[1] * sizeof( vec3_t ) ||           // points byte len
		     t[2].p != (void *)VM_ArgPtr( args[3] ) ||                     // projection
		     t[3].i != args[4] ||                                          // maxPoints
		     t[4].p != (void *)VM_ArgPtr( args[5] ) ||                     // pointBuffer ptr
		     t[4].len != (unsigned)args[4] * sizeof( vec3_t ) ||           // pointBuffer byte len
		     t[5].i != args[6] ||                                          // numFragments
		     t[6].p != (void *)VM_ArgPtr( args[7] ) ||                     // fragmentBuffer ptr
		     t[6].len != (unsigned)args[6] * sizeof( markFragment_t ) )    // fragmentBuffer byte len
			Com_Terminate( TERM_CLIENT_DROP, "CG_CM_MARKFRAGMENTS typed/opaque mismatch" );
		break;
	// collision-model queries: all VARG_VMPTR (vec3/trace_t*) / VARG_INT, generic walk.
	case CG_CM_LOADMAP:                    CL_TypedParityWalk( 1, args, t, "CG_CM_LOADMAP" ); break;
	case CG_CM_INLINEMODEL:                CL_TypedParityWalk( 1, args, t, "CG_CM_INLINEMODEL" ); break;
	case CG_CM_TEMPBOXMODEL:               CL_TypedParityWalk( 2, args, t, "CG_CM_TEMPBOXMODEL" ); break;
	case CG_CM_TEMPCAPSULEMODEL:           CL_TypedParityWalk( 2, args, t, "CG_CM_TEMPCAPSULEMODEL" ); break;
	case CG_CM_POINTCONTENTS:              CL_TypedParityWalk( 2, args, t, "CG_CM_POINTCONTENTS" ); break;
	case CG_CM_TRANSFORMEDPOINTCONTENTS:   CL_TypedParityWalk( 4, args, t, "CG_CM_TRANSFORMEDPOINTCONTENTS" ); break;
	case CG_CM_BOXTRACE:                   CL_TypedParityWalk( 7, args, t, "CG_CM_BOXTRACE" ); break;
	case CG_CM_CAPSULETRACE:               CL_TypedParityWalk( 7, args, t, "CG_CM_CAPSULETRACE" ); break;
	case CG_CM_TRANSFORMEDBOXTRACE:        CL_TypedParityWalk( 9, args, t, "CG_CM_TRANSFORMEDBOXTRACE" ); break;
	case CG_CM_TRANSFORMEDCAPSULETRACE:    CL_TypedParityWalk( 9, args, t, "CG_CM_TRANSFORMEDCAPSULETRACE" ); break;
	default:
		break;
	}
}
#define CL_TYPED_PARITY( call, args, t )  CL_TypedParityCheck( (call), (args), (t) )
#define CL_TYPED_PARITY_WALK( argc, args, t, name )  CL_TypedParityWalk( (argc), (args), (t), (name) )
#else
#define CL_TYPED_PARITY( call, args, t )  ((void)0)
#define CL_TYPED_PARITY_WALK( argc, args, t, name )  ((void)0)
#endif

// Compact per-case unmarshal for the render/sound subsystem (mirrors SV_BOTLIB):
// declares the typed array `t`, unmarshals via the descriptor, runs the generic
// parity walk. Each migrated case is `{ CL_RSND(&cl_desc_X, N); <call with t[..]>; }`.
#define CL_RSND( descp, argc ) \
	vmTypedArg_t t[ VM_MAX_TYPED_ARGS ]; \
	CL_UnmarshalCgame( (descp), args, t ); \
	CL_TYPED_PARITY_WALK( (argc), args, t, (descp)->name )


/*
====================
CL_CgameSystemCalls

The cgame module is making a system call
====================
*/
static intptr_t CL_CgameSystemCalls( intptr_t *args ) {
	// ABI handshake (exact-match): a reserved-high id NOT in the CG_* enum returns
	// the engine's cgame ABI version, which the module exact-matches at init.
	if ( args[0] == VM_SYSCALL_ABI_QUERY ) {
		return CGAME_IMPORT_API_VERSION;
	}

	switch( args[0] ) {
	case CG_PRINT: {
		vmTypedArg_t t[ VM_MAX_TYPED_ARGS ];
		CL_UnmarshalCgame( &cl_desc_CG_PRINT, args, t );
		CL_TYPED_PARITY( CG_PRINT, args, t );
		Com_Log( SEV_INFO, LOG_CH(ch_cgame), "%s", (const char*)t[0].p );
		return 0;
	}
	case CG_ERROR: {
		vmTypedArg_t t[ VM_MAX_TYPED_ARGS ];
		CL_UnmarshalCgame( &cl_desc_CG_ERROR, args, t );
		CL_TYPED_PARITY( CG_ERROR, args, t );
		Com_Terminate( TERM_CLIENT_DROP, "%s", (const char*)t[0].p );
		return 0;
	}
	case CG_LOG: {
		vmTypedArg_t t[ VM_MAX_TYPED_ARGS ];
		CL_UnmarshalCgame( &cl_desc_CG_LOG, args, t );
		CL_TYPED_PARITY( CG_LOG, args, t );
		Com_Log( (log_severity_t)t[0].i, Log_GetChannel( (const char*)t[1].p ), "%s", (const char*)t[2].p );
		return 0;
	}
	case CG_TERMINATE:
		Com_Terminate( (terminationReason_t)args[1], "%s", (const char*)VMA(2) );
		return 0;
	case CG_MILLISECONDS:
		// 0-arg syscall: typed path is a no-op (nothing to translate). Descriptor
		// documents the signature; the hot timing path stays a bare call (W-41).
		return Sys_Milliseconds();
	case CG_CVAR_REGISTER: {
		vmTypedArg_t t[ VM_MAX_TYPED_ARGS ];
		CL_UnmarshalCgame( &cl_desc_CG_CVAR_REGISTER, args, t );
		CL_TYPED_PARITY( CG_CVAR_REGISTER, args, t );
		Cvar_VM_Register( t[0].p, t[1].p, t[2].p, (int)t[3].i, VM_ActiveNativeVM()->privateFlag );
		return 0;
	}
	case CG_CVAR_UPDATE: {
		vmTypedArg_t t[ VM_MAX_TYPED_ARGS ];
		CL_UnmarshalCgame( &cl_desc_CG_CVAR_UPDATE, args, t );
		CL_TYPED_PARITY( CG_CVAR_UPDATE, args, t );
		Cvar_Update( t[0].p, VM_ActiveNativeVM()->privateFlag );
		return 0;
	}
	case CG_CVAR_SET: {
		vmTypedArg_t t[ VM_MAX_TYPED_ARGS ];
		CL_UnmarshalCgame( &cl_desc_CG_CVAR_SET, args, t );
		CL_TYPED_PARITY( CG_CVAR_SET, args, t );
		Cvar_SetSafe( t[0].p, t[1].p );
		return 0;
	}
	case CG_CVAR_VARIABLESTRINGBUFFER: {
		// VARG_VMPTR_SIZED bounds-checks (buffer, bufsize) — the CVAR_PRIVATE flag
		// (not the VM privateFlag) matches the prior cgame handler exactly.
		vmTypedArg_t t[ VM_MAX_TYPED_ARGS ];
		CL_UnmarshalCgame( &cl_desc_CG_CVAR_VARIABLESTRINGBUFFER, args, t );
		CL_TYPED_PARITY( CG_CVAR_VARIABLESTRINGBUFFER, args, t );
		Cvar_VariableStringBufferSafe( t[0].p, t[1].p, (int)t[2].i, CVAR_PRIVATE );
		return 0;
	}
	case CG_L10N_GET: {
		// VARG_VMPTR_SIZED bounds-checks (buffer, bufsize); WiredL10n_Get never
		// returns NULL (missing key -> the key itself), and Q_strncpyz bounds the
		// copy into the cgame's sized buffer.
		vmTypedArg_t t[ VM_MAX_TYPED_ARGS ];
		CL_UnmarshalCgame( &cl_desc_CG_L10N_GET, args, t );
		CL_TYPED_PARITY( CG_L10N_GET, args, t );
		Q_strncpyz( t[1].p, WiredL10n_Get( t[0].p ), (int)t[2].i );
		return 0;
	}
	case CG_ARGC:
		// 0-arg: bare call (W-41), like CG_MILLISECONDS.
		return Cmd_Argc();
	case CG_ARGV: {
		vmTypedArg_t t[ VM_MAX_TYPED_ARGS ];
		CL_UnmarshalCgame( &cl_desc_CG_ARGV, args, t );
		CL_TYPED_PARITY( CG_ARGV, args, t );
		Cmd_ArgvBuffer( (int)t[0].i, t[1].p, (int)t[2].i );
		return 0;
	}
	case CG_ARGS: {
		vmTypedArg_t t[ VM_MAX_TYPED_ARGS ];
		CL_UnmarshalCgame( &cl_desc_CG_ARGS, args, t );
		CL_TYPED_PARITY( CG_ARGS, args, t );
		Cmd_ArgsBuffer( t[0].p, (int)t[1].i );
		return 0;
	}

	case CG_FS_FOPENFILE: {
		// fileHandle_t* (arg 2) is written back by the engine — plain VMPTR.
		vmTypedArg_t t[ VM_MAX_TYPED_ARGS ];
		CL_UnmarshalCgame( &cl_desc_CG_FS_FOPENFILE, args, t );
		CL_TYPED_PARITY( CG_FS_FOPENFILE, args, t );
		return FS_VM_OpenFile( t[0].p, t[1].p, (fsMode_t)t[2].i, H_CGAME );
	}
	case CG_FS_READ: {
		// VARG_VMPTR_SIZED bounds-checks (buffer, len). NB: unlike game's G_FS_READ
		// this has no UrT early-return and discards the byte count (returns 0).
		vmTypedArg_t t[ VM_MAX_TYPED_ARGS ];
		CL_UnmarshalCgame( &cl_desc_CG_FS_READ, args, t );
		CL_TYPED_PARITY( CG_FS_READ, args, t );
		FS_VM_ReadFile( t[0].p, (int)t[1].i, (fileHandle_t)t[2].i, H_CGAME );
		return 0;
	}
	case CG_FS_WRITE: {
		vmTypedArg_t t[ VM_MAX_TYPED_ARGS ];
		CL_UnmarshalCgame( &cl_desc_CG_FS_WRITE, args, t );
		CL_TYPED_PARITY( CG_FS_WRITE, args, t );
		FS_VM_WriteFile( t[0].p, (int)t[1].i, (fileHandle_t)t[2].i, H_CGAME );
		return 0;
	}
	case CG_FS_FCLOSEFILE: {
		vmTypedArg_t t[ VM_MAX_TYPED_ARGS ];
		CL_UnmarshalCgame( &cl_desc_CG_FS_FCLOSEFILE, args, t );
		CL_TYPED_PARITY( CG_FS_FCLOSEFILE, args, t );
		FS_VM_CloseFile( (fileHandle_t)t[0].i, H_CGAME );
		return 0;
	}
	case CG_FS_SEEK: {
		vmTypedArg_t t[ VM_MAX_TYPED_ARGS ];
		CL_UnmarshalCgame( &cl_desc_CG_FS_SEEK, args, t );
		CL_TYPED_PARITY( CG_FS_SEEK, args, t );
		return FS_VM_SeekFile( (fileHandle_t)t[0].i, (long)t[1].i, (fsOrigin_t)t[2].i, H_CGAME );
	}

	case CG_SENDCONSOLECOMMAND: {
		vmTypedArg_t t[ VM_MAX_TYPED_ARGS ];
		CL_UnmarshalCgame( &cl_desc_CG_SENDCONSOLECOMMAND, args, t );
		CL_TYPED_PARITY( CG_SENDCONSOLECOMMAND, args, t );
		Cbuf_NestedAdd( (const char *)t[0].p );
		return 0;
	}
	case CG_ADDCOMMAND:
		CL_AddCgameCommand( VMA(1) );
		return 0;
	case CG_REMOVECOMMAND:
		Cmd_RemoveCommandSafe( VMA(1) );
		return 0;
	case CG_SENDCLIENTCOMMAND: {
		vmTypedArg_t t[ VM_MAX_TYPED_ARGS ];
		CL_UnmarshalCgame( &cl_desc_CG_SENDCLIENTCOMMAND, args, t );
		CL_TYPED_PARITY( CG_SENDCLIENTCOMMAND, args, t );
		CL_AddReliableCommand( CL_AppForActiveCgame(), (const char *)t[0].p, qfalse );
		return 0;
	}
	case CG_UPDATESCREEN:
		// this is used during lengthy level loading, so pump message loop
		// Com_EventLoop();	// FIXME: if a server restarts here, BAD THINGS HAPPEN!
		// We can't call Com_EventLoop here, a restart will crash and this _does_ happen
		// if there is a map change while we are downloading at pk3.
		// ZOID
		SCR_UpdateScreen();
		return 0;
	case CG_CM_LOADMAP: {
		vmTypedArg_t t[ VM_MAX_TYPED_ARGS ];
		CL_UnmarshalCgame( &cl_desc_CG_CM_LOADMAP, args, t );
		CL_TYPED_PARITY( CG_CM_LOADMAP, args, t );
		CL_CM_LoadMap( t[0].p );
		cl_loadProgress.geometry = 1.0f;
		CL_LoadingYield( "loading geometry" );
		return 0;
	}
	case CG_CM_NUMINLINEMODELS:
		// 0-arg: bare call (W-41), like CG_MILLISECONDS.
		return CM_NumInlineModels();
	case CG_CM_INLINEMODEL: {
		vmTypedArg_t t[ VM_MAX_TYPED_ARGS ];
		CL_UnmarshalCgame( &cl_desc_CG_CM_INLINEMODEL, args, t );
		CL_TYPED_PARITY( CG_CM_INLINEMODEL, args, t );
		return CM_InlineModel( (int)t[0].i );
	}
	case CG_CM_TEMPBOXMODEL: {
		vmTypedArg_t t[ VM_MAX_TYPED_ARGS ];
		CL_UnmarshalCgame( &cl_desc_CG_CM_TEMPBOXMODEL, args, t );
		CL_TYPED_PARITY( CG_CM_TEMPBOXMODEL, args, t );
		return CM_TempBoxModel( t[0].p, t[1].p, /*int capsule*/ qfalse );
	}
	case CG_CM_TEMPCAPSULEMODEL: {
		vmTypedArg_t t[ VM_MAX_TYPED_ARGS ];
		CL_UnmarshalCgame( &cl_desc_CG_CM_TEMPCAPSULEMODEL, args, t );
		CL_TYPED_PARITY( CG_CM_TEMPCAPSULEMODEL, args, t );
		return CM_TempBoxModel( t[0].p, t[1].p, /*int capsule*/ qtrue );
	}
	case CG_CM_POINTCONTENTS: {
		vmTypedArg_t t[ VM_MAX_TYPED_ARGS ];
		CL_UnmarshalCgame( &cl_desc_CG_CM_POINTCONTENTS, args, t );
		CL_TYPED_PARITY( CG_CM_POINTCONTENTS, args, t );
		return CM_PointContents( t[0].p, (clipHandle_t)t[1].i );
	}
	case CG_CM_TRANSFORMEDPOINTCONTENTS: {
		vmTypedArg_t t[ VM_MAX_TYPED_ARGS ];
		CL_UnmarshalCgame( &cl_desc_CG_CM_TRANSFORMEDPOINTCONTENTS, args, t );
		CL_TYPED_PARITY( CG_CM_TRANSFORMEDPOINTCONTENTS, args, t );
		return CM_TransformedPointContents( t[0].p, (clipHandle_t)t[1].i, t[2].p, t[3].p );
	}
	case CG_CM_BOXTRACE: {
		// trace_t* (arg 1) is the engine's write-back result; vec3s are pointers.
		vmTypedArg_t t[ VM_MAX_TYPED_ARGS ];
		CL_UnmarshalCgame( &cl_desc_CG_CM_BOXTRACE, args, t );
		CL_TYPED_PARITY( CG_CM_BOXTRACE, args, t );
		CM_BoxTrace( t[0].p, t[1].p, t[2].p, t[3].p, t[4].p, (clipHandle_t)t[5].i, (int)t[6].i, /*capsule*/ qfalse );
		return 0;
	}
	case CG_CM_CAPSULETRACE: {
		vmTypedArg_t t[ VM_MAX_TYPED_ARGS ];
		CL_UnmarshalCgame( &cl_desc_CG_CM_CAPSULETRACE, args, t );
		CL_TYPED_PARITY( CG_CM_CAPSULETRACE, args, t );
		CM_BoxTrace( t[0].p, t[1].p, t[2].p, t[3].p, t[4].p, (clipHandle_t)t[5].i, (int)t[6].i, /*capsule*/ qtrue );
		return 0;
	}
	case CG_CM_TRANSFORMEDBOXTRACE: {
		vmTypedArg_t t[ VM_MAX_TYPED_ARGS ];
		CL_UnmarshalCgame( &cl_desc_CG_CM_TRANSFORMEDBOXTRACE, args, t );
		CL_TYPED_PARITY( CG_CM_TRANSFORMEDBOXTRACE, args, t );
		CM_TransformedBoxTrace( t[0].p, t[1].p, t[2].p, t[3].p, t[4].p, (clipHandle_t)t[5].i, (int)t[6].i, t[7].p, t[8].p, /*capsule*/ qfalse );
		return 0;
	}
	case CG_CM_TRANSFORMEDCAPSULETRACE: {
		vmTypedArg_t t[ VM_MAX_TYPED_ARGS ];
		CL_UnmarshalCgame( &cl_desc_CG_CM_TRANSFORMEDCAPSULETRACE, args, t );
		CL_TYPED_PARITY( CG_CM_TRANSFORMEDCAPSULETRACE, args, t );
		CM_TransformedBoxTrace( t[0].p, t[1].p, t[2].p, t[3].p, t[4].p, (clipHandle_t)t[5].i, (int)t[6].i, t[7].p, t[8].p, /*capsule*/ qtrue );
		return 0;
	}
	case CG_CM_MARKFRAGMENTS: {
		// 3 VARG_VMPTR_COUNTED arrays — the count×size bounds (VM_CheckBounds3) now run
		// inside VM_UnmarshalTyped from the descriptor's (countArg, elemSize). The
		// dispatch to the renderer (re.MarkFragments) + the counts pass through unchanged.
		vmTypedArg_t t[ VM_MAX_TYPED_ARGS ];
		CL_UnmarshalCgame( &cl_desc_CG_CM_MARKFRAGMENTS, args, t );
		CL_TYPED_PARITY( CG_CM_MARKFRAGMENTS, args, t );
		return re.MarkFragments( (int)t[0].i, t[1].p, t[2].p, (int)t[3].i, t[4].p, (int)t[5].i, t[6].p );
	}
	case CG_S_STARTSOUND: {
		CL_RSND( &cl_desc_CG_S_STARTSOUND, 4 );
		S_StartSound( t[0].p, (int)t[1].i, (int)t[2].i, (int)t[3].i );
		return 0;
	}
	case CG_S_STARTLOCALSOUND:
		S_StartLocalSound( args[1], args[2] );
		return 0;
	case CG_S_CLEARLOOPINGSOUNDS:
		S_ClearLoopingSounds(args[1]);
		return 0;
	case CG_S_ADDLOOPINGSOUND: {
		CL_RSND( &cl_desc_CG_S_ADDLOOPINGSOUND, 4 );
		S_AddLoopingSound( (int)t[0].i, t[1].p, t[2].p, (int)t[3].i );
		return 0;
	}
	case CG_S_ADDREALLOOPINGSOUND: {
		CL_RSND( &cl_desc_CG_S_ADDREALLOOPINGSOUND, 4 );
		S_AddRealLoopingSound( (int)t[0].i, t[1].p, t[2].p, (int)t[3].i );
		return 0;
	}
	case CG_S_STOPLOOPINGSOUND:
		S_StopLoopingSound( args[1] );
		return 0;
	case CG_S_UPDATEENTITYPOSITION: {
		CL_RSND( &cl_desc_CG_S_UPDATEENTITYPOSITION, 2 );
		S_UpdateEntityPosition( (int)t[0].i, t[1].p );
		return 0;
	}
	case CG_S_RESPATIALIZE: {
		CL_RSND( &cl_desc_CG_S_RESPATIALIZE, 4 );
		S_Respatialize( (int)t[0].i, t[1].p, t[2].p, (int)t[3].i );
		return 0;
	}
	case CG_S_REGISTERSOUND: {
		CL_RSND( &cl_desc_CG_S_REGISTERSOUND, 2 );
		sfxHandle_t h = S_RegisterSound( t[0].p, (int)t[1].i );
		if ( CL_AppForActiveCgame()->state == CA_LOADING ) {
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
		// returns sound length in milliseconds (0 if invalid).
		return S_SoundDuration( args[1] );
	case CG_S_STARTBACKGROUNDTRACK: {
		CL_RSND( &cl_desc_CG_S_STARTBACKGROUNDTRACK, 2 );
		S_StartBackgroundTrack( t[0].p, t[1].p );
		return 0;
	}
	case CG_R_LOADWORLDMAP: {
		clientApp_t *app = CL_AppForActiveCgame();
		if ( !app->cgameBsp ) {
			Com_Terminate( TERM_CLIENT_DROP, "%s: world BSP was not loaded", __func__ );
		}
		// Load into the executing app's world slot (its clientApps[] index), so a
		// second app's map does not overwrite the focused app's world.
		re.LoadWorld( app->cgameBsp, (int)( app - clientApps ) );
		CL_LoadingYield( "loading BSP" );
		return 0;
	}
	case CG_R_REGISTERMODEL: {
		CL_RSND( &cl_desc_CG_R_REGISTERMODEL, 1 );
		qhandle_t h = re.RegisterModel( t[0].p );
		if ( CL_AppForActiveCgame()->state == CA_LOADING ) {
			loadYield_modelCount++;
			if ( loadYield_modelCount % LOADING_YIELD_MODELS == 0 ) {
				CL_LoadingYield( "loading models" );
			}
		}
		return h;
	}
	case CG_R_REGISTERSKIN: {
		CL_RSND( &cl_desc_CG_R_REGISTERSKIN, 1 );
		return re.RegisterSkin( t[0].p );
	}
	case CG_R_REGISTERSHADER: {
		CL_RSND( &cl_desc_CG_R_REGISTERSHADER, 1 );
		qhandle_t h = re.RegisterShader( t[0].p );
		if ( CL_AppForActiveCgame()->state == CA_LOADING ) {
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
		CL_RSND( &cl_desc_CG_R_REGISTERSHADERNOMIP, 1 );
		qhandle_t h = re.RegisterShaderNoMip( t[0].p );
		if ( CL_AppForActiveCgame()->state == CA_LOADING ) {
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
		CL_RSND( &cl_desc_CG_R_REGISTERPRIMITIVESHADER, 1 );
		qhandle_t h = re.RegisterPrimitiveShader ? re.RegisterPrimitiveShader( t[0].p ) : 0;
		if ( CL_AppForActiveCgame()->state == CA_LOADING ) {
			loadYield_shaderCount++;
			cl_loadProgress.shaders = (float)loadYield_shaderCount / LOADING_EST_SHADERS;
			if ( cl_loadProgress.shaders > 1.0f ) cl_loadProgress.shaders = 1.0f;
			if ( loadYield_shaderCount % LOADING_YIELD_SHADERS == 0 ) {
				CL_LoadingYield( "compiling shaders" );
			}
		}
		return h;
	}
	case CG_R_REGISTERFONT: {
		CL_RSND( &cl_desc_CG_R_REGISTERFONT, 3 );
		re.RegisterFont( t[0].p, (int)t[1].i, t[2].p );   // t[2] = fontInfo_t* write-back
		return 0;
	}
	case CG_R_CLEARSCENE:
		re.ClearScene();
		return 0;
	case CG_R_ADDREFENTITYTOSCENE: {
		CL_RSND( &cl_desc_CG_R_ADDREFENTITYTOSCENE, 1 );
		re.AddRefEntityToScene( t[0].p, qfalse );
		return 0;
	}
	case CG_R_ADDREFENTITYTOSCENETEMPORAL: {
		const refEntityMotion_t *motion;
		VM_CHECKBOUNDS( VM_ActiveNativeVM(), args[1], sizeof( refEntity_t ) );
		VM_CHECKBOUNDS( VM_ActiveNativeVM(), args[2], sizeof( refEntityMotion_t ) );
		CL_RSND( &cl_desc_CG_R_ADDREFENTITYTOSCENETEMPORAL, 2 );
		motion = (const refEntityMotion_t *)t[1].p;
		if ( t[0].p && RefEntityMotion_IsValid( motion ) ) {
			if ( re.AddRefEntityToSceneTemporal ) {
				CL_TemporalIngressReceiptGeneration();
				if ( motion->ownerId < MAX_GENTITIES
						&& motion->role < REF_ENTITY_MOTION_ROLE_COUNT
						&& s_temporalIngressReceipts.slotGeneration[motion->ownerId][motion->role]
							!= motion->generation ) {
					s_temporalIngressReceipts.slotGeneration[motion->ownerId][motion->role]
						= motion->generation;
					Com_Log( SEV_INFO, LOG_CH(ch_cgame),
						"temporal-entity-slot glconfig-generation=%u trap=232 owner=%u entity-generation=%u role=%u accepted=1 export=1\n",
						s_temporalIngressReceipts.glconfigGeneration,
						motion->ownerId, motion->generation, motion->role );
				}
				re.AddRefEntityToSceneTemporal( t[0].p, t[1].p );
			} else {
				re.AddRefEntityToScene( t[0].p, qfalse );
			}
		}
		return 0;
	}
	case CG_R_ADDPOLYTOSCENE: {
		CL_RSND( &cl_desc_CG_R_ADDPOLYTOSCENE, 3 );
		re.AddPolyToScene( (int)t[0].i, (int)t[1].i, t[2].p, 1 );
		return 0;
	}
	case CG_R_ADDPOLYSTOSCENE: {
		CL_RSND( &cl_desc_CG_R_ADDPOLYSTOSCENE, 4 );
		re.AddPolyToScene( (int)t[0].i, (int)t[1].i, t[2].p, (int)t[3].i );
		return 0;
	}
	case CG_R_LIGHTFORPOINT: {
		CL_RSND( &cl_desc_CG_R_LIGHTFORPOINT, 4 );
		return re.LightForPoint( t[0].p, t[1].p, t[2].p, t[3].p );
	}
	case CG_R_ADDLIGHTTOSCENE: {   // VMF(2..5) — VARG_FLOAT
		CL_RSND( &cl_desc_CG_R_ADDLIGHTTOSCENE, 5 );
		re.AddLightToScene( t[0].p, t[1].f, t[2].f, t[3].f, t[4].f );
		return 0;
	}
	case CG_R_ADDADDITIVELIGHTTOSCENE: {   // VMF(2..5) — VARG_FLOAT
		CL_RSND( &cl_desc_CG_R_ADDADDITIVELIGHTTOSCENE, 5 );
		re.AddAdditiveLightToScene( t[0].p, t[1].f, t[2].f, t[3].f, t[4].f );
		return 0;
	}
	case CG_R_ADDRIBBONTOSCENE:
		if ( re.AddRibbonToScene ) {
			// Rebuild the host-side ribbonDesc_t from individual syscall args (the
			// descriptor can't cross as a struct — it has a pointer field, see traps.h).
			CL_RSND( &cl_desc_CG_R_ADDRIBBONTOSCENE, 4 );
			ribbonDesc_t desc;
			desc.points    = t[0].p;
			desc.numPoints = (int)t[1].i;
			desc.shader    = (int)t[2].i;
			desc.flags     = (int)t[3].i;
			re.AddRibbonToScene( &desc );
		}
		return 0;
	case CG_R_ADDBEAMTOSCENE:
		if ( re.AddBeamToScene ) {
			CL_RSND( &cl_desc_CG_R_ADDBEAMTOSCENE, 1 );
			re.AddBeamToScene( t[0].p );
		}
		return 0;
	case CG_R_ADDRAILRIBBONTOSCENE:
		if ( re.AddRailRibbonToScene ) {
			CL_RSND( &cl_desc_CG_R_ADDRAILRIBBONTOSCENE, 1 );
			re.AddRailRibbonToScene( t[0].p );
		}
		return 0;
	case CG_R_ADDSPRITETOSCENE:
		if ( re.AddSpriteToScene ) {
			CL_RSND( &cl_desc_CG_R_ADDSPRITETOSCENE, 1 );
			re.AddSpriteToScene( t[0].p );
		}
		return 0;
	case CG_R_EMITPARTICLES:
		if ( re.EmitParticles ) {
			CL_RSND( &cl_desc_CG_R_EMITPARTICLES, 1 );
			re.EmitParticles( t[0].p );
		}
		return 0;
	case CG_R_ADDDECALTOSCENE:
		if ( re.AddDecalToScene ) {
			CL_RSND( &cl_desc_CG_R_ADDDECALTOSCENE, 1 );
			re.AddDecalToScene( t[0].p );
		}
		return 0;
	case CG_R_ADDLENSSOURCETOSCENE:
		if ( re.AddLensSourceToScene ) {
			CL_RSND( &cl_desc_CG_R_ADDLENSSOURCETOSCENE, 1 );
			re.AddLensSourceToScene( t[0].p );
		}
		return 0;
	case CG_R_GETLENSVISIBILITY:
		if ( re.GetLensVisibility ) {
			CL_RSND( &cl_desc_CG_R_GETLENSVISIBILITY, 2 );
			return re.GetLensVisibility( (int)t[0].i, t[1].p );
		}
		return 0;   // qfalse → cgame falls back to its CG_Trace occlusion
	case CG_R_ADDHALOTOSCENE:
#if FEAT_HALO
		// Unpack the flat haloDesc_t into the renderer's pre-existing scalar
		// AddHaloToScene signature (no refexport change). NULL slot (FEAT_HALO
		// off / renderer lacks it) → no-op.
		if ( re.AddHaloToScene ) {
			const haloDesc_t *d;
			CL_RSND( &cl_desc_CG_R_ADDHALOTOSCENE, 1 );
			d = (const haloDesc_t *)t[0].p;
			re.AddHaloToScene( d->origin, d->r, d->g, d->b,
				d->scale, d->id, (qboolean)d->visible );
		}
#endif
		return 0;
	case CG_R_REGISTERPARTICLECLASS:
		if ( re.RegisterParticleClass ) {
			CL_RSND( &cl_desc_CG_R_REGISTERPARTICLECLASS, 2 );
			re.RegisterParticleClass( (int)t[0].i, t[1].p );
		}
		return 0;
	case CG_R_REGISTERPARTICLECLASSNAMED: {
		VM_CHECKBOUNDS( VM_ActiveNativeVM(), args[2], sizeof( particleClass_t ) );
		CL_RSND( &cl_desc_CG_R_REGISTERPARTICLECLASSNAMED, 3 );
		CL_WiredFx_RegisterParticleClass( (particleClassHandle_t)t[0].i,
			t[1].p, t[2].p );
		return 0;
	}
	case CG_R_SETATMOSPHERE:
		if ( re.SetAtmosphere ) {
			CL_RSND( &cl_desc_CG_R_SETATMOSPHERE, 1 );
			re.SetAtmosphere( t[0].p );
		}
		return 0;
	case CG_R_SETATMOSPHEREHEIGHTGRID:
		if ( re.SetAtmosphereHeightgrid ) {
			// t[0].p = bounds-checked + translated float array; t[1].i = float count.
			CL_RSND( &cl_desc_CG_R_SETATMOSPHEREHEIGHTGRID, 2 );
			re.SetAtmosphereHeightgrid( t[0].p, (int)t[1].i );
		}
		return 0;
	case CG_R_ADDATMOSPHEREEMITTER:
		if ( re.AddAtmosphereEmitter ) {
			CL_RSND( &cl_desc_CG_R_ADDATMOSPHEREEMITTER, 1 );
			re.AddAtmosphereEmitter( t[0].p );
		}
		return 0;
	case CG_R_REGISTERATMOSPHEREEFFECTPROFILE:
		if ( re.RegisterAtmosphereEffectProfile ) {
			CL_RSND( &cl_desc_CG_R_REGISTERATMOSPHEREEFFECTPROFILE, 2 );
			re.RegisterAtmosphereEffectProfile( (uint32_t)t[0].i, t[1].p );
		}
		return 0;
	case CG_R_ADDATMOSPHERESURFACEEVENT:
		if ( re.AddAtmosphereSurfaceEvent ) {
			CL_RSND( &cl_desc_CG_R_ADDATMOSPHERESURFACEEVENT, 1 );
			re.AddAtmosphereSurfaceEvent( t[0].p );
		}
		return 0;
	case CG_R_ADDATMOSPHEREMEDIAVOLUME:
		if ( re.AddAtmosphereMediaVolume ) {
			CL_RSND( &cl_desc_CG_R_ADDATMOSPHEREMEDIAVOLUME, 1 );
			re.AddAtmosphereMediaVolume( t[0].p );
		}
		return 0;
	case CG_WIRED_FX_EMIT_EVENT: {
		VM_CHECKBOUNDS( VM_ActiveNativeVM(), args[1], sizeof( wiredFxEvent_t ) );
		CL_RSND( &cl_desc_CG_WIRED_FX_EMIT_EVENT, 1 );
		CL_WiredFx_SubmitEvent( t[0].p );
		return 0;
	}
	case CG_R_RENDERSCENE: {
		CL_RSND( &cl_desc_CG_R_RENDERSCENE, 1 );
		refdef_t *fd = t[0].p;
		if ( fd ) CL_WiredFx_ServiceScene( fd );
		/* Clip the world scene to the viewport's on-screen rect. The cgame
		 * builds a fullscreen refdef; intersecting it with the current viewport
		 * rect makes a non-full-screen viewport render only inside its panel.
		 * Intersection (not assignment): a refdef already inside the rect — the
		 * full-screen viewport, or an even-masked refdef smaller than an
		 * odd-pixel panel — is left untouched, so the full-screen world is
		 * byte-identical. fov_y is re-derived only when the rect actually
		 * shrinks (the renderer uses fov_x and fov_y verbatim, so a size change
		 * without it would distort); skipping it on a no-op preserves the
		 * cgame's fov_y exactly, including its underwater warp. */
		if ( fd && s_viewportClip.active ) {
			int nx = MAX( fd->x, s_viewportClip.x );
			int ny = MAX( fd->y, s_viewportClip.y );
			int nr = MIN( fd->x + fd->width,  s_viewportClip.x + s_viewportClip.w );
			int nb = MIN( fd->y + fd->height, s_viewportClip.y + s_viewportClip.h );
			int nw = nr - nx;
			int nh = nb - ny;
			if ( nw > 0 && nh > 0 &&
			     ( nx != fd->x || ny != fd->y || nw != fd->width || nh != fd->height ) ) {
				fd->x = nx; fd->y = ny; fd->width = nw; fd->height = nh;
				/* Mirror the cgame's fov derivation exactly: half-angle
				 * fov_x/360*PI, then fov_y from the height-to-projection ratio. */
				double xproj = fd->width / tan( fd->fov_x / 360.0 * M_PI );
				fd->fov_y = (float)( atan2( (double)fd->height, xproj ) * 360.0 / M_PI );
			}
		}
		// Render against the executing app's world slot (its cgame instance), so a
		// non-focused app renders its own world rather than the focused app's.
		re.RenderScene( fd, VM_CgameInstance( VM_ActiveNativeVM() ) );
		return 0;
	}
	case CG_R_SETCOLOR: {
		// rgba is a float[4] passed as a pointer (NULL = reset to white).
		CL_RSND( &cl_desc_CG_R_SETCOLOR, 1 );
		re.SetColor( t[0].p );
		return 0;
	}
	case CG_R_DRAWSTRETCHPIC:
		/* removed — all callers migrated to CG_R_DRAWSTRETCHPICNORM (ABI slot kept) */
		COM_WARN( LOG_CH(ch_client), "WARNING: CG_R_DRAWSTRETCHPIC is removed, use CG_R_DRAWSTRETCHPICNORM\n" );
		return 0;
	case CG_R_DRAWSTRETCHPICNORM: {   // VMF(1..8) — VARG_FLOAT (normalized coords + texcoords)
		CL_RSND( &cl_desc_CG_R_DRAWSTRETCHPICNORM, 9 );
		float x = t[0].f * cls.glconfig.vidWidth;
		float y = t[1].f * cls.glconfig.vidHeight;
		float w = t[2].f * cls.glconfig.vidWidth;
		float h = t[3].f * cls.glconfig.vidHeight;
		re.DrawStretchPic( x, y, w, h, t[4].f, t[5].f, t[6].f, t[7].f, (int)t[8].i );
		return 0;
	}
	case CG_R_MODELBOUNDS: {
		CL_RSND( &cl_desc_CG_R_MODELBOUNDS, 3 );
		re.ModelBounds( (int)t[0].i, t[1].p, t[2].p );   // t[1]/t[2] = mins/maxs out-vec write-back
		return 0;
	}
	case CG_R_LERPTAG: {   // VMF(5) frac
		CL_RSND( &cl_desc_CG_R_LERPTAG, 6 );
		return re.LerpTag( t[0].p, (int)t[1].i, (int)t[2].i, (int)t[3].i, t[4].f, t[5].p );
	}
	case CG_GETGLCONFIG:
		VM_CHECKBOUNDS( VM_ActiveNativeVM(), args[1], sizeof( glconfig_t ) );
		CL_GetGlconfig( VMA(1) );
		return 0;
	case CG_GET_GLCONFIG_GENERATION:
		return cls.glconfigGeneration;
	case CG_GETGAMESTATE:
		VM_CHECKBOUNDS( VM_ActiveNativeVM(), args[1], sizeof( gameState_t ) );
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
		CL_SetUserCmdValue( args[1], VMF(2), args[3] );
		return 0;
	case CG_SETUSERCMDAIM:
		CL_SetUserCmdAim( args[1], args[2], args[3] );
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
		VM_CHECKBOUNDS( VM_ActiveNativeVM(), args[1], args[3] );
		memset( VMA(1), args[2], args[3] );
		return args[1];
	case TRAP_MEMCPY:
		VM_CHECKBOUNDS2( VM_ActiveNativeVM(), args[1], args[2], args[3] );
		memcpy( VMA(1), VMA(2), args[3] );
		return args[1];
	case TRAP_STRNCPY:
		VM_CHECKBOUNDS( VM_ActiveNativeVM(), args[1], args[3] );
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
		// sprintf writes into VM memory (VMA(1)); bound the destination first so
		// an offset near dataMask can't let the formatted digits run past the
		// data segment. "-2147483648\0" is 12 bytes worst case.
		VM_CHECKBOUNDS( VM_ActiveNativeVM(), args[1], 12 );
		return sprintf( VMA(1), "%i", (int)args[2] );
	case CG_TESTPRINTFLOAT:
		// %f can emit ~30+ bytes; reserve a generous, bounded slack.
		VM_CHECKBOUNDS( VM_ActiveNativeVM(), args[1], 64 );
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

	case CG_R_REMAP_SHADER: {
		CL_RSND( &cl_desc_CG_R_REMAP_SHADER, 3 );
		re.RemapShader( t[0].p, t[1].p, t[2].p );
		return 0;
	}

/*
	case CG_LOADCAMERA:
		return loadCamera(VMA(1));

	case CG_STARTCAMERA:
		startCamera(args[1]);
		return 0;

	case CG_GETCAMERAINFO:
		return getCameraInfo(args[1], VMA(2), VMA(3));
*/
	case CG_GET_ENTITY_TOKEN: {
		// VARG_VMPTR_SIZED bounds-checks (buffer, size) — re.GetEntityToken writes the
		// token into the VM buffer (this is render-data even though named CG_GET_*).
		CL_RSND( &cl_desc_CG_GET_ENTITY_TOKEN, 2 );
		return re.GetEntityToken( t[0].p, (int)t[1].i );
	}

	case CG_R_INPVS: {
		CL_RSND( &cl_desc_CG_R_INPVS, 2 );
		return re.inPVS( t[0].p, t[1].p );
	}

	// engine extensions
	case CG_R_ADDREFENTITYTOSCENE2: {
		CL_RSND( &cl_desc_CG_R_ADDREFENTITYTOSCENE2, 1 );
		re.AddRefEntityToScene( t[0].p, qtrue );
		return 0;
	}

	case CG_R_ADDLINEARLIGHTTOSCENE: {   // VMF(3..6) — VARG_FLOAT
		CL_RSND( &cl_desc_CG_R_ADDLINEARLIGHTTOSCENE, 6 );
		re.AddLinearLightToScene( t[0].p, t[1].p, t[2].f, t[3].f, t[4].f, t[5].f );
		return 0;
	}

	case CG_R_FORCEFIXEDDLIGHTS:
		CL_ForceFixedDlights();
		return 0;

	case CG_IS_RECORDING_DEMO:
		return CL_AppForActiveCgame()->clc.demorecording;

	case CG_CVAR_SETDESCRIPTION: {
		vmTypedArg_t t[ VM_MAX_TYPED_ARGS ];
		CL_UnmarshalCgame( &cl_desc_CG_CVAR_SETDESCRIPTION, args, t );
		CL_TYPED_PARITY( CG_CVAR_SETDESCRIPTION, args, t );
		Cvar_SetDescription2( (const char*)t[0].p, (const char*)t[1].p );
		return 0;
	}

#if FEAT_IQM
	case CG_R_GETIQMANIMS:
		if ( re.GetIQMAnimations ) {
			CL_RSND( &cl_desc_CG_R_GETIQMANIMS, 3 );
			return re.GetIQMAnimations( (int)t[0].i, t[1].p, (int)t[2].i );
		}
		return 0;
#endif // FEAT_IQM
	case CG_R_GETMDLANIMS:
		if ( re.GetMDLAnimations ) {
			CL_RSND( &cl_desc_CG_R_GETMDLANIMS, 3 );
			return re.GetMDLAnimations( (int)t[0].i, t[1].p, (int)t[2].i );
		}
		return 0;

	case CG_TRAP_GETVALUE:
		VM_CHECKBOUNDS( VM_ActiveNativeVM(), args[1], args[2] );
		return CL_GetValue( VMA(1), args[2], VMA(3) );

#if FEAT_WIRED_UI
	case CG_WIREDUI_PUSH_HUD_STATE:
		WiredHud_ReceiveState( VMA(1) );
		return 0;
	case CG_WIREDUI_PUSH_EVENT:
		WiredHud_ReceiveEvent( args[1], VMA(2) );
		return 0;

	/* V-16 (2026-05-25): viewport-provider registry syscalls. Mid-enum
	 * APPEND slots 222 + 223 in cgameImport_t; shared struct layout in
	 * code/qcommon/wired/ui_viewport_types.h. */
	case CG_REGISTER_VIEWPORT_PROVIDER:
		/* The provider's fields arrive as scalar args (args[2..5]), not as a
		 * struct pointer: the provider struct embeds a fn-ptr + void* whose
		 * width differs between a wasm32 cgame and the x64 engine, so passing it
		 * by pointer would read every field at the wrong offset. The engine
		 * builds a host-layout provider from the scalars here. cgame providers
		 * are always VM-routed, so render/userdata are forced NULL (the engine
		 * enters cgame via VM_Call by vm_key, never via this render fn-ptr).
		 *
		 * Owner = the EXECUTING cgame VM (VM_ActiveNativeVM()), so the provider
		 * belongs to the VM whose CG_INIT is running. This is correct under
		 * N-app: a background-spawned app[i] registers its provider under app[i]'s
		 * VM even while focus is elsewhere — clientActiveApp->cgvm would
		 * mis-attribute it to the focused app and orphan it on that VM's free.
		 * CL_ShutdownCGame unregisters by this same owner value (register/free
		 * symmetry). Single-app the token == clientActiveApp->cgvm == slot[0]. */
		{
			/* Namespace the registry id by the executing app's slot so a
			 * second app cannot evict the focused app's provider. Owner stays
			 * the executing VM (already per-app-correct). Slot 0 → bare id. */
			char effId[ 64 ];
			int  slot = VM_CgameInstance( VM_ActiveNativeVM() );
			wuiViewportProvider_t prov;
			prov.render       = NULL;
			prov.lifetime     = (wuiViewportLifetime_t) args[2];
			prov.input_mode   = (wuiViewportInputMode_t) args[3];
			prov.userdata     = NULL;
			prov.is_vm_routed = (qboolean) args[4];
			prov.vm_key       = (int) args[5];
			WiredUI_RegisterViewportProvider(
				CL_ViewportProviderId( VMA(1), slot, effId, sizeof( effId ) ),
				&prov, VM_ActiveNativeVM() );
		}
		return 0;
	case CG_UNREGISTER_VIEWPORT_PROVIDER:
		{
			/* Same namespacing as the register path (same slot) so a slot>0
			 * app's unregister targets its own prefixed id and never the
			 * focused app's bare provider. */
			char effId[ 64 ];
			int  slot = VM_CgameInstance( VM_ActiveNativeVM() );
			WiredUI_UnregisterViewportProvider(
				CL_ViewportProviderId( VMA(1), slot, effId, sizeof( effId ) ) );
		}
		return 0;

	/* V-20 (2026-05-31): pull per-frame scene context for the world-
	 * viewport provider callback (slot 224, write-back via VMA mirroring
	 * CG_GETGLCONFIG). Replaces the engine-side direct CL_CGameRendering
	 * push that previously drove the world render from the compositor walk. */
	case CG_GET_SCENE_FRAME_CONTEXT:
		VM_CHECKBOUNDS( VM_ActiveNativeVM(), args[1], sizeof( wuiSceneFrameCtx_t ) );
		CL_GetSceneFrameContext( VMA(1) );
		return 0;

	case CG_WIRED_SCENE_LOAD:
		/* args[1] = name string, args[2] = out wiredScene_t buffer (write-back).
		 * Bounds-check the dest before the engine marshals the POD into it. */
		VM_CHECKBOUNDS( VM_ActiveNativeVM(), args[2], sizeof( wiredScene_t ) );
		return CL_WiredSceneLoad( VMA(1), VMA(2) );
	case CG_R_DRAWTEXTNORM:
		/* normalized coords (0.0-1.0) — scale to real pixels */
		{
			CL_RSND( &cl_desc_CG_R_DRAWTEXTNORM, 8 );
			float x = t[1].f * cls.glconfig.vidWidth;
			float y = t[2].f * cls.glconfig.vidHeight;
			float size = t[4].f * cls.glconfig.vidHeight;
			Text_Draw( t[0].p, x, y, (int)t[3].i, size, t[5].p, (int)t[6].i, (int)t[7].i );
		}
		return 0;
	case CG_R_MEASURETEXTNORM:
		/* normalized size (0.0-1.0) — scale to real pixels, measure, scale result back */
		{
			CL_RSND( &cl_desc_CG_R_MEASURETEXTNORM, 3 );
			float realSize = t[2].f * cls.glconfig.vidHeight;
			float realWidth = Text_Measure( t[0].p, (int)t[1].i, realSize );
			return FloatAsInt( realWidth / (float)cls.glconfig.vidWidth );
		}
	case CG_WUI_STORE_PUSH_BATCH:
		{
			const wuiStagedEntry_t *entries = VMA(1);
			int count = args[2];
			// count + the entries array extent are both VM-supplied and untrusted.
			// Reject a negative/over-large count and range-check the full
			// count*sizeof(entry) span (the base-only VMA() mask does not bound
			// the array extent — entries[i] for large i walks past the data
			// segment into host memory). 1024 mirrors the staging-store cap.
			if ( count <= 0 ) {
				return 0;
			}
			if ( count > 1024 ) {
				count = 1024;
			}
			VM_CHECKBOUNDS3( VM_ActiveNativeVM(), args[1], (unsigned)count, sizeof( wuiStagedEntry_t ) );
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
	case CG_WUI_STORE_PUSH_MARKERLIST:
		{
			const char *listKey = VMA(1);
			const wuiMarker_t *markers = VMA(2);
			int count = args[3];
			// listKey (VM string) + the markers array extent are VM-supplied and
			// untrusted. Reject bad count, cap to the client cap, and range-check
			// the full count*sizeof(marker) span (VMA() masks only the base — a
			// large index would walk past the data segment into host memory).
			if ( count < 0 ) {
				count = 0;
			}
			if ( count > WUI_MAX_MARKERS_PER_LIST ) {
				count = WUI_MAX_MARKERS_PER_LIST;
			}
			if ( count > 0 ) {
				VM_CHECKBOUNDS3( VM_ActiveNativeVM(), args[2], (unsigned)count, sizeof( wuiMarker_t ) );
			}
			WiredStore_SetMarkerList( listKey, markers, count );
		}
		return 0;
	case CG_WUI_STORE_DELETE:
		WiredStore_Delete( VMA(1) );
		return 0;
	case CG_WUI_STORE_CLEAR:
		WiredStore_Clear();
		return 0;
#endif

#if !FEAT_WIRED_UI && defined(WASM_MODULE)
	/* The browser AOT UI consumes the same cgame-owned HUD/store publications as
	 * native WiredUI. Unsupported viewport/event surfaces remain advisory until
	 * their canonical browser consumers land. */
	case CG_WIREDUI_PUSH_HUD_STATE:
		VM_CHECKBOUNDS( VM_ActiveNativeVM(), args[1], sizeof( wiredHudState_t ) );
		WiredWebUi_ReceiveHudState( VMA(1) );
		return 0;
	case CG_WUI_STORE_PUSH_BATCH:
		{
			int count = args[2];
			if ( count < 0 ) count = 0;
			if ( count > 256 ) count = 256;
			if ( count > 0 )
				VM_CHECKBOUNDS3( VM_ActiveNativeVM(), args[1], (unsigned)count,
					sizeof( wuiStagedEntry_t ) );
			WiredWebUi_ReceiveStoreBatch( VMA(1), count );
		}
		return 0;
	case CG_WIREDUI_PUSH_EVENT:
	case CG_REGISTER_VIEWPORT_PROVIDER:
	case CG_UNREGISTER_VIEWPORT_PROVIDER:
	case CG_R_DRAWTEXTNORM:
	case CG_WUI_STORE_PUSH_MARKERLIST:
	case CG_WUI_STORE_DELETE:
	case CG_WUI_STORE_CLEAR:
		return 0;
	case CG_R_MEASURETEXTNORM:
		return FloatAsInt( 0.0f );
#endif

	case CG_R_SETLIGHTSTYLEPATTERN:
		if ( re.SetLightstylePattern ) {
			CL_RSND( &cl_desc_CG_R_SETLIGHTSTYLEPATTERN, 2 );
			re.SetLightstylePattern( (int)t[0].i, t[1].p );
		}
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


// VM teardown callbacks (registered in CL_InitCGame, invoked by VM_Free): each
// frees a class of cgame-VM resource by owner, so a pure VM_Free is leak-free
// without CL_ShutdownCGame driving the sweep order. Tier-correct — these live in
// the client tier (where the resources do); VM_Free only invokes the function
// pointer. `owner` is the value passed at register time, never the vm_t.

// Commands + viewport providers are tagged with the vm handle as their owner, so
// the sweep keys on `owner` (the vm pointer, pointer-compare only).
static void CL_CgameTeardown_Commands( void *owner ) {
	Cmd_RemoveCgameCommandsByOwner( owner );
}
static void CL_CgameTeardown_Viewports( void *owner ) {
#if FEAT_WIRED_UI
	WiredUI_UnregisterViewportProvidersByOwner( owner );
#else
	(void)owner;
#endif
}
// Close this app's cgame file handles. The H_CGAME file-VM key is shared across
// concurrent apps, but the handles are now tagged with the owning app's slot at
// open, so this closes ONLY this app's files (its slot) — no focused-app gate
// needed (the per-app slot replaces it; the old gate existed only because the
// class-only key would otherwise wipe every app's files). At N=1 the single app
// is slot 0, byte-identical to the prior focused-app close of the whole table.
static void CL_CgameTeardown_Files( void *owner ) {
	const clientApp_t *app = (const clientApp_t *)owner;
	FS_VM_CloseFiles( H_CGAME, (int)( app - clientApps ) );
}


/*
====================
CL_InitCGame

Should only be called by CL_StartHunkUsers
====================
*/
void CL_InitCGame( clientApp_t *app ) {
	const char			*info;
	const char			*mapname;
	// The input-focused app drives the host-visible loading screen, console
	// state, and asset-paging side-effects. An additional app priming its own
	// cgame does the VM work but must not touch the host UI; isFocused gates
	// those side-effects so a non-focused prime is silent on screen. Renderer /
	// memory / command-buffer globals stay ungated — they are process-wide and
	// idempotent for the second prime.
	qboolean			isFocused = ( app == clientActiveApp );

	app->cl.cgameAimMode = UCMD_AIM_NONE;
	app->cl.cgameAimAngles[PITCH] = 0;
	app->cl.cgameAimAngles[YAW] = 0;

	Cbuf_NestedReset();

	int t1 = Sys_Milliseconds();

	if ( isFocused ) {
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
	}

	// find the current mapname
	info = app->cl.gameState.stringData + app->cl.gameState.stringOffsets[ CS_SERVERINFO ];
	mapname = Info_ValueForKey( info, "mapname" );
	Com_sprintf( app->cl.mapname, sizeof( app->cl.mapname ), "maps/%s.bsp", mapname );

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

	// owner = an engine-owned per-app token (pointer identity only; one per
	// client-app so app[i]'s cgame VM has a distinct owner and lands in its own
	// vmTable_cgame[i] without tripping the owner-mismatch dedup guard). NOT
	// clientActiveApp->clc.clientNum (tier rule: the VM tier never reads
	// client-protocol identity). cgameInstance = the app's slot index; at N=1
	// appIdx==0 -> vmTable_cgame[0] + owner[0], byte-identical to the prior
	// single sentinel.
	static char cl_cgameAppOwner[MAX_LOCAL_CGAME_VMS];
	int appIdx = (int)( app - clientApps );
	app->cgvm = VM_Create( VM_CGAME, appIdx, &cl_cgameAppOwner[appIdx], CL_CgameSystemCalls, CL_DllSyscall, interpret );
	if ( !app->cgvm ) {
		Com_Terminate( TERM_CLIENT_DROP, "VM_Create on cgame failed" );
	}

	// Register this VM's teardown cleanups so VM_Free frees the cgame-side
	// resources (commands, file handles, viewport providers) regardless of how the
	// VM is freed — the manual sweeps that used to live in CL_ShutdownCGame now run
	// from VM_Free, so a pure VM_Free is leak-free. Commands + viewports key on the
	// vm handle (their owner). The file callback takes the app as its owner so its
	// focused-app gate is evaluated at teardown time (the shared H_CGAME table is
	// closed only by the focused app, as before).
	VM_RegisterTeardownCallback( app->cgvm, app->cgvm, CL_CgameTeardown_Viewports );
	VM_RegisterTeardownCallback( app->cgvm, app,        CL_CgameTeardown_Files );
	VM_RegisterTeardownCallback( app->cgvm, app->cgvm, CL_CgameTeardown_Commands );
	Com_Log( SEV_INFO, LOG_CH(ch_client),
		"cls.state: -> CA_LOADING (CL_InitCGame: mapname=%s)\n", app->cl.mapname );
	CL_SetState( app, CA_LOADING );
	if ( isFocused ) {
		// state→named-UI: ensure the loading_screen backdrop is bound even if this
		// CA_LOADING was reached without going through CL_DownloadsComplete.
#if FEAT_WIRED_UI
		WiredUI_SetLoadingMenu( "ui/loading_screen.wui" );
#endif
		cl_loadYieldLastTime = Sys_Milliseconds();
	}

	// init for this gamestate
	// use the lastExecutedServerCommand instead of the serverCommandSequence
	// otherwise server commands sent just before a gamestate are dropped
	CL_WiredFx_BeginRegistration();
	VM_Call( app->cgvm, 3, CG_INIT, app->clc.serverMessageSequence, app->clc.lastExecutedServerCommand, app->clc.clientNum );
	CL_WiredFx_LoadProfiles();

	// we will send a usercmd this frame, which
	// will cause the server to send us the first snapshot
	Com_Log( SEV_INFO, LOG_CH(ch_client),
		"cls.state: -> CA_PRIMED (CG_INIT complete: mapname=%s)\n", app->cl.mapname );
	CL_SetState( app, CA_PRIMED );

	int t2 = Sys_Milliseconds();

	Com_Log( SEV_INFO, LOG_CH(ch_client), "CL_InitCGame: %5.2f seconds\n", (t2-t1)/1000.0 );

	// have the renderer touch all its images, so they are present
	// on the card even if the driver does deferred loading
	re.EndRegistration();

	if ( isFocused ) {
		// Final yield — all assets are loaded and finalized
		cl_loadProgress.geometry = 1.0f;
		cl_loadProgress.shaders = 1.0f;
		cl_loadProgress.audio = 1.0f;
		CL_LoadingYield( "finalizing" );
	}

	// make sure everything is paged in
	if (!Sys_LowPhysicalMemory()) {
		Com_TouchMemory();
	}

	if ( isFocused ) {
		// clear anything that got printed
		Con_ClearNotify ();
	}

	// do not allow vid_restart for first time
	cls.lastVidRestart = Sys_Milliseconds();
}


/*
====================
CL_PrimeHeadlessApp

Bring an in-process client to CA_PRIMED WITHOUT a cgame VM or the renderer.

A runtime-headless client (a bot/MCP client sharing the process with the
integrated host) has no cgame module and never draws, so it cannot run the
CL_InitCGame body (VM_Create / CG_INIT / re.EndRegistration). It does not need
to: the gamestate — configstrings and entity baselines — was already parsed into
app->cl.gameState by CL_ParseGamestate, so "primed" for such a client means only
"has the gamestate, ready to receive snapshots." This parses the mapname (for
diagnostics / any consumer that reads cl.mapname) and advances the client to
CA_PRIMED. The bot/MCP logic then reads app->cl.snap directly each frame.

Mirrors the mapname parse in CL_InitCGame but omits every VM/renderer/UI step.
====================
*/
void CL_PrimeHeadlessApp( clientApp_t *app ) {
	const char *info;
	const char *mapname;

	info    = app->cl.gameState.stringData + app->cl.gameState.stringOffsets[ CS_SERVERINFO ];
	mapname = Info_ValueForKey( info, "mapname" );
	Com_sprintf( app->cl.mapname, sizeof( app->cl.mapname ), "maps/%s.bsp", mapname );

	app->cgvm = NULL;   /* explicit: no cgame VM for a headless client */
	CL_SetState( app, CA_PRIMED );
	Com_Log( SEV_INFO, LOG_CH(ch_client),
		"headless client primed (no cgame VM): mapname=%s\n", app->cl.mapname );
}


/*
====================
CL_GameCommand

See if the current console command is claimed by the cgame
====================
*/

qboolean CL_GameCommand( void ) {
	qboolean bRes;

	if ( !clientActiveApp->cgvm ) {
		return qfalse;
	}

	bRes = (qboolean)VM_Call( clientActiveApp->cgvm, 0, CG_CONSOLE_COMMAND );

	Cbuf_NestedReset();

	return bRes;
}


/*
=====================
CL_CGameRendering
=====================
*/
void CL_CGameRendering( stereoFrame_t stereo ) {
	VM_Call( clientActiveApp->cgvm, 3, CG_DRAW_ACTIVE_FRAME, clientActiveApp->cl.serverTime, stereo, clientActiveApp->clc.demoplaying );
#ifdef DEBUG
	VM_Debug( 0 );
#endif
}


/*
=====================
CL_RenderCGameViewport

WN-BLACKFIX: enter the cgame to render a VM-routed in-game viewport via the
VM_Call ABI. The WiredUI viewport walk (elements/viewport.c) resolves a
VM-routed provider (is_vm_routed) and calls here with the provider's vm_key
(a wuiViewportKey_t). This is the one engine file allowed to reference the
cgame export enum (the cgame syscall bridge, it already includes cg_public.h),
so the CG_RENDER_VIEWPORT integer lives here — viewport.c stays free of cgame
headers (CLAUDE.md one-way dependency). A raw fn-ptr deref of VM-space code
was the defect; this routes correctly through VM_Call. The cgame handler pulls
per-frame context itself via CG_GET_SCENE_FRAME_CONTEXT (no push args here).

ownerCgvm is the cgame VM that REGISTERED this viewport provider (captured as
the executing VM at register time) — i.e. the owning app's cgvm. Entering that
VM (not the focused app's) renders each app's own scene into its viewport.
=====================
*/
void CL_RenderCGameViewport( void *ownerCgvm, int vmKey, int x, int y, int w, int h ) {
	vm_t *cgvm = (vm_t *)ownerCgvm;
	if ( !cgvm ) {
		return;
	}
	/* Publish this viewport's on-screen rect so the scene-render syscall clips
	 * the cgame's fullscreen refdef to it. Cleared after — the VM_Call is
	 * synchronous on the single-threaded compositor walk, so exactly one
	 * viewport's clip is live at a time. */
	s_viewportClip.x = x;
	s_viewportClip.y = y;
	s_viewportClip.w = w;
	s_viewportClip.h = h;
	s_viewportClip.active = qtrue;
	VM_Call( cgvm, 1, CG_RENDER_VIEWPORT, vmKey );
	s_viewportClip.active = qfalse;
}


/*
=================
CL_AdjustTimeDelta

Adjust the clients view of server time.

We attempt to have clientActiveApp->cl.serverTime exactly equal the server's view
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

static void CL_AdjustTimeDelta( clientApp_t *app ) {
	app->cl.newSnapshots = qfalse;

	// the delta never drifts when replaying a demo
	if ( app->clc.demoplaying ) {
		return;
	}

	int newDelta = app->cl.snap.serverTime - cls.realtime;
	int deltaDelta = abs( newDelta - app->cl.serverTimeDelta );

	if ( deltaDelta > RESET_TIME ) {
		app->cl.serverTimeDelta = newDelta;
		app->cl.oldServerTime = app->cl.snap.serverTime;	// FIXME: is this a problem for cgame?
		app->cl.serverTime = app->cl.snap.serverTime;
		if ( cl_showTimeDelta->integer ) {
			Com_Log( SEV_INFO, LOG_CH(ch_client), "<RESET> " );
		}
	} else if ( deltaDelta > 100 ) {
		// fast adjust, cut the difference in half
		if ( cl_showTimeDelta->integer ) {
			Com_Log( SEV_INFO, LOG_CH(ch_client), "<FAST> " );
		}
		app->cl.serverTimeDelta = ( app->cl.serverTimeDelta + newDelta ) >> 1;
	} else {
		// slow drift adjust, only move 1 or 2 msec

		// if any of the frames between this and the previous snapshot
		// had to be extrapolated, nudge our sense of time back a little
		// the granularity of +1 / -2 is too high for timescale modified frametimes
		if ( com_timescale->value == 0 || com_timescale->value == 1 ) {
			if ( app->cl.extrapolatedSnapshot ) {
				app->cl.extrapolatedSnapshot = qfalse;
				app->cl.serverTimeDelta -= 2;
			} else {
				// otherwise, move our sense of time forward to minimize total latency
				app->cl.serverTimeDelta++;
			}
		}
	}

	if ( cl_showTimeDelta->integer ) {
		Com_Log( SEV_INFO, LOG_CH(ch_client), "%i ", app->cl.serverTimeDelta );
	}
}


/*
==================
CL_FirstSnapshot
==================
*/
static void CL_FirstSnapshot( clientApp_t *app ) {
	// ignore snapshots that don't have entities
	if ( app->cl.snap.snapFlags & SNAPFLAG_NOT_ACTIVE ) {
		return;
	}

	// Per-map first-gameplay-frame marker. Fires exactly once per map load
	// (CL_FirstSnapshot runs once per map), carries runtime values guaranteed
	// distinct across map loads so a smoke harness can confirm independent
	// transitions actually happened (rather than the same first frame being
	// grep'd repeatedly).
	Com_Log( SEV_INFO, LOG_CH(ch_client),
		"cls.state: -> CA_ACTIVE (FIRST GAMEPLAY FRAME mapname=%s serverTime=%d numEntities=%d framecount=%d)\n",
		app->cl.mapname, app->cl.snap.serverTime, app->cl.snap.numEntities, cls.framecount );

	CL_SetState( app, CA_ACTIVE );

	/* The loading UI, the activeAction script, and the profiler are owned by the
	 * input-focused (rendered) client. An additional in-process client entering
	 * the world must not clear the host's loading screen or fire host-global
	 * scripting/profiling — it only advances its own time/state below. */
	qboolean isFocused = ( app == clientActiveApp );

	if ( isFocused ) {
#if FEAT_WIRED_UI
		WiredUI_SetLoadingMenu( NULL );  // loading over — clear the bound loading UI
#endif
	}

	// clear old game so we will not switch back to old mod on disconnect
	// (host-global fs_game bookkeeping — only the focused client owns it)
	if ( isFocused )
		CL_ResetOldGame();

	// set the timedelta so we are exactly on this first frame
	app->cl.serverTimeDelta = app->cl.snap.serverTime - cls.realtime;
	app->cl.oldServerTime = app->cl.snap.serverTime;

	app->clc.timeDemoBaseTime = app->cl.snap.serverTime;

	// if this is the first frame of active play,
	// execute the contents of activeAction now
	// this is to allow scripting a timedemo to start right
	// after loading
	if ( isFocused && cl_activeAction->string[0] ) {
		Cbuf_AddText( cl_activeAction->string );
		Cbuf_AddText( "\n" );
		Cvar_Set( "activeAction", "" );
	}

	if ( isFocused )
		Sys_BeginProfiling();
}


/*
==================
CL_AvgPing

Calculates Average Ping from snapshots in buffer. Used by AutoNudge.
==================
*/
static float CL_AvgPing( clientApp_t *app ) {
	int ping[PACKET_BACKUP];
	int count = 0;

	for ( int i = 0; i < PACKET_BACKUP; i++ ) {
		if ( app->cl.snapshots[i].ping > 0 && app->cl.snapshots[i].ping < 999 ) {
			ping[count] = app->cl.snapshots[i].ping;
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
static int CL_TimeNudge( clientApp_t *app ) {
	float autoNudge = cl_autoNudge->value;

	if ( autoNudge != 0.0f )
		return (int)((CL_AvgPing( app ) * autoNudge) + 0.5f) * -1;
	return cl_timeNudge->integer;
}


/*
==================
CL_SetCGameTime
==================
*/
void CL_SetCGameTime( clientApp_t *app ) {
	qboolean demoFreezed;

	// getting a valid frame message ends the connection process
	if ( app->state != CA_ACTIVE ) {
		if ( app->state != CA_PRIMED ) {
			return;
		}
		if ( app->clc.demoplaying ) {
			// we shouldn't get the first snapshot on the same frame
			// as the gamestate, because it causes a bad time skip
			if ( !app->clc.firstDemoFrameSkipped ) {
				app->clc.firstDemoFrameSkipped = qtrue;
				return;
			}
			CL_ReadDemoMessage();
		}
		if ( app->cl.newSnapshots ) {
#ifdef _DEBUG
			/* Dev/test loading-backdrop capture: hold the state at CA_PRIMED for
			 * debug_hold_loading frames (early-return before CL_FirstSnapshot so
			 * newSnapshots stays pending). Fire one Cbuf screenshot a couple
			 * frames in so it executes (next Com_Frame Cbuf_Execute) and is
			 * consumed by re.EndFrame while STILL held CA_PRIMED — capturing the
			 * loading compositor. Disarm on the last hold frame → normal CA_ACTIVE
			 * transition. Never wedges. Host-only: this drives process-global
			 * capture cvars + the console screenshot, so a non-focused client must
			 * not run it (gate on the input-focused app). */
			if ( app == clientActiveApp ) {
				int holdN = Cvar_VariableIntegerValue( "debug_hold_loading" );
				if ( holdN > 0 ) {
					static int holdElapsed = 0;
					holdElapsed++;
					/* fire on the 2nd held frame, EXEC_NOW so it dispatches
					 * synchronously (the smoke's +waitForMap gates the queued
					 * cbuf during CA_PRIMED, so EXEC_APPEND never ran in-window —
					 * pinned by investigation). EXEC_NOW runs the screenshot now,
					 * setting the screenshot mask; re.EndFrame later this same
					 * CL_Frame consumes it on the held CA_PRIMED frame. */
					if ( holdElapsed == 2 ) {
						Cbuf_ExecuteText( EXEC_NOW, "screenshot wn_loadshot png silent\n" );
					}
					if ( holdN > 1 ) {
						Cvar_SetValue( "debug_hold_loading", (float)( holdN - 1 ) );
						return;            /* stay CA_PRIMED; loading layer keeps drawing */
					}
					Cvar_Set( "debug_hold_loading", "0" );   /* last frame: disarm */
					holdElapsed = 0;
				}
			}
#endif
			app->cl.newSnapshots = qfalse;
			CL_FirstSnapshot( app );
		}
		if ( app->state != CA_ACTIVE ) {
			return;
		}
	}

	// if we have gotten to this point, cl.snap is guaranteed to be valid
	if ( !app->cl.snap.valid ) {
		Com_Terminate( TERM_CLIENT_DROP, "CL_SetCGameTime: !cl.snap.valid" );
	}

	// allow pause in single player
	if ( sv_paused->integer && CL_CheckPaused() && com_sv_running->integer ) {
		// paused
		return;
	}

	if ( app->cl.snap.serverTime - app->cl.oldFrameServerTime < 0 ) {
		Com_Terminate( TERM_CLIENT_DROP, "cl.snap.serverTime < cl.oldFrameServerTime" );
	}
	app->cl.oldFrameServerTime = app->cl.snap.serverTime;

	// get our current view of time
	demoFreezed = app->clc.demoplaying && com_timescale->value == 0.0f;
	if ( demoFreezed ) {
		// \timescale 0 is used to lock a demo in place for single frame advances
		app->cl.serverTimeDelta -= cls.frametime;
	} else {
		// cl_timeNudge is a user adjustable cvar that allows more
		// or less latency to be added in the interest of better
		// smoothness or better responsiveness.
		app->cl.serverTime = cls.realtime + app->cl.serverTimeDelta - CL_TimeNudge( app );

		// guarantee that time will never flow backwards, even if
		// serverTimeDelta made an adjustment or cl_timeNudge was changed
		if ( app->cl.serverTime - app->cl.oldServerTime < 0 ) {
			app->cl.serverTime = app->cl.oldServerTime;
		}
		app->cl.oldServerTime = app->cl.serverTime;

		// note if we are almost past the latest frame (without timeNudge),
		// so we will try and adjust back a bit when the next snapshot arrives
		//if ( cls.realtime + cl.serverTimeDelta >= cl.snap.serverTime - 5 ) {
		if ( cls.realtime + app->cl.serverTimeDelta - app->cl.snap.serverTime >= -5 ) {
			app->cl.extrapolatedSnapshot = qtrue;
		}
	}

	// if we have gotten new snapshots, drift serverTimeDelta
	// don't do this every frame, or a period of packet loss would
	// make a huge adjustment
	if ( app->cl.newSnapshots ) {
		CL_AdjustTimeDelta( app );
	}

	if ( !app->clc.demoplaying ) {
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
		if ( !app->clc.timeDemoStart ) {
			app->clc.timeDemoStart = Sys_Milliseconds();
		}
		app->clc.timeDemoFrames++;
		app->cl.serverTime = app->clc.timeDemoBaseTime + app->clc.timeDemoFrames * 50;
	}

	//while ( cl.serverTime >= cl.snap.serverTime ) {
	while ( app->cl.serverTime - app->cl.snap.serverTime >= 0 ) {
		// feed another message, which should change
		// the contents of cl.snap
		CL_ReadDemoMessage();
		if ( app->state != CA_ACTIVE ) {
			return; // end of demo
		}
	}
}


/*
==================
CL_DriveHeadlessApp

Advance a runtime-headless client's state machine each frame WITHOUT a cgame VM.

CL_SetCGameTime is the input-focused client's driver — it interpolates serverTime
for the renderer/cgame and is bound to clientActiveApp. A headless client (no
render, no cgame interpolation) needs only: enter the world on its first valid
snapshot (CA_PRIMED -> CA_ACTIVE) and keep cl.serverTime tracking the latest snap
so any bot/MCP logic reading app->cl.snap sees current state. No timeNudge,
extrapolation, demo handling, or cgame call — those exist for rendered playback.

Caller skips this for a client that has a cgvm (the integrated host runs the full
CL_SetCGameTime path instead).
==================
*/
void CL_DriveHeadlessApp( clientApp_t *app ) {
	if ( app->state == CA_PRIMED ) {
		if ( app->cl.newSnapshots ) {
			app->cl.newSnapshots = qfalse;
			CL_FirstSnapshot( app );   // VM-less: sets CA_ACTIVE from cl.snap
		}
		if ( app->state != CA_ACTIVE ) {
			return;
		}
	}
	if ( app->state != CA_ACTIVE || !app->cl.snap.valid ) {
		return;
	}
	// Track server time to the latest snapshot (no interpolation nudge — a
	// headless client does not render between frames).
	app->cl.serverTime    = app->cl.snap.serverTime;
	app->cl.oldServerTime = app->cl.snap.serverTime;
	if ( app->cl.newSnapshots ) {
		app->cl.newSnapshots = qfalse;
	}
}
