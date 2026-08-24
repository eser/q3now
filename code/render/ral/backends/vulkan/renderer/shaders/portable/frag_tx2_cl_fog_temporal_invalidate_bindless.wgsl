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
@id(10) override acff: i32 = 0i;
override override_type_4_8: bool = (acff == 1i);
override override_type_4_9: bool = (acff == 2i);
override override_type_4_10: bool = (acff == 3i);
override override_type_4_11: bool = (acff == 1i);
override override_type_4_12: bool = (acff == 2i);
override override_type_4_13: bool = (acff == 3i);
@id(7) override discard_mode: i32 = 0i;
override override_type_4_14: bool = (discard_mode == 1i);
override override_type_4_15: bool = (discard_mode == 2i);
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

    let _e77 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e77 + 0.5f));
    let _e82 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e84 = fogType;
    let _e87 = fogType;
    return (((_e82 > 0.5f) && (_e84 >= 1i)) && (_e87 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e77 = wired_advanced_fog_enabled_u0028_();
    if !(_e77) {
        return 0f;
    }
    let _e80 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e80, 0.000001f));
    let _e85 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e85 + 0.5f));
    let _e88 = fogType_1;
    if (_e88 == 1i) {
        let _e92 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e92 <= 0f) {
            return 0f;
        }
        let _e94 = viewDepth;
        let _e97 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e94 / _e97), 0f, 1f);
    }
    let _e102 = unnamed.advancedFogColorDensity[3u];
    let _e104 = viewDepth;
    opticalDepth = (max(_e102, 0f) * _e104);
    let _e106 = fogType_1;
    if (_e106 == 2i) {
        let _e108 = opticalDepth;
        return clamp((1f - exp(-(_e108))), 0f, 1f);
    }
    let _e113 = opticalDepth;
    let _e114 = opticalDepth;
    return clamp((1f - exp(-((_e113 * _e114)))), 0f, 1f);
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e78 = (*c);
    (*c) = max(_e78, vec3<f32>(0f, 0f, 0f));
    let _e80 = (*c);
    cutoff = (_e80 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e82 = (*c);
    lo = (_e82 / vec3(12.92f));
    let _e85 = (*c);
    hi = pow(((_e85 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e90 = hi;
    let _e91 = lo;
    let _e92 = cutoff;
    return mix(_e90, _e91, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e92));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param: vec3<f32>;

    let _e79 = (*role);
    let _e81 = (*role);
    let _e86 = unnamed.packed_indices[(_e79 / 4u)][(_e81 % 4u)];
    let _e89 = (*role);
    let _e91 = (*role);
    let _e96 = unnamed.packed_indices[(_e89 / 4u)][(_e91 % 4u)];
    let _e101 = (*uv);
    let _e102 = textureSample(wired_bindless_images[(_e86 & 4095u)], wired_bindless_samplers[((_e96 >> bitcast<u32>(12i)) & 255u)], _e101);
    c_1 = _e102;
    let _e103 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e103))) == 0i) {
        let _e108 = c_1;
        param = _e108.xyz;
        let _e110 = sRGBToLinear_u0028_vf3_u003b((&param));
        c_1[0u] = _e110.x;
        c_1[1u] = _e110.y;
        c_1[2u] = _e110.z;
    }
    let _e117 = (*slot);
    if (lightmap_slot == (_e117 + 1i)) {
        let _e122 = unnamed.worldLightParams[0u];
        let _e123 = c_1;
        let _e125 = (_e123.xyz * _e122);
        c_1[0u] = _e125.x;
        c_1[1u] = _e125.y;
        c_1[2u] = _e125.z;
    }
    let _e132 = c_1;
    return _e132;
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
    var param_49: f32;

    let _e147 = unnamed.packed_indices[0i][3u];
    let _e153 = unnamed.packed_indices[0i][3u];
    let _e158 = fog_tex_coord_1;
    let _e159 = textureSample(wired_bindless_images[(_e147 & 4095u)], wired_bindless_samplers[((_e153 >> bitcast<u32>(12i)) & 255u)], _e158);
    fog = _e159;
    let _e160 = frag_color0In_1;
    param_1 = _e160.xyz;
    let _e162 = sRGBToLinear_u0028_vf3_u003b((&param_1));
    let _e164 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e162.x, _e162.y, _e162.z, _e164);
    let _e169 = frag_color1In_1;
    param_2 = _e169.xyz;
    let _e171 = sRGBToLinear_u0028_vf3_u003b((&param_2));
    let _e173 = frag_color1In_1[3u];
    frag_color1_ = vec4<f32>(_e171.x, _e171.y, _e171.z, _e173);
    let _e178 = frag_color2In_1;
    param_3 = _e178.xyz;
    let _e180 = sRGBToLinear_u0028_vf3_u003b((&param_3));
    let _e182 = frag_color2In_1[3u];
    frag_color2_ = vec4<f32>(_e180.x, _e180.y, _e180.z, _e182);
    param_4 = 0u;
    let _e187 = frag_tex_coord0_1;
    param_5 = _e187;
    param_6 = 0i;
    let _e188 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_4), (&param_5), (&param_6));
    let _e189 = frag_color0_;
    color0_ = (_e188 * _e189);
    if override_type_4_2 {
        param_7 = 1u;
        let _e191 = frag_tex_coord1_1;
        param_8 = _e191;
        param_9 = 1i;
        let _e192 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_7), (&param_8), (&param_9));
        let _e193 = frag_color1_;
        color1_ = (_e192 * _e193);
        param_10 = 2u;
        let _e195 = frag_tex_coord2_1;
        param_11 = _e195;
        param_12 = 2i;
        let _e196 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_10), (&param_11), (&param_12));
        let _e197 = frag_color2_;
        color2_ = (_e196 * _e197);
        let _e199 = color0_;
        let _e201 = color1_;
        let _e204 = color2_;
        let _e206 = ((_e199.xyz + _e201.xyz) + _e204.xyz);
        let _e208 = color0_[3u];
        let _e210 = color1_[3u];
        let _e213 = color2_[3u];
        base = vec4<f32>(_e206.x, _e206.y, _e206.z, ((_e208 * _e210) * _e213));
    } else {
        if override_type_4_3 {
            param_13 = 1u;
            let _e219 = frag_tex_coord1_1;
            param_14 = _e219;
            param_15 = 1i;
            let _e220 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_13), (&param_14), (&param_15));
            let _e221 = frag_color1_;
            color1_1 = (_e220 * _e221);
            param_16 = 2u;
            let _e223 = frag_tex_coord2_1;
            param_17 = _e223;
            param_18 = 2i;
            let _e224 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_16), (&param_17), (&param_18));
            let _e225 = frag_color2_;
            color2_1 = (_e224 * _e225);
            let _e228 = color0_[3u];
            let _e229 = color0_;
            color0_ = (_e229 * _e228);
            let _e232 = color1_1[3u];
            let _e233 = color1_1;
            color1_1 = (_e233 * _e232);
            let _e236 = color2_1[3u];
            let _e237 = color2_1;
            color2_1 = (_e237 * _e236);
            let _e239 = color0_;
            let _e241 = color1_1;
            let _e244 = color2_1;
            let _e246 = ((_e239.xyz + _e241.xyz) + _e244.xyz);
            let _e248 = color0_[3u];
            let _e250 = color1_1[3u];
            let _e253 = color2_1[3u];
            base = vec4<f32>(_e246.x, _e246.y, _e246.z, ((_e248 * _e250) * _e253));
        } else {
            if override_type_4_4 {
                param_19 = 1u;
                let _e259 = frag_tex_coord1_1;
                param_20 = _e259;
                param_21 = 1i;
                let _e260 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_19), (&param_20), (&param_21));
                let _e261 = frag_color1_;
                color1_2 = (_e260 * _e261);
                param_22 = 2u;
                let _e263 = frag_tex_coord2_1;
                param_23 = _e263;
                param_24 = 2i;
                let _e264 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_22), (&param_23), (&param_24));
                let _e265 = frag_color2_;
                color2_2 = (_e264 * _e265);
                let _e268 = color0_[3u];
                let _e270 = color0_;
                color0_ = (_e270 * (1f - _e268));
                let _e273 = color1_2[3u];
                let _e275 = color1_2;
                color1_2 = (_e275 * (1f - _e273));
                let _e278 = color2_2[3u];
                let _e280 = color2_2;
                color2_2 = (_e280 * (1f - _e278));
                let _e282 = color0_;
                let _e284 = color1_2;
                let _e287 = color2_2;
                let _e289 = ((_e282.xyz + _e284.xyz) + _e287.xyz);
                let _e291 = color0_[3u];
                let _e293 = color1_2[3u];
                let _e296 = color2_2[3u];
                base = vec4<f32>(_e289.x, _e289.y, _e289.z, ((_e291 * _e293) * _e296));
            } else {
                if override_type_4_5 {
                    param_25 = 1u;
                    let _e302 = frag_tex_coord1_1;
                    param_26 = _e302;
                    param_27 = 1i;
                    let _e303 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_25), (&param_26), (&param_27));
                    let _e304 = frag_color1_;
                    color1_3 = (_e303 * _e304);
                    param_28 = 2u;
                    let _e306 = frag_tex_coord2_1;
                    param_29 = _e306;
                    param_30 = 2i;
                    let _e307 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_28), (&param_29), (&param_30));
                    let _e308 = frag_color2_;
                    color2_3 = (_e307 * _e308);
                    let _e310 = color0_;
                    let _e311 = color1_3;
                    let _e313 = color1_3[3u];
                    let _e316 = color2_3;
                    let _e318 = color2_3[3u];
                    base = mix(mix(_e310, _e311, vec4(_e313)), _e316, vec4(_e318));
                } else {
                    if override_type_4_6 {
                        param_31 = 1u;
                        let _e321 = frag_tex_coord1_1;
                        param_32 = _e321;
                        param_33 = 1i;
                        let _e322 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_31), (&param_32), (&param_33));
                        let _e323 = frag_color1_;
                        color1_4 = (_e322 * _e323);
                        param_34 = 2u;
                        let _e325 = frag_tex_coord2_1;
                        param_35 = _e325;
                        param_36 = 2i;
                        let _e326 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_34), (&param_35), (&param_36));
                        let _e327 = frag_color2_;
                        color2_4 = (_e326 * _e327);
                        let _e329 = color2_4;
                        let _e330 = color1_4;
                        let _e331 = color0_;
                        let _e333 = color1_4[3u];
                        let _e337 = color2_4[3u];
                        base = mix(_e329, mix(_e330, _e331, vec4(_e333)), vec4(_e337));
                    } else {
                        if override_type_4_7 {
                            param_37 = 1u;
                            let _e340 = frag_tex_coord1_1;
                            param_38 = _e340;
                            param_39 = 1i;
                            let _e341 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_37), (&param_38), (&param_39));
                            let _e342 = frag_color1_;
                            color1_5 = (_e341 * _e342);
                            param_40 = 2u;
                            let _e344 = frag_tex_coord2_1;
                            param_41 = _e344;
                            param_42 = 2i;
                            let _e345 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_40), (&param_41), (&param_42));
                            let _e346 = frag_color2_;
                            color2_5 = (_e345 * _e346);
                            let _e348 = color2_5;
                            let _e350 = color2_5[3u];
                            let _e353 = color1_5;
                            let _e355 = color1_5[3u];
                            let _e359 = color0_;
                            base = (((_e348 + vec4(_e350)) * (_e353 + vec4(_e355))) * _e359);
                        } else {
                            param_43 = 1u;
                            let _e361 = frag_tex_coord1_1;
                            param_44 = _e361;
                            param_45 = 1i;
                            let _e362 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_43), (&param_44), (&param_45));
                            let _e363 = frag_color1_;
                            color1_6 = (_e362 * _e363);
                            param_46 = 2u;
                            let _e365 = frag_tex_coord2_1;
                            param_47 = _e365;
                            param_48 = 2i;
                            let _e366 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_46), (&param_47), (&param_48));
                            let _e367 = frag_color2_;
                            color2_6 = (_e366 * _e367);
                            let _e369 = color0_;
                            let _e371 = color1_6;
                            let _e374 = color2_6;
                            let _e376 = ((_e369.xyz * _e371.xyz) * _e374.xyz);
                            base[0u] = _e376.x;
                            base[1u] = _e376.y;
                            base[2u] = _e376.z;
                            let _e384 = color0_[3u];
                            let _e386 = color1_6[3u];
                            let _e389 = color2_6[3u];
                            base[3u] = ((_e384 * _e386) * _e389);
                        }
                    }
                }
            }
        }
    }
    let _e392 = wired_advanced_fog_enabled_u0028_();
    if _e392 {
        let _e393 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e393;
        if override_type_4_8 {
            let _e394 = fogAmount;
            let _e396 = base;
            let _e398 = (_e396.xyz * (1f - _e394));
            base[0u] = _e398.x;
            base[1u] = _e398.y;
            base[2u] = _e398.z;
        } else {
            if override_type_4_9 {
                let _e405 = fogAmount;
                let _e407 = base;
                base = (_e407 * (1f - _e405));
            } else {
                if override_type_4_10 {
                    let _e409 = fogAmount;
                    let _e412 = base[3u];
                    base[3u] = (_e412 * (1f - _e409));
                } else {
                    let _e415 = base;
                    let _e418 = unnamed.advancedFogColorDensity;
                    let _e420 = fogAmount;
                    let _e422 = mix(_e415.xyz, _e418.xyz, vec3(_e420));
                    base[0u] = _e422.x;
                    base[1u] = _e422.y;
                    base[2u] = _e422.z;
                }
            }
        }
    } else {
        if override_type_4_11 {
            let _e429 = base;
            let _e432 = fog[3u];
            let _e434 = (_e429.xyz * (1f - _e432));
            base[0u] = _e434.x;
            base[1u] = _e434.y;
            base[2u] = _e434.z;
        } else {
            if override_type_4_12 {
                let _e441 = base;
                let _e443 = fog[3u];
                base = (_e441 * (1f - _e443));
            } else {
                if override_type_4_13 {
                    let _e447 = base[3u];
                    let _e449 = fog[3u];
                    base[3u] = (_e447 * (1f - _e449));
                } else {
                    let _e453 = base;
                    let _e454 = fog;
                    let _e456 = unnamed.fogColor;
                    let _e459 = fog[3u];
                    base = mix(_e453, (_e454 * _e456), vec4(_e459));
                }
            }
        }
    }
    if override_type_4_14 {
        let _e463 = base[3u];
        if (_e463 == 0f) {
            discard;
        }
    } else {
        if override_type_4_15 {
            let _e465 = base;
            let _e467 = base;
            if (dot(_e465.xyz, _e467.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e471 = base;
    out_color = _e471;
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
