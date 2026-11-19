// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
#include "../code/render/ral/backends/vulkan/renderer/vk_sky_capture.h"
#include "../code/render/ral/backends/vulkan/renderer/shaders/spirv/shader_data.c"
#include "vulkan/vulkan.h"
#include <stdio.h>

#define REQUIRE(x) do { if (!(x)) { fprintf(stderr,"sky GPU fixture:%d: %s\n",__LINE__,#x); goto done; } } while(0)

static float wired_test_half(uint16_t h) {
    unsigned exponent=(h>>10)&31u, fraction=h&1023u;
    float value = exponent == 31u ? (fraction ? NAN : INFINITY)
        : exponent ? ldexpf(1.0f+fraction/1024.0f,(int)exponent-15)
        : ldexpf((float)fraction,-24);
    return (h & 0x8000u) ? -value : value;
}

// Calls the production capture owner/draw functions. The expected image uses
// independent Q3 face orientation and scalar composition, not shader helpers.
qboolean wired_test_vulkan_sky_capture(ralBackend_t *backend) {
    wiredVkSkyCapture_t capture={0};
    ralTexture_t *faces[7]={0},*target=NULL;
    ralTextureView_t *views[7]={0};
    ralSampler_t *sampler=NULL;
    ralBuffer_t *readback=NULL;
    ralCommandBuffer_t *command=NULL;
    ralFence_t *fence=NULL;
    qboolean ok=qfalse;
    ralFrontendSkyMaterial_t packet={0};
    ralTextureCreateInfo_t ti={0};
    ralSamplerCreateInfo_t si={0};
    ralBufferCreateInfo_t bi={0};
    REQUIRE(wired_test_half(0x3c00u)==1 && wired_test_half(0xbc00u)==-1);
    REQUIRE(isnan(wired_test_half(0x7e00u)) && isinf(wired_test_half(0x7c00u)));
    ti.type=RAL_TEXTURE_2D; ti.format=RAL_FORMAT_R8G8B8A8_UNORM;
    ti.width=ti.height=4; ti.depthOrArrayLayers=ti.mipLevels=ti.sampleCount=1;
    ti.usage=RAL_TEXTURE_USAGE_SAMPLED|RAL_TEXTURE_USAGE_TRANSFER_DST;
    ti.memory=RAL_MEMORY_DEVICE_LOCAL;
    for(unsigned f=0;f<7;++f) {
        uint8_t rgba[64];
        for(unsigned y=0;y<4;++y) for(unsigned x=0;x<4;++x) {
            unsigned p=(y*4+x)*4;
            rgba[p]=(f+1)*24; rgba[p+1]=(x+1)*48; rgba[p+2]=(y+1)*48; rgba[p+3]=255;
            if(f==6) { rgba[p]=64; rgba[p+1]=128; rgba[p+2]=192; }
        }
        faces[f]=Ral_CreateTexture(backend,&ti); REQUIRE(faces[f]);
        ralTextureUploadDesc_t upload={0}; upload.data=rgba; upload.dataSize=sizeof(rgba);
        ralFence_t *uploaded=Ral_TextureUploadAsync(faces[f],&upload); REQUIRE(uploaded);
        Ral_WaitFence(uploaded,RAL_TIMEOUT_INFINITE);
        qboolean uploadedOK=Ral_FenceSignaled(uploaded); Ral_DestroyFence(uploaded); REQUIRE(uploadedOK);
        ralTextureViewCreateInfo_t vi={0}; vi.texture=faces[f]; vi.viewType=RAL_TEXTURE_2D;
        views[f]=Ral_CreateTextureView(backend,&vi); REQUIRE(views[f]);
    }
    si.minFilter=si.magFilter=RAL_FILTER_NEAREST;
    si.addressU=si.addressV=si.addressW=RAL_ADDRESS_CLAMP_TO_EDGE;
    sampler=Ral_CreateSampler(backend,&si); REQUIRE(sampler);
    ti.type=RAL_TEXTURE_CUBE; ti.format=RAL_FORMAT_R16G16B16A16_SFLOAT; ti.depthOrArrayLayers=6;
    ti.usage=RAL_TEXTURE_USAGE_COLOR_ATTACHMENT|RAL_TEXTURE_USAGE_TRANSFER_SRC;
    target=Ral_CreateTexture(backend,&ti); REQUIRE(target);
    bi.size=6*1024; bi.usage=RAL_BUFFER_TRANSFER_DST|RAL_BUFFER_MAP_READ; bi.memory=RAL_MEMORY_HOST_COHERENT;
    readback=Ral_CreateBuffer(backend,&bi); REQUIRE(readback);
    REQUIRE(wired_vk_sky_capture_init(&capture,backend,
        (const uint32_t*)ral_opengl_product_vert_spv,sizeof(ral_opengl_product_vert_spv),
        (const uint32_t*)ral_sky_capture_frag_spv,sizeof(ral_sky_capture_frag_spv)));
    packet.ready=qtrue; packet.params.meta[1]=1; packet.params.timeHeight[1]=512;
    for(unsigned i=0;i<8;++i) { packet.stageViews[i]=views[0]; packet.stageSamplers[i]=sampler; }
    for(unsigned cohort=0;cohort<3;++cohort) {
        memset(packet.params.stages,0,sizeof(packet.params.stages));
        packet.params.meta[0]=cohort?2:0;
        if(cohort) {
            packet.params.stages[0].meta[0]=packet.params.stages[0].meta[1]=1; // add
            packet.params.stages[0].meta[2]=2; // explicit white
            packet.params.stages[1].meta[0]=6; packet.params.stages[1].meta[1]=7; // src alpha
            packet.params.stages[1].meta[2]=2;
            packet.params.stages[1].generators[1]=9;
            packet.params.stages[1].constantColor[3]=128.0f/255.0f;
        }
        REQUIRE(wired_vk_sky_capture_prepare(&capture,backend,&packet,views,sampler,views[6],sampler,
            cohort==2,cohort+1,500));
        // Rejected replacement must preserve the current immutable cohort.
        packet.stageViews[7]=NULL;
        REQUIRE(!wired_vk_sky_capture_prepare(&capture,backend,&packet,views,sampler,views[0],sampler,
            qfalse,99,1000));
        packet.stageViews[7]=views[0]; REQUIRE(capture.productRevision==cohort+1);
        memset(packet.params.stages,0,sizeof(packet.params.stages)); // cannot alter frozen GPU bytes
        command=Ral_AcquireCommandBuffer(backend,RAL_QUEUE_GRAPHICS); fence=Ral_CreateFence(backend);
        REQUIRE(command&&fence);
        ralCommandReceipt_t recording,executable; ralSubmissionReceipt_t submitted;
        REQUIRE(Ral_BeginCommandBufferExact(command,&recording)==ralSuccess);
        Ral_CmdTransitionTexture(command,target,RAL_PIPELINE_STAGE_ALL_COMMANDS_BIT,
            RAL_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        // Three independently admitted groups exercise the same frozen packet.
        REQUIRE(wired_vk_sky_capture_draw(&capture,command,target,4,3));
        REQUIRE(wired_vk_sky_capture_draw(&capture,command,target,4,12));
        REQUIRE(wired_vk_sky_capture_draw(&capture,command,target,4,48));
        Ral_CmdTransitionTexture(command,target,RAL_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            RAL_PIPELINE_STAGE_TRANSFER_BIT,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
        for(unsigned f=0;f<6;++f) {
            ralBufferTextureCopy_t copy={0}; copy.bufferOffset=f*1024;
            copy.bytesPerRow=256; copy.rowsPerImage=4; copy.arrayLayer=f;
            copy.aspects=RAL_TEXTURE_ASPECT_COLOR; copy.imageRect.width=copy.imageRect.height=4;
            REQUIRE(Ral_CmdCopyTextureToBuffer(command,target,readback,&copy));
        }
        REQUIRE(Ral_EndCommandBufferExact(command,&recording,&executable)==ralSuccess);
        ralSubmitInfo_t submit={0}; submit.commandBuffers=&command; submit.numCommandBuffers=1; submit.signalFence=fence;
        REQUIRE(Ral_SubmitExact(backend,RAL_QUEUE_GRAPHICS,&submit,&executable,&submitted)==ralSuccess);
        Ral_WaitFence(fence,RAL_TIMEOUT_INFINITE); REQUIRE(Ral_FenceSignaled(fence));
        const uint8_t *bytes=Ral_MapBuffer(readback); REQUIRE(bytes);
        unsigned mismatches=0; float maxError=0;
        const unsigned faceMap[6]={0,2,1,3,4,5};
        for(unsigned f=0;f<6;++f) for(unsigned y=0;y<4;++y) for(unsigned x=0;x<4;++x) {
            unsigned u=y,v=x;
            if(f==1) {u=3-y;v=3-x;} else if(f==2) {u=x;v=3-y;} else if(f==3) {u=3-x;v=y;}
            float expected[4]={(faceMap[f]+1)*24/255.0f,(u+1)*48/255.0f,(v+1)*48/255.0f,1};
            if(cohort) for(unsigned c=0;c<3;++c) expected[c]=1+expected[c]*(127.0f/255.0f);
            if(cohort==2) {
                const float physical[3]={64/255.0f,128/255.0f,192/255.0f};
                const float weights[3]={.2126f,.7152f,.0722f};
                float authoredEnergy=0,physicalEnergy=0;
                for(unsigned c=0;c<3;++c) { authoredEnergy+=expected[c]*weights[c]; physicalEnergy+=physical[c]*weights[c]; }
                float scale=fminf(1.75f,fmaxf(.65f,physicalEnergy/fmaxf(authoredEnergy,.18f)));
                for(unsigned c=0;c<3;++c) {
                    float tint=fminf(2,fmaxf(.5f,physical[c]/physicalEnergy));
                    expected[c]=expected[c]*scale*(.88f+.12f*tint)+.08f*physical[c];
                }
            }
            for(unsigned c=0;c<4;++c) {
                uint16_t half; memcpy(&half,bytes+f*1024+y*256+x*8+c*2,2);
                float actual=wired_test_half(half);
                float error=fabsf(actual-expected[c]);
                if(error>maxError) maxError=error;
                if(!isfinite(actual)||error>.002f) ++mismatches;
            }
        }
        Ral_UnmapBuffer(readback);
        printf("Vulkan sky GPU cohort=%u mismatches=%u maxError=%g\n",cohort,mismatches,maxError);
        REQUIRE(!mismatches);
        Ral_DestroyFence(fence); fence=NULL; Ral_DestroyCommandBuffer(command); command=NULL;
    }
    ok=qtrue;
done:
    (void)Ral_WaitQueueIdle(backend,RAL_QUEUE_GRAPHICS);
    if(fence) Ral_DestroyFence(fence);
    if(command) Ral_DestroyCommandBuffer(command);
    wired_vk_sky_capture_shutdown(&capture);
    if(readback) Ral_DestroyBuffer(readback);
    if(target) Ral_DestroyTexture(target);
    if(sampler) Ral_DestroySampler(sampler);
    for(unsigned f=0;f<7;++f) { if(views[f]) Ral_DestroyTextureView(views[f]); if(faces[f]) Ral_DestroyTexture(faces[f]); }
    return ok;
}
