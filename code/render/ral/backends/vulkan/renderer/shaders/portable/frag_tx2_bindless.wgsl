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
var<private> frag_color0In_1: vec4<f32>;
var<private> frag_tex_coord0_1: vec2<f32>;
var<private> frag_tex_coord1_1: vec2<f32>;
var<private> frag_tex_coord2_1: vec2<f32>;
var<private> out_color: vec4<f32>;

fn wired_advanced_fog_enabled_u0028_() -> bool {
    var fogType: i32;

    let _e61 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e61 + 0.5f));
    let _e66 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e68 = fogType;
    let _e71 = fogType;
    return (((_e66 > 0.5f) && (_e68 >= 1i)) && (_e71 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e61 = wired_advanced_fog_enabled_u0028_();
    if !(_e61) {
        return 0f;
    }
    let _e64 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e64, 0.000001f));
    let _e69 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e69 + 0.5f));
    let _e72 = fogType_1;
    if (_e72 == 1i) {
        let _e76 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e76 <= 0f) {
            return 0f;
        }
        let _e78 = viewDepth;
        let _e81 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e78 / _e81), 0f, 1f);
    }
    let _e86 = unnamed.advancedFogColorDensity[3u];
    let _e88 = viewDepth;
    opticalDepth = (max(_e86, 0f) * _e88);
    let _e90 = fogType_1;
    if (_e90 == 2i) {
        let _e92 = opticalDepth;
        return clamp((1f - exp(-(_e92))), 0f, 1f);
    }
    let _e97 = opticalDepth;
    let _e98 = opticalDepth;
    return clamp((1f - exp(-((_e97 * _e98)))), 0f, 1f);
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e62 = (*c);
    (*c) = max(_e62, vec3<f32>(0f, 0f, 0f));
    let _e64 = (*c);
    cutoff = (_e64 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e66 = (*c);
    lo = (_e66 / vec3(12.92f));
    let _e69 = (*c);
    hi = pow(((_e69 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e74 = hi;
    let _e75 = lo;
    let _e76 = cutoff;
    return mix(_e74, _e75, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e76));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param: vec3<f32>;

    let _e63 = (*role);
    let _e65 = (*role);
    let _e70 = unnamed.packed_indices[(_e63 / 4u)][(_e65 % 4u)];
    let _e73 = (*role);
    let _e75 = (*role);
    let _e80 = unnamed.packed_indices[(_e73 / 4u)][(_e75 % 4u)];
    let _e85 = (*uv);
    let _e86 = textureSample(wired_bindless_images[(_e70 & 4095u)], wired_bindless_samplers[((_e80 >> bitcast<u32>(12i)) & 255u)], _e85);
    c_1 = _e86;
    let _e87 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e87))) == 0i) {
        let _e92 = c_1;
        param = _e92.xyz;
        let _e94 = sRGBToLinear_u0028_vf3_u003b((&param));
        c_1[0u] = _e94.x;
        c_1[1u] = _e94.y;
        c_1[2u] = _e94.z;
    }
    let _e101 = (*slot);
    if (lightmap_slot == (_e101 + 1i)) {
        let _e106 = unnamed.worldLightParams[0u];
        let _e107 = c_1;
        let _e109 = (_e107.xyz * _e106);
        c_1[0u] = _e109.x;
        c_1[1u] = _e109.y;
        c_1[2u] = _e109.z;
    }
    let _e116 = c_1;
    return _e116;
}

fn main_1() {
    var frag_color0_: vec4<f32>;
    var param_1: vec3<f32>;
    var color0_: vec4<f32>;
    var param_2: u32;
    var param_3: vec2<f32>;
    var param_4: i32;
    var color1_: vec4<f32>;
    var param_5: u32;
    var param_6: vec2<f32>;
    var param_7: i32;
    var color2_: vec4<f32>;
    var param_8: u32;
    var param_9: vec2<f32>;
    var param_10: i32;
    var base: vec4<f32>;
    var color1_1: vec4<f32>;
    var param_11: u32;
    var param_12: vec2<f32>;
    var param_13: i32;
    var color2_1: vec4<f32>;
    var param_14: u32;
    var param_15: vec2<f32>;
    var param_16: i32;
    var color1_2: vec4<f32>;
    var param_17: u32;
    var param_18: vec2<f32>;
    var param_19: i32;
    var color2_2: vec4<f32>;
    var param_20: u32;
    var param_21: vec2<f32>;
    var param_22: i32;
    var wetness: f32;
    var frost: f32;
    var luminance: f32;
    var fogAmount: f32;

    let _e93 = frag_color0In_1;
    param_1 = _e93.xyz;
    let _e95 = sRGBToLinear_u0028_vf3_u003b((&param_1));
    let _e97 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e95.x, _e95.y, _e95.z, _e97);
    param_2 = 0u;
    let _e102 = frag_tex_coord0_1;
    param_3 = _e102;
    param_4 = 0i;
    let _e103 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_2), (&param_3), (&param_4));
    let _e104 = frag_color0_;
    color0_ = (_e103 * _e104);
    if override_type_3_ {
        param_5 = 1u;
        let _e106 = frag_tex_coord1_1;
        param_6 = _e106;
        param_7 = 1i;
        let _e107 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_5), (&param_6), (&param_7));
        color1_ = _e107;
        param_8 = 2u;
        let _e108 = frag_tex_coord2_1;
        param_9 = _e108;
        param_10 = 2i;
        let _e109 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_8), (&param_9), (&param_10));
        color2_ = _e109;
        let _e110 = color0_;
        let _e112 = color1_;
        let _e115 = color2_;
        let _e117 = ((_e110.xyz + _e112.xyz) + _e115.xyz);
        let _e119 = color0_[3u];
        let _e121 = color1_[3u];
        let _e124 = color2_[3u];
        base = vec4<f32>(_e117.x, _e117.y, _e117.z, ((_e119 * _e121) * _e124));
    } else {
        if override_type_3_1 {
            param_11 = 1u;
            let _e130 = frag_tex_coord1_1;
            param_12 = _e130;
            param_13 = 1i;
            let _e131 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_11), (&param_12), (&param_13));
            let _e132 = frag_color0_;
            color1_1 = (_e131 * _e132);
            param_14 = 2u;
            let _e134 = frag_tex_coord2_1;
            param_15 = _e134;
            param_16 = 2i;
            let _e135 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_14), (&param_15), (&param_16));
            let _e136 = frag_color0_;
            color2_1 = (_e135 * _e136);
            let _e138 = color0_;
            let _e140 = color1_1;
            let _e143 = color2_1;
            let _e145 = ((_e138.xyz + _e140.xyz) + _e143.xyz);
            let _e147 = color0_[3u];
            let _e149 = color1_1[3u];
            let _e152 = color2_1[3u];
            base = vec4<f32>(_e145.x, _e145.y, _e145.z, ((_e147 * _e149) * _e152));
        } else {
            param_17 = 1u;
            let _e158 = frag_tex_coord1_1;
            param_18 = _e158;
            param_19 = 1i;
            let _e159 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_17), (&param_18), (&param_19));
            color1_2 = _e159;
            param_20 = 2u;
            let _e160 = frag_tex_coord2_1;
            param_21 = _e160;
            param_22 = 2i;
            let _e161 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_20), (&param_21), (&param_22));
            color2_2 = _e161;
            let _e162 = color0_;
            let _e164 = color1_2;
            let _e167 = color2_2;
            let _e169 = ((_e162.xyz * _e164.xyz) * _e167.xyz);
            base[0u] = _e169.x;
            base[1u] = _e169.y;
            base[2u] = _e169.z;
            let _e177 = color0_[3u];
            let _e179 = color1_2[3u];
            let _e182 = color2_2[3u];
            base[3u] = ((_e177 * _e179) * _e182);
        }
    }
    if override_type_3_2 {
        let _e187 = unnamed.worldLightParams[1u];
        wetness = clamp(_e187, 0f, 1f);
        let _e191 = unnamed.worldLightParams[2u];
        frost = clamp(_e191, 0f, 1f);
        let _e193 = base;
        luminance = dot(_e193.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e196 = wetness;
        let _e198 = base;
        let _e200 = (_e198.xyz * mix(1f, 0.82f, _e196));
        base[0u] = _e200.x;
        base[1u] = _e200.y;
        base[2u] = _e200.z;
        let _e207 = base;
        let _e209 = luminance;
        let _e211 = luminance;
        let _e213 = luminance;
        let _e215 = frost;
        let _e218 = mix(_e207.xyz, vec3<f32>((_e209 * 0.88f), (_e211 * 0.94f), _e213), vec3((_e215 * 0.55f)));
        base[0u] = _e218.x;
        base[1u] = _e218.y;
        base[2u] = _e218.z;
    }
    let _e225 = color0_;
    let _e228 = unnamed.emissionRadiance;
    let _e231 = base;
    let _e233 = (_e231.xyz + (_e225.xyz * _e228.xyz));
    base[0u] = _e233.x;
    base[1u] = _e233.y;
    base[2u] = _e233.z;
    let _e240 = wired_advanced_fog_enabled_u0028_();
    if _e240 {
        let _e241 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e241;
        let _e242 = base;
        let _e245 = unnamed.advancedFogColorDensity;
        let _e247 = fogAmount;
        let _e249 = mix(_e242.xyz, _e245.xyz, vec3(_e247));
        base[0u] = _e249.x;
        base[1u] = _e249.y;
        base[2u] = _e249.z;
    }
    if override_type_3_3 {
        let _e257 = base[3u];
        if (_e257 == 0f) {
            discard;
        }
    } else {
        if override_type_3_4 {
            let _e259 = base;
            let _e261 = base;
            if (dot(_e259.xyz, _e261.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e265 = base;
    out_color = _e265;
    return;
}

@fragment
fn main(@builtin(position) gl_FragCoord: vec4<f32>, @location(0) frag_color0In: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>, @location(3) frag_tex_coord2_: vec2<f32>) -> @location(0) vec4<f32> {
    gl_FragCoord_1 = gl_FragCoord;
    frag_color0In_1 = frag_color0In;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    frag_tex_coord2_1 = frag_tex_coord2_;
    main_1();
    let _e11 = out_color;
    return _e11;
}
