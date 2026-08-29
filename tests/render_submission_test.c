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
	s_materialScriptText = validScript;
	CHECK( RenderMaterialScript_Load( &catalog, &imports ) );
	CHECK( catalog.ready && catalog.count == 6u );
	CHECK( RenderMaterialScript_Lookup( &catalog, "textures/test/lava", &entry ) );
	CHECK( entry.hasLighting && !strcmp( entry.imageName, "textures/test/lava_d" )
		&& entry.cullMode == RENDER_CULL_NONE && entry.sortExplicit
		&& fabsf( entry.sort - 4.25f ) < 0.0001f );
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
	CHECK( RenderSubmission_BeginFrame( &state, 56u ) );
	CHECK( RenderSubmission_AddEntity( &state, &entity, NULL ) );
	CHECK( RenderSubmission_AttachLegacyLightGridEntityIrradiance(
		&state, 0u, 0.6f, 1.0f ) );
	commands = RenderSubmission_EntityCommands( &state, &commandCount );
	CHECK( commands && commandCount == 1u && commands[0].hasLocalIrradiance
		&& state.localIrradianceEntityCount == 1u
		&& commands[0].localIrradiance.blendedCoefficientsQ16[0][0] > 0
		&& commands[0].localIrradiance.blendedCoefficientsQ16[3][2] > 0
		&& commands[0].localIrradiance.blendedCoefficientsQ16[1][0] == 0
		&& commands[0].localIrradiance.blendedCoefficientsQ16[2][1] == 0 );
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
	uint8_t coefficients[8u * 24u] = { 0 }, validity[8], artifact[512], corrupt[512];
	uint8_t sidecar[1024], corruptSidecar[1024];
	uint8_t directionalRadiance[16] = { 0 }, directionalDirection[8] = { 0 }, directionalArtifact[512];
	refEntity_t entity;
	refdef_t view;
	renderSubmissionReceipt_t receipt;
	const renderIrradianceVolumeRecord_t *volumeSnapshot;
	uint32_t volumeCount;
	uint64_t volumeDigest;
	uint64_t sidecarLength, sidecarManifest, loadedDigest;
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
	arrays[0].offset		   = iqmHeader->ofsVertexArrays + 2u * sizeof( *arrays );
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
	CHECK( iqmModel > model && RenderSubmission_ModelSnapshot( &state, iqmModel, &snapshot ) &&
		   snapshot.format == RENDER_MODEL_IQM && snapshot.frameCount == 1u && snapshot.vertexCount == 3u &&
		   snapshot.indexCount == 3u && snapshot.batchCount == 1u &&
		   !strcmp( snapshot.batches[0].materialName, "textures/iqm" ) );
	CHECK( RenderSubmission_RegisterModelData( &state, "models/bad.md3", md3, sizeof( md3Header_t ) - 1u ) == 0 );
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

int main( int argc, char **argv )
{
	if ( argc == 3 && !strcmp( argv[1], "--write-irradiance-fixture" ) )
		return WriteIrradianceFixture( argv[2], 1808, -868, -64 );
	if ( argc == 6 && !strcmp( argv[1], "--write-irradiance-fixture" ) )
		return WriteIrradianceFixture( argv[2], atoi( argv[3] ), atoi( argv[4] ), atoi( argv[5] ) );
	CHECK( TestLightingSidecar() == 0 );
	CHECK( TestMaterialScriptLighting() == 0 );
	CHECK( TestLegacyLightGridIrradiance() == 0 );
	CHECK( TestIrradianceVolumeOwner() == 0 );
	CHECK( TestModelPayloads() == 0 );
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
	renderEffectPrimitiveSnapshot_t effectPrimitiveSnapshot;
	const spriteDesc_t *effectSprites;
	const emitterDesc_t *effectEmitters;
	const decalDesc_t *effectDecals;
	const renderEffectRibbonCommand_t *effectRibbons;
	const ribbonPoint_t *effectRibbonPointsOut;
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
		   atmosphereSnapshot.emitterCount == 0u );
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
	legacyAtmosphere.type	  = ATMOSPHERE_PRECIP_SNOW;
	legacyAtmosphere.distance = 900.0f;
	CHECK( RenderSubmission_SetAtmosphere( &state, &legacyAtmosphere ) );
	CHECK( RenderSubmission_AtmosphereSnapshot( &state, &atmosphereSnapshot, &atmosphereEmitters ) &&
		   atmosphereSnapshot.state.schemaVersion == WIRED_ATMOSPHERE_SCHEMA_VERSION &&
		   atmosphereSnapshot.state.qualityTier == ATMOSPHERE_QUALITY_WEATHER &&
		   atmosphereSnapshot.state.precipitation[1] == 1.0f );
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
	effectSprite.radius = 0.0f;
	CHECK( !RenderSubmission_AddEffectSprite( &state, &effectSprite ) );
	CHECK( RenderSubmission_EffectPrimitiveSnapshots( &state,
		&effectPrimitiveSnapshot, &effectSprites, &effectEmitters,
		&effectDecals, &effectRibbons, &effectRibbonPointsOut )
		&& effectPrimitiveSnapshot.digest != 0u
		&& effectPrimitiveSnapshot.spriteCount == 1u
		&& effectPrimitiveSnapshot.emitterCount == 1u
		&& effectPrimitiveSnapshot.decalCount == 1u
		&& effectPrimitiveSnapshot.ribbonCount == 1u
		&& effectPrimitiveSnapshot.ribbonPointCount == 2u
		&& effectPrimitiveSnapshot.droppedCount == 1u
		&& effectSprites[0].radius == 8.0f
		&& effectEmitters[0].count == 8
		&& effectDecals[0].lifetime == 4.0f
		&& effectRibbons[0].firstPoint == 0u
		&& effectRibbons[0].pointCount == 2u
		&& effectRibbonPointsOut[1].pos[0] == 12.0f );
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
		&effectDecals, &effectRibbons, &effectRibbonPointsOut )
		&& effectPrimitiveSnapshot.spriteCount == 0u
		&& effectPrimitiveSnapshot.emitterCount == 0u
		&& effectPrimitiveSnapshot.decalCount == 0u
		&& effectPrimitiveSnapshot.ribbonCount == 0u
		&& effectPrimitiveSnapshot.ribbonPointCount == 0u
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
	RenderSubmission_Reset( &state );
	puts( "render frontend submission: PASS" );
	return 0;
}
