// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "ral_metal_present.h"
#include "ral_metal_fragment_bindings.h"
#include "ral_metal_encoder_value.h"
#include "ral_metal_upload_cache.h"
#include "ral_metal_timing.h"
#include <float.h>
#include "ral_beam_geometry.h"
#include "ral_metal_internal.h"
#include "ral_metal_exposure.h"
#include "ral_metal_bloom.h"
#include "ral_metal_smaa.h"
#include "ral_metal_taa.h"
extern "C" {
#include "../../core/ral_temporal.h"
}
#include "ral_metal_atmosphere.h"
#include "ral_metal_ui_metallib.h"
#include "ral_metal_particle_compute_metallib.h"
#include "ral_metal_particle_vertex_metallib.h"
#include "ral_metal_particle_fragment_metallib.h"
#include "ral_metal_sky_source_metallib.h"
#include "ral_metal_sky_project_sh_metallib.h"
#include "ral_metal_sky_irradiance_metallib.h"
#include "ral_metal_sky_prefilter_metallib.h"
#include "ral_sky_radiance.h"
#include "ral_particle_contract.h"
#include "ral_lighting_composition.h"
#include "render_submission.h"
#include "render_submission_material.h"
#include "render_material_stage_gpu.h"
#include "render_submission_model.h"
#include "render_submission_ui.h"
#include "render_temporal_entity_submission.h"
#include "render_submission_world.h"

#import <CoreGraphics/CoreGraphics.h>
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#import <QuartzCore/CABase.h>
#import <dispatch/dispatch.h>

#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static qboolean PresentFailure( const char *stage ) {
	fprintf( stderr, "Wired native Metal RAL: present failure stage=%s\n",
		stage ? stage : "unknown" );
	return qfalse;
}

static id<MTLLibrary> NewEmbeddedLibrary( id<MTLDevice> device,
		const unsigned char *bytes, size_t byteCount );

typedef struct {
	qhandle_t handle;
	uint64_t generation;
	uint64_t digest;
	id<MTLTexture> texture;
	ralTextureResourceReceipt_t resource;
	ralMaterialRuntimeResource_t runtimeResource;
} ralMetalMaterialCacheEntry_t;

#define RAL_METAL_MAX_DIRECT_LIGHTS_PER_DRAW 4u
#define RAL_METAL_SKY_TRANSMITTANCE_WIDTH 256u
#define RAL_METAL_SKY_TRANSMITTANCE_HEIGHT 64u
#define RAL_METAL_SKY_MULTISCATTER_SIZE 32u
#define RAL_METAL_SKY_VIEW_WIDTH 192u
#define RAL_METAL_SKY_VIEW_HEIGHT 108u
#define RAL_METAL_SKY_SOURCE_SIZE WIRED_SKY_SOURCE_SIZE
#define RAL_METAL_SKY_IRRADIANCE_SIZE WIRED_SKY_IRRADIANCE_SIZE
#define RAL_METAL_SKY_RADIANCE_MIPS WIRED_SKY_SPECULAR_MIP_COUNT
#define RAL_METAL_LENS_ORACLE_RING 3u

enum {
	RAL_METAL_SKY_TRANSMITTANCE = 0,
	RAL_METAL_SKY_MULTISCATTER,
	RAL_METAL_SKY_VIEW,
	RAL_METAL_SKY_PHYSICAL_TEXTURE_COUNT
};

enum {
	RAL_METAL_SKY_SOURCE_PIPELINE = 0,
	RAL_METAL_SKY_PROJECT_SH_PIPELINE,
	RAL_METAL_SKY_IRRADIANCE_PIPELINE,
	RAL_METAL_SKY_PREFILTER_PIPELINE,
	RAL_METAL_SKY_RADIANCE_PIPELINE_COUNT
};

typedef struct {
	float originRadius[4];
	uint32_t meta[4];
} ralMetalLensSourceInput_t;

typedef struct {
	float view[5][4];
	float viewportOpticsBias[4];
} ralMetalLensOracleParams_t;

typedef struct {
	id<MTLBuffer> vertices;
	id<MTLBuffer> indices;
	float *batchBounds;
	uint64_t digest;
	uint32_t vertexCount;
	uint32_t indexCount;
} ralMetalWorldCacheSlot_t;

/* Entity object motion (TAA). BuildEntityVertices lists the rigid (inline BSP) entities with a motion identity; once the frame's TAA
 * plan exists, EntityMotionPrepare records their poses into the shared entity pose cache and, for those with a committed previous
 * pose, writes each vertex's previous-frame world position into `previous` (parallel to the entity vertices) and marks their
 * motion-writing batches. The pose-cache transaction is finished with the TAA frame: committed with it, discarded otherwise. */
typedef struct {
	uint32_t command;                  /* index into the frame's entity commands */
	uint32_t firstVertex, vertexCount;
	uint32_t firstBatch, batchCount;
} ralMetalEntityMotionCandidate_t;

typedef struct {
	ralMetalEntityMotionCandidate_t *candidates;
	uint32_t candidateCount, candidateCapacity;
	float (*previous)[4];
	uint32_t previousCapacity;
	int world;
	uint64_t frame;                    /* the open pose-cache transaction, 0 = none */
	uint32_t recorded, rejected, previousValid, motionBatches;   /* the last three accumulate; motionBatches is this frame's */
	uint32_t motionBatchesTotal, motionFrames;
} ralMetalEntityMotion_t;

struct ralMetalPresent_s {
	ralMetalCore_t *core;
	CAMetalLayer *layer;
	id<CAMetalDrawable> drawable;
	ralMetalPresentLayerReceipt_t receipt;
	ralMetalDrawableReceipt_t drawableReceipt;
	uint64_t lastAcquireGeneration;
	/* A native commit consumes the attempt even when later receipt/readback
	 * publication fails. Only pre-commit failures may retry that generation. */
	uint64_t lastSubmittedGeneration;
	wiredMetalUploadCache materialUniformCache;
	wiredMetalUploadCache entityVertexUploadCache;
	wiredMetalUploadCache entityIndexUploadCache;
	wiredMetalUploadCache uiVertexUploadCache;
	wiredMetalUploadCache particleUploadCaches[7];
	wiredMetalUploadCache atmosphereUploadCache;
	id<MTLLibrary> uiLibrary;
	id<MTLRenderPipelineState> uiPipeline;
	id<MTLRenderPipelineState> uiMsdfPipeline;
	id<MTLRenderPipelineState> uiBackdropPipeline;
	id<MTLRenderPipelineState> toneMapPipeline;
	id<MTLRenderPipelineState> worldPipeline;
	id<MTLRenderPipelineState> worldBlendPipeline;
	id<MTLRenderPipelineState> worldAlphaAdditivePipeline;
	id<MTLRenderPipelineState> worldAdditivePipeline;
	id<MTLDepthStencilState> uiDepthDisabledState;
	ralMetalAtmosphere_t *atmosphereGpu;
	id<MTLComputePipelineState> lensVisibilityPipeline;
	id<MTLBuffer> atmosphereFallback;
	qboolean atmosphereHistoryValid;
	qboolean atmosphereHistoryParity;
	ralSkyEnvironmentRuntimeState_t skyRuntime;
	ralSkyRadianceState_t skyRadianceState;
	ralSkyRadianceReceipt_t skyRadianceReceipt;
	ralSkyRadianceConsumerReceipt_t skyRadianceConsumer;
	uint64_t skyRadianceFrameGeneration;
	float skyRadianceTimeline;
	ralAtmospherePlanReceipt_t atmosphereReceipt;
	uint32_t atmosphereReceiptWidth;
	uint32_t atmosphereReceiptHeight;
	uint64_t skyWorldLifecycleGeneration;
	id<MTLTexture> skySource;
	id<MTLTexture> skyAuthored;
	id<MTLTexture> skyAuthoredFaces[RAL_MATERIAL_SKY_FACE_COUNT];
	id<MTLRenderPipelineState> skyCapturePipeline;
	float skyCaptureTime;
	qboolean skyMaterialUsesTime;
	id<MTLTexture> skyIrradiance[2];
	id<MTLTexture> skyRadiance[2];
	id<MTLTexture> skyRadianceMipViews[2][RAL_METAL_SKY_RADIANCE_MIPS];
	id<MTLBuffer> skySh[2];
	id<MTLTexture> skyPhysical[RAL_METAL_SKY_PHYSICAL_TEXTURE_COUNT];
	id<MTLComputePipelineState> skyPhysicalPipeline[RAL_METAL_SKY_PHYSICAL_TEXTURE_COUNT];
	id<MTLSamplerState> skyPhysicalSampler;
	id<MTLCommandBuffer> skyWorkCommand;
	uint32_t skySubmittedWorkMask;
	uint64_t skySubmittedFrameGeneration;
	ralSkyGpuTimingState_t skyPhysicalTimingState;
	uint64_t skyPhysicalTimingFrameGeneration;
	uint64_t skyPhysicalGpuDurationNanoseconds;
	id<MTLLibrary> skyRadianceLibrary[RAL_METAL_SKY_RADIANCE_PIPELINE_COUNT];
	id<MTLComputePipelineState> skyRadiancePipeline[RAL_METAL_SKY_RADIANCE_PIPELINE_COUNT];
	id<MTLArgumentEncoder> skyRadianceArguments[RAL_METAL_SKY_RADIANCE_PIPELINE_COUNT];
	id<MTLSamplerState> skyRadianceSampler;
	id<MTLCommandBuffer> skyRadianceCommand;
	uint32_t skyRadianceSubmittedSliceMask;
	uint64_t skyRadianceSubmittedRevision;
	uint64_t skyRadianceSubmittedFrameGeneration;
	ralSkyGpuTimingState_t skyRadianceTimingState;
	uint64_t skyRadianceTimingFrameGeneration;
	uint64_t skyRadianceGpuDurationNanoseconds;
	ralSkyGpuTelemetryReceipt_t skyGpuTelemetry;
	qboolean skyIrradianceWriteStarted;
	qboolean skyRadianceWriteStarted;
	uint64_t skyRadianceDispatchCount;
	uint64_t skyPhysicalDispatchCount;
	uint64_t skyResourceWorldLifecycleGeneration;
	qboolean skyResourcesReady;
	id<MTLComputePipelineState> weatherComputePipeline;
	id<MTLRenderPipelineState> weatherRenderPipeline;
	id<MTLLibrary> particleComputeLibrary;
	id<MTLLibrary> particleVertexLibrary;
	id<MTLLibrary> particleFragmentLibrary;
	id<MTLComputePipelineState> particleIntegratePipeline;
	id<MTLComputePipelineState> particleSpawnPipeline;
	id<MTLRenderPipelineState> particleAlphaPipeline;
	id<MTLRenderPipelineState> particleAdditivePipeline;
	id<MTLArgumentEncoder> particleIntegrateArguments;
	id<MTLArgumentEncoder> particleSpawnArguments;
	id<MTLArgumentEncoder> particleVertexArguments;
	id<MTLArgumentEncoder> particleFragmentArguments;
	id<MTLBuffer> weatherPools[2];
	id<MTLBuffer> weatherHeightgrids[MAX_RENDER_WORLDS];
	uint64_t weatherHeightgridDigests[MAX_RENDER_WORLDS];
	qboolean weatherParity;
	float weatherTimeline;
	id<MTLBuffer> effectPools[2];
	id<MTLBuffer> particleProfiles;
id<MTLBuffer> particleChildEvents;
renderEffectParticlePlanState_t effectParticlePlan;
qboolean effectParity;
	float effectTimeline;
	float effectActiveUntil;
	/* Shared lifetime authority; the inactive bank is reusable frame scratch.
	 * Publishing a submitted candidate only swaps the bank, with no allocation. */
	renderEffectDecalPool_t effectDecalPools[2];
	renderEffectBeamPool_t effectBeamPools[2];
	uint32_t effectPoolIndex;
	MTLPixelFormat weatherPixelFormat;
	MTLPixelFormat particlePixelFormat;
	id<MTLDepthStencilState> worldDepthState;
	id<MTLDepthStencilState> worldDepthReadState;
	id<MTLRenderPipelineState> taaMaskPipeline;   /* entity coverage into the TAA effect mask */
	id<MTLBuffer> taaMaskMaterial;                 /* unused since the trust pass binds each batch's own material; kept nil */
	id<MTLRenderPipelineState> taaTrustPipelines[3];   /* world material fragment specialized to TAA trust: blend, alpha-additive, additive */
	id<MTLRenderPipelineState> taaMotionPipeline;      /* entity object velocity into the TAA effect mask (green, blue, alpha) */
	id<MTLDepthStencilState> worldDepthEqualState;
	id<MTLDepthStencilState> worldDepthEqualReadState;
	ralMetalWorldCacheSlot_t worldCaches[MAX_RENDER_WORLDS];
	renderDistantWorldDrawMetric_t distantMetricScratch[
		RAL_SKY_DISTANT_MAX_CANDIDATES];
	ralSkyDistantDrawCandidate_t distantCandidateScratch[
		RAL_SKY_DISTANT_MAX_CANDIDATES];
	id<MTLTexture> worldDepthCache;
	id<MTLBuffer> lensInputBuffers[RAL_METAL_LENS_ORACLE_RING];
	id<MTLBuffer> lensResultBuffers[RAL_METAL_LENS_ORACLE_RING];
	id<MTLCommandBuffer> lensCommands[RAL_METAL_LENS_ORACLE_RING];
	qboolean lensActive[RAL_METAL_LENS_ORACLE_RING][LENS_MAX_CGSOURCES];
	uint64_t lensGenerations[RAL_METAL_LENS_ORACLE_RING];
	float lensVisibility[LENS_MAX_CGSOURCES];
	qboolean lensVisibilityActive[LENS_MAX_CGSOURCES];
	uint64_t lensVisibilityGeneration;
	uint32_t nextLensSlot;
	id<MTLTexture> sceneDepthCache;
	/* 1x1 depth stand-in bound at texture 19 whenever a world-fragment draw
	 * has no scene-depth snapshot (sceneFogParams.w == 0 never reads it). */
	id<MTLTexture> depthFallbackTexture;
	id<MTLTexture> sceneColorCache;
	/* Last completed auto-exposure accumulator (1-frame delay, like Vulkan's
	 * host-coherent exposure UBO); 0 until the first completion. */
	float adaptedExposureBias;
	uint32_t worldDepthWidth;
	uint32_t worldDepthHeight;
	uint32_t sceneDepthWidth;
	uint32_t sceneDepthHeight;
	uint32_t sceneColorWidth;
	uint32_t sceneColorHeight;
	id<MTLTexture> uiFallbackTexture;
	id<MTLTexture> worldLightingFallbackTexture;
	id<MTLSamplerState> uiClampSampler;
	id<MTLSamplerState> materialClampSampler;
	id<MTLSamplerState> materialRepeatSampler;
	ralSamplerCreateInfo_t materialSampling;
	id<MTLSamplerState> uiRepeatSampler;
	id<MTLTexture> directionalLightingTextures[RAL_LIGHTING_RUNTIME_MAX_PLANES];
	ralMetalLightingReceipt_t directionalLightingReceipt;
	ralMetalMaterialCacheEntry_t uiMaterials[RENDER_SUBMISSION_MAX_TEXTURE_RESOURCES];
	uintptr_t cookedMaterialTextureIdentities[RENDER_SUBMISSION_MAX_MATERIALS];
	qboolean cookedMaterialReady[RENDER_SUBMISSION_MAX_MATERIALS];
	uint64_t cookedMaterialRuntimeGeneration;
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
	id<MTLCommandBuffer> readbackSceneCommands[3];
	CFTimeInterval readbackCommitTimes[3];
	CFTimeInterval readbackAcquireBeginTimes[3], readbackAcquireEndTimes[3];
	uint64_t readbackGenerations[3];
	uint32_t readbackByteCounts[3];
	uint32_t nextReadbackSlot;
	uint64_t lastReadbackGeneration;
	ralMetalGpuTiming_t gpuTiming;
	uint8_t lastReadbackBytes[8];
	uint32_t lastReadbackByteCount;
	qboolean lastReadbackReady;
	qboolean ownsLayer;
	ralDisplayVisibilityPlan_t displayVisibility;
	float lightmapBoost;
	float aerialHaze;
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
	ralMetalExposure_t *exposure;
	ralExposureSettings_t exposureSettings;
	ralBloomSettings_t bloomSettings;
	ralMetalBloom_t *bloom;
	ralMetalSmaa_t *smaa;
	int smaaQuality;
	/* Temporal AA (r_aa 1). The plan state machine is the backend-neutral RAL one (jitter phase, history ping-pong, resets). */
	ralMetalTaa_t *taa;
	qboolean taaEnabled;
	ralTemporalState_t taaState;
	uint64_t taaFrameId;
	uint32_t taaResolvedFrames, taaBootstrapFrames;   /* evidence that TAA resolves in real play, not only the bootstrap copy */
	uint32_t taaTopologyEpoch;
	uint64_t taaWorldDigest;
	float taaPreviousView[20];
	ralMetalEntityMotion_t entityMotion;
	qboolean exposureHistoryValid;
	int exposureTimeMs;
	uint64_t exposureWorldDigest;
	uint32_t exposureWidth, exposureHeight;
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
	/* Q3 tess coordinates precede the model transform; world/sprites use world. */
	float materialPosition[4];
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
	uint32_t hasLocalIrradiance;
	float localSh[4][4];
	uint32_t staticLightingMode;
	uint32_t staticLightingLayer;
	uint32_t staticLightingEncoding;
	uint32_t hasStaticVisibility;
	float emissiveRadiance[4];
	float materialBaseColor[4];
	float materialFactors[4];
	float skyScaleScroll[4];
	float skySecondaryScaleScroll[4];
	float skyTime;
	uint32_t skySecondaryAlphaMode;
	float skyCloudHeight;
	float lightmapBoost;
	uint32_t stageProgram[4];
	uint32_t directLightMeta[4];
	float directLightPositionRadius[RAL_METAL_MAX_DIRECT_LIGHTS_PER_DRAW][4];
	float directLightColorFalloff[RAL_METAL_MAX_DIRECT_LIGHTS_PER_DRAW][4];
	float directLightEndLinear[RAL_METAL_MAX_DIRECT_LIGHTS_PER_DRAW][4];
	renderMaterialStageGpuParams_t stages[RENDER_MATERIAL_MAX_STAGES];
	/* Descriptor UV offset.xy and primitive scroll clock.z, separate from
	 * the surface/animMap clock. Matches beam.vert's scale-then-scroll rule. */
	float beamUv[4];
	/* Local normals remain vertex data; transform only the lighting normal.
	 * w marks model-space environment evaluation, zero for world/effects. */
	float modelAxes[3][4];
	float modelViewOrigin[4];
	float entityColor[4];
} ralMetalWorldMaterialParams_t;

typedef char ralMetalWorldMaterialParamsMustMatchMsl[
	sizeof( ralMetalWorldMaterialParams_t ) == 4096u ? 1 : -1];
typedef char ralMetalWorldMaterialParamsFitInlineBytes[
	sizeof( ralMetalWorldMaterialParams_t ) <= 4096u ? 1 : -1];

static void wired_clear_present_upload_caches( ralMetalPresent_t *present ) {
	present->materialUniformCache.wired_clear();
	present->entityVertexUploadCache.wired_clear();
	present->entityIndexUploadCache.wired_clear();
	present->uiVertexUploadCache.wired_clear();
	present->atmosphereUploadCache.wired_clear();
	for ( auto &cache : present->particleUploadCaches ) cache.wired_clear();
}

static void wired_reap_present_upload_readers( ralMetalPresent_t *present ) {
	present->materialUniformCache.wired_reap();
	present->entityVertexUploadCache.wired_reap();
	present->entityIndexUploadCache.wired_reap();
	present->uiVertexUploadCache.wired_reap();
	present->atmosphereUploadCache.wired_reap();
	for ( auto &cache : present->particleUploadCaches ) cache.wired_reap();
}

/* Each attempt owns its inputs until submission. Only the scene command reads
 * these packets; the output/drawable lifetime must not delay their reuse. */
struct wiredMetalParticleUploads {
	wiredMetalUpload frame, classes, spawns, integrateArguments, spawnArguments;
	wiredMetalUpload vertexArguments, fragmentArguments;
	explicit wiredMetalParticleUploads( ralMetalPresent_t *present )
		: frame( present->particleUploadCaches[0] ),
		  classes( present->particleUploadCaches[1] ),
		  spawns( present->particleUploadCaches[2] ),
		  integrateArguments( present->particleUploadCaches[3] ),
		  spawnArguments( present->particleUploadCaches[4] ),
		  vertexArguments( present->particleUploadCaches[5] ),
		  fragmentArguments( present->particleUploadCaches[6] ) {}
	void wired_publish( id<MTLCommandBuffer> lastReader ) {
		frame.wired_publish( lastReader ); classes.wired_publish( lastReader );
		spawns.wired_publish( lastReader );
		integrateArguments.wired_publish( lastReader );
		spawnArguments.wired_publish( lastReader );
		vertexArguments.wired_publish( lastReader );
		fragmentArguments.wired_publish( lastReader );
	}
};

/* One immutable packet per draw, shared by both shader stages. Reuse is only
 * allowed after native completion, not receipt publication or CPU scope exit.
 * Two entries preserve alternating in-flight packets. A full busy cache uses
 * transient storage; no wait, overwrite, or extra submission is introduced. */
struct wiredMetalDrawUniforms {
	wiredMetalUpload upload;
	NSUInteger used = 0u;
	const NSUInteger capacity;
	explicit wiredMetalDrawUniforms( ralMetalPresent_t *present, NSUInteger draws )
		: upload( present->materialUniformCache ), capacity( draws ) {}
	wiredMetalDrawUniforms( const wiredMetalDrawUniforms & ) = delete;
	bool wired_bind( id<MTLDevice> device, id<MTLRenderCommandEncoder> encoder,
			const ralMetalWorldMaterialParams_t &params,
			wiredMetalEncoderValue<id<MTLBuffer>> &binding ) {
		const NSUInteger stride = sizeof( params ); // 4096: aligned for both stages
		if ( used >= capacity || capacity > NSUIntegerMax / stride ) return false;
		if ( !upload.buffer ) {
			if ( !upload.wired_reserve( device, capacity * stride,
					MTLResourceStorageModeShared | MTLResourceCPUCacheModeWriteCombined ) ) return false;
			upload.buffer.label = @"Wired material uniforms";
		}
		const NSUInteger offset = used++ * stride;
		memcpy( (byte *)upload.buffer.contents + offset, &params, sizeof( params ) );
		wired_metal_bind_material_buffer( encoder, upload.buffer, offset, binding );
		return true;
	}
	void wired_publish_cache( id<MTLCommandBuffer> lastReader ) {
		upload.wired_publish( lastReader );
	}
};

static id<MTLBuffer> wired_upload_bytes( wiredMetalUpload &upload,
		id<MTLDevice> device, const void *bytes, NSUInteger byteCount,
		NSString *label ) {
	if ( !bytes || !upload.wired_reserve( device, byteCount,
			MTLResourceStorageModeShared ) ) return nil;
	memcpy( upload.buffer.contents, bytes, byteCount );
	upload.buffer.label = label;
	/* Existing draw-path cleanup owns this retain independently of the attempt. */
	return [upload.buffer retain];
}

static void SetDefaultMaterialFactors( ralMetalWorldMaterialParams_t *params )
{
	if ( !params ) return;
	for ( unsigned c = 0; c < 4; ++c ) params->entityColor[c] = 1.0f;
	params->materialBaseColor[0] = params->materialBaseColor[1] =
		params->materialBaseColor[2] = params->materialBaseColor[3] = 1.0f;
	params->materialFactors[0] = -1.0f;
	params->materialFactors[1] = params->materialFactors[2] =
		params->materialFactors[3] = 1.0f;
}

typedef struct {
	float eyeDensity[4];
	float colorVisibility[4];
	float heightCloud[4];
	uint32_t froxelGrid[4];
	float displayVisibility[4];
	float skyControl[4];
	/* sun direction xyz, aerial-perspective haze weight (r_atmosphereAerialHaze) */
	float aerialHaze[4];
} ralMetalWorldAtmosphereParams_t;

/* particle.frag ParticleAtmosphere (set 0, binding 5): the first four
 * vec4 of ralMetalWorldAtmosphereParams_t plus control (near plane, depth
 * convention -1 = forward-Z). Metal draws WiredFX particles after the fogged
 * scene, so the shared fragment fogs them at the opaque depth behind. */
typedef struct {
	float eyeDensity[4];
	float colorVisibility[4];
	float heightCloud[4];
	uint32_t froxelGrid[4];
	float control[4];
} ralMetalParticleAtmosphere_t;
/* Argument-buffer ids assigned by the portable translation
 * (particle_frag_spv.msl [[id(N)]]). */
#define RAL_METAL_PARTICLE_ATMOSPHERE_ID 3u
#define RAL_METAL_PARTICLE_MEDIA_ID 4u

typedef char ralMetalWorldAtmosphereParamsMustMatchMsl[
	sizeof( ralMetalWorldAtmosphereParams_t ) == 112u ? 1 : -1];

typedef struct {
	float sunDirectionAngularRadius[4];
	float sunRadiance[4];
	float moonDirectionAngularRadius[4];
	float moonRadiance[4];
	float atmosphereRadii[4];
	float scatteringScale[4];
	float groundExposure[4];
} ralMetalSkyPhysicalParams_t;

typedef char ralMetalSkyPhysicalParamsMustMatchMsl[
	sizeof( ralMetalSkyPhysicalParams_t ) == 112u ? 1 : -1];

typedef ralSkyRadianceSourceParams_t ralMetalSkySourceParams_t;

typedef struct {
	uint32_t values[4];
} ralMetalSkySliceParams_t;

typedef char ralMetalSkySourceParamsMustMatchMsl[
	sizeof( ralMetalSkySourceParams_t ) == 64u ? 1 : -1];

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
	float heightgridOriginStep[4];
	renderAtmosphereEffectGpuWorkload_t
		effectWorkloads[RENDER_SUBMISSION_MAX_ATMOSPHERE_EFFECT_WORKLOADS];
} ralMetalWeatherFrame_t;

typedef char ralMetalWeatherFrameMustMatchMsl[
	sizeof( ralMetalWeatherFrame_t ) == 1072u ? 1 : -1];

#define RAL_METAL_ATMOSPHERE_FROXEL_CAPACITY WIRED_METAL_ATMOSPHERE_CAPACITY

typedef struct {
	uint32_t firstIndex;
	uint32_t indexCount;
	qhandle_t material;
	qboolean depthHack;
	qboolean minLight;
	qboolean useVertexColor;
	qboolean primitiveBeam;
	float beamUv[4];
	float modelAxes[3][4];
	float modelViewOrigin[4];
	color4ub_t entityColor;
	qboolean visible;
	renderAlphaMode_t alphaMode;
	renderCullMode_t cullMode;
	float alphaCutoff;
	qboolean depthWrite;
	qboolean surfaceDecal;
	qboolean hasLocalIrradiance;
	qboolean legacyLightGrid;
	float localSh[4][3];
	qboolean receivesDirectLight;
	float boundsCenter[3];
	float boundsRadius;
	qboolean motion;   /* writes its object velocity into the TAA effect mask instead of invalidating the history */
} ralMetalEntityBatch_t;


#define RAL_METAL_READBACK_RING_SIZE 3u

qboolean RalMetal_PresentGpuTiming( const ralMetalPresent_t *present,
        ralMetalGpuTiming_t *outTiming ) {
    if ( !present || !outTiming ) return qfalse;
    *outTiming = present->gpuTiming;
    return qtrue;
}

static qboolean ReapReadbackRing( ralMetalPresent_t *present, qboolean waitAll ) {
	qboolean ok = qtrue;
	if ( !present ) return qfalse;
	for ( uint32_t slot = 0u; slot < RAL_METAL_READBACK_RING_SIZE; ++slot ) {
		id<MTLCommandBuffer> command = present->readbackCommands[slot];
		id<MTLCommandBuffer> scene = present->readbackSceneCommands[slot];
		MTLCommandBufferStatus status;
		if ( !command ) continue;
		if ( waitAll ) { [command waitUntilCompleted]; [scene waitUntilCompleted]; }
		status = command.status;
		if ( scene && scene.status < MTLCommandBufferStatusCompleted ) continue;
		if ( status == MTLCommandBufferStatusError ) ok = qfalse;
		if ( status == MTLCommandBufferStatusCompleted
				|| status == MTLCommandBufferStatusError ) {
            if ( scene && scene.status != MTLCommandBufferStatusCompleted ) ok = qfalse;
            if ( status == MTLCommandBufferStatusCompleted ) {
                const double start = scene ? scene.GPUStartTime : command.GPUStartTime;
                const double end = command.GPUEndTime;
                const double sceneEnd = scene ? scene.GPUEndTime : start;
                const double outputStart = command.GPUStartTime;
                /* Native buffers can overlap GPU stages even on one queue.
                 * Count the union of their execution intervals, not the idle
                 * drawable gap and not overlapping work twice. */
                const double execution = ( sceneEnd - start ) + ( end - outputStart )
                    - fmax( 0.0, fmin( sceneEnd, end ) - fmax( start, outputStart ) );
                if ( isfinite( start ) && isfinite( end ) && start > 0.0
                        && isfinite( sceneEnd ) && isfinite( outputStart )
                        && ( !scene || ( scene.status == MTLCommandBufferStatusCompleted
                            && sceneEnd > start && outputStart >= start ) )
                        && outputStart > 0.0 && end > outputStart
                        && end > start && end - start < 86400.0 ) {
                    ++present->gpuTiming.sampleCount;
                    present->gpuTiming.durationNanoseconds +=
                        (uint64_t)( execution * 1e9 + 0.5 );
                    if ( scene ) {
                        ++present->gpuTiming.splitSampleCount;
                        present->gpuTiming.sceneNanoseconds +=
                            (uint64_t)( ( sceneEnd - start ) * 1e9 + 0.5 );
                        present->gpuTiming.outputNanoseconds +=
                            (uint64_t)( ( end - outputStart ) * 1e9 + 0.5 );
                        present->gpuTiming.overlapNanoseconds +=
                            (uint64_t)( fmax( 0.0, fmin( sceneEnd, end ) - outputStart ) * 1e9 + 0.5 );
                        present->gpuTiming.gapNanoseconds +=
                            (uint64_t)( fmax( 0.0, outputStart - sceneEnd ) * 1e9 + 0.5 );
                        wiredMetalAcquirePartition partition;
                        if ( wired_metal_acquire_partition(
                                present->readbackAcquireBeginTimes[slot],
                                present->readbackAcquireEndTimes[slot], start, sceneEnd,
                                &partition ) ) {
                            ++present->gpuTiming.acquireSampleCount;
                            present->gpuTiming.acquireDurationNanoseconds += partition.duration;
                            present->gpuTiming.acquireBeforeSceneNanoseconds += partition.beforeScene;
                            present->gpuTiming.acquireDuringSceneNanoseconds += partition.duringScene;
                            present->gpuTiming.acquireAfterSceneNanoseconds += partition.afterScene;
                        } else ++present->gpuTiming.acquireUnavailableCount;
                    }
                    /* CACurrentMediaTime and Metal GPU timestamps share mach
                     * absolute seconds. The bounded readback slot retains both
                     * segments; sampling adds no handler or blocking wait. */
                    const double commit = present->readbackCommitTimes[slot];
                    if ( isfinite( commit ) && commit > 0.0 && start >= commit
                            && end - commit < 86400.0 ) {
                        ++present->gpuTiming.timelineSampleCount;
                        present->gpuTiming.commitToStartNanoseconds +=
                            (uint64_t)( ( start - commit ) * 1e9 + 0.5 );
                        present->gpuTiming.timelineDurationNanoseconds +=
                            (uint64_t)( execution * 1e9 + 0.5 );
                    } else ++present->gpuTiming.timelineUnavailableCount;
                } else {
                    ++present->gpuTiming.unavailableCount;
                    ++present->gpuTiming.timelineUnavailableCount;
                }
            }
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
			[scene release]; present->readbackSceneCommands[slot] = nil;
			present->readbackCommitTimes[slot] = 0.0;
			present->readbackAcquireBeginTimes[slot] = 0.0;
			present->readbackAcquireEndTimes[slot] = 0.0;
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
	if ( !present || !present->layer ) return qfalse;
	if ( present->uiPipeline && present->uiMsdfPipeline
			&& present->uiBackdropPipeline
			&& present->uiDepthDisabledState
			&& present->uiPixelFormat == present->layer.pixelFormat ) return qtrue;
	[present->uiPipeline release]; present->uiPipeline = nil;
	[present->uiMsdfPipeline release]; present->uiMsdfPipeline = nil;
	[present->uiBackdropPipeline release]; present->uiBackdropPipeline = nil;
	[present->uiDepthDisabledState release]; present->uiDepthDisabledState = nil;
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
		pipeline.colorAttachments[0].pixelFormat = present->layer.pixelFormat;
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
		MTLDepthStencilDescriptor *depth =
			[[[MTLDepthStencilDescriptor alloc] init] autorelease];
		depth.depthCompareFunction = MTLCompareFunctionAlways;
		depth.depthWriteEnabled = NO;
		present->uiDepthDisabledState = [device
			newDepthStencilStateWithDescriptor:depth];
	}
	[vertexFunction release]; vertexFunction = nil;
	[fragmentFunction release]; fragmentFunction = nil;
	[msdfFunction release]; msdfFunction = nil;
	[backdropFunction release]; backdropFunction = nil;
	if ( !present->uiPipeline || !present->uiMsdfPipeline
			|| !present->uiBackdropPipeline || !present->uiDepthDisabledState
			|| error ) goto fail;
	present->uiPixelFormat = present->layer.pixelFormat;
	return qtrue;
fail:
	[vertexFunction release]; [fragmentFunction release]; [msdfFunction release];
	[backdropFunction release];
	[present->uiPipeline release]; present->uiPipeline = nil;
	[present->uiMsdfPipeline release]; present->uiMsdfPipeline = nil;
	[present->uiBackdropPipeline release]; present->uiBackdropPipeline = nil;
	[present->uiDepthDisabledState release]; present->uiDepthDisabledState = nil;
	return qfalse;
}

static qboolean EnsureUiSampling( ralMetalPresent_t *present );

static qboolean EnsureLensVisibilityPipeline( ralMetalPresent_t *present )
{
	id<MTLDevice> device;
	id<MTLFunction> function = nil;
	NSError *error = nil;
	if ( !present ) return qfalse;
	if ( present->lensVisibilityPipeline ) return qtrue;
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
	function = [present->uiLibrary
		newFunctionWithName:@"wired_lens_visibility_compute"];
	if ( function )
		present->lensVisibilityPipeline = [device
			newComputePipelineStateWithFunction:function error:&error];
	[function release];
	return present->lensVisibilityPipeline && !error ? qtrue : qfalse;
}

static void CollectLensVisibility( ralMetalPresent_t *present )
{
	if ( !present ) return;
	for ( uint32_t slot = 0u; slot < RAL_METAL_LENS_ORACLE_RING; ++slot ) {
		id<MTLCommandBuffer> command = present->lensCommands[slot];
		if ( !command || command.status < MTLCommandBufferStatusCompleted ) continue;
		if ( command.status == MTLCommandBufferStatusCompleted
				&& present->lensGenerations[slot]
				>= present->lensVisibilityGeneration ) {
			const float *values = (const float *)
				[present->lensResultBuffers[slot] contents];
			if ( values ) {
				memcpy( present->lensVisibility, values,
					sizeof( present->lensVisibility ) );
				memcpy( present->lensVisibilityActive,
					present->lensActive[slot],
					sizeof( present->lensVisibilityActive ) );
				present->lensVisibilityGeneration =
					present->lensGenerations[slot];
			}
		}
		/* Keep successful, completed storage in this bounded slot. Pending
		 * commands never reach here; failed GPU work is not reused. Dropping
		 * the command still releases its drawable before the next acquisition. */
		if ( command.status != MTLCommandBufferStatusCompleted ) {
			[present->lensInputBuffers[slot] release];
			[present->lensResultBuffers[slot] release];
			present->lensInputBuffers[slot] = nil;
			present->lensResultBuffers[slot] = nil;
		}
		[command release];
		present->lensCommands[slot] = nil;
		present->lensGenerations[slot] = 0u;
		memset( present->lensActive[slot], 0,
			sizeof( present->lensActive[slot] ) );
	}
}

static void DiscardLensCommand( ralMetalPresent_t *present,
		id<MTLCommandBuffer> command )
{
	if ( !present || !command ) return;
	for ( uint32_t slot = 0u; slot < RAL_METAL_LENS_ORACLE_RING; ++slot ) {
		if ( present->lensCommands[slot] != command ) continue;
		[present->lensInputBuffers[slot] release];
		[present->lensResultBuffers[slot] release];
		[present->lensCommands[slot] release];
		present->lensInputBuffers[slot] = nil;
		present->lensResultBuffers[slot] = nil;
		present->lensCommands[slot] = nil;
		present->lensGenerations[slot] = 0u;
		memset( present->lensActive[slot], 0,
			sizeof( present->lensActive[slot] ) );
		return;
	}
}

/* Source IDs belong to one registration. Drop both published values and
 * pending readbacks at its barrier so an old map cannot republish visibility
 * for a reused ID. Native commands retain their own in-flight resources. */
static void wired_reset_lens_registration( ralMetalPresent_t *present )
{
	for ( uint32_t slot = 0u; slot < RAL_METAL_LENS_ORACLE_RING; ++slot ) {
		[present->lensInputBuffers[slot] release];
		[present->lensResultBuffers[slot] release];
		[present->lensCommands[slot] release];
		present->lensInputBuffers[slot] = nil;
		present->lensResultBuffers[slot] = nil;
		present->lensCommands[slot] = nil;
		present->lensGenerations[slot] = 0u;
		memset( present->lensActive[slot], 0, sizeof( present->lensActive[slot] ) );
	}
	memset( present->lensVisibility, 0, sizeof( present->lensVisibility ) );
	memset( present->lensVisibilityActive, 0, sizeof( present->lensVisibilityActive ) );
	present->lensVisibilityGeneration = 0u;
	present->nextLensSlot = 0u;
}

static qboolean EncodeLensVisibility( ralMetalPresent_t *present,
		id<MTLCommandBuffer> command, id<MTLTexture> worldDepth,
		const renderSubmissionState_t *frontend,
		const ralMetalWorldViewParams_t *view, uint32_t width,
		uint32_t height, float opticsVisibility, uint64_t generation,
		qboolean *outDispatched )
{
	renderLensSourceSnapshot_t snapshot;
	const lensSourceDesc_t *sources = NULL;
	ralMetalLensSourceInput_t *inputs;
	ralMetalLensOracleParams_t params;
	id<MTLComputeCommandEncoder> encoder;
	id<MTLDevice> device;
	id<MTLBuffer> inputBuffer, resultBuffer;
	uint32_t slot, packedCount;
	if ( outDispatched ) *outDispatched = qfalse;
	if ( !present || !command || !worldDepth || !frontend || !view
			|| !width || !height || !RenderSubmission_LensSourceSnapshot(
				frontend, &snapshot, &sources ) ) return qfalse;
	if ( !snapshot.count ) return qtrue;
	CollectLensVisibility( present );
	slot = present->nextLensSlot % RAL_METAL_LENS_ORACLE_RING;
	if ( present->lensCommands[slot] ) return qtrue;
	if ( !EnsureLensVisibilityPipeline( present ) ) return qfalse;
	device = RalMetal_CoreNativeDevice( present->core );
	const NSUInteger inputBytes = (NSUInteger)snapshot.count
		* sizeof( ralMetalLensSourceInput_t );
	inputBuffer = present->lensInputBuffers[slot];
	inputBuffer = inputBuffer && inputBuffer.length >= inputBytes
		? [inputBuffer retain] : [device newBufferWithLength:inputBytes
			options:MTLResourceStorageModeShared];
	resultBuffer = present->lensResultBuffers[slot];
	resultBuffer = resultBuffer ? [resultBuffer retain] : [device newBufferWithLength:
		sizeof( present->lensVisibility ) options:MTLResourceStorageModeShared];
	if ( !inputBuffer || !resultBuffer ) {
		[inputBuffer release]; [resultBuffer release]; return qfalse;
	}
	memset( [resultBuffer contents], 0, sizeof( present->lensVisibility ) );
	memset( present->lensActive[slot], 0,
		sizeof( present->lensActive[slot] ) );
	inputs = (ralMetalLensSourceInput_t *)[inputBuffer contents];
	packedCount = 0u;
	for ( uint32_t i = 0u; i < snapshot.count; ++i ) {
		const lensSourceDesc_t *source = &sources[i];
		if ( source->id < 0 || source->id >= LENS_MAX_CGSOURCES ) continue;
		memcpy( inputs[packedCount].originRadius, source->origin,
			3u * sizeof( float ) );
		inputs[packedCount].originRadius[3] = source->radius;
		memset( inputs[packedCount].meta, 0,
			sizeof( inputs[packedCount].meta ) );
		inputs[packedCount].meta[0] = (uint32_t)source->id;
		present->lensActive[slot][source->id] = qtrue;
		packedCount++;
	}
	if ( !packedCount ) {
		[inputBuffer release]; [resultBuffer release]; return qtrue;
	}
	memset( &params, 0, sizeof( params ) );
	memcpy( params.view, view, sizeof( *view ) );
	params.viewportOpticsBias[0] = (float)width;
	params.viewportOpticsBias[1] = (float)height;
	params.viewportOpticsBias[2] = fmaxf( 0.0f,
		fminf( 1.0f, opticsVisibility ) );
	memcpy( &params.viewportOpticsBias[3], &packedCount,
		sizeof( packedCount ) );
	encoder = [command computeCommandEncoder];
	if ( !encoder ) {
		[inputBuffer release]; [resultBuffer release]; return qfalse;
	}
	[encoder setComputePipelineState:present->lensVisibilityPipeline];
	[encoder setBuffer:inputBuffer offset:0u atIndex:0u];
	[encoder setBuffer:resultBuffer offset:0u atIndex:1u];
	[encoder setBytes:&params length:sizeof( params ) atIndex:2u];
	[encoder setTexture:worldDepth atIndex:0u];
	[encoder dispatchThreads:MTLSizeMake( packedCount, 1u, 1u )
		threadsPerThreadgroup:MTLSizeMake( MIN( packedCount, 64u ), 1u, 1u )];
	[encoder endEncoding];
	[present->lensInputBuffers[slot] release];
	[present->lensResultBuffers[slot] release];
	present->lensInputBuffers[slot] = inputBuffer;
	present->lensResultBuffers[slot] = resultBuffer;
	present->lensCommands[slot] = [command retain];
	present->lensGenerations[slot] = generation;
	present->nextLensSlot = ( slot + 1u ) % RAL_METAL_LENS_ORACLE_RING;
	if ( outDispatched ) *outDispatched = qtrue;
	return qtrue;
}

qboolean RalMetal_PresentLensVisibility( ralMetalPresent_t *present,
		int id, float *outVisibility )
{
	if ( !present || !outVisibility || id < 0 || id >= LENS_MAX_CGSOURCES )
		return qfalse;
	CollectLensVisibility( present );
	if ( !present->lensVisibilityGeneration
			|| !present->lensVisibilityActive[id] ) return qfalse;
	*outVisibility = fmaxf( 0.0f,
		fminf( 1.0f, present->lensVisibility[id] ) );
	return qtrue;
}

static qboolean EnsureToneMapPipeline( ralMetalPresent_t *present ) {
	id<MTLDevice> device;
	id<MTLFunction> vertexFunction = nil, fragmentFunction = nil;
	NSError *error = nil;
	MTLPixelFormat outputFormat;
	if ( !present || !present->layer || !EnsureUiSampling( present ) ) return qfalse;
	outputFormat = present->layer.pixelFormat;
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

/* Fog parameters for world-fragment draws that fog at their own surface. */
static const float s_sceneFogOff[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

static id<MTLTexture> SceneDepthOrFallback( ralMetalPresent_t *present ) {
	if ( present->sceneDepthCache ) return present->sceneDepthCache;
	if ( !present->depthFallbackTexture ) {
		MTLTextureDescriptor *descriptor = [MTLTextureDescriptor
			texture2DDescriptorWithPixelFormat:MTLPixelFormatDepth32Float
			width:1u height:1u mipmapped:NO];
		descriptor.storageMode = MTLStorageModePrivate;
		descriptor.usage = MTLTextureUsageShaderRead;
		present->depthFallbackTexture = [RalMetal_CoreNativeDevice( present->core )
			newTextureWithDescriptor:descriptor];
	}
	return present->depthFallbackTexture;
}

static qboolean EnsureSceneDepth( ralMetalPresent_t *present,
		uint32_t width, uint32_t height ) {
	id<MTLTexture> replacement;
	MTLTextureDescriptor *descriptor;
	if ( !present || !width || !height ) return qfalse;
	if ( present->sceneDepthCache && present->sceneDepthWidth == width
			&& present->sceneDepthHeight == height ) return qtrue;
	descriptor = [MTLTextureDescriptor
		texture2DDescriptorWithPixelFormat:MTLPixelFormatDepth32Float
		width:width height:height mipmapped:NO];
	descriptor.storageMode = MTLStorageModePrivate;
	descriptor.usage = MTLTextureUsageShaderRead;
	replacement = [RalMetal_CoreNativeDevice( present->core )
		newTextureWithDescriptor:descriptor];
	if ( !replacement ) return qfalse;
	[present->sceneDepthCache release];
	present->sceneDepthCache = replacement;
	present->sceneDepthWidth = width;
	present->sceneDepthHeight = height;
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
		clamp.mipFilter = MTLSamplerMipFilterLinear;
		clamp.sAddressMode = MTLSamplerAddressModeClampToEdge;
		clamp.tAddressMode = MTLSamplerAddressModeClampToEdge;
		clamp.supportArgumentBuffers = YES;
		MTLSamplerDescriptor *repeat = [[[MTLSamplerDescriptor alloc] init] autorelease];
		repeat.minFilter = MTLSamplerMinMagFilterLinear;
		repeat.magFilter = MTLSamplerMinMagFilterLinear;
		repeat.mipFilter = MTLSamplerMipFilterLinear;
		repeat.sAddressMode = MTLSamplerAddressModeRepeat;
		repeat.tAddressMode = MTLSamplerAddressModeRepeat;
		repeat.supportArgumentBuffers = YES;
		if ( !present->uiClampSampler )
			present->uiClampSampler = [device newSamplerStateWithDescriptor:clamp];
		if ( !present->uiRepeatSampler )
			present->uiRepeatSampler = [device newSamplerStateWithDescriptor:repeat];
		if ( !present->uiClampSampler || !present->uiRepeatSampler ) return qfalse;
	}
	return qtrue;
}

static uint32_t MetalTextureMipLevelCount( uint32_t width, uint32_t height,
		uint32_t requested ) {
	uint32_t extent = width > height ? width : height;
	uint32_t maximum = 1u;
	while ( extent > 1u ) {
		extent >>= 1u;
		maximum++;
	}
	if ( requested < 1u ) requested = 1u;
	return requested < maximum ? requested : maximum;
}

static qhandle_t MaterialStageHandleAtTime(
		const renderMaterialStageSnapshot_t *stage, float timeSeconds ) {
	uint32_t frame;
	if ( !stage || !stage->animationFrameCount
			|| !isfinite( stage->animationFrequency )
			|| stage->animationFrequency <= 0.0f )
		return stage ? stage->material : 0;
	frame = (uint32_t)fmaxf( floorf( fmaxf( timeSeconds, 0.0f )
		* stage->animationFrequency ), 0.0f );
	if ( stage->animationLooping ) frame %= stage->animationFrameCount;
	else if ( frame >= stage->animationFrameCount )
		frame = stage->animationFrameCount - 1u;
	return stage->animationMaterials[frame] > 0
		? stage->animationMaterials[frame] : stage->material;
}

static qboolean UiMaterial( ralMetalPresent_t *present,
		const renderSubmissionState_t *frontend, qhandle_t handle,
		id<MTLTexture> *outTexture, id<MTLSamplerState> *outSampler,
		qboolean *outTextured, qboolean *outMsdf ) {
	renderTextureSnapshot_t snapshot;
	ralMetalMaterialCacheEntry_t *entry = NULL;
	uint32_t i;
	if ( !present || !outTexture || !outSampler || !outTextured || !outMsdf
			|| !EnsureUiSampling( present ) ) return qfalse;
	*outTexture = present->uiFallbackTexture;
	*outSampler = present->uiClampSampler;
	*outTextured = qfalse;
	*outMsdf = qfalse;
	if ( !frontend || !RenderSubmission_TextureSnapshot( frontend, handle,
			&snapshot ) || snapshot.ready != qtrue ) return qtrue;
	if ( !snapshot.rgba8 || !snapshot.width || !snapshot.height
			|| snapshot.rowBytes != snapshot.width * 4u
			|| (uint64_t)snapshot.byteCount
				!= (uint64_t)snapshot.rowBytes * snapshot.height
			|| snapshot.generation == 0u || snapshot.digest == 0u ) return qfalse;
	for ( i = 0u; i < RENDER_SUBMISSION_MAX_TEXTURE_RESOURCES; ++i ) {
		ralMetalMaterialCacheEntry_t *candidate = &present->uiMaterials[
			( (uint32_t)handle + i ) % RENDER_SUBMISSION_MAX_TEXTURE_RESOURCES];
		if ( candidate->handle == handle ) { entry = candidate; break; }
		if ( candidate->handle == 0 ) { entry = candidate; break; }
	}
	if ( entry && ( entry->generation != snapshot.generation
			|| entry->digest != snapshot.digest ) && entry->handle == handle
			&& !RenderSubmission_TextureResourceMatches( &snapshot,
				&entry->runtimeResource ) ) {
		[entry->texture release]; entry->texture = nil;
		entry->generation = 0u; entry->digest = 0u;
		memset( &entry->resource, 0, sizeof( entry->resource ) );
		memset( &entry->runtimeResource, 0, sizeof( entry->runtimeResource ) );
	}
	if ( !entry ) return qfalse;
	if ( entry->handle == 0 ) {
		if ( present->uiMaterialCount >= RENDER_SUBMISSION_MAX_TEXTURE_RESOURCES ) return qfalse;
		entry->handle = handle; present->uiMaterialCount++;
	}
	if ( !entry->texture && snapshot.textureCacheKeyReady ) {
		/* Material/sampler identity is not image ownership. Each handle retains
		 * the canonical image, so invalidating one material cannot retire a
		 * sibling's live resource. Existing teardown releases every retain. */
		for ( i = 0u; i < RENDER_SUBMISSION_MAX_TEXTURE_RESOURCES; ++i ) {
			const ralMetalMaterialCacheEntry_t *shared = &present->uiMaterials[i];
			if ( shared == entry || !shared->texture
					|| !RenderSubmission_TextureResourceMatches( &snapshot,
						&shared->runtimeResource ) ) continue;
			entry->texture = [shared->texture retain];
			entry->resource = shared->resource;
			entry->runtimeResource = shared->runtimeResource;
			break;
		}
	}
	if ( !entry->texture ) {
		const uint32_t requestedMipLevels = snapshot.textureCacheKeyReady
			? snapshot.textureCacheKey.asset.targetMipLevels : 1u;
		const uint32_t mipLevels = MetalTextureMipLevelCount( snapshot.width,
			snapshot.height, requestedMipLevels );
		MTLTextureDescriptor *descriptor = [MTLTextureDescriptor
			texture2DDescriptorWithPixelFormat:snapshot.srgb
				? MTLPixelFormatRGBA8Unorm_sRGB : MTLPixelFormatRGBA8Unorm
			width:snapshot.width height:snapshot.height
			mipmapped:mipLevels > 1u ? YES : NO];
		descriptor.mipmapLevelCount = mipLevels;
		descriptor.storageMode = MTLStorageModeShared;
		descriptor.usage = MTLTextureUsageShaderRead;
		entry->texture = [RalMetal_CoreNativeDevice( present->core )
			newTextureWithDescriptor:descriptor];
		if ( !entry->texture ) return qfalse;
		[entry->texture replaceRegion:MTLRegionMake2D( 0u, 0u, snapshot.width,
			snapshot.height ) mipmapLevel:0u withBytes:snapshot.rgba8
			bytesPerRow:snapshot.rowBytes];
		if ( mipLevels > 1u ) {
			id<MTLCommandBuffer> command = [RalMetal_CoreNativeQueue( present->core )
				commandBuffer];
			id<MTLBlitCommandEncoder> blit = command ? [command blitCommandEncoder]
				: nil;
			if ( !command || !blit ) {
				[entry->texture release]; entry->texture = nil; return qfalse;
			}
			[blit generateMipmapsForTexture:entry->texture];
			[blit endEncoding]; [command commit]; [command waitUntilCompleted];
			if ( command.status != MTLCommandBufferStatusCompleted ) {
				[entry->texture release]; entry->texture = nil; return qfalse;
			}
		}
		memset( &entry->resource, 0, sizeof( entry->resource ) );
		entry->resource.schemaVersion = RAL_TEXTURE_RESOURCE_RECEIPT_SCHEMA_VERSION;
		entry->resource.backendType = RAL_BACKEND_METAL;
		entry->resource.textureIdentity = (uintptr_t)(void *)entry->texture;
		entry->resource.resourceGeneration = snapshot.generation;
		entry->resource.type = RAL_TEXTURE_2D;
		entry->resource.format = snapshot.srgb ? RAL_FORMAT_R8G8B8A8_SRGB
			: RAL_FORMAT_R8G8B8A8_UNORM;
		entry->resource.usage = (ralTextureUsage_t)( RAL_TEXTURE_USAGE_SAMPLED
			| RAL_TEXTURE_USAGE_TRANSFER_DST );
		entry->resource.width = snapshot.width;
		entry->resource.height = snapshot.height;
		entry->resource.mipLevels = mipLevels;
		entry->resource.arrayLayers = 1u;
		entry->resource.imported = qfalse;
		entry->resource.ready = qtrue;
		if ( !Ral_TextureResourceReceiptValid( &entry->resource )
				|| ( snapshot.textureCacheKeyReady
					&& !Ral_MaterialRuntimeResourceBuild(
						&snapshot.textureCacheKey, &entry->resource,
						&entry->runtimeResource ) ) ) {
			[entry->texture release]; entry->texture = nil;
			memset( &entry->resource, 0, sizeof( entry->resource ) );
			memset( &entry->runtimeResource, 0, sizeof( entry->runtimeResource ) );
			return qfalse;
		}
		entry->generation = snapshot.generation;
		entry->digest = snapshot.digest;
	}
	/* A validated factor-only revision keeps the native image and restores
	 * the cheap generation/digest cache hit on subsequent frames. */
	entry->generation = snapshot.generation;
	entry->digest = snapshot.digest;
	if ( RenderSubmission_MaterialIndex( frontend, handle, &i )
			&& present->cookedMaterialReady[i] ) {
		if ( present->cookedMaterialTextureIdentities[i]
				!= (uintptr_t)(void *)entry->texture ) return qfalse;
		*outTexture = (id<MTLTexture>)(void *)
			present->cookedMaterialTextureIdentities[i];
	} else {
		*outTexture = entry->texture;
	}
	*outSampler = snapshot.clampToEdge ? present->uiClampSampler
		: present->uiRepeatSampler;
	if ( entry->texture.mipmapLevelCount > 1u && !snapshot.msdf
			&& present->materialClampSampler )
		*outSampler = snapshot.clampToEdge ? present->materialClampSampler
			: present->materialRepeatSampler;
	*outTextured = qtrue;
	*outMsdf = snapshot.msdf;
	return qtrue;
}

qboolean RalMetal_PresentPrepareMaterialResources(
		ralMetalPresent_t *present, const renderSubmissionState_t *frontend ) {
	if ( !present || !frontend ) return qfalse;
	for ( uint32_t i = 0u; i < frontend->materialCount; ++i ) {
		const qhandle_t handle = frontend->materials[i].snapshot.handle;
		id<MTLTexture> texture = nil;
		id<MTLSamplerState> sampler = nil;
		qboolean textured = qfalse, msdf = qfalse;
		if ( handle <= 0 || !UiMaterial( present, frontend, handle,
				&texture, &sampler, &textured, &msdf ) ) return qfalse;
		(void)texture; (void)sampler; (void)textured; (void)msdf;
	}
	for ( uint32_t i = 0u; i < frontend->stageTextureCount; ++i ) {
		const qhandle_t handle = frontend->stageTextures[i].snapshot.handle;
		id<MTLTexture> texture = nil;
		id<MTLSamplerState> sampler = nil;
		qboolean textured = qfalse, msdf = qfalse;
		if ( handle <= 0 || !UiMaterial( present, frontend, handle,
				&texture, &sampler, &textured, &msdf ) ) return qfalse;
		(void)texture; (void)sampler; (void)textured; (void)msdf;
	}
	return qtrue;
}

qboolean RalMetal_PresentResolveMaterialRuntimeResource( void *opaque,
		uint64_t cacheKeyHash, ralMaterialRuntimeResource_t *outResource ) {
	const ralMetalPresent_t *present = (const ralMetalPresent_t *)opaque;
	const ralMaterialRuntimeResource_t *match = NULL;
	if ( !present || !cacheKeyHash || !outResource ) return qfalse;
	for ( uint32_t i = 0u; i < RENDER_SUBMISSION_MAX_TEXTURE_RESOURCES; ++i ) {
		const ralMaterialRuntimeResource_t *resource =
			&present->uiMaterials[i].runtimeResource;
		if ( resource->cacheKeyHash != cacheKeyHash ) continue;
		if ( match && Ral_TextureResourceReceiptExact(
				&match->resource, &resource->resource ) ) continue;
		if ( match || !Ral_TextureResourceReceiptValid( &resource->resource ) ) {
			fprintf( stderr, "Wired native Metal RAL: material resource rejected key=%llu reason=%s handle=%d\n",
				(unsigned long long)cacheKeyHash, match ? "duplicate" : "invalid",
				present->uiMaterials[i].handle );
			return qfalse;
		}
		match = resource;
	}
	if ( !match ) {
		fprintf( stderr, "Wired native Metal RAL: material resource rejected key=%llu reason=missing\n",
			(unsigned long long)cacheKeyHash );
		return qfalse;
	}
	*outResource = *match;
	return qtrue;
}

qboolean RalMetal_PresentSetMaterialRuntimeCatalog(
		ralMetalPresent_t *present,
		const renderMaterialRuntimeCatalog_t *catalog,
		renderSubmissionState_t *frontend ) {
	renderWorldSnapshot_t world;
	uintptr_t candidateTextureIdentities[RENDER_SUBMISSION_MAX_MATERIALS];
	qboolean candidateReady[RENDER_SUBMISSION_MAX_MATERIALS];
	renderMaterialFactorPublicationEntry_t factorEntries[
		RENDER_SUBMISSION_MAX_MATERIALS];
	uint32_t factorEntryCount = 0u;
	if ( !present ) return qfalse;
	if ( !catalog ) {
		if ( frontend && !RenderSubmission_PublishMaterialFactors(
				frontend, 0u, NULL, 0u ) ) return qfalse;
		memset( present->cookedMaterialTextureIdentities, 0,
			sizeof( present->cookedMaterialTextureIdentities ) );
		memset( present->cookedMaterialReady, 0,
			sizeof( present->cookedMaterialReady ) );
		present->cookedMaterialRuntimeGeneration = 0u;
		return qtrue;
	}
	if ( !frontend || catalog->ready != qtrue || !catalog->runtimeGeneration
			|| !RalMetal_PresentPrepareMaterialResources( present, frontend )
			|| !RenderSubmission_ResidentWorldSnapshot( frontend,
				frontend->activeWorldIndex, catalog->runtimeGeneration, &world ) ) return qfalse;
	memset( candidateTextureIdentities, 0,
		sizeof( candidateTextureIdentities ) );
	memset( candidateReady, 0, sizeof( candidateReady ) );
	for ( uint32_t i = 0u; i < frontend->materialCount; ++i ) {
		const renderMaterialRecord_t *record = &frontend->materials[i];
		const ralMaterialRuntimePlan_t *plan;
		ralMetalMaterialCacheEntry_t *entry = NULL;
		ralTextureResourceReceipt_t resource;
		plan = RenderMaterialRuntimeCatalog_Lookup( catalog, record->name );
		if ( !plan ) continue;
		factorEntries[factorEntryCount].material = record->snapshot.handle;
		factorEntries[factorEntryCount++].factors = plan->material.material.factors;
		/* Pure declarations (e.g. fog volumes) have no sampled image. Keep
		 * their factors, but do not invent texture readiness for draw batches. */
		if ( plan->bindingCount == 0u && plan->dependencyCount == 0u ) continue;
		if ( record->snapshot.textureCacheKeyReady != qtrue ) {
			fprintf( stderr, "Wired native Metal RAL: catalog material=%s reason=texture-key-unready bindings=%u\n",
				record->name, plan->bindingCount );
			return qfalse;
		}
		for ( uint32_t probe = 0u; probe < RENDER_SUBMISSION_MAX_TEXTURE_RESOURCES;
				++probe ) {
			ralMetalMaterialCacheEntry_t *candidate = &present->uiMaterials[
				( (uint32_t)record->snapshot.handle + probe )
					% RENDER_SUBMISSION_MAX_TEXTURE_RESOURCES];
			if ( candidate->handle == record->snapshot.handle ) {
				entry = candidate; break;
			}
			if ( candidate->handle == 0 ) break;
		}
		if ( !entry || !entry->texture
				|| !RenderMaterialRuntimeCatalog_ResolveTexture( catalog,
					record->name, record->snapshot.textureCacheKeyHash, &resource )
				|| resource.backendType != RAL_BACKEND_METAL
				|| resource.textureIdentity
					!= (uintptr_t)(void *)entry->texture ) {
			fprintf( stderr, "Wired native Metal RAL: catalog material=%s reason=texture-binding key=%llu\n",
				record->name, (unsigned long long)record->snapshot.textureCacheKeyHash );
			return qfalse;
		}
		candidateTextureIdentities[i] = resource.textureIdentity;
		candidateReady[i] = qtrue;
	}
	for ( uint32_t batch = 0u; batch < world.batchCount; ++batch ) {
		uint32_t index;
		if ( world.batches[batch].noDraw ) continue;
		if ( !RenderSubmission_MaterialIndex( frontend,
				world.batches[batch].baseMaterial, &index ) || !candidateReady[index] ) {
			fprintf( stderr, "Wired native Metal RAL: catalog batch=%u material=%d reason=unpublished\n",
				batch, world.batches[batch].baseMaterial );
			return qfalse;
		}
	}
	if ( !RenderSubmission_PublishMaterialFactors( frontend,
		catalog->runtimeGeneration, factorEntries, factorEntryCount ) ) return qfalse;
	memcpy( present->cookedMaterialTextureIdentities,
		candidateTextureIdentities, sizeof( candidateTextureIdentities ) );
	memcpy( present->cookedMaterialReady, candidateReady,
		sizeof( candidateReady ) );
	present->cookedMaterialRuntimeGeneration = catalog->runtimeGeneration;
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
	[encoder setDepthStencilState:present->uiDepthDisabledState];
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

static qboolean BuildWorldVertices( const renderWorldSnapshot_t *snapshot,
		ralMetalWorldVertex_t **outVertices, const uint32_t **outIndices,
		const renderWorldBatch_t **outBatches, uint32_t *outVertexCount,
		uint32_t *outIndexCount, uint32_t *outBatchCount ) {
	ralMetalWorldVertex_t *vertices;
	if ( !outVertices || !outIndices || !outBatches || !outVertexCount
			|| !outIndexCount || !outBatchCount ) return qfalse;
	*outVertices = NULL; *outIndices = NULL; *outBatches = NULL;
	*outVertexCount = *outIndexCount = *outBatchCount = 0u;
	if ( !snapshot ) return qtrue;
	if ( !snapshot->vertices || !snapshot->indices || !snapshot->batches
			|| !snapshot->vertexCount || !snapshot->indexCount || !snapshot->batchCount
			|| snapshot->indexCount % 3u
			|| snapshot->vertexCount > RENDER_SUBMISSION_MAX_WORLD_VERTICES
			|| snapshot->indexCount > RENDER_SUBMISSION_MAX_WORLD_INDICES
			|| snapshot->batchCount > RENDER_SUBMISSION_MAX_WORLD_BATCHES
			|| !isfinite( snapshot->fovX ) || !isfinite( snapshot->fovY )
			|| snapshot->fovX <= 1.0f || snapshot->fovX >= 179.0f
			|| snapshot->fovY <= 1.0f || snapshot->fovY >= 179.0f ) return qfalse;
	vertices = (ralMetalWorldVertex_t *)calloc( snapshot->vertexCount,
		sizeof( *vertices ) );
	if ( !vertices ) return qfalse;
	for ( uint32_t i = 0u; i < snapshot->vertexCount; ++i ) {
		memcpy( vertices[i].worldPosition, snapshot->vertices[i].position,
			3u * sizeof( float ) );
		vertices[i].worldPosition[3] = 1.0f;
		memcpy( vertices[i].materialPosition, vertices[i].worldPosition,
			sizeof( vertices[i].materialPosition ) );
		for ( uint32_t channel = 0u; channel < 4u; ++channel )
			vertices[i].color[channel] = snapshot->vertices[i].color[channel] / 255.0f;
		memcpy( vertices[i].texCoord, snapshot->vertices[i].texCoord,
			sizeof( vertices[i].texCoord ) );
		memcpy( vertices[i].lightmapCoord, snapshot->vertices[i].lightmapCoord,
			sizeof( vertices[i].lightmapCoord ) );
		memcpy( vertices[i].normal, snapshot->vertices[i].normal,
			3u * sizeof( float ) );
	}
	for ( uint32_t batch = 0u; batch < snapshot->batchCount; ++batch ) {
		const renderWorldBatch_t *draw = &snapshot->batches[batch];
		if ( !draw->indexCount || draw->indexCount % 3u
				|| draw->firstIndex > snapshot->indexCount - draw->indexCount
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
	*outVertices = vertices; *outIndices = snapshot->indices;
	*outBatches = snapshot->batches;
	*outVertexCount = snapshot->vertexCount; *outIndexCount = snapshot->indexCount;
	*outBatchCount = snapshot->batchCount;
	return qtrue;
}

static qboolean PrepareWorldSnapshotBuffers( ralMetalPresent_t *present,
		const renderWorldSnapshot_t *snapshot, int worldIndex, uint64_t digest,
		id<MTLBuffer> *outVertices,
		id<MTLBuffer> *outIndices, const renderWorldBatch_t **outBatches,
		uint32_t *outVertexCount, uint32_t *outIndexCount,
		uint32_t *outBatchCount ) {
	ralMetalWorldVertex_t *vertexBytes = NULL;
	const uint32_t *indexBytes = NULL;
	const renderWorldBatch_t *batches = NULL;
	id<MTLBuffer> vertices = nil, indices = nil;
	ralMetalWorldCacheSlot_t *cache;
	float *bounds = NULL;
	uint32_t vertexCount = 0u, indexCount = 0u, batchCount = 0u;
	if ( !present || !outVertices || !outIndices || !outBatches
			|| !outVertexCount || !outIndexCount || !outBatchCount ) return qfalse;
	*outVertices = nil; *outIndices = nil; *outBatches = NULL;
	*outVertexCount = *outIndexCount = *outBatchCount = 0u;
	if ( !snapshot || ( snapshot->rdflags & RDF_NOWORLDMODEL ) ) return qtrue;
	if ( worldIndex < 0 || worldIndex >= MAX_RENDER_WORLDS || !digest ) return qfalse;
	cache = &present->worldCaches[worldIndex];
	if ( !snapshot->vertices || !snapshot->indices || !snapshot->batches
			|| !snapshot->vertexCount || !snapshot->indexCount || !snapshot->batchCount )
		return qfalse;
	if ( cache->digest != digest
			|| cache->vertexCount != snapshot->vertexCount
			|| cache->indexCount != snapshot->indexCount
			|| !cache->vertices || !cache->indices || !cache->batchBounds ) {
		if ( !BuildWorldVertices( snapshot, &vertexBytes, &indexBytes, &batches,
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
				if ( vertex >= snapshot->vertexCount ) {
					free( bounds ); [vertices release]; [indices release]; return qfalse;
				}
				for ( uint32_t axis = 0u; axis < 3u; ++axis ) {
					float value = snapshot->vertices[vertex].position[axis];
					if ( value < box[axis] ) box[axis] = value;
					if ( value > box[axis + 3u] ) box[axis + 3u] = value;
				}
			}
		}
		[cache->vertices release];
		[cache->indices release];
		free( cache->batchBounds );
		cache->vertices = vertices;
		cache->indices = indices;
		cache->batchBounds = bounds;
		cache->digest = digest;
		cache->vertexCount = vertexCount;
		cache->indexCount = indexCount;
	} else {
		vertexCount = snapshot->vertexCount;
		indexCount = snapshot->indexCount;
		batchCount = snapshot->batchCount;
		batches = snapshot->batches;
	}
	*outVertices = [cache->vertices retain];
	*outIndices = [cache->indices retain];
	*outBatches = batches;
	*outVertexCount = vertexCount; *outIndexCount = indexCount;
	*outBatchCount = batchCount;
	return qtrue;
}

static qboolean PrepareWorldBuffers( ralMetalPresent_t *present,
		const renderSubmissionState_t *frontend, id<MTLBuffer> *outVertices,
		id<MTLBuffer> *outIndices, const renderWorldBatch_t **outBatches,
		uint32_t *outVertexCount, uint32_t *outIndexCount,
		uint32_t *outBatchCount ) {
	renderWorldSnapshot_t snapshot;
	if ( !frontend ) return PrepareWorldSnapshotBuffers( present, NULL, -1, 0u,
		outVertices, outIndices, outBatches, outVertexCount, outIndexCount,
		outBatchCount );
	if ( !RenderSubmission_WorldSnapshot( frontend, &snapshot ) ) {
		*outVertices = nil; *outIndices = nil; *outBatches = NULL;
		*outVertexCount = *outIndexCount = *outBatchCount = 0u;
		return qtrue;
	}
	return PrepareWorldSnapshotBuffers( present, &snapshot,
		frontend->activeWorldIndex, frontend->worldDigest, outVertices,
		outIndices, outBatches, outVertexCount, outIndexCount, outBatchCount );
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

static qboolean DistantWorldBatchSelected(
		const ralSkyDistantDrawPlanReceipt_t *plan, uint32_t batchIndex ) {
	if ( !plan ) return qfalse;
	for ( uint32_t selection = 0u; selection < plan->admittedCount; ++selection )
		if ( plan->selections[selection].sourceDrawIndex == batchIndex )
			return qtrue;
	return qfalse;
}

static void EntityVertex( ralMetalWorldVertex_t *vertex,
		const renderWorldSnapshot_t *view, const float world[3], float s, float t,
		const color4ub_t color ) {
	(void)view;
	memcpy( vertex->worldPosition, world, 3u * sizeof( float ) );
	vertex->worldPosition[3] = 1.0f;
	memcpy( vertex->materialPosition, vertex->worldPosition,
		sizeof( vertex->materialPosition ) );
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

static qboolean BuildWorldViewParamsFromSnapshot(
		const renderWorldSnapshot_t *view,
		ralMetalWorldViewParams_t *outParams,
		renderWorldSnapshot_t *outView ) {
	float tanX, tanY;
	if ( !view || !outParams || !isfinite( view->fovX )
			|| !isfinite( view->fovY ) || view->fovX <= 1.0f
			|| view->fovX >= 179.0f || view->fovY <= 1.0f
			|| view->fovY >= 179.0f ) return qfalse;
	tanX = tanf( view->fovX * 0.00872664625997164788f );
	tanY = tanf( view->fovY * 0.00872664625997164788f );
	memset( outParams, 0, sizeof( *outParams ) );
	memcpy( outParams->viewOrigin, view->viewOrigin, 3u * sizeof( float ) );
	memcpy( outParams->forward, view->viewAxis[0], 3u * sizeof( float ) );
	memcpy( outParams->left, view->viewAxis[1], 3u * sizeof( float ) );
	memcpy( outParams->up, view->viewAxis[2], 3u * sizeof( float ) );
	outParams->projection[0] = 1.0f / tanX;
	outParams->projection[1] = 1.0f / tanY;
	outParams->projection[2] = 4.0f;
	if ( outView ) *outView = *view;
	return qtrue;
}

static qboolean BuildWorldViewParams( const renderSubmissionState_t *frontend,
		ralMetalWorldViewParams_t *outParams,
		renderWorldSnapshot_t *outView ) {
	renderWorldSnapshot_t view;
	/* UI-owned entity previews use RDF_NOWORLDMODEL: they have a valid view
	 * receipt but intentionally do not publish a BSP world receipt. */
	if ( !frontend || !RenderSubmission_ViewSnapshot( frontend, &view ) )
		return qfalse;
	return BuildWorldViewParamsFromSnapshot( &view, outParams, outView );
}

static qboolean BuildWorldAtmosphereParams(
		const renderSubmissionState_t *frontend,
		const ralMetalWorldViewParams_t *view,
		const ralAtmospherePlanReceipt_t *plan,
		const ralDisplayVisibilityPlan_t *visibility,
		qboolean physicalSkyReadable,
		const ralSkyRadianceConsumerReceipt_t *radianceConsumer,
		float aerialHaze,
		ralMetalWorldAtmosphereParams_t *outParams ) {
	renderAtmosphereSnapshot_t snapshot;
	ralSkySurfaceReceipt_t skySurface;
	const atmosphereEmitter_t *emitters;
	float cloudTransmission, lightning;
	if ( !frontend || !view || !plan || !outParams
			|| !Ral_DisplayVisibilityPlanValid( visibility )
			|| !Ral_SkySurfacePlan( &plan->skyEnvironment,
				physicalSkyReadable, &skySurface ) ) return qfalse;
	memset( outParams, 0, sizeof( *outParams ) );
	memcpy( outParams->eyeDensity, view->viewOrigin, 3u * sizeof( float ) );
	outParams->displayVisibility[0] = visibility->exposureScale;
	outParams->displayVisibility[1] = visibility->shadowExponent;
	outParams->displayVisibility[2] = visibility->shadowPivot;
	outParams->displayVisibility[3] = visibility->userBrightness;
	outParams->skyControl[0] = skySurface.physicalBlend;
	if ( radianceConsumer && radianceConsumer->ready ) {
		if ( !Ral_SkyRadianceConsumerReceiptExact( radianceConsumer,
				radianceConsumer ) ) return qfalse;
		/* Surface IBL consumes this cohort in every mode.  Visible sky
		 * composition preserves the rasterized authored stage program. */
		outParams->skyControl[1] = 0.0f;
		outParams->skyControl[2] =
			radianceConsumer->previousToCurrentBlend;
		outParams->skyControl[3] =
			radianceConsumer->usePreviousProduct ? 1.0f : 0.0f;
	}
	if ( !RenderSubmission_AtmosphereSnapshot( frontend, &snapshot,
			&emitters ) ) return qfalse;
	(void)emitters;
	if ( !snapshot.active
			|| snapshot.state.qualityTier < ATMOSPHERE_QUALITY_ANALYTIC )
		return qtrue;
	outParams->eyeDensity[3] = snapshot.state.mediaDensity;
	outParams->colorVisibility[3] = snapshot.state.visibility;
	cloudTransmission = 1.0f - snapshot.state.cloudCover
		* plan->skyEnvironment.cloudSunOcclusion;
	lightning = snapshot.state.lightning;
	for ( uint32_t channel = 0u; channel < 3u; ++channel )
		outParams->colorVisibility[channel] = fminf( 16.0f,
			plan->skyEnvironment.ambientSh[channel]
			+ 0.08f * plan->skyEnvironment.sunRadiance[channel]
				* cloudTransmission + lightning );
	outParams->heightCloud[0] = snapshot.state.bounds[2];
	outParams->heightCloud[1] = snapshot.state.mediaHeightFalloff;
	outParams->heightCloud[2] = snapshot.state.cloudCover;
	outParams->heightCloud[3] = plan->skyEnvironment.cloudSunOcclusion;
	/* Aerial-perspective haze (analytic media only; the froxel path integrates
	 * sky lighting itself). Same fill as the Vulkan product composite. */
	if ( aerialHaze > 0.0f && plan->computeDispatchCount == 0u
			&& radianceConsumer && radianceConsumer->ready ) {
		memcpy( outParams->aerialHaze, plan->skyEnvironment.sunDirection,
			3u * sizeof( float ) );
		outParams->aerialHaze[3] = fminf( 1.0f, aerialHaze );
	}
	if ( plan->computeDispatchCount != 0u ) {
		outParams->colorVisibility[3] = RalMetal_AtmosphereFarDistance( &snapshot.state );
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
	memset( batch->entityColor.rgba, 255, sizeof( batch->entityColor.rgba ) );
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
	batch->legacyLightGrid = command->legacyLightGrid;
	for ( uint32_t coefficient = 0u; coefficient < 4u; ++coefficient )
		for ( uint32_t channel = 0u; channel < 3u; ++channel )
			batch->localSh[coefficient][channel] =
				(float)command->localIrradiance
					.blendedCoefficientsQ16[coefficient][channel]
					/ (float)RAL_LIGHT_Q16_ONE;
}

static void EntityBatchBounds( const ralMetalWorldVertex_t *vertices,
		uint32_t firstVertex, uint32_t vertexCount,
		ralMetalEntityBatch_t *batch ) {
	float minimum[3], maximum[3], radiusSquared = 0.0f;
	if ( !vertices || !vertexCount || !batch ) return;
	memcpy( minimum, vertices[firstVertex].worldPosition, sizeof( minimum ) );
	memcpy( maximum, vertices[firstVertex].worldPosition, sizeof( maximum ) );
	for ( uint32_t vertex = 1u; vertex < vertexCount; ++vertex )
		for ( uint32_t axis = 0u; axis < 3u; ++axis ) {
			const float value = vertices[firstVertex + vertex].worldPosition[axis];
			minimum[axis] = fminf( minimum[axis], value );
			maximum[axis] = fmaxf( maximum[axis], value );
		}
	for ( uint32_t axis = 0u; axis < 3u; ++axis )
		batch->boundsCenter[axis] = 0.5f * ( minimum[axis] + maximum[axis] );
	for ( uint32_t vertex = 0u; vertex < vertexCount; ++vertex ) {
		float distanceSquared = 0.0f;
		for ( uint32_t axis = 0u; axis < 3u; ++axis ) {
			const float delta = vertices[firstVertex + vertex].worldPosition[axis]
				- batch->boundsCenter[axis];
			distanceSquared += delta * delta;
		}
		radiusSquared = fmaxf( radiusSquared, distanceSquared );
	}
	batch->boundsRadius = sqrtf( radiusSquared );
	batch->receivesDirectLight = qtrue;
}

static float MetalSrgbChannelToLinear( float value ) {
	value = fmaxf( value, 0.0f );
	return value <= 0.04045f ? value / 12.92f
		: powf( ( value + 0.055f ) / 1.055f, 2.4f );
}

static float DirectLightDistanceSquared( const renderLightCommand_t *light,
		const float point[3], float closest[3] ) {
	float segment[3], relative[3], denominator = 0.0f, projection = 0.0f;
	for ( uint32_t axis = 0u; axis < 3u; ++axis ) {
		segment[axis] = light->end[axis] - light->origin[axis];
		relative[axis] = point[axis] - light->origin[axis];
		denominator += segment[axis] * segment[axis];
		projection += relative[axis] * segment[axis];
	}
	projection = light->hasEnd && denominator > 0.000001f
		? fminf( 1.0f, fmaxf( 0.0f, projection / denominator ) ) : 0.0f;
	float distanceSquared = 0.0f;
	for ( uint32_t axis = 0u; axis < 3u; ++axis ) {
		closest[axis] = light->origin[axis] + segment[axis] * projection;
		const float delta = point[axis] - closest[axis];
		distanceSquared += delta * delta;
	}
	return distanceSquared;
}

static uint32_t PopulateDirectLights( const renderSubmissionState_t *frontend,
		const float boundsCenter[3], float boundsRadius,
		ralMetalWorldMaterialParams_t *params ) {
	const renderPolyCommand_t *polygons;
	const polyVert_t *vertices;
	const renderLightCommand_t *lights;
	uint32_t polygonCount, vertexCount, lightCount;
	float selectedScore[RAL_METAL_MAX_DIRECT_LIGHTS_PER_DRAW] = { 0.0f };
	const renderLightCommand_t *selected[RAL_METAL_MAX_DIRECT_LIGHTS_PER_DRAW]
		= { NULL };
	if ( !frontend || !boundsCenter || !params
			|| !RenderSubmission_EffectSnapshots( frontend, &polygons,
				&polygonCount, &vertices, &vertexCount, &lights, &lightCount ) )
		return 0u;
	(void)polygons; (void)polygonCount; (void)vertices; (void)vertexCount;
	for ( uint32_t lightIndex = 0u; lightIndex < lightCount; ++lightIndex ) {
		const renderLightCommand_t *light = &lights[lightIndex];
		float closest[3], distanceSquared, reach, luminance, score;
		uint32_t insertion = RAL_METAL_MAX_DIRECT_LIGHTS_PER_DRAW;
		if ( !( light->sourceFlags & RAL_LIGHT_CONTRIBUTE_DIRECT )
				|| !isfinite( light->intensity ) || light->intensity <= 0.0f )
			continue;
		distanceSquared = DirectLightDistanceSquared( light, boundsCenter, closest );
		reach = light->intensity + fmaxf( boundsRadius, 0.0f );
		if ( distanceSquared > reach * reach ) continue;
		luminance = 0.2126f * fmaxf( light->color[0], 0.0f )
			+ 0.7152f * fmaxf( light->color[1], 0.0f )
			+ 0.0722f * fmaxf( light->color[2], 0.0f );
		score = luminance * light->intensity
			/ fmaxf( distanceSquared, 1.0f );
		for ( uint32_t slot = 0u; slot < RAL_METAL_MAX_DIRECT_LIGHTS_PER_DRAW;
				++slot ) if ( !selected[slot] || score > selectedScore[slot] ) {
			insertion = slot; break;
		}
		if ( insertion == RAL_METAL_MAX_DIRECT_LIGHTS_PER_DRAW ) continue;
		for ( uint32_t slot = RAL_METAL_MAX_DIRECT_LIGHTS_PER_DRAW - 1u;
				slot > insertion; --slot ) {
			selected[slot] = selected[slot - 1u];
			selectedScore[slot] = selectedScore[slot - 1u];
		}
		selected[insertion] = light;
		selectedScore[insertion] = score;
	}
	uint32_t selectedCount = 0u;
	for ( uint32_t slot = 0u; slot < RAL_METAL_MAX_DIRECT_LIGHTS_PER_DRAW;
			++slot ) {
		const renderLightCommand_t *light = selected[slot];
		if ( !light ) break;
		for ( uint32_t axis = 0u; axis < 3u; ++axis ) {
			params->directLightPositionRadius[slot][axis] = light->origin[axis];
			params->directLightColorFalloff[slot][axis] =
				MetalSrgbChannelToLinear( light->color[axis] );
			params->directLightEndLinear[slot][axis] = light->end[axis];
		}
		params->directLightPositionRadius[slot][3] = light->intensity;
		params->directLightColorFalloff[slot][3] =
			1.0f / ( light->intensity * light->intensity );
		params->directLightEndLinear[slot][3] = light->hasEnd ? 1.0f : 0.0f;
		selectedCount++;
	}
	params->directLightMeta[0] = selectedCount;
	return selectedCount;
}

static void ResetPersistentEffects( ralMetalPresent_t *present ) {
	for ( uint32_t bank = 0u; bank < 2u; ++bank ) {
		RenderEffectDecalPool_Reset( &present->effectDecalPools[bank] );
		RenderEffectBeamPool_Reset( &present->effectBeamPools[bank] );
	}
	present->effectPoolIndex = 0u;
}

static qboolean PreparePersistentEffects( ralMetalPresent_t *present,
		const renderSubmissionState_t *frontend, uint64_t frameGeneration,
		uint32_t *outDecalCount, uint32_t *outDecalDropped,
		uint32_t *outBeamCount, uint32_t *outBeamDropped ) {
	renderEffectPrimitiveSnapshot_t snapshot;
	renderWorldSnapshot_t view;
	const spriteDesc_t *sprites;
	const emitterDesc_t *emitters;
	const decalDesc_t *decals;
	const renderEffectRibbonCommand_t *ribbons;
	const ribbonPoint_t *points;
	const beamDesc_t *beams;
	renderEffectDecalPoolReceipt_t decalReceipt;
	renderEffectBeamPoolReceipt_t beamReceipt;
	if ( !RenderSubmission_EffectPrimitiveSnapshots( frontend, &snapshot,
			&sprites, &emitters, &decals, &ribbons, &points, &beams ) ) return qfalse;
	const qboolean haveView = RenderSubmission_ViewSnapshot( frontend, &view );
	const float now = haveView ? (float)view.timeMs * 0.001f : 0.0f;
	const uint32_t current = present->effectPoolIndex, candidate = current ^ 1u;
	if ( !RenderEffectDecalPool_Advance( &present->effectDecalPools[current],
			frameGeneration, haveView, now, decals, snapshot.decalCount,
			&present->effectDecalPools[candidate], &decalReceipt )
			|| !RenderEffectBeamPool_Advance( &present->effectBeamPools[current],
				frameGeneration, haveView, now, beams, snapshot.beamCount,
				&present->effectBeamPools[candidate], &beamReceipt ) ) return qfalse;
	/* Resident effects survive loading/UI-only frames, but are not drawable
	 * without a scene view. Shared admission rejects new viewless births. */
	*outDecalCount = haveView ? decalReceipt.activeCount : 0u;
	*outBeamCount = haveView ? beamReceipt.activeCount : 0u;
	*outDecalDropped = decalReceipt.droppedCount;
	*outBeamDropped = beamReceipt.droppedCount;
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

static float EffectBeamFade( const renderEffectBeamSlot_t *slot, float now ) {
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
		const renderEffectDecalPool_t *decalPool,
		const renderEffectBeamPool_t *beamPool,
		const renderSubmissionState_t *frontend,
		ralMetalWorldVertex_t **outVertices, uint32_t **outIndices,
		ralMetalEntityBatch_t **outBatches, uint32_t *outVertexCount,
		uint32_t *outIndexCount, uint32_t *outBatchCount,
		uint32_t *outModelCount, uint32_t *outPrimitiveCount,
		uint32_t *outTemporalCount, uint32_t *outLocalIrradianceCount,
		uint32_t *outUnresolvedCount, ralMetalEntityMotion_t *motion ) {
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
	if ( motion ) motion->candidateCount = 0u;
	commands = RenderSubmission_EntityCommands( frontend, &commandCount );
	memset( &effectSnapshot, 0, sizeof( effectSnapshot ) );
	if ( frontend ) haveEffects = RenderSubmission_EffectPrimitiveSnapshots(
		frontend, &effectSnapshot, &effectSprites, &effectEmitters,
		&effectDecals, &effectRibbons, &effectRibbonPoints, &effectBeams );
	(void)effectEmitters; (void)effectBeams;
	/* Resident effects survive a UI-only frame, but cannot be projected in it.
	 * Newly submitted world primitives still require a valid scene view. */
	if ( !RenderSubmission_ViewSnapshot( frontend, &view ) )
		return !commandCount && ( !haveEffects
			|| ( !effectSnapshot.spriteCount && !effectSnapshot.decalCount
				&& !effectSnapshot.ribbonCount && !effectSnapshot.beamCount ) )
			? qtrue : qfalse;
	if ( present ) for ( uint32_t slot = 0u;
			slot < RENDER_SUBMISSION_MAX_EFFECT_DECALS; ++slot )
		if ( decalPool->slots[slot].active ) {
			havePersistentDecals = qtrue; break;
		}
	if ( present ) for ( uint32_t slot = 0u;
			slot < RENDER_SUBMISSION_MAX_EFFECT_BEAMS; ++slot )
		if ( beamPool->slots[slot].active ) {
			havePersistentBeams = qtrue; break;
		}
	if ( ( !commands || !commandCount ) && ( !haveEffects
			|| ( !effectSnapshot.spriteCount && !havePersistentDecals
				&& !effectSnapshot.ribbonCount && !havePersistentBeams ) ) ) return qtrue;
	/* Entity-only UI subscenes use their submitted view, not a loaded BSP. */
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
				&& i < RENDER_SUBMISSION_MAX_EFFECT_DECALS; ++i ) {
			if ( !decalPool->slots[i].active ) continue;
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
				&& slot < RENDER_SUBMISSION_MAX_EFFECT_BEAMS; ++slot ) {
			if ( !beamPool->slots[slot].active ) continue;
			const uint32_t copies = (uint32_t)beamPool->slots[slot].beam.axialCopies;
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
			const uint32_t entityFirstBatch = batchBase;
			uint32_t frame = entity->frame < 0 ? 0u : (uint32_t)entity->frame % model.frameCount;
			uint32_t oldFrame = entity->oldframe < 0 ? frame
				: (uint32_t)entity->oldframe % model.frameCount;
			float oldWeight = entity->backlerp, newWeight = 1.0f - oldWeight;
			float modelViewOrigin[4] = { 0, 0, 0, 1 };
			float delta[3], axisScale = 1.0f;
			for ( unsigned axis = 0; axis < 3; ++axis )
				delta[axis] = view.viewOrigin[axis] - entity->origin[axis];
			/* Match Q3 R_RotateForEntity's authored nonNormalizedAxes rule. */
			if ( entity->nonNormalizedAxes ) {
				float length = sqrtf( DotProduct( entity->axis[0], entity->axis[0] ) );
				axisScale = length > 0.0f ? 1.0f / length : 0.0f;
			}
			for ( unsigned axis = 0; axis < 3; ++axis )
				modelViewOrigin[axis] = DotProduct( delta, entity->axis[axis] ) * axisScale;
			for ( uint32_t vertexIndex = 0u; vertexIndex < model.vertexCount; ++vertexIndex ) {
				float local[3], localNormal[3], world[3];
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
				}
				color4ub_t vertexColor;
				memset( vertexColor.rgba, 255, sizeof( vertexColor.rgba ) );
				if ( model.colors ) vertexColor = model.colors[vertexIndex];
				EntityVertex( &(*outVertices)[vertexBase + vertexIndex], &view, world,
					model.texCoords[vertexIndex * 2u], model.texCoords[vertexIndex * 2u + 1u],
					vertexColor );
				{
					ralMetalWorldVertex_t *written = &(*outVertices)[vertexBase + vertexIndex];
					memcpy( written->materialPosition, local, sizeof( local ) );
					memcpy( written->normal, localNormal, sizeof( localNormal ) );
				}
			}
			for ( uint32_t index = 0u; index < model.indexCount; ++index )
				(*outIndices)[indexBase + index] = vertexBase + model.indices[index];
			for ( uint32_t batchIndex = 0u; batchIndex < model.batchCount; ++batchIndex ) {
				ralMetalEntityBatch_t *batch = &(*outBatches)[batchBase++];
				const renderModelBatch_t *modelBatch = &model.batches[batchIndex];
				for ( unsigned axis = 0; axis < 3; ++axis )
					memcpy( batch->modelAxes[axis], entity->axis[axis], 3u * sizeof( float ) );
				memcpy( batch->modelViewOrigin, modelViewOrigin, sizeof( modelViewOrigin ) );
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
				EntityBatchBounds( *outVertices,
					vertexBase + modelBatch->firstVertex, modelBatch->vertexCount,
					batch );
				EntityBatchMaterial( frontend,
					RenderSubmission_EntityBatchMaterial( frontend, &commands[i],
						&model.batches[batchIndex] ), batch );
				batch->entityColor = entity->shader;
				EntityBatchLighting( &commands[i], batch );
			}
			/* A model entity with a motion identity (brush entity, MD3 item or creature, IQM) is a candidate for object motion: its
			 * previous pose replays its own animation frames. The first-person weapon moves with the camera and stays invalidated. */
			if ( motion && commands[i].hasTemporal && model.frameCount && model.positions
					&& !( entity->renderfx & ( RF_DEPTHHACK | RF_FIRST_PERSON ) ) ) {
				if ( motion->candidateCount == motion->candidateCapacity ) {
					const uint32_t capacity = motion->candidateCapacity ? motion->candidateCapacity * 2u : 64u;
					ralMetalEntityMotionCandidate_t *grown = (ralMetalEntityMotionCandidate_t *)realloc(
						motion->candidates, capacity * sizeof( *grown ) );
					if ( grown ) { motion->candidates = grown; motion->candidateCapacity = capacity; }
				}
				if ( motion->candidateCount < motion->candidateCapacity ) {
					ralMetalEntityMotionCandidate_t *candidate = &motion->candidates[motion->candidateCount++];
					candidate->command = i;
					candidate->firstVertex = vertexBase; candidate->vertexCount = model.vertexCount;
					candidate->firstBatch = entityFirstBatch; candidate->batchCount = batchBase - entityFirstBatch;
				}
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
			batch->entityColor = entity->shader;
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
			batch->entityColor = color;
			batch->cullMode = RENDER_CULL_NONE; batch->depthWrite = qfalse;
			if ( sprite->flags & PRIM_FLAG_ADDITIVE )
				batch->alphaMode = RENDER_ALPHA_ALPHA_ADDITIVE;
			vertexBase += 4u; indexBase += 6u;
		}
		for ( uint32_t i = 0u; present
				&& i < RENDER_SUBMISSION_MAX_EFFECT_DECALS; ++i ) {
			if ( !decalPool->slots[i].active ) continue;
			const decalDesc_t *decal = &decalPool->slots[i].decal;
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
			batch->entityColor = color;
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
				batch->alphaMode = RENDER_ALPHA_ALPHA_ADDITIVE;
		}
		for ( uint32_t slot = 0u; present
				&& slot < RENDER_SUBMISSION_MAX_EFFECT_BEAMS; ++slot ) {
			const renderEffectBeamSlot_t *beamSlot = &beamPool->slots[slot];
			const beamDesc_t *beam;
			float start[3], end[3], axis[3], baseExtrude[3];
			float age, fade;
			uint32_t firstIndex, copies;
			const color4ub_t whiteColor = { { 255u, 255u, 255u, 255u } };
			float startColor[4], endColor[4];
			if ( !beamSlot->active ) continue;
			beam = &beamSlot->beam;
			ResolveEffectBeamEndpoint( frontend, beam, qtrue, start );
			ResolveEffectBeamEndpoint( frontend, beam, qfalse, end );
			if ( !wired_ral_beam_frame( start, end, view.viewOrigin,
					axis, baseExtrude ) ) continue;
			age = fmaxf( (float)view.timeMs * 0.001f - beamSlot->spawnTime, 0.0f );
			const float scrollTime = beamSlot->transient
				? ( present->shaderTimeOverride > 0.0f ? present->shaderTimeOverride
					: (float)view.timeMs * 0.001f ) : age;
			fade = EffectBeamFade( beamSlot, (float)view.timeMs * 0.001f );
			const float startAlpha = beam->startColor[3] * fade;
			const float endAlpha = beam->endColor[3] * fade;
			for ( uint32_t channel = 0u; channel < 4u; ++channel ) {
				/* Preserve descriptor precision and straight sRGB until fragment
				 * interpolation, as beam.vert/beam.frag do on Vulkan. */
				startColor[channel] = channel == 3u ? startAlpha : beam->startColor[channel];
				endColor[channel] = channel == 3u ? endAlpha : beam->endColor[channel];
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
						- extrude[component] * beam->startWidth;
					corners[1][component] = start[component]
						+ extrude[component] * beam->startWidth;
					corners[2][component] = end[component]
						- extrude[component] * beam->endWidth;
					corners[3][component] = end[component]
						+ extrude[component] * beam->endWidth;
				}
				EntityVertex( &(*outVertices)[vertexBase++], &view, corners[0],
					0.0f, 0.0f, whiteColor );
				EntityVertex( &(*outVertices)[vertexBase++], &view, corners[1],
					0.0f, 1.0f, whiteColor );
				EntityVertex( &(*outVertices)[vertexBase++], &view, corners[2],
					1.0f, 0.0f, whiteColor );
				EntityVertex( &(*outVertices)[vertexBase++], &view, corners[3],
					1.0f, 1.0f, whiteColor );
				for ( uint32_t corner = 0u; corner < 4u; ++corner )
					memcpy( (*outVertices)[firstVertex + corner].color,
						corner < 2u ? startColor : endColor, sizeof( startColor ) );
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
			batch->primitiveBeam = qtrue;
			batch->beamUv[0] = scrollTime * beam->uvScroll[0];
			batch->beamUv[1] = scrollTime * beam->uvScroll[1];
			batch->beamUv[2] = scrollTime;
			batch->alphaMode = RENDER_ALPHA_ADDITIVE;
		}
	}
	*outVertexCount = vertexCount; *outIndexCount = indexCount; *outBatchCount = batchCount;
	return qtrue;
}

static bool MaterialFusedAdditive( const renderMaterialSnapshot_t *material,
		renderAlphaMode_t alphaMode, bool authoredSurface ) {
	return authoredSurface && material->stageCount > 1u
		&& material->stageDepth.singleDraw && material->stageBlendAdditive
		&& ( alphaMode == RENDER_ALPHA_ADDITIVE
			|| alphaMode == RENDER_ALPHA_ALPHA_ADDITIVE );
}

static id<MTLRenderPipelineState> MaterialBlendPipeline( ralMetalPresent_t *present,
		renderAlphaMode_t alphaMode, bool fusedAdditive ) {
	if ( fusedAdditive || alphaMode == RENDER_ALPHA_ADDITIVE )
		return present->worldAdditivePipeline;
	if ( alphaMode == RENDER_ALPHA_ALPHA_ADDITIVE ) return present->worldAlphaAdditivePipeline;
	if ( alphaMode == RENDER_ALPHA_BLEND ) return present->worldBlendPipeline;
	return present->worldPipeline;
}

/* wired_world_fragment carries function constant 20 (TAA trust output, see ui_solid.msl), so Metal only builds pipelines from a
 * specialized function: the scene pipelines specialize it off, the TAA trust pipelines on. */
static id<MTLFunction> WorldFragmentFunction( id<MTLLibrary> library, bool trustOutput, NSError **error ) {
	MTLFunctionConstantValues *constants = [[[MTLFunctionConstantValues alloc] init] autorelease];
	[constants setConstantValue:&trustOutput type:MTLDataTypeBool atIndex:20];
	return [library newFunctionWithName:@"wired_world_fragment" constantValues:constants error:error];
}

/* TAA trust pass: the batch's own blend lowered onto the trust target (see EncodeTaaEffectMask); opaque batches write trust 0. */
static id<MTLRenderPipelineState> TaaTrustPipeline( ralMetalPresent_t *present,
		id<MTLRenderPipelineState> const *trustPipelines, renderAlphaMode_t alphaMode, bool fusedAdditive ) {
	if ( fusedAdditive || alphaMode == RENDER_ALPHA_ADDITIVE ) return trustPipelines[2];
	if ( alphaMode == RENDER_ALPHA_ALPHA_ADDITIVE ) return trustPipelines[1];
	if ( alphaMode == RENDER_ALPHA_BLEND ) return trustPipelines[0];
	return present->taaMaskPipeline;
}

static id<MTLDepthStencilState> MaterialDepthState( ralMetalPresent_t *present,
		const renderMaterialSnapshot_t *material, qboolean batchDepthWrite,
		bool authoredSurface ) {
	/* Publication computes the shared proof once, not for each draw. It only
	 * proves depth: ordered blend/alpha programs still need their own lowering.
	 * Beam and sky have dedicated primitive depth semantics. */
	if ( authoredSurface && material->stageDepth.singleDraw ) {
		const ralMaterialStageDepthPlan_t *depth = &material->stageDepth;
		if ( depth->depthFunc == RAL_MATERIAL_DEPTH_EQUAL )
			return depth->depthWrite ? present->worldDepthEqualState
				: present->worldDepthEqualReadState;
		return depth->depthWrite ? present->worldDepthState : present->worldDepthReadState;
	}
	return batchDepthWrite ? present->worldDepthState : present->worldDepthReadState;
}


static qboolean DrawEntityBatches( id<MTLRenderCommandEncoder> encoder,
		ralMetalPresent_t *present, const renderSubmissionState_t *frontend,
		wiredMetalDrawUniforms &uniforms,
		id<MTLBuffer> entityVertices, id<MTLBuffer> entityIndices,
		const ralMetalEntityBatch_t *entityBatches, uint32_t entityBatchCount,
		const renderWorldSnapshot_t *sceneView, uint32_t phaseMask,
		id<MTLTexture> sceneDepth, const float *sceneFogParams,
		ralMetalPresentReceipt_t *receipt,
		id<MTLRenderPipelineState> const *trustPipelines = nullptr ) {
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
	/* wired_world_fragment scene-depth fog inputs (texture 19 / buffer 3). */
	if ( sceneDepth ) [encoder setFragmentTexture:sceneDepth atIndex:19u];
	[encoder setFragmentBytes:sceneFogParams ? sceneFogParams : s_sceneFogOff
		length:4u * sizeof( float ) atIndex:3u];
	scissor.x = viewportX; scissor.y = viewportY;
	scissor.width = viewportWidth; scissor.height = viewportHeight;
	[encoder setViewport:viewport];
	[encoder setScissorRect:scissor];
	[encoder setVertexBuffer:entityVertices offset:0u atIndex:0u];
	wiredMetalFragmentBindings<17u> fragmentBindings;
	wiredMetalEncoderValue<id<MTLBuffer>> materialBufferBinding;
	wiredMetalEncoderValue<id<MTLRenderPipelineState>> pipelineState;
	wiredMetalEncoderValue<id<MTLDepthStencilState>> depthState;
	wiredMetalEncoderValue<MTLCullMode> cullState;
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
		if ( !batch->visible || batchPhase != phase
				|| !( phaseMask & ( 1u << phase ) ) ) continue;
		if ( trustPipelines && batch->motion ) continue;   /* the TAA mask draws its object motion instead */
		const MTLCullMode cull = ( sceneView->rdflags & RDF_NOWORLDMODEL )
			|| batch->cullMode == RENDER_CULL_NONE ? MTLCullModeNone
			: ( batch->cullMode == RENDER_CULL_FRONT ? MTLCullModeFront : MTLCullModeBack );
		if ( cullState.wired_changed( cull ) ) [encoder setCullMode:cull];
		memset( &params, 0, sizeof( params ) );
		params.skyScaleScroll[0] = params.skyScaleScroll[1] = 1.0f;
		memcpy( params.modelAxes, batch->modelAxes, sizeof( params.modelAxes ) );
		memcpy( params.modelViewOrigin, batch->modelViewOrigin, sizeof( params.modelViewOrigin ) );
		params.skyTime = present->shaderTimeOverride > 0.0f
			? present->shaderTimeOverride
			: (float)sceneView->timeMs * 0.001f;
		memset( &materialSnapshot, 0, sizeof( materialSnapshot ) );
		if ( RenderSubmission_MaterialSnapshot( frontend, batch->material,
				&materialSnapshot ) ) {
			memcpy( params.skyScaleScroll, materialSnapshot.skyScaleScroll,
				sizeof( params.skyScaleScroll ) );
			params.stageProgram[0] = materialSnapshot.stageCount;
		}
		/* bit 0: lightmap bound, bit 1: authored/entity vertex colour. */
		params.hasLightmap = batch->useVertexColor ? 2u : 0u;
		/* bit 5: primitive-beam coverage; bit 4 belongs to skybox projection. */
		if ( batch->primitiveBeam ) params.hasLightmap |= 32u;
		memcpy( params.beamUv, batch->beamUv, sizeof( params.beamUv ) );
		/* First-person weapons compress only their depth range. */
		const double depthFar = batch->depthHack ? 0.3 : 1.0;
		if ( viewport.zfar != depthFar ) {
			viewport.zfar = depthFar;
			[encoder setViewport:viewport];
		}
		params.alphaMode = (uint32_t)batch->alphaMode;
		params.stageProgram[1] = MaterialFusedAdditive( &materialSnapshot,
			batch->alphaMode, !batch->primitiveBeam );
		params.alphaCutoff = batch->alphaCutoff;
		SetDefaultMaterialFactors( &params );
		for ( unsigned c = 0; c < 4; ++c )
			params.entityColor[c] = batch->entityColor.rgba[c] / 255.0f;
		/* bit 0: local sample; bit 1: RF_MINLIGHT; bit 2: Q3 byte grid. */
		params.hasLocalIrradiance = ( batch->hasLocalIrradiance ? 1u : 0u )
			| ( batch->minLight ? 2u : 0u ) | ( batch->legacyLightGrid ? 4u : 0u );
		for ( uint32_t coefficient = 0u; coefficient < 4u; ++coefficient )
			memcpy( params.localSh[coefficient], batch->localSh[coefficient],
				3u * sizeof( float ) );
		if ( batch->receivesDirectLight ) {
			const uint32_t selected = PopulateDirectLights( frontend,
				batch->boundsCenter, batch->boundsRadius, &params );
			if ( selected ) {
				receipt->directLightDrawCount++;
				receipt->directLightReferenceCount += selected;
			}
		}
		if ( !UiMaterial( present, frontend, batch->material, &texture,
				&sampler, &textured, &msdf ) || msdf ) return qfalse;
		for ( uint32_t stageIndex = 0u;
				stageIndex < RENDER_MATERIAL_MAX_STAGES; ++stageIndex ) {
			const renderMaterialStageSnapshot_t *stage =
				&materialSnapshot.stages[stageIndex];
			renderMaterialStageGpuParams_t *stageParams =
				&params.stages[stageIndex];
			qboolean stageTextured = qfalse, stageMsdf = qfalse;
			stageTextures[stageIndex] = present->uiFallbackTexture;
			stageSamplers[stageIndex] = present->uiClampSampler;
			if ( stageIndex >= materialSnapshot.stageCount ) continue;
			wired_material_stage_pack( stage, stageParams );
			/* Entity materials have no BSP lightmap operand. A malformed entity
			 * script that requests one receives neutral white, matching Q3's
			 * vertex-lit fallback rather than sampling unrelated world data. */
			if ( stage->imageSource == 0u ) {
				if ( !UiMaterial( present, frontend,
						MaterialStageHandleAtTime( stage, params.skyTime ),
						&stageTextures[stageIndex], &stageSamplers[stageIndex],
						&stageTextured, &stageMsdf ) || stageMsdf ) return qfalse;
			}
		}
		id<MTLRenderPipelineState> pipeline = trustPipelines
			? TaaTrustPipeline( present, trustPipelines, batch->alphaMode, params.stageProgram[1] != 0u )
			: MaterialBlendPipeline( present, batch->alphaMode, params.stageProgram[1] != 0u );
		id<MTLDepthStencilState> depth = MaterialDepthState( present, &materialSnapshot,
			batch->depthWrite, !batch->primitiveBeam );
		if ( pipelineState.wired_changed( pipeline ) ) [encoder setRenderPipelineState:pipeline];
		if ( depthState.wired_changed( depth ) ) [encoder setDepthStencilState:depth];
		/* Preserve every entity material operand (including neutral BSP/sky
		 * bindings) with two native calls instead of 34. Stack arrays only:
		 * command-buffer resource retention and painter order are unchanged. */
		static_assert( RENDER_MATERIAL_MAX_STAGES == 8u,
			"Entity texture slots 6-13 must match the material shader ABI" );
		id<MTLTexture> entityTextures[17];
		id<MTLSamplerState> entitySamplers[17];
		for ( uint32_t slot = 0u; slot < 17u; ++slot ) {
			entityTextures[slot] = present->uiFallbackTexture;
			entitySamplers[slot] = present->uiClampSampler;
		}
		entityTextures[0] = texture;
		entitySamplers[0] = sampler;
		for ( uint32_t slot = 2u; slot <= 4u; ++slot )
			entityTextures[slot] = present->worldLightingFallbackTexture;
		entityTextures[15] = entityTextures[16] = present->worldLightingFallbackTexture;
		for ( uint32_t stageIndex = 0u;
				stageIndex < RENDER_MATERIAL_MAX_STAGES; ++stageIndex ) {
			entityTextures[6u + stageIndex] = stageTextures[stageIndex];
			entitySamplers[6u + stageIndex] = stageSamplers[stageIndex];
		}
		if ( fragmentBindings.wired_textures_changed( entityTextures ) )
			[encoder setFragmentTextures:entityTextures withRange:NSMakeRange( 0u, 17u )];
		if ( fragmentBindings.wired_samplers_changed( entitySamplers ) )
			[encoder setFragmentSamplerStates:entitySamplers withRange:NSMakeRange( 0u, 17u )];
		if ( !uniforms.wired_bind( RalMetal_CoreNativeDevice( present->core ),
				encoder, params, materialBufferBinding ) ) return qfalse;
		if ( batch->hasLocalIrradiance ) receipt->localIrradianceDrawCount++;
		[encoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle
			indexCount:batch->indexCount indexType:MTLIndexTypeUInt32
			indexBuffer:entityIndices
			indexBufferOffset:(NSUInteger)batch->firstIndex * sizeof( uint32_t )];
	}
	return qtrue;
}

static MTLVertexDescriptor *WorldVertexDescriptor( void ) {
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
	vertices.attributes[5].format = MTLVertexFormatFloat4;
	vertices.attributes[5].offset = offsetof( ralMetalWorldVertex_t, materialPosition );
	vertices.attributes[5].bufferIndex = 0u;
	vertices.layouts[0].stride = sizeof( ralMetalWorldVertex_t );
	return vertices;
}

/* TAA history trust. The Metal scene pass has no velocity attachment, so every pixel would otherwise be reprojected as static world.
 * Entities (their motion is not tracked) and the first-person weapon (it moves with the camera) must not inherit history that way, and
 * a translucent effect may only reduce it by what it shows (the Vulkan renderer's validity rule, 73ffcbeba). The entity batches are
 * drawn again through DrawEntityBatches - the same vertex function, material bindings and stage programs as the scene - against the
 * finished scene depth into an R8 target cleared to 1: opaque batches write 0, blended batches run the world material fragment
 * specialized to output its visible contribution (max(r, g, b), 0, 0, alpha) and blend it with the batch's own factors under
 * REVERSE_SUBTRACT (trust' = trust * dst - value * src). The motion kernel takes the trust as validity. */
static qboolean EnsureTaaTrustPipelines( ralMetalPresent_t *present ) {
	if ( present->taaMaskPipeline && present->taaTrustPipelines[0] && present->taaTrustPipelines[1]
			&& present->taaTrustPipelines[2] && present->taaMotionPipeline ) return qtrue;
	id<MTLDevice> device = RalMetal_CoreNativeDevice( present->core );
	NSError *error = nil;
	id<MTLFunction> vertex = [present->uiLibrary newFunctionWithName:@"wired_world_vertex"];
	id<MTLFunction> opaque = [present->uiLibrary newFunctionWithName:@"wired_taa_mask_fragment"];
	id<MTLFunction> trust = WorldFragmentFunction( present->uiLibrary, true, &error );
	id<MTLFunction> motionVertex = [present->uiLibrary newFunctionWithName:@"wired_taa_object_motion_vertex"];
	id<MTLFunction> motionFragment = [present->uiLibrary newFunctionWithName:@"wired_taa_object_motion_fragment"];
	if ( vertex && opaque && trust && motionVertex && motionFragment && !error ) {
		MTLRenderPipelineDescriptor *pipeline = [[[MTLRenderPipelineDescriptor alloc] init] autorelease];
		pipeline.vertexDescriptor = WorldVertexDescriptor();
		pipeline.colorAttachments[0].pixelFormat = MTLPixelFormatRGBA16Float;
		pipeline.depthAttachmentPixelFormat = MTLPixelFormatDepth32Float;
		/* Object motion: green and blue carry the velocity, alpha 0 marks the pixel as object motion; red (the trust) is left to the
		 * mask and trust pipelines, so the draw order between them does not matter. */
		if ( !present->taaMotionPipeline ) {
			pipeline.vertexFunction = motionVertex;
			pipeline.fragmentFunction = motionFragment;
			pipeline.colorAttachments[0].writeMask = MTLColorWriteMaskGreen | MTLColorWriteMaskBlue | MTLColorWriteMaskAlpha;
			present->taaMotionPipeline = [device newRenderPipelineStateWithDescriptor:pipeline error:&error];
		}
		pipeline.vertexFunction = vertex;
		pipeline.colorAttachments[0].writeMask = MTLColorWriteMaskRed;
		if ( !present->taaMaskPipeline ) {
			pipeline.fragmentFunction = opaque;
			present->taaMaskPipeline = [device newRenderPipelineStateWithDescriptor:pipeline error:&error];
		}
		/* The scene's three translucent blends (EnsureWorldPipeline): SRC_ALPHA/ONE_MINUS_SRC_ALPHA, SRC_ALPHA/ONE, ONE/ONE. */
		static const MTLBlendFactor srcFactors[3] = { MTLBlendFactorSourceAlpha, MTLBlendFactorSourceAlpha, MTLBlendFactorOne };
		static const MTLBlendFactor dstFactors[3] = { MTLBlendFactorOneMinusSourceAlpha, MTLBlendFactorOne, MTLBlendFactorOne };
		pipeline.fragmentFunction = trust;
		pipeline.colorAttachments[0].blendingEnabled = YES;
		pipeline.colorAttachments[0].rgbBlendOperation = MTLBlendOperationReverseSubtract;
		pipeline.colorAttachments[0].alphaBlendOperation = MTLBlendOperationReverseSubtract;
		for ( uint32_t i = 0u; i < 3u && !error; ++i ) {
			if ( present->taaTrustPipelines[i] ) continue;
			pipeline.colorAttachments[0].sourceRGBBlendFactor = srcFactors[i];
			pipeline.colorAttachments[0].destinationRGBBlendFactor = dstFactors[i];
			pipeline.colorAttachments[0].sourceAlphaBlendFactor = srcFactors[i];
			pipeline.colorAttachments[0].destinationAlphaBlendFactor = dstFactors[i];
			present->taaTrustPipelines[i] = [device newRenderPipelineStateWithDescriptor:pipeline error:&error];
		}
	}
	[vertex release]; [opaque release]; [trust release]; [motionVertex release]; [motionFragment release];
	return present->taaMaskPipeline && present->taaTrustPipelines[0] && present->taaTrustPipelines[1]
		&& present->taaTrustPipelines[2] && present->taaMotionPipeline && !error ? qtrue : qfalse;
}

/* The entity pose cache is process-wide while TAA frame ids belong to this presenter's history: every point that restarts the history
 * restarts the cache with it, or Begin would refuse the lower ids. */
static void EntityMotionReset( ralMetalPresent_t *present ) {
	R_TemporalEntityCacheResetAll();
	present->entityMotion.frame = 0u;
	present->entityMotion.recorded = present->entityMotion.rejected = 0u;
	present->entityMotion.previousValid = present->entityMotion.motionBatches = 0u;
	present->entityMotion.motionBatchesTotal = present->entityMotion.motionFrames = 0u;
}

/* Publishes (committed) or discards the open pose-cache transaction. A frame that failed after its TAA plan leaves it open; the next
 * frame discards it before it begins its own. */
static void EntityMotionFinish( ralMetalPresent_t *present, qboolean committed ) {
	if ( !present->entityMotion.frame ) return;
	(void)R_TemporalEntityCacheFinish( present->entityMotion.world, present->entityMotion.frame, committed );
	present->entityMotion.frame = 0u;
}

/* Records this frame's candidate poses under the TAA frame id and, for each entity with a committed previous pose, fills the previous
 * world position of its vertices (the model's previous animation frames under the previous transform) and marks the batches that
 * write motion. True when at least one batch writes motion. */
static qboolean EntityMotionPrepare( ralMetalPresent_t *present, const renderSubmissionState_t *frontend,
		uint32_t vertexCount, ralMetalEntityBatch_t *batches, uint32_t batchCount,
		uint64_t frameId ) {
	ralMetalEntityMotion_t *motion = &present->entityMotion;
	const renderEntityCommand_t *commands;
	uint32_t commandCount = 0u;
	motion->motionBatches = 0u;
	EntityMotionFinish( present, qfalse );
	if ( !motion->candidateCount || !frontend || frontend->activeWorldIndex < 0 || !batches ) return qfalse;
	if ( vertexCount > motion->previousCapacity ) {
		float (*grown)[4] = (float (*)[4])realloc( motion->previous, (size_t)vertexCount * sizeof( *grown ) );
		if ( !grown ) return qfalse;
		motion->previous = grown; motion->previousCapacity = vertexCount;
	}
	if ( !R_TemporalEntityCacheBegin( frontend->activeWorldIndex, present->taaTopologyEpoch, 1u, frameId ) ) return qfalse;
	motion->world = frontend->activeWorldIndex; motion->frame = frameId;
	commands = RenderSubmission_EntityCommands( frontend, &commandCount );
	for ( uint32_t c = 0u; c < motion->candidateCount; ++c ) {
		const ralMetalEntityMotionCandidate_t *candidate = &motion->candidates[c];
		temporalEntityPose_t pose;
		temporalEntityPoseReceipt_t receipt;
		renderModelSnapshot_t model;
		if ( candidate->command >= commandCount || candidate->firstVertex > vertexCount
				|| candidate->vertexCount > vertexCount - candidate->firstVertex
				|| candidate->firstBatch > batchCount || candidate->batchCount > batchCount - candidate->firstBatch ) continue;
		const renderEntityCommand_t *command = &commands[candidate->command];
		if ( !RenderSubmission_ModelSnapshot( frontend, command->entity.hModel, &model ) ) continue;
		RenderTemporal_EntityPose( &command->entity, &model, &pose );
		if ( !R_TemporalEntityCacheRecord( motion->world, frameId, (uintptr_t)command, &command->motion, &pose, &receipt ) ) {
			motion->rejected++; continue;
		}
		motion->recorded++;
		if ( !receipt.previousValid ) continue;
		/* The previous pose replays its own animation frames (the same frame selection and lerp as BuildEntityVertices) under its
		 * own transform. */
		if ( candidate->vertexCount != model.vertexCount || !model.frameCount ) continue;
		motion->previousValid++;
		{
			const temporalEntityPose_t *previous = &receipt.previous;
			const uint32_t frame = previous->frame < 0 ? 0u : (uint32_t)previous->frame % model.frameCount;
			const uint32_t oldFrame = previous->oldframe < 0 ? frame : (uint32_t)previous->oldframe % model.frameCount;
			const float oldWeight = previous->backlerp, newWeight = 1.0f - oldWeight;
			for ( uint32_t vertex = 0u; vertex < model.vertexCount; ++vertex ) {
				float local[3];
				float *out = motion->previous[candidate->firstVertex + vertex];
				for ( uint32_t axis = 0u; axis < 3u; ++axis )
					local[axis] = model.positions[( (size_t)frame * model.vertexCount + vertex ) * 3u + axis] * newWeight
						+ model.positions[( (size_t)oldFrame * model.vertexCount + vertex ) * 3u + axis] * oldWeight;
				for ( uint32_t axis = 0u; axis < 3u; ++axis )
					out[axis] = previous->origin[axis] + previous->axis[0u * 3u + axis] * local[0]
						+ previous->axis[1u * 3u + axis] * local[1] + previous->axis[2u * 3u + axis] * local[2];
				out[3] = 1.0f;
			}
		}
		for ( uint32_t b = candidate->firstBatch; b < candidate->firstBatch + candidate->batchCount; ++b ) {
			if ( !batches[b].visible || batches[b].depthHack
					|| !RenderTemporal_EntityBatchWritesMotion( frontend, batches[b].material,
						RenderTemporal_PoseRotated( &receipt.previous, &pose ) ) ) continue;
			batches[b].motion = qtrue; motion->motionBatches++;
		}
	}
	motion->motionBatchesTotal += motion->motionBatches;
	if ( motion->motionBatches ) motion->motionFrames++;
	return motion->motionBatches ? qtrue : qfalse;
}

static qboolean EncodeTaaEffectMask( id<MTLCommandBuffer> command, ralMetalPresent_t *present,
		const renderSubmissionState_t *frontend, wiredMetalDrawUniforms &uniforms,
		id<MTLTexture> mask, id<MTLTexture> depth, id<MTLBuffer> entityVertices,
		id<MTLBuffer> entityIndices, const ralMetalEntityBatch_t *entityBatches,
		uint32_t entityBatchCount, const renderWorldSnapshot_t *sceneView,
		const ralMetalWorldViewParams_t *viewParams, const ralMetalWorldAtmosphereParams_t *atmosphereParams,
		id<MTLBuffer> atmosphereArena, const ralMetalPresentReceipt_t *receipt,
		id<MTLBuffer> motionPrevious, const float *motionViews ) {
	if ( !EnsureTaaTrustPipelines( present ) ) return qfalse;
	MTLRenderPassDescriptor *pass = [MTLRenderPassDescriptor renderPassDescriptor];
	pass.colorAttachments[0].texture = mask;
	pass.colorAttachments[0].loadAction = MTLLoadActionClear;
	/* red = history trust (1 kept), green/blue = object velocity, alpha = 1 camera reprojection / 0 object motion */
	pass.colorAttachments[0].clearColor = MTLClearColorMake( 1.0, 0.0, 0.0, 1.0 );
	pass.colorAttachments[0].storeAction = MTLStoreActionStore;
	pass.depthAttachment.texture = depth;
	pass.depthAttachment.loadAction = MTLLoadActionLoad;
	pass.depthAttachment.storeAction = MTLStoreActionDontCare;
	id<MTLRenderCommandEncoder> encoder = [command renderCommandEncoderWithDescriptor:pass];
	if ( !encoder ) return qfalse;
	encoder.label = @"wired.taa.history-trust";
	qboolean ok = qtrue;
	if ( entityBatchCount && entityVertices && entityIndices ) {
		/* The state the world material fragment reads besides the per-batch bindings DrawEntityBatches makes (as the translucent
		 * entity pass rebinds it on its own encoder). */
		const float sceneFog[4] = { viewParams->projection[2], (float)receipt->width, (float)receipt->height, 1.0f };
		[encoder setVertexBytes:viewParams length:sizeof( *viewParams ) atIndex:1u];
		[encoder setFragmentBytes:atmosphereParams length:sizeof( *atmosphereParams ) atIndex:1u];
		[encoder setFragmentBuffer:atmosphereArena offset:0u atIndex:2u];
		[encoder setFragmentTexture:present->skyRadianceConsumer.ready
			? present->skyIrradiance[0] : present->worldLightingFallbackTexture atIndex:17u];
		[encoder setFragmentTexture:present->skyRadianceConsumer.ready && present->skyRadianceConsumer.usePreviousProduct
			? present->skyIrradiance[1]
			: ( present->skyRadianceConsumer.ready ? present->skyIrradiance[0] : present->worldLightingFallbackTexture ) atIndex:18u];
		for ( uint32_t slot = 17u; slot <= 18u; ++slot )
			[encoder setFragmentSamplerState:present->skyRadianceConsumer.ready
				? present->skyRadianceSampler : present->uiClampSampler atIndex:slot];
		/* The redraw's counters (direct lights, local irradiance) belong to the scene pass, not to this one. */
		ralMetalPresentReceipt_t scratch = *receipt;
		ok = DrawEntityBatches( encoder, present, frontend, uniforms, entityVertices, entityIndices, entityBatches,
			entityBatchCount, sceneView, 7u, present->sceneDepthCache, sceneFog, &scratch, present->taaTrustPipelines );
		/* Object motion of the rigid entities with a previous pose (skipped above): their visible surfaces (depth-equal to the
		 * scene) write the screen velocity between the previous and the current unjittered view. */
		if ( ok && motionPrevious && motionViews ) {
			[encoder setRenderPipelineState:present->taaMotionPipeline];
			[encoder setDepthStencilState:present->worldDepthEqualReadState];
			[encoder setCullMode:MTLCullModeNone];
			[encoder setVertexBuffer:entityVertices offset:0u atIndex:0u];
			[encoder setVertexBytes:viewParams length:sizeof( *viewParams ) atIndex:1u];
			[encoder setVertexBytes:motionViews length:2u * sizeof( *viewParams ) atIndex:3u];
			[encoder setVertexBuffer:motionPrevious offset:0u atIndex:4u];
			for ( uint32_t b = 0u; b < entityBatchCount; ++b ) {
				const ralMetalEntityBatch_t *batch = &entityBatches[b];
				if ( !batch->motion || !batch->visible || !batch->indexCount ) continue;
				[encoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle indexCount:batch->indexCount
					indexType:MTLIndexTypeUInt32 indexBuffer:entityIndices
					indexBufferOffset:(NSUInteger)batch->firstIndex * sizeof( uint32_t )];
			}
		}
	}
	[encoder endEncoding];
	return ok;
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
			&& present->worldDepthEqualState && present->worldDepthEqualReadState
			&& present->worldPixelFormat == colorFormat ) return qtrue;
	[present->worldPipeline release]; present->worldPipeline = nil;
	[present->worldBlendPipeline release]; present->worldBlendPipeline = nil;
	[present->worldAlphaAdditivePipeline release];
	present->worldAlphaAdditivePipeline = nil;
	[present->worldAdditivePipeline release]; present->worldAdditivePipeline = nil;
	[present->worldDepthState release]; present->worldDepthState = nil;
	[present->worldDepthReadState release]; present->worldDepthReadState = nil;
	[present->worldDepthEqualState release]; present->worldDepthEqualState = nil;
	[present->worldDepthEqualReadState release]; present->worldDepthEqualReadState = nil;
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
	fragmentFunction = WorldFragmentFunction( present->uiLibrary, false, &error );
	if ( !vertexFunction || !fragmentFunction ) goto fail;
	{
		MTLVertexDescriptor *vertices = WorldVertexDescriptor();
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
		depth.depthCompareFunction = MTLCompareFunctionEqual;
		present->worldDepthEqualReadState = [device newDepthStencilStateWithDescriptor:depth];
		depth.depthWriteEnabled = YES;
		present->worldDepthEqualState = [device newDepthStencilStateWithDescriptor:depth];
	}
	[vertexFunction release]; vertexFunction = nil;
	[fragmentFunction release]; fragmentFunction = nil;
	if ( !present->worldPipeline || !present->worldBlendPipeline
			|| !present->worldAlphaAdditivePipeline
			|| !present->worldAdditivePipeline
			|| !present->worldDepthState || !present->worldDepthReadState
			|| !present->worldDepthEqualState || !present->worldDepthEqualReadState
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
	[present->worldDepthEqualState release]; present->worldDepthEqualState = nil;
	[present->worldDepthEqualReadState release]; present->worldDepthEqualReadState = nil;
	return qfalse;
}

static qboolean CollectSkyWorkCompletion( ralMetalPresent_t *present,
		qboolean wait, uint32_t *outCompletedWorkMask )
{
	MTLCommandBufferStatus status;
	if ( !present || !outCompletedWorkMask ) return qfalse;
	*outCompletedWorkMask = 0u;
	if ( !present->skyWorkCommand ) return qtrue;
	if ( wait ) [present->skyWorkCommand waitUntilCompleted];
	status = [present->skyWorkCommand status];
	if ( status == MTLCommandBufferStatusError ) {
		present->skyPhysicalTimingState = RAL_SKY_GPU_TIMING_FAILED;
		present->skyPhysicalTimingFrameGeneration =
			present->skySubmittedFrameGeneration;
		present->skyPhysicalGpuDurationNanoseconds = 0u;
		[present->skyWorkCommand release];
		present->skyWorkCommand = nil;
		present->skySubmittedWorkMask = 0u;
		return qfalse;
	}
	if ( status != MTLCommandBufferStatusCompleted ) return qtrue;
	{
		const CFTimeInterval start = [present->skyWorkCommand GPUStartTime];
		const CFTimeInterval end = [present->skyWorkCommand GPUEndTime];
		present->skyPhysicalTimingFrameGeneration =
			present->skySubmittedFrameGeneration;
		if ( end > start ) {
			present->skyPhysicalGpuDurationNanoseconds =
				(uint64_t)( ( end - start ) * 1000000000.0 + 0.5 );
			present->skyPhysicalTimingState = RAL_SKY_GPU_TIMING_VALID;
		} else {
			present->skyPhysicalGpuDurationNanoseconds = 0u;
			present->skyPhysicalTimingState = RAL_SKY_GPU_TIMING_FAILED;
		}
	}
	[present->skyWorkCommand release];
	present->skyWorkCommand = nil;
	*outCompletedWorkMask = present->skySubmittedWorkMask;
	present->skySubmittedWorkMask = 0u;
	return qtrue;
}

static qboolean CollectSkyRadianceCompletion( ralMetalPresent_t *present,
		qboolean wait, uint32_t *outCompletedSliceMask,
		uint64_t *outCompletedRevision )
{
	MTLCommandBufferStatus status;
	if ( !present || !outCompletedSliceMask || !outCompletedRevision ) return qfalse;
	*outCompletedSliceMask = 0u;
	*outCompletedRevision = 0u;
	if ( !present->skyRadianceCommand ) return qtrue;
	if ( wait ) [present->skyRadianceCommand waitUntilCompleted];
	status = [present->skyRadianceCommand status];
	if ( status == MTLCommandBufferStatusError ) {
		present->skyRadianceTimingState = RAL_SKY_GPU_TIMING_FAILED;
		present->skyRadianceTimingFrameGeneration =
			present->skyRadianceSubmittedFrameGeneration;
		present->skyRadianceGpuDurationNanoseconds = 0u;
		[present->skyRadianceCommand release];
		present->skyRadianceCommand = nil;
		present->skyRadianceSubmittedSliceMask = 0u;
		present->skyRadianceSubmittedRevision = 0u;
		return qfalse;
	}
	if ( status != MTLCommandBufferStatusCompleted ) return qtrue;
	{
		const CFTimeInterval start = [present->skyRadianceCommand GPUStartTime];
		const CFTimeInterval end = [present->skyRadianceCommand GPUEndTime];
		present->skyRadianceTimingFrameGeneration =
			present->skyRadianceSubmittedFrameGeneration;
		if ( end > start ) {
			present->skyRadianceGpuDurationNanoseconds =
				(uint64_t)( ( end - start ) * 1000000000.0 + 0.5 );
			present->skyRadianceTimingState = RAL_SKY_GPU_TIMING_VALID;
		} else {
			present->skyRadianceGpuDurationNanoseconds = 0u;
			present->skyRadianceTimingState = RAL_SKY_GPU_TIMING_FAILED;
		}
	}
	[present->skyRadianceCommand release];
	present->skyRadianceCommand = nil;
	*outCompletedSliceMask = present->skyRadianceSubmittedSliceMask;
	*outCompletedRevision = present->skyRadianceSubmittedRevision;
	present->skyRadianceSubmittedSliceMask = 0u;
	present->skyRadianceSubmittedRevision = 0u;
	return qtrue;
}

static void ReleaseSkyRadianceResources( ralMetalPresent_t *present )
{
	if ( !present ) return;
	[present->skySource release];
	[present->skyAuthored release];
	present->skyAuthored = nil;
	present->skyMaterialUsesTime = qfalse;
	for ( uint32_t face = 0u; face < RAL_MATERIAL_SKY_FACE_COUNT; ++face ) {
		[present->skyAuthoredFaces[face] release];
		present->skyAuthoredFaces[face] = nil;
	}
	for ( uint32_t i = 0u; i < 2u; ++i ) {
		[present->skyIrradiance[i] release];
		[present->skyRadiance[i] release];
		for ( uint32_t mip = 0u; mip < RAL_METAL_SKY_RADIANCE_MIPS; ++mip )
			[present->skyRadianceMipViews[i][mip] release];
		[present->skySh[i] release];
	}
	for ( uint32_t i = 0u; i < RAL_METAL_SKY_PHYSICAL_TEXTURE_COUNT; ++i )
		[present->skyPhysical[i] release];
	present->skySource = nil;
	memset( present->skyIrradiance, 0, sizeof( present->skyIrradiance ) );
	memset( present->skyRadiance, 0, sizeof( present->skyRadiance ) );
	memset( present->skyRadianceMipViews, 0,
		sizeof( present->skyRadianceMipViews ) );
	memset( present->skySh, 0, sizeof( present->skySh ) );
	memset( present->skyPhysical, 0, sizeof( present->skyPhysical ) );
	present->skyResourceWorldLifecycleGeneration = 0u;
	present->skyResourcesReady = qfalse;
}

static qboolean EnsureSkyRadianceResources( ralMetalPresent_t *present,
		uint64_t worldLifecycleGeneration )
{
	id<MTLDevice> device;
	id<MTLTexture> source = nil;
	id<MTLTexture> authored = nil;
	id<MTLTexture> authoredFaces[RAL_MATERIAL_SKY_FACE_COUNT] = {};
	id<MTLTexture> irradiance[2] = { nil, nil };
	id<MTLTexture> radiance[2] = { nil, nil };
	id<MTLTexture> radianceMipViews[2][RAL_METAL_SKY_RADIANCE_MIPS] = {};
	id<MTLBuffer> sh[2] = { nil, nil };
	id<MTLTexture> physical[RAL_METAL_SKY_PHYSICAL_TEXTURE_COUNT] = {
		nil, nil, nil };
	MTLTextureDescriptor *descriptor;
	uint32_t ignoredCompletion = 0u, ignoredSliceCompletion = 0u;
	uint64_t ignoredRevision = 0u;
	if ( !present || !worldLifecycleGeneration
			|| worldLifecycleGeneration == UINT64_MAX ) return qfalse;
	if ( present->skyResourcesReady
			&& present->skyResourceWorldLifecycleGeneration
				== worldLifecycleGeneration ) return qtrue;
	device = RalMetal_CoreNativeDevice( present->core );
	if ( !device ) return qfalse;
	if ( present->skyWorkCommand &&
		!CollectSkyWorkCompletion( present, qtrue,
			&ignoredCompletion ) ) return qfalse;
	if ( present->skyRadianceCommand &&
		!CollectSkyRadianceCompletion( present, qtrue,
			&ignoredSliceCompletion, &ignoredRevision ) ) return qfalse;

	descriptor = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:
		MTLPixelFormatRGBA16Float width:RAL_METAL_SKY_SOURCE_SIZE
		height:RAL_METAL_SKY_SOURCE_SIZE mipmapped:NO];
	descriptor.textureType = MTLTextureType2DArray;
	descriptor.arrayLength = RAL_SKY_RADIANCE_CAPTURE_FACE_COUNT;
	descriptor.storageMode = MTLStorageModePrivate;
	descriptor.usage = MTLTextureUsageShaderRead | MTLTextureUsageShaderWrite;
	source = [device newTextureWithDescriptor:descriptor];
	if ( !source ) goto fail;
	descriptor.usage = MTLTextureUsageShaderRead | MTLTextureUsageRenderTarget
		| MTLTextureUsagePixelFormatView;
	authored = [device newTextureWithDescriptor:descriptor];
	if ( !authored ) goto fail;
	for ( uint32_t face = 0u; face < RAL_MATERIAL_SKY_FACE_COUNT; ++face ) {
		authoredFaces[face] = [authored newTextureViewWithPixelFormat:MTLPixelFormatRGBA16Float
			textureType:MTLTextureType2D levels:NSMakeRange( 0u, 1u )
			slices:NSMakeRange( face, 1u )];
		if ( !authoredFaces[face] ) goto fail;
	}
	descriptor = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:
		MTLPixelFormatRGBA16Float width:RAL_METAL_SKY_IRRADIANCE_SIZE
		height:RAL_METAL_SKY_IRRADIANCE_SIZE mipmapped:NO];
	descriptor.textureType = MTLTextureType2DArray;
	descriptor.arrayLength = RAL_SKY_RADIANCE_CAPTURE_FACE_COUNT;
	descriptor.storageMode = MTLStorageModePrivate;
	descriptor.usage = MTLTextureUsageShaderRead | MTLTextureUsageShaderWrite;
	for ( uint32_t i = 0u; i < 2u; ++i ) {
		irradiance[i] = [device newTextureWithDescriptor:descriptor];
		if ( !irradiance[i] ) goto fail;
	}
	descriptor = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:
		MTLPixelFormatRGBA16Float width:WIRED_SKY_SPECULAR_SIZE
		height:WIRED_SKY_SPECULAR_SIZE mipmapped:YES];
	descriptor.textureType = MTLTextureType2DArray;
	descriptor.arrayLength = RAL_SKY_RADIANCE_CAPTURE_FACE_COUNT;
	descriptor.mipmapLevelCount = 8u;
	descriptor.storageMode = MTLStorageModePrivate;
	descriptor.usage = MTLTextureUsageShaderRead | MTLTextureUsageShaderWrite;
	for ( uint32_t i = 0u; i < 2u; ++i ) {
		radiance[i] = [device newTextureWithDescriptor:descriptor];
		if ( !radiance[i] ) goto fail;
		for ( uint32_t mip = 0u; mip < RAL_METAL_SKY_RADIANCE_MIPS; ++mip ) {
			radianceMipViews[i][mip] = [radiance[i]
				newTextureViewWithPixelFormat:MTLPixelFormatRGBA16Float
				textureType:MTLTextureType2DArray
				levels:NSMakeRange( mip, 1u )
				slices:NSMakeRange( 0u,
					RAL_SKY_RADIANCE_CAPTURE_FACE_COUNT )];
			if ( !radianceMipViews[i][mip] ) goto fail;
		}
		sh[i] = [device newBufferWithLength:
			RAL_SKY_RADIANCE_AMBIENT_SH_COEFFICIENT_COUNT * 4u * sizeof( float )
			options:MTLResourceStorageModePrivate];
		if ( !sh[i] ) goto fail;
	}
	descriptor = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:
		MTLPixelFormatRGBA16Float width:RAL_METAL_SKY_TRANSMITTANCE_WIDTH
		height:RAL_METAL_SKY_TRANSMITTANCE_HEIGHT mipmapped:NO];
	descriptor.storageMode = MTLStorageModePrivate;
	descriptor.usage = MTLTextureUsageShaderRead | MTLTextureUsageShaderWrite;
	physical[RAL_METAL_SKY_TRANSMITTANCE] =
		[device newTextureWithDescriptor:descriptor];
	descriptor = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:
		MTLPixelFormatRGBA16Float width:RAL_METAL_SKY_MULTISCATTER_SIZE
		height:RAL_METAL_SKY_MULTISCATTER_SIZE mipmapped:NO];
	descriptor.storageMode = MTLStorageModePrivate;
	descriptor.usage = MTLTextureUsageShaderRead | MTLTextureUsageShaderWrite;
	physical[RAL_METAL_SKY_MULTISCATTER] =
		[device newTextureWithDescriptor:descriptor];
	descriptor = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:
		MTLPixelFormatRGBA16Float width:RAL_METAL_SKY_VIEW_WIDTH
		height:RAL_METAL_SKY_VIEW_HEIGHT mipmapped:NO];
	descriptor.storageMode = MTLStorageModePrivate;
	descriptor.usage = MTLTextureUsageShaderRead | MTLTextureUsageShaderWrite;
	physical[RAL_METAL_SKY_VIEW] = [device newTextureWithDescriptor:descriptor];
	for ( uint32_t i = 0u; i < RAL_METAL_SKY_PHYSICAL_TEXTURE_COUNT; ++i )
		if ( !physical[i] ) goto fail;

	ReleaseSkyRadianceResources( present );
	present->skySource = source;
	present->skyAuthored = authored;
	memcpy( present->skyAuthoredFaces, authoredFaces, sizeof( authoredFaces ) );
	for ( uint32_t i = 0u; i < 2u; ++i ) {
		present->skyIrradiance[i] = irradiance[i];
		present->skyRadiance[i] = radiance[i];
		memcpy( present->skyRadianceMipViews[i], radianceMipViews[i],
			sizeof( radianceMipViews[i] ) );
		present->skySh[i] = sh[i];
	}
	memcpy( present->skyPhysical, physical, sizeof( physical ) );
	present->skyResourceWorldLifecycleGeneration = worldLifecycleGeneration;
	present->skyResourcesReady = qtrue;
	return qtrue;

fail:
	[source release];
	[authored release];
	for ( uint32_t face = 0u; face < RAL_MATERIAL_SKY_FACE_COUNT; ++face )
		[authoredFaces[face] release];
	for ( uint32_t i = 0u; i < 2u; ++i ) {
		[irradiance[i] release];
		[radiance[i] release];
		for ( uint32_t mip = 0u; mip < RAL_METAL_SKY_RADIANCE_MIPS; ++mip )
			[radianceMipViews[i][mip] release];
		[sh[i] release];
	}
	for ( uint32_t i = 0u; i < RAL_METAL_SKY_PHYSICAL_TEXTURE_COUNT; ++i )
		[physical[i] release];
	return qfalse;
}

qboolean RalMetal_PresentUnloadSkyRadianceResources(
		ralMetalPresent_t *present )
{
	uint32_t ignoredCompletion = 0u, ignoredSliceCompletion = 0u;
	uint64_t ignoredRevision = 0u;
	if ( !present ) return qfalse;
	if ( present->skyWorkCommand &&
		!CollectSkyWorkCompletion( present, qtrue,
			&ignoredCompletion ) ) return qfalse;
	if ( present->skyRadianceCommand &&
		!CollectSkyRadianceCompletion( present, qtrue,
			&ignoredSliceCompletion, &ignoredRevision ) ) return qfalse;
	ReleaseSkyRadianceResources( present );
	present->skySubmittedWorkMask = 0u;
	present->skyRadianceSubmittedSliceMask = 0u;
	present->skyRadianceSubmittedRevision = 0u;
	present->skyIrradianceWriteStarted = qfalse;
	present->skyRadianceWriteStarted = qfalse;
	present->skyRadianceFrameGeneration = 0u;
	present->skyRadianceTimeline = 0.0f;
	memset( &present->skyRadianceReceipt, 0,
		sizeof( present->skyRadianceReceipt ) );
	memset( &present->skyRadianceConsumer, 0,
		sizeof( present->skyRadianceConsumer ) );
	present->skyWorldLifecycleGeneration = 0u;
	memset( &present->atmosphereReceipt, 0,
		sizeof( present->atmosphereReceipt ) );
	present->atmosphereReceiptWidth = 0u;
	present->atmosphereReceiptHeight = 0u;
	if ( !Ral_SkyEnvironmentRuntimeInit( RAL_BACKEND_METAL,
			&present->skyRuntime ) ) return qfalse;
	return Ral_SkyRadianceInit( RAL_BACKEND_METAL,
		&present->skyRadianceState );
}

static qboolean EnsureSkyPhysicalPipeline( ralMetalPresent_t *present )
{
	static NSString *const names[RAL_METAL_SKY_PHYSICAL_TEXTURE_COUNT] = {
		@"wired_sky_transmittance_compute",
		@"wired_sky_multiscatter_compute",
		@"wired_sky_view_compute"
	};
	id<MTLDevice> device;
	id<MTLComputePipelineState> pipelines[RAL_METAL_SKY_PHYSICAL_TEXTURE_COUNT] = {
		nil, nil, nil };
	id<MTLSamplerState> sampler = nil;
	NSError *error = nil;
	if ( !present ) return qfalse;
	if ( present->skyPhysicalSampler
			&& present->skyPhysicalPipeline[RAL_METAL_SKY_TRANSMITTANCE]
			&& present->skyPhysicalPipeline[RAL_METAL_SKY_MULTISCATTER]
			&& present->skyPhysicalPipeline[RAL_METAL_SKY_VIEW] ) return qtrue;
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
	for ( uint32_t i = 0u; i < RAL_METAL_SKY_PHYSICAL_TEXTURE_COUNT; ++i ) {
		id<MTLFunction> function = [present->uiLibrary
			newFunctionWithName:names[i]];
		if ( !function ) goto fail;
		pipelines[i] = [device newComputePipelineStateWithFunction:function
			error:&error];
		[function release];
		if ( !pipelines[i] || error ) goto fail;
	}
	{
		MTLSamplerDescriptor *descriptor = [[MTLSamplerDescriptor alloc] init];
		descriptor.minFilter = MTLSamplerMinMagFilterLinear;
		descriptor.magFilter = MTLSamplerMinMagFilterLinear;
		descriptor.sAddressMode = MTLSamplerAddressModeClampToEdge;
		descriptor.tAddressMode = MTLSamplerAddressModeClampToEdge;
		sampler = [device newSamplerStateWithDescriptor:descriptor];
		[descriptor release];
	}
	if ( !sampler ) goto fail;
	memcpy( present->skyPhysicalPipeline, pipelines, sizeof( pipelines ) );
	present->skyPhysicalSampler = sampler;
	return qtrue;
fail:
	for ( uint32_t i = 0u; i < RAL_METAL_SKY_PHYSICAL_TEXTURE_COUNT; ++i )
		[pipelines[i] release];
	[sampler release];
	return qfalse;
}

static qboolean EnsureSkyRadiancePipeline( ralMetalPresent_t *present )
{
	static const unsigned char *const bytes[RAL_METAL_SKY_RADIANCE_PIPELINE_COUNT] = {
		ral_metal_sky_source_metallib,
		ral_metal_sky_project_sh_metallib,
		ral_metal_sky_irradiance_metallib,
		ral_metal_sky_prefilter_metallib
	};
	static const size_t sizes[RAL_METAL_SKY_RADIANCE_PIPELINE_COUNT] = {
		ral_metal_sky_source_metallib_size,
		ral_metal_sky_project_sh_metallib_size,
		ral_metal_sky_irradiance_metallib_size,
		ral_metal_sky_prefilter_metallib_size
	};
	id<MTLDevice> device;
	NSError *error = nil;
	if ( !present ) return qfalse;
	if ( present->skyRadianceSampler ) {
		qboolean complete = qtrue;
		for ( uint32_t i = 0u; i < RAL_METAL_SKY_RADIANCE_PIPELINE_COUNT; ++i )
			complete = ( complete && present->skyRadianceLibrary[i]
				&& present->skyRadiancePipeline[i]
				&& present->skyRadianceArguments[i] ) ? qtrue : qfalse;
		if ( complete ) return qtrue;
	}
	device = RalMetal_CoreNativeDevice( present->core );
	if ( !device ) return qfalse;
	for ( uint32_t i = 0u; i < RAL_METAL_SKY_RADIANCE_PIPELINE_COUNT; ++i ) {
		id<MTLFunction> function;
		[present->skyRadianceArguments[i] release];
		[present->skyRadiancePipeline[i] release];
		[present->skyRadianceLibrary[i] release];
		present->skyRadianceArguments[i] = nil;
		present->skyRadiancePipeline[i] = nil;
		present->skyRadianceLibrary[i] = NewEmbeddedLibrary( device,
			bytes[i], sizes[i] );
		if ( !present->skyRadianceLibrary[i] ) goto fail;
		function = [present->skyRadianceLibrary[i]
			newFunctionWithName:@"main0"];
		if ( !function ) goto fail;
		present->skyRadiancePipeline[i] = [device
			newComputePipelineStateWithFunction:function error:&error];
		present->skyRadianceArguments[i] = [function
			newArgumentEncoderWithBufferIndex:0u];
		[function release];
		if ( !present->skyRadiancePipeline[i] || !present->skyRadianceArguments[i]
				|| error ) goto fail;
	}
	if ( !present->skyRadianceSampler ) {
		MTLSamplerDescriptor *descriptor = [[MTLSamplerDescriptor alloc] init];
		descriptor.minFilter = MTLSamplerMinMagFilterLinear;
		descriptor.magFilter = MTLSamplerMinMagFilterLinear;
		descriptor.mipFilter = MTLSamplerMipFilterLinear;
		descriptor.sAddressMode = MTLSamplerAddressModeClampToEdge;
		descriptor.tAddressMode = MTLSamplerAddressModeClampToEdge;
		present->skyRadianceSampler = [device
			newSamplerStateWithDescriptor:descriptor];
		[descriptor release];
	}
	return present->skyRadianceSampler ? qtrue : qfalse;
fail:
	for ( uint32_t i = 0u; i < RAL_METAL_SKY_RADIANCE_PIPELINE_COUNT; ++i ) {
		[present->skyRadianceArguments[i] release];
		[present->skyRadiancePipeline[i] release];
		[present->skyRadianceLibrary[i] release];
		present->skyRadianceArguments[i] = nil;
		present->skyRadiancePipeline[i] = nil;
		present->skyRadianceLibrary[i] = nil;
	}
	return qfalse;
}

static qboolean EncodeSkyPhysical( ralMetalPresent_t *present,
		const renderAtmosphereSnapshot_t *snapshot, uint32_t workMask,
		uint64_t frameGeneration )
{
	const uint32_t physicalMask = RAL_SKY_DIRTY_TRANSMITTANCE |
		RAL_SKY_DIRTY_MULTISCATTER | RAL_SKY_DIRTY_SKY_VIEW;
	const atmosphereSkyEnvironment_t *sky;
	ralMetalSkyPhysicalParams_t params;
	id<MTLCommandQueue> queue;
	id<MTLCommandBuffer> command;
	id<MTLComputeCommandEncoder> encoder;
	uint32_t pipelineIndex, width, height;
	if ( !workMask ) return qtrue;
	if ( !present || !snapshot || !present->skyResourcesReady
			|| present->skyWorkCommand || ( workMask & ~physicalMask ) != 0u
			|| ( workMask & ( workMask - 1u ) ) != 0u
			|| !EnsureSkyPhysicalPipeline( present ) ) return qfalse;
	if ( workMask == RAL_SKY_DIRTY_TRANSMITTANCE ) {
		pipelineIndex = RAL_METAL_SKY_TRANSMITTANCE;
		width = RAL_METAL_SKY_TRANSMITTANCE_WIDTH;
		height = RAL_METAL_SKY_TRANSMITTANCE_HEIGHT;
	} else if ( workMask == RAL_SKY_DIRTY_MULTISCATTER ) {
		pipelineIndex = RAL_METAL_SKY_MULTISCATTER;
		width = height = RAL_METAL_SKY_MULTISCATTER_SIZE;
	} else {
		pipelineIndex = RAL_METAL_SKY_VIEW;
		width = RAL_METAL_SKY_VIEW_WIDTH;
		height = RAL_METAL_SKY_VIEW_HEIGHT;
	}
	sky = &snapshot->state.skyEnvironment;
	memset( &params, 0, sizeof( params ) );
	memcpy( params.sunDirectionAngularRadius, sky->sunDirection,
		3u * sizeof( float ) );
	params.sunDirectionAngularRadius[3] = sky->sunAngularRadius;
	memcpy( params.sunRadiance, sky->sunRadiance, 3u * sizeof( float ) );
	memcpy( params.moonDirectionAngularRadius, sky->moonDirection,
		3u * sizeof( float ) );
	params.moonDirectionAngularRadius[3] = sky->moonAngularRadius;
	memcpy( params.moonRadiance, sky->moonRadiance, 3u * sizeof( float ) );
	params.atmosphereRadii[0] = 6360.0f;
	params.atmosphereRadii[1] = 6460.0f;
	params.scatteringScale[0] = 1.0f;
	params.scatteringScale[1] = 1.0f;
	params.scatteringScale[2] = 1.0f;
	params.scatteringScale[3] = 0.8f;
	params.groundExposure[0] = sky->ambientSh[0];
	params.groundExposure[1] = sky->ambientSh[1];
	params.groundExposure[2] = sky->ambientSh[2];
	params.groundExposure[3] = sky->exposure;
	queue = RalMetal_CoreNativeQueue( present->core );
	command = queue ? [queue commandBuffer] : nil;
	encoder = command ? [command computeCommandEncoder] : nil;
	if ( !encoder ) return qfalse;
	[encoder setComputePipelineState:present->skyPhysicalPipeline[pipelineIndex]];
	[encoder setBytes:&params length:sizeof( params ) atIndex:0u];
	if ( pipelineIndex == RAL_METAL_SKY_TRANSMITTANCE ) {
		[encoder setTexture:present->skyPhysical[RAL_METAL_SKY_TRANSMITTANCE]
			atIndex:0u];
	} else if ( pipelineIndex == RAL_METAL_SKY_MULTISCATTER ) {
		[encoder setTexture:present->skyPhysical[RAL_METAL_SKY_TRANSMITTANCE]
			atIndex:0u];
		[encoder setTexture:present->skyPhysical[RAL_METAL_SKY_MULTISCATTER]
			atIndex:1u];
		[encoder setSamplerState:present->skyPhysicalSampler atIndex:0u];
	} else {
		[encoder setTexture:present->skyPhysical[RAL_METAL_SKY_TRANSMITTANCE]
			atIndex:0u];
		[encoder setTexture:present->skyPhysical[RAL_METAL_SKY_MULTISCATTER]
			atIndex:1u];
		[encoder setTexture:present->skyPhysical[RAL_METAL_SKY_VIEW]
			atIndex:2u];
		[encoder setSamplerState:present->skyPhysicalSampler atIndex:0u];
	}
	[encoder dispatchThreads:MTLSizeMake( width, height, 1u )
		threadsPerThreadgroup:MTLSizeMake( 8u, 8u, 1u )];
	[encoder endEncoding];
	present->skyWorkCommand = [command retain];
	present->skySubmittedWorkMask = workMask;
	present->skySubmittedFrameGeneration = frameGeneration;
	present->skyPhysicalTimingState = RAL_SKY_GPU_TIMING_PENDING;
	present->skyPhysicalTimingFrameGeneration = frameGeneration;
	present->skyPhysicalGpuDurationNanoseconds = 0u;
	present->skyPhysicalDispatchCount++;
	[command commit];
	return qtrue;
}

static id<MTLBuffer> NewSkyArgumentBuffer( id<MTLDevice> device,
		id<MTLArgumentEncoder> arguments )
{
	if ( !device || !arguments || ![arguments encodedLength] ) return nil;
	return [device newBufferWithLength:[arguments encodedLength]
		options:MTLResourceStorageModeShared];
}

/* Render the authored program, not a second approximation of its stages.
 * One fixed HDR pipeline reuses the visible world's shaders/vertex ABI; only
 * the target format/depth attachment differs from normal world submission. */
static qboolean EnsureSkyCapturePipeline( ralMetalPresent_t *present ) {
	if ( present->skyCapturePipeline ) return qtrue;
	id<MTLDevice> device = RalMetal_CoreNativeDevice( present->core );
	if ( !device || !EnsureUiPipeline( present ) ) return qfalse;
	id<MTLFunction> vertex = [present->uiLibrary newFunctionWithName:@"wired_world_vertex"];
	id<MTLFunction> fragment = WorldFragmentFunction( present->uiLibrary, false, NULL );
	if ( !vertex || !fragment ) { [vertex release]; [fragment release]; return qfalse; }
	MTLRenderPipelineDescriptor *descriptor =
		[[[MTLRenderPipelineDescriptor alloc] init] autorelease];
	descriptor.vertexFunction = vertex; descriptor.fragmentFunction = fragment;
	descriptor.vertexDescriptor = WorldVertexDescriptor();
	descriptor.colorAttachments[0].pixelFormat = MTLPixelFormatRGBA16Float;
	NSError *error = nil;
	present->skyCapturePipeline = [device newRenderPipelineStateWithDescriptor:descriptor error:&error];
	[vertex release]; [fragment release];
	if ( error || !present->skyCapturePipeline ) {
		[present->skyCapturePipeline release]; present->skyCapturePipeline = nil;
		return qfalse;
	}
	return qtrue;
}

static qboolean EncodeAuthoredSkyFace( ralMetalPresent_t *present,
		id<MTLCommandBuffer> command, const renderSubmissionState_t *frontend,
		const renderMaterialSnapshot_t *material, uint32_t face, qboolean physical ) {
	float axes[3][3];
	ralMetalWorldViewParams_t view = {};
	ralMetalWorldMaterialParams_t params = {};
	ralMetalWorldAtmosphereParams_t atmosphere = {};
	ralMetalWorldVertex_t vertices[4] = {};
	id<MTLTexture> base, stageTextures[RENDER_MATERIAL_MAX_STAGES];
	id<MTLSamplerState> baseSampler, stageSamplers[RENDER_MATERIAL_MAX_STAGES];
	qboolean textured, msdf;
	if ( !Ral_SkyRadianceCaptureAxes( face, axes )
			|| !EnsureSkyCapturePipeline( present )
			|| !UiMaterial( present, frontend, material->handle,
				&base, &baseSampler, &textured, &msdf ) || !textured || msdf ) return qfalse;
	SetDefaultMaterialFactors( &params );
	params.hasLightmap = 4u | ( material->skyBox ? 16u : 0u );
	params.alphaMode = RENDER_ALPHA_OPAQUE;
	params.skyCloudHeight = material->skyCloudHeight;
	params.skyTime = present->skyCaptureTime;
	params.lightmapBoost = 1.0f;
	/* Capture the target composition; the radiance consumer owns temporal
	 * crossfade. Baking an intermediate display fade would apply it twice. */
	atmosphere.skyControl[0] = physical ? 1.0f : 0.0f;
	params.stageProgram[0] = material->stageCount;
	memcpy( params.skyScaleScroll, material->skyScaleScroll, sizeof( params.skyScaleScroll ) );
	if ( material->lighting.schemaVersion == RENDER_MATERIAL_LIGHTING_SCHEMA_VERSION )
		for ( uint32_t c = 0u; c < 3u; ++c )
			params.emissiveRadiance[c] = (float)material->lighting.emissionRadianceQ16[c]
				/ (float)RENDER_MATERIAL_LIGHTING_Q16_ONE;
	for ( uint32_t slot = 0u; slot < RENDER_MATERIAL_MAX_STAGES; ++slot ) {
		stageTextures[slot] = present->uiFallbackTexture;
		stageSamplers[slot] = present->uiClampSampler;
		if ( slot >= material->stageCount ) continue;
		const renderMaterialStageSnapshot_t *stage = &material->stages[slot];
		wired_material_stage_pack( stage, &params.stages[slot] );
		if ( stage->imageSource == 0u
				&& ( !UiMaterial( present, frontend, MaterialStageHandleAtTime( stage, params.skyTime ),
					&stageTextures[slot], &stageSamplers[slot], &textured, &msdf )
					|| !textured || msdf ) ) return qfalse;
	}
	memcpy( view.forward, axes[0], sizeof( axes[0] ) );
	memcpy( view.left, axes[1], sizeof( axes[1] ) );
	memcpy( view.up, axes[2], sizeof( axes[2] ) );
	view.projection[0] = view.projection[1] = view.projection[2] = 1.0f;
	for ( uint32_t i = 0u; i < 4u; ++i ) {
		const float x = ( i & 1u ) ? 1.0f : -1.0f;
		const float y = ( i & 2u ) ? 1.0f : -1.0f;
		for ( uint32_t c = 0u; c < 3u; ++c ) {
			vertices[i].worldPosition[c] = 1024.0f * ( axes[0][c] - x * axes[1][c] + y * axes[2][c] );
			vertices[i].materialPosition[c] = vertices[i].worldPosition[c];
			vertices[i].normal[c] = -axes[0][c];
		}
		for ( uint32_t c = 0u; c < 4u; ++c ) vertices[i].color[c] = 1.0f;
	}
	MTLRenderPassDescriptor *pass = [MTLRenderPassDescriptor renderPassDescriptor];
	pass.colorAttachments[0].texture = present->skyAuthoredFaces[face];
	pass.colorAttachments[0].loadAction = MTLLoadActionClear;
	pass.colorAttachments[0].storeAction = MTLStoreActionStore;
	pass.colorAttachments[0].clearColor = MTLClearColorMake( 0, 0, 0, 1 );
	id<MTLRenderCommandEncoder> encoder = [command renderCommandEncoderWithDescriptor:pass];
	if ( !encoder ) return qfalse;
	[encoder setRenderPipelineState:present->skyCapturePipeline];
	[encoder setCullMode:MTLCullModeNone];
	[encoder setVertexBytes:vertices length:sizeof( vertices ) atIndex:0u];
	[encoder setVertexBytes:&view length:sizeof( view ) atIndex:1u];
	[encoder setVertexBytes:&params length:sizeof( params ) atIndex:2u];
	[encoder setFragmentBytes:&params length:sizeof( params ) atIndex:0u];
	[encoder setFragmentBytes:&atmosphere length:sizeof( atmosphere ) atIndex:1u];
	[encoder setFragmentBuffer:present->atmosphereFallback offset:0u atIndex:2u];
	[encoder setFragmentBytes:s_sceneFogOff length:sizeof( s_sceneFogOff ) atIndex:3u];
	[encoder setFragmentTexture:SceneDepthOrFallback( present ) atIndex:19u];
	[encoder setFragmentTexture:base atIndex:0u];
	[encoder setFragmentSamplerState:baseSampler atIndex:0u];
	[encoder setFragmentTexture:present->uiFallbackTexture atIndex:1u];
	[encoder setFragmentTexture:present->uiFallbackTexture atIndex:5u];
	[encoder setFragmentTexture:present->uiFallbackTexture atIndex:14u];
	for ( uint32_t slot = 2u; slot <= 4u; ++slot )
		[encoder setFragmentTexture:present->worldLightingFallbackTexture atIndex:slot];
	for ( uint32_t slot = 15u; slot <= 18u; ++slot )
		[encoder setFragmentTexture:present->worldLightingFallbackTexture atIndex:slot];
	for ( uint32_t slot = 1u; slot < 16u; ++slot )
		[encoder setFragmentSamplerState:present->uiClampSampler atIndex:slot];
	if ( physical ) {
		[encoder setFragmentTexture:present->skyPhysical[RAL_METAL_SKY_VIEW] atIndex:14u];
		[encoder setFragmentSamplerState:present->skyPhysicalSampler atIndex:14u];
	}
	for ( uint32_t slot = 0u; slot < RENDER_MATERIAL_MAX_STAGES; ++slot ) {
		[encoder setFragmentTexture:stageTextures[slot] atIndex:6u + slot];
		[encoder setFragmentSamplerState:stageSamplers[slot] atIndex:6u + slot];
	}
	[encoder drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:0u vertexCount:4u];
	[encoder endEncoding];
	return qtrue;
}

static qboolean EncodeSkyRadiance( ralMetalPresent_t *present,
		const renderSubmissionState_t *frontend,
		const ralSkyEnvironmentPlanReceipt_t *skyPlan,
		ralAtmosphereTier_t tier, uint32_t eventMask,
		float timelineSeconds )
{
	ralSkyTierBudgetReceipt_t budget;
	ralSkyRadianceRequest_t request;
	renderMaterialSnapshot_t material;
	renderAtmosphereSnapshot_t atmosphere;
	const atmosphereEmitter_t *atmosphereEmitters;
	char semanticName[MAX_QPATH];
	id<MTLDevice> device;
	id<MTLCommandQueue> queue;
	id<MTLCommandBuffer> command;
	id<MTLComputeCommandEncoder> encoder;
	uint32_t completedMask = 0u, admittedMask;
	uint64_t completedRevision = 0u;
	float deltaSeconds;
	qboolean authoredReady = qfalse;
	if ( !present || !frontend || !skyPlan
			|| !Ral_SkyEnvironmentPlanReceiptExact( skyPlan, skyPlan ) )
		return PresentFailure( "sky-radiance-precondition" );
	/* Zero LUT work is the normal steady state, not zero radiance work:
	 * an admitted capture/convolution cohort must still drain, and animated
	 * materials may refresh without invalidating any atmosphere LUT. Only a
	 * registration-only plan without world-owned resources is a no-op here. */
	if ( skyPlan->zeroWork && !present->skyResourcesReady ) return qtrue;
	if ( !RenderSubmission_AtmosphereSnapshot( frontend, &atmosphere,
			&atmosphereEmitters ) ) return PresentFailure( "sky-radiance-atmosphere" );
	if ( !present->skyResourcesReady )
		return PresentFailure( "sky-radiance-resources" );
	if ( !Ral_SkyTierBudget( tier, &budget )
			|| !Ral_SkyTierBudgetReceiptExact( &budget, &budget ) )
		return PresentFailure( "sky-radiance-budget" );
	if ( !budget.maxRadianceSlicesPerFrame ) {
		/* Residency is not admission. OFF/analytic must look the same before
		 * and after weather, even if a completed IBL cohort remains cached.
		 * Keep its GPU ownership/completion intact for a later enabled tier. */
		return Ral_SkyRadianceConsumerAdmit( &budget,
			&present->skyRadianceConsumer, &present->skyRadianceConsumer );
	}
	const qboolean physicalCapture = skyPlan->selectedMode == RAL_SKY_ENVIRONMENT_PHYSICAL
		? qtrue : qfalse;
	const qboolean physicalSourceReady = Ral_SkyPhysicalSourceReady( skyPlan,
			present->skyPhysical[RAL_METAL_SKY_VIEW] && present->skyPhysicalSampler
				? qtrue : qfalse );
	if ( !EnsureSkyRadiancePipeline( present ) )
		return PresentFailure( "sky-radiance-pipeline" );
	if ( !CollectSkyRadianceCompletion( present, qfalse, &completedMask,
			&completedRevision ) )
		return PresentFailure( "sky-radiance-completion" );
	deltaSeconds = timelineSeconds >= present->skyRadianceTimeline
		? timelineSeconds - present->skyRadianceTimeline : 0.0f;
	present->skyRadianceTimeline = timelineSeconds;
	if ( present->skyRadianceFrameGeneration == UINT64_MAX ) return qfalse;
	memset( &request, 0, sizeof( request ) );
	request.schemaVersion = RAL_SKY_RADIANCE_SCHEMA_VERSION;
	request.backendType = RAL_BACKEND_METAL;
	request.frameGeneration = ++present->skyRadianceFrameGeneration;
	request.sourceRevision = skyPlan->sourceRevision;
	request.completedProductRevision = completedRevision;
	request.mode = skyPlan->selectedMode;
	request.specularMipCount = RAL_METAL_SKY_RADIANCE_MIPS;
	request.eventMask = eventMask;
	request.completedSliceMask = completedMask;
	request.maxSlicesPerFrame = present->skyRadianceCommand ? 0u
		: budget.maxRadianceSlicesPerFrame;
	/* Do not publish an authored-only first cohort while physical LUTs are
	 * still being built. Poll existing work normally, without a CPU/GPU wait. */
	if ( physicalCapture && !physicalSourceReady ) request.maxSlicesPerFrame = 0u;
	request.deltaSeconds = deltaSeconds;
	request.crossfadeSeconds = 0.35f;
	/* Only the real ordered-material capture opts in. Static/outer-only sky
	 * has no periodic GPU cost, and a pinned shader clock is immutable. */
	request.refreshIntervalSeconds = present->skyMaterialUsesTime
		&& present->shaderTimeOverride <= 0.0f ? 0.5f : 0.0f;
	if ( !Ral_SkyRadianceAdvance( &present->skyRadianceState, &request,
			&present->skyRadianceReceipt ) ) {
		fprintf( stderr, "Wired native Metal RAL: sky-radiance request frame=%llu source=%llu mode=%u mips=%u event=0x%x completed=0x%x revision=%llu budget=%u dt=%g\n",
			(unsigned long long)request.frameGeneration,
			(unsigned long long)request.sourceRevision, (unsigned)request.mode,
			request.specularMipCount, request.eventMask,
			request.completedSliceMask,
			(unsigned long long)request.completedProductRevision,
			request.maxSlicesPerFrame, request.deltaSeconds );
		return PresentFailure( "sky-radiance-advance" );
	}
	if ( present->skyRadianceReceipt.sourceChanged
			|| present->skyRadianceReceipt.refreshStarted || eventMask ) {
		present->skyIrradianceWriteStarted = qfalse;
		present->skyRadianceWriteStarted = qfalse;
	}
	if ( present->skyRadianceReceipt.targetPublished ) {
		id<MTLTexture> textureSwap;
		id<MTLBuffer> bufferSwap;
		textureSwap = present->skyIrradiance[0];
		present->skyIrradiance[0] = present->skyIrradiance[1];
		present->skyIrradiance[1] = textureSwap;
		textureSwap = present->skyRadiance[0];
		present->skyRadiance[0] = present->skyRadiance[1];
		present->skyRadiance[1] = textureSwap;
		for ( uint32_t mip = 0u; mip < RAL_METAL_SKY_RADIANCE_MIPS; ++mip ) {
			textureSwap = present->skyRadianceMipViews[0][mip];
			present->skyRadianceMipViews[0][mip] =
				present->skyRadianceMipViews[1][mip];
			present->skyRadianceMipViews[1][mip] = textureSwap;
		}
		bufferSwap = present->skySh[0];
		present->skySh[0] = present->skySh[1];
		present->skySh[1] = bufferSwap;
		present->skyIrradianceWriteStarted = qfalse;
		present->skyRadianceWriteStarted = qfalse;
	}
	if ( present->skyRadianceReceipt.currentProductRevision ) {
		const uint32_t currentReadable =
			( present->skyRadiance[0]
				? RAL_SKY_RADIANCE_PRODUCT_VISIBLE_RADIANCE
					| RAL_SKY_RADIANCE_PRODUCT_SPECULAR_RADIANCE : 0u )
			| ( present->skyIrradiance[0]
				? RAL_SKY_RADIANCE_PRODUCT_DIFFUSE_IRRADIANCE : 0u )
			| ( present->skySh[0]
				? RAL_SKY_RADIANCE_PRODUCT_AMBIENT_SH : 0u );
		const uint32_t previousReadable =
			present->skyRadianceReceipt.previousProductValid
			? ( present->skyRadiance[1]
				? RAL_SKY_RADIANCE_PRODUCT_VISIBLE_RADIANCE
					| RAL_SKY_RADIANCE_PRODUCT_SPECULAR_RADIANCE : 0u )
				| ( present->skyIrradiance[1]
					? RAL_SKY_RADIANCE_PRODUCT_DIFFUSE_IRRADIANCE : 0u )
				| ( present->skySh[1]
					? RAL_SKY_RADIANCE_PRODUCT_AMBIENT_SH : 0u )
			: 0u;
		if ( !Ral_SkyRadianceConsumerPlan( &present->skyRadianceReceipt,
				currentReadable, previousReadable,
				&present->skyRadianceConsumer ) )
			return PresentFailure( "sky-radiance-consumer" );
	} else memset( &present->skyRadianceConsumer, 0,
		sizeof( present->skyRadianceConsumer ) );
	admittedMask = present->skyRadianceReceipt.admittedSliceMask;
	if ( !admittedMask ) return qtrue;
	if ( present->skyRadianceCommand ) return qfalse;
	device = RalMetal_CoreNativeDevice( present->core );
	queue = RalMetal_CoreNativeQueue( present->core );
	command = queue ? [queue commandBuffer] : nil;
	if ( !device || !command )
		return PresentFailure( "sky-radiance-command" );
	memset( &material, 0, sizeof( material ) );
	memset( semanticName, 0, sizeof( semanticName ) );
	if ( RenderSubmission_ActiveSkyMaterial( frontend, &material, semanticName )
			&& material.authoredSkyTexturesReady
			&& material.authoredSkySource.outerBox ) authoredReady = qtrue;
	if ( ( skyPlan->passMask & RAL_SKY_PASS_AUTHORED_BACKGROUND )
			&& material.authoredSkySource.outerBox && !authoredReady ) return qfalse;
	const bool captureMaterial = material.ready && ( material.stageCount > 0u || material.skyBox );
	present->skyMaterialUsesTime = captureMaterial
		? RenderSubmission_MaterialStagesUseTime( &material ) : qfalse;
	if ( captureMaterial && ( admittedMask & RAL_SKY_RADIANCE_CAPTURE_MASK ) ) {
		/* A sliced cube must use one shader/animMap clock for all six faces.
		 * The common scheduler admits +X first for each new capture cohort. */
		if ( admittedMask & 1u ) present->skyCaptureTime = present->shaderTimeOverride > 0.0f
			? present->shaderTimeOverride : (float)frontend->worldSnapshot.timeMs * 0.001f;
		static const uint32_t authoredFace[6] = { 0u, 2u, 1u, 3u, 4u, 5u };
		for ( uint32_t face = 0u; face < RAL_SKY_RADIANCE_CAPTURE_FACE_COUNT; ++face )
			if ( ( admittedMask & ( 1u << face ) )
					&& !EncodeAuthoredSkyFace( present, command, frontend,
						&material, authoredFace[face], physicalCapture ) )
				return PresentFailure( "sky-material-capture" );
		authoredReady = qtrue;
	}
	encoder = [command computeCommandEncoder];
	if ( !encoder ) return PresentFailure( "sky-radiance-encoder" );
	if ( admittedMask & RAL_SKY_RADIANCE_CAPTURE_MASK ) {
		ralMetalSkySourceParams_t params;
		id<MTLTexture> authored[RAL_MATERIAL_SKY_FACE_COUNT];
		id<MTLBuffer> arguments = NewSkyArgumentBuffer( device,
			present->skyRadianceArguments[RAL_METAL_SKY_SOURCE_PIPELINE] );
		if ( !arguments ) { [encoder endEncoding]; return qfalse; }
		for ( uint32_t face = 0u; face < RAL_MATERIAL_SKY_FACE_COUNT; ++face )
			authored[face] = present->uiFallbackTexture;
		if ( captureMaterial ) {
			for ( uint32_t face = 0u; face < RAL_MATERIAL_SKY_FACE_COUNT; ++face )
				authored[face] = present->skyAuthoredFaces[face];
		} else if ( authoredReady ) {
			for ( uint32_t face = 0u; face < RAL_MATERIAL_SKY_FACE_COUNT; ++face ) {
				id<MTLSamplerState> sampler;
				qboolean textured, msdf;
				if ( !UiMaterial( present, frontend,
						material.authoredSkyOuterTextures[face], &authored[face],
						&sampler, &textured, &msdf ) || !textured ) {
					[arguments release]; [encoder endEncoding]; return qfalse;
				}
			}
		}
		if ( !Ral_SkyRadianceSourceParamsBuild( skyPlan, atmosphere.state.cloudCover,
				captureMaterial ? RAL_SKY_SOURCE_COMPOSITED_RADIANCE
					: authoredReady ? RAL_SKY_SOURCE_AUTHORED_FACES
					: RAL_SKY_SOURCE_ANALYTIC, &params ) ) {
			[arguments release]; [encoder endEncoding]; return qfalse;
		}
		[present->skyRadianceArguments[RAL_METAL_SKY_SOURCE_PIPELINE]
			setArgumentBuffer:arguments offset:0u];
		[present->skyRadianceArguments[RAL_METAL_SKY_SOURCE_PIPELINE]
			setTexture:authored[0] atIndex:0u];
		[present->skyRadianceArguments[RAL_METAL_SKY_SOURCE_PIPELINE]
			setSamplerState:present->skyRadianceSampler atIndex:1u];
		[present->skyRadianceArguments[RAL_METAL_SKY_SOURCE_PIPELINE]
			setTexture:authored[1] atIndex:3u];
		[present->skyRadianceArguments[RAL_METAL_SKY_SOURCE_PIPELINE]
			setTexture:authored[2] atIndex:2u];
		for ( uint32_t face = 3u; face < RAL_MATERIAL_SKY_FACE_COUNT; ++face )
			[present->skyRadianceArguments[RAL_METAL_SKY_SOURCE_PIPELINE]
				setTexture:authored[face] atIndex:face + 1u];
		[present->skyRadianceArguments[RAL_METAL_SKY_SOURCE_PIPELINE]
			setTexture:present->skySource atIndex:7u];
		[encoder setComputePipelineState:
			present->skyRadiancePipeline[RAL_METAL_SKY_SOURCE_PIPELINE]];
		[encoder setBuffer:arguments offset:0u atIndex:0u];
		for ( uint32_t face = 0u; face < RAL_MATERIAL_SKY_FACE_COUNT; ++face )
			[encoder useResource:authored[face] usage:MTLResourceUsageRead];
		[encoder useResource:present->skySource usage:MTLResourceUsageWrite];
		for ( uint32_t face = 0u;
				face < RAL_SKY_RADIANCE_CAPTURE_FACE_COUNT; ++face ) {
			if ( !( admittedMask & ( 1u << face ) ) ) continue;
			params.meta[0] = face;
			[encoder setBytes:&params length:sizeof( params ) atIndex:1u];
			[encoder dispatchThreads:MTLSizeMake( RAL_METAL_SKY_SOURCE_SIZE,
				RAL_METAL_SKY_SOURCE_SIZE, 1u )
				threadsPerThreadgroup:MTLSizeMake( 8u, 8u, 1u )];
			present->skyRadianceDispatchCount++;
		}
		[arguments release];
	}
	if ( admittedMask & RAL_SKY_RADIANCE_AMBIENT_SH_MASK ) {
		if ( ( admittedMask & ( 1u << 6u ) )
				&& !present->skyIrradianceWriteStarted ) {
			id<MTLBuffer> arguments = NewSkyArgumentBuffer( device,
				present->skyRadianceArguments[RAL_METAL_SKY_IRRADIANCE_PIPELINE] );
			if ( !arguments ) { [encoder endEncoding]; return qfalse; }
			[present->skyRadianceArguments[RAL_METAL_SKY_IRRADIANCE_PIPELINE]
				setArgumentBuffer:arguments offset:0u];
			[present->skyRadianceArguments[RAL_METAL_SKY_IRRADIANCE_PIPELINE]
				setTexture:present->skySource atIndex:0u];
			[present->skyRadianceArguments[RAL_METAL_SKY_IRRADIANCE_PIPELINE]
				setSamplerState:present->skyRadianceSampler atIndex:1u];
			[present->skyRadianceArguments[RAL_METAL_SKY_IRRADIANCE_PIPELINE]
				setTexture:present->skyIrradiance[1] atIndex:2u];
			[encoder setComputePipelineState:
				present->skyRadiancePipeline[RAL_METAL_SKY_IRRADIANCE_PIPELINE]];
			[encoder setBuffer:arguments offset:0u atIndex:0u];
			[encoder useResource:present->skySource usage:MTLResourceUsageRead];
			[encoder useResource:present->skyIrradiance[1]
				usage:MTLResourceUsageWrite];
			[encoder dispatchThreads:MTLSizeMake( RAL_METAL_SKY_IRRADIANCE_SIZE,
				RAL_METAL_SKY_IRRADIANCE_SIZE,
				RAL_SKY_RADIANCE_CAPTURE_FACE_COUNT )
				threadsPerThreadgroup:MTLSizeMake( 8u, 8u, 1u )];
			[arguments release];
			present->skyIrradianceWriteStarted = qtrue;
			present->skyRadianceDispatchCount++;
		}
		{
			id<MTLBuffer> arguments = NewSkyArgumentBuffer( device,
				present->skyRadianceArguments[RAL_METAL_SKY_PROJECT_SH_PIPELINE] );
			if ( !arguments ) { [encoder endEncoding]; return qfalse; }
			[present->skyRadianceArguments[RAL_METAL_SKY_PROJECT_SH_PIPELINE]
				setArgumentBuffer:arguments offset:0u];
			[present->skyRadianceArguments[RAL_METAL_SKY_PROJECT_SH_PIPELINE]
				setTexture:present->skySource atIndex:0u];
			[present->skyRadianceArguments[RAL_METAL_SKY_PROJECT_SH_PIPELINE]
				setSamplerState:present->skyRadianceSampler atIndex:1u];
			[present->skyRadianceArguments[RAL_METAL_SKY_PROJECT_SH_PIPELINE]
				setBuffer:present->skySh[1] offset:0u atIndex:2u];
			[encoder setComputePipelineState:
				present->skyRadiancePipeline[RAL_METAL_SKY_PROJECT_SH_PIPELINE]];
			[encoder setBuffer:arguments offset:0u atIndex:0u];
			[encoder useResource:present->skySource usage:MTLResourceUsageRead];
			[encoder useResource:present->skySh[1] usage:MTLResourceUsageWrite];
			for ( uint32_t coefficient = 0u;
					coefficient < RAL_SKY_RADIANCE_AMBIENT_SH_COEFFICIENT_COUNT;
					++coefficient ) {
				ralMetalSkySliceParams_t params = {};
				if ( !( admittedMask & ( 1u << ( 6u + coefficient ) ) ) ) continue;
				params.values[0] = coefficient;
				[encoder setBytes:&params.values[0] length:sizeof( uint32_t )
					atIndex:1u];
				[encoder dispatchThreads:MTLSizeMake( 1u, 1u, 1u )
					threadsPerThreadgroup:MTLSizeMake( 1u, 1u, 1u )];
				present->skyRadianceDispatchCount++;
			}
			[arguments release];
		}
	}
	if ( admittedMask & RAL_SKY_RADIANCE_SPECULAR_MASK ) {
		[encoder setComputePipelineState:
			present->skyRadiancePipeline[RAL_METAL_SKY_PREFILTER_PIPELINE]];
		for ( uint32_t mip = 0u; mip < RAL_METAL_SKY_RADIANCE_MIPS; ++mip ) {
			ralMetalSkySliceParams_t params = {};
			id<MTLBuffer> arguments;
			uint32_t size;
			if ( !( admittedMask & ( 1u << ( 15u + mip ) ) ) ) continue;
			arguments = NewSkyArgumentBuffer( device,
				present->skyRadianceArguments[RAL_METAL_SKY_PREFILTER_PIPELINE] );
			if ( !arguments ) { [encoder endEncoding]; return qfalse; }
			[present->skyRadianceArguments[RAL_METAL_SKY_PREFILTER_PIPELINE]
				setArgumentBuffer:arguments offset:0u];
			[present->skyRadianceArguments[RAL_METAL_SKY_PREFILTER_PIPELINE]
				setTexture:present->skySource atIndex:0u];
			[present->skyRadianceArguments[RAL_METAL_SKY_PREFILTER_PIPELINE]
				setSamplerState:present->skyRadianceSampler atIndex:1u];
			[present->skyRadianceArguments[RAL_METAL_SKY_PREFILTER_PIPELINE]
				setTexture:present->skyRadianceMipViews[1][mip] atIndex:2u];
			[encoder setBuffer:arguments offset:0u atIndex:0u];
			[encoder useResource:present->skySource usage:MTLResourceUsageRead];
			[encoder useResource:present->skyRadianceMipViews[1][mip]
				usage:MTLResourceUsageWrite];
			params.values[0] = mip;
			params.values[1] = RAL_METAL_SKY_RADIANCE_MIPS;
			[encoder setBytes:&params.values[0] length:2u * sizeof( uint32_t )
				atIndex:1u];
			size = WIRED_SKY_SPECULAR_SIZE >> mip;
			[encoder dispatchThreads:MTLSizeMake( size, size,
				RAL_SKY_RADIANCE_CAPTURE_FACE_COUNT )
				threadsPerThreadgroup:MTLSizeMake( 8u, 8u, 1u )];
			[arguments release];
			present->skyRadianceDispatchCount++;
		}
		present->skyRadianceWriteStarted = qtrue;
	}
	[encoder endEncoding];
	present->skyRadianceCommand = [command retain];
	present->skyRadianceSubmittedSliceMask = admittedMask;
	present->skyRadianceSubmittedRevision =
		present->skyRadianceReceipt.targetProductRevision;
	present->skyRadianceSubmittedFrameGeneration = skyPlan->frameGeneration;
	present->skyRadianceTimingState = RAL_SKY_GPU_TIMING_PENDING;
	present->skyRadianceTimingFrameGeneration = skyPlan->frameGeneration;
	present->skyRadianceGpuDurationNanoseconds = 0u;
	[command commit];
	return qtrue;
}

qboolean RalMetal_PresentPlanAtmosphere( ralMetalPresent_t *present,
		const renderSubmissionState_t *frontend, uint64_t frameGeneration,
		uint32_t width, uint32_t height, ralAtmospherePlanReceipt_t *outReceipt ) {
	renderAtmosphereSnapshot_t snapshot;
	renderAtmosphereMediaSnapshot_t media;
	ralAtmospherePlanRequest_t request;
	ralAtmospherePlanReceipt_t receipt;
	ralSkyEnvironmentPlanRequest_t skyPlan;
	ralSkyEnvironmentRuntimeRequest_t skyRuntimeRequest;
	ralSkyEnvironmentRuntimeReceipt_t skyRuntimeReceipt;
	ralSkyEnvironmentRuntimeState_t skyRuntimeCandidate;
	ralSkyEnvironmentCapabilities_t skyCapabilities;
	uint64_t skyWorldLifecycleGeneration;
	uint32_t skyEventMask = 0u;
	uint32_t completedSkyWorkMask = 0u;
	const atmosphereEmitter_t *emitters;
	const atmosphereMediaVolume_t *volumes;
	if ( !present || !frontend || !frameGeneration || !width || !height
			|| !outReceipt
			|| !RenderSubmission_AtmosphereSnapshot( frontend, &snapshot, &emitters )
			|| !RenderSubmission_AtmosphereMediaSnapshot( frontend, &media,
				&volumes ) ) return qfalse;
	if ( present->atmosphereReceipt.frameGeneration == frameGeneration
			&& present->atmosphereReceiptWidth == width
			&& present->atmosphereReceiptHeight == height
			&& Ral_AtmospherePlanReceiptExact( &present->atmosphereReceipt,
				&present->atmosphereReceipt ) ) {
		*outReceipt = present->atmosphereReceipt;
		return qtrue;
	}
	(void)emitters; (void)volumes;
	/* Loading/UI-only frames retain the authored environment for the next
	 * scene, but have no camera in which a froxel grid could be produced.
	 * Plan zero scene work, rather than failing submission or reusing the
	 * preceding map's camera. This does not modify persistent authoring. */
	if ( !frontend->sceneRendered ) {
		snapshot.active = qfalse;
		present->atmosphereHistoryValid = qfalse;
	}
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
	request.cameraCut = RenderSubmission_ViewCameraCut( frontend );
	if ( request.cameraCut ) {
		present->atmosphereHistoryValid = qfalse;
		skyEventMask |= RAL_SKY_EVENT_CAMERA_CUT;
	}
	request.historyValid = present->atmosphereHistoryValid;
	request.capabilities.analyticComposite = qtrue;
	request.capabilities.compute = qtrue;
	request.capabilities.storageBuffers = qtrue;
	request.capabilities.temporalHistory = qtrue;
	request.capabilities.fullClouds = qtrue;
	memset( &skyCapabilities, 0, sizeof( skyCapabilities ) );
	skyWorldLifecycleGeneration = 0u;
	if ( frontend->activeWorldIndex >= 0
			&& !RenderSubmission_WorldLifecycleGeneration( frontend,
				frontend->activeWorldIndex,
				&skyWorldLifecycleGeneration ) ) return qfalse;
	if ( skyWorldLifecycleGeneration != 0u
			&& present->skyWorldLifecycleGeneration != 0u
			&& present->skyWorldLifecycleGeneration
				!= skyWorldLifecycleGeneration )
		skyEventMask |= RAL_SKY_EVENT_MAP_TRANSITION;
	if ( skyWorldLifecycleGeneration != 0u
			&& !EnsureSkyRadianceResources( present,
				skyWorldLifecycleGeneration ) ) return qfalse;
	if ( !CollectSkyWorkCompletion( present, qfalse,
			&completedSkyWorkMask ) ) return qfalse;
	skyCapabilities.physicalLuts = present->skyResourcesReady;
	skyCapabilities.temporalCrossfade = present->skyResourcesReady;
	/* Metal executes the shared distant-world receipt below through the same
	 * material, atmosphere and depth pipeline as the gameplay world.  This is
	 * a native capability, not a visual fallback. */
	skyCapabilities.distantScene = qtrue;
	if ( !RenderSubmission_BuildSkyEnvironmentRuntimeRequest( &snapshot,
			&present->skyRuntime, RAL_BACKEND_METAL, frameGeneration,
			request.requestedTier, skyEventMask, completedSkyWorkMask,
			present->skyWorkCommand ? 0u : 1u,
			snapshot.state.timelineSeconds, &skyCapabilities,
			&skyRuntimeRequest ) ) return qfalse;
	if ( present->skyWorkCommand && ( skyRuntimeRequest.eventMask &
			( RAL_SKY_EVENT_MAP_TRANSITION | RAL_SKY_EVENT_DEVICE_RECREATE |
			  RAL_SKY_EVENT_SOURCE_CHANGE ) ) )
		present->skySubmittedWorkMask = 0u;
	skyRuntimeCandidate = present->skyRuntime;
	if ( !Ral_SkyEnvironmentRuntimeAdvance( &skyRuntimeCandidate,
			&skyRuntimeRequest, &skyPlan, &skyRuntimeReceipt ) ) return qfalse;
	request.skyEnvironment = skyPlan;
	if ( !Ral_AtmospherePlan( &request, &receipt ) ||
		 !EncodeSkyPhysical( present, &snapshot,
			receipt.skyEnvironment.admittedWorkMask, frameGeneration ) ||
		 !EncodeSkyRadiance( present, frontend, &receipt.skyEnvironment,
			receipt.selectedTier, skyRuntimeRequest.eventMask,
			snapshot.state.timelineSeconds ) ) return qfalse;
	if ( !Ral_SkyEnvironmentRuntimeSchedule( &skyRuntimeCandidate,
			&receipt.skyEnvironment ) ) {
		uint32_t ignoredCompletion = 0u;
		if ( present->skyWorkCommand )
			CollectSkyWorkCompletion( present, qtrue, &ignoredCompletion );
		return qfalse;
	}
	present->skyRuntime = skyRuntimeCandidate;
	memset( &present->skyGpuTelemetry, 0,
		sizeof( present->skyGpuTelemetry ) );
	present->skyGpuTelemetry.schemaVersion =
		RAL_SKY_GPU_TELEMETRY_SCHEMA_VERSION;
	present->skyGpuTelemetry.backendType = RAL_BACKEND_METAL;
	present->skyGpuTelemetry.frameGeneration = frameGeneration;
	present->skyGpuTelemetry.sourceRevision =
		receipt.skyEnvironment.sourceRevision;
	present->skyGpuTelemetry.selectedTier = receipt.selectedTier;
	present->skyGpuTelemetry.selectedMode =
		receipt.skyEnvironment.selectedMode;
	present->skyGpuTelemetry.submittedWorkMask =
		receipt.skyEnvironment.admittedWorkMask;
	present->skyGpuTelemetry.completedWorkMask = completedSkyWorkMask;
	present->skyGpuTelemetry.deferredWorkMask =
		receipt.skyEnvironment.deferredWorkMask;
	present->skyGpuTelemetry.cancelledWorkMask =
		receipt.skyEnvironment.cancelledWorkMask |
			skyRuntimeReceipt.cancelledWorkMask;
	present->skyGpuTelemetry.submittedSliceMask =
		present->skyRadianceReceipt.admittedSliceMask;
	present->skyGpuTelemetry.completedSliceMask =
		present->skyRadianceReceipt.acceptedCompletionMask;
	present->skyGpuTelemetry.deferredSliceCount =
		present->skyRadianceReceipt.pendingSliceCount;
	present->skyGpuTelemetry.cancelledSliceCount =
		present->skyRadianceReceipt.cancelledSliceCount;
	present->skyGpuTelemetry.currentRadianceRevision =
		present->skyRadianceReceipt.currentProductRevision;
	present->skyGpuTelemetry.targetRadianceRevision =
		present->skyRadianceReceipt.targetProductRevision;
	present->skyGpuTelemetry.physicalTimingFrameGeneration =
		present->skyPhysicalTimingFrameGeneration;
	present->skyGpuTelemetry.radianceTimingFrameGeneration =
		present->skyRadianceTimingFrameGeneration;
	present->skyGpuTelemetry.physicalTimingState =
		present->skyPhysicalTimingState;
	present->skyGpuTelemetry.radianceTimingState =
		present->skyRadianceTimingState;
	present->skyGpuTelemetry.physicalGpuDurationNanoseconds =
		present->skyPhysicalGpuDurationNanoseconds;
	present->skyGpuTelemetry.radianceGpuDurationNanoseconds =
		present->skyRadianceGpuDurationNanoseconds;
	present->skyGpuTelemetry.ready = qtrue;
	if ( !Ral_SkyGpuTelemetryReceiptExact( &present->skyGpuTelemetry,
			&present->skyGpuTelemetry ) ) return qfalse;
	if ( skyWorldLifecycleGeneration != 0u )
		present->skyWorldLifecycleGeneration = skyWorldLifecycleGeneration;
	present->atmosphereReceipt = receipt;
	present->atmosphereReceiptWidth = width;
	present->atmosphereReceiptHeight = height;
	*outReceipt = receipt;
	return qtrue;
}

qboolean RalMetal_PresentSkyGpuTelemetry( const ralMetalPresent_t *present,
		ralSkyGpuTelemetryReceipt_t *outReceipt )
{
	if ( !present || !outReceipt ||
			!Ral_SkyGpuTelemetryReceiptExact( &present->skyGpuTelemetry,
				&present->skyGpuTelemetry ) ) return qfalse;
	*outReceipt = present->skyGpuTelemetry;
	return qtrue;
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
		const ralAtmospherePlanReceipt_t *plan, wiredMetalUpload &upload ) {
	if ( !present->atmosphereGpu )
		present->atmosphereGpu = RalMetal_AtmosphereCreate(
			RalMetal_CoreNativeDevice( present->core ) );
	return RalMetal_AtmosphereEncode( present->atmosphereGpu, command, frontend,
		plan, present->atmosphereHistoryValid, present->atmosphereHistoryParity,
		upload ) ? qtrue : qfalse;
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
	request.heightgridRequested = (snapshot.state.flags & ATMOSPHERE_FLAG_HEIGHTGRID) ? qtrue : qfalse;
	{
		renderAtmosphereHeightgrid_t grid;
		request.heightgridAvailable = RenderSubmission_AtmosphereHeightgrid( frontend, &grid );
		/* An explicitly requested collision input must not silently disappear. */
		if ( request.enabled && request.heightgridRequested && !request.heightgridAvailable ) return qfalse;
	}
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
			&& present->weatherPools[0]
			&& present->weatherPools[1]
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
		[pipeline release];
	}
	if ( !present->weatherRenderPipeline || error ) goto fail;
	for ( uint32_t i = 0u; i < 2u; ++i ) if ( !present->weatherPools[i] ) {
		present->weatherPools[i] = [device newBufferWithLength:
			(NSUInteger)RAL_METAL_WEATHER_PARTICLE_CAPACITY * 32u
			options:MTLResourceStorageModeShared];
		if ( present->weatherPools[i] ) memset( [present->weatherPools[i] contents],
			0, (size_t)RAL_METAL_WEATHER_PARTICLE_CAPACITY * 32u );
	}
	present->weatherPixelFormat = colorFormat;
	[compute release]; [vertex release]; [fragment release];
	return present->weatherPools[0]
		&& present->weatherPools[1] ? qtrue : qfalse;
fail:
	[compute release]; [vertex release]; [fragment release];
	return qfalse;
}

static qboolean EncodeWeather( ralMetalPresent_t *present,
		id<MTLCommandBuffer> command, const renderSubmissionState_t *frontend,
		const ralAtmosphereWeatherReceipt_t *weather,
		ralMetalWeatherFrame_t *outFrame,
		renderAtmosphereEffectWorkloadSnapshot_t *outEffects ) {
	renderAtmosphereSnapshot_t snapshot;
	renderAtmosphereEffectWorkloadSnapshot_t effects;
	renderWorldSnapshot_t view;
	const atmosphereEmitter_t *emitters;
	ralMetalWeatherFrame_t frame;
	id<MTLComputeCommandEncoder> encoder;
	float weights[5], projectionX, projectionY;
	if ( !present || !command || !frontend || !weather || !outFrame || !outEffects || weather->zeroWork
			|| !RenderSubmission_AtmosphereSnapshot( frontend, &snapshot,
				&emitters )
			|| !RenderSubmission_ViewSnapshot( frontend, &view ) ) return qfalse;
	const uint32_t readPool = present->weatherParity ? 1u : 0u;
	const uint32_t writePool = present->weatherParity ? 0u : 1u;
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
	/* Weather is transformed by the final tonemap; publish identity in the
	 * shared direct-present weather ABI (Vulkan/OpenGL/WebGPU do the same) so
	 * display visibility is not applied twice. */
	frame.viewLeft[3] = 1.0f;
	frame.viewUp[3] = 1.0f;
	frame.eyeWorld[3] = RAL_DISPLAY_SHADOW_PIVOT;
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
	memcpy( frame.sun, snapshot.state.skyEnvironment.sunDirection,
		3u * sizeof( float ) );
	frame.sun[3] = sqrtf(
		snapshot.state.skyEnvironment.sunRadiance[0]
			* snapshot.state.skyEnvironment.sunRadiance[0]
		+ snapshot.state.skyEnvironment.sunRadiance[1]
			* snapshot.state.skyEnvironment.sunRadiance[1]
		+ snapshot.state.skyEnvironment.sunRadiance[2]
			* snapshot.state.skyEnvironment.sunRadiance[2] );
	memcpy( frame.moon, snapshot.state.skyEnvironment.moonDirection,
		3u * sizeof( float ) );
	frame.moon[3] = sqrtf(
		snapshot.state.skyEnvironment.moonRadiance[0]
			* snapshot.state.skyEnvironment.moonRadiance[0]
		+ snapshot.state.skyEnvironment.moonRadiance[1]
			* snapshot.state.skyEnvironment.moonRadiance[1]
		+ snapshot.state.skyEnvironment.moonRadiance[2]
			* snapshot.state.skyEnvironment.moonRadiance[2] );
	memcpy( frame.ambientCloud, snapshot.state.skyEnvironment.ambientSh,
		3u * sizeof( float ) ); frame.ambientCloud[3] = snapshot.state.cloudCover;
	frame.cloudMedia[0] = snapshot.state.skyEnvironment.cloudSunOcclusion;
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
	id<MTLBuffer> heightgrid = nil;
	frame.weather[3] = 0.0f;
	if ( weather->heightgridActive ) {
		renderAtmosphereHeightgrid_t grid;
		if ( !RenderSubmission_AtmosphereHeightgrid( frontend, &grid ) ) return qfalse;
		const unsigned slot = (unsigned)grid.worldIndex;
		if ( !present->weatherHeightgrids[slot] || present->weatherHeightgridDigests[slot] != grid.digest ) {
			/* New immutable storage: pending commands retain the old buffer.
			 * No CPU overwrite, completion wait or per-frame copy. */
			id<MTLBuffer> candidate = [RalMetal_CoreNativeDevice( present->core )
				newBufferWithBytes:grid.samples length:(NSUInteger)grid.edge * grid.edge * sizeof( float )
				options:MTLResourceStorageModeShared];
			if ( !candidate ) return qfalse;
			[present->weatherHeightgrids[slot] release];
			present->weatherHeightgrids[slot] = candidate;
			present->weatherHeightgridDigests[slot] = grid.digest;
		}
		heightgrid = present->weatherHeightgrids[slot];
		memcpy( frame.heightgridOriginStep, grid.worldMins, sizeof( vec2_t ) );
		memcpy( frame.heightgridOriginStep + 2, grid.inverseStep, sizeof( vec2_t ) );
		frame.weather[3] = (float)grid.edge;
	}
	encoder = [command computeCommandEncoder];
	if ( !encoder ) return qfalse;
	[encoder setComputePipelineState:present->weatherComputePipeline];
	[encoder setBytes:&frame length:sizeof( frame ) atIndex:0u];
	[encoder setBuffer:present->weatherPools[readPool] offset:0u atIndex:1u];
	[encoder setBuffer:present->weatherPools[writePool] offset:0u atIndex:2u];
	/* Bind a valid unused buffer when the recipe has no heightgrid. The shader
	 * edge=0 branch never accesses it (not a collision fallback). */
	[encoder setBuffer:heightgrid ? heightgrid : present->weatherPools[readPool] offset:0u atIndex:3u];
	[encoder dispatchThreads:MTLSizeMake( weather->activeParticleCount, 1u, 1u )
		threadsPerThreadgroup:MTLSizeMake( 64u, 1u, 1u )];
	[encoder endEncoding];
	*outFrame = frame;
	*outEffects = effects;
	return qtrue;
}

static qboolean GenericEffectsPending( const ralMetalPresent_t *present,
		const renderSubmissionState_t *frontend ) {
	renderEffectPrimitiveSnapshot_t snapshot;
	renderWorldSnapshot_t view;
	const spriteDesc_t *sprites;
	const emitterDesc_t *emitters;
	const decalDesc_t *decals;
	const renderEffectRibbonCommand_t *ribbons;
	const ribbonPoint_t *points;
	const beamDesc_t *beams;
	if ( !frontend || !RenderSubmission_EffectPrimitiveSnapshots( frontend,
			&snapshot, &sprites, &emitters, &decals, &ribbons, &points, &beams ) )
		return qfalse;
	(void)beams;
	/* Reject new emitters without a view in the normal encode validation;
	 * merely resident particles must not turn loading UI into a scene draw. */
	return snapshot.emitterCount || ( present
		&& RenderSubmission_ViewSnapshot( frontend, &view )
		&& present->effectActiveUntil > present->effectTimeline ) ? qtrue : qfalse;
}

static id<MTLLibrary> NewEmbeddedLibrary( id<MTLDevice> device,
		const unsigned char *bytes, size_t byteCount ) {
	dispatch_data_t data;
	id<MTLLibrary> library;
	NSError *error = nil;
	if ( !device || !bytes || !byteCount ) return nil;
	data = dispatch_data_create( bytes, byteCount,
		dispatch_get_global_queue( QOS_CLASS_DEFAULT, 0 ), ^{} );
	if ( !data ) return nil;
	library = [device newLibraryWithData:data error:&error];
	return error ? nil : library;
}

static id<MTLFunction> NewSpecializedFunction( id<MTLLibrary> library,
		uint32_t value ) {
	MTLFunctionConstantValues *constants;
	NSError *error = nil;
	id<MTLFunction> function;
	if ( !library ) return nil;
	constants = [[[MTLFunctionConstantValues alloc] init] autorelease];
	[constants setConstantValue:&value type:MTLDataTypeUInt atIndex:0u];
	function = [library newFunctionWithName:@"main0"
		constantValues:constants error:&error];
	return error ? nil : function;
}

static qboolean EnsureParticlePipeline( ralMetalPresent_t *present,
		MTLPixelFormat colorFormat ) {
	id<MTLDevice> device;
	id<MTLFunction> integrate = nil, spawn = nil, alphaVertex = nil;
	id<MTLFunction> additiveVertex = nil, fragment = nil;
	NSError *error = nil;
	if ( !present || !EnsureUiSampling( present ) ) return qfalse;
	if ( present->particleIntegratePipeline && present->particleSpawnPipeline
			&& present->particleAlphaPipeline && present->particleAdditivePipeline
			&& present->effectPools[0] && present->effectPools[1]
			&& present->particleProfiles
			&& present->particleChildEvents
			&& present->particlePixelFormat == colorFormat ) return qtrue;
	device = RalMetal_CoreNativeDevice( present->core );
	if ( !device ) return qfalse;
	if ( !present->particleComputeLibrary ) present->particleComputeLibrary =
		NewEmbeddedLibrary( device, ral_metal_particle_compute_metallib,
			ral_metal_particle_compute_metallib_size );
	if ( !present->particleVertexLibrary ) present->particleVertexLibrary =
		NewEmbeddedLibrary( device, ral_metal_particle_vertex_metallib,
			ral_metal_particle_vertex_metallib_size );
	if ( !present->particleFragmentLibrary ) present->particleFragmentLibrary =
		NewEmbeddedLibrary( device, ral_metal_particle_fragment_metallib,
			ral_metal_particle_fragment_metallib_size );
	if ( !present->particleComputeLibrary || !present->particleVertexLibrary
			|| !present->particleFragmentLibrary ) goto fail;
	integrate = NewSpecializedFunction( present->particleComputeLibrary, 0u );
	spawn = NewSpecializedFunction( present->particleComputeLibrary, 1u );
	alphaVertex = NewSpecializedFunction( present->particleVertexLibrary, 0u );
	additiveVertex = NewSpecializedFunction( present->particleVertexLibrary, 1u );
	fragment = [present->particleFragmentLibrary newFunctionWithName:@"main0"];
	if ( !integrate || !spawn || !alphaVertex || !additiveVertex || !fragment )
		goto fail;
	[present->particleIntegratePipeline release];
	present->particleIntegratePipeline = [device
		newComputePipelineStateWithFunction:integrate error:&error];
	if ( !present->particleIntegratePipeline || error ) goto fail;
	[present->particleSpawnPipeline release];
	present->particleSpawnPipeline = [device
		newComputePipelineStateWithFunction:spawn error:&error];
	if ( !present->particleSpawnPipeline || error ) goto fail;
	{
		MTLRenderPipelineDescriptor *pipeline =
			[[[MTLRenderPipelineDescriptor alloc] init] autorelease];
		pipeline.vertexFunction = alphaVertex;
		pipeline.fragmentFunction = fragment;
		pipeline.colorAttachments[0].pixelFormat = colorFormat;
		pipeline.colorAttachments[0].blendingEnabled = YES;
		pipeline.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorSourceAlpha;
		pipeline.colorAttachments[0].destinationRGBBlendFactor =
			MTLBlendFactorOneMinusSourceAlpha;
		pipeline.colorAttachments[0].sourceAlphaBlendFactor = MTLBlendFactorOne;
		pipeline.colorAttachments[0].destinationAlphaBlendFactor =
			MTLBlendFactorOneMinusSourceAlpha;
		pipeline.depthAttachmentPixelFormat = MTLPixelFormatDepth32Float;
		[present->particleAlphaPipeline release];
		present->particleAlphaPipeline = [device
			newRenderPipelineStateWithDescriptor:pipeline error:&error];
		if ( !present->particleAlphaPipeline || error ) goto fail;
		pipeline.vertexFunction = additiveVertex;
		pipeline.colorAttachments[0].destinationRGBBlendFactor = MTLBlendFactorOne;
		[present->particleAdditivePipeline release];
		present->particleAdditivePipeline = [device
			newRenderPipelineStateWithDescriptor:pipeline error:&error];
		if ( !present->particleAdditivePipeline || error ) goto fail;
	}
	[present->particleIntegrateArguments release];
	present->particleIntegrateArguments = [integrate
		newArgumentEncoderWithBufferIndex:0u];
	[present->particleSpawnArguments release];
	present->particleSpawnArguments = [spawn
		newArgumentEncoderWithBufferIndex:0u];
	[present->particleVertexArguments release];
	present->particleVertexArguments = [alphaVertex
		newArgumentEncoderWithBufferIndex:0u];
	[present->particleFragmentArguments release];
	present->particleFragmentArguments = [fragment
		newArgumentEncoderWithBufferIndex:0u];
	if ( !present->particleIntegrateArguments || !present->particleSpawnArguments
			|| !present->particleVertexArguments
			|| !present->particleFragmentArguments ) goto fail;
	if ( !present->particleProfiles ) present->particleProfiles = [device
		newBufferWithLength:ATMOSPHERE_EFFECT_MAX_PROFILES * 592u
		options:MTLResourceStorageModeShared];
	if ( !present->particleChildEvents ) present->particleChildEvents = [device
		newBufferWithLength:80u * 1024u options:MTLResourceStorageModeShared];
	for ( uint32_t i = 0u; i < 2u; ++i ) if ( !present->effectPools[i] ) {
		present->effectPools[i] = [device newBufferWithLength:
			(NSUInteger)RAL_PARTICLE_POOL_CAPACITY * RAL_PARTICLE_GPU_BYTES
			options:MTLResourceStorageModeShared];
		if ( present->effectPools[i] ) memset( [present->effectPools[i] contents],
			0, (size_t)RAL_PARTICLE_POOL_CAPACITY * RAL_PARTICLE_GPU_BYTES );
	}
	if ( present->particleProfiles ) memset( [present->particleProfiles contents],
		0, ATMOSPHERE_EFFECT_MAX_PROFILES * 592u );
	if ( present->particleChildEvents ) memset( [present->particleChildEvents contents],
		0, 80u * 1024u );
	present->particlePixelFormat = colorFormat;
	[integrate release]; [spawn release]; [alphaVertex release];
	[additiveVertex release]; [fragment release];
	return present->effectPools[0] && present->effectPools[1]
		&& present->particleProfiles
		&& present->particleChildEvents ? qtrue : qfalse;
fail:
	[integrate release]; [spawn release]; [alphaVertex release];
	[additiveVertex release]; [fragment release];
	return qfalse;
}

static qboolean EncodeRalParticles( ralMetalPresent_t *present,
		wiredMetalParticleUploads &uploads,
		id<MTLCommandBuffer> command, const renderSubmissionState_t *frontend,
		const renderWorldSnapshot_t *view, uint64_t frameGeneration,
		uint32_t *outEmitterCount, uint32_t *outParticleCount,
		uint32_t *outDroppedCount, float *outActiveUntil,
		qboolean *outAdvancedPool,
		id<MTLBuffer> *outFrameBuffer, id<MTLBuffer> *outClassBuffer,
		uint32_t *outRenderedPool,
		renderEffectParticlePlanState_t *outCandidate,
		renderEffectParticlePlanReceipt_t *outPlanReceipt ) {
	renderEffectPrimitiveSnapshot_t snapshot;
	const spriteDesc_t *sprites; const emitterDesc_t *emitters;
	const decalDesc_t *decals; const renderEffectRibbonCommand_t *ribbons;
	const ribbonPoint_t *points; const beamDesc_t *beams;
	ralParticleFrameGpu_t frame;
	ralParticleSpawnGpu_t *requests;
	uint32_t requestCount = 0u, spawnMaxGroups = 0u;
	uint32_t classCount;
	renderEffectParticlePlanState_t candidate;
	renderEffectParticlePlanReceipt_t planReceipt;
	float now, projectionX, projectionY, activeUntil;
	uint32_t readPool, writePool;
	id<MTLDevice> device;
	id<MTLBuffer> frameBuffer = nil, classBuffer = nil, spawnBuffer = nil;
	id<MTLBuffer> integrateArguments = nil, spawnArguments = nil;
	id<MTLComputeCommandEncoder> encoder;
	if ( !present || !command || !frontend || !view || !outEmitterCount
			|| !outParticleCount || !outDroppedCount || !outActiveUntil
			|| !outAdvancedPool || !outFrameBuffer || !outClassBuffer
			|| !outRenderedPool || !outCandidate
			|| !outPlanReceipt
			|| !RenderSubmission_EffectPrimitiveSnapshots( frontend, &snapshot,
				&sprites, &emitters, &decals, &ribbons, &points, &beams ) ) return qfalse;
	(void)sprites; (void)emitters; (void)decals; (void)ribbons;
	(void)points; (void)beams;
	now = (float)view->timeMs * 0.001f;
	if ( !isfinite( now ) || now < 0.0f ) return qfalse;
	activeUntil = present->effectTimeline > now ? 0.0f
		: present->effectActiveUntil;
	*outAdvancedPool = qfalse;
	*outFrameBuffer = nil;
	*outClassBuffer = nil;
	*outRenderedPool = 0u;
	if ( !present->particleIntegratePipeline || !present->particleSpawnPipeline )
		return qfalse;
	device = RalMetal_CoreNativeDevice( present->core );
	if ( !device ) return qfalse;
	/* Reuse completed storage only. Busy inputs remain immutable, including
	 * both indirect argument buffers. The existing bounded cache never waits. */
	if ( !uploads.frame.wired_reserve( device, sizeof( ralParticleFrameGpu_t ),
			MTLResourceStorageModeShared )
			|| !uploads.classes.wired_reserve( device, (NSUInteger)MAX_PARTICLE_CLASSES
				* RAL_PARTICLE_CLASS_GPU_BYTES, MTLResourceStorageModeShared )
			|| !uploads.spawns.wired_reserve( device,
				(NSUInteger)RAL_PARTICLE_SPAWN_REQUEST_MAX * RAL_PARTICLE_SPAWN_GPU_BYTES,
				MTLResourceStorageModeShared ) ) return qfalse;
	frameBuffer = uploads.frame.buffer; classBuffer = uploads.classes.buffer;
	spawnBuffer = uploads.spawns.buffer;
	frameBuffer.label = @"Wired particle frame";
	classBuffer.label = @"Wired particle classes";
	spawnBuffer.label = @"Wired particle spawns";
	if ( present->effectTimeline > now ) {
		id<MTLBlitCommandEncoder> resetEncoder = [command blitCommandEncoder];
		if ( !resetEncoder ) return qfalse;
		for ( uint32_t pool = 0u; pool < 2u; ++pool )
			[resetEncoder fillBuffer:present->effectPools[pool]
				range:NSMakeRange( 0u, (NSUInteger)RAL_PARTICLE_POOL_CAPACITY
					* RAL_PARTICLE_GPU_BYTES ) value:0u];
		[resetEncoder endEncoding];
	}
	requests = (ralParticleSpawnGpu_t *)[spawnBuffer contents];
	if ( !RenderEffectParticlePlan_Build( frontend,
			&present->effectParticlePlan, frameGeneration, qtrue, now,
			(ralParticleClassGpu_t *)[classBuffer contents],
			MAX_PARTICLE_CLASSES, requests, RAL_PARTICLE_SPAWN_REQUEST_MAX,
			&candidate, &planReceipt )
			|| !RenderEffectParticlePlan_ReceiptExact( &candidate,
				(const ralParticleClassGpu_t *)[classBuffer contents],
				requests, &planReceipt ) ) return qfalse;
	classCount = planReceipt.classCount;
	requestCount = planReceipt.spawnRequestCount;
	for ( uint32_t i = 0u; i < requestCount; ++i ) {
		particleClass_t particleClass;
		uint32_t groups = ( requests[i].count + 63u ) / 64u;
		if ( groups > spawnMaxGroups ) spawnMaxGroups = groups;
		if ( !RenderSubmission_GetParticleClass( frontend,
				(particleClassHandle_t)requests[i].classHandle,
				&particleClass ) ) return qfalse;
		activeUntil = fmaxf( activeUntil,
			now + particleClass.lifetimeMean + particleClass.lifetimeJitter );
	}
	memset( &frame, 0, sizeof( frame ) );
	projectionX = 1.0f / tanf( view->fovX * 0.00872664625997164788f );
	projectionY = 1.0f / tanf( view->fovY * 0.00872664625997164788f );
	for ( uint32_t axis = 0u; axis < 3u; ++axis ) {
		frame.mvp[axis * 4u] = -view->viewAxis[1][axis] * projectionX;
		frame.mvp[axis * 4u + 1u] = view->viewAxis[2][axis] * projectionY;
		frame.mvp[axis * 4u + 2u] = view->viewAxis[0][axis];
		frame.mvp[axis * 4u + 3u] = view->viewAxis[0][axis];
		frame.mvp[12] += view->viewOrigin[axis] * view->viewAxis[1][axis] * projectionX;
		frame.mvp[13] -= view->viewOrigin[axis] * view->viewAxis[2][axis] * projectionY;
		frame.mvp[14] -= view->viewOrigin[axis] * view->viewAxis[0][axis];
		frame.mvp[15] -= view->viewOrigin[axis] * view->viewAxis[0][axis];
	}
	frame.mvp[14] -= 4.0f;
	memcpy( frame.viewLeft, view->viewAxis[1], 3u * sizeof( float ) );
	memcpy( frame.viewUp, view->viewAxis[2], 3u * sizeof( float ) );
	memcpy( frame.eyeWorld, view->viewOrigin, 3u * sizeof( float ) );
	frame.dt = present->effectTimeline > 0.0f ? now - present->effectTimeline : 0.0f;
	if ( frame.dt < 0.0f || frame.dt > 0.1f ) frame.dt = 1.0f / 60.0f;
	frame.poolSize = RAL_PARTICLE_POOL_CAPACITY;
	frame.numClasses = classCount; frame.pingPongRead = present->effectParity ? 1u : 0u;
	frame.invResX = view->viewportWidth ? 1.0f / (float)view->viewportWidth : 0.0f;
	frame.invResY = view->viewportHeight ? 1.0f / (float)view->viewportHeight : 0.0f;
	/* Metal's world pass uses forward-Z (near=0, far=1).  The canonical
	 * particle ABI carries depth validity and direction in one signed lane. */
	frame.depthValid = -1.0f;
	/* The tonemap multiplies the HDR scene by adapted-exposure x brightness
	 * (wired_tonemap_fragment); the additive flipbook pre-divides by exactly
	 * that product (Vulkan reads the same value from its exposure UBO). */
	frame.exposureBias = fmaxf( present->displayVisibility.exposureScale
		* ( present->exposureSettings.enabled && present->adaptedExposureBias > 0.0f
			? present->adaptedExposureBias : 1.0f ), 0.001f );
	memcpy( [frameBuffer contents], &frame, sizeof( frame ) );
	*outEmitterCount = planReceipt.admittedEmitterCount;
	*outDroppedCount = planReceipt.droppedParticleCount;
	*outParticleCount = activeUntil > now
		? RAL_PARTICLE_POOL_CAPACITY : 0u;
	*outActiveUntil = activeUntil;
	*outCandidate = candidate;
	*outPlanReceipt = planReceipt;
	if ( !*outParticleCount && !requestCount ) return qtrue;
	readPool = present->effectParity ? 1u : 0u;
	writePool = present->effectParity ? 0u : 1u;
	*outFrameBuffer = frameBuffer;
	*outClassBuffer = classBuffer;
	*outRenderedPool = writePool;
	if ( !uploads.integrateArguments.wired_reserve( device,
			present->particleIntegrateArguments.encodedLength,
			MTLResourceStorageModeShared ) ) return qfalse;
	integrateArguments = uploads.integrateArguments.buffer;
	integrateArguments.label = @"Wired particle integrate arguments";
	if ( requestCount ) {
		if ( !uploads.spawnArguments.wired_reserve( device,
				present->particleSpawnArguments.encodedLength,
				MTLResourceStorageModeShared ) ) return qfalse;
		spawnArguments = uploads.spawnArguments.buffer;
		spawnArguments.label = @"Wired particle spawn arguments";
	}
	[present->particleIntegrateArguments setArgumentBuffer:integrateArguments offset:0u];
	[present->particleIntegrateArguments setBuffer:frameBuffer offset:0u atIndex:0u];
	[present->particleIntegrateArguments setBuffer:classBuffer offset:0u atIndex:1u];
	[present->particleIntegrateArguments setBuffer:present->particleProfiles offset:0u atIndex:2u];
	[present->particleIntegrateArguments setBuffer:present->particleChildEvents offset:0u atIndex:3u];
	[present->particleIntegrateArguments setTexture:present->uiFallbackTexture atIndex:4u];
	[present->particleIntegrateArguments setSamplerState:present->uiClampSampler atIndex:5u];
	[present->particleIntegrateArguments setBuffer:present->effectPools[writePool] offset:0u atIndex:6u];
	[present->particleIntegrateArguments setBuffer:spawnBuffer offset:0u atIndex:7u];
	[present->particleIntegrateArguments setBuffer:present->effectPools[readPool] offset:0u atIndex:8u];
	encoder = [command computeCommandEncoder]; if ( !encoder ) goto fail;
	[encoder setComputePipelineState:present->particleIntegratePipeline];
	[encoder setBuffer:integrateArguments offset:0u atIndex:0u];
	[encoder useResource:frameBuffer usage:MTLResourceUsageRead];
	[encoder useResource:classBuffer usage:MTLResourceUsageRead];
	[encoder useResource:present->particleProfiles usage:MTLResourceUsageRead];
	[encoder useResource:present->particleChildEvents
		usage:MTLResourceUsageRead | MTLResourceUsageWrite];
	[encoder useResource:present->effectPools[readPool] usage:MTLResourceUsageRead];
	[encoder useResource:present->effectPools[writePool] usage:MTLResourceUsageWrite];
	[encoder dispatchThreads:MTLSizeMake( RAL_PARTICLE_POOL_CAPACITY, 1u, 1u )
		threadsPerThreadgroup:MTLSizeMake( 64u, 1u, 1u )];
	[encoder endEncoding];
	*outAdvancedPool = qtrue;
	if ( requestCount ) {
		[present->particleSpawnArguments setArgumentBuffer:spawnArguments offset:0u];
		[present->particleSpawnArguments setBuffer:frameBuffer offset:0u atIndex:0u];
		[present->particleSpawnArguments setBuffer:classBuffer offset:0u atIndex:1u];
		[present->particleSpawnArguments setBuffer:present->particleProfiles offset:0u atIndex:2u];
		[present->particleSpawnArguments setBuffer:present->particleChildEvents offset:0u atIndex:3u];
		[present->particleSpawnArguments setTexture:present->uiFallbackTexture atIndex:4u];
		[present->particleSpawnArguments setSamplerState:present->uiClampSampler atIndex:5u];
		[present->particleSpawnArguments setBuffer:present->effectPools[writePool] offset:0u atIndex:6u];
		[present->particleSpawnArguments setBuffer:spawnBuffer offset:0u atIndex:7u];
		[present->particleSpawnArguments setBuffer:present->effectPools[writePool] offset:0u atIndex:8u];
		encoder = [command computeCommandEncoder]; if ( !encoder ) goto fail;
		[encoder setComputePipelineState:present->particleSpawnPipeline];
		[encoder setBuffer:spawnArguments offset:0u atIndex:0u];
		[encoder useResource:frameBuffer usage:MTLResourceUsageRead];
		[encoder useResource:classBuffer usage:MTLResourceUsageRead];
		[encoder useResource:present->particleProfiles usage:MTLResourceUsageRead];
		[encoder useResource:present->particleChildEvents
			usage:MTLResourceUsageRead | MTLResourceUsageWrite];
		[encoder useResource:spawnBuffer usage:MTLResourceUsageRead];
		[encoder useResource:present->effectPools[writePool]
			usage:MTLResourceUsageRead | MTLResourceUsageWrite];
		[encoder dispatchThreadgroups:MTLSizeMake( requestCount, spawnMaxGroups, 1u )
			threadsPerThreadgroup:MTLSizeMake( 64u, 1u, 1u )];
		[encoder endEncoding];
	}
	return qtrue;
fail:
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

static void InitializeToneMapDefaults( ralMetalPresent_t *present ) {
	present->bloomSettings = { 0u, 2u, 0u, 0.32f, 0.5f };
	Ral_TemporalInit( &present->taaState );
	EntityMotionReset( present );
	present->taaTopologyEpoch = 1u;
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
	if ( !Ral_SkyEnvironmentRuntimeInit( RAL_BACKEND_METAL,
			&candidate->skyRuntime ) ||
		 !Ral_SkyRadianceInit( RAL_BACKEND_METAL,
			&candidate->skyRadianceState ) ) {
		[layer release]; free( candidate ); return qfalse;
	}
	InitializeToneMapDefaults( candidate );
	RenderEffectParticlePlan_Reset( &candidate->effectParticlePlan );
	ResetPersistentEffects( candidate );
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
	if ( !Ral_SkyEnvironmentRuntimeInit( RAL_BACKEND_METAL,
			&candidate->skyRuntime ) ||
		 !Ral_SkyRadianceInit( RAL_BACKEND_METAL,
			&candidate->skyRadianceState ) ) { free( candidate ); return qfalse; }
	InitializeToneMapDefaults( candidate );
	RenderEffectParticlePlan_Reset( &candidate->effectParticlePlan );
	ResetPersistentEffects( candidate );
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
	wired_clear_present_upload_caches( present );
	present->lastReadbackGeneration = 0u;
	present->exposureHistoryValid = qfalse;
	present->lastReadbackByteCount = 0u;
	present->lastReadbackReady = qfalse;
	present->atmosphereHistoryValid = qfalse;
	present->atmosphereHistoryParity = qfalse;
	present->weatherParity = qfalse;
	present->weatherTimeline = 0.0f;
	present->effectParity = qfalse;
	present->effectTimeline = 0.0f;
	present->effectActiveUntil = 0.0f;
	RenderEffectParticlePlan_Reset( &present->effectParticlePlan );
	ResetPersistentEffects( present );
	for ( uint32_t i = 0u; i < 2u; ++i ) if ( present->weatherPools[i] )
		memset( [present->weatherPools[i] contents], 0,
			(size_t)RAL_METAL_WEATHER_PARTICLE_CAPACITY * 32u );
	for ( uint32_t i = 0u; i < 2u; ++i ) if ( present->effectPools[i] )
		memset( [present->effectPools[i] contents], 0,
			(size_t)RAL_PARTICLE_POOL_CAPACITY * RAL_PARTICLE_GPU_BYTES );
	*outReceipt = receipt;
	return qtrue;
}

void RalMetal_PresentDestroy( ralMetalPresent_t *present ) {
	uint32_t i;
	uint32_t ignoredCompletion = 0u, ignoredSliceCompletion = 0u;
	uint64_t ignoredRevision = 0u;
	if ( !present ) return;
	(void)ReapReadbackRing( present, qtrue );
	wired_clear_present_upload_caches( present );
	if ( present->skyWorkCommand )
		(void)CollectSkyWorkCompletion( present, qtrue,
			&ignoredCompletion );
	if ( present->skyRadianceCommand )
		(void)CollectSkyRadianceCompletion( present, qtrue,
			&ignoredSliceCompletion, &ignoredRevision );
	for ( i = 0u; i < RAL_METAL_READBACK_RING_SIZE; ++i ) {
		[present->readbackCommands[i] release];
		[present->readbackSceneCommands[i] release];
		[present->readbackBuffers[i] release];
	}
	for ( i = 0u; i < RENDER_SUBMISSION_MAX_TEXTURE_RESOURCES; ++i )
		[present->uiMaterials[i].texture release];
	[present->uiFallbackTexture release];
	[present->worldLightingFallbackTexture release];
	for ( i = 0u; i < RAL_LIGHTING_RUNTIME_MAX_PLANES; ++i )
		[present->directionalLightingTextures[i] release];
	[present->uiClampSampler release];
	[present->uiRepeatSampler release];
	[present->materialClampSampler release];
	[present->materialRepeatSampler release];
	[present->uiPipeline release];
	[present->uiMsdfPipeline release];
	[present->uiBackdropPipeline release];
	[present->toneMapPipeline release];
	RalMetal_ExposureDestroy( present->exposure );
	RalMetal_BloomDestroy( present->bloom );
	RalMetal_SmaaDestroy( present->smaa );
	RalMetal_TaaDestroy( present->taa );
	[present->worldPipeline release];
	[present->skyCapturePipeline release];
	[present->worldBlendPipeline release];
	[present->worldAlphaAdditivePipeline release];
	[present->worldAdditivePipeline release];
	RalMetal_AtmosphereDestroy( present->atmosphereGpu );
	[present->lensVisibilityPipeline release];
	[present->atmosphereFallback release];
	ReleaseSkyRadianceResources( present );
	for ( i = 0u; i < RAL_METAL_SKY_PHYSICAL_TEXTURE_COUNT; ++i )
		[present->skyPhysicalPipeline[i] release];
	[present->skyPhysicalSampler release];
	[present->skyWorkCommand release];
	for ( i = 0u; i < RAL_METAL_SKY_RADIANCE_PIPELINE_COUNT; ++i ) {
		[present->skyRadianceArguments[i] release];
		[present->skyRadiancePipeline[i] release];
		[present->skyRadianceLibrary[i] release];
	}
	[present->skyRadianceSampler release];
	[present->skyRadianceCommand release];
	[present->weatherComputePipeline release];
	[present->weatherRenderPipeline release];
	[present->particleIntegratePipeline release];
	[present->particleSpawnPipeline release];
	[present->particleAlphaPipeline release];
	[present->particleAdditivePipeline release];
	[present->particleIntegrateArguments release];
	[present->particleSpawnArguments release];
	[present->particleVertexArguments release];
	[present->particleFragmentArguments release];
	[present->particleComputeLibrary release];
	[present->particleVertexLibrary release];
	[present->particleFragmentLibrary release];
	[present->weatherPools[0] release];
	[present->weatherPools[1] release];
	for ( unsigned slot = 0; slot < MAX_RENDER_WORLDS; ++slot )
		[present->weatherHeightgrids[slot] release];
	[present->effectPools[0] release];
	[present->effectPools[1] release];
	[present->particleProfiles release];
	[present->particleChildEvents release];
	[present->worldDepthState release];
	[present->worldDepthReadState release];
	[present->worldDepthEqualState release];
	[present->worldDepthEqualReadState release];
	[present->taaMaskPipeline release];
	[present->taaMaskMaterial release];
	for ( uint32_t i = 0u; i < 3u; ++i ) [present->taaTrustPipelines[i] release];
	[present->taaMotionPipeline release];
	free( present->entityMotion.candidates ); free( present->entityMotion.previous );
	for ( i = 0u; i < MAX_RENDER_WORLDS; ++i ) {
		[present->worldCaches[i].vertices release];
		[present->worldCaches[i].indices release];
		free( present->worldCaches[i].batchBounds );
	}
	[present->worldDepthCache release];
	for ( i = 0u; i < RAL_METAL_LENS_ORACLE_RING; ++i ) {
		[present->lensInputBuffers[i] release];
		[present->lensResultBuffers[i] release];
		[present->lensCommands[i] release];
	}
	[present->sceneDepthCache release];
	[present->depthFallbackTexture release]; present->depthFallbackTexture = nil;
	[present->sceneColorCache release];
	[present->uiDepthDisabledState release];
	[present->uiLibrary release];
	[present->drawable release];
	free( present->captureRgb );
	if ( present->ownsLayer ) [present->layer release];
	memset( present, 0, sizeof( *present ) );
	free( present );
}

id<MTLBuffer> RalMetal_PresentNativeWorldBuffer( const ralMetalPresent_t *present,
		int worldIndex, qboolean indices ) {
	if ( !present || worldIndex < 0 || worldIndex >= MAX_RENDER_WORLDS
			|| !BoolValid( indices ) ) return nil;
	return indices ? present->worldCaches[worldIndex].indices
		: present->worldCaches[worldIndex].vertices;
}

id<MTLBuffer> RalMetal_PresentNativeWeatherBuffer( const ralMetalPresent_t *present ) {
	return present ? present->weatherPools[present->weatherParity ? 1u : 0u] : nil;
}

id<MTLBuffer> RalMetal_PresentNativeHeightgrid( const ralMetalPresent_t *present, int worldIndex ) {
	return present && worldIndex >= 0 && worldIndex < MAX_RENDER_WORLDS
		? present->weatherHeightgrids[worldIndex] : nil;
}

id<MTLTexture> RalMetal_PresentNativeSkySource( const ralMetalPresent_t *present ) {
	return present ? present->skySource : nil;
}

qboolean RalMetal_PresentSimulationHistory( const ralMetalPresent_t *present,
		ralMetalSimulationHistory_t *outHistory ) {
	if ( !present || !outHistory ) return qfalse;
	memset( outHistory, 0, sizeof( *outHistory ) );
	outHistory->atmosphereValid = present->atmosphereHistoryValid;
	outHistory->atmosphereParity = present->atmosphereHistoryParity;
	outHistory->weatherParity = present->weatherParity;
	outHistory->weatherTimeline = present->weatherTimeline;
	outHistory->particleParity = present->effectParity;
	outHistory->particlePlanReady = present->effectParticlePlan.ready;
	outHistory->particleTimeline = present->effectTimeline;
	outHistory->particleNextSlot = present->effectParticlePlan.nextSlot;
	return qtrue;
}

qboolean RalMetal_PresentUnloadWorld( ralMetalPresent_t *present,
		int worldIndex ) {
	ralMetalWorldCacheSlot_t *cache;
	if ( !present || worldIndex < 0 || worldIndex >= MAX_RENDER_WORLDS )
		return qfalse;
	wired_clear_present_upload_caches( present );
	cache = &present->worldCaches[worldIndex];
	[present->weatherHeightgrids[worldIndex] release];
	present->weatherHeightgrids[worldIndex] = nil;
	present->weatherHeightgridDigests[worldIndex] = 0u;
	[cache->vertices release];
	present->exposureHistoryValid = qfalse;
	[cache->indices release];
	free( cache->batchBounds );
	memset( cache, 0, sizeof( *cache ) );
	return qtrue;
}

qboolean RalMetal_PresentResetRegistrationAssets( ralMetalPresent_t *present,
		renderSubmissionState_t *frontend ) {
	if ( !present || !frontend || !frontend->initialized || frontend->frameOpen
			|| present->drawable || RenderSubmission_ResidentWorldCount( frontend ) )
		return qfalse;
	for ( uint32_t i = 0u; i < MAX_RENDER_WORLDS; ++i )
		if ( present->worldCaches[i].vertices || present->worldCaches[i].indices )
			return qfalse;
	/* This is a registration barrier, never frame-loop eviction. Keep the
	 * device, layer and PSOs while retiring every old texture borrower before
	 * the common CPU catalog releases its decoded storage. */
	if ( !ReapReadbackRing( present, qtrue )
			|| !RalMetal_PresentUnloadSkyRadianceResources( present ) ) return qfalse;
	wired_reset_lens_registration( present );
	for ( unsigned slot = 0; slot < MAX_RENDER_WORLDS; ++slot ) {
		[present->weatherHeightgrids[slot] release];
		present->weatherHeightgrids[slot] = nil;
		present->weatherHeightgridDigests[slot] = 0u;
	}
	RalMetal_BloomDestroy( present->bloom ); present->bloom = NULL;
	RalMetal_SmaaDestroy( present->smaa ); present->smaa = NULL;
	RalMetal_TaaDestroy( present->taa ); present->taa = NULL;
	Ral_TemporalInit( &present->taaState ); present->taaFrameId = 0u;
	EntityMotionReset( present );
	wired_clear_present_upload_caches( present );
	for ( uint32_t i = 0u; i < RENDER_SUBMISSION_MAX_TEXTURE_RESOURCES; ++i )
		[present->uiMaterials[i].texture release];
	memset( present->uiMaterials, 0, sizeof( present->uiMaterials ) );
	present->uiMaterialCount = 0u;
	memset( present->cookedMaterialTextureIdentities, 0,
		sizeof( present->cookedMaterialTextureIdentities ) );
	memset( present->cookedMaterialReady, 0, sizeof( present->cookedMaterialReady ) );
	present->cookedMaterialRuntimeGeneration = 0u;
	return RenderSubmission_ResetRegistrationAssets( frontend );
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

qboolean RalMetal_PresentSetAerialHaze( ralMetalPresent_t *present,
		float aerialHaze ) {
	if ( !present || present->drawable || !isfinite( aerialHaze )
			|| aerialHaze < 0.0f || aerialHaze > 1.0f ) return qfalse;
	present->aerialHaze = aerialHaze;
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

qboolean wired_metal_present_set_sampling( ralMetalPresent_t *present,
		const ralSamplerCreateInfo_t *sampling ) {
	if ( !present || present->drawable || !Ral_SamplerCreateInfoValid( sampling )
			|| sampling->compareEnable || sampling->minLod != 0.0f
			|| ( sampling->maxLod != 0.0f && sampling->maxLod != 0.25f )
			|| sampling->addressU != RAL_ADDRESS_REPEAT
			|| sampling->addressV != RAL_ADDRESS_REPEAT
			|| sampling->addressW != RAL_ADDRESS_REPEAT
			|| sampling->maxAnisotropy < 1.0f ) return qfalse;
	ralSamplerCreateInfo_t normalized = *sampling;
	normalized.maxAnisotropy = fminf( 16.0f, floorf( sampling->maxAnisotropy ) );
	const ralSamplerCreateInfo_t *old = &present->materialSampling;
	if ( present->materialClampSampler && old->minFilter == normalized.minFilter
			&& old->magFilter == normalized.magFilter
			&& old->mipmapMode == normalized.mipmapMode
			&& old->maxLod == normalized.maxLod
			&& old->maxAnisotropy == normalized.maxAnisotropy ) return qtrue;
	id<MTLDevice> device = RalMetal_CoreNativeDevice( present->core );
	MTLSamplerDescriptor *desc = [[[MTLSamplerDescriptor alloc] init] autorelease];
	desc.minFilter = normalized.minFilter == RAL_FILTER_LINEAR
		? MTLSamplerMinMagFilterLinear : MTLSamplerMinMagFilterNearest;
	desc.magFilter = normalized.magFilter == RAL_FILTER_LINEAR
		? MTLSamplerMinMagFilterLinear : MTLSamplerMinMagFilterNearest;
	desc.mipFilter = normalized.mipmapMode == RAL_MIPMAP_LINEAR
		? MTLSamplerMipFilterLinear : MTLSamplerMipFilterNearest;
	desc.maxAnisotropy = (NSUInteger)normalized.maxAnisotropy;
	desc.lodMaxClamp = normalized.maxLod > 0.0f ? normalized.maxLod : FLT_MAX;
	desc.supportArgumentBuffers = YES;
	desc.sAddressMode = desc.tAddressMode = desc.rAddressMode = MTLSamplerAddressModeClampToEdge;
	id<MTLSamplerState> clamp = [device newSamplerStateWithDescriptor:desc];
	if ( !clamp ) return qfalse;
	desc.sAddressMode = desc.tAddressMode = desc.rAddressMode = MTLSamplerAddressModeRepeat;
	id<MTLSamplerState> repeat = [device newSamplerStateWithDescriptor:desc];
	if ( !repeat ) { [clamp release]; return qfalse; }
	[present->materialClampSampler release];
	[present->materialRepeatSampler release];
	present->materialClampSampler = clamp;
	present->materialRepeatSampler = repeat;
	normalized.debugName = NULL;
	present->materialSampling = normalized;
	return qtrue;
}

qboolean RalMetal_PresentSetBloom( ralMetalPresent_t *present,
		const ralBloomSettings_t *settings ) {
	if ( !present || !Ral_BloomSettingsValid( settings ) ) return qfalse;
	present->bloomSettings = *settings;
	return qtrue;
}

void RalMetal_PresentEntityMotionStats( const ralMetalPresent_t *present, ralMetalEntityMotionStats_t *outStats ) {
	if ( !outStats ) return;
	memset( outStats, 0, sizeof( *outStats ) );
	if ( !present ) return;
	outStats->recorded = present->entityMotion.recorded;
	outStats->rejected = present->entityMotion.rejected;
	outStats->previousValid = present->entityMotion.previousValid;
	outStats->motionBatches = present->entityMotion.motionBatchesTotal;
	outStats->motionFrames = present->entityMotion.motionFrames;
}

void RalMetal_PresentTaaFrameCounts( const ralMetalPresent_t *present, uint32_t *resolved, uint32_t *bootstrap ) {
	*resolved = present ? present->taaResolvedFrames : 0u;
	*bootstrap = present ? present->taaBootstrapFrames : 0u;
}

qboolean RalMetal_PresentSetTaa( ralMetalPresent_t *present, qboolean enabled ) {
	if ( !present ) return qfalse;
	present->taaEnabled = enabled;
	return qtrue;
}

qboolean RalMetal_PresentSetSmaaQuality( ralMetalPresent_t *present, int quality ) {
	if ( !present || quality < 0 || quality > RAL_METAL_SMAA_QUALITY_ULTRA ) return qfalse;
	present->smaaQuality = quality;
	return qtrue;
}

qboolean RalMetal_PresentSetExposure( ralMetalPresent_t *present,
		const ralExposureSettings_t *settings ) {
	if ( !present || !Ral_ExposureSettingsValid( settings ) ) return qfalse;
	if ( present->exposureSettings.enabled != settings->enabled )
		present->exposureHistoryValid = qfalse;
	present->exposureSettings = *settings;
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
	/* Completed command references must not hold a drawable while acquire
	 * waits for the layer pool. This reaps only completed work, never waits. */
	if ( !ReapReadbackRing( present, qfalse ) ) return qfalse;
	/* A completed unsplit reader may still reference a drawable. Release that
	 * command before nextDrawable, retaining only the reusable upload buffer. */
	wired_reap_present_upload_readers( present );
	/* Lens readbacks also retain the present-bearing output command. Publish
	 * completed results and drop that lease before waiting for a drawable;
	 * pending GPU work stays owned and this collection never waits. */
	CollectLensVisibility( present );
	/* Acquire happens outside the submit autorelease pool. Drain the layer's
	 * temporary references here; our explicit retain owns it until submit or
	 * teardown, independently of the caller's event-loop pool lifetime. */
	@autoreleasepool {
		drawable = [[present->layer nextDrawable] retain];
	}
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
		&& r->frontendFrameGeneration != 0u
		&& r->frontendFrameGeneration != UINT64_MAX
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
		&& r->distantWorldCandidateCount <= RAL_SKY_DISTANT_MAX_CANDIDATES
		&& r->distantWorldDrawCount <= RAL_SKY_DISTANT_MAX_VISIBLE_DRAWS
		&& r->distantWorldDrawCount <= r->distantWorldCandidateCount
		&& ( r->distantWorldIndexCount % 3u ) == 0u
		&& BoolValid( r->distantWorldDepthReset )
		&& ( r->distantWorldDrawCount
			? ( r->distantWorldIndexCount > 0u
				&& r->distantWorldDepthReset == qtrue )
			: ( r->distantWorldIndexCount == 0u
				&& r->distantWorldDepthReset == qfalse ) )
		&& r->texturedWorldBatchCount <= r->loweredWorldBatchCount
		&& r->lightmappedWorldBatchCount <= r->loweredWorldBatchCount
		&& r->legacyLightmapDrawCount + r->directionalStaticDrawCount
			<= r->loweredWorldBatchCount
		&& ( ( r->legacyLightmapDrawCount || r->directionalStaticDrawCount )
			? r->surfaceLightingBindingDigest != 0u
			: r->surfaceLightingBindingDigest == 0u )
		&& r->submittedDirectLightCount <= RENDER_SUBMISSION_MAX_LIGHTS
		&& r->directLightDrawCount
			<= r->loweredWorldBatchCount + r->loweredEntityBatchCount
		&& r->directLightReferenceCount
			<= r->directLightDrawCount * RAL_METAL_MAX_DIRECT_LIGHTS_PER_DRAW
		&& ( !r->directLightReferenceCount || r->submittedDirectLightCount > 0u )
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
		&& r->loweredEffectBeamCount <= RENDER_SUBMISSION_MAX_EFFECT_BEAMS
		&& r->effectEmitterDispatchCount <= RENDER_SUBMISSION_MAX_EFFECT_EMITTERS
		&& r->effectParticleDrawCount <= RAL_PARTICLE_POOL_CAPACITY
		&& ( r->effectParticle.schemaVersion
			? ( r->effectParticle.schemaVersion
					== RENDER_EFFECT_PARTICLE_PLAN_SCHEMA_VERSION
				&& r->effectParticle.frameGeneration
					== r->frontendFrameGeneration
				&& r->effectParticle.admittedEmitterCount
					== r->effectEmitterDispatchCount
				&& r->effectParticle.admittedParticleCount
					<= RAL_PARTICLE_POOL_CAPACITY
				&& r->effectParticle.ready == qtrue )
			: ( r->effectEmitterDispatchCount == 0u
				&& r->effectParticleDrawCount == 0u ) )
		&& r->loweredUiPrimitiveCount <= RENDER_SUBMISSION_MAX_UI_PRIMITIVES
		&& r->texturedUiPrimitiveCount <= r->loweredUiPrimitiveCount
		&& r->msdfUiPrimitiveCount <= r->texturedUiPrimitiveCount
		&& r->atmosphereDispatchCount <= 5u
		&& r->atmosphereFroxelCount <= RAL_METAL_ATMOSPHERE_FROXEL_CAPACITY
		&& r->atmosphereCompositeCount <= 1u
		&& BoolValid( r->atmosphereCloudsActive )
		&& Ral_AtmospherePlanReceiptExact( &r->atmosphere,
			&r->atmosphere )
		&& r->atmosphere.frameGeneration == r->frontendFrameGeneration
		&& ( r->atmosphere.selectedTier == RAL_ATMOSPHERE_TIER_FULL
			? ( r->atmosphere.computeDispatchCount + ( r->atmosphere.computeDispatchCount ? 1u : 0u )
					== r->atmosphereDispatchCount
				&& r->atmosphere.froxelCount == r->atmosphereFroxelCount
				&& r->atmosphere.compositeCount
					== r->atmosphereCompositeCount
				&& r->atmosphere.cloudsActive
					== r->atmosphereCloudsActive )
			: ( r->atmosphereDispatchCount == 0u
				&& r->atmosphereFroxelCount == 0u
				&& r->atmosphereCompositeCount == 0u
				&& r->atmosphereCloudsActive == qfalse ) )
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
		&& a->frontendFrameGeneration == b->frontendFrameGeneration
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
		&& a->distantWorldCandidateCount == b->distantWorldCandidateCount
		&& a->distantWorldDrawCount == b->distantWorldDrawCount
		&& a->distantWorldIndexCount == b->distantWorldIndexCount
		&& a->distantWorldDepthReset == b->distantWorldDepthReset
		&& a->texturedWorldBatchCount == b->texturedWorldBatchCount
		&& a->lightmappedWorldBatchCount == b->lightmappedWorldBatchCount
		&& a->legacyLightmapDrawCount == b->legacyLightmapDrawCount
		&& a->directionalStaticDrawCount == b->directionalStaticDrawCount
		&& a->surfaceLightingBindingDigest == b->surfaceLightingBindingDigest
		&& a->submittedDirectLightCount == b->submittedDirectLightCount
		&& a->directLightDrawCount == b->directLightDrawCount
		&& a->directLightReferenceCount == b->directLightReferenceCount
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
		&& !memcmp( &a->effectParticle, &b->effectParticle,
			sizeof( a->effectParticle ) )
		&& a->lensSourceCount == b->lensSourceCount
		&& a->lensVisibilityDispatchCount == b->lensVisibilityDispatchCount
		&& a->loweredUiPrimitiveCount == b->loweredUiPrimitiveCount
		&& a->texturedUiPrimitiveCount == b->texturedUiPrimitiveCount
		&& a->msdfUiPrimitiveCount == b->msdfUiPrimitiveCount
		&& a->atmosphereDispatchCount == b->atmosphereDispatchCount
		&& a->atmosphereFroxelCount == b->atmosphereFroxelCount
		&& a->atmosphereCompositeCount == b->atmosphereCompositeCount
		&& a->atmosphereCloudsActive == b->atmosphereCloudsActive
		&& Ral_AtmospherePlanReceiptExact( &a->atmosphere,
			&b->atmosphere )
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

typedef struct {
	uint64_t generation;
	ralMetalDrawableReceipt_t acquired;
	qboolean unavailable;
	qboolean measure;
	qboolean sampleGpuAcquire;
	CFTimeInterval acquireBeginSeconds, acquireEndSeconds;
	uint64_t beginNs, encodeNs, acquireBeginNs, acquireEndNs, submitNs, committedNs;
} ralMetalLateAcquire_t;

/* Acquire exactly where the first drawable-backed pass begins. Offscreen
 * scene/FX/exposure work needs no drawable; its committed simulation state
 * is already published before this wait in the split-submission route. */
static qboolean AcquireForOutput( ralMetalPresent_t *present,
		const ralMetalCoreReceipt_t *coreReceipt,
		const ralMetalPresentLayerReceipt_t *layerReceipt,
		ralMetalLateAcquire_t *late, ralMetalPresentReceipt_t *receipt ) {
	if ( !late ) return qtrue; // explicit-acquire caller already owns its drawable
	if ( late->measure ) late->acquireBeginNs = clock_gettime_nsec_np( CLOCK_UPTIME_RAW );
	if ( late->sampleGpuAcquire ) late->acquireBeginSeconds = CACurrentMediaTime();
	const qboolean acquired = RalMetal_PresentAcquire( present, coreReceipt,
		layerReceipt, late->generation, &late->acquired );
	if ( late->sampleGpuAcquire ) late->acquireEndSeconds = CACurrentMediaTime();
	if ( late->measure )
		late->acquireEndNs = clock_gettime_nsec_np( CLOCK_UPTIME_RAW );
	if ( !acquired ) { late->unavailable = qtrue; return qfalse; }
	receipt->drawableIdentity = late->acquired.drawableIdentity;
	receipt->textureIdentity = late->acquired.textureIdentity;
	return qtrue;
}

/* One logical RAL submission may span two ordered native command buffers.
 * If output preparation fails after the scene commit, drain that committed
 * work before frame cancellation/unload can reuse its resources. Successful
 * output stays asynchronous: its same-queue completion covers both parts. */
struct wiredMetalSplitSubmission {
	id<MTLCommandBuffer> scene = nil;
	bool outputCommitted = false;
	~wiredMetalSplitSubmission() {
		if ( scene && !outputCommitted ) [scene waitUntilCompleted];
	}
};

static qboolean RenderAndSubmit( ralMetalPresent_t *present,
		const ralMetalCoreReceipt_t *coreReceipt,
		const ralMetalPresentLayerReceipt_t *layerReceipt,
		const ralMetalDrawableReceipt_t *drawableReceipt,
		ralMetalLateAcquire_t *lateAcquire, renderSubmissionState_t *frontend,
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
	id<MTLBuffer> distantWorldVertices = nil;
	id<MTLBuffer> distantWorldIndices = nil;
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
	/* Zero = identity media for frames that draw weather without a world. */
	memset( &worldAtmosphereParams, 0, sizeof( worldAtmosphereParams ) );
	ralMetalWorldViewParams_t distantWorldViewParams;
	ralMetalWorldAtmosphereParams_t distantWorldAtmosphereParams;
	renderDistantWorldSubmission_t distantWorld;
	ralAtmospherePlanReceipt_t atmospherePlan;
	ralAtmosphereWeatherReceipt_t weatherPlan;
	ralMetalWorldVertex_t *worldVertexBytes = NULL;
	ralMetalWorldVertex_t *entityVertexBytes = NULL;
	uint32_t *entityIndexBytes = NULL;
	ralMetalEntityBatch_t *entityBatches = NULL;
	const renderWorldBatch_t *worldBatches = NULL;
	ralMetalWorldCacheSlot_t *worldCache = NULL;
	const renderWorldBatch_t *distantWorldBatches = NULL;
	ralMetalWorldCacheSlot_t *distantWorldCache = NULL;
	renderEffectPrimitiveSnapshot_t effectSnapshot;
	renderLensSourceSnapshot_t lensSnapshot;
	const lensSourceDesc_t *lensSources = NULL;
	const spriteDesc_t *effectSprites;
	const emitterDesc_t *effectEmitters;
	const decalDesc_t *effectDecals;
	const renderEffectRibbonCommand_t *effectRibbons;
	const ribbonPoint_t *effectRibbonPoints;
	const beamDesc_t *effectBeams;
	uint32_t uiPrimitiveCount = 0u;
	uint32_t worldVertexCount = 0u, worldIndexCount = 0u, worldBatchCount = 0u;
	uint32_t distantWorldVertexCount = 0u, distantWorldIndexCount = 0u;
	uint32_t distantWorldBatchCount = 0u;
	uint32_t entityVertexCount = 0u, entityIndexCount = 0u, entityBatchCount = 0u;
	uint32_t readbackBytes;
	uint32_t readbackSlot = 0u;
	NSUInteger captureBytesPerRow = 0u;
	uint32_t i;
	float maxRgb;
	qboolean sampleReadback;
	qboolean offscreenScene = qfalse;
	qboolean genericEffects = qfalse;
	ralMetalWeatherFrame_t weatherFrame = {};
	renderAtmosphereEffectWorkloadSnapshot_t weatherEffects = {};
	id<MTLTexture> weatherTextures[RENDER_SUBMISSION_MAX_ATMOSPHERE_EFFECT_WORKLOADS];
	id<MTLSamplerState> weatherSamplers[RENDER_SUBMISSION_MAX_ATMOSPHERE_EFFECT_WORKLOADS];
	qboolean physicalSkyReadable = qfalse;
	qboolean distantWorldReady = qfalse;
	uint32_t genericEffectParticleCount = 0u;
	uint32_t genericEffectDroppedCount = 0u;
	float genericEffectActiveUntil = 0.0f;
	id<MTLBuffer> effectParticleFrameBuffer = nil;
	id<MTLBuffer> effectParticleClassBuffer = nil;
	uint32_t effectParticleRenderedPool = 0u;
	renderEffectParticlePlanState_t effectParticleCandidate;
	renderEffectParticlePlanReceipt_t effectParticleReceipt;
	qboolean effectParticleCandidateReady = qfalse;
	qboolean effectParticlePoolAdvanced = qfalse;
	uint32_t persistentEffectDecalCount = 0u;
	uint32_t persistentEffectDecalDroppedCount = 0u;
	uint32_t persistentEffectBeamCount = 0u;
	uint32_t persistentEffectBeamDroppedCount = 0u;
	uint64_t frontendFrameGeneration = 0u;
	MTLPixelFormat worldColorFormat;
	if ( !outReceipt || !clearColor || !OwnerMatches( present, coreReceipt, layerReceipt )
			|| ( lateAcquire
				? ( present->drawable || !lateAcquire->generation
					|| lateAcquire->generation == UINT64_MAX
					|| lateAcquire->generation <= present->lastAcquireGeneration
					|| presentGeneration <= lateAcquire->generation )
				: ( !present->drawable || !drawableReceipt
					|| !RalMetal_DrawableReceiptExact( drawableReceipt, &present->drawableReceipt )
					|| presentGeneration <= drawableReceipt->acquireGeneration ) )
			|| presentGeneration == 0u || presentGeneration == UINT64_MAX
			|| presentGeneration <= present->lastSubmittedGeneration ) return PresentFailure( "precondition" );
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
	receipt.acquireGeneration = lateAcquire ? lateAcquire->generation
		: drawableReceipt->acquireGeneration;
	receipt.presentGeneration = presentGeneration; receipt.ownerIdentity = (uintptr_t)present;
	if ( frontend && !RenderSubmission_ReadableFrameGeneration( frontend,
			&frontendFrameGeneration ) ) return PresentFailure( "frontend-generation" );
	receipt.frontendFrameGeneration = frontend
		? frontendFrameGeneration : presentGeneration;
	receipt.layerIdentity = layerReceipt->layerIdentity;
	/* No invented native identities: the unpublished local receipt remains
	 * incomplete until the real layer drawable has been acquired. */
	if ( drawableReceipt ) {
		receipt.drawableIdentity = drawableReceipt->drawableIdentity;
		receipt.textureIdentity = drawableReceipt->textureIdentity;
	}
	receipt.format = layerReceipt->selected.format; receipt.colorSpace = layerReceipt->selected.colorSpace;
	receipt.presentMode = layerReceipt->selected.presentMode;
	receipt.width = layerReceipt->selected.width;
	receipt.height = layerReceipt->selected.height;
	receipt.displayVisibility = present->displayVisibility;
	memset( &atmospherePlan, 0, sizeof( atmospherePlan ) );
	memset( &distantWorld, 0, sizeof( distantWorld ) );
	if ( frontend ) {
		if ( !RalMetal_PresentPlanAtmosphere( present, frontend,
				receipt.frontendFrameGeneration, receipt.width, receipt.height,
				&atmospherePlan ) )
			return PresentFailure( "atmosphere-plan" );
	} else {
		ralAtmospherePlanRequest_t emptyAtmosphere;
		memset( &emptyAtmosphere, 0, sizeof( emptyAtmosphere ) );
		emptyAtmosphere.schemaVersion = RAL_ATMOSPHERE_PLAN_SCHEMA_VERSION;
		emptyAtmosphere.backendType = RAL_BACKEND_METAL;
		emptyAtmosphere.frameGeneration = receipt.frontendFrameGeneration;
		emptyAtmosphere.width = receipt.width;
		emptyAtmosphere.height = receipt.height;
		emptyAtmosphere.requestedTier = RAL_ATMOSPHERE_TIER_OFF;
		emptyAtmosphere.maxLocalVolumes = RAL_ATMOSPHERE_MAX_VOLUMES;
		emptyAtmosphere.maxLights = RAL_ATMOSPHERE_MAX_LIGHTS;
		emptyAtmosphere.maxShadowedLights =
			RAL_ATMOSPHERE_MAX_SHADOWED_LIGHTS;
		if ( !Ral_AtmospherePlan( &emptyAtmosphere, &atmospherePlan ) )
			return PresentFailure( "atmosphere-empty-plan" );
	}
	receipt.atmosphere = atmospherePlan;
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
	if ( lateAcquire ) lateAcquire->sampleGpuAcquire = sampleReadback;
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
	if ( worldIndexCount )
		worldCache = &present->worldCaches[frontend->activeWorldIndex];
	if ( frontend && RenderSubmission_ViewSnapshot( frontend, &sceneView )
			&& atmospherePlan.skyEnvironment.distantSceneActive ) {
		const int distantWorldIndex = RENDER_DISTANT_WORLD_BASE
			+ frontend->activeWorldIndex;
		if ( !RenderSubmission_DistantWorldSubmissionForActiveView( frontend,
				&atmospherePlan.skyEnvironment,
				RAL_SKY_DISTANT_MAX_LOD_LEVELS,
				RAL_SKY_DISTANT_MAX_VISIBLE_DRAWS,
				present->distantMetricScratch,
				RAL_SKY_DISTANT_MAX_CANDIDATES,
				present->distantCandidateScratch,
				RAL_SKY_DISTANT_MAX_CANDIDATES, &distantWorld )
				|| distantWorld.backendType != RAL_BACKEND_METAL
				|| !PrepareWorldSnapshotBuffers( present, &distantWorld.world,
					distantWorldIndex, distantWorld.contentIdentity,
					&distantWorldVertices, &distantWorldIndices,
					&distantWorldBatches, &distantWorldVertexCount,
					&distantWorldIndexCount, &distantWorldBatchCount )
				|| !BuildWorldViewParamsFromSnapshot( &distantWorld.world,
					&distantWorldViewParams, NULL ) ) {
			[distantWorldVertices release]; [distantWorldIndices release];
			[worldVertices release]; [worldIndices release];
			free( uiVertexBytes );
			return PresentFailure( "distant-world" );
		}
		/* The per-world cache owns these buffers. Balance the preparation API's
		 * caller retain now so every later failure path remains leak-free while
		 * the borrowed identities stay alive for the present. */
		[distantWorldVertices autorelease]; [distantWorldIndices autorelease];
		distantWorldCache = &present->worldCaches[distantWorldIndex];
		receipt.distantWorldCandidateCount =
			distantWorld.drawPlan.candidateCount;
		receipt.distantWorldDrawCount = distantWorld.drawPlan.admittedCount;
		distantWorldReady = receipt.distantWorldDrawCount ? qtrue : qfalse;
		for ( uint32_t selection = 0u;
				selection < distantWorld.drawPlan.admittedCount; ++selection ) {
			const uint32_t batchIndex = distantWorld.drawPlan
				.selections[selection].sourceDrawIndex;
			if ( batchIndex >= distantWorldBatchCount ) {
				[worldVertices release]; [worldIndices release];
				free( uiVertexBytes );
				return PresentFailure( "distant-world-selection" );
			}
			receipt.distantWorldIndexCount +=
				distantWorldBatches[batchIndex].indexCount;
		}
	}
	if ( frontend && !PreparePersistentEffects( present, frontend,
			receipt.frontendFrameGeneration, &persistentEffectDecalCount,
			&persistentEffectDecalDroppedCount, &persistentEffectBeamCount,
			&persistentEffectBeamDroppedCount ) ) {
		[worldVertices release]; [worldIndices release];
		free( worldVertexBytes ); free( uiVertexBytes );
		return PresentFailure( "effect-pools" );
	}
	const uint32_t effectCandidateIndex = present->effectPoolIndex ^ ( frontend ? 1u : 0u );
	if ( !BuildEntityVertices( present, &present->effectDecalPools[effectCandidateIndex],
			&present->effectBeamPools[effectCandidateIndex], frontend,
			&entityVertexBytes, &entityIndexBytes,
			&entityBatches, &entityVertexCount, &entityIndexCount,
			&entityBatchCount, &receipt.modelEntityCount,
			&receipt.primitiveEntityCount, &receipt.temporalEntityCount,
			&receipt.localIrradianceEntityCount,
			&receipt.unresolvedEntityCount, &present->entityMotion ) ) {
		[worldVertices release]; [worldIndices release];
		free( worldVertexBytes ); free( uiVertexBytes ); return PresentFailure( "entity-vertices" );
	}
	memset( &effectSnapshot, 0, sizeof( effectSnapshot ) );
	if ( frontend && !RenderSubmission_EffectPrimitiveSnapshots( frontend,
			&effectSnapshot, &effectSprites, &effectEmitters, &effectDecals,
			&effectRibbons, &effectRibbonPoints, &effectBeams ) ) {
		free( entityBatches ); free( entityIndexBytes ); free( entityVertexBytes );
		[worldVertices release]; [worldIndices release];
		free( worldVertexBytes ); free( uiVertexBytes );
		return PresentFailure( "effect-snapshot" );
	}
	memset( &lensSnapshot, 0, sizeof( lensSnapshot ) );
	if ( frontend && !RenderSubmission_LensSourceSnapshot( frontend,
			&lensSnapshot, &lensSources ) ) {
		free( entityBatches ); free( entityIndexBytes ); free( entityVertexBytes );
		[worldVertices release]; [worldIndices release];
		free( worldVertexBytes ); free( uiVertexBytes );
		return PresentFailure( "lens-snapshot" );
	}
	(void)lensSources;
	receipt.lensSourceCount = lensSnapshot.count;
	if ( frontend ) {
		const renderPolyCommand_t *polygons;
		const polyVert_t *polyVertices;
		const renderLightCommand_t *lights;
		uint32_t polygonCount, polyVertexCount, lightCount;
		if ( !RenderSubmission_EffectSnapshots( frontend, &polygons,
				&polygonCount, &polyVertices, &polyVertexCount,
				&lights, &lightCount ) ) {
			free( entityBatches ); free( entityIndexBytes );
			free( entityVertexBytes ); free( worldVertexBytes );
			[worldVertices release]; [worldIndices release];
			free( uiVertexBytes ); return PresentFailure( "light-snapshot" );
		}
		(void)polygons; (void)polygonCount; (void)polyVertices;
		(void)polyVertexCount;
		for ( uint32_t lightIndex = 0u; lightIndex < lightCount; ++lightIndex )
			if ( lights[lightIndex].sourceFlags & RAL_LIGHT_CONTRIBUTE_DIRECT )
				receipt.submittedDirectLightCount++;
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
		[worldVertices release]; [worldIndices release];
		free( worldVertexBytes ); free( uiVertexBytes ); return PresentFailure( "entity-irradiance" );
	}
	receipt.loweredUiPrimitiveCount = uiPrimitiveCount;
	receipt.loweredWorldIndexCount = worldIndexCount
		+ receipt.distantWorldIndexCount;
	receipt.loweredWorldBatchCount = worldBatchCount
		+ receipt.distantWorldDrawCount;
	receipt.loweredEntityIndexCount = entityIndexCount;
	receipt.loweredEntityBatchCount = entityBatchCount;
	physicalSkyReadable = present->skyResourcesReady
		&& present->skyPhysical[RAL_METAL_SKY_VIEW] != nil
		&& present->skyPhysicalSampler != nil ? qtrue : qfalse;
	memset( &sceneView, 0, sizeof( sceneView ) );
	if ( ( worldIndexCount || distantWorldReady || entityIndexCount || genericEffects
			|| lensSnapshot.count )
			&& ( !BuildWorldViewParams( frontend, &worldViewParams, &sceneView )
				|| !BuildWorldAtmosphereParams( frontend, &worldViewParams,
					&atmospherePlan, &present->displayVisibility,
					physicalSkyReadable,
					&present->skyRadianceConsumer, present->aerialHaze,
					&worldAtmosphereParams ) ) ) {
		free( entityBatches ); free( entityIndexBytes ); free( entityVertexBytes );
		[worldVertices release]; [worldIndices release];
		free( worldVertexBytes ); free( uiVertexBytes ); return PresentFailure( "world-view" );
	}
	if ( distantWorldReady
			&& !BuildWorldAtmosphereParams( frontend, &distantWorldViewParams,
				&atmospherePlan, &present->displayVisibility,
				physicalSkyReadable, &present->skyRadianceConsumer,
				present->aerialHaze, &distantWorldAtmosphereParams ) ) {
		free( entityBatches ); free( entityIndexBytes ); free( entityVertexBytes );
		[worldVertices release]; [worldIndices release];
		free( worldVertexBytes ); free( uiVertexBytes );
		return PresentFailure( "distant-world-atmosphere" );
	}
	offscreenScene = ( worldIndexCount || distantWorldReady || entityIndexCount
		|| !weatherPlan.zeroWork
		|| genericEffects )
		&& !( sceneView.rdflags & RDF_NOWORLDMODEL ) ? qtrue : qfalse;
	worldColorFormat = offscreenScene ? MTLPixelFormatRGBA16Float
		: present->layer.pixelFormat;
	/* Temporal AA: plan the frame (jitter, history slots, resets) and bake the sub-pixel jitter into the world view parameters before
	 * any pass uses them. A plan abandoned by an earlier failed frame is cancelled first. */
	qboolean taaActive = qfalse, taaEntityMotion = qfalse;
	ralTemporalFramePlan_t taaPlan;
	float taaJitterNdc[2] = { 0.0f, 0.0f };
	memset( &taaPlan, 0, sizeof( taaPlan ) );
	if ( present->taaState.pending )
		(void)Ral_TemporalCancelFrame( &present->taaState, present->taaState.pendingPlan.frameId );
	EntityMotionFinish( present, qfalse );   /* a transaction left open by a failed frame */
	/* TAA switched off (r_aa changed): drop the history so a later switch-on starts from the bootstrap frame, not a stale image. */
	if ( !present->taaEnabled && present->taaState.active ) {
		Ral_TemporalInit( &present->taaState );
		present->taaFrameId = 0u;
		EntityMotionReset( present );
	}
	if ( present->taaEnabled && offscreenScene && receipt.width && receipt.height ) {
		ralTemporalFrameInput_t taaInput;
		memset( &taaInput, 0, sizeof( taaInput ) );
		if ( present->taaWorldDigest != frontend->worldDigest ) {
			present->taaWorldDigest = frontend->worldDigest;
			++present->taaTopologyEpoch;
			if ( !present->taaTopologyEpoch ) present->taaTopologyEpoch = 1u;
		}
		taaInput.frameId = present->taaFrameId + 1u;
		taaInput.width = receipt.width; taaInput.height = receipt.height;
		taaInput.viewId = 0u; taaInput.topologyEpoch = present->taaTopologyEpoch;
		taaInput.enabled = 1u;
		taaInput.cameraCut = ( sceneView.rdflags & ( RDF_CAMERA_CUT | RDF_HYPERSPACE ) ) ? 1u : 0u;
		if ( Ral_TemporalBeginFrame( &present->taaState, &taaInput, &taaPlan ) ) {
			taaActive = qtrue;
			taaJitterNdc[0] = 2.0f * taaPlan.sceneJitterPixels[0] / (float)receipt.width;
			taaJitterNdc[1] = -2.0f * taaPlan.sceneJitterPixels[1] / (float)receipt.height;
			worldViewParams.forward[3] = taaJitterNdc[0];
			worldViewParams.left[3] = taaJitterNdc[1];
			distantWorldViewParams.forward[3] = taaJitterNdc[0];
			distantWorldViewParams.left[3] = taaJitterNdc[1];
			taaEntityMotion = EntityMotionPrepare( present, frontend, entityVertexCount, entityBatches,
				entityBatchCount, taaPlan.frameId );
		}
	}
	@autoreleasepool {
		wiredMetalSplitSubmission split;
		/* Phase filters draw each main/distant world and entity batch at most
		 * once. Invisible batches reserve capacity but incur no packet upload. */
		wiredMetalDrawUniforms uniforms( present, (NSUInteger)worldBatchCount
			+ distantWorldBatchCount + entityBatchCount );
		wiredMetalUpload entityVertexUpload( present->entityVertexUploadCache );
		wiredMetalUpload entityIndexUpload( present->entityIndexUploadCache );
		wiredMetalUpload uiVertexUpload( present->uiVertexUploadCache );
		wiredMetalParticleUploads particleUploads( present );
		wiredMetalUpload atmosphereUpload( present->atmosphereUploadCache );
		CFTimeInterval firstCommitTime = 0.0;
		bool simulationCommitted = false;
		auto wired_publish_simulation = [&]() {
			if ( simulationCommitted ) return;
			simulationCommitted = true;
			present->lastSubmittedGeneration = presentGeneration;
			if ( receipt.atmosphereDispatchCount ) {
				present->atmosphereHistoryValid = qtrue;
				present->atmosphereHistoryParity = present->atmosphereHistoryParity ? qfalse : qtrue;
			}
			if ( receipt.weatherDispatchCount ) {
				present->weatherParity = present->weatherParity ? qfalse : qtrue;
				present->weatherTimeline = weatherFrame.dtTimePoolRead[1];
			}
			if ( effectParticleCandidateReady ) {
				present->effectParticlePlan = effectParticleCandidate;
				if ( effectParticlePoolAdvanced )
					present->effectParity = present->effectParity ? qfalse : qtrue;
				present->effectTimeline = effectParticleCandidate.timelineSeconds;
				present->effectActiveUntil = genericEffectActiveUntil;
			}
			if ( frontend ) present->effectPoolIndex = effectCandidateIndex;
			present->exposureHistoryValid = offscreenScene && present->exposureSettings.enabled
				? qtrue : qfalse;
			if ( present->exposureHistoryValid ) {
				present->exposureTimeMs = sceneView.timeMs;
				present->exposureWorldDigest = frontend->worldDigest;
				present->exposureWidth = receipt.width;
				present->exposureHeight = receipt.height;
			}
		};
		if ( worldIndexCount || distantWorldReady || entityIndexCount || !weatherPlan.zeroWork
				|| genericEffects ) {
			if ( !weatherPlan.zeroWork
					&& !EnsureWeatherPipeline( present, worldColorFormat ) ) {
				free( entityBatches ); free( entityIndexBytes ); free( entityVertexBytes );
				[worldVertices release]; [worldIndices release];
				free( worldVertexBytes ); free( uiVertexBytes );
				return PresentFailure( "weather-pipeline" );
			}
			if ( genericEffects
					&& !EnsureParticlePipeline( present, worldColorFormat ) ) {
				free( entityBatches ); free( entityIndexBytes ); free( entityVertexBytes );
				[worldVertices release]; [worldIndices release];
				free( worldVertexBytes ); free( uiVertexBytes );
				return PresentFailure( "particle-pipeline" );
			}
			if ( ( worldIndexCount || distantWorldReady || entityIndexCount )
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
				entityVertices = wired_upload_bytes( entityVertexUpload,
					RalMetal_CoreNativeDevice( present->core ), entityVertexBytes,
					(NSUInteger)entityVertexCount * sizeof( *entityVertexBytes ),
					@"Wired entity vertices" );
				entityIndices = wired_upload_bytes( entityIndexUpload,
					RalMetal_CoreNativeDevice( present->core ), entityIndexBytes,
					(NSUInteger)entityIndexCount * sizeof( *entityIndexBytes ),
					@"Wired entity indices" );
			}
			if ( !present->worldDepthCache || present->worldDepthWidth != receipt.width
					|| present->worldDepthHeight != receipt.height ) {
				MTLTextureDescriptor *depth = [MTLTextureDescriptor
					texture2DDescriptorWithPixelFormat:MTLPixelFormatDepth32Float
					width:receipt.width height:receipt.height mipmapped:NO];
				depth.storageMode = MTLStorageModePrivate;
				depth.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
				id<MTLTexture> replacement = [RalMetal_CoreNativeDevice( present->core )
					newTextureWithDescriptor:depth];
				if ( !replacement ) {
					[worldVertices release]; [worldIndices release];
					[entityVertices release]; [entityIndices release]; free( entityBatches );
					free( worldVertexBytes ); free( entityVertexBytes ); free( entityIndexBytes );
					free( uiVertexBytes ); return qfalse;
				}
				[present->worldDepthCache release];
				present->worldDepthCache = replacement;
				present->worldDepthWidth = receipt.width;
				present->worldDepthHeight = receipt.height;
			}
			worldDepth = [present->worldDepthCache retain];
			if ( ( genericEffects || !weatherPlan.zeroWork || entityIndexCount ) && !EnsureSceneDepth( present,
					receipt.width, receipt.height ) ) {
				[worldVertices release]; [worldIndices release]; [worldDepth release];
				[entityVertices release]; [entityIndices release]; free( entityBatches );
				free( worldVertexBytes ); free( entityVertexBytes ); free( entityIndexBytes );
				free( uiVertexBytes ); return PresentFailure( "particle-scene-depth" );
			}
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
			uiVertices = wired_upload_bytes( uiVertexUpload,
				RalMetal_CoreNativeDevice( present->core ), uiVertexBytes,
				(NSUInteger)uiPrimitiveCount * 6u * sizeof( *uiVertexBytes ),
				@"Wired UI vertices" );
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
		if ( lateAcquire && lateAcquire->measure )
			lateAcquire->encodeNs = clock_gettime_nsec_np( CLOCK_UPTIME_RAW );
		id<MTLCommandBuffer> command = [queue commandBuffer];
		if ( !command || !RalMetal_CoreBeginCommand( present->core, presentGeneration,
				&lifecycle, &recording ) ) {
			[worldVertices release]; [worldIndices release]; [worldDepth release];
			[entityVertices release]; [entityIndices release]; free( entityBatches );
			[uiVertices release]; [readback release]; return qfalse;
		}
		if ( frontend && atmospherePlan.computeDispatchCount != 0u
				&& !EncodeAtmosphere( present, command, frontend, &atmospherePlan, atmosphereUpload ) ) {
			[worldVertices release]; [worldIndices release]; [worldDepth release];
			[entityVertices release]; [entityIndices release]; free( entityBatches );
			[uiVertices release]; [readback release]; return qfalse;
		}
		if ( frontend && !weatherPlan.zeroWork
				&& !EncodeWeather( present, command, frontend, &weatherPlan, &weatherFrame, &weatherEffects ) ) {
			[worldVertices release]; [worldIndices release]; [worldDepth release];
			[entityVertices release]; [entityIndices release]; free( entityBatches );
			[uiVertices release]; [readback release]; return qfalse;
		}
		if ( !weatherPlan.zeroWork ) receipt.weatherDispatchCount = 1u;
		if ( !weatherPlan.zeroWork ) {
			for ( unsigned i = 0; i < RENDER_SUBMISSION_MAX_ATMOSPHERE_EFFECT_WORKLOADS; ++i ) {
				// Unused slots only: precipitation never samples a texture.
				weatherTextures[i] = present->uiFallbackTexture;
				weatherSamplers[i] = present->uiClampSampler;
				if ( i >= weatherEffects.admittedEmitterCount ) continue;
				qboolean textured = qfalse, msdf = qfalse;
				const qhandle_t material = weatherEffects.workloads[i].particleClass.shader;
				if ( material <= 0 || !UiMaterial( present, frontend, material,
						&weatherTextures[i], &weatherSamplers[i], &textured, &msdf )
						|| !textured || msdf ) {
					[worldVertices release]; [worldIndices release]; [worldDepth release];
					[entityVertices release]; [entityIndices release]; free( entityBatches );
					[uiVertices release]; [readback release];
					return PresentFailure( "weather-material" );
				}
			}
		}
		if ( frontend && genericEffects
				&& !EncodeRalParticles( present, particleUploads, command, frontend, &sceneView,
					frontendFrameGeneration,
					&receipt.effectEmitterDispatchCount,
					&genericEffectParticleCount,
					&genericEffectDroppedCount, &genericEffectActiveUntil,
					&effectParticlePoolAdvanced, &effectParticleFrameBuffer,
					&effectParticleClassBuffer, &effectParticleRenderedPool,
					&effectParticleCandidate,
					&effectParticleReceipt ) ) {
			[worldVertices release]; [worldIndices release]; [worldDepth release];
			[entityVertices release]; [entityIndices release]; free( entityBatches );
			[uiVertices release]; [readback release]; return qfalse;
		}
		receipt.effectPrimitiveDroppedCount += genericEffectDroppedCount;
		if ( genericEffects ) {
			receipt.effectParticle = effectParticleReceipt;
			effectParticleCandidateReady = qtrue;
		}
		if ( atmospherePlan.selectedTier == RAL_ATMOSPHERE_TIER_FULL ) {
			/* Native dispatches include the shared GPU light-tile classifier. */
			receipt.atmosphereDispatchCount = atmospherePlan.computeDispatchCount
				+ ( atmospherePlan.computeDispatchCount ? 1u : 0u );
			receipt.atmosphereFroxelCount = atmospherePlan.froxelCount;
			receipt.atmosphereCompositeCount = atmospherePlan.compositeCount;
			receipt.atmosphereCloudsActive = atmospherePlan.cloudsActive;
		}
		if ( ( worldIndexCount || entityIndexCount )
				&& atmospherePlan.computeDispatchCount == 0u
				&& !EnsureAtmosphereFallback( present ) ) {
			[worldVertices release]; [worldIndices release]; [worldDepth release];
			[entityVertices release]; [entityIndices release]; free( entityBatches );
			[uiVertices release]; [readback release]; return qfalse;
		}
		if ( !offscreenScene && !AcquireForOutput( present, coreReceipt,
				layerReceipt, lateAcquire, &receipt ) ) {
			[worldVertices release]; [worldIndices release]; [worldDepth release];
			[entityVertices release]; [entityIndices release]; free( entityBatches );
			[uiVertices release]; [readback release]; return qfalse;
		}
		/* Translucent entity batches (no depth write, not decals) draw after the
		 * opaque scene-depth copy, fogged at the depth behind them. */
		uint32_t translucentEntityCount = 0u;
		if ( !( sceneView.rdflags & RDF_NOWORLDMODEL ) )
			for ( uint32_t b = 0u; b < entityBatchCount; ++b )
				if ( entityBatches[b].visible && !entityBatches[b].surfaceDecal
						&& !entityBatches[b].depthWrite ) translucentEntityCount++;
		MTLRenderPassDescriptor *pass = [MTLRenderPassDescriptor renderPassDescriptor];
		pass.colorAttachments[0].texture = offscreenScene
			? present->sceneColorCache : present->drawable.texture;
		pass.colorAttachments[0].loadAction = MTLLoadActionClear;
		pass.colorAttachments[0].storeAction = MTLStoreActionStore;
		pass.colorAttachments[0].clearColor = MTLClearColorMake( clearColor[0], clearColor[1],
			clearColor[2], clearColor[3] );
		if ( worldIndexCount || distantWorldReady || entityIndexCount || !weatherPlan.zeroWork
				|| genericEffectParticleCount || lensSnapshot.count ) {
			pass.depthAttachment.texture = worldDepth;
			pass.depthAttachment.loadAction = MTLLoadActionClear;
			/* Stored whenever a later pass samples the opaque depth: WiredFX
			 * soft particles, the lens oracle, and the weather quads (fogged
			 * and faded at the scene depth behind them). */
			pass.depthAttachment.storeAction = ( genericEffectParticleCount || taaActive
				|| lensSnapshot.count || !weatherPlan.zeroWork || translucentEntityCount )
				? MTLStoreActionStore : MTLStoreActionDontCare;
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
		if ( worldIndexCount || distantWorldReady || entityIndexCount )
			[encoder setVertexBytes:&worldViewParams length:sizeof( worldViewParams )
				atIndex:1u];
		if ( worldIndexCount || distantWorldReady || entityIndexCount )
			[encoder setFragmentBytes:&worldAtmosphereParams
				length:sizeof( worldAtmosphereParams ) atIndex:1u];
		if ( worldIndexCount || distantWorldReady || entityIndexCount ) {
			[encoder setFragmentBuffer:atmospherePlan.computeDispatchCount
				!= 0u ? RalMetal_AtmosphereArena( present->atmosphereGpu )
				: present->atmosphereFallback offset:0u atIndex:2u];
			[encoder setFragmentBytes:s_sceneFogOff length:sizeof( s_sceneFogOff ) atIndex:3u];
			[encoder setFragmentTexture:SceneDepthOrFallback( present ) atIndex:19u];
		}
		if ( worldIndexCount || distantWorldReady || entityIndexCount ) {
			/* The portable submission preserves Q3's clockwise triangle order.
			 * Match the established Vulkan adapter exactly; viewport origin does not
			 * change the authored front-face contract. */
			[encoder setFrontFacingWinding:MTLWindingClockwise];
			[encoder setCullMode:MTLCullModeBack];
		}
		/* Preview and gameplay models consume the same complete material program.
		 * Keep the UI insertion seam: worldless models are still drawn here. */
		if ( entityIndexCount && ( sceneView.rdflags & RDF_NOWORLDMODEL )
				&& !DrawEntityBatches( encoder, present, frontend,
					uniforms,
					entityVertices, entityIndices, entityBatches, entityBatchCount,
					&sceneView, 7u, SceneDepthOrFallback( present ), s_sceneFogOff,
					&receipt ) ) {
			[encoder endEncoding];
			[worldVertices release]; [worldIndices release]; [worldDepth release];
			[entityVertices release]; [entityIndices release]; free( entityBatches );
			[uiVertices release]; [readback release];
			return PresentFailure( "preview-entity-draw" );
		}
		if ( worldIndexCount || distantWorldReady ) {
			const id<MTLBuffer> mainWorldVertices = worldVertices;
			const id<MTLBuffer> mainWorldIndices = worldIndices;
			const renderWorldBatch_t *mainWorldBatches = worldBatches;
			ralMetalWorldCacheSlot_t *mainWorldCache = worldCache;
			const uint32_t mainWorldIndexCount = worldIndexCount;
			const uint32_t mainWorldBatchCount = worldBatchCount;
			const ralMetalWorldViewParams_t mainWorldViewParams = worldViewParams;
			const renderWorldSnapshot_t mainSceneView = sceneView;
			const ralMetalWorldAtmosphereParams_t mainWorldAtmosphereParams =
				worldAtmosphereParams;
			const uint32_t worldPhaseCount = distantWorldReady ? 3u : 1u;
			for ( uint32_t worldPhase = 0u; worldPhase < worldPhaseCount;
					++worldPhase ) {
				const qboolean distantCohort = distantWorldReady && worldPhase == 1u
					? qtrue : qfalse;
				const id<MTLBuffer> worldVertices = distantCohort
					? distantWorldVertices : mainWorldVertices;
				const id<MTLBuffer> worldIndices = distantCohort
					? distantWorldIndices : mainWorldIndices;
				const renderWorldBatch_t *worldBatches = distantCohort
					? distantWorldBatches : mainWorldBatches;
				ralMetalWorldCacheSlot_t *worldCache = distantCohort
					? distantWorldCache : mainWorldCache;
				const uint32_t worldIndexCount = distantCohort
					? distantWorldIndexCount : mainWorldIndexCount;
				const uint32_t worldBatchCount = distantCohort
					? distantWorldBatchCount : mainWorldBatchCount;
				const ralMetalWorldViewParams_t worldViewParams = distantCohort
					? distantWorldViewParams : mainWorldViewParams;
				const renderWorldSnapshot_t sceneView = distantCohort
					? distantWorld.world : mainSceneView;
				const ralMetalWorldAtmosphereParams_t worldAtmosphereParams =
					distantCohort ? distantWorldAtmosphereParams
						: mainWorldAtmosphereParams;
				if ( distantWorldReady && worldPhase == 2u ) {
					/* Distant geometry contributes color behind the gameplay world,
					 * never its depth. Preserve color, discard distant depth, then
					 * restart the ordinary foreground pass from a clean depth plane. */
					[encoder endEncoding];
					MTLRenderPassDescriptor *foregroundPass =
						[MTLRenderPassDescriptor renderPassDescriptor];
					foregroundPass.colorAttachments[0].texture = offscreenScene
						? present->sceneColorCache : present->drawable.texture;
					foregroundPass.colorAttachments[0].loadAction = MTLLoadActionLoad;
					foregroundPass.colorAttachments[0].storeAction = MTLStoreActionStore;
					foregroundPass.depthAttachment.texture = worldDepth;
					foregroundPass.depthAttachment.loadAction = MTLLoadActionClear;
					foregroundPass.depthAttachment.storeAction = ( genericEffectParticleCount || taaActive
						|| lensSnapshot.count )
						? MTLStoreActionStore : MTLStoreActionDontCare;
					foregroundPass.depthAttachment.clearDepth = 1.0;
					encoder = [command renderCommandEncoderWithDescriptor:foregroundPass];
					if ( !encoder ) {
						[mainWorldVertices release]; [mainWorldIndices release];
						[worldDepth release]; [entityVertices release];
						[entityIndices release]; free( entityBatches );
						[uiVertices release]; [readback release];
						return PresentFailure( "distant-depth-reset" );
					}
					receipt.distantWorldDepthReset = qtrue;
				}
				[encoder setVertexBytes:&worldViewParams
					length:sizeof( worldViewParams ) atIndex:1u];
				[encoder setFragmentBytes:&worldAtmosphereParams
					length:sizeof( worldAtmosphereParams ) atIndex:1u];
				[encoder setFragmentBuffer:atmospherePlan.computeDispatchCount
					!= 0u ? RalMetal_AtmosphereArena( present->atmosphereGpu )
					: present->atmosphereFallback offset:0u atIndex:2u];
				[encoder setFragmentBytes:s_sceneFogOff length:sizeof( s_sceneFogOff ) atIndex:3u];
				[encoder setFragmentTexture:SceneDepthOrFallback( present ) atIndex:19u];
				/* Immutable for this world cohort. Rebind after an encoder restart,
				 * not for every BSP surface; entity encoding owns its later state. */
				[encoder setFragmentTexture:physicalSkyReadable
					? present->skyPhysical[RAL_METAL_SKY_VIEW]
					: present->uiFallbackTexture atIndex:14u];
				[encoder setFragmentTexture:present->skyRadianceConsumer.ready
					? present->skyRadiance[0]
					: present->worldLightingFallbackTexture atIndex:15u];
				[encoder setFragmentTexture:present->skyRadianceConsumer.ready
						&& present->skyRadianceConsumer.usePreviousProduct
					? present->skyRadiance[1]
					: ( present->skyRadianceConsumer.ready
						? present->skyRadiance[0]
						: present->worldLightingFallbackTexture ) atIndex:16u];
				[encoder setFragmentTexture:present->skyRadianceConsumer.ready
					? present->skyIrradiance[0]
					: present->worldLightingFallbackTexture atIndex:17u];
				[encoder setFragmentTexture:present->skyRadianceConsumer.ready
						&& present->skyRadianceConsumer.usePreviousProduct
					? present->skyIrradiance[1]
					: ( present->skyRadianceConsumer.ready
						? present->skyIrradiance[0]
						: present->worldLightingFallbackTexture ) atIndex:18u];
				[encoder setFragmentSamplerState:physicalSkyReadable
					? present->skyPhysicalSampler : present->uiClampSampler
					atIndex:14u];
				for ( uint32_t slot = 15u; slot <= 18u; ++slot )
					[encoder setFragmentSamplerState:present->skyRadianceConsumer.ready
						? present->skyRadianceSampler : present->uiClampSampler
						atIndex:slot];
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
			qboolean skyDepthRangeActive = qfalse;
			[encoder setVertexBuffer:worldVertices offset:0u atIndex:0u];
			/* Cohort-local: foreground encoder restart and sky slot writes above
			 * always precede a fresh table. Entity/FX draws cannot dirty this scope. */
			wiredMetalFragmentBindings<6u + RENDER_MATERIAL_MAX_STAGES> fragmentBindings;
			wiredMetalEncoderValue<id<MTLBuffer>> materialBufferBinding;
			wiredMetalEncoderValue<id<MTLRenderPipelineState>> pipelineState;
			wiredMetalEncoderValue<id<MTLDepthStencilState>> depthState;
			wiredMetalEncoderValue<MTLCullMode> cullState;
			for ( uint32_t batchIndex = 0u; batchIndex < worldBatchCount; ++batchIndex ) {
				const renderWorldBatch_t *batch = &worldBatches[batchIndex];
				if ( !batch->visible ) continue;
				if ( distantWorldReady ) {
					if ( worldPhase == 0u && !batch->sky ) continue;
					if ( distantCohort && !DistantWorldBatchSelected(
							&distantWorld.drawPlan, batchIndex ) ) continue;
					if ( worldPhase == 2u && batch->sky ) continue;
				}
				/* Sky portals are not ordinary lightmapped geometry.  Their authored
				 * sky/atmosphere product is composited by the dedicated background
				 * path; sampling the selected _up face with BSP UVs produces the
				 * visibly tiled Metal-only wall that this semantic prevents. */
				if ( batch->noDraw ) continue;
				if ( !worldCache || !WorldBatchVisible( &worldViewParams,
						worldCache->batchBounds + (size_t)batchIndex * 6u ) ) continue;
				const MTLCullMode cull = batch->cullMode == RENDER_CULL_NONE ? MTLCullModeNone
					: ( batch->cullMode == RENDER_CULL_FRONT ? MTLCullModeFront : MTLCullModeBack );
				if ( cullState.wired_changed( cull ) ) [encoder setCullMode:cull];
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
					!distantCohort && present->directionalLightingReceipt.ready
						? &present->directionalLightingReceipt.plan : NULL;
				if ( !UiMaterial( present, frontend, batch->baseMaterial,
						&baseTexture, &baseSampler, &baseTextured, &baseMsdf )
						|| !UiMaterial( present, frontend, batch->lightmapMaterial,
							&lightmapTexture, &lightmapSampler, &lightmapped,
							&lightmapMsdf ) || baseMsdf || lightmapMsdf ) {
					[encoder endEncoding];
					[mainWorldVertices release]; [mainWorldIndices release]; [worldDepth release];
					[uiVertices release]; [readback release];
					[entityVertices release]; [entityIndices release]; free( entityBatches );
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
				/* Material factors are an optional cooked PBR cohort.  The authored
				 * texture path must remain exact identity while the sky-radiance
				 * product is unavailable (startup, rebuild, or legacy content).  A
				 * zero-initialized base color blackens the entire world and makes
				 * otherwise-native WiredFX appear to be missing on Metal. */
				SetDefaultMaterialFactors( &params );
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
				if ( frontend->pbrEnabled && present->skyRadianceConsumer.ready
						&& materialSnapshot.factors.ready ) {
					memcpy( params.materialBaseColor,
						materialSnapshot.factors.factors.baseColor,
						sizeof( params.materialBaseColor ) );
					params.materialFactors[0] =
						materialSnapshot.factors.factors.metallic;
					params.materialFactors[1] =
						materialSnapshot.factors.factors.roughness;
					params.materialFactors[2] =
						materialSnapshot.factors.factors.occlusionStrength;
					params.materialFactors[3] = present->skyRadianceConsumer
						.previousToCurrentBlend;
				}
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
				/* The shader has already composited sky layers over their outer
				 * cube/black background. Do not apply the last layer's blend twice. */
				if ( batch->sky && materialSnapshot.stageCount )
					params.alphaMode = RENDER_ALPHA_OPAQUE;
				params.stageProgram[1] = MaterialFusedAdditive( &materialSnapshot,
					batch->alphaMode, !batch->sky );
				if ( !batch->sky && !distantCohort ) {
					const uint32_t selected = PopulateDirectLights( frontend,
						batch->boundsCenter, batch->boundsRadius, &params );
					if ( selected ) {
						receipt.directLightDrawCount++;
						receipt.directLightReferenceCount += selected;
					}
				}
				for ( uint32_t stageIndex = 0u;
						stageIndex < RENDER_MATERIAL_MAX_STAGES; ++stageIndex ) {
					const renderMaterialStageSnapshot_t *stage =
						&materialSnapshot.stages[stageIndex];
					renderMaterialStageGpuParams_t *stageParams =
						&params.stages[stageIndex];
					qboolean stageTextured = qfalse, stageMsdf = qfalse;
					stageTextures[stageIndex] = present->uiFallbackTexture;
					stageSamplers[stageIndex] = present->uiClampSampler;
					if ( stageIndex >= materialSnapshot.stageCount ) continue;
					wired_material_stage_pack( stage, stageParams );
					if ( stage->imageSource == 1u ) {
						stageTextures[stageIndex] = lightmapped
							? lightmapTexture : present->uiFallbackTexture;
						stageSamplers[stageIndex] = lightmapSampler;
					} else if ( stage->imageSource == 0u ) {
						if ( !UiMaterial( present, frontend,
								MaterialStageHandleAtTime( stage, params.skyTime ),
								&stageTextures[stageIndex], &stageSamplers[stageIndex],
								&stageTextured, &stageMsdf ) || stageMsdf ) {
							[encoder endEncoding];
							[mainWorldVertices release]; [mainWorldIndices release];
							[worldDepth release]; [uiVertices release];
							[readback release];
							[entityVertices release]; [entityIndices release]; free( entityBatches );
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
						[mainWorldVertices release]; [mainWorldIndices release];
						[worldDepth release]; [uiVertices release];
						[readback release];
						[entityVertices release]; [entityIndices release]; free( entityBatches );
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
						[mainWorldVertices release]; [mainWorldIndices release];
						[worldDepth release]; [uiVertices release];
						[readback release];
						[entityVertices release]; [entityIndices release]; free( entityBatches );
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
				id<MTLRenderPipelineState> pipeline = MaterialBlendPipeline( present,
					(renderAlphaMode_t)params.alphaMode, params.stageProgram[1] != 0u );
				id<MTLDepthStencilState> depth = MaterialDepthState( present, &materialSnapshot,
					batch->depthWrite, !batch->sky );
				if ( pipelineState.wired_changed( pipeline ) ) [encoder setRenderPipelineState:pipeline];
				if ( depthState.wired_changed( depth ) ) [encoder setDepthStencilState:depth];
				/* Same complete slot table and resource ownership; only changed
				 * tables are sent. Each cohort/encoder starts with every slot. */
				id<MTLTexture> materialTextures[6u + RENDER_MATERIAL_MAX_STAGES] = {
					baseTexture,
					params.staticLightingMode == 1u ? lightmapTexture : present->uiFallbackTexture,
					params.staticLightingMode == 2u
						? present->directionalLightingTextures[binding->radiancePlane]
						: present->worldLightingFallbackTexture,
					params.staticLightingMode == 2u
						? present->directionalLightingTextures[binding->directionPlane]
						: present->worldLightingFallbackTexture,
					params.staticLightingMode == 2u
						&& binding->visibilityPlane != RAL_LIGHTING_SURFACE_BINDING_NO_PLANE
						? present->directionalLightingTextures[binding->visibilityPlane]
						: present->worldLightingFallbackTexture,
					skySecondaryTexture
				};
				id<MTLSamplerState> materialSamplers[6u + RENDER_MATERIAL_MAX_STAGES] = {
					baseSampler, lightmapSampler, present->uiClampSampler,
					present->uiClampSampler, present->uiClampSampler, skySecondarySampler
				};
				for ( uint32_t stageIndex = 0u;
						stageIndex < RENDER_MATERIAL_MAX_STAGES; ++stageIndex ) {
					materialTextures[6u + stageIndex] = stageTextures[stageIndex];
					materialSamplers[6u + stageIndex] = stageSamplers[stageIndex];
				}
				if ( fragmentBindings.wired_textures_changed( materialTextures ) )
					[encoder setFragmentTextures:materialTextures
						withRange:NSMakeRange( 0u, 6u + RENDER_MATERIAL_MAX_STAGES )];
				if ( fragmentBindings.wired_samplers_changed( materialSamplers ) )
					[encoder setFragmentSamplerStates:materialSamplers
						withRange:NSMakeRange( 0u, 6u + RENDER_MATERIAL_MAX_STAGES )];
				if ( !uniforms.wired_bind( RalMetal_CoreNativeDevice( present->core ),
						encoder, params, materialBufferBinding ) ) {
					[encoder endEncoding];
					[mainWorldVertices release]; [mainWorldIndices release];
					[worldDepth release]; [uiVertices release]; [readback release];
					[entityVertices release]; [entityIndices release]; free( entityBatches );
					return PresentFailure( "world-material-uniforms" );
				}
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
				if ( ( batch->sky ? qtrue : qfalse ) != skyDepthRangeActive ) {
					/* Q3/Vulkan draw sky surfaces at the far depth (glDepthRange(1,1)
					 * / DEPTH_RANGE_ONE), so the scene depth under the sky keeps the
					 * cleared value: the lens oracle, soft particles and the weather
					 * quads then see "nothing behind" exactly as Vulkan's composite
					 * does (depth <= 0 -> far slice + physical-sky compose). */
					MTLViewport skyViewport = viewport;
					skyViewport.znear = batch->sky ? 1.0 : 0.0;
					skyViewport.zfar = 1.0;
					[encoder setViewport:skyViewport];
					skyDepthRangeActive = batch->sky ? qtrue : qfalse;
				}
				[encoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle
					indexCount:batch->indexCount indexType:MTLIndexTypeUInt32
					indexBuffer:worldIndices
					indexBufferOffset:(NSUInteger)batch->firstIndex * sizeof( uint32_t )];
			}
			if ( skyDepthRangeActive ) [encoder setViewport:viewport];
			(void)worldIndexCount;
			}
		}
		/* Gameplay entities must be submitted after the BSP cohort.  In
		 * particular, depth-read/no-write sprites are transparent surfaces: if
		 * they run before opaque world batches, the later BSP pass paints over
		 * every visible lens-flare texel even though the entity was accepted. */
		/* Opaque and decal entity phases only; translucent batches (phase 2)
		 * draw after the scene-depth copy so they fog at the depth behind them. */
		if ( entityIndexCount && !( sceneView.rdflags & RDF_NOWORLDMODEL )
				&& !DrawEntityBatches( encoder, present, frontend,
					uniforms,
					entityVertices, entityIndices, entityBatches, entityBatchCount,
					&sceneView, 3u, SceneDepthOrFallback( present ), s_sceneFogOff,
					&receipt ) ) {
			[encoder endEncoding];
			[worldVertices release]; [worldIndices release]; [worldDepth release];
			[entityVertices release]; [entityIndices release]; free( entityBatches );
			[uiVertices release]; [readback release];
			return PresentFailure( "entity-draw" );
		}
		if ( !weatherPlan.zeroWork || genericEffectParticleCount || translucentEntityCount ) {
			/* Weather quads and soft particles sample an immutable scene-depth
			 * snapshot: Vulkan fogs its weather quads in the post-scene
			 * atmosphere composite at the opaque depth *behind* the quad (they
			 * write no depth) and fades them against geometry
			 * (softParticleFade); WiredFX particles fade the same way.  Metal
			 * does not permit the active depth attachment to double as a
			 * sampled texture in the same render pass, so close the opaque
			 * pass, copy its depth, and resume color/depth with load actions. */
			[encoder endEncoding];
			id<MTLBlitCommandEncoder> depthBlit = [command blitCommandEncoder];
			if ( !depthBlit ) {
				[worldVertices release]; [worldIndices release]; [worldDepth release];
				[entityVertices release]; [entityIndices release]; free( entityBatches );
				[uiVertices release]; [readback release]; return qfalse;
			}
			[depthBlit copyFromTexture:worldDepth sourceSlice:0u sourceLevel:0u
				sourceOrigin:MTLOriginMake( 0u, 0u, 0u )
				sourceSize:MTLSizeMake( receipt.width, receipt.height, 1u )
				toTexture:present->sceneDepthCache destinationSlice:0u
				destinationLevel:0u destinationOrigin:MTLOriginMake( 0u, 0u, 0u )];
			[depthBlit endEncoding];
			MTLRenderPassDescriptor *effectPass =
				[MTLRenderPassDescriptor renderPassDescriptor];
			effectPass.colorAttachments[0].texture = offscreenScene
				? present->sceneColorCache : present->drawable.texture;
			effectPass.colorAttachments[0].loadAction = MTLLoadActionLoad;
			effectPass.colorAttachments[0].storeAction = MTLStoreActionStore;
			effectPass.depthAttachment.texture = worldDepth;
			effectPass.depthAttachment.loadAction = MTLLoadActionLoad;
			effectPass.depthAttachment.storeAction = MTLStoreActionDontCare;
			encoder = [command renderCommandEncoderWithDescriptor:effectPass];
			if ( !encoder ) {
				[worldVertices release]; [worldIndices release]; [worldDepth release];
				[entityVertices release]; [entityIndices release]; free( entityBatches );
				[uiVertices release]; [readback release];
				return PresentFailure( "particle-effect-encoder" );
			}
		}
		if ( translucentEntityCount ) {
			/* Translucent entity batches (sprites, lens-flare quads) over the
			 * opaque scene, fogged at the opaque depth behind them like Vulkan's
			 * post-scene composite. Rebind the world-fragment state this new
			 * encoder lacks; the batch draw binds textures/samplers 0..16 itself. */
			const float sceneFog[4] = { worldViewParams.projection[2],
				(float)receipt.width, (float)receipt.height, 1.0f };
			[encoder setVertexBytes:&worldViewParams length:sizeof( worldViewParams ) atIndex:1u];
			[encoder setFragmentBytes:&worldAtmosphereParams
				length:sizeof( worldAtmosphereParams ) atIndex:1u];
			[encoder setFragmentBuffer:atmospherePlan.computeDispatchCount
				!= 0u ? RalMetal_AtmosphereArena( present->atmosphereGpu )
				: present->atmosphereFallback offset:0u atIndex:2u];
			[encoder setFragmentTexture:present->skyRadianceConsumer.ready
				? present->skyIrradiance[0]
				: present->worldLightingFallbackTexture atIndex:17u];
			[encoder setFragmentTexture:present->skyRadianceConsumer.ready
					&& present->skyRadianceConsumer.usePreviousProduct
				? present->skyIrradiance[1]
				: ( present->skyRadianceConsumer.ready
					? present->skyIrradiance[0]
					: present->worldLightingFallbackTexture ) atIndex:18u];
			for ( uint32_t slot = 17u; slot <= 18u; ++slot )
				[encoder setFragmentSamplerState:present->skyRadianceConsumer.ready
					? present->skyRadianceSampler : present->uiClampSampler
					atIndex:slot];
			if ( !DrawEntityBatches( encoder, present, frontend, uniforms,
					entityVertices, entityIndices, entityBatches, entityBatchCount,
					&sceneView, 4u, present->sceneDepthCache, sceneFog, &receipt ) ) {
				[encoder endEncoding]; [worldVertices release]; [worldIndices release];
				[worldDepth release]; [entityVertices release]; [entityIndices release];
				free( entityBatches ); [uiVertices release]; [readback release];
				return PresentFailure( "entity-translucent-draw" );
			}
		}
		if ( !weatherPlan.zeroWork ) {
			[encoder setCullMode:MTLCullModeNone];
			const uint32_t renderedPool = present->weatherParity ? 0u : 1u;
			[encoder setRenderPipelineState:present->weatherRenderPipeline];
			[encoder setDepthStencilState:present->worldDepthReadState];
			[encoder setVertexBytes:&weatherFrame length:sizeof( weatherFrame ) atIndex:1u];
			[encoder setVertexBuffer:present->weatherPools[renderedPool]
				offset:0u atIndex:2u];
			[encoder setFragmentTextures:weatherTextures
				withRange:NSMakeRange( 0u, RENDER_SUBMISSION_MAX_ATMOSPHERE_EFFECT_WORKLOADS )];
			[encoder setFragmentSamplerStates:weatherSamplers
				withRange:NSMakeRange( 0u, RENDER_SUBMISSION_MAX_ATMOSPHERE_EFFECT_WORKLOADS )];
			/* Weather quads take the atmospheric media at the scene depth
			 * behind them, the way Vulkan's post-scene atmosphere composite
			 * fogs the already-blended quad, and fade against that depth. */
			[encoder setFragmentBytes:&worldAtmosphereParams
				length:sizeof( worldAtmosphereParams ) atIndex:1u];
			[encoder setFragmentBuffer:atmospherePlan.computeDispatchCount
				!= 0u ? RalMetal_AtmosphereArena( present->atmosphereGpu )
				: present->atmosphereFallback offset:0u atIndex:2u];
			const float weatherFogParams[4] = { worldViewParams.projection[2],
				(float)receipt.width, (float)receipt.height, 0.0f };
			[encoder setFragmentBytes:weatherFogParams
				length:sizeof( weatherFogParams ) atIndex:3u];
			[encoder setFragmentTexture:present->sceneDepthCache atIndex:8u];
			/* Physical sky-view LUT: Vulkan re-composes sky pixels with it after
			 * the weather blend (atmosphere_composite.comp); the fragment
			 * applies the same compose to quads over cleared depth. */
			[encoder setFragmentTexture:physicalSkyReadable
				? present->skyPhysical[RAL_METAL_SKY_VIEW]
				: present->uiFallbackTexture atIndex:9u];
			[encoder setFragmentSamplerState:present->skyPhysicalSampler
				? present->skyPhysicalSampler : present->uiClampSampler atIndex:8u];
			[encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0u
				vertexCount:6u instanceCount:weatherPlan.activeParticleCount];
			receipt.weatherDrawCount = 1u;
		}
		if ( genericEffectParticleCount ) {
			id<MTLDevice> device = RalMetal_CoreNativeDevice( present->core );
			const bool argumentsReady = particleUploads.vertexArguments.wired_reserve(
				device, present->particleVertexArguments.encodedLength, MTLResourceStorageModeShared )
				&& particleUploads.fragmentArguments.wired_reserve(
					device, present->particleFragmentArguments.encodedLength, MTLResourceStorageModeShared );
			id<MTLBuffer> vertexArguments = argumentsReady
				? [particleUploads.vertexArguments.buffer retain] : nil;
			id<MTLBuffer> fragmentArguments = argumentsReady
				? [particleUploads.fragmentArguments.buffer retain] : nil;
			vertexArguments.label = @"Wired particle vertex arguments";
			fragmentArguments.label = @"Wired particle fragment arguments";
			/* Integration and spawning target the write side of the ping-pong
			 * pair. Draw that newly produced side in this command buffer; the
			 * scheduling parity flips only after native submission. */
			uint32_t renderedPool = effectParticleRenderedPool;
			const ralParticleClassGpu_t *packedClasses =
				(const ralParticleClassGpu_t *)[effectParticleClassBuffer contents];
			if ( !vertexArguments || !fragmentArguments || !effectParticleFrameBuffer
					|| !effectParticleClassBuffer || renderedPool > 1u ) {
				[vertexArguments release]; [fragmentArguments release];
				[encoder endEncoding]; [worldVertices release]; [worldIndices release];
				[worldDepth release]; [entityVertices release]; [entityIndices release];
				free( entityBatches ); [uiVertices release]; [readback release]; return qfalse;
			}
			[present->particleVertexArguments setArgumentBuffer:vertexArguments offset:0u];
			[present->particleVertexArguments setBuffer:effectParticleFrameBuffer
				offset:0u atIndex:0u];
			[present->particleVertexArguments setBuffer:present->effectPools[renderedPool]
				offset:0u atIndex:1u];
			[present->particleVertexArguments setBuffer:effectParticleClassBuffer
				offset:0u atIndex:2u];
			[present->particleFragmentArguments setArgumentBuffer:fragmentArguments offset:0u];
			[present->particleFragmentArguments setBuffer:effectParticleFrameBuffer
				offset:0u atIndex:0u];
			[present->particleFragmentArguments setTexture:present->sceneDepthCache
				atIndex:1u];
			[present->particleFragmentArguments setSamplerState:present->uiClampSampler atIndex:2u];
			/* Atmosphere media at the scene depth behind each particle (binding 5/6
			 * of particle.frag): Vulkan fogs its particles in the post-scene
			 * composite; here the effect pass runs over the fogged scene. */
			ralMetalParticleAtmosphere_t particleAtmosphere;
			memset( &particleAtmosphere, 0, sizeof( particleAtmosphere ) );
			memcpy( particleAtmosphere.eyeDensity, worldAtmosphereParams.eyeDensity,
				sizeof( particleAtmosphere.eyeDensity ) );
			memcpy( particleAtmosphere.colorVisibility, worldAtmosphereParams.colorVisibility,
				sizeof( particleAtmosphere.colorVisibility ) );
			memcpy( particleAtmosphere.heightCloud, worldAtmosphereParams.heightCloud,
				sizeof( particleAtmosphere.heightCloud ) );
			memcpy( particleAtmosphere.froxelGrid, worldAtmosphereParams.froxelGrid,
				sizeof( particleAtmosphere.froxelGrid ) );
			particleAtmosphere.control[0] = worldViewParams.projection[2];
			particleAtmosphere.control[1] = -1.0f;
			id<MTLBuffer> particleAtmosphereBuffer = [[device
				newBufferWithBytes:&particleAtmosphere length:sizeof( particleAtmosphere )
				options:MTLResourceStorageModeShared] autorelease];
			id<MTLBuffer> particleMediaBuffer = atmospherePlan.computeDispatchCount != 0u
				? RalMetal_AtmosphereArena( present->atmosphereGpu ) : present->atmosphereFallback;
			if ( !particleMediaBuffer )
				particleMediaBuffer = [[device newBufferWithLength:16u
					options:MTLResourceStorageModeShared] autorelease];
			if ( !particleAtmosphereBuffer || !particleMediaBuffer ) {
				[vertexArguments release]; [fragmentArguments release];
				[encoder endEncoding]; [worldVertices release]; [worldIndices release];
				[worldDepth release]; [entityVertices release]; [entityIndices release];
				free( entityBatches ); [uiVertices release]; [readback release];
				return PresentFailure( "particle-atmosphere" );
			}
			[present->particleFragmentArguments setBuffer:particleAtmosphereBuffer
				offset:0u atIndex:RAL_METAL_PARTICLE_ATMOSPHERE_ID];
			[present->particleFragmentArguments setBuffer:particleMediaBuffer
				offset:0u atIndex:RAL_METAL_PARTICLE_MEDIA_ID];
			[encoder useResource:particleAtmosphereBuffer usage:MTLResourceUsageRead
				stages:MTLRenderStageFragment];
			[encoder useResource:particleMediaBuffer usage:MTLResourceUsageRead
				stages:MTLRenderStageFragment];
			/* Class texture/sampler ids follow the translated argument buffer
			 * (particle_frag_spv.msl): textures [[id(5)]], samplers [[id(101)]]. */
			for ( uint32_t slot = 0u; slot < PARTICLE_SAMPLER_COUNT; ++slot ) {
				[present->particleFragmentArguments setTexture:present->uiFallbackTexture
					atIndex:5u + slot];
				[present->particleFragmentArguments setSamplerState:present->uiClampSampler
					atIndex:101u + slot];
			}
			for ( uint32_t slot = 0u; slot < MAX_PARTICLE_CLASSES; ++slot ) {
				particleClass_t particleClass;
				id<MTLTexture> texture; id<MTLSamplerState> sampler;
				qboolean textured, msdf;
				if ( !RenderSubmission_GetParticleClass( frontend,
						(particleClassHandle_t)( slot + 1u ), &particleClass ) ) continue;
				if ( !UiMaterial( present, frontend, particleClass.shader,
						&texture, &sampler, &textured, &msdf ) || !textured || msdf ) {
					[vertexArguments release]; [fragmentArguments release];
					[encoder endEncoding]; [worldVertices release]; [worldIndices release];
					[worldDepth release]; [entityVertices release]; [entityIndices release];
					free( entityBatches ); [uiVertices release]; [readback release]; return qfalse;
				}
				[present->particleFragmentArguments setTexture:texture atIndex:5u + slot];
				[present->particleFragmentArguments setSamplerState:sampler atIndex:101u + slot];
				[encoder useResource:texture usage:MTLResourceUsageRead
					stages:MTLRenderStageFragment];
				if ( packedClasses[slot].frameCount > 1u ) {
					for ( uint32_t frameIndex = 0u;
							frameIndex < packedClasses[slot].frameCount; ++frameIndex ) {
						uint32_t samplerSlot = packedClasses[slot].frameSlots[frameIndex];
						if ( samplerSlot < MAX_PARTICLE_CLASSES
								|| samplerSlot >= PARTICLE_SAMPLER_COUNT
								|| !UiMaterial( present, frontend,
									particleClass.frameShaders[frameIndex], &texture, &sampler,
									&textured, &msdf ) || !textured || msdf ) {
							/* Never leave a fallback texture in an authored frame slot. */
							[vertexArguments release]; [fragmentArguments release];
							[encoder endEncoding]; [worldVertices release]; [worldIndices release];
							[worldDepth release]; [entityVertices release]; [entityIndices release];
							free( entityBatches ); [uiVertices release]; [readback release];
							return PresentFailure( "particle-frame-material" );
						}
						[present->particleFragmentArguments setTexture:texture
							atIndex:5u + samplerSlot];
						[present->particleFragmentArguments setSamplerState:sampler
							atIndex:101u + samplerSlot];
						[encoder useResource:texture usage:MTLResourceUsageRead
							stages:MTLRenderStageFragment];
					}
				}
			}
			[encoder setCullMode:MTLCullModeNone];
			[encoder setDepthStencilState:present->worldDepthReadState];
			[encoder setVertexBuffer:vertexArguments offset:0u atIndex:0u];
			[encoder setFragmentBuffer:fragmentArguments offset:0u atIndex:0u];
			[encoder useResource:present->effectPools[renderedPool]
				usage:MTLResourceUsageRead stages:MTLRenderStageVertex];
			[encoder useResource:effectParticleClassBuffer
				usage:MTLResourceUsageRead
				stages:MTLRenderStageVertex | MTLRenderStageFragment];
			[encoder useResource:effectParticleFrameBuffer usage:MTLResourceUsageRead
				stages:MTLRenderStageVertex | MTLRenderStageFragment];
			[encoder useResource:present->sceneDepthCache usage:MTLResourceUsageRead
				stages:MTLRenderStageFragment];
			[encoder useResource:present->uiFallbackTexture usage:MTLResourceUsageRead
				stages:MTLRenderStageFragment];
			[encoder setRenderPipelineState:present->particleAlphaPipeline];
			[encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0u
				vertexCount:6u instanceCount:RAL_PARTICLE_POOL_CAPACITY];
			[encoder setRenderPipelineState:present->particleAdditivePipeline];
			[encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0u
				vertexCount:6u instanceCount:RAL_PARTICLE_POOL_CAPACITY];
			[vertexArguments release]; [fragmentArguments release];
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
			id<MTLBuffer> adaptedExposure = nil;
			if ( taaActive ) {
				if ( !present->taa ) present->taa = RalMetal_TaaCreate(
					RalMetal_CoreNativeDevice( present->core ), present->uiLibrary );
				ralMetalTaaFrame_t taaFrame;
				memset( &taaFrame, 0, sizeof( taaFrame ) );
				taaFrame.width = receipt.width; taaFrame.height = receipt.height;
				memcpy( taaFrame.currentView, &worldViewParams, sizeof( taaFrame.currentView ) );
				memcpy( taaFrame.previousView, present->taaPreviousView, sizeof( taaFrame.previousView ) );
				/* The view parameters carry the jitter in two .w slots; the motion kernel gets the unjittered camera. */
				taaFrame.currentView[7] = 0.0f; taaFrame.currentView[11] = 0.0f;
				taaFrame.jitterNdc[0] = taaJitterNdc[0]; taaFrame.jitterNdc[1] = taaJitterNdc[1];
				taaFrame.currentJitterUv[0] = taaPlan.sceneJitterPixels[0] / (float)receipt.width;
				taaFrame.currentJitterUv[1] = taaPlan.sceneJitterPixels[1] / (float)receipt.height;
				taaFrame.previousJitterUv[0] = taaPlan.previousJitterPixels[0] / (float)receipt.width;
				taaFrame.previousJitterUv[1] = taaPlan.previousJitterPixels[1] / (float)receipt.height;
				taaFrame.historyValid = taaPlan.historyValid; taaFrame.readIndex = taaPlan.historyReadIndex;
				taaFrame.writeIndex = taaPlan.historyWriteIndex;
				id<MTLTexture> effectMask = RalMetal_TaaEffectMask( present->taa, present->sceneColorCache );
				/* object motion: the previous world positions and the unjittered current + previous views */
				float motionViews[40];
				memcpy( motionViews, taaFrame.currentView, sizeof( taaFrame.currentView ) );
				memcpy( motionViews + 20, taaFrame.previousView, sizeof( taaFrame.previousView ) );
				id<MTLBuffer> motionPrevious = taaEntityMotion && taaFrame.historyValid && entityVertexCount
					? [RalMetal_CoreNativeDevice( present->core ) newBufferWithBytes:present->entityMotion.previous
						length:(NSUInteger)entityVertexCount * sizeof( present->entityMotion.previous[0] )
						options:MTLResourceStorageModeShared] : nil;
				/* No motion to draw: those batches fall back to the opaque mask (invalidated), never to an unmarked trust of 1. */
				if ( !motionPrevious ) for ( uint32_t b = 0u; b < entityBatchCount; ++b ) entityBatches[b].motion = qfalse;
				const qboolean maskEncoded = effectMask && EncodeTaaEffectMask( command, present, frontend, uniforms, effectMask,
						worldDepth, entityVertices, entityIndices, entityBatches, entityBatchCount, &sceneView, &worldViewParams,
						&worldAtmosphereParams, atmospherePlan.computeDispatchCount != 0u
							? RalMetal_AtmosphereArena( present->atmosphereGpu ) : present->atmosphereFallback, &receipt,
						motionPrevious, motionPrevious ? motionViews : NULL ) ? qtrue : qfalse;
				[motionPrevious release];
				if ( !maskEncoded ) {
					[worldVertices release]; [worldIndices release]; [worldDepth release];
					[entityVertices release]; [entityIndices release]; free( entityBatches );
					[uiVertices release]; [readback release];
					return PresentFailure( "taa-effect-mask" );
				}
				if ( !RalMetal_TaaEncode( present->taa, command, present->sceneColorCache, worldDepth, &taaFrame ) ) {
					[worldVertices release]; [worldIndices release]; [worldDepth release];
					[entityVertices release]; [entityIndices release]; free( entityBatches );
					[uiVertices release]; [readback release];
					return PresentFailure( "taa" );
				}
				memcpy( present->taaPreviousView, taaFrame.currentView, sizeof( present->taaPreviousView ) );
				present->taaFrameId = taaPlan.frameId;
				if ( taaFrame.historyValid ) present->taaResolvedFrames++; else present->taaBootstrapFrames++;
				(void)Ral_TemporalCommitFrame( &present->taaState, taaPlan.frameId );
				EntityMotionFinish( present, qtrue );
			}
			if ( present->bloomSettings.enabled ) {
				if ( !present->bloom ) present->bloom =
					RalMetal_BloomCreate( RalMetal_CoreNativeDevice( present->core ) );
				if ( !RalMetal_BloomEncode( present->bloom, command,
						present->sceneColorCache, &present->bloomSettings ) ) {
					[worldVertices release]; [worldIndices release]; [worldDepth release];
					[entityVertices release]; [entityIndices release]; free( entityBatches );
					[uiVertices release]; [readback release];
					return PresentFailure( "bloom" );
				}
			}
			if ( present->smaaQuality > 0 ) {
				if ( !present->smaa ) present->smaa =
					RalMetal_SmaaCreate( RalMetal_CoreNativeDevice( present->core ) );
				if ( !RalMetal_SmaaEncode( present->smaa, command, present->sceneColorCache, present->smaaQuality ) ) {
					[worldVertices release]; [worldIndices release]; [worldDepth release];
					[entityVertices release]; [entityIndices release]; free( entityBatches );
					[uiVertices release]; [readback release];
					return PresentFailure( "smaa" );
				}
			}
			if ( present->exposureSettings.enabled ) {
				if ( !present->exposure ) present->exposure =
					RalMetal_ExposureCreate( RalMetal_CoreNativeDevice( present->core ) );
				/* Keep a cut invalid even if this command is later discarded. */
				if ( sceneView.rdflags & ( RDF_CAMERA_CUT | RDF_HYPERSPACE ) )
					present->exposureHistoryValid = qfalse;
				const ralExposureHistoryStep_t step = Ral_ExposureHistoryStep(
					present->exposureHistoryValid, present->exposureTimeMs, sceneView.timeMs,
					present->exposureWorldDigest != frontend->worldDigest
					|| present->exposureWidth != receipt.width
					|| present->exposureHeight != receipt.height );
				if ( !RalMetal_ExposureEncode( present->exposure, command,
						present->sceneColorCache, &present->exposureSettings,
						step.deltaSeconds, step.reset != 0, &adaptedExposure ) ) {
					[worldVertices release]; [worldIndices release]; [worldDepth release];
					[entityVertices release]; [entityIndices release]; free( entityBatches );
					[uiVertices release]; [readback release];
					return PresentFailure( "exposure" );
				}
				if ( adaptedExposure ) {
					/* Publish the completed accumulator for next frame's
					 * flare-F1 cancel (particles) without a GPU stall. */
					id<MTLBuffer> adaptedRef = [adaptedExposure retain];
					ralMetalPresent_t *owner = present;
					[command addCompletedHandler:^( id<MTLCommandBuffer> finished ) {
						if ( finished.status == MTLCommandBufferStatusCompleted ) {
							const float bias = ( (const float *)[adaptedRef contents] )[0];
							if ( isfinite( bias ) && bias > 0.0f )
								owner->adaptedExposureBias = bias;
						}
						[adaptedRef release];
					}];
				}
			}
			if ( lateAcquire ) {
				/* Allocate the tail before any work becomes irrevocable. Neither
				 * buffer owns a drawable yet. Same-queue commit order supplies the
				 * scene/exposure -> output dependency without an extra fence. */
				id<MTLCommandBuffer> output = [queue commandBuffer];
				if ( !output ) {
					[adaptedExposure release];
					[worldVertices release]; [worldIndices release]; [worldDepth release];
					[entityVertices release]; [entityIndices release]; free( entityBatches );
					[uiVertices release]; [readback release]; return PresentFailure( "output-command" );
				}
				split.scene = command;
				if ( readback ) firstCommitTime = CACurrentMediaTime();
				[command commit];
				wired_publish_simulation();
				command = output;
			}
			if ( !AcquireForOutput( present, coreReceipt, layerReceipt, lateAcquire, &receipt ) ) {
				[adaptedExposure release];
				[worldVertices release]; [worldIndices release]; [worldDepth release];
				[entityVertices release]; [entityIndices release]; free( entityBatches );
				[uiVertices release]; [readback release]; return qfalse;
			}
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
				[adaptedExposure release];
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
			if ( adaptedExposure )
				[outputEncoder setFragmentBuffer:adaptedExposure offset:0u atIndex:1u];
			else {
				const float identityExposure = 1.0f;
				[outputEncoder setFragmentBytes:&identityExposure
					length:sizeof( identityExposure ) atIndex:1u];
			}
			[adaptedExposure release];
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
		qboolean lensVisibilityDispatched = qfalse;
		/* The particle pass discards its read-only depth attachment. Its
		 * pre-pass snapshot already contains the same opaque depth and remains
		 * valid for soft particles and lens occlusion, without another store/copy. */
		id<MTLTexture> lensDepth = genericEffectParticleCount
			? present->sceneDepthCache : worldDepth;
		if ( lensSnapshot.count && frontend
				&& !EncodeLensVisibility( present, command, lensDepth, frontend,
					&worldViewParams, receipt.width, receipt.height,
					atmospherePlan.skyEnvironment.opticsVisibility,
					receipt.frontendFrameGeneration,
					&lensVisibilityDispatched ) ) {
			[worldVertices release]; [worldIndices release]; [worldDepth release];
			[entityVertices release]; [entityIndices release]; free( entityBatches );
			[uiVertices release]; [captureReadback release]; [readback release];
			return PresentFailure( "lens-visibility" );
		}
		if ( lensVisibilityDispatched ) receipt.lensVisibilityDispatchCount = 1u;
		if ( Ral_CommandLifecyclePublishEnd( &lifecycle, &recording,
				&executable ) != ralSuccess ) {
			DiscardLensCommand( present, command );
			[worldVertices release]; [worldIndices release]; [worldDepth release];
			[entityVertices release]; [entityIndices release]; free( entityBatches );
			[uiVertices release]; [captureReadback release]; [readback release];
			return PresentFailure( "command-publish" );
		}
		if ( lateAcquire && lateAcquire->measure )
			lateAcquire->submitNs = clock_gettime_nsec_np( CLOCK_UPTIME_RAW );
		[command presentDrawable:present->drawable];
		if ( readback ) {
			present->readbackCommands[readbackSlot] = [command retain];
			present->readbackGenerations[readbackSlot] = presentGeneration;
			present->readbackByteCounts[readbackSlot] = readbackBytes;
			present->readbackSceneCommands[readbackSlot] = [split.scene retain];
			present->readbackCommitTimes[readbackSlot] = split.scene
				? firstCommitTime : CACurrentMediaTime();
			present->readbackAcquireBeginTimes[readbackSlot] = lateAcquire
				? lateAcquire->acquireBeginSeconds : 0.0;
			present->readbackAcquireEndTimes[readbackSlot] = lateAcquire
				? lateAcquire->acquireEndSeconds : 0.0;
		}
		[command commit];
		split.outputCommitted = true;
		wired_publish_simulation();
		if ( lateAcquire && lateAcquire->measure )
			lateAcquire->committedNs = clock_gettime_nsec_np( CLOCK_UPTIME_RAW );
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
		/* Both material draw paths precede the scene/output split. The tail is
		 * tonemap/UI only: do not retain its drawable just to guard scene input. */
		uniforms.wired_publish_cache( split.scene ? split.scene : command );
		entityVertexUpload.wired_publish( split.scene ? split.scene : command );
		entityIndexUpload.wired_publish( split.scene ? split.scene : command );
		particleUploads.wired_publish( split.scene ? split.scene : command );
		atmosphereUpload.wired_publish( split.scene ? split.scene : command );
		/* UI belongs to the output command, not the earlier offscreen scene. */
		uiVertexUpload.wired_publish( command );
	}
	receipt.recording = recording; receipt.executable = executable;
	receipt.completionGeneration = receipt.submission.generation;
	receipt.clearDigest = DigestClear( clearColor ); receipt.presented = qtrue;
	receipt.readbackDigest = DigestBytes( receipt.readbackBytes,
		receipt.readbackByteCount );
	[present->drawable release]; present->drawable = nil;
	memset( &present->drawableReceipt, 0, sizeof( present->drawableReceipt ) );
	if ( !PresentReceiptValid( &receipt ) ) return PresentFailure( "receipt" );
	*outReceipt = receipt;
	return qtrue;
}

qboolean RalMetal_PresentClearAndSubmit( ralMetalPresent_t *present,
		const ralMetalCoreReceipt_t *coreReceipt,
		const ralMetalPresentLayerReceipt_t *layerReceipt,
		const ralMetalDrawableReceipt_t *drawableReceipt,
		renderSubmissionState_t *frontend, const float clearColor[4],
		uint64_t presentGeneration, ralMetalPresentReceipt_t *outReceipt ) {
	return RenderAndSubmit( present, coreReceipt, layerReceipt, drawableReceipt,
		NULL, frontend, clearColor, presentGeneration, outReceipt );
}

ralMetalSubmitResult_t RalMetal_PresentRenderAndSubmit( ralMetalPresent_t *present,
		const ralMetalCoreReceipt_t *coreReceipt,
		const ralMetalPresentLayerReceipt_t *layerReceipt,
		renderSubmissionState_t *frontend, const float clearColor[4],
		uint64_t acquireGeneration, uint64_t presentGeneration,
		ralMetalDrawableReceipt_t *outDrawable, ralMetalPresentReceipt_t *outReceipt,
		ralMetalSubmitTiming_t *outTiming ) {
	ralMetalLateAcquire_t late = {};
	if ( !outDrawable || !outReceipt || !present || present->drawable )
		return RAL_METAL_SUBMIT_FAILED;
	late.generation = acquireGeneration;
	late.measure = outTiming ? qtrue : qfalse;
	if ( late.measure ) late.beginNs = clock_gettime_nsec_np( CLOCK_UPTIME_RAW );
	if ( !RenderAndSubmit( present, coreReceipt, layerReceipt, NULL, &late,
			frontend, clearColor, presentGeneration, outReceipt ) ) {
		/* This API owns acquire through cancel/submit. Unlike the explicit-acquire
		 * API, it never leaves an acquired drawable behind on a failed frame. */
		[present->drawable release]; present->drawable = nil;
		memset( &present->drawableReceipt, 0, sizeof( present->drawableReceipt ) );
		return late.unavailable ? RAL_METAL_SUBMIT_DRAWABLE_UNAVAILABLE : RAL_METAL_SUBMIT_FAILED;
	}
	*outDrawable = late.acquired;
	if ( outTiming ) {
		const uint64_t endNs = clock_gettime_nsec_np( CLOCK_UPTIME_RAW );
		/* One thread, one monotonic clock, no callback/shared diagnostic writes.
		 * Keep nanoseconds so the six disjoint spans sum exactly to total. */
		*outTiming = { late.encodeNs - late.beginNs,
			late.acquireBeginNs - late.encodeNs,
			late.acquireEndNs - late.acquireBeginNs,
			late.submitNs - late.acquireEndNs,
			late.committedNs - late.submitNs,
			endNs - late.committedNs, endNs - late.beginNs };
	}
	return RAL_METAL_SUBMIT_PRESENTED;
}
