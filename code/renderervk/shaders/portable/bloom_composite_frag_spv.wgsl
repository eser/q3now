@id(0) override intensity: f32 = 0.5f;

@group(0) @binding(0) 
var texture0_: texture_2d<f32>;
@group(0) @binding(32) 
var texture0_sampler: sampler;
var<private> frag_tex_coord_1: vec2<f32>;
var<private> out_color: vec4<f32>;

fn main_1() {
    var bloom: vec3<f32>;

    let _e7 = frag_tex_coord_1;
    let _e8 = textureSample(texture0_, texture0_sampler, _e7);
    bloom = _e8.xyz;
    let _e10 = bloom;
    let _e11 = (_e10 * intensity);
    out_color = vec4<f32>(_e11.x, _e11.y, _e11.z, 0f);
    return;
}

@fragment 
fn main(@location(0) frag_tex_coord: vec2<f32>) -> @location(0) vec4<f32> {
    frag_tex_coord_1 = frag_tex_coord;
    main_1();
    let _e3 = out_color;
    return _e3;
}
