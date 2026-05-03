/*
===========================================================================
tr_model_mdl.h  —  Q1 MDL alias model loader, shared across all renderers.

Builds a synthetic md3Header_t blob in hunk memory so renderer/renderervk
can store it in mod->md3[lod], and renderer2 can pass it to its own
R_LoadMD3 to produce an mdvModel_t.
===========================================================================
*/
#pragma once

#include "../qcommon/q_shared.h"
#include "../qcommon/qfiles.h"
#include "tr_public.h"

/* ---- MDL on-disk constants ---- */
/* "IDPO" as little-endian 32-bit int */
#define MDL_IDENT   (('O'<<24)+('P'<<16)+('D'<<8)+'I')
#define MDL_VERSION 6

/* Onseam flag in dstvert_t.onseam */
#define MDL_ONSEAM  0x0020

/* Skin type values */
#define ALIAS_SKIN_SINGLE  0
#define ALIAS_SKIN_GROUP   1

/* Frame type values */
#define ALIAS_FRAME_SINGLE 0
#define ALIAS_FRAME_GROUP  1

/* ---- MDL on-disk structs ---- */

typedef struct {
    int32_t  ident;
    int32_t  version;
    float    scale[3];
    float    scale_origin[3];
    float    boundingradius;
    float    eyeposition[3];
    int32_t  numskins;
    int32_t  skinwidth;
    int32_t  skinheight;
    int32_t  numverts;
    int32_t  numtris;
    int32_t  numframes;
    int32_t  synctype;
    int32_t  flags;
    float    size;
} dmdl_t;

typedef struct {
    int32_t  onseam;
    int32_t  s;
    int32_t  t;
} mdl_stvert_t;

typedef struct {
    int32_t  facesfront;
    int32_t  vertindex[3];
} mdl_triangle_t;

typedef struct {
    byte  v[3];
    byte  lightnormalindex;
} mdl_trivertx_t;

typedef struct {
    mdl_trivertx_t  bboxmin;
    mdl_trivertx_t  bboxmax;
    char            name[16];
} mdl_aliasframe_t;

typedef struct {
    int32_t         numframes;
    mdl_trivertx_t  bboxmin;
    mdl_trivertx_t  bboxmax;
} mdl_aliasgroup_t;

/*
MDL_BuildMD3Buffer
------------------
Parse an MDL file (buffer/filesize), upload skins via R_Q1_CreateImageFn
(must have been registered before this call), and build a self-contained
synthetic md3Header_t blob allocated with ri.Malloc.

On success:
  *outBuf  = pointer to the allocated md3 blob (caller must ri.Free it)
  *outSize = size of the blob in bytes
  returns qtrue

On failure: returns qfalse, *outBuf = NULL.

The resulting blob is a valid md3 layout in little-endian format so that
each renderer can pass it straight to its own static R_LoadMD3().

Skin shader names use the convention "*mdl_<modName>_skin<N>" so that
the renderers find them via R_FindShader after upload.
*/
qboolean MDL_BuildMD3Buffer( const void *buffer, int filesize,
                              const char *modName,
                              void **outBuf, int *outSize );
