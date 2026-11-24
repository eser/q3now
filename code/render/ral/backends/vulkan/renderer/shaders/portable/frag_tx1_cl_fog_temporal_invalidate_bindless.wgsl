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
var<private> frag_tex_coord0_1: vec2<f32>;
var<private> frag_tex_coord1_1: vec2<f32>;
var<private> out_color: vec4<f32>;
var<private> temporalCurrentClip_1: vec4<f32>;
var<private> temporalPreviousClip_1: vec4<f32>;
var<private> temporalOutcome_1: u32;

fn wiredTemporalWriteAux_u0028_f1_u003b_vf4_u003b(coverageConfidence: ptr<function, f32>, sceneColor: ptr<function, vec4<f32>>) {
    out_temporal_velocity = vec2<f32>(0f, 0f);
    out_temporal_validity = vec4<f32>(0f, 0f, 0f, 0f);
    let _e88 = (*sceneColor)[0u];
    let _e90 = (*sceneColor)[1u];
    let _e92 = (*sceneColor)[2u];
    let _e97 = (*sceneColor)[3u];
    out_temporal_validity = vec4<f32>(clamp(max(_e88, max(_e90, _e92)), 0f, 1f), 0f, 0f, clamp(_e97, 0f, 1f));
    return;
}

fn wired_advanced_fog_enabled_u0028_() -> bool {
    var fogType: i32;

    let _e88 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e88 + 0.5f));
    let _e93 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e95 = fogType;
    let _e98 = fogType;
    return (((_e93 > 0.5f) && (_e95 >= 1i)) && (_e98 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e88 = wired_advanced_fog_enabled_u0028_();
    if !(_e88) {
        return 0f;
    }
    let _e91 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e91, 0.000001f));
    let _e96 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e96 + 0.5f));
    let _e99 = fogType_1;
    if (_e99 == 1i) {
        let _e103 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e103 <= 0f) {
            return 0f;
        }
        let _e105 = viewDepth;
        let _e108 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e105 / _e108), 0f, 1f);
    }
    let _e113 = unnamed.advancedFogColorDensity[3u];
    let _e115 = viewDepth;
    opticalDepth = (max(_e113, 0f) * _e115);
    let _e117 = fogType_1;
    if (_e117 == 2i) {
        let _e119 = opticalDepth;
        return clamp((1f - exp(-(_e119))), 0f, 1f);
    }
    let _e124 = opticalDepth;
    let _e125 = opticalDepth;
    return clamp((1f - exp(-((_e124 * _e125)))), 0f, 1f);
}

fn wired_boost_legacy_lightmap_u0028_vf3_u003b(rgb: ptr<function, vec3<f32>>) -> vec3<f32> {
    var boosted: vec3<f32>;
    var peak: f32;

    let _e88 = (*rgb);
    let _e91 = unnamed.worldLightParams[0u];
    boosted = (_e88 * _e91);
    let _e94 = boosted[0u];
    let _e96 = boosted[1u];
    let _e98 = boosted[2u];
    peak = max(_e94, max(_e96, _e98));
    let _e101 = peak;
    if (_e101 > 1f) {
        let _e103 = peak;
        let _e104 = boosted;
        boosted = (_e104 / vec3(_e103));
    }
    let _e107 = boosted;
    return _e107;
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e89 = (*c);
    (*c) = max(_e89, vec3<f32>(0f, 0f, 0f));
    let _e91 = (*c);
    cutoff = (_e91 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e93 = (*c);
    lo = (_e93 / vec3(12.92f));
    let _e96 = (*c);
    hi = pow(((_e96 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e101 = hi;
    let _e102 = lo;
    let _e103 = cutoff;
    return mix(_e101, _e102, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e103));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param: vec3<f32>;
    var param_1: vec3<f32>;

    let _e91 = (*role);
    let _e93 = (*role);
    let _e98 = unnamed.packed_indices[(_e91 / 4u)][(_e93 % 4u)];
    let _e101 = (*role);
    let _e103 = (*role);
    let _e108 = unnamed.packed_indices[(_e101 / 4u)][(_e103 % 4u)];
    let _e113 = (*uv);
    let _e114 = textureSample(wired_bindless_images[(_e98 & 4095u)], wired_bindless_samplers[((_e108 >> bitcast<u32>(12i)) & 255u)], _e113);
    c_1 = _e114;
    let _e115 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e115))) == 0i) {
        let _e120 = c_1;
        param = _e120.xyz;
        let _e122 = sRGBToLinear_u0028_vf3_u003b((&param));
        c_1[0u] = _e122.x;
        c_1[1u] = _e122.y;
        c_1[2u] = _e122.z;
    }
    let _e129 = (*slot);
    if (lightmap_slot == (_e129 + 1i)) {
        let _e132 = c_1;
        param_1 = _e132.xyz;
        let _e134 = wired_boost_legacy_lightmap_u0028_vf3_u003b((&param_1));
        c_1[0u] = _e134.x;
        c_1[1u] = _e134.y;
        c_1[2u] = _e134.z;
    }
    let _e141 = c_1;
    return _e141;
}

fn main_1() {
    var fog: vec4<f32>;
    var frag_color0_: vec4<f32>;
    var param_2: vec3<f32>;
    var frag_color1_: vec4<f32>;
    var param_3: vec3<f32>;
    var sampledColor0_: vec4<f32>;
    var param_4: u32;
    var param_5: vec2<f32>;
    var param_6: i32;
    var sampledColor1_: vec4<f32>;
    var param_7: u32;
    var param_8: vec2<f32>;
    var param_9: i32;
    var color0_: vec4<f32>;
    var color1_: vec4<f32>;
    var base: vec4<f32>;
    var color1_1: vec4<f32>;
    var color1_2: vec4<f32>;
    var color1_3: vec4<f32>;
    var color1_4: vec4<f32>;
    var color1_5: vec4<f32>;
    var color1_6: vec4<f32>;
    var wetness: f32;
    var frost: f32;
    var luminance: f32;
    var fogAmount: f32;
    var param_10: f32;
    var param_11: vec4<f32>;

    let _e116 = unnamed.packed_indices[0i][3u];
    let _e122 = unnamed.packed_indices[0i][3u];
    let _e127 = fog_tex_coord_1;
    let _e128 = textureSample(wired_bindless_images[(_e116 & 4095u)], wired_bindless_samplers[((_e122 >> bitcast<u32>(12i)) & 255u)], _e127);
    fog = _e128;
    let _e129 = frag_color0In_1;
    param_2 = _e129.xyz;
    let _e131 = sRGBToLinear_u0028_vf3_u003b((&param_2));
    let _e133 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e131.x, _e131.y, _e131.z, _e133);
    let _e138 = frag_color1In_1;
    param_3 = _e138.xyz;
    let _e140 = sRGBToLinear_u0028_vf3_u003b((&param_3));
    let _e142 = frag_color1In_1[3u];
    frag_color1_ = vec4<f32>(_e140.x, _e140.y, _e140.z, _e142);
    param_4 = 0u;
    let _e147 = frag_tex_coord0_1;
    param_5 = _e147;
    param_6 = 0i;
    let _e148 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_4), (&param_5), (&param_6));
    sampledColor0_ = _e148;
    param_7 = 1u;
    let _e149 = frag_tex_coord1_1;
    param_8 = _e149;
    param_9 = 1i;
    let _e150 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_7), (&param_8), (&param_9));
    sampledColor1_ = _e150;
    let _e151 = sampledColor0_;
    let _e152 = frag_color0_;
    color0_ = (_e151 * _e152);
    if override_type_6_2 {
        let _e154 = sampledColor1_;
        let _e155 = frag_color1_;
        color1_ = (_e154 * _e155);
        let _e157 = color0_;
        let _e159 = color1_;
        let _e161 = (_e157.xyz + _e159.xyz);
        let _e163 = color0_[3u];
        let _e165 = color1_[3u];
        base = vec4<f32>(_e161.x, _e161.y, _e161.z, (_e163 * _e165));
    } else {
        if override_type_6_3 {
            let _e171 = sampledColor1_;
            let _e172 = frag_color1_;
            color1_1 = (_e171 * _e172);
            let _e175 = color0_[3u];
            let _e176 = color0_;
            color0_ = (_e176 * _e175);
            let _e179 = color1_1[3u];
            let _e180 = color1_1;
            color1_1 = (_e180 * _e179);
            let _e182 = color0_;
            let _e184 = color1_1;
            let _e186 = (_e182.xyz + _e184.xyz);
            let _e188 = color0_[3u];
            let _e190 = color1_1[3u];
            base = vec4<f32>(_e186.x, _e186.y, _e186.z, (_e188 * _e190));
        } else {
            if override_type_6_4 {
                let _e196 = sampledColor1_;
                let _e197 = frag_color1_;
                color1_2 = (_e196 * _e197);
                let _e200 = color0_[3u];
                let _e202 = color0_;
                color0_ = (_e202 * (1f - _e200));
                let _e205 = color1_2[3u];
                let _e207 = color1_2;
                color1_2 = (_e207 * (1f - _e205));
                let _e209 = color0_;
                let _e211 = color1_2;
                let _e213 = (_e209.xyz + _e211.xyz);
                let _e215 = color0_[3u];
                let _e217 = color1_2[3u];
                base = vec4<f32>(_e213.x, _e213.y, _e213.z, (_e215 * _e217));
            } else {
                if override_type_6_5 {
                    let _e223 = sampledColor1_;
                    let _e224 = frag_color1_;
                    color1_3 = (_e223 * _e224);
                    let _e226 = color0_;
                    let _e227 = color1_3;
                    let _e229 = color1_3[3u];
                    base = mix(_e226, _e227, vec4(_e229));
                } else {
                    if override_type_6_6 {
                        let _e232 = sampledColor1_;
                        let _e233 = frag_color1_;
                        color1_4 = (_e232 * _e233);
                        let _e235 = color1_4;
                        let _e236 = color0_;
                        let _e238 = color1_4[3u];
                        base = mix(_e235, _e236, vec4(_e238));
                    } else {
                        if override_type_6_7 {
                            let _e241 = sampledColor1_;
                            let _e242 = frag_color1_;
                            color1_5 = (_e241 * _e242);
                            let _e244 = color1_5;
                            let _e246 = color1_5[3u];
                            let _e249 = color0_;
                            base = ((_e244 + vec4(_e246)) * _e249);
                        } else {
                            let _e251 = sampledColor1_;
                            let _e252 = frag_color1_;
                            color1_6 = (_e251 * _e252);
                            let _e254 = color0_;
                            let _e256 = color1_6;
                            let _e258 = (_e254.xyz * _e256.xyz);
                            base[0u] = _e258.x;
                            base[1u] = _e258.y;
                            base[2u] = _e258.z;
                            let _e266 = color0_[3u];
                            let _e268 = color1_6[3u];
                            base[3u] = (_e266 * _e268);
                        }
                    }
                }
            }
        }
    }
    if override_type_6_8 {
        let _e273 = unnamed.worldLightParams[1u];
        wetness = clamp(_e273, 0f, 1f);
        let _e277 = unnamed.worldLightParams[2u];
        frost = clamp(_e277, 0f, 1f);
        let _e279 = base;
        luminance = dot(_e279.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e282 = wetness;
        let _e284 = base;
        let _e286 = (_e284.xyz * mix(1f, 0.82f, _e282));
        base[0u] = _e286.x;
        base[1u] = _e286.y;
        base[2u] = _e286.z;
        let _e293 = base;
        let _e295 = luminance;
        let _e297 = luminance;
        let _e299 = luminance;
        let _e301 = frost;
        let _e304 = mix(_e293.xyz, vec3<f32>((_e295 * 0.88f), (_e297 * 0.94f), _e299), vec3((_e301 * 0.55f)));
        base[0u] = _e304.x;
        base[1u] = _e304.y;
        base[2u] = _e304.z;
    }
    let _e313 = unnamed.materialFactors[0u];
    if (_e313 >= 0f) {
        let _e316 = unnamed.materialBaseColor;
        let _e317 = base;
        base = (_e317 * _e316);
    }
    let _e319 = color0_;
    let _e322 = unnamed.emissionRadiance;
    let _e325 = base;
    let _e327 = (_e325.xyz + (_e319.xyz * _e322.xyz));
    base[0u] = _e327.x;
    base[1u] = _e327.y;
    base[2u] = _e327.z;
    let _e334 = wired_advanced_fog_enabled_u0028_();
    if _e334 {
        let _e335 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e335;
        if override_type_6_9 {
            let _e336 = fogAmount;
            let _e338 = base;
            let _e340 = (_e338.xyz * (1f - _e336));
            base[0u] = _e340.x;
            base[1u] = _e340.y;
            base[2u] = _e340.z;
        } else {
            if override_type_6_10 {
                let _e347 = fogAmount;
                let _e349 = base;
                base = (_e349 * (1f - _e347));
            } else {
                if override_type_6_11 {
                    let _e351 = fogAmount;
                    let _e354 = base[3u];
                    base[3u] = (_e354 * (1f - _e351));
                } else {
                    let _e357 = base;
                    let _e360 = unnamed.advancedFogColorDensity;
                    let _e362 = fogAmount;
                    let _e364 = mix(_e357.xyz, _e360.xyz, vec3(_e362));
                    base[0u] = _e364.x;
                    base[1u] = _e364.y;
                    base[2u] = _e364.z;
                }
            }
        }
    } else {
        if override_type_6_12 {
            let _e371 = base;
            let _e374 = fog[3u];
            let _e376 = (_e371.xyz * (1f - _e374));
            base[0u] = _e376.x;
            base[1u] = _e376.y;
            base[2u] = _e376.z;
        } else {
            if override_type_6_13 {
                let _e383 = base;
                let _e385 = fog[3u];
                base = (_e383 * (1f - _e385));
            } else {
                if override_type_6_14 {
                    let _e389 = base[3u];
                    let _e391 = fog[3u];
                    base[3u] = (_e389 * (1f - _e391));
                } else {
                    let _e395 = base;
                    let _e396 = fog;
                    let _e398 = unnamed.fogColor;
                    let _e401 = fog[3u];
                    base = mix(_e395, (_e396 * _e398), vec4(_e401));
                }
            }
        }
    }
    if override_type_6_15 {
        let _e405 = base[3u];
        if (_e405 == 0f) {
            discard;
        }
    } else {
        if override_type_6_16 {
            let _e407 = base;
            let _e409 = base;
            if (dot(_e407.xyz, _e409.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e413 = base;
    out_color = _e413;
    param_10 = 1f;
    let _e414 = base;
    param_11 = _e414;
    wiredTemporalWriteAux_u0028_f1_u003b_vf4_u003b((&param_10), (&param_11));
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
