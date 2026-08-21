struct gl_PerVertex {
    @builtin(position) gl_Position: vec4<f32>,
}

struct RtMetrics {
    rtMetrics: vec4<f32>,
}

struct VertexOutput {
    @builtin(position) gl_Position: vec4<f32>,
    @location(0) member: vec2<f32>,
    @location(1) member_1: vec2<f32>,
    @location(2) member_2: vec4<f32>,
    @location(3) member_3: vec4<f32>,
    @location(4) member_4: vec4<f32>,
}

@id(0) override SMAA_MAX_SEARCH_STEPS: i32 = 16i;

var<private> gl_VertexIndex_1: i32;
var<private> unnamed: gl_PerVertex = gl_PerVertex(vec4<f32>(0f, 0f, 0f, 1f));
var<private> texcoord: vec2<f32>;
var<private> pixcoord: vec2<f32>;
@group(3) @binding(0) 
var<uniform> unnamed_1: RtMetrics;
var<private> offset0_: vec4<f32>;
var<private> offset1_: vec4<f32>;
var<private> offset2_: vec4<f32>;

fn main_1() {
    var pos: vec2<f32>;

    let _e23 = gl_VertexIndex_1;
    let _e28 = gl_VertexIndex_1;
    pos = vec2<f32>(f32(((_e23 << bitcast<u32>(1i)) & 2i)), f32((_e28 & 2i)));
    let _e32 = pos;
    let _e35 = ((_e32 * 2f) - vec2(1f));
    unnamed.gl_Position = vec4<f32>(_e35.x, _e35.y, 0f, 1f);
    let _e40 = pos;
    texcoord = _e40;
    let _e41 = texcoord;
    let _e43 = unnamed_1.rtMetrics;
    pixcoord = (_e41 * _e43.zw);
    let _e46 = texcoord;
    let _e49 = unnamed_1.rtMetrics;
    offset0_ = (_e46.xyxy + (_e49.xyxy * vec4<f32>(-0.25f, -0.125f, 1.25f, -0.125f)));
    let _e53 = texcoord;
    let _e56 = unnamed_1.rtMetrics;
    offset1_ = (_e53.xyxy + (_e56.xyxy * vec4<f32>(-0.125f, -0.25f, -0.125f, 1.25f)));
    let _e60 = offset0_;
    let _e61 = _e60.xz;
    let _e62 = offset1_;
    let _e63 = _e62.yw;
    let _e70 = unnamed_1.rtMetrics;
    offset2_ = (vec4<f32>(_e61.x, _e61.y, _e63.x, _e63.y) + ((vec4<f32>(-2f, 2f, -2f, 2f) * _e70.xxyy) * f32(SMAA_MAX_SEARCH_STEPS)));
    return;
}

@vertex 
fn main(@builtin(vertex_index) gl_VertexIndex: u32) -> VertexOutput {
    gl_VertexIndex_1 = i32(gl_VertexIndex);
    main_1();
    let _e11 = unnamed.gl_Position.y;
    unnamed.gl_Position.y = -(_e11);
    let _e13 = unnamed.gl_Position;
    let _e14 = texcoord;
    let _e15 = pixcoord;
    let _e16 = offset0_;
    let _e17 = offset1_;
    let _e18 = offset2_;
    return VertexOutput(_e13, _e14, _e15, _e16, _e17, _e18);
}
