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
var<private> frag_tex_coord0_: vec2<f32>;
var<private> in_tex_coord0_1: vec2<f32>;
var<private> fog_tex_coord: vec2<f32>;

fn main_1() {
    var mvp: mat4x4<f32>;
    var local: mat4x4<f32>;
    var s: f32;
    var t: f32;

    if override_type_5_ {
        let _e29 = gl_InstanceIndex_1;
        let _e33 = unnamed.entMat[(_e29 * 2i)];
        local = _e33;
    } else {
        let _e35 = ubo.mvp;
        local = _e35;
    }
    let _e36 = local;
    mvp = _e36;
    let _e37 = mvp;
    let _e38 = in_position_1;
    unnamed_1.gl_Position = (_e37 * vec4<f32>(_e38.x, _e38.y, _e38.z, 1f));
    let _e45 = in_tex_coord0_1;
    frag_tex_coord0_ = _e45;
    let _e46 = in_position_1;
    let _e48 = ubo.fogDistanceVector;
    let _e53 = ubo.fogDistanceVector[3u];
    s = (dot(_e46, _e48.xyz) + _e53);
    let _e55 = in_position_1;
    let _e57 = ubo.fogDepthVector;
    let _e62 = ubo.fogDepthVector[3u];
    t = (dot(_e55, _e57.xyz) + _e62);
    let _e66 = ubo.fogEyeT[1u];
    if (_e66 == 1f) {
        let _e68 = t;
        if (_e68 < 0f) {
            t = 0.03125f;
        } else {
            t = 0.96875f;
        }
    } else {
        let _e70 = t;
        if (_e70 < 1f) {
            t = 0.03125f;
        } else {
            let _e72 = t;
            let _e74 = t;
            let _e77 = ubo.fogEyeT[0u];
            t = (0.03125f + ((0.9375f * _e72) / (_e74 - _e77)));
        }
    }
    let _e81 = s;
    let _e82 = t;
    fog_tex_coord = vec2<f32>(_e81, _e82);
    return;
}

@vertex 
fn main(@builtin(instance_index) gl_InstanceIndex: u32, @location(0) in_position: vec3<f32>, @location(2) in_tex_coord0_: vec2<f32>) -> VertexOutput {
    gl_InstanceIndex_1 = i32(gl_InstanceIndex);
    in_position_1 = in_position;
    in_tex_coord0_1 = in_tex_coord0_;
    main_1();
    let _e12 = unnamed_1.gl_Position.y;
    unnamed_1.gl_Position.y = -(_e12);
    let _e14 = unnamed_1.gl_Position;
    let _e15 = frag_tex_coord0_;
    let _e16 = fog_tex_coord;
    return VertexOutput(_e14, _e15, _e16);
}
