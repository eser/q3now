// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 1999-2005 Id Software, Inc.
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
// tr_init.c -- functions that are not called every frame

#include "tr_local.h"
#include "../renderercommon/r_log.h"  // rilog-channel-mechanism Turn C — renderer.init / .gl / .cmd

R_LOG_DECLARE_CHANNEL( rch_init, "renderer.init" );
R_LOG_DECLARE_CHANNEL( rch_gl,   "renderer.gl"   );
R_LOG_DECLARE_CHANNEL( rch_cmd,  "renderer.cmd"  );

#include "tr_dsa.h"

glconfig_t  glConfig;
glRefConfig_t glRefConfig;
qboolean    textureFilterAnisotropic = qfalse;
int         maxAnisotropy = 0;

glstate_t	glState;

static void GfxInfo_f( void );
static void GfxMemInfo_f( void );

cvar_t	*r_flareSize;
cvar_t	*r_flareFade;
cvar_t	*r_flareCoeff;

#if FEAT_FOG_SYSTEM
cvar_t	*r_useGlFog;
cvar_t	*r_defaultFogParmsType;
cvar_t	*r_globalLinearFogDrawSky;
#endif

cvar_t	*r_ignore;

cvar_t	*r_detailTextures;

cvar_t	*r_znear;
cvar_t	*r_zproj;
cvar_t	*r_stereoSeparation;

cvar_t	*r_skipBackEnd;

cvar_t	*r_anaglyphMode;

cvar_t	*r_greyscale;

cvar_t	*r_ignorehwgamma;
cvar_t	*r_measureOverdraw;

cvar_t  *r_teleporterFlash;

cvar_t	*r_fastsky;
cvar_t	*r_drawSun;
cvar_t	*r_dynamiclight;
cvar_t	*r_dlightBacks;

cvar_t	*r_lodbias;
cvar_t	*r_lodscale;

cvar_t	*r_norefresh;
cvar_t	*r_drawEntities;
cvar_t	*r_drawWorld;
cvar_t	*r_speeds;
cvar_t	*r_fullbright;
cvar_t	*r_novis;
cvar_t	*r_nocull;
cvar_t	*r_facePlaneCull;
cvar_t	*r_showCluster;
cvar_t	*r_nocurves;

cvar_t	*r_allowExtensions;

cvar_t	*r_ext_compressed_textures;
cvar_t	*r_ext_multitexture;
cvar_t	*r_ext_compiled_vertex_array;
cvar_t	*r_ext_texture_env_add;
cvar_t	*r_ext_texture_filter_anisotropic;
cvar_t	*r_ext_max_anisotropy;

cvar_t  *r_ext_framebuffer_object;
cvar_t  *r_ext_texture_float;
cvar_t  *r_ext_framebuffer_multisample;
cvar_t  *r_arb_seamless_cube_map;
cvar_t  *r_arb_vertex_array_object;
cvar_t  *r_ext_direct_state_access;

cvar_t  *r_cameraExposure;

cvar_t  *r_externalGLSL;

cvar_t  *r_hdr;
cvar_t  *r_floatLightmap;
cvar_t  *r_postProcess;

cvar_t  *r_toneMap;
cvar_t  *r_forceToneMap;
cvar_t  *r_forceToneMapMin;
cvar_t  *r_forceToneMapAvg;
cvar_t  *r_forceToneMapMax;

cvar_t  *r_autoExposure;
cvar_t  *r_forceAutoExposure;
cvar_t  *r_forceAutoExposureMin;
cvar_t  *r_forceAutoExposureMax;

cvar_t  *r_depthPrepass;
cvar_t  *r_ssao;

cvar_t  *r_normalMapping;
cvar_t  *r_specularMapping;
cvar_t  *r_deluxeMapping;
cvar_t  *r_parallaxMapping;
cvar_t  *r_parallaxMapOffset;
cvar_t  *r_parallaxMapShadows;
cvar_t  *r_cubeMapping;
cvar_t  *r_cubemapSize;
cvar_t  *r_deluxeSpecular;
cvar_t  *r_pbr;
cvar_t  *r_baseNormalX;
cvar_t  *r_baseNormalY;
cvar_t  *r_baseParallax;
cvar_t  *r_baseSpecular;
cvar_t  *r_baseGloss;
cvar_t  *r_glossType;
cvar_t  *r_mergeLightmaps;
cvar_t  *r_lightmapAtlas;
cvar_t  *r_loadingFpsCap;
cvar_t  *r_dlightMode;
cvar_t  *r_pshadowDist;
cvar_t  *r_imageUpsample;
cvar_t  *r_imageUpsampleMaxSize;
cvar_t  *r_imageUpsampleType;
cvar_t  *r_genNormalMaps;
cvar_t  *r_forceSun;
cvar_t  *r_forceSunLightScale;
cvar_t  *r_forceSunAmbientScale;
cvar_t  *r_sunlightMode;
cvar_t  *r_drawSunRays;
cvar_t  *r_sunShadows;
cvar_t  *r_shadowFilter;
cvar_t  *r_shadowBlur;
cvar_t  *r_shadowMapSize;
cvar_t  *r_shadowCascadeZNear;
cvar_t  *r_shadowCascadeZFar;
cvar_t  *r_shadowCascadeZBias;
cvar_t  *r_ignoreDstAlpha;

cvar_t	*r_ignoreGLErrors;
cvar_t	*r_logFile;

//cvar_t	*r_stencilBits;
cvar_t	*r_textureBits;
cvar_t  *r_ext_multisample;

cvar_t	*r_drawBuffer;
cvar_t	*r_lightmap;
cvar_t	*r_vertexLight;
cvar_t	*r_shadows;
cvar_t	*r_flares;
cvar_t	*r_nobind;
cvar_t	*r_singleShader;
cvar_t	*r_roundImagesDown;
cvar_t	*r_colorMipLevels;
cvar_t	*r_picmip;
cvar_t	*r_showTris;
cvar_t	*r_showSky;
cvar_t	*r_showNormals;
cvar_t	*r_finish;
cvar_t	*r_clear;
cvar_t	*r_textureMode;
cvar_t	*r_offsetFactor;
cvar_t	*r_offsetUnits;
cvar_t	*r_gamma;
cvar_t	*r_intensity;
cvar_t	*r_lockpvs;
cvar_t	*r_noportals;
cvar_t	*r_portalOnly;

cvar_t	*r_subdivisions;
cvar_t	*r_lodCurveError;

cvar_t	*r_brightness;
cvar_t	*r_mapBrightness;

cvar_t	*r_debugSurface;
cvar_t	*r_simpleMipMaps;

cvar_t	*r_showImages;

cvar_t	*r_ambientScale;
cvar_t	*r_directedScale;
cvar_t	*r_debugLight;
cvar_t	*r_debugSort;
cvar_t	*r_printShaders;
cvar_t	*r_saveFontData;

cvar_t	*r_marksOnTriangleMeshes;

cvar_t	*r_aviMotionJpegQuality;
cvar_t	*r_screenshotJpegQuality;

static cvar_t *r_maxpolys;
static cvar_t* r_maxpolyverts;
int		max_polys;
int		max_polyverts;


// for modular renderer
#ifdef USE_RENDERER_DLOPEN
void QDECL Com_Log_Impl( log_severity_t severity, int channel, const char *fmt, ... )
{
	char buf[ MAXPRINTMSG ];
	va_list	argptr;
	(void)channel;  // renderer DLL routes everything through ri.Log → "renderer"
	va_start( argptr, fmt );
	vsnprintf( buf, sizeof( buf ), fmt, argptr );
	va_end( argptr );
	R_LOG( rch_init,severity, "%s", buf );
}

// Stub for q_shared.c's LOG_CH expansion. The renderer DLL has no access
// to the engine's channel registry; everything is routed through ri.Log to
// the top-level "renderer" channel anyway, so the returned id is unused.
int Log_GetChannel( const char *name )
{
	(void)name;
	return 0;
}

void NORETURN QDECL Com_Terminate( terminationReason_t reason, const char *fmt, ... )
{
	char buf[ MAXPRINTMSG ];
	va_list	argptr;
	va_start( argptr, fmt );
	vsnprintf( buf, sizeof( buf ), fmt, argptr );
	va_end( argptr );
	ri.Terminate( reason, "%s", buf );
}
#endif


/*
** InitOpenGL
**
** This function is responsible for initializing a valid OpenGL subsystem.  This
** is done by calling GLimp_Init (which gives us a working OGL subsystem) then
** setting variables, checking GL constants, and reporting the gfx system config
** to the user.
*/
static void InitOpenGL( void )
{
	char renderer_buffer[1024];

	//
	// initialize OS specific portions of the renderer
	//
	// GLimp_Init directly or indirectly references the following cvars:
	//		- r_fullscreen
	//		- r_mode
	//		- r_(color|depth|stencil)bits
	//		- r_ignorehwgamma
	//		- r_gamma
	//

	if ( glConfig.vidWidth == 0 )
	{
		GLint		temp;

		if ( !ri.GLimp_Init )
		{
			ri.Terminate( TERM_UNRECOVERABLE, "OpenGL interface is not initialized" );
		}

		ri.GLimp_Init( &glConfig );
		GLimp_InitExtraExtensions();

		glConfig.textureEnvAddAvailable = qtrue;

		strcpy( renderer_buffer, glConfig.renderer_string );
		Q_strlwr( renderer_buffer );

		// OpenGL driver constants
		qglGetIntegerv( GL_MAX_TEXTURE_SIZE, &temp );
		glConfig.maxTextureSize = temp;

		// stubbed or broken drivers may have reported 0...
		if ( glConfig.maxTextureSize <= 0 )
		{
			glConfig.maxTextureSize = 0;
		}

		ri.CL_SetScaling( 1.0, glConfig.vidWidth, glConfig.vidHeight );

		qglGetIntegerv( GL_MAX_TEXTURE_IMAGE_UNITS, &temp );
		glConfig.numTextureUnits = temp;

		// reserve 160 components for other uniforms
		qglGetIntegerv( GL_MAX_VERTEX_UNIFORM_COMPONENTS, &temp );
#if FEAT_IQM
		glRefConfig.glslMaxAnimatedBones = Com_Clamp( 0, IQM_MAX_JOINTS, ( temp - 160 ) / 16 );
		if ( glRefConfig.glslMaxAnimatedBones < 12 ) {
			glRefConfig.glslMaxAnimatedBones = 0;
		}
#else
		glRefConfig.glslMaxAnimatedBones = 0;
#endif // FEAT_IQM
	}

	// check for GLSL function textureCubeLod()
	if ( r_cubeMapping->integer && !QGL_VERSION_ATLEAST( 3, 0 ) ) {
		R_LOG( rch_init, SEV_WARN, "WARNING: Disabled r_cubeMapping because it requires OpenGL 3.0\n" );
		ri.Cvar_Set( "r_cubeMapping", "0" );
	}

	// set default state
	GL_SetDefaultState();
}


/*
==================
GL_CheckErrors
==================
*/
void GL_CheckErrs( char *file, int line ) {
	int		err;
	char	s[64];

	err = qglGetError();
	if ( err == GL_NO_ERROR ) {
		return;
	}
	if ( r_ignoreGLErrors->integer ) {
		return;
	}
	switch( err ) {
		case GL_INVALID_ENUM:
			strcpy( s, "GL_INVALID_ENUM" );
			break;
		case GL_INVALID_VALUE:
			strcpy( s, "GL_INVALID_VALUE" );
			break;
		case GL_INVALID_OPERATION:
			strcpy( s, "GL_INVALID_OPERATION" );
			break;
		case GL_STACK_OVERFLOW:
			strcpy( s, "GL_STACK_OVERFLOW" );
			break;
		case GL_STACK_UNDERFLOW:
			strcpy( s, "GL_STACK_UNDERFLOW" );
			break;
		case GL_OUT_OF_MEMORY:
			strcpy( s, "GL_OUT_OF_MEMORY" );
			break;
		default:
			Com_sprintf( s, sizeof(s), "%i", err);
			break;
	}

	ri.Terminate( TERM_UNRECOVERABLE, "GL_CheckErrors: %s in %s at line %d", s , file, line);
}


/*
==============================================================================

						SCREEN SHOTS

NOTE TTimo
some thoughts about the screenshots system:
screenshots get written in fs_homepath + fs_gamedir
q3now .. base/screenshots/ *.png

one command: "screenshot" (see renderercommon/tr_screenshot.c for the
token grammar — png|jpg|bmp|tga|clipboard|silent|levelshot|<filename>)
we use statics to store a count and start writing the first screenshot/screenshot????.tga (.jpg) available
(with FS_FileExists / FS_FOpenFileWrite calls)
FIXME: the statics don't get a reinit between fs_game changes

==============================================================================
*/

/*
==================
RB_ReadPixels

Reads an image but takes care of alignment issues for reading RGB images.

Reads a minimum offset for where the RGB data starts in the image from
integer stored at pointer offset. When the function has returned the actual
offset was written back to address offset. This address will always have an
alignment of packAlign to ensure efficient copying.

Stores the length of padding after a line of pixels to address padlen

Return value must be freed with ri.Hunk_FreeTempMemory()
==================
*/

static byte *RB_ReadPixels(int x, int y, int width, int height, size_t *offset, int *padlen)
{
	byte *buffer, *bufstart;
	int padwidth, linelen;
	GLint packAlign;

	qglGetIntegerv(GL_PACK_ALIGNMENT, &packAlign);

	linelen = width * 3;
	padwidth = PAD(linelen, packAlign);

	// Allocate a few more bytes so that we can choose an alignment we like
	buffer = ri.Hunk_AllocateTempMemory(padwidth * height + *offset + packAlign - 1);

	bufstart = PADP((intptr_t) buffer + *offset, packAlign);

	qglReadPixels(x, y, width, height, GL_RGB, GL_UNSIGNED_BYTE, bufstart);

	*offset = bufstart - buffer;
	*padlen = padwidth - linelen;

	return buffer;
}

/*
==================
RB_TakeScreenshot
==================
*/
static void RB_TakeScreenshot(int x, int y, int width, int height, const char *fileName)
{
	byte *allbuf, *buffer;
	byte *srcptr, *destptr;
	byte *endline, *endmem;
	byte temp;

	int linelen, padlen;
	size_t offset = 18, memcount;

	allbuf = RB_ReadPixels(x, y, width, height, &offset, &padlen);
	buffer = allbuf + offset - 18;

	memset (buffer, 0, 18);
	buffer[2] = 2;		// uncompressed type
	buffer[12] = width & 255;
	buffer[13] = width >> 8;
	buffer[14] = height & 255;
	buffer[15] = height >> 8;
	buffer[16] = 24;	// pixel size

	// swap rgb to bgr and remove padding from line endings
	linelen = width * 3;

	srcptr = destptr = allbuf + offset;
	endmem = srcptr + (linelen + padlen) * height;

	while(srcptr < endmem)
	{
		endline = srcptr + linelen;

		while(srcptr < endline)
		{
			temp = srcptr[0];
			*destptr++ = srcptr[2];
			*destptr++ = srcptr[1];
			*destptr++ = temp;

			srcptr += 3;
		}

		// Skip the pad
		srcptr += padlen;
	}

	memcount = linelen * height;

	// gamma correction
	if(glConfig.deviceSupportsGamma)
		R_GammaCorrect(allbuf + offset, memcount);

	ri.FS_WriteFile(fileName, buffer, memcount + 18);

	ri.Hunk_FreeTempMemory(allbuf);
}

/*
==================
RB_TakeScreenshotJPEG
==================
*/

static void RB_TakeScreenshotJPEG(int x, int y, int width, int height, const char *fileName)
{
	byte *buffer;
	size_t offset = 0, memcount;
	int padlen;

	buffer = RB_ReadPixels(x, y, width, height, &offset, &padlen);
	memcount = (width * 3 + padlen) * height;

	// gamma correction
	if(glConfig.deviceSupportsGamma)
		R_GammaCorrect(buffer + offset, memcount);

	ri.CL_SaveJPG(fileName, r_screenshotJpegQuality->integer, width, height, buffer + offset, padlen);
	ri.Hunk_FreeTempMemory(buffer);
}

static void FillBMPHeader( byte *buffer, int width, int height, int memcount, int header_size )
{
	int filesize;
	memset( buffer, 0, header_size );

	// bitmap file header
	buffer[0] = 'B';
	buffer[1] = 'M';
	filesize = memcount + header_size;
	buffer[2] = (filesize >> 0) & 255;
	buffer[3] = (filesize >> 8) & 255;
	buffer[4] = (filesize >> 16) & 255;
	buffer[5] = (filesize >> 24) & 255;
	buffer[10] = header_size; // data offset

	// bitmap info header
	buffer[14] = 40; // size of this header
	buffer[18] = (width >> 0) & 255;
	buffer[19] = (width >> 8) & 255;
	buffer[20] = (width >> 16) & 255;
	buffer[21] = (width >> 24) & 255;

	buffer[22] = (height >> 0) & 255;
	buffer[23] = (height >> 8) & 255;
	buffer[24] = (height >> 16) & 255;
	buffer[25] = (height >> 24) & 255;
	buffer[26] = 1; // number of color planes
	buffer[28] = 24; // bpp

	buffer[34] = (memcount >> 0) & 255;
	buffer[35] = (memcount >> 8) & 255;
	buffer[36] = (memcount >> 16) & 255;
	buffer[37] = (memcount >> 24) & 255;
	buffer[38] = 0xC4; // horizontal dpi
	buffer[39] = 0x0E; // horizontal dpi
	buffer[42] = 0xC4; // vertical dpi
	buffer[43] = 0x0E; // vertical dpi
}

/*
==================
RB_TakeScreenshotBMP

GL2 variant. RB_ReadPixels here takes no lineAlign argument, so the BMP
4-byte scanline padding is materialised into a freshly allocated buffer
(rather than the in-place rearrange the renderervk/GL1 path uses).
==================
*/
static void RB_TakeScreenshotBMP( int x, int y, int width, int height, const char *fileName, int clipboardOnly )
{
	byte *allbuf;
	byte *src, *out;
	size_t offset = 0, memcount;
	const int header_size = 54; // bitmapfileheader(14) + bitmapinfoheader(40)
	int padlen;
	int scanlen, srcstride;
	int row, col;

	allbuf = RB_ReadPixels( x, y, width, height, &offset, &padlen );
	src = allbuf + offset;
	srcstride = width * 3 + padlen;

	// BMP scanlines are padded to a 4-byte boundary
	scanlen = PAD( width * 3, 4 );
	memcount = (size_t)scanlen * height;

	// gamma correction
	if ( glConfig.deviceSupportsGamma )
		R_GammaCorrect( src, srcstride * height );

	out = ri.Hunk_AllocateTempMemory( memcount + header_size );
	FillBMPHeader( out, width, height, memcount, header_size );

	// swap rgb to bgr, copy scanlines with BMP padding
	for ( row = 0; row < height; row++ ) {
		byte *srcrow = src + (size_t)row * srcstride;
		byte *dstrow = out + header_size + (size_t)row * scanlen;
		for ( col = 0; col < width; col++ ) {
			dstrow[0] = srcrow[2];
			dstrow[1] = srcrow[1];
			dstrow[2] = srcrow[0];
			srcrow += 3;
			dstrow += 3;
		}
	}

	if ( clipboardOnly ) {
		// copy starting from bitmapinfoheader
		ri.Sys_SetClipboardBitmap( out + 14, memcount + 40 );
	} else {
		ri.FS_WriteFile( fileName, out, memcount + header_size );
	}

	ri.Hunk_FreeTempMemory( out );
	ri.Hunk_FreeTempMemory( allbuf );
}

/*
==================
RB_TakeScreenshotPNG

PNG encoder via renderercommon/tr_image_png_write.c. Input follows the
RB_ReadPixels convention (bottom-up RGB); padding, if any, is compacted
in place before encoding.
==================
*/
static void RB_TakeScreenshotPNG( int x, int y, int width, int height, const char *fileName, int clipboardOnly )
{
	byte	*allbuf;
	byte	*buffer;
	byte	*pngBytes = NULL;
	int		 pngLen = 0;
	size_t	 offset = 0;
	int		 padlen;

	allbuf = RB_ReadPixels( x, y, width, height, &offset, &padlen );
	buffer = allbuf + offset;

	// compact away any line padding
	if ( padlen != 0 ) {
		byte *src = buffer;
		byte *dst = buffer;
		int  i;
		for ( i = 0; i < height; i++ ) {
			if ( src != dst ) memmove( dst, src, width * 3 );
			src += width * 3 + padlen;
			dst += width * 3;
		}
	}

	// gamma correction
	if ( glConfig.deviceSupportsGamma )
		R_GammaCorrect( buffer, (size_t)width * 3 * height );

	if ( !R_EncodePNG( buffer, width, height, &pngBytes, &pngLen ) ) {
		R_LOG( rch_cmd, SEV_WARN, "RB_TakeScreenshotPNG: encode failed\n" );
		ri.Hunk_FreeTempMemory( allbuf );
		return;
	}

	if ( clipboardOnly ) {
		ri.Sys_SetClipboardImagePNG( pngBytes, pngLen );
	} else {
		ri.FS_WriteFile( fileName, pngBytes, pngLen );
	}

	ri.Free( pngBytes );
	ri.Hunk_FreeTempMemory( allbuf );
}

/*
==================
RB_TakeScreenshotCmd
==================
*/
const void *RB_TakeScreenshotCmd( const void *data ) {
	const screenshotCommand_t	*cmd;

	cmd = (const screenshotCommand_t *)data;

	// finish any 2D drawing if needed
	if(tess.numIndexes)
		RB_EndSurface();

	if ( cmd->typeMask & SCREENSHOT_JPG )
		RB_TakeScreenshotJPEG( cmd->x, cmd->y, cmd->width, cmd->height, cmd->fileName );
	else if ( cmd->typeMask & SCREENSHOT_BMP )
		RB_TakeScreenshotBMP( cmd->x, cmd->y, cmd->width, cmd->height, cmd->fileName, cmd->typeMask & SCREENSHOT_BMP_CLIPBOARD );
	else if ( cmd->typeMask & SCREENSHOT_PNG )
		RB_TakeScreenshotPNG( cmd->x, cmd->y, cmd->width, cmd->height, cmd->fileName, cmd->typeMask & SCREENSHOT_PNG_CLIPBOARD );
	else
		RB_TakeScreenshot( cmd->x, cmd->y, cmd->width, cmd->height, cmd->fileName );

	if ( !cmd->silent )
		R_ScreenshotPrintSaved( cmd->fileName );

	return (const void *)(cmd + 1);
}

/*
==================
R_TakeScreenshot

Enqueue an RC_SCREENSHOT command. typeMask is a SCREENSHOT_* value.
==================
*/
static void R_TakeScreenshot( int x, int y, int width, int height, const char *name, int typeMask, qboolean silent ) {
	static char	fileName[MAX_OSPATH]; // bad things if two screenshots per frame?
	screenshotCommand_t	*cmd;

	cmd = R_GetCommandBuffer( sizeof( *cmd ) );
	if ( !cmd ) {
		return;
	}
	cmd->commandId = RC_SCREENSHOT;

	cmd->x = x;
	cmd->y = y;
	cmd->width = width;
	cmd->height = height;
	Q_strncpyz( fileName, name, sizeof(fileName) );
	cmd->fileName = fileName;
	cmd->typeMask = typeMask;
	cmd->silent = silent;
}

/*
====================
R_LevelShot

levelshots are specialized 128*128 thumbnails for
the menu system, sampled down from full screen distorted images

Non-static — called by the shared screenshot grammar (renderercommon/
tr_screenshot.c) via the R_LevelShot hook.
====================
*/
void R_LevelShot( void ) {
	char		checkname[MAX_OSPATH];
	byte		*buffer;
	byte		*source, *allsource;
	byte		*src, *dst;
	size_t			offset = 0;
	int			padlen;
	int			x, y;
	int			r, g, b;
	float		xScale, yScale;
	int			xx, yy;

	Com_sprintf(checkname, sizeof(checkname), "levelshots/%s.tga", tr.world->baseName);

	allsource = RB_ReadPixels(0, 0, glConfig.vidWidth, glConfig.vidHeight, &offset, &padlen);
	source = allsource + offset;

	buffer = ri.Hunk_AllocateTempMemory(128 * 128*3 + 18);
	memset (buffer, 0, 18);
	buffer[2] = 2;		// uncompressed type
	buffer[12] = 128;
	buffer[14] = 128;
	buffer[16] = 24;	// pixel size

	// resample from source
	xScale = glConfig.vidWidth / 512.0f;
	yScale = glConfig.vidHeight / 384.0f;
	for ( y = 0 ; y < 128 ; y++ ) {
		for ( x = 0 ; x < 128 ; x++ ) {
			r = g = b = 0;
			for ( yy = 0 ; yy < 3 ; yy++ ) {
				for ( xx = 0 ; xx < 4 ; xx++ ) {
					src = source + (3 * glConfig.vidWidth + padlen) * (int)((y*3 + yy) * yScale) +
						3 * (int) ((x*4 + xx) * xScale);
					r += src[0];
					g += src[1];
					b += src[2];
				}
			}
			dst = buffer + 18 + 3 * ( y * 128 + x );
			dst[0] = b / 12;
			dst[1] = g / 12;
			dst[2] = r / 12;
		}
	}

	// gamma correction
	if ( glConfig.deviceSupportsGamma ) {
		R_GammaCorrect( buffer + 18, 128 * 128 * 3 );
	}

	ri.FS_WriteFile( checkname, buffer, 128 * 128*3 + 18 );

	ri.Hunk_FreeTempMemory(buffer);
	ri.Hunk_FreeTempMemory(allsource);

	R_LOG( rch_cmd, SEV_INFO, "Wrote %s\n", checkname );
}

/*
==================
RB_ScheduleScreenshot

Screenshot grammar hook (see renderercommon/tr_screenshot.h). The shared
grammar resolves (typeMask, fileName, silent); GL2 schedules the capture
through its render-command queue (RC_SCREENSHOT). Always succeeds — GL2
has no minimized/FBO restriction here.
==================
*/
qboolean RB_ScheduleScreenshot( int typeMask, const char *fileName, qboolean silent ) {
	R_TakeScreenshot( 0, 0, glConfig.vidWidth, glConfig.vidHeight, fileName, typeMask, silent );
	return qtrue;
}

//============================================================================

/*
==================
R_ExportCubemaps
==================
*/
static void R_ExportCubemaps(void)
{
	exportCubemapsCommand_t	*cmd;

	cmd = R_GetCommandBuffer(sizeof(*cmd));
	if (!cmd) {
		return;
	}
	cmd->commandId = RC_EXPORT_CUBEMAPS;
}


/*
==================
R_ExportCubemaps_f
==================
*/
static void R_ExportCubemaps_f(void)
{
	R_ExportCubemaps();
}

//============================================================================

/*
==================
RB_TakeVideoFrameCmd
==================
*/
const void *RB_TakeVideoFrameCmd( const void *data )
{
	const videoFrameCommand_t	*cmd;
	byte				*cBuf;
	size_t				memcount, linelen;
	int				padwidth, avipadwidth, padlen, avipadlen;
	GLint packAlign;

	// finish any 2D drawing if needed
	if(tess.numIndexes)
		RB_EndSurface();

	cmd = (const videoFrameCommand_t *)data;

	qglGetIntegerv(GL_PACK_ALIGNMENT, &packAlign);

	linelen = cmd->width * 3;

	// Alignment stuff for glReadPixels
	padwidth = PAD(linelen, packAlign);
	padlen = padwidth - linelen;
	// AVI line padding
	avipadwidth = PAD(linelen, AVI_LINE_PADDING);
	avipadlen = avipadwidth - linelen;

	cBuf = PADP(cmd->captureBuffer, packAlign);

	qglReadPixels(0, 0, cmd->width, cmd->height, GL_RGB,
		GL_UNSIGNED_BYTE, cBuf);

	memcount = padwidth * cmd->height;

	// gamma correction
	if(glConfig.deviceSupportsGamma)
		R_GammaCorrect(cBuf, memcount);

	if(cmd->motionJpeg)
	{
		memcount = ri.CL_SaveJPGToBuffer(cmd->encodeBuffer, linelen * cmd->height,
			r_aviMotionJpegQuality->integer,
			cmd->width, cmd->height, cBuf, padlen);
		ri.CL_WriteAVIVideoFrame(cmd->encodeBuffer, memcount);
	}
	else
	{
		byte *lineend, *memend;
		byte *srcptr, *destptr;

		srcptr = cBuf;
		destptr = cmd->encodeBuffer;
		memend = srcptr + memcount;

		// swap R and B and remove line paddings
		while(srcptr < memend)
		{
			lineend = srcptr + linelen;
			while(srcptr < lineend)
			{
				*destptr++ = srcptr[2];
				*destptr++ = srcptr[1];
				*destptr++ = srcptr[0];
				srcptr += 3;
			}

			memset(destptr, '\0', avipadlen);
			destptr += avipadlen;

			srcptr += padlen;
		}

		ri.CL_WriteAVIVideoFrame(cmd->encodeBuffer, avipadwidth * cmd->height);
	}

	return (const void *)(cmd + 1);
}

//============================================================================

/*
** GL_SetDefaultState
*/
void GL_SetDefaultState( void )
{
	qglClearDepth( 1.0f );

	qglCullFace(GL_FRONT);

	GL_BindNullTextures();

	if (glRefConfig.framebufferObject)
		GL_BindNullFramebuffers();

	GL_TextureMode( r_textureMode->string );

	//qglShadeModel( GL_SMOOTH );
	qglDepthFunc( GL_LEQUAL );

	//
	// make sure our GL state vector is set correctly
	//
	glState.glStateBits = GLS_DEPTHTEST_DISABLE | GLS_DEPTHMASK_TRUE;
	glState.storedGlState = 0;
	glState.faceCulling = CT_TWO_SIDED;
	glState.faceCullFront = qtrue;

	GL_BindNullProgram();

	if (glRefConfig.vertexArrayObject)
		qglBindVertexArray(0);

	qglBindBuffer(GL_ARRAY_BUFFER, 0);
	qglBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	glState.currentVao = NULL;
	glState.vertexAttribsEnabled = 0;

	qglPolygonMode (GL_FRONT_AND_BACK, GL_FILL);
	qglDepthMask( GL_TRUE );
	qglDisable( GL_DEPTH_TEST );
	qglEnable( GL_SCISSOR_TEST );
	qglDisable( GL_CULL_FACE );
	qglDisable( GL_BLEND );

	if (glRefConfig.seamlessCubeMap)
		qglEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);

	// GL_POLYGON_OFFSET_FILL will be glEnable()d when this is used
	qglPolygonOffset( r_offsetFactor->value, r_offsetUnits->value );

	qglClearColor( 0.0f, 0.0f, 0.0f, 1.0f );	// FIXME: get color of sky
}

/*
================
R_PrintLongString

Workaround for ri.Printf's 1024 characters buffer limit.
================
*/
static void R_PrintLongString(const char *string) {
	char buffer[1024];
	const char *p;
	int size = strlen(string);

	p = string;
	while(size > 0)
	{
		Q_strncpyz(buffer, p, sizeof (buffer) );
		R_LOG( rch_init, SEV_DEBUG, "%s", buffer );
		p += 1023;
		size -= 1023;
	}
}

/*
================
GfxInfo_f
================
*/
static void GfxInfo_f( void )
{
	const char *enablestrings[] =
	{
		"disabled",
		"enabled"
	};
	const char *fsstrings[] =
	{
		"windowed",
		"fullscreen"
	};

	R_LOG( rch_init, SEV_INFO, "\n" );
	R_LOG( rch_init, SEV_INFO, "GL_VENDOR: %s\n", glConfig.vendor_string );
	R_LOG( rch_init, SEV_INFO, "GL_RENDERER: %s\n", glConfig.renderer_string );
	R_LOG( rch_init, SEV_INFO, "GL_VERSION: %s\n", glConfig.version_string );
	R_LOG( rch_init, SEV_INFO, "GL_EXTENSIONS: " );
	if ( qglGetStringi )
	{
		GLint numExtensions;

		qglGetIntegerv( GL_NUM_EXTENSIONS, &numExtensions );
		for ( int i = 0; i < numExtensions; i++ )
		{
			R_LOG( rch_init, SEV_INFO, "%s ", qglGetStringi( GL_EXTENSIONS, i ) );
		}
	}
	else
	{
		R_PrintLongString( glConfig.extensions_string );
	}
	R_LOG( rch_init, SEV_INFO, "\n" );
	R_LOG( rch_init, SEV_INFO, "GL_MAX_TEXTURE_SIZE: %d\n", glConfig.maxTextureSize );
	R_LOG( rch_init, SEV_INFO, "GL_MAX_TEXTURE_IMAGE_UNITS: %d\n", glConfig.numTextureUnits );
	R_LOG( rch_init, SEV_INFO, "\n" );
	R_LOG( rch_init, SEV_INFO, "PIXELFORMAT: color(%d-bits) Z(%d-bit) stencil(%d-bits)\n", glConfig.colorBits, glConfig.depthBits, glConfig.stencilBits );
	R_LOG( rch_init, SEV_INFO, "MODE: %d, %d x %d %s hz:", ri.Cvar_VariableIntegerValue( "r_mode" ), glConfig.vidWidth, glConfig.vidHeight, fsstrings[ glConfig.isFullscreen != 0 ] );
	if ( glConfig.displayFrequency )
	{
		R_LOG( rch_init, SEV_INFO, "%d\n", glConfig.displayFrequency );
	}
	else
	{
		R_LOG( rch_init, SEV_INFO, "N/A\n" );
	}
	if ( glConfig.deviceSupportsGamma )
	{
		R_LOG( rch_init, SEV_DEBUG, "GAMMA: hardware w/ %d overbright bits\n", tr.overbrightBits );
	}
	else
	{
		R_LOG( rch_init, SEV_DEBUG, "GAMMA: software w/ %d overbright bits\n", tr.overbrightBits );
	}

	R_LOG( rch_init, SEV_INFO, "texturemode: %s\n", r_textureMode->string );
	R_LOG( rch_init, SEV_INFO, "picmip: %d\n", r_picmip->integer );
	R_LOG( rch_init, SEV_INFO, "texture bits: %d\n", r_textureBits->integer );
	R_LOG( rch_init, SEV_INFO, "texenv add: %s\n", enablestrings[glConfig.textureEnvAddAvailable != 0] );
	R_LOG( rch_init, SEV_INFO, "compressed textures: %s\n", enablestrings[glConfig.textureCompression!=TC_NONE] );

	if ( r_finish->integer ) {
		R_LOG( rch_init, SEV_INFO, "Forcing glFinish\n" );
	}
}


/*
================
GfxMemInfo_f
================
*/
static void GfxMemInfo_f( void )
{
	switch (glRefConfig.memInfo)
	{
		case MI_NONE:
		{
			R_LOG( rch_init, SEV_INFO, "No extension found for GPU memory info.\n");
		}
		break;
		case MI_NVX:
		{
			int value;

			qglGetIntegerv(GL_GPU_MEMORY_INFO_DEDICATED_VIDMEM_NVX, &value);
			R_LOG( rch_init, SEV_INFO, "GPU_MEMORY_INFO_DEDICATED_VIDMEM_NVX: %ikb\n", value);

			qglGetIntegerv(GL_GPU_MEMORY_INFO_TOTAL_AVAILABLE_MEMORY_NVX, &value);
			R_LOG( rch_init, SEV_INFO, "GPU_MEMORY_INFO_TOTAL_AVAILABLE_MEMORY_NVX: %ikb\n", value);

			qglGetIntegerv(GL_GPU_MEMORY_INFO_CURRENT_AVAILABLE_VIDMEM_NVX, &value);
			R_LOG( rch_init, SEV_INFO, "GPU_MEMORY_INFO_CURRENT_AVAILABLE_VIDMEM_NVX: %ikb\n", value);

			qglGetIntegerv(GL_GPU_MEMORY_INFO_EVICTION_COUNT_NVX, &value);
			R_LOG( rch_init, SEV_INFO, "GPU_MEMORY_INFO_EVICTION_COUNT_NVX: %i\n", value);

			qglGetIntegerv(GL_GPU_MEMORY_INFO_EVICTED_MEMORY_NVX, &value);
			R_LOG( rch_init, SEV_INFO, "GPU_MEMORY_INFO_EVICTED_MEMORY_NVX: %ikb\n", value);
		}
		break;
		case MI_ATI:
		{
			// GL_ATI_meminfo
			int value[4];

			qglGetIntegerv(GL_VBO_FREE_MEMORY_ATI, &value[0]);
			R_LOG( rch_init, SEV_INFO, "VBO_FREE_MEMORY_ATI: %ikb total %ikb largest aux: %ikb total %ikb largest\n", value[0], value[1], value[2], value[3]);

			qglGetIntegerv(GL_TEXTURE_FREE_MEMORY_ATI, &value[0]);
			R_LOG( rch_init, SEV_INFO, "TEXTURE_FREE_MEMORY_ATI: %ikb total %ikb largest aux: %ikb total %ikb largest\n", value[0], value[1], value[2], value[3]);

			qglGetIntegerv(GL_RENDERBUFFER_FREE_MEMORY_ATI, &value[0]);
			R_LOG( rch_init, SEV_INFO, "RENDERBUFFER_FREE_MEMORY_ATI: %ikb total %ikb largest aux: %ikb total %ikb largest\n", value[0], value[1], value[2], value[3]);
		}
		break;
	}
}


/*
===============
R_Register
===============
*/
static void R_Register( void )
{
	//
	// latched and archived variables
	//
	r_allowExtensions = ri.Cvar_Get( "r_allowExtensions", "1", CVAR_ARCHIVE | CVAR_NODEFAULT | CVAR_LATCH | CVAR_CHEAT );
	ri.Cvar_SetDescription( r_allowExtensions, "Use all of the OpenGL extensions your card is capable of." );
	r_ext_compressed_textures = ri.Cvar_Get( "r_ext_compressed_textures", "0", CVAR_ARCHIVE | CVAR_NODEFAULT | CVAR_LATCH | CVAR_CHEAT );
	ri.Cvar_SetDescription( r_ext_compressed_textures, "Enables texture compression." );
	r_ext_multitexture = ri.Cvar_Get( "r_ext_multitexture", "1", CVAR_ARCHIVE | CVAR_NODEFAULT | CVAR_LATCH | CVAR_CHEAT );
	ri.Cvar_SetDescription( r_ext_multitexture, "Enables hardware multi-texturing (0: off, 1: on)." );
	r_ext_compiled_vertex_array = ri.Cvar_Get( "r_ext_compiled_vertex_array", "1", CVAR_ARCHIVE | CVAR_NODEFAULT | CVAR_LATCH | CVAR_CHEAT );
	ri.Cvar_SetDescription( r_ext_compiled_vertex_array, "Enables hardware-compiled vertex array rendering method." );
	r_ext_texture_env_add = ri.Cvar_Get( "r_ext_texture_env_add", "1", CVAR_ARCHIVE | CVAR_NODEFAULT | CVAR_LATCH | CVAR_CHEAT );
	ri.Cvar_SetDescription( r_ext_texture_env_add, "Enables additive blending in multitexturing. Requires \\r_ext_multitexture 1." );

	r_ext_framebuffer_object = ri.Cvar_Get( "r_ext_framebuffer_object", "1", CVAR_ARCHIVE | CVAR_LATCH);
	r_ext_texture_float = ri.Cvar_Get( "r_ext_texture_float", "1", CVAR_ARCHIVE | CVAR_LATCH);
	r_ext_framebuffer_multisample = ri.Cvar_Get( "r_ext_framebuffer_multisample", "0", CVAR_ARCHIVE | CVAR_LATCH);
	ri.Cvar_SetDescription( r_ext_framebuffer_multisample, "For anti-aliasing geometry edges (FBO)." );
	r_arb_seamless_cube_map = ri.Cvar_Get( "r_arb_seamless_cube_map", "0", CVAR_ARCHIVE | CVAR_LATCH);
	r_arb_vertex_array_object = ri.Cvar_Get( "r_arb_vertex_array_object", "1", CVAR_ARCHIVE | CVAR_LATCH);
	r_ext_direct_state_access = ri.Cvar_Get("r_ext_direct_state_access", "1", CVAR_ARCHIVE | CVAR_LATCH);

	r_ext_texture_filter_anisotropic = ri.Cvar_Get( "r_ext_texture_filter_anisotropic", "1", CVAR_ARCHIVE | CVAR_LATCH );
	ri.Cvar_SetDescription( r_ext_texture_filter_anisotropic, "Allow anisotropic filtering." );
	r_ext_max_anisotropy = ri.Cvar_Get( "r_ext_max_anisotropy", "8", CVAR_ARCHIVE | CVAR_LATCH );
	ri.Cvar_SetDescription( r_ext_max_anisotropy, "Sets maximum anisotropic level for your graphics driver. Requires \\r_ext_texture_filter_anisotropic." );

	r_picmip = ri.Cvar_Get ("r_picmip", "0", CVAR_ARCHIVE | CVAR_LATCH );
	ri.Cvar_CheckRange( r_picmip, "0", "16", CV_INTEGER );
	ri.Cvar_SetDescription( r_picmip, "Set texture quality, lower is better." );
	r_roundImagesDown = ri.Cvar_Get ("r_roundImagesDown", "1", CVAR_ARCHIVE | CVAR_LATCH );
	ri.Cvar_SetDescription( r_roundImagesDown, "When images are scaled, round images down instead of up." );
	r_colorMipLevels = ri.Cvar_Get ("r_colorMipLevels", "0", CVAR_LATCH );
	ri.Cvar_SetDescription( r_colorMipLevels, "Debugging tool to artificially color different mipmap levels so that they are more apparent." );
	r_detailTextures = ri.Cvar_Get( "r_detailtextures", "1", CVAR_ARCHIVE | CVAR_LATCH );
	ri.Cvar_SetDescription( r_detailTextures, "Enables usage of shader stages flagged as detail." );
	r_textureBits = ri.Cvar_Get( "r_textureBits", "0", CVAR_ARCHIVE | CVAR_LATCH );
	ri.Cvar_SetDescription( r_textureBits, "Number of texture bits per texture." );
//	r_stencilBits = ri.Cvar_Get( "r_stencilBits", "8", CVAR_ARCHIVE | CVAR_LATCH );
	r_ext_multisample = ri.Cvar_Get( "r_ext_multisample", "0", CVAR_ARCHIVE | CVAR_LATCH );
	ri.Cvar_CheckRange( r_ext_multisample, "0", "8", CV_INTEGER );
	ri.Cvar_SetDescription( r_ext_multisample, "For anti-aliasing geometry edges." );
	r_brightness = ri.Cvar_Get( "r_brightness", "2", CVAR_ARCHIVE | CVAR_NODEFAULT | CVAR_LATCH );
	ri.Cvar_CheckRange( r_brightness, "0.25", "32", CV_FLOAT );
	ri.Cvar_SetDescription( r_brightness,
		"Float-based overall brightness multiplier.\n"
		"Range 0.25-32, default 2.0." );
	r_ignorehwgamma = ri.Cvar_Get( "r_ignorehwgamma", "0", CVAR_ARCHIVE | CVAR_LATCH);
	ri.Cvar_SetDescription( r_ignorehwgamma, "Overrides hardware gamma capabilities." );
	r_simpleMipMaps = ri.Cvar_Get( "r_simpleMipMaps", "1", CVAR_ARCHIVE | CVAR_LATCH );
	ri.Cvar_SetDescription( r_simpleMipMaps, "Whether or not to use a simple mipmapping algorithm or a more correct one:\n 0: off (proper linear filter)\n 1: on (for slower machines)" );
	r_vertexLight = ri.Cvar_Get( "r_vertexLight", "0", CVAR_ARCHIVE | CVAR_LATCH );
	ri.Cvar_SetDescription( r_vertexLight, "Set to 1 to use vertex light instead of lightmaps, collapse all multi-stage shaders into single-stage ones, might cause rendering artifacts." );
	r_subdivisions = ri.Cvar_Get ("r_subdivisions", "1", CVAR_ARCHIVE | CVAR_LATCH);
	ri.Cvar_SetDescription(r_subdivisions, "Distance to subdivide bezier curved surfaces. Higher values mean less subdivision and less geometric complexity.");
	r_greyscale = ri.Cvar_Get("r_greyscale", "0", CVAR_ARCHIVE | CVAR_LATCH);
	ri.Cvar_CheckRange( r_greyscale, "0", "1", CV_FLOAT );
	ri.Cvar_SetDescription( r_greyscale, "Desaturates rendered frame." );

	r_externalGLSL = ri.Cvar_Get( "r_externalGLSL", "0", CVAR_LATCH );

	r_hdr = ri.Cvar_Get( "r_hdr", "1", CVAR_ARCHIVE | CVAR_LATCH );
	ri.Cvar_SetDescription( r_hdr, "Do scene rendering in a framebuffer with high dynamic range." );
	r_floatLightmap = ri.Cvar_Get( "r_floatLightmap", "0", CVAR_ARCHIVE | CVAR_LATCH );
	r_postProcess = ri.Cvar_Get( "r_postProcess", "1", CVAR_ARCHIVE );
	ri.Cvar_SetDescription( r_postProcess, "Enable post-processing." );

	r_toneMap = ri.Cvar_Get( "r_toneMap", "3", CVAR_ARCHIVE );
	ri.Cvar_SetDescription( r_toneMap, "Enable tone mapping. Requires r_hdr and r_postProcess." );
	r_forceToneMap = ri.Cvar_Get( "r_forceToneMap", "0", CVAR_CHEAT );
	r_forceToneMapMin = ri.Cvar_Get( "r_forceToneMapMin", "-8.0", CVAR_CHEAT );
	r_forceToneMapAvg = ri.Cvar_Get( "r_forceToneMapAvg", "-2.0", CVAR_CHEAT );
	r_forceToneMapMax = ri.Cvar_Get( "r_forceToneMapMax", "0.0", CVAR_CHEAT );

	r_autoExposure = ri.Cvar_Get( "r_autoExposure", "1", CVAR_ARCHIVE );
	ri.Cvar_SetDescription( r_autoExposure, "Do automatic exposure based on scene brightness. Hardcoded to -2 to 2 on maps that don't specify otherwise. Requires r_hdr, r_postProcess, and r_toneMap." );
	r_forceAutoExposure = ri.Cvar_Get( "r_forceAutoExposure", "0", CVAR_CHEAT );
	r_forceAutoExposureMin = ri.Cvar_Get( "r_forceAutoExposureMin", "-2.0", CVAR_CHEAT );
	r_forceAutoExposureMax = ri.Cvar_Get( "r_forceAutoExposureMax", "2.0", CVAR_CHEAT );

	r_cameraExposure = ri.Cvar_Get( "r_cameraExposure", "1", CVAR_CHEAT );

	r_depthPrepass = ri.Cvar_Get( "r_depthPrepass", "1", CVAR_ARCHIVE );
	ri.Cvar_SetDescription( r_depthPrepass, "Do a depth-only pass before rendering. Speeds up rendering in cases where advanced features are used. Required for r_sunShadows." );
	r_ssao = ri.Cvar_Get( "r_ssao", "0", CVAR_LATCH | CVAR_ARCHIVE );
	ri.Cvar_SetDescription( r_ssao, "Enable screen-space ambient occlusion." );

	r_normalMapping = ri.Cvar_Get( "r_normalMapping", "1", CVAR_ARCHIVE | CVAR_LATCH );
	ri.Cvar_SetDescription( r_normalMapping, "Enable normal maps for materials that support it." );
	r_specularMapping = ri.Cvar_Get( "r_specularMapping", "1", CVAR_ARCHIVE | CVAR_LATCH );
	ri.Cvar_SetDescription( r_specularMapping, "Enable specular maps for materials that support it." );
	r_deluxeMapping = ri.Cvar_Get( "r_deluxeMapping", "1", CVAR_ARCHIVE | CVAR_LATCH );
	ri.Cvar_SetDescription( r_deluxeMapping, "Enable deluxe mapping (map is compiled with light directions). Even if the map doesn't have deluxe mapping compiled in, an approximation based on the lightgrid will be used." );
	r_parallaxMapping = ri.Cvar_Get( "r_parallaxMapping", "0", CVAR_ARCHIVE | CVAR_LATCH );
	ri.Cvar_SetDescription( r_parallaxMapping, "Enable parallax mapping for materials that support it.\n 0: No\n 1: Use parallax occlusion mapping\n 2: Use relief mapping" );
	r_parallaxMapOffset = ri.Cvar_Get( "r_parallaxMapOffset", "0", CVAR_ARCHIVE | CVAR_LATCH );
	ri.Cvar_SetDescription( r_parallaxMapOffset, "Set the parallax height offset." );
	r_parallaxMapShadows = ri.Cvar_Get( "r_parallaxMapShadows", "0", CVAR_ARCHIVE | CVAR_LATCH );
	ri.Cvar_SetDescription( r_parallaxMapShadows, "Enable self-shadowing on parallax map supported materials." );
	r_cubeMapping = ri.Cvar_Get( "r_cubeMapping", "0", CVAR_ARCHIVE | CVAR_LATCH );
	r_cubemapSize = ri.Cvar_Get( "r_cubemapSize", "128", CVAR_ARCHIVE | CVAR_LATCH );
	r_deluxeSpecular = ri.Cvar_Get("r_deluxeSpecular", "0.3", CVAR_ARCHIVE | CVAR_LATCH);
	r_pbr = ri.Cvar_Get("r_pbr", "0", CVAR_ARCHIVE | CVAR_LATCH);
	ri.Cvar_SetDescription( r_pbr, "Enable physically based rendering." );
	r_baseNormalX = ri.Cvar_Get( "r_baseNormalX", "1.0", CVAR_ARCHIVE | CVAR_LATCH );
	r_baseNormalY = ri.Cvar_Get( "r_baseNormalY", "1.0", CVAR_ARCHIVE | CVAR_LATCH );
	r_baseParallax = ri.Cvar_Get( "r_baseParallax", "0.05", CVAR_ARCHIVE | CVAR_LATCH );
	r_baseSpecular = ri.Cvar_Get( "r_baseSpecular", "0.04", CVAR_ARCHIVE | CVAR_LATCH );
	r_baseGloss = ri.Cvar_Get( "r_baseGloss", "0.3", CVAR_ARCHIVE | CVAR_LATCH );
	r_glossType = ri.Cvar_Get("r_glossType", "1", CVAR_ARCHIVE | CVAR_LATCH);
	r_dlightMode = ri.Cvar_Get( "r_dlightMode", "0", CVAR_ARCHIVE | CVAR_LATCH );
	ri.Cvar_SetDescription( r_dlightMode, "Dynamic light mode:\n 0: VQ3 'fake' dynamic lights\n 1: High-quality per-pixel dynamic lights, slightly faster than VQ3's on modern hardware\n 2: Same as 1 but applies to all MD3 models too" );
	r_pshadowDist = ri.Cvar_Get( "r_pshadowDist", "128", CVAR_ARCHIVE );
	r_mergeLightmaps = ri.Cvar_Get( "r_mergeLightmaps", "1", CVAR_ARCHIVE | CVAR_LATCH );
	ri.Cvar_SetDescription( r_mergeLightmaps, "Merge small lightmaps into 2 or fewer giant lightmaps." );
	// lightmap atlas optimization (alias). Both cvars must be non-zero
	// for the atlas pack to be built inside R_LoadLightmaps().
	r_lightmapAtlas = ri.Cvar_Get( "r_lightmapAtlas", "1", CVAR_ARCHIVE | CVAR_LATCH );
	ri.Cvar_SetDescription( r_lightmapAtlas,
		"Pack individual BSP lightmaps into larger atlas textures.\n"
		" 1: pack into atlases (default, fewer texture binds, faster loads)\n"
		" 0: one texture per lightmap (legacy behaviour)\n"
		" Works together with r_mergeLightmaps." );

	// swap throttle during map loads.
	r_loadingFpsCap = ri.Cvar_Get( "r_loadingFpsCap", "10", CVAR_ARCHIVE | CVAR_NODEFAULT );
	ri.Cvar_CheckRange( r_loadingFpsCap, "0", "60", CV_INTEGER );
	ri.Cvar_SetDescription( r_loadingFpsCap,
		"Maximum render backend frames per second while loading a map.\n"
		" 0: no cap (legacy behaviour)\n"
		" 1-60: cap presentation to this rate. Default 10." );
	r_imageUpsample = ri.Cvar_Get( "r_imageUpsample", "0", CVAR_ARCHIVE | CVAR_LATCH );
	r_imageUpsampleMaxSize = ri.Cvar_Get( "r_imageUpsampleMaxSize", "1024", CVAR_ARCHIVE | CVAR_LATCH );
	r_imageUpsampleType = ri.Cvar_Get( "r_imageUpsampleType", "1", CVAR_ARCHIVE | CVAR_LATCH );
	r_genNormalMaps = ri.Cvar_Get( "r_genNormalMaps", "0", CVAR_ARCHIVE | CVAR_LATCH );

	r_forceSun = ri.Cvar_Get( "r_forceSun", "0", CVAR_CHEAT );
	r_forceSunLightScale = ri.Cvar_Get( "r_forceSunLightScale", "1.0", CVAR_CHEAT );
	r_forceSunAmbientScale = ri.Cvar_Get( "r_forceSunAmbientScale", "0.5", CVAR_CHEAT );
	r_drawSunRays = ri.Cvar_Get( "r_drawSunRays", "0", CVAR_ARCHIVE | CVAR_LATCH );
	r_sunlightMode = ri.Cvar_Get( "r_sunlightMode", "1", CVAR_ARCHIVE | CVAR_LATCH );

	r_sunShadows = ri.Cvar_Get( "r_sunShadows", "1", CVAR_ARCHIVE | CVAR_LATCH );
	r_shadowFilter = ri.Cvar_Get( "r_shadowFilter", "1", CVAR_ARCHIVE | CVAR_LATCH );
	ri.Cvar_SetDescription( r_shadowFilter, "Enable filtering shadows for a smoother look (0 - No. 1 - Some. 2 - Much)." );
	r_shadowBlur = ri.Cvar_Get("r_shadowBlur", "0", CVAR_ARCHIVE | CVAR_LATCH);
	r_shadowMapSize = ri.Cvar_Get("r_shadowMapSize", "1024", CVAR_ARCHIVE | CVAR_LATCH);
	ri.Cvar_SetDescription( r_shadowMapSize, "Size of each cascaded shadow map." );
	r_shadowCascadeZNear = ri.Cvar_Get( "r_shadowCascadeZNear", "8", CVAR_ARCHIVE | CVAR_LATCH );
	ri.Cvar_SetDescription( r_shadowCascadeZNear, "Near plane for shadow cascade frustums." );
	r_shadowCascadeZFar = ri.Cvar_Get( "r_shadowCascadeZFar", "1024", CVAR_ARCHIVE | CVAR_LATCH );
	ri.Cvar_SetDescription( r_shadowCascadeZFar, "Far plane for shadow cascade frustums." );
	r_shadowCascadeZBias = ri.Cvar_Get( "r_shadowCascadeZBias", "0", CVAR_ARCHIVE | CVAR_LATCH );
	ri.Cvar_SetDescription( r_shadowCascadeZBias, "Z-bias for shadow cascade frustums." );
	r_ignoreDstAlpha = ri.Cvar_Get( "r_ignoreDstAlpha", "1", CVAR_ARCHIVE | CVAR_LATCH );

	//
	// temporary latched variables that can only change over a restart
	//
	r_fullbright = ri.Cvar_Get ("r_fullbright", "0", CVAR_LATCH|CVAR_CHEAT );
	ri.Cvar_SetDescription( r_fullbright, "Debugging tool to render the entire level without lighting." );
	r_mapBrightness = ri.Cvar_Get( "r_mapBrightness", "2", CVAR_ARCHIVE | CVAR_NODEFAULT | CVAR_LATCH );
	ri.Cvar_CheckRange( r_mapBrightness, "0.25", "32", CV_FLOAT );
	ri.Cvar_SetDescription( r_mapBrightness,
		"Float-based map (lightmap) brightness multiplier.\n"
		"Range 0.25-32, default 2.0." );
	r_intensity = ri.Cvar_Get ("r_intensity", "1", CVAR_LATCH );
	ri.Cvar_SetDescription( r_intensity, "Global texture lighting scale." );
	r_singleShader = ri.Cvar_Get ("r_singleShader", "0", CVAR_CHEAT | CVAR_LATCH );
	ri.Cvar_SetDescription( r_singleShader, "Debugging tool that only uses the default shader for all rendering." );

	//
	// archived variables that can change at any time
	//
	r_lodCurveError = ri.Cvar_Get( "r_lodCurveError", "250", CVAR_ARCHIVE );
	ri.Cvar_CheckRange( r_lodCurveError, "-1", "8192", CV_FLOAT );
	ri.Cvar_SetDescription( r_lodCurveError, "Level of detail error on curved surface grids. Higher values result in better quality at a distance." );
	r_lodbias = ri.Cvar_Get( "r_lodbias", "-2", CVAR_ARCHIVE );
	ri.Cvar_SetDescription( r_lodbias, "Sets the level of detail of in-game models:\n -2: Ultra (further delays LOD transition in the distance)\n -1: Very High (delays LOD transition in the distance)\n 0: High\n 1: Medium\n 2: Low" );
	r_flares = ri.Cvar_Get ("r_flares", "0", CVAR_ARCHIVE );
	ri.Cvar_SetDescription( r_flares, "Enables halo effects on light sources." );
	r_znear = ri.Cvar_Get( "r_znear", "4", CVAR_CHEAT );
	ri.Cvar_CheckRange( r_znear, "0.001", "200", CV_FLOAT );
	ri.Cvar_SetDescription( r_znear, "Viewport distance from view origin (how close objects can be to the player before they're clipped out of the scene)." );
	r_zproj = ri.Cvar_Get( "r_zproj", "64", CVAR_ARCHIVE );
	ri.Cvar_SetDescription( r_zproj, "Projected viewport frustum." );
	r_stereoSeparation = ri.Cvar_Get( "r_stereoSeparation", "64", CVAR_ARCHIVE );
	ri.Cvar_SetDescription( r_stereoSeparation, "Control eye separation. Resulting separation is \\r_zproj divided by this value in standard units." );
	r_ignoreGLErrors = ri.Cvar_Get( "r_ignoreGLErrors", "1", CVAR_ARCHIVE );
	ri.Cvar_SetDescription( r_ignoreGLErrors, "Ignore OpenGL errors." );
	r_teleporterFlash = ri.Cvar_Get( "r_teleporterFlash", "1", CVAR_ARCHIVE );
	ri.Cvar_SetDescription( r_teleporterFlash, "Show a white screen instead of a black screen when being teleported in hyperspace." );
	r_fastsky = ri.Cvar_Get( "r_fastsky", "0", CVAR_ARCHIVE );
	ri.Cvar_SetDescription( r_fastsky, "Draw flat colored skies." );
	r_drawSun = ri.Cvar_Get( "r_drawSun", "0", CVAR_ARCHIVE );
	ri.Cvar_SetDescription( r_drawSun, "Draw sun shader in skies." );
	r_dynamiclight = ri.Cvar_Get( "r_dynamiclight", "1", CVAR_ARCHIVE );
	ri.Cvar_SetDescription( r_dynamiclight, "Enables dynamic lighting." );
	r_dlightBacks = ri.Cvar_Get( "r_dlightBacks", "1", CVAR_ARCHIVE );
	ri.Cvar_SetDescription( r_dlightBacks, "Whether or not dynamic lights should light up back-face culled geometry." );
	r_finish = ri.Cvar_Get ("r_finish", "0", CVAR_ARCHIVE);
	ri.Cvar_SetDescription( r_finish, "Force a glFinish call after rendering a scene." );
	r_textureMode = ri.Cvar_Get( "r_textureMode", "GL_LINEAR_MIPMAP_LINEAR", CVAR_ARCHIVE );
	ri.Cvar_SetDescription( r_textureMode, "Texture interpolation mode:\n GL_NEAREST: Nearest neighbor interpolation and will therefore appear similar to Quake II except with the added colored lighting\n GL_LINEAR: Linear interpolation and will appear to blend in objects that are closer than the resolution that the textures are set as\n GL_NEAREST_MIPMAP_NEAREST: Nearest neighbor interpolation with mipmapping for bilinear hardware, mipmapping will blend objects that are farther away than the resolution that they are set as\n GL_LINEAR_MIPMAP_NEAREST: Linear interpolation with mipmapping for bilinear hardware\n GL_NEAREST_MIPMAP_LINEAR: Nearest neighbor interpolation with mipmapping for trilinear hardware\n GL_LINEAR_MIPMAP_LINEAR: Linear interpolation with mipmapping for trilinear hardware" );
	r_gamma = ri.Cvar_Get( "r_gamma", "1", CVAR_ARCHIVE );
	ri.Cvar_CheckRange( r_gamma, "0.5", "3", CV_FLOAT );
	ri.Cvar_SetDescription( r_gamma, "Gamma correction factor." );
	r_facePlaneCull = ri.Cvar_Get ("r_facePlaneCull", "1", CVAR_ARCHIVE );
	ri.Cvar_SetDescription( r_facePlaneCull, "Enables culling of planar surfaces with back side test." );

	r_ambientScale = ri.Cvar_Get( "r_ambientScale", "0.6", CVAR_CHEAT );
	ri.Cvar_SetDescription( r_ambientScale, "Light grid ambient light scaling on entity models." );
	r_directedScale = ri.Cvar_Get( "r_directedScale", "1", CVAR_CHEAT );
	ri.Cvar_SetDescription( r_directedScale, "Light grid direct light scaling on entity models." );

	r_anaglyphMode = ri.Cvar_Get("r_anaglyphMode", "0", CVAR_ARCHIVE);
	ri.Cvar_SetDescription( r_anaglyphMode, "Enable rendering of anaglyph images. Valid options for 3D glasses types:\n 0: Disabled\n 1: Red-cyan\n 2: Red-blue\n 3: Red-green\n 4: Green-magenta" );

	//
	// temporary variables that can change at any time
	//
	r_showImages = ri.Cvar_Get( "r_showImages", "0", CVAR_TEMP );
	ri.Cvar_SetDescription( r_showImages, "Draw all images currently loaded into memory:\n 0: Disabled\n 1: Show images set to uniform size\n 2: Show images with scaled relative to largest image" );

	r_debugLight = ri.Cvar_Get( "r_debugLight", "0", CVAR_TEMP );
	ri.Cvar_SetDescription( r_debugLight, "Debugging tool to print ambient and directed lighting information." );
	r_debugSort = ri.Cvar_Get( "r_debugSort", "0", CVAR_CHEAT );
	ri.Cvar_SetDescription( r_debugSort, "Debugging tool to filter out shaders with depth sorting order values higher than the set value." );
	r_printShaders = ri.Cvar_Get( "r_printShaders", "0", 0 );
	ri.Cvar_SetDescription( r_printShaders, "Debugging tool to print on console of the number of shaders used." );
	r_saveFontData = ri.Cvar_Get( "r_saveFontData", "0", 0 );

	r_nocurves = ri.Cvar_Get ("r_nocurves", "0", CVAR_CHEAT );
	ri.Cvar_SetDescription( r_nocurves, "Set to 1 to disable drawing world bezier curves. Set to 0 to enable." );
	r_drawWorld = ri.Cvar_Get ("r_drawWorld", "1", CVAR_CHEAT );
	ri.Cvar_SetDescription( r_drawWorld, "Set to 0 to disable drawing the world. Set to 1 to enable." );
	r_lightmap = ri.Cvar_Get ("r_lightmap", "0", 0 );
	ri.Cvar_SetDescription( r_lightmap, "Show only lightmaps on all world surfaces." );
	r_portalOnly = ri.Cvar_Get ("r_portalOnly", "0", CVAR_CHEAT );
	ri.Cvar_SetDescription( r_portalOnly, "Set to 1 to render only first mirror/portal view if it is present on the scene." );

	r_flareSize = ri.Cvar_Get ("r_flareSize", "40", CVAR_CHEAT);
	ri.Cvar_SetDescription( r_flareSize, "Radius of light flares. Requires \\r_flares 1." );
	r_flareFade = ri.Cvar_Get ("r_flareFade", "7", CVAR_CHEAT);
	ri.Cvar_SetDescription( r_flareFade, "Distance to fade out light flares. Requires \\r_flares 1." );
	r_flareCoeff = ri.Cvar_Get ("r_flareCoeff", FLARE_STDCOEFF, CVAR_CHEAT);
	ri.Cvar_SetDescription( r_flareCoeff, "Coefficient for the light flare intensity falloff function. Requires \\r_flares 1." );

#if FEAT_FOG_SYSTEM
	r_useGlFog = ri.Cvar_Get( "r_useGlFog", "1", CVAR_ARCHIVE | CVAR_NODEFAULT );
	ri.Cvar_SetDescription( r_useGlFog, "Use GL_FOG fixed-function fog pipeline in addition to per-vertex volume fog." );
	r_defaultFogParmsType = ri.Cvar_Get( "r_defaultFogParmsType", "0", CVAR_ARCHIVE | CVAR_NODEFAULT );
	ri.Cvar_SetDescription( r_defaultFogParmsType, "Default fogType_t for maps without explicit fog: 0=linear, 1=exp, 2=exp2." );
	ri.Cvar_CheckRange( r_defaultFogParmsType, "0", "2", CV_INTEGER );
	r_globalLinearFogDrawSky = ri.Cvar_Get( "r_globalLinearFogDrawSky", "0", CVAR_ARCHIVE | CVAR_NODEFAULT );
	ri.Cvar_SetDescription( r_globalLinearFogDrawSky, "Draw sky surfaces through linear global fog (Spearmint compat)." );
#endif

	r_skipBackEnd = ri.Cvar_Get ("r_skipBackEnd", "0", CVAR_CHEAT);
	ri.Cvar_SetDescription( r_skipBackEnd, "Skips loading rendering backend." );

	r_measureOverdraw = ri.Cvar_Get( "r_measureOverdraw", "0", CVAR_CHEAT );
	r_lodscale = ri.Cvar_Get( "r_lodscale", "5", CVAR_CHEAT );
	ri.Cvar_SetDescription( r_lodscale, "Set scale for level of detail adjustment." );
	r_norefresh = ri.Cvar_Get ("r_norefresh", "0", CVAR_CHEAT);
	ri.Cvar_SetDescription( r_norefresh, "Bypasses refreshing of the rendered scene." );
	r_drawEntities = ri.Cvar_Get ("r_drawEntities", "1", CVAR_CHEAT );
	ri.Cvar_SetDescription( r_drawEntities, "Draw all world entities." );
	r_ignore = ri.Cvar_Get( "r_ignore", "1", CVAR_CHEAT );
	r_nocull = ri.Cvar_Get ("r_nocull", "0", CVAR_CHEAT);
	ri.Cvar_SetDescription( r_nocull, "Draw all culled objects." );
	r_novis = ri.Cvar_Get ("r_novis", "0", CVAR_CHEAT);
	ri.Cvar_SetDescription( r_novis, "Disables usage of PVS." );
	r_showCluster = ri.Cvar_Get ("r_showCluster", "0", CVAR_CHEAT);
	ri.Cvar_SetDescription( r_showCluster, "Shows current cluster index." );
	r_speeds = ri.Cvar_Get ("r_speeds", "0", CVAR_CHEAT);
	ri.Cvar_SetDescription( r_speeds, "Prints out various debugging stats from PVS:\n 0: Disabled\n 1: Backend BSP\n 2: Frontend grid culling\n 3: Current view cluster index\n 4: Dynamic lighting\n 5: zFar clipping\n 6: Flares" );
	r_debugSurface = ri.Cvar_Get ("r_debugSurface", "0", CVAR_CHEAT);
	ri.Cvar_SetDescription( r_debugSurface, "Backend visual debugging tool for bezier mesh surfaces." );
	r_nobind = ri.Cvar_Get ("r_nobind", "0", CVAR_CHEAT);
	ri.Cvar_SetDescription( r_nobind, "Backend debugging tool: Disables texture binding." );
	r_showTris = ri.Cvar_Get ("r_showTris", "0", CVAR_CHEAT);
	ri.Cvar_SetDescription( r_showTris, "Debugging tool: Wireframe rendering of polygon triangles in the world." );
	r_showSky = ri.Cvar_Get ("r_showSky", "0", CVAR_CHEAT);
	ri.Cvar_SetDescription( r_showSky, "Forces sky in front of all surfaces." );
	r_showNormals = ri.Cvar_Get ("r_showNormals", "0", CVAR_CHEAT);
	ri.Cvar_SetDescription( r_showNormals, "Debugging tool: Show wireframe surface normals." );
	r_clear = ri.Cvar_Get ("r_clear", "0", CVAR_CHEAT);
	ri.Cvar_SetDescription( r_clear, "Forces screen buffer clearing every frame, removing any hall of mirrors effect in void.\n Use \\r_clearColor to set color." );
	r_offsetFactor = ri.Cvar_Get( "r_offsetFactor", "-1", CVAR_CHEAT );
	ri.Cvar_SetDescription( r_offsetFactor, "Offset factor for shaders with polygonOffset stages." );
	r_offsetUnits = ri.Cvar_Get( "r_offsetunits", "-2", CVAR_CHEAT );
	ri.Cvar_SetDescription( r_offsetUnits, "Offset units for shaders with polygonOffset stages." );
	r_drawBuffer = ri.Cvar_Get( "r_drawBuffer", "GL_BACK", CVAR_CHEAT );
	ri.Cvar_SetDescription( r_drawBuffer, "Sets which frame buffer to draw into." );
	r_lockpvs = ri.Cvar_Get ("r_lockpvs", "0", CVAR_CHEAT);
	ri.Cvar_SetDescription( r_lockpvs, "Debugging tool: Locks to current potentially visible set. Useful for testing vis-culling in maps." );
	r_noportals = ri.Cvar_Get ("r_noportals", "0", CVAR_CHEAT);
	ri.Cvar_SetDescription(r_noportals, "Disables in-game portals, valid values: 0: Portals enabled\n 1: Portals disabled\n 2: Portals and mirrors disabled" );
	// cg_shadows is a single archived cvar shared with the Vulkan renderer, where it is
	// the unified shadow level (0=off, 1=contact/blob, 2=cast, 3=all) and defaults to 2.
	// This legacy GL2 path keeps its own 0=none/1=blur/2=stencil/3=projection (and 4=pshadow)
	// semantics, so the default is aligned to 2 to match the shared value a Vulkan session
	// archives — a 2 here selects this renderer's stencil shadows (legacy, no crash) rather
	// than a surprise off/blur. GL2 shadow semantics are otherwise unchanged; Vulkan is the
	// shipped path.
	r_shadows = ri.Cvar_Get( "cg_shadows", "2", 0 );

	r_marksOnTriangleMeshes = ri.Cvar_Get("r_marksOnTriangleMeshes", "0", CVAR_ARCHIVE);
	ri.Cvar_SetDescription( r_marksOnTriangleMeshes, "Enables impact marks on triangle mesh surfaces (ie: MD3 models.) Requires impact marks to be enabled in the game code." );

	r_aviMotionJpegQuality = ri.Cvar_Get("r_aviMotionJpegQuality", "100", CVAR_ARCHIVE);
	ri.Cvar_SetDescription( r_aviMotionJpegQuality, "Controls quality of Jpeg video capture when \\cl_aviMotionJpeg 1." );
	r_screenshotJpegQuality = ri.Cvar_Get("r_screenshotJpegQuality", "100", CVAR_ARCHIVE);
	ri.Cvar_SetDescription( r_screenshotJpegQuality, "Controls quality of Jpeg screenshots when using screenshotJpeg." );

	r_maxpolys = ri.Cvar_Get( "r_maxpolys", va("%d", MAX_POLYS), 0);
	ri.Cvar_SetDescription( r_maxpolys, "Maximum number of polygons to draw in a scene." );
	r_maxpolyverts = ri.Cvar_Get( "r_maxpolyverts", va("%d", MAX_POLYVERTS), 0);
	ri.Cvar_SetDescription( r_maxpolyverts, "Maximum number of polygon vertices to draw in a scene." );

	// make sure all the commands added here are also
	// removed in R_Shutdown
	ri.Cmd_AddCommand( "imagelist", R_ImageList_f );
	ri.Cmd_AddCommand( "shaderlist", R_ShaderList_f );
	ri.Cmd_AddCommand( "skinlist", R_SkinList_f );
	ri.Cmd_AddCommand( "modellist", R_Modellist_f );
	ri.Cmd_AddCommand( "gfxinfo", GfxInfo_f );
	ri.Cmd_AddCommand( "gfxmeminfo", GfxMemInfo_f );
	ri.Cmd_AddCommand( "exportCubemaps", R_ExportCubemaps_f );
}

static void R_InitQueries(void)
{
	if (!glRefConfig.occlusionQuery)
		return;

	if (r_drawSunRays->integer)
		qglGenQueries(ARRAY_LEN(tr.sunFlareQuery), tr.sunFlareQuery);
}

static void R_ShutDownQueries(void)
{
	if (!glRefConfig.occlusionQuery)
		return;

	if (r_drawSunRays->integer)
		qglDeleteQueries(ARRAY_LEN(tr.sunFlareQuery), tr.sunFlareQuery);
}


static void RE_SyncRender( void )
{

}


/*
===============
R_Init
===============
*/
void R_Init( void ) {
	int	err;
	byte *ptr;

	R_LOG( rch_init, SEV_INFO, "----- R_Init -----\n" );

	// clear all our internal state
	memset( &tr, 0, sizeof( tr ) );
	memset( &backEnd, 0, sizeof( backEnd ) );
	memset( &tess, 0, sizeof( tess ) );

	// glconfig_t ABI tripwire. Was 11332; bumped to 11340 when vidWidthLogical/
	// vidHeightLogical (2 ints, +8 bytes) were appended for HiDPI font scaling.
	// All native modules (engine + renderer DLLs + gamecl/gamesv) are rebuilt
	// from the same tr_types.h, so they agree on the new size; this constant is
	// the human tripwire that forces that coordinated rebuild.
	if(sizeof(glconfig_t) != 11340)
		ri.Terminate( TERM_UNRECOVERABLE, "Mod ABI incompatible: sizeof(glconfig_t) == %u != 11340", (unsigned int) sizeof(glconfig_t));

	if ( (intptr_t)tess.xyz & 15 ) {
		R_LOG( rch_init, SEV_WARN, "tess.xyz not 16 byte aligned\n" );
	}
	//memset( tess.constantColor255, 255, sizeof( tess.constantColor255 ) );

	//
	// init function tables
	//
	for ( int i = 0; i < FUNCTABLE_SIZE; i++ )
	{
		tr.sinTable[i]		= sin( DEG2RAD( i * 360.0f / ( ( float ) ( FUNCTABLE_SIZE - 1 ) ) ) );
		tr.squareTable[i]	= ( i < FUNCTABLE_SIZE/2 ) ? 1.0f : -1.0f;
		tr.sawToothTable[i] = (float)i / FUNCTABLE_SIZE;
		tr.inverseSawToothTable[i] = 1.0f - tr.sawToothTable[i];

		if ( i < FUNCTABLE_SIZE / 2 )
		{
			if ( i < FUNCTABLE_SIZE / 4 )
			{
				tr.triangleTable[i] = ( float ) i / ( FUNCTABLE_SIZE / 4 );
			}
			else
			{
				tr.triangleTable[i] = 1.0f - tr.triangleTable[i-FUNCTABLE_SIZE / 4];
			}
		}
		else
		{
			tr.triangleTable[i] = -tr.triangleTable[i-FUNCTABLE_SIZE/2];
		}
	}

	R_InitFogTable();

	R_NoiseInit();

	R_Register();

	max_polys = r_maxpolys->integer;
	if (max_polys < MAX_POLYS)
		max_polys = MAX_POLYS;

	max_polyverts = r_maxpolyverts->integer;
	if (max_polyverts < MAX_POLYVERTS)
		max_polyverts = MAX_POLYVERTS;

	ptr = ri.Hunk_Alloc( sizeof( *backEndData ) + sizeof(srfPoly_t) * max_polys + sizeof(polyVert_t) * max_polyverts, h_low);
	backEndData = (backEndData_t *) ptr;
	backEndData->polys = (srfPoly_t *) ((char *) ptr + sizeof( *backEndData ));
	backEndData->polyVerts = (polyVert_t *) ((char *) ptr + sizeof( *backEndData ) + sizeof(srfPoly_t) * max_polys);
	R_InitNextFrame();

	InitOpenGL();

	R_InitImages();

	if (glRefConfig.framebufferObject)
		FBO_Init();

	GLSL_InitGPUShaders();

	R_InitVaos();

	R_InitShaders();

	R_InitSkins();

	R_ModelInit();

	R_InitFreeType();

	R_InitQueries();


	err = qglGetError();
	if ( err != GL_NO_ERROR )
		R_LOG( rch_gl, SEV_INFO, "glGetError() = 0x%x\n", err);

	// print info
	GfxInfo_f();
	R_LOG( rch_init, SEV_INFO, "----- finished R_Init -----\n" );
}


/*
===============
RE_Shutdown
===============
*/
static void RE_Shutdown( refShutdownCode_t code ) {

	R_LOG( rch_init, SEV_INFO, "RE_Shutdown( %i )\n", code );

	ri.Cmd_RemoveCommand( "imagelist" );
	ri.Cmd_RemoveCommand( "shaderlist" );
	ri.Cmd_RemoveCommand( "skinlist" );
	ri.Cmd_RemoveCommand( "modellist" );
	ri.Cmd_RemoveCommand( "modelist" );
	// `screenshot` is registered persistently from GetRefAPI so it survives
	// map transitions; only deregister it on an actual DLL unload.
	if ( code != REF_LEVEL_ONLY ) {
		R_ScreenshotUnregisterCommands();
	}
	ri.Cmd_RemoveCommand( "gfxinfo" );
	ri.Cmd_RemoveCommand( "minimize" );
	ri.Cmd_RemoveCommand( "gfxmeminfo" );
	ri.Cmd_RemoveCommand( "exportCubemaps" );


	if ( tr.registered ) {
		R_IssuePendingRenderCommands();
		R_ShutDownQueries();
		if (glRefConfig.framebufferObject)
			FBO_Shutdown();
		R_DeleteTextures();
		R_ShutdownVaos();
		GLSL_ShutdownGPUShaders();
	}

	R_DoneFreeType();

	// shut down platform specific OpenGL stuff
	if ( code != REF_LEVEL_ONLY ) {
		if ( ri.GLimp_Shutdown ) {
			ri.GLimp_Shutdown( code == REF_UNLOAD_DLL ? qtrue: qfalse );
		}

		memset( &glConfig, 0, sizeof( glConfig ) );
		memset( &glRefConfig, 0, sizeof( glRefConfig ) );

		textureFilterAnisotropic = qfalse;
		maxAnisotropy = 0;

		memset( &glState, 0, sizeof( glState ) );
	}

	ri.FreeAll();

	tr.registered = qfalse;
}


/*
=============
RE_EndRegistration

Touch all images to make sure they are resident
=============
*/
static void RE_EndRegistration( void ) {
	R_IssuePendingRenderCommands();
	if ( !ri.Sys_LowPhysicalMemory() ) {
		RB_ShowImages();
	}
}


/*
@@@@@@@@@@@@@@@@@@@@@
GetRefAPI

@@@@@@@@@@@@@@@@@@@@@
*/
#ifdef USE_RENDERER_DLOPEN
Q_EXPORT refexport_t* QDECL GetRefAPI ( int apiVersion, refimport_t *rimp ) {
#else
refexport_t *GetRefAPI ( int apiVersion, refimport_t *rimp ) {
#endif

	static refexport_t	re;

	ri = *rimp;

	memset( &re, 0, sizeof( re ) );

	if ( apiVersion != REF_API_VERSION ) {
		R_LOG( rch_init, SEV_INFO, "Mismatched REF_API_VERSION: expected %i, got %i\n",
			REF_API_VERSION, apiVersion );
		return NULL;
	}

	// the RE_ functions are Renderer Entry points

	re.Shutdown = RE_Shutdown;

	re.BeginRegistration = RE_BeginRegistration;
	re.RegisterModel = RE_RegisterModel;
	re.RegisterSkin = RE_RegisterSkin;
	re.RegisterShader = RE_RegisterShader;
	re.RegisterShaderNoMip = RE_RegisterShaderNoMip;
	re.RegisterShaderLightMap = RE_RegisterShaderLightMap;
	re.RegisterMSDFShader = RE_RegisterMSDFShader;
	re.LoadWorld = RE_LoadWorldMap;
	re.SetWorldVisData = RE_SetWorldVisData;
	re.EndRegistration = RE_EndRegistration;

	re.BeginFrame = RE_BeginFrame;
	re.EndFrame = RE_EndFrame;

	re.MarkFragments = R_MarkFragments;
	re.LerpTag = R_LerpTag;
	re.ModelBounds = R_ModelBounds;

	re.ClearScene = RE_ClearScene;
	re.AddRefEntityToScene = RE_AddRefEntityToScene;
	re.AddPolyToScene = RE_AddPolyToScene;
	re.LightForPoint = R_LightForPoint;
	re.AddLightToScene = RE_AddLightToScene;
	re.AddAdditiveLightToScene = RE_AddAdditiveLightToScene;
	re.AddRibbonToScene = RE_AddRibbonToScene;
	re.AddBeamToScene = RE_AddBeamToScene;
	re.AddRailRibbonToScene = RE_AddRailRibbonToScene;
	re.AddSpriteToScene = RE_AddSpriteToScene;
	re.EmitParticles = RE_EmitParticles;
	re.AddDecalToScene = RE_AddDecalToScene;
	re.AddLensSourceToScene = RE_AddLensSourceToScene;
	re.GetLensVisibility = RE_GetLensVisibility;
	re.RegisterParticleClass = RE_RegisterParticleClass;
#if FEAT_HALO
	re.AddHaloToScene = RE_AddHaloToScene;
#endif
#if FEAT_FOG_SYSTEM
	re.GetGlobalFog = RE_GetGlobalFog;
	re.GetViewFog = RE_GetViewFog;
#endif
	re.RenderScene = RE_RenderScene;

	re.SetColor = RE_SetColor;
	re.SetClipRegion = RE_SetClipRegion;
	re.SetMSDFOutline = RE_SetMSDFOutline;
	re.DrawStretchPic = RE_StretchPic;
	re.DrawRotatedPic = RE_RotatedPic;
	re.DrawLine = RE_DrawLine;
	re.DrawStretchRaw = RE_StretchRaw;
	re.UploadCinematic = RE_UploadCinematic;

	re.RegisterFont = RE_RegisterFont;
	re.RemapShader = R_RemapShader;
	re.GetEntityToken = R_GetEntityToken;
	re.inPVS = R_inPVS;

	re.TakeVideoFrame = RE_TakeVideoFrame;
	re.SetColorMappings = R_SetColorMappings;

	re.ThrottleBackend = RE_ThrottleBackend;
	re.CanMinimize = RE_CanMinimize;
	re.GetConfig = RE_GetConfig;
	re.VertexLighting = RE_VertexLighting;
	re.SyncRender = RE_SyncRender;

#if FEAT_IQM
	re.GetIQMAnimations = R_GetIQMAnimations;
#endif // FEAT_IQM

	// `screenshot` grammar (renderercommon/tr_screenshot.c) is registered
	// once per DLL load so it persists across map transitions — R_Register
	// commands are torn down on every REF_LEVEL_ONLY shutdown.
	R_ScreenshotRegisterCommands();

	return &re;
}
