// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "ral_webgpu_browser_emscripten.h"
#include "ral_webgpu_runtime.h"

#include <string.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>

typedef struct {
	ralWebGpuBrowserBridge_t *bridge;
	ralWebGpuRuntime_t *runtime;
	ralWebGpuRuntimeReceipt_t receipt;
	ralWebGpuPresentationReceipt_t configuredReceipt;
	uint32_t width, height;
	qboolean configured;
} browserModule_t;

static browserModule_t s_module;

EM_JS( int, WiredRalWebGpu_DispatchImport,
	( uint32_t opcode, const void *request, uint32_t requestBytes,
		void *response, uint32_t responseBytes ), {
	const dispatch = globalThis.wiredRalWebGpuDispatch;
	if (typeof dispatch !== "function") return 0;
	return dispatch(opcode, request, requestBytes, response, responseBytes) | 0;
} );

static qboolean Dispatch( void *userData, ralWebGpuBrowserOpcode_t opcode,
		const void *request, uint32_t requestBytes, void *response,
		uint32_t responseBytes ) {
	(void)userData;
	return WiredRalWebGpu_DispatchImport( (uint32_t)opcode, request, requestBytes,
		response, responseBytes ) ? qtrue : qfalse;
}

qboolean RalWebGpu_BrowserEmscriptenCreate( uint64_t generation,
		uint64_t canvasIdentity, ralWebGpuBrowserBridge_t **outBridge ) {
	ralWebGpuBrowserBridgeCreateInfo_t info;
	memset( &info, 0, sizeof( info ) );
	info.schemaVersion = RAL_WEBGPU_BROWSER_ABI_SCHEMA_VERSION;
	info.generation = generation; info.canvasIdentity = canvasIdentity;
	info.dispatch = Dispatch;
	return RalWebGpu_BrowserBridgeCreate( &info, outBridge );
}

qboolean RalWebGpu_BrowserModuleBorrow(
		ralWebGpuBrowserModuleBorrow_t *outBorrow ) {
	ralWebGpuPresentation_t *presentation;
	if ( !outBorrow || !s_module.runtime || !s_module.bridge
			|| !s_module.configured
			|| !RalWebGpu_RuntimeGetReceipt( s_module.runtime,
				&s_module.receipt ) ) return qfalse;
	presentation = RalWebGpu_RuntimePresentation( s_module.runtime,
		&s_module.receipt );
	if ( !presentation ) return qfalse;
	memset( outBorrow, 0, sizeof( *outBorrow ) );
	outBorrow->bridge = s_module.bridge;
	outBorrow->runtime = s_module.runtime;
	outBorrow->runtimeReceipt = s_module.receipt;
	outBorrow->presentation = presentation;
	outBorrow->configuredReceipt = s_module.configuredReceipt;
	outBorrow->width = s_module.width;
	outBorrow->height = s_module.height;
	return qtrue;
}

EMSCRIPTEN_KEEPALIVE int RalWebGpu_BrowserModuleStart( uint32_t generation,
		uint32_t canvasIdentity, uint32_t width, uint32_t height ) {
	ralWebGpuRuntimeCreateInfo_t info;
	if ( !generation || !canvasIdentity || s_module.runtime || s_module.bridge )
		return 0;
	if ( !width || !height ) { width = 1280u; height = 720u; }
	memset( &s_module, 0, sizeof( s_module ) ); memset( &info, 0, sizeof( info ) );
	if ( !RalWebGpu_BrowserEmscriptenCreate( generation, canvasIdentity,
			&s_module.bridge )
			|| !RalWebGpu_BrowserBridgeBuildCoreInfo( s_module.bridge, &info.core )
			|| !RalWebGpu_BrowserBridgeBuildResourceInfo( s_module.bridge,
				&info.resources )
			|| !RalWebGpu_BrowserBridgeBuildCommandInfo( s_module.bridge,
				&info.command )
			|| !RalWebGpu_BrowserBridgeBuildPresentationInfo( s_module.bridge,
				&info.presentation )
			|| !RalWebGpu_RuntimeBegin( &info, &s_module.runtime ) ) {
		RalWebGpu_BrowserModuleStop(); return 0;
	}
	s_module.width = width; s_module.height = height; return 1;
}

EMSCRIPTEN_KEEPALIVE int RalWebGpu_BrowserModulePoll( void ) {
	ralWebGpuPollStatus_t status;
	ralWebGpuPresentation_t *presentation;
	if ( !s_module.runtime ) return (int)RAL_WEBGPU_POLL_FAILED;
	status = RalWebGpu_RuntimePoll( s_module.runtime, &s_module.receipt );
	if ( status != RAL_WEBGPU_POLL_READY || s_module.configured ) return (int)status;
	presentation = RalWebGpu_RuntimePresentation( s_module.runtime,
		&s_module.receipt );
	if ( !presentation || !RalWebGpu_PresentationConfigure( presentation,
			s_module.width, s_module.height, RAL_WEBGPU_DPR_ONE_Q16,
			RAL_FORMAT_B8G8R8A8_UNORM, RAL_COLORSPACE_SRGB_NONLINEAR,
			qtrue, &s_module.configuredReceipt ) )
		return (int)RAL_WEBGPU_POLL_FAILED;
	s_module.configured = qtrue; return (int)RAL_WEBGPU_POLL_READY;
}

EMSCRIPTEN_KEEPALIVE int RalWebGpu_BrowserModuleResize(
		uint32_t width, uint32_t height ) {
	ralWebGpuPresentation_t *presentation;
	ralWebGpuPresentationReceipt_t resized;
	if ( !s_module.runtime || !s_module.configured || !width || !height
			|| !RalWebGpu_RuntimeGetReceipt( s_module.runtime,
				&s_module.receipt ) ) return 0;
	presentation = RalWebGpu_RuntimePresentation( s_module.runtime,
		&s_module.receipt );
	if ( !presentation || !RalWebGpu_PresentationConfigure( presentation,
			width, height, RAL_WEBGPU_DPR_ONE_Q16,
			RAL_FORMAT_B8G8R8A8_UNORM, RAL_COLORSPACE_SRGB_NONLINEAR,
			qtrue, &resized ) ) return 0;
	s_module.configuredReceipt = resized;
	s_module.width = width; s_module.height = height;
	return 1;
}

#ifdef RAL_WEBGPU_BROWSER_SMOKE
typedef struct {
	uint32_t spirv[2][4];
	ralShaderModuleAbi_t modules[2];
	ralShaderVertexInputAbi_t input;
	ralShaderAbiManifest_t manifest;
	ralShaderVariantAbi_t variant;
	ralWebGpuWgslModule_t wgsl[2];
	ralVertexBinding_t vertexBinding;
	ralVertexAttribute_t vertexAttribute;
	ralColorBlendAttachment_t blend;
	ralGraphicsPipelineCreateInfo_t state;
} browserSmokeGraphics_t;

typedef struct {
	ralWebGpuResourceLayer_t *resources;
	ralWebGpuCommand_t *command;
	ralWebGpuPresentation_t *presentation;
	ralWebGpuResource_t *vertex, *index;
	ralWebGpuResourceReceipt_t vertexReceipt, indexReceipt;
	ralWebGpuPipeline_t *pipeline;
	ralWebGpuPipelineReceipt_t pipelineReceipt;
	ralWebGpuFrameReceipt_t frame;
	ralWebGpuSubmissionReceipt_t submission;
	qboolean active;
} browserSmoke_t;

static browserSmoke_t s_smoke;
static const char s_smokeVertexWgsl[] =
	"@vertex fn main(@location(0) p: vec3<f32>) -> @builtin(position) vec4<f32> { return vec4<f32>(p, 1.0); }\n";
static const char s_smokeFragmentWgsl[] =
	"@fragment fn main() -> @location(0) vec4<f32> { return vec4<f32>(0.1, 0.7, 1.0, 1.0); }\n";

static qboolean SmokeArtifact( ralShaderArtifactAbi_t *artifact,
		ralShaderArtifactTarget_t target, const void *bytes, uint32_t byteCount ) {
	memset( artifact, 0, sizeof( *artifact ) ); artifact->target = target;
	if ( !bytes ) return qtrue;
	artifact->byteCount = byteCount;
	return Ral_ShaderArtifactDigest( bytes, byteCount, &artifact->digest )
		== ralSuccess;
}

static qboolean SmokeShaderModule( ralShaderModuleAbi_t *module,
		uint32_t stage, const uint32_t *spirv, const char *wgsl,
		uint32_t wgslBytes, uint64_t seed ) {
	memset( module, 0, sizeof( *module ) ); module->stage = stage;
	module->entryPoint = "main"; module->sourceDigest.lane0 = seed;
	module->sourceDigest.lane1 = seed + 1u;
	return SmokeArtifact( &module->artifacts[RAL_SHADER_ARTIFACT_SPIRV],
			RAL_SHADER_ARTIFACT_SPIRV, spirv, 16u )
		&& SmokeArtifact( &module->artifacts[RAL_SHADER_ARTIFACT_MSL],
			RAL_SHADER_ARTIFACT_MSL, NULL, 0u )
		&& SmokeArtifact( &module->artifacts[RAL_SHADER_ARTIFACT_WGSL],
			RAL_SHADER_ARTIFACT_WGSL, wgsl, wgslBytes );
}

static qboolean SmokeGraphics( browserSmokeGraphics_t *graphics ) {
	memset( graphics, 0, sizeof( *graphics ) );
	graphics->spirv[0][0] = 0x07230203u; graphics->spirv[1][0] = 0x07230203u;
	if ( !SmokeShaderModule( &graphics->modules[0], RAL_STAGE_VERTEX,
			graphics->spirv[0], s_smokeVertexWgsl,
			sizeof( s_smokeVertexWgsl ) - 1u, 11u )
			|| !SmokeShaderModule( &graphics->modules[1], RAL_STAGE_FRAGMENT,
				graphics->spirv[1], s_smokeFragmentWgsl,
				sizeof( s_smokeFragmentWgsl ) - 1u, 21u ) ) return qfalse;
	graphics->input.location = 0u;
	graphics->input.format = RAL_FORMAT_R32G32B32_SFLOAT;
	graphics->manifest.schemaVersion = RAL_SHADER_ABI_SCHEMA_VERSION;
	graphics->manifest.generation = 17u;
	graphics->manifest.pipelineKind = RAL_SHADER_PIPELINE_GRAPHICS;
	graphics->manifest.modules = graphics->modules;
	graphics->manifest.moduleCount = 2u;
	graphics->manifest.vertexInputs = &graphics->input;
	graphics->manifest.vertexInputCount = 1u;
	graphics->wgsl[0] = (ralWebGpuWgslModule_t){ s_smokeVertexWgsl,
		sizeof( s_smokeVertexWgsl ) - 1u };
	graphics->wgsl[1] = (ralWebGpuWgslModule_t){ s_smokeFragmentWgsl,
		sizeof( s_smokeFragmentWgsl ) - 1u };
	graphics->vertexBinding = (ralVertexBinding_t){ 0u, 12u,
		RAL_VERTEX_INPUT_PER_VERTEX };
	graphics->vertexAttribute = (ralVertexAttribute_t){ 0u, 0u,
		RAL_FORMAT_R32G32B32_SFLOAT, 0u };
	graphics->blend.writeMask = RAL_COLOR_WRITE_ALL;
	graphics->blend.writeMaskExplicit = qtrue;
	graphics->state.vertexBindings = &graphics->vertexBinding;
	graphics->state.numVertexBindings = 1u;
	graphics->state.vertexAttributes = &graphics->vertexAttribute;
	graphics->state.numVertexAttributes = 1u;
	graphics->state.topology = RAL_TOPOLOGY_TRIANGLE_LIST;
	graphics->state.raster.polygonMode = RAL_POLYGON_FILL;
	graphics->state.raster.lineWidth = 1.0f;
	graphics->state.colorBlends = &graphics->blend;
	graphics->state.numColorBlends = 1u;
	graphics->state.colorFormats[0] = RAL_FORMAT_B8G8R8A8_UNORM;
	graphics->state.numColorFormats = 1u; graphics->state.sampleCount = 1u;
	graphics->variant.artifactTarget = RAL_SHADER_ARTIFACT_WGSL;
	graphics->variant.generation = 3u;
	return Ral_GraphicsPipelineSemanticDigest( &graphics->state,
		&graphics->variant.semanticStateDigest ) == ralSuccess;
}

static void SmokeDestroy( void ) {
	if ( s_smoke.pipeline && s_module.runtime )
		(void)RalWebGpu_RuntimeDestroyPipeline( s_module.runtime,
			s_smoke.pipeline, &s_smoke.pipelineReceipt );
	if ( s_smoke.resources ) {
		if ( s_smoke.index ) (void)RalWebGpu_DestroyResource( s_smoke.resources,
			s_smoke.index, &s_smoke.indexReceipt );
		if ( s_smoke.vertex ) (void)RalWebGpu_DestroyResource( s_smoke.resources,
			s_smoke.vertex, &s_smoke.vertexReceipt );
	}
	memset( &s_smoke, 0, sizeof( s_smoke ) );
}

EMSCRIPTEN_KEEPALIVE int RalWebGpu_BrowserModuleSmokeBegin( void ) {
	static const float vertices[9] = {
		0.0f, 0.65f, 0.0f, -0.65f, -0.65f, 0.0f, 0.65f, -0.65f, 0.0f };
	static const uint32_t indices[4] = { 0u, 1u, 2u, 0u };
	browserSmokeGraphics_t graphics;
	ralWebGpuBufferDesc_t desc;
	ralWebGpuWriteReceipt_t writeReceipt;
	ralWebGpuPipelineCreateInfo_t pipelineInfo;
	ralWebGpuCommandReceipt_t recording, recorded, executable;
	ralWebGpuIndexedDraw_t draw;
	int stage = 1;
	if ( !s_module.runtime || !s_module.configured || s_smoke.active ) return 0;
	memset( &s_smoke, 0, sizeof( s_smoke ) );
	s_smoke.resources = RalWebGpu_RuntimeResources( s_module.runtime,
		&s_module.receipt );
	s_smoke.command = RalWebGpu_RuntimeCommand( s_module.runtime,
		&s_module.receipt );
	s_smoke.presentation = RalWebGpu_RuntimePresentation( s_module.runtime,
		&s_module.receipt );
	if ( !s_smoke.resources || !s_smoke.command || !s_smoke.presentation
			|| !RalWebGpu_PresentationAcquire( s_smoke.presentation,
				&s_module.configuredReceipt, &s_smoke.frame ) ) goto fail;
	stage = 2;
	memset( &desc, 0, sizeof( desc ) ); desc.size = sizeof( vertices );
	desc.usage = RAL_WEBGPU_BUFFER_VERTEX | RAL_WEBGPU_BUFFER_COPY_DESTINATION;
	desc.memoryClass = RAL_ALLOCATION_DEVICE_LOCAL;
	if ( !RalWebGpu_CreateBuffer( s_smoke.resources, &desc, &s_smoke.vertex,
			&s_smoke.vertexReceipt )
			|| !RalWebGpu_WriteBuffer( s_smoke.resources, s_smoke.vertex,
				&s_smoke.vertexReceipt, 0u, vertices, sizeof( vertices ),
				&writeReceipt ) ) goto fail;
	stage = 3;
	desc.size = sizeof( indices );
	desc.usage = RAL_WEBGPU_BUFFER_INDEX | RAL_WEBGPU_BUFFER_COPY_DESTINATION;
	if ( !RalWebGpu_CreateBuffer( s_smoke.resources, &desc, &s_smoke.index,
			&s_smoke.indexReceipt )
			|| !RalWebGpu_WriteBuffer( s_smoke.resources, s_smoke.index,
				&s_smoke.indexReceipt, 0u, indices, sizeof( indices ),
				&writeReceipt ) || !SmokeGraphics( &graphics ) ) goto fail;
	stage = 4;
	memset( &pipelineInfo, 0, sizeof( pipelineInfo ) );
	pipelineInfo.manifest = &graphics.manifest;
	pipelineInfo.variant = &graphics.variant; pipelineInfo.modules = graphics.wgsl;
	pipelineInfo.moduleCount = 2u; pipelineInfo.graphicsState = &graphics.state;
	pipelineInfo.generation = 41u;
	if ( !RalWebGpu_BrowserBridgeApplyPipelineInfo( s_module.bridge, &pipelineInfo )
			|| !RalWebGpu_RuntimeCreatePipeline( s_module.runtime, &s_module.receipt,
				&pipelineInfo, &s_smoke.pipeline, &s_smoke.pipelineReceipt ) ) goto fail;
	stage = 5;
	if ( !RalWebGpu_CommandBegin( s_smoke.command, RAL_WEBGPU_PASS_RENDER,
			s_smoke.frame.textureIdentity, &recording ) ) goto fail;
	stage = 6;
	memset( &draw, 0, sizeof( draw ) ); draw.kind = RAL_WEBGPU_DRAW_EFFECT;
	draw.pipelineIdentity = s_smoke.pipelineReceipt.pipelineIdentity;
	draw.vertexBufferIdentity = s_smoke.vertexReceipt.resourceIdentity;
	draw.indexBufferIdentity = s_smoke.indexReceipt.resourceIdentity;
	draw.indexCount = 3u; draw.instanceCount = 1u; draw.contentDigest = 0xabcdu;
	if ( !RalWebGpu_CommandRecordIndexedDraw( s_smoke.command, &recording,
			&draw, &recorded ) ) goto fail;
	stage = 7;
	if ( !RalWebGpu_CommandEnd( s_smoke.command, &recorded, &executable ) ) goto fail;
	stage = 8;
	if ( !RalWebGpu_CommandSubmit( s_smoke.command, &executable,
			&s_smoke.submission ) ) goto fail;
	s_smoke.active = qtrue; return 1;
fail:
	SmokeDestroy(); return -stage;
}

EMSCRIPTEN_KEEPALIVE int RalWebGpu_BrowserModuleSmokePoll( void ) {
	ralWebGpuAsyncStatus_t status;
	if ( !s_smoke.active ) return 0;
	status = RalWebGpu_CommandPoll( s_smoke.command, &s_smoke.submission );
	if ( status == RAL_WEBGPU_ASYNC_PENDING ) return 1;
	if ( status != RAL_WEBGPU_ASYNC_READY ) {
		int failure = -( 10 + (int)status );
		SmokeDestroy(); return failure;
	}
	if ( !RalWebGpu_PresentationPresent( s_smoke.presentation,
			&s_smoke.frame, &s_smoke.submission ) ) {
		SmokeDestroy(); return -2;
	}
	SmokeDestroy(); return 2;
}
#endif

EMSCRIPTEN_KEEPALIVE void RalWebGpu_BrowserModuleStop( void ) {
#ifdef RAL_WEBGPU_BROWSER_SMOKE
	SmokeDestroy();
#endif
	if ( s_module.runtime ) RalWebGpu_RuntimeDestroy( s_module.runtime );
	if ( s_module.bridge ) RalWebGpu_BrowserBridgeDestroy( s_module.bridge );
	memset( &s_module, 0, sizeof( s_module ) );
}
#else
qboolean RalWebGpu_BrowserEmscriptenCreate( uint64_t generation,
		uint64_t canvasIdentity, ralWebGpuBrowserBridge_t **outBridge ) {
	(void)generation; (void)canvasIdentity; (void)outBridge;
	return qfalse;
}
qboolean RalWebGpu_BrowserModuleBorrow(
		ralWebGpuBrowserModuleBorrow_t *outBorrow ) {
	(void)outBorrow; return qfalse;
}
int RalWebGpu_BrowserModuleStart( uint32_t generation,
		uint32_t canvasIdentity, uint32_t width, uint32_t height ) {
	(void)generation; (void)canvasIdentity; (void)width; (void)height; return 0;
}
int RalWebGpu_BrowserModulePoll( void ) { return 0; }
int RalWebGpu_BrowserModuleResize( uint32_t width, uint32_t height ) {
	(void)width; (void)height; return 0;
}
void RalWebGpu_BrowserModuleStop( void ) {}
#endif
