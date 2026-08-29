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

Layout matches the host atmFrame_t (1152 B std140) / atmParticleGPU_t (32 B
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
	float computePad0;
	float computePad1;
	float computePad2;
	vec4  windGust;
	vec4  precipitation;
	float dustAsh;
	float indoorExposure;
	uint  climateSeed;
	uint  climatePad;
	vec4  climate;
	vec4  surfaceClimate;
	vec4  sun;
	vec4  moon;
	vec4  ambientCloud;
	vec4  cloudMedia;
	vec4  effectMeta;
	vec4  effectWorkloads[48];
	vec4  renderParams;  // xy inverse resolution, z depthValid
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
const float HAIL_SIZE   = 0.8;
const float DUST_SIZE   = 1.2;

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

	if (idx >= poolSize || ( atmType == 0u && idx < uint( effectMeta.x ) )) {
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

	uint family = uint( p.flags + 0.5 );
	if ( family >= 16u ) {
		uint effect = family - 16u;
		if ( effect >= min( uint( effectMeta.y ), 8u ) ) {
			emitDegenerate();
			return;
		}
		uint base = effect * 6u;
		float lifetime = max( 0.05, effectWorkloads[base + 5u].x
			+ effectWorkloads[base + 5u].y );
		float age = 1.0 - clamp( p.seed / lifetime, 0.0, 1.0 );
		float size = mix( effectWorkloads[base + 4u].z,
			effectWorkloads[base + 4u].w, age );
		worldPos = p.pos + viewLeft.xyz * ( sx * size )
			+ viewUp.xyz * ( uy * size );
		color = effectWorkloads[base + 2u];
		color.a *= effectWorkloads[base + 1u].w;
	} else if (family == 1u || family == 3u) {
		// rain: vertical streak. Width spans the camera's left axis; length
		// runs straight down world -Z (the falling direction).
		vec3 right = viewLeft.xyz * (sx * RAIN_WIDTH);
		float length = family == 1u ? RAIN_STREAK : RAIN_STREAK * 0.55;
		vec3 down  = vec3(0.0, 0.0, 1.0) * (uy * 0.5 - 0.5) * length;
		worldPos = p.pos + right + down;
		color    = family == 1u ? vec4(0.5, 0.5, 0.55, 0.5)
			: vec4(0.72, 0.76, 0.8, 0.65);
	} else {
		float size = family == 4u ? HAIL_SIZE
			: ( family == 5u ? DUST_SIZE : SNOW_SIZE );
		worldPos = p.pos
		         + viewLeft.xyz * (sx * size)
		         + viewUp.xyz   * (uy * size);
		color = family == 4u ? vec4(0.82, 0.88, 0.92, 0.85)
			: ( family == 5u ? vec4(0.48, 0.42, 0.34, 0.45)
			: vec4(1.0, 1.0, 1.0, 0.8) );
	}

	// Weather particles share the authored sky/weather timeline. Preserve the
	// historical appearance when no lighting payload is authored, otherwise
	// consume ambient, cloud-attenuated sun, moon and lightning in linear space.
	float cloudTransmission = 1.0 - clamp(
		ambientCloud.w * cloudMedia.x, 0.0, 1.0 );
	vec3 weatherLight = ambientCloud.rgb
		+ vec3( max( sun.w, 0.0 ) * cloudTransmission )
		+ vec3( max( moon.w, 0.0 ) * 0.2 )
		+ vec3( max( cloudMedia.y, 0.0 ) );
	if ( dot( weatherLight, weatherLight ) > 1e-8 )
		color.rgb *= clamp( weatherLight, vec3( 0.04 ), vec3( 4.0 ) );

	gl_Position = mvp * vec4(worldPos, 1.0);
	fragUV      = uvCorner[vertInQuad];
	fragColor   = color;
}
