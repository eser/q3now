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

@id(4) override tex_domain: i32 = 0i;
@id(26) override lightmap_slot: i32 = 0i;
@id(0) override alpha_test_func: i32 = 0i;
override override_type_3_: bool = (alpha_test_func == 1i);
@id(1) override alpha_test_value: f32 = 0f;
override override_type_3_1: bool = (alpha_test_func == 2i);
override override_type_3_2: bool = (alpha_test_func == 3i);
override override_type_3_3: bool = (lightmap_slot != 0i);
@id(7) override discard_mode: i32 = 0i;
override override_type_3_4: bool = (discard_mode == 1i);
override override_type_3_5: bool = (discard_mode == 2i);
@id(2) override depth_fragment: f32 = 0.85f;
@id(3) override alpha_to_coverage: i32 = 0i;

@group(0) @binding(0)
var<uniform> unnamed: UBO;
var<private> gl_FragCoord_1: vec4<f32>;
@group(1) @binding(0)
var wired_bindless_images: binding_array<texture_2d<f32>>;
@group(1) @binding(1)
var wired_bindless_samplers: binding_array<sampler>;
var<private> frag_tex_coord0_1: vec2<f32>;
var<private> gl_FragDepth: f32 = 0f;

fn wired_advanced_fog_enabled_u0028_() -> bool {
    var fogType: i32;

    let _e61 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e61 + 0.5f));
    let _e66 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e68 = fogType;
    let _e71 = fogType;
    return (((_e66 > 0.5f) && (_e68 >= 1i)) && (_e71 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e61 = wired_advanced_fog_enabled_u0028_();
    if !(_e61) {
        return 0f;
    }
    let _e64 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e64, 0.000001f));
    let _e69 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e69 + 0.5f));
    let _e72 = fogType_1;
    if (_e72 == 1i) {
        let _e76 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e76 <= 0f) {
            return 0f;
        }
        let _e78 = viewDepth;
        let _e81 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e78 / _e81), 0f, 1f);
    }
    let _e86 = unnamed.advancedFogColorDensity[3u];
    let _e88 = viewDepth;
    opticalDepth = (max(_e86, 0f) * _e88);
    let _e90 = fogType_1;
    if (_e90 == 2i) {
        let _e92 = opticalDepth;
        return clamp((1f - exp(-(_e92))), 0f, 1f);
    }
    let _e97 = opticalDepth;
    let _e98 = opticalDepth;
    return clamp((1f - exp(-((_e97 * _e98)))), 0f, 1f);
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e62 = (*c);
    (*c) = max(_e62, vec3<f32>(0f, 0f, 0f));
    let _e64 = (*c);
    cutoff = (_e64 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e66 = (*c);
    lo = (_e66 / vec3(12.92f));
    let _e69 = (*c);
    hi = pow(((_e69 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e74 = hi;
    let _e75 = lo;
    let _e76 = cutoff;
    return mix(_e74, _e75, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e76));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param: vec3<f32>;

    let _e63 = (*role);
    let _e65 = (*role);
    let _e70 = unnamed.packed_indices[(_e63 / 4u)][(_e65 % 4u)];
    let _e73 = (*role);
    let _e75 = (*role);
    let _e80 = unnamed.packed_indices[(_e73 / 4u)][(_e75 % 4u)];
    let _e85 = (*uv);
    let _e86 = textureSample(wired_bindless_images[(_e70 & 4095u)], wired_bindless_samplers[((_e80 >> bitcast<u32>(12i)) & 255u)], _e85);
    c_1 = _e86;
    let _e87 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e87))) == 0i) {
        let _e92 = c_1;
        param = _e92.xyz;
        let _e94 = sRGBToLinear_u0028_vf3_u003b((&param));
        c_1[0u] = _e94.x;
        c_1[1u] = _e94.y;
        c_1[2u] = _e94.z;
    }
    let _e101 = (*slot);
    if (lightmap_slot == (_e101 + 1i)) {
        let _e106 = unnamed.worldLightParams[0u];
        let _e107 = c_1;
        let _e109 = (_e107.xyz * _e106);
        c_1[0u] = _e109.x;
        c_1[1u] = _e109.y;
        c_1[2u] = _e109.z;
    }
    let _e116 = c_1;
    return _e116;
}

fn main_1() {
    var color0_: vec4<f32>;
    var param_1: u32;
    var param_2: vec2<f32>;
    var param_3: i32;
    var base: vec4<f32>;
    var wetness: f32;
    var frost: f32;
    var luminance: f32;
    var fogAmount: f32;

    param_1 = 0u;
    let _e67 = frag_tex_coord0_1;
    param_2 = _e67;
    param_3 = 0i;
    let _e68 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_1), (&param_2), (&param_3));
    color0_ = _e68;
    let _e69 = color0_;
    base = _e69;
    if override_type_3_ {
        let _e71 = color0_[3u];
        if (_e71 == alpha_test_value) {
            discard;
        }
    } else {
        if override_type_3_1 {
            let _e74 = color0_[3u];
            if (_e74 >= alpha_test_value) {
                discard;
            }
        } else {
            if override_type_3_2 {
                let _e77 = color0_[3u];
                if (_e77 < alpha_test_value) {
                    discard;
                }
            }
        }
    }
    let _e79 = color0_;
    base = _e79;
    if override_type_3_3 {
        let _e82 = unnamed.worldLightParams[1u];
        wetness = clamp(_e82, 0f, 1f);
        let _e86 = unnamed.worldLightParams[2u];
        frost = clamp(_e86, 0f, 1f);
        let _e88 = base;
        luminance = dot(_e88.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e91 = wetness;
        let _e93 = base;
        let _e95 = (_e93.xyz * mix(1f, 0.82f, _e91));
        base[0u] = _e95.x;
        base[1u] = _e95.y;
        base[2u] = _e95.z;
        let _e102 = base;
        let _e104 = luminance;
        let _e106 = luminance;
        let _e108 = luminance;
        let _e110 = frost;
        let _e113 = mix(_e102.xyz, vec3<f32>((_e104 * 0.88f), (_e106 * 0.94f), _e108), vec3((_e110 * 0.55f)));
        base[0u] = _e113.x;
        base[1u] = _e113.y;
        base[2u] = _e113.z;
    }
    let _e120 = color0_;
    let _e123 = unnamed.emissionRadiance;
    let _e126 = base;
    let _e128 = (_e126.xyz + (_e120.xyz * _e123.xyz));
    base[0u] = _e128.x;
    base[1u] = _e128.y;
    base[2u] = _e128.z;
    let _e135 = wired_advanced_fog_enabled_u0028_();
    if _e135 {
        let _e136 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e136;
        let _e137 = base;
        let _e140 = unnamed.advancedFogColorDensity;
        let _e142 = fogAmount;
        let _e144 = mix(_e137.xyz, _e140.xyz, vec3(_e142));
        base[0u] = _e144.x;
        base[1u] = _e144.y;
        base[2u] = _e144.z;
    }
    if override_type_3_4 {
        let _e152 = base[3u];
        if (_e152 == 0f) {
            discard;
        }
    } else {
        if override_type_3_5 {
            let _e154 = base;
            let _e156 = base;
            if (dot(_e154.xyz, _e156.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e161 = base[3u];
    if (_e161 < depth_fragment) {
        discard;
    }
    let _e164 = gl_FragCoord_1[2u];
    gl_FragDepth = _e164;
    return;
}

@fragment
fn main(@builtin(position) gl_FragCoord: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>) -> @builtin(frag_depth) f32 {
    gl_FragCoord_1 = gl_FragCoord;
    frag_tex_coord0_1 = frag_tex_coord0_;
    main_1();
    let _e5 = gl_FragDepth;
    return _e5;
}
