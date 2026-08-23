// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
#include "ral_transfer.h"

#include <string.h>

static qboolean BoolValid( qboolean value ) { return value == qfalse || value == qtrue; }

static qboolean RequestValid( const ralTransferRequest_t *request ) {
	if ( !request || request->backendType < RAL_BACKEND_VULKAN
			|| request->backendType > RAL_BACKEND_WEBGL2
			|| request->direction < RAL_TRANSFER_UPLOAD
			|| request->direction > RAL_TRANSFER_READBACK
			|| request->resourceKind < RAL_TRANSFER_BUFFER
			|| request->resourceKind > RAL_TRANSFER_TEXTURE
			|| request->resourceIdentity == (uintptr_t)0
			|| request->resourceGeneration == 0u
			|| request->resourceGeneration == UINT64_MAX
			|| request->byteSize == 0u || request->byteBudget == 0u
			|| request->byteSize > request->byteBudget
			|| request->byteOffset > UINT64_MAX - request->byteSize
			|| request->byteOffset > request->byteBudget - request->byteSize
			|| request->queue < RAL_QUEUE_GRAPHICS || request->queue > RAL_QUEUE_TRANSFER ) return qfalse;
	if ( request->resourceKind == RAL_TRANSFER_BUFFER )
		return request->mipLevel == 0u && request->arrayLayer == 0u
			&& request->offsetX == 0u && request->offsetY == 0u
			&& request->width == 0u && request->height == 0u && request->depth == 0u
			&& request->bytesPerRow == 0u && request->rowsPerImage == 0u
			&& request->offsetZ == 0u && request->textureAspects == 0u;
	if ( request->byteOffset != 0u || request->width == 0u
			|| request->height == 0u || request->depth == 0u
			|| ( request->textureAspects != 0u
				&& request->textureAspects != 1u
				&& request->textureAspects != 2u
				&& request->textureAspects != 4u ) ) return qfalse;
	if ( request->direction == RAL_TRANSFER_READBACK ) {
		if ( request->depth != 1u || request->bytesPerRow == 0u
				|| ( request->bytesPerRow & 255u ) != 0u
				|| request->rowsPerImage < request->height
				|| request->height > UINT64_MAX / request->bytesPerRow
				|| request->byteSize
					!= (uint64_t)request->bytesPerRow * request->height ) return qfalse;
	} else if ( ( request->bytesPerRow == 0u ) != ( request->rowsPerImage == 0u ) )
		return qfalse;
	return qtrue;
}

static qboolean ReceiptValid( const ralTransferReceipt_t *receipt ) {
	if ( !receipt || receipt->schemaVersion != RAL_TRANSFER_SCHEMA_VERSION
			|| !RequestValid( &receipt->request )
			|| receipt->state < RAL_TRANSFER_PREPARED
			|| receipt->state > RAL_TRANSFER_CANCELED
			|| receipt->outcome < RAL_TRANSFER_OUTCOME_NONE
			|| receipt->outcome > RAL_TRANSFER_OUTCOME_MANAGED_ASYNC
			|| receipt->transferGeneration == 0u
			|| receipt->transferGeneration == UINT64_MAX
			|| !BoolValid( receipt->ready ) || receipt->ready != qtrue ) return qfalse;
	switch ( receipt->state ) {
	case RAL_TRANSFER_PREPARED:
	case RAL_TRANSFER_CANCELED:
		return receipt->outcome == RAL_TRANSFER_OUTCOME_NONE
			&& receipt->submissionGeneration == 0u
			&& receipt->completionGeneration == 0u;
	case RAL_TRANSFER_SUBMITTED:
		return ( receipt->outcome == RAL_TRANSFER_OUTCOME_NATIVE_ASYNC
				|| receipt->outcome == RAL_TRANSFER_OUTCOME_MANAGED_ASYNC )
			&& receipt->submissionGeneration != 0u
			&& receipt->submissionGeneration != UINT64_MAX
			&& receipt->completionGeneration == 0u;
	case RAL_TRANSFER_COMPLETED:
		return receipt->outcome != RAL_TRANSFER_OUTCOME_NONE
			&& receipt->submissionGeneration != 0u
			&& receipt->submissionGeneration != UINT64_MAX
			&& receipt->completionGeneration != 0u
			&& receipt->completionGeneration != UINT64_MAX
			&& ( receipt->outcome != RAL_TRANSFER_OUTCOME_SYNCHRONOUS
				|| receipt->completionGeneration == receipt->submissionGeneration );
	default: return qfalse;
	}
}

qboolean Ral_TransferPrepare( const ralTransferRequest_t *request,
		uint64_t transferGeneration, ralTransferReceipt_t *out ) {
	ralTransferReceipt_t candidate;
	if ( !out || !RequestValid( request ) || transferGeneration == 0u
			|| transferGeneration == UINT64_MAX ) return qfalse;
	memset( &candidate, 0, sizeof( candidate ) );
	candidate.schemaVersion = RAL_TRANSFER_SCHEMA_VERSION;
	candidate.request = *request;
	candidate.state = RAL_TRANSFER_PREPARED;
	candidate.transferGeneration = transferGeneration;
	candidate.ready = qtrue;
	*out = candidate;
	return qtrue;
}

qboolean Ral_TransferPublish( const ralTransferReceipt_t *prepared,
		ralTransferOutcome_t outcome, uint64_t submissionGeneration,
		ralTransferReceipt_t *out ) {
	ralTransferReceipt_t candidate;
	if ( !out || !ReceiptValid( prepared ) || prepared->state != RAL_TRANSFER_PREPARED
			|| outcome < RAL_TRANSFER_OUTCOME_SYNCHRONOUS
			|| outcome > RAL_TRANSFER_OUTCOME_MANAGED_ASYNC
			|| submissionGeneration == 0u || submissionGeneration == UINT64_MAX ) return qfalse;
	candidate = *prepared;
	candidate.outcome = outcome;
	candidate.submissionGeneration = submissionGeneration;
	if ( outcome == RAL_TRANSFER_OUTCOME_SYNCHRONOUS ) {
		candidate.state = RAL_TRANSFER_COMPLETED;
		candidate.completionGeneration = submissionGeneration;
	} else candidate.state = RAL_TRANSFER_SUBMITTED;
	*out = candidate;
	return qtrue;
}

qboolean Ral_TransferComplete( const ralTransferReceipt_t *submitted,
		uint64_t completionGeneration, qboolean fenceCompleted,
		ralTransferReceipt_t *out ) {
	ralTransferReceipt_t candidate;
	if ( !out || !ReceiptValid( submitted ) || submitted->state != RAL_TRANSFER_SUBMITTED
			|| !BoolValid( fenceCompleted ) || fenceCompleted != qtrue
			|| completionGeneration == 0u || completionGeneration == UINT64_MAX ) return qfalse;
	candidate = *submitted;
	candidate.state = RAL_TRANSFER_COMPLETED;
	candidate.completionGeneration = completionGeneration;
	*out = candidate;
	return qtrue;
}

qboolean Ral_TransferCancel( const ralTransferReceipt_t *prepared,
		ralTransferReceipt_t *out ) {
	ralTransferReceipt_t candidate;
	if ( !out || !ReceiptValid( prepared ) || prepared->state != RAL_TRANSFER_PREPARED ) return qfalse;
	candidate = *prepared;
	candidate.state = RAL_TRANSFER_CANCELED;
	*out = candidate;
	return qtrue;
}

qboolean Ral_TransferReceiptExact( const ralTransferReceipt_t *a,
		const ralTransferReceipt_t *b ) {
	return ReceiptValid( a ) && ReceiptValid( b ) && !memcmp( a, b, sizeof( *a ) );
}

static qboolean BufferUploadReceiptValid(
		const ralBufferUploadReceipt_t *receipt ) {
	return receipt
		&& receipt->schemaVersion == RAL_BUFFER_UPLOAD_RECEIPT_SCHEMA_VERSION
		&& Ral_TransferReceiptExact( &receipt->transfer, &receipt->transfer )
		&& receipt->transfer.request.direction == RAL_TRANSFER_UPLOAD
		&& receipt->transfer.request.resourceKind == RAL_TRANSFER_BUFFER
		&& receipt->transfer.state == RAL_TRANSFER_COMPLETED
		&& receipt->graphicsVisibilityGeneration > 0u
		&& receipt->graphicsVisibilityGeneration < UINT64_MAX
		&& receipt->graphicsVisibilityGeneration
			== receipt->transfer.completionGeneration
		&& receipt->ready == qtrue;
}

qboolean Ral_BufferUploadReceiptBuild( const ralTransferReceipt_t *completed,
		uint64_t graphicsVisibilityGeneration,
		ralBufferUploadReceipt_t *out ) {
	ralBufferUploadReceipt_t candidate;
	if ( !out || !completed
			|| !Ral_TransferReceiptExact( completed, completed )
			|| completed->request.direction != RAL_TRANSFER_UPLOAD
			|| completed->request.resourceKind != RAL_TRANSFER_BUFFER
			|| completed->state != RAL_TRANSFER_COMPLETED
			|| graphicsVisibilityGeneration == 0u
			|| graphicsVisibilityGeneration == UINT64_MAX ) return qfalse;
	memset( &candidate, 0, sizeof( candidate ) );
	candidate.schemaVersion = RAL_BUFFER_UPLOAD_RECEIPT_SCHEMA_VERSION;
	candidate.transfer = *completed;
	candidate.graphicsVisibilityGeneration = graphicsVisibilityGeneration;
	candidate.ready = qtrue;
	if ( !BufferUploadReceiptValid( &candidate ) ) return qfalse;
	*out = candidate;
	return qtrue;
}

qboolean Ral_BufferUploadReceiptExact( const ralBufferUploadReceipt_t *a,
		const ralBufferUploadReceipt_t *b ) {
	return BufferUploadReceiptValid( a ) && BufferUploadReceiptValid( b )
		&& !memcmp( a, b, sizeof( *a ) );
}
