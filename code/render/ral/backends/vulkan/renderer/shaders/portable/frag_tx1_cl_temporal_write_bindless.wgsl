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
var<private> frag_color0In_1: vec4<f32>;
var<private> frag_color1In_1: vec4<f32>;
var<private> frag_tex_coord0_1: vec2<f32>;
var<private> frag_tex_coord1_1: vec2<f32>;
var<private> out_color: vec4<f32>;

fn wiredTemporalFinite2_u0028_vf2_u003b(value: ptr<function, vec2<f32>>) -> bool {
    var phi_75_: bool;

    let _e69 = (*value);
    let _e70 = (*value);
    let _e72 = all((_e69 == _e70));
    phi_75_ = _e72;
    if _e72 {
        let _e73 = (*value);
        phi_75_ = all((abs(_e73) <= vec2<f32>(340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f)));
    }
    let _e78 = phi_75_;
    return _e78;
}

fn wiredTemporalFinite4_u0028_vf4_u003b(value_1: ptr<function, vec4<f32>>) -> bool {
    var phi_60_: bool;

    let _e69 = (*value_1);
    let _e70 = (*value_1);
    let _e72 = all((_e69 == _e70));
    phi_60_ = _e72;
    if _e72 {
        let _e73 = (*value_1);
        phi_60_ = all((abs(_e73) <= vec4<f32>(340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f)));
    }
    let _e78 = phi_60_;
    return _e78;
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
    let _e79 = temporalOutcome_1;
    let _e80 = (_e79 != 1u);
    phi_98_ = _e80;
    if !(_e80) {
        let _e82 = temporalCurrentClip_1;
        param = _e82;
        let _e83 = wiredTemporalFinite4_u0028_vf4_u003b((&param));
        phi_98_ = !(_e83);
    }
    let _e86 = phi_98_;
    phi_107_ = _e86;
    if !(_e86) {
        let _e88 = temporalPreviousClip_1;
        param_1 = _e88;
        let _e89 = wiredTemporalFinite4_u0028_vf4_u003b((&param_1));
        phi_107_ = !(_e89);
    }
    let _e92 = phi_107_;
    phi_117_ = _e92;
    if !(_e92) {
        let _e95 = temporalCurrentClip_1[3u];
        phi_117_ = (_e95 <= 0.000001f);
    }
    let _e98 = phi_117_;
    phi_124_ = _e98;
    if !(_e98) {
        let _e101 = temporalPreviousClip_1[3u];
        phi_124_ = (_e101 <= 0.000001f);
    }
    let _e104 = phi_124_;
    if _e104 {
        return;
    }
    let _e105 = temporalCurrentClip_1;
    let _e108 = temporalCurrentClip_1[3u];
    currentNdc = (_e105.xy / vec2(_e108));
    let _e111 = temporalPreviousClip_1;
    let _e114 = temporalPreviousClip_1[3u];
    previousNdc = (_e111.xy / vec2(_e114));
    let _e117 = currentNdc;
    param_2 = _e117;
    let _e118 = wiredTemporalFinite2_u0028_vf2_u003b((&param_2));
    let _e119 = !(_e118);
    phi_153_ = _e119;
    if !(_e119) {
        let _e121 = previousNdc;
        param_3 = _e121;
        let _e122 = wiredTemporalFinite2_u0028_vf2_u003b((&param_3));
        phi_153_ = !(_e122);
    }
    let _e125 = phi_153_;
    if _e125 {
        return;
    }
    let _e126 = currentNdc;
    currentUv = ((_e126 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e129 = previousNdc;
    previousUv = ((_e129 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e132 = currentUv;
    let _e133 = previousUv;
    velocity = (_e132 - _e133);
    let _e135 = velocity;
    param_4 = _e135;
    let _e136 = wiredTemporalFinite2_u0028_vf2_u003b((&param_4));
    if !(_e136) {
        return;
    }
    let _e138 = velocity;
    out_temporal_velocity = _e138;
    let _e139 = (*coverageConfidence);
    out_temporal_validity = clamp(_e139, 0f, 1f);
    return;
}

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
    var param_5: vec3<f32>;

    let _e73 = (*role);
    let _e75 = (*role);
    let _e80 = unnamed.packed_indices[(_e73 / 4u)][(_e75 % 4u)];
    let _e83 = (*role);
    let _e85 = (*role);
    let _e90 = unnamed.packed_indices[(_e83 / 4u)][(_e85 % 4u)];
    let _e95 = (*uv);
    let _e96 = textureSample(wired_bindless_images[(_e80 & 4095u)], wired_bindless_samplers[((_e90 >> bitcast<u32>(12i)) & 255u)], _e95);
    c_1 = _e96;
    let _e97 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e97))) == 0i) {
        let _e102 = c_1;
        param_5 = _e102.xyz;
        let _e104 = sRGBToLinear_u0028_vf3_u003b((&param_5));
        c_1[0u] = _e104.x;
        c_1[1u] = _e104.y;
        c_1[2u] = _e104.z;
    }
    let _e111 = (*slot);
    if (lightmap_slot == (_e111 + 1i)) {
        let _e116 = unnamed.worldLightParams[0u];
        let _e117 = c_1;
        let _e119 = (_e117.xyz * _e116);
        c_1[0u] = _e119.x;
        c_1[1u] = _e119.y;
        c_1[2u] = _e119.z;
    }
    let _e126 = c_1;
    return _e126;
}

fn main_1() {
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

    let _e107 = frag_color0In_1;
    param_6 = _e107.xyz;
    let _e109 = sRGBToLinear_u0028_vf3_u003b((&param_6));
    let _e111 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e109.x, _e109.y, _e109.z, _e111);
    let _e116 = frag_color1In_1;
    param_7 = _e116.xyz;
    let _e118 = sRGBToLinear_u0028_vf3_u003b((&param_7));
    let _e120 = frag_color1In_1[3u];
    frag_color1_ = vec4<f32>(_e118.x, _e118.y, _e118.z, _e120);
    param_8 = 0u;
    let _e125 = frag_tex_coord0_1;
    param_9 = _e125;
    param_10 = 0i;
    let _e126 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_8), (&param_9), (&param_10));
    let _e127 = frag_color0_;
    color0_ = (_e126 * _e127);
    if override_type_3_2 {
        param_11 = 1u;
        let _e129 = frag_tex_coord1_1;
        param_12 = _e129;
        param_13 = 1i;
        let _e130 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_11), (&param_12), (&param_13));
        let _e131 = frag_color1_;
        color1_ = (_e130 * _e131);
        let _e133 = color0_;
        let _e135 = color1_;
        let _e137 = (_e133.xyz + _e135.xyz);
        let _e139 = color0_[3u];
        let _e141 = color1_[3u];
        base = vec4<f32>(_e137.x, _e137.y, _e137.z, (_e139 * _e141));
    } else {
        if override_type_3_3 {
            param_14 = 1u;
            let _e147 = frag_tex_coord1_1;
            param_15 = _e147;
            param_16 = 1i;
            let _e148 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_14), (&param_15), (&param_16));
            let _e149 = frag_color1_;
            color1_1 = (_e148 * _e149);
            let _e152 = color0_[3u];
            let _e153 = color0_;
            color0_ = (_e153 * _e152);
            let _e156 = color1_1[3u];
            let _e157 = color1_1;
            color1_1 = (_e157 * _e156);
            let _e159 = color0_;
            let _e161 = color1_1;
            let _e163 = (_e159.xyz + _e161.xyz);
            let _e165 = color0_[3u];
            let _e167 = color1_1[3u];
            base = vec4<f32>(_e163.x, _e163.y, _e163.z, (_e165 * _e167));
        } else {
            if override_type_3_4 {
                param_17 = 1u;
                let _e173 = frag_tex_coord1_1;
                param_18 = _e173;
                param_19 = 1i;
                let _e174 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_17), (&param_18), (&param_19));
                let _e175 = frag_color1_;
                color1_2 = (_e174 * _e175);
                let _e178 = color0_[3u];
                let _e180 = color0_;
                color0_ = (_e180 * (1f - _e178));
                let _e183 = color1_2[3u];
                let _e185 = color1_2;
                color1_2 = (_e185 * (1f - _e183));
                let _e187 = color0_;
                let _e189 = color1_2;
                let _e191 = (_e187.xyz + _e189.xyz);
                let _e193 = color0_[3u];
                let _e195 = color1_2[3u];
                base = vec4<f32>(_e191.x, _e191.y, _e191.z, (_e193 * _e195));
            } else {
                if override_type_3_5 {
                    param_20 = 1u;
                    let _e201 = frag_tex_coord1_1;
                    param_21 = _e201;
                    param_22 = 1i;
                    let _e202 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_20), (&param_21), (&param_22));
                    let _e203 = frag_color1_;
                    color1_3 = (_e202 * _e203);
                    let _e205 = color0_;
                    let _e206 = color1_3;
                    let _e208 = color1_3[3u];
                    base = mix(_e205, _e206, vec4(_e208));
                } else {
                    if override_type_3_6 {
                        param_23 = 1u;
                        let _e211 = frag_tex_coord1_1;
                        param_24 = _e211;
                        param_25 = 1i;
                        let _e212 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_23), (&param_24), (&param_25));
                        let _e213 = frag_color1_;
                        color1_4 = (_e212 * _e213);
                        let _e215 = color1_4;
                        let _e216 = color0_;
                        let _e218 = color1_4[3u];
                        base = mix(_e215, _e216, vec4(_e218));
                    } else {
                        if override_type_3_7 {
                            param_26 = 1u;
                            let _e221 = frag_tex_coord1_1;
                            param_27 = _e221;
                            param_28 = 1i;
                            let _e222 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_26), (&param_27), (&param_28));
                            let _e223 = frag_color1_;
                            color1_5 = (_e222 * _e223);
                            let _e225 = color1_5;
                            let _e227 = color1_5[3u];
                            let _e230 = color0_;
                            base = ((_e225 + vec4(_e227)) * _e230);
                        } else {
                            param_29 = 1u;
                            let _e232 = frag_tex_coord1_1;
                            param_30 = _e232;
                            param_31 = 1i;
                            let _e233 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_29), (&param_30), (&param_31));
                            let _e234 = frag_color1_;
                            color1_6 = (_e233 * _e234);
                            let _e236 = color0_;
                            let _e238 = color1_6;
                            let _e240 = (_e236.xyz * _e238.xyz);
                            base[0u] = _e240.x;
                            base[1u] = _e240.y;
                            base[2u] = _e240.z;
                            let _e248 = color0_[3u];
                            let _e250 = color1_6[3u];
                            base[3u] = (_e248 * _e250);
                        }
                    }
                }
            }
        }
    }
    let _e253 = wired_advanced_fog_enabled_u0028_();
    if _e253 {
        let _e254 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e254;
        let _e255 = base;
        let _e258 = unnamed.advancedFogColorDensity;
        let _e260 = fogAmount;
        let _e262 = mix(_e255.xyz, _e258.xyz, vec3(_e260));
        base[0u] = _e262.x;
        base[1u] = _e262.y;
        base[2u] = _e262.z;
    }
    if override_type_3_8 {
        let _e270 = base[3u];
        if (_e270 == 0f) {
            discard;
        }
    } else {
        if override_type_3_9 {
            let _e272 = base;
            let _e274 = base;
            if (dot(_e272.xyz, _e274.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e278 = base;
    out_color = _e278;
    param_32 = 1f;
    wiredTemporalWriteAux_u0028_f1_u003b((&param_32));
    return;
}

@fragment
fn main(@location(12) @interpolate(flat) temporalOutcome: u32, @location(10) temporalCurrentClip: vec4<f32>, @location(11) temporalPreviousClip: vec4<f32>, @builtin(position) gl_FragCoord: vec4<f32>, @location(0) frag_color0In: vec4<f32>, @location(5) frag_color1In: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>) -> FragmentOutput {
    temporalOutcome_1 = temporalOutcome;
    temporalCurrentClip_1 = temporalCurrentClip;
    temporalPreviousClip_1 = temporalPreviousClip;
    gl_FragCoord_1 = gl_FragCoord;
    frag_color0In_1 = frag_color0In;
    frag_color1In_1 = frag_color1In;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    main_1();
    let _e19 = out_temporal_velocity;
    let _e20 = out_temporal_validity;
    let _e21 = out_color;
    return FragmentOutput(_e19, _e20, _e21);
}
