// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

/*
===========================================================================
r_q1_texture.c  —  Quake 1 miptex expansion shared across all renderers.

Compiled into each renderer's shared library via renderercommon/.
Memory: ri.Malloc / ri.Free (renderer-side allocator).
Logging: ri.Log (no Com_Log dependency).
Flags: Q1_IMGF_* (renderer-agnostic; each callback maps to imgFlags_t).
===========================================================================
*/

#include "r_q1_texture.h"
#include "maps/bsp.h"             /* full bspFile_t definition */

/* =========================================================================
   Standard Quake 1 palette — 256 RGB triplets (768 bytes total).
   Source: id-Software/Quake repository, palette.lmp.
   Indices 224-255 are the "fullbright" range (never darkened by lightmaps).
   Index 255 is additionally the "transparent" pixel marker for { textures.
   ========================================================================= */

/* Canonical Quake palette — 768 bytes verbatim from id1/pak0.pak gfx/palette.lmp.
   Source: ~/quake-source/pak0_extracted/gfx/palette.lmp
   Indices 224-255 are the fullbright range (lava/fire glow).
   Index 255 is additionally the transparent pixel marker for { textures. */
static const byte q1_palette[768] = {
    /* 0-15 */
      0,  0,  0,   15, 15, 15,   31, 31, 31,   47, 47, 47,
     63, 63, 63,   75, 75, 75,   91, 91, 91,  107,107,107,
    123,123,123,  139,139,139,  155,155,155,  171,171,171,
    187,187,187,  203,203,203,  219,219,219,  235,235,235,
    /* 16-31 */
     15, 11,  7,   23, 15, 11,   31, 23, 11,   39, 27, 15,
     47, 35, 19,   55, 43, 23,   63, 47, 23,   75, 55, 27,
     83, 59, 27,   91, 67, 31,   99, 75, 31,  107, 83, 31,
    115, 87, 31,  123, 95, 35,  131,103, 35,  143,111, 35,
    /* 32-47 */
     11, 11, 15,   19, 19, 27,   27, 27, 39,   39, 39, 51,
     47, 47, 63,   55, 55, 75,   63, 63, 87,   71, 71,103,
     79, 79,115,   91, 91,127,   99, 99,139,  107,107,151,
    115,115,163,  123,123,175,  131,131,187,  139,139,203,
    /* 48-63 */
      0,  0,  0,    7,  7,  0,   11, 11,  0,   19, 19,  0,
     27, 27,  0,   35, 35,  0,   43, 43,  7,   47, 47,  7,
     55, 55,  7,   63, 63,  7,   71, 71,  7,   75, 75, 11,
     83, 83, 11,   91, 91, 11,   99, 99, 11,  107,107, 15,
    /* 64-79 */
      7,  0,  0,   15,  0,  0,   23,  0,  0,   31,  0,  0,
     39,  0,  0,   47,  0,  0,   55,  0,  0,   63,  0,  0,
     71,  0,  0,   79,  0,  0,   87,  0,  0,   95,  0,  0,
    103,  0,  0,  111,  0,  0,  119,  0,  0,  127,  0,  0,
    /* 80-95 */
     19, 19,  0,   27, 27,  0,   35, 35,  0,   47, 43,  0,
     55, 47,  0,   67, 55,  0,   75, 59,  7,   87, 67,  7,
     95, 71,  7,  107, 75, 11,  119, 83, 15,  131, 87, 19,
    139, 91, 19,  151, 95, 27,  163, 99, 31,  175,103, 35,
    /* 96-111 */
     35, 19,  7,   47, 23, 11,   59, 31, 15,   75, 35, 19,
     87, 43, 23,   99, 47, 31,  115, 55, 35,  127, 59, 43,
    143, 67, 51,  159, 79, 51,  175, 99, 47,  191,119, 47,
    207,143, 43,  223,171, 39,  239,203, 31,  255,243, 27,
    /* 112-127 */
     11,  7,  0,   27, 19,  0,   43, 35, 15,   55, 43, 19,
     71, 51, 27,   83, 55, 35,   99, 63, 43,  111, 71, 51,
    127, 83, 63,  139, 95, 71,  155,107, 83,  167,123, 95,
    183,135,107,  195,147,123,  211,163,139,  227,179,151,
    /* 128-143 */
    171,139,163,  159,127,151,  147,115,135,  139,103,123,
    127, 91,111,  119, 83, 99,  107, 75, 87,   95, 63, 75,
     87, 55, 67,   75, 47, 55,   67, 39, 47,   55, 31, 35,
     43, 23, 27,   35, 19, 19,   23, 11, 11,   15,  7,  7,
    /* 144-159 */
    187,115,159,  175,107,143,  163, 95,131,  151, 87,119,
    139, 79,107,  127, 75, 95,  115, 67, 83,  107, 59, 75,
     95, 51, 63,   83, 43, 55,   71, 35, 43,   59, 31, 35,
     47, 23, 27,   35, 19, 19,   23, 11, 11,   15,  7,  7,
    /* 160-175 */
    219,195,187,  203,179,167,  191,163,155,  175,151,139,
    163,135,123,  151,123,111,  135,111, 95,  123, 99, 83,
    107, 87, 71,   95, 75, 59,   83, 63, 51,   67, 51, 39,
     55, 43, 31,   39, 31, 23,   27, 19, 15,   15, 11,  7,
    /* 176-191 */
    111,131,123,  103,123,111,   95,115,103,   87,107, 95,
     79, 99, 87,   71, 91, 79,   63, 83, 71,   55, 75, 63,
     47, 67, 55,   43, 59, 47,   35, 51, 39,   31, 43, 31,
     23, 35, 23,   15, 27, 19,   11, 19, 11,    7, 11,  7,
    /* 192-207 */
    255,243, 27,  239,223, 23,  219,203, 19,  203,183, 15,
    187,167, 15,  171,151, 11,  155,131,  7,  139,115,  7,
    123, 99,  7,  107, 83,  0,   91, 71,  0,   75, 55,  0,
     59, 43,  0,   43, 31,  0,   27, 15,  0,   11,  7,  0,
    /* 208-223 */
      0,  0,255,   11, 11,239,   19, 19,223,   27, 27,207,
     35, 35,191,   43, 43,175,   47, 47,159,   47, 47,143,
     47, 47,127,   47, 47,111,   47, 47, 95,   43, 43, 79,
     35, 35, 63,   27, 27, 47,   19, 19, 31,   11, 11, 15,
    /* 224-239: fullbright — lava/fire glow */
     43,  0,  0,   59,  0,  0,   75,  7,  0,   95,  7,  0,
    111, 15,  0,  127, 23,  7,  147, 31,  7,  163, 39, 11,
    183, 51, 15,  195, 75, 27,  207, 99, 43,  219,127, 59,
    227,151, 79,  231,171, 95,  239,191,119,  247,211,139,
    /* 240-255: fullbright — bright fire / screens */
    167,123, 59,  183,155, 55,  199,195, 55,  231,227, 87,
    127,191,255,  171,231,255,  215,255,255,  103,  0,  0,
    139,  0,  0,  179,  0,  0,  215,  0,  0,  255,  0,  0,
    255,243,147,  255,247,199,  255,255,255,  159, 91, 83,
};

/* =========================================================================
   Module state
   ========================================================================= */

static q1TexInfo_t             s_cache[Q1_MAX_TEXTURES];
static int                     s_cacheCount       = 0;
static R_Q1_CreateImageFn      s_createImage      = NULL;
static R_Q1_LoadImageFn        s_loadImage        = NULL;
static R_Q1_CreateImageArrayFn s_createImageArray = NULL;

void R_Q1_SetCreateImageFn      ( R_Q1_CreateImageFn      fn ) { s_createImage      = fn; }
void R_Q1_SetLoadImageFn        ( R_Q1_LoadImageFn        fn ) { s_loadImage        = fn; }
void R_Q1_SetCreateImageArrayFn ( R_Q1_CreateImageArrayFn fn ) { s_createImageArray = fn; }

/* Per-miptex raw-pixel record used by the texture-array post-pass.
   pixdata points into the BSP lump and remains valid through PrepareTextures. */
typedef struct {
    const byte *pixdata;
    uint32_t    w;
    uint32_t    h;
    qboolean    isAlpha;
} q1FrameScratch_t;

qboolean R_Q1_IsActive( void ) { return ( s_cacheCount > 0 ) ? qtrue : qfalse; }

void R_Q1_FreeTextures( void ) {
    s_cacheCount = 0;
    /* Image objects are owned by the renderer image system and freed by
       R_ShutdownImages; we just clear our pointer table. */
}

const q1TexInfo_t *R_Q1_GetTexForName( const char *name ) {
    int i;
    if ( !name || !name[0] ) return NULL;
    for ( i = 0; i < s_cacheCount; i++ ) {
        if ( Q_stricmp( s_cache[i].name, name ) == 0 )
            return &s_cache[i];
    }
    return NULL;
}

void R_Q1_CacheShader( const char *name, struct shader_s *sh ) {
    int i;
    if ( !name || !name[0] || !sh ) return;
    for ( i = 0; i < s_cacheCount; i++ ) {
        if ( Q_stricmp( s_cache[i].name, name ) == 0 ) {
            s_cache[i].cachedShader = sh;
            return;
        }
    }
}

/* =========================================================================
   Helpers
   ========================================================================= */

static q1TexClass_t ClassifyName( const char *n ) {
    if ( n[0] == '*' )                        return Q1TC_WATER;
    if ( Q_stricmpn( n, "sky", 3 ) == 0 )     return Q1TC_SKY;
    if ( n[0] == '{' )                         return Q1TC_ALPHA;
    if ( n[0] == '+' && n[1] >= '1' && n[1] <= '9' ) return Q1TC_ANIM_FRAME;
    if ( n[0] == '+' && n[1] == '0' )          return Q1TC_ANIM;
    return Q1TC_WALL;
}

static qboolean IsPow2( uint32_t v ) {
    return ( v >= 8 ) && ( v <= 2048 ) && ( ( v & ( v - 1 ) ) == 0 );
}

/* Expand width×height indexed pixels into an RGBA8 buffer already allocated
   by the caller.
   mode 0 = diffuse  (fullbright → transparent black; optional index-255 alpha)
   mode 1 = glow     (only fullbright pixels, rest transparent) */
static void ExpandIndexed( const byte *src, int npixels, byte *dst,
                            int mode, qboolean alphaIdx255 )
{
    int i;
    for ( i = 0; i < npixels; i++ ) {
        byte           idx = src[i];
        qboolean       isFB  = ( idx >= Q1_FULLBRIGHT_FIRST );
        const byte    *rgb   = &q1_palette[idx * 3];
        byte          *out   = dst + i * 4;

        if ( mode == 1 ) {
            out[0] = isFB ? rgb[0] : 0;
            out[1] = isFB ? rgb[1] : 0;
            out[2] = isFB ? rgb[2] : 0;
            out[3] = isFB ?   255  : 0;
        } else {
            if ( isFB || ( alphaIdx255 && idx == 255 ) ) {
                out[0] = out[1] = out[2] = out[3] = 0;
            } else {
                out[0] = rgb[0]; out[1] = rgb[1]; out[2] = rgb[2]; out[3] = 255;
            }
        }
    }
}

/* Allocate a temp RGBA8 buffer, expand, upload, free. */
static struct image_s *ExpandAndUpload( const char *imgName,
                                         const byte *src, int w, int h,
                                         int mode, qboolean alphaIdx255,
                                         unsigned int flags )
{
    struct image_s *img;
    byte *buf;
    if ( !s_createImage ) return NULL;
    buf = (byte *)ri.Malloc( w * h * 4 );
    ExpandIndexed( src, w * h, buf, mode, alphaIdx255 );
    img = s_createImage( imgName, buf, w, h, flags );
    ri.Free( buf );
    return img;
}

/* =========================================================================
   R_Q1_ExpandAndUploadSkin — public API for MDL skin upload
   ========================================================================= */
struct image_s *R_Q1_ExpandAndUploadSkin( const char *imgName,
                                           const byte *src, int w, int h,
                                           unsigned int flags )
{
    return ExpandAndUpload( imgName, src, w, h, 0 /* diffuse mode */,
                            qfalse /* no alpha idx255 */, flags );
}

/* BSP29 miptex header (local copy to avoid Q1-specific include chain). */
typedef struct {
    char     name[16];
    uint32_t width;
    uint32_t height;
    uint32_t offsets[4]; /* byte offsets from start of this struct */
} q1_miptex_hdr_t;

/* =========================================================================
   R_Q1_PrepareTextures
   ========================================================================= */

void R_Q1_PrepareTextures( const bspFile_t *bsp ) {
    const byte   *lump;
    int           lumpLen;
    int32_t       numMipTex;
    const int32_t *offsets;
    q1FrameScratch_t scratch[Q1_MAX_TEXTURES];
    int           i, j;
    int           skipped    = 0;
    int           withGlow   = 0;
    int           withNormal = 0;
    int           totalPx    = 0;

    memset( scratch, 0, sizeof( scratch ) );

    s_cacheCount = 0;   /* reset from any previous map */

    if ( !bsp || !bsp->embeddedTextures || bsp->embeddedTexturesLength < 4 )
        return;

    lump    = bsp->embeddedTextures;
    lumpLen = bsp->embeddedTexturesLength;

    numMipTex = LittleLong( *(const int32_t *)lump );
    if ( numMipTex <= 0 || numMipTex > 2048 ) {
        ri.Log( SEV_WARN, "R_Q1_PrepareTextures: bogus numMipTex %d\n", numMipTex );
        return;
    }

    offsets = (const int32_t *)( lump + 4 );

    for ( i = 0; i < numMipTex && s_cacheCount < Q1_MAX_TEXTURES; i++ ) {
        int32_t                ofs;
        const q1_miptex_hdr_t *hdr;
        uint32_t               w, h, pixOfs;
        const byte            *pixdata;
        q1TexInfo_t           *entry;
        char                   iName[72], gName[72];
        unsigned int           flags;
        qboolean               hasFB = qfalse;
        int                    k;

        ofs = LittleLong( offsets[i] );
        if ( ofs < 0 ) { skipped++; continue; }
        if ( ofs + (int32_t)sizeof( q1_miptex_hdr_t ) > lumpLen ) { skipped++; continue; }

        hdr    = (const q1_miptex_hdr_t *)( lump + ofs );
        w      = LittleLong( hdr->width );
        h      = LittleLong( hdr->height );
        pixOfs = LittleLong( hdr->offsets[0] );

        if ( !IsPow2( w ) || !IsPow2( h ) ) { skipped++; continue; }
        if ( (int32_t)( ofs + pixOfs + w * h ) > lumpLen ) { skipped++; continue; }

        pixdata = lump + ofs + pixOfs;

        /* Parts 2 & 3 — pixel decode + expand sanity (first miptex only) */
        if ( s_cacheCount == 0 ) {
            int pi;
            ri.Log( SEV_INFO, "Miptex '%s' (%ux%u) first 16 pixels:", hdr->name, w, h );
            for ( pi = 0; pi < 16 && pi < (int)(w * h); pi++ )
                ri.Log( SEV_INFO, " %d", (int)pixdata[pi] );
            ri.Log( SEV_INFO, "\n" );

            /* Expand first 4 pixels inline and compare against palette lookup */
            {
                byte tmp[16];
                ExpandIndexed( pixdata, 4, tmp, 0, qfalse );
                ri.Log( SEV_INFO,
                    "Expanded '%s' first 4 RGBA:"
                    " (%d,%d,%d,%d) (%d,%d,%d,%d) (%d,%d,%d,%d) (%d,%d,%d,%d)\n",
                    hdr->name,
                    tmp[0],  tmp[1],  tmp[2],  tmp[3],
                    tmp[4],  tmp[5],  tmp[6],  tmp[7],
                    tmp[8],  tmp[9],  tmp[10], tmp[11],
                    tmp[12], tmp[13], tmp[14], tmp[15] );
                ri.Log( SEV_INFO,
                    "  palette check px[0]=idx%d → (%d,%d,%d), px[1]=idx%d → (%d,%d,%d)\n",
                    (int)pixdata[0],
                    q1_palette[pixdata[0]*3+0], q1_palette[pixdata[0]*3+1], q1_palette[pixdata[0]*3+2],
                    (int)pixdata[1],
                    q1_palette[pixdata[1]*3+0], q1_palette[pixdata[1]*3+1], q1_palette[pixdata[1]*3+2] );
            }
        }

        for ( k = 0; k < (int)( w * h ); k++ ) {
            if ( pixdata[k] >= Q1_FULLBRIGHT_FIRST ) { hasFB = qtrue; break; }
        }

        entry = &s_cache[s_cacheCount];
        memset( entry, 0, sizeof( *entry ) );
        Q_strncpyz( entry->name, hdr->name, sizeof( entry->name ) );
        entry->hasFullbright = hasFB;
        entry->cls           = ClassifyName( hdr->name );

        flags = Q1_IMGF_MIPMAP | Q1_IMGF_PICMIP;
        if ( entry->cls == Q1TC_WATER ) flags = Q1_IMGF_MIPMAP; /* no picmip for liquids */

        /* ---- Sky: split 256×128 into two 128×128 halves ---- */
        if ( entry->cls == Q1TC_SKY && w == 256 && h == 128 ) {
            byte *back  = (byte *)ri.Malloc( 128 * 128 * 4 );
            byte *front = (byte *)ri.Malloc( 128 * 128 * 4 );

            for ( k = 0; k < 128; k++ ) {
                const byte *row  = pixdata + k * 256;
                byte *bRow = back  + k * 128 * 4;
                byte *fRow = front + k * 128 * 4;
                int  p;

                ExpandIndexed( row,       128, bRow, 0, qfalse );
                ExpandIndexed( row + 128, 128, fRow, 0, qfalse );

                /* palette[0] → transparent in cloud layer */
                for ( p = 0; p < 128; p++ ) {
                    if ( row[128 + p] == 0 ) fRow[p*4+3] = 0;
                }
            }

            Com_sprintf( iName, sizeof(iName), "*q1_skyback_%s",  entry->name );
            Com_sprintf( gName, sizeof(gName), "*q1_skyfront_%s", entry->name );
            entry->skyBack  = s_createImage( iName, back,  128, 128, flags );
            entry->skyFront = s_createImage( gName, front, 128, 128,
                                             flags | Q1_IMGF_CLAMP );
            ri.Free( back );
            ri.Free( front );

            /* Also keep a flat diffuse for fallback */
            Com_sprintf( iName, sizeof(iName), "*q1_%s", entry->name );
            entry->diffuse = ExpandAndUpload( iName, pixdata, w, h,
                                              0, qfalse, flags );
            scratch[s_cacheCount].pixdata = pixdata;
            scratch[s_cacheCount].w       = w;
            scratch[s_cacheCount].h       = h;
            scratch[s_cacheCount].isAlpha = qfalse;
            s_cacheCount++;
            continue;
        }

        /* ---- Standard expand (wall / liquid / alpha / anim frames) ---- */
        {
            qboolean isAlpha = ( entry->cls == Q1TC_ALPHA );
            Com_sprintf( iName, sizeof(iName), "*q1_%s", entry->name );
            entry->diffuse = ExpandAndUpload( iName, pixdata, w, h,
                                              0, isAlpha, flags );
            if ( hasFB ) {
                Com_sprintf( gName, sizeof(gName), "*q1_glow_%s", entry->name );
                entry->glow = ExpandAndUpload( gName, pixdata, w, h,
                                               1, qfalse, flags );
                withGlow++;
            }
        }

        /* ---- Disk normalmap / specular sidecar (Q1TC_WALL only) ---- */
        if ( s_loadImage && entry->cls == Q1TC_WALL ) {
            char ovPath[MAX_QPATH];
            Com_sprintf( ovPath, sizeof(ovPath), "textures/q1/%s_n.tga", entry->name );
            entry->normalmap = s_loadImage( ovPath, Q1_IMGF_MIPMAP | Q1_IMGF_NORMAL );
            Com_sprintf( ovPath, sizeof(ovPath), "textures/q1/%s_s.tga", entry->name );
            entry->specular  = s_loadImage( ovPath, Q1_IMGF_MIPMAP );
            if ( entry->normalmap ) withNormal++;
        }

        /* ---- Animated primary: frame 0 is self ---- */
        if ( entry->cls == Q1TC_ANIM ) {
            entry->animDiffuse[0] = entry->diffuse;
            entry->animGlow[0]    = entry->glow;
            entry->numAnimFrames  = 1;

            /* Scan already-committed cache for +1..+9 siblings */
            {
                char base[64];
                Q_strncpyz( base, entry->name + 2, sizeof(base) ); /* skip "+0" */
                for ( k = 1; k < Q1_MAX_ANIM_FRAMES; k++ ) {
                    char sibName[20]; int si;
                    Com_sprintf( sibName, sizeof(sibName), "+%d%s", k, base );
                    for ( si = 0; si < s_cacheCount; si++ ) {
                        if ( Q_stricmp( s_cache[si].name, sibName ) == 0 ) {
                            entry->animDiffuse[k] = s_cache[si].diffuse;
                            entry->animGlow[k]    = s_cache[si].glow;
                            entry->numAnimFrames  = k + 1;
                            break;
                        }
                    }
                    if ( si == s_cacheCount ) break;
                }
            }
        }

        /* ---- Back-fill +0 if this is a +1..+9 sibling ---- */
        if ( entry->cls == Q1TC_ANIM_FRAME && entry->name[0] == '+' ) {
            char base[64], frame0[68];
            int fn = entry->name[1] - '0';
            Q_strncpyz( base, entry->name + 2, sizeof(base) );
            Com_sprintf( frame0, sizeof(frame0), "+0%s", base );
            for ( j = 0; j < s_cacheCount; j++ ) {
                if ( s_cache[j].cls == Q1TC_ANIM &&
                     Q_stricmp( s_cache[j].name, frame0 ) == 0 ) {
                    if ( fn >= 1 && fn < Q1_MAX_ANIM_FRAMES ) {
                        s_cache[j].animDiffuse[fn] = entry->diffuse;
                        s_cache[j].animGlow[fn]    = entry->glow;
                        if ( fn + 1 > s_cache[j].numAnimFrames )
                            s_cache[j].numAnimFrames = fn + 1;
                    }
                    break;
                }
            }
        }

        scratch[s_cacheCount].pixdata = pixdata;
        scratch[s_cacheCount].w       = w;
        scratch[s_cacheCount].h       = h;
        scratch[s_cacheCount].isAlpha = ( entry->cls == Q1TC_ALPHA );

        totalPx += (int)( w * h );
        s_cacheCount++;
    }

    /* Post-pass: build 2D_ARRAY textures for animated primary frames.
       Re-expands raw BSP pixels per frame so we don't keep per-frame RGBA buffers
       alive during the main loop. Requires s_createImageArray to be registered. */
    if ( s_createImageArray ) {
        int arrBuilt = 0;
        for ( i = 0; i < s_cacheCount; i++ ) {
            q1TexInfo_t *entry = &s_cache[i];
            char         arrName[72];
            byte        *frames[Q1_MAX_ANIM_FRAMES];
            int          N, k, si;
            char         base[64];

            if ( entry->cls != Q1TC_ANIM || entry->numAnimFrames <= 1 )
                continue;
            if ( !scratch[i].pixdata )
                continue;

            N = entry->numAnimFrames;
            for ( k = 0; k < Q1_MAX_ANIM_FRAMES; k++ ) frames[k] = NULL;

            /* Frame 0 — this entry */
            frames[0] = (byte *)ri.Malloc( scratch[i].w * scratch[i].h * 4 );
            ExpandIndexed( scratch[i].pixdata, (int)( scratch[i].w * scratch[i].h ),
                           frames[0], 0, scratch[i].isAlpha );

            /* Frames 1..N-1 — siblings located by name */
            Q_strncpyz( base, entry->name + 2, sizeof( base ) );
            for ( k = 1; k < N; k++ ) {
                char sibName[20];
                Com_sprintf( sibName, sizeof( sibName ), "+%d%s", k, base );
                for ( si = 0; si < s_cacheCount; si++ ) {
                    if ( Q_stricmp( s_cache[si].name, sibName ) == 0 && scratch[si].pixdata ) {
                        if ( scratch[si].w != scratch[i].w || scratch[si].h != scratch[i].h ) {
                            ri.Log( SEV_WARN,
                                "[Q1ARRAY] '%s' frame %d dim mismatch (%ux%u vs %ux%u), skipping array\n",
                                entry->name, k,
                                scratch[si].w, scratch[si].h,
                                scratch[i].w,  scratch[i].h );
                            goto cleanup;
                        }
                        frames[k] = (byte *)ri.Malloc( scratch[si].w * scratch[si].h * 4 );
                        ExpandIndexed( scratch[si].pixdata,
                                       (int)( scratch[si].w * scratch[si].h ),
                                       frames[k], 0, scratch[si].isAlpha );
                        break;
                    }
                }
                if ( !frames[k] ) {
                    ri.Log( SEV_WARN, "[Q1ARRAY] '%s' frame %d missing, skipping array\n",
                            entry->name, k );
                    goto cleanup;
                }
            }

            Com_sprintf( arrName, sizeof( arrName ), "*q1_arr_%s", entry->name );
            entry->animArray = s_createImageArray( arrName, frames, N,
                                                   (int)scratch[i].w, (int)scratch[i].h,
                                                   Q1_IMGF_MIPMAP | Q1_IMGF_PICMIP );
            arrBuilt++;
            ri.Log( SEV_DEBUG, "[Q1ARRAY] '%s' numFrames=%d animArray=%s layerCount=%d\n",
                    entry->name, N,
                    entry->animArray ? "non-null" : "null", N );

        cleanup:
            for ( k = 0; k < N; k++ ) {
                if ( frames[k] ) ri.Free( frames[k] );
            }
        }
        ri.Log( SEV_INFO, "R_Q1_PrepareTextures: %d texture arrays built\n", arrBuilt );
    }

    {
        /* Estimate GPU memory: 4 bytes/px × ~1.33 mip chain × 2 (diffuse+glow) */
        float mbGPU = (float)totalPx * 4 * 1.33f * 2.0f / ( 1024.0f * 1024.0f );
        ri.Log( SEV_INFO,
            "R_Q1_PrepareTextures: %d miptextures, %d with fullbright glow,"
            " %d with normalmap, %d skipped, ~%.1f MB GPU\n",
            s_cacheCount, withGlow, withNormal, skipped, mbGPU );
    }
}
