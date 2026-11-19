// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#ifndef WIRED_METAL_FRAGMENT_BINDINGS_H
#define WIRED_METAL_FRAGMENT_BINDINGS_H

#import <Metal/Metal.h>
#include <string.h>

/* Stack-only observation of one uninterrupted material draw loop. It owns no
 * resources: native encoders retain bound objects as before. Start a fresh
 * instance after ANY external writer of these slots or encoder restart; never
 * carry this state across submissions, passes, or resource publication.
 * Texture and sampler tables change independently (notably filter reload). */
template <NSUInteger Slots> class wiredMetalFragmentBindings {
	id<MTLTexture> textures[Slots] = {};
	id<MTLSamplerState> samplers[Slots] = {};
	bool texturesValid = false, samplersValid = false;
public:
	bool wired_textures_changed( id<MTLTexture> const (&next)[Slots] ) {
		if ( texturesValid && memcmp( textures, next, sizeof( textures ) ) == 0 )
			return false;
		memcpy( textures, next, sizeof( textures ) );
		texturesValid = true;
		return true;
	}
	bool wired_samplers_changed( id<MTLSamplerState> const (&next)[Slots] ) {
		if ( samplersValid && memcmp( samplers, next, sizeof( samplers ) ) == 0 )
			return false;
		memcpy( samplers, next, sizeof( samplers ) );
		samplersValid = true;
		return true;
	}
};

#endif
