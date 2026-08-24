// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "render_image_decode.h"
#include "render_submission_material.h"

#include <stdio.h>
#include <string.h>
#include <strings.h>

void R_LoadPNG( const char *name, byte **pic, int *width, int *height );
void R_LoadJPG( const char *name, byte **pic, int *width, int *height );
void R_LoadTGA( const char *name, byte **pic, int *width, int *height );
void R_LoadBMP( const char *name, byte **pic, int *width, int *height );

typedef void (*imageLoader_t)( const char *, byte **, int *, int * );
typedef struct { const char *extension; imageLoader_t loader; } imageFormat_t;
static const imageFormat_t s_formats[] = {
	{ "tga", R_LoadTGA }, { "png", R_LoadPNG }, { "jpg", R_LoadJPG },
	{ "jpeg", R_LoadJPG }, { "bmp", R_LoadBMP }
};

static const char *Extension( const char *path ) {
	const char *dot = strrchr( path, '.' );
	const char *slash = strrchr( path, '/' );
	return dot && ( !slash || dot > slash ) && dot[1] ? dot + 1 : "";
}

static qboolean DecodeCandidate( const char *path, imageLoader_t loader,
		byte **outPixels, uint32_t *outWidth, uint32_t *outHeight,
		char outResolved[MAX_QPATH] ) {
	byte *pixels = NULL;
	int width = 0, height = 0;
	uint64_t bytes;
	loader( path, &pixels, &width, &height );
	if ( !pixels ) return qfalse;
	bytes = width > 0 && height > 0 ? (uint64_t)width * (uint64_t)height * 4u : 0u;
	if ( width <= 0 || height <= 0
			|| (uint32_t)width > RENDER_SUBMISSION_MAX_IMAGE_DIMENSION
			|| (uint32_t)height > RENDER_SUBMISSION_MAX_IMAGE_DIMENSION
			|| bytes > RENDER_SUBMISSION_MAX_MATERIAL_BYTES ) {
		ri.Free( pixels ); return qfalse;
	}
	*outPixels = pixels;
	*outWidth = (uint32_t)width; *outHeight = (uint32_t)height;
	(void)snprintf( outResolved, MAX_QPATH, "%s", path );
	return qtrue;
}

qboolean RenderImage_DecodeRgba8( const char *name, byte **outPixels,
		uint32_t *outWidth, uint32_t *outHeight,
		char outResolved[MAX_QPATH] ) {
	char stem[MAX_QPATH], candidate[MAX_QPATH];
	const char *extension;
	if ( !name || !name[0] || strlen( name ) >= MAX_QPATH || !outPixels
			|| !outWidth || !outHeight || !outResolved || !ri.FS_ReadFile
			|| !ri.FS_FreeFile || !ri.Malloc || !ri.Free ) return qfalse;
	*outPixels = NULL; *outWidth = *outHeight = 0u; outResolved[0] = '\0';
	extension = Extension( name );
	for ( uint32_t i = 0u; i < ARRAY_LEN( s_formats ); ++i ) {
		if ( extension[0] && !strcasecmp( extension, s_formats[i].extension )
				&& DecodeCandidate( name, s_formats[i].loader, outPixels,
					outWidth, outHeight, outResolved ) ) return qtrue;
	}
	(void)snprintf( stem, sizeof( stem ), "%s", name );
	{
		char *dot = strrchr( stem, '.' );
		char *slash = strrchr( stem, '/' );
		if ( dot && ( !slash || dot > slash ) ) *dot = '\0';
	}
	for ( uint32_t i = 0u; i < ARRAY_LEN( s_formats ); ++i ) {
		int written;
		if ( extension[0] && !strcasecmp( extension, s_formats[i].extension ) )
			continue;
		written = snprintf( candidate, sizeof( candidate ), "%s.%s", stem,
			s_formats[i].extension );
		if ( written <= 0 || (size_t)written >= sizeof( candidate ) ) continue;
		if ( DecodeCandidate( candidate, s_formats[i].loader, outPixels,
				outWidth, outHeight, outResolved ) ) return qtrue;
	}
	return qfalse;
}
