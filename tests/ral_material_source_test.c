// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "ral_material_source.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#if defined(_WIN32)
#include <windows.h>
#else
#include <pthread.h>
#endif

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL %s:%d: %s\n", \
	__FILE__, __LINE__, #x); return 1; } } while (0)

static ralMaterialSourceIr_t Base( void ) {
	ralMaterialSourceIr_t source;
	memset( &source, 0, sizeof( source ) );
	source.schemaVersion = RAL_MATERIAL_SOURCE_IR_SCHEMA_VERSION;
	source.generation = 3u;
	source.provenance = RAL_MATERIAL_PROVENANCE_Q3_SHADER;
	source.declaration.sourceId = 101u;
	source.declaration.line = 11u;
	source.declaration.column = 3u;
	(void)snprintf( source.semanticName, sizeof( source.semanticName ),
		"textures/base/wall" );
	source.sort = 3.0f;
	source.skyCloudHeight = 512.0f;
	source.diffuseReflectance[0] = source.diffuseReflectance[1] =
		source.diffuseReflectance[2] = 1.0f;
	source.dependencyCount = 1u;
	source.dependencies[0].role = RAL_MATERIAL_DEPENDENCY_TEXTURE;
	source.dependencies[0].sourceId = 5001u;
	source.dependencies[0].fsGeneration = 9u;
	source.dependencies[0].size = 4096u;
	source.dependencies[0].creationOptionsHash = 17u;
	source.dependencies[0].span.sourceId = 101u;
	source.dependencies[0].span.line = 12u;
	source.dependencies[0].span.column = 5u;
	(void)snprintf( source.dependencies[0].canonicalPath,
		sizeof( source.dependencies[0].canonicalPath ), "textures/base/wall.ktx2" );
	source.stageCount = 1u;
	source.stages[0].mapKind = RAL_MATERIAL_MAP_IMAGE;
	source.stages[0].dependencyIndex = 0u;
	source.stages[0].tcGen = RAL_MATERIAL_TCGEN_TEXTURE;
	source.stages[0].sourceBlend = 1u;
	source.stages[0].depthWrite = qtrue;
	source.stages[0].span.sourceId = 101u;
	source.stages[0].span.line = 13u;
	source.stages[0].span.column = 7u;
	source.stages[0].tcModCount = 1u;
	source.stages[0].tcMods[0].type = RAL_MATERIAL_TCMOD_SCROLL;
	source.stages[0].tcMods[0].span.sourceId = 101u;
	source.stages[0].tcMods[0].span.line = 14u;
	source.stages[0].tcMods[0].span.column = 9u;
	source.stages[0].tcMods[0].parameters[0] = 0.25f;
	return source;
}

static ralMaterialSourceIr_t Pbr( void ) {
	ralMaterialSourceIr_t source = Base();
	source.provenance = RAL_MATERIAL_PROVENANCE_WIRED_LUA;
	source.stageCount = 0u;
	source.pbrFactors.baseColor[0] = 0.8f;
	source.pbrFactors.baseColor[1] = 0.7f;
	source.pbrFactors.baseColor[2] = 0.6f;
	source.pbrFactors.baseColor[3] = 1.0f;
	source.pbrFactors.metallic = 0.25f;
	source.pbrFactors.roughness = 0.45f;
	source.pbrFactors.normalScale = 1.0f;
	source.pbrFactors.occlusionStrength = 0.9f;
	source.pbrRenderPolicy.depthTest = qtrue;
	source.pbrRenderPolicy.depthWrite = qtrue;
	source.pbrTextureCount = 1u;
	source.pbrTextures[0].span = source.declaration;
	source.pbrTextures[0].dependencyIndex = 0u;
	source.pbrTextures[0].role = RAL_MATERIAL_TEXTURE_BASE_COLOR;
	source.pbrTextures[0].channels = RAL_MATERIAL_CHANNEL_RGBA;
	return source;
}

typedef struct compileThreadContext_s {
	const ralMaterialSourceReceipt_t *expected;
	int failure;
} compileThreadContext_t;

static int CompileThreadBody( compileThreadContext_t *context ) {
	for ( unsigned iteration = 0u; iteration < 2048u; ++iteration ) {
		ralMaterialSourceIr_t source = Base();
		ralMaterialSourceReceipt_t receipt, before;
		ralMaterialSourceDiagnostic_t diagnostic;
		memset( &receipt, 0xA5, sizeof( receipt ) );
		if ( !Ral_MaterialSourceCompile( &source, &receipt, &diagnostic )
				|| diagnostic.reason != RAL_MATERIAL_SOURCE_OK
				|| !Ral_MaterialSourceReceiptExact( &receipt, context->expected ) )
			return 1;
		before = receipt;
		source.dependencies[0].canonicalPath[0] = '/';
		if ( Ral_MaterialSourceCompile( &source, &receipt, &diagnostic )
				|| diagnostic.reason != RAL_MATERIAL_SOURCE_INVALID_DEPENDENCY
				|| diagnostic.span.line != 12u
				|| diagnostic.span.column != 5u
				|| memcmp( &receipt, &before, sizeof( receipt ) ) )
			return 2;
	}
	return 0;
}

#if defined(_WIN32)
static DWORD WINAPI CompileThread( LPVOID opaque ) {
	compileThreadContext_t *context = opaque;
	context->failure = CompileThreadBody( context );
	return 0;
}
#else
static void *CompileThread( void *opaque ) {
	compileThreadContext_t *context = opaque;
	context->failure = CompileThreadBody( context );
	return NULL;
}
#endif

static int CheckConcurrentCompile( const ralMaterialSourceReceipt_t *expected ) {
	enum { THREAD_COUNT = 8 };
	compileThreadContext_t contexts[THREAD_COUNT];
#if defined(_WIN32)
	HANDLE threads[THREAD_COUNT];
#else
	pthread_t threads[THREAD_COUNT];
#endif
	memset( contexts, 0, sizeof( contexts ) );
	for ( unsigned i = 0u; i < THREAD_COUNT; ++i ) {
		contexts[i].expected = expected;
#if defined(_WIN32)
		threads[i] = CreateThread( NULL, 0, CompileThread, &contexts[i], 0, NULL );
		if ( !threads[i] ) return 1;
#else
		if ( pthread_create( &threads[i], NULL, CompileThread, &contexts[i] ) )
			return 1;
#endif
	}
	for ( unsigned i = 0u; i < THREAD_COUNT; ++i ) {
#if defined(_WIN32)
		if ( WaitForSingleObject( threads[i], INFINITE ) != WAIT_OBJECT_0 ) return 1;
		CloseHandle( threads[i] );
#else
		if ( pthread_join( threads[i], NULL ) ) return 1;
#endif
		if ( contexts[i].failure ) return contexts[i].failure;
	}
	return 0;
}

static int CheckSkySource( void ) {
	ralMaterialSourceIr_t source = Base(), bad;
	ralMaterialSourceReceipt_t receipt, before, changed;
	ralMaterialSourceDiagnostic_t diagnostic;
	source.sky = qtrue;
	source.skySource.outerBox = qtrue; /* All six faces may share one texture. */
	source.skySource.hasSun = qtrue;
	source.skySource.sunColor[0] = 1.0f;
	source.skySource.sunIntensity = 100.0f;
	source.skySource.sunElevationDegrees = 25.0f;
	CHECK( Ral_MaterialSourceCompile( &source, &receipt, NULL ) );
	before = receipt;
	source.skySource.sunColor[1] = 0.5f;
	CHECK( Ral_MaterialSourceCompile( &source, &changed, NULL ) );
	CHECK( changed.semanticHash != receipt.semanticHash );
	source.dependencies[0].sourceId++;
	CHECK( Ral_MaterialSourceCompile( &source, &receipt, NULL ) );
	CHECK( receipt.semanticHash == changed.semanticHash
		&& receipt.dependencyHash != changed.dependencyHash );
	receipt = before;
	bad = source; bad.skySource.outerDependencies[5] = bad.dependencyCount;
	CHECK( !Ral_MaterialSourceCompile( &bad, &receipt, &diagnostic ) );
	CHECK( diagnostic.reason == RAL_MATERIAL_SOURCE_INVALID_SKY
		&& !memcmp( &before, &receipt, sizeof( before ) ) );
	bad = source; bad.sky = qfalse;
	CHECK( !Ral_MaterialSourceCompile( &bad, &receipt, NULL ) );
	bad = source; bad.skySource.sunIntensity = NAN;
	CHECK( !Ral_MaterialSourceCompile( &bad, &receipt, NULL ) );
	bad = source; bad.skySource.sunColor[0] = -1.0f;
	CHECK( !Ral_MaterialSourceCompile( &bad, &receipt, NULL ) );
	bad = source; bad.skySource.sunExtended = qfalse; bad.skySource.sunSamples = 16u;
	CHECK( !Ral_MaterialSourceCompile( &bad, &receipt, NULL ) );
	bad = source; bad.skySource.innerBox = qfalse; bad.skySource.innerDependencies[0] = 1u;
	CHECK( !Ral_MaterialSourceCompile( &bad, &receipt, NULL ) );
	bad = source; bad.skySource.hasSun = qfalse;
	CHECK( !Ral_MaterialSourceCompile( &bad, &receipt, NULL ) );
	CHECK( !memcmp( &before, &receipt, sizeof( before ) ) );
	before.source.skySource.sunElevationDegrees += 1.0f;
	CHECK( !Ral_MaterialSourceReceiptValid( &before ) );
	return 0;
}

int main( void ) {
	ralMaterialSourceIr_t source = Base(), changed, bad;
	ralMaterialSourceIr_t pbr;
	ralMaterialSourceReceipt_t receipt, exact, before, changedReceipt;
	ralMaterialSourceDiagnostic_t diagnostic;
	memset( &receipt, 0xA5, sizeof( receipt ) );
	CHECK( Ral_MaterialSourceCompile( &source, &receipt, &diagnostic ) );
	CHECK( diagnostic.reason == RAL_MATERIAL_SOURCE_OK );
	CHECK( Ral_MaterialSourceReceiptValid( &receipt ) );
	exact = receipt;
	CHECK( Ral_MaterialSourceReceiptExact( &receipt, &exact ) );

	changed = source;
	changed.dependencies[0].sourceId++;
	CHECK( Ral_MaterialSourceCompile( &changed, &changedReceipt, NULL ) );
	CHECK( changedReceipt.semanticHash == receipt.semanticHash );
	CHECK( changedReceipt.dependencyHash != receipt.dependencyHash );
	CHECK( changedReceipt.artifactHash != receipt.artifactHash );

	changed = source;
	changed.stages[0].tcMods[0].parameters[0] = 0.5f;
	CHECK( Ral_MaterialSourceCompile( &changed, &changedReceipt, NULL ) );
	CHECK( changedReceipt.semanticHash != receipt.semanticHash );

	before = receipt;
	bad = source;
	bad.dependencies[0].canonicalPath[0] = '/';
	CHECK( !Ral_MaterialSourceCompile( &bad, &receipt, &diagnostic ) );
	CHECK( diagnostic.reason == RAL_MATERIAL_SOURCE_INVALID_DEPENDENCY );
	CHECK( diagnostic.span.line == 12u && diagnostic.span.column == 5u );
	CHECK( !memcmp( &receipt, &before, sizeof( receipt ) ) );

	bad = source;
	bad.dependencyCount = 2u;
	bad.dependencies[1] = bad.dependencies[0];
	CHECK( !Ral_MaterialSourceCompile( &bad, &receipt, &diagnostic ) );
	CHECK( diagnostic.reason == RAL_MATERIAL_SOURCE_DUPLICATE_DEPENDENCY );

	bad = source;
	bad.stages[0].dependencyIndex = 7u;
	CHECK( !Ral_MaterialSourceCompile( &bad, &receipt, &diagnostic ) );
	CHECK( diagnostic.reason == RAL_MATERIAL_SOURCE_INVALID_STAGE );
	CHECK( diagnostic.span.line == 13u && diagnostic.span.column == 7u );

	bad = source;
	bad.stages[0].tcMods[0].parameters[3] = NAN;
	CHECK( !Ral_MaterialSourceCompile( &bad, &receipt, &diagnostic ) );
	CHECK( diagnostic.reason == RAL_MATERIAL_SOURCE_NONFINITE_VALUE );
	CHECK( diagnostic.span.line == 14u && diagnostic.span.column == 9u );

	CHECK( CheckConcurrentCompile( &exact ) == 0 );
	CHECK( CheckSkySource() == 0 );

	pbr = Pbr();
	CHECK( Ral_MaterialSourceCompile( &pbr, &receipt, &diagnostic ) );
	CHECK( Ral_MaterialSourceReceiptValid( &receipt ) );
	changed = pbr;
	changed.pbrFactors.roughness = 0.6f;
	CHECK( Ral_MaterialSourceCompile( &changed, &changedReceipt, NULL ) );
	CHECK( changedReceipt.semanticHash != receipt.semanticHash );
	before = receipt;
	bad = pbr;
	bad.pbrTextures[0].channels = RAL_MATERIAL_CHANNEL_XY_NORMAL;
	CHECK( !Ral_MaterialSourceCompile( &bad, &receipt, &diagnostic ) );
	CHECK( diagnostic.reason == RAL_MATERIAL_SOURCE_INVALID_PBR_TEXTURE );
	CHECK( !memcmp( &receipt, &before, sizeof( receipt ) ) );
	bad = pbr;
	bad.pbrTextureCount = 2u;
	bad.pbrTextures[1] = bad.pbrTextures[0];
	CHECK( !Ral_MaterialSourceCompile( &bad, &receipt, &diagnostic ) );
	CHECK( diagnostic.reason == RAL_MATERIAL_SOURCE_DUPLICATE_PBR_TEXTURE );
	CHECK( !memcmp( &receipt, &before, sizeof( receipt ) ) );

	puts( "ral_material_source_test: ok" );
	return 0;
}
