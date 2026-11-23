// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
#include "ral_webgpu_taa.h"
#include "ral_webgpu_taa_resolve.h"
#include "ral_webgpu_taa_bootstrap.h"
#include <stdlib.h>
#include <string.h>

/* Same constants as the Vulkan and Metal renderers (vk_temporal_resolve.c, ral_metal_taa.mm). */
#define WIRED_TAA_DEPTH_ABSOLUTE 0.125f
#define WIRED_TAA_DEPTH_RELATIVE 0.02f
#define WIRED_TAA_HISTORY_WEIGHT 0.875f
#define WIRED_TAA_QUALITY_FLAGS 7u

enum { TAA_MOTION, TAA_BOOTSTRAP, TAA_RESOLVE, TAA_PIPELINE_COUNT };
enum { TEX_VELOCITY, TEX_VALIDITY, TEX_REVERSE_DEPTH, TEX_OUTPUT, TEX_HISTORY_COLOR0, TEX_HISTORY_COLOR1,
	TEX_HISTORY_DEPTH0, TEX_HISTORY_DEPTH1, TEX_EFFECT_MASK, TEX_COUNT };
enum { BUF_MOTION, BUF_BOOTSTRAP, BUF_RESOLVE, BUF_COUNT };

typedef struct { float co[4], cf[4], cl[4], cu[4], cp[4], po[4], pf[4], pl[4], pu[4], pp[4], jitter[4]; uint32_t extent[4]; } taaMotionParams_t;
typedef struct { uint32_t extent[2]; float zNear, zFar; } taaBootstrapParams_t;
typedef struct {
	uint32_t extent[2]; float currentJitterUv[2], previousJitterUv[2];
	float zNear, zFar, depthAbsolute, depthRelative, historyWeight;
	uint32_t historyReadIndex, qualityFlags, pad;
} taaResolveParams_t;

/* Inverse of ProjectVertices (ral_webgpu_product.c): clip = (side / tanHalfX, up / tanHalfY, forward * f/(f-n) - n f/(f-n), forward)
 * with side = -dot(delta, left). The scene depth is standard (0 near .. 1 far): rd = 1 - d gives forward = n f / (n + rd (f - n)),
 * which is the catalog resolve's linearization with zNear = n and zFar = f, so rd is stored as the "device depth" the resolve reads.
 * The jitter is removed before reconstruction, so the velocity is the unjittered camera motion the resolve expects. */
static const char s_motionWgsl[] =
	"struct P{co:vec4<f32>,cf:vec4<f32>,cl:vec4<f32>,cu:vec4<f32>,cp:vec4<f32>,"
	"po:vec4<f32>,pf:vec4<f32>,pl:vec4<f32>,pu:vec4<f32>,pp:vec4<f32>,jitter:vec4<f32>,extent:vec4<u32>};"
	"@group(0) @binding(0) var<uniform> p:P;"
	"@group(0) @binding(1) var sceneDepth:texture_depth_2d;"
	"@group(0) @binding(2) var velocity:texture_storage_2d<rgba16float,write>;"
	"@group(0) @binding(3) var validity:texture_storage_2d<rgba16float,write>;"
	"@group(0) @binding(4) var reverseDepth:texture_storage_2d<r32float,write>;"
	"@group(0) @binding(5) var effectMask:texture_2d<f32>;"
	"@compute @workgroup_size(8,8,1) fn main(@builtin(global_invocation_id) gid:vec3<u32>){"
	"if(gid.x>=p.extent.x||gid.y>=p.extent.y){return;}"
	"let d=textureLoad(sceneDepth,vec2<i32>(gid.xy),0);"
	"let ext=vec2<f32>(p.extent.xy);"
	"let n=p.cp.z;let f=p.cp.w;"
	"let rd=clamp(1.0-d,1.0e-7,1.0);"
	"let fwd=n*f/(n+rd*(f-n));"
	"let centre=vec2<f32>((f32(gid.x)+0.5)/ext.x*2.0-1.0,1.0-(f32(gid.y)+0.5)/ext.y*2.0);"
	"let ndc=centre-p.jitter.xy;"
	"let side=ndc.x*p.cp.x*fwd;let up=ndc.y*p.cp.y*fwd;"
	"let world=p.co.xyz+p.cf.xyz*fwd-p.cl.xyz*side+p.cu.xyz*up;"
	"let delta=world-p.po.xyz;"
	"let pfwd=dot(delta,p.pf.xyz);"
	"let covered=textureLoad(effectMask,vec2<i32>(gid.xy),0).r>0.5;"
	"let ok=pfwd>1.0e-3;"
	"var prev=ndc;"
	"if(ok){prev=vec2<f32>(-dot(delta,p.pl.xyz)/p.pp.x,dot(delta,p.pu.xyz)/p.pp.y)/pfwd;}"
	"let uvC=vec2<f32>(ndc.x*0.5+0.5,0.5-ndc.y*0.5);"
	"let uvP=vec2<f32>(prev.x*0.5+0.5,0.5-prev.y*0.5);"
	"textureStore(velocity,vec2<i32>(gid.xy),vec4<f32>(uvC-uvP,0.0,0.0));"
	"textureStore(validity,vec2<i32>(gid.xy),vec4<f32>(select(0.0,1.0,ok&&!covered),0.0,0.0,0.0));"
	"textureStore(reverseDepth,vec2<i32>(gid.xy),vec4<f32>(rd,0.0,0.0,0.0));}";

static uint32_t s_taaFailure;   /* last failing step of RalWebGpu_TaaRecord (product failure stage 100 + step) */
uint32_t RalWebGpu_TaaLastFailure( void ) { return s_taaFailure; }

struct ralWebGpuTaa_s {
	ralWebGpuRuntime_t *runtime;
	ralWebGpuBrowserBridge_t *bridge;
	uint64_t generation;
	ralWebGpuPipeline_t *pipelines[TAA_PIPELINE_COUNT];
	ralWebGpuPipelineReceipt_t pipelineReceipts[TAA_PIPELINE_COUNT];
	ralWebGpuResource_t *textures[TEX_COUNT], *buffers[BUF_COUNT], *sampler;
	ralWebGpuResourceReceipt_t textureReceipts[TEX_COUNT], bufferReceipts[BUF_COUNT], samplerReceipt;
	uintptr_t motionGroup;                       /* rebuilt per frame: it names the browser host's canvas depth */
	uintptr_t bootstrapGroups[2], resolveGroups[2];  /* indexed by the history write slot / read slot */
	uintptr_t sceneColor;
	uint32_t width, height;
};

typedef struct { uint32_t binding; ralShaderBindingClass_t kind; ralShaderTextureSampleType_t sample; ralFormat_t storage; uint64_t bytes; } taaBinding_t;

static qboolean Pipeline( ralWebGpuTaa_t *t, ralWebGpuRuntimeReceipt_t *rr, unsigned index, const char *source, uint32_t size,
		const taaBinding_t *spec, uint32_t count ) {
	const uint32_t spirv[4] = { 0x07230203u };
	ralShaderModuleAbi_t module;
	ralShaderBindingAbi_t bindings[12];
	ralShaderAbiManifest_t manifest;
	ralShaderVariantAbi_t variant;
	ralWebGpuWgslModule_t wgsl;
	ralComputePipelineCreateInfo_t compute;
	ralWebGpuPipelineCreateInfo_t info;
	const ralBindGroupLayout_t *layouts[1] = { (const ralBindGroupLayout_t *)(uintptr_t)1u };
	const uint64_t generation = t->generation + 4u * index;
	if ( count > 12u ) return qfalse;
	memset( &module, 0, sizeof( module ) );
	module.stage = RAL_STAGE_COMPUTE; module.entryPoint = "main";
	module.sourceDigest.lane0 = generation; module.sourceDigest.lane1 = generation + 1u;
	module.artifacts[RAL_SHADER_ARTIFACT_SPIRV].target = RAL_SHADER_ARTIFACT_SPIRV;
	module.artifacts[RAL_SHADER_ARTIFACT_SPIRV].byteCount = sizeof( spirv );
	module.artifacts[RAL_SHADER_ARTIFACT_MSL].target = RAL_SHADER_ARTIFACT_MSL;
	module.artifacts[RAL_SHADER_ARTIFACT_WGSL].target = RAL_SHADER_ARTIFACT_WGSL;
	module.artifacts[RAL_SHADER_ARTIFACT_WGSL].byteCount = size;
	if ( Ral_ShaderArtifactDigest( spirv, sizeof( spirv ), &module.artifacts[RAL_SHADER_ARTIFACT_SPIRV].digest ) != ralSuccess
			|| Ral_ShaderArtifactDigest( source, size, &module.artifacts[RAL_SHADER_ARTIFACT_WGSL].digest ) != ralSuccess )
		return qfalse;
	memset( bindings, 0, sizeof( bindings ) );
	for ( uint32_t i = 0u; i < count; ++i ) {
		bindings[i].binding = spec[i].binding; bindings[i].bindingClass = spec[i].kind;
		bindings[i].arrayCount = 1u; bindings[i].stageFlags = RAL_STAGE_COMPUTE;
		if ( spec[i].kind == RAL_SHADER_BIND_UNIFORM_BUFFER ) bindings[i].minBufferBindingSize = spec[i].bytes;
		else if ( spec[i].kind == RAL_SHADER_BIND_SAMPLED_TEXTURE ) {
			bindings[i].viewDimension = RAL_SHADER_VIEW_2D; bindings[i].sampleType = spec[i].sample;
		} else if ( spec[i].kind == RAL_SHADER_BIND_STORAGE_TEXTURE_WRITE ) {
			bindings[i].viewDimension = RAL_SHADER_VIEW_2D; bindings[i].storageTextureFormat = spec[i].storage;
		}
	}
	memset( &manifest, 0, sizeof( manifest ) );
	manifest.schemaVersion = RAL_SHADER_ABI_SCHEMA_VERSION; manifest.generation = generation;
	manifest.pipelineKind = RAL_SHADER_PIPELINE_COMPUTE;
	manifest.modules = &module; manifest.moduleCount = 1u;
	manifest.bindings = bindings; manifest.bindingCount = count;
	memset( &compute, 0, sizeof( compute ) );
	compute.bindGroupLayouts = layouts; compute.numBindGroupLayouts = 1u;
	memset( &variant, 0, sizeof( variant ) );
	variant.artifactTarget = RAL_SHADER_ARTIFACT_WGSL; variant.generation = generation;
	if ( Ral_ComputePipelineSemanticDigest( &compute, &variant.semanticStateDigest ) != ralSuccess ) return qfalse;
	wgsl.code = source; wgsl.byteCount = size;
	memset( &info, 0, sizeof( info ) );
	info.manifest = &manifest; info.variant = &variant; info.modules = &wgsl; info.moduleCount = 1u;
	info.computeState = &compute; info.generation = generation;
	if ( !RalWebGpu_BrowserBridgeApplyPipelineInfo( t->bridge, &info )
			|| !RalWebGpu_RuntimeCreatePipeline( t->runtime, rr, &info, &t->pipelines[index], &t->pipelineReceipts[index] ) )
		return qfalse;
	return RalWebGpu_RuntimeGetReceipt( t->runtime, rr );
}

#define SAMPLED(b, s) { b, RAL_SHADER_BIND_SAMPLED_TEXTURE, s, RAL_FORMAT_UNDEFINED, 0u }
#define STORAGE(b, f) { b, RAL_SHADER_BIND_STORAGE_TEXTURE_WRITE, RAL_SHADER_SAMPLE_FLOAT, f, 0u }
#define UNIFORM(b, n) { b, RAL_SHADER_BIND_UNIFORM_BUFFER, RAL_SHADER_SAMPLE_FLOAT, RAL_FORMAT_UNDEFINED, n }
#define SAMPLER(b) { b, RAL_SHADER_BIND_FILTERING_SAMPLER, RAL_SHADER_SAMPLE_FLOAT, RAL_FORMAT_UNDEFINED, 0u }

static qboolean Pipelines( ralWebGpuTaa_t *t, ralWebGpuRuntimeReceipt_t *rr ) {
	/* Binding tables follow the @group/@binding lines of the WGSL (the catalog modules: portable/temporal_*_comp_spv.wgsl). */
	const taaBinding_t motion[] = { UNIFORM( 0u, sizeof( taaMotionParams_t ) ), SAMPLED( 1u, RAL_SHADER_SAMPLE_DEPTH ),
		STORAGE( 2u, RAL_FORMAT_R16G16B16A16_SFLOAT ), STORAGE( 3u, RAL_FORMAT_R16G16B16A16_SFLOAT ),
		STORAGE( 4u, RAL_FORMAT_R32_SFLOAT ), SAMPLED( 5u, RAL_SHADER_SAMPLE_FLOAT ) };
	const taaBinding_t bootstrap[] = { SAMPLED( 0u, RAL_SHADER_SAMPLE_FLOAT ), SAMPLED( 1u, RAL_SHADER_SAMPLE_UNFILTERABLE_FLOAT ),
		SAMPLER( 2u ), STORAGE( 3u, RAL_FORMAT_R16G16B16A16_SFLOAT ), STORAGE( 4u, RAL_FORMAT_R16G16B16A16_SFLOAT ),
		STORAGE( 5u, RAL_FORMAT_R32_SFLOAT ), UNIFORM( 6u, sizeof( taaBootstrapParams_t ) ) };
	const taaBinding_t resolve[] = { SAMPLED( 0u, RAL_SHADER_SAMPLE_FLOAT ), SAMPLED( 1u, RAL_SHADER_SAMPLE_UNFILTERABLE_FLOAT ),
		SAMPLED( 2u, RAL_SHADER_SAMPLE_FLOAT ), SAMPLED( 3u, RAL_SHADER_SAMPLE_UNFILTERABLE_FLOAT ),
		SAMPLED( 4u, RAL_SHADER_SAMPLE_FLOAT ), SAMPLED( 5u, RAL_SHADER_SAMPLE_FLOAT ), SAMPLER( 6u ),
		STORAGE( 7u, RAL_FORMAT_R16G16B16A16_SFLOAT ), STORAGE( 8u, RAL_FORMAT_R16G16B16A16_SFLOAT ),
		STORAGE( 9u, RAL_FORMAT_R32_SFLOAT ), UNIFORM( 10u, sizeof( taaResolveParams_t ) ) };
	return Pipeline( t, rr, TAA_MOTION, s_motionWgsl, (uint32_t)sizeof( s_motionWgsl ) - 1u, motion, 6u )
		&& Pipeline( t, rr, TAA_BOOTSTRAP, (const char *)ral_webgpu_taa_bootstrap_wgsl,
			(uint32_t)sizeof( ral_webgpu_taa_bootstrap_wgsl ), bootstrap, 7u )
		&& Pipeline( t, rr, TAA_RESOLVE, (const char *)ral_webgpu_taa_resolve_wgsl,
			(uint32_t)sizeof( ral_webgpu_taa_resolve_wgsl ), resolve, 11u );
}

static void DestroyGroups( ralWebGpuTaa_t *t ) {
	for ( int i = 0; i < 2; ++i ) {
		if ( t->bootstrapGroups[i] ) RalWebGpu_BrowserBridgeDestroyBindGroup( t->bridge, t->bootstrapGroups[i] );
		if ( t->resolveGroups[i] ) RalWebGpu_BrowserBridgeDestroyBindGroup( t->bridge, t->resolveGroups[i] );
		t->bootstrapGroups[i] = t->resolveGroups[i] = 0;
	}
	if ( t->motionGroup ) RalWebGpu_BrowserBridgeDestroyBindGroup( t->bridge, t->motionGroup );
	t->motionGroup = 0;
}

static void DestroyTextures( ralWebGpuTaa_t *t, ralWebGpuResourceLayer_t *r ) {
	for ( int i = 0; i < TEX_COUNT; ++i ) {
		if ( t->textures[i] && r ) RalWebGpu_DestroyResource( r, t->textures[i], &t->textureReceipts[i] );
		t->textures[i] = NULL;
	}
	t->width = t->height = 0u; t->sceneColor = 0;
}

static ralWebGpuBrowserBindResource_t Tex( const ralWebGpuTaa_t *t, uint32_t binding, int index ) {
	return (ralWebGpuBrowserBindResource_t){ binding, RAL_WEBGPU_BROWSER_BIND_RESOURCE_TEXTURE,
		t->textureReceipts[index].resourceIdentity, 0u, 0u };
}
static ralWebGpuBrowserBindResource_t Buf( const ralWebGpuTaa_t *t, uint32_t binding, int index ) {
	return (ralWebGpuBrowserBindResource_t){ binding, RAL_WEBGPU_BROWSER_BIND_RESOURCE_BUFFER,
		t->bufferReceipts[index].resourceIdentity, 0u, t->bufferReceipts[index].byteSize };
}

/* Textures and the fixed bind groups follow the scene extent and the scene colour texture (both change only on resize). */
static qboolean Prepare( ralWebGpuTaa_t *t, ralWebGpuResourceLayer_t *r, uintptr_t sceneColor, uint32_t width, uint32_t height ) {
	if ( t->width == width && t->height == height && t->sceneColor == sceneColor && t->resolveGroups[0] ) return qtrue;
	DestroyGroups( t ); DestroyTextures( t, r );
	for ( int i = 0; i < TEX_COUNT; ++i ) {
		const qboolean depth = i == TEX_REVERSE_DEPTH || i == TEX_HISTORY_DEPTH0 || i == TEX_HISTORY_DEPTH1;
		const qboolean mask = i == TEX_EFFECT_MASK;
		ralWebGpuTextureDesc_t d;
		memset( &d, 0, sizeof( d ) );
		d.width = width; d.height = height; d.depth = d.mipLevelCount = 1u;
		d.format = mask ? RAL_FORMAT_R8_UNORM : ( depth ? RAL_FORMAT_R32_SFLOAT : RAL_FORMAT_R16G16B16A16_SFLOAT );
		d.bytesPerTexel = mask ? 1u : ( depth ? 4u : 8u );
		d.usage = RAL_WEBGPU_TEXTURE_SAMPLED | ( mask ? RAL_WEBGPU_TEXTURE_RENDER_ATTACHMENT : RAL_WEBGPU_TEXTURE_STORAGE );
		if ( !RalWebGpu_CreateTexture( r, &d, &t->textures[i], &t->textureReceipts[i] ) ) { s_taaFailure = 41u; return qfalse; }
	}
	const ralWebGpuBrowserBindResource_t scene = { 0u, RAL_WEBGPU_BROWSER_BIND_RESOURCE_TEXTURE, sceneColor, 0u, 0u };
	const ralWebGpuBrowserBindResource_t sampler = { 0u, RAL_WEBGPU_BROWSER_BIND_RESOURCE_SAMPLER,
		t->samplerReceipt.resourceIdentity, 0u, 0u };
	for ( int slot = 0; slot < 2; ++slot ) {
		ralWebGpuBrowserBindResource_t b[7] = { scene, Tex( t, 1u, TEX_REVERSE_DEPTH ), sampler, Tex( t, 3u, TEX_OUTPUT ),
			Tex( t, 4u, TEX_HISTORY_COLOR0 + slot ), Tex( t, 5u, TEX_HISTORY_DEPTH0 + slot ), Buf( t, 6u, BUF_BOOTSTRAP ) };
		b[2].binding = 2u;
		const int read = slot, write = 1 - slot;
		ralWebGpuBrowserBindResource_t s[11] = { scene, Tex( t, 1u, TEX_REVERSE_DEPTH ), Tex( t, 2u, TEX_HISTORY_COLOR0 + read ),
			Tex( t, 3u, TEX_HISTORY_DEPTH0 + read ), Tex( t, 4u, TEX_VELOCITY ), Tex( t, 5u, TEX_VALIDITY ), sampler,
			Tex( t, 7u, TEX_OUTPUT ), Tex( t, 8u, TEX_HISTORY_COLOR0 + write ), Tex( t, 9u, TEX_HISTORY_DEPTH0 + write ),
			Buf( t, 10u, BUF_RESOLVE ) };
		s[6].binding = 6u;
		if ( !RalWebGpu_BrowserBridgeCreateBindGroup( t->bridge, t->pipelineReceipts[TAA_BOOTSTRAP].bindGroupLayoutIdentities[0],
				b, 7u, &t->bootstrapGroups[slot] )
				|| !RalWebGpu_BrowserBridgeCreateBindGroup( t->bridge, t->pipelineReceipts[TAA_RESOLVE].bindGroupLayoutIdentities[0],
					s, 11u, &t->resolveGroups[slot] ) ) { s_taaFailure = 42u; return qfalse; }
	}
	t->width = width; t->height = height; t->sceneColor = sceneColor;
	return qtrue;
}

qboolean RalWebGpu_TaaCreate( ralWebGpuRuntime_t *runtime, ralWebGpuRuntimeReceipt_t *rr, ralWebGpuBrowserBridge_t *bridge,
		uint64_t generation, ralWebGpuTaa_t **out ) {
	if ( !runtime || !rr || !bridge || !generation || generation > UINT64_MAX - 64u || !out || *out ) return qfalse;
	ralWebGpuTaa_t *t = calloc( 1, sizeof( *t ) );
	if ( !t ) return qfalse;
	t->runtime = runtime; t->bridge = bridge; t->generation = generation;
	ralWebGpuResourceLayer_t *r = RalWebGpu_RuntimeResources( runtime, rr );
	const uint64_t sizes[BUF_COUNT] = { sizeof( taaMotionParams_t ), sizeof( taaBootstrapParams_t ), sizeof( taaResolveParams_t ) };
	if ( !r || !Pipelines( t, rr ) ) goto fail;
	for ( int i = 0; i < BUF_COUNT; ++i ) {
		ralWebGpuBufferDesc_t d;
		memset( &d, 0, sizeof( d ) );
		d.size = ( sizes[i] + 15u ) & ~(uint64_t)15u;
		d.usage = RAL_WEBGPU_BUFFER_UNIFORM | RAL_WEBGPU_BUFFER_COPY_DESTINATION;
		d.memoryClass = RAL_ALLOCATION_DEVICE_LOCAL;
		if ( !RalWebGpu_CreateBuffer( r, &d, &t->buffers[i], &t->bufferReceipts[i] ) ) goto fail;
	}
	{
		ralWebGpuSamplerDesc_t s;
		memset( &s, 0, sizeof( s ) ); s.clampToEdge = qtrue;
		s.linearMinification = s.linearMagnification = qtrue;   /* bicubic history fetch uses bilinear taps */
		if ( !RalWebGpu_CreateSampler( r, &s, &t->sampler, &t->samplerReceipt ) ) goto fail;
	}
	*out = t; return qtrue;
fail:
	RalWebGpu_TaaDestroy( t, rr ); return qfalse;
}

void RalWebGpu_TaaDestroy( ralWebGpuTaa_t *t, const ralWebGpuRuntimeReceipt_t *rr ) {
	if ( !t ) return;
	ralWebGpuResourceLayer_t *r = RalWebGpu_RuntimeResources( t->runtime, rr );
	DestroyGroups( t ); DestroyTextures( t, r );
	if ( r ) {
		for ( int i = BUF_COUNT; i > 0; --i ) if ( t->buffers[i - 1] ) RalWebGpu_DestroyResource( r, t->buffers[i - 1], &t->bufferReceipts[i - 1] );
		if ( t->sampler ) RalWebGpu_DestroyResource( r, t->sampler, &t->samplerReceipt );
	}
	for ( int i = TAA_PIPELINE_COUNT; i > 0; --i )
		if ( t->pipelines[i - 1] ) RalWebGpu_RuntimeDestroyPipeline( t->runtime, t->pipelines[i - 1], &t->pipelineReceipts[i - 1] );
	free( t );
}

static void Camera( float origin[4], float fwd[4], float left[4], float up[4], float proj[4], const ralWebGpuTaaCamera_t *c ) {
	for ( int i = 0; i < 3; ++i ) { origin[i] = c->origin[i]; fwd[i] = c->forward[i]; left[i] = c->left[i]; up[i] = c->up[i]; }
	origin[3] = fwd[3] = left[3] = up[3] = 0.0f;
	proj[0] = c->tanHalfX; proj[1] = c->tanHalfY; proj[2] = c->zNear; proj[3] = c->zFar;
}

uintptr_t RalWebGpu_TaaEffectMask( ralWebGpuTaa_t *t, const ralWebGpuRuntimeReceipt_t *rr, uintptr_t sceneColor,
		uint32_t width, uint32_t height ) {
	ralWebGpuResourceLayer_t *r = t && rr ? RalWebGpu_RuntimeResources( t->runtime, rr ) : NULL;
	if ( !r || !sceneColor || !width || !height || !Prepare( t, r, sceneColor, width, height ) ) return 0;
	return t->textureReceipts[TEX_EFFECT_MASK].resourceIdentity;
}

qboolean RalWebGpu_TaaRecord( ralWebGpuTaa_t *t, const ralWebGpuRuntimeReceipt_t *rr, ralWebGpuCommand_t *command,
		ralWebGpuCommandReceipt_t *recording, uintptr_t sceneColor, const ralWebGpuTaaFrame_t *frame,
		uintptr_t *outResolved, uint32_t *outDispatchCount ) {
	ralWebGpuResourceLayer_t *r;
	ralWebGpuWriteReceipt_t wr;
	ralWebGpuCommandReceipt_t next;
	ralWebGpuPassDesc_t pass;
	if ( !t || !rr || !command || !recording || !sceneColor || !frame || !outResolved || !outDispatchCount
			|| !frame->width || !frame->height || frame->readIndex > 1u || frame->writeIndex > 1u ) { s_taaFailure = 1u; return qfalse; }
	r = RalWebGpu_RuntimeResources( t->runtime, rr );
	if ( !r ) { s_taaFailure = 2u; return qfalse; }
	if ( !Prepare( t, r, sceneColor, frame->width, frame->height ) ) return qfalse;   /* Prepare sets 41.. */
	{
		taaMotionParams_t m;
		memset( &m, 0, sizeof( m ) );
		Camera( m.co, m.cf, m.cl, m.cu, m.cp, &frame->current );
		Camera( m.po, m.pf, m.pl, m.pu, m.pp, frame->historyValid ? &frame->previous : &frame->current );
		m.jitter[0] = frame->jitterNdc[0]; m.jitter[1] = frame->jitterNdc[1];
		m.extent[0] = frame->width; m.extent[1] = frame->height;
		if ( !RalWebGpu_WriteBuffer( r, t->buffers[BUF_MOTION], &t->bufferReceipts[BUF_MOTION], 0u, &m, sizeof( m ), &wr ) )
			{ s_taaFailure = 3u; return qfalse; }
	}
	if ( frame->historyValid ) {
		taaResolveParams_t p;
		memset( &p, 0, sizeof( p ) );
		p.extent[0] = frame->width; p.extent[1] = frame->height;
		p.currentJitterUv[0] = frame->currentJitterUv[0]; p.currentJitterUv[1] = frame->currentJitterUv[1];
		p.previousJitterUv[0] = frame->previousJitterUv[0]; p.previousJitterUv[1] = frame->previousJitterUv[1];
		p.zNear = frame->current.zNear; p.zFar = frame->current.zFar;
		p.depthAbsolute = WIRED_TAA_DEPTH_ABSOLUTE; p.depthRelative = WIRED_TAA_DEPTH_RELATIVE;
		p.historyWeight = WIRED_TAA_HISTORY_WEIGHT; p.historyReadIndex = frame->readIndex; p.qualityFlags = WIRED_TAA_QUALITY_FLAGS;
		if ( !RalWebGpu_WriteBuffer( r, t->buffers[BUF_RESOLVE], &t->bufferReceipts[BUF_RESOLVE], 0u, &p, sizeof( p ), &wr ) )
			{ s_taaFailure = 4u; return qfalse; }
	} else {
		const taaBootstrapParams_t p = { { frame->width, frame->height }, frame->current.zNear, frame->current.zFar };
		if ( !RalWebGpu_WriteBuffer( r, t->buffers[BUF_BOOTSTRAP], &t->bufferReceipts[BUF_BOOTSTRAP], 0u, &p, sizeof( p ), &wr ) )
			{ s_taaFailure = 5u; return qfalse; }
	}
	/* The motion group names the browser host's canvas depth, which the host recreates on every canvas configure, so it is rebuilt
	 * each frame (the previous frame has completed: one frame is outstanding at a time). */
	if ( t->motionGroup ) { RalWebGpu_BrowserBridgeDestroyBindGroup( t->bridge, t->motionGroup ); t->motionGroup = 0; }
	{
		const ralWebGpuBrowserBindResource_t m[6] = { Buf( t, 0u, BUF_MOTION ),
			{ 1u, RAL_WEBGPU_BROWSER_BIND_RESOURCE_SCENE_DEPTH, (uintptr_t)1u, 0u, 0u },
			Tex( t, 2u, TEX_VELOCITY ), Tex( t, 3u, TEX_VALIDITY ), Tex( t, 4u, TEX_REVERSE_DEPTH ), Tex( t, 5u, TEX_EFFECT_MASK ) };
		if ( !RalWebGpu_BrowserBridgeCreateBindGroup( t->bridge, t->pipelineReceipts[TAA_MOTION].bindGroupLayoutIdentities[0],
				m, 6u, &t->motionGroup ) ) { s_taaFailure = 6u; return qfalse; }
	}
	memset( &pass, 0, sizeof( pass ) ); pass.kind = RAL_WEBGPU_PASS_COMPUTE;
	if ( !RalWebGpu_CommandNextPass( command, recording, &pass, &next ) ) { s_taaFailure = 7u; return qfalse; }
	*recording = next;
	for ( int step = 0; step < 2; ++step ) {
		ralWebGpuComputeDispatch_t d;
		memset( &d, 0, sizeof( d ) );
		const unsigned pipeline = step == 0 ? TAA_MOTION : ( frame->historyValid ? TAA_RESOLVE : TAA_BOOTSTRAP );
		d.pipelineIdentity = t->pipelineReceipts[pipeline].pipelineIdentity;
		d.bindGroupCount = 1u;
		d.bindGroupIdentities[0] = step == 0 ? t->motionGroup
			: ( frame->historyValid ? t->resolveGroups[frame->readIndex] : t->bootstrapGroups[frame->writeIndex] );
		d.groupCountX = ( frame->width + 7u ) / 8u; d.groupCountY = ( frame->height + 7u ) / 8u; d.groupCountZ = 1u;
		d.contentDigest = t->generation + 1u + (uint64_t)pipeline;
		if ( !RalWebGpu_CommandRecordComputeDispatch( command, recording, &d, &next ) ) { s_taaFailure = 8u; return qfalse; }
		*recording = next;
	}
	*outResolved = t->textureReceipts[TEX_OUTPUT].resourceIdentity;
	*outDispatchCount = 2u;
	return qtrue;
}
