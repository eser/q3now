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
@id(10) override acff: i32 = 0i;
override override_type_3_9: bool = (acff == 1i);
override override_type_3_10: bool = (acff == 2i);
override override_type_3_11: bool = (acff == 3i);
override override_type_3_12: bool = (acff == 1i);
override override_type_3_13: bool = (acff == 2i);
override override_type_3_14: bool = (acff == 3i);
@id(7) override discard_mode: i32 = 0i;
override override_type_3_15: bool = (discard_mode == 1i);
override override_type_3_16: bool = (discard_mode == 2i);
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

    let _e81 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e81 + 0.5f));
    let _e86 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e88 = fogType;
    let _e91 = fogType;
    return (((_e86 > 0.5f) && (_e88 >= 1i)) && (_e91 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e81 = wired_advanced_fog_enabled_u0028_();
    if !(_e81) {
        return 0f;
    }
    let _e84 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e84, 0.000001f));
    let _e89 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e89 + 0.5f));
    let _e92 = fogType_1;
    if (_e92 == 1i) {
        let _e96 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e96 <= 0f) {
            return 0f;
        }
        let _e98 = viewDepth;
        let _e101 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e98 / _e101), 0f, 1f);
    }
    let _e106 = unnamed.advancedFogColorDensity[3u];
    let _e108 = viewDepth;
    opticalDepth = (max(_e106, 0f) * _e108);
    let _e110 = fogType_1;
    if (_e110 == 2i) {
        let _e112 = opticalDepth;
        return clamp((1f - exp(-(_e112))), 0f, 1f);
    }
    let _e117 = opticalDepth;
    let _e118 = opticalDepth;
    return clamp((1f - exp(-((_e117 * _e118)))), 0f, 1f);
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e82 = (*c);
    (*c) = max(_e82, vec3<f32>(0f, 0f, 0f));
    let _e84 = (*c);
    cutoff = (_e84 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e86 = (*c);
    lo = (_e86 / vec3(12.92f));
    let _e89 = (*c);
    hi = pow(((_e89 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e94 = hi;
    let _e95 = lo;
    let _e96 = cutoff;
    return mix(_e94, _e95, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e96));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param: vec3<f32>;

    let _e83 = (*role);
    let _e85 = (*role);
    let _e90 = unnamed.packed_indices[(_e83 / 4u)][(_e85 % 4u)];
    let _e93 = (*role);
    let _e95 = (*role);
    let _e100 = unnamed.packed_indices[(_e93 / 4u)][(_e95 % 4u)];
    let _e105 = (*uv);
    let _e106 = textureSample(wired_bindless_images[(_e90 & 4095u)], wired_bindless_samplers[((_e100 >> bitcast<u32>(12i)) & 255u)], _e105);
    c_1 = _e106;
    let _e107 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e107))) == 0i) {
        let _e112 = c_1;
        param = _e112.xyz;
        let _e114 = sRGBToLinear_u0028_vf3_u003b((&param));
        c_1[0u] = _e114.x;
        c_1[1u] = _e114.y;
        c_1[2u] = _e114.z;
    }
    let _e121 = (*slot);
    if (lightmap_slot == (_e121 + 1i)) {
        let _e126 = unnamed.worldLightParams[0u];
        let _e127 = c_1;
        let _e129 = (_e127.xyz * _e126);
        c_1[0u] = _e129.x;
        c_1[1u] = _e129.y;
        c_1[2u] = _e129.z;
    }
    let _e136 = c_1;
    return _e136;
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
    var wetness: f32;
    var frost: f32;
    var luminance: f32;
    var fogAmount: f32;

    let _e153 = unnamed.packed_indices[0i][3u];
    let _e159 = unnamed.packed_indices[0i][3u];
    let _e164 = fog_tex_coord_1;
    let _e165 = textureSample(wired_bindless_images[(_e153 & 4095u)], wired_bindless_samplers[((_e159 >> bitcast<u32>(12i)) & 255u)], _e164);
    fog = _e165;
    let _e166 = frag_color0In_1;
    param_1 = _e166.xyz;
    let _e168 = sRGBToLinear_u0028_vf3_u003b((&param_1));
    let _e170 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e168.x, _e168.y, _e168.z, _e170);
    let _e175 = frag_color1In_1;
    param_2 = _e175.xyz;
    let _e177 = sRGBToLinear_u0028_vf3_u003b((&param_2));
    let _e179 = frag_color1In_1[3u];
    frag_color1_ = vec4<f32>(_e177.x, _e177.y, _e177.z, _e179);
    let _e184 = frag_color2In_1;
    param_3 = _e184.xyz;
    let _e186 = sRGBToLinear_u0028_vf3_u003b((&param_3));
    let _e188 = frag_color2In_1[3u];
    frag_color2_ = vec4<f32>(_e186.x, _e186.y, _e186.z, _e188);
    param_4 = 0u;
    let _e193 = frag_tex_coord0_1;
    param_5 = _e193;
    param_6 = 0i;
    let _e194 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_4), (&param_5), (&param_6));
    let _e195 = frag_color0_;
    color0_ = (_e194 * _e195);
    if override_type_3_2 {
        param_7 = 1u;
        let _e197 = frag_tex_coord1_1;
        param_8 = _e197;
        param_9 = 1i;
        let _e198 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_7), (&param_8), (&param_9));
        let _e199 = frag_color1_;
        color1_ = (_e198 * _e199);
        param_10 = 2u;
        let _e201 = frag_tex_coord2_1;
        param_11 = _e201;
        param_12 = 2i;
        let _e202 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_10), (&param_11), (&param_12));
        let _e203 = frag_color2_;
        color2_ = (_e202 * _e203);
        let _e205 = color0_;
        let _e207 = color1_;
        let _e210 = color2_;
        let _e212 = ((_e205.xyz + _e207.xyz) + _e210.xyz);
        let _e214 = color0_[3u];
        let _e216 = color1_[3u];
        let _e219 = color2_[3u];
        base = vec4<f32>(_e212.x, _e212.y, _e212.z, ((_e214 * _e216) * _e219));
    } else {
        if override_type_3_3 {
            param_13 = 1u;
            let _e225 = frag_tex_coord1_1;
            param_14 = _e225;
            param_15 = 1i;
            let _e226 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_13), (&param_14), (&param_15));
            let _e227 = frag_color1_;
            color1_1 = (_e226 * _e227);
            param_16 = 2u;
            let _e229 = frag_tex_coord2_1;
            param_17 = _e229;
            param_18 = 2i;
            let _e230 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_16), (&param_17), (&param_18));
            let _e231 = frag_color2_;
            color2_1 = (_e230 * _e231);
            let _e234 = color0_[3u];
            let _e235 = color0_;
            color0_ = (_e235 * _e234);
            let _e238 = color1_1[3u];
            let _e239 = color1_1;
            color1_1 = (_e239 * _e238);
            let _e242 = color2_1[3u];
            let _e243 = color2_1;
            color2_1 = (_e243 * _e242);
            let _e245 = color0_;
            let _e247 = color1_1;
            let _e250 = color2_1;
            let _e252 = ((_e245.xyz + _e247.xyz) + _e250.xyz);
            let _e254 = color0_[3u];
            let _e256 = color1_1[3u];
            let _e259 = color2_1[3u];
            base = vec4<f32>(_e252.x, _e252.y, _e252.z, ((_e254 * _e256) * _e259));
        } else {
            if override_type_3_4 {
                param_19 = 1u;
                let _e265 = frag_tex_coord1_1;
                param_20 = _e265;
                param_21 = 1i;
                let _e266 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_19), (&param_20), (&param_21));
                let _e267 = frag_color1_;
                color1_2 = (_e266 * _e267);
                param_22 = 2u;
                let _e269 = frag_tex_coord2_1;
                param_23 = _e269;
                param_24 = 2i;
                let _e270 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_22), (&param_23), (&param_24));
                let _e271 = frag_color2_;
                color2_2 = (_e270 * _e271);
                let _e274 = color0_[3u];
                let _e276 = color0_;
                color0_ = (_e276 * (1f - _e274));
                let _e279 = color1_2[3u];
                let _e281 = color1_2;
                color1_2 = (_e281 * (1f - _e279));
                let _e284 = color2_2[3u];
                let _e286 = color2_2;
                color2_2 = (_e286 * (1f - _e284));
                let _e288 = color0_;
                let _e290 = color1_2;
                let _e293 = color2_2;
                let _e295 = ((_e288.xyz + _e290.xyz) + _e293.xyz);
                let _e297 = color0_[3u];
                let _e299 = color1_2[3u];
                let _e302 = color2_2[3u];
                base = vec4<f32>(_e295.x, _e295.y, _e295.z, ((_e297 * _e299) * _e302));
            } else {
                if override_type_3_5 {
                    param_25 = 1u;
                    let _e308 = frag_tex_coord1_1;
                    param_26 = _e308;
                    param_27 = 1i;
                    let _e309 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_25), (&param_26), (&param_27));
                    let _e310 = frag_color1_;
                    color1_3 = (_e309 * _e310);
                    param_28 = 2u;
                    let _e312 = frag_tex_coord2_1;
                    param_29 = _e312;
                    param_30 = 2i;
                    let _e313 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_28), (&param_29), (&param_30));
                    let _e314 = frag_color2_;
                    color2_3 = (_e313 * _e314);
                    let _e316 = color0_;
                    let _e317 = color1_3;
                    let _e319 = color1_3[3u];
                    let _e322 = color2_3;
                    let _e324 = color2_3[3u];
                    base = mix(mix(_e316, _e317, vec4(_e319)), _e322, vec4(_e324));
                } else {
                    if override_type_3_6 {
                        param_31 = 1u;
                        let _e327 = frag_tex_coord1_1;
                        param_32 = _e327;
                        param_33 = 1i;
                        let _e328 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_31), (&param_32), (&param_33));
                        let _e329 = frag_color1_;
                        color1_4 = (_e328 * _e329);
                        param_34 = 2u;
                        let _e331 = frag_tex_coord2_1;
                        param_35 = _e331;
                        param_36 = 2i;
                        let _e332 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_34), (&param_35), (&param_36));
                        let _e333 = frag_color2_;
                        color2_4 = (_e332 * _e333);
                        let _e335 = color2_4;
                        let _e336 = color1_4;
                        let _e337 = color0_;
                        let _e339 = color1_4[3u];
                        let _e343 = color2_4[3u];
                        base = mix(_e335, mix(_e336, _e337, vec4(_e339)), vec4(_e343));
                    } else {
                        if override_type_3_7 {
                            param_37 = 1u;
                            let _e346 = frag_tex_coord1_1;
                            param_38 = _e346;
                            param_39 = 1i;
                            let _e347 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_37), (&param_38), (&param_39));
                            let _e348 = frag_color1_;
                            color1_5 = (_e347 * _e348);
                            param_40 = 2u;
                            let _e350 = frag_tex_coord2_1;
                            param_41 = _e350;
                            param_42 = 2i;
                            let _e351 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_40), (&param_41), (&param_42));
                            let _e352 = frag_color2_;
                            color2_5 = (_e351 * _e352);
                            let _e354 = color2_5;
                            let _e356 = color2_5[3u];
                            let _e359 = color1_5;
                            let _e361 = color1_5[3u];
                            let _e365 = color0_;
                            base = (((_e354 + vec4(_e356)) * (_e359 + vec4(_e361))) * _e365);
                        } else {
                            param_43 = 1u;
                            let _e367 = frag_tex_coord1_1;
                            param_44 = _e367;
                            param_45 = 1i;
                            let _e368 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_43), (&param_44), (&param_45));
                            let _e369 = frag_color1_;
                            color1_6 = (_e368 * _e369);
                            param_46 = 2u;
                            let _e371 = frag_tex_coord2_1;
                            param_47 = _e371;
                            param_48 = 2i;
                            let _e372 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_46), (&param_47), (&param_48));
                            let _e373 = frag_color2_;
                            color2_6 = (_e372 * _e373);
                            let _e375 = color0_;
                            let _e377 = color1_6;
                            let _e380 = color2_6;
                            let _e382 = ((_e375.xyz * _e377.xyz) * _e380.xyz);
                            base[0u] = _e382.x;
                            base[1u] = _e382.y;
                            base[2u] = _e382.z;
                            let _e390 = color0_[3u];
                            let _e392 = color1_6[3u];
                            let _e395 = color2_6[3u];
                            base[3u] = ((_e390 * _e392) * _e395);
                        }
                    }
                }
            }
        }
    }
    if override_type_3_8 {
        let _e400 = unnamed.worldLightParams[1u];
        wetness = clamp(_e400, 0f, 1f);
        let _e404 = unnamed.worldLightParams[2u];
        frost = clamp(_e404, 0f, 1f);
        let _e406 = base;
        luminance = dot(_e406.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e409 = wetness;
        let _e411 = base;
        let _e413 = (_e411.xyz * mix(1f, 0.82f, _e409));
        base[0u] = _e413.x;
        base[1u] = _e413.y;
        base[2u] = _e413.z;
        let _e420 = base;
        let _e422 = luminance;
        let _e424 = luminance;
        let _e426 = luminance;
        let _e428 = frost;
        let _e431 = mix(_e420.xyz, vec3<f32>((_e422 * 0.88f), (_e424 * 0.94f), _e426), vec3((_e428 * 0.55f)));
        base[0u] = _e431.x;
        base[1u] = _e431.y;
        base[2u] = _e431.z;
    }
    let _e438 = color0_;
    let _e441 = unnamed.emissionRadiance;
    let _e444 = base;
    let _e446 = (_e444.xyz + (_e438.xyz * _e441.xyz));
    base[0u] = _e446.x;
    base[1u] = _e446.y;
    base[2u] = _e446.z;
    let _e453 = wired_advanced_fog_enabled_u0028_();
    if _e453 {
        let _e454 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e454;
        if override_type_3_9 {
            let _e455 = fogAmount;
            let _e457 = base;
            let _e459 = (_e457.xyz * (1f - _e455));
            base[0u] = _e459.x;
            base[1u] = _e459.y;
            base[2u] = _e459.z;
        } else {
            if override_type_3_10 {
                let _e466 = fogAmount;
                let _e468 = base;
                base = (_e468 * (1f - _e466));
            } else {
                if override_type_3_11 {
                    let _e470 = fogAmount;
                    let _e473 = base[3u];
                    base[3u] = (_e473 * (1f - _e470));
                } else {
                    let _e476 = base;
                    let _e479 = unnamed.advancedFogColorDensity;
                    let _e481 = fogAmount;
                    let _e483 = mix(_e476.xyz, _e479.xyz, vec3(_e481));
                    base[0u] = _e483.x;
                    base[1u] = _e483.y;
                    base[2u] = _e483.z;
                }
            }
        }
    } else {
        if override_type_3_12 {
            let _e490 = base;
            let _e493 = fog[3u];
            let _e495 = (_e490.xyz * (1f - _e493));
            base[0u] = _e495.x;
            base[1u] = _e495.y;
            base[2u] = _e495.z;
        } else {
            if override_type_3_13 {
                let _e502 = base;
                let _e504 = fog[3u];
                base = (_e502 * (1f - _e504));
            } else {
                if override_type_3_14 {
                    let _e508 = base[3u];
                    let _e510 = fog[3u];
                    base[3u] = (_e508 * (1f - _e510));
                } else {
                    let _e514 = base;
                    let _e515 = fog;
                    let _e517 = unnamed.fogColor;
                    let _e520 = fog[3u];
                    base = mix(_e514, (_e515 * _e517), vec4(_e520));
                }
            }
        }
    }
    if override_type_3_15 {
        let _e524 = base[3u];
        if (_e524 == 0f) {
            discard;
        }
    } else {
        if override_type_3_16 {
            let _e526 = base;
            let _e528 = base;
            if (dot(_e526.xyz, _e528.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e532 = base;
    out_color = _e532;
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
