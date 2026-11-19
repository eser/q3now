// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 1999-2005 Id Software, Inc.
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "../../qcommon/q_shared.h"
#include "tr_public.h"

#include "render_material_noise.h"

#define VAL( a ) wired_material_noise_permutation[ ( a ) & 255 ]
// NOLINTNEXTLINE(bugprone-macro-parentheses) — recursive VAL nesting, integer inputs.
#define INDEX( x, y, z, t ) VAL( x + VAL( y + VAL( z + VAL( t ) ) ) )

static float GetNoiseValue( int x, int y, int z, int t )
{
	return wired_material_noise_values[INDEX( x, y, z, t )];
}

void R_NoiseInit( void )
{
	/* Kept for renderer init API compatibility. The former fixed-seed table
	 * is now immutable shared CPU/GPU data, ready before any world loads. */
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
