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

    let _e55 = (*c);
    (*c) = max(_e55, vec3<f32>(0f, 0f, 0f));
    let _e57 = (*c);
    cutoff = (_e57 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e59 = (*c);
    lo = (_e59 / vec3(12.92f));
    let _e62 = (*c);
    hi = pow(((_e62 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e67 = hi;
    let _e68 = lo;
    let _e69 = cutoff;
    return mix(_e67, _e68, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e69));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param: vec3<f32>;

    let _e56 = (*role);
    let _e58 = (*role);
    let _e63 = unnamed.packed_indices[(_e56 / 4u)][(_e58 % 4u)];
    let _e66 = (*role);
    let _e68 = (*role);
    let _e73 = unnamed.packed_indices[(_e66 / 4u)][(_e68 % 4u)];
    let _e78 = (*uv);
    let _e79 = textureSample(wired_bindless_images[(_e63 & 4095u)], wired_bindless_samplers[((_e73 >> bitcast<u32>(12i)) & 255u)], _e78);
    c_1 = _e79;
    let _e80 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e80))) == 0i) {
        let _e85 = c_1;
        param = _e85.xyz;
        let _e87 = sRGBToLinear_u0028_vf3_u003b((&param));
        c_1[0u] = _e87.x;
        c_1[1u] = _e87.y;
        c_1[2u] = _e87.z;
    }
    let _e94 = (*slot);
    if (lightmap_slot == (_e94 + 1i)) {
        let _e99 = unnamed.worldLightParams[0u];
        let _e100 = c_1;
        let _e102 = (_e100.xyz * _e99);
        c_1[0u] = _e102.x;
        c_1[1u] = _e102.y;
        c_1[2u] = _e102.z;
    }
    let _e109 = c_1;
    return _e109;
}

fn main_1() {
    var fog: vec4<f32>;
    var frag_color: vec4<f32>;
    var color0_: vec4<f32>;
    var param_1: u32;
    var param_2: vec2<f32>;
    var param_3: i32;
    var base: vec4<f32>;

    let _e61 = unnamed.packed_indices[0i][3u];
    let _e67 = unnamed.packed_indices[0i][3u];
    let _e72 = fog_tex_coord_1;
    let _e73 = textureSample(wired_bindless_images[(_e61 & 4095u)], wired_bindless_samplers[((_e67 >> bitcast<u32>(12i)) & 255u)], _e72);
    fog = _e73;
    frag_color[0u] = identity_color;
    frag_color[1u] = identity_color;
    frag_color[2u] = identity_color;
    frag_color[3u] = identity_alpha;
    param_1 = 0u;
    let _e78 = frag_tex_coord0_1;
    param_2 = _e78;
    param_3 = 0i;
    let _e79 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_1), (&param_2), (&param_3));
    let _e80 = frag_color;
    color0_ = (_e79 * _e80);
    let _e82 = color0_;
    base = _e82;
    if override_type_10_ {
        let _e84 = color0_[3u];
        if (_e84 == alpha_test_value) {
            discard;
        }
    } else {
        if override_type_10_1 {
            let _e87 = color0_[3u];
            if (_e87 >= alpha_test_value) {
                discard;
            }
        } else {
            if override_type_10_2 {
                let _e90 = color0_[3u];
                if (_e90 < alpha_test_value) {
                    discard;
                }
            }
        }
    }
    let _e92 = color0_;
    base = _e92;
    if override_type_10_3 {
        let _e93 = base;
        let _e96 = fog[3u];
        let _e98 = (_e93.xyz * (1f - _e96));
        base[0u] = _e98.x;
        base[1u] = _e98.y;
        base[2u] = _e98.z;
    } else {
        if override_type_10_4 {
            let _e105 = base;
            let _e107 = fog[3u];
            base = (_e105 * (1f - _e107));
        } else {
            if override_type_10_5 {
                let _e111 = base[3u];
                let _e113 = fog[3u];
                base[3u] = (_e111 * (1f - _e113));
            } else {
                let _e117 = base;
                let _e118 = fog;
                let _e120 = unnamed.fogColor;
                let _e123 = fog[3u];
                base = mix(_e117, (_e118 * _e120), vec4(_e123));
            }
        }
    }
    if override_type_10_6 {
        let _e127 = base[3u];
        if (_e127 == 0f) {
            discard;
        }
    } else {
        if override_type_10_7 {
            let _e129 = base;
            let _e131 = base;
            if (dot(_e129.xyz, _e131.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e135 = base;
    out_color = _e135;
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
