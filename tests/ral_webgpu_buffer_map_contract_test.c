// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "ral_buffer_map.h"

#include <stdio.h>
#include <string.h>

#define CHECK(expr) do { if ( !(expr) ) { \
	fprintf( stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr ); return 1; \
} } while ( 0 )

typedef struct {
	ralBufferMapLifecycle_t lifecycle;
	const ralBuffer_t *buffer;
	uint64_t size;
	qboolean deviceLost;
	unsigned char bytes[64];
} MockWebGpuMap;

static ralResult_t MockBegin( MockWebGpuMap *mock, const ralBufferMapRequest_t *request,
	                          ralBufferMapTicket_t *outTicket ) {
	if ( !mock || mock->deviceLost ) return ralErrorDeviceLost;
	return Ral_BufferMapLifecyclePublishBegin( &mock->lifecycle, mock->buffer,
		mock->size, request, qfalse, NULL, outTicket );
}

static ralResult_t MockComplete( MockWebGpuMap *mock,
	                             const ralBufferMapTicket_t *pending,
	                             ralBufferMapTicket_t *outReady ) {
	if ( !mock || mock->deviceLost ) return ralErrorDeviceLost;
	return Ral_BufferMapLifecyclePublishReady( &mock->lifecycle, pending,
		mock->bytes + pending->request.offset, outReady );
}

static ralResult_t MockPoll( MockWebGpuMap *mock,
	                         const ralBufferMapTicket_t *authority,
	                         ralBufferMapTicket_t *outTicket ) {
	if ( !mock ) return ralErrorInvalidArgument;
	if ( mock->deviceLost ) {
		if ( mock->lifecycle.status == RAL_BUFFER_MAP_PENDING )
			(void)Ral_BufferMapLifecycleCancel( &mock->lifecycle, authority );
		return ralErrorDeviceLost;
	}
	return Ral_BufferMapLifecyclePoll( &mock->lifecycle, authority, outTicket );
}

int main( void ) {
	MockWebGpuMap mock;
	ralBufferMapRequest_t request = { RAL_MAP_WRITE, 8, 16 };
	ralBufferMapTicket_t pending, polled, ready, stale;

	memset( &mock, 0, sizeof( mock ) );
	mock.buffer = (const ralBuffer_t *)(uintptr_t)0x700u;
	mock.size = sizeof( mock.bytes );
	Ral_BufferMapLifecycleInit( &mock.lifecycle );

	CHECK( MockBegin( &mock, &request, &pending ) == ralSuccess );
	CHECK( pending.status == RAL_BUFFER_MAP_PENDING && pending.mappedRange == NULL );
	CHECK( !Ral_BufferMapLifecycleGpuUseAllowed( &mock.lifecycle ) );
	CHECK( MockPoll( &mock, &pending, &polled ) == ralSuccess );
	CHECK( Ral_BufferMapTicketExact( &pending, &polled ) );
	CHECK( MockComplete( &mock, &pending, &ready ) == ralSuccess );
	CHECK( ready.status == RAL_BUFFER_MAP_READY && ready.mappedRange == mock.bytes + 8 );
	CHECK( MockPoll( &mock, &pending, &polled ) == ralSuccess );
	CHECK( Ral_BufferMapTicketExact( &ready, &polled ) );
	CHECK( Ral_BufferMapLifecycleUnmap( &mock.lifecycle, &ready ) == ralSuccess );
	CHECK( Ral_BufferMapLifecycleGpuUseAllowed( &mock.lifecycle ) );

	// Cancellation is exact and prevents a late completion from publishing.
	CHECK( MockBegin( &mock, &request, &pending ) == ralSuccess );
	stale = pending; stale.generation++;
	CHECK( Ral_BufferMapLifecycleCancel( &mock.lifecycle, &stale ) == ralErrorInvalidArgument );
	CHECK( Ral_BufferMapLifecycleCancel( &mock.lifecycle, &pending ) == ralSuccess );
	CHECK( MockComplete( &mock, &pending, &ready ) == ralErrorInvalidArgument );

	// Device loss resolves pending authority with an error and releases GPU exclusion.
	CHECK( MockBegin( &mock, &request, &pending ) == ralSuccess );
	mock.deviceLost = qtrue;
	CHECK( MockPoll( &mock, &pending, &polled ) == ralErrorDeviceLost );
	CHECK( Ral_BufferMapLifecycleGpuUseAllowed( &mock.lifecycle ) );
	CHECK( MockComplete( &mock, &pending, &ready ) == ralErrorDeviceLost );
	CHECK( MockBegin( &mock, &request, &pending ) == ralErrorDeviceLost );

	puts( "PASS WebGPU-shaped asynchronous typed buffer-map contract" );
	return 0;
}
