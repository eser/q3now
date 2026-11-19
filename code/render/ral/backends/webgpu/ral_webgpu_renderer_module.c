// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "ral_webgpu_renderer_module.h"
#include "render_pbr_control.h"
#include "ral_webgpu_browser_emscripten.h"
#include "ral_webgpu_lighting.h"
#include "ral_atmosphere_conformance.h"
#include "maps/map_format_registry.h"
#include "render_lighting_project_cook.h"
#include "render_lighting_sidecar.h"
#include "render_image_decode.h"
#include "render_sky_atlas.h"
#include "render_material_script.h"
#include "render_world_material_usage.h"
#include "render_material_runtime_owner.h"

#include <limits.h>
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
	WEBGPU_PIPELINE_WORLD,
	WEBGPU_PIPELINE_ENTITY,
	WEBGPU_PIPELINE_EFFECT_LEGACY,
	WEBGPU_PIPELINE_EFFECT_ALPHA_DEPTH,
	WEBGPU_PIPELINE_EFFECT_ADDITIVE_DEPTH,
	WEBGPU_PIPELINE_EFFECT_ALPHA_NO_DEPTH,
	WEBGPU_PIPELINE_EFFECT_ADDITIVE_NO_DEPTH,
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
	float viewForward[4];
	float viewLeft[4];
	float viewUp[4];
	float skyControl[4];
	float radianceControl[4]; /* IBL ready, blend, previous valid, visible physical */
	float aerialHaze[4]; /* sun direction xyz, aerial-perspective haze weight */
} wiredWebGpuAtmosphereUniform_t;

_Static_assert( sizeof( wiredWebGpuAtmosphereUniform_t ) == 176u,
	"WebGPU atmosphere/visible-sky uniform ABI drift" );

typedef struct {
	ralWebGpuLighting_t *resource;
	ralWebGpuLightingReceipt_t receipt;
} wiredWebGpuWorldLighting_t;

typedef struct {
	refimport_t							  imports;
	refexport_t							  exports;
	ralWebGpuBrowserModuleBorrow_t		  borrow;
	ralWebGpuPipeline_t					 *pipelines[WEBGPU_PIPELINE_COUNT];
	ralWebGpuPipelineReceipt_t			  pipelineReceipts[WEBGPU_PIPELINE_COUNT];
	ralWebGpuResource_t					 *atmosphereUniform;
	ralWebGpuResourceReceipt_t			  atmosphereUniformReceipt;
	ralWebGpuWriteReceipt_t				  atmosphereWriteReceipt;
	uintptr_t atmosphereBindGroups[2u + RAL_WEBGPU_EFFECT_PIPELINE_COUNT];
	uint64_t                              atmosphereSkyBindingGeneration;
	ralSkyRadianceConsumerReceipt_t       skyRadianceConsumer;
	uintptr_t							  worldMaterialBindGroup;
	uintptr_t							  entityLightingBindGroup;
	ralWebGpuAtmosphere_t				 *atmosphere;
	ralWebGpuSky_t                         *sky;
	ralWebGpuWeather_t					 *weather;
	ralWebGpuProduct_t					 *product;
    cvar_t *exposureControls[10];
    cvar_t exposureDefaults[10];
	ralSkyEnvironmentRuntimeState_t skyRuntime;
	uint64_t skyWorldLifecycleGeneration;
    uint64_t lensScope, lensPublishedFrame;
    struct { uintptr_t query; uint64_t frame; } lensQueries[3];
    float lensVisibility[LENS_MAX_CGSOURCES];
    qboolean lensVisibleActive[LENS_MAX_CGSOURCES];
	renderSubmissionState_t				  frontend;
	renderMaterialScriptCatalog_t		  materialScripts;
	renderMaterialRuntimeOwner_t          materialRuntimeOwner;
	renderLightingSidecarReceipt_t		  lightingSidecar;
	renderIrradianceSidecarReceipt_t	  irradianceSidecar;
	wiredWebGpuWorldLighting_t              worldLighting[MAX_RENDER_WORLDS];
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
	qboolean                               resetRegistrationAssets;
	qboolean							  frameOpen;
	qboolean							  pendingFrameReady;
	// A frame requested while the previous submission is still unconsumed by
	// the host tick (loading-screen updates inside one host frame) is dropped
	// as a unit: BeginFrame records the skip, EndFrame consumes it, and every
	// scene entry point already no-ops while no frame is open.
	qboolean							  frameSkipped;
	qboolean							  atmosphereComputePending;
	qboolean                              skyComputePending;
	qboolean                              skyRadianceComputePending;
	uint32_t                              pendingSkyEventMask;
	uint32_t                              completedSkyWorkMask;
	qboolean							  weatherComputePending;
	qboolean							  productSubmitted;
	int									  failureStage;
	int									  logChannel;
	cvar_t								 *brightness;
	cvar_t *pbr;
	cvar_t pbrDefault;
	cvar_t								  brightnessFallback;
	cvar_t *aerialHaze;
	cvar_t aerialHazeFallback;
	cvar_t *lightmapBoost;
	cvar_t lightmapBoostFallback;
	cvar_t *skyPhysicalBlend;
	cvar_t skyPhysicalBlendFallback;
	cvar_t *lens;
	cvar_t lensFallback;
} wiredWebGpuModuleState_t;

static wiredWebGpuModuleState_t s_module;
static uint64_t					s_moduleCounter;
refimport_t						ri;

static qboolean RefreshWorldAtmosphereOwner( qboolean forceFallback );

static const byte s_white[4]	 = { 255u, 255u, 255u, 255u };

int WiredWebGpu_RendererFailureStage( void ) { return s_module.failureStage; }

static ralSkyGpuTimingState_t SkyTimingState(
		ralWebGpuGpuTimingState_t state ) {
	switch ( state ) {
	case RAL_WEBGPU_GPU_TIMING_UNAVAILABLE:
		return RAL_SKY_GPU_TIMING_UNAVAILABLE;
	case RAL_WEBGPU_GPU_TIMING_PENDING:
		return RAL_SKY_GPU_TIMING_PENDING;
	case RAL_WEBGPU_GPU_TIMING_VALID:
		return RAL_SKY_GPU_TIMING_VALID;
	default:
		return RAL_SKY_GPU_TIMING_FAILED;
	}
}
static const char s_worldVertexWgsl[] =
	"struct VertexOut { @builtin(position) position: vec4<f32>,"
	" @location(0) color: vec4<f32>, @location(1) fogCoord: vec2<f32>,"
	" @location(2) emission: vec3<f32>, @location(3) skySurface: f32,"
	" @location(4) normal: vec3<f32>,"
	" @location(5) @interpolate(flat) baseColor: vec4<f32>,"
	" @location(6) @interpolate(flat) materialFactors: vec4<f32>,"
	" @location(7) uv: vec2<f32>, @location(8) lightmapUv: vec2<f32>,"
	" @location(9) @interpolate(flat) materialExtra: vec4<f32> };"
	"struct WorldMaterials { values: array<vec4<f32>> };"
	"@group(1) @binding(0) var<storage, read> worldMaterials: WorldMaterials;"
	"@vertex fn main(@location(0) p: vec3<f32>, @location(1) clipW: f32,"
	" @location(2) color: vec4<f32>, @location(3) fogCoord: vec2<f32>,"
	" @location(4) normal: vec3<f32>, @location(5) uv: vec2<f32>,"
	" @location(6) lightmapUv: vec2<f32>,"
	" @builtin(instance_index) materialIndex: u32) -> VertexOut {"
	" var out: VertexOut; out.position = vec4<f32>(p, clipW);"
	" out.color = color; out.fogCoord = fogCoord; out.uv = uv; out.lightmapUv = lightmapUv;"
	" let base=materialIndex*4u; let material=worldMaterials.values[base];"
	" out.emission=material.rgb; out.skySurface=material.w; out.normal=normal;"
	" out.baseColor=worldMaterials.values[base+1u];"
	" out.materialFactors=worldMaterials.values[base+2u];"
	" out.materialExtra=worldMaterials.values[base+3u]; return out; }\n";
static const char s_vertexWgsl[] = "struct VertexOut { @builtin(position) position: vec4<f32>,"
								   " @location(0) color: vec4<f32>, @location(1) fogCoord: vec2<f32> };"
								   "@vertex fn main(@location(0) p: vec3<f32>, @location(1) clipW: f32,"
								   " @location(2) color: vec4<f32>, @location(3) fogCoord: vec2<f32>) -> VertexOut {"
								   " var out: VertexOut; out.position = vec4<f32>(p, clipW);"
								   " out.color = color; out.fogCoord = fogCoord; return out; }\n";
static const char s_effectVertexWgsl[] =
	"struct EffectDraw { startColor:vec4<f32>, endColor:vec4<f32>, scaleOffset:vec4<f32>, flags:vec4<u32> };"
	"struct EffectDraws { values:array<EffectDraw> };"
	"@group(0) @binding(2) var<storage,read> effectDraws:EffectDraws;"
	"struct VertexOut { @builtin(position) position: vec4<f32>,"
	" @location(0) color: vec4<f32>, @location(1) fogCoord: vec2<f32>,"
	" @location(2) uv: vec2<f32>, @location(3) @interpolate(flat) beam:u32 };"
	"@vertex fn main(@location(0) p: vec3<f32>, @location(1) clipW: f32,"
	" @location(2) color: vec4<f32>, @location(3) fogCoord: vec2<f32>,"
	" @location(4) packedUv: vec3<f32>, @builtin(instance_index) drawIndex:u32) -> VertexOut {"
	" var out: VertexOut; out.position=vec4<f32>(p,clipW); out.color=color;"
	" out.fogCoord=fogCoord; out.uv=packedUv.xy; let draw=effectDraws.values[drawIndex];"
	" out.beam=draw.flags.x; if(out.beam!=0u){"
	" out.color=mix(draw.startColor,draw.endColor,packedUv.x);"
	" out.uv=packedUv.xy*draw.scaleOffset.xy+draw.scaleOffset.zw;} return out; }\n";
static const char s_effectFragmentWgsl[] =
	"@id(0) override wired_additive:u32=0u;"
	"fn wired_srgb_to_linear(c:vec3<f32>)->vec3<f32>{"
	" return select(pow((c+vec3<f32>(0.055))/1.055,vec3<f32>(2.4)),c/12.92,c<=vec3<f32>(0.04045));}"
	"struct Atmosphere { eyeDensity: vec4<f32>, colorVisibility: vec4<f32>,"
	" heightCloud: vec4<f32>, froxelGrid: vec4<u32>, displayVisibility: vec4<f32> };"
	"struct Froxels { values: array<vec4<f32>> };"
	"@group(0) @binding(0) var<uniform> atmosphere: Atmosphere;"
	"@group(0) @binding(1) var<storage, read> froxels: Froxels;"
	"@group(1) @binding(0) var effectTexture: texture_2d<f32>;"
	"@group(1) @binding(1) var effectSampler: sampler;"
	"fn wired_display_visibility(c: vec3<f32>) -> vec3<f32> {"
	" var exposed=max(c,vec3<f32>(0.0))*atmosphere.displayVisibility.x;"
	" let l=dot(exposed,vec3<f32>(0.2126,0.7152,0.0722));"
	" if(l>0.0&&l<atmosphere.displayVisibility.z&&atmosphere.displayVisibility.y!=1.0){"
	" let curved=pow(l/atmosphere.displayVisibility.z,atmosphere.displayVisibility.y)"
	" *atmosphere.displayVisibility.z; exposed*=curved/l;} return exposed;}"
	"@fragment fn main(@location(0) color: vec4<f32>,"
	" @location(1) fogCoord: vec2<f32>, @location(2) uv: vec2<f32>,"
	" @location(3) @interpolate(flat) beam:u32,"
	" @builtin(position) pixel: vec4<f32>) -> @location(0) vec4<f32> {"
	" let texel=textureSample(effectTexture,effectSampler,uv);"
	" var sampled=texel*color;"
	" if(beam!=0u){sampled=vec4<f32>(texel.rgb*wired_srgb_to_linear(color.rgb),texel.a*color.a);}"
	" if(atmosphere.froxelGrid.z>0u){"
	" let x=min(u32(pixel.x)/16u,atmosphere.froxelGrid.x-1u);"
	" let y=min(u32(pixel.y)/16u,atmosphere.froxelGrid.y-1u);"
	" let farZ=max(atmosphere.colorVisibility.w,4.01);"
	" let slice=clamp(log(max(1.0/pixel.w,4.0)/4.0)/log(farZ/4.0),0.0,0.9999);"
	" let z=min(u32(slice*f32(atmosphere.froxelGrid.z)),atmosphere.froxelGrid.z-1u);"
	" let f=froxels.values[atmosphere.froxelGrid.w+(z*atmosphere.froxelGrid.y+y)"
	" *atmosphere.froxelGrid.x+x];"
	" let lit=sampled.rgb*f.w+select(f.rgb,vec3<f32>(0.0),wired_additive!=0u);"
	" if(beam!=0u){return vec4<f32>(lit,sampled.a);}"
	" return vec4<f32>(wired_display_visibility(lit),sampled.a);}"
	" var density=atmosphere.eyeDensity.w;"
	" if(atmosphere.colorVisibility.w>0.0){density=max(density,3.912023/"
	" atmosphere.colorVisibility.w);} var lit=sampled.rgb; if(density>0.0){"
	" let heightWeight=exp(-atmosphere.heightCloud.y*max(fogCoord.y-"
	" atmosphere.heightCloud.x,0.0)); let transmittance=exp(-density*"
	" heightWeight*fogCoord.x); lit=mix(select(atmosphere.colorVisibility.rgb,"
	" vec3<f32>(0.0),wired_additive!=0u),lit,clamp(transmittance,0.0,1.0));}"
	" if(beam!=0u){return vec4<f32>(lit,sampled.a);} return vec4<f32>("
	" wired_display_visibility(lit),sampled.a);}\n";
static const char s_entityVertexWgsl[] =
	"fn wired_srgb_to_linear(c: vec3<f32>) -> vec3<f32> {"
	" return select(pow((c+vec3<f32>(0.055))/1.055,vec3<f32>(2.4)),"
	" c/12.92,c<=vec3<f32>(0.04045)); }"
	"struct VertexOut { @builtin(position) position: vec4<f32>,"
	" @location(0) color: vec4<f32>, @location(1) fogCoord: vec2<f32>,"
	" @location(2) uv: vec2<f32> };"
	"struct EntityLighting { values: array<vec4<f32>> };"
	"@group(1) @binding(0) var<storage, read> entityLighting: EntityLighting;"
	"@vertex fn main(@location(0) p: vec3<f32>, @location(1) clipW: f32,"
	" @location(2) color: vec4<f32>, @location(3) fogCoord: vec2<f32>,"
	" @location(4) normal: vec3<f32>, @location(5) uv: vec2<f32>,"
	" @builtin(instance_index) lightingIndex: u32) -> VertexOut {"
	" let base = lightingIndex * 4u;"
	" let c0 = entityLighting.values[base];"
	" let irradiance = select(vec3<f32>(1.0), max(c0.rgb"
	" + entityLighting.values[base+1u].rgb*normal.x"
	" + entityLighting.values[base+2u].rgb*normal.y"
	" + entityLighting.values[base+3u].rgb*normal.z, vec3<f32>(0.0)),"
	" c0.w > 0.5);"
	" var out: VertexOut; out.position = vec4<f32>(p, clipW);"
	" out.color = vec4<f32>(wired_srgb_to_linear(color.rgb) * irradiance, color.a);"
	" out.fogCoord = fogCoord; out.uv = uv; return out; }\n";
/* Entity fragment: the misc fragment plus the material base texture (group 2). */
static const char s_entityFragmentWgsl[] =
	"struct Atmosphere { eyeDensity: vec4<f32>, colorVisibility: vec4<f32>,"
	" heightCloud: vec4<f32>, froxelGrid: vec4<u32>, displayVisibility: vec4<f32> };"
	"struct Froxels { values: array<vec4<f32>> };"
	"@group(0) @binding(0) var<uniform> atmosphere: Atmosphere;"
	"@group(0) @binding(1) var<storage, read> froxels: Froxels;"
	"@group(2) @binding(0) var baseTexture: texture_2d<f32>;"
	"@group(2) @binding(1) var baseSampler: sampler;"
	"fn wired_display_visibility(c: vec3<f32>) -> vec3<f32> {"
	" var exposed=max(c,vec3<f32>(0.0))*atmosphere.displayVisibility.x;"
	" let l=dot(exposed,vec3<f32>(0.2126,0.7152,0.0722));"
	" if (l>0.0 && l<atmosphere.displayVisibility.z && atmosphere.displayVisibility.y!=1.0) {"
	" let curved=pow(l/atmosphere.displayVisibility.z,atmosphere.displayVisibility.y)"
	" *atmosphere.displayVisibility.z; exposed*=curved/l; } return exposed; }"
	"@fragment fn main(@location(0) vertexColor: vec4<f32>,"
	" @location(1) fogCoord: vec2<f32>, @location(2) uv: vec2<f32>,"
	" @builtin(position) pixel: vec4<f32>)"
	" -> @location(0) vec4<f32> {"
	" let tex=textureSample(baseTexture,baseSampler,uv);"
	" let color=vec4<f32>(vertexColor.rgb*tex.rgb,vertexColor.a*tex.a);"
	" if (atmosphere.froxelGrid.z > 0u) {"
	" let x=min(u32(pixel.x)/16u,atmosphere.froxelGrid.x-1u);"
	" let y=min(u32(pixel.y)/16u,atmosphere.froxelGrid.y-1u);"
	" let farZ=max(atmosphere.colorVisibility.w,4.01);"
	" let slice=clamp(log(max(1.0/pixel.w,4.0)/4.0)/log(farZ/4.0),0.0,0.9999);"
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
	" let slice=clamp(log(max(1.0/pixel.w,4.0)/4.0)/log(farZ/4.0),0.0,0.9999);"
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
	" heightCloud: vec4<f32>, froxelGrid: vec4<u32>, displayVisibility: vec4<f32>,"
	" viewForward: vec4<f32>, viewLeft: vec4<f32>, viewUp: vec4<f32>, skyControl: vec4<f32>,"
	" radianceControl: vec4<f32>, aerialHaze: vec4<f32> };"
	"struct Froxels { values: array<vec4<f32>> };"
	"@group(0) @binding(0) var<uniform> atmosphere: Atmosphere;"
	"@group(0) @binding(1) var<storage, read> froxels: Froxels;"
	"@group(0) @binding(2) var physicalSky: texture_2d<f32>;"
	"@group(0) @binding(3) var physicalSkySampler: sampler;"
	"@group(0) @binding(4) var currentSkyRadiance: texture_2d_array<f32>;"
	"@group(0) @binding(5) var previousSkyRadiance: texture_2d_array<f32>;"
	"@group(0) @binding(6) var skyRadianceSampler: sampler;"
	"@group(0) @binding(7) var currentSkyIrradiance: texture_2d_array<f32>;"
	"@group(0) @binding(8) var previousSkyIrradiance: texture_2d_array<f32>;"
	/* Per-batch material textures (implicit bind group from the draw record). */
	"@group(2) @binding(0) var baseTexture: texture_2d<f32>;"
	"@group(2) @binding(1) var baseSampler: sampler;"
	"@group(2) @binding(2) var lightmapTexture: texture_2d<f32>;"
	"fn sky_coord(direction: vec3<f32>) -> vec3<f32> {"
	" let a=abs(direction); var face=0i; var st=vec2<f32>(0.0);"
	" if(a.x>=a.y&&a.x>=a.z){if(direction.x>=0.0){face=0;st=vec2<f32>(-direction.y,direction.z)/a.x;}"
	" else{face=1;st=vec2<f32>(direction.y,direction.z)/a.x;}}"
	" else if(a.y>=a.z){if(direction.y>=0.0){face=2;st=vec2<f32>(direction.x,direction.z)/a.y;}"
	" else{face=3;st=vec2<f32>(-direction.x,direction.z)/a.y;}}"
	" else if(direction.z>=0.0){face=4;st=vec2<f32>(-direction.y,-direction.x)/a.z;}"
	" else{face=5;st=vec2<f32>(-direction.y,direction.x)/a.z;}"
	" return vec3<f32>(st.x*0.5+0.5,0.5-st.y*0.5,f32(face)); }"
	/* Aerial-perspective haze, same semantics as the Vulkan product composite
	 * (sky_surface_composite.comp wiredAerialInscatter). */
	"fn wired_aerial_inscatter(mediaColor: vec3<f32>, viewDir: vec3<f32>) -> vec3<f32> {"
	" let aerial=clamp(atmosphere.aerialHaze.w,0.0,1.0);"
	" if (aerial<=0.0) { return mediaColor; }"
	" var skyIn=vec3<f32>(0.0);"
	" if (atmosphere.skyControl.x>0.0) {"
	" let skyViewUv=vec2<f32>(fract(atan2(viewDir.y,viewDir.x)/6.2831853+0.5),"
	" clamp(asin(clamp(viewDir.z,-1.0,1.0))/3.1415927+0.5,0.0,1.0));"
	" skyIn=textureSampleLevel(physicalSky,physicalSkySampler,skyViewUv,0.0).rgb; }"
	" else { let c=sky_coord(viewDir);"
	" skyIn=textureSampleLevel(currentSkyRadiance,skyRadianceSampler,c.xy,i32(c.z),4.0).rgb; }"
	" let luma=vec3<f32>(0.2126,0.7152,0.0722);"
	/* six face-centre taps of the coarsest prefiltered mip: backend-invariant mean */
	" var skySum=vec3<f32>(0.0);"
	" for (var face=0u; face<6u; face++) { var axis=vec3<f32>(0.0);"
	" let sgn=select(1.0,-1.0,(face&1u)!=0u);"
	" if (face<2u) { axis.x=sgn; } else if (face<4u) { axis.y=sgn; } else { axis.z=sgn; }"
	" let fc=sky_coord(axis);"
	" skySum+=textureSampleLevel(currentSkyRadiance,skyRadianceSampler,fc.xy,i32(fc.z),7.0).rgb; }"
	" let skyMean=max(dot(skySum/6.0,luma),0.0001);"
	" let directional=skyIn/skyMean;"
	" let sunDir=normalize(atmosphere.aerialHaze.xyz+vec3<f32>(0.000001));"
	" let cosT=dot(viewDir,sunDir); let g=0.55;"
	" let hg=(1.0-g*g)/max(pow(1.0+g*g-2.0*g*cosT,1.5),0.0001);"
	" let tinted=mediaColor*mix(vec3<f32>(1.0),clamp(directional,vec3<f32>(0.35),vec3<f32>(3.0)),aerial);"
	" return tinted*mix(1.0,0.75+0.25*hg,aerial); }"
	"fn wired_display_visibility(c: vec3<f32>) -> vec3<f32> {"
	" var exposed=max(c,vec3<f32>(0.0))*atmosphere.displayVisibility.x;"
	" let l=dot(exposed,vec3<f32>(0.2126,0.7152,0.0722));"
	" if (l>0.0 && l<atmosphere.displayVisibility.z && atmosphere.displayVisibility.y!=1.0) {"
	" let curved=pow(l/atmosphere.displayVisibility.z,atmosphere.displayVisibility.y)"
	" *atmosphere.displayVisibility.z; exposed*=curved/l; } return exposed; }"
	"fn wired_boost_legacy_lightmap(rgb:vec3<f32>,boost:f32)->vec3<f32>{"
	" let boosted=rgb*boost; let peak=max(boosted.r,max(boosted.g,boosted.b));"
	" if(peak>1.0){return boosted/peak;} return boosted; }"
	"@fragment fn main(@location(0) color: vec4<f32>,"
	" @location(1) fogCoord: vec2<f32>, @location(2) emission: vec3<f32>,"
	" @location(3) skySurface: f32, @location(4) normalInput: vec3<f32>,"
	" @location(5) @interpolate(flat) baseColor: vec4<f32>,"
	" @location(6) @interpolate(flat) materialFactors: vec4<f32>,"
	" @location(7) uv: vec2<f32>, @location(8) lightmapUv: vec2<f32>,"
	" @location(9) @interpolate(flat) materialExtra: vec4<f32>,"
	" @builtin(position) pixel: vec4<f32>) -> @location(0) vec4<f32> {"
	/* Authored surface: vertex colour x material base colour x base texture,
	 * modulated by the lightmap when the batch carries one. */
	" let tex=textureSample(baseTexture,baseSampler,uv);"
	" let lmTex=textureSample(lightmapTexture,baseSampler,lightmapUv).rgb;"
	" let lightmapped=materialExtra.x>0.5;"
	/* Legacy lightmaps carry the overbright lighting; the BSP vertex colour
	 * is the lighting source only for vertex-lit surfaces (Metal/Vulkan). */
	" let lm=select(vec3<f32>(1.0),wired_boost_legacy_lightmap(lmTex,materialExtra.y),lightmapped);"
	" let vertexColor=select(color.rgb,vec3<f32>(1.0),lightmapped);"
	" let authored=vertexColor*baseColor.rgb*tex.rgb*lm;"
	" var surface=vec4<f32>(authored+authored*emission,color.a*baseColor.a*tex.a);"
	/* Visible sky preserves the authored stage result.  The radiance cohort is
	 * sampled by the IBL branch below, where it is complete and exact. */
	" if (skySurface > 0.5 && atmosphere.skyControl.x > 0.0) {"
	" let ndc = vec2<f32>(pixel.x * atmosphere.skyControl.y * 2.0 - 1.0,"
	" 1.0 - pixel.y * atmosphere.skyControl.z * 2.0);"
	" let direction = normalize(atmosphere.viewForward.xyz"
	" - atmosphere.viewLeft.xyz * ndc.x * atmosphere.skyControl.w"
	" + atmosphere.viewUp.xyz * ndc.y * atmosphere.viewUp.w);"
	" let uv = vec2<f32>(atan2(direction.y,direction.x) / 6.2831853 + 0.5,"
	" asin(clamp(direction.z,-1.0,1.0)) / 3.1415927 + 0.5);"
	" let physical = textureSampleLevel(physicalSky,physicalSkySampler,uv,0.0).rgb;"
	" let aEnergy=dot(max(surface.rgb,vec3<f32>(0.0)),vec3<f32>(0.2126,0.7152,0.0722));"
	" let pEnergy=dot(max(physical,vec3<f32>(0.0)),vec3<f32>(0.2126,0.7152,0.0722));"
	" var relit=physical; if(aEnergy>0.00001){"
	" let scale=clamp(pEnergy/max(aEnergy,0.18),0.65,1.75);"
	" var tint=vec3<f32>(1.0); if(pEnergy>0.00001){tint=physical/pEnergy;}"
	" relit=surface.rgb*scale*mix(vec3<f32>(1.0),clamp(tint,vec3<f32>(0.5),vec3<f32>(2.0)),0.12)+physical*0.08;}"
	" return vec4<f32>(wired_display_visibility(mix(surface.rgb,relit,"
	" clamp(atmosphere.skyControl.x,0.0,1.0))),1.0); }"
	" if(materialFactors.x>=0.0&&atmosphere.radianceControl.x>0.5){"
	" let direction=normalize(atmosphere.viewForward.xyz"
	" -atmosphere.viewLeft.xyz*((pixel.x*atmosphere.skyControl.y*2.0-1.0)*atmosphere.skyControl.w)"
	" +atmosphere.viewUp.xyz*((1.0-pixel.y*atmosphere.skyControl.z*2.0)*atmosphere.viewUp.w));"
	" let normal=normalize(normalInput); let viewDirection=-direction;"
	" let reflected=reflect(-viewDirection,normal);"
	" let diffuseCoord=sky_coord(normal); let specularCoord=sky_coord(reflected);"
	" let roughness=clamp(materialFactors.y,0.0,1.0);"
	" let blend=clamp(atmosphere.radianceControl.y,0.0,1.0);"
	" let currentDiffuse=textureSampleLevel(currentSkyIrradiance,skyRadianceSampler,diffuseCoord.xy,i32(diffuseCoord.z),0.0).rgb;"
	" let previousDiffuse=textureSampleLevel(previousSkyIrradiance,skyRadianceSampler,diffuseCoord.xy,i32(diffuseCoord.z),0.0).rgb;"
	" let currentSpecular=textureSampleLevel(currentSkyRadiance,skyRadianceSampler,specularCoord.xy,i32(specularCoord.z),roughness*7.0).rgb;"
	" let previousSpecular=textureSampleLevel(previousSkyRadiance,skyRadianceSampler,specularCoord.xy,i32(specularCoord.z),roughness*7.0).rgb;"
	" let metallic=clamp(materialFactors.x,0.0,1.0);"
	" let f0=mix(vec3<f32>(0.04),surface.rgb,metallic);"
	" let ndv=max(dot(normal,viewDirection),0.0);"
	" let fresnel=f0+(vec3<f32>(1.0)-f0)*pow(1.0-ndv,5.0);"
	" let diffuse=mix(previousDiffuse,currentDiffuse,blend)*surface.rgb*(1.0-metallic);"
	" let specular=mix(previousSpecular,currentSpecular,blend)*fresnel;"
	" surface=vec4<f32>(surface.rgb+(diffuse+specular)*clamp(materialFactors.z,0.0,1.0),surface.a); }"
	" if (atmosphere.froxelGrid.z > 0u) {"
	" let x=min(u32(pixel.x)/16u,atmosphere.froxelGrid.x-1u);"
	" let y=min(u32(pixel.y)/16u,atmosphere.froxelGrid.y-1u);"
	" let farZ=max(atmosphere.colorVisibility.w,4.01);"
	" let slice=clamp(log(max(1.0/pixel.w,4.0)/4.0)/log(farZ/4.0),0.0,0.9999);"
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
	" var inscatter = atmosphere.colorVisibility.rgb;"
	" if (atmosphere.aerialHaze.w > 0.0) {"
	" let viewDir=normalize(atmosphere.viewForward.xyz"
	" -atmosphere.viewLeft.xyz*((pixel.x*atmosphere.skyControl.y*2.0-1.0)*atmosphere.skyControl.w)"
	" +atmosphere.viewUp.xyz*((1.0-pixel.y*atmosphere.skyControl.z*2.0)*atmosphere.viewUp.w));"
	" inscatter = wired_aerial_inscatter(inscatter, viewDir); }"
	" lit = mix(inscatter, lit, clamp(transmittance, 0.0, 1.0)); }"
	" return vec4<f32>(wired_display_visibility(lit), surface.a); }\n";
static const char s_uiVertexWgsl[]	 = "struct VertexOut { @builtin(position) position: vec4<f32>,"
									   " @location(0) uv: vec2<f32>, @location(1) color: vec4<f32> };"
									   "@vertex fn main(@location(0) p: vec3<f32>, @location(1) uv: vec2<f32>,"
									   " @location(2) color: vec4<f32>) -> VertexOut {"
									   " var out: VertexOut; out.position = vec4<f32>(p, 1.0);"
									   " out.uv = uv; out.color = color; return out; }\n";
/* UI vertex colours are authored sRGB; the canvas -srgb view encodes on
 * write, so decode them here exactly like the Metal UI fragment. */
static const char s_uiFragmentWgsl[] = "@group(0) @binding(0) var image: texture_2d<f32>;"
									   "@group(0) @binding(1) var imageSampler: sampler;"
									   "fn wired_srgb_to_linear(c:vec3<f32>)->vec3<f32>{"
									   " return select(pow((c+vec3<f32>(0.055))/1.055,vec3<f32>(2.4)),c/12.92,c<=vec3<f32>(0.04045));}"
									   "@fragment fn main(@location(0) uv: vec2<f32>, @location(1) color: vec4<f32>)"
									   " -> @location(0) vec4<f32> { let tint=vec4<f32>(wired_srgb_to_linear(color.rgb),color.a);"
									   " return textureSample(image,imageSampler,uv)*tint; }\n";
static const char s_msdfFragmentWgsl[] = "@group(0) @binding(0) var atlas: texture_2d<f32>;"
									   "@group(0) @binding(1) var atlasSampler: sampler;"
									   "fn median(r:f32,g:f32,b:f32)->f32{return max(min(r,g),min(max(r,g),b));}"
									   "fn wired_srgb_to_linear(c:vec3<f32>)->vec3<f32>{"
									   " return select(pow((c+vec3<f32>(0.055))/1.055,vec3<f32>(2.4)),c/12.92,c<=vec3<f32>(0.04045));}"
									   "@fragment fn main(@location(0) uv: vec2<f32>, @location(1) color: vec4<f32>)"
									   " -> @location(0) vec4<f32> { let s=textureSample(atlas,atlasSampler,uv);"
									   " let d=median(s.r,s.g,s.b)-0.5; let a=clamp(d/max(fwidth(d),0.0001)+0.5,0.0,1.0);"
									   " return vec4<f32>(wired_srgb_to_linear(color.rgb),color.a*a); }\n";

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
	ralAtmospherePlanReceipt_t		receipt;
	ralSkyEnvironmentPlanRequest_t skyPlan;
	ralSkyEnvironmentRuntimeRequest_t skyRuntimeRequest;
	ralSkyEnvironmentRuntimeReceipt_t skyRuntimeReceipt;
	ralSkyEnvironmentCapabilities_t skyCapabilities;
	uint64_t skyWorldLifecycleGeneration;
	uint32_t skyEventMask = 0u;
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
	request.maxLocalVolumes = RAL_ATMOSPHERE_MAX_VOLUMES;
	request.maxLights = RAL_ATMOSPHERE_MAX_LIGHTS;
	if ( !s_module.frontend.sceneRendered ) {
		/* No camera was submitted. Plan zero work without advancing the resident
		 * world's sky history, cancellation state or radiance cadence. */
		request.requestedTier = RAL_ATMOSPHERE_TIER_OFF;
		return Ral_AtmospherePlan( &request, outReceipt );
	}
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
	request.cameraCut = RenderSubmission_ViewCameraCut( &s_module.frontend );
	if ( request.cameraCut ) {
		RalWebGpu_AtmosphereInvalidateHistory( s_module.atmosphere );
		skyEventMask |= RAL_SKY_EVENT_CAMERA_CUT;
	}
	request.historyValid = RalWebGpu_AtmosphereHistoryValid( s_module.atmosphere );
	memset( &skyCapabilities, 0, sizeof( skyCapabilities ) );
	skyWorldLifecycleGeneration = 0u;
	if ( s_module.frontend.activeWorldIndex >= 0
			&& !RenderSubmission_WorldLifecycleGeneration( &s_module.frontend,
				s_module.frontend.activeWorldIndex,
				&skyWorldLifecycleGeneration ) ) return qfalse;
	if ( skyWorldLifecycleGeneration != 0u
			&& s_module.skyWorldLifecycleGeneration != 0u
			&& s_module.skyWorldLifecycleGeneration
				!= skyWorldLifecycleGeneration )
		skyEventMask |= RAL_SKY_EVENT_MAP_TRANSITION;
	if ( skyWorldLifecycleGeneration != 0u && s_module.product
			&& !RalWebGpu_ProductEnsureSkyRadianceResources( s_module.product,
				&s_module.borrow.runtimeReceipt,
				skyWorldLifecycleGeneration ) ) return qfalse;
	if ( skyWorldLifecycleGeneration != 0u && s_module.sky
			&& !RalWebGpu_SkyEnsureWorld( s_module.sky,
				&s_module.borrow.runtimeReceipt,
				skyWorldLifecycleGeneration ) ) return qfalse;
	if ( skyWorldLifecycleGeneration != 0u
			&& !RefreshWorldAtmosphereOwner( qfalse ) ) return qfalse;
	skyCapabilities.physicalLuts = RalWebGpu_SkyResourcesReady( s_module.sky );
	skyCapabilities.temporalCrossfade = skyCapabilities.physicalLuts;
	/* WebGPU lowers the shared distant-world plan through distinct resident
	 * buffers and explicit color-load/depth-clear pass boundaries. */
	skyCapabilities.distantScene = qtrue;
	if ( !RenderSubmission_BuildSkyEnvironmentRuntimeRequest( &snapshot,
			&s_module.skyRuntime, RAL_BACKEND_WEBGPU, frameGeneration,
			request.requestedTier, skyEventMask, s_module.completedSkyWorkMask,
			1u, snapshot.state.timelineSeconds, &skyCapabilities,
			&skyRuntimeRequest ) ||
		 !Ral_SkyEnvironmentRuntimeAdvance( &s_module.skyRuntime,
			&skyRuntimeRequest, &skyPlan, &skyRuntimeReceipt ) ) return qfalse;
	s_module.pendingSkyEventMask = skyRuntimeRequest.eventMask;
	s_module.completedSkyWorkMask = 0u;
	request.skyEnvironment = skyPlan;
	if ( !Ral_AtmospherePlan( &request, &receipt ) ) return qfalse;
	if ( skyWorldLifecycleGeneration != 0u )
		s_module.skyWorldLifecycleGeneration = skyWorldLifecycleGeneration;
	*outReceipt = receipt;
	return qtrue;
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
	static const ralWebGpuSkyRadianceReceipt_t noRadiance;
	static const ralSkyGpuTelemetryReceipt_t noTelemetry;
	const uint32_t physicalSkyMask = RAL_SKY_DIRTY_TRANSMITTANCE
		| RAL_SKY_DIRTY_MULTISCATTER | RAL_SKY_DIRTY_SKY_VIEW;
	const uint32_t admittedPhysicalSky = receipt
		? receipt->atmosphere.skyEnvironment.admittedWorkMask & physicalSkyMask
		: 0u;
	return receipt && receipt->schemaVersion == RAL_WEBGPU_RENDERER_MODULE_SCHEMA_VERSION &&
		   receipt->backendType == RAL_BACKEND_WEBGPU && receipt->moduleGeneration && receipt->frameGeneration &&
		   RenderSubmission_ReceiptExact( &receipt->frontend, &receipt->frontend ) &&
		   RalWebGpu_ProductFrameReceiptExact( &receipt->product, &receipt->product ) &&
		   Ral_DisplayVisibilityPlanValid( &receipt->displayVisibility ) &&
		   Ral_AtmospherePlanReceiptExact( &receipt->atmosphere, &receipt->atmosphere ) &&
		   ( admittedPhysicalSky
				 ? ( RalWebGpu_SkyReceiptExact( &receipt->skyExecution,
					&receipt->skyExecution )
					&& receipt->skyExecution.frameGeneration == receipt->frameGeneration
					&& receipt->skyExecution.workMask == admittedPhysicalSky )
				 : receipt->skyExecution.schemaVersion == 0u ) &&
		   ( receipt->frontend.sceneRendered ? RalWebGpu_SkyRadianceReceiptExact(
				&receipt->skyRadianceExecution,
				&receipt->skyRadianceExecution )
			 : ( !memcmp( &receipt->skyRadianceExecution, &noRadiance, sizeof( noRadiance ) )
				 && receipt->atmosphere.selectedTier == RAL_ATMOSPHERE_TIER_OFF
				 && receipt->atmosphere.zeroWork && receipt->weather.zeroWork
				 && !receipt->product.worldDrawCount && !receipt->product.entityDrawCount
				 && !receipt->product.atmosphereBoundDrawCount
				 && !receipt->product.toneMapDrawCount && !receipt->product.exposureDispatchCount ) ) &&
		   ( receipt->frontend.sceneRendered
			 ? Ral_SkyGpuTelemetryReceiptExact( &receipt->skyGpuTelemetry, &receipt->skyGpuTelemetry )
			 : !memcmp( &receipt->skyGpuTelemetry, &noTelemetry, sizeof( noTelemetry ) ) ) &&
		   Ral_AtmosphereWeatherReceiptExact( &receipt->weather, &receipt->weather ) &&
		   Ral_AtmosphereWeatherReceiptExact( &receipt->weather, &receipt->product.weather ) &&
		   receipt->frontend.ownerGeneration == receipt->moduleGeneration &&
		   receipt->frontend.frameGeneration == receipt->frameGeneration &&
		   receipt->product.frameGeneration == receipt->frameGeneration &&
		   receipt->atmosphere.frameGeneration == receipt->frameGeneration &&
		   ( receipt->atmosphere.computeDispatchCount != 0u
				 ? ( RalWebGpu_AtmosphereReceiptExact( &receipt->atmosphereExecution, &receipt->atmosphereExecution ) &&
					 receipt->atmosphereExecution.frameGeneration == receipt->frameGeneration &&
					 receipt->atmosphereExecution.dispatchCount == receipt->atmosphere.computeDispatchCount )
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

static qboolean IsEffectPrimitivePipeline( uint32_t index ) {
	return index >= WEBGPU_PIPELINE_EFFECT_ALPHA_DEPTH
		&& index <= WEBGPU_PIPELINE_EFFECT_ADDITIVE_NO_DEPTH;
}

static qboolean EffectPipelineAdditive( uint32_t index ) {
	return index == WEBGPU_PIPELINE_EFFECT_ADDITIVE_DEPTH
		|| index == WEBGPU_PIPELINE_EFFECT_ADDITIVE_NO_DEPTH;
}

static qboolean EffectPipelineUsesDepth( uint32_t index ) {
	return index == WEBGPU_PIPELINE_EFFECT_ALPHA_DEPTH
		|| index == WEBGPU_PIPELINE_EFFECT_ADDITIVE_DEPTH;
}

static qboolean CreatePipeline( uint32_t index )
{
	uint32_t						spirv[2][4] = { { 0x07230203u }, { 0x07230203u } };
	ralShaderModuleAbi_t			modules[2];
	ralShaderVertexInputAbi_t		shaderInputs[7];
	ralShaderBindingAbi_t			atmosphereBindings[13];
	ralShaderAbiManifest_t			manifest;
	ralShaderVariantAbi_t			variant;
	ralWebGpuWgslModule_t			wgsl[2];
	ralVertexBinding_t				binding;
	ralVertexAttribute_t			attributes[7];
	ralColorBlendAttachment_t		blend;
	const ralBindGroupLayout_t	   *groupLayouts[3];
	ralGraphicsPipelineCreateInfo_t graphics;
	ralWebGpuPipelineCreateInfo_t	info;
	qboolean isUi = index == WEBGPU_PIPELINE_UI || index == WEBGPU_PIPELINE_MSDF;
	qboolean isEffectPrimitive = IsEffectPrimitivePipeline( index );
	const ralShaderSpecConstantAbi_t effectSpec = {0,RAL_STAGE_FRAGMENT,0,RAL_SHADER_SPEC_U32};
	const ralShaderSpecValue_t effectValue = {0,EffectPipelineAdditive(index) ? 1u : 0u};
	const ralSpecConstant_t nativeEffectValue = {0,effectValue.value};
	const char					   *vertexWgsl = isUi
		? s_uiVertexWgsl : ( isEffectPrimitive ? s_effectVertexWgsl
			: ( index == WEBGPU_PIPELINE_WORLD ? s_worldVertexWgsl
			: ( index == WEBGPU_PIPELINE_ENTITY ? s_entityVertexWgsl
				: s_vertexWgsl ) ) );
	const char					   *fragmentWgsl = index == WEBGPU_PIPELINE_MSDF
		? s_msdfFragmentWgsl : ( index == WEBGPU_PIPELINE_UI ? s_uiFragmentWgsl : ( index == WEBGPU_PIPELINE_WORLD
			? s_worldFragmentWgsl : ( isEffectPrimitive ? s_effectFragmentWgsl
				: ( index == WEBGPU_PIPELINE_ENTITY ? s_entityFragmentWgsl
					: s_fragmentWgsl ) ) ) );
	uint32_t						vertexWgslBytes =
		(uint32_t)( isUi ? sizeof( s_uiVertexWgsl ) - 1u
			: ( isEffectPrimitive ? sizeof( s_effectVertexWgsl ) - 1u
				: ( index == WEBGPU_PIPELINE_WORLD ? sizeof( s_worldVertexWgsl ) - 1u
				: ( index == WEBGPU_PIPELINE_ENTITY ? sizeof( s_entityVertexWgsl ) - 1u
					: sizeof( s_vertexWgsl ) - 1u ) ) ) );
	uint32_t fragmentWgslBytes =
		(uint32_t)( index == WEBGPU_PIPELINE_MSDF ? sizeof( s_msdfFragmentWgsl ) - 1u
			: ( index == WEBGPU_PIPELINE_UI ? sizeof( s_uiFragmentWgsl ) - 1u
			: ( index == WEBGPU_PIPELINE_WORLD ? sizeof( s_worldFragmentWgsl ) - 1u
				: ( isEffectPrimitive ? sizeof( s_effectFragmentWgsl ) - 1u
					: ( index == WEBGPU_PIPELINE_ENTITY ? sizeof( s_entityFragmentWgsl ) - 1u
						: sizeof( s_fragmentWgsl ) - 1u ) ) ) ) );
	uint32_t vertexInputCount = isUi ? 3u
		: ( index == WEBGPU_PIPELINE_WORLD ? 7u
		: ( index == WEBGPU_PIPELINE_ENTITY ? 6u
		: ( isEffectPrimitive ? 5u : 4u ) ) );
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
	/* World only: authored base UV and lightmap UV. */
	shaderInputs[5].location = 5u;
	shaderInputs[5].format   = RAL_FORMAT_R32G32_SFLOAT;
	shaderInputs[6].location = 6u;
	shaderInputs[6].format   = RAL_FORMAT_R32G32_SFLOAT;
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
	atmosphereBindings[1].minBufferBindingSize = (uint64_t)RAL_WEBGPU_ATMOSPHERE_FROXEL_CAPACITY * 16u;
	if ( index == WEBGPU_PIPELINE_WORLD ) {
		atmosphereBindings[2].set = 0u;
		atmosphereBindings[2].binding = 2u;
		atmosphereBindings[2].bindingClass = RAL_SHADER_BIND_SAMPLED_TEXTURE;
		atmosphereBindings[2].arrayCount = 1u;
		atmosphereBindings[2].stageFlags = RAL_STAGE_FRAGMENT;
		atmosphereBindings[2].viewDimension = RAL_SHADER_VIEW_2D;
		atmosphereBindings[2].sampleType = RAL_SHADER_SAMPLE_FLOAT;
		atmosphereBindings[3].set = 0u;
		atmosphereBindings[3].binding = 3u;
		atmosphereBindings[3].bindingClass = RAL_SHADER_BIND_FILTERING_SAMPLER;
		atmosphereBindings[3].arrayCount = 1u;
		atmosphereBindings[3].stageFlags = RAL_STAGE_FRAGMENT;
		for ( uint32_t bindingIndex = 4u; bindingIndex <= 5u; ++bindingIndex ) {
			atmosphereBindings[bindingIndex].set = 0u;
			atmosphereBindings[bindingIndex].binding = bindingIndex;
			atmosphereBindings[bindingIndex].bindingClass =
				RAL_SHADER_BIND_SAMPLED_TEXTURE;
			atmosphereBindings[bindingIndex].arrayCount = 1u;
			atmosphereBindings[bindingIndex].stageFlags = RAL_STAGE_FRAGMENT;
			atmosphereBindings[bindingIndex].viewDimension =
				RAL_SHADER_VIEW_2D_ARRAY;
			atmosphereBindings[bindingIndex].sampleType = RAL_SHADER_SAMPLE_FLOAT;
		}
		atmosphereBindings[6].set = 0u;
		atmosphereBindings[6].binding = 6u;
		atmosphereBindings[6].bindingClass = RAL_SHADER_BIND_FILTERING_SAMPLER;
		atmosphereBindings[6].arrayCount = 1u;
		atmosphereBindings[6].stageFlags = RAL_STAGE_FRAGMENT;
		for ( uint32_t bindingIndex = 7u; bindingIndex <= 8u; ++bindingIndex ) {
			atmosphereBindings[bindingIndex].set = 0u;
			atmosphereBindings[bindingIndex].binding = bindingIndex;
			atmosphereBindings[bindingIndex].bindingClass =
				RAL_SHADER_BIND_SAMPLED_TEXTURE;
			atmosphereBindings[bindingIndex].arrayCount = 1u;
			atmosphereBindings[bindingIndex].stageFlags = RAL_STAGE_FRAGMENT;
			atmosphereBindings[bindingIndex].viewDimension =
				RAL_SHADER_VIEW_2D_ARRAY;
			atmosphereBindings[bindingIndex].sampleType =
				RAL_SHADER_SAMPLE_FLOAT;
		}
		atmosphereBindings[9].set = 1u;
		atmosphereBindings[9].binding = 0u;
		atmosphereBindings[9].bindingClass = RAL_SHADER_BIND_STORAGE_BUFFER_READ;
		atmosphereBindings[9].arrayCount = 1u;
		atmosphereBindings[9].stageFlags = RAL_STAGE_VERTEX;
		atmosphereBindings[9].minBufferBindingSize =
			(uint64_t)RENDER_SUBMISSION_MAX_WORLD_BATCHES * 16u * sizeof( float );
		/* Group 2: per-batch material textures (base, sampler, lightmap). */
		atmosphereBindings[10].set = 2u;
		atmosphereBindings[10].binding = 0u;
		atmosphereBindings[10].bindingClass = RAL_SHADER_BIND_SAMPLED_TEXTURE;
		atmosphereBindings[10].arrayCount = 1u;
		atmosphereBindings[10].stageFlags = RAL_STAGE_FRAGMENT;
		atmosphereBindings[10].viewDimension = RAL_SHADER_VIEW_2D;
		atmosphereBindings[10].sampleType = RAL_SHADER_SAMPLE_FLOAT;
		atmosphereBindings[11].set = 2u;
		atmosphereBindings[11].binding = 1u;
		atmosphereBindings[11].bindingClass = RAL_SHADER_BIND_FILTERING_SAMPLER;
		atmosphereBindings[11].arrayCount = 1u;
		atmosphereBindings[11].stageFlags = RAL_STAGE_FRAGMENT;
		atmosphereBindings[12].set = 2u;
		atmosphereBindings[12].binding = 2u;
		atmosphereBindings[12].bindingClass = RAL_SHADER_BIND_SAMPLED_TEXTURE;
		atmosphereBindings[12].arrayCount = 1u;
		atmosphereBindings[12].stageFlags = RAL_STAGE_FRAGMENT;
		atmosphereBindings[12].viewDimension = RAL_SHADER_VIEW_2D;
		atmosphereBindings[12].sampleType = RAL_SHADER_SAMPLE_FLOAT;
	} else if ( index == WEBGPU_PIPELINE_ENTITY ) {
		atmosphereBindings[2].set = 1u;
		atmosphereBindings[2].binding = 0u;
		atmosphereBindings[2].bindingClass = RAL_SHADER_BIND_STORAGE_BUFFER_READ;
		atmosphereBindings[2].arrayCount = 1u;
		atmosphereBindings[2].stageFlags = RAL_STAGE_VERTEX;
		atmosphereBindings[2].minBufferBindingSize =
			(uint64_t)RENDER_SUBMISSION_MAX_ENTITIES
				* RENDER_SUBMISSION_MAX_MODEL_BATCHES * 16u * sizeof( float );
		/* Group 2: per-draw material base texture + sampler. */
		atmosphereBindings[3].set = 2u;
		atmosphereBindings[3].binding = 0u;
		atmosphereBindings[3].bindingClass = RAL_SHADER_BIND_SAMPLED_TEXTURE;
		atmosphereBindings[3].arrayCount = 1u;
		atmosphereBindings[3].stageFlags = RAL_STAGE_FRAGMENT;
		atmosphereBindings[3].viewDimension = RAL_SHADER_VIEW_2D;
		atmosphereBindings[3].sampleType = RAL_SHADER_SAMPLE_FLOAT;
		atmosphereBindings[4].set = 2u;
		atmosphereBindings[4].binding = 1u;
		atmosphereBindings[4].bindingClass = RAL_SHADER_BIND_FILTERING_SAMPLER;
		atmosphereBindings[4].arrayCount = 1u;
		atmosphereBindings[4].stageFlags = RAL_STAGE_FRAGMENT;
	} else if ( isEffectPrimitive ) {
		atmosphereBindings[2].set = 0u;
		atmosphereBindings[2].binding = 2u;
		atmosphereBindings[2].bindingClass = RAL_SHADER_BIND_STORAGE_BUFFER_READ;
		atmosphereBindings[2].arrayCount = 1u;
		atmosphereBindings[2].stageFlags = RAL_STAGE_VERTEX;
		atmosphereBindings[2].minBufferBindingSize = RAL_WEBGPU_EFFECT_PARAMETER_BYTES;
		atmosphereBindings[3].set = 1u;
		atmosphereBindings[3].binding = 0u;
		atmosphereBindings[3].bindingClass = RAL_SHADER_BIND_SAMPLED_TEXTURE;
		atmosphereBindings[3].arrayCount = 1u;
		atmosphereBindings[3].stageFlags = RAL_STAGE_FRAGMENT;
		atmosphereBindings[3].viewDimension = RAL_SHADER_VIEW_2D;
		atmosphereBindings[3].sampleType = RAL_SHADER_SAMPLE_FLOAT;
		atmosphereBindings[4].set = 1u;
		atmosphereBindings[4].binding = 1u;
		atmosphereBindings[4].bindingClass = RAL_SHADER_BIND_FILTERING_SAMPLER;
		atmosphereBindings[4].arrayCount = 1u;
		atmosphereBindings[4].stageFlags = RAL_STAGE_FRAGMENT;
	}
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
		manifest.bindingCount = index == WEBGPU_PIPELINE_WORLD ? 13u
			: ( index == WEBGPU_PIPELINE_ENTITY ? 5u
				: ( isEffectPrimitive ? 5u : 2u ) );
	}
	wgsl[0]		  = (ralWebGpuWgslModule_t){ vertexWgsl, vertexWgslBytes };
	wgsl[1]		  = (ralWebGpuWgslModule_t){ fragmentWgsl, fragmentWgslBytes };
	if ( index == WEBGPU_PIPELINE_WORLD || index == WEBGPU_PIPELINE_ENTITY ) {
		/* Projected world/entity vertex: authored vertex intact + clipW + fog. */
		binding		  = (ralVertexBinding_t){ 0u, sizeof( ralWebGpuWorldVertex_t ), RAL_VERTEX_INPUT_PER_VERTEX };
		attributes[0] = (ralVertexAttribute_t){ 0u, 0u, RAL_FORMAT_R32G32B32_SFLOAT,
			(uint32_t)offsetof( ralWebGpuWorldVertex_t, authored.position ) };
		attributes[1] = (ralVertexAttribute_t){ 1u, 0u, RAL_FORMAT_R32_SFLOAT,
			(uint32_t)offsetof( ralWebGpuWorldVertex_t, clipW ) };
		attributes[2] = (ralVertexAttribute_t){ 2u, 0u, RAL_FORMAT_R8G8B8A8_UNORM,
			(uint32_t)offsetof( ralWebGpuWorldVertex_t, authored.color ) };
		attributes[3] = (ralVertexAttribute_t){ 3u, 0u, RAL_FORMAT_R32G32_SFLOAT,
			(uint32_t)offsetof( ralWebGpuWorldVertex_t, fogCoord ) };
		attributes[4] = (ralVertexAttribute_t){ 4u, 0u, RAL_FORMAT_R32G32B32_SFLOAT,
			(uint32_t)offsetof( ralWebGpuWorldVertex_t, authored.normal ) };
		attributes[5] = (ralVertexAttribute_t){ 5u, 0u, RAL_FORMAT_R32G32_SFLOAT,
			(uint32_t)offsetof( ralWebGpuWorldVertex_t, authored.texCoord ) };
		attributes[6] = (ralVertexAttribute_t){ 6u, 0u, RAL_FORMAT_R32G32_SFLOAT,
			(uint32_t)offsetof( ralWebGpuWorldVertex_t, authored.lightmapCoord ) };
	} else {
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
	}
	memset( &blend, 0, sizeof( blend ) );
	blend.writeMask			= RAL_COLOR_WRITE_ALL;
	blend.writeMaskExplicit = qtrue;
	if ( isUi || isEffectPrimitive ) {
		blend.blendEnable = qtrue;
		blend.srcColor = RAL_BLEND_SRC_ALPHA;
		blend.dstColor = isEffectPrimitive && EffectPipelineAdditive( index )
			? RAL_BLEND_ONE : RAL_BLEND_ONE_MINUS_SRC_ALPHA;
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
	/* UI draws into the canvas -srgb view (hardware display encoding). */
	graphics.colorFormats[0] = isUi ? RAL_FORMAT_B8G8R8A8_SRGB
        : RAL_FORMAT_R16G16B16A16_SFLOAT;
	graphics.numColorFormats	 = 1u;
	graphics.sampleCount		 = 1u;
    /* UI has an independent output pass without a scene-depth attachment. */
    graphics.depthFormat = isUi ? RAL_FORMAT_UNDEFINED : RAL_FORMAT_D32_SFLOAT;
    graphics.depthStencil.depthCompareOp = RAL_COMPARE_ALWAYS;
	if ( !isUi && ( !isEffectPrimitive || EffectPipelineUsesDepth( index ) ) ) {
		graphics.depthStencil.depthTestEnable = qtrue;
		graphics.depthStencil.depthWriteEnable =
			index == WEBGPU_PIPELINE_WORLD || index == WEBGPU_PIPELINE_ENTITY;
		graphics.depthStencil.depthCompareOp = RAL_COMPARE_LESS_EQUAL;
	}
	if ( isUi ) {
		groupLayouts[0] = (const ralBindGroupLayout_t *)(uintptr_t)1u;
		graphics.bindGroupLayouts = groupLayouts;
		graphics.numBindGroupLayouts = 1u;
	} else {
		groupLayouts[0]				 = (const ralBindGroupLayout_t *)(uintptr_t)1u;
		groupLayouts[1]				 = (const ralBindGroupLayout_t *)(uintptr_t)2u;
		groupLayouts[2]				 = (const ralBindGroupLayout_t *)(uintptr_t)3u;
		graphics.bindGroupLayouts	 = groupLayouts;
		graphics.numBindGroupLayouts = index == WEBGPU_PIPELINE_WORLD
				|| index == WEBGPU_PIPELINE_ENTITY ? 3u
			: ( isEffectPrimitive ? 2u : 1u );
	}
	memset( &variant, 0, sizeof( variant ) );
	variant.artifactTarget = RAL_SHADER_ARTIFACT_WGSL;
	variant.generation	   = generation;
	if (isEffectPrimitive) {
		manifest.specConstants = &effectSpec; manifest.specConstantCount = 1;
		variant.specValues = &effectValue; variant.specValueCount = 1;
		graphics.specConstants = &nativeEffectValue; graphics.numSpecConstants = 1;
	}
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
	ralWebGpuBrowserBindResource_t entries[9];
	uintptr_t skyTexture, skySampler, radianceFallback;
	uint64_t skyGeneration;
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
	if ( !RalWebGpu_SkySampleResources( s_module.sky, &skyTexture,
			&skySampler, &skyGeneration ) ) return qfalse;
	entries[2].binding = 2u;
	entries[2].kind = RAL_WEBGPU_BROWSER_BIND_RESOURCE_TEXTURE;
	entries[2].resourceIdentity = skyTexture;
	entries[3].binding = 3u;
	entries[3].kind = RAL_WEBGPU_BROWSER_BIND_RESOURCE_SAMPLER;
	entries[3].resourceIdentity = skySampler;
	if ( !RalWebGpu_SkyRadianceFallbackResources( s_module.sky,
			&radianceFallback, &skySampler ) ) return qfalse;
	for ( uint32_t binding = 4u; binding <= 5u; ++binding ) {
		entries[binding].binding = binding;
		entries[binding].kind = RAL_WEBGPU_BROWSER_BIND_RESOURCE_TEXTURE;
		entries[binding].resourceIdentity = radianceFallback;
	}
	entries[6].binding = 6u;
	entries[6].kind = RAL_WEBGPU_BROWSER_BIND_RESOURCE_SAMPLER;
	entries[6].resourceIdentity = skySampler;
	for ( uint32_t binding = 7u; binding <= 8u; ++binding ) {
		entries[binding].binding = binding;
		entries[binding].kind = RAL_WEBGPU_BROWSER_BIND_RESOURCE_TEXTURE;
		entries[binding].resourceIdentity = radianceFallback;
	}
	for ( uint32_t i = 0u; i < 2u + RAL_WEBGPU_EFFECT_PIPELINE_COUNT; ++i ) {
		if (IsEffectPrimitivePipeline(i)) {
			memset(&entries[2], 0, sizeof(entries[2]));
			entries[2].binding = 2u;
			entries[2].kind = RAL_WEBGPU_BROWSER_BIND_RESOURCE_BUFFER;
			if (!RalWebGpu_ProductGetEffectParameterBuffer(s_module.product, &s_module.borrow.runtimeReceipt,
					&entries[2].resourceIdentity, &entries[2].byteSize)) return qfalse;
		}
		const uint32_t expectedLayouts = i == WEBGPU_PIPELINE_WORLD
				|| i == WEBGPU_PIPELINE_ENTITY ? 3u
			: ( IsEffectPrimitivePipeline( i ) ? 2u : 1u );
		if ( s_module.pipelineReceipts[i].bindGroupLayoutCount != expectedLayouts ||
			 !RalWebGpu_BrowserBridgeCreateBindGroup( s_module.borrow.bridge,
													  s_module.pipelineReceipts[i].bindGroupLayoutIdentities[0],
											  entries, i == WEBGPU_PIPELINE_WORLD ? 9u : IsEffectPrimitivePipeline(i) ? 3u : 2u,
											  &s_module.atmosphereBindGroups[i] ) )
			return qfalse;
	}
	s_module.atmosphereSkyBindingGeneration = skyGeneration;
	return qtrue;
}

static qboolean RefreshWorldAtmosphereOwner( qboolean forceFallback )
{
	ralWebGpuBrowserBindResource_t entries[9];
	ralWebGpuSkyRadianceResources_t radiance;
	uintptr_t candidate = 0u, skyTexture, skySampler, radianceFallback;
	uint64_t skyGeneration;
	if ( !s_module.product || !s_module.atmosphereBindGroups[1]
			|| !s_module.atmosphereBindGroups[2]
			|| ( forceFallback
				? !RalWebGpu_SkyFallbackResources( s_module.sky, &skyTexture,
					&skySampler )
				: !RalWebGpu_SkySampleResources( s_module.sky, &skyTexture,
					&skySampler, &skyGeneration ) ) ) return qfalse;
	if ( forceFallback ) skyGeneration = 0u;
	skyGeneration ^= s_module.skyRadianceConsumer.currentProductRevision
		+ UINT64_C( 0x9e3779b97f4a7c15 );
	if ( s_module.atmosphereBindGroups[0]
			&& s_module.atmosphereSkyBindingGeneration == skyGeneration )
		return qtrue;
	memset( entries, 0, sizeof( entries ) );
	entries[0].binding = 0u;
	entries[0].kind = RAL_WEBGPU_BROWSER_BIND_RESOURCE_BUFFER;
	entries[0].resourceIdentity = s_module.atmosphereUniformReceipt.resourceIdentity;
	entries[0].byteSize = sizeof( wiredWebGpuAtmosphereUniform_t );
	entries[1].binding = 1u;
	entries[1].kind = RAL_WEBGPU_BROWSER_BIND_RESOURCE_BUFFER;
	entries[1].resourceIdentity = RalWebGpu_AtmosphereArenaIdentity( s_module.atmosphere );
	entries[1].byteSize = RalWebGpu_AtmosphereArenaBytes( s_module.atmosphere );
	entries[2].binding = 2u;
	entries[2].kind = RAL_WEBGPU_BROWSER_BIND_RESOURCE_TEXTURE;
	entries[2].resourceIdentity = skyTexture;
	entries[3].binding = 3u;
	entries[3].kind = RAL_WEBGPU_BROWSER_BIND_RESOURCE_SAMPLER;
	entries[3].resourceIdentity = skySampler;
	if ( !RalWebGpu_SkyRadianceFallbackResources( s_module.sky,
			&radianceFallback, &skySampler ) ) return qfalse;
	memset( &radiance, 0, sizeof( radiance ) );
	if ( !forceFallback && s_module.skyWorldLifecycleGeneration
			&& !RalWebGpu_ProductSkyRadianceResources( s_module.product,
				&s_module.borrow.runtimeReceipt, &s_module.frontend,
				&radiance ) ) return qfalse;
	entries[4].binding = 4u;
	entries[4].kind = RAL_WEBGPU_BROWSER_BIND_RESOURCE_TEXTURE;
	entries[4].resourceIdentity = radiance.currentRadianceIdentity
		? radiance.currentRadianceIdentity : radianceFallback;
	entries[5].binding = 5u;
	entries[5].kind = RAL_WEBGPU_BROWSER_BIND_RESOURCE_TEXTURE;
	entries[5].resourceIdentity = s_module.skyRadianceConsumer.usePreviousProduct
		&& radiance.targetRadianceIdentity
		? radiance.targetRadianceIdentity : entries[4].resourceIdentity;
	entries[6].binding = 6u;
	entries[6].kind = RAL_WEBGPU_BROWSER_BIND_RESOURCE_SAMPLER;
	entries[6].resourceIdentity = skySampler;
	entries[7].binding = 7u;
	entries[7].kind = RAL_WEBGPU_BROWSER_BIND_RESOURCE_TEXTURE;
	entries[7].resourceIdentity = radiance.currentIrradianceIdentity
		? radiance.currentIrradianceIdentity : radianceFallback;
	entries[8].binding = 8u;
	entries[8].kind = RAL_WEBGPU_BROWSER_BIND_RESOURCE_TEXTURE;
	entries[8].resourceIdentity = s_module.skyRadianceConsumer.usePreviousProduct
		&& radiance.targetIrradianceIdentity
		? radiance.targetIrradianceIdentity : entries[7].resourceIdentity;
	if ( !RalWebGpu_BrowserBridgeCreateBindGroup( s_module.borrow.bridge,
			s_module.pipelineReceipts[WEBGPU_PIPELINE_WORLD]
				.bindGroupLayoutIdentities[0], entries, 9u, &candidate ) )
		return qfalse;
	if ( !RalWebGpu_ProductSetAtmosphereBindGroups( s_module.product,
			&s_module.borrow.runtimeReceipt, candidate,
			s_module.atmosphereBindGroups[1], &s_module.atmosphereBindGroups[2] ) ) {
		RalWebGpu_BrowserBridgeDestroyBindGroup( s_module.borrow.bridge, candidate );
		return qfalse;
	}
	if ( s_module.atmosphereBindGroups[0] )
		RalWebGpu_BrowserBridgeDestroyBindGroup( s_module.borrow.bridge,
			s_module.atmosphereBindGroups[0] );
	s_module.atmosphereBindGroups[0] = candidate;
	s_module.atmosphereSkyBindingGeneration = skyGeneration;
	return qtrue;
}

static qboolean CreateWorldMaterialOwner( void )
{
	ralWebGpuBrowserBindResource_t entry;
	uintptr_t identity;
	uint64_t byteSize;
	if ( !s_module.product
		|| s_module.pipelineReceipts[WEBGPU_PIPELINE_WORLD].bindGroupLayoutCount != 3u
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
		|| s_module.pipelineReceipts[WEBGPU_PIPELINE_ENTITY].bindGroupLayoutCount != 3u
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
	ralSkySurfaceReceipt_t          skySurface;
	const atmosphereEmitter_t	  *emitters;
	wiredWebGpuAtmosphereUniform_t uniform;
	float						   cloudTransmission, lightning;
	if ( !plan || !Ral_DisplayVisibilityPlanValid( visibility )
			|| ( s_module.skyPhysicalBlend
				&& !Ral_SkySurfaceSetPhysicalBlendScale( s_module.skyPhysicalBlend->value ) )
			|| !Ral_SkySurfacePlan( &plan->skyEnvironment,
				RalWebGpu_SkyResourcesReady( s_module.sky ), &skySurface )
			|| !s_module.atmosphereUniform || !RenderSubmission_WorldSnapshot( &s_module.frontend, &world ) ||
		 !RenderSubmission_AtmosphereSnapshot( &s_module.frontend, &snapshot, &emitters ) )
		return qfalse;
	(void)emitters;
	memset( &uniform, 0, sizeof( uniform ) );
    /* Scene stays linear HDR; final tonemap owns user visibility exactly once. */
    uniform.displayVisibility[0] = uniform.displayVisibility[1] = 1.0f;
    uniform.displayVisibility[2] = visibility->shadowPivot;
    uniform.displayVisibility[3] = 1.0f;
	memcpy( uniform.eyeDensity, world.viewOrigin, 3u * sizeof( float ) );
	memcpy( uniform.viewForward, world.viewAxis[0], 3u * sizeof( float ) );
	memcpy( uniform.viewLeft, world.viewAxis[1], 3u * sizeof( float ) );
	memcpy( uniform.viewUp, world.viewAxis[2], 3u * sizeof( float ) );
	uniform.skyControl[1] = 1.0f / (float)s_module.borrow.width;
	uniform.skyControl[2] = 1.0f / (float)s_module.borrow.height;
	uniform.skyControl[3] = tanf( world.fovX * 0.00872664625997164788f );
	uniform.viewUp[3] = tanf( world.fovY * 0.00872664625997164788f );
	if ( !isfinite( uniform.skyControl[3] ) || uniform.skyControl[3] <= 0.0f
			|| !isfinite( uniform.viewUp[3] ) || uniform.viewUp[3] <= 0.0f )
		return qfalse;
	uniform.skyControl[0] = skySurface.physicalBlend;
	if ( s_module.skyRadianceConsumer.ready ) {
		if ( !Ral_SkyRadianceConsumerReceiptExact(
				&s_module.skyRadianceConsumer,
				&s_module.skyRadianceConsumer ) ) return qfalse;
		uniform.radianceControl[0] = 1.0f;
		uniform.radianceControl[1] =
			s_module.skyRadianceConsumer.previousToCurrentBlend;
		uniform.radianceControl[2] =
			s_module.skyRadianceConsumer.usePreviousProduct ? 1.0f : 0.0f;
		uniform.radianceControl[3] = 0.0f;
	}
	if ( snapshot.active && snapshot.state.qualityTier >= ATMOSPHERE_QUALITY_ANALYTIC ) {
		uniform.eyeDensity[3]	   = snapshot.state.mediaDensity;
		uniform.colorVisibility[3] = snapshot.state.visibility;
		cloudTransmission = 1.0f - snapshot.state.cloudCover
			* plan->skyEnvironment.cloudSunOcclusion;
		lightning = snapshot.state.lightning;
		for ( uint32_t channel = 0u; channel < 3u; ++channel )
			uniform.colorVisibility[channel] =
				fminf( 16.0f, plan->skyEnvironment.ambientSh[channel]
					+ 0.08f * plan->skyEnvironment.sunRadiance[channel]
						* cloudTransmission + lightning );
		uniform.heightCloud[0] = snapshot.state.bounds[2];
		uniform.heightCloud[1] = snapshot.state.mediaHeightFalloff;
		uniform.heightCloud[2] = snapshot.state.cloudCover;
		uniform.heightCloud[3] = plan->skyEnvironment.cloudSunOcclusion;
		/* Aerial-perspective haze (analytic media only), same fill as Vulkan. */
		if ( s_module.aerialHaze && s_module.aerialHaze->value > 0.0f
				&& plan->computeDispatchCount == 0u
				&& s_module.skyRadianceConsumer.ready ) {
			memcpy( uniform.aerialHaze, plan->skyEnvironment.sunDirection,
				3u * sizeof( float ) );
			uniform.aerialHaze[3] = fminf( 1.0f, s_module.aerialHaze->value );
		}
	}
	if ( plan->computeDispatchCount != 0u ) {
		uniform.colorVisibility[3] = RalWebGpu_AtmosphereFarDistance(&snapshot.state);
		uniform.froxelGrid[0] = plan->froxelWidth;
		uniform.froxelGrid[1] = plan->froxelHeight;
		uniform.froxelGrid[2] = plan->froxelDepth;
		uniform.froxelGrid[3] = RalWebGpu_AtmosphereIntegratedBase( s_module.atmosphere );
	}
	resources = RalWebGpu_RuntimeResources( s_module.borrow.runtime, &s_module.borrow.runtimeReceipt );
	uintptr_t skyTexture,skySampler; uint64_t skyGeneration;
	if(!RalWebGpu_SkySampleResources(s_module.sky,&skyTexture,&skySampler,&skyGeneration)
		||!RalWebGpu_ProductSetPhysicalSky(s_module.product,skyTexture,skySampler,skySurface.physicalBlend)) return qfalse;
	return resources &&
		   RalWebGpu_WriteBuffer( resources, s_module.atmosphereUniform, &s_module.atmosphereUniformReceipt, 0u,
								  &uniform, sizeof( uniform ), &s_module.atmosphereWriteReceipt );
}

static void DestroyDirectionalLighting( int worldIndex ) {
	ralWebGpuResourceLayer_t *resources = NULL;
	wiredWebGpuWorldLighting_t *slot;
	if ( worldIndex < 0 || worldIndex >= MAX_RENDER_WORLDS ) return;
	slot = &s_module.worldLighting[worldIndex];
	if ( s_module.borrow.runtime )
		resources = RalWebGpu_RuntimeResources( s_module.borrow.runtime,
			&s_module.borrow.runtimeReceipt );
	if ( slot->resource && resources )
		(void)RalWebGpu_LightingDestroy( resources, slot->resource,
			&slot->receipt );
	memset( slot, 0, sizeof( *slot ) );
}

static qboolean UploadDirectionalLighting( int worldIndex ) {
	const renderDirectionalLightingRecord_t *record;
	ralWebGpuResourceLayer_t *resources;
	ralLightingRuntimePlan_t plan;
	ralStaticLightingCapabilities_t capabilities = { qtrue, qtrue, qtrue, qtrue };
	wiredWebGpuWorldLighting_t *slot;
	uint64_t digest;
	if ( worldIndex < 0 || worldIndex >= MAX_RENDER_WORLDS ) return qfalse;
	slot = &s_module.worldLighting[worldIndex];
	DestroyDirectionalLighting( worldIndex );
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
		record->artifactByteLength, &plan, &slot->resource, &slot->receipt );
}

static void DestroyOwners( void )
{
	for ( int i = 0; i < MAX_RENDER_WORLDS; ++i )
		DestroyDirectionalLighting( i );
	if ( s_module.worldMaterialBindGroup )
		RalWebGpu_BrowserBridgeDestroyBindGroup( s_module.borrow.bridge,
			s_module.worldMaterialBindGroup );
	s_module.worldMaterialBindGroup = 0u;
	if ( s_module.entityLightingBindGroup )
		RalWebGpu_BrowserBridgeDestroyBindGroup( s_module.borrow.bridge,
			s_module.entityLightingBindGroup );
	s_module.entityLightingBindGroup = 0u;
	for ( uint32_t i = 2u + RAL_WEBGPU_EFFECT_PIPELINE_COUNT; i > 0u; --i )
		if ( s_module.atmosphereBindGroups[i - 1u] )
			RalWebGpu_BrowserBridgeDestroyBindGroup( s_module.borrow.bridge,
				s_module.atmosphereBindGroups[i - 1u] );
	memset( s_module.atmosphereBindGroups, 0,
		sizeof( s_module.atmosphereBindGroups ) );
	s_module.atmosphereSkyBindingGeneration = 0u;
	memset( &s_module.skyRadianceConsumer, 0,
		sizeof( s_module.skyRadianceConsumer ) );
	if ( s_module.product )
		RalWebGpu_ProductDestroy( s_module.product, &s_module.borrow.runtimeReceipt );
	s_module.product = NULL;
	(void)RalWebGpu_RuntimeGetReceipt( s_module.borrow.runtime,
		&s_module.borrow.runtimeReceipt );
	if ( s_module.sky ) {
		RalWebGpu_SkyDestroy( s_module.sky, &s_module.borrow.runtimeReceipt );
		s_module.sky = NULL;
		(void)RalWebGpu_RuntimeGetReceipt( s_module.borrow.runtime,
			&s_module.borrow.runtimeReceipt );
	}
	if ( s_module.weather ) {
		RalWebGpu_WeatherDestroy( s_module.weather, &s_module.borrow.runtimeReceipt );
		s_module.weather = NULL;
		(void)RalWebGpu_RuntimeGetReceipt( s_module.borrow.runtime, &s_module.borrow.runtimeReceipt );
	}
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
	if ( !RalWebGpu_SkyCreate( s_module.borrow.runtime,
			&s_module.borrow.runtimeReceipt, s_module.borrow.bridge,
			NextGeneration(), &s_module.sky ) ) {
		DestroyOwners();
		return qfalse;
	}
	s_module.failureStage = 2201;
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
	s_module.failureStage = 223;
	if ( !RalWebGpu_ProductCreate( s_module.borrow.runtime, &s_module.borrow.runtimeReceipt,
								   &s_module.pipelineReceipts[WEBGPU_PIPELINE_WORLD], s_module.moduleGeneration,
								   &s_module.product ) ) {
		DestroyOwners();
		return qfalse;
	}
	s_module.failureStage = 222;
	if ( !CreateAtmosphereOwners() ) {
		DestroyOwners();
		return qfalse;
	}
	s_module.failureStage = 224;
    if (!RalWebGpu_ProductEnableExposure(s_module.product,
            &s_module.borrow.runtimeReceipt,s_module.borrow.bridge,NextGeneration())) {
        DestroyOwners(); return qfalse;
    }
    if (!RalWebGpu_ProductEnableSkyMaterial(s_module.product,
            &s_module.borrow.runtimeReceipt,s_module.borrow.bridge,NextGeneration())) {
        DestroyOwners(); return qfalse;
    }
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
	if ( !RalWebGpu_ProductSetEffectPipelines( s_module.product,
			&s_module.borrow.runtimeReceipt,
			&s_module.pipelineReceipts[WEBGPU_PIPELINE_EFFECT_LEGACY] ) ) {
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
											&s_module.atmosphereBindGroups[2] ) ) {
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

#if defined(__EMSCRIPTEN__)
static qboolean PublishDecodedTextureIdentity( qhandle_t handle,
        renderAssetKind_t kind, const char *resolved, qboolean noMipMaps ) {
	char canonical[MAX_QPATH];
	uint64_t sourceId = 0u, byteSize = 0u;
	unsigned filesystemGeneration = 0u;
	ralTextureCacheKey_t key;
	renderTextureSnapshot_t snapshot;
	if ( handle <= 0 || !resolved || !resolved[0]
			|| !RenderSubmission_TextureSnapshot( &s_module.frontend, handle,
				&snapshot ) || snapshot.ready != qtrue || !snapshot.rgba8
			|| !snapshot.width || !snapshot.height || !ri.FS_ResolveResource
			|| !ri.FS_ResolveResource( resolved, canonical, sizeof( canonical ),
				&sourceId, &byteSize, &filesystemGeneration )
			|| !sourceId || !byteSize || !filesystemGeneration ) return qfalse;
	if ( snapshot.textureCacheKeyReady )
		return Ral_TextureCacheKeyValid( &snapshot.textureCacheKey )
			&& snapshot.textureCacheKey.asset.width == snapshot.width
			&& snapshot.textureCacheKey.asset.height == snapshot.height;
	if ( !Ral_MaterialRuntimeTextureKeyBuild( sourceId, filesystemGeneration,
			snapshot.rgba8, snapshot.width, snapshot.height,
			kind == RENDER_ASSET_MSDF ? qfalse : qtrue, noMipMaps,
			RAL_TEXTURE_RESIDENCY_STREAMED, &key ) ) return qfalse;
	return RenderSubmission_SetTextureCacheKey( &s_module.frontend, handle,
		&key );
}
#endif

static qhandle_t RegisterScriptedTextureResource( renderAssetKind_t kind,
		const renderMaterialScriptEntry_t *scripted, const char *imageName,
		qboolean clampToEdge ) {
#if defined(__EMSCRIPTEN__)
	byte *pixels = NULL;
	uint32_t width = 0u, height = 0u;
	char resolved[MAX_QPATH];
	qhandle_t handle;
	if ( !scripted || !imageName || !imageName[0] || imageName[0] == '*'
			|| !RenderImage_DecodeRgba8( imageName, &pixels, &width, &height,
				resolved ) ) return 0;
	handle = RenderSubmission_RegisterStageTexture( &s_module.frontend, kind,
		imageName, clampToEdge, pixels, width, height );
	if ( handle && !PublishDecodedTextureIdentity( handle, kind, resolved,
			scripted->noMipMaps ) ) handle = 0;
	ri.Free( pixels );
	return handle;
#else
	(void)kind; (void)scripted; (void)imageName; (void)clampToEdge;
	return 0;
#endif
}

static qboolean RegisterSkyProgramResources( renderAssetKind_t kind,
		const renderMaterialScriptEntry_t *scripted,
		qhandle_t outerTextures[RAL_MATERIAL_SKY_FACE_COUNT],
		qhandle_t innerTextures[RAL_MATERIAL_SKY_FACE_COUNT] ) {
	static const char *suffixes[RAL_MATERIAL_SKY_FACE_COUNT] = {
		"rt", "bk", "lf", "ft", "up", "dn"
	};
	const char *prefixes[2];
	qhandle_t *textureSets[2] = { outerTextures, innerTextures };
	if ( !scripted || !scripted->sky || !outerTextures || !innerTextures )
		return qfalse;
	memset( outerTextures, 0,
		RAL_MATERIAL_SKY_FACE_COUNT * sizeof( outerTextures[0] ) );
	memset( innerTextures, 0,
		RAL_MATERIAL_SKY_FACE_COUNT * sizeof( innerTextures[0] ) );
	prefixes[0] = scripted->skyBoxPrefix;
	prefixes[1] = scripted->skyInnerBoxPrefix;
	for ( uint32_t box = 0u; box < 2u; ++box ) {
		if ( !prefixes[box][0] ) continue;
		for ( uint32_t face = 0u; face < RAL_MATERIAL_SKY_FACE_COUNT; ++face ) {
			char path[MAX_QPATH];
			const int length = snprintf( path, sizeof( path ), "%s_%s",
				prefixes[box], suffixes[face] );
			if ( length <= 0 || length >= (int)sizeof( path ) ) return qfalse;
			textureSets[box][face] = RegisterScriptedTextureResource( kind, scripted,
				path, qtrue );
			if ( textureSets[box][face] <= 0 ) return qfalse;
		}
	}
	for ( uint32_t stageIndex = 0u; stageIndex < scripted->stageCount;
			++stageIndex ) {
		const renderMaterialScriptStage_t *stage = &scripted->stages[stageIndex];
		if ( stage->imageSource != RENDER_MATERIAL_STAGE_IMAGE ) continue;
		if ( stage->imageName[0]
				&& !RegisterScriptedTextureResource( kind, scripted, stage->imageName,
					stage->clampToEdge ) ) return qfalse;
		for ( uint32_t frame = 0u; frame < stage->animationFrameCount; ++frame ) {
			const char *frameName = scripted->animationFrameNames[
				stage->animationFrameOffset + frame];
			if ( !RegisterScriptedTextureResource( kind, scripted, frameName,
					stage->clampToEdge ) ) return qfalse;
		}
	}
	return qtrue;
}

typedef struct {
	renderAssetKind_t kind;
	const renderMaterialScriptEntry_t *scripted;
	qhandle_t owner;
} scriptedStageContext_t;

static qhandle_t ResolveScriptedStage( void *context,
		const renderMaterialScriptStage_t *stage, const char *imageName ) {
	const scriptedStageContext_t *source = (const scriptedStageContext_t *)context;
	/* Reuse the already decoded primary only for the same sampler usage. */
	if ( !source->scripted->skyBoxPrefix[0]
			&& stage->clampToEdge == source->scripted->clampToEdge
			&& !strcasecmp( imageName, source->scripted->imageName ) )
		return source->owner;
	const qhandle_t handle = RegisterScriptedTextureResource( source->kind,
		source->scripted, imageName, stage->clampToEdge );
	if ( !handle && ri.Log ) ri.Log( SEV_WARN,
		"material '%s': unresolved stage image '%s'\n",
		source->scripted->name, imageName );
	return handle;
}

static qboolean ApplyScriptedMaterial( renderAssetKind_t kind,
		const renderMaterialScriptEntry_t *scripted, qhandle_t handle ) {
	renderMaterialStageSnapshot_t stages[RENDER_MATERIAL_MAX_STAGES];
	qhandle_t skyOuterTextures[RAL_MATERIAL_SKY_FACE_COUNT] = { 0 };
	qhandle_t skyInnerTextures[RAL_MATERIAL_SKY_FACE_COUNT] = { 0 };
	if ( !scripted || !scripted->name[0] ) return handle > 0;
	if ( !handle
			|| !RenderSubmission_SetMaterialRasterPolicy( &s_module.frontend,
				handle, scripted->alphaMode, scripted->alphaCutoff,
				scripted->depthWrite )
			|| !RenderSubmission_SetMaterialSort( &s_module.frontend, handle,
				scripted->sort )
			|| !RenderSubmission_SetMaterialCullMode( &s_module.frontend, handle,
				scripted->cullMode )
			|| !RenderSubmission_SetMaterialSky( &s_module.frontend, handle,
				scripted->sky )
			|| !RenderSubmission_SetMaterialSkyBox( &s_module.frontend, handle,
				scripted->skyBoxPrefix[0] ? qtrue : qfalse )
			|| !RenderSubmission_SetMaterialSkyCloudHeight( &s_module.frontend,
				handle, scripted->skyCloudHeight )
			|| !RenderSubmission_SetMaterialNoDraw( &s_module.frontend, handle,
				scripted->noDraw ) ) return qfalse;
	if ( ( scripted->sky || scripted->hasTcTransform )
			&& !RenderSubmission_SetMaterialSkyProjection( &s_module.frontend,
				handle, scripted->skyScale, scripted->skyScroll ) ) return qfalse;
	if ( scripted->stageCount ) {
		scriptedStageContext_t context = { kind, scripted, handle };
		if ( !RenderMaterialScript_BuildStages( scripted, ResolveScriptedStage,
				&context, stages )
				|| !RenderSubmission_SetMaterialStages( &s_module.frontend, handle,
					stages, scripted->stageCount ) ) return qfalse;
	}
	return !scripted->sky
		|| ( RegisterSkyProgramResources( kind, scripted,
			skyOuterTextures, skyInnerTextures )
			&& RenderSubmission_SetMaterialAuthoredSkySource(
				&s_module.frontend, handle, &scripted->skySource )
			&& RenderSubmission_SetMaterialAuthoredSkyTextures(
				&s_module.frontend, handle, skyOuterTextures,
				skyInnerTextures ) );
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
	/* Explicit built-in identity, not a missing-file replacement. Never send
	 * procedural names through file decoders (including the engine JPEG import). */
	if ( !strcmp( name, "*white" ) )
		return RenderSubmission_RegisterMaterialImage( &s_module.frontend,
			kind, name, clamp, s_white, 1u, 1u );
	memset( &scripted, 0, sizeof( scripted ) );
	if ( RenderMaterialScript_Lookup( &s_module.materialScripts, name, &scripted ) ) {
		clamp = scripted.clampToEdge;
		if ( scripted.imageName[0] ) imageName = scripted.imageName;
	}
#if defined(__EMSCRIPTEN__)
	if ( scripted.skyBoxPrefix[0] ) {
		if ( !wired_sky_atlas_decode( &ri, scripted.skyBoxPrefix, &pixels,
				&width, &height ) ) return 0;
		handle = RenderSubmission_RegisterMaterialImage( &s_module.frontend,
			kind, name, qtrue, pixels, width, height );
		ri.Free( pixels );
		if ( !handle ) return 0;
	} else if ( RenderImage_DecodeRgba8( imageName, &pixels, &width, &height, resolved ) ) {
		/* Register under the semantic material name the world/entity lookups
		 * use (BSP shader names carry no extension); the canonical decoded
		 * source path stays the texture identity published below. */
		handle = RenderSubmission_RegisterMaterialImage( &s_module.frontend,
			kind, name, clamp, pixels, width, height );
		if ( handle && !PublishDecodedTextureIdentity( handle, kind, resolved,
				scripted.name[0] ? scripted.noMipMaps : qfalse ) ) handle = 0;
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
	if ( handle && scripted.name[0]
			&& !ApplyScriptedMaterial( kind, &scripted, handle ) ) return 0;
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
	if ( name[0] == '*' ) return RenderSubmission_RegisterBspModel( &s_module.frontend, name );
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
	byte used[RENDER_SUBMISSION_MAX_MATERIALS];
	byte	*rgba = NULL;
	uint32_t side = 0u;
	uint64_t pixels;
	if ( !RenderWorldMaterialUsage_Build( bsp, used, sizeof( used ) ) || bsp->numLightmapPages < 0 || bsp->lightmapPageSize < 0 ||
		 ( bsp->numShaders && !bsp->shaders ) )
		return qfalse;
	for ( int i = 0; i < bsp->numShaders; ++i )
		if ( used[i] && !RegisterShader( bsp->shaders[i].shader ) )
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

#if defined( NDEBUG )
static qboolean PublishMaterialRuntimeWorld( const mapFile_t *bsp,
		int worldIndex )
{
	renderMaterialPackageDiagnostic_t diagnostic;
	uint64_t generation;
	if ( RenderMaterialRuntimeOwner_World( &s_module.materialRuntimeOwner,
			worldIndex ) ) return qtrue;
	if ( s_module.materialRuntimeOwner.ready != qtrue
			&& !RenderMaterialRuntimeOwner_Init(
				&s_module.materialRuntimeOwner, &s_module.imports ) ) return qfalse;
	if ( !RenderSubmission_WorldLifecycleGeneration( &s_module.frontend,
			worldIndex, &generation ) ) return qfalse;
	memset( &diagnostic, 0, sizeof( diagnostic ) );
	if ( !RenderMaterialRuntimeOwner_PublishWorld(
			&s_module.materialRuntimeOwner, bsp, worldIndex, generation,
			RalWebGpu_ProductResolveMaterialRuntimeResource,
			s_module.product, &diagnostic ) ) {
		if ( s_module.imports.LogCh && s_module.logChannel >= 0 )
			s_module.imports.LogCh( s_module.logChannel, SEV_WARN,
				"Wired WebGPU RAL: cooked material catalog rejected world=%d reason=%u item=%u runtime=%u key=%llu\n",
				worldIndex, (unsigned)diagnostic.reason,
				(unsigned)diagnostic.itemIndex,
				(unsigned)diagnostic.runtime.reason,
				(unsigned long long)diagnostic.runtime.cacheKeyHash );
		return qfalse;
	}
	if ( !RalWebGpu_ProductSetMaterialRuntimeCatalog( s_module.product,
			RenderMaterialRuntimeOwner_World(
				&s_module.materialRuntimeOwner, worldIndex ),
			&s_module.frontend ) ) return qfalse;
	return qtrue;
}
#endif

static qboolean PrepareMaterialRuntimeWorld( const mapFile_t *bsp,
		int worldIndex ) {
	/* The decoded material/stage textures are GPU resources every build needs
	 * for a textured world; only the cooked material runtime publication is
	 * Release-only. Debug browser builds used to skip both and drew the world
	 * untextured (flat grey, black sky). */
	if ( !RalWebGpu_ProductPrepareMaterialResources( s_module.product,
			&s_module.borrow.runtimeReceipt, &s_module.frontend ) ) {
		MarkFailed( "world-material-resources" ); return qfalse;
	}
#if defined( NDEBUG )
	return PublishMaterialRuntimeWorld( bsp, worldIndex );
#else
	(void)bsp; (void)worldIndex;
	return qtrue;
#endif
}

static qboolean ResetLensVisibility( void )
{
    uint64_t scope = NextGeneration();
    memset(s_module.lensVisibility, 0, sizeof(s_module.lensVisibility));
    memset(s_module.lensVisibleActive, 0, sizeof(s_module.lensVisibleActive));
    s_module.lensPublishedFrame = 0u;
    if (!scope || !s_module.borrow.bridge ||
        !RalWebGpu_BrowserBridgeResetLensVisibility(s_module.borrow.bridge, scope)) return qfalse;
    memset(s_module.lensQueries, 0, sizeof(s_module.lensQueries));
    s_module.lensScope = scope;
    return qtrue;
}

static qboolean CollectLensVisibility( void )
{
    float values[LENS_MAX_CGSOURCES];
    for (uint32_t i=0; i<3u; ++i) {
        if (!s_module.lensQueries[i].query) continue;
        ralWebGpuAsyncStatus_t status = RalWebGpu_BrowserBridgePollLensVisibility(
            s_module.borrow.bridge, s_module.lensScope, s_module.lensQueries[i].frame,
            s_module.lensQueries[i].query, values);
        if (status == RAL_WEBGPU_ASYNC_PENDING) continue;
        RalWebGpu_BrowserBridgeReleaseLensVisibility(s_module.borrow.bridge, s_module.lensQueries[i].query);
        s_module.lensQueries[i].query = 0u;
        if (status != RAL_WEBGPU_ASYNC_READY) return qfalse;
        if (s_module.lensQueries[i].frame > s_module.lensPublishedFrame) {
            memcpy(s_module.lensVisibility, values, sizeof(values));
            s_module.lensPublishedFrame = s_module.lensQueries[i].frame;
        }
    }
    return qtrue;
}

/* Called only after the product's scene-depth submission, on both the direct
 * and asynchronous sky/weather paths. Readback never delays presentation. */
static qboolean SubmitLensVisibility(uint64_t frame, float optics)
{
    renderLensSourceSnapshot_t snapshot;
    const lensSourceDesc_t *sources;
    renderWorldSnapshot_t world;
    ralWebGpuBrowserLensSource_t packed[LENS_MAX_CGSOURCES];
    float params[24] = {0};
    uintptr_t query = 0u;
    uint32_t slot = 3u;
    if ((!s_module.lensScope && !ResetLensVisibility()) || !CollectLensVisibility() ||
        !RenderSubmission_LensSourceSnapshot(&s_module.frontend, &snapshot, &sources)) return qfalse;
    memset(s_module.lensVisibleActive, 0, sizeof(s_module.lensVisibleActive));
    for (uint32_t i=0; i<snapshot.count; ++i) s_module.lensVisibleActive[sources[i].id] = qtrue;
    if (!snapshot.count) return qtrue;
    for (uint32_t i=0; i<3u; ++i) if (!s_module.lensQueries[i].query) {slot=i;break;}
    if (slot==3u) return qtrue; /* bounded backpressure, no replacement oracle */
    if (!RenderSubmission_WorldSnapshot(&s_module.frontend, &world) ||
        !RalWebGpu_ProductProjection(&world, &params[16])) return qfalse;
    params[16] = 1.0f / params[16]; params[17] = 1.0f / params[17];
    memcpy(params, world.viewOrigin, 3u*sizeof(float));
    for (uint32_t i=0; i<3u; ++i) memcpy(&params[4u+i*4u], world.viewAxis[i], 3u*sizeof(float));
    params[20] = (float)s_module.config.vidWidth;
    params[21] = (float)s_module.config.vidHeight;
    params[22] = optics;
    memset(packed, 0, sizeof(packed));
    for (uint32_t i=0; i<snapshot.count; ++i) {
        memcpy(packed[i].originRadius, sources[i].origin, 3u*sizeof(float));
        packed[i].originRadius[3] = sources[i].radius;
        packed[i].identity[0] = (uint32_t)sources[i].id;
    }
    if (!RalWebGpu_BrowserBridgeBeginLensVisibility(s_module.borrow.bridge,
        s_module.lensScope, frame, params, packed, snapshot.count, &query)) return qfalse;
    if (query) {s_module.lensQueries[slot].query=query;s_module.lensQueries[slot].frame=frame;}
    return qtrue;
}

static void SubmitLensSource(const lensSourceDesc_t *source)
{
    if (s_module.lens && !s_module.lens->integer) return;
    if (s_module.frameOpen && !RenderSubmission_AddLensSource(&s_module.frontend, source))
        MarkFailed("lens-source");
}

static qboolean GetLensVisibility(int id, float *value)
{
    /* Fail closed with r_lens 0 (Vulkan RE_GetLensVisibility contract). */
    if (s_module.lens && !s_module.lens->integer) { if (value) *value=0.0f; return qfalse; }
    if (!value || id<0 || id>=LENS_MAX_CGSOURCES || !s_module.loaded || !s_module.lensScope) return qfalse;
    if (!CollectLensVisibility()) {MarkFailed("lens-result");return qfalse;}
    if (!s_module.lensPublishedFrame || !s_module.lensVisibleActive[id]) return qfalse;
    *value=s_module.lensVisibility[id];return qtrue;
}

static void LoadWorld( const mapFile_t *bsp, int worldIndex )
{
	if ( !ResetLensVisibility() ) MarkFailed( "world-lens-reset" );
	else if ( !PrepareWorldMaterials( bsp ) ) MarkFailed( "world-materials" );
	else if ( !RenderSubmission_LoadWorld( &s_module.frontend, bsp, worldIndex ) )
		MarkFailed( "world-geometry" );
	else if ( !PrepareMaterialRuntimeWorld( bsp, worldIndex ) )
		MarkFailed( "world-material-catalog" );
	else if ( !RenderLightingSidecar_LoadDirectional( &s_module.frontend,
			&s_module.imports, bsp->name, &s_module.lightingSidecar ) )
		MarkFailed( "world-directional-sidecar" );
	else if ( !RenderLightingSidecar_LoadIrradiance( &s_module.frontend,
			&s_module.imports, bsp->name, &s_module.irradianceSidecar ) )
		MarkFailed( "world-irradiance-sidecar" );
	else if ( !UploadDirectionalLighting( worldIndex ) )
		MarkFailed( "world-directional-upload" );
	else {
		s_module.loadedWorlds[worldIndex] = bsp;
		s_module.activeWorldIndex = worldIndex;
		s_module.loadedWorld = bsp;
	}
}

static qboolean SelectWorld( int worldIndex )
{
    if (worldIndex != s_module.activeWorldIndex && !ResetLensVisibility()) return qfalse;
	if ( worldIndex < 0 || worldIndex >= MAX_RENDER_WORLDS
			|| !s_module.loadedWorlds[worldIndex]
			|| !RenderSubmission_SelectWorld( &s_module.frontend, worldIndex ) ) return qfalse;
#if defined( NDEBUG )
	if ( !RalWebGpu_ProductSetMaterialRuntimeCatalog( s_module.product,
			RenderMaterialRuntimeOwner_World(
				&s_module.materialRuntimeOwner, worldIndex ),
			&s_module.frontend ) ) return qfalse;
#endif
	s_module.activeWorldIndex = worldIndex;
	s_module.loadedWorld = s_module.loadedWorlds[worldIndex];
	return qtrue;
}

static qboolean UnloadWorld( int worldIndex )
{
    if (worldIndex == s_module.activeWorldIndex && !ResetLensVisibility()) return qfalse;
	if ( worldIndex < 0 || worldIndex >= MAX_RENDER_WORLDS
			|| ( s_module.activeWorldIndex == worldIndex && s_module.product
				&& !RalWebGpu_ProductSetMaterialRuntimeCatalog(
					s_module.product, NULL, &s_module.frontend ) )
			|| ( s_module.activeWorldIndex == worldIndex && s_module.product
				&& !RalWebGpu_ProductUnloadSkyRadianceResources(
					s_module.product,
					&s_module.borrow.runtimeReceipt ) )
			|| ( s_module.activeWorldIndex == worldIndex
				&& !RefreshWorldAtmosphereOwner( qtrue ) )
			|| ( s_module.activeWorldIndex == worldIndex && s_module.sky
				&& !RalWebGpu_SkyUnloadWorld( s_module.sky,
					&s_module.borrow.runtimeReceipt ) )
			|| ( s_module.product && !RalWebGpu_ProductUnloadWorld(
				s_module.product, &s_module.borrow.runtimeReceipt, worldIndex ) ) ) return qfalse;
	DestroyDirectionalLighting( worldIndex );
	if ( !RenderSubmission_UnloadWorld( &s_module.frontend, worldIndex ) ) return qfalse;
	RenderMaterialRuntimeOwner_UnloadWorld(
		&s_module.materialRuntimeOwner, worldIndex );
	s_module.loadedWorlds[worldIndex] = NULL;
	if ( s_module.activeWorldIndex == worldIndex ) {
		if ( !Ral_SkyEnvironmentRuntimeInit( RAL_BACKEND_WEBGPU,
				&s_module.skyRuntime ) ) return qfalse;
		s_module.skyWorldLifecycleGeneration = 0u;
		s_module.completedSkyWorkMask = 0u;
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

static qboolean WorldContentIdentity( int worldIndex, uint64_t *outIdentity )
{
	return RenderSubmission_WorldContentIdentity( &s_module.frontend,
		worldIndex, outIdentity );
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
	if ( !s_module.pbr )
		s_module.pbr = wired_render_pbr_register( &ri, &s_module.pbrDefault );
	if ( !s_module.pbr ) { MarkFailed( "pbr-cvar" ); return; }
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
	if ( !s_module.aerialHaze ) {
		if ( !ri.Cvar_Get || !ri.Cvar_CheckRange ) {
			memset( &s_module.aerialHazeFallback, 0, sizeof( s_module.aerialHazeFallback ) );
			s_module.aerialHazeFallback.value = 1.0f;
			s_module.aerialHazeFallback.integer = 1;
			s_module.aerialHaze = &s_module.aerialHazeFallback;
		} else {
			s_module.aerialHaze = ri.Cvar_Get( "r_atmosphereAerialHaze", "1",
				CVAR_ARCHIVE | CVAR_NODEFAULT );
			if ( !s_module.aerialHaze ) {
				MarkFailed( "aerial-haze-cvar" ); return;
			}
			ri.Cvar_CheckRange( s_module.aerialHaze, "0", "1", CV_FLOAT );
			if ( ri.Cvar_SetDescription ) ri.Cvar_SetDescription( s_module.aerialHaze,
				"Aerial-perspective tint of analytic weather media by sky radiance (0 = flat media colour)." );
			if ( ri.Cvar_SetGroup ) ri.Cvar_SetGroup( s_module.aerialHaze, CVG_RENDERER );
		}
	}
	if ( !s_module.lightmapBoost ) {
		if ( !ri.Cvar_Get || !ri.Cvar_CheckRange ) {
			memset( &s_module.lightmapBoostFallback, 0, sizeof( s_module.lightmapBoostFallback ) );
			s_module.lightmapBoostFallback.value = 4.6f;
			s_module.lightmapBoostFallback.integer = 4;
			s_module.lightmapBoost = &s_module.lightmapBoostFallback;
		} else {
			s_module.lightmapBoost = ri.Cvar_Get( "r_lightmapBoost", "4.6",
				CVAR_ARCHIVE | CVAR_NODEFAULT );
			if ( !s_module.lightmapBoost ) {
				MarkFailed( "lightmap-boost-cvar" ); return;
			}
			ri.Cvar_CheckRange( s_module.lightmapBoost, "1", "24", CV_FLOAT );
			if ( ri.Cvar_SetDescription ) ri.Cvar_SetDescription( s_module.lightmapBoost,
				"World-lightmap overbright (linear-domain equivalent of vanilla x2; shared with the native backends)." );
			if ( ri.Cvar_SetGroup ) ri.Cvar_SetGroup( s_module.lightmapBoost, CVG_RENDERER );
		}
	}
	/* r_lens gates the lens occlusion oracle exactly like Vulkan (default off);
	 * a host without cvars (contract tests) keeps the oracle enabled. */
	if ( !s_module.lens ) {
		if ( !ri.Cvar_Get || !ri.Cvar_CheckRange ) {
			memset( &s_module.lensFallback, 0, sizeof( s_module.lensFallback ) );
			s_module.lensFallback.value = 1.0f; s_module.lensFallback.integer = 1;
			s_module.lens = &s_module.lensFallback;
		} else {
			s_module.lens = ri.Cvar_Get( "r_lens", "1", CVAR_ARCHIVE | CVAR_NODEFAULT );
			if ( !s_module.lens ) { MarkFailed( "lens-cvar" ); return; }
			ri.Cvar_CheckRange( s_module.lens, "0", "1", CV_INTEGER );
			if ( ri.Cvar_SetDescription ) ri.Cvar_SetDescription( s_module.lens, "Depth-sampled lens occlusion oracle for flares (replaces the per-flare dot-probe with a smooth N-tap visibility). Requires r_fbo 1 + r_flares 1." );
			if ( ri.Cvar_SetGroup ) ri.Cvar_SetGroup( s_module.lens, CVG_RENDERER );
		}
	}
	if ( !s_module.skyPhysicalBlend ) {
		if ( !ri.Cvar_Get || !ri.Cvar_CheckRange ) {
			memset( &s_module.skyPhysicalBlendFallback, 0, sizeof( s_module.skyPhysicalBlendFallback ) );
			s_module.skyPhysicalBlendFallback.value = 0.6f;
			s_module.skyPhysicalBlendFallback.integer = 0;
			s_module.skyPhysicalBlend = &s_module.skyPhysicalBlendFallback;
		} else {
			s_module.skyPhysicalBlend = ri.Cvar_Get( "r_skyPhysicalBlend", "0.6",
				CVAR_ARCHIVE | CVAR_NODEFAULT );
			if ( !s_module.skyPhysicalBlend ) {
				MarkFailed( "sky-physical-blend-cvar" ); return;
			}
			ri.Cvar_CheckRange( s_module.skyPhysicalBlend, "0", "1", CV_FLOAT );
			if ( ri.Cvar_SetDescription ) ri.Cvar_SetDescription( s_module.skyPhysicalBlend,
				"0..1: how strongly the physical sky relights the authored sky in physical mode (1 = full relight, 0 = authored)." );
			if ( ri.Cvar_SetGroup ) ri.Cvar_SetGroup( s_module.skyPhysicalBlend, CVG_RENDERER );
		}
	}
	if ( s_module.resetRegistrationAssets ) {
		if ( !RalWebGpu_ProductResetRegistrationAssets( s_module.product,
				&s_module.borrow.runtimeReceipt, &s_module.frontend ) ) {
			MarkFailed( "registration-assets-reset" ); return;
		}
		s_module.defaultMaterial = 0;
		s_module.resetRegistrationAssets = qfalse;
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
	if ( generation && s_module.registered && !s_module.frameOpen
			&& s_module.pendingFrameReady && !s_module.frameSkipped ) {
		// The browser host consumes exactly one submitted frame per tick. A
		// second frame inside the same tick (loading-screen SCR_UpdateScreen
		// during map load) cannot be presented; drop it instead of failing the
		// module closed, keeping the single in-flight frame contract intact.
		s_module.frameSkipped = qtrue;
		return;
	}
	if ( !generation || !s_module.registered || s_module.frameOpen || s_module.pendingFrameReady ||
		 !RenderSubmission_BeginFrame( &s_module.frontend, generation ) ) {
		MarkFailed( "frame-begin" );
		return;
	}
	s_module.currentFrameGeneration = generation;
	s_module.frameOpen				= qtrue;
	s_module.frontend.pbrEnabled = s_module.pbr && s_module.pbr->integer != 0 ? qtrue : qfalse;
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
	if ( !s_module.lightmapBoost || !RalWebGpu_ProductSetLightmapBoost( s_module.product,
			s_module.lightmapBoost->value ) ) {
		s_module.failureStage = 49;
		return qfalse;
	}
	if ( !RalWebGpu_ProductSetWeather( s_module.product, &s_module.borrow.runtimeReceipt, &s_module.pending.weather,
									   weatherExecution ) ) {
		s_module.failureStage = 48;
		return qfalse;
	}
	if ( !RalWebGpu_ProductRenderPlan( s_module.product, &s_module.borrow.runtimeReceipt, &s_module.frontend,
									   &s_module.pending.frontend, &s_module.pendingProductPlan,
									   &s_module.pending.atmosphere.skyEnvironment,
									   s_module.pendingFrame.textureIdentity, s_module.pending.frameGeneration,
									   &s_module.pending.product ) ) {
		s_module.failureStage = 700 + (int)RalWebGpu_ProductLastFailureStage();
		return qfalse;
	}
	s_module.productSubmitted = qtrue;
    if (!SubmitLensVisibility(s_module.pending.frameGeneration,
        s_module.pending.atmosphere.skyEnvironment.opticsVisibility)) {
        s_module.failureStage=49;return qfalse;
    }
	return qtrue;
}

static qboolean BeginSkyRadiance(
		ralWebGpuRendererModuleFrameReceipt_t *pending )
{
	renderAtmosphereSnapshot_t snapshot;
	ralSkyTierBudgetReceipt_t budget;
	const atmosphereEmitter_t *emitters;
	qboolean result;
	if ( !pending || !RenderSubmission_AtmosphereSnapshot( &s_module.frontend,
			&snapshot, &emitters ) ) return qfalse;
	(void)emitters;
	result = RalWebGpu_SkyRadianceBegin( s_module.sky,
		&s_module.borrow.runtimeReceipt, s_module.product,
		&s_module.frontend, &pending->atmosphere.skyEnvironment,
		pending->atmosphere.selectedTier, s_module.pendingSkyEventMask,
		snapshot.state.timelineSeconds, &pending->skyRadianceExecution );
	if ( !result ) return qfalse;
	/* Executor receipts describe readable residency, not this frame's
	 * lighting admission. Preserve them for asynchronous ownership checks. */
	if ( !Ral_SkyTierBudget( pending->atmosphere.selectedTier, &budget )
			|| !Ral_SkyRadianceConsumerAdmit( &budget,
				&pending->skyRadianceExecution.consumer,
				&s_module.skyRadianceConsumer ) ) return qfalse;
	return RefreshWorldAtmosphereOwner( qfalse )
		&& UpdateAtmosphereUniform( &pending->atmosphere,
			&pending->displayVisibility );
}

static qboolean UpdateExposure(const ralDisplayVisibilityPlan_t *visibility) {
    static const struct {const char *name,*value,*low,*high;} controls[]={
        {"r_hdrAutoExposure","1","0","1"},
        {"r_hdrExposureKey","0.0123","0.01","1"},
        {"r_hdrExposurePctLow","0.5","0","1"},
        {"r_hdrExposurePctHigh","0.2","0","1"},
        {"r_hdrAdaptionRateUp","1.5","0","100"},
        {"r_hdrAdaptionRateDown","0.5","0","100"},
        {"r_hdrExposureMin","0.25","0.01","1"},
        {"r_hdrExposureMax","8","1","64"},
        {"r_tonemap","3","0","4"},
        {"r_tonemapExposure","1","0.1","8"}};
    for(unsigned i=0;i<ARRAY_LEN(controls);++i) if(!s_module.exposureControls[i]) {
        cvar_t *v;
        if(!ri.Cvar_Get||!ri.Cvar_CheckRange) {
            v=&s_module.exposureDefaults[i];
            v->value=(float)atof(controls[i].value);v->integer=(int)v->value;
        } else {
            v=ri.Cvar_Get(controls[i].name,controls[i].value,CVAR_ARCHIVE|CVAR_NODEFAULT);
            if(!v) return qfalse;
            ri.Cvar_CheckRange(v,controls[i].low,controls[i].high,(i==0||i==8)?CV_INTEGER:CV_FLOAT);
            if(ri.Cvar_SetGroup) ri.Cvar_SetGroup(v,CVG_NONE);
        }
        s_module.exposureControls[i]=v;
    }
    const ralExposureSettings_t settings={s_module.exposureControls[0]->integer,
        s_module.exposureControls[1]->value,s_module.exposureControls[2]->value,
        s_module.exposureControls[3]->value,s_module.exposureControls[4]->value,
        s_module.exposureControls[5]->value,s_module.exposureControls[6]->value,
        s_module.exposureControls[7]->value};
    return RalWebGpu_ProductSetExposure(s_module.product,&settings,visibility,
        s_module.exposureControls[8]->integer,s_module.exposureControls[9]->value);
}
static void EndFrame( int *frontEndMsec, int *backEndMsec )
{
	ralWebGpuRendererModuleFrameReceipt_t pending;
	ralWebGpuFrontendPlanReceipt_t		  plan;
	ralDisplayVisibilityPlan_t visibility;
	ralSkyEnvironmentRuntimeState_t skyRuntimeCandidate;
	const uint32_t physicalSkyMask = RAL_SKY_DIRTY_TRANSMITTANCE
		| RAL_SKY_DIRTY_MULTISCATTER | RAL_SKY_DIRTY_SKY_VIEW;
	uint32_t admittedPhysicalSky;
	if ( frontEndMsec )
		*frontEndMsec = 0;
	if ( backEndMsec )
		*backEndMsec = 0;
	if ( s_module.frameSkipped && !s_module.frameOpen ) {
		s_module.frameSkipped = qfalse;
		return;
	}
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
    if (!UpdateExposure(&visibility)) {
        s_module.failureStage=50; MarkFailed("exposure-settings"); return;
    }
	if ( !RalWebGpu_WeatherPlan( &s_module.frontend, &pending.atmosphere, &pending.weather ) ) {
		s_module.failureStage = 47;
		MarkFailed( "weather-plan" );
		return;
	}
	if ( pending.frontend.sceneRendered && !UpdateAtmosphereUniform( &pending.atmosphere,
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
	admittedPhysicalSky = pending.atmosphere.skyEnvironment.admittedWorkMask
		& physicalSkyMask;
	skyRuntimeCandidate = s_module.skyRuntime;
	if ( pending.frontend.sceneRendered && !Ral_SkyEnvironmentRuntimeSchedule( &skyRuntimeCandidate,
			&pending.atmosphere.skyEnvironment ) ) {
		s_module.failureStage = 452;
		MarkFailed( "sky-schedule" );
		return;
	}
	if ( admittedPhysicalSky ) {
		renderAtmosphereSnapshot_t snapshot;
		const atmosphereEmitter_t *emitters;
		if ( !RenderSubmission_AtmosphereSnapshot( &s_module.frontend,
				&snapshot, &emitters )
				|| !RalWebGpu_SkyBegin( s_module.sky,
					&s_module.borrow.runtimeReceipt, &snapshot,
					&pending.atmosphere.skyEnvironment,
					&pending.skyExecution ) ) {
			(void)emitters;
			s_module.failureStage = 451;
			MarkFailed( "sky-compute" );
			return;
		}
		(void)emitters;
		s_module.skyRuntime = skyRuntimeCandidate;
		s_module.skyComputePending = qtrue;
		s_module.productSubmitted = qfalse;
	} else {
		if ( pending.frontend.sceneRendered ) s_module.skyRuntime = skyRuntimeCandidate;
		if ( pending.frontend.sceneRendered && !BeginSkyRadiance( &pending ) ) {
			s_module.failureStage = 453;
			MarkFailed( "sky-radiance-compute" );
			return;
		}
		if ( pending.skyRadianceExecution.sliceMask ) {
			s_module.skyRadianceComputePending = qtrue;
			s_module.productSubmitted = qfalse;
		} else if ( pending.atmosphere.computeDispatchCount != 0u ) {
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
											   &pending.frontend, &plan,
											   &pending.atmosphere.skyEnvironment,
											   s_module.pendingFrame.textureIdentity,
											   pending.frameGeneration, &pending.product ) ) {
				s_module.failureStage = 700 + (int)RalWebGpu_ProductLastFailureStage();
				MarkFailed( "product-frame" );
				return;
			}
			s_module.productSubmitted = qtrue;
            if (!SubmitLensVisibility(pending.frameGeneration, pending.atmosphere.skyEnvironment.opticsVisibility)) {
                s_module.failureStage=49;MarkFailed("lens-submit");return;
            }
		}
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
	if ( s_module.skyComputePending ) {
		uint32_t completedWorkMask = 0u;
		status = RalWebGpu_SkyPoll( s_module.sky,
			&s_module.pending.skyExecution, &completedWorkMask );
		if ( status != RAL_WEBGPU_ASYNC_READY ) {
			if ( status == RAL_WEBGPU_ASYNC_FAILED ) {
				s_module.failureStage = 4511;
				MarkFailed( "sky-compute-poll" );
			}
			return status;
		}
		s_module.completedSkyWorkMask = completedWorkMask;
		s_module.skyComputePending = qfalse;
		if ( !BeginSkyRadiance( &s_module.pending ) ) {
			s_module.failureStage = 453;
			MarkFailed( "sky-radiance-compute" );
			return RAL_WEBGPU_ASYNC_FAILED;
		}
		if ( s_module.pending.skyRadianceExecution.sliceMask ) {
			s_module.skyRadianceComputePending = qtrue;
		} else if ( s_module.pending.atmosphere.computeDispatchCount != 0u ) {
			if ( !RalWebGpu_AtmosphereBegin( s_module.atmosphere,
					&s_module.borrow.runtimeReceipt, &s_module.frontend,
					&s_module.pending.atmosphere,
					&s_module.pending.atmosphereExecution ) ) {
				s_module.failureStage = 46;
				MarkFailed( "atmosphere-compute" );
				return RAL_WEBGPU_ASYNC_FAILED;
			}
			s_module.atmosphereComputePending = qtrue;
		} else if ( !s_module.pending.weather.zeroWork ) {
			if ( !RalWebGpu_WeatherBegin( s_module.weather,
					&s_module.borrow.runtimeReceipt, &s_module.frontend,
					&s_module.pending.weather,
					&s_module.pending.displayVisibility,
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
	if ( s_module.skyRadianceComputePending ) {
		uint32_t completedSliceMask = 0u;
		uint64_t completedRevision = 0u;
		status = RalWebGpu_SkyRadiancePoll( s_module.sky,
			&s_module.borrow.runtimeReceipt,
			&s_module.pending.skyRadianceExecution,
			&completedSliceMask, &completedRevision );
		if ( status != RAL_WEBGPU_ASYNC_READY ) {
			if ( status == RAL_WEBGPU_ASYNC_FAILED ) {
				s_module.failureStage = 4531;
				MarkFailed( "sky-radiance-compute-poll" );
			}
			return status;
		}
		(void)completedSliceMask;
		(void)completedRevision;
		s_module.skyRadianceComputePending = qfalse;
		if ( s_module.pending.atmosphere.computeDispatchCount != 0u ) {
			if ( !RalWebGpu_AtmosphereBegin( s_module.atmosphere,
					&s_module.borrow.runtimeReceipt, &s_module.frontend,
					&s_module.pending.atmosphere,
					&s_module.pending.atmosphereExecution ) ) {
				s_module.failureStage = 46;
				MarkFailed( "atmosphere-compute" );
				return RAL_WEBGPU_ASYNC_FAILED;
			}
			s_module.atmosphereComputePending = qtrue;
		} else if ( !s_module.pending.weather.zeroWork ) {
			if ( !RalWebGpu_WeatherBegin( s_module.weather,
					&s_module.borrow.runtimeReceipt, &s_module.frontend,
					&s_module.pending.weather,
					&s_module.pending.displayVisibility,
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
	memset( &s_module.pending.skyGpuTelemetry, 0,
		sizeof( s_module.pending.skyGpuTelemetry ) );
	if ( s_module.pending.frontend.sceneRendered ) {
	s_module.pending.skyGpuTelemetry.schemaVersion =
		RAL_SKY_GPU_TELEMETRY_SCHEMA_VERSION;
	s_module.pending.skyGpuTelemetry.backendType = RAL_BACKEND_WEBGPU;
	s_module.pending.skyGpuTelemetry.frameGeneration =
		s_module.pending.frameGeneration;
	s_module.pending.skyGpuTelemetry.sourceRevision =
		s_module.pending.atmosphere.skyEnvironment.sourceRevision;
	s_module.pending.skyGpuTelemetry.selectedTier =
		s_module.pending.atmosphere.selectedTier;
	s_module.pending.skyGpuTelemetry.selectedMode =
		s_module.pending.atmosphere.skyEnvironment.selectedMode;
	s_module.pending.skyGpuTelemetry.submittedWorkMask =
		s_module.pending.atmosphere.skyEnvironment.admittedWorkMask;
	s_module.pending.skyGpuTelemetry.completedWorkMask =
		s_module.pending.skyExecution.ready
			? s_module.pending.skyExecution.workMask : 0u;
	s_module.pending.skyGpuTelemetry.deferredWorkMask =
		s_module.pending.atmosphere.skyEnvironment.deferredWorkMask;
	s_module.pending.skyGpuTelemetry.cancelledWorkMask =
		s_module.pending.atmosphere.skyEnvironment.cancelledWorkMask;
	s_module.pending.skyGpuTelemetry.submittedSliceMask =
		s_module.pending.skyRadianceExecution.sliceMask;
	s_module.pending.skyGpuTelemetry.completedSliceMask =
		s_module.pending.skyRadianceExecution.sliceMask;
	s_module.pending.skyGpuTelemetry.deferredSliceCount =
		s_module.pending.skyRadianceExecution.plan.pendingSliceCount;
	s_module.pending.skyGpuTelemetry.cancelledSliceCount =
		s_module.pending.skyRadianceExecution.plan.cancelledSliceCount;
	s_module.pending.skyGpuTelemetry.currentRadianceRevision =
		s_module.pending.skyRadianceExecution.plan.currentProductRevision;
	s_module.pending.skyGpuTelemetry.targetRadianceRevision =
		s_module.pending.skyRadianceExecution.plan.targetProductRevision;
	s_module.pending.skyGpuTelemetry.physicalTimingState =
		RAL_SKY_GPU_TIMING_UNAVAILABLE;
	s_module.pending.skyGpuTelemetry.radianceTimingState =
		RAL_SKY_GPU_TIMING_UNAVAILABLE;
	if ( s_module.pending.skyGpuTelemetry.completedWorkMask ) {
		ralWebGpuGpuTimingState_t state;
		uint64_t duration;
		if ( !RalWebGpu_SkyLastPhysicalGpuTiming( s_module.sky, &state,
				&duration ) ) {
			MarkFailed( "physical-sky-gpu-timing" );
			return RAL_WEBGPU_ASYNC_FAILED;
		}
		s_module.pending.skyGpuTelemetry.physicalTimingState =
			SkyTimingState( state );
		s_module.pending.skyGpuTelemetry.physicalTimingFrameGeneration =
			state == RAL_WEBGPU_GPU_TIMING_UNAVAILABLE ? 0u
				: s_module.pending.skyExecution.frameGeneration;
		s_module.pending.skyGpuTelemetry.physicalGpuDurationNanoseconds = duration;
	}
	if ( s_module.pending.skyGpuTelemetry.completedSliceMask ) {
		ralWebGpuGpuTimingState_t state;
		uint64_t duration;
		if ( !RalWebGpu_SkyLastRadianceGpuTiming( s_module.sky, &state,
				&duration ) ) {
			MarkFailed( "radiance-sky-gpu-timing" );
			return RAL_WEBGPU_ASYNC_FAILED;
		}
		s_module.pending.skyGpuTelemetry.radianceTimingState =
			SkyTimingState( state );
		s_module.pending.skyGpuTelemetry.radianceTimingFrameGeneration =
			state == RAL_WEBGPU_GPU_TIMING_UNAVAILABLE ? 0u
				: s_module.pending.skyRadianceExecution.frameGeneration;
		s_module.pending.skyGpuTelemetry.radianceGpuDurationNanoseconds = duration;
	}
	s_module.pending.skyGpuTelemetry.ready = qtrue;
	}
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
    if (!ResetLensVisibility()) MarkFailed("lens-shutdown");
	if ( s_module.frameOpen )
		RenderSubmission_CancelFrame( &s_module.frontend );
	s_module.frameOpen				= qfalse;
	s_module.currentFrameGeneration = 0u;
	s_module.loadedWorld			= NULL;
	for ( int i = 0; i < MAX_RENDER_WORLDS; ++i )
		DestroyDirectionalLighting( i );
	if ( s_module.product ) (void)RalWebGpu_ProductSetMaterialRuntimeCatalog(
		s_module.product, NULL, &s_module.frontend );
	RenderMaterialRuntimeOwner_Destroy( &s_module.materialRuntimeOwner );
	if ( code == REF_LEVEL_ONLY ) {
		/* Retain the old assets for loading UI, retire at the next registration. */
		s_module.resetRegistrationAssets = qtrue;
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
static void SubmitEffectSprite( const spriteDesc_t *sprite )
{
	if ( s_module.frameOpen && sprite
			&& !RenderSubmission_AddEffectSprite( &s_module.frontend, sprite ) )
		MarkFailed( "effect-sprite" );
}
static void SubmitEffectRibbon( const ribbonDesc_t *ribbon )
{
	if ( s_module.frameOpen && ribbon
			&& !RenderSubmission_AddEffectRibbon( &s_module.frontend, ribbon ) )
		MarkFailed( "effect-ribbon" );
}
static void SubmitEffectBeam( const beamDesc_t *beam )
{
	if ( s_module.frameOpen && beam
			&& !RenderSubmission_AddEffectBeam( &s_module.frontend, beam ) )
		MarkFailed( "effect-beam" );
}
static void SubmitEffectDecal( const decalDesc_t *decal )
{
	if ( s_module.frameOpen && decal
			&& !RenderSubmission_AddEffectDecal( &s_module.frontend, decal ) )
		MarkFailed( "effect-decal" );
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
static void NoRail( const railRibbonDesc_t *v )
{
	(void)v;
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
static void SetHeightgrid( const float *v, int c )
{
	if ( !RenderSubmission_SetAtmosphereHeightgrid( &s_module.frontend, v, c ) )
		MarkFailed( "atmosphere-heightgrid" );
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
static void ModelBounds( qhandle_t m, vec3_t a, vec3_t b )
{
	RenderSubmission_ModelBounds( &s_module.frontend, m, a, b );
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
         !ResetLensVisibility() ||
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
	e->WorldContentIdentity         = WorldContentIdentity;
	e->SetWorldVisData				   = NoBytes;
	e->EndRegistration				   = EndRegistration;
	e->ClearScene					   = ClearScene;
	e->AddRefEntityToScene			   = AddEntity;
	e->AddPolyToScene				   = AddPoly;
	e->LightForPoint				   = NoLight;
	e->AddLightToScene				   = AddLight;
	e->AddAdditiveLightToScene		   = AddLight;
	e->AddLinearLightToScene		   = AddLinearLight;
	/* Submission without product lowering is not support: it would make
	 * WiredFX report EXECUTED and then silently lose the primitive. */
	e->AddRibbonToScene				   = SubmitEffectRibbon;
	e->AddRailRibbonToScene			   = NoRail;
	e->AddBeamToScene				   = SubmitEffectBeam;
	e->AddSpriteToScene				   = SubmitEffectSprite;
	e->EmitParticles				   = NULL;
	e->AddDecalToScene				   = SubmitEffectDecal;
	e->RegisterParticleClass		   = RegisterParticleClass;
	e->SetAtmosphere				   = SetAtmosphere;
	e->SetAtmosphereHeightgrid		   = SetHeightgrid;
	e->AddLensSourceToScene			   = SubmitLensSource;
	e->GetLensVisibility			   = GetLensVisibility;
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
	e->ModelBounds					   = ModelBounds;
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
	if ( !Ral_SkyEnvironmentRuntimeInit( RAL_BACKEND_WEBGPU,
			&s_module.skyRuntime ) ) {
		memset( &s_module, 0, sizeof( s_module ) );
		return NULL;
	}
	if ( imports->GetLogChannel && imports->LogCh )
		s_module.logChannel = imports->GetLogChannel( "renderer.init" );
	FillExports( &s_module.exports );
	s_module.loaded = qtrue;
	return &s_module.exports;
}

#ifdef RAL_WEBGPU_BROWSER_SMOKE
#include "ral_webgpu_smoke_fixture.h"
static qboolean s_rendererSmokeActive;
static refexport_t *s_rendererSmokeExports;
static refdef_t s_rendererSmokeView;
static qhandle_t s_rendererSmokeMaterial;
static uint32_t s_rendererSmokeFixture;
static uint32_t s_rendererSmokeTeleportStep;
static int s_exposureSmokeStep, s_exposureSmokeUi;
static ralSkyEnvironmentRuntimeState_t s_uiSmokeSkyRuntime;
static uint32_t s_uiSmokeCompletedSkyWork;

Q_EXPORT int WiredWebGpu_UiSmokeFrame( int transition )
{
	glconfig_t config = {0};
	const float color[4] = {0.25f, 0.5f, 1.0f, 1.0f};
	if ( !s_rendererSmokeActive || s_module.pendingFrameReady
			|| transition < 0 || transition > 2 ) return 0;
	if ( transition == 1 ) {
		if ( !s_rendererSmokeExports->UnloadWorld( 0 ) ) return 0;
		s_rendererSmokeExports->Shutdown( REF_LEVEL_ONLY );
	} else if ( transition == 2 ) {
		s_rendererSmokeExports->BeginRegistration( &config );
		s_rendererSmokeExports->EndRegistration();
	}
	const qhandle_t material = s_rendererSmokeExports->RegisterShader( "*white" );
	if ( !material || s_module.exports.initFailed ) return 0;
	s_uiSmokeSkyRuntime = s_module.skyRuntime;
	s_uiSmokeCompletedSkyWork = s_module.completedSkyWorkMask;
	s_rendererSmokeExports->BeginFrame( STEREO_CENTER );
	s_rendererSmokeExports->SetColor( color );
	s_rendererSmokeExports->DrawStretchPic( 32, 32, 192, 96, 0, 0, 1, 1, material );
	s_rendererSmokeExports->SetColor( NULL );
	s_rendererSmokeExports->EndFrame( NULL, NULL );
	return !s_module.exports.initFailed && s_module.pendingFrameReady;
}

Q_EXPORT int WiredWebGpu_UiSmokePoll( void )
{
	ralWebGpuRendererModuleFrameReceipt_t receipt;
	const int status = WiredWebGpu_RendererPoll();
	if ( status != RAL_WEBGPU_ASYNC_READY ) return status;
	if ( !WiredWebGpu_GetFrameReceipt( &receipt ) || receipt.frontend.sceneRendered
			|| receipt.product.uiDrawCount != 1u || receipt.product.command.dispatchCount
			|| receipt.product.worldDrawCount || receipt.product.entityDrawCount
			|| receipt.product.toneMapDrawCount || receipt.product.exposureDispatchCount
			|| receipt.product.worldVertexBuffer.resourceIdentity
			|| receipt.product.worldIndexBuffer.resourceIdentity
			|| memcmp( &s_uiSmokeSkyRuntime, &s_module.skyRuntime, sizeof( s_uiSmokeSkyRuntime ) )
			|| s_uiSmokeCompletedSkyWork != s_module.completedSkyWorkMask ) return -1;
	return RAL_WEBGPU_ASYNC_READY;
}

static void RendererSmokeUsePhysicalSky( atmosphereFrameState_t *state )
{
	atmosphereSkyEnvironment_t *sky;
	if ( !state || state->qualityTier != ATMOSPHERE_QUALITY_FULL ) return;
	sky = &state->skyEnvironment;
	memset( sky, 0, sizeof( *sky ) );
	sky->schemaVersion = WIRED_SKY_ENVIRONMENT_SCHEMA_VERSION;
	sky->mode = ATMOSPHERE_SKY_PHYSICAL;
	sky->sourceRevision = 239u;
	sky->sunDirection[2] = 1.0f;
	sky->sunRadiance[0] = 1.0f;
	sky->sunRadiance[1] = 0.9f;
	sky->sunRadiance[2] = 0.8f;
	sky->moonDirection[2] = -1.0f;
	sky->skyLuminance = 1.0f;
	sky->exposure = 1.0f;
	sky->opticsVisibility = 1.0f;
	sky->distantSceneScale = 1.0f;
}

static qboolean RendererSmokeSubmitFixture( uint32_t fixtureIndex )
{
	ralAtmosphereFixtureState_t authored;
	atmosphereEmitter_t emitter;
	atmosphereMediaVolume_t media;
	if ( !s_rendererSmokeExports ||
			!Ral_AtmosphereConformanceFixture(
				(ralAtmosphereFixture_t)fixtureIndex, &authored ) )
		return qfalse;
	RendererSmokeUsePhysicalSky( &authored.state );
	authored.state.timelineSeconds = (float)fixtureIndex
		+ (float)s_rendererSmokeTeleportStep * 0.01f;
	s_rendererSmokeView.time = (int)(authored.state.timelineSeconds * 1000.0f);
	/* Alternate the real cvar consumer after the first default-enabled frame. */
	if (s_module.exposureControls[0]) {
		s_module.exposureControls[0]->integer = !(fixtureIndex & 1u);
		s_module.exposureControls[0]->value = (float)!(fixtureIndex & 1u);
		s_module.exposureControls[8]->integer = (int)((fixtureIndex-1u)%5u);
		s_module.exposureControls[8]->value = (float)s_module.exposureControls[8]->integer;
		s_module.exposureControls[9]->value = 1.4f;
	}
	s_rendererSmokeExports->SetAtmosphere( &authored.state );
	s_rendererSmokeExports->BeginFrame( STEREO_CENTER );
	s_rendererSmokeExports->ClearScene();
    {
        lensSourceDesc_t source = {0};
        source.id=7; source.origin[0]=8.0f;source.origin[2]=2.0f;source.radius=0.25f;
        for (int i=0;i<4;++i) source.rgba[i]=1.0f;
        s_rendererSmokeExports->AddLensSourceToScene(&source);
        source.id=8;source.origin[0]=-8.0f;
        s_rendererSmokeExports->AddLensSourceToScene(&source);
    }
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

Q_EXPORT int WiredWebGpu_RendererSmokeBegin( int uiOnly )
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
	WiredWebGpuSmokeFixture_Init( &imports, "/wired-smoke" );
	renderer = GetRefAPI( REF_API_VERSION, &imports );
	if ( !renderer )
		return -1;
	memset( &config, 0, sizeof( config ) );
	renderer->BeginRegistration( &config );
	if ( renderer->initFailed || config.vidWidth != 1280 || config.vidHeight != 720 )
		return s_module.failureStage ? -s_module.failureStage : -2;
	if ( uiOnly == 1 ) {
		s_rendererSmokeExports = renderer;
		s_rendererSmokeActive = qtrue;
		renderer->EndRegistration();
		return WiredWebGpu_UiSmokeFrame( 0 ) ? 1 : -34;
	}
	memset( &map, 0, sizeof( map ) );
	memset( &surface, 0, sizeof( surface ) );
	memset( vertices, 0, sizeof( vertices ) );
	memset( &shader, 0, sizeof( shader ) );
	(void)snprintf( map.name, sizeof( map.name ), "%s", WIRED_WEBGPU_SMOKE_MAP );
	map.checksum	   = WIRED_WEBGPU_SMOKE_CHECKSUM;
	map.numSurfaces	   = 1;
	map.surfaces	   = &surface;
	map.numDrawVerts   = 3;
	map.drawVerts	   = vertices;
	map.numDrawIndexes = 3;
	map.drawIndexes	   = indices;
	map.numShaders	   = 1;
	map.shaders		   = &shader;
	(void)snprintf( shader.shader, sizeof( shader.shader ), "%s", WIRED_WEBGPU_SMOKE_TEXTURE );
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
	/* A white placeholder or Debug-only registration must not pass Release
	 * acceptance: prove real image identity plus reference/package/product IO. */
	{
		renderTextureSnapshot_t texture;
		if ( !RenderSubmission_TextureSnapshot( &s_module.frontend, material, &texture )
				|| !texture.textureCacheKeyReady || texture.width != 2u
				|| texture.height != 2u ) return -32;
#if defined( NDEBUG )
		if ( WiredWebGpuSmokeFixture_CookedReads() < 3u
				|| !RenderMaterialRuntimeOwner_World( &s_module.materialRuntimeOwner, 0 ) ) return -33;
#endif
	}
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
	s_rendererSmokeTeleportStep = 0u;
	memset( &s_rendererSmokeView, 0, sizeof( s_rendererSmokeView ) );
	s_rendererSmokeView.width = 1280;
	s_rendererSmokeView.height = 720;
	s_rendererSmokeView.fov_x = 90.0f;
	s_rendererSmokeView.fov_y = 60.0f;
	s_rendererSmokeView.viewaxis[0][0] = s_rendererSmokeView.viewaxis[1][1] =
		s_rendererSmokeView.viewaxis[2][2] = 1.0f;
	s_rendererSmokeActive = qtrue;
	if ( uiOnly == 2 ) return WiredWebGpu_UiSmokeFrame( 0 ) ? 1 : -35;
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
    {
        float visible=-1.0f, behind=-1.0f;
        if (!s_rendererSmokeExports->GetLensVisibility(7,&visible) ||
            !s_rendererSmokeExports->GetLensVisibility(8,&behind))
            return s_module.exports.initFailed ? -90 : 1;
        if (s_module.lensPublishedFrame != receipt.frameGeneration) return 1;
        if (fabsf(visible - receipt.atmosphere.skyEnvironment.opticsVisibility)>0.0001f || behind!=0.0f) return -91;
    }
	if ( !Ral_AtmosphereConformanceFixture(
			(ralAtmosphereFixture_t)s_rendererSmokeFixture, &authored ) ) return -21;
	RendererSmokeUsePhysicalSky( &authored.state );
	const qboolean noWorld = (s_rendererSmokeView.rdflags & RDF_NOWORLDMODEL) ? qtrue : qfalse;
	if ( receipt.product.worldDrawCount != (noWorld ? 0u : 1u) || receipt.product.uiDrawCount != 1u ||
		 receipt.product.atmosphereBoundDrawCount != (noWorld ? 0u : 1u) ) return -3;
	if (noWorld && (receipt.product.worldVertexBuffer.resourceIdentity ||
		receipt.product.worldIndexBuffer.resourceIdentity ||
		receipt.product.worldMaterialBuffer.resourceIdentity)) return -94;
	if (receipt.product.toneMapDrawCount != 1u ||
		receipt.product.exposureDispatchCount != ((noWorld || (s_rendererSmokeFixture & 1u)) ? 0u : 2u) ||
		receipt.product.command.dispatchCount != receipt.product.exposureDispatchCount) return -93;
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
				receipt.atmosphere.computeDispatchCount ||
			 receipt.atmosphereExecution.nativeDispatchCount !=
				receipt.atmosphere.computeDispatchCount + 1u ||
			 receipt.atmosphereExecution.command.dispatchCount !=
				receipt.atmosphereExecution.nativeDispatchCount ) return -5;
		/* Count the light-tile dispatch, not only the semantic atmosphere stages.
		 * A forged native count must not pass the self-consistency predicate. */
		ralWebGpuAtmosphereReceipt_t forged = receipt.atmosphereExecution;
		++forged.nativeDispatchCount;
		if ( RalWebGpu_AtmosphereReceiptExact( &forged, &forged ) ) return -54;
	} else if ( receipt.atmosphereExecution.schemaVersion != 0u ) return -51;
	{
		const uint32_t physicalSkyMask = RAL_SKY_DIRTY_TRANSMITTANCE
			| RAL_SKY_DIRTY_MULTISCATTER | RAL_SKY_DIRTY_SKY_VIEW;
		const uint32_t admittedPhysicalSky =
			receipt.atmosphere.skyEnvironment.admittedWorkMask
				& physicalSkyMask;
		if ( admittedPhysicalSky ) {
			if ( !RalWebGpu_SkyReceiptExact( &receipt.skyExecution,
					&receipt.skyExecution )
					|| receipt.skyExecution.workMask != admittedPhysicalSky
					|| receipt.skyExecution.command.dispatchCount != 1u ) return -52;
		} else if ( receipt.skyExecution.schemaVersion != 0u ) return -53;
	}
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
	/* Actual browser GPU: normal -> no-world teleport/UI -> flag-free world.
	 * AE stays enabled in the cvar; only the no-world frame must bypass metering. */
	if ( s_rendererSmokeFixture == RAL_ATMOSPHERE_FIXTURE_CLEAR
			&& s_rendererSmokeTeleportStep < 2u ) {
		++s_rendererSmokeTeleportStep;
		s_rendererSmokeView.rdflags = s_rendererSmokeTeleportStep == 1u
			? RDF_NOWORLDMODEL | RDF_HYPERSPACE | RDF_CAMERA_CUT : 0;
		if ( !RendererSmokeSubmitFixture( s_rendererSmokeFixture ) ) return -2000 - s_module.failureStage;
		return 1;
	}
	if ( ++s_rendererSmokeFixture < RAL_ATMOSPHERE_FIXTURE_COUNT ) {
        float value=123.0f;
        if (!ResetLensVisibility() || s_rendererSmokeExports->GetLensVisibility(7,&value) || value!=123.0f) return -92;
		if ( !RendererSmokeSubmitFixture( s_rendererSmokeFixture ) ) return -2000 - s_module.failureStage;
		return 1;
	}
	s_module.exports.Shutdown( REF_UNLOAD_DLL );
	s_rendererSmokeExports = NULL;
	s_rendererSmokeActive = qfalse;
	return 2;
}
/* Real Product history/pixel probe. Reuses the registered cooked smoke world
 * and public renderer entry points; no exposure-buffer injection or alternate
 * shader. Compiled only into the browser smoke module. */
Q_EXPORT int WiredWebGpu_ExposureSmokeFrame( int step, int ui )
{
	refEntity_t surface = {0};
	atmosphereFrameState_t atmosphere = {0};
	const float settings[10] = {1, .0123f, .5f, .2f, 1.5f, .5f, .25f, 8, 0, 5.2f};
	if (!s_rendererSmokeActive || s_module.pendingFrameReady || step < 0 || step > 55)
		return 0;
	for (unsigned i=0; i<10; ++i) {
		if (!s_module.exposureControls[i]) return 0;
		s_module.exposureControls[i]->value=settings[i];
		s_module.exposureControls[i]->integer=(int)settings[i];
	}
	s_module.brightness->value=1.0f; s_module.brightness->integer=1;
	s_exposureSmokeStep=step; s_exposureSmokeUi=ui!=0;
	s_rendererSmokeView.rdflags=(step==0 || step==18) ? RDF_CAMERA_CUT : 0;
	s_rendererSmokeView.time=1000+step*100;
	if (step==17) s_rendererSmokeView.time=2600; // pause after adaptation
	if (step==19) s_rendererSmokeView.time=2800; // pause on reset result
	if (step>=36) s_rendererSmokeView.time=500+(step-36)*100; // rewind
	if (step==53) s_rendererSmokeView.rdflags=RDF_NOWORLDMODEL|RDF_HYPERSPACE|RDF_CAMERA_CUT;
	if (step==55) s_rendererSmokeView.time=2300; // pause after teleport return
	surface.reType=RT_SPRITE; surface.customShader=s_rendererSmokeExports->RegisterShader("*white");
	if (!surface.customShader) return 0;
	surface.origin[0]=8.0f; surface.radius=32.0f;
	surface.shader.rgba[0]=surface.shader.rgba[1]=surface.shader.rgba[2]=64;
	surface.shader.rgba[3]=255;
	atmosphere.schemaVersion=WIRED_ATMOSPHERE_SCHEMA_VERSION;
	atmosphere.timelineSeconds=s_rendererSmokeView.time*.001f;
	s_rendererSmokeExports->SetAtmosphere(&atmosphere);
	s_rendererSmokeExports->BeginFrame(STEREO_CENTER);
	s_rendererSmokeExports->ClearScene();
	if (step!=53) s_rendererSmokeExports->AddRefEntityToScene(&surface,qfalse);
	s_rendererSmokeExports->RenderScene(&s_rendererSmokeView,0);
	if (ui) {
		s_rendererSmokeExports->SetColor(NULL);
		s_rendererSmokeExports->DrawStretchPic(0,0,640,720,0,0,1,1,surface.customShader);
	}
	s_rendererSmokeExports->EndFrame(NULL,NULL);
	return !s_rendererSmokeExports->initFailed && s_module.pendingFrameReady;
}

/* Actual weather compute + draw: public heightgrid upload, no pool injection. */
Q_EXPORT int WiredWebGpu_WeatherGroundSmokeFrame( int step )
{
	atmosphereFrameState_t atmosphere = {0};
	float grid[4];
	const float floors[11] = {-1000, 1000, 1000, 0, -1000, 1000, 0, 0, 1000, -1000, -1000};
	const float settings[10] = {0, .0123f, .5f, .2f, 1.5f, .5f, .25f, 8, 0, 1};
	if (!s_rendererSmokeActive || s_module.pendingFrameReady || step < 0 || step >= 11) return 0;
	if (step == 8 || step == 9) {
		const mapFile_t *map = s_module.loadedWorld;
		renderAtmosphereHeightgrid_t oldGrid, probe;
		if (!map || !RenderSubmission_AtmosphereHeightgrid(&s_module.frontend, &oldGrid)) return -2;
		probe = oldGrid;
		if (!s_rendererSmokeExports->UnloadWorld(0)
			|| RenderSubmission_AtmosphereHeightgrid(&s_module.frontend, &probe)
			|| memcmp(&probe, &oldGrid, sizeof(probe))) return -3;
		s_rendererSmokeExports->LoadWorld(map, 0);
		uint64_t generation;
		if (s_rendererSmokeExports->initFailed
			|| !RenderSubmission_WorldLifecycleGeneration(&s_module.frontend, 0, &generation)
			|| generation == oldGrid.worldGeneration
			|| RenderSubmission_AtmosphereHeightgrid(&s_module.frontend, &probe)) return -4;
	}
	for (unsigned i=0; i<4; ++i) grid[i]=floors[step];
	/* z = +/- y/2: samples are deliberately asymmetric, not a flat roof.
	 * With extent512 and edge2, rows represent y=-128 and y=128. */
	if (step == 6 || step == 7) {
		const float sign = step == 6 ? 1.0f : -1.0f;
		grid[0]=grid[1]=-64.0f*sign; grid[2]=grid[3]=64.0f*sign;
	}
	for (unsigned i=0; i<10; ++i) {
		if (!s_module.exposureControls[i]) return 0;
		s_module.exposureControls[i]->value=settings[i];
		s_module.exposureControls[i]->integer=(int)settings[i];
	}
	s_module.brightness->value=1.0f; s_module.brightness->integer=1;
	atmosphere.schemaVersion=WIRED_ATMOSPHERE_SCHEMA_VERSION;
	atmosphere.qualityTier=ATMOSPHERE_QUALITY_WEATHER;
	atmosphere.flags=ATMOSPHERE_FLAG_ENABLED|ATMOSPHERE_FLAG_HEIGHTGRID;
	atmosphere.timelineSeconds=10.0f+step*.02f;
	atmosphere.indoorExposure=1.0f;
	atmosphere.precipitation[0]=1.0f;
	atmosphere.distance=1024.0f;
	atmosphere.seed=732u;
	atmosphere.gridSize=2;
	atmosphere.bounds[0]=64; atmosphere.bounds[3]=128;
	atmosphere.bounds[1]=atmosphere.bounds[2]=-128;
	atmosphere.bounds[4]=atmosphere.bounds[5]=128;
	for (unsigned i=0; i<2; ++i) {
		atmosphere.worldMins[i]=-128;
		atmosphere.worldMaxs[i]=(step == 6 || step == 7) ? 384 : 128;
	}
	s_rendererSmokeView.rdflags=RDF_NOWORLDMODEL|RDF_CAMERA_CUT;
	s_rendererSmokeView.time=10000+step*20;
	s_rendererSmokeExports->BeginFrame(STEREO_CENTER);
	s_rendererSmokeExports->ClearScene();
	s_rendererSmokeExports->SetAtmosphere(&atmosphere);
	s_rendererSmokeExports->SetAtmosphereHeightgrid(grid,4);
	s_rendererSmokeExports->RenderScene(&s_rendererSmokeView,0);
	s_rendererSmokeExports->EndFrame(NULL,NULL);
	return !s_rendererSmokeExports->initFailed && s_module.pendingFrameReady;
}

Q_EXPORT int WiredWebGpu_WeatherGroundSmokePoll( void )
{
	ralWebGpuRendererModuleFrameReceipt_t r;
	const int status=WiredWebGpu_RendererPoll();
	if (status!=RAL_WEBGPU_ASYNC_READY) return status;
	if (!WiredWebGpu_GetFrameReceipt(&r) || !r.weather.heightgridActive
		|| r.product.weatherDrawCount!=1u || r.weatherExecution.dispatchCount!=1u
		|| r.product.unresolvedCount || r.product.fallbackCount || r.product.fatalCount) return -1;
	return RAL_WEBGPU_ASYNC_READY;
}

Q_EXPORT int WiredWebGpu_ExposureSmokePoll( void )
{
	ralWebGpuRendererModuleFrameReceipt_t r;
	const int status=WiredWebGpu_RendererPoll();
	if (status!=RAL_WEBGPU_ASYNC_READY) return status;
	if (!WiredWebGpu_GetFrameReceipt(&r)) return -1;
	const qboolean noWorld=s_exposureSmokeStep==53;
	if (r.product.worldDrawCount!=(noWorld?0u:1u)
		|| r.product.entityDrawCount!=(noWorld?0u:1u)
		|| r.product.uiDrawCount!=(uint32_t)s_exposureSmokeUi
		|| r.product.exposureDispatchCount!=(noWorld?0u:2u)
		|| r.product.toneMapDrawCount!=1u || r.product.unresolvedCount
		|| r.product.fallbackCount || r.product.fatalCount) return -2;
	return RAL_WEBGPU_ASYNC_READY;
}

/* Test-only original constant texels exercise the production material upload,
 * beam lowering, fragment shader, blend state and HDR composition together. */
static uint32_t s_beamSmokeExpectedStages;
static qboolean s_beamSmokeSprite;
static int s_beamSmokeFog;
Q_EXPORT int WiredWebGpu_BeamCoverageSmokeFrame( int textureAlpha, int descriptorAlpha, int additive,
        int fogMode, int emission )
{
	beamDesc_t beam = {0};
	atmosphereFrameState_t atmosphere = {0};
	byte texel[4] = {255, 255, 255, 0};
	char name[64];
	if (!s_rendererSmokeActive || s_module.pendingFrameReady
			|| textureAlpha < 0 || textureAlpha > 255
			|| descriptorAlpha < 0 || descriptorAlpha > 255 || additive < 0 || additive > 3
            || fogMode < 0 || fogMode > 4 || emission < 0 || emission > 255) return 0;
	s_beamSmokeSprite = fogMode >= 3;
	s_beamSmokeFog = fogMode ? (fogMode - 1) % 2 + 1 : 0;
	if (fogMode) texel[0] = texel[1] = texel[2] = (byte)emission;
	texel[3] = (byte)textureAlpha;
	snprintf(name, sizeof(name), "*beam-coverage-%d-%d-%d-%d", textureAlpha, additive, fogMode, emission);
	beam.shader = RenderSubmission_RegisterMaterialImage(&s_module.frontend,
		RENDER_ASSET_MATERIAL, name, qtrue, texel, 1u, 1u);
	if (!beam.shader) return 0;
	beam.start[0] = beam.end[0] = 8.0f;
	beam.start[1] = -4.0f; beam.end[1] = 4.0f;
	beam.startWidth = beam.endWidth = 1.0f;
	for (unsigned c=0; c<3; ++c) beam.startColor[c] = beam.endColor[c] = 1.0f;
	beam.startColor[3] = beam.endColor[3] = (float)descriptorAlpha / 255.0f;
	beam.axialCopies = 1;
	beam.startEntityNum = beam.endEntityNum = -1;
	beam.flags = additive ? PRIM_FLAG_ADDITIVE : 0;
	s_beamSmokeExpectedStages = additive == 2 ? 2u : 1u;
	if (additive == 2) {
		renderMaterialStageSnapshot_t stages[2] = {{0}};
		const byte texels[2][4] = {{64,0,0,255}, {0,128,0,128}};
		for (unsigned i=0; i<2; ++i) {
			stages[i].material = RenderSubmission_RegisterStageTexture(&s_module.frontend,
				RENDER_ASSET_MATERIAL, i ? "*beam-stage-green" : "*beam-stage-red", qfalse, texels[i], 1u, 1u);
			if (!stages[i].material) return 0;
			stages[i].scaleScroll[0] = stages[i].scaleScroll[1] = 1.0f;
		}
		if (!RenderSubmission_SetMaterialStages(&s_module.frontend, beam.shader, stages, 2u)) return 0;
		beam.flags = 0; // Explicit primitive stages still accumulate, as in Vulkan.
	} else if (additive == 3) {
		for (unsigned c=0; c<3; ++c) {
			beam.startColor[c] = .12345f;
			beam.endColor[c] = .87654f;
		}
	}
	s_module.exposureControls[0]->value = 0.0f;
	s_module.exposureControls[0]->integer = 0;
	s_module.exposureControls[8]->value = 0.0f;
	s_module.exposureControls[8]->integer = 0;
	s_module.brightness->value = 1.0f; s_module.brightness->integer = 1;
	s_rendererSmokeView.rdflags = RDF_NOWORLDMODEL | RDF_CAMERA_CUT;
	s_rendererSmokeView.time += 100;
	atmosphere.schemaVersion = WIRED_ATMOSPHERE_SCHEMA_VERSION;
	atmosphere.timelineSeconds = s_rendererSmokeView.time * .001f;
	if (fogMode) {
		atmosphere.flags = ATMOSPHERE_FLAG_ENABLED | ATMOSPHERE_FLAG_SKY_LIGHTING;
		for (unsigned c=0; c<3; ++c) atmosphere.ambientColor[c] = .2f;
		atmosphere.qualityTier = s_beamSmokeFog == 2 ? ATMOSPHERE_QUALITY_FULL : ATMOSPHERE_QUALITY_ANALYTIC;
		atmosphere.mediaDensity = .1f;
		atmosphere.visibility = s_beamSmokeFog == 2 ? 64.0f : 0.0f;
		atmosphere.sunDirection[2] = 1.0f;
		atmosphere.sunIntensity = 1.0f;
	}
	s_rendererSmokeExports->SetAtmosphere(&atmosphere);
	s_rendererSmokeExports->BeginFrame(STEREO_CENTER);
	s_rendererSmokeExports->ClearScene();
	if (s_beamSmokeSprite) {
		spriteDesc_t sprite = {0};
		sprite.origin[0] = 8.0f; sprite.radius = 4.0f;
		sprite.shader = beam.shader; sprite.flags = beam.flags;
		memcpy(sprite.rgba, beam.startColor, sizeof(sprite.rgba));
		s_rendererSmokeExports->AddSpriteToScene(&sprite);
	} else s_rendererSmokeExports->AddBeamToScene(&beam);
	s_rendererSmokeExports->RenderScene(&s_rendererSmokeView, 0);
	s_rendererSmokeExports->EndFrame(NULL, NULL);
	return !s_rendererSmokeExports->initFailed && s_module.pendingFrameReady;
}

Q_EXPORT int WiredWebGpu_BeamCoverageSmokePoll( void )
{
	ralWebGpuRendererModuleFrameReceipt_t r;
	const int status = WiredWebGpu_RendererPoll();
	if (status != RAL_WEBGPU_ASYNC_READY) return status;
	if (!WiredWebGpu_GetFrameReceipt(&r)) return -1;
	if (r.product.effectBeamDrawCount != (s_beamSmokeSprite ? 0u : 1u) || r.product.worldDrawCount
			|| r.product.effectSpriteDrawCount != (s_beamSmokeSprite ? 1u : 0u)
			|| r.product.effectBeamStageDrawCount != (s_beamSmokeSprite ? 0u : s_beamSmokeExpectedStages)
			|| r.product.effectParameterCount != (s_beamSmokeSprite ? 1u : s_beamSmokeExpectedStages + 1u)
			|| r.product.entityDrawCount || r.product.uiDrawCount
			|| r.product.exposureDispatchCount || r.product.toneMapDrawCount != 1u
			|| r.product.unresolvedCount || r.product.fallbackCount || r.product.fatalCount) return -2;
	if (s_beamSmokeFog && r.atmosphere.selectedTier != (s_beamSmokeFog == 2
		? RAL_ATMOSPHERE_TIER_FULL : RAL_ATMOSPHERE_TIER_ANALYTIC)) return -3;
	if (s_beamSmokeFog == 2 && (r.atmosphere.froxelDepth != 64u
		|| r.atmosphereExecution.historyReused)) return -4;
	return RAL_WEBGPU_ASYNC_READY;
}

// Original synthetic texels, actual importer snapshots/Product draw, no shader
// substitution. The first call replaces the smoke world's geometry through
// normal unload/load, retaining its cooked material identity.
Q_EXPORT int WiredWebGpu_SkyMaterialSmokeFrame(int count,int timeMs,int looping,int reload,int capture) {
    static mapFile_t map;
    static dsurface_t surface;
    static drawVert_t vertices[4];
    static int indices[6]={0,1,2,0,2,3};
    static dshader_t shader;
    renderMaterialStageSnapshot_t stages[8]={{0}};
    atmosphereFrameState_t atmosphere={0};
    if(!s_rendererSmokeActive||s_module.pendingFrameReady||count<1||count>8||capture<0||capture>5) return 0;
    for(unsigned i=0;i<8;++i) {
        byte texel[4]={0,0,0,16}; texel[i%3]=255;
        char name[64]; snprintf(name,sizeof(name),"*sky-stage-%u",i);
        stages[i].material=RenderSubmission_RegisterStageTexture(&s_module.frontend,
            RENDER_ASSET_MATERIAL,name,qfalse,texel,1,1);
        if(!stages[i].material) return 0;
        stages[i].sourceBlend=6; stages[i].destinationBlend=1;
        stages[i].scaleScroll[0]=stages[i].scaleScroll[1]=1;
        stages[i].rgbGen=RAL_MATERIAL_COLOR_IDENTITY;
        stages[i].alphaGen=RAL_MATERIAL_ALPHA_IDENTITY;
    }
    byte animated[4]={255,255,255,32};
    const qhandle_t next=RenderSubmission_RegisterStageTexture(&s_module.frontend,
        RENDER_ASSET_MATERIAL,"*sky-anim-next",qfalse,animated,1,1);
    if(!next) return 0;
    stages[7].animationFrameCount=2; stages[7].animationFrequency=2;
    stages[7].animationLooping=looping?qtrue:qfalse;
    stages[7].animationMaterials[0]=stages[7].material;
    stages[7].animationMaterials[1]=next;
    if(capture==2 || capture==3) {
        // Three independent clamped ramps encode world X/Y/Z. A 2-texel
        // linear ramp with u=.5+position/4096 produces .5+position/2048.
        // The browser oracle uses a separate cube-face convention.
        count=3;
        memset(stages,0,sizeof(stages));
        for(unsigned axis=0;axis<3;++axis) {
            byte ramp[8]={0,0,0,255,0,0,0,255}; ramp[4+axis]=255;
            char name[64]; snprintf(name,sizeof(name),"*sky-direction-%u",axis);
            stages[axis].material=RenderSubmission_RegisterStageTexture(&s_module.frontend,
                RENDER_ASSET_MATERIAL,name,qtrue,ramp,2,1);
            if(!stages[axis].material) return 0;
            stages[axis].sourceBlend=1; stages[axis].destinationBlend=1;
            stages[axis].scaleScroll[0]=stages[axis].scaleScroll[1]=1;
            stages[axis].scaleScroll[2]=.5f;
            stages[axis].tcGen=3;
            stages[axis].tcGenVectors[0][axis]=1.0f/4096.0f;
            stages[axis].rgbGen=RAL_MATERIAL_COLOR_IDENTITY;
            stages[axis].alphaGen=RAL_MATERIAL_ALPHA_IDENTITY;
        }
    }
    if((!capture || reload) && (!RenderSubmission_SetMaterialSky(&s_module.frontend,s_rendererSmokeMaterial,qtrue)
        ||!RenderSubmission_SetMaterialStages(&s_module.frontend,s_rendererSmokeMaterial,stages,(uint32_t)count))) return 0;
    if(capture && reload) {
        const ralMaterialSourceSky_t source={0};
        if(!RenderSubmission_SetMaterialAuthoredSkySource(&s_module.frontend,s_rendererSmokeMaterial,&source)) return 0;
    }
    if(reload) {
        if(!s_rendererSmokeExports->UnloadWorld(0)) return 0;
        memset(&map,0,sizeof(map)); memset(&surface,0,sizeof(surface));
        memset(vertices,0,sizeof(vertices)); memset(&shader,0,sizeof(shader));
        snprintf(map.name,sizeof(map.name),"%s",WIRED_WEBGPU_SMOKE_MAP);
        snprintf(shader.shader,sizeof(shader.shader),"%s",WIRED_WEBGPU_SMOKE_TEXTURE);
        map.checksum=WIRED_WEBGPU_SMOKE_CHECKSUM;
        map.numSurfaces=1; map.surfaces=&surface; map.numDrawVerts=4; map.drawVerts=vertices;
        map.numDrawIndexes=6; map.drawIndexes=indices; map.numShaders=1; map.shaders=&shader;
        surface.surfaceType=MST_PLANAR; surface.numVerts=4; surface.numIndexes=6; surface.lightmapNum=-1;
        for(unsigned i=0;i<4;++i) {
            vertices[i].xyz[0]=8;
            vertices[i].xyz[1]=(i==1||i==2)?16:-16;
            vertices[i].xyz[2]=i>=2?16:-16;
            vertices[i].normal[0]=-1;
            memset(vertices[i].color.rgba,255,4);
        }
        s_rendererSmokeExports->LoadWorld(&map,0);
        if(s_rendererSmokeExports->initFailed) return 0;
    }
    s_module.exposureControls[0]->value=0; s_module.exposureControls[0]->integer=0;
    s_module.exposureControls[8]->value=0; s_module.exposureControls[8]->integer=0;
    s_module.brightness->value=1; s_module.brightness->integer=1;
    s_rendererSmokeView.time=timeMs;
    // A progressive capture spans continuous frames, not a camera cut or
    // material replacement on every face. Reload still exercises invalidation.
    s_rendererSmokeView.rdflags=(!capture || reload)?RDF_CAMERA_CUT:0;
    atmosphere.schemaVersion=WIRED_ATMOSPHERE_SCHEMA_VERSION;
    atmosphere.qualityTier=capture?RAL_ATMOSPHERE_TIER_WEATHER:RAL_ATMOSPHERE_TIER_OFF;
    atmosphere.flags=capture?ATMOSPHERE_FLAG_ENABLED:0u;
    if(capture>=3) {
        // Physical LUTs are a full-tier feature. Keep the real tier budget;
        // the browser verifies all faces admitted in each bounded batch.
        atmosphere.qualityTier=ATMOSPHERE_QUALITY_FULL;
        RendererSmokeUsePhysicalSky(&atmosphere);
        if(capture>=4) {
            // Retain the physical declaration and warm GPU products while
            // the selected tier withdraws them; no source edit or reload.
            atmosphere.qualityTier=capture==4?ATMOSPHERE_QUALITY_OFF:ATMOSPHERE_QUALITY_ANALYTIC;
            atmosphere.flags=capture==4?0u:ATMOSPHERE_FLAG_ENABLED;
        }
    }
    atmosphere.timelineSeconds=timeMs*.001f;
    s_rendererSmokeExports->SetAtmosphere(&atmosphere);
    s_rendererSmokeExports->BeginFrame(STEREO_CENTER);
    s_rendererSmokeExports->ClearScene();
    s_rendererSmokeExports->RenderScene(&s_rendererSmokeView,0);
    s_rendererSmokeExports->EndFrame(NULL,NULL);
    return !s_rendererSmokeExports->initFailed&&s_module.pendingFrameReady;
}
Q_EXPORT int WiredWebGpu_SkyMaterialSmokePoll(void) {
    ralWebGpuRendererModuleFrameReceipt_t r;
    const int status=WiredWebGpu_RendererPoll();
    if(status!=RAL_WEBGPU_ASYNC_READY) return status;
    if(!WiredWebGpu_GetFrameReceipt(&r)||r.product.worldDrawCount!=1||r.product.entityDrawCount
        ||r.product.uiDrawCount||r.product.exposureDispatchCount||r.product.toneMapDrawCount!=1
        ||r.product.unresolvedCount||r.product.fallbackCount||r.product.fatalCount) return -2;
    return RAL_WEBGPU_ASYNC_READY;
}
// Smoke-only observation of the real production capture, not a test texture.
Q_EXPORT uintptr_t WiredWebGpu_SkyMaterialSmokeSource(int query) {
    ralWebGpuSkyRadianceResources_t resources;
    if(!s_rendererSmokeActive||s_module.pendingFrameReady) return 0;
    if(query==1) return s_module.published.skyRadianceExecution.sliceMask & 63u;
    if(query==2) return s_module.published.skyRadianceExecution.command.drawCount;
    if(query==3 || query==4) {
        uintptr_t texture,sampler; uint64_t generation;
        if(!RalWebGpu_SkySampleResources(s_module.sky,&texture,&sampler,&generation)) return 0;
        return query==3?texture:sampler;
    }
    if(query==5) return s_module.published.atmosphere.skyEnvironment.completedProductMask & RAL_SKY_DIRTY_SKY_VIEW;
    if(query==6) return s_module.published.atmosphereExecution.dispatchCount;
    if(query==7) return s_module.skyRadianceConsumer.ready;
    if(query==8) return s_module.published.skyRadianceExecution.consumer.ready;
    if(query||!RalWebGpu_ProductSkyRadianceResources(s_module.product,&s_module.borrow.runtimeReceipt,
        &s_module.frontend,&resources)) return 0;
    return resources.sourceTextureIdentity;
}

Q_EXPORT void WiredWebGpu_ExposureSmokeEnd( void )
{
	if (s_rendererSmokeActive && !s_module.pendingFrameReady) {
		s_module.exports.Shutdown(REF_UNLOAD_DLL);
		s_rendererSmokeExports=NULL; s_rendererSmokeActive=qfalse;
	}
}
#else
Q_EXPORT int WiredWebGpu_RendererSmokeBegin( int uiOnly )
{
	(void)uiOnly;
	return 0;
}
Q_EXPORT int WiredWebGpu_RendererSmokePoll( void )
{
	return 0;
}
#endif
