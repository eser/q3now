// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "ral_metal_present.h"
#include "ral_metal_fragment_bindings.h"
#include "ral_metal_encoder_value.h"
#include "ral_metal_upload_cache.h"
#include "ral_metal_timing.h"
#include "ral_metal_internal.h"
#include "ral_particle_contract.h"
#include "ral_lighting_product.h"
#include "ral_presentation_policy.h"
#include "render_submission.h"
#include "render_material_script.h"
#include "render_texture_sampling.h"
#include "maps/map_format_registry.h"

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <vector>
#include <algorithm>
#include <memory>
#import <QuartzCore/CAMetalLayer.h>
#import <objc/runtime.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr,"FAIL %s:%d: %s\n",__FILE__,__LINE__,#x); return 1; } } while (0)

/* Test-only observation of the fixture's 69-index transient allocation. Weak
 * references prove deallocation, not merely a balanced logical resource count.
 * The scoped hook always restores the native method, including CHECK failures. */
static NSPointerArray *failedFrameEntityBuffers;
static IMP nativeNewEntityBufferWithLength;
static id ObserveEntityBuffer( id device, SEL selector,
		NSUInteger length, MTLResourceOptions options ) {
	id buffer = ((id (*)(id, SEL, NSUInteger, MTLResourceOptions))
		nativeNewEntityBufferWithLength)( device, selector, length, options );
	if ( length == 69u * sizeof( uint32_t ) )
		[failedFrameEntityBuffers addPointer:(void *)buffer];
	return buffer;
}
struct FailedFrameEntityBufferProbe {
	Method method;
	FailedFrameEntityBufferProbe( id<MTLDevice> device ) {
		failedFrameEntityBuffers = [[NSPointerArray alloc]
			initWithOptions:NSPointerFunctionsWeakMemory];
		method = class_getInstanceMethod( object_getClass( device ),
			@selector(newBufferWithLength:options:) );
		nativeNewEntityBufferWithLength = method_setImplementation( method,
			(IMP)ObserveEntityBuffer );
	}
	~FailedFrameEntityBufferProbe() {
		method_setImplementation( method, nativeNewEntityBufferWithLength );
		[failedFrameEntityBuffers release]; failedFrameEntityBuffers = nil;
	}
};

extern "C" void R_NoiseInit( void );
static IMP wiredNativeSampler;
static unsigned wiredSamplerCalls, wiredSamplerFailAt;
static MTLSamplerDescriptor *wiredLastSampler;
static id wired_observe_sampler( id device, SEL selector, MTLSamplerDescriptor *desc ) {
	++wiredSamplerCalls;
	[wiredLastSampler release]; wiredLastSampler = [desc copy];
	if ( wiredSamplerCalls == wiredSamplerFailAt ) return nil;
	return ((id (*)(id, SEL, MTLSamplerDescriptor *))wiredNativeSampler)(device, selector, desc);
}
struct wiredSamplerProbe {
	Method method;
	explicit wiredSamplerProbe( id<MTLDevice> device ) {
		wiredSamplerCalls = wiredSamplerFailAt = 0;
		method = class_getInstanceMethod(object_getClass(device), @selector(newSamplerStateWithDescriptor:));
		wiredNativeSampler = method_setImplementation(method, (IMP)wired_observe_sampler);
	}
	~wiredSamplerProbe() {
		method_setImplementation(method, wiredNativeSampler);
		[wiredLastSampler release]; wiredLastSampler = nil;
	}
};
extern "C" float R_NoiseGet4f( float x, float y, float z, double t );

/* Observe the one write-combined material packet allocation without retaining
 * it. Allocation failure must remain a pre-commit, same-generation retry. */
static NSPointerArray *materialUniformBuffers;
static unsigned failMaterialUniformAllocations;
static IMP nativeNewBufferWithLength;
static id wired_observe_uniform_buffer( id device, SEL selector,
        NSUInteger length, MTLResourceOptions options ) {
    const bool uniform = ( options & MTLResourceCPUCacheModeWriteCombined ) != 0u;
    if ( uniform && failMaterialUniformAllocations ) {
        fprintf( stderr, "material uniform injected allocation failure: bytes=%lu\n", (unsigned long)length );
        --failMaterialUniformAllocations;
        return nil;
    }
    id buffer = ((id (*)(id, SEL, NSUInteger, MTLResourceOptions))
        nativeNewBufferWithLength)( device, selector, length, options );
    if ( uniform && buffer ) [materialUniformBuffers addPointer:(void *)buffer];
    return buffer;
}
struct wiredMaterialUniformProbe {
    Method method;
    explicit wiredMaterialUniformProbe( id<MTLDevice> device ) {
        materialUniformBuffers = [[NSPointerArray alloc]
            initWithOptions:NSPointerFunctionsWeakMemory];
        failMaterialUniformAllocations = 0u;
        method = class_getInstanceMethod( object_getClass( device ),
            @selector(newBufferWithLength:options:) );
        nativeNewBufferWithLength = method_setImplementation( method,
            (IMP)wired_observe_uniform_buffer );
    }
    ~wiredMaterialUniformProbe() {
        method_setImplementation( method, nativeNewBufferWithLength );
        [materialUniformBuffers release]; materialUniformBuffers = nil;
        failMaterialUniformAllocations = 0u;
    }
};

struct wiredUniformQueueGate {
    id<MTLSharedEvent> event;
    explicit wiredUniformQueueGate( id<MTLDevice> device ) : event( [device newSharedEvent] ) {}
    ~wiredUniformQueueGate() { event.signaledValue = 1u; [event release]; }
};

/* Observe actual production upload allocations without extending their life.
 * Labels are assigned after allocation; collect each weak entry once after a
 * completed frame, so replacing a buffer cannot masquerade as cache reuse. */
static NSPointerArray *wiredParticleUploadBuffers;
static IMP wiredNativeParticleUploadBuffer;
static id wired_observe_particle_upload( id device, SEL selector,
		NSUInteger length, MTLResourceOptions options ) {
	id buffer = ((id (*)(id, SEL, NSUInteger, MTLResourceOptions))
		wiredNativeParticleUploadBuffer)( device, selector, length, options );
	if ( buffer && options == MTLResourceStorageModeShared )
		[wiredParticleUploadBuffers addPointer:(void *)buffer];
	return buffer;
}
struct wiredParticleUploadProbe {
	Method method;
	NSUInteger visited = 0u;
	unsigned allocations[7] = {};
	explicit wiredParticleUploadProbe( id<MTLDevice> device ) {
		wiredParticleUploadBuffers = [[NSPointerArray alloc]
			initWithOptions:NSPointerFunctionsWeakMemory];
		method = class_getInstanceMethod( object_getClass(device),
			@selector(newBufferWithLength:options:) );
		wiredNativeParticleUploadBuffer = method_setImplementation( method,
			(IMP)wired_observe_particle_upload );
	}
	void wired_collect() {
		NSString *labels[] = { @"Wired particle frame", @"Wired particle classes",
			@"Wired particle spawns", @"Wired particle integrate arguments",
			@"Wired particle spawn arguments", @"Wired particle vertex arguments",
			@"Wired particle fragment arguments" };
		for ( ; visited < wiredParticleUploadBuffers.count; ++visited ) {
			id<MTLBuffer> buffer = (id<MTLBuffer>)[wiredParticleUploadBuffers pointerAtIndex:visited];
			for ( unsigned i = 0u; i < 7u; ++i )
				if ( [buffer.label isEqualToString:labels[i]] ) ++allocations[i];
		}
	}
	~wiredParticleUploadProbe() {
		method_setImplementation( method, wiredNativeParticleUploadBuffer );
		[wiredParticleUploadBuffers release]; wiredParticleUploadBuffers = nil;
	}
};

/* The one-source lens fixture uses a 32-byte input and one float per ID.
 * Weak observations verify that the bounded cache is released at reset. */
static NSPointerArray *wiredLensBuffers;
static IMP wiredNativeLensBuffer;
static unsigned wiredLensAllocations;
static id wired_observe_lens_buffer( id device, SEL selector,
        NSUInteger length, MTLResourceOptions options ) {
    id buffer = ((id (*)(id, SEL, NSUInteger, MTLResourceOptions))
        wiredNativeLensBuffer)( device, selector, length, options );
    if ( buffer && options == MTLResourceStorageModeShared
            && ( length == 32u || length == LENS_MAX_CGSOURCES * sizeof(float) ) ) {
        ++wiredLensAllocations;
        [wiredLensBuffers addPointer:(void *)buffer];
    }
    return buffer;
}
struct wiredLensBufferProbe {
    Method method;
    explicit wiredLensBufferProbe( id<MTLDevice> device ) {
        wiredLensBuffers = [[NSPointerArray alloc] initWithOptions:NSPointerFunctionsWeakMemory];
        wiredLensAllocations = 0u;
        method = class_getInstanceMethod( object_getClass(device), @selector(newBufferWithLength:options:) );
        wiredNativeLensBuffer = method_setImplementation(method, (IMP)wired_observe_lens_buffer);
    }
    ~wiredLensBufferProbe() {
        method_setImplementation(method, wiredNativeLensBuffer);
        [wiredLensBuffers release]; wiredLensBuffers = nil;
    }
};

static unsigned acquireProbeReleased;
static IMP nativeOutputEncoder;
static id<MTLTexture> failedOutputTexture;
static IMP wiredNativeComputeTexture;
static id<MTLTexture> wiredDiscardedDepth;
static unsigned wiredDiscardedDepthReads;
static void wired_observe_compute_texture( id encoder, SEL selector,
		id<MTLTexture> texture, NSUInteger index ) {
	if ( texture && texture == wiredDiscardedDepth ) ++wiredDiscardedDepthReads;
	((void (*)(id, SEL, id<MTLTexture>, NSUInteger))wiredNativeComputeTexture)(
		encoder, selector, texture, index );
}
static id FailDrawableOutputEncoder( id command, SEL selector,
		MTLRenderPassDescriptor *descriptor ) {
	if ( failedOutputTexture && descriptor.colorAttachments[0].texture == failedOutputTexture ) {
		failedOutputTexture = nil;
		return nil;
	}
	if ( descriptor.depthAttachment.texture )
		wiredDiscardedDepth = descriptor.depthAttachment.storeAction == MTLStoreActionDontCare
			? descriptor.depthAttachment.texture : nil;
	return ((id (*)(id, SEL, MTLRenderPassDescriptor *))nativeOutputEncoder)(
		command, selector, descriptor );
}
struct FailedOutputEncoderProbe {
	Method method, computeTextureMethod;
	FailedOutputEncoderProbe( id<MTLCommandQueue> queue ) {
		wiredDiscardedDepth = nil; wiredDiscardedDepthReads = 0u;
		method = class_getInstanceMethod( object_getClass( [queue commandBuffer] ),
			@selector(renderCommandEncoderWithDescriptor:) );
		nativeOutputEncoder = method_setImplementation( method, (IMP)FailDrawableOutputEncoder );
		id<MTLComputeCommandEncoder> compute = [[queue commandBuffer] computeCommandEncoder];
		computeTextureMethod = class_getInstanceMethod( object_getClass( compute ),
			@selector(setTexture:atIndex:) );
		wiredNativeComputeTexture = method_setImplementation( computeTextureMethod,
			(IMP)wired_observe_compute_texture );
		[compute endEncoding];
	}
	~FailedOutputEncoderProbe() {
		method_setImplementation( method, nativeOutputEncoder );
		method_setImplementation( computeTextureMethod, wiredNativeComputeTexture );
		wiredDiscardedDepth = nil;
		failedOutputTexture = nil;
	}
};
@interface AcquireLifetimeProbe : NSObject
@end
@implementation AcquireLifetimeProbe
- (void)dealloc { ++acquireProbeReleased; [super dealloc]; }
@end

/* Observe the engine's command lease, not native command deallocation: a Metal
 * queue/debugger may retain a completed native command independently of Wired.
 * Forward every GPU operation unchanged; only engine references own this test
 * object. A missing engine release therefore still keeps the weak entry alive. */
@interface WiredObservedCommandLease : NSObject {
	id<MTLCommandBuffer> nativeCommand;
}
- (instancetype)initWithCommand:(id<MTLCommandBuffer>)command;
- (id<MTLCommandBuffer>)nativeCommand;
@end
@implementation WiredObservedCommandLease
- (instancetype)initWithCommand:(id<MTLCommandBuffer>)command {
	self = [super init];
	if ( self ) nativeCommand = [command retain];
	return self;
}
- (id<MTLCommandBuffer>)nativeCommand { return nativeCommand; }
- (id)forwardingTargetForSelector:(SEL)selector { return nativeCommand; }
- (void)dealloc { [nativeCommand release]; [super dealloc]; }
@end

static NSPointerArray *wiredObservedCommands;
static IMP wiredNativeCommandBuffer;
static id wired_observe_command_buffer( id queue, SEL selector ) {
	id command = ((id (*)(id, SEL))wiredNativeCommandBuffer)( queue, selector );
	if ( !command ) return nil;
	id lease = [[[WiredObservedCommandLease alloc] initWithCommand:command] autorelease];
	[wiredObservedCommands addPointer:(void *)lease];
	return lease;
}
/* Queue-gate/barrier commands belong to the fixture, not the engine. Do not
 * let their lifetime satisfy the pending-engine-lease assertion. */
static id<MTLCommandBuffer> wired_fixture_command( id<MTLCommandQueue> queue ) {
	return ((id (*)(id, SEL))wiredNativeCommandBuffer)( queue, @selector(commandBuffer) );
}
struct wiredCommandLeaseProbe {
	Method method;
	explicit wiredCommandLeaseProbe( id<MTLCommandQueue> queue ) {
		wiredObservedCommands = [[NSPointerArray alloc]
			initWithOptions:NSPointerFunctionsWeakMemory];
		method = class_getInstanceMethod( object_getClass( queue ), @selector(commandBuffer) );
		wiredNativeCommandBuffer = method_setImplementation( method, (IMP)wired_observe_command_buffer );
	}
	~wiredCommandLeaseProbe() {
		method_setImplementation( method, wiredNativeCommandBuffer );
		[wiredObservedCommands release]; wiredObservedCommands = nil;
	}
};

/* Deterministically distinguish outside native retention from a leaked engine
 * lease. Neither case depends on the driver's post-completion cleanup timing. */
static int wired_command_lease_observer_contract( void ) {
	@autoreleasepool {
		id<MTLDevice> device = MTLCreateSystemDefaultDevice();
		CHECK( device );
		id<MTLCommandQueue> queue = [device newCommandQueue];
		CHECK( queue );
		{
			wiredCommandLeaseProbe observation( queue );
			id<MTLCommandBuffer> outsideNativeOwner = nil;
			id engineLease = nil;
			@autoreleasepool {
				id<MTLCommandBuffer> command = [queue commandBuffer];
				outsideNativeOwner = [[(WiredObservedCommandLease *)command nativeCommand] retain];
				engineLease = [command retain];
				[command commit]; [command waitUntilCompleted];
				CHECK( command.status == MTLCommandBufferStatusCompleted );
			}
			CHECK( wiredObservedCommands.count == 1u );
			@autoreleasepool {
				// Completed but still retained by an engine owner: MUST be detected.
				CHECK( [wiredObservedCommands pointerAtIndex:0] != NULL );
			}
			[engineLease release]; engineLease = nil;
			CHECK( [wiredObservedCommands pointerAtIndex:0] == NULL );
			CHECK( outsideNativeOwner.status == MTLCommandBufferStatusCompleted );
			[outsideNativeOwner release];
		}
		[queue release]; [device release];
	}
	return 0;
}

@interface AcquireProbeLayer : CAMetalLayer
@property(nonatomic) BOOL returnNoDrawable;
@property(nonatomic) unsigned acquireCount;
@property(nonatomic, assign) ralMetalCore_t *publishCore;
@property(nonatomic) BOOL publishedInterveningCommand;
@property(nonatomic) BOOL failOutputEncoder;
@property(nonatomic, assign) ralMetalPresent_t *observeScene;
@property(nonatomic, assign) ralMetalCore_t *observationCore;
@property(nonatomic) BOOL sceneCommittedBeforeAcquire;
@property(nonatomic) uint64_t weatherHashBeforeAcquire;
@property(nonatomic) BOOL weatherWrittenBeforeAcquire;
@property(nonatomic, assign) NSPointerArray *completedCommands;
@property(nonatomic) BOOL completedCommandsReleasedBeforeAcquire;
@end
@implementation AcquireProbeLayer
- (id<CAMetalDrawable>)nextDrawable {
    ++self.acquireCount;
    if ( self.completedCommands ) {
        self.completedCommandsReleasedBeforeAcquire = YES;
        for ( NSUInteger i = 0; i < self.completedCommands.count; ++i )
            if ( [self.completedCommands pointerAtIndex:i] )
                self.completedCommandsReleasedBeforeAcquire = NO;
    }
    if ( self.observeScene ) {
        ralMetalSimulationHistory_t history = {};
        self.sceneCommittedBeforeAcquire = RalMetal_PresentSimulationHistory(
            self.observeScene, &history ) && history.particleNextSlot > 0u;
        /* Test-only queue barrier: if scene work was actually committed before
         * acquisition, its weather output is already visible behind this fence. */
        if ( history.weatherParity && self.observationCore ) {
            id<MTLCommandBuffer> barrier = [RalMetal_CoreNativeQueue( self.observationCore ) commandBuffer];
            [barrier commit]; [barrier waitUntilCompleted];
            id<MTLBuffer> buffer = RalMetal_PresentNativeWeatherBuffer( self.observeScene );
            const byte *bytes = (const byte *)buffer.contents;
            uint64_t hash = UINT64_C(1469598103934665603);
            for ( NSUInteger i = 0u; i < buffer.length; ++i ) {
                hash ^= bytes[i]; hash *= UINT64_C(1099511628211);
                if ( bytes[i] ) self.weatherWrittenBeforeAcquire = YES;
            }
            self.weatherHashBeforeAcquire = hash;
        }
    }
    if ( self.publishCore ) {
        /* Deterministic post-submit receipt failure injection: a real empty
         * native command advances the core publication cursor while the frame
         * is being prepared. The frame itself still executes on the real GPU;
         * its later receipt publication must reject the stale cursor. */
        ralMetalCore_t *core = self.publishCore; self.publishCore = NULL;
        ralCommandLifecycle_t lifecycle;
        ralCommandReceipt_t recording, executable;
        ralSubmissionReceipt_t submission;
        id<MTLCommandBuffer> command = [RalMetal_CoreNativeQueue( core ) commandBuffer];
        if ( command && RalMetal_CoreBeginCommand( core, 100u, &lifecycle, &recording )
                && Ral_CommandLifecyclePublishEnd( &lifecycle, &recording, &executable ) == ralSuccess ) {
            [command commit];
            self.publishedInterveningCommand = RalMetal_CorePublishSubmission(
                core, &lifecycle, &executable, &submission );
        }
    }
    [[[AcquireLifetimeProbe alloc] init] autorelease];
    id<CAMetalDrawable> drawable = self.returnNoDrawable ? nil : [super nextDrawable];
    if ( self.failOutputEncoder && drawable ) {
        self.failOutputEncoder = NO;
        failedOutputTexture = drawable.texture;
    }
    return drawable;
}
@end


static int wired_acquire_partition_test() {
	struct Case { double begin, end, sceneBegin, sceneEnd; uint64_t before, during, after; };
	const Case cases[] = {
		{ 1, 5, 2, 4, 1, 2, 1 }, // all three spans
		{ 1, 2, 3, 4, 1, 0, 0 }, // scene has not started
		{ 3, 4, 1, 2, 0, 0, 1 }, // scene finished before acquire
		{ 2, 3, 1, 4, 0, 1, 0 }, // acquire is within scene
		{ 2, 2, 1, 3, 0, 0, 0 }, // zero-duration sample
		{ 1, 2, 2, 3, 1, 0, 0 }, // touching endpoints
		{ 2, 3, 1, 2, 0, 0, 1 },
	};
	for ( const auto &c : cases ) {
		wiredMetalAcquirePartition p = {};
		CHECK( wired_metal_acquire_partition(c.begin, c.end, c.sceneBegin, c.sceneEnd, &p) );
		CHECK( p.beforeScene == c.before * UINT64_C(1000000000)
			&& p.duringScene == c.during * UINT64_C(1000000000)
			&& p.afterScene == c.after * UINT64_C(1000000000)
			&& p.duration == p.beforeScene + p.duringScene + p.afterScene );
	}
	wiredMetalAcquirePartition output = { 1, 2, 3, 4 }, saved = output;
	CHECK( !wired_metal_acquire_partition(NAN, 2, 1, 3, &output) );
	CHECK( !wired_metal_acquire_partition(1, INFINITY, 1, 3, &output) );
	CHECK( !wired_metal_acquire_partition(2, 1, 1, 3, &output) );
	CHECK( !wired_metal_acquire_partition(1, 2, 3, 2, &output) );
	CHECK( !wired_metal_acquire_partition(0, 2, 1, 3, &output) );
	CHECK( !wired_metal_acquire_partition(1, 86402, 1, 3, &output) );
	CHECK( !wired_metal_acquire_partition(1, 2, 1, 3, NULL) );
	CHECK( !memcmp(&output, &saved, sizeof(output)) );
	return 0;
}

static int wired_upload_cache_busy_retirement( id<MTLDevice> device ) {
	wiredMaterialUniformProbe probe( device );
	wiredMetalUploadCache cache = {};
	@autoreleasepool {
		id<MTLCommandQueue> queue = [device newCommandQueue];
		id<MTLBuffer> output = [device newBufferWithLength:3u * 256u
			options:MTLResourceStorageModeShared];
		wiredUniformQueueGate gate( device );
		CHECK( queue && output && gate.event );
		id<MTLCommandBuffer> block = [queue commandBuffer];
		[block encodeWaitForEvent:gate.event value:1u]; [block commit];
		for ( unsigned frame = 0u; frame < 3u; ++frame ) {
			wiredMetalUpload upload( cache );
			CHECK( upload.wired_reserve( device, 256u,
				MTLResourceStorageModeShared | MTLResourceCPUCacheModeWriteCombined ) );
			CHECK( upload.cacheSlot == ( frame < 2u ? frame : 2u ) );
			memset( upload.buffer.contents, 0x31 + frame, 256u );
			id<MTLCommandBuffer> command = [queue commandBuffer];
			id<MTLBlitCommandEncoder> blit = [command blitCommandEncoder];
			[blit copyFromBuffer:upload.buffer sourceOffset:0u toBuffer:output
				destinationOffset:frame * 256u size:256u];
			[blit endEncoding]; [command commit];
			upload.wired_publish( command );
		}
		CHECK( materialUniformBuffers.count == 3u );
		/* Clearing CPU ownership while work is blocked must not free or alter
		 * native-owned input. The third upload was never cached. */
		cache.wired_clear();
		CHECK( !cache.slots[0].buffer && !cache.slots[1].buffer );
		@autoreleasepool {
			for ( NSUInteger i = 0u; i < 3u; ++i )
				CHECK( [materialUniformBuffers pointerAtIndex:i] != NULL );
		}
		gate.event.signaledValue = 1u;
		id<MTLCommandBuffer> completed = [queue commandBuffer];
		[completed commit]; [completed waitUntilCompleted];
		CHECK( completed.status == MTLCommandBufferStatusCompleted );
		const byte *bytes = (const byte *)output.contents;
		for ( unsigned frame = 0u; frame < 3u; ++frame )
			for ( unsigned i = 0u; i < 256u; ++i )
				CHECK( bytes[frame * 256u + i] == 0x31 + frame );
		[output release]; [queue release];
	}
	for ( NSUInteger i = 0u; i < 3u; ++i )
		CHECK( [materialUniformBuffers pointerAtIndex:i] == NULL );
	fprintf( stderr, "upload cache: two busy entries plus transient overflow, clear-before-completion, GPU byte identity and native retirement passed\n" );
	return 0;
}

template <NSUInteger Slots> static int wired_fragment_binding_changes() {
	/* Identity-only test tokens; the observer must never message or own them. */
	NSObject *a = [[[NSObject alloc] init] autorelease];
	NSObject *b = [[[NSObject alloc] init] autorelease];
	id<MTLTexture> textures[Slots] = {};
	id<MTLSamplerState> samplers[Slots] = {};
	wiredMetalFragmentBindings<Slots> bindings;
	CHECK( bindings.wired_textures_changed( textures ) );
	CHECK( bindings.wired_samplers_changed( samplers ) );
	CHECK( !bindings.wired_textures_changed( textures ) );
	CHECK( !bindings.wired_samplers_changed( samplers ) );
	for ( NSUInteger slot = 0; slot < Slots; ++slot ) {
		textures[slot] = (id<MTLTexture>)a;
		CHECK( bindings.wired_textures_changed( textures ) );
		CHECK( !bindings.wired_samplers_changed( samplers ) );
		samplers[slot] = (id<MTLSamplerState>)a;
		CHECK( bindings.wired_samplers_changed( samplers ) );
		CHECK( !bindings.wired_textures_changed( textures ) );
		/* A filter-only reload changes a sampler without replacing its texture. */
		samplers[slot] = (id<MTLSamplerState>)b;
		CHECK( bindings.wired_samplers_changed( samplers ) );
		CHECK( !bindings.wired_textures_changed( textures ) );
		textures[slot] = (id<MTLTexture>)b;
		CHECK( bindings.wired_textures_changed( textures ) );
		CHECK( !bindings.wired_samplers_changed( samplers ) );
	}
	for ( unsigned draw = 0; draw < 100; ++draw ) {
		CHECK( !bindings.wired_textures_changed( textures ) );
		CHECK( !bindings.wired_samplers_changed( samplers ) );
	}
	/* Encoder/cohort restart must rebind even an identical table. */
	wiredMetalFragmentBindings<Slots> restarted;
	CHECK( restarted.wired_textures_changed( textures ) );
	CHECK( restarted.wired_samplers_changed( samplers ) );
	for ( NSUInteger slot = 0; slot < Slots; ++slot ) {
		textures[slot] = nil;
		CHECK( bindings.wired_textures_changed( textures ) );
		samplers[slot] = nil;
		CHECK( bindings.wired_samplers_changed( samplers ) );
	}
	return 0;
}

/* Both shader stages must read the selected packet, not just agree on CPU
 * bookkeeping. Exercise offset-only, buffer replacement and encoder restart. */
static int wired_material_buffer_offsets() {
	id<MTLDevice> device = MTLCreateSystemDefaultDevice();
	CHECK( device );
	NSError *error = nil;
	id<MTLLibrary> library = [device newLibraryWithSource:@"#include <metal_stdlib>\n"
		"using namespace metal; struct V { float4 p [[position]]; float4 c; };\n"
		"vertex V vs(uint i [[vertex_id]], constant float4& c [[buffer(2)]]) {\n"
		"float2 p[3]={float2(-1,-1),float2(3,-1),float2(-1,3)}; return {float4(p[i],0,1),c}; }\n"
		"fragment float4 fs(V v [[stage_in]], constant float4& c [[buffer(0)]]) { return (v.c+c)*0.5; }\n"
		options:nil error:&error];
	CHECK( library );
	MTLRenderPipelineDescriptor *descriptor = [[[MTLRenderPipelineDescriptor alloc] init] autorelease];
	descriptor.vertexFunction = [library newFunctionWithName:@"vs"];
	descriptor.fragmentFunction = [library newFunctionWithName:@"fs"];
	descriptor.colorAttachments[0].pixelFormat = MTLPixelFormatRGBA8Unorm;
	id<MTLRenderPipelineState> pipeline = [device newRenderPipelineStateWithDescriptor:descriptor error:&error];
	[descriptor.vertexFunction release]; [descriptor.fragmentFunction release];
	CHECK( pipeline );
	id<MTLBuffer> a = [device newBufferWithLength:768u options:MTLResourceStorageModeShared];
	id<MTLBuffer> b = [device newBufferWithLength:256u options:MTLResourceStorageModeShared];
	CHECK( a && b );
	const float colors[4][4] = { {1,0,0,1}, {0,1,0,1}, {0,0,1,1}, {1,1,0,1} };
	memcpy( (byte *)a.contents, colors[0], 16u );
	memcpy( (byte *)a.contents + 256u, colors[1], 16u );
	memcpy( (byte *)a.contents + 512u, colors[3], 16u );
	memcpy( b.contents, colors[2], 16u );
	MTLTextureDescriptor *textureDesc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:
		MTLPixelFormatRGBA8Unorm width:4u height:1u mipmapped:NO];
	textureDesc.storageMode = MTLStorageModeShared;
	textureDesc.usage = MTLTextureUsageRenderTarget;
	id<MTLTexture> texture = [device newTextureWithDescriptor:textureDesc];
	id<MTLCommandQueue> queue = [device newCommandQueue];
	CHECK( texture && queue );
	id<MTLCommandBuffer> command = [queue commandBuffer];
	for ( unsigned passIndex = 0u; passIndex < 2u; ++passIndex ) {
		MTLRenderPassDescriptor *pass = [MTLRenderPassDescriptor renderPassDescriptor];
		pass.colorAttachments[0].texture = texture;
		pass.colorAttachments[0].loadAction = passIndex ? MTLLoadActionLoad : MTLLoadActionClear;
		pass.colorAttachments[0].storeAction = MTLStoreActionStore;
		id<MTLRenderCommandEncoder> encoder = [command renderCommandEncoderWithDescriptor:pass];
		CHECK( encoder );
		[encoder setRenderPipelineState:pipeline];
		wiredMetalEncoderValue<id<MTLBuffer>> binding;
		for ( unsigned pixel = passIndex ? 3u : 0u; pixel < (passIndex ? 4u : 3u); ++pixel ) {
			[encoder setViewport:(MTLViewport){ (double)pixel, 0.0, 1.0, 1.0, 0.0, 1.0 }];
			const NSUInteger offset = pixel == 1u ? 256u : pixel == 3u ? 512u : 0u;
			wired_metal_bind_material_buffer( encoder, pixel == 2u ? b : a, offset, binding );
			[encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0u vertexCount:3u];
		}
		[encoder endEncoding];
	}
	[command commit]; [command waitUntilCompleted];
	CHECK( command.status == MTLCommandBufferStatusCompleted );
	byte pixels[16];
	[texture getBytes:pixels bytesPerRow:16u fromRegion:MTLRegionMake2D(0u,0u,4u,1u) mipmapLevel:0u];
	for ( unsigned pixel = 0u; pixel < 4u; ++pixel )
		for ( unsigned channel = 0u; channel < 4u; ++channel )
			CHECK( pixels[pixel * 4u + channel] == (byte)(colors[pixel][channel] * 255.0f) );
	[queue release]; [texture release]; [a release]; [b release];
	[pipeline release]; [library release]; [device release];
	return 0;
}

static int wired_encoder_state_changes() {
	wiredMetalEncoderValue<id> pipeline, depth;
	wiredMetalEncoderValue<MTLCullMode> cull;
	NSObject *a = [[[NSObject alloc] init] autorelease];
	NSObject *b = [[[NSObject alloc] init] autorelease];
	CHECK( pipeline.wired_changed( nil ) );
	CHECK( depth.wired_changed( nil ) );
	CHECK( cull.wired_changed( MTLCullModeNone ) );
	CHECK( pipeline.wired_changed( a ) );
	CHECK( depth.wired_changed( a ) );
	for ( unsigned draw = 0; draw < 100; ++draw ) {
		CHECK( !pipeline.wired_changed( a ) );
		CHECK( !depth.wired_changed( a ) );
		CHECK( !cull.wired_changed( MTLCullModeNone ) );
	}
	CHECK( depth.wired_changed( b ) ); // e.g. same blend pipeline, different depth write
	CHECK( !pipeline.wired_changed( a ) );
	CHECK( pipeline.wired_changed( b ) );
	CHECK( !depth.wired_changed( b ) );
	CHECK( cull.wired_changed( MTLCullModeFront ) );
	CHECK( cull.wired_changed( MTLCullModeBack ) );
	CHECK( cull.wired_changed( MTLCullModeNone ) );
	CHECK( depth.wired_changed( nil ) );
	CHECK( !depth.wired_changed( nil ) );
	wiredMetalEncoderValue<id> restarted;
	CHECK( restarted.wired_changed( b ) ); // fresh encoder/cohort must always bind
	return 0;
}

static ralIrradianceEntitySampleReceipt_t LocalIrradiance(
		uint64_t generation, uint64_t entityId ) {
	ralIrradianceEntitySampleReceipt_t receipt;
	memset( &receipt, 0, sizeof( receipt ) );
	receipt.schemaVersion = RAL_IRRADIANCE_ENTITY_RECEIPT_SCHEMA_VERSION;
	receipt.queryGeneration = generation;
	receipt.entityId = entityId;
	receipt.volumeId = 7u;
	receipt.layoutHash = 8u;
	receipt.probes.schemaVersion = RAL_IRRADIANCE_RECEIPT_SCHEMA_VERSION;
	receipt.probes.queryGeneration = generation;
	receipt.probes.productGeneration = 9u;
	receipt.probes.fallback = RAL_IRRADIANCE_FALLBACK_LIGHTGRID;
	receipt.probes.usedFallback = qtrue;
	receipt.probes.ready = qtrue;
	receipt.contributorHash = 10u;
	for ( uint32_t channel = 0u; channel < 3u; ++channel ) {
		receipt.blendedCoefficientsQ16[0][channel] = RAL_LIGHT_Q16_ONE;
		receipt.diffuseIrradianceQ16[channel] = RAL_LIGHT_Q16_ONE;
	}
	receipt.coefficientHash = Ral_IrradianceCoefficientHash(
		receipt.blendedCoefficientsQ16 );
	receipt.usedFallback = qtrue;
	receipt.ready = qtrue;
	return receipt;
}

/* A draw receipt alone cannot prove that the newly spawned pool is visible.
 * Exercise real GPU pixels without UI, world geometry or other FX hiding it.
 * Far-forward depth additionally catches treating Metal's cleared depth as
 * nearby geometry and fading a perfectly visible free-space particle. */
static int AcquireAutoreleaseLifetime( void ) {
    @autoreleasepool {
        ralMetalCoreCreateInfo_t info = { 1u };
        ralMetalCore_t *core = NULL;
        ralMetalCoreReceipt_t coreReceipt;
        ralMetalPresent_t *present = NULL;
        ralMetalPresentLayerReceipt_t layerReceipt;
        ralMetalDrawableReceipt_t drawable = {}, unchanged = {};
        ralSurfaceFormat_t format = {
            RAL_FORMAT_B8G8R8A8_UNORM, RAL_COLORSPACE_SRGB_NONLINEAR };
        ralPresentPreference_t mode = { RAL_PRESENT_IMMEDIATE, 2u, 2u };
        ralSwapchainCreateInfo_t create = {};
        CHECK( RalMetal_CoreCreate( &info, &core, &coreReceipt ) );
        AcquireProbeLayer *layer = [[AcquireProbeLayer alloc] init];
        create.desiredWidth = 16u; create.desiredHeight = 16u; // no window
        create.formatPreferences = &format; create.formatPreferenceCount = 1u;
        create.presentPreferences = &mode; create.presentPreferenceCount = 1u;
        create.requiredUsage = RAL_TEXTURE_USAGE_COLOR_ATTACHMENT;
        CHECK( RalMetal_PresentAdoptBorrowedLayer( core, &coreReceipt,
            &create, 2u, layer, &present, &layerReceipt ) );
        ralMetalGpuTiming_t timing = {}, zeroTiming = {};
        timing.sampleCount = 17u;
        CHECK( !RalMetal_PresentGpuTiming( NULL, &timing ) );
        CHECK( timing.sampleCount == 17u );
        CHECK( !RalMetal_PresentGpuTiming( present, NULL ) );
        CHECK( RalMetal_PresentGpuTiming( present, &timing ) );
        CHECK( !memcmp( &timing, &zeroTiming, sizeof( timing ) ) );
        acquireProbeReleased = 0u;
        layer.returnNoDrawable = YES;
        CHECK( !RalMetal_PresentAcquire( present, &coreReceipt, &layerReceipt,
            3u, &drawable ) );
        CHECK( !memcmp( &drawable, &unchanged, sizeof( drawable ) ) );
        CHECK( acquireProbeReleased == 1u );
        const float black[4] = { 0, 0, 0, 1 };
        ralMetalPresentReceipt_t receipt = {}, before = {};
        ralMetalSimulationHistory_t history = {}, initialHistory = {};
        CHECK( RalMetal_PresentSimulationHistory( present, &initialHistory ) );
        for ( unsigned attempt = 0u; attempt < 3u; ++attempt ) {
            CHECK( RalMetal_PresentRenderAndSubmit( present, &coreReceipt,
                &layerReceipt, NULL, black, 3u, 75u, &drawable, &receipt, NULL )
                == RAL_METAL_SUBMIT_DRAWABLE_UNAVAILABLE );
            CHECK( !memcmp( &drawable, &unchanged, sizeof( drawable ) ) );
            CHECK( !memcmp( &receipt, &before, sizeof( receipt ) ) );
            CHECK( RalMetal_PresentSimulationHistory( present, &history ) );
            CHECK( !memcmp( &history, &initialHistory, sizeof( history ) ) );
        }
        CHECK( acquireProbeReleased == 4u );
        layer.returnNoDrawable = NO;
        CHECK( RalMetal_PresentAcquire( present, &coreReceipt, &layerReceipt,
            3u, &drawable ) );
        CHECK( acquireProbeReleased == 5u );
        CHECK( drawable.acquired && drawable.textureIdentity );
        CHECK( RalMetal_PresentClearAndSubmit( present, &coreReceipt,
            &layerReceipt, &drawable, NULL, black, 75u, &receipt ) );
        CHECK( receipt.presented && receipt.presentGeneration == 75u );
        RalMetal_PresentDestroy( present );
        [layer release];
        RalMetal_CoreDestroy( core );
    }
    return 0;
}

/* Reference pixels encode the expected Q3 byte color/alpha in a texture.
 * The reference uses identity generators: it cannot share the generator bug. */
static int MaterialGeneratorPixels() {
	struct Sample {
		const char *name;
		renderMaterialStageSnapshot_t stage;
		byte rgba[4];
		qboolean graySource;
		qboolean modelSource;
		qboolean legacyGrid;
		byte gridAzimuth;
		qboolean implicitLighting;
	};
	std::vector<Sample> samples;
	const float levels[] = { -.25f, 0, .001f, .499f, .5f, .501f, .999f, 1.25f };
	for (unsigned alpha = 0; alpha < 2; ++alpha) for (float level : levels) {
		Sample s = {}; s.name = alpha ? "alpha-wave" : "rgb-wave";
		memset(s.rgba, 255, sizeof(s.rgba)); s.graySource = alpha ? qtrue : qfalse;
		const byte value = (byte)(255 * fminf(1, fmaxf(0, level)));
		if (alpha) {
			s.stage.alphaGen = RAL_MATERIAL_ALPHA_WAVE;
			s.stage.alphaWave.function = RAL_MATERIAL_WAVE_SIN;
			s.stage.alphaWave.base = level;
			s.rgba[0] = s.rgba[1] = s.rgba[2] = 128; s.rgba[3] = value;
		} else {
			s.stage.rgbGen = RAL_MATERIAL_COLOR_WAVE;
			s.stage.rgbWave.function = RAL_MATERIAL_WAVE_SIN;
			s.stage.rgbWave.base = level;
			s.rgba[0] = s.rgba[1] = s.rgba[2] = value;
		}
		samples.push_back(s);
		if (level >= 0 && level <= 1) {
			Sample constant = s; constant.name = alpha ? "alpha-const" : "rgb-const";
			for (unsigned c=0;c<4;++c) constant.stage.constantColor[c]=level;
			if (alpha) constant.stage.alphaGen=RAL_MATERIAL_ALPHA_CONST;
			else constant.stage.rgbGen=RAL_MATERIAL_COLOR_CONST;
			samples.push_back(constant);
		}
		if (alpha && level == .5f) {
			// 127/255 is below .5; unquantized .5 takes the opposite alpha-test branch.
			for (unsigned test=2; test<=3; ++test) {
				Sample cutoff=s; cutoff.name="alpha-wave-cutoff";
				cutoff.stage.alphaTest=test; cutoff.stage.alphaCutoff=.5f;
				samples.push_back(cutoff);
			}
		}
	}
	const ralMaterialColorGen_t colorModes[] = { RAL_MATERIAL_COLOR_ENTITY,
		RAL_MATERIAL_COLOR_ONE_MINUS_ENTITY, RAL_MATERIAL_COLOR_EXACT_VERTEX,
		RAL_MATERIAL_COLOR_VERTEX, RAL_MATERIAL_COLOR_ONE_MINUS_VERTEX };
	for (auto mode : colorModes) {
		Sample s={}; s.name="vertex-entity-color"; s.stage.rgbGen=mode;
		s.rgba[0]=63; s.rgba[1]=127; s.rgba[2]=191;
		s.rgba[3]=mode==RAL_MATERIAL_COLOR_VERTEX ? 51 : 255;
		if (mode==RAL_MATERIAL_COLOR_ONE_MINUS_ENTITY || mode==RAL_MATERIAL_COLOR_ONE_MINUS_VERTEX)
			for(unsigned c=0;c<3;++c) s.rgba[c]=255-s.rgba[c];
		samples.push_back(s);
		if (mode!=RAL_MATERIAL_COLOR_ONE_MINUS_VERTEX) {
			s.name="rgb-preserves-skip-alpha"; s.stage.alphaGen=RAL_MATERIAL_ALPHA_SKIP;
			s.rgba[3]=mode==RAL_MATERIAL_COLOR_ONE_MINUS_ENTITY ? 204 : 51;
			samples.push_back(s);
		}
	}

	// Same material, independent BSP vertex and entity RGBA. These must not alias.
	const byte meshColor[4] = { 19, 151, 93, 87 };
	for (auto mode : colorModes) {
		for (unsigned alphaMode : { 0u, 1u, 2u, 3u, 4u, 5u }) {
			Sample s = {}; s.name = "model-color-ownership"; s.modelSource = qtrue;
			s.stage.rgbGen = mode; s.stage.alphaGen = (ralMaterialAlphaGen_t)alphaMode;
			const bool entity = mode == RAL_MATERIAL_COLOR_ENTITY
				|| mode == RAL_MATERIAL_COLOR_ONE_MINUS_ENTITY;
			const bool inverse = mode == RAL_MATERIAL_COLOR_ONE_MINUS_ENTITY
				|| mode == RAL_MATERIAL_COLOR_ONE_MINUS_VERTEX;
			const byte entityColor[4] = { 63, 127, 191, 51 };
			for (unsigned c = 0; c < 3; ++c) {
				byte v = entity ? entityColor[c] : meshColor[c];
				s.rgba[c] = inverse ? 255-v : v;
			}
			if (alphaMode == 1 && mode == RAL_MATERIAL_COLOR_ONE_MINUS_VERTEX) continue;
			s.rgba[3] = alphaMode == 2 ? 51 : alphaMode == 3 ? 204
				: alphaMode == 4 ? 87 : alphaMode == 5 ? 168
				: alphaMode == 0 ? (mode == RAL_MATERIAL_COLOR_VERTEX ? 87 : 255)
				: entity ? (inverse ? 204 : 51) : 87;
			samples.push_back(s);
		}
	}
	// Q3 grid bytes are not linear SH. Front/back-facing models must keep the
	// ambient floor, clamp the directional term before adding it, then decode
	// the interpolated byte-domain vertex result. Reference is a baked texel.
	for (unsigned azimuth : {0u, 128u}) for (unsigned implicit : {0u, 1u}) {
		Sample s = {}; s.name = "legacy-grid-color-domain";
		s.modelSource = s.legacyGrid = qtrue; s.gridAzimuth = (byte)azimuth;
		s.implicitLighting = implicit ? qtrue : qfalse;
		s.stage.rgbGen = RAL_MATERIAL_COLOR_LIGHTING_DIFFUSE;
		const unsigned ambient[3] = {20, 50, 100}, directed[3] = {10, 30, 50};
		for (unsigned c = 0; c < 3; ++c)
			s.rgba[c] = (byte)std::min(255u, (unsigned)(ambient[c] * 2 * .6f + 32)
				+ (azimuth ? directed[c] * 2 : 0));
		s.rgba[3] = 255; samples.push_back(s);
	}
	ralMetalCoreCreateInfo_t info={1u}; ralMetalCore_t *core=NULL;
	ralMetalCoreReceipt_t coreReceipt; ralMetalPresent_t *present=NULL;
	ralMetalPresentLayerReceipt_t layer; ralMetalDrawableReceipt_t drawable;
	ralMetalPresentReceipt_t receipt; auto frontendStorage = std::make_unique<renderSubmissionState_t>();
	renderSubmissionState_t &frontend = *frontendStorage;
	renderSubmissionReceipt_t submission;
	ralSurfaceFormat_t format={RAL_FORMAT_B8G8R8A8_UNORM,RAL_COLORSPACE_SRGB_NONLINEAR};
	ralPresentPreference_t fifo={RAL_PRESENT_FIFO,3u,3u};
	ralSwapchainCreateInfo_t create={};
	create.desiredWidth=create.desiredHeight=16; // native offscreen layer, no window
	create.formatPreferences=&format; create.formatPreferenceCount=1;
	create.presentPreferences=&fifo; create.presentPreferenceCount=1;
	create.requiredUsage=RAL_TEXTURE_USAGE_COLOR_ATTACHMENT;
	CHECK(RalMetal_CoreCreate(&info,&core,&coreReceipt));
	CHECK(RenderSubmission_Init(&frontend,1u));
	CHECK(RalMetal_PresentCreate(core,&coreReceipt,&create,2u,&present,&layer));
	const byte white[4]={255,255,255,255}, gray[4]={128,128,128,255};
	const float black[4]={0,0,0,1};
	const qhandle_t whiteImage=RenderSubmission_RegisterMaterialImage(&frontend,
		RENDER_ASSET_MATERIAL,"test/generator-white",qtrue,white,1,1);
	const qhandle_t grayImage=RenderSubmission_RegisterMaterialImage(&frontend,
		RENDER_ASSET_MATERIAL,"test/generator-gray",qtrue,gray,1,1);
	const qhandle_t actual=RenderSubmission_RegisterMaterialImage(&frontend,
		RENDER_ASSET_MATERIAL,"test/generator-actual",qtrue,white,1,1);
	const qhandle_t implicitImage=RenderSubmission_RegisterMaterialImage(&frontend,
		RENDER_ASSET_MATERIAL,"test/grid-implicit",qtrue,white,1,1);
	CHECK(whiteImage>0 && grayImage>0 && actual>0 && implicitImage>0);
	mapFile_t world = {}; dsurface_t surface = {}; dshader_t shader = {};
	dmodel_t subModel = {};
	subModel.numSurfaces = 1;
	byte lightGrid[8] = {20, 50, 100, 10, 30, 50, 64, 0};
	world.numSubModels = 1; world.subModels = &subModel;
	world.numGridPoints = 1; world.lightGridData = lightGrid;
	drawVert_t vertices[4] = {}; int indices[6] = {0,1,2,0,2,3};
	strcpy(world.name,"maps/generator-color.bsp"); strcpy(shader.shader,"test/generator-actual");
	world.checksum=0x227456; world.numShaders=1; world.shaders=&shader;
	world.numSurfaces=1; world.surfaces=&surface; world.numDrawVerts=4;
	world.drawVerts=vertices; world.numDrawIndexes=6; world.drawIndexes=indices;
	surface.surfaceType=MST_PLANAR; surface.numVerts=4; surface.numIndexes=6;
	surface.lightmapNum=-1;
	const float yz[4][2]={{32,32},{-32,32},{-32,-32},{32,-32}};
	for (unsigned v=0;v<4;++v) {
		vertices[v].xyz[0]=8; vertices[v].xyz[1]=yz[v][0]; vertices[v].xyz[2]=yz[v][1];
		vertices[v].normal[0]=-1; memcpy(vertices[v].color.rgba,meshColor,4);
	}
	CHECK(RenderSubmission_LoadWorld(&frontend,&world,0));
	qhandle_t mesh=RenderSubmission_RegisterInlineModel(&frontend,"*generator-color",0,1);
	CHECK(mesh>0);
	refdef_t view={}; view.width=view.height=16; view.fov_x=view.fov_y=90;
	view.viewaxis[0][0]=view.viewaxis[1][1]=view.viewaxis[2][2]=1;
	view.rdflags=RDF_NOWORLDMODEL; view.time=1250;
	for (unsigned i=0;i<samples.size();++i) {
		const Sample &s=samples[i]; char name[64];
		snprintf(name,sizeof(name),"test/generator-reference-%u",i);
		const qhandle_t reference=RenderSubmission_RegisterMaterialImage(&frontend,
			RENDER_ASSET_MATERIAL,name,qtrue,s.rgba,1,1);
		CHECK(reference>0);
		byte expected[4]={};
		for(unsigned route=0;route<2;++route) {
			const qhandle_t material=route ? (s.implicitLighting ? implicitImage : actual) : reference;
			renderMaterialStageSnapshot_t stage=route ? s.stage : renderMaterialStageSnapshot_t{};
			stage.material=route ? (s.graySource ? grayImage : whiteImage) : reference;
			stage.scaleScroll[0]=stage.scaleScroll[1]=1;
			stage.sourceBlend=RENDER_MATERIAL_BLEND_SRC_ALPHA;
			stage.destinationBlend=RENDER_MATERIAL_BLEND_ONE_MINUS_SRC_ALPHA;
			stage.alphaTest=s.stage.alphaTest; stage.alphaCutoff=s.stage.alphaCutoff;
			if (!(route && s.implicitLighting))
				CHECK(RenderSubmission_SetMaterialStages(&frontend,material,&stage,1u));
			CHECK(RenderSubmission_SetMaterialRasterPolicy(&frontend,material,
				s.implicitLighting ? RENDER_ALPHA_OPAQUE : RENDER_ALPHA_BLEND,
				0, s.implicitLighting));
			refEntity_t surface={}; surface.reType=RT_SPRITE; surface.origin[0]=8;
			surface.radius=32; surface.customShader=material;
			surface.shader.rgba[0]=63; surface.shader.rgba[1]=127;
			surface.shader.rgba[2]=191; surface.shader.rgba[3]=51;
			if (s.modelSource) {
				surface.reType=RT_MODEL; surface.hModel=mesh; surface.origin[0]=0;
				surface.axis[0][0]=surface.axis[1][1]=surface.axis[2][2]=1;
			}
			const uint64_t generation=4u+i*4u+route*2u;
			CHECK(RenderSubmission_BeginFrame(&frontend,generation));
			CHECK(RenderSubmission_AddEntity(&frontend,&surface,NULL));
			if (route && s.legacyGrid) {
				lightGrid[7] = s.gridAzimuth;
				CHECK(RenderSubmission_AttachLegacyLightGridEntityIrradiance(
					&frontend, 0, .6f, 1.0f));
			}
			CHECK(RenderSubmission_RenderScene(&frontend,&view,0));
			CHECK(RalMetal_PresentAcquire(present,&coreReceipt,&layer,generation-1,&drawable));
			CHECK(RalMetal_PresentRequestCapture(present));
			CHECK(RalMetal_PresentClearAndSubmit(present,&coreReceipt,&layer,&drawable,
				&frontend,black,generation,&receipt));
			CHECK(receipt.loweredEntityIndexCount==6 && receipt.loweredWorldIndexCount==0);
			CHECK(receipt.readbackByteCount==4);
			if(!route) memcpy(expected,receipt.readbackBytes,4);
			else for(unsigned c=0;c<3;++c) {
				fprintf(stderr,"generator %s sample=%u channel=%u reference=%u actual=%u\n",
					s.name,i,c,expected[c],receipt.readbackBytes[c]);
				CHECK(abs((int)expected[c]-receipt.readbackBytes[c])<=1);
			}
			CHECK(RenderSubmission_EndFrame(&frontend,generation,&submission));
		}
	}
	RalMetal_PresentDestroy(present); RenderSubmission_Reset(&frontend); RalMetal_CoreDestroy(core);
	return 0;
}

// Independent reference for the established Q3 CPU specular-alpha equation.
// The scalar Q_rsqrt path used by the arm64 Vulkan renderer (q_math.c).
static float LegacyScalarInvSqrt(float number) {
	uint32_t bits; memcpy(&bits,&number,sizeof(bits));
	bits=0x5f3759dfu-(bits>>1);
	float result; memcpy(&result,&bits,sizeof(result));
	return result*(1.5f-number*.5f*result*result);
}
static byte LegacySpecularAlpha(const vec3_t point, const vec3_t normal, const vec3_t eye) {
	const vec3_t lightOrigin={-960,1980,96}; vec3_t light,viewer,reflected;
	VectorSubtract(lightOrigin,point,light);
	const float lightScale=LegacyScalarInvSqrt(DotProduct(light,light));
	VectorScale(light,lightScale,light);
	float d=DotProduct(normal,light);
	for(unsigned axis=0;axis<3;++axis) reflected[axis]=normal[axis]*2*d-light[axis];
	VectorSubtract(eye,point,viewer);
	float l=DotProduct(reflected,viewer)*LegacyScalarInvSqrt(DotProduct(viewer,viewer));
	if(l<0) return 0;
	l=l*l; l=l*l;
	return (byte)fminf(255,l*255);
}

// A seed, tested surface and optional probe separate depth comparison from
// depth writes. Expected visibility is computed independently on the CPU.
static int MaterialStageDepthPixels() {
	ralMetalCoreCreateInfo_t info={1u}; ralMetalCore_t *core=nullptr;
	ralMetalCoreReceipt_t coreReceipt; ralMetalPresent_t *present=nullptr;
	ralMetalPresentLayerReceipt_t layer; ralMetalDrawableReceipt_t drawable;
	ralMetalPresentReceipt_t receipt; renderSubmissionReceipt_t submission;
	auto storage=std::make_unique<renderSubmissionState_t>(); auto &frontend=*storage;
	ralSurfaceFormat_t format={RAL_FORMAT_B8G8R8A8_UNORM,RAL_COLORSPACE_SRGB_NONLINEAR};
	ralPresentPreference_t fifo={RAL_PRESENT_FIFO,3u,3u}; ralSwapchainCreateInfo_t create={};
	create.desiredWidth=create.desiredHeight=16; // offscreen CAMetalLayer; no window
	create.formatPreferences=&format; create.formatPreferenceCount=1;
	create.presentPreferences=&fifo; create.presentPreferenceCount=1;
	create.requiredUsage=RAL_TEXTURE_USAGE_COLOR_ATTACHMENT;
	CHECK(RalMetal_CoreCreate(&info,&core,&coreReceipt));
	CHECK(RenderSubmission_Init(&frontend,1u));
	CHECK(RalMetal_PresentCreate(core,&coreReceipt,&create,2u,&present,&layer));
	const byte colors[3][4]={{0,0,180,127},{180,0,0,127},{0,180,0,127}};
	const char *names[3]={"test/depth-seed","test/depth-target","test/depth-probe"};
	qhandle_t materials[3]; const float black[4]={0,0,0,1};
	for(unsigned i=0;i<3;++i) {
		materials[i]=RenderSubmission_RegisterMaterialImage(&frontend,RENDER_ASSET_MATERIAL,
			names[i],qtrue,colors[i],1,1); CHECK(materials[i]>0);
		CHECK(RenderSubmission_SetMaterialSort(&frontend,materials[i],3.0f+i*.1f));
		CHECK(RenderSubmission_SetMaterialCullMode(&frontend,materials[i],RENDER_CULL_NONE));
	}
	mapFile_t world={}; dsurface_t surfaces[3]={}; dshader_t shaders[3]={};
	drawVert_t vertices[12]={}; int indices[18]={};
	strcpy(world.name,"maps/stage-depth.bsp"); world.numShaders=3; world.shaders=shaders;
	world.surfaces=surfaces; world.numDrawVerts=12; world.drawVerts=vertices;
	world.numDrawIndexes=18; world.drawIndexes=indices;
	const float yz[4][2]={{32,32},{-32,32},{-32,-32},{32,-32}};
	const int quad[6]={0,1,2,0,2,3};
	for(unsigned i=0;i<3;++i) {
		strcpy(shaders[i].shader,names[i]); surfaces[i].shaderNum=i;
		surfaces[i].surfaceType=MST_PLANAR; surfaces[i].firstVert=i*4;
		surfaces[i].numVerts=4; surfaces[i].firstIndex=i*6; surfaces[i].numIndexes=6;
		surfaces[i].lightmapNum=-1; memcpy(indices+i*6,quad,sizeof(quad));
		for(unsigned v=0;v<4;++v) {
			vertices[i*4+v].xyz[1]=yz[v][0]; vertices[i*4+v].xyz[2]=yz[v][1];
			vertices[i*4+v].normal[0]=-1; memset(vertices[i*4+v].color.rgba,255,4);
		}
	}
	refdef_t view={}; view.width=view.height=16; view.fov_x=view.fov_y=90;
	view.viewaxis[0][0]=view.viewaxis[1][1]=view.viewaxis[2][2]=1;
	uint64_t generation=4; unsigned cases=0;
	for(unsigned model=0;model<2;++model) for(unsigned batchWrite=0;batchWrite<2;++batchWrite)
	for(unsigned function=0;function<2;++function) for(unsigned write=0;write<2;++write)
	for(unsigned distance=0;distance<3;++distance) for(unsigned alpha=0;alpha<3;++alpha)
	for(unsigned probe=0;probe<2;++probe) for(unsigned stageCount : {1u,2u,8u}) {
		// Multi-stage alpha rejection needs ordered draws; do not treat depth
		// collapse as a proof that those color/coverage programs can be fused.
		if(stageCount>1 && alpha) continue;
		const float x[3]={12.0f,8.0f+distance*4.0f,10.0f};
		// Independent visibility oracle: seed at 12, tested stage at 8/12/16,
		// then probe at 10. The probe reveals whether the stage wrote depth.
		const bool alphaPass=alpha!=2; // 127/255 passes LT .5, fails GE .5
		const bool targetPass=alphaPass && (function ? x[1]==x[0] : x[1]<=x[0]);
		float depth=targetPass && write ? x[1] : x[0];
		unsigned winner=probe && x[2]<=depth ? 2u : (targetPass ? 1u : 0u);
		const unsigned actualSurfaces=2u+probe;
		std::vector<byte> expected;
		for(unsigned route=0;route<2;++route) {
			world.numSurfaces=route ? actualSurfaces : 1u; world.checksum=(int)generation;
			for(unsigned i=0;i<3;++i) {
				for(unsigned v=0;v<4;++v) vertices[i*4+v].xyz[0]=x[i];
				renderMaterialStageSnapshot_t stage={};
				stage.material=materials[!route ? winner : i];
				stage.scaleScroll[0]=stage.scaleScroll[1]=1;
				stage.rgbGen=RAL_MATERIAL_COLOR_IDENTITY; stage.alphaGen=RAL_MATERIAL_ALPHA_IDENTITY;
				stage.sourceBlend=RENDER_MATERIAL_BLEND_ONE;
				stage.depthWrite=route && i==1 ? (qboolean)write : qtrue;
				stage.depthFunc=route && i==1 ? function : RAL_MATERIAL_DEPTH_LEQUAL;
				stage.alphaTest=route && i==1 && alpha ? alpha+1u : 0u;
				stage.alphaCutoff=.5f;
				renderMaterialStageSnapshot_t stages[8]={};
				for(unsigned s=0;s<stageCount;++s) {
					stages[s]=stage; stages[s].depthWrite=qfalse;
					if(stageCount==2 && s && stage.depthWrite
							&& stage.depthFunc==RAL_MATERIAL_DEPTH_LEQUAL)
						stages[s].depthFunc=RAL_MATERIAL_DEPTH_EQUAL;
				}
				// Two stages exercise base-write -> equal. Eight exercise the
				// write occurring only in the LAST stage (no first-stage shortcut).
				stages[stageCount==8 ? 7u : 0u].depthWrite=stage.depthWrite;
				CHECK(RenderSubmission_SetMaterialStages(&frontend,materials[i],stages,stageCount));
				CHECK(RenderSubmission_SetMaterialRasterPolicy(&frontend,materials[i],
					RENDER_ALPHA_OPAQUE,0,(qboolean)batchWrite));
			}
			CHECK(RenderSubmission_LoadWorld(&frontend,&world,0));
			CHECK(RenderSubmission_BeginFrame(&frontend,generation));
			view.rdflags=model ? RDF_NOWORLDMODEL : 0;
			if(model) for(unsigned i=0;i<(route ? actualSurfaces : 1u);++i) {
				// Geometry only depends on distance/surface, not raster state.
				// Reuse those immutable models instead of exhausting the bounded registry.
				char name[64]; snprintf(name,sizeof(name),"*depth-%u-%u",distance,i);
				refEntity_t entity={}; entity.reType=RT_MODEL;
				entity.hModel=RenderSubmission_RegisterInlineModel(&frontend,name,i,1);
				CHECK(entity.hModel>0); entity.customShader=materials[i];
				entity.axis[0][0]=entity.axis[1][1]=entity.axis[2][2]=1;
				memset(entity.shader.rgba,255,4); CHECK(RenderSubmission_AddEntity(&frontend,&entity,nullptr));
			}
			CHECK(RenderSubmission_RenderScene(&frontend,&view,0));
			CHECK(RalMetal_PresentAcquire(present,&coreReceipt,&layer,generation-1,&drawable));
			CHECK(RalMetal_PresentRequestCapture(present));
			CHECK(RalMetal_PresentClearAndSubmit(present,&coreReceipt,&layer,&drawable,
				&frontend,black,generation,&receipt));
			CHECK(model ? receipt.loweredEntityIndexCount==(route ? actualSurfaces*6u : 6u)
				: receipt.loweredWorldIndexCount==(route ? actualSurfaces*6u : 6u));
			uint32_t width=0,height=0; const byte *pixels=RalMetal_PresentCaptureRgb(present,&width,&height);
			CHECK(pixels && width==16 && height==16);
			if(!route) { expected.assign(pixels,pixels+width*height*3);
				CHECK(*std::max_element(expected.begin(),expected.end())>0); }
			else { int maxError=0;
				for(unsigned p=0;p<expected.size();++p) maxError=std::max(maxError,abs((int)pixels[p]-expected[p]));
				fprintf(stderr,"stage-depth model=%u batch-write=%u equal=%u write=%u x=%.0f alpha=%u probe=%u stages=%u maxError=%d\n",
					model,batchWrite,function,write,x[1],alpha,probe,stageCount,maxError); CHECK(maxError<=1); ++cases;
			}
			CHECK(RenderSubmission_EndFrame(&frontend,generation,&submission)); generation+=2;
		}
	}
	fprintf(stderr,"stage-depth %u GPU cases passed\n",cases);
	RalMetal_PresentDestroy(present); RenderSubmission_Reset(&frontend); RalMetal_CoreDestroy(core);
	return 0;
}

// Reference draws each authored additive stage separately over a colored
// framebuffer. The tested route fuses the same stages; no shader blend formula
// is copied into the oracle. Covers world and model submission independently.
static int MaterialAdditiveStagePixels() {
	ralMetalCoreCreateInfo_t info={1u}; ralMetalCore_t *core=nullptr;
	ralMetalCoreReceipt_t coreReceipt; ralMetalPresent_t *present=nullptr;
	ralMetalPresentLayerReceipt_t layer; ralMetalDrawableReceipt_t drawable;
	ralMetalPresentReceipt_t receipt; renderSubmissionReceipt_t submission;
	auto storage=std::make_unique<renderSubmissionState_t>(); auto &frontend=*storage;
	ralSurfaceFormat_t format={RAL_FORMAT_B8G8R8A8_UNORM,RAL_COLORSPACE_SRGB_NONLINEAR};
	ralPresentPreference_t fifo={RAL_PRESENT_FIFO,3u,3u}; ralSwapchainCreateInfo_t create={};
	create.desiredWidth=create.desiredHeight=16; // offscreen layer; no window
	create.formatPreferences=&format; create.formatPreferenceCount=1;
	create.presentPreferences=&fifo; create.presentPreferenceCount=1;
	create.requiredUsage=RAL_TEXTURE_USAGE_COLOR_ATTACHMENT;
	CHECK(RalMetal_CoreCreate(&info,&core,&coreReceipt));
	CHECK(RenderSubmission_Init(&frontend,1u));
	CHECK(RalMetal_PresentCreate(core,&coreReceipt,&create,2u,&present,&layer));
	qhandle_t materials[9]; char names[9][64];
	for(unsigned i=0;i<9;++i) {
		snprintf(names[i],sizeof(names[i]),"test/additive-stage-%u",i);
		const byte color[4]={(byte)(20+i*3),(byte)(45-i*3),(byte)(70-i*5),255};
		materials[i]=RenderSubmission_RegisterMaterialImage(&frontend,RENDER_ASSET_MATERIAL,
			names[i],qtrue,color,1,1); CHECK(materials[i]>0);
		CHECK(RenderSubmission_SetMaterialSort(&frontend,materials[i],i ? 8.0f+i*.01f : 3.0f));
		CHECK(RenderSubmission_SetMaterialCullMode(&frontend,materials[i],RENDER_CULL_NONE));
	}
	mapFile_t world={}; dsurface_t surfaces[9]={}; dshader_t shaders[9]={};
	drawVert_t vertices[36]={}; int indices[54]={};
	strcpy(world.name,"maps/additive-stages.bsp"); world.numShaders=9; world.shaders=shaders;
	world.surfaces=surfaces; world.numDrawVerts=36; world.drawVerts=vertices;
	world.numDrawIndexes=54; world.drawIndexes=indices;
	const float yz[4][2]={{32,32},{-32,32},{-32,-32},{32,-32}};
	const int quad[6]={0,1,2,0,2,3};
	for(unsigned i=0;i<9;++i) {
		strcpy(shaders[i].shader,names[i]); surfaces[i].shaderNum=i;
		surfaces[i].surfaceType=MST_PLANAR; surfaces[i].firstVert=i*4;
		surfaces[i].numVerts=4; surfaces[i].firstIndex=i*6; surfaces[i].numIndexes=6;
		surfaces[i].lightmapNum=-1; memcpy(indices+i*6,quad,sizeof(quad));
		for(unsigned v=0;v<4;++v) {
			vertices[i*4+v].xyz[0]=i ? 8 : 12;
			vertices[i*4+v].xyz[1]=yz[v][0]; vertices[i*4+v].xyz[2]=yz[v][1];
			vertices[i*4+v].normal[0]=-1; memset(vertices[i*4+v].color.rgba,255,4);
		}
	}
	refdef_t view={}; view.width=view.height=16; view.fov_x=view.fov_y=90;
	view.viewaxis[0][0]=view.viewaxis[1][1]=view.viewaxis[2][2]=1;
	const float black[4]={0,0,0,1}; const float alphas[4]={0,64/255.0f,127/255.0f,1};
	uint64_t generation=4; unsigned cases=0;
	for(unsigned model=0;model<2;++model) for(unsigned count : {1u,2u,3u,8u})
	for(unsigned pattern=0;pattern<3;++pattern) for(unsigned alpha=0;alpha<4;++alpha) {
		renderMaterialStageSnapshot_t stages[8]={};
		for(unsigned s=0;s<count;++s) {
			auto &stage=stages[s]; stage.material=materials[s+1];
			stage.scaleScroll[0]=stage.scaleScroll[1]=1;
			stage.rgbGen=RAL_MATERIAL_COLOR_IDENTITY; stage.alphaGen=RAL_MATERIAL_ALPHA_CONST;
			stage.constantColor[3]=alphas[(alpha+s)%4];
			stage.sourceBlend=pattern==1 || (pattern==2 && (s&1))
				? RENDER_MATERIAL_BLEND_ONE : RENDER_MATERIAL_BLEND_SRC_ALPHA;
			stage.destinationBlend=RENDER_MATERIAL_BLEND_ONE;
		}
		std::vector<byte> expected;
		for(unsigned fused=0;fused<2;++fused) {
			for(unsigned s=0;s<count;++s) {
				CHECK(RenderSubmission_SetMaterialStages(&frontend,materials[s+1],stages+s,1));
				CHECK(RenderSubmission_SetMaterialRasterPolicy(&frontend,materials[s+1],
					stages[s].sourceBlend==RENDER_MATERIAL_BLEND_ONE ? RENDER_ALPHA_ADDITIVE
						: RENDER_ALPHA_ALPHA_ADDITIVE,0,qfalse));
			}
			if(fused) CHECK(RenderSubmission_SetMaterialStages(&frontend,materials[1],stages,count));
			world.numSurfaces=fused ? 2 : count+1; world.checksum=(int)generation;
			CHECK(RenderSubmission_LoadWorld(&frontend,&world,0));
			CHECK(RenderSubmission_BeginFrame(&frontend,generation));
			view.rdflags=model ? RDF_NOWORLDMODEL : 0;
			if(model) for(unsigned i=0;i<(unsigned)world.numSurfaces;++i) {
				char name[64]; snprintf(name,sizeof(name),"*additive-stage-%u",i);
				refEntity_t entity={}; entity.reType=RT_MODEL;
				entity.hModel=RenderSubmission_RegisterInlineModel(&frontend,name,i,1);
				CHECK(entity.hModel>0); entity.customShader=materials[i];
				entity.axis[0][0]=entity.axis[1][1]=entity.axis[2][2]=1;
				memset(entity.shader.rgba,255,4); CHECK(RenderSubmission_AddEntity(&frontend,&entity,nullptr));
			}
			CHECK(RenderSubmission_RenderScene(&frontend,&view,0));
			CHECK(RalMetal_PresentAcquire(present,&coreReceipt,&layer,generation-1,&drawable));
			CHECK(RalMetal_PresentRequestCapture(present));
			CHECK(RalMetal_PresentClearAndSubmit(present,&coreReceipt,&layer,&drawable,
				&frontend,black,generation,&receipt));
			CHECK(model ? receipt.loweredEntityIndexCount==world.numSurfaces*6u
				: receipt.loweredWorldIndexCount==world.numSurfaces*6u);
			uint32_t width=0,height=0; const byte *pixels=RalMetal_PresentCaptureRgb(present,&width,&height);
			CHECK(pixels && width==16 && height==16);
			if(!fused) { expected.assign(pixels,pixels+width*height*3);
				CHECK(*std::max_element(expected.begin(),expected.end())>0); }
			else { int maxError=0;
				for(unsigned p=0;p<expected.size();++p) maxError=std::max(maxError,abs((int)pixels[p]-expected[p]));
				fprintf(stderr,"additive-stage model=%u stages=%u pattern=%u alpha=%u maxError=%d\n",
					model,count,pattern,alpha,maxError); CHECK(maxError<=1); ++cases;
			}
			CHECK(RenderSubmission_EndFrame(&frontend,generation,&submission)); generation+=2;
		}
	}
	fprintf(stderr,"additive-stage %u GPU cases passed\n",cases);
	RalMetal_PresentDestroy(present); RenderSubmission_Reset(&frontend); RalMetal_CoreDestroy(core);
	return 0;
}

// Vertex alpha is authored per vertex, quantized before interpolation. Use
// independently baked vertex alpha as the reference, never the GPU formula.
static int MaterialVertexAlphaPixels() {
	ralMetalCoreCreateInfo_t info={1u}; ralMetalCore_t *core=NULL;
	ralMetalCoreReceipt_t coreReceipt; ralMetalPresent_t *present=NULL;
	ralMetalPresentLayerReceipt_t layer; ralMetalDrawableReceipt_t drawable;
	ralMetalPresentReceipt_t receipt;
	auto frontendStorage = std::make_unique<renderSubmissionState_t>();
	renderSubmissionState_t &frontend = *frontendStorage;
	renderSubmissionReceipt_t submission;
	ralSurfaceFormat_t format={RAL_FORMAT_B8G8R8A8_UNORM,RAL_COLORSPACE_SRGB_NONLINEAR};
	ralPresentPreference_t fifo={RAL_PRESENT_FIFO,3u,3u};
	ralSwapchainCreateInfo_t create={};
	create.desiredWidth=create.desiredHeight=16; // Offscreen layer, no window.
	create.formatPreferences=&format; create.formatPreferenceCount=1;
	create.presentPreferences=&fifo; create.presentPreferenceCount=1;
	create.requiredUsage=RAL_TEXTURE_USAGE_COLOR_ATTACHMENT;
	CHECK(RalMetal_CoreCreate(&info,&core,&coreReceipt));
	CHECK(RenderSubmission_Init(&frontend,1u));
	CHECK(RalMetal_PresentCreate(core,&coreReceipt,&create,2u,&present,&layer));
	const byte white[4]={255,255,255,255}; const float black[4]={0,0,0,1};
	qhandle_t material=RenderSubmission_RegisterMaterialImage(&frontend,
		RENDER_ASSET_MATERIAL,"test/portal-vertex",qtrue,white,1,1);
	CHECK(material>0);
	CHECK(RenderSubmission_SetMaterialRasterPolicy(&frontend,material,RENDER_ALPHA_BLEND,0,qfalse));
	mapFile_t world={}; dsurface_t surface={}; dshader_t shader={};
	drawVert_t vertices[4]={}; int indices[6]={0,1,2,0,2,3};
	strcpy(world.name,"maps/portal-vertex.bsp"); strcpy(shader.shader,"test/portal-vertex");
	world.numShaders=1; world.shaders=&shader; world.numSurfaces=1; world.surfaces=&surface;
	world.numDrawVerts=4; world.drawVerts=vertices; world.numDrawIndexes=6; world.drawIndexes=indices;
	surface.surfaceType=MST_PLANAR; surface.numVerts=4; surface.numIndexes=6; surface.lightmapNum=-1;
	const float points[4][3]={{8,12,14},{12,-16,14},{12,-16,-10},{8,12,-10}};
	for(unsigned v=0;v<4;++v) {
		memcpy(vertices[v].xyz,points[v],sizeof(points[v])); vertices[v].normal[0]=-1;
		memset(vertices[v].color.rgba,255,4);
	}
	uint64_t generation=4;
for(unsigned model=0;model<3;++model) for(unsigned camera=0;camera<2;++camera)
	for(unsigned variant=0;variant<6;++variant) for(unsigned slot=0;slot<8;++slot) {
		const bool specular=variant>=3;
		const float ranges[3]={16.0f,37.5f,96.0f}; const float range=ranges[variant%3];
		// BSP ingestion normalizes authored normals. Use exact unit normals so
		// this fixture isolates the generator, not that separate loader policy.
		const float normals[3][3]={{-1,0,0},{0,-1,0},{0,0,1}};
		refEntity_t entity={};
		entity.axis[0][0]=entity.axis[1][1]=entity.axis[2][2]=1;
		if(model==2) {
			entity.origin[1]=4; entity.origin[2]=-2;
			entity.axis[1][1]=entity.axis[2][2]=0;
			entity.axis[1][2]=1; entity.axis[2][1]=-1;
		}
		refdef_t view={}; view.width=view.height=16; view.fov_x=view.fov_y=90;
		view.viewaxis[0][0]=view.viewaxis[1][1]=view.viewaxis[2][2]=1;
		view.vieworg[1]=camera ? 2 : 0; view.vieworg[2]=camera ? -1 : 0;
		view.rdflags=model ? RDF_NOWORLDMODEL : 0;
		vec3_t eye; VectorCopy(view.vieworg,eye);
		if(model) {
			vec3_t delta; VectorSubtract(view.vieworg,entity.origin,delta);
			for(unsigned axis=0;axis<3;++axis) eye[axis]=DotProduct(delta,entity.axis[axis]);
		}
		for(unsigned v=0;v<4;++v) {
			VectorCopy(normals[specular ? variant%3 : 0],vertices[v].normal);
			vec3_t delta; VectorSubtract(points[v],view.vieworg,delta);
			vertices[v].color.rgba[3]=specular ? LegacySpecularAlpha(points[v],vertices[v].normal,eye)
				: (byte)(255.0f*fminf(1.0f,VectorLength(delta)/range));
		}
		world.checksum=0x227800+model*100+camera*10+slot;
		CHECK(RenderSubmission_LoadWorld(&frontend,&world,0));
		char modelName[64]; snprintf(modelName,sizeof(modelName),"*portal-%llu",generation);
		qhandle_t mesh=model ? RenderSubmission_RegisterInlineModel(&frontend,modelName,0,1) : 0;
		CHECK(!model || mesh>0);
		std::vector<byte> expected;
		for(unsigned route=0;route<2;++route) {
			renderMaterialStageSnapshot_t stages[8]={};
			for(unsigned i=0;i<=slot;++i) {
				stages[i].material=material; stages[i].scaleScroll[0]=stages[i].scaleScroll[1]=1;
				stages[i].rgbGen=RAL_MATERIAL_COLOR_CONST;
				stages[i].alphaGen=RAL_MATERIAL_ALPHA_CONST;
				stages[i].sourceBlend=RENDER_MATERIAL_BLEND_SRC_ALPHA;
				stages[i].destinationBlend=RENDER_MATERIAL_BLEND_ONE_MINUS_SRC_ALPHA;
			}
			stages[slot].rgbGen=RAL_MATERIAL_COLOR_IDENTITY;
stages[slot].alphaGen=route ? (specular ? RAL_MATERIAL_ALPHA_LIGHTING_SPECULAR
				: RAL_MATERIAL_ALPHA_PORTAL) : RAL_MATERIAL_ALPHA_VERTEX;
			stages[slot].portalRange=range;
			CHECK(RenderSubmission_SetMaterialStages(&frontend,material,stages,slot+1));
			CHECK(RenderSubmission_BeginFrame(&frontend,generation));
			if(model) {
				entity.reType=RT_MODEL; entity.hModel=mesh;
				entity.customShader=material; memset(entity.shader.rgba,255,4);
				CHECK(RenderSubmission_AddEntity(&frontend,&entity,NULL));
			}
			CHECK(RenderSubmission_RenderScene(&frontend,&view,0));
			CHECK(RalMetal_PresentAcquire(present,&coreReceipt,&layer,generation-1,&drawable));
			CHECK(RalMetal_PresentRequestCapture(present));
			CHECK(RalMetal_PresentClearAndSubmit(present,&coreReceipt,&layer,&drawable,
				&frontend,black,generation,&receipt));
			CHECK(model ? receipt.loweredEntityIndexCount==6 && receipt.loweredWorldIndexCount==0
				: receipt.loweredWorldIndexCount==6);
			uint32_t width=0,height=0;
			const byte *pixels=RalMetal_PresentCaptureRgb(present,&width,&height);
			CHECK(pixels && width==16 && height==16);
			if(!route) {
				expected.assign(pixels,pixels+width*height*3);
CHECK(specular || *std::max_element(expected.begin(),expected.end())>0);
			} else {
				int maxError=0;
				for(unsigned p=0;p<expected.size();++p)
					maxError=std::max(maxError,abs((int)expected[p]-pixels[p]));
fprintf(stderr,"vertex-alpha specular=%u model=%u camera=%u variant=%u slot=%u maxError=%d\n",
					specular,model,camera,variant,slot,maxError);
				CHECK(maxError<=1);
			}
			CHECK(RenderSubmission_EndFrame(&frontend,generation,&submission)); generation+=2;
		}
	}
	RalMetal_PresentDestroy(present); RenderSubmission_Reset(&frontend); RalMetal_CoreDestroy(core);
	return 0;
}


static int ParticlePixelLifetime( qboolean lateAcquire ) {
	ralMetalCoreCreateInfo_t info = { 1u };
	ralMetalCore_t *core = NULL;
	ralMetalCoreReceipt_t coreReceipt;
	ralMetalPresent_t *present = NULL;
	ralMetalPresentLayerReceipt_t layer;
	ralMetalDrawableReceipt_t drawable;
	ralMetalPresentReceipt_t receipt;
	auto frontendStorage = std::make_unique<renderSubmissionState_t>();
	renderSubmissionState_t &frontend = *frontendStorage;
	renderSubmissionReceipt_t submission;
	ralSurfaceFormat_t format = {
		RAL_FORMAT_B8G8R8A8_UNORM, RAL_COLORSPACE_SRGB_NONLINEAR };
	ralPresentPreference_t fifo = { RAL_PRESENT_FIFO, 3u, 3u };
	ralSwapchainCreateInfo_t create = {};
	refdef_t view = {};
	particleClass_t particle = {};
	emitterDesc_t emitter = {};
	const byte white[4] = { 255u, 255u, 255u, 255u };
	const float black[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
	CHECK( RalMetal_CoreCreate( &info, &core, &coreReceipt ) );
	CHECK( RenderSubmission_Init( &frontend, 1u ) );
	particle.shader = RenderSubmission_RegisterMaterialImage( &frontend,
		RENDER_ASSET_MATERIAL, "test/particle-white", qtrue, white, 1u, 1u );
	CHECK( particle.shader > 0 );
	particle.paletteCount = 1;
	particle.colorPalette[0][1] = particle.colorPalette[0][3] = 1.0f;
	for ( unsigned channel = 0; channel < 4; ++channel )
		particle.colorEndMult[channel] = 1.0f;
	particle.lifetimeMean = 1.0f;
	particle.sizeStart = particle.sizeEnd = 2000.0f;
	CHECK( RenderSubmission_RegisterParticleClass( &frontend, 1, &particle ) );
	emitter.cls = 1; emitter.count = 1; emitter.origin[0] = 8000.0f;
	emitter.axis[0] = 1.0f;
	for ( unsigned channel = 0; channel < 4; ++channel )
		emitter.colorTint[channel] = 1.0f;
	create.desiredWidth = create.desiredHeight = 16u; // offscreen layer, no window
	create.formatPreferences = &format; create.formatPreferenceCount = 1u;
	create.presentPreferences = &fifo; create.presentPreferenceCount = 1u;
	create.requiredUsage = RAL_TEXTURE_USAGE_COLOR_ATTACHMENT;
	CHECK( RalMetal_PresentCreate( core, &coreReceipt, &create, 2u,
		&present, &layer ) );
	/* Registration publishes cooked factors after preparing texture resources.
	 * Neither that revision nor its reset may replace the native texture. */
	{
		ralTextureCacheKey_t key;
		ralMaterialRuntimeResource_t before, after, invalid;
		renderTextureSnapshot_t snapshot, changed;
		renderMaterialFactorPublicationEntry_t factors = {};
		CHECK( Ral_MaterialRuntimeTextureKeyBuild( 91u, 1u, white, 1u, 1u,
			qtrue, qtrue, RAL_TEXTURE_RESIDENCY_PINNED, &key ) );
		CHECK( RenderSubmission_SetMaterialTextureCacheKey( &frontend,
			particle.shader, &key ) );
		CHECK( RalMetal_PresentPrepareMaterialResources( present, &frontend ) );
		CHECK( RalMetal_PresentResolveMaterialRuntimeResource( present,
			key.keyHash, &before ) );
		/* Different material/sampler names may refer to one canonical image.
		 * Publishing the second handle must not create an ambiguous GPU owner. */
		qhandle_t shared = RenderSubmission_RegisterMaterialImage( &frontend,
			RENDER_ASSET_MATERIAL, "fixture/shared-cooked-image", qfalse,
			white, 1u, 1u );
		CHECK( shared > 0 && shared != particle.shader );
		CHECK( RenderSubmission_SetMaterialTextureCacheKey( &frontend, shared, &key ) );
		CHECK( RalMetal_PresentPrepareMaterialResources( present, &frontend ) );
		CHECK( RalMetal_PresentResolveMaterialRuntimeResource( present,
			key.keyHash, &after ) );
		CHECK( Ral_TextureResourceReceiptExact( &before.resource, &after.resource ) );
		CHECK( RenderSubmission_TextureSnapshot( &frontend, particle.shader,
			&snapshot ) );
		CHECK( RenderSubmission_TextureResourceMatches( &snapshot, &before ) );
		changed = snapshot; changed.textureCacheKeyReady = qfalse;
		CHECK( !RenderSubmission_TextureResourceMatches( &changed, &before ) );
		changed = snapshot; changed.textureCacheKeyHash++;
		CHECK( !RenderSubmission_TextureResourceMatches( &changed, &before ) );
		changed = snapshot; changed.width++;
		CHECK( !RenderSubmission_TextureResourceMatches( &changed, &before ) );
		changed = snapshot; changed.srgb = qfalse;
		CHECK( !RenderSubmission_TextureResourceMatches( &changed, &before ) );
		invalid = before; invalid.resource.mipLevels++;
		CHECK( !RenderSubmission_TextureResourceMatches( &snapshot, &invalid ) );
		invalid = before; invalid.resource.ready = qfalse;
		CHECK( !RenderSubmission_TextureResourceMatches( &snapshot, &invalid ) );
		factors.material = particle.shader;
		for ( unsigned channel = 0u; channel < 4u; ++channel )
			factors.factors.baseColor[channel] = 1.0f;
		factors.factors.roughness = 0.5f;
		factors.factors.normalScale = factors.factors.occlusionStrength = 1.0f;
		CHECK( RenderSubmission_PublishMaterialFactors( &frontend, 1u,
			&factors, 1u ) );
		CHECK( RenderSubmission_TextureSnapshot( &frontend, particle.shader,
			&changed ) && changed.generation != snapshot.generation );
		CHECK( RenderSubmission_TextureResourceMatches( &changed, &before ) );
		for ( unsigned repeat = 0u; repeat < 3u; ++repeat ) {
			CHECK( RalMetal_PresentPrepareMaterialResources( present, &frontend ) );
			CHECK( RalMetal_PresentResolveMaterialRuntimeResource( present,
				key.keyHash, &after ) );
			CHECK( Ral_TextureResourceReceiptExact( &before.resource, &after.resource ) );
		}
		CHECK( RenderSubmission_PublishMaterialFactors( &frontend, 0u, NULL, 0u ) );
		CHECK( RalMetal_PresentPrepareMaterialResources( present, &frontend ) );
		CHECK( RalMetal_PresentResolveMaterialRuntimeResource( present,
			key.keyHash, &after ) );
		CHECK( Ral_TextureResourceReceiptExact( &before.resource, &after.resource ) );
	}
	view.width = view.height = 16;
	view.fov_x = view.fov_y = 90.0f;
	view.viewaxis[0][0] = view.viewaxis[1][1] = view.viewaxis[2][2] = 1.0f;
	view.rdflags = RDF_NOWORLDMODEL;
	/* Cover both ping-pong sides repeatedly, then expiration and a fresh
	 * spawn after a timeline rewind. A first-frame-only check misses pool
	 * publication errors and stale particles crossing a level reset. */
	const int times[] = { 100, 116, 132, 148, 164, 180, 1200, 50, 66, 1200 };
	{
	wiredParticleUploadProbe uploads( RalMetal_CoreNativeDevice( core ) );
	for ( unsigned step = 0; step < sizeof( times ) / sizeof( times[0] ); ++step ) {
		const bool spawn = step == 0 || step == 7;
		const bool visible = step != 6 && step != 9;
		const uint64_t generation = 4u + step * 2u;
		view.time = times[step];
		CHECK( RenderSubmission_BeginFrame( &frontend, generation ) );
		CHECK( RenderSubmission_RenderScene( &frontend, &view, 0 ) );
		if ( spawn )
			CHECK( RenderSubmission_AddEffectEmitter( &frontend, &emitter ) );
		CHECK( RalMetal_PresentRequestCapture( present ) );
		if ( lateAcquire ) {
			CHECK( RalMetal_PresentRenderAndSubmit( present, &coreReceipt, &layer,
				&frontend, black, generation - 1u, generation, &drawable,
				&receipt, NULL ) == RAL_METAL_SUBMIT_PRESENTED );
			CHECK( drawable.acquired && drawable.drawableIdentity == receipt.drawableIdentity
				&& drawable.textureIdentity == receipt.textureIdentity );
		} else {
			CHECK( RalMetal_PresentAcquire( present, &coreReceipt, &layer,
				generation - 1u, &drawable ) );
			CHECK( RalMetal_PresentClearAndSubmit( present, &coreReceipt, &layer,
				&drawable, &frontend, black, generation, &receipt ) );
		}
		CHECK( receipt.readbackByteCount == 4u );
		CHECK( receipt.readbackBytes[0] < 8u && receipt.readbackBytes[2] < 8u );
		CHECK( visible ? receipt.readbackBytes[1] > 240u
			: receipt.readbackBytes[1] < 8u );
		CHECK( receipt.effectEmitterDispatchCount == ( spawn ? 1u : 0u ) );
		CHECK( receipt.effectPrimitiveDroppedCount == 0u );
		CHECK( RenderSubmission_EndFrame( &frontend, generation, &submission ) );
		uploads.wired_collect();
		for ( unsigned input = 0u; input < 7u; ++input )
			CHECK( uploads.allocations[input] == 1u );
	}
	fprintf( stderr, "particle uploads: late=%d ten frames, seven initial allocations, zero replacements; lifetime/rewind pixels passed\n", lateAcquire );
	}
	/* Every single-stage native FX blend must match its direct-texture path.
	 * Cover smoke, additive fire and alpha-additive glows with partial texture
	 * AND vertex alpha: applying either alpha twice must change GPU pixels. */
	CHECK( RalMetal_PresentSetToneMap( present, 0u, 1.0f, 1.6f, 0.977f, 0.18f, 0.267f, 8.0f ) );
	const renderAlphaMode_t modes[] = {
		RENDER_ALPHA_BLEND, RENDER_ALPHA_ADDITIVE, RENDER_ALPHA_ALPHA_ADDITIVE };
	const char *names[] = { "test/smoke", "test/fire", "test/glow" };
	const byte translucent[4] = { 255u, 224u, 192u, 128u };
	for ( unsigned blend = 0; blend < 3; ++blend ) {
		const qhandle_t material = RenderSubmission_RegisterMaterialImage( &frontend,
			RENDER_ASSET_MATERIAL, names[blend], qtrue, translucent, 1u, 1u );
		CHECK( material > 0 );
		renderMaterialStageSnapshot_t stage = {};
		stage.material = RenderSubmission_RegisterStageTexture( &frontend,
			RENDER_ASSET_MATERIAL, names[blend], qtrue, translucent, 1u, 1u );
		CHECK( stage.material > 0 );
		stage.sourceBlend = blend == 1 ? RENDER_MATERIAL_BLEND_ONE
			: RENDER_MATERIAL_BLEND_SRC_ALPHA;
		stage.destinationBlend = blend == 0
			? RENDER_MATERIAL_BLEND_ONE_MINUS_SRC_ALPHA : RENDER_MATERIAL_BLEND_ONE;
		stage.alphaGen = RAL_MATERIAL_ALPHA_VERTEX;
		stage.scaleScroll[0] = stage.scaleScroll[1] = 1.0f;
		for ( unsigned c = 0; c < 4; ++c ) stage.constantColor[c] = 1.0f;
		CHECK( RenderSubmission_SetMaterialRasterPolicy( &frontend, material,
			modes[blend], 0.0f, qfalse ) );
		byte referencePixel[4] = {};
		/* RT_SPRITE is WiredFX's animated sprite route. Also exercise the
		 * explicit RAL billboard route used by other effect consumers. */
		for ( unsigned authored = 0; authored < ( blend == 2 ? 5u : 3u ); ++authored ) {
			const uint64_t generation = 30u + blend * 6u + authored * 2u;
			refEntity_t smoke = {};
			if ( authored ) CHECK( RenderSubmission_SetMaterialStages( &frontend,
				material, &stage, 1u ) );
			smoke.reType = RT_SPRITE; smoke.customShader = material;
			smoke.origin[0] = 8.0f; smoke.radius = 4.0f;
			smoke.shader.rgba[0] = smoke.shader.rgba[1] = smoke.shader.rgba[2] = 255;
			smoke.shader.rgba[3] = 51; // authored shotgun smoke alpha 0.2
			if ( authored == 4 ) smoke.shader.rgba[3] = 0;
			view.rdflags = 0; view.time = 1300 + blend * 48 + authored * 16;
			CHECK( RenderSubmission_BeginFrame( &frontend, generation ) );
			const bool flaggedSprite = blend == 2 && ( authored == 0 || authored == 4 );
			if ( authored == 3 ) {
				ribbonPoint_t points[2] = {};
				for ( unsigned point = 0; point < 2; ++point ) {
					points[point].pos[0] = smoke.origin[0];
					points[point].pos[2] = point ? 4.0f : -4.0f;
					points[point].width = 4.0f;
					for ( unsigned c = 0; c < 4; ++c )
						points[point].rgba[c] = smoke.shader.rgba[c] / 255.0f;
				}
				ribbonDesc_t ribbon = {};
				ribbon.points = points; ribbon.numPoints = 2;
				ribbon.shader = material; ribbon.flags = PRIM_FLAG_ADDITIVE;
				CHECK( RenderSubmission_AddEffectRibbon( &frontend, &ribbon ) );
			} else if ( authored == 2 || flaggedSprite ) {
				spriteDesc_t sprite = {};
				sprite.origin[0] = smoke.origin[0];
				sprite.radius = smoke.radius; sprite.shader = material;
				sprite.flags = flaggedSprite ? PRIM_FLAG_ADDITIVE : 0;
				for ( unsigned c = 0; c < 4; ++c )
					sprite.rgba[c] = smoke.shader.rgba[c] / 255.0f;
				CHECK( RenderSubmission_AddEffectSprite( &frontend, &sprite ) );
			} else {
				CHECK( RenderSubmission_AddEntity( &frontend, &smoke, NULL ) );
			}
			CHECK( RenderSubmission_RenderScene( &frontend, &view, 0 ) );
			CHECK( RalMetal_PresentAcquire( present, &coreReceipt, &layer,
				generation - 1u, &drawable ) );
			CHECK( RalMetal_PresentRequestCapture( present ) );
			CHECK( RalMetal_PresentClearAndSubmit( present, &coreReceipt, &layer,
				&drawable, &frontend, black, generation, &receipt ) );
			CHECK( receipt.readbackByteCount == 4u );
			CHECK( receipt.loweredEffectSpriteCount == ( authored == 2 || flaggedSprite ? 1u : 0u ) );
			CHECK( receipt.loweredEffectRibbonCount == ( authored == 3 ? 1u : 0u ) );
			CHECK( receipt.effectPrimitiveDroppedCount == 0u );
			if ( !authored ) {
				memcpy( referencePixel, receipt.readbackBytes, 4u );
				CHECK( referencePixel[0] > 8u );
				/* Additive primitive flags mean SRC_ALPHA/ONE, not ONE/ONE.
				 * White red texel, alpha128 and descriptor alpha51 => red26. */
				if ( flaggedSprite ) {
					fprintf( stderr, "flagged sprite red=%u expected=26\n", referencePixel[2] );
					CHECK( abs( (int)referencePixel[2] - 26 ) <= 1 );
				}
			} else for ( unsigned c = 0; c < 3; ++c ) {
				fprintf( stderr, "FX blend=%u route=%u channel=%u direct=%u authored=%u\n",
					blend, authored, c, referencePixel[c], receipt.readbackBytes[c] );
				const int expected = authored == 4 ? 0 : referencePixel[c];
				CHECK( abs( expected - receipt.readbackBytes[c] ) <= 1 );
			}
			CHECK( RenderSubmission_EndFrame( &frontend, generation, &submission ) );
		}
	}
	/* Primitive beams use texture * linear descriptor RGB * coverage, not
	 * the surface material's blend/vertex-generator program. In particular,
	 * low-alpha lens ghosts must not square descriptor opacity. */
	CHECK( RalMetal_PresentSetToneMap( present, 0u, 1.0f, 1.6f, 0.977f, 0.18f, 0.267f, 8.0f ) );
    for ( unsigned route = 0u; route < 4u; ++route ) {
		char name[64];
		snprintf( name, sizeof( name ), "test/beam-coverage-%u", route );
		const byte texel[4] = { 255u, 255u, 255u, 128u };
		const qhandle_t material = RenderSubmission_RegisterMaterialImage( &frontend,
			RENDER_ASSET_MATERIAL, name, qtrue, texel, 1u, 1u );
		CHECK( material > 0 );
		if ( route ) {
			renderMaterialStageSnapshot_t stage = {};
			stage.material = RenderSubmission_RegisterStageTexture( &frontend,
				RENDER_ASSET_MATERIAL, name, qtrue, texel, 1u, 1u );
			CHECK( stage.material > 0 );
			stage.sourceBlend = route == 1u ? RENDER_MATERIAL_BLEND_ONE
				: RENDER_MATERIAL_BLEND_SRC_ALPHA;
			stage.destinationBlend = RENDER_MATERIAL_BLEND_ONE;
			stage.rgbGen = RAL_MATERIAL_COLOR_VERTEX;
			stage.alphaGen = RAL_MATERIAL_ALPHA_VERTEX;
			stage.scaleScroll[0] = stage.scaleScroll[1] = 1.0f;
			for ( unsigned c = 0u; c < 4u; ++c ) stage.constantColor[c] = 1.0f;
			renderMaterialStageSnapshot_t stages[2] = { stage, stage };
			CHECK( RenderSubmission_SetMaterialStages( &frontend, material,
				stages, route == 3u ? 2u : 1u ) );
		}
		for ( unsigned opacity = 0u; opacity < 2u; ++opacity ) {
			const uint64_t generation = 52u + route * 4u + opacity * 2u;
			beamDesc_t beam = {};
			beam.start[0] = beam.end[0] = 8.0f;
			beam.start[1] = -4.0f; beam.end[1] = 4.0f;
			beam.startWidth = beam.endWidth = 4.0f;
			beam.startColor[0] = beam.endColor[0] = 0.5f;
			beam.startColor[1] = beam.endColor[1] = 0.25f;
			beam.startColor[2] = beam.endColor[2] = 0.75f;
			beam.startColor[3] = beam.endColor[3] = opacity ? 0.2f : 0.1f;
			beam.shader = material; beam.axialCopies = 1;
			beam.startEntityNum = beam.endEntityNum = -1;
			beam.flags = PRIM_FLAG_ADDITIVE;
			view.rdflags = 0; view.time = 1500 + route * 32 + opacity * 16;
			CHECK( RenderSubmission_BeginFrame( &frontend, generation ) );
			CHECK( RenderSubmission_RenderScene( &frontend, &view, 0 ) );
			CHECK( RenderSubmission_AddEffectBeam( &frontend, &beam ) );
			CHECK( RalMetal_PresentAcquire( present, &coreReceipt, &layer,
				generation - 1u, &drawable ) );
			CHECK( RalMetal_PresentRequestCapture( present ) );
			CHECK( RalMetal_PresentClearAndSubmit( present, &coreReceipt, &layer,
				&drawable, &frontend, black, generation, &receipt ) );
			CHECK( receipt.readbackByteCount == 4u );
			for ( unsigned c = 0u; c < 3u; ++c ) {
				const float srgb = beam.startColor[2u - c]; // BGRA readback
				const float linear = powf( ( srgb + 0.055f ) / 1.055f, 2.4f );
				const int expected = (int)( linear * 128.0f * beam.startColor[3]
					* ( route == 3u ? 2.0f : 1.0f ) + 0.5f );
				fprintf( stderr, "FX beam route=%u opacity=%u channel=%u expected=%d actual=%u\n",
					route, opacity, c, expected, receipt.readbackBytes[c] );
				CHECK( abs( (int)receipt.readbackBytes[c] - expected ) <= 1 );
			}
			CHECK( receipt.effectPrimitiveDroppedCount == 0u );
			CHECK( RenderSubmission_EndFrame( &frontend, generation, &submission ) );
		}
	}
	/* A beam parallel to camera forward but offset from its eye ray must
	 * still have area. cross(axis, viewForward) incorrectly makes it edge-on. */
	{
		beamDesc_t beam = {};
		beam.start[0] = 4.0f; beam.end[0] = 12.0f;
		beam.start[1] = beam.end[1] = 2.0f;
		beam.startWidth = beam.endWidth = 1.0f;
		beam.startColor[1] = beam.endColor[1] = 1.0f;
		beam.startColor[3] = beam.endColor[3] = 1.0f;
		beam.shader = particle.shader; beam.axialCopies = 1;
		beam.startEntityNum = beam.endEntityNum = -1;
		beam.flags = PRIM_FLAG_ADDITIVE;
		view.time = 1700;
		CHECK( RenderSubmission_BeginFrame( &frontend, 68u ) );
		CHECK( RenderSubmission_RenderScene( &frontend, &view, 0 ) );
		CHECK( RenderSubmission_AddEffectBeam( &frontend, &beam ) );
		CHECK( RalMetal_PresentAcquire( present, &coreReceipt, &layer, 67u, &drawable ) );
		CHECK( RalMetal_PresentRequestCapture( present ) );
		CHECK( RalMetal_PresentClearAndSubmit( present, &coreReceipt, &layer,
			&drawable, &frontend, black, 68u, &receipt ) );
		uint32_t width, height, visible = 0u;
		const byte *rgb = RalMetal_PresentCaptureRgb( present, &width, &height );
		CHECK( rgb && width == 16u && height == 16u );
		for ( unsigned pixel = 0u; pixel < width * height; ++pixel )
			if ( rgb[pixel * 3u + 1u] > 200u ) ++visible;
		fprintf( stderr, "FX off-axis beam visible pixels=%u\n", visible );
		CHECK( visible >= 4u );
		CHECK( RenderSubmission_EndFrame( &frontend, 68u, &submission ) );
	}
	CHECK( RalMetal_PresentSetToneMap( present, 3u, 1.0f, 1.6f, 0.977f, 0.18f, 0.267f, 8.0f ) );
	/* Authored frame slots must advance on BOTH GPU ping-pong sides. A
	 * dispatch count or static white particle cannot catch missing flipbook
	 * textures, frozen frames, or sampling the prior frame's storage pool. */
	particle.frameCount = 3;
	particle.frameBlend = 0;
	for ( unsigned channel = 0; channel < 4; ++channel )
		particle.colorPalette[0][channel] = 1.0f;
	for ( unsigned frame = 0; frame < 3; ++frame ) {
		byte pixel[4] = { 0u, 0u, 0u, 255u };
		char name[64];
		pixel[frame] = 255u;
		snprintf( name, sizeof( name ), "test/particle-frame-%u", frame );
		particle.frameShaders[frame] = RenderSubmission_RegisterMaterialImage(
			&frontend, RENDER_ASSET_MATERIAL, name, qtrue, pixel, 1u, 1u );
		CHECK( particle.frameShaders[frame] > 0 );
	}
	CHECK( RenderSubmission_RegisterParticleClass( &frontend, 2, &particle ) );
	emitter.cls = 2;
	view.rdflags = RDF_NOWORLDMODEL;
	for ( unsigned step = 0; step < 22; ++step ) {
		const uint64_t generation = 70u + step * 2u;
		const unsigned expectedFrame = step < 7u ? 0u : step < 14u ? 1u : 2u;
		view.time = 2000 + step * 50;
		CHECK( RenderSubmission_BeginFrame( &frontend, generation ) );
		CHECK( RenderSubmission_RenderScene( &frontend, &view, 0 ) );
		if ( step == 0 )
			CHECK( RenderSubmission_AddEffectEmitter( &frontend, &emitter ) );
		CHECK( RalMetal_PresentAcquire( present, &coreReceipt, &layer,
			generation - 1u, &drawable ) );
		CHECK( RalMetal_PresentRequestCapture( present ) );
		CHECK( RalMetal_PresentClearAndSubmit( present, &coreReceipt, &layer,
			&drawable, &frontend, black, generation, &receipt ) );
		CHECK( receipt.readbackByteCount == 4u );
		fprintf( stderr, "FX animation step=%u frame=%u BGRA=%u,%u,%u,%u\n",
			step, expectedFrame, receipt.readbackBytes[0], receipt.readbackBytes[1],
			receipt.readbackBytes[2], receipt.readbackBytes[3] );
		for ( unsigned channel = 0; channel < 3; ++channel ) {
			/* Capture preserves the selected B8G8R8A8 attachment byte order. */
			const bool bright = step < 20u && channel == 2u - expectedFrame;
			/* Canonical particle.frag gives animated frames 0.70 linear
			 * energy at identity exposure; this no-world UNORM target stores
			 * it directly. Preserve that authored rule, not static white's 1. */
			const int expected = bright ? 179 : 0;
			CHECK( abs( (int)receipt.readbackBytes[channel] - expected ) <= 1 );
		}
		CHECK( receipt.effectEmitterDispatchCount == ( step == 0 ? 1u : 0u ) );
		CHECK( receipt.effectPrimitiveDroppedCount == 0u );
		CHECK( RenderSubmission_EndFrame( &frontend, generation, &submission ) );
	}
	/* A correct frame index is insufficient: authored interpolation must mix
	 * adjacent frames in linear light, keep the final frame clamped (not wrap
	 * to frame zero), and expire on both ping-pong sides. */
	particle.frameBlend = 1;
	CHECK( RenderSubmission_RegisterParticleClass( &frontend, 3, &particle ) );
	emitter.cls = 3;
	for ( unsigned step = 0; step < 22; ++step ) {
		const uint64_t generation = 120u + step * 2u;
		const float framePosition = step * 0.15f; // 50 ms / 1 s * 3 frames
		const unsigned frame0 = step < 7u ? 0u : step < 14u ? 1u : 2u;
		const unsigned frame1 = frame0 < 2u ? frame0 + 1u : 2u;
		const float blend = framePosition - (unsigned)framePosition;
		view.time = 4000 + step * 50;
		CHECK( RenderSubmission_BeginFrame( &frontend, generation ) );
		CHECK( RenderSubmission_RenderScene( &frontend, &view, 0 ) );
		if ( step == 0 )
			CHECK( RenderSubmission_AddEffectEmitter( &frontend, &emitter ) );
		CHECK( RalMetal_PresentAcquire( present, &coreReceipt, &layer,
			generation - 1u, &drawable ) );
		CHECK( RalMetal_PresentRequestCapture( present ) );
		CHECK( RalMetal_PresentClearAndSubmit( present, &coreReceipt, &layer,
			&drawable, &frontend, black, generation, &receipt ) );
		CHECK( receipt.readbackByteCount == 4u );
		for ( unsigned channel = 0; channel < 3; ++channel ) {
			const float weight = ( channel == 2u - frame0 ? 1.0f - blend : 0.0f )
				+ ( channel == 2u - frame1 ? blend : 0.0f );
			const int expected = step < 20u ? (int)( 255.0f * 0.70f * weight + 0.5f ) : 0;
			fprintf( stderr, "FX frame-blend step=%u channel=%u expected=%d actual=%u\n",
				step, channel, expected, receipt.readbackBytes[channel] );
			CHECK( abs( (int)receipt.readbackBytes[channel] - expected ) <= 1 );
		}
		CHECK( receipt.effectEmitterDispatchCount == ( step == 0 ? 1u : 0u ) );
		CHECK( receipt.effectPrimitiveDroppedCount == 0u );
		CHECK( RenderSubmission_EndFrame( &frontend, generation, &submission ) );
	}
	/* Compare real sampled pixels against independently calculated UVs. The
	 * reference draw uses only tcGen vector, never the ordered-op evaluator. */
	{
		struct TcCase {
			const char *name;
			int timeMs;
			float expected[2];
			unsigned count;
			renderMaterialTcMod_t ops[2];
		};
		std::vector<TcCase> cases = {
			{ "scroll-scale", 2000, { .9f, .2125f }, 2,
				{ { RAL_MATERIAL_TCMOD_SCROLL, { .125f, .0625f } },
				  { RAL_MATERIAL_TCMOD_SCALE, { 2.0f, .5f } } } },
			{ "scale-scroll", 2000, { .65f, .275f }, 2,
				{ { RAL_MATERIAL_TCMOD_SCALE, { 2.0f, .5f } },
				  { RAL_MATERIAL_TCMOD_SCROLL, { .125f, .0625f } } } },
			{ "shear-offset", 1000, { .375f, .45f }, 1,
				{ { RAL_MATERIAL_TCMOD_TRANSFORM, { 1, .5f, .25f, 1, .1f, .05f } } } },
			{ "rotate-direction", 1000, { .3f, .8f }, 1,
				{ { RAL_MATERIAL_TCMOD_ROTATE, { 90 } } } },
			{ "stretch-negative", 1000, { .8f, .7f }, 1,
				{ { RAL_MATERIAL_TCMOD_STRETCH, { RAL_MATERIAL_WAVE_SIN, -1, 0, 0, 0 } } } },
			{ "stretch-square", 1000, { -.1f, .1f }, 1,
				{ { RAL_MATERIAL_TCMOD_STRETCH, { RAL_MATERIAL_WAVE_SQUARE, 1, .5f, .75f, 0 } } } },
			/* Q3 casts the signed table coordinate toward zero before masking.
			 * Continuous fract/floor chooses the opposite square-wave half. */
			{ "square-negative-zero-boundary", 1000, { .3f, .36666667f }, 1,
				{ { RAL_MATERIAL_TCMOD_STRETCH, { RAL_MATERIAL_WAVE_SQUARE, 1, .5f, -.000244140625f, 0 } } } },
			{ "square-negative-half-boundary", 1000, { -.1f, .1f }, 1,
				{ { RAL_MATERIAL_TCMOD_STRETCH, { RAL_MATERIAL_WAVE_SQUARE, 1, .5f, -.500244140625f, 0 } } } },
			{ "stretch-triangle", 1000, { .35f, .4f }, 1,
				{ { RAL_MATERIAL_TCMOD_STRETCH, { RAL_MATERIAL_WAVE_TRIANGLE, 1, 1, .25f, 0 } } } },
			{ "stretch-saw", 1000, { .26f, .34f }, 1,
				{ { RAL_MATERIAL_TCMOD_STRETCH, { RAL_MATERIAL_WAVE_SAWTOOTH, 1, 1, .25f, 0 } } } },
			{ "stretch-inverse-saw", 1000, { .32857143f, .38571429f }, 1,
				{ { RAL_MATERIAL_TCMOD_STRETCH, { RAL_MATERIAL_WAVE_INVERSE_SAWTOOTH, 1, 1, .25f, 0 } } } },
			{ "turb-base-ignored", 1000, { .2f, .3f }, 1,
				{ { RAL_MATERIAL_TCMOD_TURBULENCE, { 99, 0, .25f, 1 } } } },
			/* Q3 evaluates at the four vertices, THEN interpolates. At
			 * x=256,z=+/-256 the S sine is zero at every corner; T is
			 * +/-0.25 at y=+/-256. Pixel (8,8) has y=-16, hence
			 * interpolated T=0.3-0.25*(16/256). Fragment sine is wrong. */
			{ "turb-vertex-interpolation", 1000, { .2f, .284375f }, 1,
				{ { RAL_MATERIAL_TCMOD_TURBULENCE, { 99, .25f, 0, 0 } } } }
		};
		/* Independent CPU table construction from Q3 R_Init, not the GPU's
		 * closed-form evaluator. Magnify the UV delta to expose sub-texel
		 * continuous-wave approximations without loosening the pixel gate. */
		float tables[5][1024] = {};
		for ( unsigned i = 0; i < 1024; ++i ) {
			tables[0][i] = (float)sin( DEG2RAD( i * 360.0f / 1024 ) + .0001f );
			tables[1][i] = i < 512 ? 1 : -1;
			tables[3][i] = i == 0 ? 1e-6f : (float)i / 1024;
			tables[4][i] = 1 - tables[3][i];
			if ( i < 256 ) tables[2][i] = i == 0 ? 1e-6f : (float)i / 256;
			else if ( i < 512 ) tables[2][i] = 1 - tables[2][i - 256];
			else tables[2][i] = -tables[2][i - 512];
		}
		const float phases[] = { 0, -.000244140625f, -.500244140625f,
			-.123456f, .123456f, .25f, .9999f };
		for ( unsigned function = 1; function <= 5; ++function ) {
			for ( float phase : phases ) {
				const unsigned index = (int64_t)(phase * 1024.0f) & 1023;
				const float wave = 1 + .5f * tables[function - 1][index];
				TcCase sample = { "wave-table-grid", 1000, { .5f, .5f }, 2, {} };
				sample.ops[0].type = RAL_MATERIAL_TCMOD_STRETCH;
				sample.ops[0].parameters[0] = (float)function;
				sample.ops[0].parameters[1] = 1;
				sample.ops[0].parameters[2] = .5f;
				sample.ops[0].parameters[3] = phase;
				sample.ops[1].type = RAL_MATERIAL_TCMOD_TRANSFORM;
				sample.ops[1].parameters[0] = sample.ops[1].parameters[3] = 64;
				sample.ops[1].parameters[4] = .5f - 64 * ((.2f - .5f) / wave + .5f);
				sample.ops[1].parameters[5] = .5f - 64 * ((.3f - .5f) / wave + .5f);
				cases.push_back( sample );
			}
		}
		/* Use the production Q3 CPU evaluator as the GPU pixel oracle. Noise
		 * alone uses (time + phase) * frequency, not phase + time*frequency.
		 * Negative cells, the 256-cell wrap and zero frequency must agree. */
		R_NoiseInit();
		const float noisePhases[] = { 0, .125f, -.125f, .9999f, -1.0001f,
			255.75f, -256.25f, 256.25f, 17.37f };
		const float noiseFrequencies[] = { 0, .5f, -1.5f };
		for ( float phase : noisePhases ) for ( float frequency : noiseFrequencies ) {
			const float wave = 1 + .5f * R_NoiseGet4f( 0, 0, 0, (1.25 + phase) * frequency );
			TcCase sample = { "noise-table-grid", 1250, { .5f, .5f }, 2, {} };
			sample.ops[0].type = RAL_MATERIAL_TCMOD_STRETCH;
			sample.ops[0].parameters[0] = RAL_MATERIAL_WAVE_NOISE;
			sample.ops[0].parameters[1] = 1;
			sample.ops[0].parameters[2] = .5f;
			sample.ops[0].parameters[3] = phase;
			sample.ops[0].parameters[4] = frequency;
			sample.ops[1].type = RAL_MATERIAL_TCMOD_TRANSFORM;
			sample.ops[1].parameters[0] = sample.ops[1].parameters[3] = 64;
			sample.ops[1].parameters[4] = .5f - 64 * ((.2f - .5f) / wave + .5f);
			sample.ops[1].parameters[5] = .5f - 64 * ((.3f - .5f) / wave + .5f);
			cases.push_back( sample );
		}
		const float speeds[] = { -.1f, .1f, 27.13f, -127.13f };
		for ( float speed : speeds ) {
			const int64_t index = (int64_t)(-(double)speed * (1024.0f / 360.0f));
			const float sine = tables[0][index & 1023];
			const float cosine = tables[0][(index + 256) & 1023];
			const float u = -.3f * cosine + .2f * sine + .5f;
			const float v = -.3f * sine - .2f * cosine + .5f;
			TcCase sample = { "rotate-table-grid", 1000, { .5f, .5f }, 2, {} };
			sample.ops[0].type = RAL_MATERIAL_TCMOD_ROTATE;
			sample.ops[0].parameters[0] = speed;
			sample.ops[1].type = RAL_MATERIAL_TCMOD_TRANSFORM;
			sample.ops[1].parameters[0] = sample.ops[1].parameters[3] = 64;
			sample.ops[1].parameters[4] = .5f - 64 * u;
			sample.ops[1].parameters[5] = .5f - 64 * v;
			cases.push_back( sample );
		}
		byte gradient[16u * 16u * 4u];
		for ( unsigned y = 0u; y < 16u; ++y ) for ( unsigned x = 0u; x < 16u; ++x ) {
			const unsigned index = ( y * 16u + x ) * 4u;
			gradient[index] = (byte)( x * 16u );
			gradient[index + 1u] = (byte)( y * 16u );
			gradient[index + 2u] = 64u; gradient[index + 3u] = 255u;
		}
		const qhandle_t material = RenderSubmission_RegisterMaterialImage( &frontend,
			RENDER_ASSET_MATERIAL, "test/ordered-uv", qtrue, gradient, 16u, 16u );
		CHECK( material > 0 );
		for ( unsigned c = 0u; c < cases.size(); ++c ) {
			byte expectedPixel[4] = {};
			for ( unsigned ordered = 0u; ordered < 2u; ++ordered ) {
				renderMaterialStageSnapshot_t stage = {};
				stage.material = material;
				stage.tcGen = RENDER_MATERIAL_TCGEN_VECTOR;
				stage.tcGenVectors[0][0] = ( ordered ? .2f : cases[c].expected[0] ) / 256.0f;
				stage.tcGenVectors[1][0] = ( ordered ? .3f : cases[c].expected[1] ) / 256.0f;
				stage.scaleScroll[0] = stage.scaleScroll[1] = 1.0f;
				stage.sourceBlend = RENDER_MATERIAL_BLEND_ONE;
				if ( ordered ) {
					stage.tcModCount = cases[c].count;
					memcpy( stage.tcMods, cases[c].ops, sizeof( cases[c].ops ) );
				}
				/* Exercise every packed varying pair, including stage seven.
				 * Earlier stages write zero; only the last contributes. */
				renderMaterialStageSnapshot_t stages[8] = {};
				const unsigned stageCount = ordered ? 1u + c % 8u : 1u;
				for ( unsigned s = 0u; s < stageCount; ++s ) {
					stages[s] = stage;
					if ( s + 1u < stageCount )
						stages[s].sourceBlend = RENDER_MATERIAL_BLEND_ZERO;
				}
				CHECK( RenderSubmission_SetMaterialStages( &frontend, material,
					stages, stageCount ) );
				refEntity_t surface = {};
				surface.reType = RT_SPRITE; surface.origin[0] = 256.0f;
				surface.radius = 256.0f; surface.customShader = material;
				memset( surface.shader.rgba, 255, sizeof( surface.shader.rgba ) );
				const uint64_t generation = 200u + c * 4u + ordered * 2u;
				view.time = cases[c].timeMs; view.rdflags = 0;
				CHECK( RenderSubmission_BeginFrame( &frontend, generation ) );
				CHECK( RenderSubmission_AddEntity( &frontend, &surface, NULL ) );
				CHECK( RenderSubmission_RenderScene( &frontend, &view, 0 ) );
				CHECK( RalMetal_PresentAcquire( present, &coreReceipt, &layer, generation - 1u, &drawable ) );
				CHECK( RalMetal_PresentRequestCapture( present ) );
				CHECK( RalMetal_PresentClearAndSubmit( present, &coreReceipt, &layer,
					&drawable, &frontend, black, generation, &receipt ) );
				CHECK( receipt.readbackByteCount == 4u );
				if ( !ordered ) memcpy( expectedPixel, receipt.readbackBytes, 4u );
				else for ( unsigned channel = 0u; channel < 3u; ++channel ) {
					fprintf( stderr, "tcMod %s channel=%u reference=%u actual=%u\n",
						cases[c].name, channel, expectedPixel[channel], receipt.readbackBytes[channel] );
					CHECK( abs( (int)expectedPixel[channel] - receipt.readbackBytes[channel] ) <= 1 );
				}
				CHECK( RenderSubmission_EndFrame( &frontend, generation, &submission ) );
			}
		}
		/* Paired model draws: the reference mesh carries independently computed
		 * per-vertex UVs. Translation, rotation and scale must not enter the
		 * authored vector/turb coordinates of the actual model. */
		{
			mapFile_t world = {};
			dsurface_t surfaces[2] = {};
			drawVert_t vertices[8] = {};
			dshader_t shader = {};
			int indices[12] = { 0, 1, 2, 0, 2, 3, 0, 1, 2, 0, 2, 3 };
			strcpy( world.name, "maps/tcmod-local-space.bsp" );
			strcpy( shader.shader, "test/ordered-uv" );
			world.checksum = 0x227123;
			world.numShaders = 1; world.shaders = &shader;
			world.numSurfaces = 2; world.surfaces = surfaces;
			world.numDrawVerts = 8; world.drawVerts = vertices;
			world.numDrawIndexes = 12; world.drawIndexes = indices;
			const float yz[4][2] = { { 32, 32 }, { -32, 32 },
				{ -32, -32 }, { 32, -32 } };
			for ( unsigned mesh = 0u; mesh < 2u; ++mesh ) {
				surfaces[mesh].surfaceType = MST_PLANAR;
				surfaces[mesh].firstVert = mesh * 4u; surfaces[mesh].numVerts = 4;
				surfaces[mesh].firstIndex = mesh * 6u; surfaces[mesh].numIndexes = 6;
				surfaces[mesh].lightmapNum = -1;
				for ( unsigned v = 0u; v < 4u; ++v ) {
					drawVert_t *vertex = &vertices[mesh * 4u + v];
					vertex->xyz[0] = 32; vertex->xyz[1] = yz[v][0];
					vertex->xyz[2] = yz[v][1]; vertex->normal[0] = -1;
					memset( vertex->color.rgba, 255, sizeof( vertex->color.rgba ) );
					vertex->st[0] = .2f + .25f * tables[0][(int)(32 + yz[v][1]) & 1023];
					vertex->st[1] = .3f + .25f * tables[0][(int)yz[v][0] & 1023];
				}
			}
			CHECK( RenderSubmission_LoadWorld( &frontend, &world, 0 ) );
			qhandle_t models[2] = {
				RenderSubmission_RegisterInlineModel( &frontend, "*tc-ref", 0u, 1u ),
				RenderSubmission_RegisterInlineModel( &frontend, "*tc-actual", 1u, 1u )
			};
			CHECK( models[0] > 0 && models[1] > 0 );
			CHECK( RenderSubmission_SetMaterialCullMode( &frontend, material, RENDER_CULL_NONE ) );
			for ( unsigned pose = 0u; pose < 3u; ++pose ) {
				byte reference[16u * 16u * 3u] = {};
				for ( unsigned actual = 0u; actual < 2u; ++actual ) {
					renderMaterialStageSnapshot_t stage = {};
					stage.material = material;
					stage.scaleScroll[0] = stage.scaleScroll[1] = 1;
					stage.sourceBlend = RENDER_MATERIAL_BLEND_ONE;
					if ( actual ) {
						stage.tcGen = RENDER_MATERIAL_TCGEN_VECTOR;
						stage.tcGenVectors[0][0] = .2f / 32.0f;
						stage.tcGenVectors[1][0] = .3f / 32.0f;
						stage.tcModCount = 1;
						stage.tcMods[0].type = RAL_MATERIAL_TCMOD_TURBULENCE;
						stage.tcMods[0].parameters[1] = .25f;
					}
					CHECK( RenderSubmission_SetMaterialStages( &frontend, material, &stage, 1u ) );
					refEntity_t model = {};
					model.reType = RT_MODEL; model.hModel = models[actual];
					model.customShader = material; model.origin[0] = 224;
					model.origin[1] = pose == 1u ? 13 : 0;
					model.origin[2] = pose == 1u ? -7 : 0;
					model.axis[0][0] = 1;
					if ( pose == 2u ) {
						model.axis[1][2] = 8; model.axis[2][1] = -8;
					} else model.axis[1][1] = model.axis[2][2] = 8;
					model.nonNormalizedAxes = qtrue;
					memset( model.shader.rgba, 255, sizeof( model.shader.rgba ) );
					uint64_t generation = 1000u + pose * 4u + actual * 2u;
					view.rdflags = RDF_NOWORLDMODEL;
					CHECK( RenderSubmission_BeginFrame( &frontend, generation ) );
					CHECK( RenderSubmission_AddEntity( &frontend, &model, NULL ) );
					CHECK( RenderSubmission_RenderScene( &frontend, &view, 0 ) );
					CHECK( RalMetal_PresentAcquire( present, &coreReceipt, &layer,
						generation - 1u, &drawable ) );
					CHECK( RalMetal_PresentRequestCapture( present ) );
					CHECK( RalMetal_PresentClearAndSubmit( present, &coreReceipt, &layer,
						&drawable, &frontend, black, generation, &receipt ) );
					uint32_t width = 0, height = 0;
					const byte *rgb = RalMetal_PresentCaptureRgb( present, &width, &height );
					CHECK( rgb && width == 16 && height == 16 );
					CHECK( receipt.loweredEntityIndexCount == 6u );
					CHECK( receipt.loweredWorldIndexCount == 0u );
					if ( !actual ) {
						memcpy( reference, rgb, sizeof( reference ) );
						CHECK( rgb[(8u * 16u + 8u) * 3u] > 0u );
					} else for ( unsigned pixel = 0; pixel < sizeof( reference ); ++pixel )
						CHECK( abs( (int)reference[pixel] - rgb[pixel] ) <= 1 );
					CHECK( RenderSubmission_EndFrame( &frontend, generation, &submission ) );
				}
			}
		}
	}
	/* Environment tcGen is model-local, unlike the world-space lighting
	 * normal. Each pose has an independently authored reference UV mesh. */
	{
		mapFile_t world = {};
		dsurface_t surfaces[6] = {};
		drawVert_t vertices[24] = {};
		dshader_t shader = {};
		int indices[36] = {};
		refEntity_t poses[5] = {};
		const float camera[3] = { 17, -11, 9 };
		byte gradient[16 * 16 * 4];
		for ( unsigned y = 0; y < 16; ++y ) for ( unsigned x = 0; x < 16; ++x ) {
			unsigned p = (y * 16 + x) * 4;
			gradient[p] = x * 16; gradient[p + 1] = y * 16;
			gradient[p + 2] = 64; gradient[p + 3] = 255;
		}
		qhandle_t material = RenderSubmission_RegisterMaterialImage( &frontend,
			RENDER_ASSET_MATERIAL, "test/model-environment", qtrue, gradient, 16, 16 );
		CHECK( material > 0 );
		CHECK( RenderSubmission_SetMaterialCullMode( &frontend, material, RENDER_CULL_NONE ) );
		for ( unsigned pose = 0; pose < 5; ++pose ) {
			refEntity_t *model = &poses[pose];
			model->reType = RT_MODEL; model->customShader = material;
			model->origin[0] = 128; model->origin[1] = pose == 1 ? 19 : 0;
			model->origin[2] = pose == 1 ? -23 : 0;
			model->axis[0][0] = 1;
			if ( pose == 2 ) {
				model->axis[1][2] = 1; model->axis[2][1] = -1;
			} else model->axis[1][1] = model->axis[2][2] = 1;
			if ( pose == 3 ) {
				model->axis[0][0] = model->axis[1][1] = model->axis[2][2] = 2;
				model->nonNormalizedAxes = qtrue;
			}
			memset( model->shader.rgba, 255, sizeof( model->shader.rgba ) );
		}
		strcpy( world.name, "maps/environment-model-space.bsp" );
		strcpy( shader.shader, "test/model-environment" );
		world.checksum = 0x227124; world.numShaders = 1; world.shaders = &shader;
		world.numSurfaces = 6; world.surfaces = surfaces;
		world.numDrawVerts = 24; world.drawVerts = vertices;
		world.numDrawIndexes = 36; world.drawIndexes = indices;
		const float yz[4][2] = { { 128, 128 }, { -128, 128 }, { -128, -128 }, { 128, -128 } };
		for ( unsigned mesh = 0; mesh < 6; ++mesh ) {
			surfaces[mesh].surfaceType = MST_PLANAR;
			surfaces[mesh].firstVert = mesh * 4; surfaces[mesh].numVerts = 4;
			surfaces[mesh].firstIndex = mesh * 6; surfaces[mesh].numIndexes = 6;
			surfaces[mesh].lightmapNum = -1;
			const int quad[] = { 0, 1, 2, 0, 2, 3 };
			memcpy( indices + mesh * 6, quad, sizeof( quad ) );
			for ( unsigned v = 0; v < 4; ++v ) {
				drawVert_t *vertex = &vertices[mesh * 4 + v];
				vertex->xyz[0] = 32; vertex->xyz[1] = yz[v][0]; vertex->xyz[2] = yz[v][1];
				vertex->normal[0] = -1;
				memset( vertex->color.rgba, 255, sizeof( vertex->color.rgba ) );
				if ( mesh < 5 ) {
					const refEntity_t *model = &poses[mesh];
					float viewer[3], cameraDelta[3];
					for ( unsigned axis = 0; axis < 3; ++axis )
						cameraDelta[axis] = (mesh == 4 ? camera[axis] : 0) - model->origin[axis];
					float inverseScale = model->nonNormalizedAxes ? .5f : 1;
					for ( unsigned axis = 0; axis < 3; ++axis ) {
						viewer[axis] = DotProduct( cameraDelta, model->axis[axis] ) * inverseScale
							- vertex->xyz[axis];
					}
					float length = sqrtf( DotProduct( viewer, viewer ) );
					/* Normal (-1,0,0) reflects y/z to -viewer.y/z. */
					vertex->st[0] = .5f - .5f * viewer[1] / length;
					vertex->st[1] = .5f + .5f * viewer[2] / length;
				}
			}
		}
		CHECK( RenderSubmission_LoadWorld( &frontend, &world, 0 ) );
		qhandle_t models[6];
		for ( unsigned mesh = 0; mesh < 6; ++mesh ) {
			char name[32]; snprintf( name, sizeof( name ), "*environment-%u", mesh );
			models[mesh] = RenderSubmission_RegisterInlineModel( &frontend, name, mesh, 1 );
			CHECK( models[mesh] > 0 );
			renderModelSnapshot_t snapshot;
			CHECK( RenderSubmission_ModelSnapshot( &frontend, models[mesh], &snapshot ) );
			unsigned v = snapshot.indices[0];
			CHECK( snapshot.texCoords[v * 2] == vertices[mesh * 4].st[0] );
			CHECK( snapshot.texCoords[v * 2 + 1] == vertices[mesh * 4].st[1] );
		}
		for ( unsigned pose = 0; pose < 5; ++pose ) {
			for ( unsigned axis = 0; axis < 3; ++axis )
				view.vieworg[axis] = pose == 4 ? camera[axis] : 0;
			byte reference[16 * 16 * 3];
			for ( unsigned actual = 0; actual < 2; ++actual ) {
				renderMaterialStageSnapshot_t stage = {};
				stage.material = material; stage.sourceBlend = RENDER_MATERIAL_BLEND_ONE;
				stage.scaleScroll[0] = stage.scaleScroll[1] = 1;
				stage.tcGen = actual ? RENDER_MATERIAL_TCGEN_ENVIRONMENT : RENDER_MATERIAL_TCGEN_TEXTURE;
				CHECK( RenderSubmission_SetMaterialStages( &frontend, material, &stage, 1 ) );
				refEntity_t model = poses[pose]; model.hModel = models[actual ? 5 : pose];
				uint64_t generation = 1100 + pose * 4 + actual * 2;
				view.rdflags = RDF_NOWORLDMODEL;
				CHECK( RenderSubmission_BeginFrame( &frontend, generation ) );
				CHECK( RenderSubmission_AddEntity( &frontend, &model, NULL ) );
				CHECK( RenderSubmission_RenderScene( &frontend, &view, 0 ) );
				CHECK( RalMetal_PresentAcquire( present, &coreReceipt, &layer, generation - 1, &drawable ) );
				CHECK( RalMetal_PresentRequestCapture( present ) );
				CHECK( RalMetal_PresentClearAndSubmit( present, &coreReceipt, &layer,
					&drawable, &frontend, black, generation, &receipt ) );
				uint32_t width, height;
				const byte *rgb = RalMetal_PresentCaptureRgb( present, &width, &height );
				CHECK( rgb && width == 16 && height == 16 && receipt.loweredEntityIndexCount == 6 );
				CHECK( receipt.loweredWorldIndexCount == 0 );
				if ( !actual ) {
					memcpy( reference, rgb, sizeof( reference ) );
					fprintf( stderr, "environment reference pose=%u center=%u,%u,%u\n", pose,
						rgb[(8 * 16 + 8) * 3], rgb[(8 * 16 + 8) * 3 + 1], rgb[(8 * 16 + 8) * 3 + 2] );
					CHECK( rgb[(8 * 16 + 8) * 3] > 0 );
				} else for ( unsigned p = 0; p < sizeof( reference ); ++p ) {
					if ( abs( (int)reference[p] - rgb[p] ) > 1 )
						fprintf( stderr, "environment pose=%u pixel=%u reference=%u actual=%u\n", pose, p, reference[p], rgb[p] );
					CHECK( abs( (int)reference[p] - rgb[p] ) <= 1 );
				}
				CHECK( RenderSubmission_EndFrame( &frontend, generation, &submission ) );
			}
		}
	}
	RalMetal_PresentDestroy( present );
	RenderSubmission_Reset( &frontend );
	RalMetal_CoreDestroy( core );
	return 0;
}

/* Exercise the Product path, not ExposureEncode in isolation: refdef event
 * -> HDR scene -> histogram/reduce -> tonemap -> output pixels, with UI kept
 * outside metering. No test-only exposure buffer or reset entry point. */
static int ExposureProductPixels( void ) {
	ralMetalCoreCreateInfo_t info = { 1u };
	ralMetalCore_t *core = NULL;
	ralMetalCoreReceipt_t coreReceipt;
	ralMetalPresent_t *present = NULL;
	ralMetalPresent_t *withoutUi = NULL;
	ralMetalPresentLayerReceipt_t withoutUiLayer;
	ralMetalPresentLayerReceipt_t layer;
	ralMetalDrawableReceipt_t drawable;
	ralMetalPresentReceipt_t receipt;
	static renderSubmissionState_t frontend;
	renderSubmissionReceipt_t submission;
	ralSurfaceFormat_t format = {
		RAL_FORMAT_B8G8R8A8_UNORM, RAL_COLORSPACE_SRGB_NONLINEAR };
	ralPresentPreference_t fifo = { RAL_PRESENT_FIFO, 3u, 3u };
	ralSwapchainCreateInfo_t create = {};
	ralExposureSettings_t exposure = { 1, 0.0123f, 0.5f, 0.2f, 1.5f, 0.5f, 0.25f, 8.0f };
	auto checkSampling = [&]() -> int {
		wiredSamplerProbe probe( RalMetal_CoreNativeDevice( core ) );
		ralSamplerCreateInfo_t sampling;
		CHECK( wired_texture_sampling_policy("GL_LINEAR_MIPMAP_LINEAR", qtrue, 8, &sampling) );
		CHECK( wired_metal_present_set_sampling(present, &sampling) );
		CHECK( wiredSamplerCalls == 2 && wiredLastSampler.maxAnisotropy == 8 );
		CHECK( wiredLastSampler.minFilter == MTLSamplerMinMagFilterLinear
			&& wiredLastSampler.magFilter == MTLSamplerMinMagFilterLinear
			&& wiredLastSampler.mipFilter == MTLSamplerMipFilterLinear
			&& wiredLastSampler.sAddressMode == MTLSamplerAddressModeRepeat
			&& wiredLastSampler.supportArgumentBuffers );
		CHECK( wired_metal_present_set_sampling(present, &sampling) && wiredSamplerCalls == 2 );
		ralSamplerCreateInfo_t previous = sampling;
		CHECK( wired_texture_sampling_policy("GL_NEAREST", qtrue, 8, &sampling) );
		wiredSamplerFailAt = 4; // second candidate fails; previous pair stays active
		CHECK( !wired_metal_present_set_sampling(present, &sampling) );
		CHECK( wired_metal_present_set_sampling(present, &previous) && wiredSamplerCalls == 4 );
		wiredSamplerFailAt = 0;
		CHECK( wired_metal_present_set_sampling(present, &sampling) );
		CHECK( wiredSamplerCalls == 6 && wiredLastSampler.maxAnisotropy == 1
			&& wiredLastSampler.minFilter == MTLSamplerMinMagFilterNearest
			&& wiredLastSampler.magFilter == MTLSamplerMinMagFilterNearest
			&& wiredLastSampler.mipFilter == MTLSamplerMipFilterNearest
			&& wiredLastSampler.lodMaxClamp == 0.25f );
		sampling.maxAnisotropy = NAN;
		CHECK( !wired_metal_present_set_sampling(present, &sampling) && wiredSamplerCalls == 6 );
		CHECK( !wired_metal_present_set_sampling(present, NULL) );
		// Keep the existing GPU fixture's trilinear/non-anisotropic quality.
		CHECK( wired_texture_sampling_policy("GL_LINEAR_MIPMAP_LINEAR", qfalse, 1, &sampling) );
		CHECK( wired_metal_present_set_sampling(present, &sampling) );
		return 0;
	};
	refdef_t view = {};
	refEntity_t surface = {};
	const byte gray[4] = { 64u, 64u, 64u, 255u };
	const byte white[4] = { 255u, 255u, 255u, 255u };
	const float black[4] = { 0, 0, 0, 1 };
	byte baseline[3] = {}, adapted[3] = {};
	CHECK( RalMetal_CoreCreate( &info, &core, &coreReceipt ) );
	CHECK( RenderSubmission_Init( &frontend, 1u ) );
	surface.customShader = RenderSubmission_RegisterMaterialImage( &frontend,
		RENDER_ASSET_MATERIAL, "test/exposure-gray", qtrue, gray, 1u, 1u );
	const qhandle_t ui = RenderSubmission_RegisterMaterialImage( &frontend,
		RENDER_ASSET_MATERIAL, "test/exposure-ui", qtrue, white, 1u, 1u );
	CHECK( surface.customShader > 0 && ui > 0 );
	surface.reType = RT_SPRITE; surface.origin[0] = 8.0f; surface.radius = 32.0f;
	memset( surface.shader.rgba, 255, sizeof( surface.shader.rgba ) );
	view.width = view.height = 16;
	view.fov_x = view.fov_y = 90.0f;
	view.viewaxis[0][0] = view.viewaxis[1][1] = view.viewaxis[2][2] = 1.0f;
	create.desiredWidth = create.desiredHeight = 16u; // offscreen; no window
	create.formatPreferences = &format; create.formatPreferenceCount = 1u;
	create.presentPreferences = &fifo; create.presentPreferenceCount = 1u;
	create.requiredUsage = RAL_TEXTURE_USAGE_COLOR_ATTACHMENT;
	CHECK( RalMetal_PresentCreate( core, &coreReceipt, &create, 2u, &present, &layer ) );
	/* Tone exposure is ignored with tonemap disabled, independently of AE.
	 * Compare a nonidentity setting against the unit-exposure reference. */
	CHECK( checkSampling() == 0 );
	CHECK( RalMetal_PresentSetToneMap( present, 0u, 5.2f, 1.6f, 0.977f, 0.18f, 0.267f, 8.0f ) );
	CHECK( RalMetal_PresentSetExposure( present, &exposure ) );
	CHECK( RalMetal_PresentCreate( core, &coreReceipt, &create, 2u,
		&withoutUi, &withoutUiLayer ) );
	CHECK( RalMetal_PresentSetToneMap( withoutUi, 0u, 1.0f, 1.6f, 0.977f, 0.18f, 0.267f, 8.0f ) );
	CHECK( RalMetal_PresentSetExposure( withoutUi, &exposure ) );
	for ( unsigned step = 0; step <= 55u; ++step ) {
		const uint64_t generation = 4u + step * 2u;
		const bool teleportGap = step == 53u;
		view.time = step == 17u ? 1700 : 100 + (int)step * 100;
		view.rdflags = step == 18u ? RDF_CAMERA_CUT : step == 35u ? RDF_HYPERSPACE : 0;
		if ( step == 36u ) view.time = 10; // rewind without a camera-cut flag
		if ( teleportGap ) view.rdflags = RDF_NOWORLDMODEL | RDF_HYPERSPACE | RDF_CAMERA_CUT;
		if ( step == 55u ) view.time = 5500; // hold the first post-teleport frame
		CHECK( RenderSubmission_BeginFrame( &frontend, generation ) );
		if ( !teleportGap ) CHECK( RenderSubmission_AddEntity( &frontend, &surface, NULL ) );
		CHECK( RenderSubmission_RenderScene( &frontend, &view, 0 ) );
		CHECK( RalMetal_PresentAcquire( withoutUi, &coreReceipt, &withoutUiLayer,
			generation - 1u, &drawable ) );
		CHECK( RalMetal_PresentRequestCapture( withoutUi ) );
		CHECK( RalMetal_PresentClearAndSubmit( withoutUi, &coreReceipt, &withoutUiLayer,
			&drawable, &frontend, black, generation, &receipt ) );
		uint32_t referenceWidth, referenceHeight;
		const byte *reference = RalMetal_PresentCaptureRgb( withoutUi,
			&referenceWidth, &referenceHeight );
		CHECK( reference && referenceWidth == 16u && referenceHeight == 16u );
		/* Half-screen white UI exceeds histogram highlight trimming: if UI is
		 * accidentally metered, the two independent histories must diverge. */
		CHECK( RenderSubmission_AddUiQuad( &frontend, 0, 0, 8, 16,
			0, 0, 1, 1, 0, ui ) );
		CHECK( RalMetal_PresentAcquire( present, &coreReceipt, &layer,
			generation - 1u, &drawable ) );
		CHECK( RalMetal_PresentRequestCapture( present ) );
		CHECK( RalMetal_PresentClearAndSubmit( present, &coreReceipt, &layer,
			&drawable, &frontend, black, generation, &receipt ) );
		uint32_t width, height;
		const byte *pixels = RalMetal_PresentCaptureRgb( present, &width, &height );
		CHECK( pixels && width == 16u && height == 16u );
		const byte *center = pixels + ( 8u * width + 12u ) * 3u;
		CHECK( !memcmp( center, reference + ( 8u * referenceWidth + 12u ) * 3u, 3u ) );
		/* CaptureRgb follows the screenshot API's bottom-up RGB convention. */
		const byte *overlay = pixels + ( ( height - 2u ) * width + 1u ) * 3u;
		CHECK( overlay[0] == 255u && overlay[1] == 255u && overlay[2] == 255u );
		if ( step == 0 ) {
			memcpy( baseline, center, 3u );
			CHECK( baseline[0] > 5u && baseline[0] < 200u );
		}
		if ( step == 16u || step == 34u || step == 52u ) {
			CHECK( abs( (int)center[0] - baseline[0] ) >= 3 );
			memcpy( adapted, center, 3u );
		}
		if ( step == 17u ) CHECK( !memcmp( adapted, center, 3u ) ); // pause
		/* Real teleport removes the world for a frame: the cut must survive
		 * the bypassed HDR/metering pass, then reset without a repeated flag. */
		if ( teleportGap ) CHECK( center[0] == 0u && center[1] == 0u && center[2] == 0u );
		if ( step == 18u || step == 35u || step == 36u || step == 54u || step == 55u ) {
			CHECK( !memcmp( baseline, center, 3u ) );
			fprintf( stderr, "Metal Product exposure reset step=%u identity=%u adapted=%u reset=%u UI=%u\n",
				step, baseline[0], adapted[0], center[0], overlay[0] );
		}
		CHECK( RenderSubmission_EndFrame( &frontend, generation, &submission ) );
	}
	RalMetal_PresentDestroy( withoutUi );
	RalMetal_PresentDestroy( present );
	RenderSubmission_Reset( &frontend );
	RalMetal_CoreDestroy( core );
	return 0;
}

/* Skybox and beam flags must be disjoint. Sky texels must not acquire the
 * beam path's descriptor tint/coverage or skip physical composition. */
/* Independent UV oracle: cyclically shift the texture texels on the CPU,
 * then draw a static beam. This does not reuse the production UV arithmetic. */
static int BeamUvClockAxis( unsigned axis ) {
	for ( unsigned route = 0; route < 4; ++route ) {
		for ( unsigned persistent = 0; persistent < 2; ++persistent ) {
			byte actual[3][16 * 16 * 3] = {};
			for ( unsigned reference = 0; reference < 2; ++reference ) {
				ralMetalCoreCreateInfo_t info = { 1u };
				ralMetalCore_t *core = NULL;
				ralMetalCoreReceipt_t coreReceipt;
				ralMetalPresent_t *present = NULL;
				ralMetalPresentLayerReceipt_t layer;
				ralMetalDrawableReceipt_t drawable;
				ralMetalPresentReceipt_t receipt;
				auto frontendStorage = std::make_unique<renderSubmissionState_t>();
	renderSubmissionState_t &frontend = *frontendStorage;
				renderSubmissionReceipt_t submission;
				ralSurfaceFormat_t format = { RAL_FORMAT_B8G8R8A8_UNORM, RAL_COLORSPACE_SRGB_NONLINEAR };
				ralPresentPreference_t fifo = { RAL_PRESENT_FIFO, 3u, 3u };
				ralSwapchainCreateInfo_t create = {};
				refdef_t view = {};
				const float black[4] = { 0, 0, 0, 1 };
				CHECK( RalMetal_CoreCreate( &info, &core, &coreReceipt ) );
				CHECK( RenderSubmission_Init( &frontend, 1u ) );
				create.desiredWidth = create.desiredHeight = 16u; // offscreen
				create.formatPreferences = &format; create.formatPreferenceCount = 1u;
				create.presentPreferences = &fifo; create.presentPreferenceCount = 1u;
				create.requiredUsage = RAL_TEXTURE_USAGE_COLOR_ATTACHMENT;
				CHECK( RalMetal_PresentCreate( core, &coreReceipt, &create, 2u, &present, &layer ) );
				CHECK( RalMetal_PresentSetToneMap( present, 0u, 1.0f, 1.6f, 0.977f, 0.18f, 0.267f, 8.0f ) );
				view.width = view.height = 16; view.fov_x = view.fov_y = 90;
				view.viewaxis[0][0] = view.viewaxis[1][1] = view.viewaxis[2][2] = 1;
				for ( unsigned step = 0; step < 3; ++step ) {
					const uint64_t generation = 4u + step * 2u;
					const unsigned seconds = persistent ? step : step + 1u;
					const unsigned shift = reference ? seconds * ( route == 3u ? 8u : 4u ) : 0u;
					byte pixels[16 * 16 * 4]; char name[64];
					for ( unsigned pixel = 0; pixel < 16 * 16; ++pixel ) {
						const unsigned coordinate = axis ? pixel / 16u : pixel % 16u;
						const unsigned source = ( coordinate + shift ) % 16u;
						pixels[pixel * 4u] = (byte)( source * 16u );
						pixels[pixel * 4u + 1u] = (byte)( 240u - source * 16u );
						pixels[pixel * 4u + 2u] = source < 8u ? 255u : 0u;
						pixels[pixel * 4u + 3u] = 255u;
					}
					snprintf( name, sizeof( name ), "test/beam-uv-%u", step );
					qhandle_t material = RenderSubmission_RegisterMaterialImage( &frontend,
						RENDER_ASSET_MATERIAL, name, qfalse, pixels, 16u, 16u );
					CHECK( material > 0 );
					if ( route ) {
						renderMaterialStageSnapshot_t stage = {};
						stage.material = RenderSubmission_RegisterStageTexture( &frontend,
							RENDER_ASSET_MATERIAL, name, qfalse, pixels, 16u, 16u );
						CHECK( stage.material > 0 );
						stage.sourceBlend = stage.destinationBlend = RENDER_MATERIAL_BLEND_ONE;
						stage.scaleScroll[0] = stage.scaleScroll[1] = 1.0f;
						stage.scaleScroll[axis] = 2.0f;
						stage.scaleScroll[2u + axis] = !reference && route >= 2u ? 0.25f : 0.0f;
						CHECK( RenderSubmission_SetMaterialStages( &frontend, material, &stage, 1u ) );
					}
					beamDesc_t beam = {};
					beam.start[0] = beam.end[0] = 8;
					beam.start[1] = -4; beam.end[1] = 4;
					beam.startWidth = beam.endWidth = 4;
					for ( unsigned c = 0; c < 4; ++c ) beam.startColor[c] = beam.endColor[c] = 1;
					beam.shader = material; beam.axialCopies = 1;
					beam.startEntityNum = beam.endEntityNum = -1;
					beam.flags = PRIM_FLAG_ADDITIVE;
					beam.duration = persistent && !reference ? 10.0f : 0.0f;
					beam.uvScroll[axis] = !reference && route != 2u ? 0.25f : 0.0f;
					view.time = ( step + 1u ) * 1000;
					CHECK( RenderSubmission_BeginFrame( &frontend, generation ) );
					CHECK( RenderSubmission_RenderScene( &frontend, &view, 0 ) );
					if ( reference || !persistent || !step )
						CHECK( RenderSubmission_AddEffectBeam( &frontend, &beam ) );
					CHECK( RalMetal_PresentAcquire( present, &coreReceipt, &layer, generation-1u, &drawable ) );
					CHECK( RalMetal_PresentRequestCapture( present ) );
					CHECK( RalMetal_PresentClearAndSubmit( present, &coreReceipt, &layer,
						&drawable, &frontend, black, generation, &receipt ) );
					uint32_t width, height;
					const byte *rgb = RalMetal_PresentCaptureRgb( present, &width, &height );
					CHECK( rgb && width == 16u && height == 16u );
					if ( !reference ) memcpy( actual[step], rgb, sizeof( actual[step] ) );
					else {
						unsigned mismatches = 0, visible = 0;
						for ( unsigned c = 0; c < sizeof( actual[step] ); ++c ) {
							if ( abs( (int)rgb[c] - actual[step][c] ) > 1 ) ++mismatches;
							if ( rgb[c] > 32 ) ++visible;
						}
						fprintf( stderr, "beam UV axis=%u route=%u persistent=%u step=%u mismatches=%u\n", axis, route, persistent, step, mismatches );
						CHECK( visible > 8u && mismatches == 0u );
					}
					CHECK( RenderSubmission_EndFrame( &frontend, generation, &submission ) );
				}
				RalMetal_PresentDestroy( present ); RenderSubmission_Reset( &frontend );
				RalMetal_CoreDestroy( core );
			}
		}
	}
	return 0;
}

static int SkyBoxIsNotBeam( void ) {
	ralMetalCoreCreateInfo_t info = { 1u };
	ralMetalCore_t *core = NULL;
	ralMetalCoreReceipt_t coreReceipt;
	ralMetalPresent_t *present = NULL;
	ralMetalPresentLayerReceipt_t layer;
	ralMetalDrawableReceipt_t drawable;
	ralMetalPresentReceipt_t receipt;
	auto frontendStorage = std::make_unique<renderSubmissionState_t>();
	renderSubmissionState_t &frontend = *frontendStorage;
	renderSubmissionReceipt_t submission;
	ralSurfaceFormat_t format = { RAL_FORMAT_B8G8R8A8_UNORM, RAL_COLORSPACE_SRGB_NONLINEAR };
	ralPresentPreference_t fifo = { RAL_PRESENT_FIFO, 3u, 3u };
	ralSwapchainCreateInfo_t create = {};
	mapFile_t world = {};
	dsurface_t surface = {};
	drawVert_t vertices[3] = {};
	int indices[3] = { 0, 1, 2 };
	dshader_t shader = {};
	refdef_t view = {};
	const byte green[4] = { 0u, 255u, 0u, 255u };
	const float black[4] = { 0, 0, 0, 1 };
	CHECK( RalMetal_CoreCreate( &info, &core, &coreReceipt ) );
	CHECK( RenderSubmission_Init( &frontend, 1u ) );
	strcpy( shader.shader, "test/sky-not-beam" );
	const qhandle_t material = RenderSubmission_RegisterMaterialImage( &frontend,
		RENDER_ASSET_MATERIAL, shader.shader, qtrue, green, 1u, 1u );
	CHECK( material > 0 );
	CHECK( RenderSubmission_SetMaterialSky( &frontend, material, qtrue ) );
	CHECK( RenderSubmission_SetMaterialCullMode( &frontend, material, RENDER_CULL_NONE ) );
	strcpy( world.name, "maps/sky-not-beam.bsp" );
	world.checksum = 239u;
	world.numSurfaces = 1; world.surfaces = &surface;
	world.numDrawVerts = 3; world.drawVerts = vertices;
	world.numDrawIndexes = 3; world.drawIndexes = indices;
	world.numShaders = 1; world.shaders = &shader;
	surface.surfaceType = MST_PLANAR;
	surface.numVerts = 3; surface.numIndexes = 3; surface.lightmapNum = -1;
	vertices[0].xyz[1] = -16; vertices[0].xyz[2] = -16;
	vertices[1].xyz[1] = 16; vertices[1].xyz[2] = -16;
	vertices[2].xyz[2] = 16;
	for ( unsigned i = 0; i < 3; ++i ) {
		vertices[i].xyz[0] = 8; vertices[i].normal[0] = -1;
		vertices[i].color.rgba[0] = vertices[i].color.rgba[3] = 255;
	}
	CHECK( RenderSubmission_LoadWorld( &frontend, &world, 0 ) );
	create.desiredWidth = create.desiredHeight = 16u; // offscreen, no window
	create.formatPreferences = &format; create.formatPreferenceCount = 1u;
	create.presentPreferences = &fifo; create.presentPreferenceCount = 1u;
	create.requiredUsage = RAL_TEXTURE_USAGE_COLOR_ATTACHMENT;
	CHECK( RalMetal_PresentCreate( core, &coreReceipt, &create, 2u, &present, &layer ) );
	CHECK( RalMetal_PresentSetToneMap( present, 0u, 1.0f, 1.6f, 0.977f, 0.18f, 0.267f, 8.0f ) );
	view.width = view.height = 16; view.fov_x = view.fov_y = 90;
	view.viewaxis[0][0] = view.viewaxis[1][1] = view.viewaxis[2][2] = 1;
	for ( unsigned box = 0; box < 2; ++box ) {
		const uint64_t generation = 4u + box * 2u;
		CHECK( RenderSubmission_SetMaterialSkyBox( &frontend, material, box ? qtrue : qfalse ) );
		CHECK( RenderSubmission_BeginFrame( &frontend, generation ) );
		CHECK( RenderSubmission_RenderScene( &frontend, &view, 0 ) );
		CHECK( RalMetal_PresentAcquire( present, &coreReceipt, &layer, generation-1u, &drawable ) );
		CHECK( RalMetal_PresentRequestCapture( present ) );
		CHECK( RalMetal_PresentClearAndSubmit( present, &coreReceipt, &layer,
			&drawable, &frontend, black, generation, &receipt ) );
		fprintf( stderr, "skybox=%u green pixel=%u\n", box, receipt.readbackBytes[1] );
		CHECK( receipt.readbackByteCount == 4u && receipt.readbackBytes[1] == 255u );
		CHECK( receipt.readbackBytes[0] == 0u && receipt.readbackBytes[2] == 0u );
		CHECK( RenderSubmission_EndFrame( &frontend, generation, &submission ) );
	}
	/* Empty/alpha-rejected cloud layers must preserve the outer cube. The
	 * stage image is red while the outer image is green: using the atlas as
	 * the stage texture or discarding the whole sky cannot pass this probe. */
	const byte transparentRed[4] = { 255u, 0u, 0u, 0u };
	const qhandle_t cloud = RenderSubmission_RegisterStageTexture( &frontend,
		RENDER_ASSET_MATERIAL, "test/empty-cloud", qtrue, transparentRed, 1u, 1u );
	CHECK( cloud > 0 );
	for ( unsigned rejected = 0; rejected < 2; ++rejected ) {
		renderMaterialStageSnapshot_t stage = {};
		stage.material = cloud;
		stage.scaleScroll[0] = stage.scaleScroll[1] = 1.0f;
		stage.alphaGen = RAL_MATERIAL_ALPHA_SKIP;
		stage.sourceBlend = RENDER_MATERIAL_BLEND_SRC_ALPHA;
		stage.destinationBlend = RENDER_MATERIAL_BLEND_ONE_MINUS_SRC_ALPHA;
		if ( rejected ) {
			stage.alphaTest = RAL_MATERIAL_ALPHA_TEST_KEEP_GREATER_EQUAL;
			stage.alphaCutoff = 0.5f;
		}
		CHECK( RenderSubmission_SetMaterialStages( &frontend, material, &stage, 1u ) );
		const uint64_t generation = 8u + rejected * 2u;
		CHECK( RenderSubmission_BeginFrame( &frontend, generation ) );
		CHECK( RenderSubmission_RenderScene( &frontend, &view, 0 ) );
		CHECK( RalMetal_PresentAcquire( present, &coreReceipt, &layer, generation - 1, &drawable ) );
		CHECK( RalMetal_PresentRequestCapture( present ) );
		CHECK( RalMetal_PresentClearAndSubmit( present, &coreReceipt, &layer,
			&drawable, &frontend, black, generation, &receipt ) );
		CHECK( receipt.readbackByteCount == 4u && receipt.readbackBytes[1] == 255u );
		CHECK( receipt.readbackBytes[0] == 0u && receipt.readbackBytes[2] == 0u );
		CHECK( RenderSubmission_EndFrame( &frontend, generation, &submission ) );
	}
	RalMetal_PresentDestroy( present ); RenderSubmission_Reset( &frontend );
	RalMetal_CoreDestroy( core );
	return 0;
}

/* Compare the existing analytic cloud projection with that same projection
 * consumed by the full ordered stage program. BSP UVs intentionally stay zero:
 * forwarding those UVs to a cloud stage produces a constant, wrong image. */
static int OrderedSkyProjection( void ) {
	ralMetalCoreCreateInfo_t info = { 1u };
	ralMetalCore_t *core = NULL;
	ralMetalCoreReceipt_t coreReceipt;
	ralMetalPresent_t *present = NULL;
	ralMetalPresentLayerReceipt_t layer;
	ralMetalDrawableReceipt_t drawable;
	ralMetalPresentReceipt_t receipt;
	auto storage = std::make_unique<renderSubmissionState_t>();
	renderSubmissionState_t &frontend = *storage;
	renderSubmissionReceipt_t submission;
	ralSurfaceFormat_t format = { RAL_FORMAT_B8G8R8A8_UNORM, RAL_COLORSPACE_SRGB_NONLINEAR };
	ralPresentPreference_t fifo = { RAL_PRESENT_FIFO, 3u, 3u };
	ralSwapchainCreateInfo_t create = {};
	mapFile_t world = {};
	dsurface_t surface = {};
	drawVert_t vertices[3] = {};
	int indices[3] = { 0, 1, 2 };
	dshader_t shader = {};
	refdef_t view = {};
	byte gradient[32 * 32 * 4];
	for ( unsigned y = 0; y < 32; ++y ) for ( unsigned x = 0; x < 32; ++x ) {
		byte *pixel = gradient + ( y * 32 + x ) * 4;
		pixel[0] = x * 8; pixel[1] = y * 8; pixel[2] = 32; pixel[3] = 255;
	}
	const float black[4] = { 0, 0, 0, 1 };
	const float scale[2] = { 0.37f, 0.29f }, scroll[2] = { 0.13f, 0.07f };
	CHECK( RalMetal_CoreCreate( &info, &core, &coreReceipt ) );
	CHECK( RenderSubmission_Init( &frontend, 1u ) );
	strcpy( shader.shader, "test/ordered-sky-projection" );
	const qhandle_t material = RenderSubmission_RegisterMaterialImage( &frontend,
		RENDER_ASSET_MATERIAL, shader.shader, qtrue, gradient, 32u, 32u );
	CHECK( material > 0 );
	CHECK( RenderSubmission_SetMaterialSky( &frontend, material, qtrue ) );
	CHECK( RenderSubmission_SetMaterialCullMode( &frontend, material, RENDER_CULL_NONE ) );
	CHECK( RenderSubmission_SetMaterialSkyProjection( &frontend, material, scale, scroll ) );
	strcpy( world.name, "maps/ordered-sky-projection.bsp" );
	world.checksum = 239227u;
	world.numSurfaces = 1; world.surfaces = &surface;
	world.numDrawVerts = 3; world.drawVerts = vertices;
	world.numDrawIndexes = 3; world.drawIndexes = indices;
	world.numShaders = 1; world.shaders = &shader;
	surface.surfaceType = MST_PLANAR;
	surface.numVerts = 3; surface.numIndexes = 3; surface.lightmapNum = -1;
	vertices[0].xyz[1] = -32; vertices[0].xyz[2] = -32;
	vertices[1].xyz[1] = 32; vertices[1].xyz[2] = -32;
	vertices[2].xyz[2] = 32;
	for ( unsigned i = 0; i < 3; ++i ) {
		vertices[i].xyz[0] = 8; vertices[i].normal[0] = -1;
		memset( vertices[i].color.rgba, 255, 4 );
	}
	CHECK( RenderSubmission_LoadWorld( &frontend, &world, 0 ) );
	create.desiredWidth = create.desiredHeight = 16u; // offscreen, no window
	create.formatPreferences = &format; create.formatPreferenceCount = 1u;
	create.presentPreferences = &fifo; create.presentPreferenceCount = 1u;
	create.requiredUsage = RAL_TEXTURE_USAGE_COLOR_ATTACHMENT;
	CHECK( RalMetal_PresentCreate( core, &coreReceipt, &create, 2u, &present, &layer ) );
	CHECK( RalMetal_PresentSetToneMap( present, 0u, 1.0f, 1.6f, 0.977f, 0.18f, 0.267f, 8.0f ) );
	view.width = view.height = 16; view.fov_x = view.fov_y = 90; view.time = 1000;
	view.viewaxis[0][0] = view.viewaxis[1][1] = view.viewaxis[2][2] = 1;
	std::vector<byte> reference;
	for ( unsigned route = 0; route <= 8; ++route ) {
		if ( route ) {
			renderMaterialStageSnapshot_t stages[8] = {};
			for ( unsigned i = 0; i < route; ++i ) {
				stages[i].material = material;
				stages[i].scaleScroll[0] = stages[i].scaleScroll[1] = 1.0f;
				stages[i].sourceBlend = RENDER_MATERIAL_BLEND_ZERO;
				stages[i].destinationBlend = RENDER_MATERIAL_BLEND_ONE;
			}
			auto &stage = stages[route - 1];
			stage.sourceBlend = RENDER_MATERIAL_BLEND_ONE;
			stage.destinationBlend = RENDER_MATERIAL_BLEND_ZERO;
			stage.tcModCount = 2;
			stage.tcMods[0].type = RAL_MATERIAL_TCMOD_SCALE;
			stage.tcMods[0].parameters[0] = scale[0]; stage.tcMods[0].parameters[1] = scale[1];
			stage.tcMods[1].type = RAL_MATERIAL_TCMOD_SCROLL;
			stage.tcMods[1].parameters[0] = scroll[0]; stage.tcMods[1].parameters[1] = scroll[1];
			CHECK( RenderSubmission_SetMaterialStages( &frontend, material, stages, route ) );
		}
		const uint64_t generation = 4u + route * 2u;
		CHECK( RenderSubmission_BeginFrame( &frontend, generation ) );
		CHECK( RenderSubmission_RenderScene( &frontend, &view, 0 ) );
		CHECK( RalMetal_PresentAcquire( present, &coreReceipt, &layer, generation - 1, &drawable ) );
		CHECK( RalMetal_PresentRequestCapture( present ) );
		CHECK( RalMetal_PresentClearAndSubmit( present, &coreReceipt, &layer,
			&drawable, &frontend, black, generation, &receipt ) );
		uint32_t width = 0, height = 0;
		const byte *pixels = RalMetal_PresentCaptureRgb( present, &width, &height );
		CHECK( pixels && width == 16 && height == 16 );
		if ( !route ) reference.assign( pixels, pixels + width * height * 3 );
		else {
			int maxError = 0;
			for ( unsigned p = 0; p < reference.size(); ++p )
				maxError = std::max( maxError, abs( (int)reference[p] - pixels[p] ) );
			fprintf( stderr, "sky stage slot=%u projection maxError=%d\n", route - 1, maxError );
			CHECK( maxError <= 1 );
		}
		CHECK( RenderSubmission_EndFrame( &frontend, generation, &submission ) );
	}
	/* Read the actual HDR source cube, not merely a submission counter.
	 * The eighth stage animates from blue to green and adds to the red base.
	 * Intermediate stages are identity blends; all six captured faces must
	 * use the same one-second clock and the same ordered material program. */
	const byte red[4] = { 64, 0, 0, 255 }, green[4] = { 0, 96, 0, 255 };
	const byte blue[4] = { 0, 0, 128, 255 };
	qhandle_t redMaterial = RenderSubmission_RegisterMaterialImage( &frontend,
		RENDER_ASSET_MATERIAL, "test/capture-red", qtrue, red, 1, 1 );
	qhandle_t greenMaterial = RenderSubmission_RegisterMaterialImage( &frontend,
		RENDER_ASSET_MATERIAL, "test/capture-green", qtrue, green, 1, 1 );
	qhandle_t blueMaterial = RenderSubmission_RegisterMaterialImage( &frontend,
		RENDER_ASSET_MATERIAL, "test/capture-blue", qtrue, blue, 1, 1 );
	CHECK( redMaterial > 0 && greenMaterial > 0 && blueMaterial > 0 );
	renderMaterialStageSnapshot_t captureStages[8] = {};
	for ( unsigned slot = 0; slot < 8; ++slot ) {
		captureStages[slot].material = redMaterial;
		captureStages[slot].scaleScroll[0] = captureStages[slot].scaleScroll[1] = 1;
		captureStages[slot].sourceBlend = slot == 0 || slot == 7
			? RENDER_MATERIAL_BLEND_ONE : RENDER_MATERIAL_BLEND_ZERO;
		captureStages[slot].destinationBlend = slot == 0
			? RENDER_MATERIAL_BLEND_ZERO : RENDER_MATERIAL_BLEND_ONE;
	}
	captureStages[7].material = blueMaterial;
	captureStages[7].animationFrameCount = 2;
	captureStages[7].animationFrequency = 1;
	captureStages[7].animationLooping = qtrue;
	captureStages[7].animationMaterials[0] = blueMaterial;
	captureStages[7].animationMaterials[1] = greenMaterial;
	CHECK( RenderSubmission_SetMaterialStages( &frontend, material, captureStages, 8 ) );
	ralMaterialSourceSky_t skySource = {};
	CHECK( RenderSubmission_SetMaterialAuthoredSkySource( &frontend, material, &skySource ) );
	atmosphereFrameState_t atmosphere = {};
	atmosphere.schemaVersion = WIRED_ATMOSPHERE_SCHEMA_VERSION;
	atmosphere.flags = ATMOSPHERE_FLAG_ENABLED | ATMOSPHERE_FLAG_SKY_LIGHTING;
	atmosphere.qualityTier = ATMOSPHERE_QUALITY_FULL;
	atmosphere.seed = 239;
	atmosphere.timelineSeconds = 1.0f;
	for ( unsigned c = 0; c < 3; ++c ) {
		atmosphere.bounds[c] = -64; atmosphere.bounds[c + 3] = 64;
	}
	atmosphere.indoorExposure = 1;
	atmosphere.skyEnvironment.schemaVersion = WIRED_SKY_ENVIRONMENT_SCHEMA_VERSION;
	atmosphere.skyEnvironment.mode = ATMOSPHERE_SKY_AUTHORED;
	atmosphere.skyEnvironment.sourceRevision = 1;
	atmosphere.skyEnvironment.flags = ATMOSPHERE_SKY_AUTHORED_AVAILABLE;
	atmosphere.skyEnvironment.authoredSkyIdentity = 239227;
	atmosphere.skyEnvironment.sunDirection[2] = 1;
	atmosphere.skyEnvironment.moonDirection[2] = -1;
	atmosphere.skyEnvironment.skyLuminance = 1;
	atmosphere.skyEnvironment.exposure = 1;
	atmosphere.skyEnvironment.opticsVisibility = 1;
	atmosphere.skyEnvironment.distantSceneScale = 1;
	CHECK( RenderSubmission_SetAtmosphere( &frontend, &atmosphere ) );
	for ( uint64_t frame = 0; frame < 16; ++frame ) {
		const uint64_t generation = 24 + frame * 2;
		CHECK( RenderSubmission_BeginFrame( &frontend, generation ) );
		CHECK( RenderSubmission_RenderScene( &frontend, &view, 0 ) );
		CHECK( RalMetal_PresentAcquire( present, &coreReceipt, &layer, generation - 1, &drawable ) );
		CHECK( RalMetal_PresentRequestCapture( present ) );
		CHECK( RalMetal_PresentClearAndSubmit( present, &coreReceipt, &layer,
			&drawable, &frontend, black, generation, &receipt ) );
		CHECK( RenderSubmission_EndFrame( &frontend, generation, &submission ) );
	}
	auto checkSource = [&]( bool expectGreen ) -> int {
	id<MTLTexture> source = RalMetal_PresentNativeSkySource( present );
	CHECK( source && source.arrayLength == 6 && source.pixelFormat == MTLPixelFormatRGBA16Float );
	id<MTLBuffer> readback = [RalMetal_CoreNativeDevice( core ) newBufferWithLength:6 * 256
		options:MTLResourceStorageModeShared];
	id<MTLCommandBuffer> command = [RalMetal_CoreNativeQueue( core ) commandBuffer];
	id<MTLBlitCommandEncoder> blit = [command blitCommandEncoder];
	CHECK( readback && command && blit );
	for ( unsigned face = 0; face < 6; ++face )
		[blit copyFromTexture:source sourceSlice:face sourceLevel:0
			sourceOrigin:MTLOriginMake( source.width / 2, source.height / 2, 0 )
			sourceSize:MTLSizeMake( 1, 1, 1 ) toBuffer:readback destinationOffset:face * 256
			destinationBytesPerRow:256 destinationBytesPerImage:256];
	[blit endEncoding]; [command commit]; [command waitUntilCompleted];
	CHECK( command.status == MTLCommandBufferStatusCompleted );
	for ( unsigned face = 0; face < 6; ++face ) {
		const __fp16 *pixel = (const __fp16 *)( (const byte *)readback.contents + face * 256 );
		fprintf( stderr, "sky material capture face=%u rgb=%g,%g,%g\n",
			face, (double)pixel[0], (double)pixel[1], (double)pixel[2] );
		/* Stage textures are sRGB; the capture is linear HDR, before tone mapping. */
		const float linearRed = powf( ( 64.0f / 255.0f + 0.055f ) / 1.055f, 2.4f );
		const float linearGreen = expectGreen
			? powf( ( 96.0f / 255.0f + 0.055f ) / 1.055f, 2.4f ) : 0.0f;
		const float linearBlue = expectGreen ? 0.0f
			: powf( ( 128.0f / 255.0f + 0.055f ) / 1.055f, 2.4f );
		CHECK( fabsf( (float)pixel[0] - linearRed ) < 0.002f
			&& fabsf( (float)pixel[1] - linearGreen ) < 0.002f
			&& fabsf( (float)pixel[2] - linearBlue ) < 0.002f );
	}
	[readback release];
	return 0;
	};
	CHECK( checkSource( true ) == 0 );
	/* No source-revision changes: time alone must refresh the environment.
	 * During the first recapture the live animMap moves on to its green frame;
	 * all six faces must nevertheless retain the cohort's blue 2s sample. */
	for ( unsigned phase = 0; phase < 2; ++phase ) {
		for ( unsigned frame = 0; frame < 16; ++frame ) {
			view.time = phase ? 3500 : frame ? 3000 : 2000;
			atmosphere.timelineSeconds = (float)view.time * 0.001f;
			CHECK( RenderSubmission_SetAtmosphere( &frontend, &atmosphere ) );
			const uint64_t generation = 60 + phase * 40 + frame * 2;
			CHECK( RenderSubmission_BeginFrame( &frontend, generation ) );
			CHECK( RenderSubmission_RenderScene( &frontend, &view, 0 ) );
			CHECK( RalMetal_PresentAcquire( present, &coreReceipt, &layer, generation - 1, &drawable ) );
			CHECK( RalMetal_PresentRequestCapture( present ) );
			CHECK( RalMetal_PresentClearAndSubmit( present, &coreReceipt, &layer,
				&drawable, &frontend, black, generation, &receipt ) );
			CHECK( RenderSubmission_EndFrame( &frontend, generation, &submission ) );
		}
		ralSkyGpuTelemetryReceipt_t telemetry;
		CHECK( RalMetal_PresentSkyGpuTelemetry( present, &telemetry ) );
		CHECK( telemetry.currentRadianceRevision == phase + 2u
			&& telemetry.targetRadianceRevision == telemetry.currentRadianceRevision );
		fprintf( stderr, "animated sky phase=%u current=%llu target=%llu completed=0x%x\n", phase,
			(unsigned long long)telemetry.currentRadianceRevision,
			(unsigned long long)telemetry.targetRadianceRevision, telemetry.completedSliceMask );
		CHECK( checkSource( phase != 0 ) == 0 );
	}
	CHECK( RalMetal_PresentUnloadSkyRadianceResources( present ) );
	CHECK( !RalMetal_PresentNativeSkySource( present ) );
	/* Compare the visible physical sky against its actual IBL source, not
	 * another CPU sky model. Match the cube's 128x128 texel centers exactly;
	 * GL +X coordinates transpose the +X camera's top-left image coordinates;
	 * the screenshot API publishes bottom-up RGB. */
	RalMetal_PresentDestroy( present ); present = NULL;
	create.desiredWidth = create.desiredHeight = 128u; // offscreen, no window
	CHECK( RalMetal_PresentCreate( core, &coreReceipt, &create, 2u, &present, &layer ) );
	CHECK( RalMetal_PresentSetToneMap( present, 0u, 1.0f, 1.6f, 0.977f, 0.18f, 0.267f, 8.0f ) );
	view.width = view.height = 128;
	atmosphere.qualityTier = ATMOSPHERE_QUALITY_FULL;
	atmosphere.mediaDensity = atmosphere.visibility = atmosphere.cloudCover = 0.0f;
	memset( atmosphere.precipitation, 0, sizeof( atmosphere.precipitation ) );
	atmosphere.skyEnvironment.mode = ATMOSPHERE_SKY_PHYSICAL;
	atmosphere.skyEnvironment.sourceRevision++;
	atmosphere.skyEnvironment.sunRadiance[0] = 2.0f;
	atmosphere.skyEnvironment.sunRadiance[1] = 1.0f;
	atmosphere.skyEnvironment.sunRadiance[2] = 0.5f;
	atmosphere.skyEnvironment.exposure = 2.0f;
	CHECK( RenderSubmission_SetAtmosphere( &frontend, &atmosphere ) );
	for ( unsigned frame = 0; frame < 32; ++frame ) {
		const uint64_t generation = 140 + frame * 2;
		CHECK( RenderSubmission_BeginFrame( &frontend, generation ) );
		CHECK( RenderSubmission_RenderScene( &frontend, &view, 0 ) );
		CHECK( RalMetal_PresentAcquire( present, &coreReceipt, &layer, generation - 1, &drawable ) );
		CHECK( RalMetal_PresentRequestCapture( present ) );
		CHECK( RalMetal_PresentClearAndSubmit( present, &coreReceipt, &layer,
			&drawable, &frontend, black, generation, &receipt ) );
		CHECK( receipt.atmosphere.skyEnvironment.selectedMode == RAL_SKY_ENVIRONMENT_PHYSICAL &&
			receipt.atmosphereDispatchCount == 0u && receipt.atmosphereFroxelCount == 0u );
		CHECK( RenderSubmission_EndFrame( &frontend, generation, &submission ) );
	}
	{
		uint32_t width, height;
		const byte *visible = RalMetal_PresentCaptureRgb( present, &width, &height );
		id<MTLTexture> source = RalMetal_PresentNativeSkySource( present );
		CHECK( visible && width == 128 && height == 128 && source
			&& source.width == width && source.height == height );
		id<MTLBuffer> readback = [RalMetal_CoreNativeDevice( core )
			newBufferWithLength:128 * 128 * 8 options:MTLResourceStorageModeShared];
		id<MTLCommandBuffer> command = [RalMetal_CoreNativeQueue( core ) commandBuffer];
		id<MTLBlitCommandEncoder> blit = [command blitCommandEncoder];
		CHECK( readback && command && blit );
		[blit copyFromTexture:source sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake( 0, 0, 0 )
			sourceSize:MTLSizeMake( 128, 128, 1 ) toBuffer:readback destinationOffset:0
			destinationBytesPerRow:128 * 8 destinationBytesPerImage:128 * 128 * 8];
		[blit endEncoding]; [command commit]; [command waitUntilCompleted];
		CHECK( command.status == MTLCommandBufferStatusCompleted );
		const __fp16 *pixels = (const __fp16 *)readback.contents;
		int maxError = 0;
		for ( unsigned y = 40; y < 88; ++y ) for ( unsigned x = 40; x < 88; ++x )
			for ( unsigned c = 0; c < 3; ++c ) {
				const float linear = fmaxf( 0, (float)pixels[( x * 128 + 127 - y ) * 4 + c] );
				/* Disabled tonemap + UNORM output stores linear values. */
				const int expected = (int)roundf( fminf( 1, linear ) * 255 );
				maxError = std::max( maxError, abs( expected - (int)visible[( y * 128 + x ) * 3 + c] ) );
			}
		fprintf( stderr, "physical sky visible/source 48x48 RGB maxError=%d\n", maxError );
		[readback release];
		CHECK( maxError <= 2 );
	}
	/* Resume the independent registration-retirement checks at their original
	 * output size; the physical comparison above changes no asset ownership. */
	RalMetal_PresentDestroy( present ); present = NULL;
	create.desiredWidth = create.desiredHeight = 16u;
	CHECK( RalMetal_PresentCreate( core, &coreReceipt, &create, 2u, &present, &layer ) );
	CHECK( !RalMetal_PresentResetRegistrationAssets( present, &frontend ) );
	CHECK( RalMetal_PresentUnloadWorld( present, 0 ) );
	CHECK( RenderSubmission_UnloadWorld( &frontend, 0 ) );
	CHECK( RalMetal_PresentResetRegistrationAssets( present, &frontend ) );
	qhandle_t previousMaterial = material;
	for ( unsigned cycle = 0; cycle < 4; ++cycle ) {
		NSPointerArray *textures = [[NSPointerArray alloc]
			initWithOptions:NSPointerFunctionsWeakMemory];
		@autoreleasepool {
		const byte pixel[4] = { (byte)( cycle & 1 ? 0 : 255 ),
			(byte)( cycle & 1 ? 255 : 0 ), 0, 255 };
		const qhandle_t nextMaterial = RenderSubmission_RegisterMaterialImage( &frontend,
			RENDER_ASSET_MATERIAL, shader.shader, qtrue, pixel, 1, 1 );
		CHECK( nextMaterial > previousMaterial );
		ralTextureCacheKey_t key;
		CHECK( Ral_MaterialRuntimeTextureKeyBuild( 717u, cycle + 1u, pixel,
			1, 1, qtrue, qtrue, RAL_TEXTURE_RESIDENCY_PINNED, &key ) );
		CHECK( RenderSubmission_SetMaterialTextureCacheKey( &frontend, nextMaterial, &key ) );
		const uint64_t generation = 400u + cycle * 2u;
		CHECK( RenderSubmission_BeginFrame( &frontend, generation ) );
		CHECK( RenderSubmission_AddUiQuad( &frontend, 0, 0, 16, 16,
			0, 0, 1, 1, 0, nextMaterial ) );
		CHECK( !RalMetal_PresentResetRegistrationAssets( present, &frontend ) );
		CHECK( RalMetal_PresentAcquire( present, &coreReceipt, &layer,
			generation - 1u, &drawable ) );
		CHECK( RalMetal_PresentRequestCapture( present ) );
		CHECK( RalMetal_PresentClearAndSubmit( present, &coreReceipt, &layer,
			&drawable, &frontend, black, generation, &receipt ) );
		uint32_t width, height;
		const byte *rgb = RalMetal_PresentCaptureRgb( present, &width, &height );
		CHECK( rgb && width == 16 && height == 16 );
		for ( unsigned i = 0; i < 16 * 16; ++i )
			for ( unsigned channel = 0; channel < 3; ++channel )
				CHECK( abs( (int)rgb[i * 3 + channel] - pixel[channel] ) <= 1 );
		ralMaterialRuntimeResource_t resource;
		CHECK( RalMetal_PresentResolveMaterialRuntimeResource( present, key.keyHash, &resource ) );
		[textures addPointer:(void *)resource.resource.textureIdentity];
		CHECK( RenderSubmission_EndFrame( &frontend, generation, &submission ) );
		CHECK( RalMetal_PresentResetRegistrationAssets( present, &frontend ) );
		CHECK( !frontend.materialBytes && !frontend.materialCount
			&& !frontend.stageTextureBytes && !frontend.stageTextureCount );
		previousMaterial = nextMaterial;
		}
		CHECK( textures.count == 1 && [textures pointerAtIndex:0] == NULL );
		[textures release];
	}
	RalMetal_PresentDestroy( present ); RenderSubmission_Reset( &frontend );
	RalMetal_CoreDestroy( core );
	return 0;
}

static int LateAcquireTransaction( renderSubmissionState_t *frontend,
		const ralSwapchainCreateInfo_t *create, qboolean fullWeather = qfalse,
		uint32_t volumeCount = 0u ) {
	const atmosphereFrameState_t savedAtmosphere = frontend->atmosphere;
	const uint32_t savedVolumes = frontend->atmosphereMediaVolumeCount;
	const uint64_t savedMediaDigest = frontend->atmosphereMediaDigest;
	CHECK( savedVolumes == 0u );
	if ( fullWeather ) {
		atmosphereFrameState_t atmosphere = savedAtmosphere;
		atmosphere.flags |= ATMOSPHERE_FLAG_ENABLED | ATMOSPHERE_FLAG_VOLUMETRIC_MEDIA;
		atmosphere.qualityTier = ATMOSPHERE_QUALITY_FULL;
		atmosphere.precipitation[1] = 0.5f; atmosphere.precipitation[2] = 0.5f;
		atmosphere.timelineSeconds = 1.0f;
		CHECK( RenderSubmission_SetAtmosphere( frontend, &atmosphere ) );
		for ( uint32_t i = 0; i < volumeCount; ++i ) {
			atmosphereMediaVolume_t volume = {};
			volume.schemaVersion = WIRED_ATMOSPHERE_MEDIA_VOLUME_SCHEMA_VERSION;
			volume.shape = ATMOSPHERE_MEDIA_VOLUME_SPHERE;
			volume.id = i + 1u; volume.radius = 32.0f;
			volume.extinction = 0.001f;
			volume.albedo[0] = volume.albedo[1] = volume.albedo[2] = 0.5f;
			CHECK( RenderSubmission_AddAtmosphereMediaVolume( frontend, &volume ) );
		}
	}
	const uint32_t savedUi = frontend->uiPrimitiveCount;
	const uint32_t savedIrradiance = frontend->localIrradianceEntityCount;
	std::vector<byte> expected;
	frontend->uiPrimitiveCount = 0u; // The pixel comparison must not be hidden by UI.
	for ( unsigned route = 0u; route < 5u; ++route ) {
		@autoreleasepool {
			ralMetalCoreCreateInfo_t info = { 1u };
			ralMetalCore_t *core = NULL;
			ralMetalCoreReceipt_t coreReceipt;
			ralMetalPresent_t *present = NULL;
			ralMetalPresentLayerReceipt_t layerReceipt;
			ralMetalDrawableReceipt_t drawable, drawableBefore;
			ralMetalPresentReceipt_t receipt, before;
			const float black[4] = { 0, 0, 0, 1 };
			ralMetalSimulationHistory_t history, emptyHistory = {};
			ralMetalSubmitTiming_t submitTiming, timingBefore;
			memset( &submitTiming, 0x5a, sizeof( submitTiming ) ); timingBefore = submitTiming;
			CHECK( RalMetal_CoreCreate( &info, &core, &coreReceipt ) );
			AcquireProbeLayer *layer = [[AcquireProbeLayer alloc] init];
			CHECK( RalMetal_PresentAdoptBorrowedLayer( core, &coreReceipt, create,
				2u, layer, &present, &layerReceipt ) );
			FailedOutputEncoderProbe outputProbe( RalMetal_CoreNativeQueue( core ) );
			wiredMaterialUniformProbe uniformProbe( RalMetal_CoreNativeDevice( core ) );
			layer.observeScene = present; layer.observationCore = core;
			const ralExposureSettings_t exposure = { 1, 0.0123f, 0.5f, 0.2f, 1.5f, 0.5f, 0.25f, 8.0f };
			CHECK( RalMetal_PresentSetExposure( present, &exposure ) );
			CHECK( RalMetal_PresentSimulationHistory( present, &emptyHistory ) );
			memset( &drawable, 0x5a, sizeof( drawable ) ); drawableBefore = drawable;
			memset( &receipt, 0x5a, sizeof( receipt ) ); before = receipt;
			CHECK( RalMetal_PresentRequestCapture( present ) );
			if ( route == 1u ) {
				failMaterialUniformAllocations = 3u;
				for ( unsigned retry = 0u; retry < 3u; ++retry ) {
					CHECK( RalMetal_PresentRenderAndSubmit( present, &coreReceipt,
						&layerReceipt, frontend, black, 3u, 75u, &drawable, &receipt,
						&submitTiming ) == RAL_METAL_SUBMIT_FAILED );
					CHECK( layer.acquireCount == 0u );
					CHECK( !memcmp( &drawable, &drawableBefore, sizeof( drawable ) )
						&& !memcmp( &receipt, &before, sizeof( receipt ) )
						&& !memcmp( &submitTiming, &timingBefore, sizeof( submitTiming ) ) );
					CHECK( RalMetal_PresentSimulationHistory( present, &history ) );
					CHECK( !memcmp( &history, &emptyHistory, sizeof( history ) ) );
				}
				CHECK( failMaterialUniformAllocations == 0u && materialUniformBuffers.count == 0u );
			}
			if ( route == 2u || route == 4u ) {
				FailedFrameEntityBufferProbe bufferProbe( RalMetal_CoreNativeDevice( core ) );
				frontend->localIrradianceEntityCount = savedIrradiance + 1u;
				for ( unsigned attempt = 0u; attempt < 17u; ++attempt ) {
					CHECK( RalMetal_PresentRenderAndSubmit( present, &coreReceipt,
						&layerReceipt, frontend, black, 3u, 75u, &drawable, &receipt,
						&submitTiming ) == RAL_METAL_SUBMIT_FAILED );
					CHECK( layer.acquireCount == 0u );
					CHECK( !memcmp( &drawable, &drawableBefore, sizeof( drawable ) )
						&& !memcmp( &receipt, &before, sizeof( receipt ) )
						&& !memcmp( &submitTiming, &timingBefore, sizeof( submitTiming ) ) );
				}
				frontend->localIrradianceEntityCount = savedIrradiance;
				layer.returnNoDrawable = route == 2u;
				layer.failOutputEncoder = route == 4u;
				ralMetalSimulationHistory_t partialHistory = {};
				for ( unsigned attempt = 0u; attempt < 17u; ++attempt ) {
					CHECK( RalMetal_PresentRenderAndSubmit( present, &coreReceipt,
						&layerReceipt, frontend, black, 3u, 75u, &drawable, &receipt,
						&submitTiming ) == ( attempt || route == 4u ? RAL_METAL_SUBMIT_FAILED
							: RAL_METAL_SUBMIT_DRAWABLE_UNAVAILABLE ) );
					CHECK( layer.acquireCount == 1u );
					CHECK( RalMetal_PresentSimulationHistory( present, &history ) );
					if ( !attempt ) {
						CHECK( history.particlePlanReady && history.particleNextSlot > 0u );
						CHECK( layer.sceneCommittedBeforeAcquire );
						if ( fullWeather ) CHECK( layer.weatherWrittenBeforeAcquire );
						CHECK( failedFrameEntityBuffers.count > 0u );
						CHECK( materialUniformBuffers.count == 1u
							&& [materialUniformBuffers pointerAtIndex:0u] == NULL );
						for ( NSUInteger i = 0u; i < failedFrameEntityBuffers.count; ++i )
							CHECK( [failedFrameEntityBuffers pointerAtIndex:i] == NULL );
						CHECK( memcmp( &history, &emptyHistory, sizeof( history ) ) );
						partialHistory = history;
					} else CHECK( !memcmp( &history, &partialHistory, sizeof( history ) ) );
					CHECK( !memcmp( &drawable, &drawableBefore, sizeof( drawable ) )
						&& !memcmp( &receipt, &before, sizeof( receipt ) )
						&& !memcmp( &submitTiming, &timingBefore, sizeof( submitTiming ) ) );
				}
				layer.returnNoDrawable = NO;
				/* This generation executed its scene; it is canceled, not replayed.
				 * Fresh submissions still work after the partial-submit drain. */
				CHECK( RalMetal_PresentRenderAndSubmit( present, &coreReceipt, &layerReceipt,
					frontend, black, 4u, 76u, &drawable, &receipt, &submitTiming )
					== RAL_METAL_SUBMIT_PRESENTED );
				CHECK( receipt.presented && layer.acquireCount == 2u );
				fprintf( stderr, "partial-submit route=%u: scene executed, transient buffers released, replay rejected, next generation recovered\n", route );
				RalMetal_PresentDestroy( present ); [layer release]; RalMetal_CoreDestroy( core );
				continue;
			}
			if ( route == 3u ) {
				layer.publishCore = core;
				CHECK( RalMetal_PresentRenderAndSubmit( present, &coreReceipt, &layerReceipt,
					frontend, black, 3u, 75u, &drawable, &receipt, &submitTiming )
					== RAL_METAL_SUBMIT_FAILED );
				CHECK( layer.publishedInterveningCommand && layer.acquireCount == 1u );
				CHECK( !memcmp( &drawable, &drawableBefore, sizeof( drawable ) )
					&& !memcmp( &receipt, &before, sizeof( receipt ) )
					&& !memcmp( &submitTiming, &timingBefore, sizeof( submitTiming ) ) );
				CHECK( RalMetal_PresentSimulationHistory( present, &history ) );
				CHECK( history.particleParity && history.particlePlanReady
					&& history.particleNextSlot > 0u
					&& history.particleTimeline == (float)frontend->worldSnapshot.timeMs * 0.001f );
				if ( fullWeather ) CHECK( history.weatherParity && history.atmosphereValid
					&& history.atmosphereParity && history.weatherTimeline == 1.0f );
				/* The GPU commit consumed generation 75 even though receipt
				 * publication failed. A fresh acquire token must not replay it. */
				const ralMetalSimulationHistory_t committedHistory = history;
				for ( unsigned attempt = 0u; attempt < 3u; ++attempt ) {
					CHECK( RalMetal_PresentRenderAndSubmit( present, &coreReceipt, &layerReceipt,
						frontend, black, 4u, 75u, &drawable, &receipt, &submitTiming )
						== RAL_METAL_SUBMIT_FAILED );
					CHECK( layer.acquireCount == 1u );
					CHECK( RalMetal_PresentSimulationHistory( present, &history ) );
					CHECK( !memcmp( &history, &committedHistory, sizeof( history ) ) );
					CHECK( !memcmp( &drawable, &drawableBefore, sizeof( drawable ) )
						&& !memcmp( &receipt, &before, sizeof( receipt ) )
						&& !memcmp( &submitTiming, &timingBefore, sizeof( submitTiming ) ) );
				}
				/* The explicit-acquire entry point obeys the same commit cursor.
				 * Its drawable remains owned by the caller after rejection and
				 * is reusable by a genuinely new submission, not poisoned. */
				CHECK( RalMetal_PresentAcquire( present, &coreReceipt, &layerReceipt, 4u, &drawable ) );
				CHECK( !RalMetal_PresentClearAndSubmit( present, &coreReceipt, &layerReceipt,
					&drawable, frontend, black, 75u, &receipt ) );
				CHECK( !memcmp( &receipt, &before, sizeof( receipt ) ) );
				CHECK( RalMetal_PresentSimulationHistory( present, &history ) );
				CHECK( !memcmp( &history, &committedHistory, sizeof( history ) ) );
				CHECK( RalMetal_PresentClearAndSubmit( present, &coreReceipt, &layerReceipt,
					&drawable, frontend, black, 76u, &receipt ) );
				CHECK( receipt.presented && receipt.presentGeneration == 76u );
				fprintf( stderr, "post-submit receipt failure: particle scheduling state retained\n" );
				RalMetal_PresentDestroy( present ); [layer release]; RalMetal_CoreDestroy( core );
				continue;
			}
			if ( route == 0u ) {
				CHECK( RalMetal_PresentAcquire( present, &coreReceipt, &layerReceipt, 3u, &drawable ) );
				CHECK( RalMetal_PresentClearAndSubmit( present, &coreReceipt, &layerReceipt,
					&drawable, frontend, black, 75u, &receipt ) );
			} else {
				CHECK( RalMetal_PresentRenderAndSubmit( present, &coreReceipt, &layerReceipt,
					frontend, black, 3u, 75u, &drawable, &receipt, &submitTiming )
					== RAL_METAL_SUBMIT_PRESENTED );
				CHECK( submitTiming.totalNs > 0u );
				CHECK( submitTiming.prepareNs <= submitTiming.totalNs
					&& submitTiming.encodeBeforeAcquireNs <= submitTiming.totalNs
					&& submitTiming.acquireNs <= submitTiming.totalNs
					&& submitTiming.encodeAfterAcquireNs <= submitTiming.totalNs
					&& submitTiming.submitNs <= submitTiming.totalNs
					&& submitTiming.retireNs <= submitTiming.totalNs );
				CHECK( submitTiming.prepareNs + submitTiming.encodeBeforeAcquireNs
					+ submitTiming.acquireNs + submitTiming.encodeAfterAcquireNs
					+ submitTiming.submitNs + submitTiming.retireNs == submitTiming.totalNs );
			}
			CHECK( layer.acquireCount == 1u );
			ralMetalGpuTiming_t gpuSplit = {}, gpuSplitAgain = {};
			CHECK( RalMetal_PresentGpuTiming( present, &gpuSplit ) );
			CHECK( gpuSplit.splitSampleCount == ( route ? gpuSplit.sampleCount : 0u ) );
			CHECK( gpuSplit.acquireSampleCount == gpuSplit.splitSampleCount
				&& gpuSplit.acquireUnavailableCount == 0u );
			const uint64_t acquirePartition = gpuSplit.acquireBeforeSceneNanoseconds
				+ gpuSplit.acquireDuringSceneNanoseconds + gpuSplit.acquireAfterSceneNanoseconds;
			CHECK( acquirePartition <= gpuSplit.acquireDurationNanoseconds + 2u * gpuSplit.acquireSampleCount
				&& gpuSplit.acquireDurationNanoseconds <= acquirePartition + 2u * gpuSplit.acquireSampleCount );
			if ( gpuSplit.acquireSampleCount ) CHECK( gpuSplit.acquireDurationNanoseconds > 0u );
			CHECK( gpuSplit.overlapNanoseconds <= gpuSplit.sceneNanoseconds
				&& gpuSplit.overlapNanoseconds <= gpuSplit.outputNanoseconds );
			if ( gpuSplit.splitSampleCount ) {
				CHECK( gpuSplit.sceneNanoseconds > 0u && gpuSplit.outputNanoseconds > 0u );
				const uint64_t unionNs = gpuSplit.sceneNanoseconds + gpuSplit.outputNanoseconds
					- gpuSplit.overlapNanoseconds;
				CHECK( unionNs <= gpuSplit.durationNanoseconds + 3u * gpuSplit.splitSampleCount
					&& gpuSplit.durationNanoseconds <= unionNs + 3u * gpuSplit.splitSampleCount );
			} else {
				CHECK( !gpuSplit.sceneNanoseconds && !gpuSplit.outputNanoseconds
					&& !gpuSplit.overlapNanoseconds && !gpuSplit.gapNanoseconds );
			}
			CHECK( RalMetal_PresentGpuTiming( present, &gpuSplitAgain )
				&& !memcmp( &gpuSplit, &gpuSplitAgain, sizeof( gpuSplit ) ) );
			CHECK( materialUniformBuffers.count == 1u );
			CHECK( layer.sceneCommittedBeforeAcquire == ( route != 0u ) );
			CHECK( RalMetal_PresentSimulationHistory( present, &history ) );
			if ( fullWeather ) {
				CHECK( receipt.atmosphereDispatchCount > 0u && receipt.weatherDispatchCount == 1u
					&& receipt.weatherDrawCount == 1u && receipt.weather.activeParticleCount > 0u
					&& receipt.atmosphere.admittedVolumeCount == volumeCount );
				CHECK( history.atmosphereValid && history.atmosphereParity && history.weatherParity
					&& history.weatherTimeline == 1.0f );
				if ( route == 1u ) {
					CHECK( layer.weatherWrittenBeforeAcquire );
					id<MTLBuffer> buffer = RalMetal_PresentNativeWeatherBuffer( present );
					const byte *bytes = (const byte *)buffer.contents;
					uint64_t hash = UINT64_C(1469598103934665603);
					for ( NSUInteger i = 0u; i < buffer.length; ++i ) {
						hash ^= bytes[i]; hash *= UINT64_C(1099511628211);
					}
					CHECK( hash == layer.weatherHashBeforeAcquire );
				}
			}
			CHECK( RalMetal_DrawableReceiptExact( &drawable, &drawable ) );
			/* A later compute consumer must not sample the particle pass's
			 * discarded depth attachment. Native GPUs may leave plausible bytes
			 * there, so observe the actual binding rather than trusting pixels. */
			CHECK( receipt.lensVisibilityDispatchCount == 1u && receipt.effectParticleDrawCount > 0u );
			CHECK( wiredDiscardedDepthReads == 0u );
			CHECK( drawable.drawableIdentity == receipt.drawableIdentity
				&& drawable.textureIdentity == receipt.textureIdentity );
			CHECK( receipt.loweredEffectDecalCount == 1u && receipt.loweredEffectBeamCount == 1u
				&& receipt.loweredUiPrimitiveCount == 0u
				&& receipt.effectEmitterDispatchCount == 1u );
			uint32_t width, height;
			const byte *rgb = RalMetal_PresentCaptureRgb( present, &width, &height );
			CHECK( rgb && width == create->desiredWidth && height == create->desiredHeight );
			const size_t bytes = (size_t)width * height * 3u;
			if ( route == 0u ) {
				expected.assign( rgb, rgb + bytes );
				bool visible = false;
				for ( byte value : expected ) visible |= value != 0u;
				CHECK( visible );
			} else CHECK( expected.size() == bytes && !memcmp( expected.data(), rgb, bytes ) );
			if ( route == 1u && !fullWeather ) {
				/* Completed packets must be reused without an allocation. Keep an
				 * injected allocation failure armed: any hidden allocation fails. */
				layer.observeScene = NULL;
				const uint32_t decalBirths = frontend->effectDecalCount;
				const uint32_t beamBirths = frontend->effectBeamCount;
				/* Keep resident FX, but do not add new births every repeated frame.
				 * Capacity growth is exercised separately below, not called reuse. */
				frontend->effectDecalCount = frontend->effectBeamCount = 0u;
				failMaterialUniformAllocations = 1u;
				for ( unsigned repeat = 0u; repeat < 8u; ++repeat ) {
					CHECK( RalMetal_PresentRequestCapture( present ) );
					CHECK( RalMetal_PresentRenderAndSubmit( present, &coreReceipt,
						&layerReceipt, frontend, black, 4u + repeat, 76u + repeat,
						&drawable, &receipt, &submitTiming ) == RAL_METAL_SUBMIT_PRESENTED );
					CHECK( materialUniformBuffers.count == 1u
						&& failMaterialUniformAllocations == 1u );
					const byte *reusedRgb = RalMetal_PresentCaptureRgb( present, &width, &height );
					CHECK( reusedRgb && width == create->desiredWidth && height == create->desiredHeight
						&& !memcmp( expected.data(), reusedRgb, bytes ) );
				}
				failMaterialUniformAllocations = 0u;
				/* New FX grows the required packet count. A failed growth leaves
				 * the old completed cache intact; the same generation can retry. */
				frontend->effectDecalCount = decalBirths;
				frontend->effectBeamCount = beamBirths;
				CHECK( RalMetal_PresentRequestCapture( present ) );
				failMaterialUniformAllocations = 1u;
				CHECK( RalMetal_PresentRenderAndSubmit( present, &coreReceipt,
					&layerReceipt, frontend, black, 12u, 84u, &drawable, &receipt,
					&submitTiming ) == RAL_METAL_SUBMIT_FAILED );
				CHECK( failMaterialUniformAllocations == 0u && materialUniformBuffers.count == 1u );
				CHECK( RalMetal_PresentRenderAndSubmit( present, &coreReceipt,
					&layerReceipt, frontend, black, 12u, 84u, &drawable, &receipt,
					&submitTiming ) == RAL_METAL_SUBMIT_PRESENTED );
				CHECK( materialUniformBuffers.count == 2u );
				CHECK( [materialUniformBuffers pointerAtIndex:0u] == NULL );
				frontend->effectDecalCount = frontend->effectBeamCount = 0u;
				id<MTLCommandQueue> queue = RalMetal_CoreNativeQueue( core );
				wiredUniformQueueGate gate( RalMetal_CoreNativeDevice( core ) );
				CHECK( gate.event );
				id<MTLCommandBuffer> blocked = [queue commandBuffer];
				[blocked encodeWaitForEvent:gate.event value:1u]; [blocked commit];
				CHECK( RalMetal_PresentRenderAndSubmit( present, &coreReceipt,
					&layerReceipt, frontend, black, 13u, 85u, &drawable, &receipt,
					&submitTiming ) == RAL_METAL_SUBMIT_PRESENTED );
				CHECK( materialUniformBuffers.count == 2u );
				std::vector<byte> pendingPacket;
				@autoreleasepool {
					id<MTLBuffer> pending = (id<MTLBuffer>)[materialUniformBuffers pointerAtIndex:1u];
					CHECK( pending && pending.length >= 4096u );
					const byte *data = (const byte *)pending.contents;
					pendingPacket.assign( data, data + 4096u );
				}
				CHECK( RalMetal_PresentSetShaderTimeOverride( present, 2.0f ) );
				/* Busy storage cannot be overwritten even when fresh allocation
				 * fails. This failure remains pre-commit and the generation retries. */
				failMaterialUniformAllocations = 1u;
				CHECK( RalMetal_PresentRenderAndSubmit( present, &coreReceipt,
					&layerReceipt, frontend, black, 14u, 86u, &drawable, &receipt,
					&submitTiming ) == RAL_METAL_SUBMIT_FAILED );
				CHECK( failMaterialUniformAllocations == 0u && materialUniformBuffers.count == 2u );
				CHECK( RalMetal_PresentRenderAndSubmit( present, &coreReceipt,
					&layerReceipt, frontend, black, 14u, 86u, &drawable, &receipt,
					&submitTiming ) == RAL_METAL_SUBMIT_PRESENTED );
				CHECK( materialUniformBuffers.count == 3u );
				@autoreleasepool {
					id<MTLBuffer> pending = (id<MTLBuffer>)[materialUniformBuffers pointerAtIndex:1u];
					id<MTLBuffer> newer = (id<MTLBuffer>)[materialUniformBuffers pointerAtIndex:2u];
					CHECK( pending && newer && pending != newer );
					CHECK( !memcmp( pending.contents, pendingPacket.data(), pendingPacket.size() ) );
					CHECK( memcmp( newer.contents, pendingPacket.data(), pendingPacket.size() ) );
				}
				gate.event.signaledValue = 1u;
				id<MTLCommandBuffer> complete = [queue commandBuffer];
				[complete commit]; [complete waitUntilCompleted];
				CHECK( complete.status == MTLCommandBufferStatusCompleted );
				@autoreleasepool {
					/* Weak loads may autorelease a temporary strong reference. */
					CHECK( [materialUniformBuffers pointerAtIndex:1u] != NULL );
				}
				/* Both completed packets survive. Block two new submissions and
				 * forbid allocation: each must reuse a distinct completed entry. */
				wiredUniformQueueGate secondGate( RalMetal_CoreNativeDevice( core ) );
				CHECK( secondGate.event );
				id<MTLCommandBuffer> secondBlock = [queue commandBuffer];
				[secondBlock encodeWaitForEvent:secondGate.event value:1u]; [secondBlock commit];
				failMaterialUniformAllocations = 1u;
				for ( unsigned repeat = 0u; repeat < 2u; ++repeat ) {
					CHECK( RalMetal_PresentRenderAndSubmit( present, &coreReceipt,
						&layerReceipt, frontend, black, 15u + repeat, 87u + repeat,
						&drawable, &receipt, &submitTiming ) == RAL_METAL_SUBMIT_PRESENTED );
					CHECK( failMaterialUniformAllocations == 1u && materialUniformBuffers.count == 3u );
				}
				failMaterialUniformAllocations = 0u;
				secondGate.event.signaledValue = 1u;
				id<MTLCommandBuffer> secondComplete = [queue commandBuffer];
				[secondComplete commit]; [secondComplete waitUntilCompleted];
				CHECK( secondComplete.status == MTLCommandBufferStatusCompleted );
				/* Retire delayed diagnostic readbacks through the normal next-frame
				 * path before asserting that unloading drops the final CPU owners. */
				CHECK( RalMetal_PresentRequestCapture( present ) );
				CHECK( RalMetal_PresentRenderAndSubmit( present, &coreReceipt,
					&layerReceipt, frontend, black, 17u, 89u, &drawable, &receipt,
					&submitTiming ) == RAL_METAL_SUBMIT_PRESENTED );
				CHECK( materialUniformBuffers.count == 3u );
				/* Unloading retires both bounded CPU cache entries. */
				CHECK( RalMetal_PresentUnloadWorld( present, 0 ) );
				CHECK( [materialUniformBuffers pointerAtIndex:1u] == NULL );
				CHECK( [materialUniformBuffers pointerAtIndex:2u] == NULL );
				frontend->effectDecalCount = decalBirths;
				frontend->effectBeamCount = beamBirths;
				fprintf( stderr, "material uniforms: eight completed-frame and two blocked in-flight allocation-free reuses; growth and GPU-busy OOM/retry; pending packet immutable, both cache entries cleared on unload after readback retirement\n" );
			}
			RalMetal_PresentDestroy( present ); [layer release]; RalMetal_CoreDestroy( core );
		}
	}
	frontend->uiPrimitiveCount = savedUi;
	frontend->atmosphereMediaVolumeCount = savedVolumes;
	frontend->atmosphereMediaDigest = savedMediaDigest;
	CHECK( RenderSubmission_SetAtmosphere( frontend, &savedAtmosphere ) );
	fprintf( stderr, "late-acquire: full-weather=%d volumes=%u single/split full-frame RGB identical, partial-commit failures drained without replay\n", fullWeather, volumeCount );
	return 0;
}

/* A real GPU queue is held behind an event while the CPU prepares two
 * different weather frames. Its final pool must equal serialized execution;
 * a shared mutable input buffer makes both dispatches consume the last frame. */
/* A tier with no sky products must not inherit relighting from a previous
 * weather tier. Keep texture/material/camera/time fixed to isolate consumers
 * from fog, animated sky, exposure and weather particles. */
static int BeamAtmospherePixels() {
	ralMetalCoreCreateInfo_t info={1u}; ralMetalCore_t *core=NULL;
	ralMetalCoreReceipt_t coreReceipt; ralMetalPresent_t *present=NULL;
	ralMetalPresentLayerReceipt_t layer; ralMetalDrawableReceipt_t drawable;
	ralMetalPresentReceipt_t receipt; renderSubmissionReceipt_t submission;
	auto storage=std::make_unique<renderSubmissionState_t>(); auto &frontend=*storage;
	ralSurfaceFormat_t format={RAL_FORMAT_B8G8R8A8_UNORM,RAL_COLORSPACE_SRGB_NONLINEAR};
	ralPresentPreference_t fifo={RAL_PRESENT_FIFO,3u,3u};
	ralSwapchainCreateInfo_t create={};
	create.desiredWidth=80; create.desiredHeight=48; // offscreen compute/pixel fixture, no window
	create.formatPreferences=&format; create.formatPreferenceCount=1;
	create.presentPreferences=&fifo; create.presentPreferenceCount=1;
	create.requiredUsage=RAL_TEXTURE_USAGE_COLOR_ATTACHMENT;
	CHECK(RalMetal_CoreCreate(&info,&core,&coreReceipt));
	CHECK(RenderSubmission_Init(&frontend,1));
	CHECK(RalMetal_PresentCreate(core,&coreReceipt,&create,2,&present,&layer));
	CHECK(RalMetal_PresentSetToneMap(present,0,1,1.6f,.977f,.18f,.267f,8));
	refdef_t view={}; view.width=80; view.height=48; view.fov_x=view.fov_y=90;
	view.rdflags=RDF_NOWORLDMODEL|RDF_CAMERA_CUT;
	view.viewaxis[0][0]=view.viewaxis[1][1]=view.viewaxis[2][2]=1;
	atmosphereFrameState_t atmosphere={}; atmosphere.schemaVersion=WIRED_ATMOSPHERE_SCHEMA_VERSION;
	atmosphere.flags=ATMOSPHERE_FLAG_ENABLED|ATMOSPHERE_FLAG_SKY_LIGHTING;
	atmosphere.mediaDensity=.1f; atmosphere.sunDirection[2]=1; atmosphere.sunIntensity=1;
	for(unsigned c=0;c<3;++c)atmosphere.ambientColor[c]=.2f;
	const float black[4]={0,0,0,1}; uint64_t generation=4; unsigned frames=0,pixels=0;
	for(unsigned route=0;route<4;++route)for(unsigned emission=0;emission<2;++emission) {
		char name[64]; snprintf(name,sizeof(name),"test/beam-fog-%u-%u",route,emission);
		const byte value=emission?255:0, texel[4]={value,value,value,128};
		const qhandle_t material=RenderSubmission_RegisterMaterialImage(&frontend,
			RENDER_ASSET_MATERIAL,name,qtrue,texel,1,1); CHECK(material>0);
		if(route) {
			renderMaterialStageSnapshot_t stage={};
			stage.material=RenderSubmission_RegisterStageTexture(&frontend,
				RENDER_ASSET_MATERIAL,name,qtrue,texel,1,1); CHECK(stage.material>0);
			stage.sourceBlend=route==1?RENDER_MATERIAL_BLEND_ONE:RENDER_MATERIAL_BLEND_SRC_ALPHA;
			stage.destinationBlend=RENDER_MATERIAL_BLEND_ONE;
			stage.rgbGen=RAL_MATERIAL_COLOR_VERTEX; stage.alphaGen=RAL_MATERIAL_ALPHA_VERTEX;
			stage.scaleScroll[0]=stage.scaleScroll[1]=1;
			for(unsigned c=0;c<4;++c)stage.constantColor[c]=1;
			renderMaterialStageSnapshot_t stages[2]={stage,stage};
			CHECK(RenderSubmission_SetMaterialStages(&frontend,material,stages,route==3?2:1));
		}
		for(unsigned full=0;full<2;++full)for(unsigned opacity=0;opacity<2;++opacity) {
			atmosphere.qualityTier=full?ATMOSPHERE_QUALITY_FULL:ATMOSPHERE_QUALITY_ANALYTIC;
			atmosphere.visibility=full?64:0;
			CHECK(RenderSubmission_SetAtmosphere(&frontend,&atmosphere));
			beamDesc_t beam={}; beam.start[0]=beam.end[0]=8;
			beam.start[1]=-4; beam.end[1]=4; beam.startWidth=beam.endWidth=4;
			for(unsigned c=0;c<3;++c)beam.startColor[c]=beam.endColor[c]=1;
			beam.startColor[3]=beam.endColor[3]=opacity?1:.25f;
			beam.shader=material; beam.axialCopies=1;
			beam.startEntityNum=beam.endEntityNum=-1; beam.flags=PRIM_FLAG_ADDITIVE;
			view.time=1000+frames*16;
			CHECK(RenderSubmission_BeginFrame(&frontend,generation));
			CHECK(RenderSubmission_RenderScene(&frontend,&view,0));
			CHECK(RenderSubmission_AddEffectBeam(&frontend,&beam));
			CHECK(RalMetal_PresentAcquire(present,&coreReceipt,&layer,generation-1,&drawable));
			CHECK(RalMetal_PresentRequestCapture(present));
			CHECK(RalMetal_PresentClearAndSubmit(present,&coreReceipt,&layer,&drawable,
				&frontend,black,generation,&receipt));
			CHECK(receipt.loweredEffectBeamCount==1);
			CHECK(receipt.atmosphereDispatchCount==(full?4u:0u));
			if(full)CHECK(receipt.atmosphere.froxelDepth==64 && !receipt.atmosphere.temporalReuseCount);
			uint32_t width,height; const byte *rgb=RalMetal_PresentCaptureRgb(present,&width,&height);
			CHECK(rgb && width==80 && height==48);
			for(unsigned y:{20u,24u,28u})for(unsigned x:{32u,40u,48u}) {
				// Analytic uses world-space ray length at this pixel. FULL uses
				// forward depth 8 and the prefix through log slice 16, not ray length.
				const double wy=(1-2*(x+.5)/80)*8,wz=(1-2*(y+.5)/48)*8;
				const double distance=full?4*pow(16.0,17.0/64)-4:sqrt(64+wy*wy+wz*wz);
				const int expected=(int)lround(emission*128*beam.startColor[3]
					*(route==3?2:1)*exp(-.1*distance));
				for(unsigned c=0;c<3;++c) {
					const int actual=rgb[(y*width+x)*3+c];
					if(abs(actual-expected)>1)fprintf(stderr,"beam fog route=%u emission=%u full=%u opacity=%u pixel=%u,%u actual=%d expected=%d\n",
						route,emission,full,opacity,x,y,actual,expected);
					CHECK(abs(actual-expected)<=1);
				}
				++pixels;
			}
			CHECK(RenderSubmission_EndFrame(&frontend,generation,&submission));generation+=2;++frames;
		}
	}
	fprintf(stderr,"Metal beam atmosphere: %u frames / %u pixels PASS\n",frames,pixels);
	RalMetal_PresentDestroy(present); RenderSubmission_Reset(&frontend); RalMetal_CoreDestroy(core);
	return 0;
}

static int SkyTierLightingPixelsForAlbedo(byte albedo, std::vector<int> &environmentDelta,
		int lightmapValue = -1, bool lightmapFirst = false) {
	ralMetalCoreCreateInfo_t info = {1u}; ralMetalCore_t *core = NULL;
	ralMetalCoreReceipt_t coreReceipt; ralMetalPresent_t *present = NULL;
	ralMetalPresentLayerReceipt_t layer; ralMetalDrawableReceipt_t drawable;
	ralMetalPresentReceipt_t receipt; renderSubmissionReceipt_t submission;
	auto storage = std::make_unique<renderSubmissionState_t>();
	renderSubmissionState_t &frontend = *storage;
	ralSurfaceFormat_t format = {RAL_FORMAT_B8G8R8A8_UNORM, RAL_COLORSPACE_SRGB_NONLINEAR};
	ralPresentPreference_t fifo = {RAL_PRESENT_FIFO, 3u, 3u};
	ralSwapchainCreateInfo_t create = {};
	create.desiredWidth = create.desiredHeight = 16u; // offscreen, no window
	create.formatPreferences = &format; create.formatPreferenceCount = 1;
	create.presentPreferences = &fifo; create.presentPreferenceCount = 1;
	create.requiredUsage = RAL_TEXTURE_USAGE_COLOR_ATTACHMENT;
	CHECK(RalMetal_CoreCreate(&info, &core, &coreReceipt));
	CHECK(RenderSubmission_Init(&frontend, 1u));
	CHECK(RalMetal_PresentCreate(core, &coreReceipt, &create, 2u, &present, &layer));
	CHECK(RalMetal_PresentSetToneMap(present, 0u, 1.0f, 1.6f, 0.977f, 0.18f, 0.267f, 8.0f));
	CHECK(RalMetal_PresentSetLightmapBoost(present, 4.6f));
	const byte gray[4] = {albedo,albedo,albedo,255}, amber[4] = {192,64,16,255};
	const float black[4] = {0,0,0,1};
	qhandle_t wall = RenderSubmission_RegisterMaterialImage(&frontend,
		RENDER_ASSET_MATERIAL, "test/tier-wall", qtrue, gray, 1, 1);
	qhandle_t sky = RenderSubmission_RegisterMaterialImage(&frontend,
		RENDER_ASSET_MATERIAL, "test/tier-sky", qtrue, amber, 1, 1);
	CHECK(wall > 0 && sky > 0);
	CHECK(RenderSubmission_SetMaterialSky(&frontend, sky, qtrue));
	CHECK(RenderSubmission_SetMaterialCullMode(&frontend, wall, RENDER_CULL_NONE));
	CHECK(RenderSubmission_SetMaterialCullMode(&frontend, sky, RENDER_CULL_NONE));
	renderMaterialStageSnapshot_t skyStage = {};
	skyStage.material = RenderSubmission_RegisterStageTexture(&frontend,
		RENDER_ASSET_MATERIAL, "*tier-sky-stage", qtrue, amber, 1, 1);
	CHECK(skyStage.material > 0);
	skyStage.scaleScroll[0] = skyStage.scaleScroll[1] = 1;
	skyStage.sourceBlend = 1; // ONE/ZERO: constant authored radiance
	skyStage.rgbGen = RAL_MATERIAL_COLOR_IDENTITY;
	skyStage.alphaGen = RAL_MATERIAL_ALPHA_IDENTITY;
	CHECK(RenderSubmission_SetMaterialStages(&frontend, sky, &skyStage, 1u));
	ralMaterialSourceSky_t skySource = {};
	CHECK(RenderSubmission_SetMaterialAuthoredSkySource(&frontend, sky, &skySource));
	renderMaterialFactorPublicationEntry_t factors = {};
	factors.material = wall;
	for (unsigned c = 0; c < 4; ++c) factors.factors.baseColor[c] = 1;
	factors.factors.roughness = 0.5f;
	factors.factors.metallic = lightmapValue >= 0 ? 1.0f : 0.0f;
	factors.factors.normalScale = factors.factors.occlusionStrength = 1;
	CHECK(RenderSubmission_PublishMaterialFactors(&frontend, 1u, &factors, 1u));
	mapFile_t world = {}; dsurface_t surfaces[2] = {}; dshader_t shaders[2] = {};
	drawVert_t vertices[8] = {}; int indices[12] = {0,1,2,0,2,3,0,1,2,0,2,3};
	strcpy(world.name, "maps/tier-lighting.bsp"); world.checksum = 239216;
	if (lightmapValue >= 0) {
		const byte lightmap[4] = {(byte)lightmapValue, (byte)lightmapValue, (byte)lightmapValue, 255};
		char name[MAX_QPATH];
		CHECK(RenderSubmission_LightmapMaterialName(name, world.checksum, 0));
		CHECK(RenderSubmission_RegisterMaterialImage(&frontend,
			RENDER_ASSET_LIGHTMAP, name, qtrue, lightmap, 1, 1) > 0);
		renderMaterialStageSnapshot_t stages[2] = {};
		for (unsigned i = 0; i < 2; ++i) {
			stages[i].scaleScroll[0] = stages[i].scaleScroll[1] = 1;
			stages[i].rgbGen = RAL_MATERIAL_COLOR_IDENTITY;
			stages[i].alphaGen = RAL_MATERIAL_ALPHA_IDENTITY;
			stages[i].sourceBlend = i ? RENDER_MATERIAL_BLEND_DST_COLOR : RENDER_MATERIAL_BLEND_ONE;
			stages[i].depthWrite = qtrue;
		}
		stages[lightmapFirst ? 0 : 1].imageSource = 1; // actual registered world lightmap
		stages[lightmapFirst ? 1 : 0].material = wall;
		CHECK(RenderSubmission_SetMaterialStages(&frontend, wall, stages, 2));
	}
	strcpy(shaders[0].shader, "test/tier-wall"); strcpy(shaders[1].shader, "test/tier-sky");
	world.numShaders = 2; world.shaders = shaders;
	world.numSurfaces = 2; world.surfaces = surfaces;
	world.numDrawVerts = 8; world.drawVerts = vertices;
	world.numDrawIndexes = 12; world.drawIndexes = indices;
	const float yz[4][2] = {{-32,-32},{32,-32},{32,32},{-32,32}};
	for (unsigned s = 0; s < 2; ++s) {
		surfaces[s].surfaceType = MST_PLANAR; surfaces[s].shaderNum = s;
		surfaces[s].firstVert = s * 4; surfaces[s].numVerts = 4;
		surfaces[s].firstIndex = s * 6; surfaces[s].numIndexes = 6;
		surfaces[s].lightmapNum = -1;
		if (s == 0 && lightmapValue >= 0) surfaces[s].lightmapNum = 0;
		for (unsigned v = 0; v < 4; ++v) {
			drawVert_t &vertex = vertices[s * 4 + v];
			vertex.xyz[0] = s ? 16 : 8; vertex.xyz[1] = yz[v][0]; vertex.xyz[2] = yz[v][1];
			vertex.normal[0] = -1; memset(vertex.color.rgba, 255, 4);
		}
	}
	CHECK(RenderSubmission_LoadWorld(&frontend, &world, 0));
	refdef_t view = {}; view.width = view.height = 16; view.fov_x = view.fov_y = 90;
	view.viewaxis[0][0] = view.viewaxis[1][1] = view.viewaxis[2][2] = 1;
	view.time = 1000;
	atmosphereFrameState_t atmosphere = {};
	atmosphere.schemaVersion = WIRED_ATMOSPHERE_SCHEMA_VERSION;
	atmosphere.indoorExposure = 1;
	for (unsigned c = 0; c < 3; ++c) { atmosphere.bounds[c] = -64; atmosphere.bounds[c+3] = 64; }
	auto &environment = atmosphere.skyEnvironment;
	environment.schemaVersion = WIRED_SKY_ENVIRONMENT_SCHEMA_VERSION;
	environment.mode = ATMOSPHERE_SKY_AUTHORED;
	environment.flags = ATMOSPHERE_SKY_AUTHORED_AVAILABLE;
	environment.authoredSkyIdentity = 239216;
	environment.sunDirection[2] = 1; environment.moonDirection[2] = -1;
	environment.skyLuminance = environment.exposure = environment.opticsVisibility = environment.distantSceneScale = 1;
	const atmosphereQualityTier_t tiers[] = {ATMOSPHERE_QUALITY_OFF, ATMOSPHERE_QUALITY_FULL,
		ATMOSPHERE_QUALITY_OFF, ATMOSPHERE_QUALITY_FULL, ATMOSPHERE_QUALITY_ANALYTIC,
		ATMOSPHERE_QUALITY_FULL, ATMOSPHERE_QUALITY_FULL, ATMOSPHERE_QUALITY_FULL};
	std::vector<byte> unlit, lit;
	uint64_t stableRadianceRevision = 0;
	for (unsigned phase = 0; phase < ARRAY_LEN(tiers); ++phase) {
		atmosphere.qualityTier = tiers[phase];
		atmosphere.flags = tiers[phase] == ATMOSPHERE_QUALITY_OFF ? 0
			: ATMOSPHERE_FLAG_ENABLED | ATMOSPHERE_FLAG_SKY_LIGHTING;
		environment.sourceRevision = std::min(phase + 1, 6u);
		CHECK(RenderSubmission_SetAtmosphere(&frontend, &atmosphere));
		for (unsigned frame = 0; frame < 32; ++frame) {
			const uint64_t generation = 4 + (phase * 32 + frame) * 2;
			CHECK(RenderSubmission_BeginFrame(&frontend, generation));
			// Toggle only surface response. Keep the live FULL sky/cache intact.
			frontend.pbrEnabled = phase != 6 ? qtrue : qfalse;
			CHECK(RenderSubmission_RenderScene(&frontend, &view, 0));
			CHECK(RalMetal_PresentAcquire(present, &coreReceipt, &layer, generation-1, &drawable));
			CHECK(RalMetal_PresentRequestCapture(present));
			CHECK(RalMetal_PresentClearAndSubmit(present, &coreReceipt, &layer, &drawable,
				&frontend, black, generation, &receipt));
			CHECK(receipt.loweredWorldIndexCount == 12);
			CHECK(receipt.atmosphereDispatchCount == 0 && receipt.weatherDrawCount == 0);
			ralSkyGpuTelemetryReceipt_t telemetry;
			CHECK(RalMetal_PresentSkyGpuTelemetry(present, &telemetry));
			if (phase == 5 && frame == 31) stableRadianceRevision = telemetry.currentRadianceRevision;
			if (phase >= 6) {
				CHECK(stableRadianceRevision != 0 && telemetry.currentRadianceRevision == stableRadianceRevision);
				CHECK(telemetry.submittedSliceMask == 0); // no recapture on the toggle
			}
			uint32_t width, height;
			const byte *rgb = RalMetal_PresentCaptureRgb(present, &width, &height);
			CHECK(rgb && width == 16 && height == 16);
			if (!phase && !frame) {
				unlit.assign(rgb, rgb + width * height * 3);
				if (lightmapValue >= 0) {
					auto linear = [](int code) { const float x = code / 255.0f;
						return x <= 0.04045f ? x / 12.92f : powf((x + 0.055f) / 1.055f, 2.4f); };
					const int expected = (int)lroundf(linear(albedo)
						* std::min(linear(lightmapValue) * 4.6f, 1.0f) * 255.0f);
					CHECK(expected > 0);
					for (byte channel : unlit) CHECK(abs((int)channel - expected) <= 1);
				}
			}
			if (tiers[phase] != ATMOSPHERE_QUALITY_FULL || !frontend.pbrEnabled) {
				int error = 0;
				for (unsigned p = 0; p < unlit.size(); ++p) error = std::max(error, abs((int)rgb[p] - unlit[p]));
				if (error || !frame) fprintf(stderr, "sky tier phase=%u frame=%u cold-OFF maxError=%d RGB=%u,%u,%u\n",
					phase, frame, error, rgb[0], rgb[1], rgb[2]);
				CHECK(error == 0);
			} else if (frame == 31) {
				CHECK(memcmp(unlit.data(), rgb, unlit.size()) != 0); // real IBL executed
				if (phase == 1) lit.assign(rgb, rgb + unlit.size());
				else CHECK(!memcmp(lit.data(), rgb, lit.size())); // resume, not permanent disable
			}
			CHECK(RenderSubmission_EndFrame(&frontend, generation, &submission));
		}
	}
	environmentDelta.resize(lit.size());
	for (unsigned p = 0; p < lit.size(); ++p) environmentDelta[p] = (int)lit[p] - unlit[p];
	RalMetal_PresentDestroy(present); RenderSubmission_Reset(&frontend); RalMetal_CoreDestroy(core);
	return 0;
}

static int SkyTierLightingPixels() {
	std::vector<int> grayDelta, blackDelta;
	CHECK(SkyTierLightingPixelsForAlbedo(80, grayDelta) == 0);
	CHECK(SkyTierLightingPixelsForAlbedo(0, blackDelta) == 0);
	CHECK(grayDelta.size() == blackDelta.size());
	/* Dielectric environment reflection has F0=.04 independent of albedo.
	 * Baked/vertex diffuse is already present in the OFF reference. Adding
	 * sky diffuse again makes ON-OFF grow with albedo. Two independently
	 * quantized 8-bit captures permit one code value, not a visual tolerance. */
	int maxError = 0;
	for (unsigned p = 0; p < grayDelta.size(); ++p)
		maxError = std::max(maxError, abs(grayDelta[p] - blackDelta[p]));
	fprintf(stderr, "dielectric environment albedo-independence maxError=%d\n", maxError);
	CHECK(maxError <= 1);
	std::vector<int> reference;
	for (bool lightmapFirst : {false, true}) for (int lightmapValue : {32, 128}) {
		std::vector<int> delta;
		CHECK(SkyTierLightingPixelsForAlbedo(128, delta, lightmapValue, lightmapFirst) == 0);
		if (reference.empty()) reference = delta;
		int error = 0;
		for (unsigned p = 0; p < delta.size(); ++p)
			error = std::max(error, abs(reference[p] - delta[p]));
		fprintf(stderr, "metallic environment lightmap=%d first=%d maxError=%d\n",
			lightmapValue, lightmapFirst, error);
		// Same unlit albedo and sky: changing baked diffuse or stage order
		// cannot change F0. Allow only independent 8-bit capture rounding.
		CHECK(error <= 1);
	}
	return 0;
}

static int WeatherInFlightInputs( qboolean ground = qfalse ) {
	std::vector<byte> expected;
	NSPointerArray *gridBuffers = [[NSPointerArray alloc] initWithOptions:NSPointerFunctionsWeakMemory];
	for ( unsigned overlap = 0; overlap < 2; ++overlap ) {
		@autoreleasepool {
			ralMetalCoreCreateInfo_t info = { 1u };
			ralMetalCore_t *core = NULL; ralMetalCoreReceipt_t coreReceipt;
			ralMetalPresent_t *present = NULL; ralMetalPresentLayerReceipt_t layer;
			ralMetalDrawableReceipt_t drawable; ralMetalPresentReceipt_t receipt;
			auto frontendStorage = std::make_unique<renderSubmissionState_t>();
	renderSubmissionState_t &frontend = *frontendStorage; renderSubmissionReceipt_t submission;
			ralSurfaceFormat_t format = { RAL_FORMAT_B8G8R8A8_UNORM, RAL_COLORSPACE_SRGB_NONLINEAR };
			ralPresentPreference_t fifo = { RAL_PRESENT_FIFO, 3u, 3u };
			ralSwapchainCreateInfo_t create = {}; refdef_t view = {};
			atmosphereFrameState_t atmosphere = {};
			mapFile_t world = {}; dsurface_t surface = {}; dshader_t shader = {};
			drawVert_t vertices[3] = {}; int indices[3] = {0,1,2};
			float heights[4] = {-64,-32,16,48};
			const float black[4] = { 0, 0, 0, 1 };
			CHECK( RalMetal_CoreCreate( &info, &core, &coreReceipt ) );
			CHECK( RenderSubmission_Init( &frontend, 1u ) );
			create.desiredWidth = create.desiredHeight = 16u; // native offscreen, no window
			create.formatPreferences = &format; create.formatPreferenceCount = 1u;
			create.presentPreferences = &fifo; create.presentPreferenceCount = 1u;
			create.requiredUsage = RAL_TEXTURE_USAGE_COLOR_ATTACHMENT;
			CHECK( RalMetal_PresentCreate( core, &coreReceipt, &create, 2u, &present, &layer ) );
			view.width = view.height = 16; view.fov_x = view.fov_y = 90;
			view.viewaxis[0][0] = view.viewaxis[1][1] = view.viewaxis[2][2] = 1;
			atmosphere.schemaVersion = WIRED_ATMOSPHERE_SCHEMA_VERSION;
			atmosphere.flags = ATMOSPHERE_FLAG_ENABLED | ATMOSPHERE_FLAG_VOLUMETRIC_MEDIA;
			atmosphere.qualityTier = ATMOSPHERE_QUALITY_FULL;
			atmosphere.seed = 216u; atmosphere.indoorExposure = 1.0f;
			atmosphere.precipitation[0] = 1.0f;
			for ( unsigned axis = 0; axis < 3; ++axis ) {
				atmosphere.bounds[axis] = -64.0f; atmosphere.bounds[axis + 3] = 64.0f;
			}
			if ( ground ) {
				strcpy( world.name, "maps/heightgrid-test.bsp" ); strcpy( shader.shader, "test/grid" );
				world.numShaders = 1; world.shaders = &shader;
				world.numSurfaces = 1; world.surfaces = &surface;
				world.numDrawVerts = 3; world.drawVerts = vertices;
				world.numDrawIndexes = 3; world.drawIndexes = indices;
				surface.surfaceType = MST_PLANAR; surface.numVerts = surface.numIndexes = 3;
				surface.lightmapNum = -1;
				vertices[1].xyz[0] = 1; vertices[2].xyz[1] = 1;
				CHECK( RenderSubmission_RegisterAsset( &frontend, RENDER_ASSET_MATERIAL, shader.shader ) );
				CHECK( RenderSubmission_LoadWorld( &frontend, &world, 0 ) );
				view.rdflags = RDF_NOWORLDMODEL; // exercise real weather, not fixture surface
				atmosphere.flags |= ATMOSPHERE_FLAG_HEIGHTGRID;
				atmosphere.gridSize = 2;
				atmosphere.worldMins[0] = atmosphere.worldMins[1] = -64;
				atmosphere.worldMaxs[0] = atmosphere.worldMaxs[1] = 64;
			}
			id<MTLCommandQueue> queue = RalMetal_CoreNativeQueue( core );
			id<MTLSharedEvent> gate = [RalMetal_CoreNativeDevice( core ) newSharedEvent];
			CHECK( gate );
			const unsigned heldStep = ground ? 2u : 1u, finalStep = heldStep + 1u;
			uintptr_t firstGrid = 0;
			for ( unsigned step = 0; step <= finalStep; ++step ) { @autoreleasepool {
				const uint64_t generation = 4u + step * 2u;
				view.time = 1000 + step * 20;
				atmosphere.timelineSeconds = (float)view.time * 0.001f;
				atmosphere.wind[0] = step == 1u ? 80.0f : -40.0f;
				CHECK( RenderSubmission_BeginFrame( &frontend, generation ) );
				CHECK( RenderSubmission_SetAtmosphere( &frontend, &atmosphere ) );
				if ( ground ) {
					if ( step >= heldStep ) for ( float &height : heights ) height = step == heldStep ? 1000.0f : -64.0f;
					CHECK( RenderSubmission_SetAtmosphereHeightgrid( &frontend, heights, 4 ) );
				}
				CHECK( RenderSubmission_RenderScene( &frontend, &view, 0 ) );
				if ( overlap && step == heldStep ) {
					id<MTLCommandBuffer> hold = [queue commandBuffer];
					[hold encodeWaitForEvent:gate value:1u]; [hold commit];
					// Bounded test watchdog only: a regression cannot leave the GPU stuck.
					dispatch_after( dispatch_time( DISPATCH_TIME_NOW, 5 * NSEC_PER_SEC ),
						dispatch_get_global_queue( QOS_CLASS_DEFAULT, 0 ), ^{ gate.signaledValue = 1u; } );
				}
				CHECK( RalMetal_PresentRenderAndSubmit( present, &coreReceipt, &layer,
					&frontend, black, generation - 1u, generation, &drawable, &receipt, NULL )
					== RAL_METAL_SUBMIT_PRESENTED );
				CHECK( receipt.weatherDispatchCount == 1u && receipt.weatherDrawCount == 1u );
				CHECK( receipt.weather.heightgridActive == ground );
				if ( ground ) {
					id<MTLBuffer> buffer = RalMetal_PresentNativeHeightgrid( present, 0 );
					CHECK( buffer && buffer.length == sizeof( heights ) );
					[gridBuffers addPointer:(void *)buffer];
					if ( !step ) firstGrid = (uintptr_t)buffer;
					if ( step == 1u ) CHECK( firstGrid == (uintptr_t)buffer );
				}
				if ( overlap && step == finalStep ) {
					const bool stillHeld = gate.signaledValue == 0u;
					if ( ground ) {
						CHECK( RalMetal_PresentUnloadWorld( present, 0 ) );
						CHECK( !RalMetal_PresentNativeHeightgrid( present, 0 ) );
						/* Both immutable versions must survive owner release until
						 * their blocked compute commands actually consume them. */
						CHECK( [gridBuffers pointerAtIndex:gridBuffers.count - 1] != NULL
							&& [gridBuffers pointerAtIndex:gridBuffers.count - 2] != NULL );
					}
					gate.signaledValue = 1u;
					CHECK( stillHeld ); // prove the two frames really overlapped, not a timed wait
				}
				if ( !overlap || step != heldStep ) {
					id<MTLCommandBuffer> drain = [queue commandBuffer];
					[drain commit]; [drain waitUntilCompleted];
					CHECK( drain.status == MTLCommandBufferStatusCompleted );
					if ( ground ) {
						id<MTLBuffer> pool = RalMetal_PresentNativeWeatherBuffer( present );
						const float *particles = (const float *)[pool contents];
						unsigned alive = 0;
						for ( unsigned p = 0; p < receipt.weather.activeParticleCount; ++p ) {
							const float *particle = particles + p * 8u;
							if ( particle[7] < 0.5f ) continue;
							alive++;
							const float x = fminf(1, fmaxf(0, (particle[0]+64)/64));
							const float y = fminf(1, fmaxf(0, (particle[1]+64)/64));
							const float lower = heights[0]*(1-x)+heights[1]*x;
							const float upper = heights[2]*(1-x)+heights[3]*x;
							CHECK( particle[2] >= lower*(1-y)+upper*y - 0.0001f );
						}
						if ( step == heldStep ) CHECK( alive == 0u );
						else CHECK( alive > 0u );
					}
				}
				CHECK( RenderSubmission_EndFrame( &frontend, generation, &submission ) );
			} }
			id<MTLBuffer> pool = RalMetal_PresentNativeWeatherBuffer( present );
			CHECK( pool && pool.length && [pool contents] );
			const byte *bytes = (const byte *)[pool contents];
			if ( !overlap ) expected.assign( bytes, bytes + pool.length );
			else CHECK( expected.size() == pool.length && !memcmp( expected.data(), bytes, pool.length ) );
			RalMetal_PresentDestroy( present ); RenderSubmission_Reset( &frontend );
			[gate release]; RalMetal_CoreDestroy( core );
		}
	}
	for ( NSUInteger i = 0; i < gridBuffers.count; ++i ) CHECK( [gridBuffers pointerAtIndex:i] == NULL );
	[gridBuffers release];
	fprintf( stderr, "weather: ground=%d serialized/overlapped native GPU pools byte-identical\n", ground );
	return 0;
}

/* Map-local source IDs must not inherit a previous registration's result. */
static int wired_lens_registration_epoch( qboolean checkAcquireRetirement = qfalse ) {
	@autoreleasepool {
		ralMetalCoreCreateInfo_t info = { 1u };
		ralMetalCore_t *core = NULL; ralMetalCoreReceipt_t coreReceipt;
		ralMetalPresent_t *present = NULL; ralMetalPresentLayerReceipt_t layer;
		ralMetalDrawableReceipt_t drawable; ralMetalPresentReceipt_t receipt;
		auto storage = std::make_unique<renderSubmissionState_t>();
		renderSubmissionState_t &frontend = *storage;
		renderSubmissionReceipt_t submission;
		ralSurfaceFormat_t format = { RAL_FORMAT_B8G8R8A8_UNORM, RAL_COLORSPACE_SRGB_NONLINEAR };
		ralPresentPreference_t fifo = { RAL_PRESENT_FIFO, 3u, 3u };
		ralSwapchainCreateInfo_t create = {}; refdef_t view = {};
		const float black[4] = { 0, 0, 0, 1 };
		CHECK( RalMetal_CoreCreate( &info, &core, &coreReceipt ) );
		CHECK( RenderSubmission_Init( &frontend, 1u ) );
		create.desiredWidth = create.desiredHeight = 16u; // offscreen, no window
		create.formatPreferences = &format; create.formatPreferenceCount = 1u;
		create.presentPreferences = &fifo; create.presentPreferenceCount = 1u;
		create.requiredUsage = RAL_TEXTURE_USAGE_COLOR_ATTACHMENT;
		AcquireProbeLayer *probeLayer = [[AcquireProbeLayer alloc] init];
		CHECK( RalMetal_PresentAdoptBorrowedLayer( core, &coreReceipt, &create,
			2u, probeLayer, &present, &layer ) );
		view.width = view.height = 16; view.fov_x = view.fov_y = 90;
		view.viewaxis[0][0] = view.viewaxis[1][1] = view.viewaxis[2][2] = 1;
		for ( unsigned epoch = 0; epoch < 3; ++epoch ) {
			const uint64_t generation = 4u + epoch * 64u;
			float visibility = -7.0f;
			CHECK( !RalMetal_PresentLensVisibility( present, 7, &visibility ) );
			CHECK( visibility == -7.0f );
			const byte white[4] = {255, 255, 255, 255};
			CHECK( RenderSubmission_RegisterMaterialImage( &frontend,
				RENDER_ASSET_MATERIAL, "test/lens-wall", qtrue, white, 1, 1 ) > 0 );
			mapFile_t world = {}; dsurface_t surface = {}; dshader_t shader = {};
			drawVert_t vertices[4] = {}; int indices[6] = {0,1,2,0,2,3};
			strcpy( world.name, "maps/lens-epoch.bsp" ); strcpy( shader.shader, "test/lens-wall" );
			world.checksum = 227900 + epoch; world.numShaders = 1; world.shaders = &shader;
			world.numSurfaces = 1; world.surfaces = &surface;
			world.numDrawVerts = 4; world.drawVerts = vertices;
			world.numDrawIndexes = 6; world.drawIndexes = indices;
			surface.surfaceType = MST_PLANAR; surface.numVerts = 4;
			surface.numIndexes = 6; surface.lightmapNum = -1;
			const float yz[4][2] = {{32,32},{-32,32},{-32,-32},{32,-32}};
			for ( unsigned v = 0; v < 4; ++v ) {
				vertices[v].xyz[0] = 32; vertices[v].xyz[1] = yz[v][0]; vertices[v].xyz[2] = yz[v][1];
				vertices[v].normal[0] = -1; memcpy( vertices[v].color.rgba, white, 4 );
			}
			CHECK( RenderSubmission_LoadWorld( &frontend, &world, 0 ) );
			lensSourceDesc_t source = {};
			source.id = 7; source.origin[0] = epoch == 1 ? -8.0f : 8.0f;
			source.radius = 1.0f;
			view.time = 1000; // each registration restarts the map clock
			CHECK( RenderSubmission_BeginFrame( &frontend, generation ) );
			CHECK( RenderSubmission_AddLensSource( &frontend, &source ) );
			CHECK( RenderSubmission_RenderScene( &frontend, &view, 0 ) );
			CHECK( RalMetal_PresentRequestCapture( present ) );
			wiredLensBufferProbe buffers( RalMetal_CoreNativeDevice( core ) );
			{
				wiredCommandLeaseProbe leases( RalMetal_CoreNativeQueue( core ) );
				@autoreleasepool {
					CHECK( RalMetal_PresentRenderAndSubmit( present, &coreReceipt, &layer,
						&frontend, black, generation - 1u, generation, &drawable, &receipt, NULL )
						== RAL_METAL_SUBMIT_PRESENTED );
				} // capture completed the GPU; drain temporary native ownership
				if ( checkAcquireRetirement ) {
					CHECK( wiredObservedCommands.count > 0u );
					probeLayer.completedCommands = wiredObservedCommands;
					probeLayer.returnNoDrawable = YES;
					ralMetalDrawableReceipt_t rejected = {}, unchanged = {};
					CHECK( !RalMetal_PresentAcquire( present, &coreReceipt, &layer,
						generation + 1u, &rejected ) );
					CHECK( !memcmp( &rejected, &unchanged, sizeof( rejected ) ) );
					CHECK( probeLayer.completedCommandsReleasedBeforeAcquire );
					probeLayer.completedCommands = nil; probeLayer.returnNoDrawable = NO;
					/* A warmed second frame does not force a readback wait. Hold
					 * the real GPU queue: acquisition must return without waiting
					 * or retiring pending lens work, even when no drawable exists. */
					wiredUniformQueueGate gate( RalMetal_CoreNativeDevice( core ) );
					CHECK( gate.event != nil );
					@autoreleasepool {
						id<MTLCommandBuffer> block = wired_fixture_command( RalMetal_CoreNativeQueue( core ) );
						[block encodeWaitForEvent:gate.event value:1u]; [block commit];
						@autoreleasepool {
							CHECK( RalMetal_PresentRenderAndSubmit( present, &coreReceipt, &layer,
								&frontend, black, generation + 1u, generation + 2u,
								&drawable, &receipt, NULL ) == RAL_METAL_SUBMIT_PRESENTED );
						}
						probeLayer.completedCommands = wiredObservedCommands;
						probeLayer.returnNoDrawable = YES;
						CHECK( !RalMetal_PresentAcquire( present, &coreReceipt, &layer,
							generation + 3u, &rejected ) );
						CHECK( !probeLayer.completedCommandsReleasedBeforeAcquire );
						CHECK( gate.event.signaledValue == 0u );
						gate.event.signaledValue = 1u;
						id<MTLCommandBuffer> barrier = wired_fixture_command( RalMetal_CoreNativeQueue( core ) );
						[barrier commit]; [barrier waitUntilCompleted];
						CHECK( barrier.status == MTLCommandBufferStatusCompleted );
					}
					CHECK( !RalMetal_PresentAcquire( present, &coreReceipt, &layer,
						generation + 3u, &rejected ) );
					CHECK( probeLayer.completedCommandsReleasedBeforeAcquire );
					CHECK( !memcmp( &rejected, &unchanged, sizeof( rejected ) ) );
					probeLayer.completedCommands = nil; probeLayer.returnNoDrawable = NO;
				}
			}
			CHECK( receipt.lensVisibilityDispatchCount == 1u );
			if ( !checkAcquireRetirement && epoch == 0 ) {
				unsigned warmedAllocations = 0u;
				for ( unsigned frame = 0u; frame < 12u; ++frame ) {
					@autoreleasepool {
						CHECK( RalMetal_PresentRequestCapture( present ) );
						CHECK( RalMetal_PresentRenderAndSubmit( present, &coreReceipt, &layer,
							&frontend, black, generation + frame * 2u + 1u,
							generation + frame * 2u + 2u, &drawable, &receipt, NULL )
							== RAL_METAL_SUBMIT_PRESENTED );
						CHECK( receipt.lensVisibilityDispatchCount == 1u );
						CHECK( RalMetal_PresentLensVisibility( present, 7, &visibility ) && visibility == 1.0f );
					}
					if ( frame == 3u ) warmedAllocations = wiredLensAllocations;
					if ( frame > 3u ) CHECK( wiredLensAllocations == warmedAllocations );
				}
				CHECK( warmedAllocations > 0u && warmedAllocations <= 8u );
				fprintf(stderr, "lens buffers: eight allocation-free completed-slot reuse frames\n");
			}
			if ( epoch != 2 ) {
				CHECK( RalMetal_PresentLensVisibility( present, 7, &visibility ) );
				CHECK( visibility == ( epoch == 1 ? 0.0f : 1.0f ) );
			} // last epoch leaves a completed result uncollected at reset
			CHECK( !RalMetal_PresentResetRegistrationAssets( present, &frontend ) );
			if ( epoch != 2 ) CHECK( RalMetal_PresentLensVisibility( present, 7, &visibility ) );
			CHECK( RenderSubmission_EndFrame( &frontend, generation, &submission ) );
			CHECK( RalMetal_PresentUnloadWorld( present, 0 ) );
			CHECK( RenderSubmission_UnloadWorld( &frontend, 0 ) );
			CHECK( RalMetal_PresentResetRegistrationAssets( present, &frontend ) );
			for ( NSUInteger b = 0u; b < wiredLensBuffers.count; ++b )
				CHECK( [wiredLensBuffers pointerAtIndex:b] == NULL );
			visibility = -7.0f;
			CHECK( !RalMetal_PresentLensVisibility( present, 7, &visibility ) );
			CHECK( visibility == -7.0f );
		}
		RalMetal_PresentDestroy( present ); [probeLayer release]; RenderSubmission_Reset( &frontend );
		RalMetal_CoreDestroy( core );
	}
	fprintf( stderr, "lens registration: epochs isolated, acquire retirement=%d\n", checkAcquireRetirement );
	return 0;
}

int main( int argc, const char **argv ) {
	CHECK( argc == 1 || ( argc == 2 && ( !strcmp( argv[1], "--present" )
		|| !strcmp( argv[1], "--sky-lighting" ) || !strcmp( argv[1], "--lens-lifetime" )
		|| !strcmp( argv[1], "--beam-atmosphere" ) ) ) );
	if ( argc == 2 && !strcmp( argv[1], "--beam-atmosphere" ) ) return BeamAtmospherePixels();
	if ( argc == 2 && !strcmp( argv[1], "--sky-lighting" ) )
		return SkyTierLightingPixels();
	if ( argc == 2 && !strcmp( argv[1], "--lens-lifetime" ) ) {
		CHECK( wired_command_lease_observer_contract() == 0 );
		return wired_lens_registration_epoch( qtrue );
	}
	ralMetalCoreCreateInfo_t coreInfo = { 71u };
	ralMetalCore_t *core = NULL;
	ralMetalCoreReceipt_t coreReceipt, staleCore;
	ralSurfaceFormat_t sdrFormat = {
		RAL_FORMAT_B8G8R8A8_UNORM, RAL_COLORSPACE_SRGB_NONLINEAR };
	ralSurfaceFormat_t hdrFormat = {
		RAL_FORMAT_R16G16B16A16_SFLOAT, RAL_COLORSPACE_DISPLAY_P3 };
	ralSurfaceFormat_t unsupportedFormat = {
		RAL_FORMAT_A2B10G10R10_UNORM, RAL_COLORSPACE_HDR10_ST2084 };
	ralPresentPreference_t fifo = { RAL_PRESENT_FIFO, 3u, 3u };
	ralPresentationPolicyRequest_t unlockedRequest;
	ralPresentationPolicy_t unlockedPolicy;
	ralSwapchainCreateInfo_t createInfo;
	ralMetalPresent_t *present = (ralMetalPresent_t *)(uintptr_t)0x1234u;
	ralMetalPresentLayerReceipt_t layerReceipt, layerBefore, exactLayer;
	ralMetalDrawableReceipt_t drawableReceipt, drawableBefore, exactDrawable,
		persistentDrawable;
	ralMetalPresentReceipt_t presentReceipt, presentBefore, exactPresent,
		persistentReceipt;
	ralSkyGpuTelemetryReceipt_t skyGpuTelemetry;
	renderSubmissionReceipt_t submissionReceipt;
	ralMetalLighting_t *directionalLighting = NULL;
	ralMetalLightingReceipt_t directionalLightingReceipt;
	ralLightingProductRequest_t lightingRequest;
	ralLightingArtifactReceipt_t lightingArtifact;
	ralLightingRuntimePlan_t lightingPlan;
	ralStaticLightingCapabilities_t lightingCapabilities = {
		qtrue, qtrue, qtrue, qtrue };
	ralMemoryFailureEvent_t lossEvent;
	ralMemoryFailureReceipt_t lossReceipt;
	auto frontendStorage = std::make_unique<renderSubmissionState_t>();
	renderSubmissionState_t &frontend = *frontendStorage;
	atmosphereFrameState_t atmosphere;
	mapFile_t world;
	dsurface_t worldSurface;
	drawVert_t worldVertices[3];
	int worldIndices[3] = { 0, 1, 2 };
	dshader_t worldShader;
	refdef_t worldView;
	refEntity_t modelEntity, spriteEntity, beamEntity;
	spriteDesc_t effectSprite;
	emitterDesc_t effectEmitter;
	decalDesc_t effectDecal;
	ribbonDesc_t effectRibbon;
	ribbonPoint_t effectRibbonPoints[2];
	beamDesc_t effectBeam;
	lensSourceDesc_t lensSource;
	particleClass_t effectParticleClass;
	refEntityMotion_t spriteMotion = { sizeof( spriteMotion ),
		REF_ENTITY_MOTION_VERSION, 17u, 3u,
		REF_ENTITY_MOTION_ROLE_GENERAL, 0u };
	const byte greenTexture[16] = {
		0u, 255u, 0u, 255u, 0u, 255u, 0u, 255u,
		0u, 255u, 0u, 255u, 0u, 255u, 0u, 255u
	};
	const byte neutralLightmap[4] = { 128u, 128u, 128u, 255u };
	char lightmapName[MAX_QPATH];
	qhandle_t uiMaterial, worldMaterial, lightmapMaterial, inlineModel;
	float sdrClear[4] = { 0.125f, 0.25f, 0.5f, 1.0f };
	float hdrClear[4] = { 2.0f, 1.25f, 0.5f, 1.0f };
	const byte *captureRgb;
	uint32_t captureWidth, captureHeight;
	uint64_t distantWorldIdentity = 0u;
	const ralLightVec3Q16_t indirectRadiance = {
		RAL_LIGHT_Q16_ONE, RAL_LIGHT_Q16_ONE, RAL_LIGHT_Q16_ONE };
	const ralLightVec3Q16_t dominantDirection = {
		0, 0, RAL_LIGHT_Q16_ONE };
	const uint8_t stationaryVisibility = 255u;
	uint8_t lightingRadianceBytes[8], lightingDirectionBytes[4];
	uint8_t lightingArtifactBytes[512];
	{
		ralSurfaceFormat_t preferences[2] = { unsupportedFormat, hdrFormat };
		ralSurfaceFormatSelectionInfo_t query;
		ralSurfaceFormatSelection_t selected, before;
		memset( &query, 0, sizeof( query ) );
		query.preferences = preferences;
		query.preferenceCount = 2u;
		CHECK( Ral_SelectSurfaceFormat( (ralBackend_t *)(uintptr_t)1u,
			&query, &selected ) == ralSuccess );
		CHECK( selected.selectedPreference == 1u
			&& selected.selected.format == RAL_FORMAT_R16G16B16A16_SFLOAT
			&& selected.availableFormatCount == 2u );
		memset( &selected, 0x5a, sizeof( selected ) ); before = selected;
		query.backendExtensionChain = (const void *)(uintptr_t)1u;
		CHECK( Ral_SelectSurfaceFormat( (ralBackend_t *)(uintptr_t)1u,
			&query, &selected ) == ralErrorInvalidArgument );
		CHECK( !memcmp( &selected, &before, sizeof( selected ) ) );
	}
	CHECK( AcquireAutoreleaseLifetime() == 0 );
	CHECK( wired_acquire_partition_test() == 0 );
	CHECK( wired_command_lease_observer_contract() == 0 );
	CHECK( wired_lens_registration_epoch() == 0 );
	CHECK( wired_lens_registration_epoch( qtrue ) == 0 );
	CHECK( MaterialGeneratorPixels() == 0 );
	CHECK( wired_fragment_binding_changes<14u>() == 0 );
	CHECK( wired_encoder_state_changes() == 0 );
	CHECK( wired_material_buffer_offsets() == 0 );
	CHECK( wired_fragment_binding_changes<17u>() == 0 );
	CHECK( MaterialAdditiveStagePixels() == 0 );
	CHECK( MaterialStageDepthPixels() == 0 );
CHECK( MaterialVertexAlphaPixels() == 0 );
	CHECK( ParticlePixelLifetime( qfalse ) == 0 );
	CHECK( ParticlePixelLifetime( qtrue ) == 0 );
	CHECK( ExposureProductPixels() == 0 );
	CHECK( SkyBoxIsNotBeam() == 0 );
	CHECK( OrderedSkyProjection() == 0 );
	// CTest partitions the long native suite; no argument still runs everything.
	if ( argc == 1 ) CHECK( SkyTierLightingPixels() == 0 );
	CHECK( BeamUvClockAxis( 0u ) == 0 );
	CHECK( BeamUvClockAxis( 1u ) == 0 );
	CHECK( RalMetal_CoreCreate( &coreInfo, &core, &coreReceipt ) );
	CHECK( wired_upload_cache_busy_retirement( RalMetal_CoreNativeDevice( core ) ) == 0 );
	CHECK( RenderSubmission_Init( &frontend, 71u ) );
	uiMaterial = RenderSubmission_RegisterMaterialImage( &frontend,
		RENDER_ASSET_MATERIAL, "textures/green", qtrue, greenTexture, 2u, 2u );
	CHECK( uiMaterial > 0 );
	memset( &world, 0, sizeof( world ) );
	memset( &worldSurface, 0, sizeof( worldSurface ) );
	memset( worldVertices, 0, sizeof( worldVertices ) );
	memset( &worldShader, 0, sizeof( worldShader ) );
	strcpy( world.name, "maps/metal-present-fixture.bsp" );
	world.checksum = 0x10203040;
	world.numSurfaces = 1; world.surfaces = &worldSurface;
	world.numDrawVerts = 3; world.drawVerts = worldVertices;
	world.numDrawIndexes = 3; world.drawIndexes = worldIndices;
	world.numShaders = 1; world.shaders = &worldShader;
	strcpy( worldShader.shader, "textures/world-green" );
	worldSurface.surfaceType = MST_PLANAR;
	worldSurface.numVerts = 3; worldSurface.numIndexes = 3;
	worldSurface.lightmapNum = 0;
	worldVertices[0].xyz[0] = worldVertices[1].xyz[0] = worldVertices[2].xyz[0] = 10.0f;
	worldVertices[0].xyz[1] = -1.0f; worldVertices[1].xyz[1] = 1.0f;
	worldVertices[2].xyz[2] = 1.0f;
	for ( int vertex = 0; vertex < 3; ++vertex ) {
		worldVertices[vertex].color.rgba[0] = worldVertices[vertex].color.rgba[3] = 255u;
		worldVertices[vertex].st[0] = vertex == 1 ? 1.0f : 0.0f;
		worldVertices[vertex].st[1] = vertex == 2 ? 1.0f : 0.0f;
		worldVertices[vertex].lightmap[0] = worldVertices[vertex].st[0];
		worldVertices[vertex].lightmap[1] = worldVertices[vertex].st[1];
	}
	worldMaterial = RenderSubmission_RegisterMaterialImage( &frontend,
		RENDER_ASSET_MATERIAL, worldShader.shader, qfalse,
		greenTexture, 2u, 2u );
	CHECK( worldMaterial > uiMaterial );
	CHECK( RenderSubmission_SetMaterialRasterPolicy( &frontend, worldMaterial,
		RENDER_ALPHA_BLEND, 0.25f, qfalse ) );
	CHECK( RenderSubmission_LightmapMaterialName( lightmapName,
		(uint32_t)world.checksum, 0 ) );
	lightmapMaterial = RenderSubmission_RegisterMaterialImage( &frontend,
		RENDER_ASSET_LIGHTMAP, lightmapName, qtrue, neutralLightmap, 1u, 1u );
	CHECK( lightmapMaterial > 0 );
	const qhandle_t invalidWorldMaterial = RenderSubmission_RegisterMaterialImage(
		&frontend, RENDER_ASSET_MSDF, "test/world-msdf-rejection", qtrue,
		greenTexture, 2u, 2u );
	CHECK( invalidWorldMaterial > 0 );
	CHECK( RenderSubmission_LoadWorld( &frontend, &world, 0 ) );
	CHECK( RenderSubmission_LoadWorld( &frontend, &world,
		RENDER_DISTANT_WORLD_BASE ) );
	CHECK( RenderSubmission_WorldContentIdentity( &frontend,
		RENDER_DISTANT_WORLD_BASE, &distantWorldIdentity ) );
	CHECK( RenderSubmission_SelectWorld( &frontend, 0 ) );
	inlineModel = RenderSubmission_RegisterInlineModel( &frontend, "*1", 0u, 1u );
	CHECK( inlineModel > 0 );
	memset( &atmosphere, 0, sizeof( atmosphere ) );
	atmosphere.schemaVersion = WIRED_ATMOSPHERE_SCHEMA_VERSION;
	atmosphere.flags = ATMOSPHERE_FLAG_ENABLED
		| ATMOSPHERE_FLAG_VOLUMETRIC_MEDIA | ATMOSPHERE_FLAG_SKY_LIGHTING;
	atmosphere.qualityTier = ATMOSPHERE_QUALITY_ANALYTIC;
	atmosphere.seed = 216u;
	atmosphere.bounds[0] = atmosphere.bounds[1] = atmosphere.bounds[2] = -64.0f;
	atmosphere.bounds[3] = atmosphere.bounds[4] = atmosphere.bounds[5] = 64.0f;
	atmosphere.temperatureC = -8.0f;
	atmosphere.humidity = 0.9f;
	atmosphere.indoorExposure = 1.0f;
	atmosphere.ambientColor[0] = 0.12f;
	atmosphere.ambientColor[1] = 0.16f;
	atmosphere.ambientColor[2] = 0.22f;
	atmosphere.mediaDensity = 0.1f;
	atmosphere.mediaHeightFalloff = 0.01f;
	atmosphere.skyEnvironment.schemaVersion = WIRED_SKY_ENVIRONMENT_SCHEMA_VERSION;
	atmosphere.skyEnvironment.mode = ATMOSPHERE_SKY_AUTHORED;
	atmosphere.skyEnvironment.flags = ATMOSPHERE_SKY_AUTHORED_AVAILABLE
		| ATMOSPHERE_SKY_DISTANT_SCENE;
	atmosphere.skyEnvironment.sourceRevision = 1u;
	atmosphere.skyEnvironment.authoredSkyIdentity = 1u;
	atmosphere.skyEnvironment.distantSceneIdentity = distantWorldIdentity;
	atmosphere.skyEnvironment.distantSceneScale = 16.0f;
	CHECK( RenderSubmission_SetAtmosphere( &frontend, &atmosphere ) );
	CHECK( RenderSubmission_BeginFrame( &frontend, 75u ) );
	memset( &worldView, 0, sizeof( worldView ) );
	worldView.width = 16; worldView.height = 16;
	worldView.fov_x = 90.0f; worldView.fov_y = 90.0f;
	worldView.viewaxis[0][0] = 1.0f; worldView.viewaxis[1][1] = 1.0f;
	worldView.viewaxis[2][2] = 1.0f;
	CHECK( RenderSubmission_RenderScene( &frontend, &worldView, 0 ) );
	{
		const vec3_t lightOrigin = { 8.0f, 0.0f, 0.0f };
		CHECK( RenderSubmission_AddLight( &frontend, lightOrigin, NULL,
			16.0f, 1.0f, 0.5f, 0.25f ) );
	}
	memset( &modelEntity, 0, sizeof( modelEntity ) );
	modelEntity.reType = RT_MODEL; modelEntity.hModel = inlineModel;
	modelEntity.axis[0][0] = modelEntity.axis[1][1] = modelEntity.axis[2][2] = 1.0f;
	memset( modelEntity.shader.rgba, 255, sizeof( modelEntity.shader.rgba ) );
	CHECK( RenderSubmission_AddEntity( &frontend, &modelEntity, NULL ) );
	{
		ralIrradianceEntitySampleReceipt_t local = LocalIrradiance( 75u, 1u );
		CHECK( RenderSubmission_AttachEntityIrradiance( &frontend, 0u, &local ) );
	}
	memset( &spriteEntity, 0, sizeof( spriteEntity ) );
	spriteEntity.reType = RT_SPRITE; spriteEntity.origin[0] = 8.0f;
	spriteEntity.rotation = 45.0f;
	spriteEntity.radius = 1.0f; spriteEntity.customShader = uiMaterial;
	memset( spriteEntity.shader.rgba, 255, sizeof( spriteEntity.shader.rgba ) );
	CHECK( RenderSubmission_AddEntity( &frontend, &spriteEntity, &spriteMotion ) );
	memset( &beamEntity, 0, sizeof( beamEntity ) );
	beamEntity.reType = RT_BEAM; beamEntity.origin[0] = 8.0f;
	beamEntity.origin[1] = -1.0f; beamEntity.oldorigin[0] = 8.0f;
	beamEntity.oldorigin[1] = 1.0f; beamEntity.customShader = uiMaterial;
	memset( beamEntity.shader.rgba, 255, sizeof( beamEntity.shader.rgba ) );
	CHECK( RenderSubmission_AddEntity( &frontend, &beamEntity, NULL ) );
	memset( &effectParticleClass, 0, sizeof( effectParticleClass ) );
	effectParticleClass.shader = uiMaterial;
	effectParticleClass.emitMode = EMIT_POINT;
	effectParticleClass.scatterShape = SCATTER_SPHERE;
	effectParticleClass.scatterMagnitude = 1.0f;
	effectParticleClass.velocityShape = VEL_AXIAL_PLUS_CUBE;
	effectParticleClass.axialSpeed = 8.0f;
	effectParticleClass.cubeJitter = 2.0f;
	effectParticleClass.lifetimeMean = 0.5f;
	effectParticleClass.colorPalette[0][0] = 1.0f;
	effectParticleClass.colorPalette[0][1] = 0.5f;
	effectParticleClass.colorPalette[0][3] = 1.0f;
	effectParticleClass.paletteCount = 1;
	effectParticleClass.colorEndMult[0] = 1.0f;
	effectParticleClass.colorEndMult[1] = 1.0f;
	effectParticleClass.colorEndMult[2] = 1.0f;
	effectParticleClass.sizeStart = 0.25f;
	effectParticleClass.sizeEnd = 0.5f;
	CHECK( RenderSubmission_RegisterParticleClass( &frontend, 1,
		&effectParticleClass ) );
	memset( &effectSprite, 0, sizeof( effectSprite ) );
	effectSprite.origin[0] = 8.0f; effectSprite.radius = 0.5f;
	effectSprite.rgba[0] = effectSprite.rgba[1] = effectSprite.rgba[2]
		= effectSprite.rgba[3] = 1.0f;
	effectSprite.shader = uiMaterial;
	CHECK( RenderSubmission_AddEffectSprite( &frontend, &effectSprite ) );
	memset( &effectEmitter, 0, sizeof( effectEmitter ) );
	effectEmitter.cls = 1; effectEmitter.count = 8;
	effectEmitter.origin[0] = 8.0f; effectEmitter.axis[2] = 1.0f;
	effectEmitter.colorTint[0] = effectEmitter.colorTint[1]
		= effectEmitter.colorTint[2] = effectEmitter.colorTint[3] = 1.0f;
	CHECK( RenderSubmission_AddEffectEmitter( &frontend, &effectEmitter ) );
	memset( &effectDecal, 0, sizeof( effectDecal ) );
	effectDecal.origin[0] = 9.0f; effectDecal.normal[0] = -1.0f;
	effectDecal.radius = 0.5f; effectDecal.shader = uiMaterial;
	effectDecal.rgba[0] = effectDecal.rgba[1] = effectDecal.rgba[2]
		= effectDecal.rgba[3] = 1.0f;
	effectDecal.lifetime = 1.0f;
	CHECK( RenderSubmission_AddEffectDecal( &frontend, &effectDecal ) );
	memset( effectRibbonPoints, 0, sizeof( effectRibbonPoints ) );
	for ( uint32_t point = 0u; point < 2u; ++point ) {
		effectRibbonPoints[point].pos[0] = 8.0f;
		effectRibbonPoints[point].pos[1] = point ? 1.0f : -1.0f;
		effectRibbonPoints[point].width = 0.25f;
		effectRibbonPoints[point].rgba[0] = 1.0f;
		effectRibbonPoints[point].rgba[3] = 1.0f;
	}
	memset( &effectRibbon, 0, sizeof( effectRibbon ) );
	effectRibbon.points = effectRibbonPoints;
	effectRibbon.numPoints = 2;
	effectRibbon.shader = uiMaterial;
	CHECK( RenderSubmission_AddEffectRibbon( &frontend, &effectRibbon ) );
	memset( &effectBeam, 0, sizeof( effectBeam ) );
	effectBeam.start[0] = effectBeam.end[0] = 8.0f;
	effectBeam.start[1] = -1.0f; effectBeam.end[1] = 1.0f;
	effectBeam.startWidth = 0.25f; effectBeam.endWidth = 0.1f;
	effectBeam.startColor[0] = effectBeam.startColor[3] = 1.0f;
	effectBeam.endColor[0] = effectBeam.endColor[3] = 1.0f;
	effectBeam.shader = uiMaterial; effectBeam.duration = 1.0f;
	effectBeam.fadeOut = 0.25f; effectBeam.axialCopies = 1;
	effectBeam.startEntityNum = effectBeam.endEntityNum = -1;
	CHECK( RenderSubmission_AddEffectBeam( &frontend, &effectBeam ) );
	memset( &lensSource, 0, sizeof( lensSource ) );
	lensSource.id = 7;
	lensSource.origin[0] = 8.0f;
	lensSource.rgba[0] = lensSource.rgba[1] = lensSource.rgba[2]
		= lensSource.rgba[3] = 1.0f;
	lensSource.radius = 1.0f;
	CHECK( RenderSubmission_AddLensSource( &frontend, &lensSource ) );
	{
		const float white[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
		CHECK( RenderSubmission_SetColor( &frontend, white ) );
		CHECK( RenderSubmission_AddUiQuad( &frontend, 0.0f, 0.0f, 16.0f, 16.0f,
			0.0f, 0.0f, 1.0f, 1.0f, 0.0f, uiMaterial ) );
	}
	memset( &createInfo, 0, sizeof( createInfo ) );
	createInfo.desiredWidth = 16u; createInfo.desiredHeight = 16u;
	createInfo.formatPreferences = &sdrFormat; createInfo.formatPreferenceCount = 1u;
	createInfo.presentPreferences = &fifo; createInfo.presentPreferenceCount = 1u;
	createInfo.requiredUsage = RAL_TEXTURE_USAGE_COLOR_ATTACHMENT;
	memset( &layerReceipt, 0x5a, sizeof( layerReceipt ) ); layerBefore = layerReceipt;
	createInfo.desiredWidth = 0u;
	CHECK( !RalMetal_PresentCreate( core, &coreReceipt, &createInfo, 72u,
		&present, &layerReceipt ) );
	CHECK( present == (ralMetalPresent_t *)(uintptr_t)0x1234u
		&& memcmp( &layerReceipt, &layerBefore, sizeof( layerReceipt ) ) == 0 );
	createInfo.desiredWidth = 16u; createInfo.formatPreferences = &unsupportedFormat;
	CHECK( !RalMetal_PresentCreate( core, &coreReceipt, &createInfo, 72u,
		&present, &layerReceipt ) );
	createInfo.formatPreferences = &sdrFormat; fifo.desiredImageCount = 4u;
	CHECK( !RalMetal_PresentCreate( core, &coreReceipt, &createInfo, 72u,
		&present, &layerReceipt ) );
	fifo.desiredImageCount = 3u;
	fifo.unboundedImageCount = 5u;
	CHECK( !RalMetal_PresentCreate( core, &coreReceipt, &createInfo, 72u,
		&present, &layerReceipt ) );
	fifo.unboundedImageCount = 3u;
	ralPresentPreference_t unsupportedMode = { RAL_PRESENT_MAILBOX, 3u, 3u };
	createInfo.presentPreferences = &unsupportedMode;
	CHECK( !RalMetal_PresentCreate( core, &coreReceipt, &createInfo, 72u,
		&present, &layerReceipt ) );
	createInfo.presentPreferences = &fifo;
	createInfo.requiredUsage = RAL_TEXTURE_USAGE_SAMPLED;
	CHECK( !RalMetal_PresentCreate( core, &coreReceipt, &createInfo, 72u,
		&present, &layerReceipt ) );
	createInfo.requiredUsage = RAL_TEXTURE_USAGE_COLOR_ATTACHMENT;
	CHECK( WeatherInFlightInputs() == 0 );
	CHECK( WeatherInFlightInputs( qtrue ) == 0 );
	CHECK( LateAcquireTransaction( &frontend, &createInfo ) == 0 );
	CHECK( LateAcquireTransaction( &frontend, &createInfo, qtrue, 0u ) == 0 );
	CHECK( LateAcquireTransaction( &frontend, &createInfo, qtrue, 64u ) == 0 );
	CHECK( RalMetal_PresentCreate( core, &coreReceipt, &createInfo, 72u,
		&present, &layerReceipt ) );
	ralExposureSettings_t exposure = { 1, 0.0123f, 0.5f, 0.2f, 1.5f, 0.5f, 0.25f, 8.0f };
	CHECK( !RalMetal_PresentSetExposure( NULL, &exposure ) );
	CHECK( !RalMetal_PresentSetExposure( present, NULL ) );
	CHECK( RalMetal_PresentSetExposure( present, &exposure ) );
	exposure.key = NAN;
	CHECK( !RalMetal_PresentSetExposure( present, &exposure ) );
	exposure.key = 0.0123f;
	CHECK( layerReceipt.selected.format == RAL_FORMAT_B8G8R8A8_UNORM
		&& layerReceipt.selected.colorSpace == RAL_COLORSPACE_SRGB_NONLINEAR
		&& layerReceipt.selected.presentMode == RAL_PRESENT_FIFO
		&& layerReceipt.displaySyncEnabled == qtrue
		&& layerReceipt.extendedDynamicRange == qfalse
		&& layerReceipt.ownsLayer == qtrue );
	exactLayer = layerReceipt;
	CHECK( RalMetal_PresentLayerReceiptExact( &layerReceipt, &exactLayer ) );
#define MUTATE_LAYER(field) do { exactLayer = layerReceipt; exactLayer.field++; \
	CHECK( !RalMetal_PresentLayerReceiptExact( &layerReceipt, &exactLayer ) ); } while (0)
	MUTATE_LAYER( coreGeneration ); MUTATE_LAYER( presentationGeneration );
	MUTATE_LAYER( selected.width ); MUTATE_LAYER( selected.imageCount );
	MUTATE_LAYER( selected.requestedImageCount );
#undef MUTATE_LAYER
	exactLayer = layerReceipt; exactLayer.ownsLayer = qfalse;
	CHECK( !RalMetal_PresentLayerReceiptExact( &layerReceipt, &exactLayer ) );
	exactLayer = layerReceipt; exactLayer.ownerIdentity = exactLayer.layerIdentity;
	CHECK( !RalMetal_PresentLayerReceiptExact( &exactLayer, &exactLayer ) );

	memset( &drawableReceipt, 0x5a, sizeof( drawableReceipt ) );
	drawableBefore = drawableReceipt;
	staleCore = coreReceipt; staleCore.generation++;
	CHECK( !RalMetal_PresentAcquire( present, &staleCore, &layerReceipt, 73u,
		&drawableReceipt ) );
	exactLayer = layerReceipt; exactLayer.presentationGeneration++;
	CHECK( !RalMetal_PresentAcquire( present, &coreReceipt, &exactLayer, 73u,
		&drawableReceipt ) );
	CHECK( RalMetal_PresentAcquire( present, &coreReceipt, &layerReceipt, 73u,
		&drawableReceipt ) );
	CHECK( drawableReceipt.width == 16u && drawableReceipt.height == 16u
		&& drawableReceipt.format == RAL_FORMAT_B8G8R8A8_UNORM );
	CHECK( !RalMetal_PresentAcquire( present, &coreReceipt, &layerReceipt, 74u,
		&drawableBefore ) );
	CHECK( !RalMetal_PresentReconfigure( present, &coreReceipt, &layerReceipt,
		&createInfo, 74u, &exactLayer ) );
	exactDrawable = drawableReceipt;
	CHECK( RalMetal_DrawableReceiptExact( &drawableReceipt, &exactDrawable ) );
	exactDrawable.textureIdentity++;
	CHECK( !RalMetal_DrawableReceiptExact( &drawableReceipt, &exactDrawable ) );
	memset( &presentReceipt, 0x5a, sizeof( presentReceipt ) ); presentBefore = presentReceipt;
	CHECK( !RalMetal_PresentClearAndSubmit( present, &coreReceipt, &layerReceipt,
		&exactDrawable, NULL, sdrClear, 75u, &presentReceipt ) );
	CHECK( memcmp( &presentReceipt, &presentBefore, sizeof( presentReceipt ) ) == 0 );
	sdrClear[0] = NAN;
	CHECK( !RalMetal_PresentClearAndSubmit( present, &coreReceipt, &layerReceipt,
		&drawableReceipt, NULL, sdrClear, 75u, &presentReceipt ) );
	sdrClear[0] = 0.125f;
	CHECK( memcmp( &presentReceipt, &presentBefore, sizeof( presentReceipt ) ) == 0 );
	/* A failed frame may have borrowed the cached world buffers already.
	 * Drop the cache after every failed attempt: leaked caller retains would
	 * keep native allocations alive even though the scope has been unloaded.
	 * No commands are submitted here; drain temporary references per attempt. */
	{
		const uint32_t count = frontend.localIrradianceEntityCount;
		/* Keep real decal/beam births in each rejected attempt. A retry must
		 * not admit them to persistent ownership more than once. */
		frontend.localIrradianceEntityCount = count + 1u;
		for ( unsigned attempt = 0u; attempt < 17u; ++attempt ) {
			NSPointerArray *buffers = [[NSPointerArray alloc]
				initWithOptions:NSPointerFunctionsWeakMemory];
			@autoreleasepool {
				CHECK( !RalMetal_PresentClearAndSubmit( present, &coreReceipt,
					&layerReceipt, &drawableReceipt, &frontend, sdrClear, 75u,
					&presentReceipt ) );
				CHECK( !memcmp( &presentReceipt, &presentBefore, sizeof( presentReceipt ) ) );
				for ( int worldIndex : { 0, RENDER_DISTANT_WORLD_BASE } ) {
					for ( qboolean indices : { qfalse, qtrue } ) {
						id<MTLBuffer> buffer = RalMetal_PresentNativeWorldBuffer(
							present, worldIndex, indices );
						CHECK( buffer != nil );
						[buffers addPointer:(void *)buffer];
					}
				}
				CHECK( RalMetal_PresentUnloadWorld( present, 0 ) );
				CHECK( RalMetal_PresentUnloadWorld( present, RENDER_DISTANT_WORLD_BASE ) );
			}
			for ( NSUInteger slot = 0u; slot < buffers.count; ++slot ) {
				if ( [buffers pointerAtIndex:slot] )
					fprintf( stderr, "failed-frame world buffer survived unload: slot=%lu attempt=%u\n",
						(unsigned long)slot, attempt );
				CHECK( [buffers pointerAtIndex:slot] == NULL );
			}
			[buffers release];
		}
		frontend.localIrradianceEntityCount = count;
	}
	/* Fail after entity upload, in the world material encoder. Rejected frames
	 * must release their transient native buffers and leave the receipt intact. */
	{
		FailedFrameEntityBufferProbe probe( RalMetal_CoreNativeDevice( core ) );
		renderWorldBatch_t *batch = const_cast<renderWorldBatch_t *>(
			frontend.worldSnapshot.batches );
		const renderWorldBatch_t savedBatch = *batch;
		renderMaterialSnapshot_t *material = nullptr;
		for ( uint32_t i = 0u; i < frontend.materialCount; ++i )
			if ( frontend.materials[i].snapshot.handle == worldMaterial )
				material = &frontend.materials[i].snapshot;
		CHECK( material );
		const auto savedMaterial = std::make_unique<renderMaterialSnapshot_t>( *material );
		for ( unsigned attempt = 0u; attempt < 12u; ++attempt ) {
			*batch = savedBatch; *material = *savedMaterial;
			switch ( attempt % 4u ) {
				case 0u: batch->baseMaterial = invalidWorldMaterial; break;
				case 1u:
					material->stageCount = 1u;
					material->stages[0] = {};
					material->stages[0].material = invalidWorldMaterial;
					break;
				case 2u: material->skySecondaryMaterial = invalidWorldMaterial; break;
				case 3u: batch->lightmapMaterial = 0; break;
			}
			@autoreleasepool {
				CHECK( !RalMetal_PresentClearAndSubmit( present, &coreReceipt,
					&layerReceipt, &drawableReceipt, &frontend, sdrClear, 75u,
					&presentReceipt ) );
				CHECK( !memcmp( &presentReceipt, &presentBefore, sizeof( presentReceipt ) ) );
			}
			CHECK( failedFrameEntityBuffers.count == attempt + 1u );
			CHECK( [failedFrameEntityBuffers pointerAtIndex:attempt] == NULL );
		}
		*batch = savedBatch; *material = *savedMaterial;
	}
	CHECK( RalMetal_PresentRequestCapture( present ) );
	CHECK( !RalMetal_PresentRequestCapture( present ) );
	const ralBloomSettings_t uiBloom = {1u, 2u, 0u, 0.0f, 2.0f};
	CHECK( RalMetal_PresentSetBloom( present, &uiBloom ) );
	CHECK( RalMetal_PresentClearAndSubmit( present, &coreReceipt, &layerReceipt,
		&drawableReceipt, &frontend, sdrClear, 75u, &presentReceipt ) );
	captureRgb = RalMetal_PresentCaptureRgb( present, &captureWidth, &captureHeight );
	fprintf( stderr, "persistent FX after 17 rejected births: decals=%u beams=%u\n",
		presentReceipt.loweredEffectDecalCount, presentReceipt.loweredEffectBeamCount );
	CHECK( presentReceipt.loweredEffectDecalCount == 1u
		&& presentReceipt.loweredEffectBeamCount == 1u );
	ralMetalGpuTiming_t timing = {}, timingAgain = {};
	CHECK( RalMetal_PresentGpuTiming( present, &timing ) );
	CHECK( timing.sampleCount + timing.unavailableCount == 1u );
	CHECK( timing.sampleCount == 0u || timing.durationNanoseconds > 0u );
	CHECK( timing.timelineSampleCount + timing.timelineUnavailableCount
		== timing.sampleCount + timing.unavailableCount );
	CHECK( timing.timelineSampleCount <= timing.sampleCount );
	CHECK( timing.timelineDurationNanoseconds <= timing.durationNanoseconds );
	CHECK( timing.timelineSampleCount == 0u
		? timing.timelineDurationNanoseconds == 0u && timing.commitToStartNanoseconds == 0u
		: timing.timelineDurationNanoseconds > 0u );
	CHECK( RalMetal_PresentGpuTiming( present, &timingAgain ) );
	CHECK( !memcmp( &timing, &timingAgain, sizeof( timing ) ) );
	CHECK( captureRgb && captureWidth == 16u && captureHeight == 16u
		&& captureRgb[( 7u * 16u + 8u ) * 3u + 0u] == 0u
		&& captureRgb[( 7u * 16u + 8u ) * 3u + 1u] == 255u
		&& captureRgb[( 7u * 16u + 8u ) * 3u + 2u] == 0u );
	CHECK( presentReceipt.presented == qtrue
		&& presentReceipt.frontendFrameGeneration == 75u
		&& presentReceipt.atmosphere.frameGeneration == 75u
		&& presentReceipt.submission.commands[0].state == RAL_COMMAND_SUBMITTED
		&& presentReceipt.clearDigest != 0u
		&& presentReceipt.readbackDigest != 0u
		&& presentReceipt.loweredWorldIndexCount == 6u
		&& presentReceipt.loweredWorldBatchCount == 2u
		&& presentReceipt.distantWorldCandidateCount == 1u
		&& presentReceipt.distantWorldDrawCount == 1u
		&& presentReceipt.distantWorldIndexCount == 3u
		&& presentReceipt.distantWorldDepthReset == qtrue
		&& presentReceipt.texturedWorldBatchCount == 2u
		&& presentReceipt.lightmappedWorldBatchCount == 2u
		&& presentReceipt.submittedDirectLightCount == 1u
		&& presentReceipt.directLightDrawCount >= 1u
		&& presentReceipt.directLightReferenceCount
			>= presentReceipt.directLightDrawCount
		&& presentReceipt.patchWorldBatchCount == 0u
		&& presentReceipt.maskedWorldBatchCount == 0u
		&& presentReceipt.blendedWorldBatchCount == 2u
		&& presentReceipt.depthWriteWorldBatchCount == 0u
		&& presentReceipt.loweredEntityIndexCount == 69u
		&& presentReceipt.loweredEntityBatchCount == 7u
		&& presentReceipt.modelEntityCount == 1u
		&& presentReceipt.primitiveEntityCount == 2u
		&& presentReceipt.temporalEntityCount == 1u
		&& presentReceipt.localIrradianceEntityCount == 1u
		&& presentReceipt.localIrradianceDrawCount == 1u
		&& presentReceipt.unresolvedEntityCount == 0u
		&& presentReceipt.loweredEffectSpriteCount == 1u
		&& presentReceipt.loweredEffectDecalCount == 1u
		&& presentReceipt.loweredEffectRibbonCount == 1u
		&& presentReceipt.loweredEffectBeamCount == 1u
		&& presentReceipt.effectEmitterDispatchCount == 1u
		&& presentReceipt.effectParticleDrawCount == RAL_PARTICLE_POOL_CAPACITY
		&& presentReceipt.effectPrimitiveDroppedCount == 0u
		&& presentReceipt.lensSourceCount == 1u
		&& presentReceipt.lensVisibilityDispatchCount == 1u
		&& presentReceipt.loweredUiPrimitiveCount == 1u
		&& presentReceipt.texturedUiPrimitiveCount == 1u
		&& presentReceipt.readbackX == 8u && presentReceipt.readbackY == 8u
		&& presentReceipt.readbackByteCount == 4u );
	if ( presentReceipt.readbackBytes[0] != 0u
			|| presentReceipt.readbackBytes[1] != 255u
			|| presentReceipt.readbackBytes[2] != 0u
			|| presentReceipt.readbackBytes[3] != 255u )
		fprintf( stderr, "textured readback: %u %u %u %u\n",
			presentReceipt.readbackBytes[0], presentReceipt.readbackBytes[1],
			presentReceipt.readbackBytes[2], presentReceipt.readbackBytes[3] );
	CHECK( presentReceipt.readbackBytes[0] == 0u
		&& presentReceipt.readbackBytes[1] == 255u
		&& presentReceipt.readbackBytes[2] == 0u
		&& presentReceipt.readbackBytes[3] == 255u );
	{
		float visibility = -1.0f;
		CHECK( RalMetal_PresentLensVisibility( present, 7, &visibility )
			&& visibility >= 0.0f && visibility <= 1.0f );
	}
	CHECK( RenderSubmission_EndFrame( &frontend, 75u, &submissionReceipt ) );
	const ralBloomSettings_t noBloom = {0u, 2u, 0u, .32f, .5f};
	CHECK( RalMetal_PresentSetBloom( present, &noBloom ) );
	/* Loading UI between scene frames must not attempt to draw the live
	 * persistent decal/beam/particle pools without a camera. The following
	 * scene still checks that those pools survived this viewless frame. */
	CHECK( RenderSubmission_BeginFrame( &frontend, 76u ) );
	CHECK( RenderSubmission_AddUiQuad( &frontend, 0.0f, 0.0f, 16.0f, 16.0f,
		0.0f, 0.0f, 1.0f, 1.0f, 0.0f, uiMaterial ) );
	CHECK( RalMetal_PresentAcquire( present, &coreReceipt, &layerReceipt, 74u,
		&persistentDrawable ) );
	CHECK( RalMetal_PresentClearAndSubmit( present, &coreReceipt, &layerReceipt,
		&persistentDrawable, &frontend, sdrClear, 76u, &persistentReceipt ) );
	CHECK( persistentReceipt.loweredUiPrimitiveCount == 1u
		&& persistentReceipt.loweredEntityIndexCount == 0u
		&& persistentReceipt.loweredEffectDecalCount == 0u
		&& persistentReceipt.loweredEffectBeamCount == 0u
		&& persistentReceipt.effectParticleDrawCount == 0u
		&& persistentReceipt.effectPrimitiveDroppedCount == 0u );
	CHECK( RenderSubmission_EndFrame( &frontend, 76u, &submissionReceipt ) );
	atmosphere.qualityTier = ATMOSPHERE_QUALITY_FULL;
	atmosphere.skyEnvironment.schemaVersion =
		WIRED_SKY_ENVIRONMENT_SCHEMA_VERSION;
	atmosphere.skyEnvironment.mode = ATMOSPHERE_SKY_PHYSICAL;
	atmosphere.skyEnvironment.sourceRevision = 2u;
	atmosphere.skyEnvironment.sunDirection[2] = 1.0f;
	atmosphere.skyEnvironment.sunRadiance[0] = 1.0f;
	atmosphere.skyEnvironment.sunRadiance[1] = 0.9f;
	atmosphere.skyEnvironment.sunRadiance[2] = 0.8f;
	atmosphere.skyEnvironment.moonDirection[2] = -1.0f;
	atmosphere.skyEnvironment.skyLuminance = 1.0f;
	atmosphere.skyEnvironment.exposure = 1.0f;
	atmosphere.skyEnvironment.opticsVisibility = 1.0f;
	atmosphere.skyEnvironment.distantSceneScale = 1.0f;
	CHECK( RenderSubmission_SetAtmosphere( &frontend, &atmosphere ) );
	CHECK( RenderSubmission_BeginFrame( &frontend, 77u ) );
	worldView.time = 250;
	CHECK( RenderSubmission_RenderScene( &frontend, &worldView, 0 ) );
	CHECK( RalMetal_PresentAcquire( present, &coreReceipt, &layerReceipt, 76u,
		&persistentDrawable ) );
	/* Expiry in a rejected future frame must not destroy resident effects. */
	{
		const int savedTime = frontend.worldSnapshot.timeMs;
		const uint32_t savedCount = frontend.localIrradianceEntityCount;
		const ralMetalPresentReceipt_t before = persistentReceipt;
		frontend.worldSnapshot.timeMs = 5000;
		frontend.localIrradianceEntityCount = savedCount + 1u;
		CHECK( !RalMetal_PresentClearAndSubmit( present, &coreReceipt, &layerReceipt,
			&persistentDrawable, &frontend, sdrClear, 77u, &persistentReceipt ) );
		CHECK( !memcmp( &persistentReceipt, &before, sizeof( before ) ) );
		frontend.worldSnapshot.timeMs = savedTime;
		frontend.localIrradianceEntityCount = savedCount;
	}
	CHECK( RalMetal_PresentClearAndSubmit( present, &coreReceipt, &layerReceipt,
		&persistentDrawable, &frontend, sdrClear, 77u, &persistentReceipt ) );
	CHECK( persistentReceipt.loweredEntityIndexCount == 12u
		&& persistentReceipt.atmosphere.skyEnvironment.selectedMode
			== RAL_SKY_ENVIRONMENT_PHYSICAL
		&& persistentReceipt.atmosphere.skyEnvironment.admittedWorkMask
			== RAL_SKY_DIRTY_TRANSMITTANCE
		&& ( persistentReceipt.atmosphere.skyEnvironment.passMask
			& RAL_SKY_PASS_TRANSMITTANCE ) != 0u
		&& persistentReceipt.loweredEntityBatchCount == 2u
		&& persistentReceipt.loweredEffectSpriteCount == 0u
		&& persistentReceipt.loweredEffectDecalCount == 1u
		&& persistentReceipt.loweredEffectRibbonCount == 0u
		&& persistentReceipt.loweredEffectBeamCount == 1u
		&& persistentReceipt.effectEmitterDispatchCount == 0u
		&& persistentReceipt.effectParticleDrawCount == RAL_PARTICLE_POOL_CAPACITY
		&& persistentReceipt.effectPrimitiveDroppedCount == 0u );
	CHECK( RalMetal_PresentSkyGpuTelemetry( present, &skyGpuTelemetry ) );
	CHECK( Ral_SkyGpuTelemetryReceiptExact( &skyGpuTelemetry,
		&skyGpuTelemetry ) );
	CHECK( skyGpuTelemetry.backendType == RAL_BACKEND_METAL );
	CHECK( skyGpuTelemetry.frameGeneration == 77u );
	CHECK( skyGpuTelemetry.sourceRevision > 0u );
	CHECK( skyGpuTelemetry.submittedWorkMask
		== RAL_SKY_DIRTY_TRANSMITTANCE );
	CHECK( skyGpuTelemetry.physicalTimingState
		== RAL_SKY_GPU_TIMING_PENDING );
	CHECK( skyGpuTelemetry.physicalGpuDurationNanoseconds == 0u );
	memset( &lightingRequest, 0, sizeof( lightingRequest ) );
	lightingRequest.schemaVersion = RAL_LIGHTING_PRODUCT_SCHEMA_VERSION;
	lightingRequest.artifactGeneration = 76u;
	lightingRequest.bake.schemaVersion = RAL_LIGHTING_BAKE_RECEIPT_SCHEMA_VERSION;
	lightingRequest.bake.bakeGeneration = 1u;
	lightingRequest.bake.staticIndirectKey = 2u;
	lightingRequest.bake.producerVersion = 3u;
	lightingRequest.bake.radianceHash = 4u;
	lightingRequest.bake.directionHash = 5u;
	lightingRequest.bake.patchCount = 1u;
	lightingRequest.bake.linkCount = 1u;
	lightingRequest.bake.completedBounces = 4u;
	lightingRequest.bake.ready = qtrue;
	lightingRequest.encoding = RAL_STATIC_LIGHTING_ENCODING_RGB9E5_OCT8;
	lightingRequest.pageWidth = lightingRequest.pageHeight = 1u;
	lightingRequest.pageCount = lightingRequest.texelCount = 1u;
	lightingRequest.indirectRadiance = &indirectRadiance;
	lightingRequest.dominantDirection = &dominantDirection;
	lightingRequest.stationaryVisibility = &stationaryVisibility;
	lightingRequest.stationaryVisibilityCount = 1u;
	CHECK( Ral_LightingProductWrite( &lightingRequest, lightingRadianceBytes,
		sizeof( lightingRadianceBytes ), lightingDirectionBytes,
		sizeof( lightingDirectionBytes ), lightingArtifactBytes,
		sizeof( lightingArtifactBytes ), &lightingArtifact ) );
	CHECK( Ral_LightingRuntimePlanBuild( RAL_BACKEND_METAL, 76u,
		lightingArtifactBytes, lightingArtifact.byteLength, &lightingArtifact,
		&lightingCapabilities, &lightingPlan ) );
	CHECK( RalMetal_LightingUpload( core, &coreReceipt, lightingArtifactBytes,
		lightingArtifact.byteLength, &lightingPlan, &directionalLighting,
		&directionalLightingReceipt ) );
	CHECK( RalMetal_PresentSetDirectionalLighting( present,
		&directionalLightingReceipt ) );
	CHECK( RalMetal_PresentAcquire( present, &coreReceipt, &layerReceipt, 78u,
		&drawableReceipt ) );
	/* Rewinding a rejected frame must not reset the committed timeline/pools. */
	{
		const int savedTime = frontend.worldSnapshot.timeMs;
		const uint32_t savedCount = frontend.localIrradianceEntityCount;
		const ralMetalPresentReceipt_t before = presentReceipt;
		frontend.worldSnapshot.timeMs = 100;
		frontend.localIrradianceEntityCount = savedCount + 1u;
		CHECK( !RalMetal_PresentClearAndSubmit( present, &coreReceipt, &layerReceipt,
			&drawableReceipt, &frontend, sdrClear, 79u, &presentReceipt ) );
		CHECK( !memcmp( &presentReceipt, &before, sizeof( before ) ) );
		frontend.worldSnapshot.timeMs = savedTime;
		frontend.localIrradianceEntityCount = savedCount;
	}
	CHECK( RalMetal_PresentClearAndSubmit( present, &coreReceipt, &layerReceipt,
		&drawableReceipt, &frontend, sdrClear, 79u, &presentReceipt ) );
	CHECK( presentReceipt.loweredEffectDecalCount == 1u
		&& presentReceipt.loweredEffectBeamCount == 1u );
	CHECK( presentReceipt.directionalStaticDrawCount == 1u
		&& presentReceipt.legacyLightmapDrawCount == 1u
		&& presentReceipt.surfaceLightingBindingDigest != 0u );
	CHECK( !RalMetal_PresentClearAndSubmit( present, &coreReceipt, &layerReceipt,
		&drawableReceipt, NULL, sdrClear, 80u, &presentBefore ) );
	CHECK( !RalMetal_PresentAcquire( present, &coreReceipt, &layerReceipt, 73u,
		&drawableBefore ) );
	exactPresent = presentReceipt;
	CHECK( RalMetal_PresentReceiptExact( &presentReceipt, &exactPresent ) );
#define MUTATE_PRESENT(field) do { exactPresent = presentReceipt; exactPresent.field++; \
	CHECK( !RalMetal_PresentReceiptExact( &presentReceipt, &exactPresent ) ); } while (0)
	MUTATE_PRESENT( coreGeneration ); MUTATE_PRESENT( presentationGeneration );
	MUTATE_PRESENT( acquireGeneration ); MUTATE_PRESENT( presentGeneration );
	MUTATE_PRESENT( frontendFrameGeneration );
	MUTATE_PRESENT( completionGeneration ); MUTATE_PRESENT( submission.generation );
	MUTATE_PRESENT( clearDigest ); MUTATE_PRESENT( readbackDigest );
	MUTATE_PRESENT( loweredWorldBatchCount );
	MUTATE_PRESENT( distantWorldCandidateCount );
	MUTATE_PRESENT( distantWorldDrawCount );
	MUTATE_PRESENT( distantWorldIndexCount );
	MUTATE_PRESENT( texturedWorldBatchCount );
	MUTATE_PRESENT( lightmappedWorldBatchCount );
	MUTATE_PRESENT( legacyLightmapDrawCount );
	MUTATE_PRESENT( directionalStaticDrawCount );
	MUTATE_PRESENT( surfaceLightingBindingDigest );
	MUTATE_PRESENT( submittedDirectLightCount );
	MUTATE_PRESENT( directLightDrawCount );
	MUTATE_PRESENT( directLightReferenceCount );
	MUTATE_PRESENT( patchWorldBatchCount );
	MUTATE_PRESENT( maskedWorldBatchCount );
	MUTATE_PRESENT( blendedWorldBatchCount );
	MUTATE_PRESENT( depthWriteWorldBatchCount );
	MUTATE_PRESENT( loweredEntityIndexCount );
	MUTATE_PRESENT( loweredEntityBatchCount );
	MUTATE_PRESENT( modelEntityCount );
	MUTATE_PRESENT( primitiveEntityCount );
	MUTATE_PRESENT( temporalEntityCount );
	MUTATE_PRESENT( localIrradianceEntityCount );
	MUTATE_PRESENT( localIrradianceDrawCount );
	MUTATE_PRESENT( unresolvedEntityCount );
	MUTATE_PRESENT( loweredEffectSpriteCount );
	MUTATE_PRESENT( loweredEffectDecalCount );
	MUTATE_PRESENT( loweredEffectRibbonCount );
	MUTATE_PRESENT( loweredEffectBeamCount );
	MUTATE_PRESENT( effectEmitterDispatchCount );
	MUTATE_PRESENT( effectParticleDrawCount );
	MUTATE_PRESENT( effectPrimitiveDroppedCount );
	MUTATE_PRESENT( lensSourceCount );
	MUTATE_PRESENT( lensVisibilityDispatchCount );
	MUTATE_PRESENT( texturedUiPrimitiveCount );
	MUTATE_PRESENT( msdfUiPrimitiveCount );
#undef MUTATE_PRESENT
	exactPresent = presentReceipt;
	exactPresent.distantWorldDepthReset = qfalse;
	CHECK( !RalMetal_PresentReceiptExact( &presentReceipt, &exactPresent ) );
	exactPresent = presentReceipt;
	exactPresent.atmosphere.fallbackReason =
		(ralAtmosphereFallbackReason_t)(
			exactPresent.atmosphere.fallbackReason + 1 );
	CHECK( !RalMetal_PresentReceiptExact( &presentReceipt, &exactPresent ) );

	createInfo.desiredWidth = 32u; createInfo.desiredHeight = 16u;
	createInfo.formatPreferences = &hdrFormat;
	CHECK( Ral_PresentationPolicyRequestFromLegacySwapInterval(
		0, 2u, &unlockedRequest ) );
	CHECK( Ral_ResolvePresentationPolicy( &unlockedRequest, &unlockedPolicy ) );
	CHECK( unlockedPolicy.preferenceCount == 5u
		&& unlockedPolicy.preferences[4].mode == RAL_PRESENT_FIFO
		&& unlockedPolicy.preferences[4].unboundedImageCount == 4u );
	createInfo.presentPreferences = unlockedPolicy.preferences;
	createInfo.presentPreferenceCount = unlockedPolicy.preferenceCount;
	CHECK( RalMetal_PresentReconfigure( present, &coreReceipt, &layerReceipt,
		&createInfo, 80u, &layerReceipt ) );
	CHECK( layerReceipt.selected.format == RAL_FORMAT_R16G16B16A16_SFLOAT
		&& layerReceipt.selected.colorSpace == RAL_COLORSPACE_DISPLAY_P3
		&& layerReceipt.selected.presentMode == RAL_PRESENT_IMMEDIATE
		&& layerReceipt.displaySyncEnabled == qfalse
		&& layerReceipt.extendedDynamicRange == qtrue
		&& layerReceipt.selected.width == 32u && layerReceipt.selected.imageCount == 3u );
	CHECK( !RalMetal_PresentReconfigure( present, &coreReceipt, &layerReceipt,
		&createInfo, 80u, &exactLayer ) );
	CHECK( RalMetal_PresentAcquire( present, &coreReceipt, &layerReceipt, 81u,
		&drawableReceipt ) );
	CHECK( drawableReceipt.format == RAL_FORMAT_R16G16B16A16_SFLOAT );
	CHECK( !RalMetal_PresentRequestCapture( present ) );
	CHECK( RalMetal_PresentClearAndSubmit( present, &coreReceipt, &layerReceipt,
		&drawableReceipt, NULL, hdrClear, 82u, &presentReceipt ) );
	CHECK( presentReceipt.readbackDigest != 0u
		&& presentReceipt.readbackByteCount == 8u );
	CHECK( presentReceipt.colorSpace == RAL_COLORSPACE_DISPLAY_P3
		&& presentReceipt.presentMode == RAL_PRESENT_IMMEDIATE );
	CHECK( RalMetal_PresentAcquire( present, &coreReceipt, &layerReceipt, 83u,
		&drawableReceipt ) );
	CHECK( !RalMetal_PresentClearAndSubmit( present, &coreReceipt, &layerReceipt,
		&drawableReceipt, NULL, hdrClear, 82u, &presentBefore ) );
	/* The pre-reconfigure beam must not survive while decals get reset. */
	CHECK( RenderSubmission_EndFrame( &frontend, 77u, &submissionReceipt ) );
	CHECK( RenderSubmission_BeginFrame( &frontend, 84u ) );
	CHECK( RenderSubmission_RenderScene( &frontend, &worldView, 0 ) );
	CHECK( RalMetal_PresentClearAndSubmit( present, &coreReceipt, &layerReceipt,
		&drawableReceipt, &frontend, hdrClear, 84u, &presentReceipt ) );
	CHECK( presentReceipt.loweredEffectDecalCount == 0u
		&& presentReceipt.loweredEffectBeamCount == 0u );
	CHECK( RalMetal_PresentAcquire( present, &coreReceipt, &layerReceipt, 85u,
		&drawableReceipt ) );
	/* Exercise the shared capacity, not the former Metal-only 256 slot cap.
	 * A full pool drops new births explicitly; a successful expiry retires it. */
	CHECK( RenderSubmission_EndFrame( &frontend, 84u, &submissionReceipt ) );
	CHECK( RenderSubmission_BeginFrame( &frontend, 86u ) );
	for ( uint32_t i = 0u; i < RENDER_SUBMISSION_MAX_EFFECT_DECALS; ++i )
		CHECK( RenderSubmission_AddEffectDecal( &frontend, &effectDecal ) );
	CHECK( RenderSubmission_RenderScene( &frontend, &worldView, 0 ) );
	CHECK( RalMetal_PresentClearAndSubmit( present, &coreReceipt, &layerReceipt,
		&drawableReceipt, &frontend, hdrClear, 86u, &presentReceipt ) );
	CHECK( presentReceipt.loweredEffectDecalCount == RENDER_SUBMISSION_MAX_EFFECT_DECALS
		&& presentReceipt.loweredEntityIndexCount == 6u * RENDER_SUBMISSION_MAX_EFFECT_DECALS
		&& presentReceipt.effectPrimitiveDroppedCount == 0u );
	CHECK( RenderSubmission_EndFrame( &frontend, 86u, &submissionReceipt ) );
	CHECK( RenderSubmission_BeginFrame( &frontend, 88u ) );
	CHECK( RenderSubmission_AddEffectDecal( &frontend, &effectDecal ) );
	CHECK( RenderSubmission_RenderScene( &frontend, &worldView, 0 ) );
	CHECK( RalMetal_PresentAcquire( present, &coreReceipt, &layerReceipt, 87u,
		&drawableReceipt ) );
	CHECK( RalMetal_PresentClearAndSubmit( present, &coreReceipt, &layerReceipt,
		&drawableReceipt, &frontend, hdrClear, 88u, &presentReceipt ) );
	CHECK( presentReceipt.loweredEffectDecalCount == RENDER_SUBMISSION_MAX_EFFECT_DECALS
		&& presentReceipt.effectPrimitiveDroppedCount == 1u );
	CHECK( RenderSubmission_EndFrame( &frontend, 88u, &submissionReceipt ) );
	CHECK( RenderSubmission_BeginFrame( &frontend, 90u ) );
	worldView.time = 2000;
	CHECK( RenderSubmission_RenderScene( &frontend, &worldView, 0 ) );
	CHECK( RalMetal_PresentAcquire( present, &coreReceipt, &layerReceipt, 89u,
		&drawableReceipt ) );
	CHECK( RalMetal_PresentClearAndSubmit( present, &coreReceipt, &layerReceipt,
		&drawableReceipt, &frontend, hdrClear, 90u, &presentReceipt ) );
	CHECK( presentReceipt.loweredEffectDecalCount == 0u
		&& presentReceipt.effectPrimitiveDroppedCount == 0u );
	CHECK( RalMetal_PresentAcquire( present, &coreReceipt, &layerReceipt, 91u,
		&drawableReceipt ) );
	CHECK( RalMetal_PresentSetDirectionalLighting( present, NULL ) );
	CHECK( RalMetal_LightingDestroy( core, &coreReceipt, directionalLighting,
		&directionalLightingReceipt ) );
	directionalLighting = NULL;
	memset( &lossEvent, 0, sizeof( lossEvent ) );
	lossEvent.backendType = RAL_BACKEND_METAL;
	lossEvent.cause = RAL_MEMORY_FAILURE_DEVICE_LOST;
	lossEvent.memoryClass = RAL_ALLOCATION_DEVICE_LOCAL;
	lossEvent.residency = RAL_ALLOCATION_RESIDENCY_PERMANENT;
	lossEvent.criticality = RAL_MEMORY_CRITICALITY_REQUIRED;
	lossEvent.requestedBytes = 64u; lossEvent.attempt = 1u;
	lossEvent.maxAttempts = 1u; lossEvent.liveParent = qtrue;
	CHECK( RalMetal_CorePublishDeviceLoss( core, &lossEvent, &lossReceipt ) );
	CHECK( !RalMetal_PresentClearAndSubmit( present, &coreReceipt, &layerReceipt,
		&drawableReceipt, NULL, hdrClear, 92u, &presentBefore ) );
	RenderSubmission_CancelFrame( &frontend );
	RalMetal_PresentDestroy( present );
	RalMetal_CoreDestroy( core );
	puts( "ral metal presentation: PASS" );
	return 0;
}
