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
@id(10) override acff: i32 = 0i;
override override_type_3_4: bool = (acff == 1i);
override override_type_3_5: bool = (acff == 2i);
override override_type_3_6: bool = (acff == 3i);
override override_type_3_7: bool = (acff == 1i);
override override_type_3_8: bool = (acff == 2i);
override override_type_3_9: bool = (acff == 3i);
@id(7) override discard_mode: i32 = 0i;
override override_type_3_10: bool = (discard_mode == 1i);
override override_type_3_11: bool = (discard_mode == 2i);
@id(11) override depth_fade_scale: f32 = 2f;
@id(3) override alpha_to_coverage: i32 = 0i;

@group(0) @binding(0)
var<uniform> unnamed: UBO;
var<private> gl_FragCoord_1: vec4<f32>;
@group(1) @binding(0)
var wired_bindless_images: binding_array<texture_2d<f32>>;
@group(1) @binding(1)
var wired_bindless_samplers: binding_array<sampler>;
var<private> fog_tex_coord_1: vec2<f32>;
var<private> frag_tex_coord0_1: vec2<f32>;
var<private> out_color: vec4<f32>;

fn wired_advanced_fog_enabled_u0028_() -> bool {
    var fogType: i32;

    let _e71 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e71 + 0.5f));
    let _e76 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e78 = fogType;
    let _e81 = fogType;
    return (((_e76 > 0.5f) && (_e78 >= 1i)) && (_e81 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e71 = wired_advanced_fog_enabled_u0028_();
    if !(_e71) {
        return 0f;
    }
    let _e74 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e74, 0.000001f));
    let _e79 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e79 + 0.5f));
    let _e82 = fogType_1;
    if (_e82 == 1i) {
        let _e86 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e86 <= 0f) {
            return 0f;
        }
        let _e88 = viewDepth;
        let _e91 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e88 / _e91), 0f, 1f);
    }
    let _e96 = unnamed.advancedFogColorDensity[3u];
    let _e98 = viewDepth;
    opticalDepth = (max(_e96, 0f) * _e98);
    let _e100 = fogType_1;
    if (_e100 == 2i) {
        let _e102 = opticalDepth;
        return clamp((1f - exp(-(_e102))), 0f, 1f);
    }
    let _e107 = opticalDepth;
    let _e108 = opticalDepth;
    return clamp((1f - exp(-((_e107 * _e108)))), 0f, 1f);
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e72 = (*c);
    (*c) = max(_e72, vec3<f32>(0f, 0f, 0f));
    let _e74 = (*c);
    cutoff = (_e74 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e76 = (*c);
    lo = (_e76 / vec3(12.92f));
    let _e79 = (*c);
    hi = pow(((_e79 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e84 = hi;
    let _e85 = lo;
    let _e86 = cutoff;
    return mix(_e84, _e85, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e86));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param: vec3<f32>;

    let _e73 = (*role);
    let _e75 = (*role);
    let _e80 = unnamed.packed_indices[(_e73 / 4u)][(_e75 % 4u)];
    let _e83 = (*role);
    let _e85 = (*role);
    let _e90 = unnamed.packed_indices[(_e83 / 4u)][(_e85 % 4u)];
    let _e95 = (*uv);
    let _e96 = textureSample(wired_bindless_images[(_e80 & 4095u)], wired_bindless_samplers[((_e90 >> bitcast<u32>(12i)) & 255u)], _e95);
    c_1 = _e96;
    let _e97 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e97))) == 0i) {
        let _e102 = c_1;
        param = _e102.xyz;
        let _e104 = sRGBToLinear_u0028_vf3_u003b((&param));
        c_1[0u] = _e104.x;
        c_1[1u] = _e104.y;
        c_1[2u] = _e104.z;
    }
    let _e111 = (*slot);
    if (lightmap_slot == (_e111 + 1i)) {
        let _e116 = unnamed.worldLightParams[0u];
        let _e117 = c_1;
        let _e119 = (_e117.xyz * _e116);
        c_1[0u] = _e119.x;
        c_1[1u] = _e119.y;
        c_1[2u] = _e119.z;
    }
    let _e126 = c_1;
    return _e126;
}

fn main_1() {
    var fog: vec4<f32>;
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

    let _e86 = unnamed.packed_indices[0i][3u];
    let _e92 = unnamed.packed_indices[0i][3u];
    let _e97 = fog_tex_coord_1;
    let _e98 = textureSample(wired_bindless_images[(_e86 & 4095u)], wired_bindless_samplers[((_e92 >> bitcast<u32>(12i)) & 255u)], _e97);
    fog = _e98;
    param_1 = 0u;
    let _e99 = frag_tex_coord0_1;
    param_2 = _e99;
    param_3 = 0i;
    let _e100 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_1), (&param_2), (&param_3));
    color0_ = _e100;
    let _e101 = color0_;
    base = _e101;
    if override_type_3_ {
        let _e103 = color0_[3u];
        if (_e103 == alpha_test_value) {
            discard;
        }
    } else {
        if override_type_3_1 {
            let _e106 = color0_[3u];
            if (_e106 >= alpha_test_value) {
                discard;
            }
        } else {
            if override_type_3_2 {
                let _e109 = color0_[3u];
                if (_e109 < alpha_test_value) {
                    discard;
                }
            }
        }
    }
    let _e111 = color0_;
    base = _e111;
    if override_type_3_3 {
        let _e114 = unnamed.worldLightParams[1u];
        wetness = clamp(_e114, 0f, 1f);
        let _e118 = unnamed.worldLightParams[2u];
        frost = clamp(_e118, 0f, 1f);
        let _e120 = base;
        luminance = dot(_e120.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e123 = wetness;
        let _e125 = base;
        let _e127 = (_e125.xyz * mix(1f, 0.82f, _e123));
        base[0u] = _e127.x;
        base[1u] = _e127.y;
        base[2u] = _e127.z;
        let _e134 = base;
        let _e136 = luminance;
        let _e138 = luminance;
        let _e140 = luminance;
        let _e142 = frost;
        let _e145 = mix(_e134.xyz, vec3<f32>((_e136 * 0.88f), (_e138 * 0.94f), _e140), vec3((_e142 * 0.55f)));
        base[0u] = _e145.x;
        base[1u] = _e145.y;
        base[2u] = _e145.z;
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
        if override_type_3_4 {
            let _e169 = fogAmount;
            let _e171 = base;
            let _e173 = (_e171.xyz * (1f - _e169));
            base[0u] = _e173.x;
            base[1u] = _e173.y;
            base[2u] = _e173.z;
        } else {
            if override_type_3_5 {
                let _e180 = fogAmount;
                let _e182 = base;
                base = (_e182 * (1f - _e180));
            } else {
                if override_type_3_6 {
                    let _e184 = fogAmount;
                    let _e187 = base[3u];
                    base[3u] = (_e187 * (1f - _e184));
                } else {
                    let _e190 = base;
                    let _e193 = unnamed.advancedFogColorDensity;
                    let _e195 = fogAmount;
                    let _e197 = mix(_e190.xyz, _e193.xyz, vec3(_e195));
                    base[0u] = _e197.x;
                    base[1u] = _e197.y;
                    base[2u] = _e197.z;
                }
            }
        }
    } else {
        if override_type_3_7 {
            let _e204 = base;
            let _e207 = fog[3u];
            let _e209 = (_e204.xyz * (1f - _e207));
            base[0u] = _e209.x;
            base[1u] = _e209.y;
            base[2u] = _e209.z;
        } else {
            if override_type_3_8 {
                let _e216 = base;
                let _e218 = fog[3u];
                base = (_e216 * (1f - _e218));
            } else {
                if override_type_3_9 {
                    let _e222 = base[3u];
                    let _e224 = fog[3u];
                    base[3u] = (_e222 * (1f - _e224));
                } else {
                    let _e228 = base;
                    let _e229 = fog;
                    let _e231 = unnamed.fogColor;
                    let _e234 = fog[3u];
                    base = mix(_e228, (_e229 * _e231), vec4(_e234));
                }
            }
        }
    }
    if override_type_3_10 {
        let _e238 = base[3u];
        if (_e238 == 0f) {
            discard;
        }
    } else {
        if override_type_3_11 {
            let _e240 = base;
            let _e242 = base;
            if (dot(_e240.xyz, _e242.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e246 = gl_FragCoord_1;
    let _e251 = unnamed.packed_indices[1i][0u];
    let _e257 = unnamed.packed_indices[1i][0u];
    let _e262 = textureDimensions(wired_bindless_images[(_e251 & 4095u)], 0i);
    screenUV = (_e246.xy / vec2<f32>(vec2<i32>(_e262)));
    let _e269 = unnamed.packed_indices[1i][0u];
    let _e275 = unnamed.packed_indices[1i][0u];
    let _e280 = screenUV;
    let _e281 = textureSample(wired_bindless_images[(_e269 & 4095u)], wired_bindless_samplers[((_e275 >> bitcast<u32>(12i)) & 255u)], _e280);
    sceneDepth = _e281.x;
    let _e284 = gl_FragCoord_1[2u];
    fragDepth = _e284;
    let _e285 = fragDepth;
    let _e286 = sceneDepth;
    depthDiff = (_e285 - _e286);
    let _e289 = depthDiff;
    fadeFactor = smoothstep(0f, (depth_fade_scale * 0.0005f), _e289);
    let _e291 = fadeFactor;
    let _e293 = base[3u];
    base[3u] = (_e293 * _e291);
    let _e296 = fadeFactor;
    let _e297 = base;
    let _e299 = (_e297.xyz * _e296);
    base[0u] = _e299.x;
    base[1u] = _e299.y;
    base[2u] = _e299.z;
    let _e306 = base;
    out_color = _e306;
    return;
}

@fragment
fn main(@builtin(position) gl_FragCoord: vec4<f32>, @location(4) fog_tex_coord: vec2<f32>, @location(1) frag_tex_coord0_: vec2<f32>) -> @location(0) vec4<f32> {
    gl_FragCoord_1 = gl_FragCoord;
    fog_tex_coord_1 = fog_tex_coord;
    frag_tex_coord0_1 = frag_tex_coord0_;
    main_1();
    let _e7 = out_color;
    return _e7;
}
