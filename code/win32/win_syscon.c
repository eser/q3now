/*
===========================================================================
win_syscon.c — stdio-based console for Windows (client + dedicated)

Implements the Sys_Console* / Conbuf_AppendText / Sys_SetErrorText API
that qcommon expects. Both q3now.x64.exe (client, GUI subsystem) and
q3now-ded.x64.exe (dedicated, console subsystem) link this file.

History:
  This replaces id Software's 1999-vintage custom Win32 console window
  (~1100 LOC of GDI/window setup, message handlers, edit-control output
  buffer, status bar, "Quit" button). Phase 3 of the Windows-console
  modernization (2026-05-05) deleted the GUI console in favor of stdio
  for both targets — see docs/health.md "win_syscon.c modernization
  (Phase 3 audit)".

Phase 1 (2026-05-05) had introduced a parallel win_syscon_stdio.c for
the dedicated build only; phase 3 consolidates that file into here so
client and dedicated share a single implementation. Behavior differs
in two places only, gated #ifdef DEDICATED:
  1. Sys_ConsoleInput — reads stdin for the dedicated server's typed
     command interface ("quit", "kick", "status"). Client returns NULL
     (the in-game console handles user input via the game window).
  2. Sys_SetErrorText — client surfaces fatal errors via a Windows
     MessageBox so double-click users see something; dedicated writes
     to stderr only (no GUI dialog blocking an unattended server).

The deprecated `viewlog` cvar's window-toggle effect is gone; the cvar
itself stays registered in common.c for backwards compat with existing
config files. See docs/launcher.md / health.md viewlog entry.
===========================================================================
*/

#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"
#include "win_local.h"

#include <windows.h>
#include <stdio.h>

/*
==============
Sys_CreateConsole

Console-subsystem dedicated already has a parent console; client
(GUI subsystem) connects to one via Sys_AttachParentConsole earlier in
WinMain. Either way, std handles are usable by the time we get here.

Configure line-buffered stdout/stderr for predictable real-time output
under pipe redirection (smoke harnesses, NSSM service logs, etc.) and
set the parent terminal title when one is attached.
==============
*/
void Sys_CreateConsole( const char *title, int xPos, int yPos, qboolean useXYpos )
{
	(void)xPos; (void)yPos; (void)useXYpos;

	setvbuf( stdout, NULL, _IOLBF, 4096 );
	setvbuf( stderr, NULL, _IOLBF, 4096 );

	if ( title != NULL && title[0] != '\0' ) {
		SetConsoleTitleA( title );
	}
}

void Sys_DestroyConsole( void )
{
	/* No-op: we don't own the parent console. */
}

void Sys_ShowConsole( int visLevel, qboolean quitOnClose )
{
	(void)visLevel; (void)quitOnClose;
	/* No-op: there is no separate console window to show/hide.
	 * The deprecated `viewlog` cvar still calls this but the call has
	 * no effect — engine output goes to stdio (and the in-game console
	 * for client). */
}

/*
==============
Conbuf_AppendText

Sys_Print's output sink. Strips Q3 color codes (`^N` where N is 0-9 or
'a'-'z') so terminals don't render them as literal "^1" / "^7" garbage.
Future enhancement: map to ANSI escape codes for color-aware terminals.
==============
*/
void Conbuf_AppendText( const char *msg )
{
	if ( msg == NULL || msg[0] == '\0' ) {
		return;
	}

	char buf[ MAXPRINTMSG ];
	char *out = buf;
	const char *end = buf + sizeof( buf ) - 1;

	while ( *msg != '\0' && out < end ) {
		if ( Q_IsColorString( msg ) ) {
			msg += 2;
			continue;
		}
		*out++ = *msg++;
	}
	*out = '\0';

	fputs( buf, stdout );
	fflush( stdout );
}

/*
==============
Sys_SetStatus

Was: GUI console status bar pane ("Loading map q3dm7"). Now: a
stderr-prefixed informational line. Server callers (sv_init.c) use
this for transient progress messages during long operations.
==============
*/
void QDECL Sys_SetStatus( const char *format, ... )
{
	va_list  argptr;
	char     buf[1024];

	va_start( argptr, format );
	vsnprintf( buf, sizeof( buf ), format, argptr );
	va_end( argptr );

	fputs( "[status] ", stderr );
	fputs( buf, stderr );
	fputc( '\n', stderr );
	fflush( stderr );
}

/*
==============
Sys_SetErrorText

Surface a fatal error to the user. The shared part — write to stderr
so terminal users / log captures see it — runs unconditionally.

#ifndef DEDICATED: also show a Windows-native MessageBox so the client
binary's double-click users (no terminal attached) see the error
visibly. The MessageBox blocks until the user clicks OK; afterward
Sys_Error's caller proceeds to exit(1).

Dedicated skips the MessageBox to avoid blocking an unattended server.
==============
*/
void Sys_SetErrorText( const char *buf )
{
	if ( buf == NULL || buf[0] == '\0' ) {
		return;
	}

	fputs( buf, stderr );
	fputc( '\n', stderr );
	fflush( stderr );

#ifndef DEDICATED
	MessageBoxA( NULL, buf, "q3now error", MB_OK | MB_ICONERROR );
#endif
}

/*
==============
HandleConsoleEvents

GUI version pumped Win32 messages for the custom console window. After
phase 3 there is no such window — nothing to pump. Cheap no-op kept
for the existing per-frame call from Com_Frame.
==============
*/
void HandleConsoleEvents( void )
{
	/* No GUI window — no messages to dispatch. */
}

/*
==============
Sys_ConsoleInput

#ifdef DEDICATED: non-blocking stdin reader for the dedicated server's
typed command interface ("quit", "kick", "status", etc.). Returns one
complete line per call (without the trailing newline), or NULL when
no complete line is available yet. Per-frame polling cadence comes
from the engine's main loop. Mirrors the structure of Linux's
unix_main.c Sys_ConsoleInput.

Two stdin modes handled:
  1. Real console (tty): PeekConsoleInputA / ReadConsoleInputA pulls
     keystrokes one at a time; line buffering and echo are done here.
  2. Redirected pipe or file: PeekNamedPipe + ReadFile drains
     available bytes without blocking.

#ifndef DEDICATED (client): returns NULL. The client doesn't read
stdin for typed commands — the in-game console (toggled with `~`)
handles user command input via the game window. If the user runs
`q3now.x64.exe +set dedicated 1` (deprecated client-as-dedicated
flow), they have no typed-command interface; direct them to
q3now-ded.x64.exe.
==============
*/
char *Sys_ConsoleInput( void )
{
#ifdef DEDICATED
	static char buffer[256];
	static int  pos = 0;

	HANDLE h = GetStdHandle( STD_INPUT_HANDLE );
	if ( h == NULL || h == INVALID_HANDLE_VALUE ) {
		return NULL;
	}

	/* Detect tty vs pipe/file: GetConsoleMode succeeds only for a real console. */
	DWORD ignored;
	qboolean isConsole = GetConsoleMode( h, &ignored ) ? qtrue : qfalse;

	for (;;) {
		char ch;

		if ( isConsole ) {
			INPUT_RECORD rec;
			DWORD got = 0;

			if ( !PeekConsoleInputA( h, &rec, 1, &got ) || got == 0 ) {
				return NULL;
			}
			if ( !ReadConsoleInputA( h, &rec, 1, &got ) || got == 0 ) {
				return NULL;
			}
			if ( rec.EventType != KEY_EVENT || !rec.Event.KeyEvent.bKeyDown ) {
				continue;
			}
			ch = rec.Event.KeyEvent.uChar.AsciiChar;
			if ( ch == 0 ) {
				continue;
			}
		} else {
			DWORD avail = 0;
			if ( !PeekNamedPipe( h, NULL, 0, NULL, &avail, NULL ) || avail == 0 ) {
				return NULL;
			}
			DWORD got = 0;
			if ( !ReadFile( h, &ch, 1, &got, NULL ) || got == 0 ) {
				return NULL;
			}
		}

		/* Echo on tty (we consumed raw input, so we own visual feedback). */
		if ( isConsole ) {
			if ( ch == '\r' || ch == '\n' ) {
				putchar( '\n' );
			} else if ( ch == '\b' || ch == 0x7f ) {
				if ( pos > 0 ) {
					fputs( "\b \b", stdout );
				}
			} else if ( ch >= ' ' && ch < 0x7f ) {
				putchar( ch );
			}
			fflush( stdout );
		}

		/* Line accumulation. */
		if ( ch == '\r' || ch == '\n' ) {
			buffer[pos] = '\0';
			int len = pos;
			pos = 0;
			return ( len > 0 ) ? buffer : NULL;
		}
		if ( ch == '\b' || ch == 0x7f ) {
			if ( pos > 0 ) {
				pos--;
			}
			continue;
		}
		if ( pos < (int)sizeof( buffer ) - 1 && ch >= ' ' ) {
			buffer[pos++] = (char)ch;
		}
	}
#else
	/* Client: no stdin command interface. */
	return NULL;
#endif
}
