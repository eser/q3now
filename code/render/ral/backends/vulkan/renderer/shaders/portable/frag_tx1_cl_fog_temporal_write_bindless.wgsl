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

var<private> out_temporal_velocity: vec2<f32>;
var<private> out_temporal_validity: f32;
var<private> temporalOutcome_1: u32;
var<private> temporalCurrentClip_1: vec4<f32>;
var<private> temporalPreviousClip_1: vec4<f32>;
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

fn wiredTemporalFinite2_u0028_vf2_u003b(value: ptr<function, vec2<f32>>) -> bool {
    var phi_78_: bool;

    let _e89 = (*value);
    let _e90 = (*value);
    let _e92 = all((_e89 == _e90));
    phi_78_ = _e92;
    if _e92 {
        let _e93 = (*value);
        phi_78_ = all((abs(_e93) <= vec2<f32>(0x1.fffffep+127f, 0x1.fffffep+127f)));
    }
    let _e98 = phi_78_;
    return _e98;
}

fn wiredTemporalFinite4_u0028_vf4_u003b(value_1: ptr<function, vec4<f32>>) -> bool {
    var phi_63_: bool;

    let _e89 = (*value_1);
    let _e90 = (*value_1);
    let _e92 = all((_e89 == _e90));
    phi_63_ = _e92;
    if _e92 {
        let _e93 = (*value_1);
        phi_63_ = all((abs(_e93) <= vec4<f32>(0x1.fffffep+127f, 0x1.fffffep+127f, 0x1.fffffep+127f, 0x1.fffffep+127f)));
    }
    let _e98 = phi_63_;
    return _e98;
}

fn wiredTemporalWriteAux_u0028_f1_u003b(coverageConfidence: ptr<function, f32>) {
    var param: vec4<f32>;
    var param_1: vec4<f32>;
    var currentNdc: vec2<f32>;
    var previousNdc: vec2<f32>;
    var param_2: vec2<f32>;
    var param_3: vec2<f32>;
    var currentUv: vec2<f32>;
    var previousUv: vec2<f32>;
    var velocity: vec2<f32>;
    var param_4: vec2<f32>;
    var phi_101_: bool;
    var phi_110_: bool;
    var phi_120_: bool;
    var phi_127_: bool;
    var phi_156_: bool;

    out_temporal_velocity = vec2<f32>(0f, 0f);
    out_temporal_validity = 0f;
    let _e99 = temporalOutcome_1;
    let _e100 = (_e99 != 1u);
    phi_101_ = _e100;
    if !(_e100) {
        let _e102 = temporalCurrentClip_1;
        param = _e102;
        let _e103 = wiredTemporalFinite4_u0028_vf4_u003b((&param));
        phi_101_ = !(_e103);
    }
    let _e106 = phi_101_;
    phi_110_ = _e106;
    if !(_e106) {
        let _e108 = temporalPreviousClip_1;
        param_1 = _e108;
        let _e109 = wiredTemporalFinite4_u0028_vf4_u003b((&param_1));
        phi_110_ = !(_e109);
    }
    let _e112 = phi_110_;
    phi_120_ = _e112;
    if !(_e112) {
        let _e115 = temporalCurrentClip_1[3u];
        phi_120_ = (_e115 <= 0.000001f);
    }
    let _e118 = phi_120_;
    phi_127_ = _e118;
    if !(_e118) {
        let _e121 = temporalPreviousClip_1[3u];
        phi_127_ = (_e121 <= 0.000001f);
    }
    let _e124 = phi_127_;
    if _e124 {
        return;
    }
    let _e125 = temporalCurrentClip_1;
    let _e128 = temporalCurrentClip_1[3u];
    currentNdc = (_e125.xy / vec2(_e128));
    let _e131 = temporalPreviousClip_1;
    let _e134 = temporalPreviousClip_1[3u];
    previousNdc = (_e131.xy / vec2(_e134));
    let _e137 = currentNdc;
    param_2 = _e137;
    let _e138 = wiredTemporalFinite2_u0028_vf2_u003b((&param_2));
    let _e139 = !(_e138);
    phi_156_ = _e139;
    if !(_e139) {
        let _e141 = previousNdc;
        param_3 = _e141;
        let _e142 = wiredTemporalFinite2_u0028_vf2_u003b((&param_3));
        phi_156_ = !(_e142);
    }
    let _e145 = phi_156_;
    if _e145 {
        return;
    }
    let _e146 = currentNdc;
    currentUv = ((_e146 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e149 = previousNdc;
    previousUv = ((_e149 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e152 = currentUv;
    let _e153 = previousUv;
    velocity = (_e152 - _e153);
    let _e155 = velocity;
    param_4 = _e155;
    let _e156 = wiredTemporalFinite2_u0028_vf2_u003b((&param_4));
    if !(_e156) {
        return;
    }
    let _e158 = velocity;
    out_temporal_velocity = _e158;
    let _e159 = (*coverageConfidence);
    out_temporal_validity = clamp(_e159, 0f, 1f);
    return;
}

fn wired_advanced_fog_enabled_u0028_() -> bool {
    var fogType: i32;

    let _e91 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e91 + 0.5f));
    let _e96 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e98 = fogType;
    let _e101 = fogType;
    return (((_e96 > 0.5f) && (_e98 >= 1i)) && (_e101 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e91 = wired_advanced_fog_enabled_u0028_();
    if !(_e91) {
        return 0f;
    }
    let _e94 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e94, 0.000001f));
    let _e99 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e99 + 0.5f));
    let _e102 = fogType_1;
    if (_e102 == 1i) {
        let _e106 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e106 <= 0f) {
            return 0f;
        }
        let _e108 = viewDepth;
        let _e111 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e108 / _e111), 0f, 1f);
    }
    let _e116 = unnamed.advancedFogColorDensity[3u];
    let _e118 = viewDepth;
    opticalDepth = (max(_e116, 0f) * _e118);
    let _e120 = fogType_1;
    if (_e120 == 2i) {
        let _e122 = opticalDepth;
        return clamp((1f - exp(-(_e122))), 0f, 1f);
    }
    let _e127 = opticalDepth;
    let _e128 = opticalDepth;
    return clamp((1f - exp(-((_e127 * _e128)))), 0f, 1f);
}

fn wired_boost_legacy_lightmap_u0028_vf3_u003b(rgb: ptr<function, vec3<f32>>) -> vec3<f32> {
    var boosted: vec3<f32>;
    var peak: f32;

    let _e91 = (*rgb);
    let _e94 = unnamed.worldLightParams[0u];
    boosted = (_e91 * _e94);
    let _e97 = boosted[0u];
    let _e99 = boosted[1u];
    let _e101 = boosted[2u];
    peak = max(_e97, max(_e99, _e101));
    let _e104 = peak;
    if (_e104 > 1f) {
        let _e106 = peak;
        let _e107 = boosted;
        boosted = (_e107 / vec3(_e106));
    }
    let _e110 = boosted;
    return _e110;
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e92 = (*c);
    (*c) = max(_e92, vec3<f32>(0f, 0f, 0f));
    let _e94 = (*c);
    cutoff = (_e94 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e96 = (*c);
    lo = (_e96 / vec3(12.92f));
    let _e99 = (*c);
    hi = pow(((_e99 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e104 = hi;
    let _e105 = lo;
    let _e106 = cutoff;
    return mix(_e104, _e105, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e106));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param_5: vec3<f32>;
    var param_6: vec3<f32>;

    let _e94 = (*role);
    let _e96 = (*role);
    let _e101 = unnamed.packed_indices[(_e94 / 4u)][(_e96 % 4u)];
    let _e104 = (*role);
    let _e106 = (*role);
    let _e111 = unnamed.packed_indices[(_e104 / 4u)][(_e106 % 4u)];
    let _e116 = (*uv);
    let _e117 = textureSample(wired_bindless_images[(_e101 & 4095u)], wired_bindless_samplers[((_e111 >> bitcast<u32>(12i)) & 255u)], _e116);
    c_1 = _e117;
    let _e118 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e118))) == 0i) {
        let _e123 = c_1;
        param_5 = _e123.xyz;
        let _e125 = sRGBToLinear_u0028_vf3_u003b((&param_5));
        c_1[0u] = _e125.x;
        c_1[1u] = _e125.y;
        c_1[2u] = _e125.z;
    }
    let _e132 = (*slot);
    if (lightmap_slot == (_e132 + 1i)) {
        let _e135 = c_1;
        param_6 = _e135.xyz;
        let _e137 = wired_boost_legacy_lightmap_u0028_vf3_u003b((&param_6));
        c_1[0u] = _e137.x;
        c_1[1u] = _e137.y;
        c_1[2u] = _e137.z;
    }
    let _e144 = c_1;
    return _e144;
}

fn main_1() {
    var fog: vec4<f32>;
    var frag_color0_: vec4<f32>;
    var param_7: vec3<f32>;
    var frag_color1_: vec4<f32>;
    var param_8: vec3<f32>;
    var sampledColor0_: vec4<f32>;
    var param_9: u32;
    var param_10: vec2<f32>;
    var param_11: i32;
    var sampledColor1_: vec4<f32>;
    var param_12: u32;
    var param_13: vec2<f32>;
    var param_14: i32;
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
    var param_15: f32;

    let _e118 = unnamed.packed_indices[0i][3u];
    let _e124 = unnamed.packed_indices[0i][3u];
    let _e129 = fog_tex_coord_1;
    let _e130 = textureSample(wired_bindless_images[(_e118 & 4095u)], wired_bindless_samplers[((_e124 >> bitcast<u32>(12i)) & 255u)], _e129);
    fog = _e130;
    let _e131 = frag_color0In_1;
    param_7 = _e131.xyz;
    let _e133 = sRGBToLinear_u0028_vf3_u003b((&param_7));
    let _e135 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e133.x, _e133.y, _e133.z, _e135);
    let _e140 = frag_color1In_1;
    param_8 = _e140.xyz;
    let _e142 = sRGBToLinear_u0028_vf3_u003b((&param_8));
    let _e144 = frag_color1In_1[3u];
    frag_color1_ = vec4<f32>(_e142.x, _e142.y, _e142.z, _e144);
    param_9 = 0u;
    let _e149 = frag_tex_coord0_1;
    param_10 = _e149;
    param_11 = 0i;
    let _e150 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_9), (&param_10), (&param_11));
    sampledColor0_ = _e150;
    param_12 = 1u;
    let _e151 = frag_tex_coord1_1;
    param_13 = _e151;
    param_14 = 1i;
    let _e152 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_12), (&param_13), (&param_14));
    sampledColor1_ = _e152;
    let _e153 = sampledColor0_;
    let _e154 = frag_color0_;
    color0_ = (_e153 * _e154);
    if override_type_3_2 {
        let _e156 = sampledColor1_;
        let _e157 = frag_color1_;
        color1_ = (_e156 * _e157);
        let _e159 = color0_;
        let _e161 = color1_;
        let _e163 = (_e159.xyz + _e161.xyz);
        let _e165 = color0_[3u];
        let _e167 = color1_[3u];
        base = vec4<f32>(_e163.x, _e163.y, _e163.z, (_e165 * _e167));
    } else {
        if override_type_3_3 {
            let _e173 = sampledColor1_;
            let _e174 = frag_color1_;
            color1_1 = (_e173 * _e174);
            let _e177 = color0_[3u];
            let _e178 = color0_;
            color0_ = (_e178 * _e177);
            let _e181 = color1_1[3u];
            let _e182 = color1_1;
            color1_1 = (_e182 * _e181);
            let _e184 = color0_;
            let _e186 = color1_1;
            let _e188 = (_e184.xyz + _e186.xyz);
            let _e190 = color0_[3u];
            let _e192 = color1_1[3u];
            base = vec4<f32>(_e188.x, _e188.y, _e188.z, (_e190 * _e192));
        } else {
            if override_type_3_4 {
                let _e198 = sampledColor1_;
                let _e199 = frag_color1_;
                color1_2 = (_e198 * _e199);
                let _e202 = color0_[3u];
                let _e204 = color0_;
                color0_ = (_e204 * (1f - _e202));
                let _e207 = color1_2[3u];
                let _e209 = color1_2;
                color1_2 = (_e209 * (1f - _e207));
                let _e211 = color0_;
                let _e213 = color1_2;
                let _e215 = (_e211.xyz + _e213.xyz);
                let _e217 = color0_[3u];
                let _e219 = color1_2[3u];
                base = vec4<f32>(_e215.x, _e215.y, _e215.z, (_e217 * _e219));
            } else {
                if override_type_3_5 {
                    let _e225 = sampledColor1_;
                    let _e226 = frag_color1_;
                    color1_3 = (_e225 * _e226);
                    let _e228 = color0_;
                    let _e229 = color1_3;
                    let _e231 = color1_3[3u];
                    base = mix(_e228, _e229, vec4(_e231));
                } else {
                    if override_type_3_6 {
                        let _e234 = sampledColor1_;
                        let _e235 = frag_color1_;
                        color1_4 = (_e234 * _e235);
                        let _e237 = color1_4;
                        let _e238 = color0_;
                        let _e240 = color1_4[3u];
                        base = mix(_e237, _e238, vec4(_e240));
                    } else {
                        if override_type_3_7 {
                            let _e243 = sampledColor1_;
                            let _e244 = frag_color1_;
                            color1_5 = (_e243 * _e244);
                            let _e246 = color1_5;
                            let _e248 = color1_5[3u];
                            let _e251 = color0_;
                            base = ((_e246 + vec4(_e248)) * _e251);
                        } else {
                            let _e253 = sampledColor1_;
                            let _e254 = frag_color1_;
                            color1_6 = (_e253 * _e254);
                            let _e256 = color0_;
                            let _e258 = color1_6;
                            let _e260 = (_e256.xyz * _e258.xyz);
                            base[0u] = _e260.x;
                            base[1u] = _e260.y;
                            base[2u] = _e260.z;
                            let _e268 = color0_[3u];
                            let _e270 = color1_6[3u];
                            base[3u] = (_e268 * _e270);
                        }
                    }
                }
            }
        }
    }
    if override_type_3_8 {
        let _e275 = unnamed.worldLightParams[1u];
        wetness = clamp(_e275, 0f, 1f);
        let _e279 = unnamed.worldLightParams[2u];
        frost = clamp(_e279, 0f, 1f);
        let _e281 = base;
        luminance = dot(_e281.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e284 = wetness;
        let _e286 = base;
        let _e288 = (_e286.xyz * mix(1f, 0.82f, _e284));
        base[0u] = _e288.x;
        base[1u] = _e288.y;
        base[2u] = _e288.z;
        let _e295 = base;
        let _e297 = luminance;
        let _e299 = luminance;
        let _e301 = luminance;
        let _e303 = frost;
        let _e306 = mix(_e295.xyz, vec3<f32>((_e297 * 0.88f), (_e299 * 0.94f), _e301), vec3((_e303 * 0.55f)));
        base[0u] = _e306.x;
        base[1u] = _e306.y;
        base[2u] = _e306.z;
    }
    let _e315 = unnamed.materialFactors[0u];
    if (_e315 >= 0f) {
        let _e318 = unnamed.materialBaseColor;
        let _e319 = base;
        base = (_e319 * _e318);
    }
    let _e321 = color0_;
    let _e324 = unnamed.emissionRadiance;
    let _e327 = base;
    let _e329 = (_e327.xyz + (_e321.xyz * _e324.xyz));
    base[0u] = _e329.x;
    base[1u] = _e329.y;
    base[2u] = _e329.z;
    let _e336 = wired_advanced_fog_enabled_u0028_();
    if _e336 {
        let _e337 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e337;
        if override_type_3_9 {
            let _e338 = fogAmount;
            let _e340 = base;
            let _e342 = (_e340.xyz * (1f - _e338));
            base[0u] = _e342.x;
            base[1u] = _e342.y;
            base[2u] = _e342.z;
        } else {
            if override_type_3_10 {
                let _e349 = fogAmount;
                let _e351 = base;
                base = (_e351 * (1f - _e349));
            } else {
                if override_type_3_11 {
                    let _e353 = fogAmount;
                    let _e356 = base[3u];
                    base[3u] = (_e356 * (1f - _e353));
                } else {
                    let _e359 = base;
                    let _e362 = unnamed.advancedFogColorDensity;
                    let _e364 = fogAmount;
                    let _e366 = mix(_e359.xyz, _e362.xyz, vec3(_e364));
                    base[0u] = _e366.x;
                    base[1u] = _e366.y;
                    base[2u] = _e366.z;
                }
            }
        }
    } else {
        if override_type_3_12 {
            let _e373 = base;
            let _e376 = fog[3u];
            let _e378 = (_e373.xyz * (1f - _e376));
            base[0u] = _e378.x;
            base[1u] = _e378.y;
            base[2u] = _e378.z;
        } else {
            if override_type_3_13 {
                let _e385 = base;
                let _e387 = fog[3u];
                base = (_e385 * (1f - _e387));
            } else {
                if override_type_3_14 {
                    let _e391 = base[3u];
                    let _e393 = fog[3u];
                    base[3u] = (_e391 * (1f - _e393));
                } else {
                    let _e397 = base;
                    let _e398 = fog;
                    let _e400 = unnamed.fogColor;
                    let _e403 = fog[3u];
                    base = mix(_e397, (_e398 * _e400), vec4(_e403));
                }
            }
        }
    }
    if override_type_3_15 {
        let _e407 = base[3u];
        if (_e407 == 0f) {
            discard;
        }
    } else {
        if override_type_3_16 {
            let _e409 = base;
            let _e411 = base;
            if (dot(_e409.xyz, _e411.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e415 = base;
    out_color = _e415;
    param_15 = 1f;
    wiredTemporalWriteAux_u0028_f1_u003b((&param_15));
    return;
}

@fragment
fn main(@location(12) @interpolate(flat) temporalOutcome: u32, @location(10) temporalCurrentClip: vec4<f32>, @location(11) temporalPreviousClip: vec4<f32>, @builtin(position) gl_FragCoord: vec4<f32>, @location(4) fog_tex_coord: vec2<f32>, @location(0) frag_color0In: vec4<f32>, @location(5) frag_color1In: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>) -> FragmentOutput {
    temporalOutcome_1 = temporalOutcome;
    temporalCurrentClip_1 = temporalCurrentClip;
    temporalPreviousClip_1 = temporalPreviousClip;
    gl_FragCoord_1 = gl_FragCoord;
    fog_tex_coord_1 = fog_tex_coord;
    frag_color0In_1 = frag_color0In;
    frag_color1In_1 = frag_color1In;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    main_1();
    let _e21 = out_temporal_velocity;
    let _e22 = out_temporal_validity;
    let _e23 = out_color;
    return FragmentOutput(_e21, _e22, _e23);
}
