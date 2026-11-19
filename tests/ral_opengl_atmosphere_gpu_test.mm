// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x); return 1; } } while(0)

// Execute the real translated OpenGL product fragment. This is GPU evidence
// for the portable shader, not a claim that macOS provides native OpenGL 4.6.
int main(int argc,const char **argv) { @autoreleasepool {
    CHECK(argc==2);
    id<MTLDevice> device=MTLCreateSystemDefaultDevice(); CHECK(device);
    NSError *error=nil;
    NSString *source=[NSString stringWithContentsOfFile:[NSString stringWithUTF8String:argv[1]]
        encoding:NSUTF8StringEncoding error:&error]; CHECK(source);
    id<MTLLibrary> library=[device newLibraryWithSource:source options:nil error:&error];
    if(!library)fprintf(stderr,"%s\n",error.description.UTF8String); CHECK(library);
    id<MTLFunction> fragment=[library newFunctionWithName:@"main0"]; CHECK(fragment);
    NSString *vertexSource=@"#include <metal_stdlib>\nusing namespace metal;\n"
        "struct Out{float4 p[[position]];float2 uv[[user(locn0)]];float2 lm[[user(locn1)]];"
        "float4 color[[user(locn2)]];float3 world[[user(locn3)]];float3 normal[[user(locn4)]];};"
        "vertex Out probe(uint id[[vertex_id]],constant float4 &world[[buffer(0)]],constant float4 &color[[buffer(1)]]){"
        "Out o;float2 p=float2((id<<1u)&2u,id&2u);o.p=float4(p*2-1,0,1);"
        "o.uv=o.lm=p;o.color=color;o.world=world.xyz;o.normal=float3(0,0,1);return o;}";
    id<MTLLibrary> vertices=[device newLibraryWithSource:vertexSource options:nil error:&error]; CHECK(vertices);
    MTLRenderPipelineDescriptor *pd=[MTLRenderPipelineDescriptor new];
    pd.vertexFunction=[vertices newFunctionWithName:@"probe"];pd.fragmentFunction=fragment;
    pd.colorAttachments[0].pixelFormat=MTLPixelFormatRGBA32Float;
    id<MTLRenderPipelineState> pipeline=[device newRenderPipelineStateWithDescriptor:pd error:&error];CHECK(pipeline);
    pd.colorAttachments[0].blendingEnabled=YES;
    pd.colorAttachments[0].sourceRGBBlendFactor=MTLBlendFactorSourceAlpha;
    pd.colorAttachments[0].destinationRGBBlendFactor=MTLBlendFactorOne;
    pd.colorAttachments[0].sourceAlphaBlendFactor=MTLBlendFactorOne;
    pd.colorAttachments[0].destinationAlphaBlendFactor=MTLBlendFactorZero;
    id<MTLRenderPipelineState> beamBlend=[device newRenderPipelineStateWithDescriptor:pd error:&error];CHECK(beamBlend);
    id<MTLCommandQueue> queue=[device newCommandQueue];CHECK(queue);
    id<MTLArgumentEncoder> textures=[fragment newArgumentEncoderWithBufferIndex:1];
    id<MTLArgumentEncoder> buffers=[fragment newArgumentEncoderWithBufferIndex:2];CHECK(textures&&buffers);
    id<MTLBuffer> tb=[device newBufferWithLength:textures.encodedLength options:MTLResourceStorageModeShared];
    id<MTLBuffer> ub=[device newBufferWithLength:buffers.encodedLength options:MTLResourceStorageModeShared];
    [textures setArgumentBuffer:tb offset:0];[buffers setArgumentBuffer:ub offset:0];
    float factors[24]={};for(unsigned c=0;c<4;++c)factors[c]=1;factors[4]=-1;
    id<MTLBuffer> material=[device newBufferWithBytes:factors length:sizeof(factors) options:MTLResourceStorageModeShared];
    float froxels[64][4]={};for(unsigned z=0;z<64;++z)for(unsigned c=0;c<3;++c)froxels[z][c]=float(z)/64;
    id<MTLBuffer> media=[device newBufferWithBytes:froxels length:sizeof(froxels) options:MTLResourceStorageModeShared];
    // Argument IDs follow the generated MSL's spvDescriptorSetBuffer2,
    // not declaration order in the GLSL source.
    [buffers setBuffer:media offset:0 atIndex:0];[buffers setBuffer:material offset:0 atIndex:1];
    MTLSamplerDescriptor *sd=[MTLSamplerDescriptor new];
    sd.minFilter=sd.magFilter=MTLSamplerMinMagFilterNearest;sd.supportArgumentBuffers=YES;
    id<MTLSamplerState> sampler=[device newSamplerStateWithDescriptor:sd];CHECK(sampler);
    MTLTextureDescriptor *td=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA32Float
        width:1 height:1 mipmapped:NO];td.storageMode=MTLStorageModeShared;
    id<MTLTexture> black=[device newTextureWithDescriptor:td];
    const float blackPixel[4]={0,0,0,1};
    [black replaceRegion:MTLRegionMake2D(0,0,1,1) mipmapLevel:0 withBytes:blackPixel bytesPerRow:16];
    td.textureType=MTLTextureType2DArray;td.arrayLength=6;
    id<MTLTexture> array=[device newTextureWithDescriptor:td];CHECK(black&&array);
    for(unsigned i=0;i<8;++i){
        [textures setTexture:(i==0||i==3||i==7)?black:array atIndex:i*2];
        [textures setSamplerState:sampler atIndex:i*2+1];
    }
    td=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA32Float width:16 height:16 mipmapped:NO];
    td.storageMode=MTLStorageModeShared;td.usage=MTLTextureUsageRenderTarget;
    id<MTLTexture> output=[device newTextureWithDescriptor:td];CHECK(output);
    auto render=[&](const float *draw,const float *world,const float *color,bool blend,float *pixels)->bool {
        MTLRenderPassDescriptor *pass=[MTLRenderPassDescriptor renderPassDescriptor];
        pass.colorAttachments[0].texture=output;pass.colorAttachments[0].loadAction=MTLLoadActionClear;
        pass.colorAttachments[0].clearColor=MTLClearColorMake(blend?.05:0,blend?.05:0,blend?.05:0,0);
        pass.colorAttachments[0].storeAction=MTLStoreActionStore;
        id<MTLCommandBuffer> command=[queue commandBuffer];
        id<MTLRenderCommandEncoder> encoder=[command renderCommandEncoderWithDescriptor:pass];
        if(!encoder)return false;
        [encoder setRenderPipelineState:blend?beamBlend:pipeline];
        [encoder setVertexBytes:world length:16 atIndex:0];
        [encoder setVertexBytes:color length:16 atIndex:1];
        [encoder setFragmentBuffer:tb offset:0 atIndex:1];[encoder setFragmentBuffer:ub offset:0 atIndex:2];
        [encoder setFragmentBytes:draw length:256 atIndex:3];
        [encoder useResource:material usage:MTLResourceUsageRead stages:MTLRenderStageFragment];
        [encoder useResource:media usage:MTLResourceUsageRead stages:MTLRenderStageFragment];
        [encoder useResource:black usage:MTLResourceUsageRead stages:MTLRenderStageFragment];
        [encoder useResource:array usage:MTLResourceUsageRead stages:MTLRenderStageFragment];
        [encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
        [encoder endEncoding];[command commit];[command waitUntilCompleted];
        if(command.status!=MTLCommandBufferStatusCompleted)return false;
        [output getBytes:pixels bytesPerRow:16*16 fromRegion:MTLRegionMake2D(0,0,16,16) mipmapLevel:0];
        return true;
    };
    unsigned cases=0;
    for(unsigned camera=0;camera<2;++camera)for(float farZ:{64.f,4096.f,65536.f})
    for(float depth:{5.f,17.f,40.f})for(float offset:{0.f,.8f,2.f})for(unsigned alpha:{0u,3u,4u})
    for(float transmission:{0.f,.375f,1.f})for(float emission:{0.f,1.f}){
        for(unsigned z=0;z<64;++z)froxels[z][3]=transmission;
        memcpy(media.contents,froxels,sizeof(froxels));
        const float basePixel[4]={emission,emission,emission,1};
        [black replaceRegion:MTLRegionMake2D(0,0,1,1) mipmapLevel:0 withBytes:basePixel bytesPerRow:16];
        float draw[64]={};draw[7]=draw[11]=draw[15]=1;
        const float eye[3]={camera?123.f:0.f,camera?-47.f:0.f,camera?29.f:0.f};
        const float forward[3]={camera?.8f:1.f,0,camera?.6f:0};
        float world[4]={};
        for(unsigned c=0;c<3;++c){draw[c]=draw[24+c]=eye[c];draw[4+c]=forward[c];
            world[c]=eye[c]+forward[c]*depth;}
        world[1]+=depth*offset;
        draw[22]=float(alpha);draw[31]=farZ;
        const uint32_t grid[4]={1,1,64,0};memcpy(draw+36,grid,sizeof(grid));
        float pixels[16*16*4];const float white[4]={1,1,1,1};
        CHECK(render(draw,world,white,false,pixels));
        unsigned slice=(unsigned)floor(log(double(depth)/4)/log(double(farZ)/4)*64);
        const float expected=emission*transmission+(alpha?0.f:float(slice)/64);
        for(unsigned p=0;p<256;++p)for(unsigned c=0;c<3;++c)if(fabs(pixels[p*4+c]-expected)>1e-5){
            fprintf(stderr,"camera=%u far=%g depth=%g offset=%g alpha=%u T=%g emission=%g actual=%g expected=%g\n",
                camera,farZ,depth,offset,alpha,transmission,emission,pixels[p*4+c],expected);return 1;}
        ++cases;
    }
    printf("OpenGL portable FULL depth/additive GPU: %u cases/%u pixels PASS\n",cases,cases*256);
    // The shared sampling refactor must also preserve analytic surfaces and
    // the screen-space exemption, not merely fix the beam early return.
    unsigned surfaceCases=0;
    for(unsigned camera=0;camera<2;++camera)for(unsigned screen=0;screen<2;++screen)
    for(unsigned alpha:{0u,3u,4u})for(unsigned mode=0;mode<3;++mode)
    for(float emission:{0.f,1.f}) {
        float draw[64]={};draw[3]=float(screen);draw[7]=draw[11]=draw[15]=1;
        const float eye[3]={camera?123.f:0.f,-47,29};
        const float forward[3]={camera?.8f:1.f,0,camera?.6f:0};
        float world[4]={};const float depth=17;
        for(unsigned c=0;c<3;++c){draw[c]=draw[24+c]=eye[c];draw[4+c]=forward[c];
            world[c]=eye[c]+forward[c]*depth;}
        world[1]+=depth*.8f;
        draw[22]=float(alpha);draw[27]=mode?.012f:0;
        draw[28]=.3f;draw[29]=.5f;draw[30]=.7f;
        draw[31]=mode==2?64:0;draw[32]=20;draw[33]=.005f;
        const double density=mode==2?3.912023/64:draw[27];
        const double distance=sqrt(double(depth)*depth+double(depth*.8f)*(depth*.8f));
        const double transmission=screen?1:exp(-density*exp(-double(draw[33])*fmax(world[2]-20.,0.))*distance);
        const float basePixel[4]={emission,emission,emission,.6f};
        [black replaceRegion:MTLRegionMake2D(0,0,1,1) mipmapLevel:0 withBytes:basePixel bytesPerRow:16];
        const float color[4]={1,1,1,.5f};float pixels[16*16*4];
        CHECK(render(draw,world,color,false,pixels));
        for(unsigned c=0;c<4;++c) {
            const double expected=c==3?.3:emission*transmission+(alpha?0:draw[28+c]*(1-transmission));
            for(unsigned p=0;p<256;++p)if(fabs(pixels[p*4+c]-expected)>1e-5) {
                fprintf(stderr,"surface camera=%u screen=%u alpha=%u mode=%u emission=%g channel=%u actual=%g expected=%g\n",
                    camera,screen,alpha,mode,emission,c,pixels[p*4+c],expected);return 1;
            }
        }
        ++surfaceCases;
    }
    printf("OpenGL portable analytic surface/screen-space GPU: %u cases/%u pixels PASS\n",surfaceCases,surfaceCases*256);
    unsigned beamCases=0;
    const uint32_t beamFlag=1;memcpy(factors+20,&beamFlag,sizeof(beamFlag));
    memcpy(material.contents,factors,sizeof(factors));
    for(unsigned mode=0;mode<3;++mode)for(unsigned camera=0;camera<2;++camera)
    for(float emission:{0.f,1.f})for(float tint:{.02f,.5f,1.f})
    for(float opacity:{.25f,1.f})for(bool blend:{false,true}) {
        float draw[64]={};draw[7]=draw[11]=draw[15]=1;
        const float eye[3]={camera?123.f:0.f,-47,29};
        const float forward[3]={camera?.8f:1.f,0,camera?.6f:0};
        float world[4]={};const float depth=17;
        for(unsigned c=0;c<3;++c){draw[c]=draw[24+c]=eye[c];draw[4+c]=forward[c];
            world[c]=eye[c]+forward[c]*depth;}
        world[1]+=depth*.8f;
        draw[22]=4;draw[28]=.3f;draw[29]=.5f;draw[30]=.7f;
        double transmission=1;
        if(mode==1) {
            draw[27]=.012f;draw[32]=20;draw[33]=.005f;
            const double distance=sqrt(double(depth)*depth+double(depth*.8f)*(depth*.8f));
            transmission=exp(-double(draw[27])*exp(-double(draw[33])*fmax(world[2]-20.,0.))*distance);
        } else if(mode==2) {
            draw[31]=64;const uint32_t grid[4]={1,1,64,0};memcpy(draw+36,grid,sizeof(grid));
            for(unsigned z=0;z<64;++z)froxels[z][3]=.2f+.6f*float(z)/63;
            memcpy(media.contents,froxels,sizeof(froxels));
            const unsigned slice=unsigned(floor(log(double(depth)/4)/log(64./4)*64));
            transmission=froxels[slice][3];
        }
        const float basePixel[4]={emission,emission,emission,.6f};
        [black replaceRegion:MTLRegionMake2D(0,0,1,1) mipmapLevel:0 withBytes:basePixel bytesPerRow:16];
        const float color[4]={tint,tint*.8f,tint*.5f,opacity};
        float pixels[16*16*4];CHECK(render(draw,world,color,blend,pixels));
        for(unsigned c=0;c<4;++c) {
            const double linear=c<3 ? (color[c]<=.04045f ? color[c]/12.92
                : pow((color[c]+.055)/1.055,2.4)) : 0;
            const double coverage=double(basePixel[3])*opacity;
            const double expected=c==3 ? coverage : emission*linear*transmission*(blend?coverage:1)+(blend?.05:0);
            for(unsigned p=0;p<256;++p)if(fabs(pixels[p*4+c]-expected)>1e-5) {
                fprintf(stderr,"beam mode=%u camera=%u blend=%d tint=%g alpha=%g channel=%u actual=%g expected=%g\n",
                    mode,camera,blend,tint,opacity,c,pixels[p*4+c],expected);return 1;
            }
        }
        ++beamCases;
    }
    printf("OpenGL portable beam OFF/analytic/FULL colour/coverage GPU: %u cases/%u pixels PASS\n",beamCases,beamCases*256);
    return 0;
} }
