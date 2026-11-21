// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

/*
WiredTasks private platform layer: mutex, condition variable, thread, yield and
the atomic operations the scheduler needs. Private to the module; callers never
include it. GCC/Clang atomic builtins are used (the shipping compilers, as in
wired/core/thread/thread.c); an unsupported compiler fails loudly here rather
than silently losing the memory ordering the scheduler depends on.
*/

#ifndef WIRED_TASKS_PLATFORM_H
#define WIRED_TASKS_PLATFORM_H

#include <stdint.h>

#if !defined( __GNUC__ ) && !defined( __clang__ )
#  error "WiredTasks needs the GCC/Clang __atomic builtins"
#endif

#define WT_LOAD( p )          __atomic_load_n( ( p ), __ATOMIC_ACQUIRE )
#define WT_STORE( p, v )      __atomic_store_n( ( p ), ( v ), __ATOMIC_RELEASE )
#define WT_ADD( p, v )        __atomic_fetch_add( ( p ), ( v ), __ATOMIC_ACQ_REL )
#define WT_SUB( p, v )        __atomic_fetch_sub( ( p ), ( v ), __ATOMIC_ACQ_REL )
#define WT_ADD_RELAXED( p, v ) __atomic_fetch_add( ( p ), ( v ), __ATOMIC_RELAXED )
#define WT_CAS( p, expected, desired ) \
	__atomic_compare_exchange_n( ( p ), ( expected ), ( desired ), 0, \
		__ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE )

#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#  undef WIN32_LEAN_AND_MEAN
typedef SRWLOCK            wt_mutex_t;
typedef CONDITION_VARIABLE wt_cond_t;
typedef HANDLE             wt_thread_t;
static inline void wt_mutex_init( wt_mutex_t *m )    { InitializeSRWLock( m ); }
static inline void wt_mutex_destroy( wt_mutex_t *m ) { (void)m; }
static inline void wt_mutex_lock( wt_mutex_t *m )    { AcquireSRWLockExclusive( m ); }
static inline void wt_mutex_unlock( wt_mutex_t *m )  { ReleaseSRWLockExclusive( m ); }
static inline void wt_cond_init( wt_cond_t *c )      { InitializeConditionVariable( c ); }
static inline void wt_cond_destroy( wt_cond_t *c )   { (void)c; }
static inline void wt_cond_wait( wt_cond_t *c, wt_mutex_t *m ) { SleepConditionVariableSRW( c, m, INFINITE, 0 ); }
static inline void wt_cond_broadcast( wt_cond_t *c ) { WakeAllConditionVariable( c ); }
static inline void wt_yield( void )                  { SwitchToThread(); }
/* The start record lives in the caller's worker slot (see wired_tasks.c), so it
 * outlives the thread without an allocation here. */
typedef struct { void ( *fn )( void * ); void *arg; } wt_thread_start_t;
static DWORD WINAPI wt_thread_tramp( LPVOID p ) {
	wt_thread_start_t *s = (wt_thread_start_t *)p;
	s->fn( s->arg );
	return 0;
}
static inline int wt_thread_create_rec( wt_thread_t *t, wt_thread_start_t *rec ) {
	*t = CreateThread( NULL, 0, wt_thread_tramp, rec, 0, NULL );
	return *t != NULL;
}
static inline void wt_thread_join( wt_thread_t t ) { WaitForSingleObject( t, INFINITE ); CloseHandle( t ); }
static inline void wt_thread_init_self( const char *name ) { (void)name; }
#else
#  include <pthread.h>
#  include <sched.h>
#  if defined( __APPLE__ )
#    include <pthread/qos.h>
#  endif
typedef pthread_mutex_t wt_mutex_t;
typedef pthread_cond_t  wt_cond_t;
typedef pthread_t       wt_thread_t;
static inline void wt_mutex_init( wt_mutex_t *m )    { pthread_mutex_init( m, NULL ); }
static inline void wt_mutex_destroy( wt_mutex_t *m ) { pthread_mutex_destroy( m ); }
static inline void wt_mutex_lock( wt_mutex_t *m )    { pthread_mutex_lock( m ); }
static inline void wt_mutex_unlock( wt_mutex_t *m )  { pthread_mutex_unlock( m ); }
static inline void wt_cond_init( wt_cond_t *c )      { pthread_cond_init( c, NULL ); }
static inline void wt_cond_destroy( wt_cond_t *c )   { pthread_cond_destroy( c ); }
static inline void wt_cond_wait( wt_cond_t *c, wt_mutex_t *m ) { pthread_cond_wait( c, m ); }
static inline void wt_cond_broadcast( wt_cond_t *c ) { pthread_cond_broadcast( c ); }
static inline void wt_yield( void )                  { sched_yield(); }
typedef struct { void ( *fn )( void * ); void *arg; } wt_thread_start_t;
static void *wt_thread_tramp( void *p ) {
	wt_thread_start_t *s = (wt_thread_start_t *)p;
	s->fn( s->arg );
	return NULL;
}
/* The start record lives in the caller's worker slot (see wired_tasks.c), so it
 * outlives the thread without an allocation here. */
static inline int wt_thread_create_rec( wt_thread_t *t, wt_thread_start_t *rec ) {
	return pthread_create( t, NULL, wt_thread_tramp, rec ) == 0;
}
static inline void wt_thread_join( wt_thread_t t ) { pthread_join( t, NULL ); }
/* Per-worker setup, run on the worker thread itself: a name for profilers and,
 * on Apple Silicon, the same QoS class as the main thread. A worker left at the
 * default class can land on an efficiency core while the owner thread spins on
 * it, which would make every measurement of a consumer look worse than it is. */
static inline void wt_thread_init_self( const char *name ) {
#  if defined( __APPLE__ )
	pthread_setname_np( name );
	pthread_set_qos_class_self_np( QOS_CLASS_USER_INTERACTIVE, 0 );
#  elif defined( __linux__ )
	pthread_setname_np( pthread_self(), name );
#  else
	(void)name;
#  endif
}
#endif

#endif
