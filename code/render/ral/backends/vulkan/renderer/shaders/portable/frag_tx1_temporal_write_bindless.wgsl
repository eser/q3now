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
@id(7) override discard_mode: i32 = 0i;
override override_type_3_2: bool = (discard_mode == 1i);
override override_type_3_3: bool = (discard_mode == 2i);
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
var<private> frag_tex_coord0_1: vec2<f32>;
var<private> frag_tex_coord1_1: vec2<f32>;
var<private> out_color: vec4<f32>;

fn wiredTemporalFinite2_u0028_vf2_u003b(value: ptr<function, vec2<f32>>) -> bool {
    var phi_75_: bool;

    let _e58 = (*value);
    let _e59 = (*value);
    let _e61 = all((_e58 == _e59));
    phi_75_ = _e61;
    if _e61 {
        let _e62 = (*value);
        phi_75_ = all((abs(_e62) <= vec2<f32>(340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f)));
    }
    let _e67 = phi_75_;
    return _e67;
}

fn wiredTemporalFinite4_u0028_vf4_u003b(value_1: ptr<function, vec4<f32>>) -> bool {
    var phi_60_: bool;

    let _e58 = (*value_1);
    let _e59 = (*value_1);
    let _e61 = all((_e58 == _e59));
    phi_60_ = _e61;
    if _e61 {
        let _e62 = (*value_1);
        phi_60_ = all((abs(_e62) <= vec4<f32>(340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f)));
    }
    let _e67 = phi_60_;
    return _e67;
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
    let _e68 = temporalOutcome_1;
    let _e69 = (_e68 != 1u);
    phi_98_ = _e69;
    if !(_e69) {
        let _e71 = temporalCurrentClip_1;
        param = _e71;
        let _e72 = wiredTemporalFinite4_u0028_vf4_u003b((&param));
        phi_98_ = !(_e72);
    }
    let _e75 = phi_98_;
    phi_107_ = _e75;
    if !(_e75) {
        let _e77 = temporalPreviousClip_1;
        param_1 = _e77;
        let _e78 = wiredTemporalFinite4_u0028_vf4_u003b((&param_1));
        phi_107_ = !(_e78);
    }
    let _e81 = phi_107_;
    phi_117_ = _e81;
    if !(_e81) {
        let _e84 = temporalCurrentClip_1[3u];
        phi_117_ = (_e84 <= 0.000001f);
    }
    let _e87 = phi_117_;
    phi_124_ = _e87;
    if !(_e87) {
        let _e90 = temporalPreviousClip_1[3u];
        phi_124_ = (_e90 <= 0.000001f);
    }
    let _e93 = phi_124_;
    if _e93 {
        return;
    }
    let _e94 = temporalCurrentClip_1;
    let _e97 = temporalCurrentClip_1[3u];
    currentNdc = (_e94.xy / vec2(_e97));
    let _e100 = temporalPreviousClip_1;
    let _e103 = temporalPreviousClip_1[3u];
    previousNdc = (_e100.xy / vec2(_e103));
    let _e106 = currentNdc;
    param_2 = _e106;
    let _e107 = wiredTemporalFinite2_u0028_vf2_u003b((&param_2));
    let _e108 = !(_e107);
    phi_153_ = _e108;
    if !(_e108) {
        let _e110 = previousNdc;
        param_3 = _e110;
        let _e111 = wiredTemporalFinite2_u0028_vf2_u003b((&param_3));
        phi_153_ = !(_e111);
    }
    let _e114 = phi_153_;
    if _e114 {
        return;
    }
    let _e115 = currentNdc;
    currentUv = ((_e115 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e118 = previousNdc;
    previousUv = ((_e118 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e121 = currentUv;
    let _e122 = previousUv;
    velocity = (_e121 - _e122);
    let _e124 = velocity;
    param_4 = _e124;
    let _e125 = wiredTemporalFinite2_u0028_vf2_u003b((&param_4));
    if !(_e125) {
        return;
    }
    let _e127 = velocity;
    out_temporal_velocity = _e127;
    let _e128 = (*coverageConfidence);
    out_temporal_validity = clamp(_e128, 0f, 1f);
    return;
}

fn wired_advanced_fog_enabled_u0028_() -> bool {
    var fogType: i32;

    let _e60 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e60 + 0.5f));
    let _e65 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e67 = fogType;
    let _e70 = fogType;
    return (((_e65 > 0.5f) && (_e67 >= 1i)) && (_e70 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e60 = wired_advanced_fog_enabled_u0028_();
    if !(_e60) {
        return 0f;
    }
    let _e63 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e63, 0.000001f));
    let _e68 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e68 + 0.5f));
    let _e71 = fogType_1;
    if (_e71 == 1i) {
        let _e75 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e75 <= 0f) {
            return 0f;
        }
        let _e77 = viewDepth;
        let _e80 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e77 / _e80), 0f, 1f);
    }
    let _e85 = unnamed.advancedFogColorDensity[3u];
    let _e87 = viewDepth;
    opticalDepth = (max(_e85, 0f) * _e87);
    let _e89 = fogType_1;
    if (_e89 == 2i) {
        let _e91 = opticalDepth;
        return clamp((1f - exp(-(_e91))), 0f, 1f);
    }
    let _e96 = opticalDepth;
    let _e97 = opticalDepth;
    return clamp((1f - exp(-((_e96 * _e97)))), 0f, 1f);
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e61 = (*c);
    (*c) = max(_e61, vec3<f32>(0f, 0f, 0f));
    let _e63 = (*c);
    cutoff = (_e63 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e65 = (*c);
    lo = (_e65 / vec3(12.92f));
    let _e68 = (*c);
    hi = pow(((_e68 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e73 = hi;
    let _e74 = lo;
    let _e75 = cutoff;
    return mix(_e73, _e74, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e75));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param_5: vec3<f32>;

    let _e62 = (*role);
    let _e64 = (*role);
    let _e69 = unnamed.packed_indices[(_e62 / 4u)][(_e64 % 4u)];
    let _e72 = (*role);
    let _e74 = (*role);
    let _e79 = unnamed.packed_indices[(_e72 / 4u)][(_e74 % 4u)];
    let _e84 = (*uv);
    let _e85 = textureSample(wired_bindless_images[(_e69 & 4095u)], wired_bindless_samplers[((_e79 >> bitcast<u32>(12i)) & 255u)], _e84);
    c_1 = _e85;
    let _e86 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e86))) == 0i) {
        let _e91 = c_1;
        param_5 = _e91.xyz;
        let _e93 = sRGBToLinear_u0028_vf3_u003b((&param_5));
        c_1[0u] = _e93.x;
        c_1[1u] = _e93.y;
        c_1[2u] = _e93.z;
    }
    let _e100 = (*slot);
    if (lightmap_slot == (_e100 + 1i)) {
        let _e105 = unnamed.worldLightParams[0u];
        let _e106 = c_1;
        let _e108 = (_e106.xyz * _e105);
        c_1[0u] = _e108.x;
        c_1[1u] = _e108.y;
        c_1[2u] = _e108.z;
    }
    let _e115 = c_1;
    return _e115;
}

fn main_1() {
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
    var base: vec4<f32>;
    var color1_1: vec4<f32>;
    var param_13: u32;
    var param_14: vec2<f32>;
    var param_15: i32;
    var color1_2: vec4<f32>;
    var param_16: u32;
    var param_17: vec2<f32>;
    var param_18: i32;
    var fogAmount: f32;
    var param_19: f32;

    let _e78 = frag_color0In_1;
    param_6 = _e78.xyz;
    let _e80 = sRGBToLinear_u0028_vf3_u003b((&param_6));
    let _e82 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e80.x, _e80.y, _e80.z, _e82);
    param_7 = 0u;
    let _e87 = frag_tex_coord0_1;
    param_8 = _e87;
    param_9 = 0i;
    let _e88 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_7), (&param_8), (&param_9));
    let _e89 = frag_color0_;
    color0_ = (_e88 * _e89);
    if override_type_3_ {
        param_10 = 1u;
        let _e91 = frag_tex_coord1_1;
        param_11 = _e91;
        param_12 = 1i;
        let _e92 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_10), (&param_11), (&param_12));
        color1_ = _e92;
        let _e93 = color0_;
        let _e95 = color1_;
        let _e97 = (_e93.xyz + _e95.xyz);
        let _e99 = color0_[3u];
        let _e101 = color1_[3u];
        base = vec4<f32>(_e97.x, _e97.y, _e97.z, (_e99 * _e101));
    } else {
        if override_type_3_1 {
            param_13 = 1u;
            let _e107 = frag_tex_coord1_1;
            param_14 = _e107;
            param_15 = 1i;
            let _e108 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_13), (&param_14), (&param_15));
            let _e109 = frag_color0_;
            color1_1 = (_e108 * _e109);
            let _e111 = color0_;
            let _e113 = color1_1;
            let _e115 = (_e111.xyz + _e113.xyz);
            let _e117 = color0_[3u];
            let _e119 = color1_1[3u];
            base = vec4<f32>(_e115.x, _e115.y, _e115.z, (_e117 * _e119));
        } else {
            param_16 = 1u;
            let _e125 = frag_tex_coord1_1;
            param_17 = _e125;
            param_18 = 1i;
            let _e126 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_16), (&param_17), (&param_18));
            color1_2 = _e126;
            let _e127 = color0_;
            let _e129 = color1_2;
            let _e131 = (_e127.xyz * _e129.xyz);
            base[0u] = _e131.x;
            base[1u] = _e131.y;
            base[2u] = _e131.z;
            let _e139 = color0_[3u];
            let _e141 = color1_2[3u];
            base[3u] = (_e139 * _e141);
        }
    }
    let _e144 = wired_advanced_fog_enabled_u0028_();
    if _e144 {
        let _e145 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e145;
        let _e146 = base;
        let _e149 = unnamed.advancedFogColorDensity;
        let _e151 = fogAmount;
        let _e153 = mix(_e146.xyz, _e149.xyz, vec3(_e151));
        base[0u] = _e153.x;
        base[1u] = _e153.y;
        base[2u] = _e153.z;
    }
    if override_type_3_2 {
        let _e161 = base[3u];
        if (_e161 == 0f) {
            discard;
        }
    } else {
        if override_type_3_3 {
            let _e163 = base;
            let _e165 = base;
            if (dot(_e163.xyz, _e165.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e169 = base;
    out_color = _e169;
    param_19 = 1f;
    wiredTemporalWriteAux_u0028_f1_u003b((&param_19));
    return;
}

@fragment
fn main(@location(12) @interpolate(flat) temporalOutcome: u32, @location(10) temporalCurrentClip: vec4<f32>, @location(11) temporalPreviousClip: vec4<f32>, @builtin(position) gl_FragCoord: vec4<f32>, @location(0) frag_color0In: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>) -> FragmentOutput {
    temporalOutcome_1 = temporalOutcome;
    temporalCurrentClip_1 = temporalCurrentClip;
    temporalPreviousClip_1 = temporalPreviousClip;
    gl_FragCoord_1 = gl_FragCoord;
    frag_color0In_1 = frag_color0In;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    main_1();
    let _e17 = out_temporal_velocity;
    let _e18 = out_temporal_validity;
    let _e19 = out_color;
    return FragmentOutput(_e17, _e18, _e19);
}
