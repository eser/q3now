// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 1999-2005 Id Software, Inc.
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
#include <signal.h>
#include <unistd.h>
#include <execinfo.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <time.h>

#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"
#ifndef DEDICATED
#include "../renderer/tr_local.h"
#endif

static qboolean signalcaught = qfalse;

extern void NORETURN Sys_Exit( int code );

/* Write a crash log to /tmp/wired_crash.txt.
 * Uses only async-signal-safe calls for the fd/write path; backtrace_symbols
 * technically isn't safe but is acceptable here since we're already crashing. */
static void WriteCrashLog( int sig )
{
	static char  path[] = "/tmp/wired_crash.txt";
	void        *syms[64];
	int          size;
	int          fd;
	char         header[256];
	time_t       now = time( NULL );
	struct tm   *tm  = localtime( &now );
	char         timestr[64];
	char       **strings;
	int          i;

	size = backtrace( syms, ARRAY_LEN( syms ) );

	fd = open( path, O_WRONLY | O_CREAT | O_TRUNC, 0644 );
	if ( fd < 0 )
		return;

	strftime( timestr, sizeof( timestr ), "%Y-%m-%d %H:%M:%S", tm );
	snprintf( header, sizeof( header ),
	          "=== Wired crash log ===\nTime:   %s\nSignal: %d (%s)\nFrames: %d\n\n",
	          timestr, sig,
	          sig == SIGSEGV ? "SIGSEGV (null/bad pointer)" :
	          sig == SIGBUS  ? "SIGBUS (bus error)"         :
	          sig == SIGILL  ? "SIGILL (illegal instruction)": "other",
	          size );
	write( fd, header, strlen( header ) );

	/* backtrace_symbols allocates — unsafe in strict signal context, but gives
	 * readable names which are far more useful than raw addresses for diagnosis. */
	strings = backtrace_symbols( syms, size );
	if ( strings ) {
		for ( i = 0; i < size; i++ ) {
			write( fd, strings[i], strlen( strings[i] ) );
			write( fd, "\n", 1 );
		}
		free( (void *)strings );
	} else {
		/* fallback: raw addresses to fd (always async-signal-safe) */
		backtrace_symbols_fd( syms, size, fd );
	}

	close( fd );

	/* Also dump to stderr so it shows up in the terminal */
	fprintf( stderr, "\n=== CRASH BACKTRACE (signal %d) ===\n", sig );
	backtrace_symbols_fd( syms, size, STDERR_FILENO );
	fprintf( stderr, "=== crash log written to %s ===\n", path );
}

static void signal_handler( int sig )
{
	char msg[32];

	if ( signalcaught == qtrue )
	{
		printf( "DOUBLE SIGNAL FAULT: Received signal %d, exiting...\n", sig );
		_exit( 1 );
	}

	signalcaught = qtrue;
	printf( "Received signal %d, exiting...\n", sig );

	WriteCrashLog( sig );

	sprintf( msg, "Signal caught (%d)", sig );
	VM_Forced_Unload_Start();
#ifndef DEDICATED
	CL_Shutdown( msg, qtrue );
#endif
	SV_Shutdown( msg );
	VM_Forced_Unload_Done();
	Sys_Exit( 0 ); // send a 0 to avoid DOUBLE SIGNAL FAULT
}


void InitSig( void )
{
	signal( SIGPIPE, SIG_IGN );   // ignore broken pipe — don't die on dropped socket/terminal
	signal( SIGINT, SIG_IGN );
	signal( SIGHUP, signal_handler );
	signal( SIGQUIT, signal_handler );
	signal( SIGILL, signal_handler );
	signal( SIGTRAP, signal_handler );
	signal( SIGIOT, signal_handler );
	signal( SIGBUS, signal_handler );
	signal( SIGFPE, signal_handler );
	signal( SIGSEGV, signal_handler );
	signal( SIGTERM, signal_handler );
}
