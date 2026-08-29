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
override override_type_3_2: bool = (override_type_3_ || override_type_3_1);
override override_type_3_3: bool = (tex_mode == 3i);
override override_type_3_4: bool = (tex_mode == 4i);
override override_type_3_5: bool = (tex_mode == 5i);
override override_type_3_6: bool = (tex_mode == 6i);
override override_type_3_7: bool = (tex_mode == 7i);
override override_type_3_8: bool = (lightmap_slot != 0i);
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
var<private> frag_color0In_1: vec4<f32>;
var<private> frag_color1In_1: vec4<f32>;
var<private> frag_color2In_1: vec4<f32>;
var<private> frag_tex_coord0_1: vec2<f32>;
var<private> frag_tex_coord1_1: vec2<f32>;
var<private> frag_tex_coord2_1: vec2<f32>;
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

    let _e75 = (*role);
    let _e77 = (*role);
    let _e82 = unnamed.packed_indices[(_e75 / 4u)][(_e77 % 4u)];
    let _e85 = (*role);
    let _e87 = (*role);
    let _e92 = unnamed.packed_indices[(_e85 / 4u)][(_e87 % 4u)];
    let _e97 = (*uv);
    let _e98 = textureSample(wired_bindless_images[(_e82 & 4095u)], wired_bindless_samplers[((_e92 >> bitcast<u32>(12i)) & 255u)], _e97);
    c_1 = _e98;
    let _e99 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e99))) == 0i) {
        let _e104 = c_1;
        param = _e104.xyz;
        let _e106 = sRGBToLinear_u0028_vf3_u003b((&param));
        c_1[0u] = _e106.x;
        c_1[1u] = _e106.y;
        c_1[2u] = _e106.z;
    }
    let _e113 = (*slot);
    if (lightmap_slot == (_e113 + 1i)) {
        let _e118 = unnamed.worldLightParams[0u];
        let _e119 = c_1;
        let _e121 = (_e119.xyz * _e118);
        c_1[0u] = _e121.x;
        c_1[1u] = _e121.y;
        c_1[2u] = _e121.z;
    }
    let _e128 = c_1;
    return _e128;
}

fn main_1() {
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
    var wetness: f32;
    var frost: f32;
    var luminance: f32;
    var fogAmount: f32;

    let _e141 = frag_color0In_1;
    param_1 = _e141.xyz;
    let _e143 = sRGBToLinear_u0028_vf3_u003b((&param_1));
    let _e145 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e143.x, _e143.y, _e143.z, _e145);
    let _e150 = frag_color1In_1;
    param_2 = _e150.xyz;
    let _e152 = sRGBToLinear_u0028_vf3_u003b((&param_2));
    let _e154 = frag_color1In_1[3u];
    frag_color1_ = vec4<f32>(_e152.x, _e152.y, _e152.z, _e154);
    let _e159 = frag_color2In_1;
    param_3 = _e159.xyz;
    let _e161 = sRGBToLinear_u0028_vf3_u003b((&param_3));
    let _e163 = frag_color2In_1[3u];
    frag_color2_ = vec4<f32>(_e161.x, _e161.y, _e161.z, _e163);
    param_4 = 0u;
    let _e168 = frag_tex_coord0_1;
    param_5 = _e168;
    param_6 = 0i;
    let _e169 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_4), (&param_5), (&param_6));
    let _e170 = frag_color0_;
    color0_ = (_e169 * _e170);
    if override_type_3_2 {
        param_7 = 1u;
        let _e172 = frag_tex_coord1_1;
        param_8 = _e172;
        param_9 = 1i;
        let _e173 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_7), (&param_8), (&param_9));
        let _e174 = frag_color1_;
        color1_ = (_e173 * _e174);
        param_10 = 2u;
        let _e176 = frag_tex_coord2_1;
        param_11 = _e176;
        param_12 = 2i;
        let _e177 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_10), (&param_11), (&param_12));
        let _e178 = frag_color2_;
        color2_ = (_e177 * _e178);
        let _e180 = color0_;
        let _e182 = color1_;
        let _e185 = color2_;
        let _e187 = ((_e180.xyz + _e182.xyz) + _e185.xyz);
        let _e189 = color0_[3u];
        let _e191 = color1_[3u];
        let _e194 = color2_[3u];
        base = vec4<f32>(_e187.x, _e187.y, _e187.z, ((_e189 * _e191) * _e194));
    } else {
        if override_type_3_3 {
            param_13 = 1u;
            let _e200 = frag_tex_coord1_1;
            param_14 = _e200;
            param_15 = 1i;
            let _e201 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_13), (&param_14), (&param_15));
            let _e202 = frag_color1_;
            color1_1 = (_e201 * _e202);
            param_16 = 2u;
            let _e204 = frag_tex_coord2_1;
            param_17 = _e204;
            param_18 = 2i;
            let _e205 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_16), (&param_17), (&param_18));
            let _e206 = frag_color2_;
            color2_1 = (_e205 * _e206);
            let _e209 = color0_[3u];
            let _e210 = color0_;
            color0_ = (_e210 * _e209);
            let _e213 = color1_1[3u];
            let _e214 = color1_1;
            color1_1 = (_e214 * _e213);
            let _e217 = color2_1[3u];
            let _e218 = color2_1;
            color2_1 = (_e218 * _e217);
            let _e220 = color0_;
            let _e222 = color1_1;
            let _e225 = color2_1;
            let _e227 = ((_e220.xyz + _e222.xyz) + _e225.xyz);
            let _e229 = color0_[3u];
            let _e231 = color1_1[3u];
            let _e234 = color2_1[3u];
            base = vec4<f32>(_e227.x, _e227.y, _e227.z, ((_e229 * _e231) * _e234));
        } else {
            if override_type_3_4 {
                param_19 = 1u;
                let _e240 = frag_tex_coord1_1;
                param_20 = _e240;
                param_21 = 1i;
                let _e241 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_19), (&param_20), (&param_21));
                let _e242 = frag_color1_;
                color1_2 = (_e241 * _e242);
                param_22 = 2u;
                let _e244 = frag_tex_coord2_1;
                param_23 = _e244;
                param_24 = 2i;
                let _e245 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_22), (&param_23), (&param_24));
                let _e246 = frag_color2_;
                color2_2 = (_e245 * _e246);
                let _e249 = color0_[3u];
                let _e251 = color0_;
                color0_ = (_e251 * (1f - _e249));
                let _e254 = color1_2[3u];
                let _e256 = color1_2;
                color1_2 = (_e256 * (1f - _e254));
                let _e259 = color2_2[3u];
                let _e261 = color2_2;
                color2_2 = (_e261 * (1f - _e259));
                let _e263 = color0_;
                let _e265 = color1_2;
                let _e268 = color2_2;
                let _e270 = ((_e263.xyz + _e265.xyz) + _e268.xyz);
                let _e272 = color0_[3u];
                let _e274 = color1_2[3u];
                let _e277 = color2_2[3u];
                base = vec4<f32>(_e270.x, _e270.y, _e270.z, ((_e272 * _e274) * _e277));
            } else {
                if override_type_3_5 {
                    param_25 = 1u;
                    let _e283 = frag_tex_coord1_1;
                    param_26 = _e283;
                    param_27 = 1i;
                    let _e284 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_25), (&param_26), (&param_27));
                    let _e285 = frag_color1_;
                    color1_3 = (_e284 * _e285);
                    param_28 = 2u;
                    let _e287 = frag_tex_coord2_1;
                    param_29 = _e287;
                    param_30 = 2i;
                    let _e288 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_28), (&param_29), (&param_30));
                    let _e289 = frag_color2_;
                    color2_3 = (_e288 * _e289);
                    let _e291 = color0_;
                    let _e292 = color1_3;
                    let _e294 = color1_3[3u];
                    let _e297 = color2_3;
                    let _e299 = color2_3[3u];
                    base = mix(mix(_e291, _e292, vec4(_e294)), _e297, vec4(_e299));
                } else {
                    if override_type_3_6 {
                        param_31 = 1u;
                        let _e302 = frag_tex_coord1_1;
                        param_32 = _e302;
                        param_33 = 1i;
                        let _e303 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_31), (&param_32), (&param_33));
                        let _e304 = frag_color1_;
                        color1_4 = (_e303 * _e304);
                        param_34 = 2u;
                        let _e306 = frag_tex_coord2_1;
                        param_35 = _e306;
                        param_36 = 2i;
                        let _e307 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_34), (&param_35), (&param_36));
                        let _e308 = frag_color2_;
                        color2_4 = (_e307 * _e308);
                        let _e310 = color2_4;
                        let _e311 = color1_4;
                        let _e312 = color0_;
                        let _e314 = color1_4[3u];
                        let _e318 = color2_4[3u];
                        base = mix(_e310, mix(_e311, _e312, vec4(_e314)), vec4(_e318));
                    } else {
                        if override_type_3_7 {
                            param_37 = 1u;
                            let _e321 = frag_tex_coord1_1;
                            param_38 = _e321;
                            param_39 = 1i;
                            let _e322 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_37), (&param_38), (&param_39));
                            let _e323 = frag_color1_;
                            color1_5 = (_e322 * _e323);
                            param_40 = 2u;
                            let _e325 = frag_tex_coord2_1;
                            param_41 = _e325;
                            param_42 = 2i;
                            let _e326 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_40), (&param_41), (&param_42));
                            let _e327 = frag_color2_;
                            color2_5 = (_e326 * _e327);
                            let _e329 = color2_5;
                            let _e331 = color2_5[3u];
                            let _e334 = color1_5;
                            let _e336 = color1_5[3u];
                            let _e340 = color0_;
                            base = (((_e329 + vec4(_e331)) * (_e334 + vec4(_e336))) * _e340);
                        } else {
                            param_43 = 1u;
                            let _e342 = frag_tex_coord1_1;
                            param_44 = _e342;
                            param_45 = 1i;
                            let _e343 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_43), (&param_44), (&param_45));
                            let _e344 = frag_color1_;
                            color1_6 = (_e343 * _e344);
                            param_46 = 2u;
                            let _e346 = frag_tex_coord2_1;
                            param_47 = _e346;
                            param_48 = 2i;
                            let _e347 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_46), (&param_47), (&param_48));
                            let _e348 = frag_color2_;
                            color2_6 = (_e347 * _e348);
                            let _e350 = color0_;
                            let _e352 = color1_6;
                            let _e355 = color2_6;
                            let _e357 = ((_e350.xyz * _e352.xyz) * _e355.xyz);
                            base[0u] = _e357.x;
                            base[1u] = _e357.y;
                            base[2u] = _e357.z;
                            let _e365 = color0_[3u];
                            let _e367 = color1_6[3u];
                            let _e370 = color2_6[3u];
                            base[3u] = ((_e365 * _e367) * _e370);
                        }
                    }
                }
            }
        }
    }
    if override_type_3_8 {
        let _e375 = unnamed.worldLightParams[1u];
        wetness = clamp(_e375, 0f, 1f);
        let _e379 = unnamed.worldLightParams[2u];
        frost = clamp(_e379, 0f, 1f);
        let _e381 = base;
        luminance = dot(_e381.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e384 = wetness;
        let _e386 = base;
        let _e388 = (_e386.xyz * mix(1f, 0.82f, _e384));
        base[0u] = _e388.x;
        base[1u] = _e388.y;
        base[2u] = _e388.z;
        let _e395 = base;
        let _e397 = luminance;
        let _e399 = luminance;
        let _e401 = luminance;
        let _e403 = frost;
        let _e406 = mix(_e395.xyz, vec3<f32>((_e397 * 0.88f), (_e399 * 0.94f), _e401), vec3((_e403 * 0.55f)));
        base[0u] = _e406.x;
        base[1u] = _e406.y;
        base[2u] = _e406.z;
    }
    let _e413 = color0_;
    let _e416 = unnamed.emissionRadiance;
    let _e419 = base;
    let _e421 = (_e419.xyz + (_e413.xyz * _e416.xyz));
    base[0u] = _e421.x;
    base[1u] = _e421.y;
    base[2u] = _e421.z;
    let _e428 = wired_advanced_fog_enabled_u0028_();
    if _e428 {
        let _e429 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e429;
        let _e430 = base;
        let _e433 = unnamed.advancedFogColorDensity;
        let _e435 = fogAmount;
        let _e437 = mix(_e430.xyz, _e433.xyz, vec3(_e435));
        base[0u] = _e437.x;
        base[1u] = _e437.y;
        base[2u] = _e437.z;
    }
    if override_type_3_9 {
        let _e445 = base[3u];
        if (_e445 == 0f) {
            discard;
        }
    } else {
        if override_type_3_10 {
            let _e447 = base;
            let _e449 = base;
            if (dot(_e447.xyz, _e449.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e453 = base;
    out_color = _e453;
    return;
}

@fragment
fn main(@builtin(position) gl_FragCoord: vec4<f32>, @location(0) frag_color0In: vec4<f32>, @location(5) frag_color1In: vec4<f32>, @location(6) frag_color2In: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>, @location(3) frag_tex_coord2_: vec2<f32>) -> @location(0) vec4<f32> {
    gl_FragCoord_1 = gl_FragCoord;
    frag_color0In_1 = frag_color0In;
    frag_color1In_1 = frag_color1In;
    frag_color2In_1 = frag_color2In;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    frag_tex_coord2_1 = frag_tex_coord2_;
    main_1();
    let _e15 = out_color;
    return _e15;
}
