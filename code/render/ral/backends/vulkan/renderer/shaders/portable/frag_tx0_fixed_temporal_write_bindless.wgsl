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
@id(8) override identity_color: f32 = 1f;
@id(9) override identity_alpha: f32 = 1f;
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

    let _e73 = (*value);
    let _e74 = (*value);
    let _e76 = all((_e73 == _e74));
    phi_79_ = _e76;
    if _e76 {
        let _e77 = (*value);
        phi_79_ = all((abs(_e77) <= vec2<f32>(340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f)));
    }
    let _e82 = phi_79_;
    return _e82;
}

fn wiredTemporalFinite4_u0028_vf4_u003b(value_1: ptr<function, vec4<f32>>) -> bool {
    var phi_64_: bool;

    let _e73 = (*value_1);
    let _e74 = (*value_1);
    let _e76 = all((_e73 == _e74));
    phi_64_ = _e76;
    if _e76 {
        let _e77 = (*value_1);
        phi_64_ = all((abs(_e77) <= vec4<f32>(340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f)));
    }
    let _e82 = phi_64_;
    return _e82;
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
    let _e83 = temporalOutcome_1;
    let _e84 = (_e83 != 1u);
    phi_102_ = _e84;
    if !(_e84) {
        let _e86 = temporalCurrentClip_1;
        param = _e86;
        let _e87 = wiredTemporalFinite4_u0028_vf4_u003b((&param));
        phi_102_ = !(_e87);
    }
    let _e90 = phi_102_;
    phi_111_ = _e90;
    if !(_e90) {
        let _e92 = temporalPreviousClip_1;
        param_1 = _e92;
        let _e93 = wiredTemporalFinite4_u0028_vf4_u003b((&param_1));
        phi_111_ = !(_e93);
    }
    let _e96 = phi_111_;
    phi_121_ = _e96;
    if !(_e96) {
        let _e99 = temporalCurrentClip_1[3u];
        phi_121_ = (_e99 <= 0.000001f);
    }
    let _e102 = phi_121_;
    phi_128_ = _e102;
    if !(_e102) {
        let _e105 = temporalPreviousClip_1[3u];
        phi_128_ = (_e105 <= 0.000001f);
    }
    let _e108 = phi_128_;
    if _e108 {
        return;
    }
    let _e109 = temporalCurrentClip_1;
    let _e112 = temporalCurrentClip_1[3u];
    currentNdc = (_e109.xy / vec2(_e112));
    let _e115 = temporalPreviousClip_1;
    let _e118 = temporalPreviousClip_1[3u];
    previousNdc = (_e115.xy / vec2(_e118));
    let _e121 = currentNdc;
    param_2 = _e121;
    let _e122 = wiredTemporalFinite2_u0028_vf2_u003b((&param_2));
    let _e123 = !(_e122);
    phi_157_ = _e123;
    if !(_e123) {
        let _e125 = previousNdc;
        param_3 = _e125;
        let _e126 = wiredTemporalFinite2_u0028_vf2_u003b((&param_3));
        phi_157_ = !(_e126);
    }
    let _e129 = phi_157_;
    if _e129 {
        return;
    }
    let _e130 = currentNdc;
    currentUv = ((_e130 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e133 = previousNdc;
    previousUv = ((_e133 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e136 = currentUv;
    let _e137 = previousUv;
    velocity = (_e136 - _e137);
    let _e139 = velocity;
    param_4 = _e139;
    let _e140 = wiredTemporalFinite2_u0028_vf2_u003b((&param_4));
    if !(_e140) {
        return;
    }
    let _e142 = velocity;
    out_temporal_velocity = _e142;
    let _e143 = (*coverageConfidence);
    out_temporal_validity = clamp(_e143, 0f, 1f);
    return;
}

fn wiredTemporalAtestConfidence_u0028_f1_u003b(alpha: ptr<function, f32>) -> f32 {
    var span: f32;

    if override_type_3_ {
        span = max(max(alpha_test_value, (1f - alpha_test_value)), 0.000001f);
        let _e77 = (*alpha);
        let _e80 = span;
        return clamp((abs((_e77 - alpha_test_value)) / _e80), 0f, 1f);
    } else {
        if override_type_3_1 {
            let _e83 = (*alpha);
            return clamp(((alpha_test_value - _e83) / max(alpha_test_value, 0.000001f)), 0f, 1f);
        } else {
            if override_type_3_2 {
                let _e88 = (*alpha);
                return clamp(((_e88 - alpha_test_value) / max((1f - alpha_test_value), 0.000001f)), 0f, 1f);
            }
        }
    }
    return 1f;
}

fn wired_advanced_fog_enabled_u0028_() -> bool {
    var fogType: i32;

    let _e75 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e75 + 0.5f));
    let _e80 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e82 = fogType;
    let _e85 = fogType;
    return (((_e80 > 0.5f) && (_e82 >= 1i)) && (_e85 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e75 = wired_advanced_fog_enabled_u0028_();
    if !(_e75) {
        return 0f;
    }
    let _e78 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e78, 0.000001f));
    let _e83 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e83 + 0.5f));
    let _e86 = fogType_1;
    if (_e86 == 1i) {
        let _e90 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e90 <= 0f) {
            return 0f;
        }
        let _e92 = viewDepth;
        let _e95 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e92 / _e95), 0f, 1f);
    }
    let _e100 = unnamed.advancedFogColorDensity[3u];
    let _e102 = viewDepth;
    opticalDepth = (max(_e100, 0f) * _e102);
    let _e104 = fogType_1;
    if (_e104 == 2i) {
        let _e106 = opticalDepth;
        return clamp((1f - exp(-(_e106))), 0f, 1f);
    }
    let _e111 = opticalDepth;
    let _e112 = opticalDepth;
    return clamp((1f - exp(-((_e111 * _e112)))), 0f, 1f);
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e76 = (*c);
    (*c) = max(_e76, vec3<f32>(0f, 0f, 0f));
    let _e78 = (*c);
    cutoff = (_e78 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e80 = (*c);
    lo = (_e80 / vec3(12.92f));
    let _e83 = (*c);
    hi = pow(((_e83 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e88 = hi;
    let _e89 = lo;
    let _e90 = cutoff;
    return mix(_e88, _e89, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e90));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param_5: vec3<f32>;

    let _e77 = (*role);
    let _e79 = (*role);
    let _e84 = unnamed.packed_indices[(_e77 / 4u)][(_e79 % 4u)];
    let _e87 = (*role);
    let _e89 = (*role);
    let _e94 = unnamed.packed_indices[(_e87 / 4u)][(_e89 % 4u)];
    let _e99 = (*uv);
    let _e100 = textureSample(wired_bindless_images[(_e84 & 4095u)], wired_bindless_samplers[((_e94 >> bitcast<u32>(12i)) & 255u)], _e99);
    c_1 = _e100;
    let _e101 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e101))) == 0i) {
        let _e106 = c_1;
        param_5 = _e106.xyz;
        let _e108 = sRGBToLinear_u0028_vf3_u003b((&param_5));
        c_1[0u] = _e108.x;
        c_1[1u] = _e108.y;
        c_1[2u] = _e108.z;
    }
    let _e115 = (*slot);
    if (lightmap_slot == (_e115 + 1i)) {
        let _e120 = unnamed.worldLightParams[0u];
        let _e121 = c_1;
        let _e123 = (_e121.xyz * _e120);
        c_1[0u] = _e123.x;
        c_1[1u] = _e123.y;
        c_1[2u] = _e123.z;
    }
    let _e130 = c_1;
    return _e130;
}

fn main_1() {
    var frag_color: vec4<f32>;
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

    frag_color[0u] = identity_color;
    frag_color[1u] = identity_color;
    frag_color[2u] = identity_color;
    frag_color[3u] = identity_alpha;
    param_6 = 0u;
    let _e88 = frag_tex_coord0_1;
    param_7 = _e88;
    param_8 = 0i;
    let _e89 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_6), (&param_7), (&param_8));
    let _e90 = frag_color;
    color0_ = (_e89 * _e90);
    let _e92 = color0_;
    base = _e92;
    if override_type_3_3 {
        let _e94 = color0_[3u];
        if (_e94 == alpha_test_value) {
            discard;
        }
    } else {
        if override_type_3_4 {
            let _e97 = color0_[3u];
            if (_e97 >= alpha_test_value) {
                discard;
            }
        } else {
            if override_type_3_5 {
                let _e100 = color0_[3u];
                if (_e100 < alpha_test_value) {
                    discard;
                }
            }
        }
    }
    let _e102 = color0_;
    base = _e102;
    if override_type_3_6 {
        let _e105 = unnamed.worldLightParams[1u];
        wetness = clamp(_e105, 0f, 1f);
        let _e109 = unnamed.worldLightParams[2u];
        frost = clamp(_e109, 0f, 1f);
        let _e111 = base;
        luminance = dot(_e111.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e114 = wetness;
        let _e116 = base;
        let _e118 = (_e116.xyz * mix(1f, 0.82f, _e114));
        base[0u] = _e118.x;
        base[1u] = _e118.y;
        base[2u] = _e118.z;
        let _e125 = base;
        let _e127 = luminance;
        let _e129 = luminance;
        let _e131 = luminance;
        let _e133 = frost;
        let _e136 = mix(_e125.xyz, vec3<f32>((_e127 * 0.88f), (_e129 * 0.94f), _e131), vec3((_e133 * 0.55f)));
        base[0u] = _e136.x;
        base[1u] = _e136.y;
        base[2u] = _e136.z;
    }
    let _e143 = color0_;
    let _e146 = unnamed.emissionRadiance;
    let _e149 = base;
    let _e151 = (_e149.xyz + (_e143.xyz * _e146.xyz));
    base[0u] = _e151.x;
    base[1u] = _e151.y;
    base[2u] = _e151.z;
    let _e158 = wired_advanced_fog_enabled_u0028_();
    if _e158 {
        let _e159 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e159;
        let _e160 = base;
        let _e163 = unnamed.advancedFogColorDensity;
        let _e165 = fogAmount;
        let _e167 = mix(_e160.xyz, _e163.xyz, vec3(_e165));
        base[0u] = _e167.x;
        base[1u] = _e167.y;
        base[2u] = _e167.z;
    }
    if override_type_3_7 {
        let _e175 = base[3u];
        if (_e175 == 0f) {
            discard;
        }
    } else {
        if override_type_3_8 {
            let _e177 = base;
            let _e179 = base;
            if (dot(_e177.xyz, _e179.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e183 = base;
    out_color = _e183;
    let _e185 = color0_[3u];
    param_9 = _e185;
    let _e186 = wiredTemporalAtestConfidence_u0028_f1_u003b((&param_9));
    param_10 = _e186;
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
