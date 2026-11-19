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
    @location(8) member: vec3<f32>,
    @location(9) member_1: vec3<f32>,
    @location(1) member_2: vec2<f32>,
    @location(2) member_3: vec2<f32>,
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
var<private> frag_tex_coord0_: vec2<f32>;
var<private> in_tex_coord0_1: vec2<f32>;
var<private> frag_tex_coord1_: vec2<f32>;
var<private> in_tex_coord1_1: vec2<f32>;

fn main_1() {
    var mvp: mat4x4<f32>;
    var local: mat4x4<f32>;

    if override_type_5_ {
        let _e21 = gl_InstanceIndex_1;
        let _e25 = unnamed.entMat[(_e21 * 2i)];
        local = _e25;
    } else {
        let _e27 = ubo.mvp;
        local = _e27;
    }
    let _e28 = local;
    mvp = _e28;
    let _e29 = mvp;
    let _e30 = in_position_1;
    unnamed_1.gl_Position = (_e29 * vec4<f32>(_e30.x, _e30.y, _e30.z, 1f));
    let _e37 = in_normal_1;
    ibl_N = _e37;
    let _e39 = ubo.eyePos;
    let _e41 = in_position_1;
    ibl_V = (_e39.xyz - _e41);
    let _e43 = in_tex_coord0_1;
    frag_tex_coord0_ = _e43;
    let _e44 = in_tex_coord1_1;
    frag_tex_coord1_ = _e44;
    return;
}

@vertex
fn main(@builtin(instance_index) gl_InstanceIndex: u32, @location(0) in_position: vec3<f32>, @location(5) in_normal: vec3<f32>, @location(2) in_tex_coord0_: vec2<f32>, @location(3) in_tex_coord1_: vec2<f32>) -> VertexOutput {
    gl_InstanceIndex_1 = i32(gl_InstanceIndex);
    in_position_1 = in_position;
    in_normal_1 = in_normal;
    in_tex_coord0_1 = in_tex_coord0_;
    in_tex_coord1_1 = in_tex_coord1_;
    main_1();
    let _e18 = unnamed_1.gl_Position.y;
    unnamed_1.gl_Position.y = -(_e18);
    let _e20 = unnamed_1.gl_Position;
    let _e21 = ibl_N;
    let _e22 = ibl_V;
    let _e23 = frag_tex_coord0_;
    let _e24 = frag_tex_coord1_;
    return VertexOutput(_e20, _e21, _e22, _e23, _e24);
}
