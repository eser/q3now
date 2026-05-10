#version 450

/*
ribbon.vert — primitive ribbon vertex shader

Pulls per-point data from a storage buffer of RibbonPoints and a
parallel storage buffer of RibbonHeaders (one per submitted ribbon).
gl_InstanceIndex selects the header; gl_VertexIndex/6 selects the
segment within that header; gl_VertexIndex%6 selects the corner.

For each segment between header.points[i] and header.points[i+1]
we emit two triangles forming a quad, extruded perpendicular to
the segment along a normal whose source depends on flags:

  PRIM_FLAG_CUSTOM_NORMAL  (per-point, wins over the others)
    normal = points[i].normal.xyz   // caller pre-normalized; used
                                    // as-is, per-vertex (NOT
                                    // per-segment), so adjacent
                                    // points with different normals
                                    // produce a twisting ribbon
                                    // along the path.

  PRIM_FLAG_CAMERA_FACING (or default; per-segment)
    normal = cross(segmentAxis, segmentStart - eyeWorld)

  PRIM_FLAG_VIEW_UP_PLANE (per-segment)
    normal = cross(segmentAxis, worldUp)  // worldUp = (0,0,1) in Q3

For the two derived modes, if the chosen reference is parallel to
segmentAxis, fall back to whichever world basis vector is least
parallel. Custom-normal mode skips that fallback entirely; the
caller is responsible for supplying a valid unit vector.

Half-width per point lives in posW.w. Per-point normal lives in
normal.xyz; .w is unused pad.
*/

// Push range layout (must match the host-side push):
//   bytes  0..63   mat4  mvp           (vertex stage)
//   bytes 64..79   vec4  eyeWorld      (.xyz used; world-space camera origin)
//   bytes 80..95   vec4  frameParams   (.x  = tr.identityLight,
//                                       .yzw reserved for future scalars)
// frameParams is consumed by ribbon.frag (CGEN_VERTEX-equivalent
// halving), so the host-side push range covers VERTEX | FRAGMENT.
layout(push_constant) uniform Push {
	mat4 mvp;
	vec4 eyeWorld;
	vec4 frameParams;
};

struct RibbonPoint {
	vec4 posW;    // .xyz = world position, .w = half-width
	vec4 rgba;
	vec4 normal;  // .xyz = unit extrude axis (consumed when
	              //   PRIM_FLAG_CUSTOM_NORMAL is set on the ribbon),
	              //   .w   = unused pad. Layout-matches host
	              //   ribbonPoint_t (vec3 normal + float _pad).
};

struct RibbonHeader {
	uint  pointOffset;
	uint  pointCount;
	uint  shaderHandle;
	uint  flags;
	vec2  uvScroll;     // UV/second; {0,0} = static UV
	// std430 stride: 4 uints (16 B) + vec2 (8 B) = 24 B, struct alignment 8 B
	// (vec2 is the largest member); 24 is a multiple of 8 so no trailing pad.
};

layout(std430, set = 0, binding = 0) readonly buffer Points  { RibbonPoint  points[];  };
layout(std430, set = 0, binding = 1) readonly buffer Headers { RibbonHeader headers[]; };

layout(location = 0) out vec2 fragUV;
layout(location = 1) out vec4 fragColor;
// Per-ribbon shader handle, flat-interpolated to the fragment stage
// for the binding-2 sampler-array lookup. Same value for every
// vertex of one ribbon (all share gl_InstanceIndex), so flat is
// uniform-correct.
layout(location = 2) flat out uint fragShaderHandle;

out gl_PerVertex {
	vec4 gl_Position;
};

// Mirrors the constants in primitives.h
const uint PRIM_FLAG_CAMERA_FACING = 0x0001u;
const uint PRIM_FLAG_VIEW_UP_PLANE = 0x0008u;
const uint PRIM_FLAG_CUSTOM_NORMAL = 0x0010u;

// 6 vertices per quad (2 triangles): (v0,v2,v1), (v1,v2,v3) where
//   v0 = i, left      v1 = i, right
//   v2 = i+1, left    v3 = i+1, right
// pointOff[k] is the offset to add to i for vertex k (0 or 1).
// sideSign[k] is the side of the ribbon (-1 = left, +1 = right).
const uint  pointOff[6] = uint[6] (0u, 1u, 0u, 0u, 1u, 1u);
const float sideSign[6] = float[6](-1.0, -1.0, +1.0, +1.0, -1.0, +1.0);

void main() {
	RibbonHeader hdr = headers[gl_InstanceIndex];

	uint vertInQuad = uint(gl_VertexIndex) % 6u;
	uint segIdx     = uint(gl_VertexIndex) / 6u;

	uint pi   = hdr.pointOffset + segIdx + pointOff[vertInQuad];
	float sgn = sideSign[vertInQuad];

	RibbonPoint p = points[pi];
	float halfW = p.posW.w;

	vec3 normal;
	if ((hdr.flags & PRIM_FLAG_CUSTOM_NORMAL) != 0u) {
		// Per-point normal supplied by the caller. Used as-is — no
		// normalize, no fallback. Each vertex of the quad uses ITS
		// own point's normal, so the two endpoints of a segment can
		// extrude in different directions and the ribbon twists
		// along the path. Required for path-aligned effects whose
		// extrude axis rotates with gameplay geometry rather than
		// with the view.
		normal = p.normal.xyz;
	} else {
		// Derived per-segment normal. Both endpoints of a quad share
		// the same normal in this branch, so the ribbon stays flat
		// across each segment.
		vec3 a = points[hdr.pointOffset + segIdx].posW.xyz;
		vec3 b = points[hdr.pointOffset + segIdx + 1u].posW.xyz;
		vec3 segAxis = b - a;

		// Pick the reference vector for the cross product.
		bool wantUp = (hdr.flags & PRIM_FLAG_VIEW_UP_PLANE) != 0u
		           && (hdr.flags & PRIM_FLAG_CAMERA_FACING) == 0u;
		vec3 ref = wantUp ? vec3(0.0, 0.0, 1.0) : (a - eyeWorld.xyz);

		normal = cross(segAxis, ref);
		if (dot(normal, normal) < 1e-12) {
			// Reference parallel to segAxis. Pick the world basis vector
			// least parallel to segAxis and try again. Always succeeds for
			// any non-zero segAxis.
			vec3 ad = abs(segAxis);
			vec3 fb = (ad.x <= ad.y && ad.x <= ad.z) ? vec3(1.0, 0.0, 0.0)
			        : (ad.y <= ad.z)                 ? vec3(0.0, 1.0, 0.0)
			                                          : vec3(0.0, 0.0, 1.0);
			normal = cross(segAxis, fb);
		}
		normal = normalize(normal);
	}

	vec3 worldPos = p.posW.xyz + normal * (halfW * sgn);
	gl_Position = mvp * vec4(worldPos, 1.0);

	// UV: u = position along ribbon (0 at first point, 1 at last);
	//     v = side (0 = left, 1 = right).
	float denom = (hdr.pointCount > 1u) ? float(hdr.pointCount - 1u) : 1.0;
	vec2 baseUV = vec2(float(pi - hdr.pointOffset) / denom,
	                   (sgn > 0.0) ? 1.0 : 0.0);
	// Ribbon is always transient (per-frame ring buffer; no
	// persistent pool). Use absolute frame time as the uvScroll
	// reference — per-frame re-submission keeps the scroll phase
	// continuous because the reference doesn't reset.
	//
	// fract() wraps scrolled UVs into [0, 1] because ribbon's
	// sampler is CLAMP_TO_EDGE (vk.particle.sampler, reused at
	// binding 2). Without fract, large scroll values would
	// saturate to the texture edge and kill animation. For helix
	// (the only current ribbon consumer, uvScroll = 0) fract is a
	// no-op since baseUV is already in [0, 1]; load-bearing for
	// any future scrolling ribbon consumer.
	//
	// (Beam took a different approach in Phase 5J: separate
	// REPEAT-mode sampler + no fract, which avoids fract(1.0)=0
	// V-collapse when the GLSL spec collapses the V=1 vertex.
	// Ribbon's V is per-point (varies smoothly along the strip)
	// not per-edge, so the V-collapse case doesn't apply here.)
	//
	// Ribbon has no per-submission spawnTime field — uvScroll is
	// referenced against the absolute frame clock, so per-frame
	// re-submissions don't disturb scroll continuity. If a future
	// persistent ribbon variant lands, restore the spawnTime field
	// (and host write at tr_scene.c RE_AddRibbonToScene) and
	// branch on a flag like beam.vert does.
	fragUV = fract(baseUV + hdr.uvScroll * frameParams.y);
	fragColor = p.rgba;
	fragShaderHandle = hdr.shaderHandle;
}
