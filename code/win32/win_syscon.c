// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 1999-2005 Id Software, Inc.
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

/*
===========================================================================
win_syscon.c — stdio-based console for Windows (client + dedicated)

Implements the Sys_Console* / Conbuf_AppendText / Sys_SetErrorText API
that qcommon expects. Both wired.x64.exe (client, GUI subsystem) and
wired-headless.x64.exe (headless no-GUI, console subsystem) link this file.

History:
  This replaces id Software's 1999-vintage custom Win32 console window
  (~1100 LOC of GDI/window setup, message handlers, edit-control output
  buffer, status bar, "Quit" button). The Windows-console
  modernization (2026-05-05) deleted the GUI console in favor of stdio
  for both targets — see docs/health.md "win_syscon.c modernization
  (audit)".

An earlier change (2026-05-05) had introduced a parallel win_syscon_stdio.c for
the headless build only; this consolidation folds that file into here so
client and headless share a single implementation. Behavior differs in one place only, gated #ifdef HEADLESS:
  1. Sys_ConsoleInput — reads stdin for the headless server's typed
     command interface ("quit", "kick", "status"). Client returns NULL
     (the in-game console handles user input via the game window).

Sys_SetErrorText writes a fatal error to stderr (and qconsole, via the log
path) for both client and dedicated — there is no GUI dialog.

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

/* Win10 Anniversary Update (1607)+ interpret ANSI escape sequences when
   ENABLE_VIRTUAL_TERMINAL_PROCESSING is set on the console output handle.
   Older SDK headers may not define the bit. */
#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#endif

/* qtrue once the console output handle accepts ANSI escapes. Stays qfalse
   on pre-1607 Windows and when stdout is redirected to a file/pipe — then
   Conbuf_AppendText falls back to stripping color codes. */
static qboolean s_winSysconVTP = qfalse;

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

	/* Enable ANSI escape processing so Conbuf_AppendText can render Q3 color
	   codes. Fails (s_winSysconVTP stays qfalse) on pre-1607 Windows or when
	   stdout is not a console (redirected) — Conbuf_AppendText then strips. */
	{
		HANDLE hOut = GetStdHandle( STD_OUTPUT_HANDLE );
		DWORD  mode = 0;
		if ( hOut != NULL && hOut != INVALID_HANDLE_VALUE
			&& GetConsoleMode( hOut, &mode )
			&& SetConsoleMode( hOut, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING ) ) {
			s_winSysconVTP = qtrue;
		}
	}

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
Win_AnsiForColorCode

Maps a Q3 color-code character (the char after '^') to its ANSI SGR escape.
Returns NULL for the CPMA 'a'-'z' palette and unknown codes — those are
stripped (syscon coloring covers ^0-^9 only this pass).
==============
*/
static const char *Win_AnsiForColorCode( char c )
{
	switch ( c ) {
	case '0': return "\033[30m";       /* black                         */
	case '1': return "\033[31m";       /* red                           */
	case '2': return "\033[32m";       /* green                         */
	case '3': return "\033[33m";       /* yellow                        */
	case '4': return "\033[34m";       /* blue                          */
	case '5': return "\033[36m";       /* cyan    — Q3 index 5 -> ANSI 6 */
	case '6': return "\033[35m";       /* magenta — Q3 index 6 -> ANSI 5 */
	case '7': return "\033[37m";       /* white                         */
	case '8': return "\033[38;5;208m"; /* orange  (256-color)           */
	case '9': return "\033[94m";       /* light blue (bright)           */
	default:  return NULL;             /* CPMA a-z / unknown — strip     */
	}
}

/*
==============
Conbuf_AppendText

Sys_Print's output sink. When the console accepts ANSI escapes
(s_winSysconVTP, set in Sys_CreateConsole) Q3 color codes `^0`-`^9` are
translated to ANSI SGR sequences and attributes reset at each newline;
otherwise color codes are stripped so terminals don't render literal
"^1" / "^7" garbage. The CPMA 'a'-'z' palette is always stripped.
==============
*/
void Conbuf_AppendText( const char *msg )
{
	char        buf[ MAXPRINTMSG ];
	char       *out = buf;
	const char *end = buf + sizeof( buf ) - 1;

	if ( msg == NULL || msg[0] == '\0' ) {
		return;
	}

	while ( *msg != '\0' && out < end ) {
		if ( Q_IsColorString( msg ) ) {
			if ( s_winSysconVTP ) {
				const char *esc = Win_AnsiForColorCode( msg[1] );
				if ( esc != NULL ) {
					size_t n = strlen( esc );
					if ( out + n > end ) {
						break;
					}
					memcpy( out, esc, n );
					out += n;
				}
			}
			msg += 2;
			continue;
		}

		if ( s_winSysconVTP && *msg == '\n' ) {
			/* Reset attributes at the line boundary so a color cannot bleed
			   into the next line (mirrors unix Sys_ANSIColorify). */
			if ( out + 5 > end ) {
				break;
			}
			memcpy( out, "\033[0m\n", 5 );
			out += 5;
			msg++;
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

Was: GUI console status bar pane ("Loading map arena7"). Now: a
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

Surface a fatal error to the user by writing it to stderr (and, via the
normal log path, to qconsole). There is no GUI dialog — a fatal error never
opens a popup; terminal users and log captures see the message instead. The
write is unconditional and the same for client and dedicated builds.
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

#ifdef HEADLESS: non-blocking stdin reader for the headless server's
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

#ifndef HEADLESS (client): returns NULL. The client doesn't read
stdin for typed commands — the in-game console (toggled with `~`)
handles user command input via the game window. The client build has
no headless console; for a no-GUI server console, use
wired-headless.x64.exe.
==============
*/
char *Sys_ConsoleInput( void )
{
#ifdef HEADLESS
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
