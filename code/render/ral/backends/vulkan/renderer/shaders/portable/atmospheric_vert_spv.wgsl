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
    computePad0_: f32,
    computePad1_: f32,
    computePad2_: f32,
    windGust: vec4<f32>,
    precipitation: vec4<f32>,
    dustAsh: f32,
    indoorExposure: f32,
    climateSeed: u32,
    climatePad: u32,
    climate: vec4<f32>,
    surfaceClimate: vec4<f32>,
    sun: vec4<f32>,
    moon: vec4<f32>,
    ambientCloud: vec4<f32>,
    cloudMedia: vec4<f32>,
    effectMeta: vec4<f32>,
    effectWorkloads: array<vec4<f32>, 48>,
    renderParams: vec4<f32>,
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
    var family: u32;
    var effect: u32;
    var base: u32;
    var lifetime: f32;
    var age: f32;
    var size: f32;
    var worldPos: vec3<f32>;
    var color: vec4<f32>;
    var right: vec3<f32>;
    var length_: f32;
    var down: vec3<f32>;
    var size_1: f32;
    var local: f32;
    var local_1: vec4<f32>;
    var cloudTransmission: f32;
    var weatherLight: vec3<f32>;
    var indexable_2: array<vec2<f32>, 6>;
    var phi_69_: bool;
    var phi_70_: bool;

    let _e97 = gl_InstanceIndex_1;
    idx = bitcast<u32>(_e97);
    let _e99 = gl_VertexIndex_1;
    vertInQuad = (bitcast<u32>(_e99) % 6u);
    let _e102 = idx;
    let _e104 = unnamed_1.poolSize;
    let _e105 = (_e102 >= _e104);
    phi_70_ = _e105;
    if !(_e105) {
        let _e108 = unnamed_1.atmType;
        let _e109 = (_e108 == 0u);
        phi_69_ = _e109;
        if _e109 {
            let _e110 = idx;
            let _e113 = unnamed_1.effectMeta[0u];
            phi_69_ = (_e110 < u32(_e113));
        }
        let _e117 = phi_69_;
        phi_70_ = _e117;
    }
    let _e119 = phi_70_;
    if _e119 {
        emitDegenerate_u0028_();
        return;
    }
    let _e120 = idx;
    let _e123 = unnamed_2.particles[_e120];
    p.pos = _e123.pos;
    p.seed = _e123.seed;
    p.vel = _e123.vel;
    p.flags = _e123.flags;
    let _e133 = p.flags;
    if (_e133 < 0.5f) {
        emitDegenerate_u0028_();
        return;
    }
    let _e135 = vertInQuad;
    indexable = array<f32, 6>(-1f, -1f, 1f, 1f, -1f, 1f);
    let _e137 = indexable[_e135];
    sx = _e137;
    let _e138 = vertInQuad;
    indexable_1 = array<f32, 6>(1f, -1f, 1f, 1f, -1f, -1f);
    let _e140 = indexable_1[_e138];
    uy = _e140;
    let _e142 = p.flags;
    family = u32((_e142 + 0.5f));
    let _e145 = family;
    if (_e145 >= 16u) {
        let _e147 = family;
        effect = (_e147 - 16u);
        let _e149 = effect;
        let _e152 = unnamed_1.effectMeta[1u];
        if (_e149 >= min(u32(_e152), 8u)) {
            emitDegenerate_u0028_();
            return;
        }
        let _e156 = effect;
        base = (_e156 * 6u);
        let _e158 = base;
        let _e163 = unnamed_1.effectWorkloads[(_e158 + 5u)][0u];
        let _e164 = base;
        let _e169 = unnamed_1.effectWorkloads[(_e164 + 5u)][1u];
        lifetime = max(0.05f, (_e163 + _e169));
        let _e173 = p.seed;
        let _e174 = lifetime;
        age = (1f - clamp((_e173 / _e174), 0f, 1f));
        let _e178 = base;
        let _e183 = unnamed_1.effectWorkloads[(_e178 + 4u)][2u];
        let _e184 = base;
        let _e189 = unnamed_1.effectWorkloads[(_e184 + 4u)][3u];
        let _e190 = age;
        size = mix(_e183, _e189, _e190);
        let _e193 = p.pos;
        let _e195 = unnamed_1.viewLeft;
        let _e197 = sx;
        let _e198 = size;
        let _e203 = unnamed_1.viewUp;
        let _e205 = uy;
        let _e206 = size;
        worldPos = ((_e193 + (_e195.xyz * (_e197 * _e198))) + (_e203.xyz * (_e205 * _e206)));
        let _e210 = base;
        let _e214 = unnamed_1.effectWorkloads[(_e210 + 2u)];
        color = _e214;
        let _e215 = base;
        let _e220 = unnamed_1.effectWorkloads[(_e215 + 1u)][3u];
        let _e222 = color[3u];
        color[3u] = (_e222 * _e220);
    } else {
        let _e225 = family;
        let _e227 = family;
        if ((_e225 == 1u) || (_e227 == 3u)) {
            let _e231 = unnamed_1.viewLeft;
            let _e233 = sx;
            right = (_e231.xyz * (_e233 * 0.6f));
            let _e236 = family;
            length_ = select(9.9f, 18f, (_e236 == 1u));
            let _e239 = uy;
            let _e243 = length_;
            down = ((vec3<f32>(0f, 0f, 1f) * ((_e239 * 0.5f) - 0.5f)) * _e243);
            let _e246 = p.pos;
            let _e247 = right;
            let _e249 = down;
            worldPos = ((_e246 + _e247) + _e249);
            let _e251 = family;
            color = select(vec4<f32>(0.72f, 0.76f, 0.8f, 0.65f), vec4<f32>(0.5f, 0.5f, 0.55f, 0.5f), vec4((_e251 == 1u)));
        } else {
            let _e255 = family;
            if (_e255 == 4u) {
                local = 0.8f;
            } else {
                let _e257 = family;
                local = select(1.5f, 1.2f, (_e257 == 5u));
            }
            let _e260 = local;
            size_1 = _e260;
            let _e262 = p.pos;
            let _e264 = unnamed_1.viewLeft;
            let _e266 = sx;
            let _e267 = size_1;
            let _e272 = unnamed_1.viewUp;
            let _e274 = uy;
            let _e275 = size_1;
            worldPos = ((_e262 + (_e264.xyz * (_e266 * _e267))) + (_e272.xyz * (_e274 * _e275)));
            let _e279 = family;
            if (_e279 == 4u) {
                local_1 = vec4<f32>(0.82f, 0.88f, 0.92f, 0.85f);
            } else {
                let _e281 = family;
                local_1 = select(vec4<f32>(1f, 1f, 1f, 0.8f), vec4<f32>(0.48f, 0.42f, 0.34f, 0.45f), vec4((_e281 == 5u)));
            }
            let _e285 = local_1;
            color = _e285;
        }
    }
    let _e288 = unnamed_1.ambientCloud[3u];
    let _e291 = unnamed_1.cloudMedia[0u];
    cloudTransmission = (1f - clamp((_e288 * _e291), 0f, 1f));
    let _e296 = unnamed_1.ambientCloud;
    let _e300 = unnamed_1.sun[3u];
    let _e302 = cloudTransmission;
    let _e308 = unnamed_1.moon[3u];
    let _e315 = unnamed_1.cloudMedia[1u];
    weatherLight = (((_e296.xyz + vec3((max(_e300, 0f) * _e302))) + vec3((max(_e308, 0f) * 0.2f))) + vec3(max(_e315, 0f)));
    let _e319 = weatherLight;
    let _e320 = weatherLight;
    if (dot(_e319, _e320) > 0.00000001f) {
        let _e323 = weatherLight;
        let _e325 = color;
        let _e327 = (_e325.xyz * clamp(_e323, vec3<f32>(0.04f, 0.04f, 0.04f), vec3<f32>(4f, 4f, 4f)));
        color[0u] = _e327.x;
        color[1u] = _e327.y;
        color[2u] = _e327.z;
    }
    let _e335 = unnamed_1.mvp;
    let _e336 = worldPos;
    unnamed.gl_Position = (_e335 * vec4<f32>(_e336.x, _e336.y, _e336.z, 1f));
    let _e343 = vertInQuad;
    indexable_2 = array<vec2<f32>, 6>(vec2<f32>(0f, 0f), vec2<f32>(0f, 1f), vec2<f32>(1f, 0f), vec2<f32>(1f, 0f), vec2<f32>(0f, 1f), vec2<f32>(1f, 1f));
    let _e345 = indexable_2[_e343];
    fragUV = _e345;
    let _e346 = color;
    fragColor = _e346;
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
