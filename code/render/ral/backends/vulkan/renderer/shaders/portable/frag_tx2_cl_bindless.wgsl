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
var<private> frag_color0In_1: vec4<f32>;
var<private> frag_color1In_1: vec4<f32>;
var<private> frag_color2In_1: vec4<f32>;
var<private> frag_tex_coord0_1: vec2<f32>;
var<private> frag_tex_coord1_1: vec2<f32>;
var<private> frag_tex_coord2_1: vec2<f32>;
var<private> out_color: vec4<f32>;

fn wired_advanced_fog_enabled_u0028_() -> bool {
    var fogType: i32;

    let _e63 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e63 + 0.5f));
    let _e68 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e70 = fogType;
    let _e73 = fogType;
    return (((_e68 > 0.5f) && (_e70 >= 1i)) && (_e73 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e63 = wired_advanced_fog_enabled_u0028_();
    if !(_e63) {
        return 0f;
    }
    let _e66 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e66, 0.000001f));
    let _e71 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e71 + 0.5f));
    let _e74 = fogType_1;
    if (_e74 == 1i) {
        let _e78 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e78 <= 0f) {
            return 0f;
        }
        let _e80 = viewDepth;
        let _e83 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e80 / _e83), 0f, 1f);
    }
    let _e88 = unnamed.advancedFogColorDensity[3u];
    let _e90 = viewDepth;
    opticalDepth = (max(_e88, 0f) * _e90);
    let _e92 = fogType_1;
    if (_e92 == 2i) {
        let _e94 = opticalDepth;
        return clamp((1f - exp(-(_e94))), 0f, 1f);
    }
    let _e99 = opticalDepth;
    let _e100 = opticalDepth;
    return clamp((1f - exp(-((_e99 * _e100)))), 0f, 1f);
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e64 = (*c);
    (*c) = max(_e64, vec3<f32>(0f, 0f, 0f));
    let _e66 = (*c);
    cutoff = (_e66 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e68 = (*c);
    lo = (_e68 / vec3(12.92f));
    let _e71 = (*c);
    hi = pow(((_e71 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e76 = hi;
    let _e77 = lo;
    let _e78 = cutoff;
    return mix(_e76, _e77, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e78));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param: vec3<f32>;

    let _e65 = (*role);
    let _e67 = (*role);
    let _e72 = unnamed.packed_indices[(_e65 / 4u)][(_e67 % 4u)];
    let _e75 = (*role);
    let _e77 = (*role);
    let _e82 = unnamed.packed_indices[(_e75 / 4u)][(_e77 % 4u)];
    let _e87 = (*uv);
    let _e88 = textureSample(wired_bindless_images[(_e72 & 4095u)], wired_bindless_samplers[((_e82 >> bitcast<u32>(12i)) & 255u)], _e87);
    c_1 = _e88;
    let _e89 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e89))) == 0i) {
        let _e94 = c_1;
        param = _e94.xyz;
        let _e96 = sRGBToLinear_u0028_vf3_u003b((&param));
        c_1[0u] = _e96.x;
        c_1[1u] = _e96.y;
        c_1[2u] = _e96.z;
    }
    let _e103 = (*slot);
    if (lightmap_slot == (_e103 + 1i)) {
        let _e108 = unnamed.worldLightParams[0u];
        let _e109 = c_1;
        let _e111 = (_e109.xyz * _e108);
        c_1[0u] = _e111.x;
        c_1[1u] = _e111.y;
        c_1[2u] = _e111.z;
    }
    let _e118 = c_1;
    return _e118;
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
    var fogAmount: f32;

    let _e128 = frag_color0In_1;
    param_1 = _e128.xyz;
    let _e130 = sRGBToLinear_u0028_vf3_u003b((&param_1));
    let _e132 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e130.x, _e130.y, _e130.z, _e132);
    let _e137 = frag_color1In_1;
    param_2 = _e137.xyz;
    let _e139 = sRGBToLinear_u0028_vf3_u003b((&param_2));
    let _e141 = frag_color1In_1[3u];
    frag_color1_ = vec4<f32>(_e139.x, _e139.y, _e139.z, _e141);
    let _e146 = frag_color2In_1;
    param_3 = _e146.xyz;
    let _e148 = sRGBToLinear_u0028_vf3_u003b((&param_3));
    let _e150 = frag_color2In_1[3u];
    frag_color2_ = vec4<f32>(_e148.x, _e148.y, _e148.z, _e150);
    param_4 = 0u;
    let _e155 = frag_tex_coord0_1;
    param_5 = _e155;
    param_6 = 0i;
    let _e156 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_4), (&param_5), (&param_6));
    let _e157 = frag_color0_;
    color0_ = (_e156 * _e157);
    if override_type_3_2 {
        param_7 = 1u;
        let _e159 = frag_tex_coord1_1;
        param_8 = _e159;
        param_9 = 1i;
        let _e160 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_7), (&param_8), (&param_9));
        let _e161 = frag_color1_;
        color1_ = (_e160 * _e161);
        param_10 = 2u;
        let _e163 = frag_tex_coord2_1;
        param_11 = _e163;
        param_12 = 2i;
        let _e164 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_10), (&param_11), (&param_12));
        let _e165 = frag_color2_;
        color2_ = (_e164 * _e165);
        let _e167 = color0_;
        let _e169 = color1_;
        let _e172 = color2_;
        let _e174 = ((_e167.xyz + _e169.xyz) + _e172.xyz);
        let _e176 = color0_[3u];
        let _e178 = color1_[3u];
        let _e181 = color2_[3u];
        base = vec4<f32>(_e174.x, _e174.y, _e174.z, ((_e176 * _e178) * _e181));
    } else {
        if override_type_3_3 {
            param_13 = 1u;
            let _e187 = frag_tex_coord1_1;
            param_14 = _e187;
            param_15 = 1i;
            let _e188 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_13), (&param_14), (&param_15));
            let _e189 = frag_color1_;
            color1_1 = (_e188 * _e189);
            param_16 = 2u;
            let _e191 = frag_tex_coord2_1;
            param_17 = _e191;
            param_18 = 2i;
            let _e192 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_16), (&param_17), (&param_18));
            let _e193 = frag_color2_;
            color2_1 = (_e192 * _e193);
            let _e196 = color0_[3u];
            let _e197 = color0_;
            color0_ = (_e197 * _e196);
            let _e200 = color1_1[3u];
            let _e201 = color1_1;
            color1_1 = (_e201 * _e200);
            let _e204 = color2_1[3u];
            let _e205 = color2_1;
            color2_1 = (_e205 * _e204);
            let _e207 = color0_;
            let _e209 = color1_1;
            let _e212 = color2_1;
            let _e214 = ((_e207.xyz + _e209.xyz) + _e212.xyz);
            let _e216 = color0_[3u];
            let _e218 = color1_1[3u];
            let _e221 = color2_1[3u];
            base = vec4<f32>(_e214.x, _e214.y, _e214.z, ((_e216 * _e218) * _e221));
        } else {
            if override_type_3_4 {
                param_19 = 1u;
                let _e227 = frag_tex_coord1_1;
                param_20 = _e227;
                param_21 = 1i;
                let _e228 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_19), (&param_20), (&param_21));
                let _e229 = frag_color1_;
                color1_2 = (_e228 * _e229);
                param_22 = 2u;
                let _e231 = frag_tex_coord2_1;
                param_23 = _e231;
                param_24 = 2i;
                let _e232 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_22), (&param_23), (&param_24));
                let _e233 = frag_color2_;
                color2_2 = (_e232 * _e233);
                let _e236 = color0_[3u];
                let _e238 = color0_;
                color0_ = (_e238 * (1f - _e236));
                let _e241 = color1_2[3u];
                let _e243 = color1_2;
                color1_2 = (_e243 * (1f - _e241));
                let _e246 = color2_2[3u];
                let _e248 = color2_2;
                color2_2 = (_e248 * (1f - _e246));
                let _e250 = color0_;
                let _e252 = color1_2;
                let _e255 = color2_2;
                let _e257 = ((_e250.xyz + _e252.xyz) + _e255.xyz);
                let _e259 = color0_[3u];
                let _e261 = color1_2[3u];
                let _e264 = color2_2[3u];
                base = vec4<f32>(_e257.x, _e257.y, _e257.z, ((_e259 * _e261) * _e264));
            } else {
                if override_type_3_5 {
                    param_25 = 1u;
                    let _e270 = frag_tex_coord1_1;
                    param_26 = _e270;
                    param_27 = 1i;
                    let _e271 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_25), (&param_26), (&param_27));
                    let _e272 = frag_color1_;
                    color1_3 = (_e271 * _e272);
                    param_28 = 2u;
                    let _e274 = frag_tex_coord2_1;
                    param_29 = _e274;
                    param_30 = 2i;
                    let _e275 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_28), (&param_29), (&param_30));
                    let _e276 = frag_color2_;
                    color2_3 = (_e275 * _e276);
                    let _e278 = color0_;
                    let _e279 = color1_3;
                    let _e281 = color1_3[3u];
                    let _e284 = color2_3;
                    let _e286 = color2_3[3u];
                    base = mix(mix(_e278, _e279, vec4(_e281)), _e284, vec4(_e286));
                } else {
                    if override_type_3_6 {
                        param_31 = 1u;
                        let _e289 = frag_tex_coord1_1;
                        param_32 = _e289;
                        param_33 = 1i;
                        let _e290 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_31), (&param_32), (&param_33));
                        let _e291 = frag_color1_;
                        color1_4 = (_e290 * _e291);
                        param_34 = 2u;
                        let _e293 = frag_tex_coord2_1;
                        param_35 = _e293;
                        param_36 = 2i;
                        let _e294 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_34), (&param_35), (&param_36));
                        let _e295 = frag_color2_;
                        color2_4 = (_e294 * _e295);
                        let _e297 = color2_4;
                        let _e298 = color1_4;
                        let _e299 = color0_;
                        let _e301 = color1_4[3u];
                        let _e305 = color2_4[3u];
                        base = mix(_e297, mix(_e298, _e299, vec4(_e301)), vec4(_e305));
                    } else {
                        if override_type_3_7 {
                            param_37 = 1u;
                            let _e308 = frag_tex_coord1_1;
                            param_38 = _e308;
                            param_39 = 1i;
                            let _e309 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_37), (&param_38), (&param_39));
                            let _e310 = frag_color1_;
                            color1_5 = (_e309 * _e310);
                            param_40 = 2u;
                            let _e312 = frag_tex_coord2_1;
                            param_41 = _e312;
                            param_42 = 2i;
                            let _e313 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_40), (&param_41), (&param_42));
                            let _e314 = frag_color2_;
                            color2_5 = (_e313 * _e314);
                            let _e316 = color2_5;
                            let _e318 = color2_5[3u];
                            let _e321 = color1_5;
                            let _e323 = color1_5[3u];
                            let _e327 = color0_;
                            base = (((_e316 + vec4(_e318)) * (_e321 + vec4(_e323))) * _e327);
                        } else {
                            param_43 = 1u;
                            let _e329 = frag_tex_coord1_1;
                            param_44 = _e329;
                            param_45 = 1i;
                            let _e330 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_43), (&param_44), (&param_45));
                            let _e331 = frag_color1_;
                            color1_6 = (_e330 * _e331);
                            param_46 = 2u;
                            let _e333 = frag_tex_coord2_1;
                            param_47 = _e333;
                            param_48 = 2i;
                            let _e334 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_46), (&param_47), (&param_48));
                            let _e335 = frag_color2_;
                            color2_6 = (_e334 * _e335);
                            let _e337 = color0_;
                            let _e339 = color1_6;
                            let _e342 = color2_6;
                            let _e344 = ((_e337.xyz * _e339.xyz) * _e342.xyz);
                            base[0u] = _e344.x;
                            base[1u] = _e344.y;
                            base[2u] = _e344.z;
                            let _e352 = color0_[3u];
                            let _e354 = color1_6[3u];
                            let _e357 = color2_6[3u];
                            base[3u] = ((_e352 * _e354) * _e357);
                        }
                    }
                }
            }
        }
    }
    let _e360 = wired_advanced_fog_enabled_u0028_();
    if _e360 {
        let _e361 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e361;
        let _e362 = base;
        let _e365 = unnamed.advancedFogColorDensity;
        let _e367 = fogAmount;
        let _e369 = mix(_e362.xyz, _e365.xyz, vec3(_e367));
        base[0u] = _e369.x;
        base[1u] = _e369.y;
        base[2u] = _e369.z;
    }
    if override_type_3_8 {
        let _e377 = base[3u];
        if (_e377 == 0f) {
            discard;
        }
    } else {
        if override_type_3_9 {
            let _e379 = base;
            let _e381 = base;
            if (dot(_e379.xyz, _e381.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e385 = base;
    out_color = _e385;
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
