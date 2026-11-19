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

fn wired_boost_legacy_lightmap_u0028_vf3_u003b(rgb: ptr<function, vec3<f32>>) -> vec3<f32> {
    var boosted: vec3<f32>;
    var peak: f32;

    let _e64 = (*rgb);
    let _e67 = unnamed.worldLightParams[0u];
    boosted = (_e64 * _e67);
    let _e70 = boosted[0u];
    let _e72 = boosted[1u];
    let _e74 = boosted[2u];
    peak = max(_e70, max(_e72, _e74));
    let _e77 = peak;
    if (_e77 > 1f) {
        let _e79 = peak;
        let _e80 = boosted;
        boosted = (_e80 / vec3(_e79));
    }
    let _e83 = boosted;
    return _e83;
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
    var param_1: vec3<f32>;

    let _e67 = (*role);
    let _e69 = (*role);
    let _e74 = unnamed.packed_indices[(_e67 / 4u)][(_e69 % 4u)];
    let _e77 = (*role);
    let _e79 = (*role);
    let _e84 = unnamed.packed_indices[(_e77 / 4u)][(_e79 % 4u)];
    let _e89 = (*uv);
    let _e90 = textureSample(wired_bindless_images[(_e74 & 4095u)], wired_bindless_samplers[((_e84 >> bitcast<u32>(12i)) & 255u)], _e89);
    c_1 = _e90;
    let _e91 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e91))) == 0i) {
        let _e96 = c_1;
        param = _e96.xyz;
        let _e98 = sRGBToLinear_u0028_vf3_u003b((&param));
        c_1[0u] = _e98.x;
        c_1[1u] = _e98.y;
        c_1[2u] = _e98.z;
    }
    let _e105 = (*slot);
    if (lightmap_slot == (_e105 + 1i)) {
        let _e108 = c_1;
        param_1 = _e108.xyz;
        let _e110 = wired_boost_legacy_lightmap_u0028_vf3_u003b((&param_1));
        c_1[0u] = _e110.x;
        c_1[1u] = _e110.y;
        c_1[2u] = _e110.z;
    }
    let _e117 = c_1;
    return _e117;
}

fn main_1() {
    var sampledColor0_: vec4<f32>;
    var param_2: u32;
    var param_3: vec2<f32>;
    var param_4: i32;
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

    param_2 = 0u;
    let _e76 = frag_tex_coord0_1;
    param_3 = _e76;
    param_4 = 0i;
    let _e77 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_2), (&param_3), (&param_4));
    sampledColor0_ = _e77;
    let _e78 = sampledColor0_;
    color0_ = _e78;
    let _e79 = color0_;
    base = _e79;
    if override_type_3_ {
        let _e81 = color0_[3u];
        if (_e81 == alpha_test_value) {
            discard;
        }
    } else {
        if override_type_3_1 {
            let _e84 = color0_[3u];
            if (_e84 >= alpha_test_value) {
                discard;
            }
        } else {
            if override_type_3_2 {
                let _e87 = color0_[3u];
                if (_e87 < alpha_test_value) {
                    discard;
                }
            }
        }
    }
    let _e89 = color0_;
    base = _e89;
    if override_type_3_3 {
        let _e92 = unnamed.worldLightParams[1u];
        wetness = clamp(_e92, 0f, 1f);
        let _e96 = unnamed.worldLightParams[2u];
        frost = clamp(_e96, 0f, 1f);
        let _e98 = base;
        luminance = dot(_e98.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e101 = wetness;
        let _e103 = base;
        let _e105 = (_e103.xyz * mix(1f, 0.82f, _e101));
        base[0u] = _e105.x;
        base[1u] = _e105.y;
        base[2u] = _e105.z;
        let _e112 = base;
        let _e114 = luminance;
        let _e116 = luminance;
        let _e118 = luminance;
        let _e120 = frost;
        let _e123 = mix(_e112.xyz, vec3<f32>((_e114 * 0.88f), (_e116 * 0.94f), _e118), vec3((_e120 * 0.55f)));
        base[0u] = _e123.x;
        base[1u] = _e123.y;
        base[2u] = _e123.z;
    }
    let _e132 = unnamed.materialFactors[0u];
    if (_e132 >= 0f) {
        let _e135 = unnamed.materialBaseColor;
        let _e136 = base;
        base = (_e136 * _e135);
    }
    let _e138 = color0_;
    let _e141 = unnamed.emissionRadiance;
    let _e144 = base;
    let _e146 = (_e144.xyz + (_e138.xyz * _e141.xyz));
    base[0u] = _e146.x;
    base[1u] = _e146.y;
    base[2u] = _e146.z;
    let _e153 = wired_advanced_fog_enabled_u0028_();
    if _e153 {
        let _e154 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e154;
        let _e155 = base;
        let _e158 = unnamed.advancedFogColorDensity;
        let _e160 = fogAmount;
        let _e162 = mix(_e155.xyz, _e158.xyz, vec3(_e160));
        base[0u] = _e162.x;
        base[1u] = _e162.y;
        base[2u] = _e162.z;
    }
    if override_type_3_4 {
        let _e170 = base[3u];
        if (_e170 == 0f) {
            discard;
        }
    } else {
        if override_type_3_5 {
            let _e172 = base;
            let _e174 = base;
            if (dot(_e172.xyz, _e174.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e178 = gl_FragCoord_1;
    let _e183 = unnamed.packed_indices[1i][0u];
    let _e189 = unnamed.packed_indices[1i][0u];
    let _e194 = textureDimensions(wired_bindless_images[(_e183 & 4095u)], 0i);
    screenUV = (_e178.xy / vec2<f32>(vec2<i32>(_e194)));
    let _e201 = unnamed.packed_indices[1i][0u];
    let _e207 = unnamed.packed_indices[1i][0u];
    let _e212 = screenUV;
    let _e213 = textureSample(wired_bindless_images[(_e201 & 4095u)], wired_bindless_samplers[((_e207 >> bitcast<u32>(12i)) & 255u)], _e212);
    sceneDepth = _e213.x;
    let _e216 = gl_FragCoord_1[2u];
    fragDepth = _e216;
    let _e217 = fragDepth;
    let _e218 = sceneDepth;
    depthDiff = (_e217 - _e218);
    let _e221 = depthDiff;
    fadeFactor = smoothstep(0f, (depth_fade_scale * 0.0005f), _e221);
    let _e223 = fadeFactor;
    let _e225 = base[3u];
    base[3u] = (_e225 * _e223);
    let _e228 = fadeFactor;
    let _e229 = base;
    let _e231 = (_e229.xyz * _e228);
    base[0u] = _e231.x;
    base[1u] = _e231.y;
    base[2u] = _e231.z;
    let _e238 = base;
    out_color = _e238;
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
