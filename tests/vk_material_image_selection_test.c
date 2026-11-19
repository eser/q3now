// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors
#include <assert.h>
#include <stddef.h>
#include <stdio.h>

typedef int qhandle_t;
typedef struct {
    int ralTextureCacheKeyReady, ralTextureCacheKey;
    struct { int resource; } ralMaterialRuntimeResource;
} image_t;
typedef struct { image_t *image[1]; } textureBundle_t;
#define NUM_TEXTURE_BUNDLES 4
typedef struct { textureBundle_t bundle[NUM_TEXTURE_BUNDLES]; } shaderStage_t;
typedef struct { int numUnfoggedPasses; shaderStage_t *stages[2]; } shader_t;
static shader_t *selected;
static shader_t *FrontendShaderByName(const char *name, qhandle_t handle) {
    (void)name; (void)handle; return selected;
}
static int Ral_TextureCacheKeyValid(const int *key) { return *key == 1; }
static int Ral_TextureResourceReceiptValid(const int *r) { return *r == 1; }
#include "vk_material_image_selection_impl.inc"
int main(void) {
    image_t lightmap={0,0,{0}}, colour={1,1,{1}}, normal={1,1,{1}};
    shaderStage_t first={0}, second={0};
    shader_t shader={2,{&first,&second}};
    selected=&shader;
    first.bundle[0].image[0]=&lightmap;
    first.bundle[1].image[0]=&colour;
    first.bundle[2].image[0]=&normal;
    assert(FrontendMaterialBaseImage("collapsed",0)==&colour);
    first.bundle[1].image[0]=NULL;
    assert(FrontendMaterialBaseImage("no-colour",0)==NULL);
    second.bundle[0].image[0]=&colour;
    assert(FrontendMaterialBaseImage("separate",0)==&colour);
    colour.ralMaterialRuntimeResource.resource=0;
    assert(FrontendMaterialBaseImage("invalid",0)==NULL);
    shader.stages[0]=NULL; selected=NULL;
    assert(FrontendMaterialBaseImage("missing",0)==NULL);
    puts("vk_material_image_selection_test: ok");
}
