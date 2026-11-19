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
    @location(2) @interpolate(flat) member_2: u32,
}

var<private> unnamed: gl_PerVertex = gl_PerVertex(vec4<f32>(0f, 0f, 0f, 1f));
var<private> fragUV: vec2<f32>;
var<private> fragColor: vec4<f32>;
var<private> fragFamily: u32;
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
    fragFamily = 0u;
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
    var phi_71_: bool;
    var phi_72_: bool;

    let _e98 = gl_InstanceIndex_1;
    idx = bitcast<u32>(_e98);
    let _e100 = gl_VertexIndex_1;
    vertInQuad = (bitcast<u32>(_e100) % 6u);
    let _e103 = idx;
    let _e105 = unnamed_1.poolSize;
    let _e106 = (_e103 >= _e105);
    phi_72_ = _e106;
    if !(_e106) {
        let _e109 = unnamed_1.atmType;
        let _e110 = (_e109 == 0u);
        phi_71_ = _e110;
        if _e110 {
            let _e111 = idx;
            let _e114 = unnamed_1.effectMeta[0u];
            phi_71_ = (_e111 < u32(_e114));
        }
        let _e118 = phi_71_;
        phi_72_ = _e118;
    }
    let _e120 = phi_72_;
    if _e120 {
        emitDegenerate_u0028_();
        return;
    }
    let _e121 = idx;
    let _e124 = unnamed_2.particles[_e121];
    p.pos = _e124.pos;
    p.seed = _e124.seed;
    p.vel = _e124.vel;
    p.flags = _e124.flags;
    let _e134 = p.flags;
    if (_e134 < 0.5f) {
        emitDegenerate_u0028_();
        return;
    }
    let _e136 = vertInQuad;
    indexable = array<f32, 6>(-1f, -1f, 1f, 1f, -1f, 1f);
    let _e138 = indexable[_e136];
    sx = _e138;
    let _e139 = vertInQuad;
    indexable_1 = array<f32, 6>(1f, -1f, 1f, 1f, -1f, -1f);
    let _e141 = indexable_1[_e139];
    uy = _e141;
    let _e143 = p.flags;
    family = u32((_e143 + 0.5f));
    let _e146 = family;
    if (_e146 >= 16u) {
        let _e148 = family;
        effect = (_e148 - 16u);
        let _e150 = effect;
        let _e153 = unnamed_1.effectMeta[1u];
        if (_e150 >= min(u32(_e153), 8u)) {
            emitDegenerate_u0028_();
            return;
        }
        let _e157 = effect;
        base = (_e157 * 6u);
        let _e159 = base;
        let _e164 = unnamed_1.effectWorkloads[(_e159 + 5u)][0u];
        let _e165 = base;
        let _e170 = unnamed_1.effectWorkloads[(_e165 + 5u)][1u];
        lifetime = max(0.05f, (_e164 + _e170));
        let _e174 = p.seed;
        let _e175 = lifetime;
        age = (1f - clamp((_e174 / _e175), 0f, 1f));
        let _e179 = base;
        let _e184 = unnamed_1.effectWorkloads[(_e179 + 4u)][2u];
        let _e185 = base;
        let _e190 = unnamed_1.effectWorkloads[(_e185 + 4u)][3u];
        let _e191 = age;
        size = mix(_e184, _e190, _e191);
        let _e194 = p.pos;
        let _e196 = unnamed_1.viewLeft;
        let _e198 = sx;
        let _e199 = size;
        let _e204 = unnamed_1.viewUp;
        let _e206 = uy;
        let _e207 = size;
        worldPos = ((_e194 + (_e196.xyz * (_e198 * _e199))) + (_e204.xyz * (_e206 * _e207)));
        let _e211 = base;
        let _e215 = unnamed_1.effectWorkloads[(_e211 + 2u)];
        color = _e215;
        let _e216 = base;
        let _e221 = unnamed_1.effectWorkloads[(_e216 + 1u)][3u];
        let _e223 = color[3u];
        color[3u] = (_e223 * _e221);
    } else {
        let _e226 = family;
        let _e228 = family;
        if ((_e226 == 1u) || (_e228 == 3u)) {
            let _e232 = unnamed_1.viewLeft;
            let _e234 = sx;
            right = (_e232.xyz * (_e234 * 0.6f));
            let _e237 = family;
            length_ = select(9.9f, 18f, (_e237 == 1u));
            let _e240 = uy;
            let _e244 = length_;
            down = ((vec3<f32>(0f, 0f, 1f) * ((_e240 * 0.5f) - 0.5f)) * _e244);
            let _e247 = p.pos;
            let _e248 = right;
            let _e250 = down;
            worldPos = ((_e247 + _e248) + _e250);
            let _e252 = family;
            color = select(vec4<f32>(0.72f, 0.76f, 0.8f, 0.65f), vec4<f32>(0.5f, 0.5f, 0.55f, 0.5f), vec4((_e252 == 1u)));
        } else {
            let _e256 = family;
            if (_e256 == 4u) {
                local = 0.8f;
            } else {
                let _e258 = family;
                local = select(1.5f, 1.2f, (_e258 == 5u));
            }
            let _e261 = local;
            size_1 = _e261;
            let _e263 = p.pos;
            let _e265 = unnamed_1.viewLeft;
            let _e267 = sx;
            let _e268 = size_1;
            let _e273 = unnamed_1.viewUp;
            let _e275 = uy;
            let _e276 = size_1;
            worldPos = ((_e263 + (_e265.xyz * (_e267 * _e268))) + (_e273.xyz * (_e275 * _e276)));
            let _e280 = family;
            if (_e280 == 4u) {
                local_1 = vec4<f32>(0.82f, 0.88f, 0.92f, 0.85f);
            } else {
                let _e282 = family;
                local_1 = select(vec4<f32>(1f, 1f, 1f, 0.8f), vec4<f32>(0.48f, 0.42f, 0.34f, 0.45f), vec4((_e282 == 5u)));
            }
            let _e286 = local_1;
            color = _e286;
        }
    }
    let _e289 = unnamed_1.ambientCloud[3u];
    let _e292 = unnamed_1.cloudMedia[0u];
    cloudTransmission = (1f - clamp((_e289 * _e292), 0f, 1f));
    let _e297 = unnamed_1.ambientCloud;
    let _e301 = unnamed_1.sun[3u];
    let _e303 = cloudTransmission;
    let _e309 = unnamed_1.moon[3u];
    let _e316 = unnamed_1.cloudMedia[1u];
    weatherLight = (((_e297.xyz + vec3((max(_e301, 0f) * _e303))) + vec3((max(_e309, 0f) * 0.2f))) + vec3(max(_e316, 0f)));
    let _e320 = weatherLight;
    let _e321 = weatherLight;
    if (dot(_e320, _e321) > 0.00000001f) {
        let _e324 = weatherLight;
        let _e326 = color;
        let _e328 = (_e326.xyz * clamp(_e324, vec3<f32>(0.04f, 0.04f, 0.04f), vec3<f32>(4f, 4f, 4f)));
        color[0u] = _e328.x;
        color[1u] = _e328.y;
        color[2u] = _e328.z;
    }
    let _e336 = unnamed_1.mvp;
    let _e337 = worldPos;
    unnamed.gl_Position = (_e336 * vec4<f32>(_e337.x, _e337.y, _e337.z, 1f));
    let _e344 = vertInQuad;
    indexable_2 = array<vec2<f32>, 6>(vec2<f32>(0f, 0f), vec2<f32>(0f, 1f), vec2<f32>(1f, 0f), vec2<f32>(1f, 0f), vec2<f32>(0f, 1f), vec2<f32>(1f, 1f));
    let _e346 = indexable_2[_e344];
    fragUV = _e346;
    let _e347 = color;
    fragColor = _e347;
    let _e348 = family;
    fragFamily = _e348;
    return;
}

@vertex
fn main(@builtin(instance_index) gl_InstanceIndex: u32, @builtin(vertex_index) gl_VertexIndex: u32) -> VertexOutput {
    gl_InstanceIndex_1 = i32(gl_InstanceIndex);
    gl_VertexIndex_1 = i32(gl_VertexIndex);
    main_1();
    let _e12 = unnamed.gl_Position.y;
    unnamed.gl_Position.y = -(_e12);
    let _e14 = unnamed.gl_Position;
    let _e15 = fragUV;
    let _e16 = fragColor;
    let _e17 = fragFamily;
    return VertexOutput(_e14, _e15, _e16, _e17);
}
