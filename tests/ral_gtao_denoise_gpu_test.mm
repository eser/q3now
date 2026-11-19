// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL %d: %s\n", __LINE__, #x); return 1; } } while (0)

struct DenoisePush { float width, height, nearZ, farZ; };
static_assert(sizeof(DenoisePush) == 16, "canonical denoise push ABI");

// Independent scalar oracle: full 5x5 bilateral filter, including replicated
// edges. Run the real translated shader at partial and full workgroup extents.
int main(int argc, const char **argv) {
 @autoreleasepool {
	CHECK(argc == 2);
	id<MTLDevice> device = MTLCreateSystemDefaultDevice();
	CHECK(device != nil);
	NSError *error = nil;
	NSString *source = [NSString stringWithContentsOfFile:[NSString stringWithUTF8String:argv[1]]
		encoding:NSUTF8StringEncoding error:&error];
	CHECK(source != nil);
	id<MTLLibrary> library = [device newLibraryWithSource:source options:nil error:&error];
	if (!library) fprintf(stderr, "%s\n", [[error description] UTF8String]);
	CHECK(library != nil);
	id<MTLFunction> function = [library newFunctionWithName:@"main0"];
	CHECK(function != nil);
	id<MTLComputePipelineState> pipeline = [device newComputePipelineStateWithFunction:function error:&error];
	CHECK(pipeline != nil);
	id<MTLCommandQueue> queue = [device newCommandQueue];
	CHECK(queue != nil);
	MTLSamplerDescriptor *sd = [[MTLSamplerDescriptor alloc] init];
	// Match vk_create_gtao_resources: production samples depth linearly.
	sd.minFilter = sd.magFilter = MTLSamplerMinMagFilterLinear;
	sd.sAddressMode = sd.tAddressMode = MTLSamplerAddressModeClampToEdge;
	sd.supportArgumentBuffers = YES;
	id<MTLSamplerState> sampler = [device newSamplerStateWithDescriptor:sd];
	CHECK(sampler != nil);
	unsigned checked = 0;
	double worst = 0.0;
	const int extents[][2] = {{1,1}, {7,5}, {8,8}, {9,9}, {17,13}, {63,35}};
	for (const auto &extent : extents) for (int scenario = 0; scenario < 4; ++scenario) {
		const int w = extent[0], h = extent[1];
		std::vector<_Float16> ao(w*h), output(w*h, (_Float16)NAN);
		std::vector<float> depth(w*h);
		for (int y = 0; y < h; ++y) for (int x = 0; x < w; ++x) {
			const int i = y*w+x;
			ao[i] = (_Float16)(((i*37 + scenario*11) % 257) / 256.0f);
			depth[i] = scenario == 0 ? 0.5f : scenario == 1 ? (x < w/2 ? 0.05f : 0.8f)
				: scenario == 2 ? ((i*31)%101)/100.0f : 0.0f;
		}
		auto texture = [&](MTLPixelFormat format) {
			MTLTextureDescriptor *desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:format
				width:w height:h mipmapped:NO];
			desc.storageMode = MTLStorageModeShared;
			desc.usage = MTLTextureUsageShaderRead | MTLTextureUsageShaderWrite;
			return [device newTextureWithDescriptor:desc];
		};
		id<MTLTexture> input = texture(MTLPixelFormatR16Float);
		id<MTLTexture> result = texture(MTLPixelFormatR16Float);
		id<MTLTexture> z = texture(MTLPixelFormatR32Float);
		CHECK(input && result && z);
		const MTLRegion region = MTLRegionMake2D(0,0,w,h);
		[input replaceRegion:region mipmapLevel:0 withBytes:ao.data() bytesPerRow:w*sizeof(_Float16)];
		[result replaceRegion:region mipmapLevel:0 withBytes:output.data() bytesPerRow:w*sizeof(_Float16)];
		[z replaceRegion:region mipmapLevel:0 withBytes:depth.data() bytesPerRow:w*sizeof(float)];
		id<MTLArgumentEncoder> args = [function newArgumentEncoderWithBufferIndex:0];
		CHECK(args != nil);
		id<MTLBuffer> bindings = [device newBufferWithLength:args.encodedLength options:MTLResourceStorageModeShared];
		CHECK(bindings != nil);
		[args setArgumentBuffer:bindings offset:0];
		[args setTexture:z atIndex:0];
		[args setSamplerState:sampler atIndex:1];
		[args setTexture:input atIndex:2];
		[args setTexture:result atIndex:3];
		DenoisePush push = {(float)w, (float)h, 4.0f, 4096.0f};
		id<MTLCommandBuffer> command = [queue commandBuffer];
		id<MTLComputeCommandEncoder> encoder = [command computeCommandEncoder];
		CHECK(command && encoder);
		[encoder setComputePipelineState:pipeline];
		[encoder setBuffer:bindings offset:0 atIndex:0];
		[encoder setBytes:&push length:sizeof(push) atIndex:1];
		[encoder useResource:input usage:MTLResourceUsageRead];
		[encoder useResource:z usage:MTLResourceUsageRead];
		[encoder useResource:result usage:MTLResourceUsageWrite];
		[encoder dispatchThreadgroups:MTLSizeMake((w+7)/8, (h+7)/8, 1)
			threadsPerThreadgroup:MTLSizeMake(8,8,1)];
		[encoder endEncoding];
		[command commit];
		[command waitUntilCompleted];
		CHECK(command.status == MTLCommandBufferStatusCompleted);
		[result getBytes:output.data() bytesPerRow:w*sizeof(_Float16) fromRegion:region mipmapLevel:0];
		auto linear = [&](int i) { return 4.0*4096.0/(4.0 + depth[i]*(4096.0-4.0)); };
		for (int y = 0; y < h; ++y) for (int x = 0; x < w; ++x) {
			const double center = linear(y*w+x), scale = 1.0/std::max(center*0.05, 1e-3);
			double sum = 0.0, weightSum = 0.0;
			for (int dy = -2; dy <= 2; ++dy) for (int dx = -2; dx <= 2; ++dx) {
				const int q = std::clamp(y+dy,0,h-1)*w + std::clamp(x+dx,0,w-1);
				const double delta = std::abs(linear(q)-center)*scale;
				const double weight = std::exp(-(dx*dx+dy*dy)*0.5)*std::exp(-delta*delta);
				sum += (float)ao[q]*weight;
				weightSum += weight;
			}
			const double actual = (float)output[y*w+x], expected = sum/weightSum;
			CHECK(std::isfinite(actual));
			worst = std::max(worst, std::abs(actual-expected));
			// R16F quantization over [0,1] plus shader arithmetic rounding.
			if (std::abs(actual-expected) > 0.0006)
				fprintf(stderr, "%dx%d case%d (%d,%d) actual=%g expected=%g\n",w,h,scenario,x,y,actual,expected);
			CHECK(std::abs(actual-expected) <= 0.0006);
			++checked;
		}
	}
	printf("canonical GTAO denoise GPU: 24 cases, %u pixels, worst error %.8f PASS\n", checked, worst);
 }
	return 0;
}
