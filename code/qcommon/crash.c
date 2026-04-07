/*
===========================================================================
Copyright (C) 2017-2020 Gian 'myT' Schellenbaum (CNQ3 original)
Copyright (C) 2026 q3now project (C port and adaptation)

This file is part of q3now (derived from Quake III Arena source code and
Challenge Quake 3). It is free software released under the terms of the
GNU General Public License version 2 or (at your option) any later
version. See the file COPYING.txt for details.
===========================================================================
*/
// JSON-format crash report writer.
//
// When an unhandled fault arrives at the platform signal/exception layer,
// the platform code calls Crash_WriteReport(reason, address, module). This
// walks every known VM, collects engine identifiers and key cvars, and
// writes a self-contained crash_YYYYMMDD_HHMMSS.json into the base path.

#include "crash.h"
#include "vm_local.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

#if defined( _WIN32 )
#  include <windows.h>
#  include <direct.h>
#else
#  include <unistd.h>
#  include <sys/stat.h>
#  include <sys/types.h>
#endif

cvar_t *com_crashReport = NULL;

#define CRASH_VM_COUNT ( (int)VM_COUNT )

typedef struct {
	vm_t         *vm;
	unsigned int  crc32;
} crashVM_t;

typedef struct {
	crashVM_t vm[ CRASH_VM_COUNT ];
} crashState_t;

static crashState_t s_crash;

/*
========================
Crash VM state
========================
*/
static qboolean Crash_IsVMIndexValid( vmIndex_t vmIndex )
{
	return ( vmIndex >= 0 && vmIndex < CRASH_VM_COUNT ) ? qtrue : qfalse;
}

static const char *Crash_GetVMName( vmIndex_t vmIndex )
{
	switch ( vmIndex ) {
		case VM_CGAME: return "cgame";
		case VM_GAME:  return "game";
		default:       return "unknown";
	}
}

void Crash_SaveQVMPointer( vmIndex_t vmIndex, vm_t *vm )
{
	if ( Crash_IsVMIndexValid( vmIndex ) ) {
		s_crash.vm[ vmIndex ].vm = vm;
	}
}

void Crash_SaveQVMChecksum( vmIndex_t vmIndex, unsigned int crc32 )
{
	if ( Crash_IsVMIndexValid( vmIndex ) ) {
		s_crash.vm[ vmIndex ].crc32 = crc32;
	}
}

/*
========================
JSON writer (private mini-emitter)

We deliberately don't share JSON infrastructure with sw3z / json.h — the
crash writer must work even when memory is corrupted, so it only uses the
C standard library and a single FILE*.
========================
*/
static FILE *s_jsonFile;
static int   s_jsonIndent;
static int   s_jsonCommaStack[ 16 ];

static void JSON_Indent( void )
{
	int i;
	if ( !s_jsonFile ) return;
	for ( i = 0; i < s_jsonIndent; i++ ) {
		fputs( "\t", s_jsonFile );
	}
}

static void JSON_Comma( void )
{
	if ( !s_jsonFile ) return;
	if ( s_jsonIndent > 0 && s_jsonIndent < (int)( sizeof( s_jsonCommaStack ) / sizeof( s_jsonCommaStack[ 0 ] ) ) ) {
		if ( s_jsonCommaStack[ s_jsonIndent ] ) {
			fputs( ",\n", s_jsonFile );
		} else {
			fputs( "\n", s_jsonFile );
			s_jsonCommaStack[ s_jsonIndent ] = 1;
		}
	}
}

static void JSON_WriteEscapedString( const char *s )
{
	const char *p;
	if ( !s_jsonFile ) return;
	fputc( '"', s_jsonFile );
	if ( s ) {
		for ( p = s; *p; p++ ) {
			unsigned char c = (unsigned char)*p;
			switch ( c ) {
				case '"':  fputs( "\\\"", s_jsonFile ); break;
				case '\\': fputs( "\\\\", s_jsonFile ); break;
				case '\b': fputs( "\\b", s_jsonFile );  break;
				case '\f': fputs( "\\f", s_jsonFile );  break;
				case '\n': fputs( "\\n", s_jsonFile );  break;
				case '\r': fputs( "\\r", s_jsonFile );  break;
				case '\t': fputs( "\\t", s_jsonFile );  break;
				default:
					if ( c < 0x20 ) {
						fprintf( s_jsonFile, "\\u%04x", c );
					} else {
						fputc( (int)c, s_jsonFile );
					}
					break;
			}
		}
	}
	fputc( '"', s_jsonFile );
}

static void JSON_Begin( FILE *f )
{
	int i;
	s_jsonFile = f;
	s_jsonIndent = 0;
	for ( i = 0; i < (int)( sizeof( s_jsonCommaStack ) / sizeof( s_jsonCommaStack[ 0 ] ) ); i++ ) {
		s_jsonCommaStack[ i ] = 0;
	}
	fputs( "{", s_jsonFile );
	s_jsonIndent = 1;
}

static void JSON_End( void )
{
	if ( !s_jsonFile ) return;
	fputs( "\n}\n", s_jsonFile );
	s_jsonFile = NULL;
	s_jsonIndent = 0;
}

static void JSON_BeginNamedObject( const char *name )
{
	if ( !s_jsonFile ) return;
	JSON_Comma();
	JSON_Indent();
	JSON_WriteEscapedString( name );
	fputs( ": {", s_jsonFile );
	if ( s_jsonIndent + 1 < (int)( sizeof( s_jsonCommaStack ) / sizeof( s_jsonCommaStack[ 0 ] ) ) ) {
		s_jsonCommaStack[ s_jsonIndent + 1 ] = 0;
	}
	s_jsonIndent++;
}

static void JSON_EndObject( void )
{
	if ( !s_jsonFile ) return;
	fputs( "\n", s_jsonFile );
	s_jsonIndent--;
	JSON_Indent();
	fputs( "}", s_jsonFile );
}

static void JSON_BeginNamedArray( const char *name )
{
	if ( !s_jsonFile ) return;
	JSON_Comma();
	JSON_Indent();
	JSON_WriteEscapedString( name );
	fputs( ": [", s_jsonFile );
	if ( s_jsonIndent + 1 < (int)( sizeof( s_jsonCommaStack ) / sizeof( s_jsonCommaStack[ 0 ] ) ) ) {
		s_jsonCommaStack[ s_jsonIndent + 1 ] = 0;
	}
	s_jsonIndent++;
}

static void JSON_EndArray( void )
{
	if ( !s_jsonFile ) return;
	fputs( "\n", s_jsonFile );
	s_jsonIndent--;
	JSON_Indent();
	fputs( "]", s_jsonFile );
}

static void JSON_BeginArrayObject( void )
{
	if ( !s_jsonFile ) return;
	JSON_Comma();
	JSON_Indent();
	fputs( "{", s_jsonFile );
	if ( s_jsonIndent + 1 < (int)( sizeof( s_jsonCommaStack ) / sizeof( s_jsonCommaStack[ 0 ] ) ) ) {
		s_jsonCommaStack[ s_jsonIndent + 1 ] = 0;
	}
	s_jsonIndent++;
}

static void JSON_StringValue( const char *name, const char *value )
{
	if ( !s_jsonFile ) return;
	JSON_Comma();
	JSON_Indent();
	JSON_WriteEscapedString( name );
	fputs( ": ", s_jsonFile );
	JSON_WriteEscapedString( value ? value : "" );
}

static void JSON_IntegerValue( const char *name, long long value )
{
	if ( !s_jsonFile ) return;
	JSON_Comma();
	JSON_Indent();
	JSON_WriteEscapedString( name );
	fprintf( s_jsonFile, ": %lld", value );
}

static void JSON_HexValue( const char *name, unsigned long long value )
{
	if ( !s_jsonFile ) return;
	JSON_Comma();
	JSON_Indent();
	JSON_WriteEscapedString( name );
	fprintf( s_jsonFile, ": \"0x%llx\"", value );
}

static void JSON_BooleanValue( const char *name, qboolean value )
{
	if ( !s_jsonFile ) return;
	JSON_Comma();
	JSON_Indent();
	JSON_WriteEscapedString( name );
	fputs( value ? ": true" : ": false", s_jsonFile );
}

/*
========================
Crash report content
========================
*/
static void Crash_WriteVMs( void )
{
	int  i;
	char callStack[ 4096 ];

	JSON_BeginNamedArray( "vms" );
	for ( i = 0; i < CRASH_VM_COUNT; i++ ) {
		vm_t *vm = s_crash.vm[ i ].vm;
		unsigned int crc32 = s_crash.vm[ i ].crc32;

		if ( vm == NULL && crc32 == 0 ) {
			continue;
		}

		JSON_BeginArrayObject();
		JSON_IntegerValue( "index", i );
		JSON_StringValue( "name", Crash_GetVMName( (vmIndex_t)i ) );
		JSON_BooleanValue( "loaded", vm != NULL ? qtrue : qfalse );
		if ( crc32 ) {
			JSON_HexValue( "crc32", (unsigned long long)crc32 );
		}

		if ( vm != NULL ) {
			VM_GetCallStack( vm, callStack, (int)sizeof( callStack ) );
			JSON_StringValue( "call_stack", callStack );
			JSON_IntegerValue( "call_level", vm->callLevel );
#if FEAT_LEGACY_QVM
			JSON_IntegerValue( "instruction_count", vm->instructionCount );
#endif
			JSON_HexValue( "program_stack", (unsigned long long)(unsigned int)vm->programStack );
		}
		JSON_EndObject();
	}
	JSON_EndArray();
}

static void Crash_WriteCvar( const char *name )
{
	char value[ MAX_CVAR_VALUE_STRING ];
	Cvar_VariableStringBuffer( name, value, sizeof( value ) );
	JSON_StringValue( name, value );
}

static void Crash_WriteKeyCvars( void )
{
	JSON_BeginNamedObject( "cvars" );
	Crash_WriteCvar( "com_dedicated" );
	Crash_WriteCvar( "fs_game" );
	Crash_WriteCvar( "fs_basepath" );
	Crash_WriteCvar( "mapname" );
	Crash_WriteCvar( "r_mode" );
	Crash_WriteCvar( "r_customwidth" );
	Crash_WriteCvar( "r_customheight" );
	Crash_WriteCvar( "cl_renderer" );
	Crash_WriteCvar( "cl_running" );
	Crash_WriteCvar( "sv_running" );
	Crash_WriteCvar( "sv_maxclients" );
	Crash_WriteCvar( "com_gamename" );
	Crash_WriteCvar( "protocol" );
	JSON_EndObject();
}

static void Crash_WriteEngineInfo( const char *reason, const char *address, const char *module )
{
	char timestamp[ 64 ];
	qtime_t tm;

	Com_RealTime( &tm );
	Com_sprintf( timestamp, sizeof( timestamp ),
		"%04d-%02d-%02dT%02d:%02d:%02d",
		tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
		tm.tm_hour, tm.tm_min, tm.tm_sec );

	JSON_StringValue( "engine_version", Q3NOW_ENGINE_VERSION );
	JSON_StringValue( "engine_platform", PLATFORM_STRING );
	JSON_StringValue( "engine_arch", ARCH_STRING );
	JSON_StringValue( "engine_build_date", __DATE__ );
	JSON_StringValue( "engine_build_time", __TIME__ );
#ifdef DEDICATED
	JSON_BooleanValue( "engine_dedicated_server", qtrue );
#else
	JSON_BooleanValue( "engine_dedicated_server", qfalse );
#endif
#ifdef _DEBUG
	JSON_BooleanValue( "engine_debug_build", qtrue );
#else
	JSON_BooleanValue( "engine_debug_build", qfalse );
#endif
	JSON_StringValue( "timestamp", timestamp );
	JSON_StringValue( "reason", reason ? reason : "" );
	JSON_StringValue( "exception_address", address ? address : "" );
	JSON_StringValue( "exception_module", module ? module : "" );
}

/*
========================
Crash_Init
========================
*/
void Crash_Init( void )
{
	memset( &s_crash, 0, sizeof( s_crash ) );

	com_crashReport = Cvar_Get( "com_crashReport", "1", CVAR_ARCHIVE_ND );
	Cvar_CheckRange( com_crashReport, "0", "1", CV_INTEGER );
	Cvar_SetDescription( com_crashReport,
		"Write a JSON crash report (and minidump/backtrace) when the engine faults.\n"
		"0: disabled\n1: enabled (default)" );
}

/*
========================
Crash_WriteReport

Called from signal / exception handlers. Must not assume anything about
the integrity of the engine state beyond the basics.
========================
*/
void Crash_WriteReport( const char *reason, const char *address, const char *module )
{
	char     filename[ MAX_OSPATH ];
	char     fullpath[ MAX_OSPATH * 2 ];
	FILE    *file;
	qtime_t  tm;
	const char *basepath;

	// com_crashReport may be NULL if we crashed before Crash_Init ran.
	if ( com_crashReport != NULL && com_crashReport->integer == 0 ) {
		return;
	}

	Com_RealTime( &tm );
	Com_sprintf( filename, sizeof( filename ),
		"crash_%04d%02d%02d_%02d%02d%02d.json",
		tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
		tm.tm_hour, tm.tm_min, tm.tm_sec );

	basepath = Cvar_VariableString( "fs_basepath" );
	if ( basepath == NULL || basepath[ 0 ] == '\0' ) {
		Q_strncpyz( fullpath, filename, sizeof( fullpath ) );
	} else {
		Com_sprintf( fullpath, sizeof( fullpath ), "%s%c%s", basepath, PATH_SEP, filename );
	}

	file = fopen( fullpath, "wb" );
	if ( file == NULL ) {
		// Fallback: current working directory.
		file = fopen( filename, "wb" );
		if ( file == NULL ) {
			return;
		}
	}

	JSON_Begin( file );
	Crash_WriteEngineInfo( reason, address, module );
	Crash_WriteKeyCvars();
	Crash_WriteVMs();
	JSON_End();

	fflush( file );
	fclose( file );
}

/*
========================
Crash_InstallHandlers

Forwards to the per-platform handler installer. The Windows / POSIX
implementations call Crash_WriteReport() from their fault handler.
========================
*/
#if defined( _WIN32 )
extern void Sys_InstallCrashHandler( void );
#else
extern void Sys_InstallCrashHandler( void );
#endif

void Crash_InstallHandlers( void )
{
	Sys_InstallCrashHandler();
}

/*
========================
Crash_PrintVMStackTracesASS (POSIX only, async-signal-safe)

Used from inside the signal handler before we call any FILE* API. We
deliberately avoid malloc / printf here — only write(2) and fixed stack
buffers.
========================
*/
#if !defined( _WIN32 )

static void ASS_Write( int fd, const char *s )
{
	size_t len;
	if ( s == NULL || fd < 0 ) return;
	len = strlen( s );
	if ( write( fd, s, len ) < 0 ) {
		/* ignored: we're already crashing */
	}
}

static void ASS_WriteHex( int fd, unsigned long value )
{
	char buf[ 32 ];
	int  pos = (int)sizeof( buf );
	buf[ --pos ] = '\0';
	if ( value == 0 ) {
		buf[ --pos ] = '0';
	} else {
		while ( value != 0 && pos > 0 ) {
			unsigned digit = (unsigned)( value & 0xFu );
			buf[ --pos ] = (char)( digit < 10 ? ( '0' + digit ) : ( 'a' + digit - 10 ) );
			value >>= 4;
		}
	}
	ASS_Write( fd, "0x" );
	ASS_Write( fd, &buf[ pos ] );
}

void Crash_PrintVMStackTracesASS( int fd )
{
	int i;

	ASS_Write( fd, "VM state at signal:\r\n" );
	for ( i = 0; i < CRASH_VM_COUNT; i++ ) {
		vm_t *vm = s_crash.vm[ i ].vm;
		unsigned int crc32 = s_crash.vm[ i ].crc32;

		ASS_Write( fd, "  " );
		ASS_Write( fd, Crash_GetVMName( (vmIndex_t)i ) );
		ASS_Write( fd, ": " );
		if ( vm == NULL ) {
			ASS_Write( fd, "not loaded\r\n" );
			continue;
		}
		ASS_Write( fd, "crc32=" );
		ASS_WriteHex( fd, (unsigned long)crc32 );
		ASS_Write( fd, " pstack=" );
		ASS_WriteHex( fd, (unsigned long)(unsigned int)vm->programStack );
		ASS_Write( fd, " level=" );
		ASS_WriteHex( fd, (unsigned long)(unsigned int)vm->callLevel );
		ASS_Write( fd, "\r\n" );
	}
}

#endif // !_WIN32
