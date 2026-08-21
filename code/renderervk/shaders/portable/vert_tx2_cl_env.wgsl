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
    @location(5) member_1: vec4<f32>,
    @location(6) member_2: vec4<f32>,
    @location(1) member_3: vec2<f32>,
    @location(2) member_4: vec2<f32>,
    @location(3) member_5: vec2<f32>,
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
var<private> frag_color1_: vec4<f32>;
var<private> in_color1_1: vec4<f32>;
var<private> frag_color2_: vec4<f32>;
var<private> in_color2_1: vec4<f32>;
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
        let _e33 = gl_InstanceIndex_1;
        let _e37 = unnamed.entMat[(_e33 * 2i)];
        local = _e37;
    } else {
        let _e39 = ubo.mvp;
        local = _e39;
    }
    let _e40 = local;
    mvp = _e40;
    let _e41 = mvp;
    let _e42 = in_position_1;
    unnamed_1.gl_Position = (_e41 * vec4<f32>(_e42.x, _e42.y, _e42.z, 1f));
    let _e49 = in_color0_1;
    frag_color0_ = _e49;
    let _e50 = in_color1_1;
    frag_color1_ = _e50;
    let _e51 = in_color2_1;
    frag_color2_ = _e51;
    let _e53 = ubo.eyePos;
    let _e55 = in_position_1;
    viewer = normalize((_e53.xyz - _e55));
    let _e58 = in_normal_1;
    let _e59 = viewer;
    d = dot(_e58, _e59);
    let _e61 = in_normal_1;
    let _e64 = d;
    let _e66 = viewer;
    reflected = (((_e61.yz * 2f) * _e64) - _e66.yz);
    let _e70 = reflected[0u];
    frag_tex_coord0_[0u] = (0.5f + (_e70 * 0.5f));
    let _e75 = reflected[1u];
    frag_tex_coord0_[1u] = (0.5f - (_e75 * 0.5f));
    let _e79 = in_tex_coord1_1;
    frag_tex_coord1_ = _e79;
    let _e80 = in_tex_coord2_1;
    frag_tex_coord2_ = _e80;
    return;
}

@vertex 
fn main(@builtin(instance_index) gl_InstanceIndex: u32, @location(0) in_position: vec3<f32>, @location(1) in_color0_: vec4<f32>, @location(6) in_color1_: vec4<f32>, @location(7) in_color2_: vec4<f32>, @location(5) in_normal: vec3<f32>, @location(3) in_tex_coord1_: vec2<f32>, @location(4) in_tex_coord2_: vec2<f32>) -> VertexOutput {
    gl_InstanceIndex_1 = i32(gl_InstanceIndex);
    in_position_1 = in_position;
    in_color0_1 = in_color0_;
    in_color1_1 = in_color1_;
    in_color2_1 = in_color2_;
    in_normal_1 = in_normal;
    in_tex_coord1_1 = in_tex_coord1_;
    in_tex_coord2_1 = in_tex_coord2_;
    main_1();
    let _e26 = unnamed_1.gl_Position.y;
    unnamed_1.gl_Position.y = -(_e26);
    let _e28 = unnamed_1.gl_Position;
    let _e29 = frag_color0_;
    let _e30 = frag_color1_;
    let _e31 = frag_color2_;
    let _e32 = frag_tex_coord0_;
    let _e33 = frag_tex_coord1_;
    let _e34 = frag_tex_coord2_;
    return VertexOutput(_e28, _e29, _e30, _e31, _e32, _e33, _e34);
}
