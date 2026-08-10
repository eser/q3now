// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 1999-2005 Id Software, Inc.
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "../qcommon/q_shared.h"
#include "../renderercommon/tr_public.h"

#define NOISE_SIZE 256
#define NOISE_MASK ( NOISE_SIZE - 1 )

#define VAL( a ) s_noise_perm[ ( a ) & ( NOISE_MASK )]
// NOLINTNEXTLINE(bugprone-macro-parentheses) — recursive VAL nesting; macro args are always int locals (x, y, z, t)
#define INDEX( x, y, z, t ) VAL( x + VAL( y + VAL( z + VAL( t ) ) ) )

static float s_noise_table[NOISE_SIZE];
static int s_noise_perm[NOISE_SIZE];

static float GetNoiseValue( int x, int y, int z, int t )
{
	int index = INDEX( ( int ) x, ( int ) y, ( int ) z, ( int ) t );

	return s_noise_table[index];
}

/*
 * noise-determinism-fix: fill the noise tables from a LOCAL fixed-seed
 * xorshift32 PRNG so the tables are bit-identical on every launch.
 *
 * Pre-fix: the original idTech3 code used bare `rand()` without seeding
 * R_NoiseInit itself. The C runtime's `rand()` state was whatever the most
 * recent global `srand()` had set — most notably the per-map-load
 * `srand(Com_Milliseconds())` in `SV_SpawnServer_Tick` (sv_init.c:512), which
 * is wall-clock-dependent. That made `s_noise_table` / `s_noise_perm` differ
 * across cold-cache launches, and `rgbGen wave noise` / `tcMod wave noise`
 * surfaces (R_NoiseGet4f → tr_shade_calc.c) animated at a per-launch-different
 * baseline → captured scene brightness varied per launch (smoke ~21/14/15
 * BGR-L1 mean spread). Pinned by brightness-reinvestigation; see that report
 * for the correlation evidence.
 *
 * The noise table is conceptually a fixed asset (a frozen permutation +
 * value table for procedural wave noise); it has no business consuming or
 * mutating global RNG state. The local PRNG below produces a fixed sequence
 * seeded by `NOISE_SEED`, and the C runtime's `rand()` state is left
 * untouched — sv_init.c's gameplay-RNG seeding stays in charge of the
 * global stream.
 *
 * The xorshift32 sequence has the structural quality the original `rand()`
 * provided here (uncorrelated values across the 256-element fill); the
 * specific table content is different from a `rand()`-with-seed-1 run, but
 * that does not matter — there was never a stable "canonical" noise table
 * to preserve; the table was always wall-clock-randomised. Players will see
 * the same animation behaviour they always did, just consistently the same
 * across launches now.
 */
#define NOISE_SEED 0x6D6F6953u   /* "Sion" — arbitrary non-zero constant */

static uint32_t noise_xorshift32( uint32_t *state )
{
	uint32_t x = *state;
	x ^= x << 13;
	x ^= x >> 17;
	x ^= x << 5;
	*state = x;
	return x;
}

void R_NoiseInit( void )
{
	int i;
	uint32_t state = NOISE_SEED;

	for ( i = 0; i < NOISE_SIZE; i++ )
	{
		/* match the original [-1, 1] / [0, 255] ranges of the rand()
		 * variant (semantics-preserving, just deterministic now). */
		s_noise_table[i] = ( ( float )( noise_xorshift32( &state ) & 0xFFFFFFu ) / ( float ) 0xFFFFFF ) * 2.0f - 1.0f;
		s_noise_perm[i]  = ( unsigned char )( noise_xorshift32( &state ) & 0xFFu );
	}
}

float R_NoiseGet4f( float x, float y, float z, double t )
{
	int i;
	int ix, iy, iz, it;
	float fx, fy, fz, ft;
	float front[4];
	float back[4];
	float fvalue, bvalue, value[2], finalvalue;

	ix = ( int ) floor( x );
	fx = x - ix;
	iy = ( int ) floor( y );
	fy = y - iy;
	iz = ( int ) floor( z );
	fz = z - iz;
	it = ( int ) floor( t );
	ft = t - it;

	for ( i = 0; i < 2; i++ )
	{
		front[0] = GetNoiseValue( ix, iy, iz, it + i );
		front[1] = GetNoiseValue( ix+1, iy, iz, it + i );
		front[2] = GetNoiseValue( ix, iy+1, iz, it + i );
		front[3] = GetNoiseValue( ix+1, iy+1, iz, it + i );

		back[0] = GetNoiseValue( ix, iy, iz + 1, it + i );
		back[1] = GetNoiseValue( ix+1, iy, iz + 1, it + i );
		back[2] = GetNoiseValue( ix, iy+1, iz + 1, it + i );
		back[3] = GetNoiseValue( ix+1, iy+1, iz + 1, it + i );

		fvalue = LERP( LERP( front[0], front[1], fx ), LERP( front[2], front[3], fx ), fy );
		bvalue = LERP( LERP( back[0], back[1], fx ), LERP( back[2], back[3], fx ), fy );

		value[i] = LERP( fvalue, bvalue, fz );
	}

	finalvalue = LERP( value[0], value[1], ft );

	return finalvalue;
}
