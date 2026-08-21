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
    @location(0) member_2: vec4<f32>,
    @location(5) member_3: vec4<f32>,
    @location(1) member_4: vec2<f32>,
    @location(2) member_5: vec2<f32>,
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

fn main_1() {
    var mvp: mat4x4<f32>;
    var local: mat4x4<f32>;

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
    let _e41 = in_normal_1;
    ibl_N = _e41;
    let _e43 = ubo.eyePos;
    let _e45 = in_position_1;
    ibl_V = normalize((_e43.xyz - _e45));
    let _e48 = in_color0_1;
    frag_color0_ = _e48;
    let _e49 = in_color1_1;
    frag_color1_ = _e49;
    let _e50 = in_tex_coord0_1;
    frag_tex_coord0_ = _e50;
    let _e51 = in_tex_coord1_1;
    frag_tex_coord1_ = _e51;
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
    let _e24 = unnamed_1.gl_Position.y;
    unnamed_1.gl_Position.y = -(_e24);
    let _e26 = unnamed_1.gl_Position;
    let _e27 = ibl_N;
    let _e28 = ibl_V;
    let _e29 = frag_color0_;
    let _e30 = frag_color1_;
    let _e31 = frag_tex_coord0_;
    let _e32 = frag_tex_coord1_;
    return VertexOutput(_e26, _e27, _e28, _e29, _e30, _e31, _e32);
}
