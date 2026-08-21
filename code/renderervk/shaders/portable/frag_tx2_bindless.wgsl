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
@id(7) override discard_mode: i32 = 0i;
override override_type_10_2: bool = (discard_mode == 1i);
override override_type_10_3: bool = (discard_mode == 2i);
@id(3) override alpha_to_coverage: i32 = 0i;

@group(1) @binding(0) 
var wired_bindless_images: binding_array<texture_2d<f32>>;
@group(0) @binding(0) 
var<uniform> unnamed: UBO;
@group(1) @binding(1) 
var wired_bindless_samplers: binding_array<sampler>;
var<private> frag_color0In_1: vec4<f32>;
var<private> frag_tex_coord0_1: vec2<f32>;
var<private> frag_tex_coord1_1: vec2<f32>;
var<private> frag_tex_coord2_1: vec2<f32>;
var<private> out_color: vec4<f32>;

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e47 = (*c);
    (*c) = max(_e47, vec3<f32>(0f, 0f, 0f));
    let _e49 = (*c);
    cutoff = (_e49 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e51 = (*c);
    lo = (_e51 / vec3(12.92f));
    let _e54 = (*c);
    hi = pow(((_e54 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e59 = hi;
    let _e60 = lo;
    let _e61 = cutoff;
    return mix(_e59, _e60, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e61));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param: vec3<f32>;

    let _e48 = (*role);
    let _e50 = (*role);
    let _e55 = unnamed.packed_indices[(_e48 / 4u)][(_e50 % 4u)];
    let _e58 = (*role);
    let _e60 = (*role);
    let _e65 = unnamed.packed_indices[(_e58 / 4u)][(_e60 % 4u)];
    let _e70 = (*uv);
    let _e71 = textureSample(wired_bindless_images[(_e55 & 4095u)], wired_bindless_samplers[((_e65 >> bitcast<u32>(12i)) & 255u)], _e70);
    c_1 = _e71;
    let _e72 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e72))) == 0i) {
        let _e77 = c_1;
        param = _e77.xyz;
        let _e79 = sRGBToLinear_u0028_vf3_u003b((&param));
        c_1[0u] = _e79.x;
        c_1[1u] = _e79.y;
        c_1[2u] = _e79.z;
    }
    let _e86 = (*slot);
    if (lightmap_slot == (_e86 + 1i)) {
        let _e91 = unnamed.worldLightParams[0u];
        let _e92 = c_1;
        let _e94 = (_e92.xyz * _e91);
        c_1[0u] = _e94.x;
        c_1[1u] = _e94.y;
        c_1[2u] = _e94.z;
    }
    let _e101 = c_1;
    return _e101;
}

fn main_1() {
    var frag_color0_: vec4<f32>;
    var param_1: vec3<f32>;
    var color0_: vec4<f32>;
    var param_2: u32;
    var param_3: vec2<f32>;
    var param_4: i32;
    var color1_: vec4<f32>;
    var param_5: u32;
    var param_6: vec2<f32>;
    var param_7: i32;
    var color2_: vec4<f32>;
    var param_8: u32;
    var param_9: vec2<f32>;
    var param_10: i32;
    var base: vec4<f32>;
    var color1_1: vec4<f32>;
    var param_11: u32;
    var param_12: vec2<f32>;
    var param_13: i32;
    var color2_1: vec4<f32>;
    var param_14: u32;
    var param_15: vec2<f32>;
    var param_16: i32;
    var color1_2: vec4<f32>;
    var param_17: u32;
    var param_18: vec2<f32>;
    var param_19: i32;
    var color2_2: vec4<f32>;
    var param_20: u32;
    var param_21: vec2<f32>;
    var param_22: i32;

    let _e74 = frag_color0In_1;
    param_1 = _e74.xyz;
    let _e76 = sRGBToLinear_u0028_vf3_u003b((&param_1));
    let _e78 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e76.x, _e76.y, _e76.z, _e78);
    param_2 = 0u;
    let _e83 = frag_tex_coord0_1;
    param_3 = _e83;
    param_4 = 0i;
    let _e84 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_2), (&param_3), (&param_4));
    let _e85 = frag_color0_;
    color0_ = (_e84 * _e85);
    if override_type_10_ {
        param_5 = 1u;
        let _e87 = frag_tex_coord1_1;
        param_6 = _e87;
        param_7 = 1i;
        let _e88 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_5), (&param_6), (&param_7));
        color1_ = _e88;
        param_8 = 2u;
        let _e89 = frag_tex_coord2_1;
        param_9 = _e89;
        param_10 = 2i;
        let _e90 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_8), (&param_9), (&param_10));
        color2_ = _e90;
        let _e91 = color0_;
        let _e93 = color1_;
        let _e96 = color2_;
        let _e98 = ((_e91.xyz + _e93.xyz) + _e96.xyz);
        let _e100 = color0_[3u];
        let _e102 = color1_[3u];
        let _e105 = color2_[3u];
        base = vec4<f32>(_e98.x, _e98.y, _e98.z, ((_e100 * _e102) * _e105));
    } else {
        if override_type_10_1 {
            param_11 = 1u;
            let _e111 = frag_tex_coord1_1;
            param_12 = _e111;
            param_13 = 1i;
            let _e112 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_11), (&param_12), (&param_13));
            let _e113 = frag_color0_;
            color1_1 = (_e112 * _e113);
            param_14 = 2u;
            let _e115 = frag_tex_coord2_1;
            param_15 = _e115;
            param_16 = 2i;
            let _e116 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_14), (&param_15), (&param_16));
            let _e117 = frag_color0_;
            color2_1 = (_e116 * _e117);
            let _e119 = color0_;
            let _e121 = color1_1;
            let _e124 = color2_1;
            let _e126 = ((_e119.xyz + _e121.xyz) + _e124.xyz);
            let _e128 = color0_[3u];
            let _e130 = color1_1[3u];
            let _e133 = color2_1[3u];
            base = vec4<f32>(_e126.x, _e126.y, _e126.z, ((_e128 * _e130) * _e133));
        } else {
            param_17 = 1u;
            let _e139 = frag_tex_coord1_1;
            param_18 = _e139;
            param_19 = 1i;
            let _e140 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_17), (&param_18), (&param_19));
            color1_2 = _e140;
            param_20 = 2u;
            let _e141 = frag_tex_coord2_1;
            param_21 = _e141;
            param_22 = 2i;
            let _e142 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_20), (&param_21), (&param_22));
            color2_2 = _e142;
            let _e143 = color0_;
            let _e145 = color1_2;
            let _e148 = color2_2;
            let _e150 = ((_e143.xyz * _e145.xyz) * _e148.xyz);
            base[0u] = _e150.x;
            base[1u] = _e150.y;
            base[2u] = _e150.z;
            let _e158 = color0_[3u];
            let _e160 = color1_2[3u];
            let _e163 = color2_2[3u];
            base[3u] = ((_e158 * _e160) * _e163);
        }
    }
    if override_type_10_2 {
        let _e167 = base[3u];
        if (_e167 == 0f) {
            discard;
        }
    } else {
        if override_type_10_3 {
            let _e169 = base;
            let _e171 = base;
            if (dot(_e169.xyz, _e171.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e175 = base;
    out_color = _e175;
    return;
}

@fragment 
fn main(@location(0) frag_color0In: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>, @location(3) frag_tex_coord2_: vec2<f32>) -> @location(0) vec4<f32> {
    frag_color0In_1 = frag_color0In;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    frag_tex_coord2_1 = frag_tex_coord2_;
    main_1();
    let _e9 = out_color;
    return _e9;
}
