var<private> out_color: vec4<f32>;
@group(0) @binding(0) 
var u_tex: texture_2d<f32>;
@group(0) @binding(32) 
var u_tex_sampler: sampler;
var<private> frag_tex_coord0_1: vec2<f32>;
var<private> frag_color_1: vec4<f32>;

fn main_1() {
    let _e5 = frag_tex_coord0_1;
    let _e6 = textureSample(u_tex, u_tex_sampler, _e5);
    let _e7 = frag_color_1;
    out_color = (_e6 * _e7);
    return;
}

@fragment 
fn main(@location(1) frag_tex_coord0_: vec2<f32>, @location(0) frag_color: vec4<f32>) -> @location(0) vec4<f32> {
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_color_1 = frag_color;
    main_1();
    let _e5 = out_color;
    return _e5;
}
