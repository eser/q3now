// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

// Known-answer tests for tr_temporal_resolve_oracle (the CPU model of
// temporal_resolve.comp). Every case uses exact texel positions (no sub-texel
// filtering), so the expected values are hand-computable.

#include "tr_temporal_resolve_oracle.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { \
	fprintf(stderr, "CHECK failed at %s:%d: %s\n", __FILE__, __LINE__, #x); \
	return 1; } } while (0)

enum { W = 8, H = 8, N = W * H };

static uint16_t s_current[N * 4], s_previous[N * 4], s_velocity[N * 2];
static float s_depth[N], s_previousDepth[N];
static uint8_t s_validity[N];

static uint16_t FloatToHalf( float f ) {
	// exact for the values used here (multiples of 1/1024 in [0, 2048))
	uint32_t bits; int exponent; float mantissa;
	if ( f == 0.0f ) return 0u;
	mantissa = frexpf( f, &exponent ); // f = mantissa * 2^exponent, mantissa in [0.5, 1)
	bits = (uint32_t)( ( exponent - 1 + 15 ) << 10 ) | ( (uint32_t)lrintf( ( mantissa * 2.0f - 1.0f ) * 1024.0f ) & 0x3ffu );
	return (uint16_t)bits;
}

static void SetColor( uint16_t *plane, int x, int y, float r, float g, float b, float a ) {
	uint16_t *p = plane + ( y * W + x ) * 4;
	p[0] = FloatToHalf( r ); p[1] = FloatToHalf( g ); p[2] = FloatToHalf( b ); p[3] = FloatToHalf( a );
}

// zNear 1, zFar 10: device depth 0 is linear 10 (the history depth used below).
static void Scene( float current, float previous, float previousCount ) {
	int x, y;
	for ( y = 0; y < H; ++y ) for ( x = 0; x < W; ++x ) {
		// a checker keeps the 3x3 box wide ([0.25, 0.75] around 0.5) so a 0.5 history is not clamped
		float c = current >= 0.0f ? current : ( ( x + y ) & 1 ) ? 0.75f : 0.25f;
		SetColor( s_current, x, y, c, c, c, 1.0f );
		SetColor( s_previous, x, y, previous, previous, previous, previousCount );
	}
	memset( s_velocity, 0, sizeof( s_velocity ) );
	memset( s_depth, 0, sizeof( s_depth ) );
	for ( x = 0; x < N; ++x ) { s_previousDepth[x] = 10.0f; s_validity[x] = 255u; }
}

static void Setup( temporalResolveOraclePlanes_t *p, temporalResolveOracleParams_t *k, uint32_t flags ) {
	memset( p, 0, sizeof( *p ) ); memset( k, 0, sizeof( *k ) );
	p->width = W; p->height = H;
	p->currentColor = s_current; p->currentDepth = s_depth; p->previousColor = s_previous;
	p->previousLinearDepth = s_previousDepth; p->velocity = s_velocity; p->validity = s_validity;
	k->extent[0] = W; k->extent[1] = H; k->zNear = 1.0f; k->zFar = 10.0f;
	k->depthThresholdAbsolute = 0.125f; k->depthThresholdRelative = 0.02f;
	k->historyWeight = 0.875f; k->qualityFlags = flags; k->filterWeightError = 1.0f / 256.0f;
}

static int Near( float a, float b ) { return fabsf( a - b ) <= 1.0e-5f; }

int main( void ) {
	temporalResolveOraclePlanes_t p; temporalResolveOracleParams_t k;
	temporalResolveOracleResult_t r;
	const float current = 0.25f; // (4,4) is an even square of the checker
	float w;

	// 1. Static count progression: a history inside the box keeps its count n;
	//    the weight is n / (n + 1) and the written count n + 1.
	Scene( -1.0f, 0.5f, 3.0f );
	Setup( &p, &k, 8u );                   // no sharpening: displayed == result
	CHECK( R_TemporalResolveOraclePixel( &p, &k, 4, 4, &r ) );
	CHECK( r.pixelClass == TEMPORAL_RESOLVE_ORACLE_HISTORY && !r.ambiguous && !r.moving );
	w = 3.0f / 4.0f;
	CHECK( Near( r.historyLo[0], current * ( 1.0f - w ) + 0.5f * w ) && Near( r.historyHi[0], r.historyLo[0] ) );
	CHECK( Near( r.historyLo[3], 4.0f ) && Near( r.displayedLo[0], r.historyLo[0] ) );

	// 2. The count saturates at 24 (weight 24 / 25).
	Scene( -1.0f, 0.5f, 24.0f );
	CHECK( R_TemporalResolveOraclePixel( &p, &k, 4, 4, &r ) );
	CHECK( Near( r.historyLo[3], TEMPORAL_RESOLVE_ORACLE_COUNT_CAP ) );
	w = 24.0f / 25.0f;
	CHECK( Near( r.historyLo[0], current * ( 1.0f - w ) + 0.5f * w ) );

	// 3. A pixel moving a whole texel (>= 1/8 px: fully moving) keeps at most the
	//    backend count m = w / (1 - w) = 7 for historyWeight 0.875: written 8.
	Scene( -1.0f, 0.5f, 20.0f );
	s_velocity[( 4 * W + 4 ) * 2] = FloatToHalf( 1.0f / (float)W ); // one texel to the right
	CHECK( R_TemporalResolveOraclePixel( &p, &k, 4, 4, &r ) );
	CHECK( r.pixelClass == TEMPORAL_RESOLVE_ORACLE_HISTORY && Near( r.historyLo[3], 8.0f ) );

	// 4. A history depth outside the current 3x3 range (+ max(0.125, 2 %)) is a
	//    disocclusion: no history, the current colour, count 1.
	Scene( -1.0f, 0.5f, 3.0f );
	s_previousDepth[4 * W + 4] = 12.0f;
	CHECK( R_TemporalResolveOraclePixel( &p, &k, 4, 4, &r ) );
	CHECK( r.pixelClass == TEMPORAL_RESOLVE_ORACLE_CURRENT && Near( r.displayedLo[0], current )
		&& Near( r.historyLo[3], 1.0f ) );

	// 5. Validity 0: no reprojection at all.
	Scene( -1.0f, 0.5f, 3.0f );
	s_validity[4 * W + 4] = 0u;
	CHECK( R_TemporalResolveOraclePixel( &p, &k, 4, 4, &r ) );
	CHECK( r.pixelClass == TEMPORAL_RESOLVE_ORACLE_CURRENT && Near( r.displayedLo[0], current ) );

	// 6. Sharpening is bounded by the history taps: a lone 1.0 history texel
	//    (prior 1.0 at f = 0, blur 0.25) adds 2.4 * 0.75 but displays at most 1.0;
	//    the written history stays unsharpened.
	Scene( 1.0f, 0.0f, 1.0f );
	SetColor( s_previous, 4, 4, 1.0f, 1.0f, 1.0f, 1.0f );
	Setup( &p, &k, 0u );
	CHECK( R_TemporalResolveOraclePixel( &p, &k, 4, 4, &r ) );
	CHECK( r.pixelClass == TEMPORAL_RESOLVE_ORACLE_HISTORY );
	CHECK( Near( r.historyLo[0], 1.0f ) && Near( r.displayedHi[0], 1.0f ) && Near( r.displayedLo[0], 1.0f ) );

	// 7. Spatial fallback: an invalid pixel on a hard vertical edge (0 | 1) is blended
	//    toward the neighbour across the edge; displayed == history rgb, count 1.
	{
		int x, y;
		Scene( 0.0f, 0.0f, 1.0f );
		for ( y = 0; y < H; ++y ) for ( x = 4; x < W; ++x ) SetColor( s_current, x, y, 1.0f, 1.0f, 1.0f, 1.0f );
		s_validity[4 * W + 4] = 0u;
		Setup( &p, &k, 4u );
		CHECK( R_TemporalResolveOraclePixel( &p, &k, 4, 4, &r ) );
		CHECK( r.pixelClass == TEMPORAL_RESOLVE_ORACLE_SPATIAL && !r.ambiguous );
		CHECK( r.displayedLo[0] > 0.0f && r.displayedHi[0] < 1.0f
			&& Near( r.historyLo[0], r.displayedLo[0] ) && Near( r.historyLo[3], 1.0f ) );
		Setup( &p, &k, 0u ); // fallback off: the current colour stays
		CHECK( R_TemporalResolveOraclePixel( &p, &k, 4, 4, &r ) );
		CHECK( r.pixelClass == TEMPORAL_RESOLVE_ORACLE_CURRENT && Near( r.displayedLo[0], 1.0f ) );
	}

	// 8. Half-precision comparison: inside the interval passes, a step outside fails.
	{
		uint16_t inside[4] = { FloatToHalf( 0.5f ), FloatToHalf( 0.5f ), FloatToHalf( 0.5f ), FloatToHalf( 1.0f ) };
		uint16_t outside[4] = { FloatToHalf( 0.5f ), FloatToHalf( 0.5f ), FloatToHalf( 0.53125f ), FloatToHalf( 1.0f ) };
		float lo[4] = { 0.5f, 0.5f, 0.5f, 1.0f }, hi[4] = { 0.5f, 0.5f, 0.5f, 1.0f };
		CHECK( R_TemporalResolveOracleHalf4Within( inside, lo, hi ) );
		CHECK( !R_TemporalResolveOracleHalf4Within( outside, lo, hi ) );
	}
	puts( "tr temporal resolve oracle contract: PASS" );
	return 0;
}
