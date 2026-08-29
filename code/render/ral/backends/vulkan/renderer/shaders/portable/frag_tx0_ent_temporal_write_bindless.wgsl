enable wgpu_binding_array;

struct UBO {
    eyePos: vec4<f32>,
    ent_color0_: vec4<f32>,
    ent_color1_: vec4<f32>,
    ent_color2_: vec4<f32>,
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

@id(0) override alpha_test_func: i32 = 0i;
override override_type_3_: bool = (alpha_test_func == 1i);
@id(1) override alpha_test_value: f32 = 0f;
override override_type_3_1: bool = (alpha_test_func == 2i);
override override_type_3_2: bool = (alpha_test_func == 3i);
@id(4) override tex_domain: i32 = 0i;
@id(26) override lightmap_slot: i32 = 0i;
override override_type_3_3: bool = (alpha_test_func == 1i);
override override_type_3_4: bool = (alpha_test_func == 2i);
override override_type_3_5: bool = (alpha_test_func == 3i);
override override_type_3_6: bool = (lightmap_slot != 0i);
@id(7) override discard_mode: i32 = 0i;
override override_type_3_7: bool = (discard_mode == 1i);
override override_type_3_8: bool = (discard_mode == 2i);
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
var<private> out_color: vec4<f32>;

fn wiredTemporalFinite2_u0028_vf2_u003b(value: ptr<function, vec2<f32>>) -> bool {
    var phi_79_: bool;

    let _e71 = (*value);
    let _e72 = (*value);
    let _e74 = all((_e71 == _e72));
    phi_79_ = _e74;
    if _e74 {
        let _e75 = (*value);
        phi_79_ = all((abs(_e75) <= vec2<f32>(340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f)));
    }
    let _e80 = phi_79_;
    return _e80;
}

fn wiredTemporalFinite4_u0028_vf4_u003b(value_1: ptr<function, vec4<f32>>) -> bool {
    var phi_64_: bool;

    let _e71 = (*value_1);
    let _e72 = (*value_1);
    let _e74 = all((_e71 == _e72));
    phi_64_ = _e74;
    if _e74 {
        let _e75 = (*value_1);
        phi_64_ = all((abs(_e75) <= vec4<f32>(340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f)));
    }
    let _e80 = phi_64_;
    return _e80;
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
    var phi_102_: bool;
    var phi_111_: bool;
    var phi_121_: bool;
    var phi_128_: bool;
    var phi_157_: bool;

    out_temporal_velocity = vec2<f32>(0f, 0f);
    out_temporal_validity = 0f;
    let _e81 = temporalOutcome_1;
    let _e82 = (_e81 != 1u);
    phi_102_ = _e82;
    if !(_e82) {
        let _e84 = temporalCurrentClip_1;
        param = _e84;
        let _e85 = wiredTemporalFinite4_u0028_vf4_u003b((&param));
        phi_102_ = !(_e85);
    }
    let _e88 = phi_102_;
    phi_111_ = _e88;
    if !(_e88) {
        let _e90 = temporalPreviousClip_1;
        param_1 = _e90;
        let _e91 = wiredTemporalFinite4_u0028_vf4_u003b((&param_1));
        phi_111_ = !(_e91);
    }
    let _e94 = phi_111_;
    phi_121_ = _e94;
    if !(_e94) {
        let _e97 = temporalCurrentClip_1[3u];
        phi_121_ = (_e97 <= 0.000001f);
    }
    let _e100 = phi_121_;
    phi_128_ = _e100;
    if !(_e100) {
        let _e103 = temporalPreviousClip_1[3u];
        phi_128_ = (_e103 <= 0.000001f);
    }
    let _e106 = phi_128_;
    if _e106 {
        return;
    }
    let _e107 = temporalCurrentClip_1;
    let _e110 = temporalCurrentClip_1[3u];
    currentNdc = (_e107.xy / vec2(_e110));
    let _e113 = temporalPreviousClip_1;
    let _e116 = temporalPreviousClip_1[3u];
    previousNdc = (_e113.xy / vec2(_e116));
    let _e119 = currentNdc;
    param_2 = _e119;
    let _e120 = wiredTemporalFinite2_u0028_vf2_u003b((&param_2));
    let _e121 = !(_e120);
    phi_157_ = _e121;
    if !(_e121) {
        let _e123 = previousNdc;
        param_3 = _e123;
        let _e124 = wiredTemporalFinite2_u0028_vf2_u003b((&param_3));
        phi_157_ = !(_e124);
    }
    let _e127 = phi_157_;
    if _e127 {
        return;
    }
    let _e128 = currentNdc;
    currentUv = ((_e128 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e131 = previousNdc;
    previousUv = ((_e131 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e134 = currentUv;
    let _e135 = previousUv;
    velocity = (_e134 - _e135);
    let _e137 = velocity;
    param_4 = _e137;
    let _e138 = wiredTemporalFinite2_u0028_vf2_u003b((&param_4));
    if !(_e138) {
        return;
    }
    let _e140 = velocity;
    out_temporal_velocity = _e140;
    let _e141 = (*coverageConfidence);
    out_temporal_validity = clamp(_e141, 0f, 1f);
    return;
}

fn wiredTemporalAtestConfidence_u0028_f1_u003b(alpha: ptr<function, f32>) -> f32 {
    var span: f32;

    if override_type_3_ {
        span = max(max(alpha_test_value, (1f - alpha_test_value)), 0.000001f);
        let _e75 = (*alpha);
        let _e78 = span;
        return clamp((abs((_e75 - alpha_test_value)) / _e78), 0f, 1f);
    } else {
        if override_type_3_1 {
            let _e81 = (*alpha);
            return clamp(((alpha_test_value - _e81) / max(alpha_test_value, 0.000001f)), 0f, 1f);
        } else {
            if override_type_3_2 {
                let _e86 = (*alpha);
                return clamp(((_e86 - alpha_test_value) / max((1f - alpha_test_value), 0.000001f)), 0f, 1f);
            }
        }
    }
    return 1f;
}

fn wired_advanced_fog_enabled_u0028_() -> bool {
    var fogType: i32;

    let _e73 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e73 + 0.5f));
    let _e78 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e80 = fogType;
    let _e83 = fogType;
    return (((_e78 > 0.5f) && (_e80 >= 1i)) && (_e83 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e73 = wired_advanced_fog_enabled_u0028_();
    if !(_e73) {
        return 0f;
    }
    let _e76 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e76, 0.000001f));
    let _e81 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e81 + 0.5f));
    let _e84 = fogType_1;
    if (_e84 == 1i) {
        let _e88 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e88 <= 0f) {
            return 0f;
        }
        let _e90 = viewDepth;
        let _e93 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e90 / _e93), 0f, 1f);
    }
    let _e98 = unnamed.advancedFogColorDensity[3u];
    let _e100 = viewDepth;
    opticalDepth = (max(_e98, 0f) * _e100);
    let _e102 = fogType_1;
    if (_e102 == 2i) {
        let _e104 = opticalDepth;
        return clamp((1f - exp(-(_e104))), 0f, 1f);
    }
    let _e109 = opticalDepth;
    let _e110 = opticalDepth;
    return clamp((1f - exp(-((_e109 * _e110)))), 0f, 1f);
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e74 = (*c);
    (*c) = max(_e74, vec3<f32>(0f, 0f, 0f));
    let _e76 = (*c);
    cutoff = (_e76 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e78 = (*c);
    lo = (_e78 / vec3(12.92f));
    let _e81 = (*c);
    hi = pow(((_e81 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e86 = hi;
    let _e87 = lo;
    let _e88 = cutoff;
    return mix(_e86, _e87, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e88));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param_5: vec3<f32>;

    let _e75 = (*role);
    let _e77 = (*role);
    let _e82 = unnamed.packed_indices[(_e75 / 4u)][(_e77 % 4u)];
    let _e85 = (*role);
    let _e87 = (*role);
    let _e92 = unnamed.packed_indices[(_e85 / 4u)][(_e87 % 4u)];
    let _e97 = (*uv);
    let _e98 = textureSample(wired_bindless_images[(_e82 & 4095u)], wired_bindless_samplers[((_e92 >> bitcast<u32>(12i)) & 255u)], _e97);
    c_1 = _e98;
    let _e99 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e99))) == 0i) {
        let _e104 = c_1;
        param_5 = _e104.xyz;
        let _e106 = sRGBToLinear_u0028_vf3_u003b((&param_5));
        c_1[0u] = _e106.x;
        c_1[1u] = _e106.y;
        c_1[2u] = _e106.z;
    }
    let _e113 = (*slot);
    if (lightmap_slot == (_e113 + 1i)) {
        let _e118 = unnamed.worldLightParams[0u];
        let _e119 = c_1;
        let _e121 = (_e119.xyz * _e118);
        c_1[0u] = _e121.x;
        c_1[1u] = _e121.y;
        c_1[2u] = _e121.z;
    }
    let _e128 = c_1;
    return _e128;
}

fn main_1() {
    var color0_: vec4<f32>;
    var param_6: u32;
    var param_7: vec2<f32>;
    var param_8: i32;
    var base: vec4<f32>;
    var wetness: f32;
    var frost: f32;
    var luminance: f32;
    var fogAmount: f32;
    var param_9: f32;
    var param_10: f32;

    param_6 = 0u;
    let _e81 = frag_tex_coord0_1;
    param_7 = _e81;
    param_8 = 0i;
    let _e82 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_6), (&param_7), (&param_8));
    let _e84 = unnamed.ent_color0_;
    color0_ = (_e82 * _e84);
    let _e86 = color0_;
    base = _e86;
    if override_type_3_3 {
        let _e88 = color0_[3u];
        if (_e88 == alpha_test_value) {
            discard;
        }
    } else {
        if override_type_3_4 {
            let _e91 = color0_[3u];
            if (_e91 >= alpha_test_value) {
                discard;
            }
        } else {
            if override_type_3_5 {
                let _e94 = color0_[3u];
                if (_e94 < alpha_test_value) {
                    discard;
                }
            }
        }
    }
    let _e96 = color0_;
    base = _e96;
    if override_type_3_6 {
        let _e99 = unnamed.worldLightParams[1u];
        wetness = clamp(_e99, 0f, 1f);
        let _e103 = unnamed.worldLightParams[2u];
        frost = clamp(_e103, 0f, 1f);
        let _e105 = base;
        luminance = dot(_e105.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e108 = wetness;
        let _e110 = base;
        let _e112 = (_e110.xyz * mix(1f, 0.82f, _e108));
        base[0u] = _e112.x;
        base[1u] = _e112.y;
        base[2u] = _e112.z;
        let _e119 = base;
        let _e121 = luminance;
        let _e123 = luminance;
        let _e125 = luminance;
        let _e127 = frost;
        let _e130 = mix(_e119.xyz, vec3<f32>((_e121 * 0.88f), (_e123 * 0.94f), _e125), vec3((_e127 * 0.55f)));
        base[0u] = _e130.x;
        base[1u] = _e130.y;
        base[2u] = _e130.z;
    }
    let _e137 = color0_;
    let _e140 = unnamed.emissionRadiance;
    let _e143 = base;
    let _e145 = (_e143.xyz + (_e137.xyz * _e140.xyz));
    base[0u] = _e145.x;
    base[1u] = _e145.y;
    base[2u] = _e145.z;
    let _e152 = wired_advanced_fog_enabled_u0028_();
    if _e152 {
        let _e153 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e153;
        let _e154 = base;
        let _e157 = unnamed.advancedFogColorDensity;
        let _e159 = fogAmount;
        let _e161 = mix(_e154.xyz, _e157.xyz, vec3(_e159));
        base[0u] = _e161.x;
        base[1u] = _e161.y;
        base[2u] = _e161.z;
    }
    if override_type_3_7 {
        let _e169 = base[3u];
        if (_e169 == 0f) {
            discard;
        }
    } else {
        if override_type_3_8 {
            let _e171 = base;
            let _e173 = base;
            if (dot(_e171.xyz, _e173.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e177 = base;
    out_color = _e177;
    let _e179 = color0_[3u];
    param_9 = _e179;
    let _e180 = wiredTemporalAtestConfidence_u0028_f1_u003b((&param_9));
    param_10 = _e180;
    wiredTemporalWriteAux_u0028_f1_u003b((&param_10));
    return;
}

@fragment
fn main(@location(12) @interpolate(flat) temporalOutcome: u32, @location(10) temporalCurrentClip: vec4<f32>, @location(11) temporalPreviousClip: vec4<f32>, @builtin(position) gl_FragCoord: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>) -> FragmentOutput {
    temporalOutcome_1 = temporalOutcome;
    temporalCurrentClip_1 = temporalCurrentClip;
    temporalPreviousClip_1 = temporalPreviousClip;
    gl_FragCoord_1 = gl_FragCoord;
    frag_tex_coord0_1 = frag_tex_coord0_;
    main_1();
    let _e13 = out_temporal_velocity;
    let _e14 = out_temporal_validity;
    let _e15 = out_color;
    return FragmentOutput(_e13, _e14, _e15);
}
