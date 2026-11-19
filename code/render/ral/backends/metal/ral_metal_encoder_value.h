// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#ifndef WIRED_METAL_ENCODER_VALUE_H
#define WIRED_METAL_ENCODER_VALUE_H

#import <Metal/Metal.h>

/* Stack-only observation, never resource ownership. Instantiate anew for each
 * exclusive encoder/cohort scope: another draw path may have changed state.
 * First use always writes, including nil/default-valued state. */
template <typename T> class wiredMetalEncoderValue {
	T value{};
	bool valid = false;
public:
	bool wired_changed( T next ) {
		if ( valid && value == next ) return false;
		value = next;
		valid = true;
		return true;
	}
};

/* Exclusive material cohort only. Offset changes do not change immutable
 * packet storage or its completion-bound lifetime. Start fresh after another
 * draw path owns either slot, or after restarting the native encoder. */
static inline void wired_metal_bind_material_buffer(
		id<MTLRenderCommandEncoder> encoder, id<MTLBuffer> buffer,
		NSUInteger offset, wiredMetalEncoderValue<id<MTLBuffer>> &binding ) {
	if ( binding.wired_changed( buffer ) ) {
		[encoder setVertexBuffer:buffer offset:offset atIndex:2u];
		[encoder setFragmentBuffer:buffer offset:offset atIndex:0u];
	} else {
		[encoder setVertexBufferOffset:offset atIndex:2u];
		[encoder setFragmentBufferOffset:offset atIndex:0u];
	}
}

#endif
