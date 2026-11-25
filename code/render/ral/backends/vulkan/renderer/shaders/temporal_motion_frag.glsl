// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
//
// TAA motion outputs of a temporal fragment sibling (velocity at location 1, validity at location 2), for the shaders that
// are not built from gen_frag.tmpl (q1_ls*.frag). The block below is a verbatim copy of gen_frag.tmpl's
// USE_TEMPORAL_MOTION block; cmake/vk_temporal_shader_policy_check.cmake keeps the two identical.

#ifdef USE_TEMPORAL_MOTION
layout(location = 10) in vec4 temporalCurrentClip;
layout(location = 11) in vec4 temporalPreviousClip;
layout(location = 12) flat in uint temporalOutcome;
layout(location = 1) out vec2 out_temporal_velocity;
// vec4 so an INVALIDATE draw can hand the validity blend its contribution in .r and its alpha in .a (see
// VK_TemporalInvalidateAuxBlends); the attachment is R8, only .r is stored.
layout(location = 2) out vec4 out_temporal_validity;

bool wiredTemporalFinite4( vec4 value ) {
	return all( equal( value, value ) )
		&& all( lessThanEqual( abs( value ), vec4( 3.402823466e38 ) ) );
}
bool wiredTemporalFinite2( vec2 value ) {
	return all( equal( value, value ) )
		&& all( lessThanEqual( abs( value ), vec2( 3.402823466e38 ) ) );
}

void wiredTemporalWriteAux( float coverageConfidence, vec4 sceneColor ) {
	out_temporal_velocity = vec2( 0.0 );
	out_temporal_validity = vec4( 0.0 );
#ifdef USE_TEMPORAL_INVALIDATE
	// The pipeline's validity blend decides what this means: an opaque INVALIDATE blends it away to zero, a blended one
	// subtracts its visible contribution (max channel of what it adds, weighted by the scene's own blend factors).
	out_temporal_validity = vec4( clamp( max( sceneColor.r, max( sceneColor.g, sceneColor.b ) ), 0.0, 1.0 ), 0.0, 0.0,
		clamp( sceneColor.a, 0.0, 1.0 ) );
#else
	if ( temporalOutcome != 1u || !wiredTemporalFinite4( temporalCurrentClip )
			|| !wiredTemporalFinite4( temporalPreviousClip )
			|| temporalCurrentClip.w <= 1.0e-6
			|| temporalPreviousClip.w <= 1.0e-6 ) return;
	vec2 currentNdc = temporalCurrentClip.xy / temporalCurrentClip.w;
	vec2 previousNdc = temporalPreviousClip.xy / temporalPreviousClip.w;
	if ( !wiredTemporalFinite2( currentNdc )
			|| !wiredTemporalFinite2( previousNdc ) ) return;
	vec2 currentUv = currentNdc * 0.5 + vec2( 0.5 );
	vec2 previousUv = previousNdc * 0.5 + vec2( 0.5 );
	vec2 velocity = currentUv - previousUv;
	if ( !wiredTemporalFinite2( velocity ) ) return;
	out_temporal_velocity = velocity;
	out_temporal_validity = vec4( clamp( coverageConfidence, 0.0, 1.0 ), 0.0, 0.0, 1.0 );
#endif
}
#endif
