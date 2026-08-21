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
    @location(0) member: vec4<f32>,
    @location(1) member_1: vec2<f32>,
    @location(2) member_2: vec2<f32>,
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

fn main_1() {
    var mvp: mat4x4<f32>;
    var local: mat4x4<f32>;
    var viewer: vec3<f32>;
    var d: f32;
    var reflected: vec2<f32>;

    if override_type_5_ {
        let _e27 = gl_InstanceIndex_1;
        let _e31 = unnamed.entMat[(_e27 * 2i)];
        local = _e31;
    } else {
        let _e33 = ubo.mvp;
        local = _e33;
    }
    let _e34 = local;
    mvp = _e34;
    let _e35 = mvp;
    let _e36 = in_position_1;
    unnamed_1.gl_Position = (_e35 * vec4<f32>(_e36.x, _e36.y, _e36.z, 1f));
    let _e43 = in_color0_1;
    frag_color0_ = _e43;
    let _e45 = ubo.eyePos;
    let _e47 = in_position_1;
    viewer = normalize((_e45.xyz - _e47));
    let _e50 = in_normal_1;
    let _e51 = viewer;
    d = dot(_e50, _e51);
    let _e53 = in_normal_1;
    let _e56 = d;
    let _e58 = viewer;
    reflected = (((_e53.yz * 2f) * _e56) - _e58.yz);
    let _e62 = reflected[0u];
    frag_tex_coord0_[0u] = (0.5f + (_e62 * 0.5f));
    let _e67 = reflected[1u];
    frag_tex_coord0_[1u] = (0.5f - (_e67 * 0.5f));
    let _e71 = in_tex_coord1_1;
    frag_tex_coord1_ = _e71;
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
    let _e17 = unnamed_1.gl_Position.y;
    unnamed_1.gl_Position.y = -(_e17);
    let _e19 = unnamed_1.gl_Position;
    let _e20 = frag_color0_;
    let _e21 = frag_tex_coord0_;
    let _e22 = frag_tex_coord1_;
    return VertexOutput(_e19, _e20, _e21, _e22);
}
