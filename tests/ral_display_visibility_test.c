// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "ral_display_visibility.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(expr) do { if ( !(expr) ) { \
	fprintf( stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr ); return 1; \
} } while ( 0 )

static qboolean Near( float a, float b ) {
	return fabsf( a - b ) < 0.00001f ? qtrue : qfalse;
}

int main( void ) {
	const float samples[] = { 0.0f, 0.5f, 1.0f, 1.25f, 2.0f, 3.0f,
		5.2f, 16.0f, 32.0f };
	ralDisplayVisibilityPlan_t plan[9], before, output;
	float shadow[9], value, valueBefore, exposure[9];
	float rgbIn[3] = { 0.02f, 0.04f, 0.08f }, rgbOut[3], rgbBefore[3];
	size_t i;
	for ( i = 0; i < 9; ++i ) {
		CHECK( Ral_DisplayVisibilityPlanBuild( samples[i], &plan[i] ) );
		CHECK( Ral_DisplayVisibilityPlanValid( &plan[i] ) );
		CHECK( Near( plan[i].userBrightness, samples[i] ) );
	}
	CHECK( Near( plan[0].exposureScale, 0.75f )
		&& Near( plan[0].shadowExponent, 1.35f ) );
	CHECK( Near( plan[2].exposureScale, 1.0f )
		&& Near( plan[2].shadowExponent, 1.0f ) );
	CHECK( Near( plan[6].userBrightness, 5.2f ) );
	for ( i = 1; i < 9; ++i ) {
		CHECK( plan[i].exposureScale > plan[i - 1].exposureScale );
		CHECK( plan[i].shadowExponent < plan[i - 1].shadowExponent );
	}
	for ( i = 0; i < 9; ++i ) {
		CHECK( Ral_DisplayVisibilityApplyToe( &plan[i], 0.04f, &shadow[i] ) );
		CHECK( Ral_DisplayVisibilityApplyToe( &plan[i], 0.0f, &value )
			&& Near( value, 0.0f ) );
		CHECK( Ral_DisplayVisibilityApplyToe( &plan[i],
			RAL_DISPLAY_SHADOW_PIVOT, &value )
			&& Near( value, RAL_DISPLAY_SHADOW_PIVOT ) );
		CHECK( Ral_DisplayVisibilityApplyToe( &plan[i], 1.0f, &value )
			&& Near( value, 1.0f ) );
	}
	for ( i = 1; i < 9; ++i ) CHECK( shadow[i] > shadow[i - 1] );
	for ( i = 0; i < 9; ++i ) {
		CHECK( Ral_DisplayVisibilityComposeExposure( &plan[i], 1.75f,
			&exposure[i] ) );
		CHECK( Near( exposure[i], 1.75f * plan[i].exposureScale ) );
		CHECK( Ral_DisplayVisibilityApplyRgb( &plan[i], rgbIn, rgbOut ) );
		CHECK( Near( rgbOut[1] / rgbOut[0], rgbIn[1] / rgbIn[0] ) );
		CHECK( Near( rgbOut[2] / rgbOut[0], rgbIn[2] / rgbIn[0] ) );
	}
	for ( i = 1; i < 9; ++i ) CHECK( exposure[i] > exposure[i - 1] );
	CHECK( Ral_DisplayVisibilityApplyRgb( &plan[2], rgbIn, rgbOut )
		&& !memcmp( rgbIn, rgbOut, sizeof( rgbIn ) ) );
	{
		float black[3] = { 0.0f, 0.0f, 0.0f };
		float white[3] = { 1.0f, 1.0f, 1.0f };
		CHECK( Ral_DisplayVisibilityApplyRgb( &plan[8], black, rgbOut )
			&& !memcmp( black, rgbOut, sizeof( black ) ) );
		CHECK( Ral_DisplayVisibilityApplyRgb( &plan[8], white, rgbOut )
			&& !memcmp( white, rgbOut, sizeof( white ) ) );
	}
	value = 17.0f; valueBefore = value;
	CHECK( !Ral_DisplayVisibilityApplyToe( &plan[1], -0.01f, &value )
		&& value == valueBefore );
	memset( &output, 0x5a, sizeof( output ) ); before = output;
	CHECK( !Ral_DisplayVisibilityPlanBuild( -0.01f, &output )
		&& !memcmp( &output, &before, sizeof( output ) ) );
	CHECK( !Ral_DisplayVisibilityPlanBuild( 32.01f, &output )
		&& !memcmp( &output, &before, sizeof( output ) ) );
	CHECK( !Ral_DisplayVisibilityPlanBuild( NAN, &output )
		&& !memcmp( &output, &before, sizeof( output ) ) );
	rgbOut[0] = 9.0f; rgbOut[1] = 8.0f; rgbOut[2] = 7.0f;
	memcpy( rgbBefore, rgbOut, sizeof( rgbOut ) );
	rgbIn[1] = NAN;
	CHECK( !Ral_DisplayVisibilityApplyRgb( &plan[2], rgbIn, rgbOut )
		&& !memcmp( rgbBefore, rgbOut, sizeof( rgbOut ) ) );
	value = 17.0f; valueBefore = value;
	CHECK( !Ral_DisplayVisibilityComposeExposure( &plan[2], NAN, &value )
		&& value == valueBefore );
	puts( "RAL display visibility: PASS" );
	return 0;
}
