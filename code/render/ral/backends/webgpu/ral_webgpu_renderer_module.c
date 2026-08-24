// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "ral_webgpu_renderer_module.h"
#include "ral_webgpu_browser_emscripten.h"
#include "maps/map_format_registry.h"

#include <limits.h>
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { WEBGPU_PIPELINE_WORLD, WEBGPU_PIPELINE_ENTITY,
	WEBGPU_PIPELINE_EFFECT, WEBGPU_PIPELINE_UI, WEBGPU_PIPELINE_COUNT };

typedef struct {
	refimport_t imports;
	refexport_t exports;
	ralWebGpuBrowserModuleBorrow_t borrow;
	ralWebGpuPipeline_t *pipelines[WEBGPU_PIPELINE_COUNT];
	ralWebGpuPipelineReceipt_t pipelineReceipts[WEBGPU_PIPELINE_COUNT];
	ralWebGpuProduct_t *product;
	renderSubmissionState_t frontend;
	ralWebGpuFrameReceipt_t pendingFrame;
	ralWebGpuRendererModuleFrameReceipt_t pending;
	ralWebGpuRendererModuleFrameReceipt_t published;
	glconfig_t config;
	const mapFile_t *loadedWorld;
	uint64_t moduleGeneration;
	uint64_t nextGeneration;
	uint64_t currentFrameGeneration;
	qhandle_t defaultMaterial;
	qboolean loaded;
	qboolean registered;
	qboolean frameOpen;
	qboolean pendingFrameReady;
	int failureStage;
	int logChannel;
} wiredWebGpuModuleState_t;

static wiredWebGpuModuleState_t s_module;
static uint64_t s_moduleCounter;
refimport_t ri;

static const byte s_white[4] = { 255u, 255u, 255u, 255u };
static const char s_vertexWgsl[] =
	"struct VertexOut { @builtin(position) position: vec4<f32>,"
	" @location(0) color: vec4<f32> };"
	"@vertex fn main(@location(0) p: vec3<f32>, @location(1) clipW: f32,"
	" @location(2) color: vec4<f32>) -> VertexOut {"
	" var out: VertexOut; out.position = vec4<f32>(p, clipW);"
	" out.color = color; return out; }\n";
static const char s_fragmentWgsl[] =
	"@fragment fn main(@location(0) color: vec4<f32>)"
	" -> @location(0) vec4<f32> { return color; }\n";
static const char s_uiVertexWgsl[] =
	"struct VertexOut { @builtin(position) position: vec4<f32>,"
	" @location(0) color: vec4<f32> };"
	"@vertex fn main(@location(0) p: vec3<f32>,"
	" @location(1) color: vec4<f32>) -> VertexOut {"
	" var out: VertexOut; out.position = vec4<f32>(p, 1.0);"
	" out.color = color; return out; }\n";
static const char s_uiFragmentWgsl[] =
	"@fragment fn main(@location(0) color: vec4<f32>)"
	" -> @location(0) vec4<f32> { return color; }\n";

static uint64_t NextGeneration( void ) {
	if ( s_module.nextGeneration >= UINT64_MAX - 1u ) return 0u;
	return ++s_module.nextGeneration;
}

static void MarkFailed( const char *reason ) {
	if ( !s_module.exports.initFailed && s_module.imports.LogCh
			&& s_module.logChannel >= 0 )
		s_module.imports.LogCh( s_module.logChannel, SEV_WARN,
			"Wired browser WebGPU RAL: lifecycle failure (%s, stage=%d)\n",
			reason, s_module.failureStage );
	s_module.exports.initFailed = qtrue;
}

static qboolean ReceiptValid(
		const ralWebGpuRendererModuleFrameReceipt_t *receipt ) {
	return receipt
		&& receipt->schemaVersion == RAL_WEBGPU_RENDERER_MODULE_SCHEMA_VERSION
		&& receipt->backendType == RAL_BACKEND_WEBGPU
		&& receipt->moduleGeneration && receipt->frameGeneration
		&& RenderSubmission_ReceiptExact( &receipt->frontend,
			&receipt->frontend )
		&& RalWebGpu_ProductFrameReceiptExact( &receipt->product,
			&receipt->product )
		&& receipt->frontend.ownerGeneration == receipt->moduleGeneration
		&& receipt->frontend.frameGeneration == receipt->frameGeneration
		&& receipt->product.frameGeneration == receipt->frameGeneration
		&& receipt->presented == qtrue && receipt->ready == qtrue;
}

Q_EXPORT qboolean RalWebGpu_RendererModuleFrameReceiptExact(
		const ralWebGpuRendererModuleFrameReceipt_t *a,
		const ralWebGpuRendererModuleFrameReceipt_t *b ) {
	return ReceiptValid( a ) && ReceiptValid( b )
		&& !memcmp( a, b, sizeof( *a ) );
}

Q_EXPORT qboolean WiredWebGpu_GetFrameReceipt(
		ralWebGpuRendererModuleFrameReceipt_t *outReceipt ) {
	if ( !outReceipt || !ReceiptValid( &s_module.published ) ) return qfalse;
	*outReceipt = s_module.published; return qtrue;
}

static qboolean Artifact( ralShaderArtifactAbi_t *artifact,
		ralShaderArtifactTarget_t target, const void *bytes, uint32_t byteCount ) {
	memset( artifact, 0, sizeof( *artifact ) ); artifact->target = target;
	if ( !bytes ) return qtrue;
	artifact->byteCount = byteCount;
	return Ral_ShaderArtifactDigest( bytes, byteCount, &artifact->digest )
		== ralSuccess;
}

static qboolean ShaderModule( ralShaderModuleAbi_t *module, uint32_t stage,
		const uint32_t spirv[4], const char *wgsl, uint32_t wgslBytes,
		uint64_t generation ) {
	memset( module, 0, sizeof( *module ) ); module->stage = stage;
	module->entryPoint = "main"; module->sourceDigest.lane0 = generation;
	module->sourceDigest.lane1 = generation + 1u;
	return Artifact( &module->artifacts[RAL_SHADER_ARTIFACT_SPIRV],
			RAL_SHADER_ARTIFACT_SPIRV, spirv, 16u )
		&& Artifact( &module->artifacts[RAL_SHADER_ARTIFACT_MSL],
			RAL_SHADER_ARTIFACT_MSL, NULL, 0u )
		&& Artifact( &module->artifacts[RAL_SHADER_ARTIFACT_WGSL],
			RAL_SHADER_ARTIFACT_WGSL, wgsl, wgslBytes );
}

static qboolean CreatePipeline( uint32_t index ) {
	uint32_t spirv[2][4] = { { 0x07230203u }, { 0x07230203u } };
	ralShaderModuleAbi_t modules[2];
	ralShaderVertexInputAbi_t shaderInputs[3];
	ralShaderAbiManifest_t manifest;
	ralShaderVariantAbi_t variant;
	ralWebGpuWgslModule_t wgsl[2];
	ralVertexBinding_t binding;
	ralVertexAttribute_t attributes[3];
	ralColorBlendAttachment_t blend;
	ralGraphicsPipelineCreateInfo_t graphics;
	ralWebGpuPipelineCreateInfo_t info;
	const char *vertexWgsl = index == WEBGPU_PIPELINE_UI
		? s_uiVertexWgsl : s_vertexWgsl;
	const char *fragmentWgsl = index == WEBGPU_PIPELINE_UI
		? s_uiFragmentWgsl : s_fragmentWgsl;
	uint32_t vertexWgslBytes = (uint32_t)( index == WEBGPU_PIPELINE_UI
		? sizeof( s_uiVertexWgsl ) - 1u : sizeof( s_vertexWgsl ) - 1u );
	uint32_t fragmentWgslBytes = (uint32_t)( index == WEBGPU_PIPELINE_UI
		? sizeof( s_uiFragmentWgsl ) - 1u : sizeof( s_fragmentWgsl ) - 1u );
	uint32_t vertexInputCount = index == WEBGPU_PIPELINE_UI ? 2u : 3u;
	uint64_t generation = NextGeneration();
	if ( index >= WEBGPU_PIPELINE_COUNT || !generation
			|| !ShaderModule( &modules[0], RAL_STAGE_VERTEX, spirv[0],
				vertexWgsl, vertexWgslBytes, generation )
			|| !ShaderModule( &modules[1], RAL_STAGE_FRAGMENT, spirv[1],
				fragmentWgsl, fragmentWgslBytes,
				generation + 2u ) ) return qfalse;
	memset( shaderInputs, 0, sizeof( shaderInputs ) );
	shaderInputs[0].location = 0u;
	shaderInputs[0].format = RAL_FORMAT_R32G32B32_SFLOAT;
	shaderInputs[1].location = 1u;
	shaderInputs[1].format = index == WEBGPU_PIPELINE_UI
		? RAL_FORMAT_R8G8B8A8_UNORM : RAL_FORMAT_R32_SFLOAT;
	shaderInputs[2].location = 2u;
	shaderInputs[2].format = RAL_FORMAT_R8G8B8A8_UNORM;
	memset( &manifest, 0, sizeof( manifest ) );
	manifest.schemaVersion = RAL_SHADER_ABI_SCHEMA_VERSION;
	manifest.generation = generation; manifest.pipelineKind = RAL_SHADER_PIPELINE_GRAPHICS;
	manifest.modules = modules; manifest.moduleCount = 2u;
	manifest.vertexInputs = shaderInputs;
	manifest.vertexInputCount = vertexInputCount;
	wgsl[0] = (ralWebGpuWgslModule_t){ vertexWgsl, vertexWgslBytes };
	wgsl[1] = (ralWebGpuWgslModule_t){ fragmentWgsl, fragmentWgslBytes };
	binding = (ralVertexBinding_t){ 0u, sizeof( renderWorldVertex_t ),
		RAL_VERTEX_INPUT_PER_VERTEX };
	attributes[0] = (ralVertexAttribute_t){ 0u, 0u,
		RAL_FORMAT_R32G32B32_SFLOAT, 0u };
	attributes[1] = index == WEBGPU_PIPELINE_UI
		? (ralVertexAttribute_t){ 1u, 0u, RAL_FORMAT_R8G8B8A8_UNORM,
			(uint32_t)offsetof( renderWorldVertex_t, color ) }
		: (ralVertexAttribute_t){ 1u, 0u, RAL_FORMAT_R32_SFLOAT,
			(uint32_t)offsetof( renderWorldVertex_t, texCoord ) };
	attributes[2] = (ralVertexAttribute_t){ 2u, 0u,
		RAL_FORMAT_R8G8B8A8_UNORM,
		(uint32_t)offsetof( renderWorldVertex_t, color ) };
	memset( &blend, 0, sizeof( blend ) ); blend.writeMask = RAL_COLOR_WRITE_ALL;
	blend.writeMaskExplicit = qtrue;
	memset( &graphics, 0, sizeof( graphics ) );
	graphics.vertexBindings = &binding; graphics.numVertexBindings = 1u;
	graphics.vertexAttributes = attributes;
	graphics.numVertexAttributes = vertexInputCount;
	graphics.topology = RAL_TOPOLOGY_TRIANGLE_LIST;
	graphics.raster.polygonMode = RAL_POLYGON_FILL; graphics.raster.lineWidth = 1.0f;
	graphics.colorBlends = &blend; graphics.numColorBlends = 1u;
	graphics.colorFormats[0] = RAL_FORMAT_B8G8R8A8_UNORM;
	graphics.numColorFormats = 1u; graphics.sampleCount = 1u;
	memset( &variant, 0, sizeof( variant ) );
	variant.artifactTarget = RAL_SHADER_ARTIFACT_WGSL;
	variant.generation = generation;
	if ( Ral_GraphicsPipelineSemanticDigest( &graphics,
			&variant.semanticStateDigest ) != ralSuccess ) return qfalse;
	memset( &info, 0, sizeof( info ) ); info.manifest = &manifest;
	info.variant = &variant; info.modules = wgsl; info.moduleCount = 2u;
	info.graphicsState = &graphics; info.generation = generation;
	return RalWebGpu_BrowserBridgeApplyPipelineInfo( s_module.borrow.bridge,
			&info )
		&& RalWebGpu_RuntimeCreatePipeline( s_module.borrow.runtime,
			&s_module.borrow.runtimeReceipt, &info, &s_module.pipelines[index],
			&s_module.pipelineReceipts[index] )
		&& RalWebGpu_RuntimeGetReceipt( s_module.borrow.runtime,
			&s_module.borrow.runtimeReceipt );
}

static void DestroyOwners( void ) {
	if ( s_module.product ) RalWebGpu_ProductDestroy( s_module.product,
		&s_module.borrow.runtimeReceipt );
	s_module.product = NULL;
	for ( uint32_t i = WEBGPU_PIPELINE_COUNT; i > 0u; --i ) {
		uint32_t index = i - 1u;
		if ( s_module.pipelines[index] ) {
			(void)RalWebGpu_RuntimeDestroyPipeline( s_module.borrow.runtime,
				s_module.pipelines[index], &s_module.pipelineReceipts[index] );
			(void)RalWebGpu_RuntimeGetReceipt( s_module.borrow.runtime,
				&s_module.borrow.runtimeReceipt );
		}
	}
	memset( s_module.pipelines, 0, sizeof( s_module.pipelines ) );
	memset( s_module.pipelineReceipts, 0,
		sizeof( s_module.pipelineReceipts ) );
}

static qboolean InitializeOwners( void ) {
	if ( !RalWebGpu_BrowserModuleBorrow( &s_module.borrow ) ) return qfalse;
	for ( uint32_t i = 0u; i < WEBGPU_PIPELINE_COUNT; ++i )
		if ( !CreatePipeline( i ) ) { DestroyOwners(); return qfalse; }
	if ( !RalWebGpu_ProductCreate( s_module.borrow.runtime,
			&s_module.borrow.runtimeReceipt,
			&s_module.pipelineReceipts[WEBGPU_PIPELINE_WORLD],
			s_module.moduleGeneration, &s_module.product )
			|| !RalWebGpu_ProductSetEntityPipeline( s_module.product,
				&s_module.borrow.runtimeReceipt,
				&s_module.pipelineReceipts[WEBGPU_PIPELINE_ENTITY] )
			|| !RalWebGpu_ProductSetEffectPipeline( s_module.product,
				&s_module.borrow.runtimeReceipt,
				&s_module.pipelineReceipts[WEBGPU_PIPELINE_EFFECT] )
			|| !RalWebGpu_ProductSetUiPipelines( s_module.product,
				&s_module.borrow.runtimeReceipt,
				&s_module.pipelineReceipts[WEBGPU_PIPELINE_UI],
				&s_module.pipelineReceipts[WEBGPU_PIPELINE_UI] )
			|| !RalWebGpu_ProductSetViewport( s_module.product,
				&s_module.borrow.runtimeReceipt, s_module.borrow.width,
				s_module.borrow.height ) ) {
		DestroyOwners(); return qfalse;
	}
	memset( &s_module.config, 0, sizeof( s_module.config ) );
	(void)snprintf( s_module.config.renderer_string,
		sizeof( s_module.config.renderer_string ), "Wired browser WebGPU RAL" );
	(void)snprintf( s_module.config.vendor_string,
		sizeof( s_module.config.vendor_string ), "%s",
		s_module.borrow.runtimeReceipt.core.caps.vendorName );
	(void)snprintf( s_module.config.version_string,
		sizeof( s_module.config.version_string ), "WebGPU RAL module schema %u",
		RAL_WEBGPU_RENDERER_MODULE_SCHEMA_VERSION );
	s_module.config.vidWidth = (int)s_module.borrow.width;
	s_module.config.vidHeight = (int)s_module.borrow.height;
	s_module.config.vidWidthLogical = s_module.config.vidWidth;
	s_module.config.vidHeightLogical = s_module.config.vidHeight;
	s_module.config.windowAspect = (float)s_module.config.vidWidth
		/ (float)s_module.config.vidHeight;
	s_module.config.maxTextureSize = (int)s_module.borrow.runtimeReceipt.core.caps.maxTextureDimension2D;
	s_module.config.numTextureUnits = (int)s_module.borrow.runtimeReceipt.core.caps.maxSampledTexturesPerShaderStage;
	s_module.config.colorBits = 32; s_module.config.depthBits = 0;
	s_module.config.stencilBits = 0; s_module.config.driverType = GLDRV_ICD;
	s_module.config.hardwareType = GLHW_GENERIC;
	s_module.config.deviceSupportsGamma = qfalse;
	s_module.config.textureCompression = TC_NONE;
	return qtrue;
}

static qhandle_t RegisterMaterial( renderAssetKind_t kind, const char *name,
		qboolean clamp ) {
	qhandle_t existing;
	if ( !name || !name[0] ) return 0;
	existing = RenderSubmission_MaterialHandle( &s_module.frontend, name );
	if ( existing ) return existing;
	return RenderSubmission_RegisterMaterialImage( &s_module.frontend, kind,
		name, clamp, s_white, 1u, 1u );
}

static qhandle_t RegisterModel( const char *name ) {
	void *bytes = NULL; renderModelSnapshot_t model; qhandle_t handle; int count;
	if ( !name || !name[0] ) return 0;
	if ( name[0] == '*' && s_module.loadedWorld ) {
		char *end = NULL; long index = strtol( name + 1, &end, 10 );
		if ( end != name + 1 && *end == '\0' && index > 0
				&& index < s_module.loadedWorld->numSubModels ) {
			const dmodel_t *source = &s_module.loadedWorld->subModels[index];
			return RenderSubmission_RegisterInlineModel( &s_module.frontend, name,
				(uint32_t)source->firstSurface, (uint32_t)source->numSurfaces );
		}
	}
	if ( !ri.FS_ReadFile || !ri.FS_FreeFile ) return 0;
	count = ri.FS_ReadFile( name, &bytes );
	if ( count <= 0 || !bytes ) { if ( bytes ) ri.FS_FreeFile( bytes ); return 0; }
	handle = RenderSubmission_RegisterModelData( &s_module.frontend, name,
		bytes, (uint32_t)count ); ri.FS_FreeFile( bytes );
	if ( !handle || !RenderSubmission_ModelSnapshot( &s_module.frontend,
			handle, &model ) ) return 0;
	for ( uint32_t i = 0u; i < model.batchCount; ++i ) {
		qhandle_t material = RegisterMaterial( RENDER_ASSET_MATERIAL,
			model.batches[i].materialName, qfalse );
		if ( !material || !RenderSubmission_SetModelBatchMaterial(
				&s_module.frontend, handle, i, material ) ) return 0;
	}
	return handle;
}

static qhandle_t RegisterSkin( const char *name ) {
	return RenderSubmission_RegisterAsset( &s_module.frontend,
		RENDER_ASSET_SKIN, name );
}
static qhandle_t RegisterShader( const char *name ) {
	return RegisterMaterial( RENDER_ASSET_MATERIAL, name, qfalse );
}
static qhandle_t RegisterShaderNoMip( const char *name ) {
	return RegisterMaterial( RENDER_ASSET_MATERIAL, name, qtrue );
}
static qhandle_t RegisterShaderLightMap( const char *name, int lightmap ) {
	(void)lightmap; return RegisterMaterial( RENDER_ASSET_LIGHTMAP, name, qtrue );
}
static qhandle_t RegisterMsdf( const char *name, float range, int w, int h ) {
	(void)range; (void)w; (void)h;
	return RegisterMaterial( RENDER_ASSET_MSDF, name, qtrue );
}

static qboolean PrepareWorldMaterials( const mapFile_t *bsp ) {
	byte *rgba = NULL; uint32_t side = 0u; uint64_t pixels;
	if ( !bsp || bsp->numShaders < 0 || bsp->numLightmapPages < 0
			|| bsp->lightmapPageSize < 0
			|| ( bsp->numShaders && !bsp->shaders ) ) return qfalse;
	for ( int i = 0; i < bsp->numShaders; ++i )
		if ( !RegisterShader( bsp->shaders[i].shader ) ) return qfalse;
	if ( bsp->numLightmapPages == 0 ) return qtrue;
	if ( !bsp->lightmapData || bsp->lightmapPageSize <= 0
			|| ( bsp->lightmapPageSize % 3 ) != 0 ) return qfalse;
	pixels = (uint32_t)bsp->lightmapPageSize / 3u;
	for ( side = 1u; (uint64_t)side * side < pixels; ++side )
		if ( side >= RENDER_SUBMISSION_MAX_IMAGE_DIMENSION ) return qfalse;
	if ( (uint64_t)side * side != pixels ) return qfalse;
	rgba = malloc( (size_t)pixels * 4u ); if ( !rgba ) return qfalse;
	for ( int page = 0; page < bsp->numLightmapPages; ++page ) {
		char name[MAX_QPATH]; const byte *rgb = bsp->lightmapData
			+ (size_t)page * (size_t)bsp->lightmapPageSize;
		for ( uint64_t i = 0u; i < pixels; ++i ) {
			rgba[i * 4u] = rgb[i * 3u]; rgba[i * 4u + 1u] = rgb[i * 3u + 1u];
			rgba[i * 4u + 2u] = rgb[i * 3u + 2u]; rgba[i * 4u + 3u] = 255u;
		}
		if ( !RenderSubmission_LightmapMaterialName( name,
				(uint32_t)bsp->checksum, (uint32_t)page )
				|| !RenderSubmission_RegisterMaterialImage( &s_module.frontend,
					RENDER_ASSET_LIGHTMAP, name, qtrue, rgba, side, side ) ) {
			free( rgba ); return qfalse;
		}
	}
	free( rgba ); return qtrue;
}

static void LoadWorld( const mapFile_t *bsp, int worldIndex ) {
	if ( !PrepareWorldMaterials( bsp )
			|| !RenderSubmission_LoadWorld( &s_module.frontend, bsp, worldIndex ) )
		MarkFailed( "frontend-world" );
	else s_module.loadedWorld = bsp;
}

static void BeginRegistration( glconfig_t *config ) {
	if ( !config || !s_module.loaded || s_module.frameOpen
			|| s_module.pendingFrameReady ) { MarkFailed( "registration-state" ); return; }
	if ( !s_module.product && !InitializeOwners() ) {
		MarkFailed( "browser-runtime-not-ready" ); return;
	}
	if ( !s_module.frontend.initialized
			&& !RenderSubmission_Init( &s_module.frontend,
				s_module.moduleGeneration ) ) { MarkFailed( "frontend-init" ); return; }
	if ( !s_module.defaultMaterial )
		s_module.defaultMaterial = RegisterShaderNoMip( "*white" );
	if ( !s_module.defaultMaterial ) { MarkFailed( "default-material" ); return; }
	if ( s_module.imports.CL_SetScaling )
		s_module.imports.CL_SetScaling( 1.0f, s_module.config.vidWidth,
			s_module.config.vidHeight );
	s_module.registered = qtrue; *config = s_module.config;
}

static void EndRegistration( void ) {
	if ( !s_module.registered || s_module.frameOpen ) MarkFailed( "registration-end" );
}

static void BeginFrame( stereoFrame_t stereo ) {
	uint64_t generation = NextGeneration(); (void)stereo;
	if ( !generation || !s_module.registered || s_module.frameOpen
			|| s_module.pendingFrameReady
			|| !RenderSubmission_BeginFrame( &s_module.frontend, generation ) ) {
		MarkFailed( "frame-begin" ); return;
	}
	s_module.currentFrameGeneration = generation; s_module.frameOpen = qtrue;
}

static void EndFrame( int *frontEndMsec, int *backEndMsec ) {
	ralWebGpuRendererModuleFrameReceipt_t pending;
	ralWebGpuFrontendPlanReceipt_t plan;
	if ( frontEndMsec ) *frontEndMsec = 0;
	if ( backEndMsec ) *backEndMsec = 0;
	if ( !s_module.frameOpen ) { MarkFailed( "frame-end" ); return; }
	memset( &pending, 0, sizeof( pending ) );
	pending.schemaVersion = RAL_WEBGPU_RENDERER_MODULE_SCHEMA_VERSION;
	pending.backendType = RAL_BACKEND_WEBGPU;
	pending.moduleGeneration = s_module.moduleGeneration;
	pending.frameGeneration = s_module.currentFrameGeneration;
	if ( !RenderSubmission_EndFrame( &s_module.frontend,
			pending.frameGeneration, &pending.frontend ) ) {
		s_module.failureStage = 41; s_module.frameOpen = qfalse;
		MarkFailed( "frontend-frame" ); return;
	}
	s_module.frameOpen = qfalse; s_module.currentFrameGeneration = 0u;
	/* Loading and registration can seal UI-only frames after a world is loaded,
	 * before the client has submitted its first RenderScene.  The retained
	 * world snapshot is intentionally readable only for a rendered scene, so
	 * defer product lowering until that first scene frame instead of treating
	 * the expected transition frame as a fatal backend error. */
	if ( !pending.frontend.worldLoaded || !pending.frontend.sceneRendered ) return;
	if ( !RalWebGpu_ProductPlan( s_module.product,
			&s_module.borrow.runtimeReceipt, &s_module.frontend,
			&pending.frontend, &plan ) ) {
		s_module.failureStage = 500
			+ (int)RalWebGpu_FrontendPlanLastFailureStage();
		MarkFailed( "frontend-plan" ); return;
	}
	if ( !RalWebGpu_PresentationAcquire( s_module.borrow.presentation,
			&s_module.borrow.configuredReceipt, &s_module.pendingFrame ) ) {
		s_module.failureStage = 42; MarkFailed( "presentation-acquire" ); return;
	}
	if ( !RalWebGpu_ProductRenderPlan( s_module.product,
			&s_module.borrow.runtimeReceipt, &s_module.frontend,
			&pending.frontend, &plan,
			s_module.pendingFrame.textureIdentity,
			pending.frameGeneration, &pending.product ) ) {
		s_module.failureStage = 700
			+ (int)RalWebGpu_ProductLastFailureStage();
		MarkFailed( "product-frame" ); return;
	}
	s_module.pending = pending; s_module.pendingFrameReady = qtrue;
}

Q_EXPORT ralWebGpuAsyncStatus_t WiredWebGpu_RendererPoll( void ) {
	ralWebGpuAsyncStatus_t status;
	if ( !s_module.pendingFrameReady ) return RAL_WEBGPU_ASYNC_READY;
	status = RalWebGpu_ProductPoll( s_module.product, &s_module.pending.product );
	if ( status != RAL_WEBGPU_ASYNC_READY ) {
		if ( status == RAL_WEBGPU_ASYNC_FAILED ) MarkFailed( "product-poll" );
		return status;
	}
	if ( !RalWebGpu_PresentationPresent( s_module.borrow.presentation,
			&s_module.pendingFrame, &s_module.pending.product.submission ) ) {
		MarkFailed( "present" ); return RAL_WEBGPU_ASYNC_FAILED;
	}
	s_module.pending.presented = qtrue; s_module.pending.ready = qtrue;
	if ( !ReceiptValid( &s_module.pending ) ) {
		MarkFailed( "published-receipt" ); return RAL_WEBGPU_ASYNC_FAILED;
	}
	s_module.published = s_module.pending;
	memset( &s_module.pending, 0, sizeof( s_module.pending ) );
	memset( &s_module.pendingFrame, 0, sizeof( s_module.pendingFrame ) );
	s_module.pendingFrameReady = qfalse; return RAL_WEBGPU_ASYNC_READY;
}

static void Shutdown( refShutdownCode_t code ) {
	if ( !s_module.loaded ) return;
	if ( s_module.frameOpen ) RenderSubmission_CancelFrame( &s_module.frontend );
	s_module.frameOpen = qfalse; s_module.currentFrameGeneration = 0u;
	s_module.loadedWorld = NULL;
	if ( code == REF_LEVEL_ONLY ) return;
	if ( s_module.pendingFrameReady
			&& WiredWebGpu_RendererPoll() != RAL_WEBGPU_ASYNC_READY ) {
		MarkFailed( "shutdown-in-flight" ); return;
	}
	DestroyOwners(); RenderSubmission_Reset( &s_module.frontend );
	memset( &s_module.borrow, 0, sizeof( s_module.borrow ) );
	memset( &s_module.published, 0, sizeof( s_module.published ) );
	s_module.registered = qfalse; s_module.loaded = qfalse;
}

static void ClearScene( void ) { if ( s_module.frameOpen
	&& !RenderSubmission_ClearScene( &s_module.frontend ) ) MarkFailed( "clear" ); }
static void AddEntity( const refEntity_t *e, qboolean t ) { (void)t; if ( s_module.frameOpen
	&& e && !RenderSubmission_AddEntity( &s_module.frontend, e, NULL ) ) MarkFailed( "entity" ); }
static void AddEntityTemporal( const refEntity_t *e, const refEntityMotion_t *m ) {
	if ( s_module.frameOpen && e && RefEntityMotion_IsValid( m )
		&& !RenderSubmission_AddEntity( &s_module.frontend, e, m ) ) MarkFailed( "temporal" ); }
static void AddPoly( qhandle_t h,int n,const polyVert_t*v,int c ) { if ( s_module.frameOpen
	&& !RenderSubmission_AddPoly( &s_module.frontend,h,n,v,c ) ) MarkFailed( "poly" ); }
static void AddLight( const vec3_t o,float i,float r,float g,float b ) { if ( s_module.frameOpen
	&& !RenderSubmission_AddLight( &s_module.frontend,o,NULL,i,r,g,b ) ) MarkFailed( "light" ); }
static void AddLinearLight( const vec3_t a,const vec3_t b,float i,float r,float g,float bl ) { if ( s_module.frameOpen
	&& !RenderSubmission_AddLight( &s_module.frontend,a,b,i,r,g,bl ) ) MarkFailed( "linear-light" ); }
static void RenderScene( const refdef_t *v,int w ) { if ( s_module.frameOpen
	&& !RenderSubmission_RenderScene( &s_module.frontend,v,w ) ) MarkFailed( "scene" ); }
static void SetColor( const float *c ) { if ( !RenderSubmission_SetColor( &s_module.frontend,c ) ) MarkFailed( "color" ); }
static void DrawPic( float x,float y,float w,float h,float s1,float t1,float s2,float t2,qhandle_t m ) { if ( s_module.frameOpen
	&& !RenderSubmission_AddUiQuad( &s_module.frontend,x,y,w,h,s1,t1,s2,t2,0,m ) ) MarkFailed( "ui" ); }
static void DrawRotatedPic( float x,float y,float w,float h,float s1,float t1,float s2,float t2,float a,qhandle_t m ) { if ( s_module.frameOpen
	&& !RenderSubmission_AddUiQuad( &s_module.frontend,x,y,w,h,s1,t1,s2,t2,a,m ) ) MarkFailed( "ui-rotated" ); }
static void DrawLine( float x1,float y1,float x2,float y2,float w,qhandle_t m ) { if ( s_module.frameOpen
	&& !RenderSubmission_AddUiLine( &s_module.frontend,x1,y1,x2,y2,w,m ) ) MarkFailed( "ui-line" ); }
static void DrawBackdrop( float x,float y,float w,float h,float t,float mx,float my,float q ) { DrawPic(x,y,w,h,t,mx,my,q,s_module.defaultMaterial); }

static void NoVoid(void){} static void NoBool(qboolean v){(void)v;} static void NoHandle(qhandle_t v){(void)v;}
static void NoBytes(const byte*v){(void)v;} static int NoLight(vec3_t p,vec3_t a,vec3_t d,vec3_t n){(void)p;if(a)memset(a,0,sizeof(vec3_t));if(d)memset(d,0,sizeof(vec3_t));if(n)memset(n,0,sizeof(vec3_t));return 0;}
static void NoRibbon(const ribbonDesc_t*v){(void)v;} static void NoRail(const railRibbonDesc_t*v){(void)v;} static void NoBeam(const beamDesc_t*v){(void)v;} static void NoSprite(const spriteDesc_t*v){(void)v;} static void NoEmitter(const emitterDesc_t*v){(void)v;} static void NoDecal(const decalDesc_t*v){(void)v;} static void NoParticle(particleClassHandle_t h,const particleClass_t*v){(void)h;(void)v;} static void NoAtmosphere(const atmosphericDesc_t*v){(void)v;} static void NoHeightgrid(const float*v,int c){(void)v;(void)c;} static void NoLens(const lensSourceDesc_t*v){(void)v;} static qboolean NoLensVisibility(int i,float*v){(void)i;if(v)*v=0;return qfalse;}
static void NoClip(const float*v){(void)v;} static void NoOutline(float w,const float*c,float g,const float*gc){(void)w;(void)c;(void)g;(void)gc;} static void NoShadow(float x,float y,const float*c){(void)x;(void)y;(void)c;} static void NoRaw(int x,int y,int w,int h,int c,int r,byte*d,int n,qboolean q){(void)x;(void)y;(void)w;(void)h;(void)c;(void)r;(void)d;(void)n;(void)q;} static void NoUpload(int w,int h,int c,int r,byte*d,int n,qboolean q){(void)w;(void)h;(void)c;(void)r;(void)d;(void)n;(void)q;}
static int NoFragments(int n,const vec3_t*p,const vec3_t pr,int mp,vec3_t pb,int mf,markFragment_t*f){(void)n;(void)p;(void)pr;(void)mp;(void)pb;(void)mf;(void)f;return 0;} static int NoTag(orientation_t*t,qhandle_t m,int s,int e,float f,const char*n){(void)t;(void)m;(void)s;(void)e;(void)f;(void)n;return 0;} static void NoBounds(qhandle_t m,vec3_t a,vec3_t b){(void)m;if(a)memset(a,0,sizeof(vec3_t));if(b)memset(b,0,sizeof(vec3_t));} static void NoFont(const char*n,int s,fontInfo_t*f){(void)n;(void)s;if(f)memset(f,0,sizeof(*f));} static void NoRemap(const char*a,const char*b,const char*c){(void)a;(void)b;(void)c;} static qboolean NoToken(char*b,int s){if(b&&s>0)b[0]='\0';return qfalse;} static qboolean NoPvs(const vec3_t a,const vec3_t b){(void)a;(void)b;return qfalse;} static void NoVideo(int h,int w,byte*c,byte*e,qboolean m){(void)h;(void)w;(void)c;(void)e;(void)m;}
static qboolean CanMinimize(void){return qtrue;} static const glconfig_t *GetConfig(void){return s_module.product?&s_module.config:NULL;} static qboolean GetMemory(uint64_t*a,uint64_t*b,uint64_t*c,uint64_t*d,int*p){if(a)*a=0;if(b)*b=0;if(c)*c=0;if(d)*d=0;if(p)*p=0;return qfalse;} static int NoMdl(qhandle_t m,mdlAnimRange_t*a,int c){(void)m;(void)a;(void)c;return 0;} static void NoLightstyle(int s,const char*p){(void)s;(void)p;} static qboolean NoGpu(refGpuProfileSample_t*s){if(s)memset(s,0,sizeof(*s));return qfalse;}
static void PresentationChanged( const refPresentationChange_t *change ) {
	if ( !change || !s_module.product || s_module.pendingFrameReady
			|| !change->logicalWidth || !change->logicalHeight ) return;
	if ( !WiredWebGpu_RendererResize( change->logicalWidth,
			change->logicalHeight ) ) MarkFailed( "resize" );
}

Q_EXPORT int WiredWebGpu_RendererResize( uint32_t width, uint32_t height ) {
	if ( !width || !height || !s_module.product || s_module.pendingFrameReady
			|| !RalWebGpu_BrowserModuleResize( width, height )
			|| !RalWebGpu_BrowserModuleBorrow( &s_module.borrow )
			|| !RalWebGpu_ProductSetViewport( s_module.product,
				&s_module.borrow.runtimeReceipt, width, height ) ) return 0;
	s_module.config.vidWidth = (int)width;
	s_module.config.vidHeight = (int)height;
	s_module.config.vidWidthLogical = s_module.config.vidWidth;
	s_module.config.vidHeightLogical = s_module.config.vidHeight;
	return 1;
}

static void FillExports( refexport_t *e ) {
	memset(e,0,sizeof(*e)); e->Shutdown=Shutdown;e->BeginRegistration=BeginRegistration;e->RegisterModel=RegisterModel;e->RegisterSkin=RegisterSkin;e->RegisterShader=RegisterShader;e->RegisterShaderNoMip=RegisterShaderNoMip;e->RegisterShaderLightMap=RegisterShaderLightMap;e->RegisterMSDFShader=RegisterMsdf;e->RegisterPrimitiveShader=RegisterShader;e->PinShaderImages=NoHandle;e->LoadWorld=LoadWorld;e->SetWorldVisData=NoBytes;e->EndRegistration=EndRegistration;e->ClearScene=ClearScene;e->AddRefEntityToScene=AddEntity;e->AddPolyToScene=AddPoly;e->LightForPoint=NoLight;e->AddLightToScene=AddLight;e->AddAdditiveLightToScene=AddLight;e->AddLinearLightToScene=AddLinearLight;e->AddRibbonToScene=NoRibbon;e->AddRailRibbonToScene=NoRail;e->AddBeamToScene=NoBeam;e->AddSpriteToScene=NoSprite;e->EmitParticles=NoEmitter;e->AddDecalToScene=NoDecal;e->RegisterParticleClass=NoParticle;e->SetAtmosphere=NoAtmosphere;e->SetAtmosphereHeightgrid=NoHeightgrid;e->AddLensSourceToScene=NoLens;e->GetLensVisibility=NoLensVisibility;e->RenderScene=RenderScene;e->SetColor=SetColor;e->SetMSDFOutline=NoOutline;e->SetMSDFShadow=NoShadow;e->SetClipRegion=NoClip;e->DrawStretchPic=DrawPic;e->DrawMenuBackdrop=DrawBackdrop;e->DrawStretchPicOverlay=DrawPic;e->DrawRotatedPic=DrawRotatedPic;e->DrawLine=DrawLine;e->DrawStretchRaw=NoRaw;e->UploadCinematic=NoUpload;e->BeginFrame=BeginFrame;e->EndFrame=EndFrame;e->MarkFragments=NoFragments;e->LerpTag=NoTag;e->ModelBounds=NoBounds;e->RegisterFont=NoFont;e->RemapShader=NoRemap;e->GetEntityToken=NoToken;e->inPVS=NoPvs;e->TakeVideoFrame=NoVideo;e->ThrottleBackend=NoVoid;e->FinishBloom=NoVoid;e->SetColorMappings=NoVoid;e->CanMinimize=CanMinimize;e->GetConfig=GetConfig;e->GetMemoryBudget=GetMemory;e->VertexLighting=NoBool;e->SyncRender=NoVoid;e->GetMDLAnimations=NoMdl;e->SetLightstylePattern=NoLightstyle;e->GetGpuProfileSample=NoGpu;e->AddRefEntityToSceneTemporal=AddEntityTemporal;e->PresentationChanged=PresentationChanged;
}

Q_EXPORT refexport_t *QDECL GetRefAPI( int apiVersion, refimport_t *imports ) {
	if ( !imports || apiVersion != REF_API_VERSION || s_module.loaded
			|| s_moduleCounter >= UINT64_MAX - 1u ) return NULL;
	memset( &s_module, 0, sizeof( s_module ) ); s_module.imports = *imports;
	s_module.moduleGeneration = ++s_moduleCounter;
	s_module.nextGeneration = s_module.moduleGeneration; s_module.logChannel = -1;
	ri = *imports;
	if ( imports->GetLogChannel && imports->LogCh )
		s_module.logChannel = imports->GetLogChannel( "renderer.init" );
	FillExports( &s_module.exports ); s_module.loaded = qtrue;
	return &s_module.exports;
}

#ifdef RAL_WEBGPU_BROWSER_SMOKE
static qboolean s_rendererSmokeActive;

Q_EXPORT int WiredWebGpu_RendererSmokeBegin( void ) {
	static mapFile_t map;
	static dsurface_t surface;
	static drawVert_t vertices[3];
	static int indices[3] = { 0, 1, 2 };
	static dshader_t shader;
	refimport_t imports;
	refexport_t *renderer;
	refdef_t view;
	glconfig_t config;
	qhandle_t material;
	if ( s_rendererSmokeActive || s_module.loaded ) return 0;
	memset( &imports, 0, sizeof( imports ) );
	renderer = GetRefAPI( REF_API_VERSION, &imports );
	if ( !renderer ) return -1;
	memset( &config, 0, sizeof( config ) ); renderer->BeginRegistration( &config );
	if ( renderer->initFailed || config.vidWidth != 1280
			|| config.vidHeight != 720 ) return -2;
	memset( &map, 0, sizeof( map ) ); memset( &surface, 0, sizeof( surface ) );
	memset( vertices, 0, sizeof( vertices ) ); memset( &shader, 0, sizeof( shader ) );
	(void)snprintf( map.name, sizeof( map.name ), "maps/webgpu-renderer-smoke.bsp" );
	map.checksum = 0x11821; map.numSurfaces = 1; map.surfaces = &surface;
	map.numDrawVerts = 3; map.drawVerts = vertices;
	map.numDrawIndexes = 3; map.drawIndexes = indices;
	map.numShaders = 1; map.shaders = &shader;
	(void)snprintf( shader.shader, sizeof( shader.shader ), "textures/webgpu-smoke" );
	surface.surfaceType = MST_PLANAR; surface.shaderNum = 0;
	surface.firstVert = 0; surface.numVerts = 3;
	surface.firstIndex = 0; surface.numIndexes = 3; surface.lightmapNum = -1;
	vertices[0].xyz[0] = -600.0f; vertices[0].xyz[1] = -300.0f;
	vertices[1].xyz[0] = 600.0f; vertices[1].xyz[1] = -300.0f;
	vertices[2].xyz[1] = 500.0f;
	for ( uint32_t i = 0u; i < 3u; ++i ) memset( vertices[i].color.rgba, 255, 4u );
	renderer->LoadWorld( &map, 0 );
	material = renderer->RegisterShader( shader.shader );
	if ( !material || renderer->initFailed ) return -3;
	renderer->EndRegistration(); renderer->BeginFrame( STEREO_CENTER );
	renderer->ClearScene(); memset( &view, 0, sizeof( view ) );
	view.width = 1280; view.height = 720; view.fov_x = 90.0f; view.fov_y = 60.0f;
	view.viewaxis[0][0] = view.viewaxis[1][1] = view.viewaxis[2][2] = 1.0f;
	renderer->RenderScene( &view, 0 );
	renderer->DrawStretchPic( 32.0f, 32.0f, 192.0f, 96.0f,
		0.0f, 0.0f, 1.0f, 1.0f, material );
	renderer->EndFrame( NULL, NULL );
	if ( renderer->initFailed || !s_module.pendingFrameReady )
		return s_module.failureStage ? -s_module.failureStage : -4;
	s_rendererSmokeActive = qtrue; return 1;
}

Q_EXPORT int WiredWebGpu_RendererSmokePoll( void ) {
	ralWebGpuRendererModuleFrameReceipt_t receipt;
	ralWebGpuAsyncStatus_t status;
	if ( !s_rendererSmokeActive ) return 0;
	status = WiredWebGpu_RendererPoll();
	if ( status == RAL_WEBGPU_ASYNC_PENDING ) return 1;
	if ( status != RAL_WEBGPU_ASYNC_READY
			|| !WiredWebGpu_GetFrameReceipt( &receipt )
			|| receipt.product.worldDrawCount != 1u
			|| receipt.product.uiDrawCount != 1u
			|| receipt.product.unresolvedCount || receipt.product.fallbackCount
			|| receipt.product.fatalCount ) return -1;
	s_module.exports.Shutdown( REF_UNLOAD_DLL ); s_rendererSmokeActive = qfalse;
	return 2;
}
#else
Q_EXPORT int WiredWebGpu_RendererSmokeBegin( void ) { return 0; }
Q_EXPORT int WiredWebGpu_RendererSmokePoll( void ) { return 0; }
#endif
