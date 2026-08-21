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
    @location(0) member: vec2<f32>,
    @location(1) member_1: vec3<f32>,
    @location(2) member_2: vec4<f32>,
    @location(3) member_3: vec4<f32>,
    @location(5) member_4: vec3<f32>,
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
var<private> frag_tex_coord: vec2<f32>;
var<private> in_tex_coord_1: vec2<f32>;
var<private> N: vec3<f32>;
var<private> in_normal_1: vec3<f32>;
var<private> L: vec4<f32>;
var<private> V: vec4<f32>;
var<private> frag_position: vec3<f32>;

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
    let _e38 = in_tex_coord_1;
    frag_tex_coord = _e38;
    let _e39 = in_normal_1;
    N = _e39;
    let _e41 = ubo.lightPos;
    let _e42 = in_position_1;
    L = (_e41 - vec4<f32>(_e42.x, _e42.y, _e42.z, 1f));
    let _e49 = ubo.eyePos;
    let _e50 = in_position_1;
    V = (_e49 - vec4<f32>(_e50.x, _e50.y, _e50.z, 1f));
    let _e56 = in_position_1;
    frag_position = _e56;
    return;
}

@vertex 
fn main(@builtin(instance_index) gl_InstanceIndex: u32, @location(0) in_position: vec3<f32>, @location(1) in_tex_coord: vec2<f32>, @location(2) in_normal: vec3<f32>) -> VertexOutput {
    gl_InstanceIndex_1 = i32(gl_InstanceIndex);
    in_position_1 = in_position;
    in_tex_coord_1 = in_tex_coord;
    in_normal_1 = in_normal;
    main_1();
    let _e17 = unnamed_1.gl_Position.y;
    unnamed_1.gl_Position.y = -(_e17);
    let _e19 = unnamed_1.gl_Position;
    let _e20 = frag_tex_coord;
    let _e21 = N;
    let _e22 = L;
    let _e23 = V;
    let _e24 = frag_position;
    return VertexOutput(_e19, _e20, _e21, _e22, _e23, _e24);
}
