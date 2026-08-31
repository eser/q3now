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

fn wired_boost_legacy_lightmap_u0028_vf3_u003b(rgb: ptr<function, vec3<f32>>) -> vec3<f32> {
    var boosted: vec3<f32>;
    var peak: f32;

    let _e71 = (*rgb);
    let _e74 = unnamed.worldLightParams[0u];
    boosted = (_e71 * _e74);
    let _e77 = boosted[0u];
    let _e79 = boosted[1u];
    let _e81 = boosted[2u];
    peak = max(_e77, max(_e79, _e81));
    let _e84 = peak;
    if (_e84 > 1f) {
        let _e86 = peak;
        let _e87 = boosted;
        boosted = (_e87 / vec3(_e86));
    }
    let _e90 = boosted;
    return _e90;
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
    var param_1: vec3<f32>;

    let _e74 = (*role);
    let _e76 = (*role);
    let _e81 = unnamed.packed_indices[(_e74 / 4u)][(_e76 % 4u)];
    let _e84 = (*role);
    let _e86 = (*role);
    let _e91 = unnamed.packed_indices[(_e84 / 4u)][(_e86 % 4u)];
    let _e96 = (*uv);
    let _e97 = textureSample(wired_bindless_images[(_e81 & 4095u)], wired_bindless_samplers[((_e91 >> bitcast<u32>(12i)) & 255u)], _e96);
    c_1 = _e97;
    let _e98 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e98))) == 0i) {
        let _e103 = c_1;
        param = _e103.xyz;
        let _e105 = sRGBToLinear_u0028_vf3_u003b((&param));
        c_1[0u] = _e105.x;
        c_1[1u] = _e105.y;
        c_1[2u] = _e105.z;
    }
    let _e112 = (*slot);
    if (lightmap_slot == (_e112 + 1i)) {
        let _e115 = c_1;
        param_1 = _e115.xyz;
        let _e117 = wired_boost_legacy_lightmap_u0028_vf3_u003b((&param_1));
        c_1[0u] = _e117.x;
        c_1[1u] = _e117.y;
        c_1[2u] = _e117.z;
    }
    let _e124 = c_1;
    return _e124;
}

fn main_1() {
    var fog: vec4<f32>;
    var frag_color: vec4<f32>;
    var color0_: vec4<f32>;
    var param_2: u32;
    var param_3: vec2<f32>;
    var param_4: i32;
    var base: vec4<f32>;
    var wetness: f32;
    var frost: f32;
    var luminance: f32;
    var fogAmount: f32;

    let _e82 = unnamed.packed_indices[0i][3u];
    let _e88 = unnamed.packed_indices[0i][3u];
    let _e93 = fog_tex_coord_1;
    let _e94 = textureSample(wired_bindless_images[(_e82 & 4095u)], wired_bindless_samplers[((_e88 >> bitcast<u32>(12i)) & 255u)], _e93);
    fog = _e94;
    frag_color[0u] = identity_color;
    frag_color[1u] = identity_color;
    frag_color[2u] = identity_color;
    frag_color[3u] = identity_alpha;
    param_2 = 0u;
    let _e99 = frag_tex_coord0_1;
    param_3 = _e99;
    param_4 = 0i;
    let _e100 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_2), (&param_3), (&param_4));
    let _e101 = frag_color;
    color0_ = (_e100 * _e101);
    let _e103 = color0_;
    base = _e103;
    if override_type_3_ {
        let _e105 = color0_[3u];
        if (_e105 == alpha_test_value) {
            discard;
        }
    } else {
        if override_type_3_1 {
            let _e108 = color0_[3u];
            if (_e108 >= alpha_test_value) {
                discard;
            }
        } else {
            if override_type_3_2 {
                let _e111 = color0_[3u];
                if (_e111 < alpha_test_value) {
                    discard;
                }
            }
        }
    }
    let _e113 = color0_;
    base = _e113;
    if override_type_3_3 {
        let _e116 = unnamed.worldLightParams[1u];
        wetness = clamp(_e116, 0f, 1f);
        let _e120 = unnamed.worldLightParams[2u];
        frost = clamp(_e120, 0f, 1f);
        let _e122 = base;
        luminance = dot(_e122.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e125 = wetness;
        let _e127 = base;
        let _e129 = (_e127.xyz * mix(1f, 0.82f, _e125));
        base[0u] = _e129.x;
        base[1u] = _e129.y;
        base[2u] = _e129.z;
        let _e136 = base;
        let _e138 = luminance;
        let _e140 = luminance;
        let _e142 = luminance;
        let _e144 = frost;
        let _e147 = mix(_e136.xyz, vec3<f32>((_e138 * 0.88f), (_e140 * 0.94f), _e142), vec3((_e144 * 0.55f)));
        base[0u] = _e147.x;
        base[1u] = _e147.y;
        base[2u] = _e147.z;
    }
    let _e154 = color0_;
    let _e157 = unnamed.emissionRadiance;
    let _e160 = base;
    let _e162 = (_e160.xyz + (_e154.xyz * _e157.xyz));
    base[0u] = _e162.x;
    base[1u] = _e162.y;
    base[2u] = _e162.z;
    let _e169 = wired_advanced_fog_enabled_u0028_();
    if _e169 {
        let _e170 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e170;
        if override_type_3_4 {
            let _e171 = fogAmount;
            let _e173 = base;
            let _e175 = (_e173.xyz * (1f - _e171));
            base[0u] = _e175.x;
            base[1u] = _e175.y;
            base[2u] = _e175.z;
        } else {
            if override_type_3_5 {
                let _e182 = fogAmount;
                let _e184 = base;
                base = (_e184 * (1f - _e182));
            } else {
                if override_type_3_6 {
                    let _e186 = fogAmount;
                    let _e189 = base[3u];
                    base[3u] = (_e189 * (1f - _e186));
                } else {
                    let _e192 = base;
                    let _e195 = unnamed.advancedFogColorDensity;
                    let _e197 = fogAmount;
                    let _e199 = mix(_e192.xyz, _e195.xyz, vec3(_e197));
                    base[0u] = _e199.x;
                    base[1u] = _e199.y;
                    base[2u] = _e199.z;
                }
            }
        }
    } else {
        if override_type_3_7 {
            let _e206 = base;
            let _e209 = fog[3u];
            let _e211 = (_e206.xyz * (1f - _e209));
            base[0u] = _e211.x;
            base[1u] = _e211.y;
            base[2u] = _e211.z;
        } else {
            if override_type_3_8 {
                let _e218 = base;
                let _e220 = fog[3u];
                base = (_e218 * (1f - _e220));
            } else {
                if override_type_3_9 {
                    let _e224 = base[3u];
                    let _e226 = fog[3u];
                    base[3u] = (_e224 * (1f - _e226));
                } else {
                    let _e230 = base;
                    let _e231 = fog;
                    let _e233 = unnamed.fogColor;
                    let _e236 = fog[3u];
                    base = mix(_e230, (_e231 * _e233), vec4(_e236));
                }
            }
        }
    }
    if override_type_3_10 {
        let _e240 = base[3u];
        if (_e240 == 0f) {
            discard;
        }
    } else {
        if override_type_3_11 {
            let _e242 = base;
            let _e244 = base;
            if (dot(_e242.xyz, _e244.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e248 = base;
    out_color = _e248;
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
