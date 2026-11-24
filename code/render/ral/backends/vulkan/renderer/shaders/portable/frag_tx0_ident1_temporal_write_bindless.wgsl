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

    let _e74 = (*value);
    let _e75 = (*value);
    let _e77 = all((_e74 == _e75));
    phi_83_ = _e77;
    if _e77 {
        let _e78 = (*value);
        phi_83_ = all((abs(_e78) <= vec2<f32>(0x1.fffffep+127f, 0x1.fffffep+127f)));
    }
    let _e83 = phi_83_;
    return _e83;
}

fn wiredTemporalFinite4_u0028_vf4_u003b(value_1: ptr<function, vec4<f32>>) -> bool {
    var phi_68_: bool;

    let _e74 = (*value_1);
    let _e75 = (*value_1);
    let _e77 = all((_e74 == _e75));
    phi_68_ = _e77;
    if _e77 {
        let _e78 = (*value_1);
        phi_68_ = all((abs(_e78) <= vec4<f32>(0x1.fffffep+127f, 0x1.fffffep+127f, 0x1.fffffep+127f, 0x1.fffffep+127f)));
    }
    let _e83 = phi_68_;
    return _e83;
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
    let _e85 = temporalOutcome_1;
    let _e86 = (_e85 != 1u);
    phi_107_ = _e86;
    if !(_e86) {
        let _e88 = temporalCurrentClip_1;
        param = _e88;
        let _e89 = wiredTemporalFinite4_u0028_vf4_u003b((&param));
        phi_107_ = !(_e89);
    }
    let _e92 = phi_107_;
    phi_116_ = _e92;
    if !(_e92) {
        let _e94 = temporalPreviousClip_1;
        param_1 = _e94;
        let _e95 = wiredTemporalFinite4_u0028_vf4_u003b((&param_1));
        phi_116_ = !(_e95);
    }
    let _e98 = phi_116_;
    phi_126_ = _e98;
    if !(_e98) {
        let _e101 = temporalCurrentClip_1[3u];
        phi_126_ = (_e101 <= 0.000001f);
    }
    let _e104 = phi_126_;
    phi_133_ = _e104;
    if !(_e104) {
        let _e107 = temporalPreviousClip_1[3u];
        phi_133_ = (_e107 <= 0.000001f);
    }
    let _e110 = phi_133_;
    if _e110 {
        return;
    }
    let _e111 = temporalCurrentClip_1;
    let _e114 = temporalCurrentClip_1[3u];
    currentNdc = (_e111.xy / vec2(_e114));
    let _e117 = temporalPreviousClip_1;
    let _e120 = temporalPreviousClip_1[3u];
    previousNdc = (_e117.xy / vec2(_e120));
    let _e123 = currentNdc;
    param_2 = _e123;
    let _e124 = wiredTemporalFinite2_u0028_vf2_u003b((&param_2));
    let _e125 = !(_e124);
    phi_162_ = _e125;
    if !(_e125) {
        let _e127 = previousNdc;
        param_3 = _e127;
        let _e128 = wiredTemporalFinite2_u0028_vf2_u003b((&param_3));
        phi_162_ = !(_e128);
    }
    let _e131 = phi_162_;
    if _e131 {
        return;
    }
    let _e132 = currentNdc;
    currentUv = ((_e132 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e135 = previousNdc;
    previousUv = ((_e135 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e138 = currentUv;
    let _e139 = previousUv;
    velocity = (_e138 - _e139);
    let _e141 = velocity;
    param_4 = _e141;
    let _e142 = wiredTemporalFinite2_u0028_vf2_u003b((&param_4));
    if !(_e142) {
        return;
    }
    let _e144 = velocity;
    out_temporal_velocity = _e144;
    let _e145 = (*coverageConfidence);
    out_temporal_validity = vec4<f32>(clamp(_e145, 0f, 1f), 0f, 0f, 1f);
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
    var sampledColor0_: vec4<f32>;
    var param_7: u32;
    var param_8: vec2<f32>;
    var param_9: i32;
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
    let _e86 = frag_tex_coord0_1;
    param_8 = _e86;
    param_9 = 0i;
    let _e87 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_7), (&param_8), (&param_9));
    sampledColor0_ = _e87;
    let _e88 = sampledColor0_;
    color0_ = _e88;
    let _e89 = color0_;
    base = _e89;
    if override_type_3_3 {
        let _e91 = color0_[3u];
        if (_e91 == alpha_test_value) {
            discard;
        }
    } else {
        if override_type_3_4 {
            let _e94 = color0_[3u];
            if (_e94 >= alpha_test_value) {
                discard;
            }
        } else {
            if override_type_3_5 {
                let _e97 = color0_[3u];
                if (_e97 < alpha_test_value) {
                    discard;
                }
            }
        }
    }
    let _e99 = color0_;
    base = _e99;
    if override_type_3_6 {
        let _e102 = unnamed.worldLightParams[1u];
        wetness = clamp(_e102, 0f, 1f);
        let _e106 = unnamed.worldLightParams[2u];
        frost = clamp(_e106, 0f, 1f);
        let _e108 = base;
        luminance = dot(_e108.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e111 = wetness;
        let _e113 = base;
        let _e115 = (_e113.xyz * mix(1f, 0.82f, _e111));
        base[0u] = _e115.x;
        base[1u] = _e115.y;
        base[2u] = _e115.z;
        let _e122 = base;
        let _e124 = luminance;
        let _e126 = luminance;
        let _e128 = luminance;
        let _e130 = frost;
        let _e133 = mix(_e122.xyz, vec3<f32>((_e124 * 0.88f), (_e126 * 0.94f), _e128), vec3((_e130 * 0.55f)));
        base[0u] = _e133.x;
        base[1u] = _e133.y;
        base[2u] = _e133.z;
    }
    let _e142 = unnamed.materialFactors[0u];
    if (_e142 >= 0f) {
        let _e145 = unnamed.materialBaseColor;
        let _e146 = base;
        base = (_e146 * _e145);
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
    let _e192 = base;
    param_12 = _e192;
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
