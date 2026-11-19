// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
#include "ral_metal_exposure.h"
#include "ral_metal_hdr_histogram_metallib.h"
#include "ral_metal_exposure_metallib.h"
#import <Foundation/Foundation.h>
#include <stdlib.h>

static_assert(sizeof(ralExposureBlock_t) == 64u, "exposure shader ABI");

struct ralMetalExposure_s {
	id<MTLDevice> device;
	id<MTLComputePipelineState> histogramPipeline, reducePipeline;
	id<MTLArgumentEncoder> histogramArgs, reduceArgs;
	id<MTLBuffer> histogram, accumulator, seed;
	id<MTLSamplerState> sampler;
};

void RalMetal_ExposureDestroy( ralMetalExposure_t *s ) {
	if (!s) return;
	[s->histogramPipeline release]; [s->reducePipeline release];
	[s->histogramArgs release]; [s->reduceArgs release];
	[s->histogram release]; [s->accumulator release]; [s->seed release];
	[s->sampler release]; [s->device release]; free(s);
}

static bool LoadPipeline(id<MTLDevice> device, const unsigned char *bytes, size_t size,
	id<MTLComputePipelineState> *pipeline, id<MTLArgumentEncoder> *args) {
	dispatch_data_t data = dispatch_data_create(bytes, size,
		dispatch_get_global_queue(QOS_CLASS_DEFAULT, 0), ^{});
	if (!data) return false;
	NSError *error = nil;
	id<MTLLibrary> library = [device newLibraryWithData:data error:&error];
	dispatch_release(data);
	id<MTLFunction> function = [library newFunctionWithName:@"main0"];
	if (function) {
		*pipeline = [device newComputePipelineStateWithFunction:function error:&error];
		*args = [function newArgumentEncoderWithBufferIndex:0u];
	}
	[function release]; [library release];
	return *pipeline && *args && !error;
}

ralMetalExposure_t *RalMetal_ExposureCreate(id<MTLDevice> device) {
	if (!device) return NULL;
	ralMetalExposure_t *s = (ralMetalExposure_t *)calloc(1u, sizeof(*s));
	if (!s) return NULL;
	s->device = [device retain];
	const float seed = 1.0f;
	s->histogram = [device newBufferWithLength:256u * sizeof(uint32_t)
		options:MTLResourceStorageModePrivate];
	s->accumulator = [device newBufferWithLength:sizeof(float)
		options:MTLResourceStorageModePrivate];
	s->seed = [device newBufferWithBytes:&seed length:sizeof(seed)
		options:MTLResourceStorageModeShared];
	MTLSamplerDescriptor *desc = [[[MTLSamplerDescriptor alloc] init] autorelease];
	desc.supportArgumentBuffers = YES;
	s->sampler = [device newSamplerStateWithDescriptor:desc];
	if (!s->histogram || !s->accumulator || !s->seed || !s->sampler
		|| !LoadPipeline(device, ral_metal_hdr_histogram_metallib,
			ral_metal_hdr_histogram_metallib_size, &s->histogramPipeline, &s->histogramArgs)
		|| !LoadPipeline(device, ral_metal_exposure_metallib,
			ral_metal_exposure_metallib_size, &s->reducePipeline, &s->reduceArgs)) {
		RalMetal_ExposureDestroy(s); return NULL;
	}
	return s;
}

bool RalMetal_ExposureEncode(ralMetalExposure_t *s, id<MTLCommandBuffer> command,
	id<MTLTexture> scene, const ralExposureSettings_t *settings, float dt, bool reset,
	id<MTLBuffer> *outExposure) {
	if (!s || !command || !scene || !outExposure || command.device != s->device
		|| scene.device != s->device || !scene.width || !scene.height
		|| scene.width > UINT32_MAX || scene.height > UINT32_MAX
		|| !Ral_ExposureSettingsValid(settings) || !settings->enabled
		|| !isfinite(dt) || dt < 0.0f || dt > 0.1f) return false;
	ralExposureBlock_t params = {};
	params.exposure_bias = 1.0f;
	params.key = settings->key; params.pctLow = settings->pctLow;
	params.pctHigh = settings->pctHigh; params.rateUp = settings->rateUp;
	params.rateDown = settings->rateDown; params.minExp = settings->minExp;
	params.maxExp = settings->maxExp; params.autoEnabled = 1;
	/* Adaptation only; the common display visibility transform applies the
	 * user's brightness exactly once after this multiplier. */
	params.brightness = 1.0f;
	id<MTLBuffer> output = [s->device newBufferWithBytes:&params length:sizeof(params)
		options:MTLResourceStorageModeShared];
	id<MTLBuffer> hArgs = [s->device newBufferWithLength:s->histogramArgs.encodedLength
		options:MTLResourceStorageModeShared];
	id<MTLBuffer> rArgs = [s->device newBufferWithLength:s->reduceArgs.encodedLength
		options:MTLResourceStorageModeShared];
	bool ok = false;
	do {
		if (!output || !hArgs || !rArgs) break;
		[s->histogramArgs setArgumentBuffer:hArgs offset:0u];
		[s->histogramArgs setTexture:scene atIndex:0u];
		[s->histogramArgs setSamplerState:s->sampler atIndex:1u];
		[s->histogramArgs setBuffer:s->histogram offset:0u atIndex:2u];
		[s->reduceArgs setArgumentBuffer:rArgs offset:0u];
		[s->reduceArgs setBuffer:s->histogram offset:0u atIndex:0u];
		[s->reduceArgs setBuffer:s->accumulator offset:0u atIndex:1u];
		[s->reduceArgs setBuffer:output offset:0u atIndex:2u];
		id<MTLBlitCommandEncoder> blit = [command blitCommandEncoder];
		if (!blit) break;
		[blit fillBuffer:s->histogram range:NSMakeRange(0u, 1024u) value:0u];
		if (reset) [blit copyFromBuffer:s->seed sourceOffset:0u
			toBuffer:s->accumulator destinationOffset:0u size:sizeof(float)];
		[blit endEncoding];
		id<MTLComputeCommandEncoder> encoder = [command computeCommandEncoder];
		if (!encoder) break;
		const uint32_t dims[2] = {(uint32_t)scene.width, (uint32_t)scene.height};
		[encoder setComputePipelineState:s->histogramPipeline];
		[encoder setBuffer:hArgs offset:0u atIndex:0u];
		[encoder setBytes:dims length:sizeof(dims) atIndex:1u];
		[encoder useResource:scene usage:MTLResourceUsageRead];
		[encoder useResource:s->histogram usage:MTLResourceUsageRead | MTLResourceUsageWrite];
		[encoder dispatchThreadgroups:MTLSizeMake((dims[0]+15u)/16u, (dims[1]+15u)/16u, 1u)
			threadsPerThreadgroup:MTLSizeMake(16u,16u,1u)];
		[encoder endEncoding];
		encoder = [command computeCommandEncoder];
		if (!encoder) break;
		[encoder setComputePipelineState:s->reducePipeline];
		[encoder setBuffer:rArgs offset:0u atIndex:0u];
		[encoder setBytes:&dt length:sizeof(dt) atIndex:1u];
		[encoder useResource:s->histogram usage:MTLResourceUsageRead];
		[encoder useResource:s->accumulator usage:MTLResourceUsageRead | MTLResourceUsageWrite];
		[encoder useResource:output usage:MTLResourceUsageRead | MTLResourceUsageWrite];
		[encoder dispatchThreadgroups:MTLSizeMake(1u,1u,1u)
			threadsPerThreadgroup:MTLSizeMake(1u,1u,1u)];
		[encoder endEncoding];
		ok = true;
	} while (false);
	[hArgs release]; [rArgs release];
	if (!ok) { [output release]; return false; }
	*outExposure = output;
	return true;
}
