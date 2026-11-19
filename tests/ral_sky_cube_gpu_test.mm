// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <cmath>
#include <cstdio>
#include <vector>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL %d: %s\n", __LINE__, #x); return 1; } } while (0)
struct Pixel { float x, y, z, w; };

int main(int argc, const char **argv) {
    @autoreleasepool {
        CHECK(argc == 3);
        id<MTLDevice> device = MTLCreateSystemDefaultDevice();
        CHECK(device != nil);
        constexpr unsigned edge = 32, mipCount = 3;
        MTLTextureDescriptor *desc = [MTLTextureDescriptor
            texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA32Float
            width:edge height:edge mipmapped:YES];
        desc.textureType = MTLTextureType2DArray;
        desc.arrayLength = 6;
        desc.mipmapLevelCount = mipCount;
        desc.storageMode = MTLStorageModeShared;
        desc.usage = MTLTextureUsageShaderRead;
        id<MTLTexture> sky = [device newTextureWithDescriptor:desc];
        CHECK(sky != nil);
        std::vector<Pixel> directions, expected;
        for (unsigned mip = 0; mip < mipCount; ++mip) {
            const unsigned size = edge >> mip;
            for (unsigned face = 0; face < 6; ++face) {
                std::vector<Pixel> pixels(size * size);
                for (unsigned y = 0; y < size; ++y)
                for (unsigned x = 0; x < size; ++x) {
                    const float u = 2.0f * (x + 0.5f) / size - 1.0f;
                    const float v = 2.0f * (y + 0.5f) / size - 1.0f;
                    // Independent GL cube specification: deliberately do not
                    // include the production helper, so a shared bug cannot pass.
                    const Pixel faceDirections[] = {
                        {1, -v, -u, float(mip)}, {-1, -v, u, float(mip)},
                        {u, 1, v, float(mip)}, {u, -1, -v, float(mip)},
                        {u, -v, 1, float(mip)}, {-u, -v, -1, float(mip)}
                    };
                    const Pixel value = {float(face + 1), float(x + 1), float(y + 1), 1};
                    pixels[y * size + x] = value;
                    Pixel normalized = faceDirections[face];
                    const float length = std::sqrt(normalized.x * normalized.x
                        + normalized.y * normalized.y + normalized.z * normalized.z);
                    normalized.x /= length; normalized.y /= length; normalized.z /= length;
                    directions.push_back(normalized);
                    expected.push_back(value);
                    // Non-unit vectors must pick the same direction and texel.
                    Pixel scaled = normalized;
                    scaled.x *= 7; scaled.y *= 7; scaled.z *= 7;
                    directions.push_back(scaled);
                    expected.push_back(value);
                }
                [sky replaceRegion:MTLRegionMake2D(0, 0, size, size) mipmapLevel:mip
                    slice:face withBytes:pixels.data() bytesPerRow:size * sizeof(Pixel)
                    bytesPerImage:pixels.size() * sizeof(Pixel)];
            }
        }
        const NSUInteger bytes = directions.size() * sizeof(Pixel);
        id<MTLBuffer> input = [device newBufferWithBytes:directions.data()
            length:bytes options:MTLResourceStorageModeShared];
        id<MTLBuffer> output = [device newBufferWithLength:bytes options:MTLResourceStorageModeShared];
        MTLSamplerDescriptor *samplerDesc = [[MTLSamplerDescriptor alloc] init];
        samplerDesc.minFilter = samplerDesc.magFilter = MTLSamplerMinMagFilterNearest;
        samplerDesc.mipFilter = MTLSamplerMipFilterNearest;
        samplerDesc.sAddressMode = samplerDesc.tAddressMode = MTLSamplerAddressModeClampToEdge;
        id<MTLSamplerState> sampler = [device newSamplerStateWithDescriptor:samplerDesc];
        id<MTLCommandQueue> queue = [device newCommandQueue];
        CHECK(input && output && sampler && queue);
        unsigned failures = 0;
        for (int variant = 1; variant < argc; ++variant) {
            NSError *error = nil;
            id<MTLLibrary> library = [device newLibraryWithURL:
                [NSURL fileURLWithPath:[NSString stringWithUTF8String:argv[variant]]] error:&error];
            if (!library) fprintf(stderr, "%s\n", error.description.UTF8String);
            CHECK(library != nil);
            id<MTLFunction> function = [library newFunctionWithName:@"wired_sky_cube_probe"];
            CHECK(function != nil);
            id<MTLComputePipelineState> pipeline = [device newComputePipelineStateWithFunction:function error:&error];
            CHECK(pipeline != nil);
            id<MTLCommandBuffer> command = [queue commandBuffer];
            id<MTLComputeCommandEncoder> encoder = [command computeCommandEncoder];
            CHECK(command && encoder);
            [encoder setComputePipelineState:pipeline];
            [encoder setTexture:sky atIndex:0];
            [encoder setSamplerState:sampler atIndex:0];
            [encoder setBuffer:input offset:0 atIndex:0];
            [encoder setBuffer:output offset:0 atIndex:1];
            [encoder dispatchThreads:MTLSizeMake(directions.size(), 1, 1)
                threadsPerThreadgroup:MTLSizeMake(pipeline.threadExecutionWidth, 1, 1)];
            [encoder endEncoding];
            [command commit];
            [command waitUntilCompleted];
            CHECK(command.status == MTLCommandBufferStatusCompleted);
            const Pixel *actual = (const Pixel *)output.contents;
            unsigned mismatches = 0;
            for (size_t i = 0; i < expected.size(); ++i) {
                if (actual[i].x != expected[i].x || actual[i].y != expected[i].y
                        || actual[i].z != expected[i].z || actual[i].w != 1.0f) {
                    if (!mismatches) fprintf(stderr, "variant=%d direction=(%g,%g,%g) lod=%g actual=(%g,%g,%g) expected=(%g,%g,%g)\n",
                        variant, directions[i].x, directions[i].y, directions[i].z, directions[i].w,
                        actual[i].x, actual[i].y, actual[i].z, expected[i].x, expected[i].y, expected[i].z);
                    ++mismatches;
                }
            }
            printf("sky cube GPU variant=%d: %u/%zu mismatches (six asymmetric faces, three mips, unit/non-unit directions)\n",
                variant, mismatches, expected.size());
            failures += mismatches;
        }
        CHECK(failures == 0);
    }
    return 0;
}
