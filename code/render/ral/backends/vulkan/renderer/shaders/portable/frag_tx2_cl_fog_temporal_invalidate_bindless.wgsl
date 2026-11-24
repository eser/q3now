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

struct FragmentOutput {
    @location(1) member: vec2<f32>,
    @location(2) member_1: vec4<f32>,
    @location(0) member_2: vec4<f32>,
}

@id(4) override tex_domain: i32 = 0i;
@id(26) override lightmap_slot: i32 = 0i;
@id(6) override tex_mode: i32 = 0i;
override override_type_6_: bool = (tex_mode == 1i);
override override_type_6_1: bool = (tex_mode == 2i);
override override_type_6_2: bool = (override_type_6_ || override_type_6_1);
override override_type_6_3: bool = (tex_mode == 3i);
override override_type_6_4: bool = (tex_mode == 4i);
override override_type_6_5: bool = (tex_mode == 5i);
override override_type_6_6: bool = (tex_mode == 6i);
override override_type_6_7: bool = (tex_mode == 7i);
override override_type_6_8: bool = (lightmap_slot != 0i);
@id(10) override acff: i32 = 0i;
override override_type_6_9: bool = (acff == 1i);
override override_type_6_10: bool = (acff == 2i);
override override_type_6_11: bool = (acff == 3i);
override override_type_6_12: bool = (acff == 1i);
override override_type_6_13: bool = (acff == 2i);
override override_type_6_14: bool = (acff == 3i);
@id(7) override discard_mode: i32 = 0i;
override override_type_6_15: bool = (discard_mode == 1i);
override override_type_6_16: bool = (discard_mode == 2i);
@id(3) override alpha_to_coverage: i32 = 0i;

var<private> out_temporal_velocity: vec2<f32>;
var<private> out_temporal_validity: vec4<f32>;
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
var<private> temporalCurrentClip_1: vec4<f32>;
var<private> temporalPreviousClip_1: vec4<f32>;
var<private> temporalOutcome_1: u32;

fn wiredTemporalWriteAux_u0028_f1_u003b_vf4_u003b(coverageConfidence: ptr<function, f32>, sceneColor: ptr<function, vec4<f32>>) {
    out_temporal_velocity = vec2<f32>(0f, 0f);
    out_temporal_validity = vec4<f32>(0f, 0f, 0f, 0f);
    let _e90 = (*sceneColor)[0u];
    let _e92 = (*sceneColor)[1u];
    let _e94 = (*sceneColor)[2u];
    let _e99 = (*sceneColor)[3u];
    out_temporal_validity = vec4<f32>(clamp(max(_e90, max(_e92, _e94)), 0f, 1f), 0f, 0f, clamp(_e99, 0f, 1f));
    return;
}

fn wired_advanced_fog_enabled_u0028_() -> bool {
    var fogType: i32;

    let _e90 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e90 + 0.5f));
    let _e95 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e97 = fogType;
    let _e100 = fogType;
    return (((_e95 > 0.5f) && (_e97 >= 1i)) && (_e100 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e90 = wired_advanced_fog_enabled_u0028_();
    if !(_e90) {
        return 0f;
    }
    let _e93 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e93, 0.000001f));
    let _e98 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e98 + 0.5f));
    let _e101 = fogType_1;
    if (_e101 == 1i) {
        let _e105 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e105 <= 0f) {
            return 0f;
        }
        let _e107 = viewDepth;
        let _e110 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e107 / _e110), 0f, 1f);
    }
    let _e115 = unnamed.advancedFogColorDensity[3u];
    let _e117 = viewDepth;
    opticalDepth = (max(_e115, 0f) * _e117);
    let _e119 = fogType_1;
    if (_e119 == 2i) {
        let _e121 = opticalDepth;
        return clamp((1f - exp(-(_e121))), 0f, 1f);
    }
    let _e126 = opticalDepth;
    let _e127 = opticalDepth;
    return clamp((1f - exp(-((_e126 * _e127)))), 0f, 1f);
}

fn wired_boost_legacy_lightmap_u0028_vf3_u003b(rgb: ptr<function, vec3<f32>>) -> vec3<f32> {
    var boosted: vec3<f32>;
    var peak: f32;

    let _e90 = (*rgb);
    let _e93 = unnamed.worldLightParams[0u];
    boosted = (_e90 * _e93);
    let _e96 = boosted[0u];
    let _e98 = boosted[1u];
    let _e100 = boosted[2u];
    peak = max(_e96, max(_e98, _e100));
    let _e103 = peak;
    if (_e103 > 1f) {
        let _e105 = peak;
        let _e106 = boosted;
        boosted = (_e106 / vec3(_e105));
    }
    let _e109 = boosted;
    return _e109;
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e91 = (*c);
    (*c) = max(_e91, vec3<f32>(0f, 0f, 0f));
    let _e93 = (*c);
    cutoff = (_e93 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e95 = (*c);
    lo = (_e95 / vec3(12.92f));
    let _e98 = (*c);
    hi = pow(((_e98 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e103 = hi;
    let _e104 = lo;
    let _e105 = cutoff;
    return mix(_e103, _e104, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e105));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param: vec3<f32>;
    var param_1: vec3<f32>;

    let _e93 = (*role);
    let _e95 = (*role);
    let _e100 = unnamed.packed_indices[(_e93 / 4u)][(_e95 % 4u)];
    let _e103 = (*role);
    let _e105 = (*role);
    let _e110 = unnamed.packed_indices[(_e103 / 4u)][(_e105 % 4u)];
    let _e115 = (*uv);
    let _e116 = textureSample(wired_bindless_images[(_e100 & 4095u)], wired_bindless_samplers[((_e110 >> bitcast<u32>(12i)) & 255u)], _e115);
    c_1 = _e116;
    let _e117 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e117))) == 0i) {
        let _e122 = c_1;
        param = _e122.xyz;
        let _e124 = sRGBToLinear_u0028_vf3_u003b((&param));
        c_1[0u] = _e124.x;
        c_1[1u] = _e124.y;
        c_1[2u] = _e124.z;
    }
    let _e131 = (*slot);
    if (lightmap_slot == (_e131 + 1i)) {
        let _e134 = c_1;
        param_1 = _e134.xyz;
        let _e136 = wired_boost_legacy_lightmap_u0028_vf3_u003b((&param_1));
        c_1[0u] = _e136.x;
        c_1[1u] = _e136.y;
        c_1[2u] = _e136.z;
    }
    let _e143 = c_1;
    return _e143;
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
    var param_14: f32;
    var param_15: vec4<f32>;

    let _e131 = unnamed.packed_indices[0i][3u];
    let _e137 = unnamed.packed_indices[0i][3u];
    let _e142 = fog_tex_coord_1;
    let _e143 = textureSample(wired_bindless_images[(_e131 & 4095u)], wired_bindless_samplers[((_e137 >> bitcast<u32>(12i)) & 255u)], _e142);
    fog = _e143;
    let _e144 = frag_color0In_1;
    param_2 = _e144.xyz;
    let _e146 = sRGBToLinear_u0028_vf3_u003b((&param_2));
    let _e148 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e146.x, _e146.y, _e146.z, _e148);
    let _e153 = frag_color1In_1;
    param_3 = _e153.xyz;
    let _e155 = sRGBToLinear_u0028_vf3_u003b((&param_3));
    let _e157 = frag_color1In_1[3u];
    frag_color1_ = vec4<f32>(_e155.x, _e155.y, _e155.z, _e157);
    let _e162 = frag_color2In_1;
    param_4 = _e162.xyz;
    let _e164 = sRGBToLinear_u0028_vf3_u003b((&param_4));
    let _e166 = frag_color2In_1[3u];
    frag_color2_ = vec4<f32>(_e164.x, _e164.y, _e164.z, _e166);
    param_5 = 0u;
    let _e171 = frag_tex_coord0_1;
    param_6 = _e171;
    param_7 = 0i;
    let _e172 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_5), (&param_6), (&param_7));
    sampledColor0_ = _e172;
    param_8 = 1u;
    let _e173 = frag_tex_coord1_1;
    param_9 = _e173;
    param_10 = 1i;
    let _e174 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_8), (&param_9), (&param_10));
    sampledColor1_ = _e174;
    param_11 = 2u;
    let _e175 = frag_tex_coord2_1;
    param_12 = _e175;
    param_13 = 2i;
    let _e176 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_11), (&param_12), (&param_13));
    sampledColor2_ = _e176;
    let _e177 = sampledColor0_;
    let _e178 = frag_color0_;
    color0_ = (_e177 * _e178);
    if override_type_6_2 {
        let _e180 = sampledColor1_;
        let _e181 = frag_color1_;
        color1_ = (_e180 * _e181);
        let _e183 = sampledColor2_;
        let _e184 = frag_color2_;
        color2_ = (_e183 * _e184);
        let _e186 = color0_;
        let _e188 = color1_;
        let _e191 = color2_;
        let _e193 = ((_e186.xyz + _e188.xyz) + _e191.xyz);
        let _e195 = color0_[3u];
        let _e197 = color1_[3u];
        let _e200 = color2_[3u];
        base = vec4<f32>(_e193.x, _e193.y, _e193.z, ((_e195 * _e197) * _e200));
    } else {
        if override_type_6_3 {
            let _e206 = sampledColor1_;
            let _e207 = frag_color1_;
            color1_1 = (_e206 * _e207);
            let _e209 = sampledColor2_;
            let _e210 = frag_color2_;
            color2_1 = (_e209 * _e210);
            let _e213 = color0_[3u];
            let _e214 = color0_;
            color0_ = (_e214 * _e213);
            let _e217 = color1_1[3u];
            let _e218 = color1_1;
            color1_1 = (_e218 * _e217);
            let _e221 = color2_1[3u];
            let _e222 = color2_1;
            color2_1 = (_e222 * _e221);
            let _e224 = color0_;
            let _e226 = color1_1;
            let _e229 = color2_1;
            let _e231 = ((_e224.xyz + _e226.xyz) + _e229.xyz);
            let _e233 = color0_[3u];
            let _e235 = color1_1[3u];
            let _e238 = color2_1[3u];
            base = vec4<f32>(_e231.x, _e231.y, _e231.z, ((_e233 * _e235) * _e238));
        } else {
            if override_type_6_4 {
                let _e244 = sampledColor1_;
                let _e245 = frag_color1_;
                color1_2 = (_e244 * _e245);
                let _e247 = sampledColor2_;
                let _e248 = frag_color2_;
                color2_2 = (_e247 * _e248);
                let _e251 = color0_[3u];
                let _e253 = color0_;
                color0_ = (_e253 * (1f - _e251));
                let _e256 = color1_2[3u];
                let _e258 = color1_2;
                color1_2 = (_e258 * (1f - _e256));
                let _e261 = color2_2[3u];
                let _e263 = color2_2;
                color2_2 = (_e263 * (1f - _e261));
                let _e265 = color0_;
                let _e267 = color1_2;
                let _e270 = color2_2;
                let _e272 = ((_e265.xyz + _e267.xyz) + _e270.xyz);
                let _e274 = color0_[3u];
                let _e276 = color1_2[3u];
                let _e279 = color2_2[3u];
                base = vec4<f32>(_e272.x, _e272.y, _e272.z, ((_e274 * _e276) * _e279));
            } else {
                if override_type_6_5 {
                    let _e285 = sampledColor1_;
                    let _e286 = frag_color1_;
                    color1_3 = (_e285 * _e286);
                    let _e288 = sampledColor2_;
                    let _e289 = frag_color2_;
                    color2_3 = (_e288 * _e289);
                    let _e291 = color0_;
                    let _e292 = color1_3;
                    let _e294 = color1_3[3u];
                    let _e297 = color2_3;
                    let _e299 = color2_3[3u];
                    base = mix(mix(_e291, _e292, vec4(_e294)), _e297, vec4(_e299));
                } else {
                    if override_type_6_6 {
                        let _e302 = sampledColor1_;
                        let _e303 = frag_color1_;
                        color1_4 = (_e302 * _e303);
                        let _e305 = sampledColor2_;
                        let _e306 = frag_color2_;
                        color2_4 = (_e305 * _e306);
                        let _e308 = color2_4;
                        let _e309 = color1_4;
                        let _e310 = color0_;
                        let _e312 = color1_4[3u];
                        let _e316 = color2_4[3u];
                        base = mix(_e308, mix(_e309, _e310, vec4(_e312)), vec4(_e316));
                    } else {
                        if override_type_6_7 {
                            let _e319 = sampledColor1_;
                            let _e320 = frag_color1_;
                            color1_5 = (_e319 * _e320);
                            let _e322 = sampledColor2_;
                            let _e323 = frag_color2_;
                            color2_5 = (_e322 * _e323);
                            let _e325 = color2_5;
                            let _e327 = color2_5[3u];
                            let _e330 = color1_5;
                            let _e332 = color1_5[3u];
                            let _e336 = color0_;
                            base = (((_e325 + vec4(_e327)) * (_e330 + vec4(_e332))) * _e336);
                        } else {
                            let _e338 = sampledColor1_;
                            let _e339 = frag_color1_;
                            color1_6 = (_e338 * _e339);
                            let _e341 = sampledColor2_;
                            let _e342 = frag_color2_;
                            color2_6 = (_e341 * _e342);
                            let _e344 = color0_;
                            let _e346 = color1_6;
                            let _e349 = color2_6;
                            let _e351 = ((_e344.xyz * _e346.xyz) * _e349.xyz);
                            base[0u] = _e351.x;
                            base[1u] = _e351.y;
                            base[2u] = _e351.z;
                            let _e359 = color0_[3u];
                            let _e361 = color1_6[3u];
                            let _e364 = color2_6[3u];
                            base[3u] = ((_e359 * _e361) * _e364);
                        }
                    }
                }
            }
        }
    }
    if override_type_6_8 {
        let _e369 = unnamed.worldLightParams[1u];
        wetness = clamp(_e369, 0f, 1f);
        let _e373 = unnamed.worldLightParams[2u];
        frost = clamp(_e373, 0f, 1f);
        let _e375 = base;
        luminance = dot(_e375.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e378 = wetness;
        let _e380 = base;
        let _e382 = (_e380.xyz * mix(1f, 0.82f, _e378));
        base[0u] = _e382.x;
        base[1u] = _e382.y;
        base[2u] = _e382.z;
        let _e389 = base;
        let _e391 = luminance;
        let _e393 = luminance;
        let _e395 = luminance;
        let _e397 = frost;
        let _e400 = mix(_e389.xyz, vec3<f32>((_e391 * 0.88f), (_e393 * 0.94f), _e395), vec3((_e397 * 0.55f)));
        base[0u] = _e400.x;
        base[1u] = _e400.y;
        base[2u] = _e400.z;
    }
    let _e409 = unnamed.materialFactors[0u];
    if (_e409 >= 0f) {
        let _e412 = unnamed.materialBaseColor;
        let _e413 = base;
        base = (_e413 * _e412);
    }
    let _e415 = color0_;
    let _e418 = unnamed.emissionRadiance;
    let _e421 = base;
    let _e423 = (_e421.xyz + (_e415.xyz * _e418.xyz));
    base[0u] = _e423.x;
    base[1u] = _e423.y;
    base[2u] = _e423.z;
    let _e430 = wired_advanced_fog_enabled_u0028_();
    if _e430 {
        let _e431 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e431;
        if override_type_6_9 {
            let _e432 = fogAmount;
            let _e434 = base;
            let _e436 = (_e434.xyz * (1f - _e432));
            base[0u] = _e436.x;
            base[1u] = _e436.y;
            base[2u] = _e436.z;
        } else {
            if override_type_6_10 {
                let _e443 = fogAmount;
                let _e445 = base;
                base = (_e445 * (1f - _e443));
            } else {
                if override_type_6_11 {
                    let _e447 = fogAmount;
                    let _e450 = base[3u];
                    base[3u] = (_e450 * (1f - _e447));
                } else {
                    let _e453 = base;
                    let _e456 = unnamed.advancedFogColorDensity;
                    let _e458 = fogAmount;
                    let _e460 = mix(_e453.xyz, _e456.xyz, vec3(_e458));
                    base[0u] = _e460.x;
                    base[1u] = _e460.y;
                    base[2u] = _e460.z;
                }
            }
        }
    } else {
        if override_type_6_12 {
            let _e467 = base;
            let _e470 = fog[3u];
            let _e472 = (_e467.xyz * (1f - _e470));
            base[0u] = _e472.x;
            base[1u] = _e472.y;
            base[2u] = _e472.z;
        } else {
            if override_type_6_13 {
                let _e479 = base;
                let _e481 = fog[3u];
                base = (_e479 * (1f - _e481));
            } else {
                if override_type_6_14 {
                    let _e485 = base[3u];
                    let _e487 = fog[3u];
                    base[3u] = (_e485 * (1f - _e487));
                } else {
                    let _e491 = base;
                    let _e492 = fog;
                    let _e494 = unnamed.fogColor;
                    let _e497 = fog[3u];
                    base = mix(_e491, (_e492 * _e494), vec4(_e497));
                }
            }
        }
    }
    if override_type_6_15 {
        let _e501 = base[3u];
        if (_e501 == 0f) {
            discard;
        }
    } else {
        if override_type_6_16 {
            let _e503 = base;
            let _e505 = base;
            if (dot(_e503.xyz, _e505.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e509 = base;
    out_color = _e509;
    param_14 = 1f;
    let _e510 = base;
    param_15 = _e510;
    wiredTemporalWriteAux_u0028_f1_u003b_vf4_u003b((&param_14), (&param_15));
    return;
}

@fragment
fn main(@builtin(position) gl_FragCoord: vec4<f32>, @location(4) fog_tex_coord: vec2<f32>, @location(0) frag_color0In: vec4<f32>, @location(5) frag_color1In: vec4<f32>, @location(6) frag_color2In: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>, @location(3) frag_tex_coord2_: vec2<f32>, @location(10) temporalCurrentClip: vec4<f32>, @location(11) temporalPreviousClip: vec4<f32>, @location(12) @interpolate(flat) temporalOutcome: u32) -> FragmentOutput {
    gl_FragCoord_1 = gl_FragCoord;
    fog_tex_coord_1 = fog_tex_coord;
    frag_color0In_1 = frag_color0In;
    frag_color1In_1 = frag_color1In;
    frag_color2In_1 = frag_color2In;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    frag_tex_coord2_1 = frag_tex_coord2_;
    temporalCurrentClip_1 = temporalCurrentClip;
    temporalPreviousClip_1 = temporalPreviousClip;
    temporalOutcome_1 = temporalOutcome;
    main_1();
    let _e25 = out_temporal_velocity;
    let _e26 = out_temporal_validity;
    let _e27 = out_color;
    return FragmentOutput(_e25, _e26, _e27);
}
