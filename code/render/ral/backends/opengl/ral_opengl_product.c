// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "ral_opengl_product.h"
#include "ral_opengl_internal.h"
#include "ral_opengl_aa.h"
#include "ral_temporal.h"
#include "ral_sky_radiance.h"
#include "../../../frontend/render_material_stage_gpu.h"
#include "../../../frontend/render_atmosphere_gpu.h"
#include "../../../frontend/render_temporal_view.h"

#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ral_opengl_product_shader_catalog.inc"

#if defined( _WIN32 )
#define RAL_GL_APIENTRY __stdcall
#else
#define RAL_GL_APIENTRY
#endif

typedef unsigned int glName_t;
typedef unsigned int glEnum_t;
typedef int			 glInt_t;
typedef int			 glSize_t;
typedef long		 glSizePtr_t;
typedef long		 glIntPtr_t;
typedef char		 glChar_t;
typedef unsigned char glBool_t;
typedef unsigned long long glUInt64_t;
typedef void *glSync_t;
typedef glName_t( RAL_GL_APIENTRY *createShaderFn )( glEnum_t );
typedef void( RAL_GL_APIENTRY *shaderSourceFn )( glName_t, glSize_t, const glChar_t *const *, const glInt_t * );
typedef void( RAL_GL_APIENTRY *compileShaderFn )( glName_t );
typedef void( RAL_GL_APIENTRY *getShaderivFn )( glName_t, glEnum_t, glInt_t * );
typedef void( RAL_GL_APIENTRY *deleteShaderFn )( glName_t );
typedef glName_t( RAL_GL_APIENTRY *createProgramFn )( void );
typedef void( RAL_GL_APIENTRY *attachShaderFn )( glName_t, glName_t );
typedef void( RAL_GL_APIENTRY *linkProgramFn )( glName_t );
typedef void( RAL_GL_APIENTRY *getProgramivFn )( glName_t, glEnum_t, glInt_t * );
typedef void( RAL_GL_APIENTRY *deleteProgramFn )( glName_t );
typedef void( RAL_GL_APIENTRY *useProgramFn )( glName_t );
typedef void( RAL_GL_APIENTRY *createBuffersFn )( glSize_t, glName_t * );
typedef void( RAL_GL_APIENTRY *namedBufferStorageFn )( glName_t, glSizePtr_t, const void *, unsigned int );
typedef void( RAL_GL_APIENTRY *namedBufferSubDataFn )( glName_t, glIntPtr_t, glSizePtr_t, const void * );
typedef void( RAL_GL_APIENTRY *getNamedBufferSubDataFn )( glName_t, glIntPtr_t, glSizePtr_t, void * );
typedef void( RAL_GL_APIENTRY *copyTextureSubImage2DFn )( glName_t, glInt_t, glInt_t,
	glInt_t, glInt_t, glInt_t, glSize_t, glSize_t );
typedef void( RAL_GL_APIENTRY *bindBufferBaseFn )( glEnum_t, unsigned int, glName_t );
typedef void( RAL_GL_APIENTRY *deleteBuffersFn )( glSize_t, const glName_t * );
typedef void( RAL_GL_APIENTRY *createTexturesFn )( glEnum_t, glSize_t, glName_t * );
typedef void( RAL_GL_APIENTRY *textureStorage2DFn )( glName_t, glSize_t, glEnum_t,
	glSize_t, glSize_t );
typedef void( RAL_GL_APIENTRY *textureStorage3DFn )( glName_t, glSize_t, glEnum_t,
	glSize_t, glSize_t, glSize_t );
typedef void( RAL_GL_APIENTRY *textureSubImage2DFn )( glName_t, glInt_t, glInt_t,
	glInt_t, glSize_t, glSize_t, glEnum_t, glEnum_t, const void * );
typedef void( RAL_GL_APIENTRY *generateTextureMipmapFn )( glName_t );
typedef void( RAL_GL_APIENTRY *textureParameteriFn )( glName_t, glEnum_t, glInt_t );
typedef void( RAL_GL_APIENTRY *deleteTexturesFn )( glSize_t, const glName_t * );
typedef void( RAL_GL_APIENTRY *bindTextureUnitFn )( unsigned int, glName_t );
typedef void( RAL_GL_APIENTRY *bindImageTextureFn )( unsigned int, glName_t,
	glInt_t, glBool_t, glInt_t, glEnum_t, glEnum_t );
typedef void( RAL_GL_APIENTRY *dispatchComputeFn )( unsigned int, unsigned int, unsigned int );
typedef void( RAL_GL_APIENTRY *memoryBarrierFn )( unsigned int );
typedef glSync_t( RAL_GL_APIENTRY *fenceSyncFn )( glEnum_t, unsigned int );
typedef glEnum_t( RAL_GL_APIENTRY *clientWaitSyncFn )( glSync_t, unsigned int,
	glUInt64_t );
typedef void( RAL_GL_APIENTRY *deleteSyncFn )( glSync_t );
typedef void( RAL_GL_APIENTRY *genQueriesFn )( glSize_t, glName_t * );
typedef void( RAL_GL_APIENTRY *deleteQueriesFn )( glSize_t, const glName_t * );
typedef void( RAL_GL_APIENTRY *beginQueryFn )( glEnum_t, glName_t );
typedef void( RAL_GL_APIENTRY *endQueryFn )( glEnum_t );
typedef void( RAL_GL_APIENTRY *getQueryObjectui64vFn )( glName_t, glEnum_t,
	glUInt64_t * );
typedef void( RAL_GL_APIENTRY *pixelStoreiFn )( glEnum_t, glInt_t );
typedef void( RAL_GL_APIENTRY *readPixelsFn )( glInt_t, glInt_t, glSize_t, glSize_t, glEnum_t, glEnum_t, void * );
typedef glEnum_t( RAL_GL_APIENTRY *getErrorFn )( void );
typedef void( RAL_GL_APIENTRY *getIntegervFn )( glEnum_t, glInt_t * );
typedef glBool_t( RAL_GL_APIENTRY *isEnabledFn )( glEnum_t );
typedef void( RAL_GL_APIENTRY *getFramebufferAttachmentParameterivFn )( glEnum_t, glEnum_t, glEnum_t, glInt_t * );
typedef void( RAL_GL_APIENTRY *bindFramebufferFn )( glEnum_t, glName_t );
typedef void( RAL_GL_APIENTRY *namedFramebufferTextureFn )( glName_t, glEnum_t, glName_t, glInt_t );
typedef glEnum_t( RAL_GL_APIENTRY *checkNamedFramebufferStatusFn )( glName_t, glEnum_t );
typedef void( RAL_GL_APIENTRY *copyNamedBufferSubDataFn )( glName_t, glName_t, glIntPtr_t, glIntPtr_t, glSizePtr_t );
typedef void( RAL_GL_APIENTRY *clearNamedBufferDataFn )( glName_t, glEnum_t, glEnum_t, glEnum_t, const void * );
typedef void( RAL_GL_APIENTRY *bindVertexArrayFn )( glName_t );
typedef void( RAL_GL_APIENTRY *disableFn )( glEnum_t );
typedef void( RAL_GL_APIENTRY *depthMaskFn )( glBool_t );
typedef void( RAL_GL_APIENTRY *drawArraysFn )( glEnum_t, glInt_t, glSize_t );
typedef void( RAL_GL_APIENTRY *framebufferLayerFn )( glName_t, glEnum_t, glName_t, glInt_t, glInt_t );
typedef void( RAL_GL_APIENTRY *vertexBufferFn )( glName_t, unsigned int, glName_t, glIntPtr_t, glSize_t );
typedef void( RAL_GL_APIENTRY *vertexAttribFn )( glName_t, unsigned int );
typedef void( RAL_GL_APIENTRY *vertexFormatFn )( glName_t, unsigned int, glInt_t, glEnum_t, glBool_t, unsigned int );
typedef void( RAL_GL_APIENTRY *vertexBindingFn )( glName_t, unsigned int, unsigned int );
typedef void( RAL_GL_APIENTRY *viewportFn )( glInt_t, glInt_t, glSize_t, glSize_t );

enum {
	GL_NO_ERROR_VALUE					= 0,
	GL_FRAMEBUFFER_VALUE = 0x8d40,
	GL_READ_FRAMEBUFFER_VALUE = 0x8ca8,
	GL_DRAW_FRAMEBUFFER_VALUE = 0x8ca9,
	GL_READ_FRAMEBUFFER_BINDING_VALUE = 0x8caa,
	GL_DRAW_FRAMEBUFFER_BINDING_VALUE = 0x8ca6,
	GL_FRAMEBUFFER_COMPLETE_VALUE = 0x8cd5,
	GL_FRAMEBUFFER_SRGB_VALUE = 0x8db9,
	GL_FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING_VALUE = 0x8210,
	GL_SRGB_VALUE = 0x8c40,
	GL_BACK_LEFT_VALUE = 0x0402,
	GL_COLOR_ATTACHMENT0_VALUE = 0x8ce0,
	GL_DEPTH_ATTACHMENT_VALUE = 0x8d00,
	GL_DEPTH_COMPONENT32F_VALUE = 0x8cac,
	GL_BUFFER_UPDATE_BARRIER_BIT_VALUE = 0x200,
	GL_R32UI_VALUE = 0x8236,
	GL_RED_INTEGER_VALUE = 0x8d94,
	GL_UNSIGNED_INT_VALUE = 0x1405,
	GL_DEPTH_TEST_VALUE = 0x0b71,
	GL_BLEND_VALUE = 0x0be2,
	GL_CULL_FACE_VALUE = 0x0b44,
	GL_TRIANGLE_STRIP_VALUE = 5,
	GL_VERTEX_SHADER_VALUE				= 0x8b31,
	GL_FRAGMENT_SHADER_VALUE			= 0x8b30,
	GL_COMPUTE_SHADER_VALUE				= 0x91b9,
	GL_COMPILE_STATUS_VALUE				= 0x8b81,
	GL_LINK_STATUS_VALUE				= 0x8b82,
	GL_UNIFORM_BUFFER_VALUE				= 0x8a11,
	GL_SHADER_STORAGE_BUFFER_VALUE		= 0x90d2,
	GL_DYNAMIC_STORAGE_BIT_VALUE		= 0x0100,
	GL_UNIFORM_BARRIER_BIT_VALUE		= 0x00000004,
	GL_SHADER_STORAGE_BARRIER_BIT_VALUE = 0x00002000,
	GL_TEXTURE_FETCH_BARRIER_BIT_VALUE	= 0x00000008,
	GL_SHADER_IMAGE_ACCESS_BARRIER_BIT_VALUE = 0x00000020,
	GL_PACK_ALIGNMENT_VALUE				= 0x0d05,
	GL_TEXTURE_2D_VALUE					= 0x0de1,
	GL_TEXTURE_2D_ARRAY_VALUE			= 0x8c1a,
	GL_RGBA_VALUE						= 0x1908,
	GL_RGBA8_VALUE						= 0x8058,
	GL_RGBA16F_VALUE					= 0x881a,
	GL_SRGB8_ALPHA8_VALUE				= 0x8c43,
	GL_TEXTURE_MIN_FILTER_VALUE			= 0x2801,
	GL_TEXTURE_MAG_FILTER_VALUE			= 0x2800,
	GL_TEXTURE_WRAP_S_VALUE				= 0x2802,
	GL_TEXTURE_WRAP_T_VALUE				= 0x2803,
	GL_LINEAR_VALUE						= 0x2601,
	GL_LINEAR_MIPMAP_LINEAR_VALUE		= 0x2703,
	GL_REPEAT_VALUE						= 0x2901,
	GL_CLAMP_TO_EDGE_VALUE				= 0x812f,
	GL_WRITE_ONLY_VALUE					= 0x88b9,
	GL_SYNC_GPU_COMMANDS_COMPLETE_VALUE = 0x9117,
	GL_SYNC_FLUSH_COMMANDS_BIT_VALUE	= 0x00000001,
	GL_ALREADY_SIGNALED_VALUE			= 0x911a,
	GL_TIMEOUT_EXPIRED_VALUE			= 0x911b,
	GL_CONDITION_SATISFIED_VALUE		= 0x911c,
	GL_WAIT_FAILED_VALUE				= 0x911d,
	GL_TIME_ELAPSED_VALUE				= 0x88bf,
	GL_QUERY_RESULT_VALUE				= 0x8866,
	GL_RGB_VALUE						= 0x1907,
	GL_UNSIGNED_BYTE_VALUE				= 0x1401
};

typedef struct {
	createShaderFn		 CreateShader;
	getIntegervFn GetIntegerv;
	isEnabledFn IsEnabled;
	getFramebufferAttachmentParameterivFn GetFramebufferAttachmentParameteriv;
	createBuffersFn CreateFramebuffers, CreateVertexArrays;
	deleteBuffersFn DeleteFramebuffers, DeleteVertexArrays;
	bindFramebufferFn BindFramebuffer;
	namedFramebufferTextureFn NamedFramebufferTexture;
	checkNamedFramebufferStatusFn CheckNamedFramebufferStatus;
	copyNamedBufferSubDataFn CopyNamedBufferSubData;
	clearNamedBufferDataFn ClearNamedBufferData;
	bindVertexArrayFn BindVertexArray;
	disableFn Disable;
	disableFn Enable;
	depthMaskFn DepthMask;
	drawArraysFn DrawArrays;
	framebufferLayerFn NamedFramebufferTextureLayer;
	vertexBufferFn VertexArrayVertexBuffer;
	vertexAttribFn EnableVertexArrayAttrib;
	vertexFormatFn VertexArrayAttribFormat;
	vertexBindingFn VertexArrayAttribBinding;
	viewportFn Viewport;
	shaderSourceFn		 ShaderSource;
	compileShaderFn		 CompileShader;
	getShaderivFn		 GetShaderiv;
	deleteShaderFn		 DeleteShader;
	createProgramFn		 CreateProgram;
	attachShaderFn		 AttachShader;
	linkProgramFn		 LinkProgram;
	getProgramivFn		 GetProgramiv;
	deleteProgramFn		 DeleteProgram;
	useProgramFn		 UseProgram;
	createBuffersFn		 CreateBuffers;
	namedBufferStorageFn NamedBufferStorage;
	namedBufferSubDataFn NamedBufferSubData;
	getNamedBufferSubDataFn GetNamedBufferSubData;
	copyTextureSubImage2DFn CopyTextureSubImage2D;
	bindBufferBaseFn	 BindBufferBase;
	deleteBuffersFn		 DeleteBuffers;
	createTexturesFn	 CreateTextures;
	textureStorage2DFn TextureStorage2D;
	textureStorage3DFn TextureStorage3D;
	textureSubImage2DFn TextureSubImage2D;
	generateTextureMipmapFn GenerateTextureMipmap;
	textureParameteriFn TextureParameteri;
	deleteTexturesFn DeleteTextures;
	bindTextureUnitFn BindTextureUnit;
	bindImageTextureFn BindImageTexture;
	dispatchComputeFn	 DispatchCompute;
	memoryBarrierFn		 MemoryBarrier;
	fenceSyncFn FenceSync;
	clientWaitSyncFn ClientWaitSync;
	deleteSyncFn DeleteSync;
	genQueriesFn GenQueries;
	deleteQueriesFn DeleteQueries;
	beginQueryFn BeginQuery;
	endQueryFn EndQuery;
	getQueryObjectui64vFn GetQueryObjectui64v;
	pixelStoreiFn		 PixelStorei;
	readPixelsFn		 ReadPixels;
	getErrorFn			 GetError;
} productDispatch_t;

#define RAL_OPENGL_ATMOSPHERE_FROXEL_CAPACITY 262144u
#define RAL_OPENGL_WEATHER_PARTICLE_CAPACITY  8192u
#define RAL_OPENGL_SKY_TEXTURE_COUNT 5u
#define RAL_OPENGL_SKY_SH_BUFFER_COUNT 2u
#define RAL_OPENGL_SKY_SOURCE_SIZE WIRED_SKY_SOURCE_SIZE
#define RAL_OPENGL_SKY_IRRADIANCE_SIZE WIRED_SKY_IRRADIANCE_SIZE
#define RAL_OPENGL_SKY_RADIANCE_MIPS WIRED_SKY_SPECULAR_MIP_COUNT
#define RAL_OPENGL_SKY_PHYSICAL_TEXTURE_COUNT 3u
#define RAL_OPENGL_SKY_PHYSICAL_PROGRAM_COUNT 3u
#define RAL_OPENGL_SKY_RADIANCE_PROGRAM_COUNT 4u
#define RAL_OPENGL_SKY_RADIANCE_PARAM_BUFFER_COUNT 2u
#define RAL_OPENGL_SKY_TRANSMITTANCE_WIDTH 256u
#define RAL_OPENGL_SKY_TRANSMITTANCE_HEIGHT 64u
#define RAL_OPENGL_SKY_MULTISCATTER_SIZE 32u
#define RAL_OPENGL_SKY_VIEW_WIDTH 192u
#define RAL_OPENGL_SKY_VIEW_HEIGHT 108u

enum {
	RAL_OPENGL_SKY_TRANSMITTANCE = 0,
	RAL_OPENGL_SKY_MULTISCATTER,
	RAL_OPENGL_SKY_VIEW
};

enum {
	RAL_OPENGL_SKY_SOURCE = 0,
	RAL_OPENGL_SKY_PROJECT_SH,
	RAL_OPENGL_SKY_IRRADIANCE,
	RAL_OPENGL_SKY_PREFILTER_RADIANCE
};

enum {
	RAL_OPENGL_SKY_SOURCE_PARAMS = 0,
	RAL_OPENGL_SKY_SLICE_PARAMS
};

enum {
	RAL_OPENGL_ATMOSPHERE_PARAMS = 0,
	RAL_OPENGL_ATMOSPHERE_VOLUMES,
	RAL_OPENGL_ATMOSPHERE_MEDIA,
	RAL_OPENGL_ATMOSPHERE_TILE_LIGHTS,
	RAL_OPENGL_ATMOSPHERE_LIGHTS,
	RAL_OPENGL_ATMOSPHERE_TILE_DEPTH,
	RAL_OPENGL_ATMOSPHERE_LIT,
	RAL_OPENGL_ATMOSPHERE_CLOUD,
	RAL_OPENGL_ATMOSPHERE_INTEGRATED,
	RAL_OPENGL_ATMOSPHERE_HISTORY_A,
	RAL_OPENGL_ATMOSPHERE_HISTORY_B,
	RAL_OPENGL_ATMOSPHERE_FALLBACK,
	WIRED_OPENGL_ATMOSPHERE_BUFFER_COUNT
};

typedef struct {
	float sunDirectionAngularRadius[4];
	float sunRadiance[4];
	float moonDirectionAngularRadius[4];
	float moonRadiance[4];
	float atmosphereRadii[4];
	float scatteringScale[4];
	float groundExposure[4];
} productSkyPhysicalParams_t;

typedef ralSkyRadianceSourceParams_t productSkySourceParams_t;

typedef struct {
	uint32_t values[4];
} productSkySliceParams_t;

typedef char productSkyPhysicalParamsMustMatchStd140[
	sizeof( productSkyPhysicalParams_t ) == 112u ? 1 : -1];

typedef char productAtmosphereInjectParamsMustMatchStd140[sizeof( renderAtmosphereInjectGpu_t ) == 144u ? 1 : -1];
typedef char productAtmosphereLightParamsMustMatchStd140[sizeof( renderAtmosphereLightGpu_t ) == 192u ? 1 : -1];
typedef char productAtmosphereCloudParamsMustMatchStd140[sizeof( renderAtmosphereCloudGpu_t ) == 192u ? 1 : -1];
typedef char
	productAtmosphereIntegrateParamsMustMatchStd140[sizeof( renderAtmosphereIntegrateGpu_t ) == 32u ? 1 : -1];
_Static_assert( sizeof( renderAtmosphereTileGpu_t ) == 96u, "atmosphere tile ABI" );
_Static_assert( sizeof( renderAtmosphereVolumeGpu_t ) == 80u, "atmosphere volume ABI" );
_Static_assert( sizeof( renderAtmosphereLocalLightGpu_t ) == 48u, "atmosphere light ABI" );

typedef struct {
	float								mvp[16];
	float								viewLeft[4];
	float								viewUp[4];
	float								eyeWorld[4];
	float								dt;
	float								time;
	uint32_t							poolSize;
	uint32_t							pingPongRead;
	float								boundsMin[4];
	float								boundsMax[4];
	float								worldMins[2];
	float								worldMaxs[2];
	float								invGridStep[2];
	uint32_t							gridSize;
	uint32_t							atmType;
	float								distance;
	float								computePad[3];
	float								windGust[4];
	float								precipitation[4];
	float								dustAsh;
	float								indoorExposure;
	uint32_t							climateSeed;
	uint32_t							climatePad;
	float								climate[4];
	float								surfaceClimate[4];
	float								sun[4];
	float								moon[4];
	float								ambientCloud[4];
	float								cloudMedia[4];
	float								effectMeta[4];
	renderAtmosphereEffectGpuWorkload_t effectWorkloads[RENDER_SUBMISSION_MAX_ATMOSPHERE_EFFECT_WORKLOADS];
	float								renderParams[4];
} productWeatherFrame_t;

typedef struct {
	qhandle_t handle;
	uint64_t generation;
	uint64_t digest;
	glName_t texture;
	ralTextureResourceReceipt_t resource;
	ralMaterialRuntimeResource_t runtimeResource;
	qboolean clampToEdge;
} productMaterialEntry_t;

typedef char productWeatherFrameMustMatchStd140[sizeof( productWeatherFrame_t ) == 1152u ? 1 : -1];

typedef struct { float screenPosDepth[4], colorVis[4]; } productLensSource_t;
typedef struct {
	float viewport[2]; int32_t count; float radiusPx;
	float depthConvention[4];
} productLensPush_t;
typedef struct {
	glName_t records, params;
	glSync_t fence;
	uint64_t frame;
	uint32_t count;
	int ids[LENS_MAX_CGSOURCES];
} productLensSlot_t;
typedef char productLensPushMustMatchStd140[sizeof( productLensPush_t ) == 32u ? 1 : -1];
typedef char productLensSourceMustMatchStd430[sizeof( productLensSource_t ) == 32u ? 1 : -1];

struct ralOpenGlProduct_s {
	ralExposureSettings_t exposureSettings;
	glName_t hdrFramebuffer, hdrTextures[2], hdrPrograms[3], hdrBuffers[5], hdrVao;
	/* r_aa 2 (render_antialiasing.h: SMAA on the tone-mapped scene): the tone map renders into smaaToneTexture (RGBA16F, linear, the
	 * values the output pass would encode) and the SMAA neighbourhood resolve draws it into the output's sRGB attachment. */
	glName_t smaaToneFramebuffer, smaaToneTexture;
	uint32_t smaaToneWidth, smaaToneHeight;
	uint32_t hdrWidth, hdrHeight, exposureParity, tonemapMode;
	float tonemapExposure;
	qboolean exposureHistoryValid;
	int32_t exposureTimeline;
	uint64_t exposureWorld;
	ralOpenGlCore_t		  *core;
	ralOpenGlCoreReceipt_t coreReceipt;
	productDispatch_t	   gl;
	uint64_t			   generation;
	uint64_t			   lastFrameGeneration;
	uint64_t			   materialResourceGeneration;
	uint32_t			   outputWidth;
	uint32_t			   outputHeight;
	glName_t			   program;
	glName_t               skyMaterialProgram;
	glName_t               skyCaptureFramebuffer, skyCaptureVao, skyCaptureBuffers[3];
	int                    skyCaptureTimeMs;
	qboolean               skyMaterialUsesTime;
	glName_t lensProgram, lensDepth;
	productLensSlot_t lensSlots[3];
	float lensVisibility[LENS_MAX_CGSOURCES];
	qboolean lensActive[LENS_MAX_CGSOURCES];
	qboolean lensPublishedActive[LENS_MAX_CGSOURCES];
	uint64_t lensPublishedFrame;
	int32_t lensTimeline;
	qboolean lensHasTimeline;
	glName_t			   atmospherePrograms[5];
	glName_t			   atmosphereBuffers[WIRED_OPENGL_ATMOSPHERE_BUFFER_COUNT];
	glName_t			   weatherPrograms[2];
	glName_t			   weatherBuffers[3];
	glName_t weatherHeightgrid;
	uint64_t weatherHeightgridDigest;
	int weatherHeightgridWorldIndex;
	qboolean			   atmosphereBuffersReady;
	qboolean			   atmosphereHistoryValid;
	qboolean			   atmosphereHistoryParity;
	ralSkyEnvironmentRuntimeState_t skyRuntime;
	ralSkyRadianceState_t skyRadiance;
	ralSkyRadianceReceipt_t skyRadianceReceipt;
	ralSkyRadianceConsumerReceipt_t skyRadianceConsumer;
	ralSkyGpuTelemetryReceipt_t skyGpuTelemetry;
	uint64_t               skyRadianceFrameGeneration;
	float                  skyRadianceTimeline;
	uint64_t               skyWorldLifecycleGeneration;
	glName_t               skyTextures[RAL_OPENGL_SKY_TEXTURE_COUNT];
	glName_t               skyShBuffers[RAL_OPENGL_SKY_SH_BUFFER_COUNT];
	glName_t               skyPhysicalTextures[RAL_OPENGL_SKY_PHYSICAL_TEXTURE_COUNT];
	glName_t               skyPhysicalPrograms[RAL_OPENGL_SKY_PHYSICAL_PROGRAM_COUNT];
	glName_t               skyRadiancePrograms[RAL_OPENGL_SKY_RADIANCE_PROGRAM_COUNT];
	glName_t               skyPhysicalParamBuffer;
	glName_t               skyRadianceParamBuffers[RAL_OPENGL_SKY_RADIANCE_PARAM_BUFFER_COUNT];
	glSync_t               skyFence;
	uint32_t               skySubmittedWorkMask;
	glSync_t               skyRadianceFence;
	uint32_t               skyRadianceSubmittedSliceMask;
	uint64_t               skyRadianceSubmittedRevision;
	glName_t               skyTimingQueries[2];
	ralSkyGpuTimingState_t skyTimingState[2];
	uint64_t               skyTimingFrameGeneration[2];
	uint64_t               skyTimingDurationNanoseconds[2];
	qboolean               skyIrradianceWriteStarted;
	qboolean               skyRadianceWriteStarted;
	uint64_t               skyPhysicalDispatchCount;
	uint64_t               skyRadianceDispatchCount;
	uint64_t               skyResourceWorldLifecycleGeneration;
	qboolean               skyResourcesReady;
	renderEffectDecalPool_t effectDecalPool;
	renderEffectBeamPool_t effectBeamPool;
	qboolean			   weatherBuffersReady;
	qboolean			   weatherParity;
	float				   weatherTimeline;
	ralOpenGlLightingReceipt_t directionalLighting;
	qboolean                 hasDirectionalLighting;
	ralDisplayVisibilityPlan_t displayVisibility;
	float aerialHaze; /* r_atmosphereAerialHaze weight, 0..1 */
	float lightmapBoost; /* r_lightmapBoost, linear world-lightmap overbright */
	/* Anti-aliasing (the r_aa plan: TAA, or SMAA at its preset, never both), created on first use. */
	ralOpenGlAa_t *aa;
	qboolean taaEnabled;
	int smaaQuality;
	ralTemporalState_t taaState;
	uint64_t taaWorldGeneration;
	renderTemporalCutLatch_t taaCut;   /* camera cuts held until a temporal frame commits (render_temporal_view.h) */
	uint32_t taaTopologyEpoch;
	ralOpenGlAaCamera_t taaPreviousCamera;
	uint32_t taaResolvedFrames, taaBootstrapFrames, smaaFrames;
	productMaterialEntry_t materials[RENDER_SUBMISSION_MAX_TEXTURE_RESOURCES];
	uint32_t materialTextureNames[RENDER_SUBMISSION_MAX_TEXTURE_RESOURCES];
	uint32_t cookedMaterialTextureNames[RENDER_SUBMISSION_MAX_MATERIALS];
	qboolean cookedMaterialReady[RENDER_SUBMISSION_MAX_MATERIALS];
	uint64_t cookedMaterialRuntimeGeneration;
	uint32_t materialCount;
	uint32_t textureResourceCount;
	renderDistantWorldDrawMetric_t distantMetricScratch[
		RAL_SKY_DISTANT_MAX_CANDIDATES];
	ralSkyDistantDrawCandidate_t distantCandidateScratch[
		RAL_SKY_DISTANT_MAX_CANDIDATES];
};

static qboolean LoadDispatch( const ralOpenGlCore_t *core, productDispatch_t *gl )
{
#define LOAD_GL( member, symbol, type )                               \
	do {                                                              \
		ralOpenGlProc_t proc = RalOpenGl_CoreResolve( core, symbol ); \
		if ( !proc )                                                  \
			return qfalse;                                            \
		gl->member = (type)proc;                                      \
	} while ( 0 )
	memset( gl, 0, sizeof( *gl ) );
	LOAD_GL( CreateShader, "glCreateShader", createShaderFn );
	LOAD_GL( GetIntegerv, "glGetIntegerv", getIntegervFn );
	LOAD_GL( IsEnabled, "glIsEnabled", isEnabledFn );
	LOAD_GL( GetFramebufferAttachmentParameteriv, "glGetFramebufferAttachmentParameteriv", getFramebufferAttachmentParameterivFn );
	LOAD_GL( CreateFramebuffers, "glCreateFramebuffers", createBuffersFn );
	LOAD_GL( DeleteFramebuffers, "glDeleteFramebuffers", deleteBuffersFn );
	LOAD_GL( BindFramebuffer, "glBindFramebuffer", bindFramebufferFn );
	LOAD_GL( NamedFramebufferTexture, "glNamedFramebufferTexture", namedFramebufferTextureFn );
	LOAD_GL( NamedFramebufferTextureLayer, "glNamedFramebufferTextureLayer", framebufferLayerFn );
	LOAD_GL( VertexArrayVertexBuffer, "glVertexArrayVertexBuffer", vertexBufferFn );
	LOAD_GL( EnableVertexArrayAttrib, "glEnableVertexArrayAttrib", vertexAttribFn );
	LOAD_GL( VertexArrayAttribFormat, "glVertexArrayAttribFormat", vertexFormatFn );
	LOAD_GL( VertexArrayAttribBinding, "glVertexArrayAttribBinding", vertexBindingFn );
	LOAD_GL( Viewport, "glViewport", viewportFn );
	LOAD_GL( CheckNamedFramebufferStatus, "glCheckNamedFramebufferStatus", checkNamedFramebufferStatusFn );
	LOAD_GL( CopyNamedBufferSubData, "glCopyNamedBufferSubData", copyNamedBufferSubDataFn );
	LOAD_GL( ClearNamedBufferData, "glClearNamedBufferData", clearNamedBufferDataFn );
	LOAD_GL( CreateVertexArrays, "glCreateVertexArrays", createBuffersFn );
	LOAD_GL( DeleteVertexArrays, "glDeleteVertexArrays", deleteBuffersFn );
	LOAD_GL( BindVertexArray, "glBindVertexArray", bindVertexArrayFn );
	LOAD_GL( Disable, "glDisable", disableFn );
	LOAD_GL( Enable, "glEnable", disableFn );
	LOAD_GL( DepthMask, "glDepthMask", depthMaskFn );
	LOAD_GL( DrawArrays, "glDrawArrays", drawArraysFn );
	LOAD_GL( ShaderSource, "glShaderSource", shaderSourceFn );
	LOAD_GL( CompileShader, "glCompileShader", compileShaderFn );
	LOAD_GL( GetShaderiv, "glGetShaderiv", getShaderivFn );
	LOAD_GL( DeleteShader, "glDeleteShader", deleteShaderFn );
	LOAD_GL( CreateProgram, "glCreateProgram", createProgramFn );
	LOAD_GL( AttachShader, "glAttachShader", attachShaderFn );
	LOAD_GL( LinkProgram, "glLinkProgram", linkProgramFn );
	LOAD_GL( GetProgramiv, "glGetProgramiv", getProgramivFn );
	LOAD_GL( DeleteProgram, "glDeleteProgram", deleteProgramFn );
	LOAD_GL( UseProgram, "glUseProgram", useProgramFn );
	LOAD_GL( CreateBuffers, "glCreateBuffers", createBuffersFn );
	LOAD_GL( NamedBufferStorage, "glNamedBufferStorage", namedBufferStorageFn );
	LOAD_GL( NamedBufferSubData, "glNamedBufferSubData", namedBufferSubDataFn );
	LOAD_GL( GetNamedBufferSubData, "glGetNamedBufferSubData", getNamedBufferSubDataFn );
	LOAD_GL( CopyTextureSubImage2D, "glCopyTextureSubImage2D", copyTextureSubImage2DFn );
	LOAD_GL( BindBufferBase, "glBindBufferBase", bindBufferBaseFn );
	LOAD_GL( DeleteBuffers, "glDeleteBuffers", deleteBuffersFn );
	LOAD_GL( CreateTextures, "glCreateTextures", createTexturesFn );
	LOAD_GL( TextureStorage2D, "glTextureStorage2D", textureStorage2DFn );
	LOAD_GL( TextureStorage3D, "glTextureStorage3D", textureStorage3DFn );
	LOAD_GL( TextureSubImage2D, "glTextureSubImage2D", textureSubImage2DFn );
	LOAD_GL( GenerateTextureMipmap, "glGenerateTextureMipmap", generateTextureMipmapFn );
	LOAD_GL( TextureParameteri, "glTextureParameteri", textureParameteriFn );
	LOAD_GL( DeleteTextures, "glDeleteTextures", deleteTexturesFn );
	LOAD_GL( BindTextureUnit, "glBindTextureUnit", bindTextureUnitFn );
	LOAD_GL( BindImageTexture, "glBindImageTexture", bindImageTextureFn );
	LOAD_GL( DispatchCompute, "glDispatchCompute", dispatchComputeFn );
	LOAD_GL( MemoryBarrier, "glMemoryBarrier", memoryBarrierFn );
	LOAD_GL( FenceSync, "glFenceSync", fenceSyncFn );
	LOAD_GL( ClientWaitSync, "glClientWaitSync", clientWaitSyncFn );
	LOAD_GL( DeleteSync, "glDeleteSync", deleteSyncFn );
	LOAD_GL( GenQueries, "glGenQueries", genQueriesFn );
	LOAD_GL( DeleteQueries, "glDeleteQueries", deleteQueriesFn );
	LOAD_GL( BeginQuery, "glBeginQuery", beginQueryFn );
	LOAD_GL( EndQuery, "glEndQuery", endQueryFn );
	LOAD_GL( GetQueryObjectui64v, "glGetQueryObjectui64v",
		getQueryObjectui64vFn );
	LOAD_GL( PixelStorei, "glPixelStorei", pixelStoreiFn );
	LOAD_GL( ReadPixels, "glReadPixels", readPixelsFn );
	LOAD_GL( GetError, "glGetError", getErrorFn );
#undef LOAD_GL
	return qtrue;
}

static glName_t Compile( const productDispatch_t *gl, glEnum_t stage, const unsigned char *source,
						 unsigned int sourceByteCount )
{
	glName_t		shader	 = gl->CreateShader( stage );
	glInt_t			compiled = 0;
	glInt_t			sourceLength;
	const glChar_t *sourceText = (const glChar_t *)source;
	if ( !shader || !source || !sourceByteCount || sourceByteCount > INT_MAX )
		return 0u;
	sourceLength = (glInt_t)sourceByteCount;
	gl->ShaderSource( shader, 1, &sourceText, &sourceLength );
	gl->CompileShader( shader );
	gl->GetShaderiv( shader, GL_COMPILE_STATUS_VALUE, &compiled );
	if ( !compiled || gl->GetError() != GL_NO_ERROR_VALUE ) {
		gl->DeleteShader( shader );
		return 0u;
	}
	return shader;
}

static glName_t LinkSingleStage( const productDispatch_t *gl, glName_t shader )
{
	glName_t program;
	glInt_t	 linked = 0;
	if ( !gl || !shader )
		return 0u;
	program = gl->CreateProgram();
	if ( !program )
		return 0u;
	gl->AttachShader( program, shader );
	gl->LinkProgram( program );
	gl->GetProgramiv( program, GL_LINK_STATUS_VALUE, &linked );
	if ( !linked || gl->GetError() != GL_NO_ERROR_VALUE ) {
		gl->DeleteProgram( program );
		return 0u;
	}
	return program;
}


static glName_t CompileToneProgram( ralOpenGlProduct_t *product,
	uint32_t mode, float exposure )
{
	productDispatch_t *gl = &product->gl;
	char prefix[192];
	glName_t vertex = 0u, fragment = 0u, program = 0u;
	glInt_t linked = 0;
	const char *body = (const char *)ralOpenGlProductTonemapSource;
	const size_t versionBytes = strcspn( body, "\n" ) + 1u;
	const char *parts[3];
	glInt_t lengths[3];
	int length = snprintf( prefix, sizeof( prefix ),
		"#define WIRED_SPEC_17 %u\n#define WIRED_SPEC_18 %.9e\n",
		mode, (double)( mode ? exposure : 1.0f ) );
	if ( length <= 0 || (size_t)length >= sizeof( prefix ) ) return 0u;
	vertex = Compile( gl, GL_VERTEX_SHADER_VALUE,
		ralOpenGlProductFullscreenVertexSource, ralOpenGlProductFullscreenVertexByteCount );
	if ( !vertex ) goto cleanup;
	fragment = gl->CreateShader( GL_FRAGMENT_SHADER_VALUE );
	if ( !fragment ) goto cleanup;
	/* Preserve the generated source's version line and body verbatim. Only
	 * specialization values are adapter program-creation input. */
	parts[0] = body; lengths[0] = (glInt_t)versionBytes;
	parts[1] = prefix; lengths[1] = length;
	parts[2] = body + versionBytes;
	lengths[2] = (glInt_t)( ralOpenGlProductTonemapByteCount - versionBytes );
	gl->ShaderSource( fragment, 3, parts, lengths );
	gl->CompileShader( fragment );
	gl->GetShaderiv( fragment, GL_COMPILE_STATUS_VALUE, &linked );
	if ( !linked ) goto cleanup;
	program = gl->CreateProgram();
	if ( !program ) goto cleanup;
	gl->AttachShader( program, vertex ); gl->AttachShader( program, fragment );
	gl->LinkProgram( program );
	gl->GetProgramiv( program, GL_LINK_STATUS_VALUE, &linked );
	if ( !linked ) { gl->DeleteProgram( program ); program = 0u; }
cleanup:
	if ( vertex ) gl->DeleteShader( vertex );
	if ( fragment ) gl->DeleteShader( fragment );
	return program;
}

qboolean RalOpenGl_ProductSetExposure( ralOpenGlProduct_t *product,
	const ralExposureSettings_t *settings, uint32_t tonemapMode, float tonemapExposure )
{
	if ( !product || !Ral_ExposureSettingsValid( settings ) || tonemapMode > 4u
			|| !isfinite( tonemapExposure ) || tonemapExposure < 0.01f
			|| tonemapExposure > 16.0f ) return qfalse;
	if ( product->hdrPrograms[2] && ( product->tonemapMode != tonemapMode
			|| product->tonemapExposure != tonemapExposure ) ) {
		glName_t candidate = CompileToneProgram( product, tonemapMode, tonemapExposure );
		if ( !candidate ) return qfalse;
		product->gl.DeleteProgram( product->hdrPrograms[2] );
		product->hdrPrograms[2] = candidate;
	}
	if ( product->exposureSettings.enabled != settings->enabled )
		product->exposureHistoryValid = qfalse;
	product->exposureSettings = *settings;
	product->tonemapMode = tonemapMode;
	product->tonemapExposure = tonemapExposure;
	return qtrue;
}

static qboolean PrepareHdr( ralOpenGlProduct_t *product )
{
	productDispatch_t *gl = &product->gl;
	if ( !product->hdrPrograms[0] ) {
		glName_t shader = Compile( gl, GL_COMPUTE_SHADER_VALUE,
			ralOpenGlProductHdrHistogramSource, ralOpenGlProductHdrHistogramByteCount );
		if ( !shader ) return qfalse;
		product->hdrPrograms[0] = LinkSingleStage( gl, shader );
		gl->DeleteShader( shader );
		if ( !product->hdrPrograms[0] ) return qfalse;
	}
	if ( !product->hdrPrograms[1] ) {
		glName_t shader = Compile( gl, GL_COMPUTE_SHADER_VALUE,
			ralOpenGlProductExposureSource, ralOpenGlProductExposureByteCount );
		if ( !shader ) return qfalse;
		product->hdrPrograms[1] = LinkSingleStage( gl, shader );
		gl->DeleteShader( shader );
		if ( !product->hdrPrograms[1] ) return qfalse;
	}
	if ( !product->hdrPrograms[2] ) {
		product->hdrPrograms[2] = CompileToneProgram( product,
			product->tonemapMode, product->tonemapExposure );
		if ( !product->hdrPrograms[2] ) return qfalse;
	}
	if ( !product->hdrBuffers[0] ) {
		const glSizePtr_t sizes[5] = { 256u * sizeof(uint32_t), sizeof(float),
			sizeof(float), sizeof(ralExposureBlock_t), 16u };
		glName_t candidate[5] = {0};
		gl->CreateBuffers( 5, candidate );
		for ( unsigned i = 0; i < 5; ++i ) {
			if ( !candidate[i] ) {
				gl->DeleteBuffers( 5, candidate ); return qfalse;
			}
			gl->NamedBufferStorage( candidate[i], sizes[i], NULL,
				GL_DYNAMIC_STORAGE_BIT_VALUE );
		}
		if ( gl->GetError() != GL_NO_ERROR_VALUE ) {
			gl->DeleteBuffers( 5, candidate ); return qfalse;
		}
		memcpy( product->hdrBuffers, candidate, sizeof(candidate) );
	}
	if ( !product->hdrVao ) gl->CreateVertexArrays( 1, &product->hdrVao );
	if ( !product->hdrVao ) return qfalse;
	if ( product->hdrWidth != product->outputWidth
			|| product->hdrHeight != product->outputHeight || !product->hdrFramebuffer ) {
		glName_t textures[2] = {0}, framebuffer = 0u;
		gl->CreateTextures( GL_TEXTURE_2D_VALUE, 2, textures );
		gl->CreateFramebuffers( 1, &framebuffer );
		if ( !textures[0] || !textures[1] || !framebuffer ) goto rejectTarget;
		for ( unsigned i = 0; i < 2; ++i ) {
			gl->TextureStorage2D( textures[i], 1, i == 0
				? GL_RGBA16F_VALUE : GL_DEPTH_COMPONENT32F_VALUE,
				(glSize_t)product->outputWidth, (glSize_t)product->outputHeight );
			gl->TextureParameteri( textures[i], GL_TEXTURE_MIN_FILTER_VALUE, GL_LINEAR_VALUE );
			gl->TextureParameteri( textures[i], GL_TEXTURE_MAG_FILTER_VALUE, GL_LINEAR_VALUE );
			gl->TextureParameteri( textures[i], GL_TEXTURE_WRAP_S_VALUE, GL_CLAMP_TO_EDGE_VALUE );
			gl->TextureParameteri( textures[i], GL_TEXTURE_WRAP_T_VALUE, GL_CLAMP_TO_EDGE_VALUE );
			gl->NamedFramebufferTexture( framebuffer, i == 0
				? GL_COLOR_ATTACHMENT0_VALUE : GL_DEPTH_ATTACHMENT_VALUE, textures[i], 0 );
		}
		if ( gl->CheckNamedFramebufferStatus( framebuffer, GL_FRAMEBUFFER_VALUE )
				!= GL_FRAMEBUFFER_COMPLETE_VALUE || gl->GetError() != GL_NO_ERROR_VALUE )
			goto rejectTarget;
		if ( product->hdrFramebuffer ) gl->DeleteFramebuffers( 1, &product->hdrFramebuffer );
		gl->DeleteTextures( 2, product->hdrTextures );
		product->hdrFramebuffer = framebuffer;
		memcpy( product->hdrTextures, textures, sizeof(textures) );
		product->hdrWidth = product->outputWidth;
		product->hdrHeight = product->outputHeight;
		product->exposureHistoryValid = qfalse;
		goto targetReady;
rejectTarget:
		if ( framebuffer ) gl->DeleteFramebuffers( 1, &framebuffer );
		gl->DeleteTextures( 2, textures );
		return qfalse;
	}
targetReady:
	return gl->GetError() == GL_NO_ERROR_VALUE;
}

static qboolean FrameValid( const ralOpenGlProductFrameReceipt_t *receipt )
{
	return receipt && receipt->schemaVersion == RAL_OPENGL_PRODUCT_SCHEMA_VERSION &&
		   receipt->backendType == RAL_BACKEND_OPENGL && receipt->productGeneration && receipt->frameGeneration &&
		   receipt->postprocessDrawCount == 1u &&
		   (receipt->exposureDispatchCount == 0u || receipt->exposureDispatchCount == 2u) &&
		   (receipt->aaDispatchCount == 0u || receipt->aaDispatchCount == 2u) &&
		   (receipt->aaDrawCount == 0u || receipt->aaDrawCount == 3u) &&
		   !(receipt->aaDispatchCount && receipt->aaDrawCount) &&
		   (receipt->exposureHistoryReset == qfalse || receipt->exposureHistoryReset == qtrue) &&
		   (!receipt->exposureHistoryReset || receipt->exposureDispatchCount == 2u) &&
		   receipt->vertexShaderByteCount == ralOpenGlProductVertexByteCount &&
		   receipt->fragmentShaderByteCount == ralOpenGlProductFragmentByteCount &&
		   receipt->vertexShaderDigestLane0 == ralOpenGlProductVertexDigestLane0 &&
		   receipt->vertexShaderDigestLane1 == ralOpenGlProductVertexDigestLane1 &&
		   receipt->fragmentShaderDigestLane0 == ralOpenGlProductFragmentDigestLane0 &&
		   receipt->fragmentShaderDigestLane1 == ralOpenGlProductFragmentDigestLane1 &&
		   RalOpenGl_FrontendPlanReceiptExact( &receipt->plan, &receipt->plan ) &&
		   receipt->effectDecalPool.schemaVersion
			== RENDER_EFFECT_DECAL_POOL_SCHEMA_VERSION &&
		   receipt->effectDecalPool.frameGeneration == receipt->frameGeneration &&
		   receipt->effectDecalPool.ready == qtrue &&
		   receipt->effectBeamPool.schemaVersion
			== RENDER_EFFECT_BEAM_POOL_SCHEMA_VERSION &&
		   receipt->effectBeamPool.frameGeneration == receipt->frameGeneration &&
		   receipt->effectBeamPool.ready == qtrue &&
		   Ral_DisplayVisibilityPlanValid( &receipt->displayVisibility ) &&
		   RalOpenGl_WorldReceiptExact( &receipt->native, &receipt->native ) &&
		   Ral_AtmospherePlanReceiptExact( &receipt->atmosphere, &receipt->atmosphere ) &&
		   Ral_SkyGpuTelemetryReceiptExact( &receipt->skyGpuTelemetry,
			   &receipt->skyGpuTelemetry ) &&
		   receipt->plan.planGeneration == receipt->frameGeneration &&
		   receipt->native.generation == receipt->frameGeneration &&
		   receipt->atmosphere.frameGeneration == receipt->frameGeneration &&
		   receipt->atmosphereDispatchCount == receipt->atmosphere.computeDispatchCount &&
		   receipt->atmosphereNativeDispatchCount == ( receipt->atmosphereDispatchCount
			   ? receipt->atmosphereDispatchCount + 1u : 0u ) &&
		   receipt->atmosphereFroxelCount == receipt->atmosphere.froxelCount &&
		   receipt->atmosphereCompositeCount == receipt->atmosphere.compositeCount &&
		   receipt->atmosphereCloudsActive == receipt->atmosphere.cloudsActive &&
		   Ral_AtmosphereWeatherReceiptExact( &receipt->weather, &receipt->weather ) &&
		   receipt->weatherDispatchCount == ( receipt->weather.zeroWork ? 0u : 1u ) &&
		   receipt->weatherDrawCount == ( receipt->weather.zeroWork ? 0u : 1u ) &&
		   receipt->lensSourceCount <= LENS_MAX_CGSOURCES &&
		   receipt->lensDispatchCount <= 1u && receipt->lensDeferredCount <= 1u &&
		   receipt->lensCompletedCount <= 3u &&
		   receipt->lensDispatchCount + receipt->lensDeferredCount
			== ( receipt->lensSourceCount ? 1u : 0u ) &&
		   receipt->native.weatherInstanceCount == receipt->weather.activeParticleCount &&
		   receipt->native.loweringDigest == receipt->plan.loweringDigest && !receipt->unresolvedCount &&
		   !receipt->fallbackCount && !receipt->fatalCount && receipt->ready == qtrue;
}

qboolean RalOpenGl_ProductFrameReceiptExact( const ralOpenGlProductFrameReceipt_t *a,
											 const ralOpenGlProductFrameReceipt_t *b )
{
	return FrameValid( a ) && FrameValid( b ) && !memcmp( a, b, sizeof( *a ) );
}

qboolean RalOpenGl_ProductCreate( ralOpenGlCore_t *core, const ralOpenGlCoreReceipt_t *coreReceipt, uint64_t generation,
								  ralOpenGlProduct_t **outProduct )
{
	ralOpenGlProduct_t	*product;
	glName_t			 vertex = 0u, fragment = 0u, compute = 0u, skyFragment = 0u;
	glName_t			 weatherVertex = 0u, weatherFragment = 0u;
	const unsigned char *computeSources[5] = {
		ralOpenGlProductAtmosphereInjectSource, ralOpenGlProductAtmosphereLightSource,
		ralOpenGlProductAtmosphereCloudSource, ralOpenGlProductAtmosphereIntegrateSource,
		ralOpenGlProductAtmosphereTileSource };
	const unsigned int computeByteCounts[5] = {
		ralOpenGlProductAtmosphereInjectByteCount, ralOpenGlProductAtmosphereLightByteCount,
		ralOpenGlProductAtmosphereCloudByteCount, ralOpenGlProductAtmosphereIntegrateByteCount,
		ralOpenGlProductAtmosphereTileByteCount };
	const unsigned char *skyPhysicalSources[RAL_OPENGL_SKY_PHYSICAL_PROGRAM_COUNT] = {
		ralOpenGlProductSkyTransmittanceSource,
		ralOpenGlProductSkyMultiscatterSource,
		ralOpenGlProductSkyViewSource };
	const unsigned int skyPhysicalByteCounts[RAL_OPENGL_SKY_PHYSICAL_PROGRAM_COUNT] = {
		ralOpenGlProductSkyTransmittanceByteCount,
		ralOpenGlProductSkyMultiscatterByteCount,
		ralOpenGlProductSkyViewByteCount };
	const unsigned char *skyRadianceSources[RAL_OPENGL_SKY_RADIANCE_PROGRAM_COUNT] = {
		ralOpenGlProductSkySourceSource,
		ralOpenGlProductSkyProjectShSource,
		ralOpenGlProductSkyIrradianceSource,
		ralOpenGlProductSkyPrefilterRadianceSource };
	const unsigned int skyRadianceByteCounts[RAL_OPENGL_SKY_RADIANCE_PROGRAM_COUNT] = {
		ralOpenGlProductSkySourceByteCount,
		ralOpenGlProductSkyProjectShByteCount,
		ralOpenGlProductSkyIrradianceByteCount,
		ralOpenGlProductSkyPrefilterRadianceByteCount };
	glInt_t linked = 0;
	if ( !core || !coreReceipt || !generation || generation == UINT64_MAX || !outProduct ||
		 !RalOpenGl_CoreMatchesReceipt( core, coreReceipt ) )
		return qfalse;
	product = calloc( 1u, sizeof( *product ) );
	if ( !product )
		return qfalse;
	product->core		 = core;
	product->coreReceipt = *coreReceipt;
	product->generation	 = generation;
	product->materialResourceGeneration = generation;
	product->exposureSettings = (ralExposureSettings_t){ 0, 0.0123f, 0.5f, 0.2f, 1.5f, 0.5f, 0.25f, 8.0f };
	product->tonemapExposure = 1.0f;
	if ( !Ral_SkyEnvironmentRuntimeInit( RAL_BACKEND_OPENGL,
			&product->skyRuntime ) ) goto fail;
	if ( !Ral_SkyRadianceInit( RAL_BACKEND_OPENGL,
			&product->skyRadiance ) ) goto fail;
	if ( !Ral_DisplayVisibilityPlanBuild( RAL_DISPLAY_BRIGHTNESS_AUTHORED,
			&product->displayVisibility ) ) goto fail;
	RenderEffectBeamPool_Reset( &product->effectBeamPool );
	RenderEffectDecalPool_Reset( &product->effectDecalPool );
	if ( !LoadDispatch( core, &product->gl ) )
		goto fail;
	product->lightmapBoost = 4.6f; /* r_lightmapBoost default; the module overrides it every frame */
	product->gl.GenQueries( 2, product->skyTimingQueries );
	if ( !product->skyTimingQueries[0] || !product->skyTimingQueries[1]
			|| product->gl.GetError() != GL_NO_ERROR_VALUE ) goto fail;
	product->skyTimingState[0] = RAL_SKY_GPU_TIMING_UNAVAILABLE;
	product->skyTimingState[1] = RAL_SKY_GPU_TIMING_UNAVAILABLE;
	vertex =
		Compile( &product->gl, GL_VERTEX_SHADER_VALUE, ralOpenGlProductVertexSource, ralOpenGlProductVertexByteCount );
	fragment = Compile( &product->gl, GL_FRAGMENT_SHADER_VALUE, ralOpenGlProductFragmentSource,
						ralOpenGlProductFragmentByteCount );
	if ( !vertex || !fragment )
		goto fail;
	product->program = product->gl.CreateProgram();
	if ( !product->program )
		goto fail;
	product->gl.AttachShader( product->program, vertex );
	product->gl.AttachShader( product->program, fragment );
	product->gl.LinkProgram( product->program );
	product->gl.GetProgramiv( product->program, GL_LINK_STATUS_VALUE, &linked );
	if ( !linked || product->gl.GetError() != GL_NO_ERROR_VALUE )
		goto fail;
	skyFragment = Compile( &product->gl, GL_FRAGMENT_SHADER_VALUE,
		ralOpenGlProductSkyMaterialSource, ralOpenGlProductSkyMaterialByteCount );
	if ( !skyFragment ) goto fail;
	product->skyMaterialProgram = product->gl.CreateProgram();
	if ( !product->skyMaterialProgram ) goto fail;
	product->gl.AttachShader( product->skyMaterialProgram, vertex );
	product->gl.AttachShader( product->skyMaterialProgram, skyFragment );
	product->gl.LinkProgram( product->skyMaterialProgram );
	product->gl.GetProgramiv( product->skyMaterialProgram, GL_LINK_STATUS_VALUE, &linked );
	if ( !linked || product->gl.GetError() != GL_NO_ERROR_VALUE ) goto fail;
	product->gl.DeleteShader( skyFragment );
	skyFragment = 0u;
	for ( uint32_t i = 0u; i < 5u; ++i ) {
		compute = Compile( &product->gl, GL_COMPUTE_SHADER_VALUE, computeSources[i], computeByteCounts[i] );
		if ( !compute )
			goto fail;
		product->atmospherePrograms[i] = LinkSingleStage( &product->gl, compute );
		product->gl.DeleteShader( compute );
		compute = 0u;
		if ( !product->atmospherePrograms[i] )
			goto fail;
	}
	for ( uint32_t i = 0u; i < RAL_OPENGL_SKY_PHYSICAL_PROGRAM_COUNT; ++i ) {
		compute = Compile( &product->gl, GL_COMPUTE_SHADER_VALUE,
			skyPhysicalSources[i], skyPhysicalByteCounts[i] );
		if ( !compute ) goto fail;
		product->skyPhysicalPrograms[i] = LinkSingleStage( &product->gl, compute );
		product->gl.DeleteShader( compute );
		compute = 0u;
		if ( !product->skyPhysicalPrograms[i] ) goto fail;
	}
	for ( uint32_t i = 0u; i < RAL_OPENGL_SKY_RADIANCE_PROGRAM_COUNT; ++i ) {
		compute = Compile( &product->gl, GL_COMPUTE_SHADER_VALUE,
			skyRadianceSources[i], skyRadianceByteCounts[i] );
		if ( !compute ) goto fail;
		product->skyRadiancePrograms[i] = LinkSingleStage( &product->gl, compute );
		product->gl.DeleteShader( compute );
		compute = 0u;
		if ( !product->skyRadiancePrograms[i] ) goto fail;
	}
	compute = Compile( &product->gl, GL_COMPUTE_SHADER_VALUE, ralOpenGlProductWeatherIntegrateSource,
					   ralOpenGlProductWeatherIntegrateByteCount );
	if ( !compute )
		goto fail;
	product->weatherPrograms[0] = LinkSingleStage( &product->gl, compute );
	product->gl.DeleteShader( compute );
	compute			= 0u;
	weatherVertex	= Compile( &product->gl, GL_VERTEX_SHADER_VALUE, ralOpenGlProductWeatherVertexSource,
							   ralOpenGlProductWeatherVertexByteCount );
	weatherFragment = Compile( &product->gl, GL_FRAGMENT_SHADER_VALUE, ralOpenGlProductWeatherFragmentSource,
							   ralOpenGlProductWeatherFragmentByteCount );
	if ( !product->weatherPrograms[0] || !weatherVertex || !weatherFragment )
		goto fail;
	product->weatherPrograms[1] = product->gl.CreateProgram();
	if ( !product->weatherPrograms[1] )
		goto fail;
	product->gl.AttachShader( product->weatherPrograms[1], weatherVertex );
	product->gl.AttachShader( product->weatherPrograms[1], weatherFragment );
	product->gl.LinkProgram( product->weatherPrograms[1] );
	product->gl.GetProgramiv( product->weatherPrograms[1], GL_LINK_STATUS_VALUE, &linked );
	if ( !linked || product->gl.GetError() != GL_NO_ERROR_VALUE )
		goto fail;
	product->gl.DeleteShader( weatherFragment );
	weatherFragment = 0u;
	product->gl.DeleteShader( weatherVertex );
	weatherVertex = 0u;
	product->gl.CreateBuffers( 1, &product->atmosphereBuffers[RAL_OPENGL_ATMOSPHERE_FALLBACK] );
	if ( !product->atmosphereBuffers[RAL_OPENGL_ATMOSPHERE_FALLBACK] )
		goto fail;
	product->gl.NamedBufferStorage( product->atmosphereBuffers[RAL_OPENGL_ATMOSPHERE_FALLBACK], 16, NULL, 0u );
	if ( product->gl.GetError() != GL_NO_ERROR_VALUE )
		goto fail;
	product->gl.DeleteShader( fragment );
	product->gl.DeleteShader( vertex );
	*outProduct = product;
	return qtrue;
fail:
	if ( skyFragment ) product->gl.DeleteShader( skyFragment );
	if ( product->skyMaterialProgram ) product->gl.DeleteProgram( product->skyMaterialProgram );
	if ( compute )
		product->gl.DeleteShader( compute );
	if ( weatherFragment )
		product->gl.DeleteShader( weatherFragment );
	if ( weatherVertex )
		product->gl.DeleteShader( weatherVertex );
	if ( product->atmosphereBuffers[RAL_OPENGL_ATMOSPHERE_FALLBACK] )
		product->gl.DeleteBuffers( 1, &product->atmosphereBuffers[RAL_OPENGL_ATMOSPHERE_FALLBACK] );
	for ( uint32_t i = 0u; i < 5u; ++i )
		if ( product->atmospherePrograms[i] )
			product->gl.DeleteProgram( product->atmospherePrograms[i] );
	for ( uint32_t i = 0u; i < RAL_OPENGL_SKY_PHYSICAL_PROGRAM_COUNT; ++i )
		if ( product->skyPhysicalPrograms[i] )
			product->gl.DeleteProgram( product->skyPhysicalPrograms[i] );
	for ( uint32_t i = 0u; i < RAL_OPENGL_SKY_RADIANCE_PROGRAM_COUNT; ++i )
		if ( product->skyRadiancePrograms[i] )
			product->gl.DeleteProgram( product->skyRadiancePrograms[i] );
	for ( uint32_t i = 0u; i < 2u; ++i )
		if ( product->weatherPrograms[i] )
			product->gl.DeleteProgram( product->weatherPrograms[i] );
	if ( product->program )
		product->gl.DeleteProgram( product->program );
	if ( product->skyTimingQueries[0] )
		product->gl.DeleteQueries( 2, product->skyTimingQueries );
	if ( fragment )
		product->gl.DeleteShader( fragment );
	if ( vertex )
		product->gl.DeleteShader( vertex );
	free( product );
	return qfalse;
}

static qboolean CollectSkyWorkCompletion( ralOpenGlProduct_t *product,
		qboolean wait, uint32_t *outCompletedWorkMask )
{
	glEnum_t result;
	if ( !product || !outCompletedWorkMask ) return qfalse;
	*outCompletedWorkMask = 0u;
	if ( !product->skyFence ) return qtrue;
	result = product->gl.ClientWaitSync( product->skyFence,
		wait ? GL_SYNC_FLUSH_COMMANDS_BIT_VALUE : 0u,
		wait ? ~(glUInt64_t)0u : 0u );
	if ( result == GL_WAIT_FAILED_VALUE ) return qfalse;
	if ( result == GL_TIMEOUT_EXPIRED_VALUE ) return qtrue;
	if ( result != GL_ALREADY_SIGNALED_VALUE &&
		 result != GL_CONDITION_SATISFIED_VALUE ) return qfalse;
	product->gl.DeleteSync( product->skyFence );
	product->skyFence = NULL;
	{
		/* GLuint64 is unsigned long long; uint64_t is unsigned long on LP64 Linux, so read through the GL type. */
		glUInt64_t duration = 0u;
		product->gl.GetQueryObjectui64v( product->skyTimingQueries[0], GL_QUERY_RESULT_VALUE, &duration );
		product->skyTimingDurationNanoseconds[0] = (uint64_t)duration;
	}
	product->skyTimingState[0] = product->gl.GetError() == GL_NO_ERROR_VALUE
			&& product->skyTimingDurationNanoseconds[0] > 0u
		? RAL_SKY_GPU_TIMING_VALID : RAL_SKY_GPU_TIMING_FAILED;
	*outCompletedWorkMask = product->skySubmittedWorkMask;
	product->skySubmittedWorkMask = 0u;
	return qtrue;
}

static qboolean CollectSkyRadianceCompletion( ralOpenGlProduct_t *product,
		qboolean wait, uint32_t *outCompletedSliceMask,
		uint64_t *outCompletedRevision )
{
	glEnum_t result;
	if ( !product || !outCompletedSliceMask || !outCompletedRevision ) return qfalse;
	*outCompletedSliceMask = 0u;
	*outCompletedRevision = 0u;
	if ( !product->skyRadianceFence ) return qtrue;
	result = product->gl.ClientWaitSync( product->skyRadianceFence,
		wait ? GL_SYNC_FLUSH_COMMANDS_BIT_VALUE : 0u,
		wait ? ~(glUInt64_t)0u : 0u );
	if ( result == GL_WAIT_FAILED_VALUE ) return qfalse;
	if ( result == GL_TIMEOUT_EXPIRED_VALUE ) return qtrue;
	if ( result != GL_ALREADY_SIGNALED_VALUE &&
		 result != GL_CONDITION_SATISFIED_VALUE ) return qfalse;
	product->gl.DeleteSync( product->skyRadianceFence );
	product->skyRadianceFence = NULL;
	{
		/* GLuint64 is unsigned long long; uint64_t is unsigned long on LP64 Linux, so read through the GL type. */
		glUInt64_t duration = 0u;
		product->gl.GetQueryObjectui64v( product->skyTimingQueries[1], GL_QUERY_RESULT_VALUE, &duration );
		product->skyTimingDurationNanoseconds[1] = (uint64_t)duration;
	}
	product->skyTimingState[1] = product->gl.GetError() == GL_NO_ERROR_VALUE
			&& product->skyTimingDurationNanoseconds[1] > 0u
		? RAL_SKY_GPU_TIMING_VALID : RAL_SKY_GPU_TIMING_FAILED;
	*outCompletedSliceMask = product->skyRadianceSubmittedSliceMask;
	*outCompletedRevision = product->skyRadianceSubmittedRevision;
	product->skyRadianceSubmittedSliceMask = 0u;
	product->skyRadianceSubmittedRevision = 0u;
	return qtrue;
}

void RalOpenGl_ProductDestroy( ralOpenGlProduct_t *product )
{
	uint32_t ignoredCompletion = 0u;
	uint64_t ignoredRevision = 0u;
	if ( !product )
		return;
	RalOpenGl_AaDestroy( product->aa );
	product->aa = NULL;
	if ( product->skyCaptureFramebuffer ) product->gl.DeleteFramebuffers(1,&product->skyCaptureFramebuffer);
	if ( product->skyCaptureVao ) product->gl.DeleteVertexArrays(1,&product->skyCaptureVao);
	product->gl.DeleteBuffers(3,product->skyCaptureBuffers);
	if ( product->hdrFramebuffer ) product->gl.DeleteFramebuffers( 1, &product->hdrFramebuffer );
	product->gl.DeleteTextures( 2, product->hdrTextures );
	if ( product->smaaToneFramebuffer ) product->gl.DeleteFramebuffers( 1, &product->smaaToneFramebuffer );
	if ( product->smaaToneTexture ) product->gl.DeleteTextures( 1, &product->smaaToneTexture );
	product->gl.DeleteBuffers( 5, product->hdrBuffers );
	if ( product->hdrVao ) product->gl.DeleteVertexArrays( 1, &product->hdrVao );
	for ( unsigned i = 0; i < 3; ++i )
		if ( product->hdrPrograms[i] ) product->gl.DeleteProgram( product->hdrPrograms[i] );
	RalOpenGl_ProductResetLensVisibility( product );
	if ( product->lensProgram ) product->gl.DeleteProgram( product->lensProgram );
	if ( product->skyFence &&
		!CollectSkyWorkCompletion( product, qtrue, &ignoredCompletion ) ) {
		product->gl.DeleteSync( product->skyFence );
		product->skyFence = NULL;
		product->skySubmittedWorkMask = 0u;
	}
	if ( product->skyRadianceFence && !CollectSkyRadianceCompletion( product,
			qtrue, &ignoredCompletion, &ignoredRevision ) ) {
		product->gl.DeleteSync( product->skyRadianceFence );
		product->skyRadianceFence = NULL;
	}
	for ( uint32_t i = 0u; i < 5u; ++i )
		if ( product->atmospherePrograms[i] )
			product->gl.DeleteProgram( product->atmospherePrograms[i] );
	for ( uint32_t i = 0u; i < RAL_OPENGL_SKY_PHYSICAL_PROGRAM_COUNT; ++i )
		if ( product->skyPhysicalPrograms[i] )
			product->gl.DeleteProgram( product->skyPhysicalPrograms[i] );
	for ( uint32_t i = 0u; i < RAL_OPENGL_SKY_RADIANCE_PROGRAM_COUNT; ++i )
		if ( product->skyRadiancePrograms[i] )
			product->gl.DeleteProgram( product->skyRadiancePrograms[i] );
	for ( uint32_t i = 0u; i < 2u; ++i )
		if ( product->weatherPrograms[i] )
			product->gl.DeleteProgram( product->weatherPrograms[i] );
	if ( product->weatherBuffers[0] )
		product->gl.DeleteBuffers( 3, product->weatherBuffers );
	if ( product->weatherHeightgrid )
		product->gl.DeleteTextures( 1, &product->weatherHeightgrid );
	if ( product->skyMaterialProgram ) product->gl.DeleteProgram( product->skyMaterialProgram );
	if ( product->atmosphereBuffers[0] || product->atmosphereBuffers[RAL_OPENGL_ATMOSPHERE_FALLBACK] )
		product->gl.DeleteBuffers( WIRED_OPENGL_ATMOSPHERE_BUFFER_COUNT, product->atmosphereBuffers );
	if ( product->program )
		product->gl.DeleteProgram( product->program );
	if ( product->skyTimingQueries[0] )
		product->gl.DeleteQueries( 2, product->skyTimingQueries );
	if ( product->skyTextures[0] )
		product->gl.DeleteTextures( RAL_OPENGL_SKY_TEXTURE_COUNT,
			product->skyTextures );
	if ( product->skyShBuffers[0] )
		product->gl.DeleteBuffers( RAL_OPENGL_SKY_SH_BUFFER_COUNT,
			product->skyShBuffers );
	if ( product->skyPhysicalTextures[0] )
		product->gl.DeleteTextures( RAL_OPENGL_SKY_PHYSICAL_TEXTURE_COUNT,
			product->skyPhysicalTextures );
	if ( product->skyPhysicalParamBuffer )
		product->gl.DeleteBuffers( 1, &product->skyPhysicalParamBuffer );
	if ( product->skyRadianceParamBuffers[0] )
		product->gl.DeleteBuffers( RAL_OPENGL_SKY_RADIANCE_PARAM_BUFFER_COUNT,
			product->skyRadianceParamBuffers );
	for ( uint32_t i = 0u; i < product->textureResourceCount; ++i )
		if ( product->materials[i].texture )
			product->gl.DeleteTextures( 1, &product->materials[i].texture );
	memset( product, 0, sizeof( *product ) );
	free( product );
}

qboolean RalOpenGl_ProductResetLevelResources( ralOpenGlProduct_t *product )
{
	if ( !product ) return qfalse;
	product->exposureHistoryValid = qfalse;
	RalOpenGl_ProductUnloadHeightgrid( product, product->weatherHeightgridWorldIndex );
	RenderEffectBeamPool_Reset( &product->effectBeamPool );
	RenderEffectDecalPool_Reset( &product->effectDecalPool );
	RalOpenGl_ProductResetLensVisibility( product );
	return qtrue;
}

qboolean RalOpenGl_ProductResetRegistrationAssets( ralOpenGlProduct_t *product,
		renderSubmissionState_t *frontend )
{
	if ( !product || !frontend || !frontend->initialized || frontend->frameOpen
			|| RenderSubmission_ResidentWorldCount( frontend )
			|| !RalOpenGl_CoreMatchesReceipt( product->core, &product->coreReceipt ) )
		return qfalse;
	if ( !RalOpenGl_ProductUnloadSkyRadianceResources( product ) ) return qfalse;
	RalOpenGl_ProductUnloadHeightgrid( product, product->weatherHeightgridWorldIndex );
	/* GL retains storage referenced by queued commands. Delete native names
	 * before retiring CPU snapshots; do not add a frame-loop finish/wait. */
	for ( uint32_t i = 0; i < product->textureResourceCount; ++i )
		if ( product->materials[i].texture )
			product->gl.DeleteTextures( 1, &product->materials[i].texture );
	memset( product->materials, 0, sizeof( product->materials ) );
	memset( product->materialTextureNames, 0, sizeof( product->materialTextureNames ) );
	memset( product->cookedMaterialTextureNames, 0, sizeof( product->cookedMaterialTextureNames ) );
	memset( product->cookedMaterialReady, 0, sizeof( product->cookedMaterialReady ) );
	product->materialCount = product->textureResourceCount = 0u;
	product->cookedMaterialRuntimeGeneration = 0u;
	return RenderSubmission_ResetRegistrationAssets( frontend );
}

void RalOpenGl_ProductResetLensVisibility( ralOpenGlProduct_t *product ) {
	if ( !product ) return;
	for ( uint32_t i = 0; i < 3u; ++i ) {
		productLensSlot_t *slot = &product->lensSlots[i];
		if ( slot->fence ) product->gl.DeleteSync( slot->fence );
		if ( slot->records ) product->gl.DeleteBuffers( 1, &slot->records );
		if ( slot->params ) product->gl.DeleteBuffers( 1, &slot->params );
	}
	/* GL retains objects referenced by queued commands; deletion cancels host
	 * publication without waiting or recycling in-flight storage ourselves. */
	if ( product->lensDepth ) product->gl.DeleteTextures( 1, &product->lensDepth );
	product->lensDepth = 0u;
	memset( product->lensSlots, 0, sizeof( product->lensSlots ) );
	memset( product->lensVisibility, 0, sizeof( product->lensVisibility ) );
	memset( product->lensActive, 0, sizeof( product->lensActive ) );
	memset( product->lensPublishedActive, 0, sizeof( product->lensPublishedActive ) );
	product->lensPublishedFrame = 0u;
	product->lensHasTimeline = qfalse;
}

qboolean RalOpenGl_ProductLensVisibility( const ralOpenGlProduct_t *product, int id, float *visibility ) {
	if ( !product || !visibility || id < 0 || id >= LENS_MAX_CGSOURCES
			|| !RalOpenGl_CoreMatchesReceipt( product->core, &product->coreReceipt )
			|| !product->lensPublishedFrame || !product->lensActive[id]
			|| !product->lensPublishedActive[id] ) return qfalse;
	*visibility = product->lensVisibility[id];
	return qtrue;
}

static qboolean EncodeLensVisibility( ralOpenGlProduct_t *product,
		const renderSubmissionState_t *frontend, const renderWorldSnapshot_t *view,
		ralOpenGlProductFrameReceipt_t *receipt ) {
	renderLensSourceSnapshot_t snapshot;
	const lensSourceDesc_t *sources;
	productLensSource_t records[LENS_MAX_CGSOURCES];
	productLensSlot_t *freeSlot = NULL;
	if ( !RenderSubmission_LensSourceSnapshot( frontend, &snapshot, &sources ) ) return qfalse;
	if ( !view->ready || ( view->rdflags & RDF_NOWORLDMODEL ) ) {
		RalOpenGl_ProductResetLensVisibility( product );
		return snapshot.count == 0u;
	}
	if ( product->lensHasTimeline && view->timeMs < product->lensTimeline )
		RalOpenGl_ProductResetLensVisibility( product );
	product->lensHasTimeline = qtrue;
	product->lensTimeline = view->timeMs;
	memset( product->lensActive, 0, sizeof( product->lensActive ) );
	for ( uint32_t i = 0; i < snapshot.count; ++i ) {
		if ( sources[i].id < 0 || sources[i].id >= LENS_MAX_CGSOURCES ) return qfalse;
		product->lensActive[sources[i].id] = qtrue;
	}
	for ( uint32_t i = 0; i < 3u; ++i ) {
		productLensSlot_t *slot = &product->lensSlots[i];
		if ( slot->fence ) {
			const glEnum_t result = product->gl.ClientWaitSync( slot->fence, 0u, 0u );
			if ( result == GL_TIMEOUT_EXPIRED_VALUE ) continue;
			if ( result != GL_ALREADY_SIGNALED_VALUE && result != GL_CONDITION_SATISFIED_VALUE ) goto failure;
			product->gl.GetNamedBufferSubData( slot->records, 0,
				slot->count * sizeof( records[0] ), records );
			if ( product->gl.GetError() != GL_NO_ERROR_VALUE ) goto failure;
			for ( uint32_t j = 0; j < slot->count; ++j )
				if ( !isfinite( records[j].colorVis[3] ) || records[j].colorVis[3] < 0.0f
						|| records[j].colorVis[3] > 1.0f ) goto failure;
			if ( slot->frame > product->lensPublishedFrame ) {
				memset( product->lensVisibility, 0, sizeof( product->lensVisibility ) );
				memset( product->lensPublishedActive, 0, sizeof( product->lensPublishedActive ) );
				for ( uint32_t j = 0; j < slot->count; ++j ) {
					product->lensVisibility[slot->ids[j]] = records[j].colorVis[3];
					product->lensPublishedActive[slot->ids[j]] = qtrue;
				}
				product->lensPublishedFrame = slot->frame;
			}
			product->gl.DeleteSync( slot->fence ); slot->fence = NULL;
			receipt->lensCompletedCount++;
		}
		if ( !freeSlot ) freeSlot = slot;
	}
	receipt->lensSourceCount = snapshot.count;
	if ( !snapshot.count ) return qtrue;
	if ( !freeSlot ) { receipt->lensDeferredCount = 1u; return qtrue; }
	if ( !product->lensProgram ) {
		glName_t shader = Compile( &product->gl, GL_COMPUTE_SHADER_VALUE,
			ralOpenGlProductLensOcclusionSource, ralOpenGlProductLensOcclusionByteCount );
		if ( !shader ) goto failure;
		product->lensProgram = LinkSingleStage( &product->gl, shader );
		product->gl.DeleteShader( shader );
		if ( !product->lensProgram ) goto failure;
	}
	if ( !product->lensDepth ) {
		product->gl.CreateTextures( GL_TEXTURE_2D_VALUE, 1, &product->lensDepth );
		if ( !product->lensDepth ) goto failure;
		product->gl.TextureStorage2D( product->lensDepth, 1, 0x8cacu /* DEPTH_COMPONENT32F */,
			product->outputWidth, product->outputHeight );
		product->gl.TextureParameteri( product->lensDepth, GL_TEXTURE_MIN_FILTER_VALUE, 0x2600 /* NEAREST */ );
		product->gl.TextureParameteri( product->lensDepth, GL_TEXTURE_MAG_FILTER_VALUE, 0x2600 );
		product->gl.TextureParameteri( product->lensDepth, GL_TEXTURE_WRAP_S_VALUE, GL_CLAMP_TO_EDGE_VALUE );
		product->gl.TextureParameteri( product->lensDepth, GL_TEXTURE_WRAP_T_VALUE, GL_CLAMP_TO_EDGE_VALUE );
	}
	if ( !freeSlot->records ) {
		product->gl.CreateBuffers( 1, &freeSlot->records );
		product->gl.CreateBuffers( 1, &freeSlot->params );
		if ( !freeSlot->records || !freeSlot->params ) goto failure;
		product->gl.NamedBufferStorage( freeSlot->records, sizeof( records ), NULL, GL_DYNAMIC_STORAGE_BIT_VALUE );
		product->gl.NamedBufferStorage( freeSlot->params, sizeof( productLensPush_t ), NULL, GL_DYNAMIC_STORAGE_BIT_VALUE );
	}
	memset( records, 0, sizeof( records ) );
	for ( uint32_t i = 0; i < snapshot.count; ++i ) {
		float radiusScale;
		freeSlot->ids[i] = sources[i].id;
		if ( !RalOpenGl_WorldProjectLens( view, sources[i].origin,
				product->outputHeight, records[i].screenPosDepth, &radiusScale ) ) {
			records[i].screenPosDepth[0] = -1.0f;
			continue;
		}
		records[i].screenPosDepth[3] = fminf( 16.0f, fmaxf( 1.0f, sources[i].radius * radiusScale ) );
		records[i].colorVis[0] = receipt->atmosphere.skyEnvironment.opticsVisibility;
	}
	productLensPush_t push = { { (float)product->outputWidth, (float)product->outputHeight },
		(int32_t)snapshot.count, 1.5f, { -1.0f, 0.0015f, 1.0f, 0.0f } };
	/* SDL GLimp requests zero multisamples. Copy the world depth after native
	 * drawing, before swap; UI never writes this depth. No CPU depth read. */
	product->gl.CopyTextureSubImage2D( product->lensDepth, 0, 0, 0, 0, 0,
		product->outputWidth, product->outputHeight );
	product->gl.NamedBufferSubData( freeSlot->records, 0, snapshot.count * sizeof( records[0] ), records );
	product->gl.NamedBufferSubData( freeSlot->params, 0, sizeof( push ), &push );
	product->gl.UseProgram( product->lensProgram );
	product->gl.BindTextureUnit( 0u, product->lensDepth );
	product->gl.BindBufferBase( GL_SHADER_STORAGE_BUFFER_VALUE, 0u, freeSlot->records );
	product->gl.BindBufferBase( GL_UNIFORM_BUFFER_VALUE, 3u, freeSlot->params );
	product->gl.DispatchCompute( ( snapshot.count + 63u ) / 64u, 1u, 1u );
	product->gl.MemoryBarrier( 0x00000200u /* BUFFER_UPDATE: GetNamedBufferSubData */ );
	freeSlot->fence = product->gl.FenceSync( GL_SYNC_GPU_COMMANDS_COMPLETE_VALUE, 0u );
	if ( !freeSlot->fence || product->gl.GetError() != GL_NO_ERROR_VALUE ) goto failure;
	freeSlot->frame = receipt->frameGeneration;
	freeSlot->count = snapshot.count;
	receipt->lensDispatchCount = 1u;
	return qtrue;
failure:
	RalOpenGl_ProductResetLensVisibility( product );
	return qfalse;
}


typedef struct {
	ralOpenGlProduct_t *product;
	const renderSubmissionState_t *frontend;
	const renderWorldSnapshot_t *view;
	ralOpenGlProductFrameReceipt_t *receipt;
	glInt_t drawFramebuffer, readFramebuffer;
	glBool_t outputSrgbEnabled;
	uint64_t worldDigest;
	qboolean sceneRendered, completed, metered;
	const ralOpenGlAaTaaFrame_t *taa;   /* TAA planned for this frame (r_aa 1) */
	qboolean smaa;                      /* SMAA for this frame (r_aa 2) */
} productSceneComplete_t;

static qboolean EnsureSmaaToneTarget( ralOpenGlProduct_t *product ) {
	productDispatch_t *gl = &product->gl;
	if ( product->smaaToneTexture && product->smaaToneWidth == product->outputWidth
			&& product->smaaToneHeight == product->outputHeight ) return qtrue;
	if ( product->smaaToneFramebuffer ) gl->DeleteFramebuffers( 1, &product->smaaToneFramebuffer );
	if ( product->smaaToneTexture ) gl->DeleteTextures( 1, &product->smaaToneTexture );
	product->smaaToneFramebuffer = product->smaaToneTexture = 0u;
	product->smaaToneWidth = product->smaaToneHeight = 0u;
	gl->CreateTextures( GL_TEXTURE_2D_VALUE, 1, &product->smaaToneTexture );
	gl->CreateFramebuffers( 1, &product->smaaToneFramebuffer );
	if ( !product->smaaToneTexture || !product->smaaToneFramebuffer ) return qfalse;
	gl->TextureStorage2D( product->smaaToneTexture, 1, GL_RGBA16F_VALUE,
		(glSize_t)product->outputWidth, (glSize_t)product->outputHeight );
	gl->TextureParameteri( product->smaaToneTexture, GL_TEXTURE_MIN_FILTER_VALUE, GL_LINEAR_VALUE );
	gl->TextureParameteri( product->smaaToneTexture, GL_TEXTURE_MAG_FILTER_VALUE, GL_LINEAR_VALUE );
	gl->TextureParameteri( product->smaaToneTexture, GL_TEXTURE_WRAP_S_VALUE, GL_CLAMP_TO_EDGE_VALUE );
	gl->TextureParameteri( product->smaaToneTexture, GL_TEXTURE_WRAP_T_VALUE, GL_CLAMP_TO_EDGE_VALUE );
	gl->NamedFramebufferTexture( product->smaaToneFramebuffer, GL_COLOR_ATTACHMENT0_VALUE, product->smaaToneTexture, 0 );
	if ( gl->CheckNamedFramebufferStatus( product->smaaToneFramebuffer, GL_FRAMEBUFFER_VALUE )
			!= GL_FRAMEBUFFER_COMPLETE_VALUE || gl->GetError() != GL_NO_ERROR_VALUE ) return qfalse;
	product->smaaToneWidth = product->outputWidth; product->smaaToneHeight = product->outputHeight;
	return qtrue;
}

static void RestoreOutputFramebuffer( productSceneComplete_t *frame ) {
	productDispatch_t *gl = &frame->product->gl;
	gl->BindFramebuffer( GL_DRAW_FRAMEBUFFER_VALUE, (glName_t)frame->drawFramebuffer );
	gl->BindFramebuffer( GL_READ_FRAMEBUFFER_VALUE, (glName_t)frame->readFramebuffer );
}

static qboolean CompleteHdrScene( void *context ) {
	productSceneComplete_t *frame = context;
	ralOpenGlProduct_t *product = frame->product;
	productDispatch_t *gl = &product->gl;
	glName_t sceneSource = product->hdrTextures[0];
	const ralExposureSettings_t *s = &product->exposureSettings;
	ralExposureBlock_t block = {0};
	const uint32_t next = product->exposureParity ^ 1u;
	const glName_t accumulator = product->hdrBuffers[1u + next];
	if ( frame->view->rdflags & ( RDF_CAMERA_CUT | RDF_HYPERSPACE ) )
		product->exposureHistoryValid = qfalse;
	const ralExposureHistoryStep_t step = Ral_ExposureHistoryStep(
		product->exposureHistoryValid, product->exposureTimeline, frame->view->timeMs,
		product->exposureWorld != frame->worldDigest );
	const qboolean reset = step.reset ? qtrue : qfalse;
	/* Read scene depth while the HDR framebuffer is still bound, never UI depth. */
	if ( !EncodeLensVisibility( product, frame->frontend, frame->view, frame->receipt ) )
		return qfalse;
	/* TAA on the HDR scene, before exposure metering and tone mapping: both read the anti-aliased colour. SMAA runs after the tone
	 * map (render_antialiasing.h). The scene pass is over, so the HDR framebuffer draws to attachment 0 only again. */
	if ( frame->taa ) {
		uint32_t resolved = 0u;
		if ( !RalOpenGl_AaAttachCoverage( product->aa, product->hdrFramebuffer, 0u )
				|| !RalOpenGl_AaTaa( product->aa, product->hdrTextures[0], product->hdrTextures[1], frame->taa, &resolved ) )
			return qfalse;
		frame->receipt->aaDispatchCount = 2u;
		sceneSource = (glName_t)resolved;
	}
	block.exposure_bias = product->displayVisibility.exposureScale;
	block.key = s->key; block.pctLow = s->pctLow; block.pctHigh = s->pctHigh;
	block.rateUp = s->rateUp; block.rateDown = s->rateDown;
	block.minExp = s->minExp; block.maxExp = s->maxExp;
	block.autoEnabled = s->enabled && frame->sceneRendered;
	block.brightness = product->displayVisibility.exposureScale;
	block.shadowExponent = product->displayVisibility.shadowExponent;
	block.shadowPivot = product->displayVisibility.shadowPivot;
	/* Ordered update follows prior GPU use; no exposure value crosses to CPU. */
	gl->MemoryBarrier( GL_BUFFER_UPDATE_BARRIER_BIT_VALUE );
	gl->NamedBufferSubData( product->hdrBuffers[3], 0, sizeof(block), &block );
	if ( s->enabled && frame->sceneRendered ) {
		uint32_t dims[4] = { product->outputWidth, product->outputHeight, 0u, 0u };
		uint32_t zero = 0u;
		float reduce[4] = { step.deltaSeconds, 0, 0, 0 };
		if ( reset ) {
			const float identity = 1.0f;
			gl->NamedBufferSubData( accumulator, 0, sizeof(identity), &identity );
		} else {
			gl->CopyNamedBufferSubData( product->hdrBuffers[1u + product->exposureParity],
				accumulator, 0, 0, sizeof(float) );
		}
		gl->ClearNamedBufferData( product->hdrBuffers[0], GL_R32UI_VALUE,
			GL_RED_INTEGER_VALUE, GL_UNSIGNED_INT_VALUE, &zero );
		gl->NamedBufferSubData( product->hdrBuffers[4], 0, sizeof(dims), dims );
		gl->UseProgram( product->hdrPrograms[0] );
		gl->BindTextureUnit( 0, sceneSource );
		gl->BindBufferBase( GL_SHADER_STORAGE_BUFFER_VALUE, 0, product->hdrBuffers[0] );
		gl->BindBufferBase( GL_UNIFORM_BUFFER_VALUE, 3, product->hdrBuffers[4] );
		gl->DispatchCompute( (product->outputWidth + 15u) / 16u,
			(product->outputHeight + 15u) / 16u, 1u );
		gl->MemoryBarrier( GL_SHADER_STORAGE_BARRIER_BIT_VALUE );
		gl->NamedBufferSubData( product->hdrBuffers[4], 0, sizeof(reduce), reduce );
		gl->UseProgram( product->hdrPrograms[1] );
		gl->BindBufferBase( GL_SHADER_STORAGE_BUFFER_VALUE, 1, accumulator );
		gl->BindBufferBase( GL_SHADER_STORAGE_BUFFER_VALUE, 2, product->hdrBuffers[3] );
		gl->DispatchCompute( 1, 1, 1 );
		gl->MemoryBarrier( GL_UNIFORM_BARRIER_BIT_VALUE | GL_BUFFER_UPDATE_BARRIER_BIT_VALUE );
		frame->metered = qtrue;
		frame->receipt->exposureDispatchCount = 2u;
		frame->receipt->exposureHistoryReset = reset;
	}
	if ( frame->smaa ) {
		/* r_aa 2: tone-map into the RGBA16F target (linear, no sRGB encode); SMAA resolves it into the output attachment. */
		if ( !EnsureSmaaToneTarget( product ) ) return qfalse;
		gl->BindFramebuffer( GL_DRAW_FRAMEBUFFER_VALUE, product->smaaToneFramebuffer );
		gl->Viewport( 0, 0, (glSize_t)product->outputWidth, (glSize_t)product->outputHeight );
		gl->Disable( GL_FRAMEBUFFER_SRGB_VALUE );
		gl->UseProgram( product->hdrPrograms[2] );
		gl->BindTextureUnit( 0, sceneSource );
		gl->BindBufferBase( GL_UNIFORM_BUFFER_VALUE, 0, product->hdrBuffers[3] );
		gl->Disable( GL_BLEND_VALUE ); gl->Disable( GL_DEPTH_TEST_VALUE );
		gl->Disable( GL_CULL_FACE_VALUE ); gl->DepthMask( 0 );
		gl->BindVertexArray( product->hdrVao );
		gl->DrawArrays( GL_TRIANGLE_STRIP_VALUE, 0, 4 );
		RestoreOutputFramebuffer( frame );
		gl->Enable( GL_FRAMEBUFFER_SRGB_VALUE );
		if ( !RalOpenGl_AaSmaaInto( product->aa, product->smaaToneTexture, product->outputWidth, product->outputHeight,
				product->smaaQuality, (uint32_t)frame->drawFramebuffer ) ) return qfalse;
		RestoreOutputFramebuffer( frame );
		frame->receipt->aaDrawCount = 3u;
		if ( gl->GetError() != GL_NO_ERROR_VALUE ) return qfalse;
		frame->completed = qtrue;
		frame->receipt->postprocessDrawCount = 1u;
		return qtrue;
	} else {
		RestoreOutputFramebuffer( frame );
		/* Canonical tonemap and UI return linear RGB. Encode once at the sRGB
		 * attachment, with UI blending performed in linear space by OpenGL. */
		gl->Enable( GL_FRAMEBUFFER_SRGB_VALUE );
		gl->UseProgram( product->hdrPrograms[2] );
		gl->BindTextureUnit( 0, sceneSource );
		gl->BindBufferBase( GL_UNIFORM_BUFFER_VALUE, 0, product->hdrBuffers[3] );
	}
	gl->Disable( GL_BLEND_VALUE ); gl->Disable( GL_DEPTH_TEST_VALUE );
	gl->Disable( GL_CULL_FACE_VALUE ); gl->DepthMask( 0 );
	gl->BindVertexArray( product->hdrVao );
	gl->DrawArrays( GL_TRIANGLE_STRIP_VALUE, 0, 4 );
	if ( gl->GetError() != GL_NO_ERROR_VALUE ) return qfalse;
	frame->completed = qtrue;
	frame->receipt->postprocessDrawCount = 1u;
	return qtrue;
}


static qboolean EnsureSkyRadianceResources( ralOpenGlProduct_t *product,
		uint64_t worldLifecycleGeneration )
{
	glName_t textures[RAL_OPENGL_SKY_TEXTURE_COUNT] = { 0u };
	glName_t shBuffers[RAL_OPENGL_SKY_SH_BUFFER_COUNT] = { 0u };
	glName_t physicalTextures[RAL_OPENGL_SKY_PHYSICAL_TEXTURE_COUNT] = { 0u };
	glName_t physicalParamBuffer = 0u;
	glName_t radianceParamBuffers[RAL_OPENGL_SKY_RADIANCE_PARAM_BUFFER_COUNT] = { 0u };
	uint32_t ignoredCompletion = 0u;
	uint64_t ignoredRevision = 0u;
	if ( !product || !worldLifecycleGeneration
			|| worldLifecycleGeneration == UINT64_MAX ) return qfalse;
	if ( product->skyResourcesReady
			&& product->skyResourceWorldLifecycleGeneration
				== worldLifecycleGeneration ) return qtrue;
	if ( product->skyFence &&
		!CollectSkyWorkCompletion( product, qtrue, &ignoredCompletion ) ) return qfalse;
	if ( product->skyRadianceFence &&
		!CollectSkyRadianceCompletion( product, qtrue, &ignoredCompletion,
			&ignoredRevision ) ) return qfalse;

	product->gl.CreateTextures( GL_TEXTURE_2D_ARRAY_VALUE,
		RAL_OPENGL_SKY_TEXTURE_COUNT, textures );
	for ( uint32_t i = 0u; i < RAL_OPENGL_SKY_TEXTURE_COUNT; ++i )
		if ( !textures[i] ) goto fail;
	product->gl.TextureStorage3D( textures[0], 1, GL_RGBA16F_VALUE,
		RAL_OPENGL_SKY_SOURCE_SIZE, RAL_OPENGL_SKY_SOURCE_SIZE,
		RAL_SKY_RADIANCE_CAPTURE_FACE_COUNT );
	for ( uint32_t i = 1u; i < 3u; ++i )
		product->gl.TextureStorage3D( textures[i], 1, GL_RGBA16F_VALUE,
			RAL_OPENGL_SKY_IRRADIANCE_SIZE, RAL_OPENGL_SKY_IRRADIANCE_SIZE,
			RAL_SKY_RADIANCE_CAPTURE_FACE_COUNT );
	for ( uint32_t i = 3u; i < RAL_OPENGL_SKY_TEXTURE_COUNT; ++i )
		product->gl.TextureStorage3D( textures[i], RAL_OPENGL_SKY_RADIANCE_MIPS,
			GL_RGBA16F_VALUE, WIRED_SKY_SPECULAR_SIZE,
			WIRED_SKY_SPECULAR_SIZE,
			RAL_SKY_RADIANCE_CAPTURE_FACE_COUNT );
	product->gl.CreateBuffers( RAL_OPENGL_SKY_SH_BUFFER_COUNT, shBuffers );
	for ( uint32_t i = 0u; i < RAL_OPENGL_SKY_SH_BUFFER_COUNT; ++i ) {
		if ( !shBuffers[i] ) goto fail;
		product->gl.NamedBufferStorage( shBuffers[i],
			RAL_SKY_RADIANCE_AMBIENT_SH_COEFFICIENT_COUNT * 4u * sizeof( float ),
			NULL, 0u );
	}
	product->gl.CreateTextures( GL_TEXTURE_2D_VALUE,
		RAL_OPENGL_SKY_PHYSICAL_TEXTURE_COUNT, physicalTextures );
	for ( uint32_t i = 0u; i < RAL_OPENGL_SKY_PHYSICAL_TEXTURE_COUNT; ++i )
		if ( !physicalTextures[i] ) goto fail;
	product->gl.TextureStorage2D( physicalTextures[RAL_OPENGL_SKY_TRANSMITTANCE],
		1, GL_RGBA16F_VALUE, RAL_OPENGL_SKY_TRANSMITTANCE_WIDTH,
		RAL_OPENGL_SKY_TRANSMITTANCE_HEIGHT );
	product->gl.TextureStorage2D( physicalTextures[RAL_OPENGL_SKY_MULTISCATTER],
		1, GL_RGBA16F_VALUE, RAL_OPENGL_SKY_MULTISCATTER_SIZE,
		RAL_OPENGL_SKY_MULTISCATTER_SIZE );
	product->gl.TextureStorage2D( physicalTextures[RAL_OPENGL_SKY_VIEW],
		1, GL_RGBA16F_VALUE, RAL_OPENGL_SKY_VIEW_WIDTH,
		RAL_OPENGL_SKY_VIEW_HEIGHT );
	for ( uint32_t i = 0u; i < RAL_OPENGL_SKY_PHYSICAL_TEXTURE_COUNT; ++i ) {
		product->gl.TextureParameteri( physicalTextures[i],
			GL_TEXTURE_MIN_FILTER_VALUE, GL_LINEAR_VALUE );
		product->gl.TextureParameteri( physicalTextures[i],
			GL_TEXTURE_MAG_FILTER_VALUE, GL_LINEAR_VALUE );
		product->gl.TextureParameteri( physicalTextures[i],
			GL_TEXTURE_WRAP_S_VALUE, GL_CLAMP_TO_EDGE_VALUE );
		product->gl.TextureParameteri( physicalTextures[i],
			GL_TEXTURE_WRAP_T_VALUE, GL_CLAMP_TO_EDGE_VALUE );
	}
	product->gl.CreateBuffers( 1, &physicalParamBuffer );
	if ( !physicalParamBuffer ) goto fail;
	product->gl.NamedBufferStorage( physicalParamBuffer,
		sizeof( productSkyPhysicalParams_t ), NULL,
		GL_DYNAMIC_STORAGE_BIT_VALUE );
	if ( product->gl.GetError() != GL_NO_ERROR_VALUE ) goto fail;
	product->gl.CreateBuffers( RAL_OPENGL_SKY_RADIANCE_PARAM_BUFFER_COUNT,
		radianceParamBuffers );
	if ( !radianceParamBuffers[RAL_OPENGL_SKY_SOURCE_PARAMS]
			|| !radianceParamBuffers[RAL_OPENGL_SKY_SLICE_PARAMS] ) goto fail;
	product->gl.NamedBufferStorage(
		radianceParamBuffers[RAL_OPENGL_SKY_SOURCE_PARAMS],
		sizeof( productSkySourceParams_t ), NULL, GL_DYNAMIC_STORAGE_BIT_VALUE );
	product->gl.NamedBufferStorage(
		radianceParamBuffers[RAL_OPENGL_SKY_SLICE_PARAMS],
		sizeof( productSkySliceParams_t ), NULL, GL_DYNAMIC_STORAGE_BIT_VALUE );
	if ( product->gl.GetError() != GL_NO_ERROR_VALUE ) goto fail;

	if ( product->skyTextures[0] )
		product->gl.DeleteTextures( RAL_OPENGL_SKY_TEXTURE_COUNT,
			product->skyTextures );
	if ( product->skyShBuffers[0] )
		product->gl.DeleteBuffers( RAL_OPENGL_SKY_SH_BUFFER_COUNT,
			product->skyShBuffers );
	if ( product->skyPhysicalTextures[0] )
		product->gl.DeleteTextures( RAL_OPENGL_SKY_PHYSICAL_TEXTURE_COUNT,
			product->skyPhysicalTextures );
	if ( product->skyPhysicalParamBuffer )
		product->gl.DeleteBuffers( 1, &product->skyPhysicalParamBuffer );
	if ( product->skyRadianceParamBuffers[0] )
		product->gl.DeleteBuffers( RAL_OPENGL_SKY_RADIANCE_PARAM_BUFFER_COUNT,
			product->skyRadianceParamBuffers );
	memcpy( product->skyTextures, textures, sizeof( textures ) );
	memcpy( product->skyShBuffers, shBuffers, sizeof( shBuffers ) );
	memcpy( product->skyPhysicalTextures, physicalTextures,
		sizeof( physicalTextures ) );
	product->skyPhysicalParamBuffer = physicalParamBuffer;
	memcpy( product->skyRadianceParamBuffers, radianceParamBuffers,
		sizeof( radianceParamBuffers ) );
	product->skyResourceWorldLifecycleGeneration = worldLifecycleGeneration;
	product->skyResourcesReady = qtrue;
	return qtrue;

fail:
	if ( textures[0] )
		product->gl.DeleteTextures( RAL_OPENGL_SKY_TEXTURE_COUNT, textures );
	if ( shBuffers[0] )
		product->gl.DeleteBuffers( RAL_OPENGL_SKY_SH_BUFFER_COUNT, shBuffers );
	if ( physicalTextures[0] )
		product->gl.DeleteTextures( RAL_OPENGL_SKY_PHYSICAL_TEXTURE_COUNT,
			physicalTextures );
	if ( physicalParamBuffer )
		product->gl.DeleteBuffers( 1, &physicalParamBuffer );
	if ( radianceParamBuffers[0] )
		product->gl.DeleteBuffers( RAL_OPENGL_SKY_RADIANCE_PARAM_BUFFER_COUNT,
			radianceParamBuffers );
	return qfalse;
}

qboolean RalOpenGl_ProductUnloadSkyRadianceResources(
		ralOpenGlProduct_t *product )
{
	uint32_t ignoredCompletion = 0u;
	uint64_t ignoredRevision = 0u;
	if ( !product ) return qfalse;
	if ( product->skyFence &&
		!CollectSkyWorkCompletion( product, qtrue, &ignoredCompletion ) )
		return qfalse;
	if ( product->skyRadianceFence &&
		!CollectSkyRadianceCompletion( product, qtrue, &ignoredCompletion,
			&ignoredRevision ) ) return qfalse;
	if ( product->skyTextures[0] )
		product->gl.DeleteTextures( RAL_OPENGL_SKY_TEXTURE_COUNT,
			product->skyTextures );
	if ( product->skyShBuffers[0] )
		product->gl.DeleteBuffers( RAL_OPENGL_SKY_SH_BUFFER_COUNT,
			product->skyShBuffers );
	if ( product->skyPhysicalTextures[0] )
		product->gl.DeleteTextures( RAL_OPENGL_SKY_PHYSICAL_TEXTURE_COUNT,
			product->skyPhysicalTextures );
	if ( product->skyPhysicalParamBuffer )
		product->gl.DeleteBuffers( 1, &product->skyPhysicalParamBuffer );
	if ( product->skyRadianceParamBuffers[0] )
		product->gl.DeleteBuffers( RAL_OPENGL_SKY_RADIANCE_PARAM_BUFFER_COUNT,
			product->skyRadianceParamBuffers );
	memset( product->skyTextures, 0, sizeof( product->skyTextures ) );
	memset( product->skyShBuffers, 0, sizeof( product->skyShBuffers ) );
	memset( product->skyPhysicalTextures, 0,
		sizeof( product->skyPhysicalTextures ) );
	product->skyPhysicalParamBuffer = 0u;
	memset( product->skyRadianceParamBuffers, 0,
		sizeof( product->skyRadianceParamBuffers ) );
	product->skySubmittedWorkMask = 0u;
	product->skyResourceWorldLifecycleGeneration = 0u;
	product->skyWorldLifecycleGeneration = 0u;
	product->skyResourcesReady = qfalse;
	product->skyRadianceSubmittedSliceMask = 0u;
	product->skyRadianceSubmittedRevision = 0u;
	product->skyIrradianceWriteStarted = qfalse;
	product->skyRadianceWriteStarted = qfalse;
	product->skyRadianceFrameGeneration = 0u;
	product->skyRadianceTimeline = 0.0f;
	product->skyMaterialUsesTime = qfalse;
	memset( &product->skyRadianceReceipt, 0,
		sizeof( product->skyRadianceReceipt ) );
	memset( &product->skyRadianceConsumer, 0,
		sizeof( product->skyRadianceConsumer ) );
	return Ral_SkyEnvironmentRuntimeInit( RAL_BACKEND_OPENGL,
		&product->skyRuntime ) && Ral_SkyRadianceInit( RAL_BACKEND_OPENGL,
		&product->skyRadiance );
}

/* Entries whose images share a texture cache key share one GL texture (as the Metal renderer shares the canonical image), so the
 * name is deleted only when no other entry still holds it. */
static void ReleaseMaterialTexture( ralOpenGlProduct_t *product, productMaterialEntry_t *entry ) {
	if ( !entry->texture ) return;
	for ( uint32_t i = 0u; i < RENDER_SUBMISSION_MAX_TEXTURE_RESOURCES; ++i )
		if ( &product->materials[i] != entry && product->materials[i].texture == entry->texture ) {
			entry->texture = 0u;
			return;
		}
	product->gl.DeleteTextures( 1, &entry->texture );
	entry->texture = 0u;
}

static qboolean SyncTexture( ralOpenGlProduct_t *product,
		const renderTextureSnapshot_t *texture, productMaterialEntry_t *entry ) {
		glName_t candidate = 0u;
		uint32_t mipLevels = texture->textureCacheKeyReady
			? texture->textureCacheKey.asset.targetMipLevels : 1u;
		ralTextureResourceReceipt_t resource;
		ralMaterialRuntimeResource_t runtimeResource;
		if ( texture->ready != qtrue || !texture->rgba8 || !texture->width
				|| !texture->height || texture->rowBytes != texture->width * 4u
				|| (uint64_t)texture->byteCount
					!= (uint64_t)texture->rowBytes * texture->height ) return qfalse;
		if ( entry->texture && entry->handle == texture->handle
				&& ( ( entry->generation == texture->generation
					&& entry->digest == texture->digest )
					|| ( entry->clampToEdge == texture->clampToEdge
						&& RenderSubmission_TextureResourceMatches( texture,
							&entry->runtimeResource ) ) ) ) {
			entry->generation = texture->generation;
			entry->digest = texture->digest;
			return qtrue;
		}
		/* The cache key covers the image content and creation options but not the wrap mode, which GL keeps on the texture: share
		 * only with an entry that wraps the same way. */
		if ( texture->textureCacheKeyReady ) {
			for ( uint32_t i = 0u; i < RENDER_SUBMISSION_MAX_TEXTURE_RESOURCES; ++i ) {
				const productMaterialEntry_t *shared = &product->materials[i];
				glName_t sharedTexture;
				ralTextureResourceReceipt_t sharedResource;
				ralMaterialRuntimeResource_t sharedRuntimeResource;
				if ( shared == entry || !shared->texture || shared->clampToEdge != texture->clampToEdge
						|| !RenderSubmission_TextureResourceMatches( texture, &shared->runtimeResource ) ) continue;
				sharedTexture = shared->texture;
				sharedResource = shared->resource;
				sharedRuntimeResource = shared->runtimeResource;
				ReleaseMaterialTexture( product, entry );
				memset( entry, 0, sizeof( *entry ) );
				entry->handle = texture->handle; entry->generation = texture->generation;
				entry->digest = texture->digest; entry->texture = sharedTexture;
				entry->clampToEdge = texture->clampToEdge;
				entry->resource = sharedResource; entry->runtimeResource = sharedRuntimeResource;
				return qtrue;
			}
		}
		if ( product->materialResourceGeneration == UINT64_MAX - 1u ) return qfalse;
		product->gl.CreateTextures( GL_TEXTURE_2D_VALUE, 1, &candidate );
		if ( !candidate ) return qfalse;
		product->gl.TextureStorage2D( candidate, (glSize_t)mipLevels,
			texture->srgb ? GL_SRGB8_ALPHA8_VALUE : GL_RGBA8_VALUE,
			(glSize_t)texture->width, (glSize_t)texture->height );
		product->gl.TextureSubImage2D( candidate, 0, 0, 0,
			(glSize_t)texture->width, (glSize_t)texture->height,
			GL_RGBA_VALUE, GL_UNSIGNED_BYTE_VALUE, texture->rgba8 );
		if ( mipLevels > 1u ) product->gl.GenerateTextureMipmap( candidate );
		product->gl.TextureParameteri( candidate, GL_TEXTURE_MIN_FILTER_VALUE,
			mipLevels > 1u ? GL_LINEAR_MIPMAP_LINEAR_VALUE : GL_LINEAR_VALUE );
		product->gl.TextureParameteri( candidate, GL_TEXTURE_MAG_FILTER_VALUE,
			GL_LINEAR_VALUE );
		product->gl.TextureParameteri( candidate, GL_TEXTURE_WRAP_S_VALUE,
			texture->clampToEdge ? GL_CLAMP_TO_EDGE_VALUE : GL_REPEAT_VALUE );
		product->gl.TextureParameteri( candidate, GL_TEXTURE_WRAP_T_VALUE,
			texture->clampToEdge ? GL_CLAMP_TO_EDGE_VALUE : GL_REPEAT_VALUE );
		if ( product->gl.GetError() != GL_NO_ERROR_VALUE ) {
			product->gl.DeleteTextures( 1, &candidate ); return qfalse;
		}
		memset( &resource, 0, sizeof( resource ) );
		resource.schemaVersion = RAL_TEXTURE_RESOURCE_RECEIPT_SCHEMA_VERSION;
		resource.backendType = RAL_BACKEND_OPENGL;
		resource.textureIdentity = (uintptr_t)candidate;
		resource.resourceGeneration = ++product->materialResourceGeneration;
		resource.type = RAL_TEXTURE_2D;
		resource.format = texture->srgb ? RAL_FORMAT_R8G8B8A8_SRGB
			: RAL_FORMAT_R8G8B8A8_UNORM;
		resource.usage = RAL_TEXTURE_USAGE_SAMPLED | RAL_TEXTURE_USAGE_TRANSFER_DST;
		resource.width = texture->width; resource.height = texture->height;
		resource.mipLevels = mipLevels; resource.arrayLayers = 1u;
		resource.imported = qfalse; resource.ready = qtrue;
		if ( !Ral_TextureResourceReceiptValid( &resource )
				|| ( texture->textureCacheKeyReady
					&& !Ral_MaterialRuntimeResourceBuild(
						&texture->textureCacheKey, &resource,
						&runtimeResource ) ) ) {
			product->gl.DeleteTextures( 1, &candidate ); return qfalse;
		}
		if ( !texture->textureCacheKeyReady )
			memset( &runtimeResource, 0, sizeof( runtimeResource ) );
		ReleaseMaterialTexture( product, entry );
		memset( entry, 0, sizeof( *entry ) );
		entry->handle = texture->handle; entry->generation = texture->generation;
		entry->digest = texture->digest; entry->texture = candidate;
		entry->clampToEdge = texture->clampToEdge;
		entry->resource = resource; entry->runtimeResource = runtimeResource;
	return qtrue;
}

static qboolean SyncMaterialTextures( ralOpenGlProduct_t *product,
		const renderSubmissionState_t *frontend ) {
	uint32_t nextCount;
	if ( !product || !frontend || frontend->materialCount
			> RENDER_SUBMISSION_MAX_MATERIALS
			|| frontend->stageTextureCount > RENDER_SUBMISSION_MAX_STAGE_TEXTURES )
		return qfalse;
	nextCount = frontend->materialCount + frontend->stageTextureCount;
	for ( uint32_t i = 0u; i < frontend->materialCount; ++i ) {
		renderTextureSnapshot_t texture;
		productMaterialEntry_t *entry = &product->materials[i];
		if ( !RenderSubmission_TextureSnapshot( frontend,
				frontend->materials[i].snapshot.handle, &texture )
				|| !SyncTexture( product, &texture, entry ) ) return qfalse;
		if ( product->cookedMaterialReady[i]
				&& product->cookedMaterialTextureNames[i] != entry->texture )
			return qfalse;
		product->materialTextureNames[i] = product->cookedMaterialReady[i]
			? product->cookedMaterialTextureNames[i] : entry->texture;
	}
	for ( uint32_t i = 0u; i < frontend->stageTextureCount; ++i ) {
		const uint32_t resourceIndex = frontend->materialCount + i;
		if ( !SyncTexture( product, &frontend->stageTextures[i].snapshot,
				&product->materials[resourceIndex] ) ) return qfalse;
	}
	for ( uint32_t i = nextCount; i < product->textureResourceCount; ++i ) {
		ReleaseMaterialTexture( product, &product->materials[i] );
		memset( &product->materials[i], 0, sizeof( product->materials[i] ) );
	}
	for ( uint32_t i = frontend->materialCount; i < product->materialCount; ++i ) {
		product->materialTextureNames[i] = 0u;
		product->cookedMaterialTextureNames[i] = 0u;
		product->cookedMaterialReady[i] = qfalse;
	}
	product->materialCount = frontend->materialCount;
	for (uint32_t i = frontend->materialCount; i < nextCount; ++i)
		product->materialTextureNames[i] = product->materials[i].texture;
	product->textureResourceCount = nextCount;
	return qtrue;
}

qboolean RalOpenGl_ProductPrepareMaterialResources(
		ralOpenGlProduct_t *product, const renderSubmissionState_t *frontend ) {
	return SyncMaterialTextures( product, frontend );
}

qboolean RalOpenGl_ProductResolveMaterialRuntimeResource( void *opaque,
		uint64_t cacheKeyHash, ralMaterialRuntimeResource_t *outResource ) {
	const ralOpenGlProduct_t *product = (const ralOpenGlProduct_t *)opaque;
	const ralMaterialRuntimeResource_t *match = NULL;
	if ( !product || !cacheKeyHash || !outResource ) return qfalse;
	for ( uint32_t i = 0u; i < product->textureResourceCount; ++i ) {
		const ralMaterialRuntimeResource_t *resource =
			&product->materials[i].runtimeResource;
		if ( resource->cacheKeyHash != cacheKeyHash ) continue;
		/* Entries sharing one texture (see ReleaseMaterialTexture) carry the exact same receipt. */
		if ( match && Ral_TextureResourceReceiptExact( &match->resource, &resource->resource ) ) continue;
		if ( match || !Ral_TextureResourceReceiptValid( &resource->resource ) )
			return qfalse;
		match = resource;
	}
	if ( !match ) return qfalse;
	*outResource = *match;
	return qtrue;
}

qboolean RalOpenGl_ProductSetMaterialRuntimeCatalog(
		ralOpenGlProduct_t *product,
		const renderMaterialRuntimeCatalog_t *catalog,
		renderSubmissionState_t *frontend ) {
	renderWorldSnapshot_t world;
	uint32_t candidateTextureNames[RENDER_SUBMISSION_MAX_MATERIALS];
	qboolean candidateReady[RENDER_SUBMISSION_MAX_MATERIALS];
	renderMaterialFactorPublicationEntry_t factorEntries[
		RENDER_SUBMISSION_MAX_MATERIALS];
	uint32_t factorEntryCount = 0u;
	if ( !product ) return qfalse;
	if ( !catalog ) {
		if ( frontend && !RenderSubmission_PublishMaterialFactors(
				frontend, 0u, NULL, 0u ) ) return qfalse;
		memset( product->cookedMaterialTextureNames, 0,
			sizeof( product->cookedMaterialTextureNames ) );
		memset( product->cookedMaterialReady, 0,
			sizeof( product->cookedMaterialReady ) );
		product->cookedMaterialRuntimeGeneration = 0u;
		return qtrue;
	}
	if ( !frontend || catalog->ready != qtrue
			|| !catalog->runtimeGeneration
			|| !SyncMaterialTextures( product, frontend )
			|| !RenderSubmission_ResidentWorldSnapshot( frontend,
				frontend->activeWorldIndex, catalog->runtimeGeneration, &world ) ) return qfalse;
	memset( candidateTextureNames, 0, sizeof( candidateTextureNames ) );
	memset( candidateReady, 0, sizeof( candidateReady ) );
	for ( uint32_t i = 0u; i < frontend->materialCount; ++i ) {
		const renderMaterialRecord_t *record = &frontend->materials[i];
		const ralMaterialRuntimePlan_t *plan;
		ralTextureResourceReceipt_t resource;
		plan = RenderMaterialRuntimeCatalog_Lookup( catalog, record->name );
		if ( !plan ) continue;
		factorEntries[factorEntryCount].material = record->snapshot.handle;
		factorEntries[factorEntryCount++].factors = plan->material.material.factors;
		/* Pure declarations (e.g. fog volumes) have no sampled image. Keep
		 * their factors, but do not invent texture readiness for draw batches. */
		if ( plan->bindingCount == 0u && plan->dependencyCount == 0u ) continue;
		if ( record->snapshot.textureCacheKeyReady != qtrue
				|| !RenderMaterialRuntimeCatalog_ResolveTexture( catalog,
					record->name, record->snapshot.textureCacheKeyHash, &resource )
				|| resource.backendType != RAL_BACKEND_OPENGL
				|| resource.textureIdentity
					!= (uintptr_t)product->materials[i].texture ) return qfalse;
		candidateTextureNames[i] =
			(uint32_t)resource.textureIdentity;
		candidateReady[i] = qtrue;
	}
	for ( uint32_t batch = 0u; batch < world.batchCount; ++batch ) {
		if ( world.batches[batch].noDraw ) continue;
		qboolean found = qfalse;
		for ( uint32_t i = 0u; i < frontend->materialCount; ++i ) {
			if ( frontend->materials[i].snapshot.handle
					!= world.batches[batch].baseMaterial ) continue;
			found = candidateReady[i];
			break;
		}
		if ( !found ) return qfalse;
	}
	if ( !RenderSubmission_PublishMaterialFactors( frontend,
		catalog->runtimeGeneration, factorEntries, factorEntryCount ) ) return qfalse;
	memcpy( product->cookedMaterialTextureNames, candidateTextureNames,
		sizeof( candidateTextureNames ) );
	memcpy( product->cookedMaterialReady, candidateReady,
		sizeof( candidateReady ) );
	for ( uint32_t i = 0u; i < frontend->materialCount; ++i )
		if ( candidateReady[i] )
			product->materialTextureNames[i] = candidateTextureNames[i];
	product->cookedMaterialRuntimeGeneration = catalog->runtimeGeneration;
	return qtrue;
}

qboolean RalOpenGl_ProductSetOutputExtent( ralOpenGlProduct_t *product, uint32_t width, uint32_t height )
{
	if ( !product || !width || !height || width > 16384u || height > 16384u )
		return qfalse;
	if ( width != product->outputWidth || height != product->outputHeight ) {
		RalOpenGl_ProductResetLensVisibility( product );
		product->exposureHistoryValid = qfalse;
	}
	product->outputWidth			= width;
	product->outputHeight			= height;
	product->atmosphereHistoryValid = qfalse;
	return qtrue;
}

qboolean RalOpenGl_ProductSetDirectionalLighting( ralOpenGlProduct_t *product,
		const ralOpenGlLightingReceipt_t *lighting ) {
	if ( !product ) return qfalse;
	if ( !lighting ) {
		memset( &product->directionalLighting, 0,
			sizeof( product->directionalLighting ) );
		product->hasDirectionalLighting = qfalse;
		return qtrue;
	}
	if ( !RalOpenGl_LightingReceiptExact( lighting, lighting )
			|| lighting->coreGeneration != product->coreReceipt.generation )
		return qfalse;
	product->directionalLighting = *lighting;
	product->hasDirectionalLighting = qtrue;
	return qtrue;
}

qboolean RalOpenGl_ProductSetDisplayVisibility( ralOpenGlProduct_t *product,
		const ralDisplayVisibilityPlan_t *visibility ) {
	if ( !product || !Ral_DisplayVisibilityPlanValid( visibility ) ) return qfalse;
	product->displayVisibility = *visibility;
	return qtrue;
}

qboolean RalOpenGl_ProductSetAntiAliasing( ralOpenGlProduct_t *product, const renderAntialiasingPlan_t *plan ) {
	if ( !product || !plan || plan->smaaQuality < 0 || plan->smaaQuality > 4 || ( plan->temporalRequested && plan->smaaQuality ) )
		return qfalse;
	product->taaEnabled = plan->temporalRequested ? qtrue : qfalse;
	product->smaaQuality = plan->smaaQuality;
	return qtrue;
}

void RalOpenGl_ProductAntiAliasingCounts( const ralOpenGlProduct_t *product, uint32_t *taaResolved, uint32_t *taaBootstrap,
		uint32_t *smaa ) {
	if ( taaResolved ) *taaResolved = product ? product->taaResolvedFrames : 0u;
	if ( taaBootstrap ) *taaBootstrap = product ? product->taaBootstrapFrames : 0u;
	if ( smaa ) *smaa = product ? product->smaaFrames : 0u;
}

/* The AA owner is created on the first frame that asks for anti-aliasing; its shaders are the catalog modules embedded here. */
static qboolean EnsureAa( ralOpenGlProduct_t *product ) {
	ralOpenGlAaSources_t sources;
	if ( product->aa ) return qtrue;
#define AA_SOURCE( field, name ) sources.field = (ralOpenGlAaSource_t){ ralOpenGlProduct##name##Source, ralOpenGlProduct##name##ByteCount }
	AA_SOURCE( temporalBootstrap, TemporalBootstrap ); AA_SOURCE( temporalResolve, TemporalResolve );
	AA_SOURCE( smaaEdgeVertex, SmaaEdgeVertex ); AA_SOURCE( smaaEdgeFragment, SmaaEdgeFragment );
	AA_SOURCE( smaaBlendVertex, SmaaBlendVertex ); AA_SOURCE( smaaBlendFragment, SmaaBlendFragment );
	AA_SOURCE( smaaResolveVertex, SmaaResolveVertex ); AA_SOURCE( smaaResolveFragment, SmaaResolveFragment );
#undef AA_SOURCE
	Ral_TemporalInit( &product->taaState );
	return RalOpenGl_AaCreate( product->core, &sources, &product->aa );
}

qboolean RalOpenGl_ProductSetLightmapBoost( ralOpenGlProduct_t *product,
		float lightmapBoost ) {
	if ( !product || !isfinite( lightmapBoost ) || lightmapBoost < 1.0f || lightmapBoost > 24.0f )
		return qfalse;
	product->lightmapBoost = lightmapBoost;
	return qtrue;
}

qboolean RalOpenGl_ProductSetAerialHaze( ralOpenGlProduct_t *product,
		float aerialHaze ) {
	if ( !product || !isfinite( aerialHaze ) || aerialHaze < 0.0f || aerialHaze > 1.0f )
		return qfalse;
	product->aerialHaze = aerialHaze;
	return qtrue;
}

qboolean RalOpenGl_ProductReadbackRgb( ralOpenGlProduct_t *product, byte *outPixels, uint32_t byteCount )
{
	uint64_t required;
	if ( !product || !outPixels || !product->outputWidth || !product->outputHeight )
		return qfalse;
	required = (uint64_t)product->outputWidth * product->outputHeight * 3u;
	if ( required != byteCount || required > INT_MAX )
		return qfalse;
	product->gl.PixelStorei( GL_PACK_ALIGNMENT_VALUE, 1 );
	product->gl.ReadPixels( 0, 0, (glSize_t)product->outputWidth, (glSize_t)product->outputHeight, GL_RGB_VALUE,
							GL_UNSIGNED_BYTE_VALUE, outPixels );
	return product->gl.GetError() == GL_NO_ERROR_VALUE ? qtrue : qfalse;
}

static void IdentityMatrix( float matrix[16] )
{
	memset( matrix, 0, 16u * sizeof( *matrix ) );
	matrix[0] = matrix[5] = matrix[10] = matrix[15] = 1.0f;
}

static qboolean InvertMatrix( const float source[16], float out[16] )
{
	float rows[4][8];
	for ( uint32_t row = 0u; row < 4u; ++row ) {
		for ( uint32_t column = 0u; column < 4u; ++column ) {
			rows[row][column]	   = source[column * 4u + row];
			rows[row][column + 4u] = row == column ? 1.0f : 0.0f;
		}
	}
	for ( uint32_t column = 0u; column < 4u; ++column ) {
		uint32_t pivot = column;
		for ( uint32_t row = column + 1u; row < 4u; ++row )
			if ( fabsf( rows[row][column] ) > fabsf( rows[pivot][column] ) )
				pivot = row;
		if ( fabsf( rows[pivot][column] ) < 1.0e-8f )
			return qfalse;
		if ( pivot != column )
			for ( uint32_t i = 0u; i < 8u; ++i ) {
				float swap		= rows[column][i];
				rows[column][i] = rows[pivot][i];
				rows[pivot][i]	= swap;
			}
		{
			float divisor = rows[column][column];
			for ( uint32_t i = 0u; i < 8u; ++i )
				rows[column][i] /= divisor;
		}
		for ( uint32_t row = 0u; row < 4u; ++row )
			if ( row != column ) {
				float factor = rows[row][column];
				for ( uint32_t i = 0u; i < 8u; ++i )
					rows[row][i] -= factor * rows[column][i];
			}
	}
	for ( uint32_t row = 0u; row < 4u; ++row )
		for ( uint32_t column = 0u; column < 4u; ++column )
			out[column * 4u + row] = rows[row][column + 4u];
	return qtrue;
}

static qboolean AtmosphereView( const renderSubmissionState_t *frontend, float inverse[16], float mvp[16],
								float eye[3] )
{
	renderWorldSnapshot_t view;
	float				  matrix[16] = { 0 };
	float				  projectionX, projectionY;
	if ( !frontend || !inverse || !mvp || !eye || !RenderSubmission_WorldSnapshot( frontend, &view ) ||
		 !isfinite( view.fovX ) || !isfinite( view.fovY ) || view.fovX <= 1.0f || view.fovX >= 179.0f ||
		 view.fovY <= 1.0f || view.fovY >= 179.0f ) {
		IdentityMatrix( inverse );
		IdentityMatrix( mvp );
		memset( eye, 0, 3u * sizeof( *eye ) );
		return qtrue;
	}
	projectionX = 1.0f / tanf( view.fovX * 0.00872664625997164788f );
	projectionY = 1.0f / tanf( view.fovY * 0.00872664625997164788f );
	for ( uint32_t axis = 0u; axis < 3u; ++axis ) {
		matrix[axis * 4u + 0u] = -view.viewAxis[1][axis] * projectionX;
		matrix[axis * 4u + 1u] = view.viewAxis[2][axis] * projectionY;
		matrix[axis * 4u + 2u] = view.viewAxis[0][axis];
		matrix[axis * 4u + 3u] = view.viewAxis[0][axis];
	}
	for ( uint32_t axis = 0u; axis < 3u; ++axis ) {
		matrix[12] += view.viewOrigin[axis] * view.viewAxis[1][axis] * projectionX;
		matrix[13] -= view.viewOrigin[axis] * view.viewAxis[2][axis] * projectionY;
		matrix[14] -= view.viewOrigin[axis] * view.viewAxis[0][axis];
		matrix[15] -= view.viewOrigin[axis] * view.viewAxis[0][axis];
	}
	matrix[14] -= 4.0f;
	memcpy( mvp, matrix, sizeof( matrix ) );
	memcpy( eye, view.viewOrigin, 3u * sizeof( *eye ) );
	return InvertMatrix( matrix, inverse );
}

static qboolean EncodeSkyPhysical( ralOpenGlProduct_t *product,
		const renderAtmosphereSnapshot_t *snapshot, uint32_t workMask,
		uint64_t frameGeneration )
{
	productSkyPhysicalParams_t params;
	glName_t output;
	uint32_t programIndex, width, height;
	const uint32_t physicalMask = RAL_SKY_DIRTY_TRANSMITTANCE |
		RAL_SKY_DIRTY_MULTISCATTER | RAL_SKY_DIRTY_SKY_VIEW;
	const atmosphereSkyEnvironment_t *sky;
	if ( !workMask ) return qtrue;
	if ( !product || !snapshot || !product->skyResourcesReady ||
		!product->skyPhysicalParamBuffer || product->skyFence ||
		( workMask & ~physicalMask ) != 0u ||
		( workMask & ( workMask - 1u ) ) != 0u ) return qfalse;
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
	if ( workMask == RAL_SKY_DIRTY_TRANSMITTANCE ) {
		programIndex = RAL_OPENGL_SKY_TRANSMITTANCE;
		width = RAL_OPENGL_SKY_TRANSMITTANCE_WIDTH;
		height = RAL_OPENGL_SKY_TRANSMITTANCE_HEIGHT;
	} else if ( workMask == RAL_SKY_DIRTY_MULTISCATTER ) {
		programIndex = RAL_OPENGL_SKY_MULTISCATTER;
		width = height = RAL_OPENGL_SKY_MULTISCATTER_SIZE;
	} else {
		programIndex = RAL_OPENGL_SKY_VIEW;
		width = RAL_OPENGL_SKY_VIEW_WIDTH;
		height = RAL_OPENGL_SKY_VIEW_HEIGHT;
	}
	output = product->skyPhysicalTextures[programIndex];
	if ( !output || !product->skyPhysicalPrograms[programIndex] ) return qfalse;
	product->gl.NamedBufferSubData( product->skyPhysicalParamBuffer, 0,
		sizeof( params ), &params );
	product->gl.UseProgram( product->skyPhysicalPrograms[programIndex] );
	product->gl.BindBufferBase( GL_UNIFORM_BUFFER_VALUE, 0u,
		product->skyPhysicalParamBuffer );
	if ( workMask != RAL_SKY_DIRTY_TRANSMITTANCE )
		product->gl.BindTextureUnit( 0u,
			product->skyPhysicalTextures[RAL_OPENGL_SKY_TRANSMITTANCE] );
	if ( workMask == RAL_SKY_DIRTY_SKY_VIEW )
		product->gl.BindTextureUnit( 1u,
			product->skyPhysicalTextures[RAL_OPENGL_SKY_MULTISCATTER] );
	product->gl.BindImageTexture( 0u, output, 0, 0u, 0,
		GL_WRITE_ONLY_VALUE, GL_RGBA16F_VALUE );
	product->gl.BeginQuery( GL_TIME_ELAPSED_VALUE,
		product->skyTimingQueries[0] );
	product->gl.DispatchCompute( ( width + 7u ) / 8u,
		( height + 7u ) / 8u, 1u );
	product->gl.MemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT_VALUE |
		GL_TEXTURE_FETCH_BARRIER_BIT_VALUE | GL_UNIFORM_BARRIER_BIT_VALUE );
	product->gl.EndQuery( GL_TIME_ELAPSED_VALUE );
	product->skyFence = product->gl.FenceSync(
		GL_SYNC_GPU_COMMANDS_COMPLETE_VALUE, 0u );
	if ( !product->skyFence || product->gl.GetError() != GL_NO_ERROR_VALUE ) {
		if ( product->skyFence ) product->gl.DeleteSync( product->skyFence );
		product->skyFence = NULL;
		return qfalse;
	}
	product->skySubmittedWorkMask = workMask;
	product->skyTimingFrameGeneration[0] = frameGeneration;
	product->skyTimingDurationNanoseconds[0] = 0u;
	product->skyTimingState[0] = RAL_SKY_GPU_TIMING_PENDING;
	product->skyPhysicalDispatchCount++;
	return qtrue;
}

static glName_t SkyTextureForHandle( const ralOpenGlProduct_t *product,
		qhandle_t handle )
{
	if ( !product || handle <= 0 ) return 0u;
	for ( uint32_t i = 0u; i < product->textureResourceCount; ++i )
		if ( product->materials[i].handle == handle )
			return product->materials[i].texture;
	return 0u;
}

// Cube-array coordinates match ral_sky_cube.glsl, not authored Q3 face UVs.
// One immutable quad per face; the same production sky fragment performs the
// ordered material evaluation. No CPU pixel evaluation or alternate shader.
static const float skyCaptureBasis[6][3][3] = {
    {{1,0,0},{0,0,-1},{0,-1,0}}, {{-1,0,0},{0,0,1},{0,-1,0}},
    {{0,1,0},{1,0,0},{0,0,1}}, {{0,-1,0},{1,0,0},{0,0,-1}},
    {{0,0,1},{1,0,0},{0,-1,0}}, {{0,0,-1},{-1,0,0},{0,-1,0}}
};

static qboolean EnsureSkyCapture(ralOpenGlProduct_t *p) {
    if(p->skyCaptureFramebuffer) return qtrue;
    glName_t fbo=0,vao=0,buffers[3]={0};
    renderWorldVertex_t vertices[24]={0};
    const unsigned offsets[5]={offsetof(renderWorldVertex_t,position),
        offsetof(renderWorldVertex_t,texCoord),offsetof(renderWorldVertex_t,lightmapCoord),
        offsetof(renderWorldVertex_t,color),offsetof(renderWorldVertex_t,normal)};
    const int sizes[5]={3,2,2,4,3};
    for(unsigned f=0;f<6;++f) for(unsigned v=0;v<4;++v) {
        renderWorldVertex_t *vertex=&vertices[f*4+v];
        float u=(v&1)?1:-1, t=(v&2)?1:-1;
        for(unsigned c=0;c<3;++c) {
            vertex->position[c]=1024*(skyCaptureBasis[f][0][c]
                +u*skyCaptureBasis[f][1][c]+t*skyCaptureBasis[f][2][c]);
            vertex->normal[c]=-skyCaptureBasis[f][0][c];
        }
        memset(vertex->color,255,sizeof(vertex->color));
    }
    p->gl.CreateFramebuffers(1,&fbo); p->gl.CreateVertexArrays(1,&vao);
    p->gl.CreateBuffers(3,buffers);
    if(!fbo||!vao||!buffers[0]||!buffers[1]||!buffers[2]) goto fail;
    p->gl.NamedBufferStorage(buffers[0],sizeof(vertices),vertices,0);
    p->gl.NamedBufferStorage(buffers[1],sizeof(renderProductDrawGpuParams_t),NULL,GL_DYNAMIC_STORAGE_BIT_VALUE);
    p->gl.NamedBufferStorage(buffers[2],sizeof(renderSkyMaterialGpuParams_t),NULL,GL_DYNAMIC_STORAGE_BIT_VALUE);
    p->gl.VertexArrayVertexBuffer(vao,0,buffers[0],0,sizeof(renderWorldVertex_t));
    for(unsigned i=0;i<5;++i) {
        p->gl.EnableVertexArrayAttrib(vao,i);
        p->gl.VertexArrayAttribFormat(vao,i,sizes[i],i==3?GL_UNSIGNED_BYTE_VALUE:0x1406,i==3,offsets[i]);
        p->gl.VertexArrayAttribBinding(vao,i,0);
    }
    if(p->gl.GetError()!=GL_NO_ERROR_VALUE) goto fail;
    p->skyCaptureFramebuffer=fbo; p->skyCaptureVao=vao;
    memcpy(p->skyCaptureBuffers,buffers,sizeof(buffers)); return qtrue;
fail:
    if(fbo) p->gl.DeleteFramebuffers(1,&fbo);
    if(vao) p->gl.DeleteVertexArrays(1,&vao);
    p->gl.DeleteBuffers(3,buffers); return qfalse;
}

static qboolean CaptureSkyMaterial(ralOpenGlProduct_t *p,
        const renderMaterialSnapshot_t *material,
        const ralSkyEnvironmentPlanReceipt_t *plan,uint32_t mask) {
    renderSkyMaterialGpuParams_t params;
    qhandle_t handles[8]; glName_t textures[10];
    // Sky lightmaps are not a valid environment source without a per-surface
    // binding. Reject explicitly rather than substituting the base texture.
    if(!wired_sky_material_pack(material,0,p->skyCaptureTimeMs,&params,handles)) return qfalse;
    for(unsigned i=0;i<8;++i) {
        textures[i]=SkyTextureForHandle(p,handles[i]); if(!textures[i]) return qfalse;
    }
    textures[8]=SkyTextureForHandle(p,material->handle);
    textures[9]=p->skyPhysicalTextures[RAL_OPENGL_SKY_VIEW];
    if(!textures[8]||!textures[9]||!EnsureSkyCapture(p)) return qfalse;
    glInt_t drawFbo=0,readFbo=0,viewport[4]={0},program=0,vao=0;
    const glEnum_t caps[]={GL_DEPTH_TEST_VALUE,GL_BLEND_VALUE,GL_CULL_FACE_VALUE,
        GL_FRAMEBUFFER_SRGB_VALUE,0x0c11}; // scissor
    glBool_t enabled[5]; qboolean ok=qtrue;
    p->gl.GetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING_VALUE,&drawFbo);
    p->gl.GetIntegerv(GL_READ_FRAMEBUFFER_BINDING_VALUE,&readFbo);
    p->gl.GetIntegerv(0x0ba2,viewport); // viewport
    p->gl.GetIntegerv(0x8b8d,&program); // current program
    p->gl.GetIntegerv(0x85b5,&vao); // vertex array binding
    for(unsigned i=0;i<5;++i){enabled[i]=p->gl.IsEnabled(caps[i]);p->gl.Disable(caps[i]);}
    p->gl.BindFramebuffer(GL_FRAMEBUFFER_VALUE,p->skyCaptureFramebuffer);
    p->gl.Viewport(0,0,RAL_OPENGL_SKY_SOURCE_SIZE,RAL_OPENGL_SKY_SOURCE_SIZE);
    p->gl.BindVertexArray(p->skyCaptureVao);p->gl.UseProgram(p->skyMaterialProgram);
    p->gl.BindBufferBase(GL_UNIFORM_BUFFER_VALUE,0,p->skyCaptureBuffers[1]);
    p->gl.BindBufferBase(GL_UNIFORM_BUFFER_VALUE,1,p->skyCaptureBuffers[2]);
    p->gl.NamedBufferSubData(p->skyCaptureBuffers[2],0,sizeof(params),&params);
    for(unsigned i=0;i<10;++i)p->gl.BindTextureUnit(i,textures[i]);
    for(unsigned f=0;f<6;++f) if(mask&(1u<<f)) {
        renderProductDrawGpuParams_t draw={0};
        for(unsigned c=0;c<3;++c) {
            draw.viewForward[c]=skyCaptureBasis[f][0][c];
            draw.viewLeft[c]=-skyCaptureBasis[f][1][c];
            draw.viewUp[c]=skyCaptureBasis[f][2][c];
        }
        draw.projectionLightmap[0]=draw.projectionLightmap[1]=1;
        draw.projectionLightmap[2]=1;
        draw.viewForward[3]=draw.viewLeft[3]=1; draw.viewUp[3]=.18f;
        draw.viewportAlpha[0]=draw.viewportAlpha[1]=RAL_OPENGL_SKY_SOURCE_SIZE;
        // Capture the target, not the display crossfade. Consumers apply the
        // previous/current radiance fade after all six faces are published.
        draw.atmosphereHeightSky[2]=Ral_SkyPhysicalSourceReady(plan,textures[9]!=0)?1.0f:0.0f;
        draw.atmosphereHeightSky[3]=1;
        p->gl.NamedBufferSubData(p->skyCaptureBuffers[1],0,sizeof(draw),&draw);
        p->gl.NamedFramebufferTextureLayer(p->skyCaptureFramebuffer,GL_COLOR_ATTACHMENT0_VALUE,p->skyTextures[0],0,f);
        if(p->gl.CheckNamedFramebufferStatus(p->skyCaptureFramebuffer,GL_FRAMEBUFFER_VALUE)!=GL_FRAMEBUFFER_COMPLETE_VALUE){ok=qfalse;break;}
        p->gl.DrawArrays(GL_TRIANGLE_STRIP_VALUE,f*4,4);
    }
    // Detach before world retirement; the persistent FBO must not retain the
    // old source cube. Caller restores its own UBO/texture inputs afterwards.
    p->gl.NamedFramebufferTexture(p->skyCaptureFramebuffer,GL_COLOR_ATTACHMENT0_VALUE,0,0);
    p->gl.BindFramebuffer(GL_DRAW_FRAMEBUFFER_VALUE,(glName_t)drawFbo);
    p->gl.BindFramebuffer(GL_READ_FRAMEBUFFER_VALUE,(glName_t)readFbo);
    p->gl.Viewport(viewport[0],viewport[1],viewport[2],viewport[3]);
    p->gl.UseProgram(program);p->gl.BindVertexArray(vao);
    for(unsigned i=0;i<5;++i)if(enabled[i])p->gl.Enable(caps[i]);
    return ok&&p->gl.GetError()==GL_NO_ERROR_VALUE;
}

static qboolean EncodeSkyRadiance( ralOpenGlProduct_t *product,
		const renderSubmissionState_t *frontend,
		const ralSkyEnvironmentPlanReceipt_t *skyPlan,
		ralAtmosphereTier_t tier, uint32_t eventMask,
		float timelineSeconds )
{
	ralSkyTierBudgetReceipt_t budget;
	ralSkyRadianceRequest_t request;
	uint32_t completedMask = 0u, admittedMask;
	uint64_t completedRevision = 0u;
	float deltaSeconds;
	qboolean timingQueryActive = qfalse;
	if ( !product || !frontend || !skyPlan || !product->skyResourcesReady
			|| !Ral_SkyEnvironmentPlanReceiptExact( skyPlan, skyPlan )
			|| !Ral_SkyTierBudget( tier, &budget )
			|| !Ral_SkyTierBudgetReceiptExact( &budget, &budget ) ) return qfalse;
	if ( !CollectSkyRadianceCompletion( product, qfalse, &completedMask,
			&completedRevision ) ) return qfalse;
	deltaSeconds = timelineSeconds >= product->skyRadianceTimeline
		? timelineSeconds - product->skyRadianceTimeline : 0.0f;
	product->skyRadianceTimeline = timelineSeconds;
	if ( product->skyRadianceFrameGeneration == UINT64_MAX ) return qfalse;
	memset( &request, 0, sizeof( request ) );
	request.schemaVersion = RAL_SKY_RADIANCE_SCHEMA_VERSION;
	request.backendType = RAL_BACKEND_OPENGL;
	request.frameGeneration = ++product->skyRadianceFrameGeneration;
	request.sourceRevision = skyPlan->sourceRevision;
	request.completedProductRevision = completedRevision;
	request.mode = skyPlan->selectedMode;
	request.specularMipCount = RAL_OPENGL_SKY_RADIANCE_MIPS;
	request.eventMask = eventMask;
	/* Tier degradation can select authored while the declaration still asks
	 * for physical. Invalidate only the target, retaining completed residency. */
	if ( product->skyRadiance.sourceRevision &&
			product->skyRadiance.mode != request.mode )
		request.eventMask |= RAL_SKY_EVENT_SOURCE_CHANGE;
	request.completedSliceMask = completedMask;
	request.maxSlicesPerFrame = product->skyRadianceFence ? 0u
		: budget.maxRadianceSlicesPerFrame;
	/* A physical source must not mix unready/authored and completed LUT
	 * faces in one publication. Physical LUT work remains independently sliced. */
	if (skyPlan->selectedMode == RAL_SKY_ENVIRONMENT_PHYSICAL
			&& !Ral_SkyPhysicalSourceReady(skyPlan,product->skyPhysicalTextures[RAL_OPENGL_SKY_VIEW]!=0))
		request.maxSlicesPerFrame = 0u;
	request.deltaSeconds = deltaSeconds;
	request.crossfadeSeconds = 0.35f;
	/* Reuse the shared bounded scheduler. Static stages and an unchanged
	 * material clock never trigger periodic GPU capture. */
	request.refreshIntervalSeconds = product->skyMaterialUsesTime
		&& frontend->worldSnapshot.timeMs != product->skyCaptureTimeMs ? 0.5f : 0.0f;
	if ( !Ral_SkyRadianceAdvance( &product->skyRadiance, &request,
			&product->skyRadianceReceipt ) ) return qfalse;
	if ( product->skyRadianceReceipt.sourceChanged
			|| product->skyRadianceReceipt.refreshStarted || eventMask ) {
		product->skyIrradianceWriteStarted = qfalse;
		product->skyRadianceWriteStarted = qfalse;
	}
	if ( product->skyRadianceReceipt.targetPublished ) {
		glName_t swap;
		swap = product->skyTextures[1]; product->skyTextures[1] = product->skyTextures[2];
		product->skyTextures[2] = swap;
		swap = product->skyTextures[3]; product->skyTextures[3] = product->skyTextures[4];
		product->skyTextures[4] = swap;
		swap = product->skyShBuffers[0]; product->skyShBuffers[0] = product->skyShBuffers[1];
		product->skyShBuffers[1] = swap;
		product->skyIrradianceWriteStarted = qfalse;
		product->skyRadianceWriteStarted = qfalse;
	}
	if ( product->skyRadianceReceipt.currentProductRevision ) {
		const uint32_t currentReadable =
			( product->skyTextures[3]
				? RAL_SKY_RADIANCE_PRODUCT_VISIBLE_RADIANCE
					| RAL_SKY_RADIANCE_PRODUCT_SPECULAR_RADIANCE : 0u )
			| ( product->skyTextures[1]
				? RAL_SKY_RADIANCE_PRODUCT_DIFFUSE_IRRADIANCE : 0u )
			| ( product->skyShBuffers[0]
				? RAL_SKY_RADIANCE_PRODUCT_AMBIENT_SH : 0u );
		const uint32_t previousReadable =
			product->skyRadianceReceipt.previousProductValid
			? ( product->skyTextures[4]
				? RAL_SKY_RADIANCE_PRODUCT_VISIBLE_RADIANCE
					| RAL_SKY_RADIANCE_PRODUCT_SPECULAR_RADIANCE : 0u )
				| ( product->skyTextures[2]
					? RAL_SKY_RADIANCE_PRODUCT_DIFFUSE_IRRADIANCE : 0u )
				| ( product->skyShBuffers[1]
					? RAL_SKY_RADIANCE_PRODUCT_AMBIENT_SH : 0u )
			: 0u;
		if ( !Ral_SkyRadianceConsumerPlan( &product->skyRadianceReceipt,
				currentReadable, previousReadable,
				&product->skyRadianceConsumer ) ) return qfalse;
	} else memset( &product->skyRadianceConsumer, 0,
		sizeof( product->skyRadianceConsumer ) );
	if ( !Ral_SkyRadianceConsumerAdmit( &budget,
			&product->skyRadianceConsumer, &product->skyRadianceConsumer ) ) return qfalse;
	admittedMask = product->skyRadianceReceipt.admittedSliceMask;
	if ( !admittedMask ) return qtrue;
	if ( product->skyRadianceFence ) return qfalse;
	if ( admittedMask & RAL_SKY_RADIANCE_CAPTURE_MASK ) {
		if (admittedMask & 1u) product->skyCaptureTimeMs = frontend->worldSnapshot.timeMs;
		renderMaterialSnapshot_t material;
		char semanticName[MAX_QPATH];
		productSkySourceParams_t params;
		renderAtmosphereSnapshot_t atmosphere;
		const atmosphereEmitter_t *atmosphereEmitters;
		qboolean authoredReady = qfalse;
		memset( &material, 0, sizeof( material ) );
		memset( semanticName, 0, sizeof( semanticName ) );
		const qboolean materialReady = RenderSubmission_ActiveSkyMaterial(frontend,&material,semanticName);
		product->skyMaterialUsesTime = materialReady && RenderSubmission_MaterialStagesUseTime(&material);
		if(materialReady && material.authoredSkyTexturesReady && material.authoredSkySource.outerBox) authoredReady=qtrue;
		if(materialReady && (material.stageCount || material.skyBox)) {
			product->gl.BeginQuery(GL_TIME_ELAPSED_VALUE,product->skyTimingQueries[1]);
			timingQueryActive=qtrue;
			if(!CaptureSkyMaterial(product,&material,skyPlan,admittedMask)) {
				product->gl.EndQuery(GL_TIME_ELAPSED_VALUE); return qfalse;
			}
		} else {
		if ( !RenderSubmission_AtmosphereSnapshot( frontend, &atmosphere,
				&atmosphereEmitters )
				|| !Ral_SkyRadianceSourceParamsBuild( skyPlan, atmosphere.state.cloudCover,
					authoredReady ? RAL_SKY_SOURCE_AUTHORED_FACES
						: RAL_SKY_SOURCE_ANALYTIC, &params ) ) return qfalse;
		product->gl.UseProgram( product->skyRadiancePrograms[RAL_OPENGL_SKY_SOURCE] );
		product->gl.BindImageTexture( 0u, product->skyTextures[0], 0, 1u, 0,
			GL_WRITE_ONLY_VALUE, GL_RGBA16F_VALUE );
		if ( authoredReady ) {
			for ( uint32_t face = 0u; face < RAL_MATERIAL_SKY_FACE_COUNT; ++face ) {
				glName_t texture = SkyTextureForHandle( product,
					material.authoredSkyOuterTextures[face] );
				if ( !texture ) return qfalse;
				product->gl.BindTextureUnit( face, texture );
			}
		}
		product->gl.BeginQuery( GL_TIME_ELAPSED_VALUE,
			product->skyTimingQueries[1] );
		timingQueryActive = qtrue;
		for ( uint32_t face = 0u; face < RAL_SKY_RADIANCE_CAPTURE_FACE_COUNT; ++face ) {
			const uint32_t slice = 1u << face;
			if ( !( admittedMask & slice ) ) continue;
			params.meta[0] = face;
			product->gl.NamedBufferSubData(
				product->skyRadianceParamBuffers[RAL_OPENGL_SKY_SOURCE_PARAMS],
				0, sizeof( params ), &params );
			product->gl.BindBufferBase( GL_UNIFORM_BUFFER_VALUE, 8u,
				product->skyRadianceParamBuffers[RAL_OPENGL_SKY_SOURCE_PARAMS] );
			product->gl.DispatchCompute( ( RAL_OPENGL_SKY_SOURCE_SIZE + 7u ) / 8u,
				( RAL_OPENGL_SKY_SOURCE_SIZE + 7u ) / 8u, 1u );
			product->skyRadianceDispatchCount++;
		}
		}
	}
	if ( admittedMask & RAL_SKY_RADIANCE_AMBIENT_SH_MASK ) {
		productSkySliceParams_t params;
		if ( !timingQueryActive ) {
			product->gl.BeginQuery( GL_TIME_ELAPSED_VALUE,
				product->skyTimingQueries[1] );
			timingQueryActive = qtrue;
		}
		memset( &params, 0, sizeof( params ) );
		product->gl.BindTextureUnit( 0u, product->skyTextures[0] );
		product->gl.BindBufferBase( GL_UNIFORM_BUFFER_VALUE, 3u,
			product->skyRadianceParamBuffers[RAL_OPENGL_SKY_SLICE_PARAMS] );
		if ( ( admittedMask & ( 1u << 6u ) )
				&& !product->skyIrradianceWriteStarted ) {
			product->gl.UseProgram( product->skyRadiancePrograms[RAL_OPENGL_SKY_IRRADIANCE] );
			product->gl.NamedBufferSubData(
				product->skyRadianceParamBuffers[RAL_OPENGL_SKY_SLICE_PARAMS],
				0, sizeof( params ), &params );
			product->gl.BindImageTexture( 0u, product->skyTextures[2], 0, 1u, 0,
				GL_WRITE_ONLY_VALUE, GL_RGBA16F_VALUE );
			product->gl.DispatchCompute( ( RAL_OPENGL_SKY_IRRADIANCE_SIZE + 7u ) / 8u,
				( RAL_OPENGL_SKY_IRRADIANCE_SIZE + 7u ) / 8u, 6u );
			product->skyIrradianceWriteStarted = qtrue;
			product->skyRadianceDispatchCount++;
		}
		product->gl.UseProgram( product->skyRadiancePrograms[RAL_OPENGL_SKY_PROJECT_SH] );
		product->gl.BindBufferBase( GL_SHADER_STORAGE_BUFFER_VALUE, 0u,
			product->skyShBuffers[1] );
		for ( uint32_t coefficient = 0u;
				coefficient < RAL_SKY_RADIANCE_AMBIENT_SH_COEFFICIENT_COUNT;
				++coefficient ) {
			const uint32_t slice = 1u << ( 6u + coefficient );
			if ( !( admittedMask & slice ) ) continue;
			params.values[0] = coefficient;
			product->gl.NamedBufferSubData(
				product->skyRadianceParamBuffers[RAL_OPENGL_SKY_SLICE_PARAMS],
				0, sizeof( params ), &params );
			product->gl.DispatchCompute( 1u, 1u, 1u );
			product->skyRadianceDispatchCount++;
		}
	}
	if ( admittedMask & RAL_SKY_RADIANCE_SPECULAR_MASK ) {
		productSkySliceParams_t params;
		if ( !timingQueryActive ) {
			product->gl.BeginQuery( GL_TIME_ELAPSED_VALUE,
				product->skyTimingQueries[1] );
			timingQueryActive = qtrue;
		}
		memset( &params, 0, sizeof( params ) );
		product->gl.UseProgram(
			product->skyRadiancePrograms[RAL_OPENGL_SKY_PREFILTER_RADIANCE] );
		product->gl.BindTextureUnit( 0u, product->skyTextures[0] );
		product->gl.BindBufferBase( GL_UNIFORM_BUFFER_VALUE, 3u,
			product->skyRadianceParamBuffers[RAL_OPENGL_SKY_SLICE_PARAMS] );
		for ( uint32_t mip = 0u; mip < RAL_OPENGL_SKY_RADIANCE_MIPS; ++mip ) {
			const uint32_t slice = 1u << ( 15u + mip );
			uint32_t size;
			if ( !( admittedMask & slice ) ) continue;
			params.values[0] = mip;
			params.values[1] = RAL_OPENGL_SKY_RADIANCE_MIPS;
			product->gl.NamedBufferSubData(
				product->skyRadianceParamBuffers[RAL_OPENGL_SKY_SLICE_PARAMS],
				0, sizeof( params ), &params );
			product->gl.BindImageTexture( 0u, product->skyTextures[4], mip,
				1u, 0, GL_WRITE_ONLY_VALUE, GL_RGBA16F_VALUE );
			size = WIRED_SKY_SPECULAR_SIZE >> mip;
			product->gl.DispatchCompute( ( size + 7u ) / 8u,
				( size + 7u ) / 8u, 6u );
			product->skyRadianceDispatchCount++;
		}
		product->skyRadianceWriteStarted = qtrue;
	}
	product->gl.MemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT_VALUE |
		GL_SHADER_STORAGE_BARRIER_BIT_VALUE | GL_TEXTURE_FETCH_BARRIER_BIT_VALUE |
		GL_UNIFORM_BARRIER_BIT_VALUE );
	if ( timingQueryActive ) product->gl.EndQuery( GL_TIME_ELAPSED_VALUE );
	product->skyRadianceFence = product->gl.FenceSync(
		GL_SYNC_GPU_COMMANDS_COMPLETE_VALUE, 0u );
	if ( !product->skyRadianceFence || product->gl.GetError() != GL_NO_ERROR_VALUE ) {
		if ( product->skyRadianceFence )
			product->gl.DeleteSync( product->skyRadianceFence );
		product->skyRadianceFence = NULL;
		return qfalse;
	}
	product->skyRadianceSubmittedSliceMask = admittedMask;
	product->skyRadianceSubmittedRevision =
		product->skyRadianceReceipt.targetProductRevision;
	product->skyTimingFrameGeneration[1] = skyPlan->frameGeneration;
	product->skyTimingDurationNanoseconds[1] = 0u;
	product->skyTimingState[1] = RAL_SKY_GPU_TIMING_PENDING;
	return qtrue;
}

static qboolean PlanAtmosphere( ralOpenGlProduct_t *product, const renderSubmissionState_t *frontend,
								const renderSubmissionReceipt_t *submission, uint64_t frameGeneration,
								ralAtmospherePlanReceipt_t *outReceipt )
{
	renderAtmosphereSnapshot_t		snapshot;
	renderAtmosphereMediaSnapshot_t media;
	ralAtmospherePlanRequest_t		request;
	ralAtmospherePlanReceipt_t		receipt;
	ralSkyEnvironmentPlanRequest_t skyPlan;
	ralSkyEnvironmentRuntimeRequest_t skyRuntimeRequest;
	ralSkyEnvironmentRuntimeReceipt_t skyRuntimeReceipt;
	ralSkyEnvironmentRuntimeState_t skyRuntimeCandidate;
	ralSkyEnvironmentCapabilities_t skyCapabilities;
	uint64_t skyWorldLifecycleGeneration;
	uint32_t skyEventMask = 0u;
	uint32_t completedSkyWorkMask = 0u;
	const atmosphereEmitter_t	   *emitters;
	const atmosphereMediaVolume_t  *volumes;
	if ( !product || !frontend || !submission || !outReceipt ||
		 !RenderSubmission_AtmosphereSnapshot( frontend, &snapshot, &emitters ) ||
		 !RenderSubmission_AtmosphereMediaSnapshot( frontend, &media, &volumes ) )
		return qfalse;
	(void)emitters;
	(void)volumes;
	(void)submission;
	memset( &request, 0, sizeof( request ) );
	request.schemaVersion	= RAL_ATMOSPHERE_PLAN_SCHEMA_VERSION;
	request.backendType		= RAL_BACKEND_OPENGL;
	request.frameGeneration = frameGeneration;
	request.width			= product->outputWidth;
	request.height			= product->outputHeight;
	request.requestedTier = snapshot.active ? (ralAtmosphereTier_t)snapshot.state.qualityTier : RAL_ATMOSPHERE_TIER_OFF;
	request.localVolumeCount  = media.count;
	request.lightCount = RenderSubmission_AtmosphereLightCount( frontend );
	if ( request.lightCount > RAL_ATMOSPHERE_MAX_LIGHTS )
		request.lightCount = RAL_ATMOSPHERE_MAX_LIGHTS;
	request.shadowedLightCount = RenderSubmission_AtmosphereShadowedLightCount( frontend );
	if ( request.shadowedLightCount > RAL_ATMOSPHERE_MAX_SHADOWED_LIGHTS )
		request.shadowedLightCount = RAL_ATMOSPHERE_MAX_SHADOWED_LIGHTS;
	request.maxFroxelCount	  = RAL_OPENGL_ATMOSPHERE_FROXEL_CAPACITY;
	request.maxLocalVolumes	  = RAL_ATMOSPHERE_MAX_VOLUMES;
	request.maxLights		  = RAL_ATMOSPHERE_MAX_LIGHTS;
	request.maxShadowedLights = RAL_ATMOSPHERE_MAX_SHADOWED_LIGHTS;
	request.mediaActive = snapshot.active && ( snapshot.state.mediaDensity > 0.0f || snapshot.state.visibility > 0.0f ||
											   media.count > 0u )
							  ? qtrue
							  : qfalse;
	request.skyLightingActive			   = snapshot.active;
	request.cloudsRequested				   = snapshot.active && snapshot.state.cloudCover > 0.0f;
	request.cameraCut = RenderSubmission_ViewCameraCut( frontend );
	if ( request.cameraCut ) {
		product->atmosphereHistoryValid = qfalse;
		skyEventMask |= RAL_SKY_EVENT_CAMERA_CUT;
	}
	request.historyValid = product->atmosphereHistoryValid;
	request.capabilities.analyticComposite = qtrue;
	request.capabilities.compute		   = qtrue;
	request.capabilities.storageBuffers	   = qtrue;
	request.capabilities.temporalHistory   = qtrue;
	request.capabilities.fullClouds		   = qtrue;
	memset( &skyCapabilities, 0, sizeof( skyCapabilities ) );
	skyWorldLifecycleGeneration = 0u;
	if ( frontend->activeWorldIndex >= 0
			&& !RenderSubmission_WorldLifecycleGeneration( frontend,
				frontend->activeWorldIndex,
				&skyWorldLifecycleGeneration ) ) return qfalse;
	if ( skyWorldLifecycleGeneration != 0u
			&& product->skyWorldLifecycleGeneration != 0u
			&& product->skyWorldLifecycleGeneration
				!= skyWorldLifecycleGeneration )
		skyEventMask |= RAL_SKY_EVENT_MAP_TRANSITION;
	if ( skyWorldLifecycleGeneration != 0u
			&& !EnsureSkyRadianceResources( product,
				skyWorldLifecycleGeneration ) ) return qfalse;
	if ( !CollectSkyWorkCompletion( product, qfalse,
			&completedSkyWorkMask ) ) return qfalse;
	skyCapabilities.physicalLuts = product->skyResourcesReady;
	skyCapabilities.temporalCrossfade = product->skyResourcesReady;
	/* Native world lowering consumes the exact bounded distant submission;
	 * advertise this only now that the product can execute it. */
	skyCapabilities.distantScene = qtrue;
	if ( !RenderSubmission_BuildSkyEnvironmentRuntimeRequest( &snapshot,
			&product->skyRuntime, RAL_BACKEND_OPENGL, frameGeneration,
			request.requestedTier, skyEventMask, completedSkyWorkMask,
			product->skyFence ? 0u : 1u,
			snapshot.state.timelineSeconds, &skyCapabilities,
			&skyRuntimeRequest ) ) return qfalse;
	if ( product->skyFence && ( skyRuntimeRequest.eventMask &
			( RAL_SKY_EVENT_MAP_TRANSITION | RAL_SKY_EVENT_DEVICE_RECREATE |
			  RAL_SKY_EVENT_SOURCE_CHANGE ) ) )
		product->skySubmittedWorkMask = 0u;
	skyRuntimeCandidate = product->skyRuntime;
	if ( !Ral_SkyEnvironmentRuntimeAdvance( &skyRuntimeCandidate,
			&skyRuntimeRequest, &skyPlan, &skyRuntimeReceipt ) ) return qfalse;
	request.skyEnvironment = skyPlan;
	if ( !Ral_AtmospherePlan( &request, &receipt ) ) return qfalse;
	if ( !EncodeSkyPhysical( product, &snapshot,
			receipt.skyEnvironment.admittedWorkMask,
			frameGeneration ) ) return qfalse;
	/* Sky radiance needs the per-world resources (EnsureSkyRadianceResources above); a frame without a world (menu, loading) has
	 * none and nothing to encode. */
	if ( product->skyResourcesReady && !EncodeSkyRadiance( product, frontend, &receipt.skyEnvironment,
			receipt.selectedTier, skyRuntimeRequest.eventMask,
			snapshot.state.timelineSeconds ) ) return qfalse;
	if ( !Ral_SkyEnvironmentRuntimeSchedule( &skyRuntimeCandidate,
			&receipt.skyEnvironment ) ) {
		uint32_t ignoredCompletion = 0u;
		if ( product->skyFence )
			CollectSkyWorkCompletion( product, qtrue, &ignoredCompletion );
		return qfalse;
	}
	product->skyRuntime = skyRuntimeCandidate;
	memset( &product->skyGpuTelemetry, 0,
		sizeof( product->skyGpuTelemetry ) );
	product->skyGpuTelemetry.schemaVersion =
		RAL_SKY_GPU_TELEMETRY_SCHEMA_VERSION;
	product->skyGpuTelemetry.backendType = RAL_BACKEND_OPENGL;
	product->skyGpuTelemetry.frameGeneration = frameGeneration;
	product->skyGpuTelemetry.sourceRevision =
		receipt.skyEnvironment.sourceRevision;
	product->skyGpuTelemetry.selectedTier = receipt.selectedTier;
	product->skyGpuTelemetry.selectedMode =
		receipt.skyEnvironment.selectedMode;
	product->skyGpuTelemetry.submittedWorkMask =
		receipt.skyEnvironment.admittedWorkMask;
	product->skyGpuTelemetry.completedWorkMask = completedSkyWorkMask;
	product->skyGpuTelemetry.deferredWorkMask =
		receipt.skyEnvironment.deferredWorkMask;
	product->skyGpuTelemetry.cancelledWorkMask =
		receipt.skyEnvironment.cancelledWorkMask |
			skyRuntimeReceipt.cancelledWorkMask;
	product->skyGpuTelemetry.submittedSliceMask =
		product->skyRadianceReceipt.admittedSliceMask;
	product->skyGpuTelemetry.completedSliceMask =
		product->skyRadianceReceipt.acceptedCompletionMask;
	product->skyGpuTelemetry.deferredSliceCount =
		product->skyRadianceReceipt.pendingSliceCount;
	product->skyGpuTelemetry.cancelledSliceCount =
		product->skyRadianceReceipt.cancelledSliceCount;
	product->skyGpuTelemetry.currentRadianceRevision =
		product->skyRadianceReceipt.currentProductRevision;
	product->skyGpuTelemetry.targetRadianceRevision =
		product->skyRadianceReceipt.targetProductRevision;
	product->skyGpuTelemetry.physicalTimingFrameGeneration =
		product->skyTimingFrameGeneration[0];
	product->skyGpuTelemetry.radianceTimingFrameGeneration =
		product->skyTimingFrameGeneration[1];
	product->skyGpuTelemetry.physicalTimingState =
		product->skyTimingState[0];
	product->skyGpuTelemetry.radianceTimingState =
		product->skyTimingState[1];
	product->skyGpuTelemetry.physicalGpuDurationNanoseconds =
		product->skyTimingDurationNanoseconds[0];
	product->skyGpuTelemetry.radianceGpuDurationNanoseconds =
		product->skyTimingDurationNanoseconds[1];
	product->skyGpuTelemetry.ready = qtrue;
	if ( !Ral_SkyGpuTelemetryReceiptExact( &product->skyGpuTelemetry,
			&product->skyGpuTelemetry ) ) return qfalse;
	if ( skyWorldLifecycleGeneration != 0u )
		product->skyWorldLifecycleGeneration = skyWorldLifecycleGeneration;
	*outReceipt = receipt;
	return qtrue;
}

static qboolean EnsureAtmosphereBuffers( ralOpenGlProduct_t *product )
{
	glName_t candidate[RAL_OPENGL_ATMOSPHERE_FALLBACK] = {0};
	const glSizePtr_t froxelBytes = (glSizePtr_t)RAL_OPENGL_ATMOSPHERE_FROXEL_CAPACITY * 16;
	const glSizePtr_t tileBytes =
		(glSizePtr_t)( RAL_OPENGL_ATMOSPHERE_FROXEL_CAPACITY / RAL_ATMOSPHERE_MIN_SLICES ) * 33 * 4;
	if ( !product )
		return qfalse;
	if ( product->atmosphereBuffersReady )
		return qtrue;
	/* Publish the complete immutable allocation once. Failed attempts must not
	 * overwrite owned names or touch the separately owned fallback buffer. */
	product->gl.CreateBuffers( RAL_OPENGL_ATMOSPHERE_FALLBACK, candidate );
	if ( product->gl.GetError() != GL_NO_ERROR_VALUE ) goto fail;
	for ( uint32_t i = 0u; i < RAL_OPENGL_ATMOSPHERE_FALLBACK; ++i )
		if ( !candidate[i] ) goto fail;
	product->gl.NamedBufferStorage( candidate[RAL_OPENGL_ATMOSPHERE_PARAMS], 192, NULL,
									GL_DYNAMIC_STORAGE_BIT_VALUE );
	product->gl.NamedBufferStorage( candidate[RAL_OPENGL_ATMOSPHERE_VOLUMES],
									(glSizePtr_t)RAL_ATMOSPHERE_MAX_VOLUMES * sizeof( renderAtmosphereVolumeGpu_t ), NULL,
									GL_DYNAMIC_STORAGE_BIT_VALUE );
	product->gl.NamedBufferStorage( candidate[RAL_OPENGL_ATMOSPHERE_MEDIA], froxelBytes, NULL, 0u );
	product->gl.NamedBufferStorage( candidate[RAL_OPENGL_ATMOSPHERE_TILE_LIGHTS], tileBytes, NULL,
									0u );
	product->gl.NamedBufferStorage( candidate[RAL_OPENGL_ATMOSPHERE_LIGHTS],
									(glSizePtr_t)RAL_ATMOSPHERE_MAX_LIGHTS * sizeof( renderAtmosphereLocalLightGpu_t ), NULL,
			GL_DYNAMIC_STORAGE_BIT_VALUE );
	/* No scene-depth culling: the tile packet explicitly selects the full slab. */
	const float unusedDepth = 0.0f;
	product->gl.NamedBufferStorage( candidate[RAL_OPENGL_ATMOSPHERE_TILE_DEPTH],
			sizeof( unusedDepth ), &unusedDepth, 0u );
	for ( uint32_t i = RAL_OPENGL_ATMOSPHERE_LIT; i <= RAL_OPENGL_ATMOSPHERE_HISTORY_B; ++i )
		product->gl.NamedBufferStorage( candidate[i], froxelBytes, NULL, 0u );
	if ( product->gl.GetError() != GL_NO_ERROR_VALUE )
		goto fail;
	memcpy( product->atmosphereBuffers, candidate, sizeof(candidate) );
	product->atmosphereBuffersReady = qtrue;
	return qtrue;
fail:
	product->gl.DeleteBuffers( RAL_OPENGL_ATMOSPHERE_FALLBACK, candidate );
	return qfalse;
}

static void BindComputeBuffer( const ralOpenGlProduct_t *product, unsigned int binding, uint32_t buffer )
{
	product->gl.BindBufferBase( GL_SHADER_STORAGE_BUFFER_VALUE, binding, product->atmosphereBuffers[buffer] );
}

static qboolean EncodeAtmosphere( ralOpenGlProduct_t *product, const renderSubmissionState_t *frontend,
        const ralAtmospherePlanReceipt_t *plan, uint32_t *nativeDispatchCount )
{
    renderAtmosphereGpuFrame_t frame;
    if ( !product || !nativeDispatchCount ||
         !RenderAtmosphereGpu_Build( frontend, plan, RAL_OPENGL_ATMOSPHERE_FROXEL_CAPACITY,
             product->atmosphereHistoryValid, &frame ) ||
         !EnsureAtmosphereBuffers( product ) ) return qfalse;
    *nativeDispatchCount = 0u;
    const uint32_t previous = product->atmosphereHistoryParity
        ? RAL_OPENGL_ATMOSPHERE_HISTORY_B : RAL_OPENGL_ATMOSPHERE_HISTORY_A;
    const uint32_t next = product->atmosphereHistoryParity
        ? RAL_OPENGL_ATMOSPHERE_HISTORY_A : RAL_OPENGL_ATMOSPHERE_HISTORY_B;
    product->gl.NamedBufferSubData( product->atmosphereBuffers[RAL_OPENGL_ATMOSPHERE_VOLUMES],
        0, sizeof( frame.volumes ), frame.volumes );
    product->gl.NamedBufferSubData( product->atmosphereBuffers[RAL_OPENGL_ATMOSPHERE_LIGHTS],
        0, sizeof( frame.lights ), frame.lights );
    product->gl.BindBufferBase( GL_UNIFORM_BUFFER_VALUE, 0u,
        product->atmosphereBuffers[RAL_OPENGL_ATMOSPHERE_PARAMS] );

    /* Tile lists must be refreshed even with zero lights: stale lists from a
     * previous frame are not valid input to the canonical light kernel. */
    product->gl.NamedBufferSubData( product->atmosphereBuffers[RAL_OPENGL_ATMOSPHERE_PARAMS],
        0, sizeof( frame.tile ), &frame.tile );
    product->gl.UseProgram( product->atmospherePrograms[4] );
    /* The translated push block uses binding 3, unlike Params at binding 0. */
    product->gl.BindBufferBase( GL_UNIFORM_BUFFER_VALUE, 3u,
        product->atmosphereBuffers[RAL_OPENGL_ATMOSPHERE_PARAMS] );
    BindComputeBuffer( product, 0u, RAL_OPENGL_ATMOSPHERE_LIGHTS );
    BindComputeBuffer( product, 1u, RAL_OPENGL_ATMOSPHERE_TILE_DEPTH );
    BindComputeBuffer( product, 2u, RAL_OPENGL_ATMOSPHERE_TILE_LIGHTS );
    product->gl.DispatchCompute( (plan->froxelWidth+7u)/8u, (plan->froxelHeight+7u)/8u, 1u );
    ++*nativeDispatchCount;
    product->gl.MemoryBarrier( GL_SHADER_STORAGE_BARRIER_BIT_VALUE | GL_UNIFORM_BARRIER_BIT_VALUE );

    product->gl.NamedBufferSubData( product->atmosphereBuffers[RAL_OPENGL_ATMOSPHERE_PARAMS],
        0, sizeof( frame.inject ), &frame.inject );
    product->gl.UseProgram( product->atmospherePrograms[0] );
    BindComputeBuffer( product, 0u, RAL_OPENGL_ATMOSPHERE_VOLUMES );
    BindComputeBuffer( product, 1u, RAL_OPENGL_ATMOSPHERE_MEDIA );
    product->gl.DispatchCompute( (plan->froxelWidth+3u)/4u, (plan->froxelHeight+3u)/4u,
        (plan->froxelDepth+3u)/4u );
    ++*nativeDispatchCount;
    product->gl.MemoryBarrier( GL_SHADER_STORAGE_BARRIER_BIT_VALUE | GL_UNIFORM_BARRIER_BIT_VALUE );

    product->gl.NamedBufferSubData( product->atmosphereBuffers[RAL_OPENGL_ATMOSPHERE_PARAMS],
        0, sizeof( frame.light ), &frame.light );
    product->gl.UseProgram( product->atmospherePrograms[1] );
    BindComputeBuffer( product, 0u, RAL_OPENGL_ATMOSPHERE_MEDIA );
    BindComputeBuffer( product, 1u, RAL_OPENGL_ATMOSPHERE_TILE_LIGHTS );
    BindComputeBuffer( product, 2u, RAL_OPENGL_ATMOSPHERE_LIGHTS );
    BindComputeBuffer( product, 3u, RAL_OPENGL_ATMOSPHERE_LIT );
    product->gl.DispatchCompute( (plan->froxelWidth+3u)/4u, (plan->froxelHeight+3u)/4u,
        (plan->froxelDepth+3u)/4u );
    ++*nativeDispatchCount;
    product->gl.MemoryBarrier( GL_SHADER_STORAGE_BARRIER_BIT_VALUE | GL_UNIFORM_BARRIER_BIT_VALUE );

    uint32_t source = RAL_OPENGL_ATMOSPHERE_LIT;
    if ( plan->cloudsActive ) {
        product->gl.NamedBufferSubData( product->atmosphereBuffers[RAL_OPENGL_ATMOSPHERE_PARAMS],
            0, sizeof( frame.cloud ), &frame.cloud );
        product->gl.UseProgram( product->atmospherePrograms[2] );
        BindComputeBuffer( product, 0u, RAL_OPENGL_ATMOSPHERE_LIT );
        BindComputeBuffer( product, 1u, RAL_OPENGL_ATMOSPHERE_CLOUD );
        product->gl.DispatchCompute( (plan->froxelWidth+3u)/4u, (plan->froxelHeight+3u)/4u,
            (plan->froxelDepth+3u)/4u );
        ++*nativeDispatchCount;
        product->gl.MemoryBarrier( GL_SHADER_STORAGE_BARRIER_BIT_VALUE | GL_UNIFORM_BARRIER_BIT_VALUE );
        source = RAL_OPENGL_ATMOSPHERE_CLOUD;
    }
    product->gl.NamedBufferSubData( product->atmosphereBuffers[RAL_OPENGL_ATMOSPHERE_PARAMS],
        0, sizeof( frame.integrate ), &frame.integrate );
    product->gl.UseProgram( product->atmospherePrograms[3] );
    BindComputeBuffer( product, 0u, source );
    BindComputeBuffer( product, 1u, previous );
    BindComputeBuffer( product, 2u, RAL_OPENGL_ATMOSPHERE_INTEGRATED );
    BindComputeBuffer( product, 3u, next );
    product->gl.DispatchCompute( (plan->froxelWidth+7u)/8u, (plan->froxelHeight+7u)/8u, 1u );
    ++*nativeDispatchCount;
    product->gl.MemoryBarrier( GL_SHADER_STORAGE_BARRIER_BIT_VALUE | GL_UNIFORM_BARRIER_BIT_VALUE );
    if ( product->gl.GetError() != GL_NO_ERROR_VALUE ||
         *nativeDispatchCount != plan->computeDispatchCount+1u ) return qfalse;
    product->atmosphereHistoryValid = qtrue;
    product->atmosphereHistoryParity = !product->atmosphereHistoryParity;
    return qtrue;
}

static void WeatherWeights( const atmosphereFrameState_t *state, float weights[5] )
{
	float total = 0.0f;
	memcpy( weights, state->precipitation, 5u * sizeof( *weights ) );
	for ( uint32_t i = 0u; i < 5u; ++i )
		total += weights[i];
	if ( total <= 0.0f && state->type >= ATMOSPHERE_PRECIP_RAIN && state->type <= ATMOSPHERE_PRECIP_DUST_ASH )
		weights[(uint32_t)state->type - 1u] = 1.0f;
}

void RalOpenGl_ProductUnloadHeightgrid( ralOpenGlProduct_t *product, int worldIndex )
{
	if ( !product || product->weatherHeightgridWorldIndex != worldIndex ) return;
	if ( product->weatherHeightgrid ) product->gl.DeleteTextures( 1, &product->weatherHeightgrid );
	product->weatherHeightgrid = 0;
	product->weatherHeightgridDigest = 0;
	product->weatherHeightgridWorldIndex = -1;
}

static qboolean SyncWeatherHeightgrid( ralOpenGlProduct_t *product,
		const renderAtmosphereHeightgrid_t *grid )
{
	glName_t candidate = 0;
	if ( product->weatherHeightgrid && product->weatherHeightgridDigest == grid->digest ) return qtrue;
	/* Immutable publication: GL retains old storage for queued draws. A failed
	 * replacement leaves the previous cache intact; no per-frame upload/wait. */
	product->gl.CreateTextures( GL_TEXTURE_2D_VALUE, 1, &candidate );
	if ( !candidate ) return qfalse;
	product->gl.TextureStorage2D( candidate, 1, 0x822eu /* R32F */, grid->edge, grid->edge );
	product->gl.TextureSubImage2D( candidate, 0, 0, 0, grid->edge, grid->edge,
		0x1903u /* RED */, 0x1406u /* FLOAT */, grid->samples );
	product->gl.TextureParameteri( candidate, GL_TEXTURE_MIN_FILTER_VALUE, GL_LINEAR_VALUE );
	product->gl.TextureParameteri( candidate, GL_TEXTURE_MAG_FILTER_VALUE, GL_LINEAR_VALUE );
	product->gl.TextureParameteri( candidate, GL_TEXTURE_WRAP_S_VALUE, GL_CLAMP_TO_EDGE_VALUE );
	product->gl.TextureParameteri( candidate, GL_TEXTURE_WRAP_T_VALUE, GL_CLAMP_TO_EDGE_VALUE );
	if ( product->gl.GetError() != GL_NO_ERROR_VALUE ) {
		product->gl.DeleteTextures( 1, &candidate );
		return qfalse;
	}
	RalOpenGl_ProductUnloadHeightgrid( product, product->weatherHeightgridWorldIndex );
	product->weatherHeightgrid = candidate;
	product->weatherHeightgridDigest = grid->digest;
	product->weatherHeightgridWorldIndex = grid->worldIndex;
	return qtrue;
}

static qboolean PlanWeather( const renderSubmissionState_t *frontend, const ralAtmospherePlanReceipt_t *atmosphere,
							 ralAtmosphereWeatherReceipt_t *outReceipt )
{
	renderAtmosphereSnapshot_t				 snapshot;
	renderAtmosphereEffectWorkloadSnapshot_t effects;
	ralAtmosphereWeatherRequest_t			 request;
	const atmosphereEmitter_t				*emitters;
	if ( !frontend || !atmosphere || !outReceipt ||
		 !RenderSubmission_AtmosphereSnapshot( frontend, &snapshot, &emitters ) )
		return qfalse;
	(void)emitters;
	memset( &request, 0, sizeof( request ) );
	request.schemaVersion  = RAL_ATMOSPHERE_WEATHER_SCHEMA_VERSION;
	request.tier		   = atmosphere->selectedTier;
	request.maxParticles   = RAL_OPENGL_WEATHER_PARTICLE_CAPACITY;
	request.enabled		   = snapshot.active && frontend->sceneRendered;
	request.indoorExposure = snapshot.state.indoorExposure;
	{
		renderAtmosphereHeightgrid_t grid;
		request.heightgridRequested = (snapshot.state.flags & ATMOSPHERE_FLAG_HEIGHTGRID) ? qtrue : qfalse;
		request.heightgridAvailable = RenderSubmission_AtmosphereHeightgrid( frontend, &grid );
		if ( request.enabled && request.heightgridRequested && !request.heightgridAvailable ) return qfalse;
	}
	if ( !RenderSubmission_AtmosphereEffectWorkloadSnapshot( frontend, request.maxParticles, &effects ) )
		return qfalse;
	request.semanticEmitterCount  = effects.admittedEmitterCount;
	request.semanticParticleCount = effects.admittedParticleCount;
	WeatherWeights( &snapshot.state, request.precipitation );
	return Ral_AtmospherePlanWeather( &request, outReceipt );
}

static qboolean EnsureWeatherBuffers( ralOpenGlProduct_t *product )
{
	glName_t candidate[3] = {0};
	void			 *zero;
	const glSizePtr_t bytes = (glSizePtr_t)RAL_OPENGL_WEATHER_PARTICLE_CAPACITY * 32;
	if ( !product )
		return qfalse;
	if ( product->weatherBuffersReady )
		return qtrue;
	zero = calloc( 1u, (size_t)bytes );
	if ( !zero )
		return qfalse;
	product->gl.CreateBuffers( 3, candidate );
	if ( product->gl.GetError() != GL_NO_ERROR_VALUE || !candidate[0] || !candidate[1] || !candidate[2] )
		goto fail;
	product->gl.NamedBufferStorage( candidate[0], sizeof( productWeatherFrame_t ), NULL,
									GL_DYNAMIC_STORAGE_BIT_VALUE );
	product->gl.NamedBufferStorage( candidate[1], bytes, zero, 0u );
	product->gl.NamedBufferStorage( candidate[2], bytes, zero, 0u );
	if ( product->gl.GetError() != GL_NO_ERROR_VALUE )
		goto fail;
	free( zero );
	memcpy( product->weatherBuffers, candidate, sizeof(candidate) );
	product->weatherBuffersReady = qtrue;
	return qtrue;
fail:
	free( zero );
	product->gl.DeleteBuffers( 3, candidate );
	return qfalse;
}

static qboolean EncodeWeather( ralOpenGlProduct_t *product, const renderSubmissionState_t *frontend,
							   const ralAtmosphereWeatherReceipt_t *weather, productWeatherFrame_t *outFrame )
{
	renderAtmosphereSnapshot_t				 snapshot;
	renderAtmosphereEffectWorkloadSnapshot_t effects;
	renderWorldSnapshot_t					 view;
	const atmosphereEmitter_t				*emitters;
	productWeatherFrame_t					 frame;
	float									 inverse[16], eye[3], weights[5];
	uint32_t								 readPool, writePool;
	if ( !product || !frontend || !weather || !outFrame || weather->zeroWork || !EnsureWeatherBuffers( product ) ||
		 !RenderSubmission_AtmosphereSnapshot( frontend, &snapshot, &emitters ) )
		return qfalse;
	(void)emitters;
	(void)inverse;
	memset( &frame, 0, sizeof( frame ) );
	if ( !AtmosphereView( frontend, inverse, frame.mvp, eye ) )
		return qfalse;
	if ( RenderSubmission_WorldSnapshot( frontend, &view ) ) {
		memcpy( frame.viewLeft, view.viewAxis[1], 3u * sizeof( float ) );
		memcpy( frame.viewUp, view.viewAxis[2], 3u * sizeof( float ) );
	} else {
		frame.viewLeft[1] = 1.0f;
		frame.viewUp[2]	  = 1.0f;
	}
	memcpy( frame.eyeWorld, eye, sizeof( eye ) );
	/* Scene particles stay linear HDR; display visibility is applied once after metering. */
	frame.viewLeft[3] = 1.0f;
	frame.viewUp[3] = 1.0f;
	frame.eyeWorld[3] = RAL_DISPLAY_SHADOW_PIVOT;
	frame.dt =
		product->weatherTimeline > 0.0f ? snapshot.state.timelineSeconds - product->weatherTimeline : 1.0f / 60.0f;
	if ( frame.dt < 0.0f || frame.dt > 0.1f )
		frame.dt = 1.0f / 60.0f;
	frame.time		   = snapshot.state.timelineSeconds;
	frame.poolSize	   = weather->activeParticleCount;
	frame.pingPongRead = product->weatherParity ? 1u : 0u;
	memcpy( frame.boundsMin, snapshot.state.bounds, 3u * sizeof( float ) );
	memcpy( frame.boundsMax, snapshot.state.bounds + 3u, 3u * sizeof( float ) );
	memcpy( frame.worldMins, snapshot.state.worldMins, 2u * sizeof( float ) );
	memcpy( frame.worldMaxs, snapshot.state.worldMaxs, 2u * sizeof( float ) );
	frame.atmType  = (uint32_t)snapshot.state.type;
	frame.distance = snapshot.state.distance > 0.0f ? snapshot.state.distance : 1024.0f;
	memcpy( frame.windGust, snapshot.state.wind, 3u * sizeof( float ) );
	frame.windGust[3] = snapshot.state.gustStrength;
	WeatherWeights( &snapshot.state, weights );
	memcpy( frame.precipitation, weights, 4u * sizeof( float ) );
	frame.dustAsh = weights[4];
	/* Coverage is already converted into the bounded active prefix by the
	 * portable planner; avoid paying a second full-pool thinning pass. */
	frame.indoorExposure	= 1.0f;
	frame.climateSeed		= snapshot.state.seed;
	frame.climate[0]		= snapshot.state.temperatureC;
	frame.climate[1]		= snapshot.state.humidity;
	frame.climate[2]		= snapshot.state.visibility;
	frame.climate[3]		= snapshot.state.transitionSeconds;
	frame.surfaceClimate[0] = snapshot.state.surfaceWetness;
	frame.surfaceClimate[1] = snapshot.state.surfaceFrost;
	frame.surfaceClimate[2] = snapshot.state.snowAccumulation;
	frame.surfaceClimate[3] = snapshot.state.meltRate;
	/* Weather rendering consumes the exact same sky revision as the visible
	 * background, fog and IBL.  The legacy scalar fields remain import-only. */
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
		3u * sizeof( float ) );
	frame.ambientCloud[3] = snapshot.state.cloudCover;
	frame.cloudMedia[0]	  = snapshot.state.skyEnvironment.cloudSunOcclusion;
	frame.cloudMedia[1]	  = snapshot.state.lightning;
	frame.cloudMedia[2]	  = snapshot.state.mediaDensity;
	frame.cloudMedia[3]	  = snapshot.state.mediaHeightFalloff;
	frame.renderParams[0] = 1.0f / (float)product->outputWidth;
	frame.renderParams[1] = 1.0f / (float)product->outputHeight;
	if ( !RenderSubmission_AtmosphereEffectGpuPayload( frontend, weather->semanticParticleCount,
													   weather->precipitationParticleCount, frame.effectWorkloads,
													   &effects ) ||
		 effects.admittedEmitterCount != weather->semanticEmitterCount ||
		 effects.admittedParticleCount != weather->semanticParticleCount )
		return qfalse;
	frame.effectMeta[0] = (float)weather->precipitationParticleCount;
	frame.effectMeta[1] = (float)weather->semanticEmitterCount;
	if ( weather->heightgridActive ) {
		renderAtmosphereHeightgrid_t grid;
		if ( !RenderSubmission_AtmosphereHeightgrid( frontend, &grid ) ) return qfalse;
		memcpy( frame.worldMins, grid.worldMins, sizeof(frame.worldMins) );
		memcpy( frame.worldMaxs, grid.worldMaxs, sizeof(frame.worldMaxs) );
		memcpy( frame.invGridStep, grid.inverseStep, sizeof(frame.invGridStep) );
		frame.gridSize = grid.edge;
	}
	readPool			= product->weatherParity ? 2u : 1u;
	writePool			= product->weatherParity ? 1u : 2u;
	product->gl.NamedBufferSubData( product->weatherBuffers[0], 0, sizeof( frame ), &frame );
	product->gl.UseProgram( product->weatherPrograms[0] );
	product->gl.BindBufferBase( GL_UNIFORM_BUFFER_VALUE, 0u, product->weatherBuffers[0] );
	product->gl.BindBufferBase( GL_SHADER_STORAGE_BUFFER_VALUE, 0u, product->weatherBuffers[readPool] );
	product->gl.BindBufferBase( GL_SHADER_STORAGE_BUFFER_VALUE, 1u, product->weatherBuffers[writePool] );
	product->gl.BindTextureUnit( 0u, weather->heightgridActive ? product->weatherHeightgrid : 0u );
	product->gl.DispatchCompute( ( weather->activeParticleCount + 63u ) / 64u, 1u, 1u );
	product->gl.MemoryBarrier( GL_SHADER_STORAGE_BARRIER_BIT_VALUE | GL_UNIFORM_BARRIER_BIT_VALUE );
	if ( product->gl.GetError() != GL_NO_ERROR_VALUE )
		return qfalse;
	product->weatherParity	 = product->weatherParity ? qfalse : qtrue;
	product->weatherTimeline = snapshot.state.timelineSeconds;
	*outFrame				 = frame;
	return qtrue;
}

qboolean RalOpenGl_ProductRender( ralOpenGlProduct_t *product, renderSubmissionState_t *frontend,
								  const renderSubmissionReceipt_t *submission, uint64_t frameGeneration,
								  ralOpenGlProductFrameReceipt_t *outReceipt )
{
	ralOpenGlProductFrameReceipt_t receipt;
	ralOpenGlWorldLowerInfo_t	   lower;
	renderEffectDecalPool_t decalPoolCandidate;
	renderEffectBeamPool_t beamPoolCandidate;
	renderWorldSnapshot_t effectView;
	renderEffectPrimitiveSnapshot_t effectSnapshot;
	const spriteDesc_t *effectSprites;
	const emitterDesc_t *effectEmitters;
	const decalDesc_t *effectDecals;
	const renderEffectRibbonCommand_t *effectRibbons;
	const ribbonPoint_t *effectRibbonPoints;
	const beamDesc_t *effectBeams;
	productWeatherFrame_t		   weatherFrame;
	renderDistantWorldSubmission_t distantWorld;
	qboolean distantWorldReady = qfalse;
	productSceneComplete_t sceneComplete = {0};
	ralOpenGlAaTaaFrame_t taaFrame;
	uint64_t taaPlanFrame = 0u;
	if ( !product || !frontend || !submission || !outReceipt || !frameGeneration || frameGeneration == UINT64_MAX ||
		 !product->outputWidth || !product->outputHeight || frameGeneration <= product->lastFrameGeneration ||
		 !RalOpenGl_CoreMatchesReceipt( product->core, &product->coreReceipt ) )
		return qfalse;
	memset( &receipt, 0, sizeof( receipt ) );
	receipt.schemaVersion			  = RAL_OPENGL_PRODUCT_SCHEMA_VERSION;
	receipt.backendType				  = RAL_BACKEND_OPENGL;
	receipt.productGeneration		  = product->generation;
	receipt.frameGeneration			  = frameGeneration;
	receipt.vertexShaderByteCount	  = ralOpenGlProductVertexByteCount;
	receipt.fragmentShaderByteCount	  = ralOpenGlProductFragmentByteCount;
	receipt.vertexShaderDigestLane0	  = ralOpenGlProductVertexDigestLane0;
	receipt.vertexShaderDigestLane1	  = ralOpenGlProductVertexDigestLane1;
	receipt.fragmentShaderDigestLane0 = ralOpenGlProductFragmentDigestLane0;
	receipt.fragmentShaderDigestLane1 = ralOpenGlProductFragmentDigestLane1;
	receipt.displayVisibility = product->displayVisibility;
	memset( &effectSnapshot, 0, sizeof( effectSnapshot ) );
	if ( !RenderSubmission_EffectPrimitiveSnapshots( frontend, &effectSnapshot,
			&effectSprites, &effectEmitters, &effectDecals, &effectRibbons,
			&effectRibbonPoints, &effectBeams ) ) return qfalse;
	(void)effectSprites; (void)effectEmitters; (void)effectDecals;
	(void)effectRibbons; (void)effectRibbonPoints;
	memset( &effectView, 0, sizeof( effectView ) );
	const qboolean hasEffectTimeline = RenderSubmission_ViewSnapshot( frontend,
		&effectView );
	if ( !RenderEffectDecalPool_Advance( &product->effectDecalPool,
			frameGeneration, hasEffectTimeline,
			hasEffectTimeline ? (float)effectView.timeMs * 0.001f : 0.0f,
			effectDecals, effectSnapshot.decalCount, &decalPoolCandidate,
			&receipt.effectDecalPool ) ) return qfalse;
	if ( !RenderEffectBeamPool_Advance( &product->effectBeamPool,
			frameGeneration, hasEffectTimeline,
			hasEffectTimeline ? (float)effectView.timeMs * 0.001f : 0.0f,
			effectBeams, effectSnapshot.beamCount, &beamPoolCandidate,
			&receipt.effectBeamPool ) ) return qfalse;
	if ( !RalOpenGl_FrontendPlanBuild( &product->coreReceipt, frontend,
			submission, frameGeneration, &decalPoolCandidate,
			&receipt.effectDecalPool, &beamPoolCandidate,
			&receipt.effectBeamPool, &receipt.plan ) )
		return qfalse;
	/* Reject a missing grid or failed immutable upload before the sky planner
	 * publishes this frame generation. The same sealed frame must be retryable. */
	{
		renderAtmosphereSnapshot_t snapshot;
		const atmosphereEmitter_t *emitters;
		if ( !RenderSubmission_AtmosphereSnapshot( frontend, &snapshot, &emitters ) ) return qfalse;
		if ( snapshot.active && frontend->sceneRendered
				&& (snapshot.state.flags & ATMOSPHERE_FLAG_HEIGHTGRID) ) {
			renderAtmosphereHeightgrid_t grid;
			if ( !RenderSubmission_AtmosphereHeightgrid( frontend, &grid )
				|| !SyncWeatherHeightgrid( product, &grid ) ) return qfalse;
		}
	}
	if ( !SyncMaterialTextures( product, frontend ) ) return qfalse;
	if ( !PlanAtmosphere( product, frontend, submission, frameGeneration, &receipt.atmosphere ) )
		return qfalse;
	receipt.skyGpuTelemetry = product->skyGpuTelemetry;
	memset( &distantWorld, 0, sizeof( distantWorld ) );
	if ( receipt.atmosphere.skyEnvironment.distantSceneActive
			&& !( effectView.rdflags & RDF_NOWORLDMODEL ) ) {
		distantWorldReady = RenderSubmission_DistantWorldSubmissionForActiveView(
			frontend, &receipt.atmosphere.skyEnvironment,
			RAL_SKY_DISTANT_MAX_LOD_LEVELS,
			RAL_SKY_DISTANT_MAX_VISIBLE_DRAWS,
			product->distantMetricScratch,
			RAL_SKY_DISTANT_MAX_CANDIDATES,
			product->distantCandidateScratch,
			RAL_SKY_DISTANT_MAX_CANDIDATES, &distantWorld );
		if ( !distantWorldReady ) return qfalse;
	}
	if ( receipt.atmosphere.computeDispatchCount != 0u &&
		 !EncodeAtmosphere( product, frontend, &receipt.atmosphere,
			&receipt.atmosphereNativeDispatchCount ) )
		return qfalse;
	receipt.atmosphereDispatchCount	 = receipt.atmosphere.computeDispatchCount;
	receipt.atmosphereFroxelCount	 = receipt.atmosphere.froxelCount;
	receipt.atmosphereCompositeCount = receipt.atmosphere.compositeCount;
	receipt.atmosphereCloudsActive	 = receipt.atmosphere.cloudsActive;
	if ( !PlanWeather( frontend, &receipt.atmosphere, &receipt.weather ) )
		return qfalse;
	memset( &weatherFrame, 0, sizeof( weatherFrame ) );
	if ( !receipt.weather.zeroWork ) {
		if ( !EncodeWeather( product, frontend, &receipt.weather, &weatherFrame ) )
			return qfalse;
		receipt.weatherDispatchCount = 1u;
		receipt.weatherDrawCount	 = 1u;
	}
	memset( &lower, 0, sizeof( lower ) );
	lower.generation		   = frameGeneration;
	lower.programName		   = product->program;
	lower.skyProgramName = product->skyMaterialProgram;
	lower.outputWidth		   = product->outputWidth;
	lower.outputHeight		   = product->outputHeight;
	lower.atmosphere		   = receipt.atmosphere;
	if ( !Ral_DisplayVisibilityPlanBuild( 1.0f, &lower.displayVisibility ) ) return qfalse;
	lower.aerialHaze = product->aerialHaze;
	lower.lightmapBoost = product->lightmapBoost;
	lower.atmosphereBufferName = product->atmosphereBuffers[receipt.atmosphere.computeDispatchCount != 0u
												? RAL_OPENGL_ATMOSPHERE_INTEGRATED
												: RAL_OPENGL_ATMOSPHERE_FALLBACK];
	lower.physicalSkyTextureName = product->skyResourcesReady
		? product->skyPhysicalTextures[RAL_OPENGL_SKY_VIEW] : 0u;
	if ( product->skyRadianceConsumer.ready ) {
		lower.currentSkyIrradianceTextureName = product->skyTextures[1];
		lower.previousSkyIrradianceTextureName =
			product->skyRadianceConsumer.usePreviousProduct
			? product->skyTextures[2] : product->skyTextures[1];
		lower.currentSkyRadianceTextureName = product->skyTextures[3];
		lower.previousSkyRadianceTextureName =
			product->skyRadianceConsumer.usePreviousProduct
			? product->skyTextures[4] : product->skyTextures[3];
		lower.skyRadianceConsumer = product->skyRadianceConsumer;
	}
	if ( !receipt.weather.zeroWork ) {
		lower.weatherProgramName		= product->weatherPrograms[1];
		lower.weatherUniformBufferName	= product->weatherBuffers[0];
		lower.weatherParticleBufferName = product->weatherBuffers[product->weatherParity ? 2u : 1u];
		lower.weatherParticleCount		= receipt.weather.activeParticleCount;
	}
	if ( product->hasDirectionalLighting )
		lower.directionalLighting = &product->directionalLighting;
	lower.materialTextureNames = product->materialTextureNames;
	lower.materialTextureCount = product->textureResourceCount;
	lower.distantWorld = distantWorldReady ? &distantWorld : NULL;
	lower.effectDecalPool = &decalPoolCandidate;
	lower.effectDecalPoolReceipt = receipt.effectDecalPool;
	lower.effectBeamPool = &beamPoolCandidate;
	lower.effectBeamPoolReceipt = receipt.effectBeamPool;
	if ( !PrepareHdr( product ) ) return qfalse;
	sceneComplete.product = product; sceneComplete.frontend = frontend;
	sceneComplete.view = &effectView; sceneComplete.receipt = &receipt;
	sceneComplete.worldDigest = submission->worldDigest;
	sceneComplete.sceneRendered = submission->sceneRendered
		&& !( effectView.rdflags & RDF_NOWORLDMODEL );
	/* Anti-aliasing plan. A plan left pending by a failed frame is cancelled first; leaving TAA drops its history. TAA jitters the
	 * world view (folded into the view axes by the world pass) and binds the coverage target as attachment 1 of the HDR scene. */
	if ( product->taaState.pending )
		(void)Ral_TemporalCancelFrame( &product->taaState, product->taaState.pendingPlan.frameId );
	if ( !product->taaEnabled && product->taaState.active ) Ral_TemporalInit( &product->taaState );
	RenderTemporal_ObserveCut( &product->taaCut, RenderSubmission_ViewCameraCut( frontend ) );
	memset( &taaFrame, 0, sizeof( taaFrame ) );
	{
		const qboolean anti = sceneComplete.sceneRendered && frontend->activeWorldIndex >= 0
			&& ( product->taaEnabled || product->smaaQuality > 0 );
		uint32_t coverage = 0u;
		if ( anti && !EnsureAa( product ) ) return qfalse;
		if ( anti && product->taaEnabled && RenderTemporal_PrimaryScene( effectView.rdflags ) && isfinite( effectView.fovX ) && isfinite( effectView.fovY )
				&& effectView.fovX > 1.0f && effectView.fovX < 179.0f && effectView.fovY > 1.0f && effectView.fovY < 179.0f ) {
			ralTemporalFrameInput_t input;
			ralTemporalFramePlan_t plan;
			const uint64_t lifecycle = frontend->worlds[frontend->activeWorldIndex].lifecycleGeneration;
			memset( &input, 0, sizeof( input ) );
			if ( product->taaWorldGeneration != lifecycle ) {
				product->taaWorldGeneration = lifecycle;
				if ( !++product->taaTopologyEpoch ) product->taaTopologyEpoch = 1u;
			}
			input.frameId = RenderSubmission_FrameSerial( frontend );
			input.width = product->outputWidth; input.height = product->outputHeight;
			input.topologyEpoch = product->taaTopologyEpoch; input.enabled = 1u;
			input.cameraCut = RenderTemporal_CutPending( &product->taaCut ) ? 1u : 0u;
			if ( Ral_TemporalBeginFrame( &product->taaState, &input, &plan ) ) {
				const float w = (float)product->outputWidth, h = (float)product->outputHeight;
				for ( int axis = 0; axis < 3; ++axis ) {
					taaFrame.current.origin[axis] = effectView.viewOrigin[axis];
					taaFrame.current.forward[axis] = effectView.viewAxis[0][axis];
					taaFrame.current.left[axis] = effectView.viewAxis[1][axis];
					taaFrame.current.up[axis] = effectView.viewAxis[2][axis];
				}
				taaFrame.current.projectionX = 1.0f / tanf( effectView.fovX * 0.00872664625997164788f );
				taaFrame.current.projectionY = 1.0f / tanf( effectView.fovY * 0.00872664625997164788f );
				taaFrame.previous = product->taaPreviousCamera;
				taaFrame.width = product->outputWidth; taaFrame.height = product->outputHeight;
				/* GL texture space has row 0 at NDC y = -1, so the image-space jitter maps to NDC and uv with the same sign. */
				taaFrame.jitterNdc[0] = 2.0f * plan.sceneJitterPixels[0] / w;
				taaFrame.jitterNdc[1] = 2.0f * plan.sceneJitterPixels[1] / h;
				taaFrame.currentJitterUv[0] = plan.sceneJitterPixels[0] / w;
				taaFrame.currentJitterUv[1] = plan.sceneJitterPixels[1] / h;
				taaFrame.previousJitterUv[0] = plan.previousJitterPixels[0] / w;
				taaFrame.previousJitterUv[1] = plan.previousJitterPixels[1] / h;
				taaFrame.historyValid = plan.historyValid ? qtrue : qfalse;
				taaFrame.readIndex = plan.historyReadIndex; taaFrame.writeIndex = plan.historyWriteIndex;
				coverage = RalOpenGl_AaCoverageTexture( product->aa, product->outputWidth, product->outputHeight );
				if ( !coverage ) return qfalse;
				lower.jitterNdc[0] = taaFrame.jitterNdc[0]; lower.jitterNdc[1] = taaFrame.jitterNdc[1];
				lower.coverageActive = qtrue;
				sceneComplete.taa = &taaFrame;
				taaPlanFrame = plan.frameId;
			}
		}
		sceneComplete.smaa = anti && product->smaaQuality > 0 ? qtrue : qfalse;
		if ( product->aa && !RalOpenGl_AaAttachCoverage( product->aa, product->hdrFramebuffer, coverage ) ) return qfalse;
	}
	product->gl.GetIntegerv( GL_DRAW_FRAMEBUFFER_BINDING_VALUE, &sceneComplete.drawFramebuffer );
	product->gl.GetIntegerv( GL_READ_FRAMEBUFFER_BINDING_VALUE, &sceneComplete.readFramebuffer );
	{
		glInt_t encoding = 0;
		product->gl.GetFramebufferAttachmentParameteriv( GL_DRAW_FRAMEBUFFER_VALUE,
			sceneComplete.drawFramebuffer ? GL_COLOR_ATTACHMENT0_VALUE : GL_BACK_LEFT_VALUE,
			GL_FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING_VALUE, &encoding );
		/* The product owns one sRGB output attachment; never silently present
		 * linear values to an encoded display or add an alternate gamma shader. */
		sceneComplete.outputSrgbEnabled = product->gl.IsEnabled( GL_FRAMEBUFFER_SRGB_VALUE );
		if ( product->gl.GetError() != GL_NO_ERROR_VALUE || encoding != GL_SRGB_VALUE )
			return qfalse;
	}
	product->gl.BindFramebuffer( GL_FRAMEBUFFER_VALUE, product->hdrFramebuffer );
	product->gl.Disable( GL_FRAMEBUFFER_SRGB_VALUE );
	lower.sceneComplete = CompleteHdrScene;
	lower.sceneCompleteContext = &sceneComplete;
	if ( !RalOpenGl_WorldLower( product->core, &product->coreReceipt, frontend, submission, &receipt.plan, &lower,
								&receipt.native ) ) {
		RestoreOutputFramebuffer( &sceneComplete );
		if ( sceneComplete.outputSrgbEnabled ) product->gl.Enable( GL_FRAMEBUFFER_SRGB_VALUE );
		else product->gl.Disable( GL_FRAMEBUFFER_SRGB_VALUE );
		return qfalse;
	}
	RestoreOutputFramebuffer( &sceneComplete );
	if ( sceneComplete.outputSrgbEnabled ) product->gl.Enable( GL_FRAMEBUFFER_SRGB_VALUE );
	else product->gl.Disable( GL_FRAMEBUFFER_SRGB_VALUE );
	if ( !sceneComplete.completed ) return qfalse;
	receipt.unresolvedCount = receipt.plan.unresolvedCount + receipt.native.unresolvedCount;
	receipt.fallbackCount	= receipt.plan.fallbackCount + receipt.native.fallbackCount;
	receipt.fatalCount		= receipt.plan.fatalCount + receipt.native.fatalCount;
	receipt.ready			= qtrue;
	if ( !FrameValid( &receipt ) )
		return qfalse;
	/* Publish only the completed frame's accumulator; rejected work writes the other slot. */
	if ( sceneComplete.metered ) {
		product->exposureParity ^= 1u;
		product->exposureTimeline = effectView.timeMs;
		product->exposureWorld = submission->worldDigest;
		product->exposureHistoryValid = qtrue;
	} else product->exposureHistoryValid = qfalse;
	product->effectDecalPool = decalPoolCandidate;
	product->effectBeamPool = beamPoolCandidate;
	product->lastFrameGeneration = frameGeneration;
	if ( taaPlanFrame && receipt.aaDispatchCount == 2u
			&& Ral_TemporalCommitFrame( &product->taaState, taaPlanFrame ) ) {
		RenderTemporal_CutConsumed( &product->taaCut );
		product->taaPreviousCamera = taaFrame.current;
		if ( taaFrame.historyValid ) product->taaResolvedFrames++; else product->taaBootstrapFrames++;
	}
	if ( receipt.aaDrawCount ) product->smaaFrames++;
	*outReceipt					 = receipt;
	return qtrue;
}
