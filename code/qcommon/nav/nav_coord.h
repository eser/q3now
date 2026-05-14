// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

/*
===========================================================================
nav_coord.h -- Quake ↔ Recast coordinate-system conversion

Quake uses a Z-up right-handed system:  +X=East, +Y=North, +Z=Up
Recast uses a Y-up right-handed system: +X=East, +Y=Up,   +Z=South

Conversion formulas (derived from the Euler/idMat3 analysis in
docs/nav/01-recast-study.md §6):

  Nav_QuakeToRecast([x, y, z])   → [x,  z, -y]
  Nav_RecastToQuake([rx, ry, rz]) → [rx, -rz, ry]

Round-trip identity:
  Nav_RecastToQuake(Nav_QuakeToRecast(v)) == v  for all v in R³

This header has no dependencies on q_shared.h so it can be included
from standalone unit tests.
===========================================================================
*/
#ifndef NAV_COORD_H
#define NAV_COORD_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Nav_QuakeToRecast
 *
 * Converts a Quake world position (Z-up) to Recast space (Y-up).
 * out[0] =  in[0]  (X unchanged)
 * out[1] =  in[2]  (Quake Z becomes Recast Y)
 * out[2] = -in[1]  (Quake Y negated becomes Recast Z)
 *
 * in and out must not overlap.
 */
void Nav_QuakeToRecast( const float in[3], float out[3] );

/*
 * Nav_RecastToQuake
 *
 * Converts a Recast position (Y-up) back to Quake space (Z-up).
 * out[0] =  in[0]   (X unchanged)
 * out[1] = -in[2]   (Recast Z negated becomes Quake Y)
 * out[2] =  in[1]   (Recast Y becomes Quake Z)
 *
 * in and out must not overlap.
 */
void Nav_RecastToQuake( const float in[3], float out[3] );

/*
 * Convenience scalar-argument wrappers for hot paths (inline, zero overhead).
 * Writes directly into three separate float outputs.
 */
#ifdef __cplusplus
static inline void Nav_QuakeToRecastF( float x,  float y,  float z,
                                       float *rx, float *ry, float *rz ) {
    *rx =  x;
    *ry =  z;
    *rz = -y;
}
static inline void Nav_RecastToQuakeF( float rx, float ry, float rz,
                                       float *x,  float *y,  float *z ) {
    *x =  rx;
    *y = -rz;
    *z =  ry;
}

/* Vector-form helpers: convert float[3] arrays in-place style. */
static inline void Nav_QuakeToRecastV( const float *in, float *out ) {
    out[0] =  in[0];
    out[1] =  in[2];
    out[2] = -in[1];
}
static inline void Nav_RecastToQuakeV( const float *in, float *out ) {
    out[0] =  in[0];
    out[1] = -in[2];
    out[2] =  in[1];
}
#endif /* __cplusplus */

#ifdef __cplusplus
}
#endif

#endif /* NAV_COORD_H */
