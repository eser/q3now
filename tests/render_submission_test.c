// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "render_submission.h"
#include "maps/map_format_registry.h"
#include "qfiles.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #x); return 1; } } while (0)

typedef struct {
	char magic[16]; uint32_t version, filesize, flags;
	uint32_t numText, ofsText, numMeshes, ofsMeshes;
	uint32_t numVertexArrays, numVertexes, ofsVertexArrays;
	uint32_t numTriangles, ofsTriangles, ofsAdjacency;
	uint32_t numJoints, ofsJoints, numPoses, ofsPoses, numAnims, ofsAnims;
	uint32_t numFrames, numFrameChannels, ofsFrames, ofsBounds;
	uint32_t numComment, ofsComment, numExtensions, ofsExtensions;
} testIqmHeader_t;
typedef struct { uint32_t name, material, firstVertex, numVertexes,
	firstTriangle, numTriangles; } testIqmMesh_t;
typedef struct { uint32_t type, flags, format, size, offset; } testIqmArray_t;

static int TestModelPayloads( void ) {
	renderSubmissionState_t state;
	renderModelSnapshot_t snapshot;
	byte md3[2048] = { 0 }, iqm[512] = { 0 };
	md3Header_t *header = (md3Header_t *)md3;
	md3Surface_t *surface = (md3Surface_t *)( md3 + sizeof( *header ) );
	uint32_t offset = sizeof( *surface );
	md3Triangle_t *triangle; md3Shader_t *shader; md3St_t *st;
	md3XyzNormal_t *xyz;
	header->ident = MD3_IDENT; header->version = MD3_VERSION;
	header->numFrames = 2; header->numSurfaces = 1;
	header->ofsSurfaces = sizeof( *header );
	surface->ident = MD3_IDENT; surface->numFrames = 2;
	surface->numShaders = 1; surface->numVerts = 3; surface->numTriangles = 1;
	surface->ofsTriangles = offset; triangle = (md3Triangle_t *)( (byte *)surface + offset );
	triangle->indexes[0] = 0u; triangle->indexes[1] = 1u; triangle->indexes[2] = 2u;
	offset += sizeof( *triangle ); surface->ofsShaders = offset;
	shader = (md3Shader_t *)( (byte *)surface + offset );
	strcpy( shader->name, "textures/model" ); offset += sizeof( *shader );
	surface->ofsSt = offset; st = (md3St_t *)( (byte *)surface + offset );
	st[1].st[0] = 1.0f; st[2].st[1] = 1.0f; offset += 3u * sizeof( *st );
	surface->ofsXyzNormals = offset; xyz = (md3XyzNormal_t *)( (byte *)surface + offset );
	xyz[1].xyz[0] = 64; xyz[2].xyz[1] = 64;
	xyz[3].xyz[2] = 64; xyz[4].xyz[0] = 64; xyz[5].xyz[1] = 64;
	offset += 6u * sizeof( *xyz ); surface->ofsEnd = offset;
	header->ofsEnd = header->ofsSurfaces + offset;
	CHECK( RenderSubmission_Init( &state, 51u ) );
	qhandle_t model = RenderSubmission_RegisterModelData( &state,
		"models/test.md3", md3, header->ofsEnd );
	CHECK( model > 0 && RenderSubmission_ModelSnapshot( &state, model, &snapshot )
		&& snapshot.format == RENDER_MODEL_MD3 && snapshot.frameCount == 2u
		&& snapshot.vertexCount == 3u && snapshot.indexCount == 3u
		&& snapshot.batchCount == 1u && snapshot.positions[9u + 2u] == 1.0f
		&& !strcmp( snapshot.batches[0].materialName, "textures/model" ) );
	qhandle_t material = RenderSubmission_RegisterAsset( &state,
		RENDER_ASSET_MATERIAL, "textures/model" );
	CHECK( material > 0 && RenderSubmission_SetModelBatchMaterial( &state,
		model, 0u, material ) && RenderSubmission_ModelSnapshot( &state, model,
		&snapshot ) && snapshot.batches[0].material == material );

	testIqmHeader_t *iqmHeader = (testIqmHeader_t *)iqm;
	memcpy( iqmHeader->magic, "INTERQUAKEMODEL", 16u );
	iqmHeader->version = 2u; iqmHeader->numText = 16u;
	iqmHeader->ofsText = sizeof( *iqmHeader );
	memcpy( iqm + iqmHeader->ofsText, "textures/iqm\0", 13u );
	iqmHeader->numMeshes = 1u; iqmHeader->ofsMeshes = iqmHeader->ofsText + 16u;
	testIqmMesh_t *mesh = (testIqmMesh_t *)( iqm + iqmHeader->ofsMeshes );
	mesh->material = 0u; mesh->numVertexes = 3u; mesh->numTriangles = 1u;
	iqmHeader->numVertexArrays = 2u; iqmHeader->numVertexes = 3u;
	iqmHeader->ofsVertexArrays = iqmHeader->ofsMeshes + sizeof( *mesh );
	testIqmArray_t *arrays = (testIqmArray_t *)( iqm + iqmHeader->ofsVertexArrays );
	arrays[0].type = 0u; arrays[0].format = 7u; arrays[0].size = 3u;
	arrays[0].offset = iqmHeader->ofsVertexArrays + 2u * sizeof( *arrays );
	float *positions = (float *)( iqm + arrays[0].offset ); positions[3] = 1.0f; positions[7] = 1.0f;
	arrays[1].type = 1u; arrays[1].format = 7u; arrays[1].size = 2u;
	arrays[1].offset = arrays[0].offset + 9u * sizeof( float );
	float *texCoords = (float *)( iqm + arrays[1].offset ); texCoords[2] = 1.0f; texCoords[5] = 1.0f;
	iqmHeader->numTriangles = 1u;
	iqmHeader->ofsTriangles = arrays[1].offset + 6u * sizeof( float );
	uint32_t *iqmTriangle = (uint32_t *)( iqm + iqmHeader->ofsTriangles );
	iqmTriangle[0] = 0u; iqmTriangle[1] = 1u; iqmTriangle[2] = 2u;
	iqmHeader->filesize = iqmHeader->ofsTriangles + 3u * sizeof( uint32_t );
	qhandle_t iqmModel = RenderSubmission_RegisterModelData( &state,
		"models/test.iqm", iqm, iqmHeader->filesize );
	CHECK( iqmModel > model && RenderSubmission_ModelSnapshot( &state, iqmModel,
		&snapshot ) && snapshot.format == RENDER_MODEL_IQM
		&& snapshot.frameCount == 1u && snapshot.vertexCount == 3u
		&& snapshot.indexCount == 3u && snapshot.batchCount == 1u
		&& !strcmp( snapshot.batches[0].materialName, "textures/iqm" ) );
	CHECK( RenderSubmission_RegisterModelData( &state, "models/bad.md3",
		md3, sizeof( md3Header_t ) - 1u ) == 0 );
	RenderSubmission_Reset( &state );
	return 0;
}

int main( void ) {
	CHECK( TestModelPayloads() == 0 );
	renderSubmissionState_t state;
	renderSubmissionReceipt_t receipt, exact;
	mapFile_t world;
	dsurface_t surface = { 0 };
	drawVert_t vertices[3] = { 0 };
	int indices[3] = { 0, 1, 2 };
	dshader_t shader = { 0 };
	refEntity_t entity;
	refEntityMotion_t motion = { sizeof( motion ), REF_ENTITY_MOTION_VERSION, 7u, 1u,
		REF_ENTITY_MOTION_ROLE_GENERAL, 0u };
	refdef_t view;
	polyVert_t poly[3] = { 0 };
	qhandle_t model, material, pendingMaterial;
	const byte materialPixels[16] = {
		255u, 0u, 0u, 255u, 0u, 255u, 0u, 255u,
		0u, 0u, 255u, 255u, 255u, 255u, 255u, 255u
	};
	renderMaterialSnapshot_t materialSnapshot;
	uint64_t materialGeneration;
	const renderUiPrimitive_t *uiPrimitives;
	renderWorldSnapshot_t worldSnapshot;
	mapFile_t patchWorld;
	dsurface_t patchSurface = { 0 };
	drawVert_t patchVertices[9] = { 0 };
	dshader_t patchShader = { 0 };
	qhandle_t patchMaterial;
	uint32_t uiPrimitiveCount = 0u;
	const renderPolyCommand_t *effectPolygons;
	const polyVert_t *effectVertices;
	const renderLightCommand_t *effectLights;
	uint32_t effectPolygonCommands, effectVertexCount, effectLightCount;
	memset( &world, 0, sizeof( world ) );
	strcpy( world.name, "maps/arena1.bsp" );
	world.checksum = 0x12345678;
	world.numSurfaces = 1; world.surfaces = &surface;
	world.numDrawVerts = 3; world.drawVerts = vertices;
	world.numDrawIndexes = 3; world.drawIndexes = indices;
	world.numShaders = 1; world.shaders = &shader;
	surface.surfaceType = MST_PLANAR;
	surface.firstVert = 0; surface.numVerts = 3;
	surface.firstIndex = 0; surface.numIndexes = 3;
	memset( &entity, 0, sizeof( entity ) ); entity.reType = RT_MODEL;
	memset( &view, 0, sizeof( view ) ); view.width = 1280; view.height = 720;
	view.fov_x = 90.0f; view.fov_y = 60.0f;
	view.viewaxis[0][0] = 1.0f; view.viewaxis[1][1] = 1.0f;
	view.viewaxis[2][2] = 1.0f;
	CHECK( RenderSubmission_Init( &state, 41u ) );
	model = RenderSubmission_RegisterAsset( &state, RENDER_ASSET_MODEL, "models/a.md3" );
	pendingMaterial = RenderSubmission_RegisterAsset( &state,
		RENDER_ASSET_MSDF, "textures/pending" );
	CHECK( pendingMaterial > model );
	CHECK( RenderSubmission_MaterialSnapshot( &state, pendingMaterial,
		&materialSnapshot ) && materialSnapshot.ready == qfalse );
	{
		const uint64_t pendingGeneration = materialSnapshot.generation;
		CHECK( RenderSubmission_RegisterMaterialImage( &state,
			RENDER_ASSET_MSDF, "textures/PENDING", qtrue,
			materialPixels, 2u, 2u ) == pendingMaterial );
		CHECK( RenderSubmission_MaterialSnapshot( &state, pendingMaterial,
			&materialSnapshot ) && materialSnapshot.ready == qtrue
			&& materialSnapshot.generation > pendingGeneration
			&& materialSnapshot.msdf == qtrue && materialSnapshot.srgb == qfalse );
	}
	material = RenderSubmission_RegisterMaterialImage( &state,
		RENDER_ASSET_MATERIAL, "textures/a", qtrue, materialPixels, 2u, 2u );
	CHECK( model > 0 && material > model );
	CHECK( RenderSubmission_MaterialSnapshot( &state, material, &materialSnapshot )
		&& materialSnapshot.ready == qtrue && materialSnapshot.width == 2u
		&& materialSnapshot.height == 2u && materialSnapshot.byteCount == 16u
		&& materialSnapshot.clampToEdge == qtrue
		&& materialSnapshot.msdf == qfalse && materialSnapshot.srgb == qtrue
		&& materialSnapshot.alphaMode == RENDER_ALPHA_OPAQUE
		&& materialSnapshot.depthWrite == qtrue
		&& !memcmp( materialSnapshot.rgba8, materialPixels, sizeof( materialPixels ) ) );
	materialGeneration = materialSnapshot.generation;
	CHECK( RenderSubmission_SetMaterialRasterPolicy( &state, material,
		RENDER_ALPHA_BLEND, 0.25f, qfalse ) );
	CHECK( RenderSubmission_MaterialSnapshot( &state, material, &materialSnapshot )
		&& materialSnapshot.generation > materialGeneration
		&& materialSnapshot.alphaMode == RENDER_ALPHA_BLEND
		&& materialSnapshot.alphaCutoff == 0.25f
		&& materialSnapshot.depthWrite == qfalse );
	CHECK( !RenderSubmission_SetMaterialRasterPolicy( &state, material,
		RENDER_ALPHA_BLEND, NAN, qfalse ) );
	CHECK( RenderSubmission_LoadWorld( &state, &world, 0 ) );
	CHECK( RenderSubmission_ClearScene( &state ) );
	CHECK( RenderSubmission_BeginFrame( &state, 42u ) );
	CHECK( RenderSubmission_AddPoly( &state, material, 3, poly, 1 ) );
	CHECK( RenderSubmission_AddLight( &state, entity.origin, NULL,
		100.0f, 1.0f, 0.5f, 0.25f ) );
	CHECK( RenderSubmission_ClearScene( &state ) );
	CHECK( RenderSubmission_EffectSnapshots( &state, &effectPolygons,
		&effectPolygonCommands, &effectVertices, &effectVertexCount,
		&effectLights, &effectLightCount )
		&& effectPolygonCommands == 0u && effectVertexCount == 0u
		&& effectLightCount == 0u );
	entity.hModel = model;
	CHECK( RenderSubmission_AddEntity( &state, &entity, &motion ) );
	/* Match the public renderer ABI: NaN origins are rejected at ingress, while
	 * infinity is left to the variant-specific lowering path that consumes it. */
	entity.origin[0] = INFINITY;
	CHECK( RenderSubmission_AddEntity( &state, &entity, NULL ) );
	entity.origin[0] = NAN;
	CHECK( !RenderSubmission_AddEntity( &state, &entity, NULL ) );
	entity.origin[0] = 0.0f;
	CHECK( RenderSubmission_AddPoly( &state, material, 3, poly, 1 ) );
	CHECK( RenderSubmission_AddLight( &state, entity.origin, NULL, 100.0f, 1.0f, 0.5f, 0.25f ) );
	CHECK( RenderSubmission_EffectSnapshots( &state, &effectPolygons,
		&effectPolygonCommands, &effectVertices, &effectVertexCount,
		&effectLights, &effectLightCount )
		&& effectPolygonCommands == 1u && effectVertexCount == 3u
		&& effectPolygons[0].material == material
		&& effectPolygons[0].verticesPerPolygon == 3u
		&& effectPolygons[0].polygonCount == 1u
		&& effectVertices && effectLightCount == 1u
		&& effectLights[0].intensity == 100.0f
		&& effectLights[0].color[1] == 0.5f
		&& effectLights[0].hasEnd == qfalse );
	CHECK( RenderSubmission_RenderScene( &state, &view, 0 ) );
	CHECK( RenderSubmission_WorldSnapshot( &state, &worldSnapshot )
		&& worldSnapshot.ready == qtrue
		&& worldSnapshot.vertexCount == 3u && worldSnapshot.indexCount == 3u
		&& worldSnapshot.indices[0] == 0u && worldSnapshot.indices[2] == 2u
		&& worldSnapshot.fovX == view.fov_x && worldSnapshot.fovY == view.fov_y );
	CHECK( RenderSubmission_AddUiQuad( &state, 0.0f, 0.0f, 640.0f, 360.0f,
		0.0f, 0.0f, 1.0f, 1.0f, 0.0f, material ) );
	uiPrimitives = RenderSubmission_UiPrimitives( &state, &uiPrimitiveCount );
	CHECK( uiPrimitives != NULL && uiPrimitiveCount == 1u
		&& uiPrimitives[0].kind == RENDER_UI_QUAD
		&& uiPrimitives[0].width == 640.0f
		&& uiPrimitives[0].height == 360.0f
		&& uiPrimitives[0].material == material );
	CHECK( RenderSubmission_EndFrame( &state, 42u, &receipt ) );
	CHECK( receipt.worldLoaded && receipt.sceneRendered );
	CHECK( receipt.worldSurfaceCount == 1u && receipt.worldVertexCount == 3u
		&& receipt.worldIndexCount == 3u );
	CHECK( receipt.registeredAssetCount == 3u && receipt.registeredMaterialCount == 2u );
	CHECK( receipt.resolvedMaterialCount == 2u && receipt.materialBytes == 32u
		&& receipt.materialDigest != 0u );
	CHECK( receipt.entityCount == 2u && receipt.polygonCount == 1u
		&& receipt.lightCount == 1u && receipt.uiPrimitiveCount == 1u );
	CHECK( RenderSubmission_EffectSnapshots( &state, &effectPolygons,
		&effectPolygonCommands, &effectVertices, &effectVertexCount,
		&effectLights, &effectLightCount )
		&& effectPolygonCommands == 1u && effectVertexCount == 3u
		&& effectLightCount == 1u );
	exact = receipt;
	CHECK( RenderSubmission_ReceiptExact( &receipt, &exact ) );
	exact.frameDigest++;
	CHECK( !RenderSubmission_ReceiptExact( &receipt, &exact ) );
	exact = receipt;
	exact.materialDigest++;
	CHECK( !RenderSubmission_ReceiptExact( &receipt, &exact ) );
	CHECK( RenderSubmission_BeginFrame( &state, 43u ) );
	RenderSubmission_CancelFrame( &state );
	CHECK( RenderSubmission_UiPrimitives( &state, &uiPrimitiveCount ) == NULL );
	CHECK( !RenderSubmission_EndFrame( &state, 43u, &receipt ) );
	{
		mapFile_t filteredWorld = world;
		dsurface_t filteredSurfaces[2] = { surface, surface };
		dshader_t filteredShaders[2] = { shader, shader };
		filteredSurfaces[1].shaderNum = 1;
		filteredShaders[1].surfaceFlags = SURF_NODRAW;
		filteredWorld.numSurfaces = 2; filteredWorld.surfaces = filteredSurfaces;
		filteredWorld.numShaders = 2; filteredWorld.shaders = filteredShaders;
		CHECK( RenderSubmission_LoadWorld( &state, &filteredWorld, 0 ) );
		CHECK( RenderSubmission_BeginFrame( &state, 44u ) );
		CHECK( RenderSubmission_RenderScene( &state, &view, 0 ) );
		CHECK( RenderSubmission_WorldSnapshot( &state, &worldSnapshot )
			&& worldSnapshot.batchCount == 1u
			&& worldSnapshot.vertexCount == 3u
			&& worldSnapshot.indexCount == 3u );
		RenderSubmission_CancelFrame( &state );
	}
	memset( &patchWorld, 0, sizeof( patchWorld ) );
	strcpy( patchWorld.name, "maps/patch.bsp" );
	patchWorld.checksum = 0x44556677;
	patchWorld.numSurfaces = 1; patchWorld.surfaces = &patchSurface;
	patchWorld.numDrawVerts = 9; patchWorld.drawVerts = patchVertices;
	patchWorld.numShaders = 1; patchWorld.shaders = &patchShader;
	strcpy( patchShader.shader, "textures/patch" );
	patchSurface.surfaceType = MST_PATCH;
	patchSurface.shaderNum = 0;
	patchSurface.firstVert = 0; patchSurface.numVerts = 9;
	patchSurface.patchWidth = 3; patchSurface.patchHeight = 3;
	patchSurface.lightmapNum = -1;
	for ( int y = 0; y < 3; ++y ) {
		for ( int x = 0; x < 3; ++x ) {
			drawVert_t *control = &patchVertices[y * 3 + x];
			control->xyz[0] = (float)x; control->xyz[1] = (float)y;
			control->xyz[2] = x == 1 && y == 1 ? 8.0f : 0.0f;
			control->st[0] = x * 0.5f; control->st[1] = y * 0.5f;
			control->lightmap[0] = control->st[0];
			control->lightmap[1] = control->st[1];
			memset( control->color.rgba, 255, sizeof( control->color.rgba ) );
		}
	}
	patchMaterial = RenderSubmission_RegisterAsset( &state,
		RENDER_ASSET_MATERIAL, patchShader.shader );
	CHECK( patchMaterial > 0 );
	CHECK( RenderSubmission_LoadWorld( &state, &patchWorld, 0 ) );
	CHECK( RenderSubmission_BeginFrame( &state, 45u ) );
	CHECK( RenderSubmission_RenderScene( &state, &view, 0 ) );
	CHECK( RenderSubmission_WorldSnapshot( &state, &worldSnapshot )
		&& worldSnapshot.vertexCount == 25u && worldSnapshot.indexCount == 96u
		&& worldSnapshot.batchCount == 1u && worldSnapshot.patchBatchCount == 1u
		&& worldSnapshot.patchTriangleCount == 32u
		&& worldSnapshot.batches[0].surfaceType == RENDER_WORLD_SURFACE_PATCH
		&& worldSnapshot.batches[0].baseMaterial == patchMaterial
		&& worldSnapshot.batches[0].indexCount == 96u
		&& worldSnapshot.vertices[12].position[0] == 1.0f
		&& worldSnapshot.vertices[12].position[1] == 1.0f
		&& worldSnapshot.vertices[12].position[2] == 2.0f
		&& worldSnapshot.vertices[12].texCoord[0] == 0.5f
		&& worldSnapshot.vertices[12].lightmapCoord[1] == 0.5f );
	RenderSubmission_CancelFrame( &state );
	patchSurface.patchWidth = 4;
	CHECK( !RenderSubmission_LoadWorld( &state, &patchWorld, 0 ) );
	RenderSubmission_Reset( &state );
	puts( "render frontend submission: PASS" );
	return 0;
}
