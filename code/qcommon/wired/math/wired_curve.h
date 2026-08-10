// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026-present Wired Engine contributors

/*
===========================================================================
wired_curve.h -- standalone spline math core (plain-C, vec3_t)

Ported from the ALGORITHM in RBDOOM-3-BFG neo/idlib/math/Curve.h
(header-only, id Software GPL). This is a spec-to-read port (Rule-1):
the C++ template class hierarchy (idCurve< idVec3 > +
idCurve_Spline + idCurve_CatmullRomSpline +
idCurve_KochanekBartelsSpline) was rewritten as a flat plain-C module.
Zero C++ / template / idList / idVec3 survives in the output.

Scope (math core only):
  - Arc-length base (curve-type-agnostic): Romberg integral (order 5,
    trapezoid + Richardson extrapolation), GetSpeed, GetLengthForTime,
    GetTimeForLength (Newton-inverted arc-length reparameterization),
    SetConstantSpeed (constant-speed dolly), IndexForTime.
  - Catmull-Rom evaluation (primary camera dolly; passes THROUGH every
    control point).
  - Kochanek-Bartels / TCB evaluation (authored ease-in/out; reduces to
    Catmull-Rom bit-for-bit at tension=continuity=bias=0).
  - Boundary layer: FREE / CLAMPED / CLOSED.

NOT ported (approximating curves — do not pass through control points,
not needed for camera dollies): Bezier, B-Spline, NURBS, NaturalCubic.

This module is pure CPU math: NO renderer, NO RAL, NO Vulkan dependency.
It only needs the q_shared.h vec3_t + vector macros (or, when built
standalone for the unit test, an equivalent local fallback — see
wired_curve.c). The header itself has no q_shared.h dependency so it can
be included from a standalone unit test.
===========================================================================
*/
#ifndef WIRED_CURVE_H
#define WIRED_CURVE_H

#ifdef __cplusplus
extern "C" {
#endif

/* Maximum control points (knots) per curve. A cinematic dolly path never
   needs more than a few dozen; 64 is generous. The RBDOOM _alloca16
   scratch buffers (RombergIntegral, GetTimeForLength, SetConstantSpeed)
   become fixed stack buffers of this size. */
#ifndef WIRED_MAX_KNOTS
#define WIRED_MAX_KNOTS 64
#endif

/* Curve type — collapses RBDOOM's virtual GetCurrentFirstDerivative
   dispatch into an enum + switch (the arc-length base is curve-agnostic;
   it only ever calls the concrete first-derivative). */
typedef enum {
	WCURVE_CATMULLROM = 0,   /* uniform cubic interpolating; primary dolly  */
	WCURVE_TCB               /* Kochanek-Bartels / TCB; == CatmullRom at 0  */
} wiredCurveType_t;

/* Boundary type — RBDOOM idCurve_Spline::boundary_t. */
typedef enum {
	WCURVE_BT_FREE = 0,      /* linear end-extrapolation of virtual knots   */
	WCURVE_BT_CLAMPED,       /* time clamps to [times[0], times[n-1]]       */
	WCURVE_BT_CLOSED         /* wrap-around loop                            */
} wiredCurveBoundary_t;

/* The curve. Caller owns the knot arrays (no idList). times[] must be
   monotonically non-decreasing. For WCURVE_TCB, tcb[i] = {tension,
   continuity, bias} at knot i; leave all zero for Catmull-Rom-equivalent
   behavior. tcb[] is unused for WCURVE_CATMULLROM. */
typedef struct {
	int   numKnots;
	float times [WIRED_MAX_KNOTS];
	float values[WIRED_MAX_KNOTS][3];
	float tcb   [WIRED_MAX_KNOTS][3];   /* {tension, continuity, bias}      */
	int   curveType;                    /* wiredCurveType_t                 */
	int   boundaryType;                 /* wiredCurveBoundary_t             */
	float closeTime;                    /* extra gap when boundary==CLOSED  */
} wiredCurve_t;

/* ── construction ─────────────────────────────────────────────────────── */

/* Zero the curve and set type/boundary. closeTime defaults to 0. */
void  WiredCurve_Init( wiredCurve_t *c, int curveType, int boundaryType );

/* Append a knot (time,value). tension/continuity/bias apply to TCB only
   (ignored for Catmull-Rom). Knots are inserted in sorted-by-time order,
   matching RBDOOM idCurve::AddValue (binary insert). Returns the insert
   index, or -1 if the curve is full. */
int   WiredCurve_AddValue( wiredCurve_t *c, float time, const float value[3],
                           float tension, float continuity, float bias );

/* ── evaluation ───────────────────────────────────────────────────────── */

/* Position on the curve at time t (out = float[3]). */
void  WiredCurve_GetValue( const wiredCurve_t *c, float t, float out[3] );

/* First derivative (tangent) at time t (out = float[3]). Normalize for a
   camera facing direction. */
void  WiredCurve_GetFirstDerivative( const wiredCurve_t *c, float t, float out[3] );

/* ── arc length (curve-type-agnostic base) ────────────────────────────── */

/* Arc length from times[0] up to time t. */
float WiredCurve_GetLengthForTime( const wiredCurve_t *c, float t );

/* Inverse of GetLengthForTime: the time at which the accumulated arc
   length equals `length`, found by Newton's method (32 iters). epsilon is
   the arc-length tolerance (RBDOOM default 0.1). Guards against
   division-by-zero when the local speed collapses (coincident knots). */
float WiredCurve_GetTimeForLength( const wiredCurve_t *c, float length, float epsilon );

/* Rewrite times[] so that equal dt corresponds to equal arc length — the
   constant-speed dolly reparameterization. After this call, evaluating at
   uniformly spaced times yields uniformly spaced arc lengths.
   totalTime becomes the time of the last knot. */
void  WiredCurve_SetConstantSpeed( wiredCurve_t *c, float totalTime );

/* Index of the first knot whose time is >= t (RBDOOM IndexForTime, binary
   search; the mutable cache is dropped — pure function). */
int   WiredCurve_IndexForTime( const wiredCurve_t *c, float t );

/* True once t reaches the last knot's time (for non-closed curves). */
int   WiredCurve_IsDone( const wiredCurve_t *c, float t );

#ifdef __cplusplus
}
#endif

#endif /* WIRED_CURVE_H */
