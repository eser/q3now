// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "ral_webgpu_product.h"

#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
	qhandle_t handle;
	uint64_t generation;
	uint64_t digest;
	ralWebGpuResource_t *texture;
	ralWebGpuResource_t *sampler;
	ralWebGpuResourceReceipt_t textureReceipt;
	ralWebGpuResourceReceipt_t samplerReceipt;
	qboolean ready;
} materialEntry_t;

struct ralWebGpuProduct_s {
	ralWebGpuRuntime_t *runtime;
	ralWebGpuPipelineReceipt_t worldPipeline;
	ralWebGpuPipelineReceipt_t entityPipeline;
	ralWebGpuPipelineReceipt_t effectPipeline;
	ralWebGpuPipelineReceipt_t uiPipeline;
	ralWebGpuPipelineReceipt_t msdfPipeline;
	uint64_t generation;
	uint64_t planGeneration;
	uint32_t viewportWidth;
	uint32_t viewportHeight;
	materialEntry_t materials[RENDER_SUBMISSION_MAX_MATERIALS];
	uint32_t materialCount;
	ralWebGpuResource_t *worldVertices;
	ralWebGpuResource_t *worldIndices;
	ralWebGpuResource_t *worldMaterials;
	ralWebGpuResource_t *entityVertices;
	ralWebGpuResource_t *entityIndices;
	ralWebGpuResource_t *entityLighting;
	ralWebGpuResource_t *miscVertices;
	ralWebGpuResource_t *miscIndices;
	ralWebGpuResourceReceipt_t worldVertexReceipt;
	ralWebGpuResourceReceipt_t worldIndexReceipt;
	ralWebGpuResourceReceipt_t worldMaterialReceipt;
	ralWebGpuResourceReceipt_t entityVertexReceipt;
	ralWebGpuResourceReceipt_t entityIndexReceipt;
	ralWebGpuResourceReceipt_t entityLightingReceipt;
	ralWebGpuResourceReceipt_t miscVertexReceipt;
	ralWebGpuResourceReceipt_t miscIndexReceipt;
	ralWebGpuProductFrameReceipt_t published;
	qboolean inFlight;
	qboolean entityPipelineReady;
	uintptr_t worldMaterialBindGroup;
	qboolean worldMaterialBindGroupReady;
	uintptr_t entityLightingBindGroup;
	qboolean entityLightingBindGroupReady;
	qboolean effectPipelineReady;
	qboolean uiPipelinesReady;
	uintptr_t atmosphereBindGroups[3];
	qboolean atmosphereBindGroupsReady;
	ralAtmosphereWeatherReceipt_t weatherPlan;
	ralWebGpuWeatherReceipt_t weatherExecution;
	qboolean weatherReady;
};

#define RAL_WEBGPU_ENTITY_LIGHTING_CAPACITY \
	( RENDER_SUBMISSION_MAX_ENTITIES * RENDER_SUBMISSION_MAX_MODEL_BATCHES )
#define RAL_WEBGPU_ENTITY_LIGHTING_BYTES \
	( (uint64_t)RAL_WEBGPU_ENTITY_LIGHTING_CAPACITY * 16u * sizeof( float ) )
#define RAL_WEBGPU_WORLD_MATERIAL_BYTES \
	( (uint64_t)RENDER_SUBMISSION_MAX_WORLD_BATCHES * 4u * sizeof( float ) )

static uint32_t s_productFailureStage;
uint32_t RalWebGpu_ProductLastFailureStage( void ) {
	return s_productFailureStage;
}

static uint64_t HashU64( uint64_t digest, uint64_t value ) {
	for ( uint32_t i = 0u; i < 8u; ++i ) {
		digest ^= ( value >> ( i * 8u ) ) & 0xffu;
		digest *= UINT64_C(1099511628211);
	}
	return digest;
}

static qboolean ReceiptValid( const ralWebGpuProductFrameReceipt_t *receipt ) {
	qboolean entityResourcesValid;
	qboolean miscResourcesValid;
	if ( !receipt ) return qfalse;
	entityResourcesValid = receipt->plan.entityBatchCount == 0u
		? qtrue
		: RalWebGpu_ResourceReceiptExact( &receipt->entityVertexBuffer,
			&receipt->entityVertexBuffer )
			&& RalWebGpu_ResourceReceiptExact( &receipt->entityIndexBuffer,
				&receipt->entityIndexBuffer )
			&& RalWebGpu_ResourceReceiptExact( &receipt->entityLightingBuffer,
				&receipt->entityLightingBuffer )
			&& RalWebGpu_WriteReceiptExact( &receipt->entityVertexWrite,
				&receipt->entityVertexWrite )
			&& RalWebGpu_WriteReceiptExact( &receipt->entityIndexWrite,
				&receipt->entityIndexWrite )
			&& RalWebGpu_WriteReceiptExact( &receipt->entityLightingWrite,
				&receipt->entityLightingWrite );
	miscResourcesValid = receipt->plan.polygonBatchCount == 0u
			&& receipt->plan.lightCount == 0u
			&& receipt->plan.uiPrimitiveCount == 0u
		? qtrue
		: RalWebGpu_ResourceReceiptExact( &receipt->miscVertexBuffer,
			&receipt->miscVertexBuffer )
			&& RalWebGpu_ResourceReceiptExact( &receipt->miscIndexBuffer,
				&receipt->miscIndexBuffer )
			&& RalWebGpu_WriteReceiptExact( &receipt->miscVertexWrite,
				&receipt->miscVertexWrite )
			&& RalWebGpu_WriteReceiptExact( &receipt->miscIndexWrite,
				&receipt->miscIndexWrite );
	return receipt->schemaVersion == RAL_WEBGPU_PRODUCT_SCHEMA_VERSION
		&& receipt->backendType == RAL_BACKEND_WEBGPU
		&& receipt->backendGeneration && receipt->productGeneration
		&& receipt->frameGeneration && receipt->targetIdentity
		&& RalWebGpu_FrontendPlanReceiptExact( &receipt->plan, &receipt->plan )
		&& RalWebGpu_ResourceReceiptExact( &receipt->worldVertexBuffer,
			&receipt->worldVertexBuffer )
		&& RalWebGpu_ResourceReceiptExact( &receipt->worldIndexBuffer,
			&receipt->worldIndexBuffer )
		&& RalWebGpu_WriteReceiptExact( &receipt->worldVertexWrite,
			&receipt->worldVertexWrite )
		&& RalWebGpu_WriteReceiptExact( &receipt->worldIndexWrite,
			&receipt->worldIndexWrite )
		&& RalWebGpu_ResourceReceiptExact( &receipt->worldMaterialBuffer,
			&receipt->worldMaterialBuffer )
		&& RalWebGpu_WriteReceiptExact( &receipt->worldMaterialWrite,
			&receipt->worldMaterialWrite )
		&& RalWebGpu_CommandReceiptExact( &receipt->command, &receipt->command )
		&& RalWebGpu_SubmissionReceiptExact( &receipt->submission,
			&receipt->submission )
		&& entityResourcesValid
		&& miscResourcesValid
		&& receipt->worldDrawCount == receipt->plan.worldBatchCount
		&& receipt->worldEmissiveDrawCount <= receipt->worldDrawCount
		&& receipt->modelEntityCount == receipt->plan.modelEntityCount
		&& receipt->primitiveEntityCount == receipt->plan.primitiveEntityCount
		&& receipt->temporalEntityCount == receipt->plan.temporalEntityCount
		&& receipt->entityDrawCount == receipt->plan.entityBatchCount
		&& receipt->localIrradianceEntityCount
			== receipt->plan.localIrradianceEntityCount
		&& receipt->localIrradianceDrawCount <= receipt->entityDrawCount
		&& ( !receipt->localIrradianceEntityCount
			|| receipt->localIrradianceDrawCount )
		&& receipt->polygonDrawCount == receipt->plan.polygonBatchCount
		&& receipt->lightDrawCount == receipt->plan.lightCount
		&& receipt->uiDrawCount == receipt->plan.uiPrimitiveCount
		&& receipt->atmosphereBoundDrawCount == receipt->worldDrawCount
			+ receipt->entityDrawCount + receipt->polygonDrawCount
			+ receipt->lightDrawCount
		&& Ral_AtmosphereWeatherReceiptExact( &receipt->weather,
			&receipt->weather )
		&& receipt->weatherDrawCount == ( receipt->weather.zeroWork ? 0u : 1u )
		&& receipt->deferredNonWorldDrawCount
			== receipt->plan.drawCount - receipt->worldDrawCount
				- receipt->entityDrawCount - receipt->polygonDrawCount
				- receipt->lightDrawCount - receipt->uiDrawCount
		&& receipt->deferredNonWorldDrawCount == 0u
		&& !receipt->unresolvedCount && !receipt->fallbackCount
		&& !receipt->fatalCount && receipt->ready == qtrue;
}

qboolean RalWebGpu_ProductFrameReceiptExact(
		const ralWebGpuProductFrameReceipt_t *a,
		const ralWebGpuProductFrameReceipt_t *b ) {
	return ReceiptValid( a ) && ReceiptValid( b )
		&& !memcmp( a, b, sizeof( *a ) );
}

static materialEntry_t *FindMaterial( ralWebGpuProduct_t *product,
		qhandle_t handle ) {
	for ( uint32_t i = 0u; i < product->materialCount; ++i )
		if ( product->materials[i].handle == handle ) return &product->materials[i];
	return NULL;
}

static void DestroyMaterial( ralWebGpuResourceLayer_t *resources,
		materialEntry_t *entry ) {
	if ( !resources || !entry ) return;
	if ( entry->sampler ) RalWebGpu_DestroyResource( resources, entry->sampler,
		&entry->samplerReceipt );
	if ( entry->texture ) RalWebGpu_DestroyResource( resources, entry->texture,
		&entry->textureReceipt );
	memset( entry, 0, sizeof( *entry ) );
}

static qboolean UploadMaterial( ralWebGpuResourceLayer_t *resources,
		const renderMaterialSnapshot_t *material, materialEntry_t *out ) {
	ralWebGpuTextureDesc_t textureDesc;
	ralWebGpuSamplerDesc_t samplerDesc;
	ralWebGpuWriteReceipt_t writeReceipt;
	byte *padded;
	uint32_t rowBytes;
	uint64_t paddedBytes;
	if ( !resources || !material || !out || material->ready != qtrue
			|| !material->rgba8 || !material->width || !material->height
			|| material->rowBytes != material->width * 4u ) return qfalse;
	/* Keep the browser dispatch ABI uniform for every texture extent. The
	 * shared host contract requires COPY_BYTES_PER_ROW_ALIGNMENT even for a
	 * single-row write, so 1x1 fallback/default materials must be padded too. */
	rowBytes = ( material->rowBytes + 255u ) & ~255u;
	paddedBytes = (uint64_t)rowBytes * material->height;
	if ( !paddedBytes || paddedBytes > RAL_WEBGPU_MAX_RESOURCE_BYTES ) return qfalse;
	padded = (byte *)calloc( 1u, (size_t)paddedBytes );
	if ( !padded ) return qfalse;
	for ( uint32_t row = 0u; row < material->height; ++row )
		memcpy( padded + (size_t)row * rowBytes,
			material->rgba8 + (size_t)row * material->rowBytes,
			material->rowBytes );
	memset( out, 0, sizeof( *out ) );
	memset( &textureDesc, 0, sizeof( textureDesc ) );
	textureDesc.width = material->width; textureDesc.height = material->height;
	textureDesc.depth = 1u;
	textureDesc.format = material->srgb ? RAL_FORMAT_R8G8B8A8_SRGB
		: RAL_FORMAT_R8G8B8A8_UNORM;
	textureDesc.bytesPerTexel = 4u;
	memset( &samplerDesc, 0, sizeof( samplerDesc ) );
	samplerDesc.linearMinification = qtrue;
	samplerDesc.linearMagnification = qtrue;
	samplerDesc.clampToEdge = material->clampToEdge;
	if ( !RalWebGpu_CreateTexture( resources, &textureDesc, &out->texture,
			&out->textureReceipt )
			|| !RalWebGpu_WriteTexture( resources, out->texture,
				&out->textureReceipt, padded, paddedBytes, rowBytes,
				material->height, &writeReceipt )
			|| !RalWebGpu_CreateSampler( resources, &samplerDesc, &out->sampler,
				&out->samplerReceipt ) ) {
		free( padded ); DestroyMaterial( resources, out ); return qfalse;
	}
	free( padded ); out->handle = material->handle;
	out->generation = material->generation; out->digest = material->digest;
	out->ready = qtrue; return qtrue;
}

static void DestroyFrameResources( ralWebGpuProduct_t *product,
		ralWebGpuResourceLayer_t *resources ) {
	if ( !product || !resources ) return;
	if ( product->miscIndices ) RalWebGpu_DestroyResource( resources,
		product->miscIndices, &product->miscIndexReceipt );
	if ( product->miscVertices ) RalWebGpu_DestroyResource( resources,
		product->miscVertices, &product->miscVertexReceipt );
	if ( product->entityIndices ) RalWebGpu_DestroyResource( resources,
		product->entityIndices, &product->entityIndexReceipt );
	if ( product->entityVertices ) RalWebGpu_DestroyResource( resources,
		product->entityVertices, &product->entityVertexReceipt );
	if ( product->worldIndices ) RalWebGpu_DestroyResource( resources,
		product->worldIndices, &product->worldIndexReceipt );
	if ( product->worldVertices ) RalWebGpu_DestroyResource( resources,
		product->worldVertices, &product->worldVertexReceipt );
	product->worldIndices = NULL; product->worldVertices = NULL;
	product->entityIndices = NULL; product->entityVertices = NULL;
	product->miscIndices = NULL; product->miscVertices = NULL;
	memset( &product->worldIndexReceipt, 0, sizeof( product->worldIndexReceipt ) );
	memset( &product->worldVertexReceipt, 0, sizeof( product->worldVertexReceipt ) );
	memset( &product->entityIndexReceipt, 0, sizeof( product->entityIndexReceipt ) );
	memset( &product->entityVertexReceipt, 0, sizeof( product->entityVertexReceipt ) );
	memset( &product->miscIndexReceipt, 0, sizeof( product->miscIndexReceipt ) );
	memset( &product->miscVertexReceipt, 0, sizeof( product->miscVertexReceipt ) );
}

typedef struct {
	uint32_t firstIndex;
	uint32_t indexCount;
	qhandle_t material;
	uint64_t contentDigest;
	float localSh[4][4];
} entityDrawPlan_t;

static qboolean EntityLighting( const renderEntityCommand_t *command,
		float out[4][4] ) {
	if ( !command || !out ) return qfalse;
	memset( out, 0, 16u * sizeof( float ) );
	if ( !command->hasLocalIrradiance ) return qtrue;
	if ( !Ral_IrradianceEntitySampleReceiptValid( &command->localIrradiance )
			|| !Ral_LightingCompositionReceiptValid(
				&command->lightingComposition )
			|| command->lightingComposition.diffuseAuthority
				!= RAL_LIGHTING_DIFFUSE_AUTHORITY_LOCAL_SH
			|| command->lightingComposition.activeTermMask
				!= RAL_LIGHTING_TERM_LOCAL_SH ) return qfalse;
	for ( uint32_t coefficient = 0u; coefficient < 4u; ++coefficient )
		for ( uint32_t channel = 0u; channel < 3u; ++channel )
			out[coefficient][channel] = (float)command->localIrradiance
				.blendedCoefficientsQ16[coefficient][channel] / 65536.0f;
	out[0][3] = 1.0f;
	return qtrue;
}

static void SetEntityVertex( renderWorldVertex_t *vertex, float x, float y,
		float z, float s, float t, const byte color[4] ) {
	memset( vertex, 0, sizeof( *vertex ) );
	vertex->position[0] = x; vertex->position[1] = y; vertex->position[2] = z;
	vertex->texCoord[0] = s; vertex->texCoord[1] = t;
	vertex->normal[2] = 1.0f;
	memcpy( vertex->color, color, sizeof( vertex->color ) );
}

static qboolean ProjectVertices( const renderWorldSnapshot_t *world,
		renderWorldVertex_t *vertices, uint32_t vertexCount ) {
	const float nearPlane = 4.0f, farPlane = 65536.0f;
	float tanHalfX, tanHalfY, depthScale, depthBias;
	if ( !world || ( vertexCount && !vertices )
			|| !isfinite( world->fovX ) || !isfinite( world->fovY )
			|| world->fovX <= 1.0f || world->fovX >= 179.0f
			|| world->fovY <= 1.0f || world->fovY >= 179.0f ) return qfalse;
	tanHalfX = tanf( world->fovX * 0.00872664625997164788f );
	tanHalfY = tanf( world->fovY * 0.00872664625997164788f );
	if ( !isfinite( tanHalfX ) || !isfinite( tanHalfY )
			|| tanHalfX <= 0.0f || tanHalfY <= 0.0f ) return qfalse;
	depthScale = farPlane / ( farPlane - nearPlane );
	depthBias = -nearPlane * depthScale;
	for ( uint32_t i = 0u; i < vertexCount; ++i ) {
		float delta[3], forward, side, up, worldZ;
		worldZ = vertices[i].position[2];
		for ( uint32_t axis = 0u; axis < 3u; ++axis )
			delta[axis] = vertices[i].position[axis] - world->viewOrigin[axis];
		forward = delta[0] * world->viewAxis[0][0]
			+ delta[1] * world->viewAxis[0][1]
			+ delta[2] * world->viewAxis[0][2];
		side = -( delta[0] * world->viewAxis[1][0]
			+ delta[1] * world->viewAxis[1][1]
			+ delta[2] * world->viewAxis[1][2] );
		up = delta[0] * world->viewAxis[2][0]
			+ delta[1] * world->viewAxis[2][1]
			+ delta[2] * world->viewAxis[2][2];
		vertices[i].position[0] = side / tanHalfX;
		vertices[i].position[1] = up / tanHalfY;
		vertices[i].position[2] = forward * depthScale + depthBias;
		vertices[i].texCoord[0] = forward;
		vertices[i].lightmapCoord[0] = sqrtf( delta[0] * delta[0]
			+ delta[1] * delta[1] + delta[2] * delta[2] );
		vertices[i].lightmapCoord[1] = worldZ;
	}
	return qtrue;
}

static qboolean BuildEntityGeometry( const renderSubmissionState_t *frontend,
		const ralWebGpuFrontendPlanReceipt_t *plan,
		renderWorldVertex_t **outVertices, uint32_t *outVertexCount,
		uint32_t **outIndices, uint32_t *outIndexCount,
		entityDrawPlan_t **outDraws, uint32_t *outDrawCount ) {
	const renderEntityCommand_t *entities;
	renderWorldVertex_t *vertices;
	entityDrawPlan_t *draws;
	uint32_t *indices;
	uint32_t entityCount = 0u, vertexCapacity = 0u;
	uint32_t vertexCount = 0u, indexCount = 0u, drawCount = 0u;
	if ( !frontend || !plan || !outVertices || !outVertexCount || !outIndices
			|| !outIndexCount || !outDraws || !outDrawCount ) return qfalse;
	entities = RenderSubmission_EntityCommands( frontend, &entityCount );
	if ( entityCount && !entities ) return qfalse;
	for ( uint32_t i = 0u; i < entityCount; ++i ) {
		uint32_t add = 0u;
		if ( entities[i].entity.reType == RT_MODEL ) {
			renderModelSnapshot_t model;
			if ( !RenderSubmission_ModelSnapshot( frontend,
					entities[i].entity.hModel, &model ) || !model.ready ) return qfalse;
			add = model.vertexCount;
		} else if ( entities[i].entity.reType == RT_SPRITE ) add = 4u;
		else if ( entities[i].entity.reType == RT_BEAM ) add = 8u;
		else return qfalse;
		if ( add > UINT32_MAX - vertexCapacity ) return qfalse;
		vertexCapacity += add;
	}
	if ( !vertexCapacity || !plan->entityIndexCount || !plan->entityBatchCount )
		return qfalse;
	if ( (uint64_t)vertexCapacity * sizeof( *vertices )
			> RAL_WEBGPU_MAX_RESOURCE_BYTES
			|| (uint64_t)plan->entityIndexCount * sizeof( *indices )
				> RAL_WEBGPU_MAX_RESOURCE_BYTES ) return qfalse;
	vertices = calloc( vertexCapacity, sizeof( *vertices ) );
	indices = calloc( plan->entityIndexCount, sizeof( *indices ) );
	draws = calloc( plan->entityBatchCount, sizeof( *draws ) );
	if ( !vertices || !indices || !draws ) {
		free( vertices ); free( indices ); free( draws ); return qfalse;
	}
	for ( uint32_t i = 0u; i < entityCount; ++i ) {
		const refEntity_t *entity = &entities[i].entity;
		float localSh[4][4];
		uint32_t baseVertex = vertexCount;
		if ( !EntityLighting( &entities[i], localSh ) ) goto fail;
		if ( entity->reType == RT_MODEL ) {
			renderModelSnapshot_t model;
			if ( !RenderSubmission_ModelSnapshot( frontend, entity->hModel, &model )
					|| !model.ready || !model.positions || !model.normals || !model.texCoords
					|| !model.indices || !model.batches ) goto fail;
			for ( uint32_t vertex = 0u; vertex < model.vertexCount; ++vertex ) {
				const float *current = &model.positions[
					( (uint32_t)entity->frame * model.vertexCount + vertex ) * 3u];
				const float *old = &model.positions[
					( (uint32_t)entity->oldframe * model.vertexCount + vertex ) * 3u];
				const float *currentNormal = &model.normals[
					( (uint32_t)entity->frame * model.vertexCount + vertex ) * 3u];
				const float *oldNormal = &model.normals[
					( (uint32_t)entity->oldframe * model.vertexCount + vertex ) * 3u];
				float local[3], localNormal[3], world[3], worldNormal[3];
				for ( uint32_t component = 0u; component < 3u; ++component )
					local[component] = current[component] * ( 1.0f - entity->backlerp )
						+ old[component] * entity->backlerp;
				for ( uint32_t component = 0u; component < 3u; ++component )
					localNormal[component] = currentNormal[component]
						* ( 1.0f - entity->backlerp )
						+ oldNormal[component] * entity->backlerp;
				for ( uint32_t component = 0u; component < 3u; ++component )
					world[component] = entity->origin[component]
						+ entity->axis[0][component] * local[0]
						+ entity->axis[1][component] * local[1]
						+ entity->axis[2][component] * local[2];
				for ( uint32_t component = 0u; component < 3u; ++component )
					worldNormal[component] = entity->axis[0][component] * localNormal[0]
						+ entity->axis[1][component] * localNormal[1]
						+ entity->axis[2][component] * localNormal[2];
				SetEntityVertex( &vertices[vertexCount++], world[0], world[1],
					world[2], model.texCoords[vertex * 2u],
					model.texCoords[vertex * 2u + 1u], entity->shader.rgba );
				{
					renderWorldVertex_t *written = &vertices[vertexCount - 1u];
					float length = sqrtf( worldNormal[0] * worldNormal[0]
						+ worldNormal[1] * worldNormal[1]
						+ worldNormal[2] * worldNormal[2] );
					if ( length > 0.000001f ) for ( uint32_t component = 0u;
							component < 3u; ++component )
						written->normal[component] = worldNormal[component] / length;
				}
			}
			for ( uint32_t batchIndex = 0u; batchIndex < model.batchCount;
					++batchIndex ) {
				const renderModelBatch_t *batch = &model.batches[batchIndex];
				entityDrawPlan_t *draw;
				if ( drawCount >= plan->entityBatchCount
						|| batch->firstIndex > model.indexCount
						|| batch->indexCount > model.indexCount - batch->firstIndex
						|| indexCount > plan->entityIndexCount - batch->indexCount )
					goto fail;
				draw = &draws[drawCount++]; draw->firstIndex = indexCount;
				draw->indexCount = batch->indexCount;
				draw->material = RenderSubmission_EntityBatchMaterial(
					frontend, &entities[i], batch );
				draw->contentDigest = HashU64( model.digest,
					( (uint64_t)i << 32u ) | batchIndex );
				memcpy( draw->localSh, localSh, sizeof( draw->localSh ) );
				for ( uint32_t index = 0u; index < batch->indexCount; ++index ) {
					uint32_t source = model.indices[batch->firstIndex + index];
					if ( source >= model.vertexCount ) goto fail;
					indices[indexCount++] = baseVertex + source;
				}
			}
		} else if ( entity->reType == RT_SPRITE ) {
			static const uint32_t quad[6] = { 0u, 1u, 2u, 0u, 2u, 3u };
			float radius = entity->radius > 0.0f ? entity->radius : 1.0f;
			entityDrawPlan_t *draw = &draws[drawCount++];
			if ( drawCount > plan->entityBatchCount
					|| indexCount > plan->entityIndexCount - 6u ) goto fail;
			SetEntityVertex( &vertices[vertexCount++], entity->origin[0] - radius,
				entity->origin[1] - radius, entity->origin[2], 0.0f, 0.0f,
				entity->shader.rgba );
			SetEntityVertex( &vertices[vertexCount++], entity->origin[0] + radius,
				entity->origin[1] - radius, entity->origin[2], 1.0f, 0.0f,
				entity->shader.rgba );
			SetEntityVertex( &vertices[vertexCount++], entity->origin[0] + radius,
				entity->origin[1] + radius, entity->origin[2], 1.0f, 1.0f,
				entity->shader.rgba );
			SetEntityVertex( &vertices[vertexCount++], entity->origin[0] - radius,
				entity->origin[1] + radius, entity->origin[2], 0.0f, 1.0f,
				entity->shader.rgba );
			for ( uint32_t index = 0u; index < 6u; ++index )
				indices[indexCount++] = baseVertex + quad[index];
			draw->firstIndex = indexCount - 6u; draw->indexCount = 6u;
			draw->material = entity->customShader;
			draw->contentDigest = HashU64( plan->frontendFrameDigest, i );
			memcpy( draw->localSh, localSh, sizeof( draw->localSh ) );
		} else {
			static const uint32_t box[36] = { 0,1,2,0,2,3,4,6,5,4,7,6,
				0,4,5,0,5,1,1,5,6,1,6,2,2,6,7,2,7,3,3,7,4,3,4,0 };
			float width = entity->radius > 0.0f ? entity->radius : 1.0f;
			float dx = entity->oldorigin[0] - entity->origin[0];
			float dy = entity->oldorigin[1] - entity->origin[1];
			float px = dy != 0.0f ? -dy : width;
			float py = dx != 0.0f ? dx : 0.0f;
			entityDrawPlan_t *draw = &draws[drawCount++];
			if ( drawCount > plan->entityBatchCount
					|| indexCount > plan->entityIndexCount - 36u ) goto fail;
			for ( uint32_t end = 0u; end < 2u; ++end ) {
				const float *origin = end ? entity->oldorigin : entity->origin;
				SetEntityVertex( &vertices[vertexCount++], origin[0] - px,
					origin[1] - py, origin[2] - width, 0.0f, 0.0f,
					entity->shader.rgba );
				SetEntityVertex( &vertices[vertexCount++], origin[0] + px,
					origin[1] + py, origin[2] - width, 1.0f, 0.0f,
					entity->shader.rgba );
				SetEntityVertex( &vertices[vertexCount++], origin[0] + px,
					origin[1] + py, origin[2] + width, 1.0f, 1.0f,
					entity->shader.rgba );
				SetEntityVertex( &vertices[vertexCount++], origin[0] - px,
					origin[1] - py, origin[2] + width, 0.0f, 1.0f,
					entity->shader.rgba );
			}
			for ( uint32_t index = 0u; index < 36u; ++index )
				indices[indexCount++] = baseVertex + box[index];
			draw->firstIndex = indexCount - 36u; draw->indexCount = 36u;
			draw->material = entity->customShader;
			draw->contentDigest = HashU64( plan->frontendFrameDigest, i );
			memcpy( draw->localSh, localSh, sizeof( draw->localSh ) );
		}
	}
	if ( vertexCount != vertexCapacity || indexCount != plan->entityIndexCount
			|| drawCount != plan->entityBatchCount ) goto fail;
	*outVertices = vertices; *outVertexCount = vertexCount;
	*outIndices = indices; *outIndexCount = indexCount;
	*outDraws = draws; *outDrawCount = drawCount; return qtrue;
fail:
	free( vertices ); free( indices ); free( draws ); return qfalse;
}

typedef struct {
	ralWebGpuDrawKind_t kind;
	uint32_t firstIndex;
	uint32_t indexCount;
	qhandle_t material;
	qboolean textured;
	qboolean msdf;
	uint64_t contentDigest;
} miscDrawPlan_t;

static byte ColorByte( float value ) {
	if ( value <= 0.0f ) return 0u;
	if ( value >= 1.0f ) return 255u;
	return (byte)( value * 255.0f + 0.5f );
}

static qboolean AddCapacity( uint32_t *value, uint64_t add ) {
	if ( !value || add > UINT32_MAX - *value ) return qfalse;
	*value += (uint32_t)add; return qtrue;
}

static qboolean BuildMiscGeometry( const renderSubmissionState_t *frontend,
		const ralWebGpuFrontendPlanReceipt_t *plan,
		uint32_t viewportWidth, uint32_t viewportHeight,
		renderWorldVertex_t **outVertices, uint32_t *outVertexCount,
		uint32_t **outIndices, uint32_t *outIndexCount,
		miscDrawPlan_t **outDraws, uint32_t *outDrawCount ) {
	static const uint32_t quad[6] = { 0u, 1u, 2u, 0u, 2u, 3u };
	const renderPolyCommand_t *polygons;
	const polyVert_t *polyVertices;
	const renderLightCommand_t *lights;
	const renderUiPrimitive_t *ui;
	renderWorldVertex_t *vertices = NULL;
	uint32_t *indices = NULL;
	miscDrawPlan_t *draws = NULL;
	uint32_t polygonCommandCount = 0u, polyVertexCount = 0u;
	uint32_t lightCount = 0u, uiCount = 0u;
	uint32_t vertexCapacity = 0u, indexCapacity = 0u, drawCapacity = 0u;
	uint32_t vertexCount = 0u, indexCount = 0u, drawCount = 0u;
	if ( !frontend || !plan || !outVertices || !outVertexCount || !outIndices
			|| !outIndexCount || !outDraws || !outDrawCount
			|| !RenderSubmission_EffectSnapshots( frontend, &polygons,
				&polygonCommandCount, &polyVertices, &polyVertexCount,
				&lights, &lightCount ) ) return qfalse;
	ui = RenderSubmission_UiPrimitives( frontend, &uiCount );
	if ( polygonCommandCount != plan->polygonBatchCount
			|| lightCount != plan->lightCount || uiCount != plan->uiPrimitiveCount
			|| ( uiCount && ( !viewportWidth || !viewportHeight ) )
			|| ( polygonCommandCount && ( !polygons || !polyVertices ) )
			|| ( lightCount && !lights ) || ( uiCount && !ui ) ) return qfalse;
	for ( uint32_t i = 0u; i < polygonCommandCount; ++i ) {
		uint64_t verticesInCommand = (uint64_t)polygons[i].verticesPerPolygon
			* polygons[i].polygonCount;
		uint64_t indicesInCommand;
		if ( polygons[i].verticesPerPolygon < 3u
				|| polygons[i].firstVertex > polyVertexCount
				|| verticesInCommand > polyVertexCount - polygons[i].firstVertex )
			return qfalse;
		indicesInCommand = (uint64_t)( polygons[i].verticesPerPolygon - 2u )
			* 3u * polygons[i].polygonCount;
		if ( !AddCapacity( &vertexCapacity, verticesInCommand )
				|| !AddCapacity( &indexCapacity, indicesInCommand ) ) return qfalse;
	}
	if ( !AddCapacity( &vertexCapacity, (uint64_t)( lightCount + uiCount ) * 4u )
			|| !AddCapacity( &indexCapacity,
				(uint64_t)( lightCount + uiCount ) * 6u )
			|| !AddCapacity( &drawCapacity,
				(uint64_t)polygonCommandCount + lightCount + uiCount )
			|| drawCapacity != plan->polygonBatchCount + plan->lightCount
				+ plan->uiPrimitiveCount
			|| !vertexCapacity || !indexCapacity || !drawCapacity
			|| (uint64_t)vertexCapacity * sizeof( *vertices )
				> RAL_WEBGPU_MAX_RESOURCE_BYTES
			|| (uint64_t)indexCapacity * sizeof( *indices )
				> RAL_WEBGPU_MAX_RESOURCE_BYTES ) return qfalse;
	vertices = calloc( vertexCapacity, sizeof( *vertices ) );
	indices = calloc( indexCapacity, sizeof( *indices ) );
	draws = calloc( drawCapacity, sizeof( *draws ) );
	if ( !vertices || !indices || !draws ) goto fail;
	for ( uint32_t commandIndex = 0u; commandIndex < polygonCommandCount;
			++commandIndex ) {
		const renderPolyCommand_t *command = &polygons[commandIndex];
		miscDrawPlan_t *draw = &draws[drawCount++];
		draw->kind = RAL_WEBGPU_DRAW_EFFECT; draw->firstIndex = indexCount;
		draw->material = command->material; draw->textured = qtrue;
		for ( uint32_t polygon = 0u; polygon < command->polygonCount; ++polygon ) {
			uint32_t baseVertex = vertexCount;
			uint32_t sourceBase = command->firstVertex
				+ polygon * command->verticesPerPolygon;
			for ( uint32_t vertex = 0u; vertex < command->verticesPerPolygon;
					++vertex ) {
				const polyVert_t *source = &polyVertices[sourceBase + vertex];
				SetEntityVertex( &vertices[vertexCount++], source->xyz[0],
					source->xyz[1], source->xyz[2], source->st[0], source->st[1],
					source->modulate.rgba );
			}
			for ( uint32_t triangle = 1u;
					triangle + 1u < command->verticesPerPolygon; ++triangle ) {
				indices[indexCount++] = baseVertex;
				indices[indexCount++] = baseVertex + triangle;
				indices[indexCount++] = baseVertex + triangle + 1u;
			}
		}
		draw->indexCount = indexCount - draw->firstIndex;
		draw->contentDigest = HashU64( plan->loweringDigest,
			( (uint64_t)RAL_WEBGPU_DRAW_EFFECT << 56u ) | commandIndex + 1u );
	}
	for ( uint32_t i = 0u; i < lightCount; ++i ) {
		byte color[4] = { ColorByte( lights[i].color[0] ),
			ColorByte( lights[i].color[1] ), ColorByte( lights[i].color[2] ), 255u };
		float radius = lights[i].intensity < 4096.0f ? lights[i].intensity : 4096.0f;
		uint32_t baseVertex = vertexCount;
		miscDrawPlan_t *draw = &draws[drawCount++];
		SetEntityVertex( &vertices[vertexCount++], lights[i].origin[0] - radius,
			lights[i].origin[1] - radius, lights[i].origin[2], 0.0f, 0.0f, color );
		SetEntityVertex( &vertices[vertexCount++], lights[i].origin[0] + radius,
			lights[i].origin[1] - radius, lights[i].origin[2], 1.0f, 0.0f, color );
		SetEntityVertex( &vertices[vertexCount++], lights[i].origin[0] + radius,
			lights[i].origin[1] + radius, lights[i].origin[2], 1.0f, 1.0f, color );
		SetEntityVertex( &vertices[vertexCount++], lights[i].origin[0] - radius,
			lights[i].origin[1] + radius, lights[i].origin[2], 0.0f, 1.0f, color );
		for ( uint32_t index = 0u; index < 6u; ++index )
			indices[indexCount++] = baseVertex + quad[index];
		draw->kind = RAL_WEBGPU_DRAW_EFFECT; draw->firstIndex = indexCount - 6u;
		draw->indexCount = 6u; draw->textured = qfalse;
		draw->contentDigest = HashU64( plan->loweringDigest,
			( (uint64_t)RAL_WEBGPU_DRAW_EFFECT << 56u )
				| ( (uint64_t)1u << 48u ) | i + 1u );
	}
	for ( uint32_t i = 0u; i < uiCount; ++i ) {
		const renderUiPrimitive_t *primitive = &ui[i];
		renderMaterialSnapshot_t material;
		byte color[4]; float positions[4][2];
		float texCoords[4][2];
		uint32_t baseVertex = vertexCount;
		miscDrawPlan_t *draw = &draws[drawCount++];
		if ( !RenderSubmission_MaterialSnapshot( frontend, primitive->material,
				&material ) || !material.ready ) goto fail;
		for ( uint32_t component = 0u; component < 4u; ++component )
			color[component] = ColorByte( primitive->color[component] );
		memcpy( positions, primitive->positions, sizeof( positions ) );
		if ( primitive->kind == RENDER_UI_LINE ) {
			texCoords[0][0] = 0.0f; texCoords[0][1] = 0.0f;
			texCoords[1][0] = 1.0f; texCoords[1][1] = 0.0f;
			texCoords[2][0] = 1.0f; texCoords[2][1] = 1.0f;
			texCoords[3][0] = 0.0f; texCoords[3][1] = 1.0f;
		} else {
			texCoords[0][0] = primitive->s1; texCoords[0][1] = primitive->t1;
			texCoords[1][0] = primitive->s2; texCoords[1][1] = primitive->t1;
			texCoords[2][0] = primitive->s2; texCoords[2][1] = primitive->t2;
			texCoords[3][0] = primitive->s1; texCoords[3][1] = primitive->t2;
		}
		for ( uint32_t corner = 0u; corner < 4u; ++corner ) {
			positions[corner][0] = positions[corner][0]
				/ (float)viewportWidth * 2.0f - 1.0f;
			positions[corner][1] = 1.0f - positions[corner][1]
				/ (float)viewportHeight * 2.0f;
		}
		SetEntityVertex( &vertices[vertexCount++], positions[0][0], positions[0][1],
			0.0f, texCoords[0][0], texCoords[0][1], color );
		SetEntityVertex( &vertices[vertexCount++], positions[1][0], positions[1][1],
			0.0f, texCoords[1][0], texCoords[1][1], color );
		SetEntityVertex( &vertices[vertexCount++], positions[2][0], positions[2][1],
			0.0f, texCoords[2][0], texCoords[2][1], color );
		SetEntityVertex( &vertices[vertexCount++], positions[3][0], positions[3][1],
			0.0f, texCoords[3][0], texCoords[3][1], color );
		for ( uint32_t index = 0u; index < 6u; ++index )
			indices[indexCount++] = baseVertex + quad[index];
		draw->kind = RAL_WEBGPU_DRAW_UI; draw->firstIndex = indexCount - 6u;
		draw->indexCount = 6u; draw->material = primitive->material;
		draw->textured = qtrue; draw->msdf = material.msdf;
		draw->contentDigest = HashU64( plan->loweringDigest,
			( (uint64_t)RAL_WEBGPU_DRAW_UI << 56u ) | i + 1u );
	}
	if ( vertexCount != vertexCapacity || indexCount != indexCapacity
			|| drawCount != drawCapacity ) goto fail;
	*outVertices = vertices; *outVertexCount = vertexCount;
	*outIndices = indices; *outIndexCount = indexCount;
	*outDraws = draws; *outDrawCount = drawCount; return qtrue;
fail:
	free( vertices ); free( indices ); free( draws ); return qfalse;
}

qboolean RalWebGpu_ProductCreate( ralWebGpuRuntime_t *runtime,
		const ralWebGpuRuntimeReceipt_t *runtimeReceipt,
		const ralWebGpuPipelineReceipt_t *worldPipeline, uint64_t generation,
		ralWebGpuProduct_t **outProduct ) {
	ralWebGpuProduct_t *product;
	ralWebGpuResourceLayer_t *resources;
	ralWebGpuBufferDesc_t lightingDesc;
	if ( !runtime || !runtimeReceipt || !worldPipeline || !outProduct
			|| !generation || generation == UINT64_MAX
			|| worldPipeline->kind != RAL_SHADER_PIPELINE_GRAPHICS
			|| !RalWebGpu_RuntimeOwnsPipeline( runtime, runtimeReceipt,
				worldPipeline ) ) return qfalse;
	product = (ralWebGpuProduct_t *)calloc( 1u, sizeof( *product ) );
	if ( !product ) return qfalse;
	product->runtime = runtime; product->worldPipeline = *worldPipeline;
	product->generation = generation; product->planGeneration = generation;
	resources = RalWebGpu_RuntimeResources( runtime, runtimeReceipt );
	memset( &lightingDesc, 0, sizeof( lightingDesc ) );
	lightingDesc.size = RAL_WEBGPU_ENTITY_LIGHTING_BYTES;
	lightingDesc.usage = RAL_WEBGPU_BUFFER_STORAGE
		| RAL_WEBGPU_BUFFER_COPY_DESTINATION;
	lightingDesc.memoryClass = RAL_ALLOCATION_DEVICE_LOCAL;
	if ( !resources || !RalWebGpu_CreateBuffer( resources, &lightingDesc,
			&product->entityLighting, &product->entityLightingReceipt ) ) {
		free( product ); return qfalse;
	}
	lightingDesc.size = RAL_WEBGPU_WORLD_MATERIAL_BYTES;
	if ( !RalWebGpu_CreateBuffer( resources, &lightingDesc,
			&product->worldMaterials, &product->worldMaterialReceipt ) ) {
		RalWebGpu_DestroyResource( resources, product->entityLighting,
			&product->entityLightingReceipt );
		free( product ); return qfalse;
	}
	{
		ralAtmosphereWeatherRequest_t emptyWeather;
		memset( &emptyWeather, 0, sizeof( emptyWeather ) );
		emptyWeather.schemaVersion = RAL_ATMOSPHERE_WEATHER_SCHEMA_VERSION;
		emptyWeather.tier = RAL_ATMOSPHERE_TIER_OFF;
		emptyWeather.maxParticles = RAL_WEBGPU_WEATHER_PARTICLE_CAPACITY;
		if ( !Ral_AtmospherePlanWeather( &emptyWeather,
				&product->weatherPlan ) ) {
			RalWebGpu_DestroyResource( resources, product->worldMaterials,
				&product->worldMaterialReceipt );
			RalWebGpu_DestroyResource( resources, product->entityLighting,
				&product->entityLightingReceipt );
			free( product ); return qfalse;
		}
	}
	*outProduct = product; return qtrue;
}

qboolean RalWebGpu_ProductGetWorldMaterialBuffer(
		const ralWebGpuProduct_t *product,
		const ralWebGpuRuntimeReceipt_t *runtimeReceipt,
		uintptr_t *outIdentity, uint64_t *outByteSize ) {
	if ( !product || !runtimeReceipt || !outIdentity || !outByteSize
			|| !product->worldMaterials
			|| !RalWebGpu_ResourceReceiptExact( &product->worldMaterialReceipt,
				&product->worldMaterialReceipt )
			|| !RalWebGpu_RuntimeOwnsPipeline( product->runtime, runtimeReceipt,
				&product->worldPipeline ) ) return qfalse;
	*outIdentity = product->worldMaterialReceipt.resourceIdentity;
	*outByteSize = product->worldMaterialReceipt.byteSize;
	return *outIdentity && *outByteSize == RAL_WEBGPU_WORLD_MATERIAL_BYTES;
}

qboolean RalWebGpu_ProductSetWorldMaterialBindGroup(
		ralWebGpuProduct_t *product,
		const ralWebGpuRuntimeReceipt_t *runtimeReceipt,
		uintptr_t bindGroupIdentity ) {
	if ( !product || !runtimeReceipt || !bindGroupIdentity || product->inFlight
			|| !RalWebGpu_RuntimeOwnsPipeline( product->runtime, runtimeReceipt,
				&product->worldPipeline ) ) return qfalse;
	product->worldMaterialBindGroup = bindGroupIdentity;
	product->worldMaterialBindGroupReady = qtrue;
	return qtrue;
}

qboolean RalWebGpu_ProductSetEntityPipeline( ralWebGpuProduct_t *product,
		const ralWebGpuRuntimeReceipt_t *runtimeReceipt,
		const ralWebGpuPipelineReceipt_t *entityPipeline ) {
	if ( !product || !runtimeReceipt || !entityPipeline || product->inFlight
			|| entityPipeline->kind != RAL_SHADER_PIPELINE_GRAPHICS
			|| !RalWebGpu_RuntimeOwnsPipeline( product->runtime, runtimeReceipt,
				entityPipeline ) ) return qfalse;
	product->entityPipeline = *entityPipeline;
	product->entityPipelineReady = qtrue; return qtrue;
}

qboolean RalWebGpu_ProductGetEntityLightingBuffer(
		const ralWebGpuProduct_t *product,
		const ralWebGpuRuntimeReceipt_t *runtimeReceipt,
		uintptr_t *outIdentity, uint64_t *outByteSize ) {
	if ( !product || !runtimeReceipt || !outIdentity || !outByteSize
			|| !product->entityLighting
			|| !RalWebGpu_ResourceReceiptExact( &product->entityLightingReceipt,
				&product->entityLightingReceipt )
			|| !RalWebGpu_RuntimeOwnsPipeline( product->runtime, runtimeReceipt,
				&product->worldPipeline ) ) return qfalse;
	*outIdentity = product->entityLightingReceipt.resourceIdentity;
	*outByteSize = product->entityLightingReceipt.byteSize;
	return *outIdentity && *outByteSize == RAL_WEBGPU_ENTITY_LIGHTING_BYTES;
}

qboolean RalWebGpu_ProductSetEntityLightingBindGroup(
		ralWebGpuProduct_t *product,
		const ralWebGpuRuntimeReceipt_t *runtimeReceipt,
		uintptr_t bindGroupIdentity ) {
	if ( !product || !runtimeReceipt || !bindGroupIdentity || product->inFlight
			|| !product->entityPipelineReady
			|| !RalWebGpu_RuntimeOwnsPipeline( product->runtime, runtimeReceipt,
				&product->entityPipeline ) ) return qfalse;
	product->entityLightingBindGroup = bindGroupIdentity;
	product->entityLightingBindGroupReady = qtrue;
	return qtrue;
}

qboolean RalWebGpu_ProductSetEffectPipeline( ralWebGpuProduct_t *product,
		const ralWebGpuRuntimeReceipt_t *runtimeReceipt,
		const ralWebGpuPipelineReceipt_t *effectPipeline ) {
	if ( !product || !runtimeReceipt || !effectPipeline || product->inFlight
			|| effectPipeline->kind != RAL_SHADER_PIPELINE_GRAPHICS
			|| !RalWebGpu_RuntimeOwnsPipeline( product->runtime, runtimeReceipt,
				effectPipeline ) ) return qfalse;
	product->effectPipeline = *effectPipeline;
	product->effectPipelineReady = qtrue; return qtrue;
}

qboolean RalWebGpu_ProductSetUiPipelines( ralWebGpuProduct_t *product,
		const ralWebGpuRuntimeReceipt_t *runtimeReceipt,
		const ralWebGpuPipelineReceipt_t *uiPipeline,
		const ralWebGpuPipelineReceipt_t *msdfPipeline ) {
	if ( !product || !runtimeReceipt || !uiPipeline || !msdfPipeline
			|| product->inFlight
			|| uiPipeline->kind != RAL_SHADER_PIPELINE_GRAPHICS
			|| msdfPipeline->kind != RAL_SHADER_PIPELINE_GRAPHICS
			|| !RalWebGpu_RuntimeOwnsPipeline( product->runtime, runtimeReceipt,
				uiPipeline )
			|| !RalWebGpu_RuntimeOwnsPipeline( product->runtime, runtimeReceipt,
				msdfPipeline ) ) return qfalse;
	product->uiPipeline = *uiPipeline; product->msdfPipeline = *msdfPipeline;
	product->uiPipelinesReady = qtrue; return qtrue;
}

qboolean RalWebGpu_ProductSetAtmosphereBindGroups(
		ralWebGpuProduct_t *product,
		const ralWebGpuRuntimeReceipt_t *runtimeReceipt,
		uintptr_t worldBindGroup, uintptr_t entityBindGroup,
		uintptr_t effectBindGroup ) {
	if ( !product || !runtimeReceipt || product->inFlight || !worldBindGroup
			|| !entityBindGroup || !effectBindGroup
			|| !RalWebGpu_RuntimeOwnsPipeline( product->runtime, runtimeReceipt,
				&product->worldPipeline ) ) return qfalse;
	product->atmosphereBindGroups[0] = worldBindGroup;
	product->atmosphereBindGroups[1] = entityBindGroup;
	product->atmosphereBindGroups[2] = effectBindGroup;
	product->atmosphereBindGroupsReady = qtrue;
	return qtrue;
}

qboolean RalWebGpu_ProductSetViewport( ralWebGpuProduct_t *product,
		const ralWebGpuRuntimeReceipt_t *runtimeReceipt,
		uint32_t width, uint32_t height ) {
	if ( !product || !runtimeReceipt || !width || !height || product->inFlight
			|| !RalWebGpu_RuntimeOwnsPipeline( product->runtime, runtimeReceipt,
				&product->worldPipeline ) ) return qfalse;
	product->viewportWidth = width; product->viewportHeight = height;
	return qtrue;
}

qboolean RalWebGpu_ProductSetWeather( ralWebGpuProduct_t *product,
		const ralWebGpuRuntimeReceipt_t *runtimeReceipt,
		const ralAtmosphereWeatherReceipt_t *plan,
		const ralWebGpuWeatherReceipt_t *execution ) {
	if ( !product || !runtimeReceipt || !plan || product->inFlight
			|| !Ral_AtmosphereWeatherReceiptExact( plan, plan ) ) return qfalse;
	if ( plan->zeroWork ) {
		if ( execution ) return qfalse;
		product->weatherPlan = *plan;
		memset( &product->weatherExecution, 0,
			sizeof( product->weatherExecution ) );
		product->weatherReady = qfalse;
		return qtrue;
	}
	if ( !execution || !RalWebGpu_WeatherReceiptExact( execution, execution )
			|| !Ral_AtmosphereWeatherReceiptExact( plan, &execution->plan )
			|| !RalWebGpu_RuntimeOwnsPipeline( product->runtime, runtimeReceipt,
				&execution->renderPipeline ) ) return qfalse;
	product->weatherPlan = *plan;
	product->weatherExecution = *execution;
	product->weatherReady = qtrue;
	return qtrue;
}

void RalWebGpu_ProductDestroy( ralWebGpuProduct_t *product,
		const ralWebGpuRuntimeReceipt_t *runtimeReceipt ) {
	ralWebGpuResourceLayer_t *resources;
	if ( !product ) return;
	resources = runtimeReceipt ? RalWebGpu_RuntimeResources( product->runtime,
		runtimeReceipt ) : NULL;
	if ( resources && !product->inFlight ) {
		DestroyFrameResources( product, resources );
		for ( uint32_t i = 0u; i < product->materialCount; ++i )
			DestroyMaterial( resources, &product->materials[i] );
		if ( product->entityLighting )
			RalWebGpu_DestroyResource( resources, product->entityLighting,
				&product->entityLightingReceipt );
		if ( product->worldMaterials )
			RalWebGpu_DestroyResource( resources, product->worldMaterials,
				&product->worldMaterialReceipt );
	}
	memset( product, 0, sizeof( *product ) ); free( product );
}

qboolean RalWebGpu_ProductPlan( ralWebGpuProduct_t *product,
		const ralWebGpuRuntimeReceipt_t *runtimeReceipt,
		const renderSubmissionState_t *frontend,
		const renderSubmissionReceipt_t *submission,
		ralWebGpuFrontendPlanReceipt_t *outPlan ) {
	uint64_t generation;
	if ( !product || !runtimeReceipt || !frontend || !submission || !outPlan
			|| product->inFlight
			|| product->planGeneration >= UINT64_MAX - 1u
			|| !RalWebGpu_RuntimeOwnsPipeline( product->runtime, runtimeReceipt,
				&product->worldPipeline ) ) return qfalse;
	generation = product->planGeneration + 1u;
	if ( !RalWebGpu_FrontendPlanBuild( product->runtime, runtimeReceipt,
			frontend, submission, generation, outPlan ) ) return qfalse;
	product->planGeneration = generation; return qtrue;
}

qboolean RalWebGpu_ProductRenderPlan( ralWebGpuProduct_t *product,
		const ralWebGpuRuntimeReceipt_t *runtimeReceipt,
		const renderSubmissionState_t *frontend,
		const renderSubmissionReceipt_t *submission,
		const ralWebGpuFrontendPlanReceipt_t *plan, uintptr_t targetIdentity,
		uint64_t frameGeneration, ralWebGpuProductFrameReceipt_t *outReceipt ) {
	ralWebGpuProductFrameReceipt_t candidate;
	materialEntry_t *staged = NULL;
	materialEntry_t **resolved = NULL;
	qboolean *stagedMaterial = NULL;
	ralWebGpuResourceLayer_t *resources;
	ralWebGpuCommand_t *command;
	renderWorldSnapshot_t world;
	ralWebGpuBufferDesc_t vertexDesc, indexDesc;
	ralWebGpuResource_t *vertices = NULL, *indices = NULL;
	ralWebGpuResource_t *entityVertexResource = NULL, *entityIndexResource = NULL;
	ralWebGpuResource_t *miscVertexResource = NULL, *miscIndexResource = NULL;
	ralWebGpuResourceReceipt_t vertexReceipt, indexReceipt;
	ralWebGpuResourceReceipt_t entityVertexReceipt, entityIndexReceipt;
	ralWebGpuResourceReceipt_t miscVertexReceipt, miscIndexReceipt;
	renderWorldVertex_t *entityVertices = NULL;
	renderWorldVertex_t *miscVertices = NULL;
	renderWorldVertex_t *projectedWorld = NULL;
	uint32_t *entityIndices = NULL;
	uint32_t *miscIndices = NULL;
	entityDrawPlan_t *entityDraws = NULL;
	float *entityLightingData = NULL;
	float *worldMaterialData = NULL;
	miscDrawPlan_t *miscDraws = NULL;
	uint32_t entityVertexCount = 0u, entityIndexCount = 0u, entityDrawCount = 0u;
	uint32_t miscVertexCount = 0u, miscIndexCount = 0u, miscDrawCount = 0u;
	ralWebGpuCommandReceipt_t recording, updated, executable;
	uint32_t stagedCount = 0u, materialCount = 0u, newMaterialCount = 0u;
	qboolean commandActive = qfalse;
	s_productFailureStage = 1u;
	if ( !product || !runtimeReceipt || !frontend || !submission || !plan
			|| !outReceipt ) return qfalse;
	if ( !Ral_AtmosphereWeatherReceiptExact( &product->weatherPlan,
			&product->weatherPlan ) ) {
		s_productFailureStage = 14u; return qfalse;
	}
	if ( !product->weatherPlan.zeroWork && !product->weatherReady ) {
		s_productFailureStage = 15u; return qfalse;
	}
	if ( !targetIdentity || !frameGeneration || product->inFlight ) {
		s_productFailureStage = 11u; return qfalse;
	}
	if ( !RalWebGpu_FrontendPlanReceiptExact( plan, plan ) ) {
		s_productFailureStage = 12u; return qfalse;
	}
	if ( plan->backendGeneration != runtimeReceipt->generation ) {
		s_productFailureStage = 13u; return qfalse;
	}
	if ( plan->frontendOwnerGeneration != submission->ownerGeneration ) {
		s_productFailureStage = 17u; return qfalse;
	}
	if ( plan->frontendFrameGeneration != submission->frameGeneration ) {
		s_productFailureStage = 18u; return qfalse;
	}
	if ( plan->frontendFrameDigest != submission->frameDigest ) {
		s_productFailureStage = 19u; return qfalse;
	}
	if ( product->atmosphereBindGroupsReady != qtrue
			|| product->worldMaterialBindGroupReady != qtrue ) {
		s_productFailureStage = 20u; return qfalse;
	}
	if ( !RalWebGpu_RuntimeOwnsPipeline( product->runtime, runtimeReceipt,
			&product->worldPipeline ) ) {
		s_productFailureStage = 21u; return qfalse;
	}
	s_productFailureStage = 2u;
	resources = RalWebGpu_RuntimeResources( product->runtime, runtimeReceipt );
	command = RalWebGpu_RuntimeCommand( product->runtime, runtimeReceipt );
	if ( !resources || !command || !RenderSubmission_WorldSnapshot( frontend,
			&world ) || !world.vertexCount || !world.indexCount ) return qfalse;
	memset( &candidate, 0, sizeof( candidate ) );
	candidate.plan = *plan;
	candidate.weather = product->weatherPlan;
	memset( &vertexReceipt, 0, sizeof( vertexReceipt ) );
	memset( &indexReceipt, 0, sizeof( indexReceipt ) );
	memset( &entityVertexReceipt, 0, sizeof( entityVertexReceipt ) );
	memset( &entityIndexReceipt, 0, sizeof( entityIndexReceipt ) );
	memset( &miscVertexReceipt, 0, sizeof( miscVertexReceipt ) );
	memset( &miscIndexReceipt, 0, sizeof( miscIndexReceipt ) );
	worldMaterialData = (float *)calloc( (size_t)world.batchCount * 4u,
		sizeof( worldMaterialData[0] ) );
	if ( !worldMaterialData ) goto fail;
	for ( uint32_t i = 0u; i < world.batchCount; ++i ) {
		renderMaterialSnapshot_t material;
		if ( world.batches[i].baseMaterial <= 0
				|| !RenderSubmission_MaterialSnapshot( frontend,
					world.batches[i].baseMaterial, &material ) ) goto fail;
		for ( uint32_t channel = 0u; channel < 3u; ++channel )
			worldMaterialData[i * 4u + channel] =
				(float)material.lighting.emissionRadianceQ16[channel]
				/ (float)RENDER_MATERIAL_LIGHTING_Q16_ONE;
		if ( material.lighting.emissionRadianceQ16[0]
				|| material.lighting.emissionRadianceQ16[1]
				|| material.lighting.emissionRadianceQ16[2] )
			candidate.worldEmissiveDrawCount++;
	}
	if ( !RalWebGpu_WriteBuffer( resources, product->worldMaterials,
			&product->worldMaterialReceipt, 0u, worldMaterialData,
			(uint64_t)world.batchCount * 4u * sizeof( float ),
			&candidate.worldMaterialWrite ) ) goto fail;
	candidate.worldMaterialBuffer = product->worldMaterialReceipt;
	s_productFailureStage = 4u;
	materialCount = frontend->materialCount;
	if ( materialCount > RENDER_SUBMISSION_MAX_MATERIALS ) goto fail;
	if ( materialCount ) {
		staged = (materialEntry_t *)calloc( materialCount, sizeof( *staged ) );
		resolved = (materialEntry_t **)calloc( materialCount,
			sizeof( *resolved ) );
		stagedMaterial = (qboolean *)calloc( materialCount,
			sizeof( *stagedMaterial ) );
		if ( !staged || !resolved || !stagedMaterial ) goto fail;
	}
	if ( candidate.plan.entityBatchCount
			&& ( product->entityPipelineReady != qtrue
				|| product->entityLightingBindGroupReady != qtrue
				|| !RalWebGpu_RuntimeOwnsPipeline( product->runtime, runtimeReceipt,
					&product->entityPipeline )
				|| !BuildEntityGeometry( frontend, &candidate.plan, &entityVertices,
					&entityVertexCount, &entityIndices, &entityIndexCount,
					&entityDraws, &entityDrawCount )
				|| !ProjectVertices( &world, entityVertices,
					entityVertexCount ) ) ) goto fail;
	if ( entityDrawCount ) {
		if ( entityDrawCount > RAL_WEBGPU_ENTITY_LIGHTING_CAPACITY ) goto fail;
		entityLightingData = calloc( (size_t)entityDrawCount * 16u,
			sizeof( entityLightingData[0] ) );
		if ( !entityLightingData ) goto fail;
		for ( uint32_t i = 0u; i < entityDrawCount; ++i ) {
			memcpy( &entityLightingData[i * 16u], entityDraws[i].localSh,
				16u * sizeof( float ) );
			if ( entityDraws[i].localSh[0][3] > 0.5f )
				candidate.localIrradianceDrawCount++;
		}
		candidate.localIrradianceEntityCount =
			candidate.plan.localIrradianceEntityCount;
		candidate.entityLightingBuffer = product->entityLightingReceipt;
		if ( !RalWebGpu_WriteBuffer( resources, product->entityLighting,
				&product->entityLightingReceipt, 0u, entityLightingData,
				(uint64_t)entityDrawCount * 16u * sizeof( float ),
				&candidate.entityLightingWrite ) ) goto fail;
	}
	if ( candidate.plan.polygonBatchCount || candidate.plan.lightCount
			|| candidate.plan.uiPrimitiveCount ) {
		if ( ( ( candidate.plan.polygonBatchCount || candidate.plan.lightCount )
					&& ( product->effectPipelineReady != qtrue
						|| !RalWebGpu_RuntimeOwnsPipeline( product->runtime,
							runtimeReceipt, &product->effectPipeline ) ) )
				|| ( candidate.plan.uiPrimitiveCount
					&& ( product->uiPipelinesReady != qtrue
						|| !RalWebGpu_RuntimeOwnsPipeline( product->runtime,
							runtimeReceipt, &product->uiPipeline )
						|| !RalWebGpu_RuntimeOwnsPipeline( product->runtime,
							runtimeReceipt, &product->msdfPipeline ) ) )
				|| !BuildMiscGeometry( frontend, &candidate.plan,
					product->viewportWidth, product->viewportHeight, &miscVertices,
					&miscVertexCount, &miscIndices, &miscIndexCount, &miscDraws,
					&miscDrawCount ) ) goto fail;
		if ( !ProjectVertices( &world, miscVertices,
				miscVertexCount - candidate.plan.uiPrimitiveCount * 4u ) ) goto fail;
	}
	s_productFailureStage = 5u;
	for ( uint32_t i = 0u; i < materialCount; ++i ) {
		const renderMaterialSnapshot_t *material = &frontend->materials[i].snapshot;
		materialEntry_t *cached = FindMaterial( product, material->handle );
		if ( cached && cached->generation == material->generation
				&& cached->digest == material->digest ) {
			resolved[i] = cached; candidate.reusedMaterialCount++; continue;
		}
		if ( !cached && product->materialCount + ++newMaterialCount
				> RENDER_SUBMISSION_MAX_MATERIALS ) goto fail;
		if ( stagedCount >= RENDER_SUBMISSION_MAX_MATERIALS
				|| !UploadMaterial( resources, material, &staged[stagedCount] ) )
			goto fail;
		resolved[i] = &staged[stagedCount];
		stagedMaterial[i] = qtrue;
		stagedCount++; candidate.uploadedMaterialCount++;
	}
	s_productFailureStage = 6u; memset( &vertexDesc, 0, sizeof( vertexDesc ) );
	projectedWorld = (renderWorldVertex_t *)malloc(
		(uint64_t)world.vertexCount * sizeof( projectedWorld[0] ) );
	if ( !projectedWorld ) goto fail;
	memcpy( projectedWorld, world.vertices,
		(uint64_t)world.vertexCount * sizeof( projectedWorld[0] ) );
	if ( !ProjectVertices( &world, projectedWorld, world.vertexCount ) ) goto fail;
	vertexDesc.size = (uint64_t)world.vertexCount * sizeof( world.vertices[0] );
	vertexDesc.usage = RAL_WEBGPU_BUFFER_VERTEX | RAL_WEBGPU_BUFFER_COPY_DESTINATION;
	vertexDesc.memoryClass = RAL_ALLOCATION_DEVICE_LOCAL;
	memset( &indexDesc, 0, sizeof( indexDesc ) );
	indexDesc.size = (uint64_t)world.indexCount * sizeof( world.indices[0] );
	indexDesc.usage = RAL_WEBGPU_BUFFER_INDEX | RAL_WEBGPU_BUFFER_COPY_DESTINATION;
	indexDesc.memoryClass = RAL_ALLOCATION_DEVICE_LOCAL;
	if ( !RalWebGpu_CreateBuffer( resources, &vertexDesc, &vertices,
			&vertexReceipt ) || !RalWebGpu_WriteBuffer( resources, vertices,
			&vertexReceipt, 0u, projectedWorld, vertexDesc.size,
			&candidate.worldVertexWrite )
			|| !RalWebGpu_CreateBuffer( resources, &indexDesc, &indices,
				&indexReceipt ) || !RalWebGpu_WriteBuffer( resources, indices,
			&indexReceipt, 0u, world.indices, indexDesc.size,
			&candidate.worldIndexWrite ) ) goto fail;
	if ( entityDrawCount ) {
		memset( &vertexDesc, 0, sizeof( vertexDesc ) );
		vertexDesc.size = (uint64_t)entityVertexCount * sizeof( entityVertices[0] );
		vertexDesc.usage = RAL_WEBGPU_BUFFER_VERTEX | RAL_WEBGPU_BUFFER_COPY_DESTINATION;
		vertexDesc.memoryClass = RAL_ALLOCATION_DEVICE_LOCAL;
		memset( &indexDesc, 0, sizeof( indexDesc ) );
		indexDesc.size = (uint64_t)entityIndexCount * sizeof( entityIndices[0] );
		indexDesc.usage = RAL_WEBGPU_BUFFER_INDEX | RAL_WEBGPU_BUFFER_COPY_DESTINATION;
		indexDesc.memoryClass = RAL_ALLOCATION_DEVICE_LOCAL;
		if ( !RalWebGpu_CreateBuffer( resources, &vertexDesc,
				&entityVertexResource, &entityVertexReceipt )
				|| !RalWebGpu_WriteBuffer( resources, entityVertexResource,
					&entityVertexReceipt, 0u, entityVertices, vertexDesc.size,
					&candidate.entityVertexWrite )
				|| !RalWebGpu_CreateBuffer( resources, &indexDesc,
					&entityIndexResource, &entityIndexReceipt )
				|| !RalWebGpu_WriteBuffer( resources, entityIndexResource,
					&entityIndexReceipt, 0u, entityIndices, indexDesc.size,
					&candidate.entityIndexWrite ) ) goto fail;
	}
	if ( miscDrawCount ) {
		memset( &vertexDesc, 0, sizeof( vertexDesc ) );
		vertexDesc.size = (uint64_t)miscVertexCount * sizeof( miscVertices[0] );
		vertexDesc.usage = RAL_WEBGPU_BUFFER_VERTEX | RAL_WEBGPU_BUFFER_COPY_DESTINATION;
		vertexDesc.memoryClass = RAL_ALLOCATION_DEVICE_LOCAL;
		memset( &indexDesc, 0, sizeof( indexDesc ) );
		indexDesc.size = (uint64_t)miscIndexCount * sizeof( miscIndices[0] );
		indexDesc.usage = RAL_WEBGPU_BUFFER_INDEX | RAL_WEBGPU_BUFFER_COPY_DESTINATION;
		indexDesc.memoryClass = RAL_ALLOCATION_DEVICE_LOCAL;
		if ( !RalWebGpu_CreateBuffer( resources, &vertexDesc,
				&miscVertexResource, &miscVertexReceipt )
				|| !RalWebGpu_WriteBuffer( resources, miscVertexResource,
					&miscVertexReceipt, 0u, miscVertices, vertexDesc.size,
					&candidate.miscVertexWrite )
				|| !RalWebGpu_CreateBuffer( resources, &indexDesc,
					&miscIndexResource, &miscIndexReceipt )
				|| !RalWebGpu_WriteBuffer( resources, miscIndexResource,
					&miscIndexReceipt, 0u, miscIndices, indexDesc.size,
					&candidate.miscIndexWrite ) ) goto fail;
	}
	s_productFailureStage = 7u;
	if ( !RalWebGpu_CommandBegin( command, RAL_WEBGPU_PASS_RENDER,
			targetIdentity, &recording ) ) goto fail;
	commandActive = qtrue;
	s_productFailureStage = 8u;
	for ( uint32_t i = 0u; i < world.batchCount; ++i ) {
		const renderWorldBatch_t *batch = &world.batches[i];
		materialEntry_t *base = NULL, *lightmap = NULL;
		ralWebGpuIndexedDraw_t draw;
		for ( uint32_t m = 0u; m < materialCount; ++m ) {
			if ( frontend->materials[m].snapshot.handle == batch->baseMaterial )
				base = resolved[m];
			if ( batch->lightmapMaterial > 0
					&& frontend->materials[m].snapshot.handle
						== batch->lightmapMaterial ) lightmap = resolved[m];
		}
		if ( !base ) { s_productFailureStage = 81u; goto fail; }
		if ( batch->lightmapMaterial > 0 && !lightmap ) {
			s_productFailureStage = 82u; goto fail;
		}
		if ( batch->firstIndex > world.indexCount
				|| batch->indexCount > world.indexCount - batch->firstIndex ) {
			s_productFailureStage = 83u; goto fail;
		}
		memset( &draw, 0, sizeof( draw ) ); draw.kind = RAL_WEBGPU_DRAW_WORLD;
		draw.pipelineIdentity = product->worldPipeline.pipelineIdentity;
		draw.vertexBufferIdentity = vertexReceipt.resourceIdentity;
		draw.indexBufferIdentity = indexReceipt.resourceIdentity;
		draw.textureIdentity = base->textureReceipt.resourceIdentity;
		draw.secondaryTextureIdentity = lightmap
			? lightmap->textureReceipt.resourceIdentity : (uintptr_t)0;
		draw.samplerIdentity = base->samplerReceipt.resourceIdentity;
		draw.textured = qtrue;
		draw.firstIndex = batch->firstIndex; draw.indexCount = batch->indexCount;
		draw.instanceCount = 1u;
		draw.firstInstance = i;
		draw.contentDigest = HashU64( candidate.plan.loweringDigest,
			( (uint64_t)batch->sourceSurfaceIndex << 32u ) | batch->indexCount );
		draw.bindGroupCount = 2u;
		draw.bindGroupIdentities[0] = product->atmosphereBindGroups[0];
		draw.bindGroupIdentities[1] = product->worldMaterialBindGroup;
		s_productFailureStage = 84u;
		if ( !RalWebGpu_CommandRecordIndexedDraw( command, &recording, &draw,
				&updated ) ) goto fail;
		s_productFailureStage = 8u;
		recording = updated; candidate.worldDrawCount++;
		candidate.atmosphereBoundDrawCount++;
	}
	s_productFailureStage = 9u;
	for ( uint32_t i = 0u; i < entityDrawCount; ++i ) {
		materialEntry_t *material = NULL;
		ralWebGpuIndexedDraw_t draw;
		for ( uint32_t m = 0u; m < materialCount; ++m )
			if ( frontend->materials[m].snapshot.handle == entityDraws[i].material )
				material = resolved[m];
		if ( !material ) goto fail;
		memset( &draw, 0, sizeof( draw ) ); draw.kind = RAL_WEBGPU_DRAW_ENTITY;
		draw.pipelineIdentity = product->entityPipeline.pipelineIdentity;
		draw.vertexBufferIdentity = entityVertexReceipt.resourceIdentity;
		draw.indexBufferIdentity = entityIndexReceipt.resourceIdentity;
		draw.textureIdentity = material->textureReceipt.resourceIdentity;
		draw.samplerIdentity = material->samplerReceipt.resourceIdentity;
		draw.textured = qtrue;
		draw.firstIndex = entityDraws[i].firstIndex;
		draw.indexCount = entityDraws[i].indexCount;
		draw.instanceCount = 1u; draw.contentDigest = entityDraws[i].contentDigest;
		draw.firstInstance = i;
		draw.bindGroupCount = 2u;
		draw.bindGroupIdentities[0] = product->atmosphereBindGroups[1];
		draw.bindGroupIdentities[1] = product->entityLightingBindGroup;
		if ( !RalWebGpu_CommandRecordIndexedDraw( command, &recording, &draw,
				&updated ) ) goto fail;
		recording = updated; candidate.entityDrawCount++;
		candidate.atmosphereBoundDrawCount++;
	}
	if ( !candidate.weather.zeroWork ) {
		ralWebGpuIndexedDraw_t draw;
		s_productFailureStage = 95u;
		memset( &draw, 0, sizeof( draw ) );
		draw.kind = RAL_WEBGPU_DRAW_EFFECT;
		draw.pipelineIdentity =
			product->weatherExecution.renderPipeline.pipelineIdentity;
		draw.vertexBufferIdentity =
			product->weatherExecution.vertexBufferIdentity;
		draw.indexBufferIdentity =
			product->weatherExecution.indexBufferIdentity;
		draw.textured = qfalse;
		draw.indexCount = product->weatherExecution.drawIndexCount;
		draw.instanceCount = candidate.weather.activeParticleCount;
		draw.contentDigest = HashU64( candidate.plan.loweringDigest,
			( (uint64_t)candidate.weather.familyMask << 32u )
				| candidate.weather.activeParticleCount );
		if ( !draw.contentDigest ) draw.contentDigest = 1u;
		draw.bindGroupCount = 1u;
		draw.bindGroupIdentities[0] =
			product->weatherExecution.renderBindGroupIdentity;
		if ( !RalWebGpu_CommandRecordIndexedDraw( command, &recording, &draw,
				&updated ) ) goto fail;
		recording = updated; candidate.weatherDrawCount = 1u;
	}
	s_productFailureStage = 10u;
	for ( uint32_t i = 0u; i < miscDrawCount; ++i ) {
		materialEntry_t *material = NULL;
		ralWebGpuIndexedDraw_t draw;
		if ( miscDraws[i].textured ) {
			for ( uint32_t m = 0u; m < materialCount; ++m )
				if ( frontend->materials[m].snapshot.handle
						== miscDraws[i].material ) material = resolved[m];
			if ( !material ) goto fail;
		}
		memset( &draw, 0, sizeof( draw ) ); draw.kind = miscDraws[i].kind;
		draw.pipelineIdentity = miscDraws[i].kind == RAL_WEBGPU_DRAW_UI
			? ( miscDraws[i].msdf ? product->msdfPipeline.pipelineIdentity
				: product->uiPipeline.pipelineIdentity )
			: product->effectPipeline.pipelineIdentity;
		draw.vertexBufferIdentity = miscVertexReceipt.resourceIdentity;
		draw.indexBufferIdentity = miscIndexReceipt.resourceIdentity;
		draw.textured = miscDraws[i].textured;
		if ( material ) {
			draw.textureIdentity = material->textureReceipt.resourceIdentity;
			draw.samplerIdentity = material->samplerReceipt.resourceIdentity;
		}
		draw.firstIndex = miscDraws[i].firstIndex;
		draw.indexCount = miscDraws[i].indexCount;
		draw.instanceCount = 1u; draw.contentDigest = miscDraws[i].contentDigest;
		if ( miscDraws[i].kind != RAL_WEBGPU_DRAW_UI ) {
			draw.bindGroupCount = 1u;
			draw.bindGroupIdentities[0] = product->atmosphereBindGroups[2];
		}
		if ( !RalWebGpu_CommandRecordIndexedDraw( command, &recording, &draw,
				&updated ) ) goto fail;
		recording = updated;
		if ( i < candidate.plan.polygonBatchCount ) {
			candidate.polygonDrawCount++; candidate.atmosphereBoundDrawCount++;
		}
		else if ( i < candidate.plan.polygonBatchCount + candidate.plan.lightCount )
			{ candidate.lightDrawCount++; candidate.atmosphereBoundDrawCount++; }
		else candidate.uiDrawCount++;
	}
	s_productFailureStage = 11u;
	if ( !RalWebGpu_CommandEnd( command, &recording, &executable ) ) {
		commandActive = qfalse; goto fail;
	}
	recording = executable;
	s_productFailureStage = 12u;
	if ( !RalWebGpu_CommandSubmit( command, &executable,
			&candidate.submission ) ) goto fail;
	commandActive = qfalse;
	for ( uint32_t i = 0u, stagedIndex = 0u; i < materialCount; ++i ) {
		if ( !stagedMaterial[i] ) continue;
		materialEntry_t *cached = FindMaterial( product, resolved[i]->handle );
		if ( cached ) DestroyMaterial( resources, cached );
		else cached = &product->materials[product->materialCount++];
		*cached = *resolved[i]; memset( &staged[stagedIndex++], 0,
			sizeof( staged[0] ) );
	}
	DestroyFrameResources( product, resources );
	product->worldVertices = vertices; product->worldIndices = indices;
	product->entityVertices = entityVertexResource;
	product->entityIndices = entityIndexResource;
	product->miscVertices = miscVertexResource;
	product->miscIndices = miscIndexResource;
	product->worldVertexReceipt = vertexReceipt;
	product->worldIndexReceipt = indexReceipt;
	product->entityVertexReceipt = entityVertexReceipt;
	product->entityIndexReceipt = entityIndexReceipt;
	product->miscVertexReceipt = miscVertexReceipt;
	product->miscIndexReceipt = miscIndexReceipt;
	candidate.schemaVersion = RAL_WEBGPU_PRODUCT_SCHEMA_VERSION;
	candidate.backendType = RAL_BACKEND_WEBGPU;
	candidate.backendGeneration = runtimeReceipt->generation;
	candidate.productGeneration = product->generation;
	candidate.frameGeneration = frameGeneration;
	candidate.targetIdentity = targetIdentity;
	candidate.worldVertexBuffer = vertexReceipt;
	candidate.worldIndexBuffer = indexReceipt;
	if ( entityDrawCount ) {
		candidate.entityVertexBuffer = entityVertexReceipt;
		candidate.entityIndexBuffer = entityIndexReceipt;
	}
	if ( miscDrawCount ) {
		candidate.miscVertexBuffer = miscVertexReceipt;
		candidate.miscIndexBuffer = miscIndexReceipt;
	}
	candidate.command = executable;
	candidate.modelEntityCount = candidate.plan.modelEntityCount;
	candidate.primitiveEntityCount = candidate.plan.primitiveEntityCount;
	candidate.temporalEntityCount = candidate.plan.temporalEntityCount;
	candidate.deferredNonWorldDrawCount = candidate.plan.drawCount
		- candidate.worldDrawCount - candidate.entityDrawCount
		- candidate.polygonDrawCount - candidate.lightDrawCount
		- candidate.uiDrawCount;
	candidate.ready = qtrue;
	s_productFailureStage = 13u;
	if ( !ReceiptValid( &candidate ) ) goto fail_committed;
	free( entityVertices ); free( entityIndices ); free( entityDraws );
	free( entityLightingData );
	free( worldMaterialData );
	free( miscVertices ); free( miscIndices ); free( miscDraws );
	free( projectedWorld );
	free( stagedMaterial ); free( resolved ); free( staged );
	product->published = candidate; product->inFlight = qtrue;
	s_productFailureStage = 0u; *outReceipt = candidate; return qtrue;

fail:
	if ( commandActive ) RalWebGpu_CommandCancel( command, &recording );
	free( entityVertices ); free( entityIndices ); free( entityDraws );
	free( entityLightingData );
	free( worldMaterialData );
	free( miscVertices ); free( miscIndices ); free( miscDraws );
	free( projectedWorld );
	for ( uint32_t i = 0u; i < stagedCount; ++i )
		DestroyMaterial( resources, &staged[i] );
	free( stagedMaterial ); free( resolved ); free( staged );
	if ( indices ) RalWebGpu_DestroyResource( resources, indices, &indexReceipt );
	if ( vertices ) RalWebGpu_DestroyResource( resources, vertices, &vertexReceipt );
	if ( entityIndexResource ) RalWebGpu_DestroyResource( resources,
		entityIndexResource, &entityIndexReceipt );
	if ( entityVertexResource ) RalWebGpu_DestroyResource( resources,
		entityVertexResource, &entityVertexReceipt );
	if ( miscIndexResource ) RalWebGpu_DestroyResource( resources,
		miscIndexResource, &miscIndexReceipt );
	if ( miscVertexResource ) RalWebGpu_DestroyResource( resources,
		miscVertexResource, &miscVertexReceipt );
	return qfalse;
fail_committed:
	free( entityVertices ); free( entityIndices ); free( entityDraws );
	free( entityLightingData );
	free( worldMaterialData );
	free( miscVertices ); free( miscIndices ); free( miscDraws );
	free( projectedWorld );
	free( stagedMaterial ); free( resolved ); free( staged );
	product->worldVertices = vertices; product->worldIndices = indices;
	product->worldVertexReceipt = vertexReceipt;
	product->worldIndexReceipt = indexReceipt;
	product->entityVertices = entityVertexResource;
	product->entityIndices = entityIndexResource;
	product->entityVertexReceipt = entityVertexReceipt;
	product->entityIndexReceipt = entityIndexReceipt;
	product->miscVertices = miscVertexResource;
	product->miscIndices = miscIndexResource;
	product->miscVertexReceipt = miscVertexReceipt;
	product->miscIndexReceipt = miscIndexReceipt;
	product->published = candidate; product->inFlight = qtrue;
	return qfalse;
}

qboolean RalWebGpu_ProductRender( ralWebGpuProduct_t *product,
		const ralWebGpuRuntimeReceipt_t *runtimeReceipt,
		const renderSubmissionState_t *frontend,
		const renderSubmissionReceipt_t *submission, uintptr_t targetIdentity,
		uint64_t frameGeneration, ralWebGpuProductFrameReceipt_t *outReceipt ) {
	ralWebGpuFrontendPlanReceipt_t plan;
	if ( !RalWebGpu_ProductPlan( product, runtimeReceipt, frontend, submission,
			&plan ) ) {
		s_productFailureStage = 16u; return qfalse;
	}
	return RalWebGpu_ProductRenderPlan( product, runtimeReceipt, frontend,
		submission, &plan, targetIdentity, frameGeneration, outReceipt );
}

ralWebGpuAsyncStatus_t RalWebGpu_ProductPoll( ralWebGpuProduct_t *product,
		const ralWebGpuProductFrameReceipt_t *receipt ) {
	ralWebGpuCommand_t *command;
	ralWebGpuResourceLayer_t *resources;
	ralWebGpuRuntimeReceipt_t runtimeReceipt;
	ralWebGpuAsyncStatus_t status;
	if ( !product || !receipt || !product->inFlight
			|| !RalWebGpu_ProductFrameReceiptExact( receipt,
				&product->published )
			|| !RalWebGpu_RuntimeGetReceipt( product->runtime, &runtimeReceipt ) )
		return RAL_WEBGPU_ASYNC_FAILED;
	command = RalWebGpu_RuntimeCommand( product->runtime, &runtimeReceipt );
	resources = RalWebGpu_RuntimeResources( product->runtime, &runtimeReceipt );
	if ( !command || !resources ) return RAL_WEBGPU_ASYNC_DEVICE_LOST;
	status = RalWebGpu_CommandPoll( command, &receipt->submission );
	if ( status == RAL_WEBGPU_ASYNC_READY ) {
		DestroyFrameResources( product, resources ); product->inFlight = qfalse;
	} else if ( status != RAL_WEBGPU_ASYNC_PENDING ) {
		DestroyFrameResources( product, resources ); product->inFlight = qfalse;
	}
	return status;
}
