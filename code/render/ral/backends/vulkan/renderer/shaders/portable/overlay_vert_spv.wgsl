struct gl_PerVertex {
    @builtin(position) gl_Position: vec4<f32>,
}

struct VertexOutput {
    @builtin(position) gl_Position: vec4<f32>,
    @location(0) member: vec4<f32>,
    @location(1) member_1: vec2<f32>,
}

var<private> unnamed: gl_PerVertex = gl_PerVertex(vec4<f32>(0f, 0f, 0f, 1f));
var<private> in_ndc_1: vec2<f32>;
var<private> frag_color: vec4<f32>;
var<private> in_color_1: vec4<f32>;
var<private> frag_tex_coord0_: vec2<f32>;
var<private> in_tex_coord0_1: vec2<f32>;

fn main_1() {
    let _e9 = in_ndc_1;
    unnamed.gl_Position = vec4<f32>(_e9.x, _e9.y, 0f, 1f);
    let _e14 = in_color_1;
    frag_color = _e14;
    let _e15 = in_tex_coord0_1;
    frag_tex_coord0_ = _e15;
    return;
}

@vertex
fn main(@location(0) in_ndc: vec2<f32>, @location(1) in_color: vec4<f32>, @location(2) in_tex_coord0_: vec2<f32>) -> VertexOutput {
    in_ndc_1 = in_ndc;
    in_color_1 = in_color;
    in_tex_coord0_1 = in_tex_coord0_;
    main_1();
    let _e11 = unnamed.gl_Position.y;
    unnamed.gl_Position.y = -(_e11);
    let _e13 = unnamed.gl_Position;
    let _e14 = frag_color;
    let _e15 = frag_tex_coord0_;
    return VertexOutput(_e13, _e14, _e15);
}
