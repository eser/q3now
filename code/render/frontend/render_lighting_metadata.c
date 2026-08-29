// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "render_lighting_metadata.h"

#include <limits.h>
#include <math.h>
#include <stddef.h>
#include <string.h>

#define FIELD_FOR( struct_, id_, name_, member_, type_, default_ ) \
	{ WIRED_METADATA_SCHEMA_VERSION, id_, name_, NULL, type_, WIRED_METADATA_PARSE_STRICT, \
		WIRED_METADATA_PERSIST_NONE, WIRED_METADATA_INSPECT, offsetof( struct_, member_ ), \
		type_ == WIRED_METADATA_INT ? sizeof( int ) : \
		type_ == WIRED_METADATA_FLOAT ? sizeof( float ) : sizeof( vec3_t ), default_ }
#define VOLUME_FIELD( id_, name_, member_, type_, default_ ) \
	FIELD_FOR( renderLightingVolumeAuthoring_t, id_, name_, member_, type_, default_ )
#define LIGHT_FIELD( id_, name_, member_, type_, default_ ) \
	FIELD_FOR( renderLightAuthoring_t, id_, name_, member_, type_, default_ )
#define EMISSIVE_FIELD( id_, name_, member_, type_, default_ ) \
	FIELD_FOR( renderEmissiveMaterialAuthoring_t, id_, name_, member_, type_, default_ )

static const wiredMetadataField_t volumeFields[] = {
	VOLUME_FIELD( 1001u, "probe_origin", origin, WIRED_METADATA_VEC3, "0 0 0" ),
	VOLUME_FIELD( 1002u, "probe_spacing", spacing, WIRED_METADATA_VEC3, "64 64 64" ),
	VOLUME_FIELD( 1003u, "probe_bounds_min", boundsMin, WIRED_METADATA_VEC3, "0 0 0" ),
	VOLUME_FIELD( 1004u, "probe_bounds_max", boundsMax, WIRED_METADATA_VEC3, "0 0 0" ),
	VOLUME_FIELD( 1005u, "probe_dimensions_x", dimensionsX, WIRED_METADATA_INT, "1" ),
	VOLUME_FIELD( 1006u, "probe_dimensions_y", dimensionsY, WIRED_METADATA_INT, "1" ),
	VOLUME_FIELD( 1007u, "probe_dimensions_z", dimensionsZ, WIRED_METADATA_INT, "1" ),
	VOLUME_FIELD( 1008u, "probe_priority", priority, WIRED_METADATA_INT, "0" ),
	VOLUME_FIELD( 1009u, "probe_blend_distance", blendDistance, WIRED_METADATA_FLOAT, "0" ),
	VOLUME_FIELD( 1010u, "probe_fallback", fallback, WIRED_METADATA_INT, "1" )
};

static const wiredMetadataField_t lightFields[] = {
	LIGHT_FIELD( 1101u, "light_kind", kind, WIRED_METADATA_INT, "2" ),
	LIGHT_FIELD( 1102u, "light_mobility", mobility, WIRED_METADATA_INT, "2" ),
	LIGHT_FIELD( 1103u, "light_position", position, WIRED_METADATA_VEC3, "0 0 0" ),
	LIGHT_FIELD( 1104u, "light_direction", direction, WIRED_METADATA_VEC3, "0 0 -1" ),
	LIGHT_FIELD( 1105u, "light_bounds_min", boundsMin, WIRED_METADATA_VEC3, "-256 -256 -256" ),
	LIGHT_FIELD( 1106u, "light_bounds_max", boundsMax, WIRED_METADATA_VEC3, "256 256 256" ),
	LIGHT_FIELD( 1107u, "light_radiance", radiance, WIRED_METADATA_VEC3, "1 1 1" ),
	LIGHT_FIELD( 1108u, "light_range", range, WIRED_METADATA_FLOAT, "256" ),
	LIGHT_FIELD( 1109u, "light_inner_cone_cos", innerConeCos, WIRED_METADATA_FLOAT, "0" ),
	LIGHT_FIELD( 1110u, "light_outer_cone_cos", outerConeCos, WIRED_METADATA_FLOAT, "0" ),
	LIGHT_FIELD( 1111u, "light_shadow_priority", shadowPriority, WIRED_METADATA_INT, "0" ),
	LIGHT_FIELD( 1112u, "light_flags", flags, WIRED_METADATA_INT, "1" )
};

static const wiredMetadataField_t emissiveMaterialFields[] = {
	EMISSIVE_FIELD( 1201u, "material_diffuse_reflectance", diffuseReflectance, WIRED_METADATA_VEC3, "1 1 1" ),
	EMISSIVE_FIELD( 1202u, "material_emissive_radiance", emissiveRadiance, WIRED_METADATA_VEC3, "0 0 0" ),
	EMISSIVE_FIELD( 1203u, "material_emissive_mobility", emissiveMobility, WIRED_METADATA_INT, "1" ),
	EMISSIVE_FIELD( 1204u, "material_emissive_range", emissiveInfluenceRange, WIRED_METADATA_FLOAT, "256" ),
	EMISSIVE_FIELD( 1205u, "material_emissive_shadow_priority", emissiveShadowPriority, WIRED_METADATA_INT, "0" ),
	EMISSIVE_FIELD( 1206u, "material_emissive_proxy_count", emissiveRequestedProxyCount, WIRED_METADATA_INT, "1" ),
	EMISSIVE_FIELD( 1207u, "material_static_bake", participatesInStaticBake, WIRED_METADATA_INT, "1" ),
	EMISSIVE_FIELD( 1208u, "material_explicit_proxy_authority", emissiveExplicitProxyAuthority, WIRED_METADATA_INT, "0" ),
	EMISSIVE_FIELD( 1209u, "material_inject_atmosphere", emissiveInjectsAtmosphere, WIRED_METADATA_INT, "0" )
};

const wiredMetadataRegistry_t *Render_LightingVolumeMetadataRegistry( void )
{
	static wiredMetadataRegistry_t registry;
	if ( !registry.identityHash &&
		!WiredMetadata_RegistryBuild( volumeFields,
			(uint32_t)( sizeof( volumeFields ) / sizeof( volumeFields[0] ) ), &registry ) )
		return NULL;
	return WiredMetadata_RegistryValid( &registry ) ? &registry : NULL;
}

const wiredMetadataRegistry_t *Render_LightMetadataRegistry( void )
{
	static wiredMetadataRegistry_t registry;
	if ( !registry.identityHash &&
		!WiredMetadata_RegistryBuild( lightFields,
			(uint32_t)( sizeof( lightFields ) / sizeof( lightFields[0] ) ), &registry ) )
		return NULL;
	return WiredMetadata_RegistryValid( &registry ) ? &registry : NULL;
}

const wiredMetadataRegistry_t *Render_EmissiveMaterialMetadataRegistry( void )
{
	static wiredMetadataRegistry_t registry;
	if ( !registry.identityHash &&
		!WiredMetadata_RegistryBuild( emissiveMaterialFields,
			(uint32_t)( sizeof( emissiveMaterialFields ) / sizeof( emissiveMaterialFields[0] ) ), &registry ) )
		return NULL;
	return WiredMetadata_RegistryValid( &registry ) ? &registry : NULL;
}

static qboolean FloatToQ16( float value, int32_t *out )
{
	double scaled;
	if ( !out || !isfinite( value ) ) return qfalse;
	scaled = (double)value * 65536.0;
	if ( scaled < (double)INT32_MIN || scaled > (double)INT32_MAX ) return qfalse;
	*out = (int32_t)( scaled < 0.0 ? scaled - 0.5 : scaled + 0.5 );
	return qtrue;
}

static qboolean VecToQ16( const vec3_t source, ralLightVec3Q16_t *out )
{
	return FloatToQ16( source[0], &out->x ) && FloatToQ16( source[1], &out->y ) &&
		FloatToQ16( source[2], &out->z );
}

qboolean Render_LightingVolumePlacementBuild(
	const renderLightingVolumeAuthoring_t *a,
	uint64_t volumeId, uint64_t sourceGeneration, uint64_t provenanceHash,
	ralIrradianceVolumePlacement_t *out )
{
	ralIrradianceVolumePlacement_t placement;
	int32_t blendQ16;
	if ( !a || !out || a->schemaVersion != RENDER_LIGHTING_VOLUME_AUTHORING_SCHEMA_VERSION ||
		!volumeId || !sourceGeneration || !provenanceHash ||
		a->dimensionsX <= 0 || a->dimensionsY <= 0 || a->dimensionsZ <= 0 ||
		a->priority < 0 || a->blendDistance < 0.0f ||
		( a->fallback != RAL_IRRADIANCE_FALLBACK_LIGHTGRID &&
		  a->fallback != RAL_IRRADIANCE_FALLBACK_GLOBAL_AMBIENT ) )
		return qfalse;
	memset( &placement, 0, sizeof( placement ) );
	placement.schemaVersion = RAL_IRRADIANCE_PLACEMENT_SCHEMA_VERSION;
	placement.volumeId = volumeId;
	placement.sourceGeneration = sourceGeneration;
	placement.provenanceHash = provenanceHash;
	if ( !VecToQ16( a->origin, &placement.origin ) ||
		!VecToQ16( a->spacing, &placement.spacing ) ||
		!VecToQ16( a->boundsMin, &placement.boundsMin ) ||
		!VecToQ16( a->boundsMax, &placement.boundsMax ) ||
		!FloatToQ16( a->blendDistance, &blendQ16 ) )
		return qfalse;
	placement.dimensions[0] = (uint32_t)a->dimensionsX;
	placement.dimensions[1] = (uint32_t)a->dimensionsY;
	placement.dimensions[2] = (uint32_t)a->dimensionsZ;
	placement.priority = (uint32_t)a->priority;
	placement.blendDistanceQ16 = (uint32_t)blendQ16;
	placement.fallback = (ralIrradianceFallback_t)a->fallback;
	placement.ready = qtrue;
	if ( !Ral_IrradianceVolumePlacementValid( &placement ) ) return qfalse;
	*out = placement;
	return qtrue;
}

qboolean Render_LightDescriptionBuild( const renderLightAuthoring_t *a,
	uint64_t lightId, uint64_t sourceGeneration, uint64_t provenanceHash,
	ralLightDescription_t *out )
{
	ralLightDescription_t light;
	ralLightVec3Q16_t radiance;
	if ( !a || !out || a->schemaVersion != RENDER_LIGHT_AUTHORING_SCHEMA_VERSION ||
		!lightId || !sourceGeneration || !provenanceHash || a->shadowPriority < 0 ||
		a->flags <= 0 )
		return qfalse;
	memset( &light, 0, sizeof( light ) );
	light.schemaVersion = RAL_LIGHT_SCHEMA_VERSION;
	light.lightId = lightId;
	light.sourceGeneration = sourceGeneration;
	light.provenanceHash = provenanceHash;
	light.kind = (ralLightKind_t)a->kind;
	light.mobility = (ralLightMobility_t)a->mobility;
	if ( !VecToQ16( a->position, &light.position ) ||
		!VecToQ16( a->direction, &light.direction ) ||
		!VecToQ16( a->boundsMin, &light.boundsMin ) ||
		!VecToQ16( a->boundsMax, &light.boundsMax ) ||
		!VecToQ16( a->radiance, &radiance ) ||
		!FloatToQ16( a->range, &light.rangeQ16 ) ||
		!FloatToQ16( a->innerConeCos, &light.innerConeCosQ16 ) ||
		!FloatToQ16( a->outerConeCos, &light.outerConeCosQ16 ) )
		return qfalse;
	light.radianceQ16[0] = radiance.x;
	light.radianceQ16[1] = radiance.y;
	light.radianceQ16[2] = radiance.z;
	light.shadowPriority = (uint32_t)a->shadowPriority;
	light.flags = (uint32_t)a->flags;
	if ( !Ral_LightDescriptionValid( &light ) ) return qfalse;
	*out = light;
	return qtrue;
}

qboolean Render_EmissiveMaterialLightingBuild(
	const renderEmissiveMaterialAuthoring_t *a,
	uint64_t sourceGeneration, uint64_t provenanceHash,
	renderMaterialLighting_t *out )
{
	renderMaterialLighting_t lighting;
	ralLightVec3Q16_t diffuse, emissive;
	qboolean hasEmission;
	if ( !a || !out ||
		a->schemaVersion != RENDER_EMISSIVE_MATERIAL_AUTHORING_SCHEMA_VERSION ||
		!sourceGeneration || sourceGeneration == UINT64_MAX ||
		!provenanceHash || provenanceHash == UINT64_MAX ||
		a->emissiveShadowPriority < 0 || a->emissiveShadowPriority > UINT16_MAX ||
		a->emissiveRequestedProxyCount < 0 ||
		a->emissiveRequestedProxyCount > (int)RAL_EMISSIVE_PROXY_MAX_PER_SURFACE ||
		( a->participatesInStaticBake != 0 && a->participatesInStaticBake != 1 ) ||
		( a->emissiveExplicitProxyAuthority != 0 && a->emissiveExplicitProxyAuthority != 1 ) ||
		( a->emissiveInjectsAtmosphere != 0 && a->emissiveInjectsAtmosphere != 1 ) ||
		!VecToQ16( a->diffuseReflectance, &diffuse ) ||
		!VecToQ16( a->emissiveRadiance, &emissive ) )
		return qfalse;
	if ( diffuse.x < 0 || diffuse.x > RAL_LIGHT_Q16_ONE ||
		diffuse.y < 0 || diffuse.y > RAL_LIGHT_Q16_ONE ||
		diffuse.z < 0 || diffuse.z > RAL_LIGHT_Q16_ONE ||
		emissive.x < 0 || emissive.y < 0 || emissive.z < 0 ) return qfalse;
	hasEmission = emissive.x || emissive.y || emissive.z;
	if ( hasEmission && ( a->emissiveMobility < RAL_LIGHT_MOBILITY_STATIC ||
		a->emissiveMobility > RAL_LIGHT_MOBILITY_DYNAMIC ||
		!isfinite( a->emissiveInfluenceRange ) || a->emissiveInfluenceRange <= 0.0f ||
		( a->emissiveMobility == RAL_LIGHT_MOBILITY_DYNAMIC && a->participatesInStaticBake ) ) )
		return qfalse;
	memset( &lighting, 0, sizeof( lighting ) );
	lighting.schemaVersion = RENDER_MATERIAL_LIGHTING_SCHEMA_VERSION;
	lighting.sourceGeneration = sourceGeneration;
	lighting.provenanceHash = provenanceHash;
	lighting.diffuseReflectanceQ16[0] = diffuse.x;
	lighting.diffuseReflectanceQ16[1] = diffuse.y;
	lighting.diffuseReflectanceQ16[2] = diffuse.z;
	lighting.emissionRadianceQ16[0] = emissive.x;
	lighting.emissionRadianceQ16[1] = emissive.y;
	lighting.emissionRadianceQ16[2] = emissive.z;
	if ( hasEmission ) {
		lighting.emissiveMobility = (ralLightMobility_t)a->emissiveMobility;
		if ( !FloatToQ16( a->emissiveInfluenceRange, &lighting.emissiveInfluenceRangeQ16 ) ) return qfalse;
		lighting.emissiveShadowPriority = (uint32_t)a->emissiveShadowPriority;
		lighting.emissiveRequestedProxyCount = (uint32_t)a->emissiveRequestedProxyCount;
		lighting.emissiveExplicitProxyAuthority = a->emissiveExplicitProxyAuthority ? qtrue : qfalse;
		lighting.emissiveInjectsAtmosphere = a->emissiveInjectsAtmosphere ? qtrue : qfalse;
	}
	lighting.participatesInStaticBake = a->participatesInStaticBake ? qtrue : qfalse;
	lighting.ready = qtrue;
	*out = lighting;
	return qtrue;
}

#undef EMISSIVE_FIELD
#undef LIGHT_FIELD
#undef VOLUME_FIELD
#undef FIELD_FOR
