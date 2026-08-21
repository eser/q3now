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
    @location(1) member_3: vec2<f32>,
    @location(2) member_4: vec2<f32>,
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

fn main_1() {
    var mvp: mat4x4<f32>;
    var local: mat4x4<f32>;

    if override_type_5_ {
        let _e23 = gl_InstanceIndex_1;
        let _e27 = unnamed.entMat[(_e23 * 2i)];
        local = _e27;
    } else {
        let _e29 = ubo.mvp;
        local = _e29;
    }
    let _e30 = local;
    mvp = _e30;
    let _e31 = mvp;
    let _e32 = in_position_1;
    unnamed_1.gl_Position = (_e31 * vec4<f32>(_e32.x, _e32.y, _e32.z, 1f));
    let _e39 = in_normal_1;
    ibl_N = _e39;
    let _e41 = ubo.eyePos;
    let _e43 = in_position_1;
    ibl_V = normalize((_e41.xyz - _e43));
    let _e46 = in_color0_1;
    frag_color0_ = _e46;
    let _e47 = in_tex_coord0_1;
    frag_tex_coord0_ = _e47;
    let _e48 = in_tex_coord1_1;
    frag_tex_coord1_ = _e48;
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
    let _e21 = unnamed_1.gl_Position.y;
    unnamed_1.gl_Position.y = -(_e21);
    let _e23 = unnamed_1.gl_Position;
    let _e24 = ibl_N;
    let _e25 = ibl_V;
    let _e26 = frag_color0_;
    let _e27 = frag_tex_coord0_;
    let _e28 = frag_tex_coord1_;
    return VertexOutput(_e23, _e24, _e25, _e26, _e27, _e28);
}
