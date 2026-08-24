struct gl_PerVertex {
    @builtin(position) gl_Position: vec4<f32>,
    gl_PointSize: f32,
    gl_ClipDistance: array<f32, 1>,
    gl_CullDistance: array<f32, 1>,
}

struct MsdfUBO {
    mvp: mat4x4<f32>,
    outlineWidth: f32,
    glowWidth: f32,
    shadowOffset: vec2<f32>,
    outlineColor: vec4<f32>,
    glowColor: vec4<f32>,
    shadowColor: vec4<f32>,
    bindless_packed_slot: u32,
}

struct VertexOutput {
    @builtin(position) gl_Position: vec4<f32>,
    @location(0) member: vec4<f32>,
    @location(1) member_1: vec2<f32>,
}

var<private> unnamed: gl_PerVertex = gl_PerVertex(vec4<f32>(0f, 0f, 0f, 1f), 1f, array<f32, 1>(), array<f32, 1>());
@group(3) @binding(0)
var<uniform> msdf: MsdfUBO;
var<private> in_position_1: vec3<f32>;
var<private> frag_color0_: vec4<f32>;
var<private> in_color0_1: vec4<f32>;
var<private> frag_tex_coord0_: vec2<f32>;
var<private> in_tex_coord0_1: vec2<f32>;

fn main_1() {
    let _e11 = msdf.mvp;
    let _e12 = in_position_1;
    unnamed.gl_Position = (_e11 * vec4<f32>(_e12.x, _e12.y, _e12.z, 1f));
    let _e19 = in_color0_1;
    frag_color0_ = _e19;
    let _e20 = in_tex_coord0_1;
    frag_tex_coord0_ = _e20;
    return;
}

@vertex
fn main(@location(0) in_position: vec3<f32>, @location(1) in_color0_: vec4<f32>, @location(2) in_tex_coord0_: vec2<f32>) -> VertexOutput {
    in_position_1 = in_position;
    in_color0_1 = in_color0_;
    in_tex_coord0_1 = in_tex_coord0_;
    main_1();
    let _e11 = unnamed.gl_Position.y;
    unnamed.gl_Position.y = -(_e11);
    let _e13 = unnamed.gl_Position;
    let _e14 = frag_color0_;
    let _e15 = frag_tex_coord0_;
    return VertexOutput(_e13, _e14, _e15);
}
