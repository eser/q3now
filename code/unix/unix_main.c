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
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <limits.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <string.h>
#include <ctype.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <errno.h>
#include <libgen.h> // dirname
#include <pthread.h>

#include <dlfcn.h>

#ifdef __linux__
#ifdef __GLIBC__
  #include <fpu_control.h> // bk001213 - force dumps on divide by zero
#endif
#endif

#if defined(__sun)
  #include <sys/file.h>
#endif

// FIXME TTimo should we gard this? most *nix system should comply?
#include <termios.h>

#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"
#include "../qcommon/crash.h"
#include "../renderercommon/tr_public.h"

#include "linux_local.h" // bk001204

#include <execinfo.h>
/* Phase 5: log channels */
LOG_DECLARE_CHANNEL( ch_system, "system" );

#ifndef DEDICATED
#include "../client/client.h"
#endif

unsigned sys_frame_time;

qboolean stdin_active = qfalse;
int      stdin_flags = 0;

// =============================================================
// tty console variables
// =============================================================

typedef enum {
	TTY_ENABLED,
	TTY_DISABLED,
	TTY_ERROR
} tty_err;

// enable/disabled tty input mode
// NOTE TTimo this is used during startup, cannot be changed during run
static cvar_t *ttycon = NULL;

// general flag to tell about tty console mode
static qboolean ttycon_on = qfalse;

// when printing general stuff to stdout stderr (Sys_Printf)
//   we need to disable the tty console stuff
// this increments so we can recursively disable
static int ttycon_hide = 0;

// some key codes that the terminal may be using
// TTimo NOTE: I'm not sure how relevant this is
static int tty_erase;
static int tty_eof;

static struct termios tty_tc;

static field_t tty_con;

static cvar_t *ttycon_ansicolor = NULL;
static qboolean ttycon_color_on = qfalse;

tty_err Sys_ConsoleInputInit( void );

// =======================================================================
// General routines
// =======================================================================

// bk001207
#define MEM_THRESHOLD 96*1024*1024

/*
==================
Sys_LowPhysicalMemory()
==================
*/
qboolean Sys_LowPhysicalMemory( void )
{
	//MEMORYSTATUS stat;
	//GlobalMemoryStatus (&stat);
	//return (stat.dwTotalPhys <= MEM_THRESHOLD) ? qtrue : qfalse;
	return qfalse; // bk001207 - FIXME
}


void Sys_BeginProfiling( void )
{

}


// =============================================================
// tty console routines
// NOTE: if the user is editing a line when something gets printed to the early console then it won't look good
//   so we provide tty_Clear and tty_Show to be called before and after a stdout or stderr output
// =============================================================

// flush stdin, I suspect some terminals are sending a LOT of shit
// FIXME TTimo relevant?
static void tty_FlushIn( void )
{
#if 1
	tcflush( STDIN_FILENO, TCIFLUSH );
#else
	char key;
	while ( read( STDIN_FILENO, &key, 1 ) > 0 );
#endif
}


// do a backspace
// TTimo NOTE: it seems on some terminals just sending '\b' is not enough
//   so for now, in any case we send "\b \b" .. yeah well ..
//   (there may be a way to find out if '\b' alone would work though)
static void tty_Back( void )
{
	write( STDOUT_FILENO, "\b \b", 3 );
}


// clear the display of the line currently edited
// bring cursor back to beginning of line
void tty_Hide( void )
{
	int i;

	if ( !ttycon_on )
		return;

	if ( ttycon_hide )
	{
		ttycon_hide++;
		return;
	}

	if ( tty_con.cursor > 0 )
	{
		for ( i = 0; i < tty_con.cursor; i++ )
		{
			tty_Back();
		}
	}
	tty_Back(); // delete "]" ? -EC-
	ttycon_hide++;
}


// show the current line
// FIXME TTimo need to position the cursor if needed??
void tty_Show( void )
{
	if ( !ttycon_on )
		return;

	if ( ttycon_hide > 0 )
	{
		ttycon_hide--;
		if ( ttycon_hide == 0 )
		{
			write( STDOUT_FILENO, "]", 1 ); // -EC-

			if ( tty_con.cursor > 0 )
			{
				write( STDOUT_FILENO, tty_con.buffer, tty_con.cursor );
			}
		}
	}
}


// never exit without calling this, or your terminal will be left in a pretty bad state
void Sys_ConsoleInputShutdown( void )
{
	if ( ttycon_on )
	{
//		Com_Log( SEV_INFO, LOG_CH(ch_system), "Shutdown tty console\n" ); // -EC-
		tty_Back(); // delete "]" ? -EC-
		tcsetattr( STDIN_FILENO, TCSADRAIN, &tty_tc );
	}

	// Restore blocking to stdin reads
	if ( stdin_active )
	{
		fcntl( STDIN_FILENO, F_SETFL, stdin_flags );
//		fcntl( STDIN_FILENO, F_SETFL, fcntl( STDIN_FILENO, F_GETFL, 0 ) & ~O_NONBLOCK );
	}

	memset( &tty_con, 0, sizeof( tty_con ) );

	stdin_active = qfalse;
	ttycon_on = qfalse;

	ttycon_hide = 0;
}

/*
==================
CON_SigCont
Reinitialize console input after receiving SIGCONT, as on Linux the terminal seems to lose all
set attributes if user did CTRL+Z and then does fg again.
==================
*/
void CON_SigCont( int signum )
{
	Sys_ConsoleInputInit();
}


void CON_SigTStp( int signum )
{
	sigset_t mask;

	tty_FlushIn();
	Sys_ConsoleInputShutdown();

	sigemptyset( &mask );
	sigaddset( &mask, SIGTSTP );
	sigprocmask( SIG_UNBLOCK, &mask, NULL );

	signal( SIGTSTP, SIG_DFL );

	kill( getpid(),  SIGTSTP );
}


// =============================================================
// general sys routines
// =============================================================

// single exit point (regular exit or in case of signal fault)
void NORETURN Sys_Exit( int code )
{
	Sys_ConsoleInputShutdown();

#ifdef NDEBUG // regular behavior
	// We can't do this
	//  as long as GL DLL's keep installing with atexit...
	//exit(ex);
	_exit( code );
#else
	// Give me a backtrace on error exits.
	assert( code == 0 );
	exit( code );
#endif
}


void NORETURN Sys_Quit( void )
{
#ifndef DEDICATED
	CL_Shutdown( "", qtrue );
#endif

	Sys_Exit( 0 );
}


void Sys_Init( void )
{
	Cvar_Set( "arch", OS_STRING " " ARCH_STRING );
	//IN_Init();   // rcg08312005 moved into glimp.
}


void NORETURN FORMAT_PRINTF(1, 2) QDECL Sys_Error( const char *format, ... )
{
	va_list argptr;
	char text[1024];

	// change stdin to non blocking
	// NOTE TTimo not sure how well that goes with tty console mode
	if ( stdin_active )
	{
//		fcntl( STDIN_FILENO, F_SETFL, fcntl( STDIN_FILENO, F_GETFL, 0) & ~FNDELAY );
		fcntl( STDIN_FILENO, F_SETFL, stdin_flags );
	}

	// don't bother do a show on this one heh
	if ( ttycon_on )
	{
		tty_Hide();
	}

	va_start( argptr, format );
	vsnprintf( text, sizeof( text ), format, argptr );
	va_end( argptr );

#ifndef DEDICATED
	CL_Shutdown( text, qtrue );
#endif

	Com_Log( SEV_FATAL, LOG_CH(ch_system), "Sys_Error: %s", text );
	fprintf( stderr, "Sys_Error: %s\n", text ); // belt-and-suspenders: bypasses pipeline

	Sys_Exit( 1 ); // bk010104 - use single exit point.
}


void floating_point_exception_handler( int whatever )
{
	signal( SIGFPE, floating_point_exception_handler );
}


/*
=================
Unix crash handler

Catches SIGSEGV / SIGBUS / SIGFPE / SIGILL / SIGABRT, writes a structured
JSON crash report + async-signal-safe backtrace, then re-raises the signal
so the OS still produces a core dump.
=================
*/
static volatile sig_atomic_t s_crashSignalInProgress = 0;

static const char *Sys_SignalName( int sig )
{
	switch ( sig ) {
		case SIGSEGV: return "SIGSEGV";
		case SIGBUS:  return "SIGBUS";
		case SIGFPE:  return "SIGFPE";
		case SIGILL:  return "SIGILL";
		case SIGABRT: return "SIGABRT";
		case SIGHUP:  return "SIGHUP";
		case SIGQUIT: return "SIGQUIT";
		case SIGTERM: return "SIGTERM";
		case SIGTRAP: return "SIGTRAP";
		case SIGINT:  return "SIGINT";
		default:      return "unknown";
	}
}

static void Sys_CrashSignal( int sig, siginfo_t *info, void *ucontext )
{
	void  *frames[ 64 ];
	int    nframes;
	char   addressText[ 64 ];
	const char *reason;

	(void)ucontext;

	/* Recursive signal — avoid infinite loops, just write a note and die. */
	if ( s_crashSignalInProgress ) {
		static const char msg[] = "\r\nDOUBLE CRASH, aborting.\r\n";
		if ( write( STDERR_FILENO, msg, sizeof( msg ) - 1 ) < 0 ) {
			/* ignored */
		}
		signal( sig, SIG_DFL );
		raise( sig );
		return;
	}
	s_crashSignalInProgress = 1;

	reason = Sys_SignalName( sig );

	/* Reset the terminal so async-signal-safe writes look right on tty
	   dedicated servers. We use the low-level tcsetattr path directly;
	   tty_Hide() can touch FILE* and isn't AS-safe. */
	if ( ttycon_on ) {
		tcsetattr( STDIN_FILENO, TCSADRAIN, &tty_tc );
	}

	/* Async-signal-safe native backtrace to stderr. */
	nframes = backtrace( frames, (int)( sizeof( frames ) / sizeof( frames[ 0 ] ) ) );
	{
		static const char hdr[] = "\r\n=== Wired crash ===\r\n";
		if ( write( STDERR_FILENO, hdr, sizeof( hdr ) - 1 ) < 0 ) {
			/* ignored */
		}
	}
	backtrace_symbols_fd( frames, nframes, STDERR_FILENO );
	Crash_PrintVMStackTracesASS( STDERR_FILENO );

	/* Structured JSON crash report. This may touch FILE* / malloc and is
	   technically not AS-safe, but we're exiting anyway, and the user
	   expects the report to exist after a crash. */
	if ( info != NULL && info->si_addr != NULL ) {
		snprintf( addressText, sizeof( addressText ), "%p", info->si_addr );
	} else {
		Q_strncpyz( addressText, "unknown", sizeof( addressText ) );
	}
	Crash_WriteReport( reason, addressText, "" );

	/* Re-raise with the default disposition to get a core file. */
	signal( sig, SIG_DFL );
	raise( sig );
}

void Sys_InstallCrashHandler( void )
{
	struct sigaction sa;
	int sigs[] = { SIGSEGV, SIGBUS, SIGFPE, SIGILL, SIGABRT };
	int i;

	memset( &sa, 0, sizeof( sa ) );
	sa.sa_sigaction = Sys_CrashSignal;
	sa.sa_flags = SA_SIGINFO | SA_RESETHAND;
	sigemptyset( &sa.sa_mask );

	for ( i = 0; i < (int)( sizeof( sigs ) / sizeof( sigs[ 0 ] ) ); i++ ) {
		sigaction( sigs[ i ], &sa, NULL );
	}
}


// initialize the console input (tty mode if wanted and possible)
// warning: might be called from signal handler
tty_err Sys_ConsoleInputInit( void )
{
	struct termios tc;
	const char* term;

	// TTimo
	// https://zerowing.idsoftware.com/bugzilla/show_bug.cgi?id=390
	// ttycon 0 or 1, if the process is backgrounded (running non interactively)
	// then SIGTTIN or SIGTOU is emitted, if not catched, turns into a SIGSTP
	signal( SIGTTIN, SIG_IGN );
	signal( SIGTTOU, SIG_IGN );

	// If SIGCONT is received, reinitialize console
	signal( SIGCONT, CON_SigCont );

	if ( signal( SIGTSTP, SIG_IGN ) == SIG_DFL )
	{
		signal( SIGTSTP, CON_SigTStp );
	}

	stdin_flags = fcntl( STDIN_FILENO, F_GETFL, 0 );
	if ( stdin_flags == -1 )
	{
		stdin_active = qfalse;
		return TTY_ERROR;
	}

	// set non-blocking mode
	fcntl( STDIN_FILENO, F_SETFL, stdin_flags | O_NONBLOCK );
	stdin_active = qtrue;

	// FIXME TTimo initialize this in Sys_Init or something?
	if ( !ttycon || !ttycon->integer )
	{
		ttycon_on = qfalse;
		return TTY_DISABLED;

	}
	term = getenv( "TERM" );
	if ( isatty( STDIN_FILENO ) != 1 || !term || !strcmp( term, "dumb" ) || !strcmp( term, "raw" ) )
	{
		ttycon_on = qfalse;
		return TTY_ERROR;
	}

	Field_Clear( &tty_con );
	tcgetattr( STDIN_FILENO, &tty_tc );
	tty_erase = tty_tc.c_cc[ VERASE ];
	tty_eof = tty_tc.c_cc[ VEOF ];
	tc = tty_tc;

	/*
		ECHO: don't echo input characters
		ICANON: enable canonical mode.  This  enables  the  special
			characters  EOF,  EOL,  EOL2, ERASE, KILL, REPRINT,
			STATUS, and WERASE, and buffers by lines.
		ISIG: when any of the characters  INTR,  QUIT,  SUSP,  or
			DSUSP are received, generate the corresponding signal
	*/
	tc.c_lflag &= ~(ECHO | ICANON);
	/*
		ISTRIP strip off bit 8
		INPCK enable input parity checking
	*/
	tc.c_iflag &= ~(ISTRIP | INPCK);
	tc.c_cc[VMIN] = 1;
	tc.c_cc[VTIME] = 0;
	tcsetattr( STDIN_FILENO, TCSADRAIN, &tc );

	if ( ttycon_ansicolor && ttycon_ansicolor->integer )
	{
		ttycon_color_on = qtrue;
	}

	ttycon_on = qtrue;

	tty_Hide();
	tty_Show();

	return TTY_ENABLED;
}


char *Sys_ConsoleInput( void )
{
	// we use this when sending back commands
	static char text[ sizeof( tty_con.buffer ) ];
	int avail;
	char key;
	char *s;
	field_t history;

	if ( ttycon_on )
	{
		avail = read( STDIN_FILENO, &key, 1 );
		if (avail != -1)
		{
			// we have something
			// backspace?
			// NOTE TTimo testing a lot of values .. seems it's the only way to get it to work everywhere
			if ((key == tty_erase) || (key == 127) || (key == 8))
			{
				if (tty_con.cursor > 0)
				{
					tty_con.cursor--;
					tty_con.buffer[tty_con.cursor] = '\0';
					tty_Back();
				}
				return NULL;
			}

			// check if this is a control char
			if (key && key < ' ')
			{
				if (key == '\n')
				{
					// push it in history
					Con_SaveField( &tty_con );
					s = tty_con.buffer;
					while ( *s == '\\' || *s == '/' ) // skip leading slashes
						s++;
					Q_strncpyz( text, s, sizeof( text ) );
					Field_Clear( &tty_con );
					write( STDOUT_FILENO, "\n]", 2 );
					return text;
				}

				if (key == '\t')
				{
					tty_Hide();
					Field_AutoComplete( &tty_con );
					tty_Show();
					return NULL;
				}

				avail = read( STDIN_FILENO, &key, 1 );
				if (avail != -1)
				{
					// VT 100 keys
					if (key == '[' || key == 'O')
					{
						avail = read( STDIN_FILENO, &key, 1 );
						if (avail != -1)
						{
							switch (key)
							{
							case 'A':
								if ( Con_HistoryGetPrev( &history ) )
								{
									tty_Hide();
									tty_con = history;
									tty_Show();
								}
								tty_FlushIn();
								return NULL;
								break;
							case 'B':
								if ( Con_HistoryGetNext( &history ) )
								{
									tty_Hide();
									tty_con = history;
									tty_Show();
								}
								tty_FlushIn();
								return NULL;
								break;
							case 'C': // right
							case 'D': // left
							//case 'H': // home
							//case 'F': // end
								return NULL;
							}
						}
					}
				}

				if ( key == 12 ) // clear teaminal
				{
					write( STDOUT_FILENO, "\ec]", 3 );
					if ( tty_con.cursor )
					{
						write( STDOUT_FILENO, tty_con.buffer, tty_con.cursor );
					}
					tty_FlushIn();
					return NULL;
				}

				Com_Log( SEV_DEBUG, LOG_CH(ch_system), "dropping ISCTL sequence: %d, tty_erase: %d\n", key, tty_erase );
				tty_FlushIn();
				return NULL;
			}
			if ( tty_con.cursor >= sizeof( text ) - 1 )
				return NULL;
			// push regular character
			tty_con.buffer[ tty_con.cursor ] = key;
			tty_con.cursor++;
			// print the current line (this is differential)
			write( STDOUT_FILENO, &key, 1 );
		}
		return NULL;
	}
	else if ( stdin_active && com_dedicated->integer )
	{
		int len;
		fd_set fdset;
		struct timeval timeout;

		FD_ZERO( &fdset );
		FD_SET( STDIN_FILENO, &fdset ); // stdin
		timeout.tv_sec = 0;
		timeout.tv_usec = 0;
		if ( select( STDIN_FILENO + 1, &fdset, NULL, NULL, &timeout) == -1 || !FD_ISSET( STDIN_FILENO, &fdset ) )
		{
			return NULL;
		}

		len = read( STDIN_FILENO, text, sizeof( text ) );
		if ( len == 0 ) // eof!
		{
			fcntl( STDIN_FILENO, F_SETFL, stdin_flags );
			stdin_active = qfalse;
			return NULL;
		}

		if ( len < 1 )
			return NULL;

		text[len-1] = '\0'; // rip off the /n and terminate
		s = text;

		while ( *s == '\\' || *s == '/' ) // skip leading slashes
			s++;

		return s;
	}

	return NULL;
}


/*
=================
Sys_SendKeyEvents

Platform-dependent event handling
=================
*/
void Sys_SendKeyEvents( void )
{
#ifndef DEDICATED
	HandleEvents();
#endif
}


/*
==================
Sys_Sleep

Block execution for msec or until input is received.
==================
*/
void Sys_Sleep( int msec ) {
	struct timeval timeout;
	fd_set fdset;
	int res;

	//if ( msec == 0 )
	//	return;

	if ( msec < 0 ) {
		// special case: wait for console input or network packet
		if ( stdin_active ) {
			msec = 300;
			do {
				FD_ZERO( &fdset );
				FD_SET( STDIN_FILENO, &fdset );
				timeout.tv_sec = msec / 1000;
				timeout.tv_usec = (msec % 1000) * 1000;
				res = select( STDIN_FILENO + 1, &fdset, NULL, NULL, &timeout );
			} while ( res == 0 && NET_Sleep( 10 * 1000 ) );
		} else {
			// can happen only if no map loaded
			// which means we totally stuck as stdin is also disabled :P
			//usleep( 300 * 1000 );
			while ( NET_Sleep( 3000 * 1000 ) )
				;
		}
		return;
	}
#if 1
	struct timespec req;
	req.tv_sec = msec / 1000;
	req.tv_nsec = ( msec % 1000 ) * 1000000;
	nanosleep( &req, NULL );
#else
	if ( com_dedicated->integer && stdin_active ) {
		FD_ZERO( &fdset );
		FD_SET( STDIN_FILENO, &fdset );
		timeout.tv_sec = msec / 1000;
		timeout.tv_usec = (msec % 1000) * 1000;
		select( STDIN_FILENO + 1, &fdset, NULL, NULL, &timeout );
	} else {
		usleep( msec * 1000 );
	}
#endif
}


// Sys_Mutex — wraps pthread_mutex_t in the sys_mutex_t opaque buffer.
_Static_assert( sizeof(pthread_mutex_t) <= SYS_MUTEX_OPAQUE_SIZE,
	"sys_mutex_t opaque buffer too small for pthread_mutex_t" );

qboolean Sys_MutexInit( sys_mutex_t *m )
{
	return pthread_mutex_init( (pthread_mutex_t *)m->opaque, NULL ) == 0 ? qtrue : qfalse;
}

void Sys_MutexLock( sys_mutex_t *m )
{
	pthread_mutex_lock( (pthread_mutex_t *)m->opaque );
}

void Sys_MutexUnlock( sys_mutex_t *m )
{
	pthread_mutex_unlock( (pthread_mutex_t *)m->opaque );
}

void Sys_MutexDestroy( sys_mutex_t *m )
{
	pthread_mutex_destroy( (pthread_mutex_t *)m->opaque );
}


static const struct Q3ToAnsiColorTable_s
{
	const char Q3color;
	const char *ANSIcolor;
} tty_colorTable[ ] =
{
	{ COLOR_BLACK,    "30" },
	{ COLOR_RED,      "31" },
	{ COLOR_GREEN,    "32" },
	{ COLOR_YELLOW,   "33" },
	{ COLOR_BLUE,     "34" },
	{ COLOR_CYAN,     "36" },
	{ COLOR_MAGENTA,  "35" },
	{ COLOR_WHITE,    "0" }
};


static const char *getANSIcolor( char Q3color ) {
	int i;
	for ( i = 0; i < ARRAY_LEN( tty_colorTable ); i++ ) {
		if ( Q3color == tty_colorTable[ i ].Q3color ) {
			return tty_colorTable[ i ].ANSIcolor;
		}
	}
	return NULL;
}


static qboolean printableChar( char c ) {
	if ( ( c >= ' ' && c <= '~' ) || c == '\n' || c == '\r' || c == '\t' )
		return qtrue;
	else
		return qfalse;
}


void Sys_ANSIColorify( const char *msg, char *buffer, int bufferSize )
{
  int   msgLength;
  int   i;
  char  tempBuffer[ 8 ];
  const char *ANSIcolor;

  if ( !msg || !buffer )
    return;

  msgLength = strlen( msg );
  i = 0;
  qstring_t qs = QS_Wrap( buffer, bufferSize );

  while ( i < msgLength )
  {
    if ( msg[ i ] == '\n' )
    {
      Com_sprintf( tempBuffer, sizeof( tempBuffer ), "%c[0m\n", 0x1B );
      QS_Append( &qs, tempBuffer );
      i += 1;
    }
    else if ( msg[ i ] == Q_COLOR_ESCAPE && ( ANSIcolor = getANSIcolor( msg[ i+1 ] ) ) != NULL )
    {
      Com_sprintf( tempBuffer, sizeof( tempBuffer ), "%c[%sm", 0x1B, ANSIcolor );
      QS_Append( &qs, tempBuffer );
      i += 2;
    }
    else
    {
      if ( printableChar( msg[ i ] ) ) {
        QS_AppendChar( &qs, msg[ i ] );
      }
      i += 1;
    }
  }
}


void Sys_Print( const char *msg )
{
	char printmsg[ MAXPRINTMSG ];
	size_t len;

	if ( ttycon_on )
	{
		tty_Hide();
	}

	if ( ttycon_on && ttycon_color_on )
	{
		Sys_ANSIColorify( msg, printmsg, sizeof( printmsg ) );
		len = strlen( printmsg );
	}
	else
	{
		char *out = printmsg;
		while ( *msg != '\0' && out < printmsg + sizeof( printmsg ) )
		{
			if ( printableChar( *msg ) )
				*out++ = *msg;
			msg++;
		}
		len = out - printmsg;
	}

	write( STDERR_FILENO, printmsg, len );

	if ( ttycon_on )
	{
		tty_Show();
	}
}


void QDECL Sys_SetStatus( const char *format, ... )
{
	return;
}


void Sys_ConfigureFPU( void )  // bk001213 - divide by zero
{
#ifdef __linux__
#ifdef __i386
#ifdef __GLIBC__
#ifndef NDEBUG
	// bk0101022 - enable FPE's in debug mode
	static int fpu_word = _FPU_DEFAULT & ~(_FPU_MASK_ZM | _FPU_MASK_IM);
	int current = 0;
	_FPU_GETCW( current );
	if ( current!=fpu_word)
	{
#if 0
		Com_Log( SEV_INFO, LOG_CH(ch_system), "FPU Control 0x%x (was 0x%x)\n", fpu_word, current );
		_FPU_SETCW( fpu_word );
		_FPU_GETCW( current );
		assert(fpu_word==current);
#endif
	}
#else // NDEBUG
	static int fpu_word = _FPU_DEFAULT;
	_FPU_SETCW( fpu_word );
#endif // NDEBUG
#endif // __GLIBC__
#endif // __i386
#endif // __linux
}


void Sys_PrintBinVersion( const char* name )
{
	const char *date = __DATE__;
	const char *time = __TIME__;
	const char *sep = "==============================================================";

	fprintf( stdout, "\n\n%s\n", sep );
#ifdef DEDICATED
	fprintf( stdout, "Linux Quake3 Dedicated Server [%s %s]\n", date, time );
#else
	fprintf( stdout, "Linux Quake3 Full Executable  [%s %s]\n", date, time );
#endif
	fprintf( stdout, " local install: %s\n", name );
	fprintf( stdout, "%s\n\n", sep );
}


#ifdef __APPLE__
static char binaryPath[ MAX_OSPATH ] = { 0 };
static char installPath[ MAX_OSPATH ] = { 0 };


/*
=================
Sys_SetBinaryPath
=================
*/
static void Sys_SetBinaryPath( const char *path )
{
	char *d;
	Q_strncpyz( binaryPath, path, sizeof( binaryPath ) );

	d = dirname( binaryPath );
	if ( d != NULL && d != binaryPath )
	{
		Q_strncpyz( binaryPath, d, sizeof( binaryPath ) );
	}
}


/*
=================
Sys_SetDefaultBasePath
=================
*/
static void Sys_SetDefaultBasePath( const char *path )
{
	Q_strncpyz( installPath, path, sizeof( installPath ) );
}


/*
=================
Sys_AppBundleRoot
Returns the .app bundle path when passed a path inside it (i.e., when the
input ends in /Contents/MacOS). Otherwise returns the input unchanged.

Strips two levels: <root>/Foo.app/Contents/MacOS → <root>/Foo.app.
=================
*/
static char *Sys_AppBundleRoot( char *dir )
{
	static char cwd[MAX_OSPATH];

	Q_strncpyz( cwd, dir, sizeof( cwd ) );
	if ( strcmp( basename( cwd ), "MacOS" ) != 0 )
		return dir;

	Q_strncpyz( cwd, dirname( cwd ), sizeof( cwd ) );
	if ( strcmp( basename( cwd ), "Contents" ) != 0 )
		return dir;

	Q_strncpyz( cwd, dirname( cwd ), sizeof( cwd ) );
	if ( strstr( basename( cwd ), ".app" ) == NULL )
		return dir;

	return cwd;
}
#endif // __APPLE__


/*
=================
Sys_DefaultBasePath
=================
*/
const char *Sys_DefaultBasePath( void )
{
#ifdef __APPLE__
	if ( installPath[0] != '\0' )
		return installPath;
	else
#endif
		return Sys_Pwd();
}


/*
=================
Sys_BinName

This resolves any symlinks to the binary. It's disabled for debug
builds because there are situations where you are likely to want
to symlink to binaries and /not/ have the links resolved.
=================
*/
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif
const char *Sys_BinName( const char *arg0 )
{
	static char dst[ PATH_MAX ];

#ifdef NDEBUG

#if defined (__linux__)
	int n = readlink( "/proc/self/exe", dst, PATH_MAX - 1 );

	if ( n >= 0 && n < PATH_MAX )
		dst[ n ] = '\0';
	else
		Q_strncpyz( dst, arg0, PATH_MAX );
#elif defined (__APPLE__)
	uint32_t bufsize = sizeof( dst );

	if ( _NSGetExecutablePath( dst, &bufsize ) == -1 )
	{
		Q_strncpyz( dst, arg0, PATH_MAX );
	}
#else

#warning Sys_BinName not implemented
	Q_strncpyz( dst, arg0, PATH_MAX );
#endif

#else // DEBUG
	Q_strncpyz( dst, arg0, PATH_MAX );
#endif
	return dst;
}


static int Sys_ParseArgs( int argc, const char* argv[] )
{
	if ( argc == 2 )
	{
		if ( ( !strcmp( argv[1], "--version" ) ) || ( !strcmp( argv[1], "-v" ) ) )
		{
			Sys_PrintBinVersion( Sys_BinName( argv[0] ) );
			return 1;
		}
	}

	return 0;
}


/*
=================
Sys_ParseNoHardReboot

Scan the raw argv for `+set com_noHardReboot 1` so the watchdog knows
whether to stay out of the way. We can't use the cvar system here
because it hasn't been initialised yet.
=================
*/
#ifdef DEDICATED
static qboolean Sys_ParseNoHardReboot( int argc, const char *argv[] )
{
	int i;
	for ( i = 1; i + 2 < argc; i++ ) {
		if ( strcmp( argv[ i ], "+set" ) == 0
			&& strcmp( argv[ i + 1 ], "com_noHardReboot" ) == 0
			&& atoi( argv[ i + 2 ] ) != 0 ) {
			return qtrue;
		}
	}
	return qfalse;
}

/*
=================
Sys_IsDedicatedArgv

Scan argv to determine whether this invocation is a dedicated server.
The client binary links the same unix_main.c but is built without
DEDICATED; the dedicated binary is always dedicated regardless of
command line, so this always returns qtrue inside #ifdef DEDICATED.
=================
*/
static qboolean Sys_IsDedicatedArgv( int argc, const char *argv[] )
{
	(void)argc;
	(void)argv;
	return qtrue;
}

/*
=================
Sys_RunWatchdog

Two-process supervisor for dedicated servers on POSIX platforms:

  parent (watchdog)
    +- fork() child (actual server)
    +- waitpid(child)
       - child exited 0 or died on SIGINT/SIGTERM → clean exit, parent exits
       - child exited with RESTART_EXIT_CODE → parent forks a new child
       - child died on other signal or exited non-zero → parent logs and
         forks a new child

Disabled by `+set com_noHardReboot 1`.
=================
*/
#define HARD_REBOOT_EXIT_CODE 42

static qboolean Sys_RunWatchdog( int argc, const char *argv[] )
{
	pid_t child;
	int   status;
	int   restartCount;

	if ( Sys_ParseNoHardReboot( argc, argv ) ) {
		return qfalse; // run in-process
	}
	if ( !Sys_IsDedicatedArgv( argc, argv ) ) {
		return qfalse;
	}

	restartCount = 0;
	while ( 1 ) {
		child = fork();
		if ( child < 0 ) {
			fprintf( stderr, "Sys_RunWatchdog: fork() failed: %s\n", strerror( errno ) );
			return qfalse; // fall back to in-process
		}
		if ( child == 0 ) {
			return qfalse; // child returns to main() and keeps running
		}

		// Parent: block until child exits.
		for ( ;; ) {
			pid_t r = waitpid( child, &status, 0 );
			if ( r == (pid_t)-1 ) {
				if ( errno == EINTR ) continue;
				fprintf( stderr, "Sys_RunWatchdog: waitpid() failed: %s\n", strerror( errno ) );
				_exit( 1 );
			}
			break;
		}

		if ( WIFEXITED( status ) ) {
			int code = WEXITSTATUS( status );
			if ( code == HARD_REBOOT_EXIT_CODE ) {
				restartCount++;
				fprintf( stderr, "Sys_RunWatchdog: child requested restart (#%d), relaunching.\n",
					restartCount );
				continue;
			}
			fprintf( stderr, "Sys_RunWatchdog: child exited %d, watchdog exiting.\n", code );
			_exit( code );
		}
		if ( WIFSIGNALED( status ) ) {
			int sig = WTERMSIG( status );
			if ( sig == SIGINT || sig == SIGTERM || sig == SIGHUP ) {
				fprintf( stderr, "Sys_RunWatchdog: child killed by signal %d, watchdog exiting.\n", sig );
				_exit( 128 + sig );
			}
			restartCount++;
			fprintf( stderr, "Sys_RunWatchdog: child died on signal %d (#%d), relaunching.\n",
				sig, restartCount );
			/* Small rate-limit so a crash loop doesn't burn CPU. */
			sleep( 1 );
			continue;
		}
		// Unknown status — just relaunch.
		restartCount++;
		fprintf( stderr, "Sys_RunWatchdog: unknown child exit status %d (#%d), relaunching.\n",
			status, restartCount );
		continue;
	}
	return qfalse; // unreachable
}

/*
=================
Sys_HardReboot

Command handler for `sv_restartProcess`. Tells the child to terminate
with the restart code so the watchdog will relaunch it. If there is no
watchdog (com_noHardReboot 1, or non-DEDICATED) we simply exit normally.
=================
*/
void Sys_HardReboot( void )
{
	Com_Log( SEV_INFO, LOG_CH(ch_system), "Hard reboot requested — exiting with restart code.\n" );
	_exit( HARD_REBOOT_EXIT_CODE );
}

static void Sys_HardReboot_f( void )
{
	Sys_HardReboot();
}
#endif // DEDICATED


int main( int argc, const char* argv[] )
{
	char con_title[ MAX_CVAR_VALUE_STRING ];
	int xpos, ypos;
	//qboolean useXYpos;
	char  *cmdline;
	int   len, i;
	tty_err	err;

#ifdef __APPLE__
	// This is passed if we are launched by double-clicking
	if ( argc >= 2 && strncmp( argv[1], "-psn", 4 ) == 0 ) {
		argc = 1;
	}
#endif

	if ( Sys_ParseArgs( argc, argv ) )
	{
		return 0; // print version and exit
	}

#ifdef DEDICATED
	// Parent watchdog loop — when it returns, we are the child and should
	// continue normal initialisation. When the child exits the parent will
	// either fork a new child or exit itself.
	if ( Sys_RunWatchdog( argc, argv ) ) {
		// Parent returned qtrue means "we handled it, exit now". Today
		// Sys_RunWatchdog never returns qtrue (it either returns qfalse
		// to the child or _exit()s the parent), but this branch future
		// proofs the contract.
		return 0;
	}
#endif

#ifdef __APPLE__
	/* Sys_SetBinaryPath gives <root>/Foo.app/Contents/MacOS;
	 * Sys_AppBundleRoot lifts that to <root>/Foo.app — fs_installpath wants
	 * the .app itself so FS_GetInstallBinaryPath/ResourcePath can append
	 * Contents/MacOS and Contents/Resources from a single anchor. */
	Sys_SetBinaryPath( argv[ 0 ] );
	Sys_SetDefaultBasePath( Sys_AppBundleRoot( binaryPath ) );
#endif

	// merge the command line, this is kinda silly
	for ( len = 1, i = 1; i < argc; i++ )
		len += strlen( argv[i] ) + 1;

	cmdline = malloc( len );
	*cmdline = '\0';
	for ( i = 1; i < argc; i++ )
	{
		if ( i > 1 )
			strcat( cmdline, " " );
		strcat( cmdline, argv[i] );
	}

	/*useXYpos = */ Com_EarlyParseCmdLine( cmdline, con_title, sizeof( con_title ), &xpos, &ypos );

	// bk000306 - clear queues
//	memset( &eventQue[0], 0, sizeof( eventQue ) );
//	memset( &sys_packetReceived[0], 0, sizeof( sys_packetReceived ) );

	Com_Init( cmdline );

#ifdef DEDICATED
	{
		cvar_t *cv = Cvar_Get( "com_noHardReboot", "0", CVAR_INIT );
		Cvar_CheckRange( cv, "0", "1", CV_INTEGER );
		Cvar_SetDescription( cv,
			"Disable the POSIX parent-watchdog that auto-relaunches the\n"
			"dedicated server on abnormal exit or the sv_restartProcess\n"
			"command. Effective only at startup (must be set via +set on\n"
			"the command line)." );
		Cmd_AddCommand( "sv_restartProcess", Sys_HardReboot_f );
	}
#endif

	// Sys_ConsoleInputInit() might be called in signal handler
	// so modify/init any cvars here
	ttycon = Cvar_Get( "ttycon", "1", 0 );
	Cvar_SetDescription(ttycon, "Enable access to input/output console terminal.");
	ttycon_ansicolor = Cvar_Get( "ttycon_ansicolor", "0", CVAR_ARCHIVE );
	Cvar_SetDescription(ttycon_ansicolor, "Convert in-game color codes to ANSI color codes in console terminal.");

	err = Sys_ConsoleInputInit();
	if ( err == TTY_ENABLED )
	{
		Com_Log( SEV_INFO, LOG_CH(ch_system), "Started tty console (use +set ttycon 0 to disable)\n" );
	}
	else
	{
		if ( err == TTY_ERROR )
		{
			Com_Log( SEV_INFO, LOG_CH(ch_system), "stdin is not a tty, tty console mode failed\n" );
			Cvar_Set( "ttycon", "0" );
		}
	}

#ifdef DEDICATED
	// init here for dedicated, as we don't have GLimp_Init
	InitSig();
#endif

	while (1)
	{
#ifdef __linux__
		Sys_ConfigureFPU();
#endif

#ifdef DEDICATED
		// run the game
		Com_Frame( qfalse );
#else
		// check for other input devices
		IN_Frame();
		// run the game
		Com_Frame( CL_NoDelay() );
#endif
	}
	// never gets here
	return 0;
}

#ifndef __APPLE__
/* Apple has a real implementation in code/macosx/macosx_main.c that pins the
 * main thread to a time-constraint scheduling class. Linux/BSD have no
 * equivalent kernel API; latency tuning is left to the OS scheduler. */
void Sys_SetMainThreadPolicy( void ) { }
#endif
