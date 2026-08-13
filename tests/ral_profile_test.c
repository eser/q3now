#include "ral_profile.h"

#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if ( !(x) ) { fprintf( stderr, "FAIL ral profile line %d: %s\n", __LINE__, #x ); return 1; } } while ( 0 )

int main( void ) {
	ralProfileAccumulator_t acc, before;
	ralProfileSnapshot_t snapshot, snapshotBefore;
	double history[ RAL_PROFILE_HISTORY_CAPACITY ][ RAL_PROFILE_MAX_LANES ];
	const char *baseLabels[] = { "world", "present" };
	const char *changedLabels[] = { "world", "smaa" };
	const char *oneLabel[] = { "present" };
	const char *badLabels[] = { "" };
	double first[] = { 1.25, 0.50 };
	double second[] = { 0.75, 1.50 };
	double one[] = { 2.0 };
	double negative[] = { -1.0 };

	Ral_ProfileAccumulatorInit( &acc );
	CHECK( Ral_ProfileAccumulatorAdd( &acc, baseLabels, first, 2 ) == 0 );
	CHECK( acc.topologyEpoch == 1 && acc.sampleFrames == 1 && acc.laneCount == 2 );
	CHECK( acc.totalMs[0] == 1.25 && acc.totalMs[1] == 0.50 );
	CHECK( Ral_ProfileAccumulatorAdd( &acc, baseLabels, second, 2 ) == 0 );
	CHECK( acc.sampleFrames == 2 && acc.totalMs[0] == 2.0 && acc.totalMs[1] == 2.0 );
	memset( &snapshot, 0, sizeof( snapshot ) );
	CHECK( Ral_ProfileAccumulatorSnapshot( &acc, &snapshot ) == 1 );
	CHECK( snapshot.topologyEpoch == 1 && snapshot.laneCount == 2 && snapshot.sampleCount == 2 );
	CHECK( strcmp( snapshot.labels[0], "world" ) == 0 );
	CHECK( snapshot.latestMs[0] == 0.75 && snapshot.averageMs[0] == 1.0 );
	CHECK( snapshot.minimumMs[0] == 0.75 && snapshot.maximumMs[0] == 1.25 );

	Ral_ProfileAccumulatorClearSamples( &acc );
	CHECK( acc.topologyEpoch == 1 && acc.laneCount == 2 && acc.sampleFrames == 0 );
	CHECK( Ral_ProfileAccumulatorSnapshot( &acc, &snapshot ) == 1 && snapshot.sampleCount == 2 );
	CHECK( Ral_ProfileAccumulatorAdd( &acc, baseLabels, first, 2 ) == 0 );
	CHECK( acc.topologyEpoch == 1 && acc.sampleFrames == 1 );

	CHECK( Ral_ProfileAccumulatorAdd( &acc, changedLabels, second, 2 ) == 1 );
	CHECK( acc.topologyEpoch == 2 && acc.sampleFrames == 1 && acc.laneCount == 2 );
	CHECK( strcmp( acc.labels[1], "smaa" ) == 0 && acc.totalMs[0] == 0.75 );
	CHECK( Ral_ProfileAccumulatorSnapshot( &acc, &snapshot ) == 1 );
	CHECK( snapshot.topologyEpoch == 2 && snapshot.sampleCount == 1 );
	CHECK( Ral_ProfileAccumulatorAdd( &acc, oneLabel, one, 1 ) == 1 );
	CHECK( acc.topologyEpoch == 3 && acc.sampleFrames == 1 && acc.laneCount == 1 );
	for ( int i = 0; i < RAL_PROFILE_HISTORY_CAPACITY + 5; ++i ) {
		double sample = (double)i;
		CHECK( Ral_ProfileAccumulatorAdd( &acc, oneLabel, &sample, 1 ) == 0 );
	}
	CHECK( Ral_ProfileAccumulatorSnapshot( &acc, &snapshot ) == 1 );
	CHECK( snapshot.sampleCount == RAL_PROFILE_HISTORY_CAPACITY );
	CHECK( snapshot.latestMs[0] == RAL_PROFILE_HISTORY_CAPACITY + 4 );
	CHECK( snapshot.minimumMs[0] == 5.0 );
	CHECK( snapshot.maximumMs[0] == RAL_PROFILE_HISTORY_CAPACITY + 4 );
	memset( history, 0, sizeof( history ) );
	CHECK( Ral_ProfileAccumulatorCopyHistory( &acc, &history[0][0],
		RAL_PROFILE_HISTORY_CAPACITY ) == RAL_PROFILE_HISTORY_CAPACITY );
	CHECK( history[0][0] == 5.0 );
	CHECK( history[ RAL_PROFILE_HISTORY_CAPACITY - 1 ][0]
		== RAL_PROFILE_HISTORY_CAPACITY + 4 );
	memset( history, 0x5a, sizeof( history ) );
	CHECK( Ral_ProfileAccumulatorCopyHistory( &acc, &history[0][0], 3 ) == 3 );
	CHECK( history[0][0] == RAL_PROFILE_HISTORY_CAPACITY + 2 );
	CHECK( history[2][0] == RAL_PROFILE_HISTORY_CAPACITY + 4 );

	before = acc;
	CHECK( Ral_ProfileAccumulatorAdd( &acc, badLabels, one, 1 ) == -1 );
	CHECK( memcmp( &before, &acc, sizeof( acc ) ) == 0 );
	CHECK( Ral_ProfileAccumulatorAdd( &acc, oneLabel, negative, 1 ) == -1 );
	CHECK( memcmp( &before, &acc, sizeof( acc ) ) == 0 );
	memset( &snapshotBefore, 0x5a, sizeof( snapshotBefore ) );
	snapshot = snapshotBefore;
	Ral_ProfileAccumulatorInit( &acc );
	CHECK( Ral_ProfileAccumulatorSnapshot( &acc, &snapshot ) == 0 );
	CHECK( memcmp( &snapshot, &snapshotBefore, sizeof( snapshot ) ) == 0 );
	memset( history, 0x5a, sizeof( history ) );
	CHECK( Ral_ProfileAccumulatorCopyHistory( &acc, &history[0][0], 1 ) == 0 );
	CHECK( ((unsigned char *)history)[0] == 0x5a );

	puts( "PASS RAL semantic profile accumulator" );
	return 0;
}
