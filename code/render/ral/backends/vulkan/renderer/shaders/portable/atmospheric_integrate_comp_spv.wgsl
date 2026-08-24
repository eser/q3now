struct AtmParticle {
    pos: vec3<f32>,
    seed: f32,
    vel: vec3<f32>,
    flags: f32,
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
    pad0_: f32,
    pad1_: f32,
    pad2_: f32,
}

struct WritePool {
    writeParticles: array<AtmParticle>,
}

struct ReadPool {
    readParticles: array<AtmParticle>,
}

@group(0) @binding(0)
var<uniform> unnamed: AtmFrame;
@group(0) @binding(3)
var heightgrid: texture_2d<f32>;
@group(0) @binding(35)
var heightgrid_sampler: sampler;
var<private> gl_GlobalInvocationID_1: vec3<u32>;
@group(0) @binding(2)
var<storage, read_write> unnamed_1: WritePool;
@group(0) @binding(1)
var<storage> unnamed_2: ReadPool;

fn groundHeightAt_u0028_vf2_u003b(worldXY: ptr<function, vec2<f32>>) -> f32 {
    var cell: vec2<f32>;
    var uv: vec2<f32>;

    let _e55 = (*worldXY);
    let _e57 = unnamed.worldMins;
    let _e60 = unnamed.invGridStep;
    cell = ((_e55 - _e57) * _e60);
    let _e62 = cell;
    let _e66 = unnamed.gridSize;
    uv = ((_e62 + vec2(0.5f)) / vec2(f32(_e66)));
    let _e70 = uv;
    let _e71 = textureSampleLevel(heightgrid, heightgrid_sampler, _e70, 0f);
    return _e71.x;
}

fn hash11_u0028_u1_u003b(x: ptr<function, u32>) -> f32 {
    let _e53 = (*x);
    let _e56 = (*x);
    (*x) = (_e56 ^ (_e53 >> bitcast<u32>(16i)));
    let _e58 = (*x);
    (*x) = (_e58 * 2146121005u);
    let _e60 = (*x);
    let _e63 = (*x);
    (*x) = (_e63 ^ (_e60 >> bitcast<u32>(15i)));
    let _e65 = (*x);
    (*x) = (_e65 * 2221713035u);
    let _e67 = (*x);
    let _e70 = (*x);
    (*x) = (_e70 ^ (_e67 >> bitcast<u32>(16i)));
    let _e72 = (*x);
    return (f32(_e72) * 0.00000000023283064f);
}

fn rand01_u0028_u1_u003b_u1_u003b_f1_u003b(idx: ptr<function, u32>, salt: ptr<function, u32>, seed: ptr<function, f32>) -> f32 {
    var t: u32;
    var param: u32;

    let _e58 = unnamed.time;
    let _e61 = (*seed);
    t = (u32((_e58 * 1000f)) + u32((_e61 * 4096f)));
    let _e65 = (*idx);
    let _e67 = (*salt);
    let _e70 = t;
    param = (((_e65 * 747796405u) + (_e67 * 2891336453u)) + _e70);
    let _e72 = hash11_u0028_u1_u003b((&param));
    return _e72;
}

fn deadParticle_u0028_() -> AtmParticle {
    var p: AtmParticle;

    p.pos = vec3<f32>(0f, 0f, 0f);
    p.seed = 0f;
    p.vel = vec3<f32>(0f, 0f, 0f);
    p.flags = 0f;
    let _e57 = p;
    return _e57;
}

fn main_1() {
    var idx_1: u32;
    var p_1: AtmParticle;
    var rx: f32;
    var param_1: u32;
    var param_2: u32;
    var param_3: f32;
    var ry: f32;
    var param_4: u32;
    var param_5: u32;
    var param_6: f32;
    var rz: f32;
    var param_7: u32;
    var param_8: u32;
    var param_9: f32;
    var spread: f32;
    var param_10: u32;
    var param_11: u32;
    var param_12: f32;
    var param_13: u32;
    var param_14: u32;
    var param_15: f32;
    var param_16: u32;
    var param_17: u32;
    var param_18: f32;
    var param_19: u32;
    var param_20: u32;
    var param_21: f32;
    var ground: f32;
    var param_22: vec2<f32>;
    var d: vec2<f32>;

    let _e83 = gl_GlobalInvocationID_1[0u];
    idx_1 = _e83;
    let _e84 = idx_1;
    let _e86 = unnamed.poolSize;
    if (_e84 >= _e86) {
        return;
    }
    let _e89 = unnamed.atmType;
    if (_e89 == 0u) {
        let _e91 = idx_1;
        let _e92 = deadParticle_u0028_();
        unnamed_1.writeParticles[_e91].pos = _e92.pos;
        unnamed_1.writeParticles[_e91].seed = _e92.seed;
        unnamed_1.writeParticles[_e91].vel = _e92.vel;
        unnamed_1.writeParticles[_e91].flags = _e92.flags;
        return;
    }
    let _e103 = idx_1;
    let _e106 = unnamed_2.readParticles[_e103];
    p_1.pos = _e106.pos;
    p_1.seed = _e106.seed;
    p_1.vel = _e106.vel;
    p_1.flags = _e106.flags;
    let _e116 = p_1.flags;
    if (_e116 < 0.5f) {
        let _e118 = idx_1;
        param_1 = _e118;
        param_2 = 11u;
        let _e120 = p_1.seed;
        param_3 = _e120;
        let _e121 = rand01_u0028_u1_u003b_u1_u003b_f1_u003b((&param_1), (&param_2), (&param_3));
        rx = _e121;
        let _e122 = idx_1;
        param_4 = _e122;
        param_5 = 23u;
        let _e124 = p_1.seed;
        param_6 = _e124;
        let _e125 = rand01_u0028_u1_u003b_u1_u003b_f1_u003b((&param_4), (&param_5), (&param_6));
        ry = _e125;
        let _e126 = idx_1;
        param_7 = _e126;
        param_8 = 31u;
        let _e128 = p_1.seed;
        param_9 = _e128;
        let _e129 = rand01_u0028_u1_u003b_u1_u003b_f1_u003b((&param_7), (&param_8), (&param_9));
        rz = _e129;
        let _e131 = unnamed.distance;
        spread = _e131;
        let _e134 = unnamed.eyeWorld[0u];
        let _e135 = rx;
        let _e138 = spread;
        let _e143 = unnamed.boundsMin[0u];
        let _e146 = unnamed.boundsMax[0u];
        p_1.pos[0u] = clamp((_e134 + (((_e135 * 2f) - 1f) * _e138)), _e143, _e146);
        let _e152 = unnamed.eyeWorld[1u];
        let _e153 = ry;
        let _e156 = spread;
        let _e161 = unnamed.boundsMin[1u];
        let _e164 = unnamed.boundsMax[1u];
        p_1.pos[1u] = clamp((_e152 + (((_e153 * 2f) - 1f) * _e156)), _e161, _e164);
        let _e170 = unnamed.eyeWorld[2u];
        let _e171 = rz;
        let _e176 = unnamed.boundsMax[2u];
        p_1.pos[2u] = min((_e170 + (_e171 * 800f)), _e176);
        let _e180 = rx;
        let _e181 = ry;
        let _e183 = rz;
        p_1.seed = ((_e180 + _e181) + _e183);
        let _e187 = unnamed.atmType;
        if (_e187 == 1u) {
            let _e189 = idx_1;
            param_10 = _e189;
            param_11 = 41u;
            let _e191 = p_1.seed;
            param_12 = _e191;
            let _e192 = rand01_u0028_u1_u003b_u1_u003b_f1_u003b((&param_10), (&param_11), (&param_12));
            p_1.vel[0u] = (((_e192 * 2f) - 1f) * 30f);
            let _e198 = idx_1;
            param_13 = _e198;
            param_14 = 53u;
            let _e200 = p_1.seed;
            param_15 = _e200;
            let _e201 = rand01_u0028_u1_u003b_u1_u003b_f1_u003b((&param_13), (&param_14), (&param_15));
            p_1.vel[1u] = (((_e201 * 2f) - 1f) * 30f);
            p_1.vel[2u] = -880f;
        } else {
            let _e209 = idx_1;
            param_16 = _e209;
            param_17 = 41u;
            let _e211 = p_1.seed;
            param_18 = _e211;
            let _e212 = rand01_u0028_u1_u003b_u1_u003b_f1_u003b((&param_16), (&param_17), (&param_18));
            p_1.vel[0u] = (((_e212 * 2f) - 1f) * 20f);
            let _e218 = idx_1;
            param_19 = _e218;
            param_20 = 53u;
            let _e220 = p_1.seed;
            param_21 = _e220;
            let _e221 = rand01_u0028_u1_u003b_u1_u003b_f1_u003b((&param_19), (&param_20), (&param_21));
            p_1.vel[1u] = (((_e221 * 2f) - 1f) * 20f);
            p_1.vel[2u] = -160f;
        }
        p_1.flags = 1f;
        let _e230 = idx_1;
        let _e231 = p_1;
        unnamed_1.writeParticles[_e230].pos = _e231.pos;
        unnamed_1.writeParticles[_e230].seed = _e231.seed;
        unnamed_1.writeParticles[_e230].vel = _e231.vel;
        unnamed_1.writeParticles[_e230].flags = _e231.flags;
        return;
    }
    let _e243 = p_1.vel;
    let _e245 = unnamed.dt;
    let _e248 = p_1.pos;
    p_1.pos = (_e248 + (_e243 * _e245));
    let _e252 = unnamed.atmType;
    if (_e252 == 2u) {
        let _e255 = unnamed.time;
        let _e256 = idx_1;
        let _e263 = p_1.pos[0u];
        p_1.pos[0u] = (_e263 + (sin((_e255 + f32(_e256))) * 0.5f));
        let _e268 = unnamed.time;
        let _e270 = idx_1;
        let _e277 = p_1.pos[1u];
        p_1.pos[1u] = (_e277 + (cos(((_e268 * 1.3f) + f32(_e270))) * 0.5f));
    }
    let _e282 = p_1.pos;
    param_22 = _e282.xy;
    let _e284 = groundHeightAt_u0028_vf2_u003b((&param_22));
    ground = _e284;
    let _e287 = p_1.pos[2u];
    let _e288 = ground;
    if (_e287 < _e288) {
        let _e290 = idx_1;
        let _e291 = deadParticle_u0028_();
        unnamed_1.writeParticles[_e290].pos = _e291.pos;
        unnamed_1.writeParticles[_e290].seed = _e291.seed;
        unnamed_1.writeParticles[_e290].vel = _e291.vel;
        unnamed_1.writeParticles[_e290].flags = _e291.flags;
        return;
    }
    let _e303 = p_1.pos;
    let _e306 = unnamed.eyeWorld;
    d = (_e303.xy - _e306.xy);
    let _e309 = d;
    let _e310 = d;
    let _e313 = unnamed.distance;
    let _e315 = unnamed.distance;
    if (dot(_e309, _e310) > (_e313 * _e315)) {
        let _e318 = idx_1;
        let _e319 = deadParticle_u0028_();
        unnamed_1.writeParticles[_e318].pos = _e319.pos;
        unnamed_1.writeParticles[_e318].seed = _e319.seed;
        unnamed_1.writeParticles[_e318].vel = _e319.vel;
        unnamed_1.writeParticles[_e318].flags = _e319.flags;
        return;
    }
    let _e332 = p_1.pos[2u];
    let _e335 = unnamed.eyeWorld[2u];
    if (_e332 < (_e335 - 500f)) {
        let _e338 = idx_1;
        let _e339 = deadParticle_u0028_();
        unnamed_1.writeParticles[_e338].pos = _e339.pos;
        unnamed_1.writeParticles[_e338].seed = _e339.seed;
        unnamed_1.writeParticles[_e338].vel = _e339.vel;
        unnamed_1.writeParticles[_e338].flags = _e339.flags;
        return;
    }
    let _e350 = idx_1;
    let _e351 = p_1;
    unnamed_1.writeParticles[_e350].pos = _e351.pos;
    unnamed_1.writeParticles[_e350].seed = _e351.seed;
    unnamed_1.writeParticles[_e350].vel = _e351.vel;
    unnamed_1.writeParticles[_e350].flags = _e351.flags;
    return;
}

@compute @workgroup_size(64, 1, 1)
fn main(@builtin(global_invocation_id) gl_GlobalInvocationID: vec3<u32>) {
    gl_GlobalInvocationID_1 = gl_GlobalInvocationID;
    main_1();
}
