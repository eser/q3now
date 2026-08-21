// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 1999-2005 Id Software, Inc.
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
// client.h -- primary header for client

#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"
#include "../qcommon/net_transport.h"
#include "../qcommon/maps/map_format_registry.h"
#include "../renderercommon/tr_public.h"
#include "../qcommon/vm_local.h"
#include "../cgame/cg_public.h"
#include "../qcommon/wired/protocol.h"
#include "../qcommon/menudef.h"
#include "snd_public.h"
#include "keys.h"
#include "cl_ping_owner.h"


#define	RECONNECT_TIMEOUT	3000	// time between packet retransmits at CA_CONNECTING / CA_CHALLENGING
#define	RETRANSMIT_TIMEOUT	1000	// time between packet retransmits at CA_CONNECTED / CA_LOADING

// snapshots are a view of the server at a given time
typedef struct {
	/* Browser-origin connection authentication is one attempt, one target.
	 * The secret is copied only after CL_Disconnect has wiped the previous
	 * connection and is erased as soon as the transport copies CONNECT
	 * userinfo.  Non-secret target/generation metadata survives until the
	 * attempt succeeds or fails so WiredUI can offer a scoped retry. */
	qboolean	browserOrigin;
	qboolean	credentialPending;
	char		target[MAX_OSPATH];
	int		selectionGeneration;
	char		joinPassword[33];
} clientJoinAttempt_t;

typedef struct {
	qboolean		valid;			// cleared if delta parsing was invalid
	int				snapFlags;		// rate delayed and dropped commands

	int				serverTime;		// server time the message is valid for (in msec)

	int				messageNum;		// copied from netchan->incoming_sequence
	int				deltaNum;		// messageNum the delta is from
	int				ping;			// time from when cmdNum-1 was sent to time packet was reeceived
	int				areabytes;
	byte			areamask[MAX_MAP_AREA_BYTES];		// portalarea visibility bits

	int				cmdNum;			// the next cmdNum the server is expecting
	playerState_t	ps;						// complete information about the current player at this time

	int				numEntities;			// all of the entities that need to be presented
	int				parseEntitiesNum;		// at the time of this snapshot

	int				serverCommandNum;		// execute all commands up to this before
											// making the snapshot current
} clSnapshot_t;



/*
=============================================================================

the clientActive_t structure is wiped completely at every
new gamestate_t, potentially several times during an established connection

=============================================================================
*/

typedef struct {
	int		p_cmdNumber;		// cl.cmdNumber when packet was sent
	int		p_serverTime;		// usercmd->serverTime when packet was sent
	int		p_realtime;			// cls.realtime when packet was sent
} outPacket_t;

// the parseEntities array must be large enough to hold PACKET_BACKUP frames of
// entities, so that when a delta compressed message arives from the server
// it can be un-deltad from the original
#define	MAX_PARSE_ENTITIES	( PACKET_BACKUP * MAX_SNAPSHOT_ENTITIES )

extern int g_console_field_width;

typedef struct {
	int			timeoutcount;		// it requres several frames in a timeout condition
									// to disconnect, preventing debugging breaks from
									// causing immediate disconnects on continue
	clSnapshot_t	snap;			// latest received from server

	int			serverTime;			// may be paused during play
	int			oldServerTime;		// to prevent time from flowing bakcwards
	int			oldFrameServerTime;	// to check tournament restarts
	int			serverTimeDelta;	// cl.serverTime = cls.realtime + cl.serverTimeDelta
									// this value changes as net lag varies
	qboolean	extrapolatedSnapshot;	// set if any cgame frame has been forced to extrapolate
									// cleared when CL_AdjustTimeDelta looks at it
	qboolean	newSnapshots;		// set on parse of any valid packet

	gameState_t	gameState;			// configstrings
	char		mapname[MAX_QPATH];	// extracted from CS_SERVERINFO

	int			parseEntitiesNum;	// index (not anded off) into cl_parse_entities[]

	float		mouseDx[2], mouseDy[2];	// added to by mouse events (float: sub-pixel mouse delta, not truncated)
	int			mouseIndex;
	int			joystickAxis[MAX_JOYSTICK_AXIS];	// set by joystick events

	// cgame communicates a few values to the client system
	int			cgameUserCmdValue;	// current weapon to add to usercmd_t
	float		cgameSensitivity;
	int			cgameFreezeMove;	// cinematic scene: 1 = null-move this cmd (client-only, no server change)

	// cmds[cmdNumber] is the predicted command, [cmdNumber-1] is the last
	// properly generated command
	usercmd_t	cmds[CMD_BACKUP];	// each message will send several old cmds
	int			cmdNumber;			// incremented each frame, because multiple
									// frames may need to be packed into a single packet

	outPacket_t	outPackets[PACKET_BACKUP];	// information about each packet we have sent out

	// the client maintains its own idea of view angles, which are
	// sent to the server each frame.  It is cleared to 0 upon entering each level.
	// the server sends a delta each frame which is added to the locally
	// tracked view angles to account for standing on rotating objects,
	// and teleport direction changes
	vec3_t		viewangles;

	int			serverId;			// included in each client message so the server
												// can tell if it is for a prior map_restart
	// big stuff at end of structure so most offsets are 15 bits or less
	clSnapshot_t	snapshots[PACKET_BACKUP];

	entityState_t	entityBaselines[MAX_GENTITIES];	// for delta compression when not in previous frame

	entityState_t	parseEntities[MAX_PARSE_ENTITIES];

	byte			baselineUsed[MAX_GENTITIES];
} clientActive_t;

// 'cl' is no longer a bare global — it lives in the per-app container
// clientApp_t (see below). A macro alias preserves every existing `cl.field`
// access site; in single-app it resolves to clientApps[0].cl (the pinned
// instance), byte-identical to the former file-scope global.

#define EM_GAMESTATE 1
#define EM_SNAPSHOT  2
#define EM_COMMAND   4

/*
=============================================================================

the clientConnection_t structure is wiped when disconnecting from a server,
either to go to a full screen console, play a demo, or connect to a different server

A connection can be to either a server through the network layer or a
demo through a file.

=============================================================================
*/

typedef struct {

	int			clientNum;
	int			lastPacketSentTime;			// for retransmits during connection
	int			lastPacketTime;				// for timeouts

	netadr_t	serverAddress;
	int			connectTime;				// for connection retransmits
	int			connectPacketCount;			// for display on connection dialog
	char		serverMessage[MAX_STRING_CHARS]; // for display on connection dialog

	int			challenge;					// from the server to use for connecting
	int			checksumFeed;				// from the server for checksum calculations
	qboolean	wiredRconAuthed;
	qboolean	wiredRconHasChallenge;
	char		wiredRconChallenge[65];
	netadr_t	wiredRconAddress;

	/* QUIC connection handle for this client session.
	 * Set by transport->connect() on CA_CONNECTING, cleared (zeroed) in
	 * CL_Disconnect (via memset).  Use (clc.quic_conn != CONN_INVALID)
	 * as the authoritative test for "we are on a QUIC connection". */
	conn_handle_t quic_conn;
	clientJoinAttempt_t joinAttempt;
	// these are our reliable messages that go to the server
	int			reliableSequence;
	int			reliableAcknowledge;		// the last one the server has executed
	char		reliableCommands[MAX_RELIABLE_COMMANDS][MAX_STRING_CHARS];

	// server message (unreliable) and command (reliable) sequence
	// numbers are NOT cleared at level changes, but continue to
	// increase as long as the connection is valid

	// message sequence is used by both the network layer and the
	// delta compression layer
	int			serverMessageSequence;

	// reliable messages received from server
	int			serverCommandSequence;
	int			lastExecutedServerCommand;		// last server command grabbed or executed with CL_GetServerCommand
	char		serverCommands[MAX_RELIABLE_COMMANDS][MAX_STRING_CHARS];
	qboolean	serverCommandsIgnore[MAX_RELIABLE_COMMANDS];

	// file transfer from server
	fileHandle_t download;
	char		downloadName[MAX_OSPATH];
	char		downloadTempName[MAX_OSPATH + 4]; // downloadName + ".tmp"
	int			sv_allowDownload;
	char		sv_dlURL[MAX_CVAR_VALUE_STRING];
	int			downloadNumber;
	int			downloadBlock;	// block we are waiting for
	int			downloadCount;	// how many bytes we got
	int			downloadSize;	// how many bytes we got
	char		downloadList[BIG_INFO_STRING]; // list of paks we need to download
	qboolean	downloadRestart;	// if true, we need to do another FS_Restart because we downloaded a pak


	// demo information
	char		demoName[MAX_OSPATH];
	char		recordName[MAX_OSPATH]; // without extension
	qboolean	explicitRecordName;
	char		recordNameShort[TRUNCATE_LENGTH]; // for recording message
	qboolean	spDemoRecording;
	qboolean	demorecording;
	qboolean	demoplaying;
	qboolean	demowaiting;	// don't record until a non-delta message is received
	qboolean	firstDemoFrameSkipped;
	fileHandle_t	demofile;
	fileHandle_t	recordfile;

	int		timeDemoFrames;		// counter of rendered frames
	int		timeDemoStart;		// cls.realtime before first frame
	int		timeDemoBaseTime;	// each frame will be at this time + frameNum * 50

	float	aviVideoFrameRemainder;
	float	aviSoundFrameRemainder;
	int		aviFrameEndTime;
	char	videoName[MAX_QPATH];
	int		videoIndex;

	// big stuff at end of structure so most offsets are 15 bits or less
	netchan_t	netchan;

	// simultaneous demo playback and recording
	int		eventMask;
	int		demoCommandSequence;
	int		demoDeltaNum;
	int		demoMessageSequence;

} clientConnection_t;

// 'clc' is no longer a bare global — see clientApp_t below. Macro alias
// preserves every `clc.field` site; single-app resolves to clientApps[0].clc.

/*
==================================================================

the clientStatic_t structure is never wiped, and is used even when
no client connection is active at all

==================================================================
*/

typedef struct {
	netadr_t	adr;
	unsigned int	start;
	unsigned int	timeout;
	unsigned int	generation;
	clPingOwner_t	owner;
	int			time;
	char		challenge[33];
	char		info[MAX_INFO_STRING];
} ping_t;

typedef struct {
	netadr_t	adr;
	char	  	hostName[MAX_HOSTNAME_LENGTH];
	char	  	mapName[MAX_NAME_LENGTH];
	char	  	game[MAX_NAME_LENGTH];
	int			netType;
	int			gameType;
	int		  	clients;
	int		  	maxClients;
	int			minPing;
	int			maxPing;
	int			ping;
	qboolean	visible;
	int			punkbuster;
	int			g_humanplayers;
	int			g_needpass;
} serverInfo_t;

/* Strict parser for untrusted getinfo/infoResponse browser metadata. */
qboolean CL_ParseServerInfoResponse( const char *info, int expectedProtocol,
	serverInfo_t *server );
qboolean CL_ServerInfoChallengeMatches( const char *info, const char *expectedChallenge );

/* ---- Async CL_DownloadsComplete state machine ----------------
 *
 * Mirrors the SV_SpawnServer_Tick pattern (sv_init.c).  CL_DownloadsComplete
 * used to run CL_FlushMemory + CL_InitCGame back-to-back in one call,
 * blocking the engine for ~2 s while the cgame VM initialised and registered
 * every asset.  The chunked version drives one phase per Com_Frame tick so
 * the console/loading screen stays responsive between phases.  The
 * cgame-VM-internal CG_INIT asset loop is still synchronous
 * within that single phase — the VM cooperative-yield contract is the
 * architectural follow-up. */
typedef enum {
	DLC_IDLE = 0,            /* not in flight                              */
	DLC_P1_EVENTLOOP,        /* Com_EventLoop + abort-if-state-changed     */
	DLC_P2_FLUSH_MEMORY,     /* CL_FlushMemory                             */
	DLC_P3_INIT_CGAME,       /* CL_InitCGame (single bounded hitch — VM)   */
	DLC_P4_FINALIZE          /* callvote, pure checksums, ready, packet    */
} cl_dlcomplete_phase_t;

typedef struct {
	cl_dlcomplete_phase_t phase;
} cl_dlcomplete_state_t;

typedef struct {
	// ---- clientStatic_t process-wide tier --------------------------------
	// (server-client-decoupling): the per-connection "tail" that used
	// to live here (state, gameSwitch, servername, cgameStarted, startCgame,
	// cgameBsp, captureWidth/Height, dlcomplete) has moved into clientApp_t
	// (per-app-instance container). Everything that REMAINS here is a true
	// process-wide singleton — one GPU/audio device, one global frame clock,
	// the master-server browser caches, and the glconfig-derived display
	// geometry (con_factor is computed from glconfig in CL_SetScaling, NOT
	// per connection). These stay global across N apps and are never
	// DI-threaded.

	// when the server clears the hunk, all of these must be restarted
	qboolean	rendererStarted;
	qboolean	wiredUIStarted;
	qboolean	soundStarted;
	qboolean	soundRegistered;
	qboolean	uiStarted;

	int			framecount;
	int			frametime;			// msec since last frame

	int			realtime;			// ignores pause
	int			realFrametime;		// ignoring pause, so console always works

	int			numlocalservers;
	serverInfo_t	localServers[MAX_OTHER_SERVERS];

	int			numglobalservers;
	unsigned int	globalServerGeneration;
	serverInfo_t  globalServers[MAX_GLOBAL_SERVERS];
	// additional global servers
	int			numGlobalServerAddresses;
	netadr_t		globalServerAddresses[MAX_GLOBAL_SERVERS];

	int			numfavoriteservers;
	serverInfo_t	favoriteServers[MAX_OTHER_SERVERS];

	int pingUpdateSource;		// source currently pinging or updating

	// update server info
	char		updateInfoString[MAX_INFO_STRING];

	// rendering info
	glconfig_t	glconfig;
	int			glconfigGeneration;	// bumped on every re.BeginRegistration; cgame polls it to detect resolution changes
	qhandle_t	whiteShader;
	qhandle_t	consoleShader;

	int			lastVidRestart;
	int			soundMuted;

	// glconfig-derived display geometry — recomputed in CL_SetScaling on cvar
	// change (con_factor). NOT per-connection; stays process-wide.
	float		con_factor;

} clientStatic_t;

extern int bigchar_width;
extern int bigchar_height;
extern int smallchar_width;
extern int smallchar_height;

extern	clientStatic_t		cls;

/*
==================================================================

clientApp_t — per-app-instance client-state container (server-client-
decoupling).

Holds everything that is logically scoped to ONE client app-instance
(a connection + its cgame): the active gameplay state (cl), the
connection/protocol state (clc), and the "tail" of per-connection
fields that historically lived in the process-wide clientStatic_t
(state, gameSwitch, servername, the cgame boot flags/BSP, video-capture
dimensions, and the async-download state machine).

Today the runtime instantiates exactly ONE app, pinned at clientApps[0];
the array capacity mirrors the VM tier's vmTable_cgame[MAX_LOCAL_CGAME_VMS]
so the per-app spawn path fills higher slots without reshaping
the container. Every legacy `cl.x` / `clc.y` access resolves — via the
macro aliases below — to clientApps[0], byte-identical to the former
file-scope globals. Per-connection tail fields (`cls.state` etc.) are
rewritten to the explicit container form (app->state / the active-app
accessor) since their bare names collide with locals across the tree.

The `cgvm` field is the authoritative per-app cgame VM handle (the former
bare `cgvm` global was deglobalized into it). Reads split by call context:
INSIDE a cgame syscall the active VM comes from VM_ActiveNativeVM() (the token
VM_Call sets around the vmMain call on BOTH the native and WASM backends, valid
only while a vmMain is on the C stack); call-initiating / lifecycle reads use
this field via clientActiveApp.

==================================================================
*/
typedef enum {
	DEMO_MESSAGE_ABORT_NONE = 0,
	DEMO_MESSAGE_ABORT_ILLEGAL_SVC,
	DEMO_MESSAGE_ABORT_SNAPSHOT_AREAMASK
} demoMessageAbortKind_t;

typedef struct clientApp_s {
	clientActive_t		cl;		// per-connection gameplay/frame state
	clientConnection_t	clc;	// per-connection protocol/connection state

	// per-connection "tail" relocated out of clientStatic_t
	connstate_t	state;			// connection status
	qboolean	gameSwitch;
	char		servername[MAX_OSPATH];	// server from original connect (reconnect)
	qboolean	cgameStarted;
	qboolean	startCgame;
	mapFile_t	*cgameBsp;
	int			captureWidth;
	int			captureHeight;
	cl_dlcomplete_state_t dlcomplete;	// async CL_DownloadsComplete machine

	vm_t		*cgvm;			// per-app cgame VM handle (the deglobalized cgvm)

	// per-connection CL_Frame-op state relocated out of cl_main.c file-statics
	// (in-process-queue L6) so a 2nd live app does not collide. Zero-init matches
	// the former static initializers; at N=1 only app[0]'s copy is touched.
	char		pendingConnectError[512];	// was static cl_pendingConnectError
	int			lastWarmupValue;			// was static cl_lastWarmupValue
	int			matchAlertExpire;			// was static cl_matchAlertExpire
	qboolean	timeoutWasBothPaused;		// was static wasBothPaused (CL_CheckTimeout)
	qboolean	disconnecting;				// was static cl_disconnecting (CL_Disconnect reentry guard)
	/* Process-lifetime connection epoch. Unlike clc.quic_conn (whose public
	 * handle may be reused by the next transport allocation), this advances on
	 * every successful transport connect and is not cleared by CL_Disconnect. */
	uint64_t	connectionGeneration;

	// Narrow local recovery boundary for a structurally valid demo frame with
	// a typed semantic parse failure. Live network parsing never arms this boundary.
	qboolean	demoMessageAbortArmed;
	int		demoMessageAbortCommand;
	demoMessageAbortKind_t demoMessageAbortKind;
	int		demoMessageAbortDetail;
	jmp_buf		demoMessageAbort;

	// Automated acceptance seam for the next successfully opened demo only.
	// The file adapter consumes this byte budget and then returns one typed
	// read error; normal VFS reads and live-network parsing never see it.
	qboolean	demoReadFaultArmed;
	int		demoReadFaultAfterBytes;
	int		demoReadFaultRemaining;

	// Per-app TERM_CLIENT_DROP/LEAVE/KICK recovery target. Armed each frame at
	// the CL_Frame call boundary (common.c); Com_Terminate longjmps here (via the
	// cl_frameApp cursor) instead of the process-global abortframe, so a recoverable
	// error in one app does not abort co-resident apps. At N=1 this is clientApps[0]
	// and the recovery is behaviorally identical to the global path.
	jmp_buf		appAbortFrame;
} clientApp_t;

// Single-app today: only slot 0 is ever touched. MAX_LOCAL_CGAME_VMS comes
// from the VM tier (vm_local.h) so the container and the cgame-VM table share
// one capacity bound.
extern	clientApp_t		clientApps[MAX_LOCAL_CGAME_VMS];

// The active app-instance. Single-app: always &clientApps[0] (pinned). The
// per-app spawn path repoints this from input focus; the accessor
// signature stays stable. Pull-model, engine-owned pointer identity — never
// derived from clc.clientNum (tier rule).
extern	clientApp_t		*clientActiveApp;

// The `cl`/`clc` macro aliases were retired: all access is now explicit
// through the active-app handle — `clientActiveApp->cl` / `clientActiveApp->clc`
// (single-app: clientActiveApp == &clientApps[0], byte-identical to the former
// globals). Making access explicit is the prerequisite for N-app: clientApps[i]
// for i>0 can't be reached through a macro hardcoded to [0]. There are no
// per-app loops yet (no spawn path), so every site is "the active
// app"; a site meaning "a specific app i" would be threaded an app pointer then.

// Active-app accessor (pull-model, client-tier). The WUI activation predicates
// under wired/ui/policy/ read connection state through THIS, never through a
// threaded clientApp_t* (which would leak a client-tier type into WiredUI-core
// — see the tier note). Single-app returns &clientApps[0]; a later pass repoints
// it from input focus. Never NULL.
clientApp_t *CL_ActiveApp( void );

// The active app's cgame slot index (== its cgameInstance). Lets engine-side
// level-transition teardown scope its VM clear to the active app. Single-app: 0.
int CL_ActiveCgameInstance( void );

// True if any app owning a registered viewport provider is in a world-rendering
// state (in-game, or primed with a cgame VM). Lets the world-viewport layer
// activate per owning-app rather than per input-focus. Returns a plain bool so
// the WiredUI-tier policy consumes no clientApp_t* (tier-clean).
qboolean CL_AnyViewportAppRenderable( void );

extern	char		cl_oldGame[MAX_QPATH];
extern	qboolean	cl_oldGameSet;

// Loading screen progress tracking — updated from various load callsites,
// read by cl_loading_ui.c to render progress bars
typedef struct {
	float		geometry;	// 0.0–1.0: BSP geometry loading
	float		shaders;	// 0.0–1.0: shader/texture compilation and upload
	float		audio;		// 0.0–1.0: sound loading
	float		download;	// 0.0–1.0: pak file downloads (from CL_DownloadLoop)
	float		overall;	// 0.0–1.0: weighted average of all phases
	const char	*phase;		// current human-readable phase label (e.g. "compiling shaders")
	int			startTime;	// cls.realtime when loading began (for minimum display time)
} clLoadProgress_t;

extern clLoadProgress_t cl_loadProgress;

void CL_LoadingYield( const char *phaseName );


//=============================================================================

// The cgame VM handle is per-app: clientApps[N].cgvm (reach the active one via
// clientActiveApp->cgvm or CL_ActiveApp()->cgvm). No bare global cgvm.
extern	refexport_t		re;		// interface to refresh .dll


//
// cvars
//
extern	cvar_t	*cl_noprint;
extern	cvar_t	*cl_debugMove;
extern	cvar_t	*cl_timegraph;
extern	cvar_t	*cl_shownet;
extern	cvar_t	*cl_autoNudge;
extern	cvar_t	*cl_timeNudge;
extern	cvar_t	*cl_showTimeDelta;

extern	cvar_t	*com_timedemo;
extern	cvar_t	*cl_aviFrameRate;
extern	cvar_t	*cl_aviMotionJpeg;
extern	cvar_t	*cl_aviPipeFormat;

extern	cvar_t	*cl_activeAction;

extern	cvar_t	*cl_allowDownload;
extern	cvar_t	*cl_conXOffset;
extern	cvar_t	*cl_conYOffset;
extern	cvar_t	*cl_conColor;
extern	cvar_t	*cl_inGameVideo;

extern	cvar_t	*cl_lanForcePackets;
extern	cvar_t	*cl_autoRecordDemo;
extern	cvar_t	*cl_drawRecording;

extern	cvar_t	*com_maxfps;

extern	cvar_t	*vid_xpos;
extern	cvar_t	*vid_ypos;
extern	cvar_t	*r_noborder;

extern	cvar_t	*r_allowSoftwareGL;
extern	cvar_t	*r_swapInterval;
extern	cvar_t	*r_glDriver;

extern	cvar_t	*r_displayRefresh;
extern	cvar_t	*r_fullscreen;
extern	cvar_t	*r_mode;
extern	cvar_t	*r_modeFullscreen;
extern	cvar_t	*r_customwidth;
extern	cvar_t	*r_customheight;
extern	cvar_t	*r_customPixelAspect;
extern	cvar_t	*r_colorbits;
extern	cvar_t	*cl_stencilbits;
extern	cvar_t	*cl_depthbits;
extern	cvar_t	*cl_drawBuffer;

//=================================================

//
// cl_main
//
void CL_AddReliableCommand( clientApp_t *app, const char *cmd, qboolean isDisconnectCmd );

/* Typed browser connect path.  The caller has already normalized `target`
 * and resolved `address`; no credential enters Cbuf, a cvar, or a log. */
qboolean CL_ConnectBrowserServer( const char *target, const netadr_t *address,
	const char *joinPassword, int selectionGeneration );

void CL_StartHunkUsers( void );

void CL_Disconnect_f( void );
void CL_ReadDemoMessage( void );
void CL_StopRecord_f( void );
// Close the console as part of entering a connection/demo, preserving it under
// an attract reel. Interim single decision point; folds into the CL_SetState
// transition observer when the console-ownership-decoupling workstream lands.
void CL_ConsoleCloseForConnect( void );
// The single funnel for client connection-state changes. Route every
// `app->state = CA_XXX` through this so the transition is observable in one
// place (see CL_OnClientStateChanged). Only the focused app's edges notify.
void CL_SetState( clientApp_t *app, connstate_t newState );

void CL_InitDownloads( clientApp_t *app );
void CL_NextDownload( void );

void CL_GetPing( int n, char *buf, int buflen, int *pingtime );
void CL_GetPingInfo( int n, char *buf, int buflen );
int CL_GetPingQueueCount( void );

void CL_ClearState( clientApp_t *app );

int CL_ServerStatus( const char *serverAddress, char *serverStatusString, int maxLen );
qboolean CL_NormalizeServerAddress( const char *input, netadrtype_t family,
	char *normalized, int normalizedSize, netadr_t *address );

qboolean CL_CheckPaused( void );
qboolean CL_NoDelay( void );

qboolean CL_GetModeInfo( int *width, int *height, float *windowAspect, int mode, const char *modeFS, int dw, int dh, qboolean fullscreen );


//
// cl_input
//
void CL_InitInput( void );
void CL_ClearInput( void );
void CL_SendCmd( void );
void CL_WritePacket( clientApp_t *app, int repeat );
void CL_SendAckOnly( clientApp_t *app );	// per-app ack-only datagram (in-process-queue L5)

//
// cl_keys.c
//
extern  field_t     chatField;
extern  field_t     g_consoleField;

void Field_Draw( field_t *edit, int x, int y, int width, qboolean showCursor, qboolean noColorEscape );
void Field_BigDraw( field_t *edit, int x, int y, int width, qboolean showCursor, qboolean noColorEscape );

//
// cl_parse.c
//
extern int cl_connectedToPureServer;

void CL_ParseServerMessage( clientApp_t *app, msg_t *msg );
void CL_RecordCommittedSnapshot( clientApp_t *app );
void CL_CheckReliableStreams( void );
void CL_CheckSnapshotDatagrams( void );

//====================================================================

qboolean CL_UpdateVisiblePings_f( int source );
qboolean CL_ValidPakSignature( const byte *data, int len );


//
// console
//

extern cvar_t *con_scale;

void Con_CheckResize( void );
void Con_Init( void );
void Con_Shutdown( void );
void Con_InitProjection( void );      /* UI presentation-only setup (after Con_Init) */
void Con_ShutdownProjection( void );  /* mirrors Con_InitProjection (UI teardown) */
void Con_ToggleConsole_f( void );
void Con_ClearNotify( void );
void Con_RunConsole( qboolean consoleKeyActive, int frameMsec );
void Con_DrawConsole( void );
void Con_PageUp( int lines );
void Con_PageDown( int lines );
void Con_Top( void );
void Con_Bottom( void );
void Con_Close( void );
void Con_SoftClose( void );     /* collapse visually, preserve KEYCATCH_CONSOLE */

// console search
void Con_SearchOpen( void );
void Con_SearchClose( void );
void Con_SearchNext( qboolean forward );
void Con_SearchChar( int ch );
qboolean Con_IsSearchActive( void );
int  Con_SearchLine( void );

// console mark mode (Ctrl+M text selection)
void Con_MarkOpen( void );
void Con_MarkClose( void );
qboolean Con_IsMarkActive( void );
qboolean Con_MarkKey( int key, qboolean ctrlDown, qboolean shiftDown );

void CL_LoadConsoleHistory( void );
void CL_SaveConsoleHistory( void );

//
// cl_scrn.c
//
void	SCR_Init( void );
void	SCR_Done( void );
void	SCR_UpdateScreen( void );
void CL_ProfileTelemetry_Init( void );
void CL_ProfileTelemetry_AfterEndFrame( const refexport_t *renderer );
void CL_ProfileTelemetry_RendererStarted( void );
void CL_ProfileTelemetry_RendererStopping( void );
void CL_ProfileTelemetry_Shutdown( void );

void	SCR_DebugGraph( float value );

void	SCR_DrawStringExt( int x, int y, float size, const char *string, const float *setColor, qboolean forceColor, qboolean noColorEscape );

//
// cl_net_stats.c
//
void	SCR_NetStatsInit( void );
/* outbound-packet counter: incremented by cl_input.c, registered/owned by
 * cl_net_stats.c, read by the debug_packets custom-draw handler
 * as a first-frame zero-division guard. */
extern int cl_sent;

//
// cl_loading_ui.c
//
/* CL_DrawLoadingScreen retired — compositor sole loading-screen
 * renderer via loading_screen.wmenu. */
void	CL_LoadingScreenFinished( void );
void	CL_ResetLoadingScreenState( void );
/* Per-CL_Frame publishers — fired before render dispatch so compositor
 * reads current-frame values. Loading + connect state surfaced to the
 * WiredStore for storeBind/bindwidth wmenu items. */
void	CL_PublishLoadingState( void );
void	CL_PublishConnectState( void );

//
// cl_cin.c
//

void CL_PlayCinematic_f( void );
void SCR_DrawCinematic (void);
void SCR_RunCinematic (void);
void SCR_StopCinematic (void);
int CIN_PlayCinematic( const char *arg0, int xpos, int ypos, int width, int height, int bits);
e_status CIN_StopCinematic(int handle);
e_status CIN_RunCinematic (int handle);
void CIN_DrawCinematic (int handle);
void CIN_SetExtents (int handle, int x, int y, int w, int h);
void CIN_UploadCinematic(int handle);
void CIN_CloseAllVideos(void);

//
// cl_cgame.c
//
void CL_InitCGame( clientApp_t *app );
void CL_PrimeHeadlessApp( clientApp_t *app );  // VM-less prime → CA_PRIMED
void CL_ShutdownCGame( clientApp_t *app );
qboolean CL_GameCommand( void );
void CL_CGameRendering( stereoFrame_t stereo );
void CL_RenderCGameViewport( void *ownerCgvm, int vmKey, int x, int y, int w, int h );
void CL_SetCGameTime( clientApp_t *app );
void CL_DriveHeadlessApp( clientApp_t *app );  // VM-less per-app state advance (CA_PRIMED→CA_ACTIVE)

//
// cl_characters.c
//
typedef struct {
	qboolean            loaded;
	char                dirname[MAX_QPATH];
	characterManifest_t manifest;
	qhandle_t           iconHandle;
	// Player character-select visibility. TRUE (the default when the manifest
	// omits the key) → the character appears in the player select screen AND is
	// spawnable. FALSE → creature-only: spawnable as a monster (loaded by name,
	// not via the select feeder) but hidden from the select screen. Client-only:
	// this wraps the serialized `manifest` and is never sent across the CL↔CG
	// boundary (only `manifest` is memcpy'd in CL_Characters_GetManifest).
	qboolean            selectable;
	// Authored bot-menu opt-in, made true only when a packaged primary mesh is
	// present. Manifest load alone is not sufficient renderability authority.
	qboolean            botEligible;
} clCharacterEntry_t;

void        CL_Characters_Init( void );
void        CL_Characters_Reload( void );
void        CL_Characters_Shutdown( void );
void        CL_Characters_RegisterIcons( void );
void        CL_Characters_RegisterShaders( void );
const characterManifest_t *CL_Characters_Get( const char *dirname );
int         CL_Characters_Count( void );
const clCharacterEntry_t  *CL_Characters_At( int index );
// Selectable-only view over the registry: enumerates just the characters that
// are player-selectable (selectable==qtrue), so the character-select feeder can
// use one index space for its count and its per-item lookups. SelectableAt(i)
// maps subset index i → the underlying registry entry (NULL if out of range).
int         CL_Characters_SelectableCount( void );
const clCharacterEntry_t  *CL_Characters_SelectableAt( int index );
qboolean    CL_Characters_IsBotEligible( const char *dirname );
unsigned int CL_Characters_Generation( void );
qboolean    CL_Characters_GetManifest( const char *charName, char *buf, int bufSize );
const cmSkin_t *CL_GetCharacterSkin( qhandle_t handle );

//
// cl_ui.c
//
void CL_ShutdownUI( void );
int Key_GetCatcher( void );
void Key_SetCatcher( int catcher );



//
// cl_avi.c
//
qboolean CL_OpenAVIForWriting( const char *filename, qboolean pipe, qboolean reopen );
void CL_TakeVideoFrame( void );
void CL_WriteAVIVideoFrame( const byte *imageBuffer, int size );
void CL_WriteAVIAudioFrame( const byte *pcmBuffer, int size );
qboolean CL_CloseAVI( qboolean reopen );
qboolean CL_VideoRecording( void );

//
// cl_jpeg.c
//
size_t	CL_SaveJPGToBuffer( byte *buffer, size_t bufSize, int quality, int image_width, int image_height, byte *image_buffer, int padding );
void	CL_SaveJPG( const char *filename, int quality, int image_width, int image_height, byte *image_buffer, int padding );
void	CL_LoadJPG( const char *filename, unsigned char **pic, int *width, int *height );


//
// cl_bsp_preview.c
//
#define MAP_PREVIEW_MAX_EDGES    8192
#define MAP_PREVIEW_MAX_MARKERS  256

typedef struct {
	float x1, y1, x2, y2;  // 2D projected edge (top-down XY)
	float z1, z2;           // original Z height per vertex (for height coloring)
	int   type;             // 0=outer hull, 1=internal, 2=floor
} mapPreviewEdge_t;

typedef struct {
	float x, y;             // 2D position
	int   type;             // 0=spawn, 1=item, 2=flag
} mapPreviewMarker_t;

typedef struct {
	mapPreviewEdge_t  edges[MAP_PREVIEW_MAX_EDGES];
	int               numEdges;
	mapPreviewMarker_t markers[MAP_PREVIEW_MAX_MARKERS];
	int               numMarkers;
	float             minX, minY, maxX, maxY;  // XY bounding box
	float             minZ, maxZ;              // Z height range (for height coloring)
	int               numSurfaces;  // planar surfaces parsed from BSP
	qboolean          valid;
} mapPreview_t;

extern mapPreview_t cl_mapPreview;

void CL_BuildMapPreview( const char *mapname );
void CL_ClearMapPreview( void );

//
// cl_mapinfo.c
//
typedef struct {
	char		mapName[MAX_QPATH];		// e.g. "q3dm6"
	char		longName[128];			// from worldspawn "message" or .meta
	char		series[64];				// e.g. "Pro-DM Series"
	char		archetype[32];			// "tech", "gothic", "base", or ""
	char		author[64];				// map author
	char		year[8];				// year created
	char		quote[256];				// flavor text
	char		metaWeapon[8];			// dominant weapon shortname (RL, RG, etc.)
	int			playersMin;				// min player count
	int			playersMax;				// max player count
	int			itemNodes;				// count of item/weapon entities
	char		sky[MAX_QPATH];			// worldspawn sky key (for archetype detection)
	qboolean	hasMetaFile;			// whether .meta file was found
} clMapInfo_t;

extern clMapInfo_t cl_mapInfo;

void CL_LoadMapInfo( const char *mapname );
void CL_ClearMapInfo( void );

//
// cl_loading_theme.c -- map archetype detection and loading color themes
//
typedef enum {
	ARCHETYPE_TECH,
	ARCHETYPE_GOTHIC,
	ARCHETYPE_BASE,
	ARCHETYPE_DEFAULT
} mapArchetype_t;

typedef struct {
	mapArchetype_t	archetype;
	vec4_t			bgColor;		// full-screen background fill
	vec4_t			primaryGlow;	// background radial gradient center
	vec4_t			secondaryGlow;	// background radial gradient edge
	vec4_t			accentColor;	// progress bars, wireframe, text accents
	vec4_t			gridColor;		// horizontal grid overlay (accent at 10% opacity)
} clLoadingTheme_t;

extern clLoadingTheme_t cl_loadingTheme;

mapArchetype_t	CL_DetectArchetype( const clMapInfo_t *info );
void			CL_ApplyLoadingTheme( const clMapInfo_t *info );

// cl_loading_ui.c — fade transition state
extern float		cl_loadFadeAlpha;
extern qboolean		cl_loadFading;

// base backend functions
void	HandleEvents( void );

// platform-specific
void	GLimp_InitGamma(glconfig_t *config);
void	GLimp_SetGamma(unsigned char red[256], unsigned char green[256], unsigned char blue[256]);

// OpenGL
#ifdef USE_OPENGL_API
void	GLimp_Init( glconfig_t *config );
void	GLimp_Shutdown( qboolean unloadDLL );
void	GLimp_EndFrame( void );
void	*GL_GetProcAddress( const char *name );
#endif

// Vulkan
#ifdef USE_VULKAN_API
void	VKimp_Init( glconfig_t *config );
void	VKimp_Shutdown( qboolean unloadDLL );
void	*VK_GetInstanceProcAddr( void *nativeInstance, const char *name );
const char *const *VK_GetInstanceExtensions( uint32_t *count );
qboolean VK_CreateSurface( void *nativeInstance, uint64_t *outNativeSurface );
#endif

qboolean RALimp_PresentationOpen( void *context,
	const ralPresentationHostOpenInfo_t *info,
	ralPresentationHostReceipt_t *outReceipt );
qboolean RALimp_PresentationRefresh( void *context,
	const ralPresentationHostReceipt_t *currentReceipt,
	ralPresentationHostReceipt_t *outReceipt );
qboolean RALimp_PresentationBorrow( void *context,
	const ralPresentationHostReceipt_t *currentReceipt,
	ralPresentationSurfaceBorrow_t *outBorrow );
qboolean RALimp_PresentationClose( void *context,
	const ralPresentationHostReceipt_t *currentReceipt,
	ralPresentationHostCloseMode_t mode );
