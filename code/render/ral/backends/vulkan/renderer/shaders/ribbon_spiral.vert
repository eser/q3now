// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#version 450

/*
ribbon_spiral.vert — parametric rail-helix generator.

Unlike ribbon.vert (which reads a CPU-built point array), this shader
GENERATES the helix analytically per-vertex from per-slot spawn params +
the age carried in the header. It reproduces the CPU builder in
cg_weapons.c CG_AddRailTrails exactly (verified bit-for-bit against the
integer-j loop, all fracs/beam-lengths): the expanding radius, unwinding
spacing, widening width, per-point unwind-fade, the (j*ROTATION)%36 ring
index, the d<0 muzzle cut, and the numSegs truncation/clamp.

Dispatch: one instanced draw, instanceCount = live pool slots,
vertexCount = 6 * (RAIL_RIBBON_MAX_SEGMENTS - 1). gl_InstanceIndex selects
the pool slot's header; gl_VertexIndex/6 = segment index j; %6 = quad
corner. Segments past the slot's live count (or past the muzzle) collapse
to a degenerate vertex behind the near plane, matching the CPU's
per-frame-varying point count.

The evolution constants are the RAIL_RIBBON_* values from primitives.h;
they are compile-time constants shared by convention with the cgame
emitter (which only supplies the spawn-fixed params) and the renderer.
*/

// ── evolution constants (== primitives.h RAIL_RIBBON_*) ──
const float RAIL_SPACING          = 3.0;     // RAIL_RIBBON_SPACING
const float RAIL_WIDTH_BASE       = 1.5;     // RAIL_RIBBON_WIDTH_BASE
const int   RAIL_ROTATION         = 2;       // RAIL_RIBBON_ROTATION
const float RAIL_RADIUS_BASE      = 2.0;     // RAIL_RIBBON_RADIUS_BASE
const float RAIL_RADIUS_GROW      = 2.0;     // RAIL_RIBBON_RADIUS_GROW
const float RAIL_WIDTH_GROW       = 1.5;     // RAIL_RIBBON_WIDTH_GROW
const float RAIL_SPACING_TIGHTEN  = 0.667;   // RAIL_RIBBON_SPACING_TIGHTEN
const int   RAIL_RING_COUNT       = 36;      // RAIL_RIBBON_RING_COUNT
const int   RAIL_MAX_SEGMENTS     = 2048;    // RAIL_RIBBON_MAX_SEGMENTS

layout(set = 1, binding = 0, std140) uniform EffectsUBO {
	mat4 mvp;
	vec4 eyeWorld;
	vec4 frameParams;
	vec4 _v2;
};

// Per-slot spawn params, std430 (640 B, must match RB_DrawRailRibbons).
struct RailRibbonHeader {
	vec4 startBeamLen;   // .xyz start,    .w beamLen
	vec4 beamAxisAge;    // .xyz beamAxis, .w age  (currentTime - spawnTime)
	vec4 colorDuration;  // .xyz rgb,      .w duration
	vec4 misc;           // .x baseAlpha,  .yzw pad
	vec4 perpAxis[36];   // .xyz ring dir, .w pad
};

layout(std430, set = 0, binding = 0) readonly buffer Headers { RailRibbonHeader headers[]; };

layout(location = 0) out vec2 fragUV;
layout(location = 1) out vec4 fragColor;
layout(location = 2) flat out uint fragShaderHandle;

out gl_PerVertex {
	vec4 gl_Position;
};

// 6 vertices per quad: (v0,v2,v1),(v1,v2,v3); v0/v1 = seg i, v2/v3 = seg i+1.
const uint  pointOff[6] = uint[6] (0u, 1u, 0u, 0u, 1u, 1u);
const float sideSign[6] = float[6](-1.0, -1.0, +1.0, +1.0, -1.0, +1.0);

// Reconstruct one helix control point (position, half-width, extrude
// normal, alpha) for segment index j, given the evolved params. Returns
// false when the point is past the muzzle (d < 0) or past the live count.
bool helixPoint( int j, int numSegs, vec3 start, vec3 beamAxis, float beamLen,
                 float curRadius, float curSpacing, float curWidth, float frac,
                 float trailAlpha, RailRibbonHeader hdr,
                 out vec3 pos, out vec3 normal, out float halfW, out float alpha )
{
	if ( j > numSegs - 1 ) return false;
	float d = beamLen - float(j) * curSpacing;
	if ( d < 0.0 ) return false;                       // CPU: if(d<0) break

	int ring = (j * RAIL_ROTATION) % RAIL_RING_COUNT;  // integer modulo
	if ( ring < 0 ) ring += RAIL_RING_COUNT;
	vec3 perp = hdr.perpAxis[ring].xyz;

	pos    = start + d * beamAxis + curRadius * perp;
	normal = normalize( cross( beamAxis, perp ) );
	halfW  = curWidth;

	float segPos     = float(j) / float(numSegs - 1);
	float unwindFade = 1.0 - frac * (1.0 + segPos);
	if ( unwindFade < 0.0 ) unwindFade = 0.0;
	alpha = trailAlpha * unwindFade;
	return true;
}

// Collapse a vertex to behind the near plane (culled) — matches the CPU
// dropping the point entirely when d<0 or j past the live count.
void emitDegenerate() {
	gl_Position      = vec4(0.0, 0.0, -2.0, 1.0);   // w=1, z<-w → clipped
	fragUV           = vec2(0.0);
	fragColor        = vec4(0.0);
	fragShaderHandle = 0u;
}

void main() {
	RailRibbonHeader hdr = headers[gl_InstanceIndex];

	uint vertInQuad = uint(gl_VertexIndex) % 6u;
	int  segIdx     = int(uint(gl_VertexIndex) / 6u);

	vec3  start    = hdr.startBeamLen.xyz;
	float beamLen  = hdr.startBeamLen.w;
	vec3  beamAxis = hdr.beamAxisAge.xyz;
	float age      = hdr.beamAxisAge.w;
	float duration = hdr.colorDuration.w;
	float baseAlpha = hdr.misc.x;

	// ── evolved params (== cg_weapons.c:442-446) ──
	float frac = (duration > 0.0) ? age / duration : 1.0;
	if ( frac < 0.0 ) frac = 0.0;
	if ( frac > 1.0 ) frac = 1.0;
	float trailAlpha = 1.0 - frac;
	float easedFrac  = 1.0 - (1.0 - frac) * (1.0 - frac);
	float curRadius  = RAIL_RADIUS_BASE + easedFrac * RAIL_RADIUS_GROW;
	float curSpacing = RAIL_SPACING * (1.0 - easedFrac * RAIL_SPACING_TIGHTEN);
	float curWidth   = RAIL_WIDTH_BASE * (1.0 + easedFrac * RAIL_WIDTH_GROW);
	int   numSegs    = int( beamLen / curSpacing );     // truncation matches (int)
	if ( numSegs > RAIL_MAX_SEGMENTS ) numSegs = RAIL_MAX_SEGMENTS;
	if ( numSegs < 2 ) numSegs = 2;

	// A quad spans control points segIdx and segIdx+1, so BOTH must be live
	// points (0..numSegs-1). The CPU builder forms exactly numSegs-1 quads
	// over consecutive existing points, so the last valid quad is segIdx =
	// numSegs-2. Cull the WHOLE quad past that — culling only the out-of-range
	// point would leave a partial (numSegs-1, numSegs) sliver the CPU never
	// draws. (Also matches the CPU's d<0 stop: the point at numSegs would sit
	// at d>=0 but is beyond the live count.)
	if ( segIdx > numSegs - 2 ) {
		emitDegenerate();
		return;
	}

	// This vertex belongs to the quad between control points segIdx and segIdx+1.
	int  pi   = segIdx + int(pointOff[vertInQuad]);
	float sgn = sideSign[vertInQuad];

	vec3  pos, normal; float halfW, alpha;
	if ( !helixPoint( pi, numSegs, start, beamAxis, beamLen,
	                  curRadius, curSpacing, curWidth, frac, trailAlpha, hdr,
	                  pos, normal, halfW, alpha ) ) {
		emitDegenerate();
		return;
	}

	// Path-aligned custom-normal extrude (matches ribbon.vert's
	// PRIM_FLAG_CUSTOM_NORMAL branch; each vertex uses its own point's normal
	// so the ribbon twists along the helix).
	vec3 worldPos = pos + normal * (halfW * sgn);
	gl_Position = mvp * vec4(worldPos, 1.0);

	// UV: u along the ribbon (0..1 across the live segment count), v = side.
	float denom = (numSegs > 1) ? float(numSegs - 1) : 1.0;
	fragUV = vec2( float(pi) / denom, (sgn > 0.0) ? 1.0 : 0.0 );

	// Per-point RGBA: the base helix colour with the age/unwind alpha.
	fragColor = vec4( hdr.colorDuration.xyz, baseAlpha * alpha );

	// Rail helix uses the white primitive slot (slot 0); ribbon.frag masks
	// bits 0..30 and reads the domain bit 31 (0 = sRGB, the default).
	fragShaderHandle = 0u;
}
