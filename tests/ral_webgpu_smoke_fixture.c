// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "ral_webgpu_smoke_fixture.h"

#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *s_root;
static unsigned s_cookedReads;

static FILE *Open( const char *qpath ) {
	char path[4096];
	int length;
	if ( !s_root || !qpath || !qpath[0] || qpath[0] == '/'
			|| strchr( qpath, '\\' ) || strstr( qpath, ".." ) ) return NULL;
	length = snprintf( path, sizeof( path ), "%s/%s", s_root, qpath );
	if ( length <= 0 || (size_t)length >= sizeof( path ) ) return NULL;
	return fopen( path, "rb" );
}

static qboolean Resolve( const char *qpath, char *canonical, size_t capacity,
		uint64_t *sourceId, uint64_t *size, unsigned *generation ) {
	FILE *file;
	unsigned char bytes[4096];
	size_t count;
	uint64_t hash = UINT64_C(14695981039346656037), length = 0;
	if ( !canonical || !qpath || strlen( qpath ) >= capacity
			|| !( file = Open( qpath ) ) ) return qfalse;
	while ( ( count = fread( bytes, 1, sizeof( bytes ), file ) ) != 0 ) {
		for ( size_t i = 0; i < count; ++i )
			hash = ( hash ^ bytes[i] ) * UINT64_C(1099511628211);
		length += count;
	}
	qboolean ok = !ferror( file ) && length > 0 && length <= INT_MAX;
	if ( fclose( file ) != 0 ) ok = qfalse;
	if ( !ok ) return qfalse;
	memcpy( canonical, qpath, strlen( qpath ) + 1 );
	if ( sourceId ) *sourceId = hash ? hash : 1u;
	if ( size ) *size = length;
	if ( generation ) *generation = 1u;
	return qtrue;
}

static int Read( const char *qpath, void **out ) {
	FILE *file;
	long length;
	void *bytes;
	if ( out ) *out = NULL;
	if ( !( file = Open( qpath ) ) ) return -1;
	if ( fseek( file, 0, SEEK_END ) != 0 || ( length = ftell( file ) ) <= 0
			|| length > INT_MAX || fseek( file, 0, SEEK_SET ) != 0 ) {
		fclose( file ); return -1;
	}
	if ( !out ) { fclose( file ); return (int)length; }
	bytes = malloc( (size_t)length + 1u );
	if ( !bytes ) { fclose( file ); return -1; }
	qboolean ok = fread( bytes, 1, (size_t)length, file ) == (size_t)length;
	if ( fclose( file ) != 0 ) ok = qfalse;
	if ( !ok ) { free( bytes ); return -1; }
	((byte *)bytes)[length] = 0;
	*out = bytes;
	if ( !strncmp( qpath, "materials/cooked/", 17u ) ) ++s_cookedReads;
	return (int)length;
}

static void QDECL Log( log_severity_t severity, const char *format, ... ) {
	va_list args;
	(void)severity;
	va_start( args, format ); vfprintf( stderr, format, args ); va_end( args );
}

static int LogChannel( const char *name ) { (void)name; return 0; }
static void QDECL LogCh( int channel, log_severity_t severity, const char *format, ... ) {
	va_list args;
	(void)channel; (void)severity;
	va_start( args, format ); vfprintf( stderr, format, args ); va_end( args );
}

void WiredWebGpuSmokeFixture_Init( refimport_t *imports, const char *root ) {
	s_root = root;
	s_cookedReads = 0;
	memset( imports, 0, sizeof( *imports ) );
	imports->Malloc = malloc;
	imports->Free = free;
	imports->FS_ReadFile = Read;
	imports->FS_FreeFile = free;
	imports->FS_ResolveResource = Resolve;
	imports->Log = Log;
	imports->GetLogChannel = LogChannel;
	imports->LogCh = LogCh;
}

unsigned WiredWebGpuSmokeFixture_CookedReads( void ) { return s_cookedReads; }
