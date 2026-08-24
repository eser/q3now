// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#version 450

/*
decal.frag — wired-render GPU decal projector fragment shader

Per-decal texture sampling from a bounded COMBINED_IMAGE_SAMPLER array
(binding 2, MAX_DECAL_TEXTURES = 64). The vertex shader forwards the
decal's resolved texture slot as a flat-interpolated uint (same across all
six vertices of one decal — flat is uniform-correct, no nonuniform
qualifier needed). The slot is filled host-side from each decal shader's
resolved stages[0]→bundle[0]→image[0] (three-tier fallback to defaultShader
then tr.whiteImage) by RE_AddDecalToScene's find-or-add registry.

Output = sampled texel × interpolated decal colour. The decal colour is a
display-domain RGBA (with alpha); the texel is display-domain (sRGB)
content. Both rgb are decoded to linear before the modulate so the alpha-
blend into the HDR FBO is linear-correct; alpha stays raw.
*/

layout(location = 0) in vec2 fragUV;
layout(location = 1) in vec4 fragColor;
layout(location = 2) flat in uint fragTextureIndex;
// Decal tangent frame (flat, identical across the quad), forwarded from the
// vertex shader for box-projection.
layout(location = 3) flat in vec3 fragDecalOrigin;
layout(location = 4) flat in vec3 fragDecalTangent;
layout(location = 5) flat in vec3 fragDecalBitangent;
layout(location = 6) flat in vec3 fragDecalNormal;
layout(location = 7) flat in float fragDecalRadius;
// Free-quad flag (bit 31 of the SSBO textureIndex, DECAL_FLAG_NO_PROJECT). 1 =
// render the quad verbatim at its world Z — skip the scene-depth box-projection
// (no sceneDepth read, no discard-on-no-solid, no box test). For marks that sit
// at a caller-computed height with no solid beneath (the water-line wake).
layout(location = 8) flat in uint fragNoProject;

// Same UBO the vertex shader binds; the fragment shader now consumes invMvp +
// reconParams to reconstruct the surface world position from scene depth.
layout(set = 0, binding = 0) uniform DecalFrame {
	mat4 mvp;
	vec4 timeCount;
	mat4 invMvp;         // clip->world (inverse of the y-flipped mvp)
	vec4 reconParams;    // xy = 1/render{W,H}, z = depthValid, w = pad
};

layout(set = 0, binding = 2) uniform sampler2D decalTextures[64];
// The shared scene-depth copy (vk.sceneDepth), same resource the GTAO/lens/
// tonemap consumers sample. Used to reconstruct the surface beneath the decal.
layout(set = 0, binding = 3) uniform sampler2D sceneDepthTex;

layout(location = 0) out vec4 outColor;

// How far along the decal normal the projected surface may sit and still be
// painted — the box's half-thickness. A half-radius slab lets a mark wrap onto
// a step beneath it without bleeding onto a wall well above the surface.
const float DECAL_BOX_HALF_THICKNESS_FRAC = 0.5;

// Precise piecewise sRGB → linear conversion. Duplicated per shader (the
// compile.mjs preprocessor lacks #include); matches the other shader copies
// verbatim per the engine-wide unconditional linear migration.
vec3 sRGBToLinear( vec3 c ) {
	c = max( c, vec3( 0.0 ) );
	bvec3 cutoff = lessThanEqual( c, vec3( 0.04045 ) );
	vec3 lo = c / 12.92;
	vec3 hi = pow( ( c + vec3( 0.055 ) ) / 1.055, vec3( 2.4 ) );
	return mix( hi, lo, vec3( cutoff ) );
}

void main() {
	// Default to the legacy flat-quad UV; the box-projection path overrides it
	// with the UV of the actual surface point beneath this fragment.
	vec2 decalUV = fragUV;

	// Box-projection: reconstruct the surface world position from scene depth and
	// keep only fragments whose underlying surface falls inside the decal's
	// oriented box. Skipped (flat-quad fallback) when the depth copy is not fresh
	// this frame (reconParams.z == 0), so a mark still appears rather than reading
	// undefined depth. The engine uses reversed depth unconditionally: near≈1.0,
	// far/cleared≈0.0.
	//
	// A NO_PROJECT (free-quad) decal ALSO skips this block: it is drawn verbatim at
	// its explicit world Z (the caller-computed water-line for the wake), sampling
	// the flat-quad UV directly — no sceneDepth read, no discard-on-no-solid, no
	// box test. Per-decal (fragNoProject from the SSBO), so projected impact marks
	// and free wake quads coexist in the same draw / pipeline.
	if ( reconParams.z > 0.5 && fragNoProject == 0u ) {
		vec2 screenUV = gl_FragCoord.xy * reconParams.xy;
		float sceneDepth = texture( sceneDepthTex, screenUV ).r;

		// No geometry under this fragment (cleared far plane) → nothing to project
		// onto. discard rather than paint the airborne quad — this is the floating-
		// decal fix.
		if ( sceneDepth <= 0.0 ) {
			discard;
		}

		// Reconstruct world position. invMvp inverts the SAME y-flipped mvp, so the
		// Vulkan-convention NDC (xy from gl_FragCoord, z = raw reversed-depth) maps
		// straight back to world space.
		vec3 ndc = vec3( screenUV * 2.0 - 1.0, sceneDepth );
		vec4 worldH = invMvp * vec4( ndc, 1.0 );
		vec3 worldPos = worldH.xyz / worldH.w;

		// Project the surface point into the decal's tangent frame.
		vec3  local = worldPos - fragDecalOrigin;
		float invR  = ( fragDecalRadius > 0.0 ) ? 1.0 / fragDecalRadius : 0.0;
		float u = dot( local, fragDecalTangent )   * invR;   // [-1, 1] inside the box
		float v = dot( local, fragDecalBitangent ) * invR;   // [-1, 1] inside the box
		float w = dot( local, fragDecalNormal );             // distance along the normal

		// Outside the in-plane box, or too far along the normal (a surface above/
		// below the decal's slab — e.g. a wall the quad happens to overlap) → discard.
		float halfThickness = fragDecalRadius * DECAL_BOX_HALF_THICKNESS_FRAC;
		if ( abs( u ) > 1.0 || abs( v ) > 1.0 || abs( w ) > halfThickness ) {
			discard;
		}

		// Sample the texture at the PROJECTED surface point, not the flat-quad
		// corner, so the mark lands where it actually meets geometry. Orientation
		// matches uvCorner (corner (±1,±1) → uv (0/1)): decalUV = u,v * 0.5 + 0.5.
		decalUV = vec2( u, v ) * 0.5 + 0.5;
	}

	vec4 texel = texture( decalTextures[fragTextureIndex], decalUV );
	vec3 rgb   = sRGBToLinear( texel.rgb ) * sRGBToLinear( fragColor.rgb );
	float alpha = texel.a * fragColor.a;

	outColor = vec4( rgb, alpha );
}
