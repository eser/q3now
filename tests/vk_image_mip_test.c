// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef unsigned char byte;
typedef int qboolean;
enum { qfalse, qtrue };
enum { IMGFLAG_MIPMAP=1, IMGFLAG_PICMIP=2, IMGFLAG_NOSCALE=4,
       IMGFLAG_COLORSHIFT=8, IMGFLAG_NOLIGHTSCALE=16, IMGFLAG_LIGHTMAP=32,
       IMGFLAG_DOMAIN_LINEAR=64, IMGFLAG_DOMAIN_SRGB=128 };
typedef int imgFlags_t;
typedef enum { CD_SRGB, CD_LINEAR } colorDomain_t;
typedef struct { int width, height, flags; } image_t;
typedef struct { int integer; } testCvar_t;
static testCvar_t simple, zero;
static testCvar_t *r_simpleMipMaps=&simple, *r_roundImagesDown=&zero,
    *r_nomip=&zero, *r_picmip=&zero, *r_colorMipLevels=&zero;
static struct { int maxTextureSize; } glConfig = {4096};
static struct { int mapLoading; } tr;
#define ARRAY_LEN(x) (sizeof(x)/sizeof((x)[0]))
static unsigned live;
static void *Alloc(int size) {
    assert(size>0); live++;
    return calloc(1,(size_t)size);
}
static void Free(void *p) { assert(p && live); live--; free(p); }
static struct { void *(*Hunk_AllocateTempMemory)(int);
    void (*Hunk_FreeTempMemory)(void *); } ri = {Alloc, Free};
static void ResampleTexture(unsigned *in,int w,int h,unsigned *out,int ow,int oh) {
    (void)in;(void)w;(void)h;(void)out;(void)ow;(void)oh; assert(0);
}
static void R_LightScaleTexture(byte *p,int w,int h,qboolean gamma) {
    (void)p;(void)w;(void)h;(void)gamma; assert(0);
}
static void R_ColorShiftLightingBytes(byte *in,byte *out,qboolean a,qboolean b) {
    (void)in;(void)out;(void)a;(void)b; assert(0);
}

/* CMake extracts these exact production functions, not a parallel algorithm. */
#include "vk_image_mip_impl.inc"

int main(void) {
    assert(R_ImageColorDomain(0)==CD_SRGB);
    assert(R_ImageColorDomain(IMGFLAG_DOMAIN_SRGB)==CD_SRGB);
    assert(R_ImageColorDomain(IMGFLAG_DOMAIN_LINEAR)==CD_LINEAR);
    assert(R_ImageColorDomain(IMGFLAG_NOLIGHTSCALE|IMGFLAG_DOMAIN_LINEAR)==CD_LINEAR);
    assert(R_ImageColorDomain(IMGFLAG_MIPMAP|IMGFLAG_PICMIP)==CD_SRGB);
    const int dims[][2]={{32,64},{64,32},{1,64},{64,1},{1,1},{64,64},
        {66,640},{64,126},{5,5},{1,7},{7,1}};
    for(unsigned d=0;d<ARRAY_LEN(dims);d++)
    for(int srgb=0;srgb<2;srgb++)
    for(simple.integer=0;simple.integer<2;simple.integer++) {
        image_t image={dims[d][0],dims[d][1],
            IMGFLAG_MIPMAP|IMGFLAG_NOLIGHTSCALE};
        if(srgb) image.flags |= IMGFLAG_LIGHTMAP;
        const int bytes=image.width*image.height*4;
        byte *pixels=malloc((size_t)bytes);
        for(int i=0;i<bytes;i+=4) {
            pixels[i]=pixels[i+1]=pixels[i+2]=128; pixels[i+3]=64;
        }
        Image_Upload_Data upload;
        generate_image_upload_data(&image,pixels,&upload);
        assert(upload.base_level_width==image.width);
        assert(upload.base_level_height==image.height);
        int w=image.width,h=image.height,total=0,levels=0;
        for(;;) {
            total+=w*h*4; levels++;
            if(w==1 && h==1) break;
            if(w>1) w>>=1;
            if(h>1) h>>=1;
        }
        assert(upload.mip_levels==levels && upload.buffer_size==total);
        for(int i=0;i<total;i+=4) {
            for(int c=0;c<3;c++) assert(abs((int)upload.buffer[i+c]-128)<=1);
            assert(upload.buffer[i+3]==64);
        }
        Free(upload.buffer); free(pixels); assert(live==0);
    }
    /* Odd source rows must advance by the original stride, not floor(w/2)*2. */
    byte gradient[5*5*4], reduced[2*2*4];
    for(int i=0;i<25;i++) for(int c=0;c<4;c++) gradient[i*4+c]=(byte)i;
    simple.integer=1;
    R_MipMap(reduced,gradient,5,5,qfalse);
    assert(reduced[0]==3 && reduced[4]==5 && reduced[8]==13 && reduced[12]==15);
    /* Weighted repeat addressing must wrap NPOT dimensions, not bit-mask. */
    simple.integer=0;
    R_MipMap(reduced,gradient,5,5,qfalse);
    const int weight[4]={1,2,2,1};
    for(int y=0;y<2;y++) for(int x=0;x<2;x++) {
        int sum=0;
        for(int dy=0;dy<4;dy++) for(int dx=0;dx<4;dx++)
            sum+=weight[dy]*weight[dx]*(((2*y+dy+4)%5)*5+(2*x+dx+4)%5);
        assert(reduced[(y*2+x)*4]==sum/36);
    }
    puts("Vulkan full native-size mip chains: PASS (44 cases + odd-stride/wrap)");
    return 0;
}
