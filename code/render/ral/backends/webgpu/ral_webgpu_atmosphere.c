// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors
#include "ral_webgpu_atmosphere.h"
#include "render_atmosphere_gpu.h"
#include "ral_webgpu_atmosphere_inject.h"
#include "ral_webgpu_atmosphere_light.h"
#include "ral_webgpu_atmosphere_cloud.h"
#include "ral_webgpu_atmosphere_integrate.h"
#include "ral_webgpu_atmosphere_tile.h"
#include <stdlib.h>
#include <string.h>

enum { INJECT, LIGHT, CLOUD, INTEGRATE, TILE, PASS_COUNT };
enum { MEDIA, LIT, CLOUD_MEDIA, INTEGRATED, HISTORY_A, HISTORY_B,
    VOLUMES, LIGHTS, TILES, DEPTH, PARAMS, BUFFER_COUNT=PARAMS+PASS_COUNT };
#define SECTION_BYTES ((uint64_t)RAL_WEBGPU_ATMOSPHERE_FROXEL_CAPACITY*16u)
#define TILE_BYTES ((uint64_t)RAL_WEBGPU_ATMOSPHERE_FROXEL_CAPACITY/RAL_ATMOSPHERE_MIN_SLICES*33u*4u)
_Static_assert(sizeof(renderAtmosphereInjectGpu_t)==144,"inject ABI");
_Static_assert(sizeof(renderAtmosphereLightGpu_t)==192,"light ABI");
_Static_assert(sizeof(renderAtmosphereCloudGpu_t)==192,"cloud ABI");
_Static_assert(sizeof(renderAtmosphereIntegrateGpu_t)==32,"integrate ABI");
_Static_assert(sizeof(renderAtmosphereTileGpu_t)==96,"tile ABI");
_Static_assert(sizeof(renderAtmosphereVolumeGpu_t)==80,"volume ABI");
_Static_assert(sizeof(renderAtmosphereLocalLightGpu_t)==48,"local light ABI");
struct ralWebGpuAtmosphere_s {
    ralWebGpuRuntime_t *runtime;
    ralWebGpuCommand_t *command;
    ralWebGpuBrowserBridge_t *bridge;
    uint64_t generation;
    ralWebGpuPipeline_t *pipelines[PASS_COUNT];
    ralWebGpuPipelineReceipt_t pipelineReceipts[PASS_COUNT];
    ralWebGpuResource_t *buffers[BUFFER_COUNT];
    ralWebGpuResourceReceipt_t receipts[BUFFER_COUNT];
    /* Four invariant groups, four integrate cloud/history combinations. */
    uintptr_t groups[8];
    ralWebGpuAtmosphereReceipt_t inFlight;
    qboolean active, historyValid, historyParity, discardInFlightHistory;
};
static const uint32_t paramSizes[PASS_COUNT]={144,192,192,32,96};
static const uint32_t bindingCounts[PASS_COUNT]={3,5,3,5,4};
static const uint32_t bufferBindings[PASS_COUNT][5]={
    {PARAMS+INJECT,VOLUMES,MEDIA},
    {PARAMS+LIGHT,MEDIA,TILES,LIGHTS,LIT},
    {PARAMS+CLOUD,LIT,CLOUD_MEDIA},
    {PARAMS+INTEGRATE,LIT,HISTORY_A,INTEGRATED,HISTORY_B},
    {LIGHTS,DEPTH,TILES,PARAMS+TILE}};

static uint64_t BufferSize(unsigned index) {
    if(index<=HISTORY_B) return SECTION_BYTES;
    if(index==VOLUMES) return sizeof(renderAtmosphereVolumeGpu_t)*RAL_ATMOSPHERE_MAX_VOLUMES;
    if(index==LIGHTS) return sizeof(renderAtmosphereLocalLightGpu_t)*RAL_ATMOSPHERE_MAX_LIGHTS;
    if(index==TILES) return TILE_BYTES;
    if(index==DEPTH) return 4u;
    return paramSizes[index-PARAMS];
}
static qboolean Writable(unsigned stage,unsigned binding) {
    return ((stage==INJECT || stage==CLOUD || stage==TILE) && binding==2)
        || (stage==LIGHT && binding==4) || (stage==INTEGRATE && binding>=3);
}
static qboolean ReceiptValid(const ralWebGpuAtmosphereReceipt_t *r) {
    return r && r->schemaVersion==RAL_WEBGPU_ATMOSPHERE_SCHEMA_VERSION
        && r->backendType==RAL_BACKEND_WEBGPU && r->frameGeneration && r->executorGeneration
        && Ral_AtmospherePlanReceiptExact(&r->plan,&r->plan)
        && r->plan.selectedTier==RAL_ATMOSPHERE_TIER_FULL
        && RalWebGpu_CommandReceiptExact(&r->command,&r->command)
        && RalWebGpu_SubmissionReceiptExact(&r->submission,&r->submission)
        && r->dispatchCount==r->plan.computeDispatchCount
        && r->nativeDispatchCount==r->dispatchCount+1u
        && r->command.dispatchCount==r->nativeDispatchCount
        && r->froxelCount==r->plan.froxelCount && r->ready==qtrue;
}
qboolean RalWebGpu_AtmosphereReceiptExact(const ralWebGpuAtmosphereReceipt_t *a,
        const ralWebGpuAtmosphereReceipt_t *b) {
    return ReceiptValid(a)&&ReceiptValid(b)&&!memcmp(a,b,sizeof(*a));
}
static qboolean CreatePipeline(ralWebGpuAtmosphere_t *a,ralWebGpuRuntimeReceipt_t *rr,unsigned stage) {
    const unsigned char *sources[]={ral_webgpu_atmosphere_inject_wgsl,ral_webgpu_atmosphere_light_wgsl,
        ral_webgpu_atmosphere_cloud_wgsl,ral_webgpu_atmosphere_integrate_wgsl,ral_webgpu_atmosphere_tile_wgsl};
    const uint32_t sizes[]={sizeof(ral_webgpu_atmosphere_inject_wgsl),sizeof(ral_webgpu_atmosphere_light_wgsl),
        sizeof(ral_webgpu_atmosphere_cloud_wgsl),sizeof(ral_webgpu_atmosphere_integrate_wgsl),sizeof(ral_webgpu_atmosphere_tile_wgsl)};
    const uint32_t spirv[4]={0x07230203u};
    ralShaderModuleAbi_t module={0};ralShaderBindingAbi_t bindings[5]={0};
    ralShaderAbiManifest_t manifest={0};ralShaderVariantAbi_t variant={0};
    ralComputePipelineCreateInfo_t compute={0};ralWebGpuPipelineCreateInfo_t info={0};
    const ralBindGroupLayout_t *layouts[]={(const ralBindGroupLayout_t *)(uintptr_t)1u};
    const uint64_t generation=a->generation+stage;
    module.stage=RAL_STAGE_COMPUTE;module.entryPoint="main";
    module.sourceDigest.lane0=generation;module.sourceDigest.lane1=generation+1;
    module.artifacts[RAL_SHADER_ARTIFACT_SPIRV].target=RAL_SHADER_ARTIFACT_SPIRV;
    module.artifacts[RAL_SHADER_ARTIFACT_SPIRV].byteCount=sizeof(spirv);
    module.artifacts[RAL_SHADER_ARTIFACT_MSL].target=RAL_SHADER_ARTIFACT_MSL;
    module.artifacts[RAL_SHADER_ARTIFACT_WGSL].target=RAL_SHADER_ARTIFACT_WGSL;
    module.artifacts[RAL_SHADER_ARTIFACT_WGSL].byteCount=sizes[stage];
    if(Ral_ShaderArtifactDigest(spirv,sizeof(spirv),&module.artifacts[RAL_SHADER_ARTIFACT_SPIRV].digest)!=ralSuccess
        || Ral_ShaderArtifactDigest(sources[stage],sizes[stage],&module.artifacts[RAL_SHADER_ARTIFACT_WGSL].digest)!=ralSuccess) return qfalse;
    for(unsigned i=0;i<bindingCounts[stage];++i) {
        const unsigned buffer=bufferBindings[stage][i];
        bindings[i].binding=i;bindings[i].arrayCount=1;bindings[i].stageFlags=RAL_STAGE_COMPUTE;
        bindings[i].minBufferBindingSize=BufferSize(buffer);
        bindings[i].bindingClass=buffer>=PARAMS ? RAL_SHADER_BIND_UNIFORM_BUFFER
            : Writable(stage,i) ? RAL_SHADER_BIND_STORAGE_BUFFER_READ_WRITE : RAL_SHADER_BIND_STORAGE_BUFFER_READ;
    }
    manifest.schemaVersion=RAL_SHADER_ABI_SCHEMA_VERSION;manifest.generation=generation;
    manifest.pipelineKind=RAL_SHADER_PIPELINE_COMPUTE;manifest.modules=&module;manifest.moduleCount=1;
    manifest.bindings=bindings;manifest.bindingCount=bindingCounts[stage];
    compute.bindGroupLayouts=layouts;compute.numBindGroupLayouts=1;
    variant.artifactTarget=RAL_SHADER_ARTIFACT_WGSL;variant.generation=generation;
    if(Ral_ComputePipelineSemanticDigest(&compute,&variant.semanticStateDigest)!=ralSuccess) return qfalse;
    const ralWebGpuWgslModule_t wgsl={(const char *)sources[stage],sizes[stage]};
    info.manifest=&manifest;info.variant=&variant;info.modules=&wgsl;info.moduleCount=1;
    info.computeState=&compute;info.generation=generation;
    return RalWebGpu_BrowserBridgeApplyPipelineInfo(a->bridge,&info)
        && RalWebGpu_RuntimeCreatePipeline(a->runtime,rr,&info,&a->pipelines[stage],&a->pipelineReceipts[stage])
        && RalWebGpu_RuntimeGetReceipt(a->runtime,rr);
}
static qboolean CreateGroup(ralWebGpuAtmosphere_t *a,unsigned group,unsigned stage,
        qboolean cloud,qboolean parity) {
    ralWebGpuBrowserBindResource_t entries[5]={0};
    for(unsigned i=0;i<bindingCounts[stage];++i) {
        unsigned buffer=bufferBindings[stage][i];
        if(stage==INTEGRATE) {
            if(i==1) buffer=cloud ? CLOUD_MEDIA : LIT;
            if(i==2) buffer=parity ? HISTORY_B : HISTORY_A;
            if(i==4) buffer=parity ? HISTORY_A : HISTORY_B;
        }
        entries[i].binding=i;entries[i].kind=RAL_WEBGPU_BROWSER_BIND_RESOURCE_BUFFER;
        entries[i].resourceIdentity=a->receipts[buffer].resourceIdentity;
        entries[i].byteSize=a->receipts[buffer].byteSize;
    }
    return RalWebGpu_BrowserBridgeCreateBindGroup(a->bridge,
        a->pipelineReceipts[stage].bindGroupLayoutIdentities[0],entries,bindingCounts[stage],&a->groups[group]);
}
static qboolean CreateResources(ralWebGpuAtmosphere_t *a,const ralWebGpuRuntimeReceipt_t *rr) {
    ralWebGpuResourceLayer_t *r=RalWebGpu_RuntimeResources(a->runtime,rr);
    if(!r) return qfalse;
    /* Distinct read/write products avoid cross-binding storage aliasing. Six
     * capacity-sized media sections remain; local-light tiles are additional. */
    for(unsigned i=0;i<BUFFER_COUNT;++i) {
        ralWebGpuBufferDesc_t desc={0};desc.size=BufferSize(i);
        desc.usage=(i>=PARAMS ? RAL_WEBGPU_BUFFER_UNIFORM : RAL_WEBGPU_BUFFER_STORAGE)
            | RAL_WEBGPU_BUFFER_COPY_DESTINATION;
        if(i==INTEGRATED) desc.usage|=RAL_WEBGPU_BUFFER_COPY_SOURCE;
        desc.memoryClass=RAL_ALLOCATION_DEVICE_LOCAL;
        if(!RalWebGpu_CreateBuffer(r,&desc,&a->buffers[i],&a->receipts[i])) return qfalse;
    }
    if(!CreateGroup(a,0,INJECT,qfalse,qfalse)||!CreateGroup(a,1,LIGHT,qfalse,qfalse)
        ||!CreateGroup(a,2,CLOUD,qfalse,qfalse)||!CreateGroup(a,3,TILE,qfalse,qfalse)) return qfalse;
    for(unsigned cloud=0;cloud<2;++cloud)for(unsigned parity=0;parity<2;++parity)
        if(!CreateGroup(a,4+cloud*2+parity,INTEGRATE,(qboolean)cloud,(qboolean)parity)) return qfalse;
    return qtrue;
}
qboolean RalWebGpu_AtmosphereCreate(ralWebGpuRuntime_t *runtime,ralWebGpuRuntimeReceipt_t *rr,
        ralWebGpuBrowserBridge_t *bridge,uint64_t generation,ralWebGpuAtmosphere_t **out) {
    if(!runtime||!rr||!bridge||!generation||generation>UINT64_MAX-PASS_COUNT||!out||*out) return qfalse;
    ralWebGpuAtmosphere_t *a=calloc(1,sizeof(*a));if(!a)return qfalse;
    a->runtime=runtime;a->bridge=bridge;a->generation=generation;
    a->command=RalWebGpu_RuntimeCommand(runtime,rr);
    if(!a->command)goto fail;
    for(unsigned i=0;i<PASS_COUNT;++i)if(!CreatePipeline(a,rr,i))goto fail;
    if(!CreateResources(a,rr))goto fail;
    *out=a;return qtrue;
fail:
    RalWebGpu_AtmosphereDestroy(a,rr);return qfalse;
}
void RalWebGpu_AtmosphereDestroy(ralWebGpuAtmosphere_t *a,const ralWebGpuRuntimeReceipt_t *rr) {
    if(!a)return;
    for(unsigned i=8;i>0;--i)if(a->groups[i-1])RalWebGpu_BrowserBridgeDestroyBindGroup(a->bridge,a->groups[i-1]);
    ralWebGpuResourceLayer_t *r=rr ? RalWebGpu_RuntimeResources(a->runtime,rr) : NULL;
    if(r)for(unsigned i=BUFFER_COUNT;i>0;--i)if(a->buffers[i-1])
        (void)RalWebGpu_DestroyResource(r,a->buffers[i-1],&a->receipts[i-1]);
    if(rr)for(unsigned i=PASS_COUNT;i>0;--i)if(a->pipelines[i-1])
        (void)RalWebGpu_RuntimeDestroyPipeline(a->runtime,a->pipelines[i-1],&a->pipelineReceipts[i-1]);
    free(a);
}
static qboolean WriteFrame(ralWebGpuAtmosphere_t *a,const ralWebGpuRuntimeReceipt_t *rr,
        const renderSubmissionState_t *frontend,const ralAtmospherePlanReceipt_t *plan) {
    renderAtmosphereGpuFrame_t frame;
    ralWebGpuResourceLayer_t *r=RalWebGpu_RuntimeResources(a->runtime,rr);
    if(!r||!RenderAtmosphereGpu_Build(frontend,plan,RAL_WEBGPU_ATMOSPHERE_FROXEL_CAPACITY,a->historyValid,&frame)) return qfalse;
    const void *params[]={&frame.inject,&frame.light,&frame.cloud,&frame.integrate,&frame.tile};
    ralWebGpuWriteReceipt_t write;
    for(unsigned i=0;i<PASS_COUNT;++i)
        if(!RalWebGpu_WriteBuffer(r,a->buffers[PARAMS+i],&a->receipts[PARAMS+i],0,params[i],paramSizes[i],&write))return qfalse;
    const uint64_t volumeBytes=sizeof(frame.volumes[0])*(plan->admittedVolumeCount ? plan->admittedVolumeCount : 1u);
    const uint64_t lightBytes=sizeof(frame.lights[0])*(plan->admittedLightCount ? plan->admittedLightCount : 1u);
    return RalWebGpu_WriteBuffer(r,a->buffers[VOLUMES],&a->receipts[VOLUMES],0,frame.volumes,volumeBytes,&write)
        && RalWebGpu_WriteBuffer(r,a->buffers[LIGHTS],&a->receipts[LIGHTS],0,frame.lights,lightBytes,&write);
}
qboolean RalWebGpu_AtmosphereBegin(ralWebGpuAtmosphere_t *a,const ralWebGpuRuntimeReceipt_t *rr,
        const renderSubmissionState_t *frontend,const ralAtmospherePlanReceipt_t *plan,
        ralWebGpuAtmosphereReceipt_t *out) {
    ralWebGpuCommandReceipt_t recording,updated,executable;
    ralWebGpuSubmissionReceipt_t submission;
    if(!a||!rr||!frontend||!plan||!out||a->active
        ||!Ral_AtmospherePlanReceiptExact(plan,plan)
        ||plan->computeDispatchCount!=(plan->cloudsActive ? 4u : 3u)
        ||!WriteFrame(a,rr,frontend,plan))return qfalse;
    if(!RalWebGpu_CommandBegin(a->command,RAL_WEBGPU_PASS_COMPUTE,0,&recording))return qfalse;
    const unsigned order[]={TILE,INJECT,LIGHT,CLOUD,INTEGRATE};
    uint32_t count=0;
    for(unsigned i=0;i<PASS_COUNT;++i) {
        const unsigned stage=order[i];
        if(stage==CLOUD&&!plan->cloudsActive)continue;
        const unsigned group=stage==INTEGRATE ? 4u+(plan->cloudsActive ? 2u : 0u)+(a->historyParity ? 1u : 0u)
            : stage==TILE ? 3u : stage;
        const qboolean column=stage==TILE||stage==INTEGRATE;
        const uint32_t size=column ? 8u : 4u;
        ralWebGpuComputeDispatch_t dispatch={0};
        dispatch.pipelineIdentity=a->pipelineReceipts[stage].pipelineIdentity;
        dispatch.bindGroupIdentities[0]=a->groups[group];dispatch.bindGroupCount=1;
        dispatch.groupCountX=(plan->froxelWidth+size-1u)/size;
        dispatch.groupCountY=(plan->froxelHeight+size-1u)/size;
        dispatch.groupCountZ=column ? 1u : (plan->froxelDepth+3u)/4u;
        dispatch.contentDigest=plan->frameGeneration^(uint64_t)(stage+1u);
        if(!dispatch.contentDigest)dispatch.contentDigest=stage+1u;
        if(!RalWebGpu_CommandRecordComputeDispatch(a->command,&recording,&dispatch,&updated)) {
            (void)RalWebGpu_CommandCancel(a->command,&recording);return qfalse;
        }
        recording=updated;++count;
    }
    if(!RalWebGpu_CommandEnd(a->command,&recording,&executable)
        ||!RalWebGpu_CommandSubmit(a->command,&executable,&submission))return qfalse;
    ralWebGpuAtmosphereReceipt_t receipt={0};
    receipt.schemaVersion=RAL_WEBGPU_ATMOSPHERE_SCHEMA_VERSION;receipt.backendType=RAL_BACKEND_WEBGPU;
    receipt.frameGeneration=plan->frameGeneration;receipt.executorGeneration=a->generation;
    receipt.plan=*plan;receipt.command=executable;receipt.submission=submission;
    receipt.dispatchCount=plan->computeDispatchCount;receipt.nativeDispatchCount=count;
    receipt.froxelCount=plan->froxelCount;
    receipt.historyReused=a->historyValid&&plan->temporalReuseCount ? qtrue : qfalse;
    receipt.ready=qtrue;
    if(!ReceiptValid(&receipt))return qfalse;
    a->inFlight=receipt;a->active=qtrue;a->discardInFlightHistory=qfalse;
    *out=receipt;return qtrue;
}
ralWebGpuAsyncStatus_t RalWebGpu_AtmospherePoll(ralWebGpuAtmosphere_t *a,const ralWebGpuAtmosphereReceipt_t *r) {
    if(!a||!r||!a->active||!RalWebGpu_AtmosphereReceiptExact(r,&a->inFlight))return RAL_WEBGPU_ASYNC_FAILED;
    const ralWebGpuAsyncStatus_t status=RalWebGpu_CommandPoll(a->command,&r->submission);
    if(status==RAL_WEBGPU_ASYNC_READY) {
        a->active=qfalse;a->historyValid=!a->discardInFlightHistory;
        a->historyParity=!a->historyParity;
        memset(&a->inFlight,0,sizeof(a->inFlight));
    }
    return status;
}
uintptr_t RalWebGpu_AtmosphereArenaIdentity(const ralWebGpuAtmosphere_t *a) {
    return a ? a->receipts[INTEGRATED].resourceIdentity : 0;
}
uint64_t RalWebGpu_AtmosphereArenaBytes(const ralWebGpuAtmosphere_t *a) {
    return a ? a->receipts[INTEGRATED].byteSize : 0;
}
uint32_t RalWebGpu_AtmosphereIntegratedBase(const ralWebGpuAtmosphere_t *a) { (void)a;return 0; }
qboolean RalWebGpu_AtmosphereHistoryValid(const ralWebGpuAtmosphere_t *a) { return a&&a->historyValid ? qtrue : qfalse; }
void RalWebGpu_AtmosphereInvalidateHistory(ralWebGpuAtmosphere_t *a) {
    if(!a)return;
    a->historyValid=qfalse;
    /* GPU completion still retires the submitted work and advances ping-pong,
     * but cannot publish history invalidated after that work was submitted. */
    if(a->active)a->discardInFlightHistory=qtrue;
}
float RalWebGpu_AtmosphereFarDistance(const atmosphereFrameState_t *state) {
    return RenderAtmosphereGpu_FarDistance(state);
}
