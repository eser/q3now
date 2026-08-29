// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "wired/entity/metadata.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#define CHECK( expression ) do { if ( !( expression ) ) { \
	fprintf( stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #expression ); return 1; } } while ( 0 )

typedef struct {
	int count;
	float intensity;
	char *name;
	vec3_t origin;
	vec3_t angles;
} fixture_t;

static char interned[64];
static char *Intern( const char *value )
{
	if ( strlen( value ) >= sizeof( interned ) ) return NULL;
	strcpy( interned, value ); return interned;
}

#define META( id_, name_, alias_, member_, type_, parse_, persistence_, default_ ) \
	{ WIRED_METADATA_SCHEMA_VERSION, id_, name_, alias_, type_, parse_, persistence_, \
		WIRED_METADATA_INSPECT, offsetof( fixture_t, member_ ), sizeof( ((fixture_t *)0)->member_ ), default_ }

int main( void )
{
	const wiredMetadataField_t fields[] = {
		META( 1u, "count", NULL, count, WIRED_METADATA_INT, WIRED_METADATA_PARSE_STRICT,
			WIRED_METADATA_PERSIST_SAVE, "0" ),
		META( 2u, "intensity", "light", intensity, WIRED_METADATA_FLOAT,
			WIRED_METADATA_PARSE_LEGACY_PREFIX, WIRED_METADATA_PERSIST_NONE, "1" ),
		META( 3u, "name", NULL, name, WIRED_METADATA_STRING, WIRED_METADATA_PARSE_STRICT,
			WIRED_METADATA_PERSIST_SAVE, "" ),
		META( 4u, "origin", NULL, origin, WIRED_METADATA_VEC3, WIRED_METADATA_PARSE_STRICT,
			WIRED_METADATA_PERSIST_SAVE, "0 0 0" ),
		META( 5u, "angle", NULL, angles, WIRED_METADATA_ANGLE_YAW,
			WIRED_METADATA_PARSE_STRICT, WIRED_METADATA_PERSIST_SAVE, "0" )
	};
	wiredMetadataRegistry_t registry, exact;
	wiredMetadataField_t invalid[2] = { fields[0], fields[0] };
	fixture_t fixture, untouched;
	char inspected[64];
	memset( &fixture, 0, sizeof( fixture ) );
	CHECK( WiredMetadata_RegistryBuild( fields, 5u, &registry ) );
	CHECK( WiredMetadata_RegistryValid( &registry ) && registry.identityHash != 0u );
	exact = registry;
	CHECK( WiredMetadata_RegistryValid( &exact ) );
	exact.identityHash++;
	CHECK( !WiredMetadata_RegistryValid( &exact ) );
	CHECK( WiredMetadata_Find( &registry, "COUNT" ) == &fields[0] );
	CHECK( WiredMetadata_Find( &registry, "light" ) == &fields[1] );
	CHECK( WiredMetadata_Find( &registry, "missing" ) == NULL );
	CHECK( !WiredMetadata_RegistryBuild( invalid, 2u, &exact ) );
	CHECK( WiredMetadata_Parse( &fields[0], &fixture, "17", NULL ) && fixture.count == 17 );
	untouched = fixture;
	CHECK( !WiredMetadata_Parse( &fields[0], &fixture, "18junk", NULL ) );
	CHECK( !memcmp( &fixture, &untouched, sizeof( fixture ) ) );
	CHECK( WiredMetadata_Parse( &fields[1], &fixture, "2.5legacy", NULL ) && fixture.intensity == 2.5f );
	CHECK( WiredMetadata_Parse( &fields[2], &fixture, "lava", Intern ) && !strcmp( fixture.name, "lava" ) );
	CHECK( WiredMetadata_Parse( &fields[3], &fixture, "1 2 3", NULL ) && fixture.origin[2] == 3.0f );
	untouched = fixture;
	CHECK( !WiredMetadata_Parse( &fields[3], &fixture, "1 2", NULL ) );
	CHECK( !memcmp( &fixture, &untouched, sizeof( fixture ) ) );
	CHECK( WiredMetadata_Parse( &fields[4], &fixture, "90", NULL ) &&
		fixture.angles[0] == 0.0f && fixture.angles[1] == 90.0f && fixture.angles[2] == 0.0f );
	CHECK( WiredMetadata_Inspect( &fields[3], &fixture, inspected, sizeof( inspected ) ) &&
		!strcmp( inspected, "1 2 3" ) );
	puts( "wired entity metadata: PASS" );
	return 0;
}

#undef META
