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
    @location(6) member_4: vec4<f32>,
    @location(1) member_5: vec2<f32>,
    @location(2) member_6: vec2<f32>,
    @location(3) member_7: vec2<f32>,
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
var<private> frag_color2_: vec4<f32>;
var<private> in_color2_1: vec4<f32>;
var<private> frag_tex_coord0_: vec2<f32>;
var<private> in_tex_coord0_1: vec2<f32>;
var<private> frag_tex_coord1_: vec2<f32>;
var<private> in_tex_coord1_1: vec2<f32>;
var<private> frag_tex_coord2_: vec2<f32>;
var<private> in_tex_coord2_1: vec2<f32>;

fn main_1() {
    var mvp: mat4x4<f32>;
    var local: mat4x4<f32>;

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
    let _e45 = in_normal_1;
    ibl_N = _e45;
    let _e47 = ubo.eyePos;
    let _e49 = in_position_1;
    ibl_V = normalize((_e47.xyz - _e49));
    let _e52 = in_color0_1;
    frag_color0_ = _e52;
    let _e53 = in_color1_1;
    frag_color1_ = _e53;
    let _e54 = in_color2_1;
    frag_color2_ = _e54;
    let _e55 = in_tex_coord0_1;
    frag_tex_coord0_ = _e55;
    let _e56 = in_tex_coord1_1;
    frag_tex_coord1_ = _e56;
    let _e57 = in_tex_coord2_1;
    frag_tex_coord2_ = _e57;
    return;
}

@vertex
fn main(@builtin(instance_index) gl_InstanceIndex: u32, @location(0) in_position: vec3<f32>, @location(5) in_normal: vec3<f32>, @location(1) in_color0_: vec4<f32>, @location(6) in_color1_: vec4<f32>, @location(7) in_color2_: vec4<f32>, @location(2) in_tex_coord0_: vec2<f32>, @location(3) in_tex_coord1_: vec2<f32>, @location(4) in_tex_coord2_: vec2<f32>) -> VertexOutput {
    gl_InstanceIndex_1 = i32(gl_InstanceIndex);
    in_position_1 = in_position;
    in_normal_1 = in_normal;
    in_color0_1 = in_color0_;
    in_color1_1 = in_color1_;
    in_color2_1 = in_color2_;
    in_tex_coord0_1 = in_tex_coord0_;
    in_tex_coord1_1 = in_tex_coord1_;
    in_tex_coord2_1 = in_tex_coord2_;
    main_1();
    let _e30 = unnamed_1.gl_Position.y;
    unnamed_1.gl_Position.y = -(_e30);
    let _e32 = unnamed_1.gl_Position;
    let _e33 = ibl_N;
    let _e34 = ibl_V;
    let _e35 = frag_color0_;
    let _e36 = frag_color1_;
    let _e37 = frag_color2_;
    let _e38 = frag_tex_coord0_;
    let _e39 = frag_tex_coord1_;
    let _e40 = frag_tex_coord2_;
    return VertexOutput(_e32, _e33, _e34, _e35, _e36, _e37, _e38, _e39, _e40);
}
