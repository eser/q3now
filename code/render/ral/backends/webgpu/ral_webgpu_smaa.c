// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
#include "ral_webgpu_smaa.h"
#include "ral_webgpu_smaa_edge_vert.h"
#include "ral_webgpu_smaa_edge_frag.h"
#include "ral_webgpu_smaa_blend_vert.h"
#include "ral_webgpu_smaa_blend_frag.h"
#include "ral_webgpu_smaa_resolve_vert.h"
#include "ral_webgpu_smaa_resolve_frag.h"
#include "../vulkan/renderer/smaa_area_texture.h"
#include "../vulkan/renderer/smaa_search_texture.h"
#include <stdlib.h>
#include <string.h>

/* Presets (index = quality): identical to the Vulkan and Metal renderers' tables. */
static const float kThreshold[] = { 0.0f, 0.15f, 0.10f, 0.10f, 0.05f };
static const int kSearchSteps[] = { 0, 4, 8, 16, 32 };
static const int kSearchStepsDiag[] = { 0, 0, 0, 8, 16 };
static const int kCornerRounding[] = { 0, 0, 0, 25, 25 };

enum { SMAA_EDGE, SMAA_BLEND, SMAA_RESOLVE, SMAA_PASS_COUNT };
enum { TEX_EDGES, TEX_BLEND, TEX_OUTPUT, TEX_AREA, TEX_SEARCH, TEX_COUNT };
enum { BUF_METRICS, BUF_VERTEX, BUF_INDEX, BUF_COUNT };
/* Bind groups per pass, indexed by WGSL group (0..3). */
enum { GROUP_COUNT = 4 };

static uint32_t s_smaaFailure;
uint32_t RalWebGpu_SmaaLastFailure( void ) { return s_smaaFailure; }
#define FAIL( code ) do { s_smaaFailure = ( code ); return qfalse; } while ( 0 )

struct ralWebGpuSmaa_s {
	ralWebGpuRuntime_t *runtime;
	ralWebGpuBrowserBridge_t *bridge;
	uint64_t generation, nextGeneration;
	int quality;
	ralWebGpuPipeline_t *pipelines[SMAA_PASS_COUNT];
	ralWebGpuPipelineReceipt_t pipelineReceipts[SMAA_PASS_COUNT];
	ralWebGpuResource_t *textures[TEX_COUNT], *buffers[BUF_COUNT], *point, *linear;
	ralWebGpuResourceReceipt_t textureReceipts[TEX_COUNT], bufferReceipts[BUF_COUNT], pointReceipt, linearReceipt;
	uintptr_t groups[SMAA_PASS_COUNT][GROUP_COUNT];
	uintptr_t sceneColor;
	uint32_t width, height;
};

typedef struct { uint32_t set, binding; ralShaderBindingClass_t kind; uint32_t stages; } smaaBinding_t;

static qboolean Module( ralShaderModuleAbi_t *m, uint32_t stage, const char *source, uint32_t size, uint64_t generation ) {
	const uint32_t spirv[4] = { 0x07230203u };
	memset( m, 0, sizeof( *m ) );
	m->stage = stage; m->entryPoint = "main";
	m->sourceDigest.lane0 = generation; m->sourceDigest.lane1 = generation + 1u;
	m->artifacts[RAL_SHADER_ARTIFACT_SPIRV].target = RAL_SHADER_ARTIFACT_SPIRV;
	m->artifacts[RAL_SHADER_ARTIFACT_SPIRV].byteCount = sizeof( spirv );
	m->artifacts[RAL_SHADER_ARTIFACT_MSL].target = RAL_SHADER_ARTIFACT_MSL;
	m->artifacts[RAL_SHADER_ARTIFACT_WGSL].target = RAL_SHADER_ARTIFACT_WGSL;
	m->artifacts[RAL_SHADER_ARTIFACT_WGSL].byteCount = size;
	return Ral_ShaderArtifactDigest( spirv, sizeof( spirv ), &m->artifacts[RAL_SHADER_ARTIFACT_SPIRV].digest ) == ralSuccess
		&& Ral_ShaderArtifactDigest( source, size, &m->artifacts[RAL_SHADER_ARTIFACT_WGSL].digest ) == ralSuccess;
}

/* One full-screen pass: the catalog vertex stage derives the triangle from the vertex index (the 4-byte vertex buffer is a
 * placeholder the draw ABI requires), the fragment writes one target, there is no depth. Bindings follow the WGSL. */
static qboolean Pipeline( ralWebGpuSmaa_t *t, ralWebGpuRuntimeReceipt_t *rr, unsigned pass,
		const unsigned char *vs, uint32_t vsSize, const unsigned char *fs, uint32_t fsSize, ralFormat_t target,
		const smaaBinding_t *spec, uint32_t count ) {
	ralShaderModuleAbi_t modules[2];
	ralWebGpuWgslModule_t wgsl[2];
	ralShaderBindingAbi_t bindings[8];
	ralShaderAbiManifest_t manifest;
	ralShaderVariantAbi_t variant;
	ralGraphicsPipelineCreateInfo_t graphics;
	ralWebGpuPipelineCreateInfo_t info;
	ralVertexBinding_t vertex;
	ralColorBlendAttachment_t blend;
	ralShaderSpecConstantAbi_t specs[3];
	ralShaderSpecValue_t values[3];
	ralSpecConstant_t native[3];
	uint32_t specCount = 0u, bits;
	const ralBindGroupLayout_t *layouts[GROUP_COUNT] = {
		(const ralBindGroupLayout_t *)(uintptr_t)1u, (const ralBindGroupLayout_t *)(uintptr_t)2u,
		(const ralBindGroupLayout_t *)(uintptr_t)3u, (const ralBindGroupLayout_t *)(uintptr_t)4u };
	const uint64_t generation = t->nextGeneration;
	if ( count > 8u || !Module( &modules[0], RAL_STAGE_VERTEX, (const char *)vs, vsSize, generation )
			|| !Module( &modules[1], RAL_STAGE_FRAGMENT, (const char *)fs, fsSize, generation + 2u ) ) FAIL( 10u + pass );
	wgsl[0] = (ralWebGpuWgslModule_t){ (const char *)vs, vsSize };
	wgsl[1] = (ralWebGpuWgslModule_t){ (const char *)fs, fsSize };
	memset( bindings, 0, sizeof( bindings ) );
	for ( uint32_t i = 0u; i < count; ++i ) {
		bindings[i].set = spec[i].set; bindings[i].binding = spec[i].binding; bindings[i].bindingClass = spec[i].kind;
		bindings[i].arrayCount = 1u; bindings[i].stageFlags = spec[i].stages;
		if ( spec[i].kind == RAL_SHADER_BIND_UNIFORM_BUFFER ) bindings[i].minBufferBindingSize = 16u;
		else if ( spec[i].kind == RAL_SHADER_BIND_SAMPLED_TEXTURE ) {
			bindings[i].viewDimension = RAL_SHADER_VIEW_2D; bindings[i].sampleType = RAL_SHADER_SAMPLE_FLOAT;
		}
	}
	memset( specs, 0, sizeof( specs ) ); memset( values, 0, sizeof( values ) ); memset( native, 0, sizeof( native ) );
	if ( pass == SMAA_EDGE ) {
		memcpy( &bits, &kThreshold[t->quality], sizeof( bits ) );
		specs[0] = (ralShaderSpecConstantAbi_t){ 0u, RAL_STAGE_FRAGMENT, bits, RAL_SHADER_SPEC_F32 };
		specCount = 1u;
	} else if ( pass == SMAA_BLEND ) {
		specs[0] = (ralShaderSpecConstantAbi_t){ 0u, RAL_STAGE_VERTEX, (uint32_t)kSearchSteps[t->quality], RAL_SHADER_SPEC_I32 };
		specs[1] = (ralShaderSpecConstantAbi_t){ 1u, RAL_STAGE_FRAGMENT, (uint32_t)kSearchStepsDiag[t->quality], RAL_SHADER_SPEC_I32 };
		specs[2] = (ralShaderSpecConstantAbi_t){ 2u, RAL_STAGE_FRAGMENT, (uint32_t)kCornerRounding[t->quality], RAL_SHADER_SPEC_I32 };
		specCount = 3u;
	}
	for ( uint32_t i = 0u; i < specCount; ++i ) {
		values[i] = (ralShaderSpecValue_t){ specs[i].constantId, specs[i].defaultValue };
		native[i] = (ralSpecConstant_t){ specs[i].constantId, specs[i].defaultValue };
	}
	memset( &manifest, 0, sizeof( manifest ) );
	manifest.schemaVersion = RAL_SHADER_ABI_SCHEMA_VERSION; manifest.generation = generation;
	manifest.pipelineKind = RAL_SHADER_PIPELINE_GRAPHICS;
	manifest.modules = modules; manifest.moduleCount = 2u;
	manifest.bindings = bindings; manifest.bindingCount = count;
	manifest.specConstants = specCount ? specs : NULL; manifest.specConstantCount = specCount;
	memset( &variant, 0, sizeof( variant ) );
	variant.artifactTarget = RAL_SHADER_ARTIFACT_WGSL; variant.generation = generation;
	variant.specValues = specCount ? values : NULL; variant.specValueCount = specCount;
	memset( &graphics, 0, sizeof( graphics ) );
	memset( &vertex, 0, sizeof( vertex ) ); vertex.stride = sizeof( float ); vertex.inputRate = RAL_VERTEX_INPUT_PER_VERTEX;
	memset( &blend, 0, sizeof( blend ) ); blend.writeMask = RAL_COLOR_WRITE_ALL; blend.writeMaskExplicit = qtrue;
	graphics.vertexBindings = &vertex; graphics.numVertexBindings = 1u;
	graphics.topology = RAL_TOPOLOGY_TRIANGLE_LIST;
	graphics.raster.polygonMode = RAL_POLYGON_FILL; graphics.raster.lineWidth = 1.0f;
	graphics.colorBlends = &blend; graphics.numColorBlends = 1u;
	graphics.colorFormats[0] = target; graphics.numColorFormats = 1u; graphics.sampleCount = 1u;
	graphics.depthFormat = RAL_FORMAT_UNDEFINED; graphics.depthStencil.depthCompareOp = RAL_COMPARE_ALWAYS;
	graphics.bindGroupLayouts = layouts; graphics.numBindGroupLayouts = GROUP_COUNT;
	graphics.specConstants = specCount ? native : NULL; graphics.numSpecConstants = specCount;
	if ( Ral_GraphicsPipelineSemanticDigest( &graphics, &variant.semanticStateDigest ) != ralSuccess ) FAIL( 20u + pass );
	memset( &info, 0, sizeof( info ) );
	info.manifest = &manifest; info.variant = &variant; info.modules = wgsl; info.moduleCount = 2u;
	info.graphicsState = &graphics; info.generation = generation;
	if ( !RalWebGpu_BrowserBridgeApplyPipelineInfo( t->bridge, &info ) ) FAIL( 30u + pass );
	if ( !RalWebGpu_RuntimeCreatePipeline( t->runtime, rr, &info, &t->pipelines[pass], &t->pipelineReceipts[pass] ) )
		FAIL( 40u + pass );
	t->nextGeneration += 4u;
	return RalWebGpu_RuntimeGetReceipt( t->runtime, rr );
}

#define VS RAL_STAGE_VERTEX
#define FS RAL_STAGE_FRAGMENT
static qboolean Pipelines( ralWebGpuSmaa_t *t, ralWebGpuRuntimeReceipt_t *rr ) {
	/* The catalog SMAA modules: group 0..2 hold one texture (binding 0) and its sampler (binding 32), group 3 the RtMetrics. */
	const smaaBinding_t edge[] = { { 0u, 0u, RAL_SHADER_BIND_SAMPLED_TEXTURE, FS }, { 0u, 32u, RAL_SHADER_BIND_FILTERING_SAMPLER, FS },
		{ 3u, 0u, RAL_SHADER_BIND_UNIFORM_BUFFER, VS | FS } };
	const smaaBinding_t blend[] = { { 0u, 0u, RAL_SHADER_BIND_SAMPLED_TEXTURE, FS }, { 0u, 32u, RAL_SHADER_BIND_FILTERING_SAMPLER, FS },
		{ 1u, 0u, RAL_SHADER_BIND_SAMPLED_TEXTURE, FS }, { 1u, 32u, RAL_SHADER_BIND_FILTERING_SAMPLER, FS },
		{ 2u, 0u, RAL_SHADER_BIND_SAMPLED_TEXTURE, FS }, { 2u, 32u, RAL_SHADER_BIND_FILTERING_SAMPLER, FS },
		{ 3u, 0u, RAL_SHADER_BIND_UNIFORM_BUFFER, VS | FS } };
	const smaaBinding_t resolve[] = { { 0u, 0u, RAL_SHADER_BIND_SAMPLED_TEXTURE, FS }, { 0u, 32u, RAL_SHADER_BIND_FILTERING_SAMPLER, FS },
		{ 1u, 0u, RAL_SHADER_BIND_SAMPLED_TEXTURE, FS }, { 1u, 32u, RAL_SHADER_BIND_FILTERING_SAMPLER, FS },
		{ 3u, 0u, RAL_SHADER_BIND_UNIFORM_BUFFER, VS | FS } };
	return Pipeline( t, rr, SMAA_EDGE, ral_webgpu_smaa_edge_vert_wgsl, sizeof( ral_webgpu_smaa_edge_vert_wgsl ),
			ral_webgpu_smaa_edge_frag_wgsl, sizeof( ral_webgpu_smaa_edge_frag_wgsl ), RAL_FORMAT_R8G8_UNORM, edge, 3u )
		&& Pipeline( t, rr, SMAA_BLEND, ral_webgpu_smaa_blend_vert_wgsl, sizeof( ral_webgpu_smaa_blend_vert_wgsl ),
			ral_webgpu_smaa_blend_frag_wgsl, sizeof( ral_webgpu_smaa_blend_frag_wgsl ), RAL_FORMAT_R8G8B8A8_UNORM, blend, 7u )
		&& Pipeline( t, rr, SMAA_RESOLVE, ral_webgpu_smaa_resolve_vert_wgsl, sizeof( ral_webgpu_smaa_resolve_vert_wgsl ),
			ral_webgpu_smaa_resolve_frag_wgsl, sizeof( ral_webgpu_smaa_resolve_frag_wgsl ), RAL_FORMAT_R16G16B16A16_SFLOAT, resolve, 5u );
}
#undef VS
#undef FS

static void DestroyGroups( ralWebGpuSmaa_t *t ) {
	for ( int p = 0; p < SMAA_PASS_COUNT; ++p )
		for ( int g = 0; g < GROUP_COUNT; ++g ) {
			if ( t->groups[p][g] ) RalWebGpu_BrowserBridgeDestroyBindGroup( t->bridge, t->groups[p][g] );
			t->groups[p][g] = 0;
		}
	t->sceneColor = 0;
}

static void DestroyPipelines( ralWebGpuSmaa_t *t ) {
	for ( int i = SMAA_PASS_COUNT; i > 0; --i ) {
		if ( t->pipelines[i - 1] ) RalWebGpu_RuntimeDestroyPipeline( t->runtime, t->pipelines[i - 1], &t->pipelineReceipts[i - 1] );
		t->pipelines[i - 1] = NULL;
	}
}

static qboolean Texture( ralWebGpuResourceLayer_t *r, ralWebGpuSmaa_t *t, int index, ralFormat_t format, uint32_t bytes,
		uint32_t width, uint32_t height, uint32_t usage ) {
	ralWebGpuTextureDesc_t d;
	memset( &d, 0, sizeof( d ) );
	d.width = width; d.height = height; d.depth = d.mipLevelCount = 1u;
	d.format = format; d.bytesPerTexel = bytes; d.usage = usage;
	return RalWebGpu_CreateTexture( r, &d, &t->textures[index], &t->textureReceipts[index] );
}

/* Uploads a lookup texture with rows padded to the 256-byte pitch the resource layer requires. */
static qboolean Lookup( ralWebGpuResourceLayer_t *r, ralWebGpuSmaa_t *t, int index, ralFormat_t format, uint32_t bytesPerTexel,
		uint32_t width, uint32_t height, const unsigned char *bytes, uint32_t pitch ) {
	const uint32_t row = ( width * bytesPerTexel + 255u ) & ~255u;
	unsigned char *staging = calloc( (size_t)row * height, 1u );
	ralWebGpuWriteReceipt_t wr;
	qboolean ok;
	if ( !staging ) return qfalse;
	for ( uint32_t y = 0u; y < height; ++y ) memcpy( staging + (size_t)y * row, bytes + (size_t)y * pitch, width * bytesPerTexel );
	ok = Texture( r, t, index, format, bytesPerTexel, width, height, RAL_WEBGPU_TEXTURE_SAMPLED | RAL_WEBGPU_TEXTURE_COPY_DESTINATION )
		&& RalWebGpu_WriteTexture( r, t->textures[index], &t->textureReceipts[index], staging, (uint64_t)row * height, row, height, &wr );
	free( staging );
	return ok;
}

qboolean RalWebGpu_SmaaCreate( ralWebGpuRuntime_t *runtime, ralWebGpuRuntimeReceipt_t *rr, ralWebGpuBrowserBridge_t *bridge,
		uint64_t generation, ralWebGpuSmaa_t **out ) {
	if ( !runtime || !rr || !bridge || !generation || generation > UINT64_MAX - 4096u || !out || *out ) return qfalse;
	ralWebGpuSmaa_t *t = calloc( 1, sizeof( *t ) );
	if ( !t ) return qfalse;
	t->runtime = runtime; t->bridge = bridge; t->generation = t->nextGeneration = generation; t->quality = 3;
	ralWebGpuResourceLayer_t *r = RalWebGpu_RuntimeResources( runtime, rr );
	if ( !r || !Pipelines( t, rr ) ) goto fail;
	{
		const uint32_t sizes[BUF_COUNT] = { 16u, 4u, 12u };
		const uint32_t usage[BUF_COUNT] = { RAL_WEBGPU_BUFFER_UNIFORM, RAL_WEBGPU_BUFFER_VERTEX, RAL_WEBGPU_BUFFER_INDEX };
		const uint32_t indices[3] = { 0u, 1u, 2u };
		const float placeholder = 0.0f;
		ralWebGpuWriteReceipt_t wr;
		for ( int i = 0; i < BUF_COUNT; ++i ) {
			ralWebGpuBufferDesc_t d;
			memset( &d, 0, sizeof( d ) );
			d.size = sizes[i]; d.usage = usage[i] | RAL_WEBGPU_BUFFER_COPY_DESTINATION; d.memoryClass = RAL_ALLOCATION_DEVICE_LOCAL;
			if ( !RalWebGpu_CreateBuffer( r, &d, &t->buffers[i], &t->bufferReceipts[i] ) ) goto fail;
		}
		if ( !RalWebGpu_WriteBuffer( r, t->buffers[BUF_INDEX], &t->bufferReceipts[BUF_INDEX], 0u, indices, sizeof( indices ), &wr )
				|| !RalWebGpu_WriteBuffer( r, t->buffers[BUF_VERTEX], &t->bufferReceipts[BUF_VERTEX], 0u, &placeholder,
					sizeof( placeholder ), &wr ) ) goto fail;
	}
	{
		ralWebGpuSamplerDesc_t s;
		memset( &s, 0, sizeof( s ) ); s.clampToEdge = qtrue;
		if ( !RalWebGpu_CreateSampler( r, &s, &t->point, &t->pointReceipt ) ) goto fail;
		s.linearMinification = s.linearMagnification = qtrue;
		if ( !RalWebGpu_CreateSampler( r, &s, &t->linear, &t->linearReceipt ) ) goto fail;
	}
	if ( !Lookup( r, t, TEX_AREA, RAL_FORMAT_R8G8_UNORM, 2u, AREATEX_WIDTH, AREATEX_HEIGHT, areaTexBytes, AREATEX_PITCH )
			|| !Lookup( r, t, TEX_SEARCH, RAL_FORMAT_R8_UNORM, 1u, SEARCHTEX_WIDTH, SEARCHTEX_HEIGHT, searchTexBytes, SEARCHTEX_PITCH ) )
		goto fail;
	*out = t; return qtrue;
fail:
	RalWebGpu_SmaaDestroy( t, rr ); return qfalse;
}

void RalWebGpu_SmaaDestroy( ralWebGpuSmaa_t *t, const ralWebGpuRuntimeReceipt_t *rr ) {
	if ( !t ) return;
	ralWebGpuResourceLayer_t *r = RalWebGpu_RuntimeResources( t->runtime, rr );
	DestroyGroups( t );
	if ( r ) {
		for ( int i = TEX_COUNT; i > 0; --i ) if ( t->textures[i - 1] ) RalWebGpu_DestroyResource( r, t->textures[i - 1], &t->textureReceipts[i - 1] );
		for ( int i = BUF_COUNT; i > 0; --i ) if ( t->buffers[i - 1] ) RalWebGpu_DestroyResource( r, t->buffers[i - 1], &t->bufferReceipts[i - 1] );
		if ( t->point ) RalWebGpu_DestroyResource( r, t->point, &t->pointReceipt );
		if ( t->linear ) RalWebGpu_DestroyResource( r, t->linear, &t->linearReceipt );
	}
	DestroyPipelines( t );
	free( t );
}

static ralWebGpuBrowserBindResource_t TexAt( uint32_t binding, uintptr_t identity ) {
	return (ralWebGpuBrowserBindResource_t){ binding, RAL_WEBGPU_BROWSER_BIND_RESOURCE_TEXTURE, identity, 0u, 0u };
}
static ralWebGpuBrowserBindResource_t SamplerAt( uintptr_t identity ) {
	return (ralWebGpuBrowserBindResource_t){ 32u, RAL_WEBGPU_BROWSER_BIND_RESOURCE_SAMPLER, identity, 0u, 0u };
}

/* Per-extent targets and the bind groups (they name the scene texture and the pipelines' layouts). */
static qboolean Prepare( ralWebGpuSmaa_t *t, ralWebGpuResourceLayer_t *r, uintptr_t scene, uint32_t width, uint32_t height ) {
	ralWebGpuWriteReceipt_t wr;
	if ( t->width == width && t->height == height && t->sceneColor == scene && t->groups[SMAA_RESOLVE][3] ) return qtrue;
	DestroyGroups( t );
	if ( t->width != width || t->height != height || !t->textures[TEX_OUTPUT] ) {
		for ( int i = TEX_EDGES; i <= TEX_OUTPUT; ++i ) {
			if ( t->textures[i] ) RalWebGpu_DestroyResource( r, t->textures[i], &t->textureReceipts[i] );
			t->textures[i] = NULL;
		}
		const uint32_t usage = RAL_WEBGPU_TEXTURE_SAMPLED | RAL_WEBGPU_TEXTURE_RENDER_ATTACHMENT;
		if ( !Texture( r, t, TEX_EDGES, RAL_FORMAT_R8G8_UNORM, 2u, width, height, usage )
				|| !Texture( r, t, TEX_BLEND, RAL_FORMAT_R8G8B8A8_UNORM, 4u, width, height, usage )
				|| !Texture( r, t, TEX_OUTPUT, RAL_FORMAT_R16G16B16A16_SFLOAT, 8u, width, height, usage ) ) FAIL( 50u );
		t->width = width; t->height = height;
	}
	{
		const float metrics[4] = { 1.0f / (float)width, 1.0f / (float)height, (float)width, (float)height };
		if ( !RalWebGpu_WriteBuffer( r, t->buffers[BUF_METRICS], &t->bufferReceipts[BUF_METRICS], 0u, metrics, sizeof( metrics ), &wr ) )
			FAIL( 51u );
	}
	const ralWebGpuBrowserBindResource_t metrics = { 0u, RAL_WEBGPU_BROWSER_BIND_RESOURCE_BUFFER,
		t->bufferReceipts[BUF_METRICS].resourceIdentity, 0u, 16u };
	const uintptr_t pointS = t->pointReceipt.resourceIdentity, linearS = t->linearReceipt.resourceIdentity;
	/* Samplers as in the Vulkan and Metal renderers: colour linear, edges point, blend linear, area linear, search point. */
	const ralWebGpuBrowserBindResource_t groups[SMAA_PASS_COUNT][GROUP_COUNT][2] = {
		{ { TexAt( 0u, scene ), SamplerAt( linearS ) }, { { 0 } }, { { 0 } }, { metrics } },
		{ { TexAt( 0u, t->textureReceipts[TEX_EDGES].resourceIdentity ), SamplerAt( pointS ) },
		  { TexAt( 0u, t->textureReceipts[TEX_AREA].resourceIdentity ), SamplerAt( linearS ) },
		  { TexAt( 0u, t->textureReceipts[TEX_SEARCH].resourceIdentity ), SamplerAt( pointS ) }, { metrics } },
		{ { TexAt( 0u, scene ), SamplerAt( linearS ) },
		  { TexAt( 0u, t->textureReceipts[TEX_BLEND].resourceIdentity ), SamplerAt( linearS ) }, { { 0 } }, { metrics } } };
	for ( int p = 0; p < SMAA_PASS_COUNT; ++p )
		for ( int g = 0; g < GROUP_COUNT; ++g ) {
			const uint32_t count = g == 3 ? 1u : ( groups[p][g][0].resourceIdentity ? 2u : 0u );
			if ( !RalWebGpu_BrowserBridgeCreateBindGroup( t->bridge, t->pipelineReceipts[p].bindGroupLayoutIdentities[g],
					count ? groups[p][g] : NULL, count, &t->groups[p][g] ) ) FAIL( 60u + 4u * (uint32_t)p + (uint32_t)g );
		}
	t->sceneColor = scene;
	return qtrue;
}

qboolean RalWebGpu_SmaaSetQuality( ralWebGpuSmaa_t *t, ralWebGpuRuntimeReceipt_t *rr, int quality ) {
	if ( !t || !rr ) return qfalse;
	quality = quality >= 1 && quality <= 4 ? quality : 3;
	if ( quality == t->quality ) return qtrue;
	/* The presets are pipeline overrides: rebuild the three pipelines and their groups (between frames). */
	DestroyGroups( t ); DestroyPipelines( t );
	t->quality = quality;
	return Pipelines( t, rr );
}

qboolean RalWebGpu_SmaaRecord( ralWebGpuSmaa_t *t, const ralWebGpuRuntimeReceipt_t *rr, ralWebGpuCommand_t *command,
		ralWebGpuCommandReceipt_t *recording, uintptr_t scene, uint32_t width, uint32_t height, uintptr_t *outResolved ) {
	ralWebGpuResourceLayer_t *r;
	ralWebGpuCommandReceipt_t next;
	if ( !t || !rr || !command || !recording || !scene || !width || !height || !outResolved ) FAIL( 1u );
	r = RalWebGpu_RuntimeResources( t->runtime, rr );
	if ( !r || !Prepare( t, r, scene, width, height ) ) return qfalse;
	const int targets[SMAA_PASS_COUNT] = { TEX_EDGES, TEX_BLEND, TEX_OUTPUT };
	for ( int p = 0; p < SMAA_PASS_COUNT; ++p ) {
		ralWebGpuPassDesc_t pass;
		ralWebGpuIndexedDraw_t draw;
		memset( &pass, 0, sizeof( pass ) );
		pass.kind = RAL_WEBGPU_PASS_RENDER; pass.targetIdentity = t->textureReceipts[targets[p]].resourceIdentity;
		pass.colorLoadOp = RAL_WEBGPU_LOAD_CLEAR; pass.depthEnabled = qfalse;
		if ( !RalWebGpu_CommandNextPass( command, recording, &pass, &next ) ) FAIL( 80u + (uint32_t)p );
		*recording = next;
		memset( &draw, 0, sizeof( draw ) );
		draw.kind = RAL_WEBGPU_DRAW_EFFECT;
		draw.pipelineIdentity = t->pipelineReceipts[p].pipelineIdentity;
		draw.vertexBufferIdentity = t->bufferReceipts[BUF_VERTEX].resourceIdentity;
		draw.indexBufferIdentity = t->bufferReceipts[BUF_INDEX].resourceIdentity;
		draw.indexCount = 3u; draw.instanceCount = 1u; draw.contentDigest = t->generation + 1u + (uint64_t)p;
		draw.bindGroupCount = GROUP_COUNT;
		for ( int g = 0; g < GROUP_COUNT; ++g ) draw.bindGroupIdentities[g] = t->groups[p][g];
		if ( !RalWebGpu_CommandRecordIndexedDraw( command, recording, &draw, &next ) ) FAIL( 90u + (uint32_t)p );
		*recording = next;
	}
	*outResolved = t->textureReceipts[TEX_OUTPUT].resourceIdentity;
	return qtrue;
}
