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
@id(8) override identity_color: f32 = 1f;
@id(9) override identity_alpha: f32 = 1f;
@id(0) override alpha_test_func: i32 = 0i;
override override_type_3_: bool = (alpha_test_func == 1i);
@id(1) override alpha_test_value: f32 = 0f;
override override_type_3_1: bool = (alpha_test_func == 2i);
override override_type_3_2: bool = (alpha_test_func == 3i);
@id(7) override discard_mode: i32 = 0i;
override override_type_3_3: bool = (discard_mode == 1i);
override override_type_3_4: bool = (discard_mode == 2i);
@id(11) override depth_fade_scale: f32 = 2f;
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

    let _e54 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e54 + 0.5f));
    let _e59 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e61 = fogType;
    let _e64 = fogType;
    return (((_e59 > 0.5f) && (_e61 >= 1i)) && (_e64 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e54 = wired_advanced_fog_enabled_u0028_();
    if !(_e54) {
        return 0f;
    }
    let _e57 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e57, 0.000001f));
    let _e62 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e62 + 0.5f));
    let _e65 = fogType_1;
    if (_e65 == 1i) {
        let _e69 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e69 <= 0f) {
            return 0f;
        }
        let _e71 = viewDepth;
        let _e74 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e71 / _e74), 0f, 1f);
    }
    let _e79 = unnamed.advancedFogColorDensity[3u];
    let _e81 = viewDepth;
    opticalDepth = (max(_e79, 0f) * _e81);
    let _e83 = fogType_1;
    if (_e83 == 2i) {
        let _e85 = opticalDepth;
        return clamp((1f - exp(-(_e85))), 0f, 1f);
    }
    let _e90 = opticalDepth;
    let _e91 = opticalDepth;
    return clamp((1f - exp(-((_e90 * _e91)))), 0f, 1f);
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e55 = (*c);
    (*c) = max(_e55, vec3<f32>(0f, 0f, 0f));
    let _e57 = (*c);
    cutoff = (_e57 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e59 = (*c);
    lo = (_e59 / vec3(12.92f));
    let _e62 = (*c);
    hi = pow(((_e62 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e67 = hi;
    let _e68 = lo;
    let _e69 = cutoff;
    return mix(_e67, _e68, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e69));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param: vec3<f32>;

    let _e56 = (*role);
    let _e58 = (*role);
    let _e63 = unnamed.packed_indices[(_e56 / 4u)][(_e58 % 4u)];
    let _e66 = (*role);
    let _e68 = (*role);
    let _e73 = unnamed.packed_indices[(_e66 / 4u)][(_e68 % 4u)];
    let _e78 = (*uv);
    let _e79 = textureSample(wired_bindless_images[(_e63 & 4095u)], wired_bindless_samplers[((_e73 >> bitcast<u32>(12i)) & 255u)], _e78);
    c_1 = _e79;
    let _e80 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e80))) == 0i) {
        let _e85 = c_1;
        param = _e85.xyz;
        let _e87 = sRGBToLinear_u0028_vf3_u003b((&param));
        c_1[0u] = _e87.x;
        c_1[1u] = _e87.y;
        c_1[2u] = _e87.z;
    }
    let _e94 = (*slot);
    if (lightmap_slot == (_e94 + 1i)) {
        let _e99 = unnamed.worldLightParams[0u];
        let _e100 = c_1;
        let _e102 = (_e100.xyz * _e99);
        c_1[0u] = _e102.x;
        c_1[1u] = _e102.y;
        c_1[2u] = _e102.z;
    }
    let _e109 = c_1;
    return _e109;
}

fn main_1() {
    var frag_color: vec4<f32>;
    var color0_: vec4<f32>;
    var param_1: u32;
    var param_2: vec2<f32>;
    var param_3: i32;
    var base: vec4<f32>;
    var fogAmount: f32;
    var screenUV: vec2<f32>;
    var sceneDepth: f32;
    var fragDepth: f32;
    var depthDiff: f32;
    var fadeFactor: f32;

    frag_color[0u] = identity_color;
    frag_color[1u] = identity_color;
    frag_color[2u] = identity_color;
    frag_color[3u] = identity_alpha;
    param_1 = 0u;
    let _e67 = frag_tex_coord0_1;
    param_2 = _e67;
    param_3 = 0i;
    let _e68 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_1), (&param_2), (&param_3));
    let _e69 = frag_color;
    color0_ = (_e68 * _e69);
    let _e71 = color0_;
    base = _e71;
    if override_type_3_ {
        let _e73 = color0_[3u];
        if (_e73 == alpha_test_value) {
            discard;
        }
    } else {
        if override_type_3_1 {
            let _e76 = color0_[3u];
            if (_e76 >= alpha_test_value) {
                discard;
            }
        } else {
            if override_type_3_2 {
                let _e79 = color0_[3u];
                if (_e79 < alpha_test_value) {
                    discard;
                }
            }
        }
    }
    let _e81 = color0_;
    base = _e81;
    let _e82 = wired_advanced_fog_enabled_u0028_();
    if _e82 {
        let _e83 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e83;
        let _e84 = base;
        let _e87 = unnamed.advancedFogColorDensity;
        let _e89 = fogAmount;
        let _e91 = mix(_e84.xyz, _e87.xyz, vec3(_e89));
        base[0u] = _e91.x;
        base[1u] = _e91.y;
        base[2u] = _e91.z;
    }
    if override_type_3_3 {
        let _e99 = base[3u];
        if (_e99 == 0f) {
            discard;
        }
    } else {
        if override_type_3_4 {
            let _e101 = base;
            let _e103 = base;
            if (dot(_e101.xyz, _e103.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e107 = gl_FragCoord_1;
    let _e112 = unnamed.packed_indices[1i][0u];
    let _e118 = unnamed.packed_indices[1i][0u];
    let _e123 = textureDimensions(wired_bindless_images[(_e112 & 4095u)], 0i);
    screenUV = (_e107.xy / vec2<f32>(vec2<i32>(_e123)));
    let _e130 = unnamed.packed_indices[1i][0u];
    let _e136 = unnamed.packed_indices[1i][0u];
    let _e141 = screenUV;
    let _e142 = textureSample(wired_bindless_images[(_e130 & 4095u)], wired_bindless_samplers[((_e136 >> bitcast<u32>(12i)) & 255u)], _e141);
    sceneDepth = _e142.x;
    let _e145 = gl_FragCoord_1[2u];
    fragDepth = _e145;
    let _e146 = fragDepth;
    let _e147 = sceneDepth;
    depthDiff = (_e146 - _e147);
    let _e150 = depthDiff;
    fadeFactor = smoothstep(0f, (depth_fade_scale * 0.0005f), _e150);
    let _e152 = fadeFactor;
    let _e154 = base[3u];
    base[3u] = (_e154 * _e152);
    let _e157 = fadeFactor;
    let _e158 = base;
    let _e160 = (_e158.xyz * _e157);
    base[0u] = _e160.x;
    base[1u] = _e160.y;
    base[2u] = _e160.z;
    let _e167 = base;
    out_color = _e167;
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
