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
    color0_ = _e74;
    let _e75 = color0_;
    base = _e75;
    if override_type_3_ {
        let _e77 = color0_[3u];
        if (_e77 == alpha_test_value) {
            discard;
        }
    } else {
        if override_type_3_1 {
            let _e80 = color0_[3u];
            if (_e80 >= alpha_test_value) {
                discard;
            }
        } else {
            if override_type_3_2 {
                let _e83 = color0_[3u];
                if (_e83 < alpha_test_value) {
                    discard;
                }
            }
        }
    }
    let _e85 = color0_;
    base = _e85;
    if override_type_3_3 {
        let _e88 = unnamed.worldLightParams[1u];
        wetness = clamp(_e88, 0f, 1f);
        let _e92 = unnamed.worldLightParams[2u];
        frost = clamp(_e92, 0f, 1f);
        let _e94 = base;
        luminance = dot(_e94.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e97 = wetness;
        let _e99 = base;
        let _e101 = (_e99.xyz * mix(1f, 0.82f, _e97));
        base[0u] = _e101.x;
        base[1u] = _e101.y;
        base[2u] = _e101.z;
        let _e108 = base;
        let _e110 = luminance;
        let _e112 = luminance;
        let _e114 = luminance;
        let _e116 = frost;
        let _e119 = mix(_e108.xyz, vec3<f32>((_e110 * 0.88f), (_e112 * 0.94f), _e114), vec3((_e116 * 0.55f)));
        base[0u] = _e119.x;
        base[1u] = _e119.y;
        base[2u] = _e119.z;
    }
    let _e126 = color0_;
    let _e129 = unnamed.emissionRadiance;
    let _e132 = base;
    let _e134 = (_e132.xyz + (_e126.xyz * _e129.xyz));
    base[0u] = _e134.x;
    base[1u] = _e134.y;
    base[2u] = _e134.z;
    let _e141 = wired_advanced_fog_enabled_u0028_();
    if _e141 {
        let _e142 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e142;
        let _e143 = base;
        let _e146 = unnamed.advancedFogColorDensity;
        let _e148 = fogAmount;
        let _e150 = mix(_e143.xyz, _e146.xyz, vec3(_e148));
        base[0u] = _e150.x;
        base[1u] = _e150.y;
        base[2u] = _e150.z;
    }
    if override_type_3_4 {
        let _e158 = base[3u];
        if (_e158 == 0f) {
            discard;
        }
    } else {
        if override_type_3_5 {
            let _e160 = base;
            let _e162 = base;
            if (dot(_e160.xyz, _e162.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e166 = gl_FragCoord_1;
    let _e171 = unnamed.packed_indices[1i][0u];
    let _e177 = unnamed.packed_indices[1i][0u];
    let _e182 = textureDimensions(wired_bindless_images[(_e171 & 4095u)], 0i);
    screenUV = (_e166.xy / vec2<f32>(vec2<i32>(_e182)));
    let _e189 = unnamed.packed_indices[1i][0u];
    let _e195 = unnamed.packed_indices[1i][0u];
    let _e200 = screenUV;
    let _e201 = textureSample(wired_bindless_images[(_e189 & 4095u)], wired_bindless_samplers[((_e195 >> bitcast<u32>(12i)) & 255u)], _e200);
    sceneDepth = _e201.x;
    let _e204 = gl_FragCoord_1[2u];
    fragDepth = _e204;
    let _e205 = fragDepth;
    let _e206 = sceneDepth;
    depthDiff = (_e205 - _e206);
    let _e209 = depthDiff;
    fadeFactor = smoothstep(0f, (depth_fade_scale * 0.0005f), _e209);
    let _e211 = fadeFactor;
    let _e213 = base[3u];
    base[3u] = (_e213 * _e211);
    let _e216 = fadeFactor;
    let _e217 = base;
    let _e219 = (_e217.xyz * _e216);
    base[0u] = _e219.x;
    base[1u] = _e219.y;
    base[2u] = _e219.z;
    let _e226 = base;
    out_color = _e226;
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
