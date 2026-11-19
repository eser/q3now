// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "render_material_project_cook.h"
#include "render_world_material_usage.h"
#include "render_image_decode.h"
#include "material_cook_fs.h"
#include "maps/map_format_registry.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define CHECK(x) do { if ( !(x) ) { fprintf( stderr, "FAIL %s:%d: %s\n", \
	__FILE__, __LINE__, #x ); return 1; } } while ( 0 )

static qboolean Resolve( const char *path, char *canonical,
		size_t capacity, uint64_t *sourceId, uint64_t *size,
		unsigned *generation ) {
	uint64_t hash = UINT64_C( 14695981039346656037 );
	if ( !path || !path[0] || strstr( path, "missing" )
			|| strlen( path ) >= capacity ) return qfalse;
	for ( const unsigned char *p = (const unsigned char *)path; *p; ++p )
		hash = ( hash ^ *p ) * UINT64_C( 1099511628211 );
	(void)snprintf( canonical, capacity, "%s%s", path,
		strstr( path, ".png" ) ? "" : ".png" );
	if ( sourceId ) *sourceId = hash ? hash : 1u;
	if ( size ) *size = 16u;
	if ( generation ) *generation = 7u;
	return qtrue;
}

static unsigned s_logCalls;
static char s_logText[1024];

static void QDECL Log( log_severity_t severity, const char *format, ... ) {
	va_list args;
	if ( severity != SEV_INFO || !format ) return;
	va_start( args, format );
	(void)vsnprintf( s_logText, sizeof( s_logText ), format, args );
	va_end( args );
	++s_logCalls;
}

qboolean RenderImage_DecodeRgba8( const char *name, byte **outPixels,
		uint32_t *outWidth, uint32_t *outHeight,
		char outResolved[MAX_QPATH] ) {
	byte *pixels;
	if ( !name || strstr( name, "missing" ) || !outPixels || !outWidth
			|| !outHeight || !outResolved ) return qfalse;
	pixels = malloc( 16u );
	if ( !pixels ) return qfalse;
	for ( uint32_t i = 0u; i < 16u; ++i ) pixels[i] = 255u;
	pixels[3] = 192u;
	*outPixels = pixels;
	*outWidth = *outHeight = 2u;
	(void)snprintf( outResolved, MAX_QPATH, "%s%s", name,
		strstr( name, ".png" ) ? "" : ".png" );
	return qtrue;
}

static renderMaterialScriptEntry_t Scripted( void ) {
	renderMaterialScriptEntry_t entry;
	memset( &entry, 0, sizeof( entry ) );
	(void)snprintf( entry.name, sizeof( entry.name ), "textures/test/scripted" );
	(void)snprintf( entry.declarationPath, sizeof( entry.declarationPath ),
		"scripts/test.shader" );
	entry.declarationSourceId = 100u;
	entry.declarationSize = 64u;
	entry.declarationGeneration = 7u;
	entry.declarationLine = entry.declarationColumn = 1u;
	entry.cullMode = RENDER_CULL_BACK;
	entry.depthWrite = qtrue;
	entry.sort = RENDER_MATERIAL_SORT_OPAQUE;
	entry.stageCount = 1u;
	(void)snprintf( entry.stages[0].imageName,
		sizeof( entry.stages[0].imageName ), "textures/test/scripted" );
	entry.stages[0].imageSource = RENDER_MATERIAL_STAGE_IMAGE;
	entry.stages[0].tcGen = RENDER_MATERIAL_TCGEN_TEXTURE;
	entry.stages[0].sourceBlend = RENDER_MATERIAL_BLEND_ONE;
	entry.stages[0].destinationBlend = RENDER_MATERIAL_BLEND_ZERO;
	entry.stages[0].depthWrite = qtrue;
	entry.stages[0].scale[0] = entry.stages[0].scale[1] = 1.0f;
	entry.stages[0].sourceLine = entry.stages[0].sourceColumn = 2u;
	return entry;
}

static void RemoveProducts( const char *root,
		const ralMaterialPackageReceipt_t *manifest,
		const ralMaterialProjectReferenceReceipt_t *project ) {
	char key[RAL_MATERIAL_PACKAGE_KEY_TEXT_CAPACITY];
	char path[WIRED_MATERIAL_COOK_FS_PATH_CAPACITY];
	for ( uint32_t i = 0u; i < manifest->entryCount; ++i ) {
		if ( Ral_MaterialCookCacheKeyText(
				manifest->entries[i].cookedArtifactHash, key, sizeof( key ) ) ) {
			(void)snprintf( path, sizeof( path ), "%s/%s.wrmat", root, key );
			(void)unlink( path );
		}
	}
	if ( Ral_MaterialPackageCacheKeyText( manifest->artifactHash, key,
			sizeof( key ) ) ) {
		(void)snprintf( path, sizeof( path ), "%s/%s.wrmatpkg", root, key );
		(void)unlink( path );
	}
	if ( Ral_MaterialProjectReferenceKeyText( project->mapName,
			project->mapChecksum, key, sizeof( key ) ) ) {
		(void)snprintf( path, sizeof( path ), "%s/%s.wrmatref", root, key );
		(void)unlink( path );
	}
}

int main( void ) {
	/* Cooker and every backend share one surface/fog reachability contract.
	 * Rejections must leave the caller's mask untouched. */
	{
		dshader_t shaders[3] = {0};
		dsurface_t surface = {0};
		dfog_t fog = {0};
		mapFile_t map = {0};
		byte used[3] = {9, 9, 9}, before[3];
		strcpy( shaders[0].shader, "textures/surface" );
		strcpy( shaders[1].shader, "textures/unused" );
		strcpy( shaders[2].shader, "textures/fog" );
		strcpy( fog.shader, "TEXTURES/FOG" );
		map.numShaders = 3; map.shaders = shaders;
		map.numSurfaces = 1; map.surfaces = &surface;
		map.numFogs = 1; map.fogs = &fog;
		CHECK( RenderWorldMaterialUsage_Build( &map, used, sizeof( used ) ) );
		CHECK( used[0] == 1 && used[1] == 0 && used[2] == 1 );
		memcpy( before, used, sizeof( used ) );
		CHECK( !RenderWorldMaterialUsage_Build( &map, used, 2 ) );
		CHECK( !memcmp( used, before, sizeof( used ) ) );
		surface.shaderNum = 3;
		CHECK( !RenderWorldMaterialUsage_Build( &map, used, sizeof( used ) ) );
		CHECK( !memcmp( used, before, sizeof( used ) ) );
		surface.shaderNum = 0;
		map.fogs = NULL;
		CHECK( !RenderWorldMaterialUsage_Build( &map, used, sizeof( used ) ) );
		CHECK( !memcmp( used, before, sizeof( used ) ) );
		map.fogs = &fog; map.surfaces = NULL;
		CHECK( !RenderWorldMaterialUsage_Build( &map, used, sizeof( used ) ) );
		CHECK( !memcmp( used, before, sizeof( used ) ) );
		map.surfaces = &surface; map.shaders = NULL;
		CHECK( !RenderWorldMaterialUsage_Build( &map, used, sizeof( used ) ) );
		CHECK( !memcmp( used, before, sizeof( used ) ) );
	}
	char root[] = "/tmp/wired-material-project-XXXXXX";
	refimport_t imports;
	renderMaterialScriptCatalog_t catalog;
	mapFile_t map;
	dshader_t shaders[5];
	dsurface_t surfaces[3] = {{0}};
	renderMaterialProjectCookReceipt_t receipt, before;
	renderMaterialProjectCookDiagnostic_t diagnostic;
	wiredMaterialCookFilesystem_t filesystem;
	ralMaterialPackageReceipt_t *manifest;
	ralMaterialProjectReferenceReceipt_t project, projectBefore;
	ralMaterialCookReceipt_t *cooked;
	CHECK( mkdtemp( root ) != NULL );
	memset( &imports, 0, sizeof( imports ) );
	imports.FS_ResolveResource = Resolve;
	imports.Free = free;
	imports.Log = Log;
	memset( &catalog, 0, sizeof( catalog ) );
	catalog.entries[0] = Scripted();
	/* Exercise the real map -> cooker -> cache -> reload path for a Q3 sky,
	 * including textures not referenced by an ordinary cloud stage. */
	catalog.entries[0].sky = qtrue;
	catalog.entries[0].skyCloudHeight = 384.0f;
	(void)snprintf( catalog.entries[0].skyBoxPrefix, MAX_QPATH, "env/sky" );
	catalog.entries[0].skySource.parmsSpan =
		(ralMaterialSourceSpan_t){ 100u, 3u, 1u };
	catalog.entries[0].skySource.hasSun = qtrue;
	catalog.entries[0].skySource.sunColor[0] = 1.0f;
	catalog.entries[0].skySource.sunIntensity = 120.0f;
	catalog.entries[0].skySource.sunAzimuthDegrees = 270.0f;
	catalog.entries[0].skySource.sunElevationDegrees = 35.0f;
	catalog.count = 1u;
	catalog.ready = qtrue;
	memset( &map, 0, sizeof( map ) );
	memset( shaders, 0, sizeof( shaders ) );
	(void)snprintf( map.name, sizeof( map.name ), "maps/test.bsp" );
	map.checksum = 0x12345678;
	map.numShaders = 5;
	map.shaders = shaders;
	map.numSurfaces = 3;
	map.surfaces = surfaces;
	for ( unsigned i = 0; i < 3; ++i ) {
		surfaces[i].shaderNum = i;
		surfaces[i].surfaceType = MST_PLANAR;
	}
	(void)snprintf( shaders[0].shader, sizeof( shaders[0].shader ),
		"textures/test/scripted" );
	(void)snprintf( shaders[1].shader, sizeof( shaders[1].shader ),
		"textures/test/implicit" );
	(void)snprintf( shaders[2].shader, sizeof( shaders[2].shader ),
		"textures/test/scripted" );
	(void)snprintf( shaders[3].shader, sizeof( shaders[3].shader ), "noshader" );
	/* BSP compilers retain unused shader-table rows. They must not demand
	 * assets or prevent publication: arena1 contains this exact pattern. */
	(void)snprintf( shaders[4].shader, sizeof( shaders[4].shader ),
		"textures/test/missing-unused" );
	CHECK( RenderMaterialProjectCook_Execute( &catalog, &map, &imports,
		root, 19u, &receipt, &diagnostic ) );
	CHECK( diagnostic.reason == RENDER_MATERIAL_PROJECT_COOK_OK );
	CHECK( RenderMaterialProjectCook_ReceiptValid( &receipt ) );
	CHECK( receipt.mapShaderCount == 5u && receipt.cookedMaterialCount == 2u );
	CHECK( receipt.materialVariantCount >= 1u
		&& receipt.materialVariantCount <= receipt.cookedMaterialCount );
	CHECK( receipt.legacyStageCount == 2u );
	CHECK( receipt.legacyVariantCount >= 1u
		&& receipt.legacyVariantCount <= receipt.legacyStageCount );
	CHECK( receipt.fallbackStageCount <= receipt.legacyStageCount );
	CHECK( receipt.proxyProgramCount == 0u );
	CHECK( receipt.mapChecksum == (uint32_t)map.checksum );
	CHECK( RenderMaterialProjectCook_LogTelemetry( &receipt, &imports ) );
	CHECK( s_logCalls == 1u
		&& strstr( s_logText, "shaders=5 products=2" )
		&& strstr( s_logText, "variants=" )
		&& strstr( s_logText, "fallbacks=" )
		&& strstr( s_logText, "manifest=" ) );
	before = receipt;
	before.ready = qfalse;
	CHECK( !RenderMaterialProjectCook_LogTelemetry( &before, &imports )
		&& s_logCalls == 1u );
	CHECK( WiredMaterialCookFilesystem_Init( &filesystem, root ) );
	CHECK( Ral_MaterialProjectReferenceLoad(
		WiredMaterialCookFilesystem_ProjectReferenceOps(), &filesystem,
		map.name, (uint32_t)map.checksum, &project ) );
	CHECK( project.manifestArtifactHash == receipt.manifestArtifactHash
		&& project.corpusHash == receipt.corpusHash
		&& project.artifactHash == receipt.projectReferenceArtifactHash );
	manifest = calloc( 1u, sizeof( *manifest ) );
	cooked = calloc( 1u, sizeof( *cooked ) );
	CHECK( manifest && cooked );
	CHECK( Ral_MaterialPackageCacheLoad(
		WiredMaterialCookFilesystem_PackageOps(), &filesystem,
		receipt.manifestArtifactHash, manifest ) );
	CHECK( manifest->entryCount == 2u );
	uint32_t skyProducts = 0u;
	for ( uint32_t i = 0u; i < manifest->entryCount; ++i ) {
		CHECK( Ral_MaterialCookCacheLoad(
			WiredMaterialCookFilesystem_MaterialOps(), &filesystem,
			manifest->entries[i].cookedArtifactHash, cooked ) );
		if ( cooked->source.source.sky ) {
			const ralMaterialSourceIr_t *source = &cooked->source.source;
			++skyProducts;
			CHECK( source->skySource.outerBox && source->skySource.hasSun );
			CHECK( source->skySource.sunIntensity == 120.0f
				&& source->skySource.sunAzimuthDegrees == 270.0f );
			CHECK( source->dependencyCount == 7u && cooked->bindingCount == 7u );
			for ( uint32_t face = 0u; face < RAL_MATERIAL_SKY_FACE_COUNT; ++face )
				CHECK( cooked->bindings[source->skySource.outerDependencies[face]]
					.texture.keyHash != 0u );
		}
	}
	CHECK( skyProducts == 1u );
	before = receipt;
	projectBefore = project;
	surfaces[0].shaderNum = 5;
	CHECK( !RenderMaterialProjectCook_Execute( &catalog, &map, &imports,
		root, 19u, &receipt, &diagnostic ) );
	CHECK( diagnostic.reason == RENDER_MATERIAL_PROJECT_COOK_INVALID_ARGUMENT );
	CHECK( !memcmp( &receipt, &before, sizeof( receipt ) ) );
	surfaces[0].shaderNum = 0;
	dfog_t fog = {0};
	(void)snprintf( fog.shader, sizeof( fog.shader ), "%s", shaders[4].shader );
	map.numFogs = 1;
	map.fogs = &fog;
	CHECK( !RenderMaterialProjectCook_Execute( &catalog, &map, &imports,
		root, 19u, &receipt, &diagnostic ) );
	CHECK( diagnostic.reason == RENDER_MATERIAL_PROJECT_COOK_SOURCE_REJECTED
		&& diagnostic.shaderIndex == 4u );
	CHECK( !memcmp( &receipt, &before, sizeof( receipt ) ) );
	map.numFogs = 0;
	map.fogs = NULL;
	(void)snprintf( shaders[1].shader, sizeof( shaders[1].shader ),
		"textures/test/missing" );
	CHECK( !RenderMaterialProjectCook_Execute( &catalog, &map, &imports,
		root, 19u, &receipt, &diagnostic ) );
	CHECK( diagnostic.reason == RENDER_MATERIAL_PROJECT_COOK_SOURCE_REJECTED );
	CHECK( !memcmp( &receipt, &before, sizeof( receipt ) ) );
	CHECK( Ral_MaterialProjectReferenceLoad(
		WiredMaterialCookFilesystem_ProjectReferenceOps(), &filesystem,
		map.name, (uint32_t)map.checksum, &project ) );
	CHECK( !memcmp( &project, &projectBefore, sizeof( project ) ) );
	RemoveProducts( root, manifest, &project );
	free( cooked );
	free( manifest );
	CHECK( !rmdir( root ) );
	puts( "render_material_project_cook_test: ok" );
	return 0;
}
