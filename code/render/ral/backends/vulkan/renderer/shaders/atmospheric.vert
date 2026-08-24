// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#version 450

/*
atmospheric.vert — wired-render atmospheric weather vertex shader

6 vertices per particle (two triangles). The draw issues
vkCmdDraw(6, ATM_PARTICLES_PER_POOL), so gl_InstanceIndex selects the slot
and gl_VertexIndex % 6 selects the corner.

Rain renders as a vertical streak (the quad is stretched along world -Z by
a streak length); snow renders as a small camera-facing billboard built from
the view axes (viewLeft / viewUp) the host writes into the frame UBO. Dead /
inactive slots emit a degenerate (zero) position so the rasterizer culls them.

Layout matches the host atmFrame_t (208 B std140) / atmParticleGPU_t (32 B
std430) mirrors in vk.h.
*/

struct AtmParticle {
	vec3  pos;
	float seed;
	vec3  vel;
	float flags;   // 0 = inactive, 1 = falling
};

layout(set = 0, binding = 0) uniform AtmFrame {
	mat4  mvp;
	vec4  viewLeft;
	vec4  viewUp;
	vec4  eyeWorld;
	float dt;
	float time;
	uint  poolSize;
	uint  pingPongRead;
	vec4  boundsMin;
	vec4  boundsMax;
	vec2  worldMins;
	vec2  worldMaxs;
	vec2  invGridStep;
	uint  gridSize;
	uint  atmType;
	float distance;
	float invResX;       // 1/renderWidth  (screen UV from gl_FragCoord, frag-only)
	float invResY;       // 1/renderHeight
	float depthValid;    // 1.0 when the shared scene-depth copy is fresh this frame
};

layout(std430, set = 0, binding = 1) readonly buffer Pool {
	AtmParticle particles[];
};

layout(location = 0) out vec2 fragUV;
layout(location = 1) out vec4 fragColor;

out gl_PerVertex {
	vec4 gl_Position;
};

// Rain streak length (world units, along -Z) and half-width.
const float RAIN_STREAK = 18.0;
const float RAIN_WIDTH  = 0.6;
// Snow billboard half-size.
const float SNOW_SIZE   = 1.5;

const float sideSign[6] = float[6](-1.0, -1.0, +1.0,  +1.0, -1.0, +1.0);
const float upSign[6]   = float[6](+1.0, -1.0, +1.0,  +1.0, -1.0, -1.0);
const vec2  uvCorner[6] = vec2[6](
	vec2(0.0, 0.0), vec2(0.0, 1.0), vec2(1.0, 0.0),
	vec2(1.0, 0.0), vec2(0.0, 1.0), vec2(1.0, 1.0)
);

void emitDegenerate() {
	gl_Position = vec4(0.0, 0.0, 0.0, 1.0);
	fragUV    = vec2(0.0);
	fragColor = vec4(0.0);
}

void main() {
	uint idx        = uint(gl_InstanceIndex);
	uint vertInQuad = uint(gl_VertexIndex) % 6u;

	if (idx >= poolSize || atmType == 0u) {
		emitDegenerate();
		return;
	}

	AtmParticle p = particles[idx];

	if (p.flags < 0.5) {
		emitDegenerate();
		return;
	}

	float sx = sideSign[vertInQuad];
	float uy = upSign[vertInQuad];

	vec3 worldPos;
	vec4 color;

	if (atmType == 1u) {
		// rain: vertical streak. Width spans the camera's left axis; length
		// runs straight down world -Z (the falling direction).
		vec3 right = viewLeft.xyz * (sx * RAIN_WIDTH);
		vec3 down  = vec3(0.0, 0.0, 1.0) * (uy * 0.5 - 0.5) * RAIN_STREAK; // uy:+1 -> 0 (top), -1 -> -streak
		worldPos = p.pos + right + down;
		color    = vec4(0.5, 0.5, 0.55, 0.5);   // grey, ~half alpha
	} else {
		// snow: small camera-facing billboard.
		worldPos = p.pos
		         + viewLeft.xyz * (sx * SNOW_SIZE)
		         + viewUp.xyz   * (uy * SNOW_SIZE);
		color    = vec4(1.0, 1.0, 1.0, 0.8);    // white
	}

	gl_Position = mvp * vec4(worldPos, 1.0);
	fragUV      = uvCorner[vertInQuad];
	fragColor   = color;
}
