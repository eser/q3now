// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

// CPU model of temporal_resolve.comp for the armed resolve readback. See the
// header for the interval model; the code below mirrors the shader function by
// function (same names where it helps the side-by-side reading).

#include "tr_temporal_resolve_oracle.h"

#include <float.h>
#include <math.h>
#include <string.h>

// The shader's `precise` chains forbid fused multiply-add; keep the CPU model
// unfused everywhere (the non-precise GPU steps may fuse, which the float floor
// of the comparison absorbs).
#pragma STDC FP_CONTRACT OFF

#define FLT_BIG 3.402823466e+38f

typedef struct {
	const temporalResolveOraclePlanes_t *p;
	const temporalResolveOracleParams_t *k;
	float mode;            // -1 low end, 0 nominal, +1 high end of every filtered fetch
	float countMode;       // -1 / 0 / +1: history-to-box distance shrunk / nominal / grown by its error
	float lastError[4];    // error of the last filtered fetch (per channel)
	qboolean outside;      // a read left the capture
	qboolean nearThreshold;// a branch compared values closer than float noise
	qboolean filteredHistory;
} oracleEval_t;

float R_TemporalResolveOracleHalfToFloat( uint16_t h ) {
	uint32_t sign = ( h & 0x8000u ) << 16, exponent = ( h >> 10 ) & 0x1fu,
		mantissa = h & 0x3ffu, bits;
	float out;
	if ( exponent == 0u ) {
		float v = ldexpf( (float)mantissa, -24 );
		return sign ? -v : v;
	}
	if ( exponent == 31u ) bits = sign | 0x7f800000u | ( mantissa << 13 );
	else bits = sign | ( ( exponent + 112u ) << 23 ) | ( mantissa << 13 );
	memcpy( &out, &bits, sizeof( out ) );
	return out;
}

static float HalfStep( float v );

static qboolean Finite( float v ) { return isfinite( v ) ? qtrue : qfalse; }
static qboolean Finite3( const float v[3] ) {
	return Finite( v[0] ) && Finite( v[1] ) && Finite( v[2] );
}
static qboolean Finite4( const float v[4] ) { return Finite3( v ) && Finite( v[3] ); }
static float Clampf( float v, float lo, float hi ) { return fminf( hi, fmaxf( lo, v ) ); }
static int32_t ClampI( int32_t v, int32_t lo, int32_t hi ) { return v < lo ? lo : v > hi ? hi : v; }
static float InvSqrtSq( float x ) { return 1.0f / sqrtf( x * x ); }

// Flags a comparison whose operands are within float noise of each other: the
// GPU may decide it the other way.
static void Near( oracleEval_t *e, float a, float b ) {
	float scale = fabsf( a ) + fabsf( b );
	if ( fabsf( a - b ) <= 2.0e-6f * scale + 1.0e-8f ) e->nearThreshold = qtrue;
}

// Image texel (clamped to the image) -> capture index; flags reads outside it.
static qboolean Local( oracleEval_t *e, int32_t x, int32_t y, uint32_t *index ) {
	int32_t lx, ly;
	x = ClampI( x, 0, (int32_t)e->k->extent[0] - 1 );
	y = ClampI( y, 0, (int32_t)e->k->extent[1] - 1 );
	lx = x - (int32_t)e->p->originX; ly = y - (int32_t)e->p->originY;
	if ( lx < 0 || ly < 0 || lx >= (int32_t)e->p->width || ly >= (int32_t)e->p->height ) {
		e->outside = qtrue; *index = 0u; return qfalse;
	}
	*index = (uint32_t)ly * e->p->width + (uint32_t)lx;
	return qtrue;
}

static void Fetch4( oracleEval_t *e, const uint16_t *plane, int32_t x, int32_t y, float out[4] ) {
	uint32_t i; int c;
	if ( !Local( e, x, y, &i ) ) { out[0] = out[1] = out[2] = out[3] = 0.0f; return; }
	for ( c = 0; c < 4; ++c ) out[c] = R_TemporalResolveOracleHalfToFloat( plane[i * 4u + (uint32_t)c] );
}

static float FetchF( oracleEval_t *e, const float *plane, int32_t x, int32_t y ) {
	uint32_t i;
	return Local( e, x, y, &i ) ? plane[i] : 0.0f;
}

static void FetchVelocity( oracleEval_t *e, int32_t x, int32_t y, float out[2] ) {
	uint32_t i;
	if ( !Local( e, x, y, &i ) ) { out[0] = out[1] = 0.0f; return; }
	out[0] = R_TemporalResolveOracleHalfToFloat( e->p->velocity[i * 2u] );
	out[1] = R_TemporalResolveOracleHalfToFloat( e->p->velocity[i * 2u + 1u] );
}

static float FetchValidity( oracleEval_t *e, int32_t x, int32_t y ) {
	uint32_t i;
	return Local( e, x, y, &i ) ? (float)e->p->validity[i] * ( 1.0f / 255.0f ) : 0.0f;
}

// Hardware LINEAR clamp-to-edge fetch of an rgba16f plane at a normalized uv.
// The nominal value uses exact weights; the interval end moves every channel by
// the largest change a quantized weight can cause (both axes).
static void Bilinear( oracleEval_t *e, const uint16_t *plane, float u, float v, float out[4] ) {
	float tx = u * (float)e->k->extent[0] - 0.5f, ty = v * (float)e->k->extent[1] - 0.5f;
	float bx = floorf( tx ), by = floorf( ty ), fx = tx - bx, fy = ty - by;
	float c00[4], c10[4], c01[4], c11[4];
	int32_t x0 = (int32_t)bx, y0 = (int32_t)by, c;
	Fetch4( e, plane, x0, y0, c00 ); Fetch4( e, plane, x0 + 1, y0, c10 );
	Fetch4( e, plane, x0, y0 + 1, c01 ); Fetch4( e, plane, x0 + 1, y0 + 1, c11 );
	if ( fx != 0.0f || fy != 0.0f ) e->filteredHistory = qtrue;
	for ( c = 0; c < 4; ++c ) {
		float top = c00[c] + ( c10[c] - c00[c] ) * fx;
		float bottom = c01[c] + ( c11[c] - c01[c] ) * fx;
		float value = top + ( bottom - top ) * fy;
		float lo = fminf( fminf( c00[c], c10[c] ), fminf( c01[c], c11[c] ) );
		float hi = fmaxf( fmaxf( c00[c], c10[c] ), fmaxf( c01[c], c11[c] ) );
		// Quantized weights move the result by up to the weight error times the
		// texel range on each axis; the filtered result of an rgba16f texture is
		// itself rounded to half precision (one half step at the value).
		float err = ( fx != 0.0f || fy != 0.0f )
			? 2.0f * ( hi - lo ) * e->k->filterWeightError + HalfStep( value ) : 0.0f;
		out[c] = value + e->mode * err;
		e->lastError[c] = err;
	}
}

// ---- shader helpers -------------------------------------------------------------------------------------------------------

static float LinearizeDepth( const oracleEval_t *e, float z ) {
	float range = e->k->zFar - e->k->zNear;
	float scaled = z * range;
	float denominator = e->k->zNear + scaled;
	float numerator = e->k->zNear * e->k->zFar;
	return numerator / denominator;
}

static float MixPrecise( float x, float y, float a ) {
	float oneMinus = 1.0f - a;
	float left = x * oneMinus;
	float right = y * a;
	return left + right;
}

static float MixFast( float x, float y, float a ) { return x * ( 1.0f - a ) + y * a; }

static float MovingHistoryCount( float movingWeight ) {
	float rest = fmaxf( 1.0f - movingWeight, 1.0f / TEMPORAL_RESOLVE_ORACLE_COUNT_CAP );
	return movingWeight * InvSqrtSq( rest );
}

static float AccumulatedHistoryCount( float storedCount, float movingWeight, const float motion[2] ) {
	float count = Clampf( storedCount, 1.0f, TEMPORAL_RESOLVE_ORACLE_COUNT_CAP );
	float moving = Clampf( sqrtf( motion[0] * motion[0] + motion[1] * motion[1] )
		* TEMPORAL_RESOLVE_ORACLE_STATIC_MOTION_SCALE, 0.0f, 1.0f );
	return count + ( fminf( count, MovingHistoryCount( movingWeight ) ) - count ) * moving;
}

static float RectifiedHistoryCount( float count, const float move[3], const float box[3] ) {
	float boxSize = fmaxf( sqrtf( box[0] * box[0] + box[1] * box[1] + box[2] * box[2] ), 1.0e-4f );
	float rectified = Clampf( sqrtf( move[0] * move[0] + move[1] * move[1] + move[2] * move[2] )
		* InvSqrtSq( boxSize ) - 0.5f, 0.0f, 1.0f );
	return fmaxf( 1.0f, count - ( count - 1.0f ) * rectified );
}

static float HistoryWeightForCount( float count ) {
	return count * InvSqrtSq( count + 1.0f );
}

static void SamplePreviousColor( oracleEval_t *e, const float texel[2], float tapMin[3],
		float tapMax[3], float *centreCount, float out[4], float outError[4] ) {
	const float A = TEMPORAL_RESOLVE_ORACLE_BICUBIC_A;
	float base[2], f[2], w0[2], w1[2], w2[2], w3[2], w12[2], offset12[2];
	float uv0[2], uv12[2], uv3[2], ux[5], uy[5], w[5], sum[4] = { 0, 0, 0, 0 }, total = 0.0f, norm;
	float errSum[4] = { 0, 0, 0, 0 };
	int i, a, c;
	for ( a = 0; a < 2; ++a ) {
		float extent = (float)e->k->extent[a], inv = InvSqrtSq( extent );
		float d0, d1, d2, d3;
		base[a] = floorf( texel[a] );
		f[a] = texel[a] - base[a];
		d0 = 1.0f + f[a]; d1 = f[a]; d2 = 1.0f - f[a]; d3 = 2.0f - f[a];
		w0[a] = ( ( A * d0 - 5.0f * A ) * d0 + 8.0f * A ) * d0 - 4.0f * A;
		w1[a] = ( ( A + 2.0f ) * d1 - ( A + 3.0f ) ) * d1 * d1 + 1.0f;
		w2[a] = ( ( A + 2.0f ) * d2 - ( A + 3.0f ) ) * d2 * d2 + 1.0f;
		w3[a] = ( ( A * d3 - 5.0f * A ) * d3 + 8.0f * A ) * d3 - 4.0f * A;
		w12[a] = w1[a] + w2[a];
		offset12[a] = w2[a] * InvSqrtSq( w12[a] );
		uv0[a] = ( base[a] - 1.0f + 0.5f ) * inv;
		uv12[a] = ( base[a] + offset12[a] + 0.5f ) * inv;
		uv3[a] = ( base[a] + 2.0f + 0.5f ) * inv;
	}
	ux[0] = uv12[0]; ux[1] = uv0[0]; ux[2] = uv3[0]; ux[3] = uv12[0]; ux[4] = uv12[0];
	uy[0] = uv12[1]; uy[1] = uv12[1]; uy[2] = uv12[1]; uy[3] = uv0[1]; uy[4] = uv3[1];
	w[0] = w12[0] * w12[1]; w[1] = w0[0] * w12[1]; w[2] = w3[0] * w12[1];
	w[3] = w12[0] * w0[1]; w[4] = w12[0] * w3[1];
	for ( i = 0; i < 5; ++i ) {
		float tap[4], mode = e->mode;
		// The outer bicubic weights are negative: a tap's interval end that raises the weighted sum is its low end
		// there. Shifting every tap the same way would let those terms cancel and understate the history's range.
		if ( w[i] < 0.0f ) e->mode = -mode;
		Bilinear( e, e->p->previousColor, ux[i], uy[i], tap );
		e->mode = mode;
		for ( c = 0; c < 4; ++c ) { sum[c] += w[i] * tap[c]; errSum[c] += fabsf( w[i] ) * e->lastError[c]; }
		total += w[i];
		for ( c = 0; c < 3; ++c ) { tapMin[c] = fminf( tapMin[c], tap[c] ); tapMax[c] = fmaxf( tapMax[c], tap[c] ); }
		if ( i == 0 ) *centreCount = tap[3];
	}
	norm = InvSqrtSq( total );
	for ( c = 0; c < 4; ++c ) { out[c] = sum[c] * norm; outError[c] = errSum[c] * norm; }
}

static void SamplePreviousBlur( oracleEval_t *e, const float texel[2], float tapMin[3],
		float tapMax[3], float out[3] ) {
	float inv[2] = { InvSqrtSq( (float)e->k->extent[0] ), InvSqrtSq( (float)e->k->extent[1] ) };
	float sum[3] = { 0, 0, 0 }, mode = e->mode;
	int i, c;
	// The displayed colour grows with the bicubic history and shrinks with this blur (detail = history - blur, gain
	// 2.4), so the blur's interval end is taken opposite to the history's: the hull then holds both extremes.
	e->mode = -mode;
	for ( i = 0; i < 4; ++i ) {
		float o[2] = { ( i & 1 ) ? 0.5f : -0.5f, ( i & 2 ) ? 0.5f : -0.5f }, tap[4];
		Bilinear( e, e->p->previousColor, ( texel[0] + 0.5f + o[0] ) * inv[0],
			( texel[1] + 0.5f + o[1] ) * inv[1], tap );
		for ( c = 0; c < 3; ++c ) {
			sum[c] += tap[c];
			tapMin[c] = fminf( tapMin[c], tap[c] ); tapMax[c] = fmaxf( tapMax[c], tap[c] );
		}
	}
	for ( c = 0; c < 3; ++c ) out[c] = sum[c] * 0.25f;
	e->mode = mode;
}

static float SamplePreviousDepthLinear( oracleEval_t *e, const float texel[2] ) {
	float bx = floorf( texel[0] ), by = floorf( texel[1] );
	float fx = texel[0] - bx, fy = texel[1] - by;
	int32_t x = (int32_t)bx, y = (int32_t)by;
	float d00 = FetchF( e, e->p->previousLinearDepth, x, y );
	float d10 = FetchF( e, e->p->previousLinearDepth, x + 1, y );
	float d01 = FetchF( e, e->p->previousLinearDepth, x, y + 1 );
	float d11 = FetchF( e, e->p->previousLinearDepth, x + 1, y + 1 );
	return MixFast( MixFast( d00, d10, fx ), MixFast( d01, d11, fx ), fy );
}

// ---- spatial fallback (fxSpatialAa) ----------------------------------------------------------------------------------------

static float FxLuma( const float c[3] ) {
	float l = c[0] * 0.2126f + c[1] * 0.7152f + c[2] * 0.0722f;
	if ( !Finite( l ) ) return 0.0f;
	l = Clampf( l, 0.0f, 1.0e4f );
	return l * ( 1.0f / sqrtf( 1.0f + l * l ) );
}

static float FxLumaAt( oracleEval_t *e, int32_t x, int32_t y ) {
	float c[4];
	Fetch4( e, e->p->currentColor, x, y, c );
	return FxLuma( c );
}

static float FxEdgeLuma( oracleEval_t *e, int32_t bx, int32_t by, int32_t ax, int32_t ay ) {
	return 0.5f * ( FxLumaAt( e, bx, by ) + FxLumaAt( e, bx + ax, by + ay ) );
}

// Returns qtrue when the fallback changed the colour (a blend was applied).
static qboolean FxSpatialAa( oracleEval_t *e, int32_t px, int32_t py, const float center[4], float out[4] ) {
	float fxM = FxLuma( center );
	float fxN = FxLumaAt( e, px, py - 1 ), fxS = FxLumaAt( e, px, py + 1 );
	float fxW = FxLumaAt( e, px - 1, py ), fxE = FxLumaAt( e, px + 1, py );
	float fxHi = fmaxf( fmaxf( fxN, fxS ), fmaxf( fmaxf( fxW, fxE ), fxM ) );
	float fxLo = fminf( fminf( fxN, fxS ), fminf( fminf( fxW, fxE ), fxM ) );
	float fxSpan = fxHi - fxLo, threshold = fmaxf( 0.0312f, fxHi * 0.125f );
	float fxNW, fxNE, fxSW, fxSE, fxNS, fxWE, fxNWSW, fxNESE, fxHorz, fxVert, fxAround;
	float fxLoSide, fxHiSide, fxGradLo, fxGradHi, fxGradient, fxPair, fxInvSpan, fxBlendC, fxBlendF;
	float fxSubpixel, fxHalfPair, fxScaled, fxEndN, fxEndP, fxEndG, fxSpanLen, fxNear, fxEdgeBlend, fxBlend;
	float across[4];
	int32_t ax, ay, lx, ly, stepsN = 1, stepsP = 1, i;
	qboolean horzSpan, loSteeper, midNegative, doneN, doneP, towardN, goodSpan;
	memcpy( out, center, sizeof( float ) * 4u );
	Near( e, fxSpan, threshold );
	if ( fxSpan < threshold ) return qfalse;
	fxNW = FxLumaAt( e, px - 1, py - 1 ); fxNE = FxLumaAt( e, px + 1, py - 1 );
	fxSW = FxLumaAt( e, px - 1, py + 1 ); fxSE = FxLumaAt( e, px + 1, py + 1 );
	fxNS = fxN + fxS; fxWE = fxW + fxE; fxNWSW = fxNW + fxSW; fxNESE = fxNE + fxSE;
	fxHorz = fabsf( -2.0f * fxW + fxNWSW ) + ( fabsf( -2.0f * fxM + fxNS ) * 2.0f + fabsf( -2.0f * fxE + fxNESE ) );
	fxVert = fabsf( -2.0f * fxS + ( fxSW + fxSE ) ) + ( fabsf( -2.0f * fxM + fxWE ) * 2.0f + fabsf( -2.0f * fxN + ( fxNW + fxNE ) ) );
	Near( e, fxHorz, fxVert );
	horzSpan = fxHorz >= fxVert ? qtrue : qfalse;
	fxAround = ( fxNS + fxWE ) * 2.0f + ( fxNWSW + fxNESE );
	fxLoSide = horzSpan ? fxN : fxW; fxHiSide = horzSpan ? fxS : fxE;
	fxGradLo = fxLoSide - fxM; fxGradHi = fxHiSide - fxM;
	Near( e, fabsf( fxGradLo ), fabsf( fxGradHi ) );
	loSteeper = fabsf( fxGradLo ) >= fabsf( fxGradHi ) ? qtrue : qfalse;
	fxGradient = fmaxf( fabsf( fxGradLo ), fabsf( fxGradHi ) );
	fxPair = loSteeper ? fxLoSide + fxM : fxHiSide + fxM;
	ax = horzSpan ? 0 : 1; ay = horzSpan ? 1 : 0;
	lx = horzSpan ? 1 : 0; ly = horzSpan ? 0 : 1;
	if ( loSteeper ) { ax = -ax; ay = -ay; }
	fxInvSpan = InvSqrtSq( fxSpan );
	fxBlendC = Clampf( fabsf( fxAround * ( 1.0f / 12.0f ) - fxM ) * fxInvSpan, 0.0f, 1.0f );
	fxBlendF = ( -2.0f * fxBlendC + 3.0f ) * ( fxBlendC * fxBlendC );
	fxSubpixel = fxBlendF * fxBlendF * 0.75f;
	fxHalfPair = fxPair * 0.5f;
	fxScaled = fxGradient * 0.25f;
	midNegative = fxM - fxHalfPair < 0.0f ? qtrue : qfalse;
	Near( e, fxM, fxHalfPair );
	fxEndN = FxEdgeLuma( e, px - lx, py - ly, ax, ay ) - fxHalfPair;
	fxEndP = FxEdgeLuma( e, px + lx, py + ly, ax, ay ) - fxHalfPair;
	Near( e, fabsf( fxEndN ), fxScaled ); Near( e, fabsf( fxEndP ), fxScaled );
	doneN = fabsf( fxEndN ) >= fxScaled ? qtrue : qfalse;
	doneP = fabsf( fxEndP ) >= fxScaled ? qtrue : qfalse;
	for ( i = 2; i <= 12; ++i ) {
		if ( !doneN ) {
			fxEndN = FxEdgeLuma( e, px - lx * i, py - ly * i, ax, ay ) - fxHalfPair;
			Near( e, fabsf( fxEndN ), fxScaled );
			doneN = fabsf( fxEndN ) >= fxScaled ? qtrue : qfalse;
			stepsN = i;
		}
		if ( !doneP ) {
			fxEndP = FxEdgeLuma( e, px + lx * i, py + ly * i, ax, ay ) - fxHalfPair;
			Near( e, fabsf( fxEndP ), fxScaled );
			doneP = fabsf( fxEndP ) >= fxScaled ? qtrue : qfalse;
			stepsP = i;
		}
		if ( doneN && doneP ) break;
	}
	towardN = stepsN < stepsP ? qtrue : qfalse;
	fxEndG = towardN ? fxEndN : fxEndP;
	goodSpan = ( fxEndG < 0.0f ) != midNegative ? qtrue : qfalse;
	fxSpanLen = (float)( stepsN + stepsP );
	fxNear = (float)( towardN ? stepsN : stepsP );
	fxEdgeBlend = goodSpan ? ( 0.5f - fxNear * InvSqrtSq( fxSpanLen ) ) : 0.0f;
	fxBlend = Clampf( fmaxf( fxEdgeBlend, fxSubpixel ), 0.0f, 1.0f );
	Fetch4( e, e->p->currentColor, px + ax, py + ay, across );
	for ( i = 0; i < 3; ++i ) out[i] = center[i] * ( 1.0f - fxBlend ) + across[i] * fxBlend;
	out[3] = center[3];
	return qtrue;
}

// ---- velocity dilation and neighbourhood box ---------------------------------------------------------------------------------

static void DlMotionVelocity( oracleEval_t *e, int32_t px, int32_t py, float centerValid, float out[2] ) {
	float centerVelocity[2], bestDepth, dv[2];
	int32_t bx = px, by = py, x, y;
	FetchVelocity( e, px, py, centerVelocity );
	out[0] = centerVelocity[0]; out[1] = centerVelocity[1];
	if ( ( e->k->qualityFlags & 2u ) == 0u || !( centerValid > 0.0f )
			|| !Finite( centerVelocity[0] ) || !Finite( centerVelocity[1] ) ) return;
	bestDepth = FetchF( e, e->p->currentDepth, px, py );
	for ( y = -1; y <= 1; ++y ) for ( x = -1; x <= 1; ++x ) {
		float d = FetchF( e, e->p->currentDepth, px + x, py + y );
		if ( d > bestDepth ) {
			bestDepth = d;
			bx = ClampI( px + x, 0, (int32_t)e->k->extent[0] - 1 );
			by = ClampI( py + y, 0, (int32_t)e->k->extent[1] - 1 );
		}
	}
	if ( bx == px && by == py ) return;
	if ( !( FetchValidity( e, bx, by ) > 0.0f ) ) return;
	FetchVelocity( e, bx, by, dv );
	if ( Finite( dv[0] ) && Finite( dv[1] ) ) { out[0] = dv[0]; out[1] = dv[1]; }
}

static qboolean NeighborhoodBox( oracleEval_t *e, int32_t px, int32_t py, float boxMin[3], float boxMax[3] ) {
	float mn[3] = { FLT_BIG, FLT_BIG, FLT_BIG }, mx[3] = { -FLT_BIG, -FLT_BIG, -FLT_BIG };
	float sum[3] = { 0, 0, 0 }, sq[3] = { 0, 0, 0 };
	int32_t x, y; int c;
	for ( y = -1; y <= 1; ++y ) for ( x = -1; x <= 1; ++x ) {
		float s[4];
		Fetch4( e, e->p->currentColor, px + x, py + y, s );
		for ( c = 0; c < 3; ++c ) {
			mn[c] = fminf( mn[c], s[c] ); mx[c] = fmaxf( mx[c], s[c] );
			sum[c] += s[c]; sq[c] += s[c] * s[c];
		}
	}
	if ( !( Finite3( mn ) && Finite3( mx ) ) ) return qfalse;
	for ( c = 0; c < 3; ++c ) { boxMin[c] = mn[c]; boxMax[c] = mx[c]; }
	if ( ( e->k->qualityFlags & 1u ) != 0u ) {
		for ( c = 0; c < 3; ++c ) {
			float mean = sum[c] * ( 1.0f / 9.0f );
			float sigma = sqrtf( fmaxf( sq[c] * ( 1.0f / 9.0f ) - mean * mean, 0.0f ) );
			boxMin[c] = fmaxf( mn[c], mean - TEMPORAL_RESOLVE_ORACLE_VARIANCE_SIGMA * sigma );
			boxMax[c] = fminf( mx[c], mean + TEMPORAL_RESOLVE_ORACLE_VARIANCE_SIGMA * sigma );
		}
	}
	return qtrue;
}

// ---- main ------------------------------------------------------------------------------------------------------------------------

static temporalResolveOracleClass_t Evaluate( oracleEval_t *e, int32_t px, int32_t py,
		float displayedOut[4], float historyOut[4] ) {
	const temporalResolveOracleParams_t *k = e->k;
	float current[4], result[4], displayed[4], centerTexel[4], velocity[2];
	float motionTexel[2], previousTexel[2], previousCenter[2], valid, historyCount = 1.0f;
	qboolean usedHistory = qfalse, inside, spatial = qfalse;
	int c;
	{
		float inv[2] = { InvSqrtSq( (float)k->extent[0] ), InvSqrtSq( (float)k->extent[1] ) };
		Bilinear( e, e->p->currentColor, ( (float)px + 0.5f ) * inv[0] + k->currentEffectiveJitterUv[0],
			( (float)py + 0.5f ) * inv[1] + k->currentEffectiveJitterUv[1], current );
		Fetch4( e, e->p->currentColor, px, py, centerTexel );
		current[3] = centerTexel[3];
	}
	e->filteredHistory = qfalse; // the reconstruction above is not a history tap
	memcpy( result, current, sizeof( result ) );
	memcpy( displayed, current, sizeof( displayed ) );
	valid = FetchValidity( e, px, py );
	DlMotionVelocity( e, px, py, valid, velocity );
	motionTexel[0] = velocity[0] * (float)k->extent[0];
	motionTexel[1] = velocity[1] * (float)k->extent[1];
	previousTexel[0] = (float)px - motionTexel[0];
	previousTexel[1] = (float)py - motionTexel[1];
	previousCenter[0] = previousTexel[0] + 0.5f;
	previousCenter[1] = previousTexel[1] + 0.5f;
	Near( e, previousCenter[0], 0.0f ); Near( e, previousCenter[1], 0.0f );
	Near( e, previousCenter[0], (float)k->extent[0] ); Near( e, previousCenter[1], (float)k->extent[1] );
	inside = previousCenter[0] >= 0.0f && previousCenter[1] >= 0.0f
		&& previousCenter[0] < (float)k->extent[0] && previousCenter[1] < (float)k->extent[1] ? qtrue : qfalse;
	if ( Finite4( current ) && Finite( valid ) && valid > 0.0f && Finite( velocity[0] )
			&& Finite( velocity[1] ) && Finite( previousTexel[0] ) && Finite( previousTexel[1] ) && inside ) {
		float currentDevice = FetchF( e, e->p->currentDepth, px, py ), deviceLo = currentDevice, deviceHi = currentDevice;
		float currentLinear, rangeA, rangeB, priorLinear, depthLimit, lo, hi;
		int32_t x, y;
		for ( y = -1; y <= 1; ++y ) for ( x = -1; x <= 1; ++x ) {
			float d = FetchF( e, e->p->currentDepth, px + x, py + y );
			deviceLo = fminf( deviceLo, d ); deviceHi = fmaxf( deviceHi, d );
		}
		currentLinear = LinearizeDepth( e, currentDevice );
		rangeA = LinearizeDepth( e, deviceLo );
		rangeB = LinearizeDepth( e, deviceHi );
		priorLinear = SamplePreviousDepthLinear( e, previousTexel );
		depthLimit = fmaxf( k->depthThresholdAbsolute,
			k->depthThresholdRelative * fmaxf( currentLinear, priorLinear ) );
		lo = fminf( rangeA, rangeB ) - depthLimit; hi = fmaxf( rangeA, rangeB ) + depthLimit;
		Near( e, priorLinear, lo ); Near( e, priorLinear, hi );
		if ( Finite( currentLinear ) && Finite( priorLinear ) && Finite( rangeA ) && Finite( rangeB )
				&& currentLinear > 0.0f && priorLinear > 0.0f && priorLinear >= lo && priorLinear <= hi ) {
			float clipMin[3], clipMax[3], tapMin[3] = { FLT_BIG, FLT_BIG, FLT_BIG };
			float tapMax[3] = { -FLT_BIG, -FLT_BIG, -FLT_BIG }, centreCount = FLT_BIG, prior[4], priorError[4];
			qboolean boxValid = NeighborhoodBox( e, px, py, clipMin, clipMax );
			SamplePreviousColor( e, previousTexel, tapMin, tapMax, &centreCount, prior, priorError );
			if ( boxValid && Finite4( prior ) ) {
				float clampedPrior[3], move[3], box[3], confidence, priorCount, baseWeight, weight, resolved[3];
				for ( c = 0; c < 3; ++c ) {
					clampedPrior[c] = Clampf( prior[c], clipMin[c], clipMax[c] );
					move[c] = prior[c] - clampedPrior[c];
					box[c] = clipMax[c] - clipMin[c];
				}
				confidence = Clampf( valid, 0.0f, 1.0f );
				priorCount = AccumulatedHistoryCount( centreCount, Clampf( k->historyWeight, 0.0f, 1.0f ), motionTexel );
				if ( e->countMode != 0.0f ) {
					// A box below four half-precision quanta of the colour is at noise level: the clamp distance
					// over its size (the rectification) is then set by float noise the GPU does not share, so
					// the count is only known to lie between 1 and the accumulated count.
					float boxLength = sqrtf( box[0] * box[0] + box[1] * box[1] + box[2] * box[2] );
					float magnitude = fmaxf( fmaxf( fabsf( current[0] ), fabsf( current[1] ) ), fabsf( current[2] ) );
					if ( boxLength < 4.0f * HalfStep( magnitude ) ) {
						for ( c = 0; c < 3; ++c ) move[c] = e->countMode < 0.0f ? 0.0f : 1.0f; // 0: rectification 0; 1: far outside any noise-level box
					}
				}
				if ( e->countMode != 0.0f ) {
					// The distance the clamp moves the history is a difference of two nearby values; with a
					// box at noise level it is dominated by the filtered history's own error. Bound it by
					// that error in both directions (independently of the value mode above).
					for ( c = 0; c < 3; ++c ) {
						float magnitude = fmaxf( 0.0f, fabsf( move[c] ) + e->countMode * priorError[c] );
						move[c] = move[c] < 0.0f ? -magnitude : magnitude;
					}
				}
				priorCount = RectifiedHistoryCount( priorCount, move, box );
				baseWeight = HistoryWeightForCount( priorCount );
				weight = baseWeight * confidence;
				for ( c = 0; c < 3; ++c ) resolved[c] = MixPrecise( current[c], clampedPrior[c], weight );
				for ( c = 0; c < 3; ++c ) result[c] = resolved[c];
				result[3] = current[3];
				memcpy( displayed, result, sizeof( displayed ) );
				historyCount = fminf( priorCount * confidence + 1.0f, TEMPORAL_RESOLVE_ORACLE_COUNT_CAP );
				if ( ( k->qualityFlags & 8u ) == 0u ) {
					float blur[3];
					SamplePreviousBlur( e, previousTexel, tapMin, tapMax, blur );
					for ( c = 0; c < 3; ++c ) {
						float detail = prior[c] - blur[c];
						displayed[c] = Clampf( resolved[c] + TEMPORAL_RESOLVE_ORACLE_OUTPUT_SHARPEN * confidence * detail,
							fminf( resolved[c], tapMin[c] ), fmaxf( resolved[c], tapMax[c] ) );
					}
					displayed[3] = current[3];
				}
				usedHistory = qtrue;
			}
		}
	}
	if ( !usedHistory && Finite4( current ) && ( k->qualityFlags & 4u ) != 0u )
		spatial = FxSpatialAa( e, px, py, current, result );
	if ( !Finite4( result ) ) memset( result, 0, sizeof( result ) );
	if ( !usedHistory || !Finite4( displayed ) ) memcpy( displayed, result, sizeof( displayed ) );
	memcpy( displayedOut, displayed, sizeof( displayed ) );
	historyOut[0] = result[0]; historyOut[1] = result[1]; historyOut[2] = result[2];
	historyOut[3] = historyCount;
	return usedHistory ? TEMPORAL_RESOLVE_ORACLE_HISTORY
		: spatial ? TEMPORAL_RESOLVE_ORACLE_SPATIAL : TEMPORAL_RESOLVE_ORACLE_CURRENT;
}

qboolean R_TemporalResolveOraclePixel(
		const temporalResolveOraclePlanes_t *planes,
		const temporalResolveOracleParams_t *params,
		int32_t x, int32_t y, temporalResolveOracleResult_t *out ) {
	static const float modes[9][2] = {
		{ 0.0f, 0.0f }, { -1.0f, 0.0f }, { 1.0f, 0.0f },
		{ 0.0f, -1.0f }, { -1.0f, -1.0f }, { 1.0f, -1.0f },
		{ 0.0f, 1.0f }, { -1.0f, 1.0f }, { 1.0f, 1.0f } };
	temporalResolveOracleClass_t classes[9];
	int m, c;
	if ( !planes || !params || !out || !params->extent[0] || !params->extent[1] ) return qfalse;
	memset( out, 0, sizeof( *out ) );
	for ( m = 0; m < 9; ++m ) {
		oracleEval_t e;
		float displayed[4], history[4];
		memset( &e, 0, sizeof( e ) );
		e.p = planes; e.k = params; e.mode = modes[m][0]; e.countMode = modes[m][1];
		classes[m] = Evaluate( &e, x, y, displayed, history );
		if ( e.outside ) { out->pixelClass = TEMPORAL_RESOLVE_ORACLE_UNSUPPORTED; return qtrue; }
		if ( e.nearThreshold ) out->ambiguous = qtrue;
		if ( m == 0 ) out->moving = e.filteredHistory;
		for ( c = 0; c < 4; ++c ) {
			if ( m == 0 || displayed[c] < out->displayedLo[c] ) out->displayedLo[c] = displayed[c];
			if ( m == 0 || displayed[c] > out->displayedHi[c] ) out->displayedHi[c] = displayed[c];
			if ( m == 0 || history[c] < out->historyLo[c] ) out->historyLo[c] = history[c];
			if ( m == 0 || history[c] > out->historyHi[c] ) out->historyHi[c] = history[c];
		}
	}
	out->pixelClass = classes[0];
	for ( m = 1; m < 9; ++m ) if ( classes[m] != classes[0] ) out->ambiguous = qtrue;
	return qtrue;
}

static float HalfStep( float v ) {
	int exponent;
	float a = fabsf( v );
	if ( a < 6.103515625e-05f ) return 5.9604644775390625e-08f; // subnormal half step
	(void)frexpf( a, &exponent );
	return ldexpf( 1.0f, exponent - 11 );
}

qboolean R_TemporalResolveOracleHalf4Within( const uint16_t actual[4],
		const float lo[4], const float hi[4] ) {
	int c;
	for ( c = 0; c < 4; ++c ) {
		float v = R_TemporalResolveOracleHalfToFloat( actual[c] );
		// Two half steps at the value, plus the smallest normal half: the GPU may flush half
		// subnormals (|v| < 2^-14) to zero inside filtering and conversion, so a channel that
		// small is only known to that absolute precision (far below anything visible).
		float floorTerm = 2.0f * HalfStep( fmaxf( fabsf( lo[c] ), fabsf( hi[c] ) ) ) + 6.103515625e-05f;
		if ( !Finite( v ) || v < lo[c] - floorTerm || v > hi[c] + floorTerm ) return qfalse;
	}
	return qtrue;
}
