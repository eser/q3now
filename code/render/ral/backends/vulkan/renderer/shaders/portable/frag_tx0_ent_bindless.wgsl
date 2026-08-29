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

    let _e60 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e60 + 0.5f));
    let _e65 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e67 = fogType;
    let _e70 = fogType;
    return (((_e65 > 0.5f) && (_e67 >= 1i)) && (_e70 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e60 = wired_advanced_fog_enabled_u0028_();
    if !(_e60) {
        return 0f;
    }
    let _e63 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e63, 0.000001f));
    let _e68 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e68 + 0.5f));
    let _e71 = fogType_1;
    if (_e71 == 1i) {
        let _e75 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e75 <= 0f) {
            return 0f;
        }
        let _e77 = viewDepth;
        let _e80 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e77 / _e80), 0f, 1f);
    }
    let _e85 = unnamed.advancedFogColorDensity[3u];
    let _e87 = viewDepth;
    opticalDepth = (max(_e85, 0f) * _e87);
    let _e89 = fogType_1;
    if (_e89 == 2i) {
        let _e91 = opticalDepth;
        return clamp((1f - exp(-(_e91))), 0f, 1f);
    }
    let _e96 = opticalDepth;
    let _e97 = opticalDepth;
    return clamp((1f - exp(-((_e96 * _e97)))), 0f, 1f);
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e61 = (*c);
    (*c) = max(_e61, vec3<f32>(0f, 0f, 0f));
    let _e63 = (*c);
    cutoff = (_e63 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e65 = (*c);
    lo = (_e65 / vec3(12.92f));
    let _e68 = (*c);
    hi = pow(((_e68 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e73 = hi;
    let _e74 = lo;
    let _e75 = cutoff;
    return mix(_e73, _e74, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e75));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param: vec3<f32>;

    let _e62 = (*role);
    let _e64 = (*role);
    let _e69 = unnamed.packed_indices[(_e62 / 4u)][(_e64 % 4u)];
    let _e72 = (*role);
    let _e74 = (*role);
    let _e79 = unnamed.packed_indices[(_e72 / 4u)][(_e74 % 4u)];
    let _e84 = (*uv);
    let _e85 = textureSample(wired_bindless_images[(_e69 & 4095u)], wired_bindless_samplers[((_e79 >> bitcast<u32>(12i)) & 255u)], _e84);
    c_1 = _e85;
    let _e86 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e86))) == 0i) {
        let _e91 = c_1;
        param = _e91.xyz;
        let _e93 = sRGBToLinear_u0028_vf3_u003b((&param));
        c_1[0u] = _e93.x;
        c_1[1u] = _e93.y;
        c_1[2u] = _e93.z;
    }
    let _e100 = (*slot);
    if (lightmap_slot == (_e100 + 1i)) {
        let _e105 = unnamed.worldLightParams[0u];
        let _e106 = c_1;
        let _e108 = (_e106.xyz * _e105);
        c_1[0u] = _e108.x;
        c_1[1u] = _e108.y;
        c_1[2u] = _e108.z;
    }
    let _e115 = c_1;
    return _e115;
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
    let _e66 = frag_tex_coord0_1;
    param_2 = _e66;
    param_3 = 0i;
    let _e67 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_1), (&param_2), (&param_3));
    let _e69 = unnamed.ent_color0_;
    color0_ = (_e67 * _e69);
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
    if override_type_3_3 {
        let _e84 = unnamed.worldLightParams[1u];
        wetness = clamp(_e84, 0f, 1f);
        let _e88 = unnamed.worldLightParams[2u];
        frost = clamp(_e88, 0f, 1f);
        let _e90 = base;
        luminance = dot(_e90.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e93 = wetness;
        let _e95 = base;
        let _e97 = (_e95.xyz * mix(1f, 0.82f, _e93));
        base[0u] = _e97.x;
        base[1u] = _e97.y;
        base[2u] = _e97.z;
        let _e104 = base;
        let _e106 = luminance;
        let _e108 = luminance;
        let _e110 = luminance;
        let _e112 = frost;
        let _e115 = mix(_e104.xyz, vec3<f32>((_e106 * 0.88f), (_e108 * 0.94f), _e110), vec3((_e112 * 0.55f)));
        base[0u] = _e115.x;
        base[1u] = _e115.y;
        base[2u] = _e115.z;
    }
    let _e122 = color0_;
    let _e125 = unnamed.emissionRadiance;
    let _e128 = base;
    let _e130 = (_e128.xyz + (_e122.xyz * _e125.xyz));
    base[0u] = _e130.x;
    base[1u] = _e130.y;
    base[2u] = _e130.z;
    let _e137 = wired_advanced_fog_enabled_u0028_();
    if _e137 {
        let _e138 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e138;
        let _e139 = base;
        let _e142 = unnamed.advancedFogColorDensity;
        let _e144 = fogAmount;
        let _e146 = mix(_e139.xyz, _e142.xyz, vec3(_e144));
        base[0u] = _e146.x;
        base[1u] = _e146.y;
        base[2u] = _e146.z;
    }
    if override_type_3_4 {
        let _e154 = base[3u];
        if (_e154 == 0f) {
            discard;
        }
    } else {
        if override_type_3_5 {
            let _e156 = base;
            let _e158 = base;
            if (dot(_e156.xyz, _e158.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e162 = base;
    out_color = _e162;
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
