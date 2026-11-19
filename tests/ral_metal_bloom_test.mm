// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
#include "../code/render/ral/backends/metal/ral_metal_bloom.h"
#include <vector>
#include <stdio.h>
#include <string.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL %d: %s\n", __LINE__, #x); return 1; } } while(0)
static float wired_half(uint16_t h) {
    const int exponent = (h >> 10) & 31;
    float value = exponent ? ldexpf(1.0f + (h & 1023)/1024.0f, exponent-15)
        : ldexpf((float)(h & 1023), -24);
    return (h & 32768) ? -value : value;
}
static id<MTLTexture> wired_scene(id<MTLDevice> d, unsigned w, unsigned h) {
    auto desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA16Float
        width:w height:h mipmapped:NO];
    desc.storageMode = MTLStorageModeShared;
    desc.usage = MTLTextureUsageShaderRead | MTLTextureUsageRenderTarget;
    return [d newTextureWithDescriptor:desc];
}
int main() {
    @autoreleasepool {
        ralBloomSettings_t s = {1u,2u,0u,.32f,.5f};
        ralBloomPlan_t p, before;
        memset(&p, 0xa5, sizeof(p)); before = p;
        CHECK(!Ral_BloomPlanBuild(&s,0,18,&p) && !memcmp(&p,&before,sizeof(p)));
        s.threshold = NAN;
        CHECK(!Ral_BloomPlanBuild(&s,32,18,&p) && !memcmp(&p,&before,sizeof(p)));
        s.threshold = .32f;
        CHECK(Ral_BloomPlanBuild(&s,17,9,&p));
        CHECK(p.levels==2 && p.passCount==5 && p.width[1]==8 && p.height[2]==2);
        id<MTLDevice> device = MTLCreateSystemDefaultDevice(); CHECK(device);
        id<MTLCommandQueue> queue = [device newCommandQueue]; CHECK(queue);
        ralMetalBloom_t *bloom = RalMetal_BloomCreate(device); CHECK(bloom);
        for (unsigned shape=0; shape<3; ++shape) {
            unsigned w = shape==0 ? 32 : shape==1 ? 17 : 1;
            unsigned h = shape==0 ? 18 : shape==1 ? 9 : 1;
            id<MTLTexture> scene = wired_scene(device,w,h); CHECK(scene);
            std::vector<uint16_t> input(w*h*4), output(w*h*4);
            for (unsigned i=0;i<w*h;++i) {
                input[i*4]=0x3c00; input[i*4+1]=0x3800; input[i*4+2]=0x3400; input[i*4+3]=0x3c00;
            }
            for (unsigned mode=0;mode<3;++mode) {
                s.thresholdMode=mode; s.levels=shape==2 ? 4 : 2;
                for (unsigned enabled=0;enabled<2;++enabled) {
                    s.enabled=enabled;
                    [scene replaceRegion:MTLRegionMake2D(0,0,w,h) mipmapLevel:0
                        withBytes:input.data() bytesPerRow:w*8];
                    id<MTLCommandBuffer> cb=[queue commandBuffer];
                    CHECK(RalMetal_BloomEncode(bloom,cb,scene,&s));
                    [cb commit]; [cb waitUntilCompleted]; CHECK(cb.status==MTLCommandBufferStatusCompleted);
                    [scene getBytes:output.data() bytesPerRow:w*8 fromRegion:MTLRegionMake2D(0,0,w,h) mipmapLevel:0];
                    if (!enabled) CHECK(input==output);
                    const float base[3]={1,.5f,.25f};
                    float metric = mode==1 ? 1.75f/3.0f : .2126f+.7152f*.5f+.0722f*.25f;
                    for (unsigned i=0;i<w*h;++i) for(unsigned ch=0;ch<3;++ch) {
                        float excess = mode ? base[ch]*fmaxf(metric-s.threshold,0)/metric : fmaxf(base[ch]-s.threshold,0);
                        float expected = base[ch] + enabled*excess*s.levels*s.intensity*WIRED_BLOOM_ENERGY_SCALE;
                        CHECK(fabsf(wired_half(output[i*4+ch])-expected)<.018f);
                    }
                }
            }
            [scene release];
        }
        /* Nonuniform, asymmetric bright patch: spread must stay near the source,
         * not vertically mirror it. Reconfigure while a real queue gate holds
         * the old cohort, then destroy the owner before either command runs. */
        id<MTLTexture> a=wired_scene(device,64,32), b=wired_scene(device,17,9);
        CHECK(a && b);
        std::vector<uint16_t> pixels(64*32*4,0), result(pixels.size());
        for(unsigned y=2;y<6;++y) for(unsigned x=2;x<6;++x)
            pixels[(y*64+x)*4]=pixels[(y*64+x)*4+1]=pixels[(y*64+x)*4+2]=0x4400; // 4.0
        [a replaceRegion:MTLRegionMake2D(0,0,64,32) mipmapLevel:0 withBytes:pixels.data() bytesPerRow:64*8];
        std::vector<uint16_t> zeros(17*9*4,0);
        [b replaceRegion:MTLRegionMake2D(0,0,17,9) mipmapLevel:0 withBytes:zeros.data() bytesPerRow:17*8];
        id<MTLSharedEvent> gate=[device newSharedEvent]; CHECK(gate);
        id<MTLCommandBuffer> first=[queue commandBuffer];
        [first encodeWaitForEvent:gate value:1];
        s={1u,2u,0u,.32f,.5f};
        CHECK(RalMetal_BloomEncode(bloom,first,a,&s)); [first commit];
        id<MTLCommandBuffer> second=[queue commandBuffer];
        s.levels=3;
        CHECK(RalMetal_BloomEncode(bloom,second,b,&s)); [second commit];
        RalMetal_BloomDestroy(bloom);
        CHECK(gate.signaledValue==0);
        gate.signaledValue=1;
        [second waitUntilCompleted]; CHECK(first.status==MTLCommandBufferStatusCompleted);
        CHECK(second.status==MTLCommandBufferStatusCompleted);
        [a getBytes:result.data() bytesPerRow:64*8 fromRegion:MTLRegionMake2D(0,0,64,32) mipmapLevel:0];
        CHECK(wired_half(result[(4*64+8)*4])>.01f);
        CHECK(wired_half(result[(4*64+8)*4])>wired_half(result[(27*64+8)*4])+.01f);
        [b getBytes:zeros.data() bytesPerRow:17*8 fromRegion:MTLRegionMake2D(0,0,17,9) mipmapLevel:0];
        for(unsigned i=0;i<17*9;++i) CHECK(zeros[i*4]==0 && zeros[i*4+1]==0 && zeros[i*4+2]==0);
        [gate release]; [a release]; [b release]; [queue release]; [device release];
    }
    fprintf(stderr,"PASS bloom: GPU on/off, three metrics, odd/tiny extents, asymmetric spread, in-flight resize/destroy\n");
    return 0;
}
