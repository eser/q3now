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
var<private> frag_tex_coord0_: vec2<f32>;
var<private> in_tex_coord0_1: vec2<f32>;

fn main_1() {
    var mvp: mat4x4<f32>;
    var local: mat4x4<f32>;

    if override_type_5_ {
        let _e16 = gl_InstanceIndex_1;
        let _e20 = unnamed.entMat[(_e16 * 2i)];
        local = _e20;
    } else {
        let _e22 = ubo.mvp;
        local = _e22;
    }
    let _e23 = local;
    mvp = _e23;
    let _e24 = mvp;
    let _e25 = in_position_1;
    unnamed_1.gl_Position = (_e24 * vec4<f32>(_e25.x, _e25.y, _e25.z, 1f));
    let _e32 = in_tex_coord0_1;
    frag_tex_coord0_ = _e32;
    return;
}

@vertex 
fn main(@builtin(instance_index) gl_InstanceIndex: u32, @location(0) in_position: vec3<f32>, @location(2) in_tex_coord0_: vec2<f32>) -> VertexOutput {
    gl_InstanceIndex_1 = i32(gl_InstanceIndex);
    in_position_1 = in_position;
    in_tex_coord0_1 = in_tex_coord0_;
    main_1();
    let _e11 = unnamed_1.gl_Position.y;
    unnamed_1.gl_Position.y = -(_e11);
    let _e13 = unnamed_1.gl_Position;
    let _e14 = frag_tex_coord0_;
    return VertexOutput(_e13, _e14);
}
