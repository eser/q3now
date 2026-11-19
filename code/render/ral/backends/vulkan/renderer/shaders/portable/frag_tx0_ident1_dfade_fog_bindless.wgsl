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

    let _e73 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e73 + 0.5f));
    let _e78 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e80 = fogType;
    let _e83 = fogType;
    return (((_e78 > 0.5f) && (_e80 >= 1i)) && (_e83 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e73 = wired_advanced_fog_enabled_u0028_();
    if !(_e73) {
        return 0f;
    }
    let _e76 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e76, 0.000001f));
    let _e81 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e81 + 0.5f));
    let _e84 = fogType_1;
    if (_e84 == 1i) {
        let _e88 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e88 <= 0f) {
            return 0f;
        }
        let _e90 = viewDepth;
        let _e93 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e90 / _e93), 0f, 1f);
    }
    let _e98 = unnamed.advancedFogColorDensity[3u];
    let _e100 = viewDepth;
    opticalDepth = (max(_e98, 0f) * _e100);
    let _e102 = fogType_1;
    if (_e102 == 2i) {
        let _e104 = opticalDepth;
        return clamp((1f - exp(-(_e104))), 0f, 1f);
    }
    let _e109 = opticalDepth;
    let _e110 = opticalDepth;
    return clamp((1f - exp(-((_e109 * _e110)))), 0f, 1f);
}

fn wired_boost_legacy_lightmap_u0028_vf3_u003b(rgb: ptr<function, vec3<f32>>) -> vec3<f32> {
    var boosted: vec3<f32>;
    var peak: f32;

    let _e73 = (*rgb);
    let _e76 = unnamed.worldLightParams[0u];
    boosted = (_e73 * _e76);
    let _e79 = boosted[0u];
    let _e81 = boosted[1u];
    let _e83 = boosted[2u];
    peak = max(_e79, max(_e81, _e83));
    let _e86 = peak;
    if (_e86 > 1f) {
        let _e88 = peak;
        let _e89 = boosted;
        boosted = (_e89 / vec3(_e88));
    }
    let _e92 = boosted;
    return _e92;
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e74 = (*c);
    (*c) = max(_e74, vec3<f32>(0f, 0f, 0f));
    let _e76 = (*c);
    cutoff = (_e76 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e78 = (*c);
    lo = (_e78 / vec3(12.92f));
    let _e81 = (*c);
    hi = pow(((_e81 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e86 = hi;
    let _e87 = lo;
    let _e88 = cutoff;
    return mix(_e86, _e87, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e88));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param: vec3<f32>;
    var param_1: vec3<f32>;

    let _e76 = (*role);
    let _e78 = (*role);
    let _e83 = unnamed.packed_indices[(_e76 / 4u)][(_e78 % 4u)];
    let _e86 = (*role);
    let _e88 = (*role);
    let _e93 = unnamed.packed_indices[(_e86 / 4u)][(_e88 % 4u)];
    let _e98 = (*uv);
    let _e99 = textureSample(wired_bindless_images[(_e83 & 4095u)], wired_bindless_samplers[((_e93 >> bitcast<u32>(12i)) & 255u)], _e98);
    c_1 = _e99;
    let _e100 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e100))) == 0i) {
        let _e105 = c_1;
        param = _e105.xyz;
        let _e107 = sRGBToLinear_u0028_vf3_u003b((&param));
        c_1[0u] = _e107.x;
        c_1[1u] = _e107.y;
        c_1[2u] = _e107.z;
    }
    let _e114 = (*slot);
    if (lightmap_slot == (_e114 + 1i)) {
        let _e117 = c_1;
        param_1 = _e117.xyz;
        let _e119 = wired_boost_legacy_lightmap_u0028_vf3_u003b((&param_1));
        c_1[0u] = _e119.x;
        c_1[1u] = _e119.y;
        c_1[2u] = _e119.z;
    }
    let _e126 = c_1;
    return _e126;
}

fn main_1() {
    var fog: vec4<f32>;
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

    let _e89 = unnamed.packed_indices[0i][3u];
    let _e95 = unnamed.packed_indices[0i][3u];
    let _e100 = fog_tex_coord_1;
    let _e101 = textureSample(wired_bindless_images[(_e89 & 4095u)], wired_bindless_samplers[((_e95 >> bitcast<u32>(12i)) & 255u)], _e100);
    fog = _e101;
    param_2 = 0u;
    let _e102 = frag_tex_coord0_1;
    param_3 = _e102;
    param_4 = 0i;
    let _e103 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_2), (&param_3), (&param_4));
    sampledColor0_ = _e103;
    let _e104 = sampledColor0_;
    color0_ = _e104;
    let _e105 = color0_;
    base = _e105;
    if override_type_3_ {
        let _e107 = color0_[3u];
        if (_e107 == alpha_test_value) {
            discard;
        }
    } else {
        if override_type_3_1 {
            let _e110 = color0_[3u];
            if (_e110 >= alpha_test_value) {
                discard;
            }
        } else {
            if override_type_3_2 {
                let _e113 = color0_[3u];
                if (_e113 < alpha_test_value) {
                    discard;
                }
            }
        }
    }
    let _e115 = color0_;
    base = _e115;
    if override_type_3_3 {
        let _e118 = unnamed.worldLightParams[1u];
        wetness = clamp(_e118, 0f, 1f);
        let _e122 = unnamed.worldLightParams[2u];
        frost = clamp(_e122, 0f, 1f);
        let _e124 = base;
        luminance = dot(_e124.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e127 = wetness;
        let _e129 = base;
        let _e131 = (_e129.xyz * mix(1f, 0.82f, _e127));
        base[0u] = _e131.x;
        base[1u] = _e131.y;
        base[2u] = _e131.z;
        let _e138 = base;
        let _e140 = luminance;
        let _e142 = luminance;
        let _e144 = luminance;
        let _e146 = frost;
        let _e149 = mix(_e138.xyz, vec3<f32>((_e140 * 0.88f), (_e142 * 0.94f), _e144), vec3((_e146 * 0.55f)));
        base[0u] = _e149.x;
        base[1u] = _e149.y;
        base[2u] = _e149.z;
    }
    let _e158 = unnamed.materialFactors[0u];
    if (_e158 >= 0f) {
        let _e161 = unnamed.materialBaseColor;
        let _e162 = base;
        base = (_e162 * _e161);
    }
    let _e164 = color0_;
    let _e167 = unnamed.emissionRadiance;
    let _e170 = base;
    let _e172 = (_e170.xyz + (_e164.xyz * _e167.xyz));
    base[0u] = _e172.x;
    base[1u] = _e172.y;
    base[2u] = _e172.z;
    let _e179 = wired_advanced_fog_enabled_u0028_();
    if _e179 {
        let _e180 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e180;
        if override_type_3_4 {
            let _e181 = fogAmount;
            let _e183 = base;
            let _e185 = (_e183.xyz * (1f - _e181));
            base[0u] = _e185.x;
            base[1u] = _e185.y;
            base[2u] = _e185.z;
        } else {
            if override_type_3_5 {
                let _e192 = fogAmount;
                let _e194 = base;
                base = (_e194 * (1f - _e192));
            } else {
                if override_type_3_6 {
                    let _e196 = fogAmount;
                    let _e199 = base[3u];
                    base[3u] = (_e199 * (1f - _e196));
                } else {
                    let _e202 = base;
                    let _e205 = unnamed.advancedFogColorDensity;
                    let _e207 = fogAmount;
                    let _e209 = mix(_e202.xyz, _e205.xyz, vec3(_e207));
                    base[0u] = _e209.x;
                    base[1u] = _e209.y;
                    base[2u] = _e209.z;
                }
            }
        }
    } else {
        if override_type_3_7 {
            let _e216 = base;
            let _e219 = fog[3u];
            let _e221 = (_e216.xyz * (1f - _e219));
            base[0u] = _e221.x;
            base[1u] = _e221.y;
            base[2u] = _e221.z;
        } else {
            if override_type_3_8 {
                let _e228 = base;
                let _e230 = fog[3u];
                base = (_e228 * (1f - _e230));
            } else {
                if override_type_3_9 {
                    let _e234 = base[3u];
                    let _e236 = fog[3u];
                    base[3u] = (_e234 * (1f - _e236));
                } else {
                    let _e240 = base;
                    let _e241 = fog;
                    let _e243 = unnamed.fogColor;
                    let _e246 = fog[3u];
                    base = mix(_e240, (_e241 * _e243), vec4(_e246));
                }
            }
        }
    }
    if override_type_3_10 {
        let _e250 = base[3u];
        if (_e250 == 0f) {
            discard;
        }
    } else {
        if override_type_3_11 {
            let _e252 = base;
            let _e254 = base;
            if (dot(_e252.xyz, _e254.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e258 = gl_FragCoord_1;
    let _e263 = unnamed.packed_indices[1i][0u];
    let _e269 = unnamed.packed_indices[1i][0u];
    let _e274 = textureDimensions(wired_bindless_images[(_e263 & 4095u)], 0i);
    screenUV = (_e258.xy / vec2<f32>(vec2<i32>(_e274)));
    let _e281 = unnamed.packed_indices[1i][0u];
    let _e287 = unnamed.packed_indices[1i][0u];
    let _e292 = screenUV;
    let _e293 = textureSample(wired_bindless_images[(_e281 & 4095u)], wired_bindless_samplers[((_e287 >> bitcast<u32>(12i)) & 255u)], _e292);
    sceneDepth = _e293.x;
    let _e296 = gl_FragCoord_1[2u];
    fragDepth = _e296;
    let _e297 = fragDepth;
    let _e298 = sceneDepth;
    depthDiff = (_e297 - _e298);
    let _e301 = depthDiff;
    fadeFactor = smoothstep(0f, (depth_fade_scale * 0.0005f), _e301);
    let _e303 = fadeFactor;
    let _e305 = base[3u];
    base[3u] = (_e305 * _e303);
    let _e308 = fadeFactor;
    let _e309 = base;
    let _e311 = (_e309.xyz * _e308);
    base[0u] = _e311.x;
    base[1u] = _e311.y;
    base[2u] = _e311.z;
    let _e318 = base;
    out_color = _e318;
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
