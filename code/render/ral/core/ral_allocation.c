// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
#include "ral_allocation.h"

#include <limits.h>
#include <string.h>

static qboolean BoolValid( qboolean value ) {
	return value == qfalse || value == qtrue;
}

static qboolean PowerOfTwo( uint64_t value ) {
	return value != 0u && (value & (value - 1u)) == 0u;
}

static ralAllocationPressure_t PressureAfter( uint64_t used, uint64_t budget ) {
	const uint64_t critical = budget - budget / 10u;
	const uint64_t warning = budget - budget / 4u;
	if ( used >= critical ) return RAL_ALLOCATION_PRESSURE_CRITICAL;
	if ( used >= warning ) return RAL_ALLOCATION_PRESSURE_WARNING;
	return RAL_ALLOCATION_PRESSURE_NORMAL;
}

qboolean Ral_AllocationReceiptBuild( const ralAllocationRequest_t *request,
		const ralAllocationFacts_t *facts, ralAllocationReceipt_t *out ) {
	ralAllocationReceipt_t candidate;
	uint64_t roundedSize;
	qboolean emulated = qfalse;
	if ( !request || !facts || !out
			|| request->memoryClass < RAL_ALLOCATION_DEVICE_LOCAL
			|| request->memoryClass > RAL_ALLOCATION_TRANSIENT
			|| request->residency < RAL_ALLOCATION_RESIDENCY_PERMANENT
			|| request->residency > RAL_ALLOCATION_RESIDENCY_TRANSIENT
			|| facts->backendType < RAL_BACKEND_VULKAN || facts->backendType >= RAL_BACKEND_COUNT
			|| facts->placement < RAL_ALLOCATION_PLACEMENT_DEDICATED
			|| facts->placement > RAL_ALLOCATION_PLACEMENT_MANAGED
			|| request->size == 0u || !PowerOfTwo(request->alignment)
			|| !PowerOfTwo(facts->actualAlignment)
			|| facts->actualAlignment < request->alignment
			|| request->size > UINT64_MAX - (request->alignment - 1u)
			|| request->ownerIdentity == (uintptr_t)0
			|| request->ownerGeneration == 0u || request->ownerGeneration == UINT64_MAX
			|| facts->allocationGeneration == 0u || facts->allocationGeneration == UINT64_MAX
			|| !BoolValid(request->allowFallback) || !BoolValid(facts->deviceLocal)
			|| !BoolValid(facts->hostVisible) || !BoolValid(facts->hostCoherent)
			|| !BoolValid(facts->lazy) || !BoolValid(facts->budgetKnown)
			|| (facts->hostCoherent && !facts->hostVisible) ) return qfalse;
	roundedSize = (request->size + request->alignment - 1u) & ~(request->alignment - 1u);
	if ( facts->committedSize < roundedSize ) return qfalse;
	if ( facts->budgetKnown ) {
		if ( facts->budgetBytes == 0u || facts->usedBytesBefore > facts->budgetBytes
				|| facts->committedSize > facts->budgetBytes - facts->usedBytesBefore ) return qfalse;
	} else if ( facts->budgetBytes != 0u || facts->usedBytesBefore != 0u ) return qfalse;
	if ( request->memoryClass == RAL_ALLOCATION_DEVICE_LOCAL && !facts->deviceLocal ) return qfalse;
	if ( (request->memoryClass == RAL_ALLOCATION_UPLOAD
			|| request->memoryClass == RAL_ALLOCATION_READBACK) && !facts->hostVisible ) return qfalse;
	if ( request->memoryClass == RAL_ALLOCATION_TRANSIENT ) {
		if ( request->residency != RAL_ALLOCATION_RESIDENCY_TRANSIENT ) return qfalse;
		if ( !facts->lazy ) emulated = qtrue;
	} else if ( request->residency == RAL_ALLOCATION_RESIDENCY_TRANSIENT ) return qfalse;
	if ( facts->placement == RAL_ALLOCATION_PLACEMENT_MANAGED ) emulated = qtrue;
	if ( emulated && !request->allowFallback ) return qfalse;
	memset(&candidate,0,sizeof(candidate));
	candidate.schemaVersion = RAL_ALLOCATION_SCHEMA_VERSION;
	candidate.backendType = facts->backendType;
	candidate.memoryClass = request->memoryClass;
	candidate.residency = request->residency;
	candidate.placement = facts->placement;
	candidate.outcome = emulated ? RAL_ALLOCATION_OUTCOME_EMULATED : RAL_ALLOCATION_OUTCOME_NATIVE;
	candidate.requestedSize = request->size;
	candidate.committedSize = facts->committedSize;
	candidate.alignment = facts->actualAlignment;
	candidate.ownerIdentity = request->ownerIdentity;
	candidate.ownerGeneration = request->ownerGeneration;
	candidate.allocationGeneration = facts->allocationGeneration;
	candidate.budgetKnown = facts->budgetKnown;
	candidate.budgetBytes = facts->budgetBytes;
	candidate.usedBytesBefore = facts->usedBytesBefore;
	if ( facts->budgetKnown ) {
		candidate.usedBytesAfter = facts->usedBytesBefore + facts->committedSize;
		candidate.pressureAfter = PressureAfter(candidate.usedBytesAfter,facts->budgetBytes);
	} else candidate.pressureAfter = RAL_ALLOCATION_PRESSURE_UNKNOWN;
	candidate.ready = qtrue;
	*out = candidate;
	return qtrue;
}

static qboolean ReceiptValid( const ralAllocationReceipt_t *receipt ) {
	uint64_t roundedSize;
	if ( !receipt || receipt->schemaVersion != RAL_ALLOCATION_SCHEMA_VERSION
			|| receipt->backendType < RAL_BACKEND_VULKAN || receipt->backendType >= RAL_BACKEND_COUNT
			|| receipt->memoryClass < RAL_ALLOCATION_DEVICE_LOCAL
			|| receipt->memoryClass > RAL_ALLOCATION_TRANSIENT
			|| receipt->residency < RAL_ALLOCATION_RESIDENCY_PERMANENT
			|| receipt->residency > RAL_ALLOCATION_RESIDENCY_TRANSIENT
			|| receipt->placement < RAL_ALLOCATION_PLACEMENT_DEDICATED
			|| receipt->placement > RAL_ALLOCATION_PLACEMENT_MANAGED
			|| receipt->outcome < RAL_ALLOCATION_OUTCOME_NATIVE
			|| receipt->outcome > RAL_ALLOCATION_OUTCOME_EMULATED
			|| receipt->pressureAfter < RAL_ALLOCATION_PRESSURE_UNKNOWN
			|| receipt->pressureAfter > RAL_ALLOCATION_PRESSURE_CRITICAL
			|| receipt->requestedSize == 0u || receipt->committedSize < receipt->requestedSize
			|| !PowerOfTwo(receipt->alignment)
			|| receipt->requestedSize > UINT64_MAX - (receipt->alignment - 1u)
			|| receipt->ownerIdentity == (uintptr_t)0
			|| receipt->ownerGeneration == 0u || receipt->ownerGeneration == UINT64_MAX
			|| receipt->allocationGeneration == 0u || receipt->allocationGeneration == UINT64_MAX
			|| !BoolValid(receipt->budgetKnown) || receipt->ready != qtrue ) return qfalse;
	roundedSize = (receipt->requestedSize + receipt->alignment - 1u)
		& ~(receipt->alignment - 1u);
	if ( receipt->committedSize < roundedSize ) return qfalse;
	if ( receipt->budgetKnown ) {
		if ( receipt->budgetBytes == 0u || receipt->usedBytesBefore > receipt->usedBytesAfter
				|| receipt->usedBytesAfter > receipt->budgetBytes
				|| receipt->usedBytesAfter - receipt->usedBytesBefore != receipt->committedSize
				|| receipt->pressureAfter != PressureAfter(receipt->usedBytesAfter,receipt->budgetBytes) ) return qfalse;
	} else if ( receipt->budgetBytes != 0u || receipt->usedBytesBefore != 0u
			|| receipt->usedBytesAfter != 0u || receipt->pressureAfter != RAL_ALLOCATION_PRESSURE_UNKNOWN ) return qfalse;
	if ( receipt->placement == RAL_ALLOCATION_PLACEMENT_MANAGED
			&& receipt->outcome != RAL_ALLOCATION_OUTCOME_EMULATED ) return qfalse;
	if ( receipt->memoryClass == RAL_ALLOCATION_TRANSIENT
			&& receipt->residency != RAL_ALLOCATION_RESIDENCY_TRANSIENT ) return qfalse;
	return qtrue;
}

qboolean Ral_AllocationReceiptExact( const ralAllocationReceipt_t *a,
		const ralAllocationReceipt_t *b ) {
	return ReceiptValid(a) && ReceiptValid(b)
		&& a->backendType == b->backendType && a->memoryClass == b->memoryClass
		&& a->residency == b->residency && a->placement == b->placement
		&& a->outcome == b->outcome && a->pressureAfter == b->pressureAfter
		&& a->requestedSize == b->requestedSize && a->committedSize == b->committedSize
		&& a->alignment == b->alignment && a->ownerIdentity == b->ownerIdentity
		&& a->ownerGeneration == b->ownerGeneration
		&& a->allocationGeneration == b->allocationGeneration
		&& a->budgetKnown == b->budgetKnown && a->budgetBytes == b->budgetBytes
		&& a->usedBytesBefore == b->usedBytesBefore && a->usedBytesAfter == b->usedBytesAfter;
}
