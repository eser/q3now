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
@id(7) override discard_mode: i32 = 0i;
override override_type_3_6: bool = (discard_mode == 1i);
override override_type_3_7: bool = (discard_mode == 2i);
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

    let _e61 = (*value);
    let _e62 = (*value);
    let _e64 = all((_e61 == _e62));
    phi_79_ = _e64;
    if _e64 {
        let _e65 = (*value);
        phi_79_ = all((abs(_e65) <= vec2<f32>(340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f)));
    }
    let _e70 = phi_79_;
    return _e70;
}

fn wiredTemporalFinite4_u0028_vf4_u003b(value_1: ptr<function, vec4<f32>>) -> bool {
    var phi_64_: bool;

    let _e61 = (*value_1);
    let _e62 = (*value_1);
    let _e64 = all((_e61 == _e62));
    phi_64_ = _e64;
    if _e64 {
        let _e65 = (*value_1);
        phi_64_ = all((abs(_e65) <= vec4<f32>(340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f)));
    }
    let _e70 = phi_64_;
    return _e70;
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
    let _e71 = temporalOutcome_1;
    let _e72 = (_e71 != 1u);
    phi_102_ = _e72;
    if !(_e72) {
        let _e74 = temporalCurrentClip_1;
        param = _e74;
        let _e75 = wiredTemporalFinite4_u0028_vf4_u003b((&param));
        phi_102_ = !(_e75);
    }
    let _e78 = phi_102_;
    phi_111_ = _e78;
    if !(_e78) {
        let _e80 = temporalPreviousClip_1;
        param_1 = _e80;
        let _e81 = wiredTemporalFinite4_u0028_vf4_u003b((&param_1));
        phi_111_ = !(_e81);
    }
    let _e84 = phi_111_;
    phi_121_ = _e84;
    if !(_e84) {
        let _e87 = temporalCurrentClip_1[3u];
        phi_121_ = (_e87 <= 0.000001f);
    }
    let _e90 = phi_121_;
    phi_128_ = _e90;
    if !(_e90) {
        let _e93 = temporalPreviousClip_1[3u];
        phi_128_ = (_e93 <= 0.000001f);
    }
    let _e96 = phi_128_;
    if _e96 {
        return;
    }
    let _e97 = temporalCurrentClip_1;
    let _e100 = temporalCurrentClip_1[3u];
    currentNdc = (_e97.xy / vec2(_e100));
    let _e103 = temporalPreviousClip_1;
    let _e106 = temporalPreviousClip_1[3u];
    previousNdc = (_e103.xy / vec2(_e106));
    let _e109 = currentNdc;
    param_2 = _e109;
    let _e110 = wiredTemporalFinite2_u0028_vf2_u003b((&param_2));
    let _e111 = !(_e110);
    phi_157_ = _e111;
    if !(_e111) {
        let _e113 = previousNdc;
        param_3 = _e113;
        let _e114 = wiredTemporalFinite2_u0028_vf2_u003b((&param_3));
        phi_157_ = !(_e114);
    }
    let _e117 = phi_157_;
    if _e117 {
        return;
    }
    let _e118 = currentNdc;
    currentUv = ((_e118 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e121 = previousNdc;
    previousUv = ((_e121 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e124 = currentUv;
    let _e125 = previousUv;
    velocity = (_e124 - _e125);
    let _e127 = velocity;
    param_4 = _e127;
    let _e128 = wiredTemporalFinite2_u0028_vf2_u003b((&param_4));
    if !(_e128) {
        return;
    }
    let _e130 = velocity;
    out_temporal_velocity = _e130;
    let _e131 = (*coverageConfidence);
    out_temporal_validity = clamp(_e131, 0f, 1f);
    return;
}

fn wiredTemporalAtestConfidence_u0028_f1_u003b(alpha: ptr<function, f32>) -> f32 {
    var span: f32;

    if override_type_3_ {
        span = max(max(alpha_test_value, (1f - alpha_test_value)), 0.000001f);
        let _e65 = (*alpha);
        let _e68 = span;
        return clamp((abs((_e65 - alpha_test_value)) / _e68), 0f, 1f);
    } else {
        if override_type_3_1 {
            let _e71 = (*alpha);
            return clamp(((alpha_test_value - _e71) / max(alpha_test_value, 0.000001f)), 0f, 1f);
        } else {
            if override_type_3_2 {
                let _e76 = (*alpha);
                return clamp(((_e76 - alpha_test_value) / max((1f - alpha_test_value), 0.000001f)), 0f, 1f);
            }
        }
    }
    return 1f;
}

fn wired_advanced_fog_enabled_u0028_() -> bool {
    var fogType: i32;

    let _e63 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e63 + 0.5f));
    let _e68 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e70 = fogType;
    let _e73 = fogType;
    return (((_e68 > 0.5f) && (_e70 >= 1i)) && (_e73 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e63 = wired_advanced_fog_enabled_u0028_();
    if !(_e63) {
        return 0f;
    }
    let _e66 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e66, 0.000001f));
    let _e71 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e71 + 0.5f));
    let _e74 = fogType_1;
    if (_e74 == 1i) {
        let _e78 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e78 <= 0f) {
            return 0f;
        }
        let _e80 = viewDepth;
        let _e83 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e80 / _e83), 0f, 1f);
    }
    let _e88 = unnamed.advancedFogColorDensity[3u];
    let _e90 = viewDepth;
    opticalDepth = (max(_e88, 0f) * _e90);
    let _e92 = fogType_1;
    if (_e92 == 2i) {
        let _e94 = opticalDepth;
        return clamp((1f - exp(-(_e94))), 0f, 1f);
    }
    let _e99 = opticalDepth;
    let _e100 = opticalDepth;
    return clamp((1f - exp(-((_e99 * _e100)))), 0f, 1f);
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e64 = (*c);
    (*c) = max(_e64, vec3<f32>(0f, 0f, 0f));
    let _e66 = (*c);
    cutoff = (_e66 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e68 = (*c);
    lo = (_e68 / vec3(12.92f));
    let _e71 = (*c);
    hi = pow(((_e71 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e76 = hi;
    let _e77 = lo;
    let _e78 = cutoff;
    return mix(_e76, _e77, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e78));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param_5: vec3<f32>;

    let _e65 = (*role);
    let _e67 = (*role);
    let _e72 = unnamed.packed_indices[(_e65 / 4u)][(_e67 % 4u)];
    let _e75 = (*role);
    let _e77 = (*role);
    let _e82 = unnamed.packed_indices[(_e75 / 4u)][(_e77 % 4u)];
    let _e87 = (*uv);
    let _e88 = textureSample(wired_bindless_images[(_e72 & 4095u)], wired_bindless_samplers[((_e82 >> bitcast<u32>(12i)) & 255u)], _e87);
    c_1 = _e88;
    let _e89 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e89))) == 0i) {
        let _e94 = c_1;
        param_5 = _e94.xyz;
        let _e96 = sRGBToLinear_u0028_vf3_u003b((&param_5));
        c_1[0u] = _e96.x;
        c_1[1u] = _e96.y;
        c_1[2u] = _e96.z;
    }
    let _e103 = (*slot);
    if (lightmap_slot == (_e103 + 1i)) {
        let _e108 = unnamed.worldLightParams[0u];
        let _e109 = c_1;
        let _e111 = (_e109.xyz * _e108);
        c_1[0u] = _e111.x;
        c_1[1u] = _e111.y;
        c_1[2u] = _e111.z;
    }
    let _e118 = c_1;
    return _e118;
}

fn main_1() {
    var color0_: vec4<f32>;
    var param_6: u32;
    var param_7: vec2<f32>;
    var param_8: i32;
    var base: vec4<f32>;
    var fogAmount: f32;
    var param_9: f32;
    var param_10: f32;

    param_6 = 0u;
    let _e68 = frag_tex_coord0_1;
    param_7 = _e68;
    param_8 = 0i;
    let _e69 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_6), (&param_7), (&param_8));
    let _e71 = unnamed.ent_color0_;
    color0_ = (_e69 * _e71);
    let _e73 = color0_;
    base = _e73;
    if override_type_3_3 {
        let _e75 = color0_[3u];
        if (_e75 == alpha_test_value) {
            discard;
        }
    } else {
        if override_type_3_4 {
            let _e78 = color0_[3u];
            if (_e78 >= alpha_test_value) {
                discard;
            }
        } else {
            if override_type_3_5 {
                let _e81 = color0_[3u];
                if (_e81 < alpha_test_value) {
                    discard;
                }
            }
        }
    }
    let _e83 = color0_;
    base = _e83;
    let _e84 = wired_advanced_fog_enabled_u0028_();
    if _e84 {
        let _e85 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e85;
        let _e86 = base;
        let _e89 = unnamed.advancedFogColorDensity;
        let _e91 = fogAmount;
        let _e93 = mix(_e86.xyz, _e89.xyz, vec3(_e91));
        base[0u] = _e93.x;
        base[1u] = _e93.y;
        base[2u] = _e93.z;
    }
    if override_type_3_6 {
        let _e101 = base[3u];
        if (_e101 == 0f) {
            discard;
        }
    } else {
        if override_type_3_7 {
            let _e103 = base;
            let _e105 = base;
            if (dot(_e103.xyz, _e105.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e109 = base;
    out_color = _e109;
    let _e111 = color0_[3u];
    param_9 = _e111;
    let _e112 = wiredTemporalAtestConfidence_u0028_f1_u003b((&param_9));
    param_10 = _e112;
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
