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
override override_type_3_2: bool = (override_type_3_ || override_type_3_1);
override override_type_3_3: bool = (tex_mode == 3i);
override override_type_3_4: bool = (tex_mode == 4i);
override override_type_3_5: bool = (tex_mode == 5i);
override override_type_3_6: bool = (tex_mode == 6i);
override override_type_3_7: bool = (tex_mode == 7i);
override override_type_3_8: bool = (lightmap_slot != 0i);
@id(7) override discard_mode: i32 = 0i;
override override_type_3_9: bool = (discard_mode == 1i);
override override_type_3_10: bool = (discard_mode == 2i);
@id(3) override alpha_to_coverage: i32 = 0i;

@group(0) @binding(0)
var<uniform> unnamed: UBO;
var<private> gl_FragCoord_1: vec4<f32>;
@group(1) @binding(0)
var wired_bindless_images: binding_array<texture_2d<f32>>;
@group(1) @binding(1)
var wired_bindless_samplers: binding_array<sampler>;
var<private> frag_color0In_1: vec4<f32>;
var<private> frag_color1In_1: vec4<f32>;
var<private> frag_tex_coord0_1: vec2<f32>;
var<private> frag_tex_coord1_1: vec2<f32>;
var<private> out_color: vec4<f32>;

fn wired_advanced_fog_enabled_u0028_() -> bool {
    var fogType: i32;

    let _e71 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e71 + 0.5f));
    let _e76 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e78 = fogType;
    let _e81 = fogType;
    return (((_e76 > 0.5f) && (_e78 >= 1i)) && (_e81 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e71 = wired_advanced_fog_enabled_u0028_();
    if !(_e71) {
        return 0f;
    }
    let _e74 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e74, 0.000001f));
    let _e79 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e79 + 0.5f));
    let _e82 = fogType_1;
    if (_e82 == 1i) {
        let _e86 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e86 <= 0f) {
            return 0f;
        }
        let _e88 = viewDepth;
        let _e91 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e88 / _e91), 0f, 1f);
    }
    let _e96 = unnamed.advancedFogColorDensity[3u];
    let _e98 = viewDepth;
    opticalDepth = (max(_e96, 0f) * _e98);
    let _e100 = fogType_1;
    if (_e100 == 2i) {
        let _e102 = opticalDepth;
        return clamp((1f - exp(-(_e102))), 0f, 1f);
    }
    let _e107 = opticalDepth;
    let _e108 = opticalDepth;
    return clamp((1f - exp(-((_e107 * _e108)))), 0f, 1f);
}

fn wired_boost_legacy_lightmap_u0028_vf3_u003b(rgb: ptr<function, vec3<f32>>) -> vec3<f32> {
    var boosted: vec3<f32>;
    var peak: f32;

    let _e71 = (*rgb);
    let _e74 = unnamed.worldLightParams[0u];
    boosted = (_e71 * _e74);
    let _e77 = boosted[0u];
    let _e79 = boosted[1u];
    let _e81 = boosted[2u];
    peak = max(_e77, max(_e79, _e81));
    let _e84 = peak;
    if (_e84 > 1f) {
        let _e86 = peak;
        let _e87 = boosted;
        boosted = (_e87 / vec3(_e86));
    }
    let _e90 = boosted;
    return _e90;
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e72 = (*c);
    (*c) = max(_e72, vec3<f32>(0f, 0f, 0f));
    let _e74 = (*c);
    cutoff = (_e74 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e76 = (*c);
    lo = (_e76 / vec3(12.92f));
    let _e79 = (*c);
    hi = pow(((_e79 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e84 = hi;
    let _e85 = lo;
    let _e86 = cutoff;
    return mix(_e84, _e85, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e86));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param: vec3<f32>;
    var param_1: vec3<f32>;

    let _e74 = (*role);
    let _e76 = (*role);
    let _e81 = unnamed.packed_indices[(_e74 / 4u)][(_e76 % 4u)];
    let _e84 = (*role);
    let _e86 = (*role);
    let _e91 = unnamed.packed_indices[(_e84 / 4u)][(_e86 % 4u)];
    let _e96 = (*uv);
    let _e97 = textureSample(wired_bindless_images[(_e81 & 4095u)], wired_bindless_samplers[((_e91 >> bitcast<u32>(12i)) & 255u)], _e96);
    c_1 = _e97;
    let _e98 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e98))) == 0i) {
        let _e103 = c_1;
        param = _e103.xyz;
        let _e105 = sRGBToLinear_u0028_vf3_u003b((&param));
        c_1[0u] = _e105.x;
        c_1[1u] = _e105.y;
        c_1[2u] = _e105.z;
    }
    let _e112 = (*slot);
    if (lightmap_slot == (_e112 + 1i)) {
        let _e115 = c_1;
        param_1 = _e115.xyz;
        let _e117 = wired_boost_legacy_lightmap_u0028_vf3_u003b((&param_1));
        c_1[0u] = _e117.x;
        c_1[1u] = _e117.y;
        c_1[2u] = _e117.z;
    }
    let _e124 = c_1;
    return _e124;
}

fn main_1() {
    var frag_color0_: vec4<f32>;
    var param_2: vec3<f32>;
    var frag_color1_: vec4<f32>;
    var param_3: vec3<f32>;
    var color0_: vec4<f32>;
    var param_4: u32;
    var param_5: vec2<f32>;
    var param_6: i32;
    var color1_: vec4<f32>;
    var param_7: u32;
    var param_8: vec2<f32>;
    var param_9: i32;
    var base: vec4<f32>;
    var color1_1: vec4<f32>;
    var param_10: u32;
    var param_11: vec2<f32>;
    var param_12: i32;
    var color1_2: vec4<f32>;
    var param_13: u32;
    var param_14: vec2<f32>;
    var param_15: i32;
    var color1_3: vec4<f32>;
    var param_16: u32;
    var param_17: vec2<f32>;
    var param_18: i32;
    var color1_4: vec4<f32>;
    var param_19: u32;
    var param_20: vec2<f32>;
    var param_21: i32;
    var color1_5: vec4<f32>;
    var param_22: u32;
    var param_23: vec2<f32>;
    var param_24: i32;
    var color1_6: vec4<f32>;
    var param_25: u32;
    var param_26: vec2<f32>;
    var param_27: i32;
    var wetness: f32;
    var frost: f32;
    var luminance: f32;
    var fogAmount: f32;

    let _e109 = frag_color0In_1;
    param_2 = _e109.xyz;
    let _e111 = sRGBToLinear_u0028_vf3_u003b((&param_2));
    let _e113 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e111.x, _e111.y, _e111.z, _e113);
    let _e118 = frag_color1In_1;
    param_3 = _e118.xyz;
    let _e120 = sRGBToLinear_u0028_vf3_u003b((&param_3));
    let _e122 = frag_color1In_1[3u];
    frag_color1_ = vec4<f32>(_e120.x, _e120.y, _e120.z, _e122);
    param_4 = 0u;
    let _e127 = frag_tex_coord0_1;
    param_5 = _e127;
    param_6 = 0i;
    let _e128 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_4), (&param_5), (&param_6));
    let _e129 = frag_color0_;
    color0_ = (_e128 * _e129);
    if override_type_3_2 {
        param_7 = 1u;
        let _e131 = frag_tex_coord1_1;
        param_8 = _e131;
        param_9 = 1i;
        let _e132 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_7), (&param_8), (&param_9));
        let _e133 = frag_color1_;
        color1_ = (_e132 * _e133);
        let _e135 = color0_;
        let _e137 = color1_;
        let _e139 = (_e135.xyz + _e137.xyz);
        let _e141 = color0_[3u];
        let _e143 = color1_[3u];
        base = vec4<f32>(_e139.x, _e139.y, _e139.z, (_e141 * _e143));
    } else {
        if override_type_3_3 {
            param_10 = 1u;
            let _e149 = frag_tex_coord1_1;
            param_11 = _e149;
            param_12 = 1i;
            let _e150 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_10), (&param_11), (&param_12));
            let _e151 = frag_color1_;
            color1_1 = (_e150 * _e151);
            let _e154 = color0_[3u];
            let _e155 = color0_;
            color0_ = (_e155 * _e154);
            let _e158 = color1_1[3u];
            let _e159 = color1_1;
            color1_1 = (_e159 * _e158);
            let _e161 = color0_;
            let _e163 = color1_1;
            let _e165 = (_e161.xyz + _e163.xyz);
            let _e167 = color0_[3u];
            let _e169 = color1_1[3u];
            base = vec4<f32>(_e165.x, _e165.y, _e165.z, (_e167 * _e169));
        } else {
            if override_type_3_4 {
                param_13 = 1u;
                let _e175 = frag_tex_coord1_1;
                param_14 = _e175;
                param_15 = 1i;
                let _e176 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_13), (&param_14), (&param_15));
                let _e177 = frag_color1_;
                color1_2 = (_e176 * _e177);
                let _e180 = color0_[3u];
                let _e182 = color0_;
                color0_ = (_e182 * (1f - _e180));
                let _e185 = color1_2[3u];
                let _e187 = color1_2;
                color1_2 = (_e187 * (1f - _e185));
                let _e189 = color0_;
                let _e191 = color1_2;
                let _e193 = (_e189.xyz + _e191.xyz);
                let _e195 = color0_[3u];
                let _e197 = color1_2[3u];
                base = vec4<f32>(_e193.x, _e193.y, _e193.z, (_e195 * _e197));
            } else {
                if override_type_3_5 {
                    param_16 = 1u;
                    let _e203 = frag_tex_coord1_1;
                    param_17 = _e203;
                    param_18 = 1i;
                    let _e204 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_16), (&param_17), (&param_18));
                    let _e205 = frag_color1_;
                    color1_3 = (_e204 * _e205);
                    let _e207 = color0_;
                    let _e208 = color1_3;
                    let _e210 = color1_3[3u];
                    base = mix(_e207, _e208, vec4(_e210));
                } else {
                    if override_type_3_6 {
                        param_19 = 1u;
                        let _e213 = frag_tex_coord1_1;
                        param_20 = _e213;
                        param_21 = 1i;
                        let _e214 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_19), (&param_20), (&param_21));
                        let _e215 = frag_color1_;
                        color1_4 = (_e214 * _e215);
                        let _e217 = color1_4;
                        let _e218 = color0_;
                        let _e220 = color1_4[3u];
                        base = mix(_e217, _e218, vec4(_e220));
                    } else {
                        if override_type_3_7 {
                            param_22 = 1u;
                            let _e223 = frag_tex_coord1_1;
                            param_23 = _e223;
                            param_24 = 1i;
                            let _e224 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_22), (&param_23), (&param_24));
                            let _e225 = frag_color1_;
                            color1_5 = (_e224 * _e225);
                            let _e227 = color1_5;
                            let _e229 = color1_5[3u];
                            let _e232 = color0_;
                            base = ((_e227 + vec4(_e229)) * _e232);
                        } else {
                            param_25 = 1u;
                            let _e234 = frag_tex_coord1_1;
                            param_26 = _e234;
                            param_27 = 1i;
                            let _e235 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_25), (&param_26), (&param_27));
                            let _e236 = frag_color1_;
                            color1_6 = (_e235 * _e236);
                            let _e238 = color0_;
                            let _e240 = color1_6;
                            let _e242 = (_e238.xyz * _e240.xyz);
                            base[0u] = _e242.x;
                            base[1u] = _e242.y;
                            base[2u] = _e242.z;
                            let _e250 = color0_[3u];
                            let _e252 = color1_6[3u];
                            base[3u] = (_e250 * _e252);
                        }
                    }
                }
            }
        }
    }
    if override_type_3_8 {
        let _e257 = unnamed.worldLightParams[1u];
        wetness = clamp(_e257, 0f, 1f);
        let _e261 = unnamed.worldLightParams[2u];
        frost = clamp(_e261, 0f, 1f);
        let _e263 = base;
        luminance = dot(_e263.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e266 = wetness;
        let _e268 = base;
        let _e270 = (_e268.xyz * mix(1f, 0.82f, _e266));
        base[0u] = _e270.x;
        base[1u] = _e270.y;
        base[2u] = _e270.z;
        let _e277 = base;
        let _e279 = luminance;
        let _e281 = luminance;
        let _e283 = luminance;
        let _e285 = frost;
        let _e288 = mix(_e277.xyz, vec3<f32>((_e279 * 0.88f), (_e281 * 0.94f), _e283), vec3((_e285 * 0.55f)));
        base[0u] = _e288.x;
        base[1u] = _e288.y;
        base[2u] = _e288.z;
    }
    let _e295 = color0_;
    let _e298 = unnamed.emissionRadiance;
    let _e301 = base;
    let _e303 = (_e301.xyz + (_e295.xyz * _e298.xyz));
    base[0u] = _e303.x;
    base[1u] = _e303.y;
    base[2u] = _e303.z;
    let _e310 = wired_advanced_fog_enabled_u0028_();
    if _e310 {
        let _e311 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e311;
        let _e312 = base;
        let _e315 = unnamed.advancedFogColorDensity;
        let _e317 = fogAmount;
        let _e319 = mix(_e312.xyz, _e315.xyz, vec3(_e317));
        base[0u] = _e319.x;
        base[1u] = _e319.y;
        base[2u] = _e319.z;
    }
    if override_type_3_9 {
        let _e327 = base[3u];
        if (_e327 == 0f) {
            discard;
        }
    } else {
        if override_type_3_10 {
            let _e329 = base;
            let _e331 = base;
            if (dot(_e329.xyz, _e331.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e335 = base;
    out_color = _e335;
    return;
}

@fragment
fn main(@builtin(position) gl_FragCoord: vec4<f32>, @location(0) frag_color0In: vec4<f32>, @location(5) frag_color1In: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>) -> @location(0) vec4<f32> {
    gl_FragCoord_1 = gl_FragCoord;
    frag_color0In_1 = frag_color0In;
    frag_color1In_1 = frag_color1In;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    main_1();
    let _e11 = out_color;
    return _e11;
}
