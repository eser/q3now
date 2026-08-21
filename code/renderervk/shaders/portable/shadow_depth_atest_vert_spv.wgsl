struct gl_PerVertex {
    @builtin(position) gl_Position: vec4<f32>,
}

struct CascadeMVP {
    cascadeMVP: mat4x4<f32>,
}

struct EntityMatrices {
    matrices: array<mat4x4<f32>>,
}

struct VertexOutput {
    @location(0) member: vec2<f32>,
    @location(1) @interpolate(flat) member_1: u32,
    @builtin(position) gl_Position: vec4<f32>,
}

var<private> out_texcoord: vec2<f32>;
var<private> in_texcoord_1: vec2<f32>;
var<private> out_packed: u32;
var<private> in_packed_1: f32;
var<private> unnamed: gl_PerVertex = gl_PerVertex(vec4<f32>(0f, 0f, 0f, 1f));
@group(1) @binding(0) 
var<uniform> unnamed_1: CascadeMVP;
@group(0) @binding(0) 
var<storage> unnamed_2: EntityMatrices;
var<private> gl_InstanceIndex_1: i32;
var<private> in_position_1: vec3<f32>;

fn main_1() {
    let _e11 = in_texcoord_1;
    out_texcoord = _e11;
    let _e12 = in_packed_1;
    out_packed = bitcast<u32>(_e12);
    let _e15 = unnamed_1.cascadeMVP;
    let _e16 = gl_InstanceIndex_1;
    let _e19 = unnamed_2.matrices[_e16];
    let _e21 = in_position_1;
    unnamed.gl_Position = ((_e15 * _e19) * vec4<f32>(_e21.x, _e21.y, _e21.z, 1f));
    return;
}

@vertex 
fn main(@location(1) in_texcoord: vec2<f32>, @location(2) in_packed: f32, @builtin(instance_index) gl_InstanceIndex: u32, @location(0) in_position: vec3<f32>) -> VertexOutput {
    in_texcoord_1 = in_texcoord;
    in_packed_1 = in_packed;
    gl_InstanceIndex_1 = i32(gl_InstanceIndex);
    in_position_1 = in_position;
    main_1();
    let _e14 = unnamed.gl_Position.y;
    unnamed.gl_Position.y = -(_e14);
    let _e16 = out_texcoord;
    let _e17 = out_packed;
    let _e18 = unnamed.gl_Position;
    return VertexOutput(_e16, _e17, _e18);
}
