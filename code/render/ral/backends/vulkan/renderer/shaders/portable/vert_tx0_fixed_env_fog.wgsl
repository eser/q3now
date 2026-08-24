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
    @location(4) member_1: vec2<f32>,
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
        let _e34 = gl_InstanceIndex_1;
        let _e38 = unnamed.entMat[(_e34 * 2i)];
        local = _e38;
    } else {
        let _e40 = ubo.mvp;
        local = _e40;
    }
    let _e41 = local;
    mvp = _e41;
    let _e42 = mvp;
    let _e43 = in_position_1;
    unnamed_1.gl_Position = (_e42 * vec4<f32>(_e43.x, _e43.y, _e43.z, 1f));
    let _e51 = ubo.eyePos;
    let _e53 = in_position_1;
    viewer = normalize((_e51.xyz - _e53));
    let _e56 = in_normal_1;
    let _e57 = viewer;
    d = dot(_e56, _e57);
    let _e59 = in_normal_1;
    let _e62 = d;
    let _e64 = viewer;
    reflected = (((_e59.yz * 2f) * _e62) - _e64.yz);
    let _e68 = reflected[0u];
    frag_tex_coord0_[0u] = (0.5f + (_e68 * 0.5f));
    let _e73 = reflected[1u];
    frag_tex_coord0_[1u] = (0.5f - (_e73 * 0.5f));
    let _e77 = in_position_1;
    let _e79 = ubo.fogDistanceVector;
    let _e84 = ubo.fogDistanceVector[3u];
    s = (dot(_e77, _e79.xyz) + _e84);
    let _e86 = in_position_1;
    let _e88 = ubo.fogDepthVector;
    let _e93 = ubo.fogDepthVector[3u];
    t = (dot(_e86, _e88.xyz) + _e93);
    let _e97 = ubo.fogEyeT[1u];
    if (_e97 == 1f) {
        let _e99 = t;
        if (_e99 < 0f) {
            t = 0.03125f;
        } else {
            t = 0.96875f;
        }
    } else {
        let _e101 = t;
        if (_e101 < 1f) {
            t = 0.03125f;
        } else {
            let _e103 = t;
            let _e105 = t;
            let _e108 = ubo.fogEyeT[0u];
            t = (0.03125f + ((0.9375f * _e103) / (_e105 - _e108)));
        }
    }
    let _e112 = s;
    let _e113 = t;
    fog_tex_coord = vec2<f32>(_e112, _e113);
    return;
}

@vertex
fn main(@builtin(instance_index) gl_InstanceIndex: u32, @location(0) in_position: vec3<f32>, @location(5) in_normal: vec3<f32>) -> VertexOutput {
    gl_InstanceIndex_1 = i32(gl_InstanceIndex);
    in_position_1 = in_position;
    in_normal_1 = in_normal;
    main_1();
    let _e12 = unnamed_1.gl_Position.y;
    unnamed_1.gl_Position.y = -(_e12);
    let _e14 = unnamed_1.gl_Position;
    let _e15 = frag_tex_coord0_;
    let _e16 = fog_tex_coord;
    return VertexOutput(_e14, _e15, _e16);
}
