// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "ral_buffer_map.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

#define CHECK(expr) do { if ( !(expr) ) { \
	fprintf( stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr ); return 1; \
} } while ( 0 )

int main( void ) {
	ralBufferMapLifecycle_t lifecycle;
	ralBufferMapRequest_t request = { RAL_MAP_WRITE, 16, 32 };
	ralBufferMapTicket_t pending, ready, saved, output, unchanged;
	const ralBuffer_t *buffer = (const ralBuffer_t *)(uintptr_t)0x100u;
	void *mapped = (void *)(uintptr_t)0x200u;

	Ral_BufferMapLifecycleInit( &lifecycle );
	CHECK( Ral_BufferMapLifecycleGpuUseAllowed( &lifecycle ) );
	CHECK( Ral_BufferMapRequestValid( &request, 64 ) );
	request.mode = (ralBufferMapMode_t)0;
	CHECK( !Ral_BufferMapRequestValid( &request, 64 ) );
	request.mode = RAL_MAP_WRITE;
	request.size = 0;
	CHECK( !Ral_BufferMapRequestValid( &request, 64 ) );
	request.size = 32;
	request.offset = UINT64_MAX - 15u;
	CHECK( !Ral_BufferMapRequestValid( &request, UINT64_MAX ) );
	request.offset = 16;

	memset( &output, 0x5a, sizeof( output ) );
	saved = output;
	CHECK( Ral_BufferMapLifecyclePublishBegin( &lifecycle, buffer, 64,
		&request, qfalse, NULL, &pending ) == ralSuccess );
	CHECK( pending.status == RAL_BUFFER_MAP_PENDING && pending.generation == 1 );
	CHECK( Ral_BufferMapTicketValid( &pending ) );
	CHECK( Ral_BufferMapLifecyclePoll( &lifecycle, &pending, &output ) == ralSuccess );
	CHECK( Ral_BufferMapTicketExact( &pending, &output ) );
	CHECK( !Ral_BufferMapLifecycleGpuUseAllowed( &lifecycle ) );
	saved = output;
	CHECK( Ral_BufferMapLifecyclePublishBegin( &lifecycle, buffer, 64,
		&request, qfalse, NULL, &output ) == ralErrorInvalidArgument );
	CHECK( memcmp( &output, &saved, sizeof( output ) ) == 0 );

	CHECK( Ral_BufferMapLifecyclePublishReady( &lifecycle, &pending, mapped, &ready ) == ralSuccess );
	CHECK( ready.status == RAL_BUFFER_MAP_READY && ready.mappedRange == mapped );
	CHECK( Ral_BufferMapLifecyclePoll( &lifecycle, &pending, &output ) == ralSuccess );
	CHECK( Ral_BufferMapTicketExact( &ready, &output ) );
	output = pending; output.generation++;
	memset( &saved, 0x3c, sizeof( saved ) );
	unchanged = saved;
	CHECK( Ral_BufferMapLifecyclePoll( &lifecycle, &output, &saved ) == ralErrorInvalidArgument );
	CHECK( memcmp( &saved, &unchanged, sizeof( saved ) ) == 0 );
	CHECK( !Ral_BufferMapTicketExact( &pending, &ready ) );
	CHECK( Ral_BufferMapLifecycleCancel( &lifecycle, &ready ) == ralErrorInvalidArgument );
	CHECK( Ral_BufferMapLifecycleUnmap( &lifecycle, &pending ) == ralErrorInvalidArgument );
	output = ready; output.bufferIdentity = (const ralBuffer_t *)(uintptr_t)0x101u;
	CHECK( Ral_BufferMapLifecycleUnmap( &lifecycle, &output ) == ralErrorInvalidArgument );
	CHECK( Ral_BufferMapLifecycleUnmap( &lifecycle, &ready ) == ralSuccess );
	CHECK( Ral_BufferMapLifecycleGpuUseAllowed( &lifecycle ) );

	// Vulkan-shaped immediate completion publishes READY in one transaction.
	request.mode = RAL_MAP_READ;
	CHECK( Ral_BufferMapLifecyclePublishBegin( &lifecycle, buffer, 64,
		&request, qtrue, mapped, &ready ) == ralSuccess );
	CHECK( ready.generation == 2 && ready.status == RAL_BUFFER_MAP_READY );
	CHECK( Ral_BufferMapLifecycleUnmap( &lifecycle, &ready ) == ralSuccess );

	// WebGPU-shaped pending mapping can be cancelled without publishing a range.
	CHECK( Ral_BufferMapLifecyclePublishBegin( &lifecycle, buffer, 64,
		&request, qfalse, NULL, &pending ) == ralSuccess );
	CHECK( pending.generation == 3 );
	CHECK( Ral_BufferMapLifecycleCancel( &lifecycle, &pending ) == ralSuccess );
	CHECK( Ral_BufferMapLifecycleGpuUseAllowed( &lifecycle ) );
	lifecycle.bufferIdentity = buffer;
	CHECK( !Ral_BufferMapLifecycleGpuUseAllowed( &lifecycle ) );
	CHECK( Ral_BufferMapLifecyclePublishBegin( &lifecycle, buffer, 64,
		&request, qfalse, NULL, &output ) == ralErrorInvalidArgument );
	lifecycle.bufferIdentity = NULL;

	// Every ticket field participates in exact authority.
	saved = ready;
	output = saved; output.bufferIdentity = (const ralBuffer_t *)(uintptr_t)0x101u;
	CHECK( !Ral_BufferMapTicketExact( &saved, &output ) );
	output = saved; output.generation++;
	CHECK( !Ral_BufferMapTicketExact( &saved, &output ) );
	output = saved; output.request.mode = RAL_MAP_WRITE;
	CHECK( !Ral_BufferMapTicketExact( &saved, &output ) );
	output = saved; output.request.offset++;
	CHECK( !Ral_BufferMapTicketExact( &saved, &output ) );
	output = saved; output.request.size++;
	CHECK( !Ral_BufferMapTicketExact( &saved, &output ) );
	output = saved; output.status = RAL_BUFFER_MAP_PENDING; output.mappedRange = NULL;
	CHECK( !Ral_BufferMapTicketExact( &saved, &output ) );
	output = saved; output.mappedRange = (void *)(uintptr_t)0x201u;
	CHECK( !Ral_BufferMapTicketExact( &saved, &output ) );

	// Generation saturation and ready/range mismatches reject output-atomically.
	Ral_BufferMapLifecycleInit( &lifecycle );
	lifecycle.generation = UINT64_MAX - 1u;
	memset( &output, 0xa5, sizeof( output ) );
	saved = output;
	CHECK( Ral_BufferMapLifecyclePublishBegin( &lifecycle, buffer, 64,
		&request, qtrue, mapped, &output ) == ralErrorInvalidArgument );
	CHECK( memcmp( &output, &saved, sizeof( output ) ) == 0 );
	CHECK( lifecycle.status == RAL_BUFFER_MAP_IDLE );
	Ral_BufferMapLifecycleInit( &lifecycle );
	CHECK( Ral_BufferMapLifecyclePublishBegin( &lifecycle, buffer, 64,
		&request, qtrue, NULL, &output ) == ralErrorInvalidArgument );
	CHECK( Ral_BufferMapLifecyclePublishBegin( &lifecycle, buffer, 64,
		&request, qfalse, mapped, &output ) == ralErrorInvalidArgument );

	puts( "PASS backend-neutral typed buffer-map lifecycle" );
	return 0;
}
