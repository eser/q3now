// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
#include "ral_metal_atmosphere.h"
#include "../../../frontend/render_submission_world.h"
#include "ral_metal_atmosphere_froxel_inject_metallib.h"
#include "ral_metal_atmosphere_froxel_light_metallib.h"
#include "ral_metal_atmosphere_froxel_cloud_metallib.h"
#include "ral_metal_atmosphere_froxel_integrate_metallib.h"
#include "ral_metal_forwardplus_tile_metallib.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

enum { WIRED_ATM_INJECT, WIRED_ATM_LIGHT, WIRED_ATM_CLOUD, WIRED_ATM_INTEGRATE,
    WIRED_ATM_TILE, WIRED_ATM_KERNELS };
struct ralMetalAtmosphere_s {
    id<MTLDevice> device;
    id<MTLComputePipelineState> pipeline[WIRED_ATM_KERNELS];
    id<MTLArgumentEncoder> arguments[WIRED_ATM_KERNELS];
    id<MTLBuffer> arena, tiles;
};
struct wiredAtmosphereInject {
    float inverse[16], eyeNear[4], farDensity[4], albedo[4];
    uint32_t grid[4]; float timeline[4];
};
struct wiredAtmosphereLight {
    float inverse[16], eyeNear[4], farPad[4]; uint32_t grid[4];
    float sunDirection[4], sunColor[4], moonDirection[4], moonColor[4], ambient[4];
};
struct wiredAtmosphereCloud {
    float inverse[16], eyeNear[4], farTimeCoverShadow[4], windDensity[4], layer[4];
    float sunDirection[4], sunColor[4], ambient[4]; uint32_t grid[4];
};
struct wiredAtmosphereIntegrate { uint32_t grid[4]; float depthHistory[4]; };
struct wiredAtmosphereTile { float inverse[16], screen[4], lightsDepth[4]; };
struct wiredAtmosphereVolume {
    float origin[4], extent[4], albedo[4], emission[4], media[4];
};
struct wiredAtmosphereLocalLight { float origin[4], color[4], endpoint[4]; };
static_assert(sizeof(wiredAtmosphereInject) == 144, "canonical inject ABI");
static_assert(sizeof(wiredAtmosphereLight) == 192, "canonical light ABI");
static_assert(sizeof(wiredAtmosphereCloud) == 192, "canonical cloud ABI");
static_assert(sizeof(wiredAtmosphereIntegrate) == 32, "canonical integrate ABI");
static_assert(sizeof(wiredAtmosphereTile) == 96, "canonical tile ABI");
static_assert(sizeof(wiredAtmosphereVolume) == 80, "canonical volume ABI");
static_assert(sizeof(wiredAtmosphereLocalLight) == 48, "canonical local-light ABI");

void RalMetal_AtmosphereDestroy(ralMetalAtmosphere_t *s) {
    if (!s) return;
    for (unsigned i=0; i<WIRED_ATM_KERNELS; ++i) {
        [s->pipeline[i] release]; [s->arguments[i] release];
    }
    [s->arena release]; [s->tiles release]; [s->device release]; free(s);
}
ralMetalAtmosphere_t *RalMetal_AtmosphereCreate(id<MTLDevice> device) {
    if (!device) return NULL;
    auto s = (ralMetalAtmosphere_t *)calloc(1, sizeof(ralMetalAtmosphere_t));
    if (!s) return NULL;
    s->device = [device retain];
    const unsigned char *bytes[] = {
        ral_metal_atmosphere_froxel_inject_metallib, ral_metal_atmosphere_froxel_light_metallib,
        ral_metal_atmosphere_froxel_cloud_metallib, ral_metal_atmosphere_froxel_integrate_metallib,
        ral_metal_forwardplus_tile_metallib};
    const size_t sizes[] = {
        ral_metal_atmosphere_froxel_inject_metallib_size, ral_metal_atmosphere_froxel_light_metallib_size,
        ral_metal_atmosphere_froxel_cloud_metallib_size, ral_metal_atmosphere_froxel_integrate_metallib_size,
        ral_metal_forwardplus_tile_metallib_size};
    for (unsigned i=0; i<WIRED_ATM_KERNELS; ++i) {
        dispatch_data_t data = dispatch_data_create(bytes[i], sizes[i],
            dispatch_get_global_queue(QOS_CLASS_DEFAULT, 0), ^{});
        if (!data) { RalMetal_AtmosphereDestroy(s); return NULL; }
        NSError *error = nil;
        id<MTLLibrary> library = [device newLibraryWithData:data error:&error];
        dispatch_release(data);
        id<MTLFunction> function = error ? nil : [library newFunctionWithName:@"main0"];
        if (function) {
            s->arguments[i] = [function newArgumentEncoderWithBufferIndex:0];
            s->pipeline[i] = [device newComputePipelineStateWithFunction:function error:&error];
        }
        [function release]; [library release];
        if (error || !s->arguments[i] || !s->pipeline[i]) {
            RalMetal_AtmosphereDestroy(s); return NULL;
        }
    }
    s->arena = [device newBufferWithLength:6u * WIRED_METAL_ATMOSPHERE_SECTION_BYTES
        options:MTLResourceStorageModePrivate];
    s->tiles = [device newBufferWithLength:(WIRED_METAL_ATMOSPHERE_CAPACITY
        / RAL_ATMOSPHERE_MIN_SLICES) * 33u * sizeof(uint32_t)
        options:MTLResourceStorageModePrivate];
    if (!s->arena || !s->tiles) { RalMetal_AtmosphereDestroy(s); return NULL; }
    s->arena.label = @"wired.atmosphere.canonical-arena";
    s->tiles.label = @"wired.atmosphere.light-tiles";
    return s;
}
id<MTLBuffer> RalMetal_AtmosphereArena(ralMetalAtmosphere_t *s) { return s ? s->arena : nil; }
float RalMetal_AtmosphereFarDistance(const atmosphereFrameState_t *state) {
    return wired_atmosphere_grid_far(state ? state->visibility : 0.0f);
}

/* Inverse of the raster camera, with top-down framebuffer Y. Canonical media
 * producers use forward-infinite depth, while the shared tile classifier takes
 * a finite reverse-Z near/far slab. Neither uses map bounds as a camera. */
static bool wired_atmosphere_inverse(const renderWorldSnapshot_t &view,
        float farZ, float grid[16], float tile[16]) {
    // ready describes resident BSP geometry, not the already-published view.
    if (!isfinite(view.fovX) || !isfinite(view.fovY)
            || view.fovX <= 1 || view.fovX >= 179 || view.fovY <= 1 || view.fovY >= 179)
        return false;
    const float tx = tanf(view.fovX * (float)M_PI / 360.0f);
    const float ty = tanf(view.fovY * (float)M_PI / 360.0f);
    memset(grid, 0, 16*sizeof(float)); memset(tile, 0, 16*sizeof(float));
    for (unsigned j=0; j<3; ++j) {
        if (!isfinite(view.viewOrigin[j])) return false;
        for (unsigned k=0; k<3; ++k) if (!isfinite(view.viewAxis[k][j])) return false;
        grid[j] = tile[j] = -view.viewAxis[1][j]*tx;
        grid[4+j] = tile[4+j] = -view.viewAxis[2][j]*ty;
        grid[8+j] = -view.viewOrigin[j]/4.0f;
        grid[12+j] = view.viewAxis[0][j] + view.viewOrigin[j]/4.0f;
        tile[8+j] = view.viewOrigin[j]*(.25f - 1.0f/farZ);
        tile[12+j] = view.viewAxis[0][j] + view.viewOrigin[j]/farZ;
    }
    grid[11] = -.25f; grid[15] = .25f;
    tile[11] = .25f - 1.0f/farZ; tile[15] = 1.0f/farZ;
    return true;
}
static NSUInteger wired_atmosphere_aligned(NSUInteger size) { return (size + 255u) & ~NSUInteger(255u); }

bool RalMetal_AtmosphereEncode(ralMetalAtmosphere_t *s,
        id<MTLCommandBuffer> command, const renderSubmissionState_t *frontend,
        const ralAtmospherePlanReceipt_t *plan, bool historyValid, bool historyParity,
        wiredMetalUpload &upload) {
    renderAtmosphereSnapshot_t snapshot;
    renderAtmosphereMediaSnapshot_t media;
    renderWorldSnapshot_t view;
    const atmosphereEmitter_t *emitters;
    const atmosphereMediaVolume_t *volumes;
    const renderPolyCommand_t *polys; const polyVert_t *vertices;
    const renderLightCommand_t *lights;
    uint32_t polyCount, vertexCount, lightCount;
    if (!s || !command || !frontend || !plan || plan->selectedTier != RAL_ATMOSPHERE_TIER_FULL
            || !plan->froxelWidth || !plan->froxelHeight || plan->froxelDepth < RAL_ATMOSPHERE_MIN_SLICES
            || plan->froxelWidth > WIRED_METAL_ATMOSPHERE_CAPACITY / RAL_ATMOSPHERE_MIN_SLICES
            || plan->froxelHeight > WIRED_METAL_ATMOSPHERE_CAPACITY / RAL_ATMOSPHERE_MIN_SLICES
            || plan->froxelDepth > RAL_ATMOSPHERE_MAX_SLICES
            || uint64_t(plan->froxelWidth)*plan->froxelHeight*plan->froxelDepth != plan->froxelCount
            || plan->froxelCount > WIRED_METAL_ATMOSPHERE_CAPACITY
            || plan->admittedVolumeCount > RAL_ATMOSPHERE_MAX_VOLUMES
            || plan->admittedLightCount > RAL_ATMOSPHERE_MAX_LIGHTS
            || !RenderSubmission_AtmosphereSnapshot(frontend, &snapshot, &emitters)
            || !RenderSubmission_AtmosphereMediaSnapshot(frontend, &media, &volumes)
            || plan->admittedVolumeCount > media.count
            || !RenderSubmission_ViewSnapshot(frontend, &view)
            || !RenderSubmission_EffectSnapshots(frontend, &polys, &polyCount,
                &vertices, &vertexCount, &lights, &lightCount)) return false;
    const auto &state = snapshot.state;
    const auto &sky = plan->skyEnvironment;
    const float farZ = RalMetal_AtmosphereFarDistance(&state);
    wiredAtmosphereInject inject = {};
    wiredAtmosphereLight light = {};
    wiredAtmosphereCloud cloud = {};
    wiredAtmosphereIntegrate integrate = {};
    wiredAtmosphereTile tile = {};
    if (!wired_atmosphere_inverse(view, farZ, inject.inverse, tile.inverse)) return false;
    memcpy(light.inverse, inject.inverse, sizeof(inject.inverse));
    memcpy(cloud.inverse, inject.inverse, sizeof(inject.inverse));
    memcpy(inject.eyeNear, view.viewOrigin, 3*sizeof(float)); inject.eyeNear[3]=4;
    memcpy(light.eyeNear, inject.eyeNear, sizeof(inject.eyeNear));
    memcpy(cloud.eyeNear, inject.eyeNear, sizeof(inject.eyeNear));
    inject.farDensity[0]=light.farPad[0]=farZ;
    inject.farDensity[1]=state.mediaDensity; inject.farDensity[2]=state.mediaHeightFalloff;
    const float albedo[4]={.72f,.82f,.92f,.2f}; memcpy(inject.albedo,albedo,sizeof(albedo));
    const uint32_t grid[4]={plan->froxelWidth,plan->froxelHeight,plan->froxelDepth,0};
    memcpy(inject.grid,grid,sizeof(grid)); inject.grid[3]=plan->admittedVolumeCount;
    memcpy(light.grid,grid,sizeof(grid)); light.grid[3]=plan->admittedLightCount;
    memcpy(cloud.grid,grid,sizeof(grid)); memcpy(integrate.grid,grid,sizeof(grid));
    inject.timeline[0]=state.timelineSeconds; inject.timeline[1]=(float)state.seed;
    inject.timeline[2]=1; inject.timeline[3]=(float)(state.seed ^ 0x9e3779b9u);
    memcpy(light.sunDirection,sky.sunDirection,3*sizeof(float));
    memcpy(light.moonDirection,sky.moonDirection,3*sizeof(float));
    /* RGB * unit intensity equals the canonical normalized RGB * magnitude. */
    light.sunDirection[3]=light.moonDirection[3]=1;
    memcpy(light.sunColor,sky.sunRadiance,3*sizeof(float)); light.sunColor[3]=sky.cloudSunOcclusion;
    memcpy(light.moonColor,sky.moonRadiance,3*sizeof(float)); light.moonColor[3]=state.lightning;
    memcpy(light.ambient,sky.ambientSh,3*sizeof(float)); light.ambient[3]=state.cloudCover;
    cloud.farTimeCoverShadow[0]=farZ; cloud.farTimeCoverShadow[1]=state.timelineSeconds;
    cloud.farTimeCoverShadow[2]=state.cloudCover; cloud.farTimeCoverShadow[3]=sky.cloudSunOcclusion;
    memcpy(cloud.windDensity,state.wind,3*sizeof(float)); cloud.windDensity[3]=.012f;
    const float span=state.bounds[5]-state.bounds[2];
    cloud.layer[0]=span>=512 ? state.bounds[2]+span*.65f : view.viewOrigin[2]+96;
    cloud.layer[1]=span>=512 ? state.bounds[2]+span*.90f : view.viewOrigin[2]+384;
    cloud.layer[2]=.18f; cloud.layer[3]=.22f;
    memcpy(cloud.sunDirection,light.sunDirection,sizeof(light.sunDirection));
    memcpy(cloud.sunColor,sky.sunRadiance,3*sizeof(float)); cloud.sunColor[3]=state.lightning;
    memcpy(cloud.ambient,sky.ambientSh,3*sizeof(float));
    integrate.grid[3]=historyValid && plan->temporalReuseCount ? 1u : 0u;
    integrate.depthHistory[0]=4; integrate.depthHistory[1]=farZ;
    integrate.depthHistory[2]=.9f; integrate.depthHistory[3]=integrate.grid[3] ? 0 : 1;
    // Match the producer's uniform froxel UV grid, including partial edge tiles.
    tile.screen[0]=(float)(plan->froxelWidth*16u); tile.screen[1]=(float)(plan->froxelHeight*16u);
    tile.screen[2]=(float)plan->froxelWidth; tile.screen[3]=(float)plan->froxelHeight;
    tile.lightsDepth[0]=(float)plan->admittedLightCount;
    tile.lightsDepth[1]=0; tile.lightsDepth[2]=4; tile.lightsDepth[3]=farZ;

    // One small immutable packet; argument blocks cannot alias between stages
    // or overwrite a previous in-flight frame, even on a discarded attempt.
    NSUInteger offsets[WIRED_ATM_KERNELS], bytes=5u*256u;
    for (unsigned i=0;i<WIRED_ATM_KERNELS;++i) {
        offsets[i]=bytes; bytes+=wired_atmosphere_aligned(s->arguments[i].encodedLength);
    }
    const NSUInteger volumeOffset=bytes;
    bytes+=wired_atmosphere_aligned(sizeof(wiredAtmosphereVolume)*
        (plan->admittedVolumeCount ? plan->admittedVolumeCount : 1u));
    const NSUInteger lightOffset=bytes;
    bytes+=wired_atmosphere_aligned(sizeof(wiredAtmosphereLocalLight)*
        (plan->admittedLightCount ? plan->admittedLightCount : 1u));
    const NSUInteger depthOffset=bytes; bytes+=256u;
    if (!upload.wired_reserve(s->device, bytes, MTLResourceStorageModeShared)) return false;
    auto packet=(unsigned char *)upload.buffer.contents;
    memset(packet,0,bytes);
    memcpy(packet,&inject,sizeof(inject)); memcpy(packet+256,&light,sizeof(light));
    memcpy(packet+512,&cloud,sizeof(cloud)); memcpy(packet+768,&integrate,sizeof(integrate));
    memcpy(packet+1024,&tile,sizeof(tile));
    auto packedVolumes=(wiredAtmosphereVolume *)(packet+volumeOffset);
    for (uint32_t i=0;i<plan->admittedVolumeCount;++i) {
        const auto &v=volumes[i]; auto &p=packedVolumes[i];
        memcpy(p.origin,v.origin,3*sizeof(float)); p.origin[3]=v.radius;
        memcpy(p.extent,v.extent,3*sizeof(float)); p.extent[3]=(float)v.shape;
        memcpy(p.albedo,v.albedo,3*sizeof(float)); p.albedo[3]=v.anisotropy;
        memcpy(p.emission,v.emissive,3*sizeof(float)); p.emission[3]=v.emissionIntensity;
        p.media[0]=v.extinction; p.media[1]=v.heightFalloff; p.media[2]=v.noiseScale; p.media[3]=(float)v.flags;
    }
    auto packedLights=(wiredAtmosphereLocalLight *)(packet+lightOffset);
    uint32_t admitted=0;
    for (uint32_t i=0;i<lightCount && admitted<plan->admittedLightCount;++i) {
        const auto &v=lights[i];
        if (!(v.sourceFlags & RAL_LIGHT_INJECT_ATMOSPHERE)) continue;
        auto &p=packedLights[admitted++];
        memcpy(p.origin,v.origin,3*sizeof(float)); p.origin[3]=v.intensity;
        memcpy(p.color,v.color,3*sizeof(float));
        p.color[3]=v.intensity>0 ? 1/(v.intensity*v.intensity) : 0;
        memcpy(p.endpoint,v.end,3*sizeof(float)); p.endpoint[3]=v.hasEnd ? 1 : 0;
    }
    if (admitted != plan->admittedLightCount) return false;
    const NSUInteger section=WIRED_METAL_ATMOSPHERE_SECTION_BYTES;
    for (unsigned i=0;i<WIRED_ATM_KERNELS;++i) {
        auto args=s->arguments[i]; [args setArgumentBuffer:upload.buffer offset:offsets[i]];
        if (i!=WIRED_ATM_TILE) [args setBuffer:upload.buffer offset:i*256u atIndex:0];
        if (i==WIRED_ATM_INJECT) {
            [args setBuffer:upload.buffer offset:volumeOffset atIndex:1];
            [args setBuffer:s->arena offset:0 atIndex:2];
        } else if (i==WIRED_ATM_LIGHT) {
            [args setBuffer:s->arena offset:0 atIndex:1];
            [args setBuffer:s->tiles offset:0 atIndex:2];
            [args setBuffer:upload.buffer offset:lightOffset atIndex:3];
            [args setBuffer:s->arena offset:section atIndex:4];
        } else if (i==WIRED_ATM_CLOUD) {
            [args setBuffer:s->arena offset:section atIndex:1];
            [args setBuffer:s->arena offset:2*section atIndex:2];
        } else if (i==WIRED_ATM_INTEGRATE) {
            [args setBuffer:s->arena offset:(plan->cloudsActive ? 2u : 1u)*section atIndex:1];
            [args setBuffer:s->arena offset:(historyParity ? 5u : 4u)*section atIndex:2];
            [args setBuffer:s->arena offset:3*section atIndex:3];
            [args setBuffer:s->arena offset:(historyParity ? 4u : 5u)*section atIndex:4];
        } else {
            [args setBuffer:upload.buffer offset:depthOffset atIndex:0];
            [args setBuffer:upload.buffer offset:lightOffset atIndex:1];
            [args setBuffer:s->tiles offset:0 atIndex:2];
        }
    }
    id<MTLComputeCommandEncoder> encoder=[command computeCommandEncoder];
    if (!encoder) return false;
    encoder.label=@"wired.atmosphere.canonical";
    [encoder useResource:upload.buffer usage:MTLResourceUsageRead];
    [encoder useResource:s->arena usage:MTLResourceUsageRead|MTLResourceUsageWrite];
    [encoder useResource:s->tiles usage:MTLResourceUsageRead|MTLResourceUsageWrite];
    const unsigned order[]={WIRED_ATM_TILE,WIRED_ATM_INJECT,WIRED_ATM_LIGHT,WIRED_ATM_CLOUD,WIRED_ATM_INTEGRATE};
    for (unsigned i:order) {
        if (i==WIRED_ATM_CLOUD && !plan->cloudsActive) continue;
        [encoder setComputePipelineState:s->pipeline[i]];
        [encoder setBuffer:upload.buffer offset:offsets[i] atIndex:0];
        const bool column=i==WIRED_ATM_TILE || i==WIRED_ATM_INTEGRATE;
        if (i==WIRED_ATM_TILE) [encoder setBuffer:upload.buffer offset:1024 atIndex:1];
        [encoder dispatchThreads:MTLSizeMake(plan->froxelWidth,plan->froxelHeight,column ? 1u : plan->froxelDepth)
            threadsPerThreadgroup:column ? MTLSizeMake(8,8,1) : MTLSizeMake(4,4,4)];
        if (i!=WIRED_ATM_INTEGRATE) [encoder memoryBarrierWithScope:MTLBarrierScopeBuffers];
    }
    [encoder endEncoding];
    return true;
}
