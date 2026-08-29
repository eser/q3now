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

    let _e62 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e62 + 0.5f));
    let _e67 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e69 = fogType;
    let _e72 = fogType;
    return (((_e67 > 0.5f) && (_e69 >= 1i)) && (_e72 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e62 = wired_advanced_fog_enabled_u0028_();
    if !(_e62) {
        return 0f;
    }
    let _e65 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e65, 0.000001f));
    let _e70 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e70 + 0.5f));
    let _e73 = fogType_1;
    if (_e73 == 1i) {
        let _e77 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e77 <= 0f) {
            return 0f;
        }
        let _e79 = viewDepth;
        let _e82 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e79 / _e82), 0f, 1f);
    }
    let _e87 = unnamed.advancedFogColorDensity[3u];
    let _e89 = viewDepth;
    opticalDepth = (max(_e87, 0f) * _e89);
    let _e91 = fogType_1;
    if (_e91 == 2i) {
        let _e93 = opticalDepth;
        return clamp((1f - exp(-(_e93))), 0f, 1f);
    }
    let _e98 = opticalDepth;
    let _e99 = opticalDepth;
    return clamp((1f - exp(-((_e98 * _e99)))), 0f, 1f);
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e63 = (*c);
    (*c) = max(_e63, vec3<f32>(0f, 0f, 0f));
    let _e65 = (*c);
    cutoff = (_e65 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e67 = (*c);
    lo = (_e67 / vec3(12.92f));
    let _e70 = (*c);
    hi = pow(((_e70 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e75 = hi;
    let _e76 = lo;
    let _e77 = cutoff;
    return mix(_e75, _e76, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e77));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param: vec3<f32>;

    let _e64 = (*role);
    let _e66 = (*role);
    let _e71 = unnamed.packed_indices[(_e64 / 4u)][(_e66 % 4u)];
    let _e74 = (*role);
    let _e76 = (*role);
    let _e81 = unnamed.packed_indices[(_e74 / 4u)][(_e76 % 4u)];
    let _e86 = (*uv);
    let _e87 = textureSample(wired_bindless_images[(_e71 & 4095u)], wired_bindless_samplers[((_e81 >> bitcast<u32>(12i)) & 255u)], _e86);
    c_1 = _e87;
    let _e88 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e88))) == 0i) {
        let _e93 = c_1;
        param = _e93.xyz;
        let _e95 = sRGBToLinear_u0028_vf3_u003b((&param));
        c_1[0u] = _e95.x;
        c_1[1u] = _e95.y;
        c_1[2u] = _e95.z;
    }
    let _e102 = (*slot);
    if (lightmap_slot == (_e102 + 1i)) {
        let _e107 = unnamed.worldLightParams[0u];
        let _e108 = c_1;
        let _e110 = (_e108.xyz * _e107);
        c_1[0u] = _e110.x;
        c_1[1u] = _e110.y;
        c_1[2u] = _e110.z;
    }
    let _e117 = c_1;
    return _e117;
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
    var screenUV: vec2<f32>;
    var sceneDepth: f32;
    var fragDepth: f32;
    var depthDiff: f32;
    var fadeFactor: f32;

    param_1 = 0u;
    let _e73 = frag_tex_coord0_1;
    param_2 = _e73;
    param_3 = 0i;
    let _e74 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_1), (&param_2), (&param_3));
    let _e76 = unnamed.ent_color0_;
    color0_ = (_e74 * _e76);
    let _e78 = color0_;
    base = _e78;
    if override_type_3_ {
        let _e80 = color0_[3u];
        if (_e80 == alpha_test_value) {
            discard;
        }
    } else {
        if override_type_3_1 {
            let _e83 = color0_[3u];
            if (_e83 >= alpha_test_value) {
                discard;
            }
        } else {
            if override_type_3_2 {
                let _e86 = color0_[3u];
                if (_e86 < alpha_test_value) {
                    discard;
                }
            }
        }
    }
    let _e88 = color0_;
    base = _e88;
    if override_type_3_3 {
        let _e91 = unnamed.worldLightParams[1u];
        wetness = clamp(_e91, 0f, 1f);
        let _e95 = unnamed.worldLightParams[2u];
        frost = clamp(_e95, 0f, 1f);
        let _e97 = base;
        luminance = dot(_e97.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e100 = wetness;
        let _e102 = base;
        let _e104 = (_e102.xyz * mix(1f, 0.82f, _e100));
        base[0u] = _e104.x;
        base[1u] = _e104.y;
        base[2u] = _e104.z;
        let _e111 = base;
        let _e113 = luminance;
        let _e115 = luminance;
        let _e117 = luminance;
        let _e119 = frost;
        let _e122 = mix(_e111.xyz, vec3<f32>((_e113 * 0.88f), (_e115 * 0.94f), _e117), vec3((_e119 * 0.55f)));
        base[0u] = _e122.x;
        base[1u] = _e122.y;
        base[2u] = _e122.z;
    }
    let _e129 = color0_;
    let _e132 = unnamed.emissionRadiance;
    let _e135 = base;
    let _e137 = (_e135.xyz + (_e129.xyz * _e132.xyz));
    base[0u] = _e137.x;
    base[1u] = _e137.y;
    base[2u] = _e137.z;
    let _e144 = wired_advanced_fog_enabled_u0028_();
    if _e144 {
        let _e145 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e145;
        let _e146 = base;
        let _e149 = unnamed.advancedFogColorDensity;
        let _e151 = fogAmount;
        let _e153 = mix(_e146.xyz, _e149.xyz, vec3(_e151));
        base[0u] = _e153.x;
        base[1u] = _e153.y;
        base[2u] = _e153.z;
    }
    if override_type_3_4 {
        let _e161 = base[3u];
        if (_e161 == 0f) {
            discard;
        }
    } else {
        if override_type_3_5 {
            let _e163 = base;
            let _e165 = base;
            if (dot(_e163.xyz, _e165.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e169 = gl_FragCoord_1;
    let _e174 = unnamed.packed_indices[1i][0u];
    let _e180 = unnamed.packed_indices[1i][0u];
    let _e185 = textureDimensions(wired_bindless_images[(_e174 & 4095u)], 0i);
    screenUV = (_e169.xy / vec2<f32>(vec2<i32>(_e185)));
    let _e192 = unnamed.packed_indices[1i][0u];
    let _e198 = unnamed.packed_indices[1i][0u];
    let _e203 = screenUV;
    let _e204 = textureSample(wired_bindless_images[(_e192 & 4095u)], wired_bindless_samplers[((_e198 >> bitcast<u32>(12i)) & 255u)], _e203);
    sceneDepth = _e204.x;
    let _e207 = gl_FragCoord_1[2u];
    fragDepth = _e207;
    let _e208 = fragDepth;
    let _e209 = sceneDepth;
    depthDiff = (_e208 - _e209);
    let _e212 = depthDiff;
    fadeFactor = smoothstep(0f, (depth_fade_scale * 0.0005f), _e212);
    let _e214 = fadeFactor;
    let _e216 = base[3u];
    base[3u] = (_e216 * _e214);
    let _e219 = fadeFactor;
    let _e220 = base;
    let _e222 = (_e220.xyz * _e219);
    base[0u] = _e222.x;
    base[1u] = _e222.y;
    base[2u] = _e222.z;
    let _e229 = base;
    out_color = _e229;
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
