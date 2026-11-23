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
    var phi_82_: bool;

    let _e82 = (*value);
    let _e83 = (*value);
    let _e85 = all((_e82 == _e83));
    phi_82_ = _e85;
    if _e85 {
        let _e86 = (*value);
        phi_82_ = all((abs(_e86) <= vec2<f32>(0x1.fffffep+127f, 0x1.fffffep+127f)));
    }
    let _e91 = phi_82_;
    return _e91;
}

fn wiredTemporalFinite4_u0028_vf4_u003b(value_1: ptr<function, vec4<f32>>) -> bool {
    var phi_67_: bool;

    let _e82 = (*value_1);
    let _e83 = (*value_1);
    let _e85 = all((_e82 == _e83));
    phi_67_ = _e85;
    if _e85 {
        let _e86 = (*value_1);
        phi_67_ = all((abs(_e86) <= vec4<f32>(0x1.fffffep+127f, 0x1.fffffep+127f, 0x1.fffffep+127f, 0x1.fffffep+127f)));
    }
    let _e91 = phi_67_;
    return _e91;
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
    let _e92 = temporalOutcome_1;
    let _e93 = (_e92 != 1u);
    phi_105_ = _e93;
    if !(_e93) {
        let _e95 = temporalCurrentClip_1;
        param = _e95;
        let _e96 = wiredTemporalFinite4_u0028_vf4_u003b((&param));
        phi_105_ = !(_e96);
    }
    let _e99 = phi_105_;
    phi_114_ = _e99;
    if !(_e99) {
        let _e101 = temporalPreviousClip_1;
        param_1 = _e101;
        let _e102 = wiredTemporalFinite4_u0028_vf4_u003b((&param_1));
        phi_114_ = !(_e102);
    }
    let _e105 = phi_114_;
    phi_124_ = _e105;
    if !(_e105) {
        let _e108 = temporalCurrentClip_1[3u];
        phi_124_ = (_e108 <= 0.000001f);
    }
    let _e111 = phi_124_;
    phi_131_ = _e111;
    if !(_e111) {
        let _e114 = temporalPreviousClip_1[3u];
        phi_131_ = (_e114 <= 0.000001f);
    }
    let _e117 = phi_131_;
    if _e117 {
        return;
    }
    let _e118 = temporalCurrentClip_1;
    let _e121 = temporalCurrentClip_1[3u];
    currentNdc = (_e118.xy / vec2(_e121));
    let _e124 = temporalPreviousClip_1;
    let _e127 = temporalPreviousClip_1[3u];
    previousNdc = (_e124.xy / vec2(_e127));
    let _e130 = currentNdc;
    param_2 = _e130;
    let _e131 = wiredTemporalFinite2_u0028_vf2_u003b((&param_2));
    let _e132 = !(_e131);
    phi_160_ = _e132;
    if !(_e132) {
        let _e134 = previousNdc;
        param_3 = _e134;
        let _e135 = wiredTemporalFinite2_u0028_vf2_u003b((&param_3));
        phi_160_ = !(_e135);
    }
    let _e138 = phi_160_;
    if _e138 {
        return;
    }
    let _e139 = currentNdc;
    currentUv = ((_e139 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e142 = previousNdc;
    previousUv = ((_e142 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e145 = currentUv;
    let _e146 = previousUv;
    velocity = (_e145 - _e146);
    let _e148 = velocity;
    param_4 = _e148;
    let _e149 = wiredTemporalFinite2_u0028_vf2_u003b((&param_4));
    if !(_e149) {
        return;
    }
    let _e151 = velocity;
    out_temporal_velocity = _e151;
    let _e152 = (*coverageConfidence);
    out_temporal_validity = clamp(_e152, 0f, 1f);
    return;
}

fn wiredTemporalAtestConfidence_u0028_f1_u003b(alpha: ptr<function, f32>) -> f32 {
    var span: f32;

    if override_type_3_ {
        span = max(max(alpha_test_value, (1f - alpha_test_value)), 0.000001f);
        let _e86 = (*alpha);
        let _e89 = span;
        return clamp((abs((_e86 - alpha_test_value)) / _e89), 0f, 1f);
    } else {
        if override_type_3_1 {
            let _e92 = (*alpha);
            return clamp(((alpha_test_value - _e92) / max(alpha_test_value, 0.000001f)), 0f, 1f);
        } else {
            if override_type_3_2 {
                let _e97 = (*alpha);
                return clamp(((_e97 - alpha_test_value) / max((1f - alpha_test_value), 0.000001f)), 0f, 1f);
            }
        }
    }
    return 1f;
}

fn wired_advanced_fog_enabled_u0028_() -> bool {
    var fogType: i32;

    let _e84 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e84 + 0.5f));
    let _e89 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e91 = fogType;
    let _e94 = fogType;
    return (((_e89 > 0.5f) && (_e91 >= 1i)) && (_e94 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e84 = wired_advanced_fog_enabled_u0028_();
    if !(_e84) {
        return 0f;
    }
    let _e87 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e87, 0.000001f));
    let _e92 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e92 + 0.5f));
    let _e95 = fogType_1;
    if (_e95 == 1i) {
        let _e99 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e99 <= 0f) {
            return 0f;
        }
        let _e101 = viewDepth;
        let _e104 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e101 / _e104), 0f, 1f);
    }
    let _e109 = unnamed.advancedFogColorDensity[3u];
    let _e111 = viewDepth;
    opticalDepth = (max(_e109, 0f) * _e111);
    let _e113 = fogType_1;
    if (_e113 == 2i) {
        let _e115 = opticalDepth;
        return clamp((1f - exp(-(_e115))), 0f, 1f);
    }
    let _e120 = opticalDepth;
    let _e121 = opticalDepth;
    return clamp((1f - exp(-((_e120 * _e121)))), 0f, 1f);
}

fn wired_boost_legacy_lightmap_u0028_vf3_u003b(rgb: ptr<function, vec3<f32>>) -> vec3<f32> {
    var boosted: vec3<f32>;
    var peak: f32;

    let _e84 = (*rgb);
    let _e87 = unnamed.worldLightParams[0u];
    boosted = (_e84 * _e87);
    let _e90 = boosted[0u];
    let _e92 = boosted[1u];
    let _e94 = boosted[2u];
    peak = max(_e90, max(_e92, _e94));
    let _e97 = peak;
    if (_e97 > 1f) {
        let _e99 = peak;
        let _e100 = boosted;
        boosted = (_e100 / vec3(_e99));
    }
    let _e103 = boosted;
    return _e103;
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e85 = (*c);
    (*c) = max(_e85, vec3<f32>(0f, 0f, 0f));
    let _e87 = (*c);
    cutoff = (_e87 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e89 = (*c);
    lo = (_e89 / vec3(12.92f));
    let _e92 = (*c);
    hi = pow(((_e92 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e97 = hi;
    let _e98 = lo;
    let _e99 = cutoff;
    return mix(_e97, _e98, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e99));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param_5: vec3<f32>;
    var param_6: vec3<f32>;

    let _e87 = (*role);
    let _e89 = (*role);
    let _e94 = unnamed.packed_indices[(_e87 / 4u)][(_e89 % 4u)];
    let _e97 = (*role);
    let _e99 = (*role);
    let _e104 = unnamed.packed_indices[(_e97 / 4u)][(_e99 % 4u)];
    let _e109 = (*uv);
    let _e110 = textureSample(wired_bindless_images[(_e94 & 4095u)], wired_bindless_samplers[((_e104 >> bitcast<u32>(12i)) & 255u)], _e109);
    c_1 = _e110;
    let _e111 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e111))) == 0i) {
        let _e116 = c_1;
        param_5 = _e116.xyz;
        let _e118 = sRGBToLinear_u0028_vf3_u003b((&param_5));
        c_1[0u] = _e118.x;
        c_1[1u] = _e118.y;
        c_1[2u] = _e118.z;
    }
    let _e125 = (*slot);
    if (lightmap_slot == (_e125 + 1i)) {
        let _e128 = c_1;
        param_6 = _e128.xyz;
        let _e130 = wired_boost_legacy_lightmap_u0028_vf3_u003b((&param_6));
        c_1[0u] = _e130.x;
        c_1[1u] = _e130.y;
        c_1[2u] = _e130.z;
    }
    let _e137 = c_1;
    return _e137;
}

fn main_1() {
    var fog: vec4<f32>;
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

    let _e97 = unnamed.packed_indices[0i][3u];
    let _e103 = unnamed.packed_indices[0i][3u];
    let _e108 = fog_tex_coord_1;
    let _e109 = textureSample(wired_bindless_images[(_e97 & 4095u)], wired_bindless_samplers[((_e103 >> bitcast<u32>(12i)) & 255u)], _e108);
    fog = _e109;
    param_7 = 0u;
    let _e110 = frag_tex_coord0_1;
    param_8 = _e110;
    param_9 = 0i;
    let _e111 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_7), (&param_8), (&param_9));
    sampledColor0_ = _e111;
    let _e112 = sampledColor0_;
    color0_ = _e112;
    let _e113 = color0_;
    base = _e113;
    if override_type_3_3 {
        let _e115 = color0_[3u];
        if (_e115 == alpha_test_value) {
            discard;
        }
    } else {
        if override_type_3_4 {
            let _e118 = color0_[3u];
            if (_e118 >= alpha_test_value) {
                discard;
            }
        } else {
            if override_type_3_5 {
                let _e121 = color0_[3u];
                if (_e121 < alpha_test_value) {
                    discard;
                }
            }
        }
    }
    let _e123 = color0_;
    base = _e123;
    if override_type_3_6 {
        let _e126 = unnamed.worldLightParams[1u];
        wetness = clamp(_e126, 0f, 1f);
        let _e130 = unnamed.worldLightParams[2u];
        frost = clamp(_e130, 0f, 1f);
        let _e132 = base;
        luminance = dot(_e132.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e135 = wetness;
        let _e137 = base;
        let _e139 = (_e137.xyz * mix(1f, 0.82f, _e135));
        base[0u] = _e139.x;
        base[1u] = _e139.y;
        base[2u] = _e139.z;
        let _e146 = base;
        let _e148 = luminance;
        let _e150 = luminance;
        let _e152 = luminance;
        let _e154 = frost;
        let _e157 = mix(_e146.xyz, vec3<f32>((_e148 * 0.88f), (_e150 * 0.94f), _e152), vec3((_e154 * 0.55f)));
        base[0u] = _e157.x;
        base[1u] = _e157.y;
        base[2u] = _e157.z;
    }
    let _e166 = unnamed.materialFactors[0u];
    if (_e166 >= 0f) {
        let _e169 = unnamed.materialBaseColor;
        let _e170 = base;
        base = (_e170 * _e169);
    }
    let _e172 = color0_;
    let _e175 = unnamed.emissionRadiance;
    let _e178 = base;
    let _e180 = (_e178.xyz + (_e172.xyz * _e175.xyz));
    base[0u] = _e180.x;
    base[1u] = _e180.y;
    base[2u] = _e180.z;
    let _e187 = wired_advanced_fog_enabled_u0028_();
    if _e187 {
        let _e188 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e188;
        if override_type_3_7 {
            let _e189 = fogAmount;
            let _e191 = base;
            let _e193 = (_e191.xyz * (1f - _e189));
            base[0u] = _e193.x;
            base[1u] = _e193.y;
            base[2u] = _e193.z;
        } else {
            if override_type_3_8 {
                let _e200 = fogAmount;
                let _e202 = base;
                base = (_e202 * (1f - _e200));
            } else {
                if override_type_3_9 {
                    let _e204 = fogAmount;
                    let _e207 = base[3u];
                    base[3u] = (_e207 * (1f - _e204));
                } else {
                    let _e210 = base;
                    let _e213 = unnamed.advancedFogColorDensity;
                    let _e215 = fogAmount;
                    let _e217 = mix(_e210.xyz, _e213.xyz, vec3(_e215));
                    base[0u] = _e217.x;
                    base[1u] = _e217.y;
                    base[2u] = _e217.z;
                }
            }
        }
    } else {
        if override_type_3_10 {
            let _e224 = base;
            let _e227 = fog[3u];
            let _e229 = (_e224.xyz * (1f - _e227));
            base[0u] = _e229.x;
            base[1u] = _e229.y;
            base[2u] = _e229.z;
        } else {
            if override_type_3_11 {
                let _e236 = base;
                let _e238 = fog[3u];
                base = (_e236 * (1f - _e238));
            } else {
                if override_type_3_12 {
                    let _e242 = base[3u];
                    let _e244 = fog[3u];
                    base[3u] = (_e242 * (1f - _e244));
                } else {
                    let _e248 = base;
                    let _e249 = fog;
                    let _e251 = unnamed.fogColor;
                    let _e254 = fog[3u];
                    base = mix(_e248, (_e249 * _e251), vec4(_e254));
                }
            }
        }
    }
    if override_type_3_13 {
        let _e258 = base[3u];
        if (_e258 == 0f) {
            discard;
        }
    } else {
        if override_type_3_14 {
            let _e260 = base;
            let _e262 = base;
            if (dot(_e260.xyz, _e262.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e266 = base;
    out_color = _e266;
    let _e268 = color0_[3u];
    param_10 = _e268;
    let _e269 = wiredTemporalAtestConfidence_u0028_f1_u003b((&param_10));
    param_11 = _e269;
    wiredTemporalWriteAux_u0028_f1_u003b((&param_11));
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
