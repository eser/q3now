// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
#include "ral_metal_smaa.h"
#include "ral_metal_smaa_edge_vert_metallib.h"
#include "ral_metal_smaa_edge_frag_metallib.h"
#include "ral_metal_smaa_blend_vert_metallib.h"
#include "ral_metal_smaa_blend_frag_metallib.h"
#include "ral_metal_smaa_resolve_vert_metallib.h"
#include "ral_metal_smaa_resolve_frag_metallib.h"
#include "../vulkan/renderer/smaa_area_texture.h"
#include "../vulkan/renderer/smaa_search_texture.h"
#include <stdlib.h>

/* Presets (index = quality): identical to the Vulkan renderer's tables. */
static const float kThreshold[] = { 0.0f, 0.15f, 0.10f, 0.10f, 0.05f };
static const int kSearchSteps[] = { 0, 4, 8, 16, 32 };
static const int kSearchStepsDiag[] = { 0, 0, 0, 8, 16 };
static const int kCornerRounding[] = { 0, 0, 0, 25, 25 };

enum { kEdgeVs, kEdgeFs, kBlendVs, kBlendFs, kResolveVs, kResolveFs, kLibraryCount };

struct wiredSmaaCohort {
    id<MTLTexture> scene, edges, blend, output;
    int quality;
    id<MTLRenderPipelineState> edgePipeline, blendPipeline, resolvePipeline;
    id<MTLBuffer> rtMetrics;
    id<MTLBuffer> vsArgs[3];                 /* set 3 (the RtMetrics pointer) per pass */
    id<MTLBuffer> edgeFs0;                   /* colorTex + sampler */
    id<MTLBuffer> blendFs0, blendFs1, blendFs2, blendFs3;
    id<MTLBuffer> resolveFs0, resolveFs1, resolveFs3;
};
struct ralMetalSmaa_s {
    id<MTLDevice> device;
    id<MTLLibrary> library[kLibraryCount];
    id<MTLSamplerState> point, linear;
    id<MTLTexture> area, search;
    wiredSmaaCohort *cohort;
};

static void wired_smaa_release(wiredSmaaCohort *c) {
    if (!c) return;
    [c->scene release]; [c->edges release]; [c->blend release]; [c->output release];
    [c->edgePipeline release]; [c->blendPipeline release]; [c->resolvePipeline release]; [c->rtMetrics release];
    for (int i = 0; i < 3; ++i) [c->vsArgs[i] release];
    [c->edgeFs0 release]; [c->blendFs0 release]; [c->blendFs1 release]; [c->blendFs2 release]; [c->blendFs3 release];
    [c->resolveFs0 release]; [c->resolveFs1 release]; [c->resolveFs3 release];
    free(c);
}
void RalMetal_SmaaDestroy(ralMetalSmaa_t *s) {
    if (!s) return;
    wired_smaa_release(s->cohort);
    for (int i = 0; i < kLibraryCount; ++i) [s->library[i] release];
    [s->point release]; [s->linear release]; [s->area release]; [s->search release];
    [s->device release]; free(s);
}
static id<MTLTexture> wired_smaa_lut(id<MTLDevice> device, MTLPixelFormat format, NSUInteger w, NSUInteger h,
        const unsigned char *bytes, NSUInteger pitch) {
    MTLTextureDescriptor *td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:format width:w height:h mipmapped:NO];
    td.storageMode = MTLStorageModeShared;
    td.usage = MTLTextureUsageShaderRead;
    id<MTLTexture> t = [device newTextureWithDescriptor:td];
    if (t) [t replaceRegion:MTLRegionMake2D(0, 0, w, h) mipmapLevel:0 withBytes:bytes bytesPerRow:pitch];
    return t;
}
ralMetalSmaa_t *RalMetal_SmaaCreate(id<MTLDevice> device) {
    if (!device) return NULL;
    ralMetalSmaa_t *s = (ralMetalSmaa_t *)calloc(1, sizeof(*s));
    if (!s) return NULL;
    s->device = [device retain];
    const unsigned char *bytes[kLibraryCount] = {
        ral_metal_smaa_edge_vert_metallib, ral_metal_smaa_edge_frag_metallib,
        ral_metal_smaa_blend_vert_metallib, ral_metal_smaa_blend_frag_metallib,
        ral_metal_smaa_resolve_vert_metallib, ral_metal_smaa_resolve_frag_metallib };
    const size_t sizes[kLibraryCount] = {
        ral_metal_smaa_edge_vert_metallib_size, ral_metal_smaa_edge_frag_metallib_size,
        ral_metal_smaa_blend_vert_metallib_size, ral_metal_smaa_blend_frag_metallib_size,
        ral_metal_smaa_resolve_vert_metallib_size, ral_metal_smaa_resolve_frag_metallib_size };
    for (int i = 0; i < kLibraryCount; ++i) {
        dispatch_data_t data = dispatch_data_create(bytes[i], sizes[i], dispatch_get_global_queue(QOS_CLASS_DEFAULT, 0), ^{});
        if (!data) { RalMetal_SmaaDestroy(s); return NULL; }
        NSError *error = nil;
        s->library[i] = [device newLibraryWithData:data error:&error];
        dispatch_release(data);
        if (!s->library[i] || error) { RalMetal_SmaaDestroy(s); return NULL; }
    }
    MTLSamplerDescriptor *sd = [[[MTLSamplerDescriptor alloc] init] autorelease];
    sd.sAddressMode = sd.tAddressMode = MTLSamplerAddressModeClampToEdge;
    sd.supportArgumentBuffers = YES;
    sd.minFilter = sd.magFilter = MTLSamplerMinMagFilterNearest;
    s->point = [device newSamplerStateWithDescriptor:sd];
    sd.minFilter = sd.magFilter = MTLSamplerMinMagFilterLinear;
    s->linear = [device newSamplerStateWithDescriptor:sd];
    s->area = wired_smaa_lut(device, MTLPixelFormatRG8Unorm, AREATEX_WIDTH, AREATEX_HEIGHT, areaTexBytes, AREATEX_PITCH);
    s->search = wired_smaa_lut(device, MTLPixelFormatR8Unorm, SEARCHTEX_WIDTH, SEARCHTEX_HEIGHT, searchTexBytes, SEARCHTEX_PITCH);
    if (!s->point || !s->linear || !s->area || !s->search) { RalMetal_SmaaDestroy(s); return NULL; }
    return s;
}
static id<MTLRenderPipelineState> wired_smaa_pipeline(ralMetalSmaa_t *s, int vsIndex, int fsIndex,
        MTLFunctionConstantValues *vsConstants, MTLFunctionConstantValues *fsConstants, MTLPixelFormat format) {
    NSError *error = nil;
    id<MTLFunction> vs = vsConstants ? [s->library[vsIndex] newFunctionWithName:@"main0" constantValues:vsConstants error:&error]
        : [s->library[vsIndex] newFunctionWithName:@"main0"];
    id<MTLFunction> fs = nil;
    if (vs && !error) fs = fsConstants ? [s->library[fsIndex] newFunctionWithName:@"main0" constantValues:fsConstants error:&error]
        : [s->library[fsIndex] newFunctionWithName:@"main0"];
    id<MTLRenderPipelineState> result = nil;
    if (vs && fs && !error) {
        MTLRenderPipelineDescriptor *pd = [[[MTLRenderPipelineDescriptor alloc] init] autorelease];
        pd.vertexFunction = vs; pd.fragmentFunction = fs; pd.label = @"wired.smaa";
        pd.colorAttachments[0].pixelFormat = format;
        result = [s->device newRenderPipelineStateWithDescriptor:pd error:&error];
    }
    [vs release]; [fs release];
    if (error) { [result release]; return nil; }
    return result;
}
/* One immutable argument buffer for a stage's descriptor set: up to two textures/samplers at id(0..3) or a buffer pointer at id(0). */
static id<MTLBuffer> wired_smaa_args(ralMetalSmaa_t *s, id<MTLFunction> fn, NSUInteger bufferIndex,
        id<MTLTexture> texture, id<MTLSamplerState> sampler, id<MTLBuffer> pointer) {
    id<MTLArgumentEncoder> ae = [fn newArgumentEncoderWithBufferIndex:bufferIndex];
    if (!ae) return nil;
    id<MTLBuffer> buffer = [s->device newBufferWithLength:ae.encodedLength options:MTLResourceStorageModeShared];
    if (buffer) {
        [ae setArgumentBuffer:buffer offset:0];
        if (pointer) [ae setBuffer:pointer offset:0 atIndex:0];
        else { [ae setTexture:texture atIndex:0]; [ae setSamplerState:sampler atIndex:1]; }
    }
    [ae release];
    return buffer;
}
static id<MTLTexture> wired_smaa_target(ralMetalSmaa_t *s, MTLPixelFormat format, NSUInteger w, NSUInteger h) {
    MTLTextureDescriptor *td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:format width:w height:h mipmapped:NO];
    td.storageMode = MTLStorageModePrivate;
    td.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
    return [s->device newTextureWithDescriptor:td];
}
static bool wired_smaa_prepare(ralMetalSmaa_t *s, id<MTLTexture> scene, int quality) {
    wiredSmaaCohort *old = s->cohort;
    if (old && old->scene == scene && old->quality == quality) return true;
    wiredSmaaCohort *c = (wiredSmaaCohort *)calloc(1, sizeof(*c));
    if (!c) return false;
    c->scene = [scene retain]; c->quality = quality;
    bool ok = false;
    id<MTLFunction> fn[kLibraryCount] = { nil };
    do {
        const NSUInteger w = scene.width, h = scene.height;
        const float metrics[4] = { 1.0f / (float)w, 1.0f / (float)h, (float)w, (float)h };
        c->edges = wired_smaa_target(s, MTLPixelFormatRG8Unorm, w, h);
        c->blend = wired_smaa_target(s, MTLPixelFormatRGBA8Unorm, w, h);
        c->output = wired_smaa_target(s, MTLPixelFormatRGBA16Float, w, h);
        c->rtMetrics = [s->device newBufferWithBytes:metrics length:sizeof(metrics) options:MTLResourceStorageModeShared];
        if (!c->edges || !c->blend || !c->output || !c->rtMetrics) break;
        MTLFunctionConstantValues *v = [[[MTLFunctionConstantValues alloc] init] autorelease];
        [v setConstantValue:&kThreshold[quality] type:MTLDataTypeFloat atIndex:0];
        c->edgePipeline = wired_smaa_pipeline(s, kEdgeVs, kEdgeFs, nil, v, MTLPixelFormatRG8Unorm);
        MTLFunctionConstantValues *vv = [[[MTLFunctionConstantValues alloc] init] autorelease];
        [vv setConstantValue:&kSearchSteps[quality] type:MTLDataTypeInt atIndex:0];
        MTLFunctionConstantValues *bv = [[[MTLFunctionConstantValues alloc] init] autorelease];
        [bv setConstantValue:&kSearchSteps[quality] type:MTLDataTypeInt atIndex:0];
        [bv setConstantValue:&kSearchStepsDiag[quality] type:MTLDataTypeInt atIndex:1];
        [bv setConstantValue:&kCornerRounding[quality] type:MTLDataTypeInt atIndex:2];
        c->blendPipeline = wired_smaa_pipeline(s, kBlendVs, kBlendFs, vv, bv, MTLPixelFormatRGBA8Unorm);
        c->resolvePipeline = wired_smaa_pipeline(s, kResolveVs, kResolveFs, nil, nil, MTLPixelFormatRGBA16Float);
        if (!c->edgePipeline || !c->blendPipeline || !c->resolvePipeline) break;
        /* Argument encoders come from the functions of the same modules (constants do not change the argument ABI). */
        for (int i = 0; i < kLibraryCount; ++i) {
            fn[i] = [s->library[i] newFunctionWithName:@"main0"];
            if (!fn[i]) break;
        }
        if (!fn[kEdgeVs] || !fn[kEdgeFs] || !fn[kBlendVs] || !fn[kBlendFs] || !fn[kResolveVs] || !fn[kResolveFs]) break;
        c->vsArgs[0] = wired_smaa_args(s, fn[kEdgeVs], 3, nil, nil, c->rtMetrics);
        c->vsArgs[1] = wired_smaa_args(s, fn[kBlendVs], 3, nil, nil, c->rtMetrics);
        c->vsArgs[2] = wired_smaa_args(s, fn[kResolveVs], 3, nil, nil, c->rtMetrics);
        c->edgeFs0 = wired_smaa_args(s, fn[kEdgeFs], 0, scene, s->linear, nil);
        c->blendFs0 = wired_smaa_args(s, fn[kBlendFs], 0, c->edges, s->point, nil);
        c->blendFs1 = wired_smaa_args(s, fn[kBlendFs], 1, s->area, s->linear, nil);
        c->blendFs2 = wired_smaa_args(s, fn[kBlendFs], 2, s->search, s->point, nil);
        c->blendFs3 = wired_smaa_args(s, fn[kBlendFs], 3, nil, nil, c->rtMetrics);
        c->resolveFs0 = wired_smaa_args(s, fn[kResolveFs], 0, scene, s->linear, nil);
        c->resolveFs1 = wired_smaa_args(s, fn[kResolveFs], 1, c->blend, s->linear, nil);
        c->resolveFs3 = wired_smaa_args(s, fn[kResolveFs], 3, nil, nil, c->rtMetrics);
        if (!c->vsArgs[0] || !c->vsArgs[1] || !c->vsArgs[2] || !c->edgeFs0 || !c->blendFs0 || !c->blendFs1 || !c->blendFs2
                || !c->blendFs3 || !c->resolveFs0 || !c->resolveFs1 || !c->resolveFs3) break;
        ok = true;
    } while (false);
    for (int i = 0; i < kLibraryCount; ++i) [fn[i] release];
    if (!ok) { wired_smaa_release(c); return false; }
    s->cohort = c; wired_smaa_release(old);
    return true;
}
static bool wired_smaa_pass(id<MTLCommandBuffer> command, id<MTLRenderPipelineState> pipeline, id<MTLTexture> target,
        id<MTLBuffer> vsArgs, id<MTLBuffer> const *fsArgs, const NSUInteger *fsSlots, unsigned fsCount,
        id<MTLResource> const *resources, unsigned resourceCount) {
    MTLRenderPassDescriptor *pd = [MTLRenderPassDescriptor renderPassDescriptor];
    pd.colorAttachments[0].texture = target;
    pd.colorAttachments[0].loadAction = MTLLoadActionDontCare;
    pd.colorAttachments[0].storeAction = MTLStoreActionStore;
    id<MTLRenderCommandEncoder> e = [command renderCommandEncoderWithDescriptor:pd];
    if (!e) return false;
    e.label = @"wired.smaa";
    [e setRenderPipelineState:pipeline];
    [e setVertexBuffer:vsArgs offset:0 atIndex:3];
    for (unsigned i = 0; i < fsCount; ++i) [e setFragmentBuffer:fsArgs[i] offset:0 atIndex:fsSlots[i]];
    for (unsigned i = 0; i < resourceCount; ++i)
        [e useResource:resources[i] usage:MTLResourceUsageRead stages:MTLRenderStageVertex | MTLRenderStageFragment];
    [e drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
    [e endEncoding];
    return true;
}
bool RalMetal_SmaaEncode(ralMetalSmaa_t *s, id<MTLCommandBuffer> command, id<MTLTexture> scene, int quality) {
    if (!s || !command || !scene || command.device != s->device || scene.device != s->device
        || scene.pixelFormat != MTLPixelFormatRGBA16Float || scene.sampleCount != 1u
        || quality < RAL_METAL_SMAA_QUALITY_LOW || quality > RAL_METAL_SMAA_QUALITY_ULTRA) return false;
    if (!wired_smaa_prepare(s, scene, quality)) return false;
    wiredSmaaCohort *c = s->cohort;
    {   /* 1. luma edges of the scene */
        id<MTLBuffer> fs[] = { c->edgeFs0 }; const NSUInteger slots[] = { 0 };
        id<MTLResource> res[] = { scene, c->rtMetrics, c->vsArgs[0], c->edgeFs0 };
        if (!wired_smaa_pass(command, c->edgePipeline, c->edges, c->vsArgs[0], fs, slots, 1, res, 4)) return false;
    }
    {   /* 2. blend weights from the edges and the area/search lookups */
        id<MTLBuffer> fs[] = { c->blendFs0, c->blendFs1, c->blendFs2, c->blendFs3 }; const NSUInteger slots[] = { 0, 1, 2, 3 };
        id<MTLResource> res[] = { c->edges, s->area, s->search, c->rtMetrics, c->vsArgs[1], c->blendFs0, c->blendFs1, c->blendFs2, c->blendFs3 };
        if (!wired_smaa_pass(command, c->blendPipeline, c->blend, c->vsArgs[1], fs, slots, 4, res, 9)) return false;
    }
    {   /* 3. neighbourhood blend of the scene with the weights */
        id<MTLBuffer> fs[] = { c->resolveFs0, c->resolveFs1, c->resolveFs3 }; const NSUInteger slots[] = { 0, 1, 3 };
        id<MTLResource> res[] = { scene, c->blend, c->rtMetrics, c->vsArgs[2], c->resolveFs0, c->resolveFs1, c->resolveFs3 };
        if (!wired_smaa_pass(command, c->resolvePipeline, c->output, c->vsArgs[2], fs, slots, 3, res, 7)) return false;
    }
    id<MTLBlitCommandEncoder> blit = [command blitCommandEncoder];
    if (!blit) return false;
    blit.label = @"wired.smaa.copy";
    [blit copyFromTexture:c->output sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0, 0, 0)
        sourceSize:MTLSizeMake(scene.width, scene.height, 1)
        toTexture:scene destinationSlice:0 destinationLevel:0 destinationOrigin:MTLOriginMake(0, 0, 0)];
    [blit endEncoding];
    return true;
}
