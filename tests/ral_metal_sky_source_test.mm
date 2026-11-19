// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include "../code/render/ral/core/ral_sky_radiance.h"

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL %d: %s\n", __LINE__, #x); return 1; } } while (0)

// This test executes the generated shared shader, not a CPU reimplementation.
// One texel at +Z faces the authored sun exactly; no map assets or window.
typedef ralSkyRadianceSourceParams_t skyPush_t;
static_assert(sizeof(skyPush_t) == 64u, "shared source sky push ABI");

int main(int argc, const char **argv) {
	@autoreleasepool {
		CHECK(argc == 2);
		id<MTLDevice> device = MTLCreateSystemDefaultDevice();
		CHECK(device != nil);
		NSError *error = nil;
		NSString *source = [NSString stringWithContentsOfFile:
			[NSString stringWithUTF8String:argv[1]]
			encoding:NSUTF8StringEncoding error:&error];
		CHECK(source != nil);
		id<MTLLibrary> library = [device newLibraryWithSource:source options:nil error:&error];
		if (!library) fprintf(stderr, "%s\n", [[error description] UTF8String]);
		CHECK(library != nil);
		id<MTLFunction> function = [library newFunctionWithName:@"main0"];
		CHECK(function != nil);
		id<MTLComputePipelineState> pipeline = [device newComputePipelineStateWithFunction:
			function error:&error];
		CHECK(pipeline != nil);
		id<MTLArgumentEncoder> args = [function newArgumentEncoderWithBufferIndex:0u];
		CHECK(args != nil);
		id<MTLBuffer> argBuffer = [device newBufferWithLength:args.encodedLength
			options:MTLResourceStorageModeShared];
		CHECK(argBuffer != nil);
		[args setArgumentBuffer:argBuffer offset:0u];
		MTLTextureDescriptor *inputDesc = [MTLTextureDescriptor
			texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA32Float
			width:1u height:1u mipmapped:NO];
		inputDesc.storageMode = MTLStorageModeShared;
		inputDesc.usage = MTLTextureUsageShaderRead;
		id<MTLTexture> input = [device newTextureWithDescriptor:inputDesc];
		CHECK(input != nil);
		const float authored[4] = { 0.25f, 0.5f, 0.75f, 1.0f };
		[input replaceRegion:MTLRegionMake2D(0u, 0u, 1u, 1u)
			mipmapLevel:0u withBytes:authored bytesPerRow:sizeof(authored)];
		MTLTextureDescriptor *outputDesc = [MTLTextureDescriptor
			texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA16Float
			width:1u height:1u mipmapped:NO];
		outputDesc.textureType = MTLTextureType2DArray;
		outputDesc.arrayLength = 6u;
		outputDesc.storageMode = MTLStorageModeShared;
		outputDesc.usage = MTLTextureUsageShaderWrite;
		id<MTLTexture> output = [device newTextureWithDescriptor:outputDesc];
		CHECK(output != nil);
		MTLSamplerDescriptor *samplerDesc = [[MTLSamplerDescriptor alloc] init];
		samplerDesc.supportArgumentBuffers = YES;
		id<MTLSamplerState> sampler = [device newSamplerStateWithDescriptor:samplerDesc];
		CHECK(sampler != nil);
		for (NSUInteger slot = 0u; slot < 7u; ++slot)
			if (slot != 1u) [args setTexture:input atIndex:slot];
		[args setSamplerState:sampler atIndex:1u];
		[args setTexture:output atIndex:7u];
		id<MTLCommandQueue> queue = [device newCommandQueue];
		CHECK(queue != nil);
		const float exposures[] = { 0.0f, 0.5f, 1.0f, 2.0f };
		const float coverages[] = { 0.0f, 0.5f, 1.0f };
		for (uint32_t cube = 0u; cube < 3u; ++cube) {
			for (float exposure : exposures)
			for (float coverage : coverages) {
				ralSkyEnvironmentPlanRequest_t request = {};
				request.schemaVersion = RAL_SKY_ENVIRONMENT_SCHEMA_VERSION;
				request.backendType = RAL_BACKEND_METAL;
				request.frameGeneration = request.sourceRevision = 1u;
				request.requestedTier = RAL_ATMOSPHERE_TIER_FULL;
				request.mode = RAL_SKY_ENVIRONMENT_PHYSICAL;
				request.capabilities.physicalLuts = qtrue;
				request.capabilities.radianceCapture = qtrue;
				request.capabilities.convolution = qtrue;
				request.sunDirection[2] = 1.0f;
				for (unsigned c = 0u; c < 3u; ++c) {
					request.ambientSh[c] = authored[c];
					request.sunRadiance[c] = 2.0f;
				}
				request.cloudSunOcclusion = 1.0f;
				request.exposure = exposure;
				ralSkyEnvironmentPlanReceipt_t plan;
				skyPush_t p;
				CHECK(Ral_SkyEnvironmentPlan(&request, &plan));
				CHECK(Ral_SkyRadianceSourceParamsBuild(&plan, coverage, (ralSkyRadianceSourceType_t)cube, &p));
				p.meta[0] = 4u;
				id<MTLCommandBuffer> command = [queue commandBuffer];
				id<MTLComputeCommandEncoder> encoder = [command computeCommandEncoder];
				CHECK(command != nil && encoder != nil);
				[encoder setComputePipelineState:pipeline];
				[encoder setBuffer:argBuffer offset:0u atIndex:0u];
				[encoder setBytes:&p length:sizeof(p) atIndex:1u];
				[encoder useResource:input usage:MTLResourceUsageRead];
				[encoder useResource:output usage:MTLResourceUsageWrite];
				[encoder dispatchThreadgroups:MTLSizeMake(1u, 1u, 1u)
					threadsPerThreadgroup:MTLSizeMake(8u, 8u, 1u)];
				[encoder endEncoding];
				[command commit];
				[command waitUntilCompleted];
				CHECK(command.status == MTLCommandBufferStatusCompleted);
				_Float16 rgba[4];
				[output getBytes:rgba bytesPerRow:sizeof(rgba) bytesPerImage:sizeof(rgba)
					fromRegion:MTLRegionMake2D(0u, 0u, 1u, 1u) mipmapLevel:0u slice:4u];
				for (unsigned c = 0u; c < 3u; ++c) {
					const float expected = cube == RAL_SKY_SOURCE_COMPOSITED_RADIANCE ? authored[c]
						: (authored[c] * (cube ? 1.0f : 0.55f)
						+ 2.0f * (1.0f - coverage)) * exposure;
					if (fabsf((float)rgba[c] - expected) > 0.01f)
						fprintf(stderr, "cube=%u exposure=%g channel=%u actual=%g expected=%g\n",
							cube, exposure, c, (float)rgba[c], expected);
					CHECK(fabsf((float)rgba[c] - expected) <= 0.01f);
				}
				CHECK((float)rgba[3] == 1.0f);
			}
		}
	}
	puts("ral_metal_sky_source_test: cloud/exposure and composited radiance identity, 36 GPU cases passed");
	return 0;
}
