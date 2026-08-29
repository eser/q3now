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
    var phi_79_: bool;

    let _e72 = (*value);
    let _e73 = (*value);
    let _e75 = all((_e72 == _e73));
    phi_79_ = _e75;
    if _e75 {
        let _e76 = (*value);
        phi_79_ = all((abs(_e76) <= vec2<f32>(340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f)));
    }
    let _e81 = phi_79_;
    return _e81;
}

fn wiredTemporalFinite4_u0028_vf4_u003b(value_1: ptr<function, vec4<f32>>) -> bool {
    var phi_64_: bool;

    let _e72 = (*value_1);
    let _e73 = (*value_1);
    let _e75 = all((_e72 == _e73));
    phi_64_ = _e75;
    if _e75 {
        let _e76 = (*value_1);
        phi_64_ = all((abs(_e76) <= vec4<f32>(340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f)));
    }
    let _e81 = phi_64_;
    return _e81;
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
    let _e82 = temporalOutcome_1;
    let _e83 = (_e82 != 1u);
    phi_102_ = _e83;
    if !(_e83) {
        let _e85 = temporalCurrentClip_1;
        param = _e85;
        let _e86 = wiredTemporalFinite4_u0028_vf4_u003b((&param));
        phi_102_ = !(_e86);
    }
    let _e89 = phi_102_;
    phi_111_ = _e89;
    if !(_e89) {
        let _e91 = temporalPreviousClip_1;
        param_1 = _e91;
        let _e92 = wiredTemporalFinite4_u0028_vf4_u003b((&param_1));
        phi_111_ = !(_e92);
    }
    let _e95 = phi_111_;
    phi_121_ = _e95;
    if !(_e95) {
        let _e98 = temporalCurrentClip_1[3u];
        phi_121_ = (_e98 <= 0.000001f);
    }
    let _e101 = phi_121_;
    phi_128_ = _e101;
    if !(_e101) {
        let _e104 = temporalPreviousClip_1[3u];
        phi_128_ = (_e104 <= 0.000001f);
    }
    let _e107 = phi_128_;
    if _e107 {
        return;
    }
    let _e108 = temporalCurrentClip_1;
    let _e111 = temporalCurrentClip_1[3u];
    currentNdc = (_e108.xy / vec2(_e111));
    let _e114 = temporalPreviousClip_1;
    let _e117 = temporalPreviousClip_1[3u];
    previousNdc = (_e114.xy / vec2(_e117));
    let _e120 = currentNdc;
    param_2 = _e120;
    let _e121 = wiredTemporalFinite2_u0028_vf2_u003b((&param_2));
    let _e122 = !(_e121);
    phi_157_ = _e122;
    if !(_e122) {
        let _e124 = previousNdc;
        param_3 = _e124;
        let _e125 = wiredTemporalFinite2_u0028_vf2_u003b((&param_3));
        phi_157_ = !(_e125);
    }
    let _e128 = phi_157_;
    if _e128 {
        return;
    }
    let _e129 = currentNdc;
    currentUv = ((_e129 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e132 = previousNdc;
    previousUv = ((_e132 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e135 = currentUv;
    let _e136 = previousUv;
    velocity = (_e135 - _e136);
    let _e138 = velocity;
    param_4 = _e138;
    let _e139 = wiredTemporalFinite2_u0028_vf2_u003b((&param_4));
    if !(_e139) {
        return;
    }
    let _e141 = velocity;
    out_temporal_velocity = _e141;
    let _e142 = (*coverageConfidence);
    out_temporal_validity = clamp(_e142, 0f, 1f);
    return;
}

fn wiredTemporalAtestConfidence_u0028_f1_u003b(alpha: ptr<function, f32>) -> f32 {
    var span: f32;

    if override_type_3_ {
        span = max(max(alpha_test_value, (1f - alpha_test_value)), 0.000001f);
        let _e76 = (*alpha);
        let _e79 = span;
        return clamp((abs((_e76 - alpha_test_value)) / _e79), 0f, 1f);
    } else {
        if override_type_3_1 {
            let _e82 = (*alpha);
            return clamp(((alpha_test_value - _e82) / max(alpha_test_value, 0.000001f)), 0f, 1f);
        } else {
            if override_type_3_2 {
                let _e87 = (*alpha);
                return clamp(((_e87 - alpha_test_value) / max((1f - alpha_test_value), 0.000001f)), 0f, 1f);
            }
        }
    }
    return 1f;
}

fn wired_advanced_fog_enabled_u0028_() -> bool {
    var fogType: i32;

    let _e74 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e74 + 0.5f));
    let _e79 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e81 = fogType;
    let _e84 = fogType;
    return (((_e79 > 0.5f) && (_e81 >= 1i)) && (_e84 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e74 = wired_advanced_fog_enabled_u0028_();
    if !(_e74) {
        return 0f;
    }
    let _e77 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e77, 0.000001f));
    let _e82 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e82 + 0.5f));
    let _e85 = fogType_1;
    if (_e85 == 1i) {
        let _e89 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e89 <= 0f) {
            return 0f;
        }
        let _e91 = viewDepth;
        let _e94 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e91 / _e94), 0f, 1f);
    }
    let _e99 = unnamed.advancedFogColorDensity[3u];
    let _e101 = viewDepth;
    opticalDepth = (max(_e99, 0f) * _e101);
    let _e103 = fogType_1;
    if (_e103 == 2i) {
        let _e105 = opticalDepth;
        return clamp((1f - exp(-(_e105))), 0f, 1f);
    }
    let _e110 = opticalDepth;
    let _e111 = opticalDepth;
    return clamp((1f - exp(-((_e110 * _e111)))), 0f, 1f);
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e75 = (*c);
    (*c) = max(_e75, vec3<f32>(0f, 0f, 0f));
    let _e77 = (*c);
    cutoff = (_e77 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e79 = (*c);
    lo = (_e79 / vec3(12.92f));
    let _e82 = (*c);
    hi = pow(((_e82 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e87 = hi;
    let _e88 = lo;
    let _e89 = cutoff;
    return mix(_e87, _e88, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e89));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param_5: vec3<f32>;

    let _e76 = (*role);
    let _e78 = (*role);
    let _e83 = unnamed.packed_indices[(_e76 / 4u)][(_e78 % 4u)];
    let _e86 = (*role);
    let _e88 = (*role);
    let _e93 = unnamed.packed_indices[(_e86 / 4u)][(_e88 % 4u)];
    let _e98 = (*uv);
    let _e99 = textureSample(wired_bindless_images[(_e83 & 4095u)], wired_bindless_samplers[((_e93 >> bitcast<u32>(12i)) & 255u)], _e98);
    c_1 = _e99;
    let _e100 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e100))) == 0i) {
        let _e105 = c_1;
        param_5 = _e105.xyz;
        let _e107 = sRGBToLinear_u0028_vf3_u003b((&param_5));
        c_1[0u] = _e107.x;
        c_1[1u] = _e107.y;
        c_1[2u] = _e107.z;
    }
    let _e114 = (*slot);
    if (lightmap_slot == (_e114 + 1i)) {
        let _e119 = unnamed.worldLightParams[0u];
        let _e120 = c_1;
        let _e122 = (_e120.xyz * _e119);
        c_1[0u] = _e122.x;
        c_1[1u] = _e122.y;
        c_1[2u] = _e122.z;
    }
    let _e129 = c_1;
    return _e129;
}

fn main_1() {
    var frag_color0_: vec4<f32>;
    var param_6: vec3<f32>;
    var color0_: vec4<f32>;
    var param_7: u32;
    var param_8: vec2<f32>;
    var param_9: i32;
    var base: vec4<f32>;
    var wetness: f32;
    var frost: f32;
    var luminance: f32;
    var fogAmount: f32;
    var param_10: f32;
    var param_11: f32;

    let _e84 = frag_color0In_1;
    param_6 = _e84.xyz;
    let _e86 = sRGBToLinear_u0028_vf3_u003b((&param_6));
    let _e88 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e86.x, _e86.y, _e86.z, _e88);
    param_7 = 0u;
    let _e93 = frag_tex_coord0_1;
    param_8 = _e93;
    param_9 = 0i;
    let _e94 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_7), (&param_8), (&param_9));
    let _e95 = frag_color0_;
    color0_ = (_e94 * _e95);
    let _e97 = color0_;
    base = _e97;
    if override_type_3_3 {
        let _e99 = color0_[3u];
        if (_e99 == alpha_test_value) {
            discard;
        }
    } else {
        if override_type_3_4 {
            let _e102 = color0_[3u];
            if (_e102 >= alpha_test_value) {
                discard;
            }
        } else {
            if override_type_3_5 {
                let _e105 = color0_[3u];
                if (_e105 < alpha_test_value) {
                    discard;
                }
            }
        }
    }
    let _e107 = color0_;
    base = _e107;
    if override_type_3_6 {
        let _e110 = unnamed.worldLightParams[1u];
        wetness = clamp(_e110, 0f, 1f);
        let _e114 = unnamed.worldLightParams[2u];
        frost = clamp(_e114, 0f, 1f);
        let _e116 = base;
        luminance = dot(_e116.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e119 = wetness;
        let _e121 = base;
        let _e123 = (_e121.xyz * mix(1f, 0.82f, _e119));
        base[0u] = _e123.x;
        base[1u] = _e123.y;
        base[2u] = _e123.z;
        let _e130 = base;
        let _e132 = luminance;
        let _e134 = luminance;
        let _e136 = luminance;
        let _e138 = frost;
        let _e141 = mix(_e130.xyz, vec3<f32>((_e132 * 0.88f), (_e134 * 0.94f), _e136), vec3((_e138 * 0.55f)));
        base[0u] = _e141.x;
        base[1u] = _e141.y;
        base[2u] = _e141.z;
    }
    let _e148 = color0_;
    let _e151 = unnamed.emissionRadiance;
    let _e154 = base;
    let _e156 = (_e154.xyz + (_e148.xyz * _e151.xyz));
    base[0u] = _e156.x;
    base[1u] = _e156.y;
    base[2u] = _e156.z;
    let _e163 = wired_advanced_fog_enabled_u0028_();
    if _e163 {
        let _e164 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e164;
        let _e165 = base;
        let _e168 = unnamed.advancedFogColorDensity;
        let _e170 = fogAmount;
        let _e172 = mix(_e165.xyz, _e168.xyz, vec3(_e170));
        base[0u] = _e172.x;
        base[1u] = _e172.y;
        base[2u] = _e172.z;
    }
    if override_type_3_7 {
        let _e180 = base[3u];
        if (_e180 == 0f) {
            discard;
        }
    } else {
        if override_type_3_8 {
            let _e182 = base;
            let _e184 = base;
            if (dot(_e182.xyz, _e184.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e188 = base;
    out_color = _e188;
    let _e190 = color0_[3u];
    param_10 = _e190;
    let _e191 = wiredTemporalAtestConfidence_u0028_f1_u003b((&param_10));
    param_11 = _e191;
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
