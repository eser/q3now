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
override override_type_3_2: bool = (override_type_3_ || override_type_3_1);
override override_type_3_3: bool = (tex_mode == 3i);
override override_type_3_4: bool = (tex_mode == 4i);
override override_type_3_5: bool = (tex_mode == 5i);
override override_type_3_6: bool = (tex_mode == 6i);
override override_type_3_7: bool = (tex_mode == 7i);
@id(10) override acff: i32 = 0i;
override override_type_3_8: bool = (acff == 1i);
override override_type_3_9: bool = (acff == 2i);
override override_type_3_10: bool = (acff == 3i);
override override_type_3_11: bool = (acff == 1i);
override override_type_3_12: bool = (acff == 2i);
override override_type_3_13: bool = (acff == 3i);
@id(7) override discard_mode: i32 = 0i;
override override_type_3_14: bool = (discard_mode == 1i);
override override_type_3_15: bool = (discard_mode == 2i);
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
var<private> frag_color1In_1: vec4<f32>;
var<private> frag_color2In_1: vec4<f32>;
var<private> frag_tex_coord0_1: vec2<f32>;
var<private> frag_tex_coord1_1: vec2<f32>;
var<private> frag_tex_coord2_1: vec2<f32>;
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
        let _e116 = unnamed.worldLightParams[0u];
        let _e117 = c_1;
        let _e119 = (_e117.xyz * _e116);
        c_1[0u] = _e119.x;
        c_1[1u] = _e119.y;
        c_1[2u] = _e119.z;
    }
    let _e126 = c_1;
    return _e126;
}

fn main_1() {
    var fog: vec4<f32>;
    var frag_color0_: vec4<f32>;
    var param_1: vec3<f32>;
    var frag_color1_: vec4<f32>;
    var param_2: vec3<f32>;
    var frag_color2_: vec4<f32>;
    var param_3: vec3<f32>;
    var color0_: vec4<f32>;
    var param_4: u32;
    var param_5: vec2<f32>;
    var param_6: i32;
    var color1_: vec4<f32>;
    var param_7: u32;
    var param_8: vec2<f32>;
    var param_9: i32;
    var color2_: vec4<f32>;
    var param_10: u32;
    var param_11: vec2<f32>;
    var param_12: i32;
    var base: vec4<f32>;
    var color1_1: vec4<f32>;
    var param_13: u32;
    var param_14: vec2<f32>;
    var param_15: i32;
    var color2_1: vec4<f32>;
    var param_16: u32;
    var param_17: vec2<f32>;
    var param_18: i32;
    var color1_2: vec4<f32>;
    var param_19: u32;
    var param_20: vec2<f32>;
    var param_21: i32;
    var color2_2: vec4<f32>;
    var param_22: u32;
    var param_23: vec2<f32>;
    var param_24: i32;
    var color1_3: vec4<f32>;
    var param_25: u32;
    var param_26: vec2<f32>;
    var param_27: i32;
    var color2_3: vec4<f32>;
    var param_28: u32;
    var param_29: vec2<f32>;
    var param_30: i32;
    var color1_4: vec4<f32>;
    var param_31: u32;
    var param_32: vec2<f32>;
    var param_33: i32;
    var color2_4: vec4<f32>;
    var param_34: u32;
    var param_35: vec2<f32>;
    var param_36: i32;
    var color1_5: vec4<f32>;
    var param_37: u32;
    var param_38: vec2<f32>;
    var param_39: i32;
    var color2_5: vec4<f32>;
    var param_40: u32;
    var param_41: vec2<f32>;
    var param_42: i32;
    var color1_6: vec4<f32>;
    var param_43: u32;
    var param_44: vec2<f32>;
    var param_45: i32;
    var color2_6: vec4<f32>;
    var param_46: u32;
    var param_47: vec2<f32>;
    var param_48: i32;
    var fogAmount: f32;

    let _e140 = unnamed.packed_indices[0i][3u];
    let _e146 = unnamed.packed_indices[0i][3u];
    let _e151 = fog_tex_coord_1;
    let _e152 = textureSample(wired_bindless_images[(_e140 & 4095u)], wired_bindless_samplers[((_e146 >> bitcast<u32>(12i)) & 255u)], _e151);
    fog = _e152;
    let _e153 = frag_color0In_1;
    param_1 = _e153.xyz;
    let _e155 = sRGBToLinear_u0028_vf3_u003b((&param_1));
    let _e157 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e155.x, _e155.y, _e155.z, _e157);
    let _e162 = frag_color1In_1;
    param_2 = _e162.xyz;
    let _e164 = sRGBToLinear_u0028_vf3_u003b((&param_2));
    let _e166 = frag_color1In_1[3u];
    frag_color1_ = vec4<f32>(_e164.x, _e164.y, _e164.z, _e166);
    let _e171 = frag_color2In_1;
    param_3 = _e171.xyz;
    let _e173 = sRGBToLinear_u0028_vf3_u003b((&param_3));
    let _e175 = frag_color2In_1[3u];
    frag_color2_ = vec4<f32>(_e173.x, _e173.y, _e173.z, _e175);
    param_4 = 0u;
    let _e180 = frag_tex_coord0_1;
    param_5 = _e180;
    param_6 = 0i;
    let _e181 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_4), (&param_5), (&param_6));
    let _e182 = frag_color0_;
    color0_ = (_e181 * _e182);
    if override_type_3_2 {
        param_7 = 1u;
        let _e184 = frag_tex_coord1_1;
        param_8 = _e184;
        param_9 = 1i;
        let _e185 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_7), (&param_8), (&param_9));
        let _e186 = frag_color1_;
        color1_ = (_e185 * _e186);
        param_10 = 2u;
        let _e188 = frag_tex_coord2_1;
        param_11 = _e188;
        param_12 = 2i;
        let _e189 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_10), (&param_11), (&param_12));
        let _e190 = frag_color2_;
        color2_ = (_e189 * _e190);
        let _e192 = color0_;
        let _e194 = color1_;
        let _e197 = color2_;
        let _e199 = ((_e192.xyz + _e194.xyz) + _e197.xyz);
        let _e201 = color0_[3u];
        let _e203 = color1_[3u];
        let _e206 = color2_[3u];
        base = vec4<f32>(_e199.x, _e199.y, _e199.z, ((_e201 * _e203) * _e206));
    } else {
        if override_type_3_3 {
            param_13 = 1u;
            let _e212 = frag_tex_coord1_1;
            param_14 = _e212;
            param_15 = 1i;
            let _e213 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_13), (&param_14), (&param_15));
            let _e214 = frag_color1_;
            color1_1 = (_e213 * _e214);
            param_16 = 2u;
            let _e216 = frag_tex_coord2_1;
            param_17 = _e216;
            param_18 = 2i;
            let _e217 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_16), (&param_17), (&param_18));
            let _e218 = frag_color2_;
            color2_1 = (_e217 * _e218);
            let _e221 = color0_[3u];
            let _e222 = color0_;
            color0_ = (_e222 * _e221);
            let _e225 = color1_1[3u];
            let _e226 = color1_1;
            color1_1 = (_e226 * _e225);
            let _e229 = color2_1[3u];
            let _e230 = color2_1;
            color2_1 = (_e230 * _e229);
            let _e232 = color0_;
            let _e234 = color1_1;
            let _e237 = color2_1;
            let _e239 = ((_e232.xyz + _e234.xyz) + _e237.xyz);
            let _e241 = color0_[3u];
            let _e243 = color1_1[3u];
            let _e246 = color2_1[3u];
            base = vec4<f32>(_e239.x, _e239.y, _e239.z, ((_e241 * _e243) * _e246));
        } else {
            if override_type_3_4 {
                param_19 = 1u;
                let _e252 = frag_tex_coord1_1;
                param_20 = _e252;
                param_21 = 1i;
                let _e253 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_19), (&param_20), (&param_21));
                let _e254 = frag_color1_;
                color1_2 = (_e253 * _e254);
                param_22 = 2u;
                let _e256 = frag_tex_coord2_1;
                param_23 = _e256;
                param_24 = 2i;
                let _e257 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_22), (&param_23), (&param_24));
                let _e258 = frag_color2_;
                color2_2 = (_e257 * _e258);
                let _e261 = color0_[3u];
                let _e263 = color0_;
                color0_ = (_e263 * (1f - _e261));
                let _e266 = color1_2[3u];
                let _e268 = color1_2;
                color1_2 = (_e268 * (1f - _e266));
                let _e271 = color2_2[3u];
                let _e273 = color2_2;
                color2_2 = (_e273 * (1f - _e271));
                let _e275 = color0_;
                let _e277 = color1_2;
                let _e280 = color2_2;
                let _e282 = ((_e275.xyz + _e277.xyz) + _e280.xyz);
                let _e284 = color0_[3u];
                let _e286 = color1_2[3u];
                let _e289 = color2_2[3u];
                base = vec4<f32>(_e282.x, _e282.y, _e282.z, ((_e284 * _e286) * _e289));
            } else {
                if override_type_3_5 {
                    param_25 = 1u;
                    let _e295 = frag_tex_coord1_1;
                    param_26 = _e295;
                    param_27 = 1i;
                    let _e296 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_25), (&param_26), (&param_27));
                    let _e297 = frag_color1_;
                    color1_3 = (_e296 * _e297);
                    param_28 = 2u;
                    let _e299 = frag_tex_coord2_1;
                    param_29 = _e299;
                    param_30 = 2i;
                    let _e300 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_28), (&param_29), (&param_30));
                    let _e301 = frag_color2_;
                    color2_3 = (_e300 * _e301);
                    let _e303 = color0_;
                    let _e304 = color1_3;
                    let _e306 = color1_3[3u];
                    let _e309 = color2_3;
                    let _e311 = color2_3[3u];
                    base = mix(mix(_e303, _e304, vec4(_e306)), _e309, vec4(_e311));
                } else {
                    if override_type_3_6 {
                        param_31 = 1u;
                        let _e314 = frag_tex_coord1_1;
                        param_32 = _e314;
                        param_33 = 1i;
                        let _e315 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_31), (&param_32), (&param_33));
                        let _e316 = frag_color1_;
                        color1_4 = (_e315 * _e316);
                        param_34 = 2u;
                        let _e318 = frag_tex_coord2_1;
                        param_35 = _e318;
                        param_36 = 2i;
                        let _e319 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_34), (&param_35), (&param_36));
                        let _e320 = frag_color2_;
                        color2_4 = (_e319 * _e320);
                        let _e322 = color2_4;
                        let _e323 = color1_4;
                        let _e324 = color0_;
                        let _e326 = color1_4[3u];
                        let _e330 = color2_4[3u];
                        base = mix(_e322, mix(_e323, _e324, vec4(_e326)), vec4(_e330));
                    } else {
                        if override_type_3_7 {
                            param_37 = 1u;
                            let _e333 = frag_tex_coord1_1;
                            param_38 = _e333;
                            param_39 = 1i;
                            let _e334 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_37), (&param_38), (&param_39));
                            let _e335 = frag_color1_;
                            color1_5 = (_e334 * _e335);
                            param_40 = 2u;
                            let _e337 = frag_tex_coord2_1;
                            param_41 = _e337;
                            param_42 = 2i;
                            let _e338 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_40), (&param_41), (&param_42));
                            let _e339 = frag_color2_;
                            color2_5 = (_e338 * _e339);
                            let _e341 = color2_5;
                            let _e343 = color2_5[3u];
                            let _e346 = color1_5;
                            let _e348 = color1_5[3u];
                            let _e352 = color0_;
                            base = (((_e341 + vec4(_e343)) * (_e346 + vec4(_e348))) * _e352);
                        } else {
                            param_43 = 1u;
                            let _e354 = frag_tex_coord1_1;
                            param_44 = _e354;
                            param_45 = 1i;
                            let _e355 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_43), (&param_44), (&param_45));
                            let _e356 = frag_color1_;
                            color1_6 = (_e355 * _e356);
                            param_46 = 2u;
                            let _e358 = frag_tex_coord2_1;
                            param_47 = _e358;
                            param_48 = 2i;
                            let _e359 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_46), (&param_47), (&param_48));
                            let _e360 = frag_color2_;
                            color2_6 = (_e359 * _e360);
                            let _e362 = color0_;
                            let _e364 = color1_6;
                            let _e367 = color2_6;
                            let _e369 = ((_e362.xyz * _e364.xyz) * _e367.xyz);
                            base[0u] = _e369.x;
                            base[1u] = _e369.y;
                            base[2u] = _e369.z;
                            let _e377 = color0_[3u];
                            let _e379 = color1_6[3u];
                            let _e382 = color2_6[3u];
                            base[3u] = ((_e377 * _e379) * _e382);
                        }
                    }
                }
            }
        }
    }
    let _e385 = wired_advanced_fog_enabled_u0028_();
    if _e385 {
        let _e386 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e386;
        if override_type_3_8 {
            let _e387 = fogAmount;
            let _e389 = base;
            let _e391 = (_e389.xyz * (1f - _e387));
            base[0u] = _e391.x;
            base[1u] = _e391.y;
            base[2u] = _e391.z;
        } else {
            if override_type_3_9 {
                let _e398 = fogAmount;
                let _e400 = base;
                base = (_e400 * (1f - _e398));
            } else {
                if override_type_3_10 {
                    let _e402 = fogAmount;
                    let _e405 = base[3u];
                    base[3u] = (_e405 * (1f - _e402));
                } else {
                    let _e408 = base;
                    let _e411 = unnamed.advancedFogColorDensity;
                    let _e413 = fogAmount;
                    let _e415 = mix(_e408.xyz, _e411.xyz, vec3(_e413));
                    base[0u] = _e415.x;
                    base[1u] = _e415.y;
                    base[2u] = _e415.z;
                }
            }
        }
    } else {
        if override_type_3_11 {
            let _e422 = base;
            let _e425 = fog[3u];
            let _e427 = (_e422.xyz * (1f - _e425));
            base[0u] = _e427.x;
            base[1u] = _e427.y;
            base[2u] = _e427.z;
        } else {
            if override_type_3_12 {
                let _e434 = base;
                let _e436 = fog[3u];
                base = (_e434 * (1f - _e436));
            } else {
                if override_type_3_13 {
                    let _e440 = base[3u];
                    let _e442 = fog[3u];
                    base[3u] = (_e440 * (1f - _e442));
                } else {
                    let _e446 = base;
                    let _e447 = fog;
                    let _e449 = unnamed.fogColor;
                    let _e452 = fog[3u];
                    base = mix(_e446, (_e447 * _e449), vec4(_e452));
                }
            }
        }
    }
    if override_type_3_14 {
        let _e456 = base[3u];
        if (_e456 == 0f) {
            discard;
        }
    } else {
        if override_type_3_15 {
            let _e458 = base;
            let _e460 = base;
            if (dot(_e458.xyz, _e460.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e464 = base;
    out_color = _e464;
    return;
}

@fragment
fn main(@builtin(position) gl_FragCoord: vec4<f32>, @location(4) fog_tex_coord: vec2<f32>, @location(0) frag_color0In: vec4<f32>, @location(5) frag_color1In: vec4<f32>, @location(6) frag_color2In: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>, @location(3) frag_tex_coord2_: vec2<f32>) -> @location(0) vec4<f32> {
    gl_FragCoord_1 = gl_FragCoord;
    fog_tex_coord_1 = fog_tex_coord;
    frag_color0In_1 = frag_color0In;
    frag_color1In_1 = frag_color1In;
    frag_color2In_1 = frag_color2In;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    frag_tex_coord2_1 = frag_tex_coord2_;
    main_1();
    let _e17 = out_color;
    return _e17;
}
