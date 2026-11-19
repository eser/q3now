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
var<private> frag_color0In_1: vec4<f32>;
var<private> frag_tex_coord0_1: vec2<f32>;
var<private> out_color: vec4<f32>;

fn wired_advanced_fog_enabled_u0028_() -> bool {
    var fogType: i32;

    let _e74 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e74 + 0.5f));
    let _e79 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e81 = fogType;
    let _e84 = fogType;
    return (((_e79 > 0.5f) && (_e81 >= 1i)) && (_e84 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e74 = wired_advanced_fog_enabled_u0028_();
    if !(_e74) {
        return 0f;
    }
    let _e77 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e77, 0.000001f));
    let _e82 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e82 + 0.5f));
    let _e85 = fogType_1;
    if (_e85 == 1i) {
        let _e89 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e89 <= 0f) {
            return 0f;
        }
        let _e91 = viewDepth;
        let _e94 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e91 / _e94), 0f, 1f);
    }
    let _e99 = unnamed.advancedFogColorDensity[3u];
    let _e101 = viewDepth;
    opticalDepth = (max(_e99, 0f) * _e101);
    let _e103 = fogType_1;
    if (_e103 == 2i) {
        let _e105 = opticalDepth;
        return clamp((1f - exp(-(_e105))), 0f, 1f);
    }
    let _e110 = opticalDepth;
    let _e111 = opticalDepth;
    return clamp((1f - exp(-((_e110 * _e111)))), 0f, 1f);
}

fn wired_boost_legacy_lightmap_u0028_vf3_u003b(rgb: ptr<function, vec3<f32>>) -> vec3<f32> {
    var boosted: vec3<f32>;
    var peak: f32;

    let _e74 = (*rgb);
    let _e77 = unnamed.worldLightParams[0u];
    boosted = (_e74 * _e77);
    let _e80 = boosted[0u];
    let _e82 = boosted[1u];
    let _e84 = boosted[2u];
    peak = max(_e80, max(_e82, _e84));
    let _e87 = peak;
    if (_e87 > 1f) {
        let _e89 = peak;
        let _e90 = boosted;
        boosted = (_e90 / vec3(_e89));
    }
    let _e93 = boosted;
    return _e93;
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e75 = (*c);
    (*c) = max(_e75, vec3<f32>(0f, 0f, 0f));
    let _e77 = (*c);
    cutoff = (_e77 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e79 = (*c);
    lo = (_e79 / vec3(12.92f));
    let _e82 = (*c);
    hi = pow(((_e82 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e87 = hi;
    let _e88 = lo;
    let _e89 = cutoff;
    return mix(_e87, _e88, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e89));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param: vec3<f32>;
    var param_1: vec3<f32>;

    let _e77 = (*role);
    let _e79 = (*role);
    let _e84 = unnamed.packed_indices[(_e77 / 4u)][(_e79 % 4u)];
    let _e87 = (*role);
    let _e89 = (*role);
    let _e94 = unnamed.packed_indices[(_e87 / 4u)][(_e89 % 4u)];
    let _e99 = (*uv);
    let _e100 = textureSample(wired_bindless_images[(_e84 & 4095u)], wired_bindless_samplers[((_e94 >> bitcast<u32>(12i)) & 255u)], _e99);
    c_1 = _e100;
    let _e101 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e101))) == 0i) {
        let _e106 = c_1;
        param = _e106.xyz;
        let _e108 = sRGBToLinear_u0028_vf3_u003b((&param));
        c_1[0u] = _e108.x;
        c_1[1u] = _e108.y;
        c_1[2u] = _e108.z;
    }
    let _e115 = (*slot);
    if (lightmap_slot == (_e115 + 1i)) {
        let _e118 = c_1;
        param_1 = _e118.xyz;
        let _e120 = wired_boost_legacy_lightmap_u0028_vf3_u003b((&param_1));
        c_1[0u] = _e120.x;
        c_1[1u] = _e120.y;
        c_1[2u] = _e120.z;
    }
    let _e127 = c_1;
    return _e127;
}

fn main_1() {
    var fog: vec4<f32>;
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

    let _e92 = unnamed.packed_indices[0i][3u];
    let _e98 = unnamed.packed_indices[0i][3u];
    let _e103 = fog_tex_coord_1;
    let _e104 = textureSample(wired_bindless_images[(_e92 & 4095u)], wired_bindless_samplers[((_e98 >> bitcast<u32>(12i)) & 255u)], _e103);
    fog = _e104;
    let _e105 = frag_color0In_1;
    param_2 = _e105.xyz;
    let _e107 = sRGBToLinear_u0028_vf3_u003b((&param_2));
    let _e109 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e107.x, _e107.y, _e107.z, _e109);
    param_3 = 0u;
    let _e114 = frag_tex_coord0_1;
    param_4 = _e114;
    param_5 = 0i;
    let _e115 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_3), (&param_4), (&param_5));
    sampledColor0_ = _e115;
    let _e116 = sampledColor0_;
    let _e117 = frag_color0_;
    color0_ = (_e116 * _e117);
    let _e119 = color0_;
    base = _e119;
    if override_type_3_ {
        let _e121 = color0_[3u];
        if (_e121 == alpha_test_value) {
            discard;
        }
    } else {
        if override_type_3_1 {
            let _e124 = color0_[3u];
            if (_e124 >= alpha_test_value) {
                discard;
            }
        } else {
            if override_type_3_2 {
                let _e127 = color0_[3u];
                if (_e127 < alpha_test_value) {
                    discard;
                }
            }
        }
    }
    let _e129 = color0_;
    base = _e129;
    if override_type_3_3 {
        let _e132 = unnamed.worldLightParams[1u];
        wetness = clamp(_e132, 0f, 1f);
        let _e136 = unnamed.worldLightParams[2u];
        frost = clamp(_e136, 0f, 1f);
        let _e138 = base;
        luminance = dot(_e138.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e141 = wetness;
        let _e143 = base;
        let _e145 = (_e143.xyz * mix(1f, 0.82f, _e141));
        base[0u] = _e145.x;
        base[1u] = _e145.y;
        base[2u] = _e145.z;
        let _e152 = base;
        let _e154 = luminance;
        let _e156 = luminance;
        let _e158 = luminance;
        let _e160 = frost;
        let _e163 = mix(_e152.xyz, vec3<f32>((_e154 * 0.88f), (_e156 * 0.94f), _e158), vec3((_e160 * 0.55f)));
        base[0u] = _e163.x;
        base[1u] = _e163.y;
        base[2u] = _e163.z;
    }
    let _e172 = unnamed.materialFactors[0u];
    if (_e172 >= 0f) {
        let _e175 = unnamed.materialBaseColor;
        let _e176 = base;
        base = (_e176 * _e175);
    }
    let _e178 = color0_;
    let _e181 = unnamed.emissionRadiance;
    let _e184 = base;
    let _e186 = (_e184.xyz + (_e178.xyz * _e181.xyz));
    base[0u] = _e186.x;
    base[1u] = _e186.y;
    base[2u] = _e186.z;
    let _e193 = wired_advanced_fog_enabled_u0028_();
    if _e193 {
        let _e194 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e194;
        if override_type_3_4 {
            let _e195 = fogAmount;
            let _e197 = base;
            let _e199 = (_e197.xyz * (1f - _e195));
            base[0u] = _e199.x;
            base[1u] = _e199.y;
            base[2u] = _e199.z;
        } else {
            if override_type_3_5 {
                let _e206 = fogAmount;
                let _e208 = base;
                base = (_e208 * (1f - _e206));
            } else {
                if override_type_3_6 {
                    let _e210 = fogAmount;
                    let _e213 = base[3u];
                    base[3u] = (_e213 * (1f - _e210));
                } else {
                    let _e216 = base;
                    let _e219 = unnamed.advancedFogColorDensity;
                    let _e221 = fogAmount;
                    let _e223 = mix(_e216.xyz, _e219.xyz, vec3(_e221));
                    base[0u] = _e223.x;
                    base[1u] = _e223.y;
                    base[2u] = _e223.z;
                }
            }
        }
    } else {
        if override_type_3_7 {
            let _e230 = base;
            let _e233 = fog[3u];
            let _e235 = (_e230.xyz * (1f - _e233));
            base[0u] = _e235.x;
            base[1u] = _e235.y;
            base[2u] = _e235.z;
        } else {
            if override_type_3_8 {
                let _e242 = base;
                let _e244 = fog[3u];
                base = (_e242 * (1f - _e244));
            } else {
                if override_type_3_9 {
                    let _e248 = base[3u];
                    let _e250 = fog[3u];
                    base[3u] = (_e248 * (1f - _e250));
                } else {
                    let _e254 = base;
                    let _e255 = fog;
                    let _e257 = unnamed.fogColor;
                    let _e260 = fog[3u];
                    base = mix(_e254, (_e255 * _e257), vec4(_e260));
                }
            }
        }
    }
    if override_type_3_10 {
        let _e264 = base[3u];
        if (_e264 == 0f) {
            discard;
        }
    } else {
        if override_type_3_11 {
            let _e266 = base;
            let _e268 = base;
            if (dot(_e266.xyz, _e268.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e272 = gl_FragCoord_1;
    let _e277 = unnamed.packed_indices[1i][0u];
    let _e283 = unnamed.packed_indices[1i][0u];
    let _e288 = textureDimensions(wired_bindless_images[(_e277 & 4095u)], 0i);
    screenUV = (_e272.xy / vec2<f32>(vec2<i32>(_e288)));
    let _e295 = unnamed.packed_indices[1i][0u];
    let _e301 = unnamed.packed_indices[1i][0u];
    let _e306 = screenUV;
    let _e307 = textureSample(wired_bindless_images[(_e295 & 4095u)], wired_bindless_samplers[((_e301 >> bitcast<u32>(12i)) & 255u)], _e306);
    sceneDepth = _e307.x;
    let _e310 = gl_FragCoord_1[2u];
    fragDepth = _e310;
    let _e311 = fragDepth;
    let _e312 = sceneDepth;
    depthDiff = (_e311 - _e312);
    let _e315 = depthDiff;
    fadeFactor = smoothstep(0f, (depth_fade_scale * 0.0005f), _e315);
    let _e317 = fadeFactor;
    let _e319 = base[3u];
    base[3u] = (_e319 * _e317);
    let _e322 = fadeFactor;
    let _e323 = base;
    let _e325 = (_e323.xyz * _e322);
    base[0u] = _e325.x;
    base[1u] = _e325.y;
    base[2u] = _e325.z;
    let _e332 = base;
    out_color = _e332;
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
