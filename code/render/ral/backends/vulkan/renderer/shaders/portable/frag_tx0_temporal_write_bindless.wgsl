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
    emissionRadiance: vec4<f32>,
    materialBaseColor: vec4<f32>,
    materialFactors: vec4<f32>,
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
var<private> frag_color0In_1: vec4<f32>;
var<private> frag_tex_coord0_1: vec2<f32>;
var<private> out_color: vec4<f32>;

fn wiredTemporalFinite2_u0028_vf2_u003b(value: ptr<function, vec2<f32>>) -> bool {
    var phi_82_: bool;

    let _e74 = (*value);
    let _e75 = (*value);
    let _e77 = all((_e74 == _e75));
    phi_82_ = _e77;
    if _e77 {
        let _e78 = (*value);
        phi_82_ = all((abs(_e78) <= vec2<f32>(340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f)));
    }
    let _e83 = phi_82_;
    return _e83;
}

fn wiredTemporalFinite4_u0028_vf4_u003b(value_1: ptr<function, vec4<f32>>) -> bool {
    var phi_67_: bool;

    let _e74 = (*value_1);
    let _e75 = (*value_1);
    let _e77 = all((_e74 == _e75));
    phi_67_ = _e77;
    if _e77 {
        let _e78 = (*value_1);
        phi_67_ = all((abs(_e78) <= vec4<f32>(340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f)));
    }
    let _e83 = phi_67_;
    return _e83;
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
    var phi_105_: bool;
    var phi_114_: bool;
    var phi_124_: bool;
    var phi_131_: bool;
    var phi_160_: bool;

    out_temporal_velocity = vec2<f32>(0f, 0f);
    out_temporal_validity = 0f;
    let _e84 = temporalOutcome_1;
    let _e85 = (_e84 != 1u);
    phi_105_ = _e85;
    if !(_e85) {
        let _e87 = temporalCurrentClip_1;
        param = _e87;
        let _e88 = wiredTemporalFinite4_u0028_vf4_u003b((&param));
        phi_105_ = !(_e88);
    }
    let _e91 = phi_105_;
    phi_114_ = _e91;
    if !(_e91) {
        let _e93 = temporalPreviousClip_1;
        param_1 = _e93;
        let _e94 = wiredTemporalFinite4_u0028_vf4_u003b((&param_1));
        phi_114_ = !(_e94);
    }
    let _e97 = phi_114_;
    phi_124_ = _e97;
    if !(_e97) {
        let _e100 = temporalCurrentClip_1[3u];
        phi_124_ = (_e100 <= 0.000001f);
    }
    let _e103 = phi_124_;
    phi_131_ = _e103;
    if !(_e103) {
        let _e106 = temporalPreviousClip_1[3u];
        phi_131_ = (_e106 <= 0.000001f);
    }
    let _e109 = phi_131_;
    if _e109 {
        return;
    }
    let _e110 = temporalCurrentClip_1;
    let _e113 = temporalCurrentClip_1[3u];
    currentNdc = (_e110.xy / vec2(_e113));
    let _e116 = temporalPreviousClip_1;
    let _e119 = temporalPreviousClip_1[3u];
    previousNdc = (_e116.xy / vec2(_e119));
    let _e122 = currentNdc;
    param_2 = _e122;
    let _e123 = wiredTemporalFinite2_u0028_vf2_u003b((&param_2));
    let _e124 = !(_e123);
    phi_160_ = _e124;
    if !(_e124) {
        let _e126 = previousNdc;
        param_3 = _e126;
        let _e127 = wiredTemporalFinite2_u0028_vf2_u003b((&param_3));
        phi_160_ = !(_e127);
    }
    let _e130 = phi_160_;
    if _e130 {
        return;
    }
    let _e131 = currentNdc;
    currentUv = ((_e131 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e134 = previousNdc;
    previousUv = ((_e134 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e137 = currentUv;
    let _e138 = previousUv;
    velocity = (_e137 - _e138);
    let _e140 = velocity;
    param_4 = _e140;
    let _e141 = wiredTemporalFinite2_u0028_vf2_u003b((&param_4));
    if !(_e141) {
        return;
    }
    let _e143 = velocity;
    out_temporal_velocity = _e143;
    let _e144 = (*coverageConfidence);
    out_temporal_validity = clamp(_e144, 0f, 1f);
    return;
}

fn wiredTemporalAtestConfidence_u0028_f1_u003b(alpha: ptr<function, f32>) -> f32 {
    var span: f32;

    if override_type_3_ {
        span = max(max(alpha_test_value, (1f - alpha_test_value)), 0.000001f);
        let _e78 = (*alpha);
        let _e81 = span;
        return clamp((abs((_e78 - alpha_test_value)) / _e81), 0f, 1f);
    } else {
        if override_type_3_1 {
            let _e84 = (*alpha);
            return clamp(((alpha_test_value - _e84) / max(alpha_test_value, 0.000001f)), 0f, 1f);
        } else {
            if override_type_3_2 {
                let _e89 = (*alpha);
                return clamp(((_e89 - alpha_test_value) / max((1f - alpha_test_value), 0.000001f)), 0f, 1f);
            }
        }
    }
    return 1f;
}

fn wired_advanced_fog_enabled_u0028_() -> bool {
    var fogType: i32;

    let _e76 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e76 + 0.5f));
    let _e81 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e83 = fogType;
    let _e86 = fogType;
    return (((_e81 > 0.5f) && (_e83 >= 1i)) && (_e86 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e76 = wired_advanced_fog_enabled_u0028_();
    if !(_e76) {
        return 0f;
    }
    let _e79 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e79, 0.000001f));
    let _e84 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e84 + 0.5f));
    let _e87 = fogType_1;
    if (_e87 == 1i) {
        let _e91 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e91 <= 0f) {
            return 0f;
        }
        let _e93 = viewDepth;
        let _e96 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e93 / _e96), 0f, 1f);
    }
    let _e101 = unnamed.advancedFogColorDensity[3u];
    let _e103 = viewDepth;
    opticalDepth = (max(_e101, 0f) * _e103);
    let _e105 = fogType_1;
    if (_e105 == 2i) {
        let _e107 = opticalDepth;
        return clamp((1f - exp(-(_e107))), 0f, 1f);
    }
    let _e112 = opticalDepth;
    let _e113 = opticalDepth;
    return clamp((1f - exp(-((_e112 * _e113)))), 0f, 1f);
}

fn wired_boost_legacy_lightmap_u0028_vf3_u003b(rgb: ptr<function, vec3<f32>>) -> vec3<f32> {
    var boosted: vec3<f32>;
    var peak: f32;

    let _e76 = (*rgb);
    let _e79 = unnamed.worldLightParams[0u];
    boosted = (_e76 * _e79);
    let _e82 = boosted[0u];
    let _e84 = boosted[1u];
    let _e86 = boosted[2u];
    peak = max(_e82, max(_e84, _e86));
    let _e89 = peak;
    if (_e89 > 1f) {
        let _e91 = peak;
        let _e92 = boosted;
        boosted = (_e92 / vec3(_e91));
    }
    let _e95 = boosted;
    return _e95;
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e77 = (*c);
    (*c) = max(_e77, vec3<f32>(0f, 0f, 0f));
    let _e79 = (*c);
    cutoff = (_e79 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e81 = (*c);
    lo = (_e81 / vec3(12.92f));
    let _e84 = (*c);
    hi = pow(((_e84 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e89 = hi;
    let _e90 = lo;
    let _e91 = cutoff;
    return mix(_e89, _e90, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e91));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param_5: vec3<f32>;
    var param_6: vec3<f32>;

    let _e79 = (*role);
    let _e81 = (*role);
    let _e86 = unnamed.packed_indices[(_e79 / 4u)][(_e81 % 4u)];
    let _e89 = (*role);
    let _e91 = (*role);
    let _e96 = unnamed.packed_indices[(_e89 / 4u)][(_e91 % 4u)];
    let _e101 = (*uv);
    let _e102 = textureSample(wired_bindless_images[(_e86 & 4095u)], wired_bindless_samplers[((_e96 >> bitcast<u32>(12i)) & 255u)], _e101);
    c_1 = _e102;
    let _e103 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e103))) == 0i) {
        let _e108 = c_1;
        param_5 = _e108.xyz;
        let _e110 = sRGBToLinear_u0028_vf3_u003b((&param_5));
        c_1[0u] = _e110.x;
        c_1[1u] = _e110.y;
        c_1[2u] = _e110.z;
    }
    let _e117 = (*slot);
    if (lightmap_slot == (_e117 + 1i)) {
        let _e120 = c_1;
        param_6 = _e120.xyz;
        let _e122 = wired_boost_legacy_lightmap_u0028_vf3_u003b((&param_6));
        c_1[0u] = _e122.x;
        c_1[1u] = _e122.y;
        c_1[2u] = _e122.z;
    }
    let _e129 = c_1;
    return _e129;
}

fn main_1() {
    var frag_color0_: vec4<f32>;
    var param_7: vec3<f32>;
    var sampledColor0_: vec4<f32>;
    var param_8: u32;
    var param_9: vec2<f32>;
    var param_10: i32;
    var color0_: vec4<f32>;
    var base: vec4<f32>;
    var wetness: f32;
    var frost: f32;
    var luminance: f32;
    var fogAmount: f32;
    var param_11: f32;
    var param_12: f32;

    let _e87 = frag_color0In_1;
    param_7 = _e87.xyz;
    let _e89 = sRGBToLinear_u0028_vf3_u003b((&param_7));
    let _e91 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e89.x, _e89.y, _e89.z, _e91);
    param_8 = 0u;
    let _e96 = frag_tex_coord0_1;
    param_9 = _e96;
    param_10 = 0i;
    let _e97 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_8), (&param_9), (&param_10));
    sampledColor0_ = _e97;
    let _e98 = sampledColor0_;
    let _e99 = frag_color0_;
    color0_ = (_e98 * _e99);
    let _e101 = color0_;
    base = _e101;
    if override_type_3_3 {
        let _e103 = color0_[3u];
        if (_e103 == alpha_test_value) {
            discard;
        }
    } else {
        if override_type_3_4 {
            let _e106 = color0_[3u];
            if (_e106 >= alpha_test_value) {
                discard;
            }
        } else {
            if override_type_3_5 {
                let _e109 = color0_[3u];
                if (_e109 < alpha_test_value) {
                    discard;
                }
            }
        }
    }
    let _e111 = color0_;
    base = _e111;
    if override_type_3_6 {
        let _e114 = unnamed.worldLightParams[1u];
        wetness = clamp(_e114, 0f, 1f);
        let _e118 = unnamed.worldLightParams[2u];
        frost = clamp(_e118, 0f, 1f);
        let _e120 = base;
        luminance = dot(_e120.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e123 = wetness;
        let _e125 = base;
        let _e127 = (_e125.xyz * mix(1f, 0.82f, _e123));
        base[0u] = _e127.x;
        base[1u] = _e127.y;
        base[2u] = _e127.z;
        let _e134 = base;
        let _e136 = luminance;
        let _e138 = luminance;
        let _e140 = luminance;
        let _e142 = frost;
        let _e145 = mix(_e134.xyz, vec3<f32>((_e136 * 0.88f), (_e138 * 0.94f), _e140), vec3((_e142 * 0.55f)));
        base[0u] = _e145.x;
        base[1u] = _e145.y;
        base[2u] = _e145.z;
    }
    let _e154 = unnamed.materialFactors[0u];
    if (_e154 >= 0f) {
        let _e157 = unnamed.materialBaseColor;
        let _e158 = base;
        base = (_e158 * _e157);
    }
    let _e160 = color0_;
    let _e163 = unnamed.emissionRadiance;
    let _e166 = base;
    let _e168 = (_e166.xyz + (_e160.xyz * _e163.xyz));
    base[0u] = _e168.x;
    base[1u] = _e168.y;
    base[2u] = _e168.z;
    let _e175 = wired_advanced_fog_enabled_u0028_();
    if _e175 {
        let _e176 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e176;
        let _e177 = base;
        let _e180 = unnamed.advancedFogColorDensity;
        let _e182 = fogAmount;
        let _e184 = mix(_e177.xyz, _e180.xyz, vec3(_e182));
        base[0u] = _e184.x;
        base[1u] = _e184.y;
        base[2u] = _e184.z;
    }
    if override_type_3_7 {
        let _e192 = base[3u];
        if (_e192 == 0f) {
            discard;
        }
    } else {
        if override_type_3_8 {
            let _e194 = base;
            let _e196 = base;
            if (dot(_e194.xyz, _e196.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e200 = base;
    out_color = _e200;
    let _e202 = color0_[3u];
    param_11 = _e202;
    let _e203 = wiredTemporalAtestConfidence_u0028_f1_u003b((&param_11));
    param_12 = _e203;
    wiredTemporalWriteAux_u0028_f1_u003b((&param_12));
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
