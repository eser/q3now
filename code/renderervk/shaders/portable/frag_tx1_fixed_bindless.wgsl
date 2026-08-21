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
@id(8) override identity_color: f32 = 1f;
@id(9) override identity_alpha: f32 = 1f;
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
var<private> frag_tex_coord0_1: vec2<f32>;
var<private> frag_tex_coord1_1: vec2<f32>;
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
    var frag_color: vec4<f32>;
    var color0_: vec4<f32>;
    var param_1: u32;
    var param_2: vec2<f32>;
    var param_3: i32;
    var color1_: vec4<f32>;
    var param_4: u32;
    var param_5: vec2<f32>;
    var param_6: i32;
    var base: vec4<f32>;
    var color1_1: vec4<f32>;
    var param_7: u32;
    var param_8: vec2<f32>;
    var param_9: i32;
    var color1_2: vec4<f32>;
    var param_10: u32;
    var param_11: vec2<f32>;
    var param_12: i32;

    frag_color[0u] = identity_color;
    frag_color[1u] = identity_color;
    frag_color[2u] = identity_color;
    frag_color[3u] = identity_alpha;
    param_1 = 0u;
    let _e65 = frag_tex_coord0_1;
    param_2 = _e65;
    param_3 = 0i;
    let _e66 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_1), (&param_2), (&param_3));
    let _e67 = frag_color;
    color0_ = (_e66 * _e67);
    if override_type_10_ {
        param_4 = 1u;
        let _e69 = frag_tex_coord1_1;
        param_5 = _e69;
        param_6 = 1i;
        let _e70 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_4), (&param_5), (&param_6));
        color1_ = _e70;
        let _e71 = color0_;
        let _e73 = color1_;
        let _e75 = (_e71.xyz + _e73.xyz);
        let _e77 = color0_[3u];
        let _e79 = color1_[3u];
        base = vec4<f32>(_e75.x, _e75.y, _e75.z, (_e77 * _e79));
    } else {
        if override_type_10_1 {
            param_7 = 1u;
            let _e85 = frag_tex_coord1_1;
            param_8 = _e85;
            param_9 = 1i;
            let _e86 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_7), (&param_8), (&param_9));
            let _e87 = frag_color;
            color1_1 = (_e86 * _e87);
            let _e89 = color0_;
            let _e91 = color1_1;
            let _e93 = (_e89.xyz + _e91.xyz);
            let _e95 = color0_[3u];
            let _e97 = color1_1[3u];
            base = vec4<f32>(_e93.x, _e93.y, _e93.z, (_e95 * _e97));
        } else {
            param_10 = 1u;
            let _e103 = frag_tex_coord1_1;
            param_11 = _e103;
            param_12 = 1i;
            let _e104 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_10), (&param_11), (&param_12));
            let _e105 = frag_color;
            color1_2 = (_e104 * _e105);
            let _e107 = color0_;
            let _e109 = color1_2;
            let _e111 = (_e107.xyz * _e109.xyz);
            base[0u] = _e111.x;
            base[1u] = _e111.y;
            base[2u] = _e111.z;
            let _e119 = color0_[3u];
            let _e121 = color1_2[3u];
            base[3u] = (_e119 * _e121);
        }
    }
    if override_type_10_2 {
        let _e125 = base[3u];
        if (_e125 == 0f) {
            discard;
        }
    } else {
        if override_type_10_3 {
            let _e127 = base;
            let _e129 = base;
            if (dot(_e127.xyz, _e129.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e133 = base;
    out_color = _e133;
    return;
}

@fragment 
fn main(@location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>) -> @location(0) vec4<f32> {
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    main_1();
    let _e5 = out_color;
    return _e5;
}
