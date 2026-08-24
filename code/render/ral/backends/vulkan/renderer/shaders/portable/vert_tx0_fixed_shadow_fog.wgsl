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
    @location(7) member: vec4<f32>,
    @location(1) member_1: vec2<f32>,
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
var<private> shadowData: vec4<f32>;
var<private> frag_tex_coord0_: vec2<f32>;
var<private> in_tex_coord0_1: vec2<f32>;
var<private> fog_tex_coord: vec2<f32>;

fn main_1() {
    var mvp: mat4x4<f32>;
    var local: mat4x4<f32>;
    var s: f32;
    var t: f32;

    if override_type_5_ {
        let _e30 = gl_InstanceIndex_1;
        let _e34 = unnamed.entMat[(_e30 * 2i)];
        local = _e34;
    } else {
        let _e36 = ubo.mvp;
        local = _e36;
    }
    let _e37 = local;
    mvp = _e37;
    let _e38 = mvp;
    let _e39 = in_position_1;
    unnamed_1.gl_Position = (_e38 * vec4<f32>(_e39.x, _e39.y, _e39.z, 1f));
    let _e46 = in_position_1;
    let _e49 = unnamed_1.gl_Position[3u];
    shadowData = vec4<f32>(_e46.x, _e46.y, _e46.z, _e49);
    let _e54 = in_tex_coord0_1;
    frag_tex_coord0_ = _e54;
    let _e55 = in_position_1;
    let _e57 = ubo.fogDistanceVector;
    let _e62 = ubo.fogDistanceVector[3u];
    s = (dot(_e55, _e57.xyz) + _e62);
    let _e64 = in_position_1;
    let _e66 = ubo.fogDepthVector;
    let _e71 = ubo.fogDepthVector[3u];
    t = (dot(_e64, _e66.xyz) + _e71);
    let _e75 = ubo.fogEyeT[1u];
    if (_e75 == 1f) {
        let _e77 = t;
        if (_e77 < 0f) {
            t = 0.03125f;
        } else {
            t = 0.96875f;
        }
    } else {
        let _e79 = t;
        if (_e79 < 1f) {
            t = 0.03125f;
        } else {
            let _e81 = t;
            let _e83 = t;
            let _e86 = ubo.fogEyeT[0u];
            t = (0.03125f + ((0.9375f * _e81) / (_e83 - _e86)));
        }
    }
    let _e90 = s;
    let _e91 = t;
    fog_tex_coord = vec2<f32>(_e90, _e91);
    return;
}

@vertex
fn main(@builtin(instance_index) gl_InstanceIndex: u32, @location(0) in_position: vec3<f32>, @location(2) in_tex_coord0_: vec2<f32>) -> VertexOutput {
    gl_InstanceIndex_1 = i32(gl_InstanceIndex);
    in_position_1 = in_position;
    in_tex_coord0_1 = in_tex_coord0_;
    main_1();
    let _e13 = unnamed_1.gl_Position.y;
    unnamed_1.gl_Position.y = -(_e13);
    let _e15 = unnamed_1.gl_Position;
    let _e16 = shadowData;
    let _e17 = frag_tex_coord0_;
    let _e18 = fog_tex_coord;
    return VertexOutput(_e15, _e16, _e17, _e18);
}
