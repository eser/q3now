// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "vk_temporal_resolve.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if ( !(x) ) { \
	fprintf( stderr, "CHECK failed: %s:%d: %s\n", __FILE__, __LINE__, #x ); \
	return 1; } } while ( 0 )

qboolean VK_TemporalMainActivationReceiptExact(
		const vkTemporalMainActivationReceipt_t *a,
		const vkTemporalMainActivationReceipt_t *b ) {
	return a && b && a->ready == qtrue && b->ready == qtrue
		&& memcmp( a, b, sizeof( *a ) ) == 0 ? qtrue : qfalse;
}

typedef struct { char role; const void *pointer; } destroyEvent_t;
static uintptr_t nextHandle = 0x100000u;
static uint32_t createCount, failStep, destroyCount, commandCount;
static void *aliasAtStep;
static destroyEvent_t destroys[2048];
static ralBindEntry_t capturedEntries[8];
static ralBindingValue_t capturedValues[2][8];
static uint32_t capturedGroupCount;
static ralComputePipelineCreateInfo_t capturedPipeline;
static vkTemporalResolvePush_t capturedPush;
static struct {
	char kind;
	const void *object;
	uint32_t a, b, c;
} commands[32];

static void *NewHandle( void ) {
	++createCount;
	if ( createCount == failStep ) return aliasAtStep;
	nextHandle += 0x100u;
	return (void *)nextHandle;
}

static void TraceDestroy( char role, const void *p ) {
	if ( destroyCount < 2048 ) destroys[destroyCount] = (destroyEvent_t){ role, p };
	++destroyCount;
}

static qboolean DestroyedSince( uint32_t start, const void *p ) {
	uint32_t i;
	for ( i = start; i < destroyCount && i < 2048; ++i )
		if ( destroys[i].pointer == p ) return qtrue;
	return qfalse;
}

ralTextureView_t *Ral_CreateTextureView( ralBackend_t *backend,
		const ralTextureViewCreateInfo_t *ci ) {
	(void)backend; (void)ci; return (ralTextureView_t *)NewHandle();
}
ralSampler_t *Ral_CreateSampler( ralBackend_t *backend,
		const ralSamplerCreateInfo_t *ci ) {
	(void)backend; (void)ci; return (ralSampler_t *)NewHandle();
}
ralBindGroupLayout_t *Ral_CreateBindGroupLayout( ralBackend_t *backend,
		const ralBindGroupLayoutCreateInfo_t *ci ) {
	(void)backend;
	if ( ci && ci->numEntries == 8 )
		memcpy( capturedEntries, ci->entries, sizeof( capturedEntries ) );
	return (ralBindGroupLayout_t *)NewHandle();
}
ralBindGroup_t *Ral_CreateBindGroup( ralBackend_t *backend,
		const ralBindGroupCreateInfo_t *ci ) {
	(void)backend;
	if ( ci && capturedGroupCount < 2 && ci->numValues == 8 ) {
		memcpy( capturedValues[capturedGroupCount], ci->values,
			sizeof( capturedValues[capturedGroupCount] ) );
		++capturedGroupCount;
	}
	return (ralBindGroup_t *)NewHandle();
}
ralPipeline_t *Ral_CreateComputePipeline( ralBackend_t *backend,
		const ralComputePipelineCreateInfo_t *ci ) {
	(void)backend; if ( ci ) capturedPipeline = *ci;
	return (ralPipeline_t *)NewHandle();
}
void Ral_DestroyTextureView( ralTextureView_t *p ) { TraceDestroy( 'V', p ); }
void Ral_DestroySampler( ralSampler_t *p ) { TraceDestroy( 'S', p ); }
void Ral_DestroyBindGroupLayout( ralBindGroupLayout_t *p ) { TraceDestroy( 'L', p ); }
void Ral_DestroyBindGroup( ralBindGroup_t *p ) { TraceDestroy( 'G', p ); }
void Ral_DestroyPipeline( ralPipeline_t *p ) { TraceDestroy( 'P', p ); }

static void TraceCommand( char kind, const void *object,
		uint32_t a, uint32_t b, uint32_t c ) {
	if ( commandCount < 32 ) {
		commands[commandCount].kind = kind;
		commands[commandCount].object = object;
		commands[commandCount].a = a;
		commands[commandCount].b = b;
		commands[commandCount].c = c;
	}
	++commandCount;
}

void Ral_CmdTransitionTexture( ralCommandBuffer_t *cb, ralTexture_t *texture,
		uint32_t srcStage, uint32_t dstStage, VkImageLayout newLayout ) {
	(void)cb; TraceCommand( 'T', texture, srcStage, dstStage, (uint32_t)newLayout );
}
void Ral_CmdBindPipeline( ralCommandBuffer_t *cb, ralPipeline_t *pipeline ) {
	(void)cb; TraceCommand( 'P', pipeline, 0, 0, 0 );
}
void Ral_CmdBindBindGroup( ralCommandBuffer_t *cb, uint32_t setIndex,
		ralBindGroup_t *group ) {
	(void)cb; TraceCommand( 'G', group, setIndex, 0, 0 );
}
void Ral_CmdPushConstants( ralCommandBuffer_t *cb, uint32_t stages,
		uint32_t offset, uint32_t size, const void *data ) {
	(void)cb; (void)offset;
	if ( size == sizeof( capturedPush ) ) memcpy( &capturedPush, data, size );
	TraceCommand( 'U', NULL, stages, size, 0 );
}
void Ral_CmdDispatch( ralCommandBuffer_t *cb, uint32_t x, uint32_t y,
		uint32_t z ) {
	(void)cb; TraceCommand( 'D', NULL, x, y, z );
}

/* H2 owner symbols are linked but unused by this focused test. */
static ralCaps_t caps;
const ralCaps_t *Ral_GetCaps( ralBackend_t *backend ) { (void)backend; return &caps; }
qboolean Ral_TextureFormatSupports( ralBackend_t *backend, ralFormat_t format,
		ralTextureUsage_t usage ) {
	(void)backend; (void)format; (void)usage; return qtrue;
}
ralTexture_t *Ral_CreateTexture( ralBackend_t *backend,
		const ralTextureCreateInfo_t *ci ) {
	(void)backend; (void)ci; return (ralTexture_t *)NewHandle();
}
void Ral_DestroyTexture( ralTexture_t *p ) { TraceDestroy( 'T', p ); }

typedef struct {
	vkTemporalResolveKey_t key;
	vkTemporalResolveAuthorityReceipt_t authority;
	vkTemporalResolveProductView_t products;
	vkHdrPostprocessSource_t current;
	vkTemporalResolvedHdrReceipt_t target;
	vkTemporalMainActivationReceipt_t activation;
} fixture_t;

static void *H( uintptr_t value ) { return (void *)value; }

static fixture_t MakeFixture( void ) {
	fixture_t f;
	uint32_t words[4] = { 1, 2, 3, 4 };
	(void)words;
	memset( &f, 0, sizeof( f ) );
	f.key.backend = H( 0x100 );
	f.key.width = 1280; f.key.height = 720; f.key.topologyEpoch = 3;
	f.key.sceneColorAttachmentGeneration = 5;
	f.key.historyAllocationGeneration = 7;
	f.key.motionTargetAllocationGeneration = 9;
	f.key.resolvedTargetAllocationGeneration = 11;
	f.key.currentColor = H( 0x200 ); f.key.currentDepth = H( 0x300 );
	f.key.historyColor[0] = H( 0x400 ); f.key.historyColorView[0] = H( 0x410 );
	f.key.historyDepth[0] = H( 0x420 ); f.key.historyDepthView[0] = H( 0x430 );
	f.key.historyColor[1] = H( 0x500 ); f.key.historyColorView[1] = H( 0x510 );
	f.key.historyDepth[1] = H( 0x520 ); f.key.historyDepthView[1] = H( 0x530 );
	f.key.velocity = H( 0x600 ); f.key.velocityView = H( 0x610 );
	f.key.validity = H( 0x620 ); f.key.validityView = H( 0x630 );
	f.key.resolvedTarget = H( 0x700 ); f.key.resolvedTargetView = H( 0x710 );
	f.key.computeSpirv = (const uint32_t *)(uintptr_t)0x800;
	f.key.computeSpirvSize = 16;

	f.authority.batchToken = 101; f.authority.frameId = 202;
	f.authority.previousFrameId = 201; f.authority.worldIndex = 0;
	f.authority.width = 1280; f.authority.height = 720;
	f.authority.topologyEpoch = 3; f.authority.planGeneration = 13;
	f.authority.commandSlot = 1; f.authority.frameCount = 3;
	f.authority.historyAllocationGeneration = 7;
	f.authority.historyReadIndex = 0; f.authority.historyWriteIndex = 1;
	f.authority.previousHistorySource.backend = f.key.backend;
	f.authority.previousHistorySource.sourceSceneColor = H( 0x740 );
	f.authority.previousHistorySource.sourcePostprocessGroup = H( 0x750 );
	f.authority.previousHistorySource.sourceHistogramGroup = H( 0x760 );
	f.authority.previousHistorySource.sourceColor = H( 0x720 );
	f.authority.previousHistorySource.sourceColorView = H( 0x730 );
	f.authority.previousHistorySource.postprocessGroup = H( 0x770 );
	f.authority.previousHistorySource.histogramGroup = H( 0x780 );
	f.authority.previousHistorySource.batchToken = 100;
	f.authority.previousHistorySource.frameId = 201;
	f.authority.previousHistorySource.contentSerial = 41;
	f.authority.previousHistorySource.commandSlot = 0;
	f.authority.previousHistorySource.frameCount = 3;
	f.authority.previousHistorySource.worldIndex = 0;
	f.authority.previousHistorySource.width = 1280;
	f.authority.previousHistorySource.height = 720;
	f.authority.previousHistorySource.topologyEpoch = 3;
	f.authority.previousHistorySource.planGeneration = 13;
	f.authority.previousHistorySource.sceneColorAttachmentGeneration = 5;
	f.authority.previousHistorySource.targetAllocationGeneration = 10;
	f.authority.previousHistorySource.resolveOwnerAllocationGeneration = 12;
	f.authority.previousHistorySource.storeOwnerAllocationGeneration = 14;
	f.authority.previousHistorySource.sceneFormat =
		RAL_FORMAT_R16G16B16A16_SFLOAT;
	f.authority.previousHistorySource.producer =
		TEMPORAL_HISTORY_WRITE_RESOLVED_FEEDBACK;
	f.authority.motionTargetAllocationGeneration = 9;
	f.authority.motionPipelineLayoutAllocationGeneration = 15;
	f.authority.motionMaterializationGeneration = 17;
	f.authority.pipelineTableGeneration = 19;
	f.authority.sceneColorAttachmentGeneration = 5;
	f.authority.resolvedTargetAllocationGeneration = 11;
	f.authority.temporalSegments = 2; f.authority.written = 1;
	f.authority.invalidated = 1;
	f.authority.currentEffectiveJitterUv[0] = 0.25f / 1280.0f;
	f.authority.currentEffectiveJitterUv[1] = 0.25f / 720.0f;
	f.authority.previousEffectiveJitterUv[0] = -0.25f / 1280.0f;
	f.authority.previousEffectiveJitterUv[1] = -0.25f / 720.0f;
	f.authority.zNear = 4.0f; f.authority.zFar = 8192.0f;
	f.authority.drawSequence.lane0 = 0x1111;
	f.authority.drawSequence.lane1 = 0x2222;
	f.authority.drawSequence.count = 3;
	f.authority.ready = qtrue;

	f.products.backend = f.key.backend;
	f.products.currentColor = f.key.currentColor;
	f.products.currentDepth = f.key.currentDepth;
	f.products.previousColor = f.key.historyColor[0];
	f.products.previousColorView = f.key.historyColorView[0];
	f.products.previousDepth = f.key.historyDepth[0];
	f.products.previousDepthView = f.key.historyDepthView[0];
	f.products.velocity = f.key.velocity; f.products.velocityView = f.key.velocityView;
	f.products.validity = f.key.validity; f.products.validityView = f.key.validityView;
	f.products.resolvedTarget = f.key.resolvedTarget;
	f.products.resolvedTargetView = f.key.resolvedTargetView;

	f.current.backend = f.key.backend; f.current.attachment = f.key.currentColor;
	f.current.postprocessGroup = H( 0x900 ); f.current.histogramGroup = H( 0x910 );
	f.current.batchToken = 101; f.current.frameId = 202;
	f.current.commandSlot = 1; f.current.frameCount = 3; f.current.worldIndex = 0;
	f.current.width = 1280; f.current.height = 720;
	f.current.topologyEpoch = 3; f.current.planGeneration = 13;
	f.current.sceneColorAttachmentGeneration = 5;
	f.current.sceneFormat = RAL_FORMAT_R16G16B16A16_SFLOAT;

	f.target.backend = f.key.backend; f.target.target = f.key.resolvedTarget;
	f.target.targetView = f.key.resolvedTargetView;
	f.target.postprocessGroup = H( 0xa00 ); f.target.histogramGroup = H( 0xa10 );
	f.target.currentSceneColor = f.key.currentColor;
	f.target.currentPostprocessGroup = f.current.postprocessGroup;
	f.target.currentHistogramGroup = f.current.histogramGroup;
	f.target.worldIndex = 0; f.target.width = 1280; f.target.height = 720;
	f.target.topologyEpoch = 3; f.target.planGeneration = 13;
	f.target.sceneColorAttachmentGeneration = 5;
	f.target.allocationGeneration = 11;
	f.target.sceneFormat = RAL_FORMAT_R16G16B16A16_SFLOAT;
	f.target.ready = qtrue;

	f.activation.authority.token = 101; f.activation.authority.frameId = 202;
	f.activation.authority.worldIndex = 0; f.activation.authority.width = 1280;
	f.activation.authority.height = 720; f.activation.authority.topologyEpoch = 3;
	f.activation.authority.planGeneration = 13; f.activation.authority.frameIndex = 1;
	f.activation.authority.targetAllocationGeneration = 9;
	f.activation.authority.pipelineLayoutAllocationGeneration = 15;
	f.activation.authority.materializationGeneration = 17;
	f.activation.authority.pipelineTableGeneration = 19;
	f.activation.authority.geometryBufferSize = 65536;
	f.activation.authority.uniformItemSize = 256;
	f.activation.authority.requiredCapacity = 256;
	f.activation.authority.rawEntMatCapacity = 256;
	f.activation.authority.rawEntMatAllocationGeneration = 20;
	f.activation.authority.payloadAllocationGeneration = 21;
	f.activation.authority.payloadLayoutGeneration = 22;
	f.activation.prepared = 3;
	f.activation.temporalSegments = 2; f.activation.written = 1;
	f.activation.invalidated = 1; f.activation.preserved = 1;
	f.activation.drawSequence = f.authority.drawSequence;
	f.activation.taggedSequence.lane0 = 0x2234;
	f.activation.taggedSequence.lane1 = 0x6678;
	f.activation.taggedSequence.count = 3;
	f.activation.taggedSequence.genericCount = 3;
	f.activation.auxiliaryCleared = qtrue;
	f.activation.depthStoreRequired = qtrue;
	f.activation.stencilStoreRequired = qtrue;
	f.activation.ready = qtrue;
	f.authority.activation = f.activation;
	return f;
}

int main( void ) {
	fixture_t f = MakeFixture();
	vkTemporalResolveOwner_t owner, before;
	vkTemporalResolveOwnerReceipt_t receipt;
	vkTemporalResolveTicket_t ticket, ticketBefore, submitted;
	uint32_t baseCreates, baseCommands, baseDestroy, i, j;
	const void *borrowed[17];
	const void *live[7];

	VK_TemporalResolveInit( &owner );
	CHECK( owner.initialized && !VK_TemporalResolveHasLive( &owner ) );
	CHECK( !VK_TemporalResolveEnsureAfterFence( &owner, NULL, qfalse ) );
	CHECK( VK_TemporalResolveEnsureAfterFence( &owner, &f.key, qfalse ) );
	CHECK( VK_TemporalResolveGetReceipt( &owner, &receipt ) );
	CHECK( receipt.allocationGeneration == 1 && receipt.width == 1280
		&& receipt.height == 720 && receipt.topologyEpoch == 3 );
	CHECK( createCount == 7 && capturedGroupCount == 2 );
	for ( i = 0; i < 6; ++i ) CHECK( capturedEntries[i].binding == i
		&& capturedEntries[i].type == RAL_BIND_SAMPLED_TEXTURE );
	CHECK( capturedEntries[6].type == RAL_BIND_SAMPLER
		&& capturedEntries[7].type == RAL_BIND_STORAGE_TEXTURE );
	CHECK( capturedValues[0][2].textureView == f.key.historyColorView[0]
		&& capturedValues[1][2].textureView == f.key.historyColorView[1]
		&& capturedValues[0][3].textureView == f.key.historyDepthView[0]
		&& capturedValues[1][3].textureView == f.key.historyDepthView[1]
		&& capturedValues[0][4].textureView == f.key.velocityView
		&& capturedValues[0][5].textureView == f.key.validityView
		&& capturedValues[0][6].sampler == owner.nearestSampler
		&& capturedValues[0][7].textureView == f.key.resolvedTargetView );
	CHECK( capturedPipeline.computeSpirv == f.key.computeSpirv
		&& capturedPipeline.computeSpirvSize == f.key.computeSpirvSize
		&& capturedPipeline.pushConstantSize == sizeof( vkTemporalResolvePush_t ) );
	baseCreates = createCount;
	CHECK( VK_TemporalResolveEnsureAfterFence( &owner, &f.key, qfalse ) );
	CHECK( createCount == baseCreates );

	memset( &ticket, 0xa5, sizeof( ticket ) ); ticketBefore = ticket;
	baseCommands = commandCount;
	{
		vkTemporalResolveOwner_t bad = owner;
		bad.allocationGeneration = 0;
		CHECK( !VK_TemporalResolveRecord( &bad, H( 0xb00 ), &f.authority,
			&f.products, &f.current, &f.target, 1, &ticket ) );
		CHECK( commandCount == baseCommands
			&& memcmp( &ticket, &ticketBefore, sizeof( ticket ) ) == 0 );
		bad = owner; bad.key.currentColor = bad.key.currentDepth;
		CHECK( !VK_TemporalResolveRecord( &bad, H( 0xb00 ), &f.authority,
			&f.products, &f.current, &f.target, 1, &ticket ) );
		CHECK( commandCount == baseCommands
			&& memcmp( &ticket, &ticketBefore, sizeof( ticket ) ) == 0 );
		bad = owner; bad.pipeline = NULL;
		CHECK( !VK_TemporalResolveRecord( &bad, H( 0xb00 ), &f.authority,
			&f.products, &f.current, &f.target, 1, &ticket ) );
		CHECK( commandCount == baseCommands
			&& memcmp( &ticket, &ticketBefore, sizeof( ticket ) ) == 0 );
		bad = owner; bad.groups[f.authority.historyReadIndex] = NULL;
		CHECK( !VK_TemporalResolveRecord( &bad, H( 0xb00 ), &f.authority,
			&f.products, &f.current, &f.target, 1, &ticket ) );
		CHECK( commandCount == baseCommands
			&& memcmp( &ticket, &ticketBefore, sizeof( ticket ) ) == 0 );
	}
	{
		vkHdrPostprocessSource_t bad = f.current; bad.batchToken++;
		CHECK( !VK_TemporalResolveRecord( &owner, H( 0xb00 ), &f.authority,
			&f.products, &bad, &f.target, 1, &ticket ) );
		CHECK( commandCount == baseCommands
			&& memcmp( &ticket, &ticketBefore, sizeof( ticket ) ) == 0 );
	}
	CHECK( VK_TemporalResolveRecord( &owner, H( 0xb00 ), &f.authority,
		&f.products, &f.current, &f.target, 1, &ticket ) );
	CHECK( ticket.recorded && !ticket.submitted
		&& ticket.content.producer ==
			VK_TEMPORAL_RESOLVED_HDR_PRODUCER_TEMPORAL_RESOLVE
		&& ticket.ownerAllocationGeneration == 1 );
	CHECK( VK_TemporalResolveTicketValidateRecordedExact( &ticket )
		&& VK_TemporalResolveTicketEqualExact( &ticket, &ticket ) );
	{
		vkTemporalResolveTicket_t bad;
		#define REJECT_CANONICAL(mutation) do { \
			bad = ticket; mutation; \
			CHECK( !VK_TemporalResolveTicketValidateRecordedExact( &bad ) ); \
		} while ( 0 )
		REJECT_CANONICAL( bad.push.extent[0]++ );
		REJECT_CANONICAL( bad.push.extent[1]++ );
		REJECT_CANONICAL( bad.push.currentEffectiveJitterUv[0] += 0.25f );
		REJECT_CANONICAL( bad.push.currentEffectiveJitterUv[1] += 0.25f );
		REJECT_CANONICAL( bad.push.previousEffectiveJitterUv[0] += 0.25f );
		REJECT_CANONICAL( bad.push.previousEffectiveJitterUv[1] += 0.25f );
		REJECT_CANONICAL( bad.push.zNear += 1.0f );
		REJECT_CANONICAL( bad.push.zFar += 1.0f );
		REJECT_CANONICAL( bad.push.depthThresholdAbsolute += 1.0f );
		REJECT_CANONICAL( bad.push.depthThresholdRelative += 1.0f );
		REJECT_CANONICAL( bad.push.historyWeight = 0.5f );
		REJECT_CANONICAL( bad.push.historyReadIndex = 1u );
		REJECT_CANONICAL( bad.submitted = qtrue );
		REJECT_CANONICAL( bad.content.submitted = qtrue );
		REJECT_CANONICAL( bad.content.producer =
			VK_TEMPORAL_RESOLVED_HDR_PRODUCER_COPY );
		REJECT_CANONICAL( bad.content.sourceSceneColor = H( 0xe00 ) );
		REJECT_CANONICAL( bad.content.target = H( 0xe01 ) );
		#undef REJECT_CANONICAL
		bad = ticket; bad.products.previousDepth = H( 0xe02 );
		CHECK( !VK_TemporalResolveTicketEqualExact( &ticket, &bad ) );
		bad = ticket; bad.products.velocityView = H( 0xe03 );
		CHECK( !VK_TemporalResolveTicketEqualExact( &ticket, &bad ) );
	}
	CHECK( commandCount == baseCommands + 13 );
	CHECK( capturedPush.extent[0] == 1280 && capturedPush.extent[1] == 720
		&& capturedPush.currentEffectiveJitterUv[0] ==
			f.authority.currentEffectiveJitterUv[0]
		&& capturedPush.previousEffectiveJitterUv[1] ==
			f.authority.previousEffectiveJitterUv[1]
		&& capturedPush.zNear == 4.0f && capturedPush.zFar == 8192.0f
		&& capturedPush.depthThresholdAbsolute == 0.125f
		&& capturedPush.depthThresholdRelative == 0.02f
		&& capturedPush.historyWeight == 0.875f
		&& capturedPush.historyReadIndex == 0 );
	CHECK( commands[baseCommands + 0].kind == 'T'
		&& commands[baseCommands + 0].object == f.key.currentColor
		&& commands[baseCommands + 0].a == RAL_PIPELINE_STAGE_COMPUTE_SHADER_BIT
		&& commands[baseCommands + 0].b == RAL_PIPELINE_STAGE_COMPUTE_SHADER_BIT
		&& commands[baseCommands + 0].c == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL );
	CHECK( commands[baseCommands + 1].kind == 'T'
		&& commands[baseCommands + 1].object == f.key.historyColor[0]
		&& commands[baseCommands + 1].a == RAL_PIPELINE_STAGE_COMPUTE_SHADER_BIT
		&& commands[baseCommands + 1].b == RAL_PIPELINE_STAGE_COMPUTE_SHADER_BIT
		&& commands[baseCommands + 1].c == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL );
	CHECK( commands[baseCommands + 2].kind == 'T'
		&& commands[baseCommands + 2].object == f.key.historyDepth[0]
		&& commands[baseCommands + 2].a == RAL_PIPELINE_STAGE_COMPUTE_SHADER_BIT
		&& commands[baseCommands + 2].b == RAL_PIPELINE_STAGE_COMPUTE_SHADER_BIT
		&& commands[baseCommands + 2].c == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL );
	CHECK( commands[baseCommands + 3].kind == 'T'
		&& commands[baseCommands + 3].object == f.key.velocity
		&& commands[baseCommands + 3].a == RAL_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
		&& commands[baseCommands + 3].b == RAL_PIPELINE_STAGE_COMPUTE_SHADER_BIT
		&& commands[baseCommands + 3].c == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL );
	CHECK( commands[baseCommands + 4].kind == 'T'
		&& commands[baseCommands + 4].object == f.key.validity
		&& commands[baseCommands + 4].a == RAL_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
		&& commands[baseCommands + 4].b == RAL_PIPELINE_STAGE_COMPUTE_SHADER_BIT
		&& commands[baseCommands + 4].c == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL );
	CHECK( commands[baseCommands + 5].object == f.key.resolvedTarget
		&& commands[baseCommands + 5].kind == 'T'
		&& commands[baseCommands + 5].a == RAL_PIPELINE_STAGE_ALL_COMMANDS_BIT
		&& commands[baseCommands + 5].b == RAL_PIPELINE_STAGE_COMPUTE_SHADER_BIT
		&& commands[baseCommands + 5].c == VK_IMAGE_LAYOUT_GENERAL );
	CHECK( commands[baseCommands + 6].kind == 'P'
		&& commands[baseCommands + 6].object == owner.pipeline );
	CHECK( commands[baseCommands + 7].kind == 'G'
		&& commands[baseCommands + 7].object == owner.groups[0]
		&& commands[baseCommands + 7].a == 0 );
	CHECK( commands[baseCommands + 8].kind == 'U'
		&& commands[baseCommands + 8].a == RAL_STAGE_COMPUTE
		&& commands[baseCommands + 8].b == sizeof( vkTemporalResolvePush_t ) );
	CHECK( commands[baseCommands + 9].kind == 'D'
		&& commands[baseCommands + 9].a == 160
		&& commands[baseCommands + 9].b == 90
		&& commands[baseCommands + 9].c == 1 );
	CHECK( commands[baseCommands + 10].kind == 'T'
		&& commands[baseCommands + 10].object == f.key.velocity
		&& commands[baseCommands + 10].a == RAL_PIPELINE_STAGE_COMPUTE_SHADER_BIT
		&& commands[baseCommands + 10].b ==
			RAL_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
		&& commands[baseCommands + 10].c ==
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL );
	CHECK( commands[baseCommands + 11].kind == 'T'
		&& commands[baseCommands + 11].object == f.key.validity
		&& commands[baseCommands + 11].a == RAL_PIPELINE_STAGE_COMPUTE_SHADER_BIT
		&& commands[baseCommands + 11].b ==
			RAL_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
		&& commands[baseCommands + 11].c ==
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL );
	CHECK( commands[baseCommands + 12].kind == 'T'
		&& commands[baseCommands + 12].object == f.key.resolvedTarget
		&& commands[baseCommands + 12].a == RAL_PIPELINE_STAGE_COMPUTE_SHADER_BIT
		&& commands[baseCommands + 12].b == ( RAL_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
			| RAL_PIPELINE_STAGE_COMPUTE_SHADER_BIT )
		&& commands[baseCommands + 12].c ==
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL );

	memset( &submitted, 0x5a, sizeof( submitted ) ); ticketBefore = submitted;
	CHECK( !VK_TemporalResolveResolveSubmit( &ticket, &receipt, &f.activation,
		&ticket.committedWriteExpected, &f.target, qfalse, &submitted ) );
	CHECK( memcmp( &submitted, &ticketBefore, sizeof( submitted ) ) == 0 );
	CHECK( VK_TemporalResolveResolveSubmit( &ticket, &receipt, &f.activation,
		&ticket.committedWriteExpected, &f.target, qtrue, &submitted ) );
	CHECK( submitted.submitted && submitted.content.submitted
		&& submitted.content.producer ==
			VK_TEMPORAL_RESOLVED_HDR_PRODUCER_TEMPORAL_RESOLVE );
	{
		vkTemporalResolveOwnerReceipt_t bad = receipt; bad.allocationGeneration++;
		CHECK( !VK_TemporalResolveResolveSubmit( &ticket, &bad, &f.activation,
			&ticket.committedWriteExpected, &f.target, qtrue, &submitted ) );
	}
	{
		vkTemporalMainActivationReceipt_t bad = f.activation;
		bad.auxiliaryCleared = qfalse;
		CHECK( !VK_TemporalResolveResolveSubmit( &ticket, &receipt, &bad,
			&ticket.committedWriteExpected, &f.target, qtrue, &submitted ) );
	}
	{
		temporalHistoryCommittedReceipt_t bad = ticket.committedWriteExpected;
		bad.color = H( 0xc00 );
		CHECK( !VK_TemporalResolveResolveSubmit( &ticket, &receipt, &f.activation,
			&bad, &f.target, qtrue, &submitted ) );
	}
	{
		vkTemporalResolveTicket_t badTicket = ticket;
		badTicket.committedWriteExpected.valid = qfalse;
		CHECK( !VK_TemporalResolveResolveSubmit( &badTicket, &receipt,
			&f.activation, &ticket.committedWriteExpected, &f.target,
			qtrue, &submitted ) );
		badTicket = ticket;
		badTicket.committedWriteExpected.frameId++;
		CHECK( !VK_TemporalResolveResolveSubmit( &badTicket, &receipt,
			&f.activation, &badTicket.committedWriteExpected, &f.target,
			qtrue, &submitted ) );
		badTicket = ticket;
		badTicket.committedWriteExpected.worldIndex++;
		CHECK( !VK_TemporalResolveResolveSubmit( &badTicket, &receipt,
			&f.activation, &badTicket.committedWriteExpected, &f.target,
			qtrue, &submitted ) );
		badTicket = ticket;
		badTicket.committedWriteExpected.planGeneration++;
		CHECK( !VK_TemporalResolveResolveSubmit( &badTicket, &receipt,
			&f.activation, &badTicket.committedWriteExpected, &f.target,
			qtrue, &submitted ) );
		badTicket = ticket;
		badTicket.committedWriteExpected.allocationGeneration++;
		CHECK( !VK_TemporalResolveResolveSubmit( &badTicket, &receipt,
			&f.activation, &badTicket.committedWriteExpected, &f.target,
			qtrue, &submitted ) );
		badTicket = ticket;
		badTicket.committedWriteExpected.historyIndex =
			1u - badTicket.committedWriteExpected.historyIndex;
		CHECK( !VK_TemporalResolveResolveSubmit( &badTicket, &receipt,
			&f.activation, &badTicket.committedWriteExpected, &f.target,
			qtrue, &submitted ) );
		badTicket = ticket;
		badTicket.committedWriteExpected.width++;
		CHECK( !VK_TemporalResolveResolveSubmit( &badTicket, &receipt,
			&f.activation, &badTicket.committedWriteExpected, &f.target,
			qtrue, &submitted ) );
		badTicket = ticket;
		badTicket.committedWriteExpected.height++;
		CHECK( !VK_TemporalResolveResolveSubmit( &badTicket, &receipt,
			&f.activation, &badTicket.committedWriteExpected, &f.target,
			qtrue, &submitted ) );
		badTicket = ticket;
		badTicket.committedWriteExpected.topologyEpoch++;
		CHECK( !VK_TemporalResolveResolveSubmit( &badTicket, &receipt,
			&f.activation, &badTicket.committedWriteExpected, &f.target,
			qtrue, &submitted ) );
	}
	{
		vkTemporalResolvedHdrReceipt_t bad = f.target; bad.targetView = H( 0xc10 );
		CHECK( !VK_TemporalResolveResolveSubmit( &ticket, &receipt, &f.activation,
			&ticket.committedWriteExpected, &bad, qtrue, &submitted ) );
	}

	borrowed[0] = f.key.backend; borrowed[1] = f.key.currentColor;
	borrowed[2] = f.key.currentDepth;
	borrowed[3] = f.key.historyColor[0]; borrowed[4] = f.key.historyColorView[0];
	borrowed[5] = f.key.historyDepth[0]; borrowed[6] = f.key.historyDepthView[0];
	borrowed[7] = f.key.historyColor[1]; borrowed[8] = f.key.historyColorView[1];
	borrowed[9] = f.key.historyDepth[1]; borrowed[10] = f.key.historyDepthView[1];
	borrowed[11] = f.key.velocity; borrowed[12] = f.key.velocityView;
	borrowed[13] = f.key.validity; borrowed[14] = f.key.validityView;
	borrowed[15] = f.key.resolvedTarget; borrowed[16] = f.key.resolvedTargetView;
	for ( i = 0; i < 17; ++i ) {
		for ( j = 0; j < 7; ++j ) {
			vkTemporalResolveKey_t changed = f.key;
			changed.topologyEpoch = 4 + i * 7 + j;
			before = owner; baseDestroy = destroyCount;
			failStep = createCount + j + 1; aliasAtStep = (void *)borrowed[i];
			CHECK( !VK_TemporalResolveEnsureAfterFence(
				&owner, &changed, qtrue ) );
			CHECK( memcmp( &owner, &before, sizeof( owner ) ) == 0
				&& !DestroyedSince( baseDestroy, borrowed[i] ) );
			failStep = 0; aliasAtStep = NULL;
		}
	}
	live[0] = owner.currentColorView; live[1] = owner.currentDepthView;
	live[2] = owner.nearestSampler; live[3] = owner.layout;
	live[4] = owner.groups[0]; live[5] = owner.groups[1];
	live[6] = owner.pipeline;
	for ( i = 0; i < 7; ++i ) {
		for ( j = 0; j < 7; ++j ) {
			vkTemporalResolveKey_t changed = f.key;
			changed.topologyEpoch = 1000 + i * 7 + j;
			before = owner; baseDestroy = destroyCount;
			failStep = createCount + j + 1; aliasAtStep = (void *)live[i];
			CHECK( !VK_TemporalResolveEnsureAfterFence(
				&owner, &changed, qtrue ) );
			CHECK( memcmp( &owner, &before, sizeof( owner ) ) == 0
				&& !DestroyedSince( baseDestroy, live[i] ) );
			failStep = 0; aliasAtStep = NULL;
		}
	}
	{
		vkTemporalResolveKey_t replacement = f.key;
		const void *oldBorrowed = owner.key.historyColorView[0];
		replacement.topologyEpoch = 101;
		replacement.historyColorView[0] = H( 0xd00 );
		before = owner; baseDestroy = destroyCount;
		failStep = createCount + 1; aliasAtStep = (void *)oldBorrowed;
		CHECK( !VK_TemporalResolveEnsureAfterFence( &owner, &replacement, qtrue ) );
		CHECK( memcmp( &owner, &before, sizeof( owner ) ) == 0
			&& !DestroyedSince( baseDestroy, oldBorrowed ) );
		failStep = 0; aliasAtStep = NULL;
		CHECK( VK_TemporalResolveEnsureAfterFence( &owner, &replacement, qtrue ) );
	}
	owner.allocationGeneration = UINT32_MAX;
	{
		vkTemporalResolveKey_t changed = f.key; changed.topologyEpoch = 100;
		CHECK( !VK_TemporalResolveEnsureAfterFence( &owner, &changed, qtrue ) );
	}
	CHECK( VK_TemporalResolveReleaseAfterIdle( &owner, qtrue )
		&& !VK_TemporalResolveHasLive( &owner )
		&& owner.allocationGeneration == UINT32_MAX );
	CHECK( VK_TemporalResolveReleaseAfterIdle( &owner, qtrue ) );
	puts( "vk temporal resolve contract: PASS" );
	return 0;
}
