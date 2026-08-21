// SPDX-License-Identifier: GPL-3.0-or-later

#include "vk_temporal_resolve_readback.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(x) do { if ( !(x) ) { \
	fprintf( stderr, "CHECK failed at %s:%d: %s\n", __FILE__, __LINE__, #x ); \
	return 1; \
} } while ( 0 )

struct ralBackend_s { int id; };
struct ralCommandBuffer_s { int id; };
struct ralTexture_s { int id; };
struct ralTextureView_s { int id; };
struct ralBuffer_s { void *memory; uint64_t size; int id; };

static int creates, destroys, maps, unmaps, transitions, copies, barriers, depthCopies;
static int nextId = 100;
static ralBuffer_t *aliasCreate;
static void *aliasMap;
static uint32_t eventCount, eventKind[64], eventSrc[64], eventDst[64], eventLayout[64];
static const void *eventObject[64], *eventBuffer[64];
static uint64_t eventOffset[64];
static int32_t eventX[64], eventY[64];
static uint32_t eventWidth[64], eventHeight[64];

ralBuffer_t *Ral_CreateBuffer( ralBackend_t *backend,
		const ralBufferCreateInfo_t *ci ) {
	ralBuffer_t *b;
	(void)backend;
	if ( aliasCreate ) return aliasCreate;
	if ( !ci || !ci->size
			|| ci->usage != ( RAL_BUFFER_TRANSFER_DST | RAL_BUFFER_MAP_READ )
			|| ci->memory != RAL_MEMORY_HOST_COHERENT ) return NULL;
	b = (ralBuffer_t *)calloc( 1, sizeof( *b ) );
	if ( !b ) return NULL;
	b->memory = calloc( 1, (size_t)ci->size );
	if ( !b->memory ) { free( b ); return NULL; }
	b->size = ci->size; b->id = nextId++; creates++; return b;
}
void Ral_DestroyBuffer( ralBuffer_t *b ) {
	if ( !b ) return;
	destroys++; free( b->memory ); free( b );
}
ralResult_t Ral_BufferMapBegin( ralBuffer_t *b,
		const ralBufferMapRequest_t *request, ralBufferMapTicket_t *ticket ) {
	if ( !b || !request || !ticket || request->mode != RAL_MAP_READ
			|| request->offset || request->size != b->size ) return ralErrorInvalidArgument;
	memset( ticket, 0, sizeof( *ticket ) ); maps++;
	ticket->bufferIdentity = b; ticket->generation = (uint64_t)maps;
	ticket->request = *request; ticket->status = RAL_BUFFER_MAP_READY;
	ticket->mappedRange = aliasMap ? aliasMap : b->memory;
	return ralSuccess;
}
ralResult_t Ral_BufferMapUnmap( ralBuffer_t *b,
		const ralBufferMapTicket_t *ticket ) {
	if ( !b || !ticket || ticket->bufferIdentity != b ) return ralErrorInvalidArgument;
	unmaps++; return ralSuccess;
}
ralResult_t Ral_CmdTransitionResources( ralCommandBuffer_t *cb,
		const ralResourceTransitionBatch_t *batch ) {
	return cb && batch && batch->bufferTransitionCount == 1u
		? ralSuccess : ralErrorInvalidArgument;
}
void Ral_CmdTransitionTexture( ralCommandBuffer_t *cb, ralTexture_t *texture,
		ralPipelineStageFlags_t src, ralPipelineStageFlags_t dst, uint32_t layout ) {
	(void)cb; transitions++;
	eventKind[eventCount] = 1; eventObject[eventCount] = texture;
	eventSrc[eventCount] = src; eventDst[eventCount] = dst;
	eventLayout[eventCount++] = layout;
}
void Ral_CmdCopyTextureToBuffer( ralCommandBuffer_t *cb, ralTexture_t *src,
		ralBuffer_t *dst, const ralBufferTextureCopy_t *copy ) {
	(void)cb;
	if ( copy && copy->imageRect.width && copy->imageRect.height ) {
		copies++; eventKind[eventCount] = 2; eventObject[eventCount] = src;
		eventBuffer[eventCount] = dst; eventOffset[eventCount] = copy->bufferOffset;
		eventX[eventCount] = copy->imageRect.x; eventY[eventCount] = copy->imageRect.y;
		eventWidth[eventCount] = copy->imageRect.width;
		eventHeight[eventCount] = copy->imageRect.height; eventCount++;
	}
}
void Ral_CmdPipelineBarrierFull( ralCommandBuffer_t *cb,
		const ralPipelineBarrierInfo_t *info ) {
	(void)cb;
	if ( info && info->memoryBarrierCount == 1
			&& info->memoryBarriers[0].srcAccessMask == VK_ACCESS_TRANSFER_WRITE_BIT
			&& info->memoryBarriers[0].dstAccessMask == VK_ACCESS_HOST_READ_BIT )
		{
			barriers++; eventKind[eventCount] = 3;
			eventSrc[eventCount] = info->srcStageMask;
			eventDst[eventCount] = info->dstStageMask; eventCount++;
		}
}

// The real fieldwise helper is mutation-tested in vk_temporal_resolve_test.
// This focused owner test supplies the same exact fields it exercises.
qboolean VK_TemporalResolveTicketEqualExact(
		const vkTemporalResolveTicket_t *a, const vkTemporalResolveTicket_t *b ) {
	return a && b && a->authority.batchToken == b->authority.batchToken
		&& a->authority.frameId == b->authority.frameId
		&& a->authority.commandSlot == b->authority.commandSlot
		&& a->content.contentSerial == b->content.contentSerial
		&& a->content.submitted == b->content.submitted
		&& a->products.backend == b->products.backend
		&& a->products.currentColor == b->products.currentColor
		&& a->products.currentDepth == b->products.currentDepth
		&& a->products.previousColor == b->products.previousColor
		&& a->products.previousColorView == b->products.previousColorView
		&& a->products.previousDepth == b->products.previousDepth
		&& a->products.previousDepthView == b->products.previousDepthView
		&& a->products.velocity == b->products.velocity
		&& a->products.velocityView == b->products.velocityView
		&& a->products.validity == b->products.validity
		&& a->products.validityView == b->products.validityView
		&& a->products.resolvedTarget == b->products.resolvedTarget
		&& a->products.resolvedTargetView == b->products.resolvedTargetView
		&& a->committedWriteExpected.frameId == b->committedWriteExpected.frameId
		&& a->committedWriteExpected.source.storeOwnerAllocationGeneration ==
			b->committedWriteExpected.source.storeOwnerAllocationGeneration
		&& a->push.extent[0] == b->push.extent[0]
		&& a->push.extent[1] == b->push.extent[1]
		&& a->push.currentEffectiveJitterUv[0] == b->push.currentEffectiveJitterUv[0]
		&& a->push.previousEffectiveJitterUv[0] == b->push.previousEffectiveJitterUv[0]
		&& a->push.zNear == b->push.zNear && a->push.zFar == b->push.zFar
		&& a->push.depthThresholdAbsolute == b->push.depthThresholdAbsolute
		&& a->push.depthThresholdRelative == b->push.depthThresholdRelative
		&& a->push.historyWeight == b->push.historyWeight
		&& a->push.historyReadIndex == b->push.historyReadIndex
		&& a->ownerAllocationGeneration == b->ownerAllocationGeneration
		&& a->recorded == b->recorded && a->submitted == b->submitted
		? qtrue : qfalse;
}

qboolean VK_TemporalResolveTicketValidateRecordedExact(
		const vkTemporalResolveTicket_t *t ) {
	return t && t->recorded && !t->submitted && !t->content.submitted
		&& t->authority.ready && t->content.valid
		&& t->content.producer ==
			VK_TEMPORAL_RESOLVED_HDR_PRODUCER_TEMPORAL_RESOLVE
		&& t->push.extent[0] == t->authority.width
		&& t->push.extent[1] == t->authority.height
		&& t->push.currentEffectiveJitterUv[0] ==
			t->authority.currentEffectiveJitterUv[0]
		&& t->push.currentEffectiveJitterUv[1] ==
			t->authority.currentEffectiveJitterUv[1]
		&& t->push.previousEffectiveJitterUv[0] ==
			t->authority.previousEffectiveJitterUv[0]
		&& t->push.previousEffectiveJitterUv[1] ==
			t->authority.previousEffectiveJitterUv[1]
		&& t->push.zNear == t->authority.zNear
		&& t->push.zFar == t->authority.zFar
		&& t->push.depthThresholdAbsolute == 0.125f
		&& t->push.depthThresholdRelative == 0.02f
		&& t->push.historyWeight == 0.875f
		&& t->push.historyReadIndex == t->authority.historyReadIndex
		&& t->content.commandSlot == t->authority.commandSlot
		&& t->content.frameCount == t->authority.frameCount
		&& t->content.backend == t->products.backend
		&& t->content.sourceSceneColor == t->products.currentColor
		&& t->content.target == t->products.resolvedTarget
		&& t->content.targetView == t->products.resolvedTargetView
		? qtrue : qfalse;
}

static qboolean DepthCopy( ralCommandBuffer_t *cb, ralTexture_t *depth,
		ralBuffer_t *destination, uint64_t offset, uint32_t x, uint32_t y,
		uint32_t width, uint32_t height,
		vkTemporalResolveReadbackDepthEncoding_t depthEncoding, void *user ) {
	(void)cb; (void)user;
	if ( !width || !height || depthEncoding != VK_TEMPORAL_RESOLVE_DEPTH_D32 )
		return qfalse;
	eventKind[eventCount] = 4; eventObject[eventCount] = depth;
	eventSrc[eventCount] = RAL_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
	eventDst[eventCount] = RAL_PIPELINE_STAGE_TRANSFER_BIT;
	eventLayout[eventCount++] = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	eventKind[eventCount] = 5; eventObject[eventCount] = depth;
	eventBuffer[eventCount] = destination; eventOffset[eventCount] = offset;
	eventX[eventCount] = (int32_t)x; eventY[eventCount] = (int32_t)y;
	eventWidth[eventCount] = width; eventHeight[eventCount] = height; eventCount++;
	eventKind[eventCount] = 4; eventObject[eventCount] = depth;
	eventSrc[eventCount] = RAL_PIPELINE_STAGE_TRANSFER_BIT;
	eventDst[eventCount] = RAL_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
	eventLayout[eventCount++] = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	depthCopies++; return qtrue;
}

static vkTemporalResolveTicket_t RecordedTicket( void ) {
	vkTemporalResolveTicket_t t;
	memset( &t, 0, sizeof( t ) );
	t.authority.batchToken = 11; t.authority.frameId = 20;
	t.authority.previousFrameId = 19; t.authority.worldIndex = 0;
	t.authority.width = 8; t.authority.height = 8;
	t.authority.topologyEpoch = 3; t.authority.planGeneration = 4;
	t.authority.commandSlot = 0; t.authority.frameCount = 2;
	t.authority.historyAllocationGeneration = 5;
	t.authority.historyReadIndex = 0; t.authority.historyWriteIndex = 1;
	t.authority.motionTargetAllocationGeneration = 6;
	t.authority.resolvedTargetAllocationGeneration = 7;
	t.authority.zNear = 1.0f; t.authority.zFar = 10.0f;
	t.authority.ready = qtrue;
	t.content.batchToken = 11; t.content.frameId = 20;
	t.content.contentSerial = 21; t.content.commandSlot = 0;
	t.content.frameCount = 2; t.content.worldIndex = 0;
	t.content.width = 8; t.content.height = 8;
	t.content.producer = VK_TEMPORAL_RESOLVED_HDR_PRODUCER_TEMPORAL_RESOLVE;
	t.content.valid = qtrue;
	t.committedWriteExpected.frameId = 20;
	t.committedWriteExpected.valid = qtrue;
	t.push.extent[0] = 8; t.push.extent[1] = 8;
	t.push.zNear = 1.0f; t.push.zFar = 10.0f;
	t.push.depthThresholdAbsolute = 0.125f;
	t.push.depthThresholdRelative = 0.02f;
	t.push.historyWeight = 0.875f; t.push.historyReadIndex = 0;
	t.ownerAllocationGeneration = 8; t.recorded = qtrue;
	return t;
}

static void SetHalf4( unsigned char *base, uint32_t i,
		uint16_t r, uint16_t g, uint16_t b, uint16_t a ) {
	uint16_t values[4] = { r, g, b, a };
	memcpy( base + (uint64_t)i * 8u, values, sizeof( values ) );
}

int main( void ) {
	struct ralBackend_s backend = { 1 };
	struct ralCommandBuffer_s command = { 2 };
	struct ralTexture_s texture[8];
	struct ralTextureView_s view[6];
	vkTemporalResolveProductView_t products, badProducts;
	vkTemporalResolveReadbackOwner_t owner, ownerBefore;
	vkTemporalResolveTicket_t recorded = RecordedTicket(), submitted, badSubmitted;
	vkTemporalResolveTicket_t bound, badBound;
	vkTemporalResolveReadbackTicket_t readbackTicket;
	vkTemporalResolveReadbackContentReceipt_t content, sentinel;
	unsigned char *memory, *current, *depth, *previous, *previousDepth;
	unsigned char *velocity, *validity, *resolved;
	uint32_t i, center = 4u * 8u + 4u, neighbor = 3u * 8u + 4u;
	for ( i = 0; i < 8; ++i ) texture[i].id = (int)i + 1;
	for ( i = 0; i < 6; ++i ) view[i].id = (int)i + 20;
	memset( &products, 0, sizeof( products ) );
	products.backend = &backend; products.currentColor = &texture[0];
	products.currentDepth = &texture[1]; products.previousColor = &texture[2];
	products.previousColorView = &view[0]; products.previousDepth = &texture[3];
	products.previousDepthView = &view[1]; products.velocity = &texture[4];
	products.velocityView = &view[2]; products.validity = &texture[5];
	products.validityView = &view[3]; products.resolvedTarget = &texture[6];
	products.resolvedTargetView = &view[4];
	recorded.products = products;
	recorded.content.backend = &backend;
	recorded.content.sourceSceneColor = products.currentColor;
	recorded.content.target = products.resolvedTarget;
	recorded.content.targetView = products.resolvedTargetView;
	{
		uint16_t u16 = 0xffffu;
		uint32_t u24 = 0xa5ffffffu;
		float f32 = 0.25f;
		CHECK( VK_TemporalResolveReadbackDecodeDepth(
			&u16, VK_TEMPORAL_RESOLVE_DEPTH_D16 ) == 1.0f );
		CHECK( VK_TemporalResolveReadbackDecodeDepth(
			&u16, VK_TEMPORAL_RESOLVE_DEPTH_D16_S8 ) == 1.0f );
		CHECK( VK_TemporalResolveReadbackDecodeDepth(
			&u24, VK_TEMPORAL_RESOLVE_DEPTH_X8_D24 ) == 1.0f );
		CHECK( VK_TemporalResolveReadbackDecodeDepth(
			&u24, VK_TEMPORAL_RESOLVE_DEPTH_D24_S8 ) == 1.0f );
		CHECK( VK_TemporalResolveReadbackDecodeDepth(
			&f32, VK_TEMPORAL_RESOLVE_DEPTH_D32 ) == 0.25f );
		CHECK( VK_TemporalResolveReadbackDecodeDepth(
			&f32, VK_TEMPORAL_RESOLVE_DEPTH_D32_S8 ) == 0.25f );
		CHECK( isnan( VK_TemporalResolveReadbackDecodeDepth( &u16,
			(vkTemporalResolveReadbackDepthEncoding_t)0 ) ) );
	}
	CHECK( VK_TemporalResolveReadbackFloatToHalfRne( 0.0f ) == 0x0000u );
	CHECK( VK_TemporalResolveReadbackFloatToHalfRne( -0.0f ) == 0x8000u );
	CHECK( VK_TemporalResolveReadbackFloatToHalfRne( ldexpf( 1.0f, -24 ) ) == 0x0001u );
	CHECK( VK_TemporalResolveReadbackFloatToHalfRne( -ldexpf( 1.0f, -24 ) ) == 0x8001u );
	CHECK( VK_TemporalResolveReadbackFloatToHalfRne(
		ldexpf( 1023.0f, -24 ) ) == 0x03ffu );
	CHECK( VK_TemporalResolveReadbackFloatToHalfRne(
		ldexpf( 1.0f, -14 ) ) == 0x0400u );
	CHECK( VK_TemporalResolveReadbackFloatToHalfRne( 1.0f ) == 0x3c00u );
	CHECK( VK_TemporalResolveReadbackFloatToHalfRne( 65504.0f ) == 0x7bffu );
	CHECK( VK_TemporalResolveReadbackHalfWithinOneStep( 0x0001u, 0.0f ) );
	CHECK( !VK_TemporalResolveReadbackHalfWithinOneStep( 0x0002u, 0.0f ) );
	CHECK( VK_TemporalResolveReadbackHalfWithinOneStep( 0x3bffu, 1.0f ) );
	CHECK( VK_TemporalResolveReadbackHalfWithinOneStep( 0x3c01u, 1.0f ) );
	CHECK( !VK_TemporalResolveReadbackHalfWithinOneStep( 0x3bfeu, 1.0f ) );
	CHECK( !VK_TemporalResolveReadbackHalfWithinOneStep( 0x3c02u, 1.0f ) );
	CHECK( VK_TemporalResolveReadbackHalfWithinOneStep( 0xbc01u, -1.0f ) );
	CHECK( VK_TemporalResolveReadbackHalfWithinOneStep( 0xbbffu, -1.0f ) );
	CHECK( !VK_TemporalResolveReadbackHalfWithinOneStep( 0xbc02u, -1.0f ) );
	CHECK( !VK_TemporalResolveReadbackHalfWithinOneStep( 0x7c00u, 1.0f ) );
	CHECK( !VK_TemporalResolveReadbackHalfWithinOneStep( 0x3c00u, INFINITY ) );
	{
		uint32_t pixel[2] = { 4u, 4u }, extent[2] = { 8u, 8u };
		float velocity[2] = { 0.25f, -0.5f };
		float previousJitter[2] = { 0.125f, -0.25f };
		float currentJitter[2] = { 0.0f, 0.0f }, out[2];
		VK_TemporalResolveReadbackPreviousTexel( pixel, extent, velocity,
			previousJitter, currentJitter, out );
		CHECK( out[0] == 3.0f && out[1] == 6.0f );
	}

	VK_TemporalResolveReadbackInit( &owner );
	CHECK( !VK_TemporalResolveReadbackHasLive( &owner ) );
	CHECK( VK_TemporalResolveReadbackArm( &owner, 2 ) );
	ownerBefore = owner;
	aliasCreate = (ralBuffer_t *)&texture[0];
	CHECK( !VK_TemporalResolveReadbackPrepareAfterFence( &owner, &backend,
		2, 0, 8, 8, VK_TEMPORAL_RESOLVE_DEPTH_D32, &products ) );
	CHECK( memcmp( &owner, &ownerBefore, sizeof( owner ) ) == 0 && destroys == 0 );
	aliasCreate = NULL;
	CHECK( VK_TemporalResolveReadbackPrepareAfterFence( &owner, &backend,
		2, 0, 8, 8, VK_TEMPORAL_RESOLVE_DEPTH_D32, &products ) );
	CHECK( creates == 1 && maps == 0 && owner.slots[0].bytes > 0
		&& owner.slots[0].state == VK_TEMPORAL_RESOLVE_READBACK_READY );
	ownerBefore = owner; aliasCreate = owner.slots[0].buffer;
	CHECK( !VK_TemporalResolveReadbackPrepareAfterFence( &owner, &backend,
		2, 1, 8, 8, VK_TEMPORAL_RESOLVE_DEPTH_D32, &products ) );
	CHECK( memcmp( &owner, &ownerBefore, sizeof( owner ) ) == 0 );
	aliasCreate = NULL;
	CHECK( VK_TemporalResolveReadbackPrepareAfterFence( &owner, &backend,
		2, 1, 8, 8, VK_TEMPORAL_RESOLVE_DEPTH_D32, &products ) );
	badProducts = products; badProducts.validity = badProducts.velocity;
	CHECK( !VK_TemporalResolveReadbackRecord( &owner, &command, 0,
		&recorded, &badProducts, DepthCopy, NULL ) );
	CHECK( transitions == 0 && copies == 0 && depthCopies == 0 );
	badProducts = products; badProducts.currentDepth = &texture[7];
	CHECK( !VK_TemporalResolveReadbackRecord( &owner, &command, 0,
		&recorded, &badProducts, DepthCopy, NULL ) );
	CHECK( transitions == 0 && copies == 0 && depthCopies == 0 );
	CHECK( VK_TemporalResolveReadbackRecord( &owner, &command, 0,
		&recorded, &products, DepthCopy, NULL ) );
	CHECK( depthCopies == 1 && transitions == 12 && copies == 6 && barriers == 1 );
	CHECK( owner.slots[0].ticket.captureWidth == 8
		&& owner.slots[0].ticket.coreWidth == 8 );
	CHECK( eventCount == 22 && eventKind[0] == 4
		&& eventObject[0] == products.currentDepth
		&& eventSrc[0] == RAL_PIPELINE_STAGE_COMPUTE_SHADER_BIT
		&& eventDst[0] == RAL_PIPELINE_STAGE_TRANSFER_BIT
		&& eventLayout[0] == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
		&& eventKind[1] == 5 && eventObject[1] == products.currentDepth
		&& eventBuffer[1] == owner.slots[0].buffer
		&& eventOffset[1] == owner.slots[0].ticket.currentDepthOffset
		&& eventX[1] == 0 && eventY[1] == 0
		&& eventWidth[1] == 8 && eventHeight[1] == 8
		&& eventKind[2] == 4
		&& eventLayout[2] == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL );
	{
		const ralTexture_t *expectedTexture[6] = {
			products.currentColor, products.previousColor, products.previousDepth,
			products.velocity, products.validity, products.resolvedTarget
		};
		const uint64_t expectedOffset[6] = {
			owner.slots[0].ticket.currentColorOffset,
			owner.slots[0].ticket.previousColorOffset,
			owner.slots[0].ticket.previousDepthOffset,
			owner.slots[0].ticket.velocityOffset,
			owner.slots[0].ticket.validityOffset,
			owner.slots[0].ticket.resolvedOffset
		};
		for ( i = 0; i < 6; ++i ) {
			const uint32_t expectedRestoreDst[6] = {
				RAL_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
				RAL_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
				RAL_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
				RAL_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
				RAL_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
				RAL_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
					| RAL_PIPELINE_STAGE_COMPUTE_SHADER_BIT
			};
			CHECK( eventKind[3 + i] == 1
				&& eventObject[3 + i] == expectedTexture[i]
				&& eventDst[3 + i] == RAL_PIPELINE_STAGE_TRANSFER_BIT
				&& eventLayout[3 + i] == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL );
			CHECK( eventKind[9 + i] == 2
				&& eventObject[9 + i] == expectedTexture[i]
				&& eventBuffer[9 + i] == owner.slots[0].buffer
				&& eventOffset[9 + i] == expectedOffset[i]
				&& eventX[9 + i] == 0 && eventY[9 + i] == 0
				&& eventWidth[9 + i] == 8 && eventHeight[9 + i] == 8 );
			CHECK( eventKind[16 + i] == 1
				&& eventObject[16 + i] == expectedTexture[i]
				&& eventSrc[16 + i] == RAL_PIPELINE_STAGE_TRANSFER_BIT
				&& eventDst[16 + i] == expectedRestoreDst[i] );
		}
		CHECK( eventKind[15] == 3
			&& eventSrc[15] == RAL_PIPELINE_STAGE_TRANSFER_BIT
			&& eventDst[15] == RAL_PIPELINE_STAGE_HOST_BIT );
		CHECK( eventSrc[3] == RAL_PIPELINE_STAGE_COMPUTE_SHADER_BIT
			&& eventSrc[4] == RAL_PIPELINE_STAGE_COMPUTE_SHADER_BIT
			&& eventSrc[5] == RAL_PIPELINE_STAGE_COMPUTE_SHADER_BIT
			&& eventSrc[6] == RAL_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
			&& eventSrc[7] == RAL_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
			&& eventSrc[8] == ( RAL_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
				| RAL_PIPELINE_STAGE_COMPUTE_SHADER_BIT )
			&& eventLayout[16] == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
			&& eventLayout[17] == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
			&& eventLayout[18] == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
			&& eventLayout[19] == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
			&& eventLayout[20] == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
			&& eventLayout[21] == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL );
	}
	bound = recorded;
	bound.committedWriteExpected.source.storeOwnerAllocationGeneration = 42u;
	ownerBefore = owner; badBound = bound;
	badBound.committedWriteExpected.source.storeOwnerAllocationGeneration = 0u;
	CHECK( !VK_TemporalResolveReadbackBindStoreExpected(
		&owner, 0, &badBound ) );
	CHECK( memcmp( &owner, &ownerBefore, sizeof( owner ) ) == 0 );
	badBound = bound; badBound.submitted = qtrue;
	CHECK( !VK_TemporalResolveReadbackBindStoreExpected(
		&owner, 0, &badBound ) );
	CHECK( memcmp( &owner, &ownerBefore, sizeof( owner ) ) == 0 );
	badBound = bound; badBound.content.contentSerial++;
	CHECK( !VK_TemporalResolveReadbackBindStoreExpected(
		&owner, 0, &badBound ) );
	CHECK( memcmp( &owner, &ownerBefore, sizeof( owner ) ) == 0 );
	CHECK( !VK_TemporalResolveReadbackBindStoreExpected(
		&owner, 1, &bound ) );
	CHECK( memcmp( &owner, &ownerBefore, sizeof( owner ) ) == 0 );
	CHECK( VK_TemporalResolveReadbackBindStoreExpected(
		&owner, 0, &bound ) );
	CHECK( owner.slots[0].ticket.resolve.committedWriteExpected.source.
		storeOwnerAllocationGeneration == 42u );
	ownerBefore = owner;
	CHECK( !VK_TemporalResolveReadbackBindStoreExpected(
		&owner, 0, &bound ) );
	CHECK( memcmp( &owner, &ownerBefore, sizeof( owner ) ) == 0 );
	badBound = bound;
	badBound.committedWriteExpected.source.storeOwnerAllocationGeneration = 43u;
	CHECK( !VK_TemporalResolveReadbackBindStoreExpected(
		&owner, 0, &badBound ) );
	CHECK( memcmp( &owner, &ownerBefore, sizeof( owner ) ) == 0 );
	owner.slots[0].ticket.commandSlot = 1;
	CHECK( !VK_TemporalResolveReadbackResolveSubmit(
		&owner, 0, qfalse, NULL, &readbackTicket ) );
	CHECK( owner.slots[0].state == VK_TEMPORAL_RESOLVE_READBACK_READY );
	eventCount = 0;
	CHECK( VK_TemporalResolveReadbackRecord( &owner, &command, 0,
		&recorded, &products, DepthCopy, NULL ) );
	CHECK( VK_TemporalResolveReadbackResolveSubmit(
		&owner, 0, qfalse, NULL, NULL ) );
	CHECK( owner.slots[0].state == VK_TEMPORAL_RESOLVE_READBACK_READY );
	eventCount = 0;
	CHECK( VK_TemporalResolveReadbackRecord( &owner, &command, 0,
		&recorded, &products, DepthCopy, NULL ) );
	owner.slots[0].ticket.bufferAllocationGeneration++;
	CHECK( !VK_TemporalResolveReadbackResolveSubmit(
		&owner, 0, qtrue, &recorded, &readbackTicket ) );
	CHECK( owner.slots[0].state == VK_TEMPORAL_RESOLVE_READBACK_READY );
	eventCount = 0;
	CHECK( VK_TemporalResolveReadbackRecord( &owner, &command, 0,
		&recorded, &products, DepthCopy, NULL ) );
	owner.slots[0].ticket.resolve.authority.frameCount = 1;
	CHECK( !VK_TemporalResolveReadbackResolveSubmit(
		&owner, 0, qtrue, &recorded, &readbackTicket ) );
	CHECK( owner.slots[0].state == VK_TEMPORAL_RESOLVE_READBACK_READY );
	badSubmitted = recorded; badSubmitted.submitted = qtrue;
	CHECK( !VK_TemporalResolveReadbackResolveSubmit(
		&owner, 0, qtrue, &badSubmitted, &readbackTicket ) );
	CHECK( owner.slots[0].state == VK_TEMPORAL_RESOLVE_READBACK_READY );
	eventCount = 0;
	CHECK( VK_TemporalResolveReadbackRecord( &owner, &command, 0,
		&recorded, &products, DepthCopy, NULL ) );
	badSubmitted = recorded; badSubmitted.content.submitted = qtrue;
	CHECK( !VK_TemporalResolveReadbackResolveSubmit(
		&owner, 0, qtrue, &badSubmitted, &readbackTicket ) );
	CHECK( owner.slots[0].state == VK_TEMPORAL_RESOLVE_READBACK_READY );
	eventCount = 0;
	CHECK( VK_TemporalResolveReadbackRecord( &owner, &command, 0,
		&recorded, &products, DepthCopy, NULL ) );
	badSubmitted = recorded; badSubmitted.content.submitted = qtrue;
	badSubmitted.submitted = qtrue;
	badSubmitted.push.historyWeight = 0.5f;
	memset( &sentinel, 0xa5, sizeof( sentinel ) ); content = sentinel;
	CHECK( !VK_TemporalResolveReadbackCompleteAfterFence(
		&owner, 0, qfalse, &content ) );
	CHECK( memcmp( &content, &sentinel, sizeof( content ) ) == 0 );
	CHECK( !VK_TemporalResolveReadbackResolveSubmit(
		&owner, 0, qtrue, &badSubmitted, &readbackTicket ) );
	CHECK( owner.slots[0].state == VK_TEMPORAL_RESOLVE_READBACK_READY );
	eventCount = 0;
	CHECK( VK_TemporalResolveReadbackRecord( &owner, &command, 0,
		&recorded, &products, DepthCopy, NULL ) );
	submitted = recorded; submitted.content.submitted = qtrue;
	submitted.submitted = qtrue;
	CHECK( VK_TemporalResolveReadbackResolveSubmit(
		&owner, 0, qtrue, &submitted, &readbackTicket ) );
	CHECK( readbackTicket.submitted && readbackTicket.captureSerial != 0u );
	memset( &sentinel, 0xa5, sizeof( sentinel ) ); content = sentinel;
	owner.slots[0].ticket.resolve.content.submitted = qfalse;
	CHECK( !VK_TemporalResolveReadbackCompleteAfterFence(
		&owner, 0, qtrue, &content ) );
	CHECK( memcmp( &content, &sentinel, sizeof( content ) ) == 0 );
	owner.slots[0].ticket.resolve.content.submitted = qtrue;
	owner.slots[0].ticket.resolve.push.historyWeight = 0.5f;
	CHECK( !VK_TemporalResolveReadbackCompleteAfterFence(
		&owner, 0, qtrue, &content ) );
	owner.slots[0].ticket.resolve.push.historyWeight = 0.875f;
	owner.slots[0].ticket.resolve.products.currentDepth = &texture[7];
	CHECK( !VK_TemporalResolveReadbackCompleteAfterFence(
		&owner, 0, qtrue, &content ) );
	owner.slots[0].ticket.resolve.products.currentDepth = products.currentDepth;

	memory = (unsigned char *)owner.slots[0].buffer->memory;
	current = memory + readbackTicket.currentColorOffset;
	depth = memory + readbackTicket.currentDepthOffset;
	previous = memory + readbackTicket.previousColorOffset;
	previousDepth = memory + readbackTicket.previousDepthOffset;
	velocity = memory + readbackTicket.velocityOffset;
	validity = memory + readbackTicket.validityOffset;
	resolved = memory + readbackTicket.resolvedOffset;
	memset( memory, 0, (size_t)readbackTicket.totalBytes );
	memset( depth, 0, 64u * sizeof( float ) );
	for ( i = 0; i < 64; ++i ) {
		float linear = 10.0f;
		SetHalf4( current, i, 0, 0, 0, 0x3c00 );
		SetHalf4( previous, i, 0x3c00, 0, 0, 0x3c00 );
		SetHalf4( resolved, i, 0, 0, 0, 0x3c00 );
		memcpy( previousDepth + (uint64_t)i * 4u, &linear, 4u );
	}
	SetHalf4( current, neighbor, 0x3c00, 0, 0, 0x3c00 );
	SetHalf4( resolved, neighbor, 0x3c00, 0, 0, 0x3c00 );
	validity[center] = 255u;
	{
		uint16_t motion[2] = { 0x1400, 0u };
		memcpy( velocity + (uint64_t)center * 4u, motion, sizeof( motion ) );
	}
	SetHalf4( resolved, center, 0x3b00, 0, 0, 0x3c00 );
	CHECK( VK_TemporalResolveReadbackCompleteAfterFence(
		&owner, 0, qtrue, &content ) );
	CHECK( content.fenceComplete && content.ready && content.planesPopulated
		&& content.accepted == 1 && content.acceptedMatches == 1
		&& content.acceptedInfluence == 1
		&& content.acceptedNonzeroVelocity == 1
		&& content.invalidFallbackExpected == 63
		&& content.invalidFallbackExact == 63
		&& content.validityFull == 1 && content.validityZero == 63
		&& content.validityOther == 0 && content.mismatches == 0 );
	CHECK( content.centerX == 4u && content.centerY == 4u
		&& content.centerCurrentColor[0] == 0u
		&& content.centerCurrentColor[3] == 0x3c00u
		&& content.centerDepthRaw == 0u
		&& content.centerVelocity[0] == 0x1400u
		&& content.centerVelocity[1] == 0u
		&& content.centerValidity == 255u
		&& content.centerResolvedColor[0] == 0x3b00u
		&& content.centerResolvedColor[3] == 0x3c00u );
	CHECK( VK_TemporalResolveReadbackGetLatest( &owner, &content ) );
	CHECK( VK_TemporalResolveReadbackPrepareAfterFence( &owner, &backend,
		2, 0, 8, 8, VK_TEMPORAL_RESOLVE_DEPTH_D32, &products ) );
	eventCount = 0;
	CHECK( VK_TemporalResolveReadbackRecord( &owner, &command, 0,
		&recorded, &products, DepthCopy, NULL ) );
	CHECK( VK_TemporalResolveReadbackResolveSubmit(
		&owner, 0, qtrue, &submitted, &readbackTicket ) );
	memory = (unsigned char *)owner.slots[0].buffer->memory;
	memset( memory, 0, (size_t)readbackTicket.totalBytes );
	CHECK( VK_TemporalResolveReadbackCompleteAfterFence(
		&owner, 0, qtrue, &content ) );
	CHECK( content.planesPopulated && !content.ready );

	CHECK( VK_TemporalResolveReadbackArm( &owner, 1 ) );
	CHECK( VK_TemporalResolveReadbackPrepareAfterFence( &owner, &backend,
		2, 0, 8, 8, VK_TEMPORAL_RESOLVE_DEPTH_D32, &products ) );
	eventCount = 0;
	CHECK( VK_TemporalResolveReadbackRecord( &owner, &command, 0,
		&recorded, &products, DepthCopy, NULL ) );
	CHECK( VK_TemporalResolveReadbackResolveSubmit(
		&owner, 0, qtrue, &submitted, &readbackTicket ) );
	memory = (unsigned char *)owner.slots[0].buffer->memory;
	memset( memory, 0, (size_t)readbackTicket.totalBytes );
	current = memory + readbackTicket.currentColorOffset;
	validity = memory + readbackTicket.validityOffset;
	resolved = memory + readbackTicket.resolvedOffset;
	SetHalf4( current, center, 0x7e00u, 0u, 0u, 0u );
	validity[center] = 255u;
	SetHalf4( resolved, center, 0u, 0u, 0u, 0u );
	CHECK( VK_TemporalResolveReadbackCompleteAfterFence(
		&owner, 0, qtrue, &content ) );
	CHECK( content.planesPopulated && content.zeroFallbackExpected == 1u
		&& content.zeroFallbackExact == 1u && content.nonfiniteInputs == 1u
		&& !content.ready );

	CHECK( VK_TemporalResolveReadbackArm( &owner, 1 ) );
	CHECK( VK_TemporalResolveReadbackPrepareAfterFence( &owner, &backend,
		2, 0, 8, 8, VK_TEMPORAL_RESOLVE_DEPTH_D32, &products ) );
	eventCount = 0;
	CHECK( VK_TemporalResolveReadbackRecord( &owner, &command, 0,
		&recorded, &products, DepthCopy, NULL ) );
	CHECK( VK_TemporalResolveReadbackResolveSubmit(
		&owner, 0, qtrue, &submitted, &readbackTicket ) );
	memory = (unsigned char *)owner.slots[0].buffer->memory;
	memset( memory, 0, (size_t)readbackTicket.totalBytes );
	current = memory + readbackTicket.currentColorOffset;
	velocity = memory + readbackTicket.velocityOffset;
	validity = memory + readbackTicket.validityOffset;
	resolved = memory + readbackTicket.resolvedOffset;
	SetHalf4( current, center, 0x3c00u, 0u, 0u, 0x3c00u );
	SetHalf4( resolved, center, 0x3c00u, 0u, 0u, 0x3c00u );
	validity[center] = 255u;
	{
		const uint16_t oobMotion[2] = { 0x6400u, 0u };
		memcpy( velocity + (uint64_t)center * 4u,
			oobMotion, sizeof( oobMotion ) );
	}
	CHECK( VK_TemporalResolveReadbackCompleteAfterFence(
		&owner, 0, qtrue, &content ) );
	CHECK( content.fallbackExpected == 64u
		&& content.invalidFallbackExpected == 63u
		&& content.fallbackExact == content.fallbackExpected
		&& !content.ready );
	for ( i = 0; i < 7u; ++i ) {
		uint64_t offsets[7], sizes[7];
		CHECK( VK_TemporalResolveReadbackArm( &owner, 1 ) );
		CHECK( VK_TemporalResolveReadbackPrepareAfterFence( &owner, &backend,
			2, 0, 8, 8, VK_TEMPORAL_RESOLVE_DEPTH_D32, &products ) );
		eventCount = 0;
		CHECK( VK_TemporalResolveReadbackRecord( &owner, &command, 0,
			&recorded, &products, DepthCopy, NULL ) );
		CHECK( VK_TemporalResolveReadbackResolveSubmit(
			&owner, 0, qtrue, &submitted, &readbackTicket ) );
		offsets[0] = readbackTicket.currentColorOffset;
		offsets[1] = readbackTicket.currentDepthOffset;
		offsets[2] = readbackTicket.previousColorOffset;
		offsets[3] = readbackTicket.previousDepthOffset;
		offsets[4] = readbackTicket.velocityOffset;
		offsets[5] = readbackTicket.validityOffset;
		offsets[6] = readbackTicket.resolvedOffset;
		sizes[0] = 64u * 8u; sizes[1] = 64u * 4u;
		sizes[2] = 64u * 8u; sizes[3] = 64u * 4u;
		sizes[4] = 64u * 4u; sizes[5] = 64u; sizes[6] = 64u * 8u;
		memory = (unsigned char *)owner.slots[0].buffer->memory;
		memset( memory, 0, (size_t)readbackTicket.totalBytes );
		memset( memory + offsets[i], 0x7f, (size_t)sizes[i] );
		CHECK( VK_TemporalResolveReadbackCompleteAfterFence(
			&owner, 0, qtrue, &content ) );
		CHECK( content.planesPopulated && !content.ready );
	}
	for ( i = 0; i < 9u; ++i ) {
		float priorLinear = 10.0f;
		uint32_t p;
		CHECK( VK_TemporalResolveReadbackArm( &owner, 1 ) );
		CHECK( VK_TemporalResolveReadbackPrepareAfterFence( &owner, &backend,
			2, 0, 8, 8, VK_TEMPORAL_RESOLVE_DEPTH_D32, &products ) );
		eventCount = 0;
		CHECK( VK_TemporalResolveReadbackRecord( &owner, &command, 0,
			&recorded, &products, DepthCopy, NULL ) );
		CHECK( VK_TemporalResolveReadbackResolveSubmit(
			&owner, 0, qtrue, &submitted, &readbackTicket ) );
		memory = (unsigned char *)owner.slots[0].buffer->memory;
		memset( memory, 0, (size_t)readbackTicket.totalBytes );
		current = memory + readbackTicket.currentColorOffset;
		depth = memory + readbackTicket.currentDepthOffset;
		previous = memory + readbackTicket.previousColorOffset;
		previousDepth = memory + readbackTicket.previousDepthOffset;
		velocity = memory + readbackTicket.velocityOffset;
		validity = memory + readbackTicket.validityOffset;
		resolved = memory + readbackTicket.resolvedOffset;
		for ( p = 0; p < 64u; ++p ) {
			SetHalf4( current, p, 0u, 0u, 0u, 0x3c00u );
			SetHalf4( previous, p, 0u, 0u, 0u, 0x3c00u );
			SetHalf4( resolved, p, 0u, 0u, 0u, 0x3c00u );
			memcpy( previousDepth + (uint64_t)p * 4u,
				&priorLinear, sizeof( priorLinear ) );
		}
		validity[center] = 255u;
		if ( i == 0u ) {
			const uint16_t nanVelocity[2] = { 0x7e00u, 0u };
			memcpy( velocity + (uint64_t)center * 4u,
				nanVelocity, sizeof( nanVelocity ) );
		} else if ( i == 1u ) {
			float nanDepth = NAN;
			memcpy( depth + (uint64_t)center * 4u,
				&nanDepth, sizeof( nanDepth ) );
		} else if ( i == 2u ) {
			float nanDepth = NAN;
			for ( p = 0; p < 64u; ++p ) memcpy(
				previousDepth + (uint64_t)p * 4u, &nanDepth, 4u );
		} else if ( i == 3u ) {
			memset( previousDepth, 0, 64u * 4u );
		} else if ( i == 4u ) {
			priorLinear = 1.0f;
			for ( p = 0; p < 64u; ++p ) memcpy(
				previousDepth + (uint64_t)p * 4u, &priorLinear, 4u );
		} else if ( i == 5u ) {
			SetHalf4( previous, center, 0x7e00u, 0u, 0u, 0x3c00u );
		} else if ( i == 6u ) {
			SetHalf4( current, neighbor, 0x7e00u, 0u, 0u, 0x3c00u );
			SetHalf4( resolved, neighbor, 0u, 0u, 0u, 0u );
		} else {
			priorLinear = 10.0f / 0.98f + ( i == 8u ? 0.001f : 0.0f );
			for ( p = 0; p < 64u; ++p ) memcpy(
				previousDepth + (uint64_t)p * 4u, &priorLinear, 4u );
		}
		CHECK( VK_TemporalResolveReadbackCompleteAfterFence(
			&owner, 0, qtrue, &content ) );
		CHECK( content.fallbackExact == content.fallbackExpected
			&& content.zeroFallbackExact == content.zeroFallbackExpected
			&& content.mismatches == 0u );
		if ( i == 7u ) CHECK( content.accepted == 1u
			&& content.acceptedMatches == 1u );
		else if ( i == 6u ) CHECK( content.fallbackExpected == 63u
			&& content.zeroFallbackExpected == 1u );
		else CHECK( content.fallbackExpected == 64u );
		if ( i == 0u || i == 1u || i == 2u || i == 5u || i == 6u )
			CHECK( content.nonfiniteInputs > 0u );
	}
	CHECK( VK_TemporalResolveReadbackReleaseAfterIdle( &owner, qtrue )
		&& !VK_TemporalResolveReadbackHasLive( &owner ) );
	puts( "vk temporal resolve readback contract: PASS" );
	return 0;
}
