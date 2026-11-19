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
    materialBaseColor: vec4<f32>,
    materialFactors: vec4<f32>,
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
var<private> frag_color2In_1: vec4<f32>;
var<private> frag_tex_coord0_1: vec2<f32>;
var<private> frag_tex_coord1_1: vec2<f32>;
var<private> frag_tex_coord2_1: vec2<f32>;
var<private> out_color: vec4<f32>;

fn wired_advanced_fog_enabled_u0028_() -> bool {
    var fogType: i32;

    let _e83 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e83 + 0.5f));
    let _e88 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e90 = fogType;
    let _e93 = fogType;
    return (((_e88 > 0.5f) && (_e90 >= 1i)) && (_e93 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e83 = wired_advanced_fog_enabled_u0028_();
    if !(_e83) {
        return 0f;
    }
    let _e86 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e86, 0.000001f));
    let _e91 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e91 + 0.5f));
    let _e94 = fogType_1;
    if (_e94 == 1i) {
        let _e98 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e98 <= 0f) {
            return 0f;
        }
        let _e100 = viewDepth;
        let _e103 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e100 / _e103), 0f, 1f);
    }
    let _e108 = unnamed.advancedFogColorDensity[3u];
    let _e110 = viewDepth;
    opticalDepth = (max(_e108, 0f) * _e110);
    let _e112 = fogType_1;
    if (_e112 == 2i) {
        let _e114 = opticalDepth;
        return clamp((1f - exp(-(_e114))), 0f, 1f);
    }
    let _e119 = opticalDepth;
    let _e120 = opticalDepth;
    return clamp((1f - exp(-((_e119 * _e120)))), 0f, 1f);
}

fn wired_boost_legacy_lightmap_u0028_vf3_u003b(rgb: ptr<function, vec3<f32>>) -> vec3<f32> {
    var boosted: vec3<f32>;
    var peak: f32;

    let _e83 = (*rgb);
    let _e86 = unnamed.worldLightParams[0u];
    boosted = (_e83 * _e86);
    let _e89 = boosted[0u];
    let _e91 = boosted[1u];
    let _e93 = boosted[2u];
    peak = max(_e89, max(_e91, _e93));
    let _e96 = peak;
    if (_e96 > 1f) {
        let _e98 = peak;
        let _e99 = boosted;
        boosted = (_e99 / vec3(_e98));
    }
    let _e102 = boosted;
    return _e102;
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e84 = (*c);
    (*c) = max(_e84, vec3<f32>(0f, 0f, 0f));
    let _e86 = (*c);
    cutoff = (_e86 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e88 = (*c);
    lo = (_e88 / vec3(12.92f));
    let _e91 = (*c);
    hi = pow(((_e91 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e96 = hi;
    let _e97 = lo;
    let _e98 = cutoff;
    return mix(_e96, _e97, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e98));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param: vec3<f32>;
    var param_1: vec3<f32>;

    let _e86 = (*role);
    let _e88 = (*role);
    let _e93 = unnamed.packed_indices[(_e86 / 4u)][(_e88 % 4u)];
    let _e96 = (*role);
    let _e98 = (*role);
    let _e103 = unnamed.packed_indices[(_e96 / 4u)][(_e98 % 4u)];
    let _e108 = (*uv);
    let _e109 = textureSample(wired_bindless_images[(_e93 & 4095u)], wired_bindless_samplers[((_e103 >> bitcast<u32>(12i)) & 255u)], _e108);
    c_1 = _e109;
    let _e110 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e110))) == 0i) {
        let _e115 = c_1;
        param = _e115.xyz;
        let _e117 = sRGBToLinear_u0028_vf3_u003b((&param));
        c_1[0u] = _e117.x;
        c_1[1u] = _e117.y;
        c_1[2u] = _e117.z;
    }
    let _e124 = (*slot);
    if (lightmap_slot == (_e124 + 1i)) {
        let _e127 = c_1;
        param_1 = _e127.xyz;
        let _e129 = wired_boost_legacy_lightmap_u0028_vf3_u003b((&param_1));
        c_1[0u] = _e129.x;
        c_1[1u] = _e129.y;
        c_1[2u] = _e129.z;
    }
    let _e136 = c_1;
    return _e136;
}

fn main_1() {
    var fog: vec4<f32>;
    var frag_color0_: vec4<f32>;
    var param_2: vec3<f32>;
    var frag_color1_: vec4<f32>;
    var param_3: vec3<f32>;
    var frag_color2_: vec4<f32>;
    var param_4: vec3<f32>;
    var sampledColor0_: vec4<f32>;
    var param_5: u32;
    var param_6: vec2<f32>;
    var param_7: i32;
    var sampledColor1_: vec4<f32>;
    var param_8: u32;
    var param_9: vec2<f32>;
    var param_10: i32;
    var sampledColor2_: vec4<f32>;
    var param_11: u32;
    var param_12: vec2<f32>;
    var param_13: i32;
    var color0_: vec4<f32>;
    var color1_: vec4<f32>;
    var color2_: vec4<f32>;
    var base: vec4<f32>;
    var color1_1: vec4<f32>;
    var color2_1: vec4<f32>;
    var color1_2: vec4<f32>;
    var color2_2: vec4<f32>;
    var color1_3: vec4<f32>;
    var color2_3: vec4<f32>;
    var color1_4: vec4<f32>;
    var color2_4: vec4<f32>;
    var color1_5: vec4<f32>;
    var color2_5: vec4<f32>;
    var color1_6: vec4<f32>;
    var color2_6: vec4<f32>;
    var wetness: f32;
    var frost: f32;
    var luminance: f32;
    var fogAmount: f32;

    let _e122 = unnamed.packed_indices[0i][3u];
    let _e128 = unnamed.packed_indices[0i][3u];
    let _e133 = fog_tex_coord_1;
    let _e134 = textureSample(wired_bindless_images[(_e122 & 4095u)], wired_bindless_samplers[((_e128 >> bitcast<u32>(12i)) & 255u)], _e133);
    fog = _e134;
    let _e135 = frag_color0In_1;
    param_2 = _e135.xyz;
    let _e137 = sRGBToLinear_u0028_vf3_u003b((&param_2));
    let _e139 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e137.x, _e137.y, _e137.z, _e139);
    let _e144 = frag_color1In_1;
    param_3 = _e144.xyz;
    let _e146 = sRGBToLinear_u0028_vf3_u003b((&param_3));
    let _e148 = frag_color1In_1[3u];
    frag_color1_ = vec4<f32>(_e146.x, _e146.y, _e146.z, _e148);
    let _e153 = frag_color2In_1;
    param_4 = _e153.xyz;
    let _e155 = sRGBToLinear_u0028_vf3_u003b((&param_4));
    let _e157 = frag_color2In_1[3u];
    frag_color2_ = vec4<f32>(_e155.x, _e155.y, _e155.z, _e157);
    param_5 = 0u;
    let _e162 = frag_tex_coord0_1;
    param_6 = _e162;
    param_7 = 0i;
    let _e163 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_5), (&param_6), (&param_7));
    sampledColor0_ = _e163;
    param_8 = 1u;
    let _e164 = frag_tex_coord1_1;
    param_9 = _e164;
    param_10 = 1i;
    let _e165 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_8), (&param_9), (&param_10));
    sampledColor1_ = _e165;
    param_11 = 2u;
    let _e166 = frag_tex_coord2_1;
    param_12 = _e166;
    param_13 = 2i;
    let _e167 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_11), (&param_12), (&param_13));
    sampledColor2_ = _e167;
    let _e168 = sampledColor0_;
    let _e169 = frag_color0_;
    color0_ = (_e168 * _e169);
    if override_type_3_2 {
        let _e171 = sampledColor1_;
        let _e172 = frag_color1_;
        color1_ = (_e171 * _e172);
        let _e174 = sampledColor2_;
        let _e175 = frag_color2_;
        color2_ = (_e174 * _e175);
        let _e177 = color0_;
        let _e179 = color1_;
        let _e182 = color2_;
        let _e184 = ((_e177.xyz + _e179.xyz) + _e182.xyz);
        let _e186 = color0_[3u];
        let _e188 = color1_[3u];
        let _e191 = color2_[3u];
        base = vec4<f32>(_e184.x, _e184.y, _e184.z, ((_e186 * _e188) * _e191));
    } else {
        if override_type_3_3 {
            let _e197 = sampledColor1_;
            let _e198 = frag_color1_;
            color1_1 = (_e197 * _e198);
            let _e200 = sampledColor2_;
            let _e201 = frag_color2_;
            color2_1 = (_e200 * _e201);
            let _e204 = color0_[3u];
            let _e205 = color0_;
            color0_ = (_e205 * _e204);
            let _e208 = color1_1[3u];
            let _e209 = color1_1;
            color1_1 = (_e209 * _e208);
            let _e212 = color2_1[3u];
            let _e213 = color2_1;
            color2_1 = (_e213 * _e212);
            let _e215 = color0_;
            let _e217 = color1_1;
            let _e220 = color2_1;
            let _e222 = ((_e215.xyz + _e217.xyz) + _e220.xyz);
            let _e224 = color0_[3u];
            let _e226 = color1_1[3u];
            let _e229 = color2_1[3u];
            base = vec4<f32>(_e222.x, _e222.y, _e222.z, ((_e224 * _e226) * _e229));
        } else {
            if override_type_3_4 {
                let _e235 = sampledColor1_;
                let _e236 = frag_color1_;
                color1_2 = (_e235 * _e236);
                let _e238 = sampledColor2_;
                let _e239 = frag_color2_;
                color2_2 = (_e238 * _e239);
                let _e242 = color0_[3u];
                let _e244 = color0_;
                color0_ = (_e244 * (1f - _e242));
                let _e247 = color1_2[3u];
                let _e249 = color1_2;
                color1_2 = (_e249 * (1f - _e247));
                let _e252 = color2_2[3u];
                let _e254 = color2_2;
                color2_2 = (_e254 * (1f - _e252));
                let _e256 = color0_;
                let _e258 = color1_2;
                let _e261 = color2_2;
                let _e263 = ((_e256.xyz + _e258.xyz) + _e261.xyz);
                let _e265 = color0_[3u];
                let _e267 = color1_2[3u];
                let _e270 = color2_2[3u];
                base = vec4<f32>(_e263.x, _e263.y, _e263.z, ((_e265 * _e267) * _e270));
            } else {
                if override_type_3_5 {
                    let _e276 = sampledColor1_;
                    let _e277 = frag_color1_;
                    color1_3 = (_e276 * _e277);
                    let _e279 = sampledColor2_;
                    let _e280 = frag_color2_;
                    color2_3 = (_e279 * _e280);
                    let _e282 = color0_;
                    let _e283 = color1_3;
                    let _e285 = color1_3[3u];
                    let _e288 = color2_3;
                    let _e290 = color2_3[3u];
                    base = mix(mix(_e282, _e283, vec4(_e285)), _e288, vec4(_e290));
                } else {
                    if override_type_3_6 {
                        let _e293 = sampledColor1_;
                        let _e294 = frag_color1_;
                        color1_4 = (_e293 * _e294);
                        let _e296 = sampledColor2_;
                        let _e297 = frag_color2_;
                        color2_4 = (_e296 * _e297);
                        let _e299 = color2_4;
                        let _e300 = color1_4;
                        let _e301 = color0_;
                        let _e303 = color1_4[3u];
                        let _e307 = color2_4[3u];
                        base = mix(_e299, mix(_e300, _e301, vec4(_e303)), vec4(_e307));
                    } else {
                        if override_type_3_7 {
                            let _e310 = sampledColor1_;
                            let _e311 = frag_color1_;
                            color1_5 = (_e310 * _e311);
                            let _e313 = sampledColor2_;
                            let _e314 = frag_color2_;
                            color2_5 = (_e313 * _e314);
                            let _e316 = color2_5;
                            let _e318 = color2_5[3u];
                            let _e321 = color1_5;
                            let _e323 = color1_5[3u];
                            let _e327 = color0_;
                            base = (((_e316 + vec4(_e318)) * (_e321 + vec4(_e323))) * _e327);
                        } else {
                            let _e329 = sampledColor1_;
                            let _e330 = frag_color1_;
                            color1_6 = (_e329 * _e330);
                            let _e332 = sampledColor2_;
                            let _e333 = frag_color2_;
                            color2_6 = (_e332 * _e333);
                            let _e335 = color0_;
                            let _e337 = color1_6;
                            let _e340 = color2_6;
                            let _e342 = ((_e335.xyz * _e337.xyz) * _e340.xyz);
                            base[0u] = _e342.x;
                            base[1u] = _e342.y;
                            base[2u] = _e342.z;
                            let _e350 = color0_[3u];
                            let _e352 = color1_6[3u];
                            let _e355 = color2_6[3u];
                            base[3u] = ((_e350 * _e352) * _e355);
                        }
                    }
                }
            }
        }
    }
    if override_type_3_8 {
        let _e360 = unnamed.worldLightParams[1u];
        wetness = clamp(_e360, 0f, 1f);
        let _e364 = unnamed.worldLightParams[2u];
        frost = clamp(_e364, 0f, 1f);
        let _e366 = base;
        luminance = dot(_e366.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e369 = wetness;
        let _e371 = base;
        let _e373 = (_e371.xyz * mix(1f, 0.82f, _e369));
        base[0u] = _e373.x;
        base[1u] = _e373.y;
        base[2u] = _e373.z;
        let _e380 = base;
        let _e382 = luminance;
        let _e384 = luminance;
        let _e386 = luminance;
        let _e388 = frost;
        let _e391 = mix(_e380.xyz, vec3<f32>((_e382 * 0.88f), (_e384 * 0.94f), _e386), vec3((_e388 * 0.55f)));
        base[0u] = _e391.x;
        base[1u] = _e391.y;
        base[2u] = _e391.z;
    }
    let _e400 = unnamed.materialFactors[0u];
    if (_e400 >= 0f) {
        let _e403 = unnamed.materialBaseColor;
        let _e404 = base;
        base = (_e404 * _e403);
    }
    let _e406 = color0_;
    let _e409 = unnamed.emissionRadiance;
    let _e412 = base;
    let _e414 = (_e412.xyz + (_e406.xyz * _e409.xyz));
    base[0u] = _e414.x;
    base[1u] = _e414.y;
    base[2u] = _e414.z;
    let _e421 = wired_advanced_fog_enabled_u0028_();
    if _e421 {
        let _e422 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e422;
        if override_type_3_9 {
            let _e423 = fogAmount;
            let _e425 = base;
            let _e427 = (_e425.xyz * (1f - _e423));
            base[0u] = _e427.x;
            base[1u] = _e427.y;
            base[2u] = _e427.z;
        } else {
            if override_type_3_10 {
                let _e434 = fogAmount;
                let _e436 = base;
                base = (_e436 * (1f - _e434));
            } else {
                if override_type_3_11 {
                    let _e438 = fogAmount;
                    let _e441 = base[3u];
                    base[3u] = (_e441 * (1f - _e438));
                } else {
                    let _e444 = base;
                    let _e447 = unnamed.advancedFogColorDensity;
                    let _e449 = fogAmount;
                    let _e451 = mix(_e444.xyz, _e447.xyz, vec3(_e449));
                    base[0u] = _e451.x;
                    base[1u] = _e451.y;
                    base[2u] = _e451.z;
                }
            }
        }
    } else {
        if override_type_3_12 {
            let _e458 = base;
            let _e461 = fog[3u];
            let _e463 = (_e458.xyz * (1f - _e461));
            base[0u] = _e463.x;
            base[1u] = _e463.y;
            base[2u] = _e463.z;
        } else {
            if override_type_3_13 {
                let _e470 = base;
                let _e472 = fog[3u];
                base = (_e470 * (1f - _e472));
            } else {
                if override_type_3_14 {
                    let _e476 = base[3u];
                    let _e478 = fog[3u];
                    base[3u] = (_e476 * (1f - _e478));
                } else {
                    let _e482 = base;
                    let _e483 = fog;
                    let _e485 = unnamed.fogColor;
                    let _e488 = fog[3u];
                    base = mix(_e482, (_e483 * _e485), vec4(_e488));
                }
            }
        }
    }
    if override_type_3_15 {
        let _e492 = base[3u];
        if (_e492 == 0f) {
            discard;
        }
    } else {
        if override_type_3_16 {
            let _e494 = base;
            let _e496 = base;
            if (dot(_e494.xyz, _e496.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e500 = base;
    out_color = _e500;
    return;
}

@fragment
fn main(@builtin(position) gl_FragCoord: vec4<f32>, @location(4) fog_tex_coord: vec2<f32>, @location(0) frag_color0In: vec4<f32>, @location(5) frag_color1In: vec4<f32>, @location(6) frag_color2In: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>, @location(3) frag_tex_coord2_: vec2<f32>) -> @location(0) vec4<f32> {
    gl_FragCoord_1 = gl_FragCoord;
    fog_tex_coord_1 = fog_tex_coord;
    frag_color0In_1 = frag_color0In;
    frag_color1In_1 = frag_color1In;
    frag_color2In_1 = frag_color2In;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    frag_tex_coord2_1 = frag_tex_coord2_;
    main_1();
    let _e17 = out_color;
    return _e17;
}
