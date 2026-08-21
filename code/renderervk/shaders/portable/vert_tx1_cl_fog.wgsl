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
    @location(0) member: vec4<f32>,
    @location(5) member_1: vec4<f32>,
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
var<private> frag_color0_: vec4<f32>;
var<private> in_color0_1: vec4<f32>;
var<private> frag_color1_: vec4<f32>;
var<private> in_color1_1: vec4<f32>;
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
        let _e35 = gl_InstanceIndex_1;
        let _e39 = unnamed.entMat[(_e35 * 2i)];
        local = _e39;
    } else {
        let _e41 = ubo.mvp;
        local = _e41;
    }
    let _e42 = local;
    mvp = _e42;
    let _e43 = mvp;
    let _e44 = in_position_1;
    unnamed_1.gl_Position = (_e43 * vec4<f32>(_e44.x, _e44.y, _e44.z, 1f));
    let _e51 = in_color0_1;
    frag_color0_ = _e51;
    let _e52 = in_color1_1;
    frag_color1_ = _e52;
    let _e53 = in_tex_coord0_1;
    frag_tex_coord0_ = _e53;
    let _e54 = in_tex_coord1_1;
    frag_tex_coord1_ = _e54;
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
fn main(@builtin(instance_index) gl_InstanceIndex: u32, @location(0) in_position: vec3<f32>, @location(1) in_color0_: vec4<f32>, @location(6) in_color1_: vec4<f32>, @location(2) in_tex_coord0_: vec2<f32>, @location(3) in_tex_coord1_: vec2<f32>) -> VertexOutput {
    gl_InstanceIndex_1 = i32(gl_InstanceIndex);
    in_position_1 = in_position;
    in_color0_1 = in_color0_;
    in_color1_1 = in_color1_;
    in_tex_coord0_1 = in_tex_coord0_;
    in_tex_coord1_1 = in_tex_coord1_;
    main_1();
    let _e21 = unnamed_1.gl_Position.y;
    unnamed_1.gl_Position.y = -(_e21);
    let _e23 = unnamed_1.gl_Position;
    let _e24 = frag_color0_;
    let _e25 = frag_color1_;
    let _e26 = frag_tex_coord0_;
    let _e27 = frag_tex_coord1_;
    let _e28 = fog_tex_coord;
    return VertexOutput(_e23, _e24, _e25, _e26, _e27, _e28);
}
