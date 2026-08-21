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
@id(0) override alpha_test_func: i32 = 0i;
override override_type_10_: bool = (alpha_test_func == 1i);
@id(1) override alpha_test_value: f32 = 0f;
override override_type_10_1: bool = (alpha_test_func == 2i);
override override_type_10_2: bool = (alpha_test_func == 3i);
@id(7) override discard_mode: i32 = 0i;
override override_type_10_3: bool = (discard_mode == 1i);
override override_type_10_4: bool = (discard_mode == 2i);
@id(3) override alpha_to_coverage: i32 = 0i;

@group(1) @binding(0) 
var wired_bindless_images: binding_array<texture_2d<f32>>;
@group(0) @binding(0) 
var<uniform> unnamed: UBO;
@group(1) @binding(1) 
var wired_bindless_samplers: binding_array<sampler>;
var<private> frag_color0In_1: vec4<f32>;
var<private> frag_tex_coord0_1: vec2<f32>;
var<private> out_color: vec4<f32>;

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e48 = (*c);
    (*c) = max(_e48, vec3<f32>(0f, 0f, 0f));
    let _e50 = (*c);
    cutoff = (_e50 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e52 = (*c);
    lo = (_e52 / vec3(12.92f));
    let _e55 = (*c);
    hi = pow(((_e55 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e60 = hi;
    let _e61 = lo;
    let _e62 = cutoff;
    return mix(_e60, _e61, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e62));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param: vec3<f32>;

    let _e49 = (*role);
    let _e51 = (*role);
    let _e56 = unnamed.packed_indices[(_e49 / 4u)][(_e51 % 4u)];
    let _e59 = (*role);
    let _e61 = (*role);
    let _e66 = unnamed.packed_indices[(_e59 / 4u)][(_e61 % 4u)];
    let _e71 = (*uv);
    let _e72 = textureSample(wired_bindless_images[(_e56 & 4095u)], wired_bindless_samplers[((_e66 >> bitcast<u32>(12i)) & 255u)], _e71);
    c_1 = _e72;
    let _e73 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e73))) == 0i) {
        let _e78 = c_1;
        param = _e78.xyz;
        let _e80 = sRGBToLinear_u0028_vf3_u003b((&param));
        c_1[0u] = _e80.x;
        c_1[1u] = _e80.y;
        c_1[2u] = _e80.z;
    }
    let _e87 = (*slot);
    if (lightmap_slot == (_e87 + 1i)) {
        let _e92 = unnamed.worldLightParams[0u];
        let _e93 = c_1;
        let _e95 = (_e93.xyz * _e92);
        c_1[0u] = _e95.x;
        c_1[1u] = _e95.y;
        c_1[2u] = _e95.z;
    }
    let _e102 = c_1;
    return _e102;
}

fn main_1() {
    var frag_color0_: vec4<f32>;
    var param_1: vec3<f32>;
    var color0_: vec4<f32>;
    var param_2: u32;
    var param_3: vec2<f32>;
    var param_4: i32;
    var base: vec4<f32>;

    let _e51 = frag_color0In_1;
    param_1 = _e51.xyz;
    let _e53 = sRGBToLinear_u0028_vf3_u003b((&param_1));
    let _e55 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e53.x, _e53.y, _e53.z, _e55);
    param_2 = 0u;
    let _e60 = frag_tex_coord0_1;
    param_3 = _e60;
    param_4 = 0i;
    let _e61 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_2), (&param_3), (&param_4));
    let _e62 = frag_color0_;
    color0_ = (_e61 * _e62);
    let _e64 = color0_;
    base = _e64;
    if override_type_10_ {
        let _e66 = color0_[3u];
        if (_e66 == alpha_test_value) {
            discard;
        }
    } else {
        if override_type_10_1 {
            let _e69 = color0_[3u];
            if (_e69 >= alpha_test_value) {
                discard;
            }
        } else {
            if override_type_10_2 {
                let _e72 = color0_[3u];
                if (_e72 < alpha_test_value) {
                    discard;
                }
            }
        }
    }
    let _e74 = color0_;
    base = _e74;
    if override_type_10_3 {
        let _e76 = base[3u];
        if (_e76 == 0f) {
            discard;
        }
    } else {
        if override_type_10_4 {
            let _e78 = base;
            let _e80 = base;
            if (dot(_e78.xyz, _e80.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e84 = base;
    out_color = _e84;
    return;
}

@fragment 
fn main(@location(0) frag_color0In: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>) -> @location(0) vec4<f32> {
    frag_color0In_1 = frag_color0In;
    frag_tex_coord0_1 = frag_tex_coord0_;
    main_1();
    let _e5 = out_color;
    return _e5;
}
