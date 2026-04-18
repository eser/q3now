#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"

#ifdef __APPLE__
#include <mach/mach.h>
#include <mach/thread_act.h>
#include <mach/thread_policy.h>
#include <mach/mach_time.h>

void Sys_SetMainThreadPolicy( void ) {
	mach_timebase_info_data_t tb;
	mach_timebase_info( &tb );
	/* 4 ms period, 2 ms computation budget — tells Darwin we're latency-sensitive */
	uint64_t period_abs  = 4000000ULL * tb.denom / tb.numer;
	uint64_t computation = 2000000ULL * tb.denom / tb.numer;
	uint64_t constraint  = 4000000ULL * tb.denom / tb.numer;
	thread_time_constraint_policy_data_t policy = {
		.period      = (uint32_t)period_abs,
		.computation = (uint32_t)computation,
		.constraint  = (uint32_t)constraint,
		.preemptible = TRUE,
	};
	thread_policy_set( mach_thread_self(),
	                   THREAD_TIME_CONSTRAINT_POLICY,
	                   (thread_policy_t)&policy,
	                   THREAD_TIME_CONSTRAINT_POLICY_COUNT );
}
#else
void Sys_SetMainThreadPolicy( void ) { /* no-op on non-Apple */ }
#endif
