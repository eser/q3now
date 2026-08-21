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
@id(7) override discard_mode: i32 = 0i;
override override_type_12_2: bool = (discard_mode == 1i);
override override_type_12_3: bool = (discard_mode == 2i);
@id(3) override alpha_to_coverage: i32 = 0i;

var<private> out_temporal_velocity: vec2<f32>;
var<private> out_temporal_validity: f32;
@group(1) @binding(0) 
var wired_bindless_images: binding_array<texture_2d<f32>>;
@group(0) @binding(0) 
var<uniform> unnamed: UBO;
@group(1) @binding(1) 
var wired_bindless_samplers: binding_array<sampler>;
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

    let _e51 = (*c);
    (*c) = max(_e51, vec3<f32>(0f, 0f, 0f));
    let _e53 = (*c);
    cutoff = (_e53 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e55 = (*c);
    lo = (_e55 / vec3(12.92f));
    let _e58 = (*c);
    hi = pow(((_e58 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e63 = hi;
    let _e64 = lo;
    let _e65 = cutoff;
    return mix(_e63, _e64, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e65));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param: vec3<f32>;

    let _e52 = (*role);
    let _e54 = (*role);
    let _e59 = unnamed.packed_indices[(_e52 / 4u)][(_e54 % 4u)];
    let _e62 = (*role);
    let _e64 = (*role);
    let _e69 = unnamed.packed_indices[(_e62 / 4u)][(_e64 % 4u)];
    let _e74 = (*uv);
    let _e75 = textureSample(wired_bindless_images[(_e59 & 4095u)], wired_bindless_samplers[((_e69 >> bitcast<u32>(12i)) & 255u)], _e74);
    c_1 = _e75;
    let _e76 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e76))) == 0i) {
        let _e81 = c_1;
        param = _e81.xyz;
        let _e83 = sRGBToLinear_u0028_vf3_u003b((&param));
        c_1[0u] = _e83.x;
        c_1[1u] = _e83.y;
        c_1[2u] = _e83.z;
    }
    let _e90 = (*slot);
    if (lightmap_slot == (_e90 + 1i)) {
        let _e95 = unnamed.worldLightParams[0u];
        let _e96 = c_1;
        let _e98 = (_e96.xyz * _e95);
        c_1[0u] = _e98.x;
        c_1[1u] = _e98.y;
        c_1[2u] = _e98.z;
    }
    let _e105 = c_1;
    return _e105;
}

fn main_1() {
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

    param_1 = 0u;
    let _e64 = frag_tex_coord0_1;
    param_2 = _e64;
    param_3 = 0i;
    let _e65 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_1), (&param_2), (&param_3));
    color0_ = _e65;
    if override_type_12_ {
        param_4 = 1u;
        let _e66 = frag_tex_coord1_1;
        param_5 = _e66;
        param_6 = 1i;
        let _e67 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_4), (&param_5), (&param_6));
        color1_ = _e67;
        let _e68 = color0_;
        let _e70 = color1_;
        let _e72 = (_e68.xyz + _e70.xyz);
        let _e74 = color0_[3u];
        let _e76 = color1_[3u];
        base = vec4<f32>(_e72.x, _e72.y, _e72.z, (_e74 * _e76));
    } else {
        if override_type_12_1 {
            param_7 = 1u;
            let _e82 = frag_tex_coord1_1;
            param_8 = _e82;
            param_9 = 1i;
            let _e83 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_7), (&param_8), (&param_9));
            color1_1 = _e83;
            let _e84 = color0_;
            let _e86 = color1_1;
            let _e88 = (_e84.xyz + _e86.xyz);
            let _e90 = color0_[3u];
            let _e92 = color1_1[3u];
            base = vec4<f32>(_e88.x, _e88.y, _e88.z, (_e90 * _e92));
        } else {
            param_10 = 1u;
            let _e98 = frag_tex_coord1_1;
            param_11 = _e98;
            param_12 = 1i;
            let _e99 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_10), (&param_11), (&param_12));
            color1_2 = _e99;
            let _e100 = color0_;
            let _e102 = color1_2;
            let _e104 = (_e100.xyz * _e102.xyz);
            base[0u] = _e104.x;
            base[1u] = _e104.y;
            base[2u] = _e104.z;
            let _e112 = color0_[3u];
            let _e114 = color1_2[3u];
            base[3u] = (_e112 * _e114);
        }
    }
    if override_type_12_2 {
        let _e118 = base[3u];
        if (_e118 == 0f) {
            discard;
        }
    } else {
        if override_type_12_3 {
            let _e120 = base;
            let _e122 = base;
            if (dot(_e120.xyz, _e122.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e126 = base;
    out_color = _e126;
    wiredTemporalWriteAux_u0028_();
    return;
}

@fragment 
fn main(@location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>, @location(10) temporalCurrentClip: vec4<f32>, @location(11) temporalPreviousClip: vec4<f32>, @location(12) @interpolate(flat) temporalOutcome: u32) -> FragmentOutput {
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    temporalCurrentClip_1 = temporalCurrentClip;
    temporalPreviousClip_1 = temporalPreviousClip;
    temporalOutcome_1 = temporalOutcome;
    main_1();
    let _e13 = out_temporal_velocity;
    let _e14 = out_temporal_validity;
    let _e15 = out_color;
    return FragmentOutput(_e13, _e14, _e15);
}
