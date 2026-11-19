// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "ral_webgpu_sky.h"
#include "ral_webgpu_product.h"
#include "ral_webgpu_sky_source.h"
#include "ral_webgpu_sky_project_sh.h"
#include "ral_webgpu_sky_irradiance.h"
#include "ral_webgpu_sky_prefilter.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

enum {
	WEBGPU_SKY_TRANSMITTANCE,
	WEBGPU_SKY_MULTISCATTER,
	WEBGPU_SKY_VIEW,
	WEBGPU_SKY_STAGE_COUNT
};

enum {
	WEBGPU_SKY_RADIANCE_SOURCE,
	WEBGPU_SKY_RADIANCE_PROJECT_SH,
	WEBGPU_SKY_RADIANCE_IRRADIANCE,
	WEBGPU_SKY_RADIANCE_PREFILTER,
	WEBGPU_SKY_RADIANCE_PIPELINE_COUNT
};

#define WEBGPU_SKY_RADIANCE_MIPS WIRED_SKY_SPECULAR_MIP_COUNT
#define WEBGPU_SKY_RADIANCE_MAX_JOBS \
	( RAL_SKY_RADIANCE_MAX_SLICES_PER_FRAME + 1u )

typedef ralSkyRadianceSourceParams_t skySourceParams_t;

typedef struct { uint32_t values[4]; } skySliceParams_t;

_Static_assert( sizeof( skySourceParams_t ) == 64u,
	"WebGPU sky source parameter ABI drift" );

#define WEBGPU_SKY_TRANSMITTANCE_WIDTH 256u
#define WEBGPU_SKY_TRANSMITTANCE_HEIGHT 64u
#define WEBGPU_SKY_MULTISCATTER_SIZE 32u
#define WEBGPU_SKY_VIEW_WIDTH 192u
#define WEBGPU_SKY_VIEW_HEIGHT 108u

typedef struct {
	float sunDirectionAngularRadius[4];
	float sunRadiance[4];
	float moonDirectionAngularRadius[4];
	float moonRadiance[4];
	float atmosphereRadii[4];
	float scatteringScale[4];
	float groundExposure[4];
} skyParams_t;

_Static_assert( sizeof( skyParams_t ) == 112u,
	"WebGPU physical sky parameter ABI drift" );

struct ralWebGpuSky_s {
	ralWebGpuRuntime_t *runtime;
	ralWebGpuCommand_t *command;
	ralWebGpuBrowserBridge_t *bridge;
	uint64_t generation;
	ralWebGpuPipeline_t *pipelines[WEBGPU_SKY_STAGE_COUNT];
	ralWebGpuPipelineReceipt_t pipelineReceipts[WEBGPU_SKY_STAGE_COUNT];
	ralWebGpuResource_t *params;
	ralWebGpuResourceReceipt_t paramsReceipt;
	ralWebGpuResource_t *sampler;
	ralWebGpuResourceReceipt_t samplerReceipt;
	ralWebGpuResource_t *fallback;
	ralWebGpuResourceReceipt_t fallbackReceipt;
	ralWebGpuResource_t *radianceFallback;
	ralWebGpuResourceReceipt_t radianceFallbackReceipt;
	ralWebGpuResource_t *textures[WEBGPU_SKY_STAGE_COUNT];
	ralWebGpuResourceReceipt_t textureReceipts[WEBGPU_SKY_STAGE_COUNT];
	uintptr_t bindGroups[WEBGPU_SKY_STAGE_COUNT];
	uint64_t worldLifecycleGeneration;
	ralWebGpuSkyReceipt_t inFlight;
	ralWebGpuGpuTimingState_t physicalTimingState;
	uint64_t physicalGpuDurationNanoseconds;
	qboolean resourcesReady;
	qboolean active;
	ralWebGpuPipeline_t *radiancePipelines[WEBGPU_SKY_RADIANCE_PIPELINE_COUNT];
	ralWebGpuPipelineReceipt_t radiancePipelineReceipts[WEBGPU_SKY_RADIANCE_PIPELINE_COUNT];
	ralSkyRadianceState_t radianceState;
	ralSkyRadianceReceipt_t radiancePlan;
	uint64_t radianceFrameGeneration;
	float radianceTimeline;
	uint32_t completedRadianceSliceMask;
	uint64_t completedRadianceRevision;
	qboolean radianceIrradianceWriteStarted;
	ralWebGpuResource_t *radianceParams[WEBGPU_SKY_RADIANCE_MAX_JOBS];
	ralWebGpuResourceReceipt_t radianceParamReceipts[WEBGPU_SKY_RADIANCE_MAX_JOBS];
	uintptr_t radianceBindGroups[WEBGPU_SKY_RADIANCE_MAX_JOBS];
	uint32_t radianceJobCount;
	ralWebGpuSkyRadianceReceipt_t radianceInFlight;
	ralWebGpuGpuTimingState_t radianceTimingState;
	uint64_t radianceGpuDurationNanoseconds;
	qboolean radianceActive;
	ralWebGpuProduct_t *captureProduct; // borrowed until radiance submission completes
	int captureTimeMs;
	qboolean materialUsesTime;
};

static const char s_transmittanceWgsl[] =
	"struct P { sunDirectionAngularRadius: vec4<f32>, sunRadiance: vec4<f32>, moonDirectionAngularRadius: vec4<f32>, moonRadiance: vec4<f32>, atmosphereRadii: vec4<f32>, scatteringScale: vec4<f32>, groundExposure: vec4<f32> }\n"
	"@group(0) @binding(0) var<uniform> p: P;\n"
	"@group(0) @binding(1) var outTex: texture_storage_2d<rgba16float, write>;\n"
	"@compute @workgroup_size(8, 8, 1) fn main(@builtin(global_invocation_id) c: vec3<u32>) { let e = textureDimensions(outTex); if (any(c.xy >= e)) { return; } let uv = (vec2<f32>(c.xy) + 0.5) / vec2<f32>(e); let mu = uv.x * 2.0 - 1.0; let h = sqrt(max(1.0 - mu * mu, 0.0)); var a = mix(32.0, 1.0, clamp(mu * 0.5 + 0.5, 0.0, 1.0)); a *= mix(1.0, 0.035, uv.y) * mix(1.0, 1.8, h); let r = vec3<f32>(0.005802, 0.013558, 0.0331) * max(p.scatteringScale.x, 0.001); let m = vec3<f32>(0.003996) * max(p.scatteringScale.y, 0.001); let o = vec3<f32>(0.00065, 0.001881, 0.000085) * max(p.scatteringScale.z, 0.001); textureStore(outTex, vec2<i32>(c.xy), vec4<f32>(clamp(exp(-(r + m + o) * a), vec3<f32>(0.0), vec3<f32>(1.0)), a)); }";

static const char s_multiscatterWgsl[] =
	"struct P { sunDirectionAngularRadius: vec4<f32>, sunRadiance: vec4<f32>, moonDirectionAngularRadius: vec4<f32>, moonRadiance: vec4<f32>, atmosphereRadii: vec4<f32>, scatteringScale: vec4<f32>, groundExposure: vec4<f32> }\n"
	"@group(0) @binding(0) var<uniform> p: P;\n"
	"@group(0) @binding(1) var trans: texture_2d<f32>;\n"
	"@group(0) @binding(2) var outTex: texture_storage_2d<rgba16float, write>;\n"
	"@group(0) @binding(3) var samp: sampler;\n"
	"@compute @workgroup_size(8, 8, 1) fn main(@builtin(global_invocation_id) c: vec3<u32>) { let e = textureDimensions(outTex); if (any(c.xy >= e)) { return; } let uv = (vec2<f32>(c.xy) + 0.5) / vec2<f32>(e); var a = vec3<f32>(0.0); for (var i: u32 = 0u; i < 16u; i++) { let q = vec2<f32>(fract((f32(i) + 0.5) / 16.0 + uv.x), uv.y); let t = textureSampleLevel(trans, samp, q, 0.0).rgb; a += (vec3<f32>(1.0) - t) * t; } let energy = a / 16.0 * max(p.sunRadiance.rgb, vec3<f32>(0.001)) * (0.35 + 0.65 * clamp(p.groundExposure.w, 0.0, 4.0)); textureStore(outTex, vec2<i32>(c.xy), vec4<f32>(max(energy, vec3<f32>(0.0)), 1.0)); }";

static const char s_viewWgsl[] =
	"struct P { sunDirectionAngularRadius: vec4<f32>, sunRadiance: vec4<f32>, moonDirectionAngularRadius: vec4<f32>, moonRadiance: vec4<f32>, atmosphereRadii: vec4<f32>, scatteringScale: vec4<f32>, groundExposure: vec4<f32> }\n"
	"@group(0) @binding(0) var<uniform> p: P;\n"
	"@group(0) @binding(1) var trans: texture_2d<f32>;\n"
	"@group(0) @binding(2) var multi: texture_2d<f32>;\n"
	"@group(0) @binding(3) var outTex: texture_storage_2d<rgba16float, write>;\n"
	"@group(0) @binding(4) var samp: sampler;\n"
	"@compute @workgroup_size(8, 8, 1) fn main(@builtin(global_invocation_id) c: vec3<u32>) { let e = textureDimensions(outTex); if (any(c.xy >= e)) { return; } let uv = (vec2<f32>(c.xy) + 0.5) / vec2<f32>(e); let az = (uv.x * 2.0 - 1.0) * 3.1415927; let el = (uv.y - 0.5) * 3.1415927; let v = normalize(vec3<f32>(cos(el) * cos(az), cos(el) * sin(az), sin(el))); let s = normalize(p.sunDirectionAngularRadius.xyz); let ct = clamp(dot(v, s), -1.0, 1.0); let tuv = vec2<f32>(v.z * 0.5 + 0.5, clamp(p.atmosphereRadii.z, 0.0, 1.0)); let t = textureSampleLevel(trans, samp, tuv, 0.0).rgb; let ms = textureSampleLevel(multi, samp, vec2<f32>(ct * 0.5 + 0.5, tuv.y), 0.0).rgb; let ray = vec3<f32>(0.18, 0.34, 0.72) * (3.0 * (1.0 + ct * ct) / 50.265484); let g = clamp(p.scatteringScale.w, 0.0, 0.95); let g2 = g * g; let d = max(1.0 + g2 - 2.0 * g * ct, 0.001); let mie = vec3<f32>(0.42, 0.34, 0.24) * ((1.0 - g2) / (12.566371 * d * sqrt(d))); let sh = clamp(s.z * 0.5 + 0.5, 0.0, 1.0); let twilight = 1.0 - smoothstep(0.42, 0.62, sh); let above = smoothstep(-0.08, 0.04, v.z); let em = pow(clamp(v.z, 0.0, 1.0), 0.35); let zenith = mix(vec3<f32>(0.025, 0.035, 0.10), vec3<f32>(0.035, 0.16, 0.46), sh); let horizon = mix(vec3<f32>(0.30, 0.52, 0.75), vec3<f32>(0.95, 0.24, 0.055), twilight); let ground = max(p.groundExposure.rgb, vec3<f32>(0.015)) * 0.22; let directional = mix(ground, mix(horizon, zenith, em), above); let sunEnergy = dot(p.sunRadiance.rgb, vec3<f32>(0.2126, 0.7152, 0.0722)); var radiance = directional * (0.22 + 0.48 * max(sunEnergy, 0.0)); radiance += p.sunRadiance.rgb * (ray + mie) * (vec3<f32>(1.0) - t) * 0.45 + ms * 0.12; let radius = max(p.sunDirectionAngularRadius.w, 0.006); let disk = smoothstep(cos(radius * 1.8), cos(radius), ct); let corona = pow(max(ct, 0.0), 256.0) * 0.55 + pow(max(ct, 0.0), 24.0) * 0.06; radiance += p.sunRadiance.rgb * t * (disk * 7.0 + corona); let md = normalize(p.moonDirectionAngularRadius.xyz); let mt = clamp(dot(v, md), -1.0, 1.0); let mr = max(p.moonDirectionAngularRadius.w, 0.009); let moonDisk = smoothstep(cos(mr * 1.6), cos(mr), mt); radiance += p.moonRadiance.rgb * (moonDisk * 3.0 + pow(max(mt, 0.0), 96.0) * 0.10); radiance *= max(p.groundExposure.w, 0.0); textureStore(outTex, vec2<i32>(c.xy), vec4<f32>(max(radiance, vec3<f32>(0.0)), 1.0)); }";

static qboolean ReceiptValid( const ralWebGpuSkyReceipt_t *receipt ) {
	const uint32_t mask = RAL_SKY_DIRTY_TRANSMITTANCE
		| RAL_SKY_DIRTY_MULTISCATTER | RAL_SKY_DIRTY_SKY_VIEW;
	return receipt && receipt->schemaVersion == RAL_WEBGPU_SKY_SCHEMA_VERSION
		&& receipt->backendType == RAL_BACKEND_WEBGPU
		&& receipt->executorGeneration && receipt->frameGeneration
		&& receipt->worldLifecycleGeneration && receipt->sourceRevision
		&& receipt->workMask && !( receipt->workMask & ~mask )
		&& !( receipt->workMask & ( receipt->workMask - 1u ) )
		&& RalWebGpu_CommandReceiptExact( &receipt->command, &receipt->command )
		&& RalWebGpu_SubmissionReceiptExact( &receipt->submission,
			&receipt->submission ) && receipt->command.dispatchCount == 1u
		&& receipt->ready == qtrue;
}

qboolean RalWebGpu_SkyReceiptExact( const ralWebGpuSkyReceipt_t *a,
		const ralWebGpuSkyReceipt_t *b ) {
	return ReceiptValid( a ) && ReceiptValid( b )
		&& !memcmp( a, b, sizeof( *a ) );
}

static qboolean Artifact( ralShaderArtifactAbi_t *artifact,
		const char *source, uint32_t sourceBytes ) {
	memset( artifact, 0, sizeof( *artifact ) );
	artifact->target = RAL_SHADER_ARTIFACT_WGSL;
	artifact->byteCount = sourceBytes;
	return Ral_ShaderArtifactDigest( source, sourceBytes, &artifact->digest )
		== ralSuccess;
}

static void Binding( ralShaderBindingAbi_t *binding, uint32_t slot,
		ralShaderBindingClass_t kind ) {
	memset( binding, 0, sizeof( *binding ) );
	binding->binding = slot; binding->bindingClass = kind;
	binding->arrayCount = 1u; binding->stageFlags = RAL_STAGE_COMPUTE;
	if ( kind == RAL_SHADER_BIND_UNIFORM_BUFFER )
		binding->minBufferBindingSize = sizeof( skyParams_t );
	else if ( kind == RAL_SHADER_BIND_SAMPLED_TEXTURE ) {
		binding->viewDimension = RAL_SHADER_VIEW_2D;
		binding->sampleType = RAL_SHADER_SAMPLE_FLOAT;
	} else if ( kind == RAL_SHADER_BIND_STORAGE_TEXTURE_WRITE ) {
		binding->viewDimension = RAL_SHADER_VIEW_2D;
		binding->storageTextureFormat = RAL_FORMAT_R16G16B16A16_SFLOAT;
	}
}

static qboolean CreatePipeline( ralWebGpuSky_t *sky,
		ralWebGpuRuntimeReceipt_t *runtimeReceipt, uint32_t stage,
		const char *source ) {
	uint32_t spirv[4] = { 0x07230203u };
	ralShaderModuleAbi_t module;
	ralShaderBindingAbi_t bindings[5];
	ralShaderAbiManifest_t manifest;
	ralShaderVariantAbi_t variant;
	ralWebGpuWgslModule_t wgsl;
	ralComputePipelineCreateInfo_t compute;
	ralWebGpuPipelineCreateInfo_t info;
	const ralBindGroupLayout_t *layouts[1] = {
		(const ralBindGroupLayout_t *)(uintptr_t)1u };
	uint32_t count = stage == WEBGPU_SKY_TRANSMITTANCE ? 2u
		: stage == WEBGPU_SKY_MULTISCATTER ? 4u : 5u;
	memset( &module, 0, sizeof( module ) );
	module.stage = RAL_STAGE_COMPUTE; module.entryPoint = "main";
	module.sourceDigest.lane0 = sky->generation + stage * 2u;
	module.sourceDigest.lane1 = module.sourceDigest.lane0 + 1u;
	module.artifacts[RAL_SHADER_ARTIFACT_SPIRV].target = RAL_SHADER_ARTIFACT_SPIRV;
	module.artifacts[RAL_SHADER_ARTIFACT_SPIRV].byteCount = sizeof( spirv );
	module.artifacts[RAL_SHADER_ARTIFACT_MSL].target = RAL_SHADER_ARTIFACT_MSL;
	if ( Ral_ShaderArtifactDigest( spirv, sizeof( spirv ),
			&module.artifacts[RAL_SHADER_ARTIFACT_SPIRV].digest ) != ralSuccess
			|| !Artifact( &module.artifacts[RAL_SHADER_ARTIFACT_WGSL], source,
				(uint32_t)strlen( source ) ) ) return qfalse;
	Binding( &bindings[0], 0u, RAL_SHADER_BIND_UNIFORM_BUFFER );
	if ( stage == WEBGPU_SKY_TRANSMITTANCE ) {
		Binding( &bindings[1], 1u, RAL_SHADER_BIND_STORAGE_TEXTURE_WRITE );
	} else if ( stage == WEBGPU_SKY_MULTISCATTER ) {
		Binding( &bindings[1], 1u, RAL_SHADER_BIND_SAMPLED_TEXTURE );
		Binding( &bindings[2], 2u, RAL_SHADER_BIND_STORAGE_TEXTURE_WRITE );
		Binding( &bindings[3], 3u, RAL_SHADER_BIND_FILTERING_SAMPLER );
	} else {
		Binding( &bindings[1], 1u, RAL_SHADER_BIND_SAMPLED_TEXTURE );
		Binding( &bindings[2], 2u, RAL_SHADER_BIND_SAMPLED_TEXTURE );
		Binding( &bindings[3], 3u, RAL_SHADER_BIND_STORAGE_TEXTURE_WRITE );
		Binding( &bindings[4], 4u, RAL_SHADER_BIND_FILTERING_SAMPLER );
	}
	memset( &manifest, 0, sizeof( manifest ) );
	manifest.schemaVersion = RAL_SHADER_ABI_SCHEMA_VERSION;
	manifest.generation = sky->generation + stage;
	manifest.pipelineKind = RAL_SHADER_PIPELINE_COMPUTE;
	manifest.modules = &module; manifest.moduleCount = 1u;
	manifest.bindings = bindings; manifest.bindingCount = count;
	memset( &compute, 0, sizeof( compute ) );
	compute.bindGroupLayouts = layouts; compute.numBindGroupLayouts = 1u;
	memset( &variant, 0, sizeof( variant ) );
	variant.artifactTarget = RAL_SHADER_ARTIFACT_WGSL;
	variant.generation = sky->generation + stage;
	if ( Ral_ComputePipelineSemanticDigest( &compute,
			&variant.semanticStateDigest ) != ralSuccess ) return qfalse;
	wgsl.code = source; wgsl.byteCount = (uint32_t)strlen( source );
	memset( &info, 0, sizeof( info ) ); info.manifest = &manifest;
	info.variant = &variant; info.modules = &wgsl; info.moduleCount = 1u;
	info.computeState = &compute; info.generation = variant.generation;
	if ( !RalWebGpu_BrowserBridgeApplyPipelineInfo( sky->bridge, &info )
			|| !RalWebGpu_RuntimeCreatePipeline( sky->runtime, runtimeReceipt,
				&info, &sky->pipelines[stage],
				&sky->pipelineReceipts[stage] ) ) return qfalse;
	return RalWebGpu_RuntimeGetReceipt( sky->runtime, runtimeReceipt );
}

static void RadianceBinding( ralShaderBindingAbi_t *binding, uint32_t slot,
		ralShaderBindingClass_t kind, ralShaderTextureViewDimension_t dimension,
		uint64_t minBytes ) {
	memset( binding, 0, sizeof( *binding ) );
	binding->binding = slot;
	binding->bindingClass = kind;
	binding->arrayCount = 1u;
	binding->stageFlags = RAL_STAGE_COMPUTE;
	binding->minBufferBindingSize = minBytes;
	binding->viewDimension = dimension;
	if ( kind == RAL_SHADER_BIND_SAMPLED_TEXTURE )
		binding->sampleType = RAL_SHADER_SAMPLE_FLOAT;
	else if ( kind == RAL_SHADER_BIND_STORAGE_TEXTURE_WRITE )
		binding->storageTextureFormat = RAL_FORMAT_R16G16B16A16_SFLOAT;
}

static qboolean CreateRadiancePipeline( ralWebGpuSky_t *sky,
		ralWebGpuRuntimeReceipt_t *runtimeReceipt, uint32_t stage,
		const unsigned char *source, uint32_t sourceBytes ) {
	uint32_t spirv[4] = { 0x07230203u };
	ralShaderModuleAbi_t module;
	ralShaderBindingAbi_t bindings[9];
	ralShaderAbiManifest_t manifest;
	ralShaderVariantAbi_t variant;
	ralWebGpuWgslModule_t wgsl;
	ralComputePipelineCreateInfo_t compute;
	ralWebGpuPipelineCreateInfo_t info;
	const ralBindGroupLayout_t *layouts[1] = {
		(const ralBindGroupLayout_t *)(uintptr_t)1u };
	uint32_t count = 0u;
	memset( &module, 0, sizeof( module ) );
	module.stage = RAL_STAGE_COMPUTE;
	module.entryPoint = "main";
	module.sourceDigest.lane0 = sky->generation + 32u + stage * 2u;
	module.sourceDigest.lane1 = module.sourceDigest.lane0 + 1u;
	module.artifacts[RAL_SHADER_ARTIFACT_SPIRV].target =
		RAL_SHADER_ARTIFACT_SPIRV;
	module.artifacts[RAL_SHADER_ARTIFACT_SPIRV].byteCount = sizeof( spirv );
	module.artifacts[RAL_SHADER_ARTIFACT_MSL].target = RAL_SHADER_ARTIFACT_MSL;
	if ( Ral_ShaderArtifactDigest( spirv, sizeof( spirv ),
			&module.artifacts[RAL_SHADER_ARTIFACT_SPIRV].digest ) != ralSuccess
			|| !Artifact( &module.artifacts[RAL_SHADER_ARTIFACT_WGSL],
				(const char *)source, sourceBytes ) ) return qfalse;
	memset( bindings, 0, sizeof( bindings ) );
	if ( stage == WEBGPU_SKY_RADIANCE_SOURCE ) {
		RadianceBinding( &bindings[count++], 0u,
			RAL_SHADER_BIND_STORAGE_TEXTURE_WRITE, RAL_SHADER_VIEW_2D_ARRAY, 0u );
		for ( uint32_t slot = 1u; slot <= 6u; ++slot )
			RadianceBinding( &bindings[count++], slot,
				RAL_SHADER_BIND_SAMPLED_TEXTURE, RAL_SHADER_VIEW_2D, 0u );
		RadianceBinding( &bindings[count++], 7u,
			RAL_SHADER_BIND_FILTERING_SAMPLER, 0, 0u );
		RadianceBinding( &bindings[count++], 8u,
			RAL_SHADER_BIND_UNIFORM_BUFFER, 0, sizeof( skySourceParams_t ) );
	} else {
		RadianceBinding( &bindings[count++], 0u,
			RAL_SHADER_BIND_SAMPLED_TEXTURE, RAL_SHADER_VIEW_2D_ARRAY, 0u );
		RadianceBinding( &bindings[count++], 1u,
			RAL_SHADER_BIND_FILTERING_SAMPLER, 0, 0u );
		RadianceBinding( &bindings[count++], 2u,
			stage == WEBGPU_SKY_RADIANCE_PROJECT_SH
				? RAL_SHADER_BIND_STORAGE_BUFFER_READ_WRITE
				: RAL_SHADER_BIND_STORAGE_TEXTURE_WRITE,
			stage == WEBGPU_SKY_RADIANCE_PROJECT_SH ? 0
				: RAL_SHADER_VIEW_2D_ARRAY,
			stage == WEBGPU_SKY_RADIANCE_PROJECT_SH
				? RAL_SKY_RADIANCE_AMBIENT_SH_COEFFICIENT_COUNT
					* 4u * sizeof( float ) : 0u );
		RadianceBinding( &bindings[count++], 3u,
			RAL_SHADER_BIND_UNIFORM_BUFFER, 0, sizeof( skySliceParams_t ) );
	}
	memset( &manifest, 0, sizeof( manifest ) );
	manifest.schemaVersion = RAL_SHADER_ABI_SCHEMA_VERSION;
	manifest.generation = sky->generation + 32u + stage;
	manifest.pipelineKind = RAL_SHADER_PIPELINE_COMPUTE;
	manifest.modules = &module;
	manifest.moduleCount = 1u;
	manifest.bindings = bindings;
	manifest.bindingCount = count;
	memset( &compute, 0, sizeof( compute ) );
	compute.bindGroupLayouts = layouts;
	compute.numBindGroupLayouts = 1u;
	memset( &variant, 0, sizeof( variant ) );
	variant.artifactTarget = RAL_SHADER_ARTIFACT_WGSL;
	variant.generation = manifest.generation;
	if ( Ral_ComputePipelineSemanticDigest( &compute,
			&variant.semanticStateDigest ) != ralSuccess ) return qfalse;
	wgsl.code = (const char *)source;
	wgsl.byteCount = sourceBytes;
	memset( &info, 0, sizeof( info ) );
	info.manifest = &manifest;
	info.variant = &variant;
	info.modules = &wgsl;
	info.moduleCount = 1u;
	info.computeState = &compute;
	info.generation = variant.generation;
	if ( !RalWebGpu_BrowserBridgeApplyPipelineInfo( sky->bridge, &info )
			|| !RalWebGpu_RuntimeCreatePipeline( sky->runtime, runtimeReceipt,
				&info, &sky->radiancePipelines[stage],
				&sky->radiancePipelineReceipts[stage] ) ) return qfalse;
	return RalWebGpu_RuntimeGetReceipt( sky->runtime, runtimeReceipt );
}

static void DestroyRadianceJobs( ralWebGpuSky_t *sky,
		const ralWebGpuRuntimeReceipt_t *runtimeReceipt ) {
	ralWebGpuResourceLayer_t *resources;
	if ( !sky || !runtimeReceipt ) return;
	resources = RalWebGpu_RuntimeResources( sky->runtime, runtimeReceipt );
	for ( uint32_t i = 0u; i < sky->radianceJobCount; ++i ) {
		if ( sky->radianceBindGroups[i] )
			RalWebGpu_BrowserBridgeDestroyBindGroup( sky->bridge,
				sky->radianceBindGroups[i] );
		if ( resources && sky->radianceParams[i] )
			(void)RalWebGpu_DestroyResource( resources,
				sky->radianceParams[i], &sky->radianceParamReceipts[i] );
	}
	memset( sky->radianceBindGroups, 0,
		sizeof( sky->radianceBindGroups ) );
	memset( sky->radianceParams, 0, sizeof( sky->radianceParams ) );
	memset( sky->radianceParamReceipts, 0,
		sizeof( sky->radianceParamReceipts ) );
	sky->radianceJobCount = 0u;
}

static void DestroyWorldResources( ralWebGpuSky_t *sky,
		ralWebGpuResourceLayer_t *resources ) {
	for ( uint32_t i = WEBGPU_SKY_STAGE_COUNT; i > 0u; --i )
		if ( sky->bindGroups[i - 1u] )
			RalWebGpu_BrowserBridgeDestroyBindGroup( sky->bridge,
				sky->bindGroups[i - 1u] );
	for ( uint32_t i = WEBGPU_SKY_STAGE_COUNT; i > 0u; --i )
		if ( sky->textures[i - 1u] )
			(void)RalWebGpu_DestroyResource( resources, sky->textures[i - 1u],
				&sky->textureReceipts[i - 1u] );
	memset( sky->bindGroups, 0, sizeof( sky->bindGroups ) );
	memset( sky->textures, 0, sizeof( sky->textures ) );
	memset( sky->textureReceipts, 0, sizeof( sky->textureReceipts ) );
	sky->worldLifecycleGeneration = 0u; sky->resourcesReady = qfalse;
}

qboolean RalWebGpu_SkyCreate( ralWebGpuRuntime_t *runtime,
		ralWebGpuRuntimeReceipt_t *runtimeReceipt,
		ralWebGpuBrowserBridge_t *bridge, uint64_t generation,
		ralWebGpuSky_t **outSky ) {
	ralWebGpuSky_t *sky;
	ralWebGpuResourceLayer_t *resources;
	ralWebGpuBufferDesc_t bufferDesc;
	ralWebGpuSamplerDesc_t samplerDesc;
	ralWebGpuTextureDesc_t fallbackDesc;
	const char *sources[WEBGPU_SKY_STAGE_COUNT] = {
		s_transmittanceWgsl, s_multiscatterWgsl, s_viewWgsl };
	const unsigned char *radianceSources[WEBGPU_SKY_RADIANCE_PIPELINE_COUNT] = {
		ral_webgpu_sky_source_wgsl, ral_webgpu_sky_project_sh_wgsl,
		ral_webgpu_sky_irradiance_wgsl, ral_webgpu_sky_prefilter_wgsl };
	const unsigned long radianceSourceBytes[WEBGPU_SKY_RADIANCE_PIPELINE_COUNT] = {
		ral_webgpu_sky_source_wgsl_size, ral_webgpu_sky_project_sh_wgsl_size,
		ral_webgpu_sky_irradiance_wgsl_size,
		ral_webgpu_sky_prefilter_wgsl_size };
	if ( !runtime || !runtimeReceipt || !bridge || !generation || !outSky
			|| *outSky ) return qfalse;
	sky = calloc( 1u, sizeof( *sky ) );
	if ( !sky ) return qfalse;
	sky->physicalTimingState = RAL_WEBGPU_GPU_TIMING_UNAVAILABLE;
	sky->radianceTimingState = RAL_WEBGPU_GPU_TIMING_UNAVAILABLE;
	sky->runtime = runtime; sky->bridge = bridge; sky->generation = generation;
	sky->command = RalWebGpu_RuntimeCommand( runtime, runtimeReceipt );
	if ( !sky->command ) goto fail;
	for ( uint32_t i = 0u; i < WEBGPU_SKY_STAGE_COUNT; ++i )
		if ( !CreatePipeline( sky, runtimeReceipt, i, sources[i] ) ) goto fail;
	for ( uint32_t i = 0u; i < WEBGPU_SKY_RADIANCE_PIPELINE_COUNT; ++i )
		if ( radianceSourceBytes[i] > UINT32_MAX
				|| !CreateRadiancePipeline( sky, runtimeReceipt, i,
					radianceSources[i], (uint32_t)radianceSourceBytes[i] ) ) goto fail;
	if ( !Ral_SkyRadianceInit( RAL_BACKEND_WEBGPU,
			&sky->radianceState ) ) goto fail;
	resources = RalWebGpu_RuntimeResources( runtime, runtimeReceipt );
	if ( !resources ) goto fail;
	memset( &bufferDesc, 0, sizeof( bufferDesc ) );
	bufferDesc.size = sizeof( skyParams_t );
	bufferDesc.usage = RAL_WEBGPU_BUFFER_UNIFORM
		| RAL_WEBGPU_BUFFER_COPY_DESTINATION;
	bufferDesc.memoryClass = RAL_ALLOCATION_DEVICE_LOCAL;
	if ( !RalWebGpu_CreateBuffer( resources, &bufferDesc, &sky->params,
			&sky->paramsReceipt ) ) goto fail;
	memset( &samplerDesc, 0, sizeof( samplerDesc ) );
	samplerDesc.linearMinification = qtrue;
	samplerDesc.linearMagnification = qtrue;
	samplerDesc.clampToEdge = qtrue;
	if ( !RalWebGpu_CreateSampler( resources, &samplerDesc, &sky->sampler,
			&sky->samplerReceipt ) ) goto fail;
	memset( &fallbackDesc, 0, sizeof( fallbackDesc ) );
	fallbackDesc.width = 1u; fallbackDesc.height = 1u; fallbackDesc.depth = 1u;
	fallbackDesc.mipLevelCount = 1u;
	fallbackDesc.format = RAL_FORMAT_R16G16B16A16_SFLOAT;
	fallbackDesc.bytesPerTexel = 8u;
	fallbackDesc.usage = RAL_WEBGPU_TEXTURE_SAMPLED;
	if ( !RalWebGpu_CreateTexture( resources, &fallbackDesc, &sky->fallback,
			&sky->fallbackReceipt ) ) goto fail;
	fallbackDesc.depth = RAL_SKY_RADIANCE_CAPTURE_FACE_COUNT;
	if ( !RalWebGpu_CreateTexture( resources, &fallbackDesc,
			&sky->radianceFallback, &sky->radianceFallbackReceipt ) ) goto fail;
	*outSky = sky; return qtrue;
fail:
	RalWebGpu_SkyDestroy( sky, runtimeReceipt ); return qfalse;
}

void RalWebGpu_SkyDestroy( ralWebGpuSky_t *sky,
		const ralWebGpuRuntimeReceipt_t *runtimeReceipt ) {
	ralWebGpuRuntimeReceipt_t authority;
	ralWebGpuResourceLayer_t *resources;
	if ( !sky ) return;
	authority = runtimeReceipt ? *runtimeReceipt
		: (ralWebGpuRuntimeReceipt_t){ 0 };
	resources = runtimeReceipt ? RalWebGpu_RuntimeResources( sky->runtime,
		&authority ) : NULL;
	if ( runtimeReceipt && !sky->radianceActive )
		DestroyRadianceJobs( sky, runtimeReceipt );
	if ( resources && !sky->active ) {
		DestroyWorldResources( sky, resources );
		if ( sky->sampler ) (void)RalWebGpu_DestroyResource( resources,
			sky->sampler, &sky->samplerReceipt );
		if ( sky->fallback ) (void)RalWebGpu_DestroyResource( resources,
			sky->fallback, &sky->fallbackReceipt );
		if ( sky->radianceFallback ) (void)RalWebGpu_DestroyResource( resources,
			sky->radianceFallback, &sky->radianceFallbackReceipt );
		if ( sky->params ) (void)RalWebGpu_DestroyResource( resources,
			sky->params, &sky->paramsReceipt );
	}
	for ( uint32_t i = WEBGPU_SKY_STAGE_COUNT; i > 0u; --i ) {
		uint32_t index = i - 1u;
		if ( sky->pipelines[index]
				&& RalWebGpu_RuntimeOwnsPipeline( sky->runtime, &authority,
					&sky->pipelineReceipts[index] ) ) {
			(void)RalWebGpu_RuntimeDestroyPipeline( sky->runtime,
				sky->pipelines[index], &sky->pipelineReceipts[index] );
			(void)RalWebGpu_RuntimeGetReceipt( sky->runtime, &authority );
		}
	}
	for ( uint32_t i = WEBGPU_SKY_RADIANCE_PIPELINE_COUNT; i > 0u; --i ) {
		uint32_t index = i - 1u;
		if ( sky->radiancePipelines[index]
				&& RalWebGpu_RuntimeOwnsPipeline( sky->runtime, &authority,
					&sky->radiancePipelineReceipts[index] ) ) {
			(void)RalWebGpu_RuntimeDestroyPipeline( sky->runtime,
				sky->radiancePipelines[index],
				&sky->radiancePipelineReceipts[index] );
			(void)RalWebGpu_RuntimeGetReceipt( sky->runtime, &authority );
		}
	}
	free( sky );
}

qboolean RalWebGpu_SkyEnsureWorld( ralWebGpuSky_t *sky,
		const ralWebGpuRuntimeReceipt_t *runtimeReceipt,
		uint64_t worldLifecycleGeneration ) {
	ralWebGpuResourceLayer_t *resources;
	ralWebGpuResource_t *textures[WEBGPU_SKY_STAGE_COUNT] = { NULL };
	ralWebGpuResourceReceipt_t receipts[WEBGPU_SKY_STAGE_COUNT];
	uintptr_t groups[WEBGPU_SKY_STAGE_COUNT] = { 0u };
	ralWebGpuTextureDesc_t desc;
	uint32_t widths[WEBGPU_SKY_STAGE_COUNT] = {
		WEBGPU_SKY_TRANSMITTANCE_WIDTH, WEBGPU_SKY_MULTISCATTER_SIZE,
		WEBGPU_SKY_VIEW_WIDTH };
	uint32_t heights[WEBGPU_SKY_STAGE_COUNT] = {
		WEBGPU_SKY_TRANSMITTANCE_HEIGHT, WEBGPU_SKY_MULTISCATTER_SIZE,
		WEBGPU_SKY_VIEW_HEIGHT };
	if ( !sky || !runtimeReceipt || sky->active || !worldLifecycleGeneration
			|| worldLifecycleGeneration == UINT64_MAX ) return qfalse;
	if ( sky->resourcesReady
			&& sky->worldLifecycleGeneration == worldLifecycleGeneration )
		return qtrue;
	if(sky->radianceActive) return qfalse;
	resources = RalWebGpu_RuntimeResources( sky->runtime, runtimeReceipt );
	if ( !resources ) return qfalse;
	memset( receipts, 0, sizeof( receipts ) );
	memset( &desc, 0, sizeof( desc ) ); desc.depth = 1u;
	desc.mipLevelCount = 1u; desc.format = RAL_FORMAT_R16G16B16A16_SFLOAT;
	desc.bytesPerTexel = 8u; desc.usage = RAL_WEBGPU_TEXTURE_SAMPLED
		| RAL_WEBGPU_TEXTURE_STORAGE;
	for ( uint32_t i = 0u; i < WEBGPU_SKY_STAGE_COUNT; ++i ) {
		desc.width = widths[i]; desc.height = heights[i];
		if ( !RalWebGpu_CreateTexture( resources, &desc, &textures[i],
				&receipts[i] ) ) goto fail;
	}
	for ( uint32_t stage = 0u; stage < WEBGPU_SKY_STAGE_COUNT; ++stage ) {
		ralWebGpuBrowserBindResource_t entries[5];
		uint32_t count = 0u;
		memset( entries, 0, sizeof( entries ) );
		entries[count++] = (ralWebGpuBrowserBindResource_t){ 0u,
			RAL_WEBGPU_BROWSER_BIND_RESOURCE_BUFFER,
			sky->paramsReceipt.resourceIdentity, 0u, sizeof( skyParams_t ) };
		if ( stage == WEBGPU_SKY_TRANSMITTANCE ) {
			entries[count++] = (ralWebGpuBrowserBindResource_t){ 1u,
				RAL_WEBGPU_BROWSER_BIND_RESOURCE_TEXTURE,
				receipts[WEBGPU_SKY_TRANSMITTANCE].resourceIdentity, 0u, 0u };
		} else if ( stage == WEBGPU_SKY_MULTISCATTER ) {
			entries[count++] = (ralWebGpuBrowserBindResource_t){ 1u,
				RAL_WEBGPU_BROWSER_BIND_RESOURCE_TEXTURE,
				receipts[WEBGPU_SKY_TRANSMITTANCE].resourceIdentity, 0u, 0u };
			entries[count++] = (ralWebGpuBrowserBindResource_t){ 2u,
				RAL_WEBGPU_BROWSER_BIND_RESOURCE_TEXTURE,
				receipts[WEBGPU_SKY_MULTISCATTER].resourceIdentity, 0u, 0u };
			entries[count++] = (ralWebGpuBrowserBindResource_t){ 3u,
				RAL_WEBGPU_BROWSER_BIND_RESOURCE_SAMPLER,
				sky->samplerReceipt.resourceIdentity, 0u, 0u };
		} else {
			entries[count++] = (ralWebGpuBrowserBindResource_t){ 1u,
				RAL_WEBGPU_BROWSER_BIND_RESOURCE_TEXTURE,
				receipts[WEBGPU_SKY_TRANSMITTANCE].resourceIdentity, 0u, 0u };
			entries[count++] = (ralWebGpuBrowserBindResource_t){ 2u,
				RAL_WEBGPU_BROWSER_BIND_RESOURCE_TEXTURE,
				receipts[WEBGPU_SKY_MULTISCATTER].resourceIdentity, 0u, 0u };
			entries[count++] = (ralWebGpuBrowserBindResource_t){ 3u,
				RAL_WEBGPU_BROWSER_BIND_RESOURCE_TEXTURE,
				receipts[WEBGPU_SKY_VIEW].resourceIdentity, 0u, 0u };
			entries[count++] = (ralWebGpuBrowserBindResource_t){ 4u,
				RAL_WEBGPU_BROWSER_BIND_RESOURCE_SAMPLER,
				sky->samplerReceipt.resourceIdentity, 0u, 0u };
		}
		if ( !RalWebGpu_BrowserBridgeCreateBindGroup( sky->bridge,
				sky->pipelineReceipts[stage].bindGroupLayoutIdentities[0],
				entries, count, &groups[stage] ) ) goto fail;
	}
	DestroyWorldResources( sky, resources );
	memcpy( sky->textures, textures, sizeof( textures ) );
	memcpy( sky->textureReceipts, receipts, sizeof( receipts ) );
	memcpy( sky->bindGroups, groups, sizeof( groups ) );
	sky->worldLifecycleGeneration = worldLifecycleGeneration;
	sky->resourcesReady = qtrue; return qtrue;
fail:
	for ( uint32_t i = WEBGPU_SKY_STAGE_COUNT; i > 0u; --i )
		if ( groups[i - 1u] ) RalWebGpu_BrowserBridgeDestroyBindGroup(
			sky->bridge, groups[i - 1u] );
	for ( uint32_t i = WEBGPU_SKY_STAGE_COUNT; i > 0u; --i )
		if ( textures[i - 1u] ) (void)RalWebGpu_DestroyResource( resources,
			textures[i - 1u], &receipts[i - 1u] );
	return qfalse;
}

qboolean RalWebGpu_SkyUnloadWorld( ralWebGpuSky_t *sky,
		const ralWebGpuRuntimeReceipt_t *runtimeReceipt ) {
	ralWebGpuResourceLayer_t *resources;
	if ( !sky || !runtimeReceipt || sky->active || sky->radianceActive )
		return qfalse;
	resources = RalWebGpu_RuntimeResources( sky->runtime, runtimeReceipt );
	if ( !resources ) return qfalse;
	DestroyRadianceJobs( sky, runtimeReceipt );
	DestroyWorldResources( sky, resources );
	sky->completedRadianceSliceMask = 0u;
	sky->completedRadianceRevision = 0u;
	sky->radianceFrameGeneration = 0u;
	sky->radianceTimeline = 0.0f;
	sky->materialUsesTime = qfalse;
	sky->physicalTimingState = RAL_WEBGPU_GPU_TIMING_UNAVAILABLE;
	sky->physicalGpuDurationNanoseconds = 0u;
	sky->radianceTimingState = RAL_WEBGPU_GPU_TIMING_UNAVAILABLE;
	sky->radianceGpuDurationNanoseconds = 0u;
	memset( &sky->radiancePlan, 0, sizeof( sky->radiancePlan ) );
	return Ral_SkyRadianceInit( RAL_BACKEND_WEBGPU,
		&sky->radianceState );
}

qboolean RalWebGpu_SkySampleResources( const ralWebGpuSky_t *sky,
		uintptr_t *outTextureIdentity, uintptr_t *outSamplerIdentity,
		uint64_t *outWorldLifecycleGeneration ) {
	const ralWebGpuResourceReceipt_t *texture;
	if ( !sky || !outTextureIdentity || !outSamplerIdentity
			|| !outWorldLifecycleGeneration || !sky->sampler
			|| !RalWebGpu_ResourceReceiptExact( &sky->samplerReceipt,
				&sky->samplerReceipt ) ) return qfalse;
	texture = sky->resourcesReady ? &sky->textureReceipts[WEBGPU_SKY_VIEW]
		: &sky->fallbackReceipt;
	if ( !RalWebGpu_ResourceReceiptExact( texture, texture ) ) return qfalse;
	*outTextureIdentity = texture->resourceIdentity;
	*outSamplerIdentity = sky->samplerReceipt.resourceIdentity;
	*outWorldLifecycleGeneration = sky->resourcesReady
		? sky->worldLifecycleGeneration : 0u;
	return *outTextureIdentity && *outSamplerIdentity;
}

qboolean RalWebGpu_SkyFallbackResources( const ralWebGpuSky_t *sky,
		uintptr_t *outTextureIdentity, uintptr_t *outSamplerIdentity ) {
	if ( !sky || !outTextureIdentity || !outSamplerIdentity || !sky->fallback
			|| !sky->sampler
			|| !RalWebGpu_ResourceReceiptExact( &sky->fallbackReceipt,
				&sky->fallbackReceipt )
			|| !RalWebGpu_ResourceReceiptExact( &sky->samplerReceipt,
				&sky->samplerReceipt ) ) return qfalse;
	*outTextureIdentity = sky->fallbackReceipt.resourceIdentity;
	*outSamplerIdentity = sky->samplerReceipt.resourceIdentity;
	return *outTextureIdentity && *outSamplerIdentity;
}

qboolean RalWebGpu_SkyRadianceFallbackResources( const ralWebGpuSky_t *sky,
		uintptr_t *outTextureIdentity, uintptr_t *outSamplerIdentity ) {
	if ( !sky || !outTextureIdentity || !outSamplerIdentity
			|| !sky->radianceFallback || !sky->sampler
			|| !RalWebGpu_ResourceReceiptExact( &sky->radianceFallbackReceipt,
				&sky->radianceFallbackReceipt )
			|| !RalWebGpu_ResourceReceiptExact( &sky->samplerReceipt,
				&sky->samplerReceipt ) ) return qfalse;
	*outTextureIdentity = sky->radianceFallbackReceipt.resourceIdentity;
	*outSamplerIdentity = sky->samplerReceipt.resourceIdentity;
	return *outTextureIdentity && *outSamplerIdentity;
}

static void FillParams( const renderAtmosphereSnapshot_t *snapshot,
		skyParams_t *params ) {
	const atmosphereSkyEnvironment_t *source = &snapshot->state.skyEnvironment;
	memset( params, 0, sizeof( *params ) );
	memcpy( params->sunDirectionAngularRadius, source->sunDirection,
		3u * sizeof( float ) );
	params->sunDirectionAngularRadius[3] = source->sunAngularRadius;
	memcpy( params->sunRadiance, source->sunRadiance, 3u * sizeof( float ) );
	memcpy( params->moonDirectionAngularRadius, source->moonDirection,
		3u * sizeof( float ) );
	params->moonDirectionAngularRadius[3] = source->moonAngularRadius;
	memcpy( params->moonRadiance, source->moonRadiance, 3u * sizeof( float ) );
	params->atmosphereRadii[0] = 6360.0f;
	params->atmosphereRadii[1] = 6460.0f;
	params->scatteringScale[0] = 1.0f;
	params->scatteringScale[1] = 1.0f;
	params->scatteringScale[2] = 1.0f;
	params->scatteringScale[3] = 0.8f;
	params->groundExposure[0] = source->ambientSh[0];
	params->groundExposure[1] = source->ambientSh[1];
	params->groundExposure[2] = source->ambientSh[2];
	params->groundExposure[3] = source->exposure;
}

qboolean RalWebGpu_SkyBegin( ralWebGpuSky_t *sky,
		const ralWebGpuRuntimeReceipt_t *runtimeReceipt,
		const renderAtmosphereSnapshot_t *snapshot,
		const ralSkyEnvironmentPlanReceipt_t *plan,
		ralWebGpuSkyReceipt_t *outReceipt ) {
	ralWebGpuResourceLayer_t *resources;
	ralWebGpuWriteReceipt_t write;
	ralWebGpuCommandReceipt_t recording, updated, executable;
	ralWebGpuSubmissionReceipt_t submission;
	ralWebGpuComputeDispatch_t dispatch;
	ralWebGpuPassDesc_t pass;
	ralWebGpuSkyReceipt_t receipt;
	skyParams_t params;
	uint32_t stage, width, height, workMask;
	const uint32_t physical = RAL_SKY_DIRTY_TRANSMITTANCE
		| RAL_SKY_DIRTY_MULTISCATTER | RAL_SKY_DIRTY_SKY_VIEW;
	if ( !sky || !runtimeReceipt || !snapshot || !plan || !outReceipt
			|| sky->active || !sky->resourcesReady ) return qfalse;
	workMask = plan->admittedWorkMask & physical;
	if ( !workMask || ( workMask & ( workMask - 1u ) ) ) return qfalse;
	stage = workMask == RAL_SKY_DIRTY_TRANSMITTANCE
		? WEBGPU_SKY_TRANSMITTANCE
		: workMask == RAL_SKY_DIRTY_MULTISCATTER
			? WEBGPU_SKY_MULTISCATTER : WEBGPU_SKY_VIEW;
	width = stage == WEBGPU_SKY_TRANSMITTANCE
		? WEBGPU_SKY_TRANSMITTANCE_WIDTH
		: stage == WEBGPU_SKY_MULTISCATTER
			? WEBGPU_SKY_MULTISCATTER_SIZE : WEBGPU_SKY_VIEW_WIDTH;
	height = stage == WEBGPU_SKY_TRANSMITTANCE
		? WEBGPU_SKY_TRANSMITTANCE_HEIGHT
		: stage == WEBGPU_SKY_MULTISCATTER
			? WEBGPU_SKY_MULTISCATTER_SIZE : WEBGPU_SKY_VIEW_HEIGHT;
	resources = RalWebGpu_RuntimeResources( sky->runtime, runtimeReceipt );
	if ( !resources ) return qfalse;
	FillParams( snapshot, &params );
	memset( &pass, 0, sizeof( pass ) );
	pass.kind = RAL_WEBGPU_PASS_COMPUTE;
	pass.timestampWrites = runtimeReceipt->core.caps.timestampPeriodNs > 0.0f
		? qtrue : qfalse;
	if ( !RalWebGpu_WriteBuffer( resources, sky->params, &sky->paramsReceipt,
			0u, &params, sizeof( params ), &write )
			|| !RalWebGpu_CommandBeginWithDesc( sky->command, &pass,
				&recording ) ) return qfalse;
	memset( &dispatch, 0, sizeof( dispatch ) );
	dispatch.pipelineIdentity = sky->pipelineReceipts[stage].pipelineIdentity;
	dispatch.bindGroupIdentities[0] = sky->bindGroups[stage];
	dispatch.bindGroupCount = 1u;
	dispatch.groupCountX = ( width + 7u ) / 8u;
	dispatch.groupCountY = ( height + 7u ) / 8u;
	dispatch.groupCountZ = 1u;
	dispatch.contentDigest = plan->sourceRevision ^ plan->frameGeneration
		^ (uint64_t)workMask;
	if ( !dispatch.contentDigest ) dispatch.contentDigest = workMask;
	if ( !RalWebGpu_CommandRecordComputeDispatch( sky->command, &recording,
			&dispatch, &updated ) ) {
		(void)RalWebGpu_CommandCancel( sky->command, &recording ); return qfalse;
	}
	if ( !RalWebGpu_CommandEnd( sky->command, &updated, &executable )
			|| !RalWebGpu_CommandSubmit( sky->command, &executable,
				&submission ) ) return qfalse;
	memset( &receipt, 0, sizeof( receipt ) );
	receipt.schemaVersion = RAL_WEBGPU_SKY_SCHEMA_VERSION;
	receipt.backendType = RAL_BACKEND_WEBGPU;
	receipt.executorGeneration = sky->generation;
	receipt.frameGeneration = plan->frameGeneration;
	receipt.worldLifecycleGeneration = sky->worldLifecycleGeneration;
	receipt.sourceRevision = plan->sourceRevision;
	receipt.workMask = workMask; receipt.command = executable;
	receipt.submission = submission; receipt.ready = qtrue;
	if ( !ReceiptValid( &receipt ) ) return qfalse;
	sky->inFlight = receipt; sky->active = qtrue;
	*outReceipt = receipt; return qtrue;
}

ralWebGpuAsyncStatus_t RalWebGpu_SkyPoll( ralWebGpuSky_t *sky,
		const ralWebGpuSkyReceipt_t *receipt, uint32_t *outCompletedWorkMask ) {
	ralWebGpuAsyncStatus_t status;
	if ( outCompletedWorkMask ) *outCompletedWorkMask = 0u;
	if ( !sky || !receipt || !outCompletedWorkMask || !sky->active
			|| !RalWebGpu_SkyReceiptExact( receipt, &sky->inFlight ) )
		return RAL_WEBGPU_ASYNC_FAILED;
	status = RalWebGpu_CommandPoll( sky->command, &receipt->submission );
	if ( status == RAL_WEBGPU_ASYNC_READY ) {
		if ( !RalWebGpu_CommandLastGpuTiming( sky->command,
				&sky->physicalTimingState,
				&sky->physicalGpuDurationNanoseconds ) )
			return RAL_WEBGPU_ASYNC_FAILED;
		*outCompletedWorkMask = receipt->workMask;
		sky->active = qfalse;
		memset( &sky->inFlight, 0, sizeof( sky->inFlight ) );
	}
	return status;
}

static qboolean RadianceReceiptValid(
		const ralWebGpuSkyRadianceReceipt_t *receipt ) {
	if ( !receipt || receipt->schemaVersion
			!= RAL_WEBGPU_SKY_RADIANCE_RECEIPT_SCHEMA_VERSION
			|| receipt->backendType != RAL_BACKEND_WEBGPU
			|| !receipt->executorGeneration || !receipt->frameGeneration
			|| !Ral_SkyRadianceReceiptValid( &receipt->plan )
			|| receipt->ready != qtrue ) return qfalse;
	if ( receipt->plan.currentProductRevision ) {
		if ( !Ral_SkyRadianceConsumerReceiptExact( &receipt->consumer,
				&receipt->consumer ) ) return qfalse;
	} else if ( receipt->consumer.schemaVersion != 0u ) return qfalse;
	if ( !receipt->sliceMask )
		return receipt->command.schemaVersion == 0u
			&& receipt->submission.schemaVersion == 0u;
	return receipt->productRevision
		&& receipt->sliceMask == receipt->plan.admittedSliceMask
		&& RalWebGpu_CommandReceiptExact( &receipt->command,
			&receipt->command )
		&& RalWebGpu_SubmissionReceiptExact( &receipt->submission,
			&receipt->submission )
		&& (receipt->command.dispatchCount > 0u || receipt->command.drawCount > 0u);
}

qboolean RalWebGpu_SkyRadianceReceiptExact(
		const ralWebGpuSkyRadianceReceipt_t *a,
		const ralWebGpuSkyRadianceReceipt_t *b ) {
	return RadianceReceiptValid( a ) && RadianceReceiptValid( b )
		&& !memcmp( a, b, sizeof( *a ) );
}

static qboolean CreateRadianceJob( ralWebGpuSky_t *sky,
		const ralWebGpuRuntimeReceipt_t *runtimeReceipt, uint32_t pipeline,
		const void *params, uint64_t paramsBytes,
		const ralWebGpuBrowserBindResource_t *resources, uint32_t resourceCount,
		uint32_t groupsX, uint32_t groupsY, uint32_t groupsZ,
		uint64_t digest, ralWebGpuCommandReceipt_t *recording ) {
	ralWebGpuResourceLayer_t *resourceLayer;
	ralWebGpuBufferDesc_t bufferDesc;
	ralWebGpuWriteReceipt_t write;
	ralWebGpuBrowserBindResource_t entries[9];
	ralWebGpuComputeDispatch_t dispatch;
	ralWebGpuCommandReceipt_t updated;
	uint32_t index;
	if ( !sky || !runtimeReceipt || !params || !paramsBytes || !resources
			|| !resourceCount || resourceCount >= 9u || !recording
			|| pipeline >= WEBGPU_SKY_RADIANCE_PIPELINE_COUNT
			|| sky->radianceJobCount >= WEBGPU_SKY_RADIANCE_MAX_JOBS )
		return qfalse;
	resourceLayer = RalWebGpu_RuntimeResources( sky->runtime, runtimeReceipt );
	if ( !resourceLayer ) return qfalse;
	index = sky->radianceJobCount;
	memset( &bufferDesc, 0, sizeof( bufferDesc ) );
	bufferDesc.size = paramsBytes;
	bufferDesc.usage = RAL_WEBGPU_BUFFER_UNIFORM
		| RAL_WEBGPU_BUFFER_COPY_DESTINATION;
	bufferDesc.memoryClass = RAL_ALLOCATION_DEVICE_LOCAL;
	if ( !RalWebGpu_CreateBuffer( resourceLayer, &bufferDesc,
			&sky->radianceParams[index],
			&sky->radianceParamReceipts[index] ) ) return qfalse;
	sky->radianceJobCount = index + 1u;
	if ( !RalWebGpu_WriteBuffer( resourceLayer,
				sky->radianceParams[index],
				&sky->radianceParamReceipts[index], 0u, params,
				paramsBytes, &write ) ) return qfalse;
	memcpy( entries, resources, resourceCount * sizeof( entries[0] ) );
	entries[resourceCount] = (ralWebGpuBrowserBindResource_t){
		pipeline == WEBGPU_SKY_RADIANCE_SOURCE ? 8u : 3u,
		RAL_WEBGPU_BROWSER_BIND_RESOURCE_BUFFER,
		sky->radianceParamReceipts[index].resourceIdentity, 0u, paramsBytes };
	if ( !RalWebGpu_BrowserBridgeCreateBindGroup( sky->bridge,
			sky->radiancePipelineReceipts[pipeline]
				.bindGroupLayoutIdentities[0], entries, resourceCount + 1u,
			&sky->radianceBindGroups[index] ) ) return qfalse;
	memset( &dispatch, 0, sizeof( dispatch ) );
	dispatch.pipelineIdentity =
		sky->radiancePipelineReceipts[pipeline].pipelineIdentity;
	dispatch.bindGroupIdentities[0] = sky->radianceBindGroups[index];
	dispatch.bindGroupCount = 1u;
	dispatch.groupCountX = groupsX;
	dispatch.groupCountY = groupsY;
	dispatch.groupCountZ = groupsZ;
	dispatch.contentDigest = digest ? digest : (uint64_t)index + 1u;
	if ( !RalWebGpu_CommandRecordComputeDispatch( sky->command, recording,
			&dispatch, &updated ) ) return qfalse;
	*recording = updated;
	return qtrue;
}

qboolean RalWebGpu_SkyRadianceBegin( ralWebGpuSky_t *sky,
		const ralWebGpuRuntimeReceipt_t *runtimeReceipt,
		ralWebGpuProduct_t *product, const renderSubmissionState_t *frontend,
		const ralSkyEnvironmentPlanReceipt_t *plan, ralAtmosphereTier_t tier,
		uint32_t eventMask, float timelineSeconds,
		ralWebGpuSkyRadianceReceipt_t *outReceipt ) {
	ralSkyTierBudgetReceipt_t budget;
	ralSkyRadianceRequest_t request;
	ralWebGpuSkyRadianceResources_t resources;
	skySourceParams_t sourceParams;
	ralWebGpuSkyRadianceReceipt_t receipt;
	ralWebGpuCommandReceipt_t recording, executable;
	ralWebGpuSubmissionReceipt_t submission;
	ralWebGpuPassDesc_t pass;
	uint32_t admittedMask;
	qboolean materialCapture = qfalse;
	uint32_t recordedCaptureFaces = 0u;
	float deltaSeconds;
	if ( !sky || !runtimeReceipt || !product || !frontend || !plan || !outReceipt
			|| sky->active || sky->radianceActive || !sky->resourcesReady
			|| !Ral_SkyEnvironmentPlanReceiptExact( plan, plan )
			|| !Ral_SkyTierBudget( tier, &budget ) ) return qfalse;
	deltaSeconds = timelineSeconds >= sky->radianceTimeline
		? timelineSeconds - sky->radianceTimeline : 0.0f;
	sky->radianceTimeline = timelineSeconds;
	if ( sky->radianceFrameGeneration == UINT64_MAX ) return qfalse;
	memset( &request, 0, sizeof( request ) );
	request.schemaVersion = RAL_SKY_RADIANCE_SCHEMA_VERSION;
	request.backendType = RAL_BACKEND_WEBGPU;
	request.frameGeneration = ++sky->radianceFrameGeneration;
	request.sourceRevision = plan->sourceRevision;
	request.completedProductRevision = sky->completedRadianceRevision;
	request.mode = plan->selectedMode;
	request.specularMipCount = WEBGPU_SKY_RADIANCE_MIPS;
	request.eventMask = eventMask;
    /* Tier selection can change the effective mode while the authored
     * source/requested mode stays physical (analytic -> weather/full).
     * Radiance owns the selected mode, so invalidate that product explicitly. */
    if (sky->radianceState.sourceRevision && sky->radianceState.mode != request.mode) {
        request.eventMask |= RAL_SKY_EVENT_SOURCE_CHANGE;
    }
	request.completedSliceMask = sky->completedRadianceSliceMask;
	request.maxSlicesPerFrame = budget.maxRadianceSlicesPerFrame;
	if (plan->selectedMode == RAL_SKY_ENVIRONMENT_PHYSICAL
		&& !Ral_SkyPhysicalSourceReady(plan, sky->resourcesReady))
		request.maxSlicesPerFrame = 0u;
	request.deltaSeconds = deltaSeconds;
	request.crossfadeSeconds = 0.35f;
	/* Opt in only for time-dependent ordered capture; a held material
	 * clock must not continually rewrite the same radiance product. */
	request.refreshIntervalSeconds = sky->materialUsesTime
		&& frontend->worldSnapshot.timeMs != sky->captureTimeMs ? 0.5f : 0.0f;
	sky->completedRadianceSliceMask = 0u;
	sky->completedRadianceRevision = 0u;
	if ( !Ral_SkyRadianceAdvance( &sky->radianceState, &request,
			&sky->radiancePlan ) ) return qfalse;
	if ( sky->radiancePlan.sourceChanged || sky->radiancePlan.refreshStarted || eventMask )
		sky->radianceIrradianceWriteStarted = qfalse;
	if ( sky->radiancePlan.targetPublished ) {
		if ( !RalWebGpu_ProductPublishSkyRadiance( product,
				runtimeReceipt ) ) return qfalse;
		sky->radianceIrradianceWriteStarted = qfalse;
	}
	memset( &receipt, 0, sizeof( receipt ) );
	receipt.schemaVersion = RAL_WEBGPU_SKY_RADIANCE_RECEIPT_SCHEMA_VERSION;
	receipt.backendType = RAL_BACKEND_WEBGPU;
	receipt.executorGeneration = sky->generation;
	receipt.frameGeneration = plan->frameGeneration;
	receipt.plan = sky->radiancePlan;
	if ( sky->radiancePlan.currentProductRevision ) {
		if ( !RalWebGpu_ProductSkyRadianceResources( product, runtimeReceipt,
				frontend, &resources )
				|| !Ral_SkyRadianceConsumerPlan( &sky->radiancePlan,
					( resources.currentRadianceIdentity
						? RAL_SKY_RADIANCE_PRODUCT_VISIBLE_RADIANCE
							| RAL_SKY_RADIANCE_PRODUCT_SPECULAR_RADIANCE : 0u )
						| ( resources.currentIrradianceIdentity
							? RAL_SKY_RADIANCE_PRODUCT_DIFFUSE_IRRADIANCE : 0u )
						| ( resources.currentShIdentity
							? RAL_SKY_RADIANCE_PRODUCT_AMBIENT_SH : 0u ),
					sky->radiancePlan.previousProductValid
						? ( resources.targetRadianceIdentity
							? RAL_SKY_RADIANCE_PRODUCT_VISIBLE_RADIANCE
								| RAL_SKY_RADIANCE_PRODUCT_SPECULAR_RADIANCE : 0u )
							| ( resources.targetIrradianceIdentity
								? RAL_SKY_RADIANCE_PRODUCT_DIFFUSE_IRRADIANCE : 0u )
							| ( resources.targetShIdentity
								? RAL_SKY_RADIANCE_PRODUCT_AMBIENT_SH : 0u )
						: 0u,
					&receipt.consumer ) ) return qfalse;
	}
	receipt.ready = qtrue;
	admittedMask = sky->radiancePlan.admittedSliceMask;
	if ( !admittedMask ) {
		if ( !RadianceReceiptValid( &receipt ) ) return qfalse;
		*outReceipt = receipt;
		return qtrue;
	}
	if ( !RalWebGpu_ProductPrepareMaterialResources( product, runtimeReceipt, frontend )
			|| !RalWebGpu_ProductSkyRadianceResources( product, runtimeReceipt,
			frontend, &resources ) ) return qfalse;
	if ( admittedMask & RAL_SKY_RADIANCE_CAPTURE_MASK ) {
		renderMaterialSnapshot_t material;
		char semanticName[MAX_QPATH];
		renderAtmosphereSnapshot_t atmosphere;
		const atmosphereEmitter_t *atmosphereEmitters;
		if ( !RenderSubmission_AtmosphereSnapshot( frontend, &atmosphere,
				&atmosphereEmitters )
				|| !Ral_SkyRadianceSourceParamsBuild( plan, atmosphere.state.cloudCover,
					resources.authoredReady ? RAL_SKY_SOURCE_AUTHORED_FACES
						: RAL_SKY_SOURCE_ANALYTIC, &sourceParams ) ) return qfalse;
		sky->materialUsesTime = qfalse;
		if(RenderSubmission_ActiveSkyMaterial(frontend,&material,semanticName)
			&& (material.stageCount || material.authoredSkySource.outerBox)) {
			sky->materialUsesTime = RenderSubmission_MaterialStagesUseTime(&material);
			if(admittedMask & 1u) sky->captureTimeMs=frontend->worldSnapshot.timeMs;
			if(!RalWebGpu_ProductSkyCaptureBegin(product,runtimeReceipt,frontend,&material,
					sky->captureTimeMs,admittedMask & RAL_SKY_RADIANCE_CAPTURE_MASK,
					sky->textureReceipts[WEBGPU_SKY_VIEW].resourceIdentity,
					sky->samplerReceipt.resourceIdentity,Ral_SkyPhysicalSourceReady(plan,sky->resourcesReady)?1.0f:0.0f)) return qfalse;
			sky->captureProduct=product; materialCapture=qtrue;
		}
	}
	/* ProductSkyRadianceResources validates any declared outer cube. An
	 * authored background can instead be a stage sky with no cube at all. */
	DestroyRadianceJobs( sky, runtimeReceipt );
	memset( &pass, 0, sizeof( pass ) );
	pass.kind = RAL_WEBGPU_PASS_COMPUTE;
	pass.timestampWrites = !materialCapture && runtimeReceipt->core.caps.timestampPeriodNs > 0.0f
		? qtrue : qfalse;
	if(materialCapture) {
		pass.kind=RAL_WEBGPU_PASS_RENDER; pass.targetIdentity=resources.sourceTextureIdentity;
		pass.colorLoadOp=RAL_WEBGPU_LOAD_CLEAR;
		while(!(admittedMask & (1u<<pass.targetArrayLayer))) ++pass.targetArrayLayer;
		// Per-pass timing cannot represent the entire multi-face capture.
		// Leave unavailable rather than reporting a single face as total cost.
	}
	if ( !RalWebGpu_CommandBeginWithDesc( sky->command, &pass,
			&recording ) ) goto fail_no_cancel;
	for ( uint32_t sliceIndex = 0u; sliceIndex < 27u; ++sliceIndex ) {
		const uint32_t slice = 1u << sliceIndex;
		ralWebGpuBrowserBindResource_t entries[8];
		skySliceParams_t sliceParams;
		uint32_t entryCount = 0u, pipeline, x, y, z;
		if ( !( admittedMask & slice ) ) continue;
		if(materialCapture && sliceIndex<6u) {
			ralWebGpuCommandReceipt_t updated;
			pass.targetArrayLayer=sliceIndex;
			if(recordedCaptureFaces && !RalWebGpu_CommandNextPass(sky->command,&recording,&pass,&updated)) goto fail;
			if(recordedCaptureFaces) recording=updated;
			if(!RalWebGpu_ProductSkyCaptureFace(product,runtimeReceipt,sky->command,sliceIndex,&recording,&updated)) goto fail;
			recording=updated; ++recordedCaptureFaces; continue;
		}
		if(materialCapture && pass.kind==RAL_WEBGPU_PASS_RENDER) {
			ralWebGpuCommandReceipt_t updated;
			memset(&pass,0,sizeof(pass)); pass.kind=RAL_WEBGPU_PASS_COMPUTE;
			if(!RalWebGpu_CommandNextPass(sky->command,&recording,&pass,&updated)) goto fail;
			recording=updated;
		}
		memset( entries, 0, sizeof( entries ) );
		memset( &sliceParams, 0, sizeof( sliceParams ) );
		if ( sliceIndex < 6u ) {
			uintptr_t fallbackTexture, fallbackSampler;
			static const uint32_t faceBindings[RAL_MATERIAL_SKY_FACE_COUNT] =
				{ 1u, 3u, 2u, 4u, 5u, 6u };
			sourceParams.meta[0] = sliceIndex;
			if ( !RalWebGpu_SkyFallbackResources( sky, &fallbackTexture,
					&fallbackSampler ) ) goto fail;
			entries[entryCount++] = (ralWebGpuBrowserBindResource_t){ 0u,
				RAL_WEBGPU_BROWSER_BIND_RESOURCE_TEXTURE,
				resources.sourceTextureIdentity, 0u, 0u };
			for ( uint32_t face = 0u; face < RAL_MATERIAL_SKY_FACE_COUNT; ++face )
				entries[entryCount++] = (ralWebGpuBrowserBindResource_t){
					faceBindings[face], RAL_WEBGPU_BROWSER_BIND_RESOURCE_TEXTURE,
					resources.authoredReady
						? resources.authoredTextureIdentities[face]
						: fallbackTexture, 0u, 0u };
			entries[entryCount++] = (ralWebGpuBrowserBindResource_t){ 7u,
				RAL_WEBGPU_BROWSER_BIND_RESOURCE_SAMPLER,
				resources.authoredReady ? resources.authoredSamplerIdentity
					: fallbackSampler, 0u, 0u };
			if ( !CreateRadianceJob( sky, runtimeReceipt,
					WEBGPU_SKY_RADIANCE_SOURCE, &sourceParams,
					sizeof( sourceParams ), entries, entryCount,
					( WIRED_SKY_SOURCE_SIZE + 7u ) / 8u,
					( WIRED_SKY_SOURCE_SIZE + 7u ) / 8u,
					1u, plan->sourceRevision ^ slice, &recording ) )
				goto fail;
			continue;
		}
		entries[entryCount++] = (ralWebGpuBrowserBindResource_t){ 0u,
			RAL_WEBGPU_BROWSER_BIND_RESOURCE_TEXTURE,
			resources.sourceTextureIdentity, 0u, 0u };
		entries[entryCount++] = (ralWebGpuBrowserBindResource_t){ 1u,
			RAL_WEBGPU_BROWSER_BIND_RESOURCE_SAMPLER,
			sky->samplerReceipt.resourceIdentity, 0u, 0u };
		if ( sliceIndex < 15u ) {
			if ( !sky->radianceIrradianceWriteStarted ) {
				ralWebGpuBrowserBindResource_t irradianceEntries[3] = {
					entries[0], entries[1],
					{ 2u, RAL_WEBGPU_BROWSER_BIND_RESOURCE_TEXTURE,
						resources.targetIrradianceIdentity, 0u, 0u } };
				if ( !CreateRadianceJob( sky, runtimeReceipt,
						WEBGPU_SKY_RADIANCE_IRRADIANCE, &sliceParams,
						sizeof( sliceParams ), irradianceEntries, 3u,
						( WIRED_SKY_IRRADIANCE_SIZE + 7u ) / 8u,
						( WIRED_SKY_IRRADIANCE_SIZE + 7u ) / 8u,
						6u, plan->sourceRevision ^ UINT64_C(0x100),
						&recording ) ) goto fail;
				sky->radianceIrradianceWriteStarted = qtrue;
			}
			pipeline = WEBGPU_SKY_RADIANCE_PROJECT_SH;
			entries[entryCount++] = (ralWebGpuBrowserBindResource_t){ 2u,
				RAL_WEBGPU_BROWSER_BIND_RESOURCE_BUFFER,
				resources.targetShIdentity, 0u,
				RAL_SKY_RADIANCE_AMBIENT_SH_COEFFICIENT_COUNT
					* 4u * sizeof( float ) };
			sliceParams.values[0] = sliceIndex - 6u;
			x = y = z = 1u;
		} else {
			const uint32_t mip = sliceIndex - 15u;
			pipeline = WEBGPU_SKY_RADIANCE_PREFILTER;
			entries[entryCount++] = (ralWebGpuBrowserBindResource_t){ 2u,
				RAL_WEBGPU_BROWSER_BIND_RESOURCE_TEXTURE,
				resources.targetRadianceIdentity, mip, 1u };
			sliceParams.values[0] = mip;
			sliceParams.values[1] = WEBGPU_SKY_RADIANCE_MIPS;
			x = y = ( ( WIRED_SKY_SPECULAR_SIZE >> mip ) + 7u ) / 8u;
			z = 6u;
		}
		if ( !CreateRadianceJob( sky, runtimeReceipt, pipeline, &sliceParams,
				sizeof( sliceParams ), entries, entryCount, x, y, z,
				plan->sourceRevision ^ slice, &recording ) ) goto fail;
	}
	if ( !RalWebGpu_CommandEnd( sky->command, &recording, &executable )
			|| !RalWebGpu_CommandSubmit( sky->command, &executable,
				&submission ) ) goto fail_no_cancel;
	receipt.productRevision = sky->radiancePlan.targetProductRevision;
	receipt.sliceMask = admittedMask;
	receipt.command = executable;
	receipt.submission = submission;
	if ( !RadianceReceiptValid( &receipt ) ) goto fail_no_cancel;
	sky->radianceInFlight = receipt;
	sky->radianceActive = qtrue;
	*outReceipt = receipt;
	return qtrue;

fail:
	(void)RalWebGpu_CommandCancel( sky->command, &recording );
fail_no_cancel:
	RalWebGpu_ProductSkyCaptureEnd(sky->captureProduct);
	sky->captureProduct=NULL;
	DestroyRadianceJobs( sky, runtimeReceipt );
	sky->radianceIrradianceWriteStarted = qfalse;
	return qfalse;
}

ralWebGpuAsyncStatus_t RalWebGpu_SkyRadiancePoll( ralWebGpuSky_t *sky,
		const ralWebGpuRuntimeReceipt_t *runtimeReceipt,
		const ralWebGpuSkyRadianceReceipt_t *receipt,
		uint32_t *outCompletedSliceMask, uint64_t *outCompletedRevision ) {
	ralWebGpuAsyncStatus_t status;
	if ( outCompletedSliceMask ) *outCompletedSliceMask = 0u;
	if ( outCompletedRevision ) *outCompletedRevision = 0u;
	if ( !sky || !runtimeReceipt || !receipt || !outCompletedSliceMask
			|| !outCompletedRevision || !sky->radianceActive
			|| !RalWebGpu_SkyRadianceReceiptExact( receipt,
				&sky->radianceInFlight ) ) return RAL_WEBGPU_ASYNC_FAILED;
	status = RalWebGpu_CommandPoll( sky->command, &receipt->submission );
	if ( status != RAL_WEBGPU_ASYNC_PENDING ) {
		if ( status == RAL_WEBGPU_ASYNC_READY ) {
			if ( !RalWebGpu_CommandLastGpuTiming( sky->command,
					&sky->radianceTimingState,
					&sky->radianceGpuDurationNanoseconds ) )
				status = RAL_WEBGPU_ASYNC_FAILED;
		}
		if ( status == RAL_WEBGPU_ASYNC_READY ) {
			*outCompletedSliceMask = receipt->sliceMask;
			*outCompletedRevision = receipt->productRevision;
			sky->completedRadianceSliceMask = receipt->sliceMask;
			sky->completedRadianceRevision = receipt->productRevision;
		}
		DestroyRadianceJobs( sky, runtimeReceipt );
		RalWebGpu_ProductSkyCaptureEnd(sky->captureProduct);
		sky->captureProduct=NULL;
		sky->radianceActive = qfalse;
		memset( &sky->radianceInFlight, 0,
			sizeof( sky->radianceInFlight ) );
	}
	return status;
}

qboolean RalWebGpu_SkyResourcesReady( const ralWebGpuSky_t *sky ) {
	return sky && sky->resourcesReady ? qtrue : qfalse;
}

qboolean RalWebGpu_SkyInFlight( const ralWebGpuSky_t *sky ) {
	return sky && sky->active ? qtrue : qfalse;
}

qboolean RalWebGpu_SkyLastPhysicalGpuTiming( const ralWebGpuSky_t *sky,
		ralWebGpuGpuTimingState_t *outState,
		uint64_t *outDurationNanoseconds ) {
	if ( !sky || !outState || !outDurationNanoseconds
			|| sky->physicalTimingState < RAL_WEBGPU_GPU_TIMING_UNAVAILABLE
			|| sky->physicalTimingState > RAL_WEBGPU_GPU_TIMING_FAILED )
		return qfalse;
	*outState = sky->physicalTimingState;
	*outDurationNanoseconds = sky->physicalGpuDurationNanoseconds;
	return qtrue;
}

qboolean RalWebGpu_SkyLastRadianceGpuTiming( const ralWebGpuSky_t *sky,
		ralWebGpuGpuTimingState_t *outState,
		uint64_t *outDurationNanoseconds ) {
	if ( !sky || !outState || !outDurationNanoseconds
			|| sky->radianceTimingState < RAL_WEBGPU_GPU_TIMING_UNAVAILABLE
			|| sky->radianceTimingState > RAL_WEBGPU_GPU_TIMING_FAILED )
		return qfalse;
	*outState = sky->radianceTimingState;
	*outDurationNanoseconds = sky->radianceGpuDurationNanoseconds;
	return qtrue;
}
