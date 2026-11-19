// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <cmath>
#include <cstdio>
#include "../code/render/frontend/render_material_stage_gpu.h"
#include "../code/render/frontend/render_sky_atlas.h"
#include <cstdlib>

// Original decoded fixture pixels enter the same registration-time atlas
// builder as GL, Metal and WebGPU. No game asset or production shader is replaced.
qboolean RenderImage_DecodeRgba8(const char *name, byte **pixels,
        uint32_t *width, uint32_t *height, char resolved[MAX_QPATH]) {
    const char *suffixes[6]={"rt","bk","lf","ft","up","dn"};
    for(unsigned face=0;face<6;++face) {
        char path[MAX_QPATH]; snprintf(path,sizeof(path),"env/probe_%s",suffixes[face]);
        if(strcmp(name,path)) continue;
        *pixels=(byte *)malloc(64); if(!*pixels)return qfalse;
        *width=*height=4; strcpy(resolved,path);
        for(unsigned y=0;y<4;++y)for(unsigned x=0;x<4;++x) {
            byte *p=*pixels+(y*4+x)*4;
            p[0]=(byte)(face*24+x*8); p[1]=(byte)(face*16+y*12);
            p[2]=(byte)(face*8+x*4+y*6); p[3]=255;
        }
        return qtrue;
    }
    return qfalse;
}

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL %d: %s\n", __LINE__, #x); return 1; } } while (0)

int main(int argc, const char **argv) {
    @autoreleasepool {
        CHECK(argc == 3);
        id<MTLDevice> device = MTLCreateSystemDefaultDevice();
        CHECK(device);
        NSError *error = nil;
        NSString *source = [NSString stringWithContentsOfFile:
            [NSString stringWithUTF8String:argv[1]] encoding:NSUTF8StringEncoding error:&error];
        CHECK(source);
        id<MTLLibrary> library = [device newLibraryWithSource:source options:nil error:&error];
        if (!library) fprintf(stderr, "%s\n", error.description.UTF8String);
        CHECK(library);
        // Constant direction isolates production fragment composition/bindings.
        // It is not a claim of nonlinear Q3 mesh interpolation equivalence.
        NSString *vertexSource = @"#include <metal_stdlib>\nusing namespace metal;\n"
            "struct Out { float4 pos [[position]]; float2 lm [[user(locn1)]];"
            "float4 col [[user(locn2)]]; float3 world [[user(locn3)]]; float3 normal [[user(locn4)]]; };\n"
            "vertex Out probe(uint id [[vertex_id]], constant float4 &direction [[buffer(0)]]) {"
            "Out o; float2 p = float2((id << 1u) & 2u, id & 2u);"
            "o.pos=float4(p*2.0-1.0,0.0,1.0); o.lm=p; o.col=float4(1);"
            "o.world=direction.xyz; o.normal=float3(0,0,1); return o; }";
        id<MTLLibrary> vertices = [device newLibraryWithSource:vertexSource options:nil error:&error];
        CHECK(vertices);
        id<MTLFunction> fragment = [library newFunctionWithName:@"main0"];
        MTLRenderPipelineDescriptor *pd = [[MTLRenderPipelineDescriptor alloc] init];
        pd.vertexFunction = [vertices newFunctionWithName:@"probe"];
        pd.fragmentFunction = fragment;
        pd.colorAttachments[0].pixelFormat = MTLPixelFormatRGBA32Float;
        id<MTLRenderPipelineState> pipeline = [device newRenderPipelineStateWithDescriptor:pd error:&error];
        if (!pipeline) fprintf(stderr, "%s\n", error.description.UTF8String);
        CHECK(pipeline);
        NSString *productVertexSource = [NSString stringWithContentsOfFile:
            [NSString stringWithUTF8String:argv[2]] encoding:NSUTF8StringEncoding error:&error];
        CHECK(productVertexSource);
        id<MTLLibrary> productVertices = [device newLibraryWithSource:productVertexSource options:nil error:&error];
        CHECK(productVertices);
        pd.vertexFunction = [productVertices newFunctionWithName:@"main0"];
        MTLVertexDescriptor *vd = [[MTLVertexDescriptor alloc] init];
        const MTLVertexFormat formats[5] = {MTLVertexFormatFloat3, MTLVertexFormatFloat2,
            MTLVertexFormatFloat2, MTLVertexFormatFloat4, MTLVertexFormatFloat3};
        const unsigned offsets[5] = {0,12,20,28,44};
        for (unsigned i=0; i<5; ++i) {
            vd.attributes[i].format=formats[i]; vd.attributes[i].offset=offsets[i];
            vd.attributes[i].bufferIndex=0;
        }
        vd.layouts[0].stride=56; pd.vertexDescriptor=vd;
        id<MTLRenderPipelineState> productPipeline = [device newRenderPipelineStateWithDescriptor:pd error:&error];
        if (!productPipeline) fprintf(stderr, "%s\n", error.description.UTF8String);
        CHECK(productPipeline);
        const float productMesh[3][14] = {
            {0,0,1, 0,0, 0,0, 1,1,1,1, 0,0,1},
            {32,0,1, 0,0, 0,0, 1,1,1,1, 0,0,1},
            {0,32,1, 0,0, 0,0, 1,1,1,1, 0,0,1}
        };
        id<MTLArgumentEncoder> textureArgs = [fragment newArgumentEncoderWithBufferIndex:1];
        id<MTLArgumentEncoder> uniformArgs = [fragment newArgumentEncoderWithBufferIndex:2];
        CHECK(textureArgs && uniformArgs);
        id<MTLBuffer> textureBuffer = [device newBufferWithLength:textureArgs.encodedLength options:MTLResourceStorageModeShared];
        id<MTLBuffer> uniformBuffer = [device newBufferWithLength:uniformArgs.encodedLength options:MTLResourceStorageModeShared];
        id<MTLBuffer> operands = [device newBufferWithLength:sizeof(renderSkyMaterialGpuParams_t) options:MTLResourceStorageModeShared];
        CHECK(textureBuffer && uniformBuffer && operands);
        [textureArgs setArgumentBuffer:textureBuffer offset:0];
        [uniformArgs setArgumentBuffer:uniformBuffer offset:0];
        [uniformArgs setBuffer:operands offset:0 atIndex:0];
        MTLSamplerDescriptor *sd = [[MTLSamplerDescriptor alloc] init];
        sd.minFilter = sd.magFilter = MTLSamplerMinMagFilterNearest;
        id<MTLSamplerState> sampler = [device newSamplerStateWithDescriptor:sd];
        CHECK(sampler);
        id<MTLTexture> textures[10];
        float colors[10][4];
        for (unsigned i = 0; i < 10; ++i) {
            colors[i][0] = (i+1)*0.025f; colors[i][1] = (10-i)*0.03125f;
            colors[i][2] = (i%3)*0.0625f; colors[i][3] = 0.125f*(1+i%7);
            MTLTextureDescriptor *td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA32Float
                width:i==8 ? 3 : 1 height:i==8 ? 2 : 1 mipmapped:NO];
            td.storageMode = MTLStorageModeShared;
            textures[i] = [device newTextureWithDescriptor:td];
            CHECK(textures[i]);
            if (i == 8) {
                float atlas[6][4];
                for (unsigned face = 0; face < 6; ++face)
                    for (unsigned ch = 0; ch < 4; ++ch) atlas[face][ch] = colors[i][ch] + face*0.03125f;
                [textures[i] replaceRegion:MTLRegionMake2D(0,0,3,2) mipmapLevel:0 withBytes:atlas bytesPerRow:3*16];
            } else [textures[i] replaceRegion:MTLRegionMake2D(0,0,1,1) mipmapLevel:0 withBytes:colors[i] bytesPerRow:16];
            [textureArgs setTexture:textures[i] atIndex:i*2];
            [textureArgs setSamplerState:sampler atIndex:i*2+1];
        }
        MTLTextureDescriptor *td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA32Float width:16 height:16 mipmapped:NO];
        td.storageMode = MTLStorageModeShared; td.usage = MTLTextureUsageRenderTarget;
        id<MTLTexture> output = [device newTextureWithDescriptor:td];
        id<MTLCommandQueue> queue = [device newCommandQueue];
        CHECK(output && queue);
        // Independent raw Q3 face order, NOT the GL radiance cube face order.
        const float directions[6][4] = {{1,0,0,0},{0,1,0,0},{-1,0,0,0},{0,-1,0,0},{0,0,1,0},{0,0,-1,0}};
        float draw[64] = {};
        draw[7] = 1; draw[11] = 1; draw[15] = 1; // authored display identity
        unsigned cases = 0;
        for (unsigned productVertex = 0; productVertex < 2; ++productVertex)
        for (unsigned outer = 0; outer < (productVertex ? 1u : 2u); ++outer)
        for (unsigned face = 0; face < (productVertex ? 1u : 6u); ++face)
        for (unsigned count = 0; count <= 8; ++count)
        for (unsigned mode = 0; mode < 3; ++mode) {
            renderSkyMaterialGpuParams_t params = {};
            params.meta[0] = count; params.meta[1] = outer; params.timeHeight[1] = 256;
            float expected[4] = {};
            if (outer) for (unsigned ch=0; ch<4; ++ch) expected[ch] = colors[8][ch] + face*0.03125f;
            for (unsigned i = 0; i < count; ++i) {
                renderMaterialStageSnapshot_t stage = {};
                stage.sourceBlend = mode == 0 ? 1 : 6;
                stage.destinationBlend = mode == 0 ? 1 : 7;
                stage.scaleScroll[0] = stage.scaleScroll[1] = 1;
                stage.rgbGen = 5; // Vertex white must survive sky/beam UBO aliasing.
                stage.alphaTest = mode == 2 && (i & 1) ? 3 : 0;
                stage.alphaCutoff = 0.99f;
                wired_material_stage_pack(&stage, &params.stages[i]);
                if (stage.alphaTest) continue;
                for (unsigned ch=0; ch<4; ++ch) expected[ch] = mode == 0
                    ? expected[ch] + colors[i][ch]
                    : colors[i][ch]*colors[i][3] + expected[ch]*(1-colors[i][3]);
            }
            expected[3] = 1;
            memcpy(operands.contents, &params, sizeof(params));
            MTLRenderPassDescriptor *pass = [MTLRenderPassDescriptor renderPassDescriptor];
            pass.colorAttachments[0].texture = output;
            pass.colorAttachments[0].loadAction = MTLLoadActionClear;
            pass.colorAttachments[0].storeAction = MTLStoreActionStore;
            pass.colorAttachments[0].clearColor = MTLClearColorMake(99,99,99,99);
            id<MTLCommandBuffer> command = [queue commandBuffer];
            id<MTLRenderCommandEncoder> encoder = [command renderCommandEncoderWithDescriptor:pass];
            CHECK(command && encoder);
            draw[3] = productVertex ? 1 : 0;
            draw[20] = draw[21] = 16;
            draw[35] = 1; // This is a sky draw, not a beam-material packet.
            [encoder setRenderPipelineState:productVertex ? productPipeline : pipeline];
            if (productVertex) {
                [encoder setVertexBytes:productMesh length:sizeof(productMesh) atIndex:0];
                [encoder setVertexBuffer:uniformBuffer offset:0 atIndex:2];
                [encoder setVertexBytes:draw length:sizeof(draw) atIndex:3];
            } else [encoder setVertexBytes:directions[face] length:16 atIndex:0];
            [encoder setFragmentBuffer:textureBuffer offset:0 atIndex:1];
            [encoder setFragmentBuffer:uniformBuffer offset:0 atIndex:2];
            [encoder setFragmentBytes:draw length:sizeof(draw) atIndex:3];
            [encoder useResource:operands usage:MTLResourceUsageRead stages:MTLRenderStageVertex | MTLRenderStageFragment];
            for (unsigned i=0; i<10; ++i) [encoder useResource:textures[i] usage:MTLResourceUsageRead stages:MTLRenderStageFragment];
            [encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
            [encoder endEncoding]; [command commit]; [command waitUntilCompleted];
            CHECK(command.status == MTLCommandBufferStatusCompleted);
            float actual[16*16*4];
            [output getBytes:actual bytesPerRow:16*16 fromRegion:MTLRegionMake2D(0,0,16,16) mipmapLevel:0];
            for (unsigned p=0; p<16*16*4; ++p) {
                if (!std::isfinite(actual[p]) || std::fabs(actual[p]-expected[p%4]) > 1e-4f) {
                    fprintf(stderr,"outer=%u face=%u count=%u mode=%u channel=%u actual=%g expected=%g\n",outer,face,count,mode,p%4,actual[p],expected[p%4]);
                    return 1;
                }
            }
            ++cases;
        }
        // Bilinear edge sampling must clamp within each face, not blend an
        // unrelated neighboring atlas cell. Probe near all edges/corners.
        refimport_t imports={}; imports.Malloc=malloc; imports.Free=free;
        byte *atlas=nullptr; uint32_t aw=0,ah=0;
        CHECK(wired_sky_atlas_decode(&imports,"env/probe",&atlas,&aw,&ah));
        CHECK(aw==12 && ah==8);
        td=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
            width:aw height:ah mipmapped:NO]; td.storageMode=MTLStorageModeShared;
        textures[8]=[device newTextureWithDescriptor:td]; CHECK(textures[8]);
        [textures[8] replaceRegion:MTLRegionMake2D(0,0,aw,ah) mipmapLevel:0
            withBytes:atlas bytesPerRow:aw*4]; free(atlas);
        sd.minFilter=sd.magFilter=MTLSamplerMinMagFilterLinear;
        sampler=[device newSamplerStateWithDescriptor:sd]; CHECK(sampler);
        [textureArgs setTexture:textures[8] atIndex:16];
        [textureArgs setSamplerState:sampler atIndex:17];
        renderSkyMaterialGpuParams_t params={}; params.meta[1]=1;
        memcpy(operands.contents,&params,sizeof(params)); draw[3]=0;
        const float coords[]={.0001f,.125f,.37f,.5f,.73f,.875f,.9999f};
        unsigned edgeCases=0;
        for(unsigned face=0;face<6;++face)for(float s:coords)for(float t:coords) {
            const float u=2*s-1,v=2*t-1;
            const float rays[6][4]={{1,-u,-v,0},{u,1,-v,0},{-1,u,-v,0},
                {-u,-1,-v,0},{v,-u,1,0},{-v,-u,-1,0}};
            const float x=fmaxf(0,fminf(3,s*4-.5f)), y=fmaxf(0,fminf(3,t*4-.5f));
            const float expected[4]={(face*24+x*8)/255,(face*16+y*12)/255,
                (face*8+x*4+y*6)/255,1};
            MTLRenderPassDescriptor *pass=[MTLRenderPassDescriptor renderPassDescriptor];
            pass.colorAttachments[0].texture=output;
            pass.colorAttachments[0].loadAction=MTLLoadActionClear;
            pass.colorAttachments[0].storeAction=MTLStoreActionStore;
            id<MTLCommandBuffer> command=[queue commandBuffer];
            id<MTLRenderCommandEncoder> encoder=[command renderCommandEncoderWithDescriptor:pass];
            [encoder setRenderPipelineState:pipeline];
            [encoder setVertexBytes:rays[face] length:16 atIndex:0];
            [encoder setFragmentBuffer:textureBuffer offset:0 atIndex:1];
            [encoder setFragmentBuffer:uniformBuffer offset:0 atIndex:2];
            [encoder setFragmentBytes:draw length:sizeof(draw) atIndex:3];
            [encoder useResource:operands usage:MTLResourceUsageRead stages:MTLRenderStageFragment];
            for(unsigned i=0;i<10;++i)[encoder useResource:textures[i] usage:MTLResourceUsageRead stages:MTLRenderStageFragment];
            [encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
            [encoder endEncoding]; [command commit]; [command waitUntilCompleted];
            CHECK(command.status==MTLCommandBufferStatusCompleted);
            float actual[16*16*4];
            [output getBytes:actual bytesPerRow:16*16 fromRegion:MTLRegionMake2D(0,0,16,16) mipmapLevel:0];
            for(unsigned p=0;p<16*16*4;++p) if(!std::isfinite(actual[p])
                    || fabsf(actual[p]-expected[p%4])>1.0f/255.0f) {
                fprintf(stderr,"atlas edge face=%u s=%g t=%g ch=%u actual=%g expected=%g\n",
                    face,s,t,p%4,actual[p],expected[p%4]); return 1;
            }
            ++edgeCases;
        }
        printf("sky material raster GPU: %u composition and %u bilinear atlas-edge cases PASS\n",cases,edgeCases);
    }
    return 0;
}
