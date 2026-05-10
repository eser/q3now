#version 450

/*
sprite.vert — primitive sprite vertex shader

Each draw issues 6 vertices × N instances. gl_InstanceIndex
selects the SpriteHeader; gl_VertexIndex % 6 selects the corner
of the billboard quad. Two triangles per sprite, oriented around
the camera view axes pushed via push constants.

Per-sprite quad layout (in screen-aligned space):
    0 = bottom-left   1 = top-left
    2 = bottom-right  3 = top-right
Triangle 0 = (BL, TL, BR), triangle 1 = (BR, TL, TR).

Push range layout (VERTEX | FRAGMENT, 112 bytes):
    bytes   0..63   mat4  mvp           — world MVP, Y-flipped for Vulkan
    bytes  64..79   vec4  viewLeft      — .xyz = camera-left in world space
    bytes  80..95   vec4  viewUp        — .xyz = camera-up in world space
    bytes  96..111  vec4  frameParams   — .x = tr.identityLight,
                                          .yzw reserved
frameParams is consumed by sprite.frag (CGEN_VERTEX-equivalent
halving), so the host-side push range covers VERTEX | FRAGMENT.
*/

layout(push_constant) uniform Push {
	mat4 mvp;
	vec4 viewLeft;
	vec4 viewUp;
	vec4 frameParams;
};

struct SpriteHeader {
	vec4 originW;       // .xyz = world position, .w = radius (full)
	vec4 rgba;
	uint shaderHandle;  // reserved for future texturing path
	uint flags;         // PRIM_FLAG_*
	uint pad0;
	uint pad1;
};

layout(std430, set = 0, binding = 0) readonly buffer Sprites
	{ SpriteHeader sprites[]; };

layout(location = 0) out vec2 fragUV;
layout(location = 1) out vec4 fragColor;

out gl_PerVertex {
	vec4 gl_Position;
};

// sideSign[k]  = +1 when the vertex is on the viewLeft side of the sprite
// upSign[k]    = +1 when the vertex is on the viewUp   side of the sprite
// uvCorner[k]  = standard texture corner UV (origin top-left, +V down)
const float sideSign[6] = float[6](+1.0, +1.0, -1.0,  -1.0, +1.0, -1.0);
const float upSign[6]   = float[6](-1.0, +1.0, -1.0,  -1.0, +1.0, +1.0);
const vec2  uvCorner[6] = vec2[6](
	vec2(0.0, 1.0), vec2(0.0, 0.0), vec2(1.0, 1.0),
	vec2(1.0, 1.0), vec2(0.0, 0.0), vec2(1.0, 0.0)
);

void main() {
	SpriteHeader hdr = sprites[gl_InstanceIndex];

	uint  vertInQuad = uint(gl_VertexIndex) % 6u;
	float sx     = sideSign[vertInQuad];
	float uy     = upSign[vertInQuad];
	float radius = hdr.originW.w;

	vec3 worldPos = hdr.originW.xyz
	              + viewLeft.xyz * (sx * radius)
	              + viewUp.xyz   * (uy * radius);

	gl_Position = mvp * vec4(worldPos, 1.0);
	fragUV    = uvCorner[vertInQuad];
	fragColor = hdr.rgba;
}
