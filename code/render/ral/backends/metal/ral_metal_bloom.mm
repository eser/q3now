// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
#include "ral_metal_bloom.h"
#include "ral_metal_ui_metallib.h"
#include "ral_metal_bloom_metallib.h"
#include "ral_metal_downsample_metallib.h"
#include "ral_metal_upsample_metallib.h"
#include "ral_metal_bloom_composite_metallib.h"
#include <stdlib.h>

struct wiredBloomCohort {
    ralBloomSettings_t settings;
    ralBloomPlan_t plan;
    id<MTLTexture> scene, mip[1u + WIRED_BLOOM_MAX_LEVELS];
    id<MTLBuffer> args[2u + WIRED_BLOOM_MAX_LEVELS];
    id<MTLRenderPipelineState> extract, composite;
    id<MTLRenderPipelineState> down[WIRED_BLOOM_MAX_LEVELS], up[WIRED_BLOOM_MAX_LEVELS];
};
struct ralMetalBloom_s {
    id<MTLDevice> device;
    id<MTLLibrary> library[5];
    id<MTLSamplerState> sampler;
    wiredBloomCohort *cohort;
};
static void wired_bloom_release(wiredBloomCohort *c) {
    if (!c) return;
    [c->scene release]; [c->extract release]; [c->composite release];
    for (unsigned i=0; i<=WIRED_BLOOM_MAX_LEVELS; ++i) [c->mip[i] release];
    for (unsigned i=0; i<2u+WIRED_BLOOM_MAX_LEVELS; ++i) [c->args[i] release];
    for (unsigned i=0; i<WIRED_BLOOM_MAX_LEVELS; ++i) {
        [c->down[i] release]; [c->up[i] release];
    }
    free(c);
}
void RalMetal_BloomDestroy(ralMetalBloom_t *s) {
    if (!s) return;
    wired_bloom_release(s->cohort);
    for (unsigned i=0; i<5; ++i) [s->library[i] release];
    [s->sampler release]; [s->device release]; free(s);
}
ralMetalBloom_t *RalMetal_BloomCreate(id<MTLDevice> device) {
    if (!device) return NULL;
    ralMetalBloom_t *s = (ralMetalBloom_t *)calloc(1, sizeof(*s));
    if (!s) return NULL;
    s->device = [device retain];
    const unsigned char *bytes[] = {ral_metal_ui_metallib, ral_metal_bloom_metallib,
        ral_metal_downsample_metallib, ral_metal_upsample_metallib, ral_metal_bloom_composite_metallib};
    const size_t sizes[] = {ral_metal_ui_metallib_size, ral_metal_bloom_metallib_size,
        ral_metal_downsample_metallib_size, ral_metal_upsample_metallib_size, ral_metal_bloom_composite_metallib_size};
    for (unsigned i=0; i<5; ++i) {
        dispatch_data_t data = dispatch_data_create(bytes[i], sizes[i],
            dispatch_get_global_queue(QOS_CLASS_DEFAULT, 0), ^{});
        if (!data) { RalMetal_BloomDestroy(s); return NULL; }
        NSError *error = nil;
        s->library[i] = [device newLibraryWithData:data error:&error];
        dispatch_release(data);
        if (!s->library[i] || error) { RalMetal_BloomDestroy(s); return NULL; }
    }
    MTLSamplerDescriptor *sd = [[[MTLSamplerDescriptor alloc] init] autorelease];
    sd.minFilter = sd.magFilter = MTLSamplerMinMagFilterLinear;
    sd.sAddressMode = sd.tAddressMode = MTLSamplerAddressModeClampToEdge;
    sd.supportArgumentBuffers = YES;
    s->sampler = [device newSamplerStateWithDescriptor:sd];
    if (!s->sampler) { RalMetal_BloomDestroy(s); return NULL; }
    return s;
}
static id<MTLRenderPipelineState> wired_bloom_pipeline(ralMetalBloom_t *s,
        unsigned shader, MTLFunctionConstantValues *values, bool additive) {
    NSError *error = nil;
    id<MTLFunction> vs = [s->library[0] newFunctionWithName:@"wired_bloom_vertex"];
    id<MTLFunction> fs = [s->library[shader] newFunctionWithName:@"main0"
        constantValues:values error:&error];
    id<MTLRenderPipelineState> result = nil;
    if (vs && fs && !error) {
        MTLRenderPipelineDescriptor *pd = [[[MTLRenderPipelineDescriptor alloc] init] autorelease];
        pd.vertexFunction = vs; pd.fragmentFunction = fs;
        pd.label = @"wired.bloom";
        auto a = pd.colorAttachments[0];
        a.pixelFormat = MTLPixelFormatRGBA16Float;
        a.blendingEnabled = additive;
        a.sourceRGBBlendFactor = a.sourceAlphaBlendFactor = MTLBlendFactorOne;
        a.destinationRGBBlendFactor = a.destinationAlphaBlendFactor = MTLBlendFactorOne;
        result = [s->device newRenderPipelineStateWithDescriptor:pd error:&error];
    }
    [vs release]; [fs release];
    if (error) { [result release]; return nil; }
    return result;
}
static bool wired_bloom_prepare(ralMetalBloom_t *s, id<MTLTexture> scene,
        const ralBloomSettings_t *settings, const ralBloomPlan_t *plan) {
    wiredBloomCohort *old = s->cohort;
    if (old && old->scene == scene && old->settings.levels == settings->levels
        && old->settings.thresholdMode == settings->thresholdMode
        && old->settings.threshold == settings->threshold && old->settings.intensity == settings->intensity)
        return true;
    wiredBloomCohort *c = (wiredBloomCohort *)calloc(1, sizeof(*c));
    if (!c) return false;
    c->settings = *settings; c->plan = *plan; c->scene = [scene retain];
    bool ok = false;
    do {
        MTLFunctionConstantValues *v = [[[MTLFunctionConstantValues alloc] init] autorelease];
        int mode = (int)settings->thresholdMode;
        [v setConstantValue:&settings->threshold type:MTLDataTypeFloat atIndex:3];
        [v setConstantValue:&mode type:MTLDataTypeInt atIndex:5];
        c->extract = wired_bloom_pipeline(s, 1, v, false);
        [v reset]; [v setConstantValue:&plan->compositeScale type:MTLDataTypeFloat atIndex:0];
        c->composite = wired_bloom_pipeline(s, 4, v, true);
        if (!c->extract || !c->composite) break;
        bool complete = true;
        for (unsigned i=0; i<=plan->levels; ++i) {
            MTLTextureDescriptor *td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:
                MTLPixelFormatRGBA16Float width:plan->width[i] height:plan->height[i] mipmapped:NO];
            td.storageMode = MTLStorageModePrivate;
            td.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
            c->mip[i] = [s->device newTextureWithDescriptor:td];
            if (!c->mip[i]) { complete = false; break; }
        }
        if (!complete) break;
        for (unsigned i=0; i<plan->levels; ++i) {
            float tx = 1.0f/plan->width[i], ty = 1.0f/plan->height[i];
            int first = i == 0u;
            [v reset];
            [v setConstantValue:&tx type:MTLDataTypeFloat atIndex:0];
            [v setConstantValue:&ty type:MTLDataTypeFloat atIndex:1];
            [v setConstantValue:&first type:MTLDataTypeInt atIndex:2];
            c->down[i] = wired_bloom_pipeline(s, 2, v, false);
            if (!c->down[i]) { complete = false; break; }
            if (i) {
                tx = 1.0f/plan->width[i+1]; ty = 1.0f/plan->height[i+1];
                const float radius = 1.0f;
                [v reset];
                [v setConstantValue:&tx type:MTLDataTypeFloat atIndex:0];
                [v setConstantValue:&ty type:MTLDataTypeFloat atIndex:1];
                [v setConstantValue:&radius type:MTLDataTypeFloat atIndex:2];
                c->up[i] = wired_bloom_pipeline(s, 3, v, true);
                if (!c->up[i]) { complete = false; break; }
            }
        }
        if (!complete) break;
        /* All catalog fragments share the same texture/sampler argument ABI.
         * Build immutable argument buffers once per texture cohort. */
        id<MTLFunction> fs = [s->library[1] newFunctionWithName:@"main0"];
        id<MTLArgumentEncoder> ae = [fs newArgumentEncoderWithBufferIndex:0u];
        [fs release];
        if (!ae) break;
        for (unsigned i=0; i<=plan->levels+1u; ++i) {
            c->args[i] = [s->device newBufferWithLength:ae.encodedLength options:MTLResourceStorageModeShared];
            if (!c->args[i]) { complete = false; break; }
            [ae setArgumentBuffer:c->args[i] offset:0];
            [ae setTexture:i ? c->mip[i-1] : scene atIndex:0];
            [ae setSamplerState:s->sampler atIndex:1];
        }
        [ae release];
        if (!complete) break;
        ok = true;
    } while (false);
    if (!ok) { wired_bloom_release(c); return false; }
    s->cohort = c; wired_bloom_release(old);
    return true;
}
static bool wired_bloom_pass(id<MTLCommandBuffer> command, id<MTLRenderPipelineState> pipeline,
        id<MTLTexture> source, id<MTLBuffer> args, id<MTLTexture> target, bool additive) {
    MTLRenderPassDescriptor *pd = [MTLRenderPassDescriptor renderPassDescriptor];
    pd.colorAttachments[0].texture = target;
    pd.colorAttachments[0].loadAction = additive ? MTLLoadActionLoad : MTLLoadActionDontCare;
    pd.colorAttachments[0].storeAction = MTLStoreActionStore;
    id<MTLRenderCommandEncoder> e = [command renderCommandEncoderWithDescriptor:pd];
    if (!e) return false;
    e.label = @"wired.bloom";
    [e setRenderPipelineState:pipeline];
    [e setFragmentBuffer:args offset:0 atIndex:0];
    [e useResource:source usage:MTLResourceUsageRead stages:MTLRenderStageFragment];
    [e drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
    [e endEncoding];
    return true;
}
bool RalMetal_BloomEncode(ralMetalBloom_t *s, id<MTLCommandBuffer> command,
        id<MTLTexture> scene, const ralBloomSettings_t *settings) {
    ralBloomPlan_t p;
    if (!s || !command || !scene || command.device != s->device || scene.device != s->device
        || scene.pixelFormat != MTLPixelFormatRGBA16Float || scene.sampleCount != 1u
        || scene.width > UINT32_MAX || scene.height > UINT32_MAX
        || !Ral_BloomPlanBuild(settings, (uint32_t)scene.width, (uint32_t)scene.height, &p)) return false;
    if (!p.levels) return true;
    if (!wired_bloom_prepare(s, scene, settings, &p)) return false;
    wiredBloomCohort *c = s->cohort;
    if (!wired_bloom_pass(command, c->extract, scene, c->args[0], c->mip[0], false)) return false;
    for (unsigned i=0; i<p.levels; ++i)
        if (!wired_bloom_pass(command, c->down[i], c->mip[i], c->args[i+1], c->mip[i+1], false)) return false;
    for (unsigned i=p.levels-1u; i>0; --i)
        if (!wired_bloom_pass(command, c->up[i], c->mip[i+1], c->args[i+2], c->mip[i], true)) return false;
    return wired_bloom_pass(command, c->composite, c->mip[1], c->args[2], scene, true);
}
