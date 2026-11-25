// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#ifndef WIRED_TR_TEMPORAL_RESOLVE_ORACLE_H
#define WIRED_TR_TEMPORAL_RESOLVE_ORACLE_H

// CPU model of temporal_resolve.comp, used only by the armed resolve readback
// (a diagnostic capture; nothing here runs in an ordinary frame). For one pixel
// it computes what the resolve must write: the displayed colour (resolvedColor)
// and the history (historyOutColor: unsharpened rgb, sample count in alpha).
//
// The arithmetic follows the shader step for step. The one input the CPU cannot
// reproduce bit for bit is a hardware-filtered fetch (the reconstruction of the
// current frame at the jittered position, the bicubic history taps, the blur
// taps): its weights are quantized by the sampler. Every filtered fetch therefore
// returns an interval, and the pixel is evaluated at the low end, nominally and at
// the high end of every interval. The expected value is the hull of the three
// results widened by a float floor; a pixel whose branches (history used or not,
// spatial fallback taken or not, ...) differ between the three evaluations is
// reported as ambiguous, never as a match.

#include <stdint.h>

#include "../../qcommon/q_shared.h"

// Constants duplicated from temporal_resolve.comp (pinned in both files by the
// resolve policy check).
#define TEMPORAL_RESOLVE_ORACLE_COUNT_CAP 24.0f
#define TEMPORAL_RESOLVE_ORACLE_STATIC_MOTION_SCALE 8.0f
#define TEMPORAL_RESOLVE_ORACLE_OUTPUT_SHARPEN 2.4f
#define TEMPORAL_RESOLVE_ORACLE_BICUBIC_A ( -0.75f )
#define TEMPORAL_RESOLVE_ORACLE_VARIANCE_SIGMA 1.25f

// Capture-local planes, row-major, width x height texels whose texel (0, 0) is
// image texel (originX, originY). Colours are rgba16f, velocity rg16f, validity
// r8 unorm; depths are already decoded to float (device depth, linear history).
typedef struct {
	uint32_t width, height;
	uint32_t originX, originY;
	const uint16_t *currentColor;
	const float *currentDepth;
	const uint16_t *previousColor;
	const float *previousLinearDepth;
	const uint16_t *velocity;
	const uint8_t *validity;
} temporalResolveOraclePlanes_t;

typedef struct {
	uint32_t extent[2];
	float currentEffectiveJitterUv[2];
	float zNear, zFar;
	float depthThresholdAbsolute, depthThresholdRelative;
	float historyWeight;
	uint32_t qualityFlags;
	// Sub-texel weight error of the hardware bilinear filter (8-bit weights:
	// 1/256). See VK_TEMPORAL_RESOLVE_READBACK_FILTER_WEIGHT_ERROR for the sweep.
	float filterWeightError;
} temporalResolveOracleParams_t;

typedef enum {
	TEMPORAL_RESOLVE_ORACLE_UNSUPPORTED = 0, // a read falls outside the capture
	TEMPORAL_RESOLVE_ORACLE_HISTORY,         // history blended
	TEMPORAL_RESOLVE_ORACLE_SPATIAL,         // no history: spatial fallback ran
	TEMPORAL_RESOLVE_ORACLE_CURRENT,         // no history: reconstructed current only
	TEMPORAL_RESOLVE_ORACLE_CLASS_COUNT
} temporalResolveOracleClass_t;

typedef struct {
	float displayedLo[4], displayedHi[4];
	float historyLo[4], historyHi[4];
	temporalResolveOracleClass_t pixelClass;
	qboolean ambiguous;  // the three evaluations took different branches
	qboolean moving;     // nonzero reprojection (filtered history taps)
} temporalResolveOracleResult_t;

float R_TemporalResolveOracleHalfToFloat( uint16_t h );

qboolean R_TemporalResolveOraclePixel(
	const temporalResolveOraclePlanes_t *planes,
	const temporalResolveOracleParams_t *params,
	int32_t x, int32_t y, temporalResolveOracleResult_t *out );

// True when every channel of a GPU half4 lies in [lo - floor, hi + floor], the
// floor being two half-precision steps at the value plus the smallest normal half
// (2^-14: the GPU may flush half subnormals to zero).
qboolean R_TemporalResolveOracleHalf4Within( const uint16_t actual[4],
	const float lo[4], const float hi[4] );

#endif
