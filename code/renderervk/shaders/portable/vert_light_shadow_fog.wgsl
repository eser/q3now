struct EntityMatrices {
    entMat: array<mat4x4<f32>>,
}

struct UBO {
    eyePos: vec4<f32>,
    lightPos: vec4<f32>,
    lightColor: vec4<f32>,
    lightVector: vec4<f32>,
    fogDistanceVector: vec4<f32>,
    fogDepthVector: vec4<f32>,
    fogEyeT: vec4<f32>,
    fogColor: vec4<f32>,
    _pad_to_modelMatrix: array<vec4<f32>, 18>,
    modelMatrix: mat4x4<f32>,
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
    @location(3) member_3: vec4<f32>,
    @location(6) member_4: vec4<f32>,
    @location(4) member_5: vec2<f32>,
}

@id(16) override ENTITY_SSBO: i32 = 0i;
override override_type_5_: bool = (ENTITY_SSBO != 0i);
override override_type_5_1: bool = (ENTITY_SSBO != 0i);

@group(3) @binding(0) 
var<storage> unnamed: EntityMatrices;
var<private> gl_InstanceIndex_1: i32;
@group(0) @binding(0) 
var<uniform> ubo: UBO;
var<private> unnamed_1: gl_PerVertex = gl_PerVertex(vec4<f32>(0f, 0f, 0f, 1f));
var<private> in_position_1: vec3<f32>;
var<private> frag_tex_coord: vec2<f32>;
var<private> in_tex_coord_1: vec2<f32>;
var<private> N: vec3<f32>;
var<private> in_normal_1: vec3<f32>;
var<private> L: vec4<f32>;
var<private> V: vec4<f32>;
var<private> shadowData: vec4<f32>;
var<private> fog_tex_coord: vec2<f32>;

fn main_1() {
    var mvp: mat4x4<f32>;
    var local: mat4x4<f32>;
    var modelMatrix: mat4x4<f32>;
    var local_1: mat4x4<f32>;
    var s: f32;
    var t: f32;

    if override_type_5_ {
        let _e39 = gl_InstanceIndex_1;
        let _e43 = unnamed.entMat[(_e39 * 2i)];
        local = _e43;
    } else {
        let _e45 = ubo.mvp;
        local = _e45;
    }
    let _e46 = local;
    mvp = _e46;
    let _e47 = mvp;
    let _e48 = in_position_1;
    unnamed_1.gl_Position = (_e47 * vec4<f32>(_e48.x, _e48.y, _e48.z, 1f));
    let _e55 = in_tex_coord_1;
    frag_tex_coord = _e55;
    let _e56 = in_normal_1;
    N = _e56;
    let _e58 = ubo.lightPos;
    let _e59 = in_position_1;
    L = (_e58 - vec4<f32>(_e59.x, _e59.y, _e59.z, 1f));
    let _e66 = ubo.eyePos;
    let _e67 = in_position_1;
    V = (_e66 - vec4<f32>(_e67.x, _e67.y, _e67.z, 1f));
    if override_type_5_1 {
        let _e73 = gl_InstanceIndex_1;
        let _e78 = unnamed.entMat[((_e73 * 2i) + 1i)];
        local_1 = _e78;
    } else {
        let _e80 = ubo.modelMatrix;
        local_1 = _e80;
    }
    let _e81 = local_1;
    modelMatrix = _e81;
    let _e82 = modelMatrix;
    let _e83 = in_position_1;
    let _e89 = (_e82 * vec4<f32>(_e83.x, _e83.y, _e83.z, 1f)).xyz;
    let _e92 = unnamed_1.gl_Position[3u];
    shadowData = vec4<f32>(_e89.x, _e89.y, _e89.z, _e92);
    let _e97 = in_position_1;
    let _e99 = ubo.fogDistanceVector;
    let _e104 = ubo.fogDistanceVector[3u];
    s = (dot(_e97, _e99.xyz) + _e104);
    let _e106 = in_position_1;
    let _e108 = ubo.fogDepthVector;
    let _e113 = ubo.fogDepthVector[3u];
    t = (dot(_e106, _e108.xyz) + _e113);
    let _e117 = ubo.fogEyeT[1u];
    if (_e117 == 1f) {
        let _e119 = t;
        if (_e119 < 0f) {
            t = 0.03125f;
        } else {
            t = 0.96875f;
        }
    } else {
        let _e121 = t;
        if (_e121 < 1f) {
            t = 0.03125f;
        } else {
            let _e123 = t;
            let _e125 = t;
            let _e128 = ubo.fogEyeT[0u];
            t = (0.03125f + ((0.9375f * _e123) / (_e125 - _e128)));
        }
    }
    let _e132 = s;
    let _e133 = t;
    fog_tex_coord = vec2<f32>(_e132, _e133);
    return;
}

@vertex 
fn main(@builtin(instance_index) gl_InstanceIndex: u32, @location(0) in_position: vec3<f32>, @location(1) in_tex_coord: vec2<f32>, @location(2) in_normal: vec3<f32>) -> VertexOutput {
    gl_InstanceIndex_1 = i32(gl_InstanceIndex);
    in_position_1 = in_position;
    in_tex_coord_1 = in_tex_coord;
    in_normal_1 = in_normal;
    main_1();
    let _e18 = unnamed_1.gl_Position.y;
    unnamed_1.gl_Position.y = -(_e18);
    let _e20 = unnamed_1.gl_Position;
    let _e21 = frag_tex_coord;
    let _e22 = N;
    let _e23 = L;
    let _e24 = V;
    let _e25 = shadowData;
    let _e26 = fog_tex_coord;
    return VertexOutput(_e20, _e21, _e22, _e23, _e24, _e25, _e26);
}
