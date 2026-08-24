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
@id(8) override identity_color: f32 = 1f;
@id(9) override identity_alpha: f32 = 1f;
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
var<private> frag_tex_coord0_1: vec2<f32>;
var<private> frag_tex_coord1_1: vec2<f32>;
var<private> out_color: vec4<f32>;

fn wiredTemporalFinite2_u0028_vf2_u003b(value: ptr<function, vec2<f32>>) -> bool {
    var phi_75_: bool;

    let _e59 = (*value);
    let _e60 = (*value);
    let _e62 = all((_e59 == _e60));
    phi_75_ = _e62;
    if _e62 {
        let _e63 = (*value);
        phi_75_ = all((abs(_e63) <= vec2<f32>(340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f)));
    }
    let _e68 = phi_75_;
    return _e68;
}

fn wiredTemporalFinite4_u0028_vf4_u003b(value_1: ptr<function, vec4<f32>>) -> bool {
    var phi_60_: bool;

    let _e59 = (*value_1);
    let _e60 = (*value_1);
    let _e62 = all((_e59 == _e60));
    phi_60_ = _e62;
    if _e62 {
        let _e63 = (*value_1);
        phi_60_ = all((abs(_e63) <= vec4<f32>(340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f)));
    }
    let _e68 = phi_60_;
    return _e68;
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
    let _e69 = temporalOutcome_1;
    let _e70 = (_e69 != 1u);
    phi_98_ = _e70;
    if !(_e70) {
        let _e72 = temporalCurrentClip_1;
        param = _e72;
        let _e73 = wiredTemporalFinite4_u0028_vf4_u003b((&param));
        phi_98_ = !(_e73);
    }
    let _e76 = phi_98_;
    phi_107_ = _e76;
    if !(_e76) {
        let _e78 = temporalPreviousClip_1;
        param_1 = _e78;
        let _e79 = wiredTemporalFinite4_u0028_vf4_u003b((&param_1));
        phi_107_ = !(_e79);
    }
    let _e82 = phi_107_;
    phi_117_ = _e82;
    if !(_e82) {
        let _e85 = temporalCurrentClip_1[3u];
        phi_117_ = (_e85 <= 0.000001f);
    }
    let _e88 = phi_117_;
    phi_124_ = _e88;
    if !(_e88) {
        let _e91 = temporalPreviousClip_1[3u];
        phi_124_ = (_e91 <= 0.000001f);
    }
    let _e94 = phi_124_;
    if _e94 {
        return;
    }
    let _e95 = temporalCurrentClip_1;
    let _e98 = temporalCurrentClip_1[3u];
    currentNdc = (_e95.xy / vec2(_e98));
    let _e101 = temporalPreviousClip_1;
    let _e104 = temporalPreviousClip_1[3u];
    previousNdc = (_e101.xy / vec2(_e104));
    let _e107 = currentNdc;
    param_2 = _e107;
    let _e108 = wiredTemporalFinite2_u0028_vf2_u003b((&param_2));
    let _e109 = !(_e108);
    phi_153_ = _e109;
    if !(_e109) {
        let _e111 = previousNdc;
        param_3 = _e111;
        let _e112 = wiredTemporalFinite2_u0028_vf2_u003b((&param_3));
        phi_153_ = !(_e112);
    }
    let _e115 = phi_153_;
    if _e115 {
        return;
    }
    let _e116 = currentNdc;
    currentUv = ((_e116 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e119 = previousNdc;
    previousUv = ((_e119 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e122 = currentUv;
    let _e123 = previousUv;
    velocity = (_e122 - _e123);
    let _e125 = velocity;
    param_4 = _e125;
    let _e126 = wiredTemporalFinite2_u0028_vf2_u003b((&param_4));
    if !(_e126) {
        return;
    }
    let _e128 = velocity;
    out_temporal_velocity = _e128;
    let _e129 = (*coverageConfidence);
    out_temporal_validity = clamp(_e129, 0f, 1f);
    return;
}

fn wired_advanced_fog_enabled_u0028_() -> bool {
    var fogType: i32;

    let _e61 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e61 + 0.5f));
    let _e66 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e68 = fogType;
    let _e71 = fogType;
    return (((_e66 > 0.5f) && (_e68 >= 1i)) && (_e71 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e61 = wired_advanced_fog_enabled_u0028_();
    if !(_e61) {
        return 0f;
    }
    let _e64 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e64, 0.000001f));
    let _e69 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e69 + 0.5f));
    let _e72 = fogType_1;
    if (_e72 == 1i) {
        let _e76 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e76 <= 0f) {
            return 0f;
        }
        let _e78 = viewDepth;
        let _e81 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e78 / _e81), 0f, 1f);
    }
    let _e86 = unnamed.advancedFogColorDensity[3u];
    let _e88 = viewDepth;
    opticalDepth = (max(_e86, 0f) * _e88);
    let _e90 = fogType_1;
    if (_e90 == 2i) {
        let _e92 = opticalDepth;
        return clamp((1f - exp(-(_e92))), 0f, 1f);
    }
    let _e97 = opticalDepth;
    let _e98 = opticalDepth;
    return clamp((1f - exp(-((_e97 * _e98)))), 0f, 1f);
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e62 = (*c);
    (*c) = max(_e62, vec3<f32>(0f, 0f, 0f));
    let _e64 = (*c);
    cutoff = (_e64 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e66 = (*c);
    lo = (_e66 / vec3(12.92f));
    let _e69 = (*c);
    hi = pow(((_e69 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e74 = hi;
    let _e75 = lo;
    let _e76 = cutoff;
    return mix(_e74, _e75, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e76));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param_5: vec3<f32>;

    let _e63 = (*role);
    let _e65 = (*role);
    let _e70 = unnamed.packed_indices[(_e63 / 4u)][(_e65 % 4u)];
    let _e73 = (*role);
    let _e75 = (*role);
    let _e80 = unnamed.packed_indices[(_e73 / 4u)][(_e75 % 4u)];
    let _e85 = (*uv);
    let _e86 = textureSample(wired_bindless_images[(_e70 & 4095u)], wired_bindless_samplers[((_e80 >> bitcast<u32>(12i)) & 255u)], _e85);
    c_1 = _e86;
    let _e87 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e87))) == 0i) {
        let _e92 = c_1;
        param_5 = _e92.xyz;
        let _e94 = sRGBToLinear_u0028_vf3_u003b((&param_5));
        c_1[0u] = _e94.x;
        c_1[1u] = _e94.y;
        c_1[2u] = _e94.z;
    }
    let _e101 = (*slot);
    if (lightmap_slot == (_e101 + 1i)) {
        let _e106 = unnamed.worldLightParams[0u];
        let _e107 = c_1;
        let _e109 = (_e107.xyz * _e106);
        c_1[0u] = _e109.x;
        c_1[1u] = _e109.y;
        c_1[2u] = _e109.z;
    }
    let _e116 = c_1;
    return _e116;
}

fn main_1() {
    var frag_color: vec4<f32>;
    var color0_: vec4<f32>;
    var param_6: u32;
    var param_7: vec2<f32>;
    var param_8: i32;
    var color1_: vec4<f32>;
    var param_9: u32;
    var param_10: vec2<f32>;
    var param_11: i32;
    var base: vec4<f32>;
    var color1_1: vec4<f32>;
    var param_12: u32;
    var param_13: vec2<f32>;
    var param_14: i32;
    var color1_2: vec4<f32>;
    var param_15: u32;
    var param_16: vec2<f32>;
    var param_17: i32;
    var fogAmount: f32;
    var param_18: f32;

    frag_color[0u] = identity_color;
    frag_color[1u] = identity_color;
    frag_color[2u] = identity_color;
    frag_color[3u] = identity_alpha;
    param_6 = 0u;
    let _e82 = frag_tex_coord0_1;
    param_7 = _e82;
    param_8 = 0i;
    let _e83 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_6), (&param_7), (&param_8));
    let _e84 = frag_color;
    color0_ = (_e83 * _e84);
    if override_type_3_ {
        param_9 = 1u;
        let _e86 = frag_tex_coord1_1;
        param_10 = _e86;
        param_11 = 1i;
        let _e87 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_9), (&param_10), (&param_11));
        color1_ = _e87;
        let _e88 = color0_;
        let _e90 = color1_;
        let _e92 = (_e88.xyz + _e90.xyz);
        let _e94 = color0_[3u];
        let _e96 = color1_[3u];
        base = vec4<f32>(_e92.x, _e92.y, _e92.z, (_e94 * _e96));
    } else {
        if override_type_3_1 {
            param_12 = 1u;
            let _e102 = frag_tex_coord1_1;
            param_13 = _e102;
            param_14 = 1i;
            let _e103 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_12), (&param_13), (&param_14));
            let _e104 = frag_color;
            color1_1 = (_e103 * _e104);
            let _e106 = color0_;
            let _e108 = color1_1;
            let _e110 = (_e106.xyz + _e108.xyz);
            let _e112 = color0_[3u];
            let _e114 = color1_1[3u];
            base = vec4<f32>(_e110.x, _e110.y, _e110.z, (_e112 * _e114));
        } else {
            param_15 = 1u;
            let _e120 = frag_tex_coord1_1;
            param_16 = _e120;
            param_17 = 1i;
            let _e121 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_15), (&param_16), (&param_17));
            let _e122 = frag_color;
            color1_2 = (_e121 * _e122);
            let _e124 = color0_;
            let _e126 = color1_2;
            let _e128 = (_e124.xyz * _e126.xyz);
            base[0u] = _e128.x;
            base[1u] = _e128.y;
            base[2u] = _e128.z;
            let _e136 = color0_[3u];
            let _e138 = color1_2[3u];
            base[3u] = (_e136 * _e138);
        }
    }
    let _e141 = wired_advanced_fog_enabled_u0028_();
    if _e141 {
        let _e142 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e142;
        let _e143 = base;
        let _e146 = unnamed.advancedFogColorDensity;
        let _e148 = fogAmount;
        let _e150 = mix(_e143.xyz, _e146.xyz, vec3(_e148));
        base[0u] = _e150.x;
        base[1u] = _e150.y;
        base[2u] = _e150.z;
    }
    if override_type_3_2 {
        let _e158 = base[3u];
        if (_e158 == 0f) {
            discard;
        }
    } else {
        if override_type_3_3 {
            let _e160 = base;
            let _e162 = base;
            if (dot(_e160.xyz, _e162.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e166 = base;
    out_color = _e166;
    param_18 = 1f;
    wiredTemporalWriteAux_u0028_f1_u003b((&param_18));
    return;
}

@fragment
fn main(@location(12) @interpolate(flat) temporalOutcome: u32, @location(10) temporalCurrentClip: vec4<f32>, @location(11) temporalPreviousClip: vec4<f32>, @builtin(position) gl_FragCoord: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>) -> FragmentOutput {
    temporalOutcome_1 = temporalOutcome;
    temporalCurrentClip_1 = temporalCurrentClip;
    temporalPreviousClip_1 = temporalPreviousClip;
    gl_FragCoord_1 = gl_FragCoord;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    main_1();
    let _e15 = out_temporal_velocity;
    let _e16 = out_temporal_validity;
    let _e17 = out_color;
    return FragmentOutput(_e15, _e16, _e17);
}
