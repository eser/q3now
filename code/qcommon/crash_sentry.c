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

#endif
