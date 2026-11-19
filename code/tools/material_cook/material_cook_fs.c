// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "material_cook_fs.h"

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

typedef enum {
	MATERIAL_COOK_FILE_INVALID = 0,
	MATERIAL_COOK_FILE_PRODUCT,
	MATERIAL_COOK_FILE_PACKAGE
} materialCookFileKind_t;

static materialCookFileKind_t KeyKind( uint64_t artifactHash, const char *key ) {
	char expected[RAL_MATERIAL_PACKAGE_KEY_TEXT_CAPACITY];
	if ( !artifactHash || !key ) return MATERIAL_COOK_FILE_INVALID;
	if ( Ral_MaterialCookCacheKeyText( artifactHash, expected,
			sizeof( expected ) ) && !strcmp( key, expected ) )
		return MATERIAL_COOK_FILE_PRODUCT;
	if ( Ral_MaterialPackageCacheKeyText( artifactHash, expected,
			sizeof( expected ) ) && !strcmp( key, expected ) )
		return MATERIAL_COOK_FILE_PACKAGE;
	return MATERIAL_COOK_FILE_INVALID;
}

static const char *Suffix( materialCookFileKind_t kind ) {
	if ( kind == MATERIAL_COOK_FILE_PRODUCT ) return ".wrmat";
	if ( kind == MATERIAL_COOK_FILE_PACKAGE ) return ".wrmatpkg";
	return NULL;
}

static qboolean Path( const wiredMaterialCookFilesystem_t *filesystem,
		uint64_t artifactHash, const char *key, char *out, size_t capacity ) {
	const char *suffix;
	int length;
	if ( !filesystem || filesystem->ready != qtrue || !out || !capacity
			|| !( suffix = Suffix( KeyKind( artifactHash, key ) ) ) ) return qfalse;
	length = snprintf( out, capacity, "%s/%s%s", filesystem->root, key, suffix );
	return length > 0 && (size_t)length < capacity ? qtrue : qfalse;
}

#if defined( _WIN32 )
static qboolean WriteFileAtomic( const char *temporary, const char *final,
		const void *bytes, uint64_t length ) {
	HANDLE file;
	const unsigned char *cursor = bytes;
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
	if ( !FlushFileBuffers( file ) || !CloseHandle( file )
			|| !MoveFileExA( temporary, final,
				MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH ) ) {
		DeleteFileA( temporary ); return qfalse;
	}
	return qtrue;
}
#else
static qboolean WriteFileAtomic( const char *temporary, const char *final,
		const void *bytes, uint64_t length ) {
	const unsigned char *cursor = bytes;
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

static qboolean WriteAtomic( void *opaque, uint64_t artifactHash,
		const char *key, const void *bytes, uint64_t length ) {
	wiredMaterialCookFilesystem_t *filesystem = opaque;
	char final[WIRED_MATERIAL_COOK_FS_PATH_CAPACITY];
	char temporary[WIRED_MATERIAL_COOK_FS_PATH_CAPACITY];
	unsigned long processId;
	if ( !bytes || !length || !Path( filesystem, artifactHash, key, final,
			sizeof( final ) ) ) return qfalse;
#if defined( _WIN32 )
	processId = (unsigned long)GetCurrentProcessId();
#else
	processId = (unsigned long)getpid();
#endif
	/* O_EXCL/CREATE_NEW makes each candidate private. A bounded suffix search
	 * avoids shared counters and their data races while retaining cross-process
	 * safety. Publication itself is one atomic replace after durable write. */
	for ( uint32_t attempt = 0u; attempt < 64u; ++attempt ) {
		int written = snprintf( temporary, sizeof( temporary ), "%s.tmp-%lu-%u",
			final, processId, attempt );
		if ( written <= 0 || written >= (int)sizeof( temporary ) ) return qfalse;
		if ( WriteFileAtomic( temporary, final, bytes, length ) ) return qtrue;
	}
	return qfalse;
}

static qboolean ProjectReferencePath(
		const wiredMaterialCookFilesystem_t *filesystem, const char *key,
		char *out, size_t capacity ) {
	int length;
	if ( !filesystem || filesystem->ready != qtrue || !key || !key[0]
			|| strlen( key ) != 33u
			|| strncmp( key, "wrmp-v1-", 8u )
			|| strspn( key + 8u, "0123456789abcdef" ) != 16u
			|| key[24] != '-'
			|| strspn( key + 25u, "0123456789abcdef" ) != 8u ) return qfalse;
	length = snprintf( out, capacity, "%s/%s.wrmatref", filesystem->root, key );
	return length > 0 && (size_t)length < capacity ? qtrue : qfalse;
}

static qboolean WriteProjectReferenceAtomic( void *opaque, const char *key,
		const void *bytes, uint64_t length ) {
	wiredMaterialCookFilesystem_t *filesystem = opaque;
	const ralMaterialProjectReferenceReceipt_t *receipt = bytes;
	char expected[RAL_MATERIAL_PROJECT_REFERENCE_KEY_TEXT_CAPACITY];
	char final[WIRED_MATERIAL_COOK_FS_PATH_CAPACITY];
	char temporary[WIRED_MATERIAL_COOK_FS_PATH_CAPACITY];
	unsigned long processId;
	if ( !bytes || length != sizeof( *receipt )
			|| !Ral_MaterialProjectReferenceReceiptValid( receipt )
			|| !Ral_MaterialProjectReferenceKeyText( receipt->mapName,
				receipt->mapChecksum, expected, sizeof( expected ) )
			|| strcmp( key, expected )
			|| !ProjectReferencePath( filesystem, key, final, sizeof( final ) ) )
		return qfalse;
#if defined( _WIN32 )
	processId = (unsigned long)GetCurrentProcessId();
#else
	processId = (unsigned long)getpid();
#endif
	for ( uint32_t attempt = 0u; attempt < 64u; ++attempt ) {
		int written = snprintf( temporary, sizeof( temporary ), "%s.tmp-%lu-%u",
			final, processId, attempt );
		if ( written <= 0 || written >= (int)sizeof( temporary ) ) return qfalse;
		if ( WriteFileAtomic( temporary, final, bytes, length ) ) return qtrue;
	}
	return qfalse;
}

static qboolean Read( void *opaque, uint64_t artifactHash, const char *key,
		void *bytes, uint64_t capacity, uint64_t *outLength ) {
	wiredMaterialCookFilesystem_t *filesystem = opaque;
	char path[WIRED_MATERIAL_COOK_FS_PATH_CAPACITY];
	FILE *file;
	long length;
	qboolean success = qfalse;
	if ( !bytes || !capacity || !outLength || capacity > (uint64_t)SIZE_MAX
			|| !Path( filesystem, artifactHash, key, path, sizeof( path ) ) )
		return qfalse;
	file = fopen( path, "rb" );
	if ( !file ) return qfalse;
	if ( !fseek( file, 0, SEEK_END ) && ( length = ftell( file ) ) > 0
			&& (uint64_t)length <= capacity && !fseek( file, 0, SEEK_SET )
			&& fread( bytes, 1u, (size_t)length, file ) == (size_t)length )
		success = qtrue;
	if ( fclose( file ) || !success ) return qfalse;
	*outLength = (uint64_t)length;
	return qtrue;
}

static qboolean ReadProjectReference( void *opaque, const char *key,
		void *bytes, uint64_t capacity, uint64_t *outLength ) {
	wiredMaterialCookFilesystem_t *filesystem = opaque;
	char path[WIRED_MATERIAL_COOK_FS_PATH_CAPACITY];
	FILE *file;
	long length;
	qboolean success = qfalse;
	if ( !bytes || !capacity || !outLength || capacity > (uint64_t)SIZE_MAX
			|| !ProjectReferencePath( filesystem, key, path, sizeof( path ) ) )
		return qfalse;
	file = fopen( path, "rb" );
	if ( !file ) return qfalse;
	if ( !fseek( file, 0, SEEK_END ) && ( length = ftell( file ) ) > 0
			&& (uint64_t)length <= capacity && !fseek( file, 0, SEEK_SET )
			&& fread( bytes, 1u, (size_t)length, file ) == (size_t)length )
		success = qtrue;
	if ( fclose( file ) || !success ) return qfalse;
	*outLength = (uint64_t)length;
	return qtrue;
}

qboolean WiredMaterialCookFilesystem_Init(
		wiredMaterialCookFilesystem_t *filesystem, const char *derivedRoot ) {
	size_t length;
	if ( !filesystem || !derivedRoot || !( length = strlen( derivedRoot ) )
			|| length >= sizeof( filesystem->root )
			|| derivedRoot[length - 1u] == '/' || derivedRoot[length - 1u] == '\\' )
		return qfalse;
#if defined( _WIN32 )
	{
		DWORD attributes = GetFileAttributesA( derivedRoot );
		if ( attributes == INVALID_FILE_ATTRIBUTES
				|| !( attributes & FILE_ATTRIBUTE_DIRECTORY ) ) return qfalse;
	}
#else
	{
		struct stat status;
		if ( stat( derivedRoot, &status ) || !S_ISDIR( status.st_mode ) )
			return qfalse;
	}
#endif
	memset( filesystem, 0, sizeof( *filesystem ) );
	filesystem->schemaVersion = WIRED_MATERIAL_COOK_FS_SCHEMA_VERSION;
	memcpy( filesystem->root, derivedRoot, length + 1u );
	filesystem->ready = qtrue;
	return qtrue;
}

const ralMaterialCookCacheOps_t *WiredMaterialCookFilesystem_MaterialOps( void ) {
	static const ralMaterialCookCacheOps_t ops = { WriteAtomic, Read };
	return &ops;
}

const ralMaterialPackageCacheOps_t *WiredMaterialCookFilesystem_PackageOps( void ) {
	static const ralMaterialPackageCacheOps_t ops = { WriteAtomic, Read };
	return &ops;
}

const ralMaterialProjectReferenceOps_t *
		WiredMaterialCookFilesystem_ProjectReferenceOps( void ) {
	static const ralMaterialProjectReferenceOps_t ops = {
		WriteProjectReferenceAtomic, ReadProjectReference
	};
	return &ops;
}
