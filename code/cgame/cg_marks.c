// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 1999-2005 Id Software, Inc.
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
//
// cg_marks.c -- wall marks

#include "cg_local.h"
#include "../qcommon/wired/render/primitives.h"
#include "../qcommon/wired/render/traps.h"

/*
===================================================================

MARK POLYS

Impact marks (bullet, blood, burn, shadow) are GPU decals: CG_ImpactMark hands
a projector descriptor to the renderer, which clips and composites it on the GPU
(RB_DrawDecals) and ages it from the decal ring. The old CPU path — a per-impact
BSP-clip into a persistent cg_markPolys list replayed every frame — is retired;
there is no per-frame mark bookkeeping on the game side anymore.

===================================================================
*/

// Vanilla-Q3 impact-mark lifetime: a mark holds at full opacity for
// MARK_TOTAL_TIME, then fades to nothing over the final MARK_FADE_TIME. The
// renderer ramps this entirely GPU-side (decal.vert) from the lifetime carried
// in the descriptor, so there is no per-frame game-side bookkeeping.
#define MARK_TOTAL_TIME  10000   // ms a mark lives before it has fully faded
#define MARK_FADE_TIME    1000   // ms of that span spent fading out at the end
// Temporary marks (railgun debris flecks, footsteps) are short-lived spray that
// shouldn't accumulate; they get a fraction of the full lifetime.
#define MARK_TEMP_TIME     1000   // ms lifetime for `temporary` marks

/*
=================
CG_ImpactMark

origin should be a point within a unit of the plane
dir should be the plane normal

The renderer derives the decal's blend mode from markShader, so this carries no
per-mark blend state. orientation is in degrees; decalDesc_t orientation is radians.
=================
*/
void CG_ImpactMark( qhandle_t markShader, const vec3_t origin, const vec3_t dir,
				   float orientation, float red, float green, float blue, float alpha,
				   qboolean alphaFade, float radius, qboolean temporary ) {
	decalDesc_t dd;

	if ( radius <= 0 ) {
		Com_Terminate( TERM_CLIENT_DROP, "CG_ImpactMark called with <= 0 radius" );
	}

	if ( VectorLength( dir ) < 1e-6f ) {
		return;
	}

	memset( &dd, 0, sizeof( dd ) );
	VectorCopy( origin, dd.origin );
	VectorNormalize2( dir, dd.normal );
	dd.radius      = radius;
	dd.orientation = orientation * ( (float)M_PI / 180.0f );
	dd.rgba[0] = red; dd.rgba[1] = green; dd.rgba[2] = blue; dd.rgba[3] = alpha;
	dd.shader  = markShader;
	// Hand the renderer the mark's total lifetime (seconds); it holds full
	// opacity then ramps out over the final MARK_FADE_TIME entirely on the GPU.
	// The renderer picks the fade channel from the mark's resolved blend mode
	// (alpha-blend → alpha, modulate/additive → colour), which is why the
	// caller's alphaFade hint isn't forwarded — the blend pipeline is
	// authoritative and matches it (e.g. burn marks blend as modulate).
	dd.lifetime = ( temporary ? MARK_TEMP_TIME : MARK_TOTAL_TIME ) / 1000.0f;
	(void)alphaFade;
	trap_R_AddDecalToScene( &dd );
}
