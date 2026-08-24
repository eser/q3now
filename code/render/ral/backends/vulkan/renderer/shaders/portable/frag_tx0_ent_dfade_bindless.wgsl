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

    let _e52 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e52 + 0.5f));
    let _e57 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e59 = fogType;
    let _e62 = fogType;
    return (((_e57 > 0.5f) && (_e59 >= 1i)) && (_e62 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e52 = wired_advanced_fog_enabled_u0028_();
    if !(_e52) {
        return 0f;
    }
    let _e55 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e55, 0.000001f));
    let _e60 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e60 + 0.5f));
    let _e63 = fogType_1;
    if (_e63 == 1i) {
        let _e67 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e67 <= 0f) {
            return 0f;
        }
        let _e69 = viewDepth;
        let _e72 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e69 / _e72), 0f, 1f);
    }
    let _e77 = unnamed.advancedFogColorDensity[3u];
    let _e79 = viewDepth;
    opticalDepth = (max(_e77, 0f) * _e79);
    let _e81 = fogType_1;
    if (_e81 == 2i) {
        let _e83 = opticalDepth;
        return clamp((1f - exp(-(_e83))), 0f, 1f);
    }
    let _e88 = opticalDepth;
    let _e89 = opticalDepth;
    return clamp((1f - exp(-((_e88 * _e89)))), 0f, 1f);
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e53 = (*c);
    (*c) = max(_e53, vec3<f32>(0f, 0f, 0f));
    let _e55 = (*c);
    cutoff = (_e55 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e57 = (*c);
    lo = (_e57 / vec3(12.92f));
    let _e60 = (*c);
    hi = pow(((_e60 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e65 = hi;
    let _e66 = lo;
    let _e67 = cutoff;
    return mix(_e65, _e66, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e67));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param: vec3<f32>;

    let _e54 = (*role);
    let _e56 = (*role);
    let _e61 = unnamed.packed_indices[(_e54 / 4u)][(_e56 % 4u)];
    let _e64 = (*role);
    let _e66 = (*role);
    let _e71 = unnamed.packed_indices[(_e64 / 4u)][(_e66 % 4u)];
    let _e76 = (*uv);
    let _e77 = textureSample(wired_bindless_images[(_e61 & 4095u)], wired_bindless_samplers[((_e71 >> bitcast<u32>(12i)) & 255u)], _e76);
    c_1 = _e77;
    let _e78 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e78))) == 0i) {
        let _e83 = c_1;
        param = _e83.xyz;
        let _e85 = sRGBToLinear_u0028_vf3_u003b((&param));
        c_1[0u] = _e85.x;
        c_1[1u] = _e85.y;
        c_1[2u] = _e85.z;
    }
    let _e92 = (*slot);
    if (lightmap_slot == (_e92 + 1i)) {
        let _e97 = unnamed.worldLightParams[0u];
        let _e98 = c_1;
        let _e100 = (_e98.xyz * _e97);
        c_1[0u] = _e100.x;
        c_1[1u] = _e100.y;
        c_1[2u] = _e100.z;
    }
    let _e107 = c_1;
    return _e107;
}

fn main_1() {
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

    param_1 = 0u;
    let _e60 = frag_tex_coord0_1;
    param_2 = _e60;
    param_3 = 0i;
    let _e61 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_1), (&param_2), (&param_3));
    let _e63 = unnamed.ent_color0_;
    color0_ = (_e61 * _e63);
    let _e65 = color0_;
    base = _e65;
    if override_type_3_ {
        let _e67 = color0_[3u];
        if (_e67 == alpha_test_value) {
            discard;
        }
    } else {
        if override_type_3_1 {
            let _e70 = color0_[3u];
            if (_e70 >= alpha_test_value) {
                discard;
            }
        } else {
            if override_type_3_2 {
                let _e73 = color0_[3u];
                if (_e73 < alpha_test_value) {
                    discard;
                }
            }
        }
    }
    let _e75 = color0_;
    base = _e75;
    let _e76 = wired_advanced_fog_enabled_u0028_();
    if _e76 {
        let _e77 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e77;
        let _e78 = base;
        let _e81 = unnamed.advancedFogColorDensity;
        let _e83 = fogAmount;
        let _e85 = mix(_e78.xyz, _e81.xyz, vec3(_e83));
        base[0u] = _e85.x;
        base[1u] = _e85.y;
        base[2u] = _e85.z;
    }
    if override_type_3_3 {
        let _e93 = base[3u];
        if (_e93 == 0f) {
            discard;
        }
    } else {
        if override_type_3_4 {
            let _e95 = base;
            let _e97 = base;
            if (dot(_e95.xyz, _e97.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e101 = gl_FragCoord_1;
    let _e106 = unnamed.packed_indices[1i][0u];
    let _e112 = unnamed.packed_indices[1i][0u];
    let _e117 = textureDimensions(wired_bindless_images[(_e106 & 4095u)], 0i);
    screenUV = (_e101.xy / vec2<f32>(vec2<i32>(_e117)));
    let _e124 = unnamed.packed_indices[1i][0u];
    let _e130 = unnamed.packed_indices[1i][0u];
    let _e135 = screenUV;
    let _e136 = textureSample(wired_bindless_images[(_e124 & 4095u)], wired_bindless_samplers[((_e130 >> bitcast<u32>(12i)) & 255u)], _e135);
    sceneDepth = _e136.x;
    let _e139 = gl_FragCoord_1[2u];
    fragDepth = _e139;
    let _e140 = fragDepth;
    let _e141 = sceneDepth;
    depthDiff = (_e140 - _e141);
    let _e144 = depthDiff;
    fadeFactor = smoothstep(0f, (depth_fade_scale * 0.0005f), _e144);
    let _e146 = fadeFactor;
    let _e148 = base[3u];
    base[3u] = (_e148 * _e146);
    let _e151 = fadeFactor;
    let _e152 = base;
    let _e154 = (_e152.xyz * _e151);
    base[0u] = _e154.x;
    base[1u] = _e154.y;
    base[2u] = _e154.z;
    let _e161 = base;
    out_color = _e161;
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
