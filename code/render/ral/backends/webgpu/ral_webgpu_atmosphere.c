// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "ral_webgpu_atmosphere.h"

#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

enum {
	WEBGPU_ATMOSPHERE_MEDIA,
	WEBGPU_ATMOSPHERE_LIT,
	WEBGPU_ATMOSPHERE_CLOUD,
	WEBGPU_ATMOSPHERE_INTEGRATED,
	WEBGPU_ATMOSPHERE_HISTORY_A,
	WEBGPU_ATMOSPHERE_HISTORY_B,
	WEBGPU_ATMOSPHERE_SECTION_COUNT,
	WEBGPU_ATMOSPHERE_PASS_COUNT = 4
};

typedef struct {
	uint32_t dimsStage[4];
	uint32_t bases0[4];
	uint32_t bases1[4];
	float boundsMin[4];
	float boundsMax[4];
	float climate[4];
	float lighting[4];
	float weather[4];
} atmosphereParams_t;

typedef struct {
	float originRadius[4];
	float extentShape[4];
	float albedoAnisotropy[4];
	float emissiveIntensity[4];
	float media[4];
} atmosphereVolume_t;

struct ralWebGpuAtmosphere_s {
	ralWebGpuRuntime_t *runtime;
	ralWebGpuCommand_t *command;
	ralWebGpuBrowserBridge_t *bridge;
	uint64_t generation;
	ralWebGpuPipeline_t *pipeline;
	ralWebGpuPipelineReceipt_t pipelineReceipt;
	ralWebGpuResource_t *arena;
	ralWebGpuResourceReceipt_t arenaReceipt;
	ralWebGpuResource_t *volumes;
	ralWebGpuResourceReceipt_t volumesReceipt;
	ralWebGpuResource_t *params[WEBGPU_ATMOSPHERE_PASS_COUNT];
	ralWebGpuResourceReceipt_t paramsReceipts[WEBGPU_ATMOSPHERE_PASS_COUNT];
	uintptr_t bindGroups[WEBGPU_ATMOSPHERE_PASS_COUNT];
	ralWebGpuAtmosphereReceipt_t inFlight;
	qboolean active;
	qboolean historyValid;
	qboolean historyParity;
};

static const char s_atmosphereComputeWgsl[] =
	"struct Params { dimsStage: vec4<u32>, bases0: vec4<u32>, bases1: vec4<u32>,"
	" boundsMin: vec4<f32>, boundsMax: vec4<f32>, climate: vec4<f32>,"
	" lighting: vec4<f32>, weather: vec4<f32> };"
	"struct Arena { v: array<vec4<f32>> };"
	"struct Volume { originRadius: vec4<f32>, extentShape: vec4<f32>,"
	" albedoAnisotropy: vec4<f32>, emissiveIntensity: vec4<f32>, media: vec4<f32> };"
	"struct Volumes { v: array<Volume> };"
	"@group(0) @binding(0) var<uniform> p: Params;"
	"@group(0) @binding(1) var<storage, read_write> arena: Arena;"
	"@group(0) @binding(2) var<storage, read> volumes: Volumes;"
	"fn hash3(q: vec3<f32>) -> f32 { let h=dot(q,vec3<f32>(12.9898,78.233,37.719));"
	" return fract(sin(h)*43758.5453); }"
	"@compute @workgroup_size(4,4,4) fn main(@builtin(global_invocation_id) c: vec3<u32>) {"
	" let dims=p.dimsStage.xyz; if(any(c>=dims)){return;}"
	" let i=(c.z*dims.y+c.y)*dims.x+c.x; let stage=p.dimsStage.w;"
	" let uvw=(vec3<f32>(c)+vec3<f32>(0.5))/vec3<f32>(dims);"
	" if(stage==0u){ var extinction=max(p.climate.x,0.0)*exp(-max(p.climate.y,0.0)*uvw.z);"
	" extinction+=p.weather.x*p.weather.y*0.00008; var scatter=vec3<f32>(0.62,0.70,0.78)*extinction;"
	" let world=mix(p.boundsMin.xyz,p.boundsMax.xyz,uvw);"
	" for(var n=0u;n<p.bases1.w;n++){let m=volumes.v[n];let d=world-m.originRadius.xyz;"
	" var w=1.0-smoothstep(m.originRadius.w*0.8,m.originRadius.w,length(d));"
	" if(m.extentShape.w>1.5){w=1.0-smoothstep(0.8,1.0,max(max(abs(d.x)/m.extentShape.x,abs(d.y)/m.extentShape.y),abs(d.z)/m.extentShape.z));}"
	" w=max(w,0.0);let e=max(m.media.x,0.0)*w;extinction+=e;scatter+=m.albedoAnisotropy.xyz*e+m.emissiveIntensity.xyz*m.emissiveIntensity.w*w;}"
	" arena.v[p.bases0.x+i]=vec4<f32>(scatter,extinction);return;}"
	" if(stage==1u){let media=arena.v[p.bases0.x+i];let cloudShadow=1.0-p.climate.w*p.climate.z;"
	" let light=max(p.lighting.x+p.lighting.y*cloudShadow+p.lighting.z,0.0);"
	" arena.v[p.bases0.y+i]=vec4<f32>(media.xyz*light,media.w);return;}"
	" if(stage==2u){let lit=arena.v[p.bases0.y+i];let layer=exp(-pow((uvw.z-0.72)/0.18,2.0));"
	" let erosion=mix(0.55,1.0,hash3(vec3<f32>(uvw.xy*32.0+p.weather.z,uvw.z*8.0)));"
	" let cloud=p.climate.z*layer*erosion;arena.v[p.bases0.z+i]=vec4<f32>(lit.xyz+vec3<f32>(cloud*(p.lighting.x+p.lighting.z)),lit.w+cloud*0.002);return;}"
	" let source=select(p.bases0.y,p.bases0.z,p.climate.z>0.0);let media=arena.v[source+i];"
	" let distance=mix(4.0,max(p.weather.w,4.01),uvw.z);let trans=exp(-media.w*distance);"
	" var current=vec4<f32>(media.xyz*(1.0-trans)/max(media.w,0.000001),trans);"
	" if(p.lighting.w>0.0){current=mix(current,arena.v[p.bases1.x+i],p.lighting.w);}"
	" arena.v[p.bases0.w+i]=current;arena.v[p.bases1.y+i]=current; }";

static qboolean ReceiptValid( const ralWebGpuAtmosphereReceipt_t *receipt ) {
	return receipt
		&& receipt->schemaVersion == RAL_WEBGPU_ATMOSPHERE_SCHEMA_VERSION
		&& receipt->backendType == RAL_BACKEND_WEBGPU
		&& receipt->frameGeneration && receipt->executorGeneration
		&& Ral_AtmospherePlanReceiptExact( &receipt->plan, &receipt->plan )
		&& receipt->plan.selectedTier == RAL_ATMOSPHERE_TIER_FULL
		&& RalWebGpu_CommandReceiptExact( &receipt->command, &receipt->command )
		&& RalWebGpu_SubmissionReceiptExact( &receipt->submission,
			&receipt->submission )
		&& receipt->dispatchCount == receipt->plan.computeDispatchCount
		&& receipt->froxelCount == receipt->plan.froxelCount
		&& receipt->ready == qtrue;
}

qboolean RalWebGpu_AtmosphereReceiptExact(
		const ralWebGpuAtmosphereReceipt_t *a,
		const ralWebGpuAtmosphereReceipt_t *b ) {
	return ReceiptValid( a ) && ReceiptValid( b )
		&& !memcmp( a, b, sizeof( *a ) );
}

static qboolean Artifact( ralShaderArtifactAbi_t *artifact,
		const void *bytes, uint32_t byteCount ) {
	memset( artifact, 0, sizeof( *artifact ) );
	artifact->target = RAL_SHADER_ARTIFACT_WGSL;
	artifact->byteCount = byteCount;
	return Ral_ShaderArtifactDigest( bytes, byteCount, &artifact->digest )
		== ralSuccess;
}

static qboolean CreatePipeline( ralWebGpuAtmosphere_t *atmosphere,
		const ralWebGpuRuntimeReceipt_t *runtimeReceipt ) {
	uint32_t spirv[4] = { 0x07230203u };
	ralShaderModuleAbi_t module;
	ralShaderBindingAbi_t bindings[3];
	ralShaderAbiManifest_t manifest;
	ralShaderVariantAbi_t variant;
	ralWebGpuWgslModule_t wgsl;
	ralComputePipelineCreateInfo_t compute;
	ralWebGpuPipelineCreateInfo_t info;
	const ralBindGroupLayout_t *layouts[1] = {
		(const ralBindGroupLayout_t *)(uintptr_t)1u };
	memset( &module, 0, sizeof( module ) ); module.stage = RAL_STAGE_COMPUTE;
	module.entryPoint = "main"; module.sourceDigest.lane0 = atmosphere->generation;
	module.sourceDigest.lane1 = atmosphere->generation + 1u;
	module.artifacts[RAL_SHADER_ARTIFACT_SPIRV].target = RAL_SHADER_ARTIFACT_SPIRV;
	module.artifacts[RAL_SHADER_ARTIFACT_SPIRV].byteCount = sizeof( spirv );
	module.artifacts[RAL_SHADER_ARTIFACT_MSL].target = RAL_SHADER_ARTIFACT_MSL;
	if ( Ral_ShaderArtifactDigest( spirv, sizeof( spirv ),
			&module.artifacts[RAL_SHADER_ARTIFACT_SPIRV].digest ) != ralSuccess
			|| !Artifact( &module.artifacts[RAL_SHADER_ARTIFACT_WGSL],
				s_atmosphereComputeWgsl,
				(uint32_t)sizeof( s_atmosphereComputeWgsl ) - 1u ) ) return qfalse;
	memset( bindings, 0, sizeof( bindings ) );
	bindings[0] = (ralShaderBindingAbi_t){ 0u, 0u,
		RAL_SHADER_BIND_UNIFORM_BUFFER, 1u, RAL_STAGE_COMPUTE,
		sizeof( atmosphereParams_t ), 0, 0, RAL_FORMAT_UNDEFINED, qfalse };
	bindings[1] = (ralShaderBindingAbi_t){ 0u, 1u,
		RAL_SHADER_BIND_STORAGE_BUFFER_READ_WRITE, 1u, RAL_STAGE_COMPUTE,
		(uint64_t)RAL_WEBGPU_ATMOSPHERE_FROXEL_CAPACITY
			* WEBGPU_ATMOSPHERE_SECTION_COUNT * 16u,
		0, 0, RAL_FORMAT_UNDEFINED, qfalse };
	bindings[2] = (ralShaderBindingAbi_t){ 0u, 2u,
		RAL_SHADER_BIND_STORAGE_BUFFER_READ, 1u, RAL_STAGE_COMPUTE,
		sizeof( atmosphereVolume_t ) * RAL_ATMOSPHERE_MAX_VOLUMES,
		0, 0, RAL_FORMAT_UNDEFINED, qfalse };
	memset( &manifest, 0, sizeof( manifest ) );
	manifest.schemaVersion = RAL_SHADER_ABI_SCHEMA_VERSION;
	manifest.generation = atmosphere->generation;
	manifest.pipelineKind = RAL_SHADER_PIPELINE_COMPUTE;
	manifest.modules = &module; manifest.moduleCount = 1u;
	manifest.bindings = bindings; manifest.bindingCount = 3u;
	memset( &compute, 0, sizeof( compute ) );
	compute.bindGroupLayouts = layouts; compute.numBindGroupLayouts = 1u;
	memset( &variant, 0, sizeof( variant ) );
	variant.artifactTarget = RAL_SHADER_ARTIFACT_WGSL;
	variant.generation = atmosphere->generation;
	if ( Ral_ComputePipelineSemanticDigest( &compute,
			&variant.semanticStateDigest ) != ralSuccess ) return qfalse;
	wgsl.code = s_atmosphereComputeWgsl;
	wgsl.byteCount = (uint32_t)sizeof( s_atmosphereComputeWgsl ) - 1u;
	memset( &info, 0, sizeof( info ) ); info.manifest = &manifest;
	info.variant = &variant; info.modules = &wgsl; info.moduleCount = 1u;
	info.computeState = &compute; info.generation = atmosphere->generation;
	if ( !RalWebGpu_BrowserBridgeApplyPipelineInfo( atmosphere->bridge, &info ) )
		return qfalse;
	return RalWebGpu_RuntimeCreatePipeline( atmosphere->runtime,
		runtimeReceipt, &info, &atmosphere->pipeline,
		&atmosphere->pipelineReceipt );
}

static qboolean CreateResources( ralWebGpuAtmosphere_t *atmosphere,
		const ralWebGpuRuntimeReceipt_t *runtimeReceipt ) {
	ralWebGpuResourceLayer_t *resources = RalWebGpu_RuntimeResources(
		atmosphere->runtime, runtimeReceipt );
	ralWebGpuBufferDesc_t desc;
	ralWebGpuBrowserBindResource_t entries[3];
	if ( !resources ) return qfalse;
	memset( &desc, 0, sizeof( desc ) );
	desc.size = (uint64_t)RAL_WEBGPU_ATMOSPHERE_FROXEL_CAPACITY
		* WEBGPU_ATMOSPHERE_SECTION_COUNT * 16u;
	desc.usage = RAL_WEBGPU_BUFFER_STORAGE | RAL_WEBGPU_BUFFER_COPY_DESTINATION;
	desc.memoryClass = RAL_ALLOCATION_DEVICE_LOCAL;
	if ( !RalWebGpu_CreateBuffer( resources, &desc, &atmosphere->arena,
			&atmosphere->arenaReceipt ) ) return qfalse;
	desc.size = sizeof( atmosphereVolume_t ) * RAL_ATMOSPHERE_MAX_VOLUMES;
	if ( !RalWebGpu_CreateBuffer( resources, &desc, &atmosphere->volumes,
			&atmosphere->volumesReceipt ) ) return qfalse;
	for ( uint32_t i = 0u; i < WEBGPU_ATMOSPHERE_PASS_COUNT; ++i ) {
		desc.size = sizeof( atmosphereParams_t );
		desc.usage = RAL_WEBGPU_BUFFER_UNIFORM
			| RAL_WEBGPU_BUFFER_COPY_DESTINATION;
		if ( !RalWebGpu_CreateBuffer( resources, &desc, &atmosphere->params[i],
				&atmosphere->paramsReceipts[i] ) ) return qfalse;
		memset( entries, 0, sizeof( entries ) );
		entries[0].binding = 0u; entries[0].kind = RAL_WEBGPU_BROWSER_BIND_RESOURCE_BUFFER;
		entries[0].resourceIdentity = atmosphere->paramsReceipts[i].resourceIdentity;
		entries[0].byteSize = sizeof( atmosphereParams_t );
		entries[1].binding = 1u; entries[1].kind = RAL_WEBGPU_BROWSER_BIND_RESOURCE_BUFFER;
		entries[1].resourceIdentity = atmosphere->arenaReceipt.resourceIdentity;
		entries[1].byteSize = atmosphere->arenaReceipt.byteSize;
		entries[2].binding = 2u; entries[2].kind = RAL_WEBGPU_BROWSER_BIND_RESOURCE_BUFFER;
		entries[2].resourceIdentity = atmosphere->volumesReceipt.resourceIdentity;
		entries[2].byteSize = atmosphere->volumesReceipt.byteSize;
		if ( !RalWebGpu_BrowserBridgeCreateBindGroup( atmosphere->bridge,
				atmosphere->pipelineReceipt.bindGroupLayoutIdentities[0],
				entries, 3u, &atmosphere->bindGroups[i] ) ) return qfalse;
	}
	return qtrue;
}

qboolean RalWebGpu_AtmosphereCreate( ralWebGpuRuntime_t *runtime,
		ralWebGpuRuntimeReceipt_t *runtimeReceipt,
		ralWebGpuBrowserBridge_t *bridge, uint64_t generation,
		ralWebGpuAtmosphere_t **outAtmosphere ) {
	ralWebGpuAtmosphere_t *atmosphere;
	if ( !runtime || !runtimeReceipt || !bridge || !generation
			|| !outAtmosphere || *outAtmosphere ) return qfalse;
	atmosphere = calloc( 1u, sizeof( *atmosphere ) );
	if ( !atmosphere ) return qfalse;
	atmosphere->runtime = runtime; atmosphere->bridge = bridge;
	atmosphere->generation = generation;
	atmosphere->command = RalWebGpu_RuntimeCommand( runtime, runtimeReceipt );
	if ( !atmosphere->command ) {
		RalWebGpu_AtmosphereDestroy( atmosphere, runtimeReceipt ); return qfalse;
	}
	if ( !CreatePipeline( atmosphere, runtimeReceipt ) ) {
		RalWebGpu_AtmosphereDestroy( atmosphere, runtimeReceipt ); return qfalse;
	}
	if ( !RalWebGpu_RuntimeGetReceipt( runtime, runtimeReceipt ) ) {
		RalWebGpu_AtmosphereDestroy( atmosphere, runtimeReceipt ); return qfalse;
	}
	if ( !CreateResources( atmosphere, runtimeReceipt ) ) {
		RalWebGpu_AtmosphereDestroy( atmosphere, runtimeReceipt ); return qfalse;
	}
	*outAtmosphere = atmosphere; return qtrue;
}

void RalWebGpu_AtmosphereDestroy( ralWebGpuAtmosphere_t *atmosphere,
		const ralWebGpuRuntimeReceipt_t *runtimeReceipt ) {
	ralWebGpuResourceLayer_t *resources;
	if ( !atmosphere ) return;
	for ( uint32_t i = WEBGPU_ATMOSPHERE_PASS_COUNT; i > 0u; --i )
		if ( atmosphere->bindGroups[i - 1u] )
			RalWebGpu_BrowserBridgeDestroyBindGroup( atmosphere->bridge,
				atmosphere->bindGroups[i - 1u] );
	resources = runtimeReceipt ? RalWebGpu_RuntimeResources(
		atmosphere->runtime, runtimeReceipt ) : NULL;
	if ( resources ) {
		for ( uint32_t i = WEBGPU_ATMOSPHERE_PASS_COUNT; i > 0u; --i )
			if ( atmosphere->params[i - 1u] )
				(void)RalWebGpu_DestroyResource( resources,
					atmosphere->params[i - 1u],
					&atmosphere->paramsReceipts[i - 1u] );
		if ( atmosphere->volumes ) (void)RalWebGpu_DestroyResource( resources,
			atmosphere->volumes, &atmosphere->volumesReceipt );
		if ( atmosphere->arena ) (void)RalWebGpu_DestroyResource( resources,
			atmosphere->arena, &atmosphere->arenaReceipt );
	}
	if ( atmosphere->pipeline && runtimeReceipt )
		(void)RalWebGpu_RuntimeDestroyPipeline( atmosphere->runtime,
			atmosphere->pipeline, &atmosphere->pipelineReceipt );
	free( atmosphere );
}

static qboolean WriteFrame( ralWebGpuAtmosphere_t *atmosphere,
		const ralWebGpuRuntimeReceipt_t *runtimeReceipt,
		const renderSubmissionState_t *frontend,
		const ralAtmospherePlanReceipt_t *plan ) {
	ralWebGpuResourceLayer_t *resources = RalWebGpu_RuntimeResources(
		atmosphere->runtime, runtimeReceipt );
	renderAtmosphereSnapshot_t snapshot;
	renderAtmosphereMediaSnapshot_t media;
	const atmosphereEmitter_t *emitters;
	const atmosphereMediaVolume_t *volumes;
	atmosphereVolume_t packed[RAL_ATMOSPHERE_MAX_VOLUMES];
	ralWebGpuWriteReceipt_t write;
	float precipitation = 0.0f;
	if ( !resources || !RenderSubmission_AtmosphereSnapshot( frontend,
			&snapshot, &emitters )
			|| !RenderSubmission_AtmosphereMediaSnapshot( frontend,
				&media, &volumes ) ) return qfalse;
	(void)emitters; memset( packed, 0, sizeof( packed ) );
	for ( uint32_t i = 0u; i < plan->admittedVolumeCount; ++i ) {
		const atmosphereMediaVolume_t *source = &volumes[i];
		memcpy( packed[i].originRadius, source->origin, 3u * sizeof( float ) );
		packed[i].originRadius[3] = source->radius;
		memcpy( packed[i].extentShape, source->extent, 3u * sizeof( float ) );
		packed[i].extentShape[3] = (float)source->shape;
		memcpy( packed[i].albedoAnisotropy, source->albedo, 3u * sizeof( float ) );
		packed[i].albedoAnisotropy[3] = source->anisotropy;
		memcpy( packed[i].emissiveIntensity, source->emissive, 3u * sizeof( float ) );
		packed[i].emissiveIntensity[3] = source->emissionIntensity;
		packed[i].media[0] = source->extinction;
		packed[i].media[1] = source->heightFalloff;
		packed[i].media[2] = source->noiseScale;
		packed[i].media[3] = (float)source->flags;
	}
	if ( !RalWebGpu_WriteBuffer( resources, atmosphere->volumes,
			&atmosphere->volumesReceipt, 0u, packed, sizeof( packed ), &write ) )
		return qfalse;
	for ( uint32_t i = 0u; i < 5u; ++i ) precipitation += snapshot.state.precipitation[i];
	if ( precipitation > 1.0f ) precipitation = 1.0f;
	for ( uint32_t stage = 0u; stage < WEBGPU_ATMOSPHERE_PASS_COUNT; ++stage ) {
		atmosphereParams_t params;
		uint32_t historyPrevious = atmosphere->historyParity
			? WEBGPU_ATMOSPHERE_HISTORY_B : WEBGPU_ATMOSPHERE_HISTORY_A;
		uint32_t historyNext = atmosphere->historyParity
			? WEBGPU_ATMOSPHERE_HISTORY_A : WEBGPU_ATMOSPHERE_HISTORY_B;
		memset( &params, 0, sizeof( params ) );
		params.dimsStage[0] = plan->froxelWidth;
		params.dimsStage[1] = plan->froxelHeight;
		params.dimsStage[2] = plan->froxelDepth; params.dimsStage[3] = stage;
		for ( uint32_t i = 0u; i < 4u; ++i )
			params.bases0[i] = i * RAL_WEBGPU_ATMOSPHERE_FROXEL_CAPACITY;
		params.bases1[0] = historyPrevious * RAL_WEBGPU_ATMOSPHERE_FROXEL_CAPACITY;
		params.bases1[1] = historyNext * RAL_WEBGPU_ATMOSPHERE_FROXEL_CAPACITY;
		params.bases1[2] = WEBGPU_ATMOSPHERE_INTEGRATED
			* RAL_WEBGPU_ATMOSPHERE_FROXEL_CAPACITY;
		params.bases1[3] = plan->admittedVolumeCount;
		memcpy( params.boundsMin, snapshot.state.bounds, 3u * sizeof( float ) );
		memcpy( params.boundsMax, snapshot.state.bounds + 3u, 3u * sizeof( float ) );
		params.climate[0] = snapshot.state.mediaDensity;
		params.climate[1] = snapshot.state.mediaHeightFalloff;
		params.climate[2] = plan->cloudsActive ? snapshot.state.cloudCover : 0.0f;
		params.climate[3] = snapshot.state.cloudShadow;
		params.lighting[0] = ( snapshot.state.ambientColor[0]
			+ snapshot.state.ambientColor[1] + snapshot.state.ambientColor[2] ) / 3.0f;
		params.lighting[1] = snapshot.state.sunIntensity;
		params.lighting[2] = snapshot.state.lightning;
		params.lighting[3] = atmosphere->historyValid
			&& plan->temporalReuseCount ? 0.88f : 0.0f;
		params.weather[0] = precipitation;
		params.weather[1] = snapshot.state.indoorExposure;
		params.weather[2] = snapshot.state.timelineSeconds;
		params.weather[3] = snapshot.state.visibility > 0.0f
			? snapshot.state.visibility : 65536.0f;
		if ( !RalWebGpu_WriteBuffer( resources, atmosphere->params[stage],
				&atmosphere->paramsReceipts[stage], 0u, &params,
				sizeof( params ), &write ) ) return qfalse;
	}
	return qtrue;
}

qboolean RalWebGpu_AtmosphereBegin( ralWebGpuAtmosphere_t *atmosphere,
		const ralWebGpuRuntimeReceipt_t *runtimeReceipt,
		const renderSubmissionState_t *frontend,
		const ralAtmospherePlanReceipt_t *plan,
		ralWebGpuAtmosphereReceipt_t *outReceipt ) {
	ralWebGpuCommand_t *command;
	ralWebGpuCommandReceipt_t recording, updated, executable;
	ralWebGpuSubmissionReceipt_t submission;
	ralWebGpuAtmosphereReceipt_t receipt;
	uint32_t stages[4] = { 0u, 1u, 2u, 3u }, stageCount;
	if ( !atmosphere || !runtimeReceipt || !frontend || !plan || !outReceipt
			|| atmosphere->active || plan->selectedTier != RAL_ATMOSPHERE_TIER_FULL
			|| plan->froxelCount > RAL_WEBGPU_ATMOSPHERE_FROXEL_CAPACITY
			|| !WriteFrame( atmosphere, runtimeReceipt, frontend, plan ) ) return qfalse;
	command = atmosphere->command;
	if ( !command || !RalWebGpu_CommandBegin( command, RAL_WEBGPU_PASS_COMPUTE,
			0u, &recording ) ) return qfalse;
	stageCount = plan->cloudsActive ? 4u : 3u;
	if ( !plan->cloudsActive ) stages[2] = 3u;
	for ( uint32_t i = 0u; i < stageCount; ++i ) {
		ralWebGpuComputeDispatch_t dispatch;
		memset( &dispatch, 0, sizeof( dispatch ) );
		dispatch.pipelineIdentity = atmosphere->pipelineReceipt.pipelineIdentity;
		dispatch.bindGroupIdentities[0] = atmosphere->bindGroups[stages[i]];
		dispatch.bindGroupCount = 1u;
		dispatch.groupCountX = ( plan->froxelWidth + 3u ) / 4u;
		dispatch.groupCountY = ( plan->froxelHeight + 3u ) / 4u;
		dispatch.groupCountZ = ( plan->froxelDepth + 3u ) / 4u;
		dispatch.contentDigest = plan->frameGeneration ^ (uint64_t)( stages[i] + 1u );
		if ( !dispatch.contentDigest ) dispatch.contentDigest = stages[i] + 1u;
		if ( !RalWebGpu_CommandRecordComputeDispatch( command, &recording,
				&dispatch, &updated ) ) {
			(void)RalWebGpu_CommandCancel( command, &recording ); return qfalse;
		}
		recording = updated;
	}
	if ( !RalWebGpu_CommandEnd( command, &recording, &executable )
			|| !RalWebGpu_CommandSubmit( command, &executable, &submission ) )
		return qfalse;
	memset( &receipt, 0, sizeof( receipt ) );
	receipt.schemaVersion = RAL_WEBGPU_ATMOSPHERE_SCHEMA_VERSION;
	receipt.backendType = RAL_BACKEND_WEBGPU;
	receipt.frameGeneration = plan->frameGeneration;
	receipt.executorGeneration = atmosphere->generation;
	receipt.plan = *plan; receipt.command = executable;
	receipt.submission = submission; receipt.dispatchCount = stageCount;
	receipt.froxelCount = plan->froxelCount;
	receipt.historyReused = plan->temporalReuseCount ? qtrue : qfalse;
	receipt.ready = qtrue;
	if ( !ReceiptValid( &receipt ) ) return qfalse;
	atmosphere->inFlight = receipt; atmosphere->active = qtrue;
	*outReceipt = receipt; return qtrue;
}

ralWebGpuAsyncStatus_t RalWebGpu_AtmospherePoll(
		ralWebGpuAtmosphere_t *atmosphere,
		const ralWebGpuAtmosphereReceipt_t *receipt ) {
	ralWebGpuCommand_t *command;
	ralWebGpuAsyncStatus_t status;
	if ( !atmosphere || !receipt || !atmosphere->active
			|| !RalWebGpu_AtmosphereReceiptExact( receipt,
				&atmosphere->inFlight ) ) return RAL_WEBGPU_ASYNC_FAILED;
	command = atmosphere->command;
	if ( !command ) return RAL_WEBGPU_ASYNC_FAILED;
	status = RalWebGpu_CommandPoll( command, &receipt->submission );
	if ( status == RAL_WEBGPU_ASYNC_READY ) {
		atmosphere->active = qfalse; atmosphere->historyValid = qtrue;
		atmosphere->historyParity = !atmosphere->historyParity;
		memset( &atmosphere->inFlight, 0, sizeof( atmosphere->inFlight ) );
	}
	return status;
}

uintptr_t RalWebGpu_AtmosphereArenaIdentity(
		const ralWebGpuAtmosphere_t *atmosphere ) {
	return atmosphere ? atmosphere->arenaReceipt.resourceIdentity : 0u;
}

uint64_t RalWebGpu_AtmosphereArenaBytes(
		const ralWebGpuAtmosphere_t *atmosphere ) {
	return atmosphere ? atmosphere->arenaReceipt.byteSize : 0u;
}

uint32_t RalWebGpu_AtmosphereIntegratedBase(
		const ralWebGpuAtmosphere_t *atmosphere ) {
	return atmosphere ? WEBGPU_ATMOSPHERE_INTEGRATED
		* RAL_WEBGPU_ATMOSPHERE_FROXEL_CAPACITY : 0u;
}

qboolean RalWebGpu_AtmosphereHistoryValid(
		const ralWebGpuAtmosphere_t *atmosphere ) {
	return atmosphere && atmosphere->historyValid ? qtrue : qfalse;
}

void RalWebGpu_AtmosphereInvalidateHistory(
		ralWebGpuAtmosphere_t *atmosphere ) {
	if ( atmosphere ) atmosphere->historyValid = qfalse;
}
