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

    let _e70 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e70 + 0.5f));
    let _e75 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e77 = fogType;
    let _e80 = fogType;
    return (((_e75 > 0.5f) && (_e77 >= 1i)) && (_e80 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e70 = wired_advanced_fog_enabled_u0028_();
    if !(_e70) {
        return 0f;
    }
    let _e73 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e73, 0.000001f));
    let _e78 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e78 + 0.5f));
    let _e81 = fogType_1;
    if (_e81 == 1i) {
        let _e85 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e85 <= 0f) {
            return 0f;
        }
        let _e87 = viewDepth;
        let _e90 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e87 / _e90), 0f, 1f);
    }
    let _e95 = unnamed.advancedFogColorDensity[3u];
    let _e97 = viewDepth;
    opticalDepth = (max(_e95, 0f) * _e97);
    let _e99 = fogType_1;
    if (_e99 == 2i) {
        let _e101 = opticalDepth;
        return clamp((1f - exp(-(_e101))), 0f, 1f);
    }
    let _e106 = opticalDepth;
    let _e107 = opticalDepth;
    return clamp((1f - exp(-((_e106 * _e107)))), 0f, 1f);
}

fn wired_boost_legacy_lightmap_u0028_vf3_u003b(rgb: ptr<function, vec3<f32>>) -> vec3<f32> {
    var boosted: vec3<f32>;
    var peak: f32;

    let _e70 = (*rgb);
    let _e73 = unnamed.worldLightParams[0u];
    boosted = (_e70 * _e73);
    let _e76 = boosted[0u];
    let _e78 = boosted[1u];
    let _e80 = boosted[2u];
    peak = max(_e76, max(_e78, _e80));
    let _e83 = peak;
    if (_e83 > 1f) {
        let _e85 = peak;
        let _e86 = boosted;
        boosted = (_e86 / vec3(_e85));
    }
    let _e89 = boosted;
    return _e89;
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e71 = (*c);
    (*c) = max(_e71, vec3<f32>(0f, 0f, 0f));
    let _e73 = (*c);
    cutoff = (_e73 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e75 = (*c);
    lo = (_e75 / vec3(12.92f));
    let _e78 = (*c);
    hi = pow(((_e78 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e83 = hi;
    let _e84 = lo;
    let _e85 = cutoff;
    return mix(_e83, _e84, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e85));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param: vec3<f32>;
    var param_1: vec3<f32>;

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
        let _e114 = c_1;
        param_1 = _e114.xyz;
        let _e116 = wired_boost_legacy_lightmap_u0028_vf3_u003b((&param_1));
        c_1[0u] = _e116.x;
        c_1[1u] = _e116.y;
        c_1[2u] = _e116.z;
    }
    let _e123 = c_1;
    return _e123;
}

fn main_1() {
    var fog: vec4<f32>;
    var frag_color: vec4<f32>;
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

    let _e93 = unnamed.packed_indices[0i][3u];
    let _e99 = unnamed.packed_indices[0i][3u];
    let _e104 = fog_tex_coord_1;
    let _e105 = textureSample(wired_bindless_images[(_e93 & 4095u)], wired_bindless_samplers[((_e99 >> bitcast<u32>(12i)) & 255u)], _e104);
    fog = _e105;
    frag_color[0u] = identity_color;
    frag_color[1u] = identity_color;
    frag_color[2u] = identity_color;
    frag_color[3u] = identity_alpha;
    param_2 = 0u;
    let _e110 = frag_tex_coord0_1;
    param_3 = _e110;
    param_4 = 0i;
    let _e111 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_2), (&param_3), (&param_4));
    let _e112 = frag_color;
    color0_ = (_e111 * _e112);
    if override_type_3_ {
        param_5 = 1u;
        let _e114 = frag_tex_coord1_1;
        param_6 = _e114;
        param_7 = 1i;
        let _e115 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_5), (&param_6), (&param_7));
        color1_ = _e115;
        let _e116 = color0_;
        let _e118 = color1_;
        let _e120 = (_e116.xyz + _e118.xyz);
        let _e122 = color0_[3u];
        let _e124 = color1_[3u];
        base = vec4<f32>(_e120.x, _e120.y, _e120.z, (_e122 * _e124));
    } else {
        if override_type_3_1 {
            param_8 = 1u;
            let _e130 = frag_tex_coord1_1;
            param_9 = _e130;
            param_10 = 1i;
            let _e131 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_8), (&param_9), (&param_10));
            let _e132 = frag_color;
            color1_1 = (_e131 * _e132);
            let _e134 = color0_;
            let _e136 = color1_1;
            let _e138 = (_e134.xyz + _e136.xyz);
            let _e140 = color0_[3u];
            let _e142 = color1_1[3u];
            base = vec4<f32>(_e138.x, _e138.y, _e138.z, (_e140 * _e142));
        } else {
            param_11 = 1u;
            let _e148 = frag_tex_coord1_1;
            param_12 = _e148;
            param_13 = 1i;
            let _e149 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_11), (&param_12), (&param_13));
            let _e150 = frag_color;
            color1_2 = (_e149 * _e150);
            let _e152 = color0_;
            let _e154 = color1_2;
            let _e156 = (_e152.xyz * _e154.xyz);
            base[0u] = _e156.x;
            base[1u] = _e156.y;
            base[2u] = _e156.z;
            let _e164 = color0_[3u];
            let _e166 = color1_2[3u];
            base[3u] = (_e164 * _e166);
        }
    }
    if override_type_3_2 {
        let _e171 = unnamed.worldLightParams[1u];
        wetness = clamp(_e171, 0f, 1f);
        let _e175 = unnamed.worldLightParams[2u];
        frost = clamp(_e175, 0f, 1f);
        let _e177 = base;
        luminance = dot(_e177.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e180 = wetness;
        let _e182 = base;
        let _e184 = (_e182.xyz * mix(1f, 0.82f, _e180));
        base[0u] = _e184.x;
        base[1u] = _e184.y;
        base[2u] = _e184.z;
        let _e191 = base;
        let _e193 = luminance;
        let _e195 = luminance;
        let _e197 = luminance;
        let _e199 = frost;
        let _e202 = mix(_e191.xyz, vec3<f32>((_e193 * 0.88f), (_e195 * 0.94f), _e197), vec3((_e199 * 0.55f)));
        base[0u] = _e202.x;
        base[1u] = _e202.y;
        base[2u] = _e202.z;
    }
    let _e209 = color0_;
    let _e212 = unnamed.emissionRadiance;
    let _e215 = base;
    let _e217 = (_e215.xyz + (_e209.xyz * _e212.xyz));
    base[0u] = _e217.x;
    base[1u] = _e217.y;
    base[2u] = _e217.z;
    let _e224 = wired_advanced_fog_enabled_u0028_();
    if _e224 {
        let _e225 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e225;
        if override_type_3_3 {
            let _e226 = fogAmount;
            let _e228 = base;
            let _e230 = (_e228.xyz * (1f - _e226));
            base[0u] = _e230.x;
            base[1u] = _e230.y;
            base[2u] = _e230.z;
        } else {
            if override_type_3_4 {
                let _e237 = fogAmount;
                let _e239 = base;
                base = (_e239 * (1f - _e237));
            } else {
                if override_type_3_5 {
                    let _e241 = fogAmount;
                    let _e244 = base[3u];
                    base[3u] = (_e244 * (1f - _e241));
                } else {
                    let _e247 = base;
                    let _e250 = unnamed.advancedFogColorDensity;
                    let _e252 = fogAmount;
                    let _e254 = mix(_e247.xyz, _e250.xyz, vec3(_e252));
                    base[0u] = _e254.x;
                    base[1u] = _e254.y;
                    base[2u] = _e254.z;
                }
            }
        }
    } else {
        if override_type_3_6 {
            let _e261 = base;
            let _e264 = fog[3u];
            let _e266 = (_e261.xyz * (1f - _e264));
            base[0u] = _e266.x;
            base[1u] = _e266.y;
            base[2u] = _e266.z;
        } else {
            if override_type_3_7 {
                let _e273 = base;
                let _e275 = fog[3u];
                base = (_e273 * (1f - _e275));
            } else {
                if override_type_3_8 {
                    let _e279 = base[3u];
                    let _e281 = fog[3u];
                    base[3u] = (_e279 * (1f - _e281));
                } else {
                    let _e285 = base;
                    let _e286 = fog;
                    let _e288 = unnamed.fogColor;
                    let _e291 = fog[3u];
                    base = mix(_e285, (_e286 * _e288), vec4(_e291));
                }
            }
        }
    }
    if override_type_3_9 {
        let _e295 = base[3u];
        if (_e295 == 0f) {
            discard;
        }
    } else {
        if override_type_3_10 {
            let _e297 = base;
            let _e299 = base;
            if (dot(_e297.xyz, _e299.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e303 = base;
    out_color = _e303;
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
