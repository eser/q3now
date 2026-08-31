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
var<private> frag_tex_coord0_1: vec2<f32>;
var<private> frag_tex_coord1_1: vec2<f32>;
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

    let _e85 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e85 + 0.5f));
    let _e90 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e92 = fogType;
    let _e95 = fogType;
    return (((_e90 > 0.5f) && (_e92 >= 1i)) && (_e95 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e85 = wired_advanced_fog_enabled_u0028_();
    if !(_e85) {
        return 0f;
    }
    let _e88 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e88, 0.000001f));
    let _e93 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e93 + 0.5f));
    let _e96 = fogType_1;
    if (_e96 == 1i) {
        let _e100 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e100 <= 0f) {
            return 0f;
        }
        let _e102 = viewDepth;
        let _e105 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e102 / _e105), 0f, 1f);
    }
    let _e110 = unnamed.advancedFogColorDensity[3u];
    let _e112 = viewDepth;
    opticalDepth = (max(_e110, 0f) * _e112);
    let _e114 = fogType_1;
    if (_e114 == 2i) {
        let _e116 = opticalDepth;
        return clamp((1f - exp(-(_e116))), 0f, 1f);
    }
    let _e121 = opticalDepth;
    let _e122 = opticalDepth;
    return clamp((1f - exp(-((_e121 * _e122)))), 0f, 1f);
}

fn wired_boost_legacy_lightmap_u0028_vf3_u003b(rgb: ptr<function, vec3<f32>>) -> vec3<f32> {
    var boosted: vec3<f32>;
    var peak: f32;

    let _e85 = (*rgb);
    let _e88 = unnamed.worldLightParams[0u];
    boosted = (_e85 * _e88);
    let _e91 = boosted[0u];
    let _e93 = boosted[1u];
    let _e95 = boosted[2u];
    peak = max(_e91, max(_e93, _e95));
    let _e98 = peak;
    if (_e98 > 1f) {
        let _e100 = peak;
        let _e101 = boosted;
        boosted = (_e101 / vec3(_e100));
    }
    let _e104 = boosted;
    return _e104;
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e86 = (*c);
    (*c) = max(_e86, vec3<f32>(0f, 0f, 0f));
    let _e88 = (*c);
    cutoff = (_e88 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e90 = (*c);
    lo = (_e90 / vec3(12.92f));
    let _e93 = (*c);
    hi = pow(((_e93 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e98 = hi;
    let _e99 = lo;
    let _e100 = cutoff;
    return mix(_e98, _e99, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e100));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param: vec3<f32>;
    var param_1: vec3<f32>;

    let _e88 = (*role);
    let _e90 = (*role);
    let _e95 = unnamed.packed_indices[(_e88 / 4u)][(_e90 % 4u)];
    let _e98 = (*role);
    let _e100 = (*role);
    let _e105 = unnamed.packed_indices[(_e98 / 4u)][(_e100 % 4u)];
    let _e110 = (*uv);
    let _e111 = textureSample(wired_bindless_images[(_e95 & 4095u)], wired_bindless_samplers[((_e105 >> bitcast<u32>(12i)) & 255u)], _e110);
    c_1 = _e111;
    let _e112 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e112))) == 0i) {
        let _e117 = c_1;
        param = _e117.xyz;
        let _e119 = sRGBToLinear_u0028_vf3_u003b((&param));
        c_1[0u] = _e119.x;
        c_1[1u] = _e119.y;
        c_1[2u] = _e119.z;
    }
    let _e126 = (*slot);
    if (lightmap_slot == (_e126 + 1i)) {
        let _e129 = c_1;
        param_1 = _e129.xyz;
        let _e131 = wired_boost_legacy_lightmap_u0028_vf3_u003b((&param_1));
        c_1[0u] = _e131.x;
        c_1[1u] = _e131.y;
        c_1[2u] = _e131.z;
    }
    let _e138 = c_1;
    return _e138;
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
    var param_28: f32;

    let _e128 = unnamed.packed_indices[0i][3u];
    let _e134 = unnamed.packed_indices[0i][3u];
    let _e139 = fog_tex_coord_1;
    let _e140 = textureSample(wired_bindless_images[(_e128 & 4095u)], wired_bindless_samplers[((_e134 >> bitcast<u32>(12i)) & 255u)], _e139);
    fog = _e140;
    let _e141 = frag_color0In_1;
    param_2 = _e141.xyz;
    let _e143 = sRGBToLinear_u0028_vf3_u003b((&param_2));
    let _e145 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e143.x, _e143.y, _e143.z, _e145);
    let _e150 = frag_color1In_1;
    param_3 = _e150.xyz;
    let _e152 = sRGBToLinear_u0028_vf3_u003b((&param_3));
    let _e154 = frag_color1In_1[3u];
    frag_color1_ = vec4<f32>(_e152.x, _e152.y, _e152.z, _e154);
    param_4 = 0u;
    let _e159 = frag_tex_coord0_1;
    param_5 = _e159;
    param_6 = 0i;
    let _e160 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_4), (&param_5), (&param_6));
    let _e161 = frag_color0_;
    color0_ = (_e160 * _e161);
    if override_type_4_2 {
        param_7 = 1u;
        let _e163 = frag_tex_coord1_1;
        param_8 = _e163;
        param_9 = 1i;
        let _e164 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_7), (&param_8), (&param_9));
        let _e165 = frag_color1_;
        color1_ = (_e164 * _e165);
        let _e167 = color0_;
        let _e169 = color1_;
        let _e171 = (_e167.xyz + _e169.xyz);
        let _e173 = color0_[3u];
        let _e175 = color1_[3u];
        base = vec4<f32>(_e171.x, _e171.y, _e171.z, (_e173 * _e175));
    } else {
        if override_type_4_3 {
            param_10 = 1u;
            let _e181 = frag_tex_coord1_1;
            param_11 = _e181;
            param_12 = 1i;
            let _e182 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_10), (&param_11), (&param_12));
            let _e183 = frag_color1_;
            color1_1 = (_e182 * _e183);
            let _e186 = color0_[3u];
            let _e187 = color0_;
            color0_ = (_e187 * _e186);
            let _e190 = color1_1[3u];
            let _e191 = color1_1;
            color1_1 = (_e191 * _e190);
            let _e193 = color0_;
            let _e195 = color1_1;
            let _e197 = (_e193.xyz + _e195.xyz);
            let _e199 = color0_[3u];
            let _e201 = color1_1[3u];
            base = vec4<f32>(_e197.x, _e197.y, _e197.z, (_e199 * _e201));
        } else {
            if override_type_4_4 {
                param_13 = 1u;
                let _e207 = frag_tex_coord1_1;
                param_14 = _e207;
                param_15 = 1i;
                let _e208 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_13), (&param_14), (&param_15));
                let _e209 = frag_color1_;
                color1_2 = (_e208 * _e209);
                let _e212 = color0_[3u];
                let _e214 = color0_;
                color0_ = (_e214 * (1f - _e212));
                let _e217 = color1_2[3u];
                let _e219 = color1_2;
                color1_2 = (_e219 * (1f - _e217));
                let _e221 = color0_;
                let _e223 = color1_2;
                let _e225 = (_e221.xyz + _e223.xyz);
                let _e227 = color0_[3u];
                let _e229 = color1_2[3u];
                base = vec4<f32>(_e225.x, _e225.y, _e225.z, (_e227 * _e229));
            } else {
                if override_type_4_5 {
                    param_16 = 1u;
                    let _e235 = frag_tex_coord1_1;
                    param_17 = _e235;
                    param_18 = 1i;
                    let _e236 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_16), (&param_17), (&param_18));
                    let _e237 = frag_color1_;
                    color1_3 = (_e236 * _e237);
                    let _e239 = color0_;
                    let _e240 = color1_3;
                    let _e242 = color1_3[3u];
                    base = mix(_e239, _e240, vec4(_e242));
                } else {
                    if override_type_4_6 {
                        param_19 = 1u;
                        let _e245 = frag_tex_coord1_1;
                        param_20 = _e245;
                        param_21 = 1i;
                        let _e246 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_19), (&param_20), (&param_21));
                        let _e247 = frag_color1_;
                        color1_4 = (_e246 * _e247);
                        let _e249 = color1_4;
                        let _e250 = color0_;
                        let _e252 = color1_4[3u];
                        base = mix(_e249, _e250, vec4(_e252));
                    } else {
                        if override_type_4_7 {
                            param_22 = 1u;
                            let _e255 = frag_tex_coord1_1;
                            param_23 = _e255;
                            param_24 = 1i;
                            let _e256 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_22), (&param_23), (&param_24));
                            let _e257 = frag_color1_;
                            color1_5 = (_e256 * _e257);
                            let _e259 = color1_5;
                            let _e261 = color1_5[3u];
                            let _e264 = color0_;
                            base = ((_e259 + vec4(_e261)) * _e264);
                        } else {
                            param_25 = 1u;
                            let _e266 = frag_tex_coord1_1;
                            param_26 = _e266;
                            param_27 = 1i;
                            let _e267 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_25), (&param_26), (&param_27));
                            let _e268 = frag_color1_;
                            color1_6 = (_e267 * _e268);
                            let _e270 = color0_;
                            let _e272 = color1_6;
                            let _e274 = (_e270.xyz * _e272.xyz);
                            base[0u] = _e274.x;
                            base[1u] = _e274.y;
                            base[2u] = _e274.z;
                            let _e282 = color0_[3u];
                            let _e284 = color1_6[3u];
                            base[3u] = (_e282 * _e284);
                        }
                    }
                }
            }
        }
    }
    if override_type_4_8 {
        let _e289 = unnamed.worldLightParams[1u];
        wetness = clamp(_e289, 0f, 1f);
        let _e293 = unnamed.worldLightParams[2u];
        frost = clamp(_e293, 0f, 1f);
        let _e295 = base;
        luminance = dot(_e295.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e298 = wetness;
        let _e300 = base;
        let _e302 = (_e300.xyz * mix(1f, 0.82f, _e298));
        base[0u] = _e302.x;
        base[1u] = _e302.y;
        base[2u] = _e302.z;
        let _e309 = base;
        let _e311 = luminance;
        let _e313 = luminance;
        let _e315 = luminance;
        let _e317 = frost;
        let _e320 = mix(_e309.xyz, vec3<f32>((_e311 * 0.88f), (_e313 * 0.94f), _e315), vec3((_e317 * 0.55f)));
        base[0u] = _e320.x;
        base[1u] = _e320.y;
        base[2u] = _e320.z;
    }
    let _e327 = color0_;
    let _e330 = unnamed.emissionRadiance;
    let _e333 = base;
    let _e335 = (_e333.xyz + (_e327.xyz * _e330.xyz));
    base[0u] = _e335.x;
    base[1u] = _e335.y;
    base[2u] = _e335.z;
    let _e342 = wired_advanced_fog_enabled_u0028_();
    if _e342 {
        let _e343 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e343;
        if override_type_4_9 {
            let _e344 = fogAmount;
            let _e346 = base;
            let _e348 = (_e346.xyz * (1f - _e344));
            base[0u] = _e348.x;
            base[1u] = _e348.y;
            base[2u] = _e348.z;
        } else {
            if override_type_4_10 {
                let _e355 = fogAmount;
                let _e357 = base;
                base = (_e357 * (1f - _e355));
            } else {
                if override_type_4_11 {
                    let _e359 = fogAmount;
                    let _e362 = base[3u];
                    base[3u] = (_e362 * (1f - _e359));
                } else {
                    let _e365 = base;
                    let _e368 = unnamed.advancedFogColorDensity;
                    let _e370 = fogAmount;
                    let _e372 = mix(_e365.xyz, _e368.xyz, vec3(_e370));
                    base[0u] = _e372.x;
                    base[1u] = _e372.y;
                    base[2u] = _e372.z;
                }
            }
        }
    } else {
        if override_type_4_12 {
            let _e379 = base;
            let _e382 = fog[3u];
            let _e384 = (_e379.xyz * (1f - _e382));
            base[0u] = _e384.x;
            base[1u] = _e384.y;
            base[2u] = _e384.z;
        } else {
            if override_type_4_13 {
                let _e391 = base;
                let _e393 = fog[3u];
                base = (_e391 * (1f - _e393));
            } else {
                if override_type_4_14 {
                    let _e397 = base[3u];
                    let _e399 = fog[3u];
                    base[3u] = (_e397 * (1f - _e399));
                } else {
                    let _e403 = base;
                    let _e404 = fog;
                    let _e406 = unnamed.fogColor;
                    let _e409 = fog[3u];
                    base = mix(_e403, (_e404 * _e406), vec4(_e409));
                }
            }
        }
    }
    if override_type_4_15 {
        let _e413 = base[3u];
        if (_e413 == 0f) {
            discard;
        }
    } else {
        if override_type_4_16 {
            let _e415 = base;
            let _e417 = base;
            if (dot(_e415.xyz, _e417.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e421 = base;
    out_color = _e421;
    param_28 = 1f;
    wiredTemporalWriteAux_u0028_f1_u003b((&param_28));
    return;
}

@fragment
fn main(@builtin(position) gl_FragCoord: vec4<f32>, @location(4) fog_tex_coord: vec2<f32>, @location(0) frag_color0In: vec4<f32>, @location(5) frag_color1In: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>, @location(10) temporalCurrentClip: vec4<f32>, @location(11) temporalPreviousClip: vec4<f32>, @location(12) @interpolate(flat) temporalOutcome: u32) -> FragmentOutput {
    gl_FragCoord_1 = gl_FragCoord;
    fog_tex_coord_1 = fog_tex_coord;
    frag_color0In_1 = frag_color0In;
    frag_color1In_1 = frag_color1In;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    temporalCurrentClip_1 = temporalCurrentClip;
    temporalPreviousClip_1 = temporalPreviousClip;
    temporalOutcome_1 = temporalOutcome;
    main_1();
    let _e21 = out_temporal_velocity;
    let _e22 = out_temporal_validity;
    let _e23 = out_color;
    return FragmentOutput(_e21, _e22, _e23);
}
