// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
//
// crash_sentry.c — sentry-native crash capture adapter.
//
// One capture path for Windows, macOS and Linux, replacing three hand-rolled
// per-platform handlers that agreed on nothing: POSIX wrote a text backtrace,
// Windows wrote JSON plus a minidump, and only Windows carried module
// information. See CMakeLists.txt (USE_SENTRY_CRASH) for why this backend.
//
// LOCAL-ONLY BY CONSTRUCTION. The library is built with SENTRY_TRANSPORT=none,
// so no HTTP client is linked at all — there is nothing here that can upload,
// and no DSN is set. Crash artefacts stay in the user's own directory until the
// user chooses to share them. This is not a runtime setting that could be
// flipped by a stray cvar; the network code is absent from the binary.
//
// Compiled only when FEAT_SENTRY_CRASH is 1; otherwise the whole file is empty
// and the previous per-platform handlers remain in charge.

#include "q_shared.h"
#include "qcommon.h"
#include "crash.h"
#include "wired/wired_build_stamp.h"

#if FEAT_SENTRY_CRASH

#include <sentry.h>

LOG_DECLARE_CHANNEL( ch_crash_sentry, "crash" );

static qboolean s_sentryStarted;

/*
==================
Crash_SentryDatabasePath

Where the local crash database lives.

Under fs_homepath, the same root as qconsole.jsonl, wired_playtest.jsonl and
crash_*.json — one place a user can be asked to look, and one place that is
writable on every platform. fs_installpath is wrong here for the reason
crash.c already documents: a macOS .app interior is read-only for system
installs.
==================
*/
static const char *Crash_SentryDatabasePath( void )
{
	static char path[ MAX_OSPATH * 2 ];
	const char *homepath = Cvar_VariableString( "fs_homepath" );

	if ( homepath == NULL || homepath[ 0 ] == '\0' ) {
		return NULL;
	}
	Com_sprintf( path, sizeof( path ), "%s%c%s", homepath, PATH_SEP, "crashdb" );
	return path;
}

/*
==================
Crash_SentryHandlerPath

The out-of-process handler executable.

Crash capture happens in a SEPARATE PROCESS: the faulting process only signals,
and the handler — running outside the corrupted address space — does the work.
That is what makes "must not deadlock or recurse into allocation in the fault
handler" structural rather than a discipline to maintain by hand.

The consequence is that the handler binary must ship beside the engine and be
found at runtime. It sits next to the executable, which is also where the
platform packaging already places auxiliary binaries.
==================
*/
#ifdef _WIN32
#	define CRASH_SENTRY_HANDLER_EXE "sentry-crash.exe"
#else
#	define CRASH_SENTRY_HANDLER_EXE "sentry-crash"
#endif

static const char *Crash_SentryHandlerPath( void )
{
	static char path[ MAX_OSPATH * 2 ];
	const char *bindir = FS_GetInstallBinaryPath();

	if ( bindir == NULL || bindir[ 0 ] == '\0' ) {
		return NULL;
	}
	Com_sprintf( path, sizeof( path ), "%s%c%s",
		bindir, PATH_SEP, CRASH_SENTRY_HANDLER_EXE );
	return path;
}

/*
==================
Crash_SentryHandlerPresent

Existence check for an ABSOLUTE HOST path.

Sys_FOpen, not FS_FileExists: the latter searches the VFS (paks and the search
path), and the handler executable lives in the install binary directory, which
the VFS does not index. FS_FileExists would have answered false every time and
silently kept the old handler — a wrong answer that looks exactly like a correct
"not installed" one.
==================
*/
static qboolean Crash_SentryHandlerPresent( const char *ospath )
{
	FILE *f = Sys_FOpen( ospath, "rb" );

	if ( f == NULL ) {
		return qfalse;
	}
	fclose( f );
	return qtrue;
}

/*
==================
Crash_MinidumpDatabasePath

The directory a minidump for this session would be written to, or "" when no
out-of-process backend is active.

Deliberately reports only when sentry is RUNNING, not merely compiled in: a path
in the crash report is a promise that something may be there, and pointing at an
empty directory sends the reader looking for a file that was never going to
exist.
==================
*/
const char *Crash_MinidumpDatabasePath( void )
{
	const char *path;

	if ( !s_sentryStarted ) {
		return "";
	}
	path = Crash_SentryDatabasePath();
	return path ? path : "";
}

/*
==================
Crash_SentryAnnotate

Attach the engine state a minidump cannot carry.

A minidump holds threads, registers and loaded modules — the machine's view. It
says nothing about which map was loaded, which renderer was active, or whether
this was a client or a server, and those are usually the first questions asked
of a crash report.

The cvar set is crash_cvarAllowlist (crash.c) — the SAME table the JSON report
uses, read here rather than copied. That is what keeps one privacy contract
instead of two: adding a cvar to the table discloses it in both artefacts, and
removing it withdraws it from both.

Safe to call repeatedly; each call overwrites the previous values, so refreshing
after a map change simply replaces the snapshot.
==================
*/
void Crash_SentryAnnotate( void )
{
	int i;

	if ( !s_sentryStarted ) {
		return;
	}

	/* Written as TAGS, not as a context, and that is not a style choice.
	 *
	 * Measured against this vendored version: the native backend's scope flush
	 * (src/backends/sentry_backend_native.c, native_backend_flush_scope) copies
	 * user, tags and extra onto the crash event, but of the contexts it copies
	 * only "os" and "device" — a custom context is silently dropped. A
	 * sentry_set_context("wired.cvars", …) therefore looks correct, compiles,
	 * runs, and produces a dump with none of it attached. Tags survive, so the
	 * allowlist goes through tags.
	 *
	 * Revisit when the native backend matures ("experimental and under active
	 * development" is its own startup warning): a context reads better than a
	 * flat tag namespace, and this is the only reason it is not one. */
	for ( i = 0; crash_cvarAllowlist[i] != NULL; i++ ) {
		const char *name  = crash_cvarAllowlist[i];
		const char *value = Cvar_VariableString( name );
		char key[ 64 ];

		Com_sprintf( key, sizeof( key ), "wired.%s", name );
		sentry_set_tag( key, value ? value : "" );
	}

	/* Build identity — the same values the JSON crash report and
	   wired_playtest.jsonl carry, so a dump, a report and a breadcrumb trail
	   can be matched to each other and to a commit. */
	sentry_set_tag( "wired.source_revision", WIRED_SOURCE_REVISION );
	sentry_set_tag( "wired.build_stamp",     WIRED_BUILD_DATE );
}

/*
==================
Crash_SentryInstall

Returns qtrue if sentry took ownership of crash capture.

Deliberately fail-soft: every early return leaves s_sentryStarted false, and the
caller then keeps the existing per-platform handler. A missing handler binary or
an unwritable home must degrade to the old behaviour, never to NO crash capture
— losing evidence is the one outcome this subsystem exists to prevent.
==================
*/
qboolean Crash_SentryInstall( void )
{
	sentry_options_t *options;
	const char *dbPath;
	const char *handlerPath;

	if ( s_sentryStarted ) {
		return qtrue;
	}

	dbPath = Crash_SentryDatabasePath();
	if ( dbPath == NULL ) {
		Com_Log( SEV_WARN, LOG_CH(ch_crash_sentry),
			"sentry: fs_homepath unavailable, keeping the platform handler\n" );
		return qfalse;
	}

	handlerPath = Crash_SentryHandlerPath();
	if ( handlerPath == NULL || !Crash_SentryHandlerPresent( handlerPath ) ) {
		Com_Log( SEV_WARN, LOG_CH(ch_crash_sentry),
			"sentry: handler '%s' not found, keeping the platform handler\n",
			handlerPath ? handlerPath : "(unknown)" );
		return qfalse;
	}

	options = sentry_options_new();
	if ( options == NULL ) {
		return qfalse;
	}

	/* No DSN: nothing is transmitted. With SENTRY_TRANSPORT=none there is no
	   transport compiled in to transmit WITH, so this is belt and braces. */
	sentry_options_set_database_path( options, dbPath );
	sentry_options_set_handler_path( options, handlerPath );

	/* Dump scope, pinned rather than left at the default.
	 *
	 * sentry defaults to SENTRY_MINIDUMP_MODE_SMART (stack + surrounding heap),
	 * which it documents as ~5-10 MB. That estimate holds where sentry writes
	 * the dump itself, but NOT on Windows: there the writer is a thin wrapper
	 * over the OS MiniDumpWriteDump, and the same SMART mode measured 49.8 MB —
	 * ~340x the macOS dump from an identical setting. A crash artefact is
	 * something an alpha tester has to hand over, so a scope that swings by two
	 * orders of magnitude between platforms is not one to inherit silently.
	 *
	 * STACK_ONLY is what sentry recommends for production and is documented at
	 * ~100KB-1MB. It carries the thread and module data a minidump is actually
	 * read for; what it drops is heap around the crash site, which the JSON
	 * report's engine context covers better anyway (map, renderer and VM state
	 * are named there, not reconstructed from memory).
	 *
	 * Pinning it also makes the three platforms produce comparable artefacts,
	 * which is the whole point of having one crash pipeline rather than three. */
	sentry_options_set_minidump_mode( options, SENTRY_MINIDUMP_MODE_STACK_ONLY );

	/* release identifies the build, in the same terms the crash report and
	   wired_playtest.jsonl already use, so all three name the same binary. */
	sentry_options_set_release( options, WIRED_SOURCE_REVISION );
#ifdef _DEBUG
	sentry_options_set_environment( options, "debug" );
#else
	sentry_options_set_environment( options, "release" );
#endif

	if ( sentry_init( options ) != 0 ) {
		Com_Log( SEV_WARN, LOG_CH(ch_crash_sentry),
			"sentry: init failed, keeping the platform handler\n" );
		return qfalse;
	}

	/* Tags are the searchable axes of a crash database. Kept to build identity
	   and coarse platform facts: no paths, no user or machine names — the same
	   allowlist rule the JSON report's cvar block follows. */
	sentry_set_tag( "engine.build_id",  WIRED_BUILD_ID_STR );
	sentry_set_tag( "engine.platform",  PLATFORM_STRING );
	sentry_set_tag( "engine.arch",      ARCH_STRING );

	s_sentryStarted = qtrue;
	Crash_SentryAnnotate();
	Com_Log( SEV_INFO, LOG_CH(ch_crash_sentry),
		"sentry: out-of-process crash capture active (local-only, db '%s')\n", dbPath );
	return qtrue;
}

/*
==================
Crash_SentryShutdown

Flushes and closes the local database on an orderly exit. A crash does NOT come
through here — that is the handler process's job, which is the point.
==================
*/
void Crash_SentryShutdown( void )
{
	if ( !s_sentryStarted ) {
		return;
	}
	sentry_close();
	s_sentryStarted = qfalse;
}

#else /* !FEAT_SENTRY_CRASH */

qboolean Crash_SentryInstall( void ) { return qfalse; }
void     Crash_SentryShutdown( void ) { }
void     Crash_SentryAnnotate( void ) { }
const char *Crash_MinidumpDatabasePath( void ) { return ""; }

#endif
