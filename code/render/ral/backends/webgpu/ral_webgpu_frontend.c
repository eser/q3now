// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "ral_webgpu_frontend.h"

#include <limits.h>
#include <math.h>
#include <string.h>

#define PLAN_FNV_OFFSET UINT64_C(1469598103934665603)
#define PLAN_FNV_PRIME UINT64_C(1099511628211)

static uint32_t s_planFailureStage;
uint32_t RalWebGpu_FrontendPlanLastFailureStage( void ) {
	return s_planFailureStage;
}
#define PLAN_STAGE( value ) do { if ( !s_planFailureStage ) s_planFailureStage = (value); } while ( 0 )

static qboolean AddU32( uint32_t *value, uint32_t amount ) {
	if ( !value || amount > UINT32_MAX - *value ) return qfalse;
	*value += amount; return qtrue;
}

static uint64_t HashU64( uint64_t digest, uint64_t value ) {
	for ( uint32_t i = 0u; i < 8u; ++i ) {
		digest ^= ( value >> ( i * 8u ) ) & 0xffu;
		digest *= PLAN_FNV_PRIME;
	}
	return digest;
}

static qboolean MaterialReady( const renderSubmissionState_t *frontend,
		qhandle_t handle, renderMaterialSnapshot_t *out ) {
	renderMaterialSnapshot_t material;
	if ( handle <= 0 || !RenderSubmission_MaterialSnapshot( frontend, handle,
			&material ) || material.ready != qtrue || !material.rgba8
			|| !material.byteCount || !material.width || !material.height )
		return qfalse;
	if ( out ) *out = material;
	return qtrue;
}

static qboolean ReceiptValid( const ralWebGpuFrontendPlanReceipt_t *receipt ) {
	return receipt
		&& receipt->schemaVersion == RAL_WEBGPU_FRONTEND_SCHEMA_VERSION
		&& receipt->backendType == RAL_BACKEND_WEBGPU
		&& receipt->backendGeneration && receipt->backendGeneration != UINT64_MAX
		&& receipt->planGeneration && receipt->planGeneration != UINT64_MAX
		&& receipt->frontendOwnerGeneration && receipt->frontendFrameGeneration
		&& receipt->frontendFrameDigest && receipt->loweringDigest
		&& !receipt->unresolvedCount && !receipt->fallbackCount
		&& !receipt->fatalCount && receipt->ready == qtrue;
}

qboolean RalWebGpu_FrontendPlanReceiptExact(
		const ralWebGpuFrontendPlanReceipt_t *a,
		const ralWebGpuFrontendPlanReceipt_t *b ) {
	return ReceiptValid( a ) && ReceiptValid( b )
		&& !memcmp( a, b, sizeof( *a ) );
}

qboolean RalWebGpu_FrontendPlanBuild( ralWebGpuRuntime_t *runtime,
		const ralWebGpuRuntimeReceipt_t *runtimeReceipt,
		const renderSubmissionState_t *frontend,
		const renderSubmissionReceipt_t *submission, uint64_t planGeneration,
		ralWebGpuFrontendPlanReceipt_t *outReceipt ) {
	ralWebGpuFrontendPlanReceipt_t candidate;
	renderWorldSnapshot_t world;
	const renderEntityCommand_t *entities;
	const renderUiPrimitive_t *ui;
	const renderPolyCommand_t *polygons;
	const polyVert_t *vertices;
	const renderLightCommand_t *lights;
	uint32_t entityCount = 0u, uiCount = 0u, polygonBatchCount = 0u;
	uint32_t polygonVertexCount = 0u, lightCount = 0u;
	s_planFailureStage = 0u;
	if ( !runtime || !runtimeReceipt || !frontend || !submission || !outReceipt ) {
		PLAN_STAGE( 101u ); return qfalse;
	}
	if ( !RalWebGpu_RuntimeResources( runtime, runtimeReceipt ) ) {
		PLAN_STAGE( 102u ); return qfalse;
	}
	if ( !RenderSubmission_ReceiptExact( submission, submission ) ) {
		PLAN_STAGE( 103u ); return qfalse;
	}
	if ( !planGeneration || planGeneration == UINT64_MAX ) {
		PLAN_STAGE( 104u ); return qfalse;
	}
	if ( !frontend->initialized ) { PLAN_STAGE( 105u ); return qfalse; }
	if ( frontend->frameOpen ) { PLAN_STAGE( 114u ); return qfalse; }
	if ( frontend->frameSealed != qtrue ) { PLAN_STAGE( 115u ); return qfalse; }
	if ( frontend->ownerGeneration != submission->ownerGeneration ) {
		PLAN_STAGE( 106u ); return qfalse;
	}
	if ( frontend->assetDigest != submission->assetDigest ) {
		PLAN_STAGE( 107u ); return qfalse;
	}
	if ( frontend->materialDigest != submission->materialDigest ) {
		PLAN_STAGE( 108u ); return qfalse;
	}
	if ( frontend->modelDigest != submission->modelDigest ) {
		PLAN_STAGE( 109u ); return qfalse;
	}
	if ( frontend->worldDigest != submission->worldDigest ) {
		PLAN_STAGE( 110u ); return qfalse;
	}
	if ( frontend->sceneDigest != submission->sceneDigest ) {
		PLAN_STAGE( 111u ); return qfalse;
	}
	if ( frontend->uiDigest != submission->uiDigest ) {
		PLAN_STAGE( 112u ); return qfalse;
	}
	if ( RenderSubmission_FrameDigest( frontend ) != submission->frameDigest ) {
		PLAN_STAGE( 113u ); return qfalse;
	}
	memset( &candidate, 0, sizeof( candidate ) );
	candidate.schemaVersion = RAL_WEBGPU_FRONTEND_SCHEMA_VERSION;
	candidate.backendType = RAL_BACKEND_WEBGPU;
	candidate.backendGeneration = runtimeReceipt->generation;
	candidate.planGeneration = planGeneration;
	candidate.frontendOwnerGeneration = submission->ownerGeneration;
	candidate.frontendFrameGeneration = submission->frameGeneration;
	candidate.frontendFrameDigest = submission->frameDigest;
	for ( uint32_t i = 0u; i < frontend->materialCount; ++i ) {
		if ( frontend->materials[i].snapshot.ready != qtrue
				|| !frontend->materials[i].snapshot.rgba8 ) {
			PLAN_STAGE( 2u ); candidate.unresolvedCount++;
		}
		else candidate.materialCount++;
	}
	if ( candidate.materialCount != submission->resolvedMaterialCount ) {
		PLAN_STAGE( 3u );
		candidate.unresolvedCount++;
	}
	if ( submission->worldLoaded ) {
		if ( !RenderSubmission_WorldSnapshot( frontend, &world ) ) {
			PLAN_STAGE( 4u );
			candidate.unresolvedCount++;
		} else {
			candidate.worldVertexCount = world.vertexCount;
			candidate.worldIndexCount = world.indexCount;
			candidate.worldBatchCount = world.batchCount;
			candidate.patchBatchCount = world.patchBatchCount;
			for ( uint32_t i = 0u; i < world.batchCount; ++i ) {
				if ( world.batches[i].surfaceType < RENDER_WORLD_SURFACE_PLANAR
						|| world.batches[i].surfaceType
							> RENDER_WORLD_SURFACE_TRIANGLES
						|| world.batches[i].firstIndex > world.indexCount
						|| world.batches[i].indexCount
							> world.indexCount - world.batches[i].firstIndex )
					{ PLAN_STAGE( 5u ); candidate.unresolvedCount++; }
				if ( !MaterialReady( frontend, world.batches[i].baseMaterial, NULL ) )
					{ PLAN_STAGE( 6u ); candidate.unresolvedCount++; }
				if ( world.batches[i].lightmapMaterial > 0 ) {
					if ( MaterialReady( frontend, world.batches[i].lightmapMaterial,
							NULL ) ) candidate.lightmappedWorldBatchCount++;
					else { PLAN_STAGE( 7u ); candidate.unresolvedCount++; }
				}
			}
		}
	}
	entities = RenderSubmission_EntityCommands( frontend, &entityCount );
	if ( entityCount != submission->entityCount || ( entityCount && !entities ) ) {
		PLAN_STAGE( 8u );
		candidate.unresolvedCount++;
	}
	for ( uint32_t i = 0u; entities && i < entityCount; ++i ) {
		const refEntity_t *entity = &entities[i].entity;
		if ( entities[i].hasTemporal ) candidate.temporalEntityCount++;
		if ( entities[i].hasLocalIrradiance ) {
			if ( !Ral_IrradianceEntitySampleReceiptValid( &entities[i].localIrradiance )
					|| !Ral_LightingCompositionReceiptValid( &entities[i].lightingComposition )
					|| entities[i].lightingComposition.diffuseAuthority
						!= RAL_LIGHTING_DIFFUSE_AUTHORITY_LOCAL_SH ) {
				PLAN_STAGE( 24u ); candidate.unresolvedCount++;
			} else candidate.localIrradianceEntityCount++;
		}
		if ( entity->reType == RT_MODEL ) {
			renderModelSnapshot_t model;
			if ( entity->hModel <= 0 || !RenderSubmission_ModelSnapshot( frontend,
					entity->hModel, &model ) || !model.ready || !model.frameCount
					|| entity->frame < 0 || entity->oldframe < 0
					|| (uint32_t)entity->frame >= model.frameCount
					|| (uint32_t)entity->oldframe >= model.frameCount
					|| !isfinite( entity->backlerp ) || entity->backlerp < 0.0f
					|| entity->backlerp > 1.0f
					|| !AddU32( &candidate.entityIndexCount, model.indexCount )
					|| !AddU32( &candidate.entityBatchCount, model.batchCount ) ) {
				PLAN_STAGE( 9u ); candidate.unresolvedCount++; continue;
			}
			for ( uint32_t batch = 0u; batch < model.batchCount; ++batch ) {
				qhandle_t material = RenderSubmission_EntityBatchMaterial(
					frontend, &entities[i], &model.batches[batch] );
				if ( !MaterialReady( frontend, material, NULL ) )
					{ PLAN_STAGE( 10u ); candidate.unresolvedCount++; }
			}
			candidate.modelEntityCount++;
		} else if ( entity->reType == RT_SPRITE || entity->reType == RT_BEAM ) {
			uint32_t indices = entity->reType == RT_SPRITE ? 6u : 36u;
			if ( !MaterialReady( frontend, entity->customShader, NULL )
					|| !AddU32( &candidate.entityIndexCount, indices )
					|| !AddU32( &candidate.entityBatchCount, 1u ) ) {
				PLAN_STAGE( 11u ); candidate.unresolvedCount++;
			}
			else candidate.primitiveEntityCount++;
		} else { PLAN_STAGE( 12u ); candidate.unresolvedCount++; }
	}
	if ( candidate.temporalEntityCount != submission->temporalEntityCount ) {
		PLAN_STAGE( 13u );
		candidate.unresolvedCount++;
	}
	if ( candidate.localIrradianceEntityCount
			!= submission->localIrradianceEntityCount ) {
		PLAN_STAGE( 25u ); candidate.unresolvedCount++;
	}
	ui = RenderSubmission_UiPrimitives( frontend, &uiCount );
	if ( uiCount != submission->uiPrimitiveCount || ( uiCount && !ui ) ) {
		PLAN_STAGE( 14u );
		candidate.unresolvedCount++;
	}
	for ( uint32_t i = 0u; ui && i < uiCount; ++i ) {
		renderMaterialSnapshot_t material;
		if ( ( ui[i].kind != RENDER_UI_QUAD && ui[i].kind != RENDER_UI_LINE )
				|| !MaterialReady( frontend, ui[i].material, &material ) ) {
			PLAN_STAGE( 15u ); candidate.unresolvedCount++; continue;
		}
		candidate.uiPrimitiveCount++;
		candidate.texturedUiPrimitiveCount++;
		if ( material.msdf ) candidate.msdfUiPrimitiveCount++;
	}
	if ( !RenderSubmission_EffectSnapshots( frontend, &polygons,
			&polygonBatchCount, &vertices, &polygonVertexCount,
			&lights, &lightCount ) || ( polygonBatchCount && !polygons )
			|| ( polygonVertexCount && !vertices ) || ( lightCount && !lights )
			|| lightCount != submission->lightCount ) {
		PLAN_STAGE( 16u ); candidate.unresolvedCount++;
	}
	for ( uint32_t i = 0u; polygons && i < polygonBatchCount; ++i ) {
		uint64_t vertexCount = (uint64_t)polygons[i].verticesPerPolygon
			* polygons[i].polygonCount;
		if ( polygons[i].verticesPerPolygon < 3u
				|| polygons[i].firstVertex > polygonVertexCount
				|| vertexCount > polygonVertexCount - polygons[i].firstVertex
				|| !MaterialReady( frontend, polygons[i].material, NULL )
				|| !AddU32( &candidate.polygonCount,
					polygons[i].polygonCount ) ) {
			PLAN_STAGE( 17u ); candidate.unresolvedCount++;
		}
	}
	candidate.polygonBatchCount = polygonBatchCount;
	candidate.lightCount = lightCount;
	for ( uint32_t i = 0u; lights && i < lightCount; ++i ) {
		if ( !isfinite( lights[i].intensity ) || lights[i].intensity <= 0.0f ) {
			PLAN_STAGE( 18u ); candidate.unresolvedCount++; continue;
		}
		for ( uint32_t component = 0u; component < 3u; ++component ) {
			if ( !isfinite( lights[i].origin[component] )
					|| !isfinite( lights[i].color[component] )
					|| lights[i].color[component] < 0.0f
					|| ( lights[i].hasEnd
						&& !isfinite( lights[i].end[component] ) ) ) {
				PLAN_STAGE( 19u ); candidate.unresolvedCount++; break;
			}
		}
	}
	if ( candidate.polygonCount != submission->polygonCount ) {
		PLAN_STAGE( 20u );
		candidate.unresolvedCount++;
	}
	if ( !AddU32( &candidate.drawCount, candidate.worldBatchCount )
			|| !AddU32( &candidate.drawCount, candidate.entityBatchCount )
			|| !AddU32( &candidate.drawCount, candidate.polygonBatchCount )
			|| !AddU32( &candidate.drawCount, candidate.lightCount )
			|| !AddU32( &candidate.drawCount, candidate.uiPrimitiveCount ) ) {
		PLAN_STAGE( 21u );
		return qfalse;
	}
	if ( candidate.unresolvedCount || candidate.fallbackCount
			|| candidate.fatalCount ) { PLAN_STAGE( 22u ); return qfalse; }
	candidate.loweringDigest = HashU64( PLAN_FNV_OFFSET,
		candidate.frontendFrameDigest );
	candidate.loweringDigest = HashU64( candidate.loweringDigest,
		( (uint64_t)candidate.materialCount << 32u ) | candidate.drawCount );
	candidate.loweringDigest = HashU64( candidate.loweringDigest,
		( (uint64_t)candidate.worldIndexCount << 32u )
		| candidate.entityIndexCount );
	candidate.loweringDigest = HashU64( candidate.loweringDigest,
		( (uint64_t)candidate.polygonCount << 32u ) | candidate.lightCount );
	candidate.loweringDigest = HashU64( candidate.loweringDigest,
		candidate.localIrradianceEntityCount );
	if ( !candidate.loweringDigest ) candidate.loweringDigest = 1u;
	candidate.ready = qtrue;
	if ( !ReceiptValid( &candidate ) ) { PLAN_STAGE( 23u ); return qfalse; }
	*outReceipt = candidate;
	return qtrue;
}
