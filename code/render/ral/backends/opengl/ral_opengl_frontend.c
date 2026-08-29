// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "ral_opengl_frontend.h"

#include <limits.h>
#include <math.h>
#include <string.h>

#define PLAN_FNV_OFFSET UINT64_C(1469598103934665603)
#define PLAN_FNV_PRIME UINT64_C(1099511628211)

static uint64_t HashU64( uint64_t digest, uint64_t value ) {
	for ( uint32_t i = 0u; i < 8u; ++i ) {
		digest ^= ( value >> ( i * 8u ) ) & 0xffu;
		digest *= PLAN_FNV_PRIME;
	}
	return digest;
}

static qboolean AddU32( uint32_t *value, uint32_t amount ) {
	if ( !value || amount > UINT32_MAX - *value ) return qfalse;
	*value += amount;
	return qtrue;
}

static qboolean MaterialReady( const renderSubmissionState_t *frontend,
		qhandle_t handle, renderMaterialSnapshot_t *out ) {
	renderMaterialSnapshot_t snapshot;
	if ( handle <= 0 || !RenderSubmission_MaterialSnapshot( frontend, handle,
			&snapshot ) || snapshot.ready != qtrue || !snapshot.rgba8
			|| !snapshot.byteCount || !snapshot.width || !snapshot.height ) return qfalse;
	if ( out ) *out = snapshot;
	return qtrue;
}

static qboolean ReceiptValid( const ralOpenGlFrontendPlanReceipt_t *receipt ) {
	return receipt
		&& receipt->schemaVersion == RAL_OPENGL_FRONTEND_SCHEMA_VERSION
		&& receipt->backendType == RAL_BACKEND_OPENGL
		&& receipt->coreGeneration != 0u
		&& receipt->coreGeneration != UINT64_MAX
		&& receipt->planGeneration != 0u
		&& receipt->planGeneration != UINT64_MAX
		&& receipt->frontendOwnerGeneration != 0u
		&& receipt->frontendFrameGeneration != 0u
		&& receipt->frontendFrameDigest != 0u
		&& receipt->loweringDigest != 0u
		&& receipt->unresolvedCount == 0u
		&& receipt->fallbackCount == 0u
		&& receipt->fatalCount == 0u
		&& receipt->ready == qtrue;
}

qboolean RalOpenGl_FrontendPlanReceiptExact(
		const ralOpenGlFrontendPlanReceipt_t *a,
		const ralOpenGlFrontendPlanReceipt_t *b ) {
	return ReceiptValid( a ) && ReceiptValid( b )
		&& !memcmp( a, b, sizeof( *a ) );
}

qboolean RalOpenGl_FrontendPlanBuild(
		const ralOpenGlCoreReceipt_t *coreReceipt,
		const renderSubmissionState_t *frontend,
		const renderSubmissionReceipt_t *submission, uint64_t planGeneration,
		ralOpenGlFrontendPlanReceipt_t *outReceipt ) {
	ralOpenGlFrontendPlanReceipt_t candidate;
	renderWorldSnapshot_t world;
	const renderEntityCommand_t *entities;
	const renderUiPrimitive_t *ui;
	const renderPolyCommand_t *polygons;
	const polyVert_t *polyVertices;
	const renderLightCommand_t *lights;
	uint32_t polygonCommandCount = 0u, polyVertexCount = 0u, lightCount = 0u;
	uint32_t entityCount = 0u, uiCount = 0u;
	if ( !coreReceipt || !frontend || !submission || !outReceipt
			|| !RalOpenGl_CoreReceiptExact( coreReceipt, coreReceipt )
			|| !RenderSubmission_ReceiptExact( submission, submission )
			|| planGeneration == 0u || planGeneration == UINT64_MAX
			|| !frontend->initialized || frontend->frameOpen
			|| frontend->frameSealed != qtrue
			|| frontend->ownerGeneration != submission->ownerGeneration
			|| frontend->assetDigest != submission->assetDigest
			|| frontend->materialDigest != submission->materialDigest
			|| frontend->modelDigest != submission->modelDigest
			|| frontend->worldDigest != submission->worldDigest
			|| frontend->sceneDigest != submission->sceneDigest
			|| frontend->uiDigest != submission->uiDigest
			|| RenderSubmission_FrameDigest( frontend )
				!= submission->frameDigest
			|| frontend->entityCount != submission->entityCount
			|| frontend->temporalEntityCount != submission->temporalEntityCount
			|| frontend->localIrradianceEntityCount != submission->localIrradianceEntityCount
			|| frontend->polygonCount != submission->polygonCount
			|| frontend->lightCount != submission->lightCount
			|| frontend->uiPrimitiveCount != submission->uiPrimitiveCount ) return qfalse;
	memset( &candidate, 0, sizeof( candidate ) );
	candidate.schemaVersion = RAL_OPENGL_FRONTEND_SCHEMA_VERSION;
	candidate.backendType = RAL_BACKEND_OPENGL;
	candidate.coreGeneration = coreReceipt->generation;
	candidate.planGeneration = planGeneration;
	candidate.frontendOwnerGeneration = submission->ownerGeneration;
	candidate.frontendFrameGeneration = submission->frameGeneration;
	candidate.frontendFrameDigest = submission->frameDigest;
	candidate.submittedPolygonCount = submission->polygonCount;
	candidate.submittedLightCount = submission->lightCount;
	for ( uint32_t i = 0u; i < frontend->materialCount; ++i ) {
		if ( frontend->materials[i].snapshot.ready != qtrue
				|| !frontend->materials[i].snapshot.rgba8 ) candidate.unresolvedCount++;
		else candidate.loweredMaterialCount++;
	}
	if ( candidate.loweredMaterialCount != submission->resolvedMaterialCount )
		candidate.unresolvedCount++;
	if ( submission->worldLoaded && submission->sceneRendered ) {
		if ( !RenderSubmission_WorldSnapshot( frontend, &world ) ) {
			candidate.unresolvedCount++;
		} else {
			candidate.loweredWorldVertexCount = world.vertexCount;
			candidate.loweredWorldIndexCount = world.indexCount;
			candidate.loweredWorldBatchCount = world.batchCount;
			candidate.patchWorldBatchCount = world.patchBatchCount;
			for ( uint32_t i = 0u; i < world.batchCount; ++i ) {
				const renderWorldBatch_t *batch = &world.batches[i];
				if ( MaterialReady( frontend, batch->baseMaterial, NULL ) )
					candidate.texturedWorldBatchCount++;
				else candidate.unresolvedCount++;
				if ( batch->lightmapMaterial > 0 ) {
					if ( MaterialReady( frontend, batch->lightmapMaterial, NULL ) )
						candidate.lightmappedWorldBatchCount++;
					else candidate.unresolvedCount++;
				}
			}
		}
	}
	entities = RenderSubmission_EntityCommands( frontend, &entityCount );
	if ( entityCount != submission->entityCount
			|| ( entityCount && !entities ) ) candidate.unresolvedCount++;
	for ( uint32_t i = 0u; entities && i < entityCount; ++i ) {
		const refEntity_t *entity = &entities[i].entity;
		if ( entities[i].hasTemporal ) candidate.loweredTemporalEntityCount++;
		if ( entities[i].hasLocalIrradiance ) {
			if ( !Ral_IrradianceEntitySampleReceiptValid( &entities[i].localIrradiance )
					|| !Ral_LightingCompositionReceiptValid( &entities[i].lightingComposition )
					|| entities[i].lightingComposition.diffuseAuthority
						!= RAL_LIGHTING_DIFFUSE_AUTHORITY_LOCAL_SH )
				candidate.unresolvedCount++;
			else candidate.loweredLocalIrradianceEntityCount++;
		}
		if ( entity->reType == RT_MODEL ) {
			renderModelSnapshot_t model;
			if ( entity->hModel == 0 ) {
				if ( !MaterialReady( frontend, entity->customShader, NULL )
						|| !AddU32( &candidate.loweredEntityIndexCount, 36u )
						|| !AddU32( &candidate.loweredEntityBatchCount, 1u ) )
					candidate.unresolvedCount++;
				else candidate.loweredPrimitiveEntityCount++;
				continue;
			}
			if ( entity->hModel <= 0 || !RenderSubmission_ModelSnapshot( frontend,
					entity->hModel, &model ) || !model.ready || !model.frameCount
					|| entity->frame < 0 || entity->oldframe < 0
					|| (uint32_t)entity->frame >= model.frameCount
					|| (uint32_t)entity->oldframe >= model.frameCount
					|| !isfinite( entity->backlerp ) || entity->backlerp < 0.0f
					|| entity->backlerp > 1.0f ) {
				candidate.unresolvedCount++; continue;
			}
			if ( !AddU32( &candidate.loweredEntityIndexCount, model.indexCount )
					|| !AddU32( &candidate.loweredEntityBatchCount, model.batchCount ) )
				return qfalse;
			for ( uint32_t batch = 0u; batch < model.batchCount; ++batch ) {
				qhandle_t material = RenderSubmission_EntityBatchMaterial(
					frontend, &entities[i], &model.batches[batch] );
				if ( !MaterialReady( frontend, material, NULL ) )
					candidate.unresolvedCount++;
			}
			candidate.loweredModelEntityCount++;
		} else if ( entity->reType == RT_SPRITE || entity->reType == RT_BEAM ) {
			uint32_t indexCount = entity->reType == RT_SPRITE ? 6u : 36u;
			if ( entity->reType == RT_BEAM ) {
				float x = entity->oldorigin[0] - entity->origin[0];
				float y = entity->oldorigin[1] - entity->origin[1];
				float z = entity->oldorigin[2] - entity->origin[2];
				if ( x * x + y * y + z * z <= 0.00000001f ) {
					candidate.unresolvedCount++; continue;
				}
			}
			if ( !MaterialReady( frontend, entity->customShader, NULL )
					|| !AddU32( &candidate.loweredEntityIndexCount, indexCount )
					|| !AddU32( &candidate.loweredEntityBatchCount, 1u ) )
				candidate.unresolvedCount++;
			else candidate.loweredPrimitiveEntityCount++;
		} else {
			candidate.unresolvedCount++;
		}
	}
	if ( candidate.loweredTemporalEntityCount != submission->temporalEntityCount )
		candidate.unresolvedCount++;
	if ( candidate.loweredLocalIrradianceEntityCount
			!= submission->localIrradianceEntityCount ) candidate.unresolvedCount++;
	ui = RenderSubmission_UiPrimitives( frontend, &uiCount );
	if ( uiCount != submission->uiPrimitiveCount || ( uiCount && !ui ) )
		candidate.unresolvedCount++;
	for ( uint32_t i = 0u; ui && i < uiCount; ++i ) {
		renderMaterialSnapshot_t material;
		if ( ( ui[i].kind != RENDER_UI_QUAD && ui[i].kind != RENDER_UI_LINE )
				|| !MaterialReady( frontend, ui[i].material, &material ) ) {
			candidate.unresolvedCount++; continue;
		}
		candidate.loweredUiPrimitiveCount++;
		candidate.texturedUiPrimitiveCount++;
		if ( material.msdf ) candidate.msdfUiPrimitiveCount++;
	}
	if ( !RenderSubmission_EffectSnapshots( frontend, &polygons,
			&polygonCommandCount, &polyVertices, &polyVertexCount,
			&lights, &lightCount ) || lightCount != submission->lightCount
			|| ( polygonCommandCount && !polygons )
			|| ( polyVertexCount && !polyVertices )
			|| ( lightCount && !lights ) ) candidate.unresolvedCount++;
	{
		qboolean lightsValid = lightCount == submission->lightCount
			&& ( !lightCount || lights );
		for ( uint32_t i = 0u; lightsValid && i < lightCount; ++i ) {
			const renderLightCommand_t *light = &lights[i];
			if ( !isfinite( light->intensity ) || light->intensity <= 0.0f ) {
				lightsValid = qfalse; break;
			}
			for ( uint32_t component = 0u; component < 3u; ++component ) {
				if ( !isfinite( light->origin[component] )
						|| !isfinite( light->color[component] )
						|| light->color[component] < 0.0f
						|| ( light->hasEnd
							&& !isfinite( light->end[component] ) ) ) {
					lightsValid = qfalse; break;
				}
			}
			if ( !lightsValid ) break;
		}
		if ( !lightsValid
				|| !AddU32( &candidate.loweredEffectBatchCount, lightCount )
				|| !AddU32( &candidate.loweredEffectIndexCount, lightCount * 6u ) )
			candidate.unresolvedCount++;
		else candidate.loweredLightCount = lightCount;
	}
	{
		uint32_t polygonTotal = 0u, expectedVertex = 0u;
		for ( uint32_t i = 0u; polygons && i < polygonCommandCount; ++i ) {
			const renderPolyCommand_t *polygon = &polygons[i];
			uint64_t vertices, indices;
			if ( polygon->verticesPerPolygon < 3u ) {
				candidate.unresolvedCount++; break;
			}
			vertices = (uint64_t)polygon->verticesPerPolygon
				* polygon->polygonCount;
			indices = (uint64_t)( polygon->verticesPerPolygon - 2u )
				* 3u * polygon->polygonCount;
			if ( polygon->firstVertex != expectedVertex
					|| vertices > polyVertexCount - expectedVertex
					|| indices > UINT32_MAX
					|| !MaterialReady( frontend, polygon->material, NULL )
					|| !AddU32( &polygonTotal, polygon->polygonCount )
					|| !AddU32( &candidate.loweredEffectIndexCount,
						(uint32_t)indices )
					|| !AddU32( &candidate.loweredEffectBatchCount, 1u ) ) {
				candidate.unresolvedCount++; break;
			}
			expectedVertex += (uint32_t)vertices;
		}
		if ( polygonTotal != submission->polygonCount
				|| expectedVertex != polyVertexCount ) candidate.unresolvedCount++;
		else candidate.loweredPolygonCount = polygonTotal;
	}
	if ( candidate.unresolvedCount || candidate.fallbackCount
			|| candidate.fatalCount ) return qfalse;
	candidate.loweringDigest = HashU64( PLAN_FNV_OFFSET,
		candidate.frontendFrameDigest );
	candidate.loweringDigest = HashU64( candidate.loweringDigest,
		( (uint64_t)candidate.loweredWorldIndexCount << 32u )
		| candidate.loweredEntityIndexCount );
	candidate.loweringDigest = HashU64( candidate.loweringDigest,
		( (uint64_t)candidate.loweredMaterialCount << 32u )
		| candidate.loweredUiPrimitiveCount );
	candidate.loweringDigest = HashU64( candidate.loweringDigest,
		( (uint64_t)candidate.loweredPolygonCount << 32u )
		| candidate.loweredLightCount );
	candidate.loweringDigest = HashU64( candidate.loweringDigest,
		candidate.loweredLocalIrradianceEntityCount );
	candidate.ready = qtrue;
	if ( !ReceiptValid( &candidate ) ) return qfalse;
	*outReceipt = candidate;
	return qtrue;
}
