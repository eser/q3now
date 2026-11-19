// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
#ifndef WIRED_RENDER_SKY_ATLAS_H
#define WIRED_RENDER_SKY_ATLAS_H

#include "render_image_decode.h"
#include "render_submission.h"
#include <limits.h>
#include <stdio.h>
#include <string.h>

/* Registration-time representation used by the shared atlas shader. Preserve
 * Q3 rt/bk/lf/ft/up/dn pixels verbatim; GPU direction mapping owns orientation.
 * One decoded face plus the bounded atlas is live at a time. Publish only after
 * all six faces validate. The caller owns the result through imports->Free. */
static inline qboolean wired_sky_atlas_decode( const refimport_t *imports,
		const char *prefix, byte **outPixels, uint32_t *outWidth,
		uint32_t *outHeight ) {
	static const char *suffixes[6] = { "rt", "bk", "lf", "ft", "up", "dn" };
	byte *atlas = NULL, *pixels = NULL;
	uint32_t faceWidth = 0u, faceHeight = 0u, atlasWidth = 0u;
	if ( !imports || !imports->Malloc || !imports->Free || !prefix
			|| !prefix[0] || !outPixels || !outWidth || !outHeight ) return qfalse;
	for ( uint32_t face = 0u; face < 6u; ++face ) {
		char path[MAX_QPATH], resolved[MAX_QPATH];
		uint32_t width = 0u, height = 0u;
		const int length = snprintf( path, sizeof( path ), "%s_%s", prefix,
			suffixes[face] );
		if ( length <= 0 || length >= (int)sizeof( path )
				|| !RenderImage_DecodeRgba8( path, &pixels, &width, &height,
					resolved ) || !pixels || !width || !height ) goto reject;
		if ( face == 0u ) {
			uint64_t atlasBytes;
			/* Check dimensions before multiplying, including the consumer limit. */
			if ( width > RENDER_SUBMISSION_MAX_IMAGE_DIMENSION / 3u
					|| height > RENDER_SUBMISSION_MAX_IMAGE_DIMENSION / 2u ) goto reject;
			faceWidth = width; faceHeight = height; atlasWidth = width * 3u;
			atlasBytes = (uint64_t)atlasWidth * height * 2u * 4u;
			if ( atlasBytes > RENDER_SUBMISSION_MAX_MATERIAL_BYTES
					|| atlasBytes > INT_MAX ) goto reject;
			atlas = (byte *)imports->Malloc( (int)atlasBytes );
			if ( !atlas ) goto reject;
		} else if ( width != faceWidth || height != faceHeight ) goto reject;
		for ( uint32_t row = 0u; row < faceHeight; ++row )
			memcpy( atlas + ( (size_t)( (face / 3u) * faceHeight + row )
					* atlasWidth + (face % 3u) * faceWidth ) * 4u,
				pixels + (size_t)row * faceWidth * 4u, (size_t)faceWidth * 4u );
		imports->Free( pixels ); pixels = NULL;
	}
	*outPixels = atlas; *outWidth = atlasWidth; *outHeight = faceHeight * 2u;
	return qtrue;
reject:
	if ( pixels ) imports->Free( pixels );
	if ( atlas ) imports->Free( atlas );
	return qfalse;
}
#endif
