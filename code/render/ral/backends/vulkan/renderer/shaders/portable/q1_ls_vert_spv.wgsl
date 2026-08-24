struct gl_PerVertex {
    @builtin(position) gl_Position: vec4<f32>,
    gl_PointSize: f32,
    gl_ClipDistance: array<f32, 1>,
    gl_CullDistance: array<f32, 1>,
}

struct UBO {
    _pad_to_mvp: array<vec4<f32>, 30>,
    mvp: mat4x4<f32>,
}

struct VertexOutput {
    @builtin(position) gl_Position: vec4<f32>,
    @location(1) member: vec2<f32>,
    @location(2) member_1: vec2<f32>,
}

var<private> unnamed: gl_PerVertex = gl_PerVertex(vec4<f32>(0f, 0f, 0f, 1f), 1f, array<f32, 1>(), array<f32, 1>());
@group(0) @binding(0)
var<uniform> ubo: UBO;
var<private> in_position_1: vec3<f32>;
var<private> frag_tex_coord0_: vec2<f32>;
var<private> in_tex_coord0_1: vec2<f32>;
var<private> frag_tex_coord1_: vec2<f32>;
var<private> in_tex_coord1_1: vec2<f32>;

fn main_1() {
    let _e13 = ubo.mvp;
    let _e14 = in_position_1;
    unnamed.gl_Position = (_e13 * vec4<f32>(_e14.x, _e14.y, _e14.z, 1f));
    let _e21 = in_tex_coord0_1;
    frag_tex_coord0_ = _e21;
    let _e22 = in_tex_coord1_1;
    frag_tex_coord1_ = _e22;
    return;
}

@vertex
fn main(@location(0) in_position: vec3<f32>, @location(2) in_tex_coord0_: vec2<f32>, @location(3) in_tex_coord1_: vec2<f32>) -> VertexOutput {
    in_position_1 = in_position;
    in_tex_coord0_1 = in_tex_coord0_;
    in_tex_coord1_1 = in_tex_coord1_;
    main_1();
    let _e11 = unnamed.gl_Position.y;
    unnamed.gl_Position.y = -(_e11);
    let _e13 = unnamed.gl_Position;
    let _e14 = frag_tex_coord0_;
    let _e15 = frag_tex_coord1_;
    return VertexOutput(_e13, _e14, _e15);
}
