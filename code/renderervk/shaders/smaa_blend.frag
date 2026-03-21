#version 450
// SMAA Pass 2: Blending Weight Calculation — Fragment Shader
// Ported from the canonical SMAA by Jorge Jimenez et al. (MIT license)
// This is the core SMAA algorithm: pattern detection + area/search texture lookups.

layout(set = 0, binding = 0) uniform sampler2D edgesTex;
layout(set = 1, binding = 0) uniform sampler2D areaTex;
layout(set = 2, binding = 0) uniform sampler2D searchTex;

layout(push_constant) uniform PushConstants {
	vec4 rtMetrics; // { 1/w, 1/h, w, h }
};

layout(constant_id = 0) const int SMAA_MAX_SEARCH_STEPS = 16;
layout(constant_id = 1) const int SMAA_MAX_SEARCH_STEPS_DIAG = 8;
layout(constant_id = 2) const int SMAA_CORNER_ROUNDING = 25;

layout(location = 0) in vec2 texcoord;
layout(location = 1) in vec2 pixcoord;
layout(location = 2) in vec4 offset[3];

layout(location = 0) out vec4 out_weights;

// SMAA constants
#define SMAA_AREATEX_MAX_DISTANCE     16
#define SMAA_AREATEX_MAX_DISTANCE_DIAG 20
#define SMAA_AREATEX_PIXEL_SIZE       vec2(1.0 / 160.0, 1.0 / 560.0)
#define SMAA_AREATEX_SUBTEX_SIZE      (1.0 / 7.0)
#define SMAA_SEARCHTEX_SIZE           vec2(66.0, 33.0)
#define SMAA_SEARCHTEX_PACKED_SIZE    vec2(64.0, 16.0)
#define SMAA_CORNER_ROUNDING_NORM     (float(SMAA_CORNER_ROUNDING) / 100.0)

// Helper: conditional move
void SMAAMovc(bvec2 cond, inout vec2 variable, vec2 value) {
	if (cond.x) variable.x = value.x;
	if (cond.y) variable.y = value.y;
}

void SMAAMovc(bvec4 cond, inout vec4 variable, vec4 value) {
	SMAAMovc(cond.xy, variable.xy, value.xy);
	SMAAMovc(cond.zw, variable.zw, value.zw);
}

//-----------------------------------------------------------------------------
// Diagonal Search Functions

vec2 SMAADecodeDiagBilinearAccess(vec2 e) {
	e.r = e.r * abs(5.0 * e.r - 5.0 * 0.75);
	return round(e);
}

vec4 SMAADecodeDiagBilinearAccess(vec4 e) {
	e.rb = e.rb * abs(5.0 * e.rb - 5.0 * 0.75);
	return round(e);
}

vec2 SMAASearchDiag1(vec2 tc, vec2 dir, out vec2 e) {
	vec4 coord = vec4(tc, -1.0, 1.0);
	vec3 t = vec3(rtMetrics.xy, 1.0);
	while (coord.z < float(SMAA_MAX_SEARCH_STEPS_DIAG - 1) && coord.w > 0.9) {
		coord.xyz = t * vec3(dir, 1.0) + coord.xyz;
		e = textureLod(edgesTex, coord.xy, 0.0).rg;
		coord.w = dot(e, vec2(0.5, 0.5));
	}
	return coord.zw;
}

vec2 SMAASearchDiag2(vec2 tc, vec2 dir, out vec2 e) {
	vec4 coord = vec4(tc, -1.0, 1.0);
	coord.x += 0.25 * rtMetrics.x;
	vec3 t = vec3(rtMetrics.xy, 1.0);
	while (coord.z < float(SMAA_MAX_SEARCH_STEPS_DIAG - 1) && coord.w > 0.9) {
		coord.xyz = t * vec3(dir, 1.0) + coord.xyz;
		e = textureLod(edgesTex, coord.xy, 0.0).rg;
		e = SMAADecodeDiagBilinearAccess(e);
		coord.w = dot(e, vec2(0.5, 0.5));
	}
	return coord.zw;
}

vec2 SMAAAreaDiag(vec2 dist, vec2 e, float offs) {
	vec2 tc = vec2(SMAA_AREATEX_MAX_DISTANCE_DIAG) * e + dist;
	tc = SMAA_AREATEX_PIXEL_SIZE * tc + 0.5 * SMAA_AREATEX_PIXEL_SIZE;
	tc.x += 0.5;
	tc.y += SMAA_AREATEX_SUBTEX_SIZE * offs;
	return textureLod(areaTex, tc, 0.0).rg;
}

vec2 SMAACalculateDiagWeights(vec2 tc, vec2 e) {
	vec2 weights = vec2(0.0);
	vec4 d;
	vec2 end;

	if (e.r > 0.0) {
		d.xz = SMAASearchDiag1(tc, vec2(-1.0, 1.0), end);
		d.x += float(end.y > 0.9);
	} else {
		d.xz = vec2(0.0);
	}
	d.yw = SMAASearchDiag1(tc, vec2(1.0, -1.0), end);

	if (d.x + d.y > 2.0) {
		vec4 coords = vec4(-d.x + 0.25, d.x, d.y, -d.y - 0.25) * rtMetrics.xyxy + tc.xyxy;
		vec4 c;
		c.xy = textureLodOffset(edgesTex, coords.xy, 0.0, ivec2(-1, 0)).rg;
		c.zw = textureLodOffset(edgesTex, coords.zw, 0.0, ivec2( 1, 0)).rg;
		c.yxwz = SMAADecodeDiagBilinearAccess(c.xyzw);
		vec2 cc = 2.0 * c.xz + c.yw;
		SMAAMovc(bvec2(step(0.9, d.zw)), cc, vec2(0.0));
		weights += SMAAAreaDiag(d.xy, cc, 0.0);
	}

	d.xz = SMAASearchDiag2(tc, vec2(-1.0, -1.0), end);
	if (textureLodOffset(edgesTex, tc, 0.0, ivec2(1, 0)).r > 0.0) {
		d.yw = SMAASearchDiag2(tc, vec2(1.0, 1.0), end);
		d.y += float(end.y > 0.9);
	} else {
		d.yw = vec2(0.0);
	}

	if (d.x + d.y > 2.0) {
		vec4 coords = vec4(-d.x, -d.x, d.y, d.y) * rtMetrics.xyxy + tc.xyxy;
		vec4 c;
		c.x  = textureLodOffset(edgesTex, coords.xy, 0.0, ivec2(-1,  0)).g;
		c.y  = textureLodOffset(edgesTex, coords.xy, 0.0, ivec2( 0, -1)).r;
		c.zw = textureLodOffset(edgesTex, coords.zw, 0.0, ivec2( 1,  0)).gr;
		vec2 cc = 2.0 * c.xz + c.yw;
		SMAAMovc(bvec2(step(0.9, d.zw)), cc, vec2(0.0));
		weights += SMAAAreaDiag(d.xy, cc, 0.0).gr;
	}

	return weights;
}

//-----------------------------------------------------------------------------
// Horizontal/Vertical Search Functions

float SMAASearchLength(vec2 e, float offs) {
	vec2 scale = SMAA_SEARCHTEX_SIZE * vec2(0.5, -1.0);
	vec2 bias  = SMAA_SEARCHTEX_SIZE * vec2(offs, 1.0);
	scale += vec2(-1.0,  1.0);
	bias  += vec2( 0.5, -0.5);
	scale *= 1.0 / SMAA_SEARCHTEX_PACKED_SIZE;
	bias  *= 1.0 / SMAA_SEARCHTEX_PACKED_SIZE;
	return textureLod(searchTex, scale * e + bias, 0.0).r;
}

float SMAASearchXLeft(vec2 tc, float end) {
	vec2 e = vec2(0.0, 1.0);
	while (tc.x > end && e.g > 0.8281 && e.r == 0.0) {
		e = textureLod(edgesTex, tc, 0.0).rg;
		tc += vec2(-2.0, 0.0) * rtMetrics.xy;
	}
	float offs = -(255.0 / 127.0) * SMAASearchLength(e, 0.0) + 3.25;
	return rtMetrics.x * offs + tc.x;
}

float SMAASearchXRight(vec2 tc, float end) {
	vec2 e = vec2(0.0, 1.0);
	while (tc.x < end && e.g > 0.8281 && e.r == 0.0) {
		e = textureLod(edgesTex, tc, 0.0).rg;
		tc += vec2(2.0, 0.0) * rtMetrics.xy;
	}
	float offs = -(255.0 / 127.0) * SMAASearchLength(e, 0.5) + 3.25;
	return -rtMetrics.x * offs + tc.x;
}

float SMAASearchYUp(vec2 tc, float end) {
	vec2 e = vec2(1.0, 0.0);
	while (tc.y > end && e.r > 0.8281 && e.g == 0.0) {
		e = textureLod(edgesTex, tc, 0.0).rg;
		tc += vec2(0.0, -2.0) * rtMetrics.xy;
	}
	float offs = -(255.0 / 127.0) * SMAASearchLength(e.gr, 0.0) + 3.25;
	return rtMetrics.y * offs + tc.y;
}

float SMAASearchYDown(vec2 tc, float end) {
	vec2 e = vec2(1.0, 0.0);
	while (tc.y < end && e.r > 0.8281 && e.g == 0.0) {
		e = textureLod(edgesTex, tc, 0.0).rg;
		tc += vec2(0.0, 2.0) * rtMetrics.xy;
	}
	float offs = -(255.0 / 127.0) * SMAASearchLength(e.gr, 0.5) + 3.25;
	return -rtMetrics.y * offs + tc.y;
}

//-----------------------------------------------------------------------------
// Area Lookup

vec2 SMAAArea(vec2 dist, float e1, float e2, float offs) {
	vec2 tc = vec2(SMAA_AREATEX_MAX_DISTANCE) * round(4.0 * vec2(e1, e2)) + dist;
	tc = SMAA_AREATEX_PIXEL_SIZE * tc + 0.5 * SMAA_AREATEX_PIXEL_SIZE;
	tc.y = SMAA_AREATEX_SUBTEX_SIZE * offs + tc.y;
	return textureLod(areaTex, tc, 0.0).rg;
}

//-----------------------------------------------------------------------------
// Corner Detection

void SMAADetectHorizontalCornerPattern(inout vec2 weights, vec4 tc, vec2 d) {
	vec2 leftRight = step(d.xy, d.yx);
	vec2 rounding = (1.0 - SMAA_CORNER_ROUNDING_NORM) * leftRight;
	rounding /= leftRight.x + leftRight.y;
	vec2 factor = vec2(1.0);
	factor.x -= rounding.x * textureLodOffset(edgesTex, tc.xy, 0.0, ivec2(0,  1)).r;
	factor.x -= rounding.y * textureLodOffset(edgesTex, tc.zw, 0.0, ivec2(1,  1)).r;
	factor.y -= rounding.x * textureLodOffset(edgesTex, tc.xy, 0.0, ivec2(0, -2)).r;
	factor.y -= rounding.y * textureLodOffset(edgesTex, tc.zw, 0.0, ivec2(1, -2)).r;
	weights *= clamp(factor, 0.0, 1.0);
}

void SMAADetectVerticalCornerPattern(inout vec2 weights, vec4 tc, vec2 d) {
	vec2 leftRight = step(d.xy, d.yx);
	vec2 rounding = (1.0 - SMAA_CORNER_ROUNDING_NORM) * leftRight;
	rounding /= leftRight.x + leftRight.y;
	vec2 factor = vec2(1.0);
	factor.x -= rounding.x * textureLodOffset(edgesTex, tc.xy, 0.0, ivec2( 1, 0)).g;
	factor.x -= rounding.y * textureLodOffset(edgesTex, tc.zw, 0.0, ivec2( 1, 1)).g;
	factor.y -= rounding.x * textureLodOffset(edgesTex, tc.xy, 0.0, ivec2(-2, 0)).g;
	factor.y -= rounding.y * textureLodOffset(edgesTex, tc.zw, 0.0, ivec2(-2, 1)).g;
	weights *= clamp(factor, 0.0, 1.0);
}

//-----------------------------------------------------------------------------
// Main: Blending Weight Calculation

void main() {
	vec4 weights = vec4(0.0);
	vec2 e = texture(edgesTex, texcoord).rg;

	if (e.g > 0.0) { // Edge at north
		bool skipHorizontal = false;

		// Diagonal detection (only for presets with SMAA_MAX_SEARCH_STEPS_DIAG > 0)
		if (SMAA_MAX_SEARCH_STEPS_DIAG > 0) {
			weights.rg = SMAACalculateDiagWeights(texcoord, e);

			// If diagonal weights were found, skip horizontal/vertical
			skipHorizontal = (weights.r + weights.g != 0.0);

			if (skipHorizontal) {
				e.r = 0.0; // Skip vertical processing below
			}
		}

		if (!skipHorizontal) {
			vec2 d;
			vec3 coords;

			// Find the distance to the left
			coords.x = SMAASearchXLeft(offset[0].xy, offset[2].x);
			coords.y = offset[1].y;
			d.x = coords.x;

			// Fetch the left crossing edges
			float e1 = textureLod(edgesTex, coords.xy, 0.0).r;

			// Find the distance to the right
			coords.z = SMAASearchXRight(offset[0].zw, offset[2].y);
			d.y = coords.z;

			// Convert to pixel units
			d = abs(round(rtMetrics.zz * d - pixcoord.xx));

			// Area texture needs sqrt (quadratic compression)
			vec2 sqrt_d = sqrt(d);

			// Fetch the right crossing edges
			float e2 = textureLodOffset(edgesTex, coords.zy, 0.0, ivec2(1, 0)).r;

			// Get the area
			weights.rg = SMAAArea(sqrt_d, e1, e2, 0.0);

			// Fix corners
			coords.y = texcoord.y;
			SMAADetectHorizontalCornerPattern(weights.rg, coords.xyzy, d);
		}
	}

	if (e.r > 0.0) { // Edge at west
		vec2 d;
		vec3 coords;

		// Find the distance to the top
		coords.y = SMAASearchYUp(offset[1].xy, offset[2].z);
		coords.x = offset[0].x;
		d.x = coords.y;

		// Fetch the top crossing edges
		float e1 = textureLod(edgesTex, coords.xy, 0.0).g;

		// Find the distance to the bottom
		coords.z = SMAASearchYDown(offset[1].zw, offset[2].w);
		d.y = coords.z;

		// Convert to pixel units
		d = abs(round(rtMetrics.ww * d - pixcoord.yy));

		vec2 sqrt_d = sqrt(d);

		// Fetch the bottom crossing edges
		float e2 = textureLodOffset(edgesTex, coords.xz, 0.0, ivec2(0, 1)).g;

		// Get the area
		weights.ba = SMAAArea(sqrt_d, e1, e2, 0.0);

		// Fix corners
		coords.x = texcoord.x;
		SMAADetectVerticalCornerPattern(weights.ba, coords.xyxz, d);
	}

	out_weights = weights;
}
