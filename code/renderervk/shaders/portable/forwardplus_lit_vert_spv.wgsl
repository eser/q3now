struct gl_PerVertex {
    @builtin(position) gl_Position: vec4<f32>,
}

struct UBO {
    eyePos: vec4<f32>,
    _pad_light: array<vec4<f32>, 3>,
    fogDistanceVector: vec4<f32>,
    fogDepthVector: vec4<f32>,
    fogEyeT: vec4<f32>,
    fogColor: vec4<f32>,
    _pad_to_modelMatrix: array<vec4<f32>, 18>,
    modelMatrix: mat4x4<f32>,
    mvp: mat4x4<f32>,
}

struct VertexOutput {
    @builtin(position) gl_Position: vec4<f32>,
    @location(0) member: vec2<f32>,
    @location(1) member_1: vec3<f32>,
    @location(2) member_2: vec3<f32>,
    @location(3) member_3: vec3<f32>,
}

var<private> unnamed: gl_PerVertex = gl_PerVertex(vec4<f32>(0f, 0f, 0f, 1f));
@group(0) @binding(0) 
var<uniform> unnamed_1: UBO;
var<private> in_position_1: vec3<f32>;
var<private> frag_tex_coord: vec2<f32>;
var<private> in_tex_coord_1: vec2<f32>;
var<private> world_pos: vec3<f32>;
var<private> world_normal: vec3<f32>;
var<private> in_normal_1: vec3<f32>;
var<private> world_view: vec3<f32>;

fn main_1() {
    var wp: vec3<f32>;

    let _e17 = unnamed_1.mvp;
    let _e18 = in_position_1;
    unnamed.gl_Position = (_e17 * vec4<f32>(_e18.x, _e18.y, _e18.z, 1f));
    let _e25 = in_tex_coord_1;
    frag_tex_coord = _e25;
    let _e27 = unnamed_1.modelMatrix;
    let _e28 = in_position_1;
    wp = (_e27 * vec4<f32>(_e28.x, _e28.y, _e28.z, 1f)).xyz;
    let _e35 = wp;
    world_pos = _e35;
    let _e37 = unnamed_1.modelMatrix;
    let _e45 = in_normal_1;
    world_normal = (mat3x3<f32>(_e37[0].xyz, _e37[1].xyz, _e37[2].xyz) * _e45);
    let _e48 = unnamed_1.eyePos;
    let _e50 = wp;
    world_view = (_e48.xyz - _e50);
    return;
}

@vertex 
fn main(@location(0) in_position: vec3<f32>, @location(1) in_tex_coord: vec2<f32>, @location(2) in_normal: vec3<f32>) -> VertexOutput {
    in_position_1 = in_position;
    in_tex_coord_1 = in_tex_coord;
    in_normal_1 = in_normal;
    main_1();
    let _e13 = unnamed.gl_Position.y;
    unnamed.gl_Position.y = -(_e13);
    let _e15 = unnamed.gl_Position;
    let _e16 = frag_tex_coord;
    let _e17 = world_pos;
    let _e18 = world_normal;
    let _e19 = world_view;
    return VertexOutput(_e15, _e16, _e17, _e18, _e19);
}
