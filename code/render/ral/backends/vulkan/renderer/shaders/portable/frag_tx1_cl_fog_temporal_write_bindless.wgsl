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
@id(10) override acff: i32 = 0i;
override override_type_3_8: bool = (acff == 1i);
override override_type_3_9: bool = (acff == 2i);
override override_type_3_10: bool = (acff == 3i);
override override_type_3_11: bool = (acff == 1i);
override override_type_3_12: bool = (acff == 2i);
override override_type_3_13: bool = (acff == 3i);
@id(7) override discard_mode: i32 = 0i;
override override_type_3_14: bool = (discard_mode == 1i);
override override_type_3_15: bool = (discard_mode == 2i);
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
    var phi_75_: bool;

    let _e77 = (*value);
    let _e78 = (*value);
    let _e80 = all((_e77 == _e78));
    phi_75_ = _e80;
    if _e80 {
        let _e81 = (*value);
        phi_75_ = all((abs(_e81) <= vec2<f32>(340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f)));
    }
    let _e86 = phi_75_;
    return _e86;
}

fn wiredTemporalFinite4_u0028_vf4_u003b(value_1: ptr<function, vec4<f32>>) -> bool {
    var phi_60_: bool;

    let _e77 = (*value_1);
    let _e78 = (*value_1);
    let _e80 = all((_e77 == _e78));
    phi_60_ = _e80;
    if _e80 {
        let _e81 = (*value_1);
        phi_60_ = all((abs(_e81) <= vec4<f32>(340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f)));
    }
    let _e86 = phi_60_;
    return _e86;
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
    var phi_98_: bool;
    var phi_107_: bool;
    var phi_117_: bool;
    var phi_124_: bool;
    var phi_153_: bool;

    out_temporal_velocity = vec2<f32>(0f, 0f);
    out_temporal_validity = 0f;
    let _e87 = temporalOutcome_1;
    let _e88 = (_e87 != 1u);
    phi_98_ = _e88;
    if !(_e88) {
        let _e90 = temporalCurrentClip_1;
        param = _e90;
        let _e91 = wiredTemporalFinite4_u0028_vf4_u003b((&param));
        phi_98_ = !(_e91);
    }
    let _e94 = phi_98_;
    phi_107_ = _e94;
    if !(_e94) {
        let _e96 = temporalPreviousClip_1;
        param_1 = _e96;
        let _e97 = wiredTemporalFinite4_u0028_vf4_u003b((&param_1));
        phi_107_ = !(_e97);
    }
    let _e100 = phi_107_;
    phi_117_ = _e100;
    if !(_e100) {
        let _e103 = temporalCurrentClip_1[3u];
        phi_117_ = (_e103 <= 0.000001f);
    }
    let _e106 = phi_117_;
    phi_124_ = _e106;
    if !(_e106) {
        let _e109 = temporalPreviousClip_1[3u];
        phi_124_ = (_e109 <= 0.000001f);
    }
    let _e112 = phi_124_;
    if _e112 {
        return;
    }
    let _e113 = temporalCurrentClip_1;
    let _e116 = temporalCurrentClip_1[3u];
    currentNdc = (_e113.xy / vec2(_e116));
    let _e119 = temporalPreviousClip_1;
    let _e122 = temporalPreviousClip_1[3u];
    previousNdc = (_e119.xy / vec2(_e122));
    let _e125 = currentNdc;
    param_2 = _e125;
    let _e126 = wiredTemporalFinite2_u0028_vf2_u003b((&param_2));
    let _e127 = !(_e126);
    phi_153_ = _e127;
    if !(_e127) {
        let _e129 = previousNdc;
        param_3 = _e129;
        let _e130 = wiredTemporalFinite2_u0028_vf2_u003b((&param_3));
        phi_153_ = !(_e130);
    }
    let _e133 = phi_153_;
    if _e133 {
        return;
    }
    let _e134 = currentNdc;
    currentUv = ((_e134 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e137 = previousNdc;
    previousUv = ((_e137 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e140 = currentUv;
    let _e141 = previousUv;
    velocity = (_e140 - _e141);
    let _e143 = velocity;
    param_4 = _e143;
    let _e144 = wiredTemporalFinite2_u0028_vf2_u003b((&param_4));
    if !(_e144) {
        return;
    }
    let _e146 = velocity;
    out_temporal_velocity = _e146;
    let _e147 = (*coverageConfidence);
    out_temporal_validity = clamp(_e147, 0f, 1f);
    return;
}

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
    var param_5: vec3<f32>;

    let _e81 = (*role);
    let _e83 = (*role);
    let _e88 = unnamed.packed_indices[(_e81 / 4u)][(_e83 % 4u)];
    let _e91 = (*role);
    let _e93 = (*role);
    let _e98 = unnamed.packed_indices[(_e91 / 4u)][(_e93 % 4u)];
    let _e103 = (*uv);
    let _e104 = textureSample(wired_bindless_images[(_e88 & 4095u)], wired_bindless_samplers[((_e98 >> bitcast<u32>(12i)) & 255u)], _e103);
    c_1 = _e104;
    let _e105 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e105))) == 0i) {
        let _e110 = c_1;
        param_5 = _e110.xyz;
        let _e112 = sRGBToLinear_u0028_vf3_u003b((&param_5));
        c_1[0u] = _e112.x;
        c_1[1u] = _e112.y;
        c_1[2u] = _e112.z;
    }
    let _e119 = (*slot);
    if (lightmap_slot == (_e119 + 1i)) {
        let _e124 = unnamed.worldLightParams[0u];
        let _e125 = c_1;
        let _e127 = (_e125.xyz * _e124);
        c_1[0u] = _e127.x;
        c_1[1u] = _e127.y;
        c_1[2u] = _e127.z;
    }
    let _e134 = c_1;
    return _e134;
}

fn main_1() {
    var fog: vec4<f32>;
    var frag_color0_: vec4<f32>;
    var param_6: vec3<f32>;
    var frag_color1_: vec4<f32>;
    var param_7: vec3<f32>;
    var color0_: vec4<f32>;
    var param_8: u32;
    var param_9: vec2<f32>;
    var param_10: i32;
    var color1_: vec4<f32>;
    var param_11: u32;
    var param_12: vec2<f32>;
    var param_13: i32;
    var base: vec4<f32>;
    var color1_1: vec4<f32>;
    var param_14: u32;
    var param_15: vec2<f32>;
    var param_16: i32;
    var color1_2: vec4<f32>;
    var param_17: u32;
    var param_18: vec2<f32>;
    var param_19: i32;
    var color1_3: vec4<f32>;
    var param_20: u32;
    var param_21: vec2<f32>;
    var param_22: i32;
    var color1_4: vec4<f32>;
    var param_23: u32;
    var param_24: vec2<f32>;
    var param_25: i32;
    var color1_5: vec4<f32>;
    var param_26: u32;
    var param_27: vec2<f32>;
    var param_28: i32;
    var color1_6: vec4<f32>;
    var param_29: u32;
    var param_30: vec2<f32>;
    var param_31: i32;
    var fogAmount: f32;
    var param_32: f32;

    let _e119 = unnamed.packed_indices[0i][3u];
    let _e125 = unnamed.packed_indices[0i][3u];
    let _e130 = fog_tex_coord_1;
    let _e131 = textureSample(wired_bindless_images[(_e119 & 4095u)], wired_bindless_samplers[((_e125 >> bitcast<u32>(12i)) & 255u)], _e130);
    fog = _e131;
    let _e132 = frag_color0In_1;
    param_6 = _e132.xyz;
    let _e134 = sRGBToLinear_u0028_vf3_u003b((&param_6));
    let _e136 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e134.x, _e134.y, _e134.z, _e136);
    let _e141 = frag_color1In_1;
    param_7 = _e141.xyz;
    let _e143 = sRGBToLinear_u0028_vf3_u003b((&param_7));
    let _e145 = frag_color1In_1[3u];
    frag_color1_ = vec4<f32>(_e143.x, _e143.y, _e143.z, _e145);
    param_8 = 0u;
    let _e150 = frag_tex_coord0_1;
    param_9 = _e150;
    param_10 = 0i;
    let _e151 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_8), (&param_9), (&param_10));
    let _e152 = frag_color0_;
    color0_ = (_e151 * _e152);
    if override_type_3_2 {
        param_11 = 1u;
        let _e154 = frag_tex_coord1_1;
        param_12 = _e154;
        param_13 = 1i;
        let _e155 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_11), (&param_12), (&param_13));
        let _e156 = frag_color1_;
        color1_ = (_e155 * _e156);
        let _e158 = color0_;
        let _e160 = color1_;
        let _e162 = (_e158.xyz + _e160.xyz);
        let _e164 = color0_[3u];
        let _e166 = color1_[3u];
        base = vec4<f32>(_e162.x, _e162.y, _e162.z, (_e164 * _e166));
    } else {
        if override_type_3_3 {
            param_14 = 1u;
            let _e172 = frag_tex_coord1_1;
            param_15 = _e172;
            param_16 = 1i;
            let _e173 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_14), (&param_15), (&param_16));
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
                param_17 = 1u;
                let _e198 = frag_tex_coord1_1;
                param_18 = _e198;
                param_19 = 1i;
                let _e199 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_17), (&param_18), (&param_19));
                let _e200 = frag_color1_;
                color1_2 = (_e199 * _e200);
                let _e203 = color0_[3u];
                let _e205 = color0_;
                color0_ = (_e205 * (1f - _e203));
                let _e208 = color1_2[3u];
                let _e210 = color1_2;
                color1_2 = (_e210 * (1f - _e208));
                let _e212 = color0_;
                let _e214 = color1_2;
                let _e216 = (_e212.xyz + _e214.xyz);
                let _e218 = color0_[3u];
                let _e220 = color1_2[3u];
                base = vec4<f32>(_e216.x, _e216.y, _e216.z, (_e218 * _e220));
            } else {
                if override_type_3_5 {
                    param_20 = 1u;
                    let _e226 = frag_tex_coord1_1;
                    param_21 = _e226;
                    param_22 = 1i;
                    let _e227 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_20), (&param_21), (&param_22));
                    let _e228 = frag_color1_;
                    color1_3 = (_e227 * _e228);
                    let _e230 = color0_;
                    let _e231 = color1_3;
                    let _e233 = color1_3[3u];
                    base = mix(_e230, _e231, vec4(_e233));
                } else {
                    if override_type_3_6 {
                        param_23 = 1u;
                        let _e236 = frag_tex_coord1_1;
                        param_24 = _e236;
                        param_25 = 1i;
                        let _e237 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_23), (&param_24), (&param_25));
                        let _e238 = frag_color1_;
                        color1_4 = (_e237 * _e238);
                        let _e240 = color1_4;
                        let _e241 = color0_;
                        let _e243 = color1_4[3u];
                        base = mix(_e240, _e241, vec4(_e243));
                    } else {
                        if override_type_3_7 {
                            param_26 = 1u;
                            let _e246 = frag_tex_coord1_1;
                            param_27 = _e246;
                            param_28 = 1i;
                            let _e247 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_26), (&param_27), (&param_28));
                            let _e248 = frag_color1_;
                            color1_5 = (_e247 * _e248);
                            let _e250 = color1_5;
                            let _e252 = color1_5[3u];
                            let _e255 = color0_;
                            base = ((_e250 + vec4(_e252)) * _e255);
                        } else {
                            param_29 = 1u;
                            let _e257 = frag_tex_coord1_1;
                            param_30 = _e257;
                            param_31 = 1i;
                            let _e258 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_29), (&param_30), (&param_31));
                            let _e259 = frag_color1_;
                            color1_6 = (_e258 * _e259);
                            let _e261 = color0_;
                            let _e263 = color1_6;
                            let _e265 = (_e261.xyz * _e263.xyz);
                            base[0u] = _e265.x;
                            base[1u] = _e265.y;
                            base[2u] = _e265.z;
                            let _e273 = color0_[3u];
                            let _e275 = color1_6[3u];
                            base[3u] = (_e273 * _e275);
                        }
                    }
                }
            }
        }
    }
    let _e278 = wired_advanced_fog_enabled_u0028_();
    if _e278 {
        let _e279 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e279;
        if override_type_3_8 {
            let _e280 = fogAmount;
            let _e282 = base;
            let _e284 = (_e282.xyz * (1f - _e280));
            base[0u] = _e284.x;
            base[1u] = _e284.y;
            base[2u] = _e284.z;
        } else {
            if override_type_3_9 {
                let _e291 = fogAmount;
                let _e293 = base;
                base = (_e293 * (1f - _e291));
            } else {
                if override_type_3_10 {
                    let _e295 = fogAmount;
                    let _e298 = base[3u];
                    base[3u] = (_e298 * (1f - _e295));
                } else {
                    let _e301 = base;
                    let _e304 = unnamed.advancedFogColorDensity;
                    let _e306 = fogAmount;
                    let _e308 = mix(_e301.xyz, _e304.xyz, vec3(_e306));
                    base[0u] = _e308.x;
                    base[1u] = _e308.y;
                    base[2u] = _e308.z;
                }
            }
        }
    } else {
        if override_type_3_11 {
            let _e315 = base;
            let _e318 = fog[3u];
            let _e320 = (_e315.xyz * (1f - _e318));
            base[0u] = _e320.x;
            base[1u] = _e320.y;
            base[2u] = _e320.z;
        } else {
            if override_type_3_12 {
                let _e327 = base;
                let _e329 = fog[3u];
                base = (_e327 * (1f - _e329));
            } else {
                if override_type_3_13 {
                    let _e333 = base[3u];
                    let _e335 = fog[3u];
                    base[3u] = (_e333 * (1f - _e335));
                } else {
                    let _e339 = base;
                    let _e340 = fog;
                    let _e342 = unnamed.fogColor;
                    let _e345 = fog[3u];
                    base = mix(_e339, (_e340 * _e342), vec4(_e345));
                }
            }
        }
    }
    if override_type_3_14 {
        let _e349 = base[3u];
        if (_e349 == 0f) {
            discard;
        }
    } else {
        if override_type_3_15 {
            let _e351 = base;
            let _e353 = base;
            if (dot(_e351.xyz, _e353.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e357 = base;
    out_color = _e357;
    param_32 = 1f;
    wiredTemporalWriteAux_u0028_f1_u003b((&param_32));
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
