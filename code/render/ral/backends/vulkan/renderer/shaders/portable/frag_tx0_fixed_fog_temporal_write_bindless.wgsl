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
    var phi_82_: bool;

    let _e84 = (*value);
    let _e85 = (*value);
    let _e87 = all((_e84 == _e85));
    phi_82_ = _e87;
    if _e87 {
        let _e88 = (*value);
        phi_82_ = all((abs(_e88) <= vec2<f32>(340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f)));
    }
    let _e93 = phi_82_;
    return _e93;
}

fn wiredTemporalFinite4_u0028_vf4_u003b(value_1: ptr<function, vec4<f32>>) -> bool {
    var phi_67_: bool;

    let _e84 = (*value_1);
    let _e85 = (*value_1);
    let _e87 = all((_e84 == _e85));
    phi_67_ = _e87;
    if _e87 {
        let _e88 = (*value_1);
        phi_67_ = all((abs(_e88) <= vec4<f32>(340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f)));
    }
    let _e93 = phi_67_;
    return _e93;
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
    let _e94 = temporalOutcome_1;
    let _e95 = (_e94 != 1u);
    phi_105_ = _e95;
    if !(_e95) {
        let _e97 = temporalCurrentClip_1;
        param = _e97;
        let _e98 = wiredTemporalFinite4_u0028_vf4_u003b((&param));
        phi_105_ = !(_e98);
    }
    let _e101 = phi_105_;
    phi_114_ = _e101;
    if !(_e101) {
        let _e103 = temporalPreviousClip_1;
        param_1 = _e103;
        let _e104 = wiredTemporalFinite4_u0028_vf4_u003b((&param_1));
        phi_114_ = !(_e104);
    }
    let _e107 = phi_114_;
    phi_124_ = _e107;
    if !(_e107) {
        let _e110 = temporalCurrentClip_1[3u];
        phi_124_ = (_e110 <= 0.000001f);
    }
    let _e113 = phi_124_;
    phi_131_ = _e113;
    if !(_e113) {
        let _e116 = temporalPreviousClip_1[3u];
        phi_131_ = (_e116 <= 0.000001f);
    }
    let _e119 = phi_131_;
    if _e119 {
        return;
    }
    let _e120 = temporalCurrentClip_1;
    let _e123 = temporalCurrentClip_1[3u];
    currentNdc = (_e120.xy / vec2(_e123));
    let _e126 = temporalPreviousClip_1;
    let _e129 = temporalPreviousClip_1[3u];
    previousNdc = (_e126.xy / vec2(_e129));
    let _e132 = currentNdc;
    param_2 = _e132;
    let _e133 = wiredTemporalFinite2_u0028_vf2_u003b((&param_2));
    let _e134 = !(_e133);
    phi_160_ = _e134;
    if !(_e134) {
        let _e136 = previousNdc;
        param_3 = _e136;
        let _e137 = wiredTemporalFinite2_u0028_vf2_u003b((&param_3));
        phi_160_ = !(_e137);
    }
    let _e140 = phi_160_;
    if _e140 {
        return;
    }
    let _e141 = currentNdc;
    currentUv = ((_e141 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e144 = previousNdc;
    previousUv = ((_e144 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e147 = currentUv;
    let _e148 = previousUv;
    velocity = (_e147 - _e148);
    let _e150 = velocity;
    param_4 = _e150;
    let _e151 = wiredTemporalFinite2_u0028_vf2_u003b((&param_4));
    if !(_e151) {
        return;
    }
    let _e153 = velocity;
    out_temporal_velocity = _e153;
    let _e154 = (*coverageConfidence);
    out_temporal_validity = clamp(_e154, 0f, 1f);
    return;
}

fn wiredTemporalAtestConfidence_u0028_f1_u003b(alpha: ptr<function, f32>) -> f32 {
    var span: f32;

    if override_type_3_ {
        span = max(max(alpha_test_value, (1f - alpha_test_value)), 0.000001f);
        let _e88 = (*alpha);
        let _e91 = span;
        return clamp((abs((_e88 - alpha_test_value)) / _e91), 0f, 1f);
    } else {
        if override_type_3_1 {
            let _e94 = (*alpha);
            return clamp(((alpha_test_value - _e94) / max(alpha_test_value, 0.000001f)), 0f, 1f);
        } else {
            if override_type_3_2 {
                let _e99 = (*alpha);
                return clamp(((_e99 - alpha_test_value) / max((1f - alpha_test_value), 0.000001f)), 0f, 1f);
            }
        }
    }
    return 1f;
}

fn wired_advanced_fog_enabled_u0028_() -> bool {
    var fogType: i32;

    let _e86 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e86 + 0.5f));
    let _e91 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e93 = fogType;
    let _e96 = fogType;
    return (((_e91 > 0.5f) && (_e93 >= 1i)) && (_e96 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e86 = wired_advanced_fog_enabled_u0028_();
    if !(_e86) {
        return 0f;
    }
    let _e89 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e89, 0.000001f));
    let _e94 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e94 + 0.5f));
    let _e97 = fogType_1;
    if (_e97 == 1i) {
        let _e101 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e101 <= 0f) {
            return 0f;
        }
        let _e103 = viewDepth;
        let _e106 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e103 / _e106), 0f, 1f);
    }
    let _e111 = unnamed.advancedFogColorDensity[3u];
    let _e113 = viewDepth;
    opticalDepth = (max(_e111, 0f) * _e113);
    let _e115 = fogType_1;
    if (_e115 == 2i) {
        let _e117 = opticalDepth;
        return clamp((1f - exp(-(_e117))), 0f, 1f);
    }
    let _e122 = opticalDepth;
    let _e123 = opticalDepth;
    return clamp((1f - exp(-((_e122 * _e123)))), 0f, 1f);
}

fn wired_boost_legacy_lightmap_u0028_vf3_u003b(rgb: ptr<function, vec3<f32>>) -> vec3<f32> {
    var boosted: vec3<f32>;
    var peak: f32;

    let _e86 = (*rgb);
    let _e89 = unnamed.worldLightParams[0u];
    boosted = (_e86 * _e89);
    let _e92 = boosted[0u];
    let _e94 = boosted[1u];
    let _e96 = boosted[2u];
    peak = max(_e92, max(_e94, _e96));
    let _e99 = peak;
    if (_e99 > 1f) {
        let _e101 = peak;
        let _e102 = boosted;
        boosted = (_e102 / vec3(_e101));
    }
    let _e105 = boosted;
    return _e105;
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e87 = (*c);
    (*c) = max(_e87, vec3<f32>(0f, 0f, 0f));
    let _e89 = (*c);
    cutoff = (_e89 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e91 = (*c);
    lo = (_e91 / vec3(12.92f));
    let _e94 = (*c);
    hi = pow(((_e94 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e99 = hi;
    let _e100 = lo;
    let _e101 = cutoff;
    return mix(_e99, _e100, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e101));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param_5: vec3<f32>;
    var param_6: vec3<f32>;

    let _e89 = (*role);
    let _e91 = (*role);
    let _e96 = unnamed.packed_indices[(_e89 / 4u)][(_e91 % 4u)];
    let _e99 = (*role);
    let _e101 = (*role);
    let _e106 = unnamed.packed_indices[(_e99 / 4u)][(_e101 % 4u)];
    let _e111 = (*uv);
    let _e112 = textureSample(wired_bindless_images[(_e96 & 4095u)], wired_bindless_samplers[((_e106 >> bitcast<u32>(12i)) & 255u)], _e111);
    c_1 = _e112;
    let _e113 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e113))) == 0i) {
        let _e118 = c_1;
        param_5 = _e118.xyz;
        let _e120 = sRGBToLinear_u0028_vf3_u003b((&param_5));
        c_1[0u] = _e120.x;
        c_1[1u] = _e120.y;
        c_1[2u] = _e120.z;
    }
    let _e127 = (*slot);
    if (lightmap_slot == (_e127 + 1i)) {
        let _e130 = c_1;
        param_6 = _e130.xyz;
        let _e132 = wired_boost_legacy_lightmap_u0028_vf3_u003b((&param_6));
        c_1[0u] = _e132.x;
        c_1[1u] = _e132.y;
        c_1[2u] = _e132.z;
    }
    let _e139 = c_1;
    return _e139;
}

fn main_1() {
    var fog: vec4<f32>;
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

    let _e100 = unnamed.packed_indices[0i][3u];
    let _e106 = unnamed.packed_indices[0i][3u];
    let _e111 = fog_tex_coord_1;
    let _e112 = textureSample(wired_bindless_images[(_e100 & 4095u)], wired_bindless_samplers[((_e106 >> bitcast<u32>(12i)) & 255u)], _e111);
    fog = _e112;
    param_7 = 0u;
    let _e113 = frag_tex_coord0_1;
    param_8 = _e113;
    param_9 = 0i;
    let _e114 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_7), (&param_8), (&param_9));
    sampledColor0_ = _e114;
    frag_color[0u] = identity_color;
    frag_color[1u] = identity_color;
    frag_color[2u] = identity_color;
    frag_color[3u] = identity_alpha;
    let _e119 = sampledColor0_;
    let _e120 = frag_color;
    color0_ = (_e119 * _e120);
    let _e122 = color0_;
    base = _e122;
    if override_type_3_3 {
        let _e124 = color0_[3u];
        if (_e124 == alpha_test_value) {
            discard;
        }
    } else {
        if override_type_3_4 {
            let _e127 = color0_[3u];
            if (_e127 >= alpha_test_value) {
                discard;
            }
        } else {
            if override_type_3_5 {
                let _e130 = color0_[3u];
                if (_e130 < alpha_test_value) {
                    discard;
                }
            }
        }
    }
    let _e132 = color0_;
    base = _e132;
    if override_type_3_6 {
        let _e135 = unnamed.worldLightParams[1u];
        wetness = clamp(_e135, 0f, 1f);
        let _e139 = unnamed.worldLightParams[2u];
        frost = clamp(_e139, 0f, 1f);
        let _e141 = base;
        luminance = dot(_e141.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e144 = wetness;
        let _e146 = base;
        let _e148 = (_e146.xyz * mix(1f, 0.82f, _e144));
        base[0u] = _e148.x;
        base[1u] = _e148.y;
        base[2u] = _e148.z;
        let _e155 = base;
        let _e157 = luminance;
        let _e159 = luminance;
        let _e161 = luminance;
        let _e163 = frost;
        let _e166 = mix(_e155.xyz, vec3<f32>((_e157 * 0.88f), (_e159 * 0.94f), _e161), vec3((_e163 * 0.55f)));
        base[0u] = _e166.x;
        base[1u] = _e166.y;
        base[2u] = _e166.z;
    }
    let _e175 = unnamed.materialFactors[0u];
    if (_e175 >= 0f) {
        let _e178 = unnamed.materialBaseColor;
        let _e179 = base;
        base = (_e179 * _e178);
    }
    let _e181 = color0_;
    let _e184 = unnamed.emissionRadiance;
    let _e187 = base;
    let _e189 = (_e187.xyz + (_e181.xyz * _e184.xyz));
    base[0u] = _e189.x;
    base[1u] = _e189.y;
    base[2u] = _e189.z;
    let _e196 = wired_advanced_fog_enabled_u0028_();
    if _e196 {
        let _e197 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e197;
        if override_type_3_7 {
            let _e198 = fogAmount;
            let _e200 = base;
            let _e202 = (_e200.xyz * (1f - _e198));
            base[0u] = _e202.x;
            base[1u] = _e202.y;
            base[2u] = _e202.z;
        } else {
            if override_type_3_8 {
                let _e209 = fogAmount;
                let _e211 = base;
                base = (_e211 * (1f - _e209));
            } else {
                if override_type_3_9 {
                    let _e213 = fogAmount;
                    let _e216 = base[3u];
                    base[3u] = (_e216 * (1f - _e213));
                } else {
                    let _e219 = base;
                    let _e222 = unnamed.advancedFogColorDensity;
                    let _e224 = fogAmount;
                    let _e226 = mix(_e219.xyz, _e222.xyz, vec3(_e224));
                    base[0u] = _e226.x;
                    base[1u] = _e226.y;
                    base[2u] = _e226.z;
                }
            }
        }
    } else {
        if override_type_3_10 {
            let _e233 = base;
            let _e236 = fog[3u];
            let _e238 = (_e233.xyz * (1f - _e236));
            base[0u] = _e238.x;
            base[1u] = _e238.y;
            base[2u] = _e238.z;
        } else {
            if override_type_3_11 {
                let _e245 = base;
                let _e247 = fog[3u];
                base = (_e245 * (1f - _e247));
            } else {
                if override_type_3_12 {
                    let _e251 = base[3u];
                    let _e253 = fog[3u];
                    base[3u] = (_e251 * (1f - _e253));
                } else {
                    let _e257 = base;
                    let _e258 = fog;
                    let _e260 = unnamed.fogColor;
                    let _e263 = fog[3u];
                    base = mix(_e257, (_e258 * _e260), vec4(_e263));
                }
            }
        }
    }
    if override_type_3_13 {
        let _e267 = base[3u];
        if (_e267 == 0f) {
            discard;
        }
    } else {
        if override_type_3_14 {
            let _e269 = base;
            let _e271 = base;
            if (dot(_e269.xyz, _e271.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e275 = base;
    out_color = _e275;
    let _e277 = color0_[3u];
    param_10 = _e277;
    let _e278 = wiredTemporalAtestConfidence_u0028_f1_u003b((&param_10));
    param_11 = _e278;
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
