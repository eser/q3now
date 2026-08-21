// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
#include "ral_memory_recovery.h"

#include <string.h>

static qboolean BoolValid( qboolean value ) { return value == qfalse || value == qtrue; }

static qboolean EventValid( const ralMemoryFailureEvent_t *event ) {
	return event && event->backendType >= RAL_BACKEND_VULKAN
		&& event->backendType <= RAL_BACKEND_WEBGL2
		&& event->cause >= RAL_MEMORY_FAILURE_NO_COMPATIBLE_TYPE
		&& event->cause <= RAL_MEMORY_FAILURE_DEVICE_LOST
		&& event->memoryClass >= RAL_ALLOCATION_DEVICE_LOCAL
		&& event->memoryClass <= RAL_ALLOCATION_TRANSIENT
		&& event->residency >= RAL_ALLOCATION_RESIDENCY_PERMANENT
		&& event->residency <= RAL_ALLOCATION_RESIDENCY_TRANSIENT
		&& event->criticality >= RAL_MEMORY_CRITICALITY_OPTIONAL
		&& event->criticality <= RAL_MEMORY_CRITICALITY_REQUIRED
		&& event->requestedBytes != 0 && BoolValid( event->budgetKnown )
		&& BoolValid( event->liveParent ) && event->attempt != 0
		&& event->maxAttempts != 0 && event->attempt <= event->maxAttempts
		&& ( event->budgetKnown
			? event->budgetBytes != 0 && event->usedBytes <= event->budgetBytes
			: event->budgetBytes == 0 && event->usedBytes == 0 )
		&& event->reclaimableBytes <= event->usedBytes;
}

static ralMemoryRecoveryAction_t Decide( const ralMemoryFailureEvent_t *event ) {
	if ( event->cause == RAL_MEMORY_FAILURE_DEVICE_LOST )
		return RAL_MEMORY_RECOVERY_RECREATE_BACKEND;
	if ( event->cause == RAL_MEMORY_FAILURE_RESIZE )
		return event->liveParent ? RAL_MEMORY_RECOVERY_DEFER
			: RAL_MEMORY_RECOVERY_RETRY_AFTER_DRAIN;
	if ( event->cause == RAL_MEMORY_FAILURE_NO_COMPATIBLE_TYPE )
		return event->criticality == RAL_MEMORY_CRITICALITY_OPTIONAL
			? RAL_MEMORY_RECOVERY_REDUCE_QUALITY : RAL_MEMORY_RECOVERY_FAIL;
	if ( event->cause == RAL_MEMORY_FAILURE_HOST_OOM )
		return event->criticality == RAL_MEMORY_CRITICALITY_REQUIRED
			? RAL_MEMORY_RECOVERY_FAIL : RAL_MEMORY_RECOVERY_DEFER;
	if ( event->attempt == event->maxAttempts ) {
		if ( event->criticality == RAL_MEMORY_CRITICALITY_OPTIONAL )
			return RAL_MEMORY_RECOVERY_REDUCE_QUALITY;
		if ( event->criticality == RAL_MEMORY_CRITICALITY_STREAMING )
			return RAL_MEMORY_RECOVERY_DEFER;
		return RAL_MEMORY_RECOVERY_FAIL;
	}
	if ( event->reclaimableBytes >= event->requestedBytes
			|| event->cause == RAL_MEMORY_FAILURE_PRESSURE )
		return RAL_MEMORY_RECOVERY_EVICT_AND_RETRY;
	if ( event->criticality == RAL_MEMORY_CRITICALITY_OPTIONAL )
		return RAL_MEMORY_RECOVERY_REDUCE_QUALITY;
	if ( event->criticality == RAL_MEMORY_CRITICALITY_STREAMING )
		return RAL_MEMORY_RECOVERY_DEFER;
	return RAL_MEMORY_RECOVERY_RETRY_AFTER_DRAIN;
}

static qboolean ReceiptValid( const ralMemoryFailureReceipt_t *receipt ) {
	qboolean retry;
	if ( !receipt || receipt->schemaVersion != RAL_MEMORY_FAILURE_SCHEMA_VERSION
			|| !EventValid( &receipt->event )
			|| receipt->action < RAL_MEMORY_RECOVERY_RETRY_AFTER_DRAIN
			|| receipt->action > RAL_MEMORY_RECOVERY_FAIL
			|| receipt->failureGeneration == 0
			|| receipt->failureGeneration == UINT64_MAX
			|| !BoolValid( receipt->retryAllowed )
			|| !BoolValid( receipt->preserveLiveParent )
			|| receipt->ready != qtrue || receipt->action != Decide( &receipt->event ) ) return qfalse;
	retry = receipt->action == RAL_MEMORY_RECOVERY_RETRY_AFTER_DRAIN
		|| receipt->action == RAL_MEMORY_RECOVERY_EVICT_AND_RETRY;
	return receipt->retryAllowed == retry
		&& receipt->preserveLiveParent == receipt->event.liveParent;
}

qboolean Ral_MemoryFailureDecide( const ralMemoryFailureEvent_t *event,
		uint64_t failureGeneration, ralMemoryFailureReceipt_t *outReceipt ) {
	ralMemoryFailureReceipt_t candidate;
	if ( !outReceipt || !EventValid( event ) || failureGeneration == 0
			|| failureGeneration == UINT64_MAX ) return qfalse;
	memset( &candidate, 0, sizeof( candidate ) );
	candidate.schemaVersion = RAL_MEMORY_FAILURE_SCHEMA_VERSION;
	candidate.event = *event;
	candidate.action = Decide( event );
	candidate.failureGeneration = failureGeneration;
	candidate.retryAllowed = candidate.action == RAL_MEMORY_RECOVERY_RETRY_AFTER_DRAIN
		|| candidate.action == RAL_MEMORY_RECOVERY_EVICT_AND_RETRY;
	candidate.preserveLiveParent = event->liveParent;
	candidate.ready = qtrue;
	if ( !ReceiptValid( &candidate ) ) return qfalse;
	*outReceipt = candidate;
	return qtrue;
}

qboolean Ral_MemoryFailureReceiptExact( const ralMemoryFailureReceipt_t *a,
		const ralMemoryFailureReceipt_t *b ) {
	return ReceiptValid( a ) && ReceiptValid( b ) && memcmp( a, b, sizeof( *a ) ) == 0;
}

void Ral_MemoryFailureLedgerInit( ralMemoryFailureLedger_t *ledger ) {
	if ( ledger ) memset( ledger, 0, sizeof( *ledger ) );
}

qboolean Ral_MemoryFailureLedgerPublish( ralMemoryFailureLedger_t *ledger,
		const ralMemoryFailureEvent_t *event ) {
	ralMemoryFailureReceipt_t candidate;
	if ( !ledger || ledger->nextGeneration >= UINT64_MAX - 1u
			|| !Ral_MemoryFailureDecide( event, ledger->nextGeneration + 1u,
				&candidate ) ) return qfalse;
	ledger->last = candidate;
	ledger->nextGeneration = candidate.failureGeneration;
	ledger->ready = qtrue;
	return qtrue;
}

qboolean Ral_MemoryFailureLedgerGet( const ralMemoryFailureLedger_t *ledger,
		ralMemoryFailureReceipt_t *outReceipt ) {
	ralMemoryFailureReceipt_t candidate;
	if ( !ledger || !outReceipt || ledger->ready != qtrue
			|| !Ral_MemoryFailureReceiptExact( &ledger->last, &ledger->last ) ) return qfalse;
	candidate = ledger->last;
	*outReceipt = candidate;
	return qtrue;
}
