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
var<private> frag_tex_coord0_1: vec2<f32>;
var<private> frag_tex_coord1_1: vec2<f32>;
var<private> out_color: vec4<f32>;

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e58 = (*c);
    (*c) = max(_e58, vec3<f32>(0f, 0f, 0f));
    let _e60 = (*c);
    cutoff = (_e60 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e62 = (*c);
    lo = (_e62 / vec3(12.92f));
    let _e65 = (*c);
    hi = pow(((_e65 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e70 = hi;
    let _e71 = lo;
    let _e72 = cutoff;
    return mix(_e70, _e71, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e72));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param: vec3<f32>;

    let _e59 = (*role);
    let _e61 = (*role);
    let _e66 = unnamed.packed_indices[(_e59 / 4u)][(_e61 % 4u)];
    let _e69 = (*role);
    let _e71 = (*role);
    let _e76 = unnamed.packed_indices[(_e69 / 4u)][(_e71 % 4u)];
    let _e81 = (*uv);
    let _e82 = textureSample(wired_bindless_images[(_e66 & 4095u)], wired_bindless_samplers[((_e76 >> bitcast<u32>(12i)) & 255u)], _e81);
    c_1 = _e82;
    let _e83 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e83))) == 0i) {
        let _e88 = c_1;
        param = _e88.xyz;
        let _e90 = sRGBToLinear_u0028_vf3_u003b((&param));
        c_1[0u] = _e90.x;
        c_1[1u] = _e90.y;
        c_1[2u] = _e90.z;
    }
    let _e97 = (*slot);
    if (lightmap_slot == (_e97 + 1i)) {
        let _e102 = unnamed.worldLightParams[0u];
        let _e103 = c_1;
        let _e105 = (_e103.xyz * _e102);
        c_1[0u] = _e105.x;
        c_1[1u] = _e105.y;
        c_1[2u] = _e105.z;
    }
    let _e112 = c_1;
    return _e112;
}

fn main_1() {
    var frag_color0_: vec4<f32>;
    var param_1: vec3<f32>;
    var frag_color1_: vec4<f32>;
    var param_2: vec3<f32>;
    var color0_: vec4<f32>;
    var param_3: u32;
    var param_4: vec2<f32>;
    var param_5: i32;
    var color1_: vec4<f32>;
    var param_6: u32;
    var param_7: vec2<f32>;
    var param_8: i32;
    var base: vec4<f32>;
    var color1_1: vec4<f32>;
    var param_9: u32;
    var param_10: vec2<f32>;
    var param_11: i32;
    var color1_2: vec4<f32>;
    var param_12: u32;
    var param_13: vec2<f32>;
    var param_14: i32;
    var color1_3: vec4<f32>;
    var param_15: u32;
    var param_16: vec2<f32>;
    var param_17: i32;
    var color1_4: vec4<f32>;
    var param_18: u32;
    var param_19: vec2<f32>;
    var param_20: i32;
    var color1_5: vec4<f32>;
    var param_21: u32;
    var param_22: vec2<f32>;
    var param_23: i32;
    var color1_6: vec4<f32>;
    var param_24: u32;
    var param_25: vec2<f32>;
    var param_26: i32;

    let _e91 = frag_color0In_1;
    param_1 = _e91.xyz;
    let _e93 = sRGBToLinear_u0028_vf3_u003b((&param_1));
    let _e95 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e93.x, _e93.y, _e93.z, _e95);
    let _e100 = frag_color1In_1;
    param_2 = _e100.xyz;
    let _e102 = sRGBToLinear_u0028_vf3_u003b((&param_2));
    let _e104 = frag_color1In_1[3u];
    frag_color1_ = vec4<f32>(_e102.x, _e102.y, _e102.z, _e104);
    param_3 = 0u;
    let _e109 = frag_tex_coord0_1;
    param_4 = _e109;
    param_5 = 0i;
    let _e110 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_3), (&param_4), (&param_5));
    let _e111 = frag_color0_;
    color0_ = (_e110 * _e111);
    if override_type_10_2 {
        param_6 = 1u;
        let _e113 = frag_tex_coord1_1;
        param_7 = _e113;
        param_8 = 1i;
        let _e114 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_6), (&param_7), (&param_8));
        let _e115 = frag_color1_;
        color1_ = (_e114 * _e115);
        let _e117 = color0_;
        let _e119 = color1_;
        let _e121 = (_e117.xyz + _e119.xyz);
        let _e123 = color0_[3u];
        let _e125 = color1_[3u];
        base = vec4<f32>(_e121.x, _e121.y, _e121.z, (_e123 * _e125));
    } else {
        if override_type_10_3 {
            param_9 = 1u;
            let _e131 = frag_tex_coord1_1;
            param_10 = _e131;
            param_11 = 1i;
            let _e132 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_9), (&param_10), (&param_11));
            let _e133 = frag_color1_;
            color1_1 = (_e132 * _e133);
            let _e136 = color0_[3u];
            let _e137 = color0_;
            color0_ = (_e137 * _e136);
            let _e140 = color1_1[3u];
            let _e141 = color1_1;
            color1_1 = (_e141 * _e140);
            let _e143 = color0_;
            let _e145 = color1_1;
            let _e147 = (_e143.xyz + _e145.xyz);
            let _e149 = color0_[3u];
            let _e151 = color1_1[3u];
            base = vec4<f32>(_e147.x, _e147.y, _e147.z, (_e149 * _e151));
        } else {
            if override_type_10_4 {
                param_12 = 1u;
                let _e157 = frag_tex_coord1_1;
                param_13 = _e157;
                param_14 = 1i;
                let _e158 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_12), (&param_13), (&param_14));
                let _e159 = frag_color1_;
                color1_2 = (_e158 * _e159);
                let _e162 = color0_[3u];
                let _e164 = color0_;
                color0_ = (_e164 * (1f - _e162));
                let _e167 = color1_2[3u];
                let _e169 = color1_2;
                color1_2 = (_e169 * (1f - _e167));
                let _e171 = color0_;
                let _e173 = color1_2;
                let _e175 = (_e171.xyz + _e173.xyz);
                let _e177 = color0_[3u];
                let _e179 = color1_2[3u];
                base = vec4<f32>(_e175.x, _e175.y, _e175.z, (_e177 * _e179));
            } else {
                if override_type_10_5 {
                    param_15 = 1u;
                    let _e185 = frag_tex_coord1_1;
                    param_16 = _e185;
                    param_17 = 1i;
                    let _e186 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_15), (&param_16), (&param_17));
                    let _e187 = frag_color1_;
                    color1_3 = (_e186 * _e187);
                    let _e189 = color0_;
                    let _e190 = color1_3;
                    let _e192 = color1_3[3u];
                    base = mix(_e189, _e190, vec4(_e192));
                } else {
                    if override_type_10_6 {
                        param_18 = 1u;
                        let _e195 = frag_tex_coord1_1;
                        param_19 = _e195;
                        param_20 = 1i;
                        let _e196 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_18), (&param_19), (&param_20));
                        let _e197 = frag_color1_;
                        color1_4 = (_e196 * _e197);
                        let _e199 = color1_4;
                        let _e200 = color0_;
                        let _e202 = color1_4[3u];
                        base = mix(_e199, _e200, vec4(_e202));
                    } else {
                        if override_type_10_7 {
                            param_21 = 1u;
                            let _e205 = frag_tex_coord1_1;
                            param_22 = _e205;
                            param_23 = 1i;
                            let _e206 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_21), (&param_22), (&param_23));
                            let _e207 = frag_color1_;
                            color1_5 = (_e206 * _e207);
                            let _e209 = color1_5;
                            let _e211 = color1_5[3u];
                            let _e214 = color0_;
                            base = ((_e209 + vec4(_e211)) * _e214);
                        } else {
                            param_24 = 1u;
                            let _e216 = frag_tex_coord1_1;
                            param_25 = _e216;
                            param_26 = 1i;
                            let _e217 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_24), (&param_25), (&param_26));
                            let _e218 = frag_color1_;
                            color1_6 = (_e217 * _e218);
                            let _e220 = color0_;
                            let _e222 = color1_6;
                            let _e224 = (_e220.xyz * _e222.xyz);
                            base[0u] = _e224.x;
                            base[1u] = _e224.y;
                            base[2u] = _e224.z;
                            let _e232 = color0_[3u];
                            let _e234 = color1_6[3u];
                            base[3u] = (_e232 * _e234);
                        }
                    }
                }
            }
        }
    }
    if override_type_10_8 {
        let _e238 = base[3u];
        if (_e238 == 0f) {
            discard;
        }
    } else {
        if override_type_10_9 {
            let _e240 = base;
            let _e242 = base;
            if (dot(_e240.xyz, _e242.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e246 = base;
    out_color = _e246;
    return;
}

@fragment 
fn main(@location(0) frag_color0In: vec4<f32>, @location(5) frag_color1In: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>) -> @location(0) vec4<f32> {
    frag_color0In_1 = frag_color0In;
    frag_color1In_1 = frag_color1In;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    main_1();
    let _e9 = out_color;
    return _e9;
}
