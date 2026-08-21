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
@id(10) override acff: i32 = 0i;
override override_type_10_2: bool = (acff == 1i);
override override_type_10_3: bool = (acff == 2i);
override override_type_10_4: bool = (acff == 3i);
@id(7) override discard_mode: i32 = 0i;
override override_type_10_5: bool = (discard_mode == 1i);
override override_type_10_6: bool = (discard_mode == 2i);
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
var<private> frag_tex_coord1_1: vec2<f32>;
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

    let _e72 = unnamed.packed_indices[0i][3u];
    let _e78 = unnamed.packed_indices[0i][3u];
    let _e83 = fog_tex_coord_1;
    let _e84 = textureSample(wired_bindless_images[(_e72 & 4095u)], wired_bindless_samplers[((_e78 >> bitcast<u32>(12i)) & 255u)], _e83);
    fog = _e84;
    let _e85 = frag_color0In_1;
    param_1 = _e85.xyz;
    let _e87 = sRGBToLinear_u0028_vf3_u003b((&param_1));
    let _e89 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e87.x, _e87.y, _e87.z, _e89);
    param_2 = 0u;
    let _e94 = frag_tex_coord0_1;
    param_3 = _e94;
    param_4 = 0i;
    let _e95 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_2), (&param_3), (&param_4));
    let _e96 = frag_color0_;
    color0_ = (_e95 * _e96);
    if override_type_10_ {
        param_5 = 1u;
        let _e98 = frag_tex_coord1_1;
        param_6 = _e98;
        param_7 = 1i;
        let _e99 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_5), (&param_6), (&param_7));
        color1_ = _e99;
        let _e100 = color0_;
        let _e102 = color1_;
        let _e104 = (_e100.xyz + _e102.xyz);
        let _e106 = color0_[3u];
        let _e108 = color1_[3u];
        base = vec4<f32>(_e104.x, _e104.y, _e104.z, (_e106 * _e108));
    } else {
        if override_type_10_1 {
            param_8 = 1u;
            let _e114 = frag_tex_coord1_1;
            param_9 = _e114;
            param_10 = 1i;
            let _e115 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_8), (&param_9), (&param_10));
            let _e116 = frag_color0_;
            color1_1 = (_e115 * _e116);
            let _e118 = color0_;
            let _e120 = color1_1;
            let _e122 = (_e118.xyz + _e120.xyz);
            let _e124 = color0_[3u];
            let _e126 = color1_1[3u];
            base = vec4<f32>(_e122.x, _e122.y, _e122.z, (_e124 * _e126));
        } else {
            param_11 = 1u;
            let _e132 = frag_tex_coord1_1;
            param_12 = _e132;
            param_13 = 1i;
            let _e133 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_11), (&param_12), (&param_13));
            color1_2 = _e133;
            let _e134 = color0_;
            let _e136 = color1_2;
            let _e138 = (_e134.xyz * _e136.xyz);
            base[0u] = _e138.x;
            base[1u] = _e138.y;
            base[2u] = _e138.z;
            let _e146 = color0_[3u];
            let _e148 = color1_2[3u];
            base[3u] = (_e146 * _e148);
        }
    }
    if override_type_10_2 {
        let _e151 = base;
        let _e154 = fog[3u];
        let _e156 = (_e151.xyz * (1f - _e154));
        base[0u] = _e156.x;
        base[1u] = _e156.y;
        base[2u] = _e156.z;
    } else {
        if override_type_10_3 {
            let _e163 = base;
            let _e165 = fog[3u];
            base = (_e163 * (1f - _e165));
        } else {
            if override_type_10_4 {
                let _e169 = base[3u];
                let _e171 = fog[3u];
                base[3u] = (_e169 * (1f - _e171));
            } else {
                let _e175 = base;
                let _e176 = fog;
                let _e178 = unnamed.fogColor;
                let _e181 = fog[3u];
                base = mix(_e175, (_e176 * _e178), vec4(_e181));
            }
        }
    }
    if override_type_10_5 {
        let _e185 = base[3u];
        if (_e185 == 0f) {
            discard;
        }
    } else {
        if override_type_10_6 {
            let _e187 = base;
            let _e189 = base;
            if (dot(_e187.xyz, _e189.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e193 = base;
    out_color = _e193;
    return;
}

@fragment 
fn main(@location(4) fog_tex_coord: vec2<f32>, @location(0) frag_color0In: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>) -> @location(0) vec4<f32> {
    fog_tex_coord_1 = fog_tex_coord;
    frag_color0In_1 = frag_color0In;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    main_1();
    let _e9 = out_color;
    return _e9;
}
