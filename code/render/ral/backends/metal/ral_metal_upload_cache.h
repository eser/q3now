// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
#ifndef WIRED_METAL_UPLOAD_CACHE_H
#define WIRED_METAL_UPLOAD_CACHE_H

#import <Metal/Metal.h>

/* CPU-owned storage only; never a submission queue or a source of GPU waits. */
struct wiredMetalUploadCache {
	struct Entry { id<MTLBuffer> buffer; id<MTLCommandBuffer> reader; } slots[2];
	void wired_clear() {
		for ( auto &slot : slots ) {
			[slot.reader release]; slot.reader = nil;
			[slot.buffer release]; slot.buffer = nil;
		}
	}
	void wired_reap() {
		for ( auto &slot : slots ) {
			if ( !slot.reader ) continue;
			const MTLCommandBufferStatus status = slot.reader.status;
			if ( status == MTLCommandBufferStatusCompleted || status == MTLCommandBufferStatusError ) {
				[slot.reader release]; slot.reader = nil;
				if ( status == MTLCommandBufferStatusError ) {
					[slot.buffer release]; slot.buffer = nil;
				}
			}
		}
	}
};

/* One immutable upload per attempt. Publish only after successful submission;
 * failures leave storage to native command retention, including partial submit.
 * Busy/undersized entries are never overwritten. If both are busy, the upload
 * is transient rather than growing the cache or waiting for completion. */
struct wiredMetalUpload {
	wiredMetalUploadCache &cache;
	id<MTLBuffer> buffer = nil;
	unsigned cacheSlot = 2u;
	explicit wiredMetalUpload( wiredMetalUploadCache &owner ) : cache( owner ) {}
	wiredMetalUpload( const wiredMetalUpload & ) = delete;
	~wiredMetalUpload() { [buffer release]; }
	bool wired_reserve( id<MTLDevice> device, NSUInteger bytes, MTLResourceOptions options ) {
		if ( !bytes ) return false;
		if ( buffer ) return buffer.length >= bytes;
		cacheSlot = 2u;
		cache.wired_reap();
		for ( unsigned i = 0u; i < 2u; ++i ) {
			auto &slot = cache.slots[i];
			if ( slot.reader ) continue;
			if ( cacheSlot == 2u ) cacheSlot = i;
			if ( slot.buffer && slot.buffer.length >= bytes ) {
				buffer = slot.buffer; slot.buffer = nil; // transfer cache retain
				cacheSlot = i;
				break;
			}
		}
		if ( !buffer ) buffer = [device newBufferWithLength:bytes options:options];
		return buffer != nil;
	}
	void wired_publish( id<MTLCommandBuffer> lastReader ) {
		if ( !buffer || !lastReader || cacheSlot == 2u ) return;
		auto &slot = cache.slots[cacheSlot];
		[slot.buffer release];
		slot.buffer = [buffer retain];
		slot.reader = [lastReader retain];
	}
};
#endif
