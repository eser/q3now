// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include "../code/render/ral/backends/metal/ral_metal_exposure.h"
#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL %d: %s\n", __LINE__, #x); return 1; } } while (0)
static_assert(sizeof(ralExposureBlock_t) == 64u, "canonical exposure ABI");

static id<MTLTexture> Image(id<MTLDevice> device, float luminance) {
	MTLTextureDescriptor *desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:
		MTLPixelFormatRGBA32Float width:17u height:19u mipmapped:NO];
	desc.storageMode = MTLStorageModeShared;
	desc.usage = MTLTextureUsageShaderRead;
	id<MTLTexture> image = [device newTextureWithDescriptor:desc];
	float pixels[17u * 19u * 4u];
	for (unsigned i = 0u; i < 17u * 19u; ++i) {
		pixels[i*4u] = pixels[i*4u+1u] = pixels[i*4u+2u] = luminance;
		pixels[i*4u+3u] = 1.0f;
	}
	[image replaceRegion:MTLRegionMake2D(0u,0u,17u,19u) mipmapLevel:0u
		withBytes:pixels bytesPerRow:17u*4u*sizeof(float)];
	return image;
}
static float Value(id<MTLBuffer> buffer) {
	return ((const ralExposureBlock_t *)buffer.contents)->exposure_bias;
}

int main(void) {
	@autoreleasepool {
		id<MTLDevice> device = MTLCreateSystemDefaultDevice();
		CHECK(device);
		id<MTLCommandQueue> queue = [device newCommandQueue];
		ralMetalExposure_t *state = RalMetal_ExposureCreate(device);
		CHECK(queue && state);
		id<MTLTexture> dark = Image(device, 0.002f);
		id<MTLTexture> bright = Image(device, 4.0f);
		id<MTLTexture> black = Image(device, 0.0f);
		CHECK(dark && bright && black);
		ralExposureSettings_t settings = {1, 0.0123f, 0.5f, 0.2f, 1.5f, 0.5f, 0.25f, 8.0f};
		CHECK(Ral_ExposureSettingsValid(&settings));
		id<MTLBuffer> seed = nil, up = nil, down = nil, held = nil, reset = nil;
		id<MTLCommandBuffer> first = [queue commandBuffer];
		CHECK(RalMetal_ExposureEncode(state, first, dark, &settings, 0.0f, true, &seed));
		[first commit];
		// Queue multiple frames before waiting: outputs must remain independent.
		id<MTLCommandBuffer> second = [queue commandBuffer];
		CHECK(RalMetal_ExposureEncode(state, second, dark, &settings, 0.1f, false, &up));
		[second commit];
		id<MTLCommandBuffer> third = [queue commandBuffer];
		CHECK(RalMetal_ExposureEncode(state, third, bright, &settings, 0.1f, false, &down));
		[third commit]; [third waitUntilCompleted];
		CHECK(first.status == MTLCommandBufferStatusCompleted
			&& second.status == MTLCommandBufferStatusCompleted
			&& third.status == MTLCommandBufferStatusCompleted);
		CHECK(seed != up && up != down);
		CHECK(fabsf(Value(seed) - 1.0f) < 0.00001f);
		CHECK(Value(up) > 1.5f && Value(up) < 2.0f);
		const float expectedDown = Value(up) + (0.25f - Value(up)) * (1.0f-expf(-0.05f));
		CHECK(fabsf(Value(down)-expectedDown) < 0.0001f);
		// Cancel a reset and a reduction together; neither may mutate history.
		@autoreleasepool {
			id<MTLCommandBuffer> canceled = [queue commandBuffer];
			id<MTLBuffer> ignored = nil;
			CHECK(RalMetal_ExposureEncode(state, canceled, dark, &settings, 0.1f, true, &ignored));
			[ignored release];
		}
		id<MTLCommandBuffer> fourth = [queue commandBuffer];
		CHECK(RalMetal_ExposureEncode(state, fourth, black, &settings, 0.1f, false, &held));
		[fourth commit]; [fourth waitUntilCompleted];
		CHECK(fourth.status == MTLCommandBufferStatusCompleted);
		CHECK(fabsf(Value(held)-Value(down)) < 0.00001f);
		id<MTLCommandBuffer> fifth = [queue commandBuffer];
		CHECK(RalMetal_ExposureEncode(state, fifth, bright, &settings, 0.0f, true, &reset));
		[fifth commit]; [fifth waitUntilCompleted];
		CHECK(fifth.status == MTLCommandBufferStatusCompleted && Value(reset) == 1.0f);
		// Invalid input must neither publish a buffer nor encode GPU work.
		id<MTLBuffer> unchanged = seed;
		id<MTLCommandBuffer> invalid = [queue commandBuffer];
		settings.key = NAN;
		CHECK(!RalMetal_ExposureEncode(state, invalid, dark, &settings, 0.1f, false, &unchanged));
		settings.key = 0.0123f; settings.enabled = 0;
		CHECK(!RalMetal_ExposureEncode(state, invalid, dark, &settings, 0.1f, false, &unchanged));
		settings.enabled = 1;
		CHECK(!RalMetal_ExposureEncode(state, invalid, dark, &settings, -1.0f, false, &unchanged));
		CHECK(!RalMetal_ExposureEncode(state, invalid, dark, &settings, NAN, false, &unchanged));
		CHECK(unchanged == seed);
		CHECK(((ralExposureBlock_t *)up.contents)->brightness == 1.0f);
		CHECK(((ralExposureBlock_t *)up.contents)->key == settings.key);
		[seed release]; [up release]; [down release]; [held release]; [reset release];
		[dark release]; [bright release]; [black release];
		RalMetal_ExposureDestroy(state);
		[queue release]; [device release];
		puts("Metal exposure: real GPU histogram/reduce, ordered frames, reset, cancellation, black retention and atomic rejection PASS");
	}
	return 0;
}
