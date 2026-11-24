// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "ral_webgpu_product.h"
#include "maps/map_format_registry.h"

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#define CHECK(x) do { if ( !(x) ) { fprintf( stderr, "FAIL %s:%d: %s\n", \
	__FILE__, __LINE__, #x ); return 1; } } while ( 0 )

static ralIrradianceEntitySampleReceipt_t LocalIrradiance( uint64_t generation,
		uint64_t entityId ) {
	ralIrradianceEntitySampleReceipt_t r;
	memset( &r, 0, sizeof( r ) );
	r.schemaVersion = RAL_IRRADIANCE_ENTITY_RECEIPT_SCHEMA_VERSION;
	r.queryGeneration = generation; r.entityId = entityId;
	r.volumeId = 7u; r.layoutHash = 8u;
	r.probes.schemaVersion = RAL_IRRADIANCE_RECEIPT_SCHEMA_VERSION;
	r.probes.queryGeneration = generation; r.probes.productGeneration = 9u;
	r.probes.fallback = RAL_IRRADIANCE_FALLBACK_LIGHTGRID;
	r.probes.usedFallback = qtrue; r.probes.ready = qtrue;
	r.blendedCoefficientsQ16[0][0] = 32768;
	r.blendedCoefficientsQ16[0][1] = 65536;
	r.blendedCoefficientsQ16[0][2] = 131072;
	r.diffuseIrradianceQ16[0] = 32768;
	r.diffuseIrradianceQ16[1] = 65536;
	r.diffuseIrradianceQ16[2] = 131072;
	r.contributorHash = 10u;
	r.coefficientHash = Ral_IrradianceCoefficientHash( r.blendedCoefficientsQ16 );
	r.usedFallback = qtrue; r.ready = qtrue;
	return r;
}

typedef struct {
	uintptr_t nextIdentity;
	uint32_t resourcesCreated, resourcesDestroyed;
	uint32_t bufferWrites, textureWrites, draws, passes;
	uint32_t preserveColorDepthClears;
	qboolean passHasDepth;
	uint32_t uiPasses;
	uint32_t effectDraws, uiDraws, entityLightingDraws;
	uint32_t frameUiDraws, frameEntityDraws;
	uint32_t effectPipelineDraws[RAL_WEBGPU_EFFECT_PIPELINE_COUNT];
	uintptr_t worldPipeline;
	uintptr_t skyPipeline;
	uintptr_t skyCapturePipeline;
	uint32_t skyCaptureFaces, skyCaptureDraws, targetLayer;
	uint32_t skyDraws, skyTextureGroups, skyGroupsCreated, skyGroupsDestroyed;
	uintptr_t effectPipelines[RAL_WEBGPU_EFFECT_PIPELINE_COUNT];
	uintptr_t uiPipeline, msdfPipeline;
	uintptr_t entityLightingBuffer;
	uintptr_t worldMaterialBuffer;
	float entityLightingFirst[16];
	float worldMaterialFirst[4];
	uintptr_t submissionIdentity;
	uint64_t submissionGeneration;
	qboolean failDraw;
	uintptr_t lastVertexBuffer;
	uint64_t lastVertexBytes;
	renderWorldVertex_t lastVertices[128];
	uintptr_t effectParameterBuffer;
	qboolean failEffectParameterWrite;
	uint64_t effectParameterBytes;
	ralWebGpuEffectDrawParams_t effectParameters[1u + RENDER_MATERIAL_MAX_STAGES];
	uintptr_t beamTextures[RENDER_MATERIAL_MAX_STAGES];
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
	o->limits.maxBindingsPerBindGroup = 32u; o->limits.maxStorageBufferBindingSize = UINT64_C(134217728);
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
	host->resourcesCreated++; *o = NewIdentity( host );
	if ( x->usage & RAL_WEBGPU_BUFFER_VERTEX ) host->lastVertexBuffer = *o;
	return qtrue;
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
	fakeHost_t *host = u;
	if ( b == host->effectParameterBuffer ) {
		if (host->failEffectParameterWrite) return qfalse;
		host->effectParameterBytes = n;
		memcpy( host->effectParameters, p,
			n < sizeof(host->effectParameters) ? (size_t)n : sizeof(host->effectParameters) );
	}
	if ( b == host->lastVertexBuffer ) {
		host->lastVertexBytes = n;
		memcpy( host->lastVertices, p,
			n < sizeof( host->lastVertices ) ? (size_t)n : sizeof( host->lastVertices ) );
	}
	if ( b == ((fakeHost_t *)u)->entityLightingBuffer && n >= 16u * sizeof( float ) )
		memcpy( ((fakeHost_t *)u)->entityLightingFirst, p, 16u * sizeof( float ) );
	if ( b == ((fakeHost_t *)u)->worldMaterialBuffer && n >= 4u * sizeof( float ) )
		memcpy( ((fakeHost_t *)u)->worldMaterialFirst, p, 4u * sizeof( float ) );
	((fakeHost_t *)u)->bufferWrites++; return qtrue;
}
static qboolean WriteTexture( void *u, uintptr_t q, uintptr_t t, const void *b,
		uint64_t n, uint32_t r, uint32_t i, uint32_t mip, uint32_t w,
		uint32_t h, uint32_t d ) {
	if ( q != 0x4000u || !t || !b || !n || !r || !i
			|| ( r & 255u ) || n != (uint64_t)r * i || !w || h != i || d != 1u )
		return qfalse;
	(void)mip;
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
	if ( d != 0x3000u ) return qfalse;
	((fakeHost_t *)u)->frameUiDraws = ((fakeHost_t *)u)->frameEntityDraws = 0u;
	*o = NewIdentity( u ); return qtrue;
}
static qboolean BeginPass( void *u, uintptr_t e, const ralWebGpuPassDesc_t *d, uintptr_t *o ) {
	if ( !e || !d || d->kind != RAL_WEBGPU_PASS_RENDER || !d->targetIdentity ) return qfalse;
	((fakeHost_t *)u)->passes++;
	((fakeHost_t *)u)->passHasDepth = d->depthEnabled;
	((fakeHost_t *)u)->targetLayer = d->targetArrayLayer;
	if ( !d->depthEnabled ) {
		if ( ( d->colorLoadOp != RAL_WEBGPU_LOAD_PRESERVE
				&& d->colorLoadOp != RAL_WEBGPU_LOAD_CLEAR ) || d->depthLoadOp || d->depthStore ) return qfalse;
		((fakeHost_t *)u)->uiPasses++;
	}
	if ( d->colorLoadOp == RAL_WEBGPU_LOAD_PRESERVE
			&& d->depthEnabled && d->depthLoadOp == RAL_WEBGPU_LOAD_CLEAR )
		((fakeHost_t *)u)->preserveColorDepthClears++;
	*o = NewIdentity( u ); return qtrue;
}
static qboolean RecordDraw( void *u, uintptr_t p, const ralWebGpuIndexedDraw_t *d ) {
	fakeHost_t *host = u;
	int effectPipeline = -1;
	if(d && d->pipelineIdentity==host->skyCapturePipeline) {
		if(host->passHasDepth||d->kind!=RAL_WEBGPU_DRAW_WORLD||d->indexCount!=6
			||d->firstIndex!=host->targetLayer*6||host->targetLayer>=6||d->bindGroupCount!=3
			||!d->bindGroupIdentities[0]||!d->bindGroupIdentities[1]||!d->bindGroupIdentities[2]
			||host->failDraw) return qfalse;
		host->skyCaptureFaces|=1u<<host->targetLayer; ++host->skyCaptureDraws; return qtrue;
	}
	if ( d && ((d->kind == RAL_WEBGPU_DRAW_UI) == (host->passHasDepth == qtrue)) ) return qfalse;
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
	if ( d->kind == RAL_WEBGPU_DRAW_EFFECT )
		for ( uint32_t i = 0u; i < RAL_WEBGPU_EFFECT_PIPELINE_COUNT; ++i )
			if ( d->pipelineIdentity == host->effectPipelines[i] )
				effectPipeline = (int)i;
	if ( ( ( d->kind == RAL_WEBGPU_DRAW_WORLD
				|| d->kind == RAL_WEBGPU_DRAW_ENTITY )
			&& d->pipelineIdentity != host->worldPipeline
			&& !(d->kind==RAL_WEBGPU_DRAW_WORLD && d->pipelineIdentity==host->skyPipeline) )
			|| ( d->kind == RAL_WEBGPU_DRAW_EFFECT
				&& ( effectPipeline < 0
					|| ( effectPipeline != RAL_WEBGPU_EFFECT_PIPELINE_LEGACY
						&& d->textured != qtrue ) ) )
			|| ( d->kind == RAL_WEBGPU_DRAW_UI
				&& d->pipelineIdentity != ( host->frameUiDraws % 2u == 0u
					? host->uiPipeline : host->msdfPipeline ) ) ) return qfalse;
	if ( d->kind == RAL_WEBGPU_DRAW_EFFECT ) {
		if (d->firstInstance && d->firstInstance <= RENDER_MATERIAL_MAX_STAGES)
			host->beamTextures[d->firstInstance - 1u] = d->textureIdentity;
		if ( d->textureBindGroupSlot
				!= ( effectPipeline == RAL_WEBGPU_EFFECT_PIPELINE_LEGACY
					? 0u : 2u ) ) return qfalse;
		host->effectDraws++;
		host->effectPipelineDraws[effectPipeline]++;
	}
	if ( d->kind == RAL_WEBGPU_DRAW_ENTITY ) {
		if(d->pipelineIdentity==host->skyPipeline) return qfalse;
		if ( d->bindGroupCount != 2u || d->bindGroupIdentities[1] != 0x9004u
				|| d->firstInstance != host->frameEntityDraws ) return qfalse;
		host->entityLightingDraws++; host->frameEntityDraws++;
	}
	if ( d->kind == RAL_WEBGPU_DRAW_UI ) {
		if ( d->textureBindGroupSlot != 1u || !host->uiPasses ) return qfalse;
		host->uiDraws++; host->frameUiDraws++;
	}
	if(d->pipelineIdentity==host->skyPipeline) {
		if(d->bindGroupCount!=3||d->textureBindGroupSlot||!d->bindGroupIdentities[0]
			||!d->bindGroupIdentities[1]||!d->bindGroupIdentities[2]) return qfalse;
		++host->skyDraws;
	}
	host->draws++; return qtrue;
}
static qboolean RecordDispatch( void *u, uintptr_t p,
		const ralWebGpuComputeDispatch_t *d ) {
	(void)u; (void)p; (void)d; return qfalse;
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
static qboolean PollSubmission( void *u, uintptr_t s, uint64_t g,
		ralWebGpuAsyncStatus_t *o, ralWebGpuGpuTimingState_t *t, uint64_t *n ) {
	fakeHost_t *host = u;
	if ( s != host->submissionIdentity || g != host->submissionGeneration ) return qfalse;
	*o = RAL_WEBGPU_ASYNC_READY; *t = RAL_WEBGPU_GPU_TIMING_UNAVAILABLE;
	*n = 0u; return qtrue;
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

static qboolean SkyDispatch(void *u,ralWebGpuBrowserOpcode_t op,const void *request,
        uint32_t requestBytes,void *response,uint32_t responseBytes) {
    fakeHost_t *host=u;
    if(!request||requestBytes<sizeof(ralWebGpuBrowserAbiHeader_t)
        ||!response||responseBytes<sizeof(ralWebGpuBrowserAbiHeader_t)) return qfalse;
    memset(response,0,responseBytes);
    ralWebGpuBrowserAbiHeader_t *h=response;
    h->schemaVersion=RAL_WEBGPU_BROWSER_ABI_SCHEMA_VERSION; h->opcode=op; h->byteCount=responseBytes;
    if(op==RAL_WEBGPU_BROWSER_OP_POLL_DEVICE) {
        if(responseBytes!=sizeof(ralWebGpuBrowserDeviceResponse_t)) return qfalse;
        ralWebGpuBrowserDeviceResponse_t *r=response;
        r->status=RAL_WEBGPU_REQUEST_READY; r->deviceIdentity=0x3000; r->queueIdentity=0x4000;
        return qtrue;
    }
    if(op==RAL_WEBGPU_BROWSER_OP_RELEASE_BIND_GROUP||op==RAL_WEBGPU_BROWSER_OP_DESTROY_PIPELINE_OBJECT) {
        if(responseBytes!=sizeof(ralWebGpuBrowserStatusResponse_t)) return qfalse;
        ((ralWebGpuBrowserStatusResponse_t *)response)->accepted=1;
        if(op==RAL_WEBGPU_BROWSER_OP_RELEASE_BIND_GROUP) ++host->skyGroupsDestroyed;
        return qtrue;
    }
    if(op!=RAL_WEBGPU_BROWSER_OP_CREATE_SHADER_MODULE&&op!=RAL_WEBGPU_BROWSER_OP_CREATE_BIND_GROUP_LAYOUT
        &&op!=RAL_WEBGPU_BROWSER_OP_CREATE_PIPELINE_LAYOUT&&op!=RAL_WEBGPU_BROWSER_OP_CREATE_PIPELINE
        &&op!=RAL_WEBGPU_BROWSER_OP_CREATE_BIND_GROUP) return qfalse;
    if(responseBytes!=sizeof(ralWebGpuBrowserIdentityResponse_t)) return qfalse;
    ralWebGpuBrowserIdentityResponse_t *r=response; r->identity=NewIdentity(host); r->accepted=1;
    if(op==RAL_WEBGPU_BROWSER_OP_CREATE_PIPELINE) {
        const ralWebGpuBrowserPipelineRequest_t *p=request;
        if(requestBytes!=sizeof(*p)||p->vertexAttributeCount!=5||p->depthWriteEnable
            ||p->colorFormats[0]!=RAL_FORMAT_R16G16B16A16_SFLOAT) return qfalse;
        if(p->depthTestEnable) {
            if(p->depthFormat!=RAL_FORMAT_D32_SFLOAT) return qfalse;
            host->skyPipeline=r->identity;
        } else {
            if(p->depthFormat!=RAL_FORMAT_UNDEFINED) return qfalse;
            host->skyCapturePipeline=r->identity;
        }
    }
    if(op==RAL_WEBGPU_BROWSER_OP_CREATE_BIND_GROUP) {
        const ralWebGpuBrowserBindGroupRequest_t *p=request;
        if(requestBytes!=sizeof(*p)) return qfalse;
        const ralWebGpuBrowserBindResourceAbi_t *entries=(const void *)(uintptr_t)p->entriesOffset;
        if(p->entryCount==20) {
            for(unsigned i=0;i<20;++i) if(entries[i].binding!=i||!entries[i].resourceIdentity
                ||entries[i].kind!=((i&1)?RAL_WEBGPU_BROWSER_BIND_RESOURCE_SAMPLER:RAL_WEBGPU_BROWSER_BIND_RESOURCE_TEXTURE)) return qfalse;
            ++host->skyTextureGroups;
        } else if(p->entryCount!=1) return qfalse;
        ++host->skyGroupsCreated;
    }
    return qtrue;
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
	info.command.host.recordComputeDispatch = RecordDispatch;
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
	ralWebGpuPipeline_t *effectPipelines[RAL_WEBGPU_EFFECT_PIPELINE_COUNT] = { 0 };
	ralWebGpuPipeline_t *uiPipeline = NULL;
	ralWebGpuPipeline_t *msdfPipeline = NULL;
	ralWebGpuPipelineReceipt_t uiPipelineReceipt;
	ralWebGpuPipelineReceipt_t msdfPipelineReceipt;
	ralWebGpuPipelineReceipt_t effectPipelineReceipts[
		RAL_WEBGPU_EFFECT_PIPELINE_COUNT];
	uintptr_t effectBindGroups[RAL_WEBGPU_EFFECT_PIPELINE_COUNT];
	ralWebGpuProduct_t *product = NULL;
	uintptr_t entityLightingIdentity, worldMaterialIdentity;
	uint64_t entityLightingBytes, worldMaterialBytes;
	ralWebGpuProductFrameReceipt_t productReceipt, productExact, productOutput;
	ralSkyEnvironmentPlanRequest_t distantSkyRequest;
	ralSkyEnvironmentPlanReceipt_t distantSkyReceipt;
	renderSubmissionState_t frontend;
	renderSubmissionReceipt_t submission, staleSubmission;
	ralWebGpuFrontendPlanReceipt_t plan, exact, before;
	mapFile_t map; dsurface_t surfaces[2]; drawVert_t vertices[12];
	int indices[3] = { 0, 1, 2 }; dshader_t shader; refdef_t view;
	refEntity_t modelEntity, spriteEntity, beamEntity; refEntityMotion_t motion;
	spriteDesc_t effectSprite;
	ribbonDesc_t effectRibbon;
	ribbonPoint_t effectRibbonPoints[2];
	beamDesc_t effectBeam;
	renderEffectBeamPool_t effectBeamPool;
	renderEffectBeamPoolReceipt_t effectBeamPoolReceipt;
	decalDesc_t effectDecal;
	qhandle_t material, lightmap, msdf, model; char lightmapName[MAX_QPATH];
	renderMaterialLighting_t materialLighting;
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
	for ( uint32_t i = 0u; i < RAL_WEBGPU_EFFECT_PIPELINE_COUNT; ++i ) {
		pipelineInfo.generation = 42u + i;
		CHECK( RalWebGpu_RuntimeCreatePipeline( runtime, &runtimeReceipt,
			&pipelineInfo, &effectPipelines[i], &effectPipelineReceipts[i] ) );
		CHECK( RalWebGpu_RuntimeGetReceipt( runtime, &runtimeReceipt ) );
		host.effectPipelines[i] = effectPipelineReceipts[i].pipelineIdentity;
	}
	pipelineInfo.generation = 47u;
	CHECK( RalWebGpu_RuntimeCreatePipeline( runtime, &runtimeReceipt,
		&pipelineInfo, &uiPipeline, &uiPipelineReceipt ) );
	CHECK( RalWebGpu_RuntimeGetReceipt( runtime, &runtimeReceipt ) );
	pipelineInfo.generation = 48u;
	CHECK( RalWebGpu_RuntimeCreatePipeline( runtime, &runtimeReceipt,
		&pipelineInfo, &msdfPipeline, &msdfPipelineReceipt ) );
	CHECK( RalWebGpu_RuntimeGetReceipt( runtime, &runtimeReceipt ) );
	host.worldPipeline = pipelineReceipt.pipelineIdentity;
	host.uiPipeline = uiPipelineReceipt.pipelineIdentity;
	host.msdfPipeline = msdfPipelineReceipt.pipelineIdentity;
	CHECK( RalWebGpu_ProductCreate( runtime, &runtimeReceipt, &pipelineReceipt,
		42u, &product ) );
	ralWebGpuBrowserBridge_t *skyBridge=NULL;
	ralWebGpuBrowserBridgeCreateInfo_t skyBridgeInfo={0};
	skyBridgeInfo.schemaVersion=RAL_WEBGPU_BROWSER_ABI_SCHEMA_VERSION;
	skyBridgeInfo.generation=7; skyBridgeInfo.canvasIdentity=0x7000;
	skyBridgeInfo.userData=&host; skyBridgeInfo.dispatch=SkyDispatch;
	CHECK(RalWebGpu_BrowserBridgeCreate(&skyBridgeInfo,&skyBridge));
	ralWebGpuCoreCreateInfo_t skyCore;
	ralWebGpuDevicePoll_t skyDevice;
	CHECK(RalWebGpu_BrowserBridgeBuildCoreInfo(skyBridge,&skyCore));
	CHECK(skyCore.host.pollDevice(skyCore.userData,8,&skyDevice));
	CHECK(RalWebGpu_ProductEnableSkyMaterial(product,&runtimeReceipt,skyBridge,100));
	CHECK(RalWebGpu_ProductSetPhysicalSky(product,0xa001,0xa002,0));
	{
		uint64_t bytes;
		CHECK( RalWebGpu_ProductGetEffectParameterBuffer( product, &runtimeReceipt,
			&host.effectParameterBuffer, &bytes ) );
		CHECK( host.effectParameterBuffer && bytes == RAL_WEBGPU_EFFECT_PARAMETER_BYTES );
	}
	CHECK( RalWebGpu_ProductGetWorldMaterialBuffer( product, &runtimeReceipt,
		&worldMaterialIdentity, &worldMaterialBytes )
		&& worldMaterialIdentity && worldMaterialBytes >= 4u * sizeof( float ) );
	host.worldMaterialBuffer = worldMaterialIdentity;
	CHECK( RalWebGpu_ProductSetWorldMaterialBindGroup( product, &runtimeReceipt,
		0x9005u ) );
	CHECK( RalWebGpu_ProductSetEntityPipeline( product, &runtimeReceipt,
		&pipelineReceipt ) );
	CHECK( RalWebGpu_ProductGetEntityLightingBuffer( product, &runtimeReceipt,
		&entityLightingIdentity, &entityLightingBytes )
		&& entityLightingIdentity && entityLightingBytes >= 16u * sizeof( float ) );
	host.entityLightingBuffer = entityLightingIdentity;
	CHECK( RalWebGpu_ProductSetEntityLightingBindGroup( product, &runtimeReceipt,
		0x9004u ) );
	for ( uint32_t i = 0u; i < RAL_WEBGPU_EFFECT_PIPELINE_COUNT; ++i ) {
		effectBindGroups[i] = 0x9003u + i;
	}
	CHECK( RalWebGpu_ProductSetEffectPipelines( product, &runtimeReceipt,
		effectPipelineReceipts ) );
	CHECK( RalWebGpu_ProductSetUiPipelines( product, &runtimeReceipt,
		&uiPipelineReceipt, &msdfPipelineReceipt ) );
	CHECK( RalWebGpu_ProductSetAtmosphereBindGroups( product, &runtimeReceipt,
		0x9001u, 0x9002u, effectBindGroups ) );
	CHECK( !RalWebGpu_ProductSetViewport( product, &runtimeReceipt, 0u, 720u ) );
	CHECK( RalWebGpu_ProductSetViewport( product, &runtimeReceipt, 1280u, 720u ) );
	CHECK( RenderSubmission_Init( &frontend, 71u ) );
	material = RenderSubmission_RegisterMaterialImage( &frontend, RENDER_ASSET_MATERIAL,
		"textures/webgpu", qtrue, rgba, 2u, 2u );
	msdf = RenderSubmission_RegisterMaterialImage( &frontend, RENDER_ASSET_MSDF,
		"fonts/webgpu", qtrue, rgba, 2u, 2u );
	CHECK( material > 0 && msdf > material );
	memset( &materialLighting, 0, sizeof( materialLighting ) );
	materialLighting.schemaVersion = RENDER_MATERIAL_LIGHTING_SCHEMA_VERSION;
	materialLighting.sourceGeneration = 1u;
	materialLighting.provenanceHash = 2u;
	materialLighting.diffuseReflectanceQ16[0] = RENDER_MATERIAL_LIGHTING_Q16_ONE;
	materialLighting.diffuseReflectanceQ16[1] = RENDER_MATERIAL_LIGHTING_Q16_ONE;
	materialLighting.diffuseReflectanceQ16[2] = RENDER_MATERIAL_LIGHTING_Q16_ONE;
	materialLighting.emissionRadianceQ16[0] = RENDER_MATERIAL_LIGHTING_Q16_ONE * 2;
	materialLighting.emissiveMobility = RAL_LIGHT_MOBILITY_STATIC;
	materialLighting.emissiveInfluenceRangeQ16 = RAL_LIGHT_Q16_ONE * 8;
	materialLighting.emissiveRequestedProxyCount = 0u;
	materialLighting.emissiveExplicitProxyAuthority = qtrue;
	materialLighting.ready = qtrue;
	CHECK( RenderSubmission_SetMaterialLighting( &frontend, material,
		&materialLighting ) );
	CHECK( RenderSubmission_SetMaterialSky( &frontend, material, qtrue ) );
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
	view.time = 75;
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
	{
		ralIrradianceEntitySampleReceipt_t local = LocalIrradiance( 75u, 1u );
		CHECK( RenderSubmission_AttachEntityIrradiance( &frontend, 0u, &local ) );
	}
	memset( &spriteEntity, 0, sizeof( spriteEntity ) ); spriteEntity.reType = RT_SPRITE;
	spriteEntity.radius = 1.0f; spriteEntity.customShader = material;
	memset( spriteEntity.shader.rgba, 255, 4u );
	CHECK( RenderSubmission_AddEntity( &frontend, &spriteEntity, NULL ) );
	memset( &beamEntity, 0, sizeof( beamEntity ) ); beamEntity.reType = RT_BEAM;
	beamEntity.radius = 0.5f; beamEntity.customShader = material;
	beamEntity.oldorigin[0] = 4.0f; memset( beamEntity.shader.rgba, 255, 4u );
	CHECK( RenderSubmission_AddEntity( &frontend, &beamEntity, NULL ) );
	{
		/* The local player's own body (mirror/portal only): neither planned nor drawn in the primary view. */
		refEntity_t ownBody = modelEntity;
		ownBody.renderfx |= RF_THIRD_PERSON;
		CHECK( RenderSubmission_AddEntity( &frontend, &ownBody, NULL ) );
	}
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
	memset( &effectSprite, 0, sizeof( effectSprite ) );
	effectSprite.origin[0] = 8.0f;
	effectSprite.radius = 2.0f;
	effectSprite.rgba[0] = effectSprite.rgba[1] = effectSprite.rgba[2]
		= effectSprite.rgba[3] = 1.0f;
	effectSprite.shader = material;
	effectSprite.flags = PRIM_FLAG_ADDITIVE | PRIM_FLAG_NO_DEPTH_TEST;
	CHECK( RenderSubmission_AddEffectSprite( &frontend, &effectSprite ) );
	memset( effectRibbonPoints, 0, sizeof( effectRibbonPoints ) );
	for ( uint32_t point = 0u; point < 2u; ++point ) {
		effectRibbonPoints[point].pos[0] = 8.0f;
		effectRibbonPoints[point].pos[1] = point ? 4.0f : -4.0f;
		effectRibbonPoints[point].width = 1.0f;
		effectRibbonPoints[point].rgba[0] = 1.0f;
		effectRibbonPoints[point].rgba[3] = 1.0f;
		effectRibbonPoints[point].normal[2] = 1.0f;
	}
	memset( &effectRibbon, 0, sizeof( effectRibbon ) );
	effectRibbon.points = effectRibbonPoints;
	effectRibbon.numPoints = 2;
	effectRibbon.shader = material;
	effectRibbon.flags = PRIM_FLAG_CUSTOM_NORMAL;
	CHECK( RenderSubmission_AddEffectRibbon( &frontend, &effectRibbon ) );
	memset( &effectBeam, 0, sizeof( effectBeam ) );
	effectBeam.start[0] = effectBeam.end[0] = 8.0f;
	effectBeam.start[1] = -2.0f; effectBeam.end[1] = 2.0f;
	effectBeam.startWidth = 0.5f; effectBeam.endWidth = 0.25f;
	effectBeam.startColor[0] = effectBeam.startColor[3] = 1.0f;
	effectBeam.endColor[0] = effectBeam.endColor[3] = 1.0f;
	effectBeam.shader = material; effectBeam.axialCopies = 1;
	effectBeam.flags = PRIM_FLAG_ADDITIVE;
	effectBeam.uvScroll[0] = 2.0f;
	effectBeam.uvScroll[1] = -4.0f;
	effectBeam.startEntityNum = effectBeam.endEntityNum = -1;
	CHECK( RenderSubmission_AddEffectBeam( &frontend, &effectBeam ) );
	CHECK( RenderSubmission_EndFrame( &frontend, 75u, &submission ) );
	RenderEffectBeamPool_Reset( &effectBeamPool );
	CHECK( RenderEffectBeamPool_Advance( &effectBeamPool, 76u, qtrue,
		0.075f, &effectBeam, 1u, &effectBeamPool,
		&effectBeamPoolReceipt ) );
	CHECK( RalWebGpu_FrontendPlanBuild( runtime, &runtimeReceipt, &frontend,
		&submission, 76u, NULL, NULL, &effectBeamPool,
		&effectBeamPoolReceipt, &plan ) );
	CHECK( plan.materialCount == 3u && plan.worldBatchCount == 2u
		&& plan.localIrradianceEntityCount == 1u
		&& plan.patchBatchCount == 1u && plan.lightmappedWorldBatchCount == 1u
		&& plan.modelEntityCount == 1u && plan.primitiveEntityCount == 2u
		&& plan.temporalEntityCount == 1u && plan.polygonCount == 1u
		&& plan.lightCount == 1u && plan.uiPrimitiveCount == 2u
		&& plan.effectSpriteCount == 1u && plan.effectRibbonCount == 1u
		&& plan.effectBeamCount == 1u
		&& plan.effectIndexCount == 18u && plan.effectBatchCount == 3u
		&& plan.texturedUiPrimitiveCount == 2u && plan.msdfUiPrimitiveCount == 1u
		&& plan.drawCount == 12u && !plan.unresolvedCount );
	exact = plan; CHECK( RalWebGpu_FrontendPlanReceiptExact( &plan, &exact ) );
	exact.loweringDigest++; CHECK( !RalWebGpu_FrontendPlanReceiptExact( &plan, &exact ) );
	memset( &plan, 0xa5, sizeof( plan ) ); before = plan;
	staleSubmission = submission; staleSubmission.frameDigest++;
	CHECK( !RalWebGpu_FrontendPlanBuild( runtime, &runtimeReceipt, &frontend,
		&staleSubmission, 77u, NULL, NULL, NULL, NULL, &plan )
		&& !memcmp( &plan, &before, sizeof( plan ) ) );
	staleRuntime = runtimeReceipt; staleRuntime.pipelineCount++;
	CHECK( !RalWebGpu_FrontendPlanBuild( runtime, &staleRuntime, &frontend,
		&submission, 77u, NULL, NULL, NULL, NULL, &plan )
		&& !memcmp( &plan, &before, sizeof( plan ) ) );
	const qboolean rendered=RalWebGpu_ProductRender( product, &runtimeReceipt, &frontend,
		&submission, 0x8000u, 78u, &productReceipt );
	if(!rendered) fprintf(stderr,"product failure stage %u\n",RalWebGpu_ProductLastFailureStage());
	CHECK(rendered);
	/* The beam immediately precedes the two UI quads. Inspect the real
	 * upload, including UV preservation across clip-space projection. */
	CHECK( host.lastVertexBytes / sizeof( renderWorldVertex_t ) == 27u );
	for ( unsigned corner = 0; corner < 4u; ++corner ) {
		const renderWorldVertex_t *v = &host.lastVertices[15u + corner];
		CHECK( v->normal[0] == ( corner >= 2u ? 1.0f : 0.0f ) );
		CHECK( v->normal[1] == ( corner & 1u ? 1.0f : 0.0f ) );
	}
	CHECK( host.effectParameterBytes == 2u * sizeof(ralWebGpuEffectDrawParams_t) );
	CHECK( host.effectParameters[0].meta[0] == 0u && host.effectParameters[1].meta[0] == 1u );
	CHECK( fabsf(host.effectParameters[1].scaleOffset[2] - 0.15f) < 0.00001f );
	CHECK( fabsf(host.effectParameters[1].scaleOffset[3] + 0.30f) < 0.00001f );
	CHECK( productReceipt.uploadedMaterialCount == 3u
		&& productReceipt.reusedMaterialCount == 0u
		&& productReceipt.worldDrawCount == 2u
		&& productReceipt.worldEmissiveDrawCount == 2u
		&& productReceipt.modelEntityCount == 1u
		&& productReceipt.primitiveEntityCount == 2u
		&& productReceipt.temporalEntityCount == 1u
		&& productReceipt.entityDrawCount == 3u
		&& productReceipt.localIrradianceEntityCount == 1u
		&& productReceipt.localIrradianceDrawCount == 1u
		&& productReceipt.polygonDrawCount == 1u
		&& productReceipt.lightDrawCount == 1u
		&& productReceipt.effectSpriteDrawCount == 1u
		&& productReceipt.effectRibbonDrawCount == 1u
		&& productReceipt.effectBeamDrawCount == 1u
		&& productReceipt.effectBeamPool.activeCount == 1u
		&& productReceipt.uiDrawCount == 2u
		&& productReceipt.weather.zeroWork == qtrue
		&& productReceipt.weatherDrawCount == 0u
		&& productReceipt.deferredNonWorldDrawCount == 0u
		// Initial raw sky vertices (1 write) + independent draw/stage blocks (4).
		&& host.textureWrites == 3u && host.bufferWrites == 14u
		&& host.skyDraws==2 && host.skyTextureGroups==2 && host.skyGroupsCreated==6
		&& host.worldMaterialFirst[0] == 2.0f
		&& host.worldMaterialFirst[1] == 0.0f
		&& host.worldMaterialFirst[3] == 1.0f
		&& host.entityLightingFirst[0] == 0.5f
		&& host.entityLightingFirst[1] == 1.0f
		&& host.entityLightingFirst[2] == 2.0f
		&& host.entityLightingFirst[3] == 1.0f
		&& host.entityLightingFirst[4] == 0.0f
		&& host.entityLightingFirst[8] == 0.0f
		&& host.entityLightingFirst[12] == 0.0f
		&& host.effectPipelineDraws[RAL_WEBGPU_EFFECT_PIPELINE_LEGACY] == 2u
		&& host.effectPipelineDraws[
			RAL_WEBGPU_EFFECT_PIPELINE_ADDITIVE_NO_DEPTH] == 1u
		&& host.effectPipelineDraws[
			RAL_WEBGPU_EFFECT_PIPELINE_ALPHA_DEPTH] == 1u
		&& host.effectPipelineDraws[
			RAL_WEBGPU_EFFECT_PIPELINE_ADDITIVE_DEPTH] == 1u
		&& host.draws == 12u );
	productExact = productReceipt;
	CHECK( RalWebGpu_ProductFrameReceiptExact( &productReceipt, &productExact ) );
	CHECK( RalWebGpu_ProductPoll( product, &productReceipt ) == RAL_WEBGPU_ASYNC_READY );
	CHECK( RalWebGpu_ProductRender( product, &runtimeReceipt, &frontend,
		&submission, 0x8001u, 79u, &productReceipt ) );
	CHECK( productReceipt.uploadedMaterialCount == 0u
		&& productReceipt.reusedMaterialCount == 3u
		// Raw vertices and unchanged texture groups are reused on the next frame.
		&& host.textureWrites == 3u && host.bufferWrites == 27u
		&& host.skyDraws==4 && host.skyTextureGroups==2 && host.skyGroupsCreated==6
		&& host.draws == 24u );
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
	memset( &effectDecal, 0, sizeof( effectDecal ) );
	effectDecal.origin[0] = 8.0f; effectDecal.normal[2] = 1.0f;
	effectDecal.radius = 1.0f; effectDecal.rgba[0] = 1.0f;
	effectDecal.rgba[3] = 1.0f; effectDecal.shader = material;
	effectDecal.lifetime = 1.0f;
	CHECK( RenderSubmission_AddEffectDecal( &frontend, &effectDecal ) );
	CHECK( RenderSubmission_EndFrame( &frontend, 81u, &submission ) );
	CHECK( RalWebGpu_ProductRender( product, &runtimeReceipt, &frontend,
		&submission, 0x8002u, 81u, &productReceipt ) );
	CHECK( productReceipt.uploadedMaterialCount == 1u
		&& productReceipt.reusedMaterialCount == 2u
		&& productReceipt.localIrradianceEntityCount == 0u
		&& productReceipt.localIrradianceDrawCount == 0u
		&& productReceipt.effectDecalDrawCount == 1u
		&& productReceipt.effectDecalPool.activeCount == 1u
		&& host.textureWrites == 4u && host.bufferWrites == 40u
		&& host.draws == 34u );
	CHECK( RalWebGpu_ProductPoll( product, &productReceipt ) == RAL_WEBGPU_ASYNC_READY );
	{
		uint64_t distantIdentity = 0u;
		uint32_t drawsBefore = host.draws;
		uint32_t passesBefore = host.passes;
		uint32_t resetsBefore = host.preserveColorDepthClears;
		// Distinct semantic material: distant geometry must not mutate the main
		// world's sky declaration after its batches have been classified.
		CHECK(RenderSubmission_RegisterMaterialImage(&frontend,RENDER_ASSET_MATERIAL,
			"textures/webgpu-distant",qtrue,rgba,2u,2u)>0);
		strcpy(shader.shader,"textures/webgpu-distant");
		CHECK( RenderSubmission_LoadWorld( &frontend, &map,
			RENDER_DISTANT_WORLD_BASE ) );
		CHECK( RenderSubmission_WorldContentIdentity( &frontend,
			RENDER_DISTANT_WORLD_BASE, &distantIdentity ) );
		CHECK( RenderSubmission_SelectWorld( &frontend, 0 ) );
		CHECK( RenderSubmission_BeginFrame( &frontend, 82u ) );
		CHECK( RenderSubmission_RenderScene( &frontend, &view, 0 ) );
		CHECK( RenderSubmission_EndFrame( &frontend, 82u, &submission ) );
		CHECK( RalWebGpu_ProductPlan( product, &runtimeReceipt, &frontend,
			&submission, &plan ) );
		memset( &distantSkyRequest, 0, sizeof( distantSkyRequest ) );
		distantSkyRequest.schemaVersion = RAL_SKY_ENVIRONMENT_SCHEMA_VERSION;
		distantSkyRequest.backendType = RAL_BACKEND_WEBGPU;
		distantSkyRequest.frameGeneration = 82u;
		distantSkyRequest.requestedTier = RAL_ATMOSPHERE_TIER_FULL;
		distantSkyRequest.mode = RAL_SKY_ENVIRONMENT_AUTHORED;
		distantSkyRequest.sourceRevision = 1u;
		distantSkyRequest.authoredSkyAvailable = qtrue;
		distantSkyRequest.distantSceneRequested = qtrue;
		distantSkyRequest.distantSceneIdentity = distantIdentity;
		distantSkyRequest.distantSceneOrigin[0] = -128.0f;
		distantSkyRequest.distantSceneScale = 16.0f;
		distantSkyRequest.distantSceneLodBias = 1.0f;
		distantSkyRequest.capabilities.distantScene = qtrue;
		CHECK( Ral_SkyEnvironmentPlan( &distantSkyRequest,
			&distantSkyReceipt ) );
		qboolean distantRendered = RalWebGpu_ProductRenderPlan( product, &runtimeReceipt,
			&frontend, &submission, &plan, &distantSkyReceipt,
			0x8005u, 82u, &productReceipt );
		if ( !distantRendered ) fprintf( stderr,
			"WebGPU distant product failure stage=%u\n",
			RalWebGpu_ProductLastFailureStage() );
		CHECK( distantRendered );
		CHECK( productReceipt.worldDrawCount == 2u
			&& productReceipt.distantWorldCandidateCount == 2u
			&& productReceipt.distantWorldDrawCount == 2u
			&& productReceipt.distantWorldIndexCount > 0u
			&& productReceipt.distantWorldDepthReset == qtrue
			&& productReceipt.command.passCount == 3u
			&& host.draws == drawsBefore + 5u
			&& host.passes == passesBefore + 3u
			&& host.preserveColorDepthClears == resetsBefore + 2u );
		CHECK( RalWebGpu_ProductPoll( product, &productReceipt )
			== RAL_WEBGPU_ASYNC_READY );
	}
	{
		uint32_t destroyedBeforeWorldUnload;
		CHECK( RenderSubmission_LoadWorld( &frontend, &map, 1 ) );
		CHECK( RenderSubmission_BeginFrame( &frontend, 83u ) );
		CHECK( RenderSubmission_RenderScene( &frontend, &view, 1 ) );
		CHECK( RenderSubmission_EndFrame( &frontend, 83u, &submission ) );
		CHECK( RalWebGpu_ProductRender( product, &runtimeReceipt, &frontend,
			&submission, 0x8004u, 83u, &productReceipt ) );
		CHECK( !RalWebGpu_ProductUnloadWorld( product, &runtimeReceipt, 0 ) );
		CHECK( !RalWebGpu_ProductResetRegistrationAssets( product, &runtimeReceipt, &frontend ) );
		CHECK( RalWebGpu_ProductPoll( product, &productReceipt )
			== RAL_WEBGPU_ASYNC_READY );
		for ( unsigned gap = 0u; gap < 4u; ++gap ) {
			const uint64_t frame = 84u + gap;
			view.rdflags = (gap & 1u) ? 0 : RDF_NOWORLDMODEL | RDF_HYPERSPACE | RDF_CAMERA_CUT;
			view.time += 50;
			CHECK( RenderSubmission_BeginFrame( &frontend, frame ) );
			CHECK( RenderSubmission_RenderScene( &frontend, &view, 1 ) );
			CHECK( RenderSubmission_AddEntity( &frontend, &spriteEntity, NULL ) );
			CHECK( RenderSubmission_AddUiQuad( &frontend, 0, 0, 1280, 720,
				0, 0, 1, 1, 0, material ) );
			CHECK( RenderSubmission_EndFrame( &frontend, frame, &submission ) );
			const unsigned created = host.resourcesCreated, destroyed = host.resourcesDestroyed;
			productExact = productReceipt;
			host.failDraw = gap == 2u;
			qboolean rendered = RalWebGpu_ProductRender( product, &runtimeReceipt, &frontend,
				&submission, 0x8004u, frame, &productReceipt );
			host.failDraw = qfalse;
			if ( gap == 2u ) {
				CHECK( !rendered && !memcmp( &productReceipt, &productExact, sizeof(productReceipt) ) );
				CHECK( host.resourcesCreated > created
					&& host.resourcesCreated - created == host.resourcesDestroyed - destroyed );
				continue;
			}
			if ( !rendered ) fprintf( stderr, "no-world frame=%llu frontend=%u product=%u\n",
				(unsigned long long)frame, RalWebGpu_FrontendPlanLastFailureStage(),
				RalWebGpu_ProductLastFailureStage() );
			CHECK( rendered );
			CHECK( productReceipt.entityDrawCount && productReceipt.uiDrawCount == 1u );
			if ( !gap ) {
				CHECK( !productReceipt.worldDrawCount && !productReceipt.plan.worldVertexCount
					&& !productReceipt.plan.worldIndexCount && !productReceipt.exposureDispatchCount );
				CHECK( !productReceipt.worldVertexBuffer.resourceIdentity
					&& !productReceipt.worldIndexBuffer.resourceIdentity
					&& !productReceipt.worldMaterialBuffer.resourceIdentity );
				productExact = productReceipt;
				productExact.worldVertexBuffer.resourceIdentity = 0x1234u;
				CHECK( !RalWebGpu_ProductFrameReceiptExact( &productExact, &productExact ) );
			} else CHECK( productReceipt.worldDrawCount && productReceipt.plan.worldVertexCount );
			CHECK( RalWebGpu_ProductPoll( product, &productReceipt ) == RAL_WEBGPU_ASYNC_READY );
		}
		/* Real product uploads: transient resubmit, persistent age, pause,
		 * rewind, and straight descriptor RGB in both blend modes. */
		for ( unsigned mode = 0u; mode < 2u; ++mode ) {
			const struct { int time; qboolean submit; float duration, phase; } cases[] = {
				{2000, qtrue, 0.0f, 2.0f}, {2500, qtrue, 0.0f, 2.5f},
				{3000, qtrue, 2.0f, 0.0f}, {3500, qfalse, 2.0f, 0.5f},
				{3500, qfalse, 2.0f, 0.5f}, {1000, qfalse, 0.0f, -1.0f},
				{1100, qtrue, 0.0f, 1.1f}
			};
			const float colors[2][4] = {{0.812345f, 0.412345f, 0.212345f, 0.251234f}, {0.212345f, 0.612345f, 1.0f, 0.512345f}};
			memcpy( effectBeam.startColor, colors[0], sizeof(colors[0]) );
			memcpy( effectBeam.endColor, colors[1], sizeof(colors[1]) );
			effectBeam.flags = mode ? 0 : PRIM_FLAG_ADDITIVE;
			view.rdflags = 0;
			for ( unsigned step = 0u; step < sizeof(cases)/sizeof(cases[0]); ++step ) {
				const uint64_t frame = 90u + mode * 16u + step;
				view.time = cases[step].time;
				effectBeam.duration = cases[step].duration;
				CHECK( RenderSubmission_BeginFrame( &frontend, frame ) );
				CHECK( RenderSubmission_RenderScene( &frontend, &view, 1 ) );
				if ( cases[step].submit ) CHECK( RenderSubmission_AddEffectBeam( &frontend, &effectBeam ) );
				CHECK( RenderSubmission_EndFrame( &frontend, frame, &submission ) );
				host.lastVertexBytes = 0u;
				CHECK( RalWebGpu_ProductRender( product, &runtimeReceipt, &frontend,
					&submission, 0x8004u, frame, &productReceipt ) );
				CHECK( RalWebGpu_ProductPoll( product, &productReceipt ) == RAL_WEBGPU_ASYNC_READY );
				if ( cases[step].phase < 0.0f ) {
					CHECK( !productReceipt.effectBeamDrawCount );
					CHECK( productReceipt.effectParameterCount == 1u );
					continue;
				}
				CHECK( productReceipt.effectBeamDrawCount == 1u );
				CHECK( productReceipt.effectBeamStageDrawCount == 1u && productReceipt.effectParameterCount == 2u );
				const ralWebGpuEffectDrawParams_t *params = &host.effectParameters[1];
				CHECK( params->scaleOffset[0] == 1.0f && params->scaleOffset[1] == 1.0f );
				CHECK( fabsf(params->scaleOffset[2] - cases[step].phase * 2.0f) < 0.00001f );
				CHECK( fabsf(params->scaleOffset[3] + cases[step].phase * 4.0f) < 0.00001f );
				CHECK( !memcmp(params->startColor, colors[0], 3u * sizeof(float)) );
				CHECK( !memcmp(params->endColor, colors[1], 3u * sizeof(float)) );
				if (cases[step].submit) {
					CHECK( params->startColor[3] == colors[0][3] && params->endColor[3] == colors[1][3] );
				} else CHECK( params->startColor[3] > 0.0f && params->startColor[3] <= colors[0][3] );
				CHECK( host.lastVertexBytes == 4u * sizeof(renderWorldVertex_t) );
				for ( unsigned corner = 0u; corner < 4u; ++corner ) {
					const renderWorldVertex_t *v = &host.lastVertices[corner];
					CHECK( v->normal[0] == (corner >= 2u ? 1.0f : 0.0f) );
					CHECK( v->normal[1] == (corner & 1u ? 1.0f : 0.0f) );
				}
			}
		}
		destroyedBeforeWorldUnload = host.resourcesDestroyed;
		/* Two authored images must use distinct resident resources and GPU
		 * UV parameters without duplicating the logical beam or its vertices. */
		{
			renderMaterialStageSnapshot_t stages[2] = {{0}};
			const byte pixels[2][4] = {{64,0,0,255},{0,128,0,128}};
			const qhandle_t staged = RenderSubmission_RegisterMaterialImage(&frontend,
				RENDER_ASSET_MATERIAL, "*staged-beam", qfalse, pixels[0], 1u, 1u);
			CHECK(staged > 0);
			for (unsigned i = 0u; i < 2u; ++i) {
				stages[i].material = RenderSubmission_RegisterStageTexture(&frontend,
					RENDER_ASSET_MATERIAL, i ? "*beam-green" : "*beam-red", qfalse, pixels[i], 1u, 1u);
				CHECK(stages[i].material > 0);
				stages[i].scaleScroll[0] = 2.0f + i;
				stages[i].scaleScroll[1] = 3.0f + i;
				stages[i].scaleScroll[2] = .25f + i;
				stages[i].scaleScroll[3] = -.5f - i;
			}
			CHECK(RenderSubmission_SetMaterialStages(&frontend, staged, stages, 2u));
			effectBeam.shader = staged; effectBeam.flags = 0; effectBeam.duration = 0;
			view.time = 4000;
			CHECK(RenderSubmission_BeginFrame(&frontend, 130u));
			CHECK(RenderSubmission_RenderScene(&frontend, &view, 1));
			CHECK(RenderSubmission_AddEffectBeam(&frontend, &effectBeam));
			CHECK(RenderSubmission_EndFrame(&frontend, 130u, &submission));
			const uint32_t liveBefore = host.resourcesCreated - host.resourcesDestroyed;
			memset(&productOutput, 0xa5, sizeof(productOutput)); productExact = productOutput;
			host.failEffectParameterWrite = qtrue;
			CHECK(!RalWebGpu_ProductRender(product, &runtimeReceipt, &frontend, &submission, 0x8004u, 130u, &productOutput));
			CHECK(!memcmp(&productOutput, &productExact, sizeof(productOutput)));
			CHECK(host.resourcesCreated - host.resourcesDestroyed == liveBefore);
			host.failEffectParameterWrite = qfalse;
			CHECK(RalWebGpu_ProductRender(product, &runtimeReceipt, &frontend, &submission, 0x8004u, 130u, &productReceipt));
			CHECK(RalWebGpu_ProductPoll(product, &productReceipt) == RAL_WEBGPU_ASYNC_READY);
			CHECK(productReceipt.effectBeamDrawCount == 1u && productReceipt.effectBeamStageDrawCount == 2u);
			CHECK(productReceipt.effectParameterCount == 3u && host.effectParameterBytes == 3u * sizeof(ralWebGpuEffectDrawParams_t));
			CHECK(host.lastVertexBytes == 4u * sizeof(renderWorldVertex_t));
			CHECK(host.beamTextures[0] && host.beamTextures[1] && host.beamTextures[0] != host.beamTextures[1]);
			for (unsigned i = 0u; i < 2u; ++i) {
				const float *uv = host.effectParameters[i+1u].scaleOffset;
				CHECK(uv[0] == 2.0f+i && uv[1] == 3.0f+i);
				CHECK(uv[2] == (2.25f+i)*4.0f && uv[3] == (-4.5f-i)*4.0f);
			}
			productExact = productReceipt; productExact.effectParameterWrite.byteOffset = 4u;
			CHECK(!RalWebGpu_ProductFrameReceiptExact(&productExact, &productExact));
			productExact = productReceipt; productExact.effectParameterWrite.byteSize -= 64u;
			CHECK(!RalWebGpu_ProductFrameReceiptExact(&productExact, &productExact));
			productExact = productReceipt; productExact.effectParameterWrite.resourceGeneration++;
			CHECK(!RalWebGpu_ProductFrameReceiptExact(&productExact, &productExact));
		}
		destroyedBeforeWorldUnload = host.resourcesDestroyed;
		CHECK( RalWebGpu_ProductUnloadWorld( product, &runtimeReceipt, 0 ) );
		// Projected geometry (2), cached raw sky vertices (1), two draw packets (4).
		CHECK( host.resourcesDestroyed == destroyedBeforeWorldUnload + 7u );
		CHECK( RenderSubmission_UnloadWorld( &frontend, 0 ) );
		CHECK( !RalWebGpu_ProductResetRegistrationAssets( product, &runtimeReceipt, &frontend ) );
		CHECK( RalWebGpu_ProductUnloadWorld( product, &runtimeReceipt, 1 ) );
		CHECK( host.resourcesDestroyed == destroyedBeforeWorldUnload + 9u );
		CHECK( RenderSubmission_UnloadWorld( &frontend, 1 ) );
	}
	memset( &productOutput, 0xa5, sizeof( productOutput ) ); productExact = productOutput;
	host.failDraw = qtrue;
	CHECK( !RalWebGpu_ProductRender( product, &runtimeReceipt, &frontend,
		&submission, 0x8003u, 82u, &productOutput )
		&& !memcmp( &productOutput, &productExact, sizeof( productOutput ) ) );
	host.failDraw = qfalse;
	/* A distant-sky world is also a resident borrower, not just slots 0/1. */
	CHECK( RenderSubmission_ResidentWorldCount( &frontend ) == 1 );
	CHECK( !RalWebGpu_ProductResetRegistrationAssets( product, &runtimeReceipt, &frontend ) );
	for ( int i = 0; i < MAX_RENDER_WORLDS; ++i ) {
		CHECK( RalWebGpu_ProductUnloadWorld( product, &runtimeReceipt, i ) );
		CHECK( RenderSubmission_UnloadWorld( &frontend, i ) );
	}
	CHECK( RalWebGpu_ProductResetRegistrationAssets( product, &runtimeReceipt, &frontend ) );
	{
		// Independent capture submit: all six faces, cached resources, busy
		// unload/registration rejection and cancellation before releasing lease.
		const byte pixel[4]={255,0,0,255};
		const qhandle_t handle=RenderSubmission_RegisterMaterialImage(&frontend,
			RENDER_ASSET_MATERIAL,"*capture",qtrue,pixel,1,1);
		renderMaterialSnapshot_t captureMaterial;
		ralWebGpuSkyRadianceResources_t skyResources;
		CHECK(RenderSubmission_SetMaterialSky(&frontend,handle,qtrue));
		CHECK(handle && RenderSubmission_MaterialSnapshot(&frontend,handle,&captureMaterial));
		// Generation 4 and face 3 used to XOR to a zero draw digest when the
		// generation itself was incorrectly used as the hash seed.
		CHECK(RalWebGpu_ProductEnsureSkyRadianceResources(product,&runtimeReceipt,4));
		CHECK(RalWebGpu_ProductPrepareMaterialResources(product,&runtimeReceipt,&frontend));
		CHECK(RalWebGpu_ProductSkyRadianceResources(product,&runtimeReceipt,&frontend,&skyResources));
		ralWebGpuCommand_t *captureCommand=RalWebGpu_RuntimeCommand(runtime,&runtimeReceipt);
		uint32_t created=0,groups=0;
		for(unsigned cycle=0;cycle<2;++cycle) {
			CHECK(RalWebGpu_ProductSkyCaptureBegin(product,&runtimeReceipt,&frontend,&captureMaterial,
				500,63,0x9005,0x9006,0));
			ralWebGpuSkyRadianceResources_t borrowed;
			CHECK(RalWebGpu_ProductSkyRadianceResources(product,&runtimeReceipt,&frontend,&borrowed));
			CHECK(!memcmp(&borrowed,&skyResources,sizeof(borrowed)));
			CHECK(!RalWebGpu_ProductPrepareMaterialResources(product,&runtimeReceipt,&frontend));
			CHECK(!RalWebGpu_ProductUnloadSkyRadianceResources(product,&runtimeReceipt));
			CHECK(!RalWebGpu_ProductUnloadWorld(product,&runtimeReceipt,0));
			CHECK(!RalWebGpu_ProductResetRegistrationAssets(product,&runtimeReceipt,&frontend));
			ralWebGpuCommandReceipt_t rec,updated,executable;
			ralWebGpuSubmissionReceipt_t submit;
			ralWebGpuPassDesc_t pass={0}; pass.kind=RAL_WEBGPU_PASS_RENDER;
			pass.colorLoadOp=RAL_WEBGPU_LOAD_CLEAR;
			pass.targetIdentity=skyResources.sourceTextureIdentity;
			CHECK(RalWebGpu_CommandBeginWithDesc(captureCommand,&pass,&rec));
			for(unsigned face=0;face<6;++face) {
				pass.targetArrayLayer=face;
				if(face) { CHECK(RalWebGpu_CommandNextPass(captureCommand,&rec,&pass,&updated)); rec=updated; }
				CHECK(RalWebGpu_ProductSkyCaptureFace(product,&runtimeReceipt,captureCommand,face,&rec,&updated));
				rec=updated;
				CHECK(!RalWebGpu_ProductSkyCaptureFace(product,&runtimeReceipt,captureCommand,face,&rec,&updated));
			}
			CHECK(RalWebGpu_CommandEnd(captureCommand,&rec,&executable));
			CHECK(executable.drawCount==6 && executable.passCount==6 && !executable.dispatchCount);
			CHECK(RalWebGpu_CommandSubmit(captureCommand,&executable,&submit));
			CHECK(RalWebGpu_CommandPoll(captureCommand,&submit)==RAL_WEBGPU_ASYNC_READY);
			RalWebGpu_ProductSkyCaptureEnd(product);
			if(cycle) CHECK(host.resourcesCreated==created && host.skyGroupsCreated==groups);
			created=host.resourcesCreated; groups=host.skyGroupsCreated;
		}
		CHECK(host.skyCaptureFaces==63 && host.skyCaptureDraws==12);
		CHECK(RalWebGpu_ProductSkyCaptureBegin(product,&runtimeReceipt,&frontend,&captureMaterial,0,1,0x9005,0x9006,0));
		ralWebGpuPassDesc_t pass={0}; pass.kind=RAL_WEBGPU_PASS_RENDER; pass.targetIdentity=skyResources.sourceTextureIdentity;
		pass.colorLoadOp=RAL_WEBGPU_LOAD_CLEAR;
		ralWebGpuCommandReceipt_t rec,updated;
		CHECK(RalWebGpu_CommandBeginWithDesc(captureCommand,&pass,&rec));
		host.failDraw=qtrue;
		CHECK(!RalWebGpu_ProductSkyCaptureFace(product,&runtimeReceipt,captureCommand,0,&rec,&updated));
		host.failDraw=qfalse;
		CHECK(RalWebGpu_CommandCancel(captureCommand,&rec));
		RalWebGpu_ProductSkyCaptureEnd(product);
		CHECK(RalWebGpu_ProductUnloadSkyRadianceResources(product,&runtimeReceipt));
		CHECK(RalWebGpu_ProductResetRegistrationAssets(product,&runtimeReceipt,&frontend));
	}
	{
		const uint32_t baseline = host.resourcesCreated - host.resourcesDestroyed;
		qhandle_t lastHandle = 0;
		for ( unsigned cycle = 0; cycle < 20u; ++cycle ) {
			const byte pixel[4] = {(byte)(cycle * 11u), 31, 97, 255};
			const uint32_t writes = host.textureWrites;
			const qhandle_t handle = RenderSubmission_RegisterMaterialImage( &frontend,
				RENDER_ASSET_MATERIAL, "*registration", qtrue, pixel, 1u, 1u );
			CHECK( handle > lastHandle ); lastHandle = handle;
			CHECK( RenderSubmission_RegisterStageTexture( &frontend,
				RENDER_ASSET_MATERIAL, "*registration-stage", qtrue, pixel, 1u, 1u ) > handle );
			CHECK( RalWebGpu_ProductPrepareMaterialResources( product, &runtimeReceipt, &frontend ) );
			CHECK( host.textureWrites == writes + 2u );
			CHECK( host.resourcesCreated - host.resourcesDestroyed == baseline + 4u );
			CHECK( RenderSubmission_BeginFrame( &frontend, 800u + cycle ) );
			CHECK( !RalWebGpu_ProductResetRegistrationAssets( product, &runtimeReceipt, &frontend ) );
			CHECK( frontend.materialBytes == 4u );
			CHECK( RenderSubmission_AddUiQuad( &frontend, 16, 24, 192, 96,
				0, 0, 1, 1, 0, handle ) );
			CHECK( RenderSubmission_EndFrame( &frontend, 800u + cycle, &submission ) );
			CHECK( !submission.worldLoaded && !submission.sceneRendered );
			const uint32_t passes = host.passes;
			const qboolean rendered = RalWebGpu_ProductRender( product, &runtimeReceipt, &frontend,
				&submission, 0x8004u, 800u + cycle, &productReceipt );
			if ( !rendered ) fprintf( stderr, "UI-only product stage=%u plan=%u\n",
				RalWebGpu_ProductLastFailureStage(), RalWebGpu_FrontendPlanLastFailureStage() );
			CHECK( rendered );
			CHECK( productReceipt.uiDrawCount == 1u && !productReceipt.worldDrawCount
				&& !productReceipt.toneMapDrawCount && !productReceipt.exposureDispatchCount );
			CHECK( host.passes == passes + 1u && !host.passHasDepth );
			CHECK( !RalWebGpu_ProductResetRegistrationAssets( product, &runtimeReceipt, &frontend ) );
			CHECK( RalWebGpu_ProductPoll( product, &productReceipt ) == RAL_WEBGPU_ASYNC_READY );
			CHECK( RalWebGpu_ProductResetRegistrationAssets( product, &runtimeReceipt, &frontend ) );
			CHECK( !frontend.materialBytes && !frontend.stageTextureBytes
				&& !frontend.registeredAssetCount );
			CHECK( host.resourcesCreated - host.resourcesDestroyed == baseline );
			CHECK( RalWebGpu_ProductResetRegistrationAssets( product, &runtimeReceipt, &frontend ) );
			CHECK( host.resourcesCreated - host.resourcesDestroyed == baseline );
		}
	}
	RalWebGpu_ProductDestroy( product, &runtimeReceipt );
	CHECK(host.skyGroupsCreated==host.skyGroupsDestroyed);
	RalWebGpu_BrowserBridgeDestroy(skyBridge);
	RenderSubmission_Reset( &frontend ); RalWebGpu_RuntimeDestroy( runtime );
	CHECK( host.resourcesCreated == host.resourcesDestroyed );
	puts( "ral WebGPU sealed frontend plan contract: PASS" ); return 0;
}
