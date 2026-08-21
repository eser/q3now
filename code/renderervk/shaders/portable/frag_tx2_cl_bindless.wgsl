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

@id(4) override tex_domain: i32 = 0i;
@id(26) override lightmap_slot: i32 = 0i;
@id(6) override tex_mode: i32 = 0i;
override override_type_10_: bool = (tex_mode == 1i);
override override_type_10_1: bool = (tex_mode == 2i);
override override_type_10_2: bool = (override_type_10_ || override_type_10_1);
override override_type_10_3: bool = (tex_mode == 3i);
override override_type_10_4: bool = (tex_mode == 4i);
override override_type_10_5: bool = (tex_mode == 5i);
override override_type_10_6: bool = (tex_mode == 6i);
override override_type_10_7: bool = (tex_mode == 7i);
@id(7) override discard_mode: i32 = 0i;
override override_type_10_8: bool = (discard_mode == 1i);
override override_type_10_9: bool = (discard_mode == 2i);
@id(3) override alpha_to_coverage: i32 = 0i;

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

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e60 = (*c);
    (*c) = max(_e60, vec3<f32>(0f, 0f, 0f));
    let _e62 = (*c);
    cutoff = (_e62 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e64 = (*c);
    lo = (_e64 / vec3(12.92f));
    let _e67 = (*c);
    hi = pow(((_e67 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e72 = hi;
    let _e73 = lo;
    let _e74 = cutoff;
    return mix(_e72, _e73, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e74));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param: vec3<f32>;

    let _e61 = (*role);
    let _e63 = (*role);
    let _e68 = unnamed.packed_indices[(_e61 / 4u)][(_e63 % 4u)];
    let _e71 = (*role);
    let _e73 = (*role);
    let _e78 = unnamed.packed_indices[(_e71 / 4u)][(_e73 % 4u)];
    let _e83 = (*uv);
    let _e84 = textureSample(wired_bindless_images[(_e68 & 4095u)], wired_bindless_samplers[((_e78 >> bitcast<u32>(12i)) & 255u)], _e83);
    c_1 = _e84;
    let _e85 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e85))) == 0i) {
        let _e90 = c_1;
        param = _e90.xyz;
        let _e92 = sRGBToLinear_u0028_vf3_u003b((&param));
        c_1[0u] = _e92.x;
        c_1[1u] = _e92.y;
        c_1[2u] = _e92.z;
    }
    let _e99 = (*slot);
    if (lightmap_slot == (_e99 + 1i)) {
        let _e104 = unnamed.worldLightParams[0u];
        let _e105 = c_1;
        let _e107 = (_e105.xyz * _e104);
        c_1[0u] = _e107.x;
        c_1[1u] = _e107.y;
        c_1[2u] = _e107.z;
    }
    let _e114 = c_1;
    return _e114;
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

    let _e123 = frag_color0In_1;
    param_1 = _e123.xyz;
    let _e125 = sRGBToLinear_u0028_vf3_u003b((&param_1));
    let _e127 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e125.x, _e125.y, _e125.z, _e127);
    let _e132 = frag_color1In_1;
    param_2 = _e132.xyz;
    let _e134 = sRGBToLinear_u0028_vf3_u003b((&param_2));
    let _e136 = frag_color1In_1[3u];
    frag_color1_ = vec4<f32>(_e134.x, _e134.y, _e134.z, _e136);
    let _e141 = frag_color2In_1;
    param_3 = _e141.xyz;
    let _e143 = sRGBToLinear_u0028_vf3_u003b((&param_3));
    let _e145 = frag_color2In_1[3u];
    frag_color2_ = vec4<f32>(_e143.x, _e143.y, _e143.z, _e145);
    param_4 = 0u;
    let _e150 = frag_tex_coord0_1;
    param_5 = _e150;
    param_6 = 0i;
    let _e151 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_4), (&param_5), (&param_6));
    let _e152 = frag_color0_;
    color0_ = (_e151 * _e152);
    if override_type_10_2 {
        param_7 = 1u;
        let _e154 = frag_tex_coord1_1;
        param_8 = _e154;
        param_9 = 1i;
        let _e155 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_7), (&param_8), (&param_9));
        let _e156 = frag_color1_;
        color1_ = (_e155 * _e156);
        param_10 = 2u;
        let _e158 = frag_tex_coord2_1;
        param_11 = _e158;
        param_12 = 2i;
        let _e159 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_10), (&param_11), (&param_12));
        let _e160 = frag_color2_;
        color2_ = (_e159 * _e160);
        let _e162 = color0_;
        let _e164 = color1_;
        let _e167 = color2_;
        let _e169 = ((_e162.xyz + _e164.xyz) + _e167.xyz);
        let _e171 = color0_[3u];
        let _e173 = color1_[3u];
        let _e176 = color2_[3u];
        base = vec4<f32>(_e169.x, _e169.y, _e169.z, ((_e171 * _e173) * _e176));
    } else {
        if override_type_10_3 {
            param_13 = 1u;
            let _e182 = frag_tex_coord1_1;
            param_14 = _e182;
            param_15 = 1i;
            let _e183 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_13), (&param_14), (&param_15));
            let _e184 = frag_color1_;
            color1_1 = (_e183 * _e184);
            param_16 = 2u;
            let _e186 = frag_tex_coord2_1;
            param_17 = _e186;
            param_18 = 2i;
            let _e187 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_16), (&param_17), (&param_18));
            let _e188 = frag_color2_;
            color2_1 = (_e187 * _e188);
            let _e191 = color0_[3u];
            let _e192 = color0_;
            color0_ = (_e192 * _e191);
            let _e195 = color1_1[3u];
            let _e196 = color1_1;
            color1_1 = (_e196 * _e195);
            let _e199 = color2_1[3u];
            let _e200 = color2_1;
            color2_1 = (_e200 * _e199);
            let _e202 = color0_;
            let _e204 = color1_1;
            let _e207 = color2_1;
            let _e209 = ((_e202.xyz + _e204.xyz) + _e207.xyz);
            let _e211 = color0_[3u];
            let _e213 = color1_1[3u];
            let _e216 = color2_1[3u];
            base = vec4<f32>(_e209.x, _e209.y, _e209.z, ((_e211 * _e213) * _e216));
        } else {
            if override_type_10_4 {
                param_19 = 1u;
                let _e222 = frag_tex_coord1_1;
                param_20 = _e222;
                param_21 = 1i;
                let _e223 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_19), (&param_20), (&param_21));
                let _e224 = frag_color1_;
                color1_2 = (_e223 * _e224);
                param_22 = 2u;
                let _e226 = frag_tex_coord2_1;
                param_23 = _e226;
                param_24 = 2i;
                let _e227 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_22), (&param_23), (&param_24));
                let _e228 = frag_color2_;
                color2_2 = (_e227 * _e228);
                let _e231 = color0_[3u];
                let _e233 = color0_;
                color0_ = (_e233 * (1f - _e231));
                let _e236 = color1_2[3u];
                let _e238 = color1_2;
                color1_2 = (_e238 * (1f - _e236));
                let _e241 = color2_2[3u];
                let _e243 = color2_2;
                color2_2 = (_e243 * (1f - _e241));
                let _e245 = color0_;
                let _e247 = color1_2;
                let _e250 = color2_2;
                let _e252 = ((_e245.xyz + _e247.xyz) + _e250.xyz);
                let _e254 = color0_[3u];
                let _e256 = color1_2[3u];
                let _e259 = color2_2[3u];
                base = vec4<f32>(_e252.x, _e252.y, _e252.z, ((_e254 * _e256) * _e259));
            } else {
                if override_type_10_5 {
                    param_25 = 1u;
                    let _e265 = frag_tex_coord1_1;
                    param_26 = _e265;
                    param_27 = 1i;
                    let _e266 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_25), (&param_26), (&param_27));
                    let _e267 = frag_color1_;
                    color1_3 = (_e266 * _e267);
                    param_28 = 2u;
                    let _e269 = frag_tex_coord2_1;
                    param_29 = _e269;
                    param_30 = 2i;
                    let _e270 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_28), (&param_29), (&param_30));
                    let _e271 = frag_color2_;
                    color2_3 = (_e270 * _e271);
                    let _e273 = color0_;
                    let _e274 = color1_3;
                    let _e276 = color1_3[3u];
                    let _e279 = color2_3;
                    let _e281 = color2_3[3u];
                    base = mix(mix(_e273, _e274, vec4(_e276)), _e279, vec4(_e281));
                } else {
                    if override_type_10_6 {
                        param_31 = 1u;
                        let _e284 = frag_tex_coord1_1;
                        param_32 = _e284;
                        param_33 = 1i;
                        let _e285 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_31), (&param_32), (&param_33));
                        let _e286 = frag_color1_;
                        color1_4 = (_e285 * _e286);
                        param_34 = 2u;
                        let _e288 = frag_tex_coord2_1;
                        param_35 = _e288;
                        param_36 = 2i;
                        let _e289 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_34), (&param_35), (&param_36));
                        let _e290 = frag_color2_;
                        color2_4 = (_e289 * _e290);
                        let _e292 = color2_4;
                        let _e293 = color1_4;
                        let _e294 = color0_;
                        let _e296 = color1_4[3u];
                        let _e300 = color2_4[3u];
                        base = mix(_e292, mix(_e293, _e294, vec4(_e296)), vec4(_e300));
                    } else {
                        if override_type_10_7 {
                            param_37 = 1u;
                            let _e303 = frag_tex_coord1_1;
                            param_38 = _e303;
                            param_39 = 1i;
                            let _e304 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_37), (&param_38), (&param_39));
                            let _e305 = frag_color1_;
                            color1_5 = (_e304 * _e305);
                            param_40 = 2u;
                            let _e307 = frag_tex_coord2_1;
                            param_41 = _e307;
                            param_42 = 2i;
                            let _e308 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_40), (&param_41), (&param_42));
                            let _e309 = frag_color2_;
                            color2_5 = (_e308 * _e309);
                            let _e311 = color2_5;
                            let _e313 = color2_5[3u];
                            let _e316 = color1_5;
                            let _e318 = color1_5[3u];
                            let _e322 = color0_;
                            base = (((_e311 + vec4(_e313)) * (_e316 + vec4(_e318))) * _e322);
                        } else {
                            param_43 = 1u;
                            let _e324 = frag_tex_coord1_1;
                            param_44 = _e324;
                            param_45 = 1i;
                            let _e325 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_43), (&param_44), (&param_45));
                            let _e326 = frag_color1_;
                            color1_6 = (_e325 * _e326);
                            param_46 = 2u;
                            let _e328 = frag_tex_coord2_1;
                            param_47 = _e328;
                            param_48 = 2i;
                            let _e329 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_46), (&param_47), (&param_48));
                            let _e330 = frag_color2_;
                            color2_6 = (_e329 * _e330);
                            let _e332 = color0_;
                            let _e334 = color1_6;
                            let _e337 = color2_6;
                            let _e339 = ((_e332.xyz * _e334.xyz) * _e337.xyz);
                            base[0u] = _e339.x;
                            base[1u] = _e339.y;
                            base[2u] = _e339.z;
                            let _e347 = color0_[3u];
                            let _e349 = color1_6[3u];
                            let _e352 = color2_6[3u];
                            base[3u] = ((_e347 * _e349) * _e352);
                        }
                    }
                }
            }
        }
    }
    if override_type_10_8 {
        let _e356 = base[3u];
        if (_e356 == 0f) {
            discard;
        }
    } else {
        if override_type_10_9 {
            let _e358 = base;
            let _e360 = base;
            if (dot(_e358.xyz, _e360.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e364 = base;
    out_color = _e364;
    return;
}

@fragment 
fn main(@location(0) frag_color0In: vec4<f32>, @location(5) frag_color1In: vec4<f32>, @location(6) frag_color2In: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>, @location(3) frag_tex_coord2_: vec2<f32>) -> @location(0) vec4<f32> {
    frag_color0In_1 = frag_color0In;
    frag_color1In_1 = frag_color1In;
    frag_color2In_1 = frag_color2In;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    frag_tex_coord2_1 = frag_tex_coord2_;
    main_1();
    let _e13 = out_color;
    return _e13;
}
