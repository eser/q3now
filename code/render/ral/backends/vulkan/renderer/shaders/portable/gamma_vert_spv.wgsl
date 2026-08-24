struct gl_PerVertex {
    @builtin(position) gl_Position: vec4<f32>,
}

struct VertexOutput {
    @builtin(position) gl_Position: vec4<f32>,
    @location(0) member: vec2<f32>,
}

var<private> unnamed: gl_PerVertex = gl_PerVertex(vec4<f32>(0f, 0f, 0f, 1f));
var<private> gl_VertexIndex_1: i32;
var<private> frag_tex_coord: vec2<f32>;

fn main_1() {
    var indexable: array<vec2<f32>, 4>;
    var indexable_1: array<vec2<f32>, 4>;

    let _e19 = gl_VertexIndex_1;
    indexable = array<vec2<f32>, 4>(vec2<f32>(-1f, 1f), vec2<f32>(-1f, -1f), vec2<f32>(1f, 1f), vec2<f32>(1f, -1f));
    let _e21 = indexable[_e19];
    unnamed.gl_Position = vec4<f32>(_e21.x, _e21.y, 0f, 1f);
    let _e26 = gl_VertexIndex_1;
    indexable_1 = array<vec2<f32>, 4>(vec2<f32>(0f, 1f), vec2<f32>(0f, 0f), vec2<f32>(1f, 1f), vec2<f32>(1f, 0f));
    let _e28 = indexable_1[_e26];
    frag_tex_coord = _e28;
    return;
}

@vertex
fn main(@builtin(vertex_index) gl_VertexIndex: u32) -> VertexOutput {
    gl_VertexIndex_1 = i32(gl_VertexIndex);
    main_1();
    let _e7 = unnamed.gl_Position.y;
    unnamed.gl_Position.y = -(_e7);
    let _e9 = unnamed.gl_Position;
    let _e10 = frag_tex_coord;
    return VertexOutput(_e9, _e10);
}
