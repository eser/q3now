// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <cmath>
#include <cstdio>
#include "../code/render/frontend/render_material_stage_gpu.h"

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL %d: %s\n", __LINE__, #x); return 1; } } while (0)
struct StageCase {
    renderMaterialStageGpuParams_t stage;
    float uvTime[4], sourceColor[4], destinationColor[4], vertexColor[4];
    float entityColor[4], position[4], diffuse[4];
};
static_assert(sizeof(StageCase) == 560, "std140 probe input layout");

static StageCase MakeCase(unsigned index) {
    renderMaterialStageSnapshot_t s = {};
    StageCase c = {};
    s.sourceBlend = index % 11;
    s.destinationBlend = (index / 11) % 11;
    s.rgbGen = index % 12;
    s.alphaGen = (index / 12) % 10;
    s.rgbWave.function = (ralMaterialWaveFunction_t)(1 + index % 6);
    s.rgbWave.base = 0.5f; s.rgbWave.amplitude = 0.4f;
    s.rgbWave.phase = -0.125f; s.rgbWave.frequency = 0.375f;
    s.alphaWave.function = (ralMaterialWaveFunction_t)(1 + (index / 6) % 6);
    s.alphaWave.base = 0.5f; s.alphaWave.amplitude = 0.3f;
    s.alphaWave.phase = 0.125f; s.alphaWave.frequency = 0.75f;
    for (unsigned ch = 0; ch < 4; ++ch) {
        s.constantColor[ch] = 0.15f * (ch + 1);
        c.sourceColor[ch] = 0.2f * (ch + 1);
        c.destinationColor[ch] = 0.1f * (4 - ch);
        c.vertexColor[ch] = 0.17f * (ch + 1);
        c.entityColor[ch] = 0.21f * (ch + 1);
    }
    s.scaleScroll[0] = 1.2f; s.scaleScroll[1] = 0.7f;
    s.scaleScroll[2] = -0.2f; s.scaleScroll[3] = 0.13f;
    s.rotateDegrees = 13;
    s.hasStretch = qtrue; s.stretch[0] = 1.5f; s.stretch[1] = 0.3f;
    s.stretch[2] = 0.2f; s.stretch[3] = 0.6f;
    s.hasTurbulence = qtrue; s.turbulence[1] = 0.1f; s.turbulence[3] = 0.4f;
    s.tcModCount = index % 9;
    for (unsigned op = 0; op < s.tcModCount; ++op) {
        s.tcMods[op].type = 1 + (index + op) % 6;
        float *p = s.tcMods[op].parameters;
        switch (s.tcMods[op].type) {
        case 1: p[0] = 0.75f; p[1] = 1.25f; break;
        case 2: p[0] = -0.17f; p[1] = 0.23f; break;
        case 3: p[0] = -17.0f; break;
        case 4: p[0] = 9; p[1] = 0.2f; p[2] = -0.4f; p[3] = 0.33f; break;
        case 5: p[0] = float(1 + index % 6); p[1] = index & 1 ? -1.5f : 1.5f;
            p[2] = 0.25f; p[3] = -0.25f; p[4] = 0.4f; break;
        case 6: p[0] = 1.1f; p[1] = -0.2f; p[2] = 0.3f; p[3] = 0.9f;
            p[4] = 0.1f; p[5] = -0.15f; break;
        }
    }
    wired_material_stage_pack(&s, &c.stage);
    const float times[] = {-256.125f, -4.0f, -1.0f, -0.001f, 0, 0.125f, 1, 4, 255.5f};
    c.uvTime[0] = float(index % 17) / 16; c.uvTime[1] = float(index % 13) / 12;
    c.uvTime[2] = times[(index / 9) % 9]; c.uvTime[3] = 0.35f;
    c.position[0] = 17 + float(index % 127);
    c.position[1] = -9 - float(index % 97); c.position[2] = 31 + float(index % 53);
    c.diffuse[0] = 0.3f; c.diffuse[1] = 0.5f; c.diffuse[2] = 0.9f;
    c.diffuse[3] = 128 + float(index % 400);
    return c;
}

int main(int argc, const char **argv) {
    @autoreleasepool {
        CHECK(argc == 3);
        id<MTLDevice> device = MTLCreateSystemDefaultDevice();
        CHECK(device);
        id<MTLCommandQueue> queue = [device newCommandQueue];
        CHECK(queue);
        id<MTLComputePipelineState> pipelines[2];
        id<MTLArgumentEncoder> arguments[2];
        id<MTLBuffer> argumentBuffers[2];
        id<MTLBuffer> input = [device newBufferWithLength:64 * sizeof(StageCase) options:MTLResourceStorageModeShared];
        id<MTLBuffer> outputs[2];
        CHECK(input);
        for (unsigned v = 0; v < 2; ++v) {
            NSError *error = nil;
            id<MTLLibrary> library = [device newLibraryWithURL:
                [NSURL fileURLWithPath:[NSString stringWithUTF8String:argv[v + 1]]] error:&error];
            if (!library) fprintf(stderr, "%s\n", error.description.UTF8String);
            CHECK(library);
            id<MTLFunction> function = [library newFunctionWithName:@"main0"];
            CHECK(function);
            pipelines[v] = [device newComputePipelineStateWithFunction:function error:&error];
            CHECK(pipelines[v]);
            arguments[v] = [function newArgumentEncoderWithBufferIndex:0];
            CHECK(arguments[v]);
            argumentBuffers[v] = [device newBufferWithLength:arguments[v].encodedLength options:MTLResourceStorageModeShared];
            outputs[v] = [device newBufferWithLength:64 * 16 * sizeof(float) options:MTLResourceStorageModeShared];
            CHECK(argumentBuffers[v] && outputs[v]);
            [arguments[v] setArgumentBuffer:argumentBuffers[v] offset:0];
            [arguments[v] setBuffer:input offset:0 atIndex:0];
            [arguments[v] setBuffer:outputs[v] offset:0 atIndex:1];
        }
        float maxRelativeError = 0;
        for (unsigned batch = 0; batch < 32; ++batch) {
            StageCase *cases = (StageCase *)input.contents;
            for (unsigned i = 0; i < 64; ++i) cases[i] = MakeCase(batch * 64 + i);
            // An unwritten/stale output must not pass because both paths agree.
            for (unsigned v = 0; v < 2; ++v) {
                float *values = (float *)outputs[v].contents;
                for (unsigned i = 0; i < 64 * 16; ++i) values[i] = NAN;
            }
            id<MTLCommandBuffer> command = [queue commandBuffer];
            CHECK(command);
            for (unsigned v = 0; v < 2; ++v) {
                id<MTLComputeCommandEncoder> encoder = [command computeCommandEncoder];
                CHECK(encoder);
                [encoder setComputePipelineState:pipelines[v]];
                [encoder setBuffer:argumentBuffers[v] offset:0 atIndex:0];
                [encoder useResource:input usage:MTLResourceUsageRead];
                [encoder useResource:outputs[v] usage:MTLResourceUsageWrite];
                [encoder dispatchThreadgroups:MTLSizeMake(1, 1, 1) threadsPerThreadgroup:MTLSizeMake(64, 1, 1)];
                [encoder endEncoding];
            }
            [command commit]; [command waitUntilCompleted];
            CHECK(command.status == MTLCommandBufferStatusCompleted);
            const float *a = (const float *)outputs[0].contents, *b = (const float *)outputs[1].contents;
            for (unsigned i = 0; i < 64 * 16; ++i) {
                CHECK(std::isfinite(a[i]) && std::isfinite(b[i]));
                const float error = std::fabs(a[i] - b[i]) / std::fmax(1.0f, std::fabs(a[i]));
                maxRelativeError = std::fmax(maxRelativeError, error);
                if (error > 1e-4f) fprintf(stderr, "case=%u component=%u native=%g portable=%g error=%g\n",
                    batch * 64 + i / 16, i % 16, a[i], b[i], error);
                CHECK(error <= 1e-4f);
            }
        }
        printf("shared material stage GPU: 2048 cases / 32768 components, max relative error %g\n", maxRelativeError);
    }
    return 0;
}
