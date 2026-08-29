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

fn hash11_u0028_u1_u003b(x: ptr<function, u32>) -> f32 {
    let _e92 = (*x);
    let _e95 = (*x);
    (*x) = (_e95 ^ (_e92 >> bitcast<u32>(16i)));
    let _e97 = (*x);
    (*x) = (_e97 * 2146121005u);
    let _e99 = (*x);
    let _e102 = (*x);
    (*x) = (_e102 ^ (_e99 >> bitcast<u32>(15i)));
    let _e104 = (*x);
    (*x) = (_e104 * 2221713035u);
    let _e106 = (*x);
    let _e109 = (*x);
    (*x) = (_e109 ^ (_e106 >> bitcast<u32>(16i)));
    let _e111 = (*x);
    return (f32(_e111) * 0.00000000023283064f);
}

fn rand01_u0028_u1_u003b_u1_u003b_f1_u003b(idx: ptr<function, u32>, salt: ptr<function, u32>, seed: ptr<function, f32>) -> f32 {
    var t: u32;
    var param: u32;

    let _e97 = unnamed.time;
    let _e100 = (*seed);
    let _e105 = unnamed.climateSeed;
    t = ((u32((_e97 * 1000f)) + u32((_e100 * 4096f))) + (_e105 * 2246822519u));
    let _e108 = (*idx);
    let _e110 = (*salt);
    let _e113 = t;
    param = (((_e108 * 747796405u) + (_e110 * 2891336453u)) + _e113);
    let _e115 = hash11_u0028_u1_u003b((&param));
    return _e115;
}

fn chooseFamily_u0028_u1_u003b_f1_u003b(idx_1: ptr<function, u32>, seed_1: ptr<function, f32>) -> u32 {
    var total: f32;
    var pick: f32;
    var param_1: u32;
    var param_2: u32;
    var param_3: f32;

    let _e100 = unnamed.precipitation[0u];
    let _e103 = unnamed.precipitation[1u];
    let _e107 = unnamed.precipitation[2u];
    let _e111 = unnamed.precipitation[3u];
    let _e114 = unnamed.dustAsh;
    total = ((((_e100 + _e103) + _e107) + _e111) + _e114);
    let _e116 = total;
    if (_e116 <= 0f) {
        let _e119 = unnamed.atmType;
        return _e119;
    }
    let _e120 = (*idx_1);
    param_1 = _e120;
    param_2 = 67u;
    let _e121 = (*seed_1);
    param_3 = _e121;
    let _e122 = rand01_u0028_u1_u003b_u1_u003b_f1_u003b((&param_1), (&param_2), (&param_3));
    let _e123 = total;
    pick = (_e122 * _e123);
    let _e125 = pick;
    let _e128 = unnamed.precipitation[0u];
    if (_e125 <= _e128) {
        return 1u;
    }
    let _e132 = unnamed.precipitation[0u];
    let _e133 = pick;
    pick = (_e133 - _e132);
    let _e135 = pick;
    let _e138 = unnamed.precipitation[1u];
    if (_e135 <= _e138) {
        return 2u;
    }
    let _e142 = unnamed.precipitation[1u];
    let _e143 = pick;
    pick = (_e143 - _e142);
    let _e145 = pick;
    let _e148 = unnamed.precipitation[2u];
    if (_e145 <= _e148) {
        return 3u;
    }
    let _e152 = unnamed.precipitation[2u];
    let _e153 = pick;
    pick = (_e153 - _e152);
    let _e155 = pick;
    let _e158 = unnamed.precipitation[3u];
    if (_e155 <= _e158) {
        return 4u;
    }
    return 5u;
}

fn groundHeightAt_u0028_vf2_u003b(worldXY: ptr<function, vec2<f32>>) -> f32 {
    var cell: vec2<f32>;
    var uv: vec2<f32>;

    let _e94 = (*worldXY);
    let _e96 = unnamed.worldMins;
    let _e99 = unnamed.invGridStep;
    cell = ((_e94 - _e96) * _e99);
    let _e101 = cell;
    let _e105 = unnamed.gridSize;
    uv = ((_e101 + vec2(0.5f)) / vec2(f32(_e105)));
    let _e109 = uv;
    let _e110 = textureSampleLevel(heightgrid, heightgrid_sampler, _e109, 0f);
    return _e110.x;
}

fn precipitationCoverage_u0028_u1_u003b(idx_2: ptr<function, u32>) -> f32 {
    var authored: f32;
    var intensity: f32;
    var local: f32;
    var coverage: f32;
    var slot: f32;
    var param_4: u32;

    let _e100 = unnamed.precipitation[0u];
    let _e103 = unnamed.precipitation[1u];
    let _e107 = unnamed.precipitation[2u];
    let _e111 = unnamed.precipitation[3u];
    let _e114 = unnamed.dustAsh;
    authored = ((((_e100 + _e103) + _e107) + _e111) + _e114);
    let _e116 = authored;
    if (_e116 > 0f) {
        let _e118 = authored;
        local = clamp(_e118, 0f, 1f);
    } else {
        local = 1f;
    }
    let _e120 = local;
    intensity = _e120;
    let _e121 = intensity;
    let _e123 = unnamed.indoorExposure;
    coverage = (_e121 * clamp(_e123, 0f, 1f));
    let _e126 = (*idx_2);
    let _e129 = unnamed.climateSeed;
    param_4 = ((_e126 * 1597334677u) + (_e129 * 3812015801u));
    let _e132 = hash11_u0028_u1_u003b((&param_4));
    slot = _e132;
    let _e133 = slot;
    let _e134 = coverage;
    let _e136 = coverage;
    return select(0f, _e136, (_e133 <= _e134));
}

fn deadParticle_u0028_() -> AtmParticle {
    var p: AtmParticle;

    p.pos = vec3<f32>(0f, 0f, 0f);
    p.seed = 0f;
    p.vel = vec3<f32>(0f, 0f, 0f);
    p.flags = 0f;
    let _e96 = p;
    return _e96;
}

fn effectForIndex_u0028_u1_u003b(idx_3: ptr<function, u32>) -> u32 {
    var count: u32;
    var effect: u32;
    var base: u32;
    var phi_330_: bool;

    let _e97 = unnamed.effectMeta[1u];
    count = min(u32(_e97), 8u);
    effect = 0u;
    loop {
        let _e100 = effect;
        let _e101 = count;
        if (_e100 < _e101) {
            let _e103 = effect;
            base = (_e103 * 6u);
            let _e105 = (*idx_3);
            let _e107 = base;
            let _e112 = unnamed.effectWorkloads[(_e107 + 4u)][0u];
            let _e113 = (f32(_e105) >= _e112);
            phi_330_ = _e113;
            if _e113 {
                let _e114 = (*idx_3);
                let _e116 = base;
                let _e121 = unnamed.effectWorkloads[(_e116 + 4u)][1u];
                phi_330_ = (f32(_e114) < _e121);
            }
            let _e124 = phi_330_;
            if _e124 {
                let _e125 = effect;
                return _e125;
            }
            continue;
        } else {
            break;
        }
        continuing {
            let _e126 = effect;
            effect = (_e126 + bitcast<u32>(1i));
        }
    }
    return 4294967295u;
}

fn main_1() {
    var idx_4: u32;
    var precipitationCount: u32;
    var semantic: bool;
    var effect_1: u32;
    var param_5: u32;
    var param_6: u32;
    var p_1: AtmParticle;
    var base_1: u32;
    var expectedFamily: u32;
    var stale: bool;
    var semanticSeed: u32;
    var rx: f32;
    var param_7: u32;
    var ry: f32;
    var param_8: u32;
    var rz: f32;
    var param_9: u32;
    var radius: f32;
    var param_10: u32;
    var authoredDirection: vec3<f32>;
    var direction: vec3<f32>;
    var local_1: vec3<f32>;
    var jitter: vec3<f32>;
    var param_11: u32;
    var param_12: u32;
    var param_13: u32;
    var rx_1: f32;
    var param_14: u32;
    var param_15: u32;
    var param_16: f32;
    var ry_1: f32;
    var param_17: u32;
    var param_18: u32;
    var param_19: f32;
    var rz_1: f32;
    var param_20: u32;
    var param_21: u32;
    var param_22: f32;
    var spread: f32;
    var param_23: vec2<f32>;
    var family: u32;
    var param_24: u32;
    var param_25: f32;
    var windScale: f32;
    var param_26: u32;
    var param_27: u32;
    var param_28: f32;
    var param_29: u32;
    var param_30: u32;
    var param_31: f32;
    var param_32: u32;
    var param_33: u32;
    var param_34: f32;
    var param_35: u32;
    var param_36: u32;
    var param_37: f32;
    var param_38: u32;
    var param_39: u32;
    var param_40: f32;
    var param_41: u32;
    var param_42: u32;
    var param_43: f32;
    var family_1: u32;
    var gustPhase: f32;
    var familyWind: f32;
    var local_2: f32;
    var local_3: f32;
    var local_4: f32;
    var ground: f32;
    var local_5: f32;
    var param_44: vec2<f32>;
    var d: vec2<f32>;
    var phi_400_: bool;
    var phi_423_: bool;
    var phi_756_: bool;

    let _e164 = gl_GlobalInvocationID_1[0u];
    idx_4 = _e164;
    let _e165 = idx_4;
    let _e167 = unnamed.poolSize;
    if (_e165 >= _e167) {
        return;
    }
    let _e171 = unnamed.effectMeta[0u];
    precipitationCount = u32(_e171);
    let _e173 = idx_4;
    let _e174 = precipitationCount;
    semantic = (_e173 >= _e174);
    let _e176 = idx_4;
    param_5 = _e176;
    let _e177 = effectForIndex_u0028_u1_u003b((&param_5));
    effect_1 = _e177;
    let _e178 = semantic;
    let _e179 = effect_1;
    if (_e178 && (_e179 == 4294967295u)) {
        let _e182 = idx_4;
        let _e183 = deadParticle_u0028_();
        unnamed_1.writeParticles[_e182].pos = _e183.pos;
        unnamed_1.writeParticles[_e182].seed = _e183.seed;
        unnamed_1.writeParticles[_e182].vel = _e183.vel;
        unnamed_1.writeParticles[_e182].flags = _e183.flags;
        return;
    }
    let _e194 = semantic;
    let _e195 = !(_e194);
    phi_400_ = _e195;
    if _e195 {
        let _e197 = unnamed.atmType;
        phi_400_ = (_e197 == 0u);
    }
    let _e200 = phi_400_;
    if _e200 {
        let _e201 = idx_4;
        let _e202 = deadParticle_u0028_();
        unnamed_1.writeParticles[_e201].pos = _e202.pos;
        unnamed_1.writeParticles[_e201].seed = _e202.seed;
        unnamed_1.writeParticles[_e201].vel = _e202.vel;
        unnamed_1.writeParticles[_e201].flags = _e202.flags;
        return;
    }
    let _e213 = semantic;
    let _e214 = !(_e213);
    phi_423_ = _e214;
    if _e214 {
        let _e215 = idx_4;
        param_6 = _e215;
        let _e216 = precipitationCoverage_u0028_u1_u003b((&param_6));
        phi_423_ = (_e216 <= 0f);
    }
    let _e219 = phi_423_;
    if _e219 {
        let _e220 = idx_4;
        let _e221 = deadParticle_u0028_();
        unnamed_1.writeParticles[_e220].pos = _e221.pos;
        unnamed_1.writeParticles[_e220].seed = _e221.seed;
        unnamed_1.writeParticles[_e220].vel = _e221.vel;
        unnamed_1.writeParticles[_e220].flags = _e221.flags;
        return;
    }
    let _e232 = idx_4;
    let _e235 = unnamed_2.readParticles[_e232];
    p_1.pos = _e235.pos;
    p_1.seed = _e235.seed;
    p_1.vel = _e235.vel;
    p_1.flags = _e235.flags;
    let _e244 = semantic;
    if _e244 {
        let _e245 = effect_1;
        base_1 = (_e245 * 6u);
        let _e247 = effect_1;
        expectedFamily = (16u + _e247);
        let _e250 = p_1.flags;
        let _e253 = expectedFamily;
        stale = (u32((_e250 + 0.5f)) != _e253);
        let _e256 = p_1.flags;
        let _e258 = stale;
        if ((_e256 < 0.5f) || _e258) {
            let _e260 = base_1;
            let _e265 = unnamed.effectWorkloads[(_e260 + 5u)][3u];
            let _e267 = idx_4;
            semanticSeed = (u32(_e265) + (_e267 * 13u));
            let _e270 = semanticSeed;
            param_7 = _e270;
            let _e271 = hash11_u0028_u1_u003b((&param_7));
            rx = _e271;
            let _e272 = semanticSeed;
            param_8 = (_e272 + 1u);
            let _e274 = hash11_u0028_u1_u003b((&param_8));
            ry = _e274;
            let _e275 = semanticSeed;
            param_9 = (_e275 + 2u);
            let _e277 = hash11_u0028_u1_u003b((&param_9));
            rz = _e277;
            let _e278 = base_1;
            let _e282 = unnamed.effectWorkloads[_e278][3u];
            let _e283 = base_1;
            let _e288 = unnamed.effectWorkloads[(_e283 + 5u)][2u];
            radius = max(_e282, _e288);
            let _e290 = base_1;
            let _e293 = unnamed.effectWorkloads[_e290];
            let _e295 = rx;
            let _e296 = ry;
            let _e297 = rz;
            let _e302 = radius;
            p_1.pos = (_e293.xyz + (((vec3<f32>(_e295, _e296, _e297) * 2f) - vec3(1f)) * _e302));
            let _e306 = base_1;
            let _e311 = unnamed.effectWorkloads[(_e306 + 5u)][0u];
            let _e312 = semanticSeed;
            param_10 = (_e312 + 3u);
            let _e314 = hash11_u0028_u1_u003b((&param_10));
            let _e317 = base_1;
            let _e322 = unnamed.effectWorkloads[(_e317 + 5u)][1u];
            p_1.seed = max(0.05f, (_e311 + (((_e314 * 2f) - 1f) * _e322)));
            let _e327 = base_1;
            let _e331 = unnamed.effectWorkloads[(_e327 + 1u)];
            authoredDirection = _e331.xyz;
            let _e333 = authoredDirection;
            let _e334 = authoredDirection;
            if (dot(_e333, _e334) > 0.00000001f) {
                let _e337 = authoredDirection;
                local_1 = normalize(_e337);
            } else {
                local_1 = vec3<f32>(0f, 0f, 1f);
            }
            let _e339 = local_1;
            direction = _e339;
            let _e340 = semanticSeed;
            param_11 = (_e340 + 4u);
            let _e342 = hash11_u0028_u1_u003b((&param_11));
            let _e343 = semanticSeed;
            param_12 = (_e343 + 5u);
            let _e345 = hash11_u0028_u1_u003b((&param_12));
            let _e346 = semanticSeed;
            param_13 = (_e346 + 6u);
            let _e348 = hash11_u0028_u1_u003b((&param_13));
            let _e353 = base_1;
            let _e358 = unnamed.effectWorkloads[(_e353 + 3u)][1u];
            jitter = (((vec3<f32>(_e342, _e345, _e348) * 2f) - vec3(1f)) * _e358);
            let _e360 = direction;
            let _e361 = base_1;
            let _e366 = unnamed.effectWorkloads[(_e361 + 3u)][0u];
            let _e368 = jitter;
            let _e371 = unnamed.windGust;
            p_1.vel = (((_e360 * _e366) + _e368) + (_e371.xyz * 0.1f));
            let _e376 = expectedFamily;
            p_1.flags = f32(_e376);
        } else {
            let _e379 = base_1;
            let _e384 = unnamed.effectWorkloads[(_e379 + 3u)][2u];
            let _e387 = unnamed.dt;
            let _e391 = p_1.vel[2u];
            p_1.vel[2u] = (_e391 - ((800f * _e384) * _e387));
            let _e395 = base_1;
            let _e400 = unnamed.effectWorkloads[(_e395 + 3u)][3u];
            let _e402 = unnamed.dt;
            let _e407 = p_1.vel;
            p_1.vel = (_e407 * max(0f, (1f - (_e400 * _e402))));
            let _e411 = p_1.vel;
            let _e413 = unnamed.dt;
            let _e416 = p_1.pos;
            p_1.pos = (_e416 + (_e411 * _e413));
            let _e420 = unnamed.dt;
            let _e422 = p_1.seed;
            p_1.seed = (_e422 - _e420);
            let _e426 = p_1.seed;
            if (_e426 <= 0f) {
                let _e428 = deadParticle_u0028_();
                p_1 = _e428;
            }
        }
        let _e429 = idx_4;
        let _e430 = p_1;
        unnamed_1.writeParticles[_e429].pos = _e430.pos;
        unnamed_1.writeParticles[_e429].seed = _e430.seed;
        unnamed_1.writeParticles[_e429].vel = _e430.vel;
        unnamed_1.writeParticles[_e429].flags = _e430.flags;
        return;
    }
    let _e442 = p_1.flags;
    if (_e442 < 0.5f) {
        let _e444 = idx_4;
        param_14 = _e444;
        param_15 = 11u;
        let _e446 = p_1.seed;
        param_16 = _e446;
        let _e447 = rand01_u0028_u1_u003b_u1_u003b_f1_u003b((&param_14), (&param_15), (&param_16));
        rx_1 = _e447;
        let _e448 = idx_4;
        param_17 = _e448;
        param_18 = 23u;
        let _e450 = p_1.seed;
        param_19 = _e450;
        let _e451 = rand01_u0028_u1_u003b_u1_u003b_f1_u003b((&param_17), (&param_18), (&param_19));
        ry_1 = _e451;
        let _e452 = idx_4;
        param_20 = _e452;
        param_21 = 31u;
        let _e454 = p_1.seed;
        param_22 = _e454;
        let _e455 = rand01_u0028_u1_u003b_u1_u003b_f1_u003b((&param_20), (&param_21), (&param_22));
        rz_1 = _e455;
        let _e457 = unnamed.distance;
        spread = _e457;
        let _e460 = unnamed.eyeWorld[0u];
        let _e461 = rx_1;
        let _e464 = spread;
        let _e469 = unnamed.boundsMin[0u];
        let _e472 = unnamed.boundsMax[0u];
        p_1.pos[0u] = clamp((_e460 + (((_e461 * 2f) - 1f) * _e464)), _e469, _e472);
        let _e478 = unnamed.eyeWorld[1u];
        let _e479 = ry_1;
        let _e482 = spread;
        let _e487 = unnamed.boundsMin[1u];
        let _e490 = unnamed.boundsMax[1u];
        p_1.pos[1u] = clamp((_e478 + (((_e479 * 2f) - 1f) * _e482)), _e487, _e490);
        let _e496 = unnamed.eyeWorld[2u];
        let _e497 = rz_1;
        let _e502 = unnamed.boundsMax[2u];
        p_1.pos[2u] = min((_e496 + (_e497 * 800f)), _e502);
        let _e507 = unnamed.gridSize;
        let _e508 = (_e507 > 1u);
        phi_756_ = _e508;
        if _e508 {
            let _e511 = p_1.pos[2u];
            let _e513 = p_1.pos;
            param_23 = _e513.xy;
            let _e515 = groundHeightAt_u0028_vf2_u003b((&param_23));
            phi_756_ = (_e511 <= _e515);
        }
        let _e518 = phi_756_;
        if _e518 {
            let _e519 = idx_4;
            let _e520 = deadParticle_u0028_();
            unnamed_1.writeParticles[_e519].pos = _e520.pos;
            unnamed_1.writeParticles[_e519].seed = _e520.seed;
            unnamed_1.writeParticles[_e519].vel = _e520.vel;
            unnamed_1.writeParticles[_e519].flags = _e520.flags;
            return;
        }
        let _e531 = rx_1;
        let _e532 = ry_1;
        let _e534 = rz_1;
        p_1.seed = ((_e531 + _e532) + _e534);
        let _e537 = idx_4;
        param_24 = _e537;
        let _e539 = p_1.seed;
        param_25 = _e539;
        let _e540 = chooseFamily_u0028_u1_u003b_f1_u003b((&param_24), (&param_25));
        family = _e540;
        windScale = 1f;
        let _e541 = family;
        if (_e541 == 1u) {
            let _e543 = idx_4;
            param_26 = _e543;
            param_27 = 41u;
            let _e545 = p_1.seed;
            param_28 = _e545;
            let _e546 = rand01_u0028_u1_u003b_u1_u003b_f1_u003b((&param_26), (&param_27), (&param_28));
            p_1.vel[0u] = (((_e546 * 2f) - 1f) * 30f);
            let _e552 = idx_4;
            param_29 = _e552;
            param_30 = 53u;
            let _e554 = p_1.seed;
            param_31 = _e554;
            let _e555 = rand01_u0028_u1_u003b_u1_u003b_f1_u003b((&param_29), (&param_30), (&param_31));
            p_1.vel[1u] = (((_e555 * 2f) - 1f) * 30f);
            p_1.vel[2u] = -880f;
            windScale = 0.25f;
        } else {
            let _e563 = family;
            if (_e563 == 2u) {
                let _e565 = idx_4;
                param_32 = _e565;
                param_33 = 41u;
                let _e567 = p_1.seed;
                param_34 = _e567;
                let _e568 = rand01_u0028_u1_u003b_u1_u003b_f1_u003b((&param_32), (&param_33), (&param_34));
                p_1.vel[0u] = (((_e568 * 2f) - 1f) * 20f);
                let _e574 = idx_4;
                param_35 = _e574;
                param_36 = 53u;
                let _e576 = p_1.seed;
                param_37 = _e576;
                let _e577 = rand01_u0028_u1_u003b_u1_u003b_f1_u003b((&param_35), (&param_36), (&param_37));
                p_1.vel[1u] = (((_e577 * 2f) - 1f) * 20f);
                p_1.vel[2u] = -160f;
            } else {
                let _e585 = family;
                if (_e585 == 3u) {
                    p_1.vel = vec3<f32>(0f, 0f, -600f);
                    windScale = 0.5f;
                } else {
                    let _e588 = family;
                    if (_e588 == 4u) {
                        p_1.vel = vec3<f32>(0f, 0f, -720f);
                        windScale = 0.15f;
                    } else {
                        let _e591 = idx_4;
                        param_38 = _e591;
                        param_39 = 41u;
                        let _e593 = p_1.seed;
                        param_40 = _e593;
                        let _e594 = rand01_u0028_u1_u003b_u1_u003b_f1_u003b((&param_38), (&param_39), (&param_40));
                        p_1.vel[0u] = (((_e594 * 2f) - 1f) * 35f);
                        let _e600 = idx_4;
                        param_41 = _e600;
                        param_42 = 53u;
                        let _e602 = p_1.seed;
                        param_43 = _e602;
                        let _e603 = rand01_u0028_u1_u003b_u1_u003b_f1_u003b((&param_41), (&param_42), (&param_43));
                        p_1.vel[1u] = (((_e603 * 2f) - 1f) * 35f);
                        p_1.vel[2u] = -32f;
                        windScale = 1.5f;
                    }
                }
            }
        }
        let _e612 = unnamed.windGust;
        let _e614 = windScale;
        let _e617 = p_1.vel;
        p_1.vel = (_e617 + (_e612.xyz * _e614));
        let _e620 = family;
        p_1.flags = f32(_e620);
        let _e623 = idx_4;
        let _e624 = p_1;
        unnamed_1.writeParticles[_e623].pos = _e624.pos;
        unnamed_1.writeParticles[_e623].seed = _e624.seed;
        unnamed_1.writeParticles[_e623].vel = _e624.vel;
        unnamed_1.writeParticles[_e623].flags = _e624.flags;
        return;
    }
    let _e636 = p_1.vel;
    let _e638 = unnamed.dt;
    let _e641 = p_1.pos;
    p_1.pos = (_e641 + (_e636 * _e638));
    let _e645 = p_1.flags;
    family_1 = u32((_e645 + 0.5f));
    let _e648 = family_1;
    let _e650 = family_1;
    if ((_e648 == 2u) || (_e650 == 5u)) {
        let _e654 = unnamed.time;
        let _e655 = idx_4;
        let _e662 = p_1.pos[0u];
        p_1.pos[0u] = (_e662 + (sin((_e654 + f32(_e655))) * 0.5f));
        let _e667 = unnamed.time;
        let _e669 = idx_4;
        let _e676 = p_1.pos[1u];
        p_1.pos[1u] = (_e676 + (cos(((_e667 * 1.3f) + f32(_e669))) * 0.5f));
    }
    let _e681 = unnamed.time;
    let _e683 = idx_4;
    gustPhase = sin(((_e681 * 0.7f) + (f32((_e683 % 97u)) * 0.37f)));
    let _e689 = family_1;
    if (_e689 == 1u) {
        local_2 = 0.25f;
    } else {
        let _e691 = family_1;
        if (_e691 == 3u) {
            local_3 = 0.5f;
        } else {
            let _e693 = family_1;
            if (_e693 == 4u) {
                local_4 = 0.15f;
            } else {
                let _e695 = family_1;
                local_4 = select(1f, 1.5f, (_e695 == 5u));
            }
            let _e698 = local_4;
            local_3 = _e698;
        }
        let _e699 = local_3;
        local_2 = _e699;
    }
    let _e700 = local_2;
    familyWind = _e700;
    let _e702 = unnamed.windGust;
    let _e706 = unnamed.windGust[3u];
    let _e707 = gustPhase;
    let _e709 = familyWind;
    let _e712 = unnamed.dt;
    let _e716 = p_1.pos;
    p_1.pos = (_e716 + (_e702.xyz * (((_e706 * _e707) * _e709) * _e712)));
    let _e720 = unnamed.gridSize;
    if (_e720 > 1u) {
        let _e723 = p_1.pos;
        param_44 = _e723.xy;
        let _e725 = groundHeightAt_u0028_vf2_u003b((&param_44));
        local_5 = _e725;
    } else {
        let _e728 = unnamed.boundsMin[2u];
        local_5 = _e728;
    }
    let _e729 = local_5;
    ground = _e729;
    let _e732 = p_1.pos[2u];
    let _e733 = ground;
    if (_e732 < _e733) {
        let _e735 = idx_4;
        let _e736 = deadParticle_u0028_();
        unnamed_1.writeParticles[_e735].pos = _e736.pos;
        unnamed_1.writeParticles[_e735].seed = _e736.seed;
        unnamed_1.writeParticles[_e735].vel = _e736.vel;
        unnamed_1.writeParticles[_e735].flags = _e736.flags;
        return;
    }
    let _e748 = p_1.pos;
    let _e751 = unnamed.eyeWorld;
    d = (_e748.xy - _e751.xy);
    let _e754 = d;
    let _e755 = d;
    let _e758 = unnamed.distance;
    let _e760 = unnamed.distance;
    if (dot(_e754, _e755) > (_e758 * _e760)) {
        let _e763 = idx_4;
        let _e764 = deadParticle_u0028_();
        unnamed_1.writeParticles[_e763].pos = _e764.pos;
        unnamed_1.writeParticles[_e763].seed = _e764.seed;
        unnamed_1.writeParticles[_e763].vel = _e764.vel;
        unnamed_1.writeParticles[_e763].flags = _e764.flags;
        return;
    }
    let _e777 = p_1.pos[2u];
    let _e780 = unnamed.eyeWorld[2u];
    if (_e777 < (_e780 - 500f)) {
        let _e783 = idx_4;
        let _e784 = deadParticle_u0028_();
        unnamed_1.writeParticles[_e783].pos = _e784.pos;
        unnamed_1.writeParticles[_e783].seed = _e784.seed;
        unnamed_1.writeParticles[_e783].vel = _e784.vel;
        unnamed_1.writeParticles[_e783].flags = _e784.flags;
        return;
    }
    let _e795 = idx_4;
    let _e796 = p_1;
    unnamed_1.writeParticles[_e795].pos = _e796.pos;
    unnamed_1.writeParticles[_e795].seed = _e796.seed;
    unnamed_1.writeParticles[_e795].vel = _e796.vel;
    unnamed_1.writeParticles[_e795].flags = _e796.flags;
    return;
}

@compute @workgroup_size(64, 1, 1)
fn main(@builtin(global_invocation_id) gl_GlobalInvocationID: vec3<u32>) {
    gl_GlobalInvocationID_1 = gl_GlobalInvocationID;
    main_1();
}
