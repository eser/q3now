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
var<private> frag_tex_coord2_1: vec2<f32>;
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
        let _e115 = unnamed.worldLightParams[0u];
        let _e116 = c_1;
        let _e118 = (_e116.xyz * _e115);
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
    var param_1: vec3<f32>;
    var color0_: vec4<f32>;
    var param_2: u32;
    var param_3: vec2<f32>;
    var param_4: i32;
    var color1_: vec4<f32>;
    var param_5: u32;
    var param_6: vec2<f32>;
    var param_7: i32;
    var color2_: vec4<f32>;
    var param_8: u32;
    var param_9: vec2<f32>;
    var param_10: i32;
    var base: vec4<f32>;
    var color1_1: vec4<f32>;
    var param_11: u32;
    var param_12: vec2<f32>;
    var param_13: i32;
    var color2_1: vec4<f32>;
    var param_14: u32;
    var param_15: vec2<f32>;
    var param_16: i32;
    var color1_2: vec4<f32>;
    var param_17: u32;
    var param_18: vec2<f32>;
    var param_19: i32;
    var color2_2: vec4<f32>;
    var param_20: u32;
    var param_21: vec2<f32>;
    var param_22: i32;
    var wetness: f32;
    var frost: f32;
    var luminance: f32;
    var fogAmount: f32;

    let _e106 = unnamed.packed_indices[0i][3u];
    let _e112 = unnamed.packed_indices[0i][3u];
    let _e117 = fog_tex_coord_1;
    let _e118 = textureSample(wired_bindless_images[(_e106 & 4095u)], wired_bindless_samplers[((_e112 >> bitcast<u32>(12i)) & 255u)], _e117);
    fog = _e118;
    let _e119 = frag_color0In_1;
    param_1 = _e119.xyz;
    let _e121 = sRGBToLinear_u0028_vf3_u003b((&param_1));
    let _e123 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e121.x, _e121.y, _e121.z, _e123);
    param_2 = 0u;
    let _e128 = frag_tex_coord0_1;
    param_3 = _e128;
    param_4 = 0i;
    let _e129 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_2), (&param_3), (&param_4));
    let _e130 = frag_color0_;
    color0_ = (_e129 * _e130);
    if override_type_3_ {
        param_5 = 1u;
        let _e132 = frag_tex_coord1_1;
        param_6 = _e132;
        param_7 = 1i;
        let _e133 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_5), (&param_6), (&param_7));
        color1_ = _e133;
        param_8 = 2u;
        let _e134 = frag_tex_coord2_1;
        param_9 = _e134;
        param_10 = 2i;
        let _e135 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_8), (&param_9), (&param_10));
        color2_ = _e135;
        let _e136 = color0_;
        let _e138 = color1_;
        let _e141 = color2_;
        let _e143 = ((_e136.xyz + _e138.xyz) + _e141.xyz);
        let _e145 = color0_[3u];
        let _e147 = color1_[3u];
        let _e150 = color2_[3u];
        base = vec4<f32>(_e143.x, _e143.y, _e143.z, ((_e145 * _e147) * _e150));
    } else {
        if override_type_3_1 {
            param_11 = 1u;
            let _e156 = frag_tex_coord1_1;
            param_12 = _e156;
            param_13 = 1i;
            let _e157 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_11), (&param_12), (&param_13));
            let _e158 = frag_color0_;
            color1_1 = (_e157 * _e158);
            param_14 = 2u;
            let _e160 = frag_tex_coord2_1;
            param_15 = _e160;
            param_16 = 2i;
            let _e161 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_14), (&param_15), (&param_16));
            let _e162 = frag_color0_;
            color2_1 = (_e161 * _e162);
            let _e164 = color0_;
            let _e166 = color1_1;
            let _e169 = color2_1;
            let _e171 = ((_e164.xyz + _e166.xyz) + _e169.xyz);
            let _e173 = color0_[3u];
            let _e175 = color1_1[3u];
            let _e178 = color2_1[3u];
            base = vec4<f32>(_e171.x, _e171.y, _e171.z, ((_e173 * _e175) * _e178));
        } else {
            param_17 = 1u;
            let _e184 = frag_tex_coord1_1;
            param_18 = _e184;
            param_19 = 1i;
            let _e185 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_17), (&param_18), (&param_19));
            color1_2 = _e185;
            param_20 = 2u;
            let _e186 = frag_tex_coord2_1;
            param_21 = _e186;
            param_22 = 2i;
            let _e187 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_20), (&param_21), (&param_22));
            color2_2 = _e187;
            let _e188 = color0_;
            let _e190 = color1_2;
            let _e193 = color2_2;
            let _e195 = ((_e188.xyz * _e190.xyz) * _e193.xyz);
            base[0u] = _e195.x;
            base[1u] = _e195.y;
            base[2u] = _e195.z;
            let _e203 = color0_[3u];
            let _e205 = color1_2[3u];
            let _e208 = color2_2[3u];
            base[3u] = ((_e203 * _e205) * _e208);
        }
    }
    if override_type_3_2 {
        let _e213 = unnamed.worldLightParams[1u];
        wetness = clamp(_e213, 0f, 1f);
        let _e217 = unnamed.worldLightParams[2u];
        frost = clamp(_e217, 0f, 1f);
        let _e219 = base;
        luminance = dot(_e219.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e222 = wetness;
        let _e224 = base;
        let _e226 = (_e224.xyz * mix(1f, 0.82f, _e222));
        base[0u] = _e226.x;
        base[1u] = _e226.y;
        base[2u] = _e226.z;
        let _e233 = base;
        let _e235 = luminance;
        let _e237 = luminance;
        let _e239 = luminance;
        let _e241 = frost;
        let _e244 = mix(_e233.xyz, vec3<f32>((_e235 * 0.88f), (_e237 * 0.94f), _e239), vec3((_e241 * 0.55f)));
        base[0u] = _e244.x;
        base[1u] = _e244.y;
        base[2u] = _e244.z;
    }
    let _e251 = color0_;
    let _e254 = unnamed.emissionRadiance;
    let _e257 = base;
    let _e259 = (_e257.xyz + (_e251.xyz * _e254.xyz));
    base[0u] = _e259.x;
    base[1u] = _e259.y;
    base[2u] = _e259.z;
    let _e266 = wired_advanced_fog_enabled_u0028_();
    if _e266 {
        let _e267 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e267;
        if override_type_3_3 {
            let _e268 = fogAmount;
            let _e270 = base;
            let _e272 = (_e270.xyz * (1f - _e268));
            base[0u] = _e272.x;
            base[1u] = _e272.y;
            base[2u] = _e272.z;
        } else {
            if override_type_3_4 {
                let _e279 = fogAmount;
                let _e281 = base;
                base = (_e281 * (1f - _e279));
            } else {
                if override_type_3_5 {
                    let _e283 = fogAmount;
                    let _e286 = base[3u];
                    base[3u] = (_e286 * (1f - _e283));
                } else {
                    let _e289 = base;
                    let _e292 = unnamed.advancedFogColorDensity;
                    let _e294 = fogAmount;
                    let _e296 = mix(_e289.xyz, _e292.xyz, vec3(_e294));
                    base[0u] = _e296.x;
                    base[1u] = _e296.y;
                    base[2u] = _e296.z;
                }
            }
        }
    } else {
        if override_type_3_6 {
            let _e303 = base;
            let _e306 = fog[3u];
            let _e308 = (_e303.xyz * (1f - _e306));
            base[0u] = _e308.x;
            base[1u] = _e308.y;
            base[2u] = _e308.z;
        } else {
            if override_type_3_7 {
                let _e315 = base;
                let _e317 = fog[3u];
                base = (_e315 * (1f - _e317));
            } else {
                if override_type_3_8 {
                    let _e321 = base[3u];
                    let _e323 = fog[3u];
                    base[3u] = (_e321 * (1f - _e323));
                } else {
                    let _e327 = base;
                    let _e328 = fog;
                    let _e330 = unnamed.fogColor;
                    let _e333 = fog[3u];
                    base = mix(_e327, (_e328 * _e330), vec4(_e333));
                }
            }
        }
    }
    if override_type_3_9 {
        let _e337 = base[3u];
        if (_e337 == 0f) {
            discard;
        }
    } else {
        if override_type_3_10 {
            let _e339 = base;
            let _e341 = base;
            if (dot(_e339.xyz, _e341.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e345 = base;
    out_color = _e345;
    return;
}

@fragment
fn main(@builtin(position) gl_FragCoord: vec4<f32>, @location(4) fog_tex_coord: vec2<f32>, @location(0) frag_color0In: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>, @location(3) frag_tex_coord2_: vec2<f32>) -> @location(0) vec4<f32> {
    gl_FragCoord_1 = gl_FragCoord;
    fog_tex_coord_1 = fog_tex_coord;
    frag_color0In_1 = frag_color0In;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    frag_tex_coord2_1 = frag_tex_coord2_;
    main_1();
    let _e13 = out_color;
    return _e13;
}
