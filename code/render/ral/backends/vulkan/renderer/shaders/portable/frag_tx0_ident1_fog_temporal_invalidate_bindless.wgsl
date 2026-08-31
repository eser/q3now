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
        let _e80 = (*alpha);
        let _e83 = span;
        return clamp((abs((_e80 - alpha_test_value)) / _e83), 0f, 1f);
    } else {
        if override_type_4_1 {
            let _e86 = (*alpha);
            return clamp(((alpha_test_value - _e86) / max(alpha_test_value, 0.000001f)), 0f, 1f);
        } else {
            if override_type_4_2 {
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
    var param: vec3<f32>;
    var param_1: vec3<f32>;

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
        param = _e110.xyz;
        let _e112 = sRGBToLinear_u0028_vf3_u003b((&param));
        c_1[0u] = _e112.x;
        c_1[1u] = _e112.y;
        c_1[2u] = _e112.z;
    }
    let _e119 = (*slot);
    if (lightmap_slot == (_e119 + 1i)) {
        let _e122 = c_1;
        param_1 = _e122.xyz;
        let _e124 = wired_boost_legacy_lightmap_u0028_vf3_u003b((&param_1));
        c_1[0u] = _e124.x;
        c_1[1u] = _e124.y;
        c_1[2u] = _e124.z;
    }
    let _e131 = c_1;
    return _e131;
}

fn main_1() {
    var fog: vec4<f32>;
    var color0_: vec4<f32>;
    var param_2: u32;
    var param_3: vec2<f32>;
    var param_4: i32;
    var base: vec4<f32>;
    var wetness: f32;
    var frost: f32;
    var luminance: f32;
    var fogAmount: f32;
    var param_5: f32;
    var param_6: f32;

    let _e90 = unnamed.packed_indices[0i][3u];
    let _e96 = unnamed.packed_indices[0i][3u];
    let _e101 = fog_tex_coord_1;
    let _e102 = textureSample(wired_bindless_images[(_e90 & 4095u)], wired_bindless_samplers[((_e96 >> bitcast<u32>(12i)) & 255u)], _e101);
    fog = _e102;
    param_2 = 0u;
    let _e103 = frag_tex_coord0_1;
    param_3 = _e103;
    param_4 = 0i;
    let _e104 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_2), (&param_3), (&param_4));
    color0_ = _e104;
    let _e105 = color0_;
    base = _e105;
    if override_type_4_3 {
        let _e107 = color0_[3u];
        if (_e107 == alpha_test_value) {
            discard;
        }
    } else {
        if override_type_4_4 {
            let _e110 = color0_[3u];
            if (_e110 >= alpha_test_value) {
                discard;
            }
        } else {
            if override_type_4_5 {
                let _e113 = color0_[3u];
                if (_e113 < alpha_test_value) {
                    discard;
                }
            }
        }
    }
    let _e115 = color0_;
    base = _e115;
    if override_type_4_6 {
        let _e118 = unnamed.worldLightParams[1u];
        wetness = clamp(_e118, 0f, 1f);
        let _e122 = unnamed.worldLightParams[2u];
        frost = clamp(_e122, 0f, 1f);
        let _e124 = base;
        luminance = dot(_e124.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e127 = wetness;
        let _e129 = base;
        let _e131 = (_e129.xyz * mix(1f, 0.82f, _e127));
        base[0u] = _e131.x;
        base[1u] = _e131.y;
        base[2u] = _e131.z;
        let _e138 = base;
        let _e140 = luminance;
        let _e142 = luminance;
        let _e144 = luminance;
        let _e146 = frost;
        let _e149 = mix(_e138.xyz, vec3<f32>((_e140 * 0.88f), (_e142 * 0.94f), _e144), vec3((_e146 * 0.55f)));
        base[0u] = _e149.x;
        base[1u] = _e149.y;
        base[2u] = _e149.z;
    }
    let _e156 = color0_;
    let _e159 = unnamed.emissionRadiance;
    let _e162 = base;
    let _e164 = (_e162.xyz + (_e156.xyz * _e159.xyz));
    base[0u] = _e164.x;
    base[1u] = _e164.y;
    base[2u] = _e164.z;
    let _e171 = wired_advanced_fog_enabled_u0028_();
    if _e171 {
        let _e172 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e172;
        if override_type_4_7 {
            let _e173 = fogAmount;
            let _e175 = base;
            let _e177 = (_e175.xyz * (1f - _e173));
            base[0u] = _e177.x;
            base[1u] = _e177.y;
            base[2u] = _e177.z;
        } else {
            if override_type_4_8 {
                let _e184 = fogAmount;
                let _e186 = base;
                base = (_e186 * (1f - _e184));
            } else {
                if override_type_4_9 {
                    let _e188 = fogAmount;
                    let _e191 = base[3u];
                    base[3u] = (_e191 * (1f - _e188));
                } else {
                    let _e194 = base;
                    let _e197 = unnamed.advancedFogColorDensity;
                    let _e199 = fogAmount;
                    let _e201 = mix(_e194.xyz, _e197.xyz, vec3(_e199));
                    base[0u] = _e201.x;
                    base[1u] = _e201.y;
                    base[2u] = _e201.z;
                }
            }
        }
    } else {
        if override_type_4_10 {
            let _e208 = base;
            let _e211 = fog[3u];
            let _e213 = (_e208.xyz * (1f - _e211));
            base[0u] = _e213.x;
            base[1u] = _e213.y;
            base[2u] = _e213.z;
        } else {
            if override_type_4_11 {
                let _e220 = base;
                let _e222 = fog[3u];
                base = (_e220 * (1f - _e222));
            } else {
                if override_type_4_12 {
                    let _e226 = base[3u];
                    let _e228 = fog[3u];
                    base[3u] = (_e226 * (1f - _e228));
                } else {
                    let _e232 = base;
                    let _e233 = fog;
                    let _e235 = unnamed.fogColor;
                    let _e238 = fog[3u];
                    base = mix(_e232, (_e233 * _e235), vec4(_e238));
                }
            }
        }
    }
    if override_type_4_13 {
        let _e242 = base[3u];
        if (_e242 == 0f) {
            discard;
        }
    } else {
        if override_type_4_14 {
            let _e244 = base;
            let _e246 = base;
            if (dot(_e244.xyz, _e246.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e250 = base;
    out_color = _e250;
    let _e252 = color0_[3u];
    param_5 = _e252;
    let _e253 = wiredTemporalAtestConfidence_u0028_f1_u003b((&param_5));
    param_6 = _e253;
    wiredTemporalWriteAux_u0028_f1_u003b((&param_6));
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
