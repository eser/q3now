// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "ral_webgpu_renderer_module.h"
#include "ral_webgpu_browser_emscripten.h"
#include "ral_webgpu_lighting.h"
#include "ral_atmosphere_conformance.h"
#include "maps/map_format_registry.h"
#include "render_lighting_project_cook.h"
#include "render_lighting_sidecar.h"
#include "render_image_decode.h"
#include "render_material_script.h"

#include <limits.h>
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
	WEBGPU_PIPELINE_WORLD,
	WEBGPU_PIPELINE_ENTITY,
	WEBGPU_PIPELINE_EFFECT,
	WEBGPU_PIPELINE_UI,
	WEBGPU_PIPELINE_MSDF,
	WEBGPU_PIPELINE_COUNT
};

typedef struct {
	float	 eyeDensity[4];
	float	 colorVisibility[4];
	float	 heightCloud[4];
	uint32_t froxelGrid[4];
	float displayVisibility[4];
} wiredWebGpuAtmosphereUniform_t;

typedef struct {
	refimport_t							  imports;
	refexport_t							  exports;
	ralWebGpuBrowserModuleBorrow_t		  borrow;
	ralWebGpuPipeline_t					 *pipelines[WEBGPU_PIPELINE_COUNT];
	ralWebGpuPipelineReceipt_t			  pipelineReceipts[WEBGPU_PIPELINE_COUNT];
	ralWebGpuResource_t					 *atmosphereUniform;
	ralWebGpuResourceReceipt_t			  atmosphereUniformReceipt;
	ralWebGpuWriteReceipt_t				  atmosphereWriteReceipt;
	uintptr_t							  atmosphereBindGroups[3];
	uintptr_t							  worldMaterialBindGroup;
	uintptr_t							  entityLightingBindGroup;
	ralWebGpuAtmosphere_t				 *atmosphere;
	ralWebGpuWeather_t					 *weather;
	ralWebGpuProduct_t					 *product;
	renderSubmissionState_t				  frontend;
	renderMaterialScriptCatalog_t		  materialScripts;
	renderLightingSidecarReceipt_t		  lightingSidecar;
	renderIrradianceSidecarReceipt_t	  irradianceSidecar;
	ralWebGpuLighting_t				 *directionalLighting;
	ralWebGpuLightingReceipt_t		  directionalLightingReceipt;
	ralWebGpuFrameReceipt_t				  pendingFrame;
	ralWebGpuRendererModuleFrameReceipt_t pending;
	ralWebGpuRendererModuleFrameReceipt_t published;
	ralWebGpuFrontendPlanReceipt_t		  pendingProductPlan;
	glconfig_t							  config;
	const mapFile_t						 *loadedWorld;
	const mapFile_t						 *loadedWorlds[MAX_RENDER_WORLDS];
	int                                       activeWorldIndex;
	uint64_t							  moduleGeneration;
	uint64_t							  nextGeneration;
	uint64_t							  currentFrameGeneration;
	qhandle_t							  defaultMaterial;
	qboolean							  loaded;
	qboolean							  registered;
	qboolean							  frameOpen;
	qboolean							  pendingFrameReady;
	qboolean							  atmosphereComputePending;
	qboolean							  weatherComputePending;
	qboolean							  productSubmitted;
	int									  failureStage;
	int									  logChannel;
	cvar_t								 *brightness;
	cvar_t								  brightnessFallback;
} wiredWebGpuModuleState_t;

static wiredWebGpuModuleState_t s_module;
static uint64_t					s_moduleCounter;
refimport_t						ri;

static const byte s_white[4]	 = { 255u, 255u, 255u, 255u };

int WiredWebGpu_RendererFailureStage( void ) { return s_module.failureStage; }
static const char s_worldVertexWgsl[] =
	"struct VertexOut { @builtin(position) position: vec4<f32>,"
	" @location(0) color: vec4<f32>, @location(1) fogCoord: vec2<f32>,"
	" @location(2) emission: vec3<f32> };"
	"struct WorldMaterials { values: array<vec4<f32>> };"
	"@group(1) @binding(0) var<storage, read> worldMaterials: WorldMaterials;"
	"@vertex fn main(@location(0) p: vec3<f32>, @location(1) clipW: f32,"
	" @location(2) color: vec4<f32>, @location(3) fogCoord: vec2<f32>,"
	" @builtin(instance_index) materialIndex: u32) -> VertexOut {"
	" var out: VertexOut; out.position = vec4<f32>(p, clipW);"
	" out.color = color; out.fogCoord = fogCoord;"
	" out.emission = worldMaterials.values[materialIndex].rgb; return out; }\n";
static const char s_vertexWgsl[] = "struct VertexOut { @builtin(position) position: vec4<f32>,"
								   " @location(0) color: vec4<f32>, @location(1) fogCoord: vec2<f32> };"
								   "@vertex fn main(@location(0) p: vec3<f32>, @location(1) clipW: f32,"
								   " @location(2) color: vec4<f32>, @location(3) fogCoord: vec2<f32>) -> VertexOut {"
								   " var out: VertexOut; out.position = vec4<f32>(p, clipW);"
								   " out.color = color; out.fogCoord = fogCoord; return out; }\n";
static const char s_entityVertexWgsl[] =
	"struct VertexOut { @builtin(position) position: vec4<f32>,"
	" @location(0) color: vec4<f32>, @location(1) fogCoord: vec2<f32> };"
	"struct EntityLighting { values: array<vec4<f32>> };"
	"@group(1) @binding(0) var<storage, read> entityLighting: EntityLighting;"
	"@vertex fn main(@location(0) p: vec3<f32>, @location(1) clipW: f32,"
	" @location(2) color: vec4<f32>, @location(3) fogCoord: vec2<f32>,"
	" @location(4) normal: vec3<f32>,"
	" @builtin(instance_index) lightingIndex: u32) -> VertexOut {"
	" let base = lightingIndex * 4u;"
	" let c0 = entityLighting.values[base];"
	" let irradiance = select(vec3<f32>(1.0), max(c0.rgb"
	" + entityLighting.values[base+1u].rgb*normal.x"
	" + entityLighting.values[base+2u].rgb*normal.y"
	" + entityLighting.values[base+3u].rgb*normal.z, vec3<f32>(0.0)),"
	" c0.w > 0.5);"
	" var out: VertexOut; out.position = vec4<f32>(p, clipW);"
	" out.color = vec4<f32>(color.rgb * irradiance, color.a);"
	" out.fogCoord = fogCoord; return out; }\n";
static const char s_fragmentWgsl[] =
	"struct Atmosphere { eyeDensity: vec4<f32>, colorVisibility: vec4<f32>,"
	" heightCloud: vec4<f32>, froxelGrid: vec4<u32>, displayVisibility: vec4<f32> };"
	"struct Froxels { values: array<vec4<f32>> };"
	"@group(0) @binding(0) var<uniform> atmosphere: Atmosphere;"
	"@group(0) @binding(1) var<storage, read> froxels: Froxels;"
	"fn wired_display_visibility(c: vec3<f32>) -> vec3<f32> {"
	" var exposed=max(c,vec3<f32>(0.0))*atmosphere.displayVisibility.x;"
	" let l=dot(exposed,vec3<f32>(0.2126,0.7152,0.0722));"
	" if (l>0.0 && l<atmosphere.displayVisibility.z && atmosphere.displayVisibility.y!=1.0) {"
	" let curved=pow(l/atmosphere.displayVisibility.z,atmosphere.displayVisibility.y)"
	" *atmosphere.displayVisibility.z; exposed*=curved/l; } return exposed; }"
	"@fragment fn main(@location(0) color: vec4<f32>,"
	" @location(1) fogCoord: vec2<f32>, @builtin(position) pixel: vec4<f32>)"
	" -> @location(0) vec4<f32> {"
	" if (atmosphere.froxelGrid.z > 0u) {"
	" let x=min(u32(pixel.x)/16u,atmosphere.froxelGrid.x-1u);"
	" let y=min(u32(pixel.y)/16u,atmosphere.froxelGrid.y-1u);"
	" let farZ=max(atmosphere.colorVisibility.w,4.01);"
	" let slice=clamp(log(max(fogCoord.x,4.0)/4.0)/log(farZ/4.0),0.0,0.9999);"
	" let z=min(u32(slice*f32(atmosphere.froxelGrid.z)),atmosphere.froxelGrid.z-1u);"
	" let f=froxels.values[atmosphere.froxelGrid.w+(z*atmosphere.froxelGrid.y+y)*atmosphere.froxelGrid.x+x];"
	" return vec4<f32>(wired_display_visibility(color.rgb*f.w+f.rgb),color.a); }"
	" var density = atmosphere.eyeDensity.w;"
	" if (atmosphere.colorVisibility.w > 0.0) { density = max(density,"
	" 3.912023 / atmosphere.colorVisibility.w); }"
	" var lit = color.rgb; if (density > 0.0) {"
	" let heightWeight = exp(-atmosphere.heightCloud.y *"
	" max(fogCoord.y - atmosphere.heightCloud.x, 0.0));"
	" let transmittance = exp(-density * heightWeight * fogCoord.x);"
	" lit = mix(atmosphere.colorVisibility.rgb, lit, clamp(transmittance, 0.0, 1.0)); }"
	" return vec4<f32>(wired_display_visibility(lit), color.a); }\n";
static const char s_worldFragmentWgsl[] =
	"struct Atmosphere { eyeDensity: vec4<f32>, colorVisibility: vec4<f32>,"
	" heightCloud: vec4<f32>, froxelGrid: vec4<u32>, displayVisibility: vec4<f32> };"
	"struct Froxels { values: array<vec4<f32>> };"
	"@group(0) @binding(0) var<uniform> atmosphere: Atmosphere;"
	"@group(0) @binding(1) var<storage, read> froxels: Froxels;"
	"fn wired_display_visibility(c: vec3<f32>) -> vec3<f32> {"
	" var exposed=max(c,vec3<f32>(0.0))*atmosphere.displayVisibility.x;"
	" let l=dot(exposed,vec3<f32>(0.2126,0.7152,0.0722));"
	" if (l>0.0 && l<atmosphere.displayVisibility.z && atmosphere.displayVisibility.y!=1.0) {"
	" let curved=pow(l/atmosphere.displayVisibility.z,atmosphere.displayVisibility.y)"
	" *atmosphere.displayVisibility.z; exposed*=curved/l; } return exposed; }"
	"@fragment fn main(@location(0) color: vec4<f32>,"
	" @location(1) fogCoord: vec2<f32>, @location(2) emission: vec3<f32>,"
	" @builtin(position) pixel: vec4<f32>) -> @location(0) vec4<f32> {"
	" let surface = vec4<f32>(color.rgb + color.rgb * emission, color.a);"
	" if (atmosphere.froxelGrid.z > 0u) {"
	" let x=min(u32(pixel.x)/16u,atmosphere.froxelGrid.x-1u);"
	" let y=min(u32(pixel.y)/16u,atmosphere.froxelGrid.y-1u);"
	" let farZ=max(atmosphere.colorVisibility.w,4.01);"
	" let slice=clamp(log(max(fogCoord.x,4.0)/4.0)/log(farZ/4.0),0.0,0.9999);"
	" let z=min(u32(slice*f32(atmosphere.froxelGrid.z)),atmosphere.froxelGrid.z-1u);"
	" let f=froxels.values[atmosphere.froxelGrid.w+(z*atmosphere.froxelGrid.y+y)*atmosphere.froxelGrid.x+x];"
	" return vec4<f32>(wired_display_visibility(surface.rgb*f.w+f.rgb),surface.a); }"
	" var density = atmosphere.eyeDensity.w;"
	" if (atmosphere.colorVisibility.w > 0.0) { density = max(density,"
	" 3.912023 / atmosphere.colorVisibility.w); }"
	" var lit = surface.rgb; if (density > 0.0) {"
	" let heightWeight = exp(-atmosphere.heightCloud.y *"
	" max(fogCoord.y - atmosphere.heightCloud.x, 0.0));"
	" let transmittance = exp(-density * heightWeight * fogCoord.x);"
	" lit = mix(atmosphere.colorVisibility.rgb, lit, clamp(transmittance, 0.0, 1.0)); }"
	" return vec4<f32>(wired_display_visibility(lit), surface.a); }\n";
static const char s_uiVertexWgsl[]	 = "struct VertexOut { @builtin(position) position: vec4<f32>,"
									   " @location(0) uv: vec2<f32>, @location(1) color: vec4<f32> };"
									   "@vertex fn main(@location(0) p: vec3<f32>, @location(1) uv: vec2<f32>,"
									   " @location(2) color: vec4<f32>) -> VertexOut {"
									   " var out: VertexOut; out.position = vec4<f32>(p, 1.0);"
									   " out.uv = uv; out.color = color; return out; }\n";
static const char s_uiFragmentWgsl[] = "@group(0) @binding(0) var image: texture_2d<f32>;"
									   "@group(0) @binding(1) var imageSampler: sampler;"
									   "@fragment fn main(@location(0) uv: vec2<f32>, @location(1) color: vec4<f32>)"
									   " -> @location(0) vec4<f32> { return textureSample(image,imageSampler,uv)*color; }\n";
static const char s_msdfFragmentWgsl[] = "@group(0) @binding(0) var atlas: texture_2d<f32>;"
									   "@group(0) @binding(1) var atlasSampler: sampler;"
									   "fn median(r:f32,g:f32,b:f32)->f32{return max(min(r,g),min(max(r,g),b));}"
									   "@fragment fn main(@location(0) uv: vec2<f32>, @location(1) color: vec4<f32>)"
									   " -> @location(0) vec4<f32> { let s=textureSample(atlas,atlasSampler,uv);"
									   " let d=median(s.r,s.g,s.b)-0.5; let a=clamp(d/max(fwidth(d),0.0001)+0.5,0.0,1.0);"
									   " return vec4<f32>(color.rgb,color.a*a); }\n";

static uint64_t NextGeneration( void )
{
	if ( s_module.nextGeneration >= UINT64_MAX - 1u )
		return 0u;
	return ++s_module.nextGeneration;
}

static qboolean PlanAtmosphere( uint64_t frameGeneration, ralAtmospherePlanReceipt_t *outReceipt )
{
	renderAtmosphereSnapshot_t		snapshot;
	renderAtmosphereMediaSnapshot_t media;
	ralAtmospherePlanRequest_t		request;
	const atmosphereEmitter_t	   *emitters;
	const atmosphereMediaVolume_t  *volumes;
	if ( !RenderSubmission_AtmosphereSnapshot( &s_module.frontend, &snapshot, &emitters ) )
		return qfalse;
	(void)emitters;
	if ( !RenderSubmission_AtmosphereMediaSnapshot( &s_module.frontend, &media, &volumes ) )
		return qfalse;
	(void)volumes;
	memset( &request, 0, sizeof( request ) );
	request.schemaVersion	= RAL_ATMOSPHERE_PLAN_SCHEMA_VERSION;
	request.backendType		= RAL_BACKEND_WEBGPU;
	request.frameGeneration = frameGeneration;
	request.width			= s_module.borrow.width;
	request.height			= s_module.borrow.height;
	request.requestedTier = snapshot.active ? (ralAtmosphereTier_t)snapshot.state.qualityTier : RAL_ATMOSPHERE_TIER_OFF;
	request.localVolumeCount   = media.count;
	request.lightCount = RenderSubmission_AtmosphereLightCount( &s_module.frontend );
	if ( request.lightCount > RAL_ATMOSPHERE_MAX_LIGHTS )
		request.lightCount = RAL_ATMOSPHERE_MAX_LIGHTS;
	request.shadowedLightCount = RenderSubmission_AtmosphereShadowedLightCount( &s_module.frontend );
	if ( request.shadowedLightCount > RAL_ATMOSPHERE_MAX_SHADOWED_LIGHTS )
		request.shadowedLightCount = RAL_ATMOSPHERE_MAX_SHADOWED_LIGHTS;
	request.maxFroxelCount	   = RAL_WEBGPU_ATMOSPHERE_FROXEL_CAPACITY;
	request.maxLocalVolumes	   = RAL_ATMOSPHERE_MAX_VOLUMES;
	request.maxLights		   = RAL_ATMOSPHERE_MAX_LIGHTS;
	request.maxShadowedLights  = RAL_ATMOSPHERE_MAX_SHADOWED_LIGHTS;
	request.mediaActive = snapshot.active && ( snapshot.state.mediaDensity > 0.0f || snapshot.state.visibility > 0.0f ||
											   media.count > 0u );
	request.skyLightingActive			   = snapshot.active;
	request.cloudsRequested				   = snapshot.active && snapshot.state.cloudCover > 0.0f;
	request.capabilities.analyticComposite = qtrue;
	request.capabilities.compute		   = s_module.atmosphere ? qtrue : qfalse;
	request.capabilities.storageBuffers	   = s_module.atmosphere ? qtrue : qfalse;
	request.capabilities.temporalHistory   = s_module.atmosphere ? qtrue : qfalse;
	request.capabilities.volumetricShadows = qfalse;
	request.capabilities.fullClouds		   = s_module.atmosphere ? qtrue : qfalse;
	request.historyValid				   = RalWebGpu_AtmosphereHistoryValid( s_module.atmosphere );
	return Ral_AtmospherePlan( &request, outReceipt );
}

static void MarkFailed( const char *reason )
{
	if ( !s_module.exports.initFailed && s_module.imports.LogCh && s_module.logChannel >= 0 )
		s_module.imports.LogCh( s_module.logChannel, SEV_WARN,
								"Wired browser WebGPU RAL: lifecycle failure (%s, stage=%d)\n", reason,
								s_module.failureStage );
	s_module.exports.initFailed = qtrue;
}

static qboolean ReceiptValid( const ralWebGpuRendererModuleFrameReceipt_t *receipt )
{
	return receipt && receipt->schemaVersion == RAL_WEBGPU_RENDERER_MODULE_SCHEMA_VERSION &&
		   receipt->backendType == RAL_BACKEND_WEBGPU && receipt->moduleGeneration && receipt->frameGeneration &&
		   RenderSubmission_ReceiptExact( &receipt->frontend, &receipt->frontend ) &&
		   RalWebGpu_ProductFrameReceiptExact( &receipt->product, &receipt->product ) &&
		   Ral_DisplayVisibilityPlanValid( &receipt->displayVisibility ) &&
		   Ral_AtmospherePlanReceiptExact( &receipt->atmosphere, &receipt->atmosphere ) &&
		   Ral_AtmosphereWeatherReceiptExact( &receipt->weather, &receipt->weather ) &&
		   Ral_AtmosphereWeatherReceiptExact( &receipt->weather, &receipt->product.weather ) &&
		   receipt->frontend.ownerGeneration == receipt->moduleGeneration &&
		   receipt->frontend.frameGeneration == receipt->frameGeneration &&
		   receipt->product.frameGeneration == receipt->frameGeneration &&
		   receipt->atmosphere.frameGeneration == receipt->frameGeneration &&
		   ( receipt->atmosphere.selectedTier == RAL_ATMOSPHERE_TIER_FULL
				 ? ( RalWebGpu_AtmosphereReceiptExact( &receipt->atmosphereExecution, &receipt->atmosphereExecution ) &&
					 receipt->atmosphereExecution.frameGeneration == receipt->frameGeneration )
				 : receipt->atmosphereExecution.schemaVersion == 0u ) &&
		   ( receipt->weather.zeroWork
				 ? receipt->weatherExecution.schemaVersion == 0u
				 : ( RalWebGpu_WeatherReceiptExact( &receipt->weatherExecution, &receipt->weatherExecution ) &&
					 receipt->weatherExecution.frameGeneration == receipt->frameGeneration &&
					 receipt->product.weatherDrawCount == 1u ) ) &&
		   receipt->presented == qtrue && receipt->ready == qtrue;
}

Q_EXPORT qboolean RalWebGpu_RendererModuleFrameReceiptExact( const ralWebGpuRendererModuleFrameReceipt_t *a,
															 const ralWebGpuRendererModuleFrameReceipt_t *b )
{
	return ReceiptValid( a ) && ReceiptValid( b ) && !memcmp( a, b, sizeof( *a ) );
}

Q_EXPORT qboolean WiredWebGpu_GetFrameReceipt( ralWebGpuRendererModuleFrameReceipt_t *outReceipt )
{
	if ( !outReceipt || !ReceiptValid( &s_module.published ) )
		return qfalse;
	*outReceipt = s_module.published;
	return qtrue;
}

static qboolean Artifact( ralShaderArtifactAbi_t *artifact, ralShaderArtifactTarget_t target, const void *bytes,
						  uint32_t byteCount )
{
	memset( artifact, 0, sizeof( *artifact ) );
	artifact->target = target;
	if ( !bytes )
		return qtrue;
	artifact->byteCount = byteCount;
	return Ral_ShaderArtifactDigest( bytes, byteCount, &artifact->digest ) == ralSuccess;
}

static qboolean ShaderModule( ralShaderModuleAbi_t *module, uint32_t stage, const uint32_t spirv[4], const char *wgsl,
							  uint32_t wgslBytes, uint64_t generation )
{
	memset( module, 0, sizeof( *module ) );
	module->stage			   = stage;
	module->entryPoint		   = "main";
	module->sourceDigest.lane0 = generation;
	module->sourceDigest.lane1 = generation + 1u;
	return Artifact( &module->artifacts[RAL_SHADER_ARTIFACT_SPIRV], RAL_SHADER_ARTIFACT_SPIRV, spirv, 16u ) &&
		   Artifact( &module->artifacts[RAL_SHADER_ARTIFACT_MSL], RAL_SHADER_ARTIFACT_MSL, NULL, 0u ) &&
		   Artifact( &module->artifacts[RAL_SHADER_ARTIFACT_WGSL], RAL_SHADER_ARTIFACT_WGSL, wgsl, wgslBytes );
}

static qboolean CreatePipeline( uint32_t index )
{
	uint32_t						spirv[2][4] = { { 0x07230203u }, { 0x07230203u } };
	ralShaderModuleAbi_t			modules[2];
	ralShaderVertexInputAbi_t		shaderInputs[5];
	ralShaderBindingAbi_t			atmosphereBindings[3];
	ralShaderAbiManifest_t			manifest;
	ralShaderVariantAbi_t			variant;
	ralWebGpuWgslModule_t			wgsl[2];
	ralVertexBinding_t				binding;
	ralVertexAttribute_t			attributes[5];
	ralColorBlendAttachment_t		blend;
	const ralBindGroupLayout_t	   *groupLayouts[2];
	ralGraphicsPipelineCreateInfo_t graphics;
	ralWebGpuPipelineCreateInfo_t	info;
	qboolean isUi = index == WEBGPU_PIPELINE_UI || index == WEBGPU_PIPELINE_MSDF;
	const char					   *vertexWgsl = isUi
		? s_uiVertexWgsl : ( index == WEBGPU_PIPELINE_WORLD ? s_worldVertexWgsl
			: ( index == WEBGPU_PIPELINE_ENTITY ? s_entityVertexWgsl : s_vertexWgsl ) );
	const char					   *fragmentWgsl = index == WEBGPU_PIPELINE_MSDF
		? s_msdfFragmentWgsl : ( index == WEBGPU_PIPELINE_UI ? s_uiFragmentWgsl : ( index == WEBGPU_PIPELINE_WORLD
			? s_worldFragmentWgsl : s_fragmentWgsl ) );
	uint32_t						vertexWgslBytes =
		(uint32_t)( isUi ? sizeof( s_uiVertexWgsl ) - 1u
			: ( index == WEBGPU_PIPELINE_WORLD ? sizeof( s_worldVertexWgsl ) - 1u
				: ( index == WEBGPU_PIPELINE_ENTITY ? sizeof( s_entityVertexWgsl ) - 1u
					: sizeof( s_vertexWgsl ) - 1u ) ) );
	uint32_t fragmentWgslBytes =
		(uint32_t)( index == WEBGPU_PIPELINE_MSDF ? sizeof( s_msdfFragmentWgsl ) - 1u
			: ( index == WEBGPU_PIPELINE_UI ? sizeof( s_uiFragmentWgsl ) - 1u
			: ( index == WEBGPU_PIPELINE_WORLD ? sizeof( s_worldFragmentWgsl ) - 1u
				: sizeof( s_fragmentWgsl ) - 1u ) ) );
	uint32_t vertexInputCount = isUi ? 3u
		: ( index == WEBGPU_PIPELINE_ENTITY ? 5u : 4u );
	uint64_t generation		  = NextGeneration();
	if ( index >= WEBGPU_PIPELINE_COUNT || !generation ||
		 !ShaderModule( &modules[0], RAL_STAGE_VERTEX, spirv[0], vertexWgsl, vertexWgslBytes, generation ) ||
		 !ShaderModule( &modules[1], RAL_STAGE_FRAGMENT, spirv[1], fragmentWgsl, fragmentWgslBytes, generation + 2u ) )
		return qfalse;
	memset( shaderInputs, 0, sizeof( shaderInputs ) );
	shaderInputs[0].location = 0u;
	shaderInputs[0].format	 = RAL_FORMAT_R32G32B32_SFLOAT;
	shaderInputs[1].location = 1u;
	shaderInputs[1].format	 = isUi ? RAL_FORMAT_R32G32_SFLOAT : RAL_FORMAT_R32_SFLOAT;
	shaderInputs[2].location = 2u;
	shaderInputs[2].format	 = RAL_FORMAT_R8G8B8A8_UNORM;
	shaderInputs[3].location = 3u;
	shaderInputs[3].format	 = RAL_FORMAT_R32G32_SFLOAT;
	shaderInputs[4].location = 4u;
	shaderInputs[4].format   = RAL_FORMAT_R32G32B32_SFLOAT;
	memset( atmosphereBindings, 0, sizeof( atmosphereBindings ) );
	atmosphereBindings[0].set				   = 0u;
	atmosphereBindings[0].binding			   = 0u;
	atmosphereBindings[0].bindingClass		   = RAL_SHADER_BIND_UNIFORM_BUFFER;
	atmosphereBindings[0].arrayCount		   = 1u;
	atmosphereBindings[0].stageFlags		   = RAL_STAGE_FRAGMENT;
	atmosphereBindings[0].minBufferBindingSize = sizeof( wiredWebGpuAtmosphereUniform_t );
	atmosphereBindings[1].set				   = 0u;
	atmosphereBindings[1].binding			   = 1u;
	atmosphereBindings[1].bindingClass		   = RAL_SHADER_BIND_STORAGE_BUFFER_READ;
	atmosphereBindings[1].arrayCount		   = 1u;
	atmosphereBindings[1].stageFlags		   = RAL_STAGE_FRAGMENT;
	atmosphereBindings[1].minBufferBindingSize = (uint64_t)RAL_WEBGPU_ATMOSPHERE_FROXEL_CAPACITY * 6u * 16u;
	atmosphereBindings[2].set                   = 1u;
	atmosphereBindings[2].binding               = 0u;
	atmosphereBindings[2].bindingClass          = RAL_SHADER_BIND_STORAGE_BUFFER_READ;
	atmosphereBindings[2].arrayCount            = 1u;
	atmosphereBindings[2].stageFlags            = RAL_STAGE_VERTEX;
	atmosphereBindings[2].minBufferBindingSize  = index == WEBGPU_PIPELINE_WORLD
		? (uint64_t)RENDER_SUBMISSION_MAX_WORLD_BATCHES * 4u * sizeof( float )
		: (uint64_t)RENDER_SUBMISSION_MAX_ENTITIES
			* RENDER_SUBMISSION_MAX_MODEL_BATCHES * 16u * sizeof( float );
	memset( &manifest, 0, sizeof( manifest ) );
	manifest.schemaVersion	  = RAL_SHADER_ABI_SCHEMA_VERSION;
	manifest.generation		  = generation;
	manifest.pipelineKind	  = RAL_SHADER_PIPELINE_GRAPHICS;
	manifest.modules		  = modules;
	manifest.moduleCount	  = 2u;
	manifest.vertexInputs	  = shaderInputs;
	manifest.vertexInputCount = vertexInputCount;
	if ( isUi ) {
		memset( atmosphereBindings, 0, sizeof( atmosphereBindings ) );
		atmosphereBindings[0].set = 0u;
		atmosphereBindings[0].binding = 0u;
		atmosphereBindings[0].bindingClass = RAL_SHADER_BIND_SAMPLED_TEXTURE;
		atmosphereBindings[0].arrayCount = 1u;
		atmosphereBindings[0].stageFlags = RAL_STAGE_FRAGMENT;
		atmosphereBindings[0].viewDimension = RAL_SHADER_VIEW_2D;
		atmosphereBindings[0].sampleType = RAL_SHADER_SAMPLE_FLOAT;
		atmosphereBindings[1].set = 0u;
		atmosphereBindings[1].binding = 1u;
		atmosphereBindings[1].bindingClass = RAL_SHADER_BIND_FILTERING_SAMPLER;
		atmosphereBindings[1].arrayCount = 1u;
		atmosphereBindings[1].stageFlags = RAL_STAGE_FRAGMENT;
		manifest.bindings = atmosphereBindings;
		manifest.bindingCount = 2u;
	} else {
		manifest.bindings	  = atmosphereBindings;
		manifest.bindingCount = ( index == WEBGPU_PIPELINE_WORLD
			|| index == WEBGPU_PIPELINE_ENTITY ) ? 3u : 2u;
	}
	wgsl[0]		  = (ralWebGpuWgslModule_t){ vertexWgsl, vertexWgslBytes };
	wgsl[1]		  = (ralWebGpuWgslModule_t){ fragmentWgsl, fragmentWgslBytes };
	binding		  = (ralVertexBinding_t){ 0u, sizeof( renderWorldVertex_t ), RAL_VERTEX_INPUT_PER_VERTEX };
	attributes[0] = (ralVertexAttribute_t){ 0u, 0u, RAL_FORMAT_R32G32B32_SFLOAT, 0u };
	attributes[1] = isUi
						? (ralVertexAttribute_t){ 1u, 0u, RAL_FORMAT_R32G32_SFLOAT,
												  (uint32_t)offsetof( renderWorldVertex_t, texCoord ) }
						: (ralVertexAttribute_t){ 1u, 0u, RAL_FORMAT_R32_SFLOAT,
												  (uint32_t)offsetof( renderWorldVertex_t, texCoord ) };
	attributes[2] =
		(ralVertexAttribute_t){ 2u, 0u, RAL_FORMAT_R8G8B8A8_UNORM, (uint32_t)offsetof( renderWorldVertex_t, color ) };
	attributes[3] = (ralVertexAttribute_t){ 3u, 0u, RAL_FORMAT_R32G32_SFLOAT,
											(uint32_t)offsetof( renderWorldVertex_t, lightmapCoord ) };
	attributes[4] = (ralVertexAttribute_t){ 4u, 0u, RAL_FORMAT_R32G32B32_SFLOAT,
		(uint32_t)offsetof( renderWorldVertex_t, normal ) };
	memset( &blend, 0, sizeof( blend ) );
	blend.writeMask			= RAL_COLOR_WRITE_ALL;
	blend.writeMaskExplicit = qtrue;
	if ( isUi ) {
		blend.blendEnable = qtrue;
		blend.srcColor = RAL_BLEND_SRC_ALPHA;
		blend.dstColor = RAL_BLEND_ONE_MINUS_SRC_ALPHA;
		blend.colorOp = RAL_BLEND_OP_ADD;
		blend.srcAlpha = RAL_BLEND_ONE;
		blend.dstAlpha = RAL_BLEND_ONE_MINUS_SRC_ALPHA;
		blend.alphaOp = RAL_BLEND_OP_ADD;
	}
	memset( &graphics, 0, sizeof( graphics ) );
	graphics.vertexBindings		 = &binding;
	graphics.numVertexBindings	 = 1u;
	graphics.vertexAttributes	 = attributes;
	graphics.numVertexAttributes = vertexInputCount;
	graphics.topology			 = RAL_TOPOLOGY_TRIANGLE_LIST;
	graphics.raster.polygonMode	 = RAL_POLYGON_FILL;
	graphics.raster.lineWidth	 = 1.0f;
	graphics.colorBlends		 = &blend;
	graphics.numColorBlends		 = 1u;
	graphics.colorFormats[0]	 = RAL_FORMAT_B8G8R8A8_UNORM;
	graphics.numColorFormats	 = 1u;
	graphics.sampleCount		 = 1u;
	if ( isUi ) {
		groupLayouts[0] = (const ralBindGroupLayout_t *)(uintptr_t)1u;
		graphics.bindGroupLayouts = groupLayouts;
		graphics.numBindGroupLayouts = 1u;
	} else {
		groupLayouts[0]				 = (const ralBindGroupLayout_t *)(uintptr_t)1u;
		groupLayouts[1]				 = (const ralBindGroupLayout_t *)(uintptr_t)2u;
		graphics.bindGroupLayouts	 = groupLayouts;
		graphics.numBindGroupLayouts = ( index == WEBGPU_PIPELINE_WORLD
			|| index == WEBGPU_PIPELINE_ENTITY ) ? 2u : 1u;
	}
	memset( &variant, 0, sizeof( variant ) );
	variant.artifactTarget = RAL_SHADER_ARTIFACT_WGSL;
	variant.generation	   = generation;
	if ( Ral_GraphicsPipelineSemanticDigest( &graphics, &variant.semanticStateDigest ) != ralSuccess )
		return qfalse;
	memset( &info, 0, sizeof( info ) );
	info.manifest	   = &manifest;
	info.variant	   = &variant;
	info.modules	   = wgsl;
	info.moduleCount   = 2u;
	info.graphicsState = &graphics;
	info.generation	   = generation;
	return RalWebGpu_BrowserBridgeApplyPipelineInfo( s_module.borrow.bridge, &info ) &&
		   RalWebGpu_RuntimeCreatePipeline( s_module.borrow.runtime, &s_module.borrow.runtimeReceipt, &info,
											&s_module.pipelines[index], &s_module.pipelineReceipts[index] ) &&
		   RalWebGpu_RuntimeGetReceipt( s_module.borrow.runtime, &s_module.borrow.runtimeReceipt );
}

static qboolean CreateAtmosphereOwners( void )
{
	ralWebGpuResourceLayer_t	  *resources;
	ralWebGpuBufferDesc_t		   desc;
	ralWebGpuBrowserBindResource_t entries[2];
	resources = RalWebGpu_RuntimeResources( s_module.borrow.runtime, &s_module.borrow.runtimeReceipt );
	if ( !resources )
		return qfalse;
	memset( &desc, 0, sizeof( desc ) );
	desc.size		 = sizeof( wiredWebGpuAtmosphereUniform_t );
	desc.usage		 = RAL_WEBGPU_BUFFER_UNIFORM | RAL_WEBGPU_BUFFER_COPY_DESTINATION;
	desc.memoryClass = RAL_ALLOCATION_DEVICE_LOCAL;
	if ( !RalWebGpu_CreateBuffer( resources, &desc, &s_module.atmosphereUniform, &s_module.atmosphereUniformReceipt ) )
		return qfalse;
	memset( entries, 0, sizeof( entries ) );
	entries[0].binding			= 0u;
	entries[0].kind				= RAL_WEBGPU_BROWSER_BIND_RESOURCE_BUFFER;
	entries[0].resourceIdentity = s_module.atmosphereUniformReceipt.resourceIdentity;
	entries[0].byteSize			= sizeof( wiredWebGpuAtmosphereUniform_t );
	entries[1].binding			= 1u;
	entries[1].kind				= RAL_WEBGPU_BROWSER_BIND_RESOURCE_BUFFER;
	entries[1].resourceIdentity = RalWebGpu_AtmosphereArenaIdentity( s_module.atmosphere );
	entries[1].byteSize			= RalWebGpu_AtmosphereArenaBytes( s_module.atmosphere );
	if ( !entries[1].resourceIdentity || !entries[1].byteSize )
		return qfalse;
	for ( uint32_t i = 0u; i < 3u; ++i ) {
		const uint32_t expectedLayouts = ( i == WEBGPU_PIPELINE_WORLD
			|| i == WEBGPU_PIPELINE_ENTITY ) ? 2u : 1u;
		if ( s_module.pipelineReceipts[i].bindGroupLayoutCount != expectedLayouts ||
			 !RalWebGpu_BrowserBridgeCreateBindGroup( s_module.borrow.bridge,
													  s_module.pipelineReceipts[i].bindGroupLayoutIdentities[0],
													  entries, 2u, &s_module.atmosphereBindGroups[i] ) )
			return qfalse;
	}
	return qtrue;
}

static qboolean CreateWorldMaterialOwner( void )
{
	ralWebGpuBrowserBindResource_t entry;
	uintptr_t identity;
	uint64_t byteSize;
	if ( !s_module.product
		|| s_module.pipelineReceipts[WEBGPU_PIPELINE_WORLD].bindGroupLayoutCount != 2u
		|| !RalWebGpu_ProductGetWorldMaterialBuffer( s_module.product,
			&s_module.borrow.runtimeReceipt, &identity, &byteSize ) ) return qfalse;
	memset( &entry, 0, sizeof( entry ) );
	entry.binding = 0u;
	entry.kind = RAL_WEBGPU_BROWSER_BIND_RESOURCE_BUFFER;
	entry.resourceIdentity = identity;
	entry.byteSize = byteSize;
	return RalWebGpu_BrowserBridgeCreateBindGroup( s_module.borrow.bridge,
		s_module.pipelineReceipts[WEBGPU_PIPELINE_WORLD]
			.bindGroupLayoutIdentities[1], &entry, 1u,
		&s_module.worldMaterialBindGroup );
}

static qboolean CreateEntityLightingOwner( void )
{
	ralWebGpuBrowserBindResource_t entry;
	uintptr_t identity;
	uint64_t byteSize;
	if ( !s_module.product
		|| s_module.pipelineReceipts[WEBGPU_PIPELINE_ENTITY].bindGroupLayoutCount != 2u
		|| !RalWebGpu_ProductGetEntityLightingBuffer( s_module.product,
			&s_module.borrow.runtimeReceipt, &identity, &byteSize ) )
		return qfalse;
	memset( &entry, 0, sizeof( entry ) );
	entry.binding = 0u;
	entry.kind = RAL_WEBGPU_BROWSER_BIND_RESOURCE_BUFFER;
	entry.resourceIdentity = identity;
	entry.byteSize = byteSize;
	return RalWebGpu_BrowserBridgeCreateBindGroup( s_module.borrow.bridge,
		s_module.pipelineReceipts[WEBGPU_PIPELINE_ENTITY]
			.bindGroupLayoutIdentities[1], &entry, 1u,
		&s_module.entityLightingBindGroup );
}

static qboolean UpdateAtmosphereUniform( const ralAtmospherePlanReceipt_t *plan,
		const ralDisplayVisibilityPlan_t *visibility )
{
	ralWebGpuResourceLayer_t	  *resources;
	renderAtmosphereSnapshot_t	   snapshot;
	renderWorldSnapshot_t		   world;
	const atmosphereEmitter_t	  *emitters;
	wiredWebGpuAtmosphereUniform_t uniform;
	float						   sunWeight, lightning;
	if ( !plan || !Ral_DisplayVisibilityPlanValid( visibility )
			|| !s_module.atmosphereUniform || !RenderSubmission_WorldSnapshot( &s_module.frontend, &world ) ||
		 !RenderSubmission_AtmosphereSnapshot( &s_module.frontend, &snapshot, &emitters ) )
		return qfalse;
	(void)emitters;
	memset( &uniform, 0, sizeof( uniform ) );
	uniform.displayVisibility[0] = visibility->exposureScale;
	uniform.displayVisibility[1] = visibility->shadowExponent;
	uniform.displayVisibility[2] = visibility->shadowPivot;
	uniform.displayVisibility[3] = visibility->userBrightness;
	memcpy( uniform.eyeDensity, world.viewOrigin, 3u * sizeof( float ) );
	if ( snapshot.active && snapshot.state.qualityTier >= ATMOSPHERE_QUALITY_ANALYTIC ) {
		uniform.eyeDensity[3]	   = snapshot.state.mediaDensity;
		uniform.colorVisibility[3] = snapshot.state.visibility;
		sunWeight = snapshot.state.sunIntensity * ( 1.0f - snapshot.state.cloudCover * snapshot.state.cloudShadow );
		lightning = snapshot.state.lightning;
		for ( uint32_t channel = 0u; channel < 3u; ++channel )
			uniform.colorVisibility[channel] =
				fminf( 16.0f, snapshot.state.ambientColor[channel] + 0.08f * sunWeight + lightning );
		uniform.heightCloud[0] = snapshot.state.bounds[2];
		uniform.heightCloud[1] = snapshot.state.mediaHeightFalloff;
		uniform.heightCloud[2] = snapshot.state.cloudCover;
		uniform.heightCloud[3] = snapshot.state.cloudShadow;
	}
	if ( plan->selectedTier == RAL_ATMOSPHERE_TIER_FULL ) {
		uniform.froxelGrid[0] = plan->froxelWidth;
		uniform.froxelGrid[1] = plan->froxelHeight;
		uniform.froxelGrid[2] = plan->froxelDepth;
		uniform.froxelGrid[3] = RalWebGpu_AtmosphereIntegratedBase( s_module.atmosphere );
	}
	resources = RalWebGpu_RuntimeResources( s_module.borrow.runtime, &s_module.borrow.runtimeReceipt );
	return resources &&
		   RalWebGpu_WriteBuffer( resources, s_module.atmosphereUniform, &s_module.atmosphereUniformReceipt, 0u,
								  &uniform, sizeof( uniform ), &s_module.atmosphereWriteReceipt );
}

static void DestroyDirectionalLighting( void ) {
	ralWebGpuResourceLayer_t *resources = NULL;
	if ( s_module.borrow.runtime )
		resources = RalWebGpu_RuntimeResources( s_module.borrow.runtime,
			&s_module.borrow.runtimeReceipt );
	if ( s_module.directionalLighting && resources )
		(void)RalWebGpu_LightingDestroy( resources, s_module.directionalLighting,
			&s_module.directionalLightingReceipt );
	s_module.directionalLighting = NULL;
	memset( &s_module.directionalLightingReceipt, 0,
		sizeof( s_module.directionalLightingReceipt ) );
}

static qboolean UploadDirectionalLighting( void ) {
	const renderDirectionalLightingRecord_t *record;
	ralWebGpuResourceLayer_t *resources;
	ralLightingRuntimePlan_t plan;
	ralStaticLightingCapabilities_t capabilities = { qtrue, qtrue, qtrue, qtrue };
	uint64_t digest;
	DestroyDirectionalLighting();
	if ( s_module.lightingSidecar.status ==
			RENDER_LIGHTING_SIDECAR_MISSING_COMPATIBILITY ) return qtrue;
	resources = RalWebGpu_RuntimeResources( s_module.borrow.runtime,
		&s_module.borrow.runtimeReceipt );
	if ( !resources || !RenderSubmission_DirectionalLightingSnapshot(
			&s_module.frontend, &record, &digest ) || !record || !digest
			|| !Ral_LightingRuntimePlanBuild( RAL_BACKEND_WEBGPU,
				NextGeneration(), record->ownedArtifactBytes,
				record->artifactByteLength, &record->artifact, &capabilities,
				&plan ) ) return qfalse;
	return RalWebGpu_LightingUpload( resources, record->ownedArtifactBytes,
		record->artifactByteLength, &plan, &s_module.directionalLighting,
		&s_module.directionalLightingReceipt );
}

static void DestroyOwners( void )
{
	DestroyDirectionalLighting();
	if ( s_module.worldMaterialBindGroup )
		RalWebGpu_BrowserBridgeDestroyBindGroup( s_module.borrow.bridge,
			s_module.worldMaterialBindGroup );
	s_module.worldMaterialBindGroup = 0u;
	if ( s_module.entityLightingBindGroup )
		RalWebGpu_BrowserBridgeDestroyBindGroup( s_module.borrow.bridge,
			s_module.entityLightingBindGroup );
	s_module.entityLightingBindGroup = 0u;
	if ( s_module.product )
		RalWebGpu_ProductDestroy( s_module.product, &s_module.borrow.runtimeReceipt );
	s_module.product = NULL;
	if ( s_module.weather ) {
		RalWebGpu_WeatherDestroy( s_module.weather, &s_module.borrow.runtimeReceipt );
		s_module.weather = NULL;
		(void)RalWebGpu_RuntimeGetReceipt( s_module.borrow.runtime, &s_module.borrow.runtimeReceipt );
	}
	for ( uint32_t i = 3u; i > 0u; --i )
		if ( s_module.atmosphereBindGroups[i - 1u] )
			RalWebGpu_BrowserBridgeDestroyBindGroup( s_module.borrow.bridge, s_module.atmosphereBindGroups[i - 1u] );
	memset( s_module.atmosphereBindGroups, 0, sizeof( s_module.atmosphereBindGroups ) );
	if ( s_module.atmosphereUniform ) {
		ralWebGpuResourceLayer_t *resources =
			RalWebGpu_RuntimeResources( s_module.borrow.runtime, &s_module.borrow.runtimeReceipt );
		if ( resources )
			(void)RalWebGpu_DestroyResource( resources, s_module.atmosphereUniform,
											 &s_module.atmosphereUniformReceipt );
	}
	s_module.atmosphereUniform = NULL;
	memset( &s_module.atmosphereUniformReceipt, 0, sizeof( s_module.atmosphereUniformReceipt ) );
	memset( &s_module.atmosphereWriteReceipt, 0, sizeof( s_module.atmosphereWriteReceipt ) );
	if ( s_module.atmosphere ) {
		RalWebGpu_AtmosphereDestroy( s_module.atmosphere, &s_module.borrow.runtimeReceipt );
		s_module.atmosphere = NULL;
		(void)RalWebGpu_RuntimeGetReceipt( s_module.borrow.runtime, &s_module.borrow.runtimeReceipt );
	}
	for ( uint32_t i = WEBGPU_PIPELINE_COUNT; i > 0u; --i ) {
		uint32_t index = i - 1u;
		if ( s_module.pipelines[index] ) {
			(void)RalWebGpu_RuntimeDestroyPipeline( s_module.borrow.runtime, s_module.pipelines[index],
													&s_module.pipelineReceipts[index] );
			(void)RalWebGpu_RuntimeGetReceipt( s_module.borrow.runtime, &s_module.borrow.runtimeReceipt );
		}
	}
	memset( s_module.pipelines, 0, sizeof( s_module.pipelines ) );
	memset( s_module.pipelineReceipts, 0, sizeof( s_module.pipelineReceipts ) );
}

static qboolean InitializeOwners( void )
{
	s_module.failureStage = 201;
	if ( !RalWebGpu_BrowserModuleBorrow( &s_module.borrow ) )
		return qfalse;
	for ( uint32_t i = 0u; i < WEBGPU_PIPELINE_COUNT; ++i ) {
		s_module.failureStage = 210 + (int)i;
		if ( !CreatePipeline( i ) ) {
			DestroyOwners();
			return qfalse;
		}
	}
	s_module.failureStage = 220;
	if ( !RalWebGpu_AtmosphereCreate( s_module.borrow.runtime, &s_module.borrow.runtimeReceipt, s_module.borrow.bridge,
									  NextGeneration(), &s_module.atmosphere ) ) {
		DestroyOwners();
		return qfalse;
	}
	s_module.failureStage = 221;
	if ( !RalWebGpu_WeatherCreate( s_module.borrow.runtime, &s_module.borrow.runtimeReceipt, s_module.borrow.bridge,
								   NextGeneration(), &s_module.weather ) ) {
		s_module.failureStage = 2210 + (int)RalWebGpu_WeatherLastFailureStage();
		DestroyOwners();
		return qfalse;
	}
	s_module.failureStage = 222;
	if ( !CreateAtmosphereOwners() ) {
		DestroyOwners();
		return qfalse;
	}
	s_module.failureStage = 223;
	if ( !RalWebGpu_ProductCreate( s_module.borrow.runtime, &s_module.borrow.runtimeReceipt,
								   &s_module.pipelineReceipts[WEBGPU_PIPELINE_WORLD], s_module.moduleGeneration,
								   &s_module.product ) ) {
		DestroyOwners();
		return qfalse;
	}
	s_module.failureStage = 224;
	if ( !CreateWorldMaterialOwner()
		|| !RalWebGpu_ProductSetWorldMaterialBindGroup( s_module.product,
			&s_module.borrow.runtimeReceipt, s_module.worldMaterialBindGroup ) ) {
		DestroyOwners();
		return qfalse;
	}
	s_module.failureStage = 2240;
	if ( !RalWebGpu_ProductSetEntityPipeline( s_module.product, &s_module.borrow.runtimeReceipt,
											  &s_module.pipelineReceipts[WEBGPU_PIPELINE_ENTITY] ) ) {
		DestroyOwners();
		return qfalse;
	}
	s_module.failureStage = 2241;
	if ( !CreateEntityLightingOwner()
		|| !RalWebGpu_ProductSetEntityLightingBindGroup( s_module.product,
			&s_module.borrow.runtimeReceipt, s_module.entityLightingBindGroup ) ) {
		DestroyOwners();
		return qfalse;
	}
	s_module.failureStage = 225;
	if ( !RalWebGpu_ProductSetEffectPipeline( s_module.product, &s_module.borrow.runtimeReceipt,
											  &s_module.pipelineReceipts[WEBGPU_PIPELINE_EFFECT] ) ) {
		DestroyOwners();
		return qfalse;
	}
	s_module.failureStage = 226;
	if ( !RalWebGpu_ProductSetUiPipelines( s_module.product, &s_module.borrow.runtimeReceipt,
										   &s_module.pipelineReceipts[WEBGPU_PIPELINE_UI],
										   &s_module.pipelineReceipts[WEBGPU_PIPELINE_MSDF] ) ) {
		DestroyOwners();
		return qfalse;
	}
	s_module.failureStage = 227;
	if ( !RalWebGpu_ProductSetAtmosphereBindGroups( s_module.product, &s_module.borrow.runtimeReceipt,
													s_module.atmosphereBindGroups[0], s_module.atmosphereBindGroups[1],
													s_module.atmosphereBindGroups[2] ) ) {
		DestroyOwners();
		return qfalse;
	}
	s_module.failureStage = 228;
	if ( !RalWebGpu_ProductSetViewport( s_module.product, &s_module.borrow.runtimeReceipt, s_module.borrow.width,
										s_module.borrow.height ) ) {
		DestroyOwners();
		return qfalse;
	}
	s_module.failureStage = 0;
	memset( &s_module.config, 0, sizeof( s_module.config ) );
	(void)snprintf( s_module.config.renderer_string, sizeof( s_module.config.renderer_string ),
					"Wired browser WebGPU RAL" );
	(void)snprintf( s_module.config.vendor_string, sizeof( s_module.config.vendor_string ), "%s",
					s_module.borrow.runtimeReceipt.core.caps.vendorName );
	(void)snprintf( s_module.config.version_string, sizeof( s_module.config.version_string ),
					"WebGPU RAL module schema %u", RAL_WEBGPU_RENDERER_MODULE_SCHEMA_VERSION );
	s_module.config.vidWidth		 = (int)s_module.borrow.width;
	s_module.config.vidHeight		 = (int)s_module.borrow.height;
	s_module.config.vidWidthLogical	 = s_module.config.vidWidth;
	s_module.config.vidHeightLogical = s_module.config.vidHeight;
	s_module.config.windowAspect	 = (float)s_module.config.vidWidth / (float)s_module.config.vidHeight;
	s_module.config.maxTextureSize	 = (int)s_module.borrow.runtimeReceipt.core.caps.maxTextureDimension2D;
	s_module.config.numTextureUnits	 = (int)s_module.borrow.runtimeReceipt.core.caps.maxSampledTexturesPerShaderStage;
	s_module.config.colorBits		 = 32;
	s_module.config.depthBits		 = 0;
	s_module.config.stencilBits		 = 0;
	s_module.config.driverType		 = GLDRV_ICD;
	s_module.config.hardwareType	 = GLHW_GENERIC;
	s_module.config.deviceSupportsGamma = qfalse;
	s_module.config.textureCompression	= TC_NONE;
	return qtrue;
}

static qhandle_t RegisterMaterial( renderAssetKind_t kind, const char *name, qboolean clamp )
{
	qhandle_t existing;
	qhandle_t handle;
	renderMaterialScriptEntry_t scripted;
	byte *pixels = NULL;
	uint32_t width = 0u, height = 0u;
	char resolved[MAX_QPATH];
	const char *imageName = name;
	if ( !name || !name[0] )
		return 0;
	existing = RenderSubmission_MaterialHandle( &s_module.frontend, name );
	if ( existing )
		return existing;
	memset( &scripted, 0, sizeof( scripted ) );
	if ( RenderMaterialScript_Lookup( &s_module.materialScripts, name, &scripted ) ) {
		clamp = scripted.clampToEdge;
		if ( scripted.imageName[0] ) imageName = scripted.imageName;
	}
#if defined(__EMSCRIPTEN__)
	if ( RenderImage_DecodeRgba8( imageName, &pixels, &width, &height, resolved ) ) {
		const char *identityName = scripted.name[0] ? name : resolved;
		handle = RenderSubmission_RegisterMaterialImage( &s_module.frontend, kind,
			identityName, clamp, pixels, width, height );
		ri.Free( pixels );
	} else if ( kind == RENDER_ASSET_MSDF ) {
		return 0;
	} else {
		handle = RenderSubmission_RegisterMaterialImage( &s_module.frontend, kind,
			name, clamp, s_white, 1u, 1u );
	}
#else
	(void)imageName; (void)pixels; (void)width; (void)height; (void)resolved;
	handle = RenderSubmission_RegisterMaterialImage( &s_module.frontend, kind,
		name, clamp, s_white, 1u, 1u );
#endif
	if ( handle && scripted.name[0] &&
		 !RenderSubmission_SetMaterialRasterPolicy( &s_module.frontend, handle,
			scripted.alphaMode, scripted.alphaCutoff, scripted.depthWrite ) ) return 0;
	if ( handle && scripted.hasLighting &&
		 !RenderMaterialScript_ApplyLighting( &s_module.materialScripts, name,
			&s_module.frontend, handle ) ) return 0;
	return handle;
}

static qhandle_t RegisterModel( const char *name )
{
	void				 *bytes = NULL;
	renderModelSnapshot_t model;
	qhandle_t			  handle;
	int					  count;
	char				  canonical[MAX_QPATH];
	if ( !name || !name[0] )
		return 0;
	if ( name[0] == '*' && s_module.loadedWorld ) {
		char *end	= NULL;
		long  index = strtol( name + 1, &end, 10 );
		if ( end != name + 1 && *end == '\0' && index > 0 && index < s_module.loadedWorld->numSubModels ) {
			const dmodel_t *source = &s_module.loadedWorld->subModels[index];
			return RenderSubmission_RegisterInlineModel( &s_module.frontend, name, (uint32_t)source->firstSurface,
														 (uint32_t)source->numSurfaces );
		}
	}
	if ( ri.FS_ResolveResource && ri.FS_ResolveResource( name, canonical,
			sizeof( canonical ), NULL, NULL, NULL ) ) name = canonical;
	if ( !ri.FS_ReadFile || !ri.FS_FreeFile )
		return 0;
	count = ri.FS_ReadFile( name, &bytes );
	if ( count <= 0 || !bytes ) {
		if ( bytes )
			ri.FS_FreeFile( bytes );
		return 0;
	}
	handle = RenderSubmission_RegisterModelData( &s_module.frontend, name, bytes, (uint32_t)count );
	ri.FS_FreeFile( bytes );
	if ( !handle || !RenderSubmission_ModelSnapshot( &s_module.frontend, handle, &model ) )
		return 0;
	for ( uint32_t i = 0u; i < model.batchCount; ++i ) {
		qhandle_t material = RegisterMaterial( RENDER_ASSET_MATERIAL, model.batches[i].materialName, qfalse );
		if ( !material || !RenderSubmission_SetModelBatchMaterial( &s_module.frontend, handle, i, material ) )
			return 0;
	}
	return handle;
}

static qhandle_t RegisterSkin( const char *name )
{
	return RenderSubmission_RegisterAsset( &s_module.frontend, RENDER_ASSET_SKIN, name );
}
static qhandle_t RegisterShader( const char *name )
{
	return RegisterMaterial( RENDER_ASSET_MATERIAL, name, qfalse );
}
static qhandle_t RegisterShaderNoMip( const char *name )
{
	return RegisterMaterial( RENDER_ASSET_MATERIAL, name, qtrue );
}
static qhandle_t RegisterShaderLightMap( const char *name, int lightmap )
{
	(void)lightmap;
	return RegisterMaterial( RENDER_ASSET_LIGHTMAP, name, qtrue );
}
static qhandle_t RegisterMsdf( const char *name, float range, int w, int h )
{
	char imageName[MAX_QPATH];
	size_t length;
	(void)range;
	(void)w;
	(void)h;
	if ( name && ( length = strlen( name ) ) >= 6u
			&& !strcmp( name + length - 6u, "_atlas" ) ) {
		byte *pixels = NULL;
		uint32_t width = 0u, height = 0u;
		char resolved[MAX_QPATH];
		qhandle_t handle;
		if ( length - 6u + 4u >= sizeof( imageName ) ) return 0;
		memcpy( imageName, name, length - 6u );
		memcpy( imageName + length - 6u, ".png", 5u );
#if defined(__EMSCRIPTEN__)
		if ( !RenderImage_DecodeRgba8( imageName, &pixels, &width, &height,
				resolved ) ) return 0;
		handle = RenderSubmission_RegisterMaterialImage( &s_module.frontend,
			RENDER_ASSET_MSDF, name, qtrue, pixels, width, height );
		ri.Free( pixels );
		return handle;
#else
		(void)pixels; (void)width; (void)height; (void)resolved; (void)handle;
#endif
	}
	return RegisterMaterial( RENDER_ASSET_MSDF, name, qtrue );
}

static qboolean PrepareWorldMaterials( const mapFile_t *bsp )
{
	byte	*rgba = NULL;
	uint32_t side = 0u;
	uint64_t pixels;
	if ( !bsp || bsp->numShaders < 0 || bsp->numLightmapPages < 0 || bsp->lightmapPageSize < 0 ||
		 ( bsp->numShaders && !bsp->shaders ) )
		return qfalse;
	for ( int i = 0; i < bsp->numShaders; ++i )
		if ( !RegisterShader( bsp->shaders[i].shader ) )
			return qfalse;
	if ( bsp->numLightmapPages == 0 )
		return qtrue;
	if ( !bsp->lightmapData || bsp->lightmapPageSize <= 0 || ( bsp->lightmapPageSize % 3 ) != 0 )
		return qfalse;
	pixels = (uint32_t)bsp->lightmapPageSize / 3u;
	for ( side = 1u; (uint64_t)side * side < pixels; ++side )
		if ( side >= RENDER_SUBMISSION_MAX_IMAGE_DIMENSION )
			return qfalse;
	if ( (uint64_t)side * side != pixels )
		return qfalse;
	rgba = malloc( (size_t)pixels * 4u );
	if ( !rgba )
		return qfalse;
	for ( int page = 0; page < bsp->numLightmapPages; ++page ) {
		char		name[MAX_QPATH];
		const byte *rgb = bsp->lightmapData + (size_t)page * (size_t)bsp->lightmapPageSize;
		for ( uint64_t i = 0u; i < pixels; ++i ) {
			rgba[i * 4u]	  = rgb[i * 3u];
			rgba[i * 4u + 1u] = rgb[i * 3u + 1u];
			rgba[i * 4u + 2u] = rgb[i * 3u + 2u];
			rgba[i * 4u + 3u] = 255u;
		}
		if ( !RenderSubmission_LightmapMaterialName( name, (uint32_t)bsp->checksum, (uint32_t)page ) ||
			 !RenderSubmission_RegisterMaterialImage( &s_module.frontend, RENDER_ASSET_LIGHTMAP, name, qtrue, rgba,
													  side, side ) ) {
			free( rgba );
			return qfalse;
		}
	}
	free( rgba );
	return qtrue;
}

static void LoadWorld( const mapFile_t *bsp, int worldIndex )
{
	if ( !PrepareWorldMaterials( bsp )
			|| !RenderSubmission_LoadWorld( &s_module.frontend, bsp, worldIndex )
			|| !RenderLightingSidecar_LoadDirectional( &s_module.frontend,
				&s_module.imports, bsp->name, &s_module.lightingSidecar )
			|| !RenderLightingSidecar_LoadIrradiance( &s_module.frontend,
				&s_module.imports, bsp->name, &s_module.irradianceSidecar )
			|| !UploadDirectionalLighting() )
		MarkFailed( "frontend-world" );
	else {
		s_module.loadedWorlds[worldIndex] = bsp;
		s_module.activeWorldIndex = worldIndex;
		s_module.loadedWorld = bsp;
	}
}

static qboolean SelectWorld( int worldIndex )
{
	if ( worldIndex < 0 || worldIndex >= MAX_RENDER_WORLDS
			|| !s_module.loadedWorlds[worldIndex]
			|| !RenderSubmission_SelectWorld( &s_module.frontend, worldIndex ) ) return qfalse;
	s_module.activeWorldIndex = worldIndex;
	s_module.loadedWorld = s_module.loadedWorlds[worldIndex];
	return qtrue;
}

static qboolean UnloadWorld( int worldIndex )
{
	if ( worldIndex < 0 || worldIndex >= MAX_RENDER_WORLDS
			|| !RenderSubmission_UnloadWorld( &s_module.frontend, worldIndex ) ) return qfalse;
	s_module.loadedWorlds[worldIndex] = NULL;
	if ( s_module.activeWorldIndex == worldIndex ) {
		s_module.loadedWorld = NULL;
		for ( int i = 0; i < MAX_RENDER_WORLDS; ++i )
			if ( s_module.loadedWorlds[i] ) { (void)SelectWorld( i ); break; }
	}
	return qtrue;
}

static int ResidentWorldCount( void )
{
	return RenderSubmission_ResidentWorldCount( &s_module.frontend );
}

static qboolean CookLightingProject( const char *derivedRoot )
{
#if defined( __EMSCRIPTEN__ ) || defined( WASM_MODULE )
	(void)derivedRoot;
	return qfalse;
#else
	renderLightingProjectCookRequest_t request;
	renderLightingProjectCookReceipt_t receipt;
	char worldStem[RENDER_LIGHTING_PROJECT_WORLD_STEM_CAPACITY];
	return s_module.loadedWorld &&
		RenderLightingProjectCook_DefaultRequest( s_module.loadedWorld,
			derivedRoot, &request, worldStem ) &&
		RenderLightingProjectCook_Execute( &s_module.frontend,
			s_module.loadedWorld, &s_module.imports, &request, &receipt ) &&
		RenderLightingProjectCook_ReceiptValid( &receipt );
#endif
}

static void BeginRegistration( glconfig_t *config )
{
	if ( !config || !s_module.loaded || s_module.frameOpen || s_module.pendingFrameReady ) {
		MarkFailed( "registration-state" );
		return;
	}
	if ( !s_module.product && !InitializeOwners() ) {
		MarkFailed( "browser-runtime-not-ready" );
		return;
	}
	if ( !s_module.brightness ) {
		if ( !ri.Cvar_Get || !ri.Cvar_CheckRange ) {
			memset( &s_module.brightnessFallback, 0,
				sizeof( s_module.brightnessFallback ) );
			s_module.brightnessFallback.value = 1.4f;
			s_module.brightnessFallback.integer = 1;
			s_module.brightness = &s_module.brightnessFallback;
		} else {
			s_module.brightness = ri.Cvar_Get( "r_brightness", "1.4",
				CVAR_ARCHIVE | CVAR_NODEFAULT );
			if ( !s_module.brightness ) {
				MarkFailed( "display-visibility-cvar" ); return;
			}
			ri.Cvar_CheckRange( s_module.brightness, "0", "32", CV_FLOAT );
			if ( ri.Cvar_SetDescription ) ri.Cvar_SetDescription( s_module.brightness,
				"Continuous display visibility scalar; default 1.4, 1.0 is authored identity, fractional values are preserved." );
			if ( ri.Cvar_SetGroup ) ri.Cvar_SetGroup( s_module.brightness, CVG_RENDERER );
		}
	}
	if ( !s_module.frontend.initialized && !RenderSubmission_Init( &s_module.frontend, s_module.moduleGeneration ) ) {
		MarkFailed( "frontend-init" );
		return;
	}
	if ( !s_module.materialScripts.ready &&
		 !RenderMaterialScript_Load( &s_module.materialScripts, &s_module.imports ) ) {
		MarkFailed( "material-script-catalog" );
		return;
	}
	if ( !s_module.defaultMaterial )
		s_module.defaultMaterial = RegisterShaderNoMip( "*white" );
	if ( !s_module.defaultMaterial ) {
		MarkFailed( "default-material" );
		return;
	}
	if ( s_module.imports.CL_SetScaling )
		s_module.imports.CL_SetScaling( 1.0f, s_module.config.vidWidth, s_module.config.vidHeight );
	s_module.registered = qtrue;
	*config				= s_module.config;
}

static void EndRegistration( void )
{
	if ( !s_module.registered || s_module.frameOpen )
		MarkFailed( "registration-end" );
}

static void BeginFrame( stereoFrame_t stereo )
{
	uint64_t generation = NextGeneration();
	(void)stereo;
	if ( !generation || !s_module.registered || s_module.frameOpen || s_module.pendingFrameReady ||
		 !RenderSubmission_BeginFrame( &s_module.frontend, generation ) ) {
		MarkFailed( "frame-begin" );
		return;
	}
	s_module.currentFrameGeneration = generation;
	s_module.frameOpen				= qtrue;
}

static qboolean SubmitPendingProduct( void )
{
	const ralWebGpuWeatherReceipt_t *weatherExecution =
		s_module.pending.weather.zeroWork ? NULL : &s_module.pending.weatherExecution;
	/* A browser canvas texture is valid only for the current presentation turn.
	 * Atmosphere and weather compute can span several asynchronous polls, so
	 * acquire the drawable only after those submissions have completed. */
	if ( !RalWebGpu_PresentationAcquire( s_module.borrow.presentation,
			&s_module.borrow.configuredReceipt, &s_module.pendingFrame ) ) {
		s_module.failureStage = 42;
		return qfalse;
	}
	if ( !RalWebGpu_ProductSetWeather( s_module.product, &s_module.borrow.runtimeReceipt, &s_module.pending.weather,
									   weatherExecution ) ) {
		s_module.failureStage = 48;
		return qfalse;
	}
	if ( !RalWebGpu_ProductRenderPlan( s_module.product, &s_module.borrow.runtimeReceipt, &s_module.frontend,
									   &s_module.pending.frontend, &s_module.pendingProductPlan,
									   s_module.pendingFrame.textureIdentity, s_module.pending.frameGeneration,
									   &s_module.pending.product ) ) {
		s_module.failureStage = 700 + (int)RalWebGpu_ProductLastFailureStage();
		return qfalse;
	}
	s_module.productSubmitted = qtrue;
	return qtrue;
}

static void EndFrame( int *frontEndMsec, int *backEndMsec )
{
	ralWebGpuRendererModuleFrameReceipt_t pending;
	ralWebGpuFrontendPlanReceipt_t		  plan;
	ralDisplayVisibilityPlan_t visibility;
	if ( frontEndMsec )
		*frontEndMsec = 0;
	if ( backEndMsec )
		*backEndMsec = 0;
	if ( !s_module.frameOpen ) {
		MarkFailed( "frame-end" );
		return;
	}
	memset( &pending, 0, sizeof( pending ) );
	pending.schemaVersion	 = RAL_WEBGPU_RENDERER_MODULE_SCHEMA_VERSION;
	pending.backendType		 = RAL_BACKEND_WEBGPU;
	pending.moduleGeneration = s_module.moduleGeneration;
	pending.frameGeneration	 = s_module.currentFrameGeneration;
	if ( !RenderSubmission_EndFrame( &s_module.frontend, pending.frameGeneration, &pending.frontend ) ) {
		s_module.failureStage = 41;
		s_module.frameOpen	  = qfalse;
		MarkFailed( "frontend-frame" );
		return;
	}
	s_module.frameOpen				= qfalse;
	s_module.currentFrameGeneration = 0u;
	/* Loading and registration can seal UI-only frames after a world is loaded,
	 * before the client has submitted its first RenderScene.  The retained
	 * world snapshot is intentionally readable only for a rendered scene, so
	 * defer product lowering until that first scene frame instead of treating
	 * the expected transition frame as a fatal backend error. */
	if ( !pending.frontend.worldLoaded || !pending.frontend.sceneRendered )
		return;
	if ( !PlanAtmosphere( pending.frameGeneration, &pending.atmosphere ) ) {
		s_module.failureStage = 45;
		MarkFailed( "atmosphere-plan" );
		return;
	}
	if ( !s_module.brightness
			|| !Ral_DisplayVisibilityPlanBuild( s_module.brightness->value,
				&visibility ) ) {
		s_module.failureStage = 43;
		MarkFailed( "display-visibility" );
		return;
	}
	pending.displayVisibility = visibility;
	if ( !RalWebGpu_WeatherPlan( &s_module.frontend, &pending.atmosphere, &pending.weather ) ) {
		s_module.failureStage = 47;
		MarkFailed( "weather-plan" );
		return;
	}
	if ( !UpdateAtmosphereUniform( &pending.atmosphere,
			&pending.displayVisibility ) ) {
		s_module.failureStage = 44;
		MarkFailed( "atmosphere-uniform" );
		return;
	}
	if ( !RalWebGpu_ProductPlan( s_module.product, &s_module.borrow.runtimeReceipt, &s_module.frontend,
								 &pending.frontend, &plan ) ) {
		s_module.failureStage = 500 + (int)RalWebGpu_FrontendPlanLastFailureStage();
		MarkFailed( "frontend-plan" );
		return;
	}
	if ( pending.atmosphere.selectedTier == RAL_ATMOSPHERE_TIER_FULL ) {
		if ( !RalWebGpu_AtmosphereBegin( s_module.atmosphere, &s_module.borrow.runtimeReceipt, &s_module.frontend,
										 &pending.atmosphere, &pending.atmosphereExecution ) ) {
			s_module.failureStage = 46;
			MarkFailed( "atmosphere-compute" );
			return;
		}
		s_module.atmosphereComputePending = qtrue;
		s_module.productSubmitted		  = qfalse;
	} else if ( !pending.weather.zeroWork ) {
		if ( !RalWebGpu_WeatherBegin( s_module.weather, &s_module.borrow.runtimeReceipt, &s_module.frontend,
									  &pending.weather, &pending.displayVisibility,
									  pending.frameGeneration, &pending.weatherExecution ) ) {
			s_module.failureStage = 47;
			MarkFailed( "weather-compute" );
			return;
		}
		s_module.weatherComputePending = qtrue;
		s_module.productSubmitted	   = qfalse;
	} else {
		if ( !RalWebGpu_PresentationAcquire( s_module.borrow.presentation,
				&s_module.borrow.configuredReceipt, &s_module.pendingFrame ) ) {
			s_module.failureStage = 42;
			MarkFailed( "presentation-acquire" );
			return;
		}
		if ( !RalWebGpu_ProductSetWeather( s_module.product, &s_module.borrow.runtimeReceipt, &pending.weather,
										   NULL ) ) {
			s_module.failureStage = 48;
			MarkFailed( "weather-product" );
			return;
		}
		if ( !RalWebGpu_ProductRenderPlan( s_module.product, &s_module.borrow.runtimeReceipt, &s_module.frontend,
										   &pending.frontend, &plan, s_module.pendingFrame.textureIdentity,
										   pending.frameGeneration, &pending.product ) ) {
			s_module.failureStage = 700 + (int)RalWebGpu_ProductLastFailureStage();
			MarkFailed( "product-frame" );
			return;
		}
		s_module.productSubmitted = qtrue;
	}
	s_module.pendingProductPlan = plan;
	s_module.pending			= pending;
	s_module.pendingFrameReady	= qtrue;
}

Q_EXPORT ralWebGpuAsyncStatus_t WiredWebGpu_RendererPoll( void )
{
	ralWebGpuAsyncStatus_t status;
	if ( !s_module.pendingFrameReady )
		return RAL_WEBGPU_ASYNC_READY;
	if ( s_module.atmosphereComputePending ) {
		status = RalWebGpu_AtmospherePoll( s_module.atmosphere, &s_module.pending.atmosphereExecution );
		if ( status != RAL_WEBGPU_ASYNC_READY ) {
			if ( status == RAL_WEBGPU_ASYNC_FAILED ) {
				s_module.failureStage = 461;
				MarkFailed( "atmosphere-compute-poll" );
			}
			return status;
		}
		s_module.atmosphereComputePending = qfalse;
		if ( !s_module.pending.weather.zeroWork ) {
			if ( !RalWebGpu_WeatherBegin( s_module.weather, &s_module.borrow.runtimeReceipt, &s_module.frontend,
										  &s_module.pending.weather, &s_module.pending.displayVisibility,
										  s_module.pending.frameGeneration,
										  &s_module.pending.weatherExecution ) ) {
				s_module.failureStage = 47;
				MarkFailed( "weather-compute" );
				return RAL_WEBGPU_ASYNC_FAILED;
			}
			s_module.weatherComputePending = qtrue;
		} else if ( !SubmitPendingProduct() ) {
			MarkFailed( "product-frame" );
			return RAL_WEBGPU_ASYNC_FAILED;
		}
	}
	if ( s_module.weatherComputePending ) {
		status = RalWebGpu_WeatherPoll( s_module.weather, &s_module.pending.weatherExecution );
		if ( status != RAL_WEBGPU_ASYNC_READY ) {
			if ( status == RAL_WEBGPU_ASYNC_FAILED ) {
				s_module.failureStage = 471;
				MarkFailed( "weather-compute-poll" );
			}
			return status;
		}
		s_module.weatherComputePending = qfalse;
		if ( !SubmitPendingProduct() ) {
			MarkFailed( "product-frame" );
			return RAL_WEBGPU_ASYNC_FAILED;
		}
	}
	if ( !s_module.productSubmitted )
		return RAL_WEBGPU_ASYNC_FAILED;
	status = RalWebGpu_ProductPoll( s_module.product, &s_module.pending.product );
	if ( status != RAL_WEBGPU_ASYNC_READY ) {
		if ( status == RAL_WEBGPU_ASYNC_FAILED ) {
			s_module.failureStage = 700 + (int)RalWebGpu_ProductLastFailureStage();
			MarkFailed( "product-poll" );
		}
		return status;
	}
	if ( !RalWebGpu_PresentationPresent( s_module.borrow.presentation, &s_module.pendingFrame,
										 &s_module.pending.product.submission ) ) {
		MarkFailed( "present" );
		return RAL_WEBGPU_ASYNC_FAILED;
	}
	s_module.pending.presented = qtrue;
	s_module.pending.ready	   = qtrue;
	if ( !ReceiptValid( &s_module.pending ) ) {
		s_module.failureStage = 49;
		MarkFailed( "published-receipt" );
		return RAL_WEBGPU_ASYNC_FAILED;
	}
	s_module.published = s_module.pending;
	memset( &s_module.pending, 0, sizeof( s_module.pending ) );
	memset( &s_module.pendingFrame, 0, sizeof( s_module.pendingFrame ) );
	memset( &s_module.pendingProductPlan, 0, sizeof( s_module.pendingProductPlan ) );
	s_module.pendingFrameReady = qfalse;
	s_module.productSubmitted  = qfalse;
	return RAL_WEBGPU_ASYNC_READY;
}

static void Shutdown( refShutdownCode_t code )
{
	if ( !s_module.loaded )
		return;
	if ( s_module.frameOpen )
		RenderSubmission_CancelFrame( &s_module.frontend );
	s_module.frameOpen				= qfalse;
	s_module.currentFrameGeneration = 0u;
	s_module.loadedWorld			= NULL;
	DestroyDirectionalLighting();
	if ( code == REF_LEVEL_ONLY ) {
		if ( !RenderSubmission_ResetEffectRegistries( &s_module.frontend ) )
			MarkFailed( "level-effect-registry-reset" );
		return;
	}
	if ( s_module.pendingFrameReady && WiredWebGpu_RendererPoll() != RAL_WEBGPU_ASYNC_READY ) {
		MarkFailed( "shutdown-in-flight" );
		return;
	}
	DestroyOwners();
	RenderSubmission_Reset( &s_module.frontend );
	memset( &s_module.borrow, 0, sizeof( s_module.borrow ) );
	memset( &s_module.published, 0, sizeof( s_module.published ) );
	s_module.registered = qfalse;
	s_module.loaded		= qfalse;
}

static void ClearScene( void )
{
	if ( s_module.frameOpen && !RenderSubmission_ClearScene( &s_module.frontend ) )
		MarkFailed( "clear" );
}
static void AddEntity( const refEntity_t *e, qboolean t )
{
	const cmSkin_t *skin = NULL;
	(void)t;
	if ( e && e->characterSkin && s_module.imports.GetCharacterSkin )
		skin = s_module.imports.GetCharacterSkin( e->characterSkin );
	if ( s_module.frameOpen && e && !RenderSubmission_AddEntitySkinned(
		&s_module.frontend, e, NULL, skin ) )
		MarkFailed( "entity" );
}
static void AddEntityTemporal( const refEntity_t *e, const refEntityMotion_t *m )
{
	const cmSkin_t *skin = NULL;
	if ( e && e->characterSkin && s_module.imports.GetCharacterSkin )
		skin = s_module.imports.GetCharacterSkin( e->characterSkin );
	if ( s_module.frameOpen && e && RefEntityMotion_IsValid( m ) &&
		 !RenderSubmission_AddEntitySkinned( &s_module.frontend, e, m, skin ) )
		MarkFailed( "temporal" );
}
static void AddPoly( qhandle_t h, int n, const polyVert_t *v, int c )
{
	if ( s_module.frameOpen && !RenderSubmission_AddPoly( &s_module.frontend, h, n, v, c ) )
		MarkFailed( "poly" );
}
static void AddLight( const vec3_t o, float i, float r, float g, float b )
{
	if ( s_module.frameOpen && !RenderSubmission_AddLight( &s_module.frontend, o, NULL, i, r, g, b ) )
		MarkFailed( "light" );
}
static void AddLinearLight( const vec3_t a, const vec3_t b, float i, float r, float g, float bl )
{
	if ( s_module.frameOpen && !RenderSubmission_AddLight( &s_module.frontend, a, b, i, r, g, bl ) )
		MarkFailed( "linear-light" );
}
static void RenderScene( const refdef_t *v, int w )
{
	if ( s_module.frameOpen && !RenderSubmission_RenderScene( &s_module.frontend, v, w ) )
		MarkFailed( "scene" );
}
static void SetColor( const float *c )
{
	if ( !RenderSubmission_SetColor( &s_module.frontend, c ) )
		MarkFailed( "color" );
}
static void SetUiTransform( const refUiTransform_t *transform )
{
	if ( !RenderSubmission_SetUiTransform( &s_module.frontend, transform ) )
		MarkFailed( "ui-transform" );
}
static void DrawPic( float x, float y, float w, float h, float s1, float t1, float s2, float t2, qhandle_t m )
{
	if ( s_module.frameOpen && !RenderSubmission_AddUiQuad( &s_module.frontend, x, y, w, h, s1, t1, s2, t2, 0, m ) )
		MarkFailed( "ui" );
}
static void DrawRotatedPic( float x, float y, float w, float h, float s1, float t1, float s2, float t2, float a,
							qhandle_t m )
{
	if ( s_module.frameOpen && !RenderSubmission_AddUiQuad( &s_module.frontend, x, y, w, h, s1, t1, s2, t2, a, m ) )
		MarkFailed( "ui-rotated" );
}
static void DrawLine( float x1, float y1, float x2, float y2, float w, qhandle_t m )
{
	if ( s_module.frameOpen && !RenderSubmission_AddUiLine( &s_module.frontend, x1, y1, x2, y2, w, m ) )
		MarkFailed( "ui-line" );
}
static void DrawBackdrop( float x, float y, float w, float h, float t, float mx, float my, float q )
{
	DrawPic( x, y, w, h, t, mx, my, q, s_module.defaultMaterial );
}

static void NoVoid( void )
{
}
static void NoBool( qboolean v )
{
	(void)v;
}
static void NoHandle( qhandle_t v )
{
	(void)v;
}
static void NoBytes( const byte *v )
{
	(void)v;
}
static int NoLight( vec3_t p, vec3_t a, vec3_t d, vec3_t n )
{
	(void)p;
	if ( a )
		memset( a, 0, sizeof( vec3_t ) );
	if ( d )
		memset( d, 0, sizeof( vec3_t ) );
	if ( n )
		memset( n, 0, sizeof( vec3_t ) );
	return 0;
}
static void AddEffectRibbon( const ribbonDesc_t *v )
{
	if ( !RenderSubmission_AddEffectRibbon( &s_module.frontend, v ) )
		MarkFailed( "effect-ribbon" );
}
static void NoRail( const railRibbonDesc_t *v )
{
	(void)v;
}
static void NoBeam( const beamDesc_t *v )
{
	(void)v;
}
static void AddEffectSprite( const spriteDesc_t *v )
{
	if ( !RenderSubmission_AddEffectSprite( &s_module.frontend, v ) )
		MarkFailed( "effect-sprite" );
}
static void AddEffectEmitter( const emitterDesc_t *v )
{
	if ( !RenderSubmission_AddEffectEmitter( &s_module.frontend, v ) )
		MarkFailed( "effect-emitter" );
}
static void AddEffectDecal( const decalDesc_t *v )
{
	if ( !RenderSubmission_AddEffectDecal( &s_module.frontend, v ) )
		MarkFailed( "effect-decal" );
}
static void RegisterParticleClass( particleClassHandle_t h, const particleClass_t *v )
{
	if ( !RenderSubmission_RegisterParticleClass( &s_module.frontend, h, v ) )
		MarkFailed( "particle-class" );
}
static void SetAtmosphere( const atmosphericDesc_t *v )
{
	if ( !RenderSubmission_SetAtmosphere( &s_module.frontend, v ) )
		MarkFailed( "atmosphere-state" );
}
static void AddAtmosphereEmitter( const atmosphereEmitter_t *v )
{
	if ( !RenderSubmission_AddAtmosphereEmitter( &s_module.frontend, v ) )
		MarkFailed( "atmosphere-emitter" );
}
static void RegisterAtmosphereEffectProfile( uint32_t h, const atmosphereEffectProfile_t *v )
{
	if ( !RenderSubmission_RegisterAtmosphereEffectProfile( &s_module.frontend, h, v ) )
		MarkFailed( "atmosphere-effect-profile" );
}
static void NoHeightgrid( const float *v, int c )
{
	(void)v;
	(void)c;
}
static void NoLens( const lensSourceDesc_t *v )
{
	(void)v;
}
static qboolean NoLensVisibility( int i, float *v )
{
	(void)i;
	if ( v )
		*v = 0;
	return qfalse;
}
static void AddAtmosphereSurfaceEvent( const atmosphereSurfaceEvent_t *v )
{
	if ( !RenderSubmission_AddAtmosphereSurfaceEvent( &s_module.frontend, v ) )
		MarkFailed( "atmosphere-surface-event" );
}
static void AddAtmosphereMediaVolume( const atmosphereMediaVolume_t *v )
{
	if ( !RenderSubmission_AddAtmosphereMediaVolume( &s_module.frontend, v ) )
		MarkFailed( "atmosphere-media-volume" );
}
static void NoClip( const float *v )
{
	(void)v;
}
static void NoOutline( float w, const float *c, float g, const float *gc )
{
	(void)w;
	(void)c;
	(void)g;
	(void)gc;
}
static void NoShadow( float x, float y, const float *c )
{
	(void)x;
	(void)y;
	(void)c;
}
static void NoRaw( int x, int y, int w, int h, int c, int r, byte *d, int n, qboolean q )
{
	(void)x;
	(void)y;
	(void)w;
	(void)h;
	(void)c;
	(void)r;
	(void)d;
	(void)n;
	(void)q;
}
static void NoUpload( int w, int h, int c, int r, byte *d, int n, qboolean q )
{
	(void)w;
	(void)h;
	(void)c;
	(void)r;
	(void)d;
	(void)n;
	(void)q;
}
static int NoFragments( int n, const vec3_t *p, const vec3_t pr, int mp, vec3_t pb, int mf, markFragment_t *f )
{
	(void)n;
	(void)p;
	(void)pr;
	(void)mp;
	(void)pb;
	(void)mf;
	(void)f;
	return 0;
}
static int SubmitTag( orientation_t *t, qhandle_t m, int s, int e, float f, const char *n )
{
	return RenderSubmission_LerpTag( &s_module.frontend, t, m, s, e, f, n );
}
static void NoBounds( qhandle_t m, vec3_t a, vec3_t b )
{
	(void)m;
	if ( a )
		memset( a, 0, sizeof( vec3_t ) );
	if ( b )
		memset( b, 0, sizeof( vec3_t ) );
}
static void NoFont( const char *n, int s, fontInfo_t *f )
{
	(void)n;
	(void)s;
	if ( f )
		memset( f, 0, sizeof( *f ) );
}
static void NoRemap( const char *a, const char *b, const char *c )
{
	(void)a;
	(void)b;
	(void)c;
}
static qboolean NoToken( char *b, int s )
{
	if ( b && s > 0 )
		b[0] = '\0';
	return qfalse;
}
static qboolean NoPvs( const vec3_t a, const vec3_t b )
{
	(void)a;
	(void)b;
	return qfalse;
}
static void NoVideo( int h, int w, byte *c, byte *e, qboolean m )
{
	(void)h;
	(void)w;
	(void)c;
	(void)e;
	(void)m;
}
static qboolean CanMinimize( void )
{
	return qtrue;
}
static const glconfig_t *GetConfig( void )
{
	return s_module.product ? &s_module.config : NULL;
}
static qboolean GetMemory( uint64_t *a, uint64_t *b, uint64_t *c, uint64_t *d, int *p )
{
	if ( a )
		*a = 0;
	if ( b )
		*b = 0;
	if ( c )
		*c = 0;
	if ( d )
		*d = 0;
	if ( p )
		*p = 0;
	return qfalse;
}
static int NoMdl( qhandle_t m, mdlAnimRange_t *a, int c )
{
	(void)m;
	(void)a;
	(void)c;
	return 0;
}
static void NoLightstyle( int s, const char *p )
{
	(void)s;
	(void)p;
}
static qboolean NoGpu( refGpuProfileSample_t *s )
{
	if ( s )
		memset( s, 0, sizeof( *s ) );
	return qfalse;
}
static void PresentationChanged( const refPresentationChange_t *change )
{
	if ( !change || !s_module.product || s_module.pendingFrameReady || !change->logicalWidth || !change->logicalHeight )
		return;
	if ( !WiredWebGpu_RendererResize( change->logicalWidth, change->logicalHeight ) )
		MarkFailed( "resize" );
}

Q_EXPORT int WiredWebGpu_RendererResize( uint32_t width, uint32_t height )
{
	if ( !width || !height || !s_module.product || s_module.pendingFrameReady ||
		 !RalWebGpu_BrowserModuleResize( width, height ) || !RalWebGpu_BrowserModuleBorrow( &s_module.borrow ) ||
		 !RalWebGpu_ProductSetViewport( s_module.product, &s_module.borrow.runtimeReceipt, width, height ) )
		return 0;
	s_module.config.vidWidth		 = (int)width;
	s_module.config.vidHeight		 = (int)height;
	s_module.config.vidWidthLogical	 = s_module.config.vidWidth;
	s_module.config.vidHeightLogical = s_module.config.vidHeight;
	RalWebGpu_AtmosphereInvalidateHistory( s_module.atmosphere );
	RalWebGpu_WeatherReset( s_module.weather );
	return 1;
}

static void FillExports( refexport_t *e )
{
	memset( e, 0, sizeof( *e ) );
	e->Shutdown						   = Shutdown;
	e->BeginRegistration			   = BeginRegistration;
	e->RegisterModel				   = RegisterModel;
	e->RegisterSkin					   = RegisterSkin;
	e->RegisterShader				   = RegisterShader;
	e->RegisterShaderNoMip			   = RegisterShaderNoMip;
	e->RegisterShaderLightMap		   = RegisterShaderLightMap;
	e->RegisterMSDFShader			   = RegisterMsdf;
	e->RegisterPrimitiveShader		   = RegisterShader;
	e->PinShaderImages				   = NoHandle;
	e->LoadWorld					   = LoadWorld;
	e->SelectWorld                  = SelectWorld;
	e->UnloadWorld                  = UnloadWorld;
	e->ResidentWorldCount           = ResidentWorldCount;
	e->SetWorldVisData				   = NoBytes;
	e->EndRegistration				   = EndRegistration;
	e->ClearScene					   = ClearScene;
	e->AddRefEntityToScene			   = AddEntity;
	e->AddPolyToScene				   = AddPoly;
	e->LightForPoint				   = NoLight;
	e->AddLightToScene				   = AddLight;
	e->AddAdditiveLightToScene		   = AddLight;
	e->AddLinearLightToScene		   = AddLinearLight;
	e->AddRibbonToScene				   = AddEffectRibbon;
	e->AddRailRibbonToScene			   = NoRail;
	e->AddBeamToScene				   = NoBeam;
	e->AddSpriteToScene				   = AddEffectSprite;
	e->EmitParticles				   = AddEffectEmitter;
	e->AddDecalToScene				   = AddEffectDecal;
	e->RegisterParticleClass		   = RegisterParticleClass;
	e->SetAtmosphere				   = SetAtmosphere;
	e->SetAtmosphereHeightgrid		   = NoHeightgrid;
	e->AddLensSourceToScene			   = NoLens;
	e->GetLensVisibility			   = NoLensVisibility;
	e->RenderScene					   = RenderScene;
	e->SetColor						   = SetColor;
	e->SetMSDFOutline				   = NoOutline;
	e->SetMSDFShadow				   = NoShadow;
	e->SetClipRegion				   = NoClip;
	e->SetUiTransform				   = SetUiTransform;
	e->DrawStretchPic				   = DrawPic;
	e->DrawMenuBackdrop				   = DrawBackdrop;
	e->DrawStretchPicOverlay		   = DrawPic;
	e->DrawRotatedPic				   = DrawRotatedPic;
	e->DrawLine						   = DrawLine;
	e->DrawStretchRaw				   = NoRaw;
	e->UploadCinematic				   = NoUpload;
	e->BeginFrame					   = BeginFrame;
	e->EndFrame						   = EndFrame;
	e->MarkFragments				   = NoFragments;
	e->LerpTag						   = SubmitTag;
	e->ModelBounds					   = NoBounds;
	e->RegisterFont					   = NoFont;
	e->RemapShader					   = NoRemap;
	e->GetEntityToken				   = NoToken;
	e->inPVS						   = NoPvs;
	e->TakeVideoFrame				   = NoVideo;
	e->ThrottleBackend				   = NoVoid;
	e->FinishBloom					   = NoVoid;
	e->SetColorMappings				   = NoVoid;
	e->CanMinimize					   = CanMinimize;
	e->GetConfig					   = GetConfig;
	e->GetMemoryBudget				   = GetMemory;
	e->VertexLighting				   = NoBool;
	e->SyncRender					   = NoVoid;
	e->GetMDLAnimations				   = NoMdl;
	e->SetLightstylePattern			   = NoLightstyle;
	e->GetGpuProfileSample			   = NoGpu;
	e->AddRefEntityToSceneTemporal	   = AddEntityTemporal;
	e->PresentationChanged			   = PresentationChanged;
	e->AddAtmosphereEmitter			   = AddAtmosphereEmitter;
	e->RegisterAtmosphereEffectProfile = RegisterAtmosphereEffectProfile;
	e->AddAtmosphereSurfaceEvent	   = AddAtmosphereSurfaceEvent;
	e->AddAtmosphereMediaVolume		   = AddAtmosphereMediaVolume;
	e->CookLightingProject			   = CookLightingProject;
}

Q_EXPORT refexport_t *QDECL GetRefAPI( int apiVersion, refimport_t *imports )
{
	if ( !imports || apiVersion != REF_API_VERSION || s_module.loaded || s_moduleCounter >= UINT64_MAX - 1u )
		return NULL;
	memset( &s_module, 0, sizeof( s_module ) );
	s_module.imports		  = *imports;
	s_module.moduleGeneration = ++s_moduleCounter;
	s_module.nextGeneration	  = s_module.moduleGeneration;
	s_module.logChannel		  = -1;
	ri						  = *imports;
	if ( imports->GetLogChannel && imports->LogCh )
		s_module.logChannel = imports->GetLogChannel( "renderer.init" );
	FillExports( &s_module.exports );
	s_module.loaded = qtrue;
	return &s_module.exports;
}

#ifdef RAL_WEBGPU_BROWSER_SMOKE
static qboolean s_rendererSmokeActive;
static refexport_t *s_rendererSmokeExports;
static refdef_t s_rendererSmokeView;
static qhandle_t s_rendererSmokeMaterial;
static uint32_t s_rendererSmokeFixture;

static qboolean RendererSmokeSubmitFixture( uint32_t fixtureIndex )
{
	ralAtmosphereFixtureState_t authored;
	atmosphereEmitter_t emitter;
	atmosphereMediaVolume_t media;
	if ( !s_rendererSmokeExports ||
			!Ral_AtmosphereConformanceFixture(
				(ralAtmosphereFixture_t)fixtureIndex, &authored ) )
		return qfalse;
	authored.state.timelineSeconds = (float)fixtureIndex;
	s_rendererSmokeExports->SetAtmosphere( &authored.state );
	s_rendererSmokeExports->BeginFrame( STEREO_CENTER );
	s_rendererSmokeExports->ClearScene();
	if ( authored.breathEmitter ) {
		Ral_AtmosphereConformanceBreathEmitter( 1u, &emitter );
		s_rendererSmokeExports->AddAtmosphereEmitter( &emitter );
	}
	if ( authored.localMedia ) {
		Ral_AtmosphereConformanceLocalMedia( &media );
		s_rendererSmokeExports->AddAtmosphereMediaVolume( &media );
	}
	s_rendererSmokeExports->RenderScene( &s_rendererSmokeView, 0 );
	s_rendererSmokeExports->DrawStretchPic( 32.0f, 32.0f, 192.0f, 96.0f,
		0.0f, 0.0f, 1.0f, 1.0f, s_rendererSmokeMaterial );
	s_rendererSmokeExports->EndFrame( NULL, NULL );
	return !s_rendererSmokeExports->initFailed && s_module.pendingFrameReady;
}

Q_EXPORT int WiredWebGpu_RendererSmokeBegin( void )
{
	static mapFile_t		map;
	static dsurface_t		surface;
	static drawVert_t		vertices[3];
	static int				indices[3] = { 0, 1, 2 };
	static dshader_t		shader;
	refimport_t				imports;
	refexport_t			   *renderer;
	particleClass_t			breathClass;
	atmosphereEffectProfile_t breathProfile;
	glconfig_t				config;
	qhandle_t				material;
	if ( s_rendererSmokeActive || s_module.loaded )
		return 0;
	memset( &imports, 0, sizeof( imports ) );
	renderer = GetRefAPI( REF_API_VERSION, &imports );
	if ( !renderer )
		return -1;
	memset( &config, 0, sizeof( config ) );
	renderer->BeginRegistration( &config );
	if ( renderer->initFailed || config.vidWidth != 1280 || config.vidHeight != 720 )
		return s_module.failureStage ? -s_module.failureStage : -2;
	memset( &map, 0, sizeof( map ) );
	memset( &surface, 0, sizeof( surface ) );
	memset( vertices, 0, sizeof( vertices ) );
	memset( &shader, 0, sizeof( shader ) );
	(void)snprintf( map.name, sizeof( map.name ), "maps/webgpu-renderer-smoke.bsp" );
	map.checksum	   = 0x11821;
	map.numSurfaces	   = 1;
	map.surfaces	   = &surface;
	map.numDrawVerts   = 3;
	map.drawVerts	   = vertices;
	map.numDrawIndexes = 3;
	map.drawIndexes	   = indices;
	map.numShaders	   = 1;
	map.shaders		   = &shader;
	(void)snprintf( shader.shader, sizeof( shader.shader ), "textures/webgpu-smoke" );
	surface.surfaceType = MST_PLANAR;
	surface.shaderNum	= 0;
	surface.firstVert	= 0;
	surface.numVerts	= 3;
	surface.firstIndex	= 0;
	surface.numIndexes	= 3;
	surface.lightmapNum = -1;
	vertices[0].xyz[0]	= -600.0f;
	vertices[0].xyz[1]	= -300.0f;
	vertices[1].xyz[0]	= 600.0f;
	vertices[1].xyz[1]	= -300.0f;
	vertices[2].xyz[1]	= 500.0f;
	for ( uint32_t i = 0u; i < 3u; ++i )
		memset( vertices[i].color.rgba, 255, 4u );
	renderer->LoadWorld( &map, 0 );
	material = renderer->RegisterShader( shader.shader );
	if ( !material || renderer->initFailed )
		return -3;
	memset( &breathClass, 0, sizeof( breathClass ) );
	breathClass.shader = material;
	breathClass.emitMode = EMIT_POINT;
	breathClass.scatterShape = SCATTER_SPHERE;
	breathClass.velocityShape = VEL_AXIAL_PLUS_CUBE;
	breathClass.scatterMagnitude = 1.5f;
	breathClass.axialSpeed = 8.0f;
	breathClass.cubeJitter = 1.0f;
	breathClass.lifetimeMean = 1.0f;
	breathClass.paletteCount = 1;
	breathClass.colorPalette[0][0] = 0.82f;
	breathClass.colorPalette[0][1] = 0.90f;
	breathClass.colorPalette[0][2] = 1.0f;
	breathClass.colorPalette[0][3] = 0.38f;
	breathClass.sizeStart = 1.0f;
	breathClass.sizeEnd = 8.0f;
	breathClass.drag = 0.8f;
	renderer->RegisterParticleClass( 1, &breathClass );
	memset( &breathProfile, 0, sizeof( breathProfile ) );
	breathProfile.schemaVersion = WIRED_ATMOSPHERE_EFFECT_PROFILE_SCHEMA_VERSION;
	breathProfile.stageCount = 1u;
	breathProfile.maxParticles = 24u;
	breathProfile.seed = 216u;
	breathProfile.duration = 1.0f;
	breathProfile.lodFar = 512.0f;
	breathProfile.boundsRadius = 32.0f;
	breathProfile.stages[0].trigger = ATMOSPHERE_STAGE_CONTINUOUS;
	breathProfile.stages[0].particleClass = 1u;
	breathProfile.stages[0].parentStage = UINT32_MAX;
	breathProfile.stages[0].maxParticles = 24u;
	breathProfile.stages[0].spawnRate = 24.0f;
	breathProfile.stages[0].duration = 1.0f;
	breathProfile.stages[0].lodFar = 512.0f;
	breathProfile.stages[0].boundsRadius = 32.0f;
	breathProfile.stages[0].intensityScale = 1.0f;
	renderer->RegisterAtmosphereEffectProfile( 1u, &breathProfile );
	if ( renderer->initFailed )
		return -31;
	renderer->EndRegistration();
	s_rendererSmokeExports = renderer;
	s_rendererSmokeMaterial = material;
	s_rendererSmokeFixture = RAL_ATMOSPHERE_FIXTURE_CLEAR;
	memset( &s_rendererSmokeView, 0, sizeof( s_rendererSmokeView ) );
	s_rendererSmokeView.width = 1280;
	s_rendererSmokeView.height = 720;
	s_rendererSmokeView.fov_x = 90.0f;
	s_rendererSmokeView.fov_y = 60.0f;
	s_rendererSmokeView.viewaxis[0][0] = s_rendererSmokeView.viewaxis[1][1] =
		s_rendererSmokeView.viewaxis[2][2] = 1.0f;
	if ( !RendererSmokeSubmitFixture( s_rendererSmokeFixture ) )
		return s_module.failureStage ? -s_module.failureStage : -4;
	s_rendererSmokeActive = qtrue;
	return 1;
}

Q_EXPORT int WiredWebGpu_RendererSmokePoll( void )
{
	ralWebGpuRendererModuleFrameReceipt_t receipt;
	ralWebGpuAsyncStatus_t				  status;
	ralAtmosphereFixtureState_t		  authored;
	if ( !s_rendererSmokeActive )
		return 0;
	status = WiredWebGpu_RendererPoll();
	if ( status == RAL_WEBGPU_ASYNC_PENDING )
		return 1;
	if ( status != RAL_WEBGPU_ASYNC_READY )
		return s_module.failureStage ? -1000 - s_module.failureStage : -1;
	if ( !WiredWebGpu_GetFrameReceipt( &receipt ) ) return -2;
	if ( !Ral_AtmosphereConformanceFixture(
			(ralAtmosphereFixture_t)s_rendererSmokeFixture, &authored ) ) return -21;
	if ( receipt.product.worldDrawCount != 1u || receipt.product.uiDrawCount != 1u ||
		 receipt.product.atmosphereBoundDrawCount != 1u ) return -3;
	if ( receipt.atmosphere.requestedTier !=
			(ralAtmosphereTier_t)authored.state.qualityTier ||
		 receipt.atmosphere.selectedTier !=
			(ralAtmosphereTier_t)authored.state.qualityTier ||
		 receipt.atmosphere.fallbackReason != RAL_ATMOSPHERE_FALLBACK_NONE ||
		 receipt.weather.familyMask != authored.familyMask ) return -4;
	if ( receipt.atmosphere.computeDispatchCount > 0u ) {
		if ( !RalWebGpu_AtmosphereReceiptExact( &receipt.atmosphereExecution,
				&receipt.atmosphereExecution ) ||
			 receipt.atmosphereExecution.dispatchCount !=
				receipt.atmosphere.computeDispatchCount ) return -5;
	} else if ( receipt.atmosphereExecution.schemaVersion != 0u ) return -51;
	if ( authored.breathEmitter &&
			( receipt.weather.semanticEmitterCount != 1u ||
			  receipt.weather.semanticParticleCount != 24u ) ) return -6;
	if ( receipt.weather.zeroWork ) {
		if ( receipt.weatherExecution.schemaVersion != 0u ||
				receipt.product.weatherDrawCount != 0u ) return -7;
	} else if ( !RalWebGpu_WeatherReceiptExact( &receipt.weatherExecution,
				&receipt.weatherExecution ) ||
		 receipt.weatherExecution.dispatchCount != 1u ||
		 receipt.product.weatherDrawCount != 1u ) return -8;
	if ( s_rendererSmokeFixture == RAL_ATMOSPHERE_FIXTURE_FOG &&
			( receipt.atmosphere.computeDispatchCount != 3u ||
			  receipt.atmosphere.froxelCount != 230400u ) ) return -81;
	if ( s_rendererSmokeFixture == RAL_ATMOSPHERE_FIXTURE_STORM &&
			( receipt.atmosphere.computeDispatchCount != 4u ||
			  !receipt.atmosphere.cloudsActive ||
			  receipt.weather.activeParticleCount != 8192u ) ) return -82;
	if ( receipt.product.unresolvedCount || receipt.product.fallbackCount || receipt.product.fatalCount ) return -9;
	if ( ++s_rendererSmokeFixture < RAL_ATMOSPHERE_FIXTURE_COUNT ) {
		if ( !RendererSmokeSubmitFixture( s_rendererSmokeFixture ) ) return -10;
		return 1;
	}
	s_module.exports.Shutdown( REF_UNLOAD_DLL );
	s_rendererSmokeExports = NULL;
	s_rendererSmokeActive = qfalse;
	return 2;
}
#else
Q_EXPORT int WiredWebGpu_RendererSmokeBegin( void )
{
	return 0;
}
Q_EXPORT int WiredWebGpu_RendererSmokePoll( void )
{
	return 0;
}
#endif
