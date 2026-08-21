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
    @location(1) member_2: vec2<f32>,
    @location(2) member_3: vec2<f32>,
    @location(3) member_4: vec2<f32>,
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
        let _e24 = gl_InstanceIndex_1;
        let _e28 = unnamed.entMat[(_e24 * 2i)];
        local = _e28;
    } else {
        let _e30 = ubo.mvp;
        local = _e30;
    }
    let _e31 = local;
    mvp = _e31;
    let _e32 = mvp;
    let _e33 = in_position_1;
    unnamed_1.gl_Position = (_e32 * vec4<f32>(_e33.x, _e33.y, _e33.z, 1f));
    let _e40 = in_position_1;
    let _e43 = unnamed_1.gl_Position[3u];
    shadowData = vec4<f32>(_e40.x, _e40.y, _e40.z, _e43);
    let _e48 = in_color0_1;
    frag_color0_ = _e48;
    let _e49 = in_tex_coord0_1;
    frag_tex_coord0_ = _e49;
    let _e50 = in_tex_coord1_1;
    frag_tex_coord1_ = _e50;
    let _e51 = in_tex_coord2_1;
    frag_tex_coord2_ = _e51;
    return;
}

@vertex 
fn main(@builtin(instance_index) gl_InstanceIndex: u32, @location(0) in_position: vec3<f32>, @location(1) in_color0_: vec4<f32>, @location(2) in_tex_coord0_: vec2<f32>, @location(3) in_tex_coord1_: vec2<f32>, @location(4) in_tex_coord2_: vec2<f32>) -> VertexOutput {
    gl_InstanceIndex_1 = i32(gl_InstanceIndex);
    in_position_1 = in_position;
    in_color0_1 = in_color0_;
    in_tex_coord0_1 = in_tex_coord0_;
    in_tex_coord1_1 = in_tex_coord1_;
    in_tex_coord2_1 = in_tex_coord2_;
    main_1();
    let _e21 = unnamed_1.gl_Position.y;
    unnamed_1.gl_Position.y = -(_e21);
    let _e23 = unnamed_1.gl_Position;
    let _e24 = shadowData;
    let _e25 = frag_color0_;
    let _e26 = frag_tex_coord0_;
    let _e27 = frag_tex_coord1_;
    let _e28 = frag_tex_coord2_;
    return VertexOutput(_e23, _e24, _e25, _e26, _e27, _e28);
}
