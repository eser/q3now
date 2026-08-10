// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

// sRGB EOTF / OETF (exact piecewise, not the pow(x,2.2) approximation) — matches
// RBDOOM-3-BFG / Daemon convention and the renderer's CPU-side mipmap LUTs, so
// the colour-space pipeline stays precise end-to-end. The defensive max(c,0.0)
// guards against negatives that could reach these from a float-FBO read (a UNORM
// sample is already non-negative). Shared by gen_frag + light_frag via #include.
vec3 sRGBToLinear( vec3 c ) {
	c = max( c, vec3( 0.0 ) );
	bvec3 cutoff = lessThanEqual( c, vec3( 0.04045 ) );
	vec3 lo = c / 12.92;
	vec3 hi = pow( ( c + vec3( 0.055 ) ) / 1.055, vec3( 2.4 ) );
	return mix( hi, lo, vec3( cutoff ) );
}

vec3 linearToSRGB( vec3 c ) {
	c = max( c, vec3( 0.0 ) );
	bvec3 cutoff = lessThanEqual( c, vec3( 0.0031308 ) );
	vec3 lo = c * 12.92;
	vec3 hi = pow( c, vec3( 1.0 / 2.4 ) ) * 1.055 - 0.055;
	return mix( hi, lo, vec3( cutoff ) );
}
