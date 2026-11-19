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
var<private> frag_color0In_1: vec4<f32>;
var<private> frag_tex_coord0_1: vec2<f32>;
var<private> out_color: vec4<f32>;

fn wired_advanced_fog_enabled_u0028_() -> bool {
    var fogType: i32;

    let _e65 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e65 + 0.5f));
    let _e70 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e72 = fogType;
    let _e75 = fogType;
    return (((_e70 > 0.5f) && (_e72 >= 1i)) && (_e75 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e65 = wired_advanced_fog_enabled_u0028_();
    if !(_e65) {
        return 0f;
    }
    let _e68 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e68, 0.000001f));
    let _e73 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e73 + 0.5f));
    let _e76 = fogType_1;
    if (_e76 == 1i) {
        let _e80 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e80 <= 0f) {
            return 0f;
        }
        let _e82 = viewDepth;
        let _e85 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e82 / _e85), 0f, 1f);
    }
    let _e90 = unnamed.advancedFogColorDensity[3u];
    let _e92 = viewDepth;
    opticalDepth = (max(_e90, 0f) * _e92);
    let _e94 = fogType_1;
    if (_e94 == 2i) {
        let _e96 = opticalDepth;
        return clamp((1f - exp(-(_e96))), 0f, 1f);
    }
    let _e101 = opticalDepth;
    let _e102 = opticalDepth;
    return clamp((1f - exp(-((_e101 * _e102)))), 0f, 1f);
}

fn wired_boost_legacy_lightmap_u0028_vf3_u003b(rgb: ptr<function, vec3<f32>>) -> vec3<f32> {
    var boosted: vec3<f32>;
    var peak: f32;

    let _e65 = (*rgb);
    let _e68 = unnamed.worldLightParams[0u];
    boosted = (_e65 * _e68);
    let _e71 = boosted[0u];
    let _e73 = boosted[1u];
    let _e75 = boosted[2u];
    peak = max(_e71, max(_e73, _e75));
    let _e78 = peak;
    if (_e78 > 1f) {
        let _e80 = peak;
        let _e81 = boosted;
        boosted = (_e81 / vec3(_e80));
    }
    let _e84 = boosted;
    return _e84;
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e66 = (*c);
    (*c) = max(_e66, vec3<f32>(0f, 0f, 0f));
    let _e68 = (*c);
    cutoff = (_e68 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e70 = (*c);
    lo = (_e70 / vec3(12.92f));
    let _e73 = (*c);
    hi = pow(((_e73 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e78 = hi;
    let _e79 = lo;
    let _e80 = cutoff;
    return mix(_e78, _e79, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e80));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param: vec3<f32>;
    var param_1: vec3<f32>;

    let _e68 = (*role);
    let _e70 = (*role);
    let _e75 = unnamed.packed_indices[(_e68 / 4u)][(_e70 % 4u)];
    let _e78 = (*role);
    let _e80 = (*role);
    let _e85 = unnamed.packed_indices[(_e78 / 4u)][(_e80 % 4u)];
    let _e90 = (*uv);
    let _e91 = textureSample(wired_bindless_images[(_e75 & 4095u)], wired_bindless_samplers[((_e85 >> bitcast<u32>(12i)) & 255u)], _e90);
    c_1 = _e91;
    let _e92 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e92))) == 0i) {
        let _e97 = c_1;
        param = _e97.xyz;
        let _e99 = sRGBToLinear_u0028_vf3_u003b((&param));
        c_1[0u] = _e99.x;
        c_1[1u] = _e99.y;
        c_1[2u] = _e99.z;
    }
    let _e106 = (*slot);
    if (lightmap_slot == (_e106 + 1i)) {
        let _e109 = c_1;
        param_1 = _e109.xyz;
        let _e111 = wired_boost_legacy_lightmap_u0028_vf3_u003b((&param_1));
        c_1[0u] = _e111.x;
        c_1[1u] = _e111.y;
        c_1[2u] = _e111.z;
    }
    let _e118 = c_1;
    return _e118;
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
    var screenUV: vec2<f32>;
    var sceneDepth: f32;
    var fragDepth: f32;
    var depthDiff: f32;
    var fadeFactor: f32;

    let _e79 = frag_color0In_1;
    param_2 = _e79.xyz;
    let _e81 = sRGBToLinear_u0028_vf3_u003b((&param_2));
    let _e83 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e81.x, _e81.y, _e81.z, _e83);
    param_3 = 0u;
    let _e88 = frag_tex_coord0_1;
    param_4 = _e88;
    param_5 = 0i;
    let _e89 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_3), (&param_4), (&param_5));
    sampledColor0_ = _e89;
    let _e90 = sampledColor0_;
    let _e91 = frag_color0_;
    color0_ = (_e90 * _e91);
    let _e93 = color0_;
    base = _e93;
    if override_type_3_ {
        let _e95 = color0_[3u];
        if (_e95 == alpha_test_value) {
            discard;
        }
    } else {
        if override_type_3_1 {
            let _e98 = color0_[3u];
            if (_e98 >= alpha_test_value) {
                discard;
            }
        } else {
            if override_type_3_2 {
                let _e101 = color0_[3u];
                if (_e101 < alpha_test_value) {
                    discard;
                }
            }
        }
    }
    let _e103 = color0_;
    base = _e103;
    if override_type_3_3 {
        let _e106 = unnamed.worldLightParams[1u];
        wetness = clamp(_e106, 0f, 1f);
        let _e110 = unnamed.worldLightParams[2u];
        frost = clamp(_e110, 0f, 1f);
        let _e112 = base;
        luminance = dot(_e112.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e115 = wetness;
        let _e117 = base;
        let _e119 = (_e117.xyz * mix(1f, 0.82f, _e115));
        base[0u] = _e119.x;
        base[1u] = _e119.y;
        base[2u] = _e119.z;
        let _e126 = base;
        let _e128 = luminance;
        let _e130 = luminance;
        let _e132 = luminance;
        let _e134 = frost;
        let _e137 = mix(_e126.xyz, vec3<f32>((_e128 * 0.88f), (_e130 * 0.94f), _e132), vec3((_e134 * 0.55f)));
        base[0u] = _e137.x;
        base[1u] = _e137.y;
        base[2u] = _e137.z;
    }
    let _e146 = unnamed.materialFactors[0u];
    if (_e146 >= 0f) {
        let _e149 = unnamed.materialBaseColor;
        let _e150 = base;
        base = (_e150 * _e149);
    }
    let _e152 = color0_;
    let _e155 = unnamed.emissionRadiance;
    let _e158 = base;
    let _e160 = (_e158.xyz + (_e152.xyz * _e155.xyz));
    base[0u] = _e160.x;
    base[1u] = _e160.y;
    base[2u] = _e160.z;
    let _e167 = wired_advanced_fog_enabled_u0028_();
    if _e167 {
        let _e168 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e168;
        let _e169 = base;
        let _e172 = unnamed.advancedFogColorDensity;
        let _e174 = fogAmount;
        let _e176 = mix(_e169.xyz, _e172.xyz, vec3(_e174));
        base[0u] = _e176.x;
        base[1u] = _e176.y;
        base[2u] = _e176.z;
    }
    if override_type_3_4 {
        let _e184 = base[3u];
        if (_e184 == 0f) {
            discard;
        }
    } else {
        if override_type_3_5 {
            let _e186 = base;
            let _e188 = base;
            if (dot(_e186.xyz, _e188.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e192 = gl_FragCoord_1;
    let _e197 = unnamed.packed_indices[1i][0u];
    let _e203 = unnamed.packed_indices[1i][0u];
    let _e208 = textureDimensions(wired_bindless_images[(_e197 & 4095u)], 0i);
    screenUV = (_e192.xy / vec2<f32>(vec2<i32>(_e208)));
    let _e215 = unnamed.packed_indices[1i][0u];
    let _e221 = unnamed.packed_indices[1i][0u];
    let _e226 = screenUV;
    let _e227 = textureSample(wired_bindless_images[(_e215 & 4095u)], wired_bindless_samplers[((_e221 >> bitcast<u32>(12i)) & 255u)], _e226);
    sceneDepth = _e227.x;
    let _e230 = gl_FragCoord_1[2u];
    fragDepth = _e230;
    let _e231 = fragDepth;
    let _e232 = sceneDepth;
    depthDiff = (_e231 - _e232);
    let _e235 = depthDiff;
    fadeFactor = smoothstep(0f, (depth_fade_scale * 0.0005f), _e235);
    let _e237 = fadeFactor;
    let _e239 = base[3u];
    base[3u] = (_e239 * _e237);
    let _e242 = fadeFactor;
    let _e243 = base;
    let _e245 = (_e243.xyz * _e242);
    base[0u] = _e245.x;
    base[1u] = _e245.y;
    base[2u] = _e245.z;
    let _e252 = base;
    out_color = _e252;
    return;
}

@fragment
fn main(@builtin(position) gl_FragCoord: vec4<f32>, @location(0) frag_color0In: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>) -> @location(0) vec4<f32> {
    gl_FragCoord_1 = gl_FragCoord;
    frag_color0In_1 = frag_color0In;
    frag_tex_coord0_1 = frag_tex_coord0_;
    main_1();
    let _e7 = out_color;
    return _e7;
}
