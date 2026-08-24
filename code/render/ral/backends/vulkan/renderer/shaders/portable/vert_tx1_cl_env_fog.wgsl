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
        let _e40 = gl_InstanceIndex_1;
        let _e44 = unnamed.entMat[(_e40 * 2i)];
        local = _e44;
    } else {
        let _e46 = ubo.mvp;
        local = _e46;
    }
    let _e47 = local;
    mvp = _e47;
    let _e48 = mvp;
    let _e49 = in_position_1;
    unnamed_1.gl_Position = (_e48 * vec4<f32>(_e49.x, _e49.y, _e49.z, 1f));
    let _e56 = in_color0_1;
    frag_color0_ = _e56;
    let _e57 = in_color1_1;
    frag_color1_ = _e57;
    let _e59 = ubo.eyePos;
    let _e61 = in_position_1;
    viewer = normalize((_e59.xyz - _e61));
    let _e64 = in_normal_1;
    let _e65 = viewer;
    d = dot(_e64, _e65);
    let _e67 = in_normal_1;
    let _e70 = d;
    let _e72 = viewer;
    reflected = (((_e67.yz * 2f) * _e70) - _e72.yz);
    let _e76 = reflected[0u];
    frag_tex_coord0_[0u] = (0.5f + (_e76 * 0.5f));
    let _e81 = reflected[1u];
    frag_tex_coord0_[1u] = (0.5f - (_e81 * 0.5f));
    let _e85 = in_tex_coord1_1;
    frag_tex_coord1_ = _e85;
    let _e86 = in_position_1;
    let _e88 = ubo.fogDistanceVector;
    let _e93 = ubo.fogDistanceVector[3u];
    s = (dot(_e86, _e88.xyz) + _e93);
    let _e95 = in_position_1;
    let _e97 = ubo.fogDepthVector;
    let _e102 = ubo.fogDepthVector[3u];
    t = (dot(_e95, _e97.xyz) + _e102);
    let _e106 = ubo.fogEyeT[1u];
    if (_e106 == 1f) {
        let _e108 = t;
        if (_e108 < 0f) {
            t = 0.03125f;
        } else {
            t = 0.96875f;
        }
    } else {
        let _e110 = t;
        if (_e110 < 1f) {
            t = 0.03125f;
        } else {
            let _e112 = t;
            let _e114 = t;
            let _e117 = ubo.fogEyeT[0u];
            t = (0.03125f + ((0.9375f * _e112) / (_e114 - _e117)));
        }
    }
    let _e121 = s;
    let _e122 = t;
    fog_tex_coord = vec2<f32>(_e121, _e122);
    return;
}

@vertex
fn main(@builtin(instance_index) gl_InstanceIndex: u32, @location(0) in_position: vec3<f32>, @location(1) in_color0_: vec4<f32>, @location(6) in_color1_: vec4<f32>, @location(5) in_normal: vec3<f32>, @location(3) in_tex_coord1_: vec2<f32>) -> VertexOutput {
    gl_InstanceIndex_1 = i32(gl_InstanceIndex);
    in_position_1 = in_position;
    in_color0_1 = in_color0_;
    in_color1_1 = in_color1_;
    in_normal_1 = in_normal;
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
