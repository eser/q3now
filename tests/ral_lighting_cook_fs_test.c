// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "lighting_cook_fs.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined( _WIN32 )
#include <direct.h>
#include <io.h>
#include <windows.h>
#define MKDIR(path) _mkdir( path )
#define RMDIR(path) _rmdir( path )
#define UNLINK(path) _unlink( path )
#else
#include <sys/stat.h>
#include <unistd.h>
#define MKDIR(path) mkdir( path, 0700 )
#define RMDIR(path) rmdir( path )
#define UNLINK(path) unlink( path )
#endif

#define CHECK(x) do { if ( !(x) ) { fprintf( stderr, "FAIL %s:%d: %s\n", \
	__FILE__, __LINE__, #x ); return 1; } } while ( 0 )

static int TemporaryRoot( char out[WIRED_LIGHTING_COOK_FS_PATH_CAPACITY] )
{
#if defined( _WIN32 )
	char base[MAX_PATH];
	if ( !GetTempPathA( sizeof( base ), base ) ) return 0;
	snprintf( out, WIRED_LIGHTING_COOK_FS_PATH_CAPACITY, "%swired-lighting-%lu",
		base, (unsigned long)GetCurrentProcessId() );
	return MKDIR( out ) == 0;
#else
	strcpy( out, "/tmp/wired-lighting-cook-XXXXXX" );
	return mkdtemp( out ) != NULL;
#endif
}

int main( void )
{
	static const unsigned char radiance[4] = { 1u, 2u, 3u, 4u };
	static const unsigned char direction[2] = { 5u, 6u };
	static const unsigned char sourceBytes[] = "authoritative-map";
	unsigned char artifact[512], loaded[512], sourceRead[sizeof( sourceBytes )];
	ralLightingArtifactDefinition_t definition;
	ralLightingPayloadView_t payloads[2];
	ralLightingArtifactReceipt_t written, stored, loadedReceipt;
	wiredLightingCookFilesystem_t filesystem;
	char root[WIRED_LIGHTING_COOK_FS_PATH_CAPACITY];
	char cacheRoot[WIRED_LIGHTING_COOK_FS_PATH_CAPACITY];
	char sourcePath[WIRED_LIGHTING_COOK_FS_PATH_CAPACITY];
	char cacheKey[RAL_LIGHTING_CACHE_KEY_TEXT_CAPACITY];
	char cachePath[WIRED_LIGHTING_COOK_FS_PATH_CAPACITY];
	char directionalPath[WIRED_LIGHTING_COOK_FS_PATH_CAPACITY];
	char irradiancePath[WIRED_LIGHTING_COOK_FS_PATH_CAPACITY];
	FILE *source;
	uint64_t loadedLength = 0u;
	CHECK( TemporaryRoot( root ) );
	CHECK( snprintf( cacheRoot, sizeof( cacheRoot ), "%s/derived", root ) > 0 );
	CHECK( MKDIR( cacheRoot ) == 0 );
	CHECK( snprintf( sourcePath, sizeof( sourcePath ), "%s/source.map", root ) > 0 );
	source = fopen( sourcePath, "wb" );
	CHECK( source && fwrite( sourceBytes, 1u, sizeof( sourceBytes ), source ) == sizeof( sourceBytes ) &&
		fclose( source ) == 0 );
	memset( &definition, 0, sizeof( definition ) );
	definition.schemaVersion = RAL_LIGHTING_ARTIFACT_SCHEMA_VERSION;
	definition.kind = RAL_LIGHTING_ARTIFACT_DIRECTIONAL_LIGHTMAP;
	definition.artifactGeneration = 71u; definition.cacheKey = 72u;
	definition.producerVersion = 73u;
	definition.encoding = RAL_STATIC_LIGHTING_ENCODING_RGB9E5_OCT8;
	definition.flags = RAL_LIGHTING_ARTIFACT_STATIC_DIFFUSE_INDIRECT_ONLY;
	definition.dimensions[0] = definition.dimensions[1] = definition.dimensions[2] = 1u;
	definition.payloadCount = 2u;
	payloads[0] = (ralLightingPayloadView_t){ RAL_LIGHTING_PAYLOAD_RADIANCE,
		radiance, sizeof( radiance ) };
	payloads[1] = (ralLightingPayloadView_t){ RAL_LIGHTING_PAYLOAD_DIRECTION,
		direction, sizeof( direction ) };
	CHECK( Ral_LightingArtifactWrite( &definition, payloads, artifact,
		sizeof( artifact ), &written ) );
	CHECK( WiredLightingCookFilesystem_Init( &filesystem, cacheRoot ) );
	CHECK( Ral_LightingArtifactCacheStore( WiredLightingCookFilesystem_Ops(),
		&filesystem, artifact, written.byteLength, definition.kind,
		definition.artifactGeneration, definition.cacheKey, &stored ) );
	CHECK( Ral_LightingArtifactCacheLoad( WiredLightingCookFilesystem_Ops(),
		&filesystem, definition.kind, definition.artifactGeneration,
		definition.cacheKey, loaded, sizeof( loaded ), &loadedLength, &loadedReceipt ) &&
		loadedLength == written.byteLength &&
		Ral_LightingArtifactReceiptExact( &written, &loadedReceipt ) &&
		!memcmp( artifact, loaded, (size_t)loadedLength ) );
	CHECK( WiredLightingCookFilesystem_PublishSidecar( &filesystem, "source",
		RAL_LIGHTING_ARTIFACT_DIRECTIONAL_LIGHTMAP, artifact, written.byteLength ) );
	loadedLength = 0u;
	CHECK( WiredLightingCookFilesystem_ReadSidecar( &filesystem, "source",
		RAL_LIGHTING_ARTIFACT_DIRECTIONAL_LIGHTMAP, loaded, sizeof( loaded ),
		&loadedLength ) && loadedLength == written.byteLength &&
		!memcmp( artifact, loaded, (size_t)loadedLength ) );
	CHECK( WiredLightingCookFilesystem_PublishSidecar( &filesystem, "source",
		RAL_LIGHTING_ARTIFACT_IRRADIANCE_VOLUME, artifact, written.byteLength ) );
	CHECK( !WiredLightingCookFilesystem_PublishSidecar( &filesystem, "../source",
		RAL_LIGHTING_ARTIFACT_DIRECTIONAL_LIGHTMAP, artifact, written.byteLength ) );
	source = fopen( sourcePath, "rb" );
	CHECK( source && fread( sourceRead, 1u, sizeof( sourceRead ), source ) == sizeof( sourceRead ) &&
		fclose( source ) == 0 && !memcmp( sourceRead, sourceBytes, sizeof( sourceBytes ) ) );
	CHECK( Ral_LightingArtifactCacheKeyText( definition.kind,
		definition.artifactGeneration, definition.cacheKey, cacheKey, sizeof( cacheKey ) ) );
	CHECK( snprintf( cachePath, sizeof( cachePath ), "%s/%s.wrcache",
		cacheRoot, cacheKey ) > 0 );
	CHECK( snprintf( directionalPath, sizeof( directionalPath ), "%s/source.wlight",
		cacheRoot ) > 0 );
	CHECK( snprintf( irradiancePath, sizeof( irradiancePath ), "%s/source.wprobe",
		cacheRoot ) > 0 );
	CHECK( UNLINK( cachePath ) == 0 && UNLINK( directionalPath ) == 0 &&
		UNLINK( irradiancePath ) == 0 && UNLINK( sourcePath ) == 0 &&
		RMDIR( cacheRoot ) == 0 && RMDIR( root ) == 0 );
	puts( "ral_lighting_cook_fs_test: ok" );
	return 0;
}
