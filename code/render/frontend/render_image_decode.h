// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#ifndef WIRED_RENDER_FRONTEND_IMAGE_DECODE_H
#define WIRED_RENDER_FRONTEND_IMAGE_DECODE_H

#include "q_shared.h"
#include "tr_public.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Decodes through the backend/platform adapter using renderer VFS imports.
 * The returned RGBA8 bytes belong to ri and must be released with ri.Free. */
qboolean RenderImage_DecodeRgba8( const char *name, byte **outPixels,
	uint32_t *outWidth, uint32_t *outHeight, char outResolved[MAX_QPATH] );

#ifdef __cplusplus
}
#endif

#endif
