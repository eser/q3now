enable wgpu_binding_array;

struct UBO {
    eyePos: vec4<f32>,
    lightPos: vec4<f32>,
    lightColor: vec4<f32>,
    lightVector: vec4<f32>,
    fogDistanceVector: vec4<f32>,
    fogDepthVector: vec4<f32>,
    fogEyeT: vec4<f32>,
    fogColor: vec4<f32>,
    _pad_to_packed_indices: array<vec4<f32>, 26>,
    packed_indices: array<vec4<u32>, 3>,
    worldLightParams: vec4<f32>,
    advancedFogColorDensity: vec4<f32>,
    advancedFogTypeFarEnabled: vec4<f32>,
    emissionRadiance: vec4<f32>,
}

@id(4) override tex_domain: i32 = 0i;
@id(26) override lightmap_slot: i32 = 0i;
@id(6) override tex_mode: i32 = 0i;
override override_type_3_: bool = (tex_mode == 1i);
override override_type_3_1: bool = (tex_mode == 2i);
override override_type_3_2: bool = (lightmap_slot != 0i);
@id(7) override discard_mode: i32 = 0i;
override override_type_3_3: bool = (discard_mode == 1i);
override override_type_3_4: bool = (discard_mode == 2i);
@id(3) override alpha_to_coverage: i32 = 0i;

@group(0) @binding(0)
var<uniform> unnamed: UBO;
var<private> gl_FragCoord_1: vec4<f32>;
@group(1) @binding(0)
var wired_bindless_images: binding_array<texture_2d<f32>>;
@group(1) @binding(1)
var wired_bindless_samplers: binding_array<sampler>;
var<private> frag_tex_coord0_1: vec2<f32>;
var<private> frag_tex_coord1_1: vec2<f32>;
var<private> out_color: vec4<f32>;

fn wired_advanced_fog_enabled_u0028_() -> bool {
    var fogType: i32;

    let _e59 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e59 + 0.5f));
    let _e64 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e66 = fogType;
    let _e69 = fogType;
    return (((_e64 > 0.5f) && (_e66 >= 1i)) && (_e69 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e59 = wired_advanced_fog_enabled_u0028_();
    if !(_e59) {
        return 0f;
    }
    let _e62 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e62, 0.000001f));
    let _e67 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e67 + 0.5f));
    let _e70 = fogType_1;
    if (_e70 == 1i) {
        let _e74 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e74 <= 0f) {
            return 0f;
        }
        let _e76 = viewDepth;
        let _e79 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e76 / _e79), 0f, 1f);
    }
    let _e84 = unnamed.advancedFogColorDensity[3u];
    let _e86 = viewDepth;
    opticalDepth = (max(_e84, 0f) * _e86);
    let _e88 = fogType_1;
    if (_e88 == 2i) {
        let _e90 = opticalDepth;
        return clamp((1f - exp(-(_e90))), 0f, 1f);
    }
    let _e95 = opticalDepth;
    let _e96 = opticalDepth;
    return clamp((1f - exp(-((_e95 * _e96)))), 0f, 1f);
}

fn wired_boost_legacy_lightmap_u0028_vf3_u003b(rgb: ptr<function, vec3<f32>>) -> vec3<f32> {
    var boosted: vec3<f32>;
    var peak: f32;

    let _e59 = (*rgb);
    let _e62 = unnamed.worldLightParams[0u];
    boosted = (_e59 * _e62);
    let _e65 = boosted[0u];
    let _e67 = boosted[1u];
    let _e69 = boosted[2u];
    peak = max(_e65, max(_e67, _e69));
    let _e72 = peak;
    if (_e72 > 1f) {
        let _e74 = peak;
        let _e75 = boosted;
        boosted = (_e75 / vec3(_e74));
    }
    let _e78 = boosted;
    return _e78;
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e60 = (*c);
    (*c) = max(_e60, vec3<f32>(0f, 0f, 0f));
    let _e62 = (*c);
    cutoff = (_e62 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e64 = (*c);
    lo = (_e64 / vec3(12.92f));
    let _e67 = (*c);
    hi = pow(((_e67 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e72 = hi;
    let _e73 = lo;
    let _e74 = cutoff;
    return mix(_e72, _e73, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e74));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param: vec3<f32>;
    var param_1: vec3<f32>;

    let _e62 = (*role);
    let _e64 = (*role);
    let _e69 = unnamed.packed_indices[(_e62 / 4u)][(_e64 % 4u)];
    let _e72 = (*role);
    let _e74 = (*role);
    let _e79 = unnamed.packed_indices[(_e72 / 4u)][(_e74 % 4u)];
    let _e84 = (*uv);
    let _e85 = textureSample(wired_bindless_images[(_e69 & 4095u)], wired_bindless_samplers[((_e79 >> bitcast<u32>(12i)) & 255u)], _e84);
    c_1 = _e85;
    let _e86 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e86))) == 0i) {
        let _e91 = c_1;
        param = _e91.xyz;
        let _e93 = sRGBToLinear_u0028_vf3_u003b((&param));
        c_1[0u] = _e93.x;
        c_1[1u] = _e93.y;
        c_1[2u] = _e93.z;
    }
    let _e100 = (*slot);
    if (lightmap_slot == (_e100 + 1i)) {
        let _e103 = c_1;
        param_1 = _e103.xyz;
        let _e105 = wired_boost_legacy_lightmap_u0028_vf3_u003b((&param_1));
        c_1[0u] = _e105.x;
        c_1[1u] = _e105.y;
        c_1[2u] = _e105.z;
    }
    let _e112 = c_1;
    return _e112;
}

fn main_1() {
    var color0_: vec4<f32>;
    var param_2: u32;
    var param_3: vec2<f32>;
    var param_4: i32;
    var color1_: vec4<f32>;
    var param_5: u32;
    var param_6: vec2<f32>;
    var param_7: i32;
    var base: vec4<f32>;
    var color1_1: vec4<f32>;
    var param_8: u32;
    var param_9: vec2<f32>;
    var param_10: i32;
    var color1_2: vec4<f32>;
    var param_11: u32;
    var param_12: vec2<f32>;
    var param_13: i32;
    var wetness: f32;
    var frost: f32;
    var luminance: f32;
    var fogAmount: f32;

    param_2 = 0u;
    let _e77 = frag_tex_coord0_1;
    param_3 = _e77;
    param_4 = 0i;
    let _e78 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_2), (&param_3), (&param_4));
    color0_ = _e78;
    if override_type_3_ {
        param_5 = 1u;
        let _e79 = frag_tex_coord1_1;
        param_6 = _e79;
        param_7 = 1i;
        let _e80 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_5), (&param_6), (&param_7));
        color1_ = _e80;
        let _e81 = color0_;
        let _e83 = color1_;
        let _e85 = (_e81.xyz + _e83.xyz);
        let _e87 = color0_[3u];
        let _e89 = color1_[3u];
        base = vec4<f32>(_e85.x, _e85.y, _e85.z, (_e87 * _e89));
    } else {
        if override_type_3_1 {
            param_8 = 1u;
            let _e95 = frag_tex_coord1_1;
            param_9 = _e95;
            param_10 = 1i;
            let _e96 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_8), (&param_9), (&param_10));
            color1_1 = _e96;
            let _e97 = color0_;
            let _e99 = color1_1;
            let _e101 = (_e97.xyz + _e99.xyz);
            let _e103 = color0_[3u];
            let _e105 = color1_1[3u];
            base = vec4<f32>(_e101.x, _e101.y, _e101.z, (_e103 * _e105));
        } else {
            param_11 = 1u;
            let _e111 = frag_tex_coord1_1;
            param_12 = _e111;
            param_13 = 1i;
            let _e112 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_11), (&param_12), (&param_13));
            color1_2 = _e112;
            let _e113 = color0_;
            let _e115 = color1_2;
            let _e117 = (_e113.xyz * _e115.xyz);
            base[0u] = _e117.x;
            base[1u] = _e117.y;
            base[2u] = _e117.z;
            let _e125 = color0_[3u];
            let _e127 = color1_2[3u];
            base[3u] = (_e125 * _e127);
        }
    }
    if override_type_3_2 {
        let _e132 = unnamed.worldLightParams[1u];
        wetness = clamp(_e132, 0f, 1f);
        let _e136 = unnamed.worldLightParams[2u];
        frost = clamp(_e136, 0f, 1f);
        let _e138 = base;
        luminance = dot(_e138.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e141 = wetness;
        let _e143 = base;
        let _e145 = (_e143.xyz * mix(1f, 0.82f, _e141));
        base[0u] = _e145.x;
        base[1u] = _e145.y;
        base[2u] = _e145.z;
        let _e152 = base;
        let _e154 = luminance;
        let _e156 = luminance;
        let _e158 = luminance;
        let _e160 = frost;
        let _e163 = mix(_e152.xyz, vec3<f32>((_e154 * 0.88f), (_e156 * 0.94f), _e158), vec3((_e160 * 0.55f)));
        base[0u] = _e163.x;
        base[1u] = _e163.y;
        base[2u] = _e163.z;
    }
    let _e170 = color0_;
    let _e173 = unnamed.emissionRadiance;
    let _e176 = base;
    let _e178 = (_e176.xyz + (_e170.xyz * _e173.xyz));
    base[0u] = _e178.x;
    base[1u] = _e178.y;
    base[2u] = _e178.z;
    let _e185 = wired_advanced_fog_enabled_u0028_();
    if _e185 {
        let _e186 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e186;
        let _e187 = base;
        let _e190 = unnamed.advancedFogColorDensity;
        let _e192 = fogAmount;
        let _e194 = mix(_e187.xyz, _e190.xyz, vec3(_e192));
        base[0u] = _e194.x;
        base[1u] = _e194.y;
        base[2u] = _e194.z;
    }
    if override_type_3_3 {
        let _e202 = base[3u];
        if (_e202 == 0f) {
            discard;
        }
    } else {
        if override_type_3_4 {
            let _e204 = base;
            let _e206 = base;
            if (dot(_e204.xyz, _e206.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e210 = base;
    out_color = _e210;
    return;
}

@fragment
fn main(@builtin(position) gl_FragCoord: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>) -> @location(0) vec4<f32> {
    gl_FragCoord_1 = gl_FragCoord;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    main_1();
    let _e7 = out_color;
    return _e7;
}
