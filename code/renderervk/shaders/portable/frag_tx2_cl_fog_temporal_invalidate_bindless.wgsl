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
}

struct FragmentOutput {
    @location(1) member: vec2<f32>,
    @location(2) member_1: f32,
    @location(0) member_2: vec4<f32>,
}

@id(4) override tex_domain: i32 = 0i;
@id(26) override lightmap_slot: i32 = 0i;
@id(6) override tex_mode: i32 = 0i;
override override_type_12_: bool = (tex_mode == 1i);
override override_type_12_1: bool = (tex_mode == 2i);
override override_type_12_2: bool = (override_type_12_ || override_type_12_1);
override override_type_12_3: bool = (tex_mode == 3i);
override override_type_12_4: bool = (tex_mode == 4i);
override override_type_12_5: bool = (tex_mode == 5i);
override override_type_12_6: bool = (tex_mode == 6i);
override override_type_12_7: bool = (tex_mode == 7i);
@id(10) override acff: i32 = 0i;
override override_type_12_8: bool = (acff == 1i);
override override_type_12_9: bool = (acff == 2i);
override override_type_12_10: bool = (acff == 3i);
@id(7) override discard_mode: i32 = 0i;
override override_type_12_11: bool = (discard_mode == 1i);
override override_type_12_12: bool = (discard_mode == 2i);
@id(3) override alpha_to_coverage: i32 = 0i;

var<private> out_temporal_velocity: vec2<f32>;
var<private> out_temporal_validity: f32;
@group(1) @binding(0) 
var wired_bindless_images: binding_array<texture_2d<f32>>;
@group(0) @binding(0) 
var<uniform> unnamed: UBO;
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

fn wiredTemporalWriteAux_u0028_() {
    out_temporal_velocity = vec2<f32>(0f, 0f);
    out_temporal_validity = 0f;
    return;
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

    let _e138 = unnamed.packed_indices[0i][3u];
    let _e144 = unnamed.packed_indices[0i][3u];
    let _e149 = fog_tex_coord_1;
    let _e150 = textureSample(wired_bindless_images[(_e138 & 4095u)], wired_bindless_samplers[((_e144 >> bitcast<u32>(12i)) & 255u)], _e149);
    fog = _e150;
    let _e151 = frag_color0In_1;
    param_1 = _e151.xyz;
    let _e153 = sRGBToLinear_u0028_vf3_u003b((&param_1));
    let _e155 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e153.x, _e153.y, _e153.z, _e155);
    let _e160 = frag_color1In_1;
    param_2 = _e160.xyz;
    let _e162 = sRGBToLinear_u0028_vf3_u003b((&param_2));
    let _e164 = frag_color1In_1[3u];
    frag_color1_ = vec4<f32>(_e162.x, _e162.y, _e162.z, _e164);
    let _e169 = frag_color2In_1;
    param_3 = _e169.xyz;
    let _e171 = sRGBToLinear_u0028_vf3_u003b((&param_3));
    let _e173 = frag_color2In_1[3u];
    frag_color2_ = vec4<f32>(_e171.x, _e171.y, _e171.z, _e173);
    param_4 = 0u;
    let _e178 = frag_tex_coord0_1;
    param_5 = _e178;
    param_6 = 0i;
    let _e179 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_4), (&param_5), (&param_6));
    let _e180 = frag_color0_;
    color0_ = (_e179 * _e180);
    if override_type_12_2 {
        param_7 = 1u;
        let _e182 = frag_tex_coord1_1;
        param_8 = _e182;
        param_9 = 1i;
        let _e183 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_7), (&param_8), (&param_9));
        let _e184 = frag_color1_;
        color1_ = (_e183 * _e184);
        param_10 = 2u;
        let _e186 = frag_tex_coord2_1;
        param_11 = _e186;
        param_12 = 2i;
        let _e187 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_10), (&param_11), (&param_12));
        let _e188 = frag_color2_;
        color2_ = (_e187 * _e188);
        let _e190 = color0_;
        let _e192 = color1_;
        let _e195 = color2_;
        let _e197 = ((_e190.xyz + _e192.xyz) + _e195.xyz);
        let _e199 = color0_[3u];
        let _e201 = color1_[3u];
        let _e204 = color2_[3u];
        base = vec4<f32>(_e197.x, _e197.y, _e197.z, ((_e199 * _e201) * _e204));
    } else {
        if override_type_12_3 {
            param_13 = 1u;
            let _e210 = frag_tex_coord1_1;
            param_14 = _e210;
            param_15 = 1i;
            let _e211 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_13), (&param_14), (&param_15));
            let _e212 = frag_color1_;
            color1_1 = (_e211 * _e212);
            param_16 = 2u;
            let _e214 = frag_tex_coord2_1;
            param_17 = _e214;
            param_18 = 2i;
            let _e215 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_16), (&param_17), (&param_18));
            let _e216 = frag_color2_;
            color2_1 = (_e215 * _e216);
            let _e219 = color0_[3u];
            let _e220 = color0_;
            color0_ = (_e220 * _e219);
            let _e223 = color1_1[3u];
            let _e224 = color1_1;
            color1_1 = (_e224 * _e223);
            let _e227 = color2_1[3u];
            let _e228 = color2_1;
            color2_1 = (_e228 * _e227);
            let _e230 = color0_;
            let _e232 = color1_1;
            let _e235 = color2_1;
            let _e237 = ((_e230.xyz + _e232.xyz) + _e235.xyz);
            let _e239 = color0_[3u];
            let _e241 = color1_1[3u];
            let _e244 = color2_1[3u];
            base = vec4<f32>(_e237.x, _e237.y, _e237.z, ((_e239 * _e241) * _e244));
        } else {
            if override_type_12_4 {
                param_19 = 1u;
                let _e250 = frag_tex_coord1_1;
                param_20 = _e250;
                param_21 = 1i;
                let _e251 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_19), (&param_20), (&param_21));
                let _e252 = frag_color1_;
                color1_2 = (_e251 * _e252);
                param_22 = 2u;
                let _e254 = frag_tex_coord2_1;
                param_23 = _e254;
                param_24 = 2i;
                let _e255 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_22), (&param_23), (&param_24));
                let _e256 = frag_color2_;
                color2_2 = (_e255 * _e256);
                let _e259 = color0_[3u];
                let _e261 = color0_;
                color0_ = (_e261 * (1f - _e259));
                let _e264 = color1_2[3u];
                let _e266 = color1_2;
                color1_2 = (_e266 * (1f - _e264));
                let _e269 = color2_2[3u];
                let _e271 = color2_2;
                color2_2 = (_e271 * (1f - _e269));
                let _e273 = color0_;
                let _e275 = color1_2;
                let _e278 = color2_2;
                let _e280 = ((_e273.xyz + _e275.xyz) + _e278.xyz);
                let _e282 = color0_[3u];
                let _e284 = color1_2[3u];
                let _e287 = color2_2[3u];
                base = vec4<f32>(_e280.x, _e280.y, _e280.z, ((_e282 * _e284) * _e287));
            } else {
                if override_type_12_5 {
                    param_25 = 1u;
                    let _e293 = frag_tex_coord1_1;
                    param_26 = _e293;
                    param_27 = 1i;
                    let _e294 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_25), (&param_26), (&param_27));
                    let _e295 = frag_color1_;
                    color1_3 = (_e294 * _e295);
                    param_28 = 2u;
                    let _e297 = frag_tex_coord2_1;
                    param_29 = _e297;
                    param_30 = 2i;
                    let _e298 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_28), (&param_29), (&param_30));
                    let _e299 = frag_color2_;
                    color2_3 = (_e298 * _e299);
                    let _e301 = color0_;
                    let _e302 = color1_3;
                    let _e304 = color1_3[3u];
                    let _e307 = color2_3;
                    let _e309 = color2_3[3u];
                    base = mix(mix(_e301, _e302, vec4(_e304)), _e307, vec4(_e309));
                } else {
                    if override_type_12_6 {
                        param_31 = 1u;
                        let _e312 = frag_tex_coord1_1;
                        param_32 = _e312;
                        param_33 = 1i;
                        let _e313 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_31), (&param_32), (&param_33));
                        let _e314 = frag_color1_;
                        color1_4 = (_e313 * _e314);
                        param_34 = 2u;
                        let _e316 = frag_tex_coord2_1;
                        param_35 = _e316;
                        param_36 = 2i;
                        let _e317 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_34), (&param_35), (&param_36));
                        let _e318 = frag_color2_;
                        color2_4 = (_e317 * _e318);
                        let _e320 = color2_4;
                        let _e321 = color1_4;
                        let _e322 = color0_;
                        let _e324 = color1_4[3u];
                        let _e328 = color2_4[3u];
                        base = mix(_e320, mix(_e321, _e322, vec4(_e324)), vec4(_e328));
                    } else {
                        if override_type_12_7 {
                            param_37 = 1u;
                            let _e331 = frag_tex_coord1_1;
                            param_38 = _e331;
                            param_39 = 1i;
                            let _e332 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_37), (&param_38), (&param_39));
                            let _e333 = frag_color1_;
                            color1_5 = (_e332 * _e333);
                            param_40 = 2u;
                            let _e335 = frag_tex_coord2_1;
                            param_41 = _e335;
                            param_42 = 2i;
                            let _e336 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_40), (&param_41), (&param_42));
                            let _e337 = frag_color2_;
                            color2_5 = (_e336 * _e337);
                            let _e339 = color2_5;
                            let _e341 = color2_5[3u];
                            let _e344 = color1_5;
                            let _e346 = color1_5[3u];
                            let _e350 = color0_;
                            base = (((_e339 + vec4(_e341)) * (_e344 + vec4(_e346))) * _e350);
                        } else {
                            param_43 = 1u;
                            let _e352 = frag_tex_coord1_1;
                            param_44 = _e352;
                            param_45 = 1i;
                            let _e353 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_43), (&param_44), (&param_45));
                            let _e354 = frag_color1_;
                            color1_6 = (_e353 * _e354);
                            param_46 = 2u;
                            let _e356 = frag_tex_coord2_1;
                            param_47 = _e356;
                            param_48 = 2i;
                            let _e357 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_46), (&param_47), (&param_48));
                            let _e358 = frag_color2_;
                            color2_6 = (_e357 * _e358);
                            let _e360 = color0_;
                            let _e362 = color1_6;
                            let _e365 = color2_6;
                            let _e367 = ((_e360.xyz * _e362.xyz) * _e365.xyz);
                            base[0u] = _e367.x;
                            base[1u] = _e367.y;
                            base[2u] = _e367.z;
                            let _e375 = color0_[3u];
                            let _e377 = color1_6[3u];
                            let _e380 = color2_6[3u];
                            base[3u] = ((_e375 * _e377) * _e380);
                        }
                    }
                }
            }
        }
    }
    if override_type_12_8 {
        let _e383 = base;
        let _e386 = fog[3u];
        let _e388 = (_e383.xyz * (1f - _e386));
        base[0u] = _e388.x;
        base[1u] = _e388.y;
        base[2u] = _e388.z;
    } else {
        if override_type_12_9 {
            let _e395 = base;
            let _e397 = fog[3u];
            base = (_e395 * (1f - _e397));
        } else {
            if override_type_12_10 {
                let _e401 = base[3u];
                let _e403 = fog[3u];
                base[3u] = (_e401 * (1f - _e403));
            } else {
                let _e407 = base;
                let _e408 = fog;
                let _e410 = unnamed.fogColor;
                let _e413 = fog[3u];
                base = mix(_e407, (_e408 * _e410), vec4(_e413));
            }
        }
    }
    if override_type_12_11 {
        let _e417 = base[3u];
        if (_e417 == 0f) {
            discard;
        }
    } else {
        if override_type_12_12 {
            let _e419 = base;
            let _e421 = base;
            if (dot(_e419.xyz, _e421.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e425 = base;
    out_color = _e425;
    wiredTemporalWriteAux_u0028_();
    return;
}

@fragment 
fn main(@location(4) fog_tex_coord: vec2<f32>, @location(0) frag_color0In: vec4<f32>, @location(5) frag_color1In: vec4<f32>, @location(6) frag_color2In: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>, @location(3) frag_tex_coord2_: vec2<f32>, @location(10) temporalCurrentClip: vec4<f32>, @location(11) temporalPreviousClip: vec4<f32>, @location(12) @interpolate(flat) temporalOutcome: u32) -> FragmentOutput {
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
    let _e23 = out_temporal_velocity;
    let _e24 = out_temporal_validity;
    let _e25 = out_color;
    return FragmentOutput(_e23, _e24, _e25);
}
