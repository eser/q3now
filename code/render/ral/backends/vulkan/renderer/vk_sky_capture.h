// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
#ifndef WIRED_VK_SKY_CAPTURE_H
#define WIRED_VK_SKY_CAPTURE_H
#include "vk_sky_material.h"
#include "../../../core/ral.h"

// Subresources of the existing IBL owner, not an independent renderer or queue.
typedef struct {
    ralPipeline_t *pipeline;
    ralBindGroupLayout_t *layouts[3];
    ralBuffer_t *vertices, *params;
    ralBindGroup_t *images, *environment, *uniforms;
    uint64_t productRevision;
    int timeMs;
    qboolean animated, physical;
} wiredVkSkyCapture_t;

static const float wired_vk_sky_basis[6][3][3] = {
    {{1,0,0},{0,0,-1},{0,-1,0}}, {{-1,0,0},{0,0,1},{0,-1,0}},
    {{0,1,0},{1,0,0},{0,0,1}}, {{0,-1,0},{1,0,0},{0,0,-1}},
    {{0,0,1},{1,0,0},{0,-1,0}}, {{0,0,-1},{-1,0,0},{0,-1,0}}
};
typedef struct { float position[3], uv[2], lightmap[2], color[4], normal[3]; } wiredVkSkyVertex_t;

static void wired_vk_sky_capture_retire(wiredVkSkyCapture_t *s) {
    if(s->images) Ral_DestroyBindGroup(s->images);
    if(s->environment) Ral_DestroyBindGroup(s->environment);
    if(s->uniforms) Ral_DestroyBindGroup(s->uniforms);
    if(s->params) Ral_DestroyBuffer(s->params);
    s->images=NULL; s->environment=NULL; s->uniforms=NULL; s->params=NULL; s->productRevision=0;
}
static void wired_vk_sky_capture_shutdown(wiredVkSkyCapture_t *s) {
    wired_vk_sky_capture_retire(s);
    if(s->pipeline) Ral_DestroyPipeline(s->pipeline);
    if(s->vertices) Ral_DestroyBuffer(s->vertices);
    for(unsigned i=0;i<3;++i) if(s->layouts[i]) Ral_DestroyBindGroupLayout(s->layouts[i]);
    memset(s,0,sizeof(*s));
}
static qboolean wired_vk_sky_capture_init(wiredVkSkyCapture_t *s, ralBackend_t *backend,
        const uint32_t *vertex, uint32_t vertexBytes,
        const uint32_t *fragment, uint32_t fragmentBytes) {
    if(s->pipeline) return qtrue;
    ralBindEntry_t entries[16]={0};
    ralBindGroupLayoutCreateInfo_t layout={0};
    for(unsigned i=0;i<16;++i) {
        entries[i].binding=i; entries[i].count=1; entries[i].stageFlags=RAL_STAGE_FRAGMENT;
        entries[i].type=(i&1)?RAL_BIND_SAMPLER:RAL_BIND_SAMPLED_TEXTURE;
    }
    layout.entries=entries; layout.numEntries=16; layout.debugName="sky-capture-images";
    s->layouts[1]=Ral_CreateBindGroupLayout(backend,&layout);
    for(unsigned i=0;i<9;++i)
        entries[i].type=(i==6||i==8)?RAL_BIND_SAMPLER:RAL_BIND_SAMPLED_TEXTURE;
    layout.numEntries=9; layout.debugName="sky-capture-environment";
    s->layouts[0]=Ral_CreateBindGroupLayout(backend,&layout);
    entries[0].binding=1; entries[0].type=RAL_BIND_UNIFORM_BUFFER;
    entries[0].stageFlags=RAL_STAGE_VERTEX|RAL_STAGE_FRAGMENT;
    layout.numEntries=1; layout.debugName="sky-capture-params";
    s->layouts[2]=Ral_CreateBindGroupLayout(backend,&layout);
    if(!s->layouts[0]||!s->layouts[1]||!s->layouts[2]) goto fail;
    wiredVkSkyVertex_t data[24]={0};
    for(unsigned f=0;f<6;++f) for(unsigned v=0;v<4;++v) {
        wiredVkSkyVertex_t *p=&data[f*4+v];
        for(unsigned c=0;c<3;++c) {
            p->position[c]=1024*(wired_vk_sky_basis[f][0][c]
                +((v&1)?1:-1)*wired_vk_sky_basis[f][1][c]
                +((v&2)?1:-1)*wired_vk_sky_basis[f][2][c]);
            p->normal[c]=-wired_vk_sky_basis[f][0][c];
        }
        for(unsigned c=0;c<4;++c) p->color[c]=1;
    }
    ralBufferCreateInfo_t buffer={0};
    ralBufferUploadReceipt_t write;
    buffer.size=sizeof(data); buffer.usage=RAL_BUFFER_VERTEX|RAL_BUFFER_TRANSFER_DST;
    buffer.memory=RAL_MEMORY_HOST_VISIBLE; buffer.debugName="sky-capture-vertices";
    s->vertices=Ral_CreateBuffer(backend,&buffer);
    if(!s->vertices||!Ral_BufferWriteImmediate(s->vertices,0,data,sizeof(data),&write)) goto fail;
    const ralVertexBinding_t binding={0,sizeof(wiredVkSkyVertex_t),RAL_VERTEX_INPUT_PER_VERTEX};
    const ralVertexAttribute_t attributes[]={
        {0,0,RAL_FORMAT_R32G32B32_SFLOAT,offsetof(wiredVkSkyVertex_t,position)},
        {1,0,RAL_FORMAT_R32G32_SFLOAT,offsetof(wiredVkSkyVertex_t,uv)},
        {2,0,RAL_FORMAT_R32G32_SFLOAT,offsetof(wiredVkSkyVertex_t,lightmap)},
        {3,0,RAL_FORMAT_R32G32B32A32_SFLOAT,offsetof(wiredVkSkyVertex_t,color)},
        {4,0,RAL_FORMAT_R32G32B32_SFLOAT,offsetof(wiredVkSkyVertex_t,normal)}};
    ralGraphicsPipelineCreateInfo_t pipeline={0};
    pipeline.vertexSpirv=vertex; pipeline.vertexSpirvSize=vertexBytes;
    pipeline.fragmentSpirv=fragment; pipeline.fragmentSpirvSize=fragmentBytes;
    pipeline.vertexBindings=&binding; pipeline.numVertexBindings=1;
    pipeline.vertexAttributes=attributes; pipeline.numVertexAttributes=5;
    pipeline.topology=RAL_TOPOLOGY_TRIANGLE_STRIP; pipeline.sampleCount=1;
    pipeline.numColorFormats=1; pipeline.colorFormats[0]=RAL_FORMAT_R16G16B16A16_SFLOAT;
    pipeline.bindGroupLayouts=(const ralBindGroupLayout_t *const *)s->layouts;
    pipeline.numBindGroupLayouts=3;
    pipeline.pushConstantSize=sizeof(renderProductDrawGpuParams_t);
    pipeline.pushConstantStages=RAL_STAGE_VERTEX|RAL_STAGE_FRAGMENT;
    pipeline.debugName="sky-capture-ordered-material";
    s->pipeline=Ral_CreateGraphicsPipeline(backend,&pipeline);
    if(s->pipeline) return qtrue;
fail:
    wired_vk_sky_capture_shutdown(s); return qfalse;
}

// Immutable for the complete six-face cohort. Replacement uses the existing
// RAL deferred retirement; no CPU overwrite of in-flight uniforms, no wait.
static qboolean wired_vk_sky_capture_prepare(wiredVkSkyCapture_t *s, ralBackend_t *backend,
        const ralFrontendSkyMaterial_t *packet, ralTextureView_t *const outer[6],
        ralSampler_t *outerSampler, ralTextureView_t *physical, ralSampler_t *physicalSampler,
        qboolean physicalReady, uint64_t revision, int timeMs) {
    ralBuffer_t *buffer=NULL;
    ralBindGroup_t *images=NULL,*environment=NULL,*uniforms=NULL;
    ralBindingValue_t values[25]={0};
    ralBufferUploadReceipt_t write;
    if(!s->pipeline||!packet||!packet->ready||!revision||!outerSampler||!physical||!physicalSampler) return qfalse;
    for(unsigned i=0;i<8;++i) {
        if(!packet->stageViews[i]||!packet->stageSamplers[i]) return qfalse;
        values[i*2].binding=i*2; values[i*2].type=RAL_BIND_SAMPLED_TEXTURE;
        values[i*2].textureView=packet->stageViews[i];
        values[i*2+1].binding=i*2+1; values[i*2+1].type=RAL_BIND_SAMPLER;
        values[i*2+1].sampler=packet->stageSamplers[i];
    }
    for(unsigned i=0;i<6;++i) {
        if(!outer[i]) return qfalse;
        values[16+i].binding=i; values[16+i].type=RAL_BIND_SAMPLED_TEXTURE;
        values[16+i].textureView=outer[i];
    }
    values[22].binding=6; values[22].type=RAL_BIND_SAMPLER; values[22].sampler=outerSampler;
    values[23].binding=7; values[23].type=RAL_BIND_SAMPLED_TEXTURE; values[23].textureView=physical;
    values[24].binding=8; values[24].type=RAL_BIND_SAMPLER; values[24].sampler=physicalSampler;
    ralBufferCreateInfo_t b={0}; b.size=sizeof(packet->params); b.usage=RAL_BUFFER_UNIFORM|RAL_BUFFER_TRANSFER_DST;
    b.memory=RAL_MEMORY_HOST_VISIBLE; b.debugName="sky-capture-frozen-material";
    buffer=Ral_CreateBuffer(backend,&b);
    if(!buffer||!Ral_BufferWriteImmediate(buffer,0,&packet->params,sizeof(packet->params),&write)) goto fail;
    ralBindGroupCreateInfo_t group={0}; group.layout=s->layouts[1]; group.values=values; group.numValues=16;
    group.debugName="sky-capture-resident-images";
    images=Ral_CreateBindGroup(backend,&group); if(!images) goto fail;
    group.layout=s->layouts[0]; group.values=values+16; group.numValues=9;
    group.debugName="sky-capture-environment-images";
    environment=Ral_CreateBindGroup(backend,&group); if(!environment) goto fail;
    ralBindingValue_t uniform={0}; uniform.binding=1; uniform.type=RAL_BIND_UNIFORM_BUFFER;
    uniform.buffer=buffer; uniform.bufferRange=sizeof(packet->params);
    group.layout=s->layouts[2]; group.values=&uniform; group.numValues=1;
    group.debugName="sky-capture-frozen-uniform";
    uniforms=Ral_CreateBindGroup(backend,&group); if(!uniforms) goto fail;
    wired_vk_sky_capture_retire(s);
    s->params=buffer; s->images=images; s->environment=environment; s->uniforms=uniforms;
    s->productRevision=revision; s->timeMs=timeMs; s->animated=packet->animated; s->physical=physicalReady;
    return qtrue;
fail:
    if(images) Ral_DestroyBindGroup(images);
    if(environment) Ral_DestroyBindGroup(environment);
    if(uniforms) Ral_DestroyBindGroup(uniforms);
    if(buffer) Ral_DestroyBuffer(buffer);
    return qfalse;
}

static qboolean wired_vk_sky_capture_draw(wiredVkSkyCapture_t *s, ralCommandBuffer_t *cmd,
        ralTexture_t *target, uint32_t size, uint32_t mask) {
    if(!s->pipeline||!s->images||!s->environment||!s->uniforms||!s->vertices||!cmd||!target||!size||(mask&~63u)) return qfalse;
    for(unsigned face=0;face<6;++face) if(mask&(1u<<face)) {
        ralRenderingInfo_t rendering={0};
        rendering.colorAttachments[0]=target; rendering.numColorAttachments=1;
        rendering.colorLoadOps[0]=RAL_LOAD_OP_CLEAR; rendering.colorStoreOps[0]=RAL_STORE_OP_STORE;
        rendering.colorAttachmentLayerIndices[0]=face;
        rendering.renderArea=(ralRect_t){0,0,size,size};
        rendering.debugName="sky-capture-face";
        Ral_BeginRendering(cmd,&rendering);
        Ral_CmdBindPipeline(cmd,s->pipeline);
        Ral_CmdBindBindGroup(cmd,0,s->environment);
        Ral_CmdBindBindGroup(cmd,1,s->images); Ral_CmdBindBindGroup(cmd,2,s->uniforms);
        Ral_CmdBindVertexBuffer(cmd,0,s->vertices,0);
        const ralViewport_t viewport={0,0,(float)size,(float)size,0,1};
        Ral_CmdSetViewport(cmd,&viewport); Ral_CmdSetScissor(cmd,&rendering.renderArea);
        renderProductDrawGpuParams_t draw={0};
        for(unsigned c=0;c<3;++c) {
            draw.viewForward[c]=wired_vk_sky_basis[face][0][c];
            draw.viewLeft[c]=-wired_vk_sky_basis[face][1][c];
            draw.viewUp[c]=wired_vk_sky_basis[face][2][c];
        }
        draw.viewForward[3]=draw.viewLeft[3]=1; draw.viewUp[3]=.18f;
        draw.projectionLightmap[0]=draw.projectionLightmap[1]=draw.projectionLightmap[2]=1;
        draw.viewportAlpha[0]=draw.viewportAlpha[1]=(float)size;
        draw.atmosphereHeightSky[2]=s->physical?1:0; draw.atmosphereHeightSky[3]=1;
        Ral_CmdPushConstants(cmd,RAL_STAGE_VERTEX|RAL_STAGE_FRAGMENT,0,sizeof(draw),&draw);
        Ral_CmdDraw(cmd,4,1,face*4,0);
        Ral_EndRendering(cmd);
    }
    return qtrue;
}
#endif
