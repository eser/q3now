// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors
#include "../code/render/ral/backends/vulkan/renderer/vk_sky_material.h"
#include <assert.h>
#include <stdio.h>

struct ralTextureView_s { int identity; };
struct ralSampler_s { int identity; };
typedef struct { struct ralTextureView_s *ralDescriptorView;
    struct ralSampler_s *ralDescriptorSampler; } image_t;
static struct ralTextureView_s views[3]={{11},{22},{33}};
static struct ralSampler_s samplers[3]={{7},{8},{9}};
static image_t images[3];
static struct { image_t *whiteImage; } tr;
static int s_frontendSubmission;
static renderMaterialSnapshot_t active;
static qboolean present;
static unsigned lookups;
typedef struct { int timeMs; } renderWorldSnapshot_t;
static qboolean worldReady;
static int sceneTimeMs;
static cvar_t shaderPin;
static cvar_t *r_pinShaderTime = &shaderPin;
static qboolean RenderSubmission_WorldSnapshot(const int *state, renderWorldSnapshot_t *world) {
    (void)state;
    if (!worldReady) return qfalse;
    world->timeMs = sceneTimeMs; return qtrue;
}
static qboolean RenderSubmission_ActiveSkyMaterial(const int *state,
        renderMaterialSnapshot_t *material, char name[MAX_QPATH]) {
    (void)state;
    if (!present) return qfalse;
    *material=active; strcpy(name,"test/sky"); return qtrue;
}
static qboolean RenderSubmission_MaterialStagesUseTime(const renderMaterialSnapshot_t *m) {
    assert(m->digest==active.digest); return m->stages[0].animationFrameCount>1;
}
static image_t *FrontendImageForTextureHandle(qhandle_t handle) {
    ++lookups;
    if (handle==41) return &images[1];
    if (handle==42) return &images[2];
    return NULL;
}
#include "vk_sky_material_packet_impl.inc"

int main(void) {
    int captureTime = -123, sceneTime = -456;
    assert(!R_FrontendSkyCaptureTime(NULL, &sceneTime));
    assert(!R_FrontendSkyCaptureTime(&captureTime, NULL));
    assert(!R_FrontendSkyCaptureTime(&captureTime, &sceneTime) && captureTime == -123 && sceneTime == -456);
    worldReady = qtrue; sceneTimeMs = 500;
    assert(R_FrontendSkyCaptureTime(&captureTime, &sceneTime) && captureTime == 500 && sceneTime == 500);
    shaderPin.value = 1.0f;
    for (sceneTimeMs = 1000; sceneTimeMs < 10000; sceneTimeMs += 1000)
        assert(R_FrontendSkyCaptureTime(&captureTime, &sceneTime) && captureTime == 1000 && sceneTime == sceneTimeMs);
    shaderPin.value = 0;
    sceneTimeMs = 125; // rewind follows the submitted scene, not OS time
    assert(R_FrontendSkyCaptureTime(&captureTime, &sceneTime) && captureTime == 125 && sceneTime == 125);
    shaderPin.value = INFINITY;
    assert(!R_FrontendSkyCaptureTime(&captureTime, &sceneTime) && captureTime == 125 && sceneTime == 125);
    shaderPin.value = 0;
    worldReady = qfalse;
    assert(!R_FrontendSkyCaptureTime(&captureTime, &sceneTime) && captureTime == 125 && sceneTime == 125);
    for(unsigned i=0;i<3;++i) {
        images[i].ralDescriptorView=&views[i]; images[i].ralDescriptorSampler=&samplers[i];
    }
    tr.whiteImage=&images[0];
    ralFrontendSkyMaterial_t packet, previous;
    memset(&packet,0xa5,sizeof(packet));
    assert(!R_FrontendSkyMaterialPacket(0,NULL));
    assert(R_FrontendSkyMaterialPacket(0,&packet) && !packet.ready && lookups==0);
    active.ready=qtrue; active.sky=qtrue; active.handle=17; active.digest=99;
    active.skyCloudHeight=512; active.stageCount=2;
    active.stages[0].material=41;
    active.stages[0].animationFrameCount=2;
    active.stages[0].animationFrequency=2;
    active.stages[0].animationLooping=qtrue;
    active.stages[0].animationMaterials[0]=41;
    active.stages[0].animationMaterials[1]=42;
    active.stages[1].imageSource=2; // explicit $whiteimage, no resident handle
    present=qtrue;
    const struct { int clock,index; } cases[]={{0,1},{499,1},{500,2},{500,2},{1000,1},{-10,1}};
    for(unsigned c=0;c<sizeof(cases)/sizeof(cases[0]);++c) {
        lookups=0;
        assert(R_FrontendSkyMaterialPacket(cases[c].clock,&packet));
        assert(packet.ready && packet.animated && packet.revision==99);
        assert(packet.params.meta[0]==2 && packet.params.timeHeight[1]==512);
        assert(packet.stageViews[0]==&views[cases[c].index]);
        assert(packet.stageSamplers[0]==&samplers[cases[c].index]);
        assert(lookups==1); // no lookup/decoding for unused or explicit-white slots
        for(unsigned i=1;i<RENDER_MATERIAL_MAX_STAGES;++i) {
            assert(packet.stageViews[i]==&views[0]);
            assert(packet.stageSamplers[i]==&samplers[0]);
        }
    }
    previous=packet;
    images[2].ralDescriptorSampler=NULL;
    assert(!R_FrontendSkyMaterialPacket(500,&packet));
    assert(!memcmp(&packet,&previous,sizeof(packet)));
    images[2].ralDescriptorSampler=&samplers[2];
    images[2].ralDescriptorView=NULL;
    assert(!R_FrontendSkyMaterialPacket(500,&packet));
    assert(!memcmp(&packet,&previous,sizeof(packet)));
    images[2].ralDescriptorView=&views[2];
    active.stages[0].animationMaterials[1]=77; // no white substitution
    assert(!R_FrontendSkyMaterialPacket(500,&packet));
    assert(!memcmp(&packet,&previous,sizeof(packet)));
    active.stages[0].animationMaterials[1]=42;
    active.stages[1].imageSource=1; // no fake sky lightmap
    assert(!R_FrontendSkyMaterialPacket(500,&packet));
    assert(!memcmp(&packet,&previous,sizeof(packet)));
    active.stages[1].imageSource=2;
    tr.whiteImage=NULL;
    assert(!R_FrontendSkyMaterialPacket(500,&packet));
    assert(!memcmp(&packet,&previous,sizeof(packet)));
    puts("Vulkan sky GPU packet: frozen clock, exact view/sampler, white semantics, atomic rejection PASS");
    return 0;
}
