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
@id(7) override discard_mode: i32 = 0i;
override override_type_4_7: bool = (discard_mode == 1i);
override override_type_4_8: bool = (discard_mode == 2i);
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
        let _e71 = (*alpha);
        let _e74 = span;
        return clamp((abs((_e71 - alpha_test_value)) / _e74), 0f, 1f);
    } else {
        if override_type_4_1 {
            let _e77 = (*alpha);
            return clamp(((alpha_test_value - _e77) / max(alpha_test_value, 0.000001f)), 0f, 1f);
        } else {
            if override_type_4_2 {
                let _e82 = (*alpha);
                return clamp(((_e82 - alpha_test_value) / max((1f - alpha_test_value), 0.000001f)), 0f, 1f);
            }
        }
    }
    return 1f;
}

fn wired_advanced_fog_enabled_u0028_() -> bool {
    var fogType: i32;

    let _e69 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e69 + 0.5f));
    let _e74 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e76 = fogType;
    let _e79 = fogType;
    return (((_e74 > 0.5f) && (_e76 >= 1i)) && (_e79 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e69 = wired_advanced_fog_enabled_u0028_();
    if !(_e69) {
        return 0f;
    }
    let _e72 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e72, 0.000001f));
    let _e77 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e77 + 0.5f));
    let _e80 = fogType_1;
    if (_e80 == 1i) {
        let _e84 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e84 <= 0f) {
            return 0f;
        }
        let _e86 = viewDepth;
        let _e89 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e86 / _e89), 0f, 1f);
    }
    let _e94 = unnamed.advancedFogColorDensity[3u];
    let _e96 = viewDepth;
    opticalDepth = (max(_e94, 0f) * _e96);
    let _e98 = fogType_1;
    if (_e98 == 2i) {
        let _e100 = opticalDepth;
        return clamp((1f - exp(-(_e100))), 0f, 1f);
    }
    let _e105 = opticalDepth;
    let _e106 = opticalDepth;
    return clamp((1f - exp(-((_e105 * _e106)))), 0f, 1f);
}

fn wired_boost_legacy_lightmap_u0028_vf3_u003b(rgb: ptr<function, vec3<f32>>) -> vec3<f32> {
    var boosted: vec3<f32>;
    var peak: f32;

    let _e69 = (*rgb);
    let _e72 = unnamed.worldLightParams[0u];
    boosted = (_e69 * _e72);
    let _e75 = boosted[0u];
    let _e77 = boosted[1u];
    let _e79 = boosted[2u];
    peak = max(_e75, max(_e77, _e79));
    let _e82 = peak;
    if (_e82 > 1f) {
        let _e84 = peak;
        let _e85 = boosted;
        boosted = (_e85 / vec3(_e84));
    }
    let _e88 = boosted;
    return _e88;
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e70 = (*c);
    (*c) = max(_e70, vec3<f32>(0f, 0f, 0f));
    let _e72 = (*c);
    cutoff = (_e72 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e74 = (*c);
    lo = (_e74 / vec3(12.92f));
    let _e77 = (*c);
    hi = pow(((_e77 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e82 = hi;
    let _e83 = lo;
    let _e84 = cutoff;
    return mix(_e82, _e83, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e84));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param: vec3<f32>;
    var param_1: vec3<f32>;

    let _e72 = (*role);
    let _e74 = (*role);
    let _e79 = unnamed.packed_indices[(_e72 / 4u)][(_e74 % 4u)];
    let _e82 = (*role);
    let _e84 = (*role);
    let _e89 = unnamed.packed_indices[(_e82 / 4u)][(_e84 % 4u)];
    let _e94 = (*uv);
    let _e95 = textureSample(wired_bindless_images[(_e79 & 4095u)], wired_bindless_samplers[((_e89 >> bitcast<u32>(12i)) & 255u)], _e94);
    c_1 = _e95;
    let _e96 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e96))) == 0i) {
        let _e101 = c_1;
        param = _e101.xyz;
        let _e103 = sRGBToLinear_u0028_vf3_u003b((&param));
        c_1[0u] = _e103.x;
        c_1[1u] = _e103.y;
        c_1[2u] = _e103.z;
    }
    let _e110 = (*slot);
    if (lightmap_slot == (_e110 + 1i)) {
        let _e113 = c_1;
        param_1 = _e113.xyz;
        let _e115 = wired_boost_legacy_lightmap_u0028_vf3_u003b((&param_1));
        c_1[0u] = _e115.x;
        c_1[1u] = _e115.y;
        c_1[2u] = _e115.z;
    }
    let _e122 = c_1;
    return _e122;
}

fn main_1() {
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

    param_2 = 0u;
    let _e77 = frag_tex_coord0_1;
    param_3 = _e77;
    param_4 = 0i;
    let _e78 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_2), (&param_3), (&param_4));
    let _e80 = unnamed.ent_color0_;
    color0_ = (_e78 * _e80);
    let _e82 = color0_;
    base = _e82;
    if override_type_4_3 {
        let _e84 = color0_[3u];
        if (_e84 == alpha_test_value) {
            discard;
        }
    } else {
        if override_type_4_4 {
            let _e87 = color0_[3u];
            if (_e87 >= alpha_test_value) {
                discard;
            }
        } else {
            if override_type_4_5 {
                let _e90 = color0_[3u];
                if (_e90 < alpha_test_value) {
                    discard;
                }
            }
        }
    }
    let _e92 = color0_;
    base = _e92;
    if override_type_4_6 {
        let _e95 = unnamed.worldLightParams[1u];
        wetness = clamp(_e95, 0f, 1f);
        let _e99 = unnamed.worldLightParams[2u];
        frost = clamp(_e99, 0f, 1f);
        let _e101 = base;
        luminance = dot(_e101.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e104 = wetness;
        let _e106 = base;
        let _e108 = (_e106.xyz * mix(1f, 0.82f, _e104));
        base[0u] = _e108.x;
        base[1u] = _e108.y;
        base[2u] = _e108.z;
        let _e115 = base;
        let _e117 = luminance;
        let _e119 = luminance;
        let _e121 = luminance;
        let _e123 = frost;
        let _e126 = mix(_e115.xyz, vec3<f32>((_e117 * 0.88f), (_e119 * 0.94f), _e121), vec3((_e123 * 0.55f)));
        base[0u] = _e126.x;
        base[1u] = _e126.y;
        base[2u] = _e126.z;
    }
    let _e133 = color0_;
    let _e136 = unnamed.emissionRadiance;
    let _e139 = base;
    let _e141 = (_e139.xyz + (_e133.xyz * _e136.xyz));
    base[0u] = _e141.x;
    base[1u] = _e141.y;
    base[2u] = _e141.z;
    let _e148 = wired_advanced_fog_enabled_u0028_();
    if _e148 {
        let _e149 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e149;
        let _e150 = base;
        let _e153 = unnamed.advancedFogColorDensity;
        let _e155 = fogAmount;
        let _e157 = mix(_e150.xyz, _e153.xyz, vec3(_e155));
        base[0u] = _e157.x;
        base[1u] = _e157.y;
        base[2u] = _e157.z;
    }
    if override_type_4_7 {
        let _e165 = base[3u];
        if (_e165 == 0f) {
            discard;
        }
    } else {
        if override_type_4_8 {
            let _e167 = base;
            let _e169 = base;
            if (dot(_e167.xyz, _e169.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e173 = base;
    out_color = _e173;
    let _e175 = color0_[3u];
    param_5 = _e175;
    let _e176 = wiredTemporalAtestConfidence_u0028_f1_u003b((&param_5));
    param_6 = _e176;
    wiredTemporalWriteAux_u0028_f1_u003b((&param_6));
    return;
}

@fragment
fn main(@builtin(position) gl_FragCoord: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(10) temporalCurrentClip: vec4<f32>, @location(11) temporalPreviousClip: vec4<f32>, @location(12) @interpolate(flat) temporalOutcome: u32) -> FragmentOutput {
    gl_FragCoord_1 = gl_FragCoord;
    frag_tex_coord0_1 = frag_tex_coord0_;
    temporalCurrentClip_1 = temporalCurrentClip;
    temporalPreviousClip_1 = temporalPreviousClip;
    temporalOutcome_1 = temporalOutcome;
    main_1();
    let _e13 = out_temporal_velocity;
    let _e14 = out_temporal_validity;
    let _e15 = out_color;
    return FragmentOutput(_e13, _e14, _e15);
}
