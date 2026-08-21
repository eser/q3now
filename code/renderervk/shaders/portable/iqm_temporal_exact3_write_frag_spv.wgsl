enable wgpu_binding_array;

struct TemporalIqmSurfacePush {
    imageSlot: u32,
    samplerSlot: u32,
}

struct FragmentOutput {
    @location(0) member: vec4<f32>,
    @location(1) member_1: vec2<f32>,
    @location(2) member_2: f32,
}

@group(1) @binding(0) 
var wired_bindless_images: binding_array<texture_2d<f32>>;
@group(0) @binding(0)
var<uniform> surfacePush: TemporalIqmSurfacePush;
@group(1) @binding(1) 
var wired_bindless_samplers: binding_array<sampler>;
var<private> frag_tex_coord_1: vec2<f32>;
var<private> out_color: vec4<f32>;
var<private> out_temporal_velocity: vec2<f32>;
var<private> out_temporal_validity: f32;
var<private> temporalCurrentClip_1: vec4<f32>;
var<private> temporalPreviousClip_1: vec4<f32>;
var<private> frag_normal_1: vec3<f32>;
var<private> frag_tangent_1: vec4<f32>;

fn wiredTemporalFinite2_u0028_vf2_u003b(value: ptr<function, vec2<f32>>) -> bool {
    var phi_90_: bool;

    let _e34 = (*value);
    let _e35 = (*value);
    let _e37 = all((_e34 == _e35));
    phi_90_ = _e37;
    if _e37 {
        let _e38 = (*value);
        phi_90_ = all((abs(_e38) <= vec2<f32>(340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f)));
    }
    let _e43 = phi_90_;
    return _e43;
}

fn wiredTemporalFinite4_u0028_vf4_u003b(value_1: ptr<function, vec4<f32>>) -> bool {
    var phi_75_: bool;

    let _e34 = (*value_1);
    let _e35 = (*value_1);
    let _e37 = all((_e34 == _e35));
    phi_75_ = _e37;
    if _e37 {
        let _e38 = (*value_1);
        phi_75_ = all((abs(_e38) <= vec4<f32>(340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f)));
    }
    let _e43 = phi_75_;
    return _e43;
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e37 = (*c);
    (*c) = max(_e37, vec3<f32>(0f, 0f, 0f));
    let _e39 = (*c);
    cutoff = (_e39 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e41 = (*c);
    lo = (_e41 / vec3(12.92f));
    let _e44 = (*c);
    hi = pow(((_e44 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e49 = hi;
    let _e50 = lo;
    let _e51 = cutoff;
    return mix(_e49, _e50, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e51));
}

fn main_1() {
    var sampled: vec4<f32>;
    var param: vec3<f32>;
    var param_1: vec4<f32>;
    var param_2: vec4<f32>;
    var currentNdc: vec2<f32>;
    var previousNdc: vec2<f32>;
    var param_3: vec2<f32>;
    var param_4: vec2<f32>;
    var currentUv: vec2<f32>;
    var previousUv: vec2<f32>;
    var velocity: vec2<f32>;
    var param_5: vec2<f32>;
    var phi_161_: bool;
    var phi_170_: bool;
    var phi_177_: bool;
    var phi_206_: bool;

    let _e46 = surfacePush.imageSlot;
    let _e49 = surfacePush.samplerSlot;
    let _e51 = frag_tex_coord_1;
    let _e52 = textureSample(wired_bindless_images[_e46], wired_bindless_samplers[_e49], _e51);
    sampled = _e52;
    let _e53 = sampled;
    param = _e53.xyz;
    let _e55 = sRGBToLinear_u0028_vf3_u003b((&param));
    let _e57 = sampled[3u];
    out_color = vec4<f32>(_e55.x, _e55.y, _e55.z, _e57);
    out_temporal_velocity = vec2<f32>(0f, 0f);
    out_temporal_validity = 0f;
    let _e62 = temporalCurrentClip_1;
    param_1 = _e62;
    let _e63 = wiredTemporalFinite4_u0028_vf4_u003b((&param_1));
    let _e64 = !(_e63);
    phi_161_ = _e64;
    if !(_e64) {
        let _e66 = temporalPreviousClip_1;
        param_2 = _e66;
        let _e67 = wiredTemporalFinite4_u0028_vf4_u003b((&param_2));
        phi_161_ = !(_e67);
    }
    let _e70 = phi_161_;
    phi_170_ = _e70;
    if !(_e70) {
        let _e73 = temporalCurrentClip_1[3u];
        phi_170_ = (_e73 <= 0.000001f);
    }
    let _e76 = phi_170_;
    phi_177_ = _e76;
    if !(_e76) {
        let _e79 = temporalPreviousClip_1[3u];
        phi_177_ = (_e79 <= 0.000001f);
    }
    let _e82 = phi_177_;
    if _e82 {
        return;
    }
    let _e83 = temporalCurrentClip_1;
    let _e86 = temporalCurrentClip_1[3u];
    currentNdc = (_e83.xy / vec2(_e86));
    let _e89 = temporalPreviousClip_1;
    let _e92 = temporalPreviousClip_1[3u];
    previousNdc = (_e89.xy / vec2(_e92));
    let _e95 = currentNdc;
    param_3 = _e95;
    let _e96 = wiredTemporalFinite2_u0028_vf2_u003b((&param_3));
    let _e97 = !(_e96);
    phi_206_ = _e97;
    if !(_e97) {
        let _e99 = previousNdc;
        param_4 = _e99;
        let _e100 = wiredTemporalFinite2_u0028_vf2_u003b((&param_4));
        phi_206_ = !(_e100);
    }
    let _e103 = phi_206_;
    if _e103 {
        return;
    }
    let _e104 = currentNdc;
    currentUv = ((_e104 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e107 = previousNdc;
    previousUv = ((_e107 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e110 = currentUv;
    let _e111 = previousUv;
    velocity = (_e110 - _e111);
    let _e113 = velocity;
    param_5 = _e113;
    let _e114 = wiredTemporalFinite2_u0028_vf2_u003b((&param_5));
    if !(_e114) {
        return;
    }
    let _e116 = velocity;
    out_temporal_velocity = _e116;
    out_temporal_validity = 1f;
    return;
}

@fragment 
fn main(@location(0) frag_tex_coord: vec2<f32>, @location(10) temporalCurrentClip: vec4<f32>, @location(11) temporalPreviousClip: vec4<f32>, @location(1) frag_normal: vec3<f32>, @location(2) frag_tangent: vec4<f32>) -> FragmentOutput {
    frag_tex_coord_1 = frag_tex_coord;
    temporalCurrentClip_1 = temporalCurrentClip;
    temporalPreviousClip_1 = temporalPreviousClip;
    frag_normal_1 = frag_normal;
    frag_tangent_1 = frag_tangent;
    main_1();
    let _e13 = out_color;
    let _e14 = out_temporal_velocity;
    let _e15 = out_temporal_validity;
    return FragmentOutput(_e13, _e14, _e15);
}
