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
    @location(2) member_1: f32,
    @location(0) member_2: vec4<f32>,
}

@id(4) override tex_domain: i32 = 0i;
@id(26) override lightmap_slot: i32 = 0i;
@id(6) override tex_mode: i32 = 0i;
override override_type_4_: bool = (tex_mode == 1i);
override override_type_4_1: bool = (tex_mode == 2i);
override override_type_4_2: bool = (override_type_4_ || override_type_4_1);
override override_type_4_3: bool = (tex_mode == 3i);
override override_type_4_4: bool = (tex_mode == 4i);
override override_type_4_5: bool = (tex_mode == 5i);
override override_type_4_6: bool = (tex_mode == 6i);
override override_type_4_7: bool = (tex_mode == 7i);
override override_type_4_8: bool = (lightmap_slot != 0i);
@id(10) override acff: i32 = 0i;
override override_type_4_9: bool = (acff == 1i);
override override_type_4_10: bool = (acff == 2i);
override override_type_4_11: bool = (acff == 3i);
override override_type_4_12: bool = (acff == 1i);
override override_type_4_13: bool = (acff == 2i);
override override_type_4_14: bool = (acff == 3i);
@id(7) override discard_mode: i32 = 0i;
override override_type_4_15: bool = (discard_mode == 1i);
override override_type_4_16: bool = (discard_mode == 2i);
@id(3) override alpha_to_coverage: i32 = 0i;

var<private> out_temporal_velocity: vec2<f32>;
var<private> out_temporal_validity: f32;
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

fn wiredTemporalWriteAux_u0028_f1_u003b(coverageConfidence: ptr<function, f32>) {
    out_temporal_velocity = vec2<f32>(0f, 0f);
    out_temporal_validity = 0f;
    return;
}

fn wired_advanced_fog_enabled_u0028_() -> bool {
    var fogType: i32;

    let _e89 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e89 + 0.5f));
    let _e94 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e96 = fogType;
    let _e99 = fogType;
    return (((_e94 > 0.5f) && (_e96 >= 1i)) && (_e99 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e89 = wired_advanced_fog_enabled_u0028_();
    if !(_e89) {
        return 0f;
    }
    let _e92 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e92, 0.000001f));
    let _e97 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e97 + 0.5f));
    let _e100 = fogType_1;
    if (_e100 == 1i) {
        let _e104 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e104 <= 0f) {
            return 0f;
        }
        let _e106 = viewDepth;
        let _e109 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e106 / _e109), 0f, 1f);
    }
    let _e114 = unnamed.advancedFogColorDensity[3u];
    let _e116 = viewDepth;
    opticalDepth = (max(_e114, 0f) * _e116);
    let _e118 = fogType_1;
    if (_e118 == 2i) {
        let _e120 = opticalDepth;
        return clamp((1f - exp(-(_e120))), 0f, 1f);
    }
    let _e125 = opticalDepth;
    let _e126 = opticalDepth;
    return clamp((1f - exp(-((_e125 * _e126)))), 0f, 1f);
}

fn wired_boost_legacy_lightmap_u0028_vf3_u003b(rgb: ptr<function, vec3<f32>>) -> vec3<f32> {
    var boosted: vec3<f32>;
    var peak: f32;

    let _e89 = (*rgb);
    let _e92 = unnamed.worldLightParams[0u];
    boosted = (_e89 * _e92);
    let _e95 = boosted[0u];
    let _e97 = boosted[1u];
    let _e99 = boosted[2u];
    peak = max(_e95, max(_e97, _e99));
    let _e102 = peak;
    if (_e102 > 1f) {
        let _e104 = peak;
        let _e105 = boosted;
        boosted = (_e105 / vec3(_e104));
    }
    let _e108 = boosted;
    return _e108;
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e90 = (*c);
    (*c) = max(_e90, vec3<f32>(0f, 0f, 0f));
    let _e92 = (*c);
    cutoff = (_e92 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e94 = (*c);
    lo = (_e94 / vec3(12.92f));
    let _e97 = (*c);
    hi = pow(((_e97 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e102 = hi;
    let _e103 = lo;
    let _e104 = cutoff;
    return mix(_e102, _e103, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e104));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param: vec3<f32>;
    var param_1: vec3<f32>;

    let _e92 = (*role);
    let _e94 = (*role);
    let _e99 = unnamed.packed_indices[(_e92 / 4u)][(_e94 % 4u)];
    let _e102 = (*role);
    let _e104 = (*role);
    let _e109 = unnamed.packed_indices[(_e102 / 4u)][(_e104 % 4u)];
    let _e114 = (*uv);
    let _e115 = textureSample(wired_bindless_images[(_e99 & 4095u)], wired_bindless_samplers[((_e109 >> bitcast<u32>(12i)) & 255u)], _e114);
    c_1 = _e115;
    let _e116 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e116))) == 0i) {
        let _e121 = c_1;
        param = _e121.xyz;
        let _e123 = sRGBToLinear_u0028_vf3_u003b((&param));
        c_1[0u] = _e123.x;
        c_1[1u] = _e123.y;
        c_1[2u] = _e123.z;
    }
    let _e130 = (*slot);
    if (lightmap_slot == (_e130 + 1i)) {
        let _e133 = c_1;
        param_1 = _e133.xyz;
        let _e135 = wired_boost_legacy_lightmap_u0028_vf3_u003b((&param_1));
        c_1[0u] = _e135.x;
        c_1[1u] = _e135.y;
        c_1[2u] = _e135.z;
    }
    let _e142 = c_1;
    return _e142;
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

    let _e129 = unnamed.packed_indices[0i][3u];
    let _e135 = unnamed.packed_indices[0i][3u];
    let _e140 = fog_tex_coord_1;
    let _e141 = textureSample(wired_bindless_images[(_e129 & 4095u)], wired_bindless_samplers[((_e135 >> bitcast<u32>(12i)) & 255u)], _e140);
    fog = _e141;
    let _e142 = frag_color0In_1;
    param_2 = _e142.xyz;
    let _e144 = sRGBToLinear_u0028_vf3_u003b((&param_2));
    let _e146 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e144.x, _e144.y, _e144.z, _e146);
    let _e151 = frag_color1In_1;
    param_3 = _e151.xyz;
    let _e153 = sRGBToLinear_u0028_vf3_u003b((&param_3));
    let _e155 = frag_color1In_1[3u];
    frag_color1_ = vec4<f32>(_e153.x, _e153.y, _e153.z, _e155);
    let _e160 = frag_color2In_1;
    param_4 = _e160.xyz;
    let _e162 = sRGBToLinear_u0028_vf3_u003b((&param_4));
    let _e164 = frag_color2In_1[3u];
    frag_color2_ = vec4<f32>(_e162.x, _e162.y, _e162.z, _e164);
    param_5 = 0u;
    let _e169 = frag_tex_coord0_1;
    param_6 = _e169;
    param_7 = 0i;
    let _e170 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_5), (&param_6), (&param_7));
    sampledColor0_ = _e170;
    param_8 = 1u;
    let _e171 = frag_tex_coord1_1;
    param_9 = _e171;
    param_10 = 1i;
    let _e172 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_8), (&param_9), (&param_10));
    sampledColor1_ = _e172;
    param_11 = 2u;
    let _e173 = frag_tex_coord2_1;
    param_12 = _e173;
    param_13 = 2i;
    let _e174 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_11), (&param_12), (&param_13));
    sampledColor2_ = _e174;
    let _e175 = sampledColor0_;
    let _e176 = frag_color0_;
    color0_ = (_e175 * _e176);
    if override_type_4_2 {
        let _e178 = sampledColor1_;
        let _e179 = frag_color1_;
        color1_ = (_e178 * _e179);
        let _e181 = sampledColor2_;
        let _e182 = frag_color2_;
        color2_ = (_e181 * _e182);
        let _e184 = color0_;
        let _e186 = color1_;
        let _e189 = color2_;
        let _e191 = ((_e184.xyz + _e186.xyz) + _e189.xyz);
        let _e193 = color0_[3u];
        let _e195 = color1_[3u];
        let _e198 = color2_[3u];
        base = vec4<f32>(_e191.x, _e191.y, _e191.z, ((_e193 * _e195) * _e198));
    } else {
        if override_type_4_3 {
            let _e204 = sampledColor1_;
            let _e205 = frag_color1_;
            color1_1 = (_e204 * _e205);
            let _e207 = sampledColor2_;
            let _e208 = frag_color2_;
            color2_1 = (_e207 * _e208);
            let _e211 = color0_[3u];
            let _e212 = color0_;
            color0_ = (_e212 * _e211);
            let _e215 = color1_1[3u];
            let _e216 = color1_1;
            color1_1 = (_e216 * _e215);
            let _e219 = color2_1[3u];
            let _e220 = color2_1;
            color2_1 = (_e220 * _e219);
            let _e222 = color0_;
            let _e224 = color1_1;
            let _e227 = color2_1;
            let _e229 = ((_e222.xyz + _e224.xyz) + _e227.xyz);
            let _e231 = color0_[3u];
            let _e233 = color1_1[3u];
            let _e236 = color2_1[3u];
            base = vec4<f32>(_e229.x, _e229.y, _e229.z, ((_e231 * _e233) * _e236));
        } else {
            if override_type_4_4 {
                let _e242 = sampledColor1_;
                let _e243 = frag_color1_;
                color1_2 = (_e242 * _e243);
                let _e245 = sampledColor2_;
                let _e246 = frag_color2_;
                color2_2 = (_e245 * _e246);
                let _e249 = color0_[3u];
                let _e251 = color0_;
                color0_ = (_e251 * (1f - _e249));
                let _e254 = color1_2[3u];
                let _e256 = color1_2;
                color1_2 = (_e256 * (1f - _e254));
                let _e259 = color2_2[3u];
                let _e261 = color2_2;
                color2_2 = (_e261 * (1f - _e259));
                let _e263 = color0_;
                let _e265 = color1_2;
                let _e268 = color2_2;
                let _e270 = ((_e263.xyz + _e265.xyz) + _e268.xyz);
                let _e272 = color0_[3u];
                let _e274 = color1_2[3u];
                let _e277 = color2_2[3u];
                base = vec4<f32>(_e270.x, _e270.y, _e270.z, ((_e272 * _e274) * _e277));
            } else {
                if override_type_4_5 {
                    let _e283 = sampledColor1_;
                    let _e284 = frag_color1_;
                    color1_3 = (_e283 * _e284);
                    let _e286 = sampledColor2_;
                    let _e287 = frag_color2_;
                    color2_3 = (_e286 * _e287);
                    let _e289 = color0_;
                    let _e290 = color1_3;
                    let _e292 = color1_3[3u];
                    let _e295 = color2_3;
                    let _e297 = color2_3[3u];
                    base = mix(mix(_e289, _e290, vec4(_e292)), _e295, vec4(_e297));
                } else {
                    if override_type_4_6 {
                        let _e300 = sampledColor1_;
                        let _e301 = frag_color1_;
                        color1_4 = (_e300 * _e301);
                        let _e303 = sampledColor2_;
                        let _e304 = frag_color2_;
                        color2_4 = (_e303 * _e304);
                        let _e306 = color2_4;
                        let _e307 = color1_4;
                        let _e308 = color0_;
                        let _e310 = color1_4[3u];
                        let _e314 = color2_4[3u];
                        base = mix(_e306, mix(_e307, _e308, vec4(_e310)), vec4(_e314));
                    } else {
                        if override_type_4_7 {
                            let _e317 = sampledColor1_;
                            let _e318 = frag_color1_;
                            color1_5 = (_e317 * _e318);
                            let _e320 = sampledColor2_;
                            let _e321 = frag_color2_;
                            color2_5 = (_e320 * _e321);
                            let _e323 = color2_5;
                            let _e325 = color2_5[3u];
                            let _e328 = color1_5;
                            let _e330 = color1_5[3u];
                            let _e334 = color0_;
                            base = (((_e323 + vec4(_e325)) * (_e328 + vec4(_e330))) * _e334);
                        } else {
                            let _e336 = sampledColor1_;
                            let _e337 = frag_color1_;
                            color1_6 = (_e336 * _e337);
                            let _e339 = sampledColor2_;
                            let _e340 = frag_color2_;
                            color2_6 = (_e339 * _e340);
                            let _e342 = color0_;
                            let _e344 = color1_6;
                            let _e347 = color2_6;
                            let _e349 = ((_e342.xyz * _e344.xyz) * _e347.xyz);
                            base[0u] = _e349.x;
                            base[1u] = _e349.y;
                            base[2u] = _e349.z;
                            let _e357 = color0_[3u];
                            let _e359 = color1_6[3u];
                            let _e362 = color2_6[3u];
                            base[3u] = ((_e357 * _e359) * _e362);
                        }
                    }
                }
            }
        }
    }
    if override_type_4_8 {
        let _e367 = unnamed.worldLightParams[1u];
        wetness = clamp(_e367, 0f, 1f);
        let _e371 = unnamed.worldLightParams[2u];
        frost = clamp(_e371, 0f, 1f);
        let _e373 = base;
        luminance = dot(_e373.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e376 = wetness;
        let _e378 = base;
        let _e380 = (_e378.xyz * mix(1f, 0.82f, _e376));
        base[0u] = _e380.x;
        base[1u] = _e380.y;
        base[2u] = _e380.z;
        let _e387 = base;
        let _e389 = luminance;
        let _e391 = luminance;
        let _e393 = luminance;
        let _e395 = frost;
        let _e398 = mix(_e387.xyz, vec3<f32>((_e389 * 0.88f), (_e391 * 0.94f), _e393), vec3((_e395 * 0.55f)));
        base[0u] = _e398.x;
        base[1u] = _e398.y;
        base[2u] = _e398.z;
    }
    let _e407 = unnamed.materialFactors[0u];
    if (_e407 >= 0f) {
        let _e410 = unnamed.materialBaseColor;
        let _e411 = base;
        base = (_e411 * _e410);
    }
    let _e413 = color0_;
    let _e416 = unnamed.emissionRadiance;
    let _e419 = base;
    let _e421 = (_e419.xyz + (_e413.xyz * _e416.xyz));
    base[0u] = _e421.x;
    base[1u] = _e421.y;
    base[2u] = _e421.z;
    let _e428 = wired_advanced_fog_enabled_u0028_();
    if _e428 {
        let _e429 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e429;
        if override_type_4_9 {
            let _e430 = fogAmount;
            let _e432 = base;
            let _e434 = (_e432.xyz * (1f - _e430));
            base[0u] = _e434.x;
            base[1u] = _e434.y;
            base[2u] = _e434.z;
        } else {
            if override_type_4_10 {
                let _e441 = fogAmount;
                let _e443 = base;
                base = (_e443 * (1f - _e441));
            } else {
                if override_type_4_11 {
                    let _e445 = fogAmount;
                    let _e448 = base[3u];
                    base[3u] = (_e448 * (1f - _e445));
                } else {
                    let _e451 = base;
                    let _e454 = unnamed.advancedFogColorDensity;
                    let _e456 = fogAmount;
                    let _e458 = mix(_e451.xyz, _e454.xyz, vec3(_e456));
                    base[0u] = _e458.x;
                    base[1u] = _e458.y;
                    base[2u] = _e458.z;
                }
            }
        }
    } else {
        if override_type_4_12 {
            let _e465 = base;
            let _e468 = fog[3u];
            let _e470 = (_e465.xyz * (1f - _e468));
            base[0u] = _e470.x;
            base[1u] = _e470.y;
            base[2u] = _e470.z;
        } else {
            if override_type_4_13 {
                let _e477 = base;
                let _e479 = fog[3u];
                base = (_e477 * (1f - _e479));
            } else {
                if override_type_4_14 {
                    let _e483 = base[3u];
                    let _e485 = fog[3u];
                    base[3u] = (_e483 * (1f - _e485));
                } else {
                    let _e489 = base;
                    let _e490 = fog;
                    let _e492 = unnamed.fogColor;
                    let _e495 = fog[3u];
                    base = mix(_e489, (_e490 * _e492), vec4(_e495));
                }
            }
        }
    }
    if override_type_4_15 {
        let _e499 = base[3u];
        if (_e499 == 0f) {
            discard;
        }
    } else {
        if override_type_4_16 {
            let _e501 = base;
            let _e503 = base;
            if (dot(_e501.xyz, _e503.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e507 = base;
    out_color = _e507;
    param_14 = 1f;
    wiredTemporalWriteAux_u0028_f1_u003b((&param_14));
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
