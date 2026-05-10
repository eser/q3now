/*
===========================================================================
Copyright (C) 2024 Wired engine contributors

This file is part of Quake III Arena source code.
Released under GPLv2 — see code/qcommon/maps/meta.h for the full notice.
===========================================================================
*/

// tool_init.c -- bootstrap for the extract-meta tool.
//
// The tool links against qcommon_static (the engine's qcommon code as a
// static library) and reuses its VFS / cvar / log / time machinery. It
// does NOT link the engine's win_main.c / unix_main.c (which would clash
// with the tool's own main()), nor the renderer / client / server / VM
// targets. As a result, this file must:
//
//   1. Provide replacement Sys_* platform symbols for the ones the
//      qcommon_static archive references but win_main.c would normally
//      supply (Sys_FOpen, Sys_GetFileStats, Sys_DefaultBasePath, the
//      mutex primitives, etc.).
//
//   2. Drive the engine's bootstrap sequence (Com_InitPushEvent,
//      Com_InitSmallZoneMemory, Cvar_Init, Cmd_Init, FS_InitFilesystem)
//      in the same order Com_Init does — but skipping subsystem inits
//      that pull in client/server/renderer dependencies the tool can't
//      satisfy.
//
//   3. Override Sys_Error so a fatal failure during init exits cleanly
//      (status 3) instead of trying to longjmp into the engine's
//      abortframe (which the tool never sets up).
//
// Windows-only platform stubs in this round; Linux/macOS stubs land
// when the tool's first non-Windows user shows up.

#include <stdint.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "q_shared.h"
#include "qcommon.h"
#include "arena.h"

#ifdef _WIN32
#  include <windows.h>
#  include <shlobj.h>
#  include <sys/stat.h>
#endif

// Tool_Arena is the per-run arena used by shader_index, asset_resolve
// scratch buffers, and any other tool-only allocations that should
// outlive a single function but die at tool exit. Created in
// Tool_Init after Z_Malloc is available; freed in Tool_Shutdown.
arena_t *Tool_Arena = NULL;

// Forward decls for engine internals the tool calls. These live in
// qcommon_tool. Most are publicly declared in qcommon.h; the two
// zone allocator initializers (Com_InitSmallZoneMemory /
// Com_InitZoneMemory) are exposed by the B1.5 Parts 1+2 surgical
// edits to common.c + qcommon.h.
extern void Cvar_Init( void );
extern void Cmd_Init ( void );
extern void Cbuf_Init( void );
extern void FS_InitFilesystem( void );
extern void FS_Shutdown( qboolean closemfp );
extern qboolean Log_RegisterFallbackStderrSink( void );
extern void     Log_UnregisterFallbackStderrSink( void );
extern cvar_t  *Cvar_Get( const char *var_name, const char *value, int flags );
extern void     Cvar_Set( const char *var_name, const char *value );
extern void     BSP_Init( void );          /* registers Q3+Q1 bspFormats */
extern void     BSP_Shutdown( void );
extern void     Maps_InitArena( void );
extern void     Maps_ShutdownArena( void );

// =====================================================================
// Sys_Error — tool-side override
// =====================================================================
//
// The engine's Com_Terminate cascades to Sys_Error on fatal levels and
// recursive-error escalations. The engine binaries link Sys_Error from
// win_main.c / unix_main.c (which longjmp into the engine's abortframe
// or pop a Win32 error dialog). The tool has no abortframe and wants no
// dialog — print to stderr, exit 3.
NORETURN void QDECL Sys_Error( const char *fmt, ... ) {
	va_list ap;
	fputs( "extract-meta: fatal: ", stderr );
	va_start( ap, fmt );
	vfprintf( stderr, fmt, ap );
	va_end( ap );
	fputc( '\n', stderr );
	exit( 3 );
}

// =====================================================================
// Platform stubs — Windows
// =====================================================================
//
// The engine's Sys_* surface is large; we only stub functions that
// qcommon_static references at link time. Surface determined
// empirically by linker errors during B1 implementation.

#ifdef _WIN32

void Sys_SetMainThreadPolicy( void ) {
	// Engine sets thread priority / scheduler hints here. Tool runs on
	// whatever the OS assigned; no-op.
}

qboolean Sys_LowPhysicalMemory( void ) {
	return qfalse;
}

void Sys_BeginProfiling( void ) { /* no-op */ }

const char *Sys_DefaultBasePath( void ) {
	static char buf[MAX_OSPATH];
	if ( !buf[0] ) {
		DWORD n = GetCurrentDirectoryA( sizeof( buf ), buf );
		if ( n == 0 || n >= sizeof( buf ) ) {
			Q_strncpyz( buf, ".", sizeof( buf ) );
		}
	}
	return buf;
}

const char *Sys_DefaultHomePath( void ) {
	static char buf[MAX_OSPATH];
	if ( !buf[0] ) {
		// Match the engine's win_shared.c layout exactly:
		// %USERPROFILE%\wired\<PRODUCT_NAME><CHANNEL_SUFFIX>\
		// so the tool sees the same homepath packs the engine sees
		// (e.g. pax01.sw3z dropped there by the launcher).
		const char *up = getenv( "USERPROFILE" );
		if ( up && *up ) {
			Com_sprintf( buf, sizeof( buf ), "%s\\wired\\%s%s",
			             up, PRODUCT_NAME, CHANNEL_SUFFIX );
		} else {
			Q_strncpyz( buf, ".", sizeof( buf ) );
		}
	}
	return buf;
}

const char *Sys_Pwd( void ) {
	return Sys_DefaultBasePath();
}

void Sys_Print( const char *msg ) {
	if ( msg ) fputs( msg, stderr );
}

void Sys_Sleep( int msec ) {
	if ( msec > 0 ) Sleep( (DWORD)msec );
}

qboolean Sys_RandomBytes( byte *string, int len ) {
	// Tool doesn't need cryptographic-quality randomness; the
	// FS_Restart caller passes 0 for checksumFeed in our flow, so this
	// is mostly unreached. Fixed-seed rand() keeps tool runs
	// deterministic, which matches acceptance criterion 10.
	static int seeded = 0;
	if ( !seeded ) { srand( 1 ); seeded = 1; }
	for ( int i = 0; i < len; i++ ) string[i] = (byte)( rand() & 0xff );
	return qtrue;
}

qboolean Sys_SetAffinityMask( const uint64_t mask ) {
	(void)mask;
	return qfalse;
}

// ── Mutex primitives ──────────────────────────────────────────────────
//
// Engine's sys_mutex_t is opaque; on Windows it's typically a CRITICAL_SECTION.
// We emulate with the same shape so qcommon code that allocates one as
// `sys_mutex_t m;` on the stack works correctly. The actual layout is
// defined in code/win32/win_main.c — to avoid coupling, allocate a real
// CRITICAL_SECTION on the heap and store its pointer in the first
// pointer-sized slot of the caller's struct. Engine assumes the struct
// is at least pointer-sized.

qboolean Sys_MutexInit( sys_mutex_t *m ) {
	if ( !m ) return qfalse;
	CRITICAL_SECTION *cs = (CRITICAL_SECTION *)malloc( sizeof( *cs ) );
	if ( !cs ) return qfalse;
	InitializeCriticalSection( cs );
	*(void **)m = cs;
	return qtrue;
}

void Sys_MutexLock( sys_mutex_t *m ) {
	if ( !m ) return;
	CRITICAL_SECTION *cs = *(CRITICAL_SECTION **)m;
	if ( cs ) EnterCriticalSection( cs );
}

void Sys_MutexUnlock( sys_mutex_t *m ) {
	if ( !m ) return;
	CRITICAL_SECTION *cs = *(CRITICAL_SECTION **)m;
	if ( cs ) LeaveCriticalSection( cs );
}

void Sys_MutexDestroy( sys_mutex_t *m ) {
	if ( !m ) return;
	CRITICAL_SECTION *cs = *(CRITICAL_SECTION **)m;
	if ( cs ) {
		DeleteCriticalSection( cs );
		free( cs );
	}
	*(void **)m = NULL;
}

// ── File / directory primitives ───────────────────────────────────────

qboolean Sys_Mkdir( const char *path ) {
	if ( !path || !*path ) return qfalse;
	if ( CreateDirectoryA( path, NULL ) ) return qtrue;
	return ( GetLastError() == ERROR_ALREADY_EXISTS ) ? qtrue : qfalse;
}

FILE *Sys_FOpen( const char *ospath, const char *mode ) {
	return fopen( ospath, mode );
}

qboolean Sys_ResetReadOnlyAttribute( const char *ospath ) {
	(void)ospath;
	return qfalse;   // tool doesn't touch read-only files
}

qboolean Sys_GetFileStats( const char *filename, fileOffset_t *size, fileTime_t *mtime, fileTime_t *ctime ) {
	struct _stat st;
	if ( _stat( filename, &st ) != 0 ) return qfalse;
	if ( size  ) *size  = (fileOffset_t)st.st_size;
	if ( mtime ) *mtime = (fileTime_t)st.st_mtime;
	if ( ctime ) *ctime = (fileTime_t)st.st_ctime;
	return qtrue;
}

void Sys_FreeFileList( char **list ) {
	if ( !list ) return;
	for ( char **p = list; *p; p++ ) free( *p );
	free( list );
}

// ── Library loading — tool doesn't dlopen anything ────────────────────

void *Sys_LoadLibrary( const char *name ) { (void)name; return NULL; }
void *Sys_LoadFunction( void *handle, const char *name ) { (void)handle; (void)name; return NULL; }
int   Sys_LoadFunctionErrors( void ) { return 0; }
void  Sys_UnloadLibrary( void *handle ) { (void)handle; }

// ── Other Sys_* the engine registers / expects ────────────────────────

void Sys_SendKeyEvents( void ) { /* tool has no input loop */ }

void Sys_Init( void ) {
	// Engine version registers cvars (sys_arch, sys_cpustring, etc.).
	// Tool doesn't care; no-op.
}

void Sys_InstallCrashHandler( void ) { /* no-op */ }

#endif // _WIN32

// =====================================================================
// Tool_Init / Tool_Shutdown
// =====================================================================
//
// Bootstrap order mirrors common.c::Com_Init for the subsystems the
// tool actually needs (zone, cvar, cbuf, cmd, FS), skipping
// everything client / server / VM / netstack / scripting / event-bus
// brings in. Subsystems excluded entirely from qcommon_tool:
//
//   - events/event.c              (Com_InitPushEvent stubbed)
//   - logging/log_sink_console.c  (CL_ConsolePrint dependency)
//   - core/core.c                 (Wired{Core,Script}_* stubs)
//   - msg.c, net_ip.c             (cl_shownet, CL_ networking)
//   - cm_*.c                      (BotDrawDebugPolygons; tool only
//                                  enumerates filenames, never
//                                  loads collision geometry)
//   - vm.c, vm_wasm.c             (VM dispatch + WAMR SDK)
//   - wired/net/*                 (picoquic / mpack / WN)
//
// argv is intentionally unused: the tool doesn't accept +set fs_*
// overrides this round. Install path comes from Sys_DefaultBasePath
// (above; CWD-based) and Sys_DefaultHomePath (%APPDATA%\wired). To
// override either, set the WIRED_FS_BASEPATH / WIRED_FS_HOMEPATH env
// vars before launch — engine-side support is independent of argv.

void Tool_Init( int argc, char **argv ) {
	(void)argc; (void)argv;

	// Step 1 — fallback stderr log sink. Internally calls Sys_MutexInit;
	// no Z_Malloc on the success path.
	if ( !Log_RegisterFallbackStderrSink() ) {
		fprintf( stderr, "extract-meta: Log_RegisterFallbackStderrSink failed\n" );
		exit( 3 );
	}

	// Step 2 — small zone allocator (must precede the first Z_Malloc).
	Com_InitSmallZoneMemory();

	// Step 3 — cvar system. First cvar registrations Z_Malloc into smallzone.
	Cvar_Init();

	// Step 3a — suppress DebugBreak in Com_Terminate. The engine's
	// log.c::Com_Terminate dereferences `com_noErrorInterrupt->integer`
	// in debug builds; that cvar pointer (a global in common.c) is
	// normally bound by Com_Init at Cvar_Register time, but the tool
	// skips Com_Init. If FS_InitFilesystem fails (no pak0), the
	// resulting Com_Terminate then crashes on a NULL-pointer
	// dereference before our Sys_Error override can call exit(3).
	//
	// Bind the global directly so its `->integer` is non-NULL and
	// non-zero. Setting to "1" routes Com_Terminate around the
	// DebugBreak path entirely.
	{
		extern cvar_t *com_noErrorInterrupt;
		com_noErrorInterrupt = Cvar_Get( "com_noErrorInterrupt", "1", CVAR_INIT );
	}

	// Step 4 — main zone (multi-MB block via malloc; gated by com_zoneMegs).
	Com_InitZoneMemory();

	// Step 5 — cbuf for queued commands.
	Cbuf_Init();

	// Step 6 — cmd registration.
	Cmd_Init();

	// Step 7 — VFS bring-up. Set fs_skipExecDefaults qtrue first so
	// FS_Restart's "Couldn't load default.cfg" fatal check is bypassed.
	// Tool only needs the FS to enumerate files / parse BSPs; engine
	// config scripts are not relevant here.
	fs_skipExecDefaults = qtrue;
	FS_InitFilesystem();

	// Step 7a — BSP format registry + meta arenas. Engine does this in
	// Com_Init alongside FS_InitFilesystem; tool needs them explicitly
	// because BSP_Load fails with "no matching format" until BSP_Init
	// has registered Q3 + Q1 formats.
	BSP_Init();
	Maps_InitArena();

	// Step 8 — tool-side arena (4 MB headroom; shader_index for a
	// full q3 install is ~600 KB peak in current measurements). Done
	// last so the arena is guaranteed available to every B2 module
	// that runs after Tool_Init returns.
	Tool_Arena = Arena_Create( "Tool", 4u << 20 );
	if ( !Tool_Arena ) {
		fprintf( stderr, "extract-meta: Arena_Create(\"Tool\", 4MB) failed\n" );
		exit( 3 );
	}
}

void Tool_Shutdown( void ) {
	if ( Tool_Arena ) {
		Arena_Destroy( Tool_Arena );
		Tool_Arena = NULL;
	}
	Maps_ShutdownArena();
	BSP_Shutdown();
	FS_Shutdown( qfalse );
	Log_UnregisterFallbackStderrSink();
}
