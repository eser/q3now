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
    @location(8) member: vec3<f32>,
    @location(9) member_1: vec3<f32>,
    @location(0) member_2: vec4<f32>,
    @location(1) member_3: vec2<f32>,
    @location(2) member_4: vec2<f32>,
    @location(4) member_5: vec2<f32>,
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
var<private> ibl_N: vec3<f32>;
var<private> in_normal_1: vec3<f32>;
var<private> ibl_V: vec3<f32>;
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
    let _e52 = in_normal_1;
    ibl_N = _e52;
    let _e54 = ubo.eyePos;
    let _e56 = in_position_1;
    ibl_V = normalize((_e54.xyz - _e56));
    let _e59 = in_color0_1;
    frag_color0_ = _e59;
    let _e60 = in_tex_coord0_1;
    frag_tex_coord0_ = _e60;
    let _e61 = in_tex_coord1_1;
    frag_tex_coord1_ = _e61;
    let _e62 = in_position_1;
    let _e64 = ubo.fogDistanceVector;
    let _e69 = ubo.fogDistanceVector[3u];
    s = (dot(_e62, _e64.xyz) + _e69);
    let _e71 = in_position_1;
    let _e73 = ubo.fogDepthVector;
    let _e78 = ubo.fogDepthVector[3u];
    t = (dot(_e71, _e73.xyz) + _e78);
    let _e82 = ubo.fogEyeT[1u];
    if (_e82 == 1f) {
        let _e84 = t;
        if (_e84 < 0f) {
            t = 0.03125f;
        } else {
            t = 0.96875f;
        }
    } else {
        let _e86 = t;
        if (_e86 < 1f) {
            t = 0.03125f;
        } else {
            let _e88 = t;
            let _e90 = t;
            let _e93 = ubo.fogEyeT[0u];
            t = (0.03125f + ((0.9375f * _e88) / (_e90 - _e93)));
        }
    }
    let _e97 = s;
    let _e98 = t;
    fog_tex_coord = vec2<f32>(_e97, _e98);
    return;
}

@vertex 
fn main(@builtin(instance_index) gl_InstanceIndex: u32, @location(0) in_position: vec3<f32>, @location(5) in_normal: vec3<f32>, @location(1) in_color0_: vec4<f32>, @location(2) in_tex_coord0_: vec2<f32>, @location(3) in_tex_coord1_: vec2<f32>) -> VertexOutput {
    gl_InstanceIndex_1 = i32(gl_InstanceIndex);
    in_position_1 = in_position;
    in_normal_1 = in_normal;
    in_color0_1 = in_color0_;
    in_tex_coord0_1 = in_tex_coord0_;
    in_tex_coord1_1 = in_tex_coord1_;
    main_1();
    let _e22 = unnamed_1.gl_Position.y;
    unnamed_1.gl_Position.y = -(_e22);
    let _e24 = unnamed_1.gl_Position;
    let _e25 = ibl_N;
    let _e26 = ibl_V;
    let _e27 = frag_color0_;
    let _e28 = frag_tex_coord0_;
    let _e29 = frag_tex_coord1_;
    let _e30 = fog_tex_coord;
    return VertexOutput(_e24, _e25, _e26, _e27, _e28, _e29, _e30);
}
