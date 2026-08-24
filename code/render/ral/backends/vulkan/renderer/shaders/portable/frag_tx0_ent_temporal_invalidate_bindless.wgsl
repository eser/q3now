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
@id(7) override discard_mode: i32 = 0i;
override override_type_4_6: bool = (discard_mode == 1i);
override override_type_4_7: bool = (discard_mode == 2i);
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
        let _e61 = (*alpha);
        let _e64 = span;
        return clamp((abs((_e61 - alpha_test_value)) / _e64), 0f, 1f);
    } else {
        if override_type_4_1 {
            let _e67 = (*alpha);
            return clamp(((alpha_test_value - _e67) / max(alpha_test_value, 0.000001f)), 0f, 1f);
        } else {
            if override_type_4_2 {
                let _e72 = (*alpha);
                return clamp(((_e72 - alpha_test_value) / max((1f - alpha_test_value), 0.000001f)), 0f, 1f);
            }
        }
    }
    return 1f;
}

fn wired_advanced_fog_enabled_u0028_() -> bool {
    var fogType: i32;

    let _e59 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e59 + 0.5f));
    let _e64 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e66 = fogType;
    let _e69 = fogType;
    return (((_e64 > 0.5f) && (_e66 >= 1i)) && (_e69 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e59 = wired_advanced_fog_enabled_u0028_();
    if !(_e59) {
        return 0f;
    }
    let _e62 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e62, 0.000001f));
    let _e67 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e67 + 0.5f));
    let _e70 = fogType_1;
    if (_e70 == 1i) {
        let _e74 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e74 <= 0f) {
            return 0f;
        }
        let _e76 = viewDepth;
        let _e79 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e76 / _e79), 0f, 1f);
    }
    let _e84 = unnamed.advancedFogColorDensity[3u];
    let _e86 = viewDepth;
    opticalDepth = (max(_e84, 0f) * _e86);
    let _e88 = fogType_1;
    if (_e88 == 2i) {
        let _e90 = opticalDepth;
        return clamp((1f - exp(-(_e90))), 0f, 1f);
    }
    let _e95 = opticalDepth;
    let _e96 = opticalDepth;
    return clamp((1f - exp(-((_e95 * _e96)))), 0f, 1f);
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e60 = (*c);
    (*c) = max(_e60, vec3<f32>(0f, 0f, 0f));
    let _e62 = (*c);
    cutoff = (_e62 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e64 = (*c);
    lo = (_e64 / vec3(12.92f));
    let _e67 = (*c);
    hi = pow(((_e67 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e72 = hi;
    let _e73 = lo;
    let _e74 = cutoff;
    return mix(_e72, _e73, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e74));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param: vec3<f32>;

    let _e61 = (*role);
    let _e63 = (*role);
    let _e68 = unnamed.packed_indices[(_e61 / 4u)][(_e63 % 4u)];
    let _e71 = (*role);
    let _e73 = (*role);
    let _e78 = unnamed.packed_indices[(_e71 / 4u)][(_e73 % 4u)];
    let _e83 = (*uv);
    let _e84 = textureSample(wired_bindless_images[(_e68 & 4095u)], wired_bindless_samplers[((_e78 >> bitcast<u32>(12i)) & 255u)], _e83);
    c_1 = _e84;
    let _e85 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e85))) == 0i) {
        let _e90 = c_1;
        param = _e90.xyz;
        let _e92 = sRGBToLinear_u0028_vf3_u003b((&param));
        c_1[0u] = _e92.x;
        c_1[1u] = _e92.y;
        c_1[2u] = _e92.z;
    }
    let _e99 = (*slot);
    if (lightmap_slot == (_e99 + 1i)) {
        let _e104 = unnamed.worldLightParams[0u];
        let _e105 = c_1;
        let _e107 = (_e105.xyz * _e104);
        c_1[0u] = _e107.x;
        c_1[1u] = _e107.y;
        c_1[2u] = _e107.z;
    }
    let _e114 = c_1;
    return _e114;
}

fn main_1() {
    var color0_: vec4<f32>;
    var param_1: u32;
    var param_2: vec2<f32>;
    var param_3: i32;
    var base: vec4<f32>;
    var fogAmount: f32;
    var param_4: f32;
    var param_5: f32;

    param_1 = 0u;
    let _e64 = frag_tex_coord0_1;
    param_2 = _e64;
    param_3 = 0i;
    let _e65 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_1), (&param_2), (&param_3));
    let _e67 = unnamed.ent_color0_;
    color0_ = (_e65 * _e67);
    let _e69 = color0_;
    base = _e69;
    if override_type_4_3 {
        let _e71 = color0_[3u];
        if (_e71 == alpha_test_value) {
            discard;
        }
    } else {
        if override_type_4_4 {
            let _e74 = color0_[3u];
            if (_e74 >= alpha_test_value) {
                discard;
            }
        } else {
            if override_type_4_5 {
                let _e77 = color0_[3u];
                if (_e77 < alpha_test_value) {
                    discard;
                }
            }
        }
    }
    let _e79 = color0_;
    base = _e79;
    let _e80 = wired_advanced_fog_enabled_u0028_();
    if _e80 {
        let _e81 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e81;
        let _e82 = base;
        let _e85 = unnamed.advancedFogColorDensity;
        let _e87 = fogAmount;
        let _e89 = mix(_e82.xyz, _e85.xyz, vec3(_e87));
        base[0u] = _e89.x;
        base[1u] = _e89.y;
        base[2u] = _e89.z;
    }
    if override_type_4_6 {
        let _e97 = base[3u];
        if (_e97 == 0f) {
            discard;
        }
    } else {
        if override_type_4_7 {
            let _e99 = base;
            let _e101 = base;
            if (dot(_e99.xyz, _e101.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e105 = base;
    out_color = _e105;
    let _e107 = color0_[3u];
    param_4 = _e107;
    let _e108 = wiredTemporalAtestConfidence_u0028_f1_u003b((&param_4));
    param_5 = _e108;
    wiredTemporalWriteAux_u0028_f1_u003b((&param_5));
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
