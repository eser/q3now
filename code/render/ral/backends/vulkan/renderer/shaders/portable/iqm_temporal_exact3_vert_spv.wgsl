struct TemporalIqmRecord {
    currentBones: array<vec4<f32>, 384>,
    previousBones: array<vec4<f32>, 384>,
    rasterMvp: mat4x4<f32>,
    temporalCurrentMvp: mat4x4<f32>,
    temporalPreviousMvp: mat4x4<f32>,
}

struct TemporalIqmPayload {
    records: array<TemporalIqmRecord>,
}

struct gl_PerVertex {
    @builtin(position) gl_Position: vec4<f32>,
}

struct VertexOutput {
    @builtin(position) gl_Position: vec4<f32>,
    @location(0) member: vec2<f32>,
    @location(1) member_1: vec3<f32>,
    @location(2) member_2: vec4<f32>,
    @location(10) member_3: vec4<f32>,
    @location(11) member_4: vec4<f32>,
}

@group(0) @binding(0)
var<storage> temporalIqmPayload: TemporalIqmPayload;
var<private> in_bone_weights_1: vec4<f32>;
var<private> in_bone_indices_1: vec4<u32>;
var<private> in_position_1: vec3<f32>;
var<private> gl_InstanceIndex_1: i32;
var<private> unnamed: gl_PerVertex = gl_PerVertex(vec4<f32>(0f, 0f, 0f, 1f));
var<private> frag_tex_coord: vec2<f32>;
var<private> frag_normal: vec3<f32>;
var<private> frag_tangent: vec4<f32>;
var<private> temporalCurrentClip: vec4<f32>;
var<private> temporalPreviousClip: vec4<f32>;
var<private> in_normal_1: vec3<f32>;
var<private> in_tangent_1: vec4<f32>;
var<private> in_tex_coord_1: vec2<f32>;

fn currentBoneRow_u0028_u1_u003b_u1_u003b_u1_u003b(recordIndex: ptr<function, u32>, index: ptr<function, u32>, row: ptr<function, u32>) -> vec4<f32> {
    var offset: u32;

    let _e36 = (*index);
    let _e38 = (*row);
    offset = ((_e36 * 3u) + _e38);
    let _e40 = (*recordIndex);
    let _e41 = offset;
    let _e46 = temporalIqmPayload.records[_e40].currentBones[_e41];
    return _e46;
}

fn transformDirection_u0028_u1_u003b_u1_u003b_vf3_u003b(recordIndex_1: ptr<function, u32>, index_1: ptr<function, u32>, direction: ptr<function, vec3<f32>>) -> vec3<f32> {
    var param: u32;
    var param_1: u32;
    var param_2: u32;
    var param_3: u32;
    var param_4: u32;
    var param_5: u32;
    var param_6: u32;
    var param_7: u32;
    var param_8: u32;

    let _e44 = (*recordIndex_1);
    param = _e44;
    let _e45 = (*index_1);
    param_1 = _e45;
    param_2 = 0u;
    let _e46 = currentBoneRow_u0028_u1_u003b_u1_u003b_u1_u003b((&param), (&param_1), (&param_2));
    let _e48 = (*direction);
    let _e50 = (*recordIndex_1);
    param_3 = _e50;
    let _e51 = (*index_1);
    param_4 = _e51;
    param_5 = 1u;
    let _e52 = currentBoneRow_u0028_u1_u003b_u1_u003b_u1_u003b((&param_3), (&param_4), (&param_5));
    let _e54 = (*direction);
    let _e56 = (*recordIndex_1);
    param_6 = _e56;
    let _e57 = (*index_1);
    param_7 = _e57;
    param_8 = 2u;
    let _e58 = currentBoneRow_u0028_u1_u003b_u1_u003b_u1_u003b((&param_6), (&param_7), (&param_8));
    let _e60 = (*direction);
    return vec3<f32>(dot(_e46.xyz, _e48), dot(_e52.xyz, _e54), dot(_e58.xyz, _e60));
}

fn skinDirection_u0028_u1_u003b_vf3_u003b(recordIndex_2: ptr<function, u32>, direction_1: ptr<function, vec3<f32>>) -> vec3<f32> {
    var value: vec3<f32>;
    var param_9: u32;
    var param_10: u32;
    var param_11: vec3<f32>;
    var param_12: u32;
    var param_13: u32;
    var param_14: vec3<f32>;
    var param_15: u32;
    var param_16: u32;
    var param_17: vec3<f32>;
    var param_18: u32;
    var param_19: u32;
    var param_20: vec3<f32>;

    let _e48 = in_bone_weights_1[0u];
    let _e49 = (*recordIndex_2);
    param_9 = _e49;
    let _e51 = in_bone_indices_1[0u];
    param_10 = _e51;
    let _e52 = (*direction_1);
    param_11 = _e52;
    let _e53 = transformDirection_u0028_u1_u003b_u1_u003b_vf3_u003b((&param_9), (&param_10), (&param_11));
    value = (_e53 * _e48);
    let _e56 = in_bone_weights_1[1u];
    if (_e56 > 0f) {
        let _e59 = in_bone_weights_1[1u];
        let _e60 = (*recordIndex_2);
        param_12 = _e60;
        let _e62 = in_bone_indices_1[1u];
        param_13 = _e62;
        let _e63 = (*direction_1);
        param_14 = _e63;
        let _e64 = transformDirection_u0028_u1_u003b_u1_u003b_vf3_u003b((&param_12), (&param_13), (&param_14));
        let _e66 = value;
        value = (_e66 + (_e64 * _e59));
    }
    let _e69 = in_bone_weights_1[2u];
    if (_e69 > 0f) {
        let _e72 = in_bone_weights_1[2u];
        let _e73 = (*recordIndex_2);
        param_15 = _e73;
        let _e75 = in_bone_indices_1[2u];
        param_16 = _e75;
        let _e76 = (*direction_1);
        param_17 = _e76;
        let _e77 = transformDirection_u0028_u1_u003b_u1_u003b_vf3_u003b((&param_15), (&param_16), (&param_17));
        let _e79 = value;
        value = (_e79 + (_e77 * _e72));
    }
    let _e82 = in_bone_weights_1[3u];
    if (_e82 > 0f) {
        let _e85 = in_bone_weights_1[3u];
        let _e86 = (*recordIndex_2);
        param_18 = _e86;
        let _e88 = in_bone_indices_1[3u];
        param_19 = _e88;
        let _e89 = (*direction_1);
        param_20 = _e89;
        let _e90 = transformDirection_u0028_u1_u003b_u1_u003b_vf3_u003b((&param_18), (&param_19), (&param_20));
        let _e92 = value;
        value = (_e92 + (_e90 * _e85));
    }
    let _e94 = value;
    return _e94;
}

fn previousBoneRow_u0028_u1_u003b_u1_u003b_u1_u003b(recordIndex_3: ptr<function, u32>, index_2: ptr<function, u32>, row_1: ptr<function, u32>) -> vec4<f32> {
    var offset_1: u32;

    let _e36 = (*index_2);
    let _e38 = (*row_1);
    offset_1 = ((_e36 * 3u) + _e38);
    let _e40 = (*recordIndex_3);
    let _e41 = offset_1;
    let _e46 = temporalIqmPayload.records[_e40].previousBones[_e41];
    return _e46;
}

fn transformPreviousPosition_u0028_u1_u003b_u1_u003b_vf3_u003b(recordIndex_4: ptr<function, u32>, index_3: ptr<function, u32>, position: ptr<function, vec3<f32>>) -> vec3<f32> {
    var param_21: u32;
    var param_22: u32;
    var param_23: u32;
    var param_24: u32;
    var param_25: u32;
    var param_26: u32;
    var param_27: u32;
    var param_28: u32;
    var param_29: u32;

    let _e44 = (*recordIndex_4);
    param_21 = _e44;
    let _e45 = (*index_3);
    param_22 = _e45;
    param_23 = 0u;
    let _e46 = previousBoneRow_u0028_u1_u003b_u1_u003b_u1_u003b((&param_21), (&param_22), (&param_23));
    let _e47 = (*position);
    let _e53 = (*recordIndex_4);
    param_24 = _e53;
    let _e54 = (*index_3);
    param_25 = _e54;
    param_26 = 1u;
    let _e55 = previousBoneRow_u0028_u1_u003b_u1_u003b_u1_u003b((&param_24), (&param_25), (&param_26));
    let _e56 = (*position);
    let _e62 = (*recordIndex_4);
    param_27 = _e62;
    let _e63 = (*index_3);
    param_28 = _e63;
    param_29 = 2u;
    let _e64 = previousBoneRow_u0028_u1_u003b_u1_u003b_u1_u003b((&param_27), (&param_28), (&param_29));
    let _e65 = (*position);
    return vec3<f32>(dot(_e46, vec4<f32>(_e47.x, _e47.y, _e47.z, 1f)), dot(_e55, vec4<f32>(_e56.x, _e56.y, _e56.z, 1f)), dot(_e64, vec4<f32>(_e65.x, _e65.y, _e65.z, 1f)));
}

fn skinPreviousPosition_u0028_u1_u003b(recordIndex_5: ptr<function, u32>) -> vec3<f32> {
    var value_1: vec3<f32>;
    var param_30: u32;
    var param_31: u32;
    var param_32: vec3<f32>;
    var param_33: u32;
    var param_34: u32;
    var param_35: vec3<f32>;
    var param_36: u32;
    var param_37: u32;
    var param_38: vec3<f32>;
    var param_39: u32;
    var param_40: u32;
    var param_41: vec3<f32>;

    let _e47 = in_bone_weights_1[0u];
    let _e48 = (*recordIndex_5);
    param_30 = _e48;
    let _e50 = in_bone_indices_1[0u];
    param_31 = _e50;
    let _e51 = in_position_1;
    param_32 = _e51;
    let _e52 = transformPreviousPosition_u0028_u1_u003b_u1_u003b_vf3_u003b((&param_30), (&param_31), (&param_32));
    value_1 = (_e52 * _e47);
    let _e55 = in_bone_weights_1[1u];
    if (_e55 > 0f) {
        let _e58 = in_bone_weights_1[1u];
        let _e59 = (*recordIndex_5);
        param_33 = _e59;
        let _e61 = in_bone_indices_1[1u];
        param_34 = _e61;
        let _e62 = in_position_1;
        param_35 = _e62;
        let _e63 = transformPreviousPosition_u0028_u1_u003b_u1_u003b_vf3_u003b((&param_33), (&param_34), (&param_35));
        let _e65 = value_1;
        value_1 = (_e65 + (_e63 * _e58));
    }
    let _e68 = in_bone_weights_1[2u];
    if (_e68 > 0f) {
        let _e71 = in_bone_weights_1[2u];
        let _e72 = (*recordIndex_5);
        param_36 = _e72;
        let _e74 = in_bone_indices_1[2u];
        param_37 = _e74;
        let _e75 = in_position_1;
        param_38 = _e75;
        let _e76 = transformPreviousPosition_u0028_u1_u003b_u1_u003b_vf3_u003b((&param_36), (&param_37), (&param_38));
        let _e78 = value_1;
        value_1 = (_e78 + (_e76 * _e71));
    }
    let _e81 = in_bone_weights_1[3u];
    if (_e81 > 0f) {
        let _e84 = in_bone_weights_1[3u];
        let _e85 = (*recordIndex_5);
        param_39 = _e85;
        let _e87 = in_bone_indices_1[3u];
        param_40 = _e87;
        let _e88 = in_position_1;
        param_41 = _e88;
        let _e89 = transformPreviousPosition_u0028_u1_u003b_u1_u003b_vf3_u003b((&param_39), (&param_40), (&param_41));
        let _e91 = value_1;
        value_1 = (_e91 + (_e89 * _e84));
    }
    let _e93 = value_1;
    return _e93;
}

fn transformCurrentPosition_u0028_u1_u003b_u1_u003b_vf3_u003b(recordIndex_6: ptr<function, u32>, index_4: ptr<function, u32>, position_1: ptr<function, vec3<f32>>) -> vec3<f32> {
    var param_42: u32;
    var param_43: u32;
    var param_44: u32;
    var param_45: u32;
    var param_46: u32;
    var param_47: u32;
    var param_48: u32;
    var param_49: u32;
    var param_50: u32;

    let _e44 = (*recordIndex_6);
    param_42 = _e44;
    let _e45 = (*index_4);
    param_43 = _e45;
    param_44 = 0u;
    let _e46 = currentBoneRow_u0028_u1_u003b_u1_u003b_u1_u003b((&param_42), (&param_43), (&param_44));
    let _e47 = (*position_1);
    let _e53 = (*recordIndex_6);
    param_45 = _e53;
    let _e54 = (*index_4);
    param_46 = _e54;
    param_47 = 1u;
    let _e55 = currentBoneRow_u0028_u1_u003b_u1_u003b_u1_u003b((&param_45), (&param_46), (&param_47));
    let _e56 = (*position_1);
    let _e62 = (*recordIndex_6);
    param_48 = _e62;
    let _e63 = (*index_4);
    param_49 = _e63;
    param_50 = 2u;
    let _e64 = currentBoneRow_u0028_u1_u003b_u1_u003b_u1_u003b((&param_48), (&param_49), (&param_50));
    let _e65 = (*position_1);
    return vec3<f32>(dot(_e46, vec4<f32>(_e47.x, _e47.y, _e47.z, 1f)), dot(_e55, vec4<f32>(_e56.x, _e56.y, _e56.z, 1f)), dot(_e64, vec4<f32>(_e65.x, _e65.y, _e65.z, 1f)));
}

fn skinCurrentPosition_u0028_u1_u003b(recordIndex_7: ptr<function, u32>) -> vec3<f32> {
    var value_2: vec3<f32>;
    var param_51: u32;
    var param_52: u32;
    var param_53: vec3<f32>;
    var param_54: u32;
    var param_55: u32;
    var param_56: vec3<f32>;
    var param_57: u32;
    var param_58: u32;
    var param_59: vec3<f32>;
    var param_60: u32;
    var param_61: u32;
    var param_62: vec3<f32>;

    let _e47 = in_bone_weights_1[0u];
    let _e48 = (*recordIndex_7);
    param_51 = _e48;
    let _e50 = in_bone_indices_1[0u];
    param_52 = _e50;
    let _e51 = in_position_1;
    param_53 = _e51;
    let _e52 = transformCurrentPosition_u0028_u1_u003b_u1_u003b_vf3_u003b((&param_51), (&param_52), (&param_53));
    value_2 = (_e52 * _e47);
    let _e55 = in_bone_weights_1[1u];
    if (_e55 > 0f) {
        let _e58 = in_bone_weights_1[1u];
        let _e59 = (*recordIndex_7);
        param_54 = _e59;
        let _e61 = in_bone_indices_1[1u];
        param_55 = _e61;
        let _e62 = in_position_1;
        param_56 = _e62;
        let _e63 = transformCurrentPosition_u0028_u1_u003b_u1_u003b_vf3_u003b((&param_54), (&param_55), (&param_56));
        let _e65 = value_2;
        value_2 = (_e65 + (_e63 * _e58));
    }
    let _e68 = in_bone_weights_1[2u];
    if (_e68 > 0f) {
        let _e71 = in_bone_weights_1[2u];
        let _e72 = (*recordIndex_7);
        param_57 = _e72;
        let _e74 = in_bone_indices_1[2u];
        param_58 = _e74;
        let _e75 = in_position_1;
        param_59 = _e75;
        let _e76 = transformCurrentPosition_u0028_u1_u003b_u1_u003b_vf3_u003b((&param_57), (&param_58), (&param_59));
        let _e78 = value_2;
        value_2 = (_e78 + (_e76 * _e71));
    }
    let _e81 = in_bone_weights_1[3u];
    if (_e81 > 0f) {
        let _e84 = in_bone_weights_1[3u];
        let _e85 = (*recordIndex_7);
        param_60 = _e85;
        let _e87 = in_bone_indices_1[3u];
        param_61 = _e87;
        let _e88 = in_position_1;
        param_62 = _e88;
        let _e89 = transformCurrentPosition_u0028_u1_u003b_u1_u003b_vf3_u003b((&param_60), (&param_61), (&param_62));
        let _e91 = value_2;
        value_2 = (_e91 + (_e89 * _e84));
    }
    let _e93 = value_2;
    return _e93;
}

fn main_1() {
    var recordIndex_8: u32;
    var currentPosition: vec3<f32>;
    var param_63: u32;
    var previousPosition: vec3<f32>;
    var param_64: u32;
    var currentNormal: vec3<f32>;
    var param_65: u32;
    var param_66: vec3<f32>;
    var currentTangent: vec3<f32>;
    var param_67: u32;
    var param_68: vec3<f32>;

    let _e43 = gl_InstanceIndex_1;
    recordIndex_8 = bitcast<u32>(_e43);
    let _e45 = recordIndex_8;
    if (_e45 >= 256u) {
        unnamed.gl_Position = vec4<f32>(0f, 0f, -2f, 1f);
        frag_tex_coord = vec2<f32>(0f, 0f);
        frag_normal = vec3<f32>(0f, 0f, 0f);
        frag_tangent = vec4<f32>(0f, 0f, 0f, 0f);
        temporalCurrentClip = vec4<f32>(0f, 0f, 0f, 0f);
        temporalPreviousClip = vec4<f32>(0f, 0f, 0f, 0f);
        return;
    }
    let _e48 = recordIndex_8;
    param_63 = _e48;
    let _e49 = skinCurrentPosition_u0028_u1_u003b((&param_63));
    currentPosition = _e49;
    let _e50 = recordIndex_8;
    param_64 = _e50;
    let _e51 = skinPreviousPosition_u0028_u1_u003b((&param_64));
    previousPosition = _e51;
    let _e52 = recordIndex_8;
    param_65 = _e52;
    let _e53 = in_normal_1;
    param_66 = _e53;
    let _e54 = skinDirection_u0028_u1_u003b_vf3_u003b((&param_65), (&param_66));
    currentNormal = _e54;
    let _e55 = recordIndex_8;
    param_67 = _e55;
    let _e56 = in_tangent_1;
    param_68 = _e56.xyz;
    let _e58 = skinDirection_u0028_u1_u003b_vf3_u003b((&param_67), (&param_68));
    currentTangent = _e58;
    let _e59 = recordIndex_8;
    let _e63 = temporalIqmPayload.records[_e59].rasterMvp;
    let _e64 = currentPosition;
    unnamed.gl_Position = (_e63 * vec4<f32>(_e64.x, _e64.y, _e64.z, 1f));
    let _e71 = recordIndex_8;
    let _e75 = temporalIqmPayload.records[_e71].temporalCurrentMvp;
    let _e76 = currentPosition;
    temporalCurrentClip = (_e75 * vec4<f32>(_e76.x, _e76.y, _e76.z, 1f));
    let _e82 = recordIndex_8;
    let _e86 = temporalIqmPayload.records[_e82].temporalPreviousMvp;
    let _e87 = previousPosition;
    temporalPreviousClip = (_e86 * vec4<f32>(_e87.x, _e87.y, _e87.z, 1f));
    let _e93 = in_tex_coord_1;
    frag_tex_coord = _e93;
    let _e94 = currentNormal;
    frag_normal = normalize(_e94);
    let _e96 = currentTangent;
    let _e97 = normalize(_e96);
    let _e99 = in_tangent_1[3u];
    frag_tangent = vec4<f32>(_e97.x, _e97.y, _e97.z, _e99);
    return;
}

@vertex
fn main(@location(4) in_bone_weights: vec4<f32>, @location(5) in_bone_indices: vec4<u32>, @location(0) in_position: vec3<f32>, @builtin(instance_index) gl_InstanceIndex: u32, @location(1) in_normal: vec3<f32>, @location(3) in_tangent: vec4<f32>, @location(2) in_tex_coord: vec2<f32>) -> VertexOutput {
    in_bone_weights_1 = in_bone_weights;
    in_bone_indices_1 = in_bone_indices;
    in_position_1 = in_position;
    gl_InstanceIndex_1 = i32(gl_InstanceIndex);
    in_normal_1 = in_normal;
    in_tangent_1 = in_tangent;
    in_tex_coord_1 = in_tex_coord;
    main_1();
    let _e23 = unnamed.gl_Position.y;
    unnamed.gl_Position.y = -(_e23);
    let _e25 = unnamed.gl_Position;
    let _e26 = frag_tex_coord;
    let _e27 = frag_normal;
    let _e28 = frag_tangent;
    let _e29 = temporalCurrentClip;
    let _e30 = temporalPreviousClip;
    return VertexOutput(_e25, _e26, _e27, _e28, _e29, _e30);
}
