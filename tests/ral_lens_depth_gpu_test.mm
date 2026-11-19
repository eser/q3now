// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include "wired/render/primitives.h"

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL %d: %s\n", __LINE__, #x); return 1; } } while (0)

// Run the generated canonical compute shader on a real GPU. This verifies
// standard/reversed device-depth math, not an OpenGL context or driver.
struct LensPush {
	float viewport[2];
	int32_t count;
	float radiusPx;
	float depthConvention[4];
};
struct LensSource { float screenPosDepth[4], colorVis[4]; };
static_assert(sizeof(LensPush) == 32u, "canonical lens push ABI");
static_assert(sizeof(LensSource) == 32u, "canonical lens SSBO stride");

int main(int argc, const char **argv) {
	@autoreleasepool {
		CHECK(argc == 2);
		id<MTLDevice> device = MTLCreateSystemDefaultDevice();
		CHECK(device != nil);
		NSError *error = nil;
		NSString *source = [NSString stringWithContentsOfFile:
			[NSString stringWithUTF8String:argv[1]] encoding:NSUTF8StringEncoding error:&error];
		CHECK(source != nil);
		id<MTLLibrary> library = [device newLibraryWithSource:source options:nil error:&error];
		if (!library) fprintf(stderr, "%s\n", [[error description] UTF8String]);
		CHECK(library != nil);
		id<MTLFunction> function = [library newFunctionWithName:@"main0"];
		CHECK(function != nil);
		id<MTLComputePipelineState> pipeline = [device newComputePipelineStateWithFunction:function error:&error];
		CHECK(pipeline != nil);
		id<MTLArgumentEncoder> args = [function newArgumentEncoderWithBufferIndex:0u];
		CHECK(args != nil);
		id<MTLBuffer> argBuffer = [device newBufferWithLength:args.encodedLength options:MTLResourceStorageModeShared];
		CHECK(argBuffer != nil);
		[args setArgumentBuffer:argBuffer offset:0u];
		const unsigned capacity = ((LENS_MAX_CGSOURCES + 63u) / 64u + 1u) * 64u;
		id<MTLBuffer> records = [device newBufferWithLength:capacity * sizeof(LensSource) options:MTLResourceStorageModeShared];
		CHECK(records != nil);
		MTLTextureDescriptor *desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatR32Float width:5u height:5u mipmapped:NO];
		desc.storageMode = MTLStorageModeShared;
		desc.usage = MTLTextureUsageShaderRead;
		id<MTLTexture> depth = [device newTextureWithDescriptor:desc];
		CHECK(depth != nil);
		MTLSamplerDescriptor *samplerDesc = [[MTLSamplerDescriptor alloc] init];
		samplerDesc.minFilter = samplerDesc.magFilter = MTLSamplerMinMagFilterNearest;
		samplerDesc.sAddressMode = samplerDesc.tAddressMode = MTLSamplerAddressModeClampToEdge;
		samplerDesc.supportArgumentBuffers = YES;
		id<MTLSamplerState> sampler = [device newSamplerStateWithDescriptor:samplerDesc];
		CHECK(sampler != nil);
		[args setBuffer:records offset:0u atIndex:0u];
		[args setTexture:depth atIndex:1u];
		[args setSamplerState:sampler atIndex:2u];
		id<MTLCommandQueue> queue = [device newCommandQueue];
		CHECK(queue != nil);
		unsigned cases = 0;
		for (unsigned standard = 0; standard < 2; ++standard) {
			const float sign = standard ? -1.0f : 1.0f;
			for (unsigned scenario = 0; scenario < 7; ++scenario) {
				float pixels[25];
				for (unsigned i = 0; i < 25; ++i) {
					float delta = -0.25f; // clear
					if (scenario == 1) delta = 0.25f; // occluded
					if (scenario == 2) delta = 0.0f; // own surface
					if (scenario == 3) delta = 0.0004f; // inside bias
					if (scenario == 4) delta = 0.0016f; // outside bias
					if (scenario >= 5 && i % 5 >= 3) delta = 0.25f; // right three taps
					pixels[i] = 0.5f + sign * delta;
				}
				[depth replaceRegion:MTLRegionMake2D(0u, 0u, 5u, 5u) mipmapLevel:0u withBytes:pixels bytesPerRow:5u * sizeof(float)];
				LensSource *data = (LensSource *)records.contents;
				for (unsigned i = 0; i < capacity; ++i) {
					data[i] = {{0.5f, 0.5f, 0.5f, 0.0f}, {1.0f, 0.0f, 0.0f, -7.0f}};
				}
				data[1].screenPosDepth[0] = -0.01f;
				data[2].colorVis[0] = 0.0f;
				data[3].colorVis[0] = 0.25f;
				data[4].colorVis[0] = -1.0f;
				data[5].colorVis[0] = 2.0f;
				LensPush push = {{5.0f, 5.0f}, LENS_MAX_CGSOURCES, 1.5f,
					{sign, 0.0008f, scenario == 6 ? 1.0f : 0.0f, 0.0f}};
				id<MTLCommandBuffer> command = [queue commandBuffer];
				id<MTLComputeCommandEncoder> encoder = [command computeCommandEncoder];
				CHECK(command != nil && encoder != nil);
				[encoder setComputePipelineState:pipeline];
				[encoder setBuffer:argBuffer offset:0u atIndex:0u];
				[encoder setBytes:&push length:sizeof(push) atIndex:1u];
				[encoder useResource:records usage:MTLResourceUsageRead | MTLResourceUsageWrite];
				[encoder useResource:depth usage:MTLResourceUsageRead];
				[encoder dispatchThreadgroups:MTLSizeMake(capacity / 64u, 1u, 1u) threadsPerThreadgroup:MTLSizeMake(64u, 1u, 1u)];
				[encoder endEncoding];
				[command commit];
				[command waitUntilCompleted];
				CHECK(command.status == MTLCommandBufferStatusCompleted);
				for (unsigned i = 0; i < capacity; ++i) {
					float expected = scenario == 1 || scenario == 4 ? 0.0f : (scenario == 5 ? 6.0f / 9.0f : 1.0f);
					if (i == 1 || i == 2 || i == 4) expected = 0.0f;
					if (i == 3) expected *= 0.25f;
					if (i >= LENS_MAX_CGSOURCES) expected = -7.0f;
					if (fabsf(data[i].colorVis[3] - expected) > 0.00001f)
						fprintf(stderr, "standard=%u scenario=%u source=%u actual=%g expected=%g\n", standard, scenario, i, data[i].colorVis[3], expected);
					CHECK(fabsf(data[i].colorVis[3] - expected) <= 0.00001f);
				}
				++cases;
			}
		}
		printf("canonical lens GPU: %u cases, %u sources, standard/reversed depth PASS\n", cases, LENS_MAX_CGSOURCES);
	}
	return 0;
}
