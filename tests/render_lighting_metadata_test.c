// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "render_lighting_metadata.h"

#include <stdio.h>
#include <string.h>

#define CHECK( x ) do { if ( !(x) ) { fprintf( stderr, "FAIL %d: %s\n", __LINE__, #x ); return 1; } } while ( 0 )

static qboolean Parse( const wiredMetadataRegistry_t *registry,
	void *authoring, const char *name, const char *value )
{
	const wiredMetadataField_t *field = WiredMetadata_Find( registry, name );
	return field && WiredMetadata_Parse( field, authoring, value, NULL );
}

int main( void )
{
	const wiredMetadataRegistry_t *registry = Render_LightingVolumeMetadataRegistry();
	renderLightingVolumeAuthoring_t authoring, before;
	ralIrradianceVolumePlacement_t placement, placementBefore;
	const wiredMetadataRegistry_t *lightRegistry = Render_LightMetadataRegistry();
	const wiredMetadataRegistry_t *materialRegistry = Render_EmissiveMaterialMetadataRegistry();
	renderLightAuthoring_t lightAuthoring, lightBefore;
	ralLightDescription_t light, lightBeforeOutput;
	renderEmissiveMaterialAuthoring_t materialAuthoring, materialBefore;
	renderMaterialLighting_t materialLighting, materialLightingBefore;
	char inspected[64];
	CHECK( registry && registry->fieldCount == 10u );
	memset( &authoring, 0, sizeof( authoring ) );
	authoring.schemaVersion = RENDER_LIGHTING_VOLUME_AUTHORING_SCHEMA_VERSION;
	CHECK( Parse( registry, &authoring, "probe_origin", "10 20 30" ) );
	CHECK( Parse( registry, &authoring, "probe_spacing", "64 64 32" ) );
	CHECK( Parse( registry, &authoring, "probe_bounds_min", "0 0 0" ) );
	CHECK( Parse( registry, &authoring, "probe_bounds_max", "192 192 96" ) );
	CHECK( Parse( registry, &authoring, "probe_dimensions_x", "3" ) );
	CHECK( Parse( registry, &authoring, "probe_dimensions_y", "3" ) );
	CHECK( Parse( registry, &authoring, "probe_dimensions_z", "3" ) );
	CHECK( Parse( registry, &authoring, "probe_priority", "4" ) );
	CHECK( Parse( registry, &authoring, "probe_blend_distance", "32" ) );
	CHECK( Parse( registry, &authoring, "probe_fallback", "1" ) );
	before = authoring;
	CHECK( !Parse( registry, &authoring, "probe_dimensions_x", "3junk" ) );
	CHECK( !memcmp( &before, &authoring, sizeof( before ) ) );
	CHECK( WiredMetadata_Inspect( WiredMetadata_Find( registry, "probe_origin" ),
		&authoring, inspected, sizeof( inspected ) ) );
	CHECK( !strcmp( inspected, "10 20 30" ) );
	CHECK( Render_LightingVolumePlacementBuild( &authoring, 7u, 8u, 9u, &placement ) );
	CHECK( placement.dimensions[0] == 3u && placement.priority == 4u &&
		placement.origin.x == 10 * 65536 && placement.blendDistanceQ16 == 32u * 65536u );
	placementBefore = placement;
	authoring.spacing[0] = 0.0f;
	CHECK( !Render_LightingVolumePlacementBuild( &authoring, 7u, 8u, 9u, &placement ) );
	CHECK( !memcmp( &placement, &placementBefore, sizeof( placement ) ) );
	CHECK( lightRegistry && lightRegistry->fieldCount == 12u );
	memset( &lightAuthoring, 0, sizeof( lightAuthoring ) );
	lightAuthoring.schemaVersion = RENDER_LIGHT_AUTHORING_SCHEMA_VERSION;
	CHECK( Parse( lightRegistry, &lightAuthoring, "light_kind", "2" ) );
	CHECK( Parse( lightRegistry, &lightAuthoring, "light_mobility", "2" ) );
	CHECK( Parse( lightRegistry, &lightAuthoring, "light_position", "0 0 0" ) );
	CHECK( Parse( lightRegistry, &lightAuthoring, "light_bounds_min", "-128 -128 -128" ) );
	CHECK( Parse( lightRegistry, &lightAuthoring, "light_bounds_max", "128 128 128" ) );
	CHECK( Parse( lightRegistry, &lightAuthoring, "light_radiance", "12 4 1" ) );
	CHECK( Parse( lightRegistry, &lightAuthoring, "light_range", "128" ) );
	CHECK( Parse( lightRegistry, &lightAuthoring, "light_shadow_priority", "3" ) );
	CHECK( Parse( lightRegistry, &lightAuthoring, "light_flags", "11" ) );
	CHECK( Render_LightDescriptionBuild( &lightAuthoring, 17u, 18u, 19u, &light ) );
	CHECK( light.kind == RAL_LIGHT_KIND_POINT && light.mobility == RAL_LIGHT_MOBILITY_STATIONARY &&
		light.radianceQ16[0] == 12 * 65536 && light.shadowPriority == 3u );
	lightBefore = lightAuthoring; lightBeforeOutput = light;
	CHECK( !Parse( lightRegistry, &lightAuthoring, "light_range", "128junk" ) &&
		!memcmp( &lightAuthoring, &lightBefore, sizeof( lightBefore ) ) );
	lightAuthoring.mobility = RAL_LIGHT_MOBILITY_DYNAMIC;
	lightAuthoring.flags |= RAL_LIGHT_CONTRIBUTE_BAKE;
	CHECK( !Render_LightDescriptionBuild( &lightAuthoring, 17u, 18u, 19u, &light ) &&
		!memcmp( &light, &lightBeforeOutput, sizeof( light ) ) );
	CHECK( materialRegistry && materialRegistry->fieldCount == 9u );
	memset( &materialAuthoring, 0, sizeof( materialAuthoring ) );
	materialAuthoring.schemaVersion = RENDER_EMISSIVE_MATERIAL_AUTHORING_SCHEMA_VERSION;
	CHECK( Parse( materialRegistry, &materialAuthoring, "material_diffuse_reflectance", "0.5 0.25 0.125" ) );
	CHECK( Parse( materialRegistry, &materialAuthoring, "material_emissive_radiance", "16 4 1" ) );
	CHECK( Parse( materialRegistry, &materialAuthoring, "material_emissive_mobility", "2" ) );
	CHECK( Parse( materialRegistry, &materialAuthoring, "material_emissive_range", "384" ) );
	CHECK( Parse( materialRegistry, &materialAuthoring, "material_emissive_shadow_priority", "9" ) );
	CHECK( Parse( materialRegistry, &materialAuthoring, "material_emissive_proxy_count", "3" ) );
	CHECK( Parse( materialRegistry, &materialAuthoring, "material_static_bake", "1" ) );
	CHECK( Parse( materialRegistry, &materialAuthoring, "material_explicit_proxy_authority", "0" ) );
	CHECK( Parse( materialRegistry, &materialAuthoring, "material_inject_atmosphere", "1" ) );
	CHECK( Render_EmissiveMaterialLightingBuild( &materialAuthoring, 21u, 22u, &materialLighting ) );
	CHECK( materialLighting.schemaVersion == RENDER_MATERIAL_LIGHTING_SCHEMA_VERSION &&
		materialLighting.diffuseReflectanceQ16[0] == 32768 &&
		materialLighting.emissionRadianceQ16[0] == 16 * 65536 &&
		materialLighting.emissiveMobility == RAL_LIGHT_MOBILITY_STATIONARY &&
		materialLighting.emissiveInfluenceRangeQ16 == 384 * 65536 &&
		materialLighting.emissiveRequestedProxyCount == 3u &&
		materialLighting.participatesInStaticBake && materialLighting.emissiveInjectsAtmosphere );
	materialBefore = materialAuthoring; materialLightingBefore = materialLighting;
	CHECK( !Parse( materialRegistry, &materialAuthoring, "material_emissive_proxy_count", "3junk" ) &&
		!memcmp( &materialAuthoring, &materialBefore, sizeof( materialBefore ) ) );
	materialAuthoring.emissiveMobility = RAL_LIGHT_MOBILITY_DYNAMIC;
	CHECK( !Render_EmissiveMaterialLightingBuild( &materialAuthoring, 21u, 22u, &materialLighting ) &&
		!memcmp( &materialLighting, &materialLightingBefore, sizeof( materialLighting ) ) );
	puts( "render_lighting_metadata_test: ok" );
	return 0;
}
