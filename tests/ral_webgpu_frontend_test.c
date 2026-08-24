// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "ral_webgpu_product.h"
#include "maps/map_format_registry.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(x) do { if ( !(x) ) { fprintf( stderr, "FAIL %s:%d: %s\n", \
	__FILE__, __LINE__, #x ); return 1; } } while ( 0 )

typedef struct {
	uintptr_t nextIdentity;
	uint32_t resourcesCreated, resourcesDestroyed;
	uint32_t bufferWrites, textureWrites, draws;
	uint32_t effectDraws, uiDraws;
	uintptr_t worldPipeline, effectPipeline, uiPipeline, msdfPipeline;
	uintptr_t submissionIdentity;
	uint64_t submissionGeneration;
	qboolean failDraw;
} fakeHost_t;
static uintptr_t NewIdentity( fakeHost_t *host ) { return ++host->nextIdentity; }
static qboolean BeginAdapter( void *u, uint64_t g ) { (void)u; return g ? qtrue : qfalse; }
static qboolean PollAdapter( void *u, uint64_t g, ralWebGpuAdapterPoll_t *o ) {
	(void)u; (void)g; memset( o, 0, sizeof( *o ) );
	o->status = RAL_WEBGPU_REQUEST_READY; o->adapterIdentity = 0x2000u;
	o->adapterType = RAL_ADAPTER_TYPE_DISCRETE; o->vendorName = "Wired";
	o->deviceName = "WebGPU frontend fixture"; o->limits.maxColorAttachments = 8u;
	o->limits.maxTextureDimension2D = 8192u; o->limits.maxTextureDimension3D = 2048u;
	o->limits.maxTextureArrayLayers = 2048u; o->limits.maxComputeInvocationsPerWorkgroup = 256u;
	o->limits.maxSampledTexturesPerShaderStage = 32u; o->limits.maxBindGroups = 4u;
	o->limits.maxBindingsPerBindGroup = 16u; o->limits.maxStorageBufferBindingSize = UINT64_C(134217728);
	o->limits.minUniformBufferOffsetAlignment = 256u; o->limits.minStorageBufferOffsetAlignment = 256u;
	return qtrue;
}
static qboolean BeginDevice( void *u, uintptr_t a, uint64_t g ) { (void)u; return a == 0x2000u && g > 1u; }
static qboolean PollDevice( void *u, uint64_t g, ralWebGpuDevicePoll_t *o ) {
	(void)u; (void)g; memset( o, 0, sizeof( *o ) ); o->status = RAL_WEBGPU_REQUEST_READY;
	o->deviceIdentity = 0x3000u; o->queueIdentity = 0x4000u; return qtrue;
}
static void ReleaseDevice( void *u, uintptr_t d, uintptr_t q ) { (void)u; (void)d; (void)q; }
static void ReleaseAdapter( void *u, uintptr_t a ) { (void)u; (void)a; }
static qboolean CreateBuffer( void *u, uintptr_t d, const ralWebGpuBufferDesc_t *x, uintptr_t *o ) {
	fakeHost_t *host = u; if ( d != 0x3000u || !x || !x->size ) return qfalse;
	host->resourcesCreated++; *o = NewIdentity( host ); return qtrue;
}
static qboolean CreateTexture( void *u, uintptr_t d, const ralWebGpuTextureDesc_t *x, uintptr_t *o ) {
	fakeHost_t *host = u; if ( d != 0x3000u || !x ) return qfalse;
	host->resourcesCreated++; *o = NewIdentity( host ); return qtrue;
}
static qboolean CreateSampler( void *u, uintptr_t d, const ralWebGpuSamplerDesc_t *x, uintptr_t *o ) {
	fakeHost_t *host = u; if ( d != 0x3000u || !x ) return qfalse;
	host->resourcesCreated++; *o = NewIdentity( host ); return qtrue;
}
static void DestroyResource( void *u, ralWebGpuResourceKind_t k, uintptr_t i ) {
	if ( k && i ) ((fakeHost_t *)u)->resourcesDestroyed++;
}
static qboolean WriteBuffer( void *u, uintptr_t q, uintptr_t b, uint64_t o,
	const void *p, uint64_t n ) {
	if ( q != 0x4000u || !b || ( o & 3u ) || !p || !n ) return qfalse;
	((fakeHost_t *)u)->bufferWrites++; return qtrue;
}
static qboolean WriteTexture( void *u, uintptr_t q, uintptr_t t, const void *b, uint64_t n, uint32_t r, uint32_t i ) {
	if ( q != 0x4000u || !t || !b || !n || !r || !i
			|| ( r & 255u ) || n != (uint64_t)r * i ) return qfalse;
	((fakeHost_t *)u)->textureWrites++; return qtrue;
}
static qboolean BeginRoundTrip( void *u, uintptr_t d, uintptr_t q, uintptr_t a, uintptr_t b,
	const void *p, uint64_t n, uint64_t g, uintptr_t *o ) {
	(void)d; (void)q; (void)a; (void)b; (void)p; (void)n; (void)g; *o = NewIdentity( u ); return qtrue;
}
static qboolean PollRoundTrip( void *u, uintptr_t o, uint64_t g, void *b, uint64_t c,
	uint64_t *n, ralWebGpuAsyncStatus_t *s ) {
	(void)u; (void)o; (void)g; (void)b; (void)c; (void)n; *s = RAL_WEBGPU_ASYNC_FAILED; return qtrue;
}
static void ReleaseOperation( void *u, uintptr_t o ) { (void)u; (void)o; }
static qboolean BeginEncoder( void *u, uintptr_t d, uintptr_t *o ) {
	if ( d != 0x3000u ) return qfalse; *o = NewIdentity( u ); return qtrue;
}
static qboolean BeginPass( void *u, uintptr_t e, ralWebGpuPassKind_t k, uintptr_t t, uintptr_t *o ) {
	if ( !e || k != RAL_WEBGPU_PASS_RENDER || !t ) return qfalse;
	*o = NewIdentity( u ); return qtrue;
}
static qboolean RecordDraw( void *u, uintptr_t p, const ralWebGpuIndexedDraw_t *d ) {
	fakeHost_t *host = u;
	if ( host->failDraw || !p || !d
			|| ( d->kind != RAL_WEBGPU_DRAW_WORLD
				&& d->kind != RAL_WEBGPU_DRAW_ENTITY
				&& d->kind != RAL_WEBGPU_DRAW_EFFECT
				&& d->kind != RAL_WEBGPU_DRAW_UI )
			|| !d->pipelineIdentity || !d->vertexBufferIdentity
			|| !d->indexBufferIdentity || !d->indexCount
			|| ( d->textured && ( !d->textureIdentity || !d->samplerIdentity ) )
			|| ( !d->textured && ( d->textureIdentity || d->samplerIdentity ) ) )
		return qfalse;
	if ( ( ( d->kind == RAL_WEBGPU_DRAW_WORLD
				|| d->kind == RAL_WEBGPU_DRAW_ENTITY )
			&& d->pipelineIdentity != host->worldPipeline )
			|| ( d->kind == RAL_WEBGPU_DRAW_EFFECT
				&& ( d->pipelineIdentity != host->effectPipeline
					|| d->textured != ( host->effectDraws % 2u == 0u ) ) )
			|| ( d->kind == RAL_WEBGPU_DRAW_UI
				&& d->pipelineIdentity != ( host->uiDraws % 2u == 0u
					? host->uiPipeline : host->msdfPipeline ) ) ) return qfalse;
	if ( d->kind == RAL_WEBGPU_DRAW_EFFECT ) host->effectDraws++;
	if ( d->kind == RAL_WEBGPU_DRAW_UI ) host->uiDraws++;
	host->draws++; return qtrue;
}
static qboolean EndPass( void *u, uintptr_t p ) { (void)u; return p ? qtrue : qfalse; }
static qboolean FinishEncoder( void *u, uintptr_t e, uintptr_t *o ) {
	if ( !e ) return qfalse; *o = NewIdentity( u ); return qtrue;
}
static qboolean Submit( void *u, uintptr_t q, uintptr_t b, uint64_t g, uintptr_t *o ) {
	fakeHost_t *host = u; if ( q != 0x4000u || !b || !g ) return qfalse;
	host->submissionGeneration = g; host->submissionIdentity = NewIdentity( host );
	*o = host->submissionIdentity; return qtrue;
}
static qboolean PollSubmission( void *u, uintptr_t s, uint64_t g, ralWebGpuAsyncStatus_t *o ) {
	fakeHost_t *host = u;
	if ( s != host->submissionIdentity || g != host->submissionGeneration ) return qfalse;
	*o = RAL_WEBGPU_ASYNC_READY; return qtrue;
}
static void ReleaseCommand( void *u, ralWebGpuCommandObjectKind_t k, uintptr_t o ) { (void)u; (void)k; (void)o; }
static qboolean Configure( void *u, uintptr_t c, uintptr_t d, uint32_t w, uint32_t h,
	ralFormat_t f, ralColorSpace_t s, qboolean a ) {
	(void)u; (void)c; (void)d; (void)w; (void)h; (void)f; (void)s; (void)a; return qtrue;
}
static void Unconfigure( void *u, uintptr_t c ) { (void)u; (void)c; }
static qboolean Acquire( void *u, uintptr_t c, uint64_t g, uintptr_t *o ) { (void)u; (void)c; (void)g; (void)o; return qfalse; }
static qboolean Present( void *u, uintptr_t c, uintptr_t t, uint64_t f, uint64_t s ) {
	(void)u; (void)c; (void)t; (void)f; (void)s; return qfalse;
}

static qboolean CreateShader( void *u, uintptr_t device,
		const ralWebGpuShaderModuleDesc_t *desc, uintptr_t *out ) {
	if ( device != 0x3000u || !desc || !desc->code || !desc->byteCount ) return qfalse;
	*out = NewIdentity( u ); return qtrue;
}
static qboolean CreateGroup( void *u, uintptr_t device, uint32_t group,
		const ralWebGpuBindGroupLayoutEntry_t *entries, uint32_t count,
		uintptr_t *out ) {
	(void)u; (void)device; (void)group; (void)entries; (void)count; (void)out;
	return qfalse;
}
static qboolean CreateLayout( void *u, uintptr_t device,
		const uintptr_t *layouts, uint32_t count, uintptr_t *out ) {
	(void)layouts; if ( device != 0x3000u || count ) return qfalse;
	*out = NewIdentity( u ); return qtrue;
}
static qboolean CreatePipeline( void *u, uintptr_t device,
		const ralWebGpuNativePipelineDesc_t *desc, uintptr_t *out ) {
	if ( device != 0x3000u || !desc || desc->kind != RAL_SHADER_PIPELINE_GRAPHICS
			|| desc->shaderModuleCount != 2u || !desc->graphicsState ) return qfalse;
	*out = NewIdentity( u ); return qtrue;
}
static void DestroyPipeline( void *u, ralWebGpuPipelineObjectKind_t k,
		uintptr_t o ) { (void)u; (void)k; (void)o; }

static void Artifact( ralShaderArtifactAbi_t *artifact,
		ralShaderArtifactTarget_t target, const void *bytes, uint32_t byteCount ) {
	memset( artifact, 0, sizeof( *artifact ) ); artifact->target = target;
	if ( bytes ) {
		artifact->byteCount = byteCount;
		if ( Ral_ShaderArtifactDigest( bytes, byteCount, &artifact->digest )
				!= ralSuccess ) abort();
	}
}

static void Module( ralShaderModuleAbi_t *module, uint32_t stage,
		const char *wgsl, uint64_t seed ) {
	uint32_t spirv[4] = { 0x07230203u, 0u, 0u, 0u };
	memset( module, 0, sizeof( *module ) ); module->stage = stage;
	module->entryPoint = "main"; module->sourceDigest.lane0 = seed;
	module->sourceDigest.lane1 = seed + 1u;
	Artifact( &module->artifacts[RAL_SHADER_ARTIFACT_SPIRV],
		RAL_SHADER_ARTIFACT_SPIRV, spirv, sizeof( spirv ) );
	Artifact( &module->artifacts[RAL_SHADER_ARTIFACT_MSL],
		RAL_SHADER_ARTIFACT_MSL, NULL, 0u );
	Artifact( &module->artifacts[RAL_SHADER_ARTIFACT_WGSL],
		RAL_SHADER_ARTIFACT_WGSL, wgsl, (uint32_t)strlen( wgsl ) );
}

static ralWebGpuRuntimeCreateInfo_t RuntimeInfo( fakeHost_t *host ) {
	ralWebGpuRuntimeCreateInfo_t info; memset( &info, 0, sizeof( info ) );
	info.core.generation = 7u; info.core.userData = host;
	info.core.host.beginAdapter = BeginAdapter; info.core.host.pollAdapter = PollAdapter;
	info.core.host.beginDevice = BeginDevice; info.core.host.pollDevice = PollDevice;
	info.core.host.releaseDevice = ReleaseDevice; info.core.host.releaseAdapter = ReleaseAdapter;
	info.resources.userData = host; info.resources.host.createBuffer = CreateBuffer;
	info.resources.host.createTexture = CreateTexture; info.resources.host.createSampler = CreateSampler;
	info.resources.host.destroyResource = DestroyResource; info.resources.host.writeBuffer = WriteBuffer;
	info.resources.host.writeTexture = WriteTexture;
	info.resources.host.beginRoundTrip = BeginRoundTrip; info.resources.host.pollRoundTrip = PollRoundTrip;
	info.resources.host.releaseOperation = ReleaseOperation;
	info.command.userData = host; info.command.host.beginEncoder = BeginEncoder;
	info.command.host.beginPass = BeginPass; info.command.host.recordIndexedDraw = RecordDraw;
	info.command.host.endPass = EndPass;
	info.command.host.finishEncoder = FinishEncoder; info.command.host.submit = Submit;
	info.command.host.pollSubmission = PollSubmission; info.command.host.releaseObject = ReleaseCommand;
	info.presentation.userData = host; info.presentation.canvasIdentity = 0x7000u;
	info.presentation.host.configure = Configure; info.presentation.host.unconfigure = Unconfigure;
	info.presentation.host.acquire = Acquire; info.presentation.host.present = Present;
	return info;
}

int main( void ) {
	static const char vertexWgsl[] =
		"@vertex fn main(@location(0) p:vec3<f32>)->@builtin(position) vec4<f32>{return vec4<f32>(p,1.0);}\n";
	static const char fragmentWgsl[] =
		"@fragment fn main()->@location(0) vec4<f32>{return vec4<f32>(1.0);}\n";
	fakeHost_t host = { .nextIdentity = 0x5000u };
	ralWebGpuRuntimeCreateInfo_t runtimeInfo = RuntimeInfo( &host );
	ralWebGpuRuntime_t *runtime = NULL;
	ralWebGpuRuntimeReceipt_t runtimeReceipt, staleRuntime;
	ralShaderModuleAbi_t shaderModules[2];
	ralShaderVertexInputAbi_t shaderInput;
	ralShaderAbiManifest_t shaderManifest;
	ralShaderVariantAbi_t shaderVariant;
	ralWebGpuWgslModule_t wgslModules[2];
	ralVertexBinding_t vertexBinding;
	ralVertexAttribute_t vertexAttribute;
	ralColorBlendAttachment_t blend;
	ralGraphicsPipelineCreateInfo_t graphicsState;
	ralWebGpuPipelineCreateInfo_t pipelineInfo;
	ralWebGpuPipeline_t *pipeline = NULL;
	ralWebGpuPipelineReceipt_t pipelineReceipt;
	ralWebGpuPipeline_t *effectPipeline = NULL, *uiPipeline = NULL;
	ralWebGpuPipeline_t *msdfPipeline = NULL;
	ralWebGpuPipelineReceipt_t effectPipelineReceipt, uiPipelineReceipt;
	ralWebGpuPipelineReceipt_t msdfPipelineReceipt;
	ralWebGpuProduct_t *product = NULL;
	ralWebGpuProductFrameReceipt_t productReceipt, productExact, productOutput;
	renderSubmissionState_t frontend;
	renderSubmissionReceipt_t submission, staleSubmission;
	ralWebGpuFrontendPlanReceipt_t plan, exact, before;
	mapFile_t map; dsurface_t surfaces[2]; drawVert_t vertices[12];
	int indices[3] = { 0, 1, 2 }; dshader_t shader; refdef_t view;
	refEntity_t modelEntity, spriteEntity, beamEntity; refEntityMotion_t motion;
	qhandle_t material, lightmap, msdf, model; char lightmapName[MAX_QPATH];
	const byte rgba[16] = { 0u, 255u, 0u, 255u, 0u, 255u, 0u, 255u,
		0u, 255u, 0u, 255u, 0u, 255u, 0u, 255u };
	CHECK( RalWebGpu_RuntimeBegin( &runtimeInfo, &runtime ) );
	while ( RalWebGpu_RuntimePoll( runtime, &runtimeReceipt ) == RAL_WEBGPU_POLL_PENDING ) {}
	CHECK( RalWebGpu_RuntimePoll( runtime, &runtimeReceipt ) == RAL_WEBGPU_POLL_READY );
	Module( &shaderModules[0], RAL_STAGE_VERTEX, vertexWgsl, 11u );
	Module( &shaderModules[1], RAL_STAGE_FRAGMENT, fragmentWgsl, 21u );
	memset( &shaderInput, 0, sizeof( shaderInput ) ); shaderInput.location = 0u;
	shaderInput.format = RAL_FORMAT_R32G32B32_SFLOAT;
	memset( &shaderManifest, 0, sizeof( shaderManifest ) );
	shaderManifest.schemaVersion = RAL_SHADER_ABI_SCHEMA_VERSION;
	shaderManifest.generation = 17u; shaderManifest.pipelineKind = RAL_SHADER_PIPELINE_GRAPHICS;
	shaderManifest.modules = shaderModules; shaderManifest.moduleCount = 2u;
	shaderManifest.vertexInputs = &shaderInput; shaderManifest.vertexInputCount = 1u;
	wgslModules[0] = (ralWebGpuWgslModule_t){ vertexWgsl, sizeof( vertexWgsl ) - 1u };
	wgslModules[1] = (ralWebGpuWgslModule_t){ fragmentWgsl, sizeof( fragmentWgsl ) - 1u };
	vertexBinding = (ralVertexBinding_t){ 0u, sizeof( renderWorldVertex_t ),
		RAL_VERTEX_INPUT_PER_VERTEX };
	vertexAttribute = (ralVertexAttribute_t){ 0u, 0u,
		RAL_FORMAT_R32G32B32_SFLOAT, 0u };
	memset( &blend, 0, sizeof( blend ) ); blend.writeMask = RAL_COLOR_WRITE_ALL;
	blend.writeMaskExplicit = qtrue;
	memset( &graphicsState, 0, sizeof( graphicsState ) );
	graphicsState.vertexBindings = &vertexBinding; graphicsState.numVertexBindings = 1u;
	graphicsState.vertexAttributes = &vertexAttribute; graphicsState.numVertexAttributes = 1u;
	graphicsState.topology = RAL_TOPOLOGY_TRIANGLE_LIST;
	graphicsState.raster.polygonMode = RAL_POLYGON_FILL; graphicsState.raster.lineWidth = 1.0f;
	graphicsState.colorBlends = &blend; graphicsState.numColorBlends = 1u;
	graphicsState.colorFormats[0] = RAL_FORMAT_R8G8B8A8_UNORM;
	graphicsState.numColorFormats = 1u; graphicsState.sampleCount = 1u;
	memset( &shaderVariant, 0, sizeof( shaderVariant ) );
	shaderVariant.artifactTarget = RAL_SHADER_ARTIFACT_WGSL;
	shaderVariant.generation = 3u;
	CHECK( Ral_GraphicsPipelineSemanticDigest( &graphicsState,
		&shaderVariant.semanticStateDigest ) == ralSuccess );
	memset( &pipelineInfo, 0, sizeof( pipelineInfo ) ); pipelineInfo.userData = &host;
	pipelineInfo.host.createShaderModule = CreateShader;
	pipelineInfo.host.createBindGroupLayout = CreateGroup;
	pipelineInfo.host.createPipelineLayout = CreateLayout;
	pipelineInfo.host.createPipeline = CreatePipeline;
	pipelineInfo.host.destroyObject = DestroyPipeline;
	pipelineInfo.manifest = &shaderManifest; pipelineInfo.variant = &shaderVariant;
	pipelineInfo.modules = wgslModules; pipelineInfo.moduleCount = 2u;
	pipelineInfo.graphicsState = &graphicsState; pipelineInfo.generation = 41u;
	CHECK( RalWebGpu_RuntimeCreatePipeline( runtime, &runtimeReceipt,
		&pipelineInfo, &pipeline, &pipelineReceipt ) );
	CHECK( RalWebGpu_RuntimeGetReceipt( runtime, &runtimeReceipt ) );
	pipelineInfo.generation = 42u;
	CHECK( RalWebGpu_RuntimeCreatePipeline( runtime, &runtimeReceipt,
		&pipelineInfo, &effectPipeline, &effectPipelineReceipt ) );
	CHECK( RalWebGpu_RuntimeGetReceipt( runtime, &runtimeReceipt ) );
	pipelineInfo.generation = 43u;
	CHECK( RalWebGpu_RuntimeCreatePipeline( runtime, &runtimeReceipt,
		&pipelineInfo, &uiPipeline, &uiPipelineReceipt ) );
	CHECK( RalWebGpu_RuntimeGetReceipt( runtime, &runtimeReceipt ) );
	pipelineInfo.generation = 44u;
	CHECK( RalWebGpu_RuntimeCreatePipeline( runtime, &runtimeReceipt,
		&pipelineInfo, &msdfPipeline, &msdfPipelineReceipt ) );
	CHECK( RalWebGpu_RuntimeGetReceipt( runtime, &runtimeReceipt ) );
	host.worldPipeline = pipelineReceipt.pipelineIdentity;
	host.effectPipeline = effectPipelineReceipt.pipelineIdentity;
	host.uiPipeline = uiPipelineReceipt.pipelineIdentity;
	host.msdfPipeline = msdfPipelineReceipt.pipelineIdentity;
	CHECK( RalWebGpu_ProductCreate( runtime, &runtimeReceipt, &pipelineReceipt,
		42u, &product ) );
	CHECK( RalWebGpu_ProductSetEntityPipeline( product, &runtimeReceipt,
		&pipelineReceipt ) );
	CHECK( RalWebGpu_ProductSetEffectPipeline( product, &runtimeReceipt,
		&effectPipelineReceipt ) );
	CHECK( RalWebGpu_ProductSetUiPipelines( product, &runtimeReceipt,
		&uiPipelineReceipt, &msdfPipelineReceipt ) );
	CHECK( !RalWebGpu_ProductSetViewport( product, &runtimeReceipt, 0u, 720u ) );
	CHECK( RalWebGpu_ProductSetViewport( product, &runtimeReceipt, 1280u, 720u ) );
	CHECK( RenderSubmission_Init( &frontend, 71u ) );
	material = RenderSubmission_RegisterMaterialImage( &frontend, RENDER_ASSET_MATERIAL,
		"textures/webgpu", qtrue, rgba, 2u, 2u );
	msdf = RenderSubmission_RegisterMaterialImage( &frontend, RENDER_ASSET_MSDF,
		"fonts/webgpu", qtrue, rgba, 2u, 2u );
	CHECK( material > 0 && msdf > material );
	memset( &map, 0, sizeof( map ) ); memset( surfaces, 0, sizeof( surfaces ) );
	memset( vertices, 0, sizeof( vertices ) ); memset( &shader, 0, sizeof( shader ) );
	strcpy( map.name, "maps/webgpu-plan.bsp" ); map.checksum = 0x12345678;
	map.numSurfaces = 2; map.surfaces = surfaces; map.numDrawVerts = 12;
	map.drawVerts = vertices; map.numDrawIndexes = 3; map.drawIndexes = indices;
	map.numShaders = 1; map.shaders = &shader; strcpy( shader.shader, "textures/webgpu" );
	surfaces[0].surfaceType = MST_PLANAR; surfaces[0].shaderNum = 0;
	surfaces[0].firstVert = 0; surfaces[0].numVerts = 3; surfaces[0].firstIndex = 0;
	surfaces[0].numIndexes = 3; surfaces[0].lightmapNum = 0;
	surfaces[1].surfaceType = MST_PATCH; surfaces[1].shaderNum = 0;
	surfaces[1].firstVert = 3; surfaces[1].numVerts = 9;
	surfaces[1].patchWidth = 3; surfaces[1].patchHeight = 3; surfaces[1].lightmapNum = -1;
	for ( int i = 0; i < 12; ++i ) memset( vertices[i].color.rgba, 255, 4u );
	vertices[1].xyz[0] = 1.0f; vertices[2].xyz[1] = 1.0f;
	for ( int y = 0; y < 3; ++y ) for ( int x = 0; x < 3; ++x ) {
		drawVert_t *v = &vertices[3 + y * 3 + x]; v->xyz[0] = (float)x;
		v->xyz[1] = (float)y; v->xyz[2] = x == 1 && y == 1 ? 8.0f : 0.0f;
	}
	CHECK( RenderSubmission_LightmapMaterialName( lightmapName,
		(uint32_t)map.checksum, 0 ) );
	lightmap = RenderSubmission_RegisterMaterialImage( &frontend,
		RENDER_ASSET_LIGHTMAP, lightmapName, qtrue, rgba, 2u, 2u );
	CHECK( lightmap > 0 && RenderSubmission_LoadWorld( &frontend, &map, 0 ) );
	model = RenderSubmission_RegisterInlineModel( &frontend, "*1", 0u, 1u );
	CHECK( model > 0 && RenderSubmission_BeginFrame( &frontend, 75u ) );
	memset( &view, 0, sizeof( view ) ); view.width = 1280; view.height = 720;
	view.fov_x = 90.0f; view.fov_y = 60.0f;
	view.viewaxis[0][0] = view.viewaxis[1][1] = view.viewaxis[2][2] = 1.0f;
	CHECK( RenderSubmission_RenderScene( &frontend, &view, 0 ) );
	memset( &modelEntity, 0, sizeof( modelEntity ) ); modelEntity.reType = RT_MODEL;
	modelEntity.hModel = model; modelEntity.axis[0][0] = modelEntity.axis[1][1]
		= modelEntity.axis[2][2] = 1.0f; memset( modelEntity.shader.rgba, 255, 4u );
	memset( &motion, 0, sizeof( motion ) ); motion.structSize = sizeof( motion );
	motion.version = REF_ENTITY_MOTION_VERSION; motion.ownerId = 1u; motion.generation = 75u;
	motion.role = REF_ENTITY_MOTION_ROLE_PLAYER_BODY;
	CHECK( RenderSubmission_AddEntity( &frontend, &modelEntity, &motion ) );
	memset( &spriteEntity, 0, sizeof( spriteEntity ) ); spriteEntity.reType = RT_SPRITE;
	spriteEntity.radius = 1.0f; spriteEntity.customShader = material;
	memset( spriteEntity.shader.rgba, 255, 4u );
	CHECK( RenderSubmission_AddEntity( &frontend, &spriteEntity, NULL ) );
	memset( &beamEntity, 0, sizeof( beamEntity ) ); beamEntity.reType = RT_BEAM;
	beamEntity.radius = 0.5f; beamEntity.customShader = material;
	beamEntity.oldorigin[0] = 4.0f; memset( beamEntity.shader.rgba, 255, 4u );
	CHECK( RenderSubmission_AddEntity( &frontend, &beamEntity, NULL ) );
	CHECK( RenderSubmission_AddUiQuad( &frontend, 0.0f, 0.0f, 1280.0f, 720.0f,
		0.0f, 0.0f, 1.0f, 1.0f, 0.0f, material ) );
	CHECK( RenderSubmission_AddUiLine( &frontend, 10.0f, 20.0f, 200.0f, 220.0f,
		2.0f, msdf ) );
	{
		polyVert_t poly[3]; memset( poly, 0, sizeof( poly ) );
		CHECK( RenderSubmission_AddPoly( &frontend, material, 3, poly, 1 ) );
	}
	CHECK( RenderSubmission_AddLight( &frontend, spriteEntity.origin, NULL,
		64.0f, 1.0f, 0.5f, 0.25f ) );
	CHECK( RenderSubmission_EndFrame( &frontend, 75u, &submission ) );
	CHECK( RalWebGpu_FrontendPlanBuild( runtime, &runtimeReceipt, &frontend,
		&submission, 76u, &plan ) );
	CHECK( plan.materialCount == 3u && plan.worldBatchCount == 2u
		&& plan.patchBatchCount == 1u && plan.lightmappedWorldBatchCount == 1u
		&& plan.modelEntityCount == 1u && plan.primitiveEntityCount == 2u
		&& plan.temporalEntityCount == 1u && plan.polygonCount == 1u
		&& plan.lightCount == 1u && plan.uiPrimitiveCount == 2u
		&& plan.texturedUiPrimitiveCount == 2u && plan.msdfUiPrimitiveCount == 1u
		&& plan.drawCount == 9u && !plan.unresolvedCount );
	exact = plan; CHECK( RalWebGpu_FrontendPlanReceiptExact( &plan, &exact ) );
	exact.loweringDigest++; CHECK( !RalWebGpu_FrontendPlanReceiptExact( &plan, &exact ) );
	memset( &plan, 0xa5, sizeof( plan ) ); before = plan;
	staleSubmission = submission; staleSubmission.frameDigest++;
	CHECK( !RalWebGpu_FrontendPlanBuild( runtime, &runtimeReceipt, &frontend,
		&staleSubmission, 77u, &plan ) && !memcmp( &plan, &before, sizeof( plan ) ) );
	staleRuntime = runtimeReceipt; staleRuntime.pipelineCount++;
	CHECK( !RalWebGpu_FrontendPlanBuild( runtime, &staleRuntime, &frontend,
		&submission, 77u, &plan ) && !memcmp( &plan, &before, sizeof( plan ) ) );
	CHECK( RalWebGpu_ProductRender( product, &runtimeReceipt, &frontend,
		&submission, 0x8000u, 78u, &productReceipt ) );
	CHECK( productReceipt.uploadedMaterialCount == 3u
		&& productReceipt.reusedMaterialCount == 0u
		&& productReceipt.worldDrawCount == 2u
		&& productReceipt.modelEntityCount == 1u
		&& productReceipt.primitiveEntityCount == 2u
		&& productReceipt.temporalEntityCount == 1u
		&& productReceipt.entityDrawCount == 3u
		&& productReceipt.polygonDrawCount == 1u
		&& productReceipt.lightDrawCount == 1u
		&& productReceipt.uiDrawCount == 2u
		&& productReceipt.deferredNonWorldDrawCount == 0u
		&& host.textureWrites == 3u && host.bufferWrites == 6u
		&& host.draws == 9u );
	productExact = productReceipt;
	CHECK( RalWebGpu_ProductFrameReceiptExact( &productReceipt, &productExact ) );
	CHECK( RalWebGpu_ProductPoll( product, &productReceipt ) == RAL_WEBGPU_ASYNC_READY );
	CHECK( RalWebGpu_ProductRender( product, &runtimeReceipt, &frontend,
		&submission, 0x8001u, 79u, &productReceipt ) );
	CHECK( productReceipt.uploadedMaterialCount == 0u
		&& productReceipt.reusedMaterialCount == 3u
		&& host.textureWrites == 3u && host.bufferWrites == 12u
		&& host.draws == 18u );
	CHECK( RalWebGpu_ProductPoll( product, &productReceipt ) == RAL_WEBGPU_ASYNC_READY );
	CHECK( frontend.materials[0].snapshot.handle == material );
	frontend.materials[0].snapshot.generation++;
	frontend.materials[0].snapshot.digest++;
	CHECK( RenderSubmission_BeginFrame( &frontend, 81u ) );
	CHECK( RenderSubmission_RenderScene( &frontend, &view, 0 ) );
	motion.generation = 81u;
	CHECK( RenderSubmission_AddEntity( &frontend, &modelEntity, &motion ) );
	CHECK( RenderSubmission_AddEntity( &frontend, &spriteEntity, NULL ) );
	CHECK( RenderSubmission_AddEntity( &frontend, &beamEntity, NULL ) );
	CHECK( RenderSubmission_AddUiQuad( &frontend, 0.0f, 0.0f, 1280.0f, 720.0f,
		0.0f, 0.0f, 1.0f, 1.0f, 0.0f, material ) );
	CHECK( RenderSubmission_AddUiLine( &frontend, 10.0f, 20.0f, 200.0f, 220.0f,
		2.0f, msdf ) );
	{
		polyVert_t poly[3]; memset( poly, 0, sizeof( poly ) );
		CHECK( RenderSubmission_AddPoly( &frontend, material, 3, poly, 1 ) );
	}
	CHECK( RenderSubmission_AddLight( &frontend, spriteEntity.origin, NULL,
		64.0f, 1.0f, 0.5f, 0.25f ) );
	CHECK( RenderSubmission_EndFrame( &frontend, 81u, &submission ) );
	CHECK( RalWebGpu_ProductRender( product, &runtimeReceipt, &frontend,
		&submission, 0x8002u, 81u, &productReceipt ) );
	CHECK( productReceipt.uploadedMaterialCount == 1u
		&& productReceipt.reusedMaterialCount == 2u
		&& host.textureWrites == 4u && host.bufferWrites == 18u
		&& host.draws == 27u );
	CHECK( RalWebGpu_ProductPoll( product, &productReceipt ) == RAL_WEBGPU_ASYNC_READY );
	memset( &productOutput, 0xa5, sizeof( productOutput ) ); productExact = productOutput;
	host.failDraw = qtrue;
	CHECK( !RalWebGpu_ProductRender( product, &runtimeReceipt, &frontend,
		&submission, 0x8003u, 82u, &productOutput )
		&& !memcmp( &productOutput, &productExact, sizeof( productOutput ) ) );
	host.failDraw = qfalse;
	RalWebGpu_ProductDestroy( product, &runtimeReceipt );
	RenderSubmission_Reset( &frontend ); RalWebGpu_RuntimeDestroy( runtime );
	CHECK( host.resourcesCreated == host.resourcesDestroyed );
	puts( "ral WebGPU sealed frontend plan contract: PASS" ); return 0;
}
