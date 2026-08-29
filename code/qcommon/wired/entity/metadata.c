// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "metadata.h"

#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FNV_OFFSET UINT64_C( 14695981039346656037 )
#define FNV_PRIME UINT64_C( 1099511628211 )

static uint64_t HashBytes( uint64_t hash, const void *memory, size_t length )
{
	const uint8_t *bytes = (const uint8_t *)memory;
	for ( size_t index = 0u; index < length; ++index )
		hash = ( hash ^ bytes[index] ) * FNV_PRIME;
	return hash;
}

static uint64_t HashString( uint64_t hash, const char *value )
{
	return HashBytes( hash, value, strlen( value ) + 1u );
}

static qboolean NameEqual( const char *a, const char *b )
{
	if ( !a || !b )
		return qfalse;
	while ( *a && *b ) {
		unsigned char ca = (unsigned char)*a++, cb = (unsigned char)*b++;
		if ( ca >= 'A' && ca <= 'Z' ) ca = (unsigned char)( ca - 'A' + 'a' );
		if ( cb >= 'A' && cb <= 'Z' ) cb = (unsigned char)( cb - 'A' + 'a' );
		if ( ca != cb ) return qfalse;
	}
	return *a == *b ? qtrue : qfalse;
}

static qboolean FieldValid( const wiredMetadataField_t *field )
{
	size_t expected;
	if ( !field || field->schemaVersion != WIRED_METADATA_SCHEMA_VERSION || !field->stableId ||
		 !field->name || !field->name[0] || !field->defaultValue ||
		 field->valueType < WIRED_METADATA_INT || field->valueType > WIRED_METADATA_ANGLE_YAW ||
		 field->parsePolicy < WIRED_METADATA_PARSE_STRICT ||
		 field->parsePolicy > WIRED_METADATA_PARSE_LEGACY_PREFIX ||
		 field->persistence < WIRED_METADATA_PERSIST_NONE ||
		 field->persistence > WIRED_METADATA_PERSIST_CROSS_LEVEL ||
		 ( field->policyFlags & ~( WIRED_METADATA_INSPECT | WIRED_METADATA_EVENT_READ |
			WIRED_METADATA_EVENT_WRITE ) ) )
		return qfalse;
	expected = field->valueType == WIRED_METADATA_INT ? sizeof( int ) :
		field->valueType == WIRED_METADATA_FLOAT ? sizeof( float ) :
		field->valueType == WIRED_METADATA_STRING ? sizeof( char * ) : sizeof( vec3_t );
	return field->byteSize == expected ? qtrue : qfalse;
}

qboolean WiredMetadata_RegistryBuild( const wiredMetadataField_t *fields,
	uint32_t fieldCount, wiredMetadataRegistry_t *outRegistry )
{
	wiredMetadataRegistry_t registry;
	uint64_t hash = FNV_OFFSET;
	uint32_t index, prior;
	if ( !fields || !fieldCount || !outRegistry )
		return qfalse;
	for ( index = 0u; index < fieldCount; ++index ) {
		const wiredMetadataField_t *field = &fields[index];
		if ( !FieldValid( field ) || ( index && fields[index - 1u].stableId >= field->stableId ) )
			return qfalse;
		for ( prior = 0u; prior < index; ++prior ) {
			if ( NameEqual( fields[prior].name, field->name ) ||
				( field->alias && ( NameEqual( fields[prior].name, field->alias ) ||
				  ( fields[prior].alias && NameEqual( fields[prior].alias, field->alias ) ) ) ) ||
				( fields[prior].alias && NameEqual( fields[prior].alias, field->name ) ) )
				return qfalse;
		}
		hash = HashBytes( hash, &field->stableId, sizeof( field->stableId ) );
		hash = HashString( hash, field->name );
		hash = HashString( hash, field->alias ? field->alias : "" );
		hash = HashBytes( hash, &field->valueType, sizeof( field->valueType ) );
		hash = HashBytes( hash, &field->parsePolicy, sizeof( field->parsePolicy ) );
		hash = HashBytes( hash, &field->persistence, sizeof( field->persistence ) );
		hash = HashBytes( hash, &field->policyFlags, sizeof( field->policyFlags ) );
		hash = HashString( hash, field->defaultValue );
	}
	memset( &registry, 0, sizeof( registry ) );
	registry.schemaVersion = WIRED_METADATA_REGISTRY_SCHEMA_VERSION;
	registry.fields = fields;
	registry.fieldCount = fieldCount;
	registry.identityHash = hash ? hash : 1u;
	*outRegistry = registry;
	return qtrue;
}

qboolean WiredMetadata_RegistryValid( const wiredMetadataRegistry_t *registry )
{
	wiredMetadataRegistry_t exact;
	return registry && registry->schemaVersion == WIRED_METADATA_REGISTRY_SCHEMA_VERSION &&
		WiredMetadata_RegistryBuild( registry->fields, registry->fieldCount, &exact ) &&
		exact.identityHash == registry->identityHash ? qtrue : qfalse;
}

const wiredMetadataField_t *WiredMetadata_Find(
	const wiredMetadataRegistry_t *registry, const char *name )
{
	uint32_t index;
	if ( !WiredMetadata_RegistryValid( registry ) || !name || !name[0] )
		return NULL;
	for ( index = 0u; index < registry->fieldCount; ++index )
		if ( NameEqual( registry->fields[index].name, name ) ||
			( registry->fields[index].alias && NameEqual( registry->fields[index].alias, name ) ) )
			return &registry->fields[index];
	return NULL;
}

static qboolean TailValid( const char *tail, wiredMetadataParsePolicy_t policy )
{
	if ( policy == WIRED_METADATA_PARSE_LEGACY_PREFIX )
		return qtrue;
	while ( tail && *tail && (unsigned char)*tail <= ' ' )
		tail++;
	return tail && !*tail ? qtrue : qfalse;
}

qboolean WiredMetadata_Parse( const wiredMetadataField_t *field, void *base,
	const char *value, wiredMetadataInternFn intern )
{
	uint8_t *destination;
	char *tail = NULL;
	if ( !FieldValid( field ) || !base || !value )
		return qfalse;
	destination = (uint8_t *)base + field->offset;
	switch ( field->valueType ) {
	case WIRED_METADATA_INT: {
		long parsed;
		errno = 0;
		parsed = strtol( value, &tail, 10 );
		if ( tail == value || errno == ERANGE || parsed < INT_MIN || parsed > INT_MAX ||
			 !TailValid( tail, field->parsePolicy ) )
			return qfalse;
		*(int *)destination = (int)parsed;
		return qtrue;
	}
	case WIRED_METADATA_FLOAT: {
		float parsed;
		errno = 0;
		parsed = strtof( value, &tail );
		if ( tail == value || errno == ERANGE || !isfinite( parsed ) ||
			 !TailValid( tail, field->parsePolicy ) )
			return qfalse;
		*(float *)destination = parsed;
		return qtrue;
	}
	case WIRED_METADATA_STRING: {
		char *stored;
		if ( !intern || !( stored = intern( value ) ) )
			return qfalse;
		memcpy( destination, &stored, sizeof( stored ) );
		return qtrue;
	}
	case WIRED_METADATA_VEC3: {
		float parsed[3];
		char trailing;
		int count = sscanf( value, " %f %f %f %c", &parsed[0], &parsed[1], &parsed[2], &trailing );
		if ( count != 3 || !isfinite( parsed[0] ) || !isfinite( parsed[1] ) || !isfinite( parsed[2] ) )
			return qfalse;
		memcpy( destination, parsed, sizeof( parsed ) );
		return qtrue;
	}
	case WIRED_METADATA_ANGLE_YAW: {
		float parsed;
		errno = 0;
		parsed = strtof( value, &tail );
		if ( tail == value || errno == ERANGE || !isfinite( parsed ) ||
			 !TailValid( tail, field->parsePolicy ) )
			return qfalse;
		((float *)destination)[0] = 0.0f;
		((float *)destination)[1] = parsed;
		((float *)destination)[2] = 0.0f;
		return qtrue;
	}
	default:
		return qfalse;
	}
}

qboolean WiredMetadata_Inspect( const wiredMetadataField_t *field,
	const void *base, char *destination, size_t capacity )
{
	const uint8_t *source;
	int written;
	if ( !FieldValid( field ) || !( field->policyFlags & WIRED_METADATA_INSPECT ) ||
		 !base || !destination || !capacity )
		return qfalse;
	source = (const uint8_t *)base + field->offset;
	switch ( field->valueType ) {
	case WIRED_METADATA_INT:
		written = snprintf( destination, capacity, "%d", *(const int *)source ); break;
	case WIRED_METADATA_FLOAT:
		written = snprintf( destination, capacity, "%.9g", *(const float *)source ); break;
	case WIRED_METADATA_STRING: {
		const char *value = *(const char *const *)source;
		written = snprintf( destination, capacity, "%s", value ? value : "" ); break;
	}
	case WIRED_METADATA_VEC3:
	case WIRED_METADATA_ANGLE_YAW:
		written = snprintf( destination, capacity, "%.9g %.9g %.9g",
			((const float *)source)[0], ((const float *)source)[1], ((const float *)source)[2] ); break;
	default:
		return qfalse;
	}
	return written >= 0 && (size_t)written < capacity ? qtrue : qfalse;
}
