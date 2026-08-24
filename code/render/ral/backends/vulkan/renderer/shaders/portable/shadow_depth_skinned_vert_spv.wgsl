struct BoneMatrices {
    boneMats: array<vec4<f32>, 384>,
}

struct gl_PerVertex {
    @builtin(position) gl_Position: vec4<f32>,
}

struct CascadeMVP {
    cascadeMVP: mat4x4<f32>,
}

struct EntityMatrices {
    matrices: array<mat4x4<f32>>,
}

@group(2) @binding(0)
var<uniform> unnamed: BoneMatrices;
var<private> in_bone_weights_1: vec4<f32>;
var<private> in_bone_indices_1: vec4<u32>;
var<private> in_position_1: vec3<f32>;
var<private> unnamed_1: gl_PerVertex = gl_PerVertex(vec4<f32>(0f, 0f, 0f, 1f));
@group(1) @binding(0)
var<uniform> unnamed_2: CascadeMVP;
@group(0) @binding(0)
var<storage> unnamed_3: EntityMatrices;
var<private> gl_InstanceIndex_1: i32;
var<private> in_normal_1: vec3<f32>;
var<private> in_tex_coord_1: vec2<f32>;
var<private> in_tangent_1: vec4<f32>;

fn transformByBone_u0028_u1_u003b_vf3_u003b(idx: ptr<function, u32>, v: ptr<function, vec3<f32>>) -> vec3<f32> {
    let _e21 = (*idx);
    let _e26 = unnamed.boneMats[((_e21 * 3u) + 0u)];
    let _e27 = (*v);
    let _e33 = (*idx);
    let _e38 = unnamed.boneMats[((_e33 * 3u) + 1u)];
    let _e39 = (*v);
    let _e45 = (*idx);
    let _e50 = unnamed.boneMats[((_e45 * 3u) + 2u)];
    let _e51 = (*v);
    return vec3<f32>(dot(_e26, vec4<f32>(_e27.x, _e27.y, _e27.z, 1f)), dot(_e38, vec4<f32>(_e39.x, _e39.y, _e39.z, 1f)), dot(_e50, vec4<f32>(_e51.x, _e51.y, _e51.z, 1f)));
}

fn main_1() {
    var pos: vec3<f32>;
    var param: u32;
    var param_1: vec3<f32>;
    var param_2: u32;
    var param_3: vec3<f32>;
    var param_4: u32;
    var param_5: vec3<f32>;
    var param_6: u32;
    var param_7: vec3<f32>;

    let _e29 = in_bone_weights_1[0u];
    let _e31 = in_bone_indices_1[0u];
    param = _e31;
    let _e32 = in_position_1;
    param_1 = _e32;
    let _e33 = transformByBone_u0028_u1_u003b_vf3_u003b((&param), (&param_1));
    pos = (_e33 * _e29);
    let _e36 = in_bone_weights_1[1u];
    if (_e36 > 0f) {
        let _e39 = in_bone_weights_1[1u];
        let _e41 = in_bone_indices_1[1u];
        param_2 = _e41;
        let _e42 = in_position_1;
        param_3 = _e42;
        let _e43 = transformByBone_u0028_u1_u003b_vf3_u003b((&param_2), (&param_3));
        let _e45 = pos;
        pos = (_e45 + (_e43 * _e39));
    }
    let _e48 = in_bone_weights_1[2u];
    if (_e48 > 0f) {
        let _e51 = in_bone_weights_1[2u];
        let _e53 = in_bone_indices_1[2u];
        param_4 = _e53;
        let _e54 = in_position_1;
        param_5 = _e54;
        let _e55 = transformByBone_u0028_u1_u003b_vf3_u003b((&param_4), (&param_5));
        let _e57 = pos;
        pos = (_e57 + (_e55 * _e51));
    }
    let _e60 = in_bone_weights_1[3u];
    if (_e60 > 0f) {
        let _e63 = in_bone_weights_1[3u];
        let _e65 = in_bone_indices_1[3u];
        param_6 = _e65;
        let _e66 = in_position_1;
        param_7 = _e66;
        let _e67 = transformByBone_u0028_u1_u003b_vf3_u003b((&param_6), (&param_7));
        let _e69 = pos;
        pos = (_e69 + (_e67 * _e63));
    }
    let _e72 = unnamed_2.cascadeMVP;
    let _e73 = gl_InstanceIndex_1;
    let _e76 = unnamed_3.matrices[_e73];
    let _e78 = pos;
    unnamed_1.gl_Position = ((_e72 * _e76) * vec4<f32>(_e78.x, _e78.y, _e78.z, 1f));
    return;
}

@vertex
fn main(@location(4) in_bone_weights: vec4<f32>, @location(5) in_bone_indices: vec4<u32>, @location(0) in_position: vec3<f32>, @builtin(instance_index) gl_InstanceIndex: u32, @location(1) in_normal: vec3<f32>, @location(2) in_tex_coord: vec2<f32>, @location(3) in_tangent: vec4<f32>) -> @builtin(position) vec4<f32> {
    in_bone_weights_1 = in_bone_weights;
    in_bone_indices_1 = in_bone_indices;
    in_position_1 = in_position;
    gl_InstanceIndex_1 = i32(gl_InstanceIndex);
    in_normal_1 = in_normal;
    in_tex_coord_1 = in_tex_coord;
    in_tangent_1 = in_tangent;
    main_1();
    let _e18 = unnamed_1.gl_Position.y;
    unnamed_1.gl_Position.y = -(_e18);
    let _e20 = unnamed_1.gl_Position;
    return _e20;
}
