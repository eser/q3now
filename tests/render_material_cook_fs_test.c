// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "material_cook_fs.h"
#include "render_material_cache_vfs.h"
#include "render_material_package.h"
#include "render_material_runtime_owner.h"
#include "maps/map_format_registry.h"

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
#include <pthread.h>
#include <sys/stat.h>
#include <unistd.h>
#define MKDIR(path) mkdir( path, 0700 )
#define RMDIR(path) rmdir( path )
#define UNLINK(path) unlink( path )
#endif

#define CHECK(x) do { if ( !(x) ) { fprintf( stderr, "FAIL %s:%d: %s\n", \
	__FILE__, __LINE__, #x ); return 1; } } while ( 0 )

static const void *vfsMaterialBytes, *vfsPackageBytes, *vfsProjectBytes;
static char vfsMaterialPath[MAX_QPATH], vfsPackagePath[MAX_QPATH],
	vfsProjectPath[MAX_QPATH];
static unsigned vfsGeneration = 11u, vfsReads, vfsFrees, vfsResolves;
static qboolean vfsMutateAfterRead;
static ralMaterialRuntimeResource_t residentResource;
static qboolean rejectResidentResource;

static qboolean ResolveResidentResource( void *context, uint64_t cacheKeyHash,
		ralMaterialRuntimeResource_t *outResource ) {
	(void)context;
	if ( rejectResidentResource || !outResource
			|| residentResource.cacheKeyHash != cacheKeyHash ) return qfalse;
	*outResource = residentResource;
	return qtrue;
}

static qboolean VfsResolve( const char *path, char *canonical, size_t capacity,
		uint64_t *sourceId, uint64_t *byteSize, unsigned *generation ) {
	vfsResolves++;
	if ( !strcmp( path, vfsMaterialPath ) ) {
		(void)snprintf( canonical, capacity, "%s", vfsMaterialPath );
		if ( sourceId ) *sourceId = 901u;
		if ( byteSize ) *byteSize = sizeof( ralMaterialCookReceipt_t );
	} else if ( !strcmp( path, vfsPackagePath ) ) {
		(void)snprintf( canonical, capacity, "%s", vfsPackagePath );
		if ( sourceId ) *sourceId = 902u;
		if ( byteSize ) *byteSize = sizeof( ralMaterialPackageReceipt_t );
	} else if ( !strcmp( path, vfsProjectPath ) ) {
		(void)snprintf( canonical, capacity, "%s", vfsProjectPath );
		if ( sourceId ) *sourceId = 903u;
		if ( byteSize ) *byteSize =
			sizeof( ralMaterialProjectReferenceReceipt_t );
	} else return qfalse;
	if ( generation ) *generation = vfsGeneration;
	return qtrue;
}

static int VfsRead( const char *path, void **out ) {
	vfsReads++;
	if ( !strcmp( path, vfsMaterialPath ) ) {
		*out = (void *)vfsMaterialBytes;
		if ( vfsMutateAfterRead ) vfsGeneration++;
		return (int)sizeof( ralMaterialCookReceipt_t );
	}
	if ( !strcmp( path, vfsPackagePath ) ) {
		*out = (void *)vfsPackageBytes;
		if ( vfsMutateAfterRead ) vfsGeneration++;
		return (int)sizeof( ralMaterialPackageReceipt_t );
	}
	if ( !strcmp( path, vfsProjectPath ) ) {
		*out = (void *)vfsProjectBytes;
		if ( vfsMutateAfterRead ) vfsGeneration++;
		return (int)sizeof( ralMaterialProjectReferenceReceipt_t );
	}
	return -1;
}

static void VfsFree( void *bytes ) {
	if ( bytes == vfsMaterialBytes || bytes == vfsPackageBytes
			|| bytes == vfsProjectBytes ) vfsFrees++;
}

static int TemporaryRoot( char out[WIRED_MATERIAL_COOK_FS_PATH_CAPACITY] ) {
#if defined( _WIN32 )
	char base[MAX_PATH];
	if ( !GetTempPathA( sizeof( base ), base ) ) return 0;
	(void)snprintf( out, WIRED_MATERIAL_COOK_FS_PATH_CAPACITY,
		"%swired-material-%lu", base, (unsigned long)GetCurrentProcessId() );
	return MKDIR( out ) == 0;
#else
	strcpy( out, "/tmp/wired-material-cook-XXXXXX" );
	return mkdtemp( out ) != NULL;
#endif
}

static ralMaterialSourceIr_t Source( void ) {
	ralMaterialSourceIr_t source;
	memset( &source, 0, sizeof( source ) );
	source.schemaVersion = RAL_MATERIAL_SOURCE_IR_SCHEMA_VERSION;
	source.generation = 12u;
	source.provenance = RAL_MATERIAL_PROVENANCE_WIRED_LUA;
	source.declaration.sourceId = 800u;
	source.declaration.line = source.declaration.column = 1u;
	(void)snprintf( source.semanticName, sizeof( source.semanticName ),
		"materials/package/physical" );
	source.diffuseReflectance[0] = source.diffuseReflectance[1] =
		source.diffuseReflectance[2] = 1.0f;
	source.pbrFactors.baseColor[0] = source.pbrFactors.baseColor[1] =
		source.pbrFactors.baseColor[2] = source.pbrFactors.baseColor[3] = 1.0f;
	source.pbrFactors.roughness = source.pbrFactors.normalScale =
		source.pbrFactors.occlusionStrength = 1.0f;
	source.pbrRenderPolicy.depthTest = source.pbrRenderPolicy.depthWrite = qtrue;
	source.dependencyCount = source.pbrTextureCount = 1u;
	source.dependencies[0].role = RAL_MATERIAL_DEPENDENCY_TEXTURE;
	source.dependencies[0].span = source.declaration;
	source.dependencies[0].sourceId = 801u;
	source.dependencies[0].fsGeneration = 3u;
	source.dependencies[0].size = 256u;
	source.dependencies[0].creationOptionsHash = 19u;
	(void)snprintf( source.dependencies[0].canonicalPath,
		sizeof( source.dependencies[0].canonicalPath ),
		"textures/package/physical.ktx2" );
	source.pbrTextures[0].span = source.declaration;
	source.pbrTextures[0].dependencyIndex = 0u;
	source.pbrTextures[0].role = RAL_MATERIAL_TEXTURE_BASE_COLOR;
	source.pbrTextures[0].channels = RAL_MATERIAL_CHANNEL_RGBA;
	return source;
}

static qboolean Bind( void *opaque, uint32_t index,
		const ralMaterialSourceDependency_t *dependency,
		ralMaterialCookBinding_t *out ) {
	ralTextureAssetRequest_t request;
	ralTextureAssetReceipt_t receipt;
	(void)opaque;
	memset( &request, 0, sizeof( request ) );
	request.schemaVersion = RAL_TEXTURE_ASSET_SCHEMA_VERSION;
	request.assetGeneration = 22u;
	request.provenanceHash = dependency->sourceId;
	request.dimension = RAL_TEXTURE_ASSET_2D;
	request.width = request.height = 8u;
	request.depth = request.layers = request.sourceMipLevels = 1u;
	request.colorEncoding = RAL_TEXTURE_ENCODING_SRGB;
	request.channelSemantic = RAL_TEXTURE_CHANNEL_COLOR;
	request.sourceEncoding = RAL_TEXTURE_SOURCE_UNCOMPRESSED;
	request.mipPolicy = RAL_TEXTURE_MIPS_SOURCE;
	request.residency = RAL_TEXTURE_RESIDENCY_STREAMED;
	request.preferenceCount = 1u;
	request.preferences[0] = RAL_TEXTURE_COMPRESSION_UNCOMPRESSED;
	request.allowUncompressedFallback = qtrue;
	if ( !Ral_ResolveTextureAsset( &request, &receipt ) ) return qfalse;
	memset( out, 0, sizeof( *out ) );
	out->dependencyIndex = index;
	out->role = dependency->role;
	out->sourceId = dependency->sourceId;
	out->fsGeneration = dependency->fsGeneration;
	out->creationOptionsHash = dependency->creationOptionsHash;
	if ( !Ral_TextureCacheKeyBuild( &receipt, &out->texture ) ) return qfalse;
	out->artifactHash = out->texture.keyHash;
	return qtrue;
}

typedef struct {
	const ralMaterialCookCacheOps_t *ops;
	wiredMaterialCookFilesystem_t *filesystem;
	const ralMaterialCookReceipt_t *receipt;
	int failure;
} writerContext_t;

typedef struct {
	const ralMaterialProjectReferenceReceipt_t *project;
	renderMaterialCacheVfs_t *cache;
	ralMaterialCookReceipt_t *receipt;
	renderMaterialPackageDiagnostic_t diagnostic;
	int failure;
} smallStackLoadContext_t;

static int WriterBody( writerContext_t *context ) {
	if ( !Ral_MaterialCookCacheStore( context->ops, context->filesystem,
			context->receipt ) ) return 1;
	return 0;
}

#if defined( _WIN32 )
static DWORD WINAPI Writer( LPVOID opaque ) {
	writerContext_t *context = opaque;
	context->failure = WriterBody( context );
	return 0;
}
#else
static void *Writer( void *opaque ) {
	writerContext_t *context = opaque;
	context->failure = WriterBody( context );
	return NULL;
}
#endif

static int SmallStackLoadBody( smallStackLoadContext_t *context ) {
	return RenderMaterialPackage_LoadProjectCooked( context->project->mapName,
		context->project->mapChecksum, "materials/package/physical",
		RenderMaterialCacheVfs_ProjectReferenceOps(), context->cache,
		RenderMaterialCacheVfs_PackageOps(), context->cache,
		RenderMaterialCacheVfs_MaterialOps(), context->cache, NULL,
		context->receipt, &context->diagnostic ) ? 0 : 1;
}

#if defined( _WIN32 )
static DWORD WINAPI SmallStackLoad( LPVOID opaque ) {
	smallStackLoadContext_t *context = opaque;
	context->failure = SmallStackLoadBody( context );
	return 0;
}
#else
static void *SmallStackLoad( void *opaque ) {
	smallStackLoadContext_t *context = opaque;
	context->failure = SmallStackLoadBody( context );
	return NULL;
}
#endif

static int LoadOnBoundedWorkerStack( smallStackLoadContext_t *context ) {
	/* This is deliberately smaller than the former nested 40--116 KiB
	 * automatic receipts. Production workers must not depend on a large stack. */
	enum { WORKER_STACK_BYTES = 128 * 1024 };
#if defined( _WIN32 )
	HANDLE thread = CreateThread( NULL, WORKER_STACK_BYTES, SmallStackLoad,
		context, STACK_SIZE_PARAM_IS_A_RESERVATION, NULL );
	if ( !thread ) return 1;
	if ( WaitForSingleObject( thread, INFINITE ) != WAIT_OBJECT_0 ) {
		CloseHandle( thread );
		return 1;
	}
	CloseHandle( thread );
#else
	pthread_attr_t attributes;
	pthread_t thread;
	size_t stackBytes = WORKER_STACK_BYTES;
#if defined( PTHREAD_STACK_MIN )
	if ( stackBytes < (size_t)PTHREAD_STACK_MIN )
		stackBytes = (size_t)PTHREAD_STACK_MIN;
#endif
	if ( pthread_attr_init( &attributes ) ) return 1;
	if ( pthread_attr_setstacksize( &attributes, stackBytes ) ) {
		pthread_attr_destroy( &attributes );
		return 1;
	}
	if ( pthread_create( &thread, &attributes, SmallStackLoad, context ) ) {
		pthread_attr_destroy( &attributes );
		return 1;
	}
	pthread_attr_destroy( &attributes );
	if ( pthread_join( thread, NULL ) ) return 1;
#endif
	return context->failure;
}

static int ConcurrentIdenticalPublish( const ralMaterialCookCacheOps_t *ops,
		wiredMaterialCookFilesystem_t *filesystem,
		const ralMaterialCookReceipt_t *receipt ) {
	enum { THREAD_COUNT = 8 };
	writerContext_t contexts[THREAD_COUNT];
#if defined( _WIN32 )
	HANDLE threads[THREAD_COUNT];
#else
	pthread_t threads[THREAD_COUNT];
#endif
	memset( contexts, 0, sizeof( contexts ) );
	for ( unsigned i = 0u; i < THREAD_COUNT; ++i ) {
		contexts[i].ops = ops;
		contexts[i].filesystem = filesystem;
		contexts[i].receipt = receipt;
#if defined( _WIN32 )
		threads[i] = CreateThread( NULL, 0, Writer, &contexts[i], 0, NULL );
		if ( !threads[i] ) return 1;
#else
		if ( pthread_create( &threads[i], NULL, Writer, &contexts[i] ) ) return 1;
#endif
	}
	for ( unsigned i = 0u; i < THREAD_COUNT; ++i ) {
#if defined( _WIN32 )
		if ( WaitForSingleObject( threads[i], INFINITE ) != WAIT_OBJECT_0 ) return 1;
		CloseHandle( threads[i] );
#else
		if ( pthread_join( threads[i], NULL ) ) return 1;
#endif
		if ( contexts[i].failure ) return contexts[i].failure;
	}
	return 0;
}

int main( void ) {
	static ralMaterialSourceIr_t ir;
	static ralMaterialSourceReceipt_t source;
	static ralMaterialPackageReceipt_t manifest;
	static ralMaterialProjectReferenceReceipt_t project, physicalProject;
	static ralMaterialProjectReferenceReceipt_t vfsProject, vfsProjectBefore;
	static ralMaterialCookReceipt_t loaded, concurrentLoaded;
	static ralMaterialCookReceipt_t vfsLoaded, boundedStackLoaded, vfsBefore;
	static renderMaterialPackageDiagnostic_t diagnostic;
	static smallStackLoadContext_t boundedStackContext;
	wiredMaterialCookFilesystem_t filesystem;
	char root[WIRED_MATERIAL_COOK_FS_PATH_CAPACITY];
	char materialKey[RAL_MATERIAL_COOK_CACHE_KEY_TEXT_CAPACITY];
	char packageKey[RAL_MATERIAL_PACKAGE_KEY_TEXT_CAPACITY];
	char projectKey[RAL_MATERIAL_PROJECT_REFERENCE_KEY_TEXT_CAPACITY];
	char materialPath[WIRED_MATERIAL_COOK_FS_PATH_CAPACITY];
	char packagePath[WIRED_MATERIAL_COOK_FS_PATH_CAPACITY];
	char projectPath[WIRED_MATERIAL_COOK_FS_PATH_CAPACITY];
	uint64_t rejectedLength = 0u;
	const ralMaterialCookCacheOps_t *materialOps;
	const ralMaterialPackageCacheOps_t *packageOps;
	const ralMaterialProjectReferenceOps_t *projectOps;
	refimport_t imports;
	renderMaterialCacheVfs_t vfsCache;
	renderMaterialRuntimeOwner_t runtimeOwner;
	renderMaterialRuntimeCatalog_t siblingBefore;
	mapFile_t runtimeMap;

	ir = Source();
	CHECK( TemporaryRoot( root ) );
	CHECK( WiredMaterialCookFilesystem_Init( &filesystem, root ) );
	materialOps = WiredMaterialCookFilesystem_MaterialOps();
	packageOps = WiredMaterialCookFilesystem_PackageOps();
	projectOps = WiredMaterialCookFilesystem_ProjectReferenceOps();
	CHECK( Ral_MaterialSourceCompile( &ir, &source, NULL ) );
	CHECK( RenderMaterialPackage_CookAndPublish( 7u, &source, 1u, Bind, NULL,
		materialOps, &filesystem, packageOps, &filesystem, &manifest,
		&diagnostic ) );
	CHECK( RenderMaterialPackage_LoadCooked( manifest.artifactHash,
		"materials/package/physical", packageOps, &filesystem, materialOps,
		&filesystem, &loaded, &diagnostic ) );
	CHECK( Ral_MaterialProjectReferenceSeal( "maps/physical.bsp", 0x1234u,
		7u, manifest.entryCount, UINT64_C( 0x5678 ), manifest.artifactHash,
		&project ) );
	CHECK( Ral_MaterialProjectReferenceStore( projectOps, &filesystem,
		&project ) );
	CHECK( Ral_MaterialProjectReferenceLoad( projectOps, &filesystem,
		"maps/physical.bsp", 0x1234u, &physicalProject ) );
	CHECK( !memcmp( &project, &physicalProject, sizeof( project ) ) );
	CHECK( !Ral_MaterialProjectReferenceLoad( projectOps, &filesystem,
		"maps/physical.bsp", 0x1235u, &physicalProject ) );
	CHECK( ConcurrentIdenticalPublish( materialOps, &filesystem, &loaded ) == 0 );
	CHECK( Ral_MaterialCookCacheLoad( materialOps, &filesystem,
		loaded.artifactHash, &concurrentLoaded ) );
	CHECK( Ral_MaterialCookReceiptExact( &loaded, &concurrentLoaded ) );
	CHECK( !materialOps->writeAtomic( &filesystem, loaded.artifactHash,
		"../escape", &loaded, sizeof( loaded ) ) );
	CHECK( !packageOps->read( &filesystem, manifest.artifactHash,
		"wrmat-package-v1-0000000000000000", &concurrentLoaded,
		sizeof( concurrentLoaded ), &rejectedLength ) );

	CHECK( Ral_MaterialCookCacheKeyText( loaded.artifactHash, materialKey,
		sizeof( materialKey ) ) );
	CHECK( Ral_MaterialPackageCacheKeyText( manifest.artifactHash, packageKey,
		sizeof( packageKey ) ) );
	CHECK( Ral_MaterialProjectReferenceKeyText( project.mapName,
		project.mapChecksum, projectKey, sizeof( projectKey ) ) );
	CHECK( snprintf( materialPath, sizeof( materialPath ), "%s/%s.wrmat",
		root, materialKey ) > 0 );
	CHECK( snprintf( packagePath, sizeof( packagePath ), "%s/%s.wrmatpkg",
		root, packageKey ) > 0 );
	CHECK( snprintf( projectPath, sizeof( projectPath ), "%s/%s.wrmatref",
		root, projectKey ) > 0 );

	memset( &imports, 0, sizeof( imports ) );
	imports.FS_ResolveResource = VfsResolve;
	imports.FS_ReadFile = VfsRead;
	imports.FS_FreeFile = VfsFree;
	CHECK( snprintf( vfsMaterialPath, sizeof( vfsMaterialPath ),
		"materials/cooked/%s.wrmat", materialKey ) > 0 );
	CHECK( snprintf( vfsPackagePath, sizeof( vfsPackagePath ),
		"materials/cooked/%s.wrmatpkg", packageKey ) > 0 );
	CHECK( snprintf( vfsProjectPath, sizeof( vfsProjectPath ),
		"materials/cooked/%s.wrmatref", projectKey ) > 0 );
	vfsMaterialBytes = &loaded;
	vfsPackageBytes = &manifest;
	vfsProjectBytes = &project;
	CHECK( RenderMaterialCacheVfs_Init( &vfsCache, &imports,
		"materials/cooked" ) );
	CHECK( Ral_MaterialProjectReferenceLoad(
		RenderMaterialCacheVfs_ProjectReferenceOps(), &vfsCache,
		project.mapName, project.mapChecksum, &vfsProject ) );
	CHECK( !memcmp( &project, &vfsProject, sizeof( project ) ) );
	CHECK( RenderMaterialPackage_LoadProjectCooked( project.mapName,
		project.mapChecksum, "materials/package/physical",
		RenderMaterialCacheVfs_ProjectReferenceOps(), &vfsCache,
		RenderMaterialCacheVfs_PackageOps(), &vfsCache,
		RenderMaterialCacheVfs_MaterialOps(), &vfsCache, NULL,
		&vfsLoaded, &diagnostic ) );
	CHECK( Ral_MaterialCookReceiptExact( &loaded, &vfsLoaded ) );
	CHECK( vfsReads == 4u && vfsFrees == 4u && vfsResolves == 8u );
	boundedStackContext.project = &project;
	boundedStackContext.cache = &vfsCache;
	boundedStackContext.receipt = &boundedStackLoaded;
	CHECK( LoadOnBoundedWorkerStack( &boundedStackContext ) == 0 );
	CHECK( Ral_MaterialCookReceiptExact( &loaded, &boundedStackLoaded ) );
	CHECK( vfsReads == 7u && vfsFrees == 7u && vfsResolves == 14u );
	vfsProjectBefore = vfsProject;
	vfsMutateAfterRead = qtrue;
	CHECK( !Ral_MaterialProjectReferenceLoad(
		RenderMaterialCacheVfs_ProjectReferenceOps(), &vfsCache,
		project.mapName, project.mapChecksum, &vfsProject ) );
	CHECK( !memcmp( &vfsProject, &vfsProjectBefore, sizeof( vfsProject ) ) );
	CHECK( vfsReads == 8u && vfsFrees == 8u && vfsResolves == 16u );
	vfsBefore = vfsLoaded;
	CHECK( !RenderMaterialPackage_LoadCooked( manifest.artifactHash,
		"materials/package/physical", RenderMaterialCacheVfs_PackageOps(),
		&vfsCache, RenderMaterialCacheVfs_MaterialOps(), &vfsCache,
		&vfsLoaded, &diagnostic ) );
	CHECK( diagnostic.reason
		== RENDER_MATERIAL_PACKAGE_MANIFEST_LOAD_REJECTED );
	CHECK( !memcmp( &vfsLoaded, &vfsBefore, sizeof( vfsLoaded ) ) );
	CHECK( vfsReads == 9u && vfsFrees == 9u && vfsResolves == 18u );
	vfsMutateAfterRead = qfalse;
	memset( &residentResource, 0, sizeof( residentResource ) );
	residentResource.cacheKeyHash =
		loaded.material.material.textures[0].texture.keyHash;
	residentResource.resource.schemaVersion =
		RAL_TEXTURE_RESOURCE_RECEIPT_SCHEMA_VERSION;
	residentResource.resource.backendType = RAL_BACKEND_VULKAN;
	residentResource.resource.textureIdentity = (uintptr_t)0x7000u;
	residentResource.resource.resourceGeneration = 41u;
	residentResource.resource.type = RAL_TEXTURE_2D;
	residentResource.resource.format =
		loaded.material.material.textures[0].texture.asset.targetFormat;
	residentResource.resource.usage = (ralTextureUsage_t)(
		RAL_TEXTURE_USAGE_SAMPLED | RAL_TEXTURE_USAGE_TRANSFER_DST );
	residentResource.resource.width =
		loaded.material.material.textures[0].texture.asset.width;
	residentResource.resource.height =
		loaded.material.material.textures[0].texture.asset.height;
	residentResource.resource.mipLevels =
		loaded.material.material.textures[0].texture.asset.targetMipLevels;
	residentResource.resource.arrayLayers = 1u;
	residentResource.resource.ready = qtrue;
	memset( &runtimeMap, 0, sizeof( runtimeMap ) );
	(void)snprintf( runtimeMap.name, sizeof( runtimeMap.name ), "%s",
		project.mapName );
	runtimeMap.checksum = (int)project.mapChecksum;
	memset( &runtimeOwner, 0, sizeof( runtimeOwner ) );
	CHECK( RenderMaterialRuntimeOwner_Init( &runtimeOwner, &imports ) );
	CHECK( RenderMaterialRuntimeOwner_PublishWorld( &runtimeOwner, &runtimeMap,
		0, 71u, ResolveResidentResource, NULL, &diagnostic ) );
	CHECK( RenderMaterialRuntimeOwner_PublishWorld( &runtimeOwner, &runtimeMap,
		1, 81u, ResolveResidentResource, NULL, &diagnostic ) );
	CHECK( RenderMaterialRuntimeOwner_World( &runtimeOwner, 0 )
		&& RenderMaterialRuntimeOwner_World( &runtimeOwner, 1 ) );
	siblingBefore = *RenderMaterialRuntimeOwner_World( &runtimeOwner, 1 );
	RenderMaterialRuntimeOwner_UnloadWorld( &runtimeOwner, 0 );
	CHECK( !RenderMaterialRuntimeOwner_World( &runtimeOwner, 0 )
		&& RenderMaterialRuntimeOwner_World( &runtimeOwner, 1 )
		&& !memcmp( RenderMaterialRuntimeOwner_World( &runtimeOwner, 1 ),
			&siblingBefore, sizeof( siblingBefore ) ) );
	rejectResidentResource = qtrue;
	CHECK( !RenderMaterialRuntimeOwner_PublishWorld( &runtimeOwner, &runtimeMap,
		1, 91u, ResolveResidentResource, NULL, &diagnostic ) );
	CHECK( !memcmp( RenderMaterialRuntimeOwner_World( &runtimeOwner, 1 ),
		&siblingBefore, sizeof( siblingBefore ) ) );
	RenderMaterialRuntimeOwner_Destroy( &runtimeOwner );
	CHECK( runtimeOwner.ready == qfalse
		&& !RenderMaterialRuntimeOwner_World( &runtimeOwner, 1 ) );
	CHECK( !RenderMaterialCacheVfs_Init( &vfsCache, &imports, "../escape" ) );
	CHECK( UNLINK( materialPath ) == 0 && UNLINK( packagePath ) == 0
		&& UNLINK( projectPath ) == 0
		&& RMDIR( root ) == 0 );
	puts( "render_material_cook_fs_test: ok" );
	return 0;
}
