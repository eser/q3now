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
@id(10) override acff: i32 = 0i;
override override_type_3_9: bool = (acff == 1i);
override override_type_3_10: bool = (acff == 2i);
override override_type_3_11: bool = (acff == 3i);
override override_type_3_12: bool = (acff == 1i);
override override_type_3_13: bool = (acff == 2i);
override override_type_3_14: bool = (acff == 3i);
@id(7) override discard_mode: i32 = 0i;
override override_type_3_15: bool = (discard_mode == 1i);
override override_type_3_16: bool = (discard_mode == 2i);
@id(3) override alpha_to_coverage: i32 = 0i;

@group(0) @binding(0)
var<uniform> unnamed: UBO;
var<private> gl_FragCoord_1: vec4<f32>;
@group(1) @binding(0)
var wired_bindless_images: binding_array<texture_2d<f32>>;
@group(1) @binding(1)
var wired_bindless_samplers: binding_array<sampler>;
var<private> fog_tex_coord_1: vec2<f32>;
var<private> frag_color0In_1: vec4<f32>;
var<private> frag_color1In_1: vec4<f32>;
var<private> frag_tex_coord0_1: vec2<f32>;
var<private> frag_tex_coord1_1: vec2<f32>;
var<private> out_color: vec4<f32>;

fn wired_advanced_fog_enabled_u0028_() -> bool {
    var fogType: i32;

    let _e79 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e79 + 0.5f));
    let _e84 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e86 = fogType;
    let _e89 = fogType;
    return (((_e84 > 0.5f) && (_e86 >= 1i)) && (_e89 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e79 = wired_advanced_fog_enabled_u0028_();
    if !(_e79) {
        return 0f;
    }
    let _e82 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e82, 0.000001f));
    let _e87 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e87 + 0.5f));
    let _e90 = fogType_1;
    if (_e90 == 1i) {
        let _e94 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e94 <= 0f) {
            return 0f;
        }
        let _e96 = viewDepth;
        let _e99 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e96 / _e99), 0f, 1f);
    }
    let _e104 = unnamed.advancedFogColorDensity[3u];
    let _e106 = viewDepth;
    opticalDepth = (max(_e104, 0f) * _e106);
    let _e108 = fogType_1;
    if (_e108 == 2i) {
        let _e110 = opticalDepth;
        return clamp((1f - exp(-(_e110))), 0f, 1f);
    }
    let _e115 = opticalDepth;
    let _e116 = opticalDepth;
    return clamp((1f - exp(-((_e115 * _e116)))), 0f, 1f);
}

fn wired_boost_legacy_lightmap_u0028_vf3_u003b(rgb: ptr<function, vec3<f32>>) -> vec3<f32> {
    var boosted: vec3<f32>;
    var peak: f32;

    let _e79 = (*rgb);
    let _e82 = unnamed.worldLightParams[0u];
    boosted = (_e79 * _e82);
    let _e85 = boosted[0u];
    let _e87 = boosted[1u];
    let _e89 = boosted[2u];
    peak = max(_e85, max(_e87, _e89));
    let _e92 = peak;
    if (_e92 > 1f) {
        let _e94 = peak;
        let _e95 = boosted;
        boosted = (_e95 / vec3(_e94));
    }
    let _e98 = boosted;
    return _e98;
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e80 = (*c);
    (*c) = max(_e80, vec3<f32>(0f, 0f, 0f));
    let _e82 = (*c);
    cutoff = (_e82 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e84 = (*c);
    lo = (_e84 / vec3(12.92f));
    let _e87 = (*c);
    hi = pow(((_e87 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e92 = hi;
    let _e93 = lo;
    let _e94 = cutoff;
    return mix(_e92, _e93, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e94));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param: vec3<f32>;
    var param_1: vec3<f32>;

    let _e82 = (*role);
    let _e84 = (*role);
    let _e89 = unnamed.packed_indices[(_e82 / 4u)][(_e84 % 4u)];
    let _e92 = (*role);
    let _e94 = (*role);
    let _e99 = unnamed.packed_indices[(_e92 / 4u)][(_e94 % 4u)];
    let _e104 = (*uv);
    let _e105 = textureSample(wired_bindless_images[(_e89 & 4095u)], wired_bindless_samplers[((_e99 >> bitcast<u32>(12i)) & 255u)], _e104);
    c_1 = _e105;
    let _e106 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e106))) == 0i) {
        let _e111 = c_1;
        param = _e111.xyz;
        let _e113 = sRGBToLinear_u0028_vf3_u003b((&param));
        c_1[0u] = _e113.x;
        c_1[1u] = _e113.y;
        c_1[2u] = _e113.z;
    }
    let _e120 = (*slot);
    if (lightmap_slot == (_e120 + 1i)) {
        let _e123 = c_1;
        param_1 = _e123.xyz;
        let _e125 = wired_boost_legacy_lightmap_u0028_vf3_u003b((&param_1));
        c_1[0u] = _e125.x;
        c_1[1u] = _e125.y;
        c_1[2u] = _e125.z;
    }
    let _e132 = c_1;
    return _e132;
}

fn main_1() {
    var fog: vec4<f32>;
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

    let _e121 = unnamed.packed_indices[0i][3u];
    let _e127 = unnamed.packed_indices[0i][3u];
    let _e132 = fog_tex_coord_1;
    let _e133 = textureSample(wired_bindless_images[(_e121 & 4095u)], wired_bindless_samplers[((_e127 >> bitcast<u32>(12i)) & 255u)], _e132);
    fog = _e133;
    let _e134 = frag_color0In_1;
    param_2 = _e134.xyz;
    let _e136 = sRGBToLinear_u0028_vf3_u003b((&param_2));
    let _e138 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e136.x, _e136.y, _e136.z, _e138);
    let _e143 = frag_color1In_1;
    param_3 = _e143.xyz;
    let _e145 = sRGBToLinear_u0028_vf3_u003b((&param_3));
    let _e147 = frag_color1In_1[3u];
    frag_color1_ = vec4<f32>(_e145.x, _e145.y, _e145.z, _e147);
    param_4 = 0u;
    let _e152 = frag_tex_coord0_1;
    param_5 = _e152;
    param_6 = 0i;
    let _e153 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_4), (&param_5), (&param_6));
    let _e154 = frag_color0_;
    color0_ = (_e153 * _e154);
    if override_type_3_2 {
        param_7 = 1u;
        let _e156 = frag_tex_coord1_1;
        param_8 = _e156;
        param_9 = 1i;
        let _e157 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_7), (&param_8), (&param_9));
        let _e158 = frag_color1_;
        color1_ = (_e157 * _e158);
        let _e160 = color0_;
        let _e162 = color1_;
        let _e164 = (_e160.xyz + _e162.xyz);
        let _e166 = color0_[3u];
        let _e168 = color1_[3u];
        base = vec4<f32>(_e164.x, _e164.y, _e164.z, (_e166 * _e168));
    } else {
        if override_type_3_3 {
            param_10 = 1u;
            let _e174 = frag_tex_coord1_1;
            param_11 = _e174;
            param_12 = 1i;
            let _e175 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_10), (&param_11), (&param_12));
            let _e176 = frag_color1_;
            color1_1 = (_e175 * _e176);
            let _e179 = color0_[3u];
            let _e180 = color0_;
            color0_ = (_e180 * _e179);
            let _e183 = color1_1[3u];
            let _e184 = color1_1;
            color1_1 = (_e184 * _e183);
            let _e186 = color0_;
            let _e188 = color1_1;
            let _e190 = (_e186.xyz + _e188.xyz);
            let _e192 = color0_[3u];
            let _e194 = color1_1[3u];
            base = vec4<f32>(_e190.x, _e190.y, _e190.z, (_e192 * _e194));
        } else {
            if override_type_3_4 {
                param_13 = 1u;
                let _e200 = frag_tex_coord1_1;
                param_14 = _e200;
                param_15 = 1i;
                let _e201 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_13), (&param_14), (&param_15));
                let _e202 = frag_color1_;
                color1_2 = (_e201 * _e202);
                let _e205 = color0_[3u];
                let _e207 = color0_;
                color0_ = (_e207 * (1f - _e205));
                let _e210 = color1_2[3u];
                let _e212 = color1_2;
                color1_2 = (_e212 * (1f - _e210));
                let _e214 = color0_;
                let _e216 = color1_2;
                let _e218 = (_e214.xyz + _e216.xyz);
                let _e220 = color0_[3u];
                let _e222 = color1_2[3u];
                base = vec4<f32>(_e218.x, _e218.y, _e218.z, (_e220 * _e222));
            } else {
                if override_type_3_5 {
                    param_16 = 1u;
                    let _e228 = frag_tex_coord1_1;
                    param_17 = _e228;
                    param_18 = 1i;
                    let _e229 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_16), (&param_17), (&param_18));
                    let _e230 = frag_color1_;
                    color1_3 = (_e229 * _e230);
                    let _e232 = color0_;
                    let _e233 = color1_3;
                    let _e235 = color1_3[3u];
                    base = mix(_e232, _e233, vec4(_e235));
                } else {
                    if override_type_3_6 {
                        param_19 = 1u;
                        let _e238 = frag_tex_coord1_1;
                        param_20 = _e238;
                        param_21 = 1i;
                        let _e239 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_19), (&param_20), (&param_21));
                        let _e240 = frag_color1_;
                        color1_4 = (_e239 * _e240);
                        let _e242 = color1_4;
                        let _e243 = color0_;
                        let _e245 = color1_4[3u];
                        base = mix(_e242, _e243, vec4(_e245));
                    } else {
                        if override_type_3_7 {
                            param_22 = 1u;
                            let _e248 = frag_tex_coord1_1;
                            param_23 = _e248;
                            param_24 = 1i;
                            let _e249 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_22), (&param_23), (&param_24));
                            let _e250 = frag_color1_;
                            color1_5 = (_e249 * _e250);
                            let _e252 = color1_5;
                            let _e254 = color1_5[3u];
                            let _e257 = color0_;
                            base = ((_e252 + vec4(_e254)) * _e257);
                        } else {
                            param_25 = 1u;
                            let _e259 = frag_tex_coord1_1;
                            param_26 = _e259;
                            param_27 = 1i;
                            let _e260 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_25), (&param_26), (&param_27));
                            let _e261 = frag_color1_;
                            color1_6 = (_e260 * _e261);
                            let _e263 = color0_;
                            let _e265 = color1_6;
                            let _e267 = (_e263.xyz * _e265.xyz);
                            base[0u] = _e267.x;
                            base[1u] = _e267.y;
                            base[2u] = _e267.z;
                            let _e275 = color0_[3u];
                            let _e277 = color1_6[3u];
                            base[3u] = (_e275 * _e277);
                        }
                    }
                }
            }
        }
    }
    if override_type_3_8 {
        let _e282 = unnamed.worldLightParams[1u];
        wetness = clamp(_e282, 0f, 1f);
        let _e286 = unnamed.worldLightParams[2u];
        frost = clamp(_e286, 0f, 1f);
        let _e288 = base;
        luminance = dot(_e288.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e291 = wetness;
        let _e293 = base;
        let _e295 = (_e293.xyz * mix(1f, 0.82f, _e291));
        base[0u] = _e295.x;
        base[1u] = _e295.y;
        base[2u] = _e295.z;
        let _e302 = base;
        let _e304 = luminance;
        let _e306 = luminance;
        let _e308 = luminance;
        let _e310 = frost;
        let _e313 = mix(_e302.xyz, vec3<f32>((_e304 * 0.88f), (_e306 * 0.94f), _e308), vec3((_e310 * 0.55f)));
        base[0u] = _e313.x;
        base[1u] = _e313.y;
        base[2u] = _e313.z;
    }
    let _e320 = color0_;
    let _e323 = unnamed.emissionRadiance;
    let _e326 = base;
    let _e328 = (_e326.xyz + (_e320.xyz * _e323.xyz));
    base[0u] = _e328.x;
    base[1u] = _e328.y;
    base[2u] = _e328.z;
    let _e335 = wired_advanced_fog_enabled_u0028_();
    if _e335 {
        let _e336 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e336;
        if override_type_3_9 {
            let _e337 = fogAmount;
            let _e339 = base;
            let _e341 = (_e339.xyz * (1f - _e337));
            base[0u] = _e341.x;
            base[1u] = _e341.y;
            base[2u] = _e341.z;
        } else {
            if override_type_3_10 {
                let _e348 = fogAmount;
                let _e350 = base;
                base = (_e350 * (1f - _e348));
            } else {
                if override_type_3_11 {
                    let _e352 = fogAmount;
                    let _e355 = base[3u];
                    base[3u] = (_e355 * (1f - _e352));
                } else {
                    let _e358 = base;
                    let _e361 = unnamed.advancedFogColorDensity;
                    let _e363 = fogAmount;
                    let _e365 = mix(_e358.xyz, _e361.xyz, vec3(_e363));
                    base[0u] = _e365.x;
                    base[1u] = _e365.y;
                    base[2u] = _e365.z;
                }
            }
        }
    } else {
        if override_type_3_12 {
            let _e372 = base;
            let _e375 = fog[3u];
            let _e377 = (_e372.xyz * (1f - _e375));
            base[0u] = _e377.x;
            base[1u] = _e377.y;
            base[2u] = _e377.z;
        } else {
            if override_type_3_13 {
                let _e384 = base;
                let _e386 = fog[3u];
                base = (_e384 * (1f - _e386));
            } else {
                if override_type_3_14 {
                    let _e390 = base[3u];
                    let _e392 = fog[3u];
                    base[3u] = (_e390 * (1f - _e392));
                } else {
                    let _e396 = base;
                    let _e397 = fog;
                    let _e399 = unnamed.fogColor;
                    let _e402 = fog[3u];
                    base = mix(_e396, (_e397 * _e399), vec4(_e402));
                }
            }
        }
    }
    if override_type_3_15 {
        let _e406 = base[3u];
        if (_e406 == 0f) {
            discard;
        }
    } else {
        if override_type_3_16 {
            let _e408 = base;
            let _e410 = base;
            if (dot(_e408.xyz, _e410.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e414 = base;
    out_color = _e414;
    return;
}

@fragment
fn main(@builtin(position) gl_FragCoord: vec4<f32>, @location(4) fog_tex_coord: vec2<f32>, @location(0) frag_color0In: vec4<f32>, @location(5) frag_color1In: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>) -> @location(0) vec4<f32> {
    gl_FragCoord_1 = gl_FragCoord;
    fog_tex_coord_1 = fog_tex_coord;
    frag_color0In_1 = frag_color0In;
    frag_color1In_1 = frag_color1In;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    main_1();
    let _e13 = out_color;
    return _e13;
}
