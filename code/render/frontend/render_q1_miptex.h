// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#ifndef WIRED_RENDER_Q1_MIPTEX_H
#define WIRED_RENDER_Q1_MIPTEX_H

/* Quake 1 textures live inside their .bsp (the miptex lump), not in the VFS as image files. The RAL material path addresses one of
 * them by an embedded image reference, "<bsp path>#<miptex name>" (for example "maps/e1m5.bsp#city5_4"): the VFS resolves it to the
 * container's identity extended by the member (FS_ResolveResource), and every backend's image decoder hands such a name to
 * RenderQ1Miptex_DecodeImage. The same reference is produced at cook time and at runtime by RenderQ1Miptex_MaterialImage, so the
 * cooked texture key and the resident texture key agree. This module is pure: no renderer globals, no allocation of its own. */

#include "../../qcommon/q_shared.h"
#include "tr_public.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RENDER_Q1_MIPTEX_REFERENCE_SEPARATOR '#'

/* The canonical Quake palette (gfx/palette.lmp); indices 224..255 are fullbright. */
extern const byte render_q1_palette[768];

/* True when `name` is an embedded image reference ("<file>#<member>"). */
qboolean RenderQ1Miptex_IsReference( const char *name );

/* Locates miptex `member` (case-insensitive) in a BSP29 / BSP2 file image and reports its top mip level: the indexed pixels and
 * their extent. Fails closed on any out-of-range offset or extent. */
qboolean RenderQ1Miptex_Find( const byte *bsp, size_t bspLength, const char *member,
	const byte **outPixels, uint32_t *outWidth, uint32_t *outHeight );

/* Expands indexed pixels to RGBA8. Fullbright indices keep their palette colour (the base layer must stand on its own); a `{` texture
 * (alpha cut-out) makes index 255 transparent. */
void RenderQ1Miptex_ExpandRgba8( const char *member, const byte *indexed, uint32_t pixelCount, byte *outRgba );

/* Decodes an embedded reference through the renderer imports: reads the container with FS_ReadFile and returns RGBA8 allocated with
 * imports->Malloc (released by the caller with imports->Free); outResolved receives the reference. False when `name` is not a
 * reference, the container cannot be read, or the member is missing. */
qboolean RenderQ1Miptex_DecodeImage( const refimport_t *imports, const char *name, byte **outPixels,
	uint32_t *outWidth, uint32_t *outHeight, char outResolved[MAX_QPATH] );

/* The image a world material of a Quake 1 map draws: "<map name>#<material name>" when `map` carries an embedded texture lump that
 * holds that miptex. False for any other map or name (the caller then keeps the name as an ordinary image path). */
qboolean RenderQ1Miptex_MaterialImage( const mapFile_t *map, const char *materialName, char outImage[MAX_QPATH] );

#ifdef __cplusplus
}
#endif

#endif
