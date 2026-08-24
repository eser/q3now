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
    @location(7) member: vec4<f32>,
    @location(0) member_1: vec4<f32>,
    @location(5) member_2: vec4<f32>,
    @location(6) member_3: vec4<f32>,
    @location(1) member_4: vec2<f32>,
    @location(2) member_5: vec2<f32>,
    @location(3) member_6: vec2<f32>,
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
var<private> shadowData: vec4<f32>;
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
        let _e28 = gl_InstanceIndex_1;
        let _e32 = unnamed.entMat[(_e28 * 2i)];
        local = _e32;
    } else {
        let _e34 = ubo.mvp;
        local = _e34;
    }
    let _e35 = local;
    mvp = _e35;
    let _e36 = mvp;
    let _e37 = in_position_1;
    unnamed_1.gl_Position = (_e36 * vec4<f32>(_e37.x, _e37.y, _e37.z, 1f));
    let _e44 = in_position_1;
    let _e47 = unnamed_1.gl_Position[3u];
    shadowData = vec4<f32>(_e44.x, _e44.y, _e44.z, _e47);
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
fn main(@builtin(instance_index) gl_InstanceIndex: u32, @location(0) in_position: vec3<f32>, @location(1) in_color0_: vec4<f32>, @location(6) in_color1_: vec4<f32>, @location(7) in_color2_: vec4<f32>, @location(2) in_tex_coord0_: vec2<f32>, @location(3) in_tex_coord1_: vec2<f32>, @location(4) in_tex_coord2_: vec2<f32>) -> VertexOutput {
    gl_InstanceIndex_1 = i32(gl_InstanceIndex);
    in_position_1 = in_position;
    in_color0_1 = in_color0_;
    in_color1_1 = in_color1_;
    in_color2_1 = in_color2_;
    in_tex_coord0_1 = in_tex_coord0_;
    in_tex_coord1_1 = in_tex_coord1_;
    in_tex_coord2_1 = in_tex_coord2_;
    main_1();
    let _e27 = unnamed_1.gl_Position.y;
    unnamed_1.gl_Position.y = -(_e27);
    let _e29 = unnamed_1.gl_Position;
    let _e30 = shadowData;
    let _e31 = frag_color0_;
    let _e32 = frag_color1_;
    let _e33 = frag_color2_;
    let _e34 = frag_tex_coord0_;
    let _e35 = frag_tex_coord1_;
    let _e36 = frag_tex_coord2_;
    return VertexOutput(_e29, _e30, _e31, _e32, _e33, _e34, _e35, _e36);
}
