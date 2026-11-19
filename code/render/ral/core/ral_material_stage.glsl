// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
#ifndef WIRED_RAL_MATERIAL_STAGE_GLSL
#define WIRED_RAL_MATERIAL_STAGE_GLSL

// Ordered Q3 material operands and GPU evaluator. Include the immutable
// render_material_noise.h tables first; no nested shader includes.
#if defined(__METAL_VERSION__)
#define WIRED_STAGE_FN static
#define WIRED_STAGE_ARG(type, name) constant type &name
#else
#define WIRED_STAGE_FN
#define WIRED_STAGE_ARG(type, name) type name
#define float2 vec2
#define float3 vec3
#define float4 vec4
#define uint4 uvec4
#endif

WIRED_STAGE_FN float wired_stage_remainder(float x, float y) {
#if defined(__METAL_VERSION__)
    return fmod(x, y);
#else
    // GLSL mod floors; Q3/MSL fmod truncates toward zero.
    return x - trunc(x / y) * y;
#endif
}

WIRED_STAGE_FN float wired_stage_copy_sign(float magnitude, float signSource) {
#if defined(__METAL_VERSION__)
    return copysign(magnitude, signSource);
#else
    return uintBitsToFloat((floatBitsToUint(magnitude) & 0x7fffffffu)
        | (floatBitsToUint(signSource) & 0x80000000u));
#endif
}

WIRED_STAGE_FN float3 wired_stage_srgb_to_linear(float3 value) {
    value = max(value, float3(0.0));
    float3 low = value / 12.92;
    float3 high = pow((value + 0.055) / 1.055, float3(2.4));
#if defined(__METAL_VERSION__)
    return select(high, low, value <= float3(0.04045));
#else
    return mix(high, low, lessThanEqual(value, float3(0.04045)));
#endif
}

struct MaterialTcMod {
	uint type;
	// Scalars are intentional: std140 float arrays have 16-byte stride.
	float p0, p1, p2, p3, p4, p5;
	uint reserved;
};

struct MaterialStageParams {
	uint4 meta;
	uint4 generators;
	float4 params;
	float4 scaleScroll;
	float4 turbulence;
	float4 stretch;
	float4 constantColor;
	float4 rgbWave;
	float4 alphaWave;
	float4 tcGenVectorS;
	float4 tcGenVectorT;
	uint4 tcModMeta;
	MaterialTcMod tcMods[8];
};

WIRED_STAGE_FN float4 wired_material_blend_factor(uint factor, float4 source,
                                           float4 destination) {
    if (factor == 0u) return float4(0.0);
    if (factor == 1u) return float4(1.0);
    if (factor == 2u) return source;
    if (factor == 3u) return 1.0 - source;
    if (factor == 4u) return destination;
    if (factor == 5u) return 1.0 - destination;
    if (factor == 6u) return float4(source.a);
    if (factor == 7u) return float4(1.0 - source.a);
    if (factor == 8u) return float4(destination.a);
    if (factor == 9u) return float4(1.0 - destination.a);
    return float4(min(source.a, 1.0 - destination.a));
}

// Q3 truncates the signed table coordinate BEFORE wrapping, not floor/fract.
// Reduce in floating point first to keep the integer conversion bounded.
WIRED_STAGE_FN uint wired_material_table_index(float phase) {
    return uint(int(wired_stage_remainder(phase, 1.0) * 1024.0)) & 1023u;
}

WIRED_STAGE_FN float wired_material_table_sine(uint index) {
    return sin(float(index & 1023u) * (6.28318530718 / 1024.0) + 0.0001);
}

// Q3 noise at x=y=z=0 is a periodic 1D slice of its 4D value lattice.
WIRED_STAGE_FN float wired_material_noise_sample(uint cell) {
	uint index = cell & 255u;
	for (uint axis = 0u; axis < 4u; ++axis)
		index = wired_material_noise_permutation[index];
	return wired_material_noise_values[index];
}

WIRED_STAGE_FN float wired_material_wave(uint function, float4 wave, float time) {
	if (function == 6u) {
		// Noise phase is inside the frequency multiplication, unlike table waves.
		float position = (time + wave.z) * wave.w;
		float wrapped = wired_stage_remainder(position, 256.0);
		uint cell = uint(int(floor(wrapped))) & 255u;
		float a = wired_material_noise_sample(cell);
		float b = wired_material_noise_sample(cell + 1u);
		return wave.x + wave.y * (a + (b - a) * fract(wrapped));
	}
	float phase = wave.z + time * wave.w;
	uint index = wired_material_table_index(phase);
	float cycle = float(index) / 1024.0;
	float value;
	if (function == 1u) value = wired_material_table_sine(index);
	else if (function == 2u) value = index < 512u ? 1.0 : -1.0;
	else if (function == 3u) {
		uint halfIndex = index & 511u;
		uint quarterIndex = halfIndex & 255u;
		value = quarterIndex == 0u ? 0.000001 : float(quarterIndex) / 256.0;
		if (halfIndex >= 256u) value = 1.0 - value;
		if (index >= 512u) value = -value;
	} else if (function == 4u) value = index == 0u ? 0.000001 : cycle;
	else if (function == 5u) value = 1.0 - (index == 0u ? 0.000001 : cycle);

	else value = 0.0;
	return wave.x + wave.y * value;
}

WIRED_STAGE_FN float4 wired_material_stage_generators(
		float4 source, WIRED_STAGE_ARG(MaterialStageParams, stage), float4 vertexColor, float4 entityColor,
		float3 diffuseLighting, float vertexAlpha, float time) {
	uint rgbGen = stage.generators.x;
	uint alphaGen = stage.generators.y;
	float3 rgb = float3(1.0);
	float alpha = 1.0;
	if (rgbGen == 3u || rgbGen == 4u) {
		rgb = rgbGen == 3u ? entityColor.rgb : 1.0 - entityColor.rgb;
		alpha = rgbGen == 3u ? entityColor.a : 1.0 - entityColor.a;
	} else if (rgbGen == 5u || rgbGen == 6u) {
		rgb = vertexColor.rgb;
		alpha = vertexColor.a;
	} else if (rgbGen == 7u) {
		rgb = 1.0 - vertexColor.rgb;
	} else if (rgbGen == 8u)
		rgb = float3(floor(clamp(wired_material_wave(stage.generators.z,
			stage.rgbWave, time), 0.0, 1.0) * 255.0) / 255.0);
	else if (rgbGen == 9u)
		rgb = diffuseLighting;
	else if (rgbGen == 11u) {
		float4 color = floor(clamp(stage.constantColor, 0.0, 1.0) * 255.0) / 255.0;
		rgb = color.rgb;
		alpha = color.a;
	}
	// Authored Q3 stage colors are display-referred bytes. Decode after
	// interpolation, like Vulkan; physical diffuse irradiance is already linear.
	if (rgbGen != 9u) rgb = wired_stage_srgb_to_linear(rgb);
	// AGEN_SKIP keeps the alpha established by rgbGen. AGEN_IDENTITY preserves
	// CGEN_VERTEX alpha, matching R_ComputeColors rather than forcing opacity.
	if (alphaGen == 0u && rgbGen != 6u)
		alpha = 1.0;
	else if (alphaGen == 2u || alphaGen == 3u)
		alpha = alphaGen == 2u ? entityColor.a : 1.0 - entityColor.a;
	else if (alphaGen == 4u)
		alpha = vertexColor.a;
	else if (alphaGen == 5u)
		alpha = 1.0 - vertexColor.a;
	else if (alphaGen == 7u)
		alpha = floor(clamp(wired_material_wave(stage.generators.w,
			stage.alphaWave, time), 0.0, 1.0) * 255.0) / 255.0;
	else if (alphaGen == 6u || alphaGen == 8u)
		alpha = vertexAlpha;
	else if (alphaGen == 9u)
		alpha = floor(clamp(stage.constantColor.a, 0.0, 1.0) * 255.0) / 255.0;
	source.rgb *= rgb;
	source.a *= alpha;
	return source;
}

// Raw Q3 rt/bk/lf/ft/up/dn atlas; deliberately NOT the GL radiance cube layout.
WIRED_STAGE_FN float2 wired_sky_authored_atlas_uv_inset(float3 direction, float2 inset) {
    float3 ad = abs(direction);
    uint face;
    float2 faceUv;
    if (ad.x >= ad.y && ad.x >= ad.z) {
    	if (direction.x >= 0.0) {
    		face = 0u; faceUv = float2(-direction.y, -direction.z) / ad.x;
    	} else {
    		face = 2u; faceUv = float2(direction.y, -direction.z) / ad.x;
    	}
    } else if (ad.y >= ad.z) {
    	if (direction.y >= 0.0) {
    		face = 1u; faceUv = float2(direction.x, -direction.z) / ad.y;
    	} else {
    		face = 3u; faceUv = float2(-direction.x, -direction.z) / ad.y;
    	}
    } else if (direction.z >= 0.0) {
    	face = 4u; faceUv = float2(-direction.y, direction.x) / ad.z;
    } else {
    	face = 5u; faceUv = float2(-direction.y, -direction.x) / ad.z;
    }
    faceUv = clamp(faceUv * 0.5 + 0.5, inset, float2(1.0) - inset);
    return (faceUv + float2(float(face % 3u), float(face / 3u))) / float2(3.0, 2.0);
}

WIRED_STAGE_FN float2 wired_sky_authored_atlas_uv(float3 direction) {
    return wired_sky_authored_atlas_uv_inset(direction, float2(0.001));
}

// Half a texel in each face, not a fixed fraction of an arbitrary-size atlas.
// The atlas is sampled at LOD0: generated atlas mips can mix unrelated faces.
WIRED_STAGE_FN float2 wired_sky_authored_atlas_uv_sized(float3 direction, float2 atlasSize) {
    return wired_sky_authored_atlas_uv_inset(direction,
        float2(1.5, 1.0) / max(atlasSize, float2(3.0, 2.0)));
}

WIRED_STAGE_FN float2 wired_sky_cloud_uv(float3 direction, float height) {
    const float radiusWorld = 4096.0;
    const float cloudHeight = max(height, 1.0);
    const float rayDistance = -direction.z * radiusWorld
        + sqrt(direction.z * direction.z * radiusWorld * radiusWorld
            + 2.0 * radiusWorld * cloudHeight + cloudHeight * cloudHeight);
    const float3 cloudDirection = normalize(direction * rayDistance
        + float3(0.0, 0.0, radiusWorld));
    return acos(clamp(cloudDirection.xy, -1.0, 1.0));
}

WIRED_STAGE_FN float2 wired_material_stage_uv(float2 authoredUv,
                                       WIRED_STAGE_ARG(MaterialStageParams, stage),
                                       float time, float3 position) {
    if (stage.tcModMeta.x > 0u) {
        float2 uv = authoredUv;
        for (uint i = 0u; i < min(stage.tcModMeta.x, 8u); ++i) {
            MaterialTcMod op = stage.tcMods[i];
            if (op.type == 1u) uv *= float2(op.p0, op.p1);
            else if (op.type == 2u) uv += fract(float2(op.p0, op.p1) * time);
            else if (op.type == 3u) {
                uint index = wired_material_table_index(-op.p0 * time / 360.0);
                float sine = wired_material_table_sine(index);
                float cosine = wired_material_table_sine(index + 256u);
                float2 centered = uv - 0.5;
                uv = float2(centered.x * cosine - centered.y * sine,
                    centered.x * sine + centered.y * cosine) + 0.5;
            } else if (op.type == 4u) {
                // Q3 turb ignores base: each coordinate has its own spatial phase.
                float phase = op.p2 + time * op.p3;
                float2 coordinate = float2(position.x + position.z, position.y)
                    / 1024.0 + phase;
                uv += float2(wired_material_table_sine(wired_material_table_index(coordinate.x)),
                    wired_material_table_sine(wired_material_table_index(coordinate.y))) * op.p1;
            } else if (op.type == 5u) {
                float wave = wired_material_wave(uint(op.p0),
                    float4(op.p1, op.p2, op.p3, op.p4), time);
                // Preserve negative scale; only bound the singular zero case.
                wave = wired_stage_copy_sign(max(abs(wave), 0.0001), wave);
                uv = (uv - 0.5) / wave + 0.5;
            } else if (op.type == 6u) {
                uv = float2(uv.x * op.p0 + uv.y * op.p2 + op.p4,
                    uv.x * op.p1 + uv.y * op.p3 + op.p5);
            }
        }
        return uv;
    }
    float2 uv = authoredUv * stage.scaleScroll.xy
        + stage.scaleScroll.zw * time;
    if (stage.params.w > 0.5) {
        float wave = stage.stretch.x + stage.stretch.y
            * sin((stage.stretch.z + time * stage.stretch.w) * 6.28318530718);
        uv = (uv - 0.5) / max(abs(wave), 0.0001) + 0.5;
    }
    if (abs(stage.params.y) > 0.0001) {
        float angle = stage.params.y * time * 0.01745329252;
        float sine = sin(angle), cosine = cos(angle);
        float2 centered = uv - 0.5;
        uv = float2(centered.x * cosine - centered.y * sine,
            centered.x * sine + centered.y * cosine) + 0.5;
    }
    if (stage.params.z > 0.5) {
        float phase = stage.turbulence.z + time * stage.turbulence.w;
        float offset = stage.turbulence.x + stage.turbulence.y
            * sin((uv.x + uv.y + phase) * 6.28318530718);
        uv += offset;
    }
    return uv;
}


#if !defined(__METAL_VERSION__)
#undef float2
#undef float3
#undef float4
#undef uint4
#endif
#undef WIRED_STAGE_FN
#undef WIRED_STAGE_ARG
#endif
