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
var<private> frag_color0In_1: vec4<f32>;
var<private> frag_tex_coord0_1: vec2<f32>;
var<private> out_color: vec4<f32>;

fn wiredTemporalFinite2_u0028_vf2_u003b(value: ptr<function, vec2<f32>>) -> bool {
    var phi_79_: bool;

    let _e62 = (*value);
    let _e63 = (*value);
    let _e65 = all((_e62 == _e63));
    phi_79_ = _e65;
    if _e65 {
        let _e66 = (*value);
        phi_79_ = all((abs(_e66) <= vec2<f32>(340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f)));
    }
    let _e71 = phi_79_;
    return _e71;
}

fn wiredTemporalFinite4_u0028_vf4_u003b(value_1: ptr<function, vec4<f32>>) -> bool {
    var phi_64_: bool;

    let _e62 = (*value_1);
    let _e63 = (*value_1);
    let _e65 = all((_e62 == _e63));
    phi_64_ = _e65;
    if _e65 {
        let _e66 = (*value_1);
        phi_64_ = all((abs(_e66) <= vec4<f32>(340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f)));
    }
    let _e71 = phi_64_;
    return _e71;
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
    let _e72 = temporalOutcome_1;
    let _e73 = (_e72 != 1u);
    phi_102_ = _e73;
    if !(_e73) {
        let _e75 = temporalCurrentClip_1;
        param = _e75;
        let _e76 = wiredTemporalFinite4_u0028_vf4_u003b((&param));
        phi_102_ = !(_e76);
    }
    let _e79 = phi_102_;
    phi_111_ = _e79;
    if !(_e79) {
        let _e81 = temporalPreviousClip_1;
        param_1 = _e81;
        let _e82 = wiredTemporalFinite4_u0028_vf4_u003b((&param_1));
        phi_111_ = !(_e82);
    }
    let _e85 = phi_111_;
    phi_121_ = _e85;
    if !(_e85) {
        let _e88 = temporalCurrentClip_1[3u];
        phi_121_ = (_e88 <= 0.000001f);
    }
    let _e91 = phi_121_;
    phi_128_ = _e91;
    if !(_e91) {
        let _e94 = temporalPreviousClip_1[3u];
        phi_128_ = (_e94 <= 0.000001f);
    }
    let _e97 = phi_128_;
    if _e97 {
        return;
    }
    let _e98 = temporalCurrentClip_1;
    let _e101 = temporalCurrentClip_1[3u];
    currentNdc = (_e98.xy / vec2(_e101));
    let _e104 = temporalPreviousClip_1;
    let _e107 = temporalPreviousClip_1[3u];
    previousNdc = (_e104.xy / vec2(_e107));
    let _e110 = currentNdc;
    param_2 = _e110;
    let _e111 = wiredTemporalFinite2_u0028_vf2_u003b((&param_2));
    let _e112 = !(_e111);
    phi_157_ = _e112;
    if !(_e112) {
        let _e114 = previousNdc;
        param_3 = _e114;
        let _e115 = wiredTemporalFinite2_u0028_vf2_u003b((&param_3));
        phi_157_ = !(_e115);
    }
    let _e118 = phi_157_;
    if _e118 {
        return;
    }
    let _e119 = currentNdc;
    currentUv = ((_e119 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e122 = previousNdc;
    previousUv = ((_e122 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e125 = currentUv;
    let _e126 = previousUv;
    velocity = (_e125 - _e126);
    let _e128 = velocity;
    param_4 = _e128;
    let _e129 = wiredTemporalFinite2_u0028_vf2_u003b((&param_4));
    if !(_e129) {
        return;
    }
    let _e131 = velocity;
    out_temporal_velocity = _e131;
    let _e132 = (*coverageConfidence);
    out_temporal_validity = clamp(_e132, 0f, 1f);
    return;
}

fn wiredTemporalAtestConfidence_u0028_f1_u003b(alpha: ptr<function, f32>) -> f32 {
    var span: f32;

    if override_type_3_ {
        span = max(max(alpha_test_value, (1f - alpha_test_value)), 0.000001f);
        let _e66 = (*alpha);
        let _e69 = span;
        return clamp((abs((_e66 - alpha_test_value)) / _e69), 0f, 1f);
    } else {
        if override_type_3_1 {
            let _e72 = (*alpha);
            return clamp(((alpha_test_value - _e72) / max(alpha_test_value, 0.000001f)), 0f, 1f);
        } else {
            if override_type_3_2 {
                let _e77 = (*alpha);
                return clamp(((_e77 - alpha_test_value) / max((1f - alpha_test_value), 0.000001f)), 0f, 1f);
            }
        }
    }
    return 1f;
}

fn wired_advanced_fog_enabled_u0028_() -> bool {
    var fogType: i32;

    let _e64 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e64 + 0.5f));
    let _e69 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e71 = fogType;
    let _e74 = fogType;
    return (((_e69 > 0.5f) && (_e71 >= 1i)) && (_e74 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e64 = wired_advanced_fog_enabled_u0028_();
    if !(_e64) {
        return 0f;
    }
    let _e67 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e67, 0.000001f));
    let _e72 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e72 + 0.5f));
    let _e75 = fogType_1;
    if (_e75 == 1i) {
        let _e79 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e79 <= 0f) {
            return 0f;
        }
        let _e81 = viewDepth;
        let _e84 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e81 / _e84), 0f, 1f);
    }
    let _e89 = unnamed.advancedFogColorDensity[3u];
    let _e91 = viewDepth;
    opticalDepth = (max(_e89, 0f) * _e91);
    let _e93 = fogType_1;
    if (_e93 == 2i) {
        let _e95 = opticalDepth;
        return clamp((1f - exp(-(_e95))), 0f, 1f);
    }
    let _e100 = opticalDepth;
    let _e101 = opticalDepth;
    return clamp((1f - exp(-((_e100 * _e101)))), 0f, 1f);
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e65 = (*c);
    (*c) = max(_e65, vec3<f32>(0f, 0f, 0f));
    let _e67 = (*c);
    cutoff = (_e67 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e69 = (*c);
    lo = (_e69 / vec3(12.92f));
    let _e72 = (*c);
    hi = pow(((_e72 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e77 = hi;
    let _e78 = lo;
    let _e79 = cutoff;
    return mix(_e77, _e78, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e79));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param_5: vec3<f32>;

    let _e66 = (*role);
    let _e68 = (*role);
    let _e73 = unnamed.packed_indices[(_e66 / 4u)][(_e68 % 4u)];
    let _e76 = (*role);
    let _e78 = (*role);
    let _e83 = unnamed.packed_indices[(_e76 / 4u)][(_e78 % 4u)];
    let _e88 = (*uv);
    let _e89 = textureSample(wired_bindless_images[(_e73 & 4095u)], wired_bindless_samplers[((_e83 >> bitcast<u32>(12i)) & 255u)], _e88);
    c_1 = _e89;
    let _e90 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e90))) == 0i) {
        let _e95 = c_1;
        param_5 = _e95.xyz;
        let _e97 = sRGBToLinear_u0028_vf3_u003b((&param_5));
        c_1[0u] = _e97.x;
        c_1[1u] = _e97.y;
        c_1[2u] = _e97.z;
    }
    let _e104 = (*slot);
    if (lightmap_slot == (_e104 + 1i)) {
        let _e109 = unnamed.worldLightParams[0u];
        let _e110 = c_1;
        let _e112 = (_e110.xyz * _e109);
        c_1[0u] = _e112.x;
        c_1[1u] = _e112.y;
        c_1[2u] = _e112.z;
    }
    let _e119 = c_1;
    return _e119;
}

fn main_1() {
    var frag_color0_: vec4<f32>;
    var param_6: vec3<f32>;
    var color0_: vec4<f32>;
    var param_7: u32;
    var param_8: vec2<f32>;
    var param_9: i32;
    var base: vec4<f32>;
    var fogAmount: f32;
    var param_10: f32;
    var param_11: f32;

    let _e71 = frag_color0In_1;
    param_6 = _e71.xyz;
    let _e73 = sRGBToLinear_u0028_vf3_u003b((&param_6));
    let _e75 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e73.x, _e73.y, _e73.z, _e75);
    param_7 = 0u;
    let _e80 = frag_tex_coord0_1;
    param_8 = _e80;
    param_9 = 0i;
    let _e81 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_7), (&param_8), (&param_9));
    let _e82 = frag_color0_;
    color0_ = (_e81 * _e82);
    let _e84 = color0_;
    base = _e84;
    if override_type_3_3 {
        let _e86 = color0_[3u];
        if (_e86 == alpha_test_value) {
            discard;
        }
    } else {
        if override_type_3_4 {
            let _e89 = color0_[3u];
            if (_e89 >= alpha_test_value) {
                discard;
            }
        } else {
            if override_type_3_5 {
                let _e92 = color0_[3u];
                if (_e92 < alpha_test_value) {
                    discard;
                }
            }
        }
    }
    let _e94 = color0_;
    base = _e94;
    let _e95 = wired_advanced_fog_enabled_u0028_();
    if _e95 {
        let _e96 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e96;
        let _e97 = base;
        let _e100 = unnamed.advancedFogColorDensity;
        let _e102 = fogAmount;
        let _e104 = mix(_e97.xyz, _e100.xyz, vec3(_e102));
        base[0u] = _e104.x;
        base[1u] = _e104.y;
        base[2u] = _e104.z;
    }
    if override_type_3_6 {
        let _e112 = base[3u];
        if (_e112 == 0f) {
            discard;
        }
    } else {
        if override_type_3_7 {
            let _e114 = base;
            let _e116 = base;
            if (dot(_e114.xyz, _e116.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e120 = base;
    out_color = _e120;
    let _e122 = color0_[3u];
    param_10 = _e122;
    let _e123 = wiredTemporalAtestConfidence_u0028_f1_u003b((&param_10));
    param_11 = _e123;
    wiredTemporalWriteAux_u0028_f1_u003b((&param_11));
    return;
}

@fragment
fn main(@location(12) @interpolate(flat) temporalOutcome: u32, @location(10) temporalCurrentClip: vec4<f32>, @location(11) temporalPreviousClip: vec4<f32>, @builtin(position) gl_FragCoord: vec4<f32>, @location(0) frag_color0In: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>) -> FragmentOutput {
    temporalOutcome_1 = temporalOutcome;
    temporalCurrentClip_1 = temporalCurrentClip;
    temporalPreviousClip_1 = temporalPreviousClip;
    gl_FragCoord_1 = gl_FragCoord;
    frag_color0In_1 = frag_color0In;
    frag_tex_coord0_1 = frag_tex_coord0_;
    main_1();
    let _e15 = out_temporal_velocity;
    let _e16 = out_temporal_validity;
    let _e17 = out_color;
    return FragmentOutput(_e15, _e16, _e17);
}
