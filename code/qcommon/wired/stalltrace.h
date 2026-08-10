// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
//
// stalltrace.h — temporary diagnostic for the relaunch/refocus freeze.
//
// Wraps a window/audio/focus lifecycle call with timestamped enter/exit log
// lines so a hard freeze can be pinned to the exact blocking call: the last
// "STALLTRACE enter <name>" with no matching "STALLTRACE exit <name>" is the
// call that hung. Gated on the `stalltrace` cvar (default off); when off the
// wrapped statement runs with no added work, so the build is byte-identical in
// behaviour. For the exit line to survive a hard freeze, run the engine with a
// flushed log mode: +set log_file_mode append_synced (or overwrite_synced),
// which forces each line to disk immediately.

#ifndef WIRED_STALLTRACE_H
#define WIRED_STALLTRACE_H

#include "../q_shared.h"

// Set from the `stalltrace` cvar at registration (stalltrace_register).
extern int stalltrace_active;

void stalltrace_register( void );

// Logging helpers own their own log channel so call-site TUs need no channel
// declaration. They timestamp internally (Sys_Microseconds).
void stalltrace_enter( const char *name );
void stalltrace_exit( const char *name, int64_t enter_us );
void stalltrace_mark( const char *name );

// Returns the current Sys_Microseconds timestamp (so the macro can pass the
// enter time to stalltrace_exit without each TU including the time header).
int64_t stalltrace_now( void );

// STALLTRACE( name, statement ) — run statement, bracketed by enter/exit logs
// (with elapsed microseconds) when stalltrace is active; otherwise just run the
// statement. `name` is a short string literal identifying the call.
#define STALLTRACE( name, stmt ) \
	do { \
		if ( stalltrace_active ) { \
			int64_t _st_t0 = stalltrace_now(); \
			stalltrace_enter( (name) ); \
			{ stmt; } \
			stalltrace_exit( (name), _st_t0 ); \
		} else { \
			stmt; \
		} \
	} while ( 0 )

// STALLTRACE_MARK( name ) — a single timestamped marker (for handler entry
// points that are not a single wrapped call).
#define STALLTRACE_MARK( name ) \
	do { \
		if ( stalltrace_active ) { \
			stalltrace_mark( (name) ); \
		} \
	} while ( 0 )

#endif // WIRED_STALLTRACE_H
