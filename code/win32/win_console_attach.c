/*
===========================================================================
win_console_attach.c — reattach stdio to parent console for client (wired.x64.exe)

wired.x64.exe is a Windows GUI-subsystem binary because it has a game
window. Without intervention, the OS NULLs std handles when launched
from a terminal — `wired.x64.exe +echo "..." +set developer 1` from a
console produces no visible output even though the engine is writing
to stdout/stderr.

`AttachConsole(ATTACH_PARENT_PROCESS)` reattaches the GUI binary to the
parent's console; `freopen("CONOUT$"/"CONIN$", ...)` rebinds the C
runtime's stdio against the now-attached handles. If launched without
a parent console (double-click from Explorer, scheduled task, etc.),
AttachConsole returns 0 — std handles stay detached, game window opens
normally, no spurious cmd.exe.

This file is the C/Win32 mirror of `launcher/console_windows.go`. The
launcher solved the same dual-mode console/GUI problem first; phase 2
of the Windows-console modernization (2026-05-05) ports the pattern to
the engine client. Linked into the client target only — the dedicated
server uses console subsystem (phase 1) and doesn't need this.

See:
  - docs/health.md "Client AttachConsole for dual-mode console/GUI use"
  - launcher/console_windows.go (Go original of this pattern)
===========================================================================
*/

#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"
#include "win_local.h"

#include <windows.h>
#include <stdio.h>

void Sys_AttachParentConsole( void )
{
	/* Three launch shapes to handle:
	 *
	 *  1. Inherited stdio (cmd redirect `wired.exe > out.txt`, bash/mintty
	 *     pipe, PowerShell capture, NSSM service): the parent passed valid
	 *     std handles to us via STARTUPINFO. C runtime's stdout/stderr/stdin
	 *     are already bound to those handles. **Don't touch them** —
	 *     reopening against CONOUT$ here would blow away the redirect target
	 *     and send output to a phantom console instead of the file/pipe.
	 *
	 *  2. Interactive console (real cmd.exe / Windows Terminal launch with
	 *     no redirect): no inherited handles, but the parent process owns
	 *     a console we can attach to. AttachConsole(ATTACH_PARENT_PROCESS)
	 *     succeeds, then freopen against CONOUT$/CONIN$ binds C runtime
	 *     stdio to the now-attached console.
	 *
	 *  3. Double-click from Explorer / scheduled task / no parent console:
	 *     no inherited handles, AttachConsole fails. No-op the rest — game
	 *     window opens, no terminal output, no spurious cmd.exe.
	 */

	HANDLE hStdOut = GetStdHandle( STD_OUTPUT_HANDLE );
	if ( hStdOut != NULL && hStdOut != INVALID_HANDLE_VALUE ) {
		/* Case 1: inherited stdio. Just configure unbuffered output so
		 * pipe-consumers see writes promptly (relevant for smoke
		 * harnesses that timeout and read partial output). */
		setvbuf( stdout, NULL, _IONBF, 0 );
		setvbuf( stderr, NULL, _IONBF, 0 );
		return;
	}

	/* Cases 2/3: no inherited stdio — try to attach to parent's console.
	 * ATTACH_PARENT_PROCESS == (DWORD)-1; using the literal avoids an
	 * extra header pull. */
	if ( !AttachConsole( (DWORD)-1 ) ) {
		/* Case 3: no parent console. Stdio stays detached; game window
		 * opens normally on its own. */
		return;
	}

	/* Case 2: console attached. Rebind C runtime stdio. */
	(void)freopen( "CONOUT$", "w", stdout );
	(void)freopen( "CONOUT$", "w", stderr );
	(void)freopen( "CONIN$",  "r", stdin );
	setvbuf( stdout, NULL, _IONBF, 0 );
	setvbuf( stderr, NULL, _IONBF, 0 );
}
