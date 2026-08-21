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
    @location(2) member_2: vec4<f32>,
    @location(3) member_3: vec4<f32>,
}

var<private> gl_VertexIndex_1: i32;
var<private> unnamed: gl_PerVertex = gl_PerVertex(vec4<f32>(0f, 0f, 0f, 1f));
var<private> texcoord: vec2<f32>;
var<private> offset0_: vec4<f32>;
@group(3) @binding(0) 
var<uniform> unnamed_1: RtMetrics;
var<private> offset1_: vec4<f32>;
var<private> offset2_: vec4<f32>;

fn main_1() {
    var pos: vec2<f32>;

    let _e19 = gl_VertexIndex_1;
    let _e24 = gl_VertexIndex_1;
    pos = vec2<f32>(f32(((_e19 << bitcast<u32>(1i)) & 2i)), f32((_e24 & 2i)));
    let _e28 = pos;
    let _e31 = ((_e28 * 2f) - vec2(1f));
    unnamed.gl_Position = vec4<f32>(_e31.x, _e31.y, 0f, 1f);
    let _e36 = pos;
    texcoord = _e36;
    let _e37 = texcoord;
    let _e40 = unnamed_1.rtMetrics;
    offset0_ = (_e37.xyxy + (_e40.xyxy * vec4<f32>(-1f, 0f, 0f, -1f)));
    let _e44 = texcoord;
    let _e47 = unnamed_1.rtMetrics;
    offset1_ = (_e44.xyxy + (_e47.xyxy * vec4<f32>(1f, 0f, 0f, 1f)));
    let _e51 = texcoord;
    let _e54 = unnamed_1.rtMetrics;
    offset2_ = (_e51.xyxy + (_e54.xyxy * vec4<f32>(-2f, 0f, 0f, -2f)));
    return;
}

@vertex 
fn main(@builtin(vertex_index) gl_VertexIndex: u32) -> VertexOutput {
    gl_VertexIndex_1 = i32(gl_VertexIndex);
    main_1();
    let _e10 = unnamed.gl_Position.y;
    unnamed.gl_Position.y = -(_e10);
    let _e12 = unnamed.gl_Position;
    let _e13 = texcoord;
    let _e14 = offset0_;
    let _e15 = offset1_;
    let _e16 = offset2_;
    return VertexOutput(_e12, _e13, _e14, _e15, _e16);
}
