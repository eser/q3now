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
var<private> frag_color0In_1: vec4<f32>;
var<private> frag_tex_coord0_1: vec2<f32>;
var<private> out_color: vec4<f32>;

fn wiredTemporalFinite2_u0028_vf2_u003b(value: ptr<function, vec2<f32>>) -> bool {
    var phi_83_: bool;

    let _e75 = (*value);
    let _e76 = (*value);
    let _e78 = all((_e75 == _e76));
    phi_83_ = _e78;
    if _e78 {
        let _e79 = (*value);
        phi_83_ = all((abs(_e79) <= vec2<f32>(0x1.fffffep+127f, 0x1.fffffep+127f)));
    }
    let _e84 = phi_83_;
    return _e84;
}

fn wiredTemporalFinite4_u0028_vf4_u003b(value_1: ptr<function, vec4<f32>>) -> bool {
    var phi_68_: bool;

    let _e75 = (*value_1);
    let _e76 = (*value_1);
    let _e78 = all((_e75 == _e76));
    phi_68_ = _e78;
    if _e78 {
        let _e79 = (*value_1);
        phi_68_ = all((abs(_e79) <= vec4<f32>(0x1.fffffep+127f, 0x1.fffffep+127f, 0x1.fffffep+127f, 0x1.fffffep+127f)));
    }
    let _e84 = phi_68_;
    return _e84;
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
    let _e86 = temporalOutcome_1;
    let _e87 = (_e86 != 1u);
    phi_107_ = _e87;
    if !(_e87) {
        let _e89 = temporalCurrentClip_1;
        param = _e89;
        let _e90 = wiredTemporalFinite4_u0028_vf4_u003b((&param));
        phi_107_ = !(_e90);
    }
    let _e93 = phi_107_;
    phi_116_ = _e93;
    if !(_e93) {
        let _e95 = temporalPreviousClip_1;
        param_1 = _e95;
        let _e96 = wiredTemporalFinite4_u0028_vf4_u003b((&param_1));
        phi_116_ = !(_e96);
    }
    let _e99 = phi_116_;
    phi_126_ = _e99;
    if !(_e99) {
        let _e102 = temporalCurrentClip_1[3u];
        phi_126_ = (_e102 <= 0.000001f);
    }
    let _e105 = phi_126_;
    phi_133_ = _e105;
    if !(_e105) {
        let _e108 = temporalPreviousClip_1[3u];
        phi_133_ = (_e108 <= 0.000001f);
    }
    let _e111 = phi_133_;
    if _e111 {
        return;
    }
    let _e112 = temporalCurrentClip_1;
    let _e115 = temporalCurrentClip_1[3u];
    currentNdc = (_e112.xy / vec2(_e115));
    let _e118 = temporalPreviousClip_1;
    let _e121 = temporalPreviousClip_1[3u];
    previousNdc = (_e118.xy / vec2(_e121));
    let _e124 = currentNdc;
    param_2 = _e124;
    let _e125 = wiredTemporalFinite2_u0028_vf2_u003b((&param_2));
    let _e126 = !(_e125);
    phi_162_ = _e126;
    if !(_e126) {
        let _e128 = previousNdc;
        param_3 = _e128;
        let _e129 = wiredTemporalFinite2_u0028_vf2_u003b((&param_3));
        phi_162_ = !(_e129);
    }
    let _e132 = phi_162_;
    if _e132 {
        return;
    }
    let _e133 = currentNdc;
    currentUv = ((_e133 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e136 = previousNdc;
    previousUv = ((_e136 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e139 = currentUv;
    let _e140 = previousUv;
    velocity = (_e139 - _e140);
    let _e142 = velocity;
    param_4 = _e142;
    let _e143 = wiredTemporalFinite2_u0028_vf2_u003b((&param_4));
    if !(_e143) {
        return;
    }
    let _e145 = velocity;
    out_temporal_velocity = _e145;
    let _e146 = (*coverageConfidence);
    out_temporal_validity = vec4<f32>(clamp(_e146, 0f, 1f), 0f, 0f, 1f);
    return;
}

fn wiredTemporalAtestConfidence_u0028_f1_u003b(alpha: ptr<function, f32>) -> f32 {
    var span: f32;

    if override_type_3_ {
        span = max(max(alpha_test_value, (1f - alpha_test_value)), 0.000001f);
        let _e79 = (*alpha);
        let _e82 = span;
        return clamp((abs((_e79 - alpha_test_value)) / _e82), 0f, 1f);
    } else {
        if override_type_3_1 {
            let _e85 = (*alpha);
            return clamp(((alpha_test_value - _e85) / max(alpha_test_value, 0.000001f)), 0f, 1f);
        } else {
            if override_type_3_2 {
                let _e90 = (*alpha);
                return clamp(((_e90 - alpha_test_value) / max((1f - alpha_test_value), 0.000001f)), 0f, 1f);
            }
        }
    }
    return 1f;
}

fn wired_advanced_fog_enabled_u0028_() -> bool {
    var fogType: i32;

    let _e77 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e77 + 0.5f));
    let _e82 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e84 = fogType;
    let _e87 = fogType;
    return (((_e82 > 0.5f) && (_e84 >= 1i)) && (_e87 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e77 = wired_advanced_fog_enabled_u0028_();
    if !(_e77) {
        return 0f;
    }
    let _e80 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e80, 0.000001f));
    let _e85 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e85 + 0.5f));
    let _e88 = fogType_1;
    if (_e88 == 1i) {
        let _e92 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e92 <= 0f) {
            return 0f;
        }
        let _e94 = viewDepth;
        let _e97 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e94 / _e97), 0f, 1f);
    }
    let _e102 = unnamed.advancedFogColorDensity[3u];
    let _e104 = viewDepth;
    opticalDepth = (max(_e102, 0f) * _e104);
    let _e106 = fogType_1;
    if (_e106 == 2i) {
        let _e108 = opticalDepth;
        return clamp((1f - exp(-(_e108))), 0f, 1f);
    }
    let _e113 = opticalDepth;
    let _e114 = opticalDepth;
    return clamp((1f - exp(-((_e113 * _e114)))), 0f, 1f);
}

fn wired_boost_legacy_lightmap_u0028_vf3_u003b(rgb: ptr<function, vec3<f32>>) -> vec3<f32> {
    var boosted: vec3<f32>;
    var peak: f32;

    let _e77 = (*rgb);
    let _e80 = unnamed.worldLightParams[0u];
    boosted = (_e77 * _e80);
    let _e83 = boosted[0u];
    let _e85 = boosted[1u];
    let _e87 = boosted[2u];
    peak = max(_e83, max(_e85, _e87));
    let _e90 = peak;
    if (_e90 > 1f) {
        let _e92 = peak;
        let _e93 = boosted;
        boosted = (_e93 / vec3(_e92));
    }
    let _e96 = boosted;
    return _e96;
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e78 = (*c);
    (*c) = max(_e78, vec3<f32>(0f, 0f, 0f));
    let _e80 = (*c);
    cutoff = (_e80 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e82 = (*c);
    lo = (_e82 / vec3(12.92f));
    let _e85 = (*c);
    hi = pow(((_e85 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e90 = hi;
    let _e91 = lo;
    let _e92 = cutoff;
    return mix(_e90, _e91, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e92));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param_5: vec3<f32>;
    var param_6: vec3<f32>;

    let _e80 = (*role);
    let _e82 = (*role);
    let _e87 = unnamed.packed_indices[(_e80 / 4u)][(_e82 % 4u)];
    let _e90 = (*role);
    let _e92 = (*role);
    let _e97 = unnamed.packed_indices[(_e90 / 4u)][(_e92 % 4u)];
    let _e102 = (*uv);
    let _e103 = textureSample(wired_bindless_images[(_e87 & 4095u)], wired_bindless_samplers[((_e97 >> bitcast<u32>(12i)) & 255u)], _e102);
    c_1 = _e103;
    let _e104 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e104))) == 0i) {
        let _e109 = c_1;
        param_5 = _e109.xyz;
        let _e111 = sRGBToLinear_u0028_vf3_u003b((&param_5));
        c_1[0u] = _e111.x;
        c_1[1u] = _e111.y;
        c_1[2u] = _e111.z;
    }
    let _e118 = (*slot);
    if (lightmap_slot == (_e118 + 1i)) {
        let _e121 = c_1;
        param_6 = _e121.xyz;
        let _e123 = wired_boost_legacy_lightmap_u0028_vf3_u003b((&param_6));
        c_1[0u] = _e123.x;
        c_1[1u] = _e123.y;
        c_1[2u] = _e123.z;
    }
    let _e130 = c_1;
    return _e130;
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
    var param_13: vec4<f32>;

    let _e89 = frag_color0In_1;
    param_7 = _e89.xyz;
    let _e91 = sRGBToLinear_u0028_vf3_u003b((&param_7));
    let _e93 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e91.x, _e91.y, _e91.z, _e93);
    param_8 = 0u;
    let _e98 = frag_tex_coord0_1;
    param_9 = _e98;
    param_10 = 0i;
    let _e99 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_8), (&param_9), (&param_10));
    sampledColor0_ = _e99;
    let _e100 = sampledColor0_;
    let _e101 = frag_color0_;
    color0_ = (_e100 * _e101);
    let _e103 = color0_;
    base = _e103;
    if override_type_3_3 {
        let _e105 = color0_[3u];
        if (_e105 == alpha_test_value) {
            discard;
        }
    } else {
        if override_type_3_4 {
            let _e108 = color0_[3u];
            if (_e108 >= alpha_test_value) {
                discard;
            }
        } else {
            if override_type_3_5 {
                let _e111 = color0_[3u];
                if (_e111 < alpha_test_value) {
                    discard;
                }
            }
        }
    }
    let _e113 = color0_;
    base = _e113;
    if override_type_3_6 {
        let _e116 = unnamed.worldLightParams[1u];
        wetness = clamp(_e116, 0f, 1f);
        let _e120 = unnamed.worldLightParams[2u];
        frost = clamp(_e120, 0f, 1f);
        let _e122 = base;
        luminance = dot(_e122.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e125 = wetness;
        let _e127 = base;
        let _e129 = (_e127.xyz * mix(1f, 0.82f, _e125));
        base[0u] = _e129.x;
        base[1u] = _e129.y;
        base[2u] = _e129.z;
        let _e136 = base;
        let _e138 = luminance;
        let _e140 = luminance;
        let _e142 = luminance;
        let _e144 = frost;
        let _e147 = mix(_e136.xyz, vec3<f32>((_e138 * 0.88f), (_e140 * 0.94f), _e142), vec3((_e144 * 0.55f)));
        base[0u] = _e147.x;
        base[1u] = _e147.y;
        base[2u] = _e147.z;
    }
    let _e156 = unnamed.materialFactors[0u];
    if (_e156 >= 0f) {
        let _e159 = unnamed.materialBaseColor;
        let _e160 = base;
        base = (_e160 * _e159);
    }
    let _e162 = color0_;
    let _e165 = unnamed.emissionRadiance;
    let _e168 = base;
    let _e170 = (_e168.xyz + (_e162.xyz * _e165.xyz));
    base[0u] = _e170.x;
    base[1u] = _e170.y;
    base[2u] = _e170.z;
    let _e177 = wired_advanced_fog_enabled_u0028_();
    if _e177 {
        let _e178 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e178;
        let _e179 = base;
        let _e182 = unnamed.advancedFogColorDensity;
        let _e184 = fogAmount;
        let _e186 = mix(_e179.xyz, _e182.xyz, vec3(_e184));
        base[0u] = _e186.x;
        base[1u] = _e186.y;
        base[2u] = _e186.z;
    }
    if override_type_3_7 {
        let _e194 = base[3u];
        if (_e194 == 0f) {
            discard;
        }
    } else {
        if override_type_3_8 {
            let _e196 = base;
            let _e198 = base;
            if (dot(_e196.xyz, _e198.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e202 = base;
    out_color = _e202;
    let _e204 = color0_[3u];
    param_11 = _e204;
    let _e205 = wiredTemporalAtestConfidence_u0028_f1_u003b((&param_11));
    param_12 = _e205;
    let _e206 = base;
    param_13 = _e206;
    wiredTemporalWriteAux_u0028_f1_u003b_vf4_u003b((&param_12), (&param_13));
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
