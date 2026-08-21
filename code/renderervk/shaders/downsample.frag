// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#version 450

// CoD/Jimenez dual-filtering DOWNSAMPLE (Jimenez 2014, "Next Generation Post
// Processing in Call of Duty: Advanced Warfare").
//
// 13 bilinear taps from the source mip: an inner 2x2 box at +/-1 texel and an
// outer 3x3 ring at +/-2 texels (plus the centre). The taps are accumulated in
// five weighted groups so the kernel is partition-of-unity:
//   inner 2x2 (a,b,c,d)        weight 0.5   (0.125 each)
//   outer corners (e,g,k,m)    weight 0.5   over 4 -> 0.125 group, 1/16 each tap
//   outer edges  (f,h,j,l)     weight 0.5   over 4 -> shared, 1/8 each
//   centre (i)                 weight 0.125
// (the four overlapping groups of the ring sum to the canonical 1/4,1/8 layout).
//
// On the FIRST downsample (full-res source) the per-group average is replaced by
// a Karis luma-weighted average (w = 1/(1+luma)) to suppress fireflies — a single
// very bright sub-pixel texel would otherwise bloom into a hard square. Later
// levels use the plain average (the source is already pre-averaged, so the Karis
// step is both unnecessary and slightly energy-losing there).

layout(set = 0, binding = 0) uniform sampler2D texture0;

layout(location = 0) in vec2 frag_tex_coord;

layout(location = 0) out vec4 out_color;

// 1 / source-resolution, baked per level (same re-derive-on-resolution machinery
// as the legacy blur pass's texel offsets).
layout(constant_id = 0) const float texel_x = 0.0;
layout(constant_id = 1) const float texel_y = 0.0;
// 1 only on the full-res -> half downsample (enables the Karis firefly clamp).
layout(constant_id = 2) const int first_level = 0;

float karis_luma( vec3 c )
{
	// Rec.709 luma; the Karis weight operates in the same linear space the bloom
	// chain runs in.
	return dot( c, vec3( 0.2126, 0.7152, 0.0722 ) );
}

// Karis average of four samples: weight each by 1/(1+luma) and renormalise.
vec3 karis_avg( vec3 a, vec3 b, vec3 c, vec3 d )
{
	float wa = 1.0 / ( 1.0 + karis_luma( a ) );
	float wb = 1.0 / ( 1.0 + karis_luma( b ) );
	float wc = 1.0 / ( 1.0 + karis_luma( c ) );
	float wd = 1.0 / ( 1.0 + karis_luma( d ) );
	float ws = wa + wb + wc + wd;
	return ( a * wa + b * wb + c * wc + d * wd ) / max( ws, 1e-5 );
}

void main()
{
	vec2 uv = frag_tex_coord;
	vec2 tx;
	tx.x = texel_x;
	tx.y = texel_y;

	// inner 2x2 box (+/-1 texel)
	vec3 a = texture( texture0, uv + tx * vec2( -1.0, -1.0 ) ).rgb;
	vec3 b = texture( texture0, uv + tx * vec2(  1.0, -1.0 ) ).rgb;
	vec3 c = texture( texture0, uv + tx * vec2( -1.0,  1.0 ) ).rgb;
	vec3 d = texture( texture0, uv + tx * vec2(  1.0,  1.0 ) ).rgb;

	// outer 3x3 ring (+/-2 texels) + centre
	vec3 e = texture( texture0, uv + tx * vec2( -2.0, -2.0 ) ).rgb;
	vec3 f = texture( texture0, uv + tx * vec2(  0.0, -2.0 ) ).rgb;
	vec3 g = texture( texture0, uv + tx * vec2(  2.0, -2.0 ) ).rgb;
	vec3 h = texture( texture0, uv + tx * vec2( -2.0,  0.0 ) ).rgb;
	vec3 i = texture( texture0, uv                          ).rgb;
	vec3 j = texture( texture0, uv + tx * vec2(  2.0,  0.0 ) ).rgb;
	vec3 k = texture( texture0, uv + tx * vec2( -2.0,  2.0 ) ).rgb;
	vec3 l = texture( texture0, uv + tx * vec2(  0.0,  2.0 ) ).rgb;
	vec3 m = texture( texture0, uv + tx * vec2(  2.0,  2.0 ) ).rgb;

	vec3 result;
	if ( first_level != 0 )
	{
		// Karis-weighted groups: the inner box, the four overlapping 2x2 corner
		// groups of the ring (centre shared), then the standard 0.5/0.125/0.125/
		// 0.125/0.125 group blend.
		vec3 group_inner = karis_avg( a, b, c, d );
		vec3 group_tl    = karis_avg( e, f, h, i );
		vec3 group_tr    = karis_avg( f, g, i, j );
		vec3 group_bl    = karis_avg( h, i, k, l );
		vec3 group_br    = karis_avg( i, j, l, m );
		result = group_inner * 0.5
		       + ( group_tl + group_tr + group_bl + group_br ) * 0.125;
	}
	else
	{
		// Plain weighted sum (partition of unity): inner 0.5, ring corners 1/16,
		// ring edges 1/8 (shared between two corner groups), centre 0.125.
		result  = ( a + b + c + d ) * 0.125;        // inner 2x2: 0.5 total
		result += ( e + g + k + m ) * 0.03125;      // ring corners: 0.5/4 each via 1/32
		result += ( f + h + j + l ) * 0.0625;       // ring edges:   1/16 each
		result += i * 0.125;                        // centre
	}

	out_color = vec4( result, 1.0 );
}
