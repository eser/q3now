// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
#ifndef WIRED_RENDER_TEXTURE_SAMPLING_H
#define WIRED_RENDER_TEXTURE_SAMPLING_H

#include "ral_resource.h"
#include <math.h>
#include <string.h>
#include <ctype.h>

/* Compatibility cvar names end here: adapters consume a portable descriptor.
 * Match Vulkan's existing no-anisotropy policy for nearest mip/mag filtering.
 * Device-specific anisotropy limits are applied by the adapter. */
static inline qboolean wired_texture_sampling_policy( const char *mode,
		qboolean anisotropic, float maxAnisotropy, ralSamplerCreateInfo_t *out ) {
	static const char *const names[] = { "GL_NEAREST", "GL_LINEAR",
		"GL_NEAREST_MIPMAP_NEAREST", "GL_LINEAR_MIPMAP_NEAREST",
		"GL_NEAREST_MIPMAP_LINEAR", "GL_LINEAR_MIPMAP_LINEAR" };
	unsigned index;
	ralSamplerCreateInfo_t result;
	if ( !mode || !out || !isfinite( maxAnisotropy ) || maxAnisotropy < 1.0f
			|| ( anisotropic != qfalse && anisotropic != qtrue ) ) return qfalse;
	for ( index = 0; index < 6u; ++index ) {
		const unsigned char *a = (const unsigned char *)mode;
		const unsigned char *b = (const unsigned char *)names[index];
		while ( *a && tolower( *a ) == tolower( *b ) ) { ++a; ++b; }
		if ( *a == *b ) break;
	}
	if ( index == 6u ) return qfalse;
	memset( &result, 0, sizeof( result ) );
	result.minFilter = result.magFilter = ( index & 1u )
		? RAL_FILTER_LINEAR : RAL_FILTER_NEAREST;
	result.mipmapMode = index >= 4u ? RAL_MIPMAP_LINEAR : RAL_MIPMAP_NEAREST;
	result.maxLod = index < 2u ? 0.25f : 0.0f;
	result.compareOp = RAL_COMPARE_ALWAYS;
	result.maxAnisotropy = anisotropic && index == 5u ? maxAnisotropy : 1.0f;
	*out = result;
	return qtrue;
}
#endif
