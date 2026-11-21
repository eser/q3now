struct BoneMatrices {
    boneMats: array<vec4<f32>, 384>,
}

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

@group(2) @binding(0)
var<uniform> unnamed: BoneMatrices;
var<private> in_bone_weights_1: vec4<f32>;
var<private> in_bone_indices_1: vec4<u32>;
var<private> in_position_1: vec3<f32>;
@group(0) @binding(0)
var<storage> unnamed_1: EntityMatrices;
var<private> gl_InstanceIndex_1: i32;
var<private> out_opacity: f32;
var<private> unnamed_2: gl_PerVertex = gl_PerVertex(vec4<f32>(0f, 0f, 0f, 1f));
@group(1) @binding(0)
var<uniform> unnamed_3: CascadeMVP;
var<private> in_normal_1: vec3<f32>;
var<private> in_tex_coord_1: vec2<f32>;
var<private> in_tangent_1: vec4<f32>;

fn transformByBone_u0028_u1_u003b_vf3_u003b(idx: ptr<function, u32>, v: ptr<function, vec3<f32>>) -> vec3<f32> {
    let _e22 = (*idx);
    let _e27 = unnamed.boneMats[((_e22 * 3u) + 0u)];
    let _e28 = (*v);
    let _e34 = (*idx);
    let _e39 = unnamed.boneMats[((_e34 * 3u) + 1u)];
    let _e40 = (*v);
    let _e46 = (*idx);
    let _e51 = unnamed.boneMats[((_e46 * 3u) + 2u)];
    let _e52 = (*v);
    return vec3<f32>(dot(_e27, vec4<f32>(_e28.x, _e28.y, _e28.z, 1f)), dot(_e39, vec4<f32>(_e40.x, _e40.y, _e40.z, 1f)), dot(_e51, vec4<f32>(_e52.x, _e52.y, _e52.z, 1f)));
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
    var m: mat4x4<f32>;

    let _e31 = in_bone_weights_1[0u];
    let _e33 = in_bone_indices_1[0u];
    param = _e33;
    let _e34 = in_position_1;
    param_1 = _e34;
    let _e35 = transformByBone_u0028_u1_u003b_vf3_u003b((&param), (&param_1));
    pos = (_e35 * _e31);
    let _e38 = in_bone_weights_1[1u];
    if (_e38 > 0f) {
        let _e41 = in_bone_weights_1[1u];
        let _e43 = in_bone_indices_1[1u];
        param_2 = _e43;
        let _e44 = in_position_1;
        param_3 = _e44;
        let _e45 = transformByBone_u0028_u1_u003b_vf3_u003b((&param_2), (&param_3));
        let _e47 = pos;
        pos = (_e47 + (_e45 * _e41));
    }
    let _e50 = in_bone_weights_1[2u];
    if (_e50 > 0f) {
        let _e53 = in_bone_weights_1[2u];
        let _e55 = in_bone_indices_1[2u];
        param_4 = _e55;
        let _e56 = in_position_1;
        param_5 = _e56;
        let _e57 = transformByBone_u0028_u1_u003b_vf3_u003b((&param_4), (&param_5));
        let _e59 = pos;
        pos = (_e59 + (_e57 * _e53));
    }
    let _e62 = in_bone_weights_1[3u];
    if (_e62 > 0f) {
        let _e65 = in_bone_weights_1[3u];
        let _e67 = in_bone_indices_1[3u];
        param_6 = _e67;
        let _e68 = in_position_1;
        param_7 = _e68;
        let _e69 = transformByBone_u0028_u1_u003b_vf3_u003b((&param_6), (&param_7));
        let _e71 = pos;
        pos = (_e71 + (_e69 * _e65));
    }
    let _e73 = gl_InstanceIndex_1;
    let _e76 = unnamed_1.matrices[_e73];
    m = _e76;
    let _e79 = m[0][3u];
    out_opacity = _e79;
    m[0][3u] = 0f;
    let _e83 = unnamed_3.cascadeMVP;
    let _e84 = m;
    let _e86 = pos;
    unnamed_2.gl_Position = ((_e83 * _e84) * vec4<f32>(_e86.x, _e86.y, _e86.z, 1f));
    return;
}

@vertex
fn main(@location(4) in_bone_weights: vec4<f32>, @location(5) in_bone_indices: vec4<u32>, @location(0) in_position: vec3<f32>, @builtin(instance_index) gl_InstanceIndex: u32, @location(1) in_normal: vec3<f32>, @location(2) in_tex_coord: vec2<f32>, @location(3) in_tangent: vec4<f32>) -> VertexOutput {
    in_bone_weights_1 = in_bone_weights;
    in_bone_indices_1 = in_bone_indices;
    in_position_1 = in_position;
    gl_InstanceIndex_1 = i32(gl_InstanceIndex);
    in_normal_1 = in_normal;
    in_tex_coord_1 = in_tex_coord;
    in_tangent_1 = in_tangent;
    main_1();
    let _e19 = unnamed_2.gl_Position.y;
    unnamed_2.gl_Position.y = -(_e19);
    let _e21 = out_opacity;
    let _e22 = unnamed_2.gl_Position;
    return VertexOutput(_e21, _e22);
}
