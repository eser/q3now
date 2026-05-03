/*
r_q1_shaderbuild.inc  —  Q1 synthetic shader builder.

TEXTUALLY INCLUDED inside each renderer's tr_shader.c (not compiled standalone).
Relies on static globals from the enclosing translation unit:

  stages[]            shaderStage_t [MAX_SHADER_STAGES]
  shader              current working shader_t
  texMods[][]         texModInfo_t scratch space
  InitShader()        clears shader/stages, sets name+lightmapIndex
  FinishShader()      finalises and registers the shader

Include once near the bottom of tr_shader.c, BEFORE R_FindShader.

Usage inside R_FindShader (after the hash-table lookup, before InitShader):
  if ( R_Q1_IsActive() ) {
      shader_t *q1sh = R_Q1_BuildSyntheticShader( strippedName, lightmapIndex );
      if ( q1sh ) return q1sh;
  }
*/

#ifndef R_Q1_SHADERBUILD_INCLUDED
#define R_Q1_SHADERBUILD_INCLUDED

#include "../renderercommon/r_q1_texture.h"

/*
 * Cross-renderer field-access macros.
 *
 * renderervk (USE_VULKAN): rgbGen/alphaGen live on textureBundle_t.
 * renderer (GL1) and renderer2 (GL2): they live on shaderStage_t directly.
 *
 * renderer2 uses  bundle[0].isLightmap (qboolean) instead of
 * bundle[0].lightmap (int) + LIGHTMAP_INDEX_SHADER; detect by whether
 * LIGHTMAP_INDEX_SHADER is defined (it is in GL1 and renderervk, not in GL2).
 */
#ifdef USE_VULKAN
#  define Q1_ST_RGBGEN(st)           (st)->bundle[0].rgbGen
#  define Q1_ST_ALPHAGEN(st)         (st)->bundle[0].alphaGen
#else
#  define Q1_ST_RGBGEN(st)           (st)->rgbGen
#  define Q1_ST_ALPHAGEN(st)         (st)->alphaGen
#endif

/* Lightmap stage — mirrors R_CreateDefaultShading two-pass lightmap path. */
static void Q1_AddLightmapStage( int si, int lightmapIndex ) {
    shaderStage_t *st = &stages[si];
    st->active = qtrue;
    if ( tr.lightmaps && lightmapIndex >= 0 && lightmapIndex < tr.numLightmaps )
        st->bundle[0].image[0] = tr.lightmaps[lightmapIndex];
    else
        st->bundle[0].image[0] = tr.whiteImage;
    // Store the page index in Q3 convention so tr_shade.c q1ls gate recovers it correctly.
    // LIGHTMAP_INDEX_SHADER (flat sentinel) would give lmIdx = SHADER - OFFSET = -1.
#ifdef LIGHTMAP_INDEX_SHADER
    st->bundle[0].lightmap = ( lightmapIndex >= 0 ? lightmapIndex : 0 ) + LIGHTMAP_INDEX_OFFSET;
#else
    st->bundle[0].isLightmap = qtrue;
#endif
    st->bundle[0].tcGen    = TCGEN_LIGHTMAP;
    Q1_ST_RGBGEN( st )     = CGEN_IDENTITY;
    st->stateBits          = GLS_DEFAULT;
    // Tell q1ls gate in tr_shade.c to skip this stage — q1ls is a single-pass
    // shader that fires on the DIFFUSE stage only (bundle[1].lightmap encodes
    // the atlas index there; LIGHTMAP_INDEX_NONE here suppresses the check).
    // GL2 has no bundle[].lightmap field; guard with LIGHTMAP_INDEX_SHADER.
#ifdef LIGHTMAP_INDEX_SHADER
    st->bundle[1].lightmap = LIGHTMAP_INDEX_NONE;
#endif
}

/* Diffuse stage (second pass in the two-pass lightmap scheme). */
static void Q1_AddDiffuseStage( int si, struct image_s *img ) {
    shaderStage_t *st = &stages[si];
    st->active             = qtrue;
    st->bundle[0].image[0] = img;
    st->bundle[0].tcGen    = TCGEN_TEXTURE;
    Q1_ST_RGBGEN( st )     = CGEN_IDENTITY;
    st->stateBits          = GLS_SRCBLEND_DST_COLOR | GLS_DSTBLEND_ZERO;
}

/* Additive glow stage — no depth write, additive blend.
   GLS_DEPTHFUNC_EQUAL constrains glow to pixels at the exact same depth as the
   base surface, preventing fullbright pixels from bleeding over closer geometry. */
static void Q1_AddGlowStage( int si, struct image_s *img ) {
    shaderStage_t *st = &stages[si];
    st->active             = qtrue;
    st->bundle[0].image[0] = img;
    st->bundle[0].tcGen    = TCGEN_TEXTURE;
    Q1_ST_RGBGEN( st )     = CGEN_IDENTITY;
    st->stateBits          = GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE | GLS_DEPTHFUNC_EQUAL;
}

/* Scroll tcMod helper. */
static void Q1_AddScrollTcMod( int si, float s, float t ) {
    shaderStage_t *st   = &stages[si];
    texModInfo_t  *tm   = &texMods[si][ st->bundle[0].numTexMods ];
    tm->type            = TMOD_SCROLL;
    tm->scroll[0]       = s;
    tm->scroll[1]       = t;
    st->bundle[0].numTexMods++;
}

/* Animated stage.  forGlow selects the glow frame array. */
static void Q1_AddAnimStage( int si, struct image_s * const *diffFrames,
                              struct image_s * const *glowFrames, int n, qboolean forGlow )
{
    shaderStage_t *st = &stages[si];
    int k;
    st->active = qtrue;
    for ( k = 0; k < n && k < MAX_IMAGE_ANIMATIONS; k++ )
        st->bundle[0].image[k] = forGlow ? glowFrames[k] : diffFrames[k];
    st->bundle[0].numImageAnimations = n;
    st->bundle[0].imageAnimationSpeed = 10.0;
    st->bundle[0].tcGen  = TCGEN_TEXTURE;
    Q1_ST_RGBGEN( st )   = CGEN_IDENTITY;
    if ( forGlow )
        st->stateBits = GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE | GLS_DEPTHFUNC_EQUAL;
    else
        st->stateBits = GLS_SRCBLEND_DST_COLOR | GLS_DSTBLEND_ZERO;
}


/*
R_Q1_BuildSyntheticShader
=========================
Called from R_FindShader when R_Q1_IsActive() returns qtrue.
Returns a permanently registered shader_t * or NULL if the name is not
a known Q1 miptex.
*/
static shader_t *R_Q1_BuildSyntheticShader( const char *name, int lightmapIndex ) {
    const q1TexInfo_t *qi = R_Q1_GetTexForName( name );
    if ( !qi || !qi->diffuse ) return NULL;

    InitShader( name, lightmapIndex );

    switch ( qi->cls ) {

    /* ---- D.1: Standard wall; +0* alt-frame groups; +1..+9 secondary frames */
    case Q1TC_ANIM:
    case Q1TC_ANIM_FRAME:
    case Q1TC_WALL:
    default: {
        int ns = 0;
        int diffuse_si;
        Q1_AddLightmapStage( ns++, lightmapIndex );
        diffuse_si = ns;
        Q1_AddDiffuseStage( ns++, qi->diffuse );
        if ( qi->glow )
            Q1_AddGlowStage( ns++, qi->glow );
        // bundle[1].lightmap on the diffuse stage carries the atlas page index for
        // the q1ls single-pass pipeline in tr_shade.c (reads pStage->bundle[1].lightmap).
        // LIGHTMAP_WHITEIMAGE signals a full-bright prop surface — leave bundle[1].lightmap
        // at zero (LIGHTMAP_INDEX_NONE) so the Q1 LS gate never fires for this shader.
#ifdef LIGHTMAP_INDEX_SHADER
        if ( lightmapIndex != LIGHTMAP_WHITEIMAGE )
            stages[diffuse_si].bundle[1].lightmap = ( lightmapIndex >= 0 ? lightmapIndex : 0 ) + LIGHTMAP_INDEX_OFFSET;
#endif
        /* Renderer2 packs normalmap/specular into extra bundle slots on the
           diffuse stage rather than separate stages.  TB_NORMALMAP / TB_SPECULARMAP
           are only defined in renderer2; GL1 and renderervk skip this block. */
#ifdef TB_NORMALMAP
        if ( qi->normalmap )
            stages[diffuse_si].bundle[TB_NORMALMAP].image[0] = qi->normalmap;
# ifdef TB_SPECULARMAP
        if ( qi->specular )
            stages[diffuse_si].bundle[TB_SPECULARMAP].image[0] = qi->specular;
# endif
#endif
        break;
    }

    /* ---- D.3: Sky — dual-scroll, no lightmap -------------------------- */
    case Q1TC_SKY: {
        int ns = 0;
        /* Backdrop layer — no depth write; sky is always behind everything */
        {
            shaderStage_t *st = &stages[ns];
            st->active             = qtrue;
            st->bundle[0].image[0] = qi->skyBack ? qi->skyBack : qi->diffuse;
            st->bundle[0].tcGen    = TCGEN_TEXTURE;
            Q1_ST_RGBGEN( st )     = CGEN_IDENTITY;
            st->stateBits          = GLS_DEFAULT & ~GLS_DEPTHMASK_TRUE;
            Q1_AddScrollTcMod( ns, 0.02f, 0.01f );
            ns++;
        }
        /* Cloud layer — faster scroll, alpha-blended, no depth write */
        if ( qi->skyFront ) {
            shaderStage_t *st = &stages[ns];
            st->active             = qtrue;
            st->bundle[0].image[0] = qi->skyFront;
            st->bundle[0].tcGen    = TCGEN_TEXTURE;
            Q1_ST_RGBGEN( st )     = CGEN_IDENTITY;
            st->stateBits          = GLS_SRCBLEND_SRC_ALPHA |
                                     GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA;
            Q1_AddScrollTcMod( ns, 0.04f, 0.02f );
            ns++;
        }
        shader.isSky         = qtrue;
        shader.sort          = SS_ENVIRONMENT;
        shader.surfaceFlags |= SURF_SKY | SURF_NOLIGHTMAP;
        break;
    }

    /* ---- D.4: Alpha cutout ({ prefix) --------------------------------- */
    case Q1TC_ALPHA: {
        int ns = 0;
        Q1_AddLightmapStage( ns++, lightmapIndex );
        {
            shaderStage_t *st = &stages[ns];
            st->active             = qtrue;
            st->bundle[0].image[0] = qi->diffuse;
            st->bundle[0].tcGen    = TCGEN_TEXTURE;
            Q1_ST_RGBGEN( st )     = CGEN_IDENTITY;
            Q1_ST_ALPHAGEN( st )   = AGEN_IDENTITY;
            st->stateBits          = GLS_SRCBLEND_DST_COLOR | GLS_DSTBLEND_ZERO |
                                     GLS_ATEST_GT_0;
#ifdef LIGHTMAP_INDEX_SHADER
            if ( lightmapIndex != LIGHTMAP_WHITEIMAGE )
                st->bundle[1].lightmap = ( lightmapIndex >= 0 ? lightmapIndex : 0 ) + LIGHTMAP_INDEX_OFFSET;
#endif
            ns++;
        }
        if ( qi->glow )
            Q1_AddGlowStage( ns++, qi->glow );
        shader.sort = SS_SEE_THROUGH;
        break;
    }

    } /* switch */

    /* Populate GPU array animation fields from q1TexInfo (VK renderer only).
       Non-animated shaders get numAnimFrames=1, animArray=NULL. */
#ifdef USE_VULKAN
    shader.q1NumAnimFrames = ( qi->numAnimFrames > 0 ) ? qi->numAnimFrames : 1;
    shader.q1AnimArray     = qi->animArray;
#endif

    {
        shader_t *result = FinishShader();
        R_Q1_CacheShader( name, result );
        return result;
    }
}

#endif /* R_Q1_SHADERBUILD_INCLUDED */
