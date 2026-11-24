// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "render_submission.h"
#include "render_lighting_sidecar.h"
#include "render_material_script.h"
#include "maps/map_format_registry.h"
#include "qfiles.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK( x )                                                         \
	do {                                                                   \
		if ( !( x ) ) {                                                    \
			fprintf( stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #x ); \
			return 1;                                                      \
		}                                                                  \
	} while ( 0 )

typedef struct {
	uint64_t occludedEmitterId;
} lightingVisibilityFixture_t;

static const char *s_materialScriptText;
static char *s_materialScriptFiles[] = { "lighting.shader" };

static char **MaterialScriptListFiles( const char *name,
		const char *extension, int *count ) {
	if ( !name || !extension || !count || strcmp( name, "scripts" )
			|| strcmp( extension, ".shader" ) ) return NULL;
	*count = 1;
	return s_materialScriptFiles;
}

static void MaterialScriptFreeFileList( char **files ) { (void)files; }

static int MaterialScriptReadFile( const char *name, void **bytes ) {
	if ( !name || !bytes || strcmp( name, "scripts/lighting.shader" )
			|| !s_materialScriptText ) return -1;
	*bytes = (void *)s_materialScriptText;
	return (int)strlen( s_materialScriptText );
}

static void MaterialScriptFreeFile( void *bytes ) { (void)bytes; }

static qboolean BuildTestTextureKey( uint64_t provenance, uint32_t width,
		uint32_t height, ralTextureCacheKey_t *outKey ) {
	ralTextureAssetRequest_t request;
	ralTextureAssetReceipt_t receipt;
	memset( &request, 0, sizeof( request ) );
	request.schemaVersion = RAL_TEXTURE_ASSET_SCHEMA_VERSION;
	request.assetGeneration = 3u;
	request.provenanceHash = provenance;
	request.dimension = RAL_TEXTURE_ASSET_2D;
	request.width = width;
	request.height = height;
	request.depth = request.layers = request.sourceMipLevels = 1u;
	request.colorEncoding = RAL_TEXTURE_ENCODING_SRGB;
	request.channelSemantic = RAL_TEXTURE_CHANNEL_COLOR;
	request.sourceEncoding = RAL_TEXTURE_SOURCE_UNCOMPRESSED;
	request.mipPolicy = RAL_TEXTURE_MIPS_GENERATE;
	request.residency = RAL_TEXTURE_RESIDENCY_STREAMED;
	request.preferenceCount = 1u;
	request.preferences[0] = RAL_TEXTURE_COMPRESSION_UNCOMPRESSED;
	request.allowUncompressedFallback = qtrue;
	return Ral_ResolveTextureAsset( &request, &receipt )
		&& Ral_TextureCacheKeyBuild( &receipt, outKey );
}

static qboolean MaterialScriptResolveResource( const char *name,
		char *canonicalPath, size_t canonicalCapacity, uint64_t *sourceId,
		uint64_t *size, unsigned *generation ) {
	if ( !name || !canonicalPath || !canonicalCapacity || !sourceId || !size
			|| !generation || strcmp( name, "scripts/lighting.shader" ) )
		return qfalse;
	if ( snprintf( canonicalPath, canonicalCapacity, "%s", name ) < 0
			|| strlen( name ) >= canonicalCapacity ) return qfalse;
	*sourceId = UINT64_C( 0x4c49474854494e47 );
	*size = s_materialScriptText ? (uint64_t)strlen( s_materialScriptText ) : 0u;
	*generation = 1u;
	return qtrue;
}

static int TestMaterialScriptLighting( void ) {
	static const char validScript[] =
		"textures/test/lava { cull disable sort 4.25\n"
		" wiredLighting { diffuse 0.5 0.25 1 emissive 8 2 1 mobility stationary "
		"range 384 shadowPriority 7 proxyCount 3 staticBake 1 "
		"explicitProxyAuthority 0 injectAtmosphere 1 }\n"
		" { map textures/test/lava_d }\n"
		"}\n"
		"textures/test/hell { surfaceparm sky skyparms env/test 384 - "
		"{ map textures/test/clouds.tga tcMod scroll .05 .1 "
		"tcMod scale 2 3 depthWrite } "
		"{ map textures/test/clouds_detail.tga blendFunc GL_ONE GL_ONE "
		"tcMod scroll .02 .03 tcMod scale 4 5 } }\n"
		"textures/test/hidden { surfaceparm nodraw }\n"
		"models/test/glow { { map models/test/glow.jpg blendFunc add } }\n"
		"models/test/streak { cull back { clampmap models/test/streak.tga tcGen environment "
		"blendFunc GL_SRC_ALPHA GL_ONE "
		"tcMod transform 1 0 0 48 0 -23.5 } }\n"
		"textures/test/filter { cull front { map $lightmap } "
		"{ map textures/test/filter_d blendFunc GL_DST_COLOR GL_ZERO } }\n";
	static const char invalidScript[] =
		"textures/test/bad { wiredLighting { emissive 1 2 } "
		"{ map textures/test/bad_d } }";
	refimport_t imports;
	/* The bounded submission and script catalogs intentionally model product-size
	 * capacities; keep them out of the test thread's small macOS stack. */
	static renderMaterialScriptCatalog_t catalog;
	static renderMaterialScriptEntry_t entry;
	static renderSubmissionState_t submission;
	static renderMaterialSnapshot_t snapshot;
	qhandle_t material;
	memset( &imports, 0, sizeof( imports ) );
	imports.FS_ListFiles = MaterialScriptListFiles;
	imports.FS_FreeFileList = MaterialScriptFreeFileList;
	imports.FS_ReadFile = MaterialScriptReadFile;
	imports.FS_FreeFile = MaterialScriptFreeFile;
	imports.FS_ResolveResource = MaterialScriptResolveResource;
	s_materialScriptText = validScript;
	CHECK( RenderMaterialScript_Load( &catalog, &imports ) );
	CHECK( catalog.ready && catalog.count == 6u );
	CHECK( RenderMaterialScript_Lookup( &catalog, "textures/test/lava", &entry ) );
	CHECK( entry.hasLighting && !strcmp( entry.imageName, "textures/test/lava_d" )
		&& entry.cullMode == RENDER_CULL_NONE && entry.sortExplicit
		&& fabsf( entry.sort - 4.25f ) < 0.0001f );
	CHECK( entry.declarationLine == 1u && entry.declarationColumn == 1u );
	CHECK( entry.stageCount == 1u && entry.stages[0].sourceLine == 3u
		&& entry.stages[0].sourceColumn == 2u );
	CHECK( entry.lighting.emissiveMobility == RAL_LIGHT_MOBILITY_STATIONARY );
	CHECK( entry.lighting.emissiveRequestedProxyCount == 3 );
	CHECK( entry.lighting.emissiveInjectsAtmosphere == 1 );
	CHECK( RenderMaterialScript_Lookup( &catalog, "textures/test/hell", &entry ) );
	CHECK( entry.sky && !entry.noDraw
		&& fabsf( entry.sort - RENDER_MATERIAL_SORT_ENVIRONMENT ) < 0.0001f
		&& !strcmp( entry.imageName, "env/test_up" )
		&& !strcmp( entry.skyBoxPrefix, "env/test" )
		&& fabsf( entry.skyCloudHeight - 384.0f ) < 0.0001f
		&& fabsf( entry.skyScale[0] - 2.0f ) < 0.0001f
		&& fabsf( entry.skyScale[1] - 3.0f ) < 0.0001f
		&& fabsf( entry.skyScroll[0] - 0.05f ) < 0.0001f
		&& fabsf( entry.skyScroll[1] - 0.1f ) < 0.0001f
		&& !strcmp( entry.secondaryImageName,
			"textures/test/clouds_detail.tga" )
		&& entry.secondaryAlphaMode == RENDER_ALPHA_ADDITIVE
		&& fabsf( entry.secondarySkyScale[0] - 4.0f ) < 0.0001f
		&& fabsf( entry.secondarySkyScale[1] - 5.0f ) < 0.0001f
		&& fabsf( entry.secondarySkyScroll[0] - 0.02f ) < 0.0001f
		&& fabsf( entry.secondarySkyScroll[1] - 0.03f ) < 0.0001f );
	CHECK( RenderMaterialScript_Lookup( &catalog, "textures/test/hidden", &entry )
		&& entry.noDraw && !entry.imageName[0] );
	CHECK( RenderMaterialScript_Lookup( &catalog,
		"models/test/glow.tga", &entry )
		&& !strcmp( entry.imageName, "models/test/glow.jpg" )
		&& entry.alphaMode == RENDER_ALPHA_ADDITIVE && !entry.depthWrite
		&& fabsf( entry.sort - RENDER_MATERIAL_SORT_ADDITIVE ) < 0.0001f );
	CHECK( RenderMaterialScript_Lookup( &catalog, "models/test/streak", &entry )
		&& entry.hasTcTransform
		&& entry.cullMode == RENDER_CULL_FRONT
		&& entry.stages[0].tcGen == RENDER_MATERIAL_TCGEN_ENVIRONMENT
		&& entry.alphaMode == RENDER_ALPHA_ALPHA_ADDITIVE && !entry.depthWrite
		&& fabsf( entry.skyScale[0] - 1.0f ) < 0.0001f
		&& fabsf( entry.skyScale[1] - 48.0f ) < 0.0001f
		&& fabsf( entry.skyScroll[0] ) < 0.0001f
		&& fabsf( entry.skyScroll[1] + 23.5f ) < 0.0001f );
	CHECK( RenderMaterialScript_Lookup( &catalog, "textures/test/filter", &entry )
		&& entry.alphaMode == RENDER_ALPHA_OPAQUE && entry.depthWrite
		&& entry.cullMode == RENDER_CULL_BACK
		&& entry.stageCount == 2u
		&& entry.stages[0].imageSource == RENDER_MATERIAL_STAGE_LIGHTMAP
		&& entry.stages[1].imageSource == RENDER_MATERIAL_STAGE_IMAGE
		&& entry.stages[1].sourceBlend == RENDER_MATERIAL_BLEND_DST_COLOR
		&& entry.stages[1].destinationBlend == RENDER_MATERIAL_BLEND_ZERO );
	CHECK( RenderSubmission_Init( &submission, 1u ) );
	material = RenderSubmission_RegisterAsset( &submission,
		RENDER_ASSET_MATERIAL, "textures/test/lava" );
	CHECK( material > 0 );
	CHECK( RenderMaterialScript_ApplyLighting( &catalog,
		"textures/test/lava", &submission, material ) );
	CHECK( RenderSubmission_MaterialSnapshot( &submission, material, &snapshot ) );
	CHECK( snapshot.lighting.ready
		&& snapshot.lighting.schemaVersion == RENDER_MATERIAL_LIGHTING_SCHEMA_VERSION
		&& snapshot.lighting.emissiveMobility == RAL_LIGHT_MOBILITY_STATIONARY
		&& snapshot.lighting.emissiveRequestedProxyCount == 3u
		&& snapshot.lighting.emissiveInjectsAtmosphere );
	CHECK( RenderSubmission_SetMaterialCullMode( &submission, material,
		RENDER_CULL_NONE ) );
	CHECK( RenderSubmission_SetMaterialSort( &submission, material, 4.25f ) );
	CHECK( RenderSubmission_MaterialSnapshot( &submission, material, &snapshot )
		&& snapshot.cullMode == RENDER_CULL_NONE
		&& fabsf( snapshot.sort - 4.25f ) < 0.0001f );
	CHECK( !RenderSubmission_SetMaterialSort( &submission, material, NAN ) );
	{
		static const byte pixel[4] = { 255u, 255u, 255u, 255u };
		static const float scale[2] = { 3.0f, 2.0f };
		static const float scroll[2] = { 0.05f, 0.06f };
		qhandle_t secondary = RenderSubmission_RegisterMaterialImage( &submission,
			RENDER_ASSET_MATERIAL, "textures/test/clouds_detail.tga", qfalse,
			pixel, 1u, 1u );
		CHECK( secondary > 0 && RenderSubmission_SetMaterialSkySecondary(
			&submission, material, secondary, RENDER_ALPHA_ADDITIVE,
			scale, scroll ) );
		CHECK( RenderSubmission_MaterialSnapshot( &submission, material, &snapshot )
			&& snapshot.skySecondaryMaterial == secondary
			&& snapshot.skySecondaryAlphaMode == RENDER_ALPHA_ADDITIVE
			&& fabsf( snapshot.skySecondaryScaleScroll[0] - 3.0f ) < 0.0001f
			&& fabsf( snapshot.skySecondaryScaleScroll[3] - 0.06f ) < 0.0001f );
	}
	RenderSubmission_Reset( &submission );
	s_materialScriptText = invalidScript;
	memset( &catalog, 0xa5, sizeof( catalog ) );
	CHECK( !RenderMaterialScript_Load( &catalog, &imports ) );
	CHECK( !catalog.ready && catalog.count == 0u );
	return 0;
}

/* A brush entity (func_door, func_plat, func_train) is an inline BSP model: model 0 is the static world, models 1..N own their
 * own surface ranges and are drawn through refEntity_t. Their surfaces stay out of the static world, and "*N" must still register
 * from them (the RAL renderers drew no door, plat or train while the world snapshot alone held lowered geometry). */
static int TestInlineSubmodelRegisters( void )
{
	static renderSubmissionState_t state;
	mapFile_t world;
	dsurface_t surfaces[2];
	drawVert_t vertices[6];
	int indices[6] = { 0, 1, 2, 0, 1, 2 };
	dshader_t shader = { 0 };
	dmodel_t subModels[2];
	renderModelSnapshot_t snapshot;
	qhandle_t door;
	memset( &world, 0, sizeof( world ) ); memset( surfaces, 0, sizeof( surfaces ) );
	memset( vertices, 0, sizeof( vertices ) ); memset( subModels, 0, sizeof( subModels ) );
	strcpy( world.name, "maps/inline-submodel-fixture.bsp" );
	world.checksum = 0x5151;
	world.numSurfaces = 2; world.surfaces = surfaces;
	world.numDrawVerts = 6; world.drawVerts = vertices;
	world.numDrawIndexes = 6; world.drawIndexes = indices;
	world.numShaders = 1; world.shaders = &shader;
	world.numSubModels = 2; world.subModels = subModels;
	strcpy( shader.shader, "textures/inline-submodel-fixture" );
	for ( int i = 0; i < 2; ++i ) {
		surfaces[i].surfaceType = MST_PLANAR; surfaces[i].lightmapNum = -1;
		surfaces[i].firstVert = 3 * i; surfaces[i].numVerts = 3;
		surfaces[i].firstIndex = 3 * i; surfaces[i].numIndexes = 3;
	}
	/* world triangle near the origin, door triangle at x 100..101 */
	vertices[1].xyz[0] = 1.0f; vertices[2].xyz[1] = 1.0f;
	vertices[3].xyz[0] = 100.0f; vertices[4].xyz[0] = 101.0f; vertices[5].xyz[0] = 100.0f; vertices[5].xyz[1] = 1.0f;
	subModels[0].firstSurface = 0; subModels[0].numSurfaces = 1;
	subModels[1].firstSurface = 1; subModels[1].numSurfaces = 1;
	subModels[1].mins[0] = 100.0f; subModels[1].maxs[0] = 101.0f; subModels[1].maxs[1] = 1.0f;
	CHECK( RenderSubmission_Init( &state, 57u ) );
	CHECK( RenderSubmission_RegisterAsset( &state, RENDER_ASSET_MATERIAL, shader.shader ) > 0 );
	CHECK( RenderSubmission_LoadWorld( &state, &world, 0 ) );
	/* the static world holds model 0 only */
	CHECK( state.worldSnapshot.batchCount == 1u && state.worldSnapshot.batches[0].sourceSurfaceIndex == 0u );
	door = RenderSubmission_RegisterBspModel( &state, "*1" );
	CHECK( door > 0 && RenderSubmission_ModelSnapshot( &state, door, &snapshot ) );
	CHECK( snapshot.format == RENDER_MODEL_INLINE_BSP && snapshot.batchCount == 1u && snapshot.indexCount == 3u );
	/* the door's own triangle, not the world's */
	{
		float minX = 1e9f;
		for ( uint32_t i = 0u; i < snapshot.indexCount; ++i ) {
			const float x = snapshot.positions[snapshot.indices[i] * 3u];
			if ( x < minX ) minX = x;
		}
		CHECK( minX == 100.0f );
	}
	CHECK( RenderSubmission_RegisterBspModel( &state, "*1" ) == door );
	CHECK( !RenderSubmission_RegisterBspModel( &state, "*2" ) );
	CHECK( RenderSubmission_UnloadWorld( &state, 0 ) );
	return 0;
}

static int TestLegacyLightGridIrradiance( void )
{
	static renderSubmissionState_t state;
	mapFile_t world;
	dsurface_t surface = { 0 };
	drawVert_t vertices[3] = { 0 };
	int indices[3] = { 0, 1, 2 };
	dshader_t shader = { 0 };
	dmodel_t subModel = { 0 };
	byte lightGrid[8] = { 32u, 64u, 96u, 10u, 20u, 30u, 0u, 0u };
	refEntity_t entity = { 0 };
	const renderEntityCommand_t *commands;
	uint32_t commandCount;
	qhandle_t material;
	memset( &world, 0, sizeof( world ) );
	strcpy( world.name, "maps/light-grid-fixture.bsp" );
	world.checksum = 0x11223344;
	world.numSurfaces = 1; world.surfaces = &surface;
	world.numDrawVerts = 3; world.drawVerts = vertices;
	world.numDrawIndexes = 3; world.drawIndexes = indices;
	world.numShaders = 1; world.shaders = &shader;
	world.numSubModels = 1; world.subModels = &subModel;
	world.numGridPoints = 1; world.lightGridData = lightGrid;
	surface.surfaceType = MST_PLANAR; surface.numVerts = 3;
	surface.numIndexes = 3; surface.lightmapNum = -1;
	strcpy( shader.shader, "textures/light-grid-fixture" );
	vertices[1].xyz[0] = 1.0f; vertices[2].xyz[1] = 1.0f;
	entity.reType = RT_MODEL;
	CHECK( RenderSubmission_Init( &state, 55u ) );
	material = RenderSubmission_RegisterAsset( &state,
		RENDER_ASSET_MATERIAL, shader.shader );
	CHECK( material > 0 && RenderSubmission_LoadWorld( &state, &world, 0 ) );
	{
		vec3_t mins, maxs;
		renderModelSnapshot_t metadata;
		qhandle_t original, other, replacement;
		uint32_t bytes = state.modelBytes;
		subModel.mins[0] = -128.0f; subModel.mins[1] = -256.0f; subModel.mins[2] = -32.0f;
		subModel.maxs[0] = 512.0f; subModel.maxs[1] = 1024.0f; subModel.maxs[2] = 64.0f;
		original = RenderSubmission_RegisterBspModel( &state, "*0" );
		CHECK( original > 0 && RenderSubmission_RegisterBspModel( &state, "*0" ) == original );
		CHECK( state.modelBytes == bytes && !RenderSubmission_ModelSnapshot( &state, original, &metadata ) );
		RenderSubmission_ModelBounds( &state, original, mins, maxs );
		CHECK( !memcmp( mins, subModel.mins, sizeof( mins ) ) && !memcmp( maxs, subModel.maxs, sizeof( maxs ) ) );
		CHECK( !RenderSubmission_RegisterBspModel( &state, "*-1" )
			&& !RenderSubmission_RegisterBspModel( &state, "*1" )
			&& !RenderSubmission_RegisterBspModel( &state, "*42949672960" ) );
		CHECK( RenderSubmission_LoadWorld( &state, &world, 1 ) );
		other = RenderSubmission_RegisterBspModel( &state, "*0" );
		CHECK( other > original );
		CHECK( RenderSubmission_UnloadWorld( &state, 0 ) );
		RenderSubmission_ModelBounds( &state, original, mins, maxs );
		CHECK( mins[0] == 0 && maxs[0] == 0 );
		RenderSubmission_ModelBounds( &state, other, mins, maxs );
		CHECK( mins[0] == -128.0f && maxs[1] == 1024.0f );
		CHECK( RenderSubmission_LoadWorld( &state, &world, 0 ) );
		replacement = RenderSubmission_RegisterBspModel( &state, "*0" );
		CHECK( replacement > other && state.modelBytes == bytes );
		CHECK( RenderSubmission_UnloadWorld( &state, 1 ) );
		RenderSubmission_ModelBounds( &state, other, mins, maxs );
		CHECK( mins[0] == 0 && maxs[0] == 0 );
		/* Preserve the original lightgrid fixture's authored bounds. */
		memset( subModel.mins, 0, sizeof( subModel.mins ) );
		memset( subModel.maxs, 0, sizeof( subModel.maxs ) );
		CHECK( RenderSubmission_LoadWorld( &state, &world, 0 ) );
		RenderSubmission_ModelBounds( &state, replacement, mins, maxs );
		CHECK( mins[0] == 0 && maxs[0] == 0 && state.modelCount == 0u );
	}
	{
		atmosphereFrameState_t atmosphere = {0};
		renderAtmosphereHeightgrid_t grid, previous;
		float heights[4] = { -64, -32, 16, 48 };
		atmosphere.type = ATMOSPHERE_PRECIP_RAIN;
		atmosphere.gridSize = 2;
		atmosphere.worldMaxs[0] = atmosphere.worldMaxs[1] = 64;
		CHECK( RenderSubmission_SetAtmosphere( &state, &atmosphere ) );
		CHECK( !RenderSubmission_AtmosphereHeightgrid( &state, &grid ) );
		CHECK( RenderSubmission_SetAtmosphereHeightgrid( &state, heights, 4 ) );
		CHECK( RenderSubmission_AtmosphereHeightgrid( &state, &grid )
			&& grid.samples != heights && grid.edge == 2 && grid.inverseStep[0] == 1.0f / 32.0f );
		previous = grid;
		CHECK( RenderSubmission_SetAtmosphereHeightgrid( &state, heights, 4 )
			&& RenderSubmission_AtmosphereHeightgrid( &state, &grid ) && grid.samples == previous.samples );
		heights[0] = NAN;
		CHECK( !RenderSubmission_SetAtmosphereHeightgrid( &state, heights, 4 )
			&& !RenderSubmission_SetAtmosphereHeightgrid( &state, heights, 3 )
			&& !RenderSubmission_SetAtmosphereHeightgrid( &state, NULL, 4 ) );
		CHECK( RenderSubmission_AtmosphereHeightgrid( &state, &grid )
			&& grid.digest == previous.digest && grid.samples[0] == -64 );
		atmosphere.worldMaxs[0] = 128;
		CHECK( RenderSubmission_SetAtmosphere( &state, &atmosphere )
			&& !RenderSubmission_AtmosphereHeightgrid( &state, &grid ) );
		atmosphere.worldMaxs[0] = 64;
		atmosphere.gridSize = RENDER_SUBMISSION_MAX_HEIGHTGRID_EDGE + 1u;
		CHECK( RenderSubmission_SetAtmosphere( &state, &atmosphere )
			&& !RenderSubmission_SetAtmosphereHeightgrid( &state, heights, 4 ) );
		atmosphere.gridSize = 2;
		CHECK( RenderSubmission_SetAtmosphere( &state, &atmosphere )
			&& RenderSubmission_AtmosphereHeightgrid( &state, &grid )
			&& grid.digest == previous.digest );
		heights[0] = 10;
		CHECK( RenderSubmission_LoadWorld( &state, &world, 1 ) );
		CHECK( !RenderSubmission_AtmosphereHeightgrid( &state, &grid ) );
		CHECK( RenderSubmission_SetAtmosphereHeightgrid( &state, heights, 4 ) );
		CHECK( RenderSubmission_SelectWorld( &state, 0 )
			&& RenderSubmission_AtmosphereHeightgrid( &state, &grid ) && grid.samples[0] == -64 );
		CHECK( RenderSubmission_UnloadWorld( &state, 0 )
			&& RenderSubmission_AtmosphereHeightgrid( &state, &grid ) && grid.samples[0] == 10 );
		CHECK( RenderSubmission_LoadWorld( &state, &world, 0 )
			&& !RenderSubmission_AtmosphereHeightgrid( &state, &grid ) );
		CHECK( RenderSubmission_UnloadWorld( &state, 1 ) );
		memset( &atmosphere, 0, sizeof( atmosphere ) );
		CHECK( RenderSubmission_SetAtmosphere( &state, &atmosphere ) );
	}
	CHECK( RenderSubmission_BeginFrame( &state, 56u ) );
	CHECK( RenderSubmission_AddEntity( &state, &entity, NULL ) );
	CHECK( RenderSubmission_AttachLegacyLightGridEntityIrradiance(
		&state, 0u, 0.6f, 1.0f ) );
	commands = RenderSubmission_EntityCommands( &state, &commandCount );
	CHECK( commands && commandCount == 1u && commands[0].hasLocalIrradiance
		&& commands[0].legacyLightGrid
		&& state.localIrradianceEntityCount == 1u
		&& commands[0].localIrradiance.blendedCoefficientsQ16[0][0] > 0
		&& commands[0].localIrradiance.blendedCoefficientsQ16[3][2] > 0
		&& commands[0].localIrradiance.blendedCoefficientsQ16[1][0] == 0
		&& commands[0].localIrradiance.blendedCoefficientsQ16[2][1] == 0 );
	const ralIrradianceEntitySampleReceipt_t sample = commands[0].localIrradiance;
	CHECK( RenderSubmission_AddEntity( &state, &entity, NULL ) );
	CHECK( RenderSubmission_AttachEntityIrradiance( &state, 1u, &sample ) );
	CHECK( !state.entities[1].legacyLightGrid );
	/* A failed duplicate cannot relabel an already attached linear sample. */
	CHECK( !wired_render_attach_legacy_grid_irradiance( &state, 1u, &sample ) );
	CHECK( !state.entities[1].legacyLightGrid );
	RenderSubmission_CancelFrame( &state );
	RenderSubmission_Reset( &state );
	return 0;
}

static const void *s_lightingSidecarBytes;
static int s_lightingSidecarByteLength;

static int LightingSidecarReadFile( const char *name, void **bytes ) {
	if ( !name || !bytes || ( strcmp( name, "maps/test.wlight" ) &&
		 strcmp( name, "maps/test.wprobe" ) ) ) return -1;
	*bytes = (void *)s_lightingSidecarBytes;
	return s_lightingSidecarByteLength;
}

static int TestLightingSidecar( void ) {
	ralLightingArtifactDefinition_t definition;
	ralLightingPayloadView_t payloads[2];
	ralLightingArtifactReceipt_t artifactReceipt;
	renderLightingSidecarReceipt_t receipt, untouched;
	static renderSubmissionState_t submission;
	refimport_t imports;
	uint8_t radiance[4] = { 1u, 2u, 3u, 4u };
	uint8_t direction[2] = { 127u, 127u };
	uint8_t artifact[512];
	uint8_t invalid[4] = { 0u };
	memset( &definition, 0, sizeof( definition ) );
	definition.schemaVersion = RAL_LIGHTING_ARTIFACT_SCHEMA_VERSION;
	definition.kind = RAL_LIGHTING_ARTIFACT_DIRECTIONAL_LIGHTMAP;
	definition.artifactGeneration = 51u;
	definition.cacheKey = 52u;
	definition.producerVersion = 53u;
	definition.encoding = RAL_STATIC_LIGHTING_ENCODING_RGB9E5_OCT8;
	definition.flags = RAL_LIGHTING_ARTIFACT_STATIC_DIFFUSE_INDIRECT_ONLY;
	definition.dimensions[0] = definition.dimensions[1] = definition.dimensions[2] = 1u;
	definition.payloadCount = 2u;
	payloads[0] = (ralLightingPayloadView_t){ RAL_LIGHTING_PAYLOAD_RADIANCE,
		radiance, sizeof( radiance ) };
	payloads[1] = (ralLightingPayloadView_t){ RAL_LIGHTING_PAYLOAD_DIRECTION,
		direction, sizeof( direction ) };
	CHECK( Ral_LightingArtifactWrite( &definition, payloads, artifact,
		sizeof( artifact ), &artifactReceipt ) );
	memset( &imports, 0, sizeof( imports ) );
	imports.FS_ReadFile = LightingSidecarReadFile;
	imports.FS_FreeFile = MaterialScriptFreeFile;
	CHECK( RenderSubmission_Init( &submission, 2u ) );
	s_lightingSidecarBytes = artifact;
	s_lightingSidecarByteLength = (int)artifactReceipt.byteLength;
	CHECK( RenderLightingSidecar_LoadDirectional( &submission, &imports,
		"maps/test.bsp", &receipt ) );
	CHECK( RenderLightingSidecar_ReceiptValid( &receipt )
		&& receipt.status == RENDER_LIGHTING_SIDECAR_MODERN_LOADED
		&& receipt.artifactGeneration == 51u && receipt.cacheKey == 52u
		&& !strcmp( receipt.path, "maps/test.wlight" ) );
	RenderSubmission_Reset( &submission );
	CHECK( RenderSubmission_Init( &submission, 3u ) );
	s_lightingSidecarBytes = NULL;
	s_lightingSidecarByteLength = -1;
	CHECK( RenderLightingSidecar_LoadDirectional( &submission, &imports,
		"maps/test", &receipt ) );
	CHECK( RenderLightingSidecar_ReceiptValid( &receipt )
		&& receipt.status == RENDER_LIGHTING_SIDECAR_MISSING_COMPATIBILITY );
	s_lightingSidecarBytes = invalid;
	s_lightingSidecarByteLength = (int)sizeof( invalid );
	memset( &receipt, 0xa5, sizeof( receipt ) );
	untouched = receipt;
	CHECK( !RenderLightingSidecar_LoadDirectional( &submission, &imports,
		"maps/test.bsp", &receipt ) );
	CHECK( !memcmp( &receipt, &untouched, sizeof( receipt ) ) );
	RenderSubmission_Reset( &submission );
	return 0;
}

static qboolean LightingVisibilityFixtureQuery(
	void *userData, uint64_t receiverTriangleId, uint64_t emitterTriangleId,
	const ralLightVec3Q16_t *receiverCentroid, const ralLightVec3Q16_t *emitterCentroid,
	uint32_t *outVisibilityQ16, uint64_t *outQueryProvenance )
{
	const lightingVisibilityFixture_t *fixture = (const lightingVisibilityFixture_t *)userData;
	if ( !fixture || !receiverTriangleId || !emitterTriangleId || !receiverCentroid || !emitterCentroid ||
		 !outVisibilityQ16 || !outQueryProvenance )
		return qfalse;
	*outVisibilityQ16 = emitterTriangleId == fixture->occludedEmitterId ? 0u : RAL_LIGHT_Q16_ONE;
	*outQueryProvenance = receiverTriangleId ^ ( emitterTriangleId << 1u ) ^ UINT64_C( 0x9e3779b97f4a7c15 );
	if ( !*outQueryProvenance )
		*outQueryProvenance = 1u;
	return qtrue;
}

static void PutHalf( uint8_t *bytes, uint16_t half )
{
	bytes[0] = (uint8_t)half;
	bytes[1] = (uint8_t)( half >> 8u );
}

static int TestIrradianceVolumeOwner( void )
{
	static renderSubmissionState_t state;
	static renderSubmissionState_t loadedState;
	ralLightingArtifactDefinition_t definition;
	ralLightingPayloadView_t payloads[2];
	ralLightingArtifactReceipt_t artifactReceipt;
	ralIrradianceVolumePlacement_t placement;
	ralLightVec3Q16_t normal = { 0, 0, RAL_LIGHT_Q16_ONE };
	int32_t fallback[3] = { RAL_LIGHT_Q16_ONE / 4, RAL_LIGHT_Q16_ONE / 2, RAL_LIGHT_Q16_ONE };
	uint8_t coefficients[8u * 24u] = { 0 }, validity[8], artifact[512], artifactBackup[512], corrupt[512];
	uint8_t sidecar[1024], corruptSidecar[1024];
	uint8_t directionalRadiance[16] = { 0 }, directionalDirection[8] = { 0 };
	uint8_t directionalArtifact[512], directionalArtifactBackup[512];
	refEntity_t entity;
	refdef_t view;
	renderSubmissionReceipt_t receipt;
	const renderIrradianceVolumeRecord_t *volumeSnapshot;
	uint32_t volumeCount;
	uint64_t volumeDigest;
	uint64_t sidecarLength, sidecarManifest, loadedDigest, directionalArtifactLength = 0u;
	renderIrradianceVolumeSource_t source;
	renderIrradianceSidecarReceipt_t sidecarReceipt, untouchedSidecarReceipt;
	refimport_t imports;
	const renderDirectionalLightingRecord_t *directionalSnapshot;
	uint64_t directionalDigest;
	uint32_t probe, channel;
	for ( probe = 0u; probe < 8u; ++probe ) {
		for ( channel = 0u; channel < 3u; ++channel )
			PutHalf( coefficients + probe * 24u + channel * 2u, UINT16_C( 0x3c00 ) );
		PutHalf( coefficients + probe * 24u + 18u, UINT16_C( 0x3c00 ) );
		validity[probe] = 255u;
	}
	memset( &definition, 0, sizeof( definition ) );
	definition.schemaVersion = RAL_LIGHTING_ARTIFACT_SCHEMA_VERSION;
	definition.kind = RAL_LIGHTING_ARTIFACT_IRRADIANCE_VOLUME;
	definition.artifactGeneration = 3u;
	definition.cacheKey = 4u;
	definition.producerVersion = 5u;
	definition.encoding = RAL_IRRADIANCE_SH_L1_RGB16F;
	definition.dimensions[0] = definition.dimensions[1] = definition.dimensions[2] = 2u;
	definition.payloadCount = 2u;
	payloads[0] = (ralLightingPayloadView_t){ RAL_LIGHTING_PAYLOAD_SH_COEFFICIENTS, coefficients,
		sizeof( coefficients ) };
	payloads[1] = (ralLightingPayloadView_t){ RAL_LIGHTING_PAYLOAD_PROBE_VALIDITY, validity, sizeof( validity ) };
	CHECK( Ral_LightingArtifactWrite( &definition, payloads, artifact, sizeof( artifact ), &artifactReceipt ) );
	memcpy( artifactBackup, artifact, sizeof( artifactBackup ) );
	{
		ralLightingArtifactDefinition_t directionalDefinition;
		ralLightingPayloadView_t directionalPayloads[2];
		ralLightingArtifactReceipt_t directionalReceipt;
		memset( &directionalDefinition, 0, sizeof( directionalDefinition ) );
		directionalDefinition.schemaVersion = RAL_LIGHTING_ARTIFACT_SCHEMA_VERSION;
		directionalDefinition.kind = RAL_LIGHTING_ARTIFACT_DIRECTIONAL_LIGHTMAP;
		directionalDefinition.artifactGeneration = 13u;
		directionalDefinition.cacheKey = 14u;
		directionalDefinition.producerVersion = 15u;
		directionalDefinition.encoding = RAL_STATIC_LIGHTING_ENCODING_RGB9E5_OCT8;
		directionalDefinition.flags = RAL_LIGHTING_ARTIFACT_STATIC_DIFFUSE_INDIRECT_ONLY;
		directionalDefinition.dimensions[0] = directionalDefinition.dimensions[1] = 2u;
		directionalDefinition.dimensions[2] = 1u;
		directionalDefinition.payloadCount = 2u;
		directionalPayloads[0] = (ralLightingPayloadView_t){ RAL_LIGHTING_PAYLOAD_RADIANCE,
			directionalRadiance, sizeof( directionalRadiance ) };
		directionalPayloads[1] = (ralLightingPayloadView_t){ RAL_LIGHTING_PAYLOAD_DIRECTION,
			directionalDirection, sizeof( directionalDirection ) };
		CHECK( Ral_LightingArtifactWrite( &directionalDefinition, directionalPayloads, directionalArtifact,
			sizeof( directionalArtifact ), &directionalReceipt ) );
		directionalArtifactLength = directionalReceipt.byteLength;
		memcpy( directionalArtifactBackup, directionalArtifact,
			sizeof( directionalArtifactBackup ) );
		CHECK( RenderSubmission_Init( &state, 71u ) );
		CHECK( RenderSubmission_RegisterDirectionalLighting( &state, directionalArtifact,
			directionalReceipt.byteLength ) );
	}
	memset( &placement, 0, sizeof( placement ) );
	placement.schemaVersion = RAL_IRRADIANCE_PLACEMENT_SCHEMA_VERSION;
	placement.volumeId = 7u;
	placement.sourceGeneration = definition.artifactGeneration;
	placement.provenanceHash = 9u;
	placement.spacing.x = placement.spacing.y = placement.spacing.z = RAL_LIGHT_Q16_ONE;
	placement.boundsMax.x = placement.boundsMax.y = placement.boundsMax.z = RAL_LIGHT_Q16_ONE;
	placement.dimensions[0] = placement.dimensions[1] = placement.dimensions[2] = 2u;
	placement.priority = 10u;
	placement.blendDistanceQ16 = RAL_LIGHT_Q16_ONE / 4;
	placement.fallback = RAL_IRRADIANCE_FALLBACK_LIGHTGRID;
	placement.ready = qtrue;
	memset( &source, 0, sizeof( source ) );
	source.placement = placement;
	source.artifactBytes = artifact;
	source.artifactByteLength = artifactReceipt.byteLength;
	CHECK( RenderLightingSidecar_PackIrradiance( &source, 1u, sidecar,
		sizeof( sidecar ), &sidecarLength, &sidecarManifest ) );
	CHECK( sidecarLength > RENDER_IRRADIANCE_SIDECAR_HEADER_BYTES && sidecarManifest );
	memset( &imports, 0, sizeof( imports ) );
	imports.FS_ReadFile = LightingSidecarReadFile;
	imports.FS_FreeFile = MaterialScriptFreeFile;
	CHECK( RenderSubmission_Init( &loadedState, 72u ) );
	s_lightingSidecarBytes = sidecar;
	s_lightingSidecarByteLength = (int)sidecarLength;
	CHECK( RenderLightingSidecar_LoadIrradiance( &loadedState, &imports,
		"maps/test.bsp", &sidecarReceipt ) );
	CHECK( RenderLightingSidecar_IrradianceReceiptValid( &sidecarReceipt ) &&
		sidecarReceipt.status == RENDER_IRRADIANCE_SIDECAR_MODERN_LOADED &&
		sidecarReceipt.volumeCount == 1u && sidecarReceipt.manifestHash == sidecarManifest &&
		!strcmp( sidecarReceipt.path, "maps/test.wprobe" ) );
	loadedDigest = loadedState.irradianceVolumeDigest;
	memcpy( corruptSidecar, sidecar, (size_t)sidecarLength );
	corruptSidecar[sidecarLength - 1u] ^= 1u;
	s_lightingSidecarBytes = corruptSidecar;
	memset( &sidecarReceipt, 0xa5, sizeof( sidecarReceipt ) );
	untouchedSidecarReceipt = sidecarReceipt;
	CHECK( !RenderLightingSidecar_LoadIrradiance( &loadedState, &imports,
		"maps/test.bsp", &sidecarReceipt ) );
	CHECK( !memcmp( &sidecarReceipt, &untouchedSidecarReceipt, sizeof( sidecarReceipt ) ) &&
		loadedState.irradianceVolumeCount == 1u && loadedState.irradianceVolumeDigest == loadedDigest );
	RenderSubmission_Reset( &loadedState );
	CHECK( RenderSubmission_RegisterIrradianceVolume( &state, &placement, artifact, artifactReceipt.byteLength ) );
	CHECK( state.irradianceVolumeCount == 1u && state.irradianceVolumeDigest != 0u );
	CHECK( RenderSubmission_IrradianceVolumeSnapshot( &state, &volumeSnapshot, &volumeCount, &volumeDigest ) &&
		volumeSnapshot && volumeCount == 1u && volumeDigest == state.irradianceVolumeDigest &&
		volumeSnapshot[0].placement.volumeId == 7u && volumeSnapshot[0].validityCount == 8u );
	CHECK( RenderSubmission_DirectionalLightingSnapshot( &state, &directionalSnapshot, &directionalDigest ) &&
		directionalSnapshot && directionalSnapshot->artifact.kind == RAL_LIGHTING_ARTIFACT_DIRECTIONAL_LIGHTMAP &&
		directionalDigest == state.directionalLightingDigest );
	CHECK( !RenderSubmission_RegisterDirectionalLighting( &state, directionalArtifact,
		directionalSnapshot->artifactByteLength ) );
	memset( directionalArtifact, 0, sizeof( directionalArtifact ) );
	CHECK( !RenderSubmission_RegisterIrradianceVolume( &state, &placement, artifact, artifactReceipt.byteLength ) );
	memcpy( corrupt, artifact, artifactReceipt.byteLength );
	corrupt[RAL_LIGHTING_ARTIFACT_WIRE_HEADER_BYTES] ^= 1u;
	placement.volumeId = 8u;
	CHECK( !RenderSubmission_RegisterIrradianceVolume( &state, &placement, corrupt, artifactReceipt.byteLength ) );
	CHECK( state.irradianceVolumeCount == 1u );
	memset( artifact, 0, sizeof( artifact ) );
	memset( &entity, 0, sizeof( entity ) );
	memset( &view, 0, sizeof( view ) );
	entity.reType = RT_MODEL;
	entity.origin[0] = entity.origin[1] = entity.origin[2] = 0.5f;
	CHECK( RenderSubmission_BeginFrame( &state, 19u ) );
	CHECK( RenderSubmission_AddEntity( &state, &entity, NULL ) );
	CHECK( RenderSubmission_RenderScene( &state, &view, 0 ) );
	CHECK( !state.entities[0].localIrradiance.usedFallback &&
		state.entities[0].localIrradiance.blendedCoefficientsQ16[0][0] == RAL_LIGHT_Q16_ONE &&
		state.entities[0].localIrradiance.blendedCoefficientsQ16[3][0] == RAL_LIGHT_Q16_ONE );
	CHECK( !RenderSubmission_ClearIrradianceVolumes( &state ) );
	CHECK( RenderSubmission_EndFrame( &state, 19u, &receipt ) );
	CHECK( receipt.irradianceVolumeCount == 1u && receipt.directionalLightingCount == 1u &&
		receipt.localIrradianceEntityCount == 1u &&
		receipt.irradianceVolumeDigest == state.irradianceVolumeDigest );
	entity.origin[0] = entity.origin[1] = entity.origin[2] = 2.0f;
	CHECK( RenderSubmission_BeginFrame( &state, 20u ) );
	CHECK( RenderSubmission_AddEntity( &state, &entity, NULL ) );
	CHECK( RenderSubmission_AttachConfiguredEntityIrradiance( &state, 0u, &normal, fallback ) );
	CHECK( state.entities[0].localIrradiance.usedFallback &&
		state.entities[0].localIrradiance.blendedCoefficientsQ16[0][0] == fallback[0] );
	RenderSubmission_CancelFrame( &state );
	CHECK( RenderSubmission_ClearIrradianceVolumes( &state ) && state.irradianceVolumeCount == 0u );
	CHECK( RenderSubmission_ClearDirectionalLighting( &state ) && state.directionalLightingCount == 0u );
	{
		mapFile_t worldA, worldB;
		uint64_t worldAIrradianceDigest, worldADirectionalDigest;
		uint64_t worldAGeneration, worldBGeneration, worldAReloadGeneration;
		renderWorldSnapshot_t resident, untouched;
		memset( &worldA, 0, sizeof( worldA ) );
		memset( &worldB, 0, sizeof( worldB ) );
		strcpy( worldA.name, "maps/world-a.bsp" );
		strcpy( worldB.name, "maps/world-b.bsp" );
		CHECK( RenderSubmission_LoadWorld( &state, &worldA, 0 )
			&& RenderSubmission_WorldLifecycleGeneration( &state, 0,
				&worldAGeneration ) );
		memset( &untouched, 0xa5, sizeof( untouched ) );
		resident = untouched;
		CHECK( !state.frameOpen && !state.frameSealed );
		CHECK( !RenderSubmission_WorldSnapshot( &state, &resident )
			&& !memcmp( &resident, &untouched, sizeof( resident ) ) );
		CHECK( RenderSubmission_ResidentWorldSnapshot( &state, 0,
			worldAGeneration, &resident ) && resident.ready );
		resident = untouched;
		CHECK( !RenderSubmission_ResidentWorldSnapshot( &state, 0, 0u, &resident )
			&& !RenderSubmission_ResidentWorldSnapshot( &state, -1, worldAGeneration, &resident )
			&& !RenderSubmission_ResidentWorldSnapshot( &state, MAX_RENDER_WORLDS, worldAGeneration, &resident )
			&& !memcmp( &resident, &untouched, sizeof( resident ) ) );
		placement.volumeId = 70u;
		CHECK( RenderSubmission_RegisterIrradianceVolume( &state, &placement,
			artifactBackup, artifactReceipt.byteLength ) );
		CHECK( RenderSubmission_RegisterDirectionalLighting( &state,
			directionalArtifactBackup, directionalArtifactLength ) );
		worldAIrradianceDigest = state.irradianceVolumeDigest;
		worldADirectionalDigest = state.directionalLightingDigest;
		CHECK( RenderSubmission_LoadWorld( &state, &worldB, 1 )
			&& RenderSubmission_WorldLifecycleGeneration( &state, 1,
				&worldBGeneration )
			&& worldBGeneration > worldAGeneration
			&& state.irradianceVolumeCount == 0u
			&& state.directionalLightingCount == 0u );
		/* Resident A is readable while B is active, without selecting A or
		 * manufacturing frame state. B's catalog cannot authorize A. */
		CHECK( RenderSubmission_ResidentWorldSnapshot( &state, 0,
			worldAGeneration, &resident ) && state.activeWorldIndex == 1 );
		resident = untouched;
		CHECK( !RenderSubmission_ResidentWorldSnapshot( &state, 0,
			worldBGeneration, &resident )
			&& !memcmp( &resident, &untouched, sizeof( resident ) ) );
		placement.volumeId = 71u;
		CHECK( RenderSubmission_RegisterIrradianceVolume( &state, &placement,
			artifactBackup, artifactReceipt.byteLength ) );
		CHECK( RenderSubmission_SelectWorld( &state, 0 )
			&& state.irradianceVolumeCount == 1u
			&& state.directionalLightingCount == 1u
			&& state.irradianceVolumeDigest == worldAIrradianceDigest
			&& state.directionalLightingDigest == worldADirectionalDigest
			&& state.irradianceVolumes[0].placement.volumeId == 70u );
		CHECK( RenderSubmission_SelectWorld( &state, 1 )
			&& state.irradianceVolumeCount == 1u
			&& state.directionalLightingCount == 0u
			&& state.irradianceVolumes[0].placement.volumeId == 71u );
		CHECK( RenderSubmission_UnloadWorld( &state, 0 )
			&& RenderSubmission_WorldResident( &state, 1 )
			&& !RenderSubmission_WorldLifecycleGeneration( &state, 0,
				&worldAReloadGeneration )
			&& state.irradianceVolumes[0].placement.volumeId == 71u );
		CHECK( RenderSubmission_LoadWorld( &state, &worldA, 0 )
			&& RenderSubmission_WorldLifecycleGeneration( &state, 0,
				&worldAReloadGeneration )
			&& worldAReloadGeneration > worldBGeneration );
		resident = untouched;
		CHECK( !RenderSubmission_ResidentWorldSnapshot( &state, 0,
			worldAGeneration, &resident )
			&& !memcmp( &resident, &untouched, sizeof( resident ) ) );
		CHECK( RenderSubmission_ResidentWorldSnapshot( &state, 0,
			worldAReloadGeneration, &resident ) );
		CHECK( RenderSubmission_UnloadWorld( &state, 0 ) );
		resident = untouched;
		CHECK( !RenderSubmission_ResidentWorldSnapshot( &state, 0,
			worldAReloadGeneration, &resident )
			&& !memcmp( &resident, &untouched, sizeof( resident ) ) );
		CHECK( RenderSubmission_UnloadWorld( &state, 1 )
			&& RenderSubmission_ResidentWorldCount( &state ) == 0 );
	}
	RenderSubmission_Reset( &state );
	return 0;
}

_Static_assert( sizeof( atmosphereEffectStage_t ) == 64u, "atmosphere stage ABI must remain one 64-byte GPU record" );
_Static_assert( sizeof( atmosphereEffectProfile_t ) == 592u,
				"atmosphere profile ABI must remain fixed-width and pointer-free" );

typedef struct {
	char	 magic[16];
	uint32_t version, filesize, flags;
	uint32_t numText, ofsText, numMeshes, ofsMeshes;
	uint32_t numVertexArrays, numVertexes, ofsVertexArrays;
	uint32_t numTriangles, ofsTriangles, ofsAdjacency;
	uint32_t numJoints, ofsJoints, numPoses, ofsPoses, numAnims, ofsAnims;
	uint32_t numFrames, numFrameChannels, ofsFrames, ofsBounds;
	uint32_t numComment, ofsComment, numExtensions, ofsExtensions;
} testIqmHeader_t;
typedef struct {
	uint32_t name, material, firstVertex, numVertexes, firstTriangle, numTriangles;
} testIqmMesh_t;
typedef struct {
	uint32_t type, flags, format, size, offset;
} testIqmArray_t;

static int TestModelPayloads( void )
{
	static renderSubmissionState_t state;
	renderModelSnapshot_t	snapshot;
	byte					md3[2048] = { 0 }, iqm[512] = { 0 };
	md3Header_t			   *header	= (md3Header_t *)md3;
	md3Tag_t			   *tags = (md3Tag_t *)( md3 + sizeof( *header ) );
	md3Surface_t		   *surface = (md3Surface_t *)( tags + 2u );
	uint32_t				offset	= sizeof( *surface );
	md3Triangle_t		   *triangle;
	md3Shader_t			   *shader;
	md3St_t				   *st;
	md3XyzNormal_t		   *xyz;
	header->ident		  = MD3_IDENT;
	header->version		  = MD3_VERSION;
	header->numFrames	  = 2;
	header->numTags		  = 1;
	header->numSurfaces	  = 1;
	header->ofsTags		  = sizeof( *header );
	header->ofsSurfaces	  = sizeof( *header ) + 2u * sizeof( *tags );
	strcpy( tags[0].name, "tag_torso" );
	strcpy( tags[1].name, "tag_torso" );
	tags[0].origin[0] = 1.0f;
	tags[1].origin[0] = 3.0f;
	for ( int axis = 0; axis < 3; ++axis ) {
		tags[0].axis[axis][axis] = 1.0f;
		tags[1].axis[axis][axis] = 1.0f;
	}
	surface->ident		  = MD3_IDENT;
	surface->numFrames	  = 2;
	surface->numShaders	  = 1;
	surface->numVerts	  = 3;
	surface->numTriangles = 1;
	strcpy( surface->name, "l_legs" );
	surface->ofsTriangles = offset;
	triangle			  = (md3Triangle_t *)( (byte *)surface + offset );
	triangle->indexes[0]  = 0u;
	triangle->indexes[1]  = 1u;
	triangle->indexes[2]  = 2u;
	offset += sizeof( *triangle );
	surface->ofsShaders = offset;
	shader				= (md3Shader_t *)( (byte *)surface + offset );
	strcpy( shader->name, "textures/model" );
	offset += sizeof( *shader );
	surface->ofsSt = offset;
	st			   = (md3St_t *)( (byte *)surface + offset );
	st[1].st[0]	   = 1.0f;
	st[2].st[1]	   = 1.0f;
	offset += 3u * sizeof( *st );
	surface->ofsXyzNormals = offset;
	xyz					   = (md3XyzNormal_t *)( (byte *)surface + offset );
	xyz[1].xyz[0]		   = 64;
	xyz[2].xyz[1]		   = 64;
	xyz[3].xyz[2]		   = 64;
	xyz[4].xyz[0]		   = 64;
	xyz[5].xyz[1]		   = 64;
	offset += 6u * sizeof( *xyz );
	surface->ofsEnd = offset;
	header->ofsEnd	= header->ofsSurfaces + offset;
	CHECK( RenderSubmission_Init( &state, 51u ) );
	qhandle_t model = RenderSubmission_RegisterModelData( &state, "models/test.md3", md3, header->ofsEnd );
	vec3_t modelMins, modelMaxs;
	RenderSubmission_ModelBounds( &state, model, modelMins, modelMaxs );
	CHECK( modelMins[0] == 0 && modelMins[1] == 0 && modelMins[2] == 0
		&& modelMaxs[0] == 1 && modelMaxs[1] == 1 && modelMaxs[2] == 0 );
	/* Cache source bounds once, preserving the authored first pose rather than
	 * unioning every animation frame or rescanning vertices at query time. */
	int geometryEnd = header->ofsEnd;
	md3Frame_t frames[2] = {0};
	for ( int axis = 0; axis < 3; ++axis ) {
		frames[0].bounds[0][axis] = -10.0f - axis;
		frames[0].bounds[1][axis] = 20.0f + axis;
		frames[1].bounds[0][axis] = -100.0f;
		frames[1].bounds[1][axis] = 100.0f;
	}
	memcpy( md3 + geometryEnd, frames, sizeof( frames ) );
	header->ofsFrames = geometryEnd;
	header->ofsEnd += sizeof( frames );
	qhandle_t authored = RenderSubmission_RegisterModelData( &state, "models/bounds.md3", md3, header->ofsEnd );
	CHECK( authored > model );
	memset( md3 + geometryEnd, 0, sizeof( frames ) );
	RenderSubmission_ModelBounds( &state, authored, modelMins, modelMaxs );
	CHECK( modelMins[0] == -10 && modelMins[2] == -12
		&& modelMaxs[0] == 20 && modelMaxs[2] == 22 );
	/* Bytes after the declared file end must never supply authored bounds. */
	memcpy( md3 + geometryEnd, frames, sizeof( frames ) );
	header->ofsEnd = geometryEnd;
	qhandle_t trailing = RenderSubmission_RegisterModelData( &state, "models/trailing.md3", md3, sizeof( md3 ) );
	CHECK( trailing > authored );
	RenderSubmission_ModelBounds( &state, trailing, modelMins, modelMaxs );
	CHECK( modelMins[0] == 0 && modelMaxs[0] == 1 && modelMaxs[2] == 0 );
	header->ofsFrames = 0;
	CHECK( model > 0 && RenderSubmission_ModelSnapshot( &state, model, &snapshot ) &&
		   snapshot.format == RENDER_MODEL_MD3 && snapshot.frameCount == 2u && snapshot.vertexCount == 3u &&
		   snapshot.indexCount == 3u && snapshot.batchCount == 1u && snapshot.tagCount == 1u &&
		   snapshot.positions[9u + 2u] == 1.0f &&
		   snapshot.normals && fabsf( snapshot.normals[2u] - 1.0f ) < 0.0001f &&
		   !strcmp( snapshot.batches[0].materialName, "textures/model" ) );
	orientation_t tag;
	CHECK( RenderSubmission_LerpTag( &state, &tag, model, 0, 1, 0.25f,
		"tag_torso" ) && fabsf( tag.origin[0] - 1.5f ) < 0.0001f
		&& fabsf( tag.axis[0][0] - 1.0f ) < 0.0001f );
	qhandle_t material = RenderSubmission_RegisterAsset( &state, RENDER_ASSET_MATERIAL, "textures/model" );
	CHECK( material > 0 && RenderSubmission_SetModelBatchMaterial( &state, model, 0u, material ) &&
		   RenderSubmission_ModelSnapshot( &state, model, &snapshot ) && snapshot.batches[0].material == material );
	cmSkin_t characterSkin;
	refEntity_t skinnedEntity;
	memset( &characterSkin, 0, sizeof( characterSkin ) );
	memset( &skinnedEntity, 0, sizeof( skinnedEntity ) );
	characterSkin.overrideCount = 1;
	strcpy( characterSkin.overrides[0].surfaceName, "l_legs" );
	characterSkin.overrides[0].surfaceNameHash = Q_HashSurfaceName( "l_legs" );
	characterSkin.overrides[0].shader = material + 1;
	skinnedEntity.reType = RT_MODEL;
	skinnedEntity.hModel = model;
	skinnedEntity.characterSkin = 1;
	CHECK( RenderSubmission_BeginFrame( &state, 52u )
		&& RenderSubmission_AddEntitySkinned( &state, &skinnedEntity, NULL,
			&characterSkin ) );
	uint32_t skinnedCount = 0u;
	const renderEntityCommand_t *skinnedCommands =
		RenderSubmission_EntityCommands( &state, &skinnedCount );
	CHECK( skinnedCount == 1u && skinnedCommands
		&& RenderSubmission_EntityBatchMaterial( &state, &skinnedCommands[0],
			&snapshot.batches[0] ) == material + 1 );
	RenderSubmission_CancelFrame( &state );

	testIqmHeader_t *iqmHeader = (testIqmHeader_t *)iqm;
	memcpy( iqmHeader->magic, "INTERQUAKEMODEL", 16u );
	iqmHeader->version = 2u;
	iqmHeader->numText = 16u;
	iqmHeader->ofsText = sizeof( *iqmHeader );
	memcpy( iqm + iqmHeader->ofsText, "textures/iqm\0", 13u );
	iqmHeader->numMeshes	   = 1u;
	iqmHeader->ofsMeshes	   = iqmHeader->ofsText + 16u;
	testIqmMesh_t *mesh		   = (testIqmMesh_t *)( iqm + iqmHeader->ofsMeshes );
	mesh->material			   = 0u;
	mesh->numVertexes		   = 3u;
	mesh->numTriangles		   = 1u;
	iqmHeader->numVertexArrays = 2u;
	iqmHeader->numVertexes	   = 3u;
	iqmHeader->ofsVertexArrays = iqmHeader->ofsMeshes + sizeof( *mesh );
	testIqmArray_t *arrays	   = (testIqmArray_t *)( iqm + iqmHeader->ofsVertexArrays );
	arrays[0].type			   = 0u;
	arrays[0].format		   = 7u;
	arrays[0].size			   = 3u;
	arrays[0].offset		   = iqmHeader->ofsVertexArrays + 3u * sizeof( *arrays );
	float *positions		   = (float *)( iqm + arrays[0].offset );
	positions[3]			   = 1.0f;
	positions[7]			   = 1.0f;
	arrays[1].type			   = 1u;
	arrays[1].format		   = 7u;
	arrays[1].size			   = 2u;
	arrays[1].offset		   = arrays[0].offset + 9u * sizeof( float );
	float *texCoords		   = (float *)( iqm + arrays[1].offset );
	texCoords[2]			   = 1.0f;
	texCoords[5]			   = 1.0f;
	iqmHeader->numTriangles	   = 1u;
	iqmHeader->ofsTriangles	   = arrays[1].offset + 6u * sizeof( float );
	uint32_t *iqmTriangle	   = (uint32_t *)( iqm + iqmHeader->ofsTriangles );
	iqmTriangle[0]			   = 0u;
	iqmTriangle[1]			   = 1u;
	iqmTriangle[2]			   = 2u;
	iqmHeader->filesize		   = iqmHeader->ofsTriangles + 3u * sizeof( uint32_t );
	qhandle_t iqmModel = RenderSubmission_RegisterModelData( &state, "models/test.iqm", iqm, iqmHeader->filesize );
	RenderSubmission_ModelBounds( &state, iqmModel, modelMins, modelMaxs );
	CHECK( modelMins[0] == 0 && modelMins[1] == 0 && modelMins[2] == 0
		&& modelMaxs[0] == 1 && modelMaxs[1] == 1 && modelMaxs[2] == 0 );
	CHECK( iqmModel > model && RenderSubmission_ModelSnapshot( &state, iqmModel, &snapshot ) &&
		   snapshot.format == RENDER_MODEL_IQM && snapshot.frameCount == 1u && snapshot.vertexCount == 3u &&
		   snapshot.indexCount == 3u && snapshot.batchCount == 1u &&
		   !strcmp( snapshot.batches[0].materialName, "textures/iqm" ) );

	CHECK( snapshot.colors == NULL ); // Missing IQM color means white, not entity tint.
	const byte authoredColors[12] = {19,151,93,87, 255,0,128,51, 0,255,64,204};
	arrays[2].type = 6u; arrays[2].format = 1u; arrays[2].size = 4u;
	arrays[2].offset = iqmHeader->filesize;
	memcpy(iqm + arrays[2].offset, authoredColors, sizeof(authoredColors));
	iqmHeader->filesize += sizeof(authoredColors); iqmHeader->numVertexArrays = 3u;
	const uint32_t beforeBytes = state.modelBytes;
	qhandle_t colored = RenderSubmission_RegisterModelData(&state,"models/colored.iqm",iqm,iqmHeader->filesize);
	CHECK(colored > iqmModel && RenderSubmission_ModelSnapshot(&state,colored,&snapshot));
	CHECK(snapshot.colors && !memcmp(snapshot.colors,authoredColors,sizeof(authoredColors)));
	CHECK(state.modelBytes - beforeBytes == 3u * (3u+3u+2u) * sizeof(float)
		+ 3u * sizeof(uint32_t) + sizeof(renderModelBatch_t) + sizeof(authoredColors));
	// Verify owned immutable input and color participation in the existing digest.
	iqm[arrays[2].offset] ^= 127;
	CHECK(!memcmp(snapshot.colors,authoredColors,sizeof(authoredColors)));
	qhandle_t colorMaterial = RenderSubmission_RegisterAsset(&state,RENDER_ASSET_MATERIAL,"textures/color-test");
	CHECK(RenderSubmission_SetModelBatchMaterial(&state,colored,0,colorMaterial));
	CHECK(RenderSubmission_ModelSnapshot(&state,colored,&snapshot));
	uint64_t colorDigest = snapshot.digest;
	((color4ub_t *)snapshot.colors)[0].rgba[0] ^= 1; // Test-only mutation of owned bytes.
	CHECK(RenderSubmission_SetModelBatchMaterial(&state,colored,0,colorMaterial));
	CHECK(RenderSubmission_ModelSnapshot(&state,colored,&snapshot) && snapshot.digest != colorDigest);
	((color4ub_t *)snapshot.colors)[0].rgba[0] ^= 1;
	const uint32_t beforeCount = state.modelCount, ownedBytes = state.modelBytes;
	arrays[2].offset = iqmHeader->filesize-1u;
	CHECK(!RenderSubmission_RegisterModelData(&state,"models/color-truncated.iqm",iqm,iqmHeader->filesize));
	arrays[2].offset = iqmHeader->filesize-sizeof(authoredColors); arrays[2].format = 7u;
	CHECK(!RenderSubmission_RegisterModelData(&state,"models/color-format.iqm",iqm,iqmHeader->filesize));
	arrays[2].format = 1u;
	testIqmArray_t savedTexCoord = arrays[1]; arrays[1] = arrays[2];
	CHECK(!RenderSubmission_RegisterModelData(&state,"models/color-duplicate.iqm",iqm,iqmHeader->filesize));
	arrays[1] = savedTexCoord;
	state.modelBytes = RENDER_SUBMISSION_MAX_MODEL_BYTES;
	CHECK(!RenderSubmission_RegisterModelData(&state,"models/color-overbudget.iqm",iqm,iqmHeader->filesize));
	state.modelBytes = ownedBytes;
	CHECK(state.modelCount == beforeCount);
	CHECK( RenderSubmission_RegisterModelData( &state, "models/bad.md3", md3, sizeof( md3Header_t ) - 1u ) == 0 );
	/* Registration retirement and full shutdown own every decoded plane,
	 * including optional IQM colors. Exercise repeated replacement for leak
	 * tooling as well as stale-handle rejection. */
	qhandle_t retired = colored;
	for ( unsigned cycle = 0; cycle < 40; ++cycle ) {
		CHECK( RenderSubmission_ResetRegistrationAssets( &state ) );
		CHECK( !state.modelBytes && !state.modelCount
			&& !RenderSubmission_ModelSnapshot( &state, retired, &snapshot ) );
		RenderSubmission_ModelBounds( &state, retired, modelMins, modelMaxs );
		CHECK( modelMins[0] == 0 && modelMaxs[0] == 0 );
		qhandle_t next = RenderSubmission_RegisterModelData( &state,
			"models/colored.iqm", iqm, iqmHeader->filesize );
		CHECK( next > retired
			&& RenderSubmission_ModelSnapshot( &state, next, &snapshot ) && snapshot.colors );
		retired = next;
	}
	RenderSubmission_Reset( &state );
	return 0;
}

static int WriteIrradianceFixture( const char *path, int originX, int originY, int originZ )
{
	ralLightingArtifactDefinition_t definition;
	ralLightingPayloadView_t payloads[2];
	ralLightingArtifactReceipt_t artifactReceipt;
	ralIrradianceVolumePlacement_t placement;
	renderIrradianceVolumeSource_t source;
	uint8_t coefficients[8u * 24u] = { 0 }, validity[8], artifact[512], sidecar[2048];
	uint64_t sidecarLength, manifestHash;
	FILE *file;
	uint32_t probe;
	if ( !path || !path[0] ) return 1;
	for ( probe = 0u; probe < 8u; ++probe ) {
		const qboolean cool = ( probe & 1u ) != 0u;
		PutHalf( coefficients + probe * 24u + 0u, cool ? UINT16_C( 0x0000 ) : UINT16_C( 0x3400 ) );
		PutHalf( coefficients + probe * 24u + 2u, UINT16_C( 0x2800 ) );
		PutHalf( coefficients + probe * 24u + 4u, cool ? UINT16_C( 0x3400 ) : UINT16_C( 0x0000 ) );
		PutHalf( coefficients + probe * 24u + 6u, cool ? UINT16_C( 0xa800 ) : UINT16_C( 0x2800 ) );
		validity[probe] = probe == 6u ? 0u : 255u;
	}
	memset( &definition, 0, sizeof( definition ) );
	definition.schemaVersion = RAL_LIGHTING_ARTIFACT_SCHEMA_VERSION;
	definition.kind = RAL_LIGHTING_ARTIFACT_IRRADIANCE_VOLUME;
	definition.artifactGeneration = 1001u; definition.cacheKey = 1002u;
	definition.producerVersion = 1003u; definition.encoding = RAL_IRRADIANCE_SH_L1_RGB16F;
	definition.dimensions[0] = definition.dimensions[1] = definition.dimensions[2] = 2u;
	definition.payloadCount = 2u;
	payloads[0] = (ralLightingPayloadView_t){ RAL_LIGHTING_PAYLOAD_SH_COEFFICIENTS,
		coefficients, sizeof( coefficients ) };
	payloads[1] = (ralLightingPayloadView_t){ RAL_LIGHTING_PAYLOAD_PROBE_VALIDITY,
		validity, sizeof( validity ) };
	if ( !Ral_LightingArtifactWrite( &definition, payloads, artifact, sizeof( artifact ), &artifactReceipt ) )
		return 1;
	memset( &placement, 0, sizeof( placement ) );
	placement.schemaVersion = RAL_IRRADIANCE_PLACEMENT_SCHEMA_VERSION;
	placement.volumeId = 1004u; placement.sourceGeneration = definition.artifactGeneration;
	placement.provenanceHash = 1005u;
	placement.origin = (ralLightVec3Q16_t){ originX * RAL_LIGHT_Q16_ONE,
		originY * RAL_LIGHT_Q16_ONE, originZ * RAL_LIGHT_Q16_ONE };
	placement.spacing.x = 64 * RAL_LIGHT_Q16_ONE;
	placement.spacing.y = 128 * RAL_LIGHT_Q16_ONE;
	placement.spacing.z = 256 * RAL_LIGHT_Q16_ONE;
	placement.boundsMin = placement.origin;
	placement.boundsMax = (ralLightVec3Q16_t){ placement.origin.x + placement.spacing.x,
		placement.origin.y + placement.spacing.y, placement.origin.z + placement.spacing.z };
	placement.dimensions[0] = placement.dimensions[1] = placement.dimensions[2] = 2u;
	placement.priority = 100u; placement.blendDistanceQ16 = 32 * RAL_LIGHT_Q16_ONE;
	placement.fallback = RAL_IRRADIANCE_FALLBACK_LIGHTGRID; placement.ready = qtrue;
	source = (renderIrradianceVolumeSource_t){ placement, artifact, artifactReceipt.byteLength };
	if ( !RenderLightingSidecar_PackIrradiance( &source, 1u, sidecar, sizeof( sidecar ),
		&sidecarLength, &manifestHash ) ) return 1;
	file = fopen( path, "wb" );
	if ( !file ) return 1;
	if ( fwrite( sidecar, 1u, (size_t)sidecarLength, file ) != sidecarLength || fclose( file ) != 0 )
		return 1;
	fprintf( stdout, "wprobe fixture: path=%s bytes=%llu manifest=%llu\n", path,
		(unsigned long long)sidecarLength, (unsigned long long)manifestHash );
	return 0;
}

static int TestDistantWorldDrawPlanBridge( void )
{
	const int distantWorldIndex = RENDER_DISTANT_WORLD_BASE;
	static renderSubmissionState_t state;
	renderWorldVertex_t vertices[3];
	uint32_t indices[9] = { 0u, 1u, 2u, 0u, 1u, 2u, 0u, 1u, 2u };
	renderWorldBatch_t batches[3];
	renderDistantWorldDrawMetric_t metrics[3];
	ralSkyDistantDrawCandidate_t scratch[3];
	ralSkyEnvironmentPlanRequest_t skyRequest;
	ralSkyEnvironmentPlanReceipt_t skyReceipt;
	ralSkyDistantDrawPlanReceipt_t receipt, before;
	renderDistantWorldSubmission_t submission, submissionBefore;
	mapFile_t map;
	refdef_t view;
	uint32_t metricCount = 0u;
	uint64_t contentIdentity = 0u;
	memset( &state, 0, sizeof( state ) );
	memset( vertices, 0, sizeof( vertices ) );
	memset( batches, 0, sizeof( batches ) );
	memset( metrics, 0, sizeof( metrics ) );
	memset( scratch, 0, sizeof( scratch ) );
	memset( &skyRequest, 0, sizeof( skyRequest ) );
	memset( &map, 0, sizeof( map ) );
	memset( &view, 0, sizeof( view ) );
	memset( &submission, 0, sizeof( submission ) );
	state.initialized = state.frameOpen = qtrue;
	state.currentFrameGeneration = 9u;
	state.activeWorldIndex = 0;
	CHECK( MAX_RENDER_WORLDS == MAX_RENDER_APP_WORLDS * 2
		&& distantWorldIndex < MAX_RENDER_WORLDS );
	state.worlds[distantWorldIndex].loaded = qtrue;
	state.worlds[distantWorldIndex].map = &map;
	state.worlds[distantWorldIndex].digest = 77u;
	state.worlds[distantWorldIndex].contentIdentity = 77u;
	state.worlds[distantWorldIndex].snapshot.ready = qtrue;
	state.worlds[distantWorldIndex].snapshot.vertices = vertices;
	state.worlds[distantWorldIndex].snapshot.indices = indices;
	state.worlds[distantWorldIndex].snapshot.batches = batches;
	state.worlds[distantWorldIndex].snapshot.vertexCount = 3u;
	state.worlds[distantWorldIndex].snapshot.indexCount = 9u;
	state.worlds[distantWorldIndex].snapshot.batchCount = 3u;
	for ( uint32_t index = 0u; index < 3u; ++index ) {
		batches[index].sourceSurfaceIndex = 10u + index;
		batches[index].firstIndex = index * 3u;
		batches[index].indexCount = 3u;
		batches[index].shaderIndex = (int32_t)index;
		batches[index].visible = qtrue;
		batches[index].boundsCenter[0] = 10.0f + index * 5.0f;
		batches[index].boundsRadius = 1.0f;
	}
	batches[1].sky = qtrue;
	view.fov_x = view.fov_y = 90.0f;
	view.viewaxis[0][0] = 1.0f;
	view.viewaxis[1][1] = 1.0f;
	view.viewaxis[2][2] = 1.0f;
	skyRequest.schemaVersion = RAL_SKY_ENVIRONMENT_SCHEMA_VERSION;
	skyRequest.backendType = RAL_BACKEND_VULKAN;
	skyRequest.frameGeneration = 9u;
	skyRequest.requestedTier = RAL_ATMOSPHERE_TIER_FULL;
	skyRequest.mode = RAL_SKY_ENVIRONMENT_AUTHORED;
	skyRequest.sourceRevision = 1u;
	skyRequest.authoredSkyAvailable = qtrue;
	skyRequest.distantSceneRequested = qtrue;
	skyRequest.distantSceneIdentity = 77u;
	skyRequest.distantSceneScale = 16.0f;
	skyRequest.distantSceneLodBias = 1.0f;
	skyRequest.capabilities.distantScene = qtrue;
	CHECK( Ral_SkyEnvironmentPlan( &skyRequest, &skyReceipt ) );
	CHECK( RenderSubmission_WorldContentIdentity( &state, distantWorldIndex,
		&contentIdentity ) && contentIdentity == 77u &&
		state.activeWorldIndex == 0 );
	CHECK( RenderSubmission_DistantWorldBuildMetrics( &state, distantWorldIndex, &skyReceipt,
		&view, 4u, metrics, 3u, &metricCount ) && metricCount == 3u &&
		metrics[0].visible && !metrics[1].visible && metrics[2].visible &&
		metrics[0].projectedCoverageQ16 > metrics[2].projectedCoverageQ16 &&
		metrics[0].lodLevelCount == 4u );
	CHECK( RenderSubmission_DistantWorldDrawPlan( &state, distantWorldIndex, &skyReceipt,
		metrics, metricCount, 1u, scratch, 3u, &receipt ) &&
		receipt.drawClass == RAL_SKY_DRAW_CLASS_DISTANT_SCENE &&
		receipt.candidateCount == 3u && receipt.visibleCount == 2u &&
		receipt.admittedCount == 1u && receipt.visibilityCulledCount == 1u &&
		receipt.budgetDroppedCount == 1u && !receipt.collisionQueryCount &&
		!receipt.entityDrawCount && receipt.selections[0].sourceDrawIndex == 0u );
	CHECK( RenderSubmission_DistantWorldSubmissionBuild( &state, distantWorldIndex, &view,
		&receipt, &submission ) &&
		RenderSubmission_DistantWorldSubmissionExact( &submission, &submission ) &&
		submission.world.batches == batches && submission.world.viewOrigin[0] == 0.0f &&
		submission.drawPlan.admittedCount == 1u && state.activeWorldIndex == 0 );
	submissionBefore = submission;
	receipt.selections[0].drawIdentity++;
	CHECK( !RenderSubmission_DistantWorldSubmissionBuild( &state, distantWorldIndex, &view,
		&receipt, &submission ) && !memcmp( &submission, &submissionBefore,
		sizeof( submission ) ) );
	receipt = submissionBefore.drawPlan;
	batches[2].boundsCenter[0] = -20.0f;
	CHECK( RenderSubmission_DistantWorldBuildMetrics( &state, distantWorldIndex, &skyReceipt,
		&view, 4u, metrics, 3u, &metricCount ) && metricCount == 3u &&
		metrics[0].visible && !metrics[1].visible && !metrics[2].visible );
	before = receipt;
	CHECK( !RenderSubmission_DistantWorldDrawPlan( &state, 0, &skyReceipt,
		metrics, 3u, 1u, scratch, 3u, &receipt ) &&
		!memcmp( &receipt, &before, sizeof( receipt ) ) );
	/* Native products consume the sealed frontend frame.  The high-level
	 * bridge derives the paired render-only slot without selecting it and
	 * publishes output only after metrics, admission and submission all agree. */
	state.worldSnapshot.ready = qtrue;
	state.worldSnapshot.fovX = state.worldSnapshot.fovY = 90.0f;
	state.worldSnapshot.viewportWidth = 1280u;
	state.worldSnapshot.viewportHeight = 720u;
	state.worldSnapshot.viewAxis[0][0] = 1.0f;
	state.worldSnapshot.viewAxis[1][1] = 1.0f;
	state.worldSnapshot.viewAxis[2][2] = 1.0f;
	state.sceneRendered = qtrue;
	state.frameOpen = qfalse;
	state.frameSealed = qtrue;
	state.sealedFrameGeneration = 9u;
	state.currentFrameGeneration = 0u;
	CHECK( RenderSubmission_ReadableFrameGeneration( &state,
		&contentIdentity ) && contentIdentity == 9u );
	batches[2].boundsCenter[0] = 20.0f;
	CHECK( RenderSubmission_DistantWorldSubmissionForActiveView( &state,
		&skyReceipt, 4u, 1u, metrics, 3u, scratch, 3u, &submission )
		&& RenderSubmission_DistantWorldSubmissionExact( &submission, &submission )
		&& submission.world.viewportWidth == 1280u
		&& submission.world.viewportHeight == 720u
		&& state.activeWorldIndex == 0 );
	submissionBefore = submission;
	state.sealedFrameGeneration++;
	CHECK( RenderSubmission_ReadableFrameGeneration( &state,
		&contentIdentity ) && contentIdentity == 10u );
	CHECK( !RenderSubmission_DistantWorldSubmissionForActiveView( &state,
		&skyReceipt, 4u, 1u, metrics, 3u, scratch, 3u, &submission )
		&& !memcmp( &submission, &submissionBefore, sizeof( submission ) ) );
	state.sealedFrameGeneration--;
	skyReceipt.distantSceneIdentity++;
	CHECK( !RenderSubmission_DistantWorldSubmissionForActiveView( &state,
		&skyReceipt, 4u, 1u, metrics, 3u, scratch, 3u, &submission )
		&& !memcmp( &submission, &submissionBefore, sizeof( submission ) ) );
	return 0;
}

static int TestMapAuthoredSkyAtmosphere( void )
{
	static renderSubmissionState_t state;
	mapFile_t world = { 0 };
	dsurface_t surface = { 0 };
	drawVert_t vertices[3] = { 0 };
	int indices[3] = { 0, 1, 2 };
	dshader_t shader = { 0 };
	atmosphereFrameState_t atmosphere = { 0 };
	renderAtmosphereSnapshot_t snapshot;
	const atmosphereEmitter_t *emitters;
	ralMaterialSourceSky_t skySource = { 0 }, invalidSource;
	renderMaterialSnapshot_t materialSnapshot;
	renderMaterialSnapshot_t activeSkySnapshot;
	char activeSkyName[MAX_QPATH];
	const byte pixels[4] = { 128u, 64u, 32u, 255u };
	const byte facePixels[4] = { 64u, 96u, 128u, 255u };
	const float stageAverage[3] = { 0.20f, 0.30f, 0.40f };
	float invalidAverage[3] = { 0.20f, NAN, 0.40f };
	qhandle_t outerTextures[RAL_MATERIAL_SKY_FACE_COUNT] = { 0 };
	qhandle_t innerTextures[RAL_MATERIAL_SKY_FACE_COUNT] = { 0 };
	qhandle_t invalidTextures[RAL_MATERIAL_SKY_FACE_COUNT];
	qhandle_t material;

	strcpy( world.name, "maps/authored-sky-fixture.bsp" );
	world.checksum = 0x51534b59;
	world.numSurfaces = 1; world.surfaces = &surface;
	world.numDrawVerts = 3; world.drawVerts = vertices;
	world.numDrawIndexes = 3; world.drawIndexes = indices;
	world.numShaders = 1; world.shaders = &shader;
	strcpy( shader.shader, "textures/skies/tim_hell" );
	shader.surfaceFlags = SURF_SKY;
	surface.surfaceType = MST_PLANAR;
	surface.numVerts = surface.numIndexes = 3;
	surface.lightmapNum = -1;
	vertices[1].xyz[0] = 1.0f;
	vertices[2].xyz[1] = 1.0f;
	CHECK( RenderSubmission_Init( &state, 0x51534b59u ) );
	material = RenderSubmission_RegisterMaterialImage( &state,
		RENDER_ASSET_MATERIAL, shader.shader, qfalse, pixels, 1u, 1u );
	CHECK( material > 0 && RenderSubmission_SetMaterialSky( &state, material,
		qtrue ) );
	skySource.hasSun = qtrue;
	skySource.sunColor[0] = 1.0f;
	skySource.sunColor[1] = skySource.sunColor[2] = 0.77f;
	skySource.sunIntensity = 80.0f;
	skySource.sunAzimuthDegrees = 315.0f;
	skySource.sunElevationDegrees = 70.0f;
	skySource.outerBox = qtrue;
	for ( uint32_t face = 0u; face < RAL_MATERIAL_SKY_FACE_COUNT; ++face ) {
		char name[MAX_QPATH];
		skySource.outerDependencies[face] = face;
		snprintf( name, sizeof( name ), "textures/skies/fixture_%u", face );
		outerTextures[face] = RenderSubmission_RegisterStageTexture( &state,
			RENDER_ASSET_MATERIAL, name, qtrue, facePixels, 1u, 1u );
		CHECK( outerTextures[face] > 0 );
	}
	CHECK( RenderSubmission_SetMaterialAuthoredSkySource( &state, material,
		&skySource ) );
	/* Stage-only skies must publish their real colour through the shared
	 * frontend too, not only through Vulkan's former image-decoding path. */
	{
		const byte white[4] = { 255, 255, 255, 255 };
		ralMaterialSourceSky_t stagedSource = skySource;
		renderMaterialStageSnapshot_t stage = { 0 };
		qhandle_t empty[RAL_MATERIAL_SKY_FACE_COUNT] = { 0 };
		qhandle_t staged = RenderSubmission_RegisterMaterialImage( &state,
			RENDER_ASSET_MATERIAL, "textures/skies/stage-only", qfalse, white, 1, 1 );
		stagedSource.outerBox = qfalse;
		memset( stagedSource.outerDependencies, 0, sizeof( stagedSource.outerDependencies ) );
		stage.material = outerTextures[0];
		stage.sourceBlend = PRIM_BLEND_ONE;
		stage.destinationBlend = PRIM_BLEND_ZERO;
		stage.scaleScroll[0] = stage.scaleScroll[1] = 1.0f;
		for ( unsigned c = 0; c < 4; ++c ) stage.constantColor[c] = 1.0f;
		CHECK( staged > 0 && RenderSubmission_SetMaterialSky( &state, staged, qtrue )
			&& RenderSubmission_SetMaterialStages( &state, staged, &stage, 1u )
			&& RenderSubmission_SetMaterialAuthoredSkySource( &state, staged, &stagedSource )
			&& RenderSubmission_SetMaterialAuthoredSkyTextures( &state, staged, empty, empty )
			&& RenderSubmission_MaterialSnapshot( &state, staged, &materialSnapshot ) );
		for ( unsigned channel = 0; channel < 3; ++channel )
			CHECK( fabsf( materialSnapshot.authoredSkyAverage[channel]
				- (float)facePixels[channel] / 255.0f ) < 0.0001f );
	}
	CHECK( RenderSubmission_MaterialSnapshot( &state, material,
		&materialSnapshot )
		&& fabsf( materialSnapshot.authoredSkyAverage[0] - 0.04f ) < 0.0001f );
	CHECK( RenderSubmission_SetMaterialAuthoredSkyAverage( &state, material,
		stageAverage )
		&& !RenderSubmission_SetMaterialAuthoredSkyAverage( &state, material,
			invalidAverage )
		&& RenderSubmission_MaterialSnapshot( &state, material,
			&materialSnapshot )
		&& fabsf( materialSnapshot.authoredSkyAverage[2] - 0.40f ) < 0.0001f );
	CHECK( RenderSubmission_SetMaterialAuthoredSkyTextures( &state, material,
		outerTextures, innerTextures ) );
	memcpy( invalidTextures, outerTextures, sizeof( invalidTextures ) );
	invalidTextures[2] = 0;
	CHECK( !RenderSubmission_SetMaterialAuthoredSkyTextures( &state, material,
		invalidTextures, innerTextures ) );
	invalidSource = skySource;
	invalidSource.sunIntensity = NAN;
	CHECK( !RenderSubmission_SetMaterialAuthoredSkySource( &state, material,
		&invalidSource ) );
	CHECK( RenderSubmission_MaterialSnapshot( &state, material,
		&materialSnapshot ) && materialSnapshot.authoredSkySourceReady
		&& materialSnapshot.authoredSkyTexturesReady
		&& !memcmp( materialSnapshot.authoredSkyOuterTextures, outerTextures,
			sizeof( outerTextures ) )
		&& fabsf( materialSnapshot.authoredSkyAverage[0]
			- 64.0f / 255.0f ) < 0.0001f );
	CHECK( RenderSubmission_LoadWorld( &state, &world, 0 ) );
	CHECK( RenderSubmission_ActiveSkyMaterial( &state, &activeSkySnapshot,
		activeSkyName ) && !strcmp( activeSkyName, shader.shader )
		&& activeSkySnapshot.handle == material
		&& activeSkySnapshot.digest == materialSnapshot.digest );
	atmosphere.type = ATMOSPHERE_PRECIP_NONE;
	CHECK( RenderSubmission_SetAtmosphere( &state, &atmosphere )
		&& RenderSubmission_AtmosphereSnapshot( &state, &snapshot, &emitters )
		&& emitters != NULL
		&& snapshot.state.skyEnvironment.authoredSkyIdentity
			== materialSnapshot.digest
		&& snapshot.state.skyEnvironment.sourceRevision != 0u
		&& snapshot.state.skyEnvironment.skyLuminance == 128.0f / 255.0f
		&& fabsf( snapshot.state.skyEnvironment.sunDirection[0] - 0.2418448f )
			< 0.0001f
		&& fabsf( snapshot.state.skyEnvironment.sunDirection[1] + 0.2418448f )
			< 0.0001f
		&& fabsf( snapshot.state.skyEnvironment.sunDirection[2] - 0.9396926f )
			< 0.0001f
		&& snapshot.state.skyEnvironment.sunRadiance[0]
			> snapshot.state.skyEnvironment.sunRadiance[1] );
	RenderSubmission_Reset( &state );
	return 0;
}

static int TestMaterialStageDepthCollapse( void ) {
	ralMaterialStageDepth_t stages[RAL_MATERIAL_SOURCE_MAX_STAGES] = { 0 };
	ralMaterialStageDepthPlan_t plan, before;
	/* Exhaust the four function/write combinations for every program length.
	 * Independent depth-buffer simulation checks every possible input relation. */
	for ( unsigned count = 1u; count <= RAL_MATERIAL_SOURCE_MAX_STAGES; ++count ) {
		const unsigned programs = 1u << ( count * 2u );
		for ( unsigned program = 0u; program < programs; ++program ) {
			for ( unsigned i = 0u; i < count; ++i ) {
				stages[i].depthFunc = ( program >> ( 2u * i ) ) & 1u;
				stages[i].depthWrite = (qboolean)(( program >> ( 2u * i + 1u ) ) & 1u);
				stages[i].alphaTest = RAL_MATERIAL_ALPHA_TEST_NONE;
			}
			CHECK( Ral_MaterialStageDepthCollapse( stages, count, &plan ) );
			if ( plan.singleDraw ) for ( unsigned fragment = 0u; fragment < 3u; ++fragment ) {
				unsigned depth = 1u, mask = 0u;
				for ( unsigned i = 0u; i < count; ++i ) {
					const qboolean pass = stages[i].depthFunc == RAL_MATERIAL_DEPTH_EQUAL
						? fragment == depth : fragment <= depth;
					if ( pass ) { mask |= 1u << i; if ( stages[i].depthWrite ) depth = fragment; }
				}
				const qboolean pass = plan.depthFunc == RAL_MATERIAL_DEPTH_EQUAL
					? fragment == 1u : fragment <= 1u;
				CHECK( mask == ( pass ? ( 1u << count ) - 1u : 0u ) );
				CHECK( depth == ( pass && plan.depthWrite ? fragment : 1u ) );
			}
		}
	}
	/* Common Q3 base-write -> equal-lightmap stays one depth operation. */
	memset( stages, 0, sizeof( stages ) ); stages[0].depthWrite = qtrue;
	stages[1].depthFunc = RAL_MATERIAL_DEPTH_EQUAL;
	CHECK( Ral_MaterialStageDepthCollapse( stages, 2u, &plan )
		&& plan.singleDraw && plan.depthWrite && plan.depthFunc == RAL_MATERIAL_DEPTH_LEQUAL );
	stages[0].depthWrite = qfalse;
	CHECK( Ral_MaterialStageDepthCollapse( stages, 2u, &plan ) && !plan.singleDraw );
	stages[0].depthFunc = RAL_MATERIAL_DEPTH_EQUAL;
	stages[1].depthFunc = RAL_MATERIAL_DEPTH_LEQUAL;
	CHECK( Ral_MaterialStageDepthCollapse( stages, 2u, &plan ) && !plan.singleDraw );
	for ( unsigned slot = 0u; slot < RAL_MATERIAL_SOURCE_MAX_STAGES; ++slot ) {
		memset( stages, 0, sizeof( stages ) );
		stages[slot].alphaTest = RAL_MATERIAL_ALPHA_TEST_KEEP_LESS;
		CHECK( Ral_MaterialStageDepthCollapse( stages, RAL_MATERIAL_SOURCE_MAX_STAGES, &plan )
			&& !plan.singleDraw );
	}
	memset( stages, 0, sizeof( stages ) ); stages[0].alphaTest = RAL_MATERIAL_ALPHA_TEST_KEEP_LESS;
	CHECK( Ral_MaterialStageDepthCollapse( stages, 1u, &plan ) && plan.singleDraw );
	before = plan;
	CHECK( !Ral_MaterialStageDepthCollapse( NULL, 1u, &plan ) && !memcmp( &plan, &before, sizeof( plan ) ) );
	CHECK( !Ral_MaterialStageDepthCollapse( stages, 0u, &plan ) && !memcmp( &plan, &before, sizeof( plan ) ) );
	CHECK( !Ral_MaterialStageDepthCollapse( stages, RAL_MATERIAL_SOURCE_MAX_STAGES + 1u, &plan )
		&& !memcmp( &plan, &before, sizeof( plan ) ) );
	stages[7].depthFunc = UINT32_MAX;
	CHECK( !Ral_MaterialStageDepthCollapse( stages, 8u, &plan ) && !memcmp( &plan, &before, sizeof( plan ) ) );
	stages[7].depthFunc = 0u; stages[7].depthWrite = (qboolean)2;
	CHECK( !Ral_MaterialStageDepthCollapse( stages, 8u, &plan ) && !memcmp( &plan, &before, sizeof( plan ) ) );
	stages[7].depthWrite = qfalse; stages[7].alphaTest = UINT32_MAX;
	CHECK( !Ral_MaterialStageDepthCollapse( stages, 8u, &plan ) && !memcmp( &plan, &before, sizeof( plan ) ) );
	return 0;
}

static int TestMaterialStageBlendAdditive( void ) {
	ralMaterialStageBlend_t stages[8] = { 0 };
	qboolean additive;
	/* Every factor pair in every slot: don't classify a chain from stage zero.
	 * Also pins the public source-IR encoding used by the shared classifier. */
	CHECK( RENDER_MATERIAL_BLEND_ONE == 1 && RENDER_MATERIAL_BLEND_SRC_ALPHA == 6
		&& RENDER_MATERIAL_BLEND_SRC_ALPHA_SATURATE == 10 );
	for ( unsigned slot = 0u; slot < 8u; ++slot )
	for ( unsigned src = 0u; src <= 10u; ++src )
	for ( unsigned dst = 0u; dst <= 10u; ++dst ) {
		for ( unsigned i = 0u; i < 8u; ++i ) {
			stages[i].sourceBlend = RENDER_MATERIAL_BLEND_SRC_ALPHA;
			stages[i].destinationBlend = RENDER_MATERIAL_BLEND_ONE;
		}
		stages[slot].sourceBlend = src; stages[slot].destinationBlend = dst;
		CHECK( Ral_MaterialStageBlendAdditive( stages, 8u, &additive ) );
		CHECK( !!additive == ( dst == RENDER_MATERIAL_BLEND_ONE
			&& ( src == RENDER_MATERIAL_BLEND_ONE || src == RENDER_MATERIAL_BLEND_SRC_ALPHA ) ) );
	}
	/* All 510 eligible factor programs, against three colored backgrounds and
	 * nonuniform stage color/coverage. Independent ordered recurrence vs sum. */
	for ( unsigned count = 1u; count <= 8u; ++count )
	for ( unsigned bits = 0u; bits < (1u << count); ++bits ) {
		for ( unsigned i = 0u; i < count; ++i ) {
			stages[i].sourceBlend = bits & (1u << i)
				? RENDER_MATERIAL_BLEND_ONE : RENDER_MATERIAL_BLEND_SRC_ALPHA;
			stages[i].destinationBlend = RENDER_MATERIAL_BLEND_ONE;
		}
		CHECK( Ral_MaterialStageBlendAdditive( stages, count, &additive ) && additive );
		for ( unsigned bg = 0u; bg < 3u; ++bg ) for ( unsigned c = 0u; c < 3u; ++c ) {
			double background = (bg*3+c+1)/17.0, ordered = background, sum = 0.0;
			for ( unsigned i = 0u; i < count; ++i ) {
				double color = (i+c+1)/31.0, alpha = ((i+bg)%4)/3.0;
				double contribution = bits & (1u << i) ? color : color*alpha;
				ordered = contribution + ordered; sum += contribution;
			}
			CHECK( fabs( ordered - (background+sum) ) < 1e-12 );
		}
	}
	for ( unsigned invalid = 0u; invalid < 6u; ++invalid ) {
		qboolean sentinel = (qboolean)73;
		stages[7].sourceBlend = invalid == 0u ? UINT32_MAX : 1u;
		stages[7].destinationBlend = invalid == 1u ? UINT32_MAX : 1u;
		CHECK( !Ral_MaterialStageBlendAdditive( invalid == 2u ? NULL : stages,
			invalid == 3u ? 0u : (invalid == 4u ? 9u : 8u),
			invalid == 5u ? NULL : &sentinel ) );
		CHECK( sentinel == (qboolean)73 );
	}
	return 0;
}

static int TestMaterialStageSnapshot( void )
{
	static renderSubmissionState_t state;
	const byte pixels[4] = { 255u, 255u, 255u, 255u };
	renderMaterialStageSnapshot_t stage = { 0 }, invalid;
	renderMaterialSnapshot_t snapshot, before;
	qhandle_t material, frame0, frame1;

	CHECK( RenderSubmission_Init( &state, 0x414e494du ) );
	material = RenderSubmission_RegisterMaterialImage( &state,
		RENDER_ASSET_MATERIAL, "textures/test/animated", qfalse,
		pixels, 1u, 1u );
	frame0 = RenderSubmission_RegisterStageTexture( &state,
		RENDER_ASSET_MATERIAL, "textures/test/frame0", qfalse,
		pixels, 1u, 1u );
	frame1 = RenderSubmission_RegisterStageTexture( &state,
		RENDER_ASSET_MATERIAL, "textures/test/frame1", qfalse,
		pixels, 1u, 1u );
	CHECK( material > 0 && frame0 > 0 && frame1 > 0
		&& state.materialCount == 1u && state.stageTextureCount == 2u );
	stage.material = frame0;
	stage.animationMaterials[0] = frame0;
	stage.animationMaterials[1] = frame1;
	stage.animationFrameCount = 2u;
	stage.animationFrequency = 8.0f;
	stage.animationLooping = qtrue;
	stage.tcGen = RENDER_MATERIAL_TCGEN_VECTOR;
	stage.tcGenVectors[0][0] = 0.5f;
	stage.tcGenVectors[1][1] = 0.25f;
	stage.rgbGen = RAL_MATERIAL_COLOR_WAVE;
	stage.alphaGen = RAL_MATERIAL_ALPHA_WAVE;
	stage.rgbWave.function = RAL_MATERIAL_WAVE_SIN;
	stage.rgbWave.base = 0.5f;
	stage.rgbWave.amplitude = 0.5f;
	stage.rgbWave.frequency = 2.0f;
	stage.alphaWave.function = RAL_MATERIAL_WAVE_TRIANGLE;
	stage.alphaWave.base = 0.75f;
	stage.alphaWave.amplitude = 0.25f;
	stage.alphaWave.frequency = 1.0f;
	stage.constantColor[0] = stage.constantColor[1]
		= stage.constantColor[2] = stage.constantColor[3] = 1.0f;
	stage.scaleScroll[0] = stage.scaleScroll[1] = 1.0f;
	CHECK( RenderSubmission_SetMaterialStages( &state, material, &stage, 1u )
		&& RenderSubmission_MaterialSnapshot( &state, material, &snapshot )
		&& snapshot.stageCount == 1u
		&& snapshot.stages[0].animationFrameCount == 2u
		&& snapshot.stages[0].animationMaterials[1] == frame1
		&& snapshot.stages[0].animationFrequency == 8.0f
		&& snapshot.stages[0].tcGenVectors[0][0] == 0.5f
		&& snapshot.stages[0].rgbWave.frequency == 2.0f
		&& snapshot.stages[0].alphaWave.function
			== RAL_MATERIAL_WAVE_TRIANGLE );
	before = snapshot;
	invalid = stage;
	invalid.animationMaterials[1] = 0;
	CHECK( !RenderSubmission_SetMaterialStages( &state, material, &invalid, 1u )
		&& RenderSubmission_MaterialSnapshot( &state, material, &snapshot )
		&& !memcmp( &snapshot, &before, sizeof( snapshot ) ) );
	/* Both depth operands participate in identity. Rejection must not publish
	 * a new generation or partially replace the existing stage program. */
	for ( unsigned operand = 0u; operand < 2u; ++operand ) {
		if ( operand == 0u ) stage.depthWrite = qtrue;
		else stage.depthFunc = RAL_MATERIAL_DEPTH_EQUAL;
		CHECK( RenderSubmission_SetMaterialStages( &state, material, &stage, 1u )
			&& RenderSubmission_MaterialSnapshot( &state, material, &snapshot )
			&& snapshot.digest != before.digest
			&& snapshot.generation != before.generation
			&& snapshot.stages[0].depthWrite == stage.depthWrite
			&& snapshot.stages[0].depthFunc == stage.depthFunc
			&& snapshot.stageDepth.singleDraw
			&& snapshot.stageDepth.depthWrite == stage.depthWrite
			&& snapshot.stageDepth.depthFunc == stage.depthFunc );
		before = snapshot;
		CHECK( RenderSubmission_SetMaterialStages( &state, material, &stage, 1u )
			&& RenderSubmission_MaterialSnapshot( &state, material, &snapshot )
			&& !memcmp( &snapshot, &before, sizeof( snapshot ) ) );
		invalid = stage;
		if ( operand == 0u ) invalid.depthWrite = (qboolean)2;
		else invalid.depthFunc = UINT32_MAX;
		CHECK( !RenderSubmission_SetMaterialStages( &state, material, &invalid, 1u )
			&& RenderSubmission_MaterialSnapshot( &state, material, &snapshot )
			&& !memcmp( &snapshot, &before, sizeof( snapshot ) ) );
	}
	{
		renderMaterialStageSnapshot_t pair[2] = { stage, stage };
		pair[0].depthFunc = RAL_MATERIAL_DEPTH_LEQUAL;
		pair[1].depthFunc = RAL_MATERIAL_DEPTH_EQUAL;
		CHECK( RenderSubmission_SetMaterialStages( &state, material, pair, 2u )
			&& RenderSubmission_MaterialSnapshot( &state, material, &snapshot )
			&& snapshot.stageDepth.singleDraw && snapshot.stageDepth.depthWrite
			&& snapshot.stageDepth.depthFunc == RAL_MATERIAL_DEPTH_LEQUAL );
		before = snapshot;
		pair[0].depthWrite = qfalse;
		CHECK( RenderSubmission_SetMaterialStages( &state, material, pair, 2u )
			&& RenderSubmission_MaterialSnapshot( &state, material, &snapshot )
			&& !snapshot.stageDepth.singleDraw && snapshot.digest != before.digest );
		before = snapshot;
	}
	{
		renderMaterialStageSnapshot_t pair[2] = { stage, stage };
		pair[0].sourceBlend = RENDER_MATERIAL_BLEND_SRC_ALPHA;
		pair[1].sourceBlend = RENDER_MATERIAL_BLEND_ONE;
		pair[0].destinationBlend = pair[1].destinationBlend = RENDER_MATERIAL_BLEND_ONE;
		CHECK( RenderSubmission_SetMaterialStages( &state, material, pair, 2u )
			&& RenderSubmission_MaterialSnapshot( &state, material, &snapshot )
			&& snapshot.stageBlendAdditive );
		before = snapshot;
		CHECK( RenderSubmission_SetMaterialStages( &state, material, pair, 2u )
			&& RenderSubmission_MaterialSnapshot( &state, material, &snapshot )
			&& !memcmp( &before, &snapshot, sizeof( snapshot ) ) );
		pair[1].destinationBlend = 11u;
		CHECK( !RenderSubmission_SetMaterialStages( &state, material, pair, 2u )
			&& RenderSubmission_MaterialSnapshot( &state, material, &snapshot )
			&& !memcmp( &before, &snapshot, sizeof( snapshot ) ) );
		pair[1].destinationBlend = RENDER_MATERIAL_BLEND_ONE_MINUS_SRC_ALPHA;
		CHECK( RenderSubmission_SetMaterialStages( &state, material, pair, 2u )
			&& RenderSubmission_MaterialSnapshot( &state, material, &snapshot )
			&& !snapshot.stageBlendAdditive && before.digest != snapshot.digest
			&& before.generation != snapshot.generation );
		before = snapshot;
	}
	stage.tcModCount = 2u;
	stage.tcMods[0].type = RAL_MATERIAL_TCMOD_SCROLL;
	stage.tcMods[0].parameters[0] = 0.25f;
	stage.tcMods[1].type = RAL_MATERIAL_TCMOD_SCALE;
	stage.tcMods[1].parameters[0] = stage.tcMods[1].parameters[1] = 2.0f;
	CHECK( RenderSubmission_SetMaterialStages( &state, material, &stage, 1u )
		&& RenderSubmission_MaterialSnapshot( &state, material, &snapshot )
		&& snapshot.digest != before.digest && snapshot.stages[0].tcModCount == 2u );
	before = snapshot;
	{ renderMaterialTcMod_t swap = stage.tcMods[0];
		stage.tcMods[0] = stage.tcMods[1]; stage.tcMods[1] = swap; }
	CHECK( RenderSubmission_SetMaterialStages( &state, material, &stage, 1u )
		&& RenderSubmission_MaterialSnapshot( &state, material, &snapshot )
		&& snapshot.digest != before.digest );
	before = snapshot;
	for ( unsigned defect = 0u; defect < 4u; ++defect ) {
		invalid = stage;
		if ( defect == 0u ) invalid.tcModCount = RAL_MATERIAL_SOURCE_MAX_TCMODS + 1u;
		if ( defect == 1u ) invalid.tcMods[0].type = 99u;
		if ( defect == 2u ) invalid.tcMods[0].parameters[0] = NAN;
		if ( defect == 3u ) {
			invalid.tcMods[0].type = RAL_MATERIAL_TCMOD_STRETCH;
			invalid.tcMods[0].parameters[0] = 1.5f;
		}
		CHECK( !RenderSubmission_SetMaterialStages( &state, material, &invalid, 1u )
			&& RenderSubmission_MaterialSnapshot( &state, material, &snapshot )
			&& !memcmp( &snapshot, &before, sizeof( snapshot ) ) );
	}
	/* An authored zero scale is a valid constant texture coordinate, not an
	 * absent flattened compatibility transform. */
	stage.scaleScroll[0] = 0.0f;
	stage.tcMods[0].parameters[0] = 0.0f;
	CHECK( RenderSubmission_SetMaterialStages( &state, material, &stage, 1u ) );
	RenderSubmission_Reset( &state );
	return 0;
}

static int TestStageTextureMaterialCapacityIsolation( void )
{
	static renderSubmissionState_t state;
	static renderMaterialFactorPublicationEntry_t factors[RENDER_SUBMISSION_MAX_MATERIALS];
	const byte pixels[4] = { 255u, 255u, 255u, 255u };
	char name[MAX_QPATH];
	renderTextureSnapshot_t texture;
	qhandle_t lastMaterial = 0, lastTexture = 0, gapHandle = 0;
	uint32_t index = UINT32_MAX;

	CHECK( RenderSubmission_Init( &state, 0x53544745u ) );
	CHECK( !RenderSubmission_MaterialIndex( &state, 1, &index )
		&& index == UINT32_MAX );
	for ( uint32_t i = 0u; i < 256u; ++i ) {
		snprintf( name, sizeof( name ), "textures/test/stage_%u", i );
		lastTexture = RenderSubmission_RegisterStageTexture( &state,
			RENDER_ASSET_MATERIAL, name, qfalse, pixels, 1u, 1u );
		CHECK( lastTexture > 0 );
	}
	for ( uint32_t i = 0u; i < RENDER_SUBMISSION_MAX_MATERIALS; ++i ) {
		snprintf( name, sizeof( name ), "textures/test/material_%u", i );
		lastMaterial = RenderSubmission_RegisterMaterialImage( &state,
			RENDER_ASSET_MATERIAL, name, qfalse, pixels, 1u, 1u );
		CHECK( lastMaterial > 0 && RenderSubmission_MaterialIndex( &state,
			lastMaterial, &index ) && index == i );
		/* Interleaved model handles must not be treated as array indices. */
		gapHandle = RenderSubmission_RegisterAsset( &state, RENDER_ASSET_MODEL,
			"models/test/gap.md3" );
		index = UINT32_MAX;
		CHECK( gapHandle > lastMaterial
			&& !RenderSubmission_MaterialIndex( &state, gapHandle, &index )
			&& index == UINT32_MAX );
		memset( &factors[i], 0, sizeof( factors[i] ) );
		factors[i].material = lastMaterial;
		factors[i].factors.baseColor[3] = 1.0f;
		factors[i].factors.roughness = (float)i / RENDER_SUBMISSION_MAX_MATERIALS;
	}
	CHECK( state.materialCount == RENDER_SUBMISSION_MAX_MATERIALS
		&& state.stageTextureCount == 256u
		&& RenderSubmission_TextureSnapshot( &state, lastTexture, &texture )
		&& texture.ready == qtrue
		&& RenderSubmission_MaterialHandle( &state,
			"textures/test/material_1023" ) == lastMaterial );
	/* Full-capacity arbitrary publication order, duplicate/unknown rejection,
	 * unchanged-generation identity and failure atomicity. */
	for ( uint32_t i = 0u; i < RENDER_SUBMISSION_MAX_MATERIALS / 2u; ++i ) {
		const uint32_t other = RENDER_SUBMISSION_MAX_MATERIALS - 1u - i;
		renderMaterialFactorPublicationEntry_t swap = factors[i];
		factors[i] = factors[other]; factors[other] = swap;
	}
	CHECK( RenderSubmission_PublishMaterialFactors( &state, 73u, factors,
		RENDER_SUBMISSION_MAX_MATERIALS ) );
	for ( uint32_t i = 0u; i < state.materialCount; ++i ) {
		CHECK( RenderSubmission_MaterialIndex( &state,
			state.materials[i].snapshot.handle, &index ) && index == i );
		CHECK( state.materials[i].snapshot.factors.ready
			&& state.materials[i].snapshot.factors.factors.roughness
				== (float)i / RENDER_SUBMISSION_MAX_MATERIALS );
	}
	{
		const uint64_t generation = state.nextMaterialGeneration;
		const uint64_t digest = state.materialDigest;
		const qhandle_t saved = factors[0].material;
		CHECK( RenderSubmission_PublishMaterialFactors( &state, 73u, factors,
			RENDER_SUBMISSION_MAX_MATERIALS ) );
		CHECK( state.nextMaterialGeneration == generation && state.materialDigest == digest );
		for ( unsigned defect = 0u; defect < 3u; ++defect ) {
			factors[0].material = defect == 0u ? factors[1].material
				: defect == 1u ? gapHandle : lastTexture;
			CHECK( !RenderSubmission_PublishMaterialFactors( &state, 74u, factors,
				RENDER_SUBMISSION_MAX_MATERIALS ) );
			CHECK( state.nextMaterialGeneration == generation && state.materialDigest == digest );
		}
		factors[0].material = saved;
		CHECK( RenderSubmission_PublishMaterialFactors( &state, 74u, factors,
			RENDER_SUBMISSION_MAX_MATERIALS ) );
		CHECK( state.nextMaterialGeneration == generation + state.materialCount );
	}
	CHECK( !RenderSubmission_MaterialIndex( NULL, lastMaterial, &index )
		&& !RenderSubmission_MaterialIndex( &state, lastMaterial, NULL )
		&& !RenderSubmission_MaterialIndex( &state, 0, &index ) );
	CHECK( RenderSubmission_ResetRegistrationAssets( &state ) );
	index = UINT32_MAX;
	CHECK( !RenderSubmission_MaterialIndex( &state, lastMaterial, &index )
		&& index == UINT32_MAX );
	CHECK( RenderSubmission_RegisterMaterialImage( &state,
		RENDER_ASSET_MATERIAL, "textures/test/reload", qfalse, pixels, 1u, 1u ) > lastMaterial );
	CHECK( RenderSubmission_MaterialIndex( &state, state.materials[0].snapshot.handle,
		&index ) && index == 0u );
	RenderSubmission_Reset( &state );
	return 0;
}

static int TestStageTextureDoesNotAliasShaderIdentity( void )
{
	static renderSubmissionState_t state;
	const byte primary[4] = { 255u, 0u, 255u, 255u };
	const byte body[4] = { 64u, 96u, 128u, 255u };
	const byte proxyPixel[4] = { 255u, 255u, 255u, 255u };
	renderTextureSnapshot_t texture;
	ralTextureCacheKey_t residentKey, replacementKey;
	qhandle_t material, stage, duplicate, proxy;

	CHECK( RenderSubmission_Init( &state, 0x504c534du ) );
	material = RenderSubmission_RegisterMaterialImage( &state,
		RENDER_ASSET_MATERIAL, "models/test/body", qfalse,
		primary, 1u, 1u );
	stage = RenderSubmission_RegisterStageTexture( &state,
		RENDER_ASSET_MATERIAL, "models/test/body", qfalse,
		body, 1u, 1u );
	duplicate = RenderSubmission_RegisterStageTexture( &state,
		RENDER_ASSET_MATERIAL, "models/test/body", qfalse,
		body, 1u, 1u );
	CHECK( material > 0 && stage > 0 && stage != material
		&& duplicate == stage && state.materialCount == 1u
		&& state.stageTextureCount == 1u
		&& RenderSubmission_TextureSnapshot( &state, stage, &texture )
		&& texture.rgba8 && !memcmp( texture.rgba8, body, sizeof( body ) ) );
	/* Q3 map/clampmap may use one image with different sampler addressing
	 * across stages/maps. Both immutable bindings must coexist and deduplicate. */
	qhandle_t clamped = RenderSubmission_RegisterStageTexture( &state,
		RENDER_ASSET_MATERIAL, "models/test/body", qtrue, body, 1u, 1u );
	CHECK( clamped > 0 && clamped != stage
		&& state.stageTextureCount == 2u
		&& RenderSubmission_RegisterStageTexture( &state, RENDER_ASSET_MATERIAL,
			"models/test/body", qtrue, body, 1u, 1u ) == clamped
		&& RenderSubmission_RegisterStageTexture( &state, RENDER_ASSET_MATERIAL,
			"models/test/body", qfalse, body, 1u, 1u ) == stage
		&& RenderSubmission_TextureSnapshot( &state, clamped, &texture )
		&& texture.clampToEdge == qtrue
		&& RenderSubmission_TextureSnapshot( &state, stage, &texture )
		&& texture.clampToEdge == qfalse );
	CHECK( !RenderSubmission_RegisterStageTexture( &state, RENDER_ASSET_MATERIAL,
		"models/test/body", qtrue, primary, 1u, 1u ) );
	proxy = RenderSubmission_RegisterMaterialImage( &state,
		RENDER_ASSET_MATERIAL, "textures/native-resident-proxy", qtrue,
		proxyPixel, 1u, 1u );
	CHECK( proxy > 0
		&& BuildTestTextureKey( 0x303u, 128u, 64u, &residentKey )
		&& RenderSubmission_SetMaterialTextureCacheKey( &state, proxy,
			&residentKey )
		&& BuildTestTextureKey( 0x404u, 256u, 128u, &replacementKey )
		&& !RenderSubmission_SetMaterialTextureCacheKey( &state, proxy,
			&replacementKey ) );
	RenderSubmission_Reset( &state );
	return 0;
}

static int TestLensSourceSubmission( void )
{
	static renderSubmissionState_t state;
	static renderSubmissionState_t directState;
	renderLensSourceSnapshot_t snapshot;
	renderLensSourceSnapshot_t directSnapshot;
	renderSubmissionReceipt_t receipt;
	const lensSourceDesc_t *sources = NULL;
	lensSourceDesc_t source;
	memset( &source, 0, sizeof( source ) );
	source.id = LENS_BAND_MAPFLARE + 7;
	source.origin[0] = 64.0f;
	source.origin[1] = -32.0f;
	source.origin[2] = 96.0f;
	source.radius = 12.0f;
	source.rgba[0] = source.rgba[1] = source.rgba[2] = source.rgba[3] = 1.0f;
	CHECK( RenderSubmission_Init( &state, 0x4c454e53u ) );
	CHECK( RenderSubmission_BeginFrame( &state, 1u ) );
	CHECK( RenderSubmission_AddLensSource( &state, &source ) );
	source.radius = 18.0f;
	CHECK( RenderSubmission_AddLensSource( &state, &source ) );
	CHECK( RenderSubmission_LensSourceSnapshot( &state, &snapshot, &sources )
		&& snapshot.count == 1u && snapshot.droppedCount == 0u
		&& sources && sources[0].id == source.id
		&& sources[0].radius == 18.0f );
	CHECK( RenderSubmission_Init( &directState, 0x4c454e53u ) );
	CHECK( RenderSubmission_BeginFrame( &directState, 1u ) );
	CHECK( RenderSubmission_AddLensSource( &directState, &source ) );
	CHECK( RenderSubmission_LensSourceSnapshot( &directState, &directSnapshot,
		&sources ) && directSnapshot.digest == snapshot.digest );
	source.id = LENS_MAX_CGSOURCES;
	CHECK( !RenderSubmission_AddLensSource( &state, &source ) );
	CHECK( RenderSubmission_EndFrame( &state, 1u, &receipt )
		&& receipt.lensSourceCount == 1u
		&& receipt.lensSourceDroppedCount == 0u );
	RenderSubmission_Reset( &state );
	RenderSubmission_Reset( &directState );
	return 0;
}

static int TestEffectParticlePlan( void )
{
	static renderSubmissionState_t state;
	static ralParticleClassGpu_t classes[MAX_PARTICLE_CLASSES];
	static ralParticleSpawnGpu_t spawns[RAL_PARTICLE_SPAWN_REQUEST_MAX];
	static ralParticleClassGpu_t exactClasses[MAX_PARTICLE_CLASSES];
	static ralParticleSpawnGpu_t exactSpawns[RAL_PARTICLE_SPAWN_REQUEST_MAX];
	const byte pixel[4] = { 255u, 128u, 32u, 255u };
	particleClass_t particleClass;
	emitterDesc_t emitter;
	renderEffectParticlePlanState_t current, candidate, exactCandidate;
	renderEffectParticlePlanReceipt_t receipt, exactReceipt;
	qhandle_t material;
	CHECK( RenderSubmission_Init( &state, 0x50415254u ) );
	material = RenderSubmission_RegisterMaterialImage( &state,
		RENDER_ASSET_PRIMITIVE_MATERIAL, "gfx/fx/particle", qtrue,
		pixel, 1u, 1u );
	CHECK( material > 0 && RenderSubmission_SetMaterialRasterPolicy( &state,
		material, RENDER_ALPHA_ALPHA_ADDITIVE, 0.0f, qfalse ) );
	memset( &particleClass, 0, sizeof( particleClass ) );
	particleClass.shader = material;
	particleClass.emitMode = EMIT_POINT;
	particleClass.scatterShape = SCATTER_SPHERE;
	particleClass.velocityShape = VEL_AXIAL_PLUS_CUBE;
	particleClass.scatterMagnitude = 2.0f;
	particleClass.axialSpeed = 16.0f;
	particleClass.cubeJitter = 4.0f;
	particleClass.lifetimeMean = 1.0f;
	particleClass.colorPalette[0][0] = 1.0f;
	particleClass.colorPalette[0][1] = 0.5f;
	particleClass.colorPalette[0][3] = 1.0f;
	particleClass.paletteCount = 1;
	particleClass.colorEndMult[0] = 1.0f;
	particleClass.colorEndMult[1] = 1.0f;
	particleClass.colorEndMult[2] = 1.0f;
	particleClass.sizeStart = 1.0f;
	particleClass.sizeEnd = 2.0f;
	CHECK( RenderSubmission_RegisterParticleClass( &state, 1, &particleClass ) );
	CHECK( RenderSubmission_BeginFrame( &state, 1u ) );
	memset( &emitter, 0, sizeof( emitter ) );
	emitter.cls = 1;
	emitter.count = 8192;
	emitter.axis[0] = 1.0f;
	emitter.end[0] = 64.0f;
	emitter.colorTint[0] = emitter.colorTint[1]
		= emitter.colorTint[2] = emitter.colorTint[3] = 1.0f;
	CHECK( RenderSubmission_AddEffectEmitter( &state, &emitter ) );
	CHECK( RenderSubmission_AddEffectEmitter( &state, &emitter ) );
	emitter.count = 1;
	CHECK( RenderSubmission_AddEffectEmitter( &state, &emitter ) );
	RenderEffectParticlePlan_Reset( &current );
	CHECK( RenderEffectParticlePlan_Build( &state, &current, 7u, qtrue,
		2.0f, classes, ARRAY_LEN( classes ), spawns, ARRAY_LEN( spawns ),
		&candidate, &receipt ) );
	CHECK( receipt.submittedEmitterCount == 3u
		&& receipt.admittedEmitterCount == 2u
		&& receipt.droppedEmitterCount == 1u
		&& receipt.requestedParticleCount == 16385u
		&& receipt.admittedParticleCount == RAL_PARTICLE_POOL_CAPACITY
		&& receipt.droppedParticleCount == 1u
		&& receipt.spawnRequestCount == 2u && receipt.classCount == 1u
		&& receipt.nextSlot == 0u && receipt.nextSeed == 2u
		&& classes[0].shader == (uint32_t)material
		&& classes[0].shaderBlendIsAdditive == 1u
		&& spawns[0].firstSlot == 0u && spawns[0].seed == 1u
		&& spawns[1].firstSlot == 8192u && spawns[1].seed == 2u
		&& RenderEffectParticlePlan_ReceiptExact( &candidate, classes,
			spawns, &receipt ) );
	exactCandidate = candidate;
	exactReceipt = receipt;
	memcpy( exactClasses, classes, sizeof( classes ) );
	memcpy( exactSpawns, spawns, sizeof( spawns ) );
	CHECK( !RenderEffectParticlePlan_Build( &state, &current, 8u, qtrue,
		3.0f, classes, MAX_PARTICLE_CLASSES - 1u, spawns,
		ARRAY_LEN( spawns ), &candidate, &receipt )
		&& !memcmp( &candidate, &exactCandidate, sizeof( candidate ) )
		&& !memcmp( &receipt, &exactReceipt, sizeof( receipt ) )
		&& !memcmp( classes, exactClasses, sizeof( classes ) )
		&& !memcmp( spawns, exactSpawns, sizeof( spawns ) ) );
	receipt = exactReceipt;
	receipt.admittedParticleCount--;
	CHECK( !RenderEffectParticlePlan_ReceiptExact( &exactCandidate,
		exactClasses, exactSpawns, &receipt ) );
	/* An authored animation is indivisible: an absent frame must not become
	 * the base texture, a white sampler, or a static particle. Rejection must
	 * also leave the caller's GPU upload/publication payload untouched. */
	particleClass.frameCount = 3;
	particleClass.frameBlend = 1;
	for ( int frame = 0; frame < particleClass.frameCount; ++frame )
		particleClass.frameShaders[frame] = material;
	for ( int missingFrame = 0; missingFrame <= particleClass.frameCount;
			++missingFrame ) {
		/* Classes are immutable within a scope. Each malformed declaration
		 * needs its own scope; the final scope contains the complete asset. */
		RenderSubmission_Reset( &state );
		CHECK( RenderSubmission_Init( &state, 0x50415255u + missingFrame ) );
		material = RenderSubmission_RegisterMaterialImage( &state,
			RENDER_ASSET_PRIMITIVE_MATERIAL, "gfx/fx/particle", qtrue,
			pixel, 1u, 1u );
		CHECK( material > 0 );
		particleClass.shader = material;
		for ( int frame = 0; frame < particleClass.frameCount; ++frame )
			particleClass.frameShaders[frame] = frame == missingFrame ? 0 : material;
		CHECK( RenderSubmission_RegisterParticleClass( &state, 1, &particleClass ) );
		CHECK( RenderSubmission_BeginFrame( &state, 1u ) );
		CHECK( RenderSubmission_AddEffectEmitter( &state, &emitter ) );
		if ( missingFrame == particleClass.frameCount ) break;
		receipt = exactReceipt;
		CHECK( !RenderEffectParticlePlan_Build( &state, &current, 8u, qtrue,
			3.0f, classes, ARRAY_LEN( classes ), spawns, ARRAY_LEN( spawns ),
			&candidate, &receipt ) );
		CHECK( !memcmp( &candidate, &exactCandidate, sizeof( candidate ) )
			&& !memcmp( &receipt, &exactReceipt, sizeof( receipt ) )
			&& !memcmp( classes, exactClasses, sizeof( classes ) )
			&& !memcmp( spawns, exactSpawns, sizeof( spawns ) ) );
	}
	CHECK( RenderEffectParticlePlan_Build( &state, &current, 8u, qtrue,
		3.0f, classes, ARRAY_LEN( classes ), spawns, ARRAY_LEN( spawns ),
		&candidate, &receipt ) );
	CHECK( classes[0].frameCount == 3u && classes[0].frameBlend == 1u
		&& classes[0].frameSlots[0] == MAX_PARTICLE_CLASSES
		&& classes[0].frameSlots[1] == MAX_PARTICLE_CLASSES + 1u
		&& classes[0].frameSlots[2] == MAX_PARTICLE_CLASSES + 2u
		&& RenderEffectParticlePlan_ReceiptExact( &candidate, classes,
			spawns, &receipt ) );
	/* Narrow material reads must observe fresh metadata, preserve exact bytes
	 * when restored, and reject unready records without publishing any output. */
	{
		uint32_t index;
		CHECK( RenderSubmission_MaterialIndex( &state, material, &index ) );
		renderMaterialSnapshot_t *snapshot = &state.materials[index].snapshot;
		const renderAlphaMode_t oldAlpha = snapshot->alphaMode;
		const qboolean oldSrgb = snapshot->srgb;
		exactCandidate = candidate; exactReceipt = receipt;
		memcpy( exactClasses, classes, sizeof( classes ) );
		memcpy( exactSpawns, spawns, sizeof( spawns ) );
		snapshot->alphaMode = RENDER_ALPHA_ALPHA_ADDITIVE;
		snapshot->srgb = oldSrgb ? qfalse : qtrue;
		CHECK( RenderEffectParticlePlan_Build( &state, &current, 8u, qtrue,
			3.0f, classes, ARRAY_LEN( classes ), spawns, ARRAY_LEN( spawns ),
			&candidate, &receipt ) );
		CHECK( classes[0].shaderBlendIsAdditive == 1u
			&& classes[0].colorDomain != exactClasses[0].colorDomain
			&& receipt.classDigest != exactReceipt.classDigest
			&& RenderEffectParticlePlan_ReceiptExact( &candidate, classes, spawns, &receipt ) );
		snapshot->alphaMode = oldAlpha; snapshot->srgb = oldSrgb;
		CHECK( RenderEffectParticlePlan_Build( &state, &current, 8u, qtrue,
			3.0f, classes, ARRAY_LEN( classes ), spawns, ARRAY_LEN( spawns ),
			&candidate, &receipt ) );
		CHECK( !memcmp( classes, exactClasses, sizeof( classes ) )
			&& !memcmp( spawns, exactSpawns, sizeof( spawns ) )
			&& !memcmp( &candidate, &exactCandidate, sizeof( candidate ) )
			&& !memcmp( &receipt, &exactReceipt, sizeof( receipt ) ) );
		snapshot->ready = qfalse;
		CHECK( !RenderEffectParticlePlan_Build( &state, &current, 8u, qtrue,
			3.0f, classes, ARRAY_LEN( classes ), spawns, ARRAY_LEN( spawns ),
			&candidate, &receipt ) );
		CHECK( !memcmp( classes, exactClasses, sizeof( classes ) )
			&& !memcmp( spawns, exactSpawns, sizeof( spawns ) )
			&& !memcmp( &candidate, &exactCandidate, sizeof( candidate ) )
			&& !memcmp( &receipt, &exactReceipt, sizeof( receipt ) ) );
		snapshot->ready = qtrue;
	}
	RenderSubmission_Reset( &state );
	return 0;
}

static int TestViewCameraCut( void ) {
	static renderSubmissionState_t state;
	renderSubmissionReceipt_t receipt;
	refdef_t view = {0};
	CHECK( RenderSubmission_Init( &state, 0x43414d45u ) );
	CHECK( !RenderSubmission_ViewCameraCut( &state ) );
	CHECK( RenderSubmission_BeginFrame( &state, 1u ) );
	CHECK( !RenderSubmission_ViewCameraCut( &state ) );
	view.rdflags = RDF_NOWORLDMODEL;
	CHECK( RenderSubmission_RenderScene( &state, &view, 0 ) );
	CHECK( !RenderSubmission_ViewCameraCut( &state ) );
	view.rdflags |= RDF_CAMERA_CUT;
	CHECK( RenderSubmission_RenderScene( &state, &view, 0 ) );
	CHECK( RenderSubmission_ViewCameraCut( &state ) );
	CHECK( !( state.worldSnapshot.rdflags & RDF_HYPERSPACE ) );
	CHECK( RenderSubmission_EndFrame( &state, 1u, &receipt ) );
	CHECK( RenderSubmission_ViewCameraCut( &state ) ); // sealed frame is readable
	CHECK( RenderSubmission_BeginFrame( &state, 2u ) );
	CHECK( !RenderSubmission_ViewCameraCut( &state ) ); // no stale UI-only cut
	view.rdflags = RDF_NOWORLDMODEL | RDF_HYPERSPACE;
	CHECK( RenderSubmission_RenderScene( &state, &view, 0 ) );
	CHECK( RenderSubmission_ViewCameraCut( &state ) ); // legacy teleport still resets
	RenderSubmission_Reset( &state );
	CHECK( !RenderSubmission_ViewCameraCut( &state ) );
	return 0;
}

static int TestMaterialStageTimeDependency( void ) {
	renderMaterialSnapshot_t material;
	CHECK( !RenderSubmission_MaterialStagesUseTime( NULL ) );
	for ( unsigned slot = 0; slot < RENDER_MATERIAL_MAX_STAGES; ++slot ) {
		memset( &material, 0, sizeof( material ) );
		material.stageCount = slot + 1;
		renderMaterialStageSnapshot_t *stage = &material.stages[slot];
		CHECK( !RenderSubmission_MaterialStagesUseTime( &material ) );
		stage->animationFrameCount = 2; stage->animationFrequency = 1;
		CHECK( RenderSubmission_MaterialStagesUseTime( &material ) );
		stage->animationFrameCount = 1;
		CHECK( !RenderSubmission_MaterialStagesUseTime( &material ) );
		stage->rgbGen = RAL_MATERIAL_COLOR_WAVE;
		stage->rgbWave.function = RAL_MATERIAL_WAVE_SIN;
		stage->rgbWave.amplitude = stage->rgbWave.frequency = 1;
		CHECK( RenderSubmission_MaterialStagesUseTime( &material ) );
		stage->rgbWave.frequency = 0;
		CHECK( !RenderSubmission_MaterialStagesUseTime( &material ) );
		stage->alphaGen = RAL_MATERIAL_ALPHA_WAVE;
		stage->alphaWave.function = RAL_MATERIAL_WAVE_NOISE;
		stage->alphaWave.amplitude = stage->alphaWave.frequency = 1;
		CHECK( RenderSubmission_MaterialStagesUseTime( &material ) );
		stage->alphaWave.amplitude = 0;
		CHECK( !RenderSubmission_MaterialStagesUseTime( &material ) );
		stage->scaleScroll[3] = 1;
		CHECK( RenderSubmission_MaterialStagesUseTime( &material ) );
		/* Ordered operations supersede the legacy projection fields. */
		stage->tcModCount = 1;
		stage->tcMods[0].type = RAL_MATERIAL_TCMOD_SCALE;
		stage->tcMods[0].parameters[0] = 2;
		CHECK( !RenderSubmission_MaterialStagesUseTime( &material ) );
		const uint32_t animated[] = { RAL_MATERIAL_TCMOD_SCROLL, RAL_MATERIAL_TCMOD_ROTATE,
			RAL_MATERIAL_TCMOD_TURBULENCE, RAL_MATERIAL_TCMOD_STRETCH };
		for ( unsigned op = 0; op < 4; ++op ) {
			stage->tcMods[0].type = animated[op];
			for ( unsigned p = 0; p < 6; ++p ) stage->tcMods[0].parameters[p] = 1;
			CHECK( RenderSubmission_MaterialStagesUseTime( &material ) );
			memset( stage->tcMods[0].parameters, 0, sizeof( stage->tcMods[0].parameters ) );
			CHECK( !RenderSubmission_MaterialStagesUseTime( &material ) );
		}
	}
	return 0;
}

static int TestRegistrationAssetRetirement( void ) {
	static renderSubmissionState_t state;
	static const byte pixels[1024 * 1024 * 4] = { 255, 0, 0, 255 };
	renderMaterialSnapshot_t snapshot;
	renderTextureSnapshot_t texture;
	qhandle_t previous = 0;
	CHECK( RenderSubmission_Init( &state, 717u ) );
	const void *frameStorage = state.uiPrimitives;
	for ( unsigned cycle = 0; cycle < 40; ++cycle ) {
		const qhandle_t material = RenderSubmission_RegisterMaterialImage( &state,
			RENDER_ASSET_MATERIAL, "test/registration", qfalse, pixels, 1024, 1024 );
		const qhandle_t stage = RenderSubmission_RegisterStageTexture( &state,
			RENDER_ASSET_MATERIAL, "test/registration-stage", qfalse, pixels, 1, 1 );
		CHECK( material > previous && stage > material );
		CHECK( RenderSubmission_BeginFrame( &state, cycle + 1u ) );
		CHECK( !RenderSubmission_ResetRegistrationAssets( &state ) );
		CHECK( RenderSubmission_MaterialSnapshot( &state, material, &snapshot )
			&& snapshot.byteCount == sizeof( pixels ) );
		RenderSubmission_CancelFrame( &state );
		/* Any resident secondary world also bars a global registration reset. */
		state.worlds[1].loaded = qtrue;
		CHECK( !RenderSubmission_ResetRegistrationAssets( &state )
			&& state.materialBytes == sizeof( pixels ) );
		state.worlds[1].loaded = qfalse;
		CHECK( RenderSubmission_ResetRegistrationAssets( &state ) );
		CHECK( !state.materialBytes && !state.materialCount
			&& !state.stageTextureBytes && !state.stageTextureCount
			&& !state.modelBytes && !state.modelCount
			&& !state.registeredAssetCount && !state.registeredMaterialCount
			&& state.ownerGeneration == 717u && state.uiPrimitives == frameStorage );
		CHECK( !RenderSubmission_MaterialSnapshot( &state, material, &snapshot )
			&& !RenderSubmission_TextureSnapshot( &state, stage, &texture ) );
		CHECK( RenderSubmission_ResetRegistrationAssets( &state ) );
		previous = stage;
	}
	RenderSubmission_Reset( &state );
	return 0;
}

int main( int argc, char **argv )
{
	if ( argc == 3 && !strcmp( argv[1], "--write-irradiance-fixture" ) )
		return WriteIrradianceFixture( argv[2], 1808, -868, -64 );
	if ( argc == 6 && !strcmp( argv[1], "--write-irradiance-fixture" ) )
		return WriteIrradianceFixture( argv[2], atoi( argv[3] ), atoi( argv[4] ), atoi( argv[5] ) );
	CHECK( TestLightingSidecar() == 0 );
	CHECK( TestMaterialScriptLighting() == 0 );
	CHECK( TestLegacyLightGridIrradiance() == 0 );
	CHECK( TestInlineSubmodelRegisters() == 0 );
	CHECK( TestIrradianceVolumeOwner() == 0 );
	CHECK( TestModelPayloads() == 0 );
	CHECK( TestDistantWorldDrawPlanBridge() == 0 );
	CHECK( TestMapAuthoredSkyAtmosphere() == 0 );
	CHECK( TestMaterialStageSnapshot() == 0 );
	CHECK( TestMaterialStageTimeDependency() == 0 );
	CHECK( TestRegistrationAssetRetirement() == 0 );
	CHECK( TestMaterialStageDepthCollapse() == 0 );
	CHECK( TestMaterialStageBlendAdditive() == 0 );
	CHECK( TestStageTextureMaterialCapacityIsolation() == 0 );
	CHECK( TestStageTextureDoesNotAliasShaderIdentity() == 0 );
	CHECK( TestLensSourceSubmission() == 0 );
	CHECK( TestEffectParticlePlan() == 0 );
	CHECK( TestViewCameraCut() == 0 );
	static renderSubmissionState_t	  state;
	renderSubmissionReceipt_t receipt, exact;
	mapFile_t				  world;
	dsurface_t				  surface	  = { 0 };
	drawVert_t				  vertices[3] = { 0 };
	int						  indices[3]  = { 0, 1, 2 };
	dshader_t				  shader	  = { 0 };
	refEntity_t				  entity;
	ralIrradianceEntitySampleReceipt_t entityIrradiance = { 0 };
	refEntityMotion_t		  motion = {
		sizeof( motion ), REF_ENTITY_MOTION_VERSION, 7u, 1u, REF_ENTITY_MOTION_ROLE_GENERAL, 0u };
	refdef_t								 view;
	polyVert_t								 poly[3] = { 0 };
	qhandle_t								 model, material, pendingMaterial;
	const byte								 materialPixels[16] = { 255u, 0u, 0u,	255u, 0u,	255u, 0u,	255u,
																	0u,	  0u, 255u, 255u, 255u, 255u, 255u, 255u };
	renderMaterialSnapshot_t				 materialSnapshot;
	renderMaterialLighting_t				 materialLighting = { 0 }, invalidMaterialLighting;
	ralLightingPatchTriangle_t			 lightingTriangles[1], exactLightingTriangles[1];
	renderLightingExtractionReceipt_t	 lightingReceipt, exactLightingReceipt;
	ralEmissiveRouteReceipt_t			 emissiveRoutes[1], untouchedEmissiveRoutes[1];
	ralLightDescription_t				 emissiveProxyLights[4], untouchedEmissiveProxyLights[4];
	ralEmissiveProxyPolicy_t			 emissivePolicy;
	renderEmissiveRoutingReceipt_t		 emissiveReceipt, untouchedEmissiveReceipt;
	const ralEmissiveRouteReceipt_t	*persistentEmissiveRoutes;
	const ralLightDescription_t		*persistentEmissiveProxyLights;
	const renderEmissiveRoutingReceipt_t *persistentEmissiveReceipt;
	uint32_t persistentEmissiveRouteCount, persistentEmissiveProxyLightCount;
	uint64_t								 materialGeneration;
	const renderUiPrimitive_t				*uiPrimitives;
	renderWorldSnapshot_t					 worldSnapshot;
	mapFile_t								 patchWorld;
	dsurface_t								 patchSurface	  = { 0 };
	drawVert_t								 patchVertices[9] = { 0 };
	dshader_t								 patchShader	  = { 0 };
	qhandle_t								 patchMaterial;
	uint32_t								 uiPrimitiveCount = 0u;
	refUiTransform_t					 uiTransform = { REF_UI_TRANSFORM_SCHEMA_VERSION,
		0.0f, 0.0f, 640.0f, 360.0f, 0.2f };
	const renderPolyCommand_t				*effectPolygons;
	const polyVert_t						*effectVertices;
	const renderLightCommand_t				*effectLights;
	uint32_t								 effectPolygonCommands, effectVertexCount, effectLightCount;
	atmosphereFrameState_t					 atmosphere = { 0 }, invalidAtmosphere, legacyAtmosphere = { 0 };
	atmosphereEmitter_t						 atmosphereEmitter		= { 0 };
	atmosphereSurfaceEvent_t				 atmosphereSurfaceEvent = { 0 };
	atmosphereMediaVolume_t					 atmosphereMediaVolume	= { 0 };
	atmosphereEffectProfile_t				 atmosphereProfile = { 0 }, inheritedProfile = { 0 };
	atmosphereEffectProfile_t				 resolvedProfile;
	particleClass_t							 atmosphereParticleClass = { 0 }, resolvedParticleClass;
	renderAtmosphereEffectWorkloadSnapshot_t atmosphereWorkload;
	renderAtmosphereSnapshot_t				 atmosphereSnapshot;
	renderSurfaceClimateSnapshot_t			 surfaceSnapshot;
	renderAtmosphereMediaSnapshot_t			 mediaSnapshot;
	const renderSurfaceClimateTile_t		*surfaceTiles;
	const atmosphereMediaVolume_t			*mediaVolumes;
	const atmosphereEmitter_t				*atmosphereEmitters;
	uint64_t								 atmosphereGeneration;
	spriteDesc_t effectSprite = { 0 };
	emitterDesc_t effectEmitter = { 0 };
	decalDesc_t effectDecal = { 0 };
	ribbonPoint_t effectRibbonPointsIn[2] = { 0 };
	ribbonDesc_t effectRibbon = { 0 };
	beamDesc_t effectBeam = { 0 };
	renderEffectPrimitiveSnapshot_t effectPrimitiveSnapshot;
	const spriteDesc_t *effectSprites;
	const emitterDesc_t *effectEmitters;
	const decalDesc_t *effectDecals;
	const renderEffectRibbonCommand_t *effectRibbons;
	const ribbonPoint_t *effectRibbonPointsOut;
	const beamDesc_t *effectBeams;
	memset( &world, 0, sizeof( world ) );
	strcpy( world.name, "maps/arena1.bsp" );
	world.checksum		 = 0x12345678;
	world.numSurfaces	 = 1;
	world.surfaces		 = &surface;
	world.numDrawVerts	 = 3;
	world.drawVerts		 = vertices;
	world.numDrawIndexes = 3;
	world.drawIndexes	 = indices;
	world.numShaders	 = 1;
	world.shaders		 = &shader;
	surface.surfaceType	 = MST_PLANAR;
	surface.firstVert	 = 0;
	surface.numVerts	 = 3;
	surface.firstIndex	 = 0;
	surface.numIndexes	 = 3;
	strcpy( shader.shader, "textures/a" );
	vertices[0].xyz[0] = 0.0f;
	vertices[0].xyz[1] = 0.0f;
	vertices[0].xyz[2] = 0.0f;
	vertices[1].xyz[0] = 1.0f;
	vertices[1].xyz[1] = 0.0f;
	vertices[1].xyz[2] = 0.0f;
	vertices[2].xyz[0] = 0.0f;
	vertices[2].xyz[1] = 1.0f;
	vertices[2].xyz[2] = 0.0f;
	memset( &entity, 0, sizeof( entity ) );
	entity.reType = RT_MODEL;
	memset( &view, 0, sizeof( view ) );
	view.width			= 1280;
	view.height			= 720;
	view.fov_x			= 90.0f;
	view.fov_y			= 60.0f;
	view.viewaxis[0][0] = 1.0f;
	view.viewaxis[1][1] = 1.0f;
	view.viewaxis[2][2] = 1.0f;
	CHECK( RenderSubmission_Init( &state, 41u ) );
	atmosphere.schemaVersion = WIRED_ATMOSPHERE_SCHEMA_VERSION;
	atmosphere.type			 = ATMOSPHERE_PRECIP_RAIN;
	atmosphere.flags		 = ATMOSPHERE_FLAG_ENABLED | ATMOSPHERE_FLAG_HEIGHTGRID | ATMOSPHERE_FLAG_SURFACE_CLIMATE;
	atmosphere.qualityTier	 = ATMOSPHERE_QUALITY_WEATHER;
	atmosphere.seed			 = 7u;
	atmosphere.distance = atmosphere.visibility = 1500.0f;
	atmosphere.temperatureC						= 8.0f;
	atmosphere.humidity							= 0.9f;
	atmosphere.indoorExposure					= 1.0f;
	atmosphere.surfaceWetness					= 0.4f;
	atmosphere.precipitation[0]					= 1.0f;
	CHECK( RenderSubmission_SetAtmosphere( &state, &atmosphere ) );
	{
		renderSurfaceClimateTargets_t targets;
		CHECK( RenderSubmission_AtmosphereSurfaceTargets( &atmosphere, &targets ) && targets.wetness == 1.0f &&
			   targets.frost == 0.0f && targets.snow == 0.0f && targets.melt > 0.0f );
		invalidAtmosphere				   = atmosphere;
		invalidAtmosphere.temperatureC	   = -8.0f;
		invalidAtmosphere.precipitation[0] = 0.0f;
		invalidAtmosphere.precipitation[1] = 1.0f;
		CHECK( RenderSubmission_AtmosphereSurfaceTargets( &invalidAtmosphere, &targets ) && targets.snow == 1.0f &&
			   targets.frost > 0.0f && targets.melt == 0.0f );
		invalidAtmosphere.flags &= ~ATMOSPHERE_FLAG_SURFACE_CLIMATE;
		CHECK( RenderSubmission_AtmosphereSurfaceTargets( &invalidAtmosphere, &targets ) && targets.wetness == 0.0f &&
			   targets.frost == 0.0f && targets.snow == 0.0f && targets.melt == 0.0f );
	}
	atmosphereParticleClass.shader			   = 1;
	atmosphereParticleClass.emitMode		   = EMIT_POINT;
	atmosphereParticleClass.scatterShape	   = SCATTER_SPHERE;
	atmosphereParticleClass.velocityShape	   = VEL_AXIAL_PLUS_CUBE;
	atmosphereParticleClass.scatterMagnitude   = 2.0f;
	atmosphereParticleClass.axialSpeed		   = 12.0f;
	atmosphereParticleClass.cubeJitter		   = 2.0f;
	atmosphereParticleClass.lifetimeMean	   = 1.2f;
	atmosphereParticleClass.lifetimeJitter	   = 0.2f;
	atmosphereParticleClass.paletteCount	   = 1;
	atmosphereParticleClass.colorPalette[0][0] = 0.82f;
	atmosphereParticleClass.colorPalette[0][1] = 0.90f;
	atmosphereParticleClass.colorPalette[0][2] = 1.0f;
	atmosphereParticleClass.colorPalette[0][3] = 0.38f;
	atmosphereParticleClass.sizeStart		   = 1.5f;
	atmosphereParticleClass.sizeEnd			   = 10.0f;
	atmosphereParticleClass.drag			   = 0.8f;
	CHECK( RenderSubmission_RegisterParticleClass( &state, 1, &atmosphereParticleClass ) );
	CHECK( RenderSubmission_GetParticleClass( &state, 1, &resolvedParticleClass ) &&
		   !memcmp( &resolvedParticleClass, &atmosphereParticleClass, sizeof( resolvedParticleClass ) ) );
	atmosphereParticleClass.sizeEnd = 11.0f;
	CHECK( !RenderSubmission_RegisterParticleClass( &state, 1, &atmosphereParticleClass ) );
	atmosphereParticleClass.sizeEnd			   = 10.0f;
	atmosphereProfile.schemaVersion			   = WIRED_ATMOSPHERE_EFFECT_PROFILE_SCHEMA_VERSION;
	atmosphereProfile.stageCount			   = 2u;
	atmosphereProfile.maxParticles			   = 96u;
	atmosphereProfile.seed					   = 11u;
	atmosphereProfile.duration				   = 2.0f;
	atmosphereProfile.lodFar				   = 1400.0f;
	atmosphereProfile.boundsRadius			   = 64.0f;
	atmosphereProfile.stages[0].trigger		   = ATMOSPHERE_STAGE_EMISSION;
	atmosphereProfile.stages[0].particleClass  = 1u;
	atmosphereProfile.stages[0].parentStage	   = UINT32_MAX;
	atmosphereProfile.stages[0].maxParticles   = 64u;
	atmosphereProfile.stages[0].burstCount	   = 8u;
	atmosphereProfile.stages[0].duration	   = 1.0f;
	atmosphereProfile.stages[0].lodFar		   = 1000.0f;
	atmosphereProfile.stages[0].boundsRadius   = 32.0f;
	atmosphereProfile.stages[0].intensityScale = 1.0f;
	atmosphereProfile.stages[1]				   = atmosphereProfile.stages[0];
	atmosphereProfile.stages[1].trigger		   = ATMOSPHERE_STAGE_DEATH;
	atmosphereProfile.stages[1].parentStage	   = 0u;
	atmosphereProfile.stages[1].flags		   = ATMOSPHERE_STAGE_INHERIT_POSITION | ATMOSPHERE_STAGE_INHERIT_VELOCITY;
	atmosphereProfile.stages[1].maxParticles   = 32u;
	atmosphereProfile.stages[1].burstCount	   = 2u;
	CHECK( RenderSubmission_RegisterAtmosphereEffectProfile( &state, 1u, &atmosphereProfile ) );
	atmosphereProfile.seed++;
	CHECK( !RenderSubmission_RegisterAtmosphereEffectProfile( &state, 1u, &atmosphereProfile ) );
	atmosphereProfile.seed--;
	CHECK( RenderSubmission_RegisterAtmosphereEffectProfile( &state, 1u, &atmosphereProfile ) );
	CHECK( RenderSubmission_GetAtmosphereEffectProfile( &state, 1u, &resolvedProfile ) &&
		   resolvedProfile.stageCount == 2u );
	inheritedProfile					  = atmosphereProfile;
	inheritedProfile.parentProfile		  = 1u;
	inheritedProfile.stageOverrideMask	  = 1u << 1;
	inheritedProfile.stages[1].burstCount = 4u;
	CHECK( RenderSubmission_RegisterAtmosphereEffectProfile( &state, 2u, &inheritedProfile ) );
	CHECK( RenderSubmission_GetAtmosphereEffectProfile( &state, 2u, &resolvedProfile ) &&
		   resolvedProfile.stages[0].burstCount == 8u && resolvedProfile.stages[1].burstCount == 4u );
	inheritedProfile.stageOverrideMask = 1u << 7;
	CHECK( !RenderSubmission_RegisterAtmosphereEffectProfile( &state, 3u, &inheritedProfile ) );
	CHECK( RenderSubmission_AtmosphereSnapshot( &state, &atmosphereSnapshot, &atmosphereEmitters ) &&
		   atmosphereEmitters != NULL && atmosphereSnapshot.state.schemaVersion == WIRED_ATMOSPHERE_SCHEMA_VERSION &&
		   atmosphereSnapshot.state.precipitation[0] == 1.0f && atmosphereSnapshot.active == qtrue &&
		   atmosphereSnapshot.emitterCount == 0u &&
		   atmosphereSnapshot.state.skyEnvironment.schemaVersion == WIRED_SKY_ENVIRONMENT_SCHEMA_VERSION &&
		   atmosphereSnapshot.state.skyEnvironment.mode == ATMOSPHERE_SKY_AUTHORED &&
		   ( atmosphereSnapshot.state.skyEnvironment.flags & ATMOSPHERE_SKY_AUTHORED_AVAILABLE ) );
	{
		ralSkyEnvironmentRuntimeState_t skyRuntime;
		ralSkyEnvironmentRuntimeRequest_t skyRuntimeRequest;
		ralSkyEnvironmentRuntimeReceipt_t skyRuntimeReceipt;
		ralSkyEnvironmentPlanRequest_t skyPlanRequest;
		ralSkyEnvironmentPlanReceipt_t skyPlanReceipt;
		ralSkyEnvironmentCapabilities_t skyCapabilities = { 0 };
		renderAtmosphereSnapshot_t changed = atmosphereSnapshot;
		renderAtmosphereSnapshot_t timelineOnly = atmosphereSnapshot;
		renderAtmosphereSnapshot_t distant = atmosphereSnapshot;
		const uint64_t skyDigest = RenderSubmission_SkyEnvironmentDigest(
			&atmosphereSnapshot );
		timelineOnly.state.timelineSeconds += 5.0f;
		changed = timelineOnly;
		CHECK( skyDigest != 0u &&
			RenderSubmission_SkyEnvironmentDigest( &timelineOnly ) == skyDigest );
		changed.state.skyEnvironment.sunDirection[0] += 0.25f;
		CHECK( RenderSubmission_SkyEnvironmentDigest( &changed ) != skyDigest );
		{
			renderAtmosphereSnapshot_t clouds = atmosphereSnapshot;
			clouds.state.skyEnvironment.cloudSunOcclusion = 1.0f;
			clouds.state.cloudCover = 0.0f;
			const uint64_t clear = RenderSubmission_SkyEnvironmentDigest( &clouds );
			clouds.state.cloudCover = 0.5f;
			CHECK( RenderSubmission_SkyEnvironmentDigest( &clouds ) != clear );
			clouds.state.skyEnvironment.cloudSunOcclusion = 0.0f;
			const uint64_t unshadowed = RenderSubmission_SkyEnvironmentDigest( &clouds );
			clouds.state.cloudCover = 1.0f;
			CHECK( RenderSubmission_SkyEnvironmentDigest( &clouds ) == unshadowed );
			clouds.state.cloudCover = -1.0f;
			CHECK( RenderSubmission_SkyEnvironmentDigest( &clouds ) == 0u );
		}
		distant.state.skyEnvironment.flags |= ATMOSPHERE_SKY_DISTANT_SCENE;
		distant.state.skyEnvironment.distantSceneIdentity = 99u;
		distant.state.skyEnvironment.distantSceneOrigin[0] = 10.0f;
		distant.state.skyEnvironment.distantSceneOrigin[1] = 20.0f;
		distant.state.skyEnvironment.distantSceneOrigin[2] = 30.0f;
		distant.state.skyEnvironment.distantSceneScale = 8.0f;
		distant.state.skyEnvironment.distantSceneLodBias = 1.5f;
		CHECK( Ral_SkyEnvironmentRuntimeInit( RAL_BACKEND_VULKAN, &skyRuntime ) &&
			RenderSubmission_BuildSkyEnvironmentRuntimeRequest( &atmosphereSnapshot,
				&skyRuntime, RAL_BACKEND_VULKAN, 1u,
				RAL_ATMOSPHERE_TIER_WEATHER, 0u, 0u, 0u, 0.0f,
				&skyCapabilities, &skyRuntimeRequest ) &&
				!( skyRuntimeRequest.eventMask & RAL_SKY_EVENT_SOURCE_CHANGE ) &&
				Ral_SkyEnvironmentRuntimeAdvance( &skyRuntime, &skyRuntimeRequest,
					&skyPlanRequest, &skyRuntimeReceipt ) &&
				Ral_SkyEnvironmentPlan( &skyPlanRequest, &skyPlanReceipt ) &&
				Ral_SkyEnvironmentRuntimeSchedule( &skyRuntime, &skyPlanReceipt ) &&
				skyRuntime.targetReady &&
				skyRuntimeRequest.timelineSeconds == 0.0f &&
				skyRuntimeRequest.deltaSeconds == 0.0f &&
				skyPlanReceipt.sunDirection[2] ==
					atmosphereSnapshot.state.skyEnvironment.sunDirection[2] &&
				skyPlanReceipt.exposure ==
					atmosphereSnapshot.state.skyEnvironment.exposure );
		CHECK( RenderSubmission_BuildSkyEnvironmentRuntimeRequest( &timelineOnly,
			&skyRuntime, RAL_BACKEND_VULKAN, 2u,
			RAL_ATMOSPHERE_TIER_WEATHER, 0u, 0u, 0u, 5.0f,
			&skyCapabilities, &skyRuntimeRequest ) &&
			!( skyRuntimeRequest.eventMask & RAL_SKY_EVENT_SOURCE_CHANGE ) &&
			skyRuntimeRequest.timelineSeconds == 5.0f &&
			skyRuntimeRequest.deltaSeconds == 5.0f );
		CHECK( RenderSubmission_BuildSkyEnvironmentRuntimeRequest( &changed,
			&skyRuntime, RAL_BACKEND_VULKAN, 2u,
			RAL_ATMOSPHERE_TIER_WEATHER, 0u, 0u, 0u, 5.0f,
			&skyCapabilities, &skyRuntimeRequest ) &&
			( skyRuntimeRequest.eventMask & RAL_SKY_EVENT_SOURCE_CHANGE ) &&
			skyRuntimeRequest.deltaSeconds == 0.0f );
		CHECK( RenderSubmission_BuildSkyEnvironmentRuntimeRequest( &distant,
			&skyRuntime, RAL_BACKEND_VULKAN, 3u,
			RAL_ATMOSPHERE_TIER_WEATHER, 0u, 0u, 0u, 0.0f,
			&skyCapabilities, &skyRuntimeRequest ) &&
			skyRuntimeRequest.distantSceneRequested &&
			skyRuntimeRequest.distantSceneIdentity == 99u &&
			!memcmp( skyRuntimeRequest.distantSceneOrigin,
				distant.state.skyEnvironment.distantSceneOrigin,
				sizeof( skyRuntimeRequest.distantSceneOrigin ) ) &&
			skyRuntimeRequest.distantSceneScale == 8.0f &&
			skyRuntimeRequest.distantSceneLodBias == 1.5f );
	}
	atmosphereGeneration = atmosphereSnapshot.generation;
	CHECK( RenderSubmission_SetAtmosphere( &state, &atmosphere ) );
	CHECK( RenderSubmission_AtmosphereSnapshot( &state, &atmosphereSnapshot, &atmosphereEmitters ) &&
		   atmosphereSnapshot.generation == atmosphereGeneration );
	invalidAtmosphere		   = atmosphere;
	invalidAtmosphere.humidity = 1.5f;
	CHECK( !RenderSubmission_SetAtmosphere( &state, &invalidAtmosphere ) );
	CHECK( RenderSubmission_AtmosphereSnapshot( &state, &atmosphereSnapshot, &atmosphereEmitters ) &&
		   atmosphereSnapshot.generation == atmosphereGeneration &&
		   atmosphereSnapshot.state.humidity == atmosphere.humidity );
	invalidAtmosphere = atmosphere;
	invalidAtmosphere.skyEnvironment.schemaVersion = WIRED_SKY_ENVIRONMENT_SCHEMA_VERSION;
	invalidAtmosphere.skyEnvironment.mode = ATMOSPHERE_SKY_PHYSICAL;
	invalidAtmosphere.skyEnvironment.sourceRevision = 2u;
	invalidAtmosphere.skyEnvironment.exposure = 1.0f;
	invalidAtmosphere.skyEnvironment.opticsVisibility = 1.0f;
	invalidAtmosphere.skyEnvironment.distantSceneScale = 1.0f;
	invalidAtmosphere.skyEnvironment.skyLuminance = -1.0f;
	CHECK( !RenderSubmission_SetAtmosphere( &state, &invalidAtmosphere ) );
	legacyAtmosphere.type	  = ATMOSPHERE_PRECIP_SNOW;
	legacyAtmosphere.distance = 900.0f;
	CHECK( RenderSubmission_SetAtmosphere( &state, &legacyAtmosphere ) );
	CHECK( RenderSubmission_AtmosphereSnapshot( &state, &atmosphereSnapshot, &atmosphereEmitters ) &&
		   atmosphereSnapshot.state.schemaVersion == WIRED_ATMOSPHERE_SCHEMA_VERSION &&
		   atmosphereSnapshot.state.qualityTier == ATMOSPHERE_QUALITY_WEATHER &&
		   atmosphereSnapshot.state.precipitation[1] == 1.0f &&
		   atmosphereSnapshot.state.skyEnvironment.schemaVersion == WIRED_SKY_ENVIRONMENT_SCHEMA_VERSION &&
		   atmosphereSnapshot.state.skyEnvironment.mode == ATMOSPHERE_SKY_AUTHORED );
	CHECK( RenderSubmission_SetAtmosphere( &state, &atmosphere ) );
	model			= RenderSubmission_RegisterAsset( &state, RENDER_ASSET_MODEL, "models/a.md3" );
	pendingMaterial = RenderSubmission_RegisterAsset( &state, RENDER_ASSET_MSDF, "textures/pending" );
	CHECK( pendingMaterial > model );
	CHECK( RenderSubmission_MaterialSnapshot( &state, pendingMaterial, &materialSnapshot ) &&
		   materialSnapshot.ready == qfalse );
	{
		const uint64_t pendingGeneration = materialSnapshot.generation;
		CHECK( RenderSubmission_RegisterMaterialImage( &state, RENDER_ASSET_MSDF, "textures/PENDING", qtrue,
													   materialPixels, 2u, 2u ) == pendingMaterial );
		CHECK( RenderSubmission_MaterialSnapshot( &state, pendingMaterial, &materialSnapshot ) &&
			   materialSnapshot.ready == qtrue && materialSnapshot.generation > pendingGeneration &&
			   materialSnapshot.msdf == qtrue && materialSnapshot.srgb == qfalse );
	}
	material = RenderSubmission_RegisterMaterialImage( &state, RENDER_ASSET_MATERIAL, "textures/a", qtrue,
													   materialPixels, 2u, 2u );
	CHECK( model > 0 && material > model );
	CHECK( RenderSubmission_MaterialSnapshot( &state, material, &materialSnapshot ) &&
		   materialSnapshot.ready == qtrue && materialSnapshot.width == 2u && materialSnapshot.height == 2u &&
		   materialSnapshot.byteCount == 16u && materialSnapshot.clampToEdge == qtrue &&
		   materialSnapshot.msdf == qfalse && materialSnapshot.srgb == qtrue &&
		   materialSnapshot.alphaMode == RENDER_ALPHA_OPAQUE && materialSnapshot.depthWrite == qtrue &&
		   !memcmp( materialSnapshot.rgba8, materialPixels, sizeof( materialPixels ) ) );
	{
		ralTextureCacheKey_t exactKey, mismatchedKey;
		const uint64_t generation = materialSnapshot.generation;
		CHECK( BuildTestTextureKey( 0x101u, 2u, 2u, &exactKey ) );
		CHECK( RenderSubmission_SetMaterialTextureCacheKey( &state, material,
			&exactKey ) );
		CHECK( RenderSubmission_MaterialSnapshot( &state, material,
			&materialSnapshot ) && materialSnapshot.textureCacheKeyReady
			&& Ral_TextureCacheKeyExact( &materialSnapshot.textureCacheKey,
				&exactKey ) && materialSnapshot.generation > generation );
		materialGeneration = materialSnapshot.generation;
		CHECK( BuildTestTextureKey( 0x202u, 1u, 1u, &mismatchedKey ) );
		CHECK( !RenderSubmission_SetMaterialTextureCacheKey( &state, material,
			&mismatchedKey ) );
		CHECK( RenderSubmission_MaterialSnapshot( &state, material,
			&materialSnapshot ) && materialSnapshot.generation == materialGeneration
			&& Ral_TextureCacheKeyExact( &materialSnapshot.textureCacheKey,
				&exactKey ) );
	}
	materialGeneration = materialSnapshot.generation;
	{
		const byte transparentPixels[4] = { 200u, 100u, 50u, 0u };
		qhandle_t transparentMaterial = RenderSubmission_RegisterMaterialImage(
			&state, RENDER_ASSET_MATERIAL, "textures/raw-alpha", qfalse,
			transparentPixels, 1u, 1u );
		CHECK( transparentMaterial > 0 && RenderSubmission_MaterialSnapshot(
			&state, transparentMaterial, &materialSnapshot )
			&& materialSnapshot.alphaMode == RENDER_ALPHA_OPAQUE
			&& materialSnapshot.depthWrite == qtrue );
		{
			const byte cutoutPixels[8] = {
				200u, 100u, 50u, 0u, 200u, 100u, 50u, 255u };
			qhandle_t cutoutMaterial = RenderSubmission_RegisterMaterialImage(
				&state, RENDER_ASSET_MATERIAL, "textures/raw-cutout", qfalse,
				cutoutPixels, 2u, 1u );
			CHECK( cutoutMaterial > 0 && RenderSubmission_MaterialSnapshot(
				&state, cutoutMaterial, &materialSnapshot )
				&& materialSnapshot.alphaMode == RENDER_ALPHA_MASK
				&& materialSnapshot.depthWrite == qtrue );
		}
	}
	CHECK( RenderSubmission_SetMaterialRasterPolicy( &state, material, RENDER_ALPHA_BLEND, 0.25f, qfalse ) );
	CHECK( RenderSubmission_MaterialSnapshot( &state, material, &materialSnapshot ) &&
		   materialSnapshot.generation > materialGeneration && materialSnapshot.alphaMode == RENDER_ALPHA_BLEND &&
		   materialSnapshot.alphaCutoff == 0.25f && materialSnapshot.depthWrite == qfalse );
	CHECK( !RenderSubmission_SetMaterialRasterPolicy( &state, material, RENDER_ALPHA_BLEND, NAN, qfalse ) );
	CHECK( RenderSubmission_LoadWorld( &state, &world, 0 ) );
	{
		renderMaterialFactorPublicationEntry_t factorEntry;
		renderMaterialSnapshot_t beforeFactors;
		memset( &factorEntry, 0, sizeof( factorEntry ) );
		factorEntry.material = material;
		factorEntry.factors.baseColor[0] = 0.8f;
		factorEntry.factors.baseColor[1] = 0.7f;
		factorEntry.factors.baseColor[2] = 0.6f;
		factorEntry.factors.baseColor[3] = 1.0f;
		factorEntry.factors.emissive[0] = 0.2f;
		factorEntry.factors.metallic = 0.35f;
		factorEntry.factors.roughness = 0.45f;
		factorEntry.factors.normalScale = 1.0f;
		factorEntry.factors.occlusionStrength = 0.9f;
		CHECK( RenderSubmission_MaterialSnapshot( &state, material,
			&beforeFactors ) && !beforeFactors.factors.ready );
		factorEntry.factors.roughness = NAN;
		CHECK( !RenderSubmission_PublishMaterialFactors( &state, 71u,
			&factorEntry, 1u ) );
		CHECK( RenderSubmission_MaterialSnapshot( &state, material,
			&materialSnapshot )
			&& materialSnapshot.generation == beforeFactors.generation
			&& !materialSnapshot.factors.ready );
		factorEntry.factors.roughness = 0.45f;
		CHECK( RenderSubmission_PublishMaterialFactors( &state, 71u,
			&factorEntry, 1u ) );
		CHECK( RenderSubmission_MaterialSnapshot( &state, material,
			&materialSnapshot ) && materialSnapshot.factors.ready
			&& materialSnapshot.factors.schemaVersion
				== RENDER_MATERIAL_FACTORS_SCHEMA_VERSION
			&& materialSnapshot.factors.runtimeGeneration == 71u
			&& materialSnapshot.factors.factors.metallic == 0.35f
			&& materialSnapshot.factors.factors.roughness == 0.45f );
		CHECK( RenderSubmission_PublishMaterialFactors( &state, 0u, NULL, 0u )
			&& RenderSubmission_MaterialSnapshot( &state, material,
				&materialSnapshot ) && !materialSnapshot.factors.ready
			&& materialSnapshot.factors.runtimeGeneration == 0u );
	}
	memset( lightingTriangles, 0xa5, sizeof( lightingTriangles ) );
	memset( &lightingReceipt, 0xa5, sizeof( lightingReceipt ) );
	exactLightingTriangles[0] = lightingTriangles[0];
	exactLightingReceipt = lightingReceipt;
	CHECK( !RenderSubmission_ExtractLightingTriangles( &state, 1u, lightingTriangles, 1u, &lightingReceipt ) &&
		   !memcmp( lightingTriangles, exactLightingTriangles, sizeof( lightingTriangles ) ) &&
		   !memcmp( &lightingReceipt, &exactLightingReceipt, sizeof( lightingReceipt ) ) );
	materialLighting.schemaVersion = RENDER_MATERIAL_LIGHTING_SCHEMA_VERSION;
	materialLighting.sourceGeneration = 17u;
	materialLighting.provenanceHash = 0x8912u;
	materialLighting.diffuseReflectanceQ16[0] = RENDER_MATERIAL_LIGHTING_Q16_ONE / 2;
	materialLighting.diffuseReflectanceQ16[1] = RENDER_MATERIAL_LIGHTING_Q16_ONE / 4;
	materialLighting.diffuseReflectanceQ16[2] = RENDER_MATERIAL_LIGHTING_Q16_ONE / 8;
	materialLighting.emissionRadianceQ16[0] = RENDER_MATERIAL_LIGHTING_Q16_ONE * 2;
	materialLighting.emissiveMobility = RAL_LIGHT_MOBILITY_STATIC;
	materialLighting.emissiveInfluenceRangeQ16 = RAL_LIGHT_Q16_ONE * 8;
	materialLighting.emissiveShadowPriority = 7u;
	materialLighting.emissiveRequestedProxyCount = 2u;
	materialLighting.participatesInStaticBake = qtrue;
	materialLighting.emissiveInjectsAtmosphere = qtrue;
	materialLighting.ready = qtrue;
	invalidMaterialLighting = materialLighting;
	invalidMaterialLighting.diffuseReflectanceQ16[0] = RENDER_MATERIAL_LIGHTING_Q16_ONE + 1;
	CHECK( !RenderSubmission_SetMaterialLighting( &state, material, &invalidMaterialLighting ) );
	CHECK( RenderSubmission_SetMaterialLighting( &state, material, &materialLighting ) );
	CHECK( RenderSubmission_MaterialSnapshot( &state, material, &materialSnapshot ) &&
		   materialSnapshot.lighting.ready == qtrue &&
		   materialSnapshot.lighting.sourceGeneration == materialLighting.sourceGeneration &&
		   materialSnapshot.lighting.provenanceHash == materialLighting.provenanceHash );
	CHECK( RenderSubmission_EmissiveAuthoritySnapshot( &state,
		&persistentEmissiveRoutes, &persistentEmissiveRouteCount,
		&persistentEmissiveProxyLights, &persistentEmissiveProxyLightCount,
		&persistentEmissiveReceipt ) &&
		persistentEmissiveRouteCount == 1u && persistentEmissiveProxyLightCount == 2u &&
		persistentEmissiveReceipt->routeCount == 1u &&
		persistentEmissiveReceipt->proxyLightCount == 2u &&
		persistentEmissiveRoutes[0].radianceAuthorityHash != 0u &&
		persistentEmissiveRoutes[0].radianceQ16[0] == materialLighting.emissionRadianceQ16[0] &&
		persistentEmissiveProxyLights[0].radianceQ16[0] +
			persistentEmissiveProxyLights[1].radianceQ16[0] ==
			persistentEmissiveRoutes[0].radianceQ16[0] );
	CHECK( RenderSubmission_ExtractLightingTriangles( &state, 1u, lightingTriangles, 1u, &lightingReceipt ) &&
		   RenderSubmission_LightingExtractionReceiptValid( &lightingReceipt ) && lightingReceipt.triangleCount == 1u &&
		   lightingReceipt.skippedBatchCount == 0u &&
		   lightingTriangles[0].schemaVersion == RAL_LIGHTING_PATCH_SCHEMA_VERSION &&
		   lightingTriangles[0].surfaceId == 1u && lightingTriangles[0].triangleId == ( UINT64_C( 1 ) << 32u ) + 1u &&
		   lightingTriangles[0].vertices[0].x == 0 && lightingTriangles[0].vertices[0].y == 0 &&
		   lightingTriangles[0].vertices[1].x == RAL_LIGHT_Q16_ONE && lightingTriangles[0].vertices[1].y == 0 &&
		   lightingTriangles[0].vertices[2].x == 0 && lightingTriangles[0].vertices[2].y == RAL_LIGHT_Q16_ONE &&
		   lightingTriangles[0].diffuseReflectanceQ16[1] == RENDER_MATERIAL_LIGHTING_Q16_ONE / 4 &&
		   lightingTriangles[0].emissionRadianceQ16[0] == RENDER_MATERIAL_LIGHTING_Q16_ONE * 2 );
	CHECK( RenderSubmission_ExtractLightingTriangles( &state, 1u, exactLightingTriangles, 1u,
												&exactLightingReceipt ) &&
		   !memcmp( lightingTriangles, exactLightingTriangles, sizeof( lightingTriangles ) ) &&
		   !memcmp( &lightingReceipt, &exactLightingReceipt, sizeof( lightingReceipt ) ) );
	emissivePolicy = (ralEmissiveProxyPolicy_t){ 4u, 4u, 1, 1 };
	CHECK( RenderSubmission_BuildEmissiveRoutes( &state, 3u, &emissivePolicy,
		emissiveRoutes, 1u, emissiveProxyLights, 4u, &emissiveReceipt ) &&
		RenderSubmission_EmissiveRoutingReceiptValid( &emissiveReceipt ) &&
		emissiveReceipt.routeCount == 1u && emissiveReceipt.proxyLightCount == 2u &&
		emissiveReceipt.directRouteCount == 1u && emissiveReceipt.bakeRouteCount == 1u &&
		emissiveReceipt.bloomRouteCount == 1u && emissiveReceipt.atmosphereRouteCount == 1u &&
		emissiveRoutes[0].radianceAuthorityHash && emissiveRoutes[0].proxyCount == 2u &&
		Ral_LightDescriptionValid( &emissiveProxyLights[0] ) &&
		Ral_LightDescriptionValid( &emissiveProxyLights[1] ) );
	memset( untouchedEmissiveRoutes, 0xa5, sizeof( untouchedEmissiveRoutes ) );
	memset( untouchedEmissiveProxyLights, 0x5a, sizeof( untouchedEmissiveProxyLights ) );
	memset( &untouchedEmissiveReceipt, 0x3c, sizeof( untouchedEmissiveReceipt ) );
	CHECK( !RenderSubmission_BuildEmissiveRoutes( &state, 4u, &emissivePolicy,
		untouchedEmissiveRoutes, 1u, untouchedEmissiveProxyLights, 1u,
		&untouchedEmissiveReceipt ) &&
		((const unsigned char *)untouchedEmissiveRoutes)[0] == 0xa5u &&
		((const unsigned char *)untouchedEmissiveProxyLights)[0] == 0x5au &&
		((const unsigned char *)&untouchedEmissiveReceipt)[0] == 0x3cu );
	{
		ralLightingPatchTriangle_t visibilityTriangles[3];
		const renderLightingVisibilityCandidate_t candidates[4] = {
			{ 0u, 1u }, { 0u, 2u }, { 1u, 0u }, { 2u, 0u }
		};
		ralLightingPatchVisibility_t scratchVisibility[4], visibility[4], exactVisibility[4];
		renderLightingVisibilityRequest_t visibilityRequest;
		renderLightingVisibilityReceipt_t visibilityReceipt, repeatedVisibilityReceipt;
		lightingVisibilityFixture_t fixture;
		visibilityTriangles[0] = lightingTriangles[0];
		visibilityTriangles[1] = lightingTriangles[0];
		visibilityTriangles[2] = lightingTriangles[0];
		visibilityTriangles[1].triangleId++;
		visibilityTriangles[1].regionId = 2u;
		visibilityTriangles[1].vertices[0].z = visibilityTriangles[1].vertices[1].z =
			visibilityTriangles[1].vertices[2].z = RAL_LIGHT_Q16_ONE;
		visibilityTriangles[2].triangleId += 2u;
		visibilityTriangles[2].regionId = 3u;
		visibilityTriangles[2].vertices[0].z = visibilityTriangles[2].vertices[1].z =
			visibilityTriangles[2].vertices[2].z = RAL_LIGHT_Q16_ONE * 2;
		fixture.occludedEmitterId = visibilityTriangles[2].triangleId;
		memset( &visibilityRequest, 0, sizeof( visibilityRequest ) );
		visibilityRequest.schemaVersion = RENDER_LIGHTING_VISIBILITY_SCHEMA_VERSION;
		visibilityRequest.queryGeneration = 1u;
		visibilityRequest.visibilityAuthorityHash = 0x4411u;
		visibilityRequest.triangles = visibilityTriangles;
		visibilityRequest.triangleCount = 3u;
		visibilityRequest.candidates = candidates;
		visibilityRequest.candidateCount = 4u;
		visibilityRequest.query = LightingVisibilityFixtureQuery;
		visibilityRequest.queryUserData = &fixture;
		CHECK( RenderSubmission_BuildLightingVisibility( &visibilityRequest, scratchVisibility, 4u, visibility, 4u,
													 &visibilityReceipt ) &&
			   RenderSubmission_LightingVisibilityReceiptValid( &visibilityReceipt ) &&
			   visibilityReceipt.queriedCount == 4u && visibilityReceipt.visibleCount == 3u &&
			   visibilityReceipt.occludedCount == 1u && visibility[0].receiverTriangle == 0u &&
			   visibility[0].emitterTriangle == 1u && visibility[1].receiverTriangle == 1u &&
			   visibility[1].emitterTriangle == 0u && visibility[2].receiverTriangle == 2u &&
			   visibility[2].emitterTriangle == 0u );
		CHECK( RenderSubmission_BuildLightingVisibility( &visibilityRequest, scratchVisibility, 4u, exactVisibility, 4u,
													 &repeatedVisibilityReceipt ) &&
			   !memcmp( visibility, exactVisibility,
						visibilityReceipt.visibleCount * sizeof( *visibility ) ) &&
			   !memcmp( &visibilityReceipt, &repeatedVisibilityReceipt, sizeof( visibilityReceipt ) ) );
		visibilityRequest.queryGeneration = 2u;
		visibilityRequest.dirtyRegionCount = 1u;
		visibilityRequest.dirtyRegionIds[0] = 2u;
		CHECK( RenderSubmission_BuildLightingVisibility( &visibilityRequest, scratchVisibility, 4u, exactVisibility, 4u,
													 &repeatedVisibilityReceipt ) &&
			   repeatedVisibilityReceipt.queriedCount == 2u && repeatedVisibilityReceipt.visibleCount == 2u &&
			   repeatedVisibilityReceipt.dirtyRegionCount == 1u && exactVisibility[0].receiverTriangle == 0u &&
			   exactVisibility[0].emitterTriangle == 1u && exactVisibility[1].receiverTriangle == 1u &&
			   exactVisibility[1].emitterTriangle == 0u );
	}
	{
		const uint64_t geometryHash = lightingReceipt.geometryHash;
		const uint64_t emissiveHash = lightingReceipt.emissiveHash;
		materialLighting.emissionRadianceQ16[0]++;
		CHECK( RenderSubmission_SetMaterialLighting( &state, material, &materialLighting ) &&
			   RenderSubmission_ExtractLightingTriangles( &state, 2u, exactLightingTriangles, 1u,
													&exactLightingReceipt ) &&
			   exactLightingReceipt.geometryHash == geometryHash && exactLightingReceipt.emissiveHash != emissiveHash &&
			   exactLightingReceipt.materialDigest != lightingReceipt.materialDigest );
	}
	CHECK( RenderSubmission_ClearScene( &state ) );
	CHECK( RenderSubmission_BeginFrame( &state, 42u ) );
	CHECK( RenderSubmission_AddPoly( &state, material, 3, poly, 1 ) );
	CHECK( RenderSubmission_AddLight( &state, entity.origin, NULL, 100.0f, 1.0f, 0.5f, 0.25f ) );
	CHECK( RenderSubmission_ClearScene( &state ) );
	effectSprite.origin[0] = 4.0f;
	effectSprite.radius = 8.0f;
	effectSprite.rgba[0] = effectSprite.rgba[1] = effectSprite.rgba[2]
		= effectSprite.rgba[3] = 1.0f;
	effectSprite.shader = material;
	CHECK( RenderSubmission_AddEffectSprite( &state, &effectSprite ) );
	effectEmitter.cls = 1;
	effectEmitter.count = 8;
	effectEmitter.origin[0] = 4.0f;
	effectEmitter.axis[2] = 1.0f;
	effectEmitter.colorTint[0] = effectEmitter.colorTint[1]
		= effectEmitter.colorTint[2] = effectEmitter.colorTint[3] = 1.0f;
	CHECK( RenderSubmission_AddEffectEmitter( &state, &effectEmitter ) );
	effectDecal.origin[0] = 4.0f;
	effectDecal.normal[2] = 1.0f;
	effectDecal.radius = 16.0f;
	effectDecal.rgba[0] = effectDecal.rgba[1] = effectDecal.rgba[2]
		= effectDecal.rgba[3] = 1.0f;
	effectDecal.shader = material;
	effectDecal.lifetime = 4.0f;
	CHECK( RenderSubmission_AddEffectDecal( &state, &effectDecal ) );
	effectRibbonPointsIn[0].pos[0] = 4.0f;
	effectRibbonPointsIn[1].pos[0] = 12.0f;
	for ( uint32_t i = 0u; i < 2u; ++i ) {
		effectRibbonPointsIn[i].width = 2.0f;
		effectRibbonPointsIn[i].rgba[0] = effectRibbonPointsIn[i].rgba[1]
			= effectRibbonPointsIn[i].rgba[2] = effectRibbonPointsIn[i].rgba[3]
			= 1.0f;
	}
	effectRibbon.points = effectRibbonPointsIn;
	effectRibbon.numPoints = 2;
	effectRibbon.shader = material;
	CHECK( RenderSubmission_AddEffectRibbon( &state, &effectRibbon ) );
	effectBeam.start[0] = 4.0f;
	effectBeam.end[0] = 12.0f;
	effectBeam.startWidth = 2.0f;
	effectBeam.endWidth = 1.0f;
	effectBeam.startColor[0] = effectBeam.startColor[1]
		= effectBeam.startColor[2] = effectBeam.startColor[3] = 1.0f;
	effectBeam.endColor[0] = effectBeam.endColor[1]
		= effectBeam.endColor[2] = effectBeam.endColor[3] = 1.0f;
	effectBeam.shader = material;
	effectBeam.duration = 0.25f;
	effectBeam.fadeOut = 0.1f;
	effectBeam.axialCopies = 1;
	effectBeam.startEntityNum = effectBeam.endEntityNum = -1;
	CHECK( RenderSubmission_AddEffectBeam( &state, &effectBeam ) );
	effectSprite.radius = 0.0f;
	CHECK( !RenderSubmission_AddEffectSprite( &state, &effectSprite ) );
	CHECK( RenderSubmission_EffectPrimitiveSnapshots( &state,
		&effectPrimitiveSnapshot, &effectSprites, &effectEmitters,
		&effectDecals, &effectRibbons, &effectRibbonPointsOut, &effectBeams )
		&& effectPrimitiveSnapshot.digest != 0u
		&& effectPrimitiveSnapshot.spriteCount == 1u
		&& effectPrimitiveSnapshot.emitterCount == 1u
		&& effectPrimitiveSnapshot.decalCount == 1u
		&& effectPrimitiveSnapshot.ribbonCount == 1u
		&& effectPrimitiveSnapshot.ribbonPointCount == 2u
		&& effectPrimitiveSnapshot.beamCount == 1u
		&& effectPrimitiveSnapshot.droppedCount == 1u
		&& effectSprites[0].radius == 8.0f
		&& effectEmitters[0].count == 8
		&& effectDecals[0].lifetime == 4.0f
		&& effectRibbons[0].firstPoint == 0u
		&& effectRibbons[0].pointCount == 2u
		&& effectRibbonPointsOut[1].pos[0] == 12.0f
		&& effectBeams[0].duration == 0.25f );
	{
		renderEffectDecalPool_t pool, candidate, preserved;
		renderEffectDecalPoolReceipt_t poolReceipt;
		RenderEffectDecalPool_Reset( &pool );
		CHECK( RenderEffectDecalPool_Advance( &pool, 1u, qtrue, 10.0f,
			&effectDecal, 1u, &candidate, &poolReceipt )
			&& RenderEffectDecalPool_ReceiptExact( &candidate, &poolReceipt )
			&& poolReceipt.admittedCount == 1u
			&& poolReceipt.activeCount == 1u
			&& pool.activeCount == 0u );
		pool = candidate;
		CHECK( RenderEffectDecalPool_Advance( &pool, 2u, qtrue, 11.0f,
			NULL, 0u, &candidate, &poolReceipt )
			&& poolReceipt.retiredCount == 0u && candidate.activeCount == 1u );
		pool = candidate;
		CHECK( RenderEffectDecalPool_Advance( &pool, 3u, qtrue, 14.0f,
			NULL, 0u, &candidate, &poolReceipt )
			&& poolReceipt.retiredCount == 1u && candidate.activeCount == 0u );
		pool = candidate;
		CHECK( RenderEffectDecalPool_Advance( &pool, 4u, qtrue, 15.0f,
			&effectDecal, 1u, &candidate, &poolReceipt ) );
		pool = candidate;
		CHECK( RenderEffectDecalPool_Advance( &pool, 5u, qfalse, 0.0f,
			NULL, 0u, &preserved, &poolReceipt )
			&& preserved.activeCount == 1u
			&& preserved.digest == pool.digest );
		CHECK( RenderEffectDecalPool_Advance( &pool, 6u, qtrue, 1.0f,
			NULL, 0u, &candidate, &poolReceipt )
			&& candidate.activeCount == 0u );
		CHECK( !RenderEffectDecalPool_Advance( &pool, 7u, qfalse, 0.0f,
			&effectDecal, 1u, &candidate, &poolReceipt ) );
	}
	{
		renderEffectBeamPool_t pool, candidate, preserved;
		renderEffectBeamPoolReceipt_t poolReceipt;
		beamDesc_t transientBeam = effectBeam;
		RenderEffectBeamPool_Reset( &pool );
		CHECK( RenderEffectBeamPool_Advance( &pool, 1u, qtrue, 10.0f,
			&effectBeam, 1u, &candidate, &poolReceipt )
			&& RenderEffectBeamPool_ReceiptExact( &candidate, &poolReceipt )
			&& poolReceipt.admittedCount == 1u
			&& poolReceipt.activeCount == 1u
			&& pool.activeCount == 0u );
		pool = candidate;
		CHECK( RenderEffectBeamPool_Advance( &pool, 2u, qtrue, 10.1f,
			NULL, 0u, &candidate, &poolReceipt )
			&& poolReceipt.retiredCount == 0u && candidate.activeCount == 1u );
		pool = candidate;
		transientBeam.duration = 0.0f;
		CHECK( RenderEffectBeamPool_Advance( &pool, 3u, qtrue, 10.2f,
			&transientBeam, 1u, &candidate, &poolReceipt )
			&& candidate.activeCount == 2u );
		pool = candidate;
		CHECK( RenderEffectBeamPool_Advance( &pool, 4u, qtrue, 10.3f,
			NULL, 0u, &candidate, &poolReceipt )
			&& poolReceipt.retiredCount == 2u && candidate.activeCount == 0u );
		pool = candidate;
		CHECK( RenderEffectBeamPool_Advance( &pool, 5u, qtrue, 11.0f,
			&effectBeam, 1u, &candidate, &poolReceipt ) );
		pool = candidate;
		CHECK( RenderEffectBeamPool_Advance( &pool, 6u, qfalse, 0.0f,
			NULL, 0u, &preserved, &poolReceipt )
			&& preserved.activeCount == 1u
			&& preserved.digest == pool.digest );
		CHECK( RenderEffectBeamPool_Advance( &pool, 7u, qtrue, 1.0f,
			NULL, 0u, &candidate, &poolReceipt )
			&& candidate.activeCount == 0u );
		CHECK( !RenderEffectBeamPool_Advance( &pool, 8u, qfalse, 0.0f,
			&effectBeam, 1u, &candidate, &poolReceipt ) );
	}
	{
		atmosphereFrameState_t snowAtmosphere = atmosphere;
		snowAtmosphere.timelineSeconds		  = 10.0f;
		snowAtmosphere.transitionSeconds	  = 5.0f;
		snowAtmosphere.temperatureC			  = -8.0f;
		snowAtmosphere.precipitation[0]		  = 0.0f;
		snowAtmosphere.precipitation[1]		  = 1.0f;
		CHECK( RenderSubmission_SetAtmosphere( &state, &snowAtmosphere ) );
		atmosphereSurfaceEvent.schemaVersion   = WIRED_ATMOSPHERE_SURFACE_EVENT_SCHEMA_VERSION;
		atmosphereSurfaceEvent.kind			   = ATMOSPHERE_SURFACE_EVENT_FOOTPRINT;
		atmosphereSurfaceEvent.id			   = 91u;
		atmosphereSurfaceEvent.origin[0]	   = 32.0f;
		atmosphereSurfaceEvent.origin[1]	   = 48.0f;
		atmosphereSurfaceEvent.radius		   = 0.0f;
		atmosphereSurfaceEvent.strength		   = 1.0f;
		atmosphereSurfaceEvent.timelineSeconds = 10.0f;
		CHECK( RenderSubmission_AddAtmosphereSurfaceEvent( &state, &atmosphereSurfaceEvent ) );
		CHECK( !RenderSubmission_AddAtmosphereSurfaceEvent( &state, &atmosphereSurfaceEvent ) );
		CHECK( RenderSubmission_AtmosphereSurfaceSnapshot( &state, &surfaceSnapshot, &surfaceTiles ) &&
			   surfaceSnapshot.tileCount == 1u && surfaceSnapshot.frameEventCount == 1u &&
			   surfaceSnapshot.evictionCount == 0u && surfaceSnapshot.digest != 0u && surfaceTiles != NULL &&
			   surfaceTiles[0].tileX == 0 && surfaceTiles[0].tileY == 0 && surfaceTiles[0].lastEventId == 91u &&
			   ( surfaceTiles[0].eventMask & ( 1u << ATMOSPHERE_SURFACE_EVENT_FOOTPRINT ) ) != 0u &&
			   surfaceTiles[0].snow > 0.14f && surfaceTiles[0].snow < 0.16f );
		snowAtmosphere.timelineSeconds = 15.0f;
		CHECK( RenderSubmission_SetAtmosphere( &state, &snowAtmosphere ) );
		CHECK( RenderSubmission_AtmosphereSurfaceSnapshot( &state, &surfaceSnapshot, &surfaceTiles ) &&
			   surfaceTiles[0].snow > 0.68f && surfaceTiles[0].snow < 0.70f );
		snowAtmosphere.qualityTier = ATMOSPHERE_QUALITY_FULL;
		snowAtmosphere.flags |= ATMOSPHERE_FLAG_VOLUMETRIC_MEDIA;
		snowAtmosphere.mediaDensity		  = 0.025f;
		snowAtmosphere.mediaHeightFalloff = 0.01f;
		CHECK( RenderSubmission_SetAtmosphere( &state, &snowAtmosphere ) );
	}
	atmosphereMediaVolume.schemaVersion = WIRED_ATMOSPHERE_MEDIA_VOLUME_SCHEMA_VERSION;
	atmosphereMediaVolume.shape			= ATMOSPHERE_MEDIA_VOLUME_SPHERE;
	atmosphereMediaVolume.flags			= ATMOSPHERE_MEDIA_VOLUME_CAST_SHADOW;
	atmosphereMediaVolume.priority		= 1u;
	atmosphereMediaVolume.radius		= 96.0f;
	atmosphereMediaVolume.extinction	= 0.15f;
	atmosphereMediaVolume.albedo[0]		= 0.8f;
	atmosphereMediaVolume.albedo[1]		= 0.85f;
	atmosphereMediaVolume.albedo[2]		= 0.9f;
	atmosphereMediaVolume.anisotropy	= 0.45f;
	atmosphereMediaVolume.timelineStart = 15.0f;
	for ( uint32_t i = 0u; i < RENDER_SUBMISSION_MAX_ATMOSPHERE_MEDIA_VOLUMES; ++i ) {
		atmosphereMediaVolume.id		= i + 1u;
		atmosphereMediaVolume.origin[0] = (float)i * 32.0f;
		CHECK( RenderSubmission_AddAtmosphereMediaVolume( &state, &atmosphereMediaVolume ) );
	}
	CHECK( !RenderSubmission_AddAtmosphereMediaVolume( &state, &atmosphereMediaVolume ) );
	atmosphereMediaVolume.id	   = 65u;
	atmosphereMediaVolume.priority = 0u;
	CHECK( RenderSubmission_AddAtmosphereMediaVolume( &state, &atmosphereMediaVolume ) );
	atmosphereMediaVolume.id	   = 66u;
	atmosphereMediaVolume.priority = 2u;
	CHECK( RenderSubmission_AddAtmosphereMediaVolume( &state, &atmosphereMediaVolume ) );
	CHECK( RenderSubmission_AtmosphereMediaSnapshot( &state, &mediaSnapshot, &mediaVolumes ) &&
		   mediaSnapshot.count == RENDER_SUBMISSION_MAX_ATMOSPHERE_MEDIA_VOLUMES && mediaSnapshot.droppedCount == 2u &&
		   mediaSnapshot.digest != 0u && mediaVolumes != NULL );
	atmosphereEmitter.schemaVersion = WIRED_ATMOSPHERE_SCHEMA_VERSION;
	atmosphereEmitter.kind			= ATMOSPHERE_EMITTER_BREATH;
	atmosphereEmitter.id			= 17u;
	atmosphereEmitter.seed			= 3u;
	atmosphereEmitter.profile		= 2u;
	atmosphereEmitter.intensity		= 0.75f;
	atmosphereEmitter.radius		= 8.0f;
	atmosphereEmitter.temperatureC	= 37.0f;
	atmosphereEmitter.humidity		= 1.0f;
	atmosphereEmitter.lifetime		= 1.5f;
	CHECK( RenderSubmission_AtmosphereEmitterIntensity( &atmosphere, &atmosphereEmitter ) > 0.65f );
	{
		atmosphereFrameState_t context	  = atmosphere;
		atmosphereEmitter_t	   contextual = atmosphereEmitter;
		context.qualityTier				  = ATMOSPHERE_QUALITY_OFF;
		context.flags					  = 0u;
		CHECK( RenderSubmission_AtmosphereEmitterIntensity( &context, &contextual ) == 0.0f );
		context				   = atmosphere;
		contextual.kind		   = ATMOSPHERE_EMITTER_PRECIPITATION_IMPACT;
		context.indoorExposure = 0.0f;
		CHECK( RenderSubmission_AtmosphereEmitterIntensity( &context, &contextual ) == 0.0f );
		context.indoorExposure = 1.0f;
		CHECK( RenderSubmission_AtmosphereEmitterIntensity( &context, &contextual ) == contextual.intensity );
		contextual.kind = ATMOSPHERE_EMITTER_DEBRIS;
		context.wind[0] = 18.0f;
		CHECK( RenderSubmission_AtmosphereEmitterIntensity( &context, &contextual ) == contextual.intensity );
		contextual.kind = ATMOSPHERE_EMITTER_GROUND_MIST;
		CHECK( RenderSubmission_AtmosphereEmitterIntensity( &context, &contextual ) > 0.0f );
	}
	CHECK( RenderSubmission_AddAtmosphereEmitter( &state, &atmosphereEmitter ) );
	CHECK( !RenderSubmission_AddAtmosphereEmitter( &state, &atmosphereEmitter ) );
	CHECK( RenderSubmission_AtmosphereSnapshot( &state, &atmosphereSnapshot, &atmosphereEmitters ) &&
		   atmosphereSnapshot.emitterCount == 1u && atmosphereEmitters[0].kind == ATMOSPHERE_EMITTER_BREATH &&
		   atmosphereEmitters[0].id == 17u && atmosphereEmitters[0].profile == 2u );
	CHECK( RenderSubmission_AtmosphereEffectWorkloadSnapshot( &state, 4u, &atmosphereWorkload ) &&
		   atmosphereWorkload.digest != 0u && atmosphereWorkload.sourceEmitterCount == 1u &&
		   atmosphereWorkload.admittedEmitterCount == 1u && atmosphereWorkload.admittedParticleCount == 4u &&
		   atmosphereWorkload.requestedParticleCount >= 4u &&
		   atmosphereWorkload.droppedParticleCount == atmosphereWorkload.requestedParticleCount - 4u &&
		   atmosphereWorkload.workloads[0].emitter.id == 17u && atmosphereWorkload.workloads[0].particleCount == 4u &&
		   atmosphereWorkload.workloads[0].particleClass.shader == 1 );
	CHECK( RenderSubmission_EffectSnapshots( &state, &effectPolygons, &effectPolygonCommands, &effectVertices,
											 &effectVertexCount, &effectLights, &effectLightCount ) &&
		   effectPolygonCommands == 0u && effectVertexCount == 0u && effectLightCount == 0u );
	entity.hModel = model;
	CHECK( RenderSubmission_AddEntity( &state, &entity, &motion ) );
	entityIrradiance.schemaVersion = RAL_IRRADIANCE_ENTITY_RECEIPT_SCHEMA_VERSION;
	entityIrradiance.queryGeneration = 42u;
	entityIrradiance.entityId = 1u;
	entityIrradiance.volumeId = 7u;
	entityIrradiance.layoutHash = 8u;
	entityIrradiance.probes.schemaVersion = RAL_IRRADIANCE_RECEIPT_SCHEMA_VERSION;
	entityIrradiance.probes.queryGeneration = 42u;
	entityIrradiance.probes.productGeneration = 9u;
	entityIrradiance.probes.fallback = RAL_IRRADIANCE_FALLBACK_LIGHTGRID;
	entityIrradiance.probes.usedFallback = qtrue;
	entityIrradiance.probes.ready = qtrue;
	entityIrradiance.contributorHash = 10u;
	entityIrradiance.blendedCoefficientsQ16[0][0] = RAL_LIGHT_Q16_ONE;
	entityIrradiance.blendedCoefficientsQ16[0][1] = RAL_LIGHT_Q16_ONE;
	entityIrradiance.blendedCoefficientsQ16[0][2] = RAL_LIGHT_Q16_ONE;
	entityIrradiance.diffuseIrradianceQ16[0] = RAL_LIGHT_Q16_ONE;
	entityIrradiance.diffuseIrradianceQ16[1] = RAL_LIGHT_Q16_ONE;
	entityIrradiance.diffuseIrradianceQ16[2] = RAL_LIGHT_Q16_ONE;
	entityIrradiance.coefficientHash = Ral_IrradianceCoefficientHash(
		entityIrradiance.blendedCoefficientsQ16 );
	entityIrradiance.usedFallback = qtrue;
	entityIrradiance.ready = qtrue;
	CHECK( RenderSubmission_AttachEntityIrradiance( &state, 0u, &entityIrradiance ) );
	CHECK( state.entities[0].lightingComposition.diffuseAuthority
		== RAL_LIGHTING_DIFFUSE_AUTHORITY_LOCAL_SH );
	CHECK( state.entities[0].lightingComposition.activeTermMask == RAL_LIGHTING_TERM_LOCAL_SH );
	CHECK( !RenderSubmission_AttachEntityIrradiance( &state, 0u, &entityIrradiance ) );
	/* Match the public renderer ABI: NaN origins are rejected at ingress, while
	 * infinity is left to the variant-specific lowering path that consumes it. */
	entity.origin[0] = INFINITY;
	CHECK( RenderSubmission_AddEntity( &state, &entity, NULL ) );
	entity.origin[0] = NAN;
	CHECK( !RenderSubmission_AddEntity( &state, &entity, NULL ) );
	entity.origin[0] = 0.0f;
	CHECK( RenderSubmission_AddPoly( &state, material, 3, poly, 1 ) );
	CHECK( RenderSubmission_AddLight( &state, entity.origin, NULL, 100.0f, 1.0f, 0.5f, 0.25f ) );
	CHECK( RenderSubmission_EffectSnapshots( &state, &effectPolygons, &effectPolygonCommands, &effectVertices,
											 &effectVertexCount, &effectLights, &effectLightCount ) &&
		   effectPolygonCommands == 1u && effectVertexCount == 3u && effectPolygons[0].material == material &&
		   effectPolygons[0].verticesPerPolygon == 3u && effectPolygons[0].polygonCount == 1u && effectVertices &&
		   effectLightCount == 1u && effectLights[0].intensity == 100.0f && effectLights[0].color[1] == 0.5f &&
		   effectLights[0].hasEnd == qfalse );
	CHECK( RenderSubmission_RenderScene( &state, &view, 0 ) );
	CHECK( RenderSubmission_EffectSnapshots( &state, &effectPolygons, &effectPolygonCommands, &effectVertices,
											 &effectVertexCount, &effectLights, &effectLightCount ) &&
		   effectLightCount == 3u && RenderSubmission_AtmosphereLightCount( &state ) == 3u &&
		   RenderSubmission_AtmosphereShadowedLightCount( &state ) == 2u &&
		   effectLights[1].authoredEmissive == qtrue && effectLights[2].authoredEmissive == qtrue &&
		   effectLights[1].emissiveAuthorityHash == persistentEmissiveRoutes[0].radianceAuthorityHash &&
		   effectLights[2].emissiveAuthorityHash == persistentEmissiveRoutes[0].radianceAuthorityHash &&
		   effectLights[1].intensity == 8.0f && effectLights[2].intensity == 8.0f &&
		   effectLights[1].color[0] + effectLights[2].color[0] ==
			(float)persistentEmissiveRoutes[0].radianceQ16[0] / RAL_LIGHT_Q16_ONE &&
		   ( effectLights[1].sourceFlags & RAL_LIGHT_INJECT_ATMOSPHERE ) != 0u &&
		   ( effectLights[2].sourceFlags & RAL_LIGHT_INJECT_ATMOSPHERE ) != 0u );
	CHECK( RenderSubmission_WorldSnapshot( &state, &worldSnapshot ) && worldSnapshot.ready == qtrue &&
		   worldSnapshot.vertexCount == 3u && worldSnapshot.indexCount == 3u && worldSnapshot.indices[0] == 0u &&
		   worldSnapshot.indices[2] == 2u && worldSnapshot.fovX == view.fov_x && worldSnapshot.fovY == view.fov_y &&
		   worldSnapshot.vertices[0].normal[0] == 0.0f && worldSnapshot.vertices[0].normal[1] == 0.0f &&
		   worldSnapshot.vertices[0].normal[2] == 1.0f );
	CHECK( RenderSubmission_SetUiTransform( &state, &uiTransform ) );
	CHECK( RenderSubmission_AddUiQuad( &state, 0.0f, 0.0f, 640.0f, 360.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, material ) );
	CHECK( RenderSubmission_AddUiQuad( &state, 320.0f, 120.0f, 200.0f, 36.0f,
		0.0f, 0.0f, 1.0f, 1.0f, 0.0f, material ) );
	CHECK( RenderSubmission_SetUiTransform( &state, NULL ) );
	uiPrimitives = RenderSubmission_UiPrimitives( &state, &uiPrimitiveCount );
	CHECK( uiPrimitives != NULL && uiPrimitiveCount == 2u && uiPrimitives[0].kind == RENDER_UI_QUAD &&
		   uiPrimitives[0].width == 640.0f && uiPrimitives[0].height == 360.0f &&
		   uiPrimitives[0].material == material &&
		   fabsf( uiPrimitives[0].positions[0][0] - 32.0f ) < 0.01f &&
		   fabsf( uiPrimitives[0].positions[0][1] - 46.8f ) < 0.01f &&
		   fabsf( uiPrimitives[0].positions[1][0] - 608.0f ) < 0.01f &&
		   fabsf( uiPrimitives[0].positions[1][1] ) < 0.01f &&
		   fabsf( uiPrimitives[0].positions[2][0] - 640.0f ) < 0.01f &&
		   fabsf( uiPrimitives[0].positions[2][1] - 270.0f ) < 0.01f &&
		   fabsf( uiPrimitives[0].positions[3][0] ) < 0.01f &&
		   fabsf( uiPrimitives[0].positions[3][1] - 360.0f ) < 0.01f &&
		   uiPrimitives[1].positions[0][1] > uiPrimitives[1].positions[1][1] &&
		   uiPrimitives[1].positions[3][1] > uiPrimitives[1].positions[2][1] &&
		   fabsf( uiPrimitives[1].positions[0][0] - 320.0f ) > 0.1f &&
		   uiPrimitives[1].positions[2][1] - uiPrimitives[1].positions[1][1] <
			uiPrimitives[1].positions[3][1] - uiPrimitives[1].positions[0][1] );
	invalidMaterialLighting = materialLighting;
	invalidMaterialLighting.sourceGeneration = 18u;
	invalidMaterialLighting.provenanceHash = 0x8913u;
	invalidMaterialLighting.emissionRadianceQ16[0] = RENDER_MATERIAL_LIGHTING_Q16_ONE * 3;
	CHECK( RenderSubmission_SetMaterialLighting( &state, material, &invalidMaterialLighting ) &&
		state.emissiveAuthorityDirty == qtrue );
	CHECK( RenderSubmission_EndFrame( &state, 42u, &receipt ) );
	CHECK( state.emissiveAuthorityDirty == qfalse &&
		RenderSubmission_EmissiveAuthoritySnapshot( &state,
			&persistentEmissiveRoutes, &persistentEmissiveRouteCount,
			&persistentEmissiveProxyLights, &persistentEmissiveProxyLightCount,
			&persistentEmissiveReceipt ) &&
		persistentEmissiveRouteCount == 1u && persistentEmissiveProxyLightCount == 2u &&
		persistentEmissiveRoutes[0].radianceQ16[0] == invalidMaterialLighting.emissionRadianceQ16[0] &&
		persistentEmissiveProxyLights[0].radianceQ16[0] +
			persistentEmissiveProxyLights[1].radianceQ16[0] ==
			persistentEmissiveRoutes[0].radianceQ16[0] );
	CHECK( receipt.worldLoaded && receipt.sceneRendered );
	CHECK( receipt.worldSurfaceCount == 1u && receipt.worldVertexCount == 3u && receipt.worldIndexCount == 3u );
	CHECK( receipt.registeredAssetCount == 5u && receipt.registeredMaterialCount == 4u );
	CHECK( receipt.resolvedMaterialCount == 4u && receipt.materialBytes == 44u && receipt.materialDigest != 0u );
	CHECK( receipt.entityCount == 2u && receipt.localIrradianceEntityCount == 1u &&
		   receipt.polygonCount == 1u && receipt.lightCount == 3u &&
		   receipt.uiPrimitiveCount == 2u && receipt.atmosphereEmitterCount == 1u &&
		   receipt.atmosphereProfileCount == 2u && receipt.particleClassCount == 1u &&
		   receipt.surfaceClimateTileCount == 1u && receipt.atmosphereSurfaceEventCount == 1u &&
		   receipt.surfaceClimateEvictionCount == 0u &&
		   receipt.atmosphereMediaVolumeCount == RENDER_SUBMISSION_MAX_ATMOSPHERE_MEDIA_VOLUMES &&
		   receipt.atmosphereMediaDroppedCount == 2u && receipt.atmosphereActive == qtrue &&
		   receipt.atmosphereDigest != 0u && receipt.atmosphereGeneration != 0u &&
		   receipt.atmosphereProfileDigest != 0u && receipt.atmosphereProfileGeneration != 0u &&
		   receipt.particleClassDigest != 0u && receipt.particleClassGeneration != 0u &&
		   receipt.surfaceClimateDigest != 0u && receipt.surfaceClimateGeneration != 0u &&
		   receipt.effectPrimitiveDigest == effectPrimitiveSnapshot.digest &&
		   receipt.effectSpriteCount == 1u && receipt.effectEmitterCount == 1u &&
		   receipt.effectDecalCount == 1u && receipt.effectRibbonCount == 1u &&
		   receipt.effectRibbonPointCount == 2u &&
		   receipt.effectBeamCount == 1u &&
		   receipt.effectPrimitiveDroppedCount == 1u );
	CHECK( RenderSubmission_EffectSnapshots( &state, &effectPolygons, &effectPolygonCommands, &effectVertices,
											 &effectVertexCount, &effectLights, &effectLightCount ) &&
		   effectPolygonCommands == 1u && effectVertexCount == 3u && effectLightCount == 3u );
	exact = receipt;
	CHECK( RenderSubmission_ReceiptExact( &receipt, &exact ) );
	exact.frameDigest++;
	CHECK( !RenderSubmission_ReceiptExact( &receipt, &exact ) );
	exact = receipt;
	exact.materialDigest++;
	CHECK( !RenderSubmission_ReceiptExact( &receipt, &exact ) );
	CHECK( RenderSubmission_BeginFrame( &state, 43u ) );
	CHECK( RenderSubmission_EffectPrimitiveSnapshots( &state,
		&effectPrimitiveSnapshot, &effectSprites, &effectEmitters,
		&effectDecals, &effectRibbons, &effectRibbonPointsOut, &effectBeams )
		&& effectPrimitiveSnapshot.spriteCount == 0u
		&& effectPrimitiveSnapshot.emitterCount == 0u
		&& effectPrimitiveSnapshot.decalCount == 0u
		&& effectPrimitiveSnapshot.ribbonCount == 0u
		&& effectPrimitiveSnapshot.ribbonPointCount == 0u
		&& effectPrimitiveSnapshot.beamCount == 0u
		&& effectPrimitiveSnapshot.droppedCount == 0u );
	CHECK( RenderSubmission_AtmosphereSnapshot( &state, &atmosphereSnapshot, &atmosphereEmitters ) &&
		   atmosphereSnapshot.emitterCount == 0u );
	CHECK( RenderSubmission_AtmosphereSurfaceSnapshot( &state, &surfaceSnapshot, &surfaceTiles ) &&
		   surfaceSnapshot.frameEventCount == 0u && surfaceSnapshot.tileCount == 1u );
	CHECK( RenderSubmission_AtmosphereMediaSnapshot( &state, &mediaSnapshot, &mediaVolumes ) &&
		   mediaSnapshot.count == 0u && mediaSnapshot.droppedCount == 0u );
	{
		atmosphereFrameState_t rewindAtmosphere = atmosphere;
		for ( uint32_t i = 0u; i < RENDER_SUBMISSION_MAX_ATMOSPHERE_SURFACE_EVENTS; ++i ) {
			memset( &atmosphereSurfaceEvent, 0, sizeof( atmosphereSurfaceEvent ) );
			atmosphereSurfaceEvent.schemaVersion   = WIRED_ATMOSPHERE_SURFACE_EVENT_SCHEMA_VERSION;
			atmosphereSurfaceEvent.kind			   = ATMOSPHERE_SURFACE_EVENT_IMPACT;
			atmosphereSurfaceEvent.id			   = 1000u + i;
			atmosphereSurfaceEvent.origin[0]	   = (float)( i + 1u ) * RENDER_SUBMISSION_SURFACE_CLIMATE_TILE_SIZE;
			atmosphereSurfaceEvent.strength		   = 0.5f;
			atmosphereSurfaceEvent.timelineSeconds = 15.0f;
			CHECK( RenderSubmission_AddAtmosphereSurfaceEvent( &state, &atmosphereSurfaceEvent ) );
		}
		CHECK( RenderSubmission_AtmosphereSurfaceSnapshot( &state, &surfaceSnapshot, &surfaceTiles ) &&
			   surfaceSnapshot.tileCount == RENDER_SUBMISSION_MAX_SURFACE_CLIMATE_TILES &&
			   surfaceSnapshot.frameEventCount == RENDER_SUBMISSION_MAX_ATMOSPHERE_SURFACE_EVENTS &&
			   surfaceSnapshot.evictionCount == 1u );
		atmosphereSurfaceEvent.id = 2000u;
		CHECK( !RenderSubmission_AddAtmosphereSurfaceEvent( &state, &atmosphereSurfaceEvent ) );
		rewindAtmosphere.timelineSeconds = 14.0f;
		CHECK( RenderSubmission_SetAtmosphere( &state, &rewindAtmosphere ) );
		CHECK( RenderSubmission_AtmosphereSurfaceSnapshot( &state, &surfaceSnapshot, &surfaceTiles ) &&
			   surfaceSnapshot.tileCount == 0u && surfaceSnapshot.frameEventCount == 0u &&
			   surfaceSnapshot.evictionCount == 0u );
	}
	RenderSubmission_CancelFrame( &state );
	CHECK( RenderSubmission_UiPrimitives( &state, &uiPrimitiveCount ) == NULL );
	CHECK( !RenderSubmission_EndFrame( &state, 43u, &receipt ) );
	{
		mapFile_t  filteredWorld		= world;
		dsurface_t filteredSurfaces[2]	= { surface, surface };
		dshader_t  filteredShaders[2]	= { shader, shader };
		filteredSurfaces[1].shaderNum	= 1;
		filteredShaders[1].surfaceFlags = SURF_NODRAW;
		filteredWorld.numSurfaces		= 2;
		filteredWorld.surfaces			= filteredSurfaces;
		filteredWorld.numShaders		= 2;
		filteredWorld.shaders			= filteredShaders;
		CHECK( RenderSubmission_LoadWorld( &state, &filteredWorld, 0 ) );
		CHECK( RenderSubmission_BeginFrame( &state, 44u ) );
		CHECK( RenderSubmission_RenderScene( &state, &view, 0 ) );
		CHECK( RenderSubmission_WorldSnapshot( &state, &worldSnapshot ) && worldSnapshot.batchCount == 1u &&
			   worldSnapshot.vertexCount == 3u && worldSnapshot.indexCount == 3u );
		RenderSubmission_CancelFrame( &state );
	}
	memset( &patchWorld, 0, sizeof( patchWorld ) );
	strcpy( patchWorld.name, "maps/patch.bsp" );
	patchWorld.checksum		= 0x44556677;
	patchWorld.numSurfaces	= 1;
	patchWorld.surfaces		= &patchSurface;
	patchWorld.numDrawVerts = 9;
	patchWorld.drawVerts	= patchVertices;
	patchWorld.numShaders	= 1;
	patchWorld.shaders		= &patchShader;
	strcpy( patchShader.shader, "textures/patch" );
	patchSurface.surfaceType = MST_PATCH;
	patchSurface.shaderNum	 = 0;
	patchSurface.firstVert	 = 0;
	patchSurface.numVerts	 = 9;
	patchSurface.patchWidth	 = 3;
	patchSurface.patchHeight = 3;
	patchSurface.lightmapNum = -1;
	for ( int y = 0; y < 3; ++y ) {
		for ( int x = 0; x < 3; ++x ) {
			drawVert_t *control	 = &patchVertices[y * 3 + x];
			control->xyz[0]		 = (float)x;
			control->xyz[1]		 = (float)y;
			control->xyz[2]		 = x == 1 && y == 1 ? 8.0f : 0.0f;
			control->st[0]		 = x * 0.5f;
			control->st[1]		 = y * 0.5f;
			control->lightmap[0] = control->st[0];
			control->lightmap[1] = control->st[1];
			memset( control->color.rgba, 255, sizeof( control->color.rgba ) );
		}
	}
	patchMaterial = RenderSubmission_RegisterAsset( &state, RENDER_ASSET_MATERIAL, patchShader.shader );
	CHECK( patchMaterial > 0 );
	CHECK( RenderSubmission_LoadWorld( &state, &patchWorld, 0 ) );
	CHECK( RenderSubmission_BeginFrame( &state, 45u ) );
	CHECK( RenderSubmission_RenderScene( &state, &view, 0 ) );
	CHECK( RenderSubmission_WorldSnapshot( &state, &worldSnapshot ) && worldSnapshot.vertexCount == 25u &&
		   worldSnapshot.indexCount == 96u && worldSnapshot.batchCount == 1u && worldSnapshot.patchBatchCount == 1u &&
		   worldSnapshot.patchTriangleCount == 32u &&
		   worldSnapshot.batches[0].surfaceType == RENDER_WORLD_SURFACE_PATCH &&
		   worldSnapshot.batches[0].baseMaterial == patchMaterial && worldSnapshot.batches[0].indexCount == 96u &&
		   worldSnapshot.vertices[12].position[0] == 1.0f && worldSnapshot.vertices[12].position[1] == 1.0f &&
		   worldSnapshot.vertices[12].position[2] == 2.0f && worldSnapshot.vertices[12].texCoord[0] == 0.5f &&
		   worldSnapshot.vertices[12].lightmapCoord[1] == 0.5f );
	RenderSubmission_CancelFrame( &state );
	patchSurface.patchWidth = 4;
	CHECK( !RenderSubmission_LoadWorld( &state, &patchWorld, 0 ) );
	{
		mapFile_t secondWorld = world;
		drawVert_t secondVertices[3];
		renderWorldSnapshot_t firstSnapshot, secondSnapshot;
		uint64_t firstDigest, secondDigest;
		memcpy( secondVertices, vertices, sizeof( secondVertices ) );
		secondVertices[1].xyz[0] = 7.0f;
		secondWorld.drawVerts = secondVertices;
		secondWorld.checksum = 0x7a11ce02;
		strcpy( secondWorld.name, "maps/second-resident.bsp" );
		CHECK( RenderSubmission_LoadWorld( &state, &world, 0 ) );
		firstDigest = state.worlds[0].digest;
		CHECK( RenderSubmission_LoadWorld( &state, &secondWorld, 1 ) );
		secondDigest = state.worlds[1].digest;
		CHECK( firstDigest && secondDigest && firstDigest != secondDigest );
		CHECK( RenderSubmission_WorldResident( &state, 0 )
			&& RenderSubmission_WorldResident( &state, 1 ) );
		CHECK( RenderSubmission_BeginFrame( &state, 46u )
			&& RenderSubmission_RenderScene( &state, &view, 0 )
			&& RenderSubmission_WorldSnapshot( &state, &firstSnapshot )
			&& firstSnapshot.vertices[1].position[0] == vertices[1].xyz[0] );
		RenderSubmission_CancelFrame( &state );
		CHECK( RenderSubmission_BeginFrame( &state, 47u )
			&& RenderSubmission_RenderScene( &state, &view, 1 )
			&& RenderSubmission_WorldSnapshot( &state, &secondSnapshot )
			&& secondSnapshot.vertices[1].position[0] == 7.0f );
		RenderSubmission_CancelFrame( &state );
		CHECK( firstSnapshot.vertices != secondSnapshot.vertices );
		CHECK( RenderSubmission_UnloadWorld( &state, 0 )
			&& !RenderSubmission_WorldResident( &state, 0 )
			&& RenderSubmission_WorldResident( &state, 1 )
			&& RenderSubmission_SelectWorld( &state, 1 )
			&& state.worldDigest == secondDigest );
		CHECK( RenderSubmission_UnloadWorld( &state, 1 )
			&& !state.worldLoaded && state.activeWorldIndex == -1
			&& state.irradianceVolumeDigest != 0u
			&& state.directionalLightingDigest != 0u );
		CHECK( RenderSubmission_BeginFrame( &state, 48u )
			&& RenderSubmission_EndFrame( &state, 48u, &receipt )
			&& !receipt.worldLoaded );
	}
	{
		const uint64_t profileGeneration = state.atmosphereProfileGeneration;
		const uint64_t classGeneration = state.particleClassGeneration;
		CHECK( RenderSubmission_ResetEffectRegistries( &state ) );
		CHECK( state.atmosphereProfileCount == 0u && state.particleClassCount == 0u );
		CHECK( state.atmosphereProfileGeneration == profileGeneration + 1u &&
			   state.particleClassGeneration == classGeneration + 1u );
		CHECK( !RenderSubmission_GetAtmosphereEffectProfile( &state, 1u, &resolvedProfile ) );
		CHECK( !RenderSubmission_GetParticleClass( &state, 1, &resolvedParticleClass ) );
		atmosphereParticleClass.sizeEnd = 10.0f;
		CHECK( RenderSubmission_RegisterParticleClass( &state, 1, &atmosphereParticleClass ) );
		CHECK( RenderSubmission_RegisterAtmosphereEffectProfile( &state, 1u, &atmosphereProfile ) );
	}
	RenderSubmission_Reset( &state );
	puts( "render frontend submission: PASS" );
	return 0;
}
