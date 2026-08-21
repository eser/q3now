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
var<private> frag_tex_coord0_: vec2<f32>;
var<private> in_tex_coord0_1: vec2<f32>;
var<private> frag_tex_coord1_: vec2<f32>;
var<private> in_tex_coord1_1: vec2<f32>;
var<private> fog_tex_coord: vec2<f32>;

fn main_1() {
    var mvp: mat4x4<f32>;
    var local: mat4x4<f32>;
    var s: f32;
    var t: f32;

    if override_type_5_ {
        let _e31 = gl_InstanceIndex_1;
        let _e35 = unnamed.entMat[(_e31 * 2i)];
        local = _e35;
    } else {
        let _e37 = ubo.mvp;
        local = _e37;
    }
    let _e38 = local;
    mvp = _e38;
    let _e39 = mvp;
    let _e40 = in_position_1;
    unnamed_1.gl_Position = (_e39 * vec4<f32>(_e40.x, _e40.y, _e40.z, 1f));
    let _e47 = in_tex_coord0_1;
    frag_tex_coord0_ = _e47;
    let _e48 = in_tex_coord1_1;
    frag_tex_coord1_ = _e48;
    let _e49 = in_position_1;
    let _e51 = ubo.fogDistanceVector;
    let _e56 = ubo.fogDistanceVector[3u];
    s = (dot(_e49, _e51.xyz) + _e56);
    let _e58 = in_position_1;
    let _e60 = ubo.fogDepthVector;
    let _e65 = ubo.fogDepthVector[3u];
    t = (dot(_e58, _e60.xyz) + _e65);
    let _e69 = ubo.fogEyeT[1u];
    if (_e69 == 1f) {
        let _e71 = t;
        if (_e71 < 0f) {
            t = 0.03125f;
        } else {
            t = 0.96875f;
        }
    } else {
        let _e73 = t;
        if (_e73 < 1f) {
            t = 0.03125f;
        } else {
            let _e75 = t;
            let _e77 = t;
            let _e80 = ubo.fogEyeT[0u];
            t = (0.03125f + ((0.9375f * _e75) / (_e77 - _e80)));
        }
    }
    let _e84 = s;
    let _e85 = t;
    fog_tex_coord = vec2<f32>(_e84, _e85);
    return;
}

@vertex 
fn main(@builtin(instance_index) gl_InstanceIndex: u32, @location(0) in_position: vec3<f32>, @location(2) in_tex_coord0_: vec2<f32>, @location(3) in_tex_coord1_: vec2<f32>) -> VertexOutput {
    gl_InstanceIndex_1 = i32(gl_InstanceIndex);
    in_position_1 = in_position;
    in_tex_coord0_1 = in_tex_coord0_;
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
