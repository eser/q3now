// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
#include "../code/render/ral/backends/metal/ral_metal_atmosphere.h"
#include "../code/render/ral/backends/metal/ral_metal_present.h"
#include "../code/render/frontend/render_submission_material.h"
#include "../code/render/frontend/render_atmosphere_gpu.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <vector>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x); return 1; } } while(0)
using V3=std::array<double,3>;
static double smooth(double a,double b,double x) {
    double t=std::clamp((x-a)/(b-a),0.0,1.0); return t*t*(3-2*t);
}
static double length(V3 p) { return std::sqrt(p[0]*p[0]+p[1]*p[1]+p[2]*p[2]); }
static V3 subtract(V3 a,const float *b) { for(unsigned c=0;c<3;++c)a[c]-=b[c]; return a; }
static double segmentDistance(V3 world,const float *start,const float *end) {
    V3 axis={},delta=subtract(world,start);
    for(unsigned c=0;c<3;++c) axis[c]=double(end[c])-start[c];
    const double span=length(axis);
    if(span==0) return length(delta);
    double along=0;
    for(unsigned c=0;c<3;++c) { axis[c]/=span; along+=delta[c]*axis[c]; }
    along=std::clamp(along,0.0,span);
    for(unsigned c=0;c<3;++c) delta[c]-=axis[c]*along;
    return length(delta);
}
// Float hash is intentionally evaluated at lattice corners. Geometry, density,
// colored lighting and optical integration below use a double precision oracle.
static double noise(V3 p) {
    int cell[3]; double f[3];
    for(unsigned c=0;c<3;++c) { cell[c]=(int)floor(p[c]); f[c]=smooth(0,1,p[c]-cell[c]); }
    double result=0;
    for(unsigned corner=0;corner<8;++corner) {
        float q[3]; double weight=1;
        for(unsigned c=0;c<3;++c) {
            unsigned bit=(corner>>c)&1; float v=(cell[c]+(int)bit)*.1031f;
            q[c]=v-floorf(v); weight*=bit?f[c]:1-f[c];
        }
        float d=q[0]*(q[1]+33.33f)+q[1]*(q[2]+33.33f)+q[2]*(q[0]+33.33f);
        float h=(q[0]+d+q[1]+d)*(q[2]+d); h-=floorf(h);
        result+=weight*h;
    }
    return result;
}
static int run(id<MTLDevice> device,id<MTLCommandQueue> queue) {
    auto gpu=RalMetal_AtmosphereCreate(device); CHECK(gpu);
    wiredMetalUploadCache cache={};
    // The fixed-capacity frontend is deliberately larger than a thread stack.
    static renderSubmissionState_t frontend; CHECK(RenderSubmission_Init(&frontend,1));
    const unsigned gx=5,gy=3,gz=16,count=gx*gy*gz;
    auto readback=[device newBufferWithLength:4*count*16 options:MTLResourceStorageModeShared]; CHECK(readback);
    uint64_t generation=1; unsigned checked=0,cloudCells=0,localCells=0,tubeOnlyCells=0;
    for(unsigned camera=0;camera<2;++camera) for(unsigned mode=0;mode<8;++mode) {
        atmosphereFrameState_t state={};
        state.schemaVersion=WIRED_ATMOSPHERE_SCHEMA_VERSION;
        state.flags=ATMOSPHERE_FLAG_ENABLED|ATMOSPHERE_FLAG_VOLUMETRIC_MEDIA|ATMOSPHERE_FLAG_SKY_LIGHTING;
        state.qualityTier=ATMOSPHERE_QUALITY_FULL; state.seed=216;
        state.bounds[0]=state.bounds[1]=state.bounds[2]=-128;
        state.bounds[3]=state.bounds[4]=state.bounds[5]=128;
        state.visibility=512; state.mediaDensity=.001f; state.mediaHeightFalloff=mode ? .003f:0;
        state.indoorExposure=1; state.cloudCover=mode==3?1:0;
        state.timelineSeconds=1; state.wind[0]=2;
        CHECK(RenderSubmission_SetAtmosphere(&frontend,&state));
        refdef_t view={}; view.width=gx*16; view.height=gy*16;
        view.fov_x=90; view.fov_y=70; view.rdflags=RDF_NOWORLDMODEL;
        const float angle=camera ? .65f:0;
        view.viewaxis[0][0]=cosf(angle); view.viewaxis[0][2]=sinf(angle);
        view.viewaxis[1][1]=1;
        view.viewaxis[2][0]=-sinf(angle); view.viewaxis[2][2]=cosf(angle);
        view.vieworg[0]=camera?45:0; view.vieworg[1]=camera?-31:0; view.vieworg[2]=camera?23:0;
        atmosphereMediaVolume_t volume={};
        volume.schemaVersion=WIRED_ATMOSPHERE_MEDIA_VOLUME_SCHEMA_VERSION;
        volume.id=1; volume.shape=mode==2?ATMOSPHERE_MEDIA_VOLUME_BOX:ATMOSPHERE_MEDIA_VOLUME_SPHERE;
        for(unsigned c=0;c<3;++c) {
            volume.origin[c]=view.vieworg[c]+view.viewaxis[0][c]*40+view.viewaxis[2][c]*12;
            volume.extent[c]=mode==2?16:0; volume.albedo[c]=.2f+.2f*c;
        }
        volume.radius=28; volume.extinction=.007f; volume.heightFalloff=.015f;
        volume.emissive[0]=.02f; volume.emissionIntensity=.2f;
        // Preserve the point fixtures. Also test a long segment, reversed
        // endpoints, a zero-length tube and a sub-millimetre tube.
        float lightStart[3],lightEnd[3];
        for(unsigned c=0;c<3;++c) {
            lightStart[c]=volume.origin[c];
            lightEnd[c]=lightStart[c]+(mode==4||mode==5
                ? view.viewaxis[0][c]*80+view.viewaxis[1][c]*32
                : mode==7 ? view.viewaxis[0][c]*.0001f : 0);
            if(mode==5) std::swap(lightStart[c],lightEnd[c]);
        }
        ralAtmospherePlanReceipt_t plan={}; plan.selectedTier=RAL_ATMOSPHERE_TIER_FULL;
        plan.froxelWidth=gx; plan.froxelHeight=gy; plan.froxelDepth=gz; plan.froxelCount=count;
        plan.admittedVolumeCount=(mode==1||mode==2)?1:0;
        plan.admittedLightCount=mode?1:0; plan.cloudsActive=mode==3?qtrue:qfalse;
        auto &sky=plan.skyEnvironment;
        sky.sunDirection[2]=1; sky.cloudSunOcclusion=.4f;
        const double albedo[3]={.72,.82,.92};
        for(unsigned c=0;c<3;++c) {
            sky.ambientSh[c]=.2f+.2f*c; sky.sunRadiance[c]=1.0f/(c+1);
            sky.moonRadiance[c]=.05f*c;
        }
        std::vector<double> previous(count*4);
        for(unsigned frame=0;frame<3;++frame) {
            // Second frame reuses first; third must ignore both history sections.
            plan.temporalReuseCount=frame==1?1:0;
            state.mediaDensity=.001f+.0004f*frame;
            CHECK(RenderSubmission_SetAtmosphere(&frontend,&state));
            CHECK(RenderSubmission_BeginFrame(&frontend,++generation));
            if(plan.admittedVolumeCount) CHECK(RenderSubmission_AddAtmosphereMediaVolume(&frontend,&volume));
            if(mode) CHECK(RenderSubmission_AddLight(&frontend,lightStart,mode>=4?lightEnd:NULL,30,1,.3f,.05f));
            CHECK(RenderSubmission_RenderScene(&frontend,&view,0));
            renderAtmosphereGpuFrame_t portable;
            CHECK(RenderAtmosphereGpu_Build(&frontend,&plan,WIRED_METAL_ATMOSPHERE_CAPACITY,
                frame!=0?qtrue:qfalse,&portable));
            id<MTLCommandBuffer> command=[queue commandBuffer]; CHECK(command);
            wiredMetalUpload upload(cache);
            if(camera==0 && mode==0 && frame==0) {
                auto invalid=plan; invalid.froxelWidth=UINT32_MAX;
                CHECK(!RalMetal_AtmosphereEncode(gpu,command,&frontend,&invalid,false,false,upload));
                CHECK(!upload.buffer);
                invalid=plan; invalid.admittedVolumeCount=1;
                CHECK(!RalMetal_AtmosphereEncode(gpu,command,&frontend,&invalid,false,false,upload));
                CHECK(!upload.buffer);
                invalid=plan; invalid.froxelDepth=RAL_ATMOSPHERE_MAX_SLICES+1;
                CHECK(!RalMetal_AtmosphereEncode(gpu,command,&frontend,&invalid,false,false,upload));
                CHECK(!upload.buffer);
                CHECK(!RenderAtmosphereGpu_Build(&frontend,&invalid,WIRED_METAL_ATMOSPHERE_CAPACITY,qfalse,&portable));
                CHECK(RenderAtmosphereGpu_Build(&frontend,&plan,WIRED_METAL_ATMOSPHERE_CAPACITY,qfalse,&portable));
            }
            CHECK(RalMetal_AtmosphereEncode(gpu,command,&frontend,&plan,frame!=0,frame==1,upload));
            // Independent portable packet must exactly match the five production
            // uploads whose GPU output is checked by the optical oracle below.
            const unsigned char *packet=(const unsigned char *)upload.buffer.contents;
            CHECK(!memcmp(packet,&portable.inject,sizeof(portable.inject)));
            CHECK(!memcmp(packet+256,&portable.light,sizeof(portable.light)));
            CHECK(!memcmp(packet+512,&portable.cloud,sizeof(portable.cloud)));
            CHECK(!memcmp(packet+768,&portable.integrate,sizeof(portable.integrate)));
            CHECK(!memcmp(packet+1024,&portable.tile,sizeof(portable.tile)));
            auto blit=[command blitCommandEncoder]; CHECK(blit);
            for(unsigned section=0;section<4;++section)
                [blit copyFromBuffer:RalMetal_AtmosphereArena(gpu)
                    sourceOffset:section*WIRED_METAL_ATMOSPHERE_SECTION_BYTES
                    toBuffer:readback destinationOffset:section*count*16 size:count*16];
            [blit endEncoding]; [command commit]; upload.wired_publish(command);
            [command waitUntilCompleted]; CHECK(command.status==MTLCommandBufferStatusCompleted);
            auto actual=(const float *)readback.contents;
            std::vector<double> expected(count*4);
            for(unsigned y=0;y<gy;++y) for(unsigned x=0;x<gx;++x) {
                double transmission=1,radiance[3]={};
                for(unsigned z=0;z<gz;++z) {
                    const unsigned index=((z*gy+y)*gx+x)*4;
                    double d=4*pow(128.0,(z+.5)/gz),w[4]={},illum[3]; V3 world={};
                    for(unsigned c=0;c<3;++c) world[c]=view.vieworg[c]+d*(view.viewaxis[0][c]
                        -view.viewaxis[1][c]*(2*(x+.5)/gx-1)
                        -view.viewaxis[2][c]*tan(70*M_PI/360)*(2*(y+.5)/gy-1));
                    w[3]=state.mediaDensity*exp(-state.mediaHeightFalloff*std::max(world[2],0.0));
                    for(unsigned c=0;c<3;++c) w[c]=w[3]*albedo[c];
                    if(plan.admittedVolumeCount) {
                        V3 delta=subtract(world,volume.origin);
                        double weight=mode==2?1-smooth(.85,1,std::max({fabs(delta[0]),fabs(delta[1]),fabs(delta[2])})/16)
                            :1-smooth(28*.85,28,length(delta));
                        weight*=exp(-volume.heightFalloff*std::max(delta[2],0.0));
                        w[3]+=volume.extinction*weight;
                        for(unsigned c=0;c<3;++c) w[c]+=weight*(volume.extinction*volume.albedo[c]
                            +volume.emissive[c]*volume.emissionIntensity);
                    }
                    for(unsigned c=0;c<4;++c) CHECK(fabs(actual[index+c]-w[c])<.00001);
                    double distance=mode>=4?segmentDistance(world,lightStart,lightEnd)
                        :length(subtract(world,lightStart));
                    double local=mode?pow(std::max(1-distance/30,0.0),2):0;
                    if(local>0) ++localCells;
                    if(mode>=4 && local>0 && length(subtract(world,lightStart))>30) ++tubeOnlyCells;
                    const double localColor[3]={1,.3,.05};
                    for(unsigned c=0;c<3;++c) {
                        illum[c]=sky.ambientSh[c]+sky.sunRadiance[c]*(1-sky.cloudSunOcclusion)
                            +sky.moonRadiance[c]+local*localColor[c]; w[c]*=illum[c];
                    }
                    for(unsigned c=0;c<4;++c) {
                        if(fabs(actual[count*4+index+c]-w[c])>=.00002) {
                            fprintf(stderr,"lit camera=%u mode=%u frame=%u xyz=%u,%u,%u ch=%u actual=%g expected=%g distance=%g\n",
                                camera,mode,frame,x,y,z,c,actual[count*4+index+c],w[c],distance);
                            return 1;
                        }
                    }
                    if(mode==3) {
                        double base=view.vieworg[2]+96,top=view.vieworg[2]+384,span=top-base;
                        double envelope=smooth(base,base+span*.18,world[2])*(1-smooth(top-span*.18,top,world[2]));
                        V3 p=world; for(unsigned c=0;c<3;++c) p[c]=(p[c]+state.wind[c]*state.timelineSeconds)/(span*.32);
                        double extinction=smooth(.30,.52,noise(p))*envelope*.012;
                        V3 ray=subtract(world,view.vieworg);
                        double phase=pow(std::max(ray[2]/length(ray),0.0),4);
                        const double cloudAlbedo[3]={.92,.95,1};
                        for(unsigned c=0;c<3;++c) w[c]+=extinction*cloudAlbedo[c]*(sky.ambientSh[c]
                            +sky.sunRadiance[c]*exp(-extinction*span*.18)*(.35+.65*phase)*(1-sky.cloudSunOcclusion));
                        w[3]+=extinction; if(extinction>.0001) ++cloudCells;
                        for(unsigned c=0;c<4;++c) CHECK(fabs(actual[count*8+index+c]-w[c])<.00004);
                    }
                    double step=4*(pow(128.0,double(z+1)/gz)-pow(128.0,double(z)/gz));
                    double t=exp(-w[3]*step);
                    for(unsigned c=0;c<3;++c) radiance[c]+=w[c]*transmission*(1-t)/std::max(w[3],.00001);
                    transmission*=t;
                    for(unsigned c=0;c<4;++c) {
                        expected[index+c]=c==3?transmission:radiance[c];
                        if(frame==1) expected[index+c]=.1*expected[index+c]+.9*previous[index+c];
                        double got=actual[count*12+index+c];
                        if(!std::isfinite(got)||fabs(got-expected[index+c])>.001) {
                            fprintf(stderr,"camera=%u mode=%u frame=%u xyz=%u,%u,%u ch=%u expected=%g got=%g\n",
                                camera,mode,frame,x,y,z,c,expected[index+c],got); return 1;
                        }
                    }
                    ++checked;
                }
            }
            previous=expected;
            renderSubmissionReceipt_t receipt;
            CHECK(RenderSubmission_EndFrame(&frontend,generation,&receipt));
        }
        RenderSubmission_ClearAtmosphereMediaVolumes(&frontend);
    }
    CHECK(localCells>0 && cloudCells>0 && tubeOnlyCells>0);
    // The command owns every referenced resource even if its producer is retired
    // before submission; a discarded command must not publish history/cache.
    CHECK(RenderSubmission_BeginFrame(&frontend,++generation));
    refdef_t view={}; view.width=80;view.height=48;view.fov_x=view.fov_y=90;
    view.viewaxis[0][0]=view.viewaxis[1][1]=view.viewaxis[2][2]=1;view.rdflags=RDF_NOWORLDMODEL;
    CHECK(RenderSubmission_RenderScene(&frontend,&view,0));
    ralAtmospherePlanReceipt_t plan={};plan.selectedTier=RAL_ATMOSPHERE_TIER_FULL;
    plan.froxelWidth=gx;plan.froxelHeight=gy;plan.froxelDepth=gz;plan.froxelCount=count;
    auto command=[[queue commandBuffer] retain];
    {
        wiredMetalUpload upload(cache);
        CHECK(RalMetal_AtmosphereEncode(gpu,command,&frontend,&plan,false,false,upload));
    }
    RalMetal_AtmosphereDestroy(gpu); cache.wired_clear();
    [command commit];[command waitUntilCompleted];CHECK(command.status==MTLCommandBufferStatusCompleted);
    [command release]; [readback release]; RenderSubmission_Reset(&frontend);
    printf("Metal canonical atmosphere: 48 cases, %u froxels; local=%u cloud=%u tube-only=%u; history/owner retirement PASS\n",
        checked,localCells,cloudCells,tubeOnlyCells);
    return 0;
}
static int surfaceAndLoading() {
    ralMetalCoreCreateInfo_t info={1}; ralMetalCore_t *core=NULL;
    ralMetalCoreReceipt_t coreReceipt; ralMetalPresent_t *present=NULL;
    ralMetalPresentLayerReceipt_t layer; ralMetalDrawableReceipt_t drawable;
    ralMetalPresentReceipt_t receipt; renderSubmissionReceipt_t submission;
    static renderSubmissionState_t frontend;
    CHECK(RalMetal_CoreCreate(&info,&core,&coreReceipt));
    CHECK(RenderSubmission_Init(&frontend,1));
    ralSurfaceFormat_t format={RAL_FORMAT_B8G8R8A8_UNORM,RAL_COLORSPACE_SRGB_NONLINEAR};
    ralPresentPreference_t preference={RAL_PRESENT_FIFO,3,3};
    ralSwapchainCreateInfo_t create={}; create.desiredWidth=create.desiredHeight=16; // offscreen
    create.formatPreferences=&format;create.formatPreferenceCount=1;
    create.presentPreferences=&preference;create.presentPreferenceCount=1;
    create.requiredUsage=RAL_TEXTURE_USAGE_COLOR_ATTACHMENT;
    CHECK(RalMetal_PresentCreate(core,&coreReceipt,&create,2,&present,&layer));
    CHECK(RalMetal_PresentSetToneMap(present,0,1,1.6f,.977f,.18f,.267f,8));
    const byte texel[4]={0,0,0,255}; const float clear[4]={0,0,0,1};
    qhandle_t material=RenderSubmission_RegisterMaterialImage(&frontend,RENDER_ASSET_MATERIAL,
        "test/additive-fog-black",qtrue,texel,1,1); CHECK(material>0);
    const byte white[4]={255,255,255,255};
    qhandle_t luminous=RenderSubmission_RegisterMaterialImage(&frontend,RENDER_ASSET_MATERIAL,
        "test/additive-fog-white",qtrue,white,1,1); CHECK(luminous>0);
    atmosphereFrameState_t atmosphere={}; atmosphere.schemaVersion=WIRED_ATMOSPHERE_SCHEMA_VERSION;
    atmosphere.flags=ATMOSPHERE_FLAG_ENABLED|ATMOSPHERE_FLAG_VOLUMETRIC_MEDIA|ATMOSPHERE_FLAG_SKY_LIGHTING;
    atmosphere.seed=216; atmosphere.indoorExposure=1;atmosphere.visibility=128;atmosphere.mediaDensity=.05f;
    for(unsigned c=0;c<3;++c) {
        atmosphere.bounds[c]=-64;atmosphere.bounds[c+3]=64;atmosphere.ambientColor[c]=.5f;
    }
    refdef_t view={};view.width=view.height=16;view.fov_x=view.fov_y=90;
    view.viewaxis[0][0]=view.viewaxis[1][1]=view.viewaxis[2][2]=1;
    uint64_t generation=4;
    for(unsigned full=0;full<2;++full) for(unsigned mode=0;mode<3;++mode)
    for(unsigned emission=0;emission<2;++emission) {
        atmosphere.qualityTier=full?ATMOSPHERE_QUALITY_FULL:ATMOSPHERE_QUALITY_ANALYTIC;
        CHECK(RenderSubmission_SetAtmosphere(&frontend,&atmosphere));
        auto alpha=mode==0?RENDER_ALPHA_OPAQUE:mode==1?RENDER_ALPHA_ADDITIVE:RENDER_ALPHA_ALPHA_ADDITIVE;
        qhandle_t selected=emission?luminous:material;
        CHECK(RenderSubmission_SetMaterialRasterPolicy(&frontend,selected,alpha,0,qfalse));
        CHECK(RenderSubmission_BeginFrame(&frontend,generation));
        refEntity_t sprite={}; sprite.reType=RT_SPRITE;sprite.customShader=selected;
        sprite.origin[0]=32;sprite.radius=32;memset(sprite.shader.rgba,255,4);
        CHECK(RenderSubmission_AddEntity(&frontend,&sprite,NULL));
        CHECK(RenderSubmission_RenderScene(&frontend,&view,0));
        CHECK(RalMetal_PresentAcquire(present,&coreReceipt,&layer,generation-1,&drawable));
        // Request this frame's completed pixels, not the asynchronous telemetry
        // ring's most recently completed (possibly previous-frame) sample.
        CHECK(RalMetal_PresentRequestCapture(present));
        CHECK(RalMetal_PresentClearAndSubmit(present,&coreReceipt,&layer,&drawable,&frontend,clear,generation,&receipt));
        CHECK(receipt.atmosphereDispatchCount==(full?4u:0u));
        CHECK(receipt.loweredEntityIndexCount==6);
        fprintf(stderr,"atmosphere surface full=%u mode=%u emission=%u rgb=%u,%u,%u\n",full,mode,emission,
            receipt.readbackBytes[0],receipt.readbackBytes[1],receipt.readbackBytes[2]);
        for(unsigned c=0;c<3;++c) {
            if(!emission) CHECK(mode?receipt.readbackBytes[c]<=1:receipt.readbackBytes[c]>10);
            // Opaque emissive FX survive, but do not bypass medium extinction at
            // their own depth. Translucent (no depth write) sprites draw after the
            // opaque scene and take the media at the opaque depth BEHIND them, as
            // Vulkan's post-scene composite fogs the blended pixel: over cleared
            // depth that is the far medium, which extinguishes the emission.
            else if(mode) CHECK(receipt.readbackBytes[c]<=1);
            else CHECK(receipt.readbackBytes[c]>10 && receipt.readbackBytes[c]<220);
        }
        CHECK(RenderSubmission_EndFrame(&frontend,generation,&submission));generation+=2;
    }
    // A FULL environment persists across loading, but UI-only frames must not
    // dispatch against the old camera or poison renderer initialization.
    CHECK(RenderSubmission_BeginFrame(&frontend,generation));
    CHECK(RenderSubmission_AddUiQuad(&frontend,0,0,16,16,0,0,1,1,0,material));
    CHECK(RalMetal_PresentAcquire(present,&coreReceipt,&layer,generation-1,&drawable));
    CHECK(RalMetal_PresentClearAndSubmit(present,&coreReceipt,&layer,&drawable,&frontend,clear,generation,&receipt));
    CHECK(receipt.atmosphereDispatchCount==0 && receipt.weatherDispatchCount==0 && receipt.loweredUiPrimitiveCount==1);
    CHECK(frontend.atmosphere.qualityTier==ATMOSPHERE_QUALITY_FULL && frontend.atmosphere.mediaDensity==.05f);
    CHECK(RenderSubmission_EndFrame(&frontend,generation,&submission));
    generation+=2;
    // Two simultaneous semantic emitters must sample their own authored images,
    // including transparent texels. A white placeholder or slot-zero broadcast
    // makes at least one of these pixels wrong.
    atmosphere.qualityTier=ATMOSPHERE_QUALITY_WEATHER;
    atmosphere.mediaDensity=0;atmosphere.visibility=0;atmosphere.temperatureC=-10;
    atmosphere.flags=ATMOSPHERE_FLAG_ENABLED;
    memset(atmosphere.ambientColor,0,sizeof(atmosphere.ambientColor));
    CHECK(RenderSubmission_SetAtmosphere(&frontend,&atmosphere));
    for(unsigned transparent=0;transparent<2;++transparent) {
        CHECK(RenderSubmission_BeginFrame(&frontend,generation));
        for(unsigned slot=0;slot<2;++slot) {
            const unsigned recipeId=slot+1+transparent*2;
            byte color[4]={0,0,0,(byte)(transparent?0:255)};
            color[slot]=255;
            char name[64];snprintf(name,sizeof(name),"test/weather-%u-%u",slot,transparent);
            qhandle_t shader=RenderSubmission_RegisterMaterialImage(&frontend,RENDER_ASSET_MATERIAL,
                name,qtrue,color,1,1); CHECK(shader>0);
            particleClass_t recipe={};recipe.shader=shader;recipe.paletteCount=1;
            recipe.lifetimeMean=1;recipe.sizeStart=recipe.sizeEnd=8;
            for(unsigned c=0;c<4;++c)recipe.colorPalette[0][c]=1;
            CHECK(RenderSubmission_RegisterParticleClass(&frontend,recipeId,&recipe));
            atmosphereEffectProfile_t profile={};
            profile.schemaVersion=WIRED_ATMOSPHERE_EFFECT_PROFILE_SCHEMA_VERSION;
            profile.stageCount=profile.maxParticles=1;profile.duration=1;
            profile.lodFar=512;profile.boundsRadius=32;
            auto &stage=profile.stages[0];stage.trigger=ATMOSPHERE_STAGE_CONTINUOUS;
            stage.particleClass=recipeId;stage.parentStage=UINT32_MAX;stage.maxParticles=1;
            stage.spawnRate=1;stage.duration=1;stage.lodFar=512;
            stage.boundsRadius=32;stage.intensityScale=1;
            CHECK(RenderSubmission_RegisterAtmosphereEffectProfile(&frontend,recipeId,&profile));
            atmosphereEmitter_t emitter={};emitter.schemaVersion=WIRED_ATMOSPHERE_SCHEMA_VERSION;
            emitter.kind=ATMOSPHERE_EMITTER_BREATH;emitter.id=slot+1;emitter.seed=216;
            emitter.profile=recipeId;emitter.intensity=1;emitter.direction[0]=1;
            emitter.temperatureC=37;emitter.humidity=1;
            emitter.origin[0]=32;emitter.origin[1]=slot?16:-16;
            CHECK(RenderSubmission_AddAtmosphereEmitter(&frontend,&emitter));
        }
        CHECK(RenderSubmission_RenderScene(&frontend,&view,0));
        CHECK(RalMetal_PresentAcquire(present,&coreReceipt,&layer,generation-1,&drawable));
        CHECK(RalMetal_PresentRequestCapture(present));
        CHECK(RalMetal_PresentClearAndSubmit(present,&coreReceipt,&layer,&drawable,&frontend,clear,generation,&receipt));
        CHECK(receipt.weather.semanticEmitterCount==2 && receipt.weather.semanticParticleCount==2);
        uint32_t width=0,height=0;
        const byte *rgb=RalMetal_PresentCaptureRgb(present,&width,&height);
        CHECK(rgb && width==16 && height==16);
        for(unsigned slot=0;slot<2;++slot) for(unsigned c=0;c<3;++c) {
            unsigned actual=rgb[(8*width+(slot?4:12))*3+c];
            CHECK(transparent || c!=slot ? actual<=1 : actual>=250);
        }
        CHECK(RenderSubmission_EndFrame(&frontend,generation,&submission));generation+=2;
    }
    RalMetal_PresentDestroy(present);RenderSubmission_Reset(&frontend);RalMetal_CoreDestroy(core);
    puts("Metal Product: analytic/FULL additive radiance + UI-only loading + weather material/alpha PASS");return 0;
}
int main() { @autoreleasepool {
    id<MTLDevice> device=MTLCreateSystemDefaultDevice(); CHECK(device);
    id<MTLCommandQueue> queue=[device newCommandQueue]; CHECK(queue);
    int result=run(device,queue); if(!result)result=surfaceAndLoading();
    [queue release]; [device release]; return result;
} }
