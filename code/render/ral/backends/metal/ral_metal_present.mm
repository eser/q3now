// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "ral_metal_present.h"
#include "ral_metal_internal.h"
#include "ral_metal_ui_metallib.h"
#include "ral_lighting_composition.h"
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static qboolean PresentFailure( const char *stage ) {
	fprintf( stderr, "Wired native Metal RAL: present failure stage=%s\n",
		stage ? stage : "unknown" );
	return qfalse;
}

typedef struct {
	qhandle_t handle;
	uint64_t generation;
	uint64_t digest;
	id<MTLTexture> texture;
} ralMetalMaterialCacheEntry_t;

#define RAL_METAL_EFFECT_SLOT_COUNT 8u
#define RAL_METAL_EFFECT_PARTICLES_PER_SLOT 1024u
#define RAL_METAL_EFFECT_DECAL_SLOT_COUNT 256u
#define RAL_METAL_EFFECT_BEAM_SLOT_COUNT RENDER_SUBMISSION_MAX_EFFECT_BEAMS

typedef struct {
	qboolean active;
	emitterDesc_t emitter;
	particleClass_t particleClass;
	float expiresAt;
} ralMetalEffectSlot_t;

typedef struct {
	qboolean active;
	decalDesc_t decal;
	float expiresAt;
} ralMetalEffectDecalSlot_t;

typedef struct {
	qboolean active;
	qboolean transient;
	beamDesc_t beam;
	float spawnTime;
	float expiresAt;
} ralMetalEffectBeamSlot_t;

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
	id<MTLRenderPipelineState> uiBackdropPipeline;
	id<MTLRenderPipelineState> toneMapPipeline;
	id<MTLRenderPipelineState> worldPipeline;
	id<MTLRenderPipelineState> worldBlendPipeline;
	id<MTLRenderPipelineState> worldAlphaAdditivePipeline;
	id<MTLRenderPipelineState> worldAdditivePipeline;
	id<MTLComputePipelineState> atmospherePipeline;
	id<MTLBuffer> atmosphereArena;
	id<MTLBuffer> atmosphereFallback;
	id<MTLBuffer> atmosphereVolumes;
	qboolean atmosphereHistoryValid;
	qboolean atmosphereHistoryParity;
	id<MTLComputePipelineState> weatherComputePipeline;
	id<MTLRenderPipelineState> weatherRenderPipeline;
	id<MTLRenderPipelineState> effectAdditivePipeline;
	id<MTLBuffer> weatherFrame;
	id<MTLBuffer> weatherPools[2];
	qboolean weatherParity;
	float weatherTimeline;
	id<MTLBuffer> effectFrame;
	id<MTLBuffer> effectPools[2];
	qboolean effectParity;
	float effectTimeline;
	ralMetalEffectSlot_t effectSlots[RAL_METAL_EFFECT_SLOT_COUNT];
	float effectDecalTimeline;
	ralMetalEffectDecalSlot_t effectDecalSlots[RAL_METAL_EFFECT_DECAL_SLOT_COUNT];
	float effectBeamTimeline;
	ralMetalEffectBeamSlot_t effectBeamSlots[RAL_METAL_EFFECT_BEAM_SLOT_COUNT];
	MTLPixelFormat weatherPixelFormat;
	id<MTLDepthStencilState> worldDepthState;
	id<MTLDepthStencilState> worldDepthReadState;
	id<MTLBuffer> worldVertexCache;
	id<MTLBuffer> worldIndexCache;
	id<MTLTexture> worldDepthCache;
	id<MTLTexture> sceneColorCache;
	float *worldBatchBounds;
	uint64_t worldCacheDigest;
	uint32_t worldCacheVertexCount;
	uint32_t worldCacheIndexCount;
	uint32_t worldDepthWidth;
	uint32_t worldDepthHeight;
	uint32_t sceneColorWidth;
	uint32_t sceneColorHeight;
	id<MTLTexture> uiFallbackTexture;
	id<MTLTexture> worldLightingFallbackTexture;
	id<MTLSamplerState> uiClampSampler;
	id<MTLSamplerState> uiRepeatSampler;
	id<MTLTexture> directionalLightingTextures[RAL_LIGHTING_RUNTIME_MAX_PLANES];
	ralMetalLightingReceipt_t directionalLightingReceipt;
	ralMetalMaterialCacheEntry_t uiMaterials[RENDER_SUBMISSION_MAX_MATERIALS];
	uint32_t uiMaterialCount;
	MTLPixelFormat uiPixelFormat;
	MTLPixelFormat worldPixelFormat;
	MTLPixelFormat toneMapPixelFormat;
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
	ralDisplayVisibilityPlan_t displayVisibility;
	float lightmapBoost;
	float shaderTimeOverride;
	struct {
		uint32_t mode;
		float exposure;
		float lottesContrast;
		float lottesShoulder;
		float lottesMidIn;
		float lottesMidOut;
		float lottesHdrMax;
		float pad;
		float displayVisibility[4];
	} toneMap;
};

typedef struct {
	float position[2];
	float color[4];
	float uv[2];
} ralMetalUiVertex_t;

typedef struct {
	float time, mouseX, mouseY, transition;
	float resX, resY, pad0, pad1;
} ralMetalMenuBgParams_t;

typedef struct {
	float worldPosition[4];
	float color[4];
	float texCoord[2];
	float lightmapCoord[2];
	float normal[4];
} ralMetalWorldVertex_t;

typedef struct {
	float viewOrigin[4];
	float forward[4];
	float left[4];
	float up[4];
	float projection[4];
} ralMetalWorldViewParams_t;

typedef struct {
	uint32_t meta[4];
	float params[4];
	float scaleScroll[4];
	float turbulence[4];
	float stretch[4];
} ralMetalMaterialStageParams_t;

typedef struct {
	uint32_t hasLightmap;
	uint32_t alphaMode;
	float alphaCutoff;
	uint32_t hasLocalIrradiance;
	float localSh[4][4];
	uint32_t staticLightingMode;
	uint32_t staticLightingLayer;
	uint32_t staticLightingEncoding;
	uint32_t hasStaticVisibility;
	float emissiveRadiance[4];
	float skyScaleScroll[4];
	float skySecondaryScaleScroll[4];
	float skyTime;
	uint32_t skySecondaryAlphaMode;
	float skyCloudHeight;
	float lightmapBoost;
	uint32_t stageProgram[4];
	ralMetalMaterialStageParams_t stages[RENDER_MATERIAL_MAX_STAGES];
} ralMetalWorldMaterialParams_t;

typedef struct {
	float eyeDensity[4];
	float colorVisibility[4];
	float heightCloud[4];
	uint32_t froxelGrid[4];
	float displayVisibility[4];
} ralMetalWorldAtmosphereParams_t;

typedef struct {
	uint32_t dimsStage[4];
	uint32_t bases0[4];
	uint32_t bases1[4];
	float boundsMin[4];
	float boundsMax[4];
	float climate[4];
	float lighting[4];
	float weather[4];
} ralMetalAtmosphereParams_t;

#define RAL_METAL_WEATHER_PARTICLE_CAPACITY 8192u

typedef struct {
	float mvp[16];
	float viewLeft[4];
	float viewUp[4];
	float eyeWorld[4];
	float dtTimePoolRead[4];
	float boundsMin[4];
	float boundsMax[4];
	float windGust[4];
	float precipitation[4];
	float weather[4];
	float sun[4];
	float moon[4];
	float ambientCloud[4];
	float cloudMedia[4];
	float effectMeta[4];
	renderAtmosphereEffectGpuWorkload_t
		effectWorkloads[RENDER_SUBMISSION_MAX_ATMOSPHERE_EFFECT_WORKLOADS];
} ralMetalWeatherFrame_t;

typedef char ralMetalWeatherFrameMustMatchMsl[
	sizeof( ralMetalWeatherFrame_t ) == 1056u ? 1 : -1];

typedef struct {
	float originRadius[4];
	float extentShape[4];
	float albedoAnisotropy[4];
	float emissiveIntensity[4];
	float media[4];
} ralMetalAtmosphereVolume_t;

#define RAL_METAL_ATMOSPHERE_FROXEL_CAPACITY 262144u
#define RAL_METAL_ATMOSPHERE_SECTION_COUNT 6u

typedef struct {
	uint32_t firstIndex;
	uint32_t indexCount;
	qhandle_t material;
	qboolean depthHack;
	qboolean minLight;
	qboolean useVertexColor;
	qboolean visible;
	renderAlphaMode_t alphaMode;
	renderCullMode_t cullMode;
	float alphaCutoff;
	qboolean depthWrite;
	qboolean surfaceDecal;
	qboolean hasLocalIrradiance;
	float localSh[4][3];
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
			uvs[0][0] = 0.0f; uvs[0][1] = 0.0f;
			uvs[1][0] = 1.0f; uvs[1][1] = 0.0f;
			uvs[2][0] = 1.0f; uvs[2][1] = 1.0f;
			uvs[3][0] = 0.0f; uvs[3][1] = 1.0f;
		}
		if ( primitive->material == INT_MAX ) {
			uvs[0][0] = 0.0f; uvs[0][1] = 0.0f;
			uvs[1][0] = 1.0f; uvs[1][1] = 0.0f;
			uvs[2][0] = 1.0f; uvs[2][1] = 1.0f;
			uvs[3][0] = 0.0f; uvs[3][1] = 1.0f;
		}
		for ( uint32_t corner = 0u; corner < 4u; ++corner ) {
			corners[corner][0] = primitive->positions[corner][0];
			corners[corner][1] = primitive->positions[corner][1];
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
		float centerX = ( first->positions[0][0] + first->positions[1][0]
			+ first->positions[2][0] + first->positions[3][0] ) * 0.25f;
		float centerY = ( first->positions[0][1] + first->positions[1][1]
			+ first->positions[2][1] + first->positions[3][1] ) * 0.25f;
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
	id<MTLFunction> vertexFunction = nil, fragmentFunction = nil;
	id<MTLFunction> msdfFunction = nil, backdropFunction = nil;
	NSError *error = nil;
	if ( !present || !present->drawable ) return qfalse;
	if ( present->uiPipeline && present->uiMsdfPipeline
			&& present->uiBackdropPipeline
			&& present->uiPixelFormat == present->drawable.texture.pixelFormat ) return qtrue;
	[present->uiPipeline release]; present->uiPipeline = nil;
	[present->uiMsdfPipeline release]; present->uiMsdfPipeline = nil;
	[present->uiBackdropPipeline release]; present->uiBackdropPipeline = nil;
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
	backdropFunction = [present->uiLibrary
		newFunctionWithName:@"wired_menu_bg_fragment"];
	if ( !vertexFunction || !fragmentFunction || !msdfFunction
			|| !backdropFunction ) goto fail;
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
		if ( present->uiMsdfPipeline && !error ) {
			pipeline.fragmentFunction = backdropFunction;
			present->uiBackdropPipeline = [device
				newRenderPipelineStateWithDescriptor:pipeline error:&error];
		}
	}
	[vertexFunction release]; vertexFunction = nil;
	[fragmentFunction release]; fragmentFunction = nil;
	[msdfFunction release]; msdfFunction = nil;
	[backdropFunction release]; backdropFunction = nil;
	if ( !present->uiPipeline || !present->uiMsdfPipeline
			|| !present->uiBackdropPipeline || error ) goto fail;
	present->uiPixelFormat = present->drawable.texture.pixelFormat;
	return qtrue;
fail:
	[vertexFunction release]; [fragmentFunction release]; [msdfFunction release];
	[backdropFunction release];
	[present->uiPipeline release]; present->uiPipeline = nil;
	[present->uiMsdfPipeline release]; present->uiMsdfPipeline = nil;
	[present->uiBackdropPipeline release]; present->uiBackdropPipeline = nil;
	return qfalse;
}

static qboolean EnsureUiSampling( ralMetalPresent_t *present );

static qboolean EnsureToneMapPipeline( ralMetalPresent_t *present ) {
	id<MTLDevice> device;
	id<MTLFunction> vertexFunction = nil, fragmentFunction = nil;
	NSError *error = nil;
	MTLPixelFormat outputFormat;
	if ( !present || !present->drawable || !EnsureUiSampling( present ) ) return qfalse;
	outputFormat = present->drawable.texture.pixelFormat;
	if ( present->toneMapPipeline
			&& present->toneMapPixelFormat == outputFormat ) return qtrue;
	[present->toneMapPipeline release]; present->toneMapPipeline = nil;
	device = RalMetal_CoreNativeDevice( present->core );
	if ( !present->uiLibrary ) {
		dispatch_data_t data = dispatch_data_create( ral_metal_ui_metallib,
			ral_metal_ui_metallib_size,
			dispatch_get_global_queue( QOS_CLASS_DEFAULT, 0 ), ^{} );
		if ( !data ) return qfalse;
		present->uiLibrary = [device newLibraryWithData:data error:&error];
		if ( !present->uiLibrary || error ) return qfalse;
	}
	vertexFunction = [present->uiLibrary newFunctionWithName:@"wired_fullscreen_vertex"];
	fragmentFunction = [present->uiLibrary newFunctionWithName:@"wired_tonemap_fragment"];
	if ( vertexFunction && fragmentFunction ) {
		MTLRenderPipelineDescriptor *pipeline =
			[[[MTLRenderPipelineDescriptor alloc] init] autorelease];
		pipeline.vertexFunction = vertexFunction;
		pipeline.fragmentFunction = fragmentFunction;
		pipeline.colorAttachments[0].pixelFormat = outputFormat;
		present->toneMapPipeline = [device
			newRenderPipelineStateWithDescriptor:pipeline error:&error];
	}
	[vertexFunction release]; [fragmentFunction release];
	if ( !present->toneMapPipeline || error ) {
		[present->toneMapPipeline release]; present->toneMapPipeline = nil;
		return qfalse;
	}
	present->toneMapPixelFormat = outputFormat;
	return qtrue;
}

static qboolean EnsureSceneColor( ralMetalPresent_t *present,
		uint32_t width, uint32_t height ) {
	id<MTLTexture> replacement;
	MTLTextureDescriptor *descriptor;
	if ( !present || !width || !height ) return qfalse;
	if ( present->sceneColorCache && present->sceneColorWidth == width
			&& present->sceneColorHeight == height ) return qtrue;
	descriptor = [MTLTextureDescriptor
		texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA16Float
		width:width height:height mipmapped:NO];
	descriptor.storageMode = MTLStorageModePrivate;
	descriptor.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
	replacement = [RalMetal_CoreNativeDevice( present->core )
		newTextureWithDescriptor:descriptor];
	if ( !replacement ) return qfalse;
	[present->sceneColorCache release];
	present->sceneColorCache = replacement;
	present->sceneColorWidth = width;
	present->sceneColorHeight = height;
	return qtrue;
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
	if ( !present->worldLightingFallbackTexture ) {
		static const uint8_t black[4] = { 0u, 0u, 0u, 0u };
		MTLTextureDescriptor *descriptor = [[[MTLTextureDescriptor alloc] init]
			autorelease];
		descriptor.textureType = MTLTextureType2DArray;
		descriptor.pixelFormat = MTLPixelFormatRGBA8Unorm;
		descriptor.width = descriptor.height = descriptor.arrayLength = 1u;
		descriptor.mipmapLevelCount = descriptor.sampleCount = 1u;
		descriptor.storageMode = MTLStorageModeShared;
		descriptor.usage = MTLTextureUsageShaderRead;
		present->worldLightingFallbackTexture = [device
			newTextureWithDescriptor:descriptor];
		if ( !present->worldLightingFallbackTexture ) return qfalse;
		[present->worldLightingFallbackTexture
			replaceRegion:MTLRegionMake2D( 0u, 0u, 1u, 1u ) mipmapLevel:0u
			slice:0u withBytes:black bytesPerRow:4u bytesPerImage:4u];
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

static void DrawUiPrimitiveRange( id<MTLRenderCommandEncoder> encoder,
		ralMetalPresent_t *present, id<MTLBuffer> vertices,
		const renderUiPrimitive_t *primitives,
		id<MTLTexture> *textures, id<MTLSamplerState> *samplers,
		const qboolean *msdf, const qboolean *backdrop,
		uint32_t first, uint32_t end, uint32_t width, uint32_t height ) {
	MTLViewport viewport;
	MTLScissorRect scissor;
	if ( !encoder || !present || !vertices || !primitives || first >= end ) return;
	viewport = (MTLViewport){ 0.0, 0.0, (double)width, (double)height, 0.0, 1.0 };
	scissor = (MTLScissorRect){ 0u, 0u, width, height };
	[encoder setViewport:viewport];
	[encoder setScissorRect:scissor];
	[encoder setCullMode:MTLCullModeNone];
	/* UI primitives are submitted in painter's order. Do not inherit the
	 * world pass depth state: a full-screen backdrop would otherwise write
	 * depth and reject every following primitive at the same clip depth. */
	[encoder setDepthStencilState:nil];
	[encoder setVertexBuffer:vertices offset:0u atIndex:0u];
	for ( uint32_t primitive = first; primitive < end; ++primitive ) {
		[encoder setRenderPipelineState:backdrop[primitive]
			? present->uiBackdropPipeline
			: ( msdf[primitive] ? present->uiMsdfPipeline
				: present->uiPipeline )];
		if ( backdrop[primitive] ) {
			ralMetalMenuBgParams_t params;
			params.time = primitives[primitive].s1;
			params.mouseX = primitives[primitive].t1;
			params.mouseY = primitives[primitive].s2;
			params.transition = primitives[primitive].t2;
			params.resX = width; params.resY = height;
			params.pad0 = params.pad1 = 0.0f;
			[encoder setFragmentBytes:&params length:sizeof( params ) atIndex:0u];
		}
		[encoder setFragmentTexture:textures[primitive] atIndex:0u];
		[encoder setFragmentSamplerState:samplers[primitive] atIndex:0u];
		[encoder drawPrimitives:MTLPrimitiveTypeTriangle
			vertexStart:(NSUInteger)primitive * 6u vertexCount:6u];
	}
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
		memcpy( vertices[i].normal, snapshot.vertices[i].normal,
			3u * sizeof( float ) );
	}
	for ( uint32_t batch = 0u; batch < snapshot.batchCount; ++batch ) {
		const renderWorldBatch_t *draw = &snapshot.batches[batch];
		if ( !draw->indexCount || draw->indexCount % 3u
				|| draw->firstIndex > snapshot.indexCount - draw->indexCount
				|| draw->alphaMode < RENDER_ALPHA_OPAQUE
				|| draw->alphaMode > RENDER_ALPHA_ALPHA_ADDITIVE
				|| draw->cullMode < RENDER_CULL_BACK
				|| draw->cullMode > RENDER_CULL_NONE
				|| !isfinite( draw->alphaCutoff ) || draw->alphaCutoff < 0.0f
				|| draw->alphaCutoff > 1.0f
				|| !isfinite( draw->sort ) || draw->sort <= 0.0f
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
	vertex->normal[2] = 1.0f;
}

static qboolean EntityBatchVisible( const ralMetalWorldVertex_t *vertices,
		uint32_t firstVertex, uint32_t vertexCount,
		const renderWorldSnapshot_t *view, qboolean depthHack ) {
	float bounds[6];
	ralMetalWorldViewParams_t params;
	if ( !vertices || !vertexCount || !view ) return qfalse;
	for ( uint32_t axis = 0u; axis < 3u; ++axis )
		bounds[axis] = bounds[axis + 3u] =
			vertices[firstVertex].worldPosition[axis];
	for ( uint32_t vertex = 1u; vertex < vertexCount; ++vertex ) {
		for ( uint32_t axis = 0u; axis < 3u; ++axis ) {
			const float value = vertices[firstVertex + vertex].worldPosition[axis];
			if ( value < bounds[axis] ) bounds[axis] = value;
			if ( value > bounds[axis + 3u] ) bounds[axis + 3u] = value;
		}
	}
	/* First-person geometry intentionally intersects the ordinary near plane;
	 * RF_DEPTHHACK owns its bounded depth range instead of world frustum culling. */
	if ( depthHack ) return qtrue;
	memset( &params, 0, sizeof( params ) );
	memcpy( params.viewOrigin, view->viewOrigin, 3u * sizeof( float ) );
	memcpy( params.forward, view->viewAxis[0], 3u * sizeof( float ) );
	memcpy( params.left, view->viewAxis[1], 3u * sizeof( float ) );
	memcpy( params.up, view->viewAxis[2], 3u * sizeof( float ) );
	params.projection[0] = 1.0f / tanf( view->fovX * 0.00872664625997164788f );
	params.projection[1] = 1.0f / tanf( view->fovY * 0.00872664625997164788f );
	params.projection[2] = 4.0f;
	return WorldBatchVisible( &params, bounds );
}

static qboolean BuildWorldViewParams( const renderSubmissionState_t *frontend,
		ralMetalWorldViewParams_t *outParams,
		renderWorldSnapshot_t *outView ) {
	renderWorldSnapshot_t view;
	float tanX, tanY;
	/* UI-owned entity previews use RDF_NOWORLDMODEL: they have a valid view
	 * receipt but intentionally do not publish a BSP world receipt. */
	if ( !frontend || !outParams
			|| !RenderSubmission_ViewSnapshot( frontend, &view )
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
	if ( outView ) *outView = view;
	return qtrue;
}

static qboolean BuildWorldAtmosphereParams(
		const renderSubmissionState_t *frontend,
		const ralMetalWorldViewParams_t *view,
		const ralAtmospherePlanReceipt_t *plan,
		const ralDisplayVisibilityPlan_t *visibility,
		ralMetalWorldAtmosphereParams_t *outParams ) {
	renderAtmosphereSnapshot_t snapshot;
	const atmosphereEmitter_t *emitters;
	float sunWeight, lightning;
	if ( !frontend || !view || !plan || !outParams
			|| !Ral_DisplayVisibilityPlanValid( visibility ) ) return qfalse;
	memset( outParams, 0, sizeof( *outParams ) );
	memcpy( outParams->eyeDensity, view->viewOrigin, 3u * sizeof( float ) );
	outParams->displayVisibility[0] = visibility->exposureScale;
	outParams->displayVisibility[1] = visibility->shadowExponent;
	outParams->displayVisibility[2] = visibility->shadowPivot;
	outParams->displayVisibility[3] = visibility->userBrightness;
	if ( !RenderSubmission_AtmosphereSnapshot( frontend, &snapshot,
			&emitters ) ) return qfalse;
	(void)emitters;
	if ( !snapshot.active
			|| snapshot.state.qualityTier < ATMOSPHERE_QUALITY_ANALYTIC )
		return qtrue;
	outParams->eyeDensity[3] = snapshot.state.mediaDensity;
	outParams->colorVisibility[3] = snapshot.state.visibility;
	sunWeight = snapshot.state.sunIntensity * ( 1.0f
		- snapshot.state.cloudCover * snapshot.state.cloudShadow );
	lightning = snapshot.state.lightning;
	for ( uint32_t channel = 0u; channel < 3u; ++channel )
		outParams->colorVisibility[channel] = fminf( 16.0f,
			snapshot.state.ambientColor[channel]
			+ 0.08f * sunWeight + lightning );
	outParams->heightCloud[0] = snapshot.state.bounds[2];
	outParams->heightCloud[1] = snapshot.state.mediaHeightFalloff;
	outParams->heightCloud[2] = snapshot.state.cloudCover;
	outParams->heightCloud[3] = snapshot.state.cloudShadow;
	if ( plan->selectedTier == RAL_ATMOSPHERE_TIER_FULL ) {
		outParams->froxelGrid[0] = plan->froxelWidth;
		outParams->froxelGrid[1] = plan->froxelHeight;
		outParams->froxelGrid[2] = plan->froxelDepth;
		outParams->froxelGrid[3] = 3u * RAL_METAL_ATMOSPHERE_FROXEL_CAPACITY;
	}
	return qtrue;
}

static qboolean EntityBatchMaterial( const renderSubmissionState_t *frontend,
		qhandle_t material, ralMetalEntityBatch_t *batch ) {
	renderMaterialSnapshot_t snapshot;
	batch->material = material; batch->alphaMode = RENDER_ALPHA_OPAQUE;
	batch->cullMode = RENDER_CULL_BACK;
	batch->alphaCutoff = 0.5f; batch->depthWrite = qtrue;
	if ( material > 0 && RenderSubmission_MaterialSnapshot( frontend, material,
			&snapshot ) ) {
		batch->alphaMode = snapshot.alphaMode;
		batch->cullMode = snapshot.cullMode;
		batch->alphaCutoff = snapshot.alphaCutoff;
		batch->depthWrite = snapshot.depthWrite;
		if ( snapshot.noDraw ) batch->visible = qfalse;
	}
	return qtrue;
}

static void EntityBatchLighting( const renderEntityCommand_t *command,
		ralMetalEntityBatch_t *batch ) {
	if ( !command || !batch || !command->hasLocalIrradiance ) return;
	batch->hasLocalIrradiance = qtrue;
	for ( uint32_t coefficient = 0u; coefficient < 4u; ++coefficient )
		for ( uint32_t channel = 0u; channel < 3u; ++channel )
			batch->localSh[coefficient][channel] =
				(float)command->localIrradiance
					.blendedCoefficientsQ16[coefficient][channel]
				/ (float)RAL_LIGHT_Q16_ONE;
}

static qboolean UpdatePersistentEffectDecals( ralMetalPresent_t *present,
		const renderSubmissionState_t *frontend, uint32_t *outActiveCount,
		uint32_t *outDroppedCount ) {
	renderEffectPrimitiveSnapshot_t snapshot;
	renderWorldSnapshot_t view;
	const spriteDesc_t *sprites;
	const emitterDesc_t *emitters;
	const decalDesc_t *decals;
	const renderEffectRibbonCommand_t *ribbons;
	const ribbonPoint_t *points;
	const beamDesc_t *beams;
	float now;
	uint32_t activeCount = 0u, droppedCount = 0u;
	if ( !present || !frontend || !outActiveCount || !outDroppedCount
			|| !RenderSubmission_EffectPrimitiveSnapshots( frontend, &snapshot,
				&sprites, &emitters, &decals, &ribbons, &points, &beams ) ) return qfalse;
	(void)sprites; (void)emitters; (void)ribbons; (void)points; (void)beams;
	/* Loading and registration frames legitimately have no world view.  They
	 * cannot author a new world-space decal, but an empty frame must remain
	 * presentable and must not discard an already bounded persistent pool. */
	if ( !RenderSubmission_ViewSnapshot( frontend, &view ) ) {
		if ( snapshot.decalCount ) return qfalse;
		for ( uint32_t slot = 0u; slot < RAL_METAL_EFFECT_DECAL_SLOT_COUNT; ++slot )
			if ( present->effectDecalSlots[slot].active ) activeCount++;
		*outActiveCount = activeCount;
		*outDroppedCount = 0u;
		return qtrue;
	}
	now = (float)view.timeMs * 0.001f;
	if ( !isfinite( now ) || now < 0.0f ) return qfalse;
	if ( present->effectDecalTimeline > now )
		memset( present->effectDecalSlots, 0,
			sizeof( present->effectDecalSlots ) );
	for ( uint32_t slot = 0u; slot < RAL_METAL_EFFECT_DECAL_SLOT_COUNT; ++slot )
		if ( present->effectDecalSlots[slot].active
				&& now >= present->effectDecalSlots[slot].expiresAt )
			present->effectDecalSlots[slot].active = qfalse;
	for ( uint32_t decalIndex = 0u; decalIndex < snapshot.decalCount;
			++decalIndex ) {
		uint32_t slot;
		for ( slot = 0u; slot < RAL_METAL_EFFECT_DECAL_SLOT_COUNT; ++slot )
			if ( !present->effectDecalSlots[slot].active ) break;
		if ( slot == RAL_METAL_EFFECT_DECAL_SLOT_COUNT ) {
			droppedCount++; continue;
		}
		present->effectDecalSlots[slot].active = qtrue;
		present->effectDecalSlots[slot].decal = decals[decalIndex];
		present->effectDecalSlots[slot].expiresAt = now
			+ fmaxf( decals[decalIndex].lifetime, 1.0f / 60.0f );
	}
	for ( uint32_t slot = 0u; slot < RAL_METAL_EFFECT_DECAL_SLOT_COUNT; ++slot )
		if ( present->effectDecalSlots[slot].active ) activeCount++;
	present->effectDecalTimeline = now;
	*outActiveCount = activeCount;
	*outDroppedCount = droppedCount;
	return qtrue;
}

static qboolean UpdatePersistentEffectBeams( ralMetalPresent_t *present,
		const renderSubmissionState_t *frontend, uint32_t *outActiveCount,
		uint32_t *outDroppedCount ) {
	renderEffectPrimitiveSnapshot_t snapshot;
	renderWorldSnapshot_t view;
	const spriteDesc_t *sprites;
	const emitterDesc_t *emitters;
	const decalDesc_t *decals;
	const renderEffectRibbonCommand_t *ribbons;
	const ribbonPoint_t *points;
	const beamDesc_t *beams;
	float now;
	uint32_t activeCount = 0u, droppedCount = 0u;
	if ( !present || !frontend || !outActiveCount || !outDroppedCount
			|| !RenderSubmission_EffectPrimitiveSnapshots( frontend, &snapshot,
				&sprites, &emitters, &decals, &ribbons, &points, &beams ) ) return qfalse;
	(void)sprites; (void)emitters; (void)decals; (void)ribbons; (void)points;
	/* A renderer switch and the menu/loading path can submit UI before a world
	 * view exists. Match the persistent-decal contract: an empty viewless frame
	 * is presentable and preserves the bounded backend pool; only reject a frame
	 * that attempts to author new world-space beams without a view/timeline. */
	if ( !RenderSubmission_ViewSnapshot( frontend, &view ) ) {
		if ( snapshot.beamCount ) return qfalse;
		for ( uint32_t slot = 0u; slot < RAL_METAL_EFFECT_BEAM_SLOT_COUNT; ++slot )
			if ( present->effectBeamSlots[slot].active ) activeCount++;
		*outActiveCount = activeCount;
		*outDroppedCount = 0u;
		return qtrue;
	}
	now = (float)view.timeMs * 0.001f;
	if ( !isfinite( now ) || now < 0.0f ) return qfalse;
	if ( present->effectBeamTimeline > now )
		memset( present->effectBeamSlots, 0, sizeof( present->effectBeamSlots ) );
	for ( uint32_t slot = 0u; slot < RAL_METAL_EFFECT_BEAM_SLOT_COUNT; ++slot ) {
		ralMetalEffectBeamSlot_t *active = &present->effectBeamSlots[slot];
		if ( active->active && ( active->transient || now >= active->expiresAt ) )
			active->active = qfalse;
	}
	for ( uint32_t beamIndex = 0u; beamIndex < snapshot.beamCount; ++beamIndex ) {
		uint32_t slot;
		for ( slot = 0u; slot < RAL_METAL_EFFECT_BEAM_SLOT_COUNT; ++slot )
			if ( !present->effectBeamSlots[slot].active ) break;
		if ( slot == RAL_METAL_EFFECT_BEAM_SLOT_COUNT ) {
			droppedCount++; continue;
		}
		present->effectBeamSlots[slot].active = qtrue;
		present->effectBeamSlots[slot].transient = beams[beamIndex].duration <= 0.0f
			? qtrue : qfalse;
		present->effectBeamSlots[slot].beam = beams[beamIndex];
		present->effectBeamSlots[slot].spawnTime = now;
		present->effectBeamSlots[slot].expiresAt = now
			+ fmaxf( beams[beamIndex].duration, 1.0f / 60.0f );
	}
	for ( uint32_t slot = 0u; slot < RAL_METAL_EFFECT_BEAM_SLOT_COUNT; ++slot )
		if ( present->effectBeamSlots[slot].active ) activeCount++;
	present->effectBeamTimeline = now;
	*outActiveCount = activeCount;
	*outDroppedCount = droppedCount;
	return qtrue;
}

static void ResolveEffectBeamEndpoint( const renderSubmissionState_t *frontend,
		const beamDesc_t *beam, qboolean start, float out[3] ) {
	const int entityNum = start ? beam->startEntityNum : beam->endEntityNum;
	const float *position = start ? beam->start : beam->end;
	const float *offset = start ? beam->startOffset : beam->endOffset;
	if ( frontend && entityNum >= 0 && (uint32_t)entityNum < frontend->entityCount ) {
		for ( uint32_t axis = 0u; axis < 3u; ++axis )
			out[axis] = frontend->entities[entityNum].entity.origin[axis] + offset[axis];
	} else {
		memcpy( out, position, 3u * sizeof( float ) );
	}
}

static float EffectBeamFade( const ralMetalEffectBeamSlot_t *slot, float now ) {
	float fade = 1.0f;
	float age;
	if ( !slot || slot->transient || slot->beam.duration <= 0.0f ) return 1.0f;
	age = fmaxf( now - slot->spawnTime, 0.0f );
	if ( slot->beam.fadeIn > 0.0f ) fade = fminf( fade, age / slot->beam.fadeIn );
	if ( slot->beam.fadeOut > 0.0f ) fade = fminf( fade,
		fmaxf( slot->beam.duration - age, 0.0f ) / slot->beam.fadeOut );
	return fminf( 1.0f, fmaxf( 0.0f, fade ) );
}

static void RotateEffectBeamAxis( const float vector[3], const float axis[3],
		float angle, float out[3] ) {
	const float cosine = cosf( angle ), sine = sinf( angle );
	const float dot = vector[0] * axis[0] + vector[1] * axis[1]
		+ vector[2] * axis[2];
	const float cross[3] = {
		axis[1] * vector[2] - axis[2] * vector[1],
		axis[2] * vector[0] - axis[0] * vector[2],
		axis[0] * vector[1] - axis[1] * vector[0]
	};
	for ( uint32_t component = 0u; component < 3u; ++component )
		out[component] = vector[component] * cosine + cross[component] * sine
			+ axis[component] * dot * ( 1.0f - cosine );
}

static qboolean BuildEntityVertices( const ralMetalPresent_t *present,
		const renderSubmissionState_t *frontend,
		ralMetalWorldVertex_t **outVertices, uint32_t **outIndices,
		ralMetalEntityBatch_t **outBatches, uint32_t *outVertexCount,
		uint32_t *outIndexCount, uint32_t *outBatchCount,
		uint32_t *outModelCount, uint32_t *outPrimitiveCount,
		uint32_t *outTemporalCount, uint32_t *outLocalIrradianceCount,
		uint32_t *outUnresolvedCount ) {
	const renderEntityCommand_t *commands;
	renderEffectPrimitiveSnapshot_t effectSnapshot;
	const spriteDesc_t *effectSprites = NULL;
	const emitterDesc_t *effectEmitters = NULL;
	const decalDesc_t *effectDecals = NULL;
	const renderEffectRibbonCommand_t *effectRibbons = NULL;
	const ribbonPoint_t *effectRibbonPoints = NULL;
	const beamDesc_t *effectBeams = NULL;
	renderWorldSnapshot_t view;
	uint32_t commandCount, vertexCount = 0u, indexCount = 0u, batchCount = 0u;
	qboolean haveEffects = qfalse, havePersistentDecals = qfalse;
	qboolean havePersistentBeams = qfalse;
	if ( !outVertices || !outIndices || !outBatches || !outVertexCount
			|| !outIndexCount || !outBatchCount || !outModelCount
			|| !outPrimitiveCount || !outTemporalCount
			|| !outLocalIrradianceCount || !outUnresolvedCount ) return qfalse;
	*outVertices = NULL; *outIndices = NULL; *outBatches = NULL;
	*outVertexCount = *outIndexCount = *outBatchCount = 0u;
	*outModelCount = *outPrimitiveCount = *outTemporalCount = 0u;
	*outLocalIrradianceCount = *outUnresolvedCount = 0u;
	commands = RenderSubmission_EntityCommands( frontend, &commandCount );
	memset( &effectSnapshot, 0, sizeof( effectSnapshot ) );
	if ( frontend ) haveEffects = RenderSubmission_EffectPrimitiveSnapshots(
		frontend, &effectSnapshot, &effectSprites, &effectEmitters,
		&effectDecals, &effectRibbons, &effectRibbonPoints, &effectBeams );
	(void)effectEmitters; (void)effectBeams;
	if ( present ) for ( uint32_t slot = 0u;
			slot < RAL_METAL_EFFECT_DECAL_SLOT_COUNT; ++slot )
		if ( present->effectDecalSlots[slot].active ) {
			havePersistentDecals = qtrue; break;
		}
	if ( present ) for ( uint32_t slot = 0u;
			slot < RAL_METAL_EFFECT_BEAM_SLOT_COUNT; ++slot )
		if ( present->effectBeamSlots[slot].active ) {
			havePersistentBeams = qtrue; break;
		}
	if ( ( !commands || !commandCount ) && ( !haveEffects
			|| ( !effectSnapshot.spriteCount && !havePersistentDecals
				&& !effectSnapshot.ribbonCount && !havePersistentBeams ) ) ) return qtrue;
	/* Entity-only UI subscenes are valid without a loaded BSP world. */
	if ( !RenderSubmission_ViewSnapshot( frontend, &view ) ) return qfalse;
	for ( uint32_t i = 0u; i < commandCount; ++i ) {
		const refEntity_t *entity = &commands[i].entity;
		/* RF_THIRD_PERSON means mirror/portal-only in the established renderer.
		 * Drawing it in the primary view puts the camera inside the local player
		 * model, which appeared as giant black/untextured polygons on Metal. */
		if ( entity->renderfx & RF_THIRD_PERSON ) continue;
		if ( commands[i].hasTemporal ) ( *outTemporalCount )++;
		if ( commands[i].hasLocalIrradiance ) {
			if ( !Ral_IrradianceEntitySampleReceiptValid(
					&commands[i].localIrradiance )
					|| !Ral_LightingCompositionReceiptValid(
						&commands[i].lightingComposition )
					|| commands[i].lightingComposition.diffuseAuthority
						!= RAL_LIGHTING_DIFFUSE_AUTHORITY_LOCAL_SH
					|| commands[i].lightingComposition.activeTermMask
						!= RAL_LIGHTING_TERM_LOCAL_SH )
				return qfalse;
			( *outLocalIrradianceCount )++;
		}
		if ( entity->reType == RT_MODEL ) {
			renderModelSnapshot_t model;
			if ( entity->hModel <= 0 ) continue;
			if ( !RenderSubmission_ModelSnapshot( frontend, entity->hModel, &model )
					|| !model.normals ) {
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
	if ( haveEffects ) {
		for ( uint32_t i = 0u; i < effectSnapshot.spriteCount; ++i ) {
			if ( vertexCount > RENDER_SUBMISSION_MAX_WORLD_VERTICES - 4u
					|| indexCount > RENDER_SUBMISSION_MAX_WORLD_INDICES - 6u
					|| batchCount >= RENDER_SUBMISSION_MAX_WORLD_BATCHES ) return qfalse;
			vertexCount += 4u; indexCount += 6u; batchCount++;
		}
		for ( uint32_t i = 0u; present
				&& i < RAL_METAL_EFFECT_DECAL_SLOT_COUNT; ++i ) {
			if ( !present->effectDecalSlots[i].active ) continue;
			if ( vertexCount > RENDER_SUBMISSION_MAX_WORLD_VERTICES - 4u
					|| indexCount > RENDER_SUBMISSION_MAX_WORLD_INDICES - 6u
					|| batchCount >= RENDER_SUBMISSION_MAX_WORLD_BATCHES ) return qfalse;
			vertexCount += 4u; indexCount += 6u; batchCount++;
		}
		for ( uint32_t i = 0u; i < effectSnapshot.ribbonCount; ++i ) {
			const uint32_t points = effectRibbons[i].pointCount;
			const uint32_t ribbonVertices = points * 2u;
			const uint32_t ribbonIndices = ( points - 1u ) * 6u;
			if ( points < 2u
					|| vertexCount > RENDER_SUBMISSION_MAX_WORLD_VERTICES - ribbonVertices
					|| indexCount > RENDER_SUBMISSION_MAX_WORLD_INDICES - ribbonIndices
					|| batchCount >= RENDER_SUBMISSION_MAX_WORLD_BATCHES ) return qfalse;
			vertexCount += ribbonVertices; indexCount += ribbonIndices; batchCount++;
		}
		for ( uint32_t slot = 0u; present
				&& slot < RAL_METAL_EFFECT_BEAM_SLOT_COUNT; ++slot ) {
			if ( !present->effectBeamSlots[slot].active ) continue;
			const uint32_t copies = (uint32_t)present->effectBeamSlots[slot].beam.axialCopies;
			const uint32_t beamVertices = copies * 4u;
			const uint32_t beamIndices = copies * 6u;
			if ( vertexCount > RENDER_SUBMISSION_MAX_WORLD_VERTICES - beamVertices
					|| indexCount > RENDER_SUBMISSION_MAX_WORLD_INDICES - beamIndices
					|| batchCount >= RENDER_SUBMISSION_MAX_WORLD_BATCHES ) return qfalse;
			vertexCount += beamVertices; indexCount += beamIndices; batchCount++;
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
		if ( entity->renderfx & RF_THIRD_PERSON ) continue;
		if ( entity->reType == RT_MODEL ) {
			renderModelSnapshot_t model;
			if ( !RenderSubmission_ModelSnapshot( frontend, entity->hModel, &model ) ) continue;
			uint32_t frame = entity->frame < 0 ? 0u : (uint32_t)entity->frame % model.frameCount;
			uint32_t oldFrame = entity->oldframe < 0 ? frame
				: (uint32_t)entity->oldframe % model.frameCount;
			float oldWeight = entity->backlerp, newWeight = 1.0f - oldWeight;
			for ( uint32_t vertexIndex = 0u; vertexIndex < model.vertexCount; ++vertexIndex ) {
				float local[3], localNormal[3], world[3], worldNormal[3];
				for ( uint32_t axis = 0u; axis < 3u; ++axis ) {
					float current = model.positions[((size_t)frame * model.vertexCount
						+ vertexIndex ) * 3u + axis];
					float old = model.positions[((size_t)oldFrame * model.vertexCount
						+ vertexIndex ) * 3u + axis];
					local[axis] = current * newWeight + old * oldWeight;
					current = model.normals[((size_t)frame * model.vertexCount
						+ vertexIndex ) * 3u + axis];
					old = model.normals[((size_t)oldFrame * model.vertexCount
						+ vertexIndex ) * 3u + axis];
					localNormal[axis] = current * newWeight + old * oldWeight;
				}
				for ( uint32_t axis = 0u; axis < 3u; ++axis ) {
					world[axis] = entity->origin[axis] + entity->axis[0][axis] * local[0]
						+ entity->axis[1][axis] * local[1] + entity->axis[2][axis] * local[2];
					worldNormal[axis] = entity->axis[0][axis] * localNormal[0]
						+ entity->axis[1][axis] * localNormal[1]
						+ entity->axis[2][axis] * localNormal[2];
				}
				EntityVertex( &(*outVertices)[vertexBase + vertexIndex], &view, world,
					model.texCoords[vertexIndex * 2u], model.texCoords[vertexIndex * 2u + 1u],
					entity->shader );
				{
					ralMetalWorldVertex_t *written = &(*outVertices)[vertexBase + vertexIndex];
					float length = sqrtf( worldNormal[0] * worldNormal[0]
						+ worldNormal[1] * worldNormal[1]
						+ worldNormal[2] * worldNormal[2] );
					if ( length > 0.000001f ) for ( uint32_t axis = 0u; axis < 3u; ++axis )
						written->normal[axis] = worldNormal[axis] / length;
				}
			}
			for ( uint32_t index = 0u; index < model.indexCount; ++index )
				(*outIndices)[indexBase + index] = vertexBase + model.indices[index];
			for ( uint32_t batchIndex = 0u; batchIndex < model.batchCount; ++batchIndex ) {
				ralMetalEntityBatch_t *batch = &(*outBatches)[batchBase++];
				const renderModelBatch_t *modelBatch = &model.batches[batchIndex];
				batch->firstIndex = indexBase + model.batches[batchIndex].firstIndex;
				batch->indexCount = model.batches[batchIndex].indexCount;
				batch->depthHack = ( entity->renderfx & RF_DEPTHHACK ) ? qtrue : qfalse;
				batch->minLight = ( entity->renderfx & RF_MINLIGHT )
					? qtrue : qfalse;
				/* Default MD3/IQM stages are identity-coloured.  Stage-authored
				 * rgbGen semantics will opt in through the material contract. */
				batch->useVertexColor = qfalse;
				batch->visible = EntityBatchVisible( *outVertices,
					vertexBase + modelBatch->firstVertex, modelBatch->vertexCount,
					&view, batch->depthHack );
				EntityBatchMaterial( frontend,
					RenderSubmission_EntityBatchMaterial( frontend, &commands[i],
						&model.batches[batchIndex] ), batch );
				EntityBatchLighting( &commands[i], batch );
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
			batch->depthHack = ( entity->renderfx & RF_DEPTHHACK ) ? qtrue : qfalse;
			batch->minLight = ( entity->renderfx & RF_MINLIGHT )
				? qtrue : qfalse;
			batch->useVertexColor = qtrue;
			/* Procedural sprite and beam submissions are already small, bounded
			 * primitives.  Let the GPU clip them: deriving an AABB from a
			 * camera-facing quad is not conservative for lens-flare chains and can
			 * reject a sprite that crosses a side plane after billboard rotation. */
			batch->visible = qtrue;
			EntityBatchMaterial( frontend, entity->customShader, batch );
			batch->cullMode = RENDER_CULL_NONE;
			EntityBatchLighting( &commands[i], batch );
		}
	}
	if ( haveEffects ) {
		static const uint32_t quadOrder[6] = { 0u, 1u, 2u, 0u, 2u, 3u };
		for ( uint32_t i = 0u; i < effectSnapshot.spriteCount; ++i ) {
			const spriteDesc_t *sprite = &effectSprites[i];
			float corners[4][3];
			color4ub_t color;
			for ( uint32_t channel = 0u; channel < 4u; ++channel )
				color.rgba[channel] = (byte)fminf( 255.0f,
					fmaxf( 0.0f, sprite->rgba[channel] * 255.0f ) );
			for ( uint32_t axis = 0u; axis < 3u; ++axis ) {
				const float left = view.viewAxis[1][axis] * sprite->radius;
				const float up = view.viewAxis[2][axis] * sprite->radius;
				corners[0][axis] = sprite->origin[axis] + left + up;
				corners[1][axis] = sprite->origin[axis] - left + up;
				corners[2][axis] = sprite->origin[axis] - left - up;
				corners[3][axis] = sprite->origin[axis] + left - up;
			}
			static const float uv[4][2] = { { 0, 0 }, { 1, 0 }, { 1, 1 }, { 0, 1 } };
			for ( uint32_t corner = 0u; corner < 4u; ++corner )
				EntityVertex( &(*outVertices)[vertexBase + corner], &view,
					corners[corner], uv[corner][0], uv[corner][1], color );
			for ( uint32_t index = 0u; index < 6u; ++index )
				(*outIndices)[indexBase + index] = vertexBase + quadOrder[index];
			ralMetalEntityBatch_t *batch = &(*outBatches)[batchBase++];
			batch->firstIndex = indexBase; batch->indexCount = 6u;
			batch->useVertexColor = qtrue; batch->visible = qtrue;
			EntityBatchMaterial( frontend, sprite->shader, batch );
			batch->cullMode = RENDER_CULL_NONE; batch->depthWrite = qfalse;
			if ( sprite->flags & PRIM_FLAG_ADDITIVE )
				batch->alphaMode = RENDER_ALPHA_ADDITIVE;
			vertexBase += 4u; indexBase += 6u;
		}
		for ( uint32_t i = 0u; present
				&& i < RAL_METAL_EFFECT_DECAL_SLOT_COUNT; ++i ) {
			if ( !present->effectDecalSlots[i].active ) continue;
			const decalDesc_t *decal = &present->effectDecalSlots[i].decal;
			float seed[3] = { fabsf( decal->normal[2] ) < 0.9f ? 0.0f : 1.0f,
				0.0f, fabsf( decal->normal[2] ) < 0.9f ? 1.0f : 0.0f };
			float tangent[3], bitangent[3], corners[4][3], tangentLength;
			color4ub_t color;
			tangent[0] = decal->normal[1] * seed[2] - decal->normal[2] * seed[1];
			tangent[1] = decal->normal[2] * seed[0] - decal->normal[0] * seed[2];
			tangent[2] = decal->normal[0] * seed[1] - decal->normal[1] * seed[0];
			tangentLength = sqrtf( tangent[0] * tangent[0] + tangent[1] * tangent[1]
				+ tangent[2] * tangent[2] );
			for ( uint32_t axis = 0u; axis < 3u; ++axis ) tangent[axis] /= tangentLength;
			bitangent[0] = decal->normal[1] * tangent[2] - decal->normal[2] * tangent[1];
			bitangent[1] = decal->normal[2] * tangent[0] - decal->normal[0] * tangent[2];
			bitangent[2] = decal->normal[0] * tangent[1] - decal->normal[1] * tangent[0];
			const float cosine = cosf( decal->orientation );
			const float sine = sinf( decal->orientation );
			for ( uint32_t axis = 0u; axis < 3u; ++axis ) {
				const float rotatedTangent = ( tangent[axis] * cosine
					+ bitangent[axis] * sine ) * decal->radius;
				const float rotatedBitangent = ( bitangent[axis] * cosine
					- tangent[axis] * sine ) * decal->radius;
				const float center = decal->origin[axis] + decal->normal[axis] * 0.25f;
				corners[0][axis] = center - rotatedTangent - rotatedBitangent;
				corners[1][axis] = center + rotatedTangent - rotatedBitangent;
				corners[2][axis] = center + rotatedTangent + rotatedBitangent;
				corners[3][axis] = center - rotatedTangent + rotatedBitangent;
			}
			for ( uint32_t channel = 0u; channel < 4u; ++channel )
				color.rgba[channel] = (byte)fminf( 255.0f,
					fmaxf( 0.0f, decal->rgba[channel] * 255.0f ) );
			static const float uv[4][2] = { { 0, 0 }, { 1, 0 }, { 1, 1 }, { 0, 1 } };
			for ( uint32_t corner = 0u; corner < 4u; ++corner ) {
				EntityVertex( &(*outVertices)[vertexBase + corner], &view,
					corners[corner], uv[corner][0], uv[corner][1], color );
				memcpy( (*outVertices)[vertexBase + corner].normal, decal->normal,
					3u * sizeof( float ) );
			}
			for ( uint32_t index = 0u; index < 6u; ++index )
				(*outIndices)[indexBase + index] = vertexBase + quadOrder[index];
			ralMetalEntityBatch_t *batch = &(*outBatches)[batchBase++];
			batch->firstIndex = indexBase; batch->indexCount = 6u;
			batch->useVertexColor = qtrue; batch->visible = qtrue;
			EntityBatchMaterial( frontend, decal->shader, batch );
			batch->cullMode = RENDER_CULL_NONE; batch->depthWrite = qfalse;
			batch->surfaceDecal = qtrue;
			vertexBase += 4u; indexBase += 6u;
		}
		for ( uint32_t i = 0u; i < effectSnapshot.ribbonCount; ++i ) {
			const renderEffectRibbonCommand_t *ribbon = &effectRibbons[i];
			const uint32_t firstVertex = vertexBase;
			const uint32_t firstIndex = indexBase;
			for ( uint32_t pointIndex = 0u; pointIndex < ribbon->pointCount;
					++pointIndex ) {
				const ribbonPoint_t *point = &effectRibbonPoints[
					ribbon->firstPoint + pointIndex];
				const ribbonPoint_t *previous = &effectRibbonPoints[
					ribbon->firstPoint + ( pointIndex ? pointIndex - 1u : pointIndex )];
				const ribbonPoint_t *next = &effectRibbonPoints[
					ribbon->firstPoint + ( pointIndex + 1u < ribbon->pointCount
						? pointIndex + 1u : pointIndex )];
				float tangent[3], extrude[3], length;
				color4ub_t color;
				for ( uint32_t axis = 0u; axis < 3u; ++axis )
					tangent[axis] = next->pos[axis] - previous->pos[axis];
				if ( ribbon->flags & PRIM_FLAG_CUSTOM_NORMAL ) {
					memcpy( extrude, point->normal, sizeof( extrude ) );
				} else {
					const float *plane = ( ribbon->flags & PRIM_FLAG_VIEW_UP_PLANE )
						? view.viewAxis[2] : view.viewAxis[0];
					extrude[0] = tangent[1] * plane[2] - tangent[2] * plane[1];
					extrude[1] = tangent[2] * plane[0] - tangent[0] * plane[2];
					extrude[2] = tangent[0] * plane[1] - tangent[1] * plane[0];
				}
				length = sqrtf( extrude[0] * extrude[0] + extrude[1] * extrude[1]
					+ extrude[2] * extrude[2] );
				if ( length < 0.00001f ) return qfalse;
				for ( uint32_t channel = 0u; channel < 4u; ++channel )
					color.rgba[channel] = (byte)fminf( 255.0f,
						fmaxf( 0.0f, point->rgba[channel] * 255.0f ) );
				for ( uint32_t side = 0u; side < 2u; ++side ) {
					float world[3];
					const float sign = side ? -1.0f : 1.0f;
					for ( uint32_t axis = 0u; axis < 3u; ++axis )
						world[axis] = point->pos[axis] + extrude[axis] / length
							* point->width * sign;
					EntityVertex( &(*outVertices)[vertexBase++], &view, world,
						(float)pointIndex / (float)( ribbon->pointCount - 1u ),
						(float)side, color );
				}
			}
			for ( uint32_t segment = 0u; segment + 1u < ribbon->pointCount;
					++segment ) {
				const uint32_t base = firstVertex + segment * 2u;
				const uint32_t order[6] = { base, base + 1u, base + 2u,
					base + 2u, base + 1u, base + 3u };
				memcpy( *outIndices + indexBase, order, sizeof( order ) );
				indexBase += 6u;
			}
			ralMetalEntityBatch_t *batch = &(*outBatches)[batchBase++];
			batch->firstIndex = firstIndex;
			batch->indexCount = ( ribbon->pointCount - 1u ) * 6u;
			batch->useVertexColor = qtrue; batch->visible = qtrue;
			EntityBatchMaterial( frontend, ribbon->material, batch );
			batch->cullMode = RENDER_CULL_NONE; batch->depthWrite = qfalse;
			if ( ribbon->flags & PRIM_FLAG_ADDITIVE )
				batch->alphaMode = RENDER_ALPHA_ADDITIVE;
		}
		for ( uint32_t slot = 0u; present
				&& slot < RAL_METAL_EFFECT_BEAM_SLOT_COUNT; ++slot ) {
			const ralMetalEffectBeamSlot_t *beamSlot = &present->effectBeamSlots[slot];
			const beamDesc_t *beam;
			float start[3], end[3], axis[3], baseExtrude[3], length, extrudeLength;
			float age, fade;
			uint32_t firstIndex, copies;
			color4ub_t startColor, endColor;
			if ( !beamSlot->active ) continue;
			beam = &beamSlot->beam;
			ResolveEffectBeamEndpoint( frontend, beam, qtrue, start );
			ResolveEffectBeamEndpoint( frontend, beam, qfalse, end );
			for ( uint32_t component = 0u; component < 3u; ++component )
				axis[component] = end[component] - start[component];
			length = sqrtf( axis[0] * axis[0] + axis[1] * axis[1]
				+ axis[2] * axis[2] );
			if ( length < 0.00001f ) continue;
			for ( uint32_t component = 0u; component < 3u; ++component )
				axis[component] /= length;
			baseExtrude[0] = axis[1] * view.viewAxis[0][2]
				- axis[2] * view.viewAxis[0][1];
			baseExtrude[1] = axis[2] * view.viewAxis[0][0]
				- axis[0] * view.viewAxis[0][2];
			baseExtrude[2] = axis[0] * view.viewAxis[0][1]
				- axis[1] * view.viewAxis[0][0];
			extrudeLength = sqrtf( baseExtrude[0] * baseExtrude[0]
				+ baseExtrude[1] * baseExtrude[1]
				+ baseExtrude[2] * baseExtrude[2] );
			if ( extrudeLength < 0.00001f ) {
				baseExtrude[0] = axis[1] * view.viewAxis[2][2]
					- axis[2] * view.viewAxis[2][1];
				baseExtrude[1] = axis[2] * view.viewAxis[2][0]
					- axis[0] * view.viewAxis[2][2];
				baseExtrude[2] = axis[0] * view.viewAxis[2][1]
					- axis[1] * view.viewAxis[2][0];
				extrudeLength = sqrtf( baseExtrude[0] * baseExtrude[0]
					+ baseExtrude[1] * baseExtrude[1]
					+ baseExtrude[2] * baseExtrude[2] );
			}
			if ( extrudeLength < 0.00001f ) return qfalse;
			for ( uint32_t component = 0u; component < 3u; ++component )
				baseExtrude[component] /= extrudeLength;
			age = fmaxf( (float)view.timeMs * 0.001f - beamSlot->spawnTime, 0.0f );
			fade = EffectBeamFade( beamSlot, (float)view.timeMs * 0.001f );
			for ( uint32_t channel = 0u; channel < 4u; ++channel ) {
				const float startValue = beam->startColor[channel]
					* ( channel == 3u ? fade : 1.0f );
				const float endValue = beam->endColor[channel]
					* ( channel == 3u ? fade : 1.0f );
				startColor.rgba[channel] = (byte)fminf( 255.0f,
					fmaxf( 0.0f, startValue * 255.0f ) );
				endColor.rgba[channel] = (byte)fminf( 255.0f,
					fmaxf( 0.0f, endValue * 255.0f ) );
			}
			firstIndex = indexBase;
			copies = (uint32_t)beam->axialCopies;
			for ( uint32_t copy = 0u; copy < copies; ++copy ) {
				float extrude[3], corners[4][3];
				const float angle = (float)M_PI * (float)copy / (float)copies;
				const uint32_t firstVertex = vertexBase;
				RotateEffectBeamAxis( baseExtrude, axis, angle, extrude );
				for ( uint32_t component = 0u; component < 3u; ++component ) {
					corners[0][component] = start[component]
						+ extrude[component] * beam->startWidth;
					corners[1][component] = start[component]
						- extrude[component] * beam->startWidth;
					corners[2][component] = end[component]
						+ extrude[component] * beam->endWidth;
					corners[3][component] = end[component]
						- extrude[component] * beam->endWidth;
				}
				EntityVertex( &(*outVertices)[vertexBase++], &view, corners[0],
					age * beam->uvScroll[0], age * beam->uvScroll[1], startColor );
				EntityVertex( &(*outVertices)[vertexBase++], &view, corners[1],
					age * beam->uvScroll[0], 1.0f + age * beam->uvScroll[1], startColor );
				EntityVertex( &(*outVertices)[vertexBase++], &view, corners[2],
					1.0f + age * beam->uvScroll[0], age * beam->uvScroll[1], endColor );
				EntityVertex( &(*outVertices)[vertexBase++], &view, corners[3],
					1.0f + age * beam->uvScroll[0], 1.0f + age * beam->uvScroll[1], endColor );
				const uint32_t order[6] = { firstVertex, firstVertex + 1u,
					firstVertex + 2u, firstVertex + 2u, firstVertex + 1u,
					firstVertex + 3u };
				memcpy( *outIndices + indexBase, order, sizeof( order ) );
				indexBase += 6u;
			}
			ralMetalEntityBatch_t *batch = &(*outBatches)[batchBase++];
			batch->firstIndex = firstIndex; batch->indexCount = copies * 6u;
			batch->useVertexColor = qtrue; batch->visible = qtrue;
			EntityBatchMaterial( frontend, beam->shader, batch );
			batch->cullMode = RENDER_CULL_NONE; batch->depthWrite = qfalse;
			if ( beam->flags & PRIM_FLAG_ADDITIVE )
				batch->alphaMode = RENDER_ALPHA_ADDITIVE;
		}
	}
	*outVertexCount = vertexCount; *outIndexCount = indexCount; *outBatchCount = batchCount;
	return qtrue;
}

static qboolean DrawEntityBatches( id<MTLRenderCommandEncoder> encoder,
		ralMetalPresent_t *present, const renderSubmissionState_t *frontend,
		id<MTLBuffer> entityVertices, id<MTLBuffer> entityIndices,
		const ralMetalEntityBatch_t *entityBatches, uint32_t entityBatchCount,
		const renderWorldSnapshot_t *sceneView,
		ralMetalPresentReceipt_t *receipt ) {
	MTLViewport viewport;
	MTLScissorRect scissor;
	const uint32_t viewportX = sceneView->viewportX > 0
		? (uint32_t)sceneView->viewportX : 0u;
	const uint32_t viewportY = sceneView->viewportY > 0
		? (uint32_t)sceneView->viewportY : 0u;
	const uint32_t viewportWidth = sceneView->viewportWidth
		&& viewportX < receipt->width
		? MIN( sceneView->viewportWidth, receipt->width - viewportX )
		: receipt->width;
	const uint32_t viewportHeight = sceneView->viewportHeight
		&& viewportY < receipt->height
		? MIN( sceneView->viewportHeight, receipt->height - viewportY )
		: receipt->height;
	viewport.originX = viewportX; viewport.originY = viewportY;
	viewport.width = viewportWidth; viewport.height = viewportHeight;
	viewport.znear = 0.0; viewport.zfar = 1.0;
	scissor.x = viewportX; scissor.y = viewportY;
	scissor.width = viewportWidth; scissor.height = viewportHeight;
	[encoder setViewport:viewport];
	[encoder setScissorRect:scissor];
	[encoder setVertexBuffer:entityVertices offset:0u atIndex:0u];
	/* Match Vulkan's composition contract: depth-writing entity geometry first,
	 * surface decals second, and translucent effects last. A smoke sprite does
	 * not write depth, so submission order alone would otherwise let a later
	 * bullet mark incorrectly composite over smoke that is physically nearer. */
	for ( uint32_t drawIndex = 0u; drawIndex < entityBatchCount * 3u; ++drawIndex ) {
		const uint32_t phase = drawIndex / entityBatchCount;
		const uint32_t batchIndex = drawIndex % entityBatchCount;
		const ralMetalEntityBatch_t *batch = &entityBatches[batchIndex];
		const uint32_t batchPhase = batch->surfaceDecal ? 1u
			: ( batch->depthWrite ? 0u : 2u );
		id<MTLTexture> texture;
		id<MTLSamplerState> sampler;
		id<MTLTexture> stageTextures[RENDER_MATERIAL_MAX_STAGES];
		id<MTLSamplerState> stageSamplers[RENDER_MATERIAL_MAX_STAGES];
		qboolean textured, msdf;
		ralMetalWorldMaterialParams_t params;
		renderMaterialSnapshot_t materialSnapshot;
		if ( !batch->visible || batchPhase != phase ) continue;
		if ( sceneView->rdflags & RDF_NOWORLDMODEL )
			[encoder setCullMode:MTLCullModeNone];
		else if ( batch->cullMode == RENDER_CULL_NONE )
			[encoder setCullMode:MTLCullModeNone];
		else if ( batch->cullMode == RENDER_CULL_FRONT )
			[encoder setCullMode:MTLCullModeFront];
		else
			[encoder setCullMode:MTLCullModeBack];
		memset( &params, 0, sizeof( params ) );
		params.skyScaleScroll[0] = params.skyScaleScroll[1] = 1.0f;
		memset( &materialSnapshot, 0, sizeof( materialSnapshot ) );
		if ( RenderSubmission_MaterialSnapshot( frontend, batch->material,
				&materialSnapshot ) ) {
			memcpy( params.skyScaleScroll, materialSnapshot.skyScaleScroll,
				sizeof( params.skyScaleScroll ) );
			params.stageProgram[0] = materialSnapshot.stageCount;
		}
		/* bit 0: lightmap bound, bit 1: authored/entity vertex colour. */
		params.hasLightmap = batch->useVertexColor ? 2u : 0u;
		/* First-person weapons compress only their depth range. */
		viewport.zfar = batch->depthHack ? 0.3 : 1.0;
		[encoder setViewport:viewport];
		params.alphaMode = (uint32_t)batch->alphaMode;
		params.alphaCutoff = batch->alphaCutoff;
		/* bit 0: local SH is authoritative; bit 1: RF_MINLIGHT floor. */
		params.hasLocalIrradiance = ( batch->hasLocalIrradiance ? 1u : 0u )
			| ( batch->minLight ? 2u : 0u );
		for ( uint32_t coefficient = 0u; coefficient < 4u; ++coefficient )
			memcpy( params.localSh[coefficient], batch->localSh[coefficient],
				3u * sizeof( float ) );
		if ( !UiMaterial( present, frontend, batch->material, &texture,
				&sampler, &textured, &msdf ) || msdf ) return qfalse;
		for ( uint32_t stageIndex = 0u;
				stageIndex < RENDER_MATERIAL_MAX_STAGES; ++stageIndex ) {
			const renderMaterialStageSnapshot_t *stage =
				&materialSnapshot.stages[stageIndex];
			ralMetalMaterialStageParams_t *stageParams =
				&params.stages[stageIndex];
			qboolean stageTextured = qfalse, stageMsdf = qfalse;
			stageTextures[stageIndex] = present->uiFallbackTexture;
			stageSamplers[stageIndex] = present->uiClampSampler;
			if ( stageIndex >= materialSnapshot.stageCount ) continue;
			stageParams->meta[0] = stage->sourceBlend;
			stageParams->meta[1] = stage->destinationBlend;
			stageParams->meta[2] = stage->imageSource | ( stage->tcGen << 8u );
			stageParams->meta[3] = stage->alphaTest;
			stageParams->params[0] = stage->alphaCutoff;
			stageParams->params[1] = stage->rotateDegrees;
			stageParams->params[2] = stage->hasTurbulence ? 1.0f : 0.0f;
			stageParams->params[3] = stage->hasStretch ? 1.0f : 0.0f;
			memcpy( stageParams->scaleScroll, stage->scaleScroll,
				sizeof( stageParams->scaleScroll ) );
			memcpy( stageParams->turbulence, stage->turbulence,
				sizeof( stageParams->turbulence ) );
			memcpy( stageParams->stretch, stage->stretch,
				sizeof( stageParams->stretch ) );
			/* Entity materials have no BSP lightmap operand. A malformed entity
			 * script that requests one receives neutral white, matching Q3's
			 * vertex-lit fallback rather than sampling unrelated world data. */
			if ( stage->imageSource == 0u ) {
				if ( !UiMaterial( present, frontend, stage->material,
						&stageTextures[stageIndex], &stageSamplers[stageIndex],
						&stageTextured, &stageMsdf ) || stageMsdf ) return qfalse;
			}
		}
		[encoder setRenderPipelineState:batch->alphaMode == RENDER_ALPHA_ALPHA_ADDITIVE
			? present->worldAlphaAdditivePipeline
			: ( batch->alphaMode == RENDER_ALPHA_ADDITIVE
			? present->worldAdditivePipeline
			: ( batch->alphaMode == RENDER_ALPHA_BLEND
				? present->worldBlendPipeline : present->worldPipeline ) )];
		[encoder setDepthStencilState:batch->depthWrite
			? present->worldDepthState : present->worldDepthReadState];
		[encoder setFragmentTexture:texture atIndex:0u];
		[encoder setFragmentTexture:present->uiFallbackTexture atIndex:1u];
		for ( uint32_t slot = 2u; slot <= 4u; ++slot ) {
			[encoder setFragmentTexture:present->worldLightingFallbackTexture
				atIndex:slot];
			[encoder setFragmentSamplerState:present->uiClampSampler atIndex:slot];
		}
		[encoder setFragmentTexture:present->uiFallbackTexture atIndex:5u];
		[encoder setFragmentSamplerState:sampler atIndex:0u];
		[encoder setFragmentSamplerState:present->uiClampSampler atIndex:1u];
		[encoder setFragmentSamplerState:present->uiClampSampler atIndex:5u];
		for ( uint32_t stageIndex = 0u;
				stageIndex < RENDER_MATERIAL_MAX_STAGES; ++stageIndex ) {
			[encoder setFragmentTexture:stageTextures[stageIndex]
				atIndex:6u + stageIndex];
			[encoder setFragmentSamplerState:stageSamplers[stageIndex]
				atIndex:6u + stageIndex];
		}
		[encoder setFragmentBytes:&params length:sizeof( params ) atIndex:0u];
		if ( batch->hasLocalIrradiance ) receipt->localIrradianceDrawCount++;
		[encoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle
			indexCount:batch->indexCount indexType:MTLIndexTypeUInt32
			indexBuffer:entityIndices
			indexBufferOffset:(NSUInteger)batch->firstIndex * sizeof( uint32_t )];
	}
	return qtrue;
}

static qboolean EnsureWorldPipeline( ralMetalPresent_t *present,
		MTLPixelFormat colorFormat ) {
	id<MTLDevice> device;
	id<MTLFunction> vertexFunction = nil, fragmentFunction = nil;
	NSError *error = nil;
	if ( !present || colorFormat == MTLPixelFormatInvalid ) return qfalse;
	if ( present->worldPipeline && present->worldBlendPipeline
			&& present->worldAlphaAdditivePipeline
			&& present->worldAdditivePipeline
			&& present->worldDepthState && present->worldDepthReadState
			&& present->worldPixelFormat == colorFormat ) return qtrue;
	[present->worldPipeline release]; present->worldPipeline = nil;
	[present->worldBlendPipeline release]; present->worldBlendPipeline = nil;
	[present->worldAlphaAdditivePipeline release];
	present->worldAlphaAdditivePipeline = nil;
	[present->worldAdditivePipeline release]; present->worldAdditivePipeline = nil;
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
		vertices.attributes[4].format = MTLVertexFormatFloat4;
		vertices.attributes[4].offset = 48u; vertices.attributes[4].bufferIndex = 0u;
		vertices.layouts[0].stride = sizeof( ralMetalWorldVertex_t );
		MTLRenderPipelineDescriptor *pipeline =
			[[[MTLRenderPipelineDescriptor alloc] init] autorelease];
		pipeline.vertexFunction = vertexFunction; pipeline.fragmentFunction = fragmentFunction;
		pipeline.vertexDescriptor = vertices;
		pipeline.colorAttachments[0].pixelFormat = colorFormat;
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
			if ( present->worldBlendPipeline && !error ) {
				pipeline.colorAttachments[0].sourceRGBBlendFactor =
					MTLBlendFactorSourceAlpha;
				pipeline.colorAttachments[0].destinationRGBBlendFactor = MTLBlendFactorOne;
				pipeline.colorAttachments[0].sourceAlphaBlendFactor = MTLBlendFactorOne;
				pipeline.colorAttachments[0].destinationAlphaBlendFactor = MTLBlendFactorOne;
				present->worldAlphaAdditivePipeline = [device
					newRenderPipelineStateWithDescriptor:pipeline error:&error];
			}
			if ( present->worldAlphaAdditivePipeline && !error ) {
				pipeline.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorOne;
				pipeline.colorAttachments[0].destinationRGBBlendFactor = MTLBlendFactorOne;
				pipeline.colorAttachments[0].sourceAlphaBlendFactor = MTLBlendFactorOne;
				pipeline.colorAttachments[0].destinationAlphaBlendFactor = MTLBlendFactorOne;
				present->worldAdditivePipeline = [device
					newRenderPipelineStateWithDescriptor:pipeline error:&error];
			}
		}
		MTLDepthStencilDescriptor *depth =
			[[[MTLDepthStencilDescriptor alloc] init] autorelease];
		/* Q3/Vulkan use LEQUAL for the world pass.  Exact-depth translucent
		 * sprites (notably map-light flare layers) must survive after the opaque
		 * world has populated depth; LESS silently removed them on Metal. */
		depth.depthCompareFunction = MTLCompareFunctionLessEqual;
		depth.depthWriteEnabled = YES;
		present->worldDepthState = [device newDepthStencilStateWithDescriptor:depth];
		depth.depthWriteEnabled = NO;
		present->worldDepthReadState = [device newDepthStencilStateWithDescriptor:depth];
	}
	[vertexFunction release]; vertexFunction = nil;
	[fragmentFunction release]; fragmentFunction = nil;
	if ( !present->worldPipeline || !present->worldBlendPipeline
			|| !present->worldAlphaAdditivePipeline
			|| !present->worldAdditivePipeline
			|| !present->worldDepthState || !present->worldDepthReadState
			|| error ) goto fail;
	present->worldPixelFormat = colorFormat;
	return qtrue;
fail:
	[vertexFunction release]; [fragmentFunction release];
	[present->worldPipeline release]; present->worldPipeline = nil;
	[present->worldBlendPipeline release]; present->worldBlendPipeline = nil;
	[present->worldAlphaAdditivePipeline release];
	present->worldAlphaAdditivePipeline = nil;
	[present->worldAdditivePipeline release]; present->worldAdditivePipeline = nil;
	[present->worldDepthState release]; present->worldDepthState = nil;
	[present->worldDepthReadState release]; present->worldDepthReadState = nil;
	return qfalse;
}

qboolean RalMetal_PresentPlanAtmosphere( const ralMetalPresent_t *present,
		const renderSubmissionState_t *frontend, uint64_t frameGeneration,
		uint32_t width, uint32_t height, ralAtmospherePlanReceipt_t *outReceipt ) {
	renderAtmosphereSnapshot_t snapshot;
	renderAtmosphereMediaSnapshot_t media;
	ralAtmospherePlanRequest_t request;
	const atmosphereEmitter_t *emitters;
	const atmosphereMediaVolume_t *volumes;
	if ( !present || !frontend || !frameGeneration || !width || !height
			|| !outReceipt
			|| !RenderSubmission_AtmosphereSnapshot( frontend, &snapshot, &emitters )
			|| !RenderSubmission_AtmosphereMediaSnapshot( frontend, &media,
				&volumes ) ) return qfalse;
	(void)emitters; (void)volumes;
	memset( &request, 0, sizeof( request ) );
	request.schemaVersion = RAL_ATMOSPHERE_PLAN_SCHEMA_VERSION;
	request.backendType = RAL_BACKEND_METAL;
	request.frameGeneration = frameGeneration;
	request.width = width; request.height = height;
	request.requestedTier = snapshot.active
		? (ralAtmosphereTier_t)snapshot.state.qualityTier
		: RAL_ATMOSPHERE_TIER_OFF;
	request.localVolumeCount = media.count;
	request.lightCount = RenderSubmission_AtmosphereLightCount( frontend );
	if ( request.lightCount > RAL_ATMOSPHERE_MAX_LIGHTS )
		request.lightCount = RAL_ATMOSPHERE_MAX_LIGHTS;
	request.shadowedLightCount = RenderSubmission_AtmosphereShadowedLightCount( frontend );
	if ( request.shadowedLightCount > RAL_ATMOSPHERE_MAX_SHADOWED_LIGHTS )
		request.shadowedLightCount = RAL_ATMOSPHERE_MAX_SHADOWED_LIGHTS;
	request.maxFroxelCount = RAL_METAL_ATMOSPHERE_FROXEL_CAPACITY;
	request.maxLocalVolumes = RAL_ATMOSPHERE_MAX_VOLUMES;
	request.maxLights = RAL_ATMOSPHERE_MAX_LIGHTS;
	request.maxShadowedLights = RAL_ATMOSPHERE_MAX_SHADOWED_LIGHTS;
	request.mediaActive = snapshot.active && ( snapshot.state.mediaDensity > 0.0f
		|| snapshot.state.visibility > 0.0f || media.count > 0u ) ? qtrue : qfalse;
	request.skyLightingActive = snapshot.active;
	request.cloudsRequested = snapshot.active && snapshot.state.cloudCover > 0.0f
		? qtrue : qfalse;
	request.historyValid = present->atmosphereHistoryValid;
	request.capabilities.analyticComposite = qtrue;
	request.capabilities.compute = qtrue;
	request.capabilities.storageBuffers = qtrue;
	request.capabilities.temporalHistory = qtrue;
	request.capabilities.fullClouds = qtrue;
	return Ral_AtmospherePlan( &request, outReceipt );
}

static qboolean EnsureAtmospherePipeline( ralMetalPresent_t *present ) {
	id<MTLDevice> device;
	id<MTLFunction> function = nil;
	NSError *error = nil;
	if ( !present ) return qfalse;
	if ( present->atmospherePipeline && present->atmosphereArena
			&& present->atmosphereVolumes ) return qtrue;
	device = RalMetal_CoreNativeDevice( present->core );
	if ( !device ) return qfalse;
	if ( !present->uiLibrary ) {
		dispatch_data_t data = dispatch_data_create( ral_metal_ui_metallib,
			ral_metal_ui_metallib_size,
			dispatch_get_global_queue( QOS_CLASS_DEFAULT, 0 ), ^{} );
		if ( !data ) return qfalse;
		present->uiLibrary = [device newLibraryWithData:data error:&error];
		if ( !present->uiLibrary || error ) return qfalse;
	}
	function = [present->uiLibrary newFunctionWithName:@"wired_atmosphere_compute"];
	if ( !function ) return qfalse;
	present->atmospherePipeline = [device
		newComputePipelineStateWithFunction:function error:&error];
	[function release];
	if ( !present->atmospherePipeline || error ) return qfalse;
	present->atmosphereArena = [device newBufferWithLength:
		(NSUInteger)RAL_METAL_ATMOSPHERE_FROXEL_CAPACITY
			* RAL_METAL_ATMOSPHERE_SECTION_COUNT * sizeof( float ) * 4u
		options:MTLResourceStorageModePrivate];
	present->atmosphereVolumes = [device newBufferWithLength:
		(NSUInteger)RAL_ATMOSPHERE_MAX_VOLUMES
			* sizeof( ralMetalAtmosphereVolume_t )
		options:MTLResourceStorageModeShared];
	return present->atmosphereArena && present->atmosphereVolumes ? qtrue : qfalse;
}

static qboolean EnsureAtmosphereFallback( ralMetalPresent_t *present ) {
	if ( !present ) return qfalse;
	if ( present->atmosphereFallback ) return qtrue;
	present->atmosphereFallback = [RalMetal_CoreNativeDevice( present->core )
		newBufferWithLength:sizeof( float ) * 4u
		options:MTLResourceStorageModeShared];
	return present->atmosphereFallback ? qtrue : qfalse;
}

static qboolean EncodeAtmosphere( ralMetalPresent_t *present,
		id<MTLCommandBuffer> command, const renderSubmissionState_t *frontend,
		const ralAtmospherePlanReceipt_t *plan ) {
	renderAtmosphereSnapshot_t snapshot;
	renderAtmosphereMediaSnapshot_t media;
	const atmosphereEmitter_t *emitters;
	const atmosphereMediaVolume_t *volumes;
	ralMetalAtmosphereVolume_t *packed;
	id<MTLComputeCommandEncoder> encoder;
	float precipitation = 0.0f;
	if ( !present || !command || !frontend || !plan
			|| plan->selectedTier != RAL_ATMOSPHERE_TIER_FULL
			|| plan->froxelCount > RAL_METAL_ATMOSPHERE_FROXEL_CAPACITY
			|| !EnsureAtmospherePipeline( present )
			|| !RenderSubmission_AtmosphereSnapshot( frontend, &snapshot, &emitters )
			|| !RenderSubmission_AtmosphereMediaSnapshot( frontend, &media,
				&volumes ) ) return qfalse;
	(void)emitters; (void)media;
	packed = (ralMetalAtmosphereVolume_t *)[present->atmosphereVolumes contents];
	if ( !packed ) return qfalse;
	memset( packed, 0, sizeof( *packed ) * RAL_ATMOSPHERE_MAX_VOLUMES );
	for ( uint32_t i = 0u; i < plan->admittedVolumeCount; ++i ) {
		const atmosphereMediaVolume_t *source = &volumes[i];
		memcpy( packed[i].originRadius, source->origin, 3u * sizeof( float ) );
		packed[i].originRadius[3] = source->radius;
		memcpy( packed[i].extentShape, source->extent, 3u * sizeof( float ) );
		packed[i].extentShape[3] = (float)source->shape;
		memcpy( packed[i].albedoAnisotropy, source->albedo, 3u * sizeof( float ) );
		packed[i].albedoAnisotropy[3] = source->anisotropy;
		memcpy( packed[i].emissiveIntensity, source->emissive, 3u * sizeof( float ) );
		packed[i].emissiveIntensity[3] = source->emissionIntensity;
		packed[i].media[0] = source->extinction;
		packed[i].media[1] = source->heightFalloff;
		packed[i].media[2] = source->noiseScale;
		packed[i].media[3] = (float)source->flags;
	}
	for ( uint32_t i = 0u; i < 5u; ++i )
		precipitation += snapshot.state.precipitation[i];
	if ( precipitation > 1.0f ) precipitation = 1.0f;
	encoder = [command computeCommandEncoder];
	if ( !encoder ) return qfalse;
	[encoder setComputePipelineState:present->atmospherePipeline];
	[encoder setBuffer:present->atmosphereArena offset:0u atIndex:1u];
	[encoder setBuffer:present->atmosphereVolumes offset:0u atIndex:2u];
	uint32_t stages[4] = { 0u, 1u, 2u, 3u };
	uint32_t stageCount = plan->cloudsActive ? 4u : 3u;
	if ( !plan->cloudsActive ) stages[2] = 3u;
	for ( uint32_t pass = 0u; pass < stageCount; ++pass ) {
		ralMetalAtmosphereParams_t params;
		uint32_t historyPrevious = present->atmosphereHistoryParity ? 5u : 4u;
		uint32_t historyNext = present->atmosphereHistoryParity ? 4u : 5u;
		memset( &params, 0, sizeof( params ) );
		params.dimsStage[0] = plan->froxelWidth;
		params.dimsStage[1] = plan->froxelHeight;
		params.dimsStage[2] = plan->froxelDepth;
		params.dimsStage[3] = stages[pass];
		for ( uint32_t i = 0u; i < 4u; ++i )
			params.bases0[i] = i * RAL_METAL_ATMOSPHERE_FROXEL_CAPACITY;
		params.bases1[0] = historyPrevious * RAL_METAL_ATMOSPHERE_FROXEL_CAPACITY;
		params.bases1[1] = historyNext * RAL_METAL_ATMOSPHERE_FROXEL_CAPACITY;
		params.bases1[2] = 3u * RAL_METAL_ATMOSPHERE_FROXEL_CAPACITY;
		params.bases1[3] = plan->admittedVolumeCount;
		memcpy( params.boundsMin, snapshot.state.bounds, 3u * sizeof( float ) );
		memcpy( params.boundsMax, snapshot.state.bounds + 3u, 3u * sizeof( float ) );
		params.climate[0] = snapshot.state.mediaDensity;
		params.climate[1] = snapshot.state.mediaHeightFalloff;
		params.climate[2] = plan->cloudsActive ? snapshot.state.cloudCover : 0.0f;
		params.climate[3] = snapshot.state.cloudShadow;
		params.lighting[0] = ( snapshot.state.ambientColor[0]
			+ snapshot.state.ambientColor[1] + snapshot.state.ambientColor[2] ) / 3.0f;
		params.lighting[1] = snapshot.state.sunIntensity;
		params.lighting[2] = snapshot.state.lightning;
		params.lighting[3] = present->atmosphereHistoryValid
			&& plan->temporalReuseCount ? 0.88f : 0.0f;
		params.weather[0] = precipitation;
		params.weather[1] = snapshot.state.indoorExposure;
		params.weather[2] = snapshot.state.timelineSeconds;
		params.weather[3] = snapshot.state.visibility > 0.0f
			? snapshot.state.visibility : 65536.0f;
		[encoder setBytes:&params length:sizeof( params ) atIndex:0u];
		[encoder dispatchThreads:MTLSizeMake( plan->froxelWidth,
			plan->froxelHeight, plan->froxelDepth )
			threadsPerThreadgroup:MTLSizeMake( 4u, 4u, 4u )];
		if ( pass + 1u < stageCount )
			[encoder memoryBarrierWithScope:MTLBarrierScopeBuffers];
	}
	[encoder endEncoding];
	present->atmosphereHistoryValid = qtrue;
	present->atmosphereHistoryParity = present->atmosphereHistoryParity ? qfalse : qtrue;
	return qtrue;
}

static void MetalWeatherWeights( const atmosphereFrameState_t *state,
		float weights[5] ) {
	float total = 0.0f;
	memcpy( weights, state->precipitation, 5u * sizeof( *weights ) );
	for ( uint32_t i = 0u; i < 5u; ++i ) total += weights[i];
	if ( total <= 0.0f && state->type >= ATMOSPHERE_PRECIP_RAIN
			&& state->type <= ATMOSPHERE_PRECIP_DUST_ASH )
		weights[(uint32_t)state->type - 1u] = 1.0f;
}

static qboolean PlanWeather( const renderSubmissionState_t *frontend,
		const ralAtmospherePlanReceipt_t *atmosphere,
		ralAtmosphereWeatherReceipt_t *outReceipt ) {
	renderAtmosphereSnapshot_t snapshot;
	renderAtmosphereEffectWorkloadSnapshot_t effects;
	ralAtmosphereWeatherRequest_t request;
	const atmosphereEmitter_t *emitters;
	if ( !frontend || !atmosphere || !outReceipt
			|| !RenderSubmission_AtmosphereSnapshot( frontend, &snapshot,
				&emitters ) ) return qfalse;
	(void)emitters; memset( &request, 0, sizeof( request ) );
	request.schemaVersion = RAL_ATMOSPHERE_WEATHER_SCHEMA_VERSION;
	request.tier = atmosphere->selectedTier;
	request.maxParticles = RAL_METAL_WEATHER_PARTICLE_CAPACITY;
	request.enabled = snapshot.active && frontend->sceneRendered ? qtrue : qfalse;
	request.indoorExposure = snapshot.state.indoorExposure;
	if ( !RenderSubmission_AtmosphereEffectWorkloadSnapshot( frontend,
			request.maxParticles, &effects ) ) return qfalse;
	request.semanticEmitterCount = effects.admittedEmitterCount;
	request.semanticParticleCount = effects.admittedParticleCount;
	MetalWeatherWeights( &snapshot.state, request.precipitation );
	return Ral_AtmospherePlanWeather( &request, outReceipt );
}

static qboolean EnsureWeatherPipeline( ralMetalPresent_t *present,
		MTLPixelFormat colorFormat ) {
	id<MTLDevice> device;
	id<MTLFunction> compute = nil, vertex = nil, fragment = nil;
	NSError *error = nil;
	if ( !present ) return qfalse;
	if ( present->weatherComputePipeline && present->weatherRenderPipeline
			&& present->effectAdditivePipeline
			&& present->weatherFrame && present->weatherPools[0]
			&& present->weatherPools[1] && present->effectFrame
			&& present->effectPools[0] && present->effectPools[1]
			&& present->weatherPixelFormat == colorFormat )
		return qtrue;
	device = RalMetal_CoreNativeDevice( present->core );
	if ( !device ) return qfalse;
	if ( !present->uiLibrary ) {
		dispatch_data_t data = dispatch_data_create( ral_metal_ui_metallib,
			ral_metal_ui_metallib_size,
			dispatch_get_global_queue( QOS_CLASS_DEFAULT, 0 ), ^{} );
		if ( !data ) return qfalse;
		present->uiLibrary = [device newLibraryWithData:data error:&error];
		if ( !present->uiLibrary || error ) return qfalse;
	}
	compute = [present->uiLibrary newFunctionWithName:@"wired_weather_compute"];
	vertex = [present->uiLibrary newFunctionWithName:@"wired_weather_vertex"];
	fragment = [present->uiLibrary newFunctionWithName:@"wired_weather_fragment"];
	if ( !compute || !vertex || !fragment ) goto fail;
	[present->weatherComputePipeline release];
	present->weatherComputePipeline = [device
		newComputePipelineStateWithFunction:compute error:&error];
	if ( !present->weatherComputePipeline || error ) goto fail;
	{
		MTLRenderPipelineDescriptor *pipeline =
			[[MTLRenderPipelineDescriptor alloc] init];
		pipeline.vertexFunction = vertex; pipeline.fragmentFunction = fragment;
		pipeline.colorAttachments[0].pixelFormat = colorFormat;
		pipeline.colorAttachments[0].blendingEnabled = YES;
		pipeline.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorSourceAlpha;
		pipeline.colorAttachments[0].destinationRGBBlendFactor =
			MTLBlendFactorOneMinusSourceAlpha;
		pipeline.colorAttachments[0].sourceAlphaBlendFactor = MTLBlendFactorOne;
		pipeline.colorAttachments[0].destinationAlphaBlendFactor =
			MTLBlendFactorOneMinusSourceAlpha;
		pipeline.depthAttachmentPixelFormat = MTLPixelFormatDepth32Float;
		[present->weatherRenderPipeline release];
		present->weatherRenderPipeline = [device
			newRenderPipelineStateWithDescriptor:pipeline error:&error];
		if ( !present->weatherRenderPipeline || error ) {
			[pipeline release]; goto fail;
		}
		pipeline.colorAttachments[0].sourceRGBBlendFactor =
			MTLBlendFactorSourceAlpha;
		pipeline.colorAttachments[0].destinationRGBBlendFactor =
			MTLBlendFactorOne;
		[present->effectAdditivePipeline release];
		present->effectAdditivePipeline = [device
			newRenderPipelineStateWithDescriptor:pipeline error:&error];
		[pipeline release];
	}
	if ( !present->weatherRenderPipeline || !present->effectAdditivePipeline
			|| error ) goto fail;
	if ( !present->weatherFrame ) present->weatherFrame = [device
		newBufferWithLength:sizeof( ralMetalWeatherFrame_t )
		options:MTLResourceStorageModeShared];
	if ( !present->effectFrame ) present->effectFrame = [device
		newBufferWithLength:sizeof( ralMetalWeatherFrame_t )
		options:MTLResourceStorageModeShared];
	for ( uint32_t i = 0u; i < 2u; ++i ) if ( !present->weatherPools[i] ) {
		present->weatherPools[i] = [device newBufferWithLength:
			(NSUInteger)RAL_METAL_WEATHER_PARTICLE_CAPACITY * 32u
			options:MTLResourceStorageModeShared];
		if ( present->weatherPools[i] ) memset( [present->weatherPools[i] contents],
			0, (size_t)RAL_METAL_WEATHER_PARTICLE_CAPACITY * 32u );
	}
	for ( uint32_t i = 0u; i < 2u; ++i ) if ( !present->effectPools[i] ) {
		present->effectPools[i] = [device newBufferWithLength:
			(NSUInteger)RAL_METAL_WEATHER_PARTICLE_CAPACITY * 32u
			options:MTLResourceStorageModeShared];
		if ( present->effectPools[i] ) memset( [present->effectPools[i] contents],
			0, (size_t)RAL_METAL_WEATHER_PARTICLE_CAPACITY * 32u );
	}
	present->weatherPixelFormat = colorFormat;
	[compute release]; [vertex release]; [fragment release];
	return present->weatherFrame && present->weatherPools[0]
		&& present->weatherPools[1] && present->effectFrame
		&& present->effectPools[0] && present->effectPools[1] ? qtrue : qfalse;
fail:
	[compute release]; [vertex release]; [fragment release];
	return qfalse;
}

static qboolean EncodeWeather( ralMetalPresent_t *present,
		id<MTLCommandBuffer> command, const renderSubmissionState_t *frontend,
		const ralAtmosphereWeatherReceipt_t *weather ) {
	renderAtmosphereSnapshot_t snapshot;
	renderAtmosphereEffectWorkloadSnapshot_t effects;
	renderWorldSnapshot_t view;
	const atmosphereEmitter_t *emitters;
	ralMetalWeatherFrame_t frame;
	id<MTLComputeCommandEncoder> encoder;
	float weights[5], projectionX, projectionY;
	uint32_t readPool = present->weatherParity ? 1u : 0u;
	uint32_t writePool = present->weatherParity ? 0u : 1u;
	if ( !present || !command || !frontend || !weather || weather->zeroWork
			|| !RenderSubmission_AtmosphereSnapshot( frontend, &snapshot,
				&emitters )
			|| !RenderSubmission_ViewSnapshot( frontend, &view ) ) return qfalse;
	(void)emitters; memset( &frame, 0, sizeof( frame ) );
	projectionX = 1.0f / tanf( view.fovX * 0.00872664625997164788f );
	projectionY = 1.0f / tanf( view.fovY * 0.00872664625997164788f );
	for ( uint32_t axis = 0u; axis < 3u; ++axis ) {
		frame.mvp[axis * 4u] = -view.viewAxis[1][axis] * projectionX;
		frame.mvp[axis * 4u + 1u] = view.viewAxis[2][axis] * projectionY;
		frame.mvp[axis * 4u + 2u] = view.viewAxis[0][axis];
		frame.mvp[axis * 4u + 3u] = view.viewAxis[0][axis];
		frame.mvp[12] += view.viewOrigin[axis] * view.viewAxis[1][axis]
			* projectionX;
		frame.mvp[13] -= view.viewOrigin[axis] * view.viewAxis[2][axis]
			* projectionY;
		frame.mvp[14] -= view.viewOrigin[axis] * view.viewAxis[0][axis];
		frame.mvp[15] -= view.viewOrigin[axis] * view.viewAxis[0][axis];
	}
	frame.mvp[14] -= 4.0f;
	memcpy( frame.viewLeft, view.viewAxis[1], 3u * sizeof( float ) );
	memcpy( frame.viewUp, view.viewAxis[2], 3u * sizeof( float ) );
	memcpy( frame.eyeWorld, view.viewOrigin, 3u * sizeof( float ) );
	frame.viewLeft[3] = present->displayVisibility.exposureScale;
	frame.viewUp[3] = present->displayVisibility.shadowExponent;
	frame.eyeWorld[3] = present->displayVisibility.shadowPivot;
	frame.dtTimePoolRead[0] = present->weatherTimeline > 0.0f
		? snapshot.state.timelineSeconds - present->weatherTimeline : 1.0f / 60.0f;
	if ( frame.dtTimePoolRead[0] < 0.0f || frame.dtTimePoolRead[0] > 0.1f )
		frame.dtTimePoolRead[0] = 1.0f / 60.0f;
	frame.dtTimePoolRead[1] = snapshot.state.timelineSeconds;
	frame.dtTimePoolRead[2] = (float)weather->activeParticleCount;
	frame.dtTimePoolRead[3] = (float)readPool;
	memcpy( frame.boundsMin, snapshot.state.bounds, 3u * sizeof( float ) );
	memcpy( frame.boundsMax, snapshot.state.bounds + 3u, 3u * sizeof( float ) );
	memcpy( frame.windGust, snapshot.state.wind, 3u * sizeof( float ) );
	frame.windGust[3] = snapshot.state.gustStrength;
	MetalWeatherWeights( &snapshot.state, weights );
	memcpy( frame.precipitation, weights, 4u * sizeof( float ) );
	frame.weather[0] = snapshot.state.distance > 0.0f
		? snapshot.state.distance : 1024.0f;
	frame.weather[1] = weights[4]; frame.weather[2] = (float)snapshot.state.seed;
	frame.weather[3] = 1.0f;
	memcpy( frame.sun, snapshot.state.sunDirection, 3u * sizeof( float ) );
	frame.sun[3] = snapshot.state.sunIntensity;
	memcpy( frame.moon, snapshot.state.moonDirection, 3u * sizeof( float ) );
	frame.moon[3] = snapshot.state.moonIntensity;
	memcpy( frame.ambientCloud, snapshot.state.ambientColor,
		3u * sizeof( float ) ); frame.ambientCloud[3] = snapshot.state.cloudCover;
	frame.cloudMedia[0] = snapshot.state.cloudShadow;
	frame.cloudMedia[1] = snapshot.state.lightning;
	if ( !RenderSubmission_AtmosphereEffectGpuPayload( frontend,
			weather->semanticParticleCount,
			weather->precipitationParticleCount, frame.effectWorkloads,
			&effects )
			|| effects.admittedEmitterCount != weather->semanticEmitterCount
			|| effects.admittedParticleCount != weather->semanticParticleCount )
		return qfalse;
	frame.effectMeta[0] = (float)weather->precipitationParticleCount;
	frame.effectMeta[1] = (float)weather->semanticEmitterCount;
	memcpy( [present->weatherFrame contents], &frame, sizeof( frame ) );
	encoder = [command computeCommandEncoder];
	if ( !encoder ) return qfalse;
	[encoder setComputePipelineState:present->weatherComputePipeline];
	[encoder setBuffer:present->weatherFrame offset:0u atIndex:0u];
	[encoder setBuffer:present->weatherPools[readPool] offset:0u atIndex:1u];
	[encoder setBuffer:present->weatherPools[writePool] offset:0u atIndex:2u];
	[encoder dispatchThreads:MTLSizeMake( weather->activeParticleCount, 1u, 1u )
		threadsPerThreadgroup:MTLSizeMake( 64u, 1u, 1u )];
	[encoder endEncoding];
	present->weatherParity = present->weatherParity ? qfalse : qtrue;
	present->weatherTimeline = snapshot.state.timelineSeconds;
	return qtrue;
}

static qboolean GenericEffectsPending( const ralMetalPresent_t *present,
		const renderSubmissionState_t *frontend ) {
	renderEffectPrimitiveSnapshot_t snapshot;
	const spriteDesc_t *sprites;
	const emitterDesc_t *emitters;
	const decalDesc_t *decals;
	const renderEffectRibbonCommand_t *ribbons;
	const ribbonPoint_t *points;
	const beamDesc_t *beams;
	if ( present ) for ( uint32_t slot = 0u;
			slot < RAL_METAL_EFFECT_SLOT_COUNT; ++slot )
		if ( present->effectSlots[slot].active ) return qtrue;
	if ( !frontend || !RenderSubmission_EffectPrimitiveSnapshots( frontend,
			&snapshot, &sprites, &emitters, &decals, &ribbons, &points, &beams ) )
		return qfalse;
	(void)beams;
	return snapshot.emitterCount ? qtrue : qfalse;
}

static qboolean EncodeGenericEffects( ralMetalPresent_t *present,
		id<MTLCommandBuffer> command, const renderSubmissionState_t *frontend,
		const renderWorldSnapshot_t *view, uint32_t *outEmitterCount,
		uint32_t *outParticleCount, uint32_t *outDroppedCount ) {
	renderEffectPrimitiveSnapshot_t snapshot;
	const spriteDesc_t *sprites;
	const emitterDesc_t *emitters;
	const decalDesc_t *decals;
	const renderEffectRibbonCommand_t *ribbons;
	const ribbonPoint_t *points;
	const beamDesc_t *beams;
	ralMetalWeatherFrame_t frame;
	float now, projectionX, projectionY;
	uint32_t maxEnd = 0u, activeParticleCount = 0u, admitted = 0u, dropped = 0u;
	uint32_t readPool, writePool;
	id<MTLComputeCommandEncoder> encoder;
	if ( !present || !command || !frontend || !view || !outEmitterCount
			|| !outParticleCount || !outDroppedCount
			|| !RenderSubmission_EffectPrimitiveSnapshots( frontend, &snapshot,
				&sprites, &emitters, &decals, &ribbons, &points, &beams ) ) return qfalse;
	(void)sprites; (void)decals; (void)ribbons; (void)points; (void)beams;
	now = (float)view->timeMs * 0.001f;
	if ( !isfinite( now ) || now < 0.0f ) return qfalse;
	if ( present->effectTimeline > now ) {
		memset( present->effectSlots, 0, sizeof( present->effectSlots ) );
		for ( uint32_t pool = 0u; pool < 2u; ++pool )
			memset( [present->effectPools[pool] contents], 0,
				(size_t)RAL_METAL_WEATHER_PARTICLE_CAPACITY * 32u );
	}
	for ( uint32_t slot = 0u; slot < RAL_METAL_EFFECT_SLOT_COUNT; ++slot )
		if ( present->effectSlots[slot].active
				&& now >= present->effectSlots[slot].expiresAt )
			present->effectSlots[slot].active = qfalse;
	for ( uint32_t emitterIndex = 0u; emitterIndex < snapshot.emitterCount;
			emitterIndex++ ) {
		particleClass_t particleClass;
		uint32_t slot;
		for ( slot = 0u; slot < RAL_METAL_EFFECT_SLOT_COUNT; ++slot )
			if ( !present->effectSlots[slot].active ) break;
		if ( slot == RAL_METAL_EFFECT_SLOT_COUNT
				|| !RenderSubmission_GetParticleClass( frontend,
					emitters[emitterIndex].cls, &particleClass ) ) {
			dropped++; continue;
		}
		present->effectSlots[slot].active = qtrue;
		present->effectSlots[slot].emitter = emitters[emitterIndex];
		present->effectSlots[slot].particleClass = particleClass;
		present->effectSlots[slot].expiresAt = now + fmaxf( 0.05f,
			particleClass.lifetimeMean + particleClass.lifetimeJitter );
		for ( uint32_t pool = 0u; pool < 2u; ++pool )
			memset( (byte *)[present->effectPools[pool] contents]
					+ (size_t)slot * RAL_METAL_EFFECT_PARTICLES_PER_SLOT * 32u,
				0, (size_t)RAL_METAL_EFFECT_PARTICLES_PER_SLOT * 32u );
		admitted++;
	}
	memset( &frame, 0, sizeof( frame ) );
	projectionX = 1.0f / tanf( view->fovX * 0.00872664625997164788f );
	projectionY = 1.0f / tanf( view->fovY * 0.00872664625997164788f );
	for ( uint32_t axis = 0u; axis < 3u; ++axis ) {
		frame.mvp[axis * 4u] = -view->viewAxis[1][axis] * projectionX;
		frame.mvp[axis * 4u + 1u] = view->viewAxis[2][axis] * projectionY;
		frame.mvp[axis * 4u + 2u] = view->viewAxis[0][axis];
		frame.mvp[axis * 4u + 3u] = view->viewAxis[0][axis];
		frame.mvp[12] += view->viewOrigin[axis] * view->viewAxis[1][axis]
			* projectionX;
		frame.mvp[13] -= view->viewOrigin[axis] * view->viewAxis[2][axis]
			* projectionY;
		frame.mvp[14] -= view->viewOrigin[axis] * view->viewAxis[0][axis];
		frame.mvp[15] -= view->viewOrigin[axis] * view->viewAxis[0][axis];
	}
	frame.mvp[14] -= 4.0f;
	memcpy( frame.viewLeft, view->viewAxis[1], 3u * sizeof( float ) );
	memcpy( frame.viewUp, view->viewAxis[2], 3u * sizeof( float ) );
	memcpy( frame.eyeWorld, view->viewOrigin, 3u * sizeof( float ) );
	frame.viewLeft[3] = present->displayVisibility.exposureScale;
	frame.viewUp[3] = present->displayVisibility.shadowExponent;
	frame.eyeWorld[3] = present->displayVisibility.shadowPivot;
	frame.dtTimePoolRead[0] = present->effectTimeline > 0.0f
		? now - present->effectTimeline : 1.0f / 60.0f;
	if ( frame.dtTimePoolRead[0] < 0.0f || frame.dtTimePoolRead[0] > 0.1f )
		frame.dtTimePoolRead[0] = 1.0f / 60.0f;
	frame.dtTimePoolRead[1] = now;
	frame.weather[2] = fmodf( (float)view->timeMs, 16777215.0f );
	frame.ambientCloud[0] = frame.ambientCloud[1] = frame.ambientCloud[2] = 1.0f;
	frame.effectMeta[1] = (float)RAL_METAL_EFFECT_SLOT_COUNT;
	for ( uint32_t slot = 0u; slot < RAL_METAL_EFFECT_SLOT_COUNT; ++slot ) {
		const ralMetalEffectSlot_t *active = &present->effectSlots[slot];
		renderAtmosphereEffectGpuWorkload_t *work = &frame.effectWorkloads[slot];
		uint32_t count, first, end;
		float origin[3], radius;
		if ( !active->active ) continue;
		count = (uint32_t)active->emitter.count;
		if ( count > RAL_METAL_EFFECT_PARTICLES_PER_SLOT ) {
			dropped += count - RAL_METAL_EFFECT_PARTICLES_PER_SLOT;
			count = RAL_METAL_EFFECT_PARTICLES_PER_SLOT;
		}
		first = slot * RAL_METAL_EFFECT_PARTICLES_PER_SLOT;
		end = first + count;
		if ( end > maxEnd ) maxEnd = end;
		activeParticleCount += count;
		memcpy( origin, active->emitter.origin, sizeof( origin ) );
		radius = active->particleClass.scatterMagnitude;
		if ( active->particleClass.emitMode == EMIT_PATH ) {
			float distanceSquared = 0.0f;
			for ( uint32_t axis = 0u; axis < 3u; ++axis ) {
				const float delta = active->emitter.end[axis]
					- active->emitter.origin[axis];
				origin[axis] += delta * 0.5f; distanceSquared += delta * delta;
			}
			radius += sqrtf( distanceSquared ) * 0.5f;
		}
		memcpy( work->vectors[0], origin, sizeof( origin ) );
		work->vectors[0][3] = radius;
		memcpy( work->vectors[1], active->emitter.axis, 3u * sizeof( float ) );
		work->vectors[1][3] = 1.0f;
		for ( uint32_t channel = 0u; channel < 4u; ++channel )
			work->vectors[2][channel] = active->particleClass.colorPalette[0][channel]
				* active->emitter.colorTint[channel];
		work->vectors[3][0] = active->particleClass.axialSpeed;
		work->vectors[3][1] = active->particleClass.cubeJitter;
		work->vectors[3][2] = active->particleClass.gravityScale;
		work->vectors[3][3] = active->particleClass.drag;
		work->vectors[4][0] = (float)first; work->vectors[4][1] = (float)end;
		work->vectors[4][2] = active->particleClass.sizeStart;
		work->vectors[4][3] = active->particleClass.sizeEnd;
		work->vectors[5][0] = active->particleClass.lifetimeMean;
		work->vectors[5][1] = active->particleClass.lifetimeJitter;
		work->vectors[5][2] = active->particleClass.scatterMagnitude;
		work->vectors[5][3] = (float)( ( view->timeMs + (int)slot * 7919 )
			& 0x00ffffff );
	}
	frame.dtTimePoolRead[2] = (float)maxEnd;
	*outEmitterCount = admitted;
	*outParticleCount = activeParticleCount;
	*outDroppedCount = dropped;
	present->effectTimeline = now;
	if ( !maxEnd ) return qtrue;
	memcpy( [present->effectFrame contents], &frame, sizeof( frame ) );
	readPool = present->effectParity ? 1u : 0u;
	writePool = present->effectParity ? 0u : 1u;
	encoder = [command computeCommandEncoder];
	if ( !encoder ) return qfalse;
	[encoder setComputePipelineState:present->weatherComputePipeline];
	[encoder setBuffer:present->effectFrame offset:0u atIndex:0u];
	[encoder setBuffer:present->effectPools[readPool] offset:0u atIndex:1u];
	[encoder setBuffer:present->effectPools[writePool] offset:0u atIndex:2u];
	[encoder dispatchThreads:MTLSizeMake( maxEnd, 1u, 1u )
		threadsPerThreadgroup:MTLSizeMake( 64u, 1u, 1u )];
	[encoder endEncoding];
	present->effectParity = present->effectParity ? qfalse : qtrue;
	return qtrue;
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

static void InitializeToneMapDefaults( ralMetalPresent_t *present ) {
	present->toneMap.mode = 3u;
	present->toneMap.exposure = 1.0f;
	present->toneMap.lottesContrast = 1.6f;
	present->toneMap.lottesShoulder = 0.977f;
	present->toneMap.lottesMidIn = 0.18f;
	present->toneMap.lottesMidOut = 0.267f;
	present->toneMap.lottesHdrMax = 8.0f;
	present->toneMap.displayVisibility[0] = 1.0f;
	present->toneMap.displayVisibility[1] = 1.0f;
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
	InitializeToneMapDefaults( candidate );
	if ( !Ral_DisplayVisibilityPlanBuild( RAL_DISPLAY_BRIGHTNESS_AUTHORED,
			&candidate->displayVisibility ) ) {
		[layer release]; free( candidate ); return qfalse;
	}
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
	InitializeToneMapDefaults( candidate );
	if ( !Ral_DisplayVisibilityPlanBuild( RAL_DISPLAY_BRIGHTNESS_AUTHORED,
			&candidate->displayVisibility ) ) { free( candidate ); return qfalse; }
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
	present->atmosphereHistoryValid = qfalse;
	present->atmosphereHistoryParity = qfalse;
	present->weatherParity = qfalse;
	present->weatherTimeline = 0.0f;
	present->effectParity = qfalse;
	present->effectTimeline = 0.0f;
	memset( present->effectSlots, 0, sizeof( present->effectSlots ) );
	present->effectDecalTimeline = 0.0f;
	memset( present->effectDecalSlots, 0, sizeof( present->effectDecalSlots ) );
	for ( uint32_t i = 0u; i < 2u; ++i ) if ( present->weatherPools[i] )
		memset( [present->weatherPools[i] contents], 0,
			(size_t)RAL_METAL_WEATHER_PARTICLE_CAPACITY * 32u );
	for ( uint32_t i = 0u; i < 2u; ++i ) if ( present->effectPools[i] )
		memset( [present->effectPools[i] contents], 0,
			(size_t)RAL_METAL_WEATHER_PARTICLE_CAPACITY * 32u );
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
	[present->worldLightingFallbackTexture release];
	for ( i = 0u; i < RAL_LIGHTING_RUNTIME_MAX_PLANES; ++i )
		[present->directionalLightingTextures[i] release];
	[present->uiClampSampler release];
	[present->uiRepeatSampler release];
	[present->uiPipeline release];
	[present->uiMsdfPipeline release];
	[present->uiBackdropPipeline release];
	[present->toneMapPipeline release];
	[present->worldPipeline release];
	[present->worldBlendPipeline release];
	[present->worldAlphaAdditivePipeline release];
	[present->worldAdditivePipeline release];
	[present->atmospherePipeline release];
	[present->atmosphereArena release];
	[present->atmosphereFallback release];
	[present->atmosphereVolumes release];
	[present->weatherComputePipeline release];
	[present->weatherRenderPipeline release];
	[present->effectAdditivePipeline release];
	[present->weatherFrame release];
	[present->weatherPools[0] release];
	[present->weatherPools[1] release];
	[present->effectFrame release];
	[present->effectPools[0] release];
	[present->effectPools[1] release];
	[present->worldDepthState release];
	[present->worldDepthReadState release];
	[present->worldVertexCache release];
	[present->worldIndexCache release];
	[present->worldDepthCache release];
	[present->sceneColorCache release];
	free( present->worldBatchBounds );
	[present->uiLibrary release];
	[present->drawable release];
	free( present->captureRgb );
	if ( present->ownsLayer ) [present->layer release];
	memset( present, 0, sizeof( *present ) );
	free( present );
}

qboolean RalMetal_PresentSetDirectionalLighting( ralMetalPresent_t *present,
		const ralMetalLightingReceipt_t *lightingReceipt ) {
	uint32_t plane;
	if ( !present ) return qfalse;
	if ( lightingReceipt ) {
		if ( lightingReceipt->coreGeneration != present->receipt.coreGeneration
				|| lightingReceipt->plan.backendType != RAL_BACKEND_METAL
				|| !RalMetal_LightingReceiptExact( lightingReceipt, lightingReceipt ) )
			return qfalse;
		for ( plane = 0u; plane < lightingReceipt->plan.planeCount; ++plane )
			if ( !lightingReceipt->textureIdentities[plane] ) return qfalse;
	}
	for ( plane = 0u; plane < RAL_LIGHTING_RUNTIME_MAX_PLANES; ++plane ) {
		[present->directionalLightingTextures[plane] release];
		present->directionalLightingTextures[plane] = nil;
	}
	memset( &present->directionalLightingReceipt, 0,
		sizeof( present->directionalLightingReceipt ) );
	if ( !lightingReceipt ) return qtrue;
	for ( plane = 0u; plane < lightingReceipt->plan.planeCount; ++plane )
		present->directionalLightingTextures[plane] = [(id<MTLTexture>)(void *)
			lightingReceipt->textureIdentities[plane] retain];
	present->directionalLightingReceipt = *lightingReceipt;
	return qtrue;
}

qboolean RalMetal_PresentSetDisplayVisibility( ralMetalPresent_t *present,
		const ralDisplayVisibilityPlan_t *visibility ) {
	if ( !present || present->drawable
			|| !Ral_DisplayVisibilityPlanValid( visibility ) ) return qfalse;
	present->displayVisibility = *visibility;
	present->toneMap.displayVisibility[0] = visibility->exposureScale;
	present->toneMap.displayVisibility[1] = visibility->shadowExponent;
	present->toneMap.displayVisibility[2] = visibility->shadowPivot;
	return qtrue;
}

qboolean RalMetal_PresentSetLightmapBoost( ralMetalPresent_t *present,
		float lightmapBoost ) {
	if ( !present || present->drawable || !isfinite( lightmapBoost )
			|| lightmapBoost < 1.0f || lightmapBoost > 24.0f ) return qfalse;
	present->lightmapBoost = lightmapBoost;
	return qtrue;
}

qboolean RalMetal_PresentSetShaderTimeOverride( ralMetalPresent_t *present,
		float shaderTimeOverride ) {
	if ( !present || present->drawable || !isfinite( shaderTimeOverride )
			|| shaderTimeOverride < 0.0f || shaderTimeOverride > 86400.0f )
		return qfalse;
	present->shaderTimeOverride = shaderTimeOverride;
	return qtrue;
}

qboolean RalMetal_PresentSetToneMap( ralMetalPresent_t *present,
		uint32_t mode, float exposure, float lottesContrast,
		float lottesShoulder, float lottesMidIn, float lottesMidOut,
		float lottesHdrMax ) {
	if ( !present || present->drawable || mode > 4u
			|| !isfinite( exposure ) || exposure < 0.1f || exposure > 8.0f
			|| !isfinite( lottesContrast ) || lottesContrast < 0.5f
			|| lottesContrast > 3.0f
			|| !isfinite( lottesShoulder ) || lottesShoulder < 0.5f
			|| lottesShoulder > 1.0f
			|| !isfinite( lottesMidIn ) || lottesMidIn < 0.0f
			|| lottesMidIn > 1.0f
			|| !isfinite( lottesMidOut ) || lottesMidOut < 0.0f
			|| lottesMidOut > 1.0f
			|| !isfinite( lottesHdrMax ) || lottesHdrMax < 1.0f
			|| lottesHdrMax > 64.0f ) return qfalse;
	present->toneMap.mode = mode;
	present->toneMap.exposure = exposure;
	present->toneMap.lottesContrast = lottesContrast;
	present->toneMap.lottesShoulder = lottesShoulder;
	present->toneMap.lottesMidIn = lottesMidIn;
	present->toneMap.lottesMidOut = lottesMidOut;
	present->toneMap.lottesHdrMax = lottesHdrMax;
	return qtrue;
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
		&& Ral_DisplayVisibilityPlanValid( &r->displayVisibility )
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
		&& r->legacyLightmapDrawCount + r->directionalStaticDrawCount
			<= r->loweredWorldBatchCount
		&& ( ( r->legacyLightmapDrawCount || r->directionalStaticDrawCount )
			? r->surfaceLightingBindingDigest != 0u
			: r->surfaceLightingBindingDigest == 0u )
		&& r->patchWorldBatchCount <= r->loweredWorldBatchCount
		&& r->maskedWorldBatchCount <= r->loweredWorldBatchCount
		&& r->blendedWorldBatchCount <= r->loweredWorldBatchCount
		&& r->depthWriteWorldBatchCount <= r->loweredWorldBatchCount
		&& ( r->loweredEntityIndexCount % 3u ) == 0u
		&& r->loweredEntityBatchCount <= RENDER_SUBMISSION_MAX_WORLD_BATCHES
		&& r->modelEntityCount <= RENDER_SUBMISSION_MAX_ENTITIES
		&& r->primitiveEntityCount <= RENDER_SUBMISSION_MAX_ENTITIES
		&& r->temporalEntityCount <= RENDER_SUBMISSION_MAX_ENTITIES
		&& r->localIrradianceEntityCount <= RENDER_SUBMISSION_MAX_ENTITIES
		&& r->localIrradianceDrawCount <= r->loweredEntityBatchCount
		&& r->unresolvedEntityCount <= RENDER_SUBMISSION_MAX_ENTITIES
		&& r->loweredEffectSpriteCount <= RENDER_SUBMISSION_MAX_EFFECT_SPRITES
		&& r->loweredEffectDecalCount <= RENDER_SUBMISSION_MAX_EFFECT_DECALS
		&& r->loweredEffectRibbonCount <= RENDER_SUBMISSION_MAX_EFFECT_RIBBONS
		&& r->loweredEffectBeamCount <= RAL_METAL_EFFECT_BEAM_SLOT_COUNT
		&& r->effectEmitterDispatchCount <= RENDER_SUBMISSION_MAX_EFFECT_EMITTERS
		&& r->effectParticleDrawCount <= RAL_METAL_WEATHER_PARTICLE_CAPACITY
		&& r->loweredUiPrimitiveCount <= RENDER_SUBMISSION_MAX_UI_PRIMITIVES
		&& r->texturedUiPrimitiveCount <= r->loweredUiPrimitiveCount
		&& r->msdfUiPrimitiveCount <= r->texturedUiPrimitiveCount
		&& r->atmosphereDispatchCount <= 4u
		&& r->atmosphereFroxelCount <= RAL_METAL_ATMOSPHERE_FROXEL_CAPACITY
		&& r->atmosphereCompositeCount <= 1u
		&& BoolValid( r->atmosphereCloudsActive )
		&& Ral_AtmosphereWeatherReceiptExact( &r->weather, &r->weather )
		&& r->weatherDispatchCount == ( r->weather.zeroWork ? 0u : 1u )
		&& r->weatherDrawCount == ( r->weather.zeroWork ? 0u : 1u )
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
		&& !memcmp( &a->displayVisibility, &b->displayVisibility,
			sizeof( a->displayVisibility ) )
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
		&& a->legacyLightmapDrawCount == b->legacyLightmapDrawCount
		&& a->directionalStaticDrawCount == b->directionalStaticDrawCount
		&& a->surfaceLightingBindingDigest == b->surfaceLightingBindingDigest
		&& a->patchWorldBatchCount == b->patchWorldBatchCount
		&& a->maskedWorldBatchCount == b->maskedWorldBatchCount
		&& a->blendedWorldBatchCount == b->blendedWorldBatchCount
		&& a->depthWriteWorldBatchCount == b->depthWriteWorldBatchCount
		&& a->loweredEntityIndexCount == b->loweredEntityIndexCount
		&& a->loweredEntityBatchCount == b->loweredEntityBatchCount
		&& a->modelEntityCount == b->modelEntityCount
		&& a->primitiveEntityCount == b->primitiveEntityCount
		&& a->temporalEntityCount == b->temporalEntityCount
		&& a->localIrradianceEntityCount == b->localIrradianceEntityCount
		&& a->localIrradianceDrawCount == b->localIrradianceDrawCount
		&& a->unresolvedEntityCount == b->unresolvedEntityCount
		&& a->loweredEffectSpriteCount == b->loweredEffectSpriteCount
		&& a->loweredEffectDecalCount == b->loweredEffectDecalCount
		&& a->loweredEffectRibbonCount == b->loweredEffectRibbonCount
		&& a->loweredEffectBeamCount == b->loweredEffectBeamCount
		&& a->effectEmitterDispatchCount == b->effectEmitterDispatchCount
		&& a->effectParticleDrawCount == b->effectParticleDrawCount
		&& a->effectPrimitiveDroppedCount == b->effectPrimitiveDroppedCount
		&& a->loweredUiPrimitiveCount == b->loweredUiPrimitiveCount
		&& a->texturedUiPrimitiveCount == b->texturedUiPrimitiveCount
		&& a->msdfUiPrimitiveCount == b->msdfUiPrimitiveCount
		&& a->atmosphereDispatchCount == b->atmosphereDispatchCount
		&& a->atmosphereFroxelCount == b->atmosphereFroxelCount
		&& a->atmosphereCompositeCount == b->atmosphereCompositeCount
		&& a->atmosphereCloudsActive == b->atmosphereCloudsActive
		&& Ral_AtmosphereWeatherReceiptExact( &a->weather, &b->weather )
		&& a->weatherDispatchCount == b->weatherDispatchCount
		&& a->weatherDrawCount == b->weatherDrawCount
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
	qboolean uiBackdrop[RENDER_SUBMISSION_MAX_UI_PRIMITIVES];
	ralMetalWorldViewParams_t worldViewParams;
	renderWorldSnapshot_t sceneView;
	ralMetalWorldAtmosphereParams_t worldAtmosphereParams;
	ralAtmospherePlanReceipt_t atmospherePlan;
	ralAtmosphereWeatherReceipt_t weatherPlan;
	ralMetalWorldVertex_t *worldVertexBytes = NULL;
	ralMetalWorldVertex_t *entityVertexBytes = NULL;
	uint32_t *entityIndexBytes = NULL;
	ralMetalEntityBatch_t *entityBatches = NULL;
	const renderWorldBatch_t *worldBatches = NULL;
	renderEffectPrimitiveSnapshot_t effectSnapshot;
	const spriteDesc_t *effectSprites;
	const emitterDesc_t *effectEmitters;
	const decalDesc_t *effectDecals;
	const renderEffectRibbonCommand_t *effectRibbons;
	const ribbonPoint_t *effectRibbonPoints;
	const beamDesc_t *effectBeams;
	uint32_t uiPrimitiveCount = 0u;
	uint32_t worldVertexCount = 0u, worldIndexCount = 0u, worldBatchCount = 0u;
	uint32_t entityVertexCount = 0u, entityIndexCount = 0u, entityBatchCount = 0u;
	uint32_t readbackBytes;
	uint32_t readbackSlot = 0u;
	NSUInteger captureBytesPerRow = 0u;
	uint32_t i;
	float maxRgb;
	qboolean sampleReadback;
	qboolean offscreenScene = qfalse;
	qboolean genericEffects = qfalse;
	uint32_t genericEffectParticleCount = 0u;
	uint32_t genericEffectDroppedCount = 0u;
	uint32_t persistentEffectDecalCount = 0u;
	uint32_t persistentEffectDecalDroppedCount = 0u;
	uint32_t persistentEffectBeamCount = 0u;
	uint32_t persistentEffectBeamDroppedCount = 0u;
	MTLPixelFormat worldColorFormat;
	if ( !outReceipt || !clearColor || !OwnerMatches( present, coreReceipt, layerReceipt )
			|| !present->drawable || !drawableReceipt
			|| !RalMetal_DrawableReceiptExact( drawableReceipt, &present->drawableReceipt )
			|| presentGeneration == 0u || presentGeneration == UINT64_MAX
			|| presentGeneration <= drawableReceipt->acquireGeneration
			|| presentGeneration <= present->lastPresentGeneration ) return PresentFailure( "precondition" );
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
	receipt.displayVisibility = present->displayVisibility;
	memset( &atmospherePlan, 0, sizeof( atmospherePlan ) );
	if ( frontend && !RalMetal_PresentPlanAtmosphere( present, frontend,
			presentGeneration, receipt.width, receipt.height, &atmospherePlan ) )
		return PresentFailure( "atmosphere-plan" );
	memset( &weatherPlan, 0, sizeof( weatherPlan ) );
	if ( frontend ) {
		if ( !PlanWeather( frontend, &atmospherePlan, &weatherPlan ) ) return PresentFailure( "weather-plan" );
	} else {
		ralAtmosphereWeatherRequest_t emptyWeather;
		memset( &emptyWeather, 0, sizeof( emptyWeather ) );
		emptyWeather.schemaVersion = RAL_ATMOSPHERE_WEATHER_SCHEMA_VERSION;
		emptyWeather.tier = RAL_ATMOSPHERE_TIER_OFF;
		emptyWeather.maxParticles = RAL_METAL_WEATHER_PARTICLE_CAPACITY;
		if ( !Ral_AtmospherePlanWeather( &emptyWeather, &weatherPlan ) ) return qfalse;
	}
	receipt.weather = weatherPlan;
	memcpy( receipt.clearColor, clearColor, sizeof( receipt.clearColor ) );
	readbackBytes = receipt.format == RAL_FORMAT_R16G16B16A16_SFLOAT ? 8u : 4u;
	if ( !ReapReadbackRing( present, qfalse ) ) return PresentFailure( "readback-reap" );
	sampleReadback = ( present->capturePending || !present->lastReadbackReady
		|| ( !ReadbackRingPending( present )
			&& presentGeneration - present->lastReadbackGeneration >= 60u ) )
		? qtrue : qfalse;
	if ( !BuildUiVertices( frontend, receipt.width, receipt.height, &uiVertexBytes,
			&uiPrimitiveCount, &receipt.readbackX, &receipt.readbackY ) ) return PresentFailure( "ui-vertices" );
	if ( uiPrimitiveCount ) {
		uint32_t publishedCount = 0u;
		uiPrimitives = RenderSubmission_UiPrimitives( frontend, &publishedCount );
		if ( !uiPrimitives || publishedCount != uiPrimitiveCount ) {
			free( uiVertexBytes ); return qfalse;
		}
		for ( uint32_t primitive = 0u; primitive < uiPrimitiveCount; ++primitive ) {
			qboolean textured;
			uiBackdrop[primitive] = uiPrimitives[primitive].material == INT_MAX
				? qtrue : qfalse;
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
			&worldBatchCount ) ) { free( uiVertexBytes ); return PresentFailure( "world-buffers" ); }
	if ( frontend && !UpdatePersistentEffectDecals( present, frontend,
			&persistentEffectDecalCount,
			&persistentEffectDecalDroppedCount ) ) {
		free( worldVertexBytes ); free( uiVertexBytes );
		return PresentFailure( "effect-decals" );
	}
	if ( frontend && !UpdatePersistentEffectBeams( present, frontend,
			&persistentEffectBeamCount,
			&persistentEffectBeamDroppedCount ) ) {
		free( worldVertexBytes ); free( uiVertexBytes );
		return PresentFailure( "effect-beams" );
	}
	if ( !BuildEntityVertices( present, frontend, &entityVertexBytes, &entityIndexBytes,
			&entityBatches, &entityVertexCount, &entityIndexCount,
			&entityBatchCount, &receipt.modelEntityCount,
			&receipt.primitiveEntityCount, &receipt.temporalEntityCount,
			&receipt.localIrradianceEntityCount,
			&receipt.unresolvedEntityCount ) ) {
		free( worldVertexBytes ); free( uiVertexBytes ); return PresentFailure( "entity-vertices" );
	}
	memset( &effectSnapshot, 0, sizeof( effectSnapshot ) );
	if ( frontend && !RenderSubmission_EffectPrimitiveSnapshots( frontend,
			&effectSnapshot, &effectSprites, &effectEmitters, &effectDecals,
			&effectRibbons, &effectRibbonPoints, &effectBeams ) ) {
		free( entityBatches ); free( entityIndexBytes ); free( entityVertexBytes );
		free( worldVertexBytes ); free( uiVertexBytes );
		return PresentFailure( "effect-snapshot" );
	}
	(void)effectSprites; (void)effectEmitters; (void)effectDecals;
	(void)effectRibbons; (void)effectRibbonPoints; (void)effectBeams;
	receipt.loweredEffectSpriteCount = effectSnapshot.spriteCount;
	receipt.loweredEffectDecalCount = persistentEffectDecalCount;
	receipt.loweredEffectRibbonCount = effectSnapshot.ribbonCount;
	receipt.loweredEffectBeamCount = persistentEffectBeamCount;
	receipt.effectPrimitiveDroppedCount = effectSnapshot.droppedCount
		+ persistentEffectDecalDroppedCount + persistentEffectBeamDroppedCount;
	genericEffects = GenericEffectsPending( present, frontend );
	if ( frontend && receipt.localIrradianceEntityCount
			!= frontend->localIrradianceEntityCount ) {
		free( entityBatches ); free( entityIndexBytes ); free( entityVertexBytes );
		free( worldVertexBytes ); free( uiVertexBytes ); return PresentFailure( "entity-irradiance" );
	}
	receipt.loweredUiPrimitiveCount = uiPrimitiveCount;
	receipt.loweredWorldIndexCount = worldIndexCount;
	receipt.loweredWorldBatchCount = worldBatchCount;
	receipt.loweredEntityIndexCount = entityIndexCount;
	receipt.loweredEntityBatchCount = entityBatchCount;
	memset( &sceneView, 0, sizeof( sceneView ) );
	if ( ( worldIndexCount || entityIndexCount || genericEffects )
			&& ( !BuildWorldViewParams( frontend, &worldViewParams, &sceneView )
				|| !BuildWorldAtmosphereParams( frontend, &worldViewParams,
					&atmospherePlan, &present->displayVisibility,
					&worldAtmosphereParams ) ) ) {
		free( entityBatches ); free( entityIndexBytes ); free( entityVertexBytes );
		free( worldVertexBytes ); free( uiVertexBytes ); return PresentFailure( "world-view" );
	}
	offscreenScene = ( worldIndexCount || entityIndexCount || !weatherPlan.zeroWork
		|| genericEffects )
		&& !( sceneView.rdflags & RDF_NOWORLDMODEL ) ? qtrue : qfalse;
	worldColorFormat = offscreenScene ? MTLPixelFormatRGBA16Float
		: present->drawable.texture.pixelFormat;
	@autoreleasepool {
		if ( worldIndexCount || entityIndexCount || !weatherPlan.zeroWork
				|| genericEffects ) {
			if ( ( !weatherPlan.zeroWork || genericEffects )
					&& !EnsureWeatherPipeline( present, worldColorFormat ) ) {
				free( entityBatches ); free( entityIndexBytes ); free( entityVertexBytes );
				[worldVertices release]; [worldIndices release];
				free( worldVertexBytes ); free( uiVertexBytes );
				return PresentFailure( "weather-pipeline" );
			}
			if ( ( worldIndexCount || entityIndexCount )
					&& !EnsureWorldPipeline( present, worldColorFormat ) ) {
				free( entityBatches ); free( entityIndexBytes ); free( entityVertexBytes );
				[worldVertices release]; [worldIndices release];
				free( worldVertexBytes ); free( uiVertexBytes );
				return PresentFailure( "world-pipeline" );
			}
			if ( offscreenScene && ( !EnsureSceneColor( present, receipt.width,
					receipt.height ) || !EnsureToneMapPipeline( present ) ) ) {
				free( entityBatches ); free( entityIndexBytes ); free( entityVertexBytes );
				[worldVertices release]; [worldIndices release];
				free( worldVertexBytes ); free( uiVertexBytes );
				return PresentFailure( "scene-output" );
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
				free( uiVertexBytes ); return PresentFailure( "ui-pipeline" );
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
		if ( frontend && atmospherePlan.selectedTier == RAL_ATMOSPHERE_TIER_FULL
				&& !EncodeAtmosphere( present, command, frontend, &atmospherePlan ) ) {
			[worldVertices release]; [worldIndices release]; [worldDepth release];
			[entityVertices release]; [entityIndices release]; free( entityBatches );
			[uiVertices release]; [readback release]; return qfalse;
		}
		if ( frontend && !weatherPlan.zeroWork
				&& !EncodeWeather( present, command, frontend, &weatherPlan ) ) {
			[worldVertices release]; [worldIndices release]; [worldDepth release];
			[entityVertices release]; [entityIndices release]; free( entityBatches );
			[uiVertices release]; [readback release]; return qfalse;
		}
		if ( !weatherPlan.zeroWork ) receipt.weatherDispatchCount = 1u;
		if ( frontend && genericEffects
				&& !EncodeGenericEffects( present, command, frontend, &sceneView,
					&receipt.effectEmitterDispatchCount,
					&genericEffectParticleCount,
					&genericEffectDroppedCount ) ) {
			[worldVertices release]; [worldIndices release]; [worldDepth release];
			[entityVertices release]; [entityIndices release]; free( entityBatches );
			[uiVertices release]; [readback release]; return qfalse;
		}
		receipt.effectPrimitiveDroppedCount += genericEffectDroppedCount;
		if ( atmospherePlan.selectedTier == RAL_ATMOSPHERE_TIER_FULL ) {
			receipt.atmosphereDispatchCount = atmospherePlan.computeDispatchCount;
			receipt.atmosphereFroxelCount = atmospherePlan.froxelCount;
			receipt.atmosphereCompositeCount = atmospherePlan.compositeCount;
			receipt.atmosphereCloudsActive = atmospherePlan.cloudsActive;
		}
		if ( ( worldIndexCount || entityIndexCount )
				&& atmospherePlan.selectedTier != RAL_ATMOSPHERE_TIER_FULL
				&& !EnsureAtmosphereFallback( present ) ) {
			[worldVertices release]; [worldIndices release]; [worldDepth release];
			[entityVertices release]; [entityIndices release]; free( entityBatches );
			[uiVertices release]; [readback release]; return qfalse;
		}
		MTLRenderPassDescriptor *pass = [MTLRenderPassDescriptor renderPassDescriptor];
		pass.colorAttachments[0].texture = offscreenScene
			? present->sceneColorCache : present->drawable.texture;
		pass.colorAttachments[0].loadAction = MTLLoadActionClear;
		pass.colorAttachments[0].storeAction = MTLStoreActionStore;
		pass.colorAttachments[0].clearColor = MTLClearColorMake( clearColor[0], clearColor[1],
			clearColor[2], clearColor[3] );
		if ( worldIndexCount || entityIndexCount || !weatherPlan.zeroWork
				|| genericEffectParticleCount ) {
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
		if ( entityIndexCount && ( sceneView.rdflags & RDF_NOWORLDMODEL )
				&& sceneView.uiPrimitiveInsertionIndex > 0u
				&& sceneView.uiPrimitiveInsertionIndex <= uiPrimitiveCount ) {
			/* Preserve the custom-draw painter seam: backdrop/pane primitives
			 * emitted before RenderScene must land below the worldless model,
			 * while list text, borders and the remaining menu land above it. */
			DrawUiPrimitiveRange( encoder, present, uiVertices, uiPrimitives,
				uiTextures, uiSamplers, uiMsdf, uiBackdrop, 0u,
				sceneView.uiPrimitiveInsertionIndex, receipt.width, receipt.height );
		}
		if ( worldIndexCount || entityIndexCount )
			[encoder setVertexBytes:&worldViewParams length:sizeof( worldViewParams )
				atIndex:1u];
		if ( worldIndexCount || entityIndexCount )
			[encoder setFragmentBytes:&worldAtmosphereParams
				length:sizeof( worldAtmosphereParams ) atIndex:1u];
		if ( worldIndexCount || entityIndexCount )
			[encoder setFragmentBuffer:atmospherePlan.selectedTier
				== RAL_ATMOSPHERE_TIER_FULL ? present->atmosphereArena
				: present->atmosphereFallback offset:0u atIndex:2u];
		if ( worldIndexCount || entityIndexCount ) {
			/* The portable submission preserves Q3's clockwise triangle order.
			 * Match the established Vulkan adapter exactly; viewport origin does not
			 * change the authored front-face contract. */
			[encoder setFrontFacingWinding:MTLWindingClockwise];
			[encoder setCullMode:MTLCullModeBack];
		}
		if ( entityIndexCount && ( sceneView.rdflags & RDF_NOWORLDMODEL ) ) {
			/* UI-owned previews preserve legacy two-sided character content. Keep
			 * gameplay entities on the fast back-face-cull path; a portable material
			 * cull-mode contract can widen this deliberately when it exists. */
			if ( sceneView.rdflags & RDF_NOWORLDMODEL )
				[encoder setCullMode:MTLCullModeNone];
			MTLViewport viewport;
			MTLScissorRect scissor;
			const uint32_t viewportX = sceneView.viewportX > 0
				? (uint32_t)sceneView.viewportX : 0u;
			const uint32_t viewportY = sceneView.viewportY > 0
				? (uint32_t)sceneView.viewportY : 0u;
			const uint32_t viewportWidth = sceneView.viewportWidth
				&& viewportX < receipt.width
				? MIN( sceneView.viewportWidth, receipt.width - viewportX )
				: receipt.width;
			const uint32_t viewportHeight = sceneView.viewportHeight
				&& viewportY < receipt.height
				? MIN( sceneView.viewportHeight, receipt.height - viewportY )
				: receipt.height;
			viewport.originX = viewportX; viewport.originY = viewportY;
			viewport.width = viewportWidth; viewport.height = viewportHeight;
			viewport.znear = 0.0; viewport.zfar = 1.0;
			scissor.x = viewportX; scissor.y = viewportY;
			scissor.width = viewportWidth; scissor.height = viewportHeight;
			[encoder setViewport:viewport];
			[encoder setScissorRect:scissor];
			[encoder setVertexBuffer:entityVertices offset:0u atIndex:0u];
			for ( uint32_t batchIndex = 0u; batchIndex < entityBatchCount; ++batchIndex ) {
				const ralMetalEntityBatch_t *batch = &entityBatches[batchIndex];
				if ( !batch->visible ) continue;
				if ( sceneView.rdflags & RDF_NOWORLDMODEL )
					[encoder setCullMode:MTLCullModeNone];
				else if ( batch->cullMode == RENDER_CULL_NONE )
					[encoder setCullMode:MTLCullModeNone];
				else if ( batch->cullMode == RENDER_CULL_FRONT )
					[encoder setCullMode:MTLCullModeFront];
				else
					[encoder setCullMode:MTLCullModeBack];
				id<MTLTexture> texture; id<MTLSamplerState> sampler;
				qboolean textured, msdf;
				ralMetalWorldMaterialParams_t params;
				renderMaterialSnapshot_t materialSnapshot;
				memset( &params, 0, sizeof( params ) );
				params.skyScaleScroll[0] = params.skyScaleScroll[1] = 1.0f;
				if ( RenderSubmission_MaterialSnapshot( frontend, batch->material,
						&materialSnapshot ) )
					memcpy( params.skyScaleScroll, materialSnapshot.skyScaleScroll,
						sizeof( params.skyScaleScroll ) );
				/* bit 0: lightmap bound, bit 1: authored/entity vertex colour. */
				params.hasLightmap = batch->useVertexColor ? 2u : 0u;
				/* Match the established renderer contract for first-person weapons:
				 * compress only their depth range, without changing their authored
				 * projection or leaking the range into subsequent entity/world draws. */
				viewport.zfar = batch->depthHack ? 0.3 : 1.0;
				[encoder setViewport:viewport];
				params.alphaMode = (uint32_t)batch->alphaMode;
				params.alphaCutoff = batch->alphaCutoff;
				/* bit 0: local SH is authoritative; bit 1: RF_MINLIGHT floor. */
				params.hasLocalIrradiance = ( batch->hasLocalIrradiance ? 1u : 0u )
					| ( batch->minLight ? 2u : 0u );
				for ( uint32_t coefficient = 0u; coefficient < 4u; ++coefficient )
					memcpy( params.localSh[coefficient], batch->localSh[coefficient],
						3u * sizeof( float ) );
				if ( !UiMaterial( present, frontend, batch->material, &texture,
						&sampler, &textured, &msdf ) || msdf ) {
					[encoder endEncoding];
					[worldVertices release]; [worldIndices release]; [worldDepth release];
					[entityVertices release]; [entityIndices release]; free( entityBatches );
					[uiVertices release]; [readback release]; return qfalse;
				}
				[encoder setRenderPipelineState:batch->alphaMode == RENDER_ALPHA_ALPHA_ADDITIVE
					? present->worldAlphaAdditivePipeline
					: ( batch->alphaMode == RENDER_ALPHA_ADDITIVE
					? present->worldAdditivePipeline
					: ( batch->alphaMode == RENDER_ALPHA_BLEND
						? present->worldBlendPipeline : present->worldPipeline ) )];
				[encoder setDepthStencilState:batch->depthWrite
					? present->worldDepthState : present->worldDepthReadState];
				[encoder setFragmentTexture:texture atIndex:0u];
				[encoder setFragmentTexture:present->uiFallbackTexture atIndex:1u];
				for ( uint32_t slot = 2u; slot <= 4u; ++slot ) {
					[encoder setFragmentTexture:present->worldLightingFallbackTexture
						atIndex:slot];
					[encoder setFragmentSamplerState:present->uiClampSampler atIndex:slot];
				}
				[encoder setFragmentTexture:present->uiFallbackTexture atIndex:5u];
				[encoder setFragmentSamplerState:sampler atIndex:0u];
				[encoder setFragmentSamplerState:present->uiClampSampler atIndex:1u];
				[encoder setFragmentSamplerState:present->uiClampSampler atIndex:5u];
				[encoder setFragmentBytes:&params length:sizeof( params ) atIndex:0u];
				if ( batch->hasLocalIrradiance ) receipt.localIrradianceDrawCount++;
				[encoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle
					indexCount:batch->indexCount indexType:MTLIndexTypeUInt32
					indexBuffer:entityIndices
					indexBufferOffset:(NSUInteger)batch->firstIndex * sizeof( uint32_t )];
			}
		}
		if ( worldIndexCount ) {
			MTLViewport viewport;
			const uint32_t viewportX = sceneView.viewportX > 0
				? (uint32_t)sceneView.viewportX : 0u;
			const uint32_t viewportY = sceneView.viewportY > 0
				? (uint32_t)sceneView.viewportY : 0u;
			viewport.originX = viewportX; viewport.originY = viewportY;
			viewport.width = sceneView.viewportWidth && viewportX < receipt.width
				? MIN( sceneView.viewportWidth, receipt.width - viewportX ) : receipt.width;
			viewport.height = sceneView.viewportHeight && viewportY < receipt.height
				? MIN( sceneView.viewportHeight, receipt.height - viewportY ) : receipt.height;
			viewport.znear = 0.0; viewport.zfar = 1.0;
			[encoder setViewport:viewport];
			[encoder setFrontFacingWinding:MTLWindingClockwise];
			[encoder setCullMode:MTLCullModeBack];
			[encoder setVertexBuffer:worldVertices offset:0u atIndex:0u];
			for ( uint32_t batchIndex = 0u; batchIndex < worldBatchCount; ++batchIndex ) {
				const renderWorldBatch_t *batch = &worldBatches[batchIndex];
				if ( !batch->visible ) continue;
				/* Sky portals are not ordinary lightmapped geometry.  Their authored
				 * sky/atmosphere product is composited by the dedicated background
				 * path; sampling the selected _up face with BSP UVs produces the
				 * visibly tiled Metal-only wall that this semantic prevents. */
				if ( batch->noDraw ) continue;
				if ( batch->cullMode == RENDER_CULL_NONE )
					[encoder setCullMode:MTLCullModeNone];
				else if ( batch->cullMode == RENDER_CULL_FRONT )
					[encoder setCullMode:MTLCullModeFront];
				else
					[encoder setCullMode:MTLCullModeBack];
				if ( !WorldBatchVisible( &worldViewParams,
						present->worldBatchBounds + (size_t)batchIndex * 6u ) ) continue;
				id<MTLTexture> baseTexture, lightmapTexture, skySecondaryTexture;
				id<MTLSamplerState> baseSampler, lightmapSampler, skySecondarySampler;
				id<MTLTexture> stageTextures[RENDER_MATERIAL_MAX_STAGES];
				id<MTLSamplerState> stageSamplers[RENDER_MATERIAL_MAX_STAGES];
				qboolean baseTextured, baseMsdf, lightmapped, lightmapMsdf;
				qboolean skySecondaryTextured = qfalse, skySecondaryMsdf = qfalse;
				ralMetalWorldMaterialParams_t params;
				renderMaterialSnapshot_t materialSnapshot;
				ralLightingSurfaceBindingRequest_t bindingRequest;
				ralLightingSurfaceBindingReceipt_t surfaceLighting;
				const ralLightingSurfaceBindingReceipt_t *binding = NULL;
				const ralLightingRuntimePlan_t *lightingPlan =
					present->directionalLightingReceipt.ready
						? &present->directionalLightingReceipt.plan : NULL;
				if ( !UiMaterial( present, frontend, batch->baseMaterial,
						&baseTexture, &baseSampler, &baseTextured, &baseMsdf )
						|| !UiMaterial( present, frontend, batch->lightmapMaterial,
							&lightmapTexture, &lightmapSampler, &lightmapped,
							&lightmapMsdf ) || baseMsdf || lightmapMsdf ) {
					[encoder endEncoding];
					[worldVertices release]; [worldIndices release]; [worldDepth release];
					[uiVertices release]; [readback release];
					return PresentFailure( "world-material-binding" );
				}
				if ( baseTextured ) receipt.texturedWorldBatchCount++;
				if ( lightmapped ) receipt.lightmappedWorldBatchCount++;
				if ( batch->surfaceType == RENDER_WORLD_SURFACE_PATCH )
					receipt.patchWorldBatchCount++;
				if ( batch->alphaMode == RENDER_ALPHA_MASK )
					receipt.maskedWorldBatchCount++;
				if ( batch->alphaMode == RENDER_ALPHA_BLEND
						|| batch->alphaMode == RENDER_ALPHA_ADDITIVE
						|| batch->alphaMode == RENDER_ALPHA_ALPHA_ADDITIVE )
					receipt.blendedWorldBatchCount++;
				if ( batch->depthWrite ) receipt.depthWriteWorldBatchCount++;
				memset( &params, 0, sizeof( params ) );
				/* BSP vertex colour is the lighting source only for vertex-lit
				 * surfaces. Lightmapped surfaces must not multiply both products. */
				params.hasLightmap = batch->sky ? 4u
					: ( batch->lightmapIndex < 0 ? 2u : 0u );
				params.alphaMode = (uint32_t)batch->alphaMode;
				params.alphaCutoff = batch->alphaCutoff;
				/* Registration boundaries may legitimately leave a world batch with
				 * no portable material snapshot for one frame (or permanently for
				 * legacy content). Vulkan renders that surface through its default
				 * material; Metal must degrade at the same per-surface boundary rather
				 * than poisoning the entire present lifecycle. */
				memset( &materialSnapshot, 0, sizeof( materialSnapshot ) );
				materialSnapshot.skyScaleScroll[0] = 1.0f;
				materialSnapshot.skyScaleScroll[1] = 1.0f;
				materialSnapshot.skySecondaryScaleScroll[0] = 1.0f;
				materialSnapshot.skySecondaryScaleScroll[1] = 1.0f;
				(void)RenderSubmission_MaterialSnapshot( frontend,
					batch->baseMaterial, &materialSnapshot );
				if ( materialSnapshot.skyBox ) params.hasLightmap |= 16u;
				if ( materialSnapshot.lighting.schemaVersion
						== RENDER_MATERIAL_LIGHTING_SCHEMA_VERSION )
					for ( uint32_t channel = 0u; channel < 3u; ++channel )
						params.emissiveRadiance[channel] =
							(float)materialSnapshot.lighting.emissionRadianceQ16[channel]
							/ (float)RENDER_MATERIAL_LIGHTING_Q16_ONE;
				memcpy( params.skyScaleScroll, materialSnapshot.skyScaleScroll,
					sizeof( params.skyScaleScroll ) );
				memcpy( params.skySecondaryScaleScroll,
					materialSnapshot.skySecondaryScaleScroll,
					sizeof( params.skySecondaryScaleScroll ) );
				params.skyTime = present->shaderTimeOverride > 0.0f
					? present->shaderTimeOverride
					: (float)sceneView.timeMs * 0.001f;
				params.skyCloudHeight = materialSnapshot.skyCloudHeight;
				params.lightmapBoost = present->lightmapBoost;
				params.skySecondaryAlphaMode =
					(uint32_t)materialSnapshot.skySecondaryAlphaMode;
				params.stageProgram[0] = materialSnapshot.stageCount;
				for ( uint32_t stageIndex = 0u;
						stageIndex < RENDER_MATERIAL_MAX_STAGES; ++stageIndex ) {
					const renderMaterialStageSnapshot_t *stage =
						&materialSnapshot.stages[stageIndex];
					ralMetalMaterialStageParams_t *stageParams =
						&params.stages[stageIndex];
					qboolean stageTextured = qfalse, stageMsdf = qfalse;
					stageTextures[stageIndex] = present->uiFallbackTexture;
					stageSamplers[stageIndex] = present->uiClampSampler;
					if ( stageIndex >= materialSnapshot.stageCount ) continue;
					stageParams->meta[0] = stage->sourceBlend;
					stageParams->meta[1] = stage->destinationBlend;
					stageParams->meta[2] = stage->imageSource | ( stage->tcGen << 8u );
					stageParams->meta[3] = stage->alphaTest;
					stageParams->params[0] = stage->alphaCutoff;
					stageParams->params[1] = stage->rotateDegrees;
					stageParams->params[2] = stage->hasTurbulence ? 1.0f : 0.0f;
					stageParams->params[3] = stage->hasStretch ? 1.0f : 0.0f;
					memcpy( stageParams->scaleScroll, stage->scaleScroll,
						sizeof( stageParams->scaleScroll ) );
					memcpy( stageParams->turbulence, stage->turbulence,
						sizeof( stageParams->turbulence ) );
					memcpy( stageParams->stretch, stage->stretch,
						sizeof( stageParams->stretch ) );
					if ( stage->imageSource == 1u ) {
						stageTextures[stageIndex] = lightmapped
							? lightmapTexture : present->uiFallbackTexture;
						stageSamplers[stageIndex] = lightmapSampler;
					} else if ( stage->imageSource == 0u ) {
						if ( !UiMaterial( present, frontend, stage->material,
								&stageTextures[stageIndex], &stageSamplers[stageIndex],
								&stageTextured, &stageMsdf ) || stageMsdf ) {
							[encoder endEncoding];
							[worldVertices release]; [worldIndices release];
							[worldDepth release]; [uiVertices release];
							[readback release];
							return PresentFailure( "world-stage-material" );
						}
					}
				}
				skySecondaryTexture = present->uiFallbackTexture;
				skySecondarySampler = present->uiClampSampler;
				if ( materialSnapshot.skySecondaryMaterial > 0 ) {
					if ( !UiMaterial( present, frontend,
							materialSnapshot.skySecondaryMaterial,
							&skySecondaryTexture, &skySecondarySampler,
							&skySecondaryTextured, &skySecondaryMsdf )
							|| skySecondaryMsdf ) {
						[encoder endEncoding];
						[worldVertices release]; [worldIndices release];
						[worldDepth release]; [uiVertices release];
						[readback release];
						return PresentFailure( "world-sky-secondary" );
					}
					if ( skySecondaryTextured ) params.hasLightmap |= 8u;
				}
				if ( batch->lightmapIndex >= 0 ) {
					memset( &bindingRequest, 0, sizeof( bindingRequest ) );
					bindingRequest.schemaVersion =
						RAL_LIGHTING_SURFACE_BINDING_SCHEMA_VERSION;
					bindingRequest.frameGeneration = presentGeneration;
					bindingRequest.surfaceId =
						(uint64_t)batch->sourceSurfaceIndex + 1u;
					bindingRequest.lightmapIndex = batch->lightmapIndex;
					bindingRequest.legacyLightmapAvailable = lightmapped;
					if ( !Ral_LightingSurfaceBindingBuild( &bindingRequest,
							lightingPlan, &surfaceLighting ) ) {
						[encoder endEncoding];
						[worldVertices release]; [worldIndices release];
						[worldDepth release]; [uiVertices release];
						[readback release];
						return PresentFailure( "world-lighting-binding" );
					}
					binding = &surfaceLighting;
				}
				if ( binding && binding->directionalStaticBound ) {
					params.staticLightingMode = 2u;
					params.staticLightingLayer = binding->arrayLayer;
					params.staticLightingEncoding = (uint32_t)lightingPlan->encoding;
					params.hasStaticVisibility = binding->visibilityPlane
						!= RAL_LIGHTING_SURFACE_BINDING_NO_PLANE;
				} else if ( binding && binding->legacyLightmapBound ) {
					params.hasLightmap |= 1u;
					params.staticLightingMode = 1u;
				}
				[encoder setRenderPipelineState:batch->alphaMode == RENDER_ALPHA_ALPHA_ADDITIVE
					? present->worldAlphaAdditivePipeline
					: ( batch->alphaMode == RENDER_ALPHA_ADDITIVE
					? present->worldAdditivePipeline
					: ( batch->alphaMode == RENDER_ALPHA_BLEND
						? present->worldBlendPipeline : present->worldPipeline ) )];
				[encoder setDepthStencilState:batch->depthWrite
					? present->worldDepthState : present->worldDepthReadState];
				[encoder setFragmentTexture:baseTexture atIndex:0u];
				[encoder setFragmentTexture:params.staticLightingMode == 1u
					? lightmapTexture : present->uiFallbackTexture atIndex:1u];
				[encoder setFragmentTexture:params.staticLightingMode == 2u
					? present->directionalLightingTextures[binding->radiancePlane]
					: present->worldLightingFallbackTexture atIndex:2u];
				[encoder setFragmentTexture:params.staticLightingMode == 2u
					? present->directionalLightingTextures[binding->directionPlane]
					: present->worldLightingFallbackTexture atIndex:3u];
				[encoder setFragmentTexture:params.staticLightingMode == 2u
						&& binding->visibilityPlane
							!= RAL_LIGHTING_SURFACE_BINDING_NO_PLANE
					? present->directionalLightingTextures[binding->visibilityPlane]
					: present->worldLightingFallbackTexture atIndex:4u];
				[encoder setFragmentTexture:skySecondaryTexture atIndex:5u];
				[encoder setFragmentSamplerState:baseSampler atIndex:0u];
				[encoder setFragmentSamplerState:lightmapSampler atIndex:1u];
				for ( uint32_t slot = 2u; slot <= 4u; ++slot )
					[encoder setFragmentSamplerState:present->uiClampSampler atIndex:slot];
				[encoder setFragmentSamplerState:skySecondarySampler atIndex:5u];
				for ( uint32_t stageIndex = 0u;
						stageIndex < RENDER_MATERIAL_MAX_STAGES; ++stageIndex ) {
					[encoder setFragmentTexture:stageTextures[stageIndex]
						atIndex:6u + stageIndex];
					[encoder setFragmentSamplerState:stageSamplers[stageIndex]
						atIndex:6u + stageIndex];
				}
				[encoder setFragmentBytes:&params length:sizeof( params ) atIndex:0u];
				if ( binding ) {
					uint64_t identity = binding->surfaceId
						^ ( (uint64_t)binding->diffuseAuthority << 56u )
						^ ( (uint64_t)binding->arrayLayer << 24u );
					if ( binding->legacyLightmapBound )
						receipt.legacyLightmapDrawCount++;
					else receipt.directionalStaticDrawCount++;
					receipt.surfaceLightingBindingDigest ^= identity
						+ UINT64_C( 0x9e3779b97f4a7c15 )
						+ ( receipt.surfaceLightingBindingDigest << 6u )
						+ ( receipt.surfaceLightingBindingDigest >> 2u );
					if ( !receipt.surfaceLightingBindingDigest )
						receipt.surfaceLightingBindingDigest = 1u;
				}
				[encoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle
					indexCount:batch->indexCount indexType:MTLIndexTypeUInt32
					indexBuffer:worldIndices
					indexBufferOffset:(NSUInteger)batch->firstIndex * sizeof( uint32_t )];
			}
		}
		/* Gameplay entities must be submitted after the BSP cohort.  In
		 * particular, depth-read/no-write sprites are transparent surfaces: if
		 * they run before opaque world batches, the later BSP pass paints over
		 * every visible lens-flare texel even though the entity was accepted. */
		if ( entityIndexCount && !( sceneView.rdflags & RDF_NOWORLDMODEL )
				&& !DrawEntityBatches( encoder, present, frontend,
					entityVertices, entityIndices, entityBatches, entityBatchCount,
					&sceneView, &receipt ) ) {
			[encoder endEncoding];
			[worldVertices release]; [worldIndices release]; [worldDepth release];
			[entityVertices release]; [entityIndices release]; free( entityBatches );
			[uiVertices release]; [readback release];
			return PresentFailure( "entity-draw" );
		}
		if ( !weatherPlan.zeroWork ) {
			[encoder setCullMode:MTLCullModeNone];
			uint32_t renderedPool = present->weatherParity ? 1u : 0u;
			[encoder setRenderPipelineState:present->weatherRenderPipeline];
			[encoder setDepthStencilState:present->worldDepthReadState];
			[encoder setVertexBuffer:present->weatherFrame offset:0u atIndex:1u];
			[encoder setVertexBuffer:present->weatherPools[renderedPool]
				offset:0u atIndex:2u];
			[encoder setFragmentTexture:present->uiFallbackTexture atIndex:0u];
			[encoder setFragmentSamplerState:present->uiClampSampler atIndex:0u];
			[encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0u
				vertexCount:6u instanceCount:weatherPlan.activeParticleCount];
			receipt.weatherDrawCount = 1u;
		}
		if ( genericEffectParticleCount ) {
			[encoder setCullMode:MTLCullModeNone];
			uint32_t renderedPool = present->effectParity ? 1u : 0u;
			[encoder setDepthStencilState:present->worldDepthReadState];
			[encoder setVertexBuffer:present->effectFrame offset:0u atIndex:1u];
			[encoder setVertexBuffer:present->effectPools[renderedPool]
				offset:0u atIndex:2u];
			for ( uint32_t slot = 0u; slot < RAL_METAL_EFFECT_SLOT_COUNT; ++slot ) {
				const ralMetalEffectSlot_t *active = &present->effectSlots[slot];
				id<MTLTexture> texture;
				id<MTLSamplerState> sampler;
				qboolean textured, msdf;
				uint32_t count;
				if ( !active->active ) continue;
				count = (uint32_t)MAX( active->emitter.count, 0 );
				count = MIN( count, RAL_METAL_EFFECT_PARTICLES_PER_SLOT );
				if ( !count ) continue;
				if ( !UiMaterial( present, frontend, active->particleClass.shader,
						&texture, &sampler, &textured, &msdf ) || msdf ) {
					[encoder endEncoding];
					[worldVertices release]; [worldIndices release];
					[worldDepth release]; [entityVertices release];
					[entityIndices release]; free( entityBatches );
					[uiVertices release]; [readback release]; return qfalse;
				}
				[encoder setRenderPipelineState:
					( active->particleClass.renderFlags & PRIM_FLAG_ADDITIVE )
					? present->effectAdditivePipeline
					: present->weatherRenderPipeline];
				[encoder setFragmentTexture:texture atIndex:0u];
				[encoder setFragmentSamplerState:sampler atIndex:0u];
				[encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0u
					vertexCount:6u instanceCount:count
					baseInstance:slot * RAL_METAL_EFFECT_PARTICLES_PER_SLOT];
			}
			receipt.effectParticleDrawCount = genericEffectParticleCount;
		}
		if ( uiPrimitiveCount && !offscreenScene ) {
			uint32_t first = entityIndexCount
				&& ( sceneView.rdflags & RDF_NOWORLDMODEL )
				&& sceneView.uiPrimitiveInsertionIndex <= uiPrimitiveCount
				? sceneView.uiPrimitiveInsertionIndex : 0u;
			DrawUiPrimitiveRange( encoder, present, uiVertices, uiPrimitives,
				uiTextures, uiSamplers, uiMsdf, uiBackdrop, first,
				uiPrimitiveCount, receipt.width, receipt.height );
		}
		[encoder endEncoding];
		if ( offscreenScene ) {
			MTLRenderPassDescriptor *outputPass =
				[MTLRenderPassDescriptor renderPassDescriptor];
			outputPass.colorAttachments[0].texture = present->drawable.texture;
			outputPass.colorAttachments[0].loadAction = MTLLoadActionClear;
			outputPass.colorAttachments[0].storeAction = MTLStoreActionStore;
			outputPass.colorAttachments[0].clearColor = MTLClearColorMake(
				clearColor[0], clearColor[1], clearColor[2], clearColor[3] );
			id<MTLRenderCommandEncoder> outputEncoder =
				[command renderCommandEncoderWithDescriptor:outputPass];
			if ( !outputEncoder ) {
				[worldVertices release]; [worldIndices release]; [worldDepth release];
				[entityVertices release]; [entityIndices release]; free( entityBatches );
				[uiVertices release]; [readback release];
				return PresentFailure( "output-encoder" );
			}
			[outputEncoder setRenderPipelineState:present->toneMapPipeline];
			[outputEncoder setFragmentTexture:present->sceneColorCache atIndex:0u];
			[outputEncoder setFragmentSamplerState:present->uiClampSampler atIndex:0u];
			[outputEncoder setFragmentBytes:&present->toneMap
				length:sizeof( present->toneMap ) atIndex:0u];
			[outputEncoder drawPrimitives:MTLPrimitiveTypeTriangle
				vertexStart:0u vertexCount:3u];
			if ( uiPrimitiveCount )
				DrawUiPrimitiveRange( outputEncoder, present, uiVertices, uiPrimitives,
					uiTextures, uiSamplers, uiMsdf, uiBackdrop, 0u,
					uiPrimitiveCount, receipt.width, receipt.height );
			[outputEncoder endEncoding];
		}
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
			[uiVertices release]; [captureReadback release]; [readback release];
			return PresentFailure( "command-publish" );
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
		qboolean readbackReaped = ReapReadbackRing( present, qfalse );
		qboolean submissionPublished = readbackReaped && present->lastReadbackReady
			? RalMetal_CorePublishSubmission( present->core, &lifecycle, &executable,
				&receipt.submission ) : qfalse;
		if ( !readbackReaped || !present->lastReadbackReady || !submissionPublished ) {
			const char *failureStage = !readbackReaped ? "submission-readback-reap"
				: ( !present->lastReadbackReady ? "submission-readback-ready"
					: "submission-publish" );
			[captureReadback release]; [readback release];
			[worldVertices release]; [worldIndices release]; [worldDepth release];
			[entityVertices release]; [entityIndices release]; free( entityBatches );
			[uiVertices release];
			[present->drawable release]; present->drawable = nil;
			memset( &present->drawableReceipt, 0, sizeof( present->drawableReceipt ) );
			return PresentFailure( failureStage );
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
	if ( !PresentReceiptValid( &receipt ) ) return PresentFailure( "receipt" );
	*outReceipt = receipt;
	return qtrue;
}
