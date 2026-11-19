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
@id(8) override identity_color: f32 = 1f;
@id(9) override identity_alpha: f32 = 1f;
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

    let _e75 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e75 + 0.5f));
    let _e80 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e82 = fogType;
    let _e85 = fogType;
    return (((_e80 > 0.5f) && (_e82 >= 1i)) && (_e85 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e75 = wired_advanced_fog_enabled_u0028_();
    if !(_e75) {
        return 0f;
    }
    let _e78 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e78, 0.000001f));
    let _e83 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e83 + 0.5f));
    let _e86 = fogType_1;
    if (_e86 == 1i) {
        let _e90 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e90 <= 0f) {
            return 0f;
        }
        let _e92 = viewDepth;
        let _e95 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e92 / _e95), 0f, 1f);
    }
    let _e100 = unnamed.advancedFogColorDensity[3u];
    let _e102 = viewDepth;
    opticalDepth = (max(_e100, 0f) * _e102);
    let _e104 = fogType_1;
    if (_e104 == 2i) {
        let _e106 = opticalDepth;
        return clamp((1f - exp(-(_e106))), 0f, 1f);
    }
    let _e111 = opticalDepth;
    let _e112 = opticalDepth;
    return clamp((1f - exp(-((_e111 * _e112)))), 0f, 1f);
}

fn wired_boost_legacy_lightmap_u0028_vf3_u003b(rgb: ptr<function, vec3<f32>>) -> vec3<f32> {
    var boosted: vec3<f32>;
    var peak: f32;

    let _e75 = (*rgb);
    let _e78 = unnamed.worldLightParams[0u];
    boosted = (_e75 * _e78);
    let _e81 = boosted[0u];
    let _e83 = boosted[1u];
    let _e85 = boosted[2u];
    peak = max(_e81, max(_e83, _e85));
    let _e88 = peak;
    if (_e88 > 1f) {
        let _e90 = peak;
        let _e91 = boosted;
        boosted = (_e91 / vec3(_e90));
    }
    let _e94 = boosted;
    return _e94;
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e76 = (*c);
    (*c) = max(_e76, vec3<f32>(0f, 0f, 0f));
    let _e78 = (*c);
    cutoff = (_e78 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e80 = (*c);
    lo = (_e80 / vec3(12.92f));
    let _e83 = (*c);
    hi = pow(((_e83 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e88 = hi;
    let _e89 = lo;
    let _e90 = cutoff;
    return mix(_e88, _e89, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e90));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param: vec3<f32>;
    var param_1: vec3<f32>;

    let _e78 = (*role);
    let _e80 = (*role);
    let _e85 = unnamed.packed_indices[(_e78 / 4u)][(_e80 % 4u)];
    let _e88 = (*role);
    let _e90 = (*role);
    let _e95 = unnamed.packed_indices[(_e88 / 4u)][(_e90 % 4u)];
    let _e100 = (*uv);
    let _e101 = textureSample(wired_bindless_images[(_e85 & 4095u)], wired_bindless_samplers[((_e95 >> bitcast<u32>(12i)) & 255u)], _e100);
    c_1 = _e101;
    let _e102 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e102))) == 0i) {
        let _e107 = c_1;
        param = _e107.xyz;
        let _e109 = sRGBToLinear_u0028_vf3_u003b((&param));
        c_1[0u] = _e109.x;
        c_1[1u] = _e109.y;
        c_1[2u] = _e109.z;
    }
    let _e116 = (*slot);
    if (lightmap_slot == (_e116 + 1i)) {
        let _e119 = c_1;
        param_1 = _e119.xyz;
        let _e121 = wired_boost_legacy_lightmap_u0028_vf3_u003b((&param_1));
        c_1[0u] = _e121.x;
        c_1[1u] = _e121.y;
        c_1[2u] = _e121.z;
    }
    let _e128 = c_1;
    return _e128;
}

fn main_1() {
    var fog: vec4<f32>;
    var sampledColor0_: vec4<f32>;
    var param_2: u32;
    var param_3: vec2<f32>;
    var param_4: i32;
    var frag_color: vec4<f32>;
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

    let _e92 = unnamed.packed_indices[0i][3u];
    let _e98 = unnamed.packed_indices[0i][3u];
    let _e103 = fog_tex_coord_1;
    let _e104 = textureSample(wired_bindless_images[(_e92 & 4095u)], wired_bindless_samplers[((_e98 >> bitcast<u32>(12i)) & 255u)], _e103);
    fog = _e104;
    param_2 = 0u;
    let _e105 = frag_tex_coord0_1;
    param_3 = _e105;
    param_4 = 0i;
    let _e106 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_2), (&param_3), (&param_4));
    sampledColor0_ = _e106;
    frag_color[0u] = identity_color;
    frag_color[1u] = identity_color;
    frag_color[2u] = identity_color;
    frag_color[3u] = identity_alpha;
    let _e111 = sampledColor0_;
    let _e112 = frag_color;
    color0_ = (_e111 * _e112);
    let _e114 = color0_;
    base = _e114;
    if override_type_3_ {
        let _e116 = color0_[3u];
        if (_e116 == alpha_test_value) {
            discard;
        }
    } else {
        if override_type_3_1 {
            let _e119 = color0_[3u];
            if (_e119 >= alpha_test_value) {
                discard;
            }
        } else {
            if override_type_3_2 {
                let _e122 = color0_[3u];
                if (_e122 < alpha_test_value) {
                    discard;
                }
            }
        }
    }
    let _e124 = color0_;
    base = _e124;
    if override_type_3_3 {
        let _e127 = unnamed.worldLightParams[1u];
        wetness = clamp(_e127, 0f, 1f);
        let _e131 = unnamed.worldLightParams[2u];
        frost = clamp(_e131, 0f, 1f);
        let _e133 = base;
        luminance = dot(_e133.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e136 = wetness;
        let _e138 = base;
        let _e140 = (_e138.xyz * mix(1f, 0.82f, _e136));
        base[0u] = _e140.x;
        base[1u] = _e140.y;
        base[2u] = _e140.z;
        let _e147 = base;
        let _e149 = luminance;
        let _e151 = luminance;
        let _e153 = luminance;
        let _e155 = frost;
        let _e158 = mix(_e147.xyz, vec3<f32>((_e149 * 0.88f), (_e151 * 0.94f), _e153), vec3((_e155 * 0.55f)));
        base[0u] = _e158.x;
        base[1u] = _e158.y;
        base[2u] = _e158.z;
    }
    let _e167 = unnamed.materialFactors[0u];
    if (_e167 >= 0f) {
        let _e170 = unnamed.materialBaseColor;
        let _e171 = base;
        base = (_e171 * _e170);
    }
    let _e173 = color0_;
    let _e176 = unnamed.emissionRadiance;
    let _e179 = base;
    let _e181 = (_e179.xyz + (_e173.xyz * _e176.xyz));
    base[0u] = _e181.x;
    base[1u] = _e181.y;
    base[2u] = _e181.z;
    let _e188 = wired_advanced_fog_enabled_u0028_();
    if _e188 {
        let _e189 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e189;
        if override_type_3_4 {
            let _e190 = fogAmount;
            let _e192 = base;
            let _e194 = (_e192.xyz * (1f - _e190));
            base[0u] = _e194.x;
            base[1u] = _e194.y;
            base[2u] = _e194.z;
        } else {
            if override_type_3_5 {
                let _e201 = fogAmount;
                let _e203 = base;
                base = (_e203 * (1f - _e201));
            } else {
                if override_type_3_6 {
                    let _e205 = fogAmount;
                    let _e208 = base[3u];
                    base[3u] = (_e208 * (1f - _e205));
                } else {
                    let _e211 = base;
                    let _e214 = unnamed.advancedFogColorDensity;
                    let _e216 = fogAmount;
                    let _e218 = mix(_e211.xyz, _e214.xyz, vec3(_e216));
                    base[0u] = _e218.x;
                    base[1u] = _e218.y;
                    base[2u] = _e218.z;
                }
            }
        }
    } else {
        if override_type_3_7 {
            let _e225 = base;
            let _e228 = fog[3u];
            let _e230 = (_e225.xyz * (1f - _e228));
            base[0u] = _e230.x;
            base[1u] = _e230.y;
            base[2u] = _e230.z;
        } else {
            if override_type_3_8 {
                let _e237 = base;
                let _e239 = fog[3u];
                base = (_e237 * (1f - _e239));
            } else {
                if override_type_3_9 {
                    let _e243 = base[3u];
                    let _e245 = fog[3u];
                    base[3u] = (_e243 * (1f - _e245));
                } else {
                    let _e249 = base;
                    let _e250 = fog;
                    let _e252 = unnamed.fogColor;
                    let _e255 = fog[3u];
                    base = mix(_e249, (_e250 * _e252), vec4(_e255));
                }
            }
        }
    }
    if override_type_3_10 {
        let _e259 = base[3u];
        if (_e259 == 0f) {
            discard;
        }
    } else {
        if override_type_3_11 {
            let _e261 = base;
            let _e263 = base;
            if (dot(_e261.xyz, _e263.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e267 = gl_FragCoord_1;
    let _e272 = unnamed.packed_indices[1i][0u];
    let _e278 = unnamed.packed_indices[1i][0u];
    let _e283 = textureDimensions(wired_bindless_images[(_e272 & 4095u)], 0i);
    screenUV = (_e267.xy / vec2<f32>(vec2<i32>(_e283)));
    let _e290 = unnamed.packed_indices[1i][0u];
    let _e296 = unnamed.packed_indices[1i][0u];
    let _e301 = screenUV;
    let _e302 = textureSample(wired_bindless_images[(_e290 & 4095u)], wired_bindless_samplers[((_e296 >> bitcast<u32>(12i)) & 255u)], _e301);
    sceneDepth = _e302.x;
    let _e305 = gl_FragCoord_1[2u];
    fragDepth = _e305;
    let _e306 = fragDepth;
    let _e307 = sceneDepth;
    depthDiff = (_e306 - _e307);
    let _e310 = depthDiff;
    fadeFactor = smoothstep(0f, (depth_fade_scale * 0.0005f), _e310);
    let _e312 = fadeFactor;
    let _e314 = base[3u];
    base[3u] = (_e314 * _e312);
    let _e317 = fadeFactor;
    let _e318 = base;
    let _e320 = (_e318.xyz * _e317);
    base[0u] = _e320.x;
    base[1u] = _e320.y;
    base[2u] = _e320.z;
    let _e327 = base;
    out_color = _e327;
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
