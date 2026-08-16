// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#version 450

// menubg.frag — the WiredUI SCENE background, drawn as ONE full-viewport
// procedural pass (replaces the ~200-rect Clay DemoBackdrop).
//
// The look (Eser-ratified): a triangle/dot "constellation" foreground (the
// q3now launcher's CoverAnimation aesthetic — drifting points, proximity-fade
// connecting lines, scattered triangles) over a warm dusk gradient + vignette
// (the DemoBackdrop's colour soul). Everything is smooth (smoothstep / fwidth
// anti-aliasing) — zero stair-stepping, infinite resolution, animation for free.
//
// Driven entirely by the set-2 uniform block (continuous wallclock time so the
// scene keeps advancing across menu changes and on the loading screen — it does
// NOT read the resettable WiredUI anim pool). gl_FragCoord is physical pixels.
//
// Composited as the BACKMOST 2D layer (zIndex -10 in the Clay tree); the menu
// content draws on top. Output is straight (non-premultiplied) RGBA into the
// linear-HDR UI pass (img 265), so it sits in the same colour space the rest of
// the 2D UI composites in.

layout(location = 0) in  vec2 frag_tex_coord;   // [0,1] over the quad (from gamma.vert)
layout(location = 0) out vec4 out_color;

// Per-frame dynamic block (set 2 — mirrors the ExposureBlock host pattern).
// std140: 8 floats = 32 B, a vec4 multiple.
layout(set = 2, binding = 0) uniform MenuBgBlock {
	float time;        // continuous seconds (survives menu change / map load)
	float mouseX;      // normalized cursor, [-1..1]
	float mouseY;      // normalized cursor, [-1..1]
	float transition;  // one-shot menu-change nudge, eases 1 -> 0 (0 at rest)
	float resX;        // viewport width  (physical px)
	float resY;        // viewport height (physical px)
	float pad0;
	float pad1;
} u;

// ── Warm dusk palette (normalized from cl_wired_bg.c WUI_BG_V2_*) ──────
const vec3 SKY_TOP   = vec3( 0.227, 0.094, 0.031 );  // $demoSky1 #3a1808
const vec3 SKY_LOW   = vec3( 0.102, 0.035, 0.020 );  // $demoSky2 #1a0905
const vec3 FLOOR_COL = vec3( 0.055, 0.026, 0.016 );  // deeper than $demoFloor, so the floor reads as ground
const vec3 EMBER     = vec3( 0.957, 0.627, 0.227 );  // $accent amber #f4a03a — radial glow accent
const vec3 STAR_COL  = vec3( 0.784, 0.251, 0.188 );  // launcher constellation #c84030 (warm red-orange)

// Hash helpers (Inigo Quilez style — deterministic, no state).
float hash11( float n ) { return fract( sin( n ) * 43758.5453123 ); }
vec2  hash22( vec2 p ) {
	p = vec2( dot( p, vec2( 127.1, 311.7 ) ), dot( p, vec2( 269.5, 183.3 ) ) );
	return fract( sin( p ) * 43758.5453123 );
}

// A cell's drifting point position (in cell-local [0,1] space), animated by time.
// Each cell gets a random phase/speed so the field never looks like a grid.
vec2 cellPoint( vec2 cell ) {
	vec2 h = hash22( cell );
	// slow Lissajous drift — keeps points inside the cell (0.15..0.85 range).
	float sp = 0.20 + 0.35 * h.x;
	vec2  ph = h * 6.2831853;
	return vec2(
		0.5 + 0.34 * sin( u.time * sp + ph.x ),
		0.5 + 0.34 * cos( u.time * sp * 0.87 + ph.y )
	);
}

// Distance from point p to segment ab (for the connecting lines).
float segDist( vec2 p, vec2 a, vec2 b ) {
	vec2 pa = p - a, ba = b - a;
	float t = clamp( dot( pa, ba ) / max( dot( ba, ba ), 1e-6 ), 0.0, 1.0 );
	return length( pa - ba * t );
}

// Signed distance to an equilateral triangle (radius r, pointing up), centered.
float sdTriangle( vec2 p, float r ) {
	const float k = 1.7320508;  // sqrt(3)
	p.x = abs( p.x ) - r;
	p.y = p.y + r / k;
	if ( p.x + k * p.y > 0.0 ) p = vec2( p.x - k * p.y, -k * p.x - p.y ) / 2.0;
	p.x -= clamp( p.x, -2.0 * r, 0.0 );
	return -length( p ) * sign( p.y );
}

void main() {
	vec2 res = vec2( u.resX, u.resY );
	if ( res.x < 1.0 || res.y < 1.0 ) res = vec2( 1280.0, 720.0 );
	float aspect = res.x / res.y;

	// UV in [0,1] (frag_tex_coord already spans the quad). AA is derived per-use
	// via fwidth so it stays crisp at any resolution.
	vec2 uv = frag_tex_coord;

	// Aspect-corrected space so cells + shapes are round, not stretched.
	vec2 asp = vec2( aspect, 1.0 );

	// ── Parallax base offset + per-plane depth ────────────────────────
	// ONE shared base offset (smoothed cursor + the one-shot menu-transition
	// nudge); each visual plane below multiplies it by its own DEPTH factor, so
	// planes travel at DIFFERENT speeds rather than sliding as one flat sheet.
	// That speed differential IS the depth cue — a uniform shift moves every
	// plane in lockstep and reads as a single pane sliding, not as distance.
	//
	// Depth factors run 0 (infinitely far, pinned) → 1 (nearest, leads). They
	// mirror the scene's own back-to-front stacking: the dusk gradient is the
	// far backdrop, the warm glow sits mid-field, the constellation is the near
	// foreground, and the vignette is the fixed frame (pinned by definition —
	// it is the screen border, not part of the world, so it must NOT drift or
	// the illusion collapses).
	//
	// Absolute amplitudes stay small (base ≈ ±2% of UV at the screen edge) so
	// the near plane leads without swimming; the far planes move so little they
	// register as depth rather than motion.
	vec2 par = vec2( u.mouseX * 0.020 + u.transition * 0.030, u.mouseY * 0.014 );

	const float DEPTH_SKY  = 0.10;   // far dusk gradient — barely creeps
	const float DEPTH_GLOW = 0.42;   // mid-field warm bloom
	const float DEPTH_STAR = 1.00;   // near constellation — leads the motion

	// ── 1. Warm dusk gradient (real gradient, no bands) ───────────────
	// FARTHEST plane: shifts by DEPTH_SKY, so it creeps ~10x slower than the
	// constellation. The ramp is purely vertical, so only the Y component can
	// register — an X shift on a horizontally-uniform field is a no-op. The
	// horizon line therefore rises/falls a hair as the cursor moves, which is
	// exactly how a distant skyline behaves.
	float skyY = uv.y + par.y * DEPTH_SKY;
	float g = smoothstep( 0.0, 0.62, skyY );
	vec3  col = mix( SKY_TOP, SKY_LOW, g );
	float floorMix = smoothstep( 0.60, 1.0, skyY );
	col = mix( col, FLOOR_COL, floorMix );

	// ── 2. Soft radial warm glow (the plasma/ember soul) ──────────────
	// MID plane: shifts by DEPTH_GLOW — visibly faster than the sky, clearly
	// slower than the constellation, which is what separates the three depths.
	// Both pools share one offset so they stay locked as a single plane; the
	// radial falloff makes this shift read directly as the light source moving.
	vec2  guv  = uv + par * DEPTH_GLOW;
	vec2  gp   = ( guv - vec2( 0.5, 0.66 ) ) * asp;
	float glow = exp( -dot( gp, gp ) * 6.5 );
	glow += 0.35 * exp( -dot( (guv - vec2(0.5,0.30)) * asp, (guv - vec2(0.5,0.30)) * asp ) * 9.0 );
	// gentle breathing so the warmth pulses with the continuous clock.
	glow *= 0.85 + 0.15 * sin( u.time * 0.6 );
	col += EMBER * glow * 0.11;

	// ── 3. Constellation (drifting dots / triangles + proximity lines) ─
	// Work in a cell grid over aspect-corrected, parallax-shifted UV. Each
	// cell owns one drifting point; we test the 3x3 neighborhood so points +
	// lines cross cell borders seamlessly.
	// NEAREST plane: full base offset (DEPTH_STAR == 1.0) — this is the layer
	// that leads the motion and gives the scene its foreground.
	const float CELLS = 7.0;                     // ~7 wide → launcher-sparse density
	vec2  suv  = ( uv + par * DEPTH_STAR ) * asp * CELLS;   // constellation space
	vec2  cell = floor( suv );
	vec2  f    = fract( suv );

	float lineAcc = 0.0;   // accumulated connecting-line coverage
	float dotAcc  = 0.0;   // accumulated point coverage (dots + triangles)

	for ( int oy = -1; oy <= 1; oy++ ) {
		for ( int ox = -1; ox <= 1; ox++ ) {
			vec2 nCell = cell + vec2( float(ox), float(oy) );
			vec2 pN    = vec2( float(ox), float(oy) ) + cellPoint( nCell );  // relative to center cell origin
			vec2 d     = pN - f;                     // vector from fragment to neighbor point (cell units)
			float dist = length( d );

			// point identity: type (0 dot / 1 triangle / 2 plus) + per-point alpha
			vec2  hh    = hash22( nCell + 3.17 );
			float ptype = floor( hh.x * 3.0 );
			float palpha = 0.35 + 0.45 * hh.y;

			// dot / shape — radius in cell units, AA'd via fwidth (screen-derivative
			// of the cell-space distance → crisp at any resolution).
			float aa = fwidth( dist ) + 1e-4;
			if ( ptype < 0.5 ) {
				// filled soft dot
				float r = 0.045;
				dotAcc += palpha * ( 1.0 - smoothstep( r - aa, r + aa, dist ) );
			} else if ( ptype < 1.5 ) {
				// triangle outline
				float td = abs( sdTriangle( d, 0.075 ) ) - 0.006;
				dotAcc += palpha * ( 1.0 - smoothstep( -aa, aa, td ) );
			} else {
				// plus / cross (two thin bars)
				float bar = min(
					max( abs( d.x ) - 0.010, abs( d.y ) - 0.060 ),
					max( abs( d.y ) - 0.010, abs( d.x ) - 0.060 ) );
				dotAcc += palpha * ( 1.0 - smoothstep( -aa, aa, bar ) );
			}

			// connecting line: from the CENTER cell's point to this neighbor's
			// point, alpha fading with their separation (launcher: 1 - dist/150).
			if ( !( ox == 0 && oy == 0 ) ) {
				vec2 pCenter = cellPoint( cell );           // center point, center-cell local
				vec2 pNbr    = vec2( float(ox), float(oy) ) + cellPoint( nCell );
				float sep    = length( pNbr - pCenter );
				float fade   = 1.0 - smoothstep( 0.55, 1.15, sep );   // proximity fade
				if ( fade > 0.001 ) {
					float ld  = segDist( f, pCenter, pNbr );
					float law = fwidth( ld ) + 1e-4;
					float line = ( 1.0 - smoothstep( 0.0, 0.006 + law, ld ) );
					lineAcc += line * fade * 0.5;
				}
			}
		}
	}

	// Composite constellation over the dusk. Lines are faint (launcher ~0.15),
	// points a touch brighter. STAR_COL is the warm red-orange.
	col += STAR_COL * clamp( lineAcc, 0.0, 1.0 ) * 0.22;
	col += STAR_COL * clamp( dotAcc,  0.0, 1.0 ) * 0.55;

	// ── 4. Vignette (frame it — launcher radial ellipse) ──────────────
	// DELIBERATELY PINNED (no parallax term): the vignette is the screen frame,
	// not a plane in the world. Drifting it would slide the dark corners away
	// from the actual corners and break the framing — so it reads as depth-0 by
	// intent, not by omission.
	vec2  vp   = ( uv - 0.5 ) * asp;
	float vig  = smoothstep( 1.05, 0.35, length( vp ) );   // 1 at center → 0 at corners
	col *= mix( 0.55, 1.0, vig );

	// Fine dithering to kill any residual banding in the very dark ramp.
	float dnoise = hash11( dot( gl_FragCoord.xy, vec2( 12.9898, 78.233 ) ) );
	col += ( dnoise - 0.5 ) * (1.0/255.0);

	out_color = vec4( max( col, vec3( 0.0 ) ), 1.0 );
}
