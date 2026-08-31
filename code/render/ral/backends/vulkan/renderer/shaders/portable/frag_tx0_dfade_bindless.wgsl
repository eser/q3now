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

    let _e63 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e63 + 0.5f));
    let _e68 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e70 = fogType;
    let _e73 = fogType;
    return (((_e68 > 0.5f) && (_e70 >= 1i)) && (_e73 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e63 = wired_advanced_fog_enabled_u0028_();
    if !(_e63) {
        return 0f;
    }
    let _e66 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e66, 0.000001f));
    let _e71 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e71 + 0.5f));
    let _e74 = fogType_1;
    if (_e74 == 1i) {
        let _e78 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e78 <= 0f) {
            return 0f;
        }
        let _e80 = viewDepth;
        let _e83 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e80 / _e83), 0f, 1f);
    }
    let _e88 = unnamed.advancedFogColorDensity[3u];
    let _e90 = viewDepth;
    opticalDepth = (max(_e88, 0f) * _e90);
    let _e92 = fogType_1;
    if (_e92 == 2i) {
        let _e94 = opticalDepth;
        return clamp((1f - exp(-(_e94))), 0f, 1f);
    }
    let _e99 = opticalDepth;
    let _e100 = opticalDepth;
    return clamp((1f - exp(-((_e99 * _e100)))), 0f, 1f);
}

fn wired_boost_legacy_lightmap_u0028_vf3_u003b(rgb: ptr<function, vec3<f32>>) -> vec3<f32> {
    var boosted: vec3<f32>;
    var peak: f32;

    let _e63 = (*rgb);
    let _e66 = unnamed.worldLightParams[0u];
    boosted = (_e63 * _e66);
    let _e69 = boosted[0u];
    let _e71 = boosted[1u];
    let _e73 = boosted[2u];
    peak = max(_e69, max(_e71, _e73));
    let _e76 = peak;
    if (_e76 > 1f) {
        let _e78 = peak;
        let _e79 = boosted;
        boosted = (_e79 / vec3(_e78));
    }
    let _e82 = boosted;
    return _e82;
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e64 = (*c);
    (*c) = max(_e64, vec3<f32>(0f, 0f, 0f));
    let _e66 = (*c);
    cutoff = (_e66 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e68 = (*c);
    lo = (_e68 / vec3(12.92f));
    let _e71 = (*c);
    hi = pow(((_e71 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e76 = hi;
    let _e77 = lo;
    let _e78 = cutoff;
    return mix(_e76, _e77, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e78));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param: vec3<f32>;
    var param_1: vec3<f32>;

    let _e66 = (*role);
    let _e68 = (*role);
    let _e73 = unnamed.packed_indices[(_e66 / 4u)][(_e68 % 4u)];
    let _e76 = (*role);
    let _e78 = (*role);
    let _e83 = unnamed.packed_indices[(_e76 / 4u)][(_e78 % 4u)];
    let _e88 = (*uv);
    let _e89 = textureSample(wired_bindless_images[(_e73 & 4095u)], wired_bindless_samplers[((_e83 >> bitcast<u32>(12i)) & 255u)], _e88);
    c_1 = _e89;
    let _e90 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e90))) == 0i) {
        let _e95 = c_1;
        param = _e95.xyz;
        let _e97 = sRGBToLinear_u0028_vf3_u003b((&param));
        c_1[0u] = _e97.x;
        c_1[1u] = _e97.y;
        c_1[2u] = _e97.z;
    }
    let _e104 = (*slot);
    if (lightmap_slot == (_e104 + 1i)) {
        let _e107 = c_1;
        param_1 = _e107.xyz;
        let _e109 = wired_boost_legacy_lightmap_u0028_vf3_u003b((&param_1));
        c_1[0u] = _e109.x;
        c_1[1u] = _e109.y;
        c_1[2u] = _e109.z;
    }
    let _e116 = c_1;
    return _e116;
}

fn main_1() {
    var frag_color0_: vec4<f32>;
    var param_2: vec3<f32>;
    var color0_: vec4<f32>;
    var param_3: u32;
    var param_4: vec2<f32>;
    var param_5: i32;
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

    let _e76 = frag_color0In_1;
    param_2 = _e76.xyz;
    let _e78 = sRGBToLinear_u0028_vf3_u003b((&param_2));
    let _e80 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e78.x, _e78.y, _e78.z, _e80);
    param_3 = 0u;
    let _e85 = frag_tex_coord0_1;
    param_4 = _e85;
    param_5 = 0i;
    let _e86 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_3), (&param_4), (&param_5));
    let _e87 = frag_color0_;
    color0_ = (_e86 * _e87);
    let _e89 = color0_;
    base = _e89;
    if override_type_3_ {
        let _e91 = color0_[3u];
        if (_e91 == alpha_test_value) {
            discard;
        }
    } else {
        if override_type_3_1 {
            let _e94 = color0_[3u];
            if (_e94 >= alpha_test_value) {
                discard;
            }
        } else {
            if override_type_3_2 {
                let _e97 = color0_[3u];
                if (_e97 < alpha_test_value) {
                    discard;
                }
            }
        }
    }
    let _e99 = color0_;
    base = _e99;
    if override_type_3_3 {
        let _e102 = unnamed.worldLightParams[1u];
        wetness = clamp(_e102, 0f, 1f);
        let _e106 = unnamed.worldLightParams[2u];
        frost = clamp(_e106, 0f, 1f);
        let _e108 = base;
        luminance = dot(_e108.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e111 = wetness;
        let _e113 = base;
        let _e115 = (_e113.xyz * mix(1f, 0.82f, _e111));
        base[0u] = _e115.x;
        base[1u] = _e115.y;
        base[2u] = _e115.z;
        let _e122 = base;
        let _e124 = luminance;
        let _e126 = luminance;
        let _e128 = luminance;
        let _e130 = frost;
        let _e133 = mix(_e122.xyz, vec3<f32>((_e124 * 0.88f), (_e126 * 0.94f), _e128), vec3((_e130 * 0.55f)));
        base[0u] = _e133.x;
        base[1u] = _e133.y;
        base[2u] = _e133.z;
    }
    let _e140 = color0_;
    let _e143 = unnamed.emissionRadiance;
    let _e146 = base;
    let _e148 = (_e146.xyz + (_e140.xyz * _e143.xyz));
    base[0u] = _e148.x;
    base[1u] = _e148.y;
    base[2u] = _e148.z;
    let _e155 = wired_advanced_fog_enabled_u0028_();
    if _e155 {
        let _e156 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e156;
        let _e157 = base;
        let _e160 = unnamed.advancedFogColorDensity;
        let _e162 = fogAmount;
        let _e164 = mix(_e157.xyz, _e160.xyz, vec3(_e162));
        base[0u] = _e164.x;
        base[1u] = _e164.y;
        base[2u] = _e164.z;
    }
    if override_type_3_4 {
        let _e172 = base[3u];
        if (_e172 == 0f) {
            discard;
        }
    } else {
        if override_type_3_5 {
            let _e174 = base;
            let _e176 = base;
            if (dot(_e174.xyz, _e176.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e180 = gl_FragCoord_1;
    let _e185 = unnamed.packed_indices[1i][0u];
    let _e191 = unnamed.packed_indices[1i][0u];
    let _e196 = textureDimensions(wired_bindless_images[(_e185 & 4095u)], 0i);
    screenUV = (_e180.xy / vec2<f32>(vec2<i32>(_e196)));
    let _e203 = unnamed.packed_indices[1i][0u];
    let _e209 = unnamed.packed_indices[1i][0u];
    let _e214 = screenUV;
    let _e215 = textureSample(wired_bindless_images[(_e203 & 4095u)], wired_bindless_samplers[((_e209 >> bitcast<u32>(12i)) & 255u)], _e214);
    sceneDepth = _e215.x;
    let _e218 = gl_FragCoord_1[2u];
    fragDepth = _e218;
    let _e219 = fragDepth;
    let _e220 = sceneDepth;
    depthDiff = (_e219 - _e220);
    let _e223 = depthDiff;
    fadeFactor = smoothstep(0f, (depth_fade_scale * 0.0005f), _e223);
    let _e225 = fadeFactor;
    let _e227 = base[3u];
    base[3u] = (_e227 * _e225);
    let _e230 = fadeFactor;
    let _e231 = base;
    let _e233 = (_e231.xyz * _e230);
    base[0u] = _e233.x;
    base[1u] = _e233.y;
    base[2u] = _e233.z;
    let _e240 = base;
    out_color = _e240;
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
