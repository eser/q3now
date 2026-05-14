// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2017-2020 Gian 'myT' Schellenbaum (CNQ3 original)
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
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

void Crash_SaveVMPointer( vmIndex_t vmIndex, vm_t *vm )
{
	if ( Crash_IsVMIndexValid( vmIndex ) ) {
		s_crash.vm[ vmIndex ].vm = vm;
	}
}

void Crash_SaveVMChecksum( vmIndex_t vmIndex, unsigned int crc32 )
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
	if ( !s_jsonFile ) return;
	for ( int i = 0; i < s_jsonIndent; i++ ) {
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
	if ( !s_jsonFile ) return;
	fputc( '"', s_jsonFile );
	if ( s ) {
		for ( const char *p = s; *p; p++ ) {
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
	s_jsonFile = f;
	s_jsonIndent = 0;
	for ( int i = 0; i < (int)( sizeof( s_jsonCommaStack ) / sizeof( s_jsonCommaStack[ 0 ] ) ); i++ ) {
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
	JSON_BeginNamedArray( "vms" );
	for ( int i = 0; i < CRASH_VM_COUNT; i++ ) {
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
			char callStack[ 4096 ];
			VM_GetCallStack( vm, callStack, (int)sizeof( callStack ) );
			JSON_StringValue( "call_stack", callStack );
			JSON_IntegerValue( "call_level", vm->callLevel );
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
	Crash_WriteCvar( "fs_installpath" );
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

	JSON_StringValue( "engine_version", WIRED_ENGINE_VERSION );
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

	{
		static const cvarDesc_t d = CVAR_BOOL( "com_crashReport", "1", CVAR_ARCHIVE | CVAR_NODEFAULT,
			"Write a JSON crash report (and minidump/backtrace) when the engine faults.\n"
			"0: disabled\n1: enabled (default)" );
		com_crashReport = Cvar_Register( &d );
	}
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
	// com_crashReport may be NULL if we crashed before Crash_Init ran.
	if ( com_crashReport != NULL && com_crashReport->integer == 0 ) {
		return;
	}

	qtime_t tm;
	Com_RealTime( &tm );
	char filename[ MAX_OSPATH ];
	Com_sprintf( filename, sizeof( filename ),
		"crash_%04d%02d%02d_%02d%02d%02d.json",
		tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
		tm.tm_hour, tm.tm_min, tm.tm_sec );

	/* Crash reports go to fs_homepath — a writable user-data location
	 * on every platform. fs_installpath was wrong for macOS (.app interior
	 * is read-only for system installs) and the cwd fallback wrote to
	 * an unpredictable location. If homepath is unavailable / unwritable
	 * the engine has bigger problems; we drop the report rather than
	 * scattering files. */
	const char *homepath = Cvar_VariableString( "fs_homepath" );
	if ( homepath == NULL || homepath[ 0 ] == '\0' ) {
		return;
	}

	char fullpath[ MAX_OSPATH * 2 ];
	Com_sprintf( fullpath, sizeof( fullpath ), "%s%c%s", homepath, PATH_SEP, filename );

	FILE *file = fopen( fullpath, "wb" );
	if ( file == NULL ) {
		return;
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
	if ( s == NULL || fd < 0 ) return;
	size_t len = strlen( s );
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
	ASS_Write( fd, "VM state at signal:\r\n" );
	for ( int i = 0; i < CRASH_VM_COUNT; i++ ) {
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
