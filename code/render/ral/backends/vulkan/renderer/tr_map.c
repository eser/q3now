// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 1999-2005 Id Software, Inc.
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
// tr_map.c

#include "tr_local.h"
#include "../../../../frontend/r_log.h"  // rilog-channel-mechanism Turn B — renderer.assets
#include "../../../../../qcommon/maps/map_format_registry.h"
#include "../../../../frontend/r_q1_texture.h"

R_LOG_DECLARE_CHANNEL( rch_assets,    "renderer.assets"    );
R_LOG_DECLARE_CHANNEL( rch_assets_q1, "renderer.assets.q1" );

#ifdef USE_VULKAN
#include "vk.h"
#include "vk_ral_textures.h"
#endif

/* Callback registered with the shared Q1 texture module so it can call
   the renderervk-specific R_CreateImage (name, name2, pic, w, h, flags). */
static struct image_s *RVK_Q1_CreateImage( const char *name, byte *rgba,
                                            int w, int h, unsigned int flags )
{
    return R_CreateImage( name, NULL, rgba, w, h, (imgFlags_t)flags );
}

static struct image_s *RVK_Q1_LoadImage( const char *path, unsigned int q1flags ) {
    (void)q1flags;  /* renderervk has no imgType; Q1_IMGF_NORMAL is a no-op here */
    return R_FindImageFile( path, IMGFLAG_MIPMAP );
}

static struct image_s *RVK_Q1_CreateImageArray( const char *name,
                                                 byte **frames, int numFrames,
                                                 int w, int h, unsigned int q1flags )
{
    return R_CreateImageArray( name, frames, numFrames, w, h, (imgFlags_t)q1flags );
}

/*

Loads and prepares a map file for scene rendering.

A single entry point:

void RE_LoadWorldMap( const mapFile_t *bsp, int worldIndex );

*/

// Per-app world slots: each connected client app holds its own loaded world so
// two apps can render two different maps. s_worldData is the load scratch the
// loader fills in place (keeping the load code unchanged); on a successful load
// it is published into the owning app's slot and tr.world is pointed there. The
// render path then selects the active app's slot via RE_SetWorldSlot. Single app:
// only slot 0 is ever used, and tr.world points at it exactly as before.
static	world_t		s_worldData;
static	world_t		s_worldDataSlots[ MAX_RENDER_WORLDS ];
static	int			s_activeWorldIndex = -1;
typedef struct {
	const byte *externalVisData;
	int numLightmaps;
	image_t **lightmaps;
	image_t **lightmapsStyle[3];
	int numLightmapsStyle;
	image_t **sunMaskAtlas;
	image_t **propLightmaps;
	int numPropLightmaps;
	int maxPropLightmaps;
	float lightstyleValues[64];
	char lightstylePatterns[64][64];
	qboolean mergeLightmaps;
	float lightmapOffset[2];
	float lightmapScale[2];
	int lightmapMod;
	int visCount;
	int viewCluster;
	vec3_t sunLight;
	vec3_t sunDirection;
	qboolean sunHasSource;
	int numFogs;
	int globalFog;
	fogType_t globalFogType;
	vec3_t globalFogColor;
	float globalFogDepthForOpaque;
	float globalFogDensity;
	qboolean fogEnabled;
	fogType_t fogTypeCurrent;
	qboolean vertexLightingAllowed;
} worldMapState_t;
static	worldMapState_t s_worldMapStates[ MAX_RENDER_WORLDS ];
static	const byte	*s_pendingWorldVisData;
static	byte		*fileBase;
// Q1 per-vertex style indices passed to ParseFace during R_LoadSurfaces
static const byte	*lightstyleData = NULL;

static int	c_gridVerts;

static void R_SaveWorldMapState( int worldIndex ) {
	worldMapState_t *state;
	if ( worldIndex < 0 || worldIndex >= MAX_RENDER_WORLDS ) return;
	state = &s_worldMapStates[worldIndex];
	state->externalVisData = tr.externalVisData;
	state->numLightmaps = tr.numLightmaps;
	state->lightmaps = tr.lightmaps;
	memcpy( state->lightmapsStyle, tr.lightmapsStyle, sizeof( state->lightmapsStyle ) );
	state->numLightmapsStyle = tr.numLightmapsStyle;
	state->sunMaskAtlas = tr.sunMaskAtlas;
	state->propLightmaps = tr.propLightmaps;
	state->numPropLightmaps = tr.numPropLightmaps;
	state->maxPropLightmaps = tr.maxPropLightmaps;
	memcpy( state->lightstyleValues, tr.lightstyleValues, sizeof( state->lightstyleValues ) );
	memcpy( state->lightstylePatterns, tr.lightstylePatterns, sizeof( state->lightstylePatterns ) );
	state->mergeLightmaps = tr.mergeLightmaps;
	memcpy( state->lightmapOffset, tr.lightmapOffset, sizeof( state->lightmapOffset ) );
	memcpy( state->lightmapScale, tr.lightmapScale, sizeof( state->lightmapScale ) );
	state->lightmapMod = tr.lightmapMod;
	state->visCount = tr.visCount;
	state->viewCluster = tr.viewCluster;
	VectorCopy( tr.sunLight, state->sunLight );
	VectorCopy( tr.sunDirection, state->sunDirection );
	state->sunHasSource = tr.sunHasSource;
	state->numFogs = tr.numFogs;
	state->globalFog = tr.globalFog;
	state->globalFogType = tr.globalFogType;
	VectorCopy( tr.globalFogColor, state->globalFogColor );
	state->globalFogDepthForOpaque = tr.globalFogDepthForOpaque;
	state->globalFogDensity = tr.globalFogDensity;
	state->fogEnabled = tr.fogEnabled;
	state->fogTypeCurrent = tr.fogTypeCurrent;
	state->vertexLightingAllowed = tr.vertexLightingAllowed;
}

static void R_ApplyWorldMapStateValue( const worldMapState_t *state ) {
	tr.externalVisData = state->externalVisData;
	tr.numLightmaps = state->numLightmaps;
	tr.lightmaps = state->lightmaps;
	memcpy( tr.lightmapsStyle, state->lightmapsStyle, sizeof( tr.lightmapsStyle ) );
	tr.numLightmapsStyle = state->numLightmapsStyle;
	tr.sunMaskAtlas = state->sunMaskAtlas;
	tr.propLightmaps = state->propLightmaps;
	tr.numPropLightmaps = state->numPropLightmaps;
	tr.maxPropLightmaps = state->maxPropLightmaps;
	memcpy( tr.lightstyleValues, state->lightstyleValues, sizeof( tr.lightstyleValues ) );
	memcpy( tr.lightstylePatterns, state->lightstylePatterns, sizeof( tr.lightstylePatterns ) );
	tr.mergeLightmaps = state->mergeLightmaps;
	memcpy( tr.lightmapOffset, state->lightmapOffset, sizeof( tr.lightmapOffset ) );
	memcpy( tr.lightmapScale, state->lightmapScale, sizeof( tr.lightmapScale ) );
	tr.lightmapMod = state->lightmapMod;
	tr.visCount = state->visCount;
	tr.viewCluster = state->viewCluster;
	VectorCopy( state->sunLight, tr.sunLight );
	VectorCopy( state->sunDirection, tr.sunDirection );
	tr.sunHasSource = state->sunHasSource;
	tr.numFogs = state->numFogs;
	tr.globalFog = state->globalFog;
	tr.globalFogType = state->globalFogType;
	VectorCopy( state->globalFogColor, tr.globalFogColor );
	tr.globalFogDepthForOpaque = state->globalFogDepthForOpaque;
	tr.globalFogDensity = state->globalFogDensity;
	tr.fogEnabled = state->fogEnabled;
	tr.fogTypeCurrent = state->fogTypeCurrent;
	tr.vertexLightingAllowed = state->vertexLightingAllowed;
}

static void R_ApplyWorldMapState( int worldIndex ) {
	R_ApplyWorldMapStateValue( &s_worldMapStates[worldIndex] );
}

static void R_ClearWorldMapGlobals( const byte *externalVisData,
		qboolean vertexLightingAllowed ) {
	worldMapState_t empty;
	memset( &empty, 0, sizeof( empty ) );
	empty.externalVisData = externalVisData;
	empty.vertexLightingAllowed = vertexLightingAllowed;
	empty.viewCluster = -1;
	empty.lightmapScale[0] = empty.lightmapScale[1] = 1.0f;
	empty.lightmapMod = MAX_QINT;
	R_ApplyWorldMapStateValue( &empty );
}


//===============================================================================

static void HSVtoRGB( float h, float s, float v, float rgb[3] )
{
	float f;
	float p, q, t;

	h *= 5;

	int i = floor( h );
	f = h - i;

	p = v * ( 1 - s );
	q = v * ( 1 - s * f );
	t = v * ( 1 - s * ( 1 - f ) );

	switch ( i )
	{
	case 0:
		rgb[0] = v;
		rgb[1] = t;
		rgb[2] = p;
		break;
	case 1:
		rgb[0] = q;
		rgb[1] = v;
		rgb[2] = p;
		break;
	case 2:
		rgb[0] = p;
		rgb[1] = v;
		rgb[2] = t;
		break;
	case 3:
		rgb[0] = p;
		rgb[1] = q;
		rgb[2] = v;
		break;
	case 4:
		rgb[0] = t;
		rgb[1] = p;
		rgb[2] = v;
		break;
	case 5:
		rgb[0] = v;
		rgb[1] = p;
		rgb[2] = q;
		break;
	}
}


/*
===============
R_ClampDenorm

Clamp fp values that may result in denormalization after further multiplication
===============
*/
float R_ClampDenorm( float v ) {
	if ( fabsf( v ) > 0.0f && fabsf( v ) < 1e-9f ) {
		return 0.0f;
	}
	return v;
}


/*
===============
R_ColorShiftLightingBytes — decode Q3 BSP byte data.

`linearLightmap == qtrue` (the lightmap-texel call sites): copy the byte verbatim. The ×2
q3map2-overbright doubling is done in the world fragment shader as a
linear-domain `× LIGHTMAP_BOOST` (gen_frag.tmpl, modulate-default
branches, after sampleColorTex's sRGB → linear decode) — colorimetrically
correct, unlike the byte-space `<< 1` (byte 128 doubled to 255 is ~4×
linear light, not 2×).

`linearLightmap == qfalse` (the vertex-light-colour and light-grid
call sites): apply the byte-space `<< 1` overbright shift + preserve-hue
clamp on overflow. These streams have no shader-side compensation path
yet — retiring this `<< 1` requires a model-lighting boost migration
(reconciling RB_Calc*Color outputs with a new shader path), tracked
separately. Until then it
stays here.

r_mapBrightness was removed (Q3 lightmap intensity is a
format constant, not a runtime parameter).
r_lightmapSaturation now operates in linear domain for the lightmap-
texel path (linearLightmap == qtrue) — decode → mix → re-encode, like
R_LoadFogs. The vertex-colour / light-grid path (linearLightmap ==
qfalse) stays byte-space pending the model-lighting-boost
migration.
===============
*/
void R_ColorShiftLightingBytes( const byte in[4], byte out[4], qboolean hasAlpha, qboolean linearLightmap ) {
	int r, g, b;

	if ( linearLightmap ) {
		// Byte verbatim; the world shader applies LIGHTMAP_BOOST (× 2.0)
		// in linear domain post-sRGB-decode.
		r = in[0];
		g = in[1];
		b = in[2];
	} else {
		// Byte-space << 1 overbright boost — vertex colours / light grid
		// (no shader-side compensation path yet; see the header comment).
		const int shift = 1;
		r = in[0] << shift;
		g = in[1] << shift;
		b = in[2] << shift;
		// normalize by color instead of saturating to white
		if ( ( r | g | b ) > 255 ) {
			int max = r > g ? r : g;
			max = max > b ? max : b;
			r = r * 255 / max;
			g = g * 255 / max;
			b = b * 255 / max;
		}
	}

	if ( r_lightmapSaturation->value != 1.0f ) {
		const float sat = r_lightmapSaturation->value;

		if ( linearLightmap ) {
			// Block 7 (colour closure): lightmap texels are stored
			// sRGB-encoded — R_ProcessLightmap copies them byte-verbatim
			// (the ×2 q3map2 overbright moved to the world shader as a
			// linear-domain × LIGHTMAP_BOOST). Desaturating in byte
			// space biases the result toward a too-bright grey because
			// the sRGB curve compresses midtones — same error class
			// Block 4 fixed for fog. Decode → mix toward linear luma →
			// re-encode. Mirrors R_LoadFogs below and the in-shader
			// fogColor decode (tr_shade.c R_SRGBToLinear).
			float lr = R_SRGBToLinear( (float)r / 255.0f );
			float lg = R_SRGBToLinear( (float)g / 255.0f );
			float lb = R_SRGBToLinear( (float)b / 255.0f );
			const float luma = LUMA( lr, lg, lb );
			// saturation 0 → luma (grey); 1 → identity (excluded by the
			// branch above); 2 → super-saturation, clamped below.
			lr = luma + sat * ( lr - luma );
			lg = luma + sat * ( lg - luma );
			lb = luma + sat * ( lb - luma );
			// R_LinearToSRGB clamps negatives to 0; clamp the high end.
			int ri = (int)( R_LinearToSRGB( lr ) * 255.0f + 0.5f );
			int gi = (int)( R_LinearToSRGB( lg ) * 255.0f + 0.5f );
			int bi = (int)( R_LinearToSRGB( lb ) * 255.0f + 0.5f );
			if ( ri > 255 ) ri = 255;
			if ( gi > 255 ) gi = 255;
			if ( bi > 255 ) bi = 255;
			out[0] = (byte)ri;
			out[1] = (byte)gi;
			out[2] = (byte)bi;
		} else {
			// linearLightmap == qfalse: vertex colours / light grid.
			// These streams still carry the byte << 1 overbright above
			// and have no shader-side compensation path yet — the
			// model-lighting-boost migration reconciles them. Until then
			// byte-space saturation matches the byte-space overbright it
			// sits alongside, so it stays here.
			const float luma = LUMA( r, g, b );
			const float fr = luma + sat * ( (float)r - luma );
			const float fg = luma + sat * ( (float)g - luma );
			const float fb = luma + sat * ( (float)b - luma );
			int ri = (int)( fr + 0.5f );
			int gi = (int)( fg + 0.5f );
			int bi = (int)( fb + 0.5f );
			if ( ri < 0 )   ri = 0;
			if ( ri > 255 ) ri = 255;
			if ( gi < 0 )   gi = 0;
			if ( gi > 255 ) gi = 255;
			if ( bi < 0 )   bi = 0;
			if ( bi > 255 ) bi = 255;
			out[0] = (byte)ri;
			out[1] = (byte)gi;
			out[2] = (byte)bi;
		}
	} else {
		out[0] = r;
		out[1] = g;
		out[2] = b;
	}

	if ( hasAlpha ) {
		out[3] = in[3];
	}
}


#define LIGHTMAP_SIZE 128
#define LIGHTMAP_BORDER 2
#define LIGHTMAP_LEN (LIGHTMAP_SIZE + LIGHTMAP_BORDER*2)

static const int lightmapFlags = IMGFLAG_NOLIGHTSCALE | IMGFLAG_NO_COMPRESSION | IMGFLAG_LIGHTMAP | IMGFLAG_NOSCALE;

static int lightmapWidth;
static int lightmapHeight;
static int lightmapCountX;
static int lightmapCountY;

static void FillBorders( byte *img )
{
#define PIX(xx,yy,offs) img[((yy)*LIGHTMAP_LEN + (xx))*4+(offs)]
	int x0, y0;
	int x1, y1;
	int n, len, i;

	for ( n = LIGHTMAP_BORDER; n > 0; n-- )
	{
		x0 = n - 1; x1 = LIGHTMAP_LEN - n;
		y0 = n - 1; y1 = LIGHTMAP_LEN - n;
		len = LIGHTMAP_SIZE + (LIGHTMAP_BORDER*2 - n);
		for ( i = n; i < len; i++ )
		{
			PIX( i, y0, 0 ) = PIX( i, y0+1, 0 );
			PIX( i, y0, 1 ) = PIX( i, y0+1, 1 );
			PIX( i, y0, 2 ) = PIX( i, y0+1, 2 );
			PIX( i, y0, 3 ) = PIX( i, y0+1, 3 );

			PIX( x0, i, 0 ) = PIX( x0+1, i, 0 );
			PIX( x0, i, 1 ) = PIX( x0+1, i, 1 );
			PIX( x0, i, 2 ) = PIX( x0+1, i, 2 );
			PIX( x0, i, 3 ) = PIX( x0+1, i, 3 );

			PIX( i, y1, 0 ) = PIX( i, y1-1, 0 );
			PIX( i, y1, 1 ) = PIX( i, y1-1, 1 );
			PIX( i, y1, 2 ) = PIX( i, y1-1, 2 );
			PIX( i, y1, 3 ) = PIX( i, y1-1, 3 );

			PIX( x1, i, 0 ) = PIX( x1-1, i, 0 );
			PIX( x1, i, 1 ) = PIX( x1-1, i, 1 );
			PIX( x1, i, 2 ) = PIX( x1-1, i, 2 );
			PIX( x1, i, 3 ) = PIX( x1-1, i, 3 );
		}

		// interpolate corners
		PIX( x0, y0, 0 ) = (int)(PIX( x0, y0+1, 0 ) + PIX( x0+1, y0, 0 )) >> 1;
		PIX( x0, y0, 1 ) = (int)(PIX( x0, y0+1, 1 ) + PIX( x0+1, y0, 1 )) >> 1;
		PIX( x0, y0, 2 ) = (int)(PIX( x0, y0+1, 2 ) + PIX( x0+1, y0, 2 )) >> 1;
		PIX( x0, y0, 3 ) = (int)(PIX( x0, y0+1, 3 ) + PIX( x0+1, y0, 3 )) >> 1;

		PIX( x1, y0, 0 ) = (int)(PIX( x1-1, y0, 0 ) + PIX( x1, y0+1, 0 )) >> 1;
		PIX( x1, y0, 1 ) = (int)(PIX( x1-1, y0, 1 ) + PIX( x1, y0+1, 1 )) >> 1;
		PIX( x1, y0, 2 ) = (int)(PIX( x1-1, y0, 2 ) + PIX( x1, y0+1, 2 )) >> 1;
		PIX( x1, y0, 3 ) = (int)(PIX( x1-1, y0, 3 ) + PIX( x1, y0+1, 3 )) >> 1;

		PIX( x0, y1, 0 ) = (int)(PIX( x0, y1-1, 0 ) + PIX( x0+1, y1, 0 )) >> 1;
		PIX( x0, y1, 1 ) = (int)(PIX( x0, y1-1, 1 ) + PIX( x0+1, y1, 1 )) >> 1;
		PIX( x0, y1, 2 ) = (int)(PIX( x0, y1-1, 2 ) + PIX( x0+1, y1, 2 )) >> 1;
		PIX( x0, y1, 3 ) = (int)(PIX( x0, y1-1, 3 ) + PIX( x0+1, y1, 3 )) >> 1;

		PIX( x1, y1, 0 ) = (int)(PIX( x1, y1-1, 0 ) + PIX( x1-1, y1, 0 )) >> 1;
		PIX( x1, y1, 1 ) = (int)(PIX( x1, y1-1, 1 ) + PIX( x1-1, y1, 1 )) >> 1;
		PIX( x1, y1, 2 ) = (int)(PIX( x1, y1-1, 2 ) + PIX( x1-1, y1, 2 )) >> 1;
		PIX( x1, y1, 3 ) = (int)(PIX( x1, y1-1, 3 ) + PIX( x1-1, y1, 3 )) >> 1;
	}
}


/*
===============
R_ProcessLightmap

expand the 24 bit on-disk to 32 bit and return max.intensity
===============
*/
static float R_ProcessLightmap( byte *image, const byte *buf_p, float maxIntensity, qboolean isQ1Lightmap )
{
	int x, y;

	if ( 0 && r_lightmap->integer == 2 ) {
		int j;
		// color code by intensity as development tool	(FIXME: check range)
		for ( j = 0; j < LIGHTMAP_SIZE * LIGHTMAP_SIZE; j++ )
		{
			float r = buf_p[j*3+0];
			float g = buf_p[j*3+1];
			float b = buf_p[j*3+2];
			float intensity;
			float out[3] = {0.0, 0.0, 0.0};

			intensity = 0.33f * r + 0.685f * g + 0.063f * b;

			if ( intensity > 255 )
				intensity = 1.0f;
			else
				intensity /= 255.0f;

			if ( intensity > maxIntensity )
				maxIntensity = intensity;

			HSVtoRGB( intensity, 1.00, 0.50, out );

			image[j*4+0] = out[0] * 255;
			image[j*4+1] = out[1] * 255;
			image[j*4+2] = out[2] * 255;
			image[j*4+3] = 255;
		}
	} else {
		if ( tr.mergeLightmaps ) {
			for ( y = 0 ; y < LIGHTMAP_SIZE; y++ ) {
				for ( x = 0 ; x < LIGHTMAP_SIZE; x++ ) {
					byte *dst = &image[((y + LIGHTMAP_BORDER) * LIGHTMAP_LEN + x + LIGHTMAP_BORDER) * 4];
					if ( isQ1Lightmap ) {
						// Q1 lightmap bytes are full-range 0-255 with no overbright pre-compression.
						// Copy verbatim (RGB -> RGBA expand) without the left-shift amplification.
						dst[0] = buf_p[0];
						dst[1] = buf_p[1];
						dst[2] = buf_p[2];
					} else {
						R_ColorShiftLightingBytes( buf_p, dst, qfalse, qtrue );
					}
					dst[3] = 255;
					buf_p += 3;
				}
			}
			FillBorders( image );
		} else {
			// legacy path
			for ( y = 0 ; y < LIGHTMAP_SIZE; y++ ) {
				for ( x = 0 ; x < LIGHTMAP_SIZE; x++ ) {
					byte *dst = &image[(y * LIGHTMAP_SIZE + x) * 4];
					if ( isQ1Lightmap ) {
						// Q1 lightmap bytes are full-range 0-255 with no overbright pre-compression.
						// Copy verbatim (RGB -> RGBA expand) without the left-shift amplification.
						dst[0] = buf_p[0];
						dst[1] = buf_p[1];
						dst[2] = buf_p[2];
					} else {
						R_ColorShiftLightingBytes( buf_p, dst, qfalse, qtrue );
					}
					dst[3] = 255;
					buf_p += 3;
				}
			}
		}
	}

	return maxIntensity;
}


static int SetLightmapParams( int numLightmaps, int maxTextureSize )
{
	lightmapWidth = log2pad( LIGHTMAP_LEN, 1 );
	lightmapHeight = log2pad( LIGHTMAP_LEN, 1 );

	lightmapCountX = 1;
	lightmapCountY = 1;

	while ( lightmapWidth < maxTextureSize && lightmapCountX * lightmapCountY < numLightmaps )
	{
		lightmapWidth = log2pad( lightmapWidth + LIGHTMAP_LEN, 1 );
		lightmapCountX = lightmapWidth / LIGHTMAP_LEN;
		if ( lightmapCountX * lightmapCountY >= numLightmaps )
			break;
		lightmapHeight = log2pad( lightmapHeight + LIGHTMAP_LEN, 1 );
		lightmapCountY = lightmapHeight / LIGHTMAP_LEN;
	}

	tr.lightmapMod = lightmapCountX * lightmapCountY;

	tr.lightmapScale[0] = (double)LIGHTMAP_SIZE / (double) lightmapWidth;
	tr.lightmapScale[1] = (double)LIGHTMAP_SIZE / (double) lightmapHeight;

	numLightmaps = ( numLightmaps + tr.lightmapMod - 1 ) / tr.lightmapMod;

	return numLightmaps;
}


int R_GetLightmapCoords( const int lightmapIndex, float *x, float *y )
{
	const int lightmapNum = lightmapIndex / tr.lightmapMod;
	const int cN = lightmapIndex % tr.lightmapMod;
	const int cX = cN % lightmapCountX;
	const int cY = cN / lightmapCountX;

	*x = (float)( LIGHTMAP_BORDER + cX * LIGHTMAP_LEN ) / (float) lightmapWidth;
	*y = (float)( LIGHTMAP_BORDER + cY * LIGHTMAP_LEN ) / (float) lightmapHeight;

	return lightmapNum;
}


/*
===============
R_LoadMergedLightmaps
===============
*/
static void R_LoadMergedLightmaps( const mapFile_t *bsp, byte *image, qboolean isQ1Lightmap )
{
	const byte	*buf;
	int			offs;
	int			i, x, y;
 	float		maxIntensity = 0;
	// Neutral lightmap pages (raw RGB bytes); byte length = pages * pageSize.
	int			lmBytes = bsp->numLightmapPages * bsp->lightmapPageSize;

	if ( lmBytes < LIGHTMAP_SIZE * LIGHTMAP_SIZE * 3 )
		return;

	buf = bsp->lightmapData;

	// create all the lightmaps
	tr.numLightmaps = lmBytes / (LIGHTMAP_SIZE * LIGHTMAP_SIZE * 3);

	tr.numLightmaps = SetLightmapParams( tr.numLightmaps, glConfig.maxTextureSize );

	tr.lightmaps = ri.Hunk_Alloc( tr.numLightmaps * sizeof(image_t *), h_low );

	// sun-mask atlas paralleling lightmaps[]: one page per
	// lightmap page at identical dimensions, so a merged-lightmap UV indexes
	// the matching mask texel. Empty here; the load-time ray-cast pass fills
	// .r. RGBA8 (R_CreateImage has no R8 path) — only .r is meaningful.
	tr.sunMaskAtlas = ri.Hunk_Alloc( tr.numLightmaps * sizeof(image_t *), h_low );

	for ( offs = 0, i = 0 ; i < tr.numLightmaps; i++ ) {
		byte *atlasData = ri.Hunk_AllocateTempMemory(
			lightmapWidth * lightmapHeight * 4 );
		memset( atlasData, 0, lightmapWidth * lightmapHeight * 4 );

		for ( y = 0; y < lightmapCountY; y++ ) {
			if ( offs >= lmBytes )
				break;

			for ( x = 0; x < lightmapCountX; x++ ) {
				if ( offs >= lmBytes )
					break;

				R_ProcessLightmap( image, buf + offs, maxIntensity, isQ1Lightmap );
				{
					int row;
					for ( row = 0; row < LIGHTMAP_LEN; ++row ) {
						byte *dst = atlasData
							+ ( ( y * LIGHTMAP_LEN + row ) * lightmapWidth
								+ x * LIGHTMAP_LEN ) * 4;
						memcpy( dst, image + row * LIGHTMAP_LEN * 4,
							LIGHTMAP_LEN * 4 );
					}
				}

				offs += LIGHTMAP_SIZE * LIGHTMAP_SIZE * 3;
			}
		}
		tr.lightmaps[ i ] = R_CreateImage( va( "*mergedLightmap%d", i ), NULL,
			atlasData, lightmapWidth, lightmapHeight,
			lightmapFlags | IMGFLAG_CLAMPTOBORDER | IMGFLAG_STREAM_ASSET_CHUNK );
		ri.Hunk_FreeTempMemory( atlasData );

		// NOSCALE → 1:1 with the lightmap atlas; DOMAIN_LINEAR → mask byte raw.
		tr.sunMaskAtlas[ i ] = R_CreateImage( va( "*mergedSunMask%d", i ), NULL, NULL,
			lightmapWidth, lightmapHeight,
			IMGFLAG_NOSCALE | IMGFLAG_NO_COMPRESSION | IMGFLAG_CLAMPTOBORDER | IMGFLAG_DOMAIN_LINEAR );
#ifdef USE_VULKAN
		//
#else
		R_LOG( rch_assets, SEV_DEBUG, "lightmaps[%i]=%i\n", i, tr.lightmaps[i]->texnum );
#endif
	}

	//if ( r_lightmap->integer == 2 )	{
	//	R_LOG( rch_assets, SEV_INFO, "Brightest lightmap value: %d\n", ( int ) ( maxIntensity * 255 ) );
	//}
}


// ===========================================================================
// load-time sun-visibility mask bake
//
// Each merged-lightmap atlas page has a parallel sun-mask page.
// Here every world-surface lightmap texel is resolved to a world position +
// normal and ray-cast toward tr.sunDirection: the byte written is
// round(max(N·sun,0)*255) when the sun is visible, else 0. gen_frag samples
// this (bindless role 7) to modulate the baked lightmap by the runtime CSM
// sun term. ZERO content-pipeline changes — a drop-in 1999 pk3 bakes its mask
// here at load.
//
//   F1: a ray that hits a SURF_SKY brush still sees the sun (an outdoor sky
//       is closed by SURF_SKY brushes — without this every outdoor texel
//       would read occluded).
//   F2: ray-casts per page are bounded to LIGHTMAP_SUNMASK_TEXEL_BUDGET by a
//       power-of-two stride; non-sampled ("gap") texels are bilinear-
//       interpolated. Deterministic — depends only on page dimensions.
//   R6: no-op unless r_shadows=1 AND a shader moved tr.sunDirection off
//       the engine default (HAS_SUN_SOURCE) — otherwise the sun-mask atlas
//       is left zero-filled and gen_frag's mix() returns 1.0 (graceful decline).
// ===========================================================================
#if FEAT_SHADOW_MAPPING
#define LIGHTMAP_SUNMASK_TEXEL_BUDGET	( 256 * 1024 )

typedef struct {
	int		W, H;			// atlas page dimensions
	int		stride;			// F2 power-of-two subsample stride
	int		sampW, sampH;	// sampled-grid dimensions ( ceil(dim/stride) )
	vec3_t	*pos;			// [sampW*sampH] interpolated world position
	vec3_t	*nrm;			// [sampW*sampH] interpolated surface normal
	byte	*cov;			// [sampW*sampH] 1 == a surface covers this texel
} sunMaskPage_t;

// Rasterize one triangle (in atlas-UV space) into the page's sampled buffers,
// barycentric-interpolating world position + normal. Only texel centres at
// sampled positions (multiples of stride) are written; gap texels are filled
// later by interpolation. Winding-independent (invDen carries the sign).
static void R_SunMaskRasterTri( sunMaskPage_t *pg,
		const float *p0, const float *p1, const float *p2,
		const float *n0, const float *n1, const float *n2,
		const float *uv0, const float *uv1, const float *uv2 ) {
	const float ax = uv0[0] * pg->W, ay = uv0[1] * pg->H;
	const float bx = uv1[0] * pg->W, by = uv1[1] * pg->H;
	const float cx = uv2[0] * pg->W, cy = uv2[1] * pg->H;
	const float denom = ( by - cy ) * ( ax - cx ) + ( cx - bx ) * ( ay - cy );
	float invDen, lo, hi;
	int   minSX, maxSX, minSY, maxSY, sx, sy;

	if ( fabsf( denom ) < 1e-9f )
		return;							// degenerate in atlas space
	invDen = 1.0f / denom;

	lo = ax; if ( bx < lo ) lo = bx; if ( cx < lo ) lo = cx;
	hi = ax; if ( bx > hi ) hi = bx; if ( cx > hi ) hi = cx;
	minSX = (int)floorf( lo / pg->stride ); if ( minSX < 0 ) minSX = 0;
	maxSX = (int)ceilf ( hi / pg->stride ); if ( maxSX > pg->sampW - 1 ) maxSX = pg->sampW - 1;
	lo = ay; if ( by < lo ) lo = by; if ( cy < lo ) lo = cy;
	hi = ay; if ( by > hi ) hi = by; if ( cy > hi ) hi = cy;
	minSY = (int)floorf( lo / pg->stride ); if ( minSY < 0 ) minSY = 0;
	maxSY = (int)ceilf ( hi / pg->stride ); if ( maxSY > pg->sampH - 1 ) maxSY = pg->sampH - 1;

	for ( sy = minSY; sy <= maxSY; sy++ ) {
		const float py = (float)( sy * pg->stride ) + 0.5f;
		for ( sx = minSX; sx <= maxSX; sx++ ) {
			const float px = (float)( sx * pg->stride ) + 0.5f;
			const float w0 = ( ( by - cy ) * ( px - cx ) + ( cx - bx ) * ( py - cy ) ) * invDen;
			const float w1 = ( ( cy - ay ) * ( px - cx ) + ( ax - cx ) * ( py - cy ) ) * invDen;
			const float w2 = 1.0f - w0 - w1;

			if ( w0 < -1e-4f || w1 < -1e-4f || w2 < -1e-4f )
				continue;				// texel centre outside the triangle
			{
				const int idx = sy * pg->sampW + sx;
				pg->pos[idx][0] = w0 * p0[0] + w1 * p1[0] + w2 * p2[0];
				pg->pos[idx][1] = w0 * p0[1] + w1 * p1[1] + w2 * p2[1];
				pg->pos[idx][2] = w0 * p0[2] + w1 * p1[2] + w2 * p2[2];
				pg->nrm[idx][0] = w0 * n0[0] + w1 * n1[0] + w2 * n2[0];
				pg->nrm[idx][1] = w0 * n0[1] + w1 * n1[1] + w2 * n2[1];
				pg->nrm[idx][2] = w0 * n0[2] + w1 * n1[2] + w2 * n2[2];
				pg->cov[idx] = 1;
			}
		}
	}
}

/*
===============
R_BuildSunMaskAtlas

See the block comment above. Runs once per map load
from RE_LoadWorldMap, after tr.world is valid.
===============
*/
static void R_BuildSunMaskAtlas( void ) {
	const world_t		*w = tr.world;
	const msurface_t	*worldSurfs;
	int					worldNumSurfs;
	vec3_t				sunDir;
	int					page, totalMs = 0, totalSampled = 0;

	if ( !tr.mergeLightmaps || tr.sunMaskAtlas == NULL || tr.numLightmaps <= 0 )
		return;
	if ( w == NULL || w->surfaces == NULL || w->numsurfaces <= 0 )
		return;
	if ( ri.Cvar_VariableIntegerValue( "r_shadows" ) != 1 )
		return;							// shadows off — sun-mask atlas left zero-filled

	// A real sun must exist (a shader's sun/q3map_sun keyword moved tr.sunDirection off
	// the engine default). tr.sunHasSource is the single-sourced truth, computed once at
	// world load in RE_LoadWorldMap.
	if ( !tr.sunHasSource ) {
		R_LOG( rch_assets, SEV_INFO,
			"sunmask: no sun source (HAS_SUN_SOURCE=false) — bake skipped, "
			"graceful decline\n" );
		return;
	}

	VectorCopy( tr.sunDirection, sunDir );
	VectorNormalize( sunDir );

	// worldspawn (model 0) surfaces only — bmodels move, so a load-time mask
	// would be stale (mirrors vk_build_shadow_caster's range selection).
	if ( w->bmodels && w->numBModels > 0 ) {
		worldSurfs    = w->bmodels[0].firstSurface;
		worldNumSurfs = w->bmodels[0].numSurfaces;
	} else {
		worldSurfs    = w->surfaces;
		worldNumSurfs = w->numsurfaces;
	}

	for ( page = 0; page < tr.numLightmaps; page++ ) {
		image_t			*atlas = tr.sunMaskAtlas[page];
		sunMaskPage_t	pg;
		byte			*mask, *rgba;
		int				total, x, y, s, startMs;
		int				coveredCnt = 0, rayCnt = 0, clearCnt = 0;

		if ( atlas == NULL || atlas->width <= 0 || atlas->height <= 0 )
			continue;
		pg.W = atlas->width;
		pg.H = atlas->height;
		if ( (int64_t)pg.W * (int64_t)pg.H > 16 * 1024 * 1024 ) {
			R_LOG( rch_assets, SEV_WARN,
				"sunmask: page %d %dx%d over 16M-texel cap — skipped\n",
				page, pg.W, pg.H );
			continue;
		}
		total = pg.W * pg.H;

		// F2(a) — power-of-two stride bounding ray-casts to the texel budget
		pg.stride = 1;
		while ( ( total / ( pg.stride * pg.stride ) ) > LIGHTMAP_SUNMASK_TEXEL_BUDGET )
			pg.stride *= 2;
		pg.sampW = ( pg.W + pg.stride - 1 ) / pg.stride;
		pg.sampH = ( pg.H + pg.stride - 1 ) / pg.stride;

		startMs = ri.Milliseconds();
		pg.pos  = ri.Hunk_AllocateTempMemory( pg.sampW * pg.sampH * sizeof( vec3_t ) );
		pg.nrm  = ri.Hunk_AllocateTempMemory( pg.sampW * pg.sampH * sizeof( vec3_t ) );
		pg.cov  = ri.Hunk_AllocateTempMemory( pg.sampW * pg.sampH );
		mask    = ri.Hunk_AllocateTempMemory( pg.sampW * pg.sampH );
		rgba    = ri.Hunk_AllocateTempMemory( total * 4 );
		memset( pg.cov, 0, pg.sampW * pg.sampH );

		// --- rasterize this page's world surfaces into the sampled buffers
		for ( s = 0; s < worldNumSurfs; s++ ) {
			const msurface_t	*surf = &worldSurfs[s];
			const surfaceType_t	*type;

			if ( surf->shader == NULL || surf->shader->lightmapIndex != page )
				continue;				// not lightmapped onto this page
			type = surf->data;
			if ( type == NULL )
				continue;
			if ( *type == SF_FACE ) {
				const srfSurfaceFace_t *f = (const srfSurfaceFace_t *)type;
				const int *idx = (const int *)( (const byte *)f + f->ofsIndices );
				int t;
				for ( t = 0; t + 2 < f->numIndices; t += 3 ) {
					const float *a = f->points[ idx[t  ] ];
					const float *b = f->points[ idx[t+1] ];
					const float *c = f->points[ idx[t+2] ];
					// SF_FACE is planar — every texel shares the face normal.
					R_SunMaskRasterTri( &pg, a, b, c,
						f->plane.normal, f->plane.normal, f->plane.normal,
						&a[5], &b[5], &c[5] );
				}
			} else if ( *type == SF_GRID ) {
				const srfGridMesh_t *g = (const srfGridMesh_t *)type;
				int r, c;
				for ( r = 0; r + 1 < g->height; r++ ) {
					for ( c = 0; c + 1 < g->width; c++ ) {
						const drawVert_t *v00 = &g->verts[ r       * g->width + c     ];
						const drawVert_t *v10 = &g->verts[ r       * g->width + c + 1 ];
						const drawVert_t *v01 = &g->verts[ ( r+1 ) * g->width + c     ];
						const drawVert_t *v11 = &g->verts[ ( r+1 ) * g->width + c + 1 ];
						R_SunMaskRasterTri( &pg, v00->xyz, v10->xyz, v11->xyz,
							v00->normal, v10->normal, v11->normal,
							v00->lightmap, v10->lightmap, v11->lightmap );
						R_SunMaskRasterTri( &pg, v00->xyz, v11->xyz, v01->xyz,
							v00->normal, v11->normal, v01->normal,
							v00->lightmap, v11->lightmap, v01->lightmap );
					}
				}
			}
		}

		// --- ray-cast every covered sampled texel toward the sun
		for ( y = 0; y < pg.sampH; y++ ) {
			for ( x = 0; x < pg.sampW; x++ ) {
				const int	idx = y * pg.sampW + x;
				vec3_t		n, start, end;
				trace_t		trace;
				float		ndl, len;

				mask[idx] = 0;
				if ( !pg.cov[idx] )
					continue;
				coveredCnt++;

				VectorCopy( pg.nrm[idx], n );
				len = VectorNormalize( n );
				if ( len < 1e-6f )
					continue;			// degenerate normal
				ndl = DotProduct( n, sunDir );
				if ( ndl <= 0.0f )
					continue;			// back-facing the sun — no direct term

				rayCnt++;
				VectorMA( pg.pos[idx], 2.0f, n, start );
				VectorMA( pg.pos[idx], 131072.0f, sunDir, end );
				ri.CM_BoxTrace( &trace, start, end, vec3_origin, vec3_origin,
					0, CONTENTS_SOLID, qfalse );
				// F1 — a SURF_SKY hit still means the texel sees the sun.
				if ( trace.fraction >= 1.0f || ( trace.surfaceFlags & SURF_SKY ) ) {
					int v = (int)( ndl * 255.0f + 0.5f );
					mask[idx] = (byte)( v > 255 ? 255 : v );
					clearCnt++;
				}
			}
		}

		// --- F2(b) — expand the sampled mask to the full page (bilinear)
		memset( rgba, 0, total * 4 );
		for ( y = 0; y < pg.H; y++ ) {
			const int	sy0 = y / pg.stride;
			const int	sy1 = ( sy0 + 1 < pg.sampH ) ? sy0 + 1 : sy0;
			const float	fy  = (float)( y - sy0 * pg.stride ) / (float)pg.stride;
			for ( x = 0; x < pg.W; x++ ) {
				const int	sx0 = x / pg.stride;
				const int	sx1 = ( sx0 + 1 < pg.sampW ) ? sx0 + 1 : sx0;
				const float	fx  = (float)( x - sx0 * pg.stride ) / (float)pg.stride;
				const float	m0  = (float)mask[ sy0 * pg.sampW + sx0 ] * ( 1.0f - fx )
				                + (float)mask[ sy0 * pg.sampW + sx1 ] * fx;
				const float	m1  = (float)mask[ sy1 * pg.sampW + sx0 ] * ( 1.0f - fx )
				                + (float)mask[ sy1 * pg.sampW + sx1 ] * fx;
				rgba[ ( y * pg.W + x ) * 4 ] = (byte)( m0 * ( 1.0f - fy ) + m1 * fy + 0.5f );
			}
		}

		vk_upload_image_data( atlas, 0, 0, pg.W, pg.H, 1, rgba, total * 4, qtrue, 0 );

		ri.Hunk_FreeTempMemory( rgba );
		ri.Hunk_FreeTempMemory( mask );
		ri.Hunk_FreeTempMemory( pg.cov );
		ri.Hunk_FreeTempMemory( pg.nrm );
		ri.Hunk_FreeTempMemory( pg.pos );

		{
			const int ms = ri.Milliseconds() - startMs;
			totalMs      += ms;
			totalSampled += pg.sampW * pg.sampH;
			R_LOG( rch_assets, SEV_INFO,
				"sunmask: page %d: %dx%d total=%d stride=%d sampled=%d "
				"covered=%d raycast=%d clear=%d compute=%dms\n",
				page, pg.W, pg.H, total, pg.stride, pg.sampW * pg.sampH,
				coveredCnt, rayCnt, clearCnt, ms );
		}
	}

	R_LOG( rch_assets, SEV_INFO,
		"sunmask: %d page(s) baked: %d sampled texel(s), %d ms total\n",
		tr.numLightmaps, totalSampled, totalMs );
}
#endif // FEAT_SHADOW_MAPPING


/*
===============
R_LoadLightmaps
===============
*/
static void R_LoadLightmaps( const mapFile_t *bsp, qboolean isQ1Lightmap ) {
	const byte	*buf;
	byte		image[LIGHTMAP_LEN*LIGHTMAP_LEN*4];
	int			i, numLightmaps;
	float		maxIntensity = 0;
	// Lightmap pages come from the parsed neutral map (raw RGB bytes, no
	// byte-swap). The lump byte length is numLightmapPages * lightmapPageSize.
	int			lmBytes = bsp->numLightmapPages * bsp->lightmapPageSize;

	tr.numLightmaps = 0;
	tr.mergeLightmaps = qfalse;
	// Prop lightmap image_t* pointers are stale after hunk clear on level change.
	// Reset the count here so R_UploadPropLightmaps starts fresh for the new map.
	tr.numPropLightmaps = 0;
	tr.lightmapScale[0] = 1.0f;
	tr.lightmapScale[1] = 1.0f;
	tr.lightmapOffset[0] = 0.0f;
	tr.lightmapOffset[1] = 0.0f;
	tr.lightmapMod = MAX_QINT;
	lightmapWidth = LIGHTMAP_SIZE;
	lightmapHeight = LIGHTMAP_SIZE;
	lightmapCountX = 1;
	lightmapCountY = 1;

	if ( lmBytes < LIGHTMAP_SIZE * LIGHTMAP_SIZE * 3 ) {
		return;
	}

	// if we are in r_vertexLight mode, we don't need the lightmaps at all
	if ( r_vertexLight->integer || glConfig.hardwareType == GLHW_PERMEDIA2 ) {
		return;
	}

	numLightmaps = lmBytes / (LIGHTMAP_SIZE * LIGHTMAP_SIZE * 3);

	/* Q1 maps must not use the merged mega-texture path: Q1 surface UVs are
	 * pre-baked at parse time as (lm_u / LM_PAGE_W), assuming per-page
	 * 128×128 textures. The merged path adds tile + border offsets that Q1
	 * UVs do not encode (R_GetLightmapCoords adjusts Q3 surfaces only).
	 * Force Q1 to the legacy per-page path. */
	if ( !isQ1Lightmap && r_mergeLightmaps->integer && r_lightmapAtlas->integer && numLightmaps > 1 ) {
		// check for low texture sizes
		if ( glConfig.maxTextureSize >= LIGHTMAP_LEN * 2 ) {
			tr.mergeLightmaps = qtrue;
			R_LoadMergedLightmaps( bsp, image, isQ1Lightmap ); // reuse stack space
			return;
		}
	}

	buf = bsp->lightmapData;

	// create all the lightmaps
	tr.numLightmaps = numLightmaps;

	tr.lightmaps = ri.Hunk_Alloc( tr.numLightmaps * sizeof(image_t *), h_low );

	for ( i = 0 ; i < tr.numLightmaps ; i++ ) {
		maxIntensity = R_ProcessLightmap( image, buf + i * LIGHTMAP_SIZE * LIGHTMAP_SIZE * 3, maxIntensity, isQ1Lightmap );
		tr.lightmaps[i] = R_CreateImage( va( "*lightmap%d", i ), NULL, image, LIGHTMAP_SIZE, LIGHTMAP_SIZE,
			lightmapFlags | IMGFLAG_CLAMPTOEDGE | IMGFLAG_STREAM_ASSET_CHUNK );
	}

	//if ( r_lightmap->integer == 2 )	{
	//	R_LOG( rch_assets, SEV_INFO, "Brightest lightmap value: %d\n", ( int ) ( maxIntensity * 255 ) );
	//}
}


/*
=================
R_LoadQ1StyledLightmaps

Upload Q1 per-slot lightmaps for style slots 1/2/3 from mapFile_t.
Called after R_LoadLightmaps so tr.lightmaps[] (slot 0) is already populated.
Only runs if bsp->styledLightmapData[1] is set (Q1 maps with multi-style lighting).
=================
*/
static void R_LoadQ1StyledLightmaps( const mapFile_t *bsp ) {
	byte	image[LIGHTMAP_LEN*LIGHTMAP_LEN*4];
	float	maxIntensity = 0;
	int		k, i, n;

	if ( !bsp || !bsp->styledLightmapData[1] ) {
		return;
	}

	if ( tr.mergeLightmaps ) {
		R_LOG( rch_assets, SEV_DEBUG, "R_LoadQ1StyledLightmaps: skipped (merged lightmaps not supported for Q1 style blend)\n" );
		return;
	}

	n = bsp->numLightmapPages;
	tr.numLightmapsStyle = n;

	for ( k = 1; k <= 3; k++ ) {
		if ( !bsp->styledLightmapData[k] || !bsp->numStyledLightmapPages[k] ) {
			tr.lightmapsStyle[k-1] = NULL;
			continue;
		}

		tr.lightmapsStyle[k-1] = ri.Hunk_Alloc( n * sizeof(image_t *), h_low );

		for ( i = 0; i < n; i++ ) {
			const byte *src = bsp->styledLightmapData[k] + (size_t)i * bsp->lightmapPageSize;
			maxIntensity = R_ProcessLightmap( image, src, maxIntensity, qtrue );
			tr.lightmapsStyle[k-1][i] = R_CreateImage( va( "*q1lm%d_%d", k, i ), NULL, image,
				LIGHTMAP_SIZE, LIGHTMAP_SIZE,
				lightmapFlags | IMGFLAG_CLAMPTOEDGE | IMGFLAG_STREAM_ASSET_CHUNK );
		}
	}

	R_LOG( rch_assets, SEV_DEBUG, "R_LoadQ1StyledLightmaps: uploaded %d pages × 3 style slots\n", n );
}


/*
=================
R_UploadPropLightmaps

Upload lightmap pages from a prop BSP (e.g. maps/b_explob.bsp) into
tr.propLightmaps[], a ri.Malloc growable array separate from the world
lightmaps in tr.lightmaps[].

Returns the base index into tr.propLightmaps[] for this prop's pages
(add LIGHTMAP_PROP_OFFSET to form the per-surface lightmapNum).
Returns -1 if the prop has no lightmap data; caller falls back to LIGHTMAP_WHITEIMAGE.
=================
*/
static int R_UploadPropLightmaps( const mapFile_t *bsp ) {
	byte  image[LIGHTMAP_LEN * LIGHTMAP_LEN * 4];
	int   firstIdx, i, newMax;
	image_t **newArr;

	if ( !bsp->lightmapData || bsp->numLightmapPages <= 0 )
		return -1;

	firstIdx = tr.numPropLightmaps;

	if ( firstIdx + bsp->numLightmapPages > tr.maxPropLightmaps ) {
		newMax = tr.maxPropLightmaps * 2;
		if ( newMax < firstIdx + bsp->numLightmapPages )
			newMax = firstIdx + bsp->numLightmapPages;
		if ( newMax < 64 )
			newMax = 64;
		newArr = ri.Malloc( newMax * sizeof( image_t * ) );
		if ( tr.propLightmaps ) {
			if ( tr.numPropLightmaps > 0 )
				memcpy( newArr, tr.propLightmaps, tr.numPropLightmaps * sizeof( image_t * ) );
			ri.Free( tr.propLightmaps );
		}
		tr.propLightmaps    = newArr;
		tr.maxPropLightmaps = newMax;
	}

	for ( i = 0; i < bsp->numLightmapPages; i++ ) {
		const byte *src = bsp->lightmapData + (size_t)i * bsp->lightmapPageSize;
		R_ProcessLightmap( image, src, 0.0f, qtrue );
		tr.propLightmaps[ firstIdx + i ] = R_CreateImage(
			va( "*propLightmap%d", firstIdx + i ), NULL, image,
			LIGHTMAP_SIZE, LIGHTMAP_SIZE,
			lightmapFlags | IMGFLAG_CLAMPTOEDGE | IMGFLAG_STREAM_ASSET_CHUNK );
	}
	tr.numPropLightmaps += bsp->numLightmapPages;

	R_LOG( rch_assets, SEV_DEBUG, "R_UploadPropLightmaps: uploaded %d pages (base %d)\n",
		bsp->numLightmapPages, firstIdx );
	return firstIdx;
}


/*
=================
RE_SetWorldVisData

This is called by the clipmodel subsystem so we can share the 1.8 megs of
space in big maps...
=================
*/
void RE_SetWorldVisData( const byte *vis ) {
	/* Visibility ownership is associated with the next world registration.  Do
	 * not overwrite the currently selected sibling world's visibility alias. */
	s_pendingWorldVisData = vis;
	if ( s_activeWorldIndex < 0 ) tr.externalVisData = vis;
}


/*
=================
R_LoadVisibility
=================
*/
static void R_LoadVisibility( const mapFile_t *bsp ) {
	unsigned numClusters, clusterBytes, len;
	const byte	*buf;

	len = PAD( s_worldData.numClusters, 64 ) >> 3;
	s_worldData.novis = ri.Hunk_Alloc( len, h_low );
	memset( s_worldData.novis, 0xff, len );

	// The neutral map already split the 8-byte VIS_HEADER off the lump: the
	// scalar header fields are pre-swapped (numClusters/clusterBytes) and
	// bsp->visibility points at the vis body, length = visibilityLength. So no
	// LittleLong and no VIS_HEADER offset is repeated here.
	len = bsp->visibilityLength;
	if ( !len ) {
		return;
	}

	numClusters = bsp->numClusters;
	clusterBytes = bsp->clusterBytes;
	buf = bsp->visibility;

	if ( (uint64_t)numClusters * clusterBytes > len ) {
		Com_Terminate( TERM_CLIENT_DROP, "%s: lump too short", __func__ );
	}
	if ( numClusters < s_worldData.numClusters ) {
		Com_Terminate( TERM_CLIENT_DROP, "%s: bad numClusters", __func__ );
	}
	if ( clusterBytes < (numClusters + 7) >> 3 ) {
		Com_Terminate( TERM_CLIENT_DROP, "%s: bad clusterBytes", __func__ );
	}

	s_worldData.numClusters = numClusters;
	s_worldData.clusterBytes = clusterBytes;

	// CM_Load should have given us the vis data to share, so
	// we don't need to allocate another copy
	if ( tr.externalVisData ) {
		s_worldData.vis = tr.externalVisData;
	} else {
		byte	*dest;

		dest = ri.Hunk_Alloc( len, h_low );
		memcpy( dest, buf, len );
		s_worldData.vis = dest;
	}
}

//===============================================================================


/*
===============
ShaderForShaderNum
===============
*/
static shader_t *ShaderForShaderNum( const int shaderNum, int lightmapNum ) {
	shader_t	*shader;
	const dshader_t *dsh;

	if ( shaderNum < 0 || shaderNum >= s_worldData.numShaders ) {
		ri.Terminate( TERM_CLIENT_DROP, "ShaderForShaderNum: bad num %i", shaderNum );
	}

	dsh = &s_worldData.shaders[ shaderNum ];

	if ( ( r_vertexLight->integer && tr.vertexLightingAllowed ) || glConfig.hardwareType == GLHW_PERMEDIA2 ) {
		lightmapNum = LIGHTMAP_BY_VERTEX;
	}

	if ( r_fullbright->integer ) {
		lightmapNum = LIGHTMAP_WHITEIMAGE;
	}

	shader = R_FindShader( dsh->shader, lightmapNum, qtrue );


	// if the shader had errors, just use default shader
	if ( shader->defaultShader ) {
		return tr.defaultShader;
	}

	if ( r_singleShader->integer && !shader->isSky ) {
		return tr.defaultShader;
	}

	return shader;
}


#ifdef USE_PMLIGHT
static void GenerateNormals( srfSurfaceFace_t *face )
{
	vec3_t ba, ca, cross;
	float *v1, *v2, *v3, *n1, *n2, *n3;
	int i, *indices, i0, i1, i2;

	indices = ((int *)((byte *)face + face->ofsIndices));

	// store as vec4_t so we can simply use memcpy() during tesselation
	face->normals = ri.Hunk_Alloc( face->numPoints * sizeof( tess.normal[0] ), h_low );

	for ( i = 0; i < face->numIndices; i += 3 ) {
		i0 = indices[i+0];
		i1 = indices[i+1];
		i2 = indices[i+2];
		if ( i0 >= face->numPoints || i1 >= face->numPoints || i2 >= face->numPoints )
			continue;
		v1 = face->points[i0];
		v2 = face->points[i1];
		v3 = face->points[i2];
		VectorSubtract( v3, v1, ca );
		VectorSubtract( v2, v1, ba );
		CrossProduct( ca, ba, cross );
		n1 = face->normals + indices[i+0]*4;
		n2 = face->normals + indices[i+1]*4;
		n3 = face->normals + indices[i+2]*4;
		VectorAdd( n1, cross, n1 );
		VectorAdd( n2, cross, n2 );
		VectorAdd( n3, cross, n3 );
	}

	for ( i = 0; i < face->numPoints; i++ ) {
		n1 = face->normals + i*4;
		VectorNormalize2( n1, n1 );
		for ( i0 = 0; i0 < 3; i0++ ) {
			n1[i0] = R_ClampDenorm( n1[i0] );
		}
	}
}
#endif // USE_PMLIGHT


/*
=============
qsort_idx
=============
*/
void qsort_idx( int32_t *a, const int n ) {
	int32_t temp[3], m;
	int i, j, x;

	i = 0;
	j = n;
	x = (n >> 1)*3;
	m = a[ x + 0 ] + a[ x + 1 ] + a[ x + 2 ];

	do {
		while ( a[i*3+0]+a[i*3+1]+a[i*3+2] < m )
			i++;
		while ( a[j*3+0]+a[j*3+1]+a[j*3+2] > m )
			j--;
		if ( i <= j ) {
			memcpy( temp, &a[i*3], sizeof( temp ) );
			memcpy( &a[i*3], &a[j*3], sizeof( temp ) );
			memcpy( &a[j*3], temp, sizeof( temp ) );
			i++;
			j--;
		}
	} while ( i <= j );

	if ( j > 0 ) qsort_idx( a, j );
	if ( n > i ) qsort_idx( a+i*3, n-i );
}


/*
======================
R_Q1_BuildAnimChain
======================
Builds a q1AnimChain_t for a +0..+9 animated surface shader.
Only called when the surface's base name starts with '+0'.
Returns NULL when fewer than 2 frames exist (no cycle needed).
All frame shaders receive back-pointers to the same chain.
*/
static q1AnimChain_t *R_Q1_BuildAnimChain( shader_t *baseShader, int lightmapNum ) {
	const char *name = baseShader->name;
	if ( name[0] != '+' || name[1] != '0' )
		return NULL;

	char            frameName[MAX_QPATH];
	shader_t       *frameShaders[Q1_MAX_ANIM_FRAMES];
	int             numFrames = 0;

	Q_strncpyz( frameName, name, sizeof( frameName ) );
	R_LOG( rch_assets_q1, SEV_DEBUG, "BuildAnimChain entry: base='%s' lm=%d\n", name, lightmapNum );
	for ( int i = 0; i < Q1_MAX_ANIM_FRAMES; i++ ) {
		frameName[1] = '0' + i;
		const q1TexInfo_t *qf = R_Q1_GetTexForName( frameName );
		if ( !qf || !qf->diffuse ) {
			R_LOG( rch_assets_q1, SEV_DEBUG, "  frame[%d] '%s': tex=%s\n", i, frameName, qf ? "found/no-diffuse" : "not-in-cache" );
			break;
		}
		shader_t *sh = qf->cachedShader ? qf->cachedShader : R_FindShader( frameName, lightmapNum, qtrue );
		if ( !sh || sh->defaultShader ) {
			R_LOG( rch_assets_q1, SEV_DEBUG, "  frame[%d] '%s': shader=%s\n", i, frameName, sh ? "defaultShader" : "null" );
			break;
		}
		R_LOG( rch_assets_q1, SEV_DEBUG, "  frame[%d] '%s': OK sh='%s' cached=%d\n", i, frameName, sh->name, qf->cachedShader != NULL );
		frameShaders[numFrames++] = sh;
	}

	if ( numFrames <= 1 ) {
		R_LOG( rch_assets_q1, SEV_DEBUG, "BuildAnimChain '%s': numFrames=%d -> returning NULL\n", name, numFrames );
		return NULL;
	}

	q1AnimChain_t *chain = ri.Hunk_Alloc( sizeof( *chain ), h_low );
	chain->numFrames = numFrames;
	for ( int i = 0; i < numFrames; i++ ) {
		chain->shaders[i] = frameShaders[i];
	}
	return chain;
}


/*
================
ParseFaceCommon

Shared body for ParseFace (world) and ParsePropFace (props).
Caller pre-resolves shader and lightmap coords; this function owns
vertex/index copy, plane derivation, and hunk allocation.
Returns the allocated srfSurfaceFace_t so the caller can set
altShader; also writes surf->data and surf->shader.
fogIndex is NOT set here — that is always the caller's responsibility.
================
*/
static srfSurfaceFace_t *ParseFaceCommon( const dsurface_t *ds, const drawVert_t *verts,
                                           int numPoints, msurface_t *surf,
                                           int *srcIndexes, int numIndexes,
                                           shader_t *shader,
                                           int lightmapNum, float lightmapX, float lightmapY,
                                           const byte *lightStyles ) {
	int              i, j;
	srfSurfaceFace_t *cv;
	int              *indexes;
	int              sfaceSize, ofsIndexes;

	if ( numPoints > MAX_FACE_POINTS ) {
		R_LOG( rch_assets, SEV_WARN, "WARNING: MAX_FACE_POINTS exceeded: %i\n", numPoints );
		numPoints = MAX_FACE_POINTS;
		shader = tr.defaultShader;
	}
	surf->shader = shader;

	sfaceSize  = sizeof( *cv ) - sizeof( cv->points ) + sizeof( cv->points[0] ) * numPoints;
	ofsIndexes = sfaceSize;
	sfaceSize += sizeof( int ) * numIndexes;

	cv = ri.Hunk_Alloc( sfaceSize, h_low );
	cv->surfaceType = SF_FACE;
	cv->numPoints   = numPoints;
	cv->numIndices  = numIndexes;
	cv->ofsIndices  = ofsIndexes;
	if ( lightStyles ) {
		cv->lightStyles[0] = lightStyles[0];
		cv->lightStyles[1] = lightStyles[1];
		cv->lightStyles[2] = lightStyles[2];
		cv->lightStyles[3] = lightStyles[3];
	} else {
		cv->lightStyles[0] = cv->lightStyles[1] = cv->lightStyles[2] = cv->lightStyles[3] = 255;
	}
	cv->altShader = NULL;

	// drawVert fields (xyz/st/lightmap) arrive already LittleFloat-swapped from
	// the neutral map, so they are copied verbatim here (no re-swap).
	for ( i = 0; i < numPoints; i++ ) {
		for ( j = 0; j < 3; j++ )
			cv->points[i][j] = verts[i].xyz[j];
		for ( j = 0; j < 2; j++ ) {
			cv->points[i][3+j] = verts[i].st[j];
			cv->points[i][5+j] = verts[i].lightmap[j];
		}
		R_ColorShiftLightingBytes( verts[i].color.rgba, (byte *)&cv->points[i][7], qtrue, qfalse );
		if ( lightmapNum >= 0 && tr.mergeLightmaps ) {
			cv->points[i][5] = cv->points[i][5] * tr.lightmapScale[0] + lightmapX;
			cv->points[i][6] = cv->points[i][6] * tr.lightmapScale[1] + lightmapY;
		}
	}

	indexes = (int *)((byte *)cv + cv->ofsIndices);
	for ( i = 0; i < numIndexes; i++ ) {
		unsigned num = (unsigned)srcIndexes[i];   // neutral indexes already swapped
		if ( num >= (unsigned)numPoints )
			ri.Terminate( TERM_CLIENT_DROP, "%s: bad index", __func__ );
		indexes[i] = num;
	}

	// reorder indexes to avoid bug on intel gen 9.5 hardware/vulkan driver (e.g. lun3dm5)
	if ( numIndexes >= 6 )
		qsort_idx( indexes, (numIndexes / 3) - 1 );

	for ( i = 0; i < 3; i++ )
		cv->plane.normal[i] = ds->lightmapVecs[2][i];   // neutral floats already swapped

#ifdef USE_PMLIGHT
	if ( surf->shader->numUnfoggedPasses && surf->shader->lightingStage >= 0 ) {
		if ( fabsf( cv->plane.normal[0] ) < 0.01f && fabsf( cv->plane.normal[1] ) < 0.01f && fabsf( cv->plane.normal[2] ) < 0.01f ) {
			// Zero-normals case:
			// might happen if surface contains multiple non-coplanar faces for terrain simulation
			// like in 'Pyramid of the Magician', 'tvy-bench' or 'terrast' maps
			// which results in non-working new per-pixel dynamic lighting.
			// So we will try to regenerate normals and apply smooth shading
			// for normals that is shared between multiple faces.
			// It is not a big problem for incorrectly (negative) generated normals
			// because it is unlikely for shared ones and will result in the same non-working lighting.
			// Also we will NOT update existing face->plane.normal to avoid potential surface culling issues
			GenerateNormals( cv );
		}
	}
#endif

	// fix up broken (degenerate) face plane normals from the BSP by computing
	// a unit-length normal from the first triangle's vertices, which helps
	// dynamic light face culling work correctly (e.g. map "industrial")
	if ( numPoints >= 3 && numIndexes >= 3 ) {
		const float lenSq = cv->plane.normal[0] * cv->plane.normal[0]
			+ cv->plane.normal[1] * cv->plane.normal[1]
			+ cv->plane.normal[2] * cv->plane.normal[2];
		if ( lenSq < 0.01f ) {
			vec3_t v1, v2, normal;
			const int i0 = indexes[0];
			const int i1 = indexes[1];
			const int i2 = indexes[2];
			VectorSubtract( cv->points[i2], cv->points[i0], v1 );
			VectorSubtract( cv->points[i1], cv->points[i0], v2 );
			CrossProduct( v1, v2, normal );
			VectorNormalize( normal );
			VectorCopy( normal, cv->plane.normal );
		}
	}

	for ( i = 0; i < 3; i++ )
		cv->plane.normal[i] = R_ClampDenorm( cv->plane.normal[i] );

	cv->plane.dist = DotProduct( cv->points[0], cv->plane.normal );
	SetPlaneSignbits( &cv->plane );
	cv->plane.type = PlaneTypeForNormal( cv->plane.normal );

	surf->data = (surfaceType_t *)cv;
	return cv;
}


/*
===============
ParseFace
===============
*/
static void ParseFace( const dsurface_t *ds, const drawVert_t *verts, int numPoints, msurface_t *surf, int *srcIndexes, int numIndexes, const byte *lightStyles ) {
	int              lightmapNum;
	float            lightmapX, lightmapY;
	shader_t        *shader;
	srfSurfaceFace_t *cv;

	lightmapNum = ds->lightmapNum;   // neutral surface ints already swapped
	if ( lightmapNum >= 0 && tr.mergeLightmaps ) {
		lightmapNum = R_GetLightmapCoords( lightmapNum, &lightmapX, &lightmapY );
	} else {
		lightmapX = lightmapY = 0.0f;
	}
	tr.lightmapOffset[0] = lightmapX;
	tr.lightmapOffset[1] = lightmapY;

	shader = ShaderForShaderNum( ds->shaderNum, lightmapNum );
	R_LOG( rch_assets_q1, SEV_TRACE, "ParseFace: raw='%s' resolved='%s' default=%d lm=%d\n",
	        shader->name, shader->name, shader->defaultShader, lightmapNum );

	cv = ParseFaceCommon( ds, verts, numPoints, surf, srcIndexes, numIndexes,
	                      shader, lightmapNum, lightmapX, lightmapY, lightStyles );

	// +a variant means entity-toggled (button); +0..+9 numeric cycle only for non-toggled textures
	if ( surf->shader->name[0] == '+' && surf->shader->name[1] == '0' ) {
		R_LOG( rch_assets_q1, SEV_TRACE, "+0 branch hit: surf='%s'\n", surf->shader->name );
		char altName[MAX_QPATH];
		Q_strncpyz( altName, surf->shader->name, sizeof(altName) );
		altName[1] = 'a';
		shader_t *alt = R_FindShader( altName, lightmapNum, qtrue );
		cv->altShader = ( alt && !alt->defaultShader ) ? alt : NULL;
	}
}


/*
===============
ParseMesh
===============
*/
static void ParseMesh( const dsurface_t *ds, const drawVert_t *verts, int numVerts, msurface_t *surf ) {
	srfGridMesh_t	*grid;
	int				i, j;
	unsigned		width, height, numPoints;
	drawVert_t points[MAX_PATCH_SIZE*MAX_PATCH_SIZE];
	int				lightmapNum;
	float			lightmapX, lightmapY;
	vec3_t			bounds[2];
	vec3_t			tmpVec;
	static surfaceType_t	skipData = SF_SKIP;

	lightmapNum = ds->lightmapNum;   // neutral surface ints already swapped
	if ( lightmapNum >= 0 && tr.mergeLightmaps ) {
		lightmapNum = R_GetLightmapCoords( lightmapNum, &lightmapX, &lightmapY );
	} else {
		lightmapX = lightmapY = 0.0f;
	}

	tr.lightmapOffset[0] = lightmapX;
	tr.lightmapOffset[1] = lightmapY;

	// get shader value
	surf->shader = ShaderForShaderNum( ds->shaderNum, lightmapNum );

	// we may have a nodraw surface, because they might still need to
	// be around for movement clipping
	if ( s_worldData.shaders[ ds->shaderNum ].surfaceFlags & SURF_NODRAW ) {
		surf->data = &skipData;
		return;
	}

	width = ds->patchWidth;
	height = ds->patchHeight;

	// mirror CM_GeneratePatchCollide() checks
	if (width <= 2 || height <= 2 || !(width & 1) || !(height & 1) ||
		width > MAX_GRID_SIZE || height > MAX_GRID_SIZE ||
		width * height > ARRAY_LEN(points))
		ri.Terminate( TERM_CLIENT_DROP, "%s: bad patch size", __func__ );

	numPoints = width * height;
	if (numPoints > numVerts)
		ri.Terminate( TERM_CLIENT_DROP, "%s: verts out of range", __func__ );

	// neutral drawVert floats arrive already LittleFloat-swapped (no re-swap)
	for ( i = 0 ; i < numPoints ; i++ ) {
		for ( j = 0 ; j < 3 ; j++ ) {
			points[i].xyz[j] = verts[i].xyz[j];
			points[i].normal[j] = R_ClampDenorm( verts[i].normal[j] );
		}
		for ( j = 0 ; j < 2 ; j++ ) {
			points[i].st[j] = verts[i].st[j];
			points[i].lightmap[j] = verts[i].lightmap[j];
		}
		R_ColorShiftLightingBytes( verts[i].color.rgba, points[i].color.rgba, qtrue, qfalse );
		if ( lightmapNum >= 0 && tr.mergeLightmaps ) {
			// adjust lightmap coords
			points[i].lightmap[0] = points[i].lightmap[0] * tr.lightmapScale[0] + lightmapX;
			points[i].lightmap[1] = points[i].lightmap[1] * tr.lightmapScale[1] + lightmapY;
		}
	}

	// pre-tesseleate
	grid = R_SubdividePatchToGrid( width, height, points );
	surf->data = (surfaceType_t *)grid;

	// copy the level of detail origin, which is the center
	// of the group of all curves that must subdivide the same
	// to avoid cracking (neutral surface floats already LittleFloat-swapped)
	for ( i = 0 ; i < 3 ; i++ ) {
		bounds[0][i] = ds->lightmapVecs[0][i];
		bounds[1][i] = ds->lightmapVecs[1][i];
	}
	VectorAdd( bounds[0], bounds[1], bounds[1] );
	VectorScale( bounds[1], 0.5f, grid->lodOrigin );
	VectorSubtract( bounds[0], grid->lodOrigin, tmpVec );
	grid->lodRadius = VectorLength( tmpVec );
}


/*
===============
ParseTriSurf
===============
*/
static void ParseTriSurf( const dsurface_t *ds, const drawVert_t *verts, int numVerts, msurface_t *surf, int *indexes, int numIndexes ) {
	srfTriangles_t	*tri;
	int				i, j;
	int				lightmapNum;
	float			lightmapX, lightmapY;

	lightmapNum = ds->lightmapNum;   // neutral surface ints already swapped
	if ( lightmapNum >= 0 && tr.mergeLightmaps ) {
		lightmapNum = R_GetLightmapCoords( lightmapNum, &lightmapX, &lightmapY );
	} else {
		lightmapX = lightmapY = 0.0f;
	}

	tr.lightmapOffset[0] = lightmapX;
	tr.lightmapOffset[1] = lightmapY;

	// get shader
	surf->shader = ShaderForShaderNum( ds->shaderNum, LIGHTMAP_BY_VERTEX );

	tri = ri.Hunk_Alloc( sizeof( *tri ) + numVerts * sizeof( tri->verts[0] )
		+ numIndexes * sizeof( tri->indexes[0] ), h_low );
	tri->surfaceType = SF_TRIANGLES;
	tri->numVerts = numVerts;
	tri->numIndexes = numIndexes;
	tri->verts = (drawVert_t *)(tri + 1);
	tri->indexes = (int *)(tri->verts + tri->numVerts );

	surf->data = (surfaceType_t *)tri;

	// copy vertexes (neutral drawVert floats already LittleFloat-swapped)
	ClearBounds( tri->bounds[0], tri->bounds[1] );
	for ( i = 0 ; i < numVerts ; i++ ) {
		for ( j = 0 ; j < 3 ; j++ ) {
			tri->verts[i].xyz[j] = verts[i].xyz[j];
			tri->verts[i].normal[j] = R_ClampDenorm( verts[i].normal[j] );
		}
		AddPointToBounds( tri->verts[i].xyz, tri->bounds[0], tri->bounds[1] );
		for ( j = 0 ; j < 2 ; j++ ) {
			tri->verts[i].st[j] = verts[i].st[j];
			tri->verts[i].lightmap[j] = verts[i].lightmap[j];
		}

		R_ColorShiftLightingBytes( verts[i].color.rgba, tri->verts[i].color.rgba, qtrue, qfalse );
		if ( lightmapNum >= 0 && tr.mergeLightmaps ) {
			// adjust lightmap coords
			tri->verts[i].lightmap[0] = tri->verts[i].lightmap[0] * tr.lightmapScale[0] + lightmapX;
			tri->verts[i].lightmap[1] = tri->verts[i].lightmap[1] * tr.lightmapScale[1] + lightmapY;
		}
	}

	// copy indexes (neutral indexes already LittleLong-swapped)
	for ( i = 0 ; i < numIndexes ; i++ ) {
		tri->indexes[i] = indexes[i];
		if ( tri->indexes[i] < 0 || tri->indexes[i] >= numVerts ) {
			ri.Terminate( TERM_CLIENT_DROP, "Bad index in triangle surface" );
		}
	}
}


/*
===============
ParseFlare
===============
*/
static void ParseFlare( const dsurface_t *ds, msurface_t *surf ) {
	srfFlare_t		*flare;

	// get shader (neutral surface ints already swapped)
	surf->shader = ShaderForShaderNum( ds->shaderNum, LIGHTMAP_BY_VERTEX );

	flare = ri.Hunk_Alloc( sizeof( *flare ), h_low );
	flare->surfaceType = SF_FLARE;

	surf->data = (surfaceType_t *)flare;

	// neutral surface floats already LittleFloat-swapped (no re-swap)
	for ( int i = 0 ; i < 3 ; i++ ) {
		flare->origin[i] = ds->lightmapOrigin[i];
		flare->color[i] = ds->lightmapVecs[0][i];
		flare->normal[i] = R_ClampDenorm( ds->lightmapVecs[2][i] );
	}
}


/*
=================
R_MergedWidthPoints

returns qtrue if there are grid points merged on a width edge
=================
*/
static qboolean R_MergedWidthPoints( const srfGridMesh_t *grid, int offset ) {
	int i, j;

	for (i = 1; i < grid->width-1; i++) {
		for (j = i + 1; j < grid->width-1; j++) {
			if ( fabs(grid->verts[i + offset].xyz[0] - grid->verts[j + offset].xyz[0]) > .1) continue;
			if ( fabs(grid->verts[i + offset].xyz[1] - grid->verts[j + offset].xyz[1]) > .1) continue;
			if ( fabs(grid->verts[i + offset].xyz[2] - grid->verts[j + offset].xyz[2]) > .1) continue;
			return qtrue;
		}
	}
	return qfalse;
}


/*
=================
R_MergedHeightPoints

returns qtrue if there are grid points merged on a height edge
=================
*/
static qboolean R_MergedHeightPoints( const srfGridMesh_t *grid, int offset ) {
	int i, j;

	for (i = 1; i < grid->height-1; i++) {
		for (j = i + 1; j < grid->height-1; j++) {
			if ( fabs(grid->verts[grid->width * i + offset].xyz[0] - grid->verts[grid->width * j + offset].xyz[0]) > .1) continue;
			if ( fabs(grid->verts[grid->width * i + offset].xyz[1] - grid->verts[grid->width * j + offset].xyz[1]) > .1) continue;
			if ( fabs(grid->verts[grid->width * i + offset].xyz[2] - grid->verts[grid->width * j + offset].xyz[2]) > .1) continue;
			return qtrue;
		}
	}
	return qfalse;
}


/*
=================
R_FixSharedVertexLodError_r

NOTE: never sync LoD through grid edges with merged points!

FIXME: write generalized version that also avoids cracks between a patch and one that meets half way?
=================
*/
static void R_FixSharedVertexLodError_r( int start, srfGridMesh_t *grid1 ) {
	int j, k, l, m, n, offset1, offset2, touch;
	srfGridMesh_t *grid2;

	for ( j = start; j < s_worldData.numsurfaces; j++ ) {
		//
		grid2 = (srfGridMesh_t *) s_worldData.surfaces[j].data;
		// if this surface is not a grid
		if ( grid2->surfaceType != SF_GRID ) continue;
		// if the LOD errors are already fixed for this patch
		if ( grid2->lodFixed == 2 ) continue;
		// grids in the same LOD group should have the exact same lod radius
		if ( grid1->lodRadius != grid2->lodRadius ) continue;
		// grids in the same LOD group should have the exact same lod origin
		if ( grid1->lodOrigin[0] != grid2->lodOrigin[0] ) continue;
		if ( grid1->lodOrigin[1] != grid2->lodOrigin[1] ) continue;
		if ( grid1->lodOrigin[2] != grid2->lodOrigin[2] ) continue;
		//
		touch = qfalse;
		for (n = 0; n < 2; n++) {
			//
			if (n) offset1 = (grid1->height-1) * grid1->width;
			else offset1 = 0;
			if (R_MergedWidthPoints(grid1, offset1)) continue;
			for (k = 1; k < grid1->width-1; k++) {
				for (m = 0; m < 2; m++) {

					if (m) offset2 = (grid2->height-1) * grid2->width;
					else offset2 = 0;
					if (R_MergedWidthPoints(grid2, offset2)) continue;
					for ( l = 1; l < grid2->width-1; l++) {
					//
						if ( fabs(grid1->verts[k + offset1].xyz[0] - grid2->verts[l + offset2].xyz[0]) > .1) continue;
						if ( fabs(grid1->verts[k + offset1].xyz[1] - grid2->verts[l + offset2].xyz[1]) > .1) continue;
						if ( fabs(grid1->verts[k + offset1].xyz[2] - grid2->verts[l + offset2].xyz[2]) > .1) continue;
						// ok the points are equal and should have the same lod error
						grid2->widthLodError[l] = grid1->widthLodError[k];
						touch = qtrue;
					}
				}
				for (m = 0; m < 2; m++) {

					if (m) offset2 = grid2->width-1;
					else offset2 = 0;
					if (R_MergedHeightPoints(grid2, offset2)) continue;
					for ( l = 1; l < grid2->height-1; l++) {
					//
						if ( fabs(grid1->verts[k + offset1].xyz[0] - grid2->verts[grid2->width * l + offset2].xyz[0]) > .1) continue;
						if ( fabs(grid1->verts[k + offset1].xyz[1] - grid2->verts[grid2->width * l + offset2].xyz[1]) > .1) continue;
						if ( fabs(grid1->verts[k + offset1].xyz[2] - grid2->verts[grid2->width * l + offset2].xyz[2]) > .1) continue;
						// ok the points are equal and should have the same lod error
						grid2->heightLodError[l] = grid1->widthLodError[k];
						touch = qtrue;
					}
				}
			}
		}
		for (n = 0; n < 2; n++) {
			//
			if (n) offset1 = grid1->width-1;
			else offset1 = 0;
			if (R_MergedHeightPoints(grid1, offset1)) continue;
			for (k = 1; k < grid1->height-1; k++) {
				for (m = 0; m < 2; m++) {

					if (m) offset2 = (grid2->height-1) * grid2->width;
					else offset2 = 0;
					if (R_MergedWidthPoints(grid2, offset2)) continue;
					for ( l = 1; l < grid2->width-1; l++) {
					//
						if ( fabs(grid1->verts[grid1->width * k + offset1].xyz[0] - grid2->verts[l + offset2].xyz[0]) > .1) continue;
						if ( fabs(grid1->verts[grid1->width * k + offset1].xyz[1] - grid2->verts[l + offset2].xyz[1]) > .1) continue;
						if ( fabs(grid1->verts[grid1->width * k + offset1].xyz[2] - grid2->verts[l + offset2].xyz[2]) > .1) continue;
						// ok the points are equal and should have the same lod error
						grid2->widthLodError[l] = grid1->heightLodError[k];
						touch = qtrue;
					}
				}
				for (m = 0; m < 2; m++) {

					if (m) offset2 = grid2->width-1;
					else offset2 = 0;
					if (R_MergedHeightPoints(grid2, offset2)) continue;
					for ( l = 1; l < grid2->height-1; l++) {
					//
						if ( fabs(grid1->verts[grid1->width * k + offset1].xyz[0] - grid2->verts[grid2->width * l + offset2].xyz[0]) > .1) continue;
						if ( fabs(grid1->verts[grid1->width * k + offset1].xyz[1] - grid2->verts[grid2->width * l + offset2].xyz[1]) > .1) continue;
						if ( fabs(grid1->verts[grid1->width * k + offset1].xyz[2] - grid2->verts[grid2->width * l + offset2].xyz[2]) > .1) continue;
						// ok the points are equal and should have the same lod error
						grid2->heightLodError[l] = grid1->heightLodError[k];
						touch = qtrue;
					}
				}
			}
		}
		if (touch) {
			grid2->lodFixed = 2;
			R_FixSharedVertexLodError_r ( start, grid2 );
			//NOTE: this would be correct but makes things really slow
			//grid2->lodFixed = 1;
		}
	}
}


/*
=================
R_FixSharedVertexLodError

This function assumes that all patches in one group are nicely stitched together for the highest LoD.
If this is not the case this function will still do its job but won't fix the highest LoD cracks.
=================
*/
static void R_FixSharedVertexLodError( void ) {
	srfGridMesh_t *grid1;

	for ( int i = 0; i < s_worldData.numsurfaces; i++ ) {
		//
		grid1 = (srfGridMesh_t *) s_worldData.surfaces[i].data;
		// if this surface is not a grid
		if ( grid1->surfaceType != SF_GRID )
			continue;
		//
		if ( grid1->lodFixed )
			continue;
		//
		grid1->lodFixed = 2;
		// recursively fix other patches in the same LOD group
		R_FixSharedVertexLodError_r( i + 1, grid1);
	}
}


/*
===============
R_StitchPatches
===============
*/
static int R_StitchPatches( int grid1num, int grid2num ) {
	float *v1, *v2;
	srfGridMesh_t *grid1, *grid2;
	int k, l, m, n, offset1, offset2, row, column;

	grid1 = (srfGridMesh_t *) s_worldData.surfaces[grid1num].data;
	grid2 = (srfGridMesh_t *) s_worldData.surfaces[grid2num].data;
	for (n = 0; n < 2; n++) {
		//
		if (n) offset1 = (grid1->height-1) * grid1->width;
		else offset1 = 0;
		if (R_MergedWidthPoints(grid1, offset1))
			continue;
		for (k = 0; k < grid1->width-2; k += 2) {

			for (m = 0; m < 2; m++) {

				if ( grid2->width >= MAX_GRID_SIZE )
					break;
				if (m) offset2 = (grid2->height-1) * grid2->width;
				else offset2 = 0;
				for ( l = 0; l < grid2->width-1; l++) {
				//
					v1 = grid1->verts[k + offset1].xyz;
					v2 = grid2->verts[l + offset2].xyz;
					if ( fabs(v1[0] - v2[0]) > .1)
						continue;
					if ( fabs(v1[1] - v2[1]) > .1)
						continue;
					if ( fabs(v1[2] - v2[2]) > .1)
						continue;

					v1 = grid1->verts[k + 2 + offset1].xyz;
					v2 = grid2->verts[l + 1 + offset2].xyz;
					if ( fabs(v1[0] - v2[0]) > .1)
						continue;
					if ( fabs(v1[1] - v2[1]) > .1)
						continue;
					if ( fabs(v1[2] - v2[2]) > .1)
						continue;
					//
					v1 = grid2->verts[l + offset2].xyz;
					v2 = grid2->verts[l + 1 + offset2].xyz;
					if ( fabs(v1[0] - v2[0]) < .01 &&
							fabs(v1[1] - v2[1]) < .01 &&
							fabs(v1[2] - v2[2]) < .01)
						continue;
					//
					//R_LOG( rch_assets, SEV_INFO, "found highest LoD crack between two patches\n" );
					// insert column into grid2 right after column l
					if (m) row = grid2->height-1;
					else row = 0;
					grid2 = R_GridInsertColumn( grid2, l+1, row,
									grid1->verts[k + 1 + offset1].xyz, grid1->widthLodError[k+1]);
					grid2->lodStitched = qfalse;
					s_worldData.surfaces[grid2num].data = (void *) grid2;
					return qtrue;
				}
			}
			for (m = 0; m < 2; m++) {

				if (grid2->height >= MAX_GRID_SIZE)
					break;
				if (m) offset2 = grid2->width-1;
				else offset2 = 0;
				for ( l = 0; l < grid2->height-1; l++) {
					//
					v1 = grid1->verts[k + offset1].xyz;
					v2 = grid2->verts[grid2->width * l + offset2].xyz;
					if ( fabs(v1[0] - v2[0]) > .1)
						continue;
					if ( fabs(v1[1] - v2[1]) > .1)
						continue;
					if ( fabs(v1[2] - v2[2]) > .1)
						continue;

					v1 = grid1->verts[k + 2 + offset1].xyz;
					v2 = grid2->verts[grid2->width * (l + 1) + offset2].xyz;
					if ( fabs(v1[0] - v2[0]) > .1)
						continue;
					if ( fabs(v1[1] - v2[1]) > .1)
						continue;
					if ( fabs(v1[2] - v2[2]) > .1)
						continue;
					//
					v1 = grid2->verts[grid2->width * l + offset2].xyz;
					v2 = grid2->verts[grid2->width * (l + 1) + offset2].xyz;
					if ( fabs(v1[0] - v2[0]) < .01 &&
							fabs(v1[1] - v2[1]) < .01 &&
							fabs(v1[2] - v2[2]) < .01)
						continue;
					//
					//R_LOG( rch_assets, SEV_INFO, "found highest LoD crack between two patches\n" );
					// insert row into grid2 right after row l
					if (m) column = grid2->width-1;
					else column = 0;
					grid2 = R_GridInsertRow( grid2, l+1, column,
										grid1->verts[k + 1 + offset1].xyz, grid1->widthLodError[k+1]);
					grid2->lodStitched = qfalse;
					s_worldData.surfaces[grid2num].data = (void *) grid2;
					return qtrue;
				}
			}
		}
	}
	for (n = 0; n < 2; n++) {
		//
		if (n) offset1 = grid1->width-1;
		else offset1 = 0;
		if (R_MergedHeightPoints(grid1, offset1))
			continue;
		for (k = 0; k < grid1->height-2; k += 2) {
			for (m = 0; m < 2; m++) {

				if ( grid2->width >= MAX_GRID_SIZE )
					break;
				if (m) offset2 = (grid2->height-1) * grid2->width;
				else offset2 = 0;
				for ( l = 0; l < grid2->width-1; l++) {
				//
					v1 = grid1->verts[grid1->width * k + offset1].xyz;
					v2 = grid2->verts[l + offset2].xyz;
					if ( fabs(v1[0] - v2[0]) > .1)
						continue;
					if ( fabs(v1[1] - v2[1]) > .1)
						continue;
					if ( fabs(v1[2] - v2[2]) > .1)
						continue;

					v1 = grid1->verts[grid1->width * (k + 2) + offset1].xyz;
					v2 = grid2->verts[l + 1 + offset2].xyz;
					if ( fabs(v1[0] - v2[0]) > .1)
						continue;
					if ( fabs(v1[1] - v2[1]) > .1)
						continue;
					if ( fabs(v1[2] - v2[2]) > .1)
						continue;
					//
					v1 = grid2->verts[l + offset2].xyz;
					v2 = grid2->verts[(l + 1) + offset2].xyz;
					if ( fabs(v1[0] - v2[0]) < .01 &&
							fabs(v1[1] - v2[1]) < .01 &&
							fabs(v1[2] - v2[2]) < .01)
						continue;
					//
					//R_LOG( rch_assets, SEV_INFO, "found highest LoD crack between two patches\n" );
					// insert column into grid2 right after column l
					if (m) row = grid2->height-1;
					else row = 0;
					grid2 = R_GridInsertColumn( grid2, l+1, row,
									grid1->verts[grid1->width * (k + 1) + offset1].xyz, grid1->heightLodError[k+1]);
					grid2->lodStitched = qfalse;
					s_worldData.surfaces[grid2num].data = (void *) grid2;
					return qtrue;
				}
			}
			for (m = 0; m < 2; m++) {

				if (grid2->height >= MAX_GRID_SIZE)
					break;
				if (m) offset2 = grid2->width-1;
				else offset2 = 0;
				for ( l = 0; l < grid2->height-1; l++) {
				//
					v1 = grid1->verts[grid1->width * k + offset1].xyz;
					v2 = grid2->verts[grid2->width * l + offset2].xyz;
					if ( fabs(v1[0] - v2[0]) > .1)
						continue;
					if ( fabs(v1[1] - v2[1]) > .1)
						continue;
					if ( fabs(v1[2] - v2[2]) > .1)
						continue;

					v1 = grid1->verts[grid1->width * (k + 2) + offset1].xyz;
					v2 = grid2->verts[grid2->width * (l + 1) + offset2].xyz;
					if ( fabs(v1[0] - v2[0]) > .1)
						continue;
					if ( fabs(v1[1] - v2[1]) > .1)
						continue;
					if ( fabs(v1[2] - v2[2]) > .1)
						continue;
					//
					v1 = grid2->verts[grid2->width * l + offset2].xyz;
					v2 = grid2->verts[grid2->width * (l + 1) + offset2].xyz;
					if ( fabs(v1[0] - v2[0]) < .01 &&
							fabs(v1[1] - v2[1]) < .01 &&
							fabs(v1[2] - v2[2]) < .01)
						continue;
					//
					//R_LOG( rch_assets, SEV_INFO, "found highest LoD crack between two patches\n" );
					// insert row into grid2 right after row l
					if (m) column = grid2->width-1;
					else column = 0;
					grid2 = R_GridInsertRow( grid2, l+1, column,
									grid1->verts[grid1->width * (k + 1) + offset1].xyz, grid1->heightLodError[k+1]);
					grid2->lodStitched = qfalse;
					s_worldData.surfaces[grid2num].data = (void *) grid2;
					return qtrue;
				}
			}
		}
	}
	for (n = 0; n < 2; n++) {
		//
		if (n) offset1 = (grid1->height-1) * grid1->width;
		else offset1 = 0;
		if (R_MergedWidthPoints(grid1, offset1))
			continue;
		for (k = grid1->width-1; k > 1; k -= 2) {

			for (m = 0; m < 2; m++) {

				if ( !grid2 || grid2->width >= MAX_GRID_SIZE )
					break;
				if (m) offset2 = (grid2->height-1) * grid2->width;
				else offset2 = 0;
				for ( l = 0; l < grid2->width-1; l++) {
				//
					v1 = grid1->verts[k + offset1].xyz;
					v2 = grid2->verts[l + offset2].xyz;
					if ( fabs(v1[0] - v2[0]) > .1)
						continue;
					if ( fabs(v1[1] - v2[1]) > .1)
						continue;
					if ( fabs(v1[2] - v2[2]) > .1)
						continue;

					v1 = grid1->verts[k - 2 + offset1].xyz;
					v2 = grid2->verts[l + 1 + offset2].xyz;
					if ( fabs(v1[0] - v2[0]) > .1)
						continue;
					if ( fabs(v1[1] - v2[1]) > .1)
						continue;
					if ( fabs(v1[2] - v2[2]) > .1)
						continue;
					//
					v1 = grid2->verts[l + offset2].xyz;
					v2 = grid2->verts[(l + 1) + offset2].xyz;
					if ( fabs(v1[0] - v2[0]) < .01 &&
							fabs(v1[1] - v2[1]) < .01 &&
							fabs(v1[2] - v2[2]) < .01)
						continue;
					//
					//R_LOG( rch_assets, SEV_INFO, "found highest LoD crack between two patches\n" );
					// insert column into grid2 right after column l
					if (m) row = grid2->height-1;
					else row = 0;
					grid2 = R_GridInsertColumn( grid2, l+1, row,
										grid1->verts[k - 1 + offset1].xyz, grid1->widthLodError[k+1]);
					grid2->lodStitched = qfalse;
					s_worldData.surfaces[grid2num].data = (void *) grid2;
					return qtrue;
				}
			}
			for (m = 0; m < 2; m++) {

				if (!grid2 || grid2->height >= MAX_GRID_SIZE)
					break;
				if (m) offset2 = grid2->width-1;
				else offset2 = 0;
				for ( l = 0; l < grid2->height-1; l++) {
				//
					v1 = grid1->verts[k + offset1].xyz;
					v2 = grid2->verts[grid2->width * l + offset2].xyz;
					if ( fabs(v1[0] - v2[0]) > .1)
						continue;
					if ( fabs(v1[1] - v2[1]) > .1)
						continue;
					if ( fabs(v1[2] - v2[2]) > .1)
						continue;

					v1 = grid1->verts[k - 2 + offset1].xyz;
					v2 = grid2->verts[grid2->width * (l + 1) + offset2].xyz;
					if ( fabs(v1[0] - v2[0]) > .1)
						continue;
					if ( fabs(v1[1] - v2[1]) > .1)
						continue;
					if ( fabs(v1[2] - v2[2]) > .1)
						continue;
					//
					v1 = grid2->verts[grid2->width * l + offset2].xyz;
					v2 = grid2->verts[grid2->width * (l + 1) + offset2].xyz;
					if ( fabs(v1[0] - v2[0]) < .01 &&
							fabs(v1[1] - v2[1]) < .01 &&
							fabs(v1[2] - v2[2]) < .01)
						continue;
					//
					//R_LOG( rch_assets, SEV_INFO, "found highest LoD crack between two patches\n" );
					// insert row into grid2 right after row l
					if (m) column = grid2->width-1;
					else column = 0;
					grid2 = R_GridInsertRow( grid2, l+1, column,
										grid1->verts[k - 1 + offset1].xyz, grid1->widthLodError[k+1]);
					if (!grid2)
						break;
					grid2->lodStitched = qfalse;
					s_worldData.surfaces[grid2num].data = (void *) grid2;
					return qtrue;
				}
			}
		}
	}
	for (n = 0; n < 2; n++) {
		//
		if (n) offset1 = grid1->width-1;
		else offset1 = 0;
		if (R_MergedHeightPoints(grid1, offset1))
			continue;
		for (k = grid1->height-1; k > 1; k -= 2) {
			for (m = 0; m < 2; m++) {

				if ( !grid2 || grid2->width >= MAX_GRID_SIZE )
					break;
				if (m) offset2 = (grid2->height-1) * grid2->width;
				else offset2 = 0;
				for ( l = 0; l < grid2->width-1; l++) {
				//
					v1 = grid1->verts[grid1->width * k + offset1].xyz;
					v2 = grid2->verts[l + offset2].xyz;
					if ( fabs(v1[0] - v2[0]) > .1)
						continue;
					if ( fabs(v1[1] - v2[1]) > .1)
						continue;
					if ( fabs(v1[2] - v2[2]) > .1)
						continue;

					v1 = grid1->verts[grid1->width * (k - 2) + offset1].xyz;
					v2 = grid2->verts[l + 1 + offset2].xyz;
					if ( fabs(v1[0] - v2[0]) > .1)
						continue;
					if ( fabs(v1[1] - v2[1]) > .1)
						continue;
					if ( fabs(v1[2] - v2[2]) > .1)
						continue;
					//
					v1 = grid2->verts[l + offset2].xyz;
					v2 = grid2->verts[(l + 1) + offset2].xyz;
					if ( fabs(v1[0] - v2[0]) < .01 &&
							fabs(v1[1] - v2[1]) < .01 &&
							fabs(v1[2] - v2[2]) < .01)
						continue;
					//
					//R_LOG( rch_assets, SEV_INFO, "found highest LoD crack between two patches\n" );
					// insert column into grid2 right after column l
					if (m) row = grid2->height-1;
					else row = 0;
					grid2 = R_GridInsertColumn( grid2, l+1, row,
										grid1->verts[grid1->width * (k - 1) + offset1].xyz, grid1->heightLodError[k+1]);
					grid2->lodStitched = qfalse;
					s_worldData.surfaces[grid2num].data = (void *) grid2;
					return qtrue;
				}
			}
			for (m = 0; m < 2; m++) {

				if (!grid2 || grid2->height >= MAX_GRID_SIZE)
					break;
				if (m) offset2 = grid2->width-1;
				else offset2 = 0;
				for ( l = 0; l < grid2->height-1; l++) {
				//
					v1 = grid1->verts[grid1->width * k + offset1].xyz;
					v2 = grid2->verts[grid2->width * l + offset2].xyz;
					if ( fabs(v1[0] - v2[0]) > .1)
						continue;
					if ( fabs(v1[1] - v2[1]) > .1)
						continue;
					if ( fabs(v1[2] - v2[2]) > .1)
						continue;

					v1 = grid1->verts[grid1->width * (k - 2) + offset1].xyz;
					v2 = grid2->verts[grid2->width * (l + 1) + offset2].xyz;
					if ( fabs(v1[0] - v2[0]) > .1)
						continue;
					if ( fabs(v1[1] - v2[1]) > .1)
						continue;
					if ( fabs(v1[2] - v2[2]) > .1)
						continue;
					//
					v1 = grid2->verts[grid2->width * l + offset2].xyz;
					v2 = grid2->verts[grid2->width * (l + 1) + offset2].xyz;
					if ( fabs(v1[0] - v2[0]) < .01 &&
							fabs(v1[1] - v2[1]) < .01 &&
							fabs(v1[2] - v2[2]) < .01)
						continue;
					//
					//R_LOG( rch_assets, SEV_INFO, "found highest LoD crack between two patches\n" );
					// insert row into grid2 right after row l
					if (m) column = grid2->width-1;
					else column = 0;
					grid2 = R_GridInsertRow( grid2, l+1, column,
										grid1->verts[grid1->width * (k - 1) + offset1].xyz, grid1->heightLodError[k+1]);
					grid2->lodStitched = qfalse;
					s_worldData.surfaces[grid2num].data = (void *) grid2;
					return qtrue;
				}
			}
		}
	}
	return qfalse;
}


/*
===============
R_TryStitchPatch

This function will try to stitch patches in the same LoD group together for the highest LoD.

Only single missing vertex cracks will be fixed.

Vertices will be joined at the patch side a crack is first found, at the other side
of the patch (on the same row or column) the vertices will not be joined and cracks
might still appear at that side.
===============
*/
static int R_TryStitchingPatch( int grid1num ) {
	int j, numstitches;
	srfGridMesh_t *grid1, *grid2;

	numstitches = 0;
	grid1 = (srfGridMesh_t *) s_worldData.surfaces[grid1num].data;
	for ( j = 0; j < s_worldData.numsurfaces; j++ ) {
		//
		grid2 = (srfGridMesh_t *) s_worldData.surfaces[j].data;
		// if this surface is not a grid
		if ( grid2->surfaceType != SF_GRID ) continue;
		// grids in the same LOD group should have the exact same lod radius
		if ( grid1->lodRadius != grid2->lodRadius ) continue;
		// grids in the same LOD group should have the exact same lod origin
		if ( grid1->lodOrigin[0] != grid2->lodOrigin[0] ) continue;
		if ( grid1->lodOrigin[1] != grid2->lodOrigin[1] ) continue;
		if ( grid1->lodOrigin[2] != grid2->lodOrigin[2] ) continue;
		//
		while (R_StitchPatches(grid1num, j))
		{
			numstitches++;
		}
	}
	return numstitches;
}


/*
===============
R_StitchAllPatches
===============
*/
static void R_StitchAllPatches( void ) {
	int i, stitched, numstitches;
	srfGridMesh_t *grid1;

	numstitches = 0;
	do
	{
		stitched = qfalse;
		for ( i = 0; i < s_worldData.numsurfaces; i++ ) {
			//
			grid1 = (srfGridMesh_t *) s_worldData.surfaces[i].data;
			// if this surface is not a grid
			if ( grid1->surfaceType != SF_GRID )
				continue;
			//
			if ( grid1->lodStitched )
				continue;
			//
			grid1->lodStitched = qtrue;
			stitched = qtrue;
			//
			numstitches += R_TryStitchingPatch( i );
		}
	}
	while (stitched);
}


/*
===============
R_MovePatchSurfacesToHunk
===============
*/
static void R_MovePatchSurfacesToHunk( void ) {
	int i, j, k, n, size;
	srfGridMesh_t *grid, *hunkgrid;

	for ( i = 0; i < s_worldData.numsurfaces; i++ ) {
		//
		grid = (srfGridMesh_t *) s_worldData.surfaces[i].data;
		// if this surface is not a grid
		if ( grid->surfaceType != SF_GRID )
			continue;
		//
		n = grid->width * grid->height - 1;
		size = n * sizeof( drawVert_t ) + sizeof( *grid );

		for (j = 0; j < n; j++) {
			for (k = 0; k < 3; k++) {
				grid->verts[j].normal[k] = R_ClampDenorm( grid->verts[j].normal[k] );
			}
		}

		hunkgrid = ri.Hunk_Alloc( size, h_low );
		memcpy(hunkgrid, grid, size);

		hunkgrid->widthLodError = ri.Hunk_Alloc( grid->width * 4, h_low );
		memcpy( hunkgrid->widthLodError, grid->widthLodError, grid->width * 4 );

		hunkgrid->heightLodError = ri.Hunk_Alloc( grid->height * 4, h_low );
		memcpy( hunkgrid->heightLodError, grid->heightLodError, grid->height * 4 );

		R_FreeSurfaceGridMesh( grid );

		s_worldData.surfaces[i].data = (void *) hunkgrid;
	}
}


/*
===============
R_LoadSurfaces
===============
*/
static void R_LoadSurfaces( const mapFile_t *bsp ) {
	const dsurface_t *in;
	msurface_t	*out;
	const drawVert_t *dv;
	int			*indexes;
	int			count, totalVerts, totalIndexes;
	int			numFaces, numMeshes, numTriSurfs, numFlares;
	unsigned	firstVert, numVerts, firstIndex, numIndexes;
	int			i;

	numFaces = 0;
	numMeshes = 0;
	numTriSurfs = 0;
	numFlares = 0;

	// Surfaces + draw verts + draw indexes come from the parsed neutral map
	// (every int/float field already Little*-swapped on fill, so no re-swap here
	// nor in the Parse* helpers below). The surface dispatch + patch-stitch +
	// shared-vertex-LOD fixup run as before.
	in = bsp->surfaces;
	count = bsp->numSurfaces;

	dv = bsp->drawVerts;
	totalVerts = bsp->numDrawVerts;

	indexes = bsp->drawIndexes;
	totalIndexes = bsp->numDrawIndexes;

	out = ri.Hunk_Alloc( count * sizeof(*out), h_low );

	s_worldData.surfaces = out;
	s_worldData.numsurfaces = count;

	// shut up compiler
	firstVert = numVerts = 0;
	firstIndex = numIndexes = 0;

	for ( i = 0 ; i < count ; i++, in++, out++ ) {
		unsigned type = in->surfaceType;
		unsigned fogIndex;

		if ( type != MST_FLARE ) {
			firstVert = in->firstVert;
			if ( type == MST_PATCH )
				numVerts = 0;	// use patch size
			else
				numVerts = in->numVerts;
			if ( (uint64_t)firstVert + numVerts > totalVerts )
				ri.Terminate( TERM_CLIENT_DROP, "%s: bad verts", __func__ );

			if ( type != MST_PATCH ) {
				firstIndex = in->firstIndex;
				numIndexes = in->numIndexes;
				if ( (uint64_t)firstIndex + numIndexes > totalIndexes )
					ri.Terminate( TERM_CLIENT_DROP, "%s: bad indexes", __func__ );

				// don't allow partial triangles
				if ( numIndexes % 3 ) {
					R_LOG( rch_assets, SEV_WARN, "%s: odd number of indexes: %u\n", __func__, numIndexes );
					numIndexes -= numIndexes % 3;
				}
			}
		}

		// get fog volume
		fogIndex = in->fogNum + 1U;
		if ( fogIndex >= s_worldData.numfogs ) {
			if ( type != MST_FLARE )
				R_LOG( rch_assets, SEV_WARN, "%s: bad fog index: %u\n", __func__, fogIndex );
			fogIndex = 0;
		}
		out->fogIndex = fogIndex;

		switch ( type ) {
		case MST_PATCH:
			ParseMesh( in, dv + firstVert, totalVerts - firstVert, out );
			numMeshes++;
			break;
		case MST_TRIANGLE_SOUP:
			ParseTriSurf( in, dv + firstVert, numVerts, out, indexes + firstIndex, numIndexes );
			numTriSurfs++;
			break;
		case MST_PLANAR:
			ParseFace( in, dv + firstVert, numVerts, out, indexes + firstIndex, numIndexes,
				lightstyleData ? lightstyleData + firstVert * 4 : NULL );
			numFaces++;
			break;
		case MST_FLARE:
			ParseFlare( in, out );
			numFlares++;
			break;
		default:
			ri.Terminate( TERM_CLIENT_DROP, "%s: bad surfaceType: %u", __func__, type );
		}
	}

#ifdef PATCH_STITCHING
	R_StitchAllPatches();
#endif

	R_FixSharedVertexLodError();

#ifdef PATCH_STITCHING
	R_MovePatchSurfacesToHunk();
#endif

}


/*
R_LoadSubmodels
=================
*/
static void R_LoadSubmodels( const mapFile_t *bsp ) {
	const dmodel_t *in;
	bmodel_t	*out;
	int			i, j, count;
	unsigned	firstSurface, numSurfaces;

	// Submodels come from the parsed neutral map (mins/maxs already LittleFloat,
	// firstSurface/numSurfaces already LittleLong on fill — no re-swap here). The
	// model alloc + bounds/surface-pointer fixup runs as before.
	in = bsp->subModels;
	count = bsp->numSubModels;

	s_worldData.numBModels = count;
	s_worldData.bmodels = out = ri.Hunk_Alloc( count * sizeof(*out), h_low );

	for ( i=0 ; i<count ; i++, in++, out++ ) {
		model_t *model;

		model = R_AllocModel();

		if ( model == NULL ) {
			ri.Terminate( TERM_CLIENT_DROP, "R_LoadSubmodels: R_AllocModel() failed" );
		}

		model->type = MOD_BRUSH;
		model->bmodel = out;
		Com_sprintf( model->name, sizeof( model->name ), "*%d", i );

		for (j=0 ; j<3 ; j++) {
			out->bounds[0][j] = in->mins[j];
			out->bounds[1][j] = in->maxs[j];
		}

		firstSurface = in->firstSurface;
		numSurfaces = in->numSurfaces;
		if ( (uint64_t)firstSurface + numSurfaces > s_worldData.numsurfaces ) {
			ri.Terminate( TERM_CLIENT_DROP, "%s: bad surfaces", __func__ );
		}

		out->firstSurface = s_worldData.surfaces + firstSurface;
		out->numSurfaces = numSurfaces;

		R_LOG( rch_assets, SEV_TRACE,
		        "R_LoadSubmodels VK: bmodel[%d] firstSurface=%u numSurfaces=%u"
		        " bounds=(%.0f,%.0f,%.0f)-(%.0f,%.0f,%.0f)\n",
		        i, firstSurface, numSurfaces,
		        out->bounds[0][0], out->bounds[0][1], out->bounds[0][2],
		        out->bounds[1][0], out->bounds[1][1], out->bounds[1][2] );
	}
}



//==================================================================

/*
=================
R_SetParent
=================
*/
static void R_SetParent( mnode_t *node, mnode_t *parent )
{
	if ( node->parent )
		ri.Terminate( TERM_CLIENT_DROP, "%s: cycle encountered", __func__ );
	node->parent = parent;
	if ( node->contents != CONTENTS_NODE )
		return;
	R_SetParent( node->children[0], node );
	R_SetParent( node->children[1], node );
}


/*
=================
R_LoadNodesAndLeafs
=================
*/
static void R_LoadNodesAndLeafs( const mapFile_t *bsp ) {
	int			i, j;
	unsigned	p, firstmarksurface, nummarksurfaces;
	const dnode_t		*in;
	const dleaf_t		*inLeaf;
	mnode_t 	*out;
	int			numNodes, numLeafs;

	// Nodes + leafs come from the parsed neutral map (every int field already
	// LittleLong on fill, so no re-swap here). The interleave into one mnode_t
	// array, the CONTENTS_NODE tagging, the sign-bit leaf decode, the pointer
	// fixups, and R_SetParent all run as before.
	in = bsp->nodes;
	numNodes = bsp->numNodes;
	numLeafs = bsp->numLeafs;

	out = ri.Hunk_Alloc ( (numNodes + numLeafs) * sizeof(*out), h_low);

	s_worldData.nodes = out;
	s_worldData.numnodes = numNodes + numLeafs;
	s_worldData.numDecisionNodes = numNodes;

	// load nodes
	for ( i=0 ; i<numNodes; i++, in++, out++)
	{
		for (j=0 ; j<3 ; j++)
		{
			out->mins[j] = in->mins[j];
			out->maxs[j] = in->maxs[j];
		}

		p = in->planeNum;
		if ( p >= s_worldData.numplanes ) {
			ri.Terminate( TERM_CLIENT_DROP, "%s: bad planeNum", __func__ );
		}
		out->plane = s_worldData.planes + p;

		out->contents = CONTENTS_NODE;	// differentiate from leafs

		for (j=0 ; j<2 ; j++)
		{
			p = in->children[j];
			if (p & 0x80000000) {
				p = ~p;
				if ( p >= numLeafs ) {
					ri.Terminate( TERM_CLIENT_DROP, "%s: bad leaf", __func__ );
				}
				out->children[j] = s_worldData.nodes + numNodes + p;
			} else {
				if ( p >= numNodes ) {
					ri.Terminate( TERM_CLIENT_DROP, "%s: bad node", __func__ );
				}
				out->children[j] = s_worldData.nodes + p;
			}
		}
	}

	// load leafs
	inLeaf = bsp->leafs;
	for ( i=0 ; i<numLeafs ; i++, inLeaf++, out++)
	{
		for (j=0 ; j<3 ; j++)
		{
			out->mins[j] = inLeaf->mins[j];
			out->maxs[j] = inLeaf->maxs[j];
		}

		out->cluster = inLeaf->cluster;
		if ( out->cluster + 1U > INT_MAX - 63U )
			Com_Terminate( TERM_CLIENT_DROP, "%s: bad cluster", __func__ );

		out->area = inLeaf->area;
		if ( out->area + 1U > MAX_MAP_AREAS )
			Com_Terminate( TERM_CLIENT_DROP, "%s: bad area", __func__ );

		if ( out->cluster >= s_worldData.numClusters ) {
			s_worldData.numClusters = out->cluster + 1;
		}

		firstmarksurface = inLeaf->firstLeafSurface;
		nummarksurfaces = inLeaf->numLeafSurfaces;
		if ( (uint64_t)firstmarksurface + nummarksurfaces > s_worldData.nummarksurfaces ) {
			ri.Terminate( TERM_CLIENT_DROP, "%s: bad marksurfaces", __func__ );
		}

		out->firstmarksurface = s_worldData.marksurfaces + firstmarksurface;
		out->nummarksurfaces = nummarksurfaces;
	}

	// chain descendants
	R_SetParent (s_worldData.nodes, NULL);
}

//=============================================================================


/*
=================
R_ReplaceShaders

replaces some buggy map shaders
=================
*/
static void R_ReplaceMapShaders( dshader_t *out, int count )
{
	if ( Q_stricmp( s_worldData.baseName, "mapel4b" ) == 0 && count == 86 ) {
		if ( crc32_buffer( (const byte*)out, count*sizeof(*out) ) == 0x1593623C ) {
			if ( strcmp( out[72].shader, "textures/mapel4/crate1_top3" ) == 0 ) {
				strcpy( out[72].shader, "textures/mapel4/crate1_top2" );
			}
		}
	}
}


/*
=================
R_LoadShaders
=================
*/
static void R_LoadShaders( const mapFile_t *bsp ) {
	int		count;
	const dshader_t	*in;
	dshader_t	*out;

	// Shaders come straight from the parsed neutral map: the format loader
	// already byte-swapped surfaceFlags/contentFlags on fill, so no swap is
	// repeated here (doing so would double-swap on a big-endian host).
	in = bsp->shaders;
	count = bsp->numShaders;
	out = ri.Hunk_Alloc ( count*sizeof(*out), h_low );

	s_worldData.shaders = out;
	s_worldData.numShaders = count;

	memcpy( out, in, count*sizeof(*out) );

	R_ReplaceMapShaders( out, count );
}


/*
=================
R_LoadMarksurfaces
=================
*/
static void R_LoadMarksurfaces( const mapFile_t *bsp )
{
	int		i, count;
	unsigned	j;
	const int	*in;
	msurface_t **out;

	// Leaf-surface indices come from the parsed neutral map (already
	// byte-swapped on fill, so no LittleLong is repeated here). Only the
	// index→surface-pointer fixup + bounds check runs.
	in = bsp->leafSurfaces;
	count = bsp->numLeafSurfaces;
	out = ri.Hunk_Alloc ( count*sizeof(*out), h_low);

	s_worldData.marksurfaces = out;
	s_worldData.nummarksurfaces = count;

	for ( i=0 ; i<count ; i++)
	{
		j = (unsigned)in[i];
		if ( j >= s_worldData.numsurfaces ) {
			ri.Terminate( TERM_CLIENT_DROP, "%s: bad surface", __func__ );
		}
		out[i] = s_worldData.surfaces + j;
	}
}


/*
=================
R_LoadPlanes
=================
*/
static	void R_LoadPlanes( const mapFile_t *bsp ) {
	int			i, j;
	cplane_t	*out;
	const dplane_t 	*in;
	int			count;
	int			bits;

	// Planes come from the parsed neutral map (already byte-swapped on fill, so
	// no LittleFloat is repeated here). The dplane_t -> cplane_t conversion that
	// derives type/signbits still runs.
	in = bsp->planes;
	count = bsp->numPlanes;
	out = ri.Hunk_Alloc( count*2*sizeof(*out), h_low );

	s_worldData.planes = out;
	s_worldData.numplanes = count;

	for ( i=0 ; i<count ; i++, in++, out++) {
		bits = 0;
		for (j=0 ; j<3 ; j++) {
			out->normal[j] = in->normal[j];
			if (out->normal[j] < 0) {
				bits |= 1<<j;
			}
		}

		out->dist = in->dist;
		out->type = PlaneTypeForNormal( out->normal );
		out->signbits = bits;
	}
}


/*
=================
R_PreLoadFogs
=================
*/
static void R_PreLoadFogs( const mapFile_t *bsp ) {
	// The neutral map already carries the validated fog count (the format loader
	// computed filelen/sizeof(dfog_t)); read it directly. tr.numFogs lets shaders
	// size their fog tables before R_LoadFogs runs.
	tr.numFogs = bsp->numFogs;
}


/*
=================
R_LoadFogs
=================
*/
static void R_LoadFogs( const mapFile_t *bsp ) {
	int			i, j, n;
	fog_t		*out;
	const dfog_t		*fogs;
	const dbrush_t 	*brushes, *brush;
	const dbrushside_t	*sides;
	int			count, brushesCount, sidesCount;
	unsigned	sideNum, planeNum, firstSide;
	shader_t	*shader;
	float		d;
	vec3_t		fogColor;

	// Fogs + the brushes/brushSides they reference all come from the parsed
	// neutral map (brushNum/firstSide/planeNum/visibleSide already LittleLong on
	// fill — no re-swap here). The 3-lump cross-reference + shader-derived fog
	// params run unchanged.
	fogs = bsp->fogs;
	count = bsp->numFogs;

	// create fog structures for them
	s_worldData.numfogs = count + 1;
	s_worldData.fogs = ri.Hunk_Alloc( s_worldData.numfogs*sizeof(*out), h_low);
	out = s_worldData.fogs + 1;

	if ( !count ) {
		return;
	}

	brushes = bsp->brushes;
	brushesCount = bsp->numBrushes;

	sides = bsp->brushSides;
	sidesCount = bsp->numBrushSides;
	if (sidesCount < 6) {
		ri.Terminate( TERM_CLIENT_DROP, "%s(): sides lump too short in %s", __func__, s_worldData.name );
	}

	for ( i=0 ; i<count ; i++, fogs++) {
		out->originalBrushNumber = fogs->brushNum;

		if ( (unsigned)out->originalBrushNumber >= brushesCount ) {
			ri.Terminate( TERM_CLIENT_DROP, "fog brushNumber out of range" );
		}
		brush = brushes + out->originalBrushNumber;

		firstSide = brush->firstSide;

		if ( firstSide > sidesCount - 6 ) {
			ri.Terminate( TERM_CLIENT_DROP, "fog brush sideNumber out of range" );
		}

		// brushes are always sorted with the axial sides first
		for ( j = 0; j < 6; j++ ) {
			sideNum = firstSide + j;
			planeNum = sides[ sideNum ].planeNum;
			if ( planeNum >= s_worldData.numplanes ) {
				ri.Terminate( TERM_CLIENT_DROP, "fog brush planeNum out of range" );
			}
			d = s_worldData.planes[ planeNum ].dist;
			out->bounds[j & 1][j >> 1] = (j & 1) ? d : -d;
		}

		// get information from the shader for fog parameters
		shader = R_FindShader( fogs->shader, LIGHTMAP_NONE, qtrue );

		VectorCopy( shader->fogParms.color, fogColor );

		if ( r_mapSaturation->value != 1.0f ) {
			// Block 4 (colour closure): fogParms.color is float display-
			// domain (sRGB-encoded, 0..1). Desaturating it in display
			// domain shifts the result towards a too-bright grey because
			// the sRGB curve compresses midtones. Do it in linear:
			// decode → mix toward linear luma → re-encode. Matches the
			// r_mapSaturation handling for image texels (tr_image.c
			// R_FindImageFile — migrated to linear in Block 7) and
			// fogColor for the in-shader fog (tr_shade.c R_SRGBToLinear).
			const float sat = r_mapSaturation->value;
			vec3_t lin;
			float luma;
			lin[0] = R_SRGBToLinear( fogColor[0] );
			lin[1] = R_SRGBToLinear( fogColor[1] );
			lin[2] = R_SRGBToLinear( fogColor[2] );
			luma = LUMA( lin[0], lin[1], lin[2] );
			lin[0] = luma + sat * ( lin[0] - luma );
			lin[1] = luma + sat * ( lin[1] - luma );
			lin[2] = luma + sat * ( lin[2] - luma );
			// R_LinearToSRGB clamps its input to >= 0; the float→byte
			// cast below clamps the high end.
			fogColor[0] = R_LinearToSRGB( lin[0] );
			fogColor[1] = R_LinearToSRGB( lin[1] );
			fogColor[2] = R_LinearToSRGB( lin[2] );
		}

		out->parms = shader->fogParms;

		if ( out->parms.type != FT_NONE ) {
			R_LOG( rch_assets, SEV_INFO,
				"Advanced fog volume: shader=%s type=%d density=%g farClip=%g depthForOpaque=%g\n",
				fogs->shader, (int)out->parms.type, out->parms.density,
				out->parms.farClip, out->parms.depthForOpaque );
		}

		/* dropped `* tr.identityLight` halving from
		 * fog vertex-color bake. Linear pipeline. */
		out->colorInt.rgba[0] = fogColor[0] * 255.0f;
		out->colorInt.rgba[1] = fogColor[1] * 255.0f;
		out->colorInt.rgba[2] = fogColor[2] * 255.0f;
		out->colorInt.rgba[3] = 255;

		for ( n = 0; n < 4; n++ )
			out->color[ n ] = (float) out->colorInt.rgba[ n ] / 255.0f;

		d = shader->fogParms.depthForOpaque < 1 ? 1 : shader->fogParms.depthForOpaque;
		out->tcScale = 1.0f / ( d * 8 );

		// set the gradient vector
		sideNum = fogs->visibleSide;

		if ( sideNum == -1 ) {
			out->hasSurface = qfalse;
		} else {
			if ( sideNum >= sidesCount - firstSide ) {
				R_LOG( rch_assets, SEV_WARN, "bad fog side offset\n" );
				out->hasSurface = qfalse;
			} else {
				out->hasSurface = qtrue;
				planeNum = sides[ firstSide + sideNum ].planeNum;
				if ( planeNum >= s_worldData.numplanes ) {
					ri.Terminate( TERM_CLIENT_DROP, "fog brush planeNum out of range" );
				}
				VectorSubtract( vec3_origin, s_worldData.planes[ planeNum ].normal, out->surface );
				out->surface[3] = -s_worldData.planes[ planeNum ].dist;
			}
		}

		out++;
	}

}


/*
================
R_LoadLightGrid
================
*/
static void R_LoadLightGrid( const mapFile_t *bsp ) {
	int		i;
	vec3_t	maxs;
	unsigned bounds[3];
	unsigned numGridPoints;
	world_t	*w;
	float	*wMins, *wMaxs;
	int		gridBytes;

	w = &s_worldData;

	// The neutral map carries the grid-point count + raw byte data (pure RGB
	// bytes, no byte-swap). The lump byte length is numGridPoints * 8.
	gridBytes = bsp->numGridPoints * 8;

	if ( !gridBytes ) {
		w->lightGridData = NULL;
		return;
	}

	w->lightGridInverseSize[0] = 1.0f / w->lightGridSize[0];
	w->lightGridInverseSize[1] = 1.0f / w->lightGridSize[1];
	w->lightGridInverseSize[2] = 1.0f / w->lightGridSize[2];

	wMins = w->bmodels[0].bounds[0];
	wMaxs = w->bmodels[0].bounds[1];

	for ( i = 0 ; i < 3 ; i++ ) {
		w->lightGridOrigin[i] = w->lightGridSize[i] * ceil( wMins[i] / w->lightGridSize[i] );
		maxs[i] = w->lightGridSize[i] * floor( wMaxs[i] / w->lightGridSize[i] );
		bounds[i] = (maxs[i] - w->lightGridOrigin[i])/w->lightGridSize[i] + 1;
	}

	if ( (uint64_t)bounds[0] * bounds[1] > INT_MAX ||
		 (uint64_t)bounds[0] * bounds[1] * bounds[2] > INT_MAX) {
		R_LOG( rch_assets, SEV_WARN, "WARNING: bad light grid bounds\n" );
		w->lightGridData = NULL;
		return;
	}

	numGridPoints = bounds[0] * bounds[1] * bounds[2];

	if ( gridBytes % 8 || gridBytes / 8 != numGridPoints ) {
		R_LOG( rch_assets, SEV_WARN, "WARNING: light grid mismatch\n" );
		w->lightGridData = NULL;
		return;
	}

	for ( i = 0 ; i < 3 ; i++ ) {
		w->lightGridBounds[i] = bounds[i];
	}

	w->lightGridData = ri.Hunk_Alloc( gridBytes, h_low );
	memcpy( w->lightGridData, bsp->lightGridData, gridBytes );

	// deal with overbright bits
	for ( i = 0 ; i < numGridPoints ; i++ ) {
		R_ColorShiftLightingBytes( &w->lightGridData[i*8], &w->lightGridData[i*8], qfalse, qfalse );
		R_ColorShiftLightingBytes( &w->lightGridData[i*8+3], &w->lightGridData[i*8+3], qfalse, qfalse );
	}
}


/*
================
R_LoadEntities
================
*/
static void R_LoadEntities( const mapFile_t *bsp ) {
	const char *p, *token, *s;
	char keyname[MAX_TOKEN_CHARS];
	char value[MAX_TOKEN_CHARS], *v[3];
	world_t	*w;
	ComParser parser = { 0 };
	int		entLen;

	w = &s_worldData;
	w->lightGridSize[0] = 64;
	w->lightGridSize[1] = 64;
	w->lightGridSize[2] = 128;

	// store for reference by the cgame. The entity string comes from the parsed
	// neutral map (plain ASCII, no byte-swap). Copy into the renderer's own hunk
	// buffer + NUL exactly as the raw-lump path did.
	entLen = bsp->entityStringLength;
	w->entityString = ri.Hunk_Alloc( entLen + 1, h_low );
	memcpy( w->entityString, bsp->entityString, entLen );
	w->entityString[entLen] = 0;
	w->entityParsePoint = w->entityString;

	p = w->entityString;
	token = COM_ParseExt( &parser, &p, qtrue );
	if (*token != '{') {
		return;
	}

	// only parse the world spawn
	while ( 1 ) {
		// parse key
		token = COM_ParseExt( &parser, &p, qtrue );

		if ( !*token || *token == '}' ) {
			break;
		}
		Q_strncpyz(keyname, token, sizeof(keyname));

		// parse value
		token = COM_ParseExt( &parser, &p, qtrue );

		if ( !*token || *token == '}' ) {
			break;
		}
		Q_strncpyz(value, token, sizeof(value));

		// check for remapping of shaders for vertex lighting
		s = "vertexremapshader";
		if (!strncmp(keyname, s, strlen(s)) ) {
			char *vs = strchr(value, ';');
			if (!vs) {
				R_LOG( rch_assets, SEV_WARN, "WARNING: no semi colon in vertexshaderremap '%s'\n", value );
				break;
			}
			*vs++ = '\0';
			if ( r_vertexLight->integer && tr.vertexLightingAllowed ) {
				RE_RemapShader(value, s, "0");
			}
			continue;
		}
		// check for remapping of shaders
		s = "remapshader";
		if (!strncmp(keyname, s, (int)strlen(s)) ) {
			char *vs = strchr(value, ';');
			if (!vs) {
				R_LOG( rch_assets, SEV_WARN, "WARNING: no semi colon in shaderremap '%s'\n", value );
				break;
			}
			*vs++ = '\0';
			RE_RemapShader(value, s, "0");
			continue;
		}
		// check for a different grid size
		if (!Q_stricmp(keyname, "gridsize")) {
			//sscanf(value, "%f %f %f", &w->lightGridSize[0], &w->lightGridSize[1], &w->lightGridSize[2] );
			Com_Split( value, v, 3, ' ' );
			w->lightGridSize[0] = Q_atof( v[0] );
			w->lightGridSize[1] = Q_atof( v[1] );
			w->lightGridSize[2] = Q_atof( v[2] );
			continue;
		}
		// the legacy `_q3map2_cmdline`
		// per-map linear-colorspace opt-in (Daemon `-sRGB*` convention)
		// is retired — the engine renders self-consistently linear
		// end-to-end now, so there is no per-map regime to derive. The
		// key is left unhandled (silently ignored, like any other
		// unrecognised worldspawn key).
	}
}


/*
================
R_ExtractStaticLights

Second pass over the world entity string (R_LoadEntities parses only worldspawn):
pull every non-spot `classname light` entity into s_worldData.staticLights so the
Forward+ tile compute can light with them (bypassing the 32-dlight PMLIGHT limit —
these ride the fp SSBO only, never backEndData->dlights). Gated on
r_unbakeStaticLights: when off, this returns with numStaticLights == 0 so the world
load is byte-identical.

  - "light N": q3map2 lightmap-emission intensity, NOT a distance. Inverse-square
    physics → radius ∝ sqrt(N). We store radius = clamp(MIN, K*sqrt(N), MAX) so the
    submitted reach lands in the runtime-dlight vocabulary (K=32 → light 100 ≈ 320
    submit ≈ the muzzle/missile band). A raw "light N" as radius would make dim fills
    (N=5) 5-unit dots.
  - "_color r g b": 3 floats 0..1, sRGB — stored raw; the fp upload sRGB-decodes.
  - a "target" key = a spotlight (aimed cone); the fp light record has no cone, so
    emitting it as omni-at-origin would spray light the real cone never touches → we
    SKIP targeted lights (they stay correct in the baked lightmap).
  - staticLightDim: the tuned lightmap-boost multiplier that bounds the additive
    double-count, derived from the extracted set's mean intensity (a global scalar
    can only approximate a spatially-varying bake — see the dim note below).
================
*/

// light-value -> Forward+ submitted radius (world units). See the header comment.
#define STATICLIGHT_RADIUS_K    32.0f    // light=100 -> 320 submit (~muzzle/missile band)
#define STATICLIGHT_RADIUS_MIN  64.0f    // dimmest fills still light a small pool, not a dot
#define STATICLIGHT_RADIUS_MAX  400.0f   // cap sky/sun lights below the muzzle/missile ceiling

// Lightmap-dim derivation. A single global dim cannot correctly compensate a
// spatially-local double-count (near a light it should be low, in gaps ~1.0); this
// mean-intensity heuristic sits between those and is deliberately conservative
// (FLOOR keeps it from murking the whole level). The value is ratification-gated —
// the ON look is an accepted approximation, tunable via these constants.
#define STATICLIGHT_DIM_K       0.5f     // how hard mean intensity pushes the dim
#define STATICLIGHT_DIM_REF     100.0f   // the "normal light" intensity anchor
#define STATICLIGHT_DIM_FLOOR   0.4f     // never dim the world past this

static void R_ExtractStaticLights( void ) {
	world_t		*w = &s_worldData;
	const char	*p;
	ComParser	parser = { 0 };
	const char	*token;
	int			pass;
	int			count;
	float		intensitySum;

	w->numStaticLights = 0;
	w->staticLights    = NULL;
	w->staticLightDim  = 1.0f;

	if ( !r_unbakeStaticLights || r_unbakeStaticLights->integer == 0 )
		return;   // off → byte-identical (no extraction, no dim)
	if ( !w->entityString || !w->entityString[0] )
		return;

	// Two passes over the SAME entity string: pass 0 counts matching lights so the
	// array can be hunk-sized exactly; pass 1 fills it. A private cursor is used both
	// times (entityParsePoint is left for the cgame's own lens-flare walk).
	for ( pass = 0; pass < 2; pass++ ) {
		qboolean	inEntity = qfalse;
		qboolean	isLight  = qfalse;
		qboolean	hasTarget = qfalse;
		vec3_t		origin;
		vec3_t		color;
		float		lightVal;

		count = 0;
		intensitySum = 0.0f;
		VectorClear( origin );
		VectorSet( color, 1.0f, 1.0f, 1.0f );
		lightVal = 300.0f;

		p = w->entityString;
		memset( &parser, 0, sizeof( parser ) );

		while ( 1 ) {
			char keyname[MAX_TOKEN_CHARS];

			token = COM_ParseExt( &parser, &p, qtrue );
			if ( !token[0] )
				break;

			if ( token[0] == '{' ) {
				inEntity  = qtrue;
				isLight   = qfalse;
				hasTarget = qfalse;
				VectorClear( origin );
				VectorSet( color, 1.0f, 1.0f, 1.0f );
				lightVal = 300.0f;
				continue;
			}

			if ( token[0] == '}' ) {
				if ( inEntity && isLight && !hasTarget && lightVal > 0.0f ) {
					if ( pass == 1 && count < w->numStaticLights ) {
						staticLight_t *sl = &w->staticLights[ count ];
						float radius = STATICLIGHT_RADIUS_K * sqrtf( lightVal );
						if ( radius < STATICLIGHT_RADIUS_MIN ) radius = STATICLIGHT_RADIUS_MIN;
						if ( radius > STATICLIGHT_RADIUS_MAX ) radius = STATICLIGHT_RADIUS_MAX;
						VectorCopy( origin, sl->origin );
						VectorCopy( origin, sl->origin2 );   // point light, not a tube
						VectorCopy( color, sl->color );       // raw sRGB 0-1
						sl->radius = radius;
						sl->linear = 0;
					}
					count++;
					intensitySum += lightVal;
				}
				inEntity = qfalse;
				continue;
			}

			if ( !inEntity )
				continue;

			// key -> value
			Q_strncpyz( keyname, token, sizeof( keyname ) );
			token = COM_ParseExt( &parser, &p, qtrue );
			if ( !token[0] )
				break;

			if ( !Q_stricmp( keyname, "classname" ) ) {
				if ( !Q_stricmp( token, "light" ) )
					isLight = qtrue;
			} else if ( !Q_stricmp( keyname, "origin" ) ) {
				sscanf( token, "%f %f %f", &origin[0], &origin[1], &origin[2] );
			} else if ( !Q_stricmp( keyname, "light" ) ) {
				lightVal = atof( token );
			} else if ( !Q_stricmp( keyname, "target" ) ) {
				hasTarget = qtrue;   // spotlight → already baked, skip
			} else if ( !Q_stricmp( keyname, "_color" ) || !Q_stricmp( keyname, "color" ) ) {
				if ( sscanf( token, "%f %f %f", &color[0], &color[1], &color[2] ) == 3 ) {
					// tolerate a 0..255 map (arena1 is 0..1); harmless when already 0..1
					float mx = color[0];
					if ( color[1] > mx ) mx = color[1];
					if ( color[2] > mx ) mx = color[2];
					if ( mx > 1.0f ) { color[0] /= 255.0f; color[1] /= 255.0f; color[2] /= 255.0f; }
				} else {
					VectorSet( color, 1.0f, 1.0f, 1.0f );
				}
			}
		}

		if ( pass == 0 ) {
			if ( count <= 0 )
				return;   // no static lights → nothing to allocate, dim stays 1.0
			w->numStaticLights = count;
			w->staticLights = ri.Hunk_Alloc( count * sizeof( staticLight_t ), h_low );
		}
	}

	// Tuned lightmap dim: proportional to the extracted set's mean intensity. Global
	// scalar → an approximation of a per-luxel un-bake (see the constants above).
	if ( w->numStaticLights > 0 ) {
		float mean = intensitySum / (float)w->numStaticLights;
		float dim  = 1.0f - STATICLIGHT_DIM_K * ( mean / STATICLIGHT_DIM_REF );
		if ( dim < STATICLIGHT_DIM_FLOOR ) dim = STATICLIGHT_DIM_FLOOR;
		if ( dim > 1.0f ) dim = 1.0f;
		w->staticLightDim = dim;
	}

	R_LOG( rch_assets, SEV_INFO, "static lights: extracted %d light(s), lightmap dim %.2f\n",
		w->numStaticLights, w->staticLightDim );
}


/*
================
R_BuildStaticLightClusters

Bin the extracted static lights into a WORLD-SPACE uniform 3D cluster grid, ONCE at
map load. A fragment later gathers its static-light set from the grid cell its WORLD
position falls in — so the set depends only on world geometry, never the camera. This
removes static lights from the per-frame screen-tile Forward+ cull (whose rotating
frustum + 32-light cap dropped a view-dependent subset of static lights → the
"flashlight"). View-independent by construction.

Each light is binned into EVERY cell its bounding sphere overlaps (so the fragment
reads only its own cell). The flat GPU layout is [count, idx0..] per cell (stride
FP_CLUSTER_STRIDE), storing ABSOLUTE dlights[] slots (FP_STATIC_LIGHT_BASE + k).
Cell size auto-coarsens so the grid never exceeds FP_CLUSTER_MAX_CELLS.
================
*/
static void R_BuildStaticLightClusters( world_t *w ) {
	vec3_t	worldMins, worldMaxs, ext;
	float	cell;
	int		dims[3], numCells, i, k, a;
	int		nStatic;
	uint32_t *counts;   // scratch: per-cell fill count (also the emitted count, capped)
	int64_t	cellsProduct;

	w->numClusterCells = 0;
	w->clusterFlat     = NULL;
	w->clusterCellSize = FP_CLUSTER_CELLSIZE;

	nStatic = w->numStaticLights;
	if ( nStatic <= 0 )
		return;
	if ( nStatic > FP_MAX_STATIC_LIGHTS )
		nStatic = FP_MAX_STATIC_LIGHTS;   // slots beyond the reserved region can't be addressed

	// World AABB from the worldspawn bmodel (submodel 0), populated by R_LoadSubmodels.
	VectorCopy( w->bmodels[0].bounds[0], worldMins );
	VectorCopy( w->bmodels[0].bounds[1], worldMaxs );
	VectorSubtract( worldMaxs, worldMins, ext );
	if ( ext[0] <= 0.0f || ext[1] <= 0.0f || ext[2] <= 0.0f )
		return;   // degenerate bounds — no grid

	// Choose a cell size that keeps the grid under the budget (coarsen if needed).
	cell = FP_CLUSTER_CELLSIZE;
	for ( a = 0; a < 12; a++ ) {
		for ( i = 0; i < 3; i++ )
			dims[i] = (int)ceilf( ext[i] / cell ) + 1;
		cellsProduct = (int64_t)dims[0] * dims[1] * dims[2];
		if ( cellsProduct > 0 && cellsProduct <= FP_CLUSTER_MAX_CELLS )
			break;
		cell *= 2.0f;   // too many cells → coarsen and retry
	}
	if ( cellsProduct <= 0 || cellsProduct > FP_CLUSTER_MAX_CELLS ) {
		R_LOG( rch_assets, SEV_WARN, "static clusters: world too large for grid budget (%lld cells) — static lights disabled for this map\n",
			(long long)cellsProduct );
		return;   // leave grid empty; the fragment sees no static lights (never falls back to the view-dependent path)
	}
	numCells = (int)cellsProduct;

	VectorCopy( worldMins, w->clusterGridOrigin );
	w->clusterGridDims[0] = dims[0];
	w->clusterGridDims[1] = dims[1];
	w->clusterGridDims[2] = dims[2];
	w->clusterCellSize    = cell;
	w->numClusterCells    = numCells;

	// The flat GPU buffer: [count, idx0..idx31] per cell, zero-initialized (count=0).
	w->clusterFlat = ri.Hunk_Alloc( (size_t)numCells * FP_CLUSTER_STRIDE * sizeof( uint32_t ), h_low );
	counts = ri.Hunk_AllocateTempMemory( (size_t)numCells * sizeof( uint32_t ) );
	memset( counts, 0, (size_t)numCells * sizeof( uint32_t ) );

	// Bin each static light into every cell its sphere overlaps (clamped to the grid).
	for ( k = 0; k < nStatic; k++ ) {
		const staticLight_t *sl = &w->staticLights[k];
		int lo[3], hi[3], cx, cy, cz;
		uint32_t slot = (uint32_t)( FP_STATIC_LIGHT_BASE + k );
		for ( i = 0; i < 3; i++ ) {
			float mn = sl->origin[i] - sl->radius - worldMins[i];
			float mx = sl->origin[i] + sl->radius - worldMins[i];
			lo[i] = (int)floorf( mn / cell );
			hi[i] = (int)floorf( mx / cell );
			if ( lo[i] < 0 ) lo[i] = 0;
			if ( hi[i] > dims[i] - 1 ) hi[i] = dims[i] - 1;
		}
		for ( cz = lo[2]; cz <= hi[2]; cz++ ) {
			for ( cy = lo[1]; cy <= hi[1]; cy++ ) {
				for ( cx = lo[0]; cx <= hi[0]; cx++ ) {
					int ci = ( cz * dims[1] + cy ) * dims[0] + cx;
					uint32_t c = counts[ci];
					if ( c >= (uint32_t)FP_MAX_PER_CLUSTER )
						continue;   // cell full — drop (deterministic, view-independent)
					w->clusterFlat[ (size_t)ci * FP_CLUSTER_STRIDE + 1 + c ] = slot;
					counts[ci] = c + 1;
				}
			}
		}
	}
	// Stamp the per-cell counts into the flat buffer header word.
	for ( i = 0; i < numCells; i++ )
		w->clusterFlat[ (size_t)i * FP_CLUSTER_STRIDE ] = counts[i];

	ri.Hunk_FreeTempMemory( counts );

	R_LOG( rch_assets, SEV_INFO, "static clusters: %dx%dx%d grid (%d cells, %.0fu), %d light(s) binned\n",
		dims[0], dims[1], dims[2], numCells, cell, nStatic );
}


/*
=================
RE_GetEntityToken
=================
*/
qboolean RE_GetEntityToken( char *buffer, int size ) {
	const char	*s;
	ComParser   parser = { 0 };

	// Read the active world's entity string through tr.world (the live world slot)
	// rather than a fixed storage struct — this runs post-load, when tr.world
	// already points at the world being queried.
	if ( !tr.world ) {
		buffer[0] = '\0';
		return qfalse;
	}
	s = COM_Parse( &parser, &tr.world->entityParsePoint );
	Q_strncpyz( buffer, s, size );
	if ( !tr.world->entityParsePoint && !s[0] ) {
		tr.world->entityParsePoint = tr.world->entityString;
		return qfalse;
	}
	return qtrue;
}


/*
=================
R_SetWorldSlot

Point tr.world at the given app's world slot for the render that follows. A slot
is "loaded" once RE_LoadWorldMap has published into it (its name is non-empty).
Selecting an unloaded slot leaves tr.world NULL — the worldless-scene path. Single
app: only slot 0 is ever loaded, and tr.world already points there.
=================
*/
void R_SetWorldSlot( int worldIndex ) {
	if ( worldIndex < 0 || worldIndex >= MAX_RENDER_WORLDS )
		worldIndex = 0;
	if ( s_activeWorldIndex >= 0 && s_activeWorldIndex != worldIndex )
		R_SaveWorldMapState( s_activeWorldIndex );
	if ( R_WorldSlotResident( worldIndex ) ) {
		s_activeWorldIndex = worldIndex;
		tr.world = &s_worldDataSlots[worldIndex];
		R_ApplyWorldMapState( worldIndex );
	} else {
		s_activeWorldIndex = -1;
		tr.world = NULL;
	}
	tr.worldMapLoaded = R_ResidentWorldCount() > 0 ? qtrue : qfalse;
}

qboolean R_WorldSlotResident( int worldIndex ) {
	return worldIndex >= 0 && worldIndex < MAX_RENDER_WORLDS
		&& s_worldDataSlots[worldIndex].name[0] != '\0' ? qtrue : qfalse;
}

int R_ResidentWorldCount( void ) {
	int count = 0;
	for ( int worldIndex = 0; worldIndex < MAX_RENDER_WORLDS; ++worldIndex )
		if ( R_WorldSlotResident( worldIndex ) ) count++;
	return count;
}

qboolean R_UnloadWorldSlot( int worldIndex ) {
	if ( worldIndex < 0 || worldIndex >= MAX_RENDER_WORLDS ) return qfalse;
	if ( !R_WorldSlotResident( worldIndex ) ) return qtrue;
	if ( s_activeWorldIndex == worldIndex ) {
		tr.world = NULL;
		s_activeWorldIndex = -1;
	}
	memset( &s_worldDataSlots[worldIndex], 0, sizeof( s_worldDataSlots[worldIndex] ) );
	memset( &s_worldMapStates[worldIndex], 0, sizeof( s_worldMapStates[worldIndex] ) );
	R_TemporalWorldLoaded( worldIndex );
	tr.worldMapLoaded = R_ResidentWorldCount() > 0 ? qtrue : qfalse;
	if ( !tr.world )
		for ( int replacement = 0; replacement < MAX_RENDER_WORLDS; ++replacement )
			if ( R_WorldSlotResident( replacement ) ) {
				R_SetWorldSlot( replacement );
				break;
			}
	return qtrue;
}


/*
=================
RE_LoadWorldMap

Called directly from cgame
=================
*/
void RE_LoadWorldMap( const mapFile_t *bsp, int worldIndex ) {
	const char	*name;
	int			i;
	int32_t		size;
	dheader_t	*header;
	union {
		byte *b;
		void *v;
	} buffer;
	byte		*startMarker;


	if ( worldIndex < 0 || worldIndex >= MAX_RENDER_WORLDS )
		worldIndex = 0;
	if ( R_WorldSlotResident( worldIndex ) ) {
		ri.Terminate( TERM_CLIENT_DROP, "ERROR: attempted to redundantly load world map" );
	}

	if ( !bsp ) {
		ri.Terminate( TERM_CLIENT_DROP, "%s: bsp is NULL", __func__ );
	}
	qboolean vertexLightingAllowed = tr.vertexLightingAllowed;
	if ( s_activeWorldIndex >= 0 )
		R_SaveWorldMapState( s_activeWorldIndex );
	s_activeWorldIndex = -1;
	tr.world = NULL;
	R_ClearWorldMapGlobals( s_pendingWorldVisData, vertexLightingAllowed );
	s_pendingWorldVisData = NULL;

	name = bsp->name;
	if ( !name[0] ) {
		ri.Terminate( TERM_CLIENT_DROP, "%s: bsp has empty name", __func__ );
	}

	if ( bsp->version ) {
		ri.Cvar_Set( "com_mapBspVersion", va( "%d", bsp->version ) );
	}

	// clear any stale loading flag from a previous errored load
	tr.mapLoading = qfalse;

	// Map-load timer: brackets the world load (BSP + shaders + world textures). The
	// texture uploads here are what the async path pipelines, so the span + the upload
	// sync/async split below quantify what async upload buys on a cold load.
	int64_t  mapLoadStartUs = ri.Microseconds();
	uint32_t uploadSyncStart = 0, uploadAsyncStart = 0;
#ifdef USE_VULKAN
	vk_ral_upload_counts( &uploadSyncStart, &uploadAsyncStart );
#endif

	// set default sun direction to be used if it isn't
	// overridden by a shader
	tr.sunDirection[0] = 0.45f;
	tr.sunDirection[1] = 0.3f;
	tr.sunDirection[2] = 0.9f;

	VectorNormalize( tr.sunDirection );

	tr.worldMapLoaded = qtrue;

	buffer.b = bsp->rawData;
	size = bsp->rawLength;
	if ( !buffer.b || size <= 0 ) {
		ri.Terminate( TERM_CLIENT_DROP, "%s: %s has no raw BSP data", __func__, name );
	}
	if ( size < sizeof( dheader_t ) ) {
		ri.Terminate( TERM_CLIENT_DROP, "%s: %s has truncated header", __func__, name );
	}

	tr.mapLoading = qtrue;

	// clear tr.world so if the level fails to load, the next
	// try will not look at the partially loaded version
	tr.world = NULL;

	memset( &s_worldData, 0, sizeof( s_worldData ) );
	Q_strncpyz( s_worldData.name, name, sizeof( s_worldData.name ) );

	Q_strncpyz( s_worldData.baseName, COM_SkipPath( s_worldData.name ), sizeof( s_worldData.name ) );
	COM_StripExtension(s_worldData.baseName, s_worldData.baseName, sizeof(s_worldData.baseName));

	startMarker = ri.Hunk_Alloc(0, h_low);
	c_gridVerts = 0;

	header = (dheader_t *)buffer.b;
	fileBase = (byte *)header;

	// swap all the lumps
	for ( i = 0; i < sizeof( dheader_t ) / 4; i++ ) {
		( (int32_t *)header )[i] = LittleLong( ( (int32_t *)header )[i] );
	}

	for ( i = 0; i < HEADER_LUMPS; i++ ) {
		uint32_t ofs = header->lumps[i].fileofs;
		uint32_t len = header->lumps[i].filelen;
		if ( (uint64_t)ofs + len > size ) {
			ri.Terminate( TERM_CLIENT_DROP, "%s: %s has wrong lump[%i] size/offset", __func__, name, i );
		}
	}

	// load into heap
	// Q1 maps have lightmapStyles=4 (set by bsp_q1.c); Q3 maps leave it at 0.
	// Q1 lightmap bytes are full-range 0-255 and must not be run through
	// R_ColorShiftLightingBytes (which assumes Q3 overbright pre-compression).
	R_LoadLightmaps( bsp, bsp->lightmapStyles != 0 );
	R_LoadQ1StyledLightmaps( bsp );
	R_Q1_SetCreateImageFn( RVK_Q1_CreateImage );
	R_Q1_SetLoadImageFn( RVK_Q1_LoadImage );
	R_Q1_SetCreateImageArrayFn( RVK_Q1_CreateImageArray );
	R_Q1_PrepareTextures( bsp );
	R_PreLoadFogs( bsp );
	R_LoadShaders( bsp );
	R_LoadPlanes( bsp );
	R_LoadFogs( bsp );
	lightstyleData = bsp->drawVertLightstyles;
	R_LoadSurfaces( bsp );
	lightstyleData = NULL;
	R_LoadMarksurfaces( bsp );
	R_LoadNodesAndLeafs( bsp );
	R_LoadSubmodels( bsp );
	R_LoadVisibility( bsp );
	R_LoadEntities( bsp );
	R_ExtractStaticLights();   // r_unbakeStaticLights-gated (no-op when off → byte-identical)
	if ( r_unbakeStaticLights && r_unbakeStaticLights->integer && s_worldData.numStaticLights > 0 )
		R_BuildStaticLightClusters( &s_worldData );   // world-space cluster grid (view-independent static lighting)
	R_LoadLightGrid( bsp );

#ifdef USE_VBO
	R_BuildWorldVBO( s_worldData.surfaces, s_worldData.numsurfaces );
#endif

	// build the per-surface AABB SSBO for the GPU world cull now
	// that the world surfaces are loaded (static geometry — built once, reused
	// every frame). No-op if the backend/RAL cull pipeline isn't available.
	vk_cull_build_world_aabbs( s_worldData.surfaces, s_worldData.numsurfaces );

	tr.mapLoading = qfalse;

	s_worldData.dataSize = (byte *)ri.Hunk_Alloc(0, h_low) - startMarker;

	s_worldData.lightStyles = ( bsp->styledLightmapData[1] != NULL ) ? qtrue : qfalse;

	// Publish the freshly-loaded world into the owning app's slot now that the
	// entire level loaded properly. The struct is a flat block of pointers/counts
	// into the (still-live) hunk allocations, so a shallow copy into the slot keeps
	// every reference valid. tr.world points at the slot, not the load scratch, so
	// the active app's render reads its own world. Single app: slot 0, identical to
	// pointing tr.world straight at the scratch as before.
	s_worldDataSlots[ worldIndex ] = s_worldData;
	tr.world = &s_worldDataSlots[ worldIndex ];
	s_activeWorldIndex = worldIndex;
	R_TemporalWorldLoaded( worldIndex );

	// A real sun exists only if a shader's sun/q3map_sun keyword moved tr.sunDirection
	// off the engine default set above. Computed once here from the final post-shader
	// direction, independent of r_shadows, so consumers that don't depend on the
	// shadow cvar (the sun-ray) share the same truth.
	{
		vec3_t defaultSun, diff;
		VectorSet( defaultSun, 0.45f, 0.3f, 0.9f );
		VectorNormalize( defaultSun );
		VectorSubtract( tr.sunDirection, defaultSun, diff );
		tr.sunHasSource = ( VectorLength( diff ) > 1e-4f ) ? qtrue : qfalse;
	}

#if FEAT_SHADOW_MAPPING
	// build the sun-shadow caster geometry now that tr.world is
	// valid (no-op when r_shadows is off). Mirrors R_BuildWorldVBO timing.
	vk_build_shadow_caster();
	// bake the load-time sun-visibility mask atlas
	// (no-op unless r_shadows=1 and a shader set a real sun direction).
	R_BuildSunMaskAtlas();
#endif
	R_SaveWorldMapState( worldIndex );

	{
		double   mapLoadMs = (double)( ri.Microseconds() - mapLoadStartUs ) / 1000.0;
		uint32_t uploadSyncEnd = 0, uploadAsyncEnd = 0;
#ifdef USE_VULKAN
		vk_ral_upload_counts( &uploadSyncEnd, &uploadAsyncEnd );
#endif
		R_LOG( rch_assets, SEV_INFO, "map-load %s: %.1f ms (uploads sync=%u async=%u)\n",
		        name, mapLoadMs, uploadSyncEnd - uploadSyncStart, uploadAsyncEnd - uploadAsyncStart );
	}
}

// ---------------------------------------------------------------------------
// Q1 BSP diagnostic dump command
// ---------------------------------------------------------------------------

// AABB touch with 1-unit tolerance — same metric as Q1_RawLeafAABBTouch in bsp_q1.c
static qboolean Q1_MnodesAdjacent( const mnode_t *a, const mnode_t *b ) {
	int d;
	for ( d = 0; d < 3; d++ )
		if ( a->maxs[d] + 1.0f < b->mins[d] || b->maxs[d] + 1.0f < a->mins[d] )
			return qfalse;
	return qtrue;
}

// Returns: 0=empty 1=water 2=slime 3=lava 4=solid
static int Q1_InferLeafType( const mnode_t *leaf ) {
	vec3_t center;
	int    contents;
	if ( leaf->cluster < 0 )
		return 4;
	// Q1 face-ownership: a water surface normal points into the air leaf above
	// it, so marksurface shader flags misclassify air leaves as liquid.
	// Query the collision model instead — BSP_Q1_SynthesizeLiquidBrushes
	// built correct volumetric AABB brushes from Q1 leaf raw contents.
	center[0] = ( leaf->mins[0] + leaf->maxs[0] ) * 0.5f;
	center[1] = ( leaf->mins[1] + leaf->maxs[1] ) * 0.5f;
	center[2] = ( leaf->mins[2] + leaf->maxs[2] ) * 0.5f;
	contents = ri.CM_PointContents( center, 0 );
	if ( contents & CONTENTS_LAVA  ) return 3;
	if ( contents & CONTENTS_SLIME ) return 2;
	if ( contents & CONTENTS_WATER ) return 1;
	return 0;
}

static const char *Q1_LeafTypeName( int t ) {
	switch ( t ) {
		case 0: return "empty";
		case 1: return "water";
		case 2: return "slime";
		case 3: return "lava";
		case 4: return "solid";
	}
	return "?";
}

static int Q1_PVSPopcount( const byte *row, int clusterBytes ) {
	int b, n = 0;
	for ( b = 0; b < clusterBytes; b++ ) {
		byte v = row[b];
		v = v - ((v >> 1) & 0x55u);
		v = (v & 0x33u) + ((v >> 2) & 0x33u);
		n += (int)((v + (v >> 4)) & 0x0Fu);
	}
	return n;
}

#define BSPDUMP_CAP (20 * 1024 * 1024)

static void bsp_dump_append( char *buf, size_t *off, const char *fmt, ... )
	__attribute__ ((format (printf, 3, 4)));
static void bsp_dump_append( char *buf, size_t *off, const char *fmt, ... ) {
	va_list ap;
	int n;
	if ( *off >= BSPDUMP_CAP - 1 ) return;
	va_start( ap, fmt );
	n = vsnprintf( buf + *off, BSPDUMP_CAP - *off, fmt, ap );
	va_end( ap );
	if ( n > 0 ) *off += (size_t)n;
}

static void Q1_WriteBSPDump( void ) {
	world_t    *w           = tr.world;
	int         numDecision = w->numDecisionNodes;
	int         numLeafs    = w->numnodes - numDecision;
	int         numClusters = w->numClusters;
	int         clusterBytes = w->clusterBytes;
	const byte *vis          = w->vis;
	char        path[MAX_OSPATH];
	char       *buf;
	size_t      off = 0;
	int         i, j;
	int         cnt[5] = {0,0,0,0,0}; /* empty water slime lava solid */

	buf = (char *)ri.Malloc( BSPDUMP_CAP );

	/* ---- SUMMARY ---- */
	bsp_dump_append( buf, &off, "=== SUMMARY ===\n" );
	bsp_dump_append( buf, &off, "map              %s\n", w->baseName );
	bsp_dump_append( buf, &off, "numDecisionNodes %d\n", numDecision );
	bsp_dump_append( buf, &off, "numLeafs         %d\n", numLeafs );
	bsp_dump_append( buf, &off, "numClusters      %d\n", numClusters );
	bsp_dump_append( buf, &off, "clusterBytes     %d\n", clusterBytes );

	for ( i = 0; i < numLeafs; i++ )
		cnt[ Q1_InferLeafType( &w->nodes[numDecision + i] ) ]++;

	bsp_dump_append( buf, &off,
		"leafTypes        solid=%d empty=%d water=%d slime=%d lava=%d\n\n",
		cnt[4], cnt[0], cnt[1], cnt[2], cnt[3] );

	/* ---- CONTENTS_HISTOGRAM ---- */
	bsp_dump_append( buf, &off, "=== CONTENTS_HISTOGRAM ===\n" );
	bsp_dump_append( buf, &off, "%-10s  count\n", "type" );
	bsp_dump_append( buf, &off, "%-10s  %d\n", "solid", cnt[4] );
	bsp_dump_append( buf, &off, "%-10s  %d\n", "empty", cnt[0] );
	bsp_dump_append( buf, &off, "%-10s  %d\n", "water", cnt[1] );
	bsp_dump_append( buf, &off, "%-10s  %d\n", "slime", cnt[2] );
	bsp_dump_append( buf, &off, "%-10s  %d\n\n", "lava", cnt[3] );

	/* ---- LEAVES ---- */
	bsp_dump_append( buf, &off, "=== LEAVES ===\n" );
	bsp_dump_append( buf, &off, "%-6s  %-8s  %-6s  %-6s  %-10s  %-10s  %-10s  %-10s  %-10s  %s\n",
		"idx", "cluster", "type", "surfs",
		"bmin.x", "bmin.y", "bmin.z", "bmax.x", "bmax.y", "bmax.z" );
	for ( i = 0; i < numLeafs; i++ ) {
		const mnode_t *leaf = &w->nodes[numDecision + i];
		bsp_dump_append( buf, &off,
			"%-6d  %-8d  %-6s  %-6d  %-10.0f  %-10.0f  %-10.0f  %-10.0f  %-10.0f  %.0f\n",
			i, leaf->cluster, Q1_LeafTypeName( Q1_InferLeafType( leaf ) ),
			leaf->nummarksurfaces,
			(double)leaf->mins[0], (double)leaf->mins[1], (double)leaf->mins[2],
			(double)leaf->maxs[0], (double)leaf->maxs[1], (double)leaf->maxs[2] );
	}
	bsp_dump_append( buf, &off, "\n" );

	/* ---- PVS_STATS ---- */
	if ( vis && numClusters > 0 && clusterBytes > 0 ) {
		bsp_dump_append( buf, &off, "=== PVS_STATS ===\n" );
		bsp_dump_append( buf, &off, "%-10s  %-12s  %s\n",
			"cluster", "visible_bits", "pct_of_total" );
		for ( i = 0; i < numClusters; i++ ) {
			int pop = Q1_PVSPopcount( vis + (size_t)i * clusterBytes, clusterBytes );
			bsp_dump_append( buf, &off, "%-10d  %-12d  %.1f%%\n",
				i, pop, numClusters > 0 ? (100.0f * pop / numClusters) : 0.0f );
		}
		bsp_dump_append( buf, &off, "\n" );
	}

	/* ---- LIQUID_LEAVES ---- */
	bsp_dump_append( buf, &off, "=== LIQUID_LEAVES ===\n" );
	bsp_dump_append( buf, &off, "%-6s  %-8s  %-6s  %-6s  %s\n",
		"idx", "cluster", "type", "surfs", "adj_non_liquid_clusters" );
	for ( i = 0; i < numLeafs; i++ ) {
		const mnode_t *li = &w->nodes[numDecision + i];
		int ti = Q1_InferLeafType( li );
		if ( ti < 1 || ti > 3 ) continue;
		bsp_dump_append( buf, &off, "%-6d  %-8d  %-6s  %-6d  [",
			i, li->cluster, Q1_LeafTypeName( ti ), li->nummarksurfaces );
		for ( j = 0; j < numLeafs; j++ ) {
			const mnode_t *lj = &w->nodes[numDecision + j];
			int tj = Q1_InferLeafType( lj );
			if ( tj >= 1 && tj <= 3 ) continue;
			if ( lj->cluster < 0 ) continue;
			if ( Q1_MnodesAdjacent( li, lj ) )
				bsp_dump_append( buf, &off, "%d ", lj->cluster );
		}
		bsp_dump_append( buf, &off, "]\n" );
	}
	bsp_dump_append( buf, &off, "\n" );

	/* ---- NON_LIQUID_NEIGHBOURS_OF_LIQUID ---- */
	bsp_dump_append( buf, &off, "=== NON_LIQUID_NEIGHBOURS_OF_LIQUID ===\n" );
	bsp_dump_append( buf, &off, "%-16s  %s\n",
		"liquid_cluster", "non_liquid_adjacent_clusters" );
	for ( i = 0; i < numLeafs; i++ ) {
		const mnode_t *li = &w->nodes[numDecision + i];
		int ti = Q1_InferLeafType( li );
		if ( ti < 1 || ti > 3 ) continue;
		bsp_dump_append( buf, &off, "%-16d  [", li->cluster );
		for ( j = 0; j < numLeafs; j++ ) {
			const mnode_t *lj = &w->nodes[numDecision + j];
			int tj = Q1_InferLeafType( lj );
			if ( tj >= 1 && tj <= 3 ) continue;
			if ( lj->cluster < 0 ) continue;
			if ( Q1_MnodesAdjacent( li, lj ) )
				bsp_dump_append( buf, &off, "%d ", lj->cluster );
		}
		bsp_dump_append( buf, &off, "]\n" );
	}
	bsp_dump_append( buf, &off, "\n" );

	/* ---- ABOVE_LIQUID_VIEW_DOWN ---- */
	if ( vis && numClusters > 0 && clusterBytes > 0 ) {
		bsp_dump_append( buf, &off, "=== ABOVE_LIQUID_VIEW_DOWN ===\n" );
		bsp_dump_append( buf, &off, "%-10s  %s\n",
			"cluster", "liquid_clusters_in_pvs" );
		for ( i = 0; i < numLeafs; i++ ) {
			const mnode_t *li = &w->nodes[numDecision + i];
			int ti = Q1_InferLeafType( li );
			if ( ti != 0 ) continue; /* only empty (non-liquid, non-solid) leaves */
			if ( li->cluster < 0 || li->cluster >= numClusters ) continue;
			{
				const byte *row  = vis + (size_t)li->cluster * clusterBytes;
				int         any  = 0;
				for ( j = 0; j < numLeafs; j++ ) {
					const mnode_t *lj = &w->nodes[numDecision + j];
					int tj = Q1_InferLeafType( lj );
					if ( tj < 1 || tj > 3 ) continue;
					if ( lj->cluster < 0 || lj->cluster >= numClusters ) continue;
					if ( row[lj->cluster >> 3] & (1 << (lj->cluster & 7)) ) {
						if ( !any ) {
							bsp_dump_append( buf, &off, "%-10d  [", li->cluster );
							any = 1;
						}
						bsp_dump_append( buf, &off, "%d ", lj->cluster );
					}
				}
				if ( any )
					bsp_dump_append( buf, &off, "]\n" );
			}
		}
		bsp_dump_append( buf, &off, "\n" );
	}

	/* ---- SURFACES_PER_LIQUID ---- */
	bsp_dump_append( buf, &off, "=== SURFACES_PER_LIQUID ===\n" );
	bsp_dump_append( buf, &off, "%-6s  %-8s  %-6s  %s\n",
		"idx", "cluster", "type", "shaders" );
	for ( i = 0; i < numLeafs; i++ ) {
		const mnode_t *li = &w->nodes[numDecision + i];
		int ti = Q1_InferLeafType( li );
		if ( ti < 1 || ti > 3 ) continue;
		bsp_dump_append( buf, &off, "%-6d  %-8d  %-6s  ",
			i, li->cluster, Q1_LeafTypeName( ti ) );
		for ( j = 0; j < li->nummarksurfaces; j++ )
			bsp_dump_append( buf, &off, "%s ",
				li->firstmarksurface[j]->shader->name );
		bsp_dump_append( buf, &off, "\n" );
	}
	bsp_dump_append( buf, &off, "\n" );

	/* ---- ADJACENCY_SAMPLE ---- */
	{
		int pairs = 0;
		bsp_dump_append( buf, &off, "=== ADJACENCY_SAMPLE (first 30 non-solid pairs) ===\n" );
		bsp_dump_append( buf, &off, "%-6s  %-6s  %-10s  %-10s  %-6s  %s\n",
			"leaf_a", "leaf_b", "cluster_a", "cluster_b", "type_a", "type_b" );
		for ( i = 0; i < numLeafs && pairs < 30; i++ ) {
			const mnode_t *la = &w->nodes[numDecision + i];
			if ( la->cluster < 0 ) continue;
			for ( j = i + 1; j < numLeafs && pairs < 30; j++ ) {
				const mnode_t *lb = &w->nodes[numDecision + j];
				if ( lb->cluster < 0 ) continue;
				if ( !Q1_MnodesAdjacent( la, lb ) ) continue;
				bsp_dump_append( buf, &off,
					"%-6d  %-6d  %-10d  %-10d  %-6s  %s\n",
					i, j, la->cluster, lb->cluster,
					Q1_LeafTypeName( Q1_InferLeafType( la ) ),
					Q1_LeafTypeName( Q1_InferLeafType( lb ) ) );
				pairs++;
			}
		}
	}

	/* ---- BRUSHES ---- */
	{
		int numBrushes = ri.CM_NumBrushes();
		int bi, si;
		bsp_dump_append( buf, &off, "\n=== BRUSHES (%d total) ===\n", numBrushes );
		bsp_dump_append( buf, &off, "%-6s  %-28s  %-12s  %-5s  %-20s  %s\n",
			"idx", "shader", "contents", "sides", "bounds_min", "bounds_max" );
		for ( bi = 0; bi < numBrushes; bi++ ) {
			int         contents, shaderNum, numsides;
			const char *shaderName;
			float       bmins[3], bmaxs[3];
			char        smin[32], smax[32];
			ri.CM_GetBrushData( bi, &contents, &shaderNum, &shaderName,
								bmins, bmaxs, &numsides );
			if ( fabsf(bmins[0]) >= 65535.5f || fabsf(bmins[1]) >= 65535.5f ||
				 fabsf(bmins[2]) >= 65535.5f || fabsf(bmaxs[0]) >= 65535.5f ||
				 fabsf(bmaxs[1]) >= 65535.5f || fabsf(bmaxs[2]) >= 65535.5f ) {
				Com_sprintf( smin, sizeof(smin), "UNBOUND" );
				Com_sprintf( smax, sizeof(smax), "UNBOUND" );
			} else {
				Com_sprintf( smin, sizeof(smin), "%.0f,%.0f,%.0f",
							 bmins[0], bmins[1], bmins[2] );
				Com_sprintf( smax, sizeof(smax), "%.0f,%.0f,%.0f",
							 bmaxs[0], bmaxs[1], bmaxs[2] );
			}
			bsp_dump_append( buf, &off,
				"%-6d  %-28s  0x%08x    %-5d  %-20s  %s\n",
				bi, shaderName, (unsigned)contents, numsides, smin, smax );
			for ( si = 0; si < numsides; si++ ) {
				int         pn, sShaderNum;
				float       snormal[3], sdist;
				const char *sShaderName;
				ri.CM_GetBrushSideData( bi, si, &pn, snormal, &sdist,
										&sShaderNum, &sShaderName );
				bsp_dump_append( buf, &off,
					"  side[%d]  pn=%-6d  n=(%.3f,%.3f,%.3f)  dist=%.3f  shader=%s\n",
					si, pn, snormal[0], snormal[1], snormal[2], sdist, sShaderName );
			}
		}
	}

	/* ---- write file ---- */
	Com_sprintf( path, sizeof(path), "maps/%s.bspdump", w->baseName );
	ri.FS_WriteFile( path, buf, (int)off );
	R_LOG( rch_assets, SEV_INFO, "BSP dump written to %s (%d bytes)\n", path, (int)off );

	ri.Free( buf );
}

void Cmd_BSPDump_f( void ) {
	if ( !tr.world ) {
		R_LOG( rch_assets, SEV_INFO, "bspdump: no map loaded\n" );
		return;
	}
	Q1_WriteBSPDump();
}


/*
===========
ParsePropFace

Face parser for standalone Q1 BSP props (e.g. maps/b_explob.bsp).
Resolves the shader full-bright (LIGHTMAP_WHITEIMAGE) so the Q1 LS
pipeline never fires, then delegates geometry to ParseFaceCommon.
No lightmap atlas, no anim chains, no lightstyle slots.
===========
*/
static void ParsePropFace( const dsurface_t *ds, const drawVert_t *verts, int numPoints,
                           msurface_t *surf, int *srcIndexes, int numIndexes,
                           const char *shaderName, int lightmapNum ) {
	srfSurfaceFace_t *cv;
	shader_t *shader = R_FindShader( shaderName, lightmapNum, qtrue );
	if ( shader->defaultShader )
		shader = tr.defaultShader;

	cv = ParseFaceCommon( ds, verts, numPoints, surf, srcIndexes, numIndexes,
	                      shader, lightmapNum, 0.0f, 0.0f, NULL );

	// Style slot 0 = Q1 "always-on" style; lightstyleValues[0] = 1.0 at runtime.
	// With a real prop lightmap the Q1 LS pipeline multiplies by 1.0 — correct baked light.
	// Full-bright fallback (LIGHTMAP_WHITEIMAGE) keeps all slots at 255 so the pipeline
	// is skipped entirely (bundle[1].lightmap stays LIGHTMAP_INDEX_NONE).
	if ( lightmapNum != LIGHTMAP_WHITEIMAGE )
		cv->lightStyles[0] = 0;

	surf->fogIndex = 0;
}


/*
===========
R_RegisterBSP

Model loader for standalone Q1 BSP prop files (e.g. maps/b_explob.bsp).
Registered in modelLoaders[] as "bsp" so RE_RegisterModel dispatches here.

Faz 1: full-bright only; no lightmap baking.  All faces become LIGHTMAP_WHITEIMAGE
shaders so the Q1 LS pipeline is bypassed and surfaces render at full brightness.
===========
*/
qhandle_t R_RegisterBSP( const char *name, model_t *mod ) {
	mapFile_t  *bsp      = NULL;
	bmodel_t   *bmodel;
	msurface_t *surfs;
	int         i, numFaces;
	int         allocSize;

	if ( !ri.Map_Load( name, &bsp, MAP_LOAD_FLAG_RENDER_ONLY ) || !bsp ) {
		R_LOG( rch_assets, SEV_WARN, "R_RegisterBSP: failed to load '%s'\n", name );
		return 0;
	}

	if ( bsp->numSubModels < 1 ) {
		R_LOG( rch_assets, SEV_WARN, "R_RegisterBSP: '%s' has no submodels\n", name );
		ri.Map_Free( bsp );
		return 0;
	}

	const dmodel_t *dm = &bsp->subModels[0];

	// Populate the Q1 texture cache with this prop's embedded miptex data so that
	// R_Q1_BuildSyntheticShader can look up the textures when ParsePropFace calls
	// R_FindShader.  The world's PrepareTextures was called earlier by RE_LoadWorldMap;
	// since all world shaders are already in the hash table, resetting the cache here
	// only affects shaders not yet looked up (i.e. new prop shaders).
	R_Q1_PrepareTextures( bsp );

	// Upload this prop's lightmap pages into tr.propLightmaps[].
	// propLmBase is the starting index in tr.propLightmaps[] for these pages;
	// -1 means the prop has no lightmap data (surfaces fall back to full-bright).
	int propLmBase = R_UploadPropLightmaps( bsp );

	// dm + the surfaces/shaders/drawVerts/drawIndexes it references are the parsed
	// neutral arrays (already Little*-swapped on fill), so no re-swap is repeated
	// here — same neutral-read convention as the migrated RE_LoadWorldMap loaders.
	numFaces = 0;
	for ( i = 0; i < dm->numSurfaces; i++ ) {
		const dsurface_t *ds = &bsp->surfaces[ dm->firstSurface + i ];
		if ( ds->surfaceType == MST_PLANAR )
			numFaces++;
	}

	allocSize = (int)(sizeof(bmodel_t) + sizeof(msurface_t) * numFaces);
	bmodel    = ri.Hunk_Alloc( allocSize, h_low );
	surfs     = (msurface_t *)(bmodel + 1);

	bmodel->firstSurface = surfs;
	bmodel->numSurfaces  = 0;
	bmodel->bounds[0][0] = dm->mins[0];
	bmodel->bounds[0][1] = dm->mins[1];
	bmodel->bounds[0][2] = dm->mins[2];
	bmodel->bounds[1][0] = dm->maxs[0];
	bmodel->bounds[1][1] = dm->maxs[1];
	bmodel->bounds[1][2] = dm->maxs[2];

	for ( i = 0; i < dm->numSurfaces; i++ ) {
		const dsurface_t *ds = &bsp->surfaces[ dm->firstSurface + i ];
		if ( ds->surfaceType != MST_PLANAR )
			continue;

		const char       *shaderName = bsp->shaders[ ds->shaderNum ].shader;
		const drawVert_t *verts      = bsp->drawVerts + ds->firstVert;
		int              *srcIndexes = bsp->drawIndexes + ds->firstIndex;
		int               dsLmNum   = ds->lightmapNum;

		// Per-surface lightmap index: LIGHTMAP_PROP_OFFSET + base + page_within_this_prop.
		// ds->lightmapNum holds the page index baked by bsp_q1.c; -1 = no lightmap.
		int propLmIndex = ( propLmBase >= 0 && dsLmNum >= 0 )
			? LIGHTMAP_PROP_OFFSET + propLmBase + dsLmNum
			: LIGHTMAP_WHITEIMAGE;

		ParsePropFace( ds, verts, ds->numVerts,
		               &surfs[ bmodel->numSurfaces ],
		               srcIndexes, ds->numIndexes,
		               shaderName, propLmIndex );
		bmodel->numSurfaces++;
	}

	mod->type     = MOD_BRUSH;
	mod->bmodel   = bmodel;
	mod->dataSize = allocSize;

	ri.Map_Free( bsp );

	R_LOG( rch_assets, SEV_INFO, "R_RegisterBSP: '%s' loaded (%d faces)\n", name, bmodel->numSurfaces );
	return mod->index;
}
