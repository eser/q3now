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
override override_type_4_: bool = (alpha_test_func == 1i);
@id(1) override alpha_test_value: f32 = 0f;
override override_type_4_1: bool = (alpha_test_func == 2i);
override override_type_4_2: bool = (alpha_test_func == 3i);
@id(4) override tex_domain: i32 = 0i;
@id(26) override lightmap_slot: i32 = 0i;
override override_type_4_3: bool = (alpha_test_func == 1i);
override override_type_4_4: bool = (alpha_test_func == 2i);
override override_type_4_5: bool = (alpha_test_func == 3i);
override override_type_4_6: bool = (lightmap_slot != 0i);
@id(10) override acff: i32 = 0i;
override override_type_4_7: bool = (acff == 1i);
override override_type_4_8: bool = (acff == 2i);
override override_type_4_9: bool = (acff == 3i);
override override_type_4_10: bool = (acff == 1i);
override override_type_4_11: bool = (acff == 2i);
override override_type_4_12: bool = (acff == 3i);
@id(7) override discard_mode: i32 = 0i;
override override_type_4_13: bool = (discard_mode == 1i);
override override_type_4_14: bool = (discard_mode == 2i);
@id(3) override alpha_to_coverage: i32 = 0i;

var<private> out_temporal_velocity: vec2<f32>;
var<private> out_temporal_validity: f32;
@group(0) @binding(0)
var<uniform> unnamed: UBO;
var<private> gl_FragCoord_1: vec4<f32>;
@group(1) @binding(0)
var wired_bindless_images: binding_array<texture_2d<f32>>;
@group(1) @binding(1)
var wired_bindless_samplers: binding_array<sampler>;
var<private> fog_tex_coord_1: vec2<f32>;
var<private> frag_color0In_1: vec4<f32>;
var<private> frag_tex_coord0_1: vec2<f32>;
var<private> out_color: vec4<f32>;
var<private> temporalCurrentClip_1: vec4<f32>;
var<private> temporalPreviousClip_1: vec4<f32>;
var<private> temporalOutcome_1: u32;

fn wiredTemporalWriteAux_u0028_f1_u003b(coverageConfidence: ptr<function, f32>) {
    out_temporal_velocity = vec2<f32>(0f, 0f);
    out_temporal_validity = 0f;
    return;
}

fn wiredTemporalAtestConfidence_u0028_f1_u003b(alpha: ptr<function, f32>) -> f32 {
    var span: f32;

    if override_type_4_ {
        span = max(max(alpha_test_value, (1f - alpha_test_value)), 0.000001f);
        let _e81 = (*alpha);
        let _e84 = span;
        return clamp((abs((_e81 - alpha_test_value)) / _e84), 0f, 1f);
    } else {
        if override_type_4_1 {
            let _e87 = (*alpha);
            return clamp(((alpha_test_value - _e87) / max(alpha_test_value, 0.000001f)), 0f, 1f);
        } else {
            if override_type_4_2 {
                let _e92 = (*alpha);
                return clamp(((_e92 - alpha_test_value) / max((1f - alpha_test_value), 0.000001f)), 0f, 1f);
            }
        }
    }
    return 1f;
}

fn wired_advanced_fog_enabled_u0028_() -> bool {
    var fogType: i32;

    let _e79 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e79 + 0.5f));
    let _e84 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e86 = fogType;
    let _e89 = fogType;
    return (((_e84 > 0.5f) && (_e86 >= 1i)) && (_e89 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e79 = wired_advanced_fog_enabled_u0028_();
    if !(_e79) {
        return 0f;
    }
    let _e82 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e82, 0.000001f));
    let _e87 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e87 + 0.5f));
    let _e90 = fogType_1;
    if (_e90 == 1i) {
        let _e94 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e94 <= 0f) {
            return 0f;
        }
        let _e96 = viewDepth;
        let _e99 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e96 / _e99), 0f, 1f);
    }
    let _e104 = unnamed.advancedFogColorDensity[3u];
    let _e106 = viewDepth;
    opticalDepth = (max(_e104, 0f) * _e106);
    let _e108 = fogType_1;
    if (_e108 == 2i) {
        let _e110 = opticalDepth;
        return clamp((1f - exp(-(_e110))), 0f, 1f);
    }
    let _e115 = opticalDepth;
    let _e116 = opticalDepth;
    return clamp((1f - exp(-((_e115 * _e116)))), 0f, 1f);
}

fn wired_boost_legacy_lightmap_u0028_vf3_u003b(rgb: ptr<function, vec3<f32>>) -> vec3<f32> {
    var boosted: vec3<f32>;
    var peak: f32;

    let _e79 = (*rgb);
    let _e82 = unnamed.worldLightParams[0u];
    boosted = (_e79 * _e82);
    let _e85 = boosted[0u];
    let _e87 = boosted[1u];
    let _e89 = boosted[2u];
    peak = max(_e85, max(_e87, _e89));
    let _e92 = peak;
    if (_e92 > 1f) {
        let _e94 = peak;
        let _e95 = boosted;
        boosted = (_e95 / vec3(_e94));
    }
    let _e98 = boosted;
    return _e98;
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e80 = (*c);
    (*c) = max(_e80, vec3<f32>(0f, 0f, 0f));
    let _e82 = (*c);
    cutoff = (_e82 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e84 = (*c);
    lo = (_e84 / vec3(12.92f));
    let _e87 = (*c);
    hi = pow(((_e87 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e92 = hi;
    let _e93 = lo;
    let _e94 = cutoff;
    return mix(_e92, _e93, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e94));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param: vec3<f32>;
    var param_1: vec3<f32>;

    let _e82 = (*role);
    let _e84 = (*role);
    let _e89 = unnamed.packed_indices[(_e82 / 4u)][(_e84 % 4u)];
    let _e92 = (*role);
    let _e94 = (*role);
    let _e99 = unnamed.packed_indices[(_e92 / 4u)][(_e94 % 4u)];
    let _e104 = (*uv);
    let _e105 = textureSample(wired_bindless_images[(_e89 & 4095u)], wired_bindless_samplers[((_e99 >> bitcast<u32>(12i)) & 255u)], _e104);
    c_1 = _e105;
    let _e106 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e106))) == 0i) {
        let _e111 = c_1;
        param = _e111.xyz;
        let _e113 = sRGBToLinear_u0028_vf3_u003b((&param));
        c_1[0u] = _e113.x;
        c_1[1u] = _e113.y;
        c_1[2u] = _e113.z;
    }
    let _e120 = (*slot);
    if (lightmap_slot == (_e120 + 1i)) {
        let _e123 = c_1;
        param_1 = _e123.xyz;
        let _e125 = wired_boost_legacy_lightmap_u0028_vf3_u003b((&param_1));
        c_1[0u] = _e125.x;
        c_1[1u] = _e125.y;
        c_1[2u] = _e125.z;
    }
    let _e132 = c_1;
    return _e132;
}

fn main_1() {
    var fog: vec4<f32>;
    var frag_color0_: vec4<f32>;
    var param_2: vec3<f32>;
    var color0_: vec4<f32>;
    var param_3: u32;
    var param_4: vec2<f32>;
    var param_5: i32;
    var base: vec4<f32>;
    var wetness: f32;
    var frost: f32;
    var luminance: f32;
    var fogAmount: f32;
    var param_6: f32;
    var param_7: f32;

    let _e93 = unnamed.packed_indices[0i][3u];
    let _e99 = unnamed.packed_indices[0i][3u];
    let _e104 = fog_tex_coord_1;
    let _e105 = textureSample(wired_bindless_images[(_e93 & 4095u)], wired_bindless_samplers[((_e99 >> bitcast<u32>(12i)) & 255u)], _e104);
    fog = _e105;
    let _e106 = frag_color0In_1;
    param_2 = _e106.xyz;
    let _e108 = sRGBToLinear_u0028_vf3_u003b((&param_2));
    let _e110 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e108.x, _e108.y, _e108.z, _e110);
    param_3 = 0u;
    let _e115 = frag_tex_coord0_1;
    param_4 = _e115;
    param_5 = 0i;
    let _e116 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_3), (&param_4), (&param_5));
    let _e117 = frag_color0_;
    color0_ = (_e116 * _e117);
    let _e119 = color0_;
    base = _e119;
    if override_type_4_3 {
        let _e121 = color0_[3u];
        if (_e121 == alpha_test_value) {
            discard;
        }
    } else {
        if override_type_4_4 {
            let _e124 = color0_[3u];
            if (_e124 >= alpha_test_value) {
                discard;
            }
        } else {
            if override_type_4_5 {
                let _e127 = color0_[3u];
                if (_e127 < alpha_test_value) {
                    discard;
                }
            }
        }
    }
    let _e129 = color0_;
    base = _e129;
    if override_type_4_6 {
        let _e132 = unnamed.worldLightParams[1u];
        wetness = clamp(_e132, 0f, 1f);
        let _e136 = unnamed.worldLightParams[2u];
        frost = clamp(_e136, 0f, 1f);
        let _e138 = base;
        luminance = dot(_e138.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e141 = wetness;
        let _e143 = base;
        let _e145 = (_e143.xyz * mix(1f, 0.82f, _e141));
        base[0u] = _e145.x;
        base[1u] = _e145.y;
        base[2u] = _e145.z;
        let _e152 = base;
        let _e154 = luminance;
        let _e156 = luminance;
        let _e158 = luminance;
        let _e160 = frost;
        let _e163 = mix(_e152.xyz, vec3<f32>((_e154 * 0.88f), (_e156 * 0.94f), _e158), vec3((_e160 * 0.55f)));
        base[0u] = _e163.x;
        base[1u] = _e163.y;
        base[2u] = _e163.z;
    }
    let _e170 = color0_;
    let _e173 = unnamed.emissionRadiance;
    let _e176 = base;
    let _e178 = (_e176.xyz + (_e170.xyz * _e173.xyz));
    base[0u] = _e178.x;
    base[1u] = _e178.y;
    base[2u] = _e178.z;
    let _e185 = wired_advanced_fog_enabled_u0028_();
    if _e185 {
        let _e186 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e186;
        if override_type_4_7 {
            let _e187 = fogAmount;
            let _e189 = base;
            let _e191 = (_e189.xyz * (1f - _e187));
            base[0u] = _e191.x;
            base[1u] = _e191.y;
            base[2u] = _e191.z;
        } else {
            if override_type_4_8 {
                let _e198 = fogAmount;
                let _e200 = base;
                base = (_e200 * (1f - _e198));
            } else {
                if override_type_4_9 {
                    let _e202 = fogAmount;
                    let _e205 = base[3u];
                    base[3u] = (_e205 * (1f - _e202));
                } else {
                    let _e208 = base;
                    let _e211 = unnamed.advancedFogColorDensity;
                    let _e213 = fogAmount;
                    let _e215 = mix(_e208.xyz, _e211.xyz, vec3(_e213));
                    base[0u] = _e215.x;
                    base[1u] = _e215.y;
                    base[2u] = _e215.z;
                }
            }
        }
    } else {
        if override_type_4_10 {
            let _e222 = base;
            let _e225 = fog[3u];
            let _e227 = (_e222.xyz * (1f - _e225));
            base[0u] = _e227.x;
            base[1u] = _e227.y;
            base[2u] = _e227.z;
        } else {
            if override_type_4_11 {
                let _e234 = base;
                let _e236 = fog[3u];
                base = (_e234 * (1f - _e236));
            } else {
                if override_type_4_12 {
                    let _e240 = base[3u];
                    let _e242 = fog[3u];
                    base[3u] = (_e240 * (1f - _e242));
                } else {
                    let _e246 = base;
                    let _e247 = fog;
                    let _e249 = unnamed.fogColor;
                    let _e252 = fog[3u];
                    base = mix(_e246, (_e247 * _e249), vec4(_e252));
                }
            }
        }
    }
    if override_type_4_13 {
        let _e256 = base[3u];
        if (_e256 == 0f) {
            discard;
        }
    } else {
        if override_type_4_14 {
            let _e258 = base;
            let _e260 = base;
            if (dot(_e258.xyz, _e260.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e264 = base;
    out_color = _e264;
    let _e266 = color0_[3u];
    param_6 = _e266;
    let _e267 = wiredTemporalAtestConfidence_u0028_f1_u003b((&param_6));
    param_7 = _e267;
    wiredTemporalWriteAux_u0028_f1_u003b((&param_7));
    return;
}

@fragment
fn main(@builtin(position) gl_FragCoord: vec4<f32>, @location(4) fog_tex_coord: vec2<f32>, @location(0) frag_color0In: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(10) temporalCurrentClip: vec4<f32>, @location(11) temporalPreviousClip: vec4<f32>, @location(12) @interpolate(flat) temporalOutcome: u32) -> FragmentOutput {
    gl_FragCoord_1 = gl_FragCoord;
    fog_tex_coord_1 = fog_tex_coord;
    frag_color0In_1 = frag_color0In;
    frag_tex_coord0_1 = frag_tex_coord0_;
    temporalCurrentClip_1 = temporalCurrentClip;
    temporalPreviousClip_1 = temporalPreviousClip;
    temporalOutcome_1 = temporalOutcome;
    main_1();
    let _e17 = out_temporal_velocity;
    let _e18 = out_temporal_validity;
    let _e19 = out_color;
    return FragmentOutput(_e17, _e18, _e19);
}
