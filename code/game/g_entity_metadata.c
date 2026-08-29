// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 1999-2005 Id Software, Inc.
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "g_local.h"
#include "g_entity_metadata.h"
#include "g_save.h"

LOG_DECLARE_CHANNEL( ch_game, "game" );

#define FIELD( id_, name_, member_, type_, default_ ) \
	{ WIRED_METADATA_SCHEMA_VERSION, id_, name_, NULL, type_, WIRED_METADATA_PARSE_LEGACY_PREFIX, \
		WIRED_METADATA_PERSIST_SAVE, WIRED_METADATA_INSPECT, FOFS( member_ ), \
		type_ == WIRED_METADATA_INT ? sizeof( int ) : \
		type_ == WIRED_METADATA_FLOAT ? sizeof( float ) : \
		type_ == WIRED_METADATA_STRING ? sizeof( char * ) : sizeof( vec3_t ), default_ }

static const wiredMetadataField_t entityFields[] = {
	FIELD( 1u, "classname", classname, WIRED_METADATA_STRING, "" ),
	FIELD( 2u, "origin", s.origin, WIRED_METADATA_VEC3, "0 0 0" ),
	FIELD( 3u, "model", model, WIRED_METADATA_STRING, "" ),
	FIELD( 4u, "model2", model2, WIRED_METADATA_STRING, "" ),
	FIELD( 5u, "spawnflags", spawnflags, WIRED_METADATA_INT, "0" ),
	FIELD( 6u, "speed", speed, WIRED_METADATA_FLOAT, "0" ),
	FIELD( 7u, "target", target, WIRED_METADATA_STRING, "" ),
	FIELD( 8u, "target2", target2, WIRED_METADATA_STRING, "" ),
	FIELD( 9u, "targetname", targetname, WIRED_METADATA_STRING, "" ),
	FIELD( 10u, "message", message, WIRED_METADATA_STRING, "" ),
	FIELD( 11u, "team", team, WIRED_METADATA_STRING, "" ),
	FIELD( 12u, "wait", wait, WIRED_METADATA_FLOAT, "0" ),
	FIELD( 13u, "random", random, WIRED_METADATA_FLOAT, "0" ),
	FIELD( 14u, "count", count, WIRED_METADATA_INT, "0" ),
	FIELD( 15u, "health", health, WIRED_METADATA_INT, "0" ),
	FIELD( 16u, "armor", armor, WIRED_METADATA_INT, "0" ),
	FIELD( 17u, "dmg", damage, WIRED_METADATA_INT, "0" ),
	FIELD( 18u, "angles", s.angles, WIRED_METADATA_VEC3, "0 0 0" ),
	FIELD( 19u, "angle", s.angles, WIRED_METADATA_ANGLE_YAW, "0" ),
	FIELD( 20u, "targetShaderName", targetShaderName, WIRED_METADATA_STRING, "" ),
	FIELD( 21u, "targetShaderNewName", targetShaderNewName, WIRED_METADATA_STRING, "" ),
	FIELD( 22u, "key", key, WIRED_METADATA_STRING, "" ),
	FIELD( 23u, "value", value, WIRED_METADATA_STRING, "" )
};

const wiredMetadataRegistry_t *G_EntityMetadataRegistry( void )
{
	static wiredMetadataRegistry_t registry;
	if ( !registry.identityHash &&
		 !WiredMetadata_RegistryBuild( entityFields,
			(uint32_t)( sizeof( entityFields ) / sizeof( entityFields[0] ) ), &registry ) )
		return NULL;
	return WiredMetadata_RegistryValid( &registry ) ? &registry : NULL;
}

static sgFieldType_t SaveTypeForMetadata( wiredMetadataValueType_t type )
{
	switch ( type ) {
	case WIRED_METADATA_INT: return SG_INT;
	case WIRED_METADATA_FLOAT: return SG_FLOAT;
	case WIRED_METADATA_STRING: return SG_STRING;
	case WIRED_METADATA_VEC3:
	case WIRED_METADATA_ANGLE_YAW: return SG_VECTOR;
	default: return SG_NONE;
	}
}

static qboolean SaveFieldCoversMetadata( const saveField_t *save,
	const wiredMetadataField_t *metadata )
{
	sgFieldType_t expected = SaveTypeForMetadata( metadata->valueType );
	if ( save->type == expected && save->ofs == metadata->offset )
		return qtrue;
	if ( save->type == SG_RAW && metadata->offset >= save->ofs &&
		metadata->offset + metadata->byteSize >= metadata->offset &&
		metadata->offset + metadata->byteSize <= save->ofs + save->size )
		return qtrue;
	return qfalse;
}

qboolean G_EntityMetadataSaveAdapterValid( void )
{
	const wiredMetadataRegistry_t *registry = G_EntityMetadataRegistry();
	uint32_t index;
	if ( !registry )
		return qfalse;
	for ( index = 0u; index < registry->fieldCount; ++index ) {
		const wiredMetadataField_t *metadata = &registry->fields[index];
		const saveField_t *save;
		qboolean covered = qfalse;
		if ( metadata->persistence == WIRED_METADATA_PERSIST_NONE )
			continue;
		for ( save = gentityFields; save->type != SG_NONE; ++save ) {
			if ( SaveFieldCoversMetadata( save, metadata ) ) {
				covered = qtrue;
				break;
			}
		}
		if ( !covered )
			return qfalse;
	}
	return qtrue;
}

qboolean G_EntityMetadataInspect( int entityNum, const char *fieldName )
{
	const wiredMetadataRegistry_t *registry = G_EntityMetadataRegistry();
	gentity_t *entity;
	uint32_t index;
	if ( !g_cheats.integer ) {
		Com_Log( SEV_INFO, LOG_CH(ch_game),
			"entityinspect is cheat-protected (sv_cheats 1)\n" );
		return qtrue;
	}
	if ( !registry || !G_EntityMetadataSaveAdapterValid() ) {
		Com_Log( SEV_ERROR, LOG_CH(ch_game),
			"entityinspect: metadata registry/save adapter invalid\n" );
		return qtrue;
	}
	if ( entityNum < 0 || entityNum >= level.num_entities || !g_entities[entityNum].inuse ) {
		Com_Log( SEV_INFO, LOG_CH(ch_game),
			"entityinspect: entity %d is not in use\n", entityNum );
		return qtrue;
	}
	entity = &g_entities[entityNum];
	Com_Log( SEV_INFO, LOG_CH(ch_game),
		"entityinspect: entity=%d registry=v%u:%016llx fields=%u\n",
		entityNum, registry->schemaVersion,
		(unsigned long long)registry->identityHash, registry->fieldCount );
	for ( index = 0u; index < registry->fieldCount; ++index ) {
		const wiredMetadataField_t *field = &registry->fields[index];
		char value[1024];
		if ( fieldName && fieldName[0] && WiredMetadata_Find( registry, fieldName ) != field )
			continue;
		if ( WiredMetadata_Inspect( field, entity, value, sizeof( value ) ) )
			Com_Log( SEV_INFO, LOG_CH(ch_game), "  %u:%s=%s\n",
				field->stableId, field->name, value );
	}
	return qtrue;
}

#undef FIELD
