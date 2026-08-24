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
    @location(6) member_2: vec4<f32>,
    @location(1) member_3: vec2<f32>,
    @location(2) member_4: vec2<f32>,
    @location(3) member_5: vec2<f32>,
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
var<private> frag_color0_: vec4<f32>;
var<private> in_color0_1: vec4<f32>;
var<private> frag_color1_: vec4<f32>;
var<private> in_color1_1: vec4<f32>;
var<private> frag_color2_: vec4<f32>;
var<private> in_color2_1: vec4<f32>;
var<private> in_normal_1: vec3<f32>;
var<private> frag_tex_coord0_: vec2<f32>;
var<private> frag_tex_coord1_: vec2<f32>;
var<private> in_tex_coord1_1: vec2<f32>;
var<private> frag_tex_coord2_: vec2<f32>;
var<private> in_tex_coord2_1: vec2<f32>;
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
        let _e44 = gl_InstanceIndex_1;
        let _e48 = unnamed.entMat[(_e44 * 2i)];
        local = _e48;
    } else {
        let _e50 = ubo.mvp;
        local = _e50;
    }
    let _e51 = local;
    mvp = _e51;
    let _e52 = mvp;
    let _e53 = in_position_1;
    unnamed_1.gl_Position = (_e52 * vec4<f32>(_e53.x, _e53.y, _e53.z, 1f));
    let _e60 = in_color0_1;
    frag_color0_ = _e60;
    let _e61 = in_color1_1;
    frag_color1_ = _e61;
    let _e62 = in_color2_1;
    frag_color2_ = _e62;
    let _e64 = ubo.eyePos;
    let _e66 = in_position_1;
    viewer = normalize((_e64.xyz - _e66));
    let _e69 = in_normal_1;
    let _e70 = viewer;
    d = dot(_e69, _e70);
    let _e72 = in_normal_1;
    let _e75 = d;
    let _e77 = viewer;
    reflected = (((_e72.yz * 2f) * _e75) - _e77.yz);
    let _e81 = reflected[0u];
    frag_tex_coord0_[0u] = (0.5f + (_e81 * 0.5f));
    let _e86 = reflected[1u];
    frag_tex_coord0_[1u] = (0.5f - (_e86 * 0.5f));
    let _e90 = in_tex_coord1_1;
    frag_tex_coord1_ = _e90;
    let _e91 = in_tex_coord2_1;
    frag_tex_coord2_ = _e91;
    let _e92 = in_position_1;
    let _e94 = ubo.fogDistanceVector;
    let _e99 = ubo.fogDistanceVector[3u];
    s = (dot(_e92, _e94.xyz) + _e99);
    let _e101 = in_position_1;
    let _e103 = ubo.fogDepthVector;
    let _e108 = ubo.fogDepthVector[3u];
    t = (dot(_e101, _e103.xyz) + _e108);
    let _e112 = ubo.fogEyeT[1u];
    if (_e112 == 1f) {
        let _e114 = t;
        if (_e114 < 0f) {
            t = 0.03125f;
        } else {
            t = 0.96875f;
        }
    } else {
        let _e116 = t;
        if (_e116 < 1f) {
            t = 0.03125f;
        } else {
            let _e118 = t;
            let _e120 = t;
            let _e123 = ubo.fogEyeT[0u];
            t = (0.03125f + ((0.9375f * _e118) / (_e120 - _e123)));
        }
    }
    let _e127 = s;
    let _e128 = t;
    fog_tex_coord = vec2<f32>(_e127, _e128);
    return;
}

@vertex
fn main(@builtin(instance_index) gl_InstanceIndex: u32, @location(0) in_position: vec3<f32>, @location(1) in_color0_: vec4<f32>, @location(6) in_color1_: vec4<f32>, @location(7) in_color2_: vec4<f32>, @location(5) in_normal: vec3<f32>, @location(3) in_tex_coord1_: vec2<f32>, @location(4) in_tex_coord2_: vec2<f32>) -> VertexOutput {
    gl_InstanceIndex_1 = i32(gl_InstanceIndex);
    in_position_1 = in_position;
    in_color0_1 = in_color0_;
    in_color1_1 = in_color1_;
    in_color2_1 = in_color2_;
    in_normal_1 = in_normal;
    in_tex_coord1_1 = in_tex_coord1_;
    in_tex_coord2_1 = in_tex_coord2_;
    main_1();
    let _e27 = unnamed_1.gl_Position.y;
    unnamed_1.gl_Position.y = -(_e27);
    let _e29 = unnamed_1.gl_Position;
    let _e30 = frag_color0_;
    let _e31 = frag_color1_;
    let _e32 = frag_color2_;
    let _e33 = frag_tex_coord0_;
    let _e34 = frag_tex_coord1_;
    let _e35 = frag_tex_coord2_;
    let _e36 = fog_tex_coord;
    return VertexOutput(_e29, _e30, _e31, _e32, _e33, _e34, _e35, _e36);
}
