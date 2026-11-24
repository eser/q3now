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
    @location(2) member_1: vec4<f32>,
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
var<private> out_temporal_validity: vec4<f32>;
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
    var phi_83_: bool;

    let _e76 = (*value);
    let _e77 = (*value);
    let _e79 = all((_e76 == _e77));
    phi_83_ = _e79;
    if _e79 {
        let _e80 = (*value);
        phi_83_ = all((abs(_e80) <= vec2<f32>(0x1.fffffep+127f, 0x1.fffffep+127f)));
    }
    let _e85 = phi_83_;
    return _e85;
}

fn wiredTemporalFinite4_u0028_vf4_u003b(value_1: ptr<function, vec4<f32>>) -> bool {
    var phi_68_: bool;

    let _e76 = (*value_1);
    let _e77 = (*value_1);
    let _e79 = all((_e76 == _e77));
    phi_68_ = _e79;
    if _e79 {
        let _e80 = (*value_1);
        phi_68_ = all((abs(_e80) <= vec4<f32>(0x1.fffffep+127f, 0x1.fffffep+127f, 0x1.fffffep+127f, 0x1.fffffep+127f)));
    }
    let _e85 = phi_68_;
    return _e85;
}

fn wiredTemporalWriteAux_u0028_f1_u003b_vf4_u003b(coverageConfidence: ptr<function, f32>, sceneColor: ptr<function, vec4<f32>>) {
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
    var phi_107_: bool;
    var phi_116_: bool;
    var phi_126_: bool;
    var phi_133_: bool;
    var phi_162_: bool;

    out_temporal_velocity = vec2<f32>(0f, 0f);
    out_temporal_validity = vec4<f32>(0f, 0f, 0f, 0f);
    let _e87 = temporalOutcome_1;
    let _e88 = (_e87 != 1u);
    phi_107_ = _e88;
    if !(_e88) {
        let _e90 = temporalCurrentClip_1;
        param = _e90;
        let _e91 = wiredTemporalFinite4_u0028_vf4_u003b((&param));
        phi_107_ = !(_e91);
    }
    let _e94 = phi_107_;
    phi_116_ = _e94;
    if !(_e94) {
        let _e96 = temporalPreviousClip_1;
        param_1 = _e96;
        let _e97 = wiredTemporalFinite4_u0028_vf4_u003b((&param_1));
        phi_116_ = !(_e97);
    }
    let _e100 = phi_116_;
    phi_126_ = _e100;
    if !(_e100) {
        let _e103 = temporalCurrentClip_1[3u];
        phi_126_ = (_e103 <= 0.000001f);
    }
    let _e106 = phi_126_;
    phi_133_ = _e106;
    if !(_e106) {
        let _e109 = temporalPreviousClip_1[3u];
        phi_133_ = (_e109 <= 0.000001f);
    }
    let _e112 = phi_133_;
    if _e112 {
        return;
    }
    let _e113 = temporalCurrentClip_1;
    let _e116 = temporalCurrentClip_1[3u];
    currentNdc = (_e113.xy / vec2(_e116));
    let _e119 = temporalPreviousClip_1;
    let _e122 = temporalPreviousClip_1[3u];
    previousNdc = (_e119.xy / vec2(_e122));
    let _e125 = currentNdc;
    param_2 = _e125;
    let _e126 = wiredTemporalFinite2_u0028_vf2_u003b((&param_2));
    let _e127 = !(_e126);
    phi_162_ = _e127;
    if !(_e127) {
        let _e129 = previousNdc;
        param_3 = _e129;
        let _e130 = wiredTemporalFinite2_u0028_vf2_u003b((&param_3));
        phi_162_ = !(_e130);
    }
    let _e133 = phi_162_;
    if _e133 {
        return;
    }
    let _e134 = currentNdc;
    currentUv = ((_e134 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e137 = previousNdc;
    previousUv = ((_e137 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e140 = currentUv;
    let _e141 = previousUv;
    velocity = (_e140 - _e141);
    let _e143 = velocity;
    param_4 = _e143;
    let _e144 = wiredTemporalFinite2_u0028_vf2_u003b((&param_4));
    if !(_e144) {
        return;
    }
    let _e146 = velocity;
    out_temporal_velocity = _e146;
    let _e147 = (*coverageConfidence);
    out_temporal_validity = vec4<f32>(clamp(_e147, 0f, 1f), 0f, 0f, 1f);
    return;
}

fn wiredTemporalAtestConfidence_u0028_f1_u003b(alpha: ptr<function, f32>) -> f32 {
    var span: f32;

    if override_type_3_ {
        span = max(max(alpha_test_value, (1f - alpha_test_value)), 0.000001f);
        let _e80 = (*alpha);
        let _e83 = span;
        return clamp((abs((_e80 - alpha_test_value)) / _e83), 0f, 1f);
    } else {
        if override_type_3_1 {
            let _e86 = (*alpha);
            return clamp(((alpha_test_value - _e86) / max(alpha_test_value, 0.000001f)), 0f, 1f);
        } else {
            if override_type_3_2 {
                let _e91 = (*alpha);
                return clamp(((_e91 - alpha_test_value) / max((1f - alpha_test_value), 0.000001f)), 0f, 1f);
            }
        }
    }
    return 1f;
}

fn wired_advanced_fog_enabled_u0028_() -> bool {
    var fogType: i32;

    let _e78 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e78 + 0.5f));
    let _e83 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e85 = fogType;
    let _e88 = fogType;
    return (((_e83 > 0.5f) && (_e85 >= 1i)) && (_e88 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e78 = wired_advanced_fog_enabled_u0028_();
    if !(_e78) {
        return 0f;
    }
    let _e81 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e81, 0.000001f));
    let _e86 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e86 + 0.5f));
    let _e89 = fogType_1;
    if (_e89 == 1i) {
        let _e93 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e93 <= 0f) {
            return 0f;
        }
        let _e95 = viewDepth;
        let _e98 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e95 / _e98), 0f, 1f);
    }
    let _e103 = unnamed.advancedFogColorDensity[3u];
    let _e105 = viewDepth;
    opticalDepth = (max(_e103, 0f) * _e105);
    let _e107 = fogType_1;
    if (_e107 == 2i) {
        let _e109 = opticalDepth;
        return clamp((1f - exp(-(_e109))), 0f, 1f);
    }
    let _e114 = opticalDepth;
    let _e115 = opticalDepth;
    return clamp((1f - exp(-((_e114 * _e115)))), 0f, 1f);
}

fn wired_boost_legacy_lightmap_u0028_vf3_u003b(rgb: ptr<function, vec3<f32>>) -> vec3<f32> {
    var boosted: vec3<f32>;
    var peak: f32;

    let _e78 = (*rgb);
    let _e81 = unnamed.worldLightParams[0u];
    boosted = (_e78 * _e81);
    let _e84 = boosted[0u];
    let _e86 = boosted[1u];
    let _e88 = boosted[2u];
    peak = max(_e84, max(_e86, _e88));
    let _e91 = peak;
    if (_e91 > 1f) {
        let _e93 = peak;
        let _e94 = boosted;
        boosted = (_e94 / vec3(_e93));
    }
    let _e97 = boosted;
    return _e97;
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e79 = (*c);
    (*c) = max(_e79, vec3<f32>(0f, 0f, 0f));
    let _e81 = (*c);
    cutoff = (_e81 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e83 = (*c);
    lo = (_e83 / vec3(12.92f));
    let _e86 = (*c);
    hi = pow(((_e86 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e91 = hi;
    let _e92 = lo;
    let _e93 = cutoff;
    return mix(_e91, _e92, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e93));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param_5: vec3<f32>;
    var param_6: vec3<f32>;

    let _e81 = (*role);
    let _e83 = (*role);
    let _e88 = unnamed.packed_indices[(_e81 / 4u)][(_e83 % 4u)];
    let _e91 = (*role);
    let _e93 = (*role);
    let _e98 = unnamed.packed_indices[(_e91 / 4u)][(_e93 % 4u)];
    let _e103 = (*uv);
    let _e104 = textureSample(wired_bindless_images[(_e88 & 4095u)], wired_bindless_samplers[((_e98 >> bitcast<u32>(12i)) & 255u)], _e103);
    c_1 = _e104;
    let _e105 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e105))) == 0i) {
        let _e110 = c_1;
        param_5 = _e110.xyz;
        let _e112 = sRGBToLinear_u0028_vf3_u003b((&param_5));
        c_1[0u] = _e112.x;
        c_1[1u] = _e112.y;
        c_1[2u] = _e112.z;
    }
    let _e119 = (*slot);
    if (lightmap_slot == (_e119 + 1i)) {
        let _e122 = c_1;
        param_6 = _e122.xyz;
        let _e124 = wired_boost_legacy_lightmap_u0028_vf3_u003b((&param_6));
        c_1[0u] = _e124.x;
        c_1[1u] = _e124.y;
        c_1[2u] = _e124.z;
    }
    let _e131 = c_1;
    return _e131;
}

fn main_1() {
    var sampledColor0_: vec4<f32>;
    var param_7: u32;
    var param_8: vec2<f32>;
    var param_9: i32;
    var frag_color: vec4<f32>;
    var color0_: vec4<f32>;
    var base: vec4<f32>;
    var wetness: f32;
    var frost: f32;
    var luminance: f32;
    var fogAmount: f32;
    var param_10: f32;
    var param_11: f32;
    var param_12: vec4<f32>;

    param_7 = 0u;
    let _e89 = frag_tex_coord0_1;
    param_8 = _e89;
    param_9 = 0i;
    let _e90 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_7), (&param_8), (&param_9));
    sampledColor0_ = _e90;
    frag_color[0u] = identity_color;
    frag_color[1u] = identity_color;
    frag_color[2u] = identity_color;
    frag_color[3u] = identity_alpha;
    let _e95 = sampledColor0_;
    let _e96 = frag_color;
    color0_ = (_e95 * _e96);
    let _e98 = color0_;
    base = _e98;
    if override_type_3_3 {
        let _e100 = color0_[3u];
        if (_e100 == alpha_test_value) {
            discard;
        }
    } else {
        if override_type_3_4 {
            let _e103 = color0_[3u];
            if (_e103 >= alpha_test_value) {
                discard;
            }
        } else {
            if override_type_3_5 {
                let _e106 = color0_[3u];
                if (_e106 < alpha_test_value) {
                    discard;
                }
            }
        }
    }
    let _e108 = color0_;
    base = _e108;
    if override_type_3_6 {
        let _e111 = unnamed.worldLightParams[1u];
        wetness = clamp(_e111, 0f, 1f);
        let _e115 = unnamed.worldLightParams[2u];
        frost = clamp(_e115, 0f, 1f);
        let _e117 = base;
        luminance = dot(_e117.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e120 = wetness;
        let _e122 = base;
        let _e124 = (_e122.xyz * mix(1f, 0.82f, _e120));
        base[0u] = _e124.x;
        base[1u] = _e124.y;
        base[2u] = _e124.z;
        let _e131 = base;
        let _e133 = luminance;
        let _e135 = luminance;
        let _e137 = luminance;
        let _e139 = frost;
        let _e142 = mix(_e131.xyz, vec3<f32>((_e133 * 0.88f), (_e135 * 0.94f), _e137), vec3((_e139 * 0.55f)));
        base[0u] = _e142.x;
        base[1u] = _e142.y;
        base[2u] = _e142.z;
    }
    let _e151 = unnamed.materialFactors[0u];
    if (_e151 >= 0f) {
        let _e154 = unnamed.materialBaseColor;
        let _e155 = base;
        base = (_e155 * _e154);
    }
    let _e157 = color0_;
    let _e160 = unnamed.emissionRadiance;
    let _e163 = base;
    let _e165 = (_e163.xyz + (_e157.xyz * _e160.xyz));
    base[0u] = _e165.x;
    base[1u] = _e165.y;
    base[2u] = _e165.z;
    let _e172 = wired_advanced_fog_enabled_u0028_();
    if _e172 {
        let _e173 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e173;
        let _e174 = base;
        let _e177 = unnamed.advancedFogColorDensity;
        let _e179 = fogAmount;
        let _e181 = mix(_e174.xyz, _e177.xyz, vec3(_e179));
        base[0u] = _e181.x;
        base[1u] = _e181.y;
        base[2u] = _e181.z;
    }
    if override_type_3_7 {
        let _e189 = base[3u];
        if (_e189 == 0f) {
            discard;
        }
    } else {
        if override_type_3_8 {
            let _e191 = base;
            let _e193 = base;
            if (dot(_e191.xyz, _e193.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e197 = base;
    out_color = _e197;
    let _e199 = color0_[3u];
    param_10 = _e199;
    let _e200 = wiredTemporalAtestConfidence_u0028_f1_u003b((&param_10));
    param_11 = _e200;
    let _e201 = base;
    param_12 = _e201;
    wiredTemporalWriteAux_u0028_f1_u003b_vf4_u003b((&param_11), (&param_12));
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
