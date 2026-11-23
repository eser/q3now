// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
#include "ral_metal_taa.h"
#include "ral_metal_temporal_resolve_comp_metallib.h"
#include "ral_metal_temporal_bootstrap_aa_comp_metallib.h"
#include <stdlib.h>
#include <string.h>

/* Same constants as the Vulkan resolve (vk_temporal_resolve.c). */
#define WIRED_TAA_DEPTH_ABSOLUTE 0.125f
#define WIRED_TAA_DEPTH_RELATIVE 0.02f
#define WIRED_TAA_HISTORY_WEIGHT 0.875f
#define WIRED_TAA_QUALITY_FLAGS 7u
/* The shared linearization is zNear * zFar / (zNear + z * (zFar - zNear)). Metal's forward distance is near / reverseDepth, which that
 * form reproduces with zNear = near and a very large zFar (the denominator is then ~ z * zFar). */
#define WIRED_TAA_FAR_SCALE 1.0e6f

typedef struct {
	uint32_t extent[2];
	float currentJitterUv[2], previousJitterUv[2];
	float zNear, zFar, depthAbsolute, depthRelative, historyWeight;
	uint32_t historyReadIndex, qualityFlags, pad;
} wiredTaaResolvePush;
typedef struct { uint32_t extent[2]; float zNear, zFar; } wiredTaaBootstrapPush;
typedef struct { float current[20], previous[20], jitterNdc[2]; uint32_t extent[2]; } wiredTaaMotionParams;

struct wiredTaaCohort {
	id<MTLTexture> scene;
	uint32_t width, height;
	id<MTLTexture> velocity, validity, reverseDepth, output, historyColor[2], historyDepth[2];
	id<MTLBuffer> resolveArgs[2];     /* indexed by the history read slot */
	id<MTLBuffer> bootstrapArgs[2];   /* indexed by the history write slot */
};
struct ralMetalTaa_s {
	id<MTLDevice> device;
	id<MTLLibrary> resolveLibrary, bootstrapLibrary;
	id<MTLComputePipelineState> motion, resolve, bootstrap;
	id<MTLSamplerState> nearest;
	wiredTaaCohort *cohort;
};

static void wired_taa_release(wiredTaaCohort *c) {
	if (!c) return;
	[c->scene release]; [c->velocity release]; [c->validity release]; [c->reverseDepth release]; [c->output release];
	for (int i = 0; i < 2; ++i) {
		[c->historyColor[i] release]; [c->historyDepth[i] release]; [c->resolveArgs[i] release]; [c->bootstrapArgs[i] release];
	}
	free(c);
}
void RalMetal_TaaDestroy(ralMetalTaa_t *s) {
	if (!s) return;
	wired_taa_release(s->cohort);
	[s->resolveLibrary release]; [s->bootstrapLibrary release];
	[s->motion release]; [s->resolve release]; [s->bootstrap release]; [s->nearest release]; [s->device release];
	free(s);
}
static id<MTLLibrary> wired_taa_library(id<MTLDevice> device, const unsigned char *bytes, size_t size) {
	dispatch_data_t data = dispatch_data_create(bytes, size, dispatch_get_global_queue(QOS_CLASS_DEFAULT, 0), ^{});
	if (!data) return nil;
	NSError *error = nil;
	id<MTLLibrary> library = [device newLibraryWithData:data error:&error];
	dispatch_release(data);
	if (error) { [library release]; return nil; }
	return library;
}
static id<MTLComputePipelineState> wired_taa_compute(id<MTLDevice> device, id<MTLLibrary> library, NSString *name) {
	id<MTLFunction> function = [library newFunctionWithName:name];
	if (!function) return nil;
	NSError *error = nil;
	id<MTLComputePipelineState> pipeline = [device newComputePipelineStateWithFunction:function error:&error];
	[function release];
	if (error) { [pipeline release]; return nil; }
	return pipeline;
}
ralMetalTaa_t *RalMetal_TaaCreate(id<MTLDevice> device, id<MTLLibrary> motionLibrary) {
	if (!device || !motionLibrary) return NULL;
	ralMetalTaa_t *s = (ralMetalTaa_t *)calloc(1, sizeof(*s));
	if (!s) return NULL;
	s->device = [device retain];
	s->resolveLibrary = wired_taa_library(device, ral_metal_temporal_resolve_comp_metallib,
		ral_metal_temporal_resolve_comp_metallib_size);
	s->bootstrapLibrary = wired_taa_library(device, ral_metal_temporal_bootstrap_aa_comp_metallib,
		ral_metal_temporal_bootstrap_aa_comp_metallib_size);
	if (!s->resolveLibrary || !s->bootstrapLibrary) { RalMetal_TaaDestroy(s); return NULL; }
	s->motion = wired_taa_compute(device, motionLibrary, @"wired_taa_motion_compute");
	s->resolve = wired_taa_compute(device, s->resolveLibrary, @"main0");
	s->bootstrap = wired_taa_compute(device, s->bootstrapLibrary, @"main0");
	MTLSamplerDescriptor *sd = [[[MTLSamplerDescriptor alloc] init] autorelease];
	sd.minFilter = sd.magFilter = MTLSamplerMinMagFilterNearest;
	sd.sAddressMode = sd.tAddressMode = MTLSamplerAddressModeClampToEdge;
	sd.supportArgumentBuffers = YES;
	s->nearest = [device newSamplerStateWithDescriptor:sd];
	if (!s->motion || !s->resolve || !s->bootstrap || !s->nearest) { RalMetal_TaaDestroy(s); return NULL; }
	return s;
}
static id<MTLTexture> wired_taa_texture(ralMetalTaa_t *s, MTLPixelFormat format, NSUInteger w, NSUInteger h) {
	MTLTextureDescriptor *td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:format width:w height:h mipmapped:NO];
	td.storageMode = MTLStorageModePrivate;
	td.usage = MTLTextureUsageShaderRead | MTLTextureUsageShaderWrite;
	return [s->device newTextureWithDescriptor:td];
}
static id<MTLBuffer> wired_taa_args(ralMetalTaa_t *s, id<MTLComputePipelineState> pipeline, id<MTLLibrary> library,
		NSArray<id> *entries /* texture or sampler objects in id() order */) {
	id<MTLFunction> fn = [library newFunctionWithName:@"main0"];
	id<MTLArgumentEncoder> ae = fn ? [fn newArgumentEncoderWithBufferIndex:0] : nil;
	[fn release]; (void)pipeline;
	if (!ae) return nil;
	id<MTLBuffer> buffer = [s->device newBufferWithLength:ae.encodedLength options:MTLResourceStorageModeShared];
	if (buffer) {
		[ae setArgumentBuffer:buffer offset:0];
		for (NSUInteger i = 0; i < entries.count; ++i) {
			id e = entries[i];
			if ([e conformsToProtocol:@protocol(MTLTexture)]) [ae setTexture:e atIndex:i];
			else [ae setSamplerState:e atIndex:i];
		}
	}
	[ae release];
	return buffer;
}
static bool wired_taa_prepare(ralMetalTaa_t *s, id<MTLTexture> scene) {
	wiredTaaCohort *old = s->cohort;
	if (old && old->scene == scene && old->width == scene.width && old->height == scene.height) return true;
	wiredTaaCohort *c = (wiredTaaCohort *)calloc(1, sizeof(*c));
	if (!c) return false;
	c->scene = [scene retain]; c->width = (uint32_t)scene.width; c->height = (uint32_t)scene.height;
	const NSUInteger w = scene.width, h = scene.height;
	bool ok = false;
	do {
		c->velocity = wired_taa_texture(s, MTLPixelFormatRG16Float, w, h);
		c->validity = wired_taa_texture(s, MTLPixelFormatR8Unorm, w, h);
		c->reverseDepth = wired_taa_texture(s, MTLPixelFormatR32Float, w, h);
		c->output = wired_taa_texture(s, MTLPixelFormatRGBA16Float, w, h);
		for (int i = 0; i < 2; ++i) {
			c->historyColor[i] = wired_taa_texture(s, MTLPixelFormatRGBA16Float, w, h);
			c->historyDepth[i] = wired_taa_texture(s, MTLPixelFormatR32Float, w, h);
		}
		if (!c->velocity || !c->validity || !c->reverseDepth || !c->output || !c->historyColor[0] || !c->historyColor[1]
				|| !c->historyDepth[0] || !c->historyDepth[1]) break;
		bool argsOk = true;
		for (int r = 0; r < 2 && argsOk; ++r) {
			const int wr = 1 - r;
			/* resolve ids: 0 current, 1 sampler, 2 previous colour, 3 previous depth, 4 velocity, 5 validity, 6 reverse depth,
			 * 7 resolved, 8 history colour out, 9 history depth out */
			c->resolveArgs[r] = wired_taa_args(s, s->resolve, s->resolveLibrary, @[ scene, s->nearest, c->historyColor[r],
				c->historyDepth[r], c->velocity, c->validity, c->reverseDepth, c->output, c->historyColor[wr], c->historyDepth[wr] ]);
			/* bootstrap ids: 0 current, 1 sampler, 2 bootstrap colour out, 3 history colour out, 4 history depth out, 5 reverse depth;
			 * indexed by the write slot */
			c->bootstrapArgs[r] = wired_taa_args(s, s->bootstrap, s->bootstrapLibrary, @[ scene, s->nearest, c->output,
				c->historyColor[r], c->historyDepth[r], c->reverseDepth ]);
			if (!c->resolveArgs[r] || !c->bootstrapArgs[r]) argsOk = false;
		}
		if (!argsOk) break;
		ok = true;
	} while (false);
	if (!ok) { wired_taa_release(c); return false; }
	s->cohort = c; wired_taa_release(old);
	return true;
}
static void wired_taa_dispatch(id<MTLComputeCommandEncoder> e, NSUInteger w, NSUInteger h) {
	[e dispatchThreadgroups:MTLSizeMake((w + 7u) / 8u, (h + 7u) / 8u, 1) threadsPerThreadgroup:MTLSizeMake(8, 8, 1)];
}
bool RalMetal_TaaEncode(ralMetalTaa_t *s, id<MTLCommandBuffer> command, id<MTLTexture> scene, id<MTLTexture> depth,
		const ralMetalTaaFrame_t *frame) {
	if (!s || !command || !scene || !depth || !frame || command.device != s->device || scene.device != s->device
		|| scene.pixelFormat != MTLPixelFormatRGBA16Float || depth.pixelFormat != MTLPixelFormatDepth32Float
		|| scene.sampleCount != 1u || depth.width != scene.width || depth.height != scene.height
		|| scene.width != frame->width || scene.height != frame->height || frame->readIndex > 1u || frame->writeIndex > 1u) return false;
	if (!wired_taa_prepare(s, scene)) return false;
	wiredTaaCohort *c = s->cohort;
	const float zNear = frame->currentView[16 + 2];   /* projection.z */
	{
		wiredTaaMotionParams mp;
		memset(&mp, 0, sizeof(mp));
		memcpy(mp.current, frame->currentView, sizeof(mp.current));
		memcpy(mp.previous, frame->historyValid ? frame->previousView : frame->currentView, sizeof(mp.previous));
		mp.jitterNdc[0] = frame->jitterNdc[0]; mp.jitterNdc[1] = frame->jitterNdc[1];
		mp.extent[0] = frame->width; mp.extent[1] = frame->height;
		id<MTLComputeCommandEncoder> e = [command computeCommandEncoder];
		if (!e) return false;
		e.label = @"wired.taa.motion";
		[e setComputePipelineState:s->motion];
		[e setTexture:depth atIndex:0]; [e setTexture:c->velocity atIndex:1];
		[e setTexture:c->validity atIndex:2]; [e setTexture:c->reverseDepth atIndex:3];
		[e setBytes:&mp length:sizeof(mp) atIndex:0];
		wired_taa_dispatch(e, scene.width, scene.height);
		[e endEncoding];
	}
	{
		id<MTLComputeCommandEncoder> e = [command computeCommandEncoder];
		if (!e) return false;
		if (frame->historyValid) {
			wiredTaaResolvePush push;
			memset(&push, 0, sizeof(push));
			push.extent[0] = frame->width; push.extent[1] = frame->height;
			/* The projection shifts the image by +(jx, jy) pixels (y down), so the effective jitter the resolve compensates is +(jx, jy)
			 * in UV; the Vulkan path passes -jy because its projection chain flips y. */
			push.currentJitterUv[0] = frame->currentJitterUv[0]; push.currentJitterUv[1] = frame->currentJitterUv[1];
			push.previousJitterUv[0] = frame->previousJitterUv[0]; push.previousJitterUv[1] = frame->previousJitterUv[1];
			push.zNear = zNear; push.zFar = zNear * WIRED_TAA_FAR_SCALE;
			push.depthAbsolute = WIRED_TAA_DEPTH_ABSOLUTE; push.depthRelative = WIRED_TAA_DEPTH_RELATIVE;
			push.historyWeight = WIRED_TAA_HISTORY_WEIGHT;
			push.historyReadIndex = frame->readIndex; push.qualityFlags = WIRED_TAA_QUALITY_FLAGS;
			id<MTLBuffer> args = c->resolveArgs[frame->readIndex];
			e.label = @"wired.taa.resolve";
			[e setComputePipelineState:s->resolve];
			[e setBuffer:args offset:0 atIndex:0];
			[e setBytes:&push length:sizeof(push) atIndex:1];
			[e useResource:scene usage:MTLResourceUsageRead];
			[e useResource:c->historyColor[frame->readIndex] usage:MTLResourceUsageRead];
			[e useResource:c->historyDepth[frame->readIndex] usage:MTLResourceUsageRead];
			[e useResource:c->velocity usage:MTLResourceUsageRead];
			[e useResource:c->validity usage:MTLResourceUsageRead];
			[e useResource:c->reverseDepth usage:MTLResourceUsageRead];
			[e useResource:c->output usage:MTLResourceUsageWrite];
			[e useResource:c->historyColor[frame->writeIndex] usage:MTLResourceUsageWrite];
			[e useResource:c->historyDepth[frame->writeIndex] usage:MTLResourceUsageWrite];
		} else {
			wiredTaaBootstrapPush push = { { frame->width, frame->height }, zNear, zNear * WIRED_TAA_FAR_SCALE };
			id<MTLBuffer> args = c->bootstrapArgs[frame->writeIndex];
			e.label = @"wired.taa.bootstrap";
			[e setComputePipelineState:s->bootstrap];
			[e setBuffer:args offset:0 atIndex:0];
			[e setBytes:&push length:sizeof(push) atIndex:1];
			[e useResource:scene usage:MTLResourceUsageRead];
			[e useResource:c->reverseDepth usage:MTLResourceUsageRead];
			[e useResource:c->output usage:MTLResourceUsageWrite];
			[e useResource:c->historyColor[frame->writeIndex] usage:MTLResourceUsageWrite];
			[e useResource:c->historyDepth[frame->writeIndex] usage:MTLResourceUsageWrite];
		}
		wired_taa_dispatch(e, scene.width, scene.height);
		[e endEncoding];
	}
	id<MTLBlitCommandEncoder> blit = [command blitCommandEncoder];
	if (!blit) return false;
	blit.label = @"wired.taa.copy";
	[blit copyFromTexture:c->output sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0, 0, 0)
		sourceSize:MTLSizeMake(scene.width, scene.height, 1)
		toTexture:scene destinationSlice:0 destinationLevel:0 destinationOrigin:MTLOriginMake(0, 0, 0)];
	[blit endEncoding];
	return true;
}
