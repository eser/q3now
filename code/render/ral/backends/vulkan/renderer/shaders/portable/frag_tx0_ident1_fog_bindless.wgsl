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

    let _e69 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e69 + 0.5f));
    let _e74 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e76 = fogType;
    let _e79 = fogType;
    return (((_e74 > 0.5f) && (_e76 >= 1i)) && (_e79 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e69 = wired_advanced_fog_enabled_u0028_();
    if !(_e69) {
        return 0f;
    }
    let _e72 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e72, 0.000001f));
    let _e77 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e77 + 0.5f));
    let _e80 = fogType_1;
    if (_e80 == 1i) {
        let _e84 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e84 <= 0f) {
            return 0f;
        }
        let _e86 = viewDepth;
        let _e89 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e86 / _e89), 0f, 1f);
    }
    let _e94 = unnamed.advancedFogColorDensity[3u];
    let _e96 = viewDepth;
    opticalDepth = (max(_e94, 0f) * _e96);
    let _e98 = fogType_1;
    if (_e98 == 2i) {
        let _e100 = opticalDepth;
        return clamp((1f - exp(-(_e100))), 0f, 1f);
    }
    let _e105 = opticalDepth;
    let _e106 = opticalDepth;
    return clamp((1f - exp(-((_e105 * _e106)))), 0f, 1f);
}

fn wired_boost_legacy_lightmap_u0028_vf3_u003b(rgb: ptr<function, vec3<f32>>) -> vec3<f32> {
    var boosted: vec3<f32>;
    var peak: f32;

    let _e69 = (*rgb);
    let _e72 = unnamed.worldLightParams[0u];
    boosted = (_e69 * _e72);
    let _e75 = boosted[0u];
    let _e77 = boosted[1u];
    let _e79 = boosted[2u];
    peak = max(_e75, max(_e77, _e79));
    let _e82 = peak;
    if (_e82 > 1f) {
        let _e84 = peak;
        let _e85 = boosted;
        boosted = (_e85 / vec3(_e84));
    }
    let _e88 = boosted;
    return _e88;
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e70 = (*c);
    (*c) = max(_e70, vec3<f32>(0f, 0f, 0f));
    let _e72 = (*c);
    cutoff = (_e72 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e74 = (*c);
    lo = (_e74 / vec3(12.92f));
    let _e77 = (*c);
    hi = pow(((_e77 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e82 = hi;
    let _e83 = lo;
    let _e84 = cutoff;
    return mix(_e82, _e83, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e84));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param: vec3<f32>;
    var param_1: vec3<f32>;

    let _e72 = (*role);
    let _e74 = (*role);
    let _e79 = unnamed.packed_indices[(_e72 / 4u)][(_e74 % 4u)];
    let _e82 = (*role);
    let _e84 = (*role);
    let _e89 = unnamed.packed_indices[(_e82 / 4u)][(_e84 % 4u)];
    let _e94 = (*uv);
    let _e95 = textureSample(wired_bindless_images[(_e79 & 4095u)], wired_bindless_samplers[((_e89 >> bitcast<u32>(12i)) & 255u)], _e94);
    c_1 = _e95;
    let _e96 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e96))) == 0i) {
        let _e101 = c_1;
        param = _e101.xyz;
        let _e103 = sRGBToLinear_u0028_vf3_u003b((&param));
        c_1[0u] = _e103.x;
        c_1[1u] = _e103.y;
        c_1[2u] = _e103.z;
    }
    let _e110 = (*slot);
    if (lightmap_slot == (_e110 + 1i)) {
        let _e113 = c_1;
        param_1 = _e113.xyz;
        let _e115 = wired_boost_legacy_lightmap_u0028_vf3_u003b((&param_1));
        c_1[0u] = _e115.x;
        c_1[1u] = _e115.y;
        c_1[2u] = _e115.z;
    }
    let _e122 = c_1;
    return _e122;
}

fn main_1() {
    var fog: vec4<f32>;
    var color0_: vec4<f32>;
    var param_2: u32;
    var param_3: vec2<f32>;
    var param_4: i32;
    var base: vec4<f32>;
    var wetness: f32;
    var frost: f32;
    var luminance: f32;
    var fogAmount: f32;

    let _e79 = unnamed.packed_indices[0i][3u];
    let _e85 = unnamed.packed_indices[0i][3u];
    let _e90 = fog_tex_coord_1;
    let _e91 = textureSample(wired_bindless_images[(_e79 & 4095u)], wired_bindless_samplers[((_e85 >> bitcast<u32>(12i)) & 255u)], _e90);
    fog = _e91;
    param_2 = 0u;
    let _e92 = frag_tex_coord0_1;
    param_3 = _e92;
    param_4 = 0i;
    let _e93 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_2), (&param_3), (&param_4));
    color0_ = _e93;
    let _e94 = color0_;
    base = _e94;
    if override_type_3_ {
        let _e96 = color0_[3u];
        if (_e96 == alpha_test_value) {
            discard;
        }
    } else {
        if override_type_3_1 {
            let _e99 = color0_[3u];
            if (_e99 >= alpha_test_value) {
                discard;
            }
        } else {
            if override_type_3_2 {
                let _e102 = color0_[3u];
                if (_e102 < alpha_test_value) {
                    discard;
                }
            }
        }
    }
    let _e104 = color0_;
    base = _e104;
    if override_type_3_3 {
        let _e107 = unnamed.worldLightParams[1u];
        wetness = clamp(_e107, 0f, 1f);
        let _e111 = unnamed.worldLightParams[2u];
        frost = clamp(_e111, 0f, 1f);
        let _e113 = base;
        luminance = dot(_e113.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e116 = wetness;
        let _e118 = base;
        let _e120 = (_e118.xyz * mix(1f, 0.82f, _e116));
        base[0u] = _e120.x;
        base[1u] = _e120.y;
        base[2u] = _e120.z;
        let _e127 = base;
        let _e129 = luminance;
        let _e131 = luminance;
        let _e133 = luminance;
        let _e135 = frost;
        let _e138 = mix(_e127.xyz, vec3<f32>((_e129 * 0.88f), (_e131 * 0.94f), _e133), vec3((_e135 * 0.55f)));
        base[0u] = _e138.x;
        base[1u] = _e138.y;
        base[2u] = _e138.z;
    }
    let _e145 = color0_;
    let _e148 = unnamed.emissionRadiance;
    let _e151 = base;
    let _e153 = (_e151.xyz + (_e145.xyz * _e148.xyz));
    base[0u] = _e153.x;
    base[1u] = _e153.y;
    base[2u] = _e153.z;
    let _e160 = wired_advanced_fog_enabled_u0028_();
    if _e160 {
        let _e161 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e161;
        if override_type_3_4 {
            let _e162 = fogAmount;
            let _e164 = base;
            let _e166 = (_e164.xyz * (1f - _e162));
            base[0u] = _e166.x;
            base[1u] = _e166.y;
            base[2u] = _e166.z;
        } else {
            if override_type_3_5 {
                let _e173 = fogAmount;
                let _e175 = base;
                base = (_e175 * (1f - _e173));
            } else {
                if override_type_3_6 {
                    let _e177 = fogAmount;
                    let _e180 = base[3u];
                    base[3u] = (_e180 * (1f - _e177));
                } else {
                    let _e183 = base;
                    let _e186 = unnamed.advancedFogColorDensity;
                    let _e188 = fogAmount;
                    let _e190 = mix(_e183.xyz, _e186.xyz, vec3(_e188));
                    base[0u] = _e190.x;
                    base[1u] = _e190.y;
                    base[2u] = _e190.z;
                }
            }
        }
    } else {
        if override_type_3_7 {
            let _e197 = base;
            let _e200 = fog[3u];
            let _e202 = (_e197.xyz * (1f - _e200));
            base[0u] = _e202.x;
            base[1u] = _e202.y;
            base[2u] = _e202.z;
        } else {
            if override_type_3_8 {
                let _e209 = base;
                let _e211 = fog[3u];
                base = (_e209 * (1f - _e211));
            } else {
                if override_type_3_9 {
                    let _e215 = base[3u];
                    let _e217 = fog[3u];
                    base[3u] = (_e215 * (1f - _e217));
                } else {
                    let _e221 = base;
                    let _e222 = fog;
                    let _e224 = unnamed.fogColor;
                    let _e227 = fog[3u];
                    base = mix(_e221, (_e222 * _e224), vec4(_e227));
                }
            }
        }
    }
    if override_type_3_10 {
        let _e231 = base[3u];
        if (_e231 == 0f) {
            discard;
        }
    } else {
        if override_type_3_11 {
            let _e233 = base;
            let _e235 = base;
            if (dot(_e233.xyz, _e235.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e239 = base;
    out_color = _e239;
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
