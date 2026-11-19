// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors
// Execute the production resolver with resident-image/VFS boundaries controlled.
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

typedef int qhandle_t;
typedef int qboolean;
enum { qfalse=0, qtrue=1, MAX_QPATH=64, NUM_TEXTURE_BUNDLES=4,
    MAX_IMAGE_ANIMATIONS=8, IMGFLAG_CLAMPTOEDGE=1 };
typedef struct { const char *imgName; unsigned flags; qhandle_t handle; } image_t;
typedef struct { image_t *image[MAX_IMAGE_ANIMATIONS]; int numImageAnimations; } textureBundle_t;
typedef struct { textureBundle_t bundle[NUM_TEXTURE_BUNDLES]; } shaderStage_t;
typedef struct { const char *name; int numUnfoggedPasses; shaderStage_t *stages[3]; } shader_t;
typedef struct { qboolean clampToEdge; } renderMaterialScriptStage_t;
static unsigned publications, warnings;
static image_t *published;
static int Q_stricmp(const char *a, const char *b) {
    while (*a && tolower((unsigned char)*a)==tolower((unsigned char)*b)) { ++a; ++b; }
    return tolower((unsigned char)*a)-tolower((unsigned char)*b);
}
static qboolean Resolve(const char *name, char *out, int size, void *a, void *b, void *c) {
    (void)a; (void)b; (void)c;
    if (strcmp(name,"textures/old-sky.tga")) return qfalse;
    snprintf(out,(size_t)size,"textures/sky/frame.png"); return qtrue;
}
static struct { qboolean (*FS_ResolveResource)(const char *,char *,int,void *,void *,void *); } ri={Resolve};
static qhandle_t FrontendRegisterSkyTexture(image_t *image, qboolean clamp) {
    assert(((image->flags & IMGFLAG_CLAMPTOEDGE)!=0)==(clamp!=qfalse));
    ++publications; published=image; return image->handle;
}
#define R_LOG(...) (++warnings)
#include "vk_sky_stage_resolver_impl.inc"

int main(void) {
    image_t unrelated={"textures/other.png",0,11};
    image_t repeated={"textures/sky/frame.png",0,23};
    image_t clamped={"textures/sky/frame.png",IMGFLAG_CLAMPTOEDGE,29};
    image_t last={"textures/sky/last.png",0,37};
    shaderStage_t first={0}, final={0};
    shader_t shader={"test/sky",3,{&first,NULL,&final}};
    renderMaterialScriptStage_t source={qfalse};
    first.bundle[0].image[0]=&unrelated;
    first.bundle[1].image[0]=&clamped;
    final.bundle[3].numImageAnimations=MAX_IMAGE_ANIMATIONS;
    final.bundle[3].image[2]=&repeated;
    final.bundle[3].image[MAX_IMAGE_ANIMATIONS-1]=&last;
    // Resolve all collapsed bundles and animation frames, not pass0/bundle0.
    assert(FrontendResolveSkyStage(&shader,&source,"textures/old-sky.tga")==23);
    assert(published==&repeated && publications==1 && warnings==0);
    assert(FrontendResolveSkyStage(&shader,&source,"TEXTURES/SKY/LAST.PNG")==37);
    source.clampToEdge=qtrue;
    assert(FrontendResolveSkyStage(&shader,&source,"textures/old-sky.tga")==29);
    assert(published==&clamped);
    // Same path with a different sampler must not silently reuse an image.
    assert(!FrontendResolveSkyStage(&shader,&source,"textures/sky/last.png"));
    assert(publications==3 && warnings==1);
    source.clampToEdge=qfalse;
    assert(!FrontendResolveSkyStage(&shader,&source,"textures/missing.png"));
    assert(publications==3 && warnings==2);
    assert(!FrontendResolveSkyStage(NULL,&source,"textures/sky/frame.png"));
    assert(!FrontendResolveSkyStage(&shader,NULL,"textures/sky/frame.png"));
    assert(!FrontendResolveSkyStage(&shader,&source,NULL));
    assert(!FrontendResolveSkyStage(&shader,&source,""));
    assert(publications==3);
    ri.FS_ResolveResource=NULL;
    assert(FrontendResolveSkyStage(&shader,&source,"textures/sky/frame.png")==23);
    assert(!FrontendResolveSkyStage(&shader,&source,"textures/old-sky.tga"));
    // A damaged count cannot index outside the bounded resident frame array.
    final.bundle[3].numImageAnimations=MAX_IMAGE_ANIMATIONS+100;
    assert(!FrontendResolveSkyStage(&shader,&source,"textures/missing.png"));
    puts("Vulkan sky resolver: canonical identity, all bundles/frames, exact sampler, no substitution PASS");
    return 0;
}
