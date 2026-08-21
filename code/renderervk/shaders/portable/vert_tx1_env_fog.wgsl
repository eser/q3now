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
    @location(1) member_1: vec2<f32>,
    @location(2) member_2: vec2<f32>,
    @location(4) member_3: vec2<f32>,
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
    let _e54 = in_color0_1;
    frag_color0_ = _e54;
    let _e56 = ubo.eyePos;
    let _e58 = in_position_1;
    viewer = normalize((_e56.xyz - _e58));
    let _e61 = in_normal_1;
    let _e62 = viewer;
    d = dot(_e61, _e62);
    let _e64 = in_normal_1;
    let _e67 = d;
    let _e69 = viewer;
    reflected = (((_e64.yz * 2f) * _e67) - _e69.yz);
    let _e73 = reflected[0u];
    frag_tex_coord0_[0u] = (0.5f + (_e73 * 0.5f));
    let _e78 = reflected[1u];
    frag_tex_coord0_[1u] = (0.5f - (_e78 * 0.5f));
    let _e82 = in_tex_coord1_1;
    frag_tex_coord1_ = _e82;
    let _e83 = in_position_1;
    let _e85 = ubo.fogDistanceVector;
    let _e90 = ubo.fogDistanceVector[3u];
    s = (dot(_e83, _e85.xyz) + _e90);
    let _e92 = in_position_1;
    let _e94 = ubo.fogDepthVector;
    let _e99 = ubo.fogDepthVector[3u];
    t = (dot(_e92, _e94.xyz) + _e99);
    let _e103 = ubo.fogEyeT[1u];
    if (_e103 == 1f) {
        let _e105 = t;
        if (_e105 < 0f) {
            t = 0.03125f;
        } else {
            t = 0.96875f;
        }
    } else {
        let _e107 = t;
        if (_e107 < 1f) {
            t = 0.03125f;
        } else {
            let _e109 = t;
            let _e111 = t;
            let _e114 = ubo.fogEyeT[0u];
            t = (0.03125f + ((0.9375f * _e109) / (_e111 - _e114)));
        }
    }
    let _e118 = s;
    let _e119 = t;
    fog_tex_coord = vec2<f32>(_e118, _e119);
    return;
}

@vertex 
fn main(@builtin(instance_index) gl_InstanceIndex: u32, @location(0) in_position: vec3<f32>, @location(1) in_color0_: vec4<f32>, @location(5) in_normal: vec3<f32>, @location(3) in_tex_coord1_: vec2<f32>) -> VertexOutput {
    gl_InstanceIndex_1 = i32(gl_InstanceIndex);
    in_position_1 = in_position;
    in_color0_1 = in_color0_;
    in_normal_1 = in_normal;
    in_tex_coord1_1 = in_tex_coord1_;
    main_1();
    let _e18 = unnamed_1.gl_Position.y;
    unnamed_1.gl_Position.y = -(_e18);
    let _e20 = unnamed_1.gl_Position;
    let _e21 = frag_color0_;
    let _e22 = frag_tex_coord0_;
    let _e23 = frag_tex_coord1_;
    let _e24 = fog_tex_coord;
    return VertexOutput(_e20, _e21, _e22, _e23, _e24);
}
