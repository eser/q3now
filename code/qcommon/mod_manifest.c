// SPDX-License-Identifier: GPL-2.0-or-later

#include "mod_manifest.h"

#include <string.h>

#define JSON_IMPLEMENTATION
#include "json.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

static bool ManifestError( char *error, size_t errorSize, const char *format, ... ) {
	va_list args;
	if ( error && errorSize ) {
		va_start( args, format );
		vsnprintf( error, errorSize, format, args );
		va_end( args );
		error[errorSize - 1] = '\0';
	}
	return false;
}

static bool ManifestString( const char *object, const char *end, const char *key,
	char *out, size_t outSize, bool required, char *error, size_t errorSize ) {
	const char *value = JSON_ObjectGetNamedValue( object, end, key );
	unsigned int length;
	if ( !value ) {
		if ( required ) return ManifestError( error, errorSize, "missing %s", key );
		out[0] = '\0';
		return true;
	}
	if ( JSON_ValueGetType( value, end ) != JSONTYPE_STRING )
		return ManifestError( error, errorSize, "%s must be a string", key );
	length = JSON_ValueGetString( value, end, out, (unsigned int)outSize );
	if ( length == 0 || length > outSize )
		return ManifestError( error, errorSize, "%s is empty or too long", key );
	if ( strchr( out, '\\' ) )
		return ManifestError( error, errorSize, "%s uses unsupported escaping", key );
	return true;
}

static bool ManifestIdentity( const char *text ) {
	const unsigned char *p = (const unsigned char *)text;
	if ( !p[0] || !( ( p[0] >= 'a' && p[0] <= 'z' ) || ( p[0] >= '0' && p[0] <= '9' ) ) ) return false;
	for ( ++p; *p; ++p ) {
		if ( !( ( *p >= 'a' && *p <= 'z' ) || ( *p >= '0' && *p <= '9' )
			|| *p == '.' || *p == '_' || *p == '-' ) ) return false;
	}
	return true;
}

static bool ManifestHash( const char *text ) {
	size_t i;
	if ( strlen( text ) != 64 ) return false;
	for ( i = 0; i < 64; ++i )
		if ( !( ( text[i] >= '0' && text[i] <= '9' ) || ( text[i] >= 'a' && text[i] <= 'f' ) ) ) return false;
	return true;
}

static bool ManifestPath( const char *path ) {
	if ( !path[0] || path[0] == '/' || strchr( path, '\\' ) || strstr( path, "//" ) ) return false;
	if ( !strcmp( path, "." ) || !strcmp( path, ".." ) || !strncmp( path, "../", 3 )
		|| strstr( path, "/../" ) || strstr( path, "/./" ) ) return false;
	return true;
}

static wiredPackageRole_t ManifestRole( const char *name ) {
	static const char *const names[] = { "", "runtime", "toolchain", "server", "client", "shared", "cosmetic" };
	size_t i;
	for ( i = 1; i < sizeof( names ) / sizeof( names[0] ); ++i )
		if ( !strcmp( name, names[i] ) ) return (wiredPackageRole_t)i;
	return WIRED_PACKAGE_ROLE_INVALID;
}

static bool ManifestBool( const char *value, const char *end, bool *out ) {
	if ( !value || JSON_ValueGetType( value, end ) != JSONTYPE_VALUE ) return false;
	if ( (size_t)( end - value ) >= 4 && !strncmp( value, "true", 4 ) ) {
		*out = true;
		return true;
	}
	if ( (size_t)( end - value ) >= 5 && !strncmp( value, "false", 5 ) ) {
		*out = false;
		return true;
	}
	return false;
}

const char *WiredPackageRole_Name( wiredPackageRole_t role ) {
	static const char *const names[] = { "invalid", "runtime", "toolchain", "server", "client", "shared", "cosmetic" };
	return role >= WIRED_PACKAGE_ROLE_RUNTIME && role <= WIRED_PACKAGE_ROLE_COSMETIC
		? names[role] : names[0];
}

static bool ManifestStringArray( const char *object, const char *end, const char *key,
	char *storage, size_t stride, size_t capacity, size_t *count, bool required,
	bool identities, char *error, size_t errorSize ) {
	const char *array = JSON_ObjectGetNamedValue( object, end, key );
	const char *value;
	*count = 0;
	if ( !array || JSON_ValueGetType( array, end ) != JSONTYPE_ARRAY ) {
		if ( required ) return ManifestError( error, errorSize, "missing %s array", key );
		return true;
	}
	for ( value = JSON_ArrayGetFirstValue( array, end ); value; value = JSON_ArrayGetNextValue( value, end ) ) {
		char *slot;
		unsigned int length;
		if ( *count == capacity ) return ManifestError( error, errorSize, "%s exceeds runtime bound", key );
		if ( JSON_ValueGetType( value, end ) != JSONTYPE_STRING ) return ManifestError( error, errorSize, "%s entry must be a string", key );
		slot = storage + *count * stride;
		length = JSON_ValueGetString( value, end, slot, (unsigned int)stride );
		if ( length == 0 || length > stride || strchr( slot, '\\' ) ) return ManifestError( error, errorSize, "invalid %s entry", key );
		if ( identities && !ManifestIdentity( slot ) ) return ManifestError( error, errorSize, "invalid %s identity", key );
		for ( size_t i = 0; i < *count; ++i )
			if ( !strcmp( storage + i * stride, slot ) ) return ManifestError( error, errorSize, "duplicate %s entry", key );
		( *count )++;
	}
	if ( required && *count == 0 ) return ManifestError( error, errorSize, "%s cannot be empty", key );
	return true;
}

bool WiredPackageManifest_Parse( const char *json, size_t jsonLength,
	wiredPackageManifest_t *out, char *error, size_t errorSize ) {
	const char *end;
	const char *compat;
	const char *array;
	const char *value;
	char role[WIRED_PACKAGE_TARGET_MAX];
	if ( error && errorSize ) error[0] = '\0';
	if ( !json || !jsonLength || !out ) return ManifestError( error, errorSize, "invalid arguments" );
	end = json + jsonLength;
	if ( JSON_ValueGetType( json, end ) != JSONTYPE_OBJECT ) return ManifestError( error, errorSize, "manifest must be an object" );
	memset( out, 0, sizeof( *out ) );
	if ( !ManifestString( json, end, "schema", out->schema, sizeof( out->schema ), true, error, errorSize )
		|| strcmp( out->schema, WIRED_PACKAGE_SCHEMA_V1 ) ) return ManifestError( error, errorSize, "unsupported schema" );
	if ( !ManifestString( json, end, "id", out->id, sizeof( out->id ), true, error, errorSize ) || !ManifestIdentity( out->id ) ) return ManifestError( error, errorSize, "invalid id" );
	if ( !ManifestString( json, end, "version", out->version, sizeof( out->version ), true, error, errorSize ) ) return false;
	if ( !ManifestString( json, end, "owner", out->owner, sizeof( out->owner ), true, error, errorSize ) || !ManifestIdentity( out->owner ) ) return ManifestError( error, errorSize, "invalid owner" );
	if ( !ManifestString( json, end, "role", role, sizeof( role ), true, error, errorSize ) ) return false;
	out->role = ManifestRole( role );
	if ( out->role == WIRED_PACKAGE_ROLE_INVALID ) return ManifestError( error, errorSize, "invalid role" );

	compat = JSON_ObjectGetNamedValue( json, end, "compatibility" );
	if ( !compat || JSON_ValueGetType( compat, end ) != JSONTYPE_OBJECT ) return ManifestError( error, errorSize, "missing compatibility" );
	if ( !ManifestStringArray( compat, end, "os", &out->os[0][0], sizeof( out->os[0] ), WIRED_PACKAGE_TARGET_COUNT_MAX, &out->osCount, true, false, error, errorSize )
		|| !ManifestStringArray( compat, end, "arch", &out->arch[0][0], sizeof( out->arch[0] ), WIRED_PACKAGE_TARGET_COUNT_MAX, &out->archCount, true, false, error, errorSize )
		|| !ManifestString( compat, end, "abi", out->abi, sizeof( out->abi ), true, error, errorSize )
		|| !ManifestString( compat, end, "engineMin", out->engineMin, sizeof( out->engineMin ), false, error, errorSize )
		|| !ManifestString( compat, end, "engineMax", out->engineMax, sizeof( out->engineMax ), false, error, errorSize ) ) return false;

	if ( !ManifestStringArray( json, end, "provides", &out->provides[0][0], sizeof( out->provides[0] ), WIRED_PACKAGE_PROVIDE_MAX, &out->provideCount, false, true, error, errorSize ) ) return false;

	array = JSON_ObjectGetNamedValue( json, end, "depends" );
	if ( array ) {
		if ( JSON_ValueGetType( array, end ) != JSONTYPE_ARRAY ) return ManifestError( error, errorSize, "depends must be an array" );
		for ( value = JSON_ArrayGetFirstValue( array, end ); value; value = JSON_ArrayGetNextValue( value, end ) ) {
			wiredPackageDependency_t *dep;
			const char *optional;
			if ( out->dependencyCount == WIRED_PACKAGE_DEPENDENCY_MAX ) return ManifestError( error, errorSize, "depends exceeds runtime bound" );
			if ( JSON_ValueGetType( value, end ) != JSONTYPE_OBJECT ) return ManifestError( error, errorSize, "depends entry must be an object" );
			dep = &out->depends[out->dependencyCount];
			if ( !ManifestString( value, end, "id", dep->id, sizeof( dep->id ), true, error, errorSize ) || !ManifestIdentity( dep->id ) ) return ManifestError( error, errorSize, "invalid dependency id" );
			if ( !strcmp( dep->id, out->id ) ) return ManifestError( error, errorSize, "package depends on itself" );
			for ( size_t i = 0; i < out->dependencyCount; ++i )
				if ( !strcmp( out->depends[i].id, dep->id ) ) return ManifestError( error, errorSize, "duplicate dependency" );
			if ( !ManifestString( value, end, "version", dep->version, sizeof( dep->version ), false, error, errorSize ) ) return false;
			optional = JSON_ObjectGetNamedValue( value, end, "optional" );
			if ( optional && !ManifestBool( optional, end, &dep->optional ) ) return ManifestError( error, errorSize, "optional must be boolean" );
			out->dependencyCount++;
		}
	}

	array = JSON_ObjectGetNamedValue( json, end, "files" );
	if ( array ) {
		if ( JSON_ValueGetType( array, end ) != JSONTYPE_ARRAY ) return ManifestError( error, errorSize, "files must be an array" );
		for ( value = JSON_ArrayGetFirstValue( array, end ); value; value = JSON_ArrayGetNextValue( value, end ) ) {
			wiredPackageFile_t *file;
			const char *sizeValue;
			const char *sharedValue;
			char *sizeEnd;
			if ( out->fileCount == WIRED_PACKAGE_FILE_MAX ) return ManifestError( error, errorSize, "files exceeds runtime bound" );
			if ( JSON_ValueGetType( value, end ) != JSONTYPE_OBJECT ) return ManifestError( error, errorSize, "files entry must be an object" );
			file = &out->files[out->fileCount];
			if ( !ManifestString( value, end, "path", file->path, sizeof( file->path ), true, error, errorSize ) || !ManifestPath( file->path ) ) return ManifestError( error, errorSize, "unsafe file path" );
			for ( size_t i = 0; i < out->fileCount; ++i )
				if ( !strcmp( out->files[i].path, file->path ) ) return ManifestError( error, errorSize, "duplicate file path" );
			if ( !ManifestString( value, end, "sha256", file->sha256, sizeof( file->sha256 ), true, error, errorSize ) || !ManifestHash( file->sha256 ) ) return ManifestError( error, errorSize, "invalid file hash" );
			sizeValue = JSON_ObjectGetNamedValue( value, end, "size" );
			if ( !sizeValue || JSON_ValueGetType( sizeValue, end ) != JSONTYPE_VALUE || *sizeValue == '-' ) return ManifestError( error, errorSize, "invalid file size" );
			errno = 0;
			file->size = strtoull( sizeValue, &sizeEnd, 10 );
			if ( sizeEnd == sizeValue || errno == ERANGE || ( sizeEnd < end && *sizeEnd != ',' && *sizeEnd != '}' && *sizeEnd != ']' && *sizeEnd != ' ' && *sizeEnd != '\t' && *sizeEnd != '\r' && *sizeEnd != '\n' ) ) return ManifestError( error, errorSize, "invalid file size" );
			sharedValue = JSON_ObjectGetNamedValue( value, end, "shared" );
			if ( sharedValue && !ManifestBool( sharedValue, end, &file->shared ) ) return ManifestError( error, errorSize, "shared must be boolean" );
			out->fileCount++;
		}
	}
	return true;
}

static bool ManifestProvides( const wiredPackageManifest_t *manifest, const char *id ) {
	if ( !strcmp( manifest->id, id ) ) return true;
	for ( size_t i = 0; i < manifest->provideCount; ++i )
		if ( !strcmp( manifest->provides[i], id ) ) return true;
	return false;
}

static bool ManifestVisit( size_t index, const int edges[WIRED_PACKAGE_SET_MAX][WIRED_PACKAGE_DEPENDENCY_MAX],
	const size_t edgeCounts[WIRED_PACKAGE_SET_MAX], unsigned char state[WIRED_PACKAGE_SET_MAX],
	char *error, size_t errorSize ) {
	if ( state[index] == 2 ) return true;
	if ( state[index] == 1 ) return ManifestError( error, errorSize, "dependency cycle" );
	state[index] = 1;
	for ( size_t i = 0; i < edgeCounts[index]; ++i )
		if ( !ManifestVisit( (size_t)edges[index][i], edges, edgeCounts, state, error, errorSize ) ) return false;
	state[index] = 2;
	return true;
}

bool WiredPackageManifest_ValidateSet( const wiredPackageManifest_t *manifests,
	size_t manifestCount, char *error, size_t errorSize ) {
	int edges[WIRED_PACKAGE_SET_MAX][WIRED_PACKAGE_DEPENDENCY_MAX];
	size_t edgeCounts[WIRED_PACKAGE_SET_MAX] = { 0 };
	unsigned char state[WIRED_PACKAGE_SET_MAX] = { 0 };
	if ( error && errorSize ) error[0] = '\0';
	if ( !manifests || manifestCount == 0 || manifestCount > WIRED_PACKAGE_SET_MAX )
		return ManifestError( error, errorSize, "invalid manifest set size" );
	for ( size_t i = 0; i < manifestCount; ++i ) {
		for ( size_t j = i + 1; j < manifestCount; ++j )
			if ( !strcmp( manifests[i].id, manifests[j].id ) ) return ManifestError( error, errorSize, "duplicate package id" );
		for ( size_t f = 0; f < manifests[i].fileCount; ++f ) {
			for ( size_t j = 0; j < i; ++j ) for ( size_t g = 0; g < manifests[j].fileCount; ++g ) {
				if ( strcmp( manifests[i].files[f].path, manifests[j].files[g].path ) ) continue;
				if ( !manifests[i].files[f].shared || !manifests[j].files[g].shared
					|| strcmp( manifests[i].files[f].sha256, manifests[j].files[g].sha256 )
					|| manifests[i].files[f].size != manifests[j].files[g].size )
					return ManifestError( error, errorSize, "unsafe multi-owner file collision" );
			}
		}
	}
	for ( size_t i = 0; i < manifestCount; ++i ) {
		for ( size_t d = 0; d < manifests[i].dependencyCount; ++d ) {
			const wiredPackageDependency_t *dependency = &manifests[i].depends[d];
			int match = -1;
			for ( size_t j = 0; j < manifestCount; ++j ) {
				if ( !ManifestProvides( &manifests[j], dependency->id ) ) continue;
				if ( match >= 0 ) return ManifestError( error, errorSize, "ambiguous dependency provider" );
				match = (int)j;
			}
			if ( match < 0 ) {
				if ( dependency->optional ) continue;
				return ManifestError( error, errorSize, "missing dependency" );
			}
			if ( dependency->version[0] && strcmp( dependency->version, manifests[match].version ) )
				return ManifestError( error, errorSize, "dependency version mismatch" );
			edges[i][edgeCounts[i]++] = match;
		}
	}
	for ( size_t i = 0; i < manifestCount; ++i )
		if ( !ManifestVisit( i, edges, edgeCounts, state, error, errorSize ) ) return false;
	return true;
}
