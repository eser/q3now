struct EntityMatrices {
    matrices: array<mat4x4<f32>>,
}

struct gl_PerVertex {
    @builtin(position) gl_Position: vec4<f32>,
}

struct CascadeMVP {
    cascadeMVP: mat4x4<f32>,
}

struct VertexOutput {
    @location(0) @interpolate(flat) member: f32,
    @builtin(position) gl_Position: vec4<f32>,
}

@group(0) @binding(0)
var<storage> unnamed: EntityMatrices;
var<private> gl_InstanceIndex_1: i32;
var<private> out_opacity: f32;
var<private> unnamed_1: gl_PerVertex = gl_PerVertex(vec4<f32>(0f, 0f, 0f, 1f));
@group(1) @binding(0)
var<uniform> unnamed_2: CascadeMVP;
var<private> in_position_1: vec3<f32>;

fn main_1() {
    var m: mat4x4<f32>;

    let _e11 = gl_InstanceIndex_1;
    let _e14 = unnamed.matrices[_e11];
    m = _e14;
    let _e17 = m[0][3u];
    out_opacity = _e17;
    m[0][3u] = 0f;
    let _e21 = unnamed_2.cascadeMVP;
    let _e22 = m;
    let _e24 = in_position_1;
    unnamed_1.gl_Position = ((_e21 * _e22) * vec4<f32>(_e24.x, _e24.y, _e24.z, 1f));
    return;
}

@vertex
fn main(@builtin(instance_index) gl_InstanceIndex: u32, @location(0) in_position: vec3<f32>) -> VertexOutput {
    gl_InstanceIndex_1 = i32(gl_InstanceIndex);
    in_position_1 = in_position;
    main_1();
    let _e9 = unnamed_1.gl_Position.y;
    unnamed_1.gl_Position.y = -(_e9);
    let _e11 = out_opacity;
    let _e12 = unnamed_1.gl_Position;
    return VertexOutput(_e11, _e12);
}
