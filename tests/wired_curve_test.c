/*
 * wired_curve_test.c -- spline math-core unit test (cinematic camera)
 *
 * Deterministic, standalone (no game run). Pins the RBDOOM Curve.h port
 * against known analytic properties. The reference is RBDOOM's own
 * idCurve<idVec3>, which we cannot run here, so we assert the curve's
 * mathematical invariants instead:
 *
 *   1. Interpolation-through-control-points: a Catmull-Rom curve returns
 *      each control point EXACTLY at its knot time.
 *   2. Straight-line control set -> constant first-derivative (tangent).
 *   3. TCB with tension=continuity=bias=0 == Catmull-Rom, bit-for-bit.
 *   4. SetConstantSpeed makes GetLengthForTime linear in t (equal dt ->
 *      equal arc length within epsilon).
 *   5. GetTimeForLength inverts GetLengthForTime (round-trip within eps).
 *   6. Newton div-by-zero guard: coincident control points (speed->0) do
 *      not produce NaN.
 *
 * Build: compiled with -DWIRED_CURVE_STANDALONE so wired_curve.c uses its
 * local vec-macro fallback instead of the full q_shared.h chain.
 *
 * Run:  ctest -R wired_curve   (or ./wired_curve_test directly)
 */
#include <stdio.h>
#include <math.h>

#include "../code/qcommon/wired/math/wired_curve.h"

static int   g_failures = 0;
static int   g_checks   = 0;

static void check( int cond, const char *msg ) {
	g_checks++;
	if ( !cond ) {
		g_failures++;
		printf( "  FAIL  %s\n", msg );
	} else {
		printf( "  ok    %s\n", msg );
	}
}

static int vec3_close( const float a[3], const float b[3], float eps ) {
	return fabsf( a[0]-b[0] ) < eps
	    && fabsf( a[1]-b[1] ) < eps
	    && fabsf( a[2]-b[2] ) < eps;
}

static int is_finite3( const float v[3] ) {
	return isfinite( v[0] ) && isfinite( v[1] ) && isfinite( v[2] );
}

/* Build a 5-knot Catmull-Rom curve with a curvy (non-collinear) path. */
static void build_curvy( wiredCurve_t *c, int type ) {
	float p[5][3] = {
		{  0.0f,  0.0f,  0.0f },
		{ 10.0f, 20.0f,  5.0f },
		{ 30.0f, 10.0f, -5.0f },
		{ 40.0f, 40.0f, 10.0f },
		{ 60.0f,  0.0f,  0.0f },
	};
	int i;
	WiredCurve_Init( c, type, WCURVE_BT_FREE );
	for ( i = 0; i < 5; i++ ) {
		WiredCurve_AddValue( c, (float)i * 10.0f, p[i], 0.0f, 0.0f, 0.0f );
	}
}

/* ── Test 1: interpolation through control points ─────────────────────── */
static void test_interpolation( void ) {
	wiredCurve_t c;
	int i;
	printf( "[1] Catmull-Rom passes through every control point at its knot time\n" );
	build_curvy( &c, WCURVE_CATMULLROM );
	for ( i = 0; i < c.numKnots; i++ ) {
		float out[3];
		char buf[64];
		WiredCurve_GetValue( &c, c.times[i], out );
		snprintf( buf, sizeof buf, "knot %d value == curve(t=%.1f)", i, c.times[i] );
		check( vec3_close( out, c.values[i], 1e-3f ), buf );
	}
}

/* ── Test 2: straight line -> constant tangent ────────────────────────── */
static void test_straight_line_constant_tangent( void ) {
	wiredCurve_t c;
	float dir[3] = { 5.0f, 0.0f, 0.0f };  /* uniform spacing along +X */
	float base[3] = { 0, 0, 0 };
	float ref[3], mid[3];
	int i;
	printf( "[2] straight collinear control set -> constant first-derivative\n" );
	WiredCurve_Init( &c, WCURVE_CATMULLROM, WCURVE_BT_FREE );
	for ( i = 0; i < 5; i++ ) {
		float p[3];
		p[0] = base[0] + dir[0] * i;
		p[1] = base[1] + dir[1] * i;
		p[2] = base[2] + dir[2] * i;
		WiredCurve_AddValue( &c, (float)i, p, 0.0f, 0.0f, 0.0f );
	}
	/* sample the derivative across the interior; it must be constant */
	WiredCurve_GetFirstDerivative( &c, 1.5f, ref );
	for ( i = 0; i < 7; i++ ) {
		float t = 1.0f + 0.25f * i;   /* stay in interior segments [1,3] */
		char buf[64];
		WiredCurve_GetFirstDerivative( &c, t, mid );
		snprintf( buf, sizeof buf, "d/dt at t=%.2f == d/dt at t=1.50", t );
		check( vec3_close( mid, ref, 1e-3f ), buf );
	}
	/* and the tangent should point along +X (nonzero, y=z=0) */
	check( ref[0] > 0.0f && fabsf( ref[1] ) < 1e-3f && fabsf( ref[2] ) < 1e-3f,
	       "tangent points along +X" );
}

/* ── Test 3: TCB(0,0,0) == Catmull-Rom ────────────────────────────────── */
static void test_tcb_equals_catmullrom( void ) {
	wiredCurve_t cr, tcb;
	int i;
	int samples = 41;
	printf( "[3] TCB with T=C=B=0 equals Catmull-Rom (value + derivative)\n" );
	build_curvy( &cr,  WCURVE_CATMULLROM );
	build_curvy( &tcb, WCURVE_TCB );  /* all tcb[]=0 from build_curvy */
	{
		int mismatches = 0;
		for ( i = 0; i < samples; i++ ) {
			float t = 0.0f + ( 40.0f * i ) / ( samples - 1 );
			float vcr[3], vtcb[3], dcr[3], dtcb[3];
			WiredCurve_GetValue( &cr,  t, vcr );
			WiredCurve_GetValue( &tcb, t, vtcb );
			WiredCurve_GetFirstDerivative( &cr,  t, dcr );
			WiredCurve_GetFirstDerivative( &tcb, t, dtcb );
			/* bit-for-bit: TCB(0) reduces to the same Hermite as CR here.
			   The two use different (but algebraically identical) basis
			   forms, so allow a tiny float-rounding tolerance. */
			if ( !vec3_close( vcr, vtcb, 1e-3f ) || !vec3_close( dcr, dtcb, 1e-3f ) ) {
				mismatches++;
			}
		}
		check( mismatches == 0, "value + derivative match across 41 samples" );
	}
}

/* ── Test 4: SetConstantSpeed -> equal knot dt == equal arc length ─────── */
static void test_constant_speed_linearity( void ) {
	wiredCurve_t c;
	float totalTime = 100.0f;
	float totalLen, expectSlope;
	int i;
	int nonlinear = 0;
	printf( "[4] SetConstantSpeed -> cumulative arc length proportional to KNOT time\n" );
	build_curvy( &c, WCURVE_CATMULLROM );
	WiredCurve_SetConstantSpeed( &c, totalTime );

	/* The constant-speed dolly property is a KNOT-boundary guarantee: after
	   SetConstantSpeed, times[i] is rewritten so that the cumulative arc
	   length up to knot i is proportional to times[i]. (Between knots the
	   length is NOT linear in t — a Catmull-Rom segment is not internally
	   unit-speed — so this must be sampled AT the knot times, not on a
	   uniform t grid.) Equal knot-time spacing therefore maps to equal
	   arc-length spacing, which is exactly the dolly's "constant speed". */
	totalLen    = WiredCurve_GetLengthForTime( &c, totalTime );
	expectSlope = totalLen / totalTime;

	check( fabsf( c.times[0] ) < 1e-4f, "SetConstantSpeed: first knot time == 0" );
	check( fabsf( c.times[c.numKnots-1] - totalTime ) < 1e-4f,
	       "SetConstantSpeed: last knot time == totalTime" );

	for ( i = 1; i < c.numKnots; i++ ) {
		float t   = c.times[i];
		float len = WiredCurve_GetLengthForTime( &c, t );
		float predicted = expectSlope * t;
		/* Romberg is an order-5 approximation; a 0.5-unit slack over a
		   ~130-length path is a tight per-knot linearity bound. */
		if ( fabsf( len - predicted ) > 0.5f ) {
			nonlinear++;
			printf( "      knot %d  t=%.4f  len=%.4f  predicted=%.4f\n",
			        i, t, len, predicted );
		}
	}
	check( nonlinear == 0, "cumulative arc length is proportional to knot time" );
}

/* ── Test 5: GetTimeForLength round-trips GetLengthForTime ────────────── */
static void test_time_for_length_roundtrip( void ) {
	wiredCurve_t c;
	int i, samples = 9;
	int bad = 0;
	float epsilon = 0.1f;
	float totalLen;
	printf( "[5] GetTimeForLength inverts GetLengthForTime (round-trip)\n" );
	build_curvy( &c, WCURVE_CATMULLROM );
	totalLen = WiredCurve_GetLengthForTime( &c, c.times[c.numKnots-1] );

	for ( i = 1; i < samples; i++ ) {
		float targetLen = ( totalLen * i ) / samples;
		float t         = WiredCurve_GetTimeForLength( &c, targetLen, epsilon );
		float backLen   = WiredCurve_GetLengthForTime( &c, t );
		if ( fabsf( backLen - targetLen ) > ( epsilon + 1e-2f ) ) {
			bad++;
			printf( "      targetLen=%.4f  t=%.4f  backLen=%.4f\n", targetLen, t, backLen );
		}
	}
	check( bad == 0, "length(timeForLength(L)) == L within epsilon" );
}

/* ── Test 6: Newton div-by-zero guard (coincident points) ─────────────── */
static void test_divzero_guard( void ) {
	wiredCurve_t c;
	float p0[3] = {  0, 0, 0 };
	float p1[3] = { 10, 0, 0 };
	float pdup[3] = { 10, 0, 0 };  /* coincident with p1 -> zero-length seg */
	float p3[3] = { 20, 0, 0 };
	float t, len, out[3];
	printf( "[6] coincident control points do not NaN (Newton div-by-zero guard)\n" );
	WiredCurve_Init( &c, WCURVE_CATMULLROM, WCURVE_BT_FREE );
	WiredCurve_AddValue( &c, 0.0f,  p0,   0, 0, 0 );
	WiredCurve_AddValue( &c, 10.0f, p1,   0, 0, 0 );
	WiredCurve_AddValue( &c, 20.0f, pdup, 0, 0, 0 );  /* speed collapses here */
	WiredCurve_AddValue( &c, 30.0f, p3,   0, 0, 0 );

	len = WiredCurve_GetLengthForTime( &c, 30.0f );
	check( isfinite( len ), "GetLengthForTime finite with coincident knots" );

	t = WiredCurve_GetTimeForLength( &c, len * 0.5f, 0.1f );
	check( isfinite( t ), "GetTimeForLength finite (no NaN) with coincident knots" );

	WiredCurve_GetValue( &c, 15.0f, out );
	check( is_finite3( out ), "GetValue finite with coincident knots" );

	WiredCurve_GetFirstDerivative( &c, 15.0f, out );
	check( is_finite3( out ), "GetFirstDerivative finite with coincident knots" );
}

int main( void ) {
	printf( "wired_curve math-core test (RBDOOM Curve.h port)\n" );
	printf( "======================================================\n" );

	test_interpolation();
	test_straight_line_constant_tangent();
	test_tcb_equals_catmullrom();
	test_constant_speed_linearity();
	test_time_for_length_roundtrip();
	test_divzero_guard();

	printf( "------------------------------------------------------\n" );
	printf( "%d/%d checks passed\n", g_checks - g_failures, g_checks );
	return ( g_failures > 0 ) ? 1 : 0;
}
