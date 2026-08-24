// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "ral_metal_present.h"
#include "ral_metal_internal.h"
#include "ral_metal_ui_metallib.h"
#include "render_submission.h"
#include "render_submission_material.h"
#include "render_submission_model.h"
#include "render_submission_ui.h"
#include "render_submission_world.h"

#import <CoreGraphics/CoreGraphics.h>
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#import <dispatch/dispatch.h>

#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
	qhandle_t handle;
	uint64_t generation;
	uint64_t digest;
	id<MTLTexture> texture;
} ralMetalMaterialCacheEntry_t;

struct ralMetalPresent_s {
	ralMetalCore_t *core;
	CAMetalLayer *layer;
	id<CAMetalDrawable> drawable;
	ralMetalPresentLayerReceipt_t receipt;
	ralMetalDrawableReceipt_t drawableReceipt;
	uint64_t lastAcquireGeneration;
	uint64_t lastPresentGeneration;
	id<MTLLibrary> uiLibrary;
	id<MTLRenderPipelineState> uiPipeline;
	id<MTLRenderPipelineState> uiMsdfPipeline;
	id<MTLRenderPipelineState> worldPipeline;
	id<MTLRenderPipelineState> worldBlendPipeline;
	id<MTLDepthStencilState> worldDepthState;
	id<MTLDepthStencilState> worldDepthReadState;
	id<MTLBuffer> worldVertexCache;
	id<MTLBuffer> worldIndexCache;
	id<MTLTexture> worldDepthCache;
	float *worldBatchBounds;
	uint64_t worldCacheDigest;
	uint32_t worldCacheVertexCount;
	uint32_t worldCacheIndexCount;
	uint32_t worldDepthWidth;
	uint32_t worldDepthHeight;
	id<MTLTexture> uiFallbackTexture;
	id<MTLSamplerState> uiClampSampler;
	id<MTLSamplerState> uiRepeatSampler;
	ralMetalMaterialCacheEntry_t uiMaterials[RENDER_SUBMISSION_MAX_MATERIALS];
	uint32_t uiMaterialCount;
	MTLPixelFormat uiPixelFormat;
	MTLPixelFormat worldPixelFormat;
	byte *captureRgb;
	uint32_t captureWidth;
	uint32_t captureHeight;
	qboolean capturePending;
	qboolean captureReady;
	id<MTLBuffer> readbackBuffers[3];
	id<MTLCommandBuffer> readbackCommands[3];
	uint64_t readbackGenerations[3];
	uint32_t readbackByteCounts[3];
	uint32_t nextReadbackSlot;
	uint64_t lastReadbackGeneration;
	uint8_t lastReadbackBytes[8];
	uint32_t lastReadbackByteCount;
	qboolean lastReadbackReady;
	qboolean ownsLayer;
};

typedef struct {
	float position[2];
	float color[4];
	float uv[2];
} ralMetalUiVertex_t;

typedef struct {
	float worldPosition[4];
	float color[4];
	float texCoord[2];
	float lightmapCoord[2];
} ralMetalWorldVertex_t;

typedef struct {
	float viewOrigin[4];
	float forward[4];
	float left[4];
	float up[4];
	float projection[4];
} ralMetalWorldViewParams_t;

typedef struct {
	uint32_t hasLightmap;
	uint32_t alphaMode;
	float alphaCutoff;
} ralMetalWorldMaterialParams_t;

typedef struct {
	uint32_t firstIndex;
	uint32_t indexCount;
	qhandle_t material;
	renderAlphaMode_t alphaMode;
	float alphaCutoff;
	qboolean depthWrite;
} ralMetalEntityBatch_t;

#define RAL_METAL_READBACK_RING_SIZE 3u

static qboolean ReapReadbackRing( ralMetalPresent_t *present, qboolean waitAll ) {
	qboolean ok = qtrue;
	if ( !present ) return qfalse;
	for ( uint32_t slot = 0u; slot < RAL_METAL_READBACK_RING_SIZE; ++slot ) {
		id<MTLCommandBuffer> command = present->readbackCommands[slot];
		MTLCommandBufferStatus status;
		if ( !command ) continue;
		if ( waitAll ) [command waitUntilCompleted];
		status = command.status;
		if ( status == MTLCommandBufferStatusError ) ok = qfalse;
		if ( status == MTLCommandBufferStatusCompleted
				|| status == MTLCommandBufferStatusError ) {
			if ( status == MTLCommandBufferStatusCompleted
					&& present->readbackGenerations[slot]
						> present->lastReadbackGeneration ) {
				present->lastReadbackByteCount = present->readbackByteCounts[slot];
				memcpy( present->lastReadbackBytes,
					[present->readbackBuffers[slot] contents],
					present->lastReadbackByteCount );
				present->lastReadbackGeneration = present->readbackGenerations[slot];
				present->lastReadbackReady = qtrue;
			}
			[command release]; present->readbackCommands[slot] = nil;
			present->readbackByteCounts[slot] = 0u;
			present->readbackGenerations[slot] = 0u;
		}
	}
	return ok;
}

static qboolean ReserveReadbackSlot( ralMetalPresent_t *present,
		uint32_t *outSlot, id<MTLBuffer> *outBuffer ) {
	uint32_t slot;
	if ( !present || !outSlot || !outBuffer
			|| !ReapReadbackRing( present, qfalse ) ) return qfalse;
	for ( uint32_t offset = 0u; offset < RAL_METAL_READBACK_RING_SIZE; ++offset ) {
		slot = ( present->nextReadbackSlot + offset ) % RAL_METAL_READBACK_RING_SIZE;
		if ( !present->readbackCommands[slot] ) goto found;
	}
	/* Bound CPU/GPU distance instead of growing transient readback ownership. */
	slot = present->nextReadbackSlot;
	[present->readbackCommands[slot] waitUntilCompleted];
	if ( !ReapReadbackRing( present, qfalse )
			|| present->readbackCommands[slot] ) return qfalse;
found:
	if ( !present->readbackBuffers[slot] ) {
		present->readbackBuffers[slot] = [RalMetal_CoreNativeDevice( present->core )
			newBufferWithLength:256u options:MTLResourceStorageModeShared];
		if ( !present->readbackBuffers[slot] ) return qfalse;
	}
	present->nextReadbackSlot = ( slot + 1u ) % RAL_METAL_READBACK_RING_SIZE;
	*outSlot = slot;
	*outBuffer = [present->readbackBuffers[slot] retain];
	return qtrue;
}

static qboolean ReadbackRingPending( const ralMetalPresent_t *present ) {
	if ( !present ) return qfalse;
	for ( uint32_t slot = 0u; slot < RAL_METAL_READBACK_RING_SIZE; ++slot )
		if ( present->readbackCommands[slot] ) return qtrue;
	return qfalse;
}

static float Clamp01( float value ) {
	if ( value < 0.0f ) return 0.0f;
	if ( value > 1.0f ) return 1.0f;
	return value;
}

static void UiVertex( ralMetalUiVertex_t *vertex, float x, float y,
		float s, float t, uint32_t width, uint32_t height,
		const float color[4] ) {
	vertex->position[0] = x / (float)width * 2.0f - 1.0f;
	vertex->position[1] = 1.0f - y / (float)height * 2.0f;
	for ( uint32_t i = 0u; i < 4u; ++i ) vertex->color[i] = Clamp01( color[i] );
	vertex->uv[0] = s; vertex->uv[1] = t;
}

static qboolean BuildUiVertices( const renderSubmissionState_t *frontend,
		uint32_t width, uint32_t height, ralMetalUiVertex_t **outVertices,
		uint32_t *outPrimitiveCount, uint32_t *outReadbackX,
		uint32_t *outReadbackY ) {
	const renderUiPrimitive_t *primitives;
	ralMetalUiVertex_t *vertices;
	uint32_t count;
	if ( !outVertices || !outPrimitiveCount || !outReadbackX || !outReadbackY
			|| !width || !height ) return qfalse;
	*outVertices = NULL; *outPrimitiveCount = 0u;
	*outReadbackX = width / 2u; *outReadbackY = height / 2u;
	if ( !frontend ) return qtrue;
	primitives = RenderSubmission_UiPrimitives( frontend, &count );
	if ( !primitives || count > RENDER_SUBMISSION_MAX_UI_PRIMITIVES ) return qfalse;
	if ( count == 0u ) return qtrue;
	vertices = (ralMetalUiVertex_t *)calloc( (size_t)count * 6u, sizeof( *vertices ) );
	if ( !vertices ) return qfalse;
	for ( uint32_t i = 0u; i < count; ++i ) {
		const renderUiPrimitive_t *primitive = &primitives[i];
		float corners[4][2];
		float uvs[4][2] = {
			{ primitive->s1, primitive->t1 }, { primitive->s2, primitive->t1 },
			{ primitive->s2, primitive->t2 }, { primitive->s1, primitive->t2 }
		};
		if ( primitive->kind == RENDER_UI_LINE ) {
			float dx = primitive->width - primitive->x;
			float dy = primitive->height - primitive->y;
			float length = sqrtf( dx * dx + dy * dy );
			float halfWidth = primitive->s1 * 0.5f;
			if ( !isfinite( length ) || length <= 0.0f || !isfinite( halfWidth ) ) {
				free( vertices ); return qfalse;
			}
			float nx = -dy / length * halfWidth;
			float ny = dx / length * halfWidth;
			corners[0][0] = primitive->x + nx; corners[0][1] = primitive->y + ny;
			corners[1][0] = primitive->width + nx; corners[1][1] = primitive->height + ny;
			corners[2][0] = primitive->width - nx; corners[2][1] = primitive->height - ny;
			corners[3][0] = primitive->x - nx; corners[3][1] = primitive->y - ny;
		} else {
			float centerX = primitive->x + primitive->width * 0.5f;
			float centerY = primitive->y + primitive->height * 0.5f;
			float radians = primitive->rotation * 0.01745329251994329577f;
			float cosine = cosf( radians ), sine = sinf( radians );
			float local[4][2] = {
				{ -primitive->width * 0.5f, -primitive->height * 0.5f },
				{  primitive->width * 0.5f, -primitive->height * 0.5f },
				{  primitive->width * 0.5f,  primitive->height * 0.5f },
				{ -primitive->width * 0.5f,  primitive->height * 0.5f }
			};
			for ( uint32_t corner = 0u; corner < 4u; ++corner ) {
				corners[corner][0] = centerX + local[corner][0] * cosine
					- local[corner][1] * sine;
				corners[corner][1] = centerY + local[corner][0] * sine
					+ local[corner][1] * cosine;
			}
		}
		static const uint8_t order[6] = { 0u, 1u, 2u, 0u, 2u, 3u };
		for ( uint32_t vertex = 0u; vertex < 6u; ++vertex ) {
			uint32_t corner = order[vertex];
			UiVertex( &vertices[i * 6u + vertex], corners[corner][0],
				corners[corner][1], uvs[corner][0], uvs[corner][1], width,
				height, primitive->color );
		}
	}
	{
		const renderUiPrimitive_t *first = &primitives[0];
		float centerX = first->kind == RENDER_UI_LINE
			? ( first->x + first->width ) * 0.5f : first->x + first->width * 0.5f;
		float centerY = first->kind == RENDER_UI_LINE
			? ( first->y + first->height ) * 0.5f : first->y + first->height * 0.5f;
		if ( isfinite( centerX ) && isfinite( centerY ) ) {
			*outReadbackX = (uint32_t)fmaxf( 0.0f,
				fminf( (float)( width - 1u ), centerX ) );
			*outReadbackY = (uint32_t)fmaxf( 0.0f,
				fminf( (float)( height - 1u ), centerY ) );
		}
	}
	*outVertices = vertices; *outPrimitiveCount = count;
	return qtrue;
}

static qboolean EnsureUiPipeline( ralMetalPresent_t *present ) {
	id<MTLDevice> device;
	id<MTLFunction> vertexFunction = nil, fragmentFunction = nil, msdfFunction = nil;
	NSError *error = nil;
	if ( !present || !present->drawable ) return qfalse;
	if ( present->uiPipeline && present->uiMsdfPipeline
			&& present->uiPixelFormat == present->drawable.texture.pixelFormat ) return qtrue;
	[present->uiPipeline release]; present->uiPipeline = nil;
	[present->uiMsdfPipeline release]; present->uiMsdfPipeline = nil;
	device = RalMetal_CoreNativeDevice( present->core );
	if ( !present->uiLibrary ) {
		dispatch_data_t data = dispatch_data_create( ral_metal_ui_metallib,
			ral_metal_ui_metallib_size,
			dispatch_get_global_queue( QOS_CLASS_DEFAULT, 0 ), ^{} );
		if ( !data ) return qfalse;
		present->uiLibrary = [device newLibraryWithData:data error:&error];
		if ( !present->uiLibrary || error ) return qfalse;
	}
	vertexFunction = [present->uiLibrary newFunctionWithName:@"wired_ui_vertex"];
	fragmentFunction = [present->uiLibrary newFunctionWithName:@"wired_ui_fragment"];
	msdfFunction = [present->uiLibrary newFunctionWithName:@"wired_ui_msdf_fragment"];
	if ( !vertexFunction || !fragmentFunction || !msdfFunction ) goto fail;
	{
		MTLVertexDescriptor *vertices = [[[MTLVertexDescriptor alloc] init] autorelease];
		vertices.attributes[0].format = MTLVertexFormatFloat2;
		vertices.attributes[0].offset = 0u; vertices.attributes[0].bufferIndex = 0u;
		vertices.attributes[1].format = MTLVertexFormatFloat4;
		vertices.attributes[1].offset = 8u; vertices.attributes[1].bufferIndex = 0u;
		vertices.attributes[2].format = MTLVertexFormatFloat2;
		vertices.attributes[2].offset = 24u; vertices.attributes[2].bufferIndex = 0u;
		vertices.layouts[0].stride = sizeof( ralMetalUiVertex_t );
		vertices.layouts[0].stepFunction = MTLVertexStepFunctionPerVertex;
		MTLRenderPipelineDescriptor *pipeline =
			[[[MTLRenderPipelineDescriptor alloc] init] autorelease];
		pipeline.vertexFunction = vertexFunction; pipeline.fragmentFunction = fragmentFunction;
		pipeline.vertexDescriptor = vertices;
		pipeline.colorAttachments[0].pixelFormat = present->drawable.texture.pixelFormat;
		pipeline.colorAttachments[0].blendingEnabled = YES;
		pipeline.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorSourceAlpha;
		pipeline.colorAttachments[0].destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
		pipeline.colorAttachments[0].sourceAlphaBlendFactor = MTLBlendFactorOne;
		pipeline.colorAttachments[0].destinationAlphaBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
		present->uiPipeline = [device newRenderPipelineStateWithDescriptor:pipeline error:&error];
		if ( present->uiPipeline && !error ) {
			pipeline.fragmentFunction = msdfFunction;
			present->uiMsdfPipeline = [device
				newRenderPipelineStateWithDescriptor:pipeline error:&error];
		}
	}
	[vertexFunction release]; vertexFunction = nil;
	[fragmentFunction release]; fragmentFunction = nil;
	[msdfFunction release]; msdfFunction = nil;
	if ( !present->uiPipeline || !present->uiMsdfPipeline || error ) goto fail;
	present->uiPixelFormat = present->drawable.texture.pixelFormat;
	return qtrue;
fail:
	[vertexFunction release]; [fragmentFunction release]; [msdfFunction release];
	[present->uiPipeline release]; present->uiPipeline = nil;
	[present->uiMsdfPipeline release]; present->uiMsdfPipeline = nil;
	return qfalse;
}

static qboolean EnsureUiSampling( ralMetalPresent_t *present ) {
	id<MTLDevice> device;
	if ( !present ) return qfalse;
	device = RalMetal_CoreNativeDevice( present->core );
	if ( !present->uiFallbackTexture ) {
		static const uint8_t white[4] = { 255u, 255u, 255u, 255u };
		MTLTextureDescriptor *descriptor = [MTLTextureDescriptor
			texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
			width:1u height:1u mipmapped:NO];
		descriptor.storageMode = MTLStorageModeShared;
		descriptor.usage = MTLTextureUsageShaderRead;
		present->uiFallbackTexture = [device newTextureWithDescriptor:descriptor];
		if ( !present->uiFallbackTexture ) return qfalse;
		[present->uiFallbackTexture replaceRegion:MTLRegionMake2D( 0u, 0u, 1u, 1u )
			mipmapLevel:0u withBytes:white bytesPerRow:4u];
	}
	if ( !present->uiClampSampler || !present->uiRepeatSampler ) {
		MTLSamplerDescriptor *clamp = [[[MTLSamplerDescriptor alloc] init] autorelease];
		clamp.minFilter = MTLSamplerMinMagFilterLinear;
		clamp.magFilter = MTLSamplerMinMagFilterLinear;
		clamp.sAddressMode = MTLSamplerAddressModeClampToEdge;
		clamp.tAddressMode = MTLSamplerAddressModeClampToEdge;
		MTLSamplerDescriptor *repeat = [[[MTLSamplerDescriptor alloc] init] autorelease];
		repeat.minFilter = MTLSamplerMinMagFilterLinear;
		repeat.magFilter = MTLSamplerMinMagFilterLinear;
		repeat.sAddressMode = MTLSamplerAddressModeRepeat;
		repeat.tAddressMode = MTLSamplerAddressModeRepeat;
		if ( !present->uiClampSampler )
			present->uiClampSampler = [device newSamplerStateWithDescriptor:clamp];
		if ( !present->uiRepeatSampler )
			present->uiRepeatSampler = [device newSamplerStateWithDescriptor:repeat];
		if ( !present->uiClampSampler || !present->uiRepeatSampler ) return qfalse;
	}
	return qtrue;
}

static qboolean UiMaterial( ralMetalPresent_t *present,
		const renderSubmissionState_t *frontend, qhandle_t handle,
		id<MTLTexture> *outTexture, id<MTLSamplerState> *outSampler,
		qboolean *outTextured, qboolean *outMsdf ) {
	renderMaterialSnapshot_t snapshot;
	ralMetalMaterialCacheEntry_t *entry = NULL;
	uint32_t i;
	if ( !present || !outTexture || !outSampler || !outTextured || !outMsdf
			|| !EnsureUiSampling( present ) ) return qfalse;
	*outTexture = present->uiFallbackTexture;
	*outSampler = present->uiClampSampler;
	*outTextured = qfalse;
	*outMsdf = qfalse;
	if ( !frontend || !RenderSubmission_MaterialSnapshot( frontend, handle,
			&snapshot ) || snapshot.ready != qtrue ) return qtrue;
	if ( !snapshot.rgba8 || !snapshot.width || !snapshot.height
			|| snapshot.rowBytes != snapshot.width * 4u
			|| (uint64_t)snapshot.byteCount
				!= (uint64_t)snapshot.rowBytes * snapshot.height
			|| snapshot.generation == 0u || snapshot.digest == 0u ) return qfalse;
	for ( i = 0u; i < RENDER_SUBMISSION_MAX_MATERIALS; ++i ) {
		ralMetalMaterialCacheEntry_t *candidate = &present->uiMaterials[
			( (uint32_t)handle + i ) % RENDER_SUBMISSION_MAX_MATERIALS];
		if ( candidate->handle == handle ) { entry = candidate; break; }
		if ( candidate->handle == 0 ) { entry = candidate; break; }
	}
	if ( entry && ( entry->generation != snapshot.generation
			|| entry->digest != snapshot.digest ) && entry->handle == handle ) {
		[entry->texture release]; entry->texture = nil;
		entry->generation = 0u; entry->digest = 0u;
	}
	if ( !entry ) return qfalse;
	if ( entry->handle == 0 ) {
		if ( present->uiMaterialCount >= RENDER_SUBMISSION_MAX_MATERIALS ) return qfalse;
		entry->handle = handle; present->uiMaterialCount++;
	}
	if ( !entry->texture ) {
		MTLTextureDescriptor *descriptor = [MTLTextureDescriptor
			texture2DDescriptorWithPixelFormat:snapshot.srgb
				? MTLPixelFormatRGBA8Unorm_sRGB : MTLPixelFormatRGBA8Unorm
			width:snapshot.width height:snapshot.height mipmapped:NO];
		descriptor.storageMode = MTLStorageModeShared;
		descriptor.usage = MTLTextureUsageShaderRead;
		entry->texture = [RalMetal_CoreNativeDevice( present->core )
			newTextureWithDescriptor:descriptor];
		if ( !entry->texture ) return qfalse;
		[entry->texture replaceRegion:MTLRegionMake2D( 0u, 0u, snapshot.width,
			snapshot.height ) mipmapLevel:0u withBytes:snapshot.rgba8
			bytesPerRow:snapshot.rowBytes];
		entry->generation = snapshot.generation;
		entry->digest = snapshot.digest;
	}
	*outTexture = entry->texture;
	*outSampler = snapshot.clampToEdge ? present->uiClampSampler
		: present->uiRepeatSampler;
	*outTextured = qtrue;
	*outMsdf = snapshot.msdf;
	return qtrue;
}

static qboolean BuildWorldVertices( const renderSubmissionState_t *frontend,
		ralMetalWorldVertex_t **outVertices, const uint32_t **outIndices,
		const renderWorldBatch_t **outBatches, uint32_t *outVertexCount,
		uint32_t *outIndexCount, uint32_t *outBatchCount ) {
	renderWorldSnapshot_t snapshot;
	ralMetalWorldVertex_t *vertices;
	if ( !outVertices || !outIndices || !outBatches || !outVertexCount
			|| !outIndexCount || !outBatchCount ) return qfalse;
	*outVertices = NULL; *outIndices = NULL; *outBatches = NULL;
	*outVertexCount = *outIndexCount = *outBatchCount = 0u;
	if ( !frontend || !RenderSubmission_WorldSnapshot( frontend, &snapshot ) ) return qtrue;
	if ( !snapshot.vertices || !snapshot.indices || !snapshot.batches
			|| !snapshot.vertexCount || !snapshot.indexCount || !snapshot.batchCount
			|| snapshot.indexCount % 3u
			|| snapshot.vertexCount > RENDER_SUBMISSION_MAX_WORLD_VERTICES
			|| snapshot.indexCount > RENDER_SUBMISSION_MAX_WORLD_INDICES
			|| snapshot.batchCount > RENDER_SUBMISSION_MAX_WORLD_BATCHES
			|| !isfinite( snapshot.fovX ) || !isfinite( snapshot.fovY )
			|| snapshot.fovX <= 1.0f || snapshot.fovX >= 179.0f
			|| snapshot.fovY <= 1.0f || snapshot.fovY >= 179.0f ) return qfalse;
	vertices = (ralMetalWorldVertex_t *)calloc( snapshot.vertexCount,
		sizeof( *vertices ) );
	if ( !vertices ) return qfalse;
	for ( uint32_t i = 0u; i < snapshot.vertexCount; ++i ) {
		memcpy( vertices[i].worldPosition, snapshot.vertices[i].position,
			3u * sizeof( float ) );
		vertices[i].worldPosition[3] = 1.0f;
		for ( uint32_t channel = 0u; channel < 4u; ++channel )
			vertices[i].color[channel] = snapshot.vertices[i].color[channel] / 255.0f;
		memcpy( vertices[i].texCoord, snapshot.vertices[i].texCoord,
			sizeof( vertices[i].texCoord ) );
		memcpy( vertices[i].lightmapCoord, snapshot.vertices[i].lightmapCoord,
			sizeof( vertices[i].lightmapCoord ) );
	}
	for ( uint32_t batch = 0u; batch < snapshot.batchCount; ++batch ) {
		const renderWorldBatch_t *draw = &snapshot.batches[batch];
		if ( !draw->indexCount || draw->indexCount % 3u
				|| draw->firstIndex > snapshot.indexCount - draw->indexCount
				|| draw->alphaMode < RENDER_ALPHA_OPAQUE
				|| draw->alphaMode > RENDER_ALPHA_BLEND
				|| !isfinite( draw->alphaCutoff ) || draw->alphaCutoff < 0.0f
				|| draw->alphaCutoff > 1.0f
				|| ( draw->depthWrite != qfalse && draw->depthWrite != qtrue ) ) {
			free( vertices ); return qfalse;
		}
	}
	*outVertices = vertices; *outIndices = snapshot.indices;
	*outBatches = snapshot.batches;
	*outVertexCount = snapshot.vertexCount; *outIndexCount = snapshot.indexCount;
	*outBatchCount = snapshot.batchCount;
	return qtrue;
}

static qboolean PrepareWorldBuffers( ralMetalPresent_t *present,
		const renderSubmissionState_t *frontend, id<MTLBuffer> *outVertices,
		id<MTLBuffer> *outIndices, const renderWorldBatch_t **outBatches,
		uint32_t *outVertexCount, uint32_t *outIndexCount,
		uint32_t *outBatchCount ) {
	renderWorldSnapshot_t snapshot;
	ralMetalWorldVertex_t *vertexBytes = NULL;
	const uint32_t *indexBytes = NULL;
	const renderWorldBatch_t *batches = NULL;
	id<MTLBuffer> vertices = nil, indices = nil;
	float *bounds = NULL;
	uint32_t vertexCount = 0u, indexCount = 0u, batchCount = 0u;
	if ( !present || !outVertices || !outIndices || !outBatches
			|| !outVertexCount || !outIndexCount || !outBatchCount ) return qfalse;
	*outVertices = nil; *outIndices = nil; *outBatches = NULL;
	*outVertexCount = *outIndexCount = *outBatchCount = 0u;
	if ( !frontend ) return qtrue;
	if ( !RenderSubmission_WorldSnapshot( frontend, &snapshot ) ) return qtrue;
	if ( !snapshot.vertices || !snapshot.indices || !snapshot.batches
			|| !snapshot.vertexCount || !snapshot.indexCount || !snapshot.batchCount
			|| !frontend->worldDigest ) return qfalse;
	if ( present->worldCacheDigest != frontend->worldDigest
			|| present->worldCacheVertexCount != snapshot.vertexCount
			|| present->worldCacheIndexCount != snapshot.indexCount
			|| !present->worldVertexCache || !present->worldIndexCache
			|| !present->worldBatchBounds ) {
		if ( !BuildWorldVertices( frontend, &vertexBytes, &indexBytes, &batches,
				&vertexCount, &indexCount, &batchCount ) ) return qfalse;
		vertices = [RalMetal_CoreNativeDevice( present->core )
			newBufferWithBytes:vertexBytes length:(NSUInteger)vertexCount
				* sizeof( *vertexBytes ) options:MTLResourceStorageModeShared];
		indices = [RalMetal_CoreNativeDevice( present->core )
			newBufferWithBytes:indexBytes length:(NSUInteger)indexCount
				* sizeof( *indexBytes ) options:MTLResourceStorageModeShared];
		free( vertexBytes );
		if ( !vertices || !indices ) {
			[vertices release]; [indices release]; return qfalse;
		}
		bounds = (float *)malloc( (size_t)batchCount * 6u * sizeof( float ) );
		if ( !bounds ) { [vertices release]; [indices release]; return qfalse; }
		for ( uint32_t batchIndex = 0u; batchIndex < batchCount; ++batchIndex ) {
			const renderWorldBatch_t *batch = &batches[batchIndex];
			float *box = bounds + (size_t)batchIndex * 6u;
			for ( uint32_t axis = 0u; axis < 3u; ++axis ) {
				box[axis] = INFINITY; box[axis + 3u] = -INFINITY;
			}
			for ( uint32_t offset = 0u; offset < batch->indexCount; ++offset ) {
				uint32_t vertex = indexBytes[batch->firstIndex + offset];
				if ( vertex >= snapshot.vertexCount ) {
					free( bounds ); [vertices release]; [indices release]; return qfalse;
				}
				for ( uint32_t axis = 0u; axis < 3u; ++axis ) {
					float value = snapshot.vertices[vertex].position[axis];
					if ( value < box[axis] ) box[axis] = value;
					if ( value > box[axis + 3u] ) box[axis + 3u] = value;
				}
			}
		}
		[present->worldVertexCache release];
		[present->worldIndexCache release];
		free( present->worldBatchBounds );
		present->worldVertexCache = vertices;
		present->worldIndexCache = indices;
		present->worldBatchBounds = bounds;
		present->worldCacheDigest = frontend->worldDigest;
		present->worldCacheVertexCount = vertexCount;
		present->worldCacheIndexCount = indexCount;
	} else {
		vertexCount = snapshot.vertexCount;
		indexCount = snapshot.indexCount;
		batchCount = snapshot.batchCount;
		batches = snapshot.batches;
	}
	*outVertices = [present->worldVertexCache retain];
	*outIndices = [present->worldIndexCache retain];
	*outBatches = batches;
	*outVertexCount = vertexCount; *outIndexCount = indexCount;
	*outBatchCount = batchCount;
	return qtrue;
}

static qboolean WorldBatchVisible( const ralMetalWorldViewParams_t *view,
		const float bounds[6] ) {
	float center[3], extent[3], tanX, tanY;
	float planes[5][4];
	if ( !view || !bounds ) return qfalse;
	for ( uint32_t axis = 0u; axis < 3u; ++axis ) {
		center[axis] = ( bounds[axis] + bounds[axis + 3u] ) * 0.5f;
		extent[axis] = ( bounds[axis + 3u] - bounds[axis] ) * 0.5f;
	}
	tanX = 1.0f / view->projection[0];
	tanY = 1.0f / view->projection[1];
	for ( uint32_t axis = 0u; axis < 3u; ++axis ) {
		planes[0][axis] = view->forward[axis];
		planes[1][axis] = view->forward[axis] * tanX + view->left[axis];
		planes[2][axis] = view->forward[axis] * tanX - view->left[axis];
		planes[3][axis] = view->forward[axis] * tanY + view->up[axis];
		planes[4][axis] = view->forward[axis] * tanY - view->up[axis];
	}
	for ( uint32_t plane = 0u; plane < 5u; ++plane ) {
		float distance = 0.0f, radius = 0.0f;
		for ( uint32_t axis = 0u; axis < 3u; ++axis ) {
			distance += planes[plane][axis]
				* ( center[axis] - view->viewOrigin[axis] );
			radius += fabsf( planes[plane][axis] ) * extent[axis];
		}
		if ( plane == 0u ) distance -= view->projection[2];
		if ( distance + radius < 0.0f ) return qfalse;
	}
	return qtrue;
}

static void EntityVertex( ralMetalWorldVertex_t *vertex,
		const renderWorldSnapshot_t *view, const float world[3], float s, float t,
		const color4ub_t color ) {
	(void)view;
	memcpy( vertex->worldPosition, world, 3u * sizeof( float ) );
	vertex->worldPosition[3] = 1.0f;
	for ( uint32_t channel = 0u; channel < 4u; ++channel )
		vertex->color[channel] = color.rgba[channel] / 255.0f;
	vertex->texCoord[0] = s; vertex->texCoord[1] = t;
	vertex->lightmapCoord[0] = vertex->lightmapCoord[1] = 0.0f;
}

static qboolean BuildWorldViewParams( const renderSubmissionState_t *frontend,
		ralMetalWorldViewParams_t *outParams ) {
	renderWorldSnapshot_t view;
	float tanX, tanY;
	if ( !frontend || !outParams
			|| !RenderSubmission_WorldSnapshot( frontend, &view )
			|| !isfinite( view.fovX ) || !isfinite( view.fovY )
			|| view.fovX <= 1.0f || view.fovX >= 179.0f
			|| view.fovY <= 1.0f || view.fovY >= 179.0f ) return qfalse;
	tanX = tanf( view.fovX * 0.00872664625997164788f );
	tanY = tanf( view.fovY * 0.00872664625997164788f );
	memset( outParams, 0, sizeof( *outParams ) );
	memcpy( outParams->viewOrigin, view.viewOrigin, 3u * sizeof( float ) );
	memcpy( outParams->forward, view.viewAxis[0], 3u * sizeof( float ) );
	memcpy( outParams->left, view.viewAxis[1], 3u * sizeof( float ) );
	memcpy( outParams->up, view.viewAxis[2], 3u * sizeof( float ) );
	outParams->projection[0] = 1.0f / tanX;
	outParams->projection[1] = 1.0f / tanY;
	outParams->projection[2] = 4.0f;
	return qtrue;
}

static qboolean EntityBatchMaterial( const renderSubmissionState_t *frontend,
		qhandle_t material, ralMetalEntityBatch_t *batch ) {
	renderMaterialSnapshot_t snapshot;
	batch->material = material; batch->alphaMode = RENDER_ALPHA_OPAQUE;
	batch->alphaCutoff = 0.5f; batch->depthWrite = qtrue;
	if ( material > 0 && RenderSubmission_MaterialSnapshot( frontend, material,
			&snapshot ) ) {
		batch->alphaMode = snapshot.alphaMode;
		batch->alphaCutoff = snapshot.alphaCutoff;
		batch->depthWrite = snapshot.depthWrite;
	}
	return qtrue;
}

static qboolean BuildEntityVertices( const renderSubmissionState_t *frontend,
		ralMetalWorldVertex_t **outVertices, uint32_t **outIndices,
		ralMetalEntityBatch_t **outBatches, uint32_t *outVertexCount,
		uint32_t *outIndexCount, uint32_t *outBatchCount,
		uint32_t *outModelCount, uint32_t *outPrimitiveCount,
		uint32_t *outTemporalCount, uint32_t *outUnresolvedCount ) {
	const renderEntityCommand_t *commands;
	renderWorldSnapshot_t view;
	uint32_t commandCount, vertexCount = 0u, indexCount = 0u, batchCount = 0u;
	if ( !outVertices || !outIndices || !outBatches || !outVertexCount
			|| !outIndexCount || !outBatchCount || !outModelCount
			|| !outPrimitiveCount || !outTemporalCount || !outUnresolvedCount ) return qfalse;
	*outVertices = NULL; *outIndices = NULL; *outBatches = NULL;
	*outVertexCount = *outIndexCount = *outBatchCount = 0u;
	*outModelCount = *outPrimitiveCount = *outTemporalCount = *outUnresolvedCount = 0u;
	commands = RenderSubmission_EntityCommands( frontend, &commandCount );
	if ( !commands || !commandCount ) return qtrue;
	if ( !RenderSubmission_WorldSnapshot( frontend, &view ) ) return qfalse;
	for ( uint32_t i = 0u; i < commandCount; ++i ) {
		const refEntity_t *entity = &commands[i].entity;
		if ( commands[i].hasTemporal ) ( *outTemporalCount )++;
		if ( entity->reType == RT_MODEL ) {
			renderModelSnapshot_t model;
			if ( entity->hModel <= 0 ) continue;
			if ( !RenderSubmission_ModelSnapshot( frontend, entity->hModel, &model ) ) {
				( *outUnresolvedCount )++; continue;
			}
			if ( model.vertexCount > RENDER_SUBMISSION_MAX_WORLD_VERTICES - vertexCount
					|| model.indexCount > RENDER_SUBMISSION_MAX_WORLD_INDICES - indexCount
					|| model.batchCount > RENDER_SUBMISSION_MAX_WORLD_BATCHES - batchCount )
				return qfalse;
			vertexCount += model.vertexCount; indexCount += model.indexCount;
			batchCount += model.batchCount; ( *outModelCount )++;
		} else if ( entity->reType == RT_SPRITE || entity->reType == RT_BEAM ) {
			if ( entity->reType == RT_BEAM ) {
				const float x = entity->oldorigin[0] - entity->origin[0];
				const float y = entity->oldorigin[1] - entity->origin[1];
				const float z = entity->oldorigin[2] - entity->origin[2];
				if ( x * x + y * y + z * z <= 0.00000001f ) continue;
			}
			const uint32_t primitiveVertices = entity->reType == RT_SPRITE ? 4u : 14u;
			const uint32_t primitiveIndices = entity->reType == RT_SPRITE ? 6u : 36u;
			if ( vertexCount > RENDER_SUBMISSION_MAX_WORLD_VERTICES - primitiveVertices
					|| indexCount > RENDER_SUBMISSION_MAX_WORLD_INDICES - primitiveIndices
					|| batchCount >= RENDER_SUBMISSION_MAX_WORLD_BATCHES ) return qfalse;
			vertexCount += primitiveVertices; indexCount += primitiveIndices;
			batchCount++; ( *outPrimitiveCount )++;
		}
	}
	if ( !indexCount ) return qtrue;
	*outVertices = (ralMetalWorldVertex_t *)calloc( vertexCount, sizeof( **outVertices ) );
	*outIndices = (uint32_t *)calloc( indexCount, sizeof( **outIndices ) );
	*outBatches = (ralMetalEntityBatch_t *)calloc( batchCount, sizeof( **outBatches ) );
	if ( !*outVertices || !*outIndices || !*outBatches ) {
		free( *outVertices ); free( *outIndices ); free( *outBatches );
		*outVertices = NULL; *outIndices = NULL; *outBatches = NULL; return qfalse;
	}
	uint32_t vertexBase = 0u, indexBase = 0u, batchBase = 0u;
	for ( uint32_t i = 0u; i < commandCount; ++i ) {
		const refEntity_t *entity = &commands[i].entity;
		if ( entity->reType == RT_MODEL ) {
			renderModelSnapshot_t model;
			if ( !RenderSubmission_ModelSnapshot( frontend, entity->hModel, &model ) ) continue;
			uint32_t frame = entity->frame < 0 ? 0u : (uint32_t)entity->frame % model.frameCount;
			uint32_t oldFrame = entity->oldframe < 0 ? frame
				: (uint32_t)entity->oldframe % model.frameCount;
			float oldWeight = entity->backlerp, newWeight = 1.0f - oldWeight;
			for ( uint32_t vertexIndex = 0u; vertexIndex < model.vertexCount; ++vertexIndex ) {
				float local[3], world[3];
				for ( uint32_t axis = 0u; axis < 3u; ++axis ) {
					float current = model.positions[((size_t)frame * model.vertexCount
						+ vertexIndex ) * 3u + axis];
					float old = model.positions[((size_t)oldFrame * model.vertexCount
						+ vertexIndex ) * 3u + axis];
					local[axis] = current * newWeight + old * oldWeight;
				}
				for ( uint32_t axis = 0u; axis < 3u; ++axis )
					world[axis] = entity->origin[axis] + entity->axis[0][axis] * local[0]
						+ entity->axis[1][axis] * local[1] + entity->axis[2][axis] * local[2];
				EntityVertex( &(*outVertices)[vertexBase + vertexIndex], &view, world,
					model.texCoords[vertexIndex * 2u], model.texCoords[vertexIndex * 2u + 1u],
					entity->shader );
			}
			for ( uint32_t index = 0u; index < model.indexCount; ++index )
				(*outIndices)[indexBase + index] = vertexBase + model.indices[index];
			for ( uint32_t batchIndex = 0u; batchIndex < model.batchCount; ++batchIndex ) {
				ralMetalEntityBatch_t *batch = &(*outBatches)[batchBase++];
				batch->firstIndex = indexBase + model.batches[batchIndex].firstIndex;
				batch->indexCount = model.batches[batchIndex].indexCount;
				EntityBatchMaterial( frontend, entity->customShader > 0
					? entity->customShader : model.batches[batchIndex].material, batch );
			}
			vertexBase += model.vertexCount; indexBase += model.indexCount;
		} else if ( entity->reType == RT_SPRITE || entity->reType == RT_BEAM ) {
			if ( entity->reType == RT_SPRITE ) {
				float corners[4][3], left[3], up[3];
				const float radians = entity->rotation * (float)M_PI / 180.0f;
				const float sine = sinf( radians ), cosine = cosf( radians );
				for ( uint32_t axis = 0u; axis < 3u; ++axis ) {
					left[axis] = ( view.viewAxis[1][axis] * cosine
						- view.viewAxis[2][axis] * sine ) * entity->radius;
					up[axis] = ( view.viewAxis[2][axis] * cosine
						+ view.viewAxis[1][axis] * sine ) * entity->radius;
					corners[0][axis] = entity->origin[axis] + left[axis] + up[axis];
					corners[1][axis] = entity->origin[axis] - left[axis] + up[axis];
					corners[2][axis] = entity->origin[axis] - left[axis] - up[axis];
					corners[3][axis] = entity->origin[axis] + left[axis] - up[axis];
				}
				static const float uv[4][2] = { { 0, 0 }, { 1, 0 }, { 1, 1 }, { 0, 1 } };
				static const uint32_t order[6] = { 0u, 1u, 2u, 0u, 2u, 3u };
				for ( uint32_t corner = 0u; corner < 4u; ++corner )
					EntityVertex( &(*outVertices)[vertexBase + corner], &view, corners[corner],
						uv[corner][0], uv[corner][1], entity->shader );
				for ( uint32_t index = 0u; index < 6u; ++index )
					(*outIndices)[indexBase + index] = vertexBase + order[index];
				vertexBase += 4u; indexBase += 6u;
			} else {
				float delta[3], normalized[3], perpendicular[3];
				for ( uint32_t axis = 0u; axis < 3u; ++axis )
					delta[axis] = entity->oldorigin[axis] - entity->origin[axis];
				float length = sqrtf( delta[0] * delta[0] + delta[1] * delta[1]
					+ delta[2] * delta[2] );
				if ( length <= 0.0001f ) continue;
				for ( uint32_t axis = 0u; axis < 3u; ++axis ) normalized[axis] = delta[axis] / length;
				const float seed[3] = { fabsf( normalized[2] ) < 0.9f ? 0.0f : 1.0f,
					0.0f, fabsf( normalized[2] ) < 0.9f ? 1.0f : 0.0f };
				perpendicular[0] = normalized[1] * seed[2] - normalized[2] * seed[1];
				perpendicular[1] = normalized[2] * seed[0] - normalized[0] * seed[2];
				perpendicular[2] = normalized[0] * seed[1] - normalized[1] * seed[0];
				length = sqrtf( perpendicular[0] * perpendicular[0]
					+ perpendicular[1] * perpendicular[1]
					+ perpendicular[2] * perpendicular[2] );
				for ( uint32_t segment = 0u; segment <= 6u; ++segment ) {
					const float angle = (float)segment * ( 2.0f * (float)M_PI / 6.0f );
					const float cosine = cosf( angle ), sine = sinf( angle );
					float side[3], start[3], end[3];
					for ( uint32_t axis = 0u; axis < 3u; ++axis ) {
						const uint32_t next = ( axis + 1u ) % 3u;
						const uint32_t after = ( axis + 2u ) % 3u;
						const float cross = normalized[next] * perpendicular[after]
							- normalized[after] * perpendicular[next];
						side[axis] = ( perpendicular[axis] / length * cosine
							+ cross / length * sine ) * 4.0f;
						start[axis] = entity->origin[axis] + side[axis];
						end[axis] = entity->oldorigin[axis] + side[axis];
					}
					EntityVertex( &(*outVertices)[vertexBase + segment * 2u], &view,
						start, 0.0f, (float)segment / 6.0f, entity->shader );
					EntityVertex( &(*outVertices)[vertexBase + segment * 2u + 1u], &view,
						end, 1.0f, (float)segment / 6.0f, entity->shader );
				}
				for ( uint32_t segment = 0u; segment < 6u; ++segment ) {
					const uint32_t first = vertexBase + segment * 2u;
					const uint32_t next = first + 2u;
					const uint32_t beamOrder[6] = { first, first + 1u, next,
						next, first + 1u, next + 1u };
					memcpy( *outIndices + indexBase + segment * 6u, beamOrder,
						sizeof( beamOrder ) );
				}
				vertexBase += 14u; indexBase += 36u;
			}
			ralMetalEntityBatch_t *batch = &(*outBatches)[batchBase++];
			batch->firstIndex = entity->reType == RT_SPRITE ? indexBase - 6u : indexBase - 36u;
			batch->indexCount = entity->reType == RT_SPRITE ? 6u : 36u;
			EntityBatchMaterial( frontend, entity->customShader, batch );
		}
	}
	*outVertexCount = vertexCount; *outIndexCount = indexCount; *outBatchCount = batchCount;
	return qtrue;
}

static qboolean EnsureWorldPipeline( ralMetalPresent_t *present ) {
	id<MTLDevice> device;
	id<MTLFunction> vertexFunction = nil, fragmentFunction = nil;
	NSError *error = nil;
	if ( !present || !present->drawable ) return qfalse;
	if ( present->worldPipeline && present->worldBlendPipeline
			&& present->worldDepthState && present->worldDepthReadState
			&& present->worldPixelFormat == present->drawable.texture.pixelFormat ) return qtrue;
	[present->worldPipeline release]; present->worldPipeline = nil;
	[present->worldBlendPipeline release]; present->worldBlendPipeline = nil;
	[present->worldDepthState release]; present->worldDepthState = nil;
	[present->worldDepthReadState release]; present->worldDepthReadState = nil;
	device = RalMetal_CoreNativeDevice( present->core );
	if ( !present->uiLibrary ) {
		dispatch_data_t data = dispatch_data_create( ral_metal_ui_metallib,
			ral_metal_ui_metallib_size,
			dispatch_get_global_queue( QOS_CLASS_DEFAULT, 0 ), ^{} );
		if ( !data ) return qfalse;
		present->uiLibrary = [device newLibraryWithData:data error:&error];
		if ( !present->uiLibrary || error ) return qfalse;
	}
	vertexFunction = [present->uiLibrary newFunctionWithName:@"wired_world_vertex"];
	fragmentFunction = [present->uiLibrary newFunctionWithName:@"wired_world_fragment"];
	if ( !vertexFunction || !fragmentFunction ) goto fail;
	{
		MTLVertexDescriptor *vertices = [[[MTLVertexDescriptor alloc] init] autorelease];
		vertices.attributes[0].format = MTLVertexFormatFloat4;
		vertices.attributes[0].offset = 0u; vertices.attributes[0].bufferIndex = 0u;
		vertices.attributes[1].format = MTLVertexFormatFloat4;
		vertices.attributes[1].offset = 16u; vertices.attributes[1].bufferIndex = 0u;
		vertices.attributes[2].format = MTLVertexFormatFloat2;
		vertices.attributes[2].offset = 32u; vertices.attributes[2].bufferIndex = 0u;
		vertices.attributes[3].format = MTLVertexFormatFloat2;
		vertices.attributes[3].offset = 40u; vertices.attributes[3].bufferIndex = 0u;
		vertices.layouts[0].stride = sizeof( ralMetalWorldVertex_t );
		MTLRenderPipelineDescriptor *pipeline =
			[[[MTLRenderPipelineDescriptor alloc] init] autorelease];
		pipeline.vertexFunction = vertexFunction; pipeline.fragmentFunction = fragmentFunction;
		pipeline.vertexDescriptor = vertices;
		pipeline.colorAttachments[0].pixelFormat = present->drawable.texture.pixelFormat;
		pipeline.depthAttachmentPixelFormat = MTLPixelFormatDepth32Float;
		present->worldPipeline = [device newRenderPipelineStateWithDescriptor:pipeline error:&error];
		if ( present->worldPipeline && !error ) {
			pipeline.colorAttachments[0].blendingEnabled = YES;
			pipeline.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorSourceAlpha;
			pipeline.colorAttachments[0].destinationRGBBlendFactor =
				MTLBlendFactorOneMinusSourceAlpha;
			pipeline.colorAttachments[0].sourceAlphaBlendFactor = MTLBlendFactorOne;
			pipeline.colorAttachments[0].destinationAlphaBlendFactor =
				MTLBlendFactorOneMinusSourceAlpha;
			present->worldBlendPipeline = [device
				newRenderPipelineStateWithDescriptor:pipeline error:&error];
		}
		MTLDepthStencilDescriptor *depth =
			[[[MTLDepthStencilDescriptor alloc] init] autorelease];
		depth.depthCompareFunction = MTLCompareFunctionLess;
		depth.depthWriteEnabled = YES;
		present->worldDepthState = [device newDepthStencilStateWithDescriptor:depth];
		depth.depthWriteEnabled = NO;
		present->worldDepthReadState = [device newDepthStencilStateWithDescriptor:depth];
	}
	[vertexFunction release]; vertexFunction = nil;
	[fragmentFunction release]; fragmentFunction = nil;
	if ( !present->worldPipeline || !present->worldBlendPipeline
			|| !present->worldDepthState || !present->worldDepthReadState
			|| error ) goto fail;
	present->worldPixelFormat = present->drawable.texture.pixelFormat;
	return qtrue;
fail:
	[vertexFunction release]; [fragmentFunction release];
	[present->worldPipeline release]; present->worldPipeline = nil;
	[present->worldBlendPipeline release]; present->worldBlendPipeline = nil;
	[present->worldDepthState release]; present->worldDepthState = nil;
	[present->worldDepthReadState release]; present->worldDepthReadState = nil;
	return qfalse;
}

static qboolean BoolValid( qboolean value ) {
	return value == qfalse || value == qtrue ? qtrue : qfalse;
}

static qboolean SdrFormat( ralFormat_t format ) {
	return format == RAL_FORMAT_B8G8R8A8_UNORM
		|| format == RAL_FORMAT_B8G8R8A8_SRGB ? qtrue : qfalse;
}

static qboolean FormatPairValid( ralFormat_t format, ralColorSpace_t colorSpace ) {
	return ( ( SdrFormat( format )
			&& colorSpace == RAL_COLORSPACE_SRGB_NONLINEAR )
		|| ( format == RAL_FORMAT_R16G16B16A16_SFLOAT
			&& colorSpace == RAL_COLORSPACE_DISPLAY_P3 ) ) ? qtrue : qfalse;
}

ralResult_t Ral_SelectSurfaceFormat( ralBackend_t *backend,
		const ralSurfaceFormatSelectionInfo_t *info,
		ralSurfaceFormatSelection_t *outSelection ) {
	ralSurfaceFormatSelection_t candidate;
	uint32_t i;
	if ( !backend || !info || !outSelection || !info->preferences
			|| info->preferenceCount == 0u || info->backendExtensionChain
			|| ( info->useExtendedQuery != qfalse && info->useExtendedQuery != qtrue ) )
		return ralErrorInvalidArgument;
	memset( &candidate, 0, sizeof( candidate ) );
	for ( i = 0u; i < info->preferenceCount; ++i ) {
		if ( FormatPairValid( info->preferences[i].format,
				info->preferences[i].colorSpace ) ) {
			candidate.selected = info->preferences[i];
			candidate.selectedPreference = i;
			candidate.availableFormatCount = 2u;
			candidate.extendedQuery = info->useExtendedQuery;
			*outSelection = candidate;
			return ralSuccess;
		}
	}
	return ralUnsupported;
}

static qboolean PresentModeValid( ralPresentMode_t mode ) {
	return mode == RAL_PRESENT_FIFO || mode == RAL_PRESENT_IMMEDIATE ? qtrue : qfalse;
}

static qboolean SelectPresentation( const ralSwapchainCreateInfo_t *createInfo,
		uint64_t generation, ralSwapchainInfo_t *outSelected ) {
	ralSwapchainInfo_t selected;
	uint32_t i;
	qboolean foundFormat = qfalse, foundMode = qfalse;
	if ( !createInfo || !outSelected || !createInfo->formatPreferences
			|| createInfo->formatPreferenceCount == 0u
			|| !createInfo->presentPreferences
			|| createInfo->presentPreferenceCount == 0u
			|| createInfo->desiredWidth == 0u || createInfo->desiredHeight == 0u
			|| createInfo->desiredWidth > 16384u || createInfo->desiredHeight > 16384u
			|| createInfo->requiredUsage != RAL_TEXTURE_USAGE_COLOR_ATTACHMENT
			|| createInfo->backendExtensionChain ) return qfalse;
	memset( &selected, 0, sizeof( selected ) );
	for ( i = 0u; i < createInfo->formatPreferenceCount; ++i ) {
		if ( FormatPairValid( createInfo->formatPreferences[i].format,
				createInfo->formatPreferences[i].colorSpace ) ) {
			selected.format = createInfo->formatPreferences[i].format;
			selected.colorSpace = createInfo->formatPreferences[i].colorSpace;
			foundFormat = qtrue; break;
		}
	}
	for ( i = 0u; i < createInfo->presentPreferenceCount; ++i ) {
		const ralPresentPreference_t *preference = &createInfo->presentPreferences[i];
		if ( ( preference->desiredImageCount != 0u
				&& ( preference->desiredImageCount < 2u
					|| preference->desiredImageCount > 3u ) )
			|| ( preference->unboundedImageCount != 0u
				&& ( preference->unboundedImageCount < 2u
					|| preference->unboundedImageCount > 4u ) ) ) return qfalse;
	}
	for ( i = 0u; i < createInfo->presentPreferenceCount; ++i ) {
		const ralPresentPreference_t *preference = &createInfo->presentPreferences[i];
		if ( PresentModeValid( preference->mode ) ) {
			selected.presentMode = preference->mode;
			selected.requestedImageCount = preference->desiredImageCount
				? preference->desiredImageCount : 3u;
			foundMode = qtrue; break;
		}
	}
	if ( !foundFormat || !foundMode ) return qfalse;
	selected.generation = generation;
	selected.width = createInfo->desiredWidth; selected.height = createInfo->desiredHeight;
	selected.imageCount = selected.requestedImageCount;
	selected.usage = createInfo->requiredUsage;
	*outSelected = selected;
	return qtrue;
}

static qboolean LayerReceiptValid( const ralMetalPresentLayerReceipt_t *r ) {
	return ( r && r->schemaVersion == RAL_METAL_PRESENT_SCHEMA_VERSION
		&& r->backendType == RAL_BACKEND_METAL
		&& r->coreGeneration != 0u && r->coreGeneration != UINT64_MAX
		&& r->presentationGeneration != 0u && r->presentationGeneration != UINT64_MAX
		&& r->ownerIdentity != (uintptr_t)0 && r->layerIdentity != (uintptr_t)0
		&& r->ownerIdentity != r->layerIdentity
		&& r->selected.generation == r->presentationGeneration
		&& r->selected.width != 0u && r->selected.height != 0u
		&& FormatPairValid( r->selected.format, r->selected.colorSpace )
		&& PresentModeValid( r->selected.presentMode )
		&& r->selected.requestedImageCount == r->selected.imageCount
		&& ( r->selected.imageCount == 2u || r->selected.imageCount == 3u )
		&& r->selected.usage == RAL_TEXTURE_USAGE_COLOR_ATTACHMENT
		&& BoolValid( r->displaySyncEnabled ) && BoolValid( r->extendedDynamicRange )
		&& r->displaySyncEnabled
			== ( r->selected.presentMode == RAL_PRESENT_FIFO ? qtrue : qfalse )
		&& r->extendedDynamicRange
			== ( r->selected.format == RAL_FORMAT_R16G16B16A16_SFLOAT ? qtrue : qfalse )
		&& r->framebufferOnly == qfalse && BoolValid( r->ownsLayer )
		&& r->ready == qtrue ) ? qtrue : qfalse;
}

qboolean RalMetal_PresentLayerReceiptExact( const ralMetalPresentLayerReceipt_t *a,
		const ralMetalPresentLayerReceipt_t *b ) {
	return ( LayerReceiptValid( a ) && LayerReceiptValid( b )
		&& a->backendType == b->backendType && a->coreGeneration == b->coreGeneration
		&& a->presentationGeneration == b->presentationGeneration
		&& a->ownerIdentity == b->ownerIdentity && a->layerIdentity == b->layerIdentity
		&& !memcmp( &a->selected, &b->selected, sizeof( a->selected ) )
		&& a->displaySyncEnabled == b->displaySyncEnabled
		&& a->extendedDynamicRange == b->extendedDynamicRange
		&& a->framebufferOnly == b->framebufferOnly
		&& a->ownsLayer == b->ownsLayer && a->ready == b->ready )
		? qtrue : qfalse;
}

static qboolean OwnerMatches( const ralMetalPresent_t *present,
		const ralMetalCoreReceipt_t *coreReceipt,
		const ralMetalPresentLayerReceipt_t *layerReceipt ) {
	CGColorSpaceRef nativeColorSpace;
	CFStringRef nativeName, expectedName;
	MTLPixelFormat expectedFormat;
	if ( !present || !present->layer || !layerReceipt ) return qfalse;
	nativeColorSpace = present->layer.colorspace;
	nativeName = nativeColorSpace ? CGColorSpaceGetName( nativeColorSpace ) : NULL;
	expectedName = SdrFormat( layerReceipt->selected.format )
		? kCGColorSpaceSRGB : kCGColorSpaceExtendedLinearDisplayP3;
	expectedFormat = layerReceipt->selected.format == RAL_FORMAT_B8G8R8A8_SRGB
		? MTLPixelFormatBGRA8Unorm_sRGB
		: ( layerReceipt->selected.format == RAL_FORMAT_B8G8R8A8_UNORM
			? MTLPixelFormatBGRA8Unorm : MTLPixelFormatRGBA16Float );
	return ( present && coreReceipt && layerReceipt
		&& present->core && present->layer
		&& RalMetal_CoreMatchesReceipt( present->core, coreReceipt )
		&& layerReceipt->ownerIdentity == (uintptr_t)present
		&& layerReceipt->layerIdentity == (uintptr_t)(void *)present->layer
		&& layerReceipt->ownsLayer == present->ownsLayer
		&& RalMetal_PresentLayerReceiptExact( layerReceipt, &present->receipt )
		&& present->layer.device == RalMetal_CoreNativeDevice( present->core )
		&& present->layer.pixelFormat == expectedFormat
		&& present->layer.drawableSize.width == layerReceipt->selected.width
		&& present->layer.drawableSize.height == layerReceipt->selected.height
		&& present->layer.maximumDrawableCount == layerReceipt->selected.imageCount
		&& ( present->layer.displaySyncEnabled ? qtrue : qfalse )
			== layerReceipt->displaySyncEnabled
		&& ( present->layer.framebufferOnly ? qtrue : qfalse )
			== layerReceipt->framebufferOnly
		&& ( present->layer.wantsExtendedDynamicRangeContent ? qtrue : qfalse )
			== layerReceipt->extendedDynamicRange
		&& nativeName && CFEqual( nativeName, expectedName ) )
		? qtrue : qfalse;
}

static qboolean BorrowedOwnerMatchesResize( const ralMetalPresent_t *present,
		const ralMetalCoreReceipt_t *coreReceipt,
		const ralMetalPresentLayerReceipt_t *currentReceipt,
		const ralSwapchainCreateInfo_t *createInfo, uint64_t generation ) {
	ralSwapchainInfo_t selected;
	CGColorSpaceRef nativeColorSpace;
	CFStringRef nativeName, expectedName;
	MTLPixelFormat expectedFormat;
	if ( !present || present->ownsLayer != qfalse || !present->layer
			|| !coreReceipt || !currentReceipt
			|| !SelectPresentation( createInfo, generation, &selected ) ) return qfalse;
	nativeColorSpace = present->layer.colorspace;
	nativeName = nativeColorSpace ? CGColorSpaceGetName( nativeColorSpace ) : NULL;
	expectedName = SdrFormat( currentReceipt->selected.format )
		? kCGColorSpaceSRGB : kCGColorSpaceExtendedLinearDisplayP3;
	expectedFormat = currentReceipt->selected.format == RAL_FORMAT_B8G8R8A8_SRGB
		? MTLPixelFormatBGRA8Unorm_sRGB
		: ( currentReceipt->selected.format == RAL_FORMAT_B8G8R8A8_UNORM
			? MTLPixelFormatBGRA8Unorm : MTLPixelFormatRGBA16Float );
	return ( RalMetal_CoreMatchesReceipt( present->core, coreReceipt )
		&& currentReceipt->ownerIdentity == (uintptr_t)present
		&& currentReceipt->layerIdentity == (uintptr_t)(void *)present->layer
		&& RalMetal_PresentLayerReceiptExact( currentReceipt, &present->receipt )
		&& currentReceipt->ownsLayer == qfalse
		&& present->layer.device == RalMetal_CoreNativeDevice( present->core )
		&& present->layer.pixelFormat == expectedFormat
		&& present->layer.drawableSize.width == selected.width
		&& present->layer.drawableSize.height == selected.height
		&& present->layer.maximumDrawableCount == currentReceipt->selected.imageCount
		&& ( present->layer.displaySyncEnabled ? qtrue : qfalse )
			== currentReceipt->displaySyncEnabled
		&& ( present->layer.framebufferOnly ? qtrue : qfalse )
			== currentReceipt->framebufferOnly
		&& ( present->layer.wantsExtendedDynamicRangeContent ? qtrue : qfalse )
			== currentReceipt->extendedDynamicRange
		&& nativeName && CFEqual( nativeName, expectedName ) ) ? qtrue : qfalse;
}

static qboolean ConfigureLayer( ralMetalCore_t *core,
		const ralMetalCoreReceipt_t *coreReceipt,
		const ralSwapchainCreateInfo_t *createInfo, uint64_t generation,
		uintptr_t ownerIdentity, CAMetalLayer *layer, qboolean ownsLayer,
		ralMetalPresentLayerReceipt_t *outReceipt ) {
	ralSwapchainInfo_t selected;
	ralMetalPresentLayerReceipt_t receipt;
	CGColorSpaceRef colorSpace;
	if ( !core || !coreReceipt || !layer || !outReceipt || !BoolValid( ownsLayer )
			|| ownerIdentity == (uintptr_t)0 || generation == 0u || generation == UINT64_MAX
			|| !RalMetal_CoreMatchesReceipt( core, coreReceipt )
			|| !SelectPresentation( createInfo, generation, &selected )
			|| ![layer isKindOfClass:[CAMetalLayer class]] ) return qfalse;
	colorSpace = CGColorSpaceCreateWithName( SdrFormat( selected.format )
		? kCGColorSpaceSRGB : kCGColorSpaceExtendedLinearDisplayP3 );
	if ( !colorSpace ) return qfalse;
	memset( &receipt, 0, sizeof( receipt ) );
	receipt.schemaVersion = RAL_METAL_PRESENT_SCHEMA_VERSION;
	receipt.backendType = RAL_BACKEND_METAL; receipt.coreGeneration = coreReceipt->generation;
	receipt.presentationGeneration = generation; receipt.ownerIdentity = ownerIdentity;
	receipt.layerIdentity = (uintptr_t)(void *)layer; receipt.selected = selected;
	receipt.displaySyncEnabled = selected.presentMode == RAL_PRESENT_FIFO ? qtrue : qfalse;
	receipt.extendedDynamicRange = selected.format == RAL_FORMAT_R16G16B16A16_SFLOAT
		? qtrue : qfalse;
	receipt.framebufferOnly = qfalse; receipt.ownsLayer = ownsLayer; receipt.ready = qtrue;
	if ( !LayerReceiptValid( &receipt ) ) { CGColorSpaceRelease( colorSpace ); return qfalse; }
	layer.device = RalMetal_CoreNativeDevice( core );
	layer.pixelFormat = selected.format == RAL_FORMAT_B8G8R8A8_SRGB
		? MTLPixelFormatBGRA8Unorm_sRGB
		: ( selected.format == RAL_FORMAT_B8G8R8A8_UNORM
			? MTLPixelFormatBGRA8Unorm : MTLPixelFormatRGBA16Float );
	layer.drawableSize = CGSizeMake( selected.width, selected.height );
	layer.maximumDrawableCount = selected.imageCount;
	layer.displaySyncEnabled = selected.presentMode == RAL_PRESENT_FIFO ? YES : NO;
	layer.framebufferOnly = NO;
	layer.presentsWithTransaction = NO;
	layer.allowsNextDrawableTimeout = YES;
	layer.wantsExtendedDynamicRangeContent = selected.format
		== RAL_FORMAT_R16G16B16A16_SFLOAT ? YES : NO;
	layer.colorspace = colorSpace;
	CGColorSpaceRelease( colorSpace );
	*outReceipt = receipt;
	return qtrue;
}

static qboolean BuildOwnedLayer( ralMetalCore_t *core,
		const ralMetalCoreReceipt_t *coreReceipt,
		const ralSwapchainCreateInfo_t *createInfo, uint64_t generation,
		uintptr_t ownerIdentity, CAMetalLayer **outLayer,
		ralMetalPresentLayerReceipt_t *outReceipt ) {
	CAMetalLayer *layer;
	if ( !outLayer || !outReceipt ) return qfalse;
	layer = [[CAMetalLayer alloc] init];
	if ( !layer ) return qfalse;
	if ( !ConfigureLayer( core, coreReceipt, createInfo, generation, ownerIdentity,
			layer, qtrue, outReceipt ) ) { [layer release]; return qfalse; }
	*outLayer = layer;
	return qtrue;
}

qboolean RalMetal_PresentCreate( ralMetalCore_t *core,
		const ralMetalCoreReceipt_t *coreReceipt,
		const ralSwapchainCreateInfo_t *createInfo, uint64_t generation,
		ralMetalPresent_t **outPresent, ralMetalPresentLayerReceipt_t *outReceipt ) {
	ralMetalPresent_t *candidate;
	ralMetalPresentLayerReceipt_t receipt;
	CAMetalLayer *layer = nil;
	if ( !outPresent || !outReceipt ) return qfalse;
	candidate = (ralMetalPresent_t *)calloc( 1u, sizeof( *candidate ) );
	if ( !candidate ) return qfalse;
	if ( !BuildOwnedLayer( core, coreReceipt, createInfo, generation, (uintptr_t)candidate,
			&layer, &receipt ) ) { free( candidate ); return qfalse; }
	candidate->core = core; candidate->layer = layer; candidate->receipt = receipt;
	candidate->ownsLayer = qtrue;
	*outPresent = candidate; *outReceipt = receipt;
	return qtrue;
}

qboolean RalMetal_PresentAdoptBorrowedLayer( ralMetalCore_t *core,
		const ralMetalCoreReceipt_t *coreReceipt,
		const ralSwapchainCreateInfo_t *createInfo, uint64_t generation,
		void *borrowedLayer, ralMetalPresent_t **outPresent,
		ralMetalPresentLayerReceipt_t *outReceipt ) {
	ralMetalPresent_t *candidate;
	ralMetalPresentLayerReceipt_t receipt;
	CAMetalLayer *layer = (CAMetalLayer *)borrowedLayer;
	if ( !outPresent || !outReceipt || !layer ) return qfalse;
	candidate = (ralMetalPresent_t *)calloc( 1u, sizeof( *candidate ) );
	if ( !candidate ) return qfalse;
	if ( !ConfigureLayer( core, coreReceipt, createInfo, generation,
			(uintptr_t)candidate, layer, qfalse, &receipt ) ) {
		free( candidate ); return qfalse;
	}
	candidate->core = core; candidate->layer = layer; candidate->receipt = receipt;
	candidate->ownsLayer = qfalse;
	*outPresent = candidate; *outReceipt = receipt;
	return qtrue;
}

qboolean RalMetal_PresentReconfigure( ralMetalPresent_t *present,
		const ralMetalCoreReceipt_t *coreReceipt,
		const ralMetalPresentLayerReceipt_t *currentReceipt,
		const ralSwapchainCreateInfo_t *createInfo, uint64_t generation,
		ralMetalPresentLayerReceipt_t *outReceipt ) {
	ralMetalPresentLayerReceipt_t receipt;
	CAMetalLayer *candidate = nil, *old;
	if ( !outReceipt || ( !OwnerMatches( present, coreReceipt, currentReceipt )
			&& !BorrowedOwnerMatchesResize( present, coreReceipt, currentReceipt,
				createInfo, generation ) )
			|| present->drawable || generation <= currentReceipt->presentationGeneration
			|| generation == UINT64_MAX ) return qfalse;
	if ( !ReapReadbackRing( present, qtrue ) ) return qfalse;
	if ( present->ownsLayer ) {
		if ( !BuildOwnedLayer( present->core, coreReceipt, createInfo, generation,
				(uintptr_t)present, &candidate, &receipt ) ) return qfalse;
		old = present->layer; present->layer = candidate; present->receipt = receipt;
		[old release];
	} else {
		if ( !ConfigureLayer( present->core, coreReceipt, createInfo, generation,
				(uintptr_t)present, present->layer, qfalse, &receipt ) ) return qfalse;
		present->receipt = receipt;
	}
	present->lastReadbackGeneration = 0u;
	present->lastReadbackByteCount = 0u;
	present->lastReadbackReady = qfalse;
	*outReceipt = receipt;
	return qtrue;
}

void RalMetal_PresentDestroy( ralMetalPresent_t *present ) {
	uint32_t i;
	if ( !present ) return;
	(void)ReapReadbackRing( present, qtrue );
	for ( i = 0u; i < RAL_METAL_READBACK_RING_SIZE; ++i ) {
		[present->readbackCommands[i] release];
		[present->readbackBuffers[i] release];
	}
	for ( i = 0u; i < RENDER_SUBMISSION_MAX_MATERIALS; ++i )
		[present->uiMaterials[i].texture release];
	[present->uiFallbackTexture release];
	[present->uiClampSampler release];
	[present->uiRepeatSampler release];
	[present->uiPipeline release];
	[present->uiMsdfPipeline release];
	[present->worldPipeline release];
	[present->worldBlendPipeline release];
	[present->worldDepthState release];
	[present->worldDepthReadState release];
	[present->worldVertexCache release];
	[present->worldIndexCache release];
	[present->worldDepthCache release];
	free( present->worldBatchBounds );
	[present->uiLibrary release];
	[present->drawable release];
	free( present->captureRgb );
	if ( present->ownsLayer ) [present->layer release];
	memset( present, 0, sizeof( *present ) );
	free( present );
}

qboolean RalMetal_PresentRequestCapture( ralMetalPresent_t *present ) {
	if ( !present || present->capturePending
			|| !SdrFormat( present->receipt.selected.format ) )
		return qfalse;
	free( present->captureRgb ); present->captureRgb = NULL;
	present->captureWidth = present->captureHeight = 0u;
	present->captureReady = qfalse; present->capturePending = qtrue;
	return qtrue;
}

const byte *RalMetal_PresentCaptureRgb( const ralMetalPresent_t *present,
		uint32_t *outWidth, uint32_t *outHeight ) {
	if ( outWidth ) *outWidth = 0u;
	if ( outHeight ) *outHeight = 0u;
	if ( !present || !outWidth || !outHeight || !present->captureReady
			|| !present->captureRgb || !present->captureWidth
			|| !present->captureHeight ) return NULL;
	*outWidth = present->captureWidth; *outHeight = present->captureHeight;
	return present->captureRgb;
}

static qboolean DrawableReceiptValid( const ralMetalDrawableReceipt_t *r ) {
	return ( r && r->schemaVersion == RAL_METAL_PRESENT_SCHEMA_VERSION
		&& r->backendType == RAL_BACKEND_METAL
		&& r->coreGeneration != 0u && r->coreGeneration != UINT64_MAX
		&& r->presentationGeneration != 0u && r->presentationGeneration != UINT64_MAX
		&& r->acquireGeneration != 0u && r->acquireGeneration != UINT64_MAX
		&& r->ownerIdentity != (uintptr_t)0 && r->layerIdentity != (uintptr_t)0
		&& r->drawableIdentity != (uintptr_t)0 && r->textureIdentity != (uintptr_t)0
		&& r->ownerIdentity != r->layerIdentity && r->drawableIdentity != r->textureIdentity
		&& r->width != 0u && r->height != 0u
		&& ( SdrFormat( r->format )
			|| r->format == RAL_FORMAT_R16G16B16A16_SFLOAT )
		&& r->acquired == qtrue ) ? qtrue : qfalse;
}

qboolean RalMetal_DrawableReceiptExact( const ralMetalDrawableReceipt_t *a,
		const ralMetalDrawableReceipt_t *b ) {
	return ( DrawableReceiptValid( a ) && DrawableReceiptValid( b )
		&& a->backendType == b->backendType && a->coreGeneration == b->coreGeneration
		&& a->presentationGeneration == b->presentationGeneration
		&& a->acquireGeneration == b->acquireGeneration
		&& a->ownerIdentity == b->ownerIdentity && a->layerIdentity == b->layerIdentity
		&& a->drawableIdentity == b->drawableIdentity && a->textureIdentity == b->textureIdentity
		&& a->width == b->width && a->height == b->height && a->format == b->format
		&& a->acquired == b->acquired ) ? qtrue : qfalse;
}

qboolean RalMetal_PresentAcquire( ralMetalPresent_t *present,
		const ralMetalCoreReceipt_t *coreReceipt,
		const ralMetalPresentLayerReceipt_t *layerReceipt, uint64_t acquireGeneration,
		ralMetalDrawableReceipt_t *outReceipt ) {
	ralMetalDrawableReceipt_t receipt;
	id<CAMetalDrawable> drawable;
	id<MTLTexture> texture;
	if ( !outReceipt || !OwnerMatches( present, coreReceipt, layerReceipt )
			|| present->drawable || acquireGeneration == 0u
			|| acquireGeneration == UINT64_MAX
			|| acquireGeneration <= present->lastAcquireGeneration ) return qfalse;
	drawable = [[present->layer nextDrawable] retain];
	if ( !drawable ) return qfalse;
	texture = drawable.texture;
	memset( &receipt, 0, sizeof( receipt ) );
	receipt.schemaVersion = RAL_METAL_PRESENT_SCHEMA_VERSION;
	receipt.backendType = RAL_BACKEND_METAL; receipt.coreGeneration = coreReceipt->generation;
	receipt.presentationGeneration = layerReceipt->presentationGeneration;
	receipt.acquireGeneration = acquireGeneration; receipt.ownerIdentity = (uintptr_t)present;
	receipt.layerIdentity = (uintptr_t)(void *)present->layer;
	receipt.drawableIdentity = (uintptr_t)(void *)drawable;
	receipt.textureIdentity = (uintptr_t)(void *)texture;
	receipt.width = (uint32_t)texture.width; receipt.height = (uint32_t)texture.height;
	receipt.format = layerReceipt->selected.format; receipt.acquired = qtrue;
	if ( !texture || texture.width != layerReceipt->selected.width
			|| texture.height != layerReceipt->selected.height
			|| texture.pixelFormat != present->layer.pixelFormat
			|| !DrawableReceiptValid( &receipt ) ) { [drawable release]; return qfalse; }
	present->drawable = drawable; present->drawableReceipt = receipt;
	present->lastAcquireGeneration = acquireGeneration;
	*outReceipt = receipt;
	return qtrue;
}

static qboolean PresentReceiptValid( const ralMetalPresentReceipt_t *r ) {
	return ( r && r->schemaVersion == RAL_METAL_PRESENT_SCHEMA_VERSION
		&& r->backendType == RAL_BACKEND_METAL
		&& r->coreGeneration != 0u && r->coreGeneration != UINT64_MAX
		&& r->presentationGeneration != 0u && r->presentationGeneration != UINT64_MAX
		&& r->acquireGeneration != 0u && r->acquireGeneration != UINT64_MAX
		&& r->presentGeneration != 0u && r->presentGeneration != UINT64_MAX
		&& r->ownerIdentity != (uintptr_t)0 && r->layerIdentity != (uintptr_t)0
		&& r->drawableIdentity != (uintptr_t)0 && r->textureIdentity != (uintptr_t)0
		&& FormatPairValid( r->format, r->colorSpace ) && PresentModeValid( r->presentMode )
		&& r->width != 0u && r->height != 0u
		&& isfinite( r->clearColor[0] ) && isfinite( r->clearColor[1] )
		&& isfinite( r->clearColor[2] ) && isfinite( r->clearColor[3] )
		&& Ral_CommandReceiptValid( &r->recording )
		&& Ral_CommandReceiptValid( &r->executable )
		&& Ral_SubmissionReceiptValid( &r->submission )
		&& r->recording.state == RAL_COMMAND_RECORDING
		&& r->executable.state == RAL_COMMAND_EXECUTABLE
		&& r->recording.generation == r->presentGeneration
		&& r->executable.generation == r->presentGeneration
		&& r->recording.backendIdentity == r->executable.backendIdentity
		&& r->recording.commandIdentity == r->executable.commandIdentity
		&& r->submission.backendIdentity == r->executable.backendIdentity
		&& r->submission.queue == r->executable.queue && r->submission.commandCount == 1u
		&& r->submission.commands[0].commandIdentity == r->executable.commandIdentity
		&& r->submission.commands[0].generation == r->presentGeneration
		&& r->submission.commands[0].state == RAL_COMMAND_SUBMITTED
		&& r->completionGeneration == r->submission.generation
		&& r->clearDigest != 0u && r->readbackDigest != 0u
		&& ( r->loweredWorldIndexCount % 3u ) == 0u
		&& r->loweredWorldBatchCount <= RENDER_SUBMISSION_MAX_WORLD_BATCHES
		&& r->texturedWorldBatchCount <= r->loweredWorldBatchCount
		&& r->lightmappedWorldBatchCount <= r->loweredWorldBatchCount
		&& r->patchWorldBatchCount <= r->loweredWorldBatchCount
		&& r->maskedWorldBatchCount <= r->loweredWorldBatchCount
		&& r->blendedWorldBatchCount <= r->loweredWorldBatchCount
		&& r->depthWriteWorldBatchCount <= r->loweredWorldBatchCount
		&& ( r->loweredEntityIndexCount % 3u ) == 0u
		&& r->loweredEntityBatchCount <= RENDER_SUBMISSION_MAX_WORLD_BATCHES
		&& r->modelEntityCount <= RENDER_SUBMISSION_MAX_ENTITIES
		&& r->primitiveEntityCount <= RENDER_SUBMISSION_MAX_ENTITIES
		&& r->temporalEntityCount <= RENDER_SUBMISSION_MAX_ENTITIES
		&& r->unresolvedEntityCount <= RENDER_SUBMISSION_MAX_ENTITIES
		&& r->loweredUiPrimitiveCount <= RENDER_SUBMISSION_MAX_UI_PRIMITIVES
		&& r->texturedUiPrimitiveCount <= r->loweredUiPrimitiveCount
		&& r->msdfUiPrimitiveCount <= r->texturedUiPrimitiveCount
		&& r->readbackX < r->width && r->readbackY < r->height
		&& ( r->readbackByteCount == 4u || r->readbackByteCount == 8u )
		&& r->presented == qtrue ) ? qtrue : qfalse;
}

qboolean RalMetal_PresentReceiptExact( const ralMetalPresentReceipt_t *a,
		const ralMetalPresentReceipt_t *b ) {
	return ( PresentReceiptValid( a ) && PresentReceiptValid( b )
		&& a->backendType == b->backendType && a->coreGeneration == b->coreGeneration
		&& a->presentationGeneration == b->presentationGeneration
		&& a->acquireGeneration == b->acquireGeneration
		&& a->presentGeneration == b->presentGeneration
		&& a->ownerIdentity == b->ownerIdentity && a->layerIdentity == b->layerIdentity
		&& a->drawableIdentity == b->drawableIdentity && a->textureIdentity == b->textureIdentity
		&& a->format == b->format && a->colorSpace == b->colorSpace
		&& a->presentMode == b->presentMode && a->width == b->width && a->height == b->height
		&& !memcmp( a->clearColor, b->clearColor, sizeof( a->clearColor ) )
		&& Ral_CommandReceiptExact( &a->recording, &b->recording )
		&& Ral_CommandReceiptExact( &a->executable, &b->executable )
		&& Ral_SubmissionReceiptExact( &a->submission, &b->submission )
		&& a->completionGeneration == b->completionGeneration
		&& a->clearDigest == b->clearDigest
		&& a->readbackDigest == b->readbackDigest
		&& a->loweredWorldIndexCount == b->loweredWorldIndexCount
		&& a->loweredWorldBatchCount == b->loweredWorldBatchCount
		&& a->texturedWorldBatchCount == b->texturedWorldBatchCount
		&& a->lightmappedWorldBatchCount == b->lightmappedWorldBatchCount
		&& a->patchWorldBatchCount == b->patchWorldBatchCount
		&& a->maskedWorldBatchCount == b->maskedWorldBatchCount
		&& a->blendedWorldBatchCount == b->blendedWorldBatchCount
		&& a->depthWriteWorldBatchCount == b->depthWriteWorldBatchCount
		&& a->loweredEntityIndexCount == b->loweredEntityIndexCount
		&& a->loweredEntityBatchCount == b->loweredEntityBatchCount
		&& a->modelEntityCount == b->modelEntityCount
		&& a->primitiveEntityCount == b->primitiveEntityCount
		&& a->temporalEntityCount == b->temporalEntityCount
		&& a->unresolvedEntityCount == b->unresolvedEntityCount
		&& a->loweredUiPrimitiveCount == b->loweredUiPrimitiveCount
		&& a->texturedUiPrimitiveCount == b->texturedUiPrimitiveCount
		&& a->msdfUiPrimitiveCount == b->msdfUiPrimitiveCount
		&& a->readbackX == b->readbackX && a->readbackY == b->readbackY
		&& a->readbackByteCount == b->readbackByteCount
		&& !memcmp( a->readbackBytes, b->readbackBytes,
			sizeof( a->readbackBytes ) )
		&& a->presented == b->presented ) ? qtrue : qfalse;
}

static uint64_t DigestClear( const float clearColor[4] ) {
	const uint8_t *bytes = (const uint8_t *)clearColor;
	uint64_t digest = UINT64_C(1469598103934665603);
	for ( uint32_t i = 0u; i < sizeof( float ) * 4u; ++i ) {
		digest ^= bytes[i]; digest *= UINT64_C(1099511628211);
	}
	return digest;
}

static uint64_t DigestBytes( const uint8_t *bytes, uint32_t count ) {
	uint64_t digest = UINT64_C(1469598103934665603);
	for ( uint32_t i = 0u; i < count; ++i ) {
		digest ^= bytes[i]; digest *= UINT64_C(1099511628211);
	}
	return digest;
}

qboolean RalMetal_PresentClearAndSubmit( ralMetalPresent_t *present,
		const ralMetalCoreReceipt_t *coreReceipt,
		const ralMetalPresentLayerReceipt_t *layerReceipt,
		const ralMetalDrawableReceipt_t *drawableReceipt,
		const renderSubmissionState_t *frontend,
		const float clearColor[4], uint64_t presentGeneration,
		ralMetalPresentReceipt_t *outReceipt ) {
	ralMetalPresentReceipt_t receipt;
	ralCommandLifecycle_t lifecycle;
	ralCommandReceipt_t recording, executable;
	id<MTLCommandQueue> queue;
	id<MTLBuffer> readback = nil;
	id<MTLBuffer> captureReadback = nil;
	id<MTLBuffer> uiVertices = nil;
	id<MTLBuffer> worldVertices = nil;
	id<MTLBuffer> worldIndices = nil;
	id<MTLBuffer> entityVertices = nil;
	id<MTLBuffer> entityIndices = nil;
	id<MTLTexture> worldDepth = nil;
	ralMetalUiVertex_t *uiVertexBytes = NULL;
	const renderUiPrimitive_t *uiPrimitives = NULL;
	id<MTLTexture> uiTextures[RENDER_SUBMISSION_MAX_UI_PRIMITIVES];
	id<MTLSamplerState> uiSamplers[RENDER_SUBMISSION_MAX_UI_PRIMITIVES];
	qboolean uiMsdf[RENDER_SUBMISSION_MAX_UI_PRIMITIVES];
	ralMetalWorldViewParams_t worldViewParams;
	ralMetalWorldVertex_t *worldVertexBytes = NULL;
	ralMetalWorldVertex_t *entityVertexBytes = NULL;
	uint32_t *entityIndexBytes = NULL;
	ralMetalEntityBatch_t *entityBatches = NULL;
	const renderWorldBatch_t *worldBatches = NULL;
	uint32_t uiPrimitiveCount = 0u;
	uint32_t worldVertexCount = 0u, worldIndexCount = 0u, worldBatchCount = 0u;
	uint32_t entityVertexCount = 0u, entityIndexCount = 0u, entityBatchCount = 0u;
	uint32_t readbackBytes;
	uint32_t readbackSlot = 0u;
	NSUInteger captureBytesPerRow = 0u;
	uint32_t i;
	float maxRgb;
	qboolean sampleReadback;
	if ( !outReceipt || !clearColor || !OwnerMatches( present, coreReceipt, layerReceipt )
			|| !present->drawable || !drawableReceipt
			|| !RalMetal_DrawableReceiptExact( drawableReceipt, &present->drawableReceipt )
			|| presentGeneration == 0u || presentGeneration == UINT64_MAX
			|| presentGeneration <= drawableReceipt->acquireGeneration
			|| presentGeneration <= present->lastPresentGeneration ) return qfalse;
	maxRgb = layerReceipt->extendedDynamicRange == qtrue ? 16.0f : 1.0f;
	for ( i = 0u; i < 4u; ++i ) if ( !isfinite( clearColor[i] ) ) return qfalse;
	if ( clearColor[0] < 0.0f || clearColor[0] > maxRgb
			|| clearColor[1] < 0.0f || clearColor[1] > maxRgb
			|| clearColor[2] < 0.0f || clearColor[2] > maxRgb
			|| clearColor[3] < 0.0f || clearColor[3] > 1.0f ) return qfalse;
	queue = RalMetal_CoreNativeQueue( present->core );
	memset( &receipt, 0, sizeof( receipt ) );
	receipt.schemaVersion = RAL_METAL_PRESENT_SCHEMA_VERSION;
	receipt.backendType = RAL_BACKEND_METAL; receipt.coreGeneration = coreReceipt->generation;
	receipt.presentationGeneration = layerReceipt->presentationGeneration;
	receipt.acquireGeneration = drawableReceipt->acquireGeneration;
	receipt.presentGeneration = presentGeneration; receipt.ownerIdentity = (uintptr_t)present;
	receipt.layerIdentity = layerReceipt->layerIdentity;
	receipt.drawableIdentity = drawableReceipt->drawableIdentity;
	receipt.textureIdentity = drawableReceipt->textureIdentity;
	receipt.format = layerReceipt->selected.format; receipt.colorSpace = layerReceipt->selected.colorSpace;
	receipt.presentMode = layerReceipt->selected.presentMode;
	receipt.width = drawableReceipt->width; receipt.height = drawableReceipt->height;
	memcpy( receipt.clearColor, clearColor, sizeof( receipt.clearColor ) );
	readbackBytes = receipt.format == RAL_FORMAT_R16G16B16A16_SFLOAT ? 8u : 4u;
	if ( !ReapReadbackRing( present, qfalse ) ) return qfalse;
	sampleReadback = ( present->capturePending || !present->lastReadbackReady
		|| ( !ReadbackRingPending( present )
			&& presentGeneration - present->lastReadbackGeneration >= 60u ) )
		? qtrue : qfalse;
	if ( !BuildUiVertices( frontend, receipt.width, receipt.height, &uiVertexBytes,
			&uiPrimitiveCount, &receipt.readbackX, &receipt.readbackY ) ) return qfalse;
	if ( uiPrimitiveCount ) {
		uint32_t publishedCount = 0u;
		uiPrimitives = RenderSubmission_UiPrimitives( frontend, &publishedCount );
		if ( !uiPrimitives || publishedCount != uiPrimitiveCount ) {
			free( uiVertexBytes ); return qfalse;
		}
		for ( uint32_t primitive = 0u; primitive < uiPrimitiveCount; ++primitive ) {
			qboolean textured;
			if ( !UiMaterial( present, frontend, uiPrimitives[primitive].material,
					&uiTextures[primitive], &uiSamplers[primitive], &textured,
					&uiMsdf[primitive] ) ) {
				free( uiVertexBytes ); return qfalse;
			}
			if ( textured ) receipt.texturedUiPrimitiveCount++;
			if ( uiMsdf[primitive] ) receipt.msdfUiPrimitiveCount++;
		}
	}
	if ( !PrepareWorldBuffers( present, frontend, &worldVertices, &worldIndices,
			&worldBatches, &worldVertexCount, &worldIndexCount,
			&worldBatchCount ) ) { free( uiVertexBytes ); return qfalse; }
	if ( !BuildEntityVertices( frontend, &entityVertexBytes, &entityIndexBytes,
			&entityBatches, &entityVertexCount, &entityIndexCount,
			&entityBatchCount, &receipt.modelEntityCount,
			&receipt.primitiveEntityCount, &receipt.temporalEntityCount,
			&receipt.unresolvedEntityCount ) ) {
		free( worldVertexBytes ); free( uiVertexBytes ); return qfalse;
	}
	receipt.loweredUiPrimitiveCount = uiPrimitiveCount;
	receipt.loweredWorldIndexCount = worldIndexCount;
	receipt.loweredWorldBatchCount = worldBatchCount;
	receipt.loweredEntityIndexCount = entityIndexCount;
	receipt.loweredEntityBatchCount = entityBatchCount;
	if ( ( worldIndexCount || entityIndexCount )
			&& !BuildWorldViewParams( frontend, &worldViewParams ) ) {
		free( entityBatches ); free( entityIndexBytes ); free( entityVertexBytes );
		free( worldVertexBytes ); free( uiVertexBytes ); return qfalse;
	}
	@autoreleasepool {
		if ( worldIndexCount || entityIndexCount ) {
			if ( !EnsureWorldPipeline( present ) ) {
				free( entityBatches ); free( entityIndexBytes ); free( entityVertexBytes );
				[worldVertices release]; [worldIndices release];
				free( worldVertexBytes ); free( uiVertexBytes ); return qfalse;
			}
			if ( entityIndexCount ) {
				entityVertices = [RalMetal_CoreNativeDevice( present->core )
					newBufferWithBytes:entityVertexBytes length:(NSUInteger)entityVertexCount
						* sizeof( *entityVertexBytes ) options:MTLResourceStorageModeShared];
				entityIndices = [RalMetal_CoreNativeDevice( present->core )
					newBufferWithBytes:entityIndexBytes length:(NSUInteger)entityIndexCount
						* sizeof( *entityIndexBytes ) options:MTLResourceStorageModeShared];
			}
			if ( !present->worldDepthCache || present->worldDepthWidth != receipt.width
					|| present->worldDepthHeight != receipt.height ) {
				MTLTextureDescriptor *depth = [MTLTextureDescriptor
					texture2DDescriptorWithPixelFormat:MTLPixelFormatDepth32Float
					width:receipt.width height:receipt.height mipmapped:NO];
				depth.storageMode = MTLStorageModePrivate;
				depth.usage = MTLTextureUsageRenderTarget;
				id<MTLTexture> replacement = [RalMetal_CoreNativeDevice( present->core )
					newTextureWithDescriptor:depth];
				if ( !replacement ) {
					[worldVertices release]; [worldIndices release];
					[entityVertices release]; [entityIndices release]; free( entityBatches );
					free( uiVertexBytes ); return qfalse;
				}
				[present->worldDepthCache release];
				present->worldDepthCache = replacement;
				present->worldDepthWidth = receipt.width;
				present->worldDepthHeight = receipt.height;
			}
			worldDepth = [present->worldDepthCache retain];
			free( worldVertexBytes ); worldVertexBytes = NULL;
			free( entityVertexBytes ); entityVertexBytes = NULL;
			free( entityIndexBytes ); entityIndexBytes = NULL;
			if ( ( worldIndexCount && ( !worldVertices || !worldIndices ) )
					|| ( entityIndexCount && ( !entityVertices || !entityIndices ) )
					|| !worldDepth ) {
				[worldVertices release]; [worldIndices release]; [worldDepth release];
				[entityVertices release]; [entityIndices release]; free( entityBatches );
				free( uiVertexBytes ); return qfalse;
			}
		}
		if ( uiPrimitiveCount ) {
			if ( !EnsureUiPipeline( present ) ) {
				[worldVertices release]; [worldIndices release]; [worldDepth release];
				[entityVertices release]; [entityIndices release]; free( entityBatches );
				free( uiVertexBytes ); return qfalse;
			}
			uiVertices = [RalMetal_CoreNativeDevice( present->core )
				newBufferWithBytes:uiVertexBytes
				length:(NSUInteger)uiPrimitiveCount * 6u * sizeof( *uiVertexBytes )
				options:MTLResourceStorageModeShared];
			free( uiVertexBytes ); uiVertexBytes = NULL;
			if ( !uiVertices ) {
				[worldVertices release]; [worldIndices release]; [worldDepth release];
				[entityVertices release]; [entityIndices release]; free( entityBatches );
				return qfalse;
			}
		}
		if ( sampleReadback
				&& !ReserveReadbackSlot( present, &readbackSlot, &readback ) ) {
			[worldVertices release]; [worldIndices release]; [worldDepth release];
			[entityVertices release]; [entityIndices release]; free( entityBatches );
			[uiVertices release]; return qfalse;
		}
		id<MTLCommandBuffer> command = [queue commandBuffer];
		if ( !command || !RalMetal_CoreBeginCommand( present->core, presentGeneration,
				&lifecycle, &recording ) ) {
			[worldVertices release]; [worldIndices release]; [worldDepth release];
			[entityVertices release]; [entityIndices release]; free( entityBatches );
			[uiVertices release]; [readback release]; return qfalse;
		}
		MTLRenderPassDescriptor *pass = [MTLRenderPassDescriptor renderPassDescriptor];
		pass.colorAttachments[0].texture = present->drawable.texture;
		pass.colorAttachments[0].loadAction = MTLLoadActionClear;
		pass.colorAttachments[0].storeAction = MTLStoreActionStore;
		pass.colorAttachments[0].clearColor = MTLClearColorMake( clearColor[0], clearColor[1],
			clearColor[2], clearColor[3] );
		if ( worldIndexCount || entityIndexCount ) {
			pass.depthAttachment.texture = worldDepth;
			pass.depthAttachment.loadAction = MTLLoadActionClear;
			pass.depthAttachment.storeAction = MTLStoreActionDontCare;
			pass.depthAttachment.clearDepth = 1.0;
		}
		id<MTLRenderCommandEncoder> encoder = [command renderCommandEncoderWithDescriptor:pass];
		if ( !encoder ) {
			[worldVertices release]; [worldIndices release]; [worldDepth release];
			[entityVertices release]; [entityIndices release]; free( entityBatches );
			[uiVertices release]; [readback release]; return qfalse;
		}
		if ( worldIndexCount || entityIndexCount )
			[encoder setVertexBytes:&worldViewParams length:sizeof( worldViewParams )
				atIndex:1u];
		if ( entityIndexCount ) {
			[encoder setVertexBuffer:entityVertices offset:0u atIndex:0u];
			for ( uint32_t batchIndex = 0u; batchIndex < entityBatchCount; ++batchIndex ) {
				const ralMetalEntityBatch_t *batch = &entityBatches[batchIndex];
				id<MTLTexture> texture; id<MTLSamplerState> sampler;
				qboolean textured, msdf;
				ralMetalWorldMaterialParams_t params = { 0u,
					(uint32_t)batch->alphaMode, batch->alphaCutoff };
				if ( !UiMaterial( present, frontend, batch->material, &texture,
						&sampler, &textured, &msdf ) || msdf ) {
					[encoder endEncoding];
					[worldVertices release]; [worldIndices release]; [worldDepth release];
					[entityVertices release]; [entityIndices release]; free( entityBatches );
					[uiVertices release]; [readback release]; return qfalse;
				}
				[encoder setRenderPipelineState:batch->alphaMode == RENDER_ALPHA_BLEND
					? present->worldBlendPipeline : present->worldPipeline];
				[encoder setDepthStencilState:batch->depthWrite
					? present->worldDepthState : present->worldDepthReadState];
				[encoder setFragmentTexture:texture atIndex:0u];
				[encoder setFragmentTexture:present->uiFallbackTexture atIndex:1u];
				[encoder setFragmentSamplerState:sampler atIndex:0u];
				[encoder setFragmentSamplerState:present->uiClampSampler atIndex:1u];
				[encoder setFragmentBytes:&params length:sizeof( params ) atIndex:0u];
				[encoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle
					indexCount:batch->indexCount indexType:MTLIndexTypeUInt32
					indexBuffer:entityIndices
					indexBufferOffset:(NSUInteger)batch->firstIndex * sizeof( uint32_t )];
			}
		}
		if ( worldIndexCount ) {
			[encoder setVertexBuffer:worldVertices offset:0u atIndex:0u];
			for ( uint32_t batchIndex = 0u; batchIndex < worldBatchCount; ++batchIndex ) {
				const renderWorldBatch_t *batch = &worldBatches[batchIndex];
				if ( !WorldBatchVisible( &worldViewParams,
						present->worldBatchBounds + (size_t)batchIndex * 6u ) ) continue;
				id<MTLTexture> baseTexture, lightmapTexture;
				id<MTLSamplerState> baseSampler, lightmapSampler;
				qboolean baseTextured, baseMsdf, lightmapped, lightmapMsdf;
				ralMetalWorldMaterialParams_t params;
				if ( !UiMaterial( present, frontend, batch->baseMaterial,
						&baseTexture, &baseSampler, &baseTextured, &baseMsdf )
						|| !UiMaterial( present, frontend, batch->lightmapMaterial,
							&lightmapTexture, &lightmapSampler, &lightmapped,
							&lightmapMsdf ) || baseMsdf || lightmapMsdf ) {
					[encoder endEncoding];
					[worldVertices release]; [worldIndices release]; [worldDepth release];
					[uiVertices release]; [readback release]; return qfalse;
				}
				if ( baseTextured ) receipt.texturedWorldBatchCount++;
				if ( lightmapped ) receipt.lightmappedWorldBatchCount++;
				if ( batch->surfaceType == RENDER_WORLD_SURFACE_PATCH )
					receipt.patchWorldBatchCount++;
				if ( batch->alphaMode == RENDER_ALPHA_MASK )
					receipt.maskedWorldBatchCount++;
				if ( batch->alphaMode == RENDER_ALPHA_BLEND )
					receipt.blendedWorldBatchCount++;
				if ( batch->depthWrite ) receipt.depthWriteWorldBatchCount++;
				params.hasLightmap = lightmapped ? 1u : 0u;
				params.alphaMode = (uint32_t)batch->alphaMode;
				params.alphaCutoff = batch->alphaCutoff;
				[encoder setRenderPipelineState:batch->alphaMode == RENDER_ALPHA_BLEND
					? present->worldBlendPipeline : present->worldPipeline];
				[encoder setDepthStencilState:batch->depthWrite
					? present->worldDepthState : present->worldDepthReadState];
				[encoder setFragmentTexture:baseTexture atIndex:0u];
				[encoder setFragmentTexture:lightmapTexture atIndex:1u];
				[encoder setFragmentSamplerState:baseSampler atIndex:0u];
				[encoder setFragmentSamplerState:lightmapSampler atIndex:1u];
				[encoder setFragmentBytes:&params length:sizeof( params ) atIndex:0u];
				[encoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle
					indexCount:batch->indexCount indexType:MTLIndexTypeUInt32
					indexBuffer:worldIndices
					indexBufferOffset:(NSUInteger)batch->firstIndex * sizeof( uint32_t )];
			}
		}
		if ( uiPrimitiveCount ) {
			[encoder setVertexBuffer:uiVertices offset:0u atIndex:0u];
			for ( uint32_t primitive = 0u; primitive < uiPrimitiveCount; ++primitive ) {
				[encoder setRenderPipelineState:uiMsdf[primitive]
					? present->uiMsdfPipeline : present->uiPipeline];
				[encoder setFragmentTexture:uiTextures[primitive] atIndex:0u];
				[encoder setFragmentSamplerState:uiSamplers[primitive] atIndex:0u];
				[encoder drawPrimitives:MTLPrimitiveTypeTriangle
					vertexStart:(NSUInteger)primitive * 6u vertexCount:6u];
			}
		}
		[encoder endEncoding];
		if ( present->capturePending ) {
			captureBytesPerRow = ( (NSUInteger)receipt.width * 4u + 255u ) & ~255u;
			if ( !captureBytesPerRow || receipt.height > SIZE_MAX / captureBytesPerRow ) {
				[worldVertices release]; [worldIndices release]; [worldDepth release];
				[entityVertices release]; [entityIndices release]; free( entityBatches );
				[uiVertices release]; [readback release]; return qfalse;
			}
			captureReadback = [RalMetal_CoreNativeDevice( present->core )
				newBufferWithLength:captureBytesPerRow * receipt.height
				options:MTLResourceStorageModeShared];
			if ( !captureReadback ) {
				[worldVertices release]; [worldIndices release]; [worldDepth release];
				[entityVertices release]; [entityIndices release]; free( entityBatches );
				[uiVertices release]; [readback release]; return qfalse;
			}
		}
		if ( readback || captureReadback ) {
			id<MTLBlitCommandEncoder> blit = [command blitCommandEncoder];
			if ( !blit ) {
				[worldVertices release]; [worldIndices release]; [worldDepth release];
				[entityVertices release]; [entityIndices release]; free( entityBatches );
				[uiVertices release]; [captureReadback release]; [readback release]; return qfalse;
			}
			if ( readback )
				[blit copyFromTexture:present->drawable.texture sourceSlice:0u sourceLevel:0u
					sourceOrigin:MTLOriginMake( receipt.readbackX, receipt.readbackY, 0u )
					sourceSize:MTLSizeMake( 1u, 1u, 1u ) toBuffer:readback
					destinationOffset:0u destinationBytesPerRow:256u
					destinationBytesPerImage:256u];
			if ( captureReadback )
				[blit copyFromTexture:present->drawable.texture sourceSlice:0u sourceLevel:0u
					sourceOrigin:MTLOriginMake( 0u, 0u, 0u )
					sourceSize:MTLSizeMake( receipt.width, receipt.height, 1u )
					toBuffer:captureReadback destinationOffset:0u
					destinationBytesPerRow:captureBytesPerRow
					destinationBytesPerImage:captureBytesPerRow * receipt.height];
			[blit endEncoding];
		}
		if ( Ral_CommandLifecyclePublishEnd( &lifecycle, &recording,
				&executable ) != ralSuccess ) {
			[worldVertices release]; [worldIndices release]; [worldDepth release];
			[entityVertices release]; [entityIndices release]; free( entityBatches );
			[uiVertices release]; [captureReadback release]; [readback release]; return qfalse;
		}
		[command presentDrawable:present->drawable];
		if ( readback ) {
			present->readbackCommands[readbackSlot] = [command retain];
			present->readbackGenerations[readbackSlot] = presentGeneration;
			present->readbackByteCounts[readbackSlot] = readbackBytes;
		}
		[command commit];
		if ( captureReadback || !present->lastReadbackReady )
			[command waitUntilCompleted];
		if ( !ReapReadbackRing( present, qfalse ) || !present->lastReadbackReady
				|| !RalMetal_CorePublishSubmission( present->core, &lifecycle, &executable,
					&receipt.submission ) ) {
			[captureReadback release]; [readback release];
			[worldVertices release]; [worldIndices release]; [worldDepth release];
			[entityVertices release]; [entityIndices release]; free( entityBatches );
			[uiVertices release];
			[present->drawable release]; present->drawable = nil;
			memset( &present->drawableReceipt, 0, sizeof( present->drawableReceipt ) );
			return qfalse;
		}
		if ( captureReadback ) {
			const byte *source = (const byte *)[captureReadback contents];
			const uint64_t rgbByteCount = (uint64_t)receipt.width * receipt.height * 3u;
			byte *rgb = rgbByteCount <= SIZE_MAX
				? (byte *)malloc( (size_t)rgbByteCount ) : NULL;
			if ( !rgb ) {
				[captureReadback release]; [readback release];
				[worldVertices release]; [worldIndices release]; [worldDepth release];
				[entityVertices release]; [entityIndices release]; free( entityBatches );
				[uiVertices release]; return qfalse;
			}
			for ( uint32_t y = 0u; y < receipt.height; ++y ) {
				const byte *sourceRow = source + (size_t)y * captureBytesPerRow;
				byte *targetRow = rgb + (size_t)( receipt.height - 1u - y )
					* receipt.width * 3u;
				for ( uint32_t x = 0u; x < receipt.width; ++x ) {
					targetRow[x * 3u + 0u] = sourceRow[x * 4u + 2u];
					targetRow[x * 3u + 1u] = sourceRow[x * 4u + 1u];
					targetRow[x * 3u + 2u] = sourceRow[x * 4u + 0u];
				}
			}
			free( present->captureRgb ); present->captureRgb = rgb;
			present->captureWidth = receipt.width;
			present->captureHeight = receipt.height;
			present->capturePending = qfalse; present->captureReady = qtrue;
			[captureReadback release]; captureReadback = nil;
		}
		receipt.readbackByteCount = present->lastReadbackByteCount;
		memcpy( receipt.readbackBytes, present->lastReadbackBytes,
			receipt.readbackByteCount );
		[readback release]; readback = nil;
		[worldVertices release]; worldVertices = nil;
		[worldIndices release]; worldIndices = nil;
		[worldDepth release]; worldDepth = nil;
		[entityVertices release]; entityVertices = nil;
		[entityIndices release]; entityIndices = nil;
		free( entityBatches ); entityBatches = NULL;
		[uiVertices release]; uiVertices = nil;
	}
	receipt.recording = recording; receipt.executable = executable;
	receipt.completionGeneration = receipt.submission.generation;
	receipt.clearDigest = DigestClear( clearColor ); receipt.presented = qtrue;
	receipt.readbackDigest = DigestBytes( receipt.readbackBytes,
		receipt.readbackByteCount );
	present->lastPresentGeneration = presentGeneration;
	[present->drawable release]; present->drawable = nil;
	memset( &present->drawableReceipt, 0, sizeof( present->drawableReceipt ) );
	if ( !PresentReceiptValid( &receipt ) ) return qfalse;
	*outReceipt = receipt;
	return qtrue;
}
