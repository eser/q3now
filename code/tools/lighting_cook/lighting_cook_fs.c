// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "lighting_cook_fs.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

#if defined( _WIN32 )
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

static qboolean KeyValid( const char *key )
{
	const char *cursor;
	if ( !key || strncmp( key, "wrlight-v1-", 11u ) ) return qfalse;
	for ( cursor = key; *cursor; ++cursor )
		if ( !( ( *cursor >= 'a' && *cursor <= 'z' ) ||
			( *cursor >= '0' && *cursor <= '9' ) || *cursor == '-' ) )
			return qfalse;
	return cursor != key && (size_t)( cursor - key ) < RAL_LIGHTING_CACHE_KEY_TEXT_CAPACITY;
}

static qboolean Path( const wiredLightingCookFilesystem_t *filesystem,
	const char *key, const char *suffix, char out[WIRED_LIGHTING_COOK_FS_PATH_CAPACITY] )
{
	int length;
	if ( !filesystem || filesystem->ready != qtrue || !KeyValid( key ) || !suffix || !out )
		return qfalse;
	length = snprintf( out, WIRED_LIGHTING_COOK_FS_PATH_CAPACITY,
		"%s/%s%s", filesystem->root, key, suffix );
	return length > 0 && length < (int)WIRED_LIGHTING_COOK_FS_PATH_CAPACITY;
}

static qboolean WorldStemValid( const char *stem )
{
	const char *cursor;
	if ( !stem || !*stem ) return qfalse;
	for ( cursor = stem; *cursor; ++cursor )
		if ( !( ( *cursor >= 'a' && *cursor <= 'z' ) ||
			( *cursor >= 'A' && *cursor <= 'Z' ) ||
			( *cursor >= '0' && *cursor <= '9' ) || *cursor == '-' ||
			*cursor == '_' ) ) return qfalse;
	return (size_t)( cursor - stem ) < 256u;
}

static const char *SidecarSuffix( ralLightingArtifactKind_t kind )
{
	if ( kind == RAL_LIGHTING_ARTIFACT_DIRECTIONAL_LIGHTMAP ) return ".wlight";
	if ( kind == RAL_LIGHTING_ARTIFACT_IRRADIANCE_VOLUME ) return ".wprobe";
	return NULL;
}

static qboolean SidecarPath( const wiredLightingCookFilesystem_t *filesystem,
	const char *worldStem, ralLightingArtifactKind_t kind,
	char out[WIRED_LIGHTING_COOK_FS_PATH_CAPACITY] )
{
	const char *suffix = SidecarSuffix( kind );
	int length;
	if ( !filesystem || filesystem->ready != qtrue ||
		!WorldStemValid( worldStem ) || !suffix || !out ) return qfalse;
	length = snprintf( out, WIRED_LIGHTING_COOK_FS_PATH_CAPACITY,
		"%s/%s%s", filesystem->root, worldStem, suffix );
	return length > 0 && length < (int)WIRED_LIGHTING_COOK_FS_PATH_CAPACITY;
}

#if defined( _WIN32 )
static qboolean WriteFileAtomic( const char *temporary, const char *final,
	const void *bytes, uint64_t length )
{
	HANDLE file;
	const unsigned char *cursor = (const unsigned char *)bytes;
	uint64_t remaining = length;
	file = CreateFileA( temporary, GENERIC_WRITE, 0, NULL, CREATE_NEW,
		FILE_ATTRIBUTE_NORMAL, NULL );
	if ( file == INVALID_HANDLE_VALUE ) return qfalse;
	while ( remaining ) {
		DWORD written = 0u;
		DWORD chunk = remaining > UINT32_MAX ? UINT32_MAX : (DWORD)remaining;
		if ( !WriteFile( file, cursor, chunk, &written, NULL ) || written != chunk ) {
			CloseHandle( file ); DeleteFileA( temporary ); return qfalse;
		}
		cursor += written; remaining -= written;
	}
	if ( !FlushFileBuffers( file ) || !CloseHandle( file ) ||
		!MoveFileExA( temporary, final,
			MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH ) ) {
		DeleteFileA( temporary ); return qfalse;
	}
	return qtrue;
}
#else
static qboolean WriteFileAtomic( const char *temporary, const char *final,
	const void *bytes, uint64_t length )
{
	const unsigned char *cursor = (const unsigned char *)bytes;
	uint64_t remaining = length;
	int file = open( temporary, O_WRONLY | O_CREAT | O_EXCL, 0600 );
	if ( file < 0 ) return qfalse;
	while ( remaining ) {
		ssize_t written = write( file, cursor,
			remaining > (uint64_t)SSIZE_MAX ? (size_t)SSIZE_MAX : (size_t)remaining );
		if ( written <= 0 ) {
			close( file ); unlink( temporary ); return qfalse;
		}
		cursor += written; remaining -= (uint64_t)written;
	}
	if ( fsync( file ) || close( file ) || rename( temporary, final ) ) {
		unlink( temporary ); return qfalse;
	}
	return qtrue;
}
#endif

static qboolean WriteAtomic( void *context, const char *key,
	const void *bytes, uint64_t length )
{
	wiredLightingCookFilesystem_t *filesystem =
		(wiredLightingCookFilesystem_t *)context;
	char final[WIRED_LIGHTING_COOK_FS_PATH_CAPACITY];
	char temporary[WIRED_LIGHTING_COOK_FS_PATH_CAPACITY];
	char suffix[96];
	int suffixLength;
	if ( !filesystem || !bytes || !length ||
		!Path( filesystem, key, ".wrcache", final ) )
		return qfalse;
	filesystem->temporarySerial++;
#if defined( _WIN32 )
	suffixLength = snprintf( suffix, sizeof( suffix ), ".tmp-%lu-%llu",
		(unsigned long)GetCurrentProcessId(),
		(unsigned long long)filesystem->temporarySerial );
#else
	suffixLength = snprintf( suffix, sizeof( suffix ), ".tmp-%lu-%llu",
		(unsigned long)getpid(), (unsigned long long)filesystem->temporarySerial );
#endif
	if ( suffixLength <= 0 || suffixLength >= (int)sizeof( suffix ) ||
		!Path( filesystem, key, suffix, temporary ) )
		return qfalse;
	return WriteFileAtomic( temporary, final, bytes, length );
}

static qboolean Read( void *context, const char *key, void *bytes,
	uint64_t capacity, uint64_t *outLength )
{
	wiredLightingCookFilesystem_t *filesystem =
		(wiredLightingCookFilesystem_t *)context;
	char path[WIRED_LIGHTING_COOK_FS_PATH_CAPACITY];
	FILE *file;
	long length;
	if ( !filesystem || !bytes || !capacity || !outLength ||
		capacity > (uint64_t)SIZE_MAX || !Path( filesystem, key, ".wrcache", path ) )
		return qfalse;
	file = fopen( path, "rb" );
	if ( !file ) return qfalse;
	if ( fseek( file, 0, SEEK_END ) || ( length = ftell( file ) ) <= 0 ||
		(uint64_t)length > capacity || fseek( file, 0, SEEK_SET ) ||
		fread( bytes, 1u, (size_t)length, file ) != (size_t)length ) {
		fclose( file );
		return qfalse;
	}
	if ( fclose( file ) ) return qfalse;
	*outLength = (uint64_t)length;
	return qtrue;
}

qboolean WiredLightingCookFilesystem_Init(
	wiredLightingCookFilesystem_t *filesystem, const char *derivedRoot )
{
	size_t length;
	if ( !filesystem || !derivedRoot || !( length = strlen( derivedRoot ) ) ||
		length >= sizeof( filesystem->root ) || derivedRoot[length - 1u] == '/' ||
		derivedRoot[length - 1u] == '\\' )
		return qfalse;
#if defined( _WIN32 )
	{
		DWORD attributes = GetFileAttributesA( derivedRoot );
		if ( attributes == INVALID_FILE_ATTRIBUTES ||
			!( attributes & FILE_ATTRIBUTE_DIRECTORY ) ) return qfalse;
	}
#else
	{
		struct stat status;
		if ( stat( derivedRoot, &status ) || !S_ISDIR( status.st_mode ) ) return qfalse;
	}
#endif
	memset( filesystem, 0, sizeof( *filesystem ) );
	filesystem->schemaVersion = WIRED_LIGHTING_COOK_FS_SCHEMA_VERSION;
	memcpy( filesystem->root, derivedRoot, length + 1u );
	filesystem->ready = qtrue;
	return qtrue;
}

const ralLightingCacheOps_t *WiredLightingCookFilesystem_Ops( void )
{
	static const ralLightingCacheOps_t ops = { WriteAtomic, Read };
	return &ops;
}

qboolean WiredLightingCookFilesystem_PublishSidecar(
	wiredLightingCookFilesystem_t *filesystem, const char *worldStem,
	ralLightingArtifactKind_t kind, const void *bytes, uint64_t length )
{
	char final[WIRED_LIGHTING_COOK_FS_PATH_CAPACITY];
	char temporary[WIRED_LIGHTING_COOK_FS_PATH_CAPACITY];
	int temporaryLength;
	if ( !filesystem || !bytes || !length ||
		!SidecarPath( filesystem, worldStem, kind, final ) ) return qfalse;
	filesystem->temporarySerial++;
#if defined( _WIN32 )
	temporaryLength = snprintf( temporary, sizeof( temporary ), "%s.tmp-%lu-%llu",
		final, (unsigned long)GetCurrentProcessId(),
		(unsigned long long)filesystem->temporarySerial );
#else
	temporaryLength = snprintf( temporary, sizeof( temporary ), "%s.tmp-%lu-%llu",
		final, (unsigned long)getpid(),
		(unsigned long long)filesystem->temporarySerial );
#endif
	if ( temporaryLength <= 0 || temporaryLength >= (int)sizeof( temporary ) )
		return qfalse;
	return WriteFileAtomic( temporary, final, bytes, length );
}

qboolean WiredLightingCookFilesystem_ReadSidecar(
	const wiredLightingCookFilesystem_t *filesystem, const char *worldStem,
	ralLightingArtifactKind_t kind, void *bytes, uint64_t capacity,
	uint64_t *outLength )
{
	char path[WIRED_LIGHTING_COOK_FS_PATH_CAPACITY];
	FILE *file;
	long length;
	if ( !bytes || !capacity || !outLength || capacity > (uint64_t)SIZE_MAX ||
		!SidecarPath( filesystem, worldStem, kind, path ) ) return qfalse;
	file = fopen( path, "rb" );
	if ( !file ) return qfalse;
	if ( fseek( file, 0, SEEK_END ) || ( length = ftell( file ) ) <= 0 ||
		(uint64_t)length > capacity || fseek( file, 0, SEEK_SET ) ||
		fread( bytes, 1u, (size_t)length, file ) != (size_t)length ) {
		fclose( file ); return qfalse;
	}
	if ( fclose( file ) ) return qfalse;
	*outLength = (uint64_t)length;
	return qtrue;
}
