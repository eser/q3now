// SPDX-License-Identifier: GPL-3.0-or-later

#include "tr_temporal_batch_request.h"

#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL batch request line %d: %s\n", __LINE__, #x); return 1; } } while (0)

static temporalBatchRequest_t Candidate( uint64_t token, uint8_t enabled ) {
	temporalBatchRequest_t r;
	memset( &r, 0, sizeof( r ) );
	r.state = TEMPORAL_BATCH_REQUEST_EXACT;
	r.token = token;
	r.worldIndex = 2;
	r.frameId = 99;
	r.width = 1920;
	r.height = 1080;
	r.topologyEpoch = 7;
	r.planGeneration = 8;
	r.enabled = enabled;
	return r;
}

int main( void ) {
	temporalBatchRequest_t request, candidate, before, out, outBefore;
	temporalBatchRequest_t mutated;
	memset( &request, 0xa5, sizeof( request ) );
	R_TemporalBatchRequestReset( &request, 41 );
	CHECK( request.state == TEMPORAL_BATCH_REQUEST_NONE && request.token == 41 );
	candidate = Candidate( 41, 1 );
	CHECK( R_TemporalBatchRequestPublish( &request, &candidate ) );
	CHECK( request.state == TEMPORAL_BATCH_REQUEST_EXACT );
	before = request;
	CHECK( R_TemporalBatchRequestPublish( &request, &candidate ) );
	CHECK( memcmp( &request, &before, sizeof( request ) ) == 0 );
	CHECK( !R_TemporalBatchRequestConsume( &request, &request ) );
	CHECK( memcmp( &request, &before, sizeof( request ) ) == 0 );

	// Disabled is an authored exact tuple, not inferred from a cvar. Changing it
	// (or any identity field) conflicts sticky for this batch.
	candidate.enabled = 0;
	CHECK( !R_TemporalBatchRequestPublish( &request, &candidate ) );
	CHECK( request.state == TEMPORAL_BATCH_REQUEST_CONFLICT );
	before = request;
	candidate = Candidate( 41, 1 );
	CHECK( !R_TemporalBatchRequestPublish( &request, &candidate ) );
	CHECK( memcmp( &request, &before, sizeof( request ) ) == 0 );
	memset( &out, 0x5a, sizeof( out ) );
	CHECK( R_TemporalBatchRequestConsume( &request, &out ) );
	CHECK( request.state == TEMPORAL_BATCH_REQUEST_CONSUMED
		&& out.state == TEMPORAL_BATCH_REQUEST_CONFLICT && out.token == 41 );

	R_TemporalBatchRequestReset( &request, 42 );
	candidate = Candidate( 42, 0 );
	CHECK( R_TemporalBatchRequestPublish( &request, &candidate ) );
	memset( &out, 0x5a, sizeof( out ) );
	CHECK( R_TemporalBatchRequestConsume( &request, &out ) );
	CHECK( request.state == TEMPORAL_BATCH_REQUEST_CONSUMED );
	CHECK( out.state == TEMPORAL_BATCH_REQUEST_EXACT && out.enabled == 0
		&& out.token == 42 && out.worldIndex == 2 && out.frameId == 99 );
	outBefore = out; before = request;
	CHECK( !R_TemporalBatchRequestConsume( &request, &out ) );
	CHECK( memcmp( &request, &before, sizeof( request ) ) == 0
		&& memcmp( &out, &outBefore, sizeof( out ) ) == 0 );
	CHECK( !R_TemporalBatchRequestPublish( &request, &candidate ) );

	R_TemporalBatchRequestReset( &request, 43 );
	CHECK( R_TemporalBatchRequestConsume( &request, &out ) );
	CHECK( out.state == TEMPORAL_BATCH_REQUEST_NONE && out.token == 43 );
	before = request; outBefore = out;
	R_TemporalBatchRequestReset( &request, 0 );
	CHECK( memcmp( &request, &before, sizeof( request ) ) == 0 );
	candidate = Candidate( 44, 1 ); candidate.frameId = 0;
	R_TemporalBatchRequestReset( &request, 44 ); before = request;
	CHECK( !R_TemporalBatchRequestPublish( &request, &candidate ) );
	CHECK( memcmp( &request, &before, sizeof( request ) ) == 0 );

	// Every authored tuple field participates in exact validation/conflict.
	candidate = Candidate( 50, 1 );
	R_TemporalBatchRequestReset( &request, 50 );
	CHECK( R_TemporalBatchRequestPublish( &request, &candidate ) );
#define MUTATE_AND_CONFLICT(field, value) do { \
	R_TemporalBatchRequestReset( &request, 50 ); \
	CHECK( R_TemporalBatchRequestPublish( &request, &candidate ) ); \
	mutated = candidate; mutated.field = (value); \
	CHECK( !R_TemporalBatchRequestPublish( &request, &mutated ) ); \
	CHECK( request.state == TEMPORAL_BATCH_REQUEST_CONFLICT ); \
} while (0)
	MUTATE_AND_CONFLICT( worldIndex, 3 );
	MUTATE_AND_CONFLICT( frameId, 100 );
	MUTATE_AND_CONFLICT( width, 1280 );
	MUTATE_AND_CONFLICT( height, 720 );
	MUTATE_AND_CONFLICT( topologyEpoch, 9 );
	MUTATE_AND_CONFLICT( planGeneration, 10 );
	MUTATE_AND_CONFLICT( enabled, 0 );
#undef MUTATE_AND_CONFLICT
	R_TemporalBatchRequestReset( &request, 51 ); before = request;
	mutated = Candidate( 50, 1 );
	CHECK( !R_TemporalBatchRequestPublish( &request, &mutated ) );
	CHECK( memcmp( &request, &before, sizeof( request ) ) == 0 );
	mutated = Candidate( 51, 1 ); mutated.state = TEMPORAL_BATCH_REQUEST_NONE;
	CHECK( !R_TemporalBatchRequestPublish( &request, &mutated ) );
	R_TemporalBatchRequestReset( &request, UINT64_MAX );
	CHECK( request.token == UINT64_MAX && request.state == TEMPORAL_BATCH_REQUEST_NONE );
	CHECK( !R_TemporalBatchRequestConsume( NULL, &out ) );
	CHECK( !R_TemporalBatchRequestConsume( &request, NULL ) );
	CHECK( memcmp( &out, &outBefore, sizeof( out ) ) == 0 );
	puts( "temporal batch request contract: PASS" );
	return 0;
}
