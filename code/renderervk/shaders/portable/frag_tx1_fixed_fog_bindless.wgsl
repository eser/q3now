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
    var frag_color: vec4<f32>;
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

    let _e72 = unnamed.packed_indices[0i][3u];
    let _e78 = unnamed.packed_indices[0i][3u];
    let _e83 = fog_tex_coord_1;
    let _e84 = textureSample(wired_bindless_images[(_e72 & 4095u)], wired_bindless_samplers[((_e78 >> bitcast<u32>(12i)) & 255u)], _e83);
    fog = _e84;
    frag_color[0u] = identity_color;
    frag_color[1u] = identity_color;
    frag_color[2u] = identity_color;
    frag_color[3u] = identity_alpha;
    param_1 = 0u;
    let _e89 = frag_tex_coord0_1;
    param_2 = _e89;
    param_3 = 0i;
    let _e90 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_1), (&param_2), (&param_3));
    let _e91 = frag_color;
    color0_ = (_e90 * _e91);
    if override_type_10_ {
        param_4 = 1u;
        let _e93 = frag_tex_coord1_1;
        param_5 = _e93;
        param_6 = 1i;
        let _e94 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_4), (&param_5), (&param_6));
        color1_ = _e94;
        let _e95 = color0_;
        let _e97 = color1_;
        let _e99 = (_e95.xyz + _e97.xyz);
        let _e101 = color0_[3u];
        let _e103 = color1_[3u];
        base = vec4<f32>(_e99.x, _e99.y, _e99.z, (_e101 * _e103));
    } else {
        if override_type_10_1 {
            param_7 = 1u;
            let _e109 = frag_tex_coord1_1;
            param_8 = _e109;
            param_9 = 1i;
            let _e110 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_7), (&param_8), (&param_9));
            let _e111 = frag_color;
            color1_1 = (_e110 * _e111);
            let _e113 = color0_;
            let _e115 = color1_1;
            let _e117 = (_e113.xyz + _e115.xyz);
            let _e119 = color0_[3u];
            let _e121 = color1_1[3u];
            base = vec4<f32>(_e117.x, _e117.y, _e117.z, (_e119 * _e121));
        } else {
            param_10 = 1u;
            let _e127 = frag_tex_coord1_1;
            param_11 = _e127;
            param_12 = 1i;
            let _e128 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_10), (&param_11), (&param_12));
            let _e129 = frag_color;
            color1_2 = (_e128 * _e129);
            let _e131 = color0_;
            let _e133 = color1_2;
            let _e135 = (_e131.xyz * _e133.xyz);
            base[0u] = _e135.x;
            base[1u] = _e135.y;
            base[2u] = _e135.z;
            let _e143 = color0_[3u];
            let _e145 = color1_2[3u];
            base[3u] = (_e143 * _e145);
        }
    }
    if override_type_10_2 {
        let _e148 = base;
        let _e151 = fog[3u];
        let _e153 = (_e148.xyz * (1f - _e151));
        base[0u] = _e153.x;
        base[1u] = _e153.y;
        base[2u] = _e153.z;
    } else {
        if override_type_10_3 {
            let _e160 = base;
            let _e162 = fog[3u];
            base = (_e160 * (1f - _e162));
        } else {
            if override_type_10_4 {
                let _e166 = base[3u];
                let _e168 = fog[3u];
                base[3u] = (_e166 * (1f - _e168));
            } else {
                let _e172 = base;
                let _e173 = fog;
                let _e175 = unnamed.fogColor;
                let _e178 = fog[3u];
                base = mix(_e172, (_e173 * _e175), vec4(_e178));
            }
        }
    }
    if override_type_10_5 {
        let _e182 = base[3u];
        if (_e182 == 0f) {
            discard;
        }
    } else {
        if override_type_10_6 {
            let _e184 = base;
            let _e186 = base;
            if (dot(_e184.xyz, _e186.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e190 = base;
    out_color = _e190;
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
