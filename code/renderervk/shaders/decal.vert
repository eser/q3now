// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#version 450

/*
decal.vert — wired-render GPU decal projector vertex shader

6 vertices per decal, two triangles forming a surface-aligned quad. Each
draw issues vkCmdDraw(6, N) with N = DECALS_PER_POOL, so gl_InstanceIndex
selects the decal slot and gl_VertexIndex % 6 selects the corner.

The quad is built in the decal's own tangent frame (NOT camera-facing): a
stable tangent perpendicular to the surface normal, the bitangent =
normal × tangent, both rolled around the normal by the decal's stored
orientation. The unit corner (±1, ±1) maps to:

    origin + tangent*(corner.x*radius) + bitangent*(corner.y*radius)
           + normal*zbias

where zbias lifts the quad slightly off the surface to avoid z-fighting.

Dead / empty slots (spawnTime == 0 AND lifetimeInv == 0), age-expired marks,
and out-of-range instances emit a degenerate (zero-area) triangle the
rasterizer culls.

A mark holds full opacity for [0, total - fadeWindow] then ramps linearly to
nothing over the final fadeWindow (the vanilla-Q3 10s-hold + 1s-fade shape).
lifetimeInv = 1/lifetime, or 0 for no auto-fade (mark stays until its ring
slot is reused). The fade CHANNEL is picked from the blend mode, not a caller
hint, so it matches the pipeline that actually rasterizes the mark:
  blendMode 0 (alpha)         → ramp alpha to 0   (blood, plasma)
  blendMode 1/2 (add/modulate) → ramp rgb to black (burn, bullet); in both
                                 those pipelines a black source contributes
                                 nothing, so the mark cleanly disappears.

Layout matches host-side mirrors decalGPU_t / decalFrame_t (see vk.h).
*/

struct Decal {
	vec4 originRadius;   // xyz = origin, w = radius
	vec4 normalOrient;   // xyz = normal, w = orientation (radians)
	vec4 rgba;           // colour * alpha
	uint textureIndex;   // resolved decal texture slot
	float spawnTime;     // tr.refdef.floatTime at emit
	float lifetimeInv;   // 1/lifetime (0 = no auto-fade)
	uint blendMode;      // 0=alpha, 1=additive, 2=colour; selects the pipeline
};

// Fraction of a mark's total lifetime spent fading out at the end. 0.1 = the
// vanilla 1s fade over a 10s lifetime (MARK_FADE_TIME / MARK_TOTAL_TIME). The
// preceding 0.9 of the lifetime is held at full opacity.
const float DECAL_FADE_FRAC = 0.1;

// This pipeline's blend mode (set per-pipeline via VkSpecializationInfo). The
// three projector draws share this shader; each only rasterizes decals whose
// stored blendMode matches, degenerate-culling the rest.
layout(constant_id = 0) const uint DECAL_BLEND_MODE = 0u;

layout(set = 0, binding = 0) uniform DecalFrame {
	mat4 mvp;
	vec4 timeCount;      // x = scene floatTime, y = pool size, zw = pad
	mat4 invMvp;         // clip->world; the fragment shader reconstructs surface pos
	vec4 reconParams;    // xy = 1/render{W,H}, z = depthValid, w = pad
};

layout(std430, set = 0, binding = 1) readonly buffer Pool {
	Decal decals[];
};

layout(location = 0) out vec2 fragUV;
layout(location = 1) out vec4 fragColor;
// flat-interpolated texture slot (same for all 6 verts of one decal).
layout(location = 2) flat out uint fragTextureIndex;
// Decal tangent frame, forwarded flat (identical for all 6 verts) so the
// fragment shader can box-project the depth-reconstructed surface point into
// the decal's oriented box. rTangent/rBitangent are derived here (not stored in
// the SSBO), so they must be passed through rather than recomputed.
layout(location = 3) flat out vec3 fragDecalOrigin;
layout(location = 4) flat out vec3 fragDecalTangent;
layout(location = 5) flat out vec3 fragDecalBitangent;
layout(location = 6) flat out vec3 fragDecalNormal;
layout(location = 7) flat out float fragDecalRadius;
// Free-quad flag: bit 31 of the SSBO textureIndex (packed host-side from
// DECAL_FLAG_NO_PROJECT). 1 = render a free flat quad at the explicit origin Z
// (skip the fragment's depth box-projection); 0 = normal world-projected mark.
layout(location = 8) flat out uint fragNoProject;

out gl_PerVertex {
	vec4 gl_Position;
};

// Two-triangle quad. Corner (±1, ±1) in the decal's tangent plane; matching
// UVs map the [-1,1] quad to [0,1] texture space.
const vec2 cornerSign[6] = vec2[6](
	vec2(-1.0, -1.0), vec2(-1.0, +1.0), vec2(+1.0, -1.0),
	vec2(+1.0, -1.0), vec2(-1.0, +1.0), vec2(+1.0, +1.0)
);
const vec2 uvCorner[6] = vec2[6](
	vec2(0.0, 0.0), vec2(0.0, 1.0), vec2(1.0, 0.0),
	vec2(1.0, 0.0), vec2(0.0, 1.0), vec2(1.0, 1.0)
);

void emitDegenerate() {
	gl_Position        = vec4(0.0, 0.0, 0.0, 1.0);
	fragUV             = vec2(0.0);
	fragColor          = vec4(0.0);
	fragTextureIndex   = 0u;
	fragDecalOrigin    = vec3(0.0);
	fragDecalTangent   = vec3(0.0);
	fragDecalBitangent = vec3(0.0);
	fragDecalNormal    = vec3(0.0);
	fragDecalRadius    = 0.0;
	fragNoProject      = 0u;
}

void main() {
	uint decalIdx   = uint(gl_InstanceIndex);
	uint vertInQuad = uint(gl_VertexIndex) % 6u;

	if (float(decalIdx) >= timeCount.y) {
		emitDegenerate();
		return;
	}

	Decal d = decals[decalIdx];

	// Empty slot: spawnTime == 0 AND lifetimeInv == 0 (the zero-init state).
	if (d.spawnTime == 0.0 && d.lifetimeInv == 0.0) {
		emitDegenerate();
		return;
	}

	// This draw only handles its own blend mode; cull the others so each decal
	// rasterizes exactly once across the three full-pool passes.
	if (d.blendMode != DECAL_BLEND_MODE) {
		emitDegenerate();
		return;
	}

	// Hold-then-fade lifetime ramp (GPU-side, emit-and-forget). frac is the
	// mark's progress through its lifetime in [0,1]; once it passes 1.0 the mark
	// has fully aged out, so we cull its slot (it will be overwritten when the
	// ring wraps). lifetimeInv == 0 means "no auto-fade" — frac stays 0.
	float fadeAlpha = 1.0;
	if (d.lifetimeInv > 0.0) {
		float age  = timeCount.x - d.spawnTime;
		float frac = age * d.lifetimeInv;
		if (frac >= 1.0) {
			emitDegenerate();
			return;
		}
		// Full opacity until the final DECAL_FADE_FRAC of the lifetime, then a
		// linear ramp 1 → 0 across that window.
		fadeAlpha = clamp((1.0 - frac) / DECAL_FADE_FRAC, 0.0, 1.0);
	}

	vec3  origin = d.originRadius.xyz;
	float radius = d.originRadius.w;
	vec3  normal = d.normalOrient.xyz;
	float roll   = d.normalOrient.w;

	// Degenerate normal guard — a zero-length normal can't build a frame.
	float nlen = length(normal);
	if (nlen < 1e-4) {
		emitDegenerate();
		return;
	}
	normal /= nlen;

	// Stable tangent: cross with world up (0,0,1); if the normal is nearly
	// parallel to up, fall back to crossing with the x axis so the tangent
	// stays well-defined.
	vec3 up      = vec3(0.0, 0.0, 1.0);
	vec3 tangent = cross(normal, up);
	if (dot(tangent, tangent) < 1e-6) {
		tangent = cross(normal, vec3(1.0, 0.0, 0.0));
	}
	tangent = normalize(tangent);
	vec3 bitangent = normalize(cross(normal, tangent));

	// Apply the orientation roll around the normal (Rodrigues for the in-plane
	// pair — both tangent/bitangent are perpendicular to the normal, so the
	// rotation is a plain 2D rotation in the tangent plane).
	float cs = cos(roll);
	float sn = sin(roll);
	vec3  rTangent   =  cs * tangent + sn * bitangent;
	vec3  rBitangent = -sn * tangent + cs * bitangent;

	vec2  c     = cornerSign[vertInQuad];
	// Small positive z-bias off the surface to avoid z-fighting; scaled with
	// radius and floored so tiny decals still lift clear of the surface.
	float zbias = max(radius * 0.02, 0.5);

	vec3 worldPos = origin
	              + rTangent   * (c.x * radius)
	              + rBitangent * (c.y * radius)
	              + normal     * zbias;

	gl_Position      = mvp * vec4(worldPos, 1.0);
	fragUV           = uvCorner[vertInQuad];
	fragColor        = d.rgba;
	// Apply the lifetime fade, channel-keyed by this pipeline's blend mode
	// (DECAL_BLEND_MODE is the spec constant the surviving decal matched above):
	//   alpha (0)         → scale alpha so the mark dissolves.
	//   additive/modulate → scale rgb toward black; a black source adds nothing
	//                       (additive) and darkens nothing (modulate, blends
	//                       dst*(1-0)=dst), so the mark cleanly disappears.
	// fadeAlpha is 1.0 for no-fade decals (lifetimeInv 0), making this a no-op.
	if (DECAL_BLEND_MODE == 0u) {
		fragColor.a   *= fadeAlpha;
	} else {
		fragColor.rgb *= fadeAlpha;
	}
	// Low bits = the sampler slot (< MAX_DECAL_TEXTURES); bit 31 = the free-quad
	// (NO_PROJECT) flag packed host-side in RE_AddDecalToScene. Split them here.
	fragTextureIndex = d.textureIndex & 0x7FFFFFFFu;
	fragNoProject    = ( d.textureIndex >> 31u ) & 1u;

	// Forward the decal's tangent frame so the fragment shader can box-project
	// the depth-reconstructed surface point. origin/normal/radius come from the
	// SSBO; rTangent/rBitangent are the roll-rotated locals built above.
	fragDecalOrigin    = origin;
	fragDecalTangent   = rTangent;
	fragDecalBitangent = rBitangent;
	fragDecalNormal    = normal;
	fragDecalRadius    = radius;
}
