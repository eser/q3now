// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "ral_backend_conformance.h"

#include <limits.h>
#include <string.h>

static qboolean BackendTypeValid( ralBackendType_t backendType ) {
	return backendType >= RAL_BACKEND_VULKAN
		&& backendType < RAL_BACKEND_COUNT ? qtrue : qfalse;
}

static qboolean GenerationValid( uint64_t generation ) {
	return generation != 0u && generation != UINT64_MAX ? qtrue : qfalse;
}

static qboolean ReceiptValid( const ralBackendConformanceReceipt_t *receipt ) {
	if ( !receipt
			|| receipt->schemaVersion != RAL_BACKEND_CONFORMANCE_SCHEMA_VERSION
			|| !BackendTypeValid( receipt->backendType )
			|| !GenerationValid( receipt->backendGeneration )
			|| receipt->backendIdentity == (uintptr_t)0
			|| receipt->deviceIdentity == (uintptr_t)0
			|| receipt->graphicsQueueIdentity == (uintptr_t)0
			|| receipt->samplerIdentity == (uintptr_t)0
			|| !GenerationValid( receipt->samplerGeneration )
			|| receipt->copiedByteCount == 0u
			|| receipt->copiedByteDigest == 0u
			|| receipt->ready != qtrue ) return qfalse;
	if ( !Ral_CapabilityProfileExact( &receipt->capabilities,
			&receipt->capabilities )
			|| receipt->capabilities.backendType != receipt->backendType
			|| receipt->capabilities.generation != receipt->backendGeneration )
		return qfalse;
	if ( !Ral_AllocationReceiptExact( &receipt->uploadAllocation,
			&receipt->uploadAllocation )
			|| !Ral_AllocationReceiptExact( &receipt->readbackAllocation,
				&receipt->readbackAllocation )
			|| !Ral_AllocationReceiptExact( &receipt->textureAllocation,
				&receipt->textureAllocation ) ) return qfalse;
	if ( receipt->uploadAllocation.backendType != receipt->backendType
			|| receipt->readbackAllocation.backendType != receipt->backendType
			|| receipt->textureAllocation.backendType != receipt->backendType
			|| receipt->uploadAllocation.memoryClass != RAL_ALLOCATION_UPLOAD
			|| receipt->readbackAllocation.memoryClass != RAL_ALLOCATION_READBACK
			|| receipt->textureAllocation.memoryClass != RAL_ALLOCATION_DEVICE_LOCAL
			|| receipt->uploadAllocation.ownerIdentity
				== receipt->readbackAllocation.ownerIdentity
			|| receipt->uploadAllocation.requestedSize < receipt->copiedByteCount
			|| receipt->readbackAllocation.requestedSize < receipt->copiedByteCount )
		return qfalse;
	if ( !Ral_SubmissionReceiptValid( &receipt->submission )
			|| (uintptr_t)receipt->submission.backendIdentity
				!= receipt->backendIdentity
			|| receipt->submission.queue != RAL_QUEUE_GRAPHICS
			|| !Ral_TransferReceiptExact( &receipt->transfer,
				&receipt->transfer )
			|| receipt->transfer.request.backendType != receipt->backendType
			|| receipt->transfer.request.direction != RAL_TRANSFER_READBACK
			|| receipt->transfer.request.resourceKind != RAL_TRANSFER_BUFFER
			|| receipt->transfer.request.resourceIdentity
				!= receipt->readbackAllocation.ownerIdentity
			|| receipt->transfer.request.resourceGeneration
				!= receipt->readbackAllocation.allocationGeneration
			|| receipt->transfer.request.byteSize != receipt->copiedByteCount
			|| receipt->transfer.request.queue != RAL_QUEUE_GRAPHICS
			|| receipt->transfer.state != RAL_TRANSFER_COMPLETED
			|| receipt->transfer.submissionGeneration
				!= receipt->submission.generation
			|| receipt->completionGeneration != receipt->submission.generation
			|| receipt->transfer.completionGeneration
				!= receipt->completionGeneration ) return qfalse;
	return qtrue;
}

qboolean Ral_BackendConformanceBuild(
		const ralBackendConformanceFacts_t *facts,
		ralBackendConformanceReceipt_t *outReceipt ) {
	ralBackendConformanceReceipt_t candidate;
	if ( !facts || !outReceipt || !facts->capabilities
			|| !facts->uploadAllocation || !facts->readbackAllocation
			|| !facts->textureAllocation || !facts->submission
			|| !facts->transfer ) return qfalse;
	memset( &candidate, 0, sizeof( candidate ) );
	candidate.schemaVersion = RAL_BACKEND_CONFORMANCE_SCHEMA_VERSION;
	candidate.backendType = facts->backendType;
	candidate.backendGeneration = facts->backendGeneration;
	candidate.backendIdentity = facts->backendIdentity;
	candidate.deviceIdentity = facts->deviceIdentity;
	candidate.graphicsQueueIdentity = facts->graphicsQueueIdentity;
	candidate.capabilities = *facts->capabilities;
	candidate.uploadAllocation = *facts->uploadAllocation;
	candidate.readbackAllocation = *facts->readbackAllocation;
	candidate.textureAllocation = *facts->textureAllocation;
	candidate.samplerIdentity = facts->samplerIdentity;
	candidate.samplerGeneration = facts->samplerGeneration;
	candidate.submission = *facts->submission;
	candidate.transfer = *facts->transfer;
	candidate.completionGeneration = facts->completionGeneration;
	candidate.copiedByteCount = facts->copiedByteCount;
	candidate.copiedByteDigest = facts->copiedByteDigest;
	candidate.ready = qtrue;
	if ( !ReceiptValid( &candidate ) ) return qfalse;
	*outReceipt = candidate;
	return qtrue;
}

qboolean Ral_BackendConformanceReceiptExact(
		const ralBackendConformanceReceipt_t *a,
		const ralBackendConformanceReceipt_t *b ) {
	return ReceiptValid( a ) && ReceiptValid( b )
		&& a->backendType == b->backendType
		&& a->backendGeneration == b->backendGeneration
		&& a->backendIdentity == b->backendIdentity
		&& a->deviceIdentity == b->deviceIdentity
		&& a->graphicsQueueIdentity == b->graphicsQueueIdentity
		&& Ral_CapabilityProfileExact( &a->capabilities, &b->capabilities )
		&& Ral_AllocationReceiptExact( &a->uploadAllocation,
			&b->uploadAllocation )
		&& Ral_AllocationReceiptExact( &a->readbackAllocation,
			&b->readbackAllocation )
		&& Ral_AllocationReceiptExact( &a->textureAllocation,
			&b->textureAllocation )
		&& a->samplerIdentity == b->samplerIdentity
		&& a->samplerGeneration == b->samplerGeneration
		&& Ral_SubmissionReceiptExact( &a->submission, &b->submission )
		&& Ral_TransferReceiptExact( &a->transfer, &b->transfer )
		&& a->completionGeneration == b->completionGeneration
		&& a->copiedByteCount == b->copiedByteCount
		&& a->copiedByteDigest == b->copiedByteDigest
		&& a->ready == b->ready ? qtrue : qfalse;
}

qboolean Ral_BackendConformanceCompatible(
		const ralBackendConformanceReceipt_t *a,
		const ralBackendConformanceReceipt_t *b ) {
	uint32_t i;
	if ( !ReceiptValid( a ) || !ReceiptValid( b )
			|| a->backendType == b->backendType
			|| a->copiedByteCount != b->copiedByteCount
			|| a->copiedByteDigest != b->copiedByteDigest ) return qfalse;
	for ( i = 0u; i < RAL_CAP_COUNT; ++i ) {
		const ralCapabilityEntry_t *left = &a->capabilities.entries[i];
		const ralCapabilityEntry_t *right = &b->capabilities.entries[i];
		if ( left->id != right->id || left->requirement != right->requirement )
			return qfalse;
		if ( left->requirement == RAL_CAP_REQUIREMENT_REQUIRED
				&& ( left->outcome == RAL_CAP_OUTCOME_DISABLED
					|| right->outcome == RAL_CAP_OUTCOME_DISABLED ) ) return qfalse;
	}
	return qtrue;
}

static qboolean RecreateValid( const ralBackendRecreateReceipt_t *receipt ) {
	return receipt && receipt->schemaVersion == RAL_BACKEND_CONFORMANCE_SCHEMA_VERSION
		&& ReceiptValid( &receipt->previous )
		&& ReceiptValid( &receipt->replacement )
		&& receipt->backendType == receipt->previous.backendType
		&& receipt->backendType == receipt->replacement.backendType
		&& receipt->replacement.backendGeneration
			> receipt->previous.backendGeneration
		&& Ral_MemoryFailureReceiptExact( &receipt->loss, &receipt->loss )
		&& receipt->loss.event.backendType == receipt->backendType
		&& receipt->loss.event.cause == RAL_MEMORY_FAILURE_DEVICE_LOST
		&& receipt->loss.action == RAL_MEMORY_RECOVERY_RECREATE_BACKEND
		&& receipt->loss.retryAllowed == qfalse
		&& receipt->loss.preserveLiveParent == qtrue
		&& receipt->ready == qtrue ? qtrue : qfalse;
}

qboolean Ral_BackendRecreateBuild(
		const ralBackendConformanceReceipt_t *previous,
		const ralMemoryFailureReceipt_t *loss,
		const ralBackendConformanceReceipt_t *replacement,
		ralBackendRecreateReceipt_t *outReceipt ) {
	ralBackendRecreateReceipt_t candidate;
	if ( !previous || !loss || !replacement || !outReceipt ) return qfalse;
	memset( &candidate, 0, sizeof( candidate ) );
	candidate.schemaVersion = RAL_BACKEND_CONFORMANCE_SCHEMA_VERSION;
	candidate.backendType = previous->backendType;
	candidate.previous = *previous;
	candidate.loss = *loss;
	candidate.replacement = *replacement;
	candidate.ready = qtrue;
	if ( !RecreateValid( &candidate ) ) return qfalse;
	*outReceipt = candidate;
	return qtrue;
}

qboolean Ral_BackendRecreateReceiptExact(
		const ralBackendRecreateReceipt_t *a,
		const ralBackendRecreateReceipt_t *b ) {
	return RecreateValid( a ) && RecreateValid( b )
		&& a->backendType == b->backendType
		&& Ral_BackendConformanceReceiptExact( &a->previous, &b->previous )
		&& Ral_MemoryFailureReceiptExact( &a->loss, &b->loss )
		&& Ral_BackendConformanceReceiptExact( &a->replacement,
			&b->replacement )
		&& a->ready == b->ready ? qtrue : qfalse;
}
