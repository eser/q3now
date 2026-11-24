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
override override_type_6_: bool = (alpha_test_func == 1i);
@id(1) override alpha_test_value: f32 = 0f;
override override_type_6_1: bool = (alpha_test_func == 2i);
override override_type_6_2: bool = (alpha_test_func == 3i);
@id(4) override tex_domain: i32 = 0i;
@id(26) override lightmap_slot: i32 = 0i;
override override_type_6_3: bool = (alpha_test_func == 1i);
override override_type_6_4: bool = (alpha_test_func == 2i);
override override_type_6_5: bool = (alpha_test_func == 3i);
override override_type_6_6: bool = (lightmap_slot != 0i);
@id(10) override acff: i32 = 0i;
override override_type_6_7: bool = (acff == 1i);
override override_type_6_8: bool = (acff == 2i);
override override_type_6_9: bool = (acff == 3i);
override override_type_6_10: bool = (acff == 1i);
override override_type_6_11: bool = (acff == 2i);
override override_type_6_12: bool = (acff == 3i);
@id(7) override discard_mode: i32 = 0i;
override override_type_6_13: bool = (discard_mode == 1i);
override override_type_6_14: bool = (discard_mode == 2i);
@id(3) override alpha_to_coverage: i32 = 0i;

var<private> out_temporal_velocity: vec2<f32>;
var<private> out_temporal_validity: vec4<f32>;
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

fn wiredTemporalWriteAux_u0028_f1_u003b_vf4_u003b(coverageConfidence: ptr<function, f32>, sceneColor: ptr<function, vec4<f32>>) {
    out_temporal_velocity = vec2<f32>(0f, 0f);
    out_temporal_validity = vec4<f32>(0f, 0f, 0f, 0f);
    let _e82 = (*sceneColor)[0u];
    let _e84 = (*sceneColor)[1u];
    let _e86 = (*sceneColor)[2u];
    let _e91 = (*sceneColor)[3u];
    out_temporal_validity = vec4<f32>(clamp(max(_e82, max(_e84, _e86)), 0f, 1f), 0f, 0f, clamp(_e91, 0f, 1f));
    return;
}

fn wiredTemporalAtestConfidence_u0028_f1_u003b(alpha: ptr<function, f32>) -> f32 {
    var span: f32;

    if override_type_6_ {
        span = max(max(alpha_test_value, (1f - alpha_test_value)), 0.000001f);
        let _e84 = (*alpha);
        let _e87 = span;
        return clamp((abs((_e84 - alpha_test_value)) / _e87), 0f, 1f);
    } else {
        if override_type_6_1 {
            let _e90 = (*alpha);
            return clamp(((alpha_test_value - _e90) / max(alpha_test_value, 0.000001f)), 0f, 1f);
        } else {
            if override_type_6_2 {
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

fn wired_boost_legacy_lightmap_u0028_vf3_u003b(rgb: ptr<function, vec3<f32>>) -> vec3<f32> {
    var boosted: vec3<f32>;
    var peak: f32;

    let _e82 = (*rgb);
    let _e85 = unnamed.worldLightParams[0u];
    boosted = (_e82 * _e85);
    let _e88 = boosted[0u];
    let _e90 = boosted[1u];
    let _e92 = boosted[2u];
    peak = max(_e88, max(_e90, _e92));
    let _e95 = peak;
    if (_e95 > 1f) {
        let _e97 = peak;
        let _e98 = boosted;
        boosted = (_e98 / vec3(_e97));
    }
    let _e101 = boosted;
    return _e101;
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
    var param: vec3<f32>;
    var param_1: vec3<f32>;

    let _e85 = (*role);
    let _e87 = (*role);
    let _e92 = unnamed.packed_indices[(_e85 / 4u)][(_e87 % 4u)];
    let _e95 = (*role);
    let _e97 = (*role);
    let _e102 = unnamed.packed_indices[(_e95 / 4u)][(_e97 % 4u)];
    let _e107 = (*uv);
    let _e108 = textureSample(wired_bindless_images[(_e92 & 4095u)], wired_bindless_samplers[((_e102 >> bitcast<u32>(12i)) & 255u)], _e107);
    c_1 = _e108;
    let _e109 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e109))) == 0i) {
        let _e114 = c_1;
        param = _e114.xyz;
        let _e116 = sRGBToLinear_u0028_vf3_u003b((&param));
        c_1[0u] = _e116.x;
        c_1[1u] = _e116.y;
        c_1[2u] = _e116.z;
    }
    let _e123 = (*slot);
    if (lightmap_slot == (_e123 + 1i)) {
        let _e126 = c_1;
        param_1 = _e126.xyz;
        let _e128 = wired_boost_legacy_lightmap_u0028_vf3_u003b((&param_1));
        c_1[0u] = _e128.x;
        c_1[1u] = _e128.y;
        c_1[2u] = _e128.z;
    }
    let _e135 = c_1;
    return _e135;
}

fn main_1() {
    var fog: vec4<f32>;
    var frag_color0_: vec4<f32>;
    var param_2: vec3<f32>;
    var sampledColor0_: vec4<f32>;
    var param_3: u32;
    var param_4: vec2<f32>;
    var param_5: i32;
    var color0_: vec4<f32>;
    var base: vec4<f32>;
    var wetness: f32;
    var frost: f32;
    var luminance: f32;
    var fogAmount: f32;
    var param_6: f32;
    var param_7: f32;
    var param_8: vec4<f32>;

    let _e98 = unnamed.packed_indices[0i][3u];
    let _e104 = unnamed.packed_indices[0i][3u];
    let _e109 = fog_tex_coord_1;
    let _e110 = textureSample(wired_bindless_images[(_e98 & 4095u)], wired_bindless_samplers[((_e104 >> bitcast<u32>(12i)) & 255u)], _e109);
    fog = _e110;
    let _e111 = frag_color0In_1;
    param_2 = _e111.xyz;
    let _e113 = sRGBToLinear_u0028_vf3_u003b((&param_2));
    let _e115 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e113.x, _e113.y, _e113.z, _e115);
    param_3 = 0u;
    let _e120 = frag_tex_coord0_1;
    param_4 = _e120;
    param_5 = 0i;
    let _e121 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_3), (&param_4), (&param_5));
    sampledColor0_ = _e121;
    let _e122 = sampledColor0_;
    let _e123 = frag_color0_;
    color0_ = (_e122 * _e123);
    let _e125 = color0_;
    base = _e125;
    if override_type_6_3 {
        let _e127 = color0_[3u];
        if (_e127 == alpha_test_value) {
            discard;
        }
    } else {
        if override_type_6_4 {
            let _e130 = color0_[3u];
            if (_e130 >= alpha_test_value) {
                discard;
            }
        } else {
            if override_type_6_5 {
                let _e133 = color0_[3u];
                if (_e133 < alpha_test_value) {
                    discard;
                }
            }
        }
    }
    let _e135 = color0_;
    base = _e135;
    if override_type_6_6 {
        let _e138 = unnamed.worldLightParams[1u];
        wetness = clamp(_e138, 0f, 1f);
        let _e142 = unnamed.worldLightParams[2u];
        frost = clamp(_e142, 0f, 1f);
        let _e144 = base;
        luminance = dot(_e144.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e147 = wetness;
        let _e149 = base;
        let _e151 = (_e149.xyz * mix(1f, 0.82f, _e147));
        base[0u] = _e151.x;
        base[1u] = _e151.y;
        base[2u] = _e151.z;
        let _e158 = base;
        let _e160 = luminance;
        let _e162 = luminance;
        let _e164 = luminance;
        let _e166 = frost;
        let _e169 = mix(_e158.xyz, vec3<f32>((_e160 * 0.88f), (_e162 * 0.94f), _e164), vec3((_e166 * 0.55f)));
        base[0u] = _e169.x;
        base[1u] = _e169.y;
        base[2u] = _e169.z;
    }
    let _e178 = unnamed.materialFactors[0u];
    if (_e178 >= 0f) {
        let _e181 = unnamed.materialBaseColor;
        let _e182 = base;
        base = (_e182 * _e181);
    }
    let _e184 = color0_;
    let _e187 = unnamed.emissionRadiance;
    let _e190 = base;
    let _e192 = (_e190.xyz + (_e184.xyz * _e187.xyz));
    base[0u] = _e192.x;
    base[1u] = _e192.y;
    base[2u] = _e192.z;
    let _e199 = wired_advanced_fog_enabled_u0028_();
    if _e199 {
        let _e200 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e200;
        if override_type_6_7 {
            let _e201 = fogAmount;
            let _e203 = base;
            let _e205 = (_e203.xyz * (1f - _e201));
            base[0u] = _e205.x;
            base[1u] = _e205.y;
            base[2u] = _e205.z;
        } else {
            if override_type_6_8 {
                let _e212 = fogAmount;
                let _e214 = base;
                base = (_e214 * (1f - _e212));
            } else {
                if override_type_6_9 {
                    let _e216 = fogAmount;
                    let _e219 = base[3u];
                    base[3u] = (_e219 * (1f - _e216));
                } else {
                    let _e222 = base;
                    let _e225 = unnamed.advancedFogColorDensity;
                    let _e227 = fogAmount;
                    let _e229 = mix(_e222.xyz, _e225.xyz, vec3(_e227));
                    base[0u] = _e229.x;
                    base[1u] = _e229.y;
                    base[2u] = _e229.z;
                }
            }
        }
    } else {
        if override_type_6_10 {
            let _e236 = base;
            let _e239 = fog[3u];
            let _e241 = (_e236.xyz * (1f - _e239));
            base[0u] = _e241.x;
            base[1u] = _e241.y;
            base[2u] = _e241.z;
        } else {
            if override_type_6_11 {
                let _e248 = base;
                let _e250 = fog[3u];
                base = (_e248 * (1f - _e250));
            } else {
                if override_type_6_12 {
                    let _e254 = base[3u];
                    let _e256 = fog[3u];
                    base[3u] = (_e254 * (1f - _e256));
                } else {
                    let _e260 = base;
                    let _e261 = fog;
                    let _e263 = unnamed.fogColor;
                    let _e266 = fog[3u];
                    base = mix(_e260, (_e261 * _e263), vec4(_e266));
                }
            }
        }
    }
    if override_type_6_13 {
        let _e270 = base[3u];
        if (_e270 == 0f) {
            discard;
        }
    } else {
        if override_type_6_14 {
            let _e272 = base;
            let _e274 = base;
            if (dot(_e272.xyz, _e274.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e278 = base;
    out_color = _e278;
    let _e280 = color0_[3u];
    param_6 = _e280;
    let _e281 = wiredTemporalAtestConfidence_u0028_f1_u003b((&param_6));
    param_7 = _e281;
    let _e282 = base;
    param_8 = _e282;
    wiredTemporalWriteAux_u0028_f1_u003b_vf4_u003b((&param_7), (&param_8));
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
