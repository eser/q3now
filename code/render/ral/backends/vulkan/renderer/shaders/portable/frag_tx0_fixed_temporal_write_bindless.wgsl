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

@id(0) override alpha_test_func: i32 = 0i;
override override_type_3_: bool = (alpha_test_func == 1i);
@id(1) override alpha_test_value: f32 = 0f;
override override_type_3_1: bool = (alpha_test_func == 2i);
override override_type_3_2: bool = (alpha_test_func == 3i);
@id(4) override tex_domain: i32 = 0i;
@id(26) override lightmap_slot: i32 = 0i;
@id(8) override identity_color: f32 = 1f;
@id(9) override identity_alpha: f32 = 1f;
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

    let _e63 = (*value);
    let _e64 = (*value);
    let _e66 = all((_e63 == _e64));
    phi_79_ = _e66;
    if _e66 {
        let _e67 = (*value);
        phi_79_ = all((abs(_e67) <= vec2<f32>(340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f)));
    }
    let _e72 = phi_79_;
    return _e72;
}

fn wiredTemporalFinite4_u0028_vf4_u003b(value_1: ptr<function, vec4<f32>>) -> bool {
    var phi_64_: bool;

    let _e63 = (*value_1);
    let _e64 = (*value_1);
    let _e66 = all((_e63 == _e64));
    phi_64_ = _e66;
    if _e66 {
        let _e67 = (*value_1);
        phi_64_ = all((abs(_e67) <= vec4<f32>(340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f)));
    }
    let _e72 = phi_64_;
    return _e72;
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
    let _e73 = temporalOutcome_1;
    let _e74 = (_e73 != 1u);
    phi_102_ = _e74;
    if !(_e74) {
        let _e76 = temporalCurrentClip_1;
        param = _e76;
        let _e77 = wiredTemporalFinite4_u0028_vf4_u003b((&param));
        phi_102_ = !(_e77);
    }
    let _e80 = phi_102_;
    phi_111_ = _e80;
    if !(_e80) {
        let _e82 = temporalPreviousClip_1;
        param_1 = _e82;
        let _e83 = wiredTemporalFinite4_u0028_vf4_u003b((&param_1));
        phi_111_ = !(_e83);
    }
    let _e86 = phi_111_;
    phi_121_ = _e86;
    if !(_e86) {
        let _e89 = temporalCurrentClip_1[3u];
        phi_121_ = (_e89 <= 0.000001f);
    }
    let _e92 = phi_121_;
    phi_128_ = _e92;
    if !(_e92) {
        let _e95 = temporalPreviousClip_1[3u];
        phi_128_ = (_e95 <= 0.000001f);
    }
    let _e98 = phi_128_;
    if _e98 {
        return;
    }
    let _e99 = temporalCurrentClip_1;
    let _e102 = temporalCurrentClip_1[3u];
    currentNdc = (_e99.xy / vec2(_e102));
    let _e105 = temporalPreviousClip_1;
    let _e108 = temporalPreviousClip_1[3u];
    previousNdc = (_e105.xy / vec2(_e108));
    let _e111 = currentNdc;
    param_2 = _e111;
    let _e112 = wiredTemporalFinite2_u0028_vf2_u003b((&param_2));
    let _e113 = !(_e112);
    phi_157_ = _e113;
    if !(_e113) {
        let _e115 = previousNdc;
        param_3 = _e115;
        let _e116 = wiredTemporalFinite2_u0028_vf2_u003b((&param_3));
        phi_157_ = !(_e116);
    }
    let _e119 = phi_157_;
    if _e119 {
        return;
    }
    let _e120 = currentNdc;
    currentUv = ((_e120 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e123 = previousNdc;
    previousUv = ((_e123 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e126 = currentUv;
    let _e127 = previousUv;
    velocity = (_e126 - _e127);
    let _e129 = velocity;
    param_4 = _e129;
    let _e130 = wiredTemporalFinite2_u0028_vf2_u003b((&param_4));
    if !(_e130) {
        return;
    }
    let _e132 = velocity;
    out_temporal_velocity = _e132;
    let _e133 = (*coverageConfidence);
    out_temporal_validity = clamp(_e133, 0f, 1f);
    return;
}

fn wiredTemporalAtestConfidence_u0028_f1_u003b(alpha: ptr<function, f32>) -> f32 {
    var span: f32;

    if override_type_3_ {
        span = max(max(alpha_test_value, (1f - alpha_test_value)), 0.000001f);
        let _e67 = (*alpha);
        let _e70 = span;
        return clamp((abs((_e67 - alpha_test_value)) / _e70), 0f, 1f);
    } else {
        if override_type_3_1 {
            let _e73 = (*alpha);
            return clamp(((alpha_test_value - _e73) / max(alpha_test_value, 0.000001f)), 0f, 1f);
        } else {
            if override_type_3_2 {
                let _e78 = (*alpha);
                return clamp(((_e78 - alpha_test_value) / max((1f - alpha_test_value), 0.000001f)), 0f, 1f);
            }
        }
    }
    return 1f;
}

fn wired_advanced_fog_enabled_u0028_() -> bool {
    var fogType: i32;

    let _e65 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e65 + 0.5f));
    let _e70 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e72 = fogType;
    let _e75 = fogType;
    return (((_e70 > 0.5f) && (_e72 >= 1i)) && (_e75 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e65 = wired_advanced_fog_enabled_u0028_();
    if !(_e65) {
        return 0f;
    }
    let _e68 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e68, 0.000001f));
    let _e73 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e73 + 0.5f));
    let _e76 = fogType_1;
    if (_e76 == 1i) {
        let _e80 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e80 <= 0f) {
            return 0f;
        }
        let _e82 = viewDepth;
        let _e85 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e82 / _e85), 0f, 1f);
    }
    let _e90 = unnamed.advancedFogColorDensity[3u];
    let _e92 = viewDepth;
    opticalDepth = (max(_e90, 0f) * _e92);
    let _e94 = fogType_1;
    if (_e94 == 2i) {
        let _e96 = opticalDepth;
        return clamp((1f - exp(-(_e96))), 0f, 1f);
    }
    let _e101 = opticalDepth;
    let _e102 = opticalDepth;
    return clamp((1f - exp(-((_e101 * _e102)))), 0f, 1f);
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e66 = (*c);
    (*c) = max(_e66, vec3<f32>(0f, 0f, 0f));
    let _e68 = (*c);
    cutoff = (_e68 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e70 = (*c);
    lo = (_e70 / vec3(12.92f));
    let _e73 = (*c);
    hi = pow(((_e73 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e78 = hi;
    let _e79 = lo;
    let _e80 = cutoff;
    return mix(_e78, _e79, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e80));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param_5: vec3<f32>;

    let _e67 = (*role);
    let _e69 = (*role);
    let _e74 = unnamed.packed_indices[(_e67 / 4u)][(_e69 % 4u)];
    let _e77 = (*role);
    let _e79 = (*role);
    let _e84 = unnamed.packed_indices[(_e77 / 4u)][(_e79 % 4u)];
    let _e89 = (*uv);
    let _e90 = textureSample(wired_bindless_images[(_e74 & 4095u)], wired_bindless_samplers[((_e84 >> bitcast<u32>(12i)) & 255u)], _e89);
    c_1 = _e90;
    let _e91 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e91))) == 0i) {
        let _e96 = c_1;
        param_5 = _e96.xyz;
        let _e98 = sRGBToLinear_u0028_vf3_u003b((&param_5));
        c_1[0u] = _e98.x;
        c_1[1u] = _e98.y;
        c_1[2u] = _e98.z;
    }
    let _e105 = (*slot);
    if (lightmap_slot == (_e105 + 1i)) {
        let _e110 = unnamed.worldLightParams[0u];
        let _e111 = c_1;
        let _e113 = (_e111.xyz * _e110);
        c_1[0u] = _e113.x;
        c_1[1u] = _e113.y;
        c_1[2u] = _e113.z;
    }
    let _e120 = c_1;
    return _e120;
}

fn main_1() {
    var frag_color: vec4<f32>;
    var color0_: vec4<f32>;
    var param_6: u32;
    var param_7: vec2<f32>;
    var param_8: i32;
    var base: vec4<f32>;
    var fogAmount: f32;
    var param_9: f32;
    var param_10: f32;

    frag_color[0u] = identity_color;
    frag_color[1u] = identity_color;
    frag_color[2u] = identity_color;
    frag_color[3u] = identity_alpha;
    param_6 = 0u;
    let _e75 = frag_tex_coord0_1;
    param_7 = _e75;
    param_8 = 0i;
    let _e76 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_6), (&param_7), (&param_8));
    let _e77 = frag_color;
    color0_ = (_e76 * _e77);
    let _e79 = color0_;
    base = _e79;
    if override_type_3_3 {
        let _e81 = color0_[3u];
        if (_e81 == alpha_test_value) {
            discard;
        }
    } else {
        if override_type_3_4 {
            let _e84 = color0_[3u];
            if (_e84 >= alpha_test_value) {
                discard;
            }
        } else {
            if override_type_3_5 {
                let _e87 = color0_[3u];
                if (_e87 < alpha_test_value) {
                    discard;
                }
            }
        }
    }
    let _e89 = color0_;
    base = _e89;
    let _e90 = wired_advanced_fog_enabled_u0028_();
    if _e90 {
        let _e91 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e91;
        let _e92 = base;
        let _e95 = unnamed.advancedFogColorDensity;
        let _e97 = fogAmount;
        let _e99 = mix(_e92.xyz, _e95.xyz, vec3(_e97));
        base[0u] = _e99.x;
        base[1u] = _e99.y;
        base[2u] = _e99.z;
    }
    if override_type_3_6 {
        let _e107 = base[3u];
        if (_e107 == 0f) {
            discard;
        }
    } else {
        if override_type_3_7 {
            let _e109 = base;
            let _e111 = base;
            if (dot(_e109.xyz, _e111.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e115 = base;
    out_color = _e115;
    let _e117 = color0_[3u];
    param_9 = _e117;
    let _e118 = wiredTemporalAtestConfidence_u0028_f1_u003b((&param_9));
    param_10 = _e118;
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
