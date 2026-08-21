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
var<private> frag_normal_1: vec3<f32>;
var<private> frag_tangent_1: vec4<f32>;
var<private> temporalCurrentClip_1: vec4<f32>;
var<private> temporalPreviousClip_1: vec4<f32>;

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e31 = (*c);
    (*c) = max(_e31, vec3<f32>(0f, 0f, 0f));
    let _e33 = (*c);
    cutoff = (_e33 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e35 = (*c);
    lo = (_e35 / vec3(12.92f));
    let _e38 = (*c);
    hi = pow(((_e38 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e43 = hi;
    let _e44 = lo;
    let _e45 = cutoff;
    return mix(_e43, _e44, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e45));
}

fn main_1() {
    var sampled: vec4<f32>;
    var param: vec3<f32>;

    let _e30 = surfacePush.imageSlot;
    let _e33 = surfacePush.samplerSlot;
    let _e35 = frag_tex_coord_1;
    let _e36 = textureSample(wired_bindless_images[_e30], wired_bindless_samplers[_e33], _e35);
    sampled = _e36;
    let _e37 = sampled;
    param = _e37.xyz;
    let _e39 = sRGBToLinear_u0028_vf3_u003b((&param));
    let _e41 = sampled[3u];
    out_color = vec4<f32>(_e39.x, _e39.y, _e39.z, _e41);
    out_temporal_velocity = vec2<f32>(0f, 0f);
    out_temporal_validity = 0f;
    return;
}

@fragment 
fn main(@location(0) frag_tex_coord: vec2<f32>, @location(1) frag_normal: vec3<f32>, @location(2) frag_tangent: vec4<f32>, @location(10) temporalCurrentClip: vec4<f32>, @location(11) temporalPreviousClip: vec4<f32>) -> FragmentOutput {
    frag_tex_coord_1 = frag_tex_coord;
    frag_normal_1 = frag_normal;
    frag_tangent_1 = frag_tangent;
    temporalCurrentClip_1 = temporalCurrentClip;
    temporalPreviousClip_1 = temporalPreviousClip;
    main_1();
    let _e13 = out_color;
    let _e14 = out_temporal_velocity;
    let _e15 = out_temporal_validity;
    return FragmentOutput(_e13, _e14, _e15);
}
