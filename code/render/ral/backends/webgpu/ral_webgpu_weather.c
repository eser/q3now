// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "ral_webgpu_weather.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
	float v[65][4];
} weatherFrame_t;
typedef struct {
	float positionLife[4], velocityFamily[4];
} weatherParticle_t;

struct ralWebGpuWeather_s {
	ralWebGpuRuntime_t		  *runtime;
	ralWebGpuBrowserBridge_t  *bridge;
	ralWebGpuCommand_t		  *command;
	ralWebGpuPipeline_t		  *computePipeline, *renderPipeline;
	ralWebGpuPipelineReceipt_t computeReceipt, renderReceipt;
	ralWebGpuResource_t		  *frame, *pools[2], *vertices, *indices;
	ralWebGpuResourceReceipt_t frameReceipt, poolReceipts[2], vertexReceipt, indexReceipt;
	uintptr_t				   computeBindGroups[2], renderBindGroups[2];
	uint64_t				   generation;
	float					   timeline;
	float					   pendingTimeline;
	uint32_t				   pendingWritePool;
	qboolean				   parity, active;
	ralWebGpuWeatherReceipt_t  inFlight;
};

static uint32_t s_weatherFailureStage;
uint32_t		RalWebGpu_WeatherLastFailureStage( void )
{
	return s_weatherFailureStage;
}

static const char s_computeWgsl[] =
	"struct F{v:array<vec4<f32>,65>};struct P{p:vec4<f32>,v:vec4<f32>};"
	"struct B{p:array<P>};@group(0) @binding(0)var<uniform>f:F;"
	"@group(0) @binding(1)var<storage,read>a:B;@group(0) @binding(2)var<storage,read_write>b:B;"
	"fn h(x:u32)->f32{var n=x*747796405u+2891336453u;n=((n>>((n>>28u)+4u))^n)*277803737u;"
	"return f32((n>>22u)^n)/4294967295.0;}"
	"fn effect(i:u32)->u32{let count=u32(f.v[16].z);for(var k=0u;k<8u;k++){if(k>=count){break;}"
	"let q=17u+k*6u;if(f32(i)>=f.v[q+4u].x&&f32(i)<f.v[q+4u].y){return k;}}return 0xffffffffu;}"
	"@compute @workgroup_size(64)fn main(@builtin(global_invocation_id)i:vec3<u32>){"
	"let n=u32(f.v[7].z);if(i.x>=n){return;}let precip=u32(f.v[7].w);let local=i.x>=precip;"
	"let e=effect(i.x);var p=a.p[i.x];let lo=f.v[8].xyz;let hi=f.v[9].xyz;"
	"if(local&&e==0xffffffffu){p.p.w=0.0;b.p[i.x]=p;return;}"
	"let expected=select(0u,16u+e,local);let stale=select(u32(p.v.w+.5)>=16u,u32(p.v.w+.5)!=expected,local);"
	"let fresh=p.p.w<=0.0||stale||(!local&&(any(p.p.xyz<lo)||any(p.p.xyz>hi)));"
	"if(fresh){if(local){let q=17u+e*6u;let seed=u32(f.v[q+5u].w)+i.x*13u;"
	"let r=max(f.v[q].w,f.v[q+5u].z);p.p=vec4<f32>(f.v[q].xyz+(vec3<f32>(h(seed),h(seed+1u),h(seed+2u))*2.0-1.0)*r,"
	"max(.05,f.v[q+5u].x+(h(seed+3u)*2.0-1.0)*f.v[q+5u].y));"
	"let d=select(vec3<f32>(0.,0.,1.),normalize(f.v[q+1u].xyz),length(f.v[q+1u].xyz)>.0001);"
	"let j=(vec3<f32>(h(seed+4u),h(seed+5u),h(seed+6u))*2.0-1.0)*f.v[q+3u].y;"
	"p.v=vec4<f32>(d*f.v[q+3u].x+j+f.v[10].xyz*.1,f32(expected));}else{"
	"let seed=u32(f.v[12].z)+i.x*13u;p.p=vec4<f32>(mix(lo,hi,vec3<f32>(h(seed),h(seed+1u),h(seed+2u))),1.0);"
	"let w=array<f32,5>(f.v[11].x,f.v[11].y,f.v[11].z,f.v[11].w,f.v[12].y);let total=w[0]+w[1]+w[2]+w[3]+w[4];"
	"let r=h(seed+3u)*total;var acc=0.0;var fam=0.0;for(var k=0u;k<5u;k++){acc+=w[k];if(r>acc){fam=f32(k+1u);}}"
	"p.v=vec4<f32>(f.v[10].xyz+vec3<f32>((h(seed+4u)-.5)*30.0,(h(seed+5u)-.5)*30.0,-180.0-260.0*h(seed+6u)),fam);"
	"if(fam==1.0){p.v=vec4<f32>(p.v.xy,-45.0,p.v.w);}if(fam==3.0){p.v=vec4<f32>(p.v.xy,-300.0,p.v.w);}if(fam==4.0){p.v=vec4<f32>(p.v.xy,-25.0,p.v.w);}}}"
	"let dt=clamp(f.v[7].x,0.0,.1);if(local){let q=17u+e*6u;let vz=p.v.z-800.0*f.v[q+3u].z*dt;"
	"p.v=vec4<f32>(p.v.xy,vz,p.v.w);p.v=vec4<f32>(p.v.xyz*max(0.0,1.0-f.v[q+3u].w*dt),p.v.w);p.p=vec4<f32>(p.p.xyz+p.v.xyz*dt,p.p.w-dt);}else{"
	"p.v=vec4<f32>(p.v.xyz+f.v[10].xyz*f.v[10].w*dt,p.v.w);p.p=vec4<f32>(p.p.xyz+p.v.xyz*dt,p.p.w);if(distance(p.p.xyz,f.v[6].xyz)>f.v[12].x){p.p.w=0.0;}}b.p["
	"i.x]=p;}";

static const char s_vertexWgsl[] =
	"struct F{v:array<vec4<f32>,65>};struct P{p:vec4<f32>,v:vec4<f32>};struct B{p:array<P>};"
	"struct O{@builtin(position)p:vec4<f32>,@location(0)c:vec4<f32>,@location(1)uv:vec2<f32>};"
	"@group(0) @binding(0)var<uniform>f:F;@group(0) @binding(1)var<storage,read>b:B;"
	"@vertex fn main(@builtin(vertex_index)vi:u32,@builtin(instance_index)ii:u32)->O{"
	"let q=array<vec2<f32>,6>(vec2(-1.,-1.),vec2(1.,-1.),vec2(1.,1.),vec2(-1.,-1.),vec2(1.,1.),vec2(-1.,1.));"
	"let x=b.p[ii];let fam=u32(x.v.w+.5);var size=3.0;var color=vec4<f32>(.75,.85,1.,.72);"
	"if(fam>=16u){let e=fam-16u;let b=17u+e*6u;let life=max(.05,f.v[b+5u].x+f.v[b+5u].y);"
	"let age=1.0-clamp(x.p.w/life,0.0,1.0);size=mix(f.v[b+4u].z,f.v[b+4u].w,age);let c=f.v[b+2u];color=vec4(c.rgb,c.a*f.v[b+1u].w);}"
	"else{let c=select(vec3(.75,.85,1.),vec3(.85,.9,1.),fam==1u);color=vec4(c,color.a);}"
	"let world=x.p.xyz+f.v[4].xyz*q[vi].x*size+f.v[5].xyz*q[vi].y*size;var o:O;"
	"o.p=f.v[0]*world.x+f.v[1]*world.y+f.v[2]*world.z+f.v[3];"
	"let light=clamp(f.v[15].xyz+vec3(f.v[13].w*(1.0-f.v[15].w*f.v[16].x)+f.v[16].y),vec3(.05),vec3(4.));"
	"var rgb=max(color.rgb*light,vec3(0.0))*f.v[4].w;let l=dot(rgb,vec3(.2126,.7152,.0722));"
	"if(l>0.0&&l<f.v[6].w&&f.v[5].w!=1.0){let curved=pow(l/f.v[6].w,f.v[5].w)*f.v[6].w;rgb*=curved/l;}"
	"o.c=vec4(rgb,color.a);o.uv=q[vi]*.5+.5;return o;}";
static const char s_fragmentWgsl[] =
	"@fragment fn main(@location(0)c:vec4<f32>,@location(1)uv:vec2<f32>)->@location(0)vec4<f32>{"
	"let a=max(0.0,1.0-length(uv*2.0-1.0));return vec4(c.rgb,c.a*a);}";

static qboolean Artifact( ralShaderArtifactAbi_t *a, const char *s )
{
	memset( a, 0, sizeof( *a ) );
	a->target	 = RAL_SHADER_ARTIFACT_WGSL;
	a->byteCount = (uint32_t)strlen( s );
	return Ral_ShaderArtifactDigest( s, a->byteCount, &a->digest ) == ralSuccess;
}

static qboolean Module( ralShaderModuleAbi_t *m, uint32_t stage, const char *source, uint64_t generation )
{
	uint32_t spirv[4] = { 0x07230203u };
	memset( m, 0, sizeof( *m ) );
	m->stage										  = stage;
	m->entryPoint									  = "main";
	m->sourceDigest.lane0							  = generation;
	m->sourceDigest.lane1							  = generation + 1u;
	m->artifacts[RAL_SHADER_ARTIFACT_SPIRV].target	  = RAL_SHADER_ARTIFACT_SPIRV;
	m->artifacts[RAL_SHADER_ARTIFACT_SPIRV].byteCount = sizeof( spirv );
	m->artifacts[RAL_SHADER_ARTIFACT_MSL].target	  = RAL_SHADER_ARTIFACT_MSL;
	return Ral_ShaderArtifactDigest( spirv, sizeof( spirv ), &m->artifacts[RAL_SHADER_ARTIFACT_SPIRV].digest ) ==
			   ralSuccess &&
		   Artifact( &m->artifacts[RAL_SHADER_ARTIFACT_WGSL], source );
}

static qboolean CreatePipelines( ralWebGpuWeather_t *w, const ralWebGpuRuntimeReceipt_t *rr )
{
	uint32_t						spirv[4] = { 0x07230203u };
	ralShaderModuleAbi_t			cm, gm[2];
	ralShaderBindingAbi_t			cb[3], gb[2];
	ralShaderAbiManifest_t			manifest;
	ralShaderVariantAbi_t			variant;
	ralWebGpuWgslModule_t			wgsl[2];
	ralWebGpuPipelineCreateInfo_t	info;
	ralComputePipelineCreateInfo_t	compute;
	ralGraphicsPipelineCreateInfo_t graphics;
	ralVertexBinding_t				dummyBinding;
	ralColorBlendAttachment_t		blend;
	const ralBindGroupLayout_t	   *layouts[1] = { (const ralBindGroupLayout_t *)(uintptr_t)1u };
	(void)spirv;
	s_weatherFailureStage = 21u;
	if ( !Module( &cm, RAL_STAGE_COMPUTE, s_computeWgsl, w->generation ) )
		return qfalse;
	memset( cb, 0, sizeof( cb ) );
	cb[0] = (ralShaderBindingAbi_t){
		0, 0, RAL_SHADER_BIND_UNIFORM_BUFFER, 1,	 RAL_STAGE_COMPUTE, sizeof( weatherFrame_t ),
		0, 0, RAL_FORMAT_UNDEFINED,			  qfalse };
	cb[1] = (ralShaderBindingAbi_t){ 0,
									 1,
									 RAL_SHADER_BIND_STORAGE_BUFFER_READ,
									 1,
									 RAL_STAGE_COMPUTE,
									 sizeof( weatherParticle_t ) * RAL_WEBGPU_WEATHER_PARTICLE_CAPACITY,
									 0,
									 0,
									 RAL_FORMAT_UNDEFINED,
									 qfalse };
	cb[2] = (ralShaderBindingAbi_t){ 0,
									 2,
									 RAL_SHADER_BIND_STORAGE_BUFFER_READ_WRITE,
									 1,
									 RAL_STAGE_COMPUTE,
									 sizeof( weatherParticle_t ) * RAL_WEBGPU_WEATHER_PARTICLE_CAPACITY,
									 0,
									 0,
									 RAL_FORMAT_UNDEFINED,
									 qfalse };
	memset( &manifest, 0, sizeof( manifest ) );
	manifest.schemaVersion = RAL_SHADER_ABI_SCHEMA_VERSION;
	manifest.generation	   = w->generation;
	manifest.pipelineKind  = RAL_SHADER_PIPELINE_COMPUTE;
	manifest.modules	   = &cm;
	manifest.moduleCount   = 1;
	manifest.bindings	   = cb;
	manifest.bindingCount  = 3;
	memset( &compute, 0, sizeof( compute ) );
	compute.bindGroupLayouts	= layouts;
	compute.numBindGroupLayouts = 1;
	memset( &variant, 0, sizeof( variant ) );
	variant.artifactTarget = RAL_SHADER_ARTIFACT_WGSL;
	variant.generation	   = w->generation;
	s_weatherFailureStage  = 22u;
	if ( Ral_ComputePipelineSemanticDigest( &compute, &variant.semanticStateDigest ) != ralSuccess )
		return qfalse;
	wgsl[0] = (ralWebGpuWgslModule_t){ s_computeWgsl, (uint32_t)sizeof( s_computeWgsl ) - 1u };
	memset( &info, 0, sizeof( info ) );
	info.manifest		  = &manifest;
	info.variant		  = &variant;
	info.modules		  = wgsl;
	info.moduleCount	  = 1;
	info.computeState	  = &compute;
	info.generation		  = w->generation;
	s_weatherFailureStage = 23u;
	if ( !RalWebGpu_BrowserBridgeApplyPipelineInfo( w->bridge, &info ) )
		return qfalse;
	s_weatherFailureStage = 24u;
	if ( !RalWebGpu_RuntimeCreatePipeline( w->runtime, rr, &info, &w->computePipeline, &w->computeReceipt ) )
		return qfalse;
	s_weatherFailureStage = 25u;
	if ( !RalWebGpu_RuntimeGetReceipt( w->runtime, (ralWebGpuRuntimeReceipt_t *)rr ) )
		return qfalse;
	s_weatherFailureStage = 26u;
	if ( !Module( &gm[0], RAL_STAGE_VERTEX, s_vertexWgsl, w->generation + 2u ) ||
		 !Module( &gm[1], RAL_STAGE_FRAGMENT, s_fragmentWgsl, w->generation + 4u ) )
		return qfalse;
	memset( gb, 0, sizeof( gb ) );
	gb[0] = (ralShaderBindingAbi_t){
		0, 0, RAL_SHADER_BIND_UNIFORM_BUFFER, 1,	 RAL_STAGE_VERTEX, sizeof( weatherFrame_t ),
		0, 0, RAL_FORMAT_UNDEFINED,			  qfalse };
	gb[1] = (ralShaderBindingAbi_t){ 0,
									 1,
									 RAL_SHADER_BIND_STORAGE_BUFFER_READ,
									 1,
									 RAL_STAGE_VERTEX,
									 sizeof( weatherParticle_t ) * RAL_WEBGPU_WEATHER_PARTICLE_CAPACITY,
									 0,
									 0,
									 RAL_FORMAT_UNDEFINED,
									 qfalse };
	memset( &manifest, 0, sizeof( manifest ) );
	manifest.schemaVersion = RAL_SHADER_ABI_SCHEMA_VERSION;
	manifest.generation	   = w->generation + 2u;
	manifest.pipelineKind  = RAL_SHADER_PIPELINE_GRAPHICS;
	manifest.modules	   = gm;
	manifest.moduleCount   = 2;
	manifest.bindings	   = gb;
	manifest.bindingCount  = 2;
	memset( &blend, 0, sizeof( blend ) );
	blend.blendEnable		= qtrue;
	blend.srcColor			= RAL_BLEND_SRC_ALPHA;
	blend.dstColor			= RAL_BLEND_ONE_MINUS_SRC_ALPHA;
	blend.colorOp			= RAL_BLEND_OP_ADD;
	blend.srcAlpha			= RAL_BLEND_ONE;
	blend.dstAlpha			= RAL_BLEND_ONE_MINUS_SRC_ALPHA;
	blend.alphaOp			= RAL_BLEND_OP_ADD;
	blend.writeMask			= RAL_COLOR_WRITE_ALL;
	blend.writeMaskExplicit = qtrue;
	memset( &dummyBinding, 0, sizeof( dummyBinding ) );
	dummyBinding.binding   = 0;
	dummyBinding.stride	   = sizeof( float );
	dummyBinding.inputRate = RAL_VERTEX_INPUT_PER_VERTEX;
	memset( &graphics, 0, sizeof( graphics ) );
	graphics.vertexBindings		 = &dummyBinding;
	graphics.numVertexBindings	 = 1;
	graphics.topology			 = RAL_TOPOLOGY_TRIANGLE_LIST;
	graphics.raster.polygonMode	 = RAL_POLYGON_FILL;
	graphics.raster.lineWidth	 = 1.0f;
	graphics.colorBlends		 = &blend;
	graphics.numColorBlends		 = 1;
	graphics.colorFormats[0]	 = RAL_FORMAT_B8G8R8A8_UNORM;
	graphics.numColorFormats	 = 1;
	graphics.sampleCount		 = 1;
	graphics.bindGroupLayouts	 = layouts;
	graphics.numBindGroupLayouts = 1;
	memset( &variant, 0, sizeof( variant ) );
	variant.artifactTarget = RAL_SHADER_ARTIFACT_WGSL;
	variant.generation	   = w->generation + 2u;
	s_weatherFailureStage  = 27u;
	if ( Ral_GraphicsPipelineSemanticDigest( &graphics, &variant.semanticStateDigest ) != ralSuccess )
		return qfalse;
	wgsl[0] = (ralWebGpuWgslModule_t){ s_vertexWgsl, (uint32_t)sizeof( s_vertexWgsl ) - 1u };
	wgsl[1] = (ralWebGpuWgslModule_t){ s_fragmentWgsl, (uint32_t)sizeof( s_fragmentWgsl ) - 1u };
	memset( &info, 0, sizeof( info ) );
	info.manifest		  = &manifest;
	info.variant		  = &variant;
	info.modules		  = wgsl;
	info.moduleCount	  = 2;
	info.graphicsState	  = &graphics;
	info.generation		  = w->generation + 2u;
	s_weatherFailureStage = 28u;
	if ( !RalWebGpu_BrowserBridgeApplyPipelineInfo( w->bridge, &info ) )
		return qfalse;
	s_weatherFailureStage = 29u;
	if ( !RalWebGpu_RuntimeCreatePipeline( w->runtime, rr, &info, &w->renderPipeline, &w->renderReceipt ) )
		return qfalse;
	s_weatherFailureStage = 30u;
	return RalWebGpu_RuntimeGetReceipt( w->runtime, (ralWebGpuRuntimeReceipt_t *)rr );
}

static qboolean CreateResources( ralWebGpuWeather_t *w, const ralWebGpuRuntimeReceipt_t *rr )
{
	ralWebGpuResourceLayer_t	  *r = RalWebGpu_RuntimeResources( w->runtime, rr );
	ralWebGpuBufferDesc_t		   d;
	ralWebGpuBrowserBindResource_t e[3];
	ralWebGpuWriteReceipt_t		   wr;
	uint32_t					   indices[6] = { 0, 1, 2, 3, 4, 5 };
	float						   vertex	  = 0.0f;
	if ( !r )
		return qfalse;
	memset( &d, 0, sizeof( d ) );
	d.memoryClass = RAL_ALLOCATION_DEVICE_LOCAL;
	d.size		  = sizeof( weatherFrame_t );
	d.usage		  = RAL_WEBGPU_BUFFER_UNIFORM | RAL_WEBGPU_BUFFER_COPY_DESTINATION;
	if ( !RalWebGpu_CreateBuffer( r, &d, &w->frame, &w->frameReceipt ) )
		return qfalse;
	d.size	= sizeof( weatherParticle_t ) * RAL_WEBGPU_WEATHER_PARTICLE_CAPACITY;
	d.usage = RAL_WEBGPU_BUFFER_STORAGE | RAL_WEBGPU_BUFFER_COPY_DESTINATION;
	for ( uint32_t i = 0; i < 2; i++ )
		if ( !RalWebGpu_CreateBuffer( r, &d, &w->pools[i], &w->poolReceipts[i] ) )
			return qfalse;
	d.size	= sizeof( vertex );
	d.usage = RAL_WEBGPU_BUFFER_VERTEX | RAL_WEBGPU_BUFFER_COPY_DESTINATION;
	if ( !RalWebGpu_CreateBuffer( r, &d, &w->vertices, &w->vertexReceipt ) ||
		 !RalWebGpu_WriteBuffer( r, w->vertices, &w->vertexReceipt, 0, &vertex, sizeof( vertex ), &wr ) )
		return qfalse;
	d.size	= sizeof( indices );
	d.usage = RAL_WEBGPU_BUFFER_INDEX | RAL_WEBGPU_BUFFER_COPY_DESTINATION;
	if ( !RalWebGpu_CreateBuffer( r, &d, &w->indices, &w->indexReceipt ) ||
		 !RalWebGpu_WriteBuffer( r, w->indices, &w->indexReceipt, 0, indices, sizeof( indices ), &wr ) )
		return qfalse;
	for ( uint32_t i = 0; i < 2; i++ ) {
		memset( e, 0, sizeof( e ) );
		e[0] = (ralWebGpuBrowserBindResource_t){ 0, RAL_WEBGPU_BROWSER_BIND_RESOURCE_BUFFER,
												 w->frameReceipt.resourceIdentity, 0, sizeof( weatherFrame_t ) };
		e[1] = (ralWebGpuBrowserBindResource_t){ 1, RAL_WEBGPU_BROWSER_BIND_RESOURCE_BUFFER,
												 w->poolReceipts[i].resourceIdentity, 0, w->poolReceipts[i].byteSize };
		e[2] = (ralWebGpuBrowserBindResource_t){ 2, RAL_WEBGPU_BROWSER_BIND_RESOURCE_BUFFER,
												 w->poolReceipts[1u - i].resourceIdentity, 0,
												 w->poolReceipts[1u - i].byteSize };
		if ( !RalWebGpu_BrowserBridgeCreateBindGroup( w->bridge, w->computeReceipt.bindGroupLayoutIdentities[0], e, 3,
													  &w->computeBindGroups[i] ) )
			return qfalse;
		memset( e, 0, sizeof( e ) );
		e[0] = (ralWebGpuBrowserBindResource_t){ 0, RAL_WEBGPU_BROWSER_BIND_RESOURCE_BUFFER,
												 w->frameReceipt.resourceIdentity, 0, sizeof( weatherFrame_t ) };
		e[1] = (ralWebGpuBrowserBindResource_t){ 1, RAL_WEBGPU_BROWSER_BIND_RESOURCE_BUFFER,
												 w->poolReceipts[i].resourceIdentity, 0, w->poolReceipts[i].byteSize };
		if ( !RalWebGpu_BrowserBridgeCreateBindGroup( w->bridge, w->renderReceipt.bindGroupLayoutIdentities[0], e, 2,
													  &w->renderBindGroups[i] ) )
			return qfalse;
	}
	return qtrue;
}

qboolean RalWebGpu_WeatherCreate( ralWebGpuRuntime_t *runtime, ralWebGpuRuntimeReceipt_t *rr,
								  ralWebGpuBrowserBridge_t *bridge, uint64_t generation, ralWebGpuWeather_t **out )
{
	ralWebGpuWeather_t *w;
	s_weatherFailureStage = 1u;
	if ( !runtime || !rr || !bridge || !generation || !out || *out )
		return qfalse;
	w = calloc( 1, sizeof( *w ) );
	if ( !w )
		return qfalse;
	w->runtime	  = runtime;
	w->bridge	  = bridge;
	w->generation = generation;
	w->command	  = RalWebGpu_RuntimeCommand( runtime, rr );
	if ( !w->command ) {
		RalWebGpu_WeatherDestroy( w, rr );
		return qfalse;
	}
	s_weatherFailureStage = 2u;
	if ( !CreatePipelines( w, rr ) ) {
		RalWebGpu_WeatherDestroy( w, rr );
		return qfalse;
	}
	s_weatherFailureStage = 3u;
	if ( !CreateResources( w, rr ) ) {
		RalWebGpu_WeatherDestroy( w, rr );
		return qfalse;
	}
	s_weatherFailureStage = 0u;
	*out				  = w;
	return qtrue;
}

void RalWebGpu_WeatherDestroy( ralWebGpuWeather_t *w, const ralWebGpuRuntimeReceipt_t *rr )
{
	ralWebGpuResourceLayer_t *r;
	if ( !w )
		return;
	for ( uint32_t i = 2; i > 0; i-- ) {
		if ( w->renderBindGroups[i - 1] )
			RalWebGpu_BrowserBridgeDestroyBindGroup( w->bridge, w->renderBindGroups[i - 1] );
		if ( w->computeBindGroups[i - 1] )
			RalWebGpu_BrowserBridgeDestroyBindGroup( w->bridge, w->computeBindGroups[i - 1] );
	}
	r = rr ? RalWebGpu_RuntimeResources( w->runtime, rr ) : NULL;
	if ( r ) {
		if ( w->indices )
			RalWebGpu_DestroyResource( r, w->indices, &w->indexReceipt );
		if ( w->vertices )
			RalWebGpu_DestroyResource( r, w->vertices, &w->vertexReceipt );
		for ( uint32_t i = 2; i > 0; i-- )
			if ( w->pools[i - 1] )
				RalWebGpu_DestroyResource( r, w->pools[i - 1], &w->poolReceipts[i - 1] );
		if ( w->frame )
			RalWebGpu_DestroyResource( r, w->frame, &w->frameReceipt );
	}
	if ( rr && w->renderPipeline )
		RalWebGpu_RuntimeDestroyPipeline( w->runtime, w->renderPipeline, &w->renderReceipt );
	if ( rr && w->computePipeline )
		RalWebGpu_RuntimeDestroyPipeline( w->runtime, w->computePipeline, &w->computeReceipt );
	free( w );
}

qboolean RalWebGpu_WeatherPlan( const renderSubmissionState_t *frontend, const ralAtmospherePlanReceipt_t *atmosphere,
								ralAtmosphereWeatherReceipt_t *out )
{
	renderAtmosphereSnapshot_t				 s;
	renderAtmosphereEffectWorkloadSnapshot_t effects;
	ralAtmosphereWeatherRequest_t			 q;
	const atmosphereEmitter_t				*e;
	if ( !frontend || !atmosphere || !out || !RenderSubmission_AtmosphereSnapshot( frontend, &s, &e ) )
		return qfalse;
	(void)e;
	memset( &q, 0, sizeof( q ) );
	q.schemaVersion	 = RAL_ATMOSPHERE_WEATHER_SCHEMA_VERSION;
	q.tier			 = atmosphere->selectedTier;
	q.maxParticles	 = RAL_WEBGPU_WEATHER_PARTICLE_CAPACITY;
	q.enabled		 = s.active && frontend->sceneRendered;
	q.indoorExposure = s.state.indoorExposure;
	if ( !RenderSubmission_AtmosphereEffectWorkloadSnapshot( frontend, q.maxParticles, &effects ) )
		return qfalse;
	q.semanticEmitterCount	= effects.admittedEmitterCount;
	q.semanticParticleCount = effects.admittedParticleCount;
	memcpy( q.precipitation, s.state.precipitation, sizeof( q.precipitation ) );
	return Ral_AtmospherePlanWeather( &q, out );
}

static void Matrix( const renderWorldSnapshot_t *v, float m[16] )
{
	float px = 1.0f / tanf( v->fovX * 0.00872664625997164788f ), py = 1.0f / tanf( v->fovY * 0.00872664625997164788f );
	memset( m, 0, 16 * sizeof( float ) );
	for ( uint32_t a = 0; a < 3; a++ ) {
		m[a * 4]	 = -v->viewAxis[1][a] * px;
		m[a * 4 + 1] = v->viewAxis[2][a] * py;
		m[a * 4 + 2] = v->viewAxis[0][a];
		m[a * 4 + 3] = v->viewAxis[0][a];
		m[12] += v->viewOrigin[a] * v->viewAxis[1][a] * px;
		m[13] -= v->viewOrigin[a] * v->viewAxis[2][a] * py;
		m[14] -= v->viewOrigin[a] * v->viewAxis[0][a];
		m[15] -= v->viewOrigin[a] * v->viewAxis[0][a];
	}
	m[14] -= 4.0f;
}

qboolean RalWebGpu_WeatherBegin( ralWebGpuWeather_t *w, const ralWebGpuRuntimeReceipt_t *rr,
								 const renderSubmissionState_t *frontend, const ralAtmosphereWeatherReceipt_t *plan,
								 const ralDisplayVisibilityPlan_t *visibility,
								 uint64_t frameGeneration, ralWebGpuWeatherReceipt_t *out )
{
	renderAtmosphereSnapshot_t				 s;
	renderAtmosphereEffectWorkloadSnapshot_t effects;
	renderAtmosphereEffectGpuWorkload_t		 gpuEffects[RENDER_SUBMISSION_MAX_ATMOSPHERE_EFFECT_WORKLOADS];
	renderWorldSnapshot_t					 v;
	const atmosphereEmitter_t				*e;
	weatherFrame_t							 f;
	ralWebGpuResourceLayer_t				*r;
	ralWebGpuWriteReceipt_t					 wr;
	ralWebGpuCommandReceipt_t				 rec, next, exe;
	ralWebGpuSubmissionReceipt_t			 sub;
	ralWebGpuComputeDispatch_t				 d;
	uint32_t								 read = w->parity ? 1u : 0u, write = 1u - read;
	if ( !w || !rr || !frontend || !plan || !visibility || !out || w->active
		 || plan->zeroWork || !frameGeneration
		 || !Ral_DisplayVisibilityPlanValid( visibility ) ||
		 !Ral_AtmosphereWeatherReceiptExact( plan, plan ) || !RenderSubmission_AtmosphereSnapshot( frontend, &s, &e ) ||
		 !RenderSubmission_ViewSnapshot( frontend, &v ) )
		return qfalse;
	(void)e;
	memset( &f, 0, sizeof( f ) );
	Matrix( &v, &f.v[0][0] );
	memcpy( f.v[5], v.viewAxis[2], 3 * sizeof( float ) );
	memcpy( f.v[4], v.viewAxis[1], 3 * sizeof( float ) );
	memcpy( f.v[6], v.viewOrigin, 3 * sizeof( float ) );
	f.v[4][3] = visibility->exposureScale;
	f.v[5][3] = visibility->shadowExponent;
	f.v[6][3] = visibility->shadowPivot;
	f.v[7][0] = w->timeline > 0 ? s.state.timelineSeconds - w->timeline : 1.0f / 60.0f;
	f.v[7][1] = s.state.timelineSeconds;
	f.v[7][2] = (float)plan->activeParticleCount;
	f.v[7][3] = (float)plan->precipitationParticleCount;
	memcpy( f.v[8], s.state.bounds, 3 * sizeof( float ) );
	memcpy( f.v[9], s.state.bounds + 3, 3 * sizeof( float ) );
	memcpy( f.v[10], s.state.wind, 3 * sizeof( float ) );
	f.v[10][3] = s.state.gustStrength;
	memcpy( f.v[11], s.state.precipitation, 4 * sizeof( float ) );
	f.v[12][0] = s.state.distance > 0 ? s.state.distance : 1024.0f;
	f.v[12][1] = s.state.precipitation[4];
	f.v[12][2] = (float)s.state.seed;
	memcpy( f.v[13], s.state.sunDirection, 3 * sizeof( float ) );
	f.v[13][3] = s.state.sunIntensity;
	memcpy( f.v[14], s.state.moonDirection, 3 * sizeof( float ) );
	f.v[14][3] = s.state.moonIntensity;
	memcpy( f.v[15], s.state.ambientColor, 3 * sizeof( float ) );
	f.v[15][3] = s.state.cloudCover;
	f.v[16][0] = s.state.cloudShadow;
	f.v[16][1] = s.state.lightning;
	f.v[16][2] = (float)plan->semanticEmitterCount;
	if ( !RenderSubmission_AtmosphereEffectGpuPayload( frontend, plan->semanticParticleCount,
													   plan->precipitationParticleCount, gpuEffects, &effects ) ||
		 effects.admittedEmitterCount != plan->semanticEmitterCount ||
		 effects.admittedParticleCount != plan->semanticParticleCount )
		return qfalse;
	memcpy( &f.v[17][0], gpuEffects, sizeof( gpuEffects ) );
	r = RalWebGpu_RuntimeResources( w->runtime, rr );
	if ( !r || !RalWebGpu_WriteBuffer( r, w->frame, &w->frameReceipt, 0, &f, sizeof( f ), &wr ) ||
		 !RalWebGpu_CommandBegin( w->command, RAL_WEBGPU_PASS_COMPUTE, 0, &rec ) )
		return qfalse;
	memset( &d, 0, sizeof( d ) );
	d.pipelineIdentity		 = w->computeReceipt.pipelineIdentity;
	d.bindGroupIdentities[0] = w->computeBindGroups[read];
	d.bindGroupCount		 = 1;
	d.groupCountX			 = ( plan->activeParticleCount + 63u ) / 64u;
	d.groupCountY = d.groupCountZ = 1;
	d.contentDigest				  = frameGeneration ^ plan->activeParticleCount;
	if ( !d.contentDigest )
		d.contentDigest = 1;
	if ( !RalWebGpu_CommandRecordComputeDispatch( w->command, &rec, &d, &next ) ) {
		RalWebGpu_CommandCancel( w->command, &rec );
		return qfalse;
	}
	rec = next;
	if ( !RalWebGpu_CommandEnd( w->command, &rec, &exe ) ) {
		RalWebGpu_CommandCancel( w->command, &rec );
		return qfalse;
	}
	if ( !RalWebGpu_CommandSubmit( w->command, &exe, &sub ) )
		return qfalse;
	memset( out, 0, sizeof( *out ) );
	out->schemaVersion			 = RAL_WEBGPU_WEATHER_SCHEMA_VERSION;
	out->backendType			 = RAL_BACKEND_WEBGPU;
	out->frameGeneration		 = frameGeneration;
	out->executorGeneration		 = w->generation;
	out->plan					 = *plan;
	out->displayVisibility	 = *visibility;
	out->command				 = exe;
	out->submission				 = sub;
	out->renderPipeline			 = w->renderReceipt;
	out->renderBindGroupIdentity = w->renderBindGroups[write];
	out->vertexBufferIdentity	 = w->vertexReceipt.resourceIdentity;
	out->indexBufferIdentity	 = w->indexReceipt.resourceIdentity;
	out->dispatchCount			 = 1;
	out->drawIndexCount			 = 6;
	out->ready					 = qtrue;
	w->pendingWritePool			 = write;
	w->pendingTimeline			 = s.state.timelineSeconds;
	w->inFlight					 = *out;
	w->active					 = qtrue;
	return RalWebGpu_WeatherReceiptExact( out, out );
}

static qboolean Valid( const ralWebGpuWeatherReceipt_t *r )
{
	return r && r->schemaVersion == RAL_WEBGPU_WEATHER_SCHEMA_VERSION && r->backendType == RAL_BACKEND_WEBGPU &&
		   r->frameGeneration && r->executorGeneration && Ral_AtmosphereWeatherReceiptExact( &r->plan, &r->plan ) &&
		   Ral_DisplayVisibilityPlanValid( &r->displayVisibility ) &&
		   !r->plan.zeroWork && RalWebGpu_CommandReceiptExact( &r->command, &r->command ) &&
		   RalWebGpu_SubmissionReceiptExact( &r->submission, &r->submission ) &&
		   RalWebGpu_PipelineReceiptExact( &r->renderPipeline, &r->renderPipeline ) &&
		   r->renderPipeline.kind == RAL_SHADER_PIPELINE_GRAPHICS && r->renderBindGroupIdentity &&
		   r->vertexBufferIdentity && r->indexBufferIdentity && r->dispatchCount == 1 && r->drawIndexCount == 6 &&
		   r->ready;
}
qboolean RalWebGpu_WeatherReceiptExact( const ralWebGpuWeatherReceipt_t *a, const ralWebGpuWeatherReceipt_t *b )
{
	return Valid( a ) && Valid( b ) && !memcmp( a, b, sizeof( *a ) );
}
ralWebGpuAsyncStatus_t RalWebGpu_WeatherPoll( ralWebGpuWeather_t *w, const ralWebGpuWeatherReceipt_t *r )
{
	ralWebGpuAsyncStatus_t s;
	if ( !w || !r || !w->active || !RalWebGpu_WeatherReceiptExact( r, &w->inFlight ) )
		return RAL_WEBGPU_ASYNC_FAILED;
	s = RalWebGpu_CommandPoll( w->command, &r->submission );
	if ( s == RAL_WEBGPU_ASYNC_READY ) {
		w->parity	= w->pendingWritePool ? qtrue : qfalse;
		w->timeline = w->pendingTimeline;
		w->active	= qfalse;
		memset( &w->inFlight, 0, sizeof( w->inFlight ) );
	}
	return s;
}
void RalWebGpu_WeatherReset( ralWebGpuWeather_t *w )
{
	if ( w && !w->active ) {
		w->parity	= qfalse;
		w->timeline = 0.0f;
	}
}
