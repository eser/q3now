struct gl_PerVertex {
    @builtin(position) gl_Position: vec4<f32>,
}

struct CascadeMVP {
    cascadeMVP: mat4x4<f32>,
}

struct EntityMatrices {
    matrices: array<mat4x4<f32>>,
}

var<private> unnamed: gl_PerVertex = gl_PerVertex(vec4<f32>(0f, 0f, 0f, 1f));
@group(1) @binding(0) 
var<uniform> unnamed_1: CascadeMVP;
@group(0) @binding(0) 
var<storage> unnamed_2: EntityMatrices;
var<private> gl_InstanceIndex_1: i32;
var<private> in_position_1: vec3<f32>;

fn main_1() {
    let _e8 = unnamed_1.cascadeMVP;
    let _e9 = gl_InstanceIndex_1;
    let _e12 = unnamed_2.matrices[_e9];
    let _e14 = in_position_1;
    unnamed.gl_Position = ((_e8 * _e12) * vec4<f32>(_e14.x, _e14.y, _e14.z, 1f));
    return;
}

@vertex 
fn main(@builtin(instance_index) gl_InstanceIndex: u32, @location(0) in_position: vec3<f32>) -> @builtin(position) vec4<f32> {
    gl_InstanceIndex_1 = i32(gl_InstanceIndex);
    in_position_1 = in_position;
    main_1();
    let _e8 = unnamed.gl_Position.y;
    unnamed.gl_Position.y = -(_e8);
    let _e10 = unnamed.gl_Position;
    return _e10;
}
