// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

/*
===========================================================================
tr_model_mdl.c  —  Q1 MDL (IDPO) alias model loader, shared across all renderers.

Parses the MDL binary format, expands indexed skins to RGBA8, and builds a
self-contained synthetic md3Header_t blob (allocated with ri.Malloc) that
each renderer's R_RegisterMDL wrapper passes to its own static R_LoadMD3().

MDL format reference: FTEQW engine/client/modelgen.h (ALIAS_VERSION 6).
===========================================================================
*/

#include <math.h>

#include "../qcommon/q_shared.h"
#include "../qcommon/qfiles.h"
#include "tr_public.h"
#include "tr_model_mdl.h"
#include "r_q1_texture.h"

/* bytedirs[NUMVERTEXNORMALS] — declared in q_shared.h, defined in q_math.c */

/* -------------------------------------------------------------------------
   Seam expansion helpers
   MDL verts on the seam need to be duplicated: one copy for front-facing
   triangles (s as-is), one for back-facing (s + skinwidth/2).
   -------------------------------------------------------------------------*/

#define MDL_MAX_EXPANDED_VERTS  (MD3_MAX_VERTS * 2)

typedef struct {
    int  origIdx;
    int  backSeam;
} mdl_expandedVert_t;

typedef struct {
    int  origIdx;
    int  backSeam;
    int  expandedIdx;
} mdl_seamEntry_t;

/* -------------------------------------------------------------------------
   XyzNormal encoding — matches MD3 spherical convention.
   -------------------------------------------------------------------------*/
static void MDL_EncodeNormal( const float n[3], short *out )
{
    float lng, lat;
    int   ilat, ilng;

    if ( n[0] == 0.0f && n[1] == 0.0f ) {
        ilat = 0;
        ilng = ( n[2] > 0.0f ) ? 0 : 128;
    } else {
        /* atan2f/acosf: float variants avoid double-promotion */
        lng = atan2f( n[1], n[0] ) * ( 255.0f / ( 2.0f * 3.14159265358979323846f ) );
        lat = acosf( n[2] ) * ( 255.0f / ( 2.0f * 3.14159265358979323846f ) );
        ilat = (int)lat  & 0xFF;
        ilng = ( (int)lng + 256 ) & 0xFF;
    }
    *out = (short)( ( ilat << 8 ) | ilng );
}

/* -------------------------------------------------------------------------
   MDL_BuildMD3Buffer
   -------------------------------------------------------------------------*/
qboolean MDL_BuildMD3Buffer( const void *buffer, int filesize,
                              const char *modName,
                              void **outBuf, int *outSize )
{
    const byte    *p    = (const byte *)buffer;
    const byte    *pEnd = p + filesize;
    const dmdl_t  *phdr;
    const char    *baseName; /* last path component of modName */
    const char    *sl;

    int   i, j, f;
    int   numverts, numtris, numframes, numskins;
    int   skinwidth, skinheight, skinpixels;
    float scale[3], scale_origin[3];
    int   expandedCount = 0;
    int   totalFrames   = 0;
    int   numValidSkins = 0;

    /* Seam map */
    static mdl_expandedVert_t s_expanded[MDL_MAX_EXPANDED_VERTS];
    static int                s_remappedTris[MD3_MAX_TRIANGLES][3];
    static mdl_seamEntry_t    s_seamMap[MDL_MAX_EXPANDED_VERTS];
    int                       s_seamCount = 0;

    /* Skin shader names */
    static char  s_skinName[MD3_MAX_SHADERS][MAX_QPATH];

    /* Per-vert UV (per expanded vert) */
    static float s_st[MDL_MAX_EXPANDED_VERTS][2];

    /* Per-frame XYZ and normals */
    static float s_xyz[MD3_MAX_FRAMES][MDL_MAX_EXPANDED_VERTS][3];
    static float s_nrm[MD3_MAX_FRAMES][MDL_MAX_EXPANDED_VERTS][3];

    /* Per-frame bounds */
    static float s_bmin[MD3_MAX_FRAMES][3];
    static float s_bmax[MD3_MAX_FRAMES][3];

    /* MDL on-disk data buffers */
    static mdl_stvert_t    s_stverts[MD3_MAX_VERTS];
    static mdl_triangle_t  s_tris[MD3_MAX_TRIANGLES];
    static mdl_trivertx_t  s_frameverts[MD3_MAX_VERTS];

    *outBuf  = NULL;
    *outSize = 0;

    /* Extract base name (last component) */
    baseName = modName;
    for ( sl = modName; *sl; sl++ )
        if ( *sl == '/' || *sl == '\\' )
            baseName = sl + 1;

    /* ---- Validate header ---- */
    if ( filesize < (int)sizeof( dmdl_t ) ) {
        ri.Log( SEV_WARN, "%s: %s too small for MDL header\n", __func__, modName );
        return qfalse;
    }

    phdr = (const dmdl_t *)p;
    if ( LittleLong( phdr->ident )   != MDL_IDENT  ||
         LittleLong( phdr->version ) != MDL_VERSION ) {
        ri.Log( SEV_WARN, "%s: %s: bad MDL ident/version\n", __func__, modName );
        return qfalse;
    }

    numskins   = LittleLong( phdr->numskins );
    skinwidth  = LittleLong( phdr->skinwidth );
    skinheight = LittleLong( phdr->skinheight );
    numverts   = LittleLong( phdr->numverts );
    numtris    = LittleLong( phdr->numtris );
    numframes  = LittleLong( phdr->numframes );
    for ( i = 0; i < 3; i++ ) {
        scale[i]        = LittleFloat( phdr->scale[i] );
        scale_origin[i] = LittleFloat( phdr->scale_origin[i] );
    }
    skinpixels = skinwidth * skinheight;

    if ( numskins  < 1 || numskins  > MD3_MAX_SHADERS   ||
         skinwidth  < 1 || skinheight < 1                 ||
         numverts   < 1 || numverts   > MD3_MAX_VERTS     ||
         numtris    < 1 || numtris    > MD3_MAX_TRIANGLES ||
         numframes  < 1 || numframes  > MD3_MAX_FRAMES ) {
        ri.Log( SEV_WARN, "%s: %s out-of-range MDL counts\n", __func__, modName );
        return qfalse;
    }

    p += sizeof( dmdl_t );

    /* ==================================================================
       Phase 1: Parse skins
       ================================================================== */
    for ( i = 0; i < numskins; i++ ) {
        int32_t skintype;
        if ( p + 4 > pEnd ) goto trunc;
        skintype = LittleLong( *(const int32_t *)p );
        p += 4;

        if ( skintype == ALIAS_SKIN_SINGLE ) {
            if ( p + skinpixels > pEnd ) goto trunc;
            if ( numValidSkins < MD3_MAX_SHADERS ) {
                Com_sprintf( s_skinName[numValidSkins], MAX_QPATH,
                             "*mdl_%.48s_skin%d", baseName, numValidSkins );
                R_Q1_ExpandAndUploadSkin( s_skinName[numValidSkins],
                                          p, skinwidth, skinheight,
                                          Q1_IMGF_MIPMAP | Q1_IMGF_PICMIP );
                numValidSkins++;
            }
            p += skinpixels;
        } else {
            /* ALIAS_SKIN_GROUP */
            int32_t ngroup;
            if ( p + 4 > pEnd ) goto trunc;
            ngroup = LittleLong( *(const int32_t *)p );
            p += 4;
            if ( ngroup < 1 ) goto trunc;
            /* skip float intervals */
            if ( p + ngroup * 4 > pEnd ) goto trunc;
            p += ngroup * 4;
            /* upload first frame only */
            if ( p + skinpixels > pEnd ) goto trunc;
            if ( numValidSkins < MD3_MAX_SHADERS ) {
                ri.Log( SEV_WARN, "%s: %s skin %d is a group; using first frame\n",
                        __func__, modName, i );
                Com_sprintf( s_skinName[numValidSkins], MAX_QPATH,
                             "*mdl_%.48s_skin%d", baseName, numValidSkins );
                R_Q1_ExpandAndUploadSkin( s_skinName[numValidSkins],
                                          p, skinwidth, skinheight,
                                          Q1_IMGF_MIPMAP | Q1_IMGF_PICMIP );
                numValidSkins++;
            }
            /* skip all group pixel data */
            if ( p + (size_t)ngroup * skinpixels > pEnd ) goto trunc;
            p += (size_t)ngroup * skinpixels;
        }
    }

    if ( numValidSkins == 0 ) {
        ri.Log( SEV_WARN, "%s: %s no valid skins\n", __func__, modName );
        return qfalse;
    }

    /* ==================================================================
       Phase 2: Parse ST verts
       ================================================================== */
    if ( p + numverts * (int)sizeof( mdl_stvert_t ) > pEnd ) goto trunc;
    {
        const mdl_stvert_t *sv = (const mdl_stvert_t *)p;
        for ( i = 0; i < numverts; i++ ) {
            s_stverts[i].onseam = LittleLong( sv[i].onseam );
            s_stverts[i].s      = LittleLong( sv[i].s );
            s_stverts[i].t      = LittleLong( sv[i].t );
        }
    }
    p += numverts * sizeof( mdl_stvert_t );

    /* ==================================================================
       Phase 3: Parse triangles
       ================================================================== */
    if ( p + numtris * (int)sizeof( mdl_triangle_t ) > pEnd ) goto trunc;
    {
        const mdl_triangle_t *dt = (const mdl_triangle_t *)p;
        for ( i = 0; i < numtris; i++ ) {
            s_tris[i].facesfront   = LittleLong( dt[i].facesfront );
            s_tris[i].vertindex[0] = LittleLong( dt[i].vertindex[0] );
            s_tris[i].vertindex[1] = LittleLong( dt[i].vertindex[1] );
            s_tris[i].vertindex[2] = LittleLong( dt[i].vertindex[2] );
        }
    }
    p += numtris * sizeof( mdl_triangle_t );

    /* ==================================================================
       Phase 4: Seam expansion — build expanded vert table
       ================================================================== */
    for ( i = 0; i < numtris; i++ ) {
        int ff = s_tris[i].facesfront;
        for ( j = 0; j < 3; j++ ) {
            int orig   = s_tris[i].vertindex[j];
            int onseam = ( s_stverts[orig].onseam & MDL_ONSEAM ) ? 1 : 0;
            int back   = onseam && !ff;
            int expIdx = -1;
            int k;
            /* Linear scan for existing entry */
            for ( k = 0; k < s_seamCount; k++ ) {
                if ( s_seamMap[k].origIdx == orig && s_seamMap[k].backSeam == back ) {
                    expIdx = s_seamMap[k].expandedIdx;
                    break;
                }
            }
            if ( expIdx < 0 ) {
                if ( expandedCount >= MDL_MAX_EXPANDED_VERTS ||
                     s_seamCount   >= MDL_MAX_EXPANDED_VERTS ) {
                    expIdx = orig; /* clamp */
                } else {
                    expIdx = expandedCount;
                    s_expanded[expandedCount].origIdx  = orig;
                    s_expanded[expandedCount].backSeam = back;
                    s_seamMap[s_seamCount].origIdx     = orig;
                    s_seamMap[s_seamCount].backSeam    = back;
                    s_seamMap[s_seamCount].expandedIdx = expIdx;
                    s_seamCount++;
                    expandedCount++;
                }
            }
            s_remappedTris[i][j] = expIdx;
        }
    }

    if ( expandedCount == 0 ) {
        ri.Log( SEV_WARN, "%s: %s no expanded verts\n", __func__, modName );
        return qfalse;
    }

    /* Assign UV per expanded vert */
    for ( i = 0; i < expandedCount; i++ ) {
        int   orig = s_expanded[i].origIdx;
        float sf   = (float)s_stverts[orig].s;
        float tf   = (float)s_stverts[orig].t;
        if ( s_expanded[i].backSeam )
            sf += (float)skinwidth * 0.5f;
        s_st[i][0] = ( sf + 0.5f ) / (float)skinwidth;
        s_st[i][1] = ( tf + 0.5f ) / (float)skinheight;
    }

    /* ==================================================================
       Phase 5: Parse frames
       ================================================================== */
    for ( f = 0; f < numframes; f++ ) {
        int32_t frametype;
        if ( p + 4 > pEnd ) goto trunc;
        frametype = LittleLong( *(const int32_t *)p );
        p += 4;

        if ( frametype == ALIAS_FRAME_SINGLE ) {
            /* Skip daliasframe_t: bboxmin(4) + bboxmax(4) + name[16] */
            if ( p + (int)sizeof( mdl_aliasframe_t ) > pEnd ) goto trunc;
            p += sizeof( mdl_aliasframe_t );

            if ( p + numverts * (int)sizeof( mdl_trivertx_t ) > pEnd ) goto trunc;
            if ( totalFrames < MD3_MAX_FRAMES ) {
                const mdl_trivertx_t *fv = (const mdl_trivertx_t *)p;
                float bmin[3] = {  1e30f,  1e30f,  1e30f };
                float bmax[3] = { -1e30f, -1e30f, -1e30f };

                for ( i = 0; i < numverts; i++ )
                    s_frameverts[i] = fv[i];

                for ( i = 0; i < expandedCount; i++ ) {
                    int   ni;
                    float x, y, z;
                    int orig = s_expanded[i].origIdx;
                    x = scale[0] * (float)s_frameverts[orig].v[0] + scale_origin[0];
                    y = scale[1] * (float)s_frameverts[orig].v[1] + scale_origin[1];
                    z = scale[2] * (float)s_frameverts[orig].v[2] + scale_origin[2];
                    s_xyz[totalFrames][i][0] = x;
                    s_xyz[totalFrames][i][1] = y;
                    s_xyz[totalFrames][i][2] = z;
                    ni = (int)(unsigned char)s_frameverts[orig].lightnormalindex;
                    if ( ni < 0 || ni >= NUMVERTEXNORMALS ) ni = 0;
                    s_nrm[totalFrames][i][0] = bytedirs[ni][0];
                    s_nrm[totalFrames][i][1] = bytedirs[ni][1];
                    s_nrm[totalFrames][i][2] = bytedirs[ni][2];
                    if ( x < bmin[0] ) bmin[0] = x;
                    if ( y < bmin[1] ) bmin[1] = y;
                    if ( z < bmin[2] ) bmin[2] = z;
                    if ( x > bmax[0] ) bmax[0] = x;
                    if ( y > bmax[1] ) bmax[1] = y;
                    if ( z > bmax[2] ) bmax[2] = z;
                }
                s_bmin[totalFrames][0] = bmin[0];
                s_bmin[totalFrames][1] = bmin[1];
                s_bmin[totalFrames][2] = bmin[2];
                s_bmax[totalFrames][0] = bmax[0];
                s_bmax[totalFrames][1] = bmax[1];
                s_bmax[totalFrames][2] = bmax[2];
                totalFrames++;
            }
            p += numverts * sizeof( mdl_trivertx_t );

        } else {
            /* ALIAS_FRAME_GROUP */
            int32_t ngroup;
            if ( p + (int)sizeof( mdl_aliasgroup_t ) > pEnd ) goto trunc;
            ngroup = LittleLong( ((const mdl_aliasgroup_t *)p)->numframes );
            p += sizeof( mdl_aliasgroup_t );
            /* skip intervals */
            if ( p + ngroup * 4 > pEnd ) goto trunc;
            p += ngroup * 4;

            for ( j = 0; j < ngroup; j++ ) {
                if ( p + (int)sizeof( mdl_aliasframe_t ) > pEnd ) goto trunc;
                p += sizeof( mdl_aliasframe_t );
                if ( p + numverts * (int)sizeof( mdl_trivertx_t ) > pEnd ) goto trunc;
                if ( totalFrames < MD3_MAX_FRAMES ) {
                    const mdl_trivertx_t *fv = (const mdl_trivertx_t *)p;
                    float bmin[3] = {  1e30f,  1e30f,  1e30f };
                    float bmax[3] = { -1e30f, -1e30f, -1e30f };
                    int   k;
                    for ( k = 0; k < numverts; k++ )
                        s_frameverts[k] = fv[k];
                    for ( k = 0; k < expandedCount; k++ ) {
                        int   ni;
                        float x, y, z;
                        int orig = s_expanded[k].origIdx;
                        x = scale[0] * (float)s_frameverts[orig].v[0] + scale_origin[0];
                        y = scale[1] * (float)s_frameverts[orig].v[1] + scale_origin[1];
                        z = scale[2] * (float)s_frameverts[orig].v[2] + scale_origin[2];
                        s_xyz[totalFrames][k][0] = x;
                        s_xyz[totalFrames][k][1] = y;
                        s_xyz[totalFrames][k][2] = z;
                        ni = (int)(unsigned char)s_frameverts[orig].lightnormalindex;
                        if ( ni < 0 || ni >= NUMVERTEXNORMALS ) ni = 0;
                        s_nrm[totalFrames][k][0] = bytedirs[ni][0];
                        s_nrm[totalFrames][k][1] = bytedirs[ni][1];
                        s_nrm[totalFrames][k][2] = bytedirs[ni][2];
                        if ( x < bmin[0] ) bmin[0] = x;
                        if ( y < bmin[1] ) bmin[1] = y;
                        if ( z < bmin[2] ) bmin[2] = z;
                        if ( x > bmax[0] ) bmax[0] = x;
                        if ( y > bmax[1] ) bmax[1] = y;
                        if ( z > bmax[2] ) bmax[2] = z;
                    }
                    s_bmin[totalFrames][0] = bmin[0];
                    s_bmin[totalFrames][1] = bmin[1];
                    s_bmin[totalFrames][2] = bmin[2];
                    s_bmax[totalFrames][0] = bmax[0];
                    s_bmax[totalFrames][1] = bmax[1];
                    s_bmax[totalFrames][2] = bmax[2];
                    totalFrames++;
                }
                p += numverts * sizeof( mdl_trivertx_t );
            }
        }
    }

    if ( totalFrames == 0 ) {
        ri.Log( SEV_WARN, "%s: %s produced no frames\n", __func__, modName );
        return qfalse;
    }

    /* ==================================================================
       Phase 6: Build synthetic md3 blob
       Layout:
         md3Header_t
         md3Frame_t[totalFrames]
         (numTags = 0, no tag data)
         md3Surface_t  [header only]
           md3Shader_t[numValidSkins]
           md3Triangle_t[numtris]
           md3St_t[expandedCount]
           md3XyzNormal_t[expandedCount * totalFrames]
       ================================================================== */
    {
        int   headerSz  = (int)sizeof( md3Header_t );
        int   framesSz  = (int)sizeof( md3Frame_t ) * totalFrames;
        int   surfHdrSz = (int)sizeof( md3Surface_t );
        int   shadersSz = (int)sizeof( md3Shader_t )    * numValidSkins;
        int   trisSz    = (int)sizeof( md3Triangle_t )  * numtris;
        int   stSz      = (int)sizeof( md3St_t )        * expandedCount;
        int   xyzSz     = (int)sizeof( md3XyzNormal_t ) * expandedCount * totalFrames;
        int   surfSz    = surfHdrSz + shadersSz + trisSz + stSz + xyzSz;
        int   totalSz   = headerSz + framesSz + surfSz;
        int   surfOfs   = headerSz + framesSz;

        byte           *blob;
        md3Header_t    *hdr;
        md3Frame_t     *mframe;
        md3Surface_t   *msurf;
        md3Shader_t    *mshader;
        md3Triangle_t  *mtri;
        md3St_t        *mst;
        md3XyzNormal_t *mxyz;

        blob = (byte *)ri.Malloc( totalSz );
        if ( !blob ) {
            ri.Log( SEV_WARN, "%s: out of memory for %s\n", __func__, modName );
            return qfalse;
        }
        memset( blob, 0, totalSz );

        /* -- Header -- */
        hdr = (md3Header_t *)blob;
        hdr->ident       = LittleLong( MD3_IDENT );
        hdr->version     = LittleLong( MD3_VERSION );
        Q_strncpyz( hdr->name, modName, sizeof( hdr->name ) );
        hdr->numFrames   = LittleLong( totalFrames );
        hdr->numTags     = LittleLong( 0 );
        hdr->numSurfaces = LittleLong( 1 );
        hdr->numSkins    = LittleLong( 0 );
        hdr->ofsFrames   = LittleLong( headerSz );
        hdr->ofsTags     = LittleLong( headerSz + framesSz );
        hdr->ofsSurfaces = LittleLong( surfOfs );
        hdr->ofsEnd      = LittleLong( totalSz );

        /* -- Frames -- */
        mframe = (md3Frame_t *)( blob + headerSz );
        for ( f = 0; f < totalFrames; f++, mframe++ ) {
            float cx, cy, cz, dx, dy, dz, r;
            mframe->bounds[0][0] = LittleFloat( s_bmin[f][0] );
            mframe->bounds[0][1] = LittleFloat( s_bmin[f][1] );
            mframe->bounds[0][2] = LittleFloat( s_bmin[f][2] );
            mframe->bounds[1][0] = LittleFloat( s_bmax[f][0] );
            mframe->bounds[1][1] = LittleFloat( s_bmax[f][1] );
            mframe->bounds[1][2] = LittleFloat( s_bmax[f][2] );
            cx = 0.5f * ( s_bmin[f][0] + s_bmax[f][0] );
            cy = 0.5f * ( s_bmin[f][1] + s_bmax[f][1] );
            cz = 0.5f * ( s_bmin[f][2] + s_bmax[f][2] );
            mframe->localOrigin[0] = LittleFloat( cx );
            mframe->localOrigin[1] = LittleFloat( cy );
            mframe->localOrigin[2] = LittleFloat( cz );
            dx = s_bmax[f][0] - cx;
            dy = s_bmax[f][1] - cy;
            dz = s_bmax[f][2] - cz;
            r  = (float)sqrt( (double)( dx*dx + dy*dy + dz*dz ) );
            mframe->radius = LittleFloat( r );
        }

        /* -- Surface header -- */
        msurf = (md3Surface_t *)( blob + surfOfs );
        /* ident: MD3 file format uses the same "IDP3" tag as the main header */
        msurf->ident        = LittleLong( MD3_IDENT );
        Q_strncpyz( msurf->name, "mdl_surface", sizeof( msurf->name ) );
        msurf->numFrames    = LittleLong( totalFrames );
        msurf->numShaders   = LittleLong( numValidSkins );
        msurf->numVerts     = LittleLong( expandedCount );
        msurf->numTriangles = LittleLong( numtris );
        msurf->ofsShaders    = LittleLong( surfHdrSz );
        msurf->ofsTriangles  = LittleLong( surfHdrSz + shadersSz );
        msurf->ofsSt         = LittleLong( surfHdrSz + shadersSz + trisSz );
        msurf->ofsXyzNormals = LittleLong( surfHdrSz + shadersSz + trisSz + stSz );
        msurf->ofsEnd        = LittleLong( surfSz );

        /* -- Shaders -- */
        mshader = (md3Shader_t *)( blob + surfOfs + surfHdrSz );
        for ( i = 0; i < numValidSkins; i++, mshader++ ) {
            Q_strncpyz( mshader->name, s_skinName[i], sizeof( mshader->name ) );
            mshader->shaderIndex = LittleLong( 0 );
        }

        /* -- Triangles -- */
        mtri = (md3Triangle_t *)( blob + surfOfs + surfHdrSz + shadersSz );
        for ( i = 0; i < numtris; i++, mtri++ ) {
            mtri->indexes[0] = LittleLong( (unsigned)s_remappedTris[i][0] );
            mtri->indexes[1] = LittleLong( (unsigned)s_remappedTris[i][1] );
            mtri->indexes[2] = LittleLong( (unsigned)s_remappedTris[i][2] );
        }

        /* -- ST coords -- */
        mst = (md3St_t *)( blob + surfOfs + surfHdrSz + shadersSz + trisSz );
        for ( i = 0; i < expandedCount; i++, mst++ ) {
            mst->st[0] = LittleFloat( s_st[i][0] );
            mst->st[1] = LittleFloat( s_st[i][1] );
        }

        /* -- XyzNormals: frame-major order [frame0_v0, frame0_v1, ..., frame1_v0, ...] -- */
        mxyz = (md3XyzNormal_t *)( blob + surfOfs + surfHdrSz + shadersSz + trisSz + stSz );
        for ( f = 0; f < totalFrames; f++ ) {
            for ( i = 0; i < expandedCount; i++, mxyz++ ) {
                float x = s_xyz[f][i][0];
                float y = s_xyz[f][i][1];
                float z = s_xyz[f][i][2];
                mxyz->xyz[0] = LittleShort( (short)( x / (float)MD3_XYZ_SCALE ) );
                mxyz->xyz[1] = LittleShort( (short)( y / (float)MD3_XYZ_SCALE ) );
                mxyz->xyz[2] = LittleShort( (short)( z / (float)MD3_XYZ_SCALE ) );
                MDL_EncodeNormal( s_nrm[f][i], &mxyz->normal );
                mxyz->normal = LittleShort( mxyz->normal );
            }
        }

        *outBuf  = blob;
        *outSize = totalSz;
    }

    return qtrue;

trunc:
    ri.Log( SEV_WARN, "%s: %s truncated MDL data\n", __func__, modName );
    return qfalse;
}
