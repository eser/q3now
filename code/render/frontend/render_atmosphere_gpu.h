// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
#ifndef WIRED_RENDER_ATMOSPHERE_GPU_H
#define WIRED_RENDER_ATMOSPHERE_GPU_H

#include "render_submission.h"
#include "render_submission_world.h"
#include "../ral/core/ral_atmosphere.h"
#include <math.h>
#include <string.h>

/* Canonical inject/light/cloud/integrate/tile shader packets. Native resource
 * allocation and completion remain owned by each adapter. */
typedef struct { float inverse[16], eyeNear[4], farDensity[4], albedo[4];
    uint32_t grid[4]; float timeline[4]; } renderAtmosphereInjectGpu_t;
typedef struct { float inverse[16], eyeNear[4], farPad[4]; uint32_t grid[4];
    float sunDirection[4], sunColor[4], moonDirection[4], moonColor[4], ambient[4];
} renderAtmosphereLightGpu_t;
typedef struct { float inverse[16], eyeNear[4], farTimeCoverShadow[4], windDensity[4], layer[4];
    float sunDirection[4], sunColor[4], ambient[4]; uint32_t grid[4];
} renderAtmosphereCloudGpu_t;
typedef struct { uint32_t grid[4]; float depthHistory[4]; } renderAtmosphereIntegrateGpu_t;
typedef struct { float inverse[16], screen[4], lightsDepth[4]; } renderAtmosphereTileGpu_t;
typedef struct { float origin[4], extent[4], albedo[4], emission[4], media[4]; } renderAtmosphereVolumeGpu_t;
typedef struct { float origin[4], color[4], endpoint[4]; } renderAtmosphereLocalLightGpu_t;
typedef struct {
    renderAtmosphereInjectGpu_t inject;
    renderAtmosphereLightGpu_t light;
    renderAtmosphereCloudGpu_t cloud;
    renderAtmosphereIntegrateGpu_t integrate;
    renderAtmosphereTileGpu_t tile;
    renderAtmosphereVolumeGpu_t volumes[RAL_ATMOSPHERE_MAX_VOLUMES];
    renderAtmosphereLocalLightGpu_t lights[RAL_ATMOSPHERE_MAX_LIGHTS];
} renderAtmosphereGpuFrame_t;

static inline float RenderAtmosphereGpu_FarDistance(const atmosphereFrameState_t *state) {
    return wired_atmosphere_grid_far(state ? state->visibility : 0.0f);
}

static inline qboolean RenderAtmosphereGpu_Build(const renderSubmissionState_t *frontend,
        const ralAtmospherePlanReceipt_t *plan, uint32_t capacity, qboolean historyValid,
        renderAtmosphereGpuFrame_t *out) {
    renderAtmosphereSnapshot_t snapshot;
    renderAtmosphereMediaSnapshot_t media;
    renderWorldSnapshot_t view;
    const atmosphereEmitter_t *emitters;
    const atmosphereMediaVolume_t *volumes;
    const renderPolyCommand_t *polys; const polyVert_t *vertices;
    const renderLightCommand_t *lights;
    uint32_t polyCount, vertexCount, lightCount;
    if (!frontend || !plan || !out || plan->selectedTier!=RAL_ATMOSPHERE_TIER_FULL
        || !plan->froxelWidth || !plan->froxelHeight
        || plan->froxelWidth>capacity/RAL_ATMOSPHERE_MIN_SLICES
        || plan->froxelHeight>capacity/RAL_ATMOSPHERE_MIN_SLICES
        || plan->froxelDepth<RAL_ATMOSPHERE_MIN_SLICES || plan->froxelDepth>RAL_ATMOSPHERE_MAX_SLICES
        || (uint64_t)plan->froxelWidth*plan->froxelHeight*plan->froxelDepth!=plan->froxelCount
        || plan->froxelCount>capacity || plan->admittedVolumeCount>RAL_ATMOSPHERE_MAX_VOLUMES
        || plan->admittedLightCount>RAL_ATMOSPHERE_MAX_LIGHTS
        || !RenderSubmission_AtmosphereSnapshot(frontend,&snapshot,&emitters)
        || !RenderSubmission_AtmosphereMediaSnapshot(frontend,&media,&volumes)
        || plan->admittedVolumeCount>media.count || !RenderSubmission_ViewSnapshot(frontend,&view)
        || !RenderSubmission_EffectSnapshots(frontend,&polys,&polyCount,&vertices,&vertexCount,&lights,&lightCount)
        || !isfinite(view.fovX) || !isfinite(view.fovY)
        || view.fovX<=1 || view.fovX>=179 || view.fovY<=1 || view.fovY>=179) return qfalse;
    const atmosphereFrameState_t *state=&snapshot.state;
    const float farZ=RenderAtmosphereGpu_FarDistance(state);
    if (!isfinite(farZ)) return qfalse;
    memset(out,0,sizeof(*out));
    const float tx=tanf(view.fovX*3.14159265358979323846f/360.0f);
    const float ty=tanf(view.fovY*3.14159265358979323846f/360.0f);
    for(unsigned j=0;j<3;++j) {
        if (!isfinite(view.viewOrigin[j])) return qfalse;
        for(unsigned k=0;k<3;++k) if(!isfinite(view.viewAxis[k][j])) return qfalse;
        out->inject.inverse[j]=out->tile.inverse[j]=-view.viewAxis[1][j]*tx;
        out->inject.inverse[4+j]=out->tile.inverse[4+j]=-view.viewAxis[2][j]*ty;
        out->inject.inverse[8+j]=-view.viewOrigin[j]/4;
        out->inject.inverse[12+j]=view.viewAxis[0][j]+view.viewOrigin[j]/4;
        out->tile.inverse[8+j]=view.viewOrigin[j]*(.25f-1/farZ);
        out->tile.inverse[12+j]=view.viewAxis[0][j]+view.viewOrigin[j]/farZ;
    }
    out->inject.inverse[11]=-.25f;out->inject.inverse[15]=.25f;
    out->tile.inverse[11]=.25f-1/farZ;out->tile.inverse[15]=1/farZ;
    memcpy(out->light.inverse,out->inject.inverse,sizeof(out->inject.inverse));
    memcpy(out->cloud.inverse,out->inject.inverse,sizeof(out->inject.inverse));
    memcpy(out->inject.eyeNear,view.viewOrigin,3*sizeof(float));out->inject.eyeNear[3]=4;
    memcpy(out->light.eyeNear,out->inject.eyeNear,sizeof(out->inject.eyeNear));
    memcpy(out->cloud.eyeNear,out->inject.eyeNear,sizeof(out->inject.eyeNear));
    out->inject.farDensity[0]=out->light.farPad[0]=farZ;
    out->inject.farDensity[1]=state->mediaDensity;out->inject.farDensity[2]=state->mediaHeightFalloff;
    const float albedo[4]={.72f,.82f,.92f,.2f};memcpy(out->inject.albedo,albedo,sizeof(albedo));
    const uint32_t grid[4]={plan->froxelWidth,plan->froxelHeight,plan->froxelDepth,0};
    memcpy(out->inject.grid,grid,sizeof(grid));out->inject.grid[3]=plan->admittedVolumeCount;
    memcpy(out->light.grid,grid,sizeof(grid));out->light.grid[3]=plan->admittedLightCount;
    memcpy(out->cloud.grid,grid,sizeof(grid));memcpy(out->integrate.grid,grid,sizeof(grid));
    out->inject.timeline[0]=state->timelineSeconds;out->inject.timeline[1]=(float)state->seed;
    out->inject.timeline[2]=1;out->inject.timeline[3]=(float)(state->seed^0x9e3779b9u);
    memcpy(out->light.sunDirection,plan->skyEnvironment.sunDirection,3*sizeof(float));
    memcpy(out->light.moonDirection,plan->skyEnvironment.moonDirection,3*sizeof(float));
    out->light.sunDirection[3]=out->light.moonDirection[3]=1;
    memcpy(out->light.sunColor,plan->skyEnvironment.sunRadiance,3*sizeof(float));
    out->light.sunColor[3]=plan->skyEnvironment.cloudSunOcclusion;
    memcpy(out->light.moonColor,plan->skyEnvironment.moonRadiance,3*sizeof(float));
    out->light.moonColor[3]=state->lightning;
    memcpy(out->light.ambient,plan->skyEnvironment.ambientSh,3*sizeof(float));out->light.ambient[3]=state->cloudCover;
    out->cloud.farTimeCoverShadow[0]=farZ;out->cloud.farTimeCoverShadow[1]=state->timelineSeconds;
    out->cloud.farTimeCoverShadow[2]=state->cloudCover;out->cloud.farTimeCoverShadow[3]=plan->skyEnvironment.cloudSunOcclusion;
    memcpy(out->cloud.windDensity,state->wind,3*sizeof(float));out->cloud.windDensity[3]=.012f;
    const float span=state->bounds[5]-state->bounds[2];
    out->cloud.layer[0]=span>=512 ? state->bounds[2]+span*.65f : view.viewOrigin[2]+96;
    out->cloud.layer[1]=span>=512 ? state->bounds[2]+span*.90f : view.viewOrigin[2]+384;
    out->cloud.layer[2]=.18f;out->cloud.layer[3]=.22f;
    memcpy(out->cloud.sunDirection,out->light.sunDirection,sizeof(out->light.sunDirection));
    memcpy(out->cloud.sunColor,plan->skyEnvironment.sunRadiance,3*sizeof(float));out->cloud.sunColor[3]=state->lightning;
    memcpy(out->cloud.ambient,plan->skyEnvironment.ambientSh,3*sizeof(float));
    out->integrate.grid[3]=historyValid && plan->temporalReuseCount ? 1u : 0u;
    out->integrate.depthHistory[0]=4;out->integrate.depthHistory[1]=farZ;
    out->integrate.depthHistory[2]=.9f;out->integrate.depthHistory[3]=out->integrate.grid[3] ? 0 : 1;
    out->tile.screen[0]=(float)(plan->froxelWidth*16u);out->tile.screen[1]=(float)(plan->froxelHeight*16u);
    out->tile.screen[2]=(float)plan->froxelWidth;out->tile.screen[3]=(float)plan->froxelHeight;
    out->tile.lightsDepth[0]=(float)plan->admittedLightCount;out->tile.lightsDepth[2]=4;out->tile.lightsDepth[3]=farZ;
    for(uint32_t i=0;i<plan->admittedVolumeCount;++i) {
        const atmosphereMediaVolume_t *v=&volumes[i];renderAtmosphereVolumeGpu_t *p=&out->volumes[i];
        memcpy(p->origin,v->origin,3*sizeof(float));p->origin[3]=v->radius;
        memcpy(p->extent,v->extent,3*sizeof(float));p->extent[3]=(float)v->shape;
        memcpy(p->albedo,v->albedo,3*sizeof(float));p->albedo[3]=v->anisotropy;
        memcpy(p->emission,v->emissive,3*sizeof(float));p->emission[3]=v->emissionIntensity;
        p->media[0]=v->extinction;p->media[1]=v->heightFalloff;p->media[2]=v->noiseScale;p->media[3]=(float)v->flags;
    }
    uint32_t admitted=0;
    for(uint32_t i=0;i<lightCount && admitted<plan->admittedLightCount;++i) {
        const renderLightCommand_t *v=&lights[i];
        if (!(v->sourceFlags&RAL_LIGHT_INJECT_ATMOSPHERE)) continue;
        renderAtmosphereLocalLightGpu_t *p=&out->lights[admitted++];
        memcpy(p->origin,v->origin,3*sizeof(float));p->origin[3]=v->intensity;
        memcpy(p->color,v->color,3*sizeof(float));p->color[3]=v->intensity>0 ? 1/(v->intensity*v->intensity) : 0;
        memcpy(p->endpoint,v->end,3*sizeof(float));p->endpoint[3]=v->hasEnd ? 1 : 0;
    }
    return admitted==plan->admittedLightCount ? qtrue : qfalse;
}
#endif
