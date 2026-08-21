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
@id(10) override acff: i32 = 0i;
override override_type_10_8: bool = (acff == 1i);
override override_type_10_9: bool = (acff == 2i);
override override_type_10_10: bool = (acff == 3i);
@id(7) override discard_mode: i32 = 0i;
override override_type_10_11: bool = (discard_mode == 1i);
override override_type_10_12: bool = (discard_mode == 2i);
@id(3) override alpha_to_coverage: i32 = 0i;

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

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e65 = (*c);
    (*c) = max(_e65, vec3<f32>(0f, 0f, 0f));
    let _e67 = (*c);
    cutoff = (_e67 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e69 = (*c);
    lo = (_e69 / vec3(12.92f));
    let _e72 = (*c);
    hi = pow(((_e72 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e77 = hi;
    let _e78 = lo;
    let _e79 = cutoff;
    return mix(_e77, _e78, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e79));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param: vec3<f32>;

    let _e66 = (*role);
    let _e68 = (*role);
    let _e73 = unnamed.packed_indices[(_e66 / 4u)][(_e68 % 4u)];
    let _e76 = (*role);
    let _e78 = (*role);
    let _e83 = unnamed.packed_indices[(_e76 / 4u)][(_e78 % 4u)];
    let _e88 = (*uv);
    let _e89 = textureSample(wired_bindless_images[(_e73 & 4095u)], wired_bindless_samplers[((_e83 >> bitcast<u32>(12i)) & 255u)], _e88);
    c_1 = _e89;
    let _e90 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e90))) == 0i) {
        let _e95 = c_1;
        param = _e95.xyz;
        let _e97 = sRGBToLinear_u0028_vf3_u003b((&param));
        c_1[0u] = _e97.x;
        c_1[1u] = _e97.y;
        c_1[2u] = _e97.z;
    }
    let _e104 = (*slot);
    if (lightmap_slot == (_e104 + 1i)) {
        let _e109 = unnamed.worldLightParams[0u];
        let _e110 = c_1;
        let _e112 = (_e110.xyz * _e109);
        c_1[0u] = _e112.x;
        c_1[1u] = _e112.y;
        c_1[2u] = _e112.z;
    }
    let _e119 = c_1;
    return _e119;
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

    let _e132 = unnamed.packed_indices[0i][3u];
    let _e138 = unnamed.packed_indices[0i][3u];
    let _e143 = fog_tex_coord_1;
    let _e144 = textureSample(wired_bindless_images[(_e132 & 4095u)], wired_bindless_samplers[((_e138 >> bitcast<u32>(12i)) & 255u)], _e143);
    fog = _e144;
    let _e145 = frag_color0In_1;
    param_1 = _e145.xyz;
    let _e147 = sRGBToLinear_u0028_vf3_u003b((&param_1));
    let _e149 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e147.x, _e147.y, _e147.z, _e149);
    let _e154 = frag_color1In_1;
    param_2 = _e154.xyz;
    let _e156 = sRGBToLinear_u0028_vf3_u003b((&param_2));
    let _e158 = frag_color1In_1[3u];
    frag_color1_ = vec4<f32>(_e156.x, _e156.y, _e156.z, _e158);
    let _e163 = frag_color2In_1;
    param_3 = _e163.xyz;
    let _e165 = sRGBToLinear_u0028_vf3_u003b((&param_3));
    let _e167 = frag_color2In_1[3u];
    frag_color2_ = vec4<f32>(_e165.x, _e165.y, _e165.z, _e167);
    param_4 = 0u;
    let _e172 = frag_tex_coord0_1;
    param_5 = _e172;
    param_6 = 0i;
    let _e173 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_4), (&param_5), (&param_6));
    let _e174 = frag_color0_;
    color0_ = (_e173 * _e174);
    if override_type_10_2 {
        param_7 = 1u;
        let _e176 = frag_tex_coord1_1;
        param_8 = _e176;
        param_9 = 1i;
        let _e177 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_7), (&param_8), (&param_9));
        let _e178 = frag_color1_;
        color1_ = (_e177 * _e178);
        param_10 = 2u;
        let _e180 = frag_tex_coord2_1;
        param_11 = _e180;
        param_12 = 2i;
        let _e181 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_10), (&param_11), (&param_12));
        let _e182 = frag_color2_;
        color2_ = (_e181 * _e182);
        let _e184 = color0_;
        let _e186 = color1_;
        let _e189 = color2_;
        let _e191 = ((_e184.xyz + _e186.xyz) + _e189.xyz);
        let _e193 = color0_[3u];
        let _e195 = color1_[3u];
        let _e198 = color2_[3u];
        base = vec4<f32>(_e191.x, _e191.y, _e191.z, ((_e193 * _e195) * _e198));
    } else {
        if override_type_10_3 {
            param_13 = 1u;
            let _e204 = frag_tex_coord1_1;
            param_14 = _e204;
            param_15 = 1i;
            let _e205 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_13), (&param_14), (&param_15));
            let _e206 = frag_color1_;
            color1_1 = (_e205 * _e206);
            param_16 = 2u;
            let _e208 = frag_tex_coord2_1;
            param_17 = _e208;
            param_18 = 2i;
            let _e209 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_16), (&param_17), (&param_18));
            let _e210 = frag_color2_;
            color2_1 = (_e209 * _e210);
            let _e213 = color0_[3u];
            let _e214 = color0_;
            color0_ = (_e214 * _e213);
            let _e217 = color1_1[3u];
            let _e218 = color1_1;
            color1_1 = (_e218 * _e217);
            let _e221 = color2_1[3u];
            let _e222 = color2_1;
            color2_1 = (_e222 * _e221);
            let _e224 = color0_;
            let _e226 = color1_1;
            let _e229 = color2_1;
            let _e231 = ((_e224.xyz + _e226.xyz) + _e229.xyz);
            let _e233 = color0_[3u];
            let _e235 = color1_1[3u];
            let _e238 = color2_1[3u];
            base = vec4<f32>(_e231.x, _e231.y, _e231.z, ((_e233 * _e235) * _e238));
        } else {
            if override_type_10_4 {
                param_19 = 1u;
                let _e244 = frag_tex_coord1_1;
                param_20 = _e244;
                param_21 = 1i;
                let _e245 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_19), (&param_20), (&param_21));
                let _e246 = frag_color1_;
                color1_2 = (_e245 * _e246);
                param_22 = 2u;
                let _e248 = frag_tex_coord2_1;
                param_23 = _e248;
                param_24 = 2i;
                let _e249 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_22), (&param_23), (&param_24));
                let _e250 = frag_color2_;
                color2_2 = (_e249 * _e250);
                let _e253 = color0_[3u];
                let _e255 = color0_;
                color0_ = (_e255 * (1f - _e253));
                let _e258 = color1_2[3u];
                let _e260 = color1_2;
                color1_2 = (_e260 * (1f - _e258));
                let _e263 = color2_2[3u];
                let _e265 = color2_2;
                color2_2 = (_e265 * (1f - _e263));
                let _e267 = color0_;
                let _e269 = color1_2;
                let _e272 = color2_2;
                let _e274 = ((_e267.xyz + _e269.xyz) + _e272.xyz);
                let _e276 = color0_[3u];
                let _e278 = color1_2[3u];
                let _e281 = color2_2[3u];
                base = vec4<f32>(_e274.x, _e274.y, _e274.z, ((_e276 * _e278) * _e281));
            } else {
                if override_type_10_5 {
                    param_25 = 1u;
                    let _e287 = frag_tex_coord1_1;
                    param_26 = _e287;
                    param_27 = 1i;
                    let _e288 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_25), (&param_26), (&param_27));
                    let _e289 = frag_color1_;
                    color1_3 = (_e288 * _e289);
                    param_28 = 2u;
                    let _e291 = frag_tex_coord2_1;
                    param_29 = _e291;
                    param_30 = 2i;
                    let _e292 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_28), (&param_29), (&param_30));
                    let _e293 = frag_color2_;
                    color2_3 = (_e292 * _e293);
                    let _e295 = color0_;
                    let _e296 = color1_3;
                    let _e298 = color1_3[3u];
                    let _e301 = color2_3;
                    let _e303 = color2_3[3u];
                    base = mix(mix(_e295, _e296, vec4(_e298)), _e301, vec4(_e303));
                } else {
                    if override_type_10_6 {
                        param_31 = 1u;
                        let _e306 = frag_tex_coord1_1;
                        param_32 = _e306;
                        param_33 = 1i;
                        let _e307 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_31), (&param_32), (&param_33));
                        let _e308 = frag_color1_;
                        color1_4 = (_e307 * _e308);
                        param_34 = 2u;
                        let _e310 = frag_tex_coord2_1;
                        param_35 = _e310;
                        param_36 = 2i;
                        let _e311 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_34), (&param_35), (&param_36));
                        let _e312 = frag_color2_;
                        color2_4 = (_e311 * _e312);
                        let _e314 = color2_4;
                        let _e315 = color1_4;
                        let _e316 = color0_;
                        let _e318 = color1_4[3u];
                        let _e322 = color2_4[3u];
                        base = mix(_e314, mix(_e315, _e316, vec4(_e318)), vec4(_e322));
                    } else {
                        if override_type_10_7 {
                            param_37 = 1u;
                            let _e325 = frag_tex_coord1_1;
                            param_38 = _e325;
                            param_39 = 1i;
                            let _e326 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_37), (&param_38), (&param_39));
                            let _e327 = frag_color1_;
                            color1_5 = (_e326 * _e327);
                            param_40 = 2u;
                            let _e329 = frag_tex_coord2_1;
                            param_41 = _e329;
                            param_42 = 2i;
                            let _e330 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_40), (&param_41), (&param_42));
                            let _e331 = frag_color2_;
                            color2_5 = (_e330 * _e331);
                            let _e333 = color2_5;
                            let _e335 = color2_5[3u];
                            let _e338 = color1_5;
                            let _e340 = color1_5[3u];
                            let _e344 = color0_;
                            base = (((_e333 + vec4(_e335)) * (_e338 + vec4(_e340))) * _e344);
                        } else {
                            param_43 = 1u;
                            let _e346 = frag_tex_coord1_1;
                            param_44 = _e346;
                            param_45 = 1i;
                            let _e347 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_43), (&param_44), (&param_45));
                            let _e348 = frag_color1_;
                            color1_6 = (_e347 * _e348);
                            param_46 = 2u;
                            let _e350 = frag_tex_coord2_1;
                            param_47 = _e350;
                            param_48 = 2i;
                            let _e351 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_46), (&param_47), (&param_48));
                            let _e352 = frag_color2_;
                            color2_6 = (_e351 * _e352);
                            let _e354 = color0_;
                            let _e356 = color1_6;
                            let _e359 = color2_6;
                            let _e361 = ((_e354.xyz * _e356.xyz) * _e359.xyz);
                            base[0u] = _e361.x;
                            base[1u] = _e361.y;
                            base[2u] = _e361.z;
                            let _e369 = color0_[3u];
                            let _e371 = color1_6[3u];
                            let _e374 = color2_6[3u];
                            base[3u] = ((_e369 * _e371) * _e374);
                        }
                    }
                }
            }
        }
    }
    if override_type_10_8 {
        let _e377 = base;
        let _e380 = fog[3u];
        let _e382 = (_e377.xyz * (1f - _e380));
        base[0u] = _e382.x;
        base[1u] = _e382.y;
        base[2u] = _e382.z;
    } else {
        if override_type_10_9 {
            let _e389 = base;
            let _e391 = fog[3u];
            base = (_e389 * (1f - _e391));
        } else {
            if override_type_10_10 {
                let _e395 = base[3u];
                let _e397 = fog[3u];
                base[3u] = (_e395 * (1f - _e397));
            } else {
                let _e401 = base;
                let _e402 = fog;
                let _e404 = unnamed.fogColor;
                let _e407 = fog[3u];
                base = mix(_e401, (_e402 * _e404), vec4(_e407));
            }
        }
    }
    if override_type_10_11 {
        let _e411 = base[3u];
        if (_e411 == 0f) {
            discard;
        }
    } else {
        if override_type_10_12 {
            let _e413 = base;
            let _e415 = base;
            if (dot(_e413.xyz, _e415.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e419 = base;
    out_color = _e419;
    return;
}

@fragment 
fn main(@location(4) fog_tex_coord: vec2<f32>, @location(0) frag_color0In: vec4<f32>, @location(5) frag_color1In: vec4<f32>, @location(6) frag_color2In: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>, @location(3) frag_tex_coord2_: vec2<f32>) -> @location(0) vec4<f32> {
    fog_tex_coord_1 = fog_tex_coord;
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
