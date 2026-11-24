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
var<private> frag_tex_coord0_1: vec2<f32>;
var<private> out_color: vec4<f32>;
var<private> temporalCurrentClip_1: vec4<f32>;
var<private> temporalPreviousClip_1: vec4<f32>;
var<private> temporalOutcome_1: u32;

fn wiredTemporalWriteAux_u0028_f1_u003b_vf4_u003b(coverageConfidence: ptr<function, f32>, sceneColor: ptr<function, vec4<f32>>) {
    out_temporal_velocity = vec2<f32>(0f, 0f);
    out_temporal_validity = vec4<f32>(0f, 0f, 0f, 0f);
    let _e74 = (*sceneColor)[0u];
    let _e76 = (*sceneColor)[1u];
    let _e78 = (*sceneColor)[2u];
    let _e83 = (*sceneColor)[3u];
    out_temporal_validity = vec4<f32>(clamp(max(_e74, max(_e76, _e78)), 0f, 1f), 0f, 0f, clamp(_e83, 0f, 1f));
    return;
}

fn wiredTemporalAtestConfidence_u0028_f1_u003b(alpha: ptr<function, f32>) -> f32 {
    var span: f32;

    if override_type_6_ {
        span = max(max(alpha_test_value, (1f - alpha_test_value)), 0.000001f);
        let _e76 = (*alpha);
        let _e79 = span;
        return clamp((abs((_e76 - alpha_test_value)) / _e79), 0f, 1f);
    } else {
        if override_type_6_1 {
            let _e82 = (*alpha);
            return clamp(((alpha_test_value - _e82) / max(alpha_test_value, 0.000001f)), 0f, 1f);
        } else {
            if override_type_6_2 {
                let _e87 = (*alpha);
                return clamp(((_e87 - alpha_test_value) / max((1f - alpha_test_value), 0.000001f)), 0f, 1f);
            }
        }
    }
    return 1f;
}

fn wired_advanced_fog_enabled_u0028_() -> bool {
    var fogType: i32;

    let _e74 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e74 + 0.5f));
    let _e79 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e81 = fogType;
    let _e84 = fogType;
    return (((_e79 > 0.5f) && (_e81 >= 1i)) && (_e84 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e74 = wired_advanced_fog_enabled_u0028_();
    if !(_e74) {
        return 0f;
    }
    let _e77 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e77, 0.000001f));
    let _e82 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e82 + 0.5f));
    let _e85 = fogType_1;
    if (_e85 == 1i) {
        let _e89 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e89 <= 0f) {
            return 0f;
        }
        let _e91 = viewDepth;
        let _e94 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e91 / _e94), 0f, 1f);
    }
    let _e99 = unnamed.advancedFogColorDensity[3u];
    let _e101 = viewDepth;
    opticalDepth = (max(_e99, 0f) * _e101);
    let _e103 = fogType_1;
    if (_e103 == 2i) {
        let _e105 = opticalDepth;
        return clamp((1f - exp(-(_e105))), 0f, 1f);
    }
    let _e110 = opticalDepth;
    let _e111 = opticalDepth;
    return clamp((1f - exp(-((_e110 * _e111)))), 0f, 1f);
}

fn wired_boost_legacy_lightmap_u0028_vf3_u003b(rgb: ptr<function, vec3<f32>>) -> vec3<f32> {
    var boosted: vec3<f32>;
    var peak: f32;

    let _e74 = (*rgb);
    let _e77 = unnamed.worldLightParams[0u];
    boosted = (_e74 * _e77);
    let _e80 = boosted[0u];
    let _e82 = boosted[1u];
    let _e84 = boosted[2u];
    peak = max(_e80, max(_e82, _e84));
    let _e87 = peak;
    if (_e87 > 1f) {
        let _e89 = peak;
        let _e90 = boosted;
        boosted = (_e90 / vec3(_e89));
    }
    let _e93 = boosted;
    return _e93;
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e75 = (*c);
    (*c) = max(_e75, vec3<f32>(0f, 0f, 0f));
    let _e77 = (*c);
    cutoff = (_e77 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e79 = (*c);
    lo = (_e79 / vec3(12.92f));
    let _e82 = (*c);
    hi = pow(((_e82 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e87 = hi;
    let _e88 = lo;
    let _e89 = cutoff;
    return mix(_e87, _e88, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e89));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param: vec3<f32>;
    var param_1: vec3<f32>;

    let _e77 = (*role);
    let _e79 = (*role);
    let _e84 = unnamed.packed_indices[(_e77 / 4u)][(_e79 % 4u)];
    let _e87 = (*role);
    let _e89 = (*role);
    let _e94 = unnamed.packed_indices[(_e87 / 4u)][(_e89 % 4u)];
    let _e99 = (*uv);
    let _e100 = textureSample(wired_bindless_images[(_e84 & 4095u)], wired_bindless_samplers[((_e94 >> bitcast<u32>(12i)) & 255u)], _e99);
    c_1 = _e100;
    let _e101 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e101))) == 0i) {
        let _e106 = c_1;
        param = _e106.xyz;
        let _e108 = sRGBToLinear_u0028_vf3_u003b((&param));
        c_1[0u] = _e108.x;
        c_1[1u] = _e108.y;
        c_1[2u] = _e108.z;
    }
    let _e115 = (*slot);
    if (lightmap_slot == (_e115 + 1i)) {
        let _e118 = c_1;
        param_1 = _e118.xyz;
        let _e120 = wired_boost_legacy_lightmap_u0028_vf3_u003b((&param_1));
        c_1[0u] = _e120.x;
        c_1[1u] = _e120.y;
        c_1[2u] = _e120.z;
    }
    let _e127 = c_1;
    return _e127;
}

fn main_1() {
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

    param_2 = 0u;
    let _e85 = frag_tex_coord0_1;
    param_3 = _e85;
    param_4 = 0i;
    let _e86 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_2), (&param_3), (&param_4));
    sampledColor0_ = _e86;
    frag_color[0u] = identity_color;
    frag_color[1u] = identity_color;
    frag_color[2u] = identity_color;
    frag_color[3u] = identity_alpha;
    let _e91 = sampledColor0_;
    let _e92 = frag_color;
    color0_ = (_e91 * _e92);
    let _e94 = color0_;
    base = _e94;
    if override_type_6_3 {
        let _e96 = color0_[3u];
        if (_e96 == alpha_test_value) {
            discard;
        }
    } else {
        if override_type_6_4 {
            let _e99 = color0_[3u];
            if (_e99 >= alpha_test_value) {
                discard;
            }
        } else {
            if override_type_6_5 {
                let _e102 = color0_[3u];
                if (_e102 < alpha_test_value) {
                    discard;
                }
            }
        }
    }
    let _e104 = color0_;
    base = _e104;
    if override_type_6_6 {
        let _e107 = unnamed.worldLightParams[1u];
        wetness = clamp(_e107, 0f, 1f);
        let _e111 = unnamed.worldLightParams[2u];
        frost = clamp(_e111, 0f, 1f);
        let _e113 = base;
        luminance = dot(_e113.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e116 = wetness;
        let _e118 = base;
        let _e120 = (_e118.xyz * mix(1f, 0.82f, _e116));
        base[0u] = _e120.x;
        base[1u] = _e120.y;
        base[2u] = _e120.z;
        let _e127 = base;
        let _e129 = luminance;
        let _e131 = luminance;
        let _e133 = luminance;
        let _e135 = frost;
        let _e138 = mix(_e127.xyz, vec3<f32>((_e129 * 0.88f), (_e131 * 0.94f), _e133), vec3((_e135 * 0.55f)));
        base[0u] = _e138.x;
        base[1u] = _e138.y;
        base[2u] = _e138.z;
    }
    let _e147 = unnamed.materialFactors[0u];
    if (_e147 >= 0f) {
        let _e150 = unnamed.materialBaseColor;
        let _e151 = base;
        base = (_e151 * _e150);
    }
    let _e153 = color0_;
    let _e156 = unnamed.emissionRadiance;
    let _e159 = base;
    let _e161 = (_e159.xyz + (_e153.xyz * _e156.xyz));
    base[0u] = _e161.x;
    base[1u] = _e161.y;
    base[2u] = _e161.z;
    let _e168 = wired_advanced_fog_enabled_u0028_();
    if _e168 {
        let _e169 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e169;
        let _e170 = base;
        let _e173 = unnamed.advancedFogColorDensity;
        let _e175 = fogAmount;
        let _e177 = mix(_e170.xyz, _e173.xyz, vec3(_e175));
        base[0u] = _e177.x;
        base[1u] = _e177.y;
        base[2u] = _e177.z;
    }
    if override_type_6_7 {
        let _e185 = base[3u];
        if (_e185 == 0f) {
            discard;
        }
    } else {
        if override_type_6_8 {
            let _e187 = base;
            let _e189 = base;
            if (dot(_e187.xyz, _e189.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e193 = base;
    out_color = _e193;
    let _e195 = color0_[3u];
    param_5 = _e195;
    let _e196 = wiredTemporalAtestConfidence_u0028_f1_u003b((&param_5));
    param_6 = _e196;
    let _e197 = base;
    param_7 = _e197;
    wiredTemporalWriteAux_u0028_f1_u003b_vf4_u003b((&param_6), (&param_7));
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
