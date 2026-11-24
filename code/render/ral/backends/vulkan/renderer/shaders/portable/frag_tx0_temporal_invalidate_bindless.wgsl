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
@id(7) override discard_mode: i32 = 0i;
override override_type_6_7: bool = (discard_mode == 1i);
override override_type_6_8: bool = (discard_mode == 2i);
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
var<private> frag_color0In_1: vec4<f32>;
var<private> frag_tex_coord0_1: vec2<f32>;
var<private> out_color: vec4<f32>;
var<private> temporalCurrentClip_1: vec4<f32>;
var<private> temporalPreviousClip_1: vec4<f32>;
var<private> temporalOutcome_1: u32;

fn wiredTemporalWriteAux_u0028_f1_u003b_vf4_u003b(coverageConfidence: ptr<function, f32>, sceneColor: ptr<function, vec4<f32>>) {
    out_temporal_velocity = vec2<f32>(0f, 0f);
    out_temporal_validity = vec4<f32>(0f, 0f, 0f, 0f);
    let _e73 = (*sceneColor)[0u];
    let _e75 = (*sceneColor)[1u];
    let _e77 = (*sceneColor)[2u];
    let _e82 = (*sceneColor)[3u];
    out_temporal_validity = vec4<f32>(clamp(max(_e73, max(_e75, _e77)), 0f, 1f), 0f, 0f, clamp(_e82, 0f, 1f));
    return;
}

fn wiredTemporalAtestConfidence_u0028_f1_u003b(alpha: ptr<function, f32>) -> f32 {
    var span: f32;

    if override_type_6_ {
        span = max(max(alpha_test_value, (1f - alpha_test_value)), 0.000001f);
        let _e75 = (*alpha);
        let _e78 = span;
        return clamp((abs((_e75 - alpha_test_value)) / _e78), 0f, 1f);
    } else {
        if override_type_6_1 {
            let _e81 = (*alpha);
            return clamp(((alpha_test_value - _e81) / max(alpha_test_value, 0.000001f)), 0f, 1f);
        } else {
            if override_type_6_2 {
                let _e86 = (*alpha);
                return clamp(((_e86 - alpha_test_value) / max((1f - alpha_test_value), 0.000001f)), 0f, 1f);
            }
        }
    }
    return 1f;
}

fn wired_advanced_fog_enabled_u0028_() -> bool {
    var fogType: i32;

    let _e73 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e73 + 0.5f));
    let _e78 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e80 = fogType;
    let _e83 = fogType;
    return (((_e78 > 0.5f) && (_e80 >= 1i)) && (_e83 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e73 = wired_advanced_fog_enabled_u0028_();
    if !(_e73) {
        return 0f;
    }
    let _e76 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e76, 0.000001f));
    let _e81 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e81 + 0.5f));
    let _e84 = fogType_1;
    if (_e84 == 1i) {
        let _e88 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e88 <= 0f) {
            return 0f;
        }
        let _e90 = viewDepth;
        let _e93 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e90 / _e93), 0f, 1f);
    }
    let _e98 = unnamed.advancedFogColorDensity[3u];
    let _e100 = viewDepth;
    opticalDepth = (max(_e98, 0f) * _e100);
    let _e102 = fogType_1;
    if (_e102 == 2i) {
        let _e104 = opticalDepth;
        return clamp((1f - exp(-(_e104))), 0f, 1f);
    }
    let _e109 = opticalDepth;
    let _e110 = opticalDepth;
    return clamp((1f - exp(-((_e109 * _e110)))), 0f, 1f);
}

fn wired_boost_legacy_lightmap_u0028_vf3_u003b(rgb: ptr<function, vec3<f32>>) -> vec3<f32> {
    var boosted: vec3<f32>;
    var peak: f32;

    let _e73 = (*rgb);
    let _e76 = unnamed.worldLightParams[0u];
    boosted = (_e73 * _e76);
    let _e79 = boosted[0u];
    let _e81 = boosted[1u];
    let _e83 = boosted[2u];
    peak = max(_e79, max(_e81, _e83));
    let _e86 = peak;
    if (_e86 > 1f) {
        let _e88 = peak;
        let _e89 = boosted;
        boosted = (_e89 / vec3(_e88));
    }
    let _e92 = boosted;
    return _e92;
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e74 = (*c);
    (*c) = max(_e74, vec3<f32>(0f, 0f, 0f));
    let _e76 = (*c);
    cutoff = (_e76 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e78 = (*c);
    lo = (_e78 / vec3(12.92f));
    let _e81 = (*c);
    hi = pow(((_e81 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e86 = hi;
    let _e87 = lo;
    let _e88 = cutoff;
    return mix(_e86, _e87, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e88));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param: vec3<f32>;
    var param_1: vec3<f32>;

    let _e76 = (*role);
    let _e78 = (*role);
    let _e83 = unnamed.packed_indices[(_e76 / 4u)][(_e78 % 4u)];
    let _e86 = (*role);
    let _e88 = (*role);
    let _e93 = unnamed.packed_indices[(_e86 / 4u)][(_e88 % 4u)];
    let _e98 = (*uv);
    let _e99 = textureSample(wired_bindless_images[(_e83 & 4095u)], wired_bindless_samplers[((_e93 >> bitcast<u32>(12i)) & 255u)], _e98);
    c_1 = _e99;
    let _e100 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e100))) == 0i) {
        let _e105 = c_1;
        param = _e105.xyz;
        let _e107 = sRGBToLinear_u0028_vf3_u003b((&param));
        c_1[0u] = _e107.x;
        c_1[1u] = _e107.y;
        c_1[2u] = _e107.z;
    }
    let _e114 = (*slot);
    if (lightmap_slot == (_e114 + 1i)) {
        let _e117 = c_1;
        param_1 = _e117.xyz;
        let _e119 = wired_boost_legacy_lightmap_u0028_vf3_u003b((&param_1));
        c_1[0u] = _e119.x;
        c_1[1u] = _e119.y;
        c_1[2u] = _e119.z;
    }
    let _e126 = c_1;
    return _e126;
}

fn main_1() {
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

    let _e85 = frag_color0In_1;
    param_2 = _e85.xyz;
    let _e87 = sRGBToLinear_u0028_vf3_u003b((&param_2));
    let _e89 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e87.x, _e87.y, _e87.z, _e89);
    param_3 = 0u;
    let _e94 = frag_tex_coord0_1;
    param_4 = _e94;
    param_5 = 0i;
    let _e95 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_3), (&param_4), (&param_5));
    sampledColor0_ = _e95;
    let _e96 = sampledColor0_;
    let _e97 = frag_color0_;
    color0_ = (_e96 * _e97);
    let _e99 = color0_;
    base = _e99;
    if override_type_6_3 {
        let _e101 = color0_[3u];
        if (_e101 == alpha_test_value) {
            discard;
        }
    } else {
        if override_type_6_4 {
            let _e104 = color0_[3u];
            if (_e104 >= alpha_test_value) {
                discard;
            }
        } else {
            if override_type_6_5 {
                let _e107 = color0_[3u];
                if (_e107 < alpha_test_value) {
                    discard;
                }
            }
        }
    }
    let _e109 = color0_;
    base = _e109;
    if override_type_6_6 {
        let _e112 = unnamed.worldLightParams[1u];
        wetness = clamp(_e112, 0f, 1f);
        let _e116 = unnamed.worldLightParams[2u];
        frost = clamp(_e116, 0f, 1f);
        let _e118 = base;
        luminance = dot(_e118.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e121 = wetness;
        let _e123 = base;
        let _e125 = (_e123.xyz * mix(1f, 0.82f, _e121));
        base[0u] = _e125.x;
        base[1u] = _e125.y;
        base[2u] = _e125.z;
        let _e132 = base;
        let _e134 = luminance;
        let _e136 = luminance;
        let _e138 = luminance;
        let _e140 = frost;
        let _e143 = mix(_e132.xyz, vec3<f32>((_e134 * 0.88f), (_e136 * 0.94f), _e138), vec3((_e140 * 0.55f)));
        base[0u] = _e143.x;
        base[1u] = _e143.y;
        base[2u] = _e143.z;
    }
    let _e152 = unnamed.materialFactors[0u];
    if (_e152 >= 0f) {
        let _e155 = unnamed.materialBaseColor;
        let _e156 = base;
        base = (_e156 * _e155);
    }
    let _e158 = color0_;
    let _e161 = unnamed.emissionRadiance;
    let _e164 = base;
    let _e166 = (_e164.xyz + (_e158.xyz * _e161.xyz));
    base[0u] = _e166.x;
    base[1u] = _e166.y;
    base[2u] = _e166.z;
    let _e173 = wired_advanced_fog_enabled_u0028_();
    if _e173 {
        let _e174 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e174;
        let _e175 = base;
        let _e178 = unnamed.advancedFogColorDensity;
        let _e180 = fogAmount;
        let _e182 = mix(_e175.xyz, _e178.xyz, vec3(_e180));
        base[0u] = _e182.x;
        base[1u] = _e182.y;
        base[2u] = _e182.z;
    }
    if override_type_6_7 {
        let _e190 = base[3u];
        if (_e190 == 0f) {
            discard;
        }
    } else {
        if override_type_6_8 {
            let _e192 = base;
            let _e194 = base;
            if (dot(_e192.xyz, _e194.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e198 = base;
    out_color = _e198;
    let _e200 = color0_[3u];
    param_6 = _e200;
    let _e201 = wiredTemporalAtestConfidence_u0028_f1_u003b((&param_6));
    param_7 = _e201;
    let _e202 = base;
    param_8 = _e202;
    wiredTemporalWriteAux_u0028_f1_u003b_vf4_u003b((&param_7), (&param_8));
    return;
}

@fragment
fn main(@builtin(position) gl_FragCoord: vec4<f32>, @location(0) frag_color0In: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(10) temporalCurrentClip: vec4<f32>, @location(11) temporalPreviousClip: vec4<f32>, @location(12) @interpolate(flat) temporalOutcome: u32) -> FragmentOutput {
    gl_FragCoord_1 = gl_FragCoord;
    frag_color0In_1 = frag_color0In;
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
