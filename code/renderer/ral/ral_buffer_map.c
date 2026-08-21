// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "ral_buffer_map.h"

#include <limits.h>
#include <string.h>

static qboolean ralBufferMapModeValid( ralBufferMapMode_t mode ) {
	return mode == RAL_MAP_READ || mode == RAL_MAP_WRITE;
}

qboolean Ral_BufferMapRequestValid( const ralBufferMapRequest_t *request,
	                                 uint64_t bufferSize ) {
	if ( !request || !ralBufferMapModeValid( request->mode ) || request->size == 0 )
		return qfalse;
	if ( request->offset > bufferSize || request->size > bufferSize - request->offset )
		return qfalse;
	return qtrue;
}

qboolean Ral_BufferMapTicketValid( const ralBufferMapTicket_t *ticket ) {
	if ( !ticket || !ticket->bufferIdentity
	  || ticket->generation == 0 || ticket->generation == UINT64_MAX
	  || !ralBufferMapModeValid( ticket->request.mode ) || ticket->request.size == 0 )
		return qfalse;
	if ( ticket->request.offset > UINT64_MAX - ticket->request.size )
		return qfalse;
	if ( ticket->status == RAL_BUFFER_MAP_PENDING )
		return ticket->mappedRange == NULL;
	if ( ticket->status == RAL_BUFFER_MAP_READY )
		return ticket->mappedRange != NULL;
	return qfalse;
}

qboolean Ral_BufferMapTicketExact( const ralBufferMapTicket_t *a,
	                                const ralBufferMapTicket_t *b ) {
	return Ral_BufferMapTicketValid( a ) && Ral_BufferMapTicketValid( b )
		&& a->bufferIdentity == b->bufferIdentity
		&& a->generation == b->generation
		&& a->request.mode == b->request.mode
		&& a->request.offset == b->request.offset
		&& a->request.size == b->request.size
		&& a->status == b->status
		&& a->mappedRange == b->mappedRange;
}

void Ral_BufferMapLifecycleInit( ralBufferMapLifecycle_t *lifecycle ) {
	if ( lifecycle ) memset( lifecycle, 0, sizeof( *lifecycle ) );
}

qboolean Ral_BufferMapLifecycleGpuUseAllowed( const ralBufferMapLifecycle_t *lifecycle ) {
	return lifecycle && lifecycle->status == RAL_BUFFER_MAP_IDLE
		&& lifecycle->bufferIdentity == NULL
		&& lifecycle->request.mode == 0
		&& lifecycle->request.offset == 0
		&& lifecycle->request.size == 0
		&& lifecycle->mappedRange == NULL;
}

static void ralBufferMapMakeTicket( const ralBufferMapLifecycle_t *lifecycle,
	                                const ralBuffer_t *buffer,
	                                ralBufferMapTicket_t *ticket ) {
	memset( ticket, 0, sizeof( *ticket ) );
	ticket->bufferIdentity = buffer;
	ticket->generation = lifecycle->generation;
	ticket->request = lifecycle->request;
	ticket->status = lifecycle->status;
	ticket->mappedRange = lifecycle->mappedRange;
}

static qboolean ralBufferMapLifecycleMatches( const ralBufferMapLifecycle_t *lifecycle,
	                                          const ralBufferMapTicket_t *ticket ) {
	return lifecycle && Ral_BufferMapTicketValid( ticket )
		&& lifecycle->bufferIdentity == ticket->bufferIdentity
		&& lifecycle->generation == ticket->generation
		&& lifecycle->request.mode == ticket->request.mode
		&& lifecycle->request.offset == ticket->request.offset
		&& lifecycle->request.size == ticket->request.size
		&& lifecycle->status == ticket->status
		&& lifecycle->mappedRange == ticket->mappedRange;
}

static qboolean ralBufferMapAuthorityMatches( const ralBufferMapLifecycle_t *lifecycle,
	                                          const ralBufferMapTicket_t *authority ) {
	if ( !lifecycle || !Ral_BufferMapTicketValid( authority )
	  || lifecycle->status == RAL_BUFFER_MAP_IDLE
	  || lifecycle->bufferIdentity != authority->bufferIdentity
	  || lifecycle->generation != authority->generation
	  || lifecycle->request.mode != authority->request.mode
	  || lifecycle->request.offset != authority->request.offset
	  || lifecycle->request.size != authority->request.size )
		return qfalse;
	if ( authority->status == RAL_BUFFER_MAP_PENDING )
		return qtrue;
	return lifecycle->status == RAL_BUFFER_MAP_READY
		&& lifecycle->mappedRange == authority->mappedRange;
}

ralResult_t Ral_BufferMapLifecyclePublishBegin( ralBufferMapLifecycle_t *lifecycle,
	                                             const ralBuffer_t *buffer,
	                                             uint64_t bufferSize,
	                                             const ralBufferMapRequest_t *request,
	                                             qboolean ready,
	                                             void *mappedRange,
	                                             ralBufferMapTicket_t *outTicket ) {
	ralBufferMapLifecycle_t candidate;
	ralBufferMapTicket_t ticket;
	if ( !lifecycle || !buffer || !outTicket
	  || !Ral_BufferMapLifecycleGpuUseAllowed( lifecycle )
	  || lifecycle->generation >= UINT64_MAX - 1u
	  || !Ral_BufferMapRequestValid( request, bufferSize )
	  || ( ready != qfalse && ready != qtrue )
	  || ( ready && !mappedRange ) || ( !ready && mappedRange ) )
		return ralErrorInvalidArgument;
	candidate = *lifecycle;
	candidate.bufferIdentity = buffer;
	candidate.generation++;
	candidate.request = *request;
	candidate.status = ready ? RAL_BUFFER_MAP_READY : RAL_BUFFER_MAP_PENDING;
	candidate.mappedRange = mappedRange;
	ralBufferMapMakeTicket( &candidate, buffer, &ticket );
	if ( !Ral_BufferMapTicketValid( &ticket ) )
		return ralErrorInvalidArgument;
	*lifecycle = candidate;
	*outTicket = ticket;
	return ralSuccess;
}

ralResult_t Ral_BufferMapLifecyclePublishReady( ralBufferMapLifecycle_t *lifecycle,
	                                             const ralBufferMapTicket_t *pendingTicket,
	                                             void *mappedRange,
	                                             ralBufferMapTicket_t *outTicket ) {
	ralBufferMapLifecycle_t candidate;
	ralBufferMapTicket_t ticket;
	if ( !lifecycle || !mappedRange || !outTicket
	  || !ralBufferMapLifecycleMatches( lifecycle, pendingTicket )
	  || pendingTicket->status != RAL_BUFFER_MAP_PENDING )
		return ralErrorInvalidArgument;
	candidate = *lifecycle;
	candidate.status = RAL_BUFFER_MAP_READY;
	candidate.mappedRange = mappedRange;
	ralBufferMapMakeTicket( &candidate, pendingTicket->bufferIdentity, &ticket );
	if ( !Ral_BufferMapTicketValid( &ticket ) )
		return ralErrorInvalidArgument;
	*lifecycle = candidate;
	*outTicket = ticket;
	return ralSuccess;
}

ralResult_t Ral_BufferMapLifecyclePoll( const ralBufferMapLifecycle_t *lifecycle,
	                                    const ralBufferMapTicket_t *authority,
	                                    ralBufferMapTicket_t *outTicket ) {
	ralBufferMapTicket_t ticket;
	if ( !outTicket || !ralBufferMapAuthorityMatches( lifecycle, authority ) )
		return ralErrorInvalidArgument;
	ralBufferMapMakeTicket( lifecycle, authority->bufferIdentity, &ticket );
	if ( !Ral_BufferMapTicketValid( &ticket ) )
		return ralErrorInvalidArgument;
	*outTicket = ticket;
	return ralSuccess;
}

static void ralBufferMapLifecycleClear( ralBufferMapLifecycle_t *lifecycle ) {
	lifecycle->bufferIdentity = NULL;
	memset( &lifecycle->request, 0, sizeof( lifecycle->request ) );
	lifecycle->status = RAL_BUFFER_MAP_IDLE;
	lifecycle->mappedRange = NULL;
}

ralResult_t Ral_BufferMapLifecycleUnmap( ralBufferMapLifecycle_t *lifecycle,
	                                     const ralBufferMapTicket_t *readyTicket ) {
	if ( !ralBufferMapLifecycleMatches( lifecycle, readyTicket )
	  || readyTicket->status != RAL_BUFFER_MAP_READY )
		return ralErrorInvalidArgument;
	ralBufferMapLifecycleClear( lifecycle );
	return ralSuccess;
}

ralResult_t Ral_BufferMapLifecycleCancel( ralBufferMapLifecycle_t *lifecycle,
	                                      const ralBufferMapTicket_t *pendingTicket ) {
	if ( !ralBufferMapLifecycleMatches( lifecycle, pendingTicket )
	  || pendingTicket->status != RAL_BUFFER_MAP_PENDING )
		return ralErrorInvalidArgument;
	ralBufferMapLifecycleClear( lifecycle );
	return ralSuccess;
}
