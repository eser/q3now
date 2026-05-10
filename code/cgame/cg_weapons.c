/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
//
// cg_weapons.c -- events and effects dealing with weapons
#include "cg_local.h"
#include "../qcommon/wired/render/primitives.h"
#include "../qcommon/wired/render/traps.h"

/*
==========================
CG_MachineGunEjectBrass
==========================
*/
static void CG_MachineGunEjectBrass( centity_t *cent ) {
	localEntity_t	*le;
	refEntity_t		*re;
	vec3_t			velocity, xvelocity;
	vec3_t			offset, xoffset;
	float			waterScale = 1.0f;
	vec3_t			v[3];

	le = CG_AllocLocalEntity();
	re = &le->refEntity;

	velocity[0] = 0;
	velocity[1] = -50 + 40 * crandom();
	velocity[2] = 100 + 50 * crandom();

	le->leType = LE_FRAGMENT;
	le->startTime = cg.time;
    le->endTime = le->startTime + BRASS_TIME + (BRASS_TIME / 4) * random();

	le->pos.trType = TR_GRAVITY;
	le->pos.trTime = cg.time - (rand()&15);

	AnglesToAxis( cent->lerpAngles, v );

	offset[0] = 8;
	offset[1] = -4;
	offset[2] = 24;

	xoffset[0] = offset[0] * v[0][0] + offset[1] * v[1][0] + offset[2] * v[2][0];
	xoffset[1] = offset[0] * v[0][1] + offset[1] * v[1][1] + offset[2] * v[2][1];
	xoffset[2] = offset[0] * v[0][2] + offset[1] * v[1][2] + offset[2] * v[2][2];
	VectorAdd( cent->lerpOrigin, xoffset, re->origin );

	VectorCopy( re->origin, le->pos.trBase );

	if ( CG_PointContents( re->origin, -1 ) & CONTENTS_WATER ) {
		waterScale = 0.10f;
	}

	xvelocity[0] = velocity[0] * v[0][0] + velocity[1] * v[1][0] + velocity[2] * v[2][0];
	xvelocity[1] = velocity[0] * v[0][1] + velocity[1] * v[1][1] + velocity[2] * v[2][1];
	xvelocity[2] = velocity[0] * v[0][2] + velocity[1] * v[1][2] + velocity[2] * v[2][2];
	VectorScale( xvelocity, waterScale, le->pos.trDelta );

	AxisCopy( axisDefault, re->axis );
	re->hModel = cgs.media.machinegunBrassModel;

	le->bounceFactor = 0.4 * waterScale;

	le->angles.trType = TR_LINEAR;
	le->angles.trTime = cg.time;
	le->angles.trBase[0] = rand()&31;
	le->angles.trBase[1] = rand()&31;
	le->angles.trBase[2] = rand()&31;
	le->angles.trDelta[0] = 2;
	le->angles.trDelta[1] = 1;
	le->angles.trDelta[2] = 0;

	le->leFlags = LEF_TUMBLE;
	le->leBounceSoundType = LEBS_BRASS;
	le->leMarkType = LEMT_NONE;
}

/*
==========================
CG_ShotgunEjectBrass
==========================
*/
static void CG_ShotgunEjectBrass( centity_t *cent ) {
	localEntity_t	*le;
	refEntity_t		*re;
	vec3_t			velocity, xvelocity;
	vec3_t			offset, xoffset;
	vec3_t			v[3];

	for ( int i = 0; i < 2; i++ ) {
		float	waterScale = 1.0f;

		le = CG_AllocLocalEntity();
		re = &le->refEntity;

		velocity[0] = 60 + 60 * crandom();
		if ( i == 0 ) {
			velocity[1] = 40 + 10 * crandom();
		} else {
			velocity[1] = -40 + 10 * crandom();
		}
		velocity[2] = 100 + 50 * crandom();

		le->leType = LE_FRAGMENT;
		le->startTime = cg.time;
        le->endTime = le->startTime + BRASS_TIME * 3 + BRASS_TIME * random();

		le->pos.trType = TR_GRAVITY;
		le->pos.trTime = cg.time;

		AnglesToAxis( cent->lerpAngles, v );

		offset[0] = 8;
		offset[1] = 0;
		offset[2] = 24;

		xoffset[0] = offset[0] * v[0][0] + offset[1] * v[1][0] + offset[2] * v[2][0];
		xoffset[1] = offset[0] * v[0][1] + offset[1] * v[1][1] + offset[2] * v[2][1];
		xoffset[2] = offset[0] * v[0][2] + offset[1] * v[1][2] + offset[2] * v[2][2];
		VectorAdd( cent->lerpOrigin, xoffset, re->origin );
		VectorCopy( re->origin, le->pos.trBase );
		if ( CG_PointContents( re->origin, -1 ) & CONTENTS_WATER ) {
			waterScale = 0.10f;
		}

		xvelocity[0] = velocity[0] * v[0][0] + velocity[1] * v[1][0] + velocity[2] * v[2][0];
		xvelocity[1] = velocity[0] * v[0][1] + velocity[1] * v[1][1] + velocity[2] * v[2][1];
		xvelocity[2] = velocity[0] * v[0][2] + velocity[1] * v[1][2] + velocity[2] * v[2][2];
		VectorScale( xvelocity, waterScale, le->pos.trDelta );

		AxisCopy( axisDefault, re->axis );
		re->hModel = cgs.media.shotgunBrassModel;
		le->bounceFactor = 0.3f;

		le->angles.trType = TR_LINEAR;
		le->angles.trTime = cg.time;
		le->angles.trBase[0] = rand()&31;
		le->angles.trBase[1] = rand()&31;
		le->angles.trBase[2] = rand()&31;
		le->angles.trDelta[0] = 1;
		le->angles.trDelta[1] = 0.5;
		le->angles.trDelta[2] = 0;

		le->leFlags = LEF_TUMBLE;
		le->leBounceSoundType = LEBS_BRASS;
		le->leMarkType = LEMT_NONE;
	}
}

// /*
// ==========================
// CG_NailgunEjectBrass
// ==========================
// */
// static void CG_NailgunEjectBrass( centity_t *cent ) {
// 	localEntity_t	*smoke;
// 	vec3_t			origin;
// 	vec3_t			v[3];
// 	vec3_t			offset;
// 	vec3_t			xoffset;
// 	vec3_t			up;

// 	AnglesToAxis( cent->lerpAngles, v );

// 	offset[0] = 0;
// 	offset[1] = -12;
// 	offset[2] = 24;

// 	xoffset[0] = offset[0] * v[0][0] + offset[1] * v[1][0] + offset[2] * v[2][0];
// 	xoffset[1] = offset[0] * v[0][1] + offset[1] * v[1][1] + offset[2] * v[2][1];
// 	xoffset[2] = offset[0] * v[0][2] + offset[1] * v[1][2] + offset[2] * v[2][2];
// 	VectorAdd( cent->lerpOrigin, xoffset, origin );

// 	VectorSet( up, 0, 0, 64 );

// 	smoke = CG_SmokePuff( origin, up, 32, 1, 1, 1, 0.33f, 700, cg.time, 0, 0, cgs.media.smokePuffShader );
// 	// use the optimized local entity add
// 	smoke->leType = LE_SCALE_FADE;
// }

/*
==========================
CG_RailTrail — Q2-spirit modernized rail trail

Architecture:
  1. Helix ribbon: quad-per-segment batch via AddPolyToScene
     - Beam-axis-fixed spiral (PerpendicularVector + RotatePointAroundVector)
     - Stored in railTrail_t for per-frame fade re-submission
  2. White debris: batched billboard quads (random scatter)
  3. Impact sparks: surface-normal-based velocity, animated per-frame
  4. Dynamic light: AddLinearLightToScene with HDR intensity

Zero localEntities used. Zero entity pool impact.
==========================
*/

static railTrail_t  cg_railTrails[MAX_RAIL_TRAILS];

// shared temp buffer for per-frame fade submission (avoids stack pressure)
static polyVert_t   cg_railTempVerts[MAX_RAIL_SEGMENTS * 4];

// Helix GPU-ribbon scratch. Sized at MAX_RAIL_SEGMENTS = 2048 which
// is also RIBBON_MAX_POINTS — the per-call cap on
// trap_R_AddRibbonToScene. ~96 KB; file-static rather than a stack
// local to avoid stack pressure on the rare deep call chain. Reused
// across all active rail trails — only one trail's worth of points
// are alive at any instant since trap_R_AddRibbonToScene memcpies
// out before returning.
static ribbonPoint_t cg_railHelixPoints[MAX_RAIL_SEGMENTS];

/*
==========================
CG_BuildBillboardQuad — reusable camera-facing quad builder
==========================
*/
static void CG_BuildBillboardQuad( polyVert_t *out, vec3_t origin, float radius, byte *rgba ) {
	vec3_t left, up;
	VectorScale( cg.refdef.viewaxis[1], radius, left );
	VectorScale( cg.refdef.viewaxis[2], radius, up );

	VectorAdd( origin, left, out[0].xyz );
	VectorAdd( out[0].xyz, up, out[0].xyz );
	out[0].st[0] = 0; out[0].st[1] = 0;
	out[0].modulate.rgba[0] = rgba[0]; out[0].modulate.rgba[1] = rgba[1];
	out[0].modulate.rgba[2] = rgba[2]; out[0].modulate.rgba[3] = rgba[3];

	VectorSubtract( origin, left, out[1].xyz );
	VectorAdd( out[1].xyz, up, out[1].xyz );
	out[1].st[0] = 1; out[1].st[1] = 0;
	out[1].modulate.rgba[0] = rgba[0]; out[1].modulate.rgba[1] = rgba[1];
	out[1].modulate.rgba[2] = rgba[2]; out[1].modulate.rgba[3] = rgba[3];

	VectorSubtract( origin, left, out[2].xyz );
	VectorSubtract( out[2].xyz, up, out[2].xyz );
	out[2].st[0] = 1; out[2].st[1] = 1;
	out[2].modulate.rgba[0] = rgba[0]; out[2].modulate.rgba[1] = rgba[1];
	out[2].modulate.rgba[2] = rgba[2]; out[2].modulate.rgba[3] = rgba[3];

	VectorAdd( origin, left, out[3].xyz );
	VectorSubtract( out[3].xyz, up, out[3].xyz );
	out[3].st[0] = 0; out[3].st[1] = 1;
	out[3].modulate.rgba[0] = rgba[0]; out[3].modulate.rgba[1] = rgba[1];
	out[3].modulate.rgba[2] = rgba[2]; out[3].modulate.rgba[3] = rgba[3];
}

/*
==========================
CG_ClearRailTrails
==========================
*/
void CG_ClearRailTrails( void ) {
	memset( cg_railTrails, 0, sizeof( cg_railTrails ) );
}

/*
==========================
CG_RailTrail
==========================
*/
void CG_RailTrail( clientInfo_t *ci, vec3_t start, vec3_t end ) {
	railTrail_t *trail;
	vec3_t      beamAxis, temp;
	vec3_t      axis[36];
	float       len;
	int         i, j, oldest, numSegs;

	// compute beam direction and length
	VectorSubtract( end, start, beamAxis );
	len = VectorNormalize( beamAxis );
	if ( len < 1.0f ) {
		return; // degenerate (self-hit or zero-length)
	}

	// find a free trail slot (or recycle oldest)
	trail = NULL;
	oldest = 0;
	for ( i = 0; i < MAX_RAIL_TRAILS; i++ ) {
		if ( !cg_railTrails[i].active ) {
			trail = &cg_railTrails[i];
			break;
		}
		if ( cg_railTrails[i].startTime < cg_railTrails[oldest].startTime ) {
			oldest = i;
		}
	}
	if ( !trail ) {
		trail = &cg_railTrails[oldest]; // recycle oldest
	}

	// initialize trail metadata
	memset( trail, 0, sizeof( *trail ) );
	VectorCopy( start, trail->start );
	VectorCopy( end, trail->end );
	trail->startTime = cg.time;
	trail->color[0] = 255; trail->color[1] = 255;
	trail->color[2] = 255; trail->color[3] = 255;
	VectorCopy( end, trail->impactPoint );

	// ── 1. Store helix axis data (rebuilt each frame with evolving radius/spacing) ──

	// stable perpendicular reference frame (Q2 approach)
	VectorCopy( beamAxis, trail->beamAxis );
	trail->beamLen = len;
	PerpendicularVector( temp, beamAxis );
	for ( i = 0; i < 36; i++ ) {
		RotatePointAroundVector( trail->perpAxis[i], beamAxis, temp, i * 10 );
	}
	trail->numSegments = (int)( len / RAIL_HELIX_SPACING );
	if ( trail->numSegments > MAX_RAIL_SEGMENTS ) {
		trail->numSegments = MAX_RAIL_SEGMENTS;
	}

	// ── 2. Build debris particles (Q2-style: 1 quad per 7.5 units along beam) ──
	//
	// Q2 places one particle every 0.75 units (dec = 0.75 in CL_RailTrail).
	// Since Q3 debris are quads instead of GL_POINTS, use a 1:10 sampling
	// ratio — one quad every ~7.5 units. Colors map Q2 palette 0x00-0x0F
	// (white to mid-grey) via rand()&15 normalized to [0..1].
	{
		float debrisDec = 7.5f;
		float d = 0.0f;
		int   idx = 0;

		while ( d < len && idx < MAX_RAIL_DEBRIS ) {
			// Q2 palette 0x00-0x0F: white(255) → mid-grey(128)
			// lerp: grey = 255 - (rand()&15) * 8  →  range [255..135]
			int greyVal = 255 - ( rand() & 15 ) * 8;
			byte debrisColor[4];
			debrisColor[0] = greyVal; debrisColor[1] = greyVal;
			debrisColor[2] = greyVal; debrisColor[3] = 255;

			// spawn position: along beam center + scatter ±3 (Q2: crand()*3)
			trail->debrisOrg[idx][0] = start[0] + d * beamAxis[0] + crandom() * 3;
			trail->debrisOrg[idx][1] = start[1] + d * beamAxis[1] + crandom() * 3;
			trail->debrisOrg[idx][2] = start[2] + d * beamAxis[2] + crandom() * 3;

			// random drift velocity (Q2: crand()*3)
			trail->debrisDelta[idx][0] = crandom() * 3;
			trail->debrisDelta[idx][1] = crandom() * 3;
			trail->debrisDelta[idx][2] = crandom() * 3;

			// store initial color in debris quad
			CG_BuildBillboardQuad( &trail->debris[idx * 4], trail->debrisOrg[idx], 0.5f, debrisColor );

			d += debrisDec;
			idx++;
		}
		trail->numDebris = idx;
	}

	// ── 3. Build impact sparks ────────────────────────────────────

	trail->numSparks = MAX_RAIL_SPARKS;
	for ( i = 0; i < trail->numSparks; i++ ) {
		byte sparkColor[4] = { 255, 255, 220, 255 }; // warm white

		VectorCopy( end, trail->sparkOrg[i] );

		// velocity: surface normal + random scatter
		trail->sparkVel[i][0] = trail->impactNormal[0] * 80 + crandom() * 40;
		trail->sparkVel[i][1] = trail->impactNormal[1] * 80 + crandom() * 40;
		trail->sparkVel[i][2] = trail->impactNormal[2] * 80 + crandom() * 40;

		CG_BuildBillboardQuad( &trail->sparks[i * 4], trail->sparkOrg[i], 0.3f, sparkColor );
	}

	// ── 4. GPU particle emission (default path) ────────────────────
	//
	// When cg_cpuEffects == 0 (default) the per-frame CPU debris
	// and sparks loops in CG_AddRailTrails are skipped; instead each
	// trail emits once at spawn into the GPU particle pool. The
	// compute shader integrates each particle over its lifetime; the
	// render shader draws billboards. Helix is unaffected — it
	// remains on the CPU poly path regardless of this cvar.
	//
	// CPU spawn-time setup above (debrisOrg/Delta and spark arrays)
	// is left intact so flipping the cvar at runtime back to 1
	// resumes the CPU path mid-session without re-firing.
	if ( !cg_cpuEffects.integer ) {
		emitterDesc_t emitter;

		// Debris: along trail->start..trail->end, MAX_RAIL_DEBRIS
		// particles, EMIT_PATH + SCATTER_CUBE per the rail_debris
		// class definition.
		memset( &emitter, 0, sizeof( emitter ) );
		emitter.cls   = cgs.media.railDebrisClass;
		emitter.count = MAX_RAIL_DEBRIS;
		VectorCopy( trail->start,    emitter.origin );
		VectorCopy( trail->end,      emitter.end );
		VectorCopy( trail->beamAxis, emitter.axis );
		emitter.colorTint[0] = 1.0f; emitter.colorTint[1] = 1.0f;
		emitter.colorTint[2] = 1.0f; emitter.colorTint[3] = 1.0f;
		trap_R_EmitParticles( &emitter );

		// Sparks: at trail->end with impactNormal as cone axis,
		// MAX_RAIL_SPARKS particles, EMIT_POINT + VEL_AXIAL_PLUS_CUBE
		// per the rail_sparks class definition.
		memset( &emitter, 0, sizeof( emitter ) );
		emitter.cls   = cgs.media.railSparksClass;
		emitter.count = MAX_RAIL_SPARKS;
		VectorCopy( trail->end,          emitter.origin );
		VectorCopy( trail->impactNormal, emitter.axis );
		emitter.colorTint[0] = 1.0f; emitter.colorTint[1] = 1.0f;
		emitter.colorTint[2] = 1.0f; emitter.colorTint[3] = 1.0f;
		trap_R_EmitParticles( &emitter );
	}

	trail->active = qtrue;
}

/*
==========================
CG_AddRailTrails — per-frame submission with fade, animation
==========================
*/
void CG_AddRailTrails( void ) {
	int i, j;

	for ( i = 0; i < MAX_RAIL_TRAILS; i++ ) {
		railTrail_t *trail = &cg_railTrails[i];
		float       frac, alpha, segAlpha;
		float       elapsed;
		int         numQuads;
		byte        fadedColor[4];
		qboolean    gpuHelixSubmitted = qfalse;

		if ( !trail->active ) {
			continue;
		}

		frac = (float)( cg.time - trail->startTime ) / RAIL_TRAILTIME;
		if ( frac >= 1.0f ) {
			trail->active = qfalse;
			continue;
		}
		elapsed = ( cg.time - trail->startTime ) / 1000.0f;
		alpha = 1.0f - frac;

		// ── Helix ──
		//
		// cg_cpuEffects 0 (default): GPU path — one
		//   trap_R_AddRibbonToScene per active trail with
		//   PRIM_FLAG_CUSTOM_NORMAL. The renderer's ribbon vertex
		//   shader uses each point's normal directly as the extrude
		//   axis, reproducing the path-aligned twist the CPU loop
		//   builds from cross(beamAxis, perpAxis[ring]).
		// cg_cpuEffects 1: legacy CPU path — per-segment quads via
		//   trap_R_AddPolyToScene (one call per segment, ~250 calls
		//   per trail per frame). Kept for A/B compare and
		//   regression fallback.
		if ( cg_cpuEffects.integer ) {

		// === CPU PATH (legacy; cg_cpuEffects == 1) =================
		// Body verbatim from pre-migration; do not edit inside this
		// block. The sprite primitive's camera-facing billboards
		// overlapped heavily along the spiral path, saturating to
		// white, which is why this path was kept after the sprite
		// migration was reverted.
		{
			// ease-out curve: starts slow, accelerates at end
			float easedFrac  = 1.0f - (1.0f - frac) * (1.0f - frac);
			// radius: 2 → 4 over lifetime, spacing: 3 → 1
			float curRadius  = 2.0f + easedFrac * 2.0f;
			float curSpacing = RAIL_HELIX_SPACING * ( 1.0f - easedFrac * 0.667f );
			float curWidth   = RAIL_RIBBON_WIDTH * ( 1.0f + easedFrac * 1.5f );
			int   numSegs    = (int)( trail->beamLen / curSpacing );
			int   step       = 1;
			int   tempIdx    = 0;
			int   ringJ      = 0;
			vec3_t midpoint;

			if ( numSegs > MAX_RAIL_SEGMENTS ) numSegs = MAX_RAIL_SEGMENTS;
			if ( numSegs < 2 ) numSegs = 2;

			// distance LOD
			midpoint[0] = trail->start[0] + 0.5f * (trail->end[0] - trail->start[0]);
			midpoint[1] = trail->start[1] + 0.5f * (trail->end[1] - trail->start[1]);
			midpoint[2] = trail->start[2] + 0.5f * (trail->end[2] - trail->start[2]);
			if ( Distance( cg.refdef.vieworg, midpoint ) > 1000.0f ) {
				step = 2;
			}

			for ( j = 0; j < numSegs - 1; j += step ) {
				// build from END backward — endpoint stays stable, muzzle recedes
				float d0 = trail->beamLen - j * curSpacing;
				float d1 = trail->beamLen - ( j + step ) * curSpacing;
				int   ring0 = ringJ % 36;
				int   ring1 = ( ringJ + RAIL_HELIX_ROTATION * step ) % 36;
				vec3_t sp0, sp1, rn0, rn1;
				float segPos, unwindFade;
				byte  a;

				if ( d0 < 0.0f ) d0 = 0.0f;
				if ( d1 < 0.0f ) break; // past muzzle, stop

				// spiral positions along beam (d measured from start)
				VectorMA( trail->start, d0, trail->beamAxis, sp0 );
				VectorMA( sp0, curRadius, trail->perpAxis[ring0], sp0 );
				VectorMA( trail->start, d1, trail->beamAxis, sp1 );
				VectorMA( sp1, curRadius, trail->perpAxis[ring1], sp1 );

				// ribbon normals
				CrossProduct( trail->beamAxis, trail->perpAxis[ring0], rn0 );
				VectorNormalize( rn0 );
				CrossProduct( trail->beamAxis, trail->perpAxis[ring1], rn1 );
				VectorNormalize( rn1 );

				// unwind fade: segments near muzzle (far from impact) fade first
				segPos = (float)j / ( numSegs - 1 ); // 0=impact end, 1=muzzle end
				unwindFade = 1.0f - frac * ( 1.0f + segPos );
				if ( unwindFade < 0.0f ) unwindFade = 0.0f;
				a = (byte)( alpha * unwindFade * 255 );

				// quad verts
				VectorMA( sp0, -curWidth, rn0, cg_railTempVerts[tempIdx + 0].xyz );
				VectorMA( sp0,  curWidth, rn0, cg_railTempVerts[tempIdx + 1].xyz );
				VectorMA( sp1, -curWidth, rn1, cg_railTempVerts[tempIdx + 2].xyz );
				VectorMA( sp1,  curWidth, rn1, cg_railTempVerts[tempIdx + 3].xyz );

				cg_railTempVerts[tempIdx + 0].st[0] = d0 / 64.0f; cg_railTempVerts[tempIdx + 0].st[1] = 0.0f;
				cg_railTempVerts[tempIdx + 1].st[0] = d0 / 64.0f; cg_railTempVerts[tempIdx + 1].st[1] = 1.0f;
				cg_railTempVerts[tempIdx + 2].st[0] = d1 / 64.0f; cg_railTempVerts[tempIdx + 2].st[1] = 0.0f;
				cg_railTempVerts[tempIdx + 3].st[0] = d1 / 64.0f; cg_railTempVerts[tempIdx + 3].st[1] = 1.0f;

				{
					// turquoise/ocean-blue: R=80 G=200 B=255
					for ( int v = 0; v < 4; v++ ) {
						cg_railTempVerts[tempIdx + v].modulate.rgba[0] = 80;
						cg_railTempVerts[tempIdx + v].modulate.rgba[1] = 200;
						cg_railTempVerts[tempIdx + v].modulate.rgba[2] = 255;
						cg_railTempVerts[tempIdx + v].modulate.rgba[3] = a;
					}
				}

				tempIdx += 4;
				ringJ += RAIL_HELIX_ROTATION * step;
			}

			if ( tempIdx > 0 ) {
				for ( int k = 0; k < tempIdx; k += 4 ) {
					trap_R_AddPolyToScene( cgs.media.whiteShader, 4,
						&cg_railTempVerts[k] );
				}
			}
		}

		} else {

		// === GPU PATH (default; cg_cpuEffects == 0) ================
		// Builds N spiral sample positions and submits ONE ribbon
		// with PRIM_FLAG_CUSTOM_NORMAL. Each point carries its own
		// world-space position, half-width, RGBA, and unit extrude
		// normal — the ribbon vertex shader reads .normal directly
		// (no cross-product fallback). The N points form N-1 quad
		// segments per the ribbon shader's segIdx → points[i],
		// points[i+1] indexing.
		//
		// The CPU loop emits a quad per CPU iteration using sp0/sp1
		// and rn0/rn1; equivalently, it walks N sample positions
		// and forms N-1 segments. The migration walks the same
		// sample positions and lets the ribbon shader form the
		// segments. Per-segment alpha in CPU was set from the
		// segment's start vertex; in ribbon mode each point carries
		// its own alpha and the GPU interpolates linearly across
		// each segment. The alpha gradient between adjacent points
		// is small (frac is fixed for the frame; segPos changes by
		// step/(numSegs-1) per point), so the interpolation is
		// visually indistinguishable from the CPU's stepped values.
		{
			// Same evolved parameters as the CPU branch.
			float  easedFrac  = 1.0f - (1.0f - frac) * (1.0f - frac);
			float  curRadius  = 2.0f + easedFrac * 2.0f;
			float  curSpacing = RAIL_HELIX_SPACING * ( 1.0f - easedFrac * 0.667f );
			float  curWidth   = RAIL_RIBBON_WIDTH * ( 1.0f + easedFrac * 1.5f );
			int    numSegs    = (int)( trail->beamLen / curSpacing );
			int    step       = 1;
			int    numPoints  = 0;
			vec3_t midpoint;

			if ( numSegs > MAX_RAIL_SEGMENTS ) numSegs = MAX_RAIL_SEGMENTS;
			if ( numSegs < 2 ) numSegs = 2;

			// Distance LOD — same threshold as CPU.
			midpoint[0] = trail->start[0] + 0.5f * (trail->end[0] - trail->start[0]);
			midpoint[1] = trail->start[1] + 0.5f * (trail->end[1] - trail->start[1]);
			midpoint[2] = trail->start[2] + 0.5f * (trail->end[2] - trail->start[2]);
			if ( Distance( cg.refdef.vieworg, midpoint ) > 1000.0f ) {
				step = 2;
			}

			// Walk sample positions j = 0, step, 2*step, ...,
			// up to numSegs - 1 INCLUSIVE — one more sample than
			// the CPU loop's `j < numSegs - 1` to capture the final
			// d1 endpoint that the CPU iteration would have used as
			// its sp1. N samples → N-1 ribbon segments → N-1
			// quads, matching CPU's quad count exactly when both
			// paths use the same step.
			for ( j = 0; j <= numSegs - 1; j += step ) {
				float        d, segPos, unwindFade, a;
				int          ring;
				ribbonPoint_t *p;

				d = trail->beamLen - j * curSpacing;
				if ( d < 0.0f ) {
					// CPU's `if (d1 < 0) break` analogue. Stops
					// adding points past the muzzle.
					break;
				}

				ring = ( j * RAIL_HELIX_ROTATION ) % 36;
				if ( ring < 0 ) ring += 36; // defensive; j*RAIL_HELIX_ROTATION is non-negative in practice

				p = &cg_railHelixPoints[ numPoints ];

				// Position: same formula as CPU's sp0.
				VectorMA( trail->start, d,         trail->beamAxis,        p->pos );
				VectorMA( p->pos,       curRadius, trail->perpAxis[ring],  p->pos );

				// Normal: same formula as CPU's rn0. Caller-side
				// normalize is required by the ribbon shader's
				// PRIM_FLAG_CUSTOM_NORMAL contract.
				CrossProduct( trail->beamAxis, trail->perpAxis[ring], p->normal );
				VectorNormalize( p->normal );
				p->_pad = 0.0f;

				// Half-width: uniform across the trail; written per
				// point because ribbonPoint_t carries it per-point.
				p->width = curWidth;

				// Per-point alpha: same unwind-fade formula CPU
				// uses for the segment-START vertex. CPU emits the
				// quad with this alpha on all four vertices; in
				// ribbon mode the GPU interpolates between adjacent
				// points' alpha, producing a smoother but visually
				// equivalent fade.
				segPos = (float)j / ( numSegs - 1 );
				unwindFade = 1.0f - frac * ( 1.0f + segPos );
				if ( unwindFade < 0.0f ) unwindFade = 0.0f;
				a = alpha * unwindFade;

				// Turquoise/ocean-blue: R=80 G=200 B=255. Float
				// [0..1] per primitives.h convention (HDR-friendly,
				// no quantization).
				p->rgba[0] =  80.0f / 255.0f;
				p->rgba[1] = 200.0f / 255.0f;
				p->rgba[2] = 1.0f;
				// (B=255/255 → 1.0f; written explicitly so the maxed channel reads at a glance.)
				p->rgba[3] = a;

				numPoints++;
			}

			// Submit. Ribbon trap takes four scalars (not a
			// ribbonDesc_t pointer) so the descriptor's points
			// pointer can be VMA-translated cleanly across the
			// WASM-VM boundary; see traps.h for rationale.
			if ( numPoints >= 2 ) {
				trap_R_AddRibbonToScene( cg_railHelixPoints, numPoints,
					cgs.media.whiteShader, PRIM_FLAG_CUSTOM_NORMAL );
			}
		}

		}

		// ── Debris + impact sparks ──
		//
		// cg_cpuEffects == 1 → run the legacy CPU per-frame loops
		// (trap_R_AddPolyToScene). Default 0 → skip; the GPU particle
		// path emitted at trail spawn time in CG_RailTrail handles
		// these effects via trap_R_EmitParticles. Helix block above
		// runs unconditionally.
		if ( cg_cpuEffects.integer ) {

		// ── Debris (with gravity drift) ──

		if ( trail->numDebris > 0 ) {
			int tempIdx = 0;
			for ( j = 0; j < trail->numDebris; j++ ) {
				vec3_t pos;
				byte a;
				byte debrisColor[4];

				VectorMA( trail->debrisOrg[j], elapsed, trail->debrisDelta[j], pos );

				a = (byte)( alpha * 255 );
				debrisColor[0] = trail->debris[j * 4].modulate.rgba[0];
				debrisColor[1] = trail->debris[j * 4].modulate.rgba[1];
				debrisColor[2] = trail->debris[j * 4].modulate.rgba[2];
				debrisColor[3] = a;

				CG_BuildBillboardQuad( &cg_railTempVerts[tempIdx], pos, 0.5f, debrisColor );
				tempIdx += 4;
			}

			{
				for ( int k = 0; k < tempIdx; k += 4 ) {
					trap_R_AddPolyToScene( cgs.media.railRingsShader, 4,
						&cg_railTempVerts[k] );
				}
			}
		}

		// ── Impact sparks (first 200ms, velocity + gravity) ──

		if ( frac < 0.2f && trail->numSparks > 0 ) {
			int tempIdx = 0;
			float sparkAlpha = 1.0f - ( frac / 0.2f );
			for ( j = 0; j < trail->numSparks; j++ ) {
				vec3_t pos;
				byte sparkColor[4];
				byte a;

				a = (byte)( sparkAlpha * 255 );

				VectorMA( trail->sparkOrg[j], elapsed, trail->sparkVel[j], pos );
				pos[2] -= 0.5f * 400.0f * elapsed * elapsed; // strong gravity on sparks

				sparkColor[0] = 255; sparkColor[1] = 255;
				sparkColor[2] = 220; sparkColor[3] = a;

				CG_BuildBillboardQuad( &cg_railTempVerts[tempIdx], pos, 0.3f, sparkColor );
				tempIdx += 4;
			}

			{
				for ( int k = 0; k < tempIdx; k += 4 ) {
					trap_R_AddPolyToScene( cgs.media.whiteShader, 4,
						&cg_railTempVerts[k] );
				}
			}
		}

		} // end cg_cpuEffects gate

		// ── Dynamic light (synced fade) ──

		{
			float lightIntensity = 200.0f * alpha;
			trap_R_AddLightToScene( trail->start, lightIntensity, 0.3f, 0.5f, 1.0f );
			trap_R_AddLightToScene( trail->end, lightIntensity, 0.3f, 0.5f, 1.0f );
		}
	}
}


/*
==========================
CG_RocketTrail
==========================
*/
static void CG_RocketTrail( centity_t *ent, const weaponInfo_t *wi ) {
	int		step;
	vec3_t	origin, lastPos;
	int		t;
	int		startTime, contents;
	int		lastContents;
	entityState_t	*es;
	vec3_t	up;
	localEntity_t	*smoke;

	if ( cg_noProjectileTrail.integer ) {
		return;
	}
#if FEAT_SCREENSHOT_TOOLS
	if ( cg.stopTime ) return;
#endif

	up[0] = 0;
	up[1] = 0;
	up[2] = 0;

	step = 50;

	es = &ent->currentState;
	startTime = ent->trailTime;
	t = step * ( (startTime + step) / step );

	BG_EvaluateTrajectory( &es->pos, cg.time, origin );
	contents = CG_PointContents( origin, -1 );

	// if object (e.g. grenade) is stationary, don't toss up smoke
	if ( es->pos.trType == TR_STATIONARY ) {
		ent->trailTime = cg.time;
		return;
	}

	BG_EvaluateTrajectory( &es->pos, ent->trailTime, lastPos );
	lastContents = CG_PointContents( lastPos, -1 );

	ent->trailTime = cg.time;

	if ( contents & ( CONTENTS_WATER | CONTENTS_SLIME | CONTENTS_LAVA ) ) {
		if ( contents & lastContents & CONTENTS_WATER ) {
			CG_BubbleTrail( lastPos, origin, 8 );
		}
		return;
	}

	for ( ; t <= ent->trailTime ; t += step ) {
		BG_EvaluateTrajectory( &es->pos, t, lastPos );

		smoke = CG_SmokePuff( lastPos, up,
					  wi->trailRadius,
					  1, 1, 1, 0.33f,
					  wi->wiTrailTime,
					  t,
					  0,
					  0,
					  cgs.media.smokePuffShader );
		// use the optimized local entity add
		smoke->leType = LE_SCALE_FADE;
	}

}

// /*
// ==========================
// CG_NailTrail
// ==========================
// */
// static void CG_NailTrail( centity_t *ent, const weaponInfo_t *wi ) {
// 	int		step;
// 	vec3_t	origin, lastPos;
// 	int		t;
// 	int		startTime, contents;
// 	int		lastContents;
// 	entityState_t	*es;
// 	vec3_t	up;
// 	localEntity_t	*smoke;

// 	if ( cg_noProjectileTrail.integer ) {
// 		return;
// 	}

// 	up[0] = 0;
// 	up[1] = 0;
// 	up[2] = 0;

// 	step = 50;

// 	es = &ent->currentState;
// 	startTime = ent->trailTime;
// 	t = step * ( (startTime + step) / step );

// 	BG_EvaluateTrajectory( &es->pos, cg.time, origin );
// 	contents = CG_PointContents( origin, -1 );

// 	// if object (e.g. grenade) is stationary, don't toss up smoke
// 	if ( es->pos.trType == TR_STATIONARY ) {
// 		ent->trailTime = cg.time;
// 		return;
// 	}

// 	BG_EvaluateTrajectory( &es->pos, ent->trailTime, lastPos );
// 	lastContents = CG_PointContents( lastPos, -1 );

// 	ent->trailTime = cg.time;

// 	if ( contents & ( CONTENTS_WATER | CONTENTS_SLIME | CONTENTS_LAVA ) ) {
// 		if ( contents & lastContents & CONTENTS_WATER ) {
// 			CG_BubbleTrail( lastPos, origin, 8 );
// 		}
// 		return;
// 	}

// 	for ( ; t <= ent->trailTime ; t += step ) {
// 		BG_EvaluateTrajectory( &es->pos, t, lastPos );

// 		smoke = CG_SmokePuff( lastPos, up,
// 					  wi->trailRadius,
// 					  1, 1, 1, 0.33f,
// 					  wi->wiTrailTime,
// 					  t,
// 					  0,
// 					  0,
// 					  cgs.media.nailPuffShader );
// 		// use the optimized local entity add
// 		smoke->leType = LE_SCALE_FADE;
// 	}

// }

/*
==========================
CG_PlasmaTrail
==========================
*/
static void CG_PlasmaTrail( centity_t *cent, const weaponInfo_t *wi ) {
	localEntity_t	*le;
	refEntity_t		*re;
	entityState_t	*es;
	vec3_t			velocity, xvelocity, origin;
	vec3_t			offset, xoffset;
	vec3_t			v[3];

	float	waterScale = 1.0f;

	if ( cg_noProjectileTrail.integer ) {
		return;
	}
#if FEAT_SCREENSHOT_TOOLS
	if ( cg.stopTime ) return;
#endif

	es = &cent->currentState;

	BG_EvaluateTrajectory( &es->pos, cg.time, origin );

	le = CG_AllocLocalEntity();
	re = &le->refEntity;

	velocity[0] = 60 - 120 * crandom();
	velocity[1] = 40 - 80 * crandom();
	velocity[2] = 100 - 200 * crandom();

	le->leType = LE_MOVE_SCALE_FADE;
	le->leFlags = LEF_TUMBLE;
	le->leBounceSoundType = LEBS_NONE;
	le->leMarkType = LEMT_NONE;

	le->startTime = cg.time;
	le->endTime = le->startTime + 600;

	le->pos.trType = TR_GRAVITY;
	le->pos.trTime = cg.time;

	AnglesToAxis( cent->lerpAngles, v );

	offset[0] = 2;
	offset[1] = 2;
	offset[2] = 2;

	xoffset[0] = offset[0] * v[0][0] + offset[1] * v[1][0] + offset[2] * v[2][0];
	xoffset[1] = offset[0] * v[0][1] + offset[1] * v[1][1] + offset[2] * v[2][1];
	xoffset[2] = offset[0] * v[0][2] + offset[1] * v[1][2] + offset[2] * v[2][2];

	VectorAdd( origin, xoffset, re->origin );
	VectorCopy( re->origin, le->pos.trBase );

	if ( CG_PointContents( re->origin, -1 ) & CONTENTS_WATER ) {
		waterScale = 0.10f;
	}

	xvelocity[0] = velocity[0] * v[0][0] + velocity[1] * v[1][0] + velocity[2] * v[2][0];
	xvelocity[1] = velocity[0] * v[0][1] + velocity[1] * v[1][1] + velocity[2] * v[2][1];
	xvelocity[2] = velocity[0] * v[0][2] + velocity[1] * v[1][2] + velocity[2] * v[2][2];
	VectorScale( xvelocity, waterScale, le->pos.trDelta );

	AxisCopy( axisDefault, re->axis );
	re->shaderTime.f =cg.time / 1000.0f;
	re->reType = RT_SPRITE;
	re->radius = 0.25f;
	re->customShader = cgs.media.railRingsShader;
	le->bounceFactor = 0.3f;

	re->shaderRGBA[0] = wi->flashDlightColor[0] * 63;
	re->shaderRGBA[1] = wi->flashDlightColor[1] * 63;
	re->shaderRGBA[2] = wi->flashDlightColor[2] * 63;
	re->shaderRGBA[3] = 63;

	le->color[0] = wi->flashDlightColor[0] * 0.2;
	le->color[1] = wi->flashDlightColor[1] * 0.2;
	le->color[2] = wi->flashDlightColor[2] * 0.2;
	le->color[3] = 0.25f;

	le->angles.trType = TR_LINEAR;
	le->angles.trTime = cg.time;
	le->angles.trBase[0] = rand()&31;
	le->angles.trBase[1] = rand()&31;
	le->angles.trBase[2] = rand()&31;
	le->angles.trDelta[0] = 1;
	le->angles.trDelta[1] = 0.5;
	le->angles.trDelta[2] = 0;

}
/*
==========================
CG_GrappleTrail
==========================
*/
void CG_GrappleTrail( centity_t *ent, const weaponInfo_t *wi ) {
	vec3_t	origin;
	vec3_t	beamStart;
	entityState_t	*es;
	vec3_t	up;

	es = &ent->currentState;

	BG_EvaluateTrajectory( &es->pos, cg.time, origin );
	ent->trailTime = cg.time;

	// FIXME adjust for muzzle position
	VectorCopy( cg_entities[ ent->currentState.otherEntityNum ].lerpOrigin, beamStart );
	beamStart[2] += 26;
	AngleVectors( cg_entities[ ent->currentState.otherEntityNum ].lerpAngles, NULL, NULL, up );
	VectorMA( beamStart, -6, up, beamStart );

	if ( Distance( beamStart, origin ) < 64 ) {
		return; // Don't draw if close
	}

	beamDesc_t bd;
	memset( &bd, 0, sizeof( bd ) );
	VectorCopy( beamStart, bd.start );
	VectorCopy( origin, bd.end );
	bd.startWidth     = 8.0f;
	bd.endWidth       = 8.0f;
	bd.startColor[0] = bd.startColor[1] = bd.startColor[2] = bd.startColor[3] = 1.0f;
	bd.endColor[0]   = bd.endColor[1]   = bd.endColor[2]   = bd.endColor[3]   = 1.0f;
	bd.shader         = cgs.media.lightningShaderPrim;
	bd.duration       = 0.0f;
	bd.axialCopies    = 1;             // rope-like, NOT cross (legacy was 4 forced by engine)
	bd.startEntityNum = -1;
	bd.endEntityNum   = -1;
	bd.uvScroll[0]    = 0.0f;          // shader handles scroll
	bd.uvScroll[1]    = 0.0f;
	trap_R_AddBeamToScene( &bd );
}

/*
==========================
CG_GrenadeTrail
==========================
*/
static void CG_GrenadeTrail( centity_t *ent, const weaponInfo_t *wi ) {
	CG_RocketTrail( ent, wi );
}


/*
=================
CG_RegisterWeapon

The server says this item is used on this level
=================
*/
void CG_RegisterWeapon( int weaponNum ) {
	weaponInfo_t	*weaponInfo;
	gitem_t			*item, *ammo;
	char			path[MAX_QPATH];
	vec3_t			mins, maxs;
	int				i;

	weaponInfo = &cg_weapons[weaponNum];

	if ( weaponInfo->registered ) {
		return;
	}

    memset(weaponInfo, 0, sizeof(*weaponInfo));
    weaponInfo->registered = qtrue;

    if (weaponNum == WP_NONE) {
        MAKERGB(weaponInfo->flashDlightColor, 0.6f, 0.6f, 1.0f);
        weaponInfo->flashSound[0] = trap_S_RegisterSound("sound/weapons/rocket/rocklf1a.opus", qfalse);
        weaponInfo->missileModel = 0;
        weaponInfo->missileTrailFunc = CG_GrappleTrail;
        weaponInfo->missileDlight = 200;

        return;
    }

	for ( item = bg_itemlist + 1 ; item->classname ; item++ ) {
		if ( item->giType == IT_WEAPON && item->giTag == weaponNum ) {
			weaponInfo->item = item;
			break;
		}
	}
	if ( !item->classname ) {
		Com_Terminate( TERM_CLIENT_DROP, "Couldn't find weapon %i", weaponNum );
	}
	CG_RegisterItemVisuals( item - bg_itemlist );

	// load cmodel before model so filecache works
	weaponInfo->weaponModel = trap_R_RegisterModel( item->world_model[0] );

	// calc midpoint for rotation
	trap_R_ModelBounds( weaponInfo->weaponModel, mins, maxs );
	for ( i = 0 ; i < 3 ; i++ ) {
		weaponInfo->weaponMidpoint[i] = mins[i] + 0.5 * ( maxs[i] - mins[i] );
	}

	weaponInfo->weaponIcon = trap_R_RegisterShader( item->icon );
	weaponInfo->ammoIcon = trap_R_RegisterShader( item->icon );

	for ( ammo = bg_itemlist + 1 ; ammo->classname ; ammo++ ) {
		if ( ammo->giType == IT_AMMO && ammo->giTag == weaponNum ) {
			break;
		}
	}
	if ( ammo->classname && ammo->world_model[0] ) {
		weaponInfo->ammoModel = trap_R_RegisterModel( ammo->world_model[0] );
	}

	COM_StripExtension( item->world_model[0], path, sizeof(path) );
	{ qstring_t _p_qs = QS_WrapExisting(path, sizeof(path)); QS_Append(&_p_qs, "_flash.md3"); }
	weaponInfo->flashModel = trap_R_RegisterModel( path );

	COM_StripExtension( item->world_model[0], path, sizeof(path) );
	{ qstring_t _p_qs = QS_WrapExisting(path, sizeof(path)); QS_Append(&_p_qs, "_barrel.md3"); }
	weaponInfo->barrelModel = trap_R_RegisterModel( path );

	COM_StripExtension( item->world_model[0], path, sizeof(path) );
	{ qstring_t _p_qs = QS_WrapExisting(path, sizeof(path)); QS_Append(&_p_qs, "_hand.md3"); }
	weaponInfo->handsModel = trap_R_RegisterModel( path );

	if ( !weaponInfo->handsModel ) {
		weaponInfo->handsModel = trap_R_RegisterModel( "models/weapons2/shotgun/shotgun_hand.md3" );
	}

	switch ( weaponNum ) {
	case WP_GAUNTLET:
		MAKERGB( weaponInfo->flashDlightColor, 0.6f, 0.6f, 0 );

		weaponInfo->firingPriSound = trap_S_RegisterSound( "sound/weapons/melee/fstrun.opus", qfalse );
		weaponInfo->firingSecSound = trap_S_RegisterSound( "sound/weapons/melee/fstrun.opus", qfalse );
		weaponInfo->flashSound[0] = trap_S_RegisterSound( "sound/weapons/melee/fstatck.opus", qfalse );
		break;

	case WP_MACHINEGUN:
		MAKERGB( weaponInfo->flashDlightColor, 1, 1, 0 );

		weaponInfo->flashSound[0] = trap_S_RegisterSound( "sound/weapons/machinegun/machgf1b.opus", qfalse );
		weaponInfo->flashSound[1] = trap_S_RegisterSound( "sound/weapons/machinegun/machgf2b.opus", qfalse );
		weaponInfo->flashSound[2] = trap_S_RegisterSound( "sound/weapons/machinegun/machgf3b.opus", qfalse );
		weaponInfo->flashSound[3] = trap_S_RegisterSound( "sound/weapons/machinegun/machgf4b.opus", qfalse );
		weaponInfo->ejectBrassFunc = CG_MachineGunEjectBrass;

		cgs.media.bulletExplosionShader = trap_R_RegisterShader( "bulletExplosion" );
		break;

	case WP_SHOTGUN:
		MAKERGB( weaponInfo->flashDlightColor, 1, 1, 0 );

		weaponInfo->flashSound[0] = trap_S_RegisterSound( "sound/weapons/shotgun/sshotf1b.opus", qfalse );
		weaponInfo->ejectBrassFunc = CG_ShotgunEjectBrass;
		break;

	case WP_GRENADE_LAUNCHER:
		MAKERGB( weaponInfo->flashDlightColor, 1, 0.70f, 0 );

		weaponInfo->missileModel = trap_R_RegisterModel( "models/ammo/grenade1.md3" );
		weaponInfo->missileTrailFunc = CG_GrenadeTrail;
		weaponInfo->wiTrailTime = 600;		// reduced from 700 for competitive visibility
		weaponInfo->trailRadius = 24;		// reduced from 32

		weaponInfo->flashSound[0] = trap_S_RegisterSound( "sound/weapons/grenade/grenlf1a.opus", qfalse );
		cgs.media.grenadeExplosionShader = trap_R_RegisterShader( "grenadeExplosion" );
		break;

	case WP_ROCKET_LAUNCHER:
		MAKERGB( weaponInfo->missileDlightColor, 1, 0.75f, 0 );
		MAKERGB( weaponInfo->flashDlightColor, 1, 0.75f, 0 );

		weaponInfo->missileModel = trap_R_RegisterModel( "models/ammo/rocket/rocket.md3" );
		weaponInfo->missileSound = trap_S_RegisterSound( "sound/weapons/rocket/rockfly.opus", qfalse );
		weaponInfo->missileTrailFunc = CG_RocketTrail;
		weaponInfo->missileDlight = 200;
		weaponInfo->wiTrailTime = 1800;		// reduced from 2000 for competitive visibility
		weaponInfo->trailRadius = 48;		// reduced from 64
		weaponInfo->flashSound[0] = trap_S_RegisterSound( "sound/weapons/rocket/rocklf1a.opus", qfalse );

		cgs.media.rocketExplosionShader = trap_R_RegisterShader( "rocketExplosion" );
		break;

	case WP_LIGHTNING_GUN:
		MAKERGB( weaponInfo->flashDlightColor, 0.6f, 0.6f, 1.0f );

		weaponInfo->readySound = trap_S_RegisterSound( "sound/weapons/melee/fsthum.opus", qfalse );
		weaponInfo->firingPriSound = trap_S_RegisterSound( "sound/weapons/lightning/lg_hum.opus", qfalse );
		weaponInfo->firingSecSound = trap_S_RegisterSound( "sound/weapons/lightning/lg_hum.opus", qfalse );
		weaponInfo->flashSound[0] = trap_S_RegisterSound( "sound/weapons/lightning/lg_fire.opus", qfalse );

		cgs.media.lightningExplosionModel = trap_R_RegisterModel( "models/weaphits/crackle.md3" );
		cgs.media.sfx_lghit1 = trap_S_RegisterSound( "sound/weapons/lightning/lg_hit.opus", qfalse );
		cgs.media.sfx_lghit2 = trap_S_RegisterSound( "sound/weapons/lightning/lg_hit2.opus", qfalse );
		cgs.media.sfx_lghit3 = trap_S_RegisterSound( "sound/weapons/lightning/lg_hit3.opus", qfalse );

		// Particle class for the impact-spark shower. Must run after
		// cgs.media.lightningSparkShader is bound — that handle is
		// registered in CG_RegisterGraphics (cg_main.c) which precedes
		// CG_RegisterWeapon, so it is already valid here.
		CG_RegisterLightningParticleClasses();
		break;

	case WP_RAILGUN:
		MAKERGB( weaponInfo->flashDlightColor, 1, 0.5f, 0 );

		weaponInfo->readySound = trap_S_RegisterSound( "sound/weapons/railgun/rg_hum.opus", qfalse );
		weaponInfo->flashSound[0] = trap_S_RegisterSound( "sound/weapons/railgun/railgf1a.opus", qfalse );

		cgs.media.railExplosionShader = trap_R_RegisterShader( "railExplosion" );
		cgs.media.railRingsShader = trap_R_RegisterShader( "railDisc" );
		cgs.media.railCoreShader = trap_R_RegisterShader( "railCore" );

		// Particle classes for GPU debris + sparks paths. Must run
		// after the rail shaders above are bound — the class defs
		// reference cgs.media.railRingsShader and cgs.media.whiteShader.
		// whiteShader was already bound earlier in CG_Init.
		CG_RegisterRailParticleClasses();
		break;

	case WP_PLASMA_RIFLE:
		// weaponInfo->missileTrailFunc = CG_PlasmaTrail;
        MAKERGB(weaponInfo->flashDlightColor, 1.0f, 0.4f, 1.0f);

		weaponInfo->readySound = trap_S_RegisterSound("sound/weapons/bfg/bfg_hum.opus", qfalse);
        weaponInfo->flashSound[0] = trap_S_RegisterSound("sound/weapons/bfg/bfg_fire.opus", qfalse);
		// weaponInfo->flashSound[0] = trap_S_RegisterSound("sound/weapons/plasma/hyprbf1a.opus", qfalse);
        weaponInfo->trailRadius = 4;
        weaponInfo->wiTrailTime = 100;

		cgs.media.plasmaExplosionShader = trap_R_RegisterShader("plasmaExplosion");
		cgs.media.railRingsShader = trap_R_RegisterShader( "railDisc" );
		break;

	 default:
		MAKERGB( weaponInfo->flashDlightColor, 1, 1, 1 );

		weaponInfo->flashSound[0] = trap_S_RegisterSound( "sound/weapons/rocket/rocklf1a.opus", qfalse );
		break;
	}
}

/*
=================
CG_RegisterItemVisuals

The server says this item is used on this level
=================
*/
void CG_RegisterItemVisuals( int itemNum ) {
	itemInfo_t		*itemInfo;
	gitem_t			*item;

	if ( itemNum < 0 || itemNum >= bg_numItems ) {
		Com_Terminate( TERM_CLIENT_DROP, "CG_RegisterItemVisuals: itemNum %d out of range [0-%d]", itemNum, bg_numItems-1 );
	}

	itemInfo = &cg_items[ itemNum ];
	if ( itemInfo->registered ) {
		return;
	}

	item = &bg_itemlist[ itemNum ];

	memset( itemInfo, 0, sizeof( *itemInfo ) );
	itemInfo->registered = qtrue;

	itemInfo->models[0] = trap_R_RegisterModel( item->world_model[0] );

	itemInfo->icon = item->icon ? trap_R_RegisterShaderNoMip( item->icon ) : 0;

	if ( item->giType == IT_WEAPON ) {
		CG_RegisterWeapon( item->giTag );
	}

	//
	// powerups have an accompanying ring or sphere
	//
	if ( item->giType == IT_POWERUP || item->giType == IT_HEALTH ||
		item->giType == IT_ARMOR || item->giType == IT_HOLDABLE ) {
		if ( item->world_model[1] ) {
			itemInfo->models[1] = trap_R_RegisterModel( item->world_model[1] );
		}
	}
}


/*
========================================================================================

VIEW WEAPON

========================================================================================
*/

/*
=================
CG_MapTorsoToWeaponFrame

=================
*/
static int CG_MapTorsoToWeaponFrame( clientInfo_t *ci, int frame ) {

	// change weapon
	if ( frame >= ci->animations[TORSO_DROP].firstFrame
		&& frame < ci->animations[TORSO_DROP].firstFrame + 9 ) {
		return frame - ci->animations[TORSO_DROP].firstFrame + 6;
	}

	// stand attack
	if ( frame >= ci->animations[TORSO_ATTACK].firstFrame
		&& frame < ci->animations[TORSO_ATTACK].firstFrame + 6 ) {
		return 1 + frame - ci->animations[TORSO_ATTACK].firstFrame;
	}

	// stand attack 2
	if ( frame >= ci->animations[TORSO_ATTACK2].firstFrame
		&& frame < ci->animations[TORSO_ATTACK2].firstFrame + 6 ) {
		return 1 + frame - ci->animations[TORSO_ATTACK2].firstFrame;
	}

	return 0;
}


/*
==============
CG_CalculateWeaponPosition
==============
*/
static void CG_CalculateWeaponPosition( vec3_t origin, vec3_t angles ) {
	float	scale;
	int		delta;
	float	fracsin;

	VectorCopy( cg.refdef.vieworg, origin );
	VectorCopy( cg.refdefViewAngles, angles );

	// on odd legs, invert some angles
	if ( cg.bobcycle & 1 ) {
		scale = -cg.xyspeed;
	} else {
		scale = cg.xyspeed;
	}

	// gun angles from bobbing
	angles[ROLL] += scale * cg.bobfracsin * 0.005;
	angles[YAW] += scale * cg.bobfracsin * 0.01;
	angles[PITCH] += cg.xyspeed * cg.bobfracsin * 0.005;

	// drop the weapon when landing
	delta = cg.time - cg.landTime;
	if ( delta < LAND_DEFLECT_TIME ) {
		origin[2] += cg.landChange*0.25 * delta / LAND_DEFLECT_TIME;
	} else if ( delta < LAND_DEFLECT_TIME + LAND_RETURN_TIME ) {
		origin[2] += cg.landChange*0.25 *
			(LAND_DEFLECT_TIME + LAND_RETURN_TIME - delta) / LAND_RETURN_TIME;
	}

#if 0
	// drop the weapon when stair climbing
	delta = cg.time - cg.stepTime;
	if ( delta < STEP_TIME/2 ) {
		origin[2] -= cg.stepChange*0.25 * delta / (STEP_TIME/2);
	} else if ( delta < STEP_TIME ) {
		origin[2] -= cg.stepChange*0.25 * (STEP_TIME - delta) / (STEP_TIME/2);
	}
#endif

	// idle drift
	scale = cg.xyspeed + 40;
	fracsin = sin( cg.time * 0.001 );
	angles[ROLL] += scale * fracsin * 0.01;
	angles[YAW] += scale * fracsin * 0.01;
	angles[PITCH] += scale * fracsin * 0.01;
}


/*
===============
CG_LightningBolt

Origin will be the exact tag point, which is slightly
different than the muzzle point used for determining hits.
The cent should be the non-predicted cent if it is from the player,
so the endpoint will reflect the simulated strike (lagging the predicted
angle)
===============
*/
static void CG_LightningBolt( centity_t *cent, vec3_t origin ) {
	trace_t  trace;
	vec3_t   forward;
	vec3_t   muzzlePoint, endPoint;
	int      anim;

	if (cent->currentState.weapon != WP_LIGHTNING_GUN) {
		return;
	}

	// interpolate beam between server angle and client prediction
	if (cent->currentState.number == cg.predictedPlayerState.clientNum) {
		vec3_t angle;
		// eser - true lightning
        // might as well fix up true lightning while we're at it
        AngleVectors(cg.predictedPlayerState.viewangles, forward, NULL, NULL);
        VectorCopy(cg.predictedPlayerState.origin, muzzlePoint);
		// eser - true lightning

		for (int i = 0; i < 3; i++) {
			float a = cent->lerpAngles[i] - cg.refdefViewAngles[i];
			if (a > 180) {
				a -= 360;
			}
			if (a < -180) {
				a += 360;
			}

			angle[i] = cg.refdefViewAngles[i] + a;
			if (angle[i] < 0) {
				angle[i] += 360;
			}
			if (angle[i] > 360) {
				angle[i] -= 360;
			}
		}

		// AngleVectors( angle, forward, NULL, NULL );
		// VectorCopy( cent->lerpOrigin, muzzlePoint );
		// VectorCopy( cg.refdef.vieworg, muzzlePoint );

		// *this* is the correct origin for true lightning
        VectorCopy(cg.predictedPlayerState.origin, muzzlePoint);
	} else {
		// other players: use server angles
		AngleVectors( cent->lerpAngles, forward, NULL, NULL );
		VectorCopy( cent->lerpOrigin, muzzlePoint );
	}

	anim = cent->currentState.legsAnim & ~ANIM_TOGGLEBIT;
	if ( anim == LEGS_WALKCR || anim == LEGS_IDLECR ) {
		muzzlePoint[2] += CROUCH_VIEWHEIGHT;
	} else {
		muzzlePoint[2] += DEFAULT_VIEWHEIGHT;
	}

	VectorMA( muzzlePoint, 14, forward, muzzlePoint );

// eser - lightning discharge
    if (trap_CM_PointContents(muzzlePoint, 0) & MASK_WATER) {
        return;
    }
// eser - lightning discharge

	// project forward by the lightning range
	VectorMA( muzzlePoint, LIGHTNING_RANGE, forward, endPoint );

// eser - lightning beams
	// see if it hit a wall — match server-side beam width (±4)
	{
		static vec3_t lgMins = { -4, -4, -4 };
		static vec3_t lgMaxs = {  4,  4,  4 };
		CG_Trace( &trace, muzzlePoint, lgMins, lgMaxs, endPoint,
			cent->currentState.number, MASK_SHOT );
	}
// eser - lightning beams

	// === Beam submission =====================================
	// Phase 5I: legacy DoRailCore + DoRailCoreTapered two-segment
	// shape. Main body uniform width 8 from flash.origin to
	// (trace.endpos - 64u along axis); tail tapers from width 8 to
	// width 0 over the last 64u near the wall. Both segments use
	// the same shader (multi-stage scroll handled engine-side via
	// the per-stage SSBO), 4-axial cross, full white.
	//
	// Short-beam path (length <= 64): the entire beam is the tail;
	// no main-body submission. Avoids degenerate negative-length
	// main body when firing point-blank.
	{
		const float kTaperLength = 64.0f;
		vec3_t      axis;
		float       totalLength;
		vec3_t      axisN;
		vec3_t      splitPoint;
		beamDesc_t  bd;

		VectorSubtract( trace.endpos, origin, axis );
		totalLength = VectorLength( axis );

		if ( totalLength <= kTaperLength ) {
			// Short beam: entire length is tail. Single tapered submission.
			memset( &bd, 0, sizeof( bd ) );
			VectorCopy( origin, bd.start );
			VectorCopy( trace.endpos, bd.end );
			bd.startWidth     = 8.0f;
			bd.endWidth       = 0.0f;          // taper to point at wall
			bd.startColor[0] = bd.startColor[1] = bd.startColor[2] = 1.0f;
			bd.startColor[3] = 1.0f;
			bd.endColor[0]   = bd.endColor[1]   = bd.endColor[2]   = 1.0f;
			bd.endColor[3]   = 1.0f;
			bd.shader         = cgs.media.lightningShaderPrim;
			bd.duration       = 0.0f;
			bd.axialCopies    = 4;
			bd.startEntityNum = -1;
			bd.endEntityNum   = -1;
			bd.uvScroll[0]    = 0.0f;
			bd.uvScroll[1]    = 0.0f;
			trap_R_AddBeamToScene( &bd );
		} else {
			// Long beam: uniform main body + tapered tail.
			VectorCopy( axis, axisN );
			VectorNormalize( axisN );
			VectorMA( trace.endpos, -kTaperLength, axisN, splitPoint );

			// Main body — uniform width.
			memset( &bd, 0, sizeof( bd ) );
			VectorCopy( origin, bd.start );
			VectorCopy( splitPoint, bd.end );
			bd.startWidth     = 8.0f;
			bd.endWidth       = 8.0f;
			bd.startColor[0] = bd.startColor[1] = bd.startColor[2] = 1.0f;
			bd.startColor[3] = 1.0f;
			bd.endColor[0]   = bd.endColor[1]   = bd.endColor[2]   = 1.0f;
			bd.endColor[3]   = 1.0f;
			bd.shader         = cgs.media.lightningShaderPrim;
			bd.duration       = 0.0f;
			bd.axialCopies    = 4;
			bd.startEntityNum = -1;
			bd.endEntityNum   = -1;
			bd.uvScroll[0]    = 0.0f;
			bd.uvScroll[1]    = 0.0f;
			trap_R_AddBeamToScene( &bd );

			// Tail — width tapers 8 → 0 over the last 64 units.
			memset( &bd, 0, sizeof( bd ) );
			VectorCopy( splitPoint, bd.start );
			VectorCopy( trace.endpos, bd.end );
			bd.startWidth     = 8.0f;
			bd.endWidth       = 0.0f;
			bd.startColor[0] = bd.startColor[1] = bd.startColor[2] = 1.0f;
			bd.startColor[3] = 1.0f;
			bd.endColor[0]   = bd.endColor[1]   = bd.endColor[2]   = 1.0f;
			bd.endColor[3]   = 1.0f;
			bd.shader         = cgs.media.lightningShaderPrim;
			bd.duration       = 0.0f;
			bd.axialCopies    = 4;
			bd.startEntityNum = -1;
			bd.endEntityNum   = -1;
			bd.uvScroll[0]    = 0.0f;
			bd.uvScroll[1]    = 0.0f;
			trap_R_AddBeamToScene( &bd );
		}
	}

	// === Impact flare + sparks (independent of beam pipeline) ===
	if ( trace.fraction < 1.0 ) {
		refEntity_t flare;
		vec3_t      angles;
		vec3_t      dir;

		VectorSubtract( trace.endpos, origin, dir );
		VectorNormalize( dir );

		memset( &flare, 0, sizeof( flare ) );
		flare.hModel = cgs.media.lightningExplosionModel;

		VectorMA( trace.endpos, -16, dir, flare.origin );

		// make a random orientation
		angles[0] = rand() % 360;
		angles[1] = rand() % 360;
		angles[2] = rand() % 360;
		AnglesToAxis( angles, flare.axis );
		VectorScale( flare.axis[0], 0.5f, flare.axis[0] );
		VectorScale( flare.axis[1], 0.5f, flare.axis[1] );
		VectorScale( flare.axis[2], 0.5f, flare.axis[2] );
		flare.nonNormalizedAxes = qtrue;
		trap_R_AddRefEntityToScene( &flare );

		// dir points muzzle→wall; negate so sparks fly outward from the surface
		{
			vec3_t sparkDir;
			VectorNegate( dir, sparkDir );
			CG_LightningSparks( trace.endpos, sparkDir );
		}
	}
}

/*
===============
CG_SpawnRailTrail

Origin will be the exact tag point, which is slightly
different than the muzzle point used for determining hits.
===============
*/
static void CG_SpawnRailTrail(centity_t *cent, vec3_t origin) {
    clientInfo_t	*ci;

    if (cent->currentState.weapon != WP_RAILGUN) {
        return;
    }
    if (!cent->pe.railgunFlash) {
        return;
    }
    cent->pe.railgunFlash = qtrue;
    ci = &cgs.clientinfo[cent->currentState.clientNum];
	CG_RailTrail( ci, origin, cent->pe.railgunImpact );
}

/*
======================
CG_BarrelSpinAngle
======================
*/
#define		SPIN_SPEED	0.9
#define		COAST_TIME	1000
static float	CG_BarrelSpinAngle( centity_t *cent ) {
	int		delta;
	float	angle;
	float	speed;

	// EF_FIRING_* stays asserted across WEAPON_DROPPING/WEAPON_RAISING while
	// the trigger is held, so weapon-change detection is the only reliable
	// way to give a freshly-equipped weapon's barrel a clean starting state.
	if ( cent->pe.barrelWeapon != cent->currentState.weapon ) {
		cent->pe.barrelWeapon   = cent->currentState.weapon;
		cent->pe.barrelSpinning = qfalse;
		cent->pe.barrelAngle    = 0.0f;
		cent->pe.barrelTime     = cg.time;
	}

	delta = cg.time - cent->pe.barrelTime;
	if ( cent->pe.barrelSpinning ) {
		angle = cent->pe.barrelAngle + delta * SPIN_SPEED;
	} else {
		if ( delta > COAST_TIME ) {
			delta = COAST_TIME;
		}

		speed = 0.5 * ( SPIN_SPEED + (float)( COAST_TIME - delta ) / COAST_TIME );
		angle = cent->pe.barrelAngle + delta * speed;
	}

	if ( cent->pe.barrelSpinning == !(cent->currentState.eFlags & EF_FIRING_PRI || cent->currentState.eFlags & EF_FIRING_SEC) ) {
		qboolean newSpinning = !!( cent->currentState.eFlags & EF_FIRING_PRI || cent->currentState.eFlags & EF_FIRING_SEC );
		// Don't start spinning while the local player's weapon is still being raised
		// or dropped — EF_FIRING_* leaks across weapon transitions.
		if ( newSpinning && cent->currentState.number == cg.predictedPlayerEntity.currentState.number ) {
			int ws = cg.predictedPlayerState.weaponstate;
			if ( ws == WEAPON_RAISING || ws == WEAPON_DROPPING ) {
				newSpinning = qfalse;
			}
		}
		if ( cent->pe.barrelSpinning != newSpinning ) {
			cent->pe.barrelTime    = cg.time;
			cent->pe.barrelAngle   = AngleMod( angle );
			cent->pe.barrelSpinning = newSpinning;
		}
	}

	return angle;
}


/*
========================
CG_AddWeaponWithPowerups
========================
*/
static void CG_AddWeaponWithPowerups( centity_t *cent, refEntity_t *gun, int powerups ) {
	// add powerup effects
	if ( CG_IsPlayerInvisible(cent) ) {
		gun->customShader = cgs.media.invisShader;
		trap_R_AddRefEntityToScene( gun );
	} else {
		trap_R_AddRefEntityToScene( gun );

		if ( powerups & ( 1 << PW_QUAD ) ) {
			gun->customShader = cgs.media.quadWeaponShader;
			trap_R_AddRefEntityToScene( gun );
		}
		if ( powerups & ( 1 << PW_BERSERK ) ) {
			gun->customShader = cgs.media.berserkWeaponShader;
			trap_R_AddRefEntityToScene( gun );
		}
		if ( powerups & ( 1 << PW_BATTLESUIT ) ) {
			gun->customShader = cgs.media.battleWeaponShader;
			trap_R_AddRefEntityToScene( gun );
		}
		if ( cent->currentState.eFlags & EF_SPAWN_PROTECT ) {
			gun->customShader = cgs.media.spawnProtectShader;
			trap_R_AddRefEntityToScene( gun );
		}
	}
}


/*
=============
CG_AddPlayerWeapon

Used for both the view weapon (ps is valid) and the world modelother character models (ps is NULL)
The main player will have this called for BOTH cases, so effects like light and
sound should only be done on the world model case.
=============
*/
void CG_AddPlayerWeapon( refEntity_t *parent, playerState_t *ps, centity_t *cent, int team ) {
	refEntity_t	gun;
	refEntity_t	barrel;
	refEntity_t	flash;
	vec3_t		angles;
	weapon_t	weaponNum;
	weaponInfo_t	*weapon;
	centity_t	*nonPredictedCent;
	orientation_t	lerped;
    clientInfo_t    *ci;

	weaponNum = cent->currentState.weapon;
    ci = &cgs.clientinfo[cent->currentState.clientNum];

	CG_RegisterWeapon( weaponNum );
	weapon = &cg_weapons[weaponNum];

	// add the weapon
	memset( &gun, 0, sizeof( gun ) );
	VectorCopy( parent->lightingOrigin, gun.lightingOrigin );
	gun.shadowPlane = parent->shadowPlane;
	gun.renderfx = parent->renderfx;

	// set custom shading for railgun refire rate
    if (ps) {
        if (cg.predictedPlayerState.weapon == WP_RAILGUN) {
            float	f;

            if (cg.predictedPlayerState.weaponstate == WEAPON_FIRING) {
                f = (float)cg.predictedPlayerState.weaponTime / bg_attacklist[bg_weaponlist[WP_RAILGUN].attack].reloadTime;
            }
            else {
                f = 0.0;
            }

            gun.shaderRGBA[0] = 255 * (colorSkyBlue[0] + (1 - colorSkyBlue[0]) * f); // 255 * (colorSkyBlue[0] * (1.0f - f));
            gun.shaderRGBA[1] = 255 * (colorSkyBlue[1] + (1 - colorSkyBlue[1]) * f);
            gun.shaderRGBA[2] = 255 * (colorSkyBlue[2] + (1 - colorSkyBlue[2]) * f);
            gun.shaderRGBA[3] = 255;
        }
        else {
            gun.shaderRGBA[0] = 255;
            gun.shaderRGBA[1] = 255;
            gun.shaderRGBA[2] = 255;
            gun.shaderRGBA[3] = 255;
        }
	}

	gun.hModel = weapon->weaponModel;
	if (!gun.hModel) {
		return;
	}

	if ( !ps ) {
		// add weapon ready sound
		cent->pe.lightningFiring = qfalse;
        cent->pe.grappleFiring = qfalse;

        if (cent->currentState.eFlags & EF_GRAPPLE) {
            cent->pe.grappleFiring = qtrue;
        }

		if ( ( cent->currentState.eFlags & EF_FIRING_PRI ) && weapon->firingPriSound ) {
			// lightning gun and guantlet make a different sound when primary fire is held down
			trap_S_AddLoopingSound( cent->currentState.number, cent->lerpOrigin, vec3_origin, weapon->firingPriSound );
			cent->pe.lightningFiring = qtrue;
		} else if ( ( cent->currentState.eFlags & EF_FIRING_SEC ) && weapon->firingSecSound ) {
			// lightning gun and guantlet make a different sound when secondary fire is held down
			trap_S_AddLoopingSound( cent->currentState.number, cent->lerpOrigin, vec3_origin, weapon->firingSecSound );
			cent->pe.lightningFiring = qtrue;
		} else if ( weapon->readySound ) {
			trap_S_AddLoopingSound( cent->currentState.number, cent->lerpOrigin, vec3_origin, weapon->readySound );
		}

        if (weaponNum == WP_RAILGUN) {
            float	f;

            //if (cg.predictedPlayerState.weaponstate == WEAPON_FIRING) {
            //    f = (float)cent->muzzleFlashTime / bg_weaponlist[WP_RAILGUN].reloadTime;
            //}
            //else {
            //    f = 0.0;
            //}
            f = 0.0;

            gun.shaderRGBA[0] = 255 * (colorSkyBlue[0] + (1 - colorSkyBlue[0]) * f); // 255 * (colorSkyBlue[0] * (1.0f - f));
            gun.shaderRGBA[1] = 255 * (colorSkyBlue[1] + (1 - colorSkyBlue[1]) * f);
            gun.shaderRGBA[2] = 255 * (colorSkyBlue[2] + (1 - colorSkyBlue[2]) * f);
            gun.shaderRGBA[3] = 255;
        }
        else {
            gun.shaderRGBA[0] = 255;
            gun.shaderRGBA[1] = 255;
            gun.shaderRGBA[2] = 255;
            gun.shaderRGBA[3] = 255;
        }
	}

	trap_R_LerpTag(&lerped, parent->hModel, parent->oldframe, parent->frame,
		1.0 - parent->backlerp, "tag_weapon");
	VectorCopy(parent->origin, gun.origin);

	VectorMA(gun.origin, lerped.origin[0], parent->axis[0], gun.origin);

	// Make weapon appear left-handed for 2 and centered for 3
	if(ps && cg_drawGun.integer == 2)
		VectorMA(gun.origin, -lerped.origin[1], parent->axis[1], gun.origin);
	else if(!ps || cg_drawGun.integer != 3)
	       	VectorMA(gun.origin, lerped.origin[1], parent->axis[1], gun.origin);

	VectorMA(gun.origin, lerped.origin[2], parent->axis[2], gun.origin);

	MatrixMultiply(lerped.axis, ((refEntity_t *)parent)->axis, gun.axis);
	gun.backlerp = parent->backlerp;

	CG_AddWeaponWithPowerups( cent, &gun, cent->currentState.powerups );

	// add the spinning barrel
	if ( weapon->barrelModel ) {
		memset( &barrel, 0, sizeof( barrel ) );
		VectorCopy( parent->lightingOrigin, barrel.lightingOrigin );
		barrel.shadowPlane = parent->shadowPlane;
		barrel.renderfx = parent->renderfx;

		barrel.hModel = weapon->barrelModel;
		angles[YAW] = 0;
		angles[PITCH] = 0;
		angles[ROLL] = CG_BarrelSpinAngle( cent );
		AnglesToAxis( angles, barrel.axis );

		CG_PositionRotatedEntityOnTag( &barrel, &gun, weapon->weaponModel, "tag_barrel" );

		CG_AddWeaponWithPowerups( cent, &barrel, cent->currentState.powerups );
	} else {
		// No barrel model: invalidate ownership so the next barrel-equipped weapon
		// always gets a clean reset regardless of which weapon came before.
		cent->pe.barrelWeapon = WP_NONE;
	}

	// make sure we aren't looking at cg.predictedPlayerEntity for LG
	nonPredictedCent = &cg_entities[cent->currentState.number];

	// if the index of the nonPredictedCent is not the same as the clientNum
	// then this is a fake player (like on the single player podiums), so
	// go ahead and use the cent
	if( ( nonPredictedCent - cg_entities ) != cent->currentState.clientNum ) {
		nonPredictedCent = cent;
	}

	// add the flash
	if ( ( weaponNum == WP_LIGHTNING_GUN || weaponNum == WP_GAUNTLET ) && ( nonPredictedCent->currentState.eFlags & EF_FIRING_PRI ) ) {
		// continuous flash
	} else if ( ( weaponNum == WP_LIGHTNING_GUN ) && ( nonPredictedCent->currentState.eFlags & EF_FIRING_SEC ) ) {
		// continuous flash
	} else {
		// impulse flash
        if (cg.time - cent->muzzleFlashTime > MUZZLE_FLASH_TIME && !cent->pe.railgunFlash) {
			return;
		}
	}

	memset( &flash, 0, sizeof( flash ) );
	VectorCopy( parent->lightingOrigin, flash.lightingOrigin );
	flash.shadowPlane = parent->shadowPlane;
	flash.renderfx = parent->renderfx;

	flash.hModel = weapon->flashModel;
	if (!flash.hModel) {
		return;
	}
	angles[YAW] = 0;
	angles[PITCH] = 0;
	angles[ROLL] = crandom() * 10;
	AnglesToAxis( angles, flash.axis );

	// colorize the railgun blast
	if ( weaponNum == WP_RAILGUN ) {
		clientInfo_t	*ci;

		ci = &cgs.clientinfo[ cent->currentState.clientNum ];
        flash.shaderRGBA[0] = 255 * colorSkyBlue[0];
        flash.shaderRGBA[1] = 255 * colorSkyBlue[1];
        flash.shaderRGBA[2] = 255 * colorSkyBlue[2];
	}

	CG_PositionRotatedEntityOnTag( &flash, &gun, weapon->weaponModel, "tag_flash");
	trap_R_AddRefEntityToScene( &flash );

	if ( ps || cg.renderingThirdPerson ||
		cent->currentState.number != cg.predictedPlayerState.clientNum ) {
		// add lightning bolt
		CG_LightningBolt( nonPredictedCent, flash.origin );

		// add rail trail
		CG_SpawnRailTrail( cent, flash.origin );

		if ( weapon->flashDlightColor[0] || weapon->flashDlightColor[1] || weapon->flashDlightColor[2] ) {
			trap_R_AddLightToScene( flash.origin, 300 + (rand()&31), weapon->flashDlightColor[0],
				weapon->flashDlightColor[1], weapon->flashDlightColor[2] );
		}
	}
}

/*
==============
CG_AddViewWeapon

Add the weapon, and flash for the player's view
==============
*/
int cg_weapon_positions[][3][3] = {
	{ // WP_NONE
		/* Right  */ {  0,   0,  0 },
		/* Center */ {  0,   0,  0 },
		/* Left   */ {  0,   0,  0 },
	},
	{ // WP_GAUNTLET
		/* Right  */ {  1,  -6, -2 },
		/* Center */ {  1,   1, -2 },
		/* Left   */ {  1,   7, -2 },
	},
	{ // WP_MACHINEGUN
		/* Right  */ { -2,  -6, -4 },
		/* Center */ { -2,   1, -4 },
		/* Left   */ { -2,   5, -4 },
	},
	{ // WP_SHOTGUN
		/* Right  */ { -2,  -7, -2 },
		/* Center */ { -2,  -2, -2 },
		/* Left   */ { -2,   2, -2 },
	},
	{ // WP_GRENADE_LAUNCHER
		/* Right  */ { -1,  -6, -4 },
		/* Center */ { -1,  -3, -4 },
		/* Left   */ { -1, -11, -4 },
	},
	{ // WP_ROCKET_LAUNCHER
		/* Right  */ { -7,  -5, -4 },
		/* Center */ { -7,   4, -4 },
		/* Left   */ { -7,  12, -4 },
	},
	{ // WP_LIGHTNING_GUN
		/* Right  */ { -7,  -5, -4 },
		/* Center */ { -7,   0, -4 },
		/* Left   */ { -7,   4, -4 },
	},
	{ // WP_RAILGUN
		/* Right  */ { -1,  -7, -2 },
		/* Center */ { -1,   2, -2 },
		/* Left   */ { -1,  10, -2 },
	},
	{ // WP_PLASMA_RIFLE
		/* Right  */ { -7,  -7, -3 },
		/* Center */ { -7,   0, -3 },
		/* Left   */ { -7,   6, -3 },
	},
};

void CG_AddViewWeapon( playerState_t *ps ) {
	refEntity_t		hand;
	centity_t		*cent;
	clientInfo_t	*ci;
	float			fovOffset;
	vec3_t			angles;
	weaponInfo_t	*weapon;
    float 			gen_gunx, gen_guny, gen_gunz;
	int				gun_position;

	if ( ps->persistant[PERS_TEAM] == TEAM_SPECTATOR ) {
		return;
	}

	if ( ps->pm_type == PM_INTERMISSION ) {
		return;
	}

	// no gun if in third person view or a camera is active
	//if ( cg.renderingThirdPerson || cg.cameraMode) {
	if ( cg.renderingThirdPerson ) {
		return;
	}

	// allow the gun to be completely removed
	if ( !cg_drawGun.integer ) {
		vec3_t		origin;

		if ( cg.predictedPlayerState.eFlags & EF_FIRING_PRI || cg.predictedPlayerState.eFlags & EF_FIRING_SEC ) {
			// special hack for lightning gun: beam emerges from
			// below the eye-line when the gun model is hidden.
			// Near-camera-clip distortion is handled engine-side
			// in beam.vert's start vertex guard.
			VectorCopy( cg.refdef.vieworg, origin );
			VectorMA( origin, -8, cg.refdef.viewaxis[2], origin );
			CG_LightningBolt( &cg_entities[ps->clientNum], origin );
		}
		return;
	}

	// don't draw if testing a gun model
	if ( cg.testGun ) {
		return;
	}

	switch (cg_drawGun.integer) {
		case 1:
			gun_position = 0;
			break;
		case 2:
			gun_position = 1;
			break;
		case 3:
			gun_position = 2;;
			break;
		default:
			gun_position = 0;;
			break;
		}

	gen_gunx = cg_gunX.value + cg_weapon_positions[ps->weapon][gun_position][0];
	gen_guny = cg_gunY.value + cg_weapon_positions[ps->weapon][gun_position][1];
	gen_gunz = cg_gunZ.value + cg_weapon_positions[ps->weapon][gun_position][2];

	// drop gun lower at higher fov
	if ( cg_fov.integer > 90 ) {
		fovOffset = -0.2 * ( cg_fov.integer - 90 );
	} else {
		fovOffset = 0;
	}

	cent = &cg.predictedPlayerEntity;	// &cg_entities[cg.snap->ps.clientNum];
	CG_RegisterWeapon( ps->weapon );
	weapon = &cg_weapons[ ps->weapon ];

	memset (&hand, 0, sizeof(hand));

	// set up gun position
	CG_CalculateWeaponPosition( hand.origin, angles );

	VectorMA( hand.origin, gen_gunx, cg.refdef.viewaxis[0], hand.origin );
	VectorMA( hand.origin, gen_guny, cg.refdef.viewaxis[1], hand.origin );
	VectorMA( hand.origin, (gen_gunz+fovOffset), cg.refdef.viewaxis[2], hand.origin );

#if FEAT_SHOTGUN_PUMP
	// Doom-style shotgun pump: recoil → dwell → pump back → pump forward → settle
	// Doom ratio (tics): 7:14:13:7 — scaled to ~70% for Q3's pace
	if ( ps->weapon == WP_SHOTGUN && cg.sgPumpTime ) {
		int		delta = cg.time - cg.sgPumpTime;

		#define SG_RECOIL_MS		100		// phase 1: kick up from blast
		#define SG_DWELL_MS			140		// phase 2: hold recoil, flash fades (the pause)
		#define SG_PUMPBACK_MS		250		// phase 3: pull back toward player (ch-)
		#define SG_PUMPFWD_MS		250		// phase 4: push forward (-chk)
		#define SG_SETTLE_MS		160		// phase 5: ease to ready
		#define SG_PUMP_TOTAL_MS	(SG_RECOIL_MS + SG_DWELL_MS + SG_PUMPBACK_MS + SG_PUMPFWD_MS + SG_SETTLE_MS)

		#define SG_RECOIL_PITCH		(-6.0f)		// recoil pitch (subtle)
		#define SG_PUMP_PITCH		(-10.0f)	// pump pitch (more pronounced)
		#define SG_PUMP_DROP		(-2.5f)		// drop during pump
		#define SG_PUMP_PULLBACK	(-3.5f)		// pull toward camera during pump

		if ( delta < SG_PUMP_TOTAL_MS ) {
			float kickPitch = 0.0f;
			float pumpPitch = 0.0f;
			float pumpPull = 0.0f;
			float pumpDrop = 0.0f;
			float t;

			if ( delta < SG_RECOIL_MS ) {
				// phase 1: quick recoil kick — pitch only, no pullback
				t = (float)delta / SG_RECOIL_MS;
				kickPitch = t * t;
			} else if ( delta < SG_RECOIL_MS + SG_DWELL_MS ) {
				// phase 2: dwell — recoil settling, the critical pause
				t = (float)( delta - SG_RECOIL_MS ) / SG_DWELL_MS;
				kickPitch = 1.0f - t;  // recoil fading out linearly
			} else if ( delta < SG_RECOIL_MS + SG_DWELL_MS + SG_PUMPBACK_MS ) {
				// phase 3: pump back — pitch up + pull toward player
				t = (float)( delta - SG_RECOIL_MS - SG_DWELL_MS ) / SG_PUMPBACK_MS;
				t = t * ( 2.0f - t );  // ease-out (decelerating into position)
				pumpPitch = t;
				pumpPull = t;
				pumpDrop = t;
			} else if ( delta < SG_RECOIL_MS + SG_DWELL_MS + SG_PUMPBACK_MS + SG_PUMPFWD_MS ) {
				// phase 4: pump forward — push back out
				t = (float)( delta - SG_RECOIL_MS - SG_DWELL_MS - SG_PUMPBACK_MS ) / SG_PUMPFWD_MS;
				t = 1.0f - t * t;  // ease-in (accelerating away)
				pumpPitch = t;
				pumpPull = t;
				pumpDrop = t;
			} else {
				// phase 5: settle — final ease to ready
				t = (float)( delta - SG_RECOIL_MS - SG_DWELL_MS - SG_PUMPBACK_MS - SG_PUMPFWD_MS ) / SG_SETTLE_MS;
				pumpPitch = 0.0f;
				pumpPull = 0.0f;
				pumpDrop = 0.0f;
			}

			angles[PITCH] += SG_RECOIL_PITCH * kickPitch + SG_PUMP_PITCH * pumpPitch;
			VectorMA( hand.origin, SG_PUMP_DROP * pumpDrop, cg.refdef.viewaxis[2], hand.origin );
			VectorMA( hand.origin, SG_PUMP_PULLBACK * pumpPull, cg.refdef.viewaxis[0], hand.origin );
		} else {
			cg.sgPumpTime = 0;
		}
	}
#endif

	AnglesToAxis( angles, hand.axis );

	// map torso animations to weapon animations
	if ( cg_gun_frame.integer ) {
		// development tool
		hand.frame = hand.oldframe = cg_gun_frame.integer;
		hand.backlerp = 0;
	} else {
		// get clientinfo for animation map
		ci = &cgs.clientinfo[ cent->currentState.clientNum ];
		hand.frame = CG_MapTorsoToWeaponFrame( ci, cent->pe.torso.frame );
		hand.oldframe = CG_MapTorsoToWeaponFrame( ci, cent->pe.torso.oldFrame );
		hand.backlerp = cent->pe.torso.backlerp;
	}

	hand.hModel = weapon->handsModel;
	hand.renderfx = RF_DEPTHHACK | RF_FIRST_PERSON | RF_MINLIGHT;

	// add everything onto the hand
	CG_AddPlayerWeapon( &hand, ps, &cg.predictedPlayerEntity, ps->persistant[PERS_TEAM] );
}

/*
==============================================================================

WEAPON SELECTION

==============================================================================
*/

/*
===============
CG_WeaponSelect
===============
*/
qboolean CG_WeaponSelect(int i) {
    char string[64];
    char varname[128];

    cg.weaponSelectTime = cg.time;
    cg.weaponSelect = i;

    Com_sprintf(varname, sizeof(varname), "cg_weaponConfig_%s", bg_weaponlist[i].shortname);
    trap_Cvar_VariableStringBuffer(varname, string, 32);
    if (string[0]) {
        trap_SendConsoleCommand(va("vstr %s;", varname));
    }

    return qtrue;

}

/*
===============
CG_WeaponSelectable
===============
*/
static qboolean CG_WeaponSelectable( int i ) {
	if ( !cg.snap->ps.ammo[i] ) {
		return qfalse;
	}
	if ( ! (cg.snap->ps.stats[ STAT_WEAPONS ] & ( 1 << i ) ) ) {
		return qfalse;
	}

	return qtrue;
}

/*
===============
CG_NextWeapon_f
===============
*/
void CG_NextWeapon_f( void ) {
    if (!cg.snap) {
        return;
    }

    if (cg.snap->ps.pm_flags & PMF_FOLLOW) {
        return;
    }

    cg.weaponSelectTime = cg.time;

    for (int i = cg.weaponSelect + 1; i < WP_NUM_WEAPONS; i++) {
        if (!bg_weaponlist[i].switchOnCycle) {
            continue;
        }
        if (CG_WeaponSelectable(i)) {
            CG_WeaponSelect(i);
            break;
        }
    }
}

/*
===============
CG_PrevWeapon_f
===============
*/
void CG_PrevWeapon_f( void ) {
    if (!cg.snap) {
        return;
    }
    if (cg.snap->ps.pm_flags & PMF_FOLLOW) {
        return;
    }

    cg.weaponSelectTime = cg.time;

    for (int i = cg.weaponSelect - 1; i > WP_NONE; i--) {
        if (!bg_weaponlist[i].switchOnCycle) {
            continue;
        }
        if (CG_WeaponSelectable(i)) {
            CG_WeaponSelect(i);
            break;
        }
    }
}

/*
===============
CG_Weapon_f
===============
*/
void CG_Weapon_f( void ) {
    int		num;

    if (!cg.snap) {
        return;
    }
    if (cg.snap->ps.pm_flags & PMF_FOLLOW) {
        return;
    }

    num = atoi(CG_Argv(1));

    if (num <= WP_NONE || num >= WP_NUM_WEAPONS) {
        return;
    }

    cg.weaponSelectTime = cg.time;
    if (!cg_switchToEmpty.integer && !cg.snap->ps.ammo[num]) {
        return;
    }
    if (!(cg.snap->ps.stats[STAT_WEAPONS] & (1 << num))) {
        return;		// don't have the weapon
    }

    CG_WeaponSelect(num);
}

/*
===============
CG_WeaponGrabbed_f

Switch to the last weapon picked up.
===============
*/
void CG_WeaponGrabbed_f( void ) {
	if ( !cg.lastGrabbedWeapon ) {
		return;
	}
	if ( !CG_WeaponSelectable( cg.lastGrabbedWeapon ) ) {
		return;
	}
	CG_WeaponSelect( cg.lastGrabbedWeapon );
}

/*
===================
CG_OutOfAmmoChange

The current weapon has just run out of ammo
===================
*/
void CG_OutOfAmmoChange( void ) {
    cg.weaponSelectTime = cg.time;

    for (int i = WP_NUM_WEAPONS - 1; i > WP_NONE; i--) {
        if (!bg_weaponlist[i].switchOnOutOfAmmo) {
            continue;
        }
        if (CG_WeaponSelectable(i)) {
            CG_WeaponSelect(i);
            break;
        }
    }
}



/*
===================================================================================================

WEAPON EVENTS

===================================================================================================
*/

/*
================
CG_FireWeapon

Caused by an EV_FIRE_WEAPON_* events
================
*/
void CG_FireWeapon( centity_t *cent ) {
	entityState_t *ent;
	int				c;
	weaponInfo_t	*weap;

	ent = &cent->currentState;
	if ( ent->weapon == WP_NONE ) {
		return;
	}
	if ( ent->weapon >= WP_NUM_WEAPONS ) {
		Com_Terminate( TERM_CLIENT_DROP, "CG_FireWeapon: ent->weapon >= WP_NUM_WEAPONS" );
		return;
	}
	weap = &cg_weapons[ ent->weapon ];

	// mark the entity as muzzle flashing, so when it is added it will
	// append the flash to the weapon model
	cent->muzzleFlashTime = cg.time;

	// lightning gun only does this this on initial press
	if ( ent->weapon == WP_LIGHTNING_GUN ) {
		if ( cent->pe.lightningFiring ) {
			return;
		}
	}

	if( ent->weapon == WP_RAILGUN ) {
		cent->pe.railFireTime = cg.time;
	}

#if FEAT_SHOTGUN_PUMP
	if ( ent->weapon == WP_SHOTGUN ) {
		cg.sgPumpTime = cg.time;
	}
#endif

	// play quad sound if needed
	if ( cent->currentState.powerups & ( 1 << PW_QUAD ) ) {
		trap_S_StartSound (NULL, cent->currentState.number, CHAN_ITEM, cgs.media.quadSound );
	}

	// play berserk sound if needed
	if ( cent->currentState.powerups & ( 1 << PW_BERSERK ) ) {
		trap_S_StartSound (NULL, cent->currentState.number, CHAN_ITEM, cgs.media.berserkSound );
	}

	// play a sound
	for ( c = 0 ; c < 4 ; c++ ) {
		if ( !weap->flashSound[c] ) {
			break;
		}
	}
	if ( c > 0 ) {
		c = rand() % c;
		if ( weap->flashSound[c] )
		{
			trap_S_StartSound( NULL, ent->number, CHAN_WEAPON, weap->flashSound[c] );
		}
	}

	// do brass ejection
	if ( weap->ejectBrassFunc ) {
		weap->ejectBrassFunc( cent );
	}
}


/*
=================
CG_MissileHitWall

Caused by an EV_MISSILE_MISS event, or directly by local bullet tracing
=================
*/
void CG_MissileHitWall( int pType, int clientNum, vec3_t origin, vec3_t dir, impactSound_t soundType, int sourceEntityNum ) {
	qhandle_t		mod;
	qhandle_t		mark;
	qhandle_t		shader;
	sfxHandle_t		sfx;
	float			radius;
	float			light;
	vec3_t			lightColor;
	localEntity_t	*le;
	int				r;
	qboolean		alphaFade;
	qboolean		isSprite;
	int				duration;
	vec3_t			sprOrg;
	vec3_t			sprVel;

	mod = 0;
	shader = 0;
	light = 0;
	lightColor[0] = 1;
	lightColor[1] = 1;
	lightColor[2] = 0;

	// set defaults
	isSprite = qfalse;
	duration = 600;

	// hitscan impacts at a liquid surface: show a splash and skip the wall mark.
	// explosive projectiles (grenade, rocket, etc.) detonate normally underwater.
	if ( pType == PROJ_NONE ) {
		int hitContents = CG_PointContents( origin, 0 );
		if ( hitContents & ( CONTENTS_WATER | CONTENTS_SLIME | CONTENTS_LAVA ) ) {
			CG_BubbleTrail( origin, origin, 32 );
			return;
		}
	}

	switch ( pType ) {
	default:
	case PROJ_NONE:
		// generic fallback / no explosion
		r = rand() & 3;
		if ( r < 2 ) {
			sfx = cgs.media.sfx_lghit2;
		} else if ( r == 2 ) {
			sfx = cgs.media.sfx_lghit1;
		} else {
			sfx = cgs.media.sfx_lghit3;
		}
		mark = cgs.media.holeMarkShader;
		radius = 12;
		break;
	case PROJ_GRENADE:
		mod = cgs.media.dishFlashModel;
		shader = cgs.media.grenadeExplosionShader;
		sfx = cgs.media.sfx_rockexp;
		mark = cgs.media.burnMarkShader;
		radius = 64;
		light = 300;
		isSprite = qtrue;
#if FEAT_EARTHQUAKE_SYSTEM
		CG_AddEarthquake( origin, 600, 0.6f, 0, 0.5f, 300 );
#endif
		break;
	case PROJ_ROCKET:
		mod = cgs.media.dishFlashModel;
		shader = cgs.media.rocketExplosionShader;
		sfx = cgs.media.sfx_rockexp;
		mark = cgs.media.burnMarkShader;
		radius = 64;
		light = 300;
		isSprite = qtrue;
		duration = 1000;
		lightColor[0] = 1;
		lightColor[1] = 0.75;
		lightColor[2] = 0.0;
		// eser - removed explosion sprite animation
		// VectorMA( origin, 24, dir, sprOrg );
		// VectorScale( dir, 64, sprVel );

		// CG_ParticleExplosion( "explode1", sprOrg, sprVel, 1400, 20, 30 );
#if FEAT_EARTHQUAKE_SYSTEM
		CG_AddEarthquake( origin, 480, 0.6f, 0, 0.5f, 300 );
#endif
		break;
	case PROJ_RAILGUN:
		mod = cgs.media.ringFlashModel;
		shader = cgs.media.railExplosionShader;
		//sfx = cgs.media.sfx_railg;
		sfx = cgs.media.sfx_plasmaexp;
		mark = cgs.media.energyMarkShader;
		radius = 24;
		break;
	case PROJ_PLASMA:
		mod = cgs.media.ringFlashModel;
		shader = cgs.media.plasmaExplosionShader;
		sfx = cgs.media.sfx_plasmaexp;
		mark = cgs.media.energyMarkShader;
		radius = 8;
		break;
	case PROJ_SPIKE:
	case PROJ_LASER:
		mod = cgs.media.bulletFlashModel;
		shader = cgs.media.bulletExplosionShader;
		mark = cgs.media.bulletMarkShader;
		sfx = cgs.media.sfx_ric1;
		radius = 4;
		break;
	case PROJ_LAVABALL:
		mod = cgs.media.dishFlashModel;
		shader = cgs.media.grenadeExplosionShader;
		sfx = cgs.media.sfx_rockexp;
		mark = cgs.media.burnMarkShader;
		radius = 48;
		light = 200;
		isSprite = qtrue;
		break;
	case PROJ_SHOTGUN:
		mod = cgs.media.bulletFlashModel;
		shader = cgs.media.bulletExplosionShader;
		mark = cgs.media.bulletMarkShader;
		sfx = 0;
		radius = 4;
		break;

	case PROJ_MACHINEGUN:
		mod = cgs.media.bulletFlashModel;
		shader = cgs.media.bulletExplosionShader;
		mark = cgs.media.bulletMarkShader;

		r = rand() & 3;
		if ( r == 0 ) {
			sfx = cgs.media.sfx_ric1;
		} else if ( r == 1 ) {
			sfx = cgs.media.sfx_ric2;
		} else {
			sfx = cgs.media.sfx_ric3;
		}
        /*
        if( soundType == IMPACTSOUND_FLESH ) {
        sfx = cgs.media.sfx_chghitflesh;
        } else if( soundType == IMPACTSOUND_METAL ) {
        sfx = cgs.media.sfx_chghitmetal;
        } else {
        sfx = cgs.media.sfx_chghit;
        }
        */

		radius = 8;
		break;
	}

	if ( sfx ) {
		trap_S_StartSound( origin, sourceEntityNum, CHAN_AUTO, sfx );
	}

	//
	// create the explosion
	//
	if ( mod ) {
		le = CG_MakeExplosion( origin, dir,
							   mod,	shader,
							   duration, isSprite );
		le->light = light;
		VectorCopy( lightColor, le->lightColor );
		if ( pType == PROJ_RAILGUN ) {
			// colorize with client color
			VectorCopy( cgs.clientinfo[clientNum].color1, le->color );
			le->refEntity.shaderRGBA[0] = le->color[0] * 0xff;
			le->refEntity.shaderRGBA[1] = le->color[1] * 0xff;
			le->refEntity.shaderRGBA[2] = le->color[2] * 0xff;
			le->refEntity.shaderRGBA[3] = 0xff;
		} else if ( pType == PROJ_PLASMA ) {
			float scale = 0.5f; // halve the explosion size
			VectorScale( le->refEntity.axis[0], scale, le->refEntity.axis[0] );
			VectorScale( le->refEntity.axis[1], scale, le->refEntity.axis[1] );
			VectorScale( le->refEntity.axis[2], scale, le->refEntity.axis[2] );
		}
	}

	//
	// impact mark
	//
	alphaFade = (mark == cgs.media.energyMarkShader);	// plasma fades alpha, all others fade color
	if ( pType == PROJ_RAILGUN ) {
        CG_ImpactMark(mark, origin, dir, random() * 360, colorSkyBlue[0], colorSkyBlue[1], colorSkyBlue[2], 1, alphaFade, radius, qfalse);
	} else {
		CG_ImpactMark( mark, origin, dir, random()*360, 1,1,1,1, alphaFade, radius, qfalse );
	}

// eser - explosions
    CG_ExplosionParticles(pType, origin);
// eser - explosions
}


/*
=================
CG_MissileHitPlayer
=================
*/
void CG_MissileHitPlayer( int pType, vec3_t origin, vec3_t dir, int entityNum ) {
	CG_Bleed( origin, entityNum );

#if FEAT_IMPACT_SPARKS
	if ( cg_impactSparks.integer ) {
		CG_ImpactSparks( origin, dir );
	}
#endif

	// some weapons will make an explosion with the blood, while
	// others will just make the blood
	switch ( pType ) {
	case PROJ_GRENADE:
	case PROJ_ROCKET:
	case PROJ_LAVABALL:
		CG_MissileHitWall( pType, 0, origin, dir, IMPACTSOUND_FLESH, ENTITYNUM_WORLD );
		break;
	default:
		break;
	}
}

// eser - explosions
/*
=================
CG_ExplosionParticles
=================
*/
void CG_ExplosionParticles(int pType, vec3_t origin) {
    int number = 32; // number of particles
    int jump = 50; // amount to nudge the particles trajectory vector up by
    int speed = 300; // speed of particles
    int light = 50; // amount of light for each particle
    vec4_t lColor; // color of light for each particle
    qhandle_t shader; // shader to use for the particles
    vec3_t randVec, tempVec;
    lColor[0] = 1.0f;
    lColor[1] = 1.0f;
    lColor[2] = 1.0f;
    lColor[3] = 1.0f; // alpha

    switch (pType) {
    case PROJ_ROCKET:
        number = 128;
        jump = 70;
        light = 100;
        lColor[0] = 1.0f;
        lColor[1] = 0.56f;
        lColor[2] = 0.0f;
        shader = cgs.media.sparkShader;
        break;

    case PROJ_GRENADE:
    case PROJ_LAVABALL:
        number = 64;
        jump = 60;
        light = 100;
        lColor[0] = 1.0f;
        lColor[1] = 0.56f;
        lColor[2] = 0.0f;
        shader = cgs.media.sparkShader;
        break;

    default:
        return;
    }

    for (int index = 0; index < number; index++) {
        localEntity_t *le;
        refEntity_t *re;

        le = CG_AllocLocalEntity(); //allocate a local entity
        re = &le->refEntity;
        le->leFlags = LEF_PUFF_DONT_SCALE; //don't change the particle size
        le->leType = LE_MOVE_SCALE_FADE; // particle should fade over time
        le->startTime = cg.time; // set the start time of the particle to the current time
        le->endTime = cg.time + 3000 + random() * 250; //set the end time
        le->lifeRate = 1.0 / (le->endTime - le->startTime);
        re = &le->refEntity;
        re->shaderTime.f =cg.time / 1000.0f;
        re->reType = RT_SPRITE;
        re->rotation = 0;
        re->radius = 3;
        re->customShader = shader;
        re->shaderRGBA[0] = 0xff;
        re->shaderRGBA[1] = 0xff;
        re->shaderRGBA[2] = 0xff;
        re->shaderRGBA[3] = 0xff;
        le->light = light;
        VectorCopy(lColor, le->lightColor);
        le->color[3] = 1.0;
        le->pos.trType = TR_GRAVITY; // moves in a gravity affected arc
        le->pos.trTime = cg.time;
        VectorCopy(origin, le->pos.trBase);

        tempVec[0] = crandom(); //between 1 and -1
        tempVec[1] = crandom();
        tempVec[2] = crandom();
        VectorNormalize(tempVec);
        VectorScale(tempVec, speed, randVec);
        randVec[2] += jump; //nudge the particles up a bit
        VectorCopy(randVec, le->pos.trDelta);
    }
}
// eser - explosions


/*
============================================================================

SHOTGUN TRACING

============================================================================
*/

/*
================
CG_ShotgunPellet
================
*/
static void CG_ShotgunPellet( vec3_t start, vec3_t end, int skipNum ) {
	trace_t		tr;
	int sourceContentType, destContentType;

	CG_Trace( &tr, start, NULL, NULL, end, skipNum, MASK_SHOT );

	sourceContentType = CG_PointContents( start, 0 );
	destContentType = CG_PointContents( tr.endpos, 0 );

	// FIXME: should probably move this cruft into CG_BubbleTrail
	if ( sourceContentType == destContentType ) {
		if ( sourceContentType & CONTENTS_WATER ) {
			CG_BubbleTrail( start, tr.endpos, 32 );
		}
	} else if ( sourceContentType & CONTENTS_WATER ) {
		trace_t trace;

		trap_CM_BoxTrace( &trace, end, start, NULL, NULL, 0, CONTENTS_WATER );
		CG_BubbleTrail( start, trace.endpos, 32 );
	} else if ( destContentType & CONTENTS_WATER ) {
		trace_t trace;

		trap_CM_BoxTrace( &trace, start, end, NULL, NULL, 0, CONTENTS_WATER );
		CG_BubbleTrail( tr.endpos, trace.endpos, 32 );
	} else {
		// bullet crosses a water volume without starting or ending in it
		trace_t waterTr;
		trap_CM_BoxTrace( &waterTr, start, tr.endpos, NULL, NULL, 0, CONTENTS_WATER );
		if ( waterTr.fraction < 1.0f ) {
			CG_BubbleTrail( waterTr.endpos, tr.endpos, 32 );
		}
	}

	if (  tr.surfaceFlags & SURF_NOIMPACT ) {
		return;
	}

	if ( cg_entities[tr.entityNum].currentState.eType == ET_PLAYER ) {
		CG_MissileHitPlayer( PROJ_SHOTGUN, tr.endpos, tr.plane.normal, tr.entityNum );
	} else {
		if ( tr.surfaceFlags & SURF_NOIMPACT ) {
			// SURF_NOIMPACT will not make a flame puff or a mark
			return;
		}
		if ( tr.surfaceFlags & SURF_METALSTEPS ) {
			CG_MissileHitWall( PROJ_SHOTGUN, 0, tr.endpos, tr.plane.normal, IMPACTSOUND_METAL, ENTITYNUM_WORLD );
		} else {
			CG_MissileHitWall( PROJ_SHOTGUN, 0, tr.endpos, tr.plane.normal, IMPACTSOUND_DEFAULT, ENTITYNUM_WORLD );
		}
	}
}

/*
================
CG_ShotgunPattern

Perform the same traces the server did to locate the
hit splashes
================
*/
static void CG_ShotgunPattern( vec3_t origin, vec3_t origin2, int seed, int otherEntNum ) {
	int			i;
	float		r, u;
	vec3_t		end;
	vec3_t		forward, right, up;

	// derive the right and up vectors from the forward vector, because
	// the client won't have any other information
	VectorNormalize2( origin2, forward );
	PerpendicularVector( right, forward );
	CrossProduct( forward, right, up );

#if FEAT_SHOTGUN_PATTERN
	{
		float rotation = ( seed / 256.0f ) * 2.0f * M_PI;
		float spreadScale = DEFAULT_SHOTGUN_SPREAD * 16;

		for ( i = 0; i < DEFAULT_SHOTGUN_COUNT; i++ ) {
			float angle = bg_shotgunPattern[i].angle + rotation;
			float radius = bg_shotgunPattern[i].radius * spreadScale;

			r = radius * cos( angle );
			u = radius * sin( angle );

			VectorMA( origin, 8192 * 16, forward, end );
			VectorMA( end, r, right, end );
			VectorMA( end, u, up, end );

			CG_ShotgunPellet( origin, end, otherEntNum );
		}
	}
#else
	// generate the "random" spread pattern
	for ( i = 0 ; i < DEFAULT_SHOTGUN_COUNT ; i++ ) {
		r = Q_crandom( &seed ) * DEFAULT_SHOTGUN_SPREAD * 16;
		u = Q_crandom( &seed ) * DEFAULT_SHOTGUN_SPREAD * 16;
		VectorMA( origin, 8192 * 16, forward, end);
		VectorMA (end, r, right, end);
		VectorMA (end, u, up, end);

		CG_ShotgunPellet( origin, end, otherEntNum );
	}
#endif
}

/*
==============
CG_ShotgunFire
==============
*/
void CG_ShotgunFire( entityState_t *es ) {
	vec3_t	v;
	int		contents;

	VectorSubtract( es->origin2, es->pos.trBase, v );
	VectorNormalize( v );
	VectorScale( v, 32, v );
	VectorAdd( es->pos.trBase, v, v );
	if ( cgs.glconfig.hardwareType != GLHW_RAGEPRO ) {
		// ragepro can't alpha fade, so don't even bother with smoke
		vec3_t			up;

		contents = CG_PointContents( es->pos.trBase, 0 );
		if ( !( contents & CONTENTS_WATER ) ) {
			VectorSet( up, 0, 0, 8 );
			CG_SmokePuff( v, up, 16, 1, 1, 1, 0.20f, 800, cg.time, 0, LEF_PUFF_DONT_SCALE, cgs.media.shotgunSmokePuffShader );
		}
	}
	CG_ShotgunPattern( es->pos.trBase, es->origin2, es->eventParm, es->otherEntityNum );
}

/*
==============
CG_ShotgunPatternSpread

Same as CG_ShotgunPattern but with a configurable spread value.
==============
*/
static void CG_ShotgunPatternSpread( vec3_t origin, vec3_t origin2, int seed, int otherEntNum, int spread ) {
	int			i;
	float		r, u;
	vec3_t		end;
	vec3_t		forward, right, up;

	VectorNormalize2( origin2, forward );
	PerpendicularVector( right, forward );
	CrossProduct( forward, right, up );

#if FEAT_SHOTGUN_PATTERN
	{
		float rotation = ( seed / 256.0f ) * 2.0f * M_PI;
		float spreadScale = spread * 16;

		for ( i = 0; i < DEFAULT_SHOTGUN_COUNT; i++ ) {
			float angle = bg_shotgunPattern[i].angle + rotation;
			float radius = bg_shotgunPattern[i].radius * spreadScale;

			r = radius * cos( angle );
			u = radius * sin( angle );

			VectorMA( origin, 8192 * 16, forward, end );
			VectorMA( end, r, right, end );
			VectorMA( end, u, up, end );

			CG_ShotgunPellet( origin, end, otherEntNum );
		}
	}
#else
	for ( i = 0 ; i < DEFAULT_SHOTGUN_COUNT ; i++ ) {
		r = Q_crandom( &seed ) * spread * 16;
		u = Q_crandom( &seed ) * spread * 16;
		VectorMA( origin, 8192 * 16, forward, end);
		VectorMA (end, r, right, end);
		VectorMA (end, u, up, end);

		CG_ShotgunPellet( origin, end, otherEntNum );
	}
#endif
}

/*
==============
CG_ShotgunFireWide — double-blast uses wider spread
==============
*/
void CG_ShotgunFireWide( entityState_t *es ) {
	vec3_t	v;
	int		contents;

	VectorSubtract( es->origin2, es->pos.trBase, v );
	VectorNormalize( v );
	VectorScale( v, 32, v );
	VectorAdd( es->pos.trBase, v, v );
	if ( cgs.glconfig.hardwareType != GLHW_RAGEPRO ) {
		vec3_t			up;

		contents = CG_PointContents( es->pos.trBase, 0 );
		if ( !( contents & CONTENTS_WATER ) ) {
			VectorSet( up, 0, 0, 8 );
			// slightly bigger smoke puff for the sawed-off feel
			CG_SmokePuff( v, up, 24, 1, 1, 1, 0.25f, 900, cg.time, 0, LEF_PUFF_DONT_SCALE, cgs.media.shotgunSmokePuffShader );
		}
	}
	CG_ShotgunPatternSpread( es->pos.trBase, es->origin2, es->eventParm, es->otherEntityNum, DEFAULT_SHOTGUN_DOUBLE_BLAST_SPREAD );
}

/*
============================================================================

BULLETS

============================================================================
*/


/*
===============
CG_Tracer
===============
*/
void CG_Tracer( vec3_t source, vec3_t dest ) {
	vec3_t		forward, right;
	polyVert_t	verts[4];
	vec3_t		line;
	float		len, begin, end;
	vec3_t		start, finish;
	vec3_t		midpoint;

	// tracer
	VectorSubtract( dest, source, forward );
	len = VectorNormalize( forward );

	// start at least a little ways from the muzzle
	if ( len < 100 ) {
		return;
	}
	begin = 50 + random() * (len - 60);
	end = begin + cg_tracerLength.value;
	if ( end > len ) {
		end = len;
	}
	VectorMA( source, begin, forward, start );
	VectorMA( source, end, forward, finish );

	line[0] = DotProduct( forward, cg.refdef.viewaxis[1] );
	line[1] = DotProduct( forward, cg.refdef.viewaxis[2] );

	VectorScale( cg.refdef.viewaxis[1], line[1], right );
	VectorMA( right, -line[0], cg.refdef.viewaxis[2], right );
	VectorNormalize( right );

	VectorMA( finish, cg_tracerWidth.value, right, verts[0].xyz );
	verts[0].st[0] = 0;
	verts[0].st[1] = 1;
	verts[0].modulate.rgba[0] = 255;
	verts[0].modulate.rgba[1] = 255;
	verts[0].modulate.rgba[2] = 255;
	verts[0].modulate.rgba[3] = 255;

	VectorMA( finish, -cg_tracerWidth.value, right, verts[1].xyz );
	verts[1].st[0] = 1;
	verts[1].st[1] = 0;
	verts[1].modulate.rgba[0] = 255;
	verts[1].modulate.rgba[1] = 255;
	verts[1].modulate.rgba[2] = 255;
	verts[1].modulate.rgba[3] = 255;

	VectorMA( start, -cg_tracerWidth.value, right, verts[2].xyz );
	verts[2].st[0] = 1;
	verts[2].st[1] = 1;
	verts[2].modulate.rgba[0] = 255;
	verts[2].modulate.rgba[1] = 255;
	verts[2].modulate.rgba[2] = 255;
	verts[2].modulate.rgba[3] = 255;

	VectorMA( start, cg_tracerWidth.value, right, verts[3].xyz );
	verts[3].st[0] = 0;
	verts[3].st[1] = 0;
	verts[3].modulate.rgba[0] = 255;
	verts[3].modulate.rgba[1] = 255;
	verts[3].modulate.rgba[2] = 255;
	verts[3].modulate.rgba[3] = 255;

	trap_R_AddPolyToScene( cgs.media.tracerShader, 4, verts );

	midpoint[0] = ( start[0] + finish[0] ) * 0.5;
	midpoint[1] = ( start[1] + finish[1] ) * 0.5;
	midpoint[2] = ( start[2] + finish[2] ) * 0.5;

	// add the tracer sound
	trap_S_StartSound( midpoint, ENTITYNUM_WORLD, CHAN_AUTO, cgs.media.tracerSound );

}


/*
======================
CG_CalcMuzzlePoint
======================
*/
static qboolean	CG_CalcMuzzlePoint( int entityNum, vec3_t muzzle ) {
	vec3_t		forward;
	centity_t	*cent;
	int			anim;

	if ( entityNum == cg.snap->ps.clientNum ) {
		VectorCopy( cg.snap->ps.origin, muzzle );
		muzzle[2] += cg.snap->ps.viewheight;
		AngleVectors( cg.snap->ps.viewangles, forward, NULL, NULL );
		VectorMA( muzzle, 14, forward, muzzle );
		return qtrue;
	}

	cent = &cg_entities[entityNum];
	if ( !cent->currentValid ) {
		return qfalse;
	}

	VectorCopy( cent->currentState.pos.trBase, muzzle );

	AngleVectors( cent->currentState.apos.trBase, forward, NULL, NULL );
	anim = cent->currentState.legsAnim & ~ANIM_TOGGLEBIT;
	if ( anim == LEGS_WALKCR || anim == LEGS_IDLECR ) {
		muzzle[2] += CROUCH_VIEWHEIGHT;
	} else {
		muzzle[2] += DEFAULT_VIEWHEIGHT;
	}

	VectorMA( muzzle, 14, forward, muzzle );

	return qtrue;

}

/*
======================
CG_Bullet

Renders bullet effects.
======================
*/
void CG_Bullet( vec3_t end, int sourceEntityNum, vec3_t normal, qboolean flesh, int fleshEntityNum ) {
	trace_t trace;
	int sourceContentType, destContentType;
	vec3_t		start;

	// if the shooter is currently valid, calc a source point and possibly
	// do trail effects
	if ( sourceEntityNum >= 0 && cg_tracerChance.value > 0 ) {
		if ( CG_CalcMuzzlePoint( sourceEntityNum, start ) ) {
			sourceContentType = CG_PointContents( start, 0 );
			destContentType = CG_PointContents( end, 0 );

			// do a complete bubble trail if necessary
			if ( ( sourceContentType == destContentType ) && ( sourceContentType & CONTENTS_WATER ) ) {
				CG_BubbleTrail( start, end, 32 );
			}
			// bubble trail from water into air
			else if ( ( sourceContentType & CONTENTS_WATER ) ) {
				trap_CM_BoxTrace( &trace, end, start, NULL, NULL, 0, CONTENTS_WATER );
				CG_BubbleTrail( start, trace.endpos, 32 );
			}
			// bubble trail from air into water
			else if ( ( destContentType & CONTENTS_WATER ) ) {
				trap_CM_BoxTrace( &trace, start, end, NULL, NULL, 0, CONTENTS_WATER );
				CG_BubbleTrail( trace.endpos, end, 32 );
			}
			// bubble trail when bullet crosses water without starting or ending in it
			else {
				trap_CM_BoxTrace( &trace, start, end, NULL, NULL, 0, CONTENTS_WATER );
				if ( trace.fraction < 1.0f ) {
					CG_BubbleTrail( trace.endpos, end, 32 );
				}
			}

			// draw a tracer
			if ( sourceEntityNum == cg.snap->ps.clientNum && cg.predictedPlayerState.burstRoundsRemaining > 0 ) {
				// burst mode: always show tracer for visual distinction
				CG_Tracer( start, end );
			} else if ( random() < cg_tracerChance.value ) {
				CG_Tracer( start, end );
			}
		}
	}

	// impact splash and mark
	if ( flesh ) {
		CG_Bleed( end, fleshEntityNum );
	} else {
		CG_MissileHitWall( PROJ_MACHINEGUN, 0, end, normal, IMPACTSOUND_DEFAULT, ENTITYNUM_WORLD );
	}

}
