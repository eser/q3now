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
}

@group(1) @binding(0)
var wired_bindless_images: binding_array<texture_2d<f32>>;
@group(0) @binding(0)
var<uniform> unnamed: UBO;
@group(1) @binding(1)
var wired_bindless_samplers: binding_array<sampler>;
var<private> fog_tex_coord_1: vec2<f32>;
var<private> out_color: vec4<f32>;

fn main_1() {
    var fog: vec4<f32>;

    let _e17 = unnamed.packed_indices[0i][3u];
    let _e23 = unnamed.packed_indices[0i][3u];
    let _e28 = fog_tex_coord_1;
    let _e29 = textureSample(wired_bindless_images[(_e17 & 4095u)], wired_bindless_samplers[((_e23 >> bitcast<u32>(12i)) & 255u)], _e28);
    fog = _e29;
    let _e30 = fog;
    let _e32 = unnamed.fogColor;
    out_color = (_e30 * _e32);
    return;
}

@fragment
fn main(@location(4) fog_tex_coord: vec2<f32>) -> @location(0) vec4<f32> {
    fog_tex_coord_1 = fog_tex_coord;
    main_1();
    let _e3 = out_color;
    return _e3;
}
