struct EntityMatrices {
    entMat: array<mat4x4<f32>>,
}

struct UBO {
    eyePos: vec4<f32>,
    lightPos: vec4<f32>,
    lightColor: vec4<f32>,
    lightVector: vec4<f32>,
    _pad_to_mvp: array<vec4<f32>, 26>,
    mvp: mat4x4<f32>,
}

struct gl_PerVertex {
    @builtin(position) gl_Position: vec4<f32>,
}

struct VertexOutput {
    @builtin(position) gl_Position: vec4<f32>,
    @location(1) member: vec2<f32>,
    @location(2) member_1: vec2<f32>,
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
var<private> in_normal_1: vec3<f32>;
var<private> frag_tex_coord0_: vec2<f32>;
var<private> frag_tex_coord1_: vec2<f32>;
var<private> in_tex_coord1_1: vec2<f32>;

fn main_1() {
    var mvp: mat4x4<f32>;
    var local: mat4x4<f32>;
    var viewer: vec3<f32>;
    var d: f32;
    var reflected: vec2<f32>;

    if override_type_5_ {
        let _e25 = gl_InstanceIndex_1;
        let _e29 = unnamed.entMat[(_e25 * 2i)];
        local = _e29;
    } else {
        let _e31 = ubo.mvp;
        local = _e31;
    }
    let _e32 = local;
    mvp = _e32;
    let _e33 = mvp;
    let _e34 = in_position_1;
    unnamed_1.gl_Position = (_e33 * vec4<f32>(_e34.x, _e34.y, _e34.z, 1f));
    let _e42 = ubo.eyePos;
    let _e44 = in_position_1;
    viewer = normalize((_e42.xyz - _e44));
    let _e47 = in_normal_1;
    let _e48 = viewer;
    d = dot(_e47, _e48);
    let _e50 = in_normal_1;
    let _e53 = d;
    let _e55 = viewer;
    reflected = (((_e50.yz * 2f) * _e53) - _e55.yz);
    let _e59 = reflected[0u];
    frag_tex_coord0_[0u] = (0.5f + (_e59 * 0.5f));
    let _e64 = reflected[1u];
    frag_tex_coord0_[1u] = (0.5f - (_e64 * 0.5f));
    let _e68 = in_tex_coord1_1;
    frag_tex_coord1_ = _e68;
    return;
}

@vertex
fn main(@builtin(instance_index) gl_InstanceIndex: u32, @location(0) in_position: vec3<f32>, @location(5) in_normal: vec3<f32>, @location(3) in_tex_coord1_: vec2<f32>) -> VertexOutput {
    gl_InstanceIndex_1 = i32(gl_InstanceIndex);
    in_position_1 = in_position;
    in_normal_1 = in_normal;
    in_tex_coord1_1 = in_tex_coord1_;
    main_1();
    let _e14 = unnamed_1.gl_Position.y;
    unnamed_1.gl_Position.y = -(_e14);
    let _e16 = unnamed_1.gl_Position;
    let _e17 = frag_tex_coord0_;
    let _e18 = frag_tex_coord1_;
    return VertexOutput(_e16, _e17, _e18);
}
