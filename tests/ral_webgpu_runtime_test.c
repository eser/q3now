// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "ral_webgpu_runtime.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(x) do { if ( !(x) ) { fprintf( stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #x ); return 1; } } while ( 0 )

typedef struct {
	uintptr_t nextIdentity;
	uint32_t trace[64], traceCount;
} fakeHost_t;

static void Trace( fakeHost_t *host, uint32_t value ) {
	if ( host->traceCount < 64u ) host->trace[host->traceCount++] = value;
}
static qboolean BeginAdapter( void *user, uint64_t generation ) {
	(void)user; return generation ? qtrue : qfalse;
}
static qboolean PollAdapter( void *user, uint64_t generation,
		ralWebGpuAdapterPoll_t *out ) {
	(void)user; (void)generation; memset( out, 0, sizeof( *out ) );
	out->status = RAL_WEBGPU_REQUEST_READY; out->adapterIdentity = 0x2000u;
	out->adapterType = RAL_ADAPTER_TYPE_DISCRETE;
	out->vendorName = "Wired"; out->deviceName = "WebGPU runtime fixture";
	out->limits.maxColorAttachments = 8u; out->limits.maxTextureDimension2D = 8192u;
	out->limits.maxTextureDimension3D = 2048u; out->limits.maxTextureArrayLayers = 2048u;
	out->limits.maxComputeInvocationsPerWorkgroup = 256u;
	out->limits.maxSampledTexturesPerShaderStage = 32u;
	out->limits.maxBindGroups = 4u; out->limits.maxBindingsPerBindGroup = 16u;
	out->limits.maxStorageBufferBindingSize = UINT64_C(134217728);
	out->limits.minUniformBufferOffsetAlignment = 256u;
	out->limits.minStorageBufferOffsetAlignment = 256u; return qtrue;
}
static qboolean BeginDevice( void *user, uintptr_t adapter, uint64_t generation ) {
	(void)user; return adapter == 0x2000u && generation > 1u;
}
static qboolean PollDevice( void *user, uint64_t generation,
		ralWebGpuDevicePoll_t *out ) {
	(void)user; (void)generation; memset( out, 0, sizeof( *out ) );
	out->status = RAL_WEBGPU_REQUEST_READY; out->deviceIdentity = 0x3000u;
	out->queueIdentity = 0x4000u; return qtrue;
}
static void ReleaseDevice( void *user, uintptr_t device, uintptr_t queue ) {
	if ( device && queue ) Trace( (fakeHost_t *)user, 30u );
}
static void ReleaseAdapter( void *user, uintptr_t adapter ) {
	if ( adapter ) Trace( (fakeHost_t *)user, 40u );
}

static uintptr_t NewIdentity( fakeHost_t *host ) { return ++host->nextIdentity; }
static qboolean CreateBuffer( void *user, uintptr_t device,
		const ralWebGpuBufferDesc_t *desc, uintptr_t *out ) {
	if ( device != 0x3000u || !desc || !desc->size ) return qfalse;
	*out = NewIdentity( (fakeHost_t *)user ); return qtrue;
}
static qboolean CreateTexture( void *user, uintptr_t device,
		const ralWebGpuTextureDesc_t *desc, uintptr_t *out ) {
	if ( device != 0x3000u || !desc ) return qfalse;
	*out = NewIdentity( (fakeHost_t *)user ); return qtrue;
}
static qboolean CreateSampler( void *user, uintptr_t device,
		const ralWebGpuSamplerDesc_t *desc, uintptr_t *out ) {
	if ( device != 0x3000u || !desc ) return qfalse;
	*out = NewIdentity( (fakeHost_t *)user ); return qtrue;
}
static void DestroyResource( void *user, ralWebGpuResourceKind_t kind,
		uintptr_t identity ) {
	if ( kind && identity ) Trace( (fakeHost_t *)user, 20u );
}
static qboolean WriteBuffer( void *u, uintptr_t q, uintptr_t b, uint64_t o,
		const void *p, uint64_t n ) {
	(void)u; (void)q; (void)b; (void)o; (void)p; (void)n; return qtrue;
}
static qboolean WriteTexture( void *u, uintptr_t q, uintptr_t t, const void *b,
		uint64_t n, uint32_t r, uint32_t i ) {
	(void)u; (void)q; (void)t; (void)b; (void)n; (void)r; (void)i; return qtrue;
}
static qboolean BeginRoundTrip( void *u, uintptr_t d, uintptr_t q, uintptr_t a,
		uintptr_t b, const void *p, uint64_t n, uint64_t g, uintptr_t *out ) {
	(void)d; (void)q; (void)a; (void)b; (void)p; (void)n; (void)g;
	*out = NewIdentity( (fakeHost_t *)u ); return qtrue;
}
static qboolean PollRoundTrip( void *u, uintptr_t o, uint64_t g, void *b,
		uint64_t c, uint64_t *n, ralWebGpuAsyncStatus_t *s ) {
	(void)u; (void)o; (void)g; (void)b; (void)c; (void)n; *s = RAL_WEBGPU_ASYNC_FAILED; return qtrue;
}
static void ReleaseOperation( void *u, uintptr_t o ) { (void)u; (void)o; }

static qboolean BeginEncoder( void *u, uintptr_t d, uintptr_t *o ) { (void)u; (void)d; (void)o; return qfalse; }
static qboolean BeginPass( void *u, uintptr_t e, ralWebGpuPassKind_t k, uintptr_t t, uintptr_t *o ) { (void)u; (void)e; (void)k; (void)t; (void)o; return qfalse; }
static qboolean RecordDraw( void *u, uintptr_t p,
		const ralWebGpuIndexedDraw_t *d ) { (void)u; (void)p; (void)d; return qfalse; }
static qboolean RecordDispatch( void *u, uintptr_t p,
		const ralWebGpuComputeDispatch_t *d ) { (void)u; (void)p; (void)d; return qfalse; }
static qboolean EndPass( void *u, uintptr_t p ) { (void)u; (void)p; return qfalse; }
static qboolean FinishEncoder( void *u, uintptr_t e, uintptr_t *o ) { (void)u; (void)e; (void)o; return qfalse; }
static qboolean Submit( void *u, uintptr_t q, uintptr_t b, uint64_t g, uintptr_t *o ) { (void)u; (void)q; (void)b; (void)g; (void)o; return qfalse; }
static qboolean PollSubmission( void *u, uintptr_t s, uint64_t g, ralWebGpuAsyncStatus_t *o ) { (void)u; (void)s; (void)g; (void)o; return qfalse; }
static void ReleaseCommand( void *u, ralWebGpuCommandObjectKind_t k, uintptr_t o ) { (void)u; (void)k; (void)o; }
static qboolean Configure( void *u, uintptr_t c, uintptr_t d, uint32_t w, uint32_t h,
		ralFormat_t f, ralColorSpace_t s, qboolean a ) { (void)u; (void)c; (void)d; (void)w; (void)h; (void)f; (void)s; (void)a; return qtrue; }
static void Unconfigure( void *u, uintptr_t c ) { (void)u; (void)c; }
static qboolean Acquire( void *u, uintptr_t c, uint64_t g, uintptr_t *o ) { (void)u; (void)c; (void)g; (void)o; return qfalse; }
static qboolean Present( void *u, uintptr_t c, uintptr_t t, uint64_t f, uint64_t s ) { (void)u; (void)c; (void)t; (void)f; (void)s; return qfalse; }

static qboolean CreateShader( void *u, uintptr_t d,
		const ralWebGpuShaderModuleDesc_t *x, uintptr_t *o ) {
	if ( d != 0x3000u || !x ) return qfalse; *o = NewIdentity( (fakeHost_t *)u ); return qtrue;
}
static qboolean CreateGroup( void *u, uintptr_t d, uint32_t g,
		const ralWebGpuBindGroupLayoutEntry_t *e, uint32_t n, uintptr_t *o ) {
	if ( d != 0x3000u || g || !e || n != 1u ) return qfalse;
	*o = NewIdentity( (fakeHost_t *)u ); return qtrue;
}
static qboolean CreateLayout( void *u, uintptr_t d, const uintptr_t *l,
		uint32_t n, uintptr_t *o ) {
	if ( d != 0x3000u || !l || n != 1u ) return qfalse;
	*o = NewIdentity( (fakeHost_t *)u ); return qtrue;
}
static qboolean CreatePipeline( void *u, uintptr_t d,
		const ralWebGpuNativePipelineDesc_t *x, uintptr_t *o ) {
	if ( d != 0x3000u || !x ) return qfalse; *o = NewIdentity( (fakeHost_t *)u ); return qtrue;
}
static void DestroyPipelineObject( void *u, ralWebGpuPipelineObjectKind_t k,
		uintptr_t o ) { if ( k && o ) Trace( (fakeHost_t *)u, 10u + (uint32_t)k ); }

static ralWebGpuRuntimeCreateInfo_t RuntimeInfo( fakeHost_t *host,
		uintptr_t canvas ) {
	ralWebGpuRuntimeCreateInfo_t info;
	memset( &info, 0, sizeof( info ) );
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
	info.command.host.recordComputeDispatch = RecordDispatch;
	info.command.host.endPass = EndPass;
	info.command.host.finishEncoder = FinishEncoder; info.command.host.submit = Submit;
	info.command.host.pollSubmission = PollSubmission; info.command.host.releaseObject = ReleaseCommand;
	info.presentation.userData = host; info.presentation.canvasIdentity = canvas;
	info.presentation.host.configure = Configure; info.presentation.host.unconfigure = Unconfigure;
	info.presentation.host.acquire = Acquire; info.presentation.host.present = Present; return info;
}

static void Artifact( ralShaderArtifactAbi_t *a, ralShaderArtifactTarget_t t,
		const void *b, uint32_t n ) {
	memset( a, 0, sizeof( *a ) ); a->target = t;
	if ( b ) { a->byteCount = n; if ( Ral_ShaderArtifactDigest( b, n, &a->digest ) != ralSuccess ) abort(); }
}

int main( void ) {
	static const char wgsl[] = "struct U{x:vec4<u32>,};\n@group(0) @binding(0)\nvar<uniform> u:U;\n@compute @workgroup_size(1) fn main(){let x=u.x.x;}\n";
	fakeHost_t host = { 0 }, partial = { 0 };
	ralWebGpuRuntimeCreateInfo_t info;
	ralWebGpuRuntime_t *runtime = NULL, *partialRuntime = NULL;
	ralWebGpuRuntimeReceipt_t runtimeReceipt, updatedRuntimeReceipt;
	ralWebGpuResourceLayer_t *resources;
	ralWebGpuBufferDesc_t bufferDesc = { 16u, RAL_WEBGPU_BUFFER_STORAGE, RAL_ALLOCATION_DEVICE_LOCAL };
	ralWebGpuResource_t *buffer = NULL; ralWebGpuResourceReceipt_t bufferReceipt;
	uint32_t spirv[4] = { 0x07230203u, 0u, 0u, 0u };
	ralShaderModuleAbi_t module; ralShaderBindingAbi_t binding;
	ralShaderAbiManifest_t manifest; ralShaderVariantAbi_t variant;
	const ralBindGroupLayout_t *layouts[1] = { (const ralBindGroupLayout_t *)(uintptr_t)1u };
	ralComputePipelineCreateInfo_t state; ralWebGpuWgslModule_t source;
	ralWebGpuPipelineCreateInfo_t pipelineInfo; ralWebGpuPipeline_t *pipeline;
	ralWebGpuPipelineReceipt_t pipelineReceipt;
	ralMemoryFailureEvent_t event; ralMemoryFailureReceipt_t loss;
	uint32_t i, resourceIndex = UINT32_MAX, deviceIndex = UINT32_MAX, adapterIndex = UINT32_MAX;
	host.nextIdentity = 0x5000u; info = RuntimeInfo( &host, 0x7000u );
	CHECK( RalWebGpu_RuntimeBegin( &info, &runtime ) );
	while ( RalWebGpu_RuntimePoll( runtime, &runtimeReceipt )
			== RAL_WEBGPU_POLL_PENDING ) {}
	CHECK( RalWebGpu_RuntimePoll( runtime, &runtimeReceipt ) == RAL_WEBGPU_POLL_READY );
	CHECK( RalWebGpu_RuntimeReceiptExact( &runtimeReceipt, &runtimeReceipt ) );
	resources = RalWebGpu_RuntimeResources( runtime, &runtimeReceipt ); CHECK( resources );
	CHECK( RalWebGpu_CreateBuffer( resources, &bufferDesc, &buffer, &bufferReceipt ) );
	memset( &module, 0, sizeof( module ) ); module.stage = RAL_STAGE_COMPUTE;
	module.entryPoint = "main"; module.sourceDigest = (ralShaderDigest_t){ 1u, 2u };
	Artifact( &module.artifacts[0], RAL_SHADER_ARTIFACT_SPIRV, spirv, sizeof( spirv ) );
	Artifact( &module.artifacts[1], RAL_SHADER_ARTIFACT_MSL, NULL, 0u );
	Artifact( &module.artifacts[2], RAL_SHADER_ARTIFACT_WGSL, wgsl, sizeof( wgsl ) - 1u );
	memset( &binding, 0, sizeof( binding ) ); binding.bindingClass = RAL_SHADER_BIND_UNIFORM_BUFFER;
	binding.arrayCount = 1u; binding.stageFlags = RAL_STAGE_COMPUTE; binding.minBufferBindingSize = 16u;
	memset( &manifest, 0, sizeof( manifest ) ); manifest.schemaVersion = 1u; manifest.generation = 9u;
	manifest.pipelineKind = RAL_SHADER_PIPELINE_COMPUTE; manifest.modules = &module; manifest.moduleCount = 1u;
	manifest.bindings = &binding; manifest.bindingCount = 1u; manifest.inlineData.byteSize = 16u;
	manifest.inlineData.stageFlags = RAL_STAGE_COMPUTE;
	memset( &state, 0, sizeof( state ) ); state.bindGroupLayouts = layouts;
	state.numBindGroupLayouts = 1u; state.pushConstantSize = 16u;
	memset( &variant, 0, sizeof( variant ) ); variant.artifactTarget = RAL_SHADER_ARTIFACT_WGSL;
	variant.generation = 3u; CHECK( Ral_ComputePipelineSemanticDigest( &state, &variant.semanticStateDigest ) == ralSuccess );
	source = (ralWebGpuWgslModule_t){ wgsl, sizeof( wgsl ) - 1u };
	memset( &pipelineInfo, 0, sizeof( pipelineInfo ) ); pipelineInfo.userData = &host;
	pipelineInfo.host.createShaderModule = CreateShader; pipelineInfo.host.createBindGroupLayout = CreateGroup;
	pipelineInfo.host.createPipelineLayout = CreateLayout; pipelineInfo.host.createPipeline = CreatePipeline;
	pipelineInfo.host.destroyObject = DestroyPipelineObject; pipelineInfo.manifest = &manifest;
	pipelineInfo.variant = &variant; pipelineInfo.modules = &source; pipelineInfo.moduleCount = 1u;
	pipelineInfo.computeState = &state; pipelineInfo.generation = 10u;
	CHECK( RalWebGpu_RuntimeCreatePipeline( runtime, &runtimeReceipt, &pipelineInfo, &pipeline, &pipelineReceipt ) );
	CHECK( !RalWebGpu_RuntimeResources( runtime, &runtimeReceipt ) );
	CHECK( RalWebGpu_RuntimeGetReceipt( runtime, &updatedRuntimeReceipt ) );
	CHECK( updatedRuntimeReceipt.pipelineCount == 1u );
	CHECK( RalWebGpu_RuntimeResources( runtime, &updatedRuntimeReceipt ) == resources );
	CHECK( !RalWebGpu_RuntimeReceiptExact( &runtimeReceipt, &updatedRuntimeReceipt ) );
	memset( &event, 0, sizeof( event ) ); event.backendType = RAL_BACKEND_WEBGPU;
	event.cause = RAL_MEMORY_FAILURE_DEVICE_LOST; event.memoryClass = RAL_ALLOCATION_DEVICE_LOCAL;
	event.residency = RAL_ALLOCATION_RESIDENCY_PERMANENT; event.criticality = RAL_MEMORY_CRITICALITY_REQUIRED;
	event.requestedBytes = 16u; event.attempt = 1u; event.maxAttempts = 1u; event.liveParent = qtrue;
	CHECK( RalWebGpu_RuntimePublishDeviceLoss( runtime, &event, &loss ) );
	RalWebGpu_RuntimeDestroy( runtime );
	for ( i = 0u; i < host.traceCount; ++i ) {
		if ( host.trace[i] == 20u ) resourceIndex = i;
		if ( host.trace[i] == 30u ) deviceIndex = i;
		if ( host.trace[i] == 40u ) adapterIndex = i;
	}
	CHECK( host.traceCount >= 7u && host.trace[0] >= 11u && host.trace[0] <= 14u );
	CHECK( resourceIndex != UINT32_MAX && deviceIndex > resourceIndex && adapterIndex > deviceIndex );
	partial.nextIdentity = 0x9000u; info = RuntimeInfo( &partial, 0u );
	CHECK( RalWebGpu_RuntimeBegin( &info, &partialRuntime ) );
	while ( RalWebGpu_RuntimePoll( partialRuntime, &runtimeReceipt )
			== RAL_WEBGPU_POLL_PENDING ) {}
	CHECK( RalWebGpu_RuntimePoll( partialRuntime, &runtimeReceipt )
		== RAL_WEBGPU_POLL_FAILED );
	CHECK( partial.traceCount == 2u && partial.trace[0] == 30u && partial.trace[1] == 40u );
	RalWebGpu_RuntimeDestroy( partialRuntime );
	puts( "ral WebGPU aggregate runtime ownership contract: PASS" ); return 0;
}
