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

struct FragmentOutput {
    @location(1) member: vec2<f32>,
    @location(2) member_1: f32,
    @location(0) member_2: vec4<f32>,
}

@id(4) override tex_domain: i32 = 0i;
@id(26) override lightmap_slot: i32 = 0i;
@id(6) override tex_mode: i32 = 0i;
override override_type_12_: bool = (tex_mode == 1i);
override override_type_12_1: bool = (tex_mode == 2i);
@id(10) override acff: i32 = 0i;
override override_type_12_2: bool = (acff == 1i);
override override_type_12_3: bool = (acff == 2i);
override override_type_12_4: bool = (acff == 3i);
@id(7) override discard_mode: i32 = 0i;
override override_type_12_5: bool = (discard_mode == 1i);
override override_type_12_6: bool = (discard_mode == 2i);
@id(3) override alpha_to_coverage: i32 = 0i;

var<private> out_temporal_velocity: vec2<f32>;
var<private> out_temporal_validity: f32;
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
var<private> temporalCurrentClip_1: vec4<f32>;
var<private> temporalPreviousClip_1: vec4<f32>;
var<private> temporalOutcome_1: u32;

fn wiredTemporalWriteAux_u0028_() {
    out_temporal_velocity = vec2<f32>(0f, 0f);
    out_temporal_validity = 0f;
    return;
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e58 = (*c);
    (*c) = max(_e58, vec3<f32>(0f, 0f, 0f));
    let _e60 = (*c);
    cutoff = (_e60 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e62 = (*c);
    lo = (_e62 / vec3(12.92f));
    let _e65 = (*c);
    hi = pow(((_e65 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e70 = hi;
    let _e71 = lo;
    let _e72 = cutoff;
    return mix(_e70, _e71, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e72));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param: vec3<f32>;

    let _e59 = (*role);
    let _e61 = (*role);
    let _e66 = unnamed.packed_indices[(_e59 / 4u)][(_e61 % 4u)];
    let _e69 = (*role);
    let _e71 = (*role);
    let _e76 = unnamed.packed_indices[(_e69 / 4u)][(_e71 % 4u)];
    let _e81 = (*uv);
    let _e82 = textureSample(wired_bindless_images[(_e66 & 4095u)], wired_bindless_samplers[((_e76 >> bitcast<u32>(12i)) & 255u)], _e81);
    c_1 = _e82;
    let _e83 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e83))) == 0i) {
        let _e88 = c_1;
        param = _e88.xyz;
        let _e90 = sRGBToLinear_u0028_vf3_u003b((&param));
        c_1[0u] = _e90.x;
        c_1[1u] = _e90.y;
        c_1[2u] = _e90.z;
    }
    let _e97 = (*slot);
    if (lightmap_slot == (_e97 + 1i)) {
        let _e102 = unnamed.worldLightParams[0u];
        let _e103 = c_1;
        let _e105 = (_e103.xyz * _e102);
        c_1[0u] = _e105.x;
        c_1[1u] = _e105.y;
        c_1[2u] = _e105.z;
    }
    let _e112 = c_1;
    return _e112;
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

    let _e75 = unnamed.packed_indices[0i][3u];
    let _e81 = unnamed.packed_indices[0i][3u];
    let _e86 = fog_tex_coord_1;
    let _e87 = textureSample(wired_bindless_images[(_e75 & 4095u)], wired_bindless_samplers[((_e81 >> bitcast<u32>(12i)) & 255u)], _e86);
    fog = _e87;
    param_1 = 0u;
    let _e88 = frag_tex_coord0_1;
    param_2 = _e88;
    param_3 = 0i;
    let _e89 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_1), (&param_2), (&param_3));
    color0_ = _e89;
    if override_type_12_ {
        param_4 = 1u;
        let _e90 = frag_tex_coord1_1;
        param_5 = _e90;
        param_6 = 1i;
        let _e91 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_4), (&param_5), (&param_6));
        color1_ = _e91;
        let _e92 = color0_;
        let _e94 = color1_;
        let _e96 = (_e92.xyz + _e94.xyz);
        let _e98 = color0_[3u];
        let _e100 = color1_[3u];
        base = vec4<f32>(_e96.x, _e96.y, _e96.z, (_e98 * _e100));
    } else {
        if override_type_12_1 {
            param_7 = 1u;
            let _e106 = frag_tex_coord1_1;
            param_8 = _e106;
            param_9 = 1i;
            let _e107 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_7), (&param_8), (&param_9));
            color1_1 = _e107;
            let _e108 = color0_;
            let _e110 = color1_1;
            let _e112 = (_e108.xyz + _e110.xyz);
            let _e114 = color0_[3u];
            let _e116 = color1_1[3u];
            base = vec4<f32>(_e112.x, _e112.y, _e112.z, (_e114 * _e116));
        } else {
            param_10 = 1u;
            let _e122 = frag_tex_coord1_1;
            param_11 = _e122;
            param_12 = 1i;
            let _e123 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_10), (&param_11), (&param_12));
            color1_2 = _e123;
            let _e124 = color0_;
            let _e126 = color1_2;
            let _e128 = (_e124.xyz * _e126.xyz);
            base[0u] = _e128.x;
            base[1u] = _e128.y;
            base[2u] = _e128.z;
            let _e136 = color0_[3u];
            let _e138 = color1_2[3u];
            base[3u] = (_e136 * _e138);
        }
    }
    if override_type_12_2 {
        let _e141 = base;
        let _e144 = fog[3u];
        let _e146 = (_e141.xyz * (1f - _e144));
        base[0u] = _e146.x;
        base[1u] = _e146.y;
        base[2u] = _e146.z;
    } else {
        if override_type_12_3 {
            let _e153 = base;
            let _e155 = fog[3u];
            base = (_e153 * (1f - _e155));
        } else {
            if override_type_12_4 {
                let _e159 = base[3u];
                let _e161 = fog[3u];
                base[3u] = (_e159 * (1f - _e161));
            } else {
                let _e165 = base;
                let _e166 = fog;
                let _e168 = unnamed.fogColor;
                let _e171 = fog[3u];
                base = mix(_e165, (_e166 * _e168), vec4(_e171));
            }
        }
    }
    if override_type_12_5 {
        let _e175 = base[3u];
        if (_e175 == 0f) {
            discard;
        }
    } else {
        if override_type_12_6 {
            let _e177 = base;
            let _e179 = base;
            if (dot(_e177.xyz, _e179.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e183 = base;
    out_color = _e183;
    wiredTemporalWriteAux_u0028_();
    return;
}

@fragment 
fn main(@location(4) fog_tex_coord: vec2<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>, @location(10) temporalCurrentClip: vec4<f32>, @location(11) temporalPreviousClip: vec4<f32>, @location(12) @interpolate(flat) temporalOutcome: u32) -> FragmentOutput {
    fog_tex_coord_1 = fog_tex_coord;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    temporalCurrentClip_1 = temporalCurrentClip;
    temporalPreviousClip_1 = temporalPreviousClip;
    temporalOutcome_1 = temporalOutcome;
    main_1();
    let _e15 = out_temporal_velocity;
    let _e16 = out_temporal_validity;
    let _e17 = out_color;
    return FragmentOutput(_e15, _e16, _e17);
}
