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
@id(10) override acff: i32 = 0i;
override override_type_3_7: bool = (acff == 1i);
override override_type_3_8: bool = (acff == 2i);
override override_type_3_9: bool = (acff == 3i);
override override_type_3_10: bool = (acff == 1i);
override override_type_3_11: bool = (acff == 2i);
override override_type_3_12: bool = (acff == 3i);
@id(7) override discard_mode: i32 = 0i;
override override_type_3_13: bool = (discard_mode == 1i);
override override_type_3_14: bool = (discard_mode == 2i);
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
var<private> fog_tex_coord_1: vec2<f32>;
var<private> frag_tex_coord0_1: vec2<f32>;
var<private> out_color: vec4<f32>;

fn wiredTemporalFinite2_u0028_vf2_u003b(value: ptr<function, vec2<f32>>) -> bool {
    var phi_79_: bool;

    let _e80 = (*value);
    let _e81 = (*value);
    let _e83 = all((_e80 == _e81));
    phi_79_ = _e83;
    if _e83 {
        let _e84 = (*value);
        phi_79_ = all((abs(_e84) <= vec2<f32>(340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f)));
    }
    let _e89 = phi_79_;
    return _e89;
}

fn wiredTemporalFinite4_u0028_vf4_u003b(value_1: ptr<function, vec4<f32>>) -> bool {
    var phi_64_: bool;

    let _e80 = (*value_1);
    let _e81 = (*value_1);
    let _e83 = all((_e80 == _e81));
    phi_64_ = _e83;
    if _e83 {
        let _e84 = (*value_1);
        phi_64_ = all((abs(_e84) <= vec4<f32>(340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f)));
    }
    let _e89 = phi_64_;
    return _e89;
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
    let _e90 = temporalOutcome_1;
    let _e91 = (_e90 != 1u);
    phi_102_ = _e91;
    if !(_e91) {
        let _e93 = temporalCurrentClip_1;
        param = _e93;
        let _e94 = wiredTemporalFinite4_u0028_vf4_u003b((&param));
        phi_102_ = !(_e94);
    }
    let _e97 = phi_102_;
    phi_111_ = _e97;
    if !(_e97) {
        let _e99 = temporalPreviousClip_1;
        param_1 = _e99;
        let _e100 = wiredTemporalFinite4_u0028_vf4_u003b((&param_1));
        phi_111_ = !(_e100);
    }
    let _e103 = phi_111_;
    phi_121_ = _e103;
    if !(_e103) {
        let _e106 = temporalCurrentClip_1[3u];
        phi_121_ = (_e106 <= 0.000001f);
    }
    let _e109 = phi_121_;
    phi_128_ = _e109;
    if !(_e109) {
        let _e112 = temporalPreviousClip_1[3u];
        phi_128_ = (_e112 <= 0.000001f);
    }
    let _e115 = phi_128_;
    if _e115 {
        return;
    }
    let _e116 = temporalCurrentClip_1;
    let _e119 = temporalCurrentClip_1[3u];
    currentNdc = (_e116.xy / vec2(_e119));
    let _e122 = temporalPreviousClip_1;
    let _e125 = temporalPreviousClip_1[3u];
    previousNdc = (_e122.xy / vec2(_e125));
    let _e128 = currentNdc;
    param_2 = _e128;
    let _e129 = wiredTemporalFinite2_u0028_vf2_u003b((&param_2));
    let _e130 = !(_e129);
    phi_157_ = _e130;
    if !(_e130) {
        let _e132 = previousNdc;
        param_3 = _e132;
        let _e133 = wiredTemporalFinite2_u0028_vf2_u003b((&param_3));
        phi_157_ = !(_e133);
    }
    let _e136 = phi_157_;
    if _e136 {
        return;
    }
    let _e137 = currentNdc;
    currentUv = ((_e137 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e140 = previousNdc;
    previousUv = ((_e140 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e143 = currentUv;
    let _e144 = previousUv;
    velocity = (_e143 - _e144);
    let _e146 = velocity;
    param_4 = _e146;
    let _e147 = wiredTemporalFinite2_u0028_vf2_u003b((&param_4));
    if !(_e147) {
        return;
    }
    let _e149 = velocity;
    out_temporal_velocity = _e149;
    let _e150 = (*coverageConfidence);
    out_temporal_validity = clamp(_e150, 0f, 1f);
    return;
}

fn wiredTemporalAtestConfidence_u0028_f1_u003b(alpha: ptr<function, f32>) -> f32 {
    var span: f32;

    if override_type_3_ {
        span = max(max(alpha_test_value, (1f - alpha_test_value)), 0.000001f);
        let _e84 = (*alpha);
        let _e87 = span;
        return clamp((abs((_e84 - alpha_test_value)) / _e87), 0f, 1f);
    } else {
        if override_type_3_1 {
            let _e90 = (*alpha);
            return clamp(((alpha_test_value - _e90) / max(alpha_test_value, 0.000001f)), 0f, 1f);
        } else {
            if override_type_3_2 {
                let _e95 = (*alpha);
                return clamp(((_e95 - alpha_test_value) / max((1f - alpha_test_value), 0.000001f)), 0f, 1f);
            }
        }
    }
    return 1f;
}

fn wired_advanced_fog_enabled_u0028_() -> bool {
    var fogType: i32;

    let _e82 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e82 + 0.5f));
    let _e87 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e89 = fogType;
    let _e92 = fogType;
    return (((_e87 > 0.5f) && (_e89 >= 1i)) && (_e92 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e82 = wired_advanced_fog_enabled_u0028_();
    if !(_e82) {
        return 0f;
    }
    let _e85 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e85, 0.000001f));
    let _e90 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e90 + 0.5f));
    let _e93 = fogType_1;
    if (_e93 == 1i) {
        let _e97 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e97 <= 0f) {
            return 0f;
        }
        let _e99 = viewDepth;
        let _e102 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e99 / _e102), 0f, 1f);
    }
    let _e107 = unnamed.advancedFogColorDensity[3u];
    let _e109 = viewDepth;
    opticalDepth = (max(_e107, 0f) * _e109);
    let _e111 = fogType_1;
    if (_e111 == 2i) {
        let _e113 = opticalDepth;
        return clamp((1f - exp(-(_e113))), 0f, 1f);
    }
    let _e118 = opticalDepth;
    let _e119 = opticalDepth;
    return clamp((1f - exp(-((_e118 * _e119)))), 0f, 1f);
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e83 = (*c);
    (*c) = max(_e83, vec3<f32>(0f, 0f, 0f));
    let _e85 = (*c);
    cutoff = (_e85 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e87 = (*c);
    lo = (_e87 / vec3(12.92f));
    let _e90 = (*c);
    hi = pow(((_e90 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e95 = hi;
    let _e96 = lo;
    let _e97 = cutoff;
    return mix(_e95, _e96, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e97));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param_5: vec3<f32>;

    let _e84 = (*role);
    let _e86 = (*role);
    let _e91 = unnamed.packed_indices[(_e84 / 4u)][(_e86 % 4u)];
    let _e94 = (*role);
    let _e96 = (*role);
    let _e101 = unnamed.packed_indices[(_e94 / 4u)][(_e96 % 4u)];
    let _e106 = (*uv);
    let _e107 = textureSample(wired_bindless_images[(_e91 & 4095u)], wired_bindless_samplers[((_e101 >> bitcast<u32>(12i)) & 255u)], _e106);
    c_1 = _e107;
    let _e108 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e108))) == 0i) {
        let _e113 = c_1;
        param_5 = _e113.xyz;
        let _e115 = sRGBToLinear_u0028_vf3_u003b((&param_5));
        c_1[0u] = _e115.x;
        c_1[1u] = _e115.y;
        c_1[2u] = _e115.z;
    }
    let _e122 = (*slot);
    if (lightmap_slot == (_e122 + 1i)) {
        let _e127 = unnamed.worldLightParams[0u];
        let _e128 = c_1;
        let _e130 = (_e128.xyz * _e127);
        c_1[0u] = _e130.x;
        c_1[1u] = _e130.y;
        c_1[2u] = _e130.z;
    }
    let _e137 = c_1;
    return _e137;
}

fn main_1() {
    var fog: vec4<f32>;
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

    let _e94 = unnamed.packed_indices[0i][3u];
    let _e100 = unnamed.packed_indices[0i][3u];
    let _e105 = fog_tex_coord_1;
    let _e106 = textureSample(wired_bindless_images[(_e94 & 4095u)], wired_bindless_samplers[((_e100 >> bitcast<u32>(12i)) & 255u)], _e105);
    fog = _e106;
    param_6 = 0u;
    let _e107 = frag_tex_coord0_1;
    param_7 = _e107;
    param_8 = 0i;
    let _e108 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_6), (&param_7), (&param_8));
    let _e110 = unnamed.ent_color0_;
    color0_ = (_e108 * _e110);
    let _e112 = color0_;
    base = _e112;
    if override_type_3_3 {
        let _e114 = color0_[3u];
        if (_e114 == alpha_test_value) {
            discard;
        }
    } else {
        if override_type_3_4 {
            let _e117 = color0_[3u];
            if (_e117 >= alpha_test_value) {
                discard;
            }
        } else {
            if override_type_3_5 {
                let _e120 = color0_[3u];
                if (_e120 < alpha_test_value) {
                    discard;
                }
            }
        }
    }
    let _e122 = color0_;
    base = _e122;
    if override_type_3_6 {
        let _e125 = unnamed.worldLightParams[1u];
        wetness = clamp(_e125, 0f, 1f);
        let _e129 = unnamed.worldLightParams[2u];
        frost = clamp(_e129, 0f, 1f);
        let _e131 = base;
        luminance = dot(_e131.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e134 = wetness;
        let _e136 = base;
        let _e138 = (_e136.xyz * mix(1f, 0.82f, _e134));
        base[0u] = _e138.x;
        base[1u] = _e138.y;
        base[2u] = _e138.z;
        let _e145 = base;
        let _e147 = luminance;
        let _e149 = luminance;
        let _e151 = luminance;
        let _e153 = frost;
        let _e156 = mix(_e145.xyz, vec3<f32>((_e147 * 0.88f), (_e149 * 0.94f), _e151), vec3((_e153 * 0.55f)));
        base[0u] = _e156.x;
        base[1u] = _e156.y;
        base[2u] = _e156.z;
    }
    let _e163 = color0_;
    let _e166 = unnamed.emissionRadiance;
    let _e169 = base;
    let _e171 = (_e169.xyz + (_e163.xyz * _e166.xyz));
    base[0u] = _e171.x;
    base[1u] = _e171.y;
    base[2u] = _e171.z;
    let _e178 = wired_advanced_fog_enabled_u0028_();
    if _e178 {
        let _e179 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e179;
        if override_type_3_7 {
            let _e180 = fogAmount;
            let _e182 = base;
            let _e184 = (_e182.xyz * (1f - _e180));
            base[0u] = _e184.x;
            base[1u] = _e184.y;
            base[2u] = _e184.z;
        } else {
            if override_type_3_8 {
                let _e191 = fogAmount;
                let _e193 = base;
                base = (_e193 * (1f - _e191));
            } else {
                if override_type_3_9 {
                    let _e195 = fogAmount;
                    let _e198 = base[3u];
                    base[3u] = (_e198 * (1f - _e195));
                } else {
                    let _e201 = base;
                    let _e204 = unnamed.advancedFogColorDensity;
                    let _e206 = fogAmount;
                    let _e208 = mix(_e201.xyz, _e204.xyz, vec3(_e206));
                    base[0u] = _e208.x;
                    base[1u] = _e208.y;
                    base[2u] = _e208.z;
                }
            }
        }
    } else {
        if override_type_3_10 {
            let _e215 = base;
            let _e218 = fog[3u];
            let _e220 = (_e215.xyz * (1f - _e218));
            base[0u] = _e220.x;
            base[1u] = _e220.y;
            base[2u] = _e220.z;
        } else {
            if override_type_3_11 {
                let _e227 = base;
                let _e229 = fog[3u];
                base = (_e227 * (1f - _e229));
            } else {
                if override_type_3_12 {
                    let _e233 = base[3u];
                    let _e235 = fog[3u];
                    base[3u] = (_e233 * (1f - _e235));
                } else {
                    let _e239 = base;
                    let _e240 = fog;
                    let _e242 = unnamed.fogColor;
                    let _e245 = fog[3u];
                    base = mix(_e239, (_e240 * _e242), vec4(_e245));
                }
            }
        }
    }
    if override_type_3_13 {
        let _e249 = base[3u];
        if (_e249 == 0f) {
            discard;
        }
    } else {
        if override_type_3_14 {
            let _e251 = base;
            let _e253 = base;
            if (dot(_e251.xyz, _e253.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e257 = base;
    out_color = _e257;
    let _e259 = color0_[3u];
    param_9 = _e259;
    let _e260 = wiredTemporalAtestConfidence_u0028_f1_u003b((&param_9));
    param_10 = _e260;
    wiredTemporalWriteAux_u0028_f1_u003b((&param_10));
    return;
}

@fragment
fn main(@location(12) @interpolate(flat) temporalOutcome: u32, @location(10) temporalCurrentClip: vec4<f32>, @location(11) temporalPreviousClip: vec4<f32>, @builtin(position) gl_FragCoord: vec4<f32>, @location(4) fog_tex_coord: vec2<f32>, @location(1) frag_tex_coord0_: vec2<f32>) -> FragmentOutput {
    temporalOutcome_1 = temporalOutcome;
    temporalCurrentClip_1 = temporalCurrentClip;
    temporalPreviousClip_1 = temporalPreviousClip;
    gl_FragCoord_1 = gl_FragCoord;
    fog_tex_coord_1 = fog_tex_coord;
    frag_tex_coord0_1 = frag_tex_coord0_;
    main_1();
    let _e15 = out_temporal_velocity;
    let _e16 = out_temporal_validity;
    let _e17 = out_color;
    return FragmentOutput(_e15, _e16, _e17);
}
