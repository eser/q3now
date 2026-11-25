// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
#include "ral_webgpu_exposure.h"
#include "ral_webgpu_exposure_histogram.h"
#include "ral_webgpu_exposure_reduce.h"
#include "ral_webgpu_exposure_tonemap.h"
#include <stdlib.h>
#include <string.h>

enum { HIST, ACCUM, BLOCK, DIMS, DELTA, VERTEX, INDEX, BUFFER_COUNT };
enum { PIPE_HISTOGRAM, PIPE_REDUCE, PIPE_TONE, PIPE_TONE_LINEAR, PIPE_COPY, PIPELINE_COUNT };
struct ralWebGpuExposure_s {
    ralWebGpuRuntime_t *runtime;
    ralWebGpuBrowserBridge_t *bridge;
    uint64_t generation;
    uint64_t nextPipelineGeneration;
    int toneMode;
    float toneExposure;
    /* histogram, reduce, tone map into the canvas; r_aa 2: tone map into `linear` (RGBA16F) and the copy of the SMAA result into the
     * canvas (render_antialiasing.h: SMAA runs on the tone-mapped scene). */
    ralWebGpuPipeline_t *pipelines[PIPELINE_COUNT];
    ralWebGpuPipelineReceipt_t pipelineReceipts[PIPELINE_COUNT];
    ralWebGpuResource_t *buffers[BUFFER_COUNT], *hdr, *sampler, *linear;
    ralWebGpuResourceReceipt_t receipts[BUFFER_COUNT], hdrReceipt, samplerReceipt, linearReceipt;
    uintptr_t groups[5]; /* histogram, reduce, tonemap groups 0/1/2 */
    uintptr_t sourceGroups[2], source; /* histogram and tonemap group 0 for another HDR input (the TAA output) */
    uintptr_t linearGroups[3];          /* tone map into `linear`: groups 0/1/2 */
    uintptr_t copyGroup, copySource;    /* copy of `copySource` into the canvas */
    uint32_t width, height;
    uint64_t worldGeneration, pendingWorld;
    int timeMs, pendingTime;
    qboolean history, active, enabled, pendingEnabled, recorded;
};
static const char vertexSource[] =
    "struct O{@builtin(position)p:vec4<f32>,@location(0)uv:vec2<f32>};"
    "@vertex fn main(@builtin(vertex_index)i:u32)->O{"
    "let p=array<vec2<f32>,3>(vec2(-1.,-1.),vec2(3.,-1.),vec2(-1.,3.));"
    "var o:O;o.p=vec4(p[i],0.,1.);o.uv=vec2(p[i].x*.5+.5,.5-p[i].y*.5);return o;}";
/* r_aa 2: the tone-mapped, SMAA-resolved scene copied into the canvas -srgb view (the values the tone map would have written). */
static const char copySource[] =
    "@group(0) @binding(0) var scene:texture_2d<f32>;"
    "@group(0) @binding(32) var sceneSampler:sampler;"
    "struct O{@builtin(position)p:vec4<f32>,@location(0)uv:vec2<f32>};"
    "@fragment fn main(i:O)->@location(0) vec4<f32>{return vec4(textureSampleLevel(scene,sceneSampler,i.uv,0.).rgb,1.);}";

static qboolean Module( ralShaderModuleAbi_t *m, uint32_t stage,
        const char *source, uint32_t size, uint64_t generation ) {
    const uint32_t spirv[4] = { 0x07230203u };
    memset(m, 0, sizeof(*m));
    m->stage=stage; m->entryPoint="main";
    m->sourceDigest.lane0=generation; m->sourceDigest.lane1=generation+1;
    m->artifacts[RAL_SHADER_ARTIFACT_SPIRV].target=RAL_SHADER_ARTIFACT_SPIRV;
    m->artifacts[RAL_SHADER_ARTIFACT_SPIRV].byteCount=sizeof(spirv);
    m->artifacts[RAL_SHADER_ARTIFACT_MSL].target=RAL_SHADER_ARTIFACT_MSL;
    m->artifacts[RAL_SHADER_ARTIFACT_WGSL].target=RAL_SHADER_ARTIFACT_WGSL;
    m->artifacts[RAL_SHADER_ARTIFACT_WGSL].byteCount=size;
    return Ral_ShaderArtifactDigest(spirv,sizeof(spirv),
        &m->artifacts[RAL_SHADER_ARTIFACT_SPIRV].digest)==ralSuccess
        && Ral_ShaderArtifactDigest(source,size,
        &m->artifacts[RAL_SHADER_ARTIFACT_WGSL].digest)==ralSuccess;
}
static qboolean Pipeline( ralWebGpuExposure_t *e, ralWebGpuRuntimeReceipt_t *rr,
        unsigned index, const unsigned char *source, uint32_t size,
        ralShaderBindingAbi_t *bindings, uint32_t bindingCount ) {
    ralShaderModuleAbi_t modules[2];
    ralWebGpuWgslModule_t wgsl[2];
    ralShaderAbiManifest_t manifest={0};
    ralShaderVariantAbi_t variant={0};
    const ralShaderSpecConstantAbi_t toneSpecs[]={
        {17,RAL_STAGE_FRAGMENT,1,RAL_SHADER_SPEC_I32},
        {18,RAL_STAGE_FRAGMENT,0x3f800000u,RAL_SHADER_SPEC_F32}};
    uint32_t exposureBits;
    /* Mode zero matches Vulkan's disabled tonemap variant: no tone exposure. */
    const float exposure=e->toneMode?e->toneExposure:1.0f;
    memcpy(&exposureBits,&exposure,sizeof(exposureBits));
    const ralShaderSpecValue_t toneValues[]={{17,(uint32_t)e->toneMode},{18,exposureBits}};
    const ralSpecConstant_t nativeToneValues[]={{17,(uint32_t)e->toneMode},{18,exposureBits}};
    ralWebGpuPipelineCreateInfo_t info={0};
    ralComputePipelineCreateInfo_t compute={0};
    ralGraphicsPipelineCreateInfo_t graphics={0};
    ralVertexBinding_t vertex={0};
    ralColorBlendAttachment_t blend={0};
    const ralBindGroupLayout_t *layouts[3]={
        (const ralBindGroupLayout_t *)(uintptr_t)1,
        (const ralBindGroupLayout_t *)(uintptr_t)2,
        (const ralBindGroupLayout_t *)(uintptr_t)3};
    const qboolean graphicsPipeline=index>=PIPE_TONE;
    const qboolean tone=index==PIPE_TONE||index==PIPE_TONE_LINEAR;
    const uint64_t generation=e->nextPipelineGeneration;
    if (!generation || generation>UINT64_MAX-4u) return qfalse;
    if (!Module(&modules[0],graphicsPipeline?RAL_STAGE_VERTEX:RAL_STAGE_COMPUTE,
        graphicsPipeline?vertexSource:(const char *)source,
        graphicsPipeline?(uint32_t)sizeof(vertexSource)-1:size,generation)) return qfalse;
    wgsl[0]=(ralWebGpuWgslModule_t){graphicsPipeline?vertexSource:(const char *)source,
        graphicsPipeline?(uint32_t)sizeof(vertexSource)-1:size};
    if (graphicsPipeline) {
        if (!Module(&modules[1],RAL_STAGE_FRAGMENT,(const char *)source,size,generation+2)) return qfalse;
        wgsl[1]=(ralWebGpuWgslModule_t){(const char *)source,size};
    }
    manifest.schemaVersion=RAL_SHADER_ABI_SCHEMA_VERSION;
    manifest.generation=generation;
    manifest.pipelineKind=graphicsPipeline?RAL_SHADER_PIPELINE_GRAPHICS:RAL_SHADER_PIPELINE_COMPUTE;
    manifest.modules=modules; manifest.moduleCount=graphicsPipeline?2:1;
    manifest.bindings=bindings; manifest.bindingCount=bindingCount;
    variant.artifactTarget=RAL_SHADER_ARTIFACT_WGSL; variant.generation=generation;
    if (graphicsPipeline) {
        if (tone) {
            manifest.specConstants=toneSpecs; manifest.specConstantCount=2;
            variant.specValues=toneValues; variant.specValueCount=2;
            graphics.specConstants=nativeToneValues; graphics.numSpecConstants=2;
        }
        vertex.stride=sizeof(float); vertex.inputRate=RAL_VERTEX_INPUT_PER_VERTEX;
        blend.writeMask=RAL_COLOR_WRITE_ALL; blend.writeMaskExplicit=qtrue;
        graphics.vertexBindings=&vertex; graphics.numVertexBindings=1;
        graphics.topology=RAL_TOPOLOGY_TRIANGLE_LIST;
        graphics.raster.polygonMode=RAL_POLYGON_FILL; graphics.raster.lineWidth=1;
        graphics.colorBlends=&blend; graphics.numColorBlends=1;
        /* Tonemapped LDR linear light into the canvas -srgb view; r_aa 2 tone-maps into an RGBA16F target with the same values. */
        graphics.colorFormats[0]=index==PIPE_TONE_LINEAR?RAL_FORMAT_R16G16B16A16_SFLOAT:RAL_FORMAT_B8G8R8A8_SRGB;
        graphics.numColorFormats=1; graphics.sampleCount=1;
        graphics.bindGroupLayouts=layouts; graphics.numBindGroupLayouts=tone?3:1;
        if (Ral_GraphicsPipelineSemanticDigest(&graphics,&variant.semanticStateDigest)!=ralSuccess) return qfalse;
        info.graphicsState=&graphics;
    } else {
        compute.bindGroupLayouts=layouts; compute.numBindGroupLayouts=1;
        if (Ral_ComputePipelineSemanticDigest(&compute,&variant.semanticStateDigest)!=ralSuccess) return qfalse;
        info.computeState=&compute;
    }
    info.manifest=&manifest; info.variant=&variant; info.modules=wgsl;
    info.moduleCount=manifest.moduleCount; info.generation=generation;
    ralWebGpuPipeline_t *replacement=NULL;
    ralWebGpuPipelineReceipt_t replacementReceipt;
    if (!RalWebGpu_BrowserBridgeApplyPipelineInfo(e->bridge,&info)
        || !RalWebGpu_RuntimeCreatePipeline(e->runtime,rr,&info,
            &replacement,&replacementReceipt)) return qfalse;
    if (e->pipelines[index] && !RalWebGpu_RuntimeDestroyPipeline(e->runtime,
            e->pipelines[index],&e->pipelineReceipts[index])) {
        RalWebGpu_RuntimeDestroyPipeline(e->runtime,replacement,&replacementReceipt);
        return qfalse;
    }
    e->pipelines[index]=replacement; e->pipelineReceipts[index]=replacementReceipt;
    e->nextPipelineGeneration+=4u;
    return RalWebGpu_RuntimeGetReceipt(e->runtime,rr);
}
#define B(g,b,t,s,n) {g,b,t,1,s,n,0,0,RAL_FORMAT_UNDEFINED,qfalse}
static qboolean Pipelines( ralWebGpuExposure_t *e, ralWebGpuRuntimeReceipt_t *rr ) {
    ralShaderBindingAbi_t hist[]={
        B(0,0,RAL_SHADER_BIND_SAMPLED_TEXTURE,RAL_STAGE_COMPUTE,0),
        B(0,1,RAL_SHADER_BIND_FILTERING_SAMPLER,RAL_STAGE_COMPUTE,0),
        B(0,2,RAL_SHADER_BIND_STORAGE_BUFFER_READ_WRITE,RAL_STAGE_COMPUTE,1024),
        B(0,3,RAL_SHADER_BIND_UNIFORM_BUFFER,RAL_STAGE_COMPUTE,16)};
    ralShaderBindingAbi_t reduce[]={
        B(0,0,RAL_SHADER_BIND_STORAGE_BUFFER_READ,RAL_STAGE_COMPUTE,1024),
        B(0,1,RAL_SHADER_BIND_STORAGE_BUFFER_READ_WRITE,RAL_STAGE_COMPUTE,4),
        B(0,2,RAL_SHADER_BIND_STORAGE_BUFFER_READ_WRITE,RAL_STAGE_COMPUTE,64),
        B(0,3,RAL_SHADER_BIND_UNIFORM_BUFFER,RAL_STAGE_COMPUTE,16)};
    ralShaderBindingAbi_t tone[]={
        B(0,0,RAL_SHADER_BIND_SAMPLED_TEXTURE,RAL_STAGE_FRAGMENT,0),
        B(0,32,RAL_SHADER_BIND_FILTERING_SAMPLER,RAL_STAGE_FRAGMENT,0),
        B(2,0,RAL_SHADER_BIND_UNIFORM_BUFFER,RAL_STAGE_FRAGMENT,64)};
    hist[0].viewDimension=tone[0].viewDimension=RAL_SHADER_VIEW_2D;
    hist[0].sampleType=tone[0].sampleType=RAL_SHADER_SAMPLE_FLOAT;
    ralShaderBindingAbi_t copy[]={tone[0],tone[1]};
    return Pipeline(e,rr,PIPE_HISTOGRAM,ral_webgpu_exposure_histogram_wgsl,
        sizeof(ral_webgpu_exposure_histogram_wgsl),hist,4)
        && Pipeline(e,rr,PIPE_REDUCE,ral_webgpu_exposure_reduce_wgsl,
        sizeof(ral_webgpu_exposure_reduce_wgsl),reduce,4)
        && Pipeline(e,rr,PIPE_TONE,ral_webgpu_exposure_tonemap_wgsl,
        sizeof(ral_webgpu_exposure_tonemap_wgsl),tone,3)
        && Pipeline(e,rr,PIPE_TONE_LINEAR,ral_webgpu_exposure_tonemap_wgsl,
        sizeof(ral_webgpu_exposure_tonemap_wgsl),tone,3)
        && Pipeline(e,rr,PIPE_COPY,(const unsigned char *)copySource,
        (uint32_t)sizeof(copySource)-1,copy,2);
}
#undef B
qboolean RalWebGpu_ExposureSetToneMap(ralWebGpuExposure_t *e,int mode,float exposure) {
    if (!e || e->active || mode<0 || mode>4 || !isfinite(exposure)
        || exposure<.1f || exposure>8.f) return qfalse;
    if (e->toneMode==mode && e->toneExposure==exposure) return qtrue;
    ralWebGpuRuntimeReceipt_t rr;
    if (!RalWebGpu_RuntimeGetReceipt(e->runtime,&rr)) return qfalse;
    ralShaderBindingAbi_t bindings[3]={0};
    bindings[0].bindingClass=RAL_SHADER_BIND_SAMPLED_TEXTURE;
    bindings[0].viewDimension=RAL_SHADER_VIEW_2D;
    bindings[0].sampleType=RAL_SHADER_SAMPLE_FLOAT;
    bindings[1].binding=32; bindings[1].bindingClass=RAL_SHADER_BIND_FILTERING_SAMPLER;
    bindings[2].set=2; bindings[2].bindingClass=RAL_SHADER_BIND_UNIFORM_BUFFER;
    bindings[2].minBufferBindingSize=64;
    for (unsigned i=0;i<3;++i) {
        bindings[i].arrayCount=1; bindings[i].stageFlags=RAL_STAGE_FRAGMENT;
    }
    const int oldMode=e->toneMode; const float oldExposure=e->toneExposure;
    e->toneMode=mode; e->toneExposure=exposure;
    if (!Pipeline(e,&rr,PIPE_TONE,ral_webgpu_exposure_tonemap_wgsl,
            sizeof(ral_webgpu_exposure_tonemap_wgsl),bindings,3)
        || !Pipeline(e,&rr,PIPE_TONE_LINEAR,ral_webgpu_exposure_tonemap_wgsl,
            sizeof(ral_webgpu_exposure_tonemap_wgsl),bindings,3)) {
        e->toneMode=oldMode; e->toneExposure=oldExposure; return qfalse;
    }
    /* Bind groups remain compatible: only override values changed, not layouts.
     * No frame is outstanding here; neither HDR storage nor AE history resets. */
    return qtrue;
}
static void SourceGroupsDestroy( ralWebGpuExposure_t *e ) {
    for(unsigned i=0;i<2;++i) if(e->sourceGroups[i]) {
        RalWebGpu_BrowserBridgeDestroyBindGroup(e->bridge,e->sourceGroups[i]);
        e->sourceGroups[i]=0;
    }
    e->source=0;
}
static void LinearGroupsDestroy( ralWebGpuExposure_t *e ) {
    for(unsigned i=0;i<3;++i) if(e->linearGroups[i]) {
        RalWebGpu_BrowserBridgeDestroyBindGroup(e->bridge,e->linearGroups[i]);
        e->linearGroups[i]=0;
    }
    if(e->copyGroup) RalWebGpu_BrowserBridgeDestroyBindGroup(e->bridge,e->copyGroup);
    e->copyGroup=0; e->copySource=0;
}
static void GroupsDestroy( ralWebGpuExposure_t *e ) {
    SourceGroupsDestroy(e);
    LinearGroupsDestroy(e);
    for(unsigned i=5;i>0;--i) if(e->groups[i-1]) {
        RalWebGpu_BrowserBridgeDestroyBindGroup(e->bridge,e->groups[i-1]);
        e->groups[i-1]=0;
    }
}
static ralWebGpuBrowserBindResource_t BufferBinding( ralWebGpuExposure_t *e,
        unsigned binding, unsigned buffer ) {
    return (ralWebGpuBrowserBindResource_t){binding,
        RAL_WEBGPU_BROWSER_BIND_RESOURCE_BUFFER,e->receipts[buffer].resourceIdentity,
        0,e->receipts[buffer].byteSize};
}
static qboolean GroupsCreate( ralWebGpuExposure_t *e ) {
    ralWebGpuBrowserBindResource_t hist[]={
        {0,RAL_WEBGPU_BROWSER_BIND_RESOURCE_TEXTURE,e->hdrReceipt.resourceIdentity,0,0},
        {1,RAL_WEBGPU_BROWSER_BIND_RESOURCE_SAMPLER,e->samplerReceipt.resourceIdentity,0,0},
        BufferBinding(e,2,HIST),BufferBinding(e,3,DIMS)};
    ralWebGpuBrowserBindResource_t reduce[]={BufferBinding(e,0,HIST),
        BufferBinding(e,1,ACCUM),BufferBinding(e,2,BLOCK),BufferBinding(e,3,DELTA)};
    ralWebGpuBrowserBindResource_t tone[]={hist[0],hist[1]};
    ralWebGpuBrowserBindResource_t block=BufferBinding(e,0,BLOCK);
    tone[1].binding=32;
    return RalWebGpu_BrowserBridgeCreateBindGroup(e->bridge,e->pipelineReceipts[0].bindGroupLayoutIdentities[0],hist,4,&e->groups[0])
        && RalWebGpu_BrowserBridgeCreateBindGroup(e->bridge,e->pipelineReceipts[1].bindGroupLayoutIdentities[0],reduce,4,&e->groups[1])
        && RalWebGpu_BrowserBridgeCreateBindGroup(e->bridge,e->pipelineReceipts[2].bindGroupLayoutIdentities[0],tone,2,&e->groups[2])
        && RalWebGpu_BrowserBridgeCreateBindGroup(e->bridge,e->pipelineReceipts[2].bindGroupLayoutIdentities[1],NULL,0,&e->groups[3])
        && RalWebGpu_BrowserBridgeCreateBindGroup(e->bridge,e->pipelineReceipts[2].bindGroupLayoutIdentities[2],&block,1,&e->groups[4]);
}
qboolean RalWebGpu_ExposureCreate( ralWebGpuRuntime_t *runtime,
        ralWebGpuRuntimeReceipt_t *rr, ralWebGpuBrowserBridge_t *bridge,
        uint64_t generation, ralWebGpuExposure_t **out ) {
    if(!runtime||!rr||!bridge||!generation||generation>UINT64_MAX-12||!out||*out) return qfalse;
    ralWebGpuExposure_t *e=calloc(1,sizeof(*e));
    if(!e) return qfalse;
    e->runtime=runtime; e->bridge=bridge; e->generation=generation;
    e->nextPipelineGeneration=generation; e->toneMode=3; e->toneExposure=1.0f;
    if(!Pipelines(e,rr)) goto fail;
    ralWebGpuResourceLayer_t *r=RalWebGpu_RuntimeResources(runtime,rr);
    if(!r) goto fail;
    const uint32_t sizes[]={1024,4,64,16,16,4,12};
    const uint32_t usage[]={RAL_WEBGPU_BUFFER_STORAGE,RAL_WEBGPU_BUFFER_STORAGE,
        RAL_WEBGPU_BUFFER_STORAGE|RAL_WEBGPU_BUFFER_UNIFORM,
        RAL_WEBGPU_BUFFER_UNIFORM,RAL_WEBGPU_BUFFER_UNIFORM,
        RAL_WEBGPU_BUFFER_VERTEX,RAL_WEBGPU_BUFFER_INDEX};
    for(unsigned i=0;i<BUFFER_COUNT;++i) {
        ralWebGpuBufferDesc_t d={0}; d.size=sizes[i];
        d.usage=usage[i]|RAL_WEBGPU_BUFFER_COPY_DESTINATION;
        d.memoryClass=RAL_ALLOCATION_DEVICE_LOCAL;
        if(!RalWebGpu_CreateBuffer(r,&d,&e->buffers[i],&e->receipts[i])) goto fail;
    }
    ralWebGpuWriteReceipt_t wr;
    const uint32_t indices[3]={0,1,2}; const float vertex=0;
    if(!RalWebGpu_WriteBuffer(r,e->buffers[INDEX],&e->receipts[INDEX],0,indices,sizeof(indices),&wr)
        ||!RalWebGpu_WriteBuffer(r,e->buffers[VERTEX],&e->receipts[VERTEX],0,&vertex,sizeof(vertex),&wr)) goto fail;
    ralWebGpuSamplerDesc_t sampler={0};
    sampler.linearMinification=sampler.linearMagnification=qtrue;
    sampler.clampToEdge=qtrue;
    if(!RalWebGpu_CreateSampler(r,&sampler,&e->sampler,&e->samplerReceipt)) goto fail;
    *out=e; return qtrue;
fail:
    RalWebGpu_ExposureDestroy(e,rr); return qfalse;
}
void RalWebGpu_ExposureDestroy( ralWebGpuExposure_t *e,const ralWebGpuRuntimeReceipt_t *rr ) {
    if(!e) return;
    GroupsDestroy(e);
    ralWebGpuResourceLayer_t *r=RalWebGpu_RuntimeResources(e->runtime,rr);
    if(r) {
        if(e->linear) RalWebGpu_DestroyResource(r,e->linear,&e->linearReceipt);
        if(e->hdr) RalWebGpu_DestroyResource(r,e->hdr,&e->hdrReceipt);
        if(e->sampler) RalWebGpu_DestroyResource(r,e->sampler,&e->samplerReceipt);
        for(unsigned i=BUFFER_COUNT;i>0;--i) if(e->buffers[i-1])
            RalWebGpu_DestroyResource(r,e->buffers[i-1],&e->receipts[i-1]);
    }
    for(unsigned i=PIPELINE_COUNT;i>0;--i) if(e->pipelines[i-1])
        RalWebGpu_RuntimeDestroyPipeline(e->runtime,e->pipelines[i-1],&e->pipelineReceipts[i-1]);
    free(e);
}
qboolean RalWebGpu_ExposurePrepare( ralWebGpuExposure_t *e,
        const ralWebGpuRuntimeReceipt_t *rr,uint32_t width,uint32_t height,
        uint64_t world,int timeMs,qboolean cameraCut,const ralExposureSettings_t *s,
        const ralDisplayVisibilityPlan_t *v,uintptr_t *outHdr ) {
    if(!e||e->active||!width||!height||!world||!outHdr
        ||!Ral_ExposureSettingsValid(s)||!Ral_DisplayVisibilityPlanValid(v)) return qfalse;
    ralWebGpuResourceLayer_t *r=RalWebGpu_RuntimeResources(e->runtime,rr);
    if(!r) return qfalse;
    if(width!=e->width||height!=e->height||!e->groups[0]) {
        GroupsDestroy(e);
        if(e->linear) RalWebGpu_DestroyResource(r,e->linear,&e->linearReceipt);
        e->linear=NULL;
        if(e->hdr) RalWebGpu_DestroyResource(r,e->hdr,&e->hdrReceipt);
        e->hdr=NULL; e->width=e->height=0; e->history=qfalse;
        ralWebGpuTextureDesc_t d={0};
        d.width=width; d.height=height; d.depth=d.mipLevelCount=1;
        d.bytesPerTexel=8;
        d.format=RAL_FORMAT_R16G16B16A16_SFLOAT;
        d.usage=RAL_WEBGPU_TEXTURE_SAMPLED|RAL_WEBGPU_TEXTURE_RENDER_ATTACHMENT;
        if(!RalWebGpu_CreateTexture(r,&d,&e->hdr,&e->hdrReceipt)||!GroupsCreate(e)) return qfalse;
        e->width=width; e->height=height;
    }
    if(cameraCut) e->history=qfalse;
    const ralExposureHistoryStep_t step=Ral_ExposureHistoryStep(e->history,e->timeMs,timeMs,
        e->worldGeneration!=world||(int)e->enabled!=s->enabled);
    const qboolean reset=step.reset?qtrue:qfalse;
    const uint32_t dims[4]={width,height,0,0}, zeros[256]={0};
    const float dt[4]={step.deltaSeconds,0,0,0};
    const float identity=1;
    ralExposureBlock_t block={0};
    block.exposure_bias=block.brightness=v->exposureScale;
    block.shadowExponent=v->shadowExponent; block.shadowPivot=v->shadowPivot;
    block.autoEnabled=s->enabled; block.key=s->key;
    block.pctLow=s->pctLow; block.pctHigh=s->pctHigh;
    block.rateUp=s->rateUp; block.rateDown=s->rateDown;
    block.minExp=s->minExp; block.maxExp=s->maxExp;
    ralWebGpuWriteReceipt_t wr;
    if(!RalWebGpu_WriteBuffer(r,e->buffers[BLOCK],&e->receipts[BLOCK],0,&block,sizeof(block),&wr)
        ||!RalWebGpu_WriteBuffer(r,e->buffers[DIMS],&e->receipts[DIMS],0,dims,sizeof(dims),&wr)
        ||!RalWebGpu_WriteBuffer(r,e->buffers[DELTA],&e->receipts[DELTA],0,dt,sizeof(dt),&wr)
        ||(s->enabled&&!RalWebGpu_WriteBuffer(r,e->buffers[HIST],&e->receipts[HIST],0,zeros,sizeof(zeros),&wr))
        ||(reset&&!RalWebGpu_WriteBuffer(r,e->buffers[ACCUM],&e->receipts[ACCUM],0,&identity,sizeof(identity),&wr))) return qfalse;
    e->pendingWorld=world; e->pendingTime=timeMs; e->pendingEnabled=s->enabled;
    e->active=qtrue; e->recorded=qfalse; *outHdr=e->hdrReceipt.resourceIdentity; return qtrue;
}
/* Bind groups that meter and tonemap `source` instead of the scene HDR (same size and format); kept while the source is the same. */
static qboolean SourceGroups( ralWebGpuExposure_t *e, uintptr_t source ) {
    if(e->source==source&&e->sourceGroups[0]&&e->sourceGroups[1]) return qtrue;
    SourceGroupsDestroy(e);
    ralWebGpuBrowserBindResource_t hist[]={
        {0,RAL_WEBGPU_BROWSER_BIND_RESOURCE_TEXTURE,source,0,0},
        {1,RAL_WEBGPU_BROWSER_BIND_RESOURCE_SAMPLER,e->samplerReceipt.resourceIdentity,0,0},
        BufferBinding(e,2,HIST),BufferBinding(e,3,DIMS)};
    ralWebGpuBrowserBindResource_t tone[]={hist[0],hist[1]};
    tone[1].binding=32;
    if(!RalWebGpu_BrowserBridgeCreateBindGroup(e->bridge,e->pipelineReceipts[0].bindGroupLayoutIdentities[0],hist,4,&e->sourceGroups[0])
        ||!RalWebGpu_BrowserBridgeCreateBindGroup(e->bridge,e->pipelineReceipts[2].bindGroupLayoutIdentities[0],tone,2,&e->sourceGroups[1])) {
        SourceGroupsDestroy(e); return qfalse;
    }
    e->source=source; return qtrue;
}
static qboolean RecordMetering( ralWebGpuExposure_t *e, ralWebGpuCommand_t *cmd, ralWebGpuCommandReceipt_t *rec, qboolean alt ) {
    ralWebGpuCommandReceipt_t next;
    ralWebGpuPassDesc_t pass={0};
    if(e->pendingEnabled) {
        pass.kind=RAL_WEBGPU_PASS_COMPUTE;
        if(!RalWebGpu_CommandNextPass(cmd,rec,&pass,&next)) return qfalse;
        *rec=next;
        for(unsigned i=0;i<2;++i) {
            ralWebGpuComputeDispatch_t d={0};
            d.pipelineIdentity=e->pipelineReceipts[i].pipelineIdentity;
            d.bindGroupCount=1; d.bindGroupIdentities[0]=(alt&&i==0)?e->sourceGroups[0]:e->groups[i];
            d.groupCountX=i?1:(e->width+15)/16;
            d.groupCountY=i?1:(e->height+15)/16; d.groupCountZ=1;
            d.contentDigest=e->generation;
            if(!RalWebGpu_CommandRecordComputeDispatch(cmd,rec,&d,&next)) return qfalse;
            *rec=next;
        }
    }
    return qtrue;
}
/* One full-screen draw of `pipeline` with `groups` into `output`. */
static qboolean RecordFullscreen( ralWebGpuExposure_t *e, ralWebGpuCommand_t *cmd, ralWebGpuCommandReceipt_t *rec,
        uintptr_t output, unsigned pipeline, const uintptr_t *groups, unsigned groupCount ) {
    ralWebGpuCommandReceipt_t next;
    ralWebGpuPassDesc_t pass={0};
    pass.kind=RAL_WEBGPU_PASS_RENDER;
    pass.targetIdentity=output; pass.colorLoadOp=RAL_WEBGPU_LOAD_CLEAR;
    if(!RalWebGpu_CommandNextPass(cmd,rec,&pass,&next)) return qfalse;
    *rec=next;
    ralWebGpuIndexedDraw_t draw={0}; draw.kind=RAL_WEBGPU_DRAW_EFFECT;
    draw.pipelineIdentity=e->pipelineReceipts[pipeline].pipelineIdentity;
    draw.vertexBufferIdentity=e->receipts[VERTEX].resourceIdentity;
    draw.indexBufferIdentity=e->receipts[INDEX].resourceIdentity;
    draw.indexCount=3; draw.instanceCount=1; draw.contentDigest=e->generation;
    draw.bindGroupCount=groupCount;
    for(unsigned i=0;i<groupCount;++i) draw.bindGroupIdentities[i]=groups[i];
    if(!RalWebGpu_CommandRecordIndexedDraw(cmd,rec,&draw,&next)) return qfalse;
    *rec=next; return qtrue;
}
qboolean RalWebGpu_ExposureRecord( ralWebGpuExposure_t *e,
        ralWebGpuCommand_t *cmd,ralWebGpuCommandReceipt_t *rec,uintptr_t output,uintptr_t source ) {
    if(!e||!e->active||e->recorded||!cmd||!rec||!output) return qfalse;
    if(source&&source!=e->hdrReceipt.resourceIdentity&&!SourceGroups(e,source)) return qfalse;
    const qboolean alt=source&&source!=e->hdrReceipt.resourceIdentity;
    uintptr_t groups[3]={e->groups[2],e->groups[3],e->groups[4]};
    if(alt) groups[0]=e->sourceGroups[1];
    if(!RecordMetering(e,cmd,rec,alt)||!RecordFullscreen(e,cmd,rec,output,PIPE_TONE,groups,3)) return qfalse;
    e->recorded=qtrue; return qtrue;
}
qboolean RalWebGpu_ExposurePrepareLinear( ralWebGpuExposure_t *e, const ralWebGpuRuntimeReceipt_t *rr, uintptr_t *outLinear ) {
    if(!e||!e->active||!e->width||!e->height||!outLinear) return qfalse;
    if(!e->linear) {
        ralWebGpuResourceLayer_t *r=RalWebGpu_RuntimeResources(e->runtime,rr);
        ralWebGpuTextureDesc_t d={0};
        if(!r) return qfalse;
        d.width=e->width; d.height=e->height; d.depth=d.mipLevelCount=1;
        d.bytesPerTexel=8; d.format=RAL_FORMAT_R16G16B16A16_SFLOAT;
        d.usage=RAL_WEBGPU_TEXTURE_SAMPLED|RAL_WEBGPU_TEXTURE_RENDER_ATTACHMENT;
        if(!RalWebGpu_CreateTexture(r,&d,&e->linear,&e->linearReceipt)) return qfalse;
    }
    if(!e->linearGroups[0]) {
        ralWebGpuBrowserBindResource_t tone[]={
            {0,RAL_WEBGPU_BROWSER_BIND_RESOURCE_TEXTURE,e->hdrReceipt.resourceIdentity,0,0},
            {32,RAL_WEBGPU_BROWSER_BIND_RESOURCE_SAMPLER,e->samplerReceipt.resourceIdentity,0,0}};
        ralWebGpuBrowserBindResource_t block=BufferBinding(e,0,BLOCK);
        if(!RalWebGpu_BrowserBridgeCreateBindGroup(e->bridge,e->pipelineReceipts[PIPE_TONE_LINEAR].bindGroupLayoutIdentities[0],tone,2,&e->linearGroups[0])
            ||!RalWebGpu_BrowserBridgeCreateBindGroup(e->bridge,e->pipelineReceipts[PIPE_TONE_LINEAR].bindGroupLayoutIdentities[1],NULL,0,&e->linearGroups[1])
            ||!RalWebGpu_BrowserBridgeCreateBindGroup(e->bridge,e->pipelineReceipts[PIPE_TONE_LINEAR].bindGroupLayoutIdentities[2],&block,1,&e->linearGroups[2])) {
            LinearGroupsDestroy(e); return qfalse;
        }
    }
    *outLinear=e->linearReceipt.resourceIdentity; return qtrue;
}
qboolean RalWebGpu_ExposureRecordLinear( ralWebGpuExposure_t *e, ralWebGpuCommand_t *cmd, ralWebGpuCommandReceipt_t *rec ) {
    if(!e||!e->active||e->recorded||!cmd||!rec||!e->linear||!e->linearGroups[0]) return qfalse;
    if(!RecordMetering(e,cmd,rec,qfalse)
        ||!RecordFullscreen(e,cmd,rec,e->linearReceipt.resourceIdentity,PIPE_TONE_LINEAR,e->linearGroups,3)) return qfalse;
    e->recorded=qtrue; return qtrue;
}
qboolean RalWebGpu_ExposureRecordCopy( ralWebGpuExposure_t *e, ralWebGpuCommand_t *cmd, ralWebGpuCommandReceipt_t *rec,
        uintptr_t output, uintptr_t source ) {
    if(!e||!e->active||!e->recorded||!cmd||!rec||!output||!source) return qfalse;
    if(e->copySource!=source||!e->copyGroup) {
        ralWebGpuBrowserBindResource_t copy[]={
            {0,RAL_WEBGPU_BROWSER_BIND_RESOURCE_TEXTURE,source,0,0},
            {32,RAL_WEBGPU_BROWSER_BIND_RESOURCE_SAMPLER,e->samplerReceipt.resourceIdentity,0,0}};
        if(e->copyGroup) RalWebGpu_BrowserBridgeDestroyBindGroup(e->bridge,e->copyGroup);
        e->copyGroup=0; e->copySource=0;
        if(!RalWebGpu_BrowserBridgeCreateBindGroup(e->bridge,e->pipelineReceipts[PIPE_COPY].bindGroupLayoutIdentities[0],copy,2,&e->copyGroup)) return qfalse;
        e->copySource=source;
    }
    return RecordFullscreen(e,cmd,rec,output,PIPE_COPY,&e->copyGroup,1);
}
void RalWebGpu_ExposureComplete( ralWebGpuExposure_t *e,qboolean success ) {
    if(!e||!e->active) return;
    e->history=success&&e->recorded;
    if(e->history) {e->worldGeneration=e->pendingWorld;e->timeMs=e->pendingTime;e->enabled=e->pendingEnabled;}
    e->active=qfalse; e->recorded=qfalse;
}
void RalWebGpu_ExposureReset( ralWebGpuExposure_t *e ) {if(e) e->history=qfalse;}
