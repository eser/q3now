struct EntityMatrices {
    entMat: array<mat4x4<f32>>,
}

struct UBO {
    eyePos: vec4<f32>,
    lightPos: vec4<f32>,
    lightColor: vec4<f32>,
    lightVector: vec4<f32>,
    _pad_to_modelMatrix: array<vec4<f32>, 22>,
    modelMatrix: mat4x4<f32>,
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
    @location(6) member_4: vec4<f32>,
}

@id(16) override ENTITY_SSBO: i32 = 0i;
override override_type_5_: bool = (ENTITY_SSBO != 0i);
override override_type_5_1: bool = (ENTITY_SSBO != 0i);

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
var<private> shadowData: vec4<f32>;

fn main_1() {
    var mvp: mat4x4<f32>;
    var local: mat4x4<f32>;
    var modelMatrix: mat4x4<f32>;
    var local_1: mat4x4<f32>;

    if override_type_5_ {
        let _e27 = gl_InstanceIndex_1;
        let _e31 = unnamed.entMat[(_e27 * 2i)];
        local = _e31;
    } else {
        let _e33 = ubo.mvp;
        local = _e33;
    }
    let _e34 = local;
    mvp = _e34;
    let _e35 = mvp;
    let _e36 = in_position_1;
    unnamed_1.gl_Position = (_e35 * vec4<f32>(_e36.x, _e36.y, _e36.z, 1f));
    let _e43 = in_tex_coord_1;
    frag_tex_coord = _e43;
    let _e44 = in_normal_1;
    N = _e44;
    let _e46 = ubo.lightPos;
    let _e47 = in_position_1;
    L = (_e46 - vec4<f32>(_e47.x, _e47.y, _e47.z, 1f));
    let _e54 = ubo.eyePos;
    let _e55 = in_position_1;
    V = (_e54 - vec4<f32>(_e55.x, _e55.y, _e55.z, 1f));
    if override_type_5_1 {
        let _e61 = gl_InstanceIndex_1;
        let _e66 = unnamed.entMat[((_e61 * 2i) + 1i)];
        local_1 = _e66;
    } else {
        let _e68 = ubo.modelMatrix;
        local_1 = _e68;
    }
    let _e69 = local_1;
    modelMatrix = _e69;
    let _e70 = modelMatrix;
    let _e71 = in_position_1;
    let _e77 = (_e70 * vec4<f32>(_e71.x, _e71.y, _e71.z, 1f)).xyz;
    let _e80 = unnamed_1.gl_Position[3u];
    shadowData = vec4<f32>(_e77.x, _e77.y, _e77.z, _e80);
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
    let _e24 = shadowData;
    return VertexOutput(_e19, _e20, _e21, _e22, _e23, _e24);
}
