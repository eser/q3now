// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "vk_temporal_resolve_authority.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { \
	fprintf( stderr, "check failed at %s:%d: %s\n", __FILE__, __LINE__, #x ); \
	return 1; } } while ( 0 )

qboolean VK_TemporalMainActivationReceiptExact(
		const vkTemporalMainActivationReceipt_t *a,
		const vkTemporalMainActivationReceipt_t *b ) {
	return a && b && a->ready == qtrue && b->ready == qtrue
		&& memcmp( a, b, sizeof( *a ) ) == 0 ? qtrue : qfalse;
}

typedef struct {
	vkTemporalResolveAuthorityInput_t input;
	ralTemporalFramePlan_t plan;
	temporalHistoryFrameView_t history;
	vkTemporalMainActivationReceipt_t activation;
	vkTemporalMotionMaterializationReceipt_t motion;
	vkTemporalMotionMaterializationProductView_t motionProducts;
	vkTemporalResolvedHdrReceipt_t resolved;
} Fixture;

static void *P( uintptr_t n ) { return (void *)( n * 0x100u + 0x1000u ); }

static Fixture MakeFixture( void ) {
	Fixture f;
	vkTemporalMotionRecordingAuthority_t *a;
	memset( &f, 0, sizeof( f ) );
	f.input.backend = (ralBackend_t *)P( 1 );
	f.input.currentColor = (ralTexture_t *)P( 2 );
	f.input.currentDepth = (ralTexture_t *)P( 3 );
	f.input.sceneColorAttachmentGeneration = 19;
	f.input.commandSlot = 1; f.input.frameCount = 2;
	f.input.zNear = 4.0f; f.input.zFar = 8192.0f;
	f.plan.frameId = 101; f.plan.generation = 7; f.plan.enabled = 1;
	f.plan.historyValid = 1; f.plan.historyReadIndex = 0;
	f.plan.historyWriteIndex = 1;
	f.plan.sceneJitterPixels[0] = 0.25f;
	f.plan.sceneJitterPixels[1] = -0.5f;
	f.plan.sceneJitterUv[0] = 0.25f / 1280.0f;
	f.plan.sceneJitterUv[1] = -0.5f / 720.0f;
	f.plan.previousJitterPixels[0] = -0.25f;
	f.plan.previousJitterPixels[1] = 0.5f;
	f.history.historyValid = qtrue; f.history.readIndex = 0;
	f.history.writeIndex = 1;
	f.history.readColor = (ralTexture_t *)P( 4 );
	f.history.readColorView = (ralTextureView_t *)P( 5 );
	f.history.readDepth = (ralTexture_t *)P( 6 );
	f.history.readDepthView = (ralTextureView_t *)P( 7 );
	f.history.writeColor = (ralTexture_t *)P( 8 );
	f.history.writeColorView = (ralTextureView_t *)P( 9 );
	f.history.writeDepth = (ralTexture_t *)P( 10 );
	f.history.writeDepthView = (ralTextureView_t *)P( 11 );
	f.history.committed.frameId = 100;
	f.history.committed.worldIndex = 2;
	f.history.committed.planGeneration = 7;
	f.history.committed.allocationGeneration = 13;
	f.history.committed.historyIndex = 0;
	f.history.committed.width = 1280; f.history.committed.height = 720;
	f.history.committed.topologyEpoch = 5;
	f.history.committed.color = f.history.readColor;
	f.history.committed.colorView = f.history.readColorView;
	f.history.committed.depth = f.history.readDepth;
	f.history.committed.depthView = f.history.readDepthView;
	f.history.committed.source.backend = f.input.backend;
	f.history.committed.source.sourceSceneColor = (ralTexture_t *)P( 24 );
	f.history.committed.source.sourcePostprocessGroup = (ralBindGroup_t *)P( 25 );
	f.history.committed.source.sourceHistogramGroup = (ralBindGroup_t *)P( 26 );
	f.history.committed.source.sourceColor = (ralTexture_t *)P( 27 );
	f.history.committed.source.sourceColorView = (ralTextureView_t *)P( 28 );
	f.history.committed.source.postprocessGroup = (ralBindGroup_t *)P( 29 );
	f.history.committed.source.histogramGroup = (ralBindGroup_t *)P( 30 );
	f.history.committed.source.batchToken = 54;
	f.history.committed.source.frameId = 100;
	f.history.committed.source.contentSerial = 40;
	f.history.committed.source.commandSlot = 0;
	f.history.committed.source.frameCount = 2;
	f.history.committed.source.worldIndex = 2;
	f.history.committed.source.width = 1280;
	f.history.committed.source.height = 720;
	f.history.committed.source.topologyEpoch = 5;
	f.history.committed.source.planGeneration = 7;
	f.history.committed.source.sceneColorAttachmentGeneration = 18;
	f.history.committed.source.targetAllocationGeneration = 36;
	f.history.committed.source.resolveOwnerAllocationGeneration = 35;
	f.history.committed.source.storeOwnerAllocationGeneration = 34;
	f.history.committed.source.sceneFormat = RAL_FORMAT_R16G16B16A16_SFLOAT;
	f.history.committed.source.producer =
		TEMPORAL_HISTORY_WRITE_RESOLVED_FEEDBACK;
	f.history.committed.valid = qtrue;
	a = &f.activation.authority;
	a->token = 55; a->frameId = 101; a->worldIndex = 2;
	a->width = 1280; a->height = 720; a->topologyEpoch = 5;
	a->planGeneration = 7; a->targetAllocationGeneration = 17;
	a->pipelineLayoutAllocationGeneration = 23;
	a->materializationGeneration = 29; a->pipelineTableGeneration = 31;
	a->geometryBufferSize = 65536; a->uniformItemSize = 256;
	a->requiredCapacity = 256; a->rawEntMatCapacity = 256;
	a->rawEntMatAllocationGeneration = 32; a->payloadAllocationGeneration = 33;
	a->payloadLayoutGeneration = 34;
	a->frameIndex = 1;
	f.activation.prepared = 3;
	f.activation.temporalSegments = 2; f.activation.written = 1;
	f.activation.invalidated = 1; f.activation.preserved = 1;
	f.activation.drawSequence.lane0 = 0x1234;
	f.activation.drawSequence.lane1 = 0x5678;
	f.activation.drawSequence.count = 3;
	f.activation.taggedSequence.lane0 = 0x2234;
	f.activation.taggedSequence.lane1 = 0x6678;
	f.activation.taggedSequence.count = 3;
	f.activation.taggedSequence.genericCount = 3;
	f.activation.auxiliaryCleared = qtrue;
	f.activation.depthStoreRequired = qtrue;
	f.activation.stencilStoreRequired = qtrue;
	f.activation.ready = qtrue;
	f.motion.worldIndex = 2; f.motion.width = 1280; f.motion.height = 720;
	f.motion.topologyEpoch = 5; f.motion.planGeneration = 7;
	f.motion.targetAllocationGeneration = 17;
	f.motion.pipelineLayoutAllocationGeneration = 23;
	f.motion.allocationGeneration = 29; f.motion.ready = qtrue;
	f.motionProducts.scene = f.input.currentColor;
	f.motionProducts.depth = f.input.currentDepth;
	f.motionProducts.velocity = (ralTexture_t *)P( 12 );
	f.motionProducts.validity = (ralTexture_t *)P( 13 );
	f.motionProducts.velocityView = (ralTextureView_t *)P( 14 );
	f.motionProducts.validityView = (ralTextureView_t *)P( 15 );
	f.motionProducts.pipelineLayout = (ralPipelineLayout_t *)P( 16 );
	f.motionProducts.rawPipelineLayout = (VkPipelineLayout)P( 17 );
	f.motionProducts.targetAllocationGeneration = 17;
	f.motionProducts.pipelineLayoutAllocationGeneration = 23;
	f.motionProducts.allocationGeneration = 29;
	f.resolved.backend = f.input.backend;
	f.resolved.target = (ralTexture_t *)P( 18 );
	f.resolved.targetView = (ralTextureView_t *)P( 19 );
	f.resolved.postprocessGroup = (ralBindGroup_t *)P( 20 );
	f.resolved.histogramGroup = (ralBindGroup_t *)P( 21 );
	f.resolved.currentSceneColor = f.input.currentColor;
	f.resolved.currentPostprocessGroup = (ralBindGroup_t *)P( 22 );
	f.resolved.currentHistogramGroup = (ralBindGroup_t *)P( 23 );
	f.resolved.worldIndex = 2; f.resolved.width = 1280;
	f.resolved.height = 720; f.resolved.topologyEpoch = 5;
	f.resolved.planGeneration = 7;
	f.resolved.sceneColorAttachmentGeneration = 19;
	f.resolved.allocationGeneration = 37;
	f.resolved.sceneFormat = RAL_FORMAT_R16G16B16A16_SFLOAT;
	f.resolved.ready = qtrue;
	f.input.plan = &f.plan; f.input.history = &f.history;
	f.input.activation = &f.activation; f.input.motion = &f.motion;
	f.input.motionProducts = &f.motionProducts; f.input.resolved = &f.resolved;
	return f;
}

static void RepairPointers( Fixture *f ) {
	f->input.plan = &f->plan; f->input.history = &f->history;
	f->input.activation = &f->activation; f->input.motion = &f->motion;
	f->input.motionProducts = &f->motionProducts; f->input.resolved = &f->resolved;
}

static int RejectsAtomically( Fixture *f ) {
	vkTemporalResolveAuthorityReceipt_t receipt, beforeReceipt;
	vkTemporalResolveProductView_t products, beforeProducts;
	memset( &receipt, 0xa5, sizeof( receipt ) ); beforeReceipt = receipt;
	memset( &products, 0x5a, sizeof( products ) ); beforeProducts = products;
	RepairPointers( f );
	if ( VK_TemporalResolveBuildAuthority( &f->input, &receipt, &products ) )
		return 0;
	return memcmp( &receipt, &beforeReceipt, sizeof( receipt ) ) == 0
		&& memcmp( &products, &beforeProducts, sizeof( products ) ) == 0;
}

int main( void ) {
	Fixture base = MakeFixture(), f;
	vkTemporalResolveAuthorityReceipt_t receipt;
	vkTemporalResolveProductView_t products;
	RepairPointers( &base );
	CHECK( VK_TemporalResolveBuildAuthority( &base.input, &receipt, &products ) );
	CHECK( receipt.ready && receipt.batchToken == 55 && receipt.frameId == 101
		&& receipt.previousFrameId == 100 && receipt.worldIndex == 2
		&& receipt.historyReadIndex == 0 && receipt.historyWriteIndex == 1
		&& receipt.historyAllocationGeneration == 13
		&& receipt.motionMaterializationGeneration == 29
		&& receipt.pipelineTableGeneration == 31
		&& receipt.sceneColorAttachmentGeneration == 19
		&& receipt.resolvedTargetAllocationGeneration == 37
		&& receipt.temporalSegments == 2 && receipt.written == 1
		&& receipt.invalidated == 1 && receipt.drawSequence.count == 3
		&& fabsf( receipt.currentEffectiveJitterUv[0] - 0.25f / 1280.0f ) < 1e-9f
		&& fabsf( receipt.currentEffectiveJitterUv[1] - 0.5f / 720.0f ) < 1e-9f
		&& fabsf( receipt.previousEffectiveJitterUv[0] + 0.25f / 1280.0f ) < 1e-9f
		&& fabsf( receipt.previousEffectiveJitterUv[1] + 0.5f / 720.0f ) < 1e-9f
		&& receipt.zNear == 4.0f && receipt.zFar == 8192.0f );
	CHECK( products.backend == base.input.backend
		&& products.currentColor == base.input.currentColor
		&& products.currentDepth == base.input.currentDepth
		&& products.previousColor == base.history.readColor
		&& products.previousDepth == base.history.readDepth
		&& products.velocity == base.motionProducts.velocity
		&& products.velocityView == base.motionProducts.velocityView
		&& products.validity == base.motionProducts.validity
		&& products.validityView == base.motionProducts.validityView
		&& products.resolvedTarget == base.resolved.target );

#define REJECT(mut) do { f = base; RepairPointers( &f ); mut; CHECK( RejectsAtomically( &f ) ); } while ( 0 )
	REJECT( f.input.backend = NULL );
	REJECT( f.input.currentColor = NULL );
	REJECT( f.input.currentDepth = NULL );
	REJECT( f.input.sceneColorAttachmentGeneration = 0 );
	REJECT( f.input.commandSlot = f.input.frameCount );
	REJECT( f.input.zNear = 0.0f );
	REJECT( f.input.zFar = f.input.zNear );
	REJECT( f.input.zNear = NAN );
	REJECT( f.plan.enabled = 0 );
	REJECT( f.plan.historyValid = 0 );
	REJECT( f.plan.frameId++ );
	REJECT( f.plan.generation++ );
	REJECT( f.plan.historyReadIndex = f.plan.historyWriteIndex );
	REJECT( f.plan.sceneJitterPixels[0] = NAN );
	REJECT( f.plan.sceneJitterPixels[1] = INFINITY );
	REJECT( f.plan.sceneJitterUv[0] = NAN );
	REJECT( f.plan.sceneJitterUv[1] += 0.001f );
	REJECT( f.plan.previousJitterPixels[0] = NAN );
	REJECT( f.plan.previousJitterPixels[1] = -INFINITY );
	REJECT( f.activation.ready = qfalse );
	REJECT( f.activation.temporalSegments = 0 );
	REJECT( f.activation.written++ );
	REJECT( f.activation.drawSequence.count++ );
	REJECT( f.activation.auxiliaryCleared = qfalse );
	REJECT( f.activation.depthStoreRequired = qfalse );
	REJECT( f.activation.stencilStoreRequired = qfalse );
	REJECT( f.activation.authority.token = 0 );
	REJECT( f.activation.authority.frameId++ );
	REJECT( f.activation.authority.worldIndex++ );
	REJECT( f.activation.authority.width++ );
	REJECT( f.activation.authority.height++ );
	REJECT( f.activation.authority.topologyEpoch++ );
	REJECT( f.activation.authority.planGeneration++ );
	REJECT( f.activation.authority.targetAllocationGeneration++ );
	REJECT( f.activation.authority.pipelineLayoutAllocationGeneration++ );
	REJECT( f.activation.authority.materializationGeneration++ );
	REJECT( f.activation.authority.pipelineTableGeneration = 0 );
	REJECT( f.activation.authority.frameIndex++ );
	REJECT( f.motion.ready = qfalse );
	REJECT( f.motion.worldIndex++ );
	REJECT( f.motion.width++ );
	REJECT( f.motion.height++ );
	REJECT( f.motion.topologyEpoch++ );
	REJECT( f.motion.planGeneration++ );
	REJECT( f.motion.targetAllocationGeneration++ );
	REJECT( f.motion.pipelineLayoutAllocationGeneration++ );
	REJECT( f.motion.allocationGeneration++ );
	REJECT( f.motionProducts.scene = (ralTexture_t *)P( 30 ) );
	REJECT( f.motionProducts.depth = (ralTexture_t *)P( 30 ) );
	REJECT( f.motionProducts.velocity = NULL );
	REJECT( f.motionProducts.velocityView = NULL );
	REJECT( f.motionProducts.validity = NULL );
	REJECT( f.motionProducts.validityView = NULL );
	REJECT( f.motionProducts.targetAllocationGeneration++ );
	REJECT( f.motionProducts.pipelineLayoutAllocationGeneration++ );
	REJECT( f.motionProducts.allocationGeneration++ );
	REJECT( f.history.historyValid = qfalse );
	REJECT( f.history.readIndex++ );
	REJECT( f.history.writeIndex = f.history.readIndex );
	REJECT( f.history.committed.valid = qfalse );
	REJECT( f.history.committed.frameId-- );
	REJECT( f.history.committed.worldIndex++ );
	REJECT( f.history.committed.planGeneration++ );
	REJECT( f.history.committed.allocationGeneration = 0 );
	REJECT( f.history.committed.historyIndex++ );
	REJECT( f.history.committed.width++ );
	REJECT( f.history.committed.height++ );
	REJECT( f.history.committed.topologyEpoch++ );
	REJECT( f.history.committed.color = (ralTexture_t *)P( 30 ) );
	REJECT( f.history.committed.colorView = (ralTextureView_t *)P( 30 ) );
	REJECT( f.history.committed.depth = (ralTexture_t *)P( 30 ) );
	REJECT( f.history.committed.depthView = (ralTextureView_t *)P( 30 ) );
	REJECT( f.history.writeColor = NULL );
	REJECT( f.history.writeDepth = NULL );
	REJECT( f.resolved.ready = qfalse );
	REJECT( f.resolved.backend = (ralBackend_t *)P( 30 ) );
	REJECT( f.resolved.currentSceneColor = (ralTexture_t *)P( 30 ) );
	REJECT( f.resolved.worldIndex++ );
	REJECT( f.resolved.width++ );
	REJECT( f.resolved.height++ );
	REJECT( f.resolved.topologyEpoch++ );
	REJECT( f.resolved.planGeneration++ );
	REJECT( f.resolved.sceneColorAttachmentGeneration++ );
	REJECT( f.resolved.allocationGeneration = 0 );
	REJECT( f.resolved.sceneFormat = RAL_FORMAT_R8G8B8A8_UNORM );
	REJECT( f.motionProducts.velocity = f.input.currentColor );
	REJECT( f.motionProducts.validity = f.history.readDepth );
	REJECT( f.motionProducts.validityView = f.history.readColorView );
	REJECT( f.resolved.target = f.motionProducts.velocity );
	REJECT( f.resolved.targetView = f.history.readColorView );
	REJECT( f.resolved.targetView = (ralTextureView_t *)f.input.currentColor );
#undef REJECT

	puts( "vk temporal resolve authority contract: PASS" );
	return 0;
}
