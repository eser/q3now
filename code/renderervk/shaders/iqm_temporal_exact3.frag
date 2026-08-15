// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#version 450
#extension GL_EXT_nonuniform_qualifier : require

layout(set = 1, binding = 0) uniform texture2D wired_bindless_images[];
layout(set = 1, binding = 1) uniform sampler wired_bindless_samplers[];

layout(push_constant) uniform TemporalIqmSurfacePush {
	uint imageSlot;
	uint samplerSlot;
} surfacePush;

layout(location = 0) in vec2 frag_tex_coord;
layout(location = 1) in vec3 frag_normal;
layout(location = 2) in vec4 frag_tangent;
layout(location = 10) in vec4 temporalCurrentClip;
layout(location = 11) in vec4 temporalPreviousClip;

layout(location = 0) out vec4 out_color;
layout(location = 1) out vec2 out_temporal_velocity;
layout(location = 2) out float out_temporal_validity;

#include "colorspace.glsl"

bool wiredTemporalFinite4( vec4 value ) {
	return all( not( isnan( value ) ) ) && all( not( isinf( value ) ) );
}

void main() {
	vec4 sampled = texture( sampler2D(
		wired_bindless_images[nonuniformEXT( surfacePush.imageSlot )],
		wired_bindless_samplers[nonuniformEXT( surfacePush.samplerSlot )] ),
		frag_tex_coord );
	out_color = vec4( sRGBToLinear( sampled.rgb ), sampled.a );
	out_temporal_velocity = vec2( 0.0 );
	out_temporal_validity = 0.0;

#ifndef USE_TEMPORAL_INVALIDATE
	if ( !wiredTemporalFinite4( temporalCurrentClip )
			|| !wiredTemporalFinite4( temporalPreviousClip )
			|| temporalCurrentClip.w <= 1.0e-6
			|| temporalPreviousClip.w <= 1.0e-6 ) return;
	vec2 currentNdc = temporalCurrentClip.xy / temporalCurrentClip.w;
	vec2 previousNdc = temporalPreviousClip.xy / temporalPreviousClip.w;
	if ( any( isnan( currentNdc ) ) || any( isinf( currentNdc ) )
			|| any( isnan( previousNdc ) ) || any( isinf( previousNdc ) ) ) return;
	vec2 currentUv = currentNdc * 0.5 + vec2( 0.5 );
	vec2 previousUv = previousNdc * 0.5 + vec2( 0.5 );
	vec2 velocity = currentUv - previousUv;
	if ( any( isnan( velocity ) ) || any( isinf( velocity ) ) ) return;
	out_temporal_velocity = velocity;
	out_temporal_validity = 1.0;
#endif
}
