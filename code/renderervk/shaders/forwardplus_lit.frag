// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#version 450
// Forward+ world-space lit fragment shader (the tiled-lighting consumer).
//
// Computes the fragment's screen tile from gl_FragCoord, reads that tile's light-
// index list (the tile-classification compute's output SSBO), and for EACH light
// sums the SAME per-light contribution the PMLIGHT additive pass computes — the
// falloff + classic-Phong BRDF are BYTE-COPIED from light_frag.tmpl:344-355,427-453
// (the equivalence basis). Only the COMBINATION differs (in-shader sum vs PMLIGHT's
// additive-FBO accumulate). The base colour is the single diffuse texture (role 0),
// the SAME `base` light_frag uses — NOT a lightmap modulate (the additive dlight
// pass adds `(base*diffuse + spec)*intens`; the lightmap is the base pass's job).
//
// WebGPU lens: storage-buffer reads + a bounded per-fragment loop, set-based (no
// push-constants for tile data).

#extension GL_EXT_nonuniform_qualifier : require

#define TILE_SIZE 16
#define MAX_LIGHTS_PER_TILE 32
#define TILELIGHT_STRIDE ( 1 + MAX_LIGHTS_PER_TILE )

// set 0 — per-draw UBO (the bindless table at offset 544, same as gen_frag/light_frag).
layout(set = 0, binding = 0) uniform UBO {
	vec4 eyePos;
	vec4 _pad_light[3];
	vec4 fogDistanceVector;
	vec4 fogDepthVector;
	vec4 fogEyeT;
	vec4 fogColor;
	vec4 _pad_to_packed_indices[26];   // 128 -> 544
	uvec4 packed_indices[3];           // offset 544
};

// set 1 — bindless 2D textures.
layout(set = 1, binding = 0) uniform texture2D wired_bindless_images[];
layout(set = 1, binding = 1) uniform sampler   wired_bindless_samplers[];
#define WIRED_BINDLESS_PACKED(role) packed_indices[ (role) / 4u ][ (role) % 4u ]
#define WIRED_BINDLESS_TEX(role) sampler2D( \
	wired_bindless_images  [ nonuniformEXT(   WIRED_BINDLESS_PACKED( role )         & 0xFFFu ) ], \
	wired_bindless_samplers[ nonuniformEXT( ( WIRED_BINDLESS_PACKED( role ) >> 12 ) & 0xFFu  ) ] )
#define texture0 0u

// set 2 — Forward+ resources: the tile light list (4) + dlight params (5) + the
// tile-grid params (6). Alongside the existing engine-resources bindings (0/1/2/3)
// in layout terms, but the fp pipeline binds only this dedicated set.
struct Light {
	vec4 posRadius;   // xyz = world origin, w = radius
	vec4 color;       // rgb = linear color, w = 1/r^2 falloff (== lightColorRadius.w)
	vec4 posRadius2;  // xyz = tube endpoint, w = isLine
};
layout(std430, set = 2, binding = 4) readonly buffer TileLights {
	uint tileLights[];   // per tile: [count, idx0, ...] stride TILELIGHT_STRIDE
};
layout(std430, set = 2, binding = 5) readonly buffer DLightParams {
	Light dlights[];
};
layout(set = 2, binding = 6, std140) uniform TileParams {
	vec4 tileParams;   // x=screenW, y=screenH, z=tilesX, w=tilesY
};
// Point-light omni shadow (top-K budgeted lights). The atlas is a 6*K-wide strip of
// depth tiles: light L occupies columns [L*6 .. L*6+6) (one per cube face); each
// light's 6 face MVPs project world_pos → that face's NDC for the depth compare.
// shadowLights[s] = a budgeted light slot (x=dlights[] index, y=valid, z=bias);
// shadowMeta.x = numShadowLights, shadowMeta.y = the atlas column count (6*K, the UV
// divisor). K_MAX=4 → 6*4=24 face MVPs + 4 light slots. K=1 → only slot 0 + columns
// 0-5 are used, with /6.0 UV → byte-identical to a single-light atlas.
#define DLIGHT_SHADOW_K_MAX 4
layout(set = 2, binding = 7) uniform texture2D dlightShadowAtlas;
layout(set = 2, binding = 8) uniform sampler   dlightShadowSampler;
layout(set = 2, binding = 9, std140) uniform DlightShadowParams {
	mat4 shadowFaceMVP[6 * DLIGHT_SHADOW_K_MAX];   // light L face f at index L*6+f
	vec4 shadowLights[DLIGHT_SHADOW_K_MAX];        // x=lightIndex, y=valid, z=bias, w=unused
	vec4 shadowMeta;                               // x=numShadowLights, y=column count (6*K)
};

// r_unbakeStaticLights WORLD-space cluster grid (bindings 10/11). Static BSP lights are
// binned at map load into a uniform 3D world grid; a fragment gathers its static set
// from the grid CELL its world position falls in — NOT from its screen tile — so the
// set depends only on world geometry, never the camera (fixes the view-dependent
// "flashlight"). Each cell holds [count, idx0..] absolute dlights[] slots (stride
// CLUSTER_STRIDE). When the cvar is off / no static lights, the empty fallback grid
// (dims=(1,1,1), count=0) is bound → the cluster loop iterates zero times (byte-identical).
#define MAX_LIGHTS_PER_CLUSTER 32
#define CLUSTER_STRIDE ( 1 + MAX_LIGHTS_PER_CLUSTER )
layout(std430, set = 2, binding = 10) readonly buffer ClusterLights {
	uint clusterLights[];   // per cell: [count, idx0..] stride CLUSTER_STRIDE
};
layout(set = 2, binding = 11, std140) uniform ClusterGridParams {
	vec4  gridOrigin;   // xyz = world-space grid origin, w = cell size
	ivec4 gridDims;     // xyz = cell counts per axis, w = pad
};

// Manual cube-face-select + atlas sample (2D atlas, NOT samplerCube — WebGPU portable).
// From the world→light vector, pick the dominant axis → face index, project world_pos by
// that light's face MVP → NDC → atlas-column UV → depth-compare for occlusion. lightSlot
// selects the light's columns ([lightSlot*6 .. lightSlot*6+6)) of the 6*K-wide strip.
// Returns 1.0 (lit) when unshadowed / out of the face, 0.0 (shadowed) when occluded.
float dlightShadowOcclusion( vec3 worldPos, vec3 lightToFrag, int lightSlot ) {
	vec3 a = abs( lightToFrag );
	int face;
	if ( a.x >= a.y && a.x >= a.z )      face = ( lightToFrag.x >= 0.0 ) ? 0 : 1;
	else if ( a.y >= a.z )               face = ( lightToFrag.y >= 0.0 ) ? 2 : 3;
	else                                 face = ( lightToFrag.z >= 0.0 ) ? 4 : 5;

	int col = lightSlot * 6 + face;
	vec4 clip = shadowFaceMVP[col] * vec4( worldPos, 1.0 );
	if ( clip.w <= 0.0 ) return 1.0;            // behind the face plane → not in this face
	vec3 ndc = clip.xyz / clip.w;
	if ( ndc.x < -1.0 || ndc.x > 1.0 || ndc.y < -1.0 || ndc.y > 1.0 || ndc.z > 1.0 ) return 1.0;

	vec2 faceUV = ndc.xy * 0.5 + 0.5;           // [0,1] within the face
	float cols = max( shadowMeta.y, 6.0 );      // 6*K (K=1 → 6.0, identical to single-light)
	// atlas column c spans U in [c/cols, (c+1)/cols].
	vec2 atlasUV = vec2( ( float(col) + faceUV.x ) / cols, faceUV.y );
	float storedDepth = texture( sampler2D( dlightShadowAtlas, dlightShadowSampler ), atlasUV ).r;
	float receiverDepth = ndc.z;
	// LESS_OR_EQUAL depth, clear 1.0: a receiver deeper than the stored caster depth
	// (by more than bias) is occluded.
	return ( receiverDepth - shadowLights[lightSlot].z > storedDepth ) ? 0.0 : 1.0;
}

layout(location = 0) in vec2 frag_tex_coord;
layout(location = 1) in vec3 world_pos;
layout(location = 2) in vec3 world_normal;
layout(location = 3) in vec3 world_view;

layout(location = 0) out vec4 out_color;

#include "colorspace.glsl"

// Per-light contribution — the SAME classic-Phong falloff + BRDF the PMLIGHT additive
// pass computes (byte-for-byte light_frag.tmpl:350-355 intens + 427-453 base*diffuse+spec).
// Shared by BOTH the screen-tile loop (runtime dlights) and the world-cluster loop
// (static lights) so their per-light math is identical — the only difference is which
// index list drives them (screen tile vs world cell). The compiler inlines this, so the
// tile loop's output is instruction-identical to the previous inline form.
vec3 fp_light_contrib( uint li, vec3 Np, vec3 nV, vec4 base, vec3 world_pos ) {
	Light L = dlights[ li ];
	vec3  Lvec   = L.posRadius.xyz - world_pos;
	float falloff = L.color.w;   // 1/r^2
	float intensFactor = 1.0 - dot( Lvec, Lvec ) * falloff;
	if ( intensFactor <= 0.0 )
		return vec3( 0.0 );
	vec3 intens = L.color.rgb * intensFactor;

	vec3 nL = normalize( Lvec );
	float diffuse = max( dot( Np, nL ), 0.0 );
	float specFactor = max( dot( Np, normalize( nL + nV ) ), 0.0 );
	vec4 spec = vec4( pow( specFactor, 10.0 ) * 0.25 ) * base * 0.8;

	// Omni shadow: only budgeted shadow-casting lights are occluded; others ×1.0.
	float occ = 1.0;
	int numShadowLights = int( shadowMeta.x + 0.5 );
	for ( int s = 0; s < numShadowLights; s++ ) {
		if ( shadowLights[s].y > 0.5 && uint( shadowLights[s].x + 0.5 ) == li ) {
			occ = dlightShadowOcclusion( world_pos, -Lvec, s );   // -Lvec = light→fragment
			break;
		}
	}
	return ( base.rgb * diffuse + spec.rgb ) * intens * occ;
}

void main() {
	// base = the single diffuse texture, sRGB→linear (light_frag's sampleColorTex).
	vec4 base = texture( WIRED_BINDLESS_TEX( texture0 ), frag_tex_coord );
	base.rgb = sRGBToLinear( base.rgb );

	// tile index from the fragment's screen position (matches the compute's grid).
	uint tx = uint( gl_FragCoord.x ) / uint( TILE_SIZE );
	uint ty = uint( gl_FragCoord.y ) / uint( TILE_SIZE );
	uint tilesX = uint( tileParams.z );
	uint tilesY = uint( tileParams.w );
	if ( tilesX == 0u ) tilesX = 1u;
	if ( tx >= tilesX ) tx = tilesX - 1u;
	if ( ty >= tilesY ) ty = tilesY - 1u;
	uint tileBase = ( ty * tilesX + tx ) * uint( TILELIGHT_STRIDE );

	uint count = tileLights[ tileBase ];
	if ( count > uint( MAX_LIGHTS_PER_TILE ) ) count = uint( MAX_LIGHTS_PER_TILE );

	vec3 Np = normalize( world_normal );
	vec3 nV = normalize( world_view );

	vec3 lit = vec3( 0.0 );

	// Runtime dlights: the fragment's SCREEN tile's light list (per-frame, view-dependent
	// membership — correct for genuinely dynamic lights).
	for ( uint k = 0u; k < count; k++ )
		lit += fp_light_contrib( tileLights[ tileBase + 1u + k ], Np, nV, base, world_pos );

	// Static BSP lights (r_unbakeStaticLights): the fragment's WORLD cluster cell's light
	// list (built once at map load — membership depends only on world position, so it is
	// identical from every camera angle → no "flashlight"). Empty fallback grid → cN=0.
	float cs = gridOrigin.w;
	if ( cs > 0.0 ) {
		ivec3 cell = ivec3( floor( ( world_pos - gridOrigin.xyz ) / cs ) );
		if ( all( greaterThanEqual( cell, ivec3( 0 ) ) ) && all( lessThan( cell, gridDims.xyz ) ) ) {
			int  ci    = ( cell.z * gridDims.y + cell.y ) * gridDims.x + cell.x;
			uint cBase = uint( ci ) * uint( CLUSTER_STRIDE );
			uint cN    = clusterLights[ cBase ];
			if ( cN > uint( MAX_LIGHTS_PER_CLUSTER ) ) cN = uint( MAX_LIGHTS_PER_CLUSTER );
			for ( uint k = 0u; k < cN; k++ )
				lit += fp_light_contrib( clusterLights[ cBase + 1u + k ], Np, nV, base, world_pos );
		}
	}

	out_color = vec4( lit, base.a );
}
