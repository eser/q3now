// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 1999-2005 Id Software, Inc.
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
#ifndef TR_COMMON_H
#define TR_COMMON_H

#define USE_VULKAN

#include "../qcommon/q_shared.h"
#include "../renderercommon/tr_public.h"

#define MAX_TEXTURE_UNITS 8

typedef enum
{
	IMGFLAG_NONE           = 0x0000,
	IMGFLAG_MIPMAP         = 0x0001,
	IMGFLAG_PICMIP         = 0x0002,
	IMGFLAG_CLAMPTOEDGE    = 0x0004,
	IMGFLAG_CLAMPTOBORDER  = 0x0008,
	IMGFLAG_NO_COMPRESSION = 0x0010,
	IMGFLAG_NOLIGHTSCALE   = 0x0020,
	IMGFLAG_LIGHTMAP       = 0x0040,
	IMGFLAG_NOSCALE        = 0x0080,
	IMGFLAG_RGB            = 0x0100,
	IMGFLAG_COLORSHIFT     = 0x0200,
	IMGFLAG_ARRAY          = 0x0400,  /* 2D_ARRAY image; layerCount gives depth */
	/* explicit colour-domain overrides (shader keywords linearMap /
	   srgbMap / gammaMap). When neither is set, R_CreateImage auto-classifies
	   by filename suffix. */
	IMGFLAG_DOMAIN_LINEAR  = 0x0800,
	IMGFLAG_DOMAIN_SRGB    = 0x1000,
	/* caller wants a cubemap (shader keyword `cubeMap`). The DDS
	   loader auto-detects cube/volume from the file header regardless; this
	   flag only records intent so a mismatch (file isn't a cubemap) can warn. */
	IMGFLAG_CUBEMAP        = 0x2000,
	/* texture must never be evicted by the texture-LRU (Phase 7.15.4). Stamped on
	   procedural/no-disk-source built-ins + lightmaps (class A, at R_CreateImage),
	   on arbitrary-named UI/persistent atlases whose consumer caches the image_t
	   pointer or bindless slot without re-checking residency (class B, post-register
	   via RE_PinShaderImages), and on skybox/cubemap/animMap frames that are live
	   but look idle to a frameUsed LRU (class C, at shader-parse). Read by the
	   victim-scan in 7.15.4-c (R_ImageIsPinned); has NO reader in 7.15.4-a, so it
	   is a dark classification. */
	IMGFLAG_PINNED         = 0x4000,
	/* transient: this evicted texture (ral==NULL) was sampled this frame and is
	   queued for automatic re-register at the next render-thread drain boundary
	   (Phase 7.15.3 bind-miss handler). Set in vk_bindless_track (enqueue-only,
	   O(1) idempotent dedup — many draws sampling the same evicted texture set it
	   once), cleared by vk_ral_drain_reregisters after vk_ral_reregister_image
	   restores it. Renderer-internal, transient — NOT a classification. */
	IMGFLAG_REREGISTER_PENDING = 0x8000,
	/* explicit whole-texture residency state. Set only by the scheduler-owned
	   eviction transition and cleared after successful RAL recreation. This
	   distinguishes a genuinely evicted page from images that never had a RAL
	   allocation (unsupported type/data/capacity), so bind-miss recovery cannot
	   manufacture a false request. */
	IMGFLAG_RESIDENCY_EVICTED  = 0x10000,
} imgFlags_t;

// image dimensionality. The DDS loader classifies by header
// (DDSCAPS2_CUBEMAP / DDSCAPS2_VOLUME, or the DXT10 resourceDimension /
// D3D10_RESOURCE_MISC_TEXTURECUBE). image_t.texType drives vk_create_image's
// VkImageType / view type / arrayLayers; CUBE_ARRAY is reserved (the loader
// does not produce it yet). TEXTYPE_2D == 0 so a zero-initialised image_t is
// a plain 2D texture by default.
typedef enum {
	TEXTYPE_2D         = 0,
	TEXTYPE_CUBE       = 1,   // 6 array layers, VK_IMAGE_VIEW_TYPE_CUBE
	TEXTYPE_3D         = 2,   // extent.depth slices, VK_IMAGE_VIEW_TYPE_3D
	TEXTYPE_CUBE_ARRAY = 3,   // reserved
} texType_t;

// extra DDS classification returned by R_LoadDDS alongside the
// raw mip buffer. layers == 6 for a plain cubemap (6*N for a cube array, not
// yet produced); depth > 1 for a volume texture. numMips is the per-face /
// per-volume mip-chain length (returned separately by R_LoadDDS as before).
typedef struct {
	texType_t texType;
	int       layers;   // VkImageCreateInfo.arrayLayers (1 for 2D / 3D)
	int       depth;    // VkImageCreateInfo.extent.depth (1 for 2D / cube)
} ddsImageInfo_t;

typedef enum {
	CT_FRONT_SIDED = 0,
	CT_BACK_SIDED,
	CT_TWO_SIDED
} cullType_t;

typedef struct image_s image_t;

// any change in the LIGHTMAP_* defines here MUST be reflected in
// R_FindShader() in tr_map.c
#ifndef LIGHTMAP_2D
#define LIGHTMAP_2D         -4	// shader is for 2D rendering
#define LIGHTMAP_BY_VERTEX  -3	// pre-lit triangle models
#define LIGHTMAP_WHITEIMAGE -2
#define LIGHTMAP_NONE       -1
#endif

extern glconfig_t	glConfig;		// outside of TR since it shouldn't be cleared during ref re-init

// These variables should live inside glConfig but can't because of
// compatibility issues to the original ID vms.  If you release a stand-alone
// game and your mod uses tr_types.h from this build you can safely move them
// to the glconfig_t struct.
extern qboolean  textureFilterAnisotropic;
extern int       maxAnisotropy;

//
// cvars
//
//extern cvar_t *r_stencilBits;			// number of desired stencil bits
extern cvar_t *r_textureBits;			// number of desired texture bits
										// 0 = use framebuffer depth
										// 16 = use 16-bit textures
										// 32 = use 32-bit textures
										// all else = error

// r_useRALTextures / r_useRALBuffers / r_useRALPipelines retired. The RAL
// backend is now unconditional; the cvars all gated trivially-true branches
// with no disabled fallback path.

extern cvar_t *r_drawBuffer;

extern cvar_t *r_allowExtensions;				// global enable/disable of OpenGL extensions
extern cvar_t *r_ext_compressed_textures;		// these control use of specific extensions
extern cvar_t *r_ext_multitexture;
extern cvar_t *r_ext_compiled_vertex_array;
extern cvar_t *r_ext_texture_env_add;

extern cvar_t *r_ext_texture_filter_anisotropic;
extern cvar_t *r_ext_max_anisotropy;

float R_NoiseGet4f( float x, float y, float z, double t );
void  R_NoiseInit( void );

image_t *R_FindImageFile( const char *name, imgFlags_t flags );
image_t *R_CreateImage( const char *name, const char *name2, byte *pic, int width, int height, imgFlags_t flags );
image_t *R_CreateImageArray( const char *name, byte **frames, int numFrames, int width, int height, imgFlags_t flags );
void R_UploadSubImage( byte *data, int x, int y, int width, int height, image_t *image );

qhandle_t RE_RegisterShaderLightMap( const char *name, int lightmapIndex );
qhandle_t RE_RegisterShader( const char *name );
qhandle_t RE_RegisterShaderNoMip( const char *name );
qhandle_t RE_RegisterMSDFShader( const char *name, float distanceRange, int atlasWidth, int atlasHeight );
qhandle_t RE_RegisterPrimitiveShader( const char *name );
void      RE_PinShaderImages( qhandle_t hShader );   // Phase 7.15.4-a class-B/C pin (see refexport_t)
qhandle_t RE_RegisterShaderFromImage(const char *name, int lightmapIndex, image_t *image, qboolean mipRawImage);

void RE_SetMSDFOutline( float outlineWidth, const float *outlineColor,
                         float glowWidth, const float *glowColor );

// font stuff
void R_InitFreeType( void );
void R_DoneFreeType( void );
void RE_RegisterFont(const char *fontName, int pointSize, fontInfo_t *font);

/*
=============================================================

IMAGE LOADERS

=============================================================
*/

void R_LoadBMP( const char *name, byte **pic, int *width, int *height );
void R_LoadJPG( const char *name, byte **pic, int *width, int *height );
void R_LoadPCX( const char *name, byte **pic, int *width, int *height );
void R_LoadPNG( const char *name, byte **pic, int *width, int *height );
void R_LoadTGA( const char *name, byte **pic, int *width, int *height );

// PNG encoder. Inputs are bottom-up RGB (the
// vk_read_pixels / RB_ReadPixels output convention); outputs are 8-bit
// truecolour PNGs using zlib STORED blocks (no real DEFLATE — see
// tr_image_png_write.c). R_EncodePNG returns the bytes via a Z_Malloc
// buffer the caller must ri.Free; R_SavePNG goes straight to FS_WriteFile.
qboolean R_EncodePNG( const byte *rgb_bottomup, int width, int height,
                     byte **outBytes, int *outLen );
qboolean R_SavePNG( const char *fileName, const byte *rgb_bottomup, int width, int height );
// DDS BCn loader. Returns the raw mip-chain buffer; caller
// owns *pic (ri.Free) and inspects *picFormat / *numMips / *dataSize
// to schedule the compressed upload (vk_upload_image_data_compressed).
// *info (may be NULL) receives the cubemap / volume / array
// classification — see ddsImageInfo_t. *numMips is the per-face mip count
// for cubemaps and the per-volume mip count for 3D textures.
void R_LoadDDS( const char *name, byte **pic, int *width, int *height, VkFormat *picFormat, int *numMips, int *dataSize, ddsImageInfo_t *info );

/*
====================================================================

IMPLEMENTATION SPECIFIC FUNCTIONS

====================================================================
*/

#endif
