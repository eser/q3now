// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 1999-2005 Id Software, Inc.
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
// tr_image.c
#include "tr_local.h"
#include "../renderercommon/r_log.h"  // rilog-channel-mechanism — renderer.assets
#ifdef USE_VULKAN
#include "vk_ral_textures.h"   // parallel-paths RAL texture migration
#endif

R_LOG_DECLARE_CHANNEL( rch_assets, "renderer.assets" );

static byte			 s_intensitytable[256];
static unsigned char s_gammatable[256];

#ifdef USE_VULKAN
static unsigned char s_gammatable_linear[256];
#endif

GLint	gl_filter_min = GL_LINEAR_MIPMAP_NEAREST;
GLint	gl_filter_max = GL_LINEAR;

#define FILE_HASH_SIZE		1024
static	image_t*		hashTable[FILE_HASH_SIZE];

/*
================
return a hash value for the filename
================
*/
#define generateHashValue(fname) Com_GenerateHashValue((fname),FILE_HASH_SIZE)


/*
** R_GammaCorrect
*/
void R_GammaCorrect( byte *buffer, int bufSize ) {
#ifdef USE_VULKAN
	if ( vk.capture.image != VK_NULL_HANDLE )
		return;
	if ( !gls.deviceSupportsGamma )
		return;
#endif
	for ( int i = 0; i < bufSize; i++ ) {
		buffer[i] = s_gammatable[buffer[i]];
	}
}

typedef struct {
	const char *name;
	GLint minimize, maximize;
} textureMode_t;

static const textureMode_t modes[] = {
	{"GL_NEAREST", GL_NEAREST, GL_NEAREST},
	{"GL_LINEAR", GL_LINEAR, GL_LINEAR},
	{"GL_NEAREST_MIPMAP_NEAREST", GL_NEAREST_MIPMAP_NEAREST, GL_NEAREST},
	{"GL_LINEAR_MIPMAP_NEAREST", GL_LINEAR_MIPMAP_NEAREST, GL_LINEAR},
	{"GL_NEAREST_MIPMAP_LINEAR", GL_NEAREST_MIPMAP_LINEAR, GL_NEAREST},
	{"GL_LINEAR_MIPMAP_LINEAR", GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR}
};

/*
===============
GL_TextureMode
===============
*/
void GL_TextureMode( const char *string ) {
	const textureMode_t *mode;
	image_t	*img;
	int		i;

	mode = NULL;
	for ( i = 0 ; i < ARRAY_LEN( modes ) ; i++ ) {
		if ( !Q_stricmp( modes[i].name, string ) ) {
			mode = &modes[i];
			break;
		}
	}

	if ( mode == NULL ) {
		R_LOG( rch_assets, SEV_INFO, "bad texture filter name '%s'\n", string );
		return;
	}

	gl_filter_min = mode->minimize;
	gl_filter_max = mode->maximize;

#ifdef USE_VULKAN
	if ( gl_filter_min == vk.samplers.filter_min && gl_filter_max == vk.samplers.filter_max ) {
		return;
	}
	vk_wait_idle();
	vk_destroy_samplers();

	vk.samplers.filter_min = gl_filter_min;
	vk.samplers.filter_max = gl_filter_max;
	vk_update_attachment_descriptors();
	for ( i = 0; i < tr.numImages; i++ ) {
		img = tr.images[i];
		if ( img->flags & IMGFLAG_MIPMAP ) {
			vk_update_descriptor_set( img, qtrue );
		}
	}
#else
	// hack to prevent trilinear from being set on voodoo,
	// because their driver freaks...
	if ( glConfig.hardwareType == GLHW_3DFX_2D3D && gl_filter_max == GL_LINEAR &&
		gl_filter_min == GL_LINEAR_MIPMAP_LINEAR ) {
		gl_filter_min = GL_LINEAR_MIPMAP_NEAREST;
		R_LOG( rch_assets, SEV_INFO, "Refusing to set trilinear on a voodoo.\n" );
	}

	// change all the existing mipmap texture objects
	for ( i = 0; i < tr.numImages; i++ ) {
		img = tr.images[ i ];
		if ( img->flags & IMGFLAG_MIPMAP ) {
			GL_Bind( img );
			qglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_min );
			qglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max );
		}
	}
#endif
}


/*
===============
R_SumOfUsedImages
===============
*/
int R_SumOfUsedImages( void ) {
	const image_t *img;
	int i, total = 0;

	for ( i = 0; i < tr.numImages; i++ ) {
		img = tr.images[ i ];
		if ( img->frameUsed == tr.frameCount ) {
			total += img->uploadWidth * img->uploadHeight;
		}
	}

	return total;
}

/*
===============
R_ImageIsPinned

Do-not-evict predicate for the texture-LRU (Phase 7.15.4). True when the image is
flagged IMGFLAG_PINNED (set in 7.15.4-a on class A/B/C). The victim-scan in
7.15.4-c skips pinned images. No caller yet in 7.15.4-a — dark classification.
===============
*/
qboolean R_ImageIsPinned( const image_t *img ) {
	return ( img && ( img->flags & IMGFLAG_PINNED ) ) ? qtrue : qfalse;
}


/*
===============
R_ImageList_f
===============
*/
void R_ImageList_f( void ) {
	const image_t *image;
	int i, estTotalSize = 0;
	char *name, buf[MAX_QPATH*2 + 5];

	R_LOG( rch_assets, SEV_INFO, "\n -n- --w-- --h-- type  -size- --name-------\n" );

	for ( i = 0; i < tr.numImages; i++ )
	{
		const char *format = "???? ";
		const char *sizeSuffix;
		int estSize;
		int displaySize;

		image = tr.images[ i ];
		estSize = image->uploadHeight * image->uploadWidth;

		switch ( image->internalFormat )
		{
#ifdef USE_VULKAN
			case VK_FORMAT_B8G8R8A8_UNORM:
				format = "BGRA ";
				estSize *= 4;
				break;
			case VK_FORMAT_R8G8B8A8_UNORM:
				format = "RGBA ";
				estSize *= 4;
				break;
			case VK_FORMAT_R8G8B8_UNORM:
				format = "RGB  ";
				estSize *= 3;
				break;
			case VK_FORMAT_B4G4R4A4_UNORM_PACK16:
				format = "RGBA ";
				estSize *= 2;
				break;
			case VK_FORMAT_A1R5G5B5_UNORM_PACK16:
				format = "RGB  ";
				estSize *= 2;
				break;
#else
			case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
			case GL_COMPRESSED_RGB_S3TC_DXT1_EXT:
				format = "DXT1 ";
				// 64 bits per 16 pixels, so 4 bits per pixel
				estSize /= 2;
				break;
			case GL_RGB4_S3TC:
				format = "S3TC ";
				// same as DXT1?
				estSize /= 2;
				break;
			case GL_RGBA4:
			case GL_RGBA8:
			case GL_RGBA:
				format = "RGBA ";
				// 4 bytes per pixel
				estSize *= 4;
				break;
			case GL_RGB5:
			case GL_RGB8:
			case GL_RGB:
				format = "RGB  ";
				// 3 bytes per pixel?
				estSize *= 3;
				break;
#endif
		}

		// mipmap adds about 50%
		if (image->flags & IMGFLAG_MIPMAP)
			estSize += estSize / 2;

		sizeSuffix = "b ";
		displaySize = estSize;

		if ( displaySize >= 2048 )
		{
			displaySize = ( displaySize + 1023 ) / 1024;
			sizeSuffix = "kb";
		}

		if ( displaySize >= 2048 )
		{
			displaySize = ( displaySize + 1023 ) / 1024;
			sizeSuffix = "Mb";
		}

		if ( displaySize >= 2048 )
		{
			displaySize = ( displaySize + 1023 ) / 1024;
			sizeSuffix = "Gb";
		}

		if ( Q_stricmp( image->imgName, image->imgName2 ) == 0 ) {
			name = image->imgName;
		} else {
			Com_sprintf( buf, sizeof( buf ), "%s => " S_COLOR_YELLOW "%s",
				image->imgName, image->imgName2 );
			name = buf;
		}

		R_LOG( rch_assets, SEV_INFO, " %3i %5i %5i %s %4i%s %s\n", i, image->uploadWidth, image->uploadHeight, format, displaySize, sizeSuffix, name );
		estTotalSize += estSize;
	}

	R_LOG( rch_assets, SEV_INFO, " -----------------------\n" );
	R_LOG( rch_assets, SEV_INFO, " approx %i kbytes\n", (estTotalSize + 1023) / 1024 );
	R_LOG( rch_assets, SEV_INFO, " %i total images\n\n", tr.numImages );
}


/*
===============
R_TestDDS_f

developer command — `testdds <path[.dds]>`. Loads the
named DDS through the normal image path (so cubemap / volume header
classification and the cube/3D VkImage + view creation all run) and
prints what came back: dimensions, texType, array layers, internal
VkFormat, and whether the GPU view / descriptor were created. This is
the engine-side smoke for the DDS cubemap/volume loader — there is no
shader yet that samples a samplerCube/sampler3D (that lands with the
IBL work), so this is how you confirm the loader + upload mechanics.
===============
*/
void R_TestDDS_f( void ) {
	static const char *texTypeName[] = { "2D", "CUBE", "3D", "CUBE_ARRAY" };
	char         path[MAX_QPATH];
	const image_t *image;
	int          tt;

	if ( ri.Cmd_Argc() < 2 ) {
		R_LOG( rch_assets, SEV_INFO, "usage: testdds <path>  (e.g. testdds gfx/env/test_cube.dds)\n" );
		return;
	}

	{
		const char *arg = ri.Cmd_Argv( 1 );
		if ( COM_GetExtension( arg )[0] == '\0' )
			Com_sprintf( path, sizeof( path ), "%s.dds", arg );
		else
			Q_strncpyz( path, arg, sizeof( path ) );
	}

	image = R_FindImageFile( path, IMGFLAG_NONE );
	if ( image == NULL ) {
		R_LOG( rch_assets, SEV_WARN, "testdds: '%s' could not be loaded (missing, bad header, or unsupported format).\n", path );
		return;
	}

	tt = (int)image->texType;
	if ( tt < 0 || tt >= (int)ARRAY_LEN( texTypeName ) )
		tt = 0;

	R_LOG( rch_assets, SEV_INFO, "testdds: '%s' -> %dx%d depth=%d  texType=%s  arrayLayers=%u  VkFormat=%d  view=%s descriptor=%s\n",
		image->imgName, image->width, image->height, image->depth,
		texTypeName[tt], image->layerCount, (int)image->internalFormat,
		( image->view != VK_NULL_HANDLE ) ? "ok" : "NULL",
		( image->descriptor != VK_NULL_HANDLE ) ? "ok" : "NULL" );
}

//=======================================================================

/*
================
ResampleTexture

Used to resample images in a more general than quartering fashion.

This will only be filtered properly if the resampled size
is greater than half the original size.

If a larger shrinking is needed, use the mipmap function
before or after.
================
*/
static void ResampleTexture( unsigned *in, int inwidth, int inheight, unsigned *out,
							int outwidth, int outheight ) {
	int		i, j;
	unsigned	*inrow, *inrow2;
	unsigned	frac, fracstep;
	unsigned	p1[MAX_TEXTURE_SIZE];
	unsigned	p2[MAX_TEXTURE_SIZE];
	byte		*pix1, *pix2, *pix3, *pix4;

	if ( outwidth > ARRAY_LEN( p1 ) )
		ri.Terminate( TERM_CLIENT_DROP, "ResampleTexture: max width" );

	// NOLINTNEXTLINE(clang-analyzer-core.DivideZero) — caller never passes outwidth == 0; image-resampling caller invariant
	fracstep = inwidth * 0x10000 / outwidth;

	frac = fracstep>>2;
	for ( i=0 ; i<outwidth ; i++ ) {
		p1[i] = 4*(frac>>16);
		frac += fracstep;
	}
	frac = 3*(fracstep>>2);
	for ( i=0 ; i<outwidth ; i++ ) {
		p2[i] = 4*(frac>>16);
		frac += fracstep;
	}

	for (i=0 ; i<outheight ; i++, out += outwidth) {
		inrow = in + inwidth*(int)((i+0.25)*inheight/outheight);
		inrow2 = in + inwidth*(int)((i+0.75)*inheight/outheight);
		for (j=0 ; j<outwidth ; j++) {
			pix1 = (byte *)inrow + p1[j];
			pix2 = (byte *)inrow + p2[j];
			pix3 = (byte *)inrow2 + p1[j];
			pix4 = (byte *)inrow2 + p2[j];
			((byte *)(out+j))[0] = (pix1[0] + pix2[0] + pix3[0] + pix4[0])>>2;
			((byte *)(out+j))[1] = (pix1[1] + pix2[1] + pix3[1] + pix4[1])>>2;
			((byte *)(out+j))[2] = (pix1[2] + pix2[2] + pix3[2] + pix4[2])>>2;
			((byte *)(out+j))[3] = (pix1[3] + pix2[3] + pix3[3] + pix4[3])>>2;
		}
	}
}


/*
================
R_LightScaleTexture

Scale up the pixel values in a texture to increase the
lighting range
================
*/
static void R_LightScaleTexture( byte *in, int inwidth, int inheight, qboolean only_gamma )
{
	if ( in == NULL )
		return;

	if ( only_gamma )
	{
#ifdef USE_VULKAN
		if ( !glConfig.deviceSupportsGamma && !vk.fboActive )
#else
		if ( !glConfig.deviceSupportsGamma )
#endif
		{
			int		i, c;
			byte	*p;

			p = (byte *)in;

			c = inwidth*inheight;
			for (i=0 ; i<c ; i++, p+=4)
			{
				p[0] = s_gammatable[p[0]];
				p[1] = s_gammatable[p[1]];
				p[2] = s_gammatable[p[2]];
			}
		}
	}
	else
	{
		int		i, c;
		byte	*p;

		p = (byte *)in;

		c = inwidth*inheight;

#ifdef USE_VULKAN
		if ( glConfig.deviceSupportsGamma || vk.fboActive )
#else
		if ( glConfig.deviceSupportsGamma )
#endif
		{
			for (i=0 ; i<c ; i++, p+=4)
			{
				p[0] = s_intensitytable[p[0]];
				p[1] = s_intensitytable[p[1]];
				p[2] = s_intensitytable[p[2]];
			}
		}
		else
		{
			for (i=0 ; i<c ; i++, p+=4)
			{
				p[0] = s_gammatable[s_intensitytable[p[0]]];
				p[1] = s_gammatable[s_intensitytable[p[1]]];
				p[2] = s_gammatable[s_intensitytable[p[2]]];
			}
		}
	}
}


//
// sRGB <-> linear lookup tables for gamma-correct
// mipmap generation. Mipmap bytes are sRGB-encoded; naive byte-
// space averaging compresses dark tones disproportionately,
// producing chromatic shift in distant LODs. Decode -> linear-
// average -> re-encode preserves authored mid-tone luminance.
//
// Approximate sRGB transfer (pow 2.2) chosen over the piecewise
// sRGB OETF for the same reasons as gamma.frag Fix B: dark-end
// error is small, and the LUT speedup vs. piecewise branching
// matters when applied to every byte of every mip level of every
// loaded texture.
//
// The decode LUT has 256 entries (one per byte value). The encode
// LUT has 1024 entries (oversampled vs. the 256-byte output range)
// so that closely-spaced linear averages don't collapse into the
// same output bucket after the encode round-trip.
//
static float    s_srgb_to_linear_lut[256];     // sRGB byte -> linear [0,1]
static byte     s_linear_to_srgb_lut[1024];    // quantised linear -> sRGB byte
static qboolean s_mipmap_luts_initialized = qfalse;

static void R_InitMipmapLUTs( void ) {
	int i;
	if ( s_mipmap_luts_initialized ) {
		return;
	}
	for ( i = 0; i < 256; i++ ) {
		s_srgb_to_linear_lut[i] = (float)pow( (double)i / 255.0, 2.2 );
	}
	for ( i = 0; i < 1024; i++ ) {
		double linear = (double)i / 1023.0;
		s_linear_to_srgb_lut[i] = (byte)( pow( linear, 1.0 / 2.2 ) * 255.0 + 0.5 );
	}
	s_mipmap_luts_initialized = qtrue;
}


/*
================
R_SRGBToLinear / R_LinearToSRGB

Precise piecewise sRGB EOTF / OETF for one-off host-side colour
decodes (push constants, colour uniforms). Math matches the GLSL
sRGBToLinear / linearToSRGB helpers verbatim. Deliberately *not*
the pow(x, 2.2) approximation used by the mipmap LUTs above — that
LUT trades precision for bulk throughput across every texel of
every mip; these helpers run a handful of times per frame, so
accuracy wins. Defensive clamp of negatives to 0, mirroring the
shader-side max(c, 0.0).
================
*/
float R_SRGBToLinear( float c ) {
	if ( c <= 0.0f )
		return 0.0f;
	if ( c <= 0.04045f )
		return c / 12.92f;
	return (float)pow( ( (double)c + 0.055 ) / 1.055, 2.4 );
}

float R_LinearToSRGB( float c ) {
	if ( c <= 0.0f )
		return 0.0f;
	if ( c <= 0.0031308f )
		return c * 12.92f;
	return (float)( pow( (double)c, 1.0 / 2.4 ) * 1.055 - 0.055 );
}


/*
================
R_MipMap2

Operates in place, quartering the size of the texture
Proper linear filter

When isSRGB == qtrue, the inner loop decodes
input bytes via s_srgb_to_linear_lut, averages in linear domain,
re-encodes via s_linear_to_srgb_lut. Alpha is averaged byte-space
in both modes (alpha is not sRGB-encoded). When isSRGB == qfalse
the original byte-space path runs verbatim, preserving behaviour
for non-color content (normal maps, masks, MSDF distance fields,
LUTs — anything carrying IMGFLAG_NOLIGHTSCALE without
IMGFLAG_LIGHTMAP).
================
*/
static void R_MipMap2( unsigned * const out, unsigned * const in, int inWidth, int inHeight, qboolean isSRGB ) {
	int			i, j, k;
	byte		*outpix;
	int			inWidthMask, inHeightMask;
	int			total;
	int			outWidth, outHeight;
	unsigned	*temp;

	outWidth = inWidth >> 1;
	outHeight = inHeight >> 1;

	if ( out == in )
		temp = ri.Hunk_AllocateTempMemory( outWidth * outHeight * 4 );
	else
		temp = out;

	inWidthMask = inWidth - 1;
	inHeightMask = inHeight - 1;

	if ( isSRGB ) {
		R_InitMipmapLUTs();
	}

	// Sample helper: lifts the 4-fold byte-address arithmetic into
	// a single expression and lets both branches reuse the same
	// 16-tap pattern without duplicating the address-mask math.
#define SRC(yi, xi, ch) ((byte *)&in[((yi) & inHeightMask) * inWidth + ((xi) & inWidthMask)])[(ch)]

	for ( i = 0 ; i < outHeight ; i++ ) {
		for ( j = 0 ; j < outWidth ; j++ ) {
			outpix = (byte *) ( temp + i * outWidth + j );

			if ( isSRGB ) {
				// RGB: decode -> weighted linear average -> encode.
				for ( k = 0 ; k < 3 ; k++ ) {
					float ftotal =
						1.0f * s_srgb_to_linear_lut[SRC(i*2-1, j*2-1, k)] +
						2.0f * s_srgb_to_linear_lut[SRC(i*2-1, j*2  , k)] +
						2.0f * s_srgb_to_linear_lut[SRC(i*2-1, j*2+1, k)] +
						1.0f * s_srgb_to_linear_lut[SRC(i*2-1, j*2+2, k)] +

						2.0f * s_srgb_to_linear_lut[SRC(i*2  , j*2-1, k)] +
						4.0f * s_srgb_to_linear_lut[SRC(i*2  , j*2  , k)] +
						4.0f * s_srgb_to_linear_lut[SRC(i*2  , j*2+1, k)] +
						2.0f * s_srgb_to_linear_lut[SRC(i*2  , j*2+2, k)] +

						2.0f * s_srgb_to_linear_lut[SRC(i*2+1, j*2-1, k)] +
						4.0f * s_srgb_to_linear_lut[SRC(i*2+1, j*2  , k)] +
						4.0f * s_srgb_to_linear_lut[SRC(i*2+1, j*2+1, k)] +
						2.0f * s_srgb_to_linear_lut[SRC(i*2+1, j*2+2, k)] +

						1.0f * s_srgb_to_linear_lut[SRC(i*2+2, j*2-1, k)] +
						2.0f * s_srgb_to_linear_lut[SRC(i*2+2, j*2  , k)] +
						2.0f * s_srgb_to_linear_lut[SRC(i*2+2, j*2+1, k)] +
						1.0f * s_srgb_to_linear_lut[SRC(i*2+2, j*2+2, k)];
					int idx = (int)( ftotal * ( 1023.0f / 36.0f ) + 0.5f );
					if ( idx < 0 )    idx = 0;
					if ( idx > 1023 ) idx = 1023;
					outpix[k] = s_linear_to_srgb_lut[idx];
				}
				// Alpha: byte-space weighted average (alpha is not sRGB).
				total =
					1 * SRC(i*2-1, j*2-1, 3) + 2 * SRC(i*2-1, j*2  , 3) + 2 * SRC(i*2-1, j*2+1, 3) + 1 * SRC(i*2-1, j*2+2, 3) +
					2 * SRC(i*2  , j*2-1, 3) + 4 * SRC(i*2  , j*2  , 3) + 4 * SRC(i*2  , j*2+1, 3) + 2 * SRC(i*2  , j*2+2, 3) +
					2 * SRC(i*2+1, j*2-1, 3) + 4 * SRC(i*2+1, j*2  , 3) + 4 * SRC(i*2+1, j*2+1, 3) + 2 * SRC(i*2+1, j*2+2, 3) +
					1 * SRC(i*2+2, j*2-1, 3) + 2 * SRC(i*2+2, j*2  , 3) + 2 * SRC(i*2+2, j*2+1, 3) + 1 * SRC(i*2+2, j*2+2, 3);
				outpix[3] = total / 36;
			} else {
				// Original byte-space path (non-color content).
				for ( k = 0 ; k < 4 ; k++ ) {
					total =
						1 * SRC(i*2-1, j*2-1, k) + 2 * SRC(i*2-1, j*2  , k) + 2 * SRC(i*2-1, j*2+1, k) + 1 * SRC(i*2-1, j*2+2, k) +
						2 * SRC(i*2  , j*2-1, k) + 4 * SRC(i*2  , j*2  , k) + 4 * SRC(i*2  , j*2+1, k) + 2 * SRC(i*2  , j*2+2, k) +
						2 * SRC(i*2+1, j*2-1, k) + 4 * SRC(i*2+1, j*2  , k) + 4 * SRC(i*2+1, j*2+1, k) + 2 * SRC(i*2+1, j*2+2, k) +
						1 * SRC(i*2+2, j*2-1, k) + 2 * SRC(i*2+2, j*2  , k) + 2 * SRC(i*2+2, j*2+1, k) + 1 * SRC(i*2+2, j*2+2, k);
					outpix[k] = total / 36;
				}
			}
		}
	}

#undef SRC

	if ( out == in ) {
		memcpy( out, temp, outWidth * outHeight * 4 );
		ri.Hunk_FreeTempMemory( temp );
	}
}


/*
================
R_MipMap

Operates in place, quartering the size of the texture
================
*/
static void R_MipMap( byte *out, byte *in, int width, int height, qboolean isSRGB ) {
	int		i, j;
	int		row;

	if ( in == NULL )
		return;

	if ( !r_simpleMipMaps->integer ) {
		R_MipMap2( (unsigned *)out, (unsigned *)in, width, height, isSRGB );
		return;
	}

	if ( width == 1 && height == 1 ) {
		return;
	}

	if ( isSRGB ) {
		R_InitMipmapLUTs();
	}

	row = width * 4;
	width >>= 1;
	height >>= 1;

	// per-channel sRGB-aware averaging when isSRGB.
	// RGB channels round-trip through the decode/encode LUTs; alpha
	// stays in byte space. The encode LUT is 1024-entry so the
	// (sum / 4) bucket index uses `* (1023.0f / 4.0f)` directly.
	if ( width == 0 || height == 0 ) {
		width += height;	// get largest
		if ( isSRGB ) {
			for ( i = 0 ; i < width ; i++, out += 4, in += 8 ) {
				int kk;
				for ( kk = 0 ; kk < 3 ; kk++ ) {
					float ftotal = s_srgb_to_linear_lut[in[kk]] + s_srgb_to_linear_lut[in[kk+4]];
					int idx = (int)( ftotal * ( 1023.0f / 2.0f ) + 0.5f );
					if ( idx < 0 )    idx = 0;
					if ( idx > 1023 ) idx = 1023;
					out[kk] = s_linear_to_srgb_lut[idx];
				}
				out[3] = ( in[3] + in[7] ) >> 1;
			}
		} else {
			for ( i = 0 ; i < width ; i++, out += 4, in += 8 ) {
				out[0] = ( in[0] + in[4] ) >> 1;
				out[1] = ( in[1] + in[5] ) >> 1;
				out[2] = ( in[2] + in[6] ) >> 1;
				out[3] = ( in[3] + in[7] ) >> 1;
			}
		}
		return;
	}

	if ( isSRGB ) {
		for ( i = 0 ; i < height ; i++, in += row ) {
			for ( j = 0 ; j < width ; j++, out += 4, in += 8 ) {
				int kk;
				for ( kk = 0 ; kk < 3 ; kk++ ) {
					float ftotal = s_srgb_to_linear_lut[in[kk]]
					             + s_srgb_to_linear_lut[in[kk + 4]]
					             + s_srgb_to_linear_lut[in[row + kk]]
					             + s_srgb_to_linear_lut[in[row + kk + 4]];
					int idx = (int)( ftotal * ( 1023.0f / 4.0f ) + 0.5f );
					if ( idx < 0 )    idx = 0;
					if ( idx > 1023 ) idx = 1023;
					out[kk] = s_linear_to_srgb_lut[idx];
				}
				out[3] = ( in[3] + in[7] + in[row+3] + in[row+7] ) >> 2;
			}
		}
	} else {
		for ( i = 0 ; i < height ; i++, in += row ) {
			for ( j = 0 ; j < width ; j++, out += 4, in += 8 ) {
				out[0] = ( in[0] + in[4] + in[row+0] + in[row+4] ) >> 2;
				out[1] = ( in[1] + in[5] + in[row+1] + in[row+5] ) >> 2;
				out[2] = ( in[2] + in[6] + in[row+2] + in[row+6] ) >> 2;
				out[3] = ( in[3] + in[7] + in[row+3] + in[row+7] ) >> 2;
			}
		}
	}
}


/*
==================
R_BlendOverTexture

Apply a color blend over a set of pixels
==================
*/
static void R_BlendOverTexture( byte *data, int pixelCount, int mipLevel ) {

	static const byte blendColors[][4] = {
		{255,0,0,128},
		{255,255,0,128},
		{0,255,0,128},
		{0,255,255,128},
		{0,0,255,128},
		{255,0,255,128}
	};

	const byte *blend;
	int		i;
	int		inverseAlpha;
	int		premult[3];

	if ( data == NULL )
		return;

	if ( mipLevel <= 0 )
		return;

	blend = blendColors[ ( mipLevel - 1 ) % ARRAY_LEN( blendColors ) ];

	inverseAlpha = 255 - blend[3];
	premult[0] = blend[0] * blend[3];
	premult[1] = blend[1] * blend[3];
	premult[2] = blend[2] * blend[3];

	for ( i = 0 ; i < pixelCount ; i++, data+=4 ) {
		data[0] = ( data[0] * inverseAlpha + premult[0] ) >> 9;
		data[1] = ( data[1] * inverseAlpha + premult[1] ) >> 9;
		data[2] = ( data[2] * inverseAlpha + premult[2] ) >> 9;
	}
}


static qboolean RawImage_HasAlpha( const byte *scan, const int numPixels )
{
	if ( !scan )
		return qtrue;

	for ( int i = 0; i < numPixels; i++ )
	{
		if ( scan[i*4 + 3] != 255 )
		{
			return qtrue;
		}
	}

	return qfalse;
}

#ifdef USE_VULKAN

typedef struct {
	byte *buffer;
	int buffer_size;
	int mip_levels;
	int base_level_width;
	int base_level_height;
} Image_Upload_Data;

static void generate_image_upload_data( image_t *image, byte *data, Image_Upload_Data *upload_data ) {

	qboolean mipmap = (image->flags & IMGFLAG_MIPMAP) ? qtrue : qfalse;
	qboolean picmip = (image->flags & IMGFLAG_PICMIP) ? qtrue : qfalse;
	byte* resampled_buffer = NULL;
	int scaled_width, scaled_height;
	int width = image->width;
	int height = image->height;
	unsigned* scaled_buffer;
	int mip_level_size;
	int miplevel;

	memset( upload_data, 0, sizeof( *upload_data ) );

	// Guard against a zero-dimension image: under IMGFLAG_NOSCALE the
	// power-of-two normalization is skipped, so a 0 width/height would flow
	// straight into the size math below and Hunk_AllocateTempMemory(0), with
	// downstream memset/ResampleTexture/mip-gen then operating on a 0-byte
	// buffer. Clamp to at least 1×1.
	if ( width < 1 )  width = 1;
	if ( height < 1 ) height = 1;

	if ( image->flags & IMGFLAG_NOSCALE ) {
		//
		// keep original dimensions
		//
		scaled_width = width;
		scaled_height = height;
	} else {
		//
		// convert to exact power of 2 sizes
		//
		for (scaled_width = 1 ; scaled_width < width ; scaled_width<<=1)
			;
		for (scaled_height = 1 ; scaled_height < height ; scaled_height<<=1)
			;

		if ( r_roundImagesDown->integer && scaled_width > width )
			scaled_width >>= 1;
		if ( r_roundImagesDown->integer && scaled_height > height )
			scaled_height >>= 1;
	}

	//
	// clamp to the current upper OpenGL limit
	// scale both axis down equally so we don't have to
	// deal with a half mip resampling
	//
	while ( scaled_width > glConfig.maxTextureSize
		|| scaled_height > glConfig.maxTextureSize ) {
		scaled_width >>= 1;
		scaled_height >>= 1;
	}

	upload_data->buffer = (byte*) ri.Hunk_AllocateTempMemory( 2 * 4 * scaled_width * scaled_height );
	if ( data == NULL ) {
		memset( upload_data->buffer, 0, 2 * 4 * scaled_width * scaled_height );
	}

	if ( ( scaled_width != width || scaled_height != height ) && data ) {
		resampled_buffer = (byte*) ri.Hunk_AllocateTempMemory( scaled_width * scaled_height * 4 );
		ResampleTexture ((unsigned*)data, width, height, (unsigned*)resampled_buffer, scaled_width, scaled_height);
		data = resampled_buffer;
	}

	width = scaled_width;
	height = scaled_height;

	if ( data == NULL ) {
		data = upload_data->buffer;
	} else {
		if ( image->flags & IMGFLAG_COLORSHIFT ) {
			byte *p = data;
			int i, n = width * height;
			for ( i = 0; i < n; i++, p+=4 ) {
				// IMGFLAG_COLORSHIFT is currently never set (see the
				// commented-out line in R_CreateImage for external
				// lm_XXXX atlases) — this loop is dead. If resurrected
				// it'd be for lightmap atlases, so byte-verbatim (qtrue):
				// the ×2 q3map2-overbright doubling is done in the shader
				// (LIGHTMAP_BOOST).
				R_ColorShiftLightingBytes( p, p, qfalse, qtrue );
			}
		}
	}

	//
	// perform optional picmip operation
	//
	if ( picmip && ( tr.mapLoading || r_nomip->integer == 0 ) ) {
		scaled_width >>= r_picmip->integer;
		scaled_height >>= r_picmip->integer;
		//x >>= r_picmip->integer;
		//y >>= r_picmip->integer;
	}

	//
	// clamp to minimum size
	//
	if (scaled_width < 1) {
		scaled_width = 1;
	}
	if (scaled_height < 1) {
		scaled_height = 1;
	}

	upload_data->base_level_width = scaled_width;
	upload_data->base_level_height = scaled_height;

	if ( scaled_width == width && scaled_height == height && !mipmap ) {
		upload_data->mip_levels = 1;
		upload_data->buffer_size = scaled_width * scaled_height * 4;

		if ( data != NULL ) {
			memcpy( upload_data->buffer, data, upload_data->buffer_size );
		}

		if ( resampled_buffer != NULL ) {
			ri.Hunk_FreeTempMemory( resampled_buffer );
		}

		return;	//return upload_data;
	}

	// source-content classification for sRGB-aware
	// mipmap averaging. Color content (no NOLIGHTSCALE, OR LIGHTMAP)
	// gets the decode/average/encode round-trip. Non-color content
	// (normal maps, masks, MSDF, LUTs — IMGFLAG_NOLIGHTSCALE without
	// IMGFLAG_LIGHTMAP) preserves the legacy byte-space averaging.
	// The LIGHTMAP override is required because tr_map.c's lightmap
	// flags carry both NOLIGHTSCALE and LIGHTMAP — lightmaps are
	// sRGB-encoded radiance from q3map2 and must decode/encode.
	{
		qboolean isSRGB = ( !( image->flags & IMGFLAG_NOLIGHTSCALE )
		                    || ( image->flags & IMGFLAG_LIGHTMAP ) ) ? qtrue : qfalse;

		// Use the normal mip-mapping to go down from [width, height] to [scaled_width, scaled_height] dimensions.
		while ( width > scaled_width || height > scaled_height ) {
			R_MipMap( data, data, width, height, isSRGB );

			width >>= 1;
			if ( width < 1 ) width = 1;

			height >>= 1;
			if ( height < 1 ) height = 1;
		}
	}

	// At this point width == scaled_width and height == scaled_height.

	scaled_buffer = (unsigned int*) ri.Hunk_AllocateTempMemory( sizeof( unsigned ) * scaled_width * scaled_height );
	memcpy(scaled_buffer, data, scaled_width * scaled_height * 4);

	if ( !(image->flags & IMGFLAG_NOLIGHTSCALE ) ) {
		R_LightScaleTexture( (byte*)scaled_buffer, scaled_width, scaled_height, !mipmap );
	}

	miplevel = 0;
	mip_level_size = scaled_width * scaled_height * 4;

	memcpy(upload_data->buffer, scaled_buffer, mip_level_size);
	upload_data->buffer_size = mip_level_size;

	if ( mipmap ) {
		qboolean isSRGB = ( !( image->flags & IMGFLAG_NOLIGHTSCALE )
		                    || ( image->flags & IMGFLAG_LIGHTMAP ) ) ? qtrue : qfalse;
		while (scaled_width > 1 && scaled_height > 1) {
			R_MipMap((byte *)scaled_buffer, (byte *)scaled_buffer, scaled_width, scaled_height, isSRGB);

			scaled_width >>= 1;
			if (scaled_width < 1) scaled_width = 1;

			scaled_height >>= 1;
			if (scaled_height < 1) scaled_height = 1;

			miplevel++;
			mip_level_size = scaled_width * scaled_height * 4;

			if ( r_colorMipLevels->integer ) {
				R_BlendOverTexture( (byte *)scaled_buffer, scaled_width * scaled_height, miplevel );
			}

			memcpy(&upload_data->buffer[upload_data->buffer_size], scaled_buffer, mip_level_size);
			upload_data->buffer_size += mip_level_size;
		}
	}

	upload_data->mip_levels = miplevel + 1;

	ri.Hunk_FreeTempMemory( scaled_buffer );

	if ( resampled_buffer != NULL )
		ri.Hunk_FreeTempMemory( resampled_buffer );
}


static void upload_vk_image( image_t *image, byte *pic ) {

	Image_Upload_Data upload_data;
	int w, h;

	generate_image_upload_data( image, pic, &upload_data );

	w = upload_data.base_level_width;
	h = upload_data.base_level_height;

	/* Linear (non-color) data — PBR ORM, normal maps, depth — must
	 * preserve 8-bit/channel precision. 4-5 bit packed formats
	 * destroy the scalar precision required by physical shading. */
	if ( r_textureBits->integer > 16 || r_textureBits->integer == 0
		 || ( image->flags & IMGFLAG_LIGHTMAP )
		 || !( image->flags & IMGFLAG_MIPMAP )
		 || ( image->flags & IMGFLAG_NOLIGHTSCALE ) ) {
		image->internalFormat = VK_FORMAT_R8G8B8A8_UNORM;
		//image->internalFormat = VK_FORMAT_B8G8R8A8_UNORM;
	} else {
		qboolean has_alpha = RawImage_HasAlpha( upload_data.buffer, w * h );
		image->internalFormat = has_alpha ? VK_FORMAT_B4G4R4A4_UNORM_PACK16 : VK_FORMAT_A1R5G5B5_UNORM_PACK16;
	}

	image->uploadWidth  = w;
	image->uploadHeight = h;
	image->layerCount   = 1;

	vk_create_image( image, w, h, upload_data.mip_levels );
	vk_upload_image_data( image, 0, 0, w, h, upload_data.mip_levels, upload_data.buffer, upload_data.buffer_size, qfalse, 0 );

	ri.Hunk_FreeTempMemory( upload_data.buffer );
}

/*
 * R_CreateImageArray — upload N RGBA frames as a single VK_IMAGE_VIEW_TYPE_2D_ARRAY.
 * All frames must have matching width × height. Returns NULL if inputs are invalid.
 * If numFrames == 1, delegates to R_CreateImage (no array overhead).
 */
image_t *R_CreateImageArray( const char *name, byte **frames, int numFrames, int width, int height, imgFlags_t flags ) {
	image_t    *image;
	long        hash;
	int         namelen;
	int         k;

	if ( numFrames < 1 || !frames || width <= 0 || height <= 0 )
		return NULL;
	for ( k = 0; k < numFrames; k++ ) {
		if ( !frames[k] ) return NULL;
	}

	if ( numFrames == 1 )
		return R_CreateImage( name, NULL, frames[0], width, height, flags );

	if ( tr.numImages == MAX_DRAWIMAGES ) {
		ri.Terminate( TERM_CLIENT_DROP, "R_CreateImageArray: MAX_DRAWIMAGES hit" );
	}

	namelen = (int)strlen( name ) + 1;
	image   = ri.Hunk_Alloc( sizeof( *image ) + namelen, h_low );
	image->imgName  = (char *)( image + 1 );
	image->imgName2 = image->imgName;
	strcpy( image->imgName, name );

	hash             = generateHashValue( name );
	image->next      = hashTable[hash];
	hashTable[hash]  = image;
	tr.images[tr.numImages++] = image;

	image->flags      = flags | IMGFLAG_ARRAY;
	image->width      = width;
	image->height     = height;
	image->layerCount = (uint32_t)numFrames;

	image->wrapClampMode = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	image->handle        = VK_NULL_HANDLE;
	image->view          = VK_NULL_HANDLE;
	image->descriptor    = VK_NULL_HANDLE;
	image->ral           = NULL;
	image->ralBindlessSlot     = -1;
	image->bindlessSamplerSlot = -1;

	/* Determine format from first frame */
	{
		Image_Upload_Data ud0;
		generate_image_upload_data( image, frames[0], &ud0 );

		if ( r_textureBits->integer > 16 || r_textureBits->integer == 0 || !( flags & IMGFLAG_MIPMAP ) ) {
			image->internalFormat = VK_FORMAT_R8G8B8A8_UNORM;
		} else {
			image->internalFormat = RawImage_HasAlpha( ud0.buffer, ud0.base_level_width * ud0.base_level_height )
			                        ? VK_FORMAT_B4G4R4A4_UNORM_PACK16 : VK_FORMAT_A1R5G5B5_UNORM_PACK16;
		}

		image->uploadWidth  = ud0.base_level_width;
		image->uploadHeight = ud0.base_level_height;

		vk_create_image( image, ud0.base_level_width, ud0.base_level_height, ud0.mip_levels );

		/* Layer 0: initial transition (UNDEFINED → TRANSFER_DST → SHADER_READ) */
		vk_upload_image_data( image, 0, 0, ud0.base_level_width, ud0.base_level_height,
		                      ud0.mip_levels, ud0.buffer, ud0.buffer_size, qfalse, 0 );
		ri.Hunk_FreeTempMemory( ud0.buffer );

		/* Layers 1..N-1: update transition (SHADER_READ → TRANSFER_DST → SHADER_READ) */
		for ( k = 1; k < numFrames; k++ ) {
			Image_Upload_Data ud;
			generate_image_upload_data( image, frames[k], &ud );
			vk_upload_image_data( image, 0, 0, ud.base_level_width, ud.base_level_height,
			                      ud.mip_levels, ud.buffer, ud.buffer_size, qtrue, (uint32_t)k );
			ri.Hunk_FreeTempMemory( ud.buffer );
		}
	}

	// register the 2D-array view into the parallel bindless
	// SAMPLED_IMAGE binding so q1_ls_array.frag's animArray (and any future
	// content 2D-array consumer) can sample by role index. vk_create_image
	// already populated image->bindlessSamplerSlot via vk_update_descriptor_set;
	// this only allocates the array-binding slot + writes image->view there.
	vk_ral_register_image_array( image );

	return image;
}

#else // !USE_VULKAN

static GLint RawImage_GetInternalFormat( const byte *scan, int numPixels, qboolean lightMap, qboolean allowCompression )
{
	GLint internalFormat;

	if ( lightMap )
		return GL_RGB;

	if ( RawImage_HasAlpha( scan, numPixels ) )
	{
		if ( r_textureBits->integer == 16 )
		{
			internalFormat = GL_RGBA4;
		}
		else if ( r_textureBits->integer == 32 )
		{
			internalFormat = GL_RGBA8;
		}
		else
		{
			internalFormat = GL_RGBA;
		}
	}
	else
	{
		if ( allowCompression && glConfig.textureCompression == TC_S3TC_ARB )
		{
			internalFormat = GL_COMPRESSED_RGB_S3TC_DXT1_EXT;
		}
		else if ( allowCompression && glConfig.textureCompression == TC_S3TC )
		{
			internalFormat = GL_RGB4_S3TC;
		}
		else if ( r_textureBits->integer == 16 )
		{
			internalFormat = GL_RGB5;
		}
		else if ( r_textureBits->integer == 32 )
		{
			internalFormat = GL_RGB8;
		}
		else
		{
			internalFormat = GL_RGB;
		}
	}

	return internalFormat;
}


static void LoadTexture( int miplevel, int x, int y, int width, int height, const byte *data, qboolean subImage, image_t *image )
{
	if ( subImage )
		qglTexSubImage2D( GL_TEXTURE_2D, miplevel, x, y, width, height, GL_RGBA, GL_UNSIGNED_BYTE, data );
	else
		qglTexImage2D( GL_TEXTURE_2D, miplevel, image->internalFormat, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data );
}


/*
===============
Upload32
===============
*/
static void Upload32( byte *data, int x, int y, int width, int height, image_t *image, qboolean subImage )
{
	qboolean allowCompression = !(image->flags & IMGFLAG_NO_COMPRESSION);
	qboolean lightMap = image->flags & IMGFLAG_LIGHTMAP;
	qboolean mipmap = image->flags & IMGFLAG_MIPMAP;
	qboolean picmip = image->flags & IMGFLAG_PICMIP;
	byte		*resampledBuffer = NULL;
	int			scaled_width, scaled_height;

	if ( image->flags & IMGFLAG_NOSCALE ) {
		//
		// keep original dimensions
		//
		scaled_width = width;
		scaled_height = height;
	} else {
		//
		// convert to exact power of 2 sizes
		//
		for (scaled_width = 1 ; scaled_width < width ; scaled_width<<=1)
			;
		for (scaled_height = 1 ; scaled_height < height ; scaled_height<<=1)
			;

		if ( r_roundImagesDown->integer && scaled_width > width )
			scaled_width >>= 1;
		if ( r_roundImagesDown->integer && scaled_height > height )
			scaled_height >>= 1;
	}

	//
	// clamp to the current texture size limit
	// scale both axis down equally so we don't have to
	// deal with a half mip resampling
	//
	while ( scaled_width > glConfig.maxTextureSize
		|| scaled_height > glConfig.maxTextureSize ) {
		scaled_width >>= 1;
		scaled_height >>= 1;
		x >>= 1;
		y >>= 1;
	}

	if ( scaled_width != width || scaled_height != height ) {
		if ( data ) {
			resampledBuffer = ri.Hunk_AllocateTempMemory( scaled_width * scaled_height * 4 );
			ResampleTexture( (unsigned*)data, width, height, (unsigned*)resampledBuffer, scaled_width, scaled_height );
			data = resampledBuffer;
		}
		width = scaled_width;
		height = scaled_height;
	}

	if ( image->flags & IMGFLAG_COLORSHIFT ) {
		byte *p = data;
		int i, n = width * height;
		for ( i = 0; i < n; i++, p+=4 ) {
			// dead loop (IMGFLAG_COLORSHIFT never set); see the twin in
			// the Vulkan path above. byte-verbatim.
			R_ColorShiftLightingBytes( p, p, qfalse, qtrue );
		}
	}

	//
	// perform optional picmip operation
	//
	if ( picmip && ( tr.mapLoading || r_nomip->integer == 0 ) ) {
		scaled_width >>= r_picmip->integer;
		scaled_height >>= r_picmip->integer;
		x >>= r_picmip->integer;
		y >>= r_picmip->integer;
	}

	//
	// clamp to minimum size
	//
	if (scaled_width < 1) {
		scaled_width = 1;
	}
	if (scaled_height < 1) {
		scaled_height = 1;
	}

	if ( !subImage ) {
		// verify if the alpha channel is being used or not
		if ( image->internalFormat == 0 ) {
			image->internalFormat = RawImage_GetInternalFormat( data, width*height, lightMap, allowCompression );
		}
		image->uploadWidth = scaled_width;
		image->uploadHeight = scaled_height;
	}

	// copy or resample data as appropriate for first MIP level
	if ( ( scaled_width == width ) && ( scaled_height == height ) )
	{
		if ( !mipmap )
		{
			LoadTexture( 0, x, y, scaled_width, scaled_height, data, subImage, image );
			goto done;
		}
	}
	else
	{
		qboolean isSRGB = ( !( image->flags & IMGFLAG_NOLIGHTSCALE )
		                    || ( image->flags & IMGFLAG_LIGHTMAP ) ) ? qtrue : qfalse;
		// use the normal mip-mapping function to go down from here
		while ( width > scaled_width || height > scaled_height ) {
			R_MipMap( data, data, width, height, isSRGB );
			width = MAX( 1, width >> 1 );
			height = MAX( 1, height >> 1 );
		}
	}

	if ( !(image->flags & IMGFLAG_NOLIGHTSCALE) )
		R_LightScaleTexture( data, scaled_width, scaled_height, !mipmap );

	LoadTexture( 0, x, y, scaled_width, scaled_height, data, subImage, image );

	if ( mipmap )
	{
		int	miplevel = 0;
		qboolean isSRGB = ( !( image->flags & IMGFLAG_NOLIGHTSCALE )
		                    || ( image->flags & IMGFLAG_LIGHTMAP ) ) ? qtrue : qfalse;
		while (scaled_width > 1 || scaled_height > 1)
		{
			R_MipMap( data, data, scaled_width, scaled_height, isSRGB );
			scaled_width = MAX( 1, scaled_width >> 1 );
			scaled_height = MAX( 1, scaled_height >> 1 );
			x >>= 1;
			y >>= 1;
			miplevel++;

			if ( r_colorMipLevels->integer ) {
				R_BlendOverTexture( data, scaled_width * scaled_height, miplevel );
			}

			LoadTexture( miplevel, x, y, scaled_width, scaled_height, data, subImage, image );
		}
	}
done:
	if ( resampledBuffer != NULL )
		ri.Hunk_FreeTempMemory( resampledBuffer );

	GL_CheckErrors();
}


/*
================
R_UploadSubImage
================
*/
void R_UploadSubImage( byte *data, int x, int y, int width, int height, image_t *image )
{
	if ( image )
	{
		GL_Bind( image );
		Upload32( data, x, y, width, height, image, qtrue ); // subImage = qtrue
	}
}
#endif // !USE_VULKAN


/*
================
R_ClassifyColorDomain

Block 5d: auto-classify a texture's colour domain from its filename suffix
(case-insensitive, basename before the last '.'). Channel-data maps
(normal / spec / roughness / metallic / AO / height) are CD_LINEAR (sampled
raw); everything else — including emissive, which is display content — is
CD_SRGB (decoded sRGB->linear at sample time). Matches the modern-engine
convention (Unreal SamplerType, Unity HDRP sRGB checkbox, Source 2 explicit
declaration). Overridable per shader stage via linearMap / srgbMap / gammaMap
(IMGFLAG_DOMAIN_LINEAR / IMGFLAG_DOMAIN_SRGB), handled at the call site.

Only the unambiguous long-form suffixes are auto-detected. The bare
single-letter forms (_n / _s / _r / _m / _h) are deliberately NOT in the
table: in id-tech-3-era content they are common variant markers on plain
diffuse textures (lion_m.tga, spawn3_r.tga, ...) and auto-classifying them
as CD_LINEAR mis-renders stock maps. Content that genuinely ships a
single-letter-suffixed channel map should declare it with the linearMap
shader keyword.
================
*/
static colorDomain_t R_ClassifyColorDomain( const char *name ) {
	static const char *linTok[] = {
		"_normal", "_norm", "_nrm",
		"_spec", "_specular", "_shiny", "_shiney",
		"_rough", "_roughness",
		"_metal", "_metallic",
		"_ao", "_occlusion",
		"_height", "_disp", "_displacement",
		NULL
	};
	const char	*slash, *dot, *us;
	char		base[MAX_QPATH];
	int			i, n;

	slash = strrchr( name, '/' );
	if ( slash )
		name = slash + 1;
	dot = strrchr( name, '.' );
	n = dot ? (int)( dot - name ) : (int)strlen( name );
	if ( n <= 0 || n >= (int)sizeof( base ) )
		return CD_SRGB;
	memcpy( base, name, n );
	base[n] = '\0';
	for ( i = 0; i < n; i++ )
		if ( base[i] >= 'A' && base[i] <= 'Z' )
			base[i] += 32;

	us = strrchr( base, '_' );
	if ( !us )
		return CD_SRGB;	// no trailing "_<token>" — display content
	for ( i = 0; linTok[i] != NULL; i++ )
		if ( strcmp( us, linTok[i] ) == 0 )
			return CD_LINEAR;
	// _e / _emissive / _emit and everything else → CD_SRGB (display content)
	return CD_SRGB;
}


/*
================
R_CreateImage

This is the only way any image_t are created
Picture data may be modified in-place during mipmap processing
================
*/
image_t *R_CreateImage( const char *name, const char *name2, byte *pic, int width, int height, imgFlags_t flags ) {
	image_t		*image;
	long		hash;
#ifndef USE_VULKAN
	GLint		glWrapClampMode;
	GLuint		currTexture;
	int			currTMU;
#endif
	int			namelen, namelen2;
	const char	*slash;

	namelen = (int)strlen( name ) + 1;
	if ( namelen > MAX_QPATH ) {
		ri.Terminate( TERM_CLIENT_DROP, "R_CreateImage: \"%s\" is too long", name );
	}

	if ( name2 && Q_stricmp( name, name2 ) != 0 ) {
		// leave only file name
		name2 = ( slash = strrchr( name2, '/' ) ) != NULL ? slash + 1 : name2;
		namelen2 = (int)strlen( name2 ) + 1;
	} else {
		namelen2 = 0;
	}

	if ( tr.numImages == MAX_DRAWIMAGES ) {
		ri.Terminate( TERM_CLIENT_DROP, "R_CreateImage: MAX_DRAWIMAGES hit" );
	}

	image = ri.Hunk_Alloc( sizeof( *image ) + namelen + namelen2, h_low );
	image->imgName = (char *)( image + 1 );
	strcpy( image->imgName, name );
	if ( namelen2 ) {
		image->imgName2 = image->imgName + namelen;
		strcpy( image->imgName2, name2 );
	} else {
		image->imgName2 = image->imgName;
	}

	hash = generateHashValue( name );
	image->next = hashTable[ hash ];
	hashTable[ hash ] = image;

	tr.images[ tr.numImages++ ] = image;

	// Phase 7.15.4-a class-A pin: procedural/no-disk-source built-ins (*default,
	// *white, *black, *identityLight, *dlight, *fog, *scratch*, ...) and lightmap /
	// sun-mask atlases (*mergedLightmap%d / *mergedSunMask%d, IMGFLAG_LIGHTMAP,
	// filled in-place from BSP) cannot be re-decoded from disk, so they must never
	// be evicted. They are exactly the '*'-named OR IMGFLAG_LIGHTMAP set and all
	// route through R_CreateImage, so this single site covers class A. World/content
	// textures are neither '*'-named nor lightmap-flagged, so they are NOT pinned
	// here (the over-pin failure mode). Dark in 7.15.4-a (no reader yet).
	if ( name[0] == '*' || ( flags & IMGFLAG_LIGHTMAP ) ) {
		flags |= IMGFLAG_PINNED;
	}

	image->flags      = flags;
	image->width      = width;
	image->height     = height;
	image->layerCount = 1;

	// Block 5d: colour domain — explicit shader-keyword override wins;
	// otherwise auto-classify by filename suffix. Built-in images ("*white",
	// "*default", ...) have no path/suffix → CD_SRGB, which is harmless
	// (sRGBToLinear is identity at 0 and 1).
	if ( flags & IMGFLAG_DOMAIN_LINEAR )
		image->colorDomain = CD_LINEAR;
	else if ( flags & IMGFLAG_DOMAIN_SRGB )
		image->colorDomain = CD_SRGB;
	else
		image->colorDomain = R_ClassifyColorDomain( name );

	if ( namelen > 6 && Q_stristr( image->imgName, "maps/" ) == image->imgName && Q_stristr( image->imgName + 6, "/lm_" ) != NULL ) {
		// external lightmap atlases stored in maps/<mapname>/lm_XXXX textures
		// image->flags = IMGFLAG_NOLIGHTSCALE | IMGFLAG_NO_COMPRESSION | IMGFLAG_NOSCALE | IMGFLAG_COLORSHIFT;
		image->flags |= IMGFLAG_NO_COMPRESSION | IMGFLAG_NOSCALE;
	}

#ifdef USE_VULKAN
	if ( flags & IMGFLAG_CLAMPTOBORDER )
		image->wrapClampMode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
	else if ( flags & IMGFLAG_CLAMPTOEDGE )
		image->wrapClampMode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	else
		image->wrapClampMode = VK_SAMPLER_ADDRESS_MODE_REPEAT;

	image->handle = VK_NULL_HANDLE;
	image->view = VK_NULL_HANDLE;
	image->descriptor = VK_NULL_HANDLE;
	image->ral = NULL;
	image->ralResidencyMipCount = 0;
	image->ralBindlessSlot = -1;
	image->bindlessSamplerSlot = -1;

	// parallel-paths migration. The RAL texture is registered
	// BEFORE upload_vk_image because generate_image_upload_data may mutate
	// `pic` in place (NPOT scaling, R_MipMap downsamples). Ral_TextureUpload-
	// Async copies pic into a staging buffer + waits internally, so by the
	// time it returns the pic data is safe to mutate again. Registration is
	// a no-op when r_useRALTextures=0 or the RAL infra failed to init.
	vk_ral_register_image( image, pic, width, height );

	upload_vk_image( image, pic );
#else
	if ( flags & IMGFLAG_RGB )
		image->internalFormat = GL_RGB;
	else
		image->internalFormat = 0; // autodetect

	if ( flags & IMGFLAG_CLAMPTOBORDER )
		glWrapClampMode = GL_CLAMP_TO_BORDER;
	else if ( flags & IMGFLAG_CLAMPTOEDGE )
		glWrapClampMode = gl_clamp_mode;
	else
		glWrapClampMode = GL_REPEAT;

	// save current state
	currTMU = glState.currenttmu;
	currTexture = glState.currenttextures[ glState.currenttmu ];

	qglGenTextures( 1, &image->texnum );

	// lightmaps are always allocated on TMU 1
	if ( qglActiveTextureARB && (flags & IMGFLAG_LIGHTMAP) ) {
		image->TMU = 1;
	} else {
		image->TMU = 0;
	}

	if ( qglActiveTextureARB ) {
		GL_SelectTexture( image->TMU );
	}

	GL_Bind( image );
	Upload32( pic, 0, 0, image->width, image->height, image, qfalse ); // subImage = qfalse

	if ( image->flags & IMGFLAG_MIPMAP )
	{
		if ( textureFilterAnisotropic ) {
			qglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, (GLint) maxAnisotropy );
		}

		qglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_min );
		qglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max );
	}
	else
	{
		if ( textureFilterAnisotropic )
			qglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, 1 );

		qglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
		qglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
	}

	qglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, glWrapClampMode );
	qglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, glWrapClampMode );

	// restore original state
	GL_SelectTexture( currTMU );
	glState.currenttextures[ glState.currenttmu ] = currTexture;
	qglBindTexture( GL_TEXTURE_2D, currTexture );

#endif
	return image;
}

//===================================================================

typedef struct
{
	const char *ext;
	void (*ImageLoader)( const char *, unsigned char **, int *, int * );
} imageExtToLoaderMap_t;

// Note that the ordering indicates the order of preference used
// when there are multiple images of different formats available
static const imageExtToLoaderMap_t imageLoaders[] =
{
	{ "png",  R_LoadPNG },
	{ "jpg",  R_LoadJPG },
	{ "jpeg", R_LoadJPG },
#if FEAT_LEGACY_FORMATS_IMAGE
	{ "tga",  R_LoadTGA },
	{ "pcx",  R_LoadPCX },
	{ "bmp",  R_LoadBMP },
#endif
};

static const int numImageLoaders = ARRAY_LEN( imageLoaders );

static qboolean R_ShouldPreferLegacyImages( void ) {
	char profile[16];
	int version;

	ri.Cvar_VariableStringBuffer( "com_mapAssetProfile", profile, sizeof( profile ) );
	if ( !Q_stricmp( profile, "legacy" ) ) {
		return qtrue;
	}
	if ( !Q_stricmp( profile, "modern" ) ) {
		return qfalse;
	}

	version = ri.Cvar_VariableIntegerValue( "com_mapBspVersion" );
	return ( version > 0 && ( version <= 46 || version == 68 ) ) ? qtrue : qfalse;
}

/*
=================
R_LoadImage

Loads any of the supported image types into a canonical
32 bit format.
=================
*/
static const char *R_LoadImage( const char *name, byte **pic, int *width, int *height )
{
	static char localName[ MAX_QPATH ];
	const char *altName, *ext;
	//qboolean orgNameFailed = qfalse;
	int orgLoader = -1;
	int i;

	*pic = NULL;
	*width = 0;
	*height = 0;

	Q_strncpyz( localName, name, sizeof( localName ) );

	ext = COM_GetExtension( localName );
	if ( !*ext ) {
		const char *preferredExt = R_ShouldPreferLegacyImages() ? "tga" : "png";
		for ( i = 0; i < numImageLoaders; i++ ) {
			if ( Q_stricmp( imageLoaders[i].ext, preferredExt ) ) {
				continue;
			}

			altName = va( "%s.%s", localName, imageLoaders[i].ext );
			imageLoaders[i].ImageLoader( altName, pic, width, height );
			if ( *pic ) {
				return altName;
			}

			break;
		}
	}

	if ( *ext )
	{
		// Look for the correct loader and use it
		for ( i = 0; i < numImageLoaders; i++ )
		{
			if ( !Q_stricmp( ext, imageLoaders[ i ].ext ) )
			{
				// Load
				imageLoaders[ i ].ImageLoader( localName, pic, width, height );
				break;
			}
		}

		// A loader was found
		if ( i < numImageLoaders )
		{
			if ( *pic == NULL )
			{
				// Loader failed, most likely because the file isn't there;
				// try again without the extension
				//orgNameFailed = qtrue;
				orgLoader = i;
				COM_StripExtension( name, localName, MAX_QPATH );
			}
			else
			{
				// Something loaded
				return localName;
			}
		}
	}

	// Try and find a suitable match using all
	// the image formats supported
	for ( i = 0; i < numImageLoaders; i++ )
	{
		if ( i == orgLoader )
			continue;

		altName = va( "%s.%s", localName, imageLoaders[ i ].ext );

		// Load
		imageLoaders[ i ].ImageLoader( altName, pic, width, height );

		if ( *pic )
		{
#if 0
			if ( orgNameFailed )
			{
				R_LOG( rch_assets, SEV_WARN, "WARNING: %s not present, using %s instead\n",
						name, altName );
			}
#endif
			Q_strncpyz( localName, altName, sizeof( localName ) );
			break;
		}
	}

	return localName;
}


/*
===============
R_FindImageFile

Finds or loads the given image.
Returns NULL if it fails, not a default image.
==============
*/
/* Dedup cache for mixed-flags warnings — warn once per (image, flagPair) per session. */
#define MIXED_FLAGS_CACHE_SIZE 64
typedef struct {
	unsigned int nameHash;
	imgFlags_t   storedFlags;
	imgFlags_t   requestedFlags;
	qboolean     used;
} mixedFlagsCacheEntry_t;
static mixedFlagsCacheEntry_t s_mixedFlagsCache[ MIXED_FLAGS_CACHE_SIZE ];

static qboolean MixedFlagsWarnOnce( const char *name, imgFlags_t storedFlags, imgFlags_t requestedFlags ) {
	unsigned int h = 0;
	const char *p = name;
	while ( *p ) h = h * 31 + (unsigned char)*p++;
	unsigned int slot = ( h ^ (unsigned int)storedFlags ^ (unsigned int)requestedFlags ) % MIXED_FLAGS_CACHE_SIZE;
	/* linear probe */
	for ( int i = 0; i < MIXED_FLAGS_CACHE_SIZE; i++ ) {
		int idx = ( slot + i ) % MIXED_FLAGS_CACHE_SIZE;
		mixedFlagsCacheEntry_t *e = &s_mixedFlagsCache[idx];
		if ( !e->used ) {
			e->nameHash = h; e->storedFlags = storedFlags; e->requestedFlags = requestedFlags; e->used = qtrue;
			return qtrue;
		}
		if ( e->nameHash == h && e->storedFlags == storedFlags && e->requestedFlags == requestedFlags )
			return qfalse;
	}
	return qtrue; /* cache full — allow through */
}

/*
================
R_CreateImageDDS

image_t allocation + GPU upload for DDS assets. Skips the
RGBA-byte processing chain (Upload32, r_mapSaturation mutation,
mipmap generation, format auto-detect) — the DDS file ships
pre-encoded blocks with its own mip chain. Mirrors the bookkeeping
side of R_CreateImage (hash table, tr.images[], wrapClampMode) so the
engine treats the result like any other sampleable image.

`info` carries the cubemap / volume classification from
R_LoadDDS; image->texType / layerCount / depth are populated from it
before vk_create_image (which then picks VkImageType, view type and
arrayLayers) and vk_upload_dds_image_data (which picks the upload
path). Cubemaps default to CLAMP_TO_EDGE wrapping (seamless cube
sampling still needs the cube view; edge clamp avoids visible seams
on drivers that honour it for cube faces).
================
*/
static image_t *R_CreateImageDDS( const char *name, byte *data, int width, int height, VkFormat picFormat, int numMips, int dataSize, imgFlags_t flags, const ddsImageInfo_t *info ) {
	image_t *image;
	long     hash;
	int      namelen;
	ddsImageInfo_t local2d = { TEXTYPE_2D, 1, 1 };

	if ( info == NULL )
		info = &local2d;

	namelen = (int)strlen( name ) + 1;
	if ( namelen > MAX_QPATH ) {
		ri.Terminate( TERM_CLIENT_DROP, "R_CreateImageDDS: \"%s\" is too long", name );
	}

	if ( tr.numImages == MAX_DRAWIMAGES ) {
		ri.Terminate( TERM_CLIENT_DROP, "R_CreateImageDDS: MAX_DRAWIMAGES hit" );
	}

	image = ri.Hunk_Alloc( sizeof( *image ) + namelen, h_low );
	image->imgName = (char *)( image + 1 );
	strcpy( image->imgName, name );
	image->imgName2 = image->imgName;

	hash = generateHashValue( name );
	image->next = hashTable[ hash ];
	hashTable[ hash ] = image;

	tr.images[ tr.numImages++ ] = image;

	image->flags          = flags;
	image->width          = width;
	image->height         = height;
	image->uploadWidth    = width;
	image->uploadHeight   = height;
	image->texType        = info->texType;
	image->layerCount     = ( info->layers > 1 ) ? (uint32_t)info->layers : 1;
	image->depth          = ( info->depth  > 1 ) ? info->depth : 1;
	image->internalFormat = picFormat;

	// A VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT image must be square; if a .dds
	// claims to be a cubemap but isn't, fall back to a plain 2D texture
	// rather than letting vkCreateImage fail fatally.
	if ( image->texType == TEXTYPE_CUBE && width != height ) {
		R_LOG( rch_assets, SEV_WARN, "DDS %s is a non-square cubemap (%dx%d); loading as a plain 2D texture.\n", name, width, height );
		image->texType    = TEXTYPE_2D;
		image->layerCount = 1;
		image->depth      = 1;
	}

	if ( flags & IMGFLAG_CLAMPTOBORDER )
		image->wrapClampMode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
	else if ( ( flags & IMGFLAG_CLAMPTOEDGE ) || info->texType == TEXTYPE_CUBE )
		image->wrapClampMode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	else
		image->wrapClampMode = VK_SAMPLER_ADDRESS_MODE_REPEAT;

	image->handle     = VK_NULL_HANDLE;
	image->view       = VK_NULL_HANDLE;
	image->descriptor = VK_NULL_HANDLE;
	image->ral        = NULL;
	image->ralBindlessSlot     = -1;
	image->bindlessSamplerSlot = -1;

	// validate the DDS-supplied dimensions BEFORE they reach
	// vkCreateImage. A malformed / hostile .dds whose header claims width or
	// height above glConfig.maxTextureSize (or zero), or a mip count beyond
	// log2(max(w,h))+1, would otherwise cause a VUID violation / VK_ERROR_
	// DEVICE_LOST inside vk_create_image. Reject (return NULL → caller falls
	// back to the default image) rather than trusting the file.
	if ( width <= 0 || height <= 0
	  || width  > glConfig.maxTextureSize
	  || height > glConfig.maxTextureSize ) {
		R_LOG( rch_assets, SEV_WARN, "R_CreateImageDDS: %s has out-of-range dimensions %dx%d (max %d) — rejecting\n",
			name, width, height, glConfig.maxTextureSize );
		tr.numImages--;                 // undo the tr.images[] add above
		hashTable[ hash ] = image->next;
		return NULL;
	}
	{
		int maxDim  = ( width > height ) ? width : height;
		int maxMips = 1;
		while ( ( maxDim >>= 1 ) > 0 )
			maxMips++;                  // log2(max(w,h)) + 1
		if ( numMips < 1 )
			numMips = 1;
		if ( numMips > maxMips ) {
			R_LOG( rch_assets, SEV_WARN, "R_CreateImageDDS: %s declares %d mips but %dx%d allows at most %d — clamping\n",
				name, numMips, width, height, maxMips );
			numMips = maxMips;
		}
	}

	// register the DDS image in the bindless 2D table. Every other content image
	// gets a bindless slot via vk_ral_register_image; DDS skips that (it can't go
	// through vk_ral_register_image, which forces an RGBA8 RAL texture + RGBA8
	// upload that is wrong for BC*/packed DDS bytes). Instead it shares only the
	// SLOT SOURCE — vk_ral_assign_dds_slot routes through the same free-list
	// allocator + over-capacity bookkeeping as the main path, so there is one
	// unified slot index (no raw tr.numImages-1 bypass that would collide with
	// eviction's slot-recycle). Done BEFORE vk_create_image: vk_create_image →
	// vk_update_descriptor_set then writes the real DDS-format image->view into
	// the assigned bindless slot (the SOLE main-path texture source). Over-capacity
	// leaves ralBindlessSlot=-1 and the image renders as the slot-0 / sentinel
	// fallback, same as the main path.
	if ( vk_ral_textures_available() ) {
		vk_ral_assign_dds_slot( image );
	}

	vk_create_image( image, width, height, numMips );
	vk_upload_dds_image_data( image, width, height, image->depth, numMips, data, dataSize );

	return image;
}


image_t	*R_FindImageFile( const char *name, imgFlags_t flags )
{
	image_t	*image;
	const char *localName;
	char	strippedName[ MAX_QPATH ];
	int		width, height;
	byte	*pic;
	int		hash;

	if ( !name ) {
		return NULL;
	}

	hash = generateHashValue( name );

	//
	// see if the image is already loaded
	//
	for ( image = hashTable[ hash ]; image; image = image->next ) {
		if ( !Q_stricmp( name, image->imgName ) ) {
			// the white image can be used with any set of parms, but other mismatches are errors
			if ( strcmp( name, "*white" ) != 0 ) {
				if ( image->flags != flags ) {
					if ( MixedFlagsWarnOnce( name, image->flags, flags ) )
						R_LOG( rch_assets, SEV_DEBUG, "WARNING: reused image %s with mixed flags (%i vs %i)\n", name, image->flags, flags );
				}
			}
			return image;
		}
	}

	if ( strrchr( name, '.' ) > name ) {
		// try with stripped extension
		COM_StripExtension( name, strippedName, sizeof( strippedName ) );
		for ( image = hashTable[ hash ]; image; image = image->next ) {
			if ( !Q_stricmp( strippedName, image->imgName ) ) {
				//if ( strcmp( strippedName, "*white" ) ) {
					if ( image->flags != flags ) {
						if ( MixedFlagsWarnOnce( strippedName, image->flags, flags ) )
							R_LOG( rch_assets, SEV_DEBUG, "WARNING: reused image %s with mixed flags (%i vs %i)\n", strippedName, image->flags, flags );
					}
				//}
				return image;
			}
		}
	}

	//
	// explicit .dds reference routes through the DDS loader.
	// Skip the RGBA byte-processing path entirely — DDS files ship
	// pre-encoded blocks + a mip chain. r_mapSaturation is not applied
	// (BC data isn't mutable per-pixel without decompress + recompress,
	// out of scope). Falls back to the standard loader chain if the file
	// is missing or the GPU can't use the format.
	// the loader also classifies cubemap / volume textures
	// (ddsImageInfo_t); R_CreateImageDDS turns that into the right
	// VkImage / view. IMGFLAG_CUBEMAP (set by the `cubeMap` shader keyword)
	// only records that a cubemap was expected so a mismatch can warn.
	{
		const char *ext = COM_GetExtension( name );
		if ( ext && !Q_stricmp( ext, "dds" ) ) {
			VkFormat picFormat = VK_FORMAT_UNDEFINED;
			int numMips = 1;
			int dataSize = 0;
			byte *ddsData = NULL;
			ddsImageInfo_t ddsInfo;

			R_LoadDDS( name, &ddsData, &width, &height, &picFormat, &numMips, &dataSize, &ddsInfo );
			if ( ddsData != NULL ) {
				if ( !vk_dds_format_uploadable( picFormat ) ) {
					R_LOG( rch_assets, SEV_WARN, "DDS %s uses VkFormat %d which this GPU can't use for sampling; skipping.\n",
						name, (int)picFormat );
					ri.Free( ddsData );
					return NULL;
				}
				if ( ( flags & IMGFLAG_CUBEMAP ) && ddsInfo.texType != TEXTYPE_CUBE ) {
					R_LOG( rch_assets, SEV_WARN, "DDS %s was loaded as a cubemap but the file is not a 6-face cubemap (texType %d).\n",
						name, (int)ddsInfo.texType );
				}

				image = R_CreateImageDDS( name, ddsData, width, height, picFormat, numMips, dataSize, flags, &ddsInfo );
				ri.Free( ddsData );
				return image;
			}
			// Loader returned NULL — either parse failure or file missing.
			// Drop through to the regular loader chain (which will look
			// for non-.dds substitutes).
		}
	}

	//
	// load the pic from disk
	//
	localName = R_LoadImage( name, &pic, &width, &height );
	if ( pic == NULL ) {
		return NULL;
	}

	if ( tr.mapLoading && r_mapSaturation->value != 1.0f ) {
		const float sat = r_mapSaturation->value;
		byte *img = pic;
		for ( int i = 0; i < width * height; i++, img += 4 ) {
			// Block 7 (colour closure): world texture bytes are
			// sRGB-encoded. Mixing toward grey in byte space biases the
			// result toward a too-bright grey because the sRGB curve
			// compresses midtones. Decode → mix toward linear luma →
			// re-encode — mirrors the Block 4 fog path (R_LoadFogs) and
			// the in-shader fog/grade saturation math. saturation 0 →
			// luma (grey); 1 → identity (branch excluded above); 2 →
			// super-saturation, clamped to byte range below.
			float lr = R_SRGBToLinear( (float)img[0] / 255.0f );
			float lg = R_SRGBToLinear( (float)img[1] / 255.0f );
			float lb = R_SRGBToLinear( (float)img[2] / 255.0f );
			const float luma = LUMA( lr, lg, lb );
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
			img[0] = (byte)ri;
			img[1] = (byte)gi;
			img[2] = (byte)bi;
		}
	}

	image = R_CreateImage( name, localName, pic, width, height, flags );
	ri.Free( pic );
	return image;
}


/*
===============
vk_ral_reregister_image

Phase 7.15.4-b — the reversibility leg of texture eviction. Restore an evicted-
but-still-registered image_t: re-decode its source from disk and re-create BOTH the
RAL bindless texture AND the legacy VkImage, mirroring R_CreateImage's pairing
(vk_ral_register_image + upload_vk_image). The image_t survives eviction intact
(imgName / width / height / flags / texType retained; only ral + ralBindlessSlot +
handle/view were nulled), so re-creation needs nothing eviction discarded.

Idempotent: a no-op if the image is already resident (image->ral != NULL). Pinned
images must never be evicted, so one must never reach here — defensively skip + warn
(a pinned image arriving here is a step-c victim-selection bug). Decode failure (the
asset is gone) leaves the image non-resident and logs a warning; the next bind
sentinel-declines to white (graceful, not a crash).

This is the explicit re-register used by the step-b forced-evict round-trip test and,
later, by the step-c eviction path. The automatic bind-time re-register-on-miss is
7.15.3 (folded last).
===============
*/
void vk_ral_reregister_image( image_t *image ) {
	byte       *pic = NULL;
	int         w = 0, h = 0;

	if ( !image ) return;
	if ( image->ral != NULL ) return;            // already resident — idempotent

	if ( R_ImageIsPinned( image ) ) {
		R_LOG( rch_assets, SEV_WARN, "vk_ral_reregister_image: pinned image '%s' reached re-register — should never be evicted (victim-selection bug)\n",
		        image->imgName );
		return;
	}

	// re-decode the source from disk via the retained imgName. R_LoadImage owns
	// the pic allocation; we free it after both uploads, exactly as R_CreateImage.
	R_LoadImage( image->imgName, &pic, &w, &h );
	if ( pic == NULL ) {
		R_LOG( rch_assets, SEV_WARN, "vk_ral_reregister_image: '%s' could not be re-decoded — left non-resident (bind sentinel-declines)\n",
		        image->imgName );
		return;
	}

	// The disk dimensions must match the original — the slot/descriptor + any cached
	// size assumptions depend on it. A mid-session asset resize is not expected.
	if ( w != image->width || h != image->height ) {
		R_LOG( rch_assets, SEV_WARN, "vk_ral_reregister_image: '%s' re-decoded at %dx%d but was %dx%d — refusing re-register (asset changed)\n",
		        image->imgName, w, h, image->width, image->height );
		ri.Free( pic );
		return;
	}

	// Re-create both halves, mirroring R_CreateImage's pairing (RAL first because
	// generate_image_upload_data may mutate pic in place; the RAL upload copies into
	// staging first). vk_ral_register_image is guarded on image->ral == NULL, which
	// holds here, so it runs the normal registration (free-list slot + async/sync
	// upload). upload_vk_image rebuilds the legacy VkImage + view.
	vk_ral_register_image( image, pic, w, h );
	upload_vk_image( image, pic );

	ri.Free( pic );
}

/*
===============
R_TexEvictForce_f

Phase 7.15.4-b TEST HARNESS — manual, render-thread, default-inert. Console command
`r_texEvictForce <N>`: evict the N least-recently-used UNPINNED content images, on
the render thread (command handlers run there), freeing BOTH the RAL texture (+ slot
→ free-list) AND the legacy VkImage so device memory actually drops. This is NOT the
eviction policy — there is no pressure hook, no poll thread, no hysteresis (step-c/d).
Its only purpose is to produce a real non-resident state so vk_ral_reregister_image's
round-trip is testable. Pinned images (R_ImageIsPinned) are skipped — proving the
pinned-set holds. The victim scan READS frameUsed (LRU) + R_ImageIsPinned: this is
the FIRST, MANUAL reader of both (the automatic pressure-driven reader is step-c).
===============
*/
/*
===============
R_EvictOneOldestUnpinned

Phase 7.15.4-c shared eviction primitive (render-thread only). Finds the single
oldest-frameUsed, unpinned, RAL-resident image in tr.images[] and frees BOTH its
halves: vk_ral_unregister_image (RAL texture + bindless slot → free-list) AND
vk_destroy_image_resources (legacy VkImage + view) — the load-bearing dual-free
(without the legacy half the device memory does not drop). Returns the evicted
image (for logging) or NULL when nothing evictable remains. The reader of frameUsed
+ R_ImageIsPinned. Shared by the manual r_texEvictForce harness AND the automatic
vk_ral_drain_evictions pressure path — both run on the render thread, so the scan +
free are single-threaded by construction (Option-A).
===============
*/
static ralResidencyCandidate_t R_TextureResidencyCandidate( const image_t *image,
		uint32_t resourceId, ralResidencyState_t state,
		ralResidencyTier_t tier ) {
	ralResidencyCandidate_t candidate;
	int age = 0;

	memset( &candidate, 0, sizeof( candidate ) );
	candidate.id.classId = RAL_RESIDENCY_CLASS_TEXTURE;
	candidate.id.resource = resourceId;
	candidate.id.planeMask = 1; // current adapter is one whole-texture coherence group
	candidate.state = state;
	candidate.tier = tier;
	if ( image ) {
		uint64_t budgetWidth = (uint64_t)image->uploadWidth;
		uint64_t budgetHeight = (uint64_t)image->uploadHeight;
		age = tr.frameCount - image->frameUsed;
		if ( age < 0 ) age = 0;
		candidate.ageFrames = (uint32_t)age;
		candidate.screenErrorQ8 = tier == RAL_RESIDENCY_TIER_VISIBLE ? 256u : 0u;
		candidate.sampleCount = tier == RAL_RESIDENCY_TIER_VISIBLE ? 1u : 0u;
		if ( glConfig.maxTextureSize > 0 ) {
			if ( budgetWidth > (uint64_t)glConfig.maxTextureSize ) budgetWidth = (uint64_t)glConfig.maxTextureSize;
			if ( budgetHeight > (uint64_t)glConfig.maxTextureSize ) budgetHeight = (uint64_t)glConfig.maxTextureSize;
		}
		candidate.costBytes = budgetWidth * budgetHeight * 4ull;
		candidate.pinned = R_ImageIsPinned( image ) ? 1u : 0u;
		// Whole textures do not yet have a parent mip page.  The existing white
		// bindless sentinel is their explicit coarse fallback during eviction.
		candidate.fallbackReady = 1u;
	}
	return candidate;
}

image_t *R_EvictOneOldestUnpinned( void ) {
	image_t *victim = NULL;
	ralResidencyCandidate_t victimCandidate;
	int i;

	for ( i = 0; i < tr.numImages; i++ ) {
		image_t *im = tr.images[i];
		ralResidencyCandidate_t candidate;
		ralResidencyTier_t tier;
		if ( im == NULL || im->ral == NULL ) continue;      // already evicted / never RAL-resident
		tier = im->frameUsed == tr.frameCount
			? RAL_RESIDENCY_TIER_VISIBLE : RAL_RESIDENCY_TIER_BACKGROUND;
		candidate = R_TextureResidencyCandidate( im, (uint32_t)i,
			RAL_RESIDENCY_RESIDENT, tier );
		if ( !Ral_ResidencyEvictionEligible( &candidate ) ) continue;
		if ( victim == NULL ||
		     Ral_ResidencyCompareEviction( &candidate, &victimCandidate,
				&ralResidencyDefaultPolicy ) < 0 ) {
			victim = im;
			victimCandidate = candidate;
		}
	}
	if ( victim == NULL ) return NULL;

	{
		ralResidencyScore_t score = Ral_ResidencyScore( &victimCandidate,
			&ralResidencyDefaultPolicy );
		R_LOG( rch_assets, SEV_DEBUG,
			"RAL residency select: action=evict class=texture resource=%u tier=background age=%u screenAge=%llu samples=%llu motion=%llu explicit=%llu cost=%llu total=%llu fallback=white\n",
			victimCandidate.id.resource, victimCandidate.ageFrames,
			(unsigned long long)score.screenAge,
			(unsigned long long)score.samples,
			(unsigned long long)score.motion,
			(unsigned long long)score.explicitBoost,
			(unsigned long long)score.cost,
			(unsigned long long)score.total );
	}

	// free the RAL half (texture + bindless slot → free-list)
	vk_ral_unregister_image( victim );
	// free the legacy VkImage half — the load-bearing free: without it the
	// device memory does not drop and eviction would be a no-op.
	vk_destroy_image_resources( &victim->handle, &victim->view );
	victim->handle = VK_NULL_HANDLE;
	victim->view   = VK_NULL_HANDLE;
	victim->flags |= IMGFLAG_RESIDENCY_EVICTED;
	return victim;
}

static image_t *R_SelectReregisterRequest( qboolean pendingOnly,
		const byte *skip, ralResidencyCandidate_t *outCandidate ) {
	image_t *bestImage = NULL;
	ralResidencyCandidate_t bestCandidate;
	int i;

	for ( i = 0; i < tr.numImages; i++ ) {
		image_t *candidateImage = tr.images[i];
		ralResidencyCandidate_t candidate;
		if ( skip && skip[i] ) continue;
		if ( candidateImage == NULL || candidateImage->ral != NULL ||
		     !( candidateImage->flags & IMGFLAG_RESIDENCY_EVICTED ) ||
		     R_ImageIsPinned( candidateImage ) || !candidateImage->imgName ||
		     candidateImage->imgName[0] == '*' ) continue;
		if ( pendingOnly && !( candidateImage->flags & IMGFLAG_REREGISTER_PENDING ) ) continue;
		candidate = R_TextureResidencyCandidate( candidateImage, (uint32_t)i,
			RAL_RESIDENCY_REQUESTED, RAL_RESIDENCY_TIER_VISIBLE );
		if ( !Ral_ResidencyRequestEligible( &candidate ) ) continue;
		if ( bestImage == NULL ||
		     Ral_ResidencyCompareRequest( &candidate, &bestCandidate,
				&ralResidencyDefaultPolicy ) < 0 ) {
			bestImage = candidateImage;
			bestCandidate = candidate;
		}
	}
	if ( bestImage && outCandidate ) *outCandidate = bestCandidate;
	return bestImage;
}

static void R_LogReregisterSelection( const ralResidencyCandidate_t *candidate ) {
	ralResidencyScore_t score;
	if ( !candidate ) return;
	score = Ral_ResidencyScore( candidate, &ralResidencyDefaultPolicy );
	R_LOG( rch_assets, SEV_DEBUG,
		"RAL residency select: action=request class=texture resource=%u tier=visible age=%u screenAge=%llu samples=%llu motion=%llu explicit=%llu cost=%llu total=%llu fallback=white\n",
		candidate->id.resource, candidate->ageFrames,
		(unsigned long long)score.screenAge,
		(unsigned long long)score.samples,
		(unsigned long long)score.motion,
		(unsigned long long)score.explicitBoost,
		(unsigned long long)score.cost,
		(unsigned long long)score.total );
}

/*
===============
vk_ral_drain_evictions

Phase 7.15.4-c automatic pressure-driven eviction — the render-thread DESTROY half
of Option-A. Called once per frame at the safe boundary (vk.c, right after
vk_ral_drain_pending_uploads), where the per-frame fence guarantees the prior GPU
work is complete and no command buffer is recording — the same boundary the manual
harness and the deferred-destroy drain use.

Common case (no pressure): the volatile flag is clear → return immediately, zero
cost. When the poll thread raised the flag (CRITICAL crossed), evict oldest-unpinned
victims toward a hysteresis target below the WARNING line, then clear the flag.

Memory free is FRAME-DEFERRED (step-b finding): vk_ral_unregister_image queues the
VkImage/memory destroy, and ralDeviceLocalBytes only drops once the defer-destroy
ring drains RAL_VK_MAX_FRAMES_IN_FLIGHT frames later. So this does NOT poll live
bytes mid-loop (they won't move this frame) — it evicts a COUNT estimated to reach
the target from the snapshot taken when the flag was seen, then clears the flag. If
pressure is still CRITICAL after the deferred frees land, the poller re-raises the
flag on the next transition (clean re-arm). This avoids both a busy-loop and a
thrash near the threshold.
===============
*/
void vk_ral_drain_evictions( void ) {
	uint64_t dlUsed = 0, dlBudget = 0, target;
	int      evicted = 0;

	if ( !vk_ral_evict_requested() ) return;           // common case — flag clear, zero cost
	vk_ral_clear_evict_request();                      // consume the mark (re-armed by the next CRITICAL transition)

	if ( !vk_ral_query_memory_budget( &dlUsed, &dlBudget, NULL, NULL, NULL ) || dlBudget == 0 )
		return;

	// Hysteresis target derived from the poller's thresholds (ralVk_LevelOf:
	// WARNING 75 %, CRITICAL 90 %). Evict down to WIRED_TEX_EVICT_TARGET_PERMILLE
	// (700 ‰ = 70 %, a margin below the 75 % WARNING line) so the next poll does not
	// immediately re-fire — anti-thrash. Below target → nothing to do.
	{
		uint64_t testDrop = vk_ral_get_evict_test_drop_bytes();
		if ( testDrop > 0 ) {
			// Synthetic-pressure test (default-OFF): real ~4 % pressure never reaches
			// the 70 % target, so the test forces a target of (used - N MiB) to drive
			// the REAL eviction loop. Consume the override once.
			target = ( dlUsed > testDrop ) ? ( dlUsed - testDrop ) : 0;
			vk_ral_clear_evict_test_drop();
		} else {
			target = ( dlBudget / 1000ull ) * WIRED_TEX_EVICT_TARGET_PERMILLE;
		}
	}
	if ( dlUsed <= target ) return;

	// Evict toward the target by ESTIMATED reclaim (uploadWidth*uploadHeight*4, RGBA),
	// not by polling live bytes (frame-deferred — won't move this frame). Cap the
	// per-frame batch so a huge over-budget never stalls one frame; the flag re-arms
	// on the next CRITICAL transition if more is needed.
	{
		uint64_t need = dlUsed - target;
		uint64_t freedEst = 0;
		int      budget_iters = WIRED_TEX_EVICT_MAX_PER_FRAME;
		while ( freedEst < need && budget_iters-- > 0 ) {
			image_t *v = R_EvictOneOldestUnpinned();
			if ( v == NULL ) break;                    // no more unpinned victims
			freedEst += (uint64_t)v->uploadWidth * (uint64_t)v->uploadHeight * 4ull;
			evicted++;
		}
	}

	if ( evicted > 0 ) {
		R_LOG( rch_assets, SEV_WARN, "vk_ral_drain_evictions: CRITICAL pressure — evicted %d unpinned texture(s) toward %u/%u MiB target (free is frame-deferred)\n",
		        evicted, (unsigned)( target >> 20 ), (unsigned)( dlBudget >> 20 ) );
	}
}

/*
===============
vk_ral_drain_reregisters

Phase 7.15.3 — automatic bind-miss recovery (the lazy-residency fold, render-thread).
Called once per frame at the safe boundary (vk.c, right after vk_ral_drain_evictions).
Scans for images marked IMGFLAG_REREGISTER_PENDING — set in vk_bindless_track when an
evicted texture (ral==NULL) was sampled this frame — and re-streams each via
vk_ral_reregister_image (step-b: re-decode from imgName + re-create RAL + legacy).
Lazy: only sampled-evicted textures carry the flag, so only they re-stream; an evicted
texture never sampled stays evicted. The pending flag IS the queue membership (O(1)
dedup at enqueue) — the scan is bounded by tr.numImages and only does work when
something is pending.

Per-frame cap (WIRED_TEX_REREGISTER_MAX_PER_FRAME) spreads a mass-resample over frames
— a still-pending texture stays white-sentinel (graceful) until its turn. Thrash guard:
each restored texture gets a fresh frameUsed = tr.frameCount (it was just sampled), so
it is the NEWEST, not the LRU victim — the eviction drain (step-c) picks older textures
first and won't immediately re-evict what was just re-streamed.
===============
*/
void vk_ral_drain_reregisters( void ) {
	static ralResidencyCandidate_t candidates[MAX_DRAWIMAGES];
	static image_t *images[MAX_DRAWIMAGES];
	static size_t selected[MAX_DRAWIMAGES];
	static byte selectedScratch[MAX_DRAWIMAGES];
	ralResidencyBudget_t budget;
	size_t count = 0, selectedCount, n;
	int i, restored = 0;
	uint64_t restoredBytes = 0;

	for ( i = 0; i < tr.numImages; ++i ) {
		image_t *im = tr.images[i];
		if ( im == NULL || !( im->flags & IMGFLAG_REREGISTER_PENDING ) ||
		     im->ral != NULL || !( im->flags & IMGFLAG_RESIDENCY_EVICTED ) ||
		     R_ImageIsPinned( im ) || !im->imgName || im->imgName[0] == '*' ) continue;
		images[count] = im;
		candidates[count] = R_TextureResidencyCandidate( im, (uint32_t)i,
			RAL_RESIDENCY_REQUESTED, RAL_RESIDENCY_TIER_VISIBLE );
		count++;
	}
	if ( count == 0 ) return;

	memset( &budget, 0, sizeof( budget ) );
	budget.maxPages = WIRED_TEX_REREGISTER_MAX_PER_FRAME;
	budget.maxBytes = WIRED_TEX_REREGISTER_MAX_BYTES_PER_FRAME;
	budget.minPerClass[RAL_RESIDENCY_CLASS_TEXTURE] = 1;
	selectedCount = Ral_ResidencySelectRequests( candidates, count,
		&ralResidencyDefaultPolicy, &budget, selected,
		MAX_DRAWIMAGES, selectedScratch );

	for ( n = 0; n < selectedCount; ++n ) {
		const size_t index = selected[n];
		image_t *im = images[index];
		const ralResidencyCandidate_t *chosen = &candidates[index];

		// clear the pending mark first, regardless of outcome, so a failed
		// re-decode (asset gone) does not re-enqueue every frame forever — it
		// stays white-sentinel and will only re-mark if sampled again (and fail
		// again, harmlessly). A successful re-register makes ral!=NULL so the
		// enqueue guard (ral==NULL) won't re-mark it.
		im->flags &= ~IMGFLAG_REREGISTER_PENDING;

		vk_ral_reregister_image( im );         // step-b: re-decode + re-create both halves

		if ( im->ral != NULL ) {
			// Thrash guard: the texture was just sampled, so stamp it as freshly
			// used → it is the NEWEST in the LRU, not a re-eviction victim next
			// pressure event. (vk_ral_reregister_image rebuilds the GPU texture but
			// does not touch frameUsed; set it here.)
			im->frameUsed = tr.frameCount;
			R_LogReregisterSelection( chosen );
			restored++;
			restoredBytes += chosen->costBytes;
		}
	}

	if ( restored > 0 ) {
		R_LOG( rch_assets, SEV_WARN, "vk_ral_drain_reregisters: auto-restreamed %d sampled-evicted texture(s), %u KiB within %u-page/%u-MiB frame budget\n",
			restored, (unsigned)( restoredBytes >> 10 ),
			WIRED_TEX_REREGISTER_MAX_PER_FRAME,
			WIRED_TEX_REREGISTER_MAX_BYTES_PER_FRAME >> 20 );
	}
}

void R_TexEvictForce_f( void ) {
	int       want, evicted = 0, pass;
	uint64_t  dlBefore = 0;

	if ( ri.Cmd_Argc() < 2 ) {
		R_LOG( rch_assets, SEV_INFO, "usage: r_texEvictForce <N>  (evict N oldest unpinned content textures; test harness)\n" );
		return;
	}
	want = atoi( ri.Cmd_Argv( 1 ) );
	if ( want <= 0 ) return;

	vk_ral_query_memory_budget( &dlBefore, NULL, NULL, NULL, NULL );

	// Evict N oldest unpinned via the shared primitive (same path the automatic
	// pressure drain uses).
	for ( pass = 0; pass < want; pass++ ) {
		image_t *victim = R_EvictOneOldestUnpinned();
		if ( victim == NULL ) break;                           // nothing left to evict
		R_LOG( rch_assets, SEV_DEBUG, "r_texEvictForce: evicting '%s' (frameUsed %d, age %d)\n",
		        victim->imgName, victim->frameUsed, tr.frameCount - victim->frameUsed );
		evicted++;
	}

	// NOTE: the device-local free is DEFERRED — vk_ral_unregister_image →
	// Ral_DestroyTexture → ralVk_DeferDestroy queues the VkImage/VkDeviceMemory
	// destroy keyed by frame, and ralVk_DrainPendingDestroy (driven per-frame by
	// Ral_DrainDeferred) only does the real vkFreeMemory + ralDeviceLocalBytes
	// decrement after RAL_VK_MAX_FRAMES_IN_FLIGHT frames pass (frame-fenced UAF
	// safety). So the budget queried synchronously here is unchanged; the actual
	// drop lands a few frames later (observable via the budget once the ring
	// drains — e.g. r_texReregisterAll's pre-restore reading). dlBefore is logged
	// for reference only.
	// SEV_WARN so the test harness's captured log retains it (the round-trip gate
	// is W-67-automatable off this + the r_texReregisterAll memory report).
	R_LOG( rch_assets, SEV_WARN, "r_texEvictForce: evicted %d image(s) (free is frame-deferred; device-local %u MiB at enqueue, drops after the defer-destroy ring drains)\n",
	        evicted, (unsigned)( dlBefore >> 20 ) );
}

/*
===============
R_TexReregisterAll_f

Phase 7.15.4-b TEST HARNESS — restore every evicted (ral==NULL, non-pinned, non-'*')
content image via vk_ral_reregister_image. Pairs with r_texEvictForce to exercise the
full evict → re-register round-trip (the gate). Render-thread, manual.
===============
*/
void R_TexReregisterAll_f( void ) {
	byte attempted[MAX_DRAWIMAGES];
	int restored = 0;
	uint64_t dlPre = 0, dlPost = 0;

	// This runs frames after r_texEvictForce (the harness inserts +wait between),
	// so the deferred-destroy ring has drained and ralDeviceLocalBytes now reflects
	// the actual post-evict free — the real memory-drop measurement point.
	vk_ral_query_memory_budget( &dlPre, NULL, NULL, NULL, NULL );

	memset( attempted, 0, sizeof( attempted ) );
	for ( ;; ) {
		ralResidencyCandidate_t candidate;
		image_t *im = R_SelectReregisterRequest( qfalse, attempted, &candidate );
		if ( im == NULL ) break;
		attempted[candidate.id.resource] = 1;
		vk_ral_reregister_image( im );
		if ( im->ral != NULL ) {
			R_LogReregisterSelection( &candidate );
			restored++;
		}
	}

	vk_ral_query_memory_budget( &dlPost, NULL, NULL, NULL, NULL );
	// dlPre (queried here, frames after r_texEvictForce) reflects the drained
	// post-evict footprint — the real memory-drop measurement; dlPost is after the
	// re-upload restores it. (Round-trip evidence: e.g. 509 -> 507 post-evict drop,
	// 507 -> 509 restore.)
	// SEV_WARN so the captured log retains the memory-drop evidence for the
	// W-67-automatable round-trip gate (pre = drained post-evict footprint).
	R_LOG( rch_assets, SEV_WARN, "r_texReregisterAll: restored %d image(s); device-local %u MiB (post-evict/pre-restore) -> %u MiB (post-restore)\n",
	        restored, (unsigned)( dlPre >> 20 ), (unsigned)( dlPost >> 20 ) );
}

/*
===============
R_TexResidencyBudgetTest_f

Default-inert process-gate hook. Marks every genuinely evicted whole-texture
page as requested, then invokes the SAME production budgeted drain used at the
per-frame safe boundary. Unlike R_TexReregisterAll it cannot bypass page/byte
limits or the shared selector.
===============
*/
void R_TexResidencyBudgetTest_f( void ) {
	byte requestedSet[MAX_DRAWIMAGES];
	uint64_t dlPre = 0, dlPost = 0;
	int i, requested = 0, restored = 0, pending = 0;

	memset( requestedSet, 0, sizeof( requestedSet ) );
	vk_ral_query_memory_budget( &dlPre, NULL, NULL, NULL, NULL );
	for ( i = 0; i < tr.numImages; ++i ) {
		image_t *im = tr.images[i];
		if ( im == NULL || im->ral != NULL ||
		     !( im->flags & IMGFLAG_RESIDENCY_EVICTED ) ||
		     R_ImageIsPinned( im ) || !im->imgName || im->imgName[0] == '*' ) continue;
		im->flags |= IMGFLAG_REREGISTER_PENDING;
		requestedSet[i] = 1;
		requested++;
	}

	vk_ral_drain_reregisters();
	for ( i = 0; i < tr.numImages; ++i ) {
		image_t *im = tr.images[i];
		if ( !requestedSet[i] || !im ) continue;
		if ( im->flags & IMGFLAG_REREGISTER_PENDING ) pending++;
		else if ( im->ral != NULL && !( im->flags & IMGFLAG_RESIDENCY_EVICTED ) ) restored++;
	}
	vk_ral_query_memory_budget( &dlPost, NULL, NULL, NULL, NULL );
	R_LOG( rch_assets, SEV_WARN,
		"r_texResidencyBudgetTest: requested %d, restored %d, pending %d; device-local %u MiB (post-evict/pre-restore) -> %u MiB (post-restore); budget %u pages/%u MiB\n",
		requested, restored, pending, (unsigned)( dlPre >> 20 ),
		(unsigned)( dlPost >> 20 ), WIRED_TEX_REREGISTER_MAX_PER_FRAME,
		WIRED_TEX_REREGISTER_MAX_BYTES_PER_FRAME >> 20 );
}

void R_TexResidencyMipTest_f( void ) {
	const char *action;
	if ( ri.Cmd_Argc() != 2 ) {
		R_LOG( rch_assets, SEV_INFO, "usage: r_texResidencyMipTest <hold|upload|restore>\n" );
		return;
	}
	action = ri.Cmd_Argv( 1 );
	if ( !Q_stricmp( action, "hold" ) ) {
		(void)vk_ral_residency_mip_test( qfalse );
	} else if ( !Q_stricmp( action, "upload" ) ) {
		byte       *pic = NULL;
		int         expectedWidth = 0, expectedHeight = 0;
		int         width = 0, height = 0;
		const char *source = vk_ral_residency_mip_test_source( &expectedWidth, &expectedHeight );
		if ( !source ) {
			R_LOG( rch_assets, SEV_WARN, "RAL residency mip test: upload refused (no held parent source)\n" );
			return;
		}
		R_LoadImage( source, &pic, &width, &height );
		if ( !pic ) {
			R_LOG( rch_assets, SEV_WARN, "RAL residency mip test: upload refused (decode failed for %s)\n", source );
			return;
		}
		if ( width != expectedWidth || height != expectedHeight ) {
			R_LOG( rch_assets, SEV_WARN,
			       "RAL residency mip test: upload refused (decoded source changed for %s: %dx%d expected %dx%d)\n",
			       source, width, height, expectedWidth, expectedHeight );
			ri.Free( pic );
			return;
		}
		(void)vk_ral_residency_mip_upload( pic, width, height );
		ri.Free( pic );
	} else if ( !Q_stricmp( action, "restore" ) ) {
		(void)vk_ral_residency_mip_test( qtrue );
	} else {
		R_LOG( rch_assets, SEV_INFO, "usage: r_texResidencyMipTest <hold|upload|restore>\n" );
	}
}

void R_TexResidencyMaterialTest_f( void ) {
	const char *action;
	if ( ri.Cmd_Argc() != 2 ) {
		R_LOG( rch_assets, SEV_INFO, "usage: r_texResidencyMaterialTest <hold|upload|restore>\n" );
		return;
	}
	action = ri.Cmd_Argv( 1 );
	if ( !Q_stricmp( action, "hold" ) ) {
		if ( !vk_ral_residency_material_test( qfalse ) )
			R_LOG( rch_assets, SEV_WARN, "RAL residency material: hold refused (PBR base+ORM material unavailable)\n" );
	} else if ( !Q_stricmp( action, "upload" ) ) {
		byte *pics[2] = { NULL, NULL }; int widths[2], heights[2], i;
		for ( i = 0; i < 2; ++i ) {
			int expectedWidth, expectedHeight; const char *source;
			source = vk_ral_residency_material_source( i, &expectedWidth, &expectedHeight );
			if ( !source ) break;
			R_LoadImage( source, &pics[i], &widths[i], &heights[i] );
			if ( !pics[i] || widths[i] != expectedWidth || heights[i] != expectedHeight ) break;
		}
		if ( i == 2 ) (void)vk_ral_residency_material_upload( pics, widths, heights );
		else R_LOG( rch_assets, SEV_WARN, "RAL residency material: upload refused (decode/source mismatch)\n" );
		for ( i = 0; i < 2; ++i ) if ( pics[i] ) ri.Free( pics[i] );
	} else if ( !Q_stricmp( action, "restore" ) ) {
		(void)vk_ral_residency_material_test( qtrue );
	} else {
		R_LOG( rch_assets, SEV_INFO, "usage: r_texResidencyMaterialTest <hold|upload|restore>\n" );
	}
}

/*
===============
R_TexEvictPressureTest_f

Phase 7.15.4-c SYNTHETIC-PRESSURE TEST HARNESS (default-inert). `r_texEvictPressureTest <N>`
exercises the AUTOMATIC pressure-driven path (not the direct r_texEvictForce loop):
it sets a synthetic target-drop of N MiB and raises the same evict-request flag the
poll thread raises on CRITICAL. The actual eviction then happens next frame in
vk_ral_drain_evictions, on the render thread, through the real hysteresis loop —
proving the flag→render-drain hand-off + the hysteresis target logic + the
victim-scan/dual-free, exactly as automatic CRITICAL pressure would (which never
occurs at ~4 % real usage). Render-thread caller (command handler).
===============
*/
void R_TexEvictPressureTest_f( void ) {
	int mib;
	if ( ri.Cmd_Argc() < 2 ) {
		R_LOG( rch_assets, SEV_INFO, "usage: r_texEvictPressureTest <N>  (synthetic CRITICAL pressure: drive automatic eviction to drop ~N MiB)\n" );
		return;
	}
	mib = atoi( ri.Cmd_Argv( 1 ) );
	if ( mib <= 0 ) return;
	vk_ral_set_evict_test_drop_mib( (unsigned)mib );
	vk_ral_request_eviction();   // raise the flag → vk_ral_drain_evictions runs next frame
	R_LOG( rch_assets, SEV_WARN, "r_texEvictPressureTest: synthetic CRITICAL raised (target-drop %d MiB) — automatic drain evicts next frame\n", mib );
}

/*
================
R_CreateDlightImage
================
*/
#define	DLIGHT_SIZE	16
static void R_CreateDlightImage( void ) {
	int		x,y;
	byte	data[DLIGHT_SIZE][DLIGHT_SIZE][4];
	int		b;

	// make a centered inverse-square falloff blob for dynamic lighting
	for (x=0 ; x<DLIGHT_SIZE ; x++) {
		for (y=0 ; y<DLIGHT_SIZE ; y++) {
			float	d;

			d = ( DLIGHT_SIZE/2 - 0.5f - x ) * ( DLIGHT_SIZE/2 - 0.5f - x ) +
				( DLIGHT_SIZE/2 - 0.5f - y ) * ( DLIGHT_SIZE/2 - 0.5f - y );
			b = 4000 / d;
			if (b > 255) {
				b = 255;
			} else if ( b < 75 ) {
				b = 0;
			}
			data[y][x][0] =
			data[y][x][1] =
			data[y][x][2] = b;
			data[y][x][3] = 255;
		}
	}
	tr.dlightImage = R_CreateImage( "*dlight", NULL, (byte*)data, DLIGHT_SIZE, DLIGHT_SIZE, IMGFLAG_CLAMPTOEDGE );
}


/*
=================
R_InitFogTable
=================
*/
void R_InitFogTable( void ) {
	float	d;
	float	exp;

	exp = 0.5;

	for ( int i = 0 ; i < FOG_TABLE_SIZE ; i++ ) {
		d = powf( (float)i/(FOG_TABLE_SIZE-1), exp );

		tr.fogTable[i] = d;
	}
}


/*
================
R_FogFactor

Returns a 0.0 to 1.0 fog density value
This is called for each texel of the fog texture on startup
and for each vertex of transparent shaders in fog dynamically
================
*/
float R_FogFactor( float s, float t ) {
	float	d;

	s -= 1.0/512;
	if ( s < 0 ) {
		return 0;
	}
	if ( t < 1.0/32 ) {
		return 0;
	}
	if ( t < 31.0/32 ) {
		s *= (t - 1.0f/32.0f) / (30.0f/32.0f);
	}

	// we need to leave a lot of clamp range
	s *= 8;

	if ( s > 1.0 ) {
		s = 1.0;
	}

	d = tr.fogTable[ (uint32_t)(s * (FOG_TABLE_SIZE-1)) ];

	return d;
}


/*
================
R_CreateFogImage
================
*/
#define	FOG_S	256
#define	FOG_T	32
static void R_CreateFogImage( void ) {
	int		x,y;
	byte	*data;
	float	d;

	data = ri.Hunk_AllocateTempMemory( FOG_S * FOG_T * 4 );

	// S is distance, T is depth
	for (x=0 ; x<FOG_S ; x++) {
		for (y=0 ; y<FOG_T ; y++) {
			d = R_FogFactor( ( x + 0.5f ) / FOG_S, ( y + 0.5f ) / FOG_T );

			data[(y*FOG_S+x)*4+0] =
			data[(y*FOG_S+x)*4+1] =
			data[(y*FOG_S+x)*4+2] = 255;
			data[(y*FOG_S+x)*4+3] = 255*d;
		}
	}
	tr.fogImage = R_CreateImage( "*fog", NULL, data, FOG_S, FOG_T, IMGFLAG_CLAMPTOEDGE );
	ri.Hunk_FreeTempMemory( data );
}


static int Hex( char c )
{
	if ( c >= '0' && c <= '9' ) {
		return c - '0';
	}
	if ( c >= 'A' && c <= 'F' ) {
		return 10 + c - 'A';
	}
	if ( c >= 'a' && c <= 'f' ) {
		return 10 + c - 'a';
	}

	return -1;
}


/*
==================
R_BuildDefaultImage

Create solid color texture from following input formats (hex):
#rgb
#rrggbb
==================
*/
#define	DEFAULT_SIZE 16
static qboolean R_BuildDefaultImage( const char *format ) {
	byte data[DEFAULT_SIZE][DEFAULT_SIZE][4];
	byte color[4];
	int i, len, hex[6];
	int x, y;

	if ( *format++ != '#' ) {
		return qfalse;
	}

	len = (int)strlen( format );
	if ( len <= 0 || len > 6 ) {
		return qfalse;
	}

	for ( i = 0; i < len; i++ ) {
		hex[i] = Hex( format[i] );
		if ( hex[i] == -1 ) {
			return qfalse;
		}
	}

	switch ( len ) {
		case 3: // #rgb
			color[0] = hex[0] << 4 | hex[0];
			color[1] = hex[1] << 4 | hex[1];
			color[2] = hex[2] << 4 | hex[2];
			color[3] = 255;
			break;
		case 6: // #rrggbb
			color[0] = hex[0] << 4 | hex[1];
			color[1] = hex[2] << 4 | hex[3];
			color[2] = hex[4] << 4 | hex[5];
			color[3] = 255;
			break;
		default: // unsupported format
			return qfalse;
	}

	for ( y = 0; y < DEFAULT_SIZE; y++ ) {
		for ( x = 0; x < DEFAULT_SIZE; x++ ) {
			data[x][y][0] = color[0];
			data[x][y][1] = color[1];
			data[x][y][2] = color[2];
			data[x][y][3] = color[3];
		}
	}

	tr.defaultImage = R_CreateImage( "*default", NULL, (byte *)data, DEFAULT_SIZE, DEFAULT_SIZE, IMGFLAG_MIPMAP );

	return qtrue;
}


/*
==================
R_CreateDefaultImage
==================
*/
static void R_CreateDefaultImage( void ) {
	int		x;
	byte	data[DEFAULT_SIZE][DEFAULT_SIZE][4];

	if ( r_defaultImage->string[0] )
	{
		// build from format
		if ( R_BuildDefaultImage( r_defaultImage->string ) )
			return;
		// load from external file
		tr.defaultImage = R_FindImageFile( r_defaultImage->string, IMGFLAG_MIPMAP | IMGFLAG_PICMIP );
		if ( tr.defaultImage )
			return;
	}

	// the default image will be a box, to allow you to see the mapping coordinates
	memset( data, 32, sizeof( data ) );
	for ( x = 0 ; x < DEFAULT_SIZE ; x++ ) {
		data[0][x][0] =
		data[0][x][1] =
		data[0][x][2] =
		data[0][x][3] = 255;

		data[x][0][0] =
		data[x][0][1] =
		data[x][0][2] =
		data[x][0][3] = 255;

		data[DEFAULT_SIZE-1][x][0] =
		data[DEFAULT_SIZE-1][x][1] =
		data[DEFAULT_SIZE-1][x][2] =
		data[DEFAULT_SIZE-1][x][3] = 255;

		data[x][DEFAULT_SIZE-1][0] =
		data[x][DEFAULT_SIZE-1][1] =
		data[x][DEFAULT_SIZE-1][2] =
		data[x][DEFAULT_SIZE-1][3] = 255;
	}

	tr.defaultImage = R_CreateImage( "*default", NULL, (byte *)data, DEFAULT_SIZE, DEFAULT_SIZE, IMGFLAG_MIPMAP );
}


/*
==================
R_CreateBuiltinImages
==================
*/
static void R_CreateBuiltinImages( void ) {
	int		x,y;
	byte	data[DEFAULT_SIZE][DEFAULT_SIZE][4];

	R_CreateDefaultImage();

	memset( data, 0, sizeof( data ) );
	tr.blackImage = R_CreateImage( "*black", NULL, (byte *)data, 8, 8, IMGFLAG_NONE );

	// we use a solid white image instead of disabling texturing
	memset( data, 255, sizeof( data ) );
	tr.whiteImage = R_CreateImage( "*white", NULL, (byte *)data, 8, 8, IMGFLAG_NONE );

	// in the linear pipeline, "*identityLight" is a
	// solid white 8x8 (was 0.5×white under the legacy LDR overbright
	// scheme). Used as a default sampler bind for stages without a
	// real texture — the default fragment color is pure white,
	// multiplied by whatever vertex color modulates it.
	for (x=0 ; x<DEFAULT_SIZE ; x++) {
		for (y=0 ; y<DEFAULT_SIZE ; y++) {
			data[y][x][0] = 255;
			data[y][x][1] = 255;
			data[y][x][2] = 255;
			data[y][x][3] = 255;
		}
	}

	tr.identityLightImage = R_CreateImage( "*identityLight", NULL, (byte *)data, 8, 8, IMGFLAG_NONE );

	//for ( x = 0; x < ARRAY_LEN( tr.scratchImage ); x++ ) {
		// scratchimage is usually used for cinematic drawing
		//tr.scratchImage[x] = R_CreateImage( "*scratch", (byte*)data, DEFAULT_SIZE, DEFAULT_SIZE,
		//	IMGFLAG_PICMIP | IMGFLAG_CLAMPTOEDGE | IMGFLAG_RGB );
	//}

	R_CreateDlightImage();
	R_CreateFogImage();
}


/*
===============
R_SetColorMappings
===============
*/
void R_SetColorMappings( void ) {
	int		i, j;
	float	g;
	int		inf;
	qboolean applyGamma;
	// Lifecycle freeze diagnostic: bracket this function (called from the focus-
	// gained handler) so a hard freeze pins whether it is the blocker. Gated on
	// the engine `stalltrace` cvar, read across the renderer boundary.
	int		stTrace = ri.Cvar_VariableIntegerValue( "stalltrace" );
	int64_t	stT0 = stTrace ? ri.Microseconds() : 0;

	if ( !tr.inited ) {
		// it may be called from window handling functions where gamma flags is now yet known/set
		return;
	}

	if ( stTrace )
		R_LOG( rch_assets, SEV_INFO, "STALLTRACE enter R_SetColorMappings t=%lld\n", (long long)stT0 );

	// r_brightness no longer participates in the LUT
	// build. It drives the pre-tonemap exposure_bias spec constant
	// (see vk.c) directly as a float; no log2/clamp transformation.
	// r_mapBrightness removed. R_ColorShiftLightingBytes
	// uses a fixed << 1 decode shift per Q3 BSP lightmap format spec.

	// applyGamma still gates the hardware-display LUT upload below.
	// Under r_fbo 1 the shader gamma path handles it; r_fbo 0 + no
	// hardware gamma falls back to per-texel s_gammatable upload via
	// R_LightScaleTexture.
	applyGamma = ( glConfig.deviceSupportsGamma || vk.fboActive ) ? qtrue : qfalse;

	g = r_gamma->value;

	// Pure gamma LUT (no overbright shift in the linear pipeline).
	for ( i = 0; i < ARRAY_LEN( s_gammatable ); i++ ) {
		if ( g == 1.0f ) {
			inf = i;
		} else {
			inf = 255 * powf( i/255.0f, 1.0f / g ) + 0.5f;
		}
		if (inf > 255) {
			inf = 255;
		}
		s_gammatable[i] = inf;
	}

	for ( i = 0; i < ARRAY_LEN( s_intensitytable ); i++ ) {
		j = i * r_intensity->value;
		if ( j > 255 ) {
			j = 255;
		}
		s_intensitytable[i] = j;
	}

	if ( stTrace )
		R_LOG( rch_assets, SEV_INFO, "STALLTRACE mark  R_SetColorMappings pre-GLimp_SetGamma supportsGamma=%d t=%lld\n",
			(int)gls.deviceSupportsGamma, (long long)ri.Microseconds() );

#ifdef USE_VULKAN
	if ( gls.deviceSupportsGamma ) {
		if ( vk.fboActive )
			ri.GLimp_SetGamma( s_gammatable_linear, s_gammatable_linear, s_gammatable_linear );
		else {
			if ( applyGamma ) {
				ri.GLimp_SetGamma( s_gammatable, s_gammatable, s_gammatable );
			}
		}
	}
#else
	if ( gls.deviceSupportsGamma ) {
		if ( applyGamma ) {
			ri.GLimp_SetGamma( s_gammatable, s_gammatable, s_gammatable );
		}
	}
#endif

	if ( stTrace )
		R_LOG( rch_assets, SEV_INFO, "STALLTRACE exit  R_SetColorMappings dt=%lldus\n",
			(long long)( ri.Microseconds() - stT0 ) );
}


/*
===============
R_InitImages
===============
*/
void R_InitImages( void ) {

	// R_Init() runs the prior-session cleanup (R_DeleteTextures +
	// vk_release_resources + ri.FreeAll) ahead of its memset(&tr,...) so
	// tr.numImages is guaranteed to be 0 here.  We still call ri.FreeAll
	// to clear any zone state R_Init didn't already drop.
	ri.FreeAll();

#ifdef USE_VULKAN
	// RAL backend bringup moved into vk_initialize's tail
	// (Option A — shared VkDevice with renderervk). Backend internal
	// allocations are on stdlib malloc/free since PART C, so the ri.FreeAll
	// above no longer wipes them and we don't need to re-init here.

	// initialize linear gamma table before setting color mappings for the first time
	for ( int i = 0; i < 256; i++ )
		s_gammatable_linear[i] = (unsigned char)i;
#endif

	memset( hashTable, 0, sizeof( hashTable ) );

	// build brightness translation tables
	R_SetColorMappings();

	// create default texture and white texture
	R_CreateBuiltinImages();

#ifdef USE_VULKAN
	// bind the black sun-mask decline sentinel into reserved
	// bindless slot 4094 now that tr.blackImage exists. Re-asserted here on
	// every R_InitImages so vid_restart (which recreates both tr.blackImage
	// and the bindless set) re-establishes it.
	vk_register_black_sentinel();
	// twin: the white unused-texture-role sentinel into reserved
	// bindless slot 4093 now that tr.whiteImage exists.
	vk_register_white_sentinel();
	vk_update_post_process_pipelines();
#endif
}


/*
===============
R_DeleteTextures
===============
*/
void R_DeleteTextures( void ) {
	int i;

	if ( tr.numImages == 0 ) {
		return;
	}

#ifdef USE_VULKAN
	vk_wait_idle();

	for ( i = 0; i < tr.numImages; i++ ) {
		image_t *img = tr.images[ i ];
		// tear down the parallel RAL texture (if any) FIRST so the
		// bindless slot is cleared while the RAL texture is still alive. Order
		// against the legacy VkImage destroy doesn't matter (different VkDevice)
		// but conceptually pairs with the registration order in R_CreateImage.
		vk_ral_unregister_image( img );
		vk_destroy_image_resources( &img->handle, &img->view );

		// img->descriptor will be released with pool reset
	}
	// The unregister loop above pushed every slot onto the bindless free-list.
	// tr.numImages is about to reset to 0, so the next registration pass must
	// restart at slot 0 — reset the allocator to keep the slot handout sequential
	// and byte-identical to the old tr.numImages-1 source (Phase 7.15.2). Per-
	// texture eviction (7.15.4) frees individual slots WITHOUT a bulk reset; that
	// is the case the free-list is built for.
	vk_ral_reset_bindless_slots();
#else
	for ( i = 0; i < tr.numImages; i++ ) {
		image_t *img = tr.images[ i ];
		qglDeleteTextures( 1, &img->texnum );
	}

	if ( qglActiveTextureARB ) {
		for ( i = glConfig.numTextureUnits - 1; i >= 0; i-- ) {
			qglActiveTextureARB( GL_TEXTURE0_ARB + i );
			qglBindTexture( GL_TEXTURE_2D, 0 );
		}
	} else {
		qglBindTexture( GL_TEXTURE_2D, 0 );
	}
#endif

	memset( tr.images, 0, sizeof( tr.images ) );
	memset( tr.scratchImage, 0, sizeof( tr.scratchImage ) );
	tr.numImages = 0;

	memset( glState.currenttextures, 0, sizeof( glState.currenttextures ) );
}


/*
============================================================================

SKINS

============================================================================
*/

/*
==================
CommaParse

This is unfortunate, but the skin files aren't
compatible with our normal parsing rules.
==================
*/
static char *CommaParse( const char **data_p ) {
	int c, len;
	const char *data;
	static char com_token[ MAX_TOKEN_CHARS ];

	data = *data_p;
	com_token[0] = '\0';

	// make sure incoming data is valid
	if ( !data ) {
		*data_p = NULL;
		return com_token;
	}

	len = 0;

	while ( 1 ) {
		// skip whitespace
		 while ( (c = (byte)*data) <= ' ' ) {
			if ( c == '\0' ) {
				break;
			}
			data++;
		}

		c = (byte)*data;

		// skip double slash comments
		if ( c == '/' && data[1] == '/' )
		{
			data += 2;
			while ( *data && *data != '\n' ) {
				data++;
			}
		}
		// skip /* */ comments
		else if ( c == '/' && data[1] == '*' )
		{
			data += 2;
			while ( *data && ( *data != '*' || data[1] != '/' ) )
			{
				data++;
			}
			if ( *data )
			{
				data += 2;
			}
		}
		else
		{
			break;
		}
	}

	if ( c == '\0' ) {
		return "";
	}

	// handle quoted strings
	if ( c == '\"' )
	{
		data++;
		while (1)
		{
			c = (byte)*data;
			if ( c == '\"' || c == '\0' )
			{
				if ( c == '\"' )
					data++;
				com_token[ len ] = '\0';
				*data_p = data;
				return com_token;
			}
			data++;
			if ( len < MAX_TOKEN_CHARS-1 )
			{
				com_token[ len ] = c;
				len++;
			}
		}
	}

	// parse a regular word
	do
	{
		if ( len < MAX_TOKEN_CHARS-1 )
		{
			com_token[ len ] = c;
			len++;
		}
		data++;
		c = (byte)*data;
	} while ( c > ' ' && c != ',' );

	com_token[ len ] = '\0';

	*data_p = data;
	return com_token;
}


/*
===============
RE_RegisterSkin
===============
*/
qhandle_t RE_RegisterSkin( const char *name ) {
	skinSurface_t parseSurfaces[MAX_SKIN_SURFACES];
	qhandle_t	hSkin;
	skin_t		*skin;
	skinSurface_t	*surf;
	union {
		char *c;
		void *v;
	} text;
	const char	*text_p;
	const char	*token;
	char		surfName[MAX_QPATH];
	int			totalSurfaces;

	if ( !name || !name[0] ) {
		R_LOG( rch_assets, SEV_DEBUG, "Empty name passed to RE_RegisterSkin\n" );
		return 0;
	}

	if ( strlen( name ) >= MAX_QPATH ) {
		R_LOG( rch_assets, SEV_DEBUG, "Skin name exceeds MAX_QPATH\n" );
		return 0;
	}


	// see if the skin is already loaded
	for ( hSkin = 1; hSkin < tr.numSkins ; hSkin++ ) {
		skin = tr.skins[hSkin];
		if ( !Q_stricmp( skin->name, name ) ) {
			if( skin->numSurfaces == 0 ) {
				return 0;		// default skin
			}
			return hSkin;
		}
	}

	// allocate a new skin
	if ( tr.numSkins == MAX_SKINS ) {
		R_LOG( rch_assets, SEV_WARN, "WARNING: RE_RegisterSkin( '%s' ) MAX_SKINS hit\n", name );
		return 0;
	}
	tr.numSkins++;
	skin = ri.Hunk_Alloc( sizeof( skin_t ), h_low );
	tr.skins[hSkin] = skin;
	Q_strncpyz( skin->name, name, sizeof( skin->name ) );
	skin->numSurfaces = 0;

	// If not a .skin file, load as a single shader
	if ( strcmp( name + strlen( name ) - 5, ".skin" ) != 0 ) {
		skin->numSurfaces = 1;
		skin->surfaces = ri.Hunk_Alloc( sizeof( skinSurface_t ), h_low );
		skin->surfaces[0].shader = R_FindShader( name, LIGHTMAP_NONE, qtrue );
		return hSkin;
	}

	// load and parse the skin file
	ri.FS_ReadFile( name, &text.v );
	if ( !text.c ) {
		return 0;
	}

	totalSurfaces = 0;
	text_p = text.c;
	while ( text_p && *text_p ) {
		// get surface name
		token = CommaParse( &text_p );
		Q_strncpyz( surfName, token, sizeof( surfName ) );

		if ( !token[0] ) {
			break;
		}
		// lowercase the surface name so skin compares are faster
		Q_strlwr( surfName );

		if ( *text_p == ',' ) {
			text_p++;
		}

		if ( strstr( token, "tag_" ) ) {
			continue;
		}

		// parse the shader name
		token = CommaParse( &text_p );

		if ( skin->numSurfaces < MAX_SKIN_SURFACES ) {
			surf = &parseSurfaces[skin->numSurfaces];
			Q_strncpyz( surf->name, surfName, sizeof( surf->name ) );
			surf->nameHash = Q_HashSurfaceName( surf->name );
			surf->shader = R_FindShader( token, LIGHTMAP_NONE, qtrue );
			skin->numSurfaces++;
		}

		totalSurfaces++;
	}

	ri.FS_FreeFile( text.v );

	if ( totalSurfaces > MAX_SKIN_SURFACES ) {
		R_LOG( rch_assets, SEV_WARN, "WARNING: Ignoring excess surfaces (found %d, max is %d) in skin '%s'!\n",
					totalSurfaces, MAX_SKIN_SURFACES, name );
	}

	// never let a skin have 0 shaders
	if ( skin->numSurfaces == 0 ) {
		return 0;		// use default skin
	}

	// copy surfaces to skin
	skin->surfaces = ri.Hunk_Alloc( skin->numSurfaces * sizeof( skinSurface_t ), h_low );
	memcpy( skin->surfaces, parseSurfaces, skin->numSurfaces * sizeof( skinSurface_t ) );

	return hSkin;
}


/*
===============
R_InitSkins
===============
*/
void	R_InitSkins( void ) {
	skin_t		*skin;

	tr.numSkins = 1;

	// make the default skin have all default shaders
	skin = tr.skins[0] = ri.Hunk_Alloc( sizeof( skin_t ), h_low );
	Q_strncpyz( skin->name, "<default skin>", sizeof( skin->name )  );
	skin->numSurfaces = 1;
	skin->surfaces = ri.Hunk_Alloc( sizeof( skinSurface_t ), h_low );
	skin->surfaces[0].shader = tr.defaultShader;
}


/*
===============
R_GetSkinByHandle
===============
*/
skin_t	*R_GetSkinByHandle( qhandle_t hSkin ) {
	if ( hSkin < 1 || hSkin >= tr.numSkins ) {
		return tr.skins[0];
	}
	return tr.skins[ hSkin ];
}


/*
===============
R_SkinList_f
===============
*/
void	R_SkinList_f( void ) {
	int			i, j;
	skin_t		*skin;

	R_LOG( rch_assets, SEV_INFO, "------------------\n");

	for ( i = 0 ; i < tr.numSkins ; i++ ) {
		skin = tr.skins[i];

		R_LOG( rch_assets, SEV_INFO, "%3i:%s (%d surfaces)\n", i, skin->name, skin->numSurfaces );
		for ( j = 0 ; j < skin->numSurfaces ; j++ ) {
			R_LOG( rch_assets, SEV_INFO, "       %s = %s\n",
				skin->surfaces[j].name, skin->surfaces[j].shader->name );
		}
	}
	R_LOG( rch_assets, SEV_INFO, "------------------\n");
}
