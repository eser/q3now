// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
//
// Engine-symbol stubs for the command-layer gtest suite.
//
// cmd.c is self-contained in behaviour but not in linkage: it calls into the
// console, cvar, filesystem and client/server dispatch layers. Linking the real
// ones would drag in most of the engine, and the tokenizer and the registration
// table — the things under test — do not depend on any of them.
//
// This is a deliberate departure from the other contract tests here, which are
// pure seams needing no stubs at all (cvar_value.c, fs_qpath.c). Stubbing is the
// price of testing a subsystem that was never carved into a seam; the honest
// alternative would be to extract one, which is a larger change than adding
// coverage. Recorded so the next reader knows this file is a bridge, not a
// pattern to copy by default.
//
// Every stub is inert. None of them may be relied on to observe behaviour: a
// test that needs to see a side effect through one of these is testing the
// wrong layer, and should exercise the real subsystem instead.

#include "q_shared.h"
#include "qcommon.h"

// ── logging / fatal ─────────────────────────────────────────────────────────
// Terminate is the one stub that MUST NOT return: callers treat it as noreturn
// and the code after a Com_Terminate call is unreachable by contract. Aborting
// keeps that true, so a test that trips a fatal path fails loudly instead of
// walking into undefined behaviour.
void NORETURN QDECL Com_Terminate( terminationReason_t level, const char *fmt, ... ) {
	(void) level; (void) fmt;
	abort();
}

void Com_Log_Impl( log_severity_t sev, int channel, const char *fmt, ... ) {
	(void) sev; (void) channel; (void) fmt;
}

int Log_GetChannel( const char *name ) {
	(void) name;
	return 0;
}

// ── cvar layer ──────────────────────────────────────────────────────────────
// Returning qfalse means "no cvar consumed this command", which keeps command
// dispatch on the command path — exactly what the dispatch tests want to see.
qboolean Cvar_Command( void ) {
	return qfalse;
}

void Cvar_CompleteCvarName( const char *args, int argNum ) {
	(void) args; (void) argNum;
}

const char *Cvar_VariableString( const char *var_name ) {
	(void) var_name;
	return "";
}

void Com_WriteConfiguration( void ) {
}

// ── filesystem ──────────────────────────────────────────────────────────────
// ReadFile reporting -1 with a NULL buffer is the documented "not found" shape,
// so exec-style commands take their missing-file path rather than reading
// uninitialised memory.
int FS_ReadFile( const char *qpath, void **buffer ) {
	(void) qpath;
	if ( buffer ) {
		*buffer = NULL;
	}
	return -1;
}

void FS_FreeFile( void *buffer ) {
	(void) buffer;
}

void FS_BypassPure( void ) {
}

void FS_RestorePure( void ) {
}

void Field_CompleteFilename( const char *dir, const char *ext,
		qboolean stripExt, int flags ) {
	(void) dir; (void) ext; (void) stripExt; (void) flags;
}

// ── client / server dispatch ────────────────────────────────────────────────
// qfalse = "not handled here", so a command the test registered stays with the
// local command table instead of being claimed by a game module.
qboolean CL_GameCommand( void ) {
	return qfalse;
}

static qboolean s_serverSpawnIdle = qtrue;
static qboolean s_serverGameHandled;
static int s_serverGameCalls;

qboolean SV_GameCommand( void ) {
	s_serverGameCalls++;
	return s_serverGameHandled;
}

qboolean SV_IsSpawnIdle( void ) {
	return s_serverSpawnIdle;
}

void CmdTest_SetServerDispatch( qboolean running, qboolean spawnIdle,
		qboolean handled ) {
	com_sv_running->integer = running;
	s_serverSpawnIdle = spawnIdle;
	s_serverGameHandled = handled;
	s_serverGameCalls = 0;
}

int CmdTest_ServerGameCalls( void ) {
	return s_serverGameCalls;
}

void CL_ForwardCommandToServer( const char *string ) {
	(void) string;
}

// ── memory ──────────────────────────────────────────────────────────────────
// The zone allocator is replaced with plain malloc/free. cmd.c only needs
// allocation to behave like allocation; zone bookkeeping (tags, hunk marks) is
// not observable through any command-layer contract.
//
// The zone exposes TWO MUTUALLY EXCLUSIVE surfaces (qcommon.h, ZONE_DEBUG): a
// debug build turns Z_Malloc/S_Malloc/Z_TagMalloc into macros over the
// *Debug entry points, while a release build declares them as plain functions
// and the *Debug names do not exist at all. A stub that provides only one
// surface therefore links in exactly one configuration and fails in the other
// — which is what happened here: the debug-only stubs left cmd_gtest with an
// undefined _S_Malloc in Release. Mirror the same #if so whichever surface
// cmd.c was compiled against is the one that gets defined.
static void *Stub_Alloc( size_t size ) {
	void *p = calloc( 1, size );
	if ( !p ) {
		abort();
	}
	return p;
}

#ifdef ZONE_DEBUG
void *Z_MallocDebug( size_t size, const char *label, const char *file, int line ) {
	(void) label; (void) file; (void) line;
	return Stub_Alloc( size );
}

void *S_MallocDebug( size_t size, const char *label, const char *file, int line ) {
	(void) label; (void) file; (void) line;
	return Stub_Alloc( size );
}

void *Z_TagMallocDebug( size_t size, memtag_t tag, const char *label, const char *file, int line ) {
	(void) tag; (void) label; (void) file; (void) line;
	return Stub_Alloc( size );
}
#else
void *Z_Malloc( size_t size ) {
	return Stub_Alloc( size );
}

void *S_Malloc( size_t size ) {
	return Stub_Alloc( size );
}

void *Z_TagMalloc( size_t size, memtag_t tag ) {
	(void) tag;
	return Stub_Alloc( size );
}
#endif

void Z_Free( void *ptr ) {
	free( ptr );
}

char *CopyString( const char *in ) {
	size_t len = strlen( in ) + 1;
	char *out = (char *) malloc( len );
	if ( !out ) {
		abort();
	}
	memcpy( out, in, len );
	return out;
}

// ── misc ────────────────────────────────────────────────────────────────────
int Sys_Milliseconds( void ) {
	return 0;
}

// cmd.c consults these to decide whether an unrecognised command should be
// forwarded to a running client or server. Both are zero-valued here, i.e.
// "nothing is running", which keeps dispatch entirely local — the condition the
// registration tests need in order to observe their own handlers being called
// rather than the line being forwarded away.
static cvar_t s_notRunning;
cvar_t *com_cl_running = &s_notRunning;
cvar_t *com_sv_running = &s_notRunning;

// Com_Filter is deliberately NOT stubbed: its real implementation lives in
// code/qcommon/util/string.c, which this target already compiles for
// Com_sprintf. Using the real wildcard matcher is strictly better than a stub —
// a stub would define behaviour the engine does not have.
