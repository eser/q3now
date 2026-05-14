// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 1999-2005 Id Software, Inc.
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"
#include "win_local.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <direct.h>
#include <io.h>
#include <conio.h>
#include <intrin.h>
/* Phase 5: log channels */
LOG_DECLARE_CHANNEL( ch_system, "system" );

/* Sys_Milliseconds → wired/core/time/time.c */

/*
================
Sys_RandomBytes
================
*/
qboolean Sys_RandomBytes( byte *string, int len )
{
	HCRYPTPROV  prov;

	if( !CryptAcquireContext( &prov, NULL, NULL,
		PROV_RSA_FULL, CRYPT_VERIFYCONTEXT ) )  {

		return qfalse;
	}

	if( !CryptGenRandom( prov, len, (BYTE *)string ) )  {
		CryptReleaseContext( prov, 0 );
		return qfalse;
	}
	CryptReleaseContext( prov, 0 );
	return qtrue;
}


#ifdef UNICODE
LPWSTR AtoW( const char *s )
{
	static WCHAR buffer[MAXPRINTMSG*2];
	MultiByteToWideChar( CP_ACP, 0, s, strlen( s ) + 1, (LPWSTR) buffer, ARRAYSIZE( buffer ) );
	return buffer;
}

const char *WtoA( const LPWSTR s )
{
	static char buffer[MAXPRINTMSG*2];
	WideCharToMultiByte( CP_ACP, 0, s, -1, buffer, ARRAYSIZE( buffer ), NULL, NULL );
	return buffer;
}
#endif


/*
================
Sys_DefaultHomePath
================
*/
const char *Sys_DefaultHomePath( void )
{
	static char path[MAX_OSPATH];
	const char *userProfile;

	if ( *path )
		return path;

	userProfile = getenv( "USERPROFILE" );
	if ( userProfile == NULL || *userProfile == '\0' )
	{
		Com_Log( SEV_INFO, LOG_CH(ch_system), "Unable to detect USERPROFILE\n" );
		return NULL;
	}

	// Engine root <userprofile>{SEP}wired{SEP} — shared across every game built
	// on the wired engine. PATH_SEP comes from q_platform.h ('\\' on Windows,
	// '/' on Unix); using it instead of literal separators keeps the source
	// portable across platform shims even though this file only compiles on
	// Windows.
	{
		char engineRoot[MAX_OSPATH];
		Com_sprintf( engineRoot, sizeof( engineRoot ),
			"%s%c%s", userProfile, PATH_SEP, "wired" );
		if ( !CreateDirectory( engineRoot, NULL ) && GetLastError() != ERROR_ALREADY_EXISTS )
		{
			Com_Log( SEV_INFO, LOG_CH(ch_system), "Unable to create directory \"%s\"\n", engineRoot );
			return NULL;
		}
	}

	// Per-product subfolder <userprofile>{SEP}wired{SEP}<PRODUCT_NAME><CHANNEL_SUFFIX>{SEP}
	Com_sprintf( path, sizeof( path ),
		"%s%c%s%c%s%s", userProfile, PATH_SEP, "wired", PATH_SEP,
		PRODUCT_NAME, CHANNEL_SUFFIX );

	if ( !CreateDirectory( path, NULL ) )
	{
		if ( GetLastError() != ERROR_ALREADY_EXISTS )
		{
			Com_Log( SEV_INFO, LOG_CH(ch_system), "Unable to create directory \"%s\"\n", path );
			return NULL;
		}
	}

	return path;
}


/*
================
Sys_SetAffinityMask
================
*/
#ifdef USE_AFFINITY_MASK
static HANDLE hCurrentProcess = 0;

uint64_t Sys_GetAffinityMask( void )
{
	DWORD_PTR dwProcessAffinityMask;
	DWORD_PTR dwSystemAffinityMask;

	if ( hCurrentProcess == 0 )	{
		hCurrentProcess = GetCurrentProcess();
	}

	if ( GetProcessAffinityMask( hCurrentProcess, &dwProcessAffinityMask, &dwSystemAffinityMask ) )	{
		return (uint64_t)dwProcessAffinityMask;
	}

	return 0;
}


qboolean Sys_SetAffinityMask( const uint64_t mask )
{
	DWORD_PTR dwProcessAffinityMask = (DWORD_PTR)mask;

	if ( hCurrentProcess == 0 ) {
		hCurrentProcess = GetCurrentProcess();
	}

	if ( SetProcessAffinityMask( hCurrentProcess, dwProcessAffinityMask ) )	{
		//Sleep( 0 );
		return qtrue;
	}

	return qfalse;
}
#endif // USE_AFFINITY_MASK

/* Apple has a real implementation in code/macosx/macosx_main.c (mach
 * THREAD_TIME_CONSTRAINT_POLICY). Windows has no direct equivalent for the
 * "real-time-ish, low-jitter main thread" semantics that policy provides;
 * SetThreadPriority(THREAD_PRIORITY_TIME_CRITICAL) is too aggressive and
 * disrupts the rest of the system. Leaving as a no-op here matches the
 * Linux/BSD default. */
void Sys_SetMainThreadPolicy( void ) { }
