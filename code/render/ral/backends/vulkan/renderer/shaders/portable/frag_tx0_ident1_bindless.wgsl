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
}

@id(4) override tex_domain: i32 = 0i;
@id(26) override lightmap_slot: i32 = 0i;
@id(0) override alpha_test_func: i32 = 0i;
override override_type_3_: bool = (alpha_test_func == 1i);
@id(1) override alpha_test_value: f32 = 0f;
override override_type_3_1: bool = (alpha_test_func == 2i);
override override_type_3_2: bool = (alpha_test_func == 3i);
@id(7) override discard_mode: i32 = 0i;
override override_type_3_3: bool = (discard_mode == 1i);
override override_type_3_4: bool = (discard_mode == 2i);
@id(3) override alpha_to_coverage: i32 = 0i;

@group(0) @binding(0)
var<uniform> unnamed: UBO;
var<private> gl_FragCoord_1: vec4<f32>;
@group(1) @binding(0)
var wired_bindless_images: binding_array<texture_2d<f32>>;
@group(1) @binding(1)
var wired_bindless_samplers: binding_array<sampler>;
var<private> frag_tex_coord0_1: vec2<f32>;
var<private> out_color: vec4<f32>;

fn wired_advanced_fog_enabled_u0028_() -> bool {
    var fogType: i32;

    let _e50 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e50 + 0.5f));
    let _e55 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e57 = fogType;
    let _e60 = fogType;
    return (((_e55 > 0.5f) && (_e57 >= 1i)) && (_e60 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e50 = wired_advanced_fog_enabled_u0028_();
    if !(_e50) {
        return 0f;
    }
    let _e53 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e53, 0.000001f));
    let _e58 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e58 + 0.5f));
    let _e61 = fogType_1;
    if (_e61 == 1i) {
        let _e65 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e65 <= 0f) {
            return 0f;
        }
        let _e67 = viewDepth;
        let _e70 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e67 / _e70), 0f, 1f);
    }
    let _e75 = unnamed.advancedFogColorDensity[3u];
    let _e77 = viewDepth;
    opticalDepth = (max(_e75, 0f) * _e77);
    let _e79 = fogType_1;
    if (_e79 == 2i) {
        let _e81 = opticalDepth;
        return clamp((1f - exp(-(_e81))), 0f, 1f);
    }
    let _e86 = opticalDepth;
    let _e87 = opticalDepth;
    return clamp((1f - exp(-((_e86 * _e87)))), 0f, 1f);
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e51 = (*c);
    (*c) = max(_e51, vec3<f32>(0f, 0f, 0f));
    let _e53 = (*c);
    cutoff = (_e53 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e55 = (*c);
    lo = (_e55 / vec3(12.92f));
    let _e58 = (*c);
    hi = pow(((_e58 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e63 = hi;
    let _e64 = lo;
    let _e65 = cutoff;
    return mix(_e63, _e64, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e65));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param: vec3<f32>;

    let _e52 = (*role);
    let _e54 = (*role);
    let _e59 = unnamed.packed_indices[(_e52 / 4u)][(_e54 % 4u)];
    let _e62 = (*role);
    let _e64 = (*role);
    let _e69 = unnamed.packed_indices[(_e62 / 4u)][(_e64 % 4u)];
    let _e74 = (*uv);
    let _e75 = textureSample(wired_bindless_images[(_e59 & 4095u)], wired_bindless_samplers[((_e69 >> bitcast<u32>(12i)) & 255u)], _e74);
    c_1 = _e75;
    let _e76 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e76))) == 0i) {
        let _e81 = c_1;
        param = _e81.xyz;
        let _e83 = sRGBToLinear_u0028_vf3_u003b((&param));
        c_1[0u] = _e83.x;
        c_1[1u] = _e83.y;
        c_1[2u] = _e83.z;
    }
    let _e90 = (*slot);
    if (lightmap_slot == (_e90 + 1i)) {
        let _e95 = unnamed.worldLightParams[0u];
        let _e96 = c_1;
        let _e98 = (_e96.xyz * _e95);
        c_1[0u] = _e98.x;
        c_1[1u] = _e98.y;
        c_1[2u] = _e98.z;
    }
    let _e105 = c_1;
    return _e105;
}

fn main_1() {
    var color0_: vec4<f32>;
    var param_1: u32;
    var param_2: vec2<f32>;
    var param_3: i32;
    var base: vec4<f32>;
    var fogAmount: f32;

    param_1 = 0u;
    let _e53 = frag_tex_coord0_1;
    param_2 = _e53;
    param_3 = 0i;
    let _e54 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_1), (&param_2), (&param_3));
    color0_ = _e54;
    let _e55 = color0_;
    base = _e55;
    if override_type_3_ {
        let _e57 = color0_[3u];
        if (_e57 == alpha_test_value) {
            discard;
        }
    } else {
        if override_type_3_1 {
            let _e60 = color0_[3u];
            if (_e60 >= alpha_test_value) {
                discard;
            }
        } else {
            if override_type_3_2 {
                let _e63 = color0_[3u];
                if (_e63 < alpha_test_value) {
                    discard;
                }
            }
        }
    }
    let _e65 = color0_;
    base = _e65;
    let _e66 = wired_advanced_fog_enabled_u0028_();
    if _e66 {
        let _e67 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e67;
        let _e68 = base;
        let _e71 = unnamed.advancedFogColorDensity;
        let _e73 = fogAmount;
        let _e75 = mix(_e68.xyz, _e71.xyz, vec3(_e73));
        base[0u] = _e75.x;
        base[1u] = _e75.y;
        base[2u] = _e75.z;
    }
    if override_type_3_3 {
        let _e83 = base[3u];
        if (_e83 == 0f) {
            discard;
        }
    } else {
        if override_type_3_4 {
            let _e85 = base;
            let _e87 = base;
            if (dot(_e85.xyz, _e87.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e91 = base;
    out_color = _e91;
    return;
}

@fragment
fn main(@builtin(position) gl_FragCoord: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>) -> @location(0) vec4<f32> {
    gl_FragCoord_1 = gl_FragCoord;
    frag_tex_coord0_1 = frag_tex_coord0_;
    main_1();
    let _e5 = out_color;
    return _e5;
}
