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
@id(6) override tex_mode: i32 = 0i;
override override_type_3_: bool = (tex_mode == 1i);
override override_type_3_1: bool = (tex_mode == 2i);
override override_type_3_2: bool = (lightmap_slot != 0i);
@id(10) override acff: i32 = 0i;
override override_type_3_3: bool = (acff == 1i);
override override_type_3_4: bool = (acff == 2i);
override override_type_3_5: bool = (acff == 3i);
override override_type_3_6: bool = (acff == 1i);
override override_type_3_7: bool = (acff == 2i);
override override_type_3_8: bool = (acff == 3i);
@id(7) override discard_mode: i32 = 0i;
override override_type_3_9: bool = (discard_mode == 1i);
override override_type_3_10: bool = (discard_mode == 2i);
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
var<private> frag_tex_coord1_1: vec2<f32>;
var<private> out_color: vec4<f32>;

fn wired_advanced_fog_enabled_u0028_() -> bool {
    var fogType: i32;

    let _e68 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e68 + 0.5f));
    let _e73 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e75 = fogType;
    let _e78 = fogType;
    return (((_e73 > 0.5f) && (_e75 >= 1i)) && (_e78 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e68 = wired_advanced_fog_enabled_u0028_();
    if !(_e68) {
        return 0f;
    }
    let _e71 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e71, 0.000001f));
    let _e76 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e76 + 0.5f));
    let _e79 = fogType_1;
    if (_e79 == 1i) {
        let _e83 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e83 <= 0f) {
            return 0f;
        }
        let _e85 = viewDepth;
        let _e88 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e85 / _e88), 0f, 1f);
    }
    let _e93 = unnamed.advancedFogColorDensity[3u];
    let _e95 = viewDepth;
    opticalDepth = (max(_e93, 0f) * _e95);
    let _e97 = fogType_1;
    if (_e97 == 2i) {
        let _e99 = opticalDepth;
        return clamp((1f - exp(-(_e99))), 0f, 1f);
    }
    let _e104 = opticalDepth;
    let _e105 = opticalDepth;
    return clamp((1f - exp(-((_e104 * _e105)))), 0f, 1f);
}

fn wired_boost_legacy_lightmap_u0028_vf3_u003b(rgb: ptr<function, vec3<f32>>) -> vec3<f32> {
    var boosted: vec3<f32>;
    var peak: f32;

    let _e68 = (*rgb);
    let _e71 = unnamed.worldLightParams[0u];
    boosted = (_e68 * _e71);
    let _e74 = boosted[0u];
    let _e76 = boosted[1u];
    let _e78 = boosted[2u];
    peak = max(_e74, max(_e76, _e78));
    let _e81 = peak;
    if (_e81 > 1f) {
        let _e83 = peak;
        let _e84 = boosted;
        boosted = (_e84 / vec3(_e83));
    }
    let _e87 = boosted;
    return _e87;
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e69 = (*c);
    (*c) = max(_e69, vec3<f32>(0f, 0f, 0f));
    let _e71 = (*c);
    cutoff = (_e71 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e73 = (*c);
    lo = (_e73 / vec3(12.92f));
    let _e76 = (*c);
    hi = pow(((_e76 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e81 = hi;
    let _e82 = lo;
    let _e83 = cutoff;
    return mix(_e81, _e82, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e83));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param: vec3<f32>;
    var param_1: vec3<f32>;

    let _e71 = (*role);
    let _e73 = (*role);
    let _e78 = unnamed.packed_indices[(_e71 / 4u)][(_e73 % 4u)];
    let _e81 = (*role);
    let _e83 = (*role);
    let _e88 = unnamed.packed_indices[(_e81 / 4u)][(_e83 % 4u)];
    let _e93 = (*uv);
    let _e94 = textureSample(wired_bindless_images[(_e78 & 4095u)], wired_bindless_samplers[((_e88 >> bitcast<u32>(12i)) & 255u)], _e93);
    c_1 = _e94;
    let _e95 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e95))) == 0i) {
        let _e100 = c_1;
        param = _e100.xyz;
        let _e102 = sRGBToLinear_u0028_vf3_u003b((&param));
        c_1[0u] = _e102.x;
        c_1[1u] = _e102.y;
        c_1[2u] = _e102.z;
    }
    let _e109 = (*slot);
    if (lightmap_slot == (_e109 + 1i)) {
        let _e112 = c_1;
        param_1 = _e112.xyz;
        let _e114 = wired_boost_legacy_lightmap_u0028_vf3_u003b((&param_1));
        c_1[0u] = _e114.x;
        c_1[1u] = _e114.y;
        c_1[2u] = _e114.z;
    }
    let _e121 = c_1;
    return _e121;
}

fn main_1() {
    var fog: vec4<f32>;
    var color0_: vec4<f32>;
    var param_2: u32;
    var param_3: vec2<f32>;
    var param_4: i32;
    var color1_: vec4<f32>;
    var param_5: u32;
    var param_6: vec2<f32>;
    var param_7: i32;
    var base: vec4<f32>;
    var color1_1: vec4<f32>;
    var param_8: u32;
    var param_9: vec2<f32>;
    var param_10: i32;
    var color1_2: vec4<f32>;
    var param_11: u32;
    var param_12: vec2<f32>;
    var param_13: i32;
    var wetness: f32;
    var frost: f32;
    var luminance: f32;
    var fogAmount: f32;

    let _e90 = unnamed.packed_indices[0i][3u];
    let _e96 = unnamed.packed_indices[0i][3u];
    let _e101 = fog_tex_coord_1;
    let _e102 = textureSample(wired_bindless_images[(_e90 & 4095u)], wired_bindless_samplers[((_e96 >> bitcast<u32>(12i)) & 255u)], _e101);
    fog = _e102;
    param_2 = 0u;
    let _e103 = frag_tex_coord0_1;
    param_3 = _e103;
    param_4 = 0i;
    let _e104 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_2), (&param_3), (&param_4));
    color0_ = _e104;
    if override_type_3_ {
        param_5 = 1u;
        let _e105 = frag_tex_coord1_1;
        param_6 = _e105;
        param_7 = 1i;
        let _e106 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_5), (&param_6), (&param_7));
        color1_ = _e106;
        let _e107 = color0_;
        let _e109 = color1_;
        let _e111 = (_e107.xyz + _e109.xyz);
        let _e113 = color0_[3u];
        let _e115 = color1_[3u];
        base = vec4<f32>(_e111.x, _e111.y, _e111.z, (_e113 * _e115));
    } else {
        if override_type_3_1 {
            param_8 = 1u;
            let _e121 = frag_tex_coord1_1;
            param_9 = _e121;
            param_10 = 1i;
            let _e122 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_8), (&param_9), (&param_10));
            color1_1 = _e122;
            let _e123 = color0_;
            let _e125 = color1_1;
            let _e127 = (_e123.xyz + _e125.xyz);
            let _e129 = color0_[3u];
            let _e131 = color1_1[3u];
            base = vec4<f32>(_e127.x, _e127.y, _e127.z, (_e129 * _e131));
        } else {
            param_11 = 1u;
            let _e137 = frag_tex_coord1_1;
            param_12 = _e137;
            param_13 = 1i;
            let _e138 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_11), (&param_12), (&param_13));
            color1_2 = _e138;
            let _e139 = color0_;
            let _e141 = color1_2;
            let _e143 = (_e139.xyz * _e141.xyz);
            base[0u] = _e143.x;
            base[1u] = _e143.y;
            base[2u] = _e143.z;
            let _e151 = color0_[3u];
            let _e153 = color1_2[3u];
            base[3u] = (_e151 * _e153);
        }
    }
    if override_type_3_2 {
        let _e158 = unnamed.worldLightParams[1u];
        wetness = clamp(_e158, 0f, 1f);
        let _e162 = unnamed.worldLightParams[2u];
        frost = clamp(_e162, 0f, 1f);
        let _e164 = base;
        luminance = dot(_e164.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e167 = wetness;
        let _e169 = base;
        let _e171 = (_e169.xyz * mix(1f, 0.82f, _e167));
        base[0u] = _e171.x;
        base[1u] = _e171.y;
        base[2u] = _e171.z;
        let _e178 = base;
        let _e180 = luminance;
        let _e182 = luminance;
        let _e184 = luminance;
        let _e186 = frost;
        let _e189 = mix(_e178.xyz, vec3<f32>((_e180 * 0.88f), (_e182 * 0.94f), _e184), vec3((_e186 * 0.55f)));
        base[0u] = _e189.x;
        base[1u] = _e189.y;
        base[2u] = _e189.z;
    }
    let _e196 = color0_;
    let _e199 = unnamed.emissionRadiance;
    let _e202 = base;
    let _e204 = (_e202.xyz + (_e196.xyz * _e199.xyz));
    base[0u] = _e204.x;
    base[1u] = _e204.y;
    base[2u] = _e204.z;
    let _e211 = wired_advanced_fog_enabled_u0028_();
    if _e211 {
        let _e212 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e212;
        if override_type_3_3 {
            let _e213 = fogAmount;
            let _e215 = base;
            let _e217 = (_e215.xyz * (1f - _e213));
            base[0u] = _e217.x;
            base[1u] = _e217.y;
            base[2u] = _e217.z;
        } else {
            if override_type_3_4 {
                let _e224 = fogAmount;
                let _e226 = base;
                base = (_e226 * (1f - _e224));
            } else {
                if override_type_3_5 {
                    let _e228 = fogAmount;
                    let _e231 = base[3u];
                    base[3u] = (_e231 * (1f - _e228));
                } else {
                    let _e234 = base;
                    let _e237 = unnamed.advancedFogColorDensity;
                    let _e239 = fogAmount;
                    let _e241 = mix(_e234.xyz, _e237.xyz, vec3(_e239));
                    base[0u] = _e241.x;
                    base[1u] = _e241.y;
                    base[2u] = _e241.z;
                }
            }
        }
    } else {
        if override_type_3_6 {
            let _e248 = base;
            let _e251 = fog[3u];
            let _e253 = (_e248.xyz * (1f - _e251));
            base[0u] = _e253.x;
            base[1u] = _e253.y;
            base[2u] = _e253.z;
        } else {
            if override_type_3_7 {
                let _e260 = base;
                let _e262 = fog[3u];
                base = (_e260 * (1f - _e262));
            } else {
                if override_type_3_8 {
                    let _e266 = base[3u];
                    let _e268 = fog[3u];
                    base[3u] = (_e266 * (1f - _e268));
                } else {
                    let _e272 = base;
                    let _e273 = fog;
                    let _e275 = unnamed.fogColor;
                    let _e278 = fog[3u];
                    base = mix(_e272, (_e273 * _e275), vec4(_e278));
                }
            }
        }
    }
    if override_type_3_9 {
        let _e282 = base[3u];
        if (_e282 == 0f) {
            discard;
        }
    } else {
        if override_type_3_10 {
            let _e284 = base;
            let _e286 = base;
            if (dot(_e284.xyz, _e286.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e290 = base;
    out_color = _e290;
    return;
}

@fragment
fn main(@builtin(position) gl_FragCoord: vec4<f32>, @location(4) fog_tex_coord: vec2<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>) -> @location(0) vec4<f32> {
    gl_FragCoord_1 = gl_FragCoord;
    fog_tex_coord_1 = fog_tex_coord;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    main_1();
    let _e9 = out_color;
    return _e9;
}
