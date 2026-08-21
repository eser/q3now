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
var<private> frag_tex_coord0_1: vec2<f32>;
var<private> frag_tex_coord1_1: vec2<f32>;
var<private> out_color: vec4<f32>;

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e52 = (*c);
    (*c) = max(_e52, vec3<f32>(0f, 0f, 0f));
    let _e54 = (*c);
    cutoff = (_e54 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e56 = (*c);
    lo = (_e56 / vec3(12.92f));
    let _e59 = (*c);
    hi = pow(((_e59 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e64 = hi;
    let _e65 = lo;
    let _e66 = cutoff;
    return mix(_e64, _e65, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e66));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param: vec3<f32>;

    let _e53 = (*role);
    let _e55 = (*role);
    let _e60 = unnamed.packed_indices[(_e53 / 4u)][(_e55 % 4u)];
    let _e63 = (*role);
    let _e65 = (*role);
    let _e70 = unnamed.packed_indices[(_e63 / 4u)][(_e65 % 4u)];
    let _e75 = (*uv);
    let _e76 = textureSample(wired_bindless_images[(_e60 & 4095u)], wired_bindless_samplers[((_e70 >> bitcast<u32>(12i)) & 255u)], _e75);
    c_1 = _e76;
    let _e77 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e77))) == 0i) {
        let _e82 = c_1;
        param = _e82.xyz;
        let _e84 = sRGBToLinear_u0028_vf3_u003b((&param));
        c_1[0u] = _e84.x;
        c_1[1u] = _e84.y;
        c_1[2u] = _e84.z;
    }
    let _e91 = (*slot);
    if (lightmap_slot == (_e91 + 1i)) {
        let _e96 = unnamed.worldLightParams[0u];
        let _e97 = c_1;
        let _e99 = (_e97.xyz * _e96);
        c_1[0u] = _e99.x;
        c_1[1u] = _e99.y;
        c_1[2u] = _e99.z;
    }
    let _e106 = c_1;
    return _e106;
}

fn main_1() {
    var fog: vec4<f32>;
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

    let _e69 = unnamed.packed_indices[0i][3u];
    let _e75 = unnamed.packed_indices[0i][3u];
    let _e80 = fog_tex_coord_1;
    let _e81 = textureSample(wired_bindless_images[(_e69 & 4095u)], wired_bindless_samplers[((_e75 >> bitcast<u32>(12i)) & 255u)], _e80);
    fog = _e81;
    param_1 = 0u;
    let _e82 = frag_tex_coord0_1;
    param_2 = _e82;
    param_3 = 0i;
    let _e83 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_1), (&param_2), (&param_3));
    color0_ = _e83;
    if override_type_10_ {
        param_4 = 1u;
        let _e84 = frag_tex_coord1_1;
        param_5 = _e84;
        param_6 = 1i;
        let _e85 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_4), (&param_5), (&param_6));
        color1_ = _e85;
        let _e86 = color0_;
        let _e88 = color1_;
        let _e90 = (_e86.xyz + _e88.xyz);
        let _e92 = color0_[3u];
        let _e94 = color1_[3u];
        base = vec4<f32>(_e90.x, _e90.y, _e90.z, (_e92 * _e94));
    } else {
        if override_type_10_1 {
            param_7 = 1u;
            let _e100 = frag_tex_coord1_1;
            param_8 = _e100;
            param_9 = 1i;
            let _e101 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_7), (&param_8), (&param_9));
            color1_1 = _e101;
            let _e102 = color0_;
            let _e104 = color1_1;
            let _e106 = (_e102.xyz + _e104.xyz);
            let _e108 = color0_[3u];
            let _e110 = color1_1[3u];
            base = vec4<f32>(_e106.x, _e106.y, _e106.z, (_e108 * _e110));
        } else {
            param_10 = 1u;
            let _e116 = frag_tex_coord1_1;
            param_11 = _e116;
            param_12 = 1i;
            let _e117 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_10), (&param_11), (&param_12));
            color1_2 = _e117;
            let _e118 = color0_;
            let _e120 = color1_2;
            let _e122 = (_e118.xyz * _e120.xyz);
            base[0u] = _e122.x;
            base[1u] = _e122.y;
            base[2u] = _e122.z;
            let _e130 = color0_[3u];
            let _e132 = color1_2[3u];
            base[3u] = (_e130 * _e132);
        }
    }
    if override_type_10_2 {
        let _e135 = base;
        let _e138 = fog[3u];
        let _e140 = (_e135.xyz * (1f - _e138));
        base[0u] = _e140.x;
        base[1u] = _e140.y;
        base[2u] = _e140.z;
    } else {
        if override_type_10_3 {
            let _e147 = base;
            let _e149 = fog[3u];
            base = (_e147 * (1f - _e149));
        } else {
            if override_type_10_4 {
                let _e153 = base[3u];
                let _e155 = fog[3u];
                base[3u] = (_e153 * (1f - _e155));
            } else {
                let _e159 = base;
                let _e160 = fog;
                let _e162 = unnamed.fogColor;
                let _e165 = fog[3u];
                base = mix(_e159, (_e160 * _e162), vec4(_e165));
            }
        }
    }
    if override_type_10_5 {
        let _e169 = base[3u];
        if (_e169 == 0f) {
            discard;
        }
    } else {
        if override_type_10_6 {
            let _e171 = base;
            let _e173 = base;
            if (dot(_e171.xyz, _e173.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e177 = base;
    out_color = _e177;
    return;
}

@fragment 
fn main(@location(4) fog_tex_coord: vec2<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>) -> @location(0) vec4<f32> {
    fog_tex_coord_1 = fog_tex_coord;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    main_1();
    let _e7 = out_color;
    return _e7;
}
