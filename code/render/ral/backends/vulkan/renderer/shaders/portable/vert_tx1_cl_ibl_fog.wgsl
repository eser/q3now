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
    @location(5) member_3: vec4<f32>,
    @location(1) member_4: vec2<f32>,
    @location(2) member_5: vec2<f32>,
    @location(4) member_6: vec2<f32>,
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
        let _e38 = gl_InstanceIndex_1;
        let _e42 = unnamed.entMat[(_e38 * 2i)];
        local = _e42;
    } else {
        let _e44 = ubo.mvp;
        local = _e44;
    }
    let _e45 = local;
    mvp = _e45;
    let _e46 = mvp;
    let _e47 = in_position_1;
    unnamed_1.gl_Position = (_e46 * vec4<f32>(_e47.x, _e47.y, _e47.z, 1f));
    let _e54 = in_normal_1;
    ibl_N = _e54;
    let _e56 = ubo.eyePos;
    let _e58 = in_position_1;
    ibl_V = normalize((_e56.xyz - _e58));
    let _e61 = in_color0_1;
    frag_color0_ = _e61;
    let _e62 = in_color1_1;
    frag_color1_ = _e62;
    let _e63 = in_tex_coord0_1;
    frag_tex_coord0_ = _e63;
    let _e64 = in_tex_coord1_1;
    frag_tex_coord1_ = _e64;
    let _e65 = in_position_1;
    let _e67 = ubo.fogDistanceVector;
    let _e72 = ubo.fogDistanceVector[3u];
    s = (dot(_e65, _e67.xyz) + _e72);
    let _e74 = in_position_1;
    let _e76 = ubo.fogDepthVector;
    let _e81 = ubo.fogDepthVector[3u];
    t = (dot(_e74, _e76.xyz) + _e81);
    let _e85 = ubo.fogEyeT[1u];
    if (_e85 == 1f) {
        let _e87 = t;
        if (_e87 < 0f) {
            t = 0.03125f;
        } else {
            t = 0.96875f;
        }
    } else {
        let _e89 = t;
        if (_e89 < 1f) {
            t = 0.03125f;
        } else {
            let _e91 = t;
            let _e93 = t;
            let _e96 = ubo.fogEyeT[0u];
            t = (0.03125f + ((0.9375f * _e91) / (_e93 - _e96)));
        }
    }
    let _e100 = s;
    let _e101 = t;
    fog_tex_coord = vec2<f32>(_e100, _e101);
    return;
}

@vertex
fn main(@builtin(instance_index) gl_InstanceIndex: u32, @location(0) in_position: vec3<f32>, @location(5) in_normal: vec3<f32>, @location(1) in_color0_: vec4<f32>, @location(6) in_color1_: vec4<f32>, @location(2) in_tex_coord0_: vec2<f32>, @location(3) in_tex_coord1_: vec2<f32>) -> VertexOutput {
    gl_InstanceIndex_1 = i32(gl_InstanceIndex);
    in_position_1 = in_position;
    in_normal_1 = in_normal;
    in_color0_1 = in_color0_;
    in_color1_1 = in_color1_;
    in_tex_coord0_1 = in_tex_coord0_;
    in_tex_coord1_1 = in_tex_coord1_;
    main_1();
    let _e25 = unnamed_1.gl_Position.y;
    unnamed_1.gl_Position.y = -(_e25);
    let _e27 = unnamed_1.gl_Position;
    let _e28 = ibl_N;
    let _e29 = ibl_V;
    let _e30 = frag_color0_;
    let _e31 = frag_color1_;
    let _e32 = frag_tex_coord0_;
    let _e33 = frag_tex_coord1_;
    let _e34 = fog_tex_coord;
    return VertexOutput(_e27, _e28, _e29, _e30, _e31, _e32, _e33, _e34);
}
