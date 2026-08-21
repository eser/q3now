// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "ral_readback.h"

#include <stdlib.h>
#include <string.h>

struct ralReadbackOwner_s {
	ralReadbackReceipt_t receipt;
	ralBuffer_t *staging;
	ralBufferMapTicket_t activeMap;
	qboolean mapActive;
	void *context;
	const ralReadbackOps_t *ops;
};

static qboolean BoolValid( qboolean value ) { return value == qfalse || value == qtrue; }

static qboolean OpsValid( const ralReadbackOps_t *ops ) {
	return ops && ops->createStaging && ops->submitCopy && ops->submissionCompleted
		&& ops->mapBegin && ops->mapPoll && ops->mapUnmap && ops->mapCancel
		&& ops->candidateAllowed && ops->retireStaging && ops->retireSubmission;
}

static qboolean ReceiptValid( const ralReadbackReceipt_t *receipt ) {
	if ( !receipt || receipt->schemaVersion != RAL_READBACK_SCHEMA_VERSION
			|| !receipt->stagingIdentity || !receipt->submissionIdentity
			|| !BoolValid( receipt->ready )
			|| !Ral_TransferReceiptExact( &receipt->transfer, &receipt->transfer )
			|| !Ral_AllocationReceiptExact( &receipt->stagingAllocation,
				&receipt->stagingAllocation )
			|| receipt->transfer.request.direction != RAL_TRANSFER_READBACK
			|| receipt->transfer.request.backendType != receipt->stagingAllocation.backendType
			|| receipt->stagingAllocation.memoryClass != RAL_ALLOCATION_READBACK
			|| receipt->stagingAllocation.ownerIdentity != receipt->stagingIdentity
			|| receipt->stagingAllocation.requestedSize < receipt->transfer.request.byteSize
			|| receipt->stagingAllocation.committedSize < receipt->transfer.request.byteSize ) return qfalse;
	if ( receipt->ready )
		return receipt->transfer.state == RAL_TRANSFER_COMPLETED
			&& receipt->transfer.ready == qtrue;
	return receipt->transfer.state == RAL_TRANSFER_SUBMITTED
		&& receipt->transfer.ready == qtrue;
}

qboolean Ral_ReadbackReceiptExact( const ralReadbackReceipt_t *a,
		const ralReadbackReceipt_t *b ) {
	return ReceiptValid( a ) && ReceiptValid( b )
		&& Ral_TransferReceiptExact( &a->transfer, &b->transfer )
		&& Ral_AllocationReceiptExact( &a->stagingAllocation, &b->stagingAllocation )
		&& a->stagingIdentity == b->stagingIdentity
		&& a->submissionIdentity == b->submissionIdentity
		&& a->ready == b->ready;
}

qboolean Ral_ReadbackCreate( const ralReadbackCreateInfo_t *createInfo,
		ralReadbackOwner_t **outOwner ) {
	ralReadbackOwner_t *candidate = NULL;
	ralTransferReceipt_t prepared, submitted;
	ralAllocationReceipt_t allocation;
	ralBuffer_t *staging = NULL;
	ralTransferOutcome_t outcome = RAL_TRANSFER_OUTCOME_NONE;
	uintptr_t submissionIdentity = 0;
	uint64_t submissionGeneration = 0;
	if ( !createInfo || !outOwner || *outOwner || !OpsValid( createInfo->ops )
			|| createInfo->transfer.direction != RAL_TRANSFER_READBACK
			|| createInfo->transferGeneration == 0
			|| createInfo->transferGeneration == UINT64_MAX
			|| !Ral_TransferPrepare( &createInfo->transfer,
				createInfo->transferGeneration, &prepared ) ) return qfalse;
	memset( &allocation, 0, sizeof( allocation ) );
	if ( !createInfo->ops->createStaging( createInfo->context,
			createInfo->transfer.byteSize, &staging, &allocation )
			|| !staging || !Ral_AllocationReceiptExact( &allocation, &allocation )
			|| allocation.memoryClass != RAL_ALLOCATION_READBACK
			|| allocation.backendType != createInfo->transfer.backendType
			|| allocation.ownerIdentity != (uintptr_t)staging
			|| allocation.requestedSize < createInfo->transfer.byteSize
			|| !createInfo->ops->candidateAllowed( createInfo->context,
				RAL_READBACK_ROLE_STAGING, (uintptr_t)staging ) ) goto fail;
	if ( !createInfo->ops->submitCopy( createInfo->context, &createInfo->transfer,
			staging, &outcome, &submissionIdentity, &submissionGeneration )
			|| ( outcome != RAL_TRANSFER_OUTCOME_NATIVE_ASYNC
				&& outcome != RAL_TRANSFER_OUTCOME_MANAGED_ASYNC )
			|| !submissionIdentity || submissionGeneration == 0
			|| submissionGeneration == UINT64_MAX
			|| !createInfo->ops->candidateAllowed( createInfo->context,
				RAL_READBACK_ROLE_SUBMISSION, submissionIdentity )
			|| !Ral_TransferPublish( &prepared, outcome, submissionGeneration, &submitted ) ) goto fail;
	candidate = (ralReadbackOwner_t *)calloc( 1, sizeof( *candidate ) );
	if ( !candidate ) goto fail;
	candidate->receipt.schemaVersion = RAL_READBACK_SCHEMA_VERSION;
	candidate->receipt.transfer = submitted;
	candidate->receipt.stagingAllocation = allocation;
	candidate->receipt.stagingIdentity = (uintptr_t)staging;
	candidate->receipt.submissionIdentity = submissionIdentity;
	candidate->receipt.ready = qfalse;
	if ( !ReceiptValid( &candidate->receipt ) ) goto fail;
	candidate->staging = staging;
	candidate->context = createInfo->context;
	candidate->ops = createInfo->ops;
	*outOwner = candidate;
	return qtrue;
fail:
	if ( candidate ) free( candidate );
	if ( submissionIdentity ) createInfo->ops->retireSubmission(
		createInfo->context, submissionIdentity );
	if ( staging ) createInfo->ops->retireStaging( createInfo->context, staging );
	if ( createInfo->ops->destroyContext ) createInfo->ops->destroyContext( createInfo->context );
	return qfalse;
}

qboolean Ral_ReadbackGetReceipt( const ralReadbackOwner_t *owner,
		ralReadbackReceipt_t *outReceipt ) {
	if ( !owner || !outReceipt || !ReceiptValid( &owner->receipt ) ) return qfalse;
	*outReceipt = owner->receipt;
	return qtrue;
}

qboolean Ral_ReadbackComplete( ralReadbackOwner_t *owner ) {
	ralTransferReceipt_t completed;
	uint64_t completionGeneration;
	qboolean fenceCompleted = qfalse;
	if ( !owner || owner->mapActive || owner->receipt.ready
			|| owner->receipt.transfer.submissionGeneration == 0u
			|| owner->receipt.transfer.submissionGeneration == UINT64_MAX
			|| !owner->ops->submissionCompleted( owner->context,
				owner->receipt.submissionIdentity, &fenceCompleted ) ) return qfalse;
	completionGeneration = owner->receipt.transfer.submissionGeneration;
	if ( !Ral_TransferComplete( &owner->receipt.transfer,
			completionGeneration, fenceCompleted, &completed ) ) return qfalse;
	owner->receipt.transfer = completed;
	owner->receipt.ready = qtrue;
	return ReceiptValid( &owner->receipt );
}

ralResult_t Ral_ReadbackMapBegin( ralReadbackOwner_t *owner,
		ralBufferMapTicket_t *outTicket ) {
	ralBufferMapRequest_t request;
	ralBufferMapTicket_t ticket;
	ralResult_t result;
	if ( !owner || !outTicket || owner->mapActive || !owner->receipt.ready )
		return ralErrorInvalidArgument;
	memset( &request, 0, sizeof( request ) );
	request.mode = RAL_MAP_READ;
	request.size = owner->receipt.transfer.request.byteSize;
	result = owner->ops->mapBegin( owner->context, owner->staging, &request, &ticket );
	if ( result != ralSuccess || !Ral_BufferMapTicketValid( &ticket )
			|| ticket.bufferIdentity != owner->staging
			|| ticket.request.mode != RAL_MAP_READ || ticket.request.offset != 0
			|| ticket.request.size != request.size ) return result == ralSuccess
			? ralErrorInvalidArgument : result;
	owner->activeMap = ticket;
	owner->mapActive = qtrue;
	*outTicket = ticket;
	return ralSuccess;
}

ralResult_t Ral_ReadbackMapPoll( ralReadbackOwner_t *owner,
		const ralBufferMapTicket_t *authority, ralBufferMapTicket_t *outTicket ) {
	ralBufferMapTicket_t ticket;
	ralResult_t result;
	if ( !owner || !authority || !outTicket || !owner->mapActive
			|| !Ral_BufferMapTicketExact( &owner->activeMap, authority ) )
		return ralErrorInvalidArgument;
	result = owner->ops->mapPoll( owner->context, owner->staging, authority, &ticket );
	if ( result != ralSuccess || !Ral_BufferMapTicketValid( &ticket )
			|| ticket.bufferIdentity != owner->staging
			|| ticket.generation != authority->generation
			|| ticket.request.mode != authority->request.mode
			|| ticket.request.offset != authority->request.offset
			|| ticket.request.size != authority->request.size ) return result == ralSuccess
			? ralErrorInvalidArgument : result;
	owner->activeMap = ticket;
	*outTicket = ticket;
	return ralSuccess;
}

ralResult_t Ral_ReadbackMapUnmap( ralReadbackOwner_t *owner,
		const ralBufferMapTicket_t *readyTicket ) {
	if ( !owner || !readyTicket || !owner->mapActive
			|| readyTicket->status != RAL_BUFFER_MAP_READY
			|| !Ral_BufferMapTicketExact( &owner->activeMap, readyTicket )
			|| owner->ops->mapUnmap( owner->context, owner->staging,
				readyTicket ) != ralSuccess ) return ralErrorInvalidArgument;
	memset( &owner->activeMap, 0, sizeof( owner->activeMap ) );
	owner->mapActive = qfalse;
	return ralSuccess;
}

ralResult_t Ral_ReadbackMapCancel( ralReadbackOwner_t *owner,
		const ralBufferMapTicket_t *pendingTicket ) {
	if ( !owner || !pendingTicket || !owner->mapActive
			|| pendingTicket->status != RAL_BUFFER_MAP_PENDING
			|| !Ral_BufferMapTicketExact( &owner->activeMap, pendingTicket )
			|| owner->ops->mapCancel( owner->context, owner->staging,
				pendingTicket ) != ralSuccess ) return ralErrorInvalidArgument;
	memset( &owner->activeMap, 0, sizeof( owner->activeMap ) );
	owner->mapActive = qfalse;
	return ralSuccess;
}

qboolean Ral_ReadbackRelease( ralReadbackOwner_t **owner ) {
	ralReadbackOwner_t *live;
	if ( !owner ) return qfalse;
	if ( !*owner ) return qtrue;
	live = *owner;
	if ( !ReceiptValid( &live->receipt ) || !live->receipt.ready || live->mapActive )
		return qfalse;
	live->ops->retireStaging( live->context, live->staging );
	live->ops->retireSubmission( live->context, live->receipt.submissionIdentity );
	if ( live->ops->destroyContext ) live->ops->destroyContext( live->context );
	free( live );
	*owner = NULL;
	return qtrue;
}
