// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
// Test-only entry points: real RAL planner, frontend packets and GPU executor.
#include "ral_webgpu_atmosphere.h"
#include "ral_webgpu_browser_emscripten.h"
#include <emscripten.h>
#include <math.h>

static renderSubmissionState_t frontend;
static ralWebGpuBrowserModuleBorrow_t borrow;
static ralWebGpuAtmosphere_t *executor;
static ralWebGpuAtmosphereReceipt_t receipt;
static uint64_t generation=10000;
static qboolean pending;
#define REQUIRE(x) do { if(!(x)) return -__LINE__; } while(0)

EMSCRIPTEN_KEEPALIVE int wired_atmosphere_probe_begin(void) {
    REQUIRE(!executor && RalWebGpu_BrowserModuleBorrow(&borrow));
    REQUIRE(RenderSubmission_Init(&frontend,1));
    REQUIRE(RalWebGpu_AtmosphereCreate(borrow.runtime,&borrow.runtimeReceipt,
        borrow.bridge,++generation,&executor));
    return 1;
}
EMSCRIPTEN_KEEPALIVE int wired_atmosphere_probe_frame(int camera,int mode,int frame) {
    REQUIRE(executor && !pending && camera>=0 && camera<2 && mode>=0 && mode<8 && frame>=0 && frame<6);
    atmosphereFrameState_t state={0}; refdef_t view={0}; atmosphereMediaVolume_t volume={0};
    ralAtmospherePlanRequest_t request={0}; ralAtmospherePlanReceipt_t plan;
    if(frame==0 || frame==2) RalWebGpu_AtmosphereInvalidateHistory(executor);
    state.schemaVersion=WIRED_ATMOSPHERE_SCHEMA_VERSION;
    state.flags=ATMOSPHERE_FLAG_ENABLED|ATMOSPHERE_FLAG_VOLUMETRIC_MEDIA|ATMOSPHERE_FLAG_SKY_LIGHTING;
    state.qualityTier=ATMOSPHERE_QUALITY_FULL; state.seed=216;
    state.bounds[0]=state.bounds[1]=state.bounds[2]=-128;
    state.bounds[3]=state.bounds[4]=state.bounds[5]=128;
    state.visibility=512; state.mediaDensity=.001f+.0004f*frame;
    state.mediaHeightFalloff=mode?.003f:0; state.indoorExposure=1;
    state.cloudCover=mode==3?1:0; state.timelineSeconds=1; state.wind[0]=2;
    REQUIRE(RenderSubmission_SetAtmosphere(&frontend,&state));
    REQUIRE(RenderSubmission_BeginFrame(&frontend,++generation));
    // Tiny offscreen compute grid; the harness window remains 1280x720.
    view.width=80; view.height=48; view.fov_x=90; view.fov_y=70; view.rdflags=RDF_NOWORLDMODEL;
    const float angle=camera?.65f:0;
    view.viewaxis[0][0]=cosf(angle); view.viewaxis[0][2]=sinf(angle);
    view.viewaxis[1][1]=1; view.viewaxis[2][0]=-sinf(angle); view.viewaxis[2][2]=cosf(angle);
    view.vieworg[0]=camera?45:0; view.vieworg[1]=camera?-31:0; view.vieworg[2]=camera?23:0;
    volume.schemaVersion=WIRED_ATMOSPHERE_MEDIA_VOLUME_SCHEMA_VERSION; volume.id=1;
    volume.shape=mode==2?ATMOSPHERE_MEDIA_VOLUME_BOX:ATMOSPHERE_MEDIA_VOLUME_SPHERE;
    for(unsigned c=0;c<3;++c) {
        volume.origin[c]=view.vieworg[c]+view.viewaxis[0][c]*40+view.viewaxis[2][c]*12;
        volume.extent[c]=mode==2?16:0; volume.albedo[c]=.2f+.2f*c;
    }
    volume.radius=28; volume.extinction=.007f; volume.heightFalloff=.015f;
    volume.emissive[0]=.02f; volume.emissionIntensity=.2f;
    if(mode==1 || mode==2) REQUIRE(RenderSubmission_AddAtmosphereMediaVolume(&frontend,&volume));
    float lightStart[3],lightEnd[3];
    for(unsigned c=0;c<3;++c) {
        lightStart[c]=volume.origin[c];
        lightEnd[c]=lightStart[c]+(mode==4||mode==5
            ? view.viewaxis[0][c]*80+view.viewaxis[1][c]*32
            : mode==7 ? view.viewaxis[0][c]*.0001f : 0);
        if(mode==5) { float swap=lightStart[c];lightStart[c]=lightEnd[c];lightEnd[c]=swap; }
    }
    if(mode) REQUIRE(RenderSubmission_AddLight(&frontend,lightStart,mode>=4?lightEnd:NULL,30,1,.3f,.05f));
    REQUIRE(RenderSubmission_RenderScene(&frontend,&view,0));
    renderSubmissionReceipt_t frontendReceipt;
    REQUIRE(RenderSubmission_EndFrame(&frontend,generation,&frontendReceipt));
    request.schemaVersion=RAL_ATMOSPHERE_PLAN_SCHEMA_VERSION; request.backendType=RAL_BACKEND_WEBGPU;
    request.frameGeneration=generation; request.width=80; request.height=48;
    request.requestedTier=RAL_ATMOSPHERE_TIER_FULL; request.maxFroxelCount=240;
    request.localVolumeCount=(mode==1 || mode==2)?1:0; request.lightCount=mode?1:0;
    request.maxLocalVolumes=RAL_ATMOSPHERE_MAX_VOLUMES; request.maxLights=RAL_ATMOSPHERE_MAX_LIGHTS;
    request.mediaActive=qtrue; request.skyLightingActive=qtrue; request.cloudsRequested=mode==3;
    request.historyValid=RalWebGpu_AtmosphereHistoryValid(executor);
    request.capabilities.analyticComposite=qtrue; request.capabilities.compute=qtrue;
    request.capabilities.storageBuffers=qtrue; request.capabilities.temporalHistory=qtrue;
    request.capabilities.fullClouds=qtrue;
    REQUIRE(Ral_AtmospherePlan(&request,&plan));
    REQUIRE(plan.froxelCount==240 && plan.froxelDepth==16 && plan.selectedTier==RAL_ATMOSPHERE_TIER_FULL);
    // Fixed lighting fixture, as in the Metal optical oracle; no sky LUT test.
    plan.skyEnvironment.sunDirection[2]=1; plan.skyEnvironment.cloudSunOcclusion=.4f;
    for(unsigned c=0;c<3;++c) {
        plan.skyEnvironment.ambientSh[c]=.2f+.2f*c;
        plan.skyEnvironment.sunRadiance[c]=1.0f/(c+1);
        plan.skyEnvironment.moonRadiance[c]=.05f*c;
    }
    REQUIRE(RalWebGpu_AtmosphereBegin(executor,&borrow.runtimeReceipt,&frontend,&plan,&receipt));
    pending=qtrue;
    if(frame==3) { // Must survive completion of this already submitted work.
        RalWebGpu_AtmosphereInvalidateHistory(executor);
        RalWebGpu_AtmosphereInvalidateHistory(executor); // idempotent
    }
    return 1;
}
EMSCRIPTEN_KEEPALIVE int wired_atmosphere_probe_poll(void) {
    REQUIRE(executor && pending);
    const int status=RalWebGpu_AtmospherePoll(executor,&receipt);
    if(status==RAL_WEBGPU_ASYNC_READY) pending=qfalse;
    return status;
}
EMSCRIPTEN_KEEPALIVE unsigned wired_atmosphere_probe_query(int query) {
    if(!executor) return 0;
    if(query==0) return (unsigned)RalWebGpu_AtmosphereArenaIdentity(executor);
    if(query==1) return RalWebGpu_AtmosphereHistoryValid(executor);
    if(query==2) return receipt.historyReused;
    return receipt.nativeDispatchCount;
}
EMSCRIPTEN_KEEPALIVE int wired_atmosphere_probe_end(void) {
    REQUIRE(!pending);
    RalWebGpu_AtmosphereDestroy(executor,&borrow.runtimeReceipt); executor=NULL;
    return 1;
}
