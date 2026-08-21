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
@id(7) override discard_mode: i32 = 0i;
override override_type_12_8: bool = (discard_mode == 1i);
override override_type_12_9: bool = (discard_mode == 2i);
@id(3) override alpha_to_coverage: i32 = 0i;

var<private> out_temporal_velocity: vec2<f32>;
var<private> out_temporal_validity: f32;
@group(1) @binding(0) 
var wired_bindless_images: binding_array<texture_2d<f32>>;
@group(0) @binding(0) 
var<uniform> unnamed: UBO;
@group(1) @binding(1) 
var wired_bindless_samplers: binding_array<sampler>;
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

    let _e66 = (*c);
    (*c) = max(_e66, vec3<f32>(0f, 0f, 0f));
    let _e68 = (*c);
    cutoff = (_e68 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e70 = (*c);
    lo = (_e70 / vec3(12.92f));
    let _e73 = (*c);
    hi = pow(((_e73 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e78 = hi;
    let _e79 = lo;
    let _e80 = cutoff;
    return mix(_e78, _e79, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e80));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param: vec3<f32>;

    let _e67 = (*role);
    let _e69 = (*role);
    let _e74 = unnamed.packed_indices[(_e67 / 4u)][(_e69 % 4u)];
    let _e77 = (*role);
    let _e79 = (*role);
    let _e84 = unnamed.packed_indices[(_e77 / 4u)][(_e79 % 4u)];
    let _e89 = (*uv);
    let _e90 = textureSample(wired_bindless_images[(_e74 & 4095u)], wired_bindless_samplers[((_e84 >> bitcast<u32>(12i)) & 255u)], _e89);
    c_1 = _e90;
    let _e91 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e91))) == 0i) {
        let _e96 = c_1;
        param = _e96.xyz;
        let _e98 = sRGBToLinear_u0028_vf3_u003b((&param));
        c_1[0u] = _e98.x;
        c_1[1u] = _e98.y;
        c_1[2u] = _e98.z;
    }
    let _e105 = (*slot);
    if (lightmap_slot == (_e105 + 1i)) {
        let _e110 = unnamed.worldLightParams[0u];
        let _e111 = c_1;
        let _e113 = (_e111.xyz * _e110);
        c_1[0u] = _e113.x;
        c_1[1u] = _e113.y;
        c_1[2u] = _e113.z;
    }
    let _e120 = c_1;
    return _e120;
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

    let _e129 = frag_color0In_1;
    param_1 = _e129.xyz;
    let _e131 = sRGBToLinear_u0028_vf3_u003b((&param_1));
    let _e133 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e131.x, _e131.y, _e131.z, _e133);
    let _e138 = frag_color1In_1;
    param_2 = _e138.xyz;
    let _e140 = sRGBToLinear_u0028_vf3_u003b((&param_2));
    let _e142 = frag_color1In_1[3u];
    frag_color1_ = vec4<f32>(_e140.x, _e140.y, _e140.z, _e142);
    let _e147 = frag_color2In_1;
    param_3 = _e147.xyz;
    let _e149 = sRGBToLinear_u0028_vf3_u003b((&param_3));
    let _e151 = frag_color2In_1[3u];
    frag_color2_ = vec4<f32>(_e149.x, _e149.y, _e149.z, _e151);
    param_4 = 0u;
    let _e156 = frag_tex_coord0_1;
    param_5 = _e156;
    param_6 = 0i;
    let _e157 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_4), (&param_5), (&param_6));
    let _e158 = frag_color0_;
    color0_ = (_e157 * _e158);
    if override_type_12_2 {
        param_7 = 1u;
        let _e160 = frag_tex_coord1_1;
        param_8 = _e160;
        param_9 = 1i;
        let _e161 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_7), (&param_8), (&param_9));
        let _e162 = frag_color1_;
        color1_ = (_e161 * _e162);
        param_10 = 2u;
        let _e164 = frag_tex_coord2_1;
        param_11 = _e164;
        param_12 = 2i;
        let _e165 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_10), (&param_11), (&param_12));
        let _e166 = frag_color2_;
        color2_ = (_e165 * _e166);
        let _e168 = color0_;
        let _e170 = color1_;
        let _e173 = color2_;
        let _e175 = ((_e168.xyz + _e170.xyz) + _e173.xyz);
        let _e177 = color0_[3u];
        let _e179 = color1_[3u];
        let _e182 = color2_[3u];
        base = vec4<f32>(_e175.x, _e175.y, _e175.z, ((_e177 * _e179) * _e182));
    } else {
        if override_type_12_3 {
            param_13 = 1u;
            let _e188 = frag_tex_coord1_1;
            param_14 = _e188;
            param_15 = 1i;
            let _e189 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_13), (&param_14), (&param_15));
            let _e190 = frag_color1_;
            color1_1 = (_e189 * _e190);
            param_16 = 2u;
            let _e192 = frag_tex_coord2_1;
            param_17 = _e192;
            param_18 = 2i;
            let _e193 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_16), (&param_17), (&param_18));
            let _e194 = frag_color2_;
            color2_1 = (_e193 * _e194);
            let _e197 = color0_[3u];
            let _e198 = color0_;
            color0_ = (_e198 * _e197);
            let _e201 = color1_1[3u];
            let _e202 = color1_1;
            color1_1 = (_e202 * _e201);
            let _e205 = color2_1[3u];
            let _e206 = color2_1;
            color2_1 = (_e206 * _e205);
            let _e208 = color0_;
            let _e210 = color1_1;
            let _e213 = color2_1;
            let _e215 = ((_e208.xyz + _e210.xyz) + _e213.xyz);
            let _e217 = color0_[3u];
            let _e219 = color1_1[3u];
            let _e222 = color2_1[3u];
            base = vec4<f32>(_e215.x, _e215.y, _e215.z, ((_e217 * _e219) * _e222));
        } else {
            if override_type_12_4 {
                param_19 = 1u;
                let _e228 = frag_tex_coord1_1;
                param_20 = _e228;
                param_21 = 1i;
                let _e229 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_19), (&param_20), (&param_21));
                let _e230 = frag_color1_;
                color1_2 = (_e229 * _e230);
                param_22 = 2u;
                let _e232 = frag_tex_coord2_1;
                param_23 = _e232;
                param_24 = 2i;
                let _e233 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_22), (&param_23), (&param_24));
                let _e234 = frag_color2_;
                color2_2 = (_e233 * _e234);
                let _e237 = color0_[3u];
                let _e239 = color0_;
                color0_ = (_e239 * (1f - _e237));
                let _e242 = color1_2[3u];
                let _e244 = color1_2;
                color1_2 = (_e244 * (1f - _e242));
                let _e247 = color2_2[3u];
                let _e249 = color2_2;
                color2_2 = (_e249 * (1f - _e247));
                let _e251 = color0_;
                let _e253 = color1_2;
                let _e256 = color2_2;
                let _e258 = ((_e251.xyz + _e253.xyz) + _e256.xyz);
                let _e260 = color0_[3u];
                let _e262 = color1_2[3u];
                let _e265 = color2_2[3u];
                base = vec4<f32>(_e258.x, _e258.y, _e258.z, ((_e260 * _e262) * _e265));
            } else {
                if override_type_12_5 {
                    param_25 = 1u;
                    let _e271 = frag_tex_coord1_1;
                    param_26 = _e271;
                    param_27 = 1i;
                    let _e272 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_25), (&param_26), (&param_27));
                    let _e273 = frag_color1_;
                    color1_3 = (_e272 * _e273);
                    param_28 = 2u;
                    let _e275 = frag_tex_coord2_1;
                    param_29 = _e275;
                    param_30 = 2i;
                    let _e276 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_28), (&param_29), (&param_30));
                    let _e277 = frag_color2_;
                    color2_3 = (_e276 * _e277);
                    let _e279 = color0_;
                    let _e280 = color1_3;
                    let _e282 = color1_3[3u];
                    let _e285 = color2_3;
                    let _e287 = color2_3[3u];
                    base = mix(mix(_e279, _e280, vec4(_e282)), _e285, vec4(_e287));
                } else {
                    if override_type_12_6 {
                        param_31 = 1u;
                        let _e290 = frag_tex_coord1_1;
                        param_32 = _e290;
                        param_33 = 1i;
                        let _e291 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_31), (&param_32), (&param_33));
                        let _e292 = frag_color1_;
                        color1_4 = (_e291 * _e292);
                        param_34 = 2u;
                        let _e294 = frag_tex_coord2_1;
                        param_35 = _e294;
                        param_36 = 2i;
                        let _e295 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_34), (&param_35), (&param_36));
                        let _e296 = frag_color2_;
                        color2_4 = (_e295 * _e296);
                        let _e298 = color2_4;
                        let _e299 = color1_4;
                        let _e300 = color0_;
                        let _e302 = color1_4[3u];
                        let _e306 = color2_4[3u];
                        base = mix(_e298, mix(_e299, _e300, vec4(_e302)), vec4(_e306));
                    } else {
                        if override_type_12_7 {
                            param_37 = 1u;
                            let _e309 = frag_tex_coord1_1;
                            param_38 = _e309;
                            param_39 = 1i;
                            let _e310 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_37), (&param_38), (&param_39));
                            let _e311 = frag_color1_;
                            color1_5 = (_e310 * _e311);
                            param_40 = 2u;
                            let _e313 = frag_tex_coord2_1;
                            param_41 = _e313;
                            param_42 = 2i;
                            let _e314 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_40), (&param_41), (&param_42));
                            let _e315 = frag_color2_;
                            color2_5 = (_e314 * _e315);
                            let _e317 = color2_5;
                            let _e319 = color2_5[3u];
                            let _e322 = color1_5;
                            let _e324 = color1_5[3u];
                            let _e328 = color0_;
                            base = (((_e317 + vec4(_e319)) * (_e322 + vec4(_e324))) * _e328);
                        } else {
                            param_43 = 1u;
                            let _e330 = frag_tex_coord1_1;
                            param_44 = _e330;
                            param_45 = 1i;
                            let _e331 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_43), (&param_44), (&param_45));
                            let _e332 = frag_color1_;
                            color1_6 = (_e331 * _e332);
                            param_46 = 2u;
                            let _e334 = frag_tex_coord2_1;
                            param_47 = _e334;
                            param_48 = 2i;
                            let _e335 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_46), (&param_47), (&param_48));
                            let _e336 = frag_color2_;
                            color2_6 = (_e335 * _e336);
                            let _e338 = color0_;
                            let _e340 = color1_6;
                            let _e343 = color2_6;
                            let _e345 = ((_e338.xyz * _e340.xyz) * _e343.xyz);
                            base[0u] = _e345.x;
                            base[1u] = _e345.y;
                            base[2u] = _e345.z;
                            let _e353 = color0_[3u];
                            let _e355 = color1_6[3u];
                            let _e358 = color2_6[3u];
                            base[3u] = ((_e353 * _e355) * _e358);
                        }
                    }
                }
            }
        }
    }
    if override_type_12_8 {
        let _e362 = base[3u];
        if (_e362 == 0f) {
            discard;
        }
    } else {
        if override_type_12_9 {
            let _e364 = base;
            let _e366 = base;
            if (dot(_e364.xyz, _e366.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e370 = base;
    out_color = _e370;
    wiredTemporalWriteAux_u0028_();
    return;
}

@fragment 
fn main(@location(0) frag_color0In: vec4<f32>, @location(5) frag_color1In: vec4<f32>, @location(6) frag_color2In: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>, @location(3) frag_tex_coord2_: vec2<f32>, @location(10) temporalCurrentClip: vec4<f32>, @location(11) temporalPreviousClip: vec4<f32>, @location(12) @interpolate(flat) temporalOutcome: u32) -> FragmentOutput {
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
    let _e21 = out_temporal_velocity;
    let _e22 = out_temporal_validity;
    let _e23 = out_color;
    return FragmentOutput(_e21, _e22, _e23);
}
