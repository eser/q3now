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

fn main_1() {
    var mvp: mat4x4<f32>;
    var local: mat4x4<f32>;

    if override_type_5_ {
        let _e22 = gl_InstanceIndex_1;
        let _e26 = unnamed.entMat[(_e22 * 2i)];
        local = _e26;
    } else {
        let _e28 = ubo.mvp;
        local = _e28;
    }
    let _e29 = local;
    mvp = _e29;
    let _e30 = mvp;
    let _e31 = in_position_1;
    unnamed_1.gl_Position = (_e30 * vec4<f32>(_e31.x, _e31.y, _e31.z, 1f));
    let _e38 = in_position_1;
    let _e41 = unnamed_1.gl_Position[3u];
    shadowData = vec4<f32>(_e38.x, _e38.y, _e38.z, _e41);
    let _e46 = in_color0_1;
    frag_color0_ = _e46;
    let _e47 = in_tex_coord0_1;
    frag_tex_coord0_ = _e47;
    let _e48 = in_tex_coord1_1;
    frag_tex_coord1_ = _e48;
    return;
}

@vertex
fn main(@builtin(instance_index) gl_InstanceIndex: u32, @location(0) in_position: vec3<f32>, @location(1) in_color0_: vec4<f32>, @location(2) in_tex_coord0_: vec2<f32>, @location(3) in_tex_coord1_: vec2<f32>) -> VertexOutput {
    gl_InstanceIndex_1 = i32(gl_InstanceIndex);
    in_position_1 = in_position;
    in_color0_1 = in_color0_;
    in_tex_coord0_1 = in_tex_coord0_;
    in_tex_coord1_1 = in_tex_coord1_;
    main_1();
    let _e18 = unnamed_1.gl_Position.y;
    unnamed_1.gl_Position.y = -(_e18);
    let _e20 = unnamed_1.gl_Position;
    let _e21 = shadowData;
    let _e22 = frag_color0_;
    let _e23 = frag_tex_coord0_;
    let _e24 = frag_tex_coord1_;
    return VertexOutput(_e20, _e21, _e22, _e23, _e24);
}
