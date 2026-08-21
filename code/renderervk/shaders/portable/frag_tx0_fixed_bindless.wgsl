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
var<private> frag_tex_coord0_1: vec2<f32>;
var<private> out_color: vec4<f32>;

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e49 = (*c);
    (*c) = max(_e49, vec3<f32>(0f, 0f, 0f));
    let _e51 = (*c);
    cutoff = (_e51 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e53 = (*c);
    lo = (_e53 / vec3(12.92f));
    let _e56 = (*c);
    hi = pow(((_e56 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e61 = hi;
    let _e62 = lo;
    let _e63 = cutoff;
    return mix(_e61, _e62, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e63));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param: vec3<f32>;

    let _e50 = (*role);
    let _e52 = (*role);
    let _e57 = unnamed.packed_indices[(_e50 / 4u)][(_e52 % 4u)];
    let _e60 = (*role);
    let _e62 = (*role);
    let _e67 = unnamed.packed_indices[(_e60 / 4u)][(_e62 % 4u)];
    let _e72 = (*uv);
    let _e73 = textureSample(wired_bindless_images[(_e57 & 4095u)], wired_bindless_samplers[((_e67 >> bitcast<u32>(12i)) & 255u)], _e72);
    c_1 = _e73;
    let _e74 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e74))) == 0i) {
        let _e79 = c_1;
        param = _e79.xyz;
        let _e81 = sRGBToLinear_u0028_vf3_u003b((&param));
        c_1[0u] = _e81.x;
        c_1[1u] = _e81.y;
        c_1[2u] = _e81.z;
    }
    let _e88 = (*slot);
    if (lightmap_slot == (_e88 + 1i)) {
        let _e93 = unnamed.worldLightParams[0u];
        let _e94 = c_1;
        let _e96 = (_e94.xyz * _e93);
        c_1[0u] = _e96.x;
        c_1[1u] = _e96.y;
        c_1[2u] = _e96.z;
    }
    let _e103 = c_1;
    return _e103;
}

fn main_1() {
    var frag_color: vec4<f32>;
    var color0_: vec4<f32>;
    var param_1: u32;
    var param_2: vec2<f32>;
    var param_3: i32;
    var base: vec4<f32>;

    frag_color[0u] = identity_color;
    frag_color[1u] = identity_color;
    frag_color[2u] = identity_color;
    frag_color[3u] = identity_alpha;
    param_1 = 0u;
    let _e55 = frag_tex_coord0_1;
    param_2 = _e55;
    param_3 = 0i;
    let _e56 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_1), (&param_2), (&param_3));
    let _e57 = frag_color;
    color0_ = (_e56 * _e57);
    let _e59 = color0_;
    base = _e59;
    if override_type_10_ {
        let _e61 = color0_[3u];
        if (_e61 == alpha_test_value) {
            discard;
        }
    } else {
        if override_type_10_1 {
            let _e64 = color0_[3u];
            if (_e64 >= alpha_test_value) {
                discard;
            }
        } else {
            if override_type_10_2 {
                let _e67 = color0_[3u];
                if (_e67 < alpha_test_value) {
                    discard;
                }
            }
        }
    }
    let _e69 = color0_;
    base = _e69;
    if override_type_10_3 {
        let _e71 = base[3u];
        if (_e71 == 0f) {
            discard;
        }
    } else {
        if override_type_10_4 {
            let _e73 = base;
            let _e75 = base;
            if (dot(_e73.xyz, _e75.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e79 = base;
    out_color = _e79;
    return;
}

@fragment 
fn main(@location(1) frag_tex_coord0_: vec2<f32>) -> @location(0) vec4<f32> {
    frag_tex_coord0_1 = frag_tex_coord0_;
    main_1();
    let _e3 = out_color;
    return _e3;
}
