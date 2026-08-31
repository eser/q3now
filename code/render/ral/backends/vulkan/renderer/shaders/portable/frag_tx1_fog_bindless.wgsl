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
var<private> frag_color0In_1: vec4<f32>;
var<private> frag_tex_coord0_1: vec2<f32>;
var<private> frag_tex_coord1_1: vec2<f32>;
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
    var frag_color0_: vec4<f32>;
    var param_2: vec3<f32>;
    var color0_: vec4<f32>;
    var param_3: u32;
    var param_4: vec2<f32>;
    var param_5: i32;
    var color1_: vec4<f32>;
    var param_6: u32;
    var param_7: vec2<f32>;
    var param_8: i32;
    var base: vec4<f32>;
    var color1_1: vec4<f32>;
    var param_9: u32;
    var param_10: vec2<f32>;
    var param_11: i32;
    var color1_2: vec4<f32>;
    var param_12: u32;
    var param_13: vec2<f32>;
    var param_14: i32;
    var wetness: f32;
    var frost: f32;
    var luminance: f32;
    var fogAmount: f32;

    let _e93 = unnamed.packed_indices[0i][3u];
    let _e99 = unnamed.packed_indices[0i][3u];
    let _e104 = fog_tex_coord_1;
    let _e105 = textureSample(wired_bindless_images[(_e93 & 4095u)], wired_bindless_samplers[((_e99 >> bitcast<u32>(12i)) & 255u)], _e104);
    fog = _e105;
    let _e106 = frag_color0In_1;
    param_2 = _e106.xyz;
    let _e108 = sRGBToLinear_u0028_vf3_u003b((&param_2));
    let _e110 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e108.x, _e108.y, _e108.z, _e110);
    param_3 = 0u;
    let _e115 = frag_tex_coord0_1;
    param_4 = _e115;
    param_5 = 0i;
    let _e116 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_3), (&param_4), (&param_5));
    let _e117 = frag_color0_;
    color0_ = (_e116 * _e117);
    if override_type_3_ {
        param_6 = 1u;
        let _e119 = frag_tex_coord1_1;
        param_7 = _e119;
        param_8 = 1i;
        let _e120 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_6), (&param_7), (&param_8));
        color1_ = _e120;
        let _e121 = color0_;
        let _e123 = color1_;
        let _e125 = (_e121.xyz + _e123.xyz);
        let _e127 = color0_[3u];
        let _e129 = color1_[3u];
        base = vec4<f32>(_e125.x, _e125.y, _e125.z, (_e127 * _e129));
    } else {
        if override_type_3_1 {
            param_9 = 1u;
            let _e135 = frag_tex_coord1_1;
            param_10 = _e135;
            param_11 = 1i;
            let _e136 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_9), (&param_10), (&param_11));
            let _e137 = frag_color0_;
            color1_1 = (_e136 * _e137);
            let _e139 = color0_;
            let _e141 = color1_1;
            let _e143 = (_e139.xyz + _e141.xyz);
            let _e145 = color0_[3u];
            let _e147 = color1_1[3u];
            base = vec4<f32>(_e143.x, _e143.y, _e143.z, (_e145 * _e147));
        } else {
            param_12 = 1u;
            let _e153 = frag_tex_coord1_1;
            param_13 = _e153;
            param_14 = 1i;
            let _e154 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_12), (&param_13), (&param_14));
            color1_2 = _e154;
            let _e155 = color0_;
            let _e157 = color1_2;
            let _e159 = (_e155.xyz * _e157.xyz);
            base[0u] = _e159.x;
            base[1u] = _e159.y;
            base[2u] = _e159.z;
            let _e167 = color0_[3u];
            let _e169 = color1_2[3u];
            base[3u] = (_e167 * _e169);
        }
    }
    if override_type_3_2 {
        let _e174 = unnamed.worldLightParams[1u];
        wetness = clamp(_e174, 0f, 1f);
        let _e178 = unnamed.worldLightParams[2u];
        frost = clamp(_e178, 0f, 1f);
        let _e180 = base;
        luminance = dot(_e180.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e183 = wetness;
        let _e185 = base;
        let _e187 = (_e185.xyz * mix(1f, 0.82f, _e183));
        base[0u] = _e187.x;
        base[1u] = _e187.y;
        base[2u] = _e187.z;
        let _e194 = base;
        let _e196 = luminance;
        let _e198 = luminance;
        let _e200 = luminance;
        let _e202 = frost;
        let _e205 = mix(_e194.xyz, vec3<f32>((_e196 * 0.88f), (_e198 * 0.94f), _e200), vec3((_e202 * 0.55f)));
        base[0u] = _e205.x;
        base[1u] = _e205.y;
        base[2u] = _e205.z;
    }
    let _e212 = color0_;
    let _e215 = unnamed.emissionRadiance;
    let _e218 = base;
    let _e220 = (_e218.xyz + (_e212.xyz * _e215.xyz));
    base[0u] = _e220.x;
    base[1u] = _e220.y;
    base[2u] = _e220.z;
    let _e227 = wired_advanced_fog_enabled_u0028_();
    if _e227 {
        let _e228 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e228;
        if override_type_3_3 {
            let _e229 = fogAmount;
            let _e231 = base;
            let _e233 = (_e231.xyz * (1f - _e229));
            base[0u] = _e233.x;
            base[1u] = _e233.y;
            base[2u] = _e233.z;
        } else {
            if override_type_3_4 {
                let _e240 = fogAmount;
                let _e242 = base;
                base = (_e242 * (1f - _e240));
            } else {
                if override_type_3_5 {
                    let _e244 = fogAmount;
                    let _e247 = base[3u];
                    base[3u] = (_e247 * (1f - _e244));
                } else {
                    let _e250 = base;
                    let _e253 = unnamed.advancedFogColorDensity;
                    let _e255 = fogAmount;
                    let _e257 = mix(_e250.xyz, _e253.xyz, vec3(_e255));
                    base[0u] = _e257.x;
                    base[1u] = _e257.y;
                    base[2u] = _e257.z;
                }
            }
        }
    } else {
        if override_type_3_6 {
            let _e264 = base;
            let _e267 = fog[3u];
            let _e269 = (_e264.xyz * (1f - _e267));
            base[0u] = _e269.x;
            base[1u] = _e269.y;
            base[2u] = _e269.z;
        } else {
            if override_type_3_7 {
                let _e276 = base;
                let _e278 = fog[3u];
                base = (_e276 * (1f - _e278));
            } else {
                if override_type_3_8 {
                    let _e282 = base[3u];
                    let _e284 = fog[3u];
                    base[3u] = (_e282 * (1f - _e284));
                } else {
                    let _e288 = base;
                    let _e289 = fog;
                    let _e291 = unnamed.fogColor;
                    let _e294 = fog[3u];
                    base = mix(_e288, (_e289 * _e291), vec4(_e294));
                }
            }
        }
    }
    if override_type_3_9 {
        let _e298 = base[3u];
        if (_e298 == 0f) {
            discard;
        }
    } else {
        if override_type_3_10 {
            let _e300 = base;
            let _e302 = base;
            if (dot(_e300.xyz, _e302.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e306 = base;
    out_color = _e306;
    return;
}

@fragment
fn main(@builtin(position) gl_FragCoord: vec4<f32>, @location(4) fog_tex_coord: vec2<f32>, @location(0) frag_color0In: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>) -> @location(0) vec4<f32> {
    gl_FragCoord_1 = gl_FragCoord;
    fog_tex_coord_1 = fog_tex_coord;
    frag_color0In_1 = frag_color0In;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    main_1();
    let _e11 = out_color;
    return _e11;
}
