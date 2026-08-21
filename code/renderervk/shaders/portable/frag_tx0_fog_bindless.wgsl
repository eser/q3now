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
var<private> frag_color0In_1: vec4<f32>;
var<private> frag_tex_coord0_1: vec2<f32>;
var<private> out_color: vec4<f32>;

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e54 = (*c);
    (*c) = max(_e54, vec3<f32>(0f, 0f, 0f));
    let _e56 = (*c);
    cutoff = (_e56 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e58 = (*c);
    lo = (_e58 / vec3(12.92f));
    let _e61 = (*c);
    hi = pow(((_e61 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e66 = hi;
    let _e67 = lo;
    let _e68 = cutoff;
    return mix(_e66, _e67, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e68));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param: vec3<f32>;

    let _e55 = (*role);
    let _e57 = (*role);
    let _e62 = unnamed.packed_indices[(_e55 / 4u)][(_e57 % 4u)];
    let _e65 = (*role);
    let _e67 = (*role);
    let _e72 = unnamed.packed_indices[(_e65 / 4u)][(_e67 % 4u)];
    let _e77 = (*uv);
    let _e78 = textureSample(wired_bindless_images[(_e62 & 4095u)], wired_bindless_samplers[((_e72 >> bitcast<u32>(12i)) & 255u)], _e77);
    c_1 = _e78;
    let _e79 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e79))) == 0i) {
        let _e84 = c_1;
        param = _e84.xyz;
        let _e86 = sRGBToLinear_u0028_vf3_u003b((&param));
        c_1[0u] = _e86.x;
        c_1[1u] = _e86.y;
        c_1[2u] = _e86.z;
    }
    let _e93 = (*slot);
    if (lightmap_slot == (_e93 + 1i)) {
        let _e98 = unnamed.worldLightParams[0u];
        let _e99 = c_1;
        let _e101 = (_e99.xyz * _e98);
        c_1[0u] = _e101.x;
        c_1[1u] = _e101.y;
        c_1[2u] = _e101.z;
    }
    let _e108 = c_1;
    return _e108;
}

fn main_1() {
    var fog: vec4<f32>;
    var frag_color0_: vec4<f32>;
    var param_1: vec3<f32>;
    var color0_: vec4<f32>;
    var param_2: u32;
    var param_3: vec2<f32>;
    var param_4: i32;
    var base: vec4<f32>;

    let _e61 = unnamed.packed_indices[0i][3u];
    let _e67 = unnamed.packed_indices[0i][3u];
    let _e72 = fog_tex_coord_1;
    let _e73 = textureSample(wired_bindless_images[(_e61 & 4095u)], wired_bindless_samplers[((_e67 >> bitcast<u32>(12i)) & 255u)], _e72);
    fog = _e73;
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
    let _e87 = color0_;
    base = _e87;
    if override_type_10_ {
        let _e89 = color0_[3u];
        if (_e89 == alpha_test_value) {
            discard;
        }
    } else {
        if override_type_10_1 {
            let _e92 = color0_[3u];
            if (_e92 >= alpha_test_value) {
                discard;
            }
        } else {
            if override_type_10_2 {
                let _e95 = color0_[3u];
                if (_e95 < alpha_test_value) {
                    discard;
                }
            }
        }
    }
    let _e97 = color0_;
    base = _e97;
    if override_type_10_3 {
        let _e98 = base;
        let _e101 = fog[3u];
        let _e103 = (_e98.xyz * (1f - _e101));
        base[0u] = _e103.x;
        base[1u] = _e103.y;
        base[2u] = _e103.z;
    } else {
        if override_type_10_4 {
            let _e110 = base;
            let _e112 = fog[3u];
            base = (_e110 * (1f - _e112));
        } else {
            if override_type_10_5 {
                let _e116 = base[3u];
                let _e118 = fog[3u];
                base[3u] = (_e116 * (1f - _e118));
            } else {
                let _e122 = base;
                let _e123 = fog;
                let _e125 = unnamed.fogColor;
                let _e128 = fog[3u];
                base = mix(_e122, (_e123 * _e125), vec4(_e128));
            }
        }
    }
    if override_type_10_6 {
        let _e132 = base[3u];
        if (_e132 == 0f) {
            discard;
        }
    } else {
        if override_type_10_7 {
            let _e134 = base;
            let _e136 = base;
            if (dot(_e134.xyz, _e136.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e140 = base;
    out_color = _e140;
    return;
}

@fragment 
fn main(@location(4) fog_tex_coord: vec2<f32>, @location(0) frag_color0In: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>) -> @location(0) vec4<f32> {
    fog_tex_coord_1 = fog_tex_coord;
    frag_color0In_1 = frag_color0In;
    frag_tex_coord0_1 = frag_tex_coord0_;
    main_1();
    let _e7 = out_color;
    return _e7;
}
