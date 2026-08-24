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
    _pad_to_mvp: array<vec4<f32>, 22>,
    mvp: mat4x4<f32>,
}

struct gl_PerVertex {
    @builtin(position) gl_Position: vec4<f32>,
}

struct VertexOutput {
    @builtin(position) gl_Position: vec4<f32>,
    @location(1) member: vec2<f32>,
    @location(2) member_1: vec2<f32>,
    @location(4) member_2: vec2<f32>,
}

@id(16) override ENTITY_SSBO: i32 = 0i;
override override_type_5_: bool = (ENTITY_SSBO != 0i);

@group(3) @binding(0)
var<storage> unnamed: EntityMatrices;
var<private> gl_InstanceIndex_1: i32;
@group(0) @binding(0)
var<uniform> ubo: UBO;
var<private> unnamed_1: gl_PerVertex = gl_PerVertex(vec4<f32>(0f, 0f, 0f, 1f));
var<private> in_position_1: vec3<f32>;
var<private> in_normal_1: vec3<f32>;
var<private> frag_tex_coord0_: vec2<f32>;
var<private> frag_tex_coord1_: vec2<f32>;
var<private> in_tex_coord1_1: vec2<f32>;
var<private> fog_tex_coord: vec2<f32>;

fn main_1() {
    var mvp: mat4x4<f32>;
    var local: mat4x4<f32>;
    var viewer: vec3<f32>;
    var d: f32;
    var reflected: vec2<f32>;
    var s: f32;
    var t: f32;

    if override_type_5_ {
        let _e36 = gl_InstanceIndex_1;
        let _e40 = unnamed.entMat[(_e36 * 2i)];
        local = _e40;
    } else {
        let _e42 = ubo.mvp;
        local = _e42;
    }
    let _e43 = local;
    mvp = _e43;
    let _e44 = mvp;
    let _e45 = in_position_1;
    unnamed_1.gl_Position = (_e44 * vec4<f32>(_e45.x, _e45.y, _e45.z, 1f));
    let _e53 = ubo.eyePos;
    let _e55 = in_position_1;
    viewer = normalize((_e53.xyz - _e55));
    let _e58 = in_normal_1;
    let _e59 = viewer;
    d = dot(_e58, _e59);
    let _e61 = in_normal_1;
    let _e64 = d;
    let _e66 = viewer;
    reflected = (((_e61.yz * 2f) * _e64) - _e66.yz);
    let _e70 = reflected[0u];
    frag_tex_coord0_[0u] = (0.5f + (_e70 * 0.5f));
    let _e75 = reflected[1u];
    frag_tex_coord0_[1u] = (0.5f - (_e75 * 0.5f));
    let _e79 = in_tex_coord1_1;
    frag_tex_coord1_ = _e79;
    let _e80 = in_position_1;
    let _e82 = ubo.fogDistanceVector;
    let _e87 = ubo.fogDistanceVector[3u];
    s = (dot(_e80, _e82.xyz) + _e87);
    let _e89 = in_position_1;
    let _e91 = ubo.fogDepthVector;
    let _e96 = ubo.fogDepthVector[3u];
    t = (dot(_e89, _e91.xyz) + _e96);
    let _e100 = ubo.fogEyeT[1u];
    if (_e100 == 1f) {
        let _e102 = t;
        if (_e102 < 0f) {
            t = 0.03125f;
        } else {
            t = 0.96875f;
        }
    } else {
        let _e104 = t;
        if (_e104 < 1f) {
            t = 0.03125f;
        } else {
            let _e106 = t;
            let _e108 = t;
            let _e111 = ubo.fogEyeT[0u];
            t = (0.03125f + ((0.9375f * _e106) / (_e108 - _e111)));
        }
    }
    let _e115 = s;
    let _e116 = t;
    fog_tex_coord = vec2<f32>(_e115, _e116);
    return;
}

@vertex
fn main(@builtin(instance_index) gl_InstanceIndex: u32, @location(0) in_position: vec3<f32>, @location(5) in_normal: vec3<f32>, @location(3) in_tex_coord1_: vec2<f32>) -> VertexOutput {
    gl_InstanceIndex_1 = i32(gl_InstanceIndex);
    in_position_1 = in_position;
    in_normal_1 = in_normal;
    in_tex_coord1_1 = in_tex_coord1_;
    main_1();
    let _e15 = unnamed_1.gl_Position.y;
    unnamed_1.gl_Position.y = -(_e15);
    let _e17 = unnamed_1.gl_Position;
    let _e18 = frag_tex_coord0_;
    let _e19 = frag_tex_coord1_;
    let _e20 = fog_tex_coord;
    return VertexOutput(_e17, _e18, _e19, _e20);
}
