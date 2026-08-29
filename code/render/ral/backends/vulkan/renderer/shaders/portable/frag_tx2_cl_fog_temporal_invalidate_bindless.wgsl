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

struct FragmentOutput {
    @location(1) member: vec2<f32>,
    @location(2) member_1: f32,
    @location(0) member_2: vec4<f32>,
}

@id(4) override tex_domain: i32 = 0i;
@id(26) override lightmap_slot: i32 = 0i;
@id(6) override tex_mode: i32 = 0i;
override override_type_4_: bool = (tex_mode == 1i);
override override_type_4_1: bool = (tex_mode == 2i);
override override_type_4_2: bool = (override_type_4_ || override_type_4_1);
override override_type_4_3: bool = (tex_mode == 3i);
override override_type_4_4: bool = (tex_mode == 4i);
override override_type_4_5: bool = (tex_mode == 5i);
override override_type_4_6: bool = (tex_mode == 6i);
override override_type_4_7: bool = (tex_mode == 7i);
override override_type_4_8: bool = (lightmap_slot != 0i);
@id(10) override acff: i32 = 0i;
override override_type_4_9: bool = (acff == 1i);
override override_type_4_10: bool = (acff == 2i);
override override_type_4_11: bool = (acff == 3i);
override override_type_4_12: bool = (acff == 1i);
override override_type_4_13: bool = (acff == 2i);
override override_type_4_14: bool = (acff == 3i);
@id(7) override discard_mode: i32 = 0i;
override override_type_4_15: bool = (discard_mode == 1i);
override override_type_4_16: bool = (discard_mode == 2i);
@id(3) override alpha_to_coverage: i32 = 0i;

var<private> out_temporal_velocity: vec2<f32>;
var<private> out_temporal_validity: f32;
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
var<private> temporalCurrentClip_1: vec4<f32>;
var<private> temporalPreviousClip_1: vec4<f32>;
var<private> temporalOutcome_1: u32;

fn wiredTemporalWriteAux_u0028_f1_u003b(coverageConfidence: ptr<function, f32>) {
    out_temporal_velocity = vec2<f32>(0f, 0f);
    out_temporal_validity = 0f;
    return;
}

fn wired_advanced_fog_enabled_u0028_() -> bool {
    var fogType: i32;

    let _e87 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e87 + 0.5f));
    let _e92 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e94 = fogType;
    let _e97 = fogType;
    return (((_e92 > 0.5f) && (_e94 >= 1i)) && (_e97 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e87 = wired_advanced_fog_enabled_u0028_();
    if !(_e87) {
        return 0f;
    }
    let _e90 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e90, 0.000001f));
    let _e95 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e95 + 0.5f));
    let _e98 = fogType_1;
    if (_e98 == 1i) {
        let _e102 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e102 <= 0f) {
            return 0f;
        }
        let _e104 = viewDepth;
        let _e107 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e104 / _e107), 0f, 1f);
    }
    let _e112 = unnamed.advancedFogColorDensity[3u];
    let _e114 = viewDepth;
    opticalDepth = (max(_e112, 0f) * _e114);
    let _e116 = fogType_1;
    if (_e116 == 2i) {
        let _e118 = opticalDepth;
        return clamp((1f - exp(-(_e118))), 0f, 1f);
    }
    let _e123 = opticalDepth;
    let _e124 = opticalDepth;
    return clamp((1f - exp(-((_e123 * _e124)))), 0f, 1f);
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e88 = (*c);
    (*c) = max(_e88, vec3<f32>(0f, 0f, 0f));
    let _e90 = (*c);
    cutoff = (_e90 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e92 = (*c);
    lo = (_e92 / vec3(12.92f));
    let _e95 = (*c);
    hi = pow(((_e95 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e100 = hi;
    let _e101 = lo;
    let _e102 = cutoff;
    return mix(_e100, _e101, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e102));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param: vec3<f32>;

    let _e89 = (*role);
    let _e91 = (*role);
    let _e96 = unnamed.packed_indices[(_e89 / 4u)][(_e91 % 4u)];
    let _e99 = (*role);
    let _e101 = (*role);
    let _e106 = unnamed.packed_indices[(_e99 / 4u)][(_e101 % 4u)];
    let _e111 = (*uv);
    let _e112 = textureSample(wired_bindless_images[(_e96 & 4095u)], wired_bindless_samplers[((_e106 >> bitcast<u32>(12i)) & 255u)], _e111);
    c_1 = _e112;
    let _e113 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e113))) == 0i) {
        let _e118 = c_1;
        param = _e118.xyz;
        let _e120 = sRGBToLinear_u0028_vf3_u003b((&param));
        c_1[0u] = _e120.x;
        c_1[1u] = _e120.y;
        c_1[2u] = _e120.z;
    }
    let _e127 = (*slot);
    if (lightmap_slot == (_e127 + 1i)) {
        let _e132 = unnamed.worldLightParams[0u];
        let _e133 = c_1;
        let _e135 = (_e133.xyz * _e132);
        c_1[0u] = _e135.x;
        c_1[1u] = _e135.y;
        c_1[2u] = _e135.z;
    }
    let _e142 = c_1;
    return _e142;
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
    var param_49: f32;

    let _e160 = unnamed.packed_indices[0i][3u];
    let _e166 = unnamed.packed_indices[0i][3u];
    let _e171 = fog_tex_coord_1;
    let _e172 = textureSample(wired_bindless_images[(_e160 & 4095u)], wired_bindless_samplers[((_e166 >> bitcast<u32>(12i)) & 255u)], _e171);
    fog = _e172;
    let _e173 = frag_color0In_1;
    param_1 = _e173.xyz;
    let _e175 = sRGBToLinear_u0028_vf3_u003b((&param_1));
    let _e177 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e175.x, _e175.y, _e175.z, _e177);
    let _e182 = frag_color1In_1;
    param_2 = _e182.xyz;
    let _e184 = sRGBToLinear_u0028_vf3_u003b((&param_2));
    let _e186 = frag_color1In_1[3u];
    frag_color1_ = vec4<f32>(_e184.x, _e184.y, _e184.z, _e186);
    let _e191 = frag_color2In_1;
    param_3 = _e191.xyz;
    let _e193 = sRGBToLinear_u0028_vf3_u003b((&param_3));
    let _e195 = frag_color2In_1[3u];
    frag_color2_ = vec4<f32>(_e193.x, _e193.y, _e193.z, _e195);
    param_4 = 0u;
    let _e200 = frag_tex_coord0_1;
    param_5 = _e200;
    param_6 = 0i;
    let _e201 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_4), (&param_5), (&param_6));
    let _e202 = frag_color0_;
    color0_ = (_e201 * _e202);
    if override_type_4_2 {
        param_7 = 1u;
        let _e204 = frag_tex_coord1_1;
        param_8 = _e204;
        param_9 = 1i;
        let _e205 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_7), (&param_8), (&param_9));
        let _e206 = frag_color1_;
        color1_ = (_e205 * _e206);
        param_10 = 2u;
        let _e208 = frag_tex_coord2_1;
        param_11 = _e208;
        param_12 = 2i;
        let _e209 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_10), (&param_11), (&param_12));
        let _e210 = frag_color2_;
        color2_ = (_e209 * _e210);
        let _e212 = color0_;
        let _e214 = color1_;
        let _e217 = color2_;
        let _e219 = ((_e212.xyz + _e214.xyz) + _e217.xyz);
        let _e221 = color0_[3u];
        let _e223 = color1_[3u];
        let _e226 = color2_[3u];
        base = vec4<f32>(_e219.x, _e219.y, _e219.z, ((_e221 * _e223) * _e226));
    } else {
        if override_type_4_3 {
            param_13 = 1u;
            let _e232 = frag_tex_coord1_1;
            param_14 = _e232;
            param_15 = 1i;
            let _e233 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_13), (&param_14), (&param_15));
            let _e234 = frag_color1_;
            color1_1 = (_e233 * _e234);
            param_16 = 2u;
            let _e236 = frag_tex_coord2_1;
            param_17 = _e236;
            param_18 = 2i;
            let _e237 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_16), (&param_17), (&param_18));
            let _e238 = frag_color2_;
            color2_1 = (_e237 * _e238);
            let _e241 = color0_[3u];
            let _e242 = color0_;
            color0_ = (_e242 * _e241);
            let _e245 = color1_1[3u];
            let _e246 = color1_1;
            color1_1 = (_e246 * _e245);
            let _e249 = color2_1[3u];
            let _e250 = color2_1;
            color2_1 = (_e250 * _e249);
            let _e252 = color0_;
            let _e254 = color1_1;
            let _e257 = color2_1;
            let _e259 = ((_e252.xyz + _e254.xyz) + _e257.xyz);
            let _e261 = color0_[3u];
            let _e263 = color1_1[3u];
            let _e266 = color2_1[3u];
            base = vec4<f32>(_e259.x, _e259.y, _e259.z, ((_e261 * _e263) * _e266));
        } else {
            if override_type_4_4 {
                param_19 = 1u;
                let _e272 = frag_tex_coord1_1;
                param_20 = _e272;
                param_21 = 1i;
                let _e273 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_19), (&param_20), (&param_21));
                let _e274 = frag_color1_;
                color1_2 = (_e273 * _e274);
                param_22 = 2u;
                let _e276 = frag_tex_coord2_1;
                param_23 = _e276;
                param_24 = 2i;
                let _e277 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_22), (&param_23), (&param_24));
                let _e278 = frag_color2_;
                color2_2 = (_e277 * _e278);
                let _e281 = color0_[3u];
                let _e283 = color0_;
                color0_ = (_e283 * (1f - _e281));
                let _e286 = color1_2[3u];
                let _e288 = color1_2;
                color1_2 = (_e288 * (1f - _e286));
                let _e291 = color2_2[3u];
                let _e293 = color2_2;
                color2_2 = (_e293 * (1f - _e291));
                let _e295 = color0_;
                let _e297 = color1_2;
                let _e300 = color2_2;
                let _e302 = ((_e295.xyz + _e297.xyz) + _e300.xyz);
                let _e304 = color0_[3u];
                let _e306 = color1_2[3u];
                let _e309 = color2_2[3u];
                base = vec4<f32>(_e302.x, _e302.y, _e302.z, ((_e304 * _e306) * _e309));
            } else {
                if override_type_4_5 {
                    param_25 = 1u;
                    let _e315 = frag_tex_coord1_1;
                    param_26 = _e315;
                    param_27 = 1i;
                    let _e316 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_25), (&param_26), (&param_27));
                    let _e317 = frag_color1_;
                    color1_3 = (_e316 * _e317);
                    param_28 = 2u;
                    let _e319 = frag_tex_coord2_1;
                    param_29 = _e319;
                    param_30 = 2i;
                    let _e320 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_28), (&param_29), (&param_30));
                    let _e321 = frag_color2_;
                    color2_3 = (_e320 * _e321);
                    let _e323 = color0_;
                    let _e324 = color1_3;
                    let _e326 = color1_3[3u];
                    let _e329 = color2_3;
                    let _e331 = color2_3[3u];
                    base = mix(mix(_e323, _e324, vec4(_e326)), _e329, vec4(_e331));
                } else {
                    if override_type_4_6 {
                        param_31 = 1u;
                        let _e334 = frag_tex_coord1_1;
                        param_32 = _e334;
                        param_33 = 1i;
                        let _e335 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_31), (&param_32), (&param_33));
                        let _e336 = frag_color1_;
                        color1_4 = (_e335 * _e336);
                        param_34 = 2u;
                        let _e338 = frag_tex_coord2_1;
                        param_35 = _e338;
                        param_36 = 2i;
                        let _e339 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_34), (&param_35), (&param_36));
                        let _e340 = frag_color2_;
                        color2_4 = (_e339 * _e340);
                        let _e342 = color2_4;
                        let _e343 = color1_4;
                        let _e344 = color0_;
                        let _e346 = color1_4[3u];
                        let _e350 = color2_4[3u];
                        base = mix(_e342, mix(_e343, _e344, vec4(_e346)), vec4(_e350));
                    } else {
                        if override_type_4_7 {
                            param_37 = 1u;
                            let _e353 = frag_tex_coord1_1;
                            param_38 = _e353;
                            param_39 = 1i;
                            let _e354 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_37), (&param_38), (&param_39));
                            let _e355 = frag_color1_;
                            color1_5 = (_e354 * _e355);
                            param_40 = 2u;
                            let _e357 = frag_tex_coord2_1;
                            param_41 = _e357;
                            param_42 = 2i;
                            let _e358 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_40), (&param_41), (&param_42));
                            let _e359 = frag_color2_;
                            color2_5 = (_e358 * _e359);
                            let _e361 = color2_5;
                            let _e363 = color2_5[3u];
                            let _e366 = color1_5;
                            let _e368 = color1_5[3u];
                            let _e372 = color0_;
                            base = (((_e361 + vec4(_e363)) * (_e366 + vec4(_e368))) * _e372);
                        } else {
                            param_43 = 1u;
                            let _e374 = frag_tex_coord1_1;
                            param_44 = _e374;
                            param_45 = 1i;
                            let _e375 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_43), (&param_44), (&param_45));
                            let _e376 = frag_color1_;
                            color1_6 = (_e375 * _e376);
                            param_46 = 2u;
                            let _e378 = frag_tex_coord2_1;
                            param_47 = _e378;
                            param_48 = 2i;
                            let _e379 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_46), (&param_47), (&param_48));
                            let _e380 = frag_color2_;
                            color2_6 = (_e379 * _e380);
                            let _e382 = color0_;
                            let _e384 = color1_6;
                            let _e387 = color2_6;
                            let _e389 = ((_e382.xyz * _e384.xyz) * _e387.xyz);
                            base[0u] = _e389.x;
                            base[1u] = _e389.y;
                            base[2u] = _e389.z;
                            let _e397 = color0_[3u];
                            let _e399 = color1_6[3u];
                            let _e402 = color2_6[3u];
                            base[3u] = ((_e397 * _e399) * _e402);
                        }
                    }
                }
            }
        }
    }
    if override_type_4_8 {
        let _e407 = unnamed.worldLightParams[1u];
        wetness = clamp(_e407, 0f, 1f);
        let _e411 = unnamed.worldLightParams[2u];
        frost = clamp(_e411, 0f, 1f);
        let _e413 = base;
        luminance = dot(_e413.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e416 = wetness;
        let _e418 = base;
        let _e420 = (_e418.xyz * mix(1f, 0.82f, _e416));
        base[0u] = _e420.x;
        base[1u] = _e420.y;
        base[2u] = _e420.z;
        let _e427 = base;
        let _e429 = luminance;
        let _e431 = luminance;
        let _e433 = luminance;
        let _e435 = frost;
        let _e438 = mix(_e427.xyz, vec3<f32>((_e429 * 0.88f), (_e431 * 0.94f), _e433), vec3((_e435 * 0.55f)));
        base[0u] = _e438.x;
        base[1u] = _e438.y;
        base[2u] = _e438.z;
    }
    let _e445 = color0_;
    let _e448 = unnamed.emissionRadiance;
    let _e451 = base;
    let _e453 = (_e451.xyz + (_e445.xyz * _e448.xyz));
    base[0u] = _e453.x;
    base[1u] = _e453.y;
    base[2u] = _e453.z;
    let _e460 = wired_advanced_fog_enabled_u0028_();
    if _e460 {
        let _e461 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e461;
        if override_type_4_9 {
            let _e462 = fogAmount;
            let _e464 = base;
            let _e466 = (_e464.xyz * (1f - _e462));
            base[0u] = _e466.x;
            base[1u] = _e466.y;
            base[2u] = _e466.z;
        } else {
            if override_type_4_10 {
                let _e473 = fogAmount;
                let _e475 = base;
                base = (_e475 * (1f - _e473));
            } else {
                if override_type_4_11 {
                    let _e477 = fogAmount;
                    let _e480 = base[3u];
                    base[3u] = (_e480 * (1f - _e477));
                } else {
                    let _e483 = base;
                    let _e486 = unnamed.advancedFogColorDensity;
                    let _e488 = fogAmount;
                    let _e490 = mix(_e483.xyz, _e486.xyz, vec3(_e488));
                    base[0u] = _e490.x;
                    base[1u] = _e490.y;
                    base[2u] = _e490.z;
                }
            }
        }
    } else {
        if override_type_4_12 {
            let _e497 = base;
            let _e500 = fog[3u];
            let _e502 = (_e497.xyz * (1f - _e500));
            base[0u] = _e502.x;
            base[1u] = _e502.y;
            base[2u] = _e502.z;
        } else {
            if override_type_4_13 {
                let _e509 = base;
                let _e511 = fog[3u];
                base = (_e509 * (1f - _e511));
            } else {
                if override_type_4_14 {
                    let _e515 = base[3u];
                    let _e517 = fog[3u];
                    base[3u] = (_e515 * (1f - _e517));
                } else {
                    let _e521 = base;
                    let _e522 = fog;
                    let _e524 = unnamed.fogColor;
                    let _e527 = fog[3u];
                    base = mix(_e521, (_e522 * _e524), vec4(_e527));
                }
            }
        }
    }
    if override_type_4_15 {
        let _e531 = base[3u];
        if (_e531 == 0f) {
            discard;
        }
    } else {
        if override_type_4_16 {
            let _e533 = base;
            let _e535 = base;
            if (dot(_e533.xyz, _e535.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e539 = base;
    out_color = _e539;
    param_49 = 1f;
    wiredTemporalWriteAux_u0028_f1_u003b((&param_49));
    return;
}

@fragment
fn main(@builtin(position) gl_FragCoord: vec4<f32>, @location(4) fog_tex_coord: vec2<f32>, @location(0) frag_color0In: vec4<f32>, @location(5) frag_color1In: vec4<f32>, @location(6) frag_color2In: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>, @location(3) frag_tex_coord2_: vec2<f32>, @location(10) temporalCurrentClip: vec4<f32>, @location(11) temporalPreviousClip: vec4<f32>, @location(12) @interpolate(flat) temporalOutcome: u32) -> FragmentOutput {
    gl_FragCoord_1 = gl_FragCoord;
    fog_tex_coord_1 = fog_tex_coord;
    frag_color0In_1 = frag_color0In;
    frag_color1In_1 = frag_color1In;
    frag_color2In_1 = frag_color2In;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    frag_tex_coord2_1 = frag_tex_coord2_;
    temporalCurrentClip_1 = temporalCurrentClip;
    temporalPreviousClip_1 = temporalPreviousClip;
    temporalOutcome_1 = temporalOutcome;
    main_1();
    let _e25 = out_temporal_velocity;
    let _e26 = out_temporal_validity;
    let _e27 = out_color;
    return FragmentOutput(_e25, _e26, _e27);
}
