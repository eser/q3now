/*
===========================================================================
Copyright (C) 2024-2026 Wired engine contributors
This file is part of the Wired engine source code. GPLv2.
===========================================================================
*/

// stubs.c — residual symbol resolution for the extract-meta tool.
//
// qcommon_tool (this directory's static library) is a curated subset
// of the engine's qcommon sources. Even with the curation, common.c,
// log.c, cmd.c, and files.c retain link-time references to engine
// entry points the tool can't pull in:
//
//   common.c → CL_*, SV_*, CIN_*, Sys_* (Com_Quit / Com_Init paths),
//              WiredCore_Init/Shutdown, WiredScript_PostInit,
//              Com_InitPushEvent
//   log.c    → CL_*, SV_*, g_wv (debug-build window-minimize)
//   cmd.c    → CL_GameCommand, SV_GameCommand,
//              CL_ForwardCommandToServer
//   files.c  → Sys_ListFiles (real Win32 wrapper, not a stub)
//
// All CL_*/SV_*/CIN_*/Wired*/Com_InitPushEvent stubs are no-op; they
// resolve link symbols only. The tool's bootstrap (Tool_Init →
// FS_InitFilesystem → FS_ListFiles → FS_FreeFileList → Tool_Shutdown)
// never enters the call chains that would invoke any of them. They
// would fire only on Com_Quit / Com_GameRestart / Com_Frame paths,
// none of which the tool reaches.
//
// Sys_ListFiles is the lone exception: FS_InitFilesystem calls it
// to discover .pk3 / .sw3z packs, and the tool's whole purpose is
// file enumeration. Sys_ListFiles here is a real Win32 wrapper
// (FindFirstFileA / FindNextFileA + extension filter), modeled on
// the engine's win_main.c version but trimmed to what files.c
// actually requests.

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "q_shared.h"
#include "qcommon.h"
#include "wired/core/logging/log.h"   /* log_sink_t */

#ifdef _WIN32
#  include <windows.h>
#  include <io.h>
#  include "../../code/win32/win_local.h"   // WinVars_t for g_wv
#endif

// =====================================================================
// CL_* — never reached from tool path
// =====================================================================

void     CL_Init                    ( void ) { /* unreached */ }
void     CL_Characters_Init         ( void ) { /* unreached */ }
void     CL_AbortFrame              ( void ) { /* unreached */ }
qboolean CL_DemoPlaying             ( void ) { return qfalse; }
qboolean CL_Disconnect              ( qboolean showMainMenu ) { (void)showMainMenu; return qfalse; }
void     CL_Shutdown                ( const char *finalmsg, qboolean quit ) { (void)finalmsg; (void)quit; }
void     CL_Frame                   ( int msec, int realMsec ) { (void)msec; (void)realMsec; }
qboolean CL_GameCommand             ( void ) { return qfalse; }
void     CL_ForwardCommandToServer  ( const char *string ) { (void)string; }
void     CL_ShutdownAll             ( void ) { /* unreached */ }
void     CL_ClearMemory             ( void ) { /* unreached */ }
void     CL_FlushMemory             ( void ) { /* unreached */ }
void     CL_StartHunkUsers          ( void ) { /* unreached */ }
void     CL_SystemInfoChanged       ( qboolean onlyGame ) { (void)onlyGame; }
qboolean CL_GameSwitch              ( void ) { return qfalse; }
void     CL_ShutdownCGame           ( void ) { /* unreached */ }
void     CL_ShutdownUI              ( void ) { /* unreached */ }
void     CIN_CloseAllVideos         ( void ) { /* unreached */ }

// =====================================================================
// SV_* — never reached from tool path
// =====================================================================

void     SV_Init                    ( void ) { /* unreached */ }
void     SV_Shutdown                ( const char *finalmsg ) { (void)finalmsg; }
void     SV_SpawnServer_Tick        ( void ) { /* unreached */ }
void     SV_Frame                   ( int msec ) { (void)msec; }
int      SV_FrameMsec               ( void ) { return 0; }
qboolean SV_GameCommand             ( void ) { return qfalse; }
int      SV_SendQueuedPackets       ( void ) { return 0; }
void     SV_AddDedicatedCommands    ( void ) { /* unreached */ }
void     SV_RemoveDedicatedCommands ( void ) { /* unreached */ }
void     SV_ShutdownGameProgs       ( void ) { /* unreached */ }

// =====================================================================
// Wired core / scripting — orchestration calls inside Com_Init/Shutdown
// =====================================================================

void WiredCore_Init        ( void ) { /* unreached */ }
void WiredCore_Shutdown    ( void ) { /* unreached */ }
void WiredScript_PostInit  ( void ) { /* unreached */ }

// =====================================================================
// Event subsystem — events/event.c excluded
// =====================================================================
//
// common.c::Com_Init / Com_Frame call into the event queue. Tool
// never enters either path (bootstrap stops at FS_InitFilesystem).

void Com_InitPushEvent( void ) { /* unreached on tool path */ }
int  Com_EventLoop    ( void ) { return 0; /* unreached */ }

// Com_Milliseconds is defined in events/event.c (alongside the
// pushed-event queue). Tool path: nothing in FS_InitFilesystem
// requires it, but common.c::Com_Frame uses it. Keeping the call
// signature; trivial wrapper around the OS clock so any callsite
// reached during tool runs gets a sane value rather than 0.
int Com_Milliseconds( void ) {
#ifdef _WIN32
	return (int)GetTickCount();
#else
	return 0;
#endif
}

// =====================================================================
// VM machinery — vm.c excluded; common.c references at link time
// =====================================================================

void VM_Init                  ( void ) { /* unreached */ }
void VM_Clear                 ( void ) { /* unreached */ }
void VM_Forced_Unload_Start   ( void ) { /* unreached */ }
void VM_Forced_Unload_Done    ( void ) { /* unreached */ }
int  VM_GetCallStack          ( void *vm, char *out, int outsize ) {
	(void)vm;
	if ( out && outsize > 0 ) out[0] = '\0';
	return 0;
}

// =====================================================================
// Network init — net_ip.c excluded
// =====================================================================
//
// common.c::Com_Init calls NET_Init; Com_Frame uses NET_Sleep and
// NET_FlushPacketQueue. Tool path doesn't enter Com_Init/Frame.

void     NET_Init             ( void ) { /* unreached */ }
qboolean NET_Sleep            ( int timeout ) { (void)timeout; return qfalse; }
void     NET_FlushPacketQueue ( int timeout ) { (void)timeout; }

// =====================================================================
// BSP / map loaders — formerly stubbed; B2 re-enables maps/*.c and
// cm_q1.c / nav/nav_coord.c in the tool's source list, so the real
// implementations are linked in. No stubs needed.
// =====================================================================

// =====================================================================
// MSG diagnostics — msg.c excluded
// =====================================================================
//
// common.c registers `changeVectors` cmd at Com_Init time pointing
// at MSG_ReportChangeVectors_f. The cmd handler is never invoked on
// tool path (no console input loop), so a no-op is safe.

void MSG_ReportChangeVectors_f( void ) { /* unreached */ }

// =====================================================================
// Console sink — log_sink_console.c excluded
// =====================================================================
//
// common.c calls Log_RegisterConsoleSink / Log_UnregisterConsoleSink
// during Com_Init / Com_Shutdown. Tool path uses the stderr fallback
// sink instead (see tool_init.c::Tool_Init).

log_sink_t *Log_RegisterConsoleSink   ( void ) { return NULL; /* unreached */ }
void        Log_UnregisterConsoleSink ( void ) { /* unreached */ }

// =====================================================================
// CM trace counters — formerly stubbed; cm_*.c sources are now in
// QCOMMON_TOOL_SRCS (B2 brought them back in via the BSP_Load chain),
// and the counters are defined inside cm_trace.c. No tool-side storage
// needed.
// =====================================================================

// =====================================================================
// BotDrawDebugPolygons — cm_patch.c references it (botlib hook for
// debug overlays). botlib isn't linked into the tool. cm_patch.c's
// caller is the in-game patch debug-draw, never reached on tool path.
// =====================================================================

void BotDrawDebugPolygons( void (*drawPoly)(int color, int numPoints, float *points), int value ) {
	(void)drawPoly; (void)value; /* unreached on tool path */
}

// =====================================================================
// Sys_* — common.c shutdown chain refs (link-only on tool path)
// =====================================================================

NORETURN void Sys_Quit( void ) {
	fputs( "extract-meta: Sys_Quit (unexpected)\n", stderr );
	exit( 0 );
}

void Sys_ShowConsole( int level, qboolean quitOnClose ) {
	(void)level; (void)quitOnClose;   /* tool has no console window */
}

uint64_t Sys_GetAffinityMask( void ) {
	return 0;   /* engine treats 0 as "no preference" */
}

// =====================================================================
// Sys_ListFiles — real Win32 wrapper
// =====================================================================
//
// Used by FS_InitFilesystem (scans .pk3 / .sw3z) and by FS_ListFiles
// (the tool's main use case). Modeled on win_main.c's version but
// trimmed: the tool exercises only flat scans with a single extension
// filter or the directory-only "/" sentinel. FS_MATCH_FILTER and
// recursive scans aren't needed for B1's enumeration.

#ifdef _WIN32

#define MAX_FOUND_FILES_TOOL 4096

char **Sys_ListFiles( const char *directory, const char *extension, const char *filter, int *numfiles, int subdirs ) {
	(void)filter;     /* tool path doesn't use FS_MATCH_FILTER */
	(void)subdirs;    /* tool path is flat-scan only */

	if ( !numfiles ) return NULL;
	*numfiles = 0;

	if ( !directory || !*directory ) return NULL;
	if ( !extension ) extension = "";

	const qboolean dirsOnly = ( extension[0] == '/' && extension[1] == '\0' );
	const int      extLen   = (int)strlen( extension );

	char findpath[ MAX_OSPATH ];
	Com_sprintf( findpath, sizeof( findpath ), "%s\\*", directory );

	WIN32_FIND_DATAA fd;
	HANDLE h = FindFirstFileA( findpath, &fd );
	if ( h == INVALID_HANDLE_VALUE ) return NULL;

	char *names[ MAX_FOUND_FILES_TOOL ];
	int   count = 0;

	do {
		if ( fd.cFileName[0] == '.' &&
		     ( fd.cFileName[1] == '\0' ||
		       ( fd.cFileName[1] == '.' && fd.cFileName[2] == '\0' ) ) ) {
			continue;
		}

		const qboolean isDir = ( fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ) != 0;

		if ( dirsOnly ) {
			if ( !isDir ) continue;
		} else {
			if ( isDir ) continue;
			if ( extLen ) {
				const int nameLen = (int)strlen( fd.cFileName );
				if ( nameLen < extLen ) continue;
				if ( Q_stricmp( fd.cFileName + nameLen - extLen, extension ) != 0 ) continue;
			}
		}

		if ( count >= MAX_FOUND_FILES_TOOL - 1 ) break;
		const size_t len = strlen( fd.cFileName ) + 1;
		char *copy = (char *)malloc( len );
		if ( !copy ) break;
		memcpy( copy, fd.cFileName, len );
		names[count++] = copy;
	} while ( FindNextFileA( h, &fd ) );

	FindClose( h );

	if ( count == 0 ) return NULL;

	char **out = (char **)malloc( ( count + 1 ) * sizeof( char * ) );
	if ( !out ) {
		for ( int i = 0; i < count; i++ ) free( names[i] );
		return NULL;
	}
	for ( int i = 0; i < count; i++ ) out[i] = names[i];
	out[count] = NULL;
	*numfiles = count;
	return out;
}

#endif // _WIN32

// =====================================================================
// g_wv — Win32 globals storage
// =====================================================================
//
// log.c references g_wv.hWnd inside `#if defined(_WIN32) && defined(_DEBUG)`
// for ShowWindow on fatal error. Definition normally lives in
// win_main.c which the tool intentionally doesn't link. Storage
// here lets the linker resolve; the field is never read on the
// tool's normal exit path because Sys_Error fires first.

#ifdef _WIN32
WinVars_t g_wv;
#endif
