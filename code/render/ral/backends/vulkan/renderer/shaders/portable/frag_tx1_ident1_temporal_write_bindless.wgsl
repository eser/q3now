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
var<private> frag_tex_coord0_1: vec2<f32>;
var<private> frag_tex_coord1_1: vec2<f32>;
var<private> out_color: vec4<f32>;

fn wiredTemporalFinite2_u0028_vf2_u003b(value: ptr<function, vec2<f32>>) -> bool {
    var phi_75_: bool;

    let _e57 = (*value);
    let _e58 = (*value);
    let _e60 = all((_e57 == _e58));
    phi_75_ = _e60;
    if _e60 {
        let _e61 = (*value);
        phi_75_ = all((abs(_e61) <= vec2<f32>(340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f)));
    }
    let _e66 = phi_75_;
    return _e66;
}

fn wiredTemporalFinite4_u0028_vf4_u003b(value_1: ptr<function, vec4<f32>>) -> bool {
    var phi_60_: bool;

    let _e57 = (*value_1);
    let _e58 = (*value_1);
    let _e60 = all((_e57 == _e58));
    phi_60_ = _e60;
    if _e60 {
        let _e61 = (*value_1);
        phi_60_ = all((abs(_e61) <= vec4<f32>(340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f)));
    }
    let _e66 = phi_60_;
    return _e66;
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
    let _e67 = temporalOutcome_1;
    let _e68 = (_e67 != 1u);
    phi_98_ = _e68;
    if !(_e68) {
        let _e70 = temporalCurrentClip_1;
        param = _e70;
        let _e71 = wiredTemporalFinite4_u0028_vf4_u003b((&param));
        phi_98_ = !(_e71);
    }
    let _e74 = phi_98_;
    phi_107_ = _e74;
    if !(_e74) {
        let _e76 = temporalPreviousClip_1;
        param_1 = _e76;
        let _e77 = wiredTemporalFinite4_u0028_vf4_u003b((&param_1));
        phi_107_ = !(_e77);
    }
    let _e80 = phi_107_;
    phi_117_ = _e80;
    if !(_e80) {
        let _e83 = temporalCurrentClip_1[3u];
        phi_117_ = (_e83 <= 0.000001f);
    }
    let _e86 = phi_117_;
    phi_124_ = _e86;
    if !(_e86) {
        let _e89 = temporalPreviousClip_1[3u];
        phi_124_ = (_e89 <= 0.000001f);
    }
    let _e92 = phi_124_;
    if _e92 {
        return;
    }
    let _e93 = temporalCurrentClip_1;
    let _e96 = temporalCurrentClip_1[3u];
    currentNdc = (_e93.xy / vec2(_e96));
    let _e99 = temporalPreviousClip_1;
    let _e102 = temporalPreviousClip_1[3u];
    previousNdc = (_e99.xy / vec2(_e102));
    let _e105 = currentNdc;
    param_2 = _e105;
    let _e106 = wiredTemporalFinite2_u0028_vf2_u003b((&param_2));
    let _e107 = !(_e106);
    phi_153_ = _e107;
    if !(_e107) {
        let _e109 = previousNdc;
        param_3 = _e109;
        let _e110 = wiredTemporalFinite2_u0028_vf2_u003b((&param_3));
        phi_153_ = !(_e110);
    }
    let _e113 = phi_153_;
    if _e113 {
        return;
    }
    let _e114 = currentNdc;
    currentUv = ((_e114 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e117 = previousNdc;
    previousUv = ((_e117 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e120 = currentUv;
    let _e121 = previousUv;
    velocity = (_e120 - _e121);
    let _e123 = velocity;
    param_4 = _e123;
    let _e124 = wiredTemporalFinite2_u0028_vf2_u003b((&param_4));
    if !(_e124) {
        return;
    }
    let _e126 = velocity;
    out_temporal_velocity = _e126;
    let _e127 = (*coverageConfidence);
    out_temporal_validity = clamp(_e127, 0f, 1f);
    return;
}

fn wired_advanced_fog_enabled_u0028_() -> bool {
    var fogType: i32;

    let _e59 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e59 + 0.5f));
    let _e64 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e66 = fogType;
    let _e69 = fogType;
    return (((_e64 > 0.5f) && (_e66 >= 1i)) && (_e69 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e59 = wired_advanced_fog_enabled_u0028_();
    if !(_e59) {
        return 0f;
    }
    let _e62 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e62, 0.000001f));
    let _e67 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e67 + 0.5f));
    let _e70 = fogType_1;
    if (_e70 == 1i) {
        let _e74 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e74 <= 0f) {
            return 0f;
        }
        let _e76 = viewDepth;
        let _e79 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e76 / _e79), 0f, 1f);
    }
    let _e84 = unnamed.advancedFogColorDensity[3u];
    let _e86 = viewDepth;
    opticalDepth = (max(_e84, 0f) * _e86);
    let _e88 = fogType_1;
    if (_e88 == 2i) {
        let _e90 = opticalDepth;
        return clamp((1f - exp(-(_e90))), 0f, 1f);
    }
    let _e95 = opticalDepth;
    let _e96 = opticalDepth;
    return clamp((1f - exp(-((_e95 * _e96)))), 0f, 1f);
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e60 = (*c);
    (*c) = max(_e60, vec3<f32>(0f, 0f, 0f));
    let _e62 = (*c);
    cutoff = (_e62 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e64 = (*c);
    lo = (_e64 / vec3(12.92f));
    let _e67 = (*c);
    hi = pow(((_e67 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e72 = hi;
    let _e73 = lo;
    let _e74 = cutoff;
    return mix(_e72, _e73, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e74));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param_5: vec3<f32>;

    let _e61 = (*role);
    let _e63 = (*role);
    let _e68 = unnamed.packed_indices[(_e61 / 4u)][(_e63 % 4u)];
    let _e71 = (*role);
    let _e73 = (*role);
    let _e78 = unnamed.packed_indices[(_e71 / 4u)][(_e73 % 4u)];
    let _e83 = (*uv);
    let _e84 = textureSample(wired_bindless_images[(_e68 & 4095u)], wired_bindless_samplers[((_e78 >> bitcast<u32>(12i)) & 255u)], _e83);
    c_1 = _e84;
    let _e85 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e85))) == 0i) {
        let _e90 = c_1;
        param_5 = _e90.xyz;
        let _e92 = sRGBToLinear_u0028_vf3_u003b((&param_5));
        c_1[0u] = _e92.x;
        c_1[1u] = _e92.y;
        c_1[2u] = _e92.z;
    }
    let _e99 = (*slot);
    if (lightmap_slot == (_e99 + 1i)) {
        let _e104 = unnamed.worldLightParams[0u];
        let _e105 = c_1;
        let _e107 = (_e105.xyz * _e104);
        c_1[0u] = _e107.x;
        c_1[1u] = _e107.y;
        c_1[2u] = _e107.z;
    }
    let _e114 = c_1;
    return _e114;
}

fn main_1() {
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

    param_6 = 0u;
    let _e75 = frag_tex_coord0_1;
    param_7 = _e75;
    param_8 = 0i;
    let _e76 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_6), (&param_7), (&param_8));
    color0_ = _e76;
    if override_type_3_ {
        param_9 = 1u;
        let _e77 = frag_tex_coord1_1;
        param_10 = _e77;
        param_11 = 1i;
        let _e78 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_9), (&param_10), (&param_11));
        color1_ = _e78;
        let _e79 = color0_;
        let _e81 = color1_;
        let _e83 = (_e79.xyz + _e81.xyz);
        let _e85 = color0_[3u];
        let _e87 = color1_[3u];
        base = vec4<f32>(_e83.x, _e83.y, _e83.z, (_e85 * _e87));
    } else {
        if override_type_3_1 {
            param_12 = 1u;
            let _e93 = frag_tex_coord1_1;
            param_13 = _e93;
            param_14 = 1i;
            let _e94 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_12), (&param_13), (&param_14));
            color1_1 = _e94;
            let _e95 = color0_;
            let _e97 = color1_1;
            let _e99 = (_e95.xyz + _e97.xyz);
            let _e101 = color0_[3u];
            let _e103 = color1_1[3u];
            base = vec4<f32>(_e99.x, _e99.y, _e99.z, (_e101 * _e103));
        } else {
            param_15 = 1u;
            let _e109 = frag_tex_coord1_1;
            param_16 = _e109;
            param_17 = 1i;
            let _e110 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_15), (&param_16), (&param_17));
            color1_2 = _e110;
            let _e111 = color0_;
            let _e113 = color1_2;
            let _e115 = (_e111.xyz * _e113.xyz);
            base[0u] = _e115.x;
            base[1u] = _e115.y;
            base[2u] = _e115.z;
            let _e123 = color0_[3u];
            let _e125 = color1_2[3u];
            base[3u] = (_e123 * _e125);
        }
    }
    let _e128 = wired_advanced_fog_enabled_u0028_();
    if _e128 {
        let _e129 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e129;
        let _e130 = base;
        let _e133 = unnamed.advancedFogColorDensity;
        let _e135 = fogAmount;
        let _e137 = mix(_e130.xyz, _e133.xyz, vec3(_e135));
        base[0u] = _e137.x;
        base[1u] = _e137.y;
        base[2u] = _e137.z;
    }
    if override_type_3_2 {
        let _e145 = base[3u];
        if (_e145 == 0f) {
            discard;
        }
    } else {
        if override_type_3_3 {
            let _e147 = base;
            let _e149 = base;
            if (dot(_e147.xyz, _e149.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e153 = base;
    out_color = _e153;
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
