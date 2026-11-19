// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
#include "ral_webgpu_sky_material.h"
#include "ral_webgpu_sky_vertex.h"
#include "ral_webgpu_sky_fragment.h"
#include "../../../frontend/render_submission_world.h"
#include <stdlib.h>
#include <string.h>

typedef struct {
    ralWebGpuResource_t *buffers[2];
    ralWebGpuResourceReceipt_t receipts[2];
    uintptr_t groups[3], textures[10], samplers[10];
} skyDrawSlot_t;
struct ralWebGpuSkyMaterial_s {
    ralWebGpuRuntime_t *runtime;
    ralWebGpuBrowserBridge_t *bridge;
    ralWebGpuPipeline_t *pipeline;
    ralWebGpuPipelineReceipt_t pipelineReceipt;
    skyDrawSlot_t *slots;
    uint32_t capacity, count, used;
};

static qboolean Module(ralShaderModuleAbi_t *m, const char *name,
        const unsigned char *wgsl, uint32_t size) {
    memset(m,0,sizeof(*m));
    m->entryPoint="main";
    // Real canonical SPIR-V/source identities, not placeholder SPIR-V bytes.
#define RAL_SHADER_SPIRV_ARTIFACT(ordinal,symbol,flags,size,src0,src1,bin0,bin1) \
    if (!strcmp(name,#symbol)) { \
        m->stage=flags; m->sourceDigest=(ralShaderDigest_t){src0,src1}; \
        m->artifacts[RAL_SHADER_ARTIFACT_SPIRV].byteCount=size; \
        m->artifacts[RAL_SHADER_ARTIFACT_SPIRV].digest=(ralShaderDigest_t){bin0,bin1}; \
    }
#include "../vulkan/renderer/shaders/spirv/ral_shader_artifact_catalog.inc"
#undef RAL_SHADER_SPIRV_ARTIFACT
    for (unsigned i=0;i<RAL_SHADER_ARTIFACT_COUNT;++i) m->artifacts[i].target=(ralShaderArtifactTarget_t)i;
    m->artifacts[RAL_SHADER_ARTIFACT_WGSL].byteCount=size;
    return m->stage && Ral_ShaderArtifactDigest(wgsl,size,
        &m->artifacts[RAL_SHADER_ARTIFACT_WGSL].digest)==ralSuccess;
}

static qboolean Pipeline(ralWebGpuSkyMaterial_t *s, ralWebGpuRuntimeReceipt_t *rr,
        uint64_t generation, qboolean depthEnabled) {
    ralShaderModuleAbi_t modules[2];
    if (!Module(&modules[0],"ral_opengl_product_vert_spv",ral_webgpu_sky_vertex_wgsl,
            sizeof(ral_webgpu_sky_vertex_wgsl))
        || !Module(&modules[1],"ral_sky_material_frag_spv",ral_webgpu_sky_fragment_wgsl,
            sizeof(ral_webgpu_sky_fragment_wgsl))) return qfalse;
    const ralWebGpuWgslModule_t wgsl[] = {
        {(const char *)ral_webgpu_sky_vertex_wgsl,sizeof(ral_webgpu_sky_vertex_wgsl)},
        {(const char *)ral_webgpu_sky_fragment_wgsl,sizeof(ral_webgpu_sky_fragment_wgsl)}};
    ralShaderBindingAbi_t bindings[22]={0};
    bindings[0].set=0; bindings[0].binding=0;
    bindings[0].bindingClass=RAL_SHADER_BIND_UNIFORM_BUFFER;
    bindings[0].minBufferBindingSize=sizeof(renderProductDrawGpuParams_t);
    bindings[0].stageFlags=RAL_STAGE_VERTEX|RAL_STAGE_FRAGMENT;
    for(unsigned i=0;i<20;++i) {
        ralShaderBindingAbi_t *b=&bindings[1+i];
        b->set=1; b->binding=i; b->stageFlags=RAL_STAGE_FRAGMENT;
        b->bindingClass=(i&1)?RAL_SHADER_BIND_FILTERING_SAMPLER:RAL_SHADER_BIND_SAMPLED_TEXTURE;
        if (!(i&1)) { b->viewDimension=RAL_SHADER_VIEW_2D; b->sampleType=RAL_SHADER_SAMPLE_FLOAT; }
    }
    bindings[21].set=2; bindings[21].binding=1;
    bindings[21].bindingClass=RAL_SHADER_BIND_UNIFORM_BUFFER;
    bindings[21].minBufferBindingSize=sizeof(renderSkyMaterialGpuParams_t);
    bindings[21].stageFlags=RAL_STAGE_VERTEX|RAL_STAGE_FRAGMENT;
    for(unsigned i=0;i<22;++i) bindings[i].arrayCount=1;
    const ralFormat_t formats[]={RAL_FORMAT_R32G32B32_SFLOAT,RAL_FORMAT_R32G32_SFLOAT,
        RAL_FORMAT_R32G32_SFLOAT,RAL_FORMAT_R8G8B8A8_UNORM,RAL_FORMAT_R32G32B32_SFLOAT};
    const uint32_t offsets[]={offsetof(renderWorldVertex_t,position),offsetof(renderWorldVertex_t,texCoord),
        offsetof(renderWorldVertex_t,lightmapCoord),offsetof(renderWorldVertex_t,color),offsetof(renderWorldVertex_t,normal)};
    ralShaderVertexInputAbi_t inputs[5]={0};
    ralVertexAttribute_t attributes[5]={0};
    for(unsigned i=0;i<5;++i) {
        inputs[i].location=i; inputs[i].format=formats[i];
        attributes[i].location=i; attributes[i].format=formats[i]; attributes[i].offset=offsets[i];
    }
    ralShaderAbiManifest_t manifest={0};
    manifest.schemaVersion=RAL_SHADER_ABI_SCHEMA_VERSION; manifest.generation=generation;
    manifest.pipelineKind=RAL_SHADER_PIPELINE_GRAPHICS;
    manifest.modules=modules; manifest.moduleCount=2;
    manifest.bindings=bindings; manifest.bindingCount=22;
    manifest.vertexInputs=inputs; manifest.vertexInputCount=5;
    ralVertexBinding_t vertex={0}; vertex.stride=sizeof(renderWorldVertex_t);
    vertex.inputRate=RAL_VERTEX_INPUT_PER_VERTEX;
    ralColorBlendAttachment_t blend={0}; blend.writeMask=RAL_COLOR_WRITE_ALL; blend.writeMaskExplicit=qtrue;
    const ralBindGroupLayout_t *layouts[]={(void *)(uintptr_t)1,(void *)(uintptr_t)2,(void *)(uintptr_t)3};
    ralGraphicsPipelineCreateInfo_t graphics={0};
    graphics.vertexBindings=&vertex; graphics.numVertexBindings=1;
    graphics.vertexAttributes=attributes; graphics.numVertexAttributes=5;
    graphics.topology=RAL_TOPOLOGY_TRIANGLE_LIST;
    graphics.raster.polygonMode=RAL_POLYGON_FILL; graphics.raster.lineWidth=1;
    graphics.depthStencil.depthTestEnable=depthEnabled;
    graphics.depthStencil.depthCompareOp=RAL_COMPARE_LESS_EQUAL;
    graphics.colorBlends=&blend; graphics.numColorBlends=1;
    graphics.colorFormats[0]=RAL_FORMAT_R16G16B16A16_SFLOAT; graphics.numColorFormats=1;
    graphics.depthFormat=depthEnabled?RAL_FORMAT_D32_SFLOAT:RAL_FORMAT_UNDEFINED; graphics.sampleCount=1;
    graphics.bindGroupLayouts=layouts; graphics.numBindGroupLayouts=3;
    ralShaderVariantAbi_t variant={0}; variant.generation=generation; variant.artifactTarget=RAL_SHADER_ARTIFACT_WGSL;
    if(Ral_GraphicsPipelineSemanticDigest(&graphics,&variant.semanticStateDigest)!=ralSuccess) return qfalse;
    ralWebGpuPipelineCreateInfo_t info={0};
    info.manifest=&manifest; info.variant=&variant; info.modules=wgsl; info.moduleCount=2;
    info.generation=generation; info.graphicsState=&graphics;
    return RalWebGpu_BrowserBridgeApplyPipelineInfo(s->bridge,&info)
        && RalWebGpu_RuntimeCreatePipeline(s->runtime,rr,&info,&s->pipeline,&s->pipelineReceipt)
        && RalWebGpu_RuntimeGetReceipt(s->runtime,rr);
}

qboolean RalWebGpu_SkyMaterialCreate(ralWebGpuRuntime_t *runtime,
        ralWebGpuRuntimeReceipt_t *rr, ralWebGpuBrowserBridge_t *bridge,
        uint64_t generation, qboolean depthEnabled, ralWebGpuSkyMaterial_t **out) {
    if(!runtime||!rr||!bridge||!generation||!out||*out) return qfalse;
    ralWebGpuSkyMaterial_t *s=calloc(1,sizeof(*s));
    if(!s) return qfalse;
    s->runtime=runtime; s->bridge=bridge;
    if(!Pipeline(s,rr,generation,depthEnabled)) { RalWebGpu_SkyMaterialDestroy(s,rr); return qfalse; }
    *out=s; return qtrue;
}
void RalWebGpu_SkyMaterialReset(ralWebGpuSkyMaterial_t *s,const ralWebGpuRuntimeReceipt_t *rr) {
    if(!s) return;
    ralWebGpuResourceLayer_t *r=RalWebGpu_RuntimeResources(s->runtime,rr);
    for(unsigned i=0;i<s->capacity;++i) {
        skyDrawSlot_t *slot=&s->slots[i];
        for(unsigned g=0;g<3;++g) if(slot->groups[g])
            RalWebGpu_BrowserBridgeDestroyBindGroup(s->bridge,slot->groups[g]);
        for(unsigned b=0;b<2;++b) if(r&&slot->buffers[b])
            RalWebGpu_DestroyResource(r,slot->buffers[b],&slot->receipts[b]);
    }
    free(s->slots); s->slots=NULL; s->capacity=s->count=s->used=0;
}
void RalWebGpu_SkyMaterialDestroy(ralWebGpuSkyMaterial_t *s,const ralWebGpuRuntimeReceipt_t *rr) {
    if(!s) return;
    RalWebGpu_SkyMaterialReset(s,rr);
    if(s->pipeline) RalWebGpu_RuntimeDestroyPipeline(s->runtime,s->pipeline,&s->pipelineReceipt);
    free(s);
}
qboolean RalWebGpu_SkyMaterialPrepare(ralWebGpuSkyMaterial_t *s,
        const ralWebGpuRuntimeReceipt_t *rr,uint32_t count) {
    if(!s || count>RENDER_SUBMISSION_MAX_WORLD_BATCHES
        || !RalWebGpu_RuntimeOwnsPipeline(s->runtime,rr,&s->pipelineReceipt)) return qfalse;
    if(count>s->capacity) {
        skyDrawSlot_t *slots=realloc(s->slots,(size_t)count*sizeof(*slots));
        if(!slots) return qfalse;
        memset(slots+s->capacity,0,(count-s->capacity)*sizeof(*slots));
        s->slots=slots; s->capacity=count;
    }
    s->count=count; s->used=0; return qtrue;
}
qboolean RalWebGpu_SkyMaterialRecord(ralWebGpuSkyMaterial_t *s,
        const ralWebGpuRuntimeReceipt_t *rr,ralWebGpuCommand_t *command,
        const renderProductDrawGpuParams_t *drawParams,const renderSkyMaterialGpuParams_t *materialParams,
        const uintptr_t textures[10],const uintptr_t samplers[10],const ralWebGpuIndexedDraw_t *geometry,
        const ralWebGpuCommandReceipt_t *recording,ralWebGpuCommandReceipt_t *updated) {
    if(!s||s->used>=s->count||!drawParams||!materialParams||!textures||!samplers||!geometry
        ||drawParams->atmosphereHeightSky[3]!=1.0f||materialParams->meta[0]>8) return qfalse;
    for(unsigned i=0;i<10;++i) if(!textures[i]||!samplers[i]) return qfalse;
    ralWebGpuResourceLayer_t *r=RalWebGpu_RuntimeResources(s->runtime,rr);
    if(!r) return qfalse;
    skyDrawSlot_t *slot=&s->slots[s->used];
    const uint32_t sizes[]={sizeof(*drawParams),sizeof(*materialParams)};
    const void *bytes[]={drawParams,materialParams};
    for(unsigned i=0;i<2;++i) {
        if(!slot->buffers[i]) {
            ralWebGpuBufferDesc_t d={0}; d.size=sizes[i];
            d.usage=RAL_WEBGPU_BUFFER_UNIFORM|RAL_WEBGPU_BUFFER_COPY_DESTINATION;
            d.memoryClass=RAL_ALLOCATION_DEVICE_LOCAL;
            if(!RalWebGpu_CreateBuffer(r,&d,&slot->buffers[i],&slot->receipts[i])) return qfalse;
        }
        ralWebGpuWriteReceipt_t wr;
        if(!RalWebGpu_WriteBuffer(r,slot->buffers[i],&slot->receipts[i],0,bytes[i],sizes[i],&wr)) return qfalse;
        const unsigned group=i?2:0;
        if(!slot->groups[group]) {
            ralWebGpuBrowserBindResource_t b={i?1:0,RAL_WEBGPU_BROWSER_BIND_RESOURCE_BUFFER,
                slot->receipts[i].resourceIdentity,0,sizes[i]};
            if(!RalWebGpu_BrowserBridgeCreateBindGroup(s->bridge,s->pipelineReceipt.bindGroupLayoutIdentities[group],
                &b,1,&slot->groups[group])) return qfalse;
        }
    }
    if(!slot->groups[1]||memcmp(textures,slot->textures,sizeof(slot->textures))
        ||memcmp(samplers,slot->samplers,sizeof(slot->samplers))) {
        ralWebGpuBrowserBindResource_t bindings[20];
        for(unsigned i=0;i<10;++i) {
            bindings[i*2]=(ralWebGpuBrowserBindResource_t){i*2,RAL_WEBGPU_BROWSER_BIND_RESOURCE_TEXTURE,textures[i],0,0};
            bindings[i*2+1]=(ralWebGpuBrowserBindResource_t){i*2+1,RAL_WEBGPU_BROWSER_BIND_RESOURCE_SAMPLER,samplers[i],0,0};
        }
        uintptr_t group=0;
        if(!RalWebGpu_BrowserBridgeCreateBindGroup(s->bridge,s->pipelineReceipt.bindGroupLayoutIdentities[1],
            bindings,20,&group)) return qfalse;
        if(slot->groups[1]) RalWebGpu_BrowserBridgeDestroyBindGroup(s->bridge,slot->groups[1]);
        slot->groups[1]=group;
        memcpy(slot->textures,textures,sizeof(slot->textures)); memcpy(slot->samplers,samplers,sizeof(slot->samplers));
    }
    ralWebGpuIndexedDraw_t draw=*geometry;
    draw.pipelineIdentity=s->pipelineReceipt.pipelineIdentity;
    draw.bindGroupCount=3; draw.textureBindGroupSlot=0;
    memcpy(draw.bindGroupIdentities,slot->groups,sizeof(slot->groups));
    if(!RalWebGpu_CommandRecordIndexedDraw(command,recording,&draw,updated)) return qfalse;
    ++s->used; return qtrue;
}
