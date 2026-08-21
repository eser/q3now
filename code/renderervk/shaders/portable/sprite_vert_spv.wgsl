struct SpriteHeader {
    originW: vec4<f32>,
    rgba: vec4<f32>,
    shaderHandle: u32,
    flags: u32,
    pad0_: u32,
    pad1_: u32,
}

struct Sprites {
    sprites: array<SpriteHeader>,
}

struct EffectsUBO {
    mvp: mat4x4<f32>,
    viewLeft: vec4<f32>,
    viewUp: vec4<f32>,
    frameParams: vec4<f32>,
}

struct gl_PerVertex {
    @builtin(position) gl_Position: vec4<f32>,
}

struct VertexOutput {
    @builtin(position) gl_Position: vec4<f32>,
    @location(0) member: vec2<f32>,
    @location(1) member_1: vec4<f32>,
}

@group(0) @binding(0) 
var<storage> unnamed: Sprites;
var<private> gl_InstanceIndex_1: i32;
var<private> gl_VertexIndex_1: i32;
@group(1) @binding(0) 
var<uniform> unnamed_1: EffectsUBO;
var<private> unnamed_2: gl_PerVertex = gl_PerVertex(vec4<f32>(0f, 0f, 0f, 1f));
var<private> fragUV: vec2<f32>;
var<private> fragColor: vec4<f32>;

fn main_1() {
    var hdr: SpriteHeader;
    var vertInQuad: u32;
    var sx: f32;
    var indexable: array<f32, 6>;
    var uy: f32;
    var indexable_1: array<f32, 6>;
    var radius: f32;
    var worldPos: vec3<f32>;
    var indexable_2: array<vec2<f32>, 6>;

    let _e34 = gl_InstanceIndex_1;
    let _e37 = unnamed.sprites[_e34];
    hdr.originW = _e37.originW;
    hdr.rgba = _e37.rgba;
    hdr.shaderHandle = _e37.shaderHandle;
    hdr.flags = _e37.flags;
    hdr.pad0_ = _e37.pad0_;
    hdr.pad1_ = _e37.pad1_;
    let _e50 = gl_VertexIndex_1;
    vertInQuad = (bitcast<u32>(_e50) % 6u);
    let _e53 = vertInQuad;
    indexable = array<f32, 6>(1f, 1f, -1f, -1f, 1f, -1f);
    let _e55 = indexable[_e53];
    sx = _e55;
    let _e56 = vertInQuad;
    indexable_1 = array<f32, 6>(-1f, 1f, -1f, -1f, 1f, 1f);
    let _e58 = indexable_1[_e56];
    uy = _e58;
    let _e61 = hdr.originW[3u];
    radius = _e61;
    let _e63 = hdr.originW;
    let _e66 = unnamed_1.viewLeft;
    let _e68 = sx;
    let _e69 = radius;
    let _e74 = unnamed_1.viewUp;
    let _e76 = uy;
    let _e77 = radius;
    worldPos = ((_e63.xyz + (_e66.xyz * (_e68 * _e69))) + (_e74.xyz * (_e76 * _e77)));
    let _e82 = unnamed_1.mvp;
    let _e83 = worldPos;
    unnamed_2.gl_Position = (_e82 * vec4<f32>(_e83.x, _e83.y, _e83.z, 1f));
    let _e90 = vertInQuad;
    indexable_2 = array<vec2<f32>, 6>(vec2<f32>(0f, 1f), vec2<f32>(0f, 0f), vec2<f32>(1f, 1f), vec2<f32>(1f, 1f), vec2<f32>(0f, 0f), vec2<f32>(1f, 0f));
    let _e92 = indexable_2[_e90];
    fragUV = _e92;
    let _e94 = hdr.rgba;
    fragColor = _e94;
    return;
}

@vertex 
fn main(@builtin(instance_index) gl_InstanceIndex: u32, @builtin(vertex_index) gl_VertexIndex: u32) -> VertexOutput {
    gl_InstanceIndex_1 = i32(gl_InstanceIndex);
    gl_VertexIndex_1 = i32(gl_VertexIndex);
    main_1();
    let _e11 = unnamed_2.gl_Position.y;
    unnamed_2.gl_Position.y = -(_e11);
    let _e13 = unnamed_2.gl_Position;
    let _e14 = fragUV;
    let _e15 = fragColor;
    return VertexOutput(_e13, _e14, _e15);
}
