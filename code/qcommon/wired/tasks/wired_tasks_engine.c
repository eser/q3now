// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "../../q_shared.h"
#include "../../qcommon.h"
#include "wired_tasks_engine.h"

#if defined( __APPLE__ )
#  include <sys/sysctl.h>
#elif !defined( _WIN32 )
#  include <unistd.h>
#endif

LOG_DECLARE_CHANNEL( ch_system, "system" );

#define WIRED_TASKS_MAX_WORKERS 16

static cvar_t         *com_taskWorkers;
static wired_tasks_t  *s_tasks;
static qboolean        s_created;

static void *WT_Alloc( size_t n ) { return Z_Malloc( n ); }
static void  WT_Release( void *p ) { Z_Free( p ); }
static uint64_t WT_Clock( void ) { return (uint64_t)Sys_NanoTime(); }

/* The number of workers "auto" (-1) selects: the performance cores minus the one
 * the main thread uses, so the workers never share a core with the owner. On
 * Apple Silicon the efficiency cores are not counted; elsewhere the logical CPUs
 * are. Capped, because the consumers here are small loops. */
static int WT_AutoWorkers( void )
{
	int cores = 0;
#if defined( __APPLE__ )
	size_t size = sizeof( cores );
	if ( sysctlbyname( "hw.perflevel0.physicalcpu", &cores, &size, NULL, 0 ) != 0 || cores <= 0 ) {
		size = sizeof( cores );
		if ( sysctlbyname( "hw.physicalcpu", &cores, &size, NULL, 0 ) != 0 ) cores = 0;
	}
#elif defined( _WIN32 )
	cores = 0;
#else
	cores = (int)sysconf( _SC_NPROCESSORS_ONLN );
#endif
	cores -= 1;
	if ( cores < 0 ) cores = 0;
	if ( cores > 6 ) cores = 6;
	return cores;
}

void WiredTasks_Register( void )
{
	static const cvarDesc_t d = CVAR_INT( "com_taskWorkers", "-1", CVAR_LATCH,
		"WiredTasks worker threads: -1 (the default) picks performance cores minus "
		"one, N starts N workers, 0 runs everything inline. Threads start only when "
		"a consumer first asks for the scheduler, so a game with no crowd never "
		"creates any. Results do not depend on this value; it only changes which "
		"thread runs a task. Applies at the next start.", -1, WIRED_TASKS_MAX_WORKERS );
	com_taskWorkers = Cvar_Register( &d );
}

wired_tasks_t *WiredTasks_Get( void )
{
	if ( !s_created ) {
		wired_tasks_env_t env;
		wired_tasks_config_t cfg;
		int workers = com_taskWorkers ? com_taskWorkers->integer : 0;
		s_created = qtrue;
		if ( workers < 0 ) workers = WT_AutoWorkers();
		if ( workers > WIRED_TASKS_MAX_WORKERS ) workers = WIRED_TASKS_MAX_WORKERS;
		if ( workers == 0 ) return NULL;      /* inline: no scheduler object at all */
		memset( &env, 0, sizeof( env ) );
		memset( &cfg, 0, sizeof( cfg ) );
		env.alloc = WT_Alloc; env.release = WT_Release; env.clock_ns = WT_Clock;
		cfg.workers = (unsigned)workers;
		cfg.name = "wtask";
		s_tasks = wired_tasks_create( &env, &cfg );
		Com_Log( SEV_INFO, LOG_CH( ch_system ), "WiredTasks: %u worker(s)%s\n",
			s_tasks ? wired_tasks_workers( s_tasks ) : 0u,
			s_tasks ? "" : " (start failed, running inline)" );
	}
	return s_tasks;
}

void WiredTasks_Shutdown( void )
{
	wired_tasks_destroy( s_tasks );
	s_tasks = NULL;
	s_created = qfalse;
}
