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
@id(8) override identity_color: f32 = 1f;
@id(9) override identity_alpha: f32 = 1f;
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

    let _e64 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e64 + 0.5f));
    let _e69 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e71 = fogType;
    let _e74 = fogType;
    return (((_e69 > 0.5f) && (_e71 >= 1i)) && (_e74 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e64 = wired_advanced_fog_enabled_u0028_();
    if !(_e64) {
        return 0f;
    }
    let _e67 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e67, 0.000001f));
    let _e72 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e72 + 0.5f));
    let _e75 = fogType_1;
    if (_e75 == 1i) {
        let _e79 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e79 <= 0f) {
            return 0f;
        }
        let _e81 = viewDepth;
        let _e84 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e81 / _e84), 0f, 1f);
    }
    let _e89 = unnamed.advancedFogColorDensity[3u];
    let _e91 = viewDepth;
    opticalDepth = (max(_e89, 0f) * _e91);
    let _e93 = fogType_1;
    if (_e93 == 2i) {
        let _e95 = opticalDepth;
        return clamp((1f - exp(-(_e95))), 0f, 1f);
    }
    let _e100 = opticalDepth;
    let _e101 = opticalDepth;
    return clamp((1f - exp(-((_e100 * _e101)))), 0f, 1f);
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e65 = (*c);
    (*c) = max(_e65, vec3<f32>(0f, 0f, 0f));
    let _e67 = (*c);
    cutoff = (_e67 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e69 = (*c);
    lo = (_e69 / vec3(12.92f));
    let _e72 = (*c);
    hi = pow(((_e72 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e77 = hi;
    let _e78 = lo;
    let _e79 = cutoff;
    return mix(_e77, _e78, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e79));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param: vec3<f32>;

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
        let _e109 = unnamed.worldLightParams[0u];
        let _e110 = c_1;
        let _e112 = (_e110.xyz * _e109);
        c_1[0u] = _e112.x;
        c_1[1u] = _e112.y;
        c_1[2u] = _e112.z;
    }
    let _e119 = c_1;
    return _e119;
}

fn main_1() {
    var frag_color: vec4<f32>;
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

    frag_color[0u] = identity_color;
    frag_color[1u] = identity_color;
    frag_color[2u] = identity_color;
    frag_color[3u] = identity_alpha;
    param_1 = 0u;
    let _e80 = frag_tex_coord0_1;
    param_2 = _e80;
    param_3 = 0i;
    let _e81 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_1), (&param_2), (&param_3));
    let _e82 = frag_color;
    color0_ = (_e81 * _e82);
    let _e84 = color0_;
    base = _e84;
    if override_type_3_ {
        let _e86 = color0_[3u];
        if (_e86 == alpha_test_value) {
            discard;
        }
    } else {
        if override_type_3_1 {
            let _e89 = color0_[3u];
            if (_e89 >= alpha_test_value) {
                discard;
            }
        } else {
            if override_type_3_2 {
                let _e92 = color0_[3u];
                if (_e92 < alpha_test_value) {
                    discard;
                }
            }
        }
    }
    let _e94 = color0_;
    base = _e94;
    if override_type_3_3 {
        let _e97 = unnamed.worldLightParams[1u];
        wetness = clamp(_e97, 0f, 1f);
        let _e101 = unnamed.worldLightParams[2u];
        frost = clamp(_e101, 0f, 1f);
        let _e103 = base;
        luminance = dot(_e103.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e106 = wetness;
        let _e108 = base;
        let _e110 = (_e108.xyz * mix(1f, 0.82f, _e106));
        base[0u] = _e110.x;
        base[1u] = _e110.y;
        base[2u] = _e110.z;
        let _e117 = base;
        let _e119 = luminance;
        let _e121 = luminance;
        let _e123 = luminance;
        let _e125 = frost;
        let _e128 = mix(_e117.xyz, vec3<f32>((_e119 * 0.88f), (_e121 * 0.94f), _e123), vec3((_e125 * 0.55f)));
        base[0u] = _e128.x;
        base[1u] = _e128.y;
        base[2u] = _e128.z;
    }
    let _e135 = color0_;
    let _e138 = unnamed.emissionRadiance;
    let _e141 = base;
    let _e143 = (_e141.xyz + (_e135.xyz * _e138.xyz));
    base[0u] = _e143.x;
    base[1u] = _e143.y;
    base[2u] = _e143.z;
    let _e150 = wired_advanced_fog_enabled_u0028_();
    if _e150 {
        let _e151 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e151;
        let _e152 = base;
        let _e155 = unnamed.advancedFogColorDensity;
        let _e157 = fogAmount;
        let _e159 = mix(_e152.xyz, _e155.xyz, vec3(_e157));
        base[0u] = _e159.x;
        base[1u] = _e159.y;
        base[2u] = _e159.z;
    }
    if override_type_3_4 {
        let _e167 = base[3u];
        if (_e167 == 0f) {
            discard;
        }
    } else {
        if override_type_3_5 {
            let _e169 = base;
            let _e171 = base;
            if (dot(_e169.xyz, _e171.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e175 = gl_FragCoord_1;
    let _e180 = unnamed.packed_indices[1i][0u];
    let _e186 = unnamed.packed_indices[1i][0u];
    let _e191 = textureDimensions(wired_bindless_images[(_e180 & 4095u)], 0i);
    screenUV = (_e175.xy / vec2<f32>(vec2<i32>(_e191)));
    let _e198 = unnamed.packed_indices[1i][0u];
    let _e204 = unnamed.packed_indices[1i][0u];
    let _e209 = screenUV;
    let _e210 = textureSample(wired_bindless_images[(_e198 & 4095u)], wired_bindless_samplers[((_e204 >> bitcast<u32>(12i)) & 255u)], _e209);
    sceneDepth = _e210.x;
    let _e213 = gl_FragCoord_1[2u];
    fragDepth = _e213;
    let _e214 = fragDepth;
    let _e215 = sceneDepth;
    depthDiff = (_e214 - _e215);
    let _e218 = depthDiff;
    fadeFactor = smoothstep(0f, (depth_fade_scale * 0.0005f), _e218);
    let _e220 = fadeFactor;
    let _e222 = base[3u];
    base[3u] = (_e222 * _e220);
    let _e225 = fadeFactor;
    let _e226 = base;
    let _e228 = (_e226.xyz * _e225);
    base[0u] = _e228.x;
    base[1u] = _e228.y;
    base[2u] = _e228.z;
    let _e235 = base;
    out_color = _e235;
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
