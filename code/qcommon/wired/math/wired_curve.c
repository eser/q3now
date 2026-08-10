// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026-present Wired Engine contributors

/*
===========================================================================
wired_curve.c -- standalone spline math core (plain-C, vec3_t)

Algorithm ported from RBDOOM-3-BFG neo/idlib/math/Curve.h (spec-to-read,
Rule-1). See wired_curve.h for scope. C++ -> plain-C strip summary:

  template< class type >           -> hardcoded to vec3_t (float[3]).
  idList<float> times / values     -> caller-owned fixed arrays + numKnots.
  idList<float> tension/cont/bias  -> per-knot tcb[N][3].
  idList ops [] .Num .Insert .Clear-> array indexing + memmove insert.
  idVec3 operators (a-b, a+b, s*v, -> q_shared.h macros: VectorSubtract,
    v += s*pt, |v|)                   VectorAdd, VectorScale, VectorMA,
                                      DotProduct/sqrtf.
  _alloca16 scratch                -> fixed WIRED_MAX_KNOTS stack buffers.
  idMath::Sqrt / idMath::Fabs      -> sqrtf / fabsf.
  virtual GetCurrentFirstDerivative-> curveType enum + switch.
  mutable int currentIndex cache   -> dropped; IndexForTime is a pure
                                      binary search (cache was perf-only;
                                      identical results without it).

When built as part of the engine, q_shared.h supplies vec3_t + the vector
macros. When built standalone for the unit test (-DWIRED_CURVE_STANDALONE),
a minimal local fallback of the SAME macros is used so the algorithm is
written to the identical q_shared macro API in both configurations.
===========================================================================
*/

#include "wired_curve.h"

#include <math.h>

#ifdef WIRED_CURVE_STANDALONE
/* Standalone unit-test build: avoid pulling the full q_shared.h platform
   chain into a leaf test executable. Provide the exact q_shared vector
   macros the algorithm uses (identical definitions to q_shared.h:494-499). */
typedef float vec_t;
typedef vec_t vec3_t[3];
#define DotProduct(x,y)			((x)[0]*(y)[0]+(x)[1]*(y)[1]+(x)[2]*(y)[2])
#define VectorSubtract(a,b,c)	((c)[0]=(a)[0]-(b)[0],(c)[1]=(a)[1]-(b)[1],(c)[2]=(a)[2]-(b)[2])
#define VectorAdd(a,b,c)		((c)[0]=(a)[0]+(b)[0],(c)[1]=(a)[1]+(b)[1],(c)[2]=(a)[2]+(b)[2])
#define VectorCopy(a,b)			((b)[0]=(a)[0],(b)[1]=(a)[1],(b)[2]=(a)[2])
#define VectorScale(v, s, o)	((o)[0]=(v)[0]*(s),(o)[1]=(v)[1]*(s),(o)[2]=(v)[2]*(s))
#define VectorMA(v, s, b, o)	((o)[0]=(v)[0]+(b)[0]*(s),(o)[1]=(v)[1]+(b)[1]*(s),(o)[2]=(v)[2]+(b)[2]*(s))
#define VectorClear(a)			((a)[0]=(a)[1]=(a)[2]=0)
#else
#include "q_shared.h"
#endif

#include <string.h>

/*
====================
value / time interpolation helpers  (RBDOOM idCurve_Spline::ValueForIndex,
TimeForIndex — the boundary-aware overrides; both Catmull-Rom and TCB
inherit from idCurve_Spline, so these are the canonical helpers).

RBDOOM Curve.h:1115-1180.
====================
*/
static void WiredCurve_ValueForIndex( const wiredCurve_t *c, int index, float out[3] ) {
	int n = c->numKnots - 1;

	if ( index < 0 ) {
		if ( c->boundaryType == WCURVE_BT_CLOSED ) {
			/* values[ numKnots + index % numKnots ] */
			int k = c->numKnots + ( index % c->numKnots );
			VectorCopy( c->values[k], out );
		} else {
			/* values[0] + index * ( values[1] - values[0] ) */
			float d[3];
			VectorSubtract( c->values[1], c->values[0], d );
			VectorMA( c->values[0], (float)index, d, out );
		}
		return;
	} else if ( index > n ) {
		if ( c->boundaryType == WCURVE_BT_CLOSED ) {
			int k = index % c->numKnots;
			VectorCopy( c->values[k], out );
		} else {
			/* values[n] + ( index - n ) * ( values[n] - values[n-1] ) */
			float d[3];
			VectorSubtract( c->values[n], c->values[n-1], d );
			VectorMA( c->values[n], (float)( index - n ), d, out );
		}
		return;
	}
	VectorCopy( c->values[index], out );
}

static float WiredCurve_TimeForIndex( const wiredCurve_t *c, int index ) {
	int n = c->numKnots - 1;

	if ( index < 0 ) {
		if ( c->boundaryType == WCURVE_BT_CLOSED ) {
			return ( index / c->numKnots ) * ( c->times[n] + c->closeTime )
			     - ( c->times[n] + c->closeTime - c->times[ c->numKnots + ( index % c->numKnots ) ] );
		}
		return c->times[0] + index * ( c->times[1] - c->times[0] );
	} else if ( index > n ) {
		if ( c->boundaryType == WCURVE_BT_CLOSED ) {
			return ( index / c->numKnots ) * ( c->times[n] + c->closeTime )
			     + c->times[ index % c->numKnots ];
		}
		return c->times[n] + ( index - n ) * ( c->times[n] - c->times[n-1] );
	}
	return c->times[index];
}

/*
====================
WiredCurve_ClampedTime  (RBDOOM idCurve_Spline::ClampedTime, Curve.h:1189)
====================
*/
static float WiredCurve_ClampedTime( const wiredCurve_t *c, float t ) {
	if ( c->boundaryType == WCURVE_BT_CLAMPED ) {
		if ( t < c->times[0] ) {
			return c->times[0];
		} else if ( t >= c->times[ c->numKnots - 1 ] ) {
			return c->times[ c->numKnots - 1 ];
		}
	}
	return t;
}

/*
====================
WiredCurve_IndexForTime  (RBDOOM idCurve::IndexForTime, Curve.h:455)

Binary search for the first index whose time is >= t. RBDOOM's mutable
currentIndex cache is dropped — this is the pure binary-search branch,
which returns identical results (the cache was a speed optimization only).
====================
*/
int WiredCurve_IndexForTime( const wiredCurve_t *c, float t ) {
	int len, mid, offset, res;

	len    = c->numKnots;
	mid    = len;
	offset = 0;
	res    = 0;
	while ( mid > 0 ) {
		mid = len >> 1;
		if ( t == c->times[offset + mid] ) {
			return offset + mid;
		} else if ( t > c->times[offset + mid] ) {
			offset += mid;
			len    -= mid;
			res     = 1;
		} else {
			len -= mid;
			res  = 0;
		}
	}
	return offset + res;
}

/* ── Catmull-Rom basis (RBDOOM Curve.h:1698-1740) ─────────────────────── */

static void WiredCurve_CR_Basis( const wiredCurve_t *c, int index, float t, float bvals[4] ) {
	float s = ( t - WiredCurve_TimeForIndex( c, index ) )
	        / ( WiredCurve_TimeForIndex( c, index + 1 ) - WiredCurve_TimeForIndex( c, index ) );
	bvals[0] = ( ( -s + 2.0f ) * s - 1.0f ) * s * 0.5f;
	bvals[1] = ( ( ( 3.0f * s - 5.0f ) * s ) * s + 2.0f ) * 0.5f;
	bvals[2] = ( ( -3.0f * s + 4.0f ) * s + 1.0f ) * s * 0.5f;
	bvals[3] = ( ( s - 1.0f ) * s * s ) * 0.5f;
}

static void WiredCurve_CR_BasisFirstDerivative( const wiredCurve_t *c, int index, float t, float bvals[4] ) {
	float s = ( t - WiredCurve_TimeForIndex( c, index ) )
	        / ( WiredCurve_TimeForIndex( c, index + 1 ) - WiredCurve_TimeForIndex( c, index ) );
	bvals[0] = ( -1.5f * s + 2.0f ) * s - 0.5f;
	bvals[1] = ( 4.5f * s - 5.0f ) * s;
	bvals[2] = ( -4.5f * s + 4.0f ) * s + 0.5f;
	bvals[3] = 1.5f * s * s - s;
}

/* ── TCB / Kochanek-Bartels basis (Hermite; RBDOOM Curve.h:1989-2031) ─── */

static void WiredCurve_TCB_Basis( const wiredCurve_t *c, int index, float t, float bvals[4] ) {
	float s = ( t - WiredCurve_TimeForIndex( c, index ) )
	        / ( WiredCurve_TimeForIndex( c, index + 1 ) - WiredCurve_TimeForIndex( c, index ) );
	bvals[0] = ( ( 2.0f * s - 3.0f ) * s ) * s + 1.0f;
	bvals[1] = ( ( -2.0f * s + 3.0f ) * s ) * s;
	bvals[2] = ( ( s - 2.0f ) * s ) * s + s;
	bvals[3] = ( ( s - 1.0f ) * s ) * s;
}

static void WiredCurve_TCB_BasisFirstDerivative( const wiredCurve_t *c, int index, float t, float bvals[4] ) {
	float s = ( t - WiredCurve_TimeForIndex( c, index ) )
	        / ( WiredCurve_TimeForIndex( c, index + 1 ) - WiredCurve_TimeForIndex( c, index ) );
	bvals[0] = ( 6.0f * s - 6.0f ) * s;
	bvals[1] = ( -6.0f * s + 6.0f ) * s;
	bvals[2] = ( 3.0f * s - 4.0f ) * s + 1.0f;
	bvals[3] = ( 3.0f * s - 2.0f ) * s;
}

/*
====================
WiredCurve_TCB_TangentsForIndex  (RBDOOM Curve.h:1948-1980)

TCB (tension/continuity/bias) tangent computation, including the
non-uniform time-adjust factor `adj`.
====================
*/
static void WiredCurve_TCB_TangentsForIndex( const wiredCurve_t *c, int index, float t0[3], float t1[3] ) {
	float dt, omt, omc, opc, omb, opb, adj, s0, s1;
	float delta[3], vi[3], vim1[3], vip2[3], vip1[3], tmp[3];

	/* delta = ValueForIndex(index+1) - ValueForIndex(index) */
	{
		float va[3], vb[3];
		WiredCurve_ValueForIndex( c, index + 1, va );
		WiredCurve_ValueForIndex( c, index, vb );
		VectorSubtract( va, vb, delta );
	}
	dt = WiredCurve_TimeForIndex( c, index + 1 ) - WiredCurve_TimeForIndex( c, index );

	/* first point's outgoing tangent */
	omt = 1.0f - c->tcb[index][0];   /* tension    */
	omc = 1.0f - c->tcb[index][1];   /* continuity */
	opc = 1.0f + c->tcb[index][1];
	omb = 1.0f - c->tcb[index][2];   /* bias       */
	opb = 1.0f + c->tcb[index][2];
	adj = 2.0f * dt / ( WiredCurve_TimeForIndex( c, index + 1 ) - WiredCurve_TimeForIndex( c, index - 1 ) );
	s0  = 0.5f * adj * omt * opc * opb;
	s1  = 0.5f * adj * omt * omc * omb;

	/* t0 = s1 * delta + s0 * ( ValueForIndex(index) - ValueForIndex(index-1) ) */
	WiredCurve_ValueForIndex( c, index, vi );
	WiredCurve_ValueForIndex( c, index - 1, vim1 );
	VectorSubtract( vi, vim1, tmp );          /* vi - vim1 */
	VectorScale( delta, s1, t0 );             /* t0  = s1 * delta */
	VectorMA( t0, s0, tmp, t0 );              /* t0 += s0 * (vi - vim1) */

	/* second point's incoming tangent */
	omt = 1.0f - c->tcb[index + 1][0];
	omc = 1.0f - c->tcb[index + 1][1];
	opc = 1.0f + c->tcb[index + 1][1];
	omb = 1.0f - c->tcb[index + 1][2];
	opb = 1.0f + c->tcb[index + 1][2];
	adj = 2.0f * dt / ( WiredCurve_TimeForIndex( c, index + 2 ) - WiredCurve_TimeForIndex( c, index ) );
	s0  = 0.5f * adj * omt * omc * opb;
	s1  = 0.5f * adj * omt * opc * omb;

	/* t1 = s1 * ( ValueForIndex(index+2) - ValueForIndex(index+1) ) + s0 * delta */
	WiredCurve_ValueForIndex( c, index + 2, vip2 );
	WiredCurve_ValueForIndex( c, index + 1, vip1 );
	VectorSubtract( vip2, vip1, tmp );        /* vip2 - vip1 */
	VectorScale( tmp, s1, t1 );               /* t1  = s1 * (vip2 - vip1) */
	VectorMA( t1, s0, delta, t1 );            /* t1 += s0 * delta */
}

/*
====================
WiredCurve_GetValue  (position)

Catmull-Rom: RBDOOM Curve.h:1604-1625 (4-neighbor blend i-2..i+1).
TCB:         RBDOOM Curve.h:1858-1878 (2 knots + 2 TCB tangents).
====================
*/
void WiredCurve_GetValue( const wiredCurve_t *c, float t, float out[3] ) {
	int   i, j, k;
	float bvals[4], clampedTime;
	float v[3], acc[3];

	if ( c->numKnots == 1 ) {
		VectorCopy( c->values[0], out );
		return;
	}

	clampedTime = WiredCurve_ClampedTime( c, t );
	i = WiredCurve_IndexForTime( c, clampedTime );

	if ( c->curveType == WCURVE_TCB ) {
		float t0[3], t1[3];
		WiredCurve_TCB_TangentsForIndex( c, i - 1, t0, t1 );
		WiredCurve_TCB_Basis( c, i - 1, clampedTime, bvals );
		WiredCurve_ValueForIndex( c, i - 1, v );
		VectorScale( v, bvals[0], out );
		WiredCurve_ValueForIndex( c, i, v );
		VectorMA( out, bvals[1], v, out );
		VectorMA( out, bvals[2], t0, out );
		VectorMA( out, bvals[3], t1, out );
		return;
	}

	/* Catmull-Rom */
	WiredCurve_CR_Basis( c, i - 1, clampedTime, bvals );
	VectorClear( acc );
	for ( j = 0; j < 4; j++ ) {
		k = i + j - 2;
		WiredCurve_ValueForIndex( c, k, v );
		VectorMA( acc, bvals[j], v, acc );
	}
	VectorCopy( acc, out );
}

/*
====================
WiredCurve_GetFirstDerivative  (tangent -> camera facing)

Catmull-Rom: RBDOOM Curve.h:1635-1657.
TCB:         RBDOOM Curve.h:1889-1910.
Both divide the blended derivative by d = TimeForIndex(i)-TimeForIndex(i-1).
====================
*/
void WiredCurve_GetFirstDerivative( const wiredCurve_t *c, float t, float out[3] ) {
	int   i, j, k;
	float bvals[4], d, clampedTime;
	float v[3], acc[3];

	if ( c->numKnots == 1 ) {
		VectorClear( out );
		return;
	}

	clampedTime = WiredCurve_ClampedTime( c, t );
	i = WiredCurve_IndexForTime( c, clampedTime );

	if ( c->curveType == WCURVE_TCB ) {
		float t0[3], t1[3];
		WiredCurve_TCB_TangentsForIndex( c, i - 1, t0, t1 );
		WiredCurve_TCB_BasisFirstDerivative( c, i - 1, clampedTime, bvals );
		WiredCurve_ValueForIndex( c, i - 1, v );
		VectorScale( v, bvals[0], acc );
		WiredCurve_ValueForIndex( c, i, v );
		VectorMA( acc, bvals[1], v, acc );
		VectorMA( acc, bvals[2], t0, acc );
		VectorMA( acc, bvals[3], t1, acc );
	} else {
		WiredCurve_CR_BasisFirstDerivative( c, i - 1, clampedTime, bvals );
		VectorClear( acc );
		for ( j = 0; j < 4; j++ ) {
			k = i + j - 2;
			WiredCurve_ValueForIndex( c, k, v );
			VectorMA( acc, bvals[j], v, acc );
		}
	}

	d = ( WiredCurve_TimeForIndex( c, i ) - WiredCurve_TimeForIndex( c, i - 1 ) );
	VectorScale( acc, 1.0f / d, out );
}

/*
====================
WiredCurve_GetSpeed  (RBDOOM idCurve::GetSpeed, Curve.h:222)

= length of the first derivative. This is the ONE place the curve-agnostic
arc-length base couples to the concrete curve type (via
WiredCurve_GetFirstDerivative's switch), matching RBDOOM's virtual
GetCurrentFirstDerivative dispatch.
====================
*/
static float WiredCurve_GetSpeed( const wiredCurve_t *c, float t ) {
	float d[3];
	WiredCurve_GetFirstDerivative( c, t, d );
	return sqrtf( DotProduct( d, d ) );
}

/*
====================
WiredCurve_RombergIntegral  (RBDOOM idCurve::RombergIntegral, Curve.h:242)

Trapezoid rule + Richardson extrapolation. order is fixed at 5 by all
callers (GetLengthBetweenKnots / GetLengthForTime / GetTimeForLength).
_alloca16 scratch -> fixed stack buffers of size ORDER.
====================
*/
#define WIRED_ROMBERG_ORDER 5

static float WiredCurve_RombergIntegral( const wiredCurve_t *c, float t0, float t1, int order ) {
	int   i, j, k, m, n;
	float sum, delta;
	float temp0[WIRED_ROMBERG_ORDER];
	float temp1[WIRED_ROMBERG_ORDER];

	delta    = t1 - t0;
	temp0[0] = 0.5f * delta * ( WiredCurve_GetSpeed( c, t0 ) + WiredCurve_GetSpeed( c, t1 ) );

	for ( i = 2, m = 1; i <= order; i++, m *= 2, delta *= 0.5f ) {

		/* approximate using the trapezoid rule */
		sum = 0.0f;
		for ( j = 1; j <= m; j++ ) {
			sum += WiredCurve_GetSpeed( c, t0 + delta * ( j - 0.5f ) );
		}

		/* Richardson extrapolation */
		temp1[0] = 0.5f * ( temp0[0] + delta * sum );
		for ( k = 1, n = 4; k < i; k++, n *= 4 ) {
			temp1[k] = ( n * temp1[k-1] - temp0[k-1] ) / ( n - 1 );
		}

		for ( j = 0; j < i; j++ ) {
			temp0[j] = temp1[j];
		}
	}
	return temp0[order - 1];
}

/*
====================
WiredCurve_GetLengthBetweenKnots  (RBDOOM Curve.h:285)
====================
*/
static float WiredCurve_GetLengthBetweenKnots( const wiredCurve_t *c, int i0, int i1 ) {
	float length = 0.0f;
	int   i;
	for ( i = i0; i < i1; i++ ) {
		length += WiredCurve_RombergIntegral( c, c->times[i], c->times[i+1], WIRED_ROMBERG_ORDER );
	}
	return length;
}

/*
====================
WiredCurve_GetLengthForTime  (RBDOOM Curve.h:301)
====================
*/
float WiredCurve_GetLengthForTime( const wiredCurve_t *c, float t ) {
	float length = 0.0f;
	int   index = WiredCurve_IndexForTime( c, t );
	int   i;
	for ( i = 0; i < index; i++ ) {
		length += WiredCurve_RombergIntegral( c, c->times[i], c->times[i+1], WIRED_ROMBERG_ORDER );
	}
	length += WiredCurve_RombergIntegral( c, c->times[index], t, WIRED_ROMBERG_ORDER );
	return length;
}

/*
====================
WiredCurve_GetTimeForLength  (RBDOOM Curve.h:319)

Inverts the arc-length integral by Newton's method. RISK POINT (2): if the
local speed collapses to ~0 (coincident control points), the Newton step
`t -= diff / GetSpeed(...)` divides by zero -> NaN. Guard added below:
if speed is near zero, bail out and return the current best t (RBDOOM has
no such guard and would NaN here).
====================
*/
float WiredCurve_GetTimeForLength( const wiredCurve_t *c, float length, float epsilon ) {
	int   i, index;
	float accumLength[WIRED_MAX_KNOTS];
	float totalLength, len0, len1, t, diff, speed;

	if ( length <= 0.0f ) {
		return c->times[0];
	}

	totalLength = 0.0f;
	for ( index = 0; index < c->numKnots - 1; index++ ) {
		totalLength += WiredCurve_GetLengthBetweenKnots( c, index, index + 1 );
		accumLength[index] = totalLength;
		if ( length < accumLength[index] ) {
			break;
		}
	}

	if ( index >= c->numKnots - 1 ) {
		return c->times[ c->numKnots - 1 ];
	}

	if ( index == 0 ) {
		len0 = length;
		len1 = accumLength[0];
	} else {
		len0 = length - accumLength[index - 1];
		len1 = accumLength[index] - accumLength[index - 1];
	}

	/* invert the arc length integral using Newton's method */
	t = ( c->times[index + 1] - c->times[index] ) * len0 / len1;
	for ( i = 0; i < 32; i++ ) {
		diff = WiredCurve_RombergIntegral( c, c->times[index], c->times[index] + t, WIRED_ROMBERG_ORDER ) - len0;
		if ( fabsf( diff ) <= epsilon ) {
			return c->times[index] + t;
		}
		/* RISK (2): near-zero-speed guard — coincident knots -> speed 0 -> NaN. */
		speed = WiredCurve_GetSpeed( c, c->times[index] + t );
		if ( speed <= 1e-6f ) {
			return c->times[index] + t;
		}
		t -= diff / speed;
	}
	return c->times[index] + t;
}

/*
====================
WiredCurve_SetConstantSpeed  (RBDOOM idCurve::SetConstantSpeed, Curve.h:395)

RISK POINT (1): the read/write ordering. All per-segment arc lengths are
computed FIRST (reading the ORIGINAL times[] via GetLengthBetweenKnots ->
RombergIntegral -> GetSpeed -> GetFirstDerivative -> TimeForIndex), into a
scratch buffer. ONLY THEN is times[] rewritten. Interleaving the write with
the length reads would corrupt later segment integrals and the dolly would
drift. This port preserves that ordering verbatim.
====================
*/
void WiredCurve_SetConstantSpeed( wiredCurve_t *c, float totalTime ) {
	int   i;
	float length[WIRED_MAX_KNOTS];
	float totalLength, scale, t;

	/* first pass: read all segment lengths against the ORIGINAL times[] */
	totalLength = 0.0f;
	for ( i = 0; i < c->numKnots - 1; i++ ) {
		length[i] = WiredCurve_GetLengthBetweenKnots( c, i, i + 1 );
		totalLength += length[i];
	}

	/* second pass: rewrite times[] so equal dt == equal arc length */
	scale = totalTime / totalLength;
	for ( t = 0.0f, i = 0; i < c->numKnots - 1; i++ ) {
		c->times[i] = t;
		t += scale * length[i];
	}
	c->times[ c->numKnots - 1 ] = totalTime;
}

/*
====================
WiredCurve_IsDone  (RBDOOM idCurve_Spline::IsDone, Curve.h:1211)
====================
*/
int WiredCurve_IsDone( const wiredCurve_t *c, float t ) {
	return ( c->boundaryType != WCURVE_BT_CLOSED && t >= c->times[ c->numKnots - 1 ] );
}

/* ── construction ─────────────────────────────────────────────────────── */

void WiredCurve_Init( wiredCurve_t *c, int curveType, int boundaryType ) {
	memset( c, 0, sizeof( *c ) );
	c->curveType    = curveType;
	c->boundaryType = boundaryType;
	c->closeTime    = 0.0f;
}

/*
====================
WiredCurve_AddValue  (RBDOOM idCurve::AddValue / KochanekBartels::AddValue)

Sorted insert by time (idList::Insert at IndexForTime). Over a fixed array
the idList insert becomes a memmove. Returns the insert index, or -1 if
full.
====================
*/
int WiredCurve_AddValue( wiredCurve_t *c, float time, const float value[3],
                         float tension, float continuity, float bias ) {
	int i;

	if ( c->numKnots >= WIRED_MAX_KNOTS ) {
		return -1;
	}

	i = WiredCurve_IndexForTime( c, time );
	if ( i > c->numKnots ) {
		i = c->numKnots;
	}

	/* shift [i .. numKnots-1] up by one to open a slot at i */
	if ( i < c->numKnots ) {
		memmove( &c->times [i+1], &c->times [i], (size_t)( c->numKnots - i ) * sizeof( c->times[0] ) );
		memmove( &c->values[i+1], &c->values[i], (size_t)( c->numKnots - i ) * sizeof( c->values[0] ) );
		memmove( &c->tcb   [i+1], &c->tcb   [i], (size_t)( c->numKnots - i ) * sizeof( c->tcb[0] ) );
	}

	c->times[i] = time;
	c->values[i][0] = value[0];
	c->values[i][1] = value[1];
	c->values[i][2] = value[2];
	c->tcb[i][0] = tension;
	c->tcb[i][1] = continuity;
	c->tcb[i][2] = bias;
	c->numKnots++;
	return i;
}
