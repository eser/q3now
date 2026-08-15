// SPDX-License-Identifier: GPL-3.0-or-later

#include "vk_temporal_motion_readback.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(x) do { if ( !(x) ) { \
	fprintf( stderr, "CHECK failed at %s:%d: %s\n", __FILE__, __LINE__, #x ); \
	return 1; \
} } while ( 0 )

qboolean VK_TemporalMainActivationReceiptExact(
		const vkTemporalMainActivationReceipt_t *a,
		const vkTemporalMainActivationReceipt_t *b ) {
	return a && b && a->ready == qtrue && b->ready == qtrue
		&& memcmp( a, b, sizeof( *a ) ) == 0 ? qtrue : qfalse;
}

struct ralBackend_s { int id; };
struct ralCommandBuffer_s { int id; };
struct ralTexture_s { int id; };
struct ralBuffer_s { void *memory; uint64_t size; int id; };

static int creates, destroys, maps, unmaps, transitions, copies, barriers;
static int nextId = 1;
static ralBuffer_t *aliasCreate;
static void *aliasMap;
static int eventCount, eventKind[32], eventTexture[32];
static uint64_t eventOffset[32];
static uint32_t eventLayout[32], eventWidth[32], eventHeight[32];
static int32_t eventX[32], eventY[32];
static int eventBuffer[32];

ralBuffer_t *Ral_CreateBuffer( ralBackend_t *backend,
		const ralBufferCreateInfo_t *ci ) {
	ralBuffer_t *b;
	(void)backend;
	if ( aliasCreate ) return aliasCreate;
	if ( !ci || !ci->size || ci->usage != RAL_BUFFER_TRANSFER_DST
			|| ci->memory != RAL_MEMORY_HOST_COHERENT ) return NULL;
	b = (ralBuffer_t *)calloc( 1, sizeof( *b ) );
	if ( !b ) return NULL;
	b->memory = calloc( 1, (size_t)ci->size );
	if ( !b->memory ) { free( b ); return NULL; }
	b->size = ci->size; b->id = nextId++; creates++;
	return b;
}
void Ral_DestroyBuffer( ralBuffer_t *b ) {
	if ( !b ) return;
	destroys++; free( b->memory ); free( b );
}
void *Ral_MapBuffer( ralBuffer_t *b ) {
	if ( !b ) return NULL;
	maps++;
	return aliasMap ? aliasMap : b->memory;
}
void Ral_UnmapBuffer( ralBuffer_t *b ) { if ( b ) unmaps++; }
void Ral_CmdTransitionTexture( ralCommandBuffer_t *cb, ralTexture_t *tex,
		ralPipelineStageFlags_t src, ralPipelineStageFlags_t dst,
		uint32_t layout ) {
	(void)cb; transitions++;
	eventKind[eventCount] = 1; eventTexture[eventCount] = tex ? tex->id : 0;
	eventOffset[eventCount++] = ( (uint64_t)src << 32 ) | (uint64_t)dst;
	eventLayout[eventCount - 1] = layout;
}
void Ral_CmdCopyTextureToBuffer( ralCommandBuffer_t *cb, ralTexture_t *src,
		ralBuffer_t *dst, const ralBufferTextureCopy_t *copy ) {
	(void)cb; copies++;
	eventKind[eventCount] = 2; eventTexture[eventCount] = src ? src->id : 0;
	eventOffset[eventCount] = copy ? copy->bufferOffset : UINT64_MAX;
	eventBuffer[eventCount] = dst ? dst->id : 0;
	if ( copy ) {
		eventX[eventCount] = copy->imageRect.x;
		eventY[eventCount] = copy->imageRect.y;
		eventWidth[eventCount] = copy->imageRect.width;
		eventHeight[eventCount] = copy->imageRect.height;
	}
	eventCount++;
}
void Ral_CmdPipelineBarrierFull( ralCommandBuffer_t *cb,
		const ralPipelineBarrierInfo_t *info ) {
	(void)cb;
	if ( info && info->memoryBarrierCount == 1 ) {
		barriers++;
		eventKind[eventCount] = 3; eventTexture[eventCount] = 0;
		eventOffset[eventCount++] =
			( (uint64_t)info->memoryBarriers[0].srcAccessMask << 32 )
			| info->memoryBarriers[0].dstAccessMask;
		eventLayout[eventCount - 1] = info->srcStageMask;
		eventWidth[eventCount - 1] = info->dstStageMask;
	}
}

static vkTemporalMainActivationReceipt_t Activation( void ) {
	vkTemporalMainActivationReceipt_t r;
	memset( &r, 0, sizeof( r ) );
	r.authority.token = 11; r.authority.frameId = 12;
	r.authority.worldIndex = 0; r.authority.width = 4; r.authority.height = 2;
	r.authority.topologyEpoch = 3; r.authority.planGeneration = 4;
	r.authority.geometryBufferSize = 1024; r.authority.uniformItemSize = 256;
	r.authority.requiredCapacity = 4; r.authority.rawEntMatCapacity = 4;
	r.authority.rawEntMatAllocationGeneration = 5;
	r.authority.payloadAllocationGeneration = 6;
	r.authority.payloadLayoutGeneration = 7;
	r.authority.targetAllocationGeneration = 8;
	r.authority.pipelineLayoutAllocationGeneration = 9;
	r.authority.materializationGeneration = 10;
	r.authority.pipelineTableGeneration = 11; r.authority.frameIndex = 0;
	r.prepared = 2; r.temporalSegments = 2; r.written = 1; r.invalidated = 1;
	r.drawSequence.lane0 = 0x1234; r.drawSequence.lane1 = 0x5678;
	r.drawSequence.count = 2;
	r.taggedSequence.lane0 = 0x2234; r.taggedSequence.lane1 = 0x6678;
	r.taggedSequence.count = 2; r.taggedSequence.genericCount = 2;
	r.auxiliaryCleared = qtrue;
	r.depthStoreRequired = qtrue; r.stencilStoreRequired = qtrue; r.ready = qtrue;
	return r;
}

int main( void ) {
	struct ralBackend_s backend = { 1 };
	struct ralCommandBuffer_s command = { 2 };
	struct ralTexture_s velocity = { 3 }, validity = { 4 };
	vkTemporalMotionMaterializationProductView_t view;
	vkTemporalMotionReadbackOwner_t owner;
	vkTemporalMotionReadbackTicket_t ticket, ticketSentinel;
	vkTemporalMotionReadbackContentReceipt_t content, contentSentinel;
	vkTemporalMainActivationReceipt_t activation = Activation(), mutated;
	temporalIqmGpuRecord_t iqmRecord;
	uint16_t *halfs;
	unsigned char *valid;
	memset( &view, 0, sizeof( view ) );
	view.velocity = &velocity; view.validity = &validity;
	view.targetAllocationGeneration = 8;
	view.pipelineLayoutAllocationGeneration = 9;
	view.allocationGeneration = 10;
	memset( &iqmRecord, 0, sizeof( iqmRecord ) );
	iqmRecord.currentBones[0][0] = 1.0f;
	iqmRecord.previousBones[0][0] = 2.0f;
	for ( uint32_t i = 0; i < 16u; ++i ) iqmRecord.rasterMvp[i] = (float)i;
	activation.iqm.ready = qtrue;
	activation.iqm.entityCount = 1u;
	activation.iqm.content.recordCount = 1u;
	activation.iqm.content.payload.ready = qtrue;
	activation.iqm.content.payload.mappedIdentity = &iqmRecord;
	activation.iqm.content.payload.recordBytes = TEMPORAL_IQM_RECORD_SIZE;
	activation.iqm.content.payload.recordCapacity = TEMPORAL_IQM_MAX_RECORDS;
	activation.iqm.content.payload.descriptorRange = TEMPORAL_IQM_SLOT_BYTES;
	activation.taggedSequence.count = 3u;
	activation.taggedSequence.iqmCount = 1u;

	VK_TemporalMotionReadbackInit( &owner );
	CHECK( !VK_TemporalMotionReadbackHasLive( &owner ) );
	CHECK( !VK_TemporalMotionReadbackIsArmed( &owner ) );
	CHECK( VK_TemporalMotionReadbackArm( &owner, 3 ) );
	CHECK( VK_TemporalMotionReadbackHasLive( &owner ) );
	CHECK( !VK_TemporalMotionReadbackArm( &owner, 1 ) );
	CHECK( VK_TemporalMotionReadbackPrepareAfterFence(
		&owner, &backend, 2, 0, 4, 2 ) );
	CHECK( creates == 1 && maps == 1 && owner.slots[0].bytes == 40 );
	{
		const unsigned char *p = (const unsigned char *)owner.slots[0].mapped;
		uint32_t i;
		for ( i = 0; i < 40; ++i ) CHECK( p[i] == 0x7f );
	}
	aliasCreate = owner.slots[0].buffer;
	CHECK( !VK_TemporalMotionReadbackPrepareAfterFence(
		&owner, &backend, 2, 1, 4, 2 ) );
	CHECK( destroys == 0 && unmaps == 0 && owner.slots[1].buffer == NULL );
	aliasCreate = NULL;
	mutated = activation; mutated.authority.frameIndex = 1;
	CHECK( !VK_TemporalMotionReadbackRecord(
		&owner, &command, 0, &mutated, &view ) );
	CHECK( transitions == 0 && copies == 0 && barriers == 0
		&& owner.slots[0].state == VK_TEMPORAL_READBACK_READY );
	CHECK( VK_TemporalMotionReadbackRecord(
		&owner, &command, 0, &activation, &view ) );
	CHECK( transitions == 4 && copies == 2 && barriers == 1 );
	CHECK( eventCount == 7 );
	CHECK( eventKind[0] == 1 && eventTexture[0] == velocity.id
		&& eventOffset[0] == ( (uint64_t)RAL_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT << 32
			| RAL_PIPELINE_STAGE_TRANSFER_BIT )
		&& eventLayout[0] == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL );
	CHECK( eventKind[1] == 1 && eventTexture[1] == validity.id
		&& eventOffset[1] == eventOffset[0]
		&& eventLayout[1] == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL );
	CHECK( eventKind[2] == 2 && eventTexture[2] == velocity.id
		&& eventOffset[2] == 0 && eventBuffer[2] == owner.slots[0].buffer->id
		&& eventX[2] == 0 && eventY[2] == 0
		&& eventWidth[2] == 4 && eventHeight[2] == 2 );
	CHECK( eventKind[3] == 2 && eventTexture[3] == validity.id
		&& eventOffset[3] == owner.slots[0].ticket.validityOffset
		&& eventBuffer[3] == owner.slots[0].buffer->id
		&& eventX[3] == 0 && eventY[3] == 0
		&& eventWidth[3] == 4 && eventHeight[3] == 2 );
	CHECK( eventKind[4] == 3
		&& eventOffset[4] == ( (uint64_t)VK_ACCESS_TRANSFER_WRITE_BIT << 32
			| VK_ACCESS_HOST_READ_BIT )
		&& eventLayout[4] == RAL_PIPELINE_STAGE_TRANSFER_BIT
		&& eventWidth[4] == RAL_PIPELINE_STAGE_HOST_BIT );
	CHECK( eventKind[5] == 1 && eventTexture[5] == velocity.id
		&& eventOffset[5] == ( (uint64_t)RAL_PIPELINE_STAGE_TRANSFER_BIT << 32
			| RAL_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT )
		&& eventLayout[5] == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL );
	CHECK( eventKind[6] == 1 && eventTexture[6] == validity.id
		&& eventOffset[6] == eventOffset[5]
		&& eventLayout[6] == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL );
	memset( &contentSentinel, 0xA5, sizeof( contentSentinel ) ); content = contentSentinel;
	CHECK( !VK_TemporalMotionReadbackCompleteAfterFence(
		&owner, 0, qfalse, &content ) );
	CHECK( memcmp( &content, &contentSentinel, sizeof( content ) ) == 0 );
	mutated = activation; mutated.drawSequence.lane0++;
	memset( &ticketSentinel, 0xA5, sizeof( ticketSentinel ) ); ticket = ticketSentinel;
	CHECK( !VK_TemporalMotionReadbackResolveSubmit(
		&owner, 0, qtrue, &mutated, &ticket ) );
	CHECK( memcmp( &ticket, &ticketSentinel, sizeof( ticket ) ) == 0 );
	CHECK( VK_TemporalMotionReadbackRecord(
		&owner, &command, 0, &activation, &view ) );
	CHECK( VK_TemporalMotionReadbackResolveSubmit(
		&owner, 0, qtrue, &activation, &ticket ) );
	CHECK( ticket.submitted && ticket.captureSerial == 2
		&& ticket.roiWidth == 4 && ticket.roiHeight == 2
		&& ticket.iqmRecordCount == 1u
		&& ticket.iqmCurrentPaletteHash
		&& ticket.iqmPreviousPaletteHash
		&& ticket.iqmCurrentPaletteHash != ticket.iqmPreviousPaletteHash
		&& memcmp( ticket.iqmRasterMvpBits, iqmRecord.rasterMvp,
			sizeof( ticket.iqmRasterMvpBits ) ) == 0 );
	halfs = (uint16_t *)owner.slots[0].mapped;
	valid = (unsigned char *)owner.slots[0].mapped + ticket.validityOffset;
	memset( halfs, 0, (size_t)ticket.velocityBytes );
	halfs[0] = 0x3c00;
	CHECK( VK_TemporalMotionReadbackCompleteAfterFence(
		&owner, 0, qtrue, &content ) );
	CHECK( !content.ready && content.validityOther == 8
		&& content.nonzeroValidVelocity == 0 );
	CHECK( VK_TemporalMotionReadbackPrepareAfterFence(
		&owner, &backend, 2, 0, 4, 2 ) );
	CHECK( VK_TemporalMotionReadbackRecord(
		&owner, &command, 0, &activation, &view ) );
	CHECK( VK_TemporalMotionReadbackResolveSubmit(
		&owner, 0, qtrue, &activation, &ticket ) );
	CHECK( ticket.captureSerial == 3 );
	halfs = (uint16_t *)owner.slots[0].mapped;
	valid = (unsigned char *)owner.slots[0].mapped + ticket.validityOffset;
	memset( halfs, 0, (size_t)ticket.velocityBytes );
	memset( valid, 0, 8 );
	halfs[0] = 0x3c00; valid[0] = 255;
	valid[2] = 255;
	CHECK( !VK_TemporalMotionReadbackCompleteAfterFence(
		&owner, 0, qfalse, &content ) );
	{
		vkTemporalMotionReadbackTicket_t saved = owner.slots[0].ticket;
		float savedBone = iqmRecord.currentBones[0][0];
		iqmRecord.currentBones[0][0] = 3.0f;
		CHECK( !VK_TemporalMotionReadbackCompleteAfterFence(
			&owner, 0, qtrue, &content ) );
		iqmRecord.currentBones[0][0] = savedBone;
		owner.slots[0].ticket.bufferAllocationGeneration++;
		CHECK( !VK_TemporalMotionReadbackCompleteAfterFence(
			&owner, 0, qtrue, &content ) );
		owner.slots[0].ticket = saved;
		owner.slots[0].ticket.roiX++;
		CHECK( !VK_TemporalMotionReadbackCompleteAfterFence(
			&owner, 0, qtrue, &content ) );
		owner.slots[0].ticket = saved;
		owner.slots[0].ticket.validityOffset++;
		CHECK( !VK_TemporalMotionReadbackCompleteAfterFence(
			&owner, 0, qtrue, &content ) );
		owner.slots[0].ticket = saved;
		owner.slots[0].ticket.totalBytes++;
		CHECK( !VK_TemporalMotionReadbackCompleteAfterFence(
			&owner, 0, qtrue, &content ) );
		owner.slots[0].ticket = saved;
	}
	owner.slots[0].ticket.commandSlot = 1;
	CHECK( !VK_TemporalMotionReadbackCompleteAfterFence(
		&owner, 0, qtrue, &content ) );
	owner.slots[0].ticket.commandSlot = 0;
	CHECK( VK_TemporalMotionReadbackCompleteAfterFence(
		&owner, 0, qtrue, &content ) );
	CHECK( content.fenceComplete && content.ready && content.pixels == 8
		&& content.validityFull == 2 && content.validityZero == 6
		&& content.validityOther == 0 && content.finiteVelocity == 8
		&& content.nonfiniteVelocity == 0
		&& content.nonzeroValidVelocity == 1
		&& content.nonzeroInvalidVelocity == 0 );
	CHECK( VK_TemporalMotionReadbackGetLatest( &owner, &content ) );
	memset( owner.slots[0].mapped, 0, (size_t)owner.slots[0].bytes );
	CHECK( VK_TemporalMotionReadbackPrepareAfterFence(
		&owner, &backend, 2, 0, 4, 2 ) );
	{
		const unsigned char *p = (const unsigned char *)owner.slots[0].mapped;
		uint32_t i;
		for ( i = 0; i < 40; ++i ) CHECK( p[i] == 0x7f );
	}
	aliasMap = owner.slots[0].mapped;
	CHECK( !VK_TemporalMotionReadbackPrepareAfterFence(
		&owner, &backend, 2, 1, 4, 2 ) );
	CHECK( owner.slots[1].buffer == NULL );
	aliasMap = NULL;

	activation.authority.frameIndex = 1;
	CHECK( VK_TemporalMotionReadbackPrepareAfterFence(
		&owner, &backend, 2, 1, 4, 2 ) );
	CHECK( VK_TemporalMotionReadbackRecord(
		&owner, &command, 1, &activation, &view ) );
	CHECK( VK_TemporalMotionReadbackResolveSubmit(
		&owner, 1, qtrue, &activation, NULL ) );
	CHECK( !VK_TemporalMotionReadbackArm( &owner, 1 ) );
	halfs = (uint16_t *)owner.slots[1].mapped;
	valid = (unsigned char *)owner.slots[1].mapped
		+ owner.slots[1].ticket.validityOffset;
	memset( halfs, 0, (size_t)owner.slots[1].ticket.velocityBytes );
	memset( valid, 0, 8 );
	halfs[0] = 0x7e00; valid[0] = 7;
	CHECK( !VK_TemporalMotionReadbackReleaseAfterIdle( &owner, qtrue ) );
	CHECK( VK_TemporalMotionReadbackCompleteAfterFence(
		&owner, 1, qtrue, &content ) );
	CHECK( !content.ready && content.validityOther == 1
		&& content.nonfiniteVelocity == 1 );
	CHECK( !VK_TemporalMotionReadbackIsArmed( &owner ) );
	CHECK( !VK_TemporalMotionReadbackReleaseAfterIdle( &owner, qfalse ) );
	CHECK( VK_TemporalMotionReadbackReleaseAfterIdle( &owner, qtrue ) );
	CHECK( destroys == 3 && unmaps == 3 && !VK_TemporalMotionReadbackHasLive( &owner ) );
	puts( "PASS vk temporal motion readback contract" );
	return 0;
}
