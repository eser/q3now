// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "ral_webgpu_pipeline.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(x) do { if ( !(x) ) { fprintf( stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #x ); return 1; } } while ( 0 )

typedef struct {
	uintptr_t nextIdentity;
	uint32_t created[5], destroyed[5];
	ralWebGpuPipelineObjectKind_t failKind;
} fakeHost_t;

typedef struct {
	uint32_t vertexSpirv[4], fragmentSpirv[4];
	ralShaderModuleAbi_t modules[2];
	ralShaderBindingAbi_t bindings[3];
	ralShaderVertexInputAbi_t input;
	ralShaderAbiManifest_t manifest;
	ralShaderVariantAbi_t variant;
	ralWebGpuWgslModule_t wgsl[2];
	ralVertexBinding_t vertexBinding;
	ralVertexAttribute_t vertexAttribute;
	ralColorBlendAttachment_t blend;
	const ralBindGroupLayout_t *layouts[1];
	ralGraphicsPipelineCreateInfo_t state;
} graphicsFixture_t;

typedef struct {
	uint32_t spirv[4];
	ralShaderModuleAbi_t module;
	ralShaderBindingAbi_t binding;
	ralShaderAbiManifest_t manifest;
	ralShaderVariantAbi_t variant;
	ralWebGpuWgslModule_t wgsl;
	const ralBindGroupLayout_t *layouts[1];
	ralComputePipelineCreateInfo_t state;
} computeFixture_t;

static const char GRAPHICS_VERTEX[] =
	"struct U { value: vec4<f32>, };\n"
	"@group(0) @binding(0)\nvar<uniform> u: U;\n"
	"@vertex fn main(@location(0) p: vec3<f32>) -> @builtin(position) vec4<f32> { return vec4<f32>(p, 1.0); }\n";
static const char GRAPHICS_FRAGMENT[] =
	"struct U { value: vec4<f32>, };\n"
	"@group(0) @binding(0)\nvar<uniform> u: U;\n"
	"@group(0) @binding(1)\nvar image: texture_2d<f32>;\n"
	"@group(0) @binding(2)\nvar imageSampler: sampler;\n"
	"@fragment fn main() -> @location(0) vec4<f32> { return u.value; }\n";
static const char COMPUTE[] =
	"struct U { value: vec4<u32>, };\n"
	"@group(0) @binding(0)\nvar<uniform> u: U;\n"
	"@compute @workgroup_size(1) fn main() { let value = u.value.x; }\n";

static qboolean BeginAdapter( void *user, uint64_t generation ) {
	(void)user; return generation ? qtrue : qfalse;
}

static qboolean PollAdapter( void *user, uint64_t generation,
		ralWebGpuAdapterPoll_t *out ) {
	(void)user; (void)generation; memset( out, 0, sizeof( *out ) );
	out->status = RAL_WEBGPU_REQUEST_READY;
	out->adapterIdentity = (uintptr_t)0x2000u;
	out->adapterType = RAL_ADAPTER_TYPE_DISCRETE;
	out->vendorName = "Wired"; out->deviceName = "WebGPU pipeline fixture";
	out->limits.maxColorAttachments = 8u;
	out->limits.maxTextureDimension2D = 8192u;
	out->limits.maxTextureDimension3D = 2048u;
	out->limits.maxTextureArrayLayers = 2048u;
	out->limits.maxComputeInvocationsPerWorkgroup = 256u;
	out->limits.maxSampledTexturesPerShaderStage = 32u;
	out->limits.maxBindGroups = 4u;
	out->limits.maxBindingsPerBindGroup = 16u;
	out->limits.maxStorageBufferBindingSize = UINT64_C(134217728);
	out->limits.minUniformBufferOffsetAlignment = 256u;
	out->limits.minStorageBufferOffsetAlignment = 256u;
	return qtrue;
}

static qboolean BeginDevice( void *user, uintptr_t adapter, uint64_t generation ) {
	(void)user; return adapter == (uintptr_t)0x2000u && generation > 1u;
}

static qboolean PollDevice( void *user, uint64_t generation,
		ralWebGpuDevicePoll_t *out ) {
	(void)user; (void)generation; memset( out, 0, sizeof( *out ) );
	out->status = RAL_WEBGPU_REQUEST_READY;
	out->deviceIdentity = (uintptr_t)0x3000u;
	out->queueIdentity = (uintptr_t)0x4000u;
	return qtrue;
}

static void ReleaseDevice( void *user, uintptr_t device, uintptr_t queue ) {
	(void)user; (void)device; (void)queue;
}

static void ReleaseAdapter( void *user, uintptr_t adapter ) {
	(void)user; (void)adapter;
}

static qboolean CreateIdentity( fakeHost_t *host,
		ralWebGpuPipelineObjectKind_t kind, uintptr_t *out ) {
	if ( host->failKind == kind ) return qfalse;
	host->created[kind]++; *out = ++host->nextIdentity; return qtrue;
}

static qboolean CreateShaderModule( void *user, uintptr_t device,
		const ralWebGpuShaderModuleDesc_t *desc, uintptr_t *out ) {
	fakeHost_t *host = (fakeHost_t *)user;
	if ( device != (uintptr_t)0x3000u || !desc || !desc->entryPoint
			|| strcmp( desc->entryPoint, "main" ) || !desc->code
			|| !desc->byteCount || !Ral_ShaderDigestValid( &desc->digest ) ) return qfalse;
	return CreateIdentity( host, RAL_WEBGPU_PIPELINE_OBJECT_SHADER_MODULE, out );
}

static qboolean CreateBindGroupLayout( void *user, uintptr_t device,
		uint32_t group, const ralWebGpuBindGroupLayoutEntry_t *entries,
		uint32_t entryCount, uintptr_t *out ) {
	fakeHost_t *host = (fakeHost_t *)user;
	uint32_t i;
	if ( device != (uintptr_t)0x3000u || group != 0u || !entries
			|| !entryCount || entryCount > 3u ) return qfalse;
	for ( i = 0u; i < entryCount; ++i )
		if ( entries[i].binding != i || !entries[i].arrayCount ) return qfalse;
	return CreateIdentity( host,
		RAL_WEBGPU_PIPELINE_OBJECT_BIND_GROUP_LAYOUT, out );
}

static qboolean CreatePipelineLayout( void *user, uintptr_t device,
		const uintptr_t *layouts, uint32_t layoutCount, uintptr_t *out ) {
	fakeHost_t *host = (fakeHost_t *)user;
	if ( device != (uintptr_t)0x3000u || !layouts || layoutCount != 1u
			|| !layouts[0] ) return qfalse;
	return CreateIdentity( host, RAL_WEBGPU_PIPELINE_OBJECT_PIPELINE_LAYOUT, out );
}

static qboolean CreatePipeline( void *user, uintptr_t device,
		const ralWebGpuNativePipelineDesc_t *desc, uintptr_t *out ) {
	fakeHost_t *host = (fakeHost_t *)user;
	if ( device != (uintptr_t)0x3000u || !desc || !desc->pipelineLayout
			|| desc->bindGroupLayoutCount != 1u
			|| ( desc->kind == RAL_SHADER_PIPELINE_GRAPHICS
				&& ( desc->shaderModuleCount != 2u || !desc->graphicsState
					|| desc->computeState ) )
			|| ( desc->kind == RAL_SHADER_PIPELINE_COMPUTE
				&& ( desc->shaderModuleCount != 1u || !desc->computeState
					|| desc->graphicsState ) ) ) return qfalse;
	return CreateIdentity( host, RAL_WEBGPU_PIPELINE_OBJECT_PIPELINE, out );
}

static void DestroyObject( void *user, ralWebGpuPipelineObjectKind_t kind,
		uintptr_t identity ) {
	fakeHost_t *host = (fakeHost_t *)user;
	if ( kind >= RAL_WEBGPU_PIPELINE_OBJECT_SHADER_MODULE
			&& kind <= RAL_WEBGPU_PIPELINE_OBJECT_PIPELINE && identity )
		host->destroyed[kind]++;
}

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
		const uint32_t *spirv, const char *wgsl, uint32_t wgslBytes,
		uint64_t seed ) {
	memset( module, 0, sizeof( *module ) );
	module->stage = stage; module->entryPoint = "main";
	module->sourceDigest.lane0 = seed; module->sourceDigest.lane1 = seed + 1u;
	Artifact( &module->artifacts[RAL_SHADER_ARTIFACT_SPIRV],
		RAL_SHADER_ARTIFACT_SPIRV, spirv, 16u );
	Artifact( &module->artifacts[RAL_SHADER_ARTIFACT_MSL],
		RAL_SHADER_ARTIFACT_MSL, NULL, 0u );
	Artifact( &module->artifacts[RAL_SHADER_ARTIFACT_WGSL],
		RAL_SHADER_ARTIFACT_WGSL, wgsl, wgslBytes );
}

static void GraphicsSetup( graphicsFixture_t *fixture ) {
	memset( fixture, 0, sizeof( *fixture ) );
	fixture->vertexSpirv[0] = 0x07230203u;
	fixture->fragmentSpirv[0] = 0x07230203u;
	Module( &fixture->modules[0], RAL_STAGE_VERTEX, fixture->vertexSpirv,
		GRAPHICS_VERTEX, sizeof( GRAPHICS_VERTEX ) - 1u, 11u );
	Module( &fixture->modules[1], RAL_STAGE_FRAGMENT, fixture->fragmentSpirv,
		GRAPHICS_FRAGMENT, sizeof( GRAPHICS_FRAGMENT ) - 1u, 21u );
	fixture->bindings[0] = (ralShaderBindingAbi_t){ 0u, 0u,
		RAL_SHADER_BIND_UNIFORM_BUFFER, 1u, RAL_STAGE_ALL_GRAPHICS, 16u,
		0, 0, RAL_FORMAT_UNDEFINED, qfalse };
	fixture->bindings[1] = (ralShaderBindingAbi_t){ 0u, 1u,
		RAL_SHADER_BIND_SAMPLED_TEXTURE, 1u, RAL_STAGE_FRAGMENT, 0u,
		RAL_SHADER_VIEW_2D, RAL_SHADER_SAMPLE_FLOAT, RAL_FORMAT_UNDEFINED, qfalse };
	fixture->bindings[2] = (ralShaderBindingAbi_t){ 0u, 2u,
		RAL_SHADER_BIND_FILTERING_SAMPLER, 1u, RAL_STAGE_FRAGMENT, 0u,
		0, 0, RAL_FORMAT_UNDEFINED, qfalse };
	fixture->input.location = 0u;
	fixture->input.format = RAL_FORMAT_R32G32B32_SFLOAT;
	fixture->manifest.schemaVersion = RAL_SHADER_ABI_SCHEMA_VERSION;
	fixture->manifest.generation = 17u;
	fixture->manifest.pipelineKind = RAL_SHADER_PIPELINE_GRAPHICS;
	fixture->manifest.modules = fixture->modules; fixture->manifest.moduleCount = 2u;
	fixture->manifest.bindings = fixture->bindings; fixture->manifest.bindingCount = 3u;
	fixture->manifest.vertexInputs = &fixture->input;
	fixture->manifest.vertexInputCount = 1u;
	fixture->manifest.inlineData.byteSize = 16u;
	fixture->manifest.inlineData.stageFlags = RAL_STAGE_ALL_GRAPHICS;
	fixture->manifest.inlineData.webgpuUniformSet = 0u;
	fixture->manifest.inlineData.webgpuUniformBinding = 0u;
	fixture->wgsl[0] = (ralWebGpuWgslModule_t){ GRAPHICS_VERTEX,
		sizeof( GRAPHICS_VERTEX ) - 1u };
	fixture->wgsl[1] = (ralWebGpuWgslModule_t){ GRAPHICS_FRAGMENT,
		sizeof( GRAPHICS_FRAGMENT ) - 1u };
	fixture->vertexBinding = (ralVertexBinding_t){ 0u, 12u,
		RAL_VERTEX_INPUT_PER_VERTEX };
	fixture->vertexAttribute = (ralVertexAttribute_t){ 0u, 0u,
		RAL_FORMAT_R32G32B32_SFLOAT, 0u };
	fixture->blend.writeMask = RAL_COLOR_WRITE_ALL;
	fixture->blend.writeMaskExplicit = qtrue;
	fixture->layouts[0] = (const ralBindGroupLayout_t *)(uintptr_t)1u;
	fixture->state.vertexBindings = &fixture->vertexBinding;
	fixture->state.numVertexBindings = 1u;
	fixture->state.vertexAttributes = &fixture->vertexAttribute;
	fixture->state.numVertexAttributes = 1u;
	fixture->state.topology = RAL_TOPOLOGY_TRIANGLE_LIST;
	fixture->state.raster.polygonMode = RAL_POLYGON_FILL;
	fixture->state.raster.lineWidth = 1.0f;
	fixture->state.colorBlends = &fixture->blend;
	fixture->state.numColorBlends = 1u;
	fixture->state.colorFormats[0] = RAL_FORMAT_R8G8B8A8_UNORM;
	fixture->state.numColorFormats = 1u;
	fixture->state.sampleCount = 1u;
	fixture->state.bindGroupLayouts = fixture->layouts;
	fixture->state.numBindGroupLayouts = 1u;
	fixture->state.pushConstantSize = 16u;
	fixture->state.pushConstantStages = RAL_STAGE_ALL_GRAPHICS;
	fixture->variant.artifactTarget = RAL_SHADER_ARTIFACT_WGSL;
	fixture->variant.generation = 3u;
	if ( Ral_GraphicsPipelineSemanticDigest( &fixture->state,
			&fixture->variant.semanticStateDigest ) != ralSuccess ) abort();
}

static void ComputeSetup( computeFixture_t *fixture ) {
	memset( fixture, 0, sizeof( *fixture ) ); fixture->spirv[0] = 0x07230203u;
	Module( &fixture->module, RAL_STAGE_COMPUTE, fixture->spirv,
		COMPUTE, sizeof( COMPUTE ) - 1u, 31u );
	fixture->binding = (ralShaderBindingAbi_t){ 0u, 0u,
		RAL_SHADER_BIND_UNIFORM_BUFFER, 1u, RAL_STAGE_COMPUTE, 16u,
		0, 0, RAL_FORMAT_UNDEFINED, qfalse };
	fixture->manifest.schemaVersion = RAL_SHADER_ABI_SCHEMA_VERSION;
	fixture->manifest.generation = 18u;
	fixture->manifest.pipelineKind = RAL_SHADER_PIPELINE_COMPUTE;
	fixture->manifest.modules = &fixture->module; fixture->manifest.moduleCount = 1u;
	fixture->manifest.bindings = &fixture->binding; fixture->manifest.bindingCount = 1u;
	fixture->manifest.inlineData.byteSize = 16u;
	fixture->manifest.inlineData.stageFlags = RAL_STAGE_COMPUTE;
	fixture->wgsl = (ralWebGpuWgslModule_t){ COMPUTE, sizeof( COMPUTE ) - 1u };
	fixture->layouts[0] = (const ralBindGroupLayout_t *)(uintptr_t)1u;
	fixture->state.bindGroupLayouts = fixture->layouts;
	fixture->state.numBindGroupLayouts = 1u;
	fixture->state.pushConstantSize = 16u;
	fixture->variant.artifactTarget = RAL_SHADER_ARTIFACT_WGSL;
	fixture->variant.generation = 4u;
	if ( Ral_ComputePipelineSemanticDigest( &fixture->state,
			&fixture->variant.semanticStateDigest ) != ralSuccess ) abort();
}

static ralWebGpuCoreCreateInfo_t CoreInfo( fakeHost_t *host,
		uint64_t generation ) {
	ralWebGpuCoreCreateInfo_t info;
	memset( &info, 0, sizeof( info ) ); info.generation = generation;
	info.userData = host; info.host.beginAdapter = BeginAdapter;
	info.host.pollAdapter = PollAdapter; info.host.beginDevice = BeginDevice;
	info.host.pollDevice = PollDevice; info.host.releaseDevice = ReleaseDevice;
	info.host.releaseAdapter = ReleaseAdapter; return info;
}

static ralWebGpuPipelineCreateInfo_t PipelineInfo( fakeHost_t *host ) {
	ralWebGpuPipelineCreateInfo_t info;
	memset( &info, 0, sizeof( info ) ); info.userData = host;
	info.host.createShaderModule = CreateShaderModule;
	info.host.createBindGroupLayout = CreateBindGroupLayout;
	info.host.createPipelineLayout = CreatePipelineLayout;
	info.host.createPipeline = CreatePipeline;
	info.host.destroyObject = DestroyObject; return info;
}

static qboolean ReadyCore( fakeHost_t *host, uint64_t generation,
		ralWebGpuCore_t **outCore, ralWebGpuCoreReceipt_t *outReceipt ) {
	ralWebGpuCoreCreateInfo_t info = CoreInfo( host, generation );
	if ( !RalWebGpu_CoreBegin( &info, outCore ) ) return qfalse;
	while ( RalWebGpu_CorePoll( *outCore, outReceipt ) == RAL_WEBGPU_POLL_PENDING ) {}
	return RalWebGpu_CoreMatchesReceipt( *outCore, outReceipt );
}

static ralMemoryFailureEvent_t DeviceLoss( void ) {
	ralMemoryFailureEvent_t event;
	memset( &event, 0, sizeof( event ) );
	event.backendType = RAL_BACKEND_WEBGPU;
	event.cause = RAL_MEMORY_FAILURE_DEVICE_LOST;
	event.memoryClass = RAL_ALLOCATION_DEVICE_LOCAL;
	event.residency = RAL_ALLOCATION_RESIDENCY_PERMANENT;
	event.criticality = RAL_MEMORY_CRITICALITY_REQUIRED;
	event.requestedBytes = 16u; event.attempt = 1u; event.maxAttempts = 1u;
	event.liveParent = qtrue; return event;
}

int main( void ) {
	fakeHost_t host = { 0 }, failing = { 0 };
	graphicsFixture_t graphics;
	computeFixture_t compute;
	ralWebGpuCore_t *core = NULL, *replacement = NULL;
	ralWebGpuCoreReceipt_t coreReceipt, replacementReceipt;
	ralWebGpuPipelineCreateInfo_t info;
	ralWebGpuPipeline_t *pipeline = NULL, *computePipeline = NULL;
	ralWebGpuPipelineReceipt_t receipt, stale, computeReceipt, sentinel, output;
	ralMemoryFailureEvent_t event;
	ralMemoryFailureReceipt_t loss;
	host.nextIdentity = (uintptr_t)0x5000u;
	CHECK( ReadyCore( &host, 7u, &core, &coreReceipt ) );
	GraphicsSetup( &graphics ); info = PipelineInfo( &host );
	info.manifest = &graphics.manifest; info.variant = &graphics.variant;
	info.modules = graphics.wgsl; info.moduleCount = 2u;
	info.graphicsState = &graphics.state; info.generation = 41u;
	CHECK( RalWebGpu_PipelineCreate( core, &coreReceipt, &info,
		&pipeline, &receipt ) );
	CHECK( RalWebGpu_PipelineReceiptExact( &receipt, &receipt ) );
	CHECK( receipt.kind == RAL_SHADER_PIPELINE_GRAPHICS
		&& receipt.bindingCount == 3u && receipt.bindGroupLayoutCount == 1u );
	stale = receipt; stale.generation++;
	CHECK( !RalWebGpu_PipelineDestroy( core, pipeline, &stale ) );
	CHECK( RalWebGpu_PipelineDestroy( core, pipeline, &receipt ) );

	ComputeSetup( &compute ); info = PipelineInfo( &host );
	info.manifest = &compute.manifest; info.variant = &compute.variant;
	info.modules = &compute.wgsl; info.moduleCount = 1u;
	info.computeState = &compute.state; info.generation = 42u;
	CHECK( RalWebGpu_PipelineCreate( core, &coreReceipt, &info,
		&computePipeline, &computeReceipt ) );
	CHECK( computeReceipt.kind == RAL_SHADER_PIPELINE_COMPUTE );
	CHECK( RalWebGpu_PipelineDestroy( core, computePipeline, &computeReceipt ) );

	memset( &sentinel, 0xa5, sizeof( sentinel ) ); output = sentinel;
	event = DeviceLoss();
	CHECK( RalWebGpu_CorePublishDeviceLoss( core, &event, &loss ) );
	pipeline = (ralWebGpuPipeline_t *)(uintptr_t)0x1234u;
	info = PipelineInfo( &host ); info.manifest = &graphics.manifest;
	info.variant = &graphics.variant; info.modules = graphics.wgsl;
	info.moduleCount = 2u; info.graphicsState = &graphics.state; info.generation = 43u;
	CHECK( !RalWebGpu_PipelineCreate( core, &coreReceipt, &info,
		&pipeline, &output ) );
	CHECK( pipeline == (ralWebGpuPipeline_t *)(uintptr_t)0x1234u
		&& !memcmp( &output, &sentinel, sizeof( output ) ) );

	failing.nextIdentity = (uintptr_t)0x9000u;
	failing.failKind = RAL_WEBGPU_PIPELINE_OBJECT_PIPELINE;
	CHECK( ReadyCore( &failing, 8u, &replacement, &replacementReceipt ) );
	info = PipelineInfo( &failing ); info.manifest = &graphics.manifest;
	info.variant = &graphics.variant; info.modules = graphics.wgsl;
	info.moduleCount = 2u; info.graphicsState = &graphics.state; info.generation = 44u;
	pipeline = NULL; output = sentinel;
	CHECK( !RalWebGpu_PipelineCreate( replacement, &replacementReceipt, &info,
		&pipeline, &output ) );
	CHECK( !pipeline && !memcmp( &output, &sentinel, sizeof( output ) ) );
	CHECK( failing.created[RAL_WEBGPU_PIPELINE_OBJECT_SHADER_MODULE]
		== failing.destroyed[RAL_WEBGPU_PIPELINE_OBJECT_SHADER_MODULE] );
	CHECK( failing.created[RAL_WEBGPU_PIPELINE_OBJECT_BIND_GROUP_LAYOUT]
		== failing.destroyed[RAL_WEBGPU_PIPELINE_OBJECT_BIND_GROUP_LAYOUT] );
	CHECK( failing.created[RAL_WEBGPU_PIPELINE_OBJECT_PIPELINE_LAYOUT]
		== failing.destroyed[RAL_WEBGPU_PIPELINE_OBJECT_PIPELINE_LAYOUT] );
	RalWebGpu_CoreDestroy( replacement ); RalWebGpu_CoreDestroy( core );
	puts( "ral WebGPU WGSL binding/pipeline contract: PASS" );
	return 0;
}
