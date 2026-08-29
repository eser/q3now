// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "render_lighting_project_source.h"
#include "render_lighting_project_cook.h"
#include "lighting_cook_fs.h"
#include "maps/map_format_registry.h"
#include "qfiles.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined( _WIN32 )
#include <windows.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#define CHECK( expression ) do { if ( !( expression ) ) { \
	fprintf( stderr, "FAIL %d: %s\n", __LINE__, #expression ); return 1; \
} } while ( 0 )

static void VisibleTrace( trace_t *result, const vec3_t start, const vec3_t end,
	const vec3_t mins, const vec3_t maxs, clipHandle_t model, int brushmask,
	qboolean capsule )
{
	(void)start; (void)mins; (void)maxs; (void)model; (void)brushmask; (void)capsule;
	memset( result, 0, sizeof( *result ) );
	result->fraction = 1.0f;
	VectorCopy( end, result->endpos );
}

static void Vertex( drawVert_t *vertex, float x, float y, float z,
	float u, float v )
{
	memset( vertex, 0, sizeof( *vertex ) );
	vertex->xyz[0] = x; vertex->xyz[1] = y; vertex->xyz[2] = z;
	vertex->lightmap[0] = u; vertex->lightmap[1] = v;
	vertex->normal[2] = 1.0f;
	vertex->color.rgba[0] = vertex->color.rgba[1] =
		vertex->color.rgba[2] = vertex->color.rgba[3] = 255u;
}

static qboolean MakeTemporaryDirectory( char *path, size_t capacity )
{
#if defined( _WIN32 )
	char root[MAX_PATH];
	DWORD rootLength = GetTempPathA( (DWORD)sizeof( root ), root );
	int length;
	if ( !rootLength || rootLength >= sizeof( root ) ) return qfalse;
	length = snprintf( path, capacity, "%swired-lighting-project-%lu",
		root, (unsigned long)GetCurrentProcessId() );
	return length > 0 && (size_t)length < capacity &&
		CreateDirectoryA( path, NULL ) ? qtrue : qfalse;
#else
	char temporary[] = "/tmp/wired-lighting-project-XXXXXX";
	char *created = mkdtemp( temporary );
	if ( !created || strlen( created ) >= capacity ) return qfalse;
	memcpy( path, created, strlen( created ) + 1u );
	return qtrue;
#endif
}

static void RemoveTemporaryDirectory( const char *path )
{
#if defined( _WIN32 )
	WIN32_FIND_DATAA entry;
	HANDLE search;
	char pattern[MAX_PATH], child[MAX_PATH];
	(void)snprintf( pattern, sizeof( pattern ), "%s\\*", path );
	search = FindFirstFileA( pattern, &entry );
	if ( search != INVALID_HANDLE_VALUE ) {
		do {
			if ( strcmp( entry.cFileName, "." ) && strcmp( entry.cFileName, ".." ) ) {
				(void)snprintf( child, sizeof( child ), "%s\\%s", path,
					entry.cFileName );
				(void)DeleteFileA( child );
			}
		} while ( FindNextFileA( search, &entry ) );
		FindClose( search );
	}
	(void)RemoveDirectoryA( path );
#else
	DIR *directory = opendir( path );
	struct dirent *entry;
	char child[WIRED_LIGHTING_COOK_FS_PATH_CAPACITY];
	if ( directory ) {
		while ( ( entry = readdir( directory ) ) != NULL ) {
			if ( strcmp( entry->d_name, "." ) && strcmp( entry->d_name, ".." ) ) {
				(void)snprintf( child, sizeof( child ), "%s/%s", path,
					entry->d_name );
				(void)unlink( child );
			}
		}
		(void)closedir( directory );
	}
	(void)rmdir( path );
#endif
}

int main( void )
{
	static char entities[] =
		"{ \"classname\" \"worldspawn\" }\n"
		"{ \"classname\" \"wired_probe_volume\" \"origin\" \"-8 -8 8\" "
		"\"probe_spacing\" \"16 16 16\" "
		"\"probe_bounds_min\" \"-8 -8 8\" "
		"\"probe_bounds_max\" \"8 8 24\" "
		"\"probe_dimensions_x\" \"2\" \"probe_dimensions_y\" \"2\" "
		"\"probe_dimensions_z\" \"2\" \"probe_priority\" \"3\" "
		"\"probe_blend_distance\" \"8\" \"probe_fallback\" \"1\" }\n";
	mapFile_t map;
	dshader_t shaders[2];
	dsurface_t surfaces[2];
	drawVert_t vertices[6];
	int indices[6] = { 0, 1, 2, 0, 1, 2 };
	byte lightmap[4u * 4u * 3u];
	byte pixels[4] = { 255u, 255u, 255u, 255u };
	renderSubmissionState_t submission;
	renderMaterialLighting_t lighting;
	qhandle_t materials[2], lightmapMaterial;
	char lightmapName[MAX_QPATH];
	refimport_t imports;
	renderLightingProjectSourceRequest_t request;
	renderLightingProjectSourceWorkspace_t workspace;
	renderLightingProjectSourceReceipt_t first, second, before;
	renderLightingProjectCookRequest_t cookRequest;
	renderLightingProjectCookReceipt_t cookReceipt, repeatedCookReceipt;
	wiredLightingCookFilesystem_t filesystem;
	ralLightingPatchTriangle_t triangles[2];
	renderLightingVisibilityCandidate_t candidates[4];
	ralLightingPatchVisibility_t visibilityScratch[4], visibility[4];
	ralLightingBakePatch_t patches[2];
	ralLightingBakeLink_t links[4];
	uint8_t dirty[2];
	uint32_t texelPatches[16];
	renderLightingProjectVolume_t volumes[2];
	ralIrradianceProductSample_t samples[32];
	char temporaryRoot[WIRED_LIGHTING_COOK_FS_PATH_CAPACITY];
	char defaultWorldStem[RENDER_LIGHTING_PROJECT_WORLD_STEM_CAPACITY];
	char entitiesBefore[sizeof( entities )];
	byte lightmapBefore[sizeof( lightmap )];
	void *sidecarBytes;
	uint64_t sidecarLength;

	memset( &map, 0, sizeof( map ) );
	memset( shaders, 0, sizeof( shaders ) );
	memset( surfaces, 0, sizeof( surfaces ) );
	memset( lightmap, 64, sizeof( lightmap ) );
	(void)snprintf( map.name, sizeof( map.name ), "%s", "maps/project-source.bsp" );
	map.checksum = 0x4567;
	map.entityString = entities;
	map.entityStringLength = (int)strlen( entities ) + 1;
	map.numShaders = 2; map.shaders = shaders;
	(void)snprintf( shaders[0].shader, sizeof( shaders[0].shader ), "%s",
		"textures/project/emitter" );
	(void)snprintf( shaders[1].shader, sizeof( shaders[1].shader ), "%s",
		"textures/project/receiver" );
	map.numSurfaces = 2; map.surfaces = surfaces;
	map.numDrawVerts = 6; map.drawVerts = vertices;
	map.numDrawIndexes = 6; map.drawIndexes = indices;
	map.numLightmapPages = 1; map.lightmapPageSize = (int)sizeof( lightmap );
	map.lightmapData = lightmap;
	Vertex( &vertices[0], -32.0f, -32.0f, 0.0f, 0.0f, 0.0f );
	Vertex( &vertices[1], 32.0f, -32.0f, 0.0f, 1.0f, 0.0f );
	Vertex( &vertices[2], -32.0f, 32.0f, 0.0f, 0.0f, 1.0f );
	Vertex( &vertices[3], -32.0f, -32.0f, 64.0f, 0.0f, 0.0f );
	Vertex( &vertices[4], -32.0f, 32.0f, 64.0f, 0.0f, 1.0f );
	Vertex( &vertices[5], 32.0f, -32.0f, 64.0f, 1.0f, 0.0f );
	for ( int index = 0; index < 2; ++index ) {
		surfaces[index].surfaceType = MST_PLANAR;
		surfaces[index].shaderNum = index;
		surfaces[index].lightmapNum = 0;
		surfaces[index].firstVert = index * 3;
		surfaces[index].numVerts = 3;
		surfaces[index].firstIndex = index * 3;
		surfaces[index].numIndexes = 3;
	}
	CHECK( RenderSubmission_Init( &submission, 1u ) );
	materials[0] = RenderSubmission_RegisterMaterialImage( &submission,
		RENDER_ASSET_MATERIAL, shaders[0].shader, qtrue, pixels, 1u, 1u );
	materials[1] = RenderSubmission_RegisterMaterialImage( &submission,
		RENDER_ASSET_MATERIAL, shaders[1].shader, qtrue, pixels, 1u, 1u );
	CHECK( materials[0] > 0 && materials[1] > 0 );
	memset( &lighting, 0, sizeof( lighting ) );
	lighting.schemaVersion = RENDER_MATERIAL_LIGHTING_SCHEMA_VERSION;
	lighting.sourceGeneration = 2u;
	lighting.provenanceHash = 3u;
	lighting.diffuseReflectanceQ16[0] = lighting.diffuseReflectanceQ16[1] =
		lighting.diffuseReflectanceQ16[2] = RAL_LIGHT_Q16_ONE / 2;
	lighting.emissionRadianceQ16[0] = RAL_LIGHT_Q16_ONE * 8;
	lighting.emissionRadianceQ16[1] = RAL_LIGHT_Q16_ONE * 2;
	lighting.emissionRadianceQ16[2] = RAL_LIGHT_Q16_ONE;
	lighting.emissiveMobility = RAL_LIGHT_MOBILITY_STATIC;
	lighting.emissiveInfluenceRangeQ16 = RAL_LIGHT_Q16_ONE * 128;
	lighting.emissiveRequestedProxyCount = 1u;
	lighting.participatesInStaticBake = qtrue;
	lighting.ready = qtrue;
	CHECK( RenderSubmission_SetMaterialLighting( &submission, materials[0], &lighting ) );
	lighting.sourceGeneration = 4u; lighting.provenanceHash = 5u;
	lighting.emissionRadianceQ16[0] = lighting.emissionRadianceQ16[1] =
		lighting.emissionRadianceQ16[2] = 0;
	lighting.emissiveMobility = 0;
	lighting.emissiveInfluenceRangeQ16 = 0;
	lighting.emissiveRequestedProxyCount = 0u;
	CHECK( RenderSubmission_SetMaterialLighting( &submission, materials[1], &lighting ) );
	CHECK( RenderSubmission_LightmapMaterialName( lightmapName,
		(uint32_t)map.checksum, 0 ) );
	lightmapMaterial = RenderSubmission_RegisterMaterialImage( &submission,
		RENDER_ASSET_LIGHTMAP, lightmapName, qtrue, pixels, 1u, 1u );
	CHECK( lightmapMaterial > 0 && RenderSubmission_LoadWorld( &submission, &map, 0 ) );
	memset( &imports, 0, sizeof( imports ) ); imports.CM_BoxTrace = VisibleTrace;
	memset( &request, 0, sizeof( request ) );
	request.schemaVersion = RENDER_LIGHTING_PROJECT_SOURCE_SCHEMA_VERSION;
	request.sourceGeneration = 7u; request.visibilityAuthorityHash = 8u;
	request.maximumLinksPerPatch = 1u; request.maximumSamplesPerProbe = 2u;
	memset( &workspace, 0, sizeof( workspace ) );
	workspace.triangles = triangles; workspace.triangleCapacity = 2u;
	workspace.candidates = candidates; workspace.candidateCapacity = 4u;
	workspace.visibilityScratch = visibilityScratch; workspace.visibility = visibility;
	workspace.visibilityCapacity = 4u;
	workspace.patches = patches; workspace.patchCapacity = 2u;
	workspace.links = links; workspace.linkCapacity = 4u;
	workspace.dirtyPatches = dirty; workspace.dirtyPatchCapacity = 2u;
	workspace.texelPatchIndices = texelPatches; workspace.texelCapacity = 16u;
	workspace.volumes = volumes; workspace.volumeCapacity = 2u;
	workspace.samples = samples; workspace.sampleCapacity = 32u;
	CHECK( RenderLightingProjectSource_Build( &submission, &map, &imports,
		&request, &workspace, &first ) &&
		RenderLightingProjectSource_ReceiptValid( &first ) &&
		first.triangleCount == 2u && first.candidateCount == 2u &&
		first.visibilityCount == 2u && first.patchCount == 2u &&
		first.linkCount == 2u && first.pageWidth == 4u &&
		first.pageHeight == 4u && first.pageCount == 1u &&
		first.mappedTexelCount > 0u && first.mappedTexelCount < first.texelCount &&
		first.volumeCount == 1u && first.sampleCount == 16u &&
		volumes[0].placement.dimensions[0] == 2u &&
		volumes[0].placement.priority == 3u );
	CHECK( RenderLightingProjectSource_Build( &submission, &map, &imports,
		&request, &workspace, &second ) && !memcmp( &first, &second, sizeof( first ) ) );
	before = second; map.entityString = "{ \"classname\" \"worldspawn\" }";
	CHECK( !RenderLightingProjectSource_Build( &submission, &map, &imports,
		&request, &workspace, &second ) && !memcmp( &before, &second, sizeof( second ) ) );
	map.entityString = entities;
	memcpy( entitiesBefore, entities, sizeof( entitiesBefore ) );
	memcpy( lightmapBefore, lightmap, sizeof( lightmapBefore ) );
	CHECK( MakeTemporaryDirectory( temporaryRoot, sizeof( temporaryRoot ) ) );
	CHECK( RenderLightingProjectCook_DefaultRequest( &map, temporaryRoot,
		&cookRequest, defaultWorldStem ) &&
		!strcmp( defaultWorldStem, "project-source" ) &&
		cookRequest.schemaVersion == RENDER_LIGHTING_PROJECT_COOK_SCHEMA_VERSION &&
		cookRequest.bounceCount == 2u && cookRequest.derivedRoot == temporaryRoot &&
		cookRequest.worldStem == defaultWorldStem );
	memset( &cookRequest, 0, sizeof( cookRequest ) );
	cookRequest.schemaVersion = RENDER_LIGHTING_PROJECT_COOK_SCHEMA_VERSION;
	cookRequest.cookGeneration = 100u;
	cookRequest.producerVersion = 101u;
	cookRequest.settingsHash = 102u;
	cookRequest.visibilityAuthorityHash = 8u;
	cookRequest.bounceCount = 2u;
	cookRequest.maximumLinksPerPatch = 1u;
	cookRequest.maximumSamplesPerProbe = 2u;
	cookRequest.workerRegionBudget = 1u;
	cookRequest.energyClampQ16 = RAL_LIGHT_Q16_ONE * 16;
	cookRequest.derivedRoot = temporaryRoot;
	cookRequest.worldStem = "project-source";
	memset( &cookReceipt, 0, sizeof( cookReceipt ) );
	CHECK( RenderLightingProjectCook_Execute( &submission, &map, &imports,
		&cookRequest, &cookReceipt ) &&
		RenderLightingProjectCook_ReceiptValid( &cookReceipt ) &&
		cookReceipt.patchCount == 2u && cookReceipt.linkCount == 2u &&
		cookReceipt.volumeCount == 1u && cookReceipt.batchCount == 2u &&
		cookReceipt.completedRegionCount == 2u &&
		!memcmp( entitiesBefore, entities, sizeof( entitiesBefore ) ) &&
		!memcmp( lightmapBefore, lightmap, sizeof( lightmapBefore ) ) );
	CHECK( WiredLightingCookFilesystem_Init( &filesystem, temporaryRoot ) );
	sidecarBytes = malloc( (size_t)( cookReceipt.directionalByteLength >
		cookReceipt.irradianceByteLength ? cookReceipt.directionalByteLength :
		cookReceipt.irradianceByteLength ) );
	CHECK( sidecarBytes != NULL );
	CHECK( WiredLightingCookFilesystem_ReadSidecar( &filesystem,
		"project-source", RAL_LIGHTING_ARTIFACT_DIRECTIONAL_LIGHTMAP,
		sidecarBytes, cookReceipt.directionalByteLength, &sidecarLength ) &&
		sidecarLength == cookReceipt.directionalByteLength );
	CHECK( WiredLightingCookFilesystem_ReadSidecar( &filesystem,
		"project-source", RAL_LIGHTING_ARTIFACT_IRRADIANCE_VOLUME,
		sidecarBytes, cookReceipt.irradianceByteLength, &sidecarLength ) &&
		sidecarLength == cookReceipt.irradianceByteLength );
	memset( &repeatedCookReceipt, 0, sizeof( repeatedCookReceipt ) );
	CHECK( RenderLightingProjectCook_Execute( &submission, &map, &imports,
		&cookRequest, &repeatedCookReceipt ) &&
		!memcmp( &cookReceipt, &repeatedCookReceipt, sizeof( cookReceipt ) ) &&
		!memcmp( entitiesBefore, entities, sizeof( entitiesBefore ) ) &&
		!memcmp( lightmapBefore, lightmap, sizeof( lightmapBefore ) ) );
	free( sidecarBytes );
	RemoveTemporaryDirectory( temporaryRoot );
	RenderSubmission_Reset( &submission );
	puts( "render_lighting_project_source_test: ok" );
	return 0;
}
