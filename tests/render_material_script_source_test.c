// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "render_material_script.h"

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL %s:%d: %s\n", \
	__FILE__, __LINE__, #x); return 1; } } while (0)

static const char *missingResource;
static const char *replacementShader;
static qboolean precedenceFixture;

static qboolean Resolve( const char *name, char *canonical, size_t capacity,
		uint64_t *sourceId, uint64_t *size, unsigned *generation ) {
	uint64_t hash = UINT64_C( 14695981039346656037 );
	const char *resolved = name;
	if ( !name || !name[0] ) return qfalse;
	if ( missingResource && !strcmp( name, missingResource ) ) return qfalse;
	if ( !strcmp( name, "legacy/wall.tga" ) ) resolved = "textures/wall.ktx2";
	if ( !strcmp( name, "env/map_rt.tga" ) ) resolved = "env/map_rt.png";
	for ( const unsigned char *cursor = (const unsigned char *)resolved;
			*cursor; ++cursor ) {
		hash ^= *cursor;
		hash *= UINT64_C( 1099511628211 );
	}
	if ( snprintf( canonical, capacity, "%s", resolved ) >= (int)capacity )
		return qfalse;
	*sourceId = hash ? hash : 1u; *size = 4096u; *generation = 5u;
	return qtrue;
}

static const char richShader[] =
	"textures/test/rich\n"
	"{\n"
	" nopicmip\n"
	" polygonOffset\n"
	" fogParms (0.10 0.20 0.30) 256 exp2 0.002 4096\n"
	" deformVertexes move (1 2 3) sin 0 1 0 2\n"
	" deformVertexes projectionShadow\n"
	" {\n"
	"  animMap 4 textures/a.png textures/b.png\n"
	"  tcGen vector (1 0 0) (0 1 0)\n"
	"  tcMod transform 1 0.2 0.3 1 0.4 0.5\n"
	"  rgbGen wave sin 0.5 0.5 0 2\n"
	"  alphaGen wave triangle 0.25 0.75 0 1\n"
	"  alphaFunc LT128\n"
	"  depthFunc equal\n"
	"  depthWrite\n"
	" }\n"
	" {\n"
	"  map $whiteimage\n"
	"  rgbGen identityLight\n"
	"  alphaGen portal\n"
	" }\n"
	"}\n";

static char *shaderFiles[] = { "rich.shader" };
static char *precedenceShaderFiles[] = { "high.shader", "low.shader" };

static const char highPriorityShader[] =
	"textures/test/override { { map textures/high.png } }\n"
	"textures/test/same-file { { map textures/first.png } }\n"
	"textures/test/same-file { { map textures/second.png } }\n";
static const char lowPriorityShader[] =
	"textures/test/override { { map textures/low.png } }\n";

static char **ListFiles( const char *name, const char *extension,
		int *numFilesFound ) {
	if ( strcmp( name, "scripts" ) || strcmp( extension, ".shader" ) ) {
		*numFilesFound = 0;
		return NULL;
	}
	if ( precedenceFixture ) {
		*numFilesFound = 2;
		return precedenceShaderFiles;
	}
	*numFilesFound = 1;
	return shaderFiles;
}

static void FreeFileList( char **files ) { (void)files; }

static int ReadFile( const char *name, void **buffer ) {
	if ( precedenceFixture ) {
		if ( !strcmp( name, "scripts/high.shader" ) ) {
			*buffer = (void *)highPriorityShader;
			return (int)( sizeof( highPriorityShader ) - 1u );
		}
		if ( !strcmp( name, "scripts/low.shader" ) ) {
			*buffer = (void *)lowPriorityShader;
			return (int)( sizeof( lowPriorityShader ) - 1u );
		}
		return -1;
	}
	if ( strcmp( name, "scripts/rich.shader" ) ) return -1;
	if ( replacementShader ) {
		*buffer = (void *)replacementShader;
		return (int)strlen( replacementShader );
	}
	*buffer = (void *)richShader;
	return (int)( sizeof( richShader ) - 1u );
}

static void FreeFile( void *buffer ) { (void)buffer; }

typedef struct { unsigned calls; qboolean missingFrame; } stageResolverTest_t;

static qhandle_t ResolveStage( void *context,
		const renderMaterialScriptStage_t *stage, const char *name ) {
	stageResolverTest_t *test = context;
	++test->calls;
	if ( stage->imageSource != RENDER_MATERIAL_STAGE_IMAGE ) return 0;
	if ( !strcmp( name, "textures/a.png" ) ) return 17;
	if ( !strcmp( name, "textures/b.png" ) ) return test->missingFrame ? 0 : 23;
	return 0;
}

static int CheckStageDepthPublication( const renderMaterialScriptEntry_t *parsed ) {
	renderMaterialScriptEntry_t entry = *parsed;
	renderMaterialStageSnapshot_t stages[RENDER_MATERIAL_MAX_STAGES], before[RENDER_MATERIAL_MAX_STAGES];
	stageResolverTest_t resolver = { 0u, qfalse };
	entry.stageCount = RENDER_MATERIAL_MAX_STAGES;
	for ( unsigned i = 1u; i < entry.stageCount; ++i )
		entry.stages[i] = parsed->stages[1];
	for ( unsigned materialWrite = 0u; materialWrite < 2u; ++materialWrite ) {
		entry.depthWrite = (qboolean)materialWrite;
		for ( unsigned pass = 0u; pass < 4u; ++pass ) {
			for ( unsigned i = 0u; i < entry.stageCount; ++i ) {
				entry.stages[i].depthWrite = (qboolean)(( i + pass ) & 1u);
				entry.stages[i].depthFunc = (ralMaterialDepthFunc_t)((( i + pass ) >> 1u) & 1u);
			}
			CHECK( RenderMaterialScript_BuildStages( &entry, ResolveStage, &resolver, stages ) );
			for ( unsigned i = 0u; i < entry.stageCount; ++i ) {
				CHECK( stages[i].depthWrite == ( entry.stages[i].depthWrite
					|| ( i == 0u && entry.depthWrite ) ) );
				CHECK( stages[i].depthFunc == (uint32_t)entry.stages[i].depthFunc );
			}
		}
	}
	memcpy( before, stages, sizeof( before ) );
	/* Even invalid state in the final slot is rejected before resolving slot 0. */
	for ( unsigned defect = 0u; defect < 2u; ++defect ) {
		renderMaterialScriptEntry_t invalid = entry;
		if ( defect == 0u ) invalid.stages[entry.stageCount - 1u].depthFunc = (ralMaterialDepthFunc_t)-1;
		else invalid.stages[entry.stageCount - 1u].depthWrite = (qboolean)2;
		resolver.calls = 0u;
		CHECK( !RenderMaterialScript_BuildStages( &invalid, ResolveStage, &resolver, stages ) );
		CHECK( resolver.calls == 0u && !memcmp( before, stages, sizeof( before ) ) );
	}
	return 0;
}

static int CheckStagePublication( const renderMaterialScriptEntry_t *parsed ) {
	CHECK( CheckStageDepthPublication( parsed ) == 0 );
	renderMaterialScriptEntry_t entry = *parsed;
	renderMaterialStageSnapshot_t stages[RENDER_MATERIAL_MAX_STAGES], before[RENDER_MATERIAL_MAX_STAGES];
	stageResolverTest_t resolver = { 0u, qfalse };
	CHECK( RenderMaterialScript_BuildStages( &entry, ResolveStage, &resolver, stages ) );
	CHECK( stages[0].material == 17 && stages[0].animationFrameCount == 2u );
	CHECK( stages[0].animationMaterials[0] == 17 && stages[0].animationMaterials[1] == 23 );
	CHECK( stages[0].animationFrequency == 4.0f && stages[0].animationLooping );
	CHECK( stages[0].tcGen == RENDER_MATERIAL_TCGEN_VECTOR
		&& stages[0].tcGenVectors[0][0] == 1.0f && stages[0].tcGenVectors[1][1] == 1.0f );
	CHECK( stages[0].rgbWave.function == RAL_MATERIAL_WAVE_SIN
		&& stages[0].alphaWave.function == RAL_MATERIAL_WAVE_TRIANGLE );
	CHECK( stages[0].alphaTest == RAL_MATERIAL_ALPHA_TEST_KEEP_LESS );
	CHECK( stages[0].depthWrite == qtrue
		&& stages[0].depthFunc == RAL_MATERIAL_DEPTH_EQUAL );
	CHECK( stages[1].depthWrite == qfalse
		&& stages[1].depthFunc == RAL_MATERIAL_DEPTH_LEQUAL );
	CHECK( stages[0].tcModCount == 1u
		&& stages[0].tcMods[0].type == RAL_MATERIAL_TCMOD_TRANSFORM
		&& stages[0].tcMods[0].parameters[1] == 0.2f
		&& stages[0].tcMods[0].parameters[2] == 0.3f
		&& stages[0].tcMods[0].parameters[4] == 0.4f );
	CHECK( stages[1].imageSource == RENDER_MATERIAL_STAGE_WHITE && !stages[1].material );
	CHECK( stages[1].alphaGen == RAL_MATERIAL_ALPHA_PORTAL && stages[1].portalRange == 256.0f );
	CHECK( !stages[2].material && !stages[2].animationFrameCount );
	memcpy( before, stages, sizeof( before ) );
	resolver.missingFrame = qtrue;
	CHECK( !RenderMaterialScript_BuildStages( &entry, ResolveStage, &resolver, stages ) );
	CHECK( !memcmp( before, stages, sizeof( before ) ) );
	resolver.calls = 0u;
	entry.stages[0].animationFrameOffset = UINT32_MAX;
	CHECK( !RenderMaterialScript_BuildStages( &entry, ResolveStage, &resolver, stages ) );
	CHECK( !resolver.calls && !memcmp( before, stages, sizeof( before ) ) );
	entry = *parsed;
	entry.stageCount = RENDER_MATERIAL_MAX_STAGES + 1u;
	CHECK( !RenderMaterialScript_BuildStages( &entry, ResolveStage, &resolver, stages ) );
	CHECK( !resolver.calls && !memcmp( before, stages, sizeof( before ) ) );
	entry = *parsed;
	entry.stages[0].sourceTcModCount = RAL_MATERIAL_SOURCE_MAX_TCMODS + 1u;
	CHECK( !RenderMaterialScript_BuildStages( &entry, ResolveStage, &resolver, stages ) );
	CHECK( !resolver.calls && !memcmp( before, stages, sizeof( before ) ) );
	entry = *parsed;
	entry.stages[0].animationFrameCount = 0u;
	entry.stages[0].imageSource = RENDER_MATERIAL_STAGE_LIGHTMAP;
	CHECK( RenderMaterialScript_BuildStages( &entry, ResolveStage, &resolver, stages ) );
	CHECK( !resolver.calls && stages[0].imageSource == RENDER_MATERIAL_STAGE_LIGHTMAP
		&& !stages[0].material && !stages[1].material );
	return 0;
}

static int CheckRichParser( void ) {
	static renderMaterialScriptCatalog_t catalog;
	renderMaterialScriptEntry_t parsed;
	refimport_t imports;
	ralMaterialSourceReceipt_t receipt;
	ralMaterialSourceDiagnostic_t diagnostic;
	memset( &imports, 0, sizeof( imports ) );
	imports.FS_ListFiles = ListFiles;
	imports.FS_FreeFileList = FreeFileList;
	imports.FS_ReadFile = ReadFile;
	imports.FS_FreeFile = FreeFile;
	imports.FS_ResolveResource = Resolve;
	CHECK( RenderMaterialScript_Load( &catalog, &imports ) );
	CHECK( RenderMaterialScript_Lookup( &catalog, "textures/test/rich", &parsed ) );
	CHECK( parsed.noPicMip && !parsed.noMipMaps && parsed.polygonOffset );
	CHECK( parsed.fogType == RAL_MATERIAL_FOG_EXP2 );
	CHECK( parsed.fogDepth == 256.0f && parsed.fogDensity == 0.002f
		&& parsed.fogFarClip == 4096.0f );
	CHECK( parsed.deformCount == 2u
		&& parsed.deforms[0].type == RAL_MATERIAL_DEFORM_MOVE
		&& parsed.deforms[1].type == RAL_MATERIAL_DEFORM_PROJECTION_SHADOW );
	CHECK( parsed.stageCount == 2u );
	CHECK( CheckStagePublication( &parsed ) == 0 );
	CHECK( parsed.stages[0].animationFrameCount == 2u
		&& parsed.stages[0].animationFrequency == 4.0f
		&& parsed.stages[0].animationLooping );
	CHECK( parsed.stages[0].tcGen == RENDER_MATERIAL_TCGEN_VECTOR );
	CHECK( parsed.stages[0].sourceTcModCount == 1u
		&& parsed.stages[0].sourceTcMods[0].type
			== RAL_MATERIAL_TCMOD_TRANSFORM );
	CHECK( parsed.stages[0].rgbGen == RAL_MATERIAL_COLOR_WAVE
		&& parsed.stages[0].rgbWave.function == RAL_MATERIAL_WAVE_SIN
		&& parsed.stages[0].alphaGen == RAL_MATERIAL_ALPHA_WAVE
		&& parsed.stages[0].alphaWave.function == RAL_MATERIAL_WAVE_TRIANGLE
		&& parsed.stages[0].alphaTest == RAL_MATERIAL_ALPHA_TEST_KEEP_LESS
		&& parsed.stages[0].depthFunc == RAL_MATERIAL_DEPTH_EQUAL );
	CHECK( parsed.stages[1].rgbGen == RAL_MATERIAL_COLOR_IDENTITY_LIGHTING
		&& parsed.stages[1].alphaGen == RAL_MATERIAL_ALPHA_PORTAL
		&& parsed.stages[1].portalRange == 256.0f );
	CHECK( RenderMaterialScript_CompileSource( &parsed, &imports, 12u,
		&receipt, &diagnostic ) );
	CHECK( Ral_MaterialSourceReceiptValid( &receipt ) );
	CHECK( receipt.source.noPicMip && receipt.source.polygonOffset );
	CHECK( receipt.source.stages[0].animationFrameCount == 2u );
	CHECK( receipt.source.stageCount == 2u
		&& receipt.source.stages[1].mapKind == RAL_MATERIAL_MAP_WHITE );
	CHECK( receipt.source.stages[0].animationDependencyIndices[0]
		== receipt.source.stages[0].dependencyIndex );
	CHECK( receipt.source.dependencyCount == 2u );
	CHECK( receipt.source.stages[0].tcMods[0].parameters[1] == 0.2f );
	CHECK( receipt.source.deforms[0].span.sourceId
		== parsed.declarationSourceId );
	return 0;
}

static int CheckVfsDeclarationPrecedence( void ) {
	static renderMaterialScriptCatalog_t catalog;
	renderMaterialScriptEntry_t parsed;
	refimport_t imports;
	memset( &imports, 0, sizeof( imports ) );
	imports.FS_ListFiles = ListFiles;
	imports.FS_FreeFileList = FreeFileList;
	imports.FS_ReadFile = ReadFile;
	imports.FS_FreeFile = FreeFile;
	imports.FS_ResolveResource = Resolve;
	precedenceFixture = qtrue;
	CHECK( RenderMaterialScript_Load( &catalog, &imports ) );
	CHECK( RenderMaterialScript_Lookup( &catalog,
		"textures/test/override", &parsed ) );
	CHECK( !strcmp( parsed.imageName, "textures/high.png" ) );
	CHECK( !strcmp( parsed.declarationPath, "scripts/high.shader" ) );
	CHECK( RenderMaterialScript_Lookup( &catalog,
		"textures/test/same-file", &parsed ) );
	CHECK( !strcmp( parsed.imageName, "textures/second.png" ) );
	precedenceFixture = qfalse;
	return 0;
}

static int CheckSkyParser( void ) {
	static renderMaterialScriptCatalog_t catalog;
	static const char shader[] =
		"textures/test/sky\n{\n"
		" surfaceparm sky\n"
		" skyParms env/map 384 env/inner\n"
		" q3map_sunExt 1 0.5 0.25 120 270 35 2 16\n"
		" {\n map textures/clouds.png\n tcMod scale 3 2\n"
		" tcMod scroll 0.1 -0.05\n rgbGen identity\n }\n"
		" {\n animMap 4 textures/red1.png textures/red2.png\n"
		" blendFunc add\n tcMod rotate 7\n"
		" rgbGen wave sin 0.5 0.5 0 0.2\n }\n}\n"
		"textures/test/cloudonly\n{\n skyParms 512 full -\n skyParms - 0 -\n"
		" q3map_sun 0.2 0.4 1 80 90 -10\n"
		" { map textures/clouds.png }\n}\n"
		"textures/test/space\n{\n skyParms env/stars - -\n}\n";
	renderMaterialScriptEntry_t parsed;
	ralMaterialSourceReceipt_t receipt, before, changed;
	refimport_t imports;
	memset( &imports, 0, sizeof( imports ) );
	imports.FS_ListFiles = ListFiles;
	imports.FS_FreeFileList = FreeFileList;
	imports.FS_ReadFile = ReadFile;
	imports.FS_FreeFile = FreeFile;
	imports.FS_ResolveResource = Resolve;
	replacementShader = shader;
	CHECK( RenderMaterialScript_Load( &catalog, &imports ) );
	CHECK( RenderMaterialScript_Lookup( &catalog, "textures/test/sky", &parsed ) );
	CHECK( !strcmp( parsed.skyBoxPrefix, "env/map" )
		&& !strcmp( parsed.skyInnerBoxPrefix, "env/inner" ) );
	CHECK( parsed.skyCloudHeight == 384.0f );
	CHECK( parsed.skySource.hasSun && parsed.skySource.sunExtended );
	CHECK( parsed.skySource.sunIntensity == 120.0f
		&& parsed.skySource.sunColor[1] == 0.5f
		&& parsed.skySource.sunAzimuthDegrees == 270.0f
		&& parsed.skySource.sunElevationDegrees == 35.0f
		&& parsed.skySource.sunDevianceDegrees == 2.0f
		&& parsed.skySource.sunSamples == 16u );
	CHECK( RenderMaterialScript_CompileSource( &parsed, &imports, 12u,
		&receipt, NULL ) );
	CHECK( Ral_MaterialSourceReceiptValid( &receipt ) );
	CHECK( receipt.source.skySource.outerBox && receipt.source.skySource.innerBox );
	CHECK( receipt.source.dependencyCount == 15u && receipt.source.stageCount == 2u );
	for ( uint32_t face = 0u; face < RAL_MATERIAL_SKY_FACE_COUNT; ++face ) {
		static const char *suffix[] = { "rt", "bk", "lf", "ft", "up", "dn" };
		char expected[MAX_QPATH];
		uint32_t index = receipt.source.skySource.outerDependencies[face];
		(void)snprintf( expected, sizeof( expected ), "env/map_%s.%s",
			suffix[face], face == 0u ? "png" : "tga" );
		CHECK( !strcmp( receipt.source.dependencies[index].canonicalPath, expected ) );
		CHECK( receipt.source.dependencies[index].span.line == 4u );
		index = receipt.source.skySource.innerDependencies[face];
		(void)snprintf( expected, sizeof( expected ), "env/inner_%s.tga", suffix[face] );
		CHECK( !strcmp( receipt.source.dependencies[index].canonicalPath, expected ) );
	}
	CHECK( receipt.source.stages[0].tcModCount == 2u
		&& receipt.source.stages[0].tcMods[0].type == RAL_MATERIAL_TCMOD_SCALE
		&& receipt.source.stages[0].tcMods[1].type == RAL_MATERIAL_TCMOD_SCROLL );
	CHECK( receipt.source.stages[1].animationFrameCount == 2u
		&& receipt.source.stages[1].destinationBlend == RENDER_MATERIAL_BLEND_ONE
		&& receipt.source.stages[1].rgbGen == RAL_MATERIAL_COLOR_WAVE );
	before = receipt;
	parsed.skySource.sunAzimuthDegrees += 1.0f;
	CHECK( RenderMaterialScript_CompileSource( &parsed, &imports, 12u, &changed, NULL ) );
	CHECK( changed.semanticHash != receipt.semanticHash
		&& changed.dependencyHash == receipt.dependencyHash );
	missingResource = "env/map_dn.tga";
	CHECK( !RenderMaterialScript_CompileSource( &parsed, &imports, 12u, &receipt, NULL ) );
	CHECK( !memcmp( &before, &receipt, sizeof( receipt ) ) );
	missingResource = NULL;
	CHECK( RenderMaterialScript_Lookup( &catalog, "textures/test/cloudonly", &parsed ) );
	CHECK( parsed.skyCloudHeight == 512.0f );
	CHECK( RenderMaterialScript_CompileSource( &parsed, &imports, 12u, &receipt, NULL ) );
	CHECK( !receipt.source.skySource.outerBox && !receipt.source.skySource.innerBox
		&& receipt.source.dependencyCount == 1u && receipt.source.skySource.hasSun
		&& !receipt.source.skySource.sunExtended
		&& receipt.source.skySource.sunElevationDegrees == -10.0f );
	CHECK( RenderMaterialScript_Lookup( &catalog, "textures/test/space", &parsed ) );
	CHECK( parsed.skyCloudHeight == 512.0f );
	CHECK( RenderMaterialScript_CompileSource( &parsed, &imports, 12u, &receipt, NULL ) );
	CHECK( receipt.source.stageCount == 0u && receipt.source.dependencyCount == 6u
		&& !receipt.source.skySource.hasSun );
	/* Q3 fog-only declarations are volume metadata, not missing images. */
	replacementShader = "textures/sfx/fog_intel {\n"
		"qer_editorimage textures/editor-only-missing.tga\n"
		"surfaceparm trans\nsurfaceparm nonsolid\nsurfaceparm fog\n"
		"surfaceparm nolightmap\nfogparms ( .75 .38 0 ) 800\n}\n";
	CHECK( RenderMaterialScript_Load( &catalog, &imports ) );
	CHECK( RenderMaterialScript_Lookup( &catalog, "textures/sfx/fog_intel", &parsed ) );
	CHECK( parsed.stageCount == 0u && parsed.fogDepth == 800.0f && parsed.noDraw );
	CHECK( RenderMaterialScript_CompileSource( &parsed, &imports, 9u,
		&receipt, NULL ) );
	CHECK( receipt.source.dependencyCount == 0u && receipt.source.stageCount == 0u
		&& receipt.source.fogDepth == 800.0f && receipt.source.fogColor[0] == .75f );
	replacementShader = "bad {\n q3map_sun 1 1 1\n skyParms - 512 -\n}";
	CHECK( !RenderMaterialScript_Load( &catalog, &imports ) && !catalog.ready );
	replacementShader = "bad {\n q3map_sunExt 1 1 1 50 0 90 2 1.5\n}";
	CHECK( !RenderMaterialScript_Load( &catalog, &imports ) );
	replacementShader = "bad {\n q3map_sunExt 1 1 1 50 0 90 2 4294967296\n}";
	CHECK( !RenderMaterialScript_Load( &catalog, &imports ) );
	replacementShader = "bad {\n skyParms - nan -\n}";
	CHECK( !RenderMaterialScript_Load( &catalog, &imports ) );
	replacementShader = NULL;
	return 0;
}

int main( void ) {
	renderMaterialScriptEntry_t entry;
	refimport_t imports;
	ralMaterialSourceReceipt_t receipt, exact, before;
	ralMaterialSourceDiagnostic_t diagnostic;
	memset( &entry, 0, sizeof( entry ) );
	CHECK( CheckVfsDeclarationPrecedence() == 0 );
	memset( &imports, 0, sizeof( imports ) );
	(void)snprintf( entry.name, sizeof( entry.name ), "textures/test/wall" );
	(void)snprintf( entry.declarationPath, sizeof( entry.declarationPath ),
		"scripts/test.shader" );
	entry.declarationSourceId = 11u;
	entry.declarationLine = 4u;
	entry.declarationColumn = 2u;
	entry.declarationSize = 512u;
	entry.declarationGeneration = 5u;
	entry.sort = 3.0f;
	entry.cullMode = RENDER_CULL_BACK;
	entry.depthWrite = qtrue;
	entry.skyCloudHeight = 512.0f;
	entry.lighting.diffuseReflectance[0] =
		entry.lighting.diffuseReflectance[1] =
		entry.lighting.diffuseReflectance[2] = 1.0f;
	entry.stageCount = 2u;
	for ( uint32_t i = 0u; i < entry.stageCount; ++i ) {
		entry.stages[i].sourceLine = 6u + i;
		entry.stages[i].sourceColumn = 3u;
		entry.stages[i].imageSource = RENDER_MATERIAL_STAGE_IMAGE;
		(void)snprintf( entry.stages[i].imageName,
			sizeof( entry.stages[i].imageName ), "%s",
			i ? "textures/wall.ktx2" : "legacy/wall.tga" );
		entry.stages[i].sourceBlend = RENDER_MATERIAL_BLEND_ONE;
		entry.stages[i].scale[0] = entry.stages[i].scale[1] = 1.0f;
	}
	imports.FS_ResolveResource = Resolve;
	CHECK( RenderMaterialScript_CompileSource( &entry, &imports, 9u,
		&receipt, &diagnostic ) );
	CHECK( Ral_MaterialSourceReceiptValid( &receipt ) );
	CHECK( receipt.source.dependencyCount == 1u );
	CHECK( receipt.source.declaration.line == 4u );
	CHECK( receipt.source.stages[0].span.line == 6u );
	CHECK( receipt.source.dependencies[0].span.line == 6u );
	CHECK( receipt.source.stages[0].dependencyIndex == 0u );
	CHECK( receipt.source.stages[1].dependencyIndex == 0u );
	CHECK( !strcmp( receipt.source.dependencies[0].canonicalPath,
		"textures/wall.ktx2" ) );
	exact = receipt;
	CHECK( Ral_MaterialSourceReceiptExact( &receipt, &exact ) );
	before = receipt;
	entry.stages[0].imageName[0] = '\0';
	CHECK( !RenderMaterialScript_CompileSource( &entry, &imports, 10u,
		&receipt, &diagnostic ) );
	CHECK( !memcmp( &receipt, &before, sizeof( receipt ) ) );
	CHECK( CheckRichParser() == 0 );
	CHECK( CheckSkyParser() == 0 );
	puts( "render_material_script_source_test: ok" );
	return 0;
}
