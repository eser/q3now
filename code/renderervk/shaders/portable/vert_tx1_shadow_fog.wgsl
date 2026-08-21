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
    @location(0) member_1: vec4<f32>,
    @location(1) member_2: vec2<f32>,
    @location(2) member_3: vec2<f32>,
    @location(4) member_4: vec2<f32>,
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
var<private> frag_color0_: vec4<f32>;
var<private> in_color0_1: vec4<f32>;
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
    let _e50 = in_position_1;
    let _e53 = unnamed_1.gl_Position[3u];
    shadowData = vec4<f32>(_e50.x, _e50.y, _e50.z, _e53);
    let _e58 = in_color0_1;
    frag_color0_ = _e58;
    let _e59 = in_tex_coord0_1;
    frag_tex_coord0_ = _e59;
    let _e60 = in_tex_coord1_1;
    frag_tex_coord1_ = _e60;
    let _e61 = in_position_1;
    let _e63 = ubo.fogDistanceVector;
    let _e68 = ubo.fogDistanceVector[3u];
    s = (dot(_e61, _e63.xyz) + _e68);
    let _e70 = in_position_1;
    let _e72 = ubo.fogDepthVector;
    let _e77 = ubo.fogDepthVector[3u];
    t = (dot(_e70, _e72.xyz) + _e77);
    let _e81 = ubo.fogEyeT[1u];
    if (_e81 == 1f) {
        let _e83 = t;
        if (_e83 < 0f) {
            t = 0.03125f;
        } else {
            t = 0.96875f;
        }
    } else {
        let _e85 = t;
        if (_e85 < 1f) {
            t = 0.03125f;
        } else {
            let _e87 = t;
            let _e89 = t;
            let _e92 = ubo.fogEyeT[0u];
            t = (0.03125f + ((0.9375f * _e87) / (_e89 - _e92)));
        }
    }
    let _e96 = s;
    let _e97 = t;
    fog_tex_coord = vec2<f32>(_e96, _e97);
    return;
}

@vertex 
fn main(@builtin(instance_index) gl_InstanceIndex: u32, @location(0) in_position: vec3<f32>, @location(1) in_color0_: vec4<f32>, @location(2) in_tex_coord0_: vec2<f32>, @location(3) in_tex_coord1_: vec2<f32>) -> VertexOutput {
    gl_InstanceIndex_1 = i32(gl_InstanceIndex);
    in_position_1 = in_position;
    in_color0_1 = in_color0_;
    in_tex_coord0_1 = in_tex_coord0_;
    in_tex_coord1_1 = in_tex_coord1_;
    main_1();
    let _e19 = unnamed_1.gl_Position.y;
    unnamed_1.gl_Position.y = -(_e19);
    let _e21 = unnamed_1.gl_Position;
    let _e22 = shadowData;
    let _e23 = frag_color0_;
    let _e24 = frag_tex_coord0_;
    let _e25 = frag_tex_coord1_;
    let _e26 = fog_tex_coord;
    return VertexOutput(_e21, _e22, _e23, _e24, _e25, _e26);
}
