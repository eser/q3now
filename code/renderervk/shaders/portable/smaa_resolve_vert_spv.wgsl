struct gl_PerVertex {
    @builtin(position) gl_Position: vec4<f32>,
}

struct RtMetrics {
    rtMetrics: vec4<f32>,
}

struct VertexOutput {
    @builtin(position) gl_Position: vec4<f32>,
    @location(0) member: vec2<f32>,
    @location(1) member_1: vec4<f32>,
}

var<private> gl_VertexIndex_1: i32;
var<private> unnamed: gl_PerVertex = gl_PerVertex(vec4<f32>(0f, 0f, 0f, 1f));
var<private> texcoord: vec2<f32>;
var<private> offset: vec4<f32>;
@group(3) @binding(0) 
var<uniform> unnamed_1: RtMetrics;

fn main_1() {
    var pos: vec2<f32>;

    let _e13 = gl_VertexIndex_1;
    let _e18 = gl_VertexIndex_1;
    pos = vec2<f32>(f32(((_e13 << bitcast<u32>(1i)) & 2i)), f32((_e18 & 2i)));
    let _e22 = pos;
    let _e25 = ((_e22 * 2f) - vec2(1f));
    unnamed.gl_Position = vec4<f32>(_e25.x, _e25.y, 0f, 1f);
    let _e30 = pos;
    texcoord = _e30;
    let _e31 = texcoord;
    let _e34 = unnamed_1.rtMetrics;
    offset = (_e31.xyxy + (_e34.xyxy * vec4<f32>(1f, 0f, 0f, 1f)));
    return;
}

@vertex 
fn main(@builtin(vertex_index) gl_VertexIndex: u32) -> VertexOutput {
    gl_VertexIndex_1 = i32(gl_VertexIndex);
    main_1();
    let _e8 = unnamed.gl_Position.y;
    unnamed.gl_Position.y = -(_e8);
    let _e10 = unnamed.gl_Position;
    let _e11 = texcoord;
    let _e12 = offset;
    return VertexOutput(_e10, _e11, _e12);
}
