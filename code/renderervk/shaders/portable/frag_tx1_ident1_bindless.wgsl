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
var<private> frag_tex_coord0_1: vec2<f32>;
var<private> frag_tex_coord1_1: vec2<f32>;
var<private> out_color: vec4<f32>;

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e45 = (*c);
    (*c) = max(_e45, vec3<f32>(0f, 0f, 0f));
    let _e47 = (*c);
    cutoff = (_e47 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e49 = (*c);
    lo = (_e49 / vec3(12.92f));
    let _e52 = (*c);
    hi = pow(((_e52 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e57 = hi;
    let _e58 = lo;
    let _e59 = cutoff;
    return mix(_e57, _e58, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e59));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param: vec3<f32>;

    let _e46 = (*role);
    let _e48 = (*role);
    let _e53 = unnamed.packed_indices[(_e46 / 4u)][(_e48 % 4u)];
    let _e56 = (*role);
    let _e58 = (*role);
    let _e63 = unnamed.packed_indices[(_e56 / 4u)][(_e58 % 4u)];
    let _e68 = (*uv);
    let _e69 = textureSample(wired_bindless_images[(_e53 & 4095u)], wired_bindless_samplers[((_e63 >> bitcast<u32>(12i)) & 255u)], _e68);
    c_1 = _e69;
    let _e70 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e70))) == 0i) {
        let _e75 = c_1;
        param = _e75.xyz;
        let _e77 = sRGBToLinear_u0028_vf3_u003b((&param));
        c_1[0u] = _e77.x;
        c_1[1u] = _e77.y;
        c_1[2u] = _e77.z;
    }
    let _e84 = (*slot);
    if (lightmap_slot == (_e84 + 1i)) {
        let _e89 = unnamed.worldLightParams[0u];
        let _e90 = c_1;
        let _e92 = (_e90.xyz * _e89);
        c_1[0u] = _e92.x;
        c_1[1u] = _e92.y;
        c_1[2u] = _e92.z;
    }
    let _e99 = c_1;
    return _e99;
}

fn main_1() {
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

    param_1 = 0u;
    let _e58 = frag_tex_coord0_1;
    param_2 = _e58;
    param_3 = 0i;
    let _e59 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_1), (&param_2), (&param_3));
    color0_ = _e59;
    if override_type_10_ {
        param_4 = 1u;
        let _e60 = frag_tex_coord1_1;
        param_5 = _e60;
        param_6 = 1i;
        let _e61 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_4), (&param_5), (&param_6));
        color1_ = _e61;
        let _e62 = color0_;
        let _e64 = color1_;
        let _e66 = (_e62.xyz + _e64.xyz);
        let _e68 = color0_[3u];
        let _e70 = color1_[3u];
        base = vec4<f32>(_e66.x, _e66.y, _e66.z, (_e68 * _e70));
    } else {
        if override_type_10_1 {
            param_7 = 1u;
            let _e76 = frag_tex_coord1_1;
            param_8 = _e76;
            param_9 = 1i;
            let _e77 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_7), (&param_8), (&param_9));
            color1_1 = _e77;
            let _e78 = color0_;
            let _e80 = color1_1;
            let _e82 = (_e78.xyz + _e80.xyz);
            let _e84 = color0_[3u];
            let _e86 = color1_1[3u];
            base = vec4<f32>(_e82.x, _e82.y, _e82.z, (_e84 * _e86));
        } else {
            param_10 = 1u;
            let _e92 = frag_tex_coord1_1;
            param_11 = _e92;
            param_12 = 1i;
            let _e93 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_10), (&param_11), (&param_12));
            color1_2 = _e93;
            let _e94 = color0_;
            let _e96 = color1_2;
            let _e98 = (_e94.xyz * _e96.xyz);
            base[0u] = _e98.x;
            base[1u] = _e98.y;
            base[2u] = _e98.z;
            let _e106 = color0_[3u];
            let _e108 = color1_2[3u];
            base[3u] = (_e106 * _e108);
        }
    }
    if override_type_10_2 {
        let _e112 = base[3u];
        if (_e112 == 0f) {
            discard;
        }
    } else {
        if override_type_10_3 {
            let _e114 = base;
            let _e116 = base;
            if (dot(_e114.xyz, _e116.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e120 = base;
    out_color = _e120;
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
