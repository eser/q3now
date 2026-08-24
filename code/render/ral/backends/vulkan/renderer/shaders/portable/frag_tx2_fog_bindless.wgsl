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
}

@id(4) override tex_domain: i32 = 0i;
@id(26) override lightmap_slot: i32 = 0i;
@id(6) override tex_mode: i32 = 0i;
override override_type_3_: bool = (tex_mode == 1i);
override override_type_3_1: bool = (tex_mode == 2i);
@id(10) override acff: i32 = 0i;
override override_type_3_2: bool = (acff == 1i);
override override_type_3_3: bool = (acff == 2i);
override override_type_3_4: bool = (acff == 3i);
override override_type_3_5: bool = (acff == 1i);
override override_type_3_6: bool = (acff == 2i);
override override_type_3_7: bool = (acff == 3i);
@id(7) override discard_mode: i32 = 0i;
override override_type_3_8: bool = (discard_mode == 1i);
override override_type_3_9: bool = (discard_mode == 2i);
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

    let _e60 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e60 + 0.5f));
    let _e65 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e67 = fogType;
    let _e70 = fogType;
    return (((_e65 > 0.5f) && (_e67 >= 1i)) && (_e70 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e60 = wired_advanced_fog_enabled_u0028_();
    if !(_e60) {
        return 0f;
    }
    let _e63 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e63, 0.000001f));
    let _e68 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e68 + 0.5f));
    let _e71 = fogType_1;
    if (_e71 == 1i) {
        let _e75 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e75 <= 0f) {
            return 0f;
        }
        let _e77 = viewDepth;
        let _e80 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e77 / _e80), 0f, 1f);
    }
    let _e85 = unnamed.advancedFogColorDensity[3u];
    let _e87 = viewDepth;
    opticalDepth = (max(_e85, 0f) * _e87);
    let _e89 = fogType_1;
    if (_e89 == 2i) {
        let _e91 = opticalDepth;
        return clamp((1f - exp(-(_e91))), 0f, 1f);
    }
    let _e96 = opticalDepth;
    let _e97 = opticalDepth;
    return clamp((1f - exp(-((_e96 * _e97)))), 0f, 1f);
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e61 = (*c);
    (*c) = max(_e61, vec3<f32>(0f, 0f, 0f));
    let _e63 = (*c);
    cutoff = (_e63 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e65 = (*c);
    lo = (_e65 / vec3(12.92f));
    let _e68 = (*c);
    hi = pow(((_e68 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e73 = hi;
    let _e74 = lo;
    let _e75 = cutoff;
    return mix(_e73, _e74, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e75));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param: vec3<f32>;

    let _e62 = (*role);
    let _e64 = (*role);
    let _e69 = unnamed.packed_indices[(_e62 / 4u)][(_e64 % 4u)];
    let _e72 = (*role);
    let _e74 = (*role);
    let _e79 = unnamed.packed_indices[(_e72 / 4u)][(_e74 % 4u)];
    let _e84 = (*uv);
    let _e85 = textureSample(wired_bindless_images[(_e69 & 4095u)], wired_bindless_samplers[((_e79 >> bitcast<u32>(12i)) & 255u)], _e84);
    c_1 = _e85;
    let _e86 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e86))) == 0i) {
        let _e91 = c_1;
        param = _e91.xyz;
        let _e93 = sRGBToLinear_u0028_vf3_u003b((&param));
        c_1[0u] = _e93.x;
        c_1[1u] = _e93.y;
        c_1[2u] = _e93.z;
    }
    let _e100 = (*slot);
    if (lightmap_slot == (_e100 + 1i)) {
        let _e105 = unnamed.worldLightParams[0u];
        let _e106 = c_1;
        let _e108 = (_e106.xyz * _e105);
        c_1[0u] = _e108.x;
        c_1[1u] = _e108.y;
        c_1[2u] = _e108.z;
    }
    let _e115 = c_1;
    return _e115;
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
    var fogAmount: f32;

    let _e93 = unnamed.packed_indices[0i][3u];
    let _e99 = unnamed.packed_indices[0i][3u];
    let _e104 = fog_tex_coord_1;
    let _e105 = textureSample(wired_bindless_images[(_e93 & 4095u)], wired_bindless_samplers[((_e99 >> bitcast<u32>(12i)) & 255u)], _e104);
    fog = _e105;
    let _e106 = frag_color0In_1;
    param_1 = _e106.xyz;
    let _e108 = sRGBToLinear_u0028_vf3_u003b((&param_1));
    let _e110 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e108.x, _e108.y, _e108.z, _e110);
    param_2 = 0u;
    let _e115 = frag_tex_coord0_1;
    param_3 = _e115;
    param_4 = 0i;
    let _e116 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_2), (&param_3), (&param_4));
    let _e117 = frag_color0_;
    color0_ = (_e116 * _e117);
    if override_type_3_ {
        param_5 = 1u;
        let _e119 = frag_tex_coord1_1;
        param_6 = _e119;
        param_7 = 1i;
        let _e120 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_5), (&param_6), (&param_7));
        color1_ = _e120;
        param_8 = 2u;
        let _e121 = frag_tex_coord2_1;
        param_9 = _e121;
        param_10 = 2i;
        let _e122 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_8), (&param_9), (&param_10));
        color2_ = _e122;
        let _e123 = color0_;
        let _e125 = color1_;
        let _e128 = color2_;
        let _e130 = ((_e123.xyz + _e125.xyz) + _e128.xyz);
        let _e132 = color0_[3u];
        let _e134 = color1_[3u];
        let _e137 = color2_[3u];
        base = vec4<f32>(_e130.x, _e130.y, _e130.z, ((_e132 * _e134) * _e137));
    } else {
        if override_type_3_1 {
            param_11 = 1u;
            let _e143 = frag_tex_coord1_1;
            param_12 = _e143;
            param_13 = 1i;
            let _e144 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_11), (&param_12), (&param_13));
            let _e145 = frag_color0_;
            color1_1 = (_e144 * _e145);
            param_14 = 2u;
            let _e147 = frag_tex_coord2_1;
            param_15 = _e147;
            param_16 = 2i;
            let _e148 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_14), (&param_15), (&param_16));
            let _e149 = frag_color0_;
            color2_1 = (_e148 * _e149);
            let _e151 = color0_;
            let _e153 = color1_1;
            let _e156 = color2_1;
            let _e158 = ((_e151.xyz + _e153.xyz) + _e156.xyz);
            let _e160 = color0_[3u];
            let _e162 = color1_1[3u];
            let _e165 = color2_1[3u];
            base = vec4<f32>(_e158.x, _e158.y, _e158.z, ((_e160 * _e162) * _e165));
        } else {
            param_17 = 1u;
            let _e171 = frag_tex_coord1_1;
            param_18 = _e171;
            param_19 = 1i;
            let _e172 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_17), (&param_18), (&param_19));
            color1_2 = _e172;
            param_20 = 2u;
            let _e173 = frag_tex_coord2_1;
            param_21 = _e173;
            param_22 = 2i;
            let _e174 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_20), (&param_21), (&param_22));
            color2_2 = _e174;
            let _e175 = color0_;
            let _e177 = color1_2;
            let _e180 = color2_2;
            let _e182 = ((_e175.xyz * _e177.xyz) * _e180.xyz);
            base[0u] = _e182.x;
            base[1u] = _e182.y;
            base[2u] = _e182.z;
            let _e190 = color0_[3u];
            let _e192 = color1_2[3u];
            let _e195 = color2_2[3u];
            base[3u] = ((_e190 * _e192) * _e195);
        }
    }
    let _e198 = wired_advanced_fog_enabled_u0028_();
    if _e198 {
        let _e199 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e199;
        if override_type_3_2 {
            let _e200 = fogAmount;
            let _e202 = base;
            let _e204 = (_e202.xyz * (1f - _e200));
            base[0u] = _e204.x;
            base[1u] = _e204.y;
            base[2u] = _e204.z;
        } else {
            if override_type_3_3 {
                let _e211 = fogAmount;
                let _e213 = base;
                base = (_e213 * (1f - _e211));
            } else {
                if override_type_3_4 {
                    let _e215 = fogAmount;
                    let _e218 = base[3u];
                    base[3u] = (_e218 * (1f - _e215));
                } else {
                    let _e221 = base;
                    let _e224 = unnamed.advancedFogColorDensity;
                    let _e226 = fogAmount;
                    let _e228 = mix(_e221.xyz, _e224.xyz, vec3(_e226));
                    base[0u] = _e228.x;
                    base[1u] = _e228.y;
                    base[2u] = _e228.z;
                }
            }
        }
    } else {
        if override_type_3_5 {
            let _e235 = base;
            let _e238 = fog[3u];
            let _e240 = (_e235.xyz * (1f - _e238));
            base[0u] = _e240.x;
            base[1u] = _e240.y;
            base[2u] = _e240.z;
        } else {
            if override_type_3_6 {
                let _e247 = base;
                let _e249 = fog[3u];
                base = (_e247 * (1f - _e249));
            } else {
                if override_type_3_7 {
                    let _e253 = base[3u];
                    let _e255 = fog[3u];
                    base[3u] = (_e253 * (1f - _e255));
                } else {
                    let _e259 = base;
                    let _e260 = fog;
                    let _e262 = unnamed.fogColor;
                    let _e265 = fog[3u];
                    base = mix(_e259, (_e260 * _e262), vec4(_e265));
                }
            }
        }
    }
    if override_type_3_8 {
        let _e269 = base[3u];
        if (_e269 == 0f) {
            discard;
        }
    } else {
        if override_type_3_9 {
            let _e271 = base;
            let _e273 = base;
            if (dot(_e271.xyz, _e273.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e277 = base;
    out_color = _e277;
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
