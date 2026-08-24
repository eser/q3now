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
@id(10) override acff: i32 = 0i;
override override_type_3_2: bool = (acff == 1i);
override override_type_3_3: bool = (acff == 2i);
override override_type_3_4: bool = (acff == 3i);
override override_type_3_5: bool = (acff == 1i);
override override_type_3_6: bool = (acff == 2i);
override override_type_3_7: bool = (acff == 3i);
@id(7) override discard_mode: i32 = 0i;
override override_type_3_8: bool = (discard_mode == 1i);
override override_type_3_9: bool = (discard_mode == 2i);
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
var<private> frag_tex_coord0_1: vec2<f32>;
var<private> frag_tex_coord1_1: vec2<f32>;
var<private> frag_tex_coord2_1: vec2<f32>;
var<private> out_color: vec4<f32>;

fn wiredTemporalFinite2_u0028_vf2_u003b(value: ptr<function, vec2<f32>>) -> bool {
    var phi_75_: bool;

    let _e68 = (*value);
    let _e69 = (*value);
    let _e71 = all((_e68 == _e69));
    phi_75_ = _e71;
    if _e71 {
        let _e72 = (*value);
        phi_75_ = all((abs(_e72) <= vec2<f32>(340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f)));
    }
    let _e77 = phi_75_;
    return _e77;
}

fn wiredTemporalFinite4_u0028_vf4_u003b(value_1: ptr<function, vec4<f32>>) -> bool {
    var phi_60_: bool;

    let _e68 = (*value_1);
    let _e69 = (*value_1);
    let _e71 = all((_e68 == _e69));
    phi_60_ = _e71;
    if _e71 {
        let _e72 = (*value_1);
        phi_60_ = all((abs(_e72) <= vec4<f32>(340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f)));
    }
    let _e77 = phi_60_;
    return _e77;
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
    let _e78 = temporalOutcome_1;
    let _e79 = (_e78 != 1u);
    phi_98_ = _e79;
    if !(_e79) {
        let _e81 = temporalCurrentClip_1;
        param = _e81;
        let _e82 = wiredTemporalFinite4_u0028_vf4_u003b((&param));
        phi_98_ = !(_e82);
    }
    let _e85 = phi_98_;
    phi_107_ = _e85;
    if !(_e85) {
        let _e87 = temporalPreviousClip_1;
        param_1 = _e87;
        let _e88 = wiredTemporalFinite4_u0028_vf4_u003b((&param_1));
        phi_107_ = !(_e88);
    }
    let _e91 = phi_107_;
    phi_117_ = _e91;
    if !(_e91) {
        let _e94 = temporalCurrentClip_1[3u];
        phi_117_ = (_e94 <= 0.000001f);
    }
    let _e97 = phi_117_;
    phi_124_ = _e97;
    if !(_e97) {
        let _e100 = temporalPreviousClip_1[3u];
        phi_124_ = (_e100 <= 0.000001f);
    }
    let _e103 = phi_124_;
    if _e103 {
        return;
    }
    let _e104 = temporalCurrentClip_1;
    let _e107 = temporalCurrentClip_1[3u];
    currentNdc = (_e104.xy / vec2(_e107));
    let _e110 = temporalPreviousClip_1;
    let _e113 = temporalPreviousClip_1[3u];
    previousNdc = (_e110.xy / vec2(_e113));
    let _e116 = currentNdc;
    param_2 = _e116;
    let _e117 = wiredTemporalFinite2_u0028_vf2_u003b((&param_2));
    let _e118 = !(_e117);
    phi_153_ = _e118;
    if !(_e118) {
        let _e120 = previousNdc;
        param_3 = _e120;
        let _e121 = wiredTemporalFinite2_u0028_vf2_u003b((&param_3));
        phi_153_ = !(_e121);
    }
    let _e124 = phi_153_;
    if _e124 {
        return;
    }
    let _e125 = currentNdc;
    currentUv = ((_e125 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e128 = previousNdc;
    previousUv = ((_e128 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e131 = currentUv;
    let _e132 = previousUv;
    velocity = (_e131 - _e132);
    let _e134 = velocity;
    param_4 = _e134;
    let _e135 = wiredTemporalFinite2_u0028_vf2_u003b((&param_4));
    if !(_e135) {
        return;
    }
    let _e137 = velocity;
    out_temporal_velocity = _e137;
    let _e138 = (*coverageConfidence);
    out_temporal_validity = clamp(_e138, 0f, 1f);
    return;
}

fn wired_advanced_fog_enabled_u0028_() -> bool {
    var fogType: i32;

    let _e70 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e70 + 0.5f));
    let _e75 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e77 = fogType;
    let _e80 = fogType;
    return (((_e75 > 0.5f) && (_e77 >= 1i)) && (_e80 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e70 = wired_advanced_fog_enabled_u0028_();
    if !(_e70) {
        return 0f;
    }
    let _e73 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e73, 0.000001f));
    let _e78 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e78 + 0.5f));
    let _e81 = fogType_1;
    if (_e81 == 1i) {
        let _e85 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e85 <= 0f) {
            return 0f;
        }
        let _e87 = viewDepth;
        let _e90 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e87 / _e90), 0f, 1f);
    }
    let _e95 = unnamed.advancedFogColorDensity[3u];
    let _e97 = viewDepth;
    opticalDepth = (max(_e95, 0f) * _e97);
    let _e99 = fogType_1;
    if (_e99 == 2i) {
        let _e101 = opticalDepth;
        return clamp((1f - exp(-(_e101))), 0f, 1f);
    }
    let _e106 = opticalDepth;
    let _e107 = opticalDepth;
    return clamp((1f - exp(-((_e106 * _e107)))), 0f, 1f);
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e71 = (*c);
    (*c) = max(_e71, vec3<f32>(0f, 0f, 0f));
    let _e73 = (*c);
    cutoff = (_e73 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e75 = (*c);
    lo = (_e75 / vec3(12.92f));
    let _e78 = (*c);
    hi = pow(((_e78 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e83 = hi;
    let _e84 = lo;
    let _e85 = cutoff;
    return mix(_e83, _e84, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e85));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param_5: vec3<f32>;

    let _e72 = (*role);
    let _e74 = (*role);
    let _e79 = unnamed.packed_indices[(_e72 / 4u)][(_e74 % 4u)];
    let _e82 = (*role);
    let _e84 = (*role);
    let _e89 = unnamed.packed_indices[(_e82 / 4u)][(_e84 % 4u)];
    let _e94 = (*uv);
    let _e95 = textureSample(wired_bindless_images[(_e79 & 4095u)], wired_bindless_samplers[((_e89 >> bitcast<u32>(12i)) & 255u)], _e94);
    c_1 = _e95;
    let _e96 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e96))) == 0i) {
        let _e101 = c_1;
        param_5 = _e101.xyz;
        let _e103 = sRGBToLinear_u0028_vf3_u003b((&param_5));
        c_1[0u] = _e103.x;
        c_1[1u] = _e103.y;
        c_1[2u] = _e103.z;
    }
    let _e110 = (*slot);
    if (lightmap_slot == (_e110 + 1i)) {
        let _e115 = unnamed.worldLightParams[0u];
        let _e116 = c_1;
        let _e118 = (_e116.xyz * _e115);
        c_1[0u] = _e118.x;
        c_1[1u] = _e118.y;
        c_1[2u] = _e118.z;
    }
    let _e125 = c_1;
    return _e125;
}

fn main_1() {
    var fog: vec4<f32>;
    var frag_color0_: vec4<f32>;
    var param_6: vec3<f32>;
    var color0_: vec4<f32>;
    var param_7: u32;
    var param_8: vec2<f32>;
    var param_9: i32;
    var color1_: vec4<f32>;
    var param_10: u32;
    var param_11: vec2<f32>;
    var param_12: i32;
    var color2_: vec4<f32>;
    var param_13: u32;
    var param_14: vec2<f32>;
    var param_15: i32;
    var base: vec4<f32>;
    var color1_1: vec4<f32>;
    var param_16: u32;
    var param_17: vec2<f32>;
    var param_18: i32;
    var color2_1: vec4<f32>;
    var param_19: u32;
    var param_20: vec2<f32>;
    var param_21: i32;
    var color1_2: vec4<f32>;
    var param_22: u32;
    var param_23: vec2<f32>;
    var param_24: i32;
    var color2_2: vec4<f32>;
    var param_25: u32;
    var param_26: vec2<f32>;
    var param_27: i32;
    var fogAmount: f32;
    var param_28: f32;

    let _e104 = unnamed.packed_indices[0i][3u];
    let _e110 = unnamed.packed_indices[0i][3u];
    let _e115 = fog_tex_coord_1;
    let _e116 = textureSample(wired_bindless_images[(_e104 & 4095u)], wired_bindless_samplers[((_e110 >> bitcast<u32>(12i)) & 255u)], _e115);
    fog = _e116;
    let _e117 = frag_color0In_1;
    param_6 = _e117.xyz;
    let _e119 = sRGBToLinear_u0028_vf3_u003b((&param_6));
    let _e121 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e119.x, _e119.y, _e119.z, _e121);
    param_7 = 0u;
    let _e126 = frag_tex_coord0_1;
    param_8 = _e126;
    param_9 = 0i;
    let _e127 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_7), (&param_8), (&param_9));
    let _e128 = frag_color0_;
    color0_ = (_e127 * _e128);
    if override_type_3_ {
        param_10 = 1u;
        let _e130 = frag_tex_coord1_1;
        param_11 = _e130;
        param_12 = 1i;
        let _e131 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_10), (&param_11), (&param_12));
        color1_ = _e131;
        param_13 = 2u;
        let _e132 = frag_tex_coord2_1;
        param_14 = _e132;
        param_15 = 2i;
        let _e133 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_13), (&param_14), (&param_15));
        color2_ = _e133;
        let _e134 = color0_;
        let _e136 = color1_;
        let _e139 = color2_;
        let _e141 = ((_e134.xyz + _e136.xyz) + _e139.xyz);
        let _e143 = color0_[3u];
        let _e145 = color1_[3u];
        let _e148 = color2_[3u];
        base = vec4<f32>(_e141.x, _e141.y, _e141.z, ((_e143 * _e145) * _e148));
    } else {
        if override_type_3_1 {
            param_16 = 1u;
            let _e154 = frag_tex_coord1_1;
            param_17 = _e154;
            param_18 = 1i;
            let _e155 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_16), (&param_17), (&param_18));
            let _e156 = frag_color0_;
            color1_1 = (_e155 * _e156);
            param_19 = 2u;
            let _e158 = frag_tex_coord2_1;
            param_20 = _e158;
            param_21 = 2i;
            let _e159 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_19), (&param_20), (&param_21));
            let _e160 = frag_color0_;
            color2_1 = (_e159 * _e160);
            let _e162 = color0_;
            let _e164 = color1_1;
            let _e167 = color2_1;
            let _e169 = ((_e162.xyz + _e164.xyz) + _e167.xyz);
            let _e171 = color0_[3u];
            let _e173 = color1_1[3u];
            let _e176 = color2_1[3u];
            base = vec4<f32>(_e169.x, _e169.y, _e169.z, ((_e171 * _e173) * _e176));
        } else {
            param_22 = 1u;
            let _e182 = frag_tex_coord1_1;
            param_23 = _e182;
            param_24 = 1i;
            let _e183 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_22), (&param_23), (&param_24));
            color1_2 = _e183;
            param_25 = 2u;
            let _e184 = frag_tex_coord2_1;
            param_26 = _e184;
            param_27 = 2i;
            let _e185 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_25), (&param_26), (&param_27));
            color2_2 = _e185;
            let _e186 = color0_;
            let _e188 = color1_2;
            let _e191 = color2_2;
            let _e193 = ((_e186.xyz * _e188.xyz) * _e191.xyz);
            base[0u] = _e193.x;
            base[1u] = _e193.y;
            base[2u] = _e193.z;
            let _e201 = color0_[3u];
            let _e203 = color1_2[3u];
            let _e206 = color2_2[3u];
            base[3u] = ((_e201 * _e203) * _e206);
        }
    }
    let _e209 = wired_advanced_fog_enabled_u0028_();
    if _e209 {
        let _e210 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e210;
        if override_type_3_2 {
            let _e211 = fogAmount;
            let _e213 = base;
            let _e215 = (_e213.xyz * (1f - _e211));
            base[0u] = _e215.x;
            base[1u] = _e215.y;
            base[2u] = _e215.z;
        } else {
            if override_type_3_3 {
                let _e222 = fogAmount;
                let _e224 = base;
                base = (_e224 * (1f - _e222));
            } else {
                if override_type_3_4 {
                    let _e226 = fogAmount;
                    let _e229 = base[3u];
                    base[3u] = (_e229 * (1f - _e226));
                } else {
                    let _e232 = base;
                    let _e235 = unnamed.advancedFogColorDensity;
                    let _e237 = fogAmount;
                    let _e239 = mix(_e232.xyz, _e235.xyz, vec3(_e237));
                    base[0u] = _e239.x;
                    base[1u] = _e239.y;
                    base[2u] = _e239.z;
                }
            }
        }
    } else {
        if override_type_3_5 {
            let _e246 = base;
            let _e249 = fog[3u];
            let _e251 = (_e246.xyz * (1f - _e249));
            base[0u] = _e251.x;
            base[1u] = _e251.y;
            base[2u] = _e251.z;
        } else {
            if override_type_3_6 {
                let _e258 = base;
                let _e260 = fog[3u];
                base = (_e258 * (1f - _e260));
            } else {
                if override_type_3_7 {
                    let _e264 = base[3u];
                    let _e266 = fog[3u];
                    base[3u] = (_e264 * (1f - _e266));
                } else {
                    let _e270 = base;
                    let _e271 = fog;
                    let _e273 = unnamed.fogColor;
                    let _e276 = fog[3u];
                    base = mix(_e270, (_e271 * _e273), vec4(_e276));
                }
            }
        }
    }
    if override_type_3_8 {
        let _e280 = base[3u];
        if (_e280 == 0f) {
            discard;
        }
    } else {
        if override_type_3_9 {
            let _e282 = base;
            let _e284 = base;
            if (dot(_e282.xyz, _e284.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e288 = base;
    out_color = _e288;
    param_28 = 1f;
    wiredTemporalWriteAux_u0028_f1_u003b((&param_28));
    return;
}

@fragment
fn main(@location(12) @interpolate(flat) temporalOutcome: u32, @location(10) temporalCurrentClip: vec4<f32>, @location(11) temporalPreviousClip: vec4<f32>, @builtin(position) gl_FragCoord: vec4<f32>, @location(4) fog_tex_coord: vec2<f32>, @location(0) frag_color0In: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>, @location(3) frag_tex_coord2_: vec2<f32>) -> FragmentOutput {
    temporalOutcome_1 = temporalOutcome;
    temporalCurrentClip_1 = temporalCurrentClip;
    temporalPreviousClip_1 = temporalPreviousClip;
    gl_FragCoord_1 = gl_FragCoord;
    fog_tex_coord_1 = fog_tex_coord;
    frag_color0In_1 = frag_color0In;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    frag_tex_coord2_1 = frag_tex_coord2_;
    main_1();
    let _e21 = out_temporal_velocity;
    let _e22 = out_temporal_validity;
    let _e23 = out_color;
    return FragmentOutput(_e21, _e22, _e23);
}
