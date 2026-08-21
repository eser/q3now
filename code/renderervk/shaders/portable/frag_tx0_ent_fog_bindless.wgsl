enable wgpu_binding_array;

struct UBO {
    eyePos: vec4<f32>,
    ent_color0_: vec4<f32>,
    ent_color1_: vec4<f32>,
    ent_color2_: vec4<f32>,
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
@id(10) override acff: i32 = 0i;
override override_type_10_3: bool = (acff == 1i);
override override_type_10_4: bool = (acff == 2i);
override override_type_10_5: bool = (acff == 3i);
@id(7) override discard_mode: i32 = 0i;
override override_type_10_6: bool = (discard_mode == 1i);
override override_type_10_7: bool = (discard_mode == 2i);
@id(3) override alpha_to_coverage: i32 = 0i;

@group(1) @binding(0) 
var wired_bindless_images: binding_array<texture_2d<f32>>;
@group(0) @binding(0) 
var<uniform> unnamed: UBO;
@group(1) @binding(1) 
var wired_bindless_samplers: binding_array<sampler>;
var<private> fog_tex_coord_1: vec2<f32>;
var<private> frag_tex_coord0_1: vec2<f32>;
var<private> out_color: vec4<f32>;

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e53 = (*c);
    (*c) = max(_e53, vec3<f32>(0f, 0f, 0f));
    let _e55 = (*c);
    cutoff = (_e55 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e57 = (*c);
    lo = (_e57 / vec3(12.92f));
    let _e60 = (*c);
    hi = pow(((_e60 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e65 = hi;
    let _e66 = lo;
    let _e67 = cutoff;
    return mix(_e65, _e66, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e67));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param: vec3<f32>;

    let _e54 = (*role);
    let _e56 = (*role);
    let _e61 = unnamed.packed_indices[(_e54 / 4u)][(_e56 % 4u)];
    let _e64 = (*role);
    let _e66 = (*role);
    let _e71 = unnamed.packed_indices[(_e64 / 4u)][(_e66 % 4u)];
    let _e76 = (*uv);
    let _e77 = textureSample(wired_bindless_images[(_e61 & 4095u)], wired_bindless_samplers[((_e71 >> bitcast<u32>(12i)) & 255u)], _e76);
    c_1 = _e77;
    let _e78 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e78))) == 0i) {
        let _e83 = c_1;
        param = _e83.xyz;
        let _e85 = sRGBToLinear_u0028_vf3_u003b((&param));
        c_1[0u] = _e85.x;
        c_1[1u] = _e85.y;
        c_1[2u] = _e85.z;
    }
    let _e92 = (*slot);
    if (lightmap_slot == (_e92 + 1i)) {
        let _e97 = unnamed.worldLightParams[0u];
        let _e98 = c_1;
        let _e100 = (_e98.xyz * _e97);
        c_1[0u] = _e100.x;
        c_1[1u] = _e100.y;
        c_1[2u] = _e100.z;
    }
    let _e107 = c_1;
    return _e107;
}

fn main_1() {
    var fog: vec4<f32>;
    var color0_: vec4<f32>;
    var param_1: u32;
    var param_2: vec2<f32>;
    var param_3: i32;
    var base: vec4<f32>;

    let _e58 = unnamed.packed_indices[0i][3u];
    let _e64 = unnamed.packed_indices[0i][3u];
    let _e69 = fog_tex_coord_1;
    let _e70 = textureSample(wired_bindless_images[(_e58 & 4095u)], wired_bindless_samplers[((_e64 >> bitcast<u32>(12i)) & 255u)], _e69);
    fog = _e70;
    param_1 = 0u;
    let _e71 = frag_tex_coord0_1;
    param_2 = _e71;
    param_3 = 0i;
    let _e72 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_1), (&param_2), (&param_3));
    let _e74 = unnamed.ent_color0_;
    color0_ = (_e72 * _e74);
    let _e76 = color0_;
    base = _e76;
    if override_type_10_ {
        let _e78 = color0_[3u];
        if (_e78 == alpha_test_value) {
            discard;
        }
    } else {
        if override_type_10_1 {
            let _e81 = color0_[3u];
            if (_e81 >= alpha_test_value) {
                discard;
            }
        } else {
            if override_type_10_2 {
                let _e84 = color0_[3u];
                if (_e84 < alpha_test_value) {
                    discard;
                }
            }
        }
    }
    let _e86 = color0_;
    base = _e86;
    if override_type_10_3 {
        let _e87 = base;
        let _e90 = fog[3u];
        let _e92 = (_e87.xyz * (1f - _e90));
        base[0u] = _e92.x;
        base[1u] = _e92.y;
        base[2u] = _e92.z;
    } else {
        if override_type_10_4 {
            let _e99 = base;
            let _e101 = fog[3u];
            base = (_e99 * (1f - _e101));
        } else {
            if override_type_10_5 {
                let _e105 = base[3u];
                let _e107 = fog[3u];
                base[3u] = (_e105 * (1f - _e107));
            } else {
                let _e111 = base;
                let _e112 = fog;
                let _e114 = unnamed.fogColor;
                let _e117 = fog[3u];
                base = mix(_e111, (_e112 * _e114), vec4(_e117));
            }
        }
    }
    if override_type_10_6 {
        let _e121 = base[3u];
        if (_e121 == 0f) {
            discard;
        }
    } else {
        if override_type_10_7 {
            let _e123 = base;
            let _e125 = base;
            if (dot(_e123.xyz, _e125.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e129 = base;
    out_color = _e129;
    return;
}

@fragment 
fn main(@location(4) fog_tex_coord: vec2<f32>, @location(1) frag_tex_coord0_: vec2<f32>) -> @location(0) vec4<f32> {
    fog_tex_coord_1 = fog_tex_coord;
    frag_tex_coord0_1 = frag_tex_coord0_;
    main_1();
    let _e5 = out_color;
    return _e5;
}
