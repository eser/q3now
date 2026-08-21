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
var<private> out_color: vec4<f32>;

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e46 = (*c);
    (*c) = max(_e46, vec3<f32>(0f, 0f, 0f));
    let _e48 = (*c);
    cutoff = (_e48 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e50 = (*c);
    lo = (_e50 / vec3(12.92f));
    let _e53 = (*c);
    hi = pow(((_e53 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e58 = hi;
    let _e59 = lo;
    let _e60 = cutoff;
    return mix(_e58, _e59, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e60));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param: vec3<f32>;

    let _e47 = (*role);
    let _e49 = (*role);
    let _e54 = unnamed.packed_indices[(_e47 / 4u)][(_e49 % 4u)];
    let _e57 = (*role);
    let _e59 = (*role);
    let _e64 = unnamed.packed_indices[(_e57 / 4u)][(_e59 % 4u)];
    let _e69 = (*uv);
    let _e70 = textureSample(wired_bindless_images[(_e54 & 4095u)], wired_bindless_samplers[((_e64 >> bitcast<u32>(12i)) & 255u)], _e69);
    c_1 = _e70;
    let _e71 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e71))) == 0i) {
        let _e76 = c_1;
        param = _e76.xyz;
        let _e78 = sRGBToLinear_u0028_vf3_u003b((&param));
        c_1[0u] = _e78.x;
        c_1[1u] = _e78.y;
        c_1[2u] = _e78.z;
    }
    let _e85 = (*slot);
    if (lightmap_slot == (_e85 + 1i)) {
        let _e90 = unnamed.worldLightParams[0u];
        let _e91 = c_1;
        let _e93 = (_e91.xyz * _e90);
        c_1[0u] = _e93.x;
        c_1[1u] = _e93.y;
        c_1[2u] = _e93.z;
    }
    let _e100 = c_1;
    return _e100;
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
    var base: vec4<f32>;
    var color1_1: vec4<f32>;
    var param_8: u32;
    var param_9: vec2<f32>;
    var param_10: i32;
    var color1_2: vec4<f32>;
    var param_11: u32;
    var param_12: vec2<f32>;
    var param_13: i32;

    let _e61 = frag_color0In_1;
    param_1 = _e61.xyz;
    let _e63 = sRGBToLinear_u0028_vf3_u003b((&param_1));
    let _e65 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e63.x, _e63.y, _e63.z, _e65);
    param_2 = 0u;
    let _e70 = frag_tex_coord0_1;
    param_3 = _e70;
    param_4 = 0i;
    let _e71 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_2), (&param_3), (&param_4));
    let _e72 = frag_color0_;
    color0_ = (_e71 * _e72);
    if override_type_10_ {
        param_5 = 1u;
        let _e74 = frag_tex_coord1_1;
        param_6 = _e74;
        param_7 = 1i;
        let _e75 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_5), (&param_6), (&param_7));
        color1_ = _e75;
        let _e76 = color0_;
        let _e78 = color1_;
        let _e80 = (_e76.xyz + _e78.xyz);
        let _e82 = color0_[3u];
        let _e84 = color1_[3u];
        base = vec4<f32>(_e80.x, _e80.y, _e80.z, (_e82 * _e84));
    } else {
        if override_type_10_1 {
            param_8 = 1u;
            let _e90 = frag_tex_coord1_1;
            param_9 = _e90;
            param_10 = 1i;
            let _e91 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_8), (&param_9), (&param_10));
            let _e92 = frag_color0_;
            color1_1 = (_e91 * _e92);
            let _e94 = color0_;
            let _e96 = color1_1;
            let _e98 = (_e94.xyz + _e96.xyz);
            let _e100 = color0_[3u];
            let _e102 = color1_1[3u];
            base = vec4<f32>(_e98.x, _e98.y, _e98.z, (_e100 * _e102));
        } else {
            param_11 = 1u;
            let _e108 = frag_tex_coord1_1;
            param_12 = _e108;
            param_13 = 1i;
            let _e109 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_11), (&param_12), (&param_13));
            color1_2 = _e109;
            let _e110 = color0_;
            let _e112 = color1_2;
            let _e114 = (_e110.xyz * _e112.xyz);
            base[0u] = _e114.x;
            base[1u] = _e114.y;
            base[2u] = _e114.z;
            let _e122 = color0_[3u];
            let _e124 = color1_2[3u];
            base[3u] = (_e122 * _e124);
        }
    }
    if override_type_10_2 {
        let _e128 = base[3u];
        if (_e128 == 0f) {
            discard;
        }
    } else {
        if override_type_10_3 {
            let _e130 = base;
            let _e132 = base;
            if (dot(_e130.xyz, _e132.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e136 = base;
    out_color = _e136;
    return;
}

@fragment 
fn main(@location(0) frag_color0In: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>) -> @location(0) vec4<f32> {
    frag_color0In_1 = frag_color0In;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    main_1();
    let _e7 = out_color;
    return _e7;
}
