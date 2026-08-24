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
    @location(3) member_3: vec2<f32>,
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
var<private> frag_tex_coord2_: vec2<f32>;
var<private> in_tex_coord2_1: vec2<f32>;

fn main_1() {
    var mvp: mat4x4<f32>;
    var local: mat4x4<f32>;
    var viewer: vec3<f32>;
    var d: f32;
    var reflected: vec2<f32>;

    if override_type_5_ {
        let _e29 = gl_InstanceIndex_1;
        let _e33 = unnamed.entMat[(_e29 * 2i)];
        local = _e33;
    } else {
        let _e35 = ubo.mvp;
        local = _e35;
    }
    let _e36 = local;
    mvp = _e36;
    let _e37 = mvp;
    let _e38 = in_position_1;
    unnamed_1.gl_Position = (_e37 * vec4<f32>(_e38.x, _e38.y, _e38.z, 1f));
    let _e45 = in_color0_1;
    frag_color0_ = _e45;
    let _e47 = ubo.eyePos;
    let _e49 = in_position_1;
    viewer = normalize((_e47.xyz - _e49));
    let _e52 = in_normal_1;
    let _e53 = viewer;
    d = dot(_e52, _e53);
    let _e55 = in_normal_1;
    let _e58 = d;
    let _e60 = viewer;
    reflected = (((_e55.yz * 2f) * _e58) - _e60.yz);
    let _e64 = reflected[0u];
    frag_tex_coord0_[0u] = (0.5f + (_e64 * 0.5f));
    let _e69 = reflected[1u];
    frag_tex_coord0_[1u] = (0.5f - (_e69 * 0.5f));
    let _e73 = in_tex_coord1_1;
    frag_tex_coord1_ = _e73;
    let _e74 = in_tex_coord2_1;
    frag_tex_coord2_ = _e74;
    return;
}

@vertex
fn main(@builtin(instance_index) gl_InstanceIndex: u32, @location(0) in_position: vec3<f32>, @location(1) in_color0_: vec4<f32>, @location(5) in_normal: vec3<f32>, @location(3) in_tex_coord1_: vec2<f32>, @location(4) in_tex_coord2_: vec2<f32>) -> VertexOutput {
    gl_InstanceIndex_1 = i32(gl_InstanceIndex);
    in_position_1 = in_position;
    in_color0_1 = in_color0_;
    in_normal_1 = in_normal;
    in_tex_coord1_1 = in_tex_coord1_;
    in_tex_coord2_1 = in_tex_coord2_;
    main_1();
    let _e20 = unnamed_1.gl_Position.y;
    unnamed_1.gl_Position.y = -(_e20);
    let _e22 = unnamed_1.gl_Position;
    let _e23 = frag_color0_;
    let _e24 = frag_tex_coord0_;
    let _e25 = frag_tex_coord1_;
    let _e26 = frag_tex_coord2_;
    return VertexOutput(_e22, _e23, _e24, _e25, _e26);
}
