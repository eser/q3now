// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
//
// stalltrace.c — see stalltrace.h. Owns the `stalltrace` cvar, the active flag
// the bracket macro reads, and the timestamped log helpers (on a dedicated log
// channel so call-site TUs need no channel declaration).

#include "../q_shared.h"
#include "../qcommon.h"
#include "stalltrace.h"

LOG_DECLARE_CHANNEL( ch_stalltrace, "stalltrace" );

int stalltrace_active = 0;

static cvar_t *stalltrace_cvar;

// keep stalltrace_active in sync when the cvar changes at runtime.
static void Stalltrace_OnChange( cvar_t *cv )
{
	stalltrace_active = ( cv && cv->integer ) ? 1 : 0;
}

void stalltrace_register( void )
{
	static const cvarDesc_t d = CVAR_BOOL_CB( "stalltrace", "0", CVAR_CHEAT,
		"Diagnostic: log timestamped enter/exit around window/audio/focus "
		"lifecycle calls so a freeze pins the blocking call (run with "
		"+set log_file_mode append_synced). Default 0.", Stalltrace_OnChange );
	stalltrace_cvar = Cvar_Register( &d );
	Stalltrace_OnChange( stalltrace_cvar );
}

int64_t stalltrace_now( void )
{
	return Sys_Microseconds();
}

void stalltrace_enter( const char *name )
{
	Com_Log( SEV_INFO, LOG_CH( ch_stalltrace ), "STALLTRACE enter %s t=%lld\n",
		name, (long long)Sys_Microseconds() );
}

void stalltrace_exit( const char *name, int64_t enter_us )
{
	Com_Log( SEV_INFO, LOG_CH( ch_stalltrace ), "STALLTRACE exit  %s dt=%lldus\n",
		name, (long long)( Sys_Microseconds() - enter_us ) );
}

void stalltrace_mark( const char *name )
{
	Com_Log( SEV_INFO, LOG_CH( ch_stalltrace ), "STALLTRACE mark  %s t=%lld\n",
		name, (long long)Sys_Microseconds() );
}
