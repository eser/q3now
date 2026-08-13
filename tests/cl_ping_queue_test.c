// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
#include "cl_ping_queue.h"

#include <stdio.h>
#include <string.h>

static int failures;
#define CHECK(x) do { if ( !(x) ) { fprintf( stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #x ); failures++; } } while ( 0 )

static clPingQueueEntry_t entry( bool occupied, bool completed,
	uint32_t start, uint32_t timeout ) {
	clPingQueueEntry_t value = { occupied, completed, start, timeout };
	return value;
}

static void expect( const clPingQueueEntry_t *entries, size_t count, uint32_t now,
	size_t index, clPingQueueReuseReason_t reason, uint32_t age ) {
	clPingQueueSelection_t output = { 99, CL_PING_QUEUE_REUSE_OLDEST_PENDING, 99 };
	CHECK( CL_PingQueueSelect( entries, count, now, &output ) );
	CHECK( output.index == index );
	CHECK( output.reason == reason );
	CHECK( output.age == age );
}

int main( void ) {
	clPingQueueEntry_t entries[4];
	clPingQueueEntry_t described;
	clPingQueueEntry_t describedSentinel = { true, true, 17, 23 };
	clPingQueueSelection_t sentinel = { 17, CL_PING_QUEUE_REUSE_COMPLETED, 23 };
	clPingQueueSelection_t output;

	CHECK( CL_PingQueueDescribe( true, 10, 999, 0, &described ) );
	CHECK( described.occupied && !described.completed
		&& described.start == 10 && described.timeout == 999 );
	for ( int resultTime = 1; resultTime <= 998; resultTime =
		resultTime == 1 ? 499 : resultTime == 499 ? 500 : 998 ) {
		CHECK( CL_PingQueueDescribe( true, 10, 999, resultTime, &described ) );
		CHECK( described.occupied && described.completed
			&& described.start == 10 && described.timeout == 999 );
		if ( resultTime == 998 ) break;
	}
	described = describedSentinel;
	CHECK( !CL_PingQueueDescribe( true, 10, 999, -1, &described ) );
	CHECK( memcmp( &described, &describedSentinel, sizeof( described ) ) == 0 );
	CHECK( !CL_PingQueueDescribe( true, 10, 999, 1, NULL ) );

	/* A free slot is authoritative even when an expired request and a completed
	 * result are also available. */
	entries[0] = entry( true, false, 10, 20 );
	entries[1] = entry( true, true, 5, 100 );
	entries[2] = entry( false, false, 0, 0 );
	expect( entries, 3, 50, 2, CL_PING_QUEUE_REUSE_FREE, 0 );

	entries[0] = entry( true, true, 5, 999 );
	entries[1] = entry( true, false, 50, 50 );
	entries[2] = entry( true, false, 90, 100 );
	expect( entries, 3, 100, 1, CL_PING_QUEUE_REUSE_EXPIRED, 50 );
	entries[1] = entry( true, false, 40, 50 );
	expect( entries, 3, 100, 1, CL_PING_QUEUE_REUSE_EXPIRED, 60 );
	entries[0] = entry( true, false, 30, 50 );
	entries[1] = entry( true, false, 10, 50 );
	entries[2] = entry( true, true, 0, 999 );
	expect( entries, 3, 100, 1, CL_PING_QUEUE_REUSE_EXPIRED, 90 );
	entries[0] = entry( true, false, 10, 50 );
	entries[1] = entry( true, false, 10, 50 );
	expect( entries, 2, 100, 0, CL_PING_QUEUE_REUSE_EXPIRED, 90 );

	/* Completed is lifecycle state, not an RTT threshold. Both the historical
	 * 499ms case and a valid current 500..998ms result are reusable. */
	entries[0] = entry( true, true, 501, 999 );
	entries[1] = entry( true, true, 500, 999 );
	entries[2] = entry( true, false, 900, 999 );
	expect( entries, 3, 1000, 1, CL_PING_QUEUE_REUSE_COMPLETED, 500 );
	entries[0] = entry( true, true, 2, 999 );
	entries[1] = entry( true, false, 3, 999 );
	expect( entries, 2, 1000, 0, CL_PING_QUEUE_REUSE_COMPLETED, 998 );

	entries[0] = entry( true, false, 900, 999 );
	entries[1] = entry( true, false, 800, 999 );
	entries[2] = entry( true, false, 850, 999 );
	expect( entries, 3, 1000, 1, CL_PING_QUEUE_REUSE_OLDEST_PENDING, 200 );
	entries[0] = entry( true, false, 800, 999 );
	entries[1] = entry( true, false, 800, 999 );
	expect( entries, 2, 1000, 0, CL_PING_QUEUE_REUSE_OLDEST_PENDING, 200 );

	entries[0] = entry( true, false, UINT32_MAX - 5u, 10 );
	entries[1] = entry( true, false, 2, 10 );
	expect( entries, 2, 4, 0, CL_PING_QUEUE_REUSE_EXPIRED, 10 );

	output = sentinel;
	CHECK( !CL_PingQueueSelect( NULL, 2, 10, &output ) );
	CHECK( memcmp( &output, &sentinel, sizeof( output ) ) == 0 );
	output = sentinel;
	CHECK( !CL_PingQueueSelect( entries, 0, 10, &output ) );
	CHECK( memcmp( &output, &sentinel, sizeof( output ) ) == 0 );
	CHECK( !CL_PingQueueSelect( entries, 2, 10, NULL ) );

	return failures ? 1 : 0;
}
