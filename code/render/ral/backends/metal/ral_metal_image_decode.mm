// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

extern "C" {
#include "render_image_decode.h"
}
#include "render_submission_material.h"

#include <stdio.h>
#include <string.h>
#include <strings.h>

extern "C" {
void R_LoadPNG( const char *name, byte **pic, int *width, int *height );
void R_LoadJPG( const char *name, byte **pic, int *width, int *height );
void R_LoadTGA( const char *name, byte **pic, int *width, int *height );
void R_LoadBMP( const char *name, byte **pic, int *width, int *height );
}

typedef void (*imageLoader_t)( const char *, byte **, int *, int * );
typedef struct {
	const char *extension;
	imageLoader_t loader;
} imageExtension_t;

static const imageExtension_t s_extensions[] = {
	{ "tga", R_LoadTGA }, { "png", R_LoadPNG }, { "jpg", R_LoadJPG },
	{ "jpeg", R_LoadJPG }, { "bmp", R_LoadBMP }
};

static const char *Extension( const char *path ) {
	const char *dot = strrchr( path, '.' );
	const char *slash = strrchr( path, '/' );
	return ( dot && ( !slash || dot > slash ) && dot[1] ) ? dot + 1 : "";
}

static qboolean DecodePath( const char *path, imageLoader_t loader,
		byte **outPixels,
		uint32_t *outWidth, uint32_t *outHeight,
		char outResolved[MAX_QPATH] ) {
	byte *pixels = NULL;
	int width = 0, height = 0;
	uint64_t byteCount;
	loader( path, &pixels, &width, &height );
	if ( !pixels ) return qfalse;
	byteCount = width > 0 && height > 0
		? (uint64_t)width * (uint64_t)height * 4u : 0u;
	if ( width <= 0 || height <= 0
			|| (uint32_t)width > RENDER_SUBMISSION_MAX_IMAGE_DIMENSION
			|| (uint32_t)height > RENDER_SUBMISSION_MAX_IMAGE_DIMENSION
			|| byteCount > RENDER_SUBMISSION_MAX_MATERIAL_BYTES ) {
		ri.Free( pixels );
		return qfalse;
	}
	/* The shared Q3 loaders publish straight RGBA8 and preserve RGB under a
	 * fully-zero legacy alpha plane.  ImageIO/CoreGraphics premultiplied those
	 * pixels to black, making skull_door_* geometry Metal-only invisible. */
	*outPixels = pixels;
	*outWidth = (uint32_t)width; *outHeight = (uint32_t)height;
	(void)snprintf( outResolved, MAX_QPATH, "%s", path );
	return qtrue;
}

extern "C" qboolean RenderImage_DecodeRgba8( const char *name,
		byte **outPixels, uint32_t *outWidth, uint32_t *outHeight,
		char outResolved[MAX_QPATH] ) {
	char stem[MAX_QPATH], candidate[MAX_QPATH];
	const char *extension;
	uint32_t i;
	if ( !name || !name[0] || strlen( name ) >= MAX_QPATH || !outPixels
			|| !outWidth || !outHeight || !outResolved || !ri.FS_ReadFile
			|| !ri.FS_FreeFile || !ri.Malloc || !ri.Free ) return qfalse;
	*outPixels = NULL; *outWidth = *outHeight = 0u; outResolved[0] = '\0';
	extension = Extension( name );
	if ( extension[0] ) {
		for ( i = 0u; i < ARRAY_LEN( s_extensions ); ++i ) {
			if ( !strcasecmp( extension, s_extensions[i].extension )
					&& DecodePath( name, s_extensions[i].loader,
						outPixels, outWidth, outHeight,
						outResolved ) ) return qtrue;
		}
	}
	(void)snprintf( stem, sizeof( stem ), "%s", name );
	{
		char *dot = strrchr( stem, '.' );
		char *slash = strrchr( stem, '/' );
		if ( dot && ( !slash || dot > slash ) ) *dot = '\0';
	}
	for ( i = 0u; i < ARRAY_LEN( s_extensions ); ++i ) {
		int written;
		if ( extension[0] && !strcasecmp( extension, s_extensions[i].extension ) ) continue;
		written = snprintf( candidate, sizeof( candidate ), "%s.%s", stem,
			s_extensions[i].extension );
		if ( written < 0 || (size_t)written >= sizeof( candidate ) ) continue;
		if ( DecodePath( candidate, s_extensions[i].loader,
				outPixels, outWidth, outHeight,
				outResolved ) ) return qtrue;
	}
	return qfalse;
}
