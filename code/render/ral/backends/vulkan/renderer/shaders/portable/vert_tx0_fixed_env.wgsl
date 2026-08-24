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
var<private> in_normal_1: vec3<f32>;
var<private> frag_tex_coord0_: vec2<f32>;

fn main_1() {
    var mvp: mat4x4<f32>;
    var local: mat4x4<f32>;
    var viewer: vec3<f32>;
    var d: f32;
    var reflected: vec2<f32>;

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
    let _e40 = ubo.eyePos;
    let _e42 = in_position_1;
    viewer = normalize((_e40.xyz - _e42));
    let _e45 = in_normal_1;
    let _e46 = viewer;
    d = dot(_e45, _e46);
    let _e48 = in_normal_1;
    let _e51 = d;
    let _e53 = viewer;
    reflected = (((_e48.yz * 2f) * _e51) - _e53.yz);
    let _e57 = reflected[0u];
    frag_tex_coord0_[0u] = (0.5f + (_e57 * 0.5f));
    let _e62 = reflected[1u];
    frag_tex_coord0_[1u] = (0.5f - (_e62 * 0.5f));
    return;
}

@vertex
fn main(@builtin(instance_index) gl_InstanceIndex: u32, @location(0) in_position: vec3<f32>, @location(5) in_normal: vec3<f32>) -> VertexOutput {
    gl_InstanceIndex_1 = i32(gl_InstanceIndex);
    in_position_1 = in_position;
    in_normal_1 = in_normal;
    main_1();
    let _e11 = unnamed_1.gl_Position.y;
    unnamed_1.gl_Position.y = -(_e11);
    let _e13 = unnamed_1.gl_Position;
    let _e14 = frag_tex_coord0_;
    return VertexOutput(_e13, _e14);
}
