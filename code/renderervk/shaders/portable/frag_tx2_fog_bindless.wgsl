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
var<private> frag_tex_coord2_1: vec2<f32>;
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
    var color1_: vec4<f32>;
    var param_5: u32;
    var param_6: vec2<f32>;
    var param_7: i32;
    var color2_: vec4<f32>;
    var param_8: u32;
    var param_9: vec2<f32>;
    var param_10: i32;
    var base: vec4<f32>;
    var color1_1: vec4<f32>;
    var param_11: u32;
    var param_12: vec2<f32>;
    var param_13: i32;
    var color2_1: vec4<f32>;
    var param_14: u32;
    var param_15: vec2<f32>;
    var param_16: i32;
    var color1_2: vec4<f32>;
    var param_17: u32;
    var param_18: vec2<f32>;
    var param_19: i32;
    var color2_2: vec4<f32>;
    var param_20: u32;
    var param_21: vec2<f32>;
    var param_22: i32;

    let _e85 = unnamed.packed_indices[0i][3u];
    let _e91 = unnamed.packed_indices[0i][3u];
    let _e96 = fog_tex_coord_1;
    let _e97 = textureSample(wired_bindless_images[(_e85 & 4095u)], wired_bindless_samplers[((_e91 >> bitcast<u32>(12i)) & 255u)], _e96);
    fog = _e97;
    let _e98 = frag_color0In_1;
    param_1 = _e98.xyz;
    let _e100 = sRGBToLinear_u0028_vf3_u003b((&param_1));
    let _e102 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e100.x, _e100.y, _e100.z, _e102);
    param_2 = 0u;
    let _e107 = frag_tex_coord0_1;
    param_3 = _e107;
    param_4 = 0i;
    let _e108 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_2), (&param_3), (&param_4));
    let _e109 = frag_color0_;
    color0_ = (_e108 * _e109);
    if override_type_10_ {
        param_5 = 1u;
        let _e111 = frag_tex_coord1_1;
        param_6 = _e111;
        param_7 = 1i;
        let _e112 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_5), (&param_6), (&param_7));
        color1_ = _e112;
        param_8 = 2u;
        let _e113 = frag_tex_coord2_1;
        param_9 = _e113;
        param_10 = 2i;
        let _e114 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_8), (&param_9), (&param_10));
        color2_ = _e114;
        let _e115 = color0_;
        let _e117 = color1_;
        let _e120 = color2_;
        let _e122 = ((_e115.xyz + _e117.xyz) + _e120.xyz);
        let _e124 = color0_[3u];
        let _e126 = color1_[3u];
        let _e129 = color2_[3u];
        base = vec4<f32>(_e122.x, _e122.y, _e122.z, ((_e124 * _e126) * _e129));
    } else {
        if override_type_10_1 {
            param_11 = 1u;
            let _e135 = frag_tex_coord1_1;
            param_12 = _e135;
            param_13 = 1i;
            let _e136 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_11), (&param_12), (&param_13));
            let _e137 = frag_color0_;
            color1_1 = (_e136 * _e137);
            param_14 = 2u;
            let _e139 = frag_tex_coord2_1;
            param_15 = _e139;
            param_16 = 2i;
            let _e140 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_14), (&param_15), (&param_16));
            let _e141 = frag_color0_;
            color2_1 = (_e140 * _e141);
            let _e143 = color0_;
            let _e145 = color1_1;
            let _e148 = color2_1;
            let _e150 = ((_e143.xyz + _e145.xyz) + _e148.xyz);
            let _e152 = color0_[3u];
            let _e154 = color1_1[3u];
            let _e157 = color2_1[3u];
            base = vec4<f32>(_e150.x, _e150.y, _e150.z, ((_e152 * _e154) * _e157));
        } else {
            param_17 = 1u;
            let _e163 = frag_tex_coord1_1;
            param_18 = _e163;
            param_19 = 1i;
            let _e164 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_17), (&param_18), (&param_19));
            color1_2 = _e164;
            param_20 = 2u;
            let _e165 = frag_tex_coord2_1;
            param_21 = _e165;
            param_22 = 2i;
            let _e166 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_20), (&param_21), (&param_22));
            color2_2 = _e166;
            let _e167 = color0_;
            let _e169 = color1_2;
            let _e172 = color2_2;
            let _e174 = ((_e167.xyz * _e169.xyz) * _e172.xyz);
            base[0u] = _e174.x;
            base[1u] = _e174.y;
            base[2u] = _e174.z;
            let _e182 = color0_[3u];
            let _e184 = color1_2[3u];
            let _e187 = color2_2[3u];
            base[3u] = ((_e182 * _e184) * _e187);
        }
    }
    if override_type_10_2 {
        let _e190 = base;
        let _e193 = fog[3u];
        let _e195 = (_e190.xyz * (1f - _e193));
        base[0u] = _e195.x;
        base[1u] = _e195.y;
        base[2u] = _e195.z;
    } else {
        if override_type_10_3 {
            let _e202 = base;
            let _e204 = fog[3u];
            base = (_e202 * (1f - _e204));
        } else {
            if override_type_10_4 {
                let _e208 = base[3u];
                let _e210 = fog[3u];
                base[3u] = (_e208 * (1f - _e210));
            } else {
                let _e214 = base;
                let _e215 = fog;
                let _e217 = unnamed.fogColor;
                let _e220 = fog[3u];
                base = mix(_e214, (_e215 * _e217), vec4(_e220));
            }
        }
    }
    if override_type_10_5 {
        let _e224 = base[3u];
        if (_e224 == 0f) {
            discard;
        }
    } else {
        if override_type_10_6 {
            let _e226 = base;
            let _e228 = base;
            if (dot(_e226.xyz, _e228.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e232 = base;
    out_color = _e232;
    return;
}

@fragment 
fn main(@location(4) fog_tex_coord: vec2<f32>, @location(0) frag_color0In: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>, @location(3) frag_tex_coord2_: vec2<f32>) -> @location(0) vec4<f32> {
    fog_tex_coord_1 = fog_tex_coord;
    frag_color0In_1 = frag_color0In;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    frag_tex_coord2_1 = frag_tex_coord2_;
    main_1();
    let _e11 = out_color;
    return _e11;
}
