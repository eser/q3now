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
@id(8) override identity_color: f32 = 1f;
@id(9) override identity_alpha: f32 = 1f;
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
var<private> frag_tex_coord0_1: vec2<f32>;
var<private> out_color: vec4<f32>;
var<private> temporalCurrentClip_1: vec4<f32>;
var<private> temporalPreviousClip_1: vec4<f32>;
var<private> temporalOutcome_1: u32;

fn wiredTemporalWriteAux_u0028_f1_u003b_vf4_u003b(coverageConfidence: ptr<function, f32>, sceneColor: ptr<function, vec4<f32>>) {
    out_temporal_velocity = vec2<f32>(0f, 0f);
    out_temporal_validity = vec4<f32>(0f, 0f, 0f, 0f);
    let _e83 = (*sceneColor)[0u];
    let _e85 = (*sceneColor)[1u];
    let _e87 = (*sceneColor)[2u];
    let _e92 = (*sceneColor)[3u];
    out_temporal_validity = vec4<f32>(clamp(max(_e83, max(_e85, _e87)), 0f, 1f), 0f, 0f, clamp(_e92, 0f, 1f));
    return;
}

fn wiredTemporalAtestConfidence_u0028_f1_u003b(alpha: ptr<function, f32>) -> f32 {
    var span: f32;

    if override_type_6_ {
        span = max(max(alpha_test_value, (1f - alpha_test_value)), 0.000001f);
        let _e85 = (*alpha);
        let _e88 = span;
        return clamp((abs((_e85 - alpha_test_value)) / _e88), 0f, 1f);
    } else {
        if override_type_6_1 {
            let _e91 = (*alpha);
            return clamp(((alpha_test_value - _e91) / max(alpha_test_value, 0.000001f)), 0f, 1f);
        } else {
            if override_type_6_2 {
                let _e96 = (*alpha);
                return clamp(((_e96 - alpha_test_value) / max((1f - alpha_test_value), 0.000001f)), 0f, 1f);
            }
        }
    }
    return 1f;
}

fn wired_advanced_fog_enabled_u0028_() -> bool {
    var fogType: i32;

    let _e83 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e83 + 0.5f));
    let _e88 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e90 = fogType;
    let _e93 = fogType;
    return (((_e88 > 0.5f) && (_e90 >= 1i)) && (_e93 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e83 = wired_advanced_fog_enabled_u0028_();
    if !(_e83) {
        return 0f;
    }
    let _e86 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e86, 0.000001f));
    let _e91 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e91 + 0.5f));
    let _e94 = fogType_1;
    if (_e94 == 1i) {
        let _e98 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e98 <= 0f) {
            return 0f;
        }
        let _e100 = viewDepth;
        let _e103 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e100 / _e103), 0f, 1f);
    }
    let _e108 = unnamed.advancedFogColorDensity[3u];
    let _e110 = viewDepth;
    opticalDepth = (max(_e108, 0f) * _e110);
    let _e112 = fogType_1;
    if (_e112 == 2i) {
        let _e114 = opticalDepth;
        return clamp((1f - exp(-(_e114))), 0f, 1f);
    }
    let _e119 = opticalDepth;
    let _e120 = opticalDepth;
    return clamp((1f - exp(-((_e119 * _e120)))), 0f, 1f);
}

fn wired_boost_legacy_lightmap_u0028_vf3_u003b(rgb: ptr<function, vec3<f32>>) -> vec3<f32> {
    var boosted: vec3<f32>;
    var peak: f32;

    let _e83 = (*rgb);
    let _e86 = unnamed.worldLightParams[0u];
    boosted = (_e83 * _e86);
    let _e89 = boosted[0u];
    let _e91 = boosted[1u];
    let _e93 = boosted[2u];
    peak = max(_e89, max(_e91, _e93));
    let _e96 = peak;
    if (_e96 > 1f) {
        let _e98 = peak;
        let _e99 = boosted;
        boosted = (_e99 / vec3(_e98));
    }
    let _e102 = boosted;
    return _e102;
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e84 = (*c);
    (*c) = max(_e84, vec3<f32>(0f, 0f, 0f));
    let _e86 = (*c);
    cutoff = (_e86 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e88 = (*c);
    lo = (_e88 / vec3(12.92f));
    let _e91 = (*c);
    hi = pow(((_e91 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e96 = hi;
    let _e97 = lo;
    let _e98 = cutoff;
    return mix(_e96, _e97, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e98));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param: vec3<f32>;
    var param_1: vec3<f32>;

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
        param = _e115.xyz;
        let _e117 = sRGBToLinear_u0028_vf3_u003b((&param));
        c_1[0u] = _e117.x;
        c_1[1u] = _e117.y;
        c_1[2u] = _e117.z;
    }
    let _e124 = (*slot);
    if (lightmap_slot == (_e124 + 1i)) {
        let _e127 = c_1;
        param_1 = _e127.xyz;
        let _e129 = wired_boost_legacy_lightmap_u0028_vf3_u003b((&param_1));
        c_1[0u] = _e129.x;
        c_1[1u] = _e129.y;
        c_1[2u] = _e129.z;
    }
    let _e136 = c_1;
    return _e136;
}

fn main_1() {
    var fog: vec4<f32>;
    var sampledColor0_: vec4<f32>;
    var param_2: u32;
    var param_3: vec2<f32>;
    var param_4: i32;
    var frag_color: vec4<f32>;
    var color0_: vec4<f32>;
    var base: vec4<f32>;
    var wetness: f32;
    var frost: f32;
    var luminance: f32;
    var fogAmount: f32;
    var param_5: f32;
    var param_6: f32;
    var param_7: vec4<f32>;

    let _e98 = unnamed.packed_indices[0i][3u];
    let _e104 = unnamed.packed_indices[0i][3u];
    let _e109 = fog_tex_coord_1;
    let _e110 = textureSample(wired_bindless_images[(_e98 & 4095u)], wired_bindless_samplers[((_e104 >> bitcast<u32>(12i)) & 255u)], _e109);
    fog = _e110;
    param_2 = 0u;
    let _e111 = frag_tex_coord0_1;
    param_3 = _e111;
    param_4 = 0i;
    let _e112 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_2), (&param_3), (&param_4));
    sampledColor0_ = _e112;
    frag_color[0u] = identity_color;
    frag_color[1u] = identity_color;
    frag_color[2u] = identity_color;
    frag_color[3u] = identity_alpha;
    let _e117 = sampledColor0_;
    let _e118 = frag_color;
    color0_ = (_e117 * _e118);
    let _e120 = color0_;
    base = _e120;
    if override_type_6_3 {
        let _e122 = color0_[3u];
        if (_e122 == alpha_test_value) {
            discard;
        }
    } else {
        if override_type_6_4 {
            let _e125 = color0_[3u];
            if (_e125 >= alpha_test_value) {
                discard;
            }
        } else {
            if override_type_6_5 {
                let _e128 = color0_[3u];
                if (_e128 < alpha_test_value) {
                    discard;
                }
            }
        }
    }
    let _e130 = color0_;
    base = _e130;
    if override_type_6_6 {
        let _e133 = unnamed.worldLightParams[1u];
        wetness = clamp(_e133, 0f, 1f);
        let _e137 = unnamed.worldLightParams[2u];
        frost = clamp(_e137, 0f, 1f);
        let _e139 = base;
        luminance = dot(_e139.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e142 = wetness;
        let _e144 = base;
        let _e146 = (_e144.xyz * mix(1f, 0.82f, _e142));
        base[0u] = _e146.x;
        base[1u] = _e146.y;
        base[2u] = _e146.z;
        let _e153 = base;
        let _e155 = luminance;
        let _e157 = luminance;
        let _e159 = luminance;
        let _e161 = frost;
        let _e164 = mix(_e153.xyz, vec3<f32>((_e155 * 0.88f), (_e157 * 0.94f), _e159), vec3((_e161 * 0.55f)));
        base[0u] = _e164.x;
        base[1u] = _e164.y;
        base[2u] = _e164.z;
    }
    let _e173 = unnamed.materialFactors[0u];
    if (_e173 >= 0f) {
        let _e176 = unnamed.materialBaseColor;
        let _e177 = base;
        base = (_e177 * _e176);
    }
    let _e179 = color0_;
    let _e182 = unnamed.emissionRadiance;
    let _e185 = base;
    let _e187 = (_e185.xyz + (_e179.xyz * _e182.xyz));
    base[0u] = _e187.x;
    base[1u] = _e187.y;
    base[2u] = _e187.z;
    let _e194 = wired_advanced_fog_enabled_u0028_();
    if _e194 {
        let _e195 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e195;
        if override_type_6_7 {
            let _e196 = fogAmount;
            let _e198 = base;
            let _e200 = (_e198.xyz * (1f - _e196));
            base[0u] = _e200.x;
            base[1u] = _e200.y;
            base[2u] = _e200.z;
        } else {
            if override_type_6_8 {
                let _e207 = fogAmount;
                let _e209 = base;
                base = (_e209 * (1f - _e207));
            } else {
                if override_type_6_9 {
                    let _e211 = fogAmount;
                    let _e214 = base[3u];
                    base[3u] = (_e214 * (1f - _e211));
                } else {
                    let _e217 = base;
                    let _e220 = unnamed.advancedFogColorDensity;
                    let _e222 = fogAmount;
                    let _e224 = mix(_e217.xyz, _e220.xyz, vec3(_e222));
                    base[0u] = _e224.x;
                    base[1u] = _e224.y;
                    base[2u] = _e224.z;
                }
            }
        }
    } else {
        if override_type_6_10 {
            let _e231 = base;
            let _e234 = fog[3u];
            let _e236 = (_e231.xyz * (1f - _e234));
            base[0u] = _e236.x;
            base[1u] = _e236.y;
            base[2u] = _e236.z;
        } else {
            if override_type_6_11 {
                let _e243 = base;
                let _e245 = fog[3u];
                base = (_e243 * (1f - _e245));
            } else {
                if override_type_6_12 {
                    let _e249 = base[3u];
                    let _e251 = fog[3u];
                    base[3u] = (_e249 * (1f - _e251));
                } else {
                    let _e255 = base;
                    let _e256 = fog;
                    let _e258 = unnamed.fogColor;
                    let _e261 = fog[3u];
                    base = mix(_e255, (_e256 * _e258), vec4(_e261));
                }
            }
        }
    }
    if override_type_6_13 {
        let _e265 = base[3u];
        if (_e265 == 0f) {
            discard;
        }
    } else {
        if override_type_6_14 {
            let _e267 = base;
            let _e269 = base;
            if (dot(_e267.xyz, _e269.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e273 = base;
    out_color = _e273;
    let _e275 = color0_[3u];
    param_5 = _e275;
    let _e276 = wiredTemporalAtestConfidence_u0028_f1_u003b((&param_5));
    param_6 = _e276;
    let _e277 = base;
    param_7 = _e277;
    wiredTemporalWriteAux_u0028_f1_u003b_vf4_u003b((&param_6), (&param_7));
    return;
}

@fragment
fn main(@builtin(position) gl_FragCoord: vec4<f32>, @location(4) fog_tex_coord: vec2<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(10) temporalCurrentClip: vec4<f32>, @location(11) temporalPreviousClip: vec4<f32>, @location(12) @interpolate(flat) temporalOutcome: u32) -> FragmentOutput {
    gl_FragCoord_1 = gl_FragCoord;
    fog_tex_coord_1 = fog_tex_coord;
    frag_tex_coord0_1 = frag_tex_coord0_;
    temporalCurrentClip_1 = temporalCurrentClip;
    temporalPreviousClip_1 = temporalPreviousClip;
    temporalOutcome_1 = temporalOutcome;
    main_1();
    let _e15 = out_temporal_velocity;
    let _e16 = out_temporal_validity;
    let _e17 = out_color;
    return FragmentOutput(_e15, _e16, _e17);
}
