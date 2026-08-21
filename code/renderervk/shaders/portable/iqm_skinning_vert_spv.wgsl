struct BoneMatrices {
    boneMats: array<vec4<f32>, 384>,
    mvp: mat4x4<f32>,
}

struct gl_PerVertex {
    @builtin(position) gl_Position: vec4<f32>,
}

struct VertexOutput {
    @builtin(position) gl_Position: vec4<f32>,
    @location(0) member: vec2<f32>,
    @location(1) member_1: vec3<f32>,
    @location(2) member_2: vec4<f32>,
}

@group(0) @binding(0) 
var<uniform> unnamed: BoneMatrices;
var<private> in_bone_weights_1: vec4<f32>;
var<private> in_bone_indices_1: vec4<u32>;
var<private> in_position_1: vec3<f32>;
var<private> in_normal_1: vec3<f32>;
var<private> in_tangent_1: vec4<f32>;
var<private> unnamed_1: gl_PerVertex = gl_PerVertex(vec4<f32>(0f, 0f, 0f, 1f));
var<private> frag_tex_coord: vec2<f32>;
var<private> in_tex_coord_1: vec2<f32>;
var<private> frag_normal: vec3<f32>;
var<private> frag_tangent: vec4<f32>;

fn transformNormalByBone_u0028_u1_u003b_vf3_u003b(idx: ptr<function, u32>, n: ptr<function, vec3<f32>>) -> vec3<f32> {
    let _e23 = (*idx);
    let _e28 = unnamed.boneMats[((_e23 * 3u) + 0u)];
    let _e30 = (*n);
    let _e32 = (*idx);
    let _e37 = unnamed.boneMats[((_e32 * 3u) + 1u)];
    let _e39 = (*n);
    let _e41 = (*idx);
    let _e46 = unnamed.boneMats[((_e41 * 3u) + 2u)];
    let _e48 = (*n);
    return vec3<f32>(dot(_e28.xyz, _e30), dot(_e37.xyz, _e39), dot(_e46.xyz, _e48));
}

fn transformByBone_u0028_u1_u003b_vf3_u003b(idx_1: ptr<function, u32>, v: ptr<function, vec3<f32>>) -> vec3<f32> {
    let _e23 = (*idx_1);
    let _e28 = unnamed.boneMats[((_e23 * 3u) + 0u)];
    let _e29 = (*v);
    let _e35 = (*idx_1);
    let _e40 = unnamed.boneMats[((_e35 * 3u) + 1u)];
    let _e41 = (*v);
    let _e47 = (*idx_1);
    let _e52 = unnamed.boneMats[((_e47 * 3u) + 2u)];
    let _e53 = (*v);
    return vec3<f32>(dot(_e28, vec4<f32>(_e29.x, _e29.y, _e29.z, 1f)), dot(_e40, vec4<f32>(_e41.x, _e41.y, _e41.z, 1f)), dot(_e52, vec4<f32>(_e53.x, _e53.y, _e53.z, 1f)));
}

fn main_1() {
    var pos: vec3<f32>;
    var nrm: vec3<f32>;
    var tan_: vec3<f32>;
    var param: u32;
    var param_1: vec3<f32>;
    var param_2: u32;
    var param_3: vec3<f32>;
    var param_4: u32;
    var param_5: vec3<f32>;
    var param_6: u32;
    var param_7: vec3<f32>;
    var param_8: u32;
    var param_9: vec3<f32>;
    var param_10: u32;
    var param_11: vec3<f32>;
    var param_12: u32;
    var param_13: vec3<f32>;
    var param_14: u32;
    var param_15: vec3<f32>;
    var param_16: u32;
    var param_17: vec3<f32>;
    var param_18: u32;
    var param_19: vec3<f32>;
    var param_20: u32;
    var param_21: vec3<f32>;
    var param_22: u32;
    var param_23: vec3<f32>;

    pos = vec3<f32>(0f, 0f, 0f);
    nrm = vec3<f32>(0f, 0f, 0f);
    tan_ = vec3<f32>(0f, 0f, 0f);
    let _e49 = in_bone_weights_1[0u];
    let _e51 = in_bone_indices_1[0u];
    param = _e51;
    let _e52 = in_position_1;
    param_1 = _e52;
    let _e53 = transformByBone_u0028_u1_u003b_vf3_u003b((&param), (&param_1));
    let _e55 = pos;
    pos = (_e55 + (_e53 * _e49));
    let _e58 = in_bone_weights_1[0u];
    let _e60 = in_bone_indices_1[0u];
    param_2 = _e60;
    let _e61 = in_normal_1;
    param_3 = _e61;
    let _e62 = transformNormalByBone_u0028_u1_u003b_vf3_u003b((&param_2), (&param_3));
    let _e64 = nrm;
    nrm = (_e64 + (_e62 * _e58));
    let _e67 = in_bone_weights_1[0u];
    let _e69 = in_bone_indices_1[0u];
    param_4 = _e69;
    let _e70 = in_tangent_1;
    param_5 = _e70.xyz;
    let _e72 = transformNormalByBone_u0028_u1_u003b_vf3_u003b((&param_4), (&param_5));
    let _e74 = tan_;
    tan_ = (_e74 + (_e72 * _e67));
    let _e77 = in_bone_weights_1[1u];
    if (_e77 > 0f) {
        let _e80 = in_bone_weights_1[1u];
        let _e82 = in_bone_indices_1[1u];
        param_6 = _e82;
        let _e83 = in_position_1;
        param_7 = _e83;
        let _e84 = transformByBone_u0028_u1_u003b_vf3_u003b((&param_6), (&param_7));
        let _e86 = pos;
        pos = (_e86 + (_e84 * _e80));
        let _e89 = in_bone_weights_1[1u];
        let _e91 = in_bone_indices_1[1u];
        param_8 = _e91;
        let _e92 = in_normal_1;
        param_9 = _e92;
        let _e93 = transformNormalByBone_u0028_u1_u003b_vf3_u003b((&param_8), (&param_9));
        let _e95 = nrm;
        nrm = (_e95 + (_e93 * _e89));
        let _e98 = in_bone_weights_1[1u];
        let _e100 = in_bone_indices_1[1u];
        param_10 = _e100;
        let _e101 = in_tangent_1;
        param_11 = _e101.xyz;
        let _e103 = transformNormalByBone_u0028_u1_u003b_vf3_u003b((&param_10), (&param_11));
        let _e105 = tan_;
        tan_ = (_e105 + (_e103 * _e98));
    }
    let _e108 = in_bone_weights_1[2u];
    if (_e108 > 0f) {
        let _e111 = in_bone_weights_1[2u];
        let _e113 = in_bone_indices_1[2u];
        param_12 = _e113;
        let _e114 = in_position_1;
        param_13 = _e114;
        let _e115 = transformByBone_u0028_u1_u003b_vf3_u003b((&param_12), (&param_13));
        let _e117 = pos;
        pos = (_e117 + (_e115 * _e111));
        let _e120 = in_bone_weights_1[2u];
        let _e122 = in_bone_indices_1[2u];
        param_14 = _e122;
        let _e123 = in_normal_1;
        param_15 = _e123;
        let _e124 = transformNormalByBone_u0028_u1_u003b_vf3_u003b((&param_14), (&param_15));
        let _e126 = nrm;
        nrm = (_e126 + (_e124 * _e120));
        let _e129 = in_bone_weights_1[2u];
        let _e131 = in_bone_indices_1[2u];
        param_16 = _e131;
        let _e132 = in_tangent_1;
        param_17 = _e132.xyz;
        let _e134 = transformNormalByBone_u0028_u1_u003b_vf3_u003b((&param_16), (&param_17));
        let _e136 = tan_;
        tan_ = (_e136 + (_e134 * _e129));
    }
    let _e139 = in_bone_weights_1[3u];
    if (_e139 > 0f) {
        let _e142 = in_bone_weights_1[3u];
        let _e144 = in_bone_indices_1[3u];
        param_18 = _e144;
        let _e145 = in_position_1;
        param_19 = _e145;
        let _e146 = transformByBone_u0028_u1_u003b_vf3_u003b((&param_18), (&param_19));
        let _e148 = pos;
        pos = (_e148 + (_e146 * _e142));
        let _e151 = in_bone_weights_1[3u];
        let _e153 = in_bone_indices_1[3u];
        param_20 = _e153;
        let _e154 = in_normal_1;
        param_21 = _e154;
        let _e155 = transformNormalByBone_u0028_u1_u003b_vf3_u003b((&param_20), (&param_21));
        let _e157 = nrm;
        nrm = (_e157 + (_e155 * _e151));
        let _e160 = in_bone_weights_1[3u];
        let _e162 = in_bone_indices_1[3u];
        param_22 = _e162;
        let _e163 = in_tangent_1;
        param_23 = _e163.xyz;
        let _e165 = transformNormalByBone_u0028_u1_u003b_vf3_u003b((&param_22), (&param_23));
        let _e167 = tan_;
        tan_ = (_e167 + (_e165 * _e160));
    }
    let _e170 = unnamed.mvp;
    let _e171 = pos;
    unnamed_1.gl_Position = (_e170 * vec4<f32>(_e171.x, _e171.y, _e171.z, 1f));
    let _e178 = in_tex_coord_1;
    frag_tex_coord = _e178;
    let _e179 = nrm;
    frag_normal = normalize(_e179);
    let _e181 = tan_;
    let _e182 = normalize(_e181);
    let _e184 = in_tangent_1[3u];
    frag_tangent = vec4<f32>(_e182.x, _e182.y, _e182.z, _e184);
    return;
}

@vertex 
fn main(@location(4) in_bone_weights: vec4<f32>, @location(5) in_bone_indices: vec4<u32>, @location(0) in_position: vec3<f32>, @location(1) in_normal: vec3<f32>, @location(3) in_tangent: vec4<f32>, @location(2) in_tex_coord: vec2<f32>) -> VertexOutput {
    in_bone_weights_1 = in_bone_weights;
    in_bone_indices_1 = in_bone_indices;
    in_position_1 = in_position;
    in_normal_1 = in_normal;
    in_tangent_1 = in_tangent;
    in_tex_coord_1 = in_tex_coord;
    main_1();
    let _e18 = unnamed_1.gl_Position.y;
    unnamed_1.gl_Position.y = -(_e18);
    let _e20 = unnamed_1.gl_Position;
    let _e21 = frag_tex_coord;
    let _e22 = frag_normal;
    let _e23 = frag_tangent;
    return VertexOutput(_e20, _e21, _e22, _e23);
}
