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
var<private> frag_color0In_1: vec4<f32>;
var<private> frag_tex_coord0_1: vec2<f32>;
var<private> out_color: vec4<f32>;

fn wired_advanced_fog_enabled_u0028_() -> bool {
    var fogType: i32;

    let _e72 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e72 + 0.5f));
    let _e77 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e79 = fogType;
    let _e82 = fogType;
    return (((_e77 > 0.5f) && (_e79 >= 1i)) && (_e82 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e72 = wired_advanced_fog_enabled_u0028_();
    if !(_e72) {
        return 0f;
    }
    let _e75 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e75, 0.000001f));
    let _e80 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e80 + 0.5f));
    let _e83 = fogType_1;
    if (_e83 == 1i) {
        let _e87 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e87 <= 0f) {
            return 0f;
        }
        let _e89 = viewDepth;
        let _e92 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e89 / _e92), 0f, 1f);
    }
    let _e97 = unnamed.advancedFogColorDensity[3u];
    let _e99 = viewDepth;
    opticalDepth = (max(_e97, 0f) * _e99);
    let _e101 = fogType_1;
    if (_e101 == 2i) {
        let _e103 = opticalDepth;
        return clamp((1f - exp(-(_e103))), 0f, 1f);
    }
    let _e108 = opticalDepth;
    let _e109 = opticalDepth;
    return clamp((1f - exp(-((_e108 * _e109)))), 0f, 1f);
}

fn wired_boost_legacy_lightmap_u0028_vf3_u003b(rgb: ptr<function, vec3<f32>>) -> vec3<f32> {
    var boosted: vec3<f32>;
    var peak: f32;

    let _e72 = (*rgb);
    let _e75 = unnamed.worldLightParams[0u];
    boosted = (_e72 * _e75);
    let _e78 = boosted[0u];
    let _e80 = boosted[1u];
    let _e82 = boosted[2u];
    peak = max(_e78, max(_e80, _e82));
    let _e85 = peak;
    if (_e85 > 1f) {
        let _e87 = peak;
        let _e88 = boosted;
        boosted = (_e88 / vec3(_e87));
    }
    let _e91 = boosted;
    return _e91;
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e73 = (*c);
    (*c) = max(_e73, vec3<f32>(0f, 0f, 0f));
    let _e75 = (*c);
    cutoff = (_e75 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e77 = (*c);
    lo = (_e77 / vec3(12.92f));
    let _e80 = (*c);
    hi = pow(((_e80 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e85 = hi;
    let _e86 = lo;
    let _e87 = cutoff;
    return mix(_e85, _e86, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e87));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param: vec3<f32>;
    var param_1: vec3<f32>;

    let _e75 = (*role);
    let _e77 = (*role);
    let _e82 = unnamed.packed_indices[(_e75 / 4u)][(_e77 % 4u)];
    let _e85 = (*role);
    let _e87 = (*role);
    let _e92 = unnamed.packed_indices[(_e85 / 4u)][(_e87 % 4u)];
    let _e97 = (*uv);
    let _e98 = textureSample(wired_bindless_images[(_e82 & 4095u)], wired_bindless_samplers[((_e92 >> bitcast<u32>(12i)) & 255u)], _e97);
    c_1 = _e98;
    let _e99 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e99))) == 0i) {
        let _e104 = c_1;
        param = _e104.xyz;
        let _e106 = sRGBToLinear_u0028_vf3_u003b((&param));
        c_1[0u] = _e106.x;
        c_1[1u] = _e106.y;
        c_1[2u] = _e106.z;
    }
    let _e113 = (*slot);
    if (lightmap_slot == (_e113 + 1i)) {
        let _e116 = c_1;
        param_1 = _e116.xyz;
        let _e118 = wired_boost_legacy_lightmap_u0028_vf3_u003b((&param_1));
        c_1[0u] = _e118.x;
        c_1[1u] = _e118.y;
        c_1[2u] = _e118.z;
    }
    let _e125 = c_1;
    return _e125;
}

fn main_1() {
    var fog: vec4<f32>;
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

    let _e89 = unnamed.packed_indices[0i][3u];
    let _e95 = unnamed.packed_indices[0i][3u];
    let _e100 = fog_tex_coord_1;
    let _e101 = textureSample(wired_bindless_images[(_e89 & 4095u)], wired_bindless_samplers[((_e95 >> bitcast<u32>(12i)) & 255u)], _e100);
    fog = _e101;
    let _e102 = frag_color0In_1;
    param_2 = _e102.xyz;
    let _e104 = sRGBToLinear_u0028_vf3_u003b((&param_2));
    let _e106 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e104.x, _e104.y, _e104.z, _e106);
    param_3 = 0u;
    let _e111 = frag_tex_coord0_1;
    param_4 = _e111;
    param_5 = 0i;
    let _e112 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_3), (&param_4), (&param_5));
    let _e113 = frag_color0_;
    color0_ = (_e112 * _e113);
    let _e115 = color0_;
    base = _e115;
    if override_type_3_ {
        let _e117 = color0_[3u];
        if (_e117 == alpha_test_value) {
            discard;
        }
    } else {
        if override_type_3_1 {
            let _e120 = color0_[3u];
            if (_e120 >= alpha_test_value) {
                discard;
            }
        } else {
            if override_type_3_2 {
                let _e123 = color0_[3u];
                if (_e123 < alpha_test_value) {
                    discard;
                }
            }
        }
    }
    let _e125 = color0_;
    base = _e125;
    if override_type_3_3 {
        let _e128 = unnamed.worldLightParams[1u];
        wetness = clamp(_e128, 0f, 1f);
        let _e132 = unnamed.worldLightParams[2u];
        frost = clamp(_e132, 0f, 1f);
        let _e134 = base;
        luminance = dot(_e134.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e137 = wetness;
        let _e139 = base;
        let _e141 = (_e139.xyz * mix(1f, 0.82f, _e137));
        base[0u] = _e141.x;
        base[1u] = _e141.y;
        base[2u] = _e141.z;
        let _e148 = base;
        let _e150 = luminance;
        let _e152 = luminance;
        let _e154 = luminance;
        let _e156 = frost;
        let _e159 = mix(_e148.xyz, vec3<f32>((_e150 * 0.88f), (_e152 * 0.94f), _e154), vec3((_e156 * 0.55f)));
        base[0u] = _e159.x;
        base[1u] = _e159.y;
        base[2u] = _e159.z;
    }
    let _e166 = color0_;
    let _e169 = unnamed.emissionRadiance;
    let _e172 = base;
    let _e174 = (_e172.xyz + (_e166.xyz * _e169.xyz));
    base[0u] = _e174.x;
    base[1u] = _e174.y;
    base[2u] = _e174.z;
    let _e181 = wired_advanced_fog_enabled_u0028_();
    if _e181 {
        let _e182 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e182;
        if override_type_3_4 {
            let _e183 = fogAmount;
            let _e185 = base;
            let _e187 = (_e185.xyz * (1f - _e183));
            base[0u] = _e187.x;
            base[1u] = _e187.y;
            base[2u] = _e187.z;
        } else {
            if override_type_3_5 {
                let _e194 = fogAmount;
                let _e196 = base;
                base = (_e196 * (1f - _e194));
            } else {
                if override_type_3_6 {
                    let _e198 = fogAmount;
                    let _e201 = base[3u];
                    base[3u] = (_e201 * (1f - _e198));
                } else {
                    let _e204 = base;
                    let _e207 = unnamed.advancedFogColorDensity;
                    let _e209 = fogAmount;
                    let _e211 = mix(_e204.xyz, _e207.xyz, vec3(_e209));
                    base[0u] = _e211.x;
                    base[1u] = _e211.y;
                    base[2u] = _e211.z;
                }
            }
        }
    } else {
        if override_type_3_7 {
            let _e218 = base;
            let _e221 = fog[3u];
            let _e223 = (_e218.xyz * (1f - _e221));
            base[0u] = _e223.x;
            base[1u] = _e223.y;
            base[2u] = _e223.z;
        } else {
            if override_type_3_8 {
                let _e230 = base;
                let _e232 = fog[3u];
                base = (_e230 * (1f - _e232));
            } else {
                if override_type_3_9 {
                    let _e236 = base[3u];
                    let _e238 = fog[3u];
                    base[3u] = (_e236 * (1f - _e238));
                } else {
                    let _e242 = base;
                    let _e243 = fog;
                    let _e245 = unnamed.fogColor;
                    let _e248 = fog[3u];
                    base = mix(_e242, (_e243 * _e245), vec4(_e248));
                }
            }
        }
    }
    if override_type_3_10 {
        let _e252 = base[3u];
        if (_e252 == 0f) {
            discard;
        }
    } else {
        if override_type_3_11 {
            let _e254 = base;
            let _e256 = base;
            if (dot(_e254.xyz, _e256.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e260 = gl_FragCoord_1;
    let _e265 = unnamed.packed_indices[1i][0u];
    let _e271 = unnamed.packed_indices[1i][0u];
    let _e276 = textureDimensions(wired_bindless_images[(_e265 & 4095u)], 0i);
    screenUV = (_e260.xy / vec2<f32>(vec2<i32>(_e276)));
    let _e283 = unnamed.packed_indices[1i][0u];
    let _e289 = unnamed.packed_indices[1i][0u];
    let _e294 = screenUV;
    let _e295 = textureSample(wired_bindless_images[(_e283 & 4095u)], wired_bindless_samplers[((_e289 >> bitcast<u32>(12i)) & 255u)], _e294);
    sceneDepth = _e295.x;
    let _e298 = gl_FragCoord_1[2u];
    fragDepth = _e298;
    let _e299 = fragDepth;
    let _e300 = sceneDepth;
    depthDiff = (_e299 - _e300);
    let _e303 = depthDiff;
    fadeFactor = smoothstep(0f, (depth_fade_scale * 0.0005f), _e303);
    let _e305 = fadeFactor;
    let _e307 = base[3u];
    base[3u] = (_e307 * _e305);
    let _e310 = fadeFactor;
    let _e311 = base;
    let _e313 = (_e311.xyz * _e310);
    base[0u] = _e313.x;
    base[1u] = _e313.y;
    base[2u] = _e313.z;
    let _e320 = base;
    out_color = _e320;
    return;
}

@fragment
fn main(@builtin(position) gl_FragCoord: vec4<f32>, @location(4) fog_tex_coord: vec2<f32>, @location(0) frag_color0In: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>) -> @location(0) vec4<f32> {
    gl_FragCoord_1 = gl_FragCoord;
    fog_tex_coord_1 = fog_tex_coord;
    frag_color0In_1 = frag_color0In;
    frag_tex_coord0_1 = frag_tex_coord0_;
    main_1();
    let _e9 = out_color;
    return _e9;
}
