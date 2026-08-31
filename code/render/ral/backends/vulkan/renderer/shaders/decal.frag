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

// This pipeline's host-resolved decal blend mode. The vertex stage uses the
// same specialization constant to cull decals that belong to the other two
// pipelines; the fragment stage uses it to translate legacy colour-modulate
// marks into bounded, alpha-aware darkening.
layout(constant_id = 0) const uint DECAL_BLEND_MODE = 0u;

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

struct SurfaceClimateTile {
	ivec2 key;
	uvec2 meta;
	vec4 climate;
};
layout(std430, set = 0, binding = 4) readonly buffer SurfaceClimateTable {
	SurfaceClimateTile surfaceClimateTiles[256];
};

layout(location = 0) out vec4 outColor;

// How far along the decal normal the projected surface may sit and still be
// painted — the box's half-thickness. A half-radius slab lets a mark wrap onto
// a step beneath it without bleeding onto a wall well above the surface.
const float DECAL_BOX_HALF_THICKNESS_FRAC = 0.5;

// Legacy bullet marks use ZERO / ONE_MINUS_SRC_COLOR and store coverage in
// RGB. That blend ignores alpha and lets a white texel turn the destination
// mathematically black in one pass. A later translucent smoke particle cannot
// visually veil that zero-valued background, which makes the mark look as if
// it was drawn on top of the smoke. Preserve the authored RGB mask, but bound
// its darkening strength so impact smoke can composite over it normally.
const float DECAL_COLOUR_MAX_OPACITY = 0.42;

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

vec4 surfaceClimateAt( vec2 worldXY ) {
	ivec2 wanted = ivec2( floor( worldXY / 128.0 ) );
	uint count = min( uint( reconParams.w ), 256u );
	uint low = 0u;
	uint high = count;
	// Exact lower_bound over the frontend-sorted fixed table. Eight comparisons
	// cover all 256 entries; empty tables perform no storage read.
	for ( uint step = 0u; step < 8u; ++step ) {
		if ( low >= high ) break;
		uint middle = ( low + high ) >> 1u;
		ivec2 key = surfaceClimateTiles[middle].key;
		bool less = key.x < wanted.x || ( key.x == wanted.x && key.y < wanted.y );
		if ( less ) low = middle + 1u;
		else high = middle;
	}
	if ( low < count && all( equal( surfaceClimateTiles[low].key, wanted ) ) )
		return surfaceClimateTiles[low].climate;
	return vec4( 0.0 );
}

void main() {
	// Free quads use their authored flat UV. Projected decals replace it with the
	// UV of the actual surface point beneath this fragment.
	vec2 decalUV = fragUV;
	vec4 localSurfaceClimate = vec4( 0.0 );

	// Box-projection: reconstruct the surface world position from scene depth and
	// keep only fragments whose underlying surface falls inside the decal's
	// oriented box. A projected mark never falls back to flat-quad UVs: changing
	// coordinate paths across frames makes persistent marks jump. If the host
	// cannot provide fresh depth, fail closed for that projected fragment. The
	// engine uses reversed depth unconditionally: near≈1.0, far/cleared≈0.0.
	//
	// A NO_PROJECT (free-quad) decal ALSO skips this block: it is drawn verbatim at
	// its explicit world Z (the caller-computed water-line for the wake), sampling
	// the flat-quad UV directly — no sceneDepth read, no discard-on-no-solid, no
	// box test. Per-decal (fragNoProject from the SSBO), so projected impact marks
	// and free wake quads coexist in the same draw / pipeline.
	if ( fragNoProject == 0u ) {
		if ( reconParams.z <= 0.5 ) {
			discard;
		}
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
		localSurfaceClimate = surfaceClimateAt( worldPos.xy );

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
	float localWetness = clamp( localSurfaceClimate.x
		+ 0.5 * localSurfaceClimate.w, 0.0, 1.0 );
	float localFrost = clamp( localSurfaceClimate.y, 0.0, 1.0 );
	float localSnow = clamp( localSurfaceClimate.z, 0.0, 1.0 );
	float luminance = dot( rgb, vec3( 0.2126, 0.7152, 0.0722 ) );
	rgb *= mix( 1.0, 0.86, localWetness );
	rgb = mix( rgb, vec3( luminance * 0.88, luminance * 0.94, luminance ),
		localFrost * 0.45 );
	// Fresh accumulation partially veils older marks. Footprint/impact events
	// reduce the tile's snow first, so their own projected mark remains legible.
	float accumulationVisibility = mix( 1.0, 0.65, localSnow );
	alpha *= accumulationVisibility;

	if ( DECAL_BLEND_MODE == 2u ) {
		// Colour-modulate decals encode their shape in RGB rather than alpha.
		// Convert that mask into bounded black alpha-compositing. The vertex
		// lifetime fade already scales fragColor.rgb, so coverage still dissolves
		// through the final lifetime window without a second fade channel.
		float coverage = max( rgb.r, max( rgb.g, rgb.b ) );
		coverage = clamp( coverage * fragColor.a * accumulationVisibility,
			0.0, DECAL_COLOUR_MAX_OPACITY );
		outColor = vec4( 0.0, 0.0, 0.0, coverage );
	} else {
		outColor = vec4( rgb, alpha );
	}
}
