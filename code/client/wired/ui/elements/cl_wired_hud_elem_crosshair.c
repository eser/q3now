// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

/*
cl_wired_hud_elem_crosshair.c — Wired UI crosshair element

Procedural per-weapon reticle (chain 6.0). The draw-spec is produced by the
Lua pipeline in cl_wired_crosshair.c (default base -> weapon static base ->
per-frame update(state) delta); this element renders it with solid-color quad
and line primitives (re.DrawStretchPic + the cgame HUD-pass white shader,
re.DrawLine for the ring). No texture/material — the old static-PNG blit is gone.
*/

#include "../../../client.h"
#include "cl_wired_ui_hud_compat.h"
#include "cl_wired_ui_hud_private.h"
/* cl_wired_crosshair.h lives in code/client/wired/hud/ (the procedural-
 * crosshair Lua pipeline stays there); resolved via the -I .../wired/hud
 * include dir. Was "../cl_wired_crosshair.h" when this file lived in
 * hud/elements/ — bare include after the V-?? relocation to ui/elements/. */
#include "cl_wired_crosshair.h"

#if FEAT_WIRED_UI

#include <math.h>

typedef struct {
	modernhudConfig_t config;
} modernHudElementCrosshair_t;

void *CG_ModernHUDElementCrosshairCreate( const modernhudConfig_t *config ) {
	modernHudElementCrosshair_t *element;

	ModernHUD_ELEMENT_INIT( element, config );

	/* Procedural reticle: no shader to register — drawn with the cgame HUD-pass
	 * white shader (wiredHud->whiteShader) resolved per-frame in the routine. */
	return element;
}

/* Fill an axis-aligned solid-color quad, with an optional dark outline drawn
 * first (expanded by `outline` px on every side). All inputs are real pixels.
 * `white` is the cgame HUD-pass white shader (cgs.media.whiteShader via
 * wiredHud->whiteShader) — NOT cls.whiteShader, which is registered in the
 * client/menu render context and does not resolve in the HUD pass. */
static void XH_Quad( float x, float y, float w, float h,
                     const vec4_t color, float outline, float outlineAlpha,
                     qhandle_t white ) {
	if ( w <= 0.0f || h <= 0.0f ) return;

	if ( outline > 0.0f && outlineAlpha > 0.0f ) {
		vec4_t dark = { 0.0f, 0.0f, 0.0f, 0.0f };
		dark[3] = outlineAlpha;
		re.SetColor( dark );
		re.DrawStretchPic( x - outline, y - outline,
		                   w + outline * 2.0f, h + outline * 2.0f,
		                   0, 0, 1, 1, white );
	}

	re.SetColor( color );
	re.DrawStretchPic( x, y, w, h, 0, 0, 1, 1, white );
}

/* Composed outline ring: `segments` line segments around a circle of radius r,
 * line width `thickness`. Draws a dark backing ring first when outlined. */
static void XH_Ring( float cx, float cy, float r, float thickness,
                     const vec4_t color, float outline, float outlineAlpha,
                     qhandle_t white ) {
	const int segments = 48;
	int   i;
	float a0, a1;

	if ( r <= 0.0f ) return;
	if ( thickness <= 0.0f ) thickness = 1.0f;

	if ( outline > 0.0f && outlineAlpha > 0.0f ) {
		vec4_t dark = { 0.0f, 0.0f, 0.0f, 0.0f };
		dark[3] = outlineAlpha;
		re.SetColor( dark );
		for ( i = 0; i < segments; i++ ) {
			a0 = ( (float)M_PI * 2.0f / segments ) * i;
			a1 = ( (float)M_PI * 2.0f / segments ) * ( i + 1 );
			re.DrawLine( cx + cosf( a0 ) * r, cy + sinf( a0 ) * r,
			             cx + cosf( a1 ) * r, cy + sinf( a1 ) * r,
			             thickness + outline * 2.0f, white );
		}
	}

	re.SetColor( color );
	for ( i = 0; i < segments; i++ ) {
		a0 = ( (float)M_PI * 2.0f / segments ) * i;
		a1 = ( (float)M_PI * 2.0f / segments ) * ( i + 1 );
		re.DrawLine( cx + cosf( a0 ) * r, cy + sinf( a0 ) * r,
		             cx + cosf( a1 ) * r, cy + sinf( a1 ) * r,
		             thickness, white );
	}
}

void CG_ModernHUDElementCrosshairRoutine( void *context ) {
	cgCrosshairDrawSpec_t spec;
	float cx, cy, s, gap, ol, olA;
	const cgCrosshairArm_t *arm;
	qhandle_t white;

	(void)context;

	/* cgame's existing draw-suppression policy still applies: shaderIndex < 0
	 * means hidden (cg_drawCrosshair off, third-person, or spectator). The
	 * procedural path only replaces HOW the reticle is drawn, not WHEN. */
	if ( wiredHud->crosshair.shaderIndex < 0 ) return;

	if ( !WiredCrosshair_Eval( &spec ) ) return;
	if ( !spec.visible ) return;

	/* Use the cgame HUD-pass white shader (synced from cgs.media.whiteShader),
	 * NOT cls.whiteShader — the latter is registered in the client/menu render
	 * context and does not resolve during the in-game HUD render pass. */
	white = wiredHud->whiteShader;
	if ( !white ) return;

	cx = cls.glconfig.vidWidth  * 0.5f;
	cy = cls.glconfig.vidHeight * 0.5f;
	/* WA-1: apply the cgame-staged world-anchored offset (real pixels, from
	 * center). 0,0 in first person → unchanged center; non-zero in third person →
	 * the reticle sits at the bullet-impact point. All arms/dot/ring/corner draws
	 * below are relative to (cx,cy), so they shift with the offset automatically. */
	cx += wiredHud->crosshair.x;
	cy += wiredHud->crosshair.y;
	s  = ( spec.scale > 0.0f ) ? spec.scale : 1.0f;
	gap = spec.gap * s;
	ol  = spec.outline.thickness * s;
	olA = spec.outline.alpha;

	/* ── arms (top/right/bottom/left) ── */
	arm = &spec.arms[CG_XH_ARM_TOP];
	if ( arm->enabled && arm->length > 0.0f && arm->thickness > 0.0f ) {
		float len = arm->length * s, th = arm->thickness * s;
		XH_Quad( cx - th * 0.5f, cy - gap - len, th, len, spec.color, ol, olA, white );
	}
	arm = &spec.arms[CG_XH_ARM_BOTTOM];
	if ( arm->enabled && arm->length > 0.0f && arm->thickness > 0.0f ) {
		float len = arm->length * s, th = arm->thickness * s;
		XH_Quad( cx - th * 0.5f, cy + gap, th, len, spec.color, ol, olA, white );
	}
	arm = &spec.arms[CG_XH_ARM_LEFT];
	if ( arm->enabled && arm->length > 0.0f && arm->thickness > 0.0f ) {
		float len = arm->length * s, th = arm->thickness * s;
		XH_Quad( cx - gap - len, cy - th * 0.5f, len, th, spec.color, ol, olA, white );
	}
	arm = &spec.arms[CG_XH_ARM_RIGHT];
	if ( arm->enabled && arm->length > 0.0f && arm->thickness > 0.0f ) {
		float len = arm->length * s, th = arm->thickness * s;
		XH_Quad( cx + gap, cy - th * 0.5f, len, th, spec.color, ol, olA, white );
	}

	/* ── ring ── */
	if ( spec.ring.enabled && spec.ring.radius > 0.0f ) {
		float r = spec.ring.radius * s;
		if ( spec.ring.filled ) {
			/* filled disc: approximate with a centered quad of diameter 2r */
			XH_Quad( cx - r, cy - r, r * 2.0f, r * 2.0f, spec.color, ol, olA, white );
		} else {
			XH_Ring( cx, cy, r, spec.ring.thickness * s, spec.color, ol, olA, white );
		}
	}

	/* ── corners (viewfinder brackets) ── */
	if ( spec.corners.enabled && spec.corners.arm_length > 0.0f ) {
		float off = spec.corners.offset * s;
		float al  = spec.corners.arm_length * s;
		float th  = ( spec.corners.thickness > 0.0f ? spec.corners.thickness : 1.0f ) * s;
		/* four L-brackets; each = horizontal leg + vertical leg meeting at the
		 * corner vertex (±off, ±off) and pointing inward. */
		/* top-left */
		XH_Quad( cx - off, cy - off, al, th, spec.color, ol, olA, white );
		XH_Quad( cx - off, cy - off, th, al, spec.color, ol, olA, white );
		/* top-right */
		XH_Quad( cx + off - al, cy - off, al, th, spec.color, ol, olA, white );
		XH_Quad( cx + off - th, cy - off, th, al, spec.color, ol, olA, white );
		/* bottom-left */
		XH_Quad( cx - off, cy + off - th, al, th, spec.color, ol, olA, white );
		XH_Quad( cx - off, cy + off - al, th, al, spec.color, ol, olA, white );
		/* bottom-right */
		XH_Quad( cx + off - al, cy + off - th, al, th, spec.color, ol, olA, white );
		XH_Quad( cx + off - th, cy + off - al, th, al, spec.color, ol, olA, white );
	}

	/* ── dot (drawn last, on top) ── */
	if ( spec.dot.enabled && spec.dot.radius > 0.0f ) {
		float r = spec.dot.radius * s;
		XH_Quad( cx - r, cy - r, r * 2.0f, r * 2.0f, spec.color, ol, olA, white );
	}

	re.SetColor( NULL );
}

#endif // FEAT_WIRED_UI
