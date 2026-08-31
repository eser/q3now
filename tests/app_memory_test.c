// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

/*
 * TASK-96 ownership contract: resetting one app's Level lifetime must neither
 * invalidate its App lifetime nor touch another app.  This compiles the real
 * arena and app-memory implementations; the small stubs only detach those
 * translation units from the process-wide logger/error machinery.
 */

#include "app_memory.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;

#define CHECK(expr) do { \
	if ( !(expr) ) { \
		fprintf( stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr ); \
		failures++; \
	} \
} while ( 0 )

void Q_strncpyz( char *dest, const char *src, int destsize ) {
	if ( !dest || !src || destsize < 1 ) return;
	while ( --destsize > 0 && ( *dest++ = *src++ ) != '\0' )
		;
	*dest = '\0';
}

int QDECL Com_sprintf( char *dest, int size, const char *fmt, ... ) {
	int result;
	va_list ap;
	va_start( ap, fmt );
	result = vsnprintf( dest, (size_t)size, fmt, ap );
	va_end( ap );
	if ( size > 0 ) dest[size - 1] = '\0';
	return result;
}

void QDECL Com_Terminate( terminationReason_t level, const char *fmt, ... ) {
	(void)level;
	(void)fmt;
	abort();
}

int Log_GetChannel( const char *name ) {
	(void)name;
	return 0;
}

void Com_Log_Impl( log_severity_t severity, int channel, const char *fmt, ... ) {
	(void)severity;
	(void)channel;
	(void)fmt;
}

int main( void ) {
	appMemory_t first = { 0 };
	appMemory_t second = { 0 };
	uint32_t *firstApp;
	uint32_t *firstLevel;
	uint32_t *secondLevel;
	void *reused;

	AppMemory_Init( &first, "first", 4096, 4096 );
	AppMemory_Init( &second, "second", 4096, 4096 );
	CHECK( AppMemory_IsInitialized( &first ) );
	CHECK( AppMemory_IsInitialized( &second ) );
	CHECK( AppMemory_LevelGeneration( &first ) == 1 );

	firstApp = App_AllocType( &first, uint32_t );
	firstLevel = Level_AllocType( &first, uint32_t );
	secondLevel = Level_AllocType( &second, uint32_t );
	*firstApp = 0xA11F00D;
	*firstLevel = 0x1E7E1;
	*secondLevel = 0x5EC0AD;

	Level_Reset( &first );
	CHECK( AppMemory_LevelGeneration( &first ) == 2 );
	CHECK( *firstApp == 0xA11F00D );
	CHECK( *secondLevel == 0x5EC0AD );
	CHECK( Arena_Used( first.appArena ) >= sizeof( *firstApp ) );
	CHECK( Arena_Used( first.levelArena ) == 0 );
	CHECK( Arena_Used( second.levelArena ) >= sizeof( *secondLevel ) );

	reused = Level_AllocType( &first, uint32_t );
	CHECK( reused == firstLevel );
	CHECK( ( (uintptr_t)reused % _Alignof( uint32_t ) ) == 0 );

	AppMemory_Destroy( &first );
	CHECK( !AppMemory_IsInitialized( &first ) );
	CHECK( AppMemory_LevelGeneration( &first ) == 0 );
	CHECK( *secondLevel == 0x5EC0AD );
	AppMemory_Destroy( &second );

	if ( failures ) return 1;
	puts( "app memory ownership contract: PASS" );
	return 0;
}
