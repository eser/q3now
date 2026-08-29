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

    let _e82 = (*value);
    let _e83 = (*value);
    let _e85 = all((_e82 == _e83));
    phi_79_ = _e85;
    if _e85 {
        let _e86 = (*value);
        phi_79_ = all((abs(_e86) <= vec2<f32>(340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f)));
    }
    let _e91 = phi_79_;
    return _e91;
}

fn wiredTemporalFinite4_u0028_vf4_u003b(value_1: ptr<function, vec4<f32>>) -> bool {
    var phi_64_: bool;

    let _e82 = (*value_1);
    let _e83 = (*value_1);
    let _e85 = all((_e82 == _e83));
    phi_64_ = _e85;
    if _e85 {
        let _e86 = (*value_1);
        phi_64_ = all((abs(_e86) <= vec4<f32>(340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f)));
    }
    let _e91 = phi_64_;
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
    var phi_102_: bool;
    var phi_111_: bool;
    var phi_121_: bool;
    var phi_128_: bool;
    var phi_157_: bool;

    out_temporal_velocity = vec2<f32>(0f, 0f);
    out_temporal_validity = 0f;
    let _e92 = temporalOutcome_1;
    let _e93 = (_e92 != 1u);
    phi_102_ = _e93;
    if !(_e93) {
        let _e95 = temporalCurrentClip_1;
        param = _e95;
        let _e96 = wiredTemporalFinite4_u0028_vf4_u003b((&param));
        phi_102_ = !(_e96);
    }
    let _e99 = phi_102_;
    phi_111_ = _e99;
    if !(_e99) {
        let _e101 = temporalPreviousClip_1;
        param_1 = _e101;
        let _e102 = wiredTemporalFinite4_u0028_vf4_u003b((&param_1));
        phi_111_ = !(_e102);
    }
    let _e105 = phi_111_;
    phi_121_ = _e105;
    if !(_e105) {
        let _e108 = temporalCurrentClip_1[3u];
        phi_121_ = (_e108 <= 0.000001f);
    }
    let _e111 = phi_121_;
    phi_128_ = _e111;
    if !(_e111) {
        let _e114 = temporalPreviousClip_1[3u];
        phi_128_ = (_e114 <= 0.000001f);
    }
    let _e117 = phi_128_;
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
    phi_157_ = _e132;
    if !(_e132) {
        let _e134 = previousNdc;
        param_3 = _e134;
        let _e135 = wiredTemporalFinite2_u0028_vf2_u003b((&param_3));
        phi_157_ = !(_e135);
    }
    let _e138 = phi_157_;
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

    let _e86 = (*role);
    let _e88 = (*role);
    let _e93 = unnamed.packed_indices[(_e86 / 4u)][(_e88 % 4u)];
    let _e96 = (*role);
    let _e98 = (*role);
    let _e103 = unnamed.packed_indices[(_e96 / 4u)][(_e98 % 4u)];
    let _e108 = (*uv);
    let _e109 = textureSample(wired_bindless_images[(_e93 & 4095u)], wired_bindless_samplers[((_e103 >> bitcast<u32>(12i)) & 255u)], _e108);
    c_1 = _e109;
    let _e110 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e110))) == 0i) {
        let _e115 = c_1;
        param_5 = _e115.xyz;
        let _e117 = sRGBToLinear_u0028_vf3_u003b((&param_5));
        c_1[0u] = _e117.x;
        c_1[1u] = _e117.y;
        c_1[2u] = _e117.z;
    }
    let _e124 = (*slot);
    if (lightmap_slot == (_e124 + 1i)) {
        let _e129 = unnamed.worldLightParams[0u];
        let _e130 = c_1;
        let _e132 = (_e130.xyz * _e129);
        c_1[0u] = _e132.x;
        c_1[1u] = _e132.y;
        c_1[2u] = _e132.z;
    }
    let _e139 = c_1;
    return _e139;
}

fn main_1() {
    var fog: vec4<f32>;
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

    let _e97 = unnamed.packed_indices[0i][3u];
    let _e103 = unnamed.packed_indices[0i][3u];
    let _e108 = fog_tex_coord_1;
    let _e109 = textureSample(wired_bindless_images[(_e97 & 4095u)], wired_bindless_samplers[((_e103 >> bitcast<u32>(12i)) & 255u)], _e108);
    fog = _e109;
    frag_color[0u] = identity_color;
    frag_color[1u] = identity_color;
    frag_color[2u] = identity_color;
    frag_color[3u] = identity_alpha;
    param_6 = 0u;
    let _e114 = frag_tex_coord0_1;
    param_7 = _e114;
    param_8 = 0i;
    let _e115 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_6), (&param_7), (&param_8));
    let _e116 = frag_color;
    color0_ = (_e115 * _e116);
    let _e118 = color0_;
    base = _e118;
    if override_type_3_3 {
        let _e120 = color0_[3u];
        if (_e120 == alpha_test_value) {
            discard;
        }
    } else {
        if override_type_3_4 {
            let _e123 = color0_[3u];
            if (_e123 >= alpha_test_value) {
                discard;
            }
        } else {
            if override_type_3_5 {
                let _e126 = color0_[3u];
                if (_e126 < alpha_test_value) {
                    discard;
                }
            }
        }
    }
    let _e128 = color0_;
    base = _e128;
    if override_type_3_6 {
        let _e131 = unnamed.worldLightParams[1u];
        wetness = clamp(_e131, 0f, 1f);
        let _e135 = unnamed.worldLightParams[2u];
        frost = clamp(_e135, 0f, 1f);
        let _e137 = base;
        luminance = dot(_e137.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e140 = wetness;
        let _e142 = base;
        let _e144 = (_e142.xyz * mix(1f, 0.82f, _e140));
        base[0u] = _e144.x;
        base[1u] = _e144.y;
        base[2u] = _e144.z;
        let _e151 = base;
        let _e153 = luminance;
        let _e155 = luminance;
        let _e157 = luminance;
        let _e159 = frost;
        let _e162 = mix(_e151.xyz, vec3<f32>((_e153 * 0.88f), (_e155 * 0.94f), _e157), vec3((_e159 * 0.55f)));
        base[0u] = _e162.x;
        base[1u] = _e162.y;
        base[2u] = _e162.z;
    }
    let _e169 = color0_;
    let _e172 = unnamed.emissionRadiance;
    let _e175 = base;
    let _e177 = (_e175.xyz + (_e169.xyz * _e172.xyz));
    base[0u] = _e177.x;
    base[1u] = _e177.y;
    base[2u] = _e177.z;
    let _e184 = wired_advanced_fog_enabled_u0028_();
    if _e184 {
        let _e185 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e185;
        if override_type_3_7 {
            let _e186 = fogAmount;
            let _e188 = base;
            let _e190 = (_e188.xyz * (1f - _e186));
            base[0u] = _e190.x;
            base[1u] = _e190.y;
            base[2u] = _e190.z;
        } else {
            if override_type_3_8 {
                let _e197 = fogAmount;
                let _e199 = base;
                base = (_e199 * (1f - _e197));
            } else {
                if override_type_3_9 {
                    let _e201 = fogAmount;
                    let _e204 = base[3u];
                    base[3u] = (_e204 * (1f - _e201));
                } else {
                    let _e207 = base;
                    let _e210 = unnamed.advancedFogColorDensity;
                    let _e212 = fogAmount;
                    let _e214 = mix(_e207.xyz, _e210.xyz, vec3(_e212));
                    base[0u] = _e214.x;
                    base[1u] = _e214.y;
                    base[2u] = _e214.z;
                }
            }
        }
    } else {
        if override_type_3_10 {
            let _e221 = base;
            let _e224 = fog[3u];
            let _e226 = (_e221.xyz * (1f - _e224));
            base[0u] = _e226.x;
            base[1u] = _e226.y;
            base[2u] = _e226.z;
        } else {
            if override_type_3_11 {
                let _e233 = base;
                let _e235 = fog[3u];
                base = (_e233 * (1f - _e235));
            } else {
                if override_type_3_12 {
                    let _e239 = base[3u];
                    let _e241 = fog[3u];
                    base[3u] = (_e239 * (1f - _e241));
                } else {
                    let _e245 = base;
                    let _e246 = fog;
                    let _e248 = unnamed.fogColor;
                    let _e251 = fog[3u];
                    base = mix(_e245, (_e246 * _e248), vec4(_e251));
                }
            }
        }
    }
    if override_type_3_13 {
        let _e255 = base[3u];
        if (_e255 == 0f) {
            discard;
        }
    } else {
        if override_type_3_14 {
            let _e257 = base;
            let _e259 = base;
            if (dot(_e257.xyz, _e259.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e263 = base;
    out_color = _e263;
    let _e265 = color0_[3u];
    param_9 = _e265;
    let _e266 = wiredTemporalAtestConfidence_u0028_f1_u003b((&param_9));
    param_10 = _e266;
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
