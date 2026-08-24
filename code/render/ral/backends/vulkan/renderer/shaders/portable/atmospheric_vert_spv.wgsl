struct gl_PerVertex {
    @builtin(position) gl_Position: vec4<f32>,
}

struct AtmFrame {
    mvp: mat4x4<f32>,
    viewLeft: vec4<f32>,
    viewUp: vec4<f32>,
    eyeWorld: vec4<f32>,
    dt: f32,
    time: f32,
    poolSize: u32,
    pingPongRead: u32,
    boundsMin: vec4<f32>,
    boundsMax: vec4<f32>,
    worldMins: vec2<f32>,
    worldMaxs: vec2<f32>,
    invGridStep: vec2<f32>,
    gridSize: u32,
    atmType: u32,
    distance: f32,
    invResX: f32,
    invResY: f32,
    depthValid: f32,
}

struct AtmParticle {
    pos: vec3<f32>,
    seed: f32,
    vel: vec3<f32>,
    flags: f32,
}

struct Pool {
    particles: array<AtmParticle>,
}

struct VertexOutput {
    @builtin(position) gl_Position: vec4<f32>,
    @location(0) member: vec2<f32>,
    @location(1) member_1: vec4<f32>,
}

var<private> unnamed: gl_PerVertex = gl_PerVertex(vec4<f32>(0f, 0f, 0f, 1f));
var<private> fragUV: vec2<f32>;
var<private> fragColor: vec4<f32>;
var<private> gl_InstanceIndex_1: i32;
var<private> gl_VertexIndex_1: i32;
@group(0) @binding(0)
var<uniform> unnamed_1: AtmFrame;
@group(0) @binding(1)
var<storage> unnamed_2: Pool;

fn emitDegenerate_u0028_() {
    unnamed.gl_Position = vec4<f32>(0f, 0f, 0f, 1f);
    fragUV = vec2<f32>(0f, 0f);
    fragColor = vec4<f32>(0f, 0f, 0f, 0f);
    return;
}

fn main_1() {
    var idx: u32;
    var vertInQuad: u32;
    var p: AtmParticle;
    var sx: f32;
    var indexable: array<f32, 6>;
    var uy: f32;
    var indexable_1: array<f32, 6>;
    var right: vec3<f32>;
    var down: vec3<f32>;
    var worldPos: vec3<f32>;
    var color: vec4<f32>;
    var indexable_2: array<vec2<f32>, 6>;
    var phi_58_: bool;

    let _e49 = gl_InstanceIndex_1;
    idx = bitcast<u32>(_e49);
    let _e51 = gl_VertexIndex_1;
    vertInQuad = (bitcast<u32>(_e51) % 6u);
    let _e54 = idx;
    let _e56 = unnamed_1.poolSize;
    let _e57 = (_e54 >= _e56);
    phi_58_ = _e57;
    if !(_e57) {
        let _e60 = unnamed_1.atmType;
        phi_58_ = (_e60 == 0u);
    }
    let _e63 = phi_58_;
    if _e63 {
        emitDegenerate_u0028_();
        return;
    }
    let _e64 = idx;
    let _e67 = unnamed_2.particles[_e64];
    p.pos = _e67.pos;
    p.seed = _e67.seed;
    p.vel = _e67.vel;
    p.flags = _e67.flags;
    let _e77 = p.flags;
    if (_e77 < 0.5f) {
        emitDegenerate_u0028_();
        return;
    }
    let _e79 = vertInQuad;
    indexable = array<f32, 6>(-1f, -1f, 1f, 1f, -1f, 1f);
    let _e81 = indexable[_e79];
    sx = _e81;
    let _e82 = vertInQuad;
    indexable_1 = array<f32, 6>(1f, -1f, 1f, 1f, -1f, -1f);
    let _e84 = indexable_1[_e82];
    uy = _e84;
    let _e86 = unnamed_1.atmType;
    if (_e86 == 1u) {
        let _e89 = unnamed_1.viewLeft;
        let _e91 = sx;
        right = (_e89.xyz * (_e91 * 0.6f));
        let _e94 = uy;
        down = ((vec3<f32>(0f, 0f, 1f) * ((_e94 * 0.5f) - 0.5f)) * 18f);
        let _e100 = p.pos;
        let _e101 = right;
        let _e103 = down;
        worldPos = ((_e100 + _e101) + _e103);
        color = vec4<f32>(0.5f, 0.5f, 0.55f, 0.5f);
    } else {
        let _e106 = p.pos;
        let _e108 = unnamed_1.viewLeft;
        let _e110 = sx;
        let _e115 = unnamed_1.viewUp;
        let _e117 = uy;
        worldPos = ((_e106 + (_e108.xyz * (_e110 * 1.5f))) + (_e115.xyz * (_e117 * 1.5f)));
        color = vec4<f32>(1f, 1f, 1f, 0.8f);
    }
    let _e122 = unnamed_1.mvp;
    let _e123 = worldPos;
    unnamed.gl_Position = (_e122 * vec4<f32>(_e123.x, _e123.y, _e123.z, 1f));
    let _e130 = vertInQuad;
    indexable_2 = array<vec2<f32>, 6>(vec2<f32>(0f, 0f), vec2<f32>(0f, 1f), vec2<f32>(1f, 0f), vec2<f32>(1f, 0f), vec2<f32>(0f, 1f), vec2<f32>(1f, 1f));
    let _e132 = indexable_2[_e130];
    fragUV = _e132;
    let _e133 = color;
    fragColor = _e133;
    return;
}

@vertex
fn main(@builtin(instance_index) gl_InstanceIndex: u32, @builtin(vertex_index) gl_VertexIndex: u32) -> VertexOutput {
    gl_InstanceIndex_1 = i32(gl_InstanceIndex);
    gl_VertexIndex_1 = i32(gl_VertexIndex);
    main_1();
    let _e11 = unnamed.gl_Position.y;
    unnamed.gl_Position.y = -(_e11);
    let _e13 = unnamed.gl_Position;
    let _e14 = fragUV;
    let _e15 = fragColor;
    return VertexOutput(_e13, _e14, _e15);
}
