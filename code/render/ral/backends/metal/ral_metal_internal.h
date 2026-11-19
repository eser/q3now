// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#ifndef WIRED_RAL_METAL_INTERNAL_H
#define WIRED_RAL_METAL_INTERNAL_H

#include "ral_metal_core.h"
#include "ral_metal_bind_group.h"
#include "ral_metal_pipeline.h"
#include "ral_metal_present.h"
#include "ral_command_lifecycle.h"

#import <Metal/Metal.h>

id<MTLDevice> RalMetal_CoreNativeDevice( ralMetalCore_t *core );
id<MTLCommandQueue> RalMetal_CoreNativeQueue( ralMetalCore_t *core );
/* Borrowed adapter-private cache identity; invalidated by world unload. */
id<MTLBuffer> RalMetal_PresentNativeWorldBuffer( const ralMetalPresent_t *present,
	int worldIndex, qboolean indices );
/* Borrowed current GPU weather pool; read only after queue completion. */
id<MTLBuffer> RalMetal_PresentNativeWeatherBuffer( const ralMetalPresent_t *present );
id<MTLBuffer> RalMetal_PresentNativeHeightgrid( const ralMetalPresent_t *present, int worldIndex );
/* Borrowed world-scoped source cube; read only after queue completion. */
id<MTLTexture> RalMetal_PresentNativeSkySource( const ralMetalPresent_t *present );
/* Read-only adapter-private simulation ownership diagnostics. */
typedef struct {
	qboolean atmosphereValid, atmosphereParity, weatherParity;
	float weatherTimeline;
	qboolean particleParity, particlePlanReady;
	float particleTimeline;
	uint32_t particleNextSlot;
} ralMetalSimulationHistory_t;
qboolean RalMetal_PresentSimulationHistory( const ralMetalPresent_t *present,
	ralMetalSimulationHistory_t *outHistory );
qboolean RalMetal_CoreMatchesReceipt( ralMetalCore_t *core,
	const ralMetalCoreReceipt_t *receipt );
qboolean RalMetal_CoreBeginCommand( ralMetalCore_t *core, uint64_t generation,
	ralCommandLifecycle_t *outLifecycle, ralCommandReceipt_t *outRecording );
qboolean RalMetal_CorePublishSubmission( ralMetalCore_t *core,
	ralCommandLifecycle_t *commandLifecycle,
	const ralCommandReceipt_t *executable,
	ralSubmissionReceipt_t *outSubmission );
id<MTLBuffer> RalMetal_BindGroupNativeArgumentBuffer( ralMetalBindGroup_t *group );
id RalMetal_BindGroupNativeResource( ralMetalBindGroup_t *group, uint32_t index );
void RalMetal_BindGroupUseFragmentResources( ralMetalBindGroup_t *group,
	id<MTLRenderCommandEncoder> encoder );
id<MTLRenderPipelineState> RalMetal_PipelineNativeState( ralMetalPipeline_t *pipeline );
id<MTLDepthStencilState> RalMetal_PipelineNativeDepthState( ralMetalPipeline_t *pipeline );
#endif
