/*
===========================================================================
r_q1_texture.h  —  Q1 embedded miptex expansion, shared across all renderers.

Compiled into each renderer shared library via renderercommon/.
Only includes headers visible to renderercommon compilation:
  - code/qcommon/q_shared.h  (byte, qboolean, Q_stricmp, …)
  - code/renderercommon/tr_public.h  (bspFile_t forward-decl, ri extern)
Deliberately does NOT include tr_common.h or tr_local.h (renderer-specific).
===========================================================================
*/
#pragma once

#include "../qcommon/q_shared.h"
#include "tr_public.h"   /* bspFile_t forward-decl, extern refimport_t ri */

/* Renderer-agnostic image flags passed to the createImage callback.
   Each renderer's callback wrapper maps these to its own imgFlags_t values.
   Values chosen to match renderer/renderervk (most renderers agree; only
   CLAMPTOEDGE differs in renderer2 which uses 0x0040 instead of 0x0004). */
#define Q1_IMGF_NONE    0x00u
#define Q1_IMGF_MIPMAP  0x01u   /* IMGFLAG_MIPMAP  in all renderers */
#define Q1_IMGF_PICMIP  0x02u   /* IMGFLAG_PICMIP  in all renderers */
#define Q1_IMGF_CLAMP   0x04u   /* sentinel for IMGFLAG_CLAMPTOEDGE */
#define Q1_IMGF_NORMAL  0x08u   /* hint: interpret as tangent-space normalmap (renderer2 IMGTYPE_NORMAL) */

/* First fullbright palette index (indices 224-255 are unlit by lightmaps). */
#define Q1_FULLBRIGHT_FIRST  224

/* Maximum miptextures to cache per BSP load. */
#define Q1_MAX_TEXTURES      512

/* Maximum animation frames per group. */
#define Q1_MAX_ANIM_FRAMES   10

/* Texture classification derived from Q1 name prefix. */
typedef enum {
    Q1TC_WALL       = 0,  /* standard wall                                  */
    Q1TC_WATER,           /* * prefix  (water / lava / slime)               */
    Q1TC_SKY,             /* sky* prefix                                     */
    Q1TC_ALPHA,           /* { prefix  (index 255 = transparent)             */
    Q1TC_ANIM,            /* +0* primary animated frame                      */
    Q1TC_ANIM_FRAME,      /* +1*..+9* secondary frames (no standalone shader)*/
} q1TexClass_t;

/* Forward declarations — these types are defined per-renderer in tr_local.h.
   renderercommon compiles inside each renderer shared library so the real
   structs are always available at link time. */
struct image_s;
struct shader_s;

/* Numeric texture animation chain (+0*..+9*), built once at BSP load time.
   One q1AnimChain_t is allocated per animated base texture; every frame
   shader in the chain back-references the same allocation. */
typedef struct q1AnimChain_s {
    int              numFrames;
    struct shader_s *shaders[Q1_MAX_ANIM_FRAMES];
} q1AnimChain_t;

/* Per-miptex entry stored in the module cache after PrepareTextures. */
typedef struct {
    char             name[64];
    struct image_s  *diffuse;             /* RGBA8, fullbright pixels zeroed   */
    struct image_s  *glow;               /* RGBA8 additive glow; NULL if none */
    struct image_s  *normalmap;          /* _n.tga sidecar; NULL if absent    */
    struct image_s  *specular;           /* _s.tga sidecar; NULL if absent    */
    qboolean         hasFullbright;
    q1TexClass_t     cls;

    /* Sky only: 256×128 split into left (backdrop) and right (clouds). */
    struct image_s  *skyBack;            /* 128×128 opaque backdrop            */
    struct image_s  *skyFront;           /* 128×128, palette[0] → alpha=0     */

    /* Animation (+0* primary only). */
    int              numAnimFrames;
    struct image_s  *animDiffuse[Q1_MAX_ANIM_FRAMES];
    struct image_s  *animGlow[Q1_MAX_ANIM_FRAMES];  /* NULL per-slot if none */
    struct image_s  *animArray;   /* VK_IMAGE_VIEW_TYPE_2D_ARRAY, N layers; NULL if single-frame or no array support */

    /* Synthetic shader built by R_Q1_BuildSyntheticShader; NULL until first use. */
    struct shader_s *cachedShader;
} q1TexInfo_t;

/* Callback for image creation — registered by each renderer before
   PrepareTextures.  flags is a bitmask of Q1_IMGF_* values; the renderer
   wrapper maps them to its own imgFlags_t. */
typedef struct image_s *(*R_Q1_CreateImageFn)( const char *name, byte *rgba,
                                                int width, int height,
                                                unsigned int q1flags );

/* Callback for disk-based image loading — wraps R_FindImageFile per renderer.
   path is a full VFS path without extension (renderer searches .tga/.png etc.)
   or with explicit extension.  Returns NULL if the file does not exist.
   Set Q1_IMGF_NORMAL in q1flags to request tangent-space normal processing
   (renderer2 maps this to IMGTYPE_NORMAL; GL1/VK ignore it). */
typedef struct image_s *(*R_Q1_LoadImageFn)( const char *path, unsigned int q1flags );

/* Callback for texture-array creation — optional; renderers that don't support
   2D_ARRAY images leave this NULL.  frames[0..numFrames-1] are RGBA8 buffers,
   all width×height bytes.  flags is a bitmask of Q1_IMGF_* values. */
typedef struct image_s *(*R_Q1_CreateImageArrayFn)( const char *name,
                                                     byte **frames, int numFrames,
                                                     int width, int height,
                                                     unsigned int q1flags );

/* ---- Public API ---- */

void               R_Q1_SetCreateImageFn( R_Q1_CreateImageFn fn );
void               R_Q1_SetLoadImageFn( R_Q1_LoadImageFn fn );
void               R_Q1_SetCreateImageArrayFn( R_Q1_CreateImageArrayFn fn );
void               R_Q1_PrepareTextures( const bspFile_t *bsp );
void               R_Q1_FreeTextures( void );
const q1TexInfo_t *R_Q1_GetTexForName( const char *name );
void               R_Q1_CacheShader( const char *name, struct shader_s *sh );
qboolean           R_Q1_IsActive( void );

/*
R_Q1_ExpandAndUploadSkin
------------------------
Expand width*height indexed palette bytes to RGBA8 and upload via the
registered createImage callback.  Returns the resulting image_t* or NULL
if no callback is registered.

imgName: name to register the image under (e.g. "*mdl_foo_skin0")
src:     indexed pixel data, width*height bytes
w, h:    dimensions
flags:   bitmask of Q1_IMGF_* values
*/
struct image_s *R_Q1_ExpandAndUploadSkin( const char *imgName,
                                           const byte *src, int w, int h,
                                           unsigned int flags );
