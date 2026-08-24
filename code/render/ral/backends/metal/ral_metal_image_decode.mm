// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

extern "C" {
#include "render_image_decode.h"
}
#include "render_submission_material.h"

#import <CoreGraphics/CoreGraphics.h>
#import <ImageIO/ImageIO.h>

#include <stdio.h>
#include <string.h>
#include <strings.h>

typedef struct {
	const char *extension;
} imageExtension_t;

static const imageExtension_t s_extensions[] = {
	{ "png" }, { "jpg" }, { "jpeg" }, { "tga" }, { "bmp" }
};

static const char *Extension( const char *path ) {
	const char *dot = strrchr( path, '.' );
	const char *slash = strrchr( path, '/' );
	return ( dot && ( !slash || dot > slash ) && dot[1] ) ? dot + 1 : "";
}

static qboolean DecodePath( const char *path, byte **outPixels,
		uint32_t *outWidth, uint32_t *outHeight,
		char outResolved[MAX_QPATH] ) {
	void *fileBytes = NULL;
	int fileLength = ri.FS_ReadFile( path, &fileBytes );
	CFDataRef data = NULL;
	CGImageSourceRef source = NULL;
	CGImageRef image = NULL;
	CGColorSpaceRef colorSpace = NULL;
	CGContextRef context = NULL;
	byte *pixels = NULL;
	size_t width, height, rowBytes, byteCount;
	qboolean result = qfalse;
	if ( fileLength <= 0 || !fileBytes ) return qfalse;
	data = CFDataCreate( kCFAllocatorDefault, (const UInt8 *)fileBytes,
		(CFIndex)fileLength );
	ri.FS_FreeFile( fileBytes ); fileBytes = NULL;
	if ( !data ) goto cleanup;
	source = CGImageSourceCreateWithData( data, NULL );
	if ( !source ) goto cleanup;
	image = CGImageSourceCreateImageAtIndex( source, 0u, NULL );
	if ( !image ) goto cleanup;
	width = CGImageGetWidth( image ); height = CGImageGetHeight( image );
	if ( !width || !height || width > RENDER_SUBMISSION_MAX_IMAGE_DIMENSION
			|| height > RENDER_SUBMISSION_MAX_IMAGE_DIMENSION
			|| width > SIZE_MAX / 4u ) goto cleanup;
	rowBytes = width * 4u;
	if ( height > SIZE_MAX / rowBytes
			|| rowBytes * height > RENDER_SUBMISSION_MAX_MATERIAL_BYTES ) goto cleanup;
	byteCount = rowBytes * height;
	pixels = (byte *)ri.Malloc( byteCount );
	if ( !pixels ) goto cleanup;
	colorSpace = CGColorSpaceCreateWithName( kCGColorSpaceSRGB );
	if ( !colorSpace ) goto cleanup;
	context = CGBitmapContextCreate( pixels, width, height, 8u, rowBytes,
		colorSpace, kCGImageAlphaPremultipliedLast | kCGBitmapByteOrder32Big );
	if ( !context ) goto cleanup;
	CGContextTranslateCTM( context, 0.0, (CGFloat)height );
	CGContextScaleCTM( context, 1.0, -1.0 );
	CGContextSetBlendMode( context, kCGBlendModeCopy );
	CGContextDrawImage( context, CGRectMake( 0.0, 0.0, (CGFloat)width,
		(CGFloat)height ), image );
	for ( size_t offset = 0u; offset < byteCount; offset += 4u ) {
		const uint32_t alpha = pixels[offset + 3u];
		if ( alpha != 0u && alpha != 255u ) {
			for ( size_t channel = 0u; channel < 3u; ++channel ) {
				uint32_t straight = ( (uint32_t)pixels[offset + channel] * 255u
					+ alpha / 2u ) / alpha;
				pixels[offset + channel] = (byte)( straight > 255u ? 255u : straight );
			}
		}
	}
	*outPixels = pixels; pixels = NULL;
	*outWidth = (uint32_t)width; *outHeight = (uint32_t)height;
	(void)snprintf( outResolved, MAX_QPATH, "%s", path );
	result = qtrue;
cleanup:
	if ( pixels ) ri.Free( pixels );
	if ( context ) CGContextRelease( context );
	if ( colorSpace ) CGColorSpaceRelease( colorSpace );
	if ( image ) CGImageRelease( image );
	if ( source ) CFRelease( source );
	if ( data ) CFRelease( data );
	return result;
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
					&& DecodePath( name, outPixels, outWidth, outHeight,
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
		if ( DecodePath( candidate, outPixels, outWidth, outHeight,
				outResolved ) ) return qtrue;
	}
	return qfalse;
}
