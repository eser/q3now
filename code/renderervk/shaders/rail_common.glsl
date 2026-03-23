#version 450

// rail_common.glsl — shared SSBO structs for rail trail compute + render
//
// Prepended to rail_helix.comp, rail_debris.comp, rail_sparks.comp, rail_helix.vert
// via compile.sh. Single source of truth for SSBO layout.
//
// IMPORTANT: #version 450 is here — individual shader files must NOT have it.

struct TrailParams {
	vec4  start;           // xyz = start position, w = beamLen
	vec4  beamAxis;        // xyz = normalized beam direction, w = frac (0..1)
	vec4  perpAxis[36];    // precomputed ring positions (xyz only, w unused)
	vec4  params;          // x = curRadius, y = curSpacing, z = curWidth, w = float(numSegments)
	vec4  color;           // rgba with fade alpha
	vec4  extra;           // x = rotation step, y = elapsed seconds, z/w reserved
};

struct GPUVertex {
	vec4 pos;              // xyz position, w = 1.0
	vec2 uv;               // texture coordinates
	vec2 _pad;             // alignment padding
	vec4 color;            // rgba [0..1]
};
