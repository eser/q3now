struct ParticleParm {
    calc: i32,
    hasCurve: i32,
    val0_: f32,
    val1_: f32,
    variance: f32,
    parmPad0_: f32,
    parmPad1_: f32,
    parmPad2_: f32,
    samples: array<f32, 8>,
}

struct Particle {
    pos: vec3<f32>,
    age: f32,
    vel: vec3<f32>,
    lifetimeInv: f32,
    classHandle: u32,
    paletteIndex: u32,
    sizeJitterPick: f32,
    pad1_: u32,
    pad2_: u32,
    pad3_: u32,
    pad4_: u32,
    pad5_: u32,
}

struct ParticleSpawnRequest {
    origin: vec4<f32>,
    axis: vec4<f32>,
    end: vec4<f32>,
    colorTint: vec4<f32>,
    classHandle: u32,
    count: u32,
    firstSlot: u32,
    seed: u32,
    profileHandle: u32,
    stageIndex: u32,
    stageFlags: u32,
    reserved: u32,
}

struct AtmosphereEffectStage {
    trigger: u32,
    particleClass: u32,
    parentStage: u32,
    flags: u32,
    maxParticles: u32,
    burstCount: u32,
    spawnRate: f32,
    delay: f32,
    duration: f32,
    lodNear: f32,
    lodFar: f32,
    boundsRadius: f32,
    intensityScale: f32,
    reserved0_: u32,
    reserved1_: u32,
    reserved2_: u32,
}

struct ParticleFrame {
    mvp: mat4x4<f32>,
    viewLeft: vec4<f32>,
    viewUp: vec4<f32>,
    eyeWorld: vec4<f32>,
    dt: f32,
    poolSize: u32,
    numClasses: u32,
    pingPongRead: u32,
    pad0_: f32,
    pad1_: f32,
    pad2_: f32,
    pad3_: f32,
}

struct ParticleClassGPU {
    shader: u32,
    renderFlags: u32,
    emitMode: u32,
    scatterShape: u32,
    scatterMagnitude: f32,
    velocityShape: u32,
    axialSpeed: f32,
    cubeJitter: f32,
    coneHalfAngle: f32,
    lifetimeMean: f32,
    lifetimeJitter: f32,
    paletteCount: i32,
    colorPalette: array<vec4<f32>, 16>,
    colorEndMult: vec4<f32>,
    sizeStart: f32,
    sizeEnd: f32,
    gravityScale: f32,
    drag: f32,
    shaderBlendIsAdditive: u32,
    pad1_: u32,
    pad2_: u32,
    pad3_: u32,
    velocityBias: vec4<f32>,
    velocityBiasJitter: vec4<f32>,
    speedJitter: f32,
    sizeJitter: f32,
    colorDomain: u32,
    pad5_: u32,
    frameSlots: array<u32, 16>,
    frameCount: u32,
    frameBlend: u32,
    framePad0_: u32,
    framePad1_: u32,
    sizeParm: ParticleParm,
    alphaParm: ParticleParm,
    dragParm: ParticleParm,
    gravityParm: ParticleParm,
}

struct Classes {
    classes: array<ParticleClassGPU>,
}

struct AtmosphereEffectProfile {
    schemaVersion: u32,
    stageCount: u32,
    maxParticles: u32,
    flags: u32,
    parentProfile: u32,
    stageOverrideMask: u32,
    seed: u32,
    reserved0_: u32,
    duration: f32,
    lodNear: f32,
    lodFar: f32,
    boundsRadius: f32,
    stages: array<AtmosphereEffectStage, 8>,
    reserved: array<u32, 8>,
}

struct AtmosphereProfiles {
    atmosphereProfiles: array<AtmosphereEffectProfile>,
}

struct ParticleChildEvent {
    origin: vec4<f32>,
    velocity: vec4<f32>,
    colorTint: vec4<f32>,
    profileHandle: u32,
    stageIndex: u32,
    count: u32,
    seed: u32,
}

struct ChildEvents {
    dispatchX: u32,
    dispatchY: u32,
    dispatchZ: u32,
    dispatchPad: u32,
    eventCount: u32,
    particleCursor: u32,
    droppedEvents: u32,
    droppedParticles: u32,
    eventBaseSlot: u32,
    particleBudget: u32,
    eventCapacity: u32,
    collisionEnabled: u32,
    collisionWorldMins: vec2<f32>,
    collisionWorldMaxs: vec2<f32>,
    childEvents: array<ParticleChildEvent, 1024>,
    stageParticles: array<u32, 512>,
    profileParticles: array<u32, 64>,
}

struct WritePool {
    writeParticles: array<Particle>,
}

struct SpawnRequests {
    spawnRequests: array<ParticleSpawnRequest>,
}

struct ReadPool {
    readParticles: array<Particle>,
}

struct ChildEvents_1 {
    dispatchX: u32,
    dispatchY: u32,
    dispatchZ: u32,
    dispatchPad: u32,
    eventCount: atomic<u32>,
    particleCursor: atomic<u32>,
    droppedEvents: atomic<u32>,
    droppedParticles: atomic<u32>,
    eventBaseSlot: u32,
    particleBudget: u32,
    eventCapacity: u32,
    collisionEnabled: u32,
    collisionWorldMins: vec2<f32>,
    collisionWorldMaxs: vec2<f32>,
    childEvents: array<ParticleChildEvent, 1024>,
    stageParticles: array<atomic<u32>, 512>,
    profileParticles: array<atomic<u32>, 64>,
}

@id(0) override PARTICLE_PASS: u32 = 0u;
override override_type_6_: bool = (PARTICLE_PASS == 3u);
override override_type_6_1: bool = (PARTICLE_PASS == 2u);
override override_type_6_2: bool = (PARTICLE_PASS == 1u);

@group(0) @binding(0)
var<uniform> unnamed: ParticleFrame;
@group(0) @binding(3)
var<storage> unnamed_1: Classes;
@group(0) @binding(5)
var<storage> unnamed_2: AtmosphereProfiles;
@group(0) @binding(6)
var<storage, read_write> unnamed_3: ChildEvents_1;
@group(0) @binding(7)
var collisionHeightgrid: texture_2d<f32>;
@group(0) @binding(39)
var collisionHeightgrid_sampler: sampler;
var<private> gl_GlobalInvocationID_1: vec3<u32>;
var<private> gl_WorkGroupID_1: vec3<u32>;
var<private> gl_LocalInvocationID_1: vec3<u32>;
var<workgroup> childFirstSlot: u32;
var<workgroup> childAcceptedCount: u32;
@group(0) @binding(2)
var<storage, read_write> unnamed_4: WritePool;
@group(0) @binding(4)
var<storage> unnamed_5: SpawnRequests;
@group(0) @binding(1)
var<storage, read_write> unnamed_6: ReadPool;

fn spawnHash_u0028_u1_u003b(value: ptr<function, u32>) -> u32 {
    let _e113 = (*value);
    let _e116 = (*value);
    (*value) = (_e116 ^ (_e113 >> bitcast<u32>(16i)));
    let _e118 = (*value);
    (*value) = (_e118 * 2146121005u);
    let _e120 = (*value);
    let _e123 = (*value);
    (*value) = (_e123 ^ (_e120 >> bitcast<u32>(15i)));
    let _e125 = (*value);
    (*value) = (_e125 * 2221713035u);
    let _e127 = (*value);
    let _e130 = (*value);
    (*value) = (_e130 ^ (_e127 >> bitcast<u32>(16i)));
    let _e132 = (*value);
    return _e132;
}

fn stageTriggerCount_u0028_struct_u002d_AtmosphereEffectStage_u002d_u1_u002d_u1_u002d_u1_u002d_u1_u002d_u1_u002d_u1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u002d_u1_u002d_u1_u002d_u11_u003b_struct_u002d_Particle_u002d_vf3_u002d_f1_u002d_vf3_u002d_f1_u002d_u1_u002d_u1_u002d_f1_u002d_u1_u002d_u1_u002d_u1_u002d_u1_u002d_u11_u003b_f1_u003b_f1_u003b_b1_u003b_b1_u003b(stage: ptr<function, AtmosphereEffectStage>, p: ptr<function, Particle>, previousAge: ptr<function, f32>, currentAge: ptr<function, f32>, collided: ptr<function, bool>, died: ptr<function, bool>) -> u32 {
    var lifetime: f32;
    var local: f32;
    var previousSeconds: f32;
    var currentSeconds: f32;
    var count: u32;
    var previousActive: f32;
    var currentActive: f32;
    var before: f32;
    var after: f32;
    var parentTint: vec4<f32>;
    var intensity: f32;
    var local_1: f32;
    var phi_1356_: bool;
    var phi_1385_: bool;
    var phi_1430_: bool;
    var phi_1440_: bool;
    var phi_1469_: bool;
    var phi_1479_: bool;

    let _e131 = (*p).lifetimeInv;
    if (_e131 > 0f) {
        let _e134 = (*p).lifetimeInv;
        local = (1f / _e134);
    } else {
        local = 0f;
    }
    let _e136 = local;
    lifetime = _e136;
    let _e137 = (*previousAge);
    let _e138 = lifetime;
    previousSeconds = (_e137 * _e138);
    let _e140 = (*currentAge);
    let _e141 = lifetime;
    currentSeconds = (_e140 * _e141);
    count = 0u;
    let _e144 = (*stage).trigger;
    let _e145 = (_e144 == 0u);
    phi_1356_ = _e145;
    if !(_e145) {
        let _e148 = (*stage).trigger;
        phi_1356_ = (_e148 == 1u);
    }
    let _e151 = phi_1356_;
    if _e151 {
        let _e152 = previousSeconds;
        let _e154 = (*stage).delay;
        let _e157 = (*stage).duration;
        previousActive = clamp((_e152 - _e154), 0f, _e157);
        let _e159 = currentSeconds;
        let _e161 = (*stage).delay;
        let _e164 = (*stage).duration;
        currentActive = clamp((_e159 - _e161), 0f, _e164);
        let _e166 = previousSeconds;
        let _e168 = (*stage).delay;
        let _e169 = (_e166 <= _e168);
        phi_1385_ = _e169;
        if _e169 {
            let _e170 = currentSeconds;
            let _e172 = (*stage).delay;
            phi_1385_ = (_e170 > _e172);
        }
        let _e175 = phi_1385_;
        if _e175 {
            let _e177 = (*stage).burstCount;
            let _e178 = count;
            count = (_e178 + _e177);
        }
        let _e180 = previousActive;
        let _e182 = (*stage).spawnRate;
        before = floor(((_e180 * _e182) + 0.00001f));
        let _e186 = currentActive;
        let _e188 = (*stage).spawnRate;
        after = floor(((_e186 * _e188) + 0.00001f));
        let _e192 = after;
        let _e193 = before;
        if (_e192 > _e193) {
            let _e195 = after;
            let _e196 = before;
            let _e199 = count;
            count = (_e199 + u32((_e195 - _e196)));
        }
    } else {
        let _e202 = (*stage).trigger;
        let _e204 = (*collided);
        let _e205 = ((_e202 == 2u) && _e204);
        phi_1430_ = _e205;
        if _e205 {
            let _e206 = currentSeconds;
            let _e208 = (*stage).delay;
            phi_1430_ = (_e206 >= _e208);
        }
        let _e211 = phi_1430_;
        phi_1440_ = _e211;
        if _e211 {
            let _e212 = currentSeconds;
            let _e214 = (*stage).delay;
            let _e216 = (*stage).duration;
            phi_1440_ = (_e212 <= (_e214 + _e216));
        }
        let _e220 = phi_1440_;
        if _e220 {
            let _e222 = (*stage).burstCount;
            let _e224 = (*stage).spawnRate;
            let _e226 = unnamed.dt;
            let _e228 = (*stage).duration;
            count = (_e222 + u32(ceil((_e224 * min(_e226, _e228)))));
        } else {
            let _e235 = (*stage).trigger;
            let _e237 = (*died);
            let _e238 = ((_e235 == 3u) && _e237);
            phi_1469_ = _e238;
            if _e238 {
                let _e239 = currentSeconds;
                let _e241 = (*stage).delay;
                phi_1469_ = (_e239 >= _e241);
            }
            let _e244 = phi_1469_;
            phi_1479_ = _e244;
            if _e244 {
                let _e245 = currentSeconds;
                let _e247 = (*stage).delay;
                let _e249 = (*stage).duration;
                phi_1479_ = (_e245 <= (_e247 + _e249));
            }
            let _e253 = phi_1479_;
            if _e253 {
                let _e255 = (*stage).burstCount;
                let _e257 = (*stage).spawnRate;
                let _e259 = unnamed.dt;
                let _e261 = (*stage).duration;
                count = (_e255 + u32(ceil((_e257 * min(_e259, _e261)))));
            }
        }
    }
    let _e268 = (*p).pad5_;
    parentTint = unpack4x8unorm(_e268);
    let _e271 = (*stage).flags;
    if ((_e271 & 8u) != 0u) {
        let _e275 = parentTint[3u];
        local_1 = _e275;
    } else {
        local_1 = 1f;
    }
    let _e276 = local_1;
    intensity = _e276;
    let _e277 = count;
    let _e279 = intensity;
    let _e282 = (*stage).intensityScale;
    count = u32(ceil(((f32(_e277) * _e279) * _e282)));
    let _e286 = count;
    let _e288 = (*stage).maxParticles;
    return min(min(_e286, _e288), 64u);
}

fn appendChildEvents_u0028_struct_u002d_Particle_u002d_vf3_u002d_f1_u002d_vf3_u002d_f1_u002d_u1_u002d_u1_u002d_f1_u002d_u1_u002d_u1_u002d_u1_u002d_u1_u002d_u11_u003b_f1_u003b_f1_u003b_b1_u003b_b1_u003b(p_1: ptr<function, Particle>, previousAge_1: ptr<function, f32>, currentAge_1: ptr<function, f32>, collided_1: ptr<function, bool>, died_1: ptr<function, bool>) {
    var stageCount: u32;
    var i: u32;
    var stage_1: AtmosphereEffectStage;
    var count_1: u32;
    var param: AtmosphereEffectStage;
    var param_1: Particle;
    var param_2: f32;
    var param_3: f32;
    var param_4: bool;
    var param_5: bool;
    var stageCounterIndex: u32;
    var stageBefore: u32;
    var stageAccepted: u32;
    var local_2: u32;
    var profileBefore: u32;
    var profileBudget: u32;
    var accepted: u32;
    var local_3: u32;
    var eventIndex: u32;
    var parentTint_1: vec4<f32>;
    var tint: vec4<f32>;
    var event: ParticleChildEvent;
    var local_4: vec3<f32>;
    var param_6: u32;
    var phi_1536_: bool;
    var phi_1543_: bool;
    var phi_1625_: bool;

    let _e142 = (*p_1).pad1_;
    let _e143 = (_e142 == 0u);
    phi_1536_ = _e143;
    if !(_e143) {
        let _e146 = (*p_1).pad1_;
        phi_1536_ = (_e146 > 64u);
    }
    let _e149 = phi_1536_;
    phi_1543_ = _e149;
    if !(_e149) {
        let _e152 = (*p_1).pad2_;
        phi_1543_ = (_e152 >= 8u);
    }
    let _e155 = phi_1543_;
    if _e155 {
        return;
    }
    let _e157 = (*p_1).pad1_;
    let _e162 = unnamed_2.atmosphereProfiles[(_e157 - 1u)].stageCount;
    stageCount = min(_e162, 8u);
    i = 0u;
    loop {
        let _e164 = i;
        let _e165 = stageCount;
        if (_e164 < _e165) {
            let _e168 = (*p_1).pad1_;
            let _e170 = i;
            let _e175 = unnamed_2.atmosphereProfiles[(_e168 - 1u)].stages[_e170];
            stage_1.trigger = _e175.trigger;
            stage_1.particleClass = _e175.particleClass;
            stage_1.parentStage = _e175.parentStage;
            stage_1.flags = _e175.flags;
            stage_1.maxParticles = _e175.maxParticles;
            stage_1.burstCount = _e175.burstCount;
            stage_1.spawnRate = _e175.spawnRate;
            stage_1.delay = _e175.delay;
            stage_1.duration = _e175.duration;
            stage_1.lodNear = _e175.lodNear;
            stage_1.lodFar = _e175.lodFar;
            stage_1.boundsRadius = _e175.boundsRadius;
            stage_1.intensityScale = _e175.intensityScale;
            stage_1.reserved0_ = _e175.reserved0_;
            stage_1.reserved1_ = _e175.reserved1_;
            stage_1.reserved2_ = _e175.reserved2_;
            let _e209 = stage_1.parentStage;
            let _e211 = (*p_1).pad2_;
            if (_e209 != _e211) {
                continue;
            }
            let _e214 = stage_1.lodFar;
            let _e215 = (_e214 > 0f);
            phi_1625_ = _e215;
            if _e215 {
                let _e217 = (*p_1).pos;
                let _e219 = unnamed.eyeWorld;
                let _e223 = stage_1.lodFar;
                phi_1625_ = (distance(_e217, _e219.xyz) > _e223);
            }
            let _e226 = phi_1625_;
            if _e226 {
                continue;
            }
            let _e227 = stage_1;
            param = _e227;
            let _e228 = (*p_1);
            param_1 = _e228;
            let _e229 = (*previousAge_1);
            param_2 = _e229;
            let _e230 = (*currentAge_1);
            param_3 = _e230;
            let _e231 = (*collided_1);
            param_4 = _e231;
            let _e232 = (*died_1);
            param_5 = _e232;
            let _e233 = stageTriggerCount_u0028_struct_u002d_AtmosphereEffectStage_u002d_u1_u002d_u1_u002d_u1_u002d_u1_u002d_u1_u002d_u1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u002d_u1_u002d_u1_u002d_u11_u003b_struct_u002d_Particle_u002d_vf3_u002d_f1_u002d_vf3_u002d_f1_u002d_u1_u002d_u1_u002d_f1_u002d_u1_u002d_u1_u002d_u1_u002d_u1_u002d_u11_u003b_f1_u003b_f1_u003b_b1_u003b_b1_u003b((&param), (&param_1), (&param_2), (&param_3), (&param_4), (&param_5));
            count_1 = _e233;
            let _e234 = count_1;
            if (_e234 == 0u) {
                continue;
            }
            let _e237 = (*p_1).pad1_;
            let _e240 = i;
            stageCounterIndex = (((_e237 - 1u) * 8u) + _e240);
            let _e242 = stageCounterIndex;
            let _e245 = count_1;
            let _e246 = atomicAdd((&unnamed_3.stageParticles[_e242]), _e245);
            stageBefore = _e246;
            let _e247 = stageBefore;
            let _e249 = stage_1.maxParticles;
            if (_e247 < _e249) {
                let _e251 = count_1;
                let _e253 = stage_1.maxParticles;
                let _e254 = stageBefore;
                local_2 = min(_e251, (_e253 - _e254));
            } else {
                local_2 = 0u;
            }
            let _e257 = local_2;
            stageAccepted = _e257;
            let _e259 = (*p_1).pad1_;
            let _e263 = stageAccepted;
            let _e264 = atomicAdd((&unnamed_3.profileParticles[(_e259 - 1u)]), _e263);
            profileBefore = _e264;
            let _e266 = (*p_1).pad1_;
            let _e271 = unnamed_2.atmosphereProfiles[(_e266 - 1u)].maxParticles;
            profileBudget = _e271;
            let _e272 = profileBefore;
            let _e273 = profileBudget;
            if (_e272 < _e273) {
                let _e275 = stageAccepted;
                let _e276 = profileBudget;
                let _e277 = profileBefore;
                local_3 = min(_e275, (_e276 - _e277));
            } else {
                local_3 = 0u;
            }
            let _e280 = local_3;
            accepted = _e280;
            let _e281 = accepted;
            let _e282 = count_1;
            if (_e281 < _e282) {
                let _e285 = count_1;
                let _e286 = accepted;
                let _e288 = atomicAdd((&unnamed_3.droppedParticles), (_e285 - _e286));
            }
            let _e289 = accepted;
            count_1 = _e289;
            let _e290 = count_1;
            if (_e290 == 0u) {
                continue;
            }
            let _e293 = atomicAdd((&unnamed_3.eventCount), 1u);
            eventIndex = _e293;
            let _e294 = eventIndex;
            let _e296 = unnamed_3.eventCapacity;
            if (_e294 >= _e296) {
                let _e299 = atomicAdd((&unnamed_3.droppedEvents), 1u);
                let _e301 = count_1;
                let _e302 = atomicAdd((&unnamed_3.droppedParticles), _e301);
                continue;
            }
            let _e304 = (*p_1).pad5_;
            parentTint_1 = unpack4x8unorm(_e304);
            tint = vec4<f32>(1f, 1f, 1f, 1f);
            let _e307 = stage_1.flags;
            if ((_e307 & 4u) != 0u) {
                let _e310 = parentTint_1;
                let _e311 = _e310.xyz;
                tint[0u] = _e311.x;
                tint[1u] = _e311.y;
                tint[2u] = _e311.z;
            }
            let _e319 = stage_1.flags;
            if ((_e319 & 8u) != 0u) {
                let _e323 = parentTint_1[3u];
                tint[3u] = _e323;
            }
            let _e326 = tint[3u];
            let _e328 = stage_1.intensityScale;
            tint[3u] = clamp((_e326 * _e328), 0f, 1f);
            let _e333 = (*p_1).pos;
            event.origin = vec4<f32>(_e333.x, _e333.y, _e333.z, 1f);
            let _e340 = stage_1.flags;
            if ((_e340 & 2u) != 0u) {
                let _e344 = (*p_1).vel;
                local_4 = _e344;
            } else {
                local_4 = vec3<f32>(0f, 0f, 1f);
            }
            let _e345 = local_4;
            event.velocity = vec4<f32>(_e345.x, _e345.y, _e345.z, 0f);
            let _e351 = tint;
            event.colorTint = _e351;
            let _e354 = (*p_1).pad1_;
            event.profileHandle = _e354;
            let _e356 = i;
            event.stageIndex = _e356;
            let _e358 = count_1;
            event.count = _e358;
            let _e361 = (*p_1).pad3_;
            let _e362 = i;
            let _e365 = (*currentAge_1);
            param_6 = ((_e361 ^ (_e362 * 2246822519u)) ^ bitcast<u32>(_e365));
            let _e368 = spawnHash_u0028_u1_u003b((&param_6));
            event.seed = _e368;
            let _e370 = eventIndex;
            let _e371 = event;
            unnamed_3.childEvents[_e370].origin = _e371.origin;
            unnamed_3.childEvents[_e370].velocity = _e371.velocity;
            unnamed_3.childEvents[_e370].colorTint = _e371.colorTint;
            unnamed_3.childEvents[_e370].profileHandle = _e371.profileHandle;
            unnamed_3.childEvents[_e370].stageIndex = _e371.stageIndex;
            unnamed_3.childEvents[_e370].count = _e371.count;
            unnamed_3.childEvents[_e370].seed = _e371.seed;
            continue;
        } else {
            break;
        }
        continuing {
            let _e388 = i;
            i = (_e388 + bitcast<u32>(1i));
        }
    }
    return;
}

fn particleCollided_u0028_struct_u002d_Particle_u002d_vf3_u002d_f1_u002d_vf3_u002d_f1_u002d_u1_u002d_u1_u002d_f1_u002d_u1_u002d_u1_u002d_u1_u002d_u1_u002d_u11_u003b(p_2: ptr<function, Particle>) -> bool {
    var extent: vec2<f32>;
    var uv: vec2<f32>;
    var phi_1287_: bool;
    var phi_1298_: bool;

    let _e116 = unnamed_3.collisionEnabled;
    if (_e116 == 0u) {
        return false;
    }
    let _e119 = unnamed_3.collisionWorldMaxs;
    let _e121 = unnamed_3.collisionWorldMins;
    extent = (_e119 - _e121);
    let _e123 = extent;
    let _e125 = any((_e123 <= vec2<f32>(0f, 0f)));
    phi_1287_ = _e125;
    if !(_e125) {
        let _e128 = (*p_2).pos;
        let _e131 = unnamed_3.collisionWorldMins;
        phi_1287_ = any((_e128.xy < _e131));
    }
    let _e135 = phi_1287_;
    phi_1298_ = _e135;
    if !(_e135) {
        let _e138 = (*p_2).pos;
        let _e141 = unnamed_3.collisionWorldMaxs;
        phi_1298_ = any((_e138.xy > _e141));
    }
    let _e145 = phi_1298_;
    if _e145 {
        return false;
    }
    let _e147 = (*p_2).pos;
    let _e150 = unnamed_3.collisionWorldMins;
    let _e152 = extent;
    uv = clamp(((_e147.xy - _e150) / _e152), vec2<f32>(0f, 0f), vec2<f32>(1f, 1f));
    let _e157 = (*p_2).pos[2u];
    let _e158 = uv;
    let _e159 = textureSampleLevel(collisionHeightgrid, collisionHeightgrid_sampler, _e158, 0f);
    return (_e157 <= _e159.x);
}

fn particleHasCollisionChild_u0028_struct_u002d_Particle_u002d_vf3_u002d_f1_u002d_vf3_u002d_f1_u002d_u1_u002d_u1_u002d_f1_u002d_u1_u002d_u1_u002d_u1_u002d_u1_u002d_u11_u003b(p_3: ptr<function, Particle>) -> bool {
    var count_2: u32;
    var i_1: u32;
    var stage_2: AtmosphereEffectStage;
    var phi_1153_: bool;
    var phi_1160_: bool;
    var phi_1239_: bool;

    let _e117 = (*p_3).pad1_;
    let _e118 = (_e117 == 0u);
    phi_1153_ = _e118;
    if !(_e118) {
        let _e121 = (*p_3).pad1_;
        phi_1153_ = (_e121 > 64u);
    }
    let _e124 = phi_1153_;
    phi_1160_ = _e124;
    if !(_e124) {
        let _e127 = (*p_3).pad2_;
        phi_1160_ = (_e127 >= 8u);
    }
    let _e130 = phi_1160_;
    if _e130 {
        return false;
    }
    let _e132 = (*p_3).pad1_;
    let _e137 = unnamed_2.atmosphereProfiles[(_e132 - 1u)].stageCount;
    count_2 = min(_e137, 8u);
    i_1 = 0u;
    loop {
        let _e139 = i_1;
        let _e140 = count_2;
        if (_e139 < _e140) {
            let _e143 = (*p_3).pad1_;
            let _e145 = i_1;
            let _e150 = unnamed_2.atmosphereProfiles[(_e143 - 1u)].stages[_e145];
            stage_2.trigger = _e150.trigger;
            stage_2.particleClass = _e150.particleClass;
            stage_2.parentStage = _e150.parentStage;
            stage_2.flags = _e150.flags;
            stage_2.maxParticles = _e150.maxParticles;
            stage_2.burstCount = _e150.burstCount;
            stage_2.spawnRate = _e150.spawnRate;
            stage_2.delay = _e150.delay;
            stage_2.duration = _e150.duration;
            stage_2.lodNear = _e150.lodNear;
            stage_2.lodFar = _e150.lodFar;
            stage_2.boundsRadius = _e150.boundsRadius;
            stage_2.intensityScale = _e150.intensityScale;
            stage_2.reserved0_ = _e150.reserved0_;
            stage_2.reserved1_ = _e150.reserved1_;
            stage_2.reserved2_ = _e150.reserved2_;
            let _e184 = stage_2.parentStage;
            let _e186 = (*p_3).pad2_;
            let _e187 = (_e184 == _e186);
            phi_1239_ = _e187;
            if _e187 {
                let _e189 = stage_2.trigger;
                phi_1239_ = (_e189 == 2u);
            }
            let _e192 = phi_1239_;
            if _e192 {
                return true;
            }
            continue;
        } else {
            break;
        }
        continuing {
            let _e193 = i_1;
            i_1 = (_e193 + bitcast<u32>(1i));
        }
    }
    return false;
}

fn parmSampleCurve_u0028_struct_u002d_ParticleParm_u002d_i1_u002d_i1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u005b_8_u005d_1_u003b_f1_u003b(p_4: ptr<function, ParticleParm>, fraction: ptr<function, f32>) -> f32 {
    var pos: f32;
    var i0_: i32;
    var i1_: i32;
    var frac: f32;

    let _e119 = (*p_4).hasCurve;
    if (_e119 == 0i) {
        return 1f;
    }
    let _e121 = (*fraction);
    if (_e121 <= 0f) {
        let _e125 = (*p_4).samples[0i];
        return _e125;
    }
    let _e126 = (*fraction);
    if (_e126 >= 1f) {
        let _e130 = (*p_4).samples[7i];
        return _e130;
    }
    let _e131 = (*fraction);
    pos = (_e131 * 7f);
    let _e133 = pos;
    i0_ = i32(_e133);
    let _e135 = i0_;
    i1_ = min((_e135 + 1i), 7i);
    let _e138 = pos;
    let _e139 = i0_;
    frac = (_e138 - f32(_e139));
    let _e142 = i0_;
    let _e145 = (*p_4).samples[_e142];
    let _e146 = i1_;
    let _e149 = (*p_4).samples[_e146];
    let _e150 = frac;
    return mix(_e145, _e149, _e150);
}

fn parmEval_u0028_struct_u002d_ParticleParm_u002d_i1_u002d_i1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u005b_8_u005d_1_u003b_f1_u003b_f1_u003b(p_5: ptr<function, ParticleParm>, fraction_1: ptr<function, f32>, jitterPick: ptr<function, f32>) -> f32 {
    var base: f32;
    var param_7: ParticleParm;
    var param_8: f32;
    var param_9: ParticleParm;
    var param_10: f32;

    let _e120 = (*fraction_1);
    (*fraction_1) = clamp(_e120, 0f, 1f);
    let _e123 = (*p_5).calc;
    if (_e123 == 1i) {
        let _e126 = (*p_5).val0_;
        let _e128 = (*p_5).val1_;
        let _e129 = (*fraction_1);
        base = mix(_e126, _e128, _e129);
    } else {
        let _e132 = (*p_5).calc;
        if (_e132 == 2i) {
            let _e135 = (*p_5).val0_;
            let _e137 = (*p_5).val1_;
            let _e138 = (*p_5);
            param_7 = _e138;
            let _e139 = (*fraction_1);
            param_8 = _e139;
            let _e140 = parmSampleCurve_u0028_struct_u002d_ParticleParm_u002d_i1_u002d_i1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u005b_8_u005d_1_u003b_f1_u003b((&param_7), (&param_8));
            base = mix(_e135, _e137, _e140);
        } else {
            let _e143 = (*p_5).calc;
            if (_e143 == 3i) {
                let _e145 = (*p_5);
                param_9 = _e145;
                let _e146 = (*fraction_1);
                param_10 = _e146;
                let _e147 = parmSampleCurve_u0028_struct_u002d_ParticleParm_u002d_i1_u002d_i1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u005b_8_u005d_1_u003b_f1_u003b((&param_9), (&param_10));
                let _e149 = (*p_5).val0_;
                let _e151 = (*p_5).val1_;
                let _e152 = (*fraction_1);
                base = (_e147 * mix(_e149, _e151, _e152));
            } else {
                let _e156 = (*p_5).val0_;
                base = _e156;
            }
        }
    }
    let _e157 = base;
    let _e159 = (*p_5).variance;
    let _e160 = (*jitterPick);
    return (_e157 + (_e159 * _e160));
}

fn parmIsUnset_u0028_struct_u002d_ParticleParm_u002d_i1_u002d_i1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u005b_8_u005d_1_u003b(p_6: ptr<function, ParticleParm>) -> bool {
    var phi_215_: bool;

    let _e114 = (*p_6).calc;
    let _e115 = (_e114 == 0i);
    phi_215_ = _e115;
    if _e115 {
        let _e117 = (*p_6).val0_;
        phi_215_ = (_e117 == 0f);
    }
    let _e120 = phi_215_;
    return _e120;
}

fn deadParticle_u0028_() -> Particle {
    var p_7: Particle;

    p_7.pos = vec3<f32>(0f, 0f, 0f);
    p_7.age = 0f;
    p_7.vel = vec3<f32>(0f, 0f, 0f);
    p_7.lifetimeInv = 0f;
    p_7.classHandle = 0u;
    p_7.paletteIndex = 0u;
    p_7.sizeJitterPick = 0f;
    p_7.pad1_ = 0u;
    p_7.pad2_ = 0u;
    p_7.pad3_ = 0u;
    p_7.pad4_ = 0u;
    p_7.pad5_ = 0u;
    let _e125 = p_7;
    return _e125;
}

fn spawn01_u0028_u1_u003b_u1_u003b_u1_u003b(seed: ptr<function, u32>, ordinal: ptr<function, u32>, lane: ptr<function, u32>) -> f32 {
    var param_11: u32;

    let _e116 = (*seed);
    let _e117 = (*ordinal);
    let _e120 = (*lane);
    param_11 = ((_e116 ^ (_e117 * 747796405u)) ^ (_e120 * 2891336453u));
    let _e123 = spawnHash_u0028_u1_u003b((&param_11));
    return (f32(_e123) * 0.00000000023283064f);
}

fn spawnSigned_u0028_u1_u003b_u1_u003b_u1_u003b(seed_1: ptr<function, u32>, ordinal_1: ptr<function, u32>, lane_1: ptr<function, u32>) -> f32 {
    var param_12: u32;
    var param_13: u32;
    var param_14: u32;

    let _e118 = (*seed_1);
    param_12 = _e118;
    let _e119 = (*ordinal_1);
    param_13 = _e119;
    let _e120 = (*lane_1);
    param_14 = _e120;
    let _e121 = spawn01_u0028_u1_u003b_u1_u003b_u1_u003b((&param_12), (&param_13), (&param_14));
    return ((_e121 * 2f) - 1f);
}

fn spawnAxis_u0028_vf3_u003b(axis: ptr<function, vec3<f32>>) -> vec3<f32> {
    var lengthSquared: f32;
    var local_5: vec3<f32>;

    let _e115 = (*axis);
    let _e116 = (*axis);
    lengthSquared = dot(_e115, _e116);
    let _e118 = lengthSquared;
    if (_e118 > 0.00000001f) {
        let _e120 = (*axis);
        let _e121 = lengthSquared;
        local_5 = (_e120 * inverseSqrt(_e121));
    } else {
        local_5 = vec3<f32>(0f, 0f, 1f);
    }
    let _e124 = local_5;
    return _e124;
}

fn buildSpawnParticle_u0028_struct_u002d_ParticleSpawnRequest_u002d_vf4_u002d_vf4_u002d_vf4_u002d_vf4_u002d_u1_u002d_u1_u002d_u1_u002d_u1_u002d_u1_u002d_u1_u002d_u1_u002d_u11_u003b_u1_u003b(request: ptr<function, ParticleSpawnRequest>, ordinal_2: ptr<function, u32>) -> Particle {
    var c: ParticleClassGPU;
    var axis_1: vec3<f32>;
    var param_15: vec3<f32>;
    var base_1: vec3<f32>;
    var pathFraction: f32;
    var scatter: vec3<f32>;
    var param_16: u32;
    var param_17: u32;
    var param_18: u32;
    var param_19: u32;
    var param_20: u32;
    var param_21: u32;
    var param_22: u32;
    var param_23: u32;
    var param_24: u32;
    var z: f32;
    var param_25: u32;
    var param_26: u32;
    var param_27: u32;
    var phi: f32;
    var param_28: u32;
    var param_29: u32;
    var param_30: u32;
    var radial: f32;
    var radius: f32;
    var param_31: u32;
    var param_32: u32;
    var param_33: u32;
    var tangent: vec3<f32>;
    var local_6: vec3<f32>;
    var bitangent: vec3<f32>;
    var radius_1: f32;
    var param_34: u32;
    var param_35: u32;
    var param_36: u32;
    var phi_1: f32;
    var param_37: u32;
    var param_38: u32;
    var param_39: u32;
    var speed: f32;
    var param_40: u32;
    var param_41: u32;
    var param_42: u32;
    var velocity: vec3<f32>;
    var param_43: u32;
    var param_44: u32;
    var param_45: u32;
    var param_46: u32;
    var param_47: u32;
    var param_48: u32;
    var param_49: u32;
    var param_50: u32;
    var param_51: u32;
    var tangent_1: vec3<f32>;
    var local_7: vec3<f32>;
    var bitangent_1: vec3<f32>;
    var cosHalf: f32;
    var z_1: f32;
    var param_52: u32;
    var param_53: u32;
    var param_54: u32;
    var radial_1: f32;
    var phi_2: f32;
    var param_55: u32;
    var param_56: u32;
    var param_57: u32;
    var param_58: u32;
    var param_59: u32;
    var param_60: u32;
    var param_61: u32;
    var param_62: u32;
    var param_63: u32;
    var param_64: u32;
    var param_65: u32;
    var param_66: u32;
    var param_67: u32;
    var param_68: u32;
    var param_69: u32;
    var param_70: u32;
    var param_71: u32;
    var param_72: u32;
    var param_73: u32;
    var param_74: u32;
    var param_75: u32;
    var lifetime_1: f32;
    var param_76: u32;
    var param_77: u32;
    var param_78: u32;
    var p_8: Particle;
    var local_8: u32;
    var param_79: u32;
    var param_80: u32;
    var param_81: u32;
    var param_82: u32;
    var phi_327_: bool;
    var phi_334_: bool;
    var phi_341_: bool;

    let _e209 = (*request).classHandle;
    let _e210 = (_e209 == 0u);
    phi_327_ = _e210;
    if !(_e210) {
        let _e213 = (*request).classHandle;
        let _e215 = unnamed.numClasses;
        phi_327_ = (_e213 > _e215);
    }
    let _e218 = phi_327_;
    phi_334_ = _e218;
    if !(_e218) {
        let _e221 = (*request).count;
        phi_334_ = (_e221 == 0u);
    }
    let _e224 = phi_334_;
    phi_341_ = _e224;
    if !(_e224) {
        let _e227 = unnamed.poolSize;
        phi_341_ = (_e227 == 0u);
    }
    let _e230 = phi_341_;
    if _e230 {
        let _e231 = deadParticle_u0028_();
        return _e231;
    }
    let _e233 = (*request).classHandle;
    let _e237 = unnamed_1.classes[(_e233 - 1u)];
    c.shader = _e237.shader;
    c.renderFlags = _e237.renderFlags;
    c.emitMode = _e237.emitMode;
    c.scatterShape = _e237.scatterShape;
    c.scatterMagnitude = _e237.scatterMagnitude;
    c.velocityShape = _e237.velocityShape;
    c.axialSpeed = _e237.axialSpeed;
    c.cubeJitter = _e237.cubeJitter;
    c.coneHalfAngle = _e237.coneHalfAngle;
    c.lifetimeMean = _e237.lifetimeMean;
    c.lifetimeJitter = _e237.lifetimeJitter;
    c.paletteCount = _e237.paletteCount;
    c.colorPalette[0i] = _e237.colorPalette[0];
    c.colorPalette[1i] = _e237.colorPalette[1];
    c.colorPalette[2i] = _e237.colorPalette[2];
    c.colorPalette[3i] = _e237.colorPalette[3];
    c.colorPalette[4i] = _e237.colorPalette[4];
    c.colorPalette[5i] = _e237.colorPalette[5];
    c.colorPalette[6i] = _e237.colorPalette[6];
    c.colorPalette[7i] = _e237.colorPalette[7];
    c.colorPalette[8i] = _e237.colorPalette[8];
    c.colorPalette[9i] = _e237.colorPalette[9];
    c.colorPalette[10i] = _e237.colorPalette[10];
    c.colorPalette[11i] = _e237.colorPalette[11];
    c.colorPalette[12i] = _e237.colorPalette[12];
    c.colorPalette[13i] = _e237.colorPalette[13];
    c.colorPalette[14i] = _e237.colorPalette[14];
    c.colorPalette[15i] = _e237.colorPalette[15];
    c.colorEndMult = _e237.colorEndMult;
    c.sizeStart = _e237.sizeStart;
    c.sizeEnd = _e237.sizeEnd;
    c.gravityScale = _e237.gravityScale;
    c.drag = _e237.drag;
    c.shaderBlendIsAdditive = _e237.shaderBlendIsAdditive;
    c.pad1_ = _e237.pad1_;
    c.pad2_ = _e237.pad2_;
    c.pad3_ = _e237.pad3_;
    c.velocityBias = _e237.velocityBias;
    c.velocityBiasJitter = _e237.velocityBiasJitter;
    c.speedJitter = _e237.speedJitter;
    c.sizeJitter = _e237.sizeJitter;
    c.colorDomain = _e237.colorDomain;
    c.pad5_ = _e237.pad5_;
    c.frameSlots[0i] = _e237.frameSlots[0];
    c.frameSlots[1i] = _e237.frameSlots[1];
    c.frameSlots[2i] = _e237.frameSlots[2];
    c.frameSlots[3i] = _e237.frameSlots[3];
    c.frameSlots[4i] = _e237.frameSlots[4];
    c.frameSlots[5i] = _e237.frameSlots[5];
    c.frameSlots[6i] = _e237.frameSlots[6];
    c.frameSlots[7i] = _e237.frameSlots[7];
    c.frameSlots[8i] = _e237.frameSlots[8];
    c.frameSlots[9i] = _e237.frameSlots[9];
    c.frameSlots[10i] = _e237.frameSlots[10];
    c.frameSlots[11i] = _e237.frameSlots[11];
    c.frameSlots[12i] = _e237.frameSlots[12];
    c.frameSlots[13i] = _e237.frameSlots[13];
    c.frameSlots[14i] = _e237.frameSlots[14];
    c.frameSlots[15i] = _e237.frameSlots[15];
    c.frameCount = _e237.frameCount;
    c.frameBlend = _e237.frameBlend;
    c.framePad0_ = _e237.framePad0_;
    c.framePad1_ = _e237.framePad1_;
    c.sizeParm.calc = _e237.sizeParm.calc;
    c.sizeParm.hasCurve = _e237.sizeParm.hasCurve;
    c.sizeParm.val0_ = _e237.sizeParm.val0_;
    c.sizeParm.val1_ = _e237.sizeParm.val1_;
    c.sizeParm.variance = _e237.sizeParm.variance;
    c.sizeParm.parmPad0_ = _e237.sizeParm.parmPad0_;
    c.sizeParm.parmPad1_ = _e237.sizeParm.parmPad1_;
    c.sizeParm.parmPad2_ = _e237.sizeParm.parmPad2_;
    c.sizeParm.samples[0i] = _e237.sizeParm.samples[0];
    c.sizeParm.samples[1i] = _e237.sizeParm.samples[1];
    c.sizeParm.samples[2i] = _e237.sizeParm.samples[2];
    c.sizeParm.samples[3i] = _e237.sizeParm.samples[3];
    c.sizeParm.samples[4i] = _e237.sizeParm.samples[4];
    c.sizeParm.samples[5i] = _e237.sizeParm.samples[5];
    c.sizeParm.samples[6i] = _e237.sizeParm.samples[6];
    c.sizeParm.samples[7i] = _e237.sizeParm.samples[7];
    c.alphaParm.calc = _e237.alphaParm.calc;
    c.alphaParm.hasCurve = _e237.alphaParm.hasCurve;
    c.alphaParm.val0_ = _e237.alphaParm.val0_;
    c.alphaParm.val1_ = _e237.alphaParm.val1_;
    c.alphaParm.variance = _e237.alphaParm.variance;
    c.alphaParm.parmPad0_ = _e237.alphaParm.parmPad0_;
    c.alphaParm.parmPad1_ = _e237.alphaParm.parmPad1_;
    c.alphaParm.parmPad2_ = _e237.alphaParm.parmPad2_;
    c.alphaParm.samples[0i] = _e237.alphaParm.samples[0];
    c.alphaParm.samples[1i] = _e237.alphaParm.samples[1];
    c.alphaParm.samples[2i] = _e237.alphaParm.samples[2];
    c.alphaParm.samples[3i] = _e237.alphaParm.samples[3];
    c.alphaParm.samples[4i] = _e237.alphaParm.samples[4];
    c.alphaParm.samples[5i] = _e237.alphaParm.samples[5];
    c.alphaParm.samples[6i] = _e237.alphaParm.samples[6];
    c.alphaParm.samples[7i] = _e237.alphaParm.samples[7];
    c.dragParm.calc = _e237.dragParm.calc;
    c.dragParm.hasCurve = _e237.dragParm.hasCurve;
    c.dragParm.val0_ = _e237.dragParm.val0_;
    c.dragParm.val1_ = _e237.dragParm.val1_;
    c.dragParm.variance = _e237.dragParm.variance;
    c.dragParm.parmPad0_ = _e237.dragParm.parmPad0_;
    c.dragParm.parmPad1_ = _e237.dragParm.parmPad1_;
    c.dragParm.parmPad2_ = _e237.dragParm.parmPad2_;
    c.dragParm.samples[0i] = _e237.dragParm.samples[0];
    c.dragParm.samples[1i] = _e237.dragParm.samples[1];
    c.dragParm.samples[2i] = _e237.dragParm.samples[2];
    c.dragParm.samples[3i] = _e237.dragParm.samples[3];
    c.dragParm.samples[4i] = _e237.dragParm.samples[4];
    c.dragParm.samples[5i] = _e237.dragParm.samples[5];
    c.dragParm.samples[6i] = _e237.dragParm.samples[6];
    c.dragParm.samples[7i] = _e237.dragParm.samples[7];
    c.gravityParm.calc = _e237.gravityParm.calc;
    c.gravityParm.hasCurve = _e237.gravityParm.hasCurve;
    c.gravityParm.val0_ = _e237.gravityParm.val0_;
    c.gravityParm.val1_ = _e237.gravityParm.val1_;
    c.gravityParm.variance = _e237.gravityParm.variance;
    c.gravityParm.parmPad0_ = _e237.gravityParm.parmPad0_;
    c.gravityParm.parmPad1_ = _e237.gravityParm.parmPad1_;
    c.gravityParm.parmPad2_ = _e237.gravityParm.parmPad2_;
    c.gravityParm.samples[0i] = _e237.gravityParm.samples[0];
    c.gravityParm.samples[1i] = _e237.gravityParm.samples[1];
    c.gravityParm.samples[2i] = _e237.gravityParm.samples[2];
    c.gravityParm.samples[3i] = _e237.gravityParm.samples[3];
    c.gravityParm.samples[4i] = _e237.gravityParm.samples[4];
    c.gravityParm.samples[5i] = _e237.gravityParm.samples[5];
    c.gravityParm.samples[6i] = _e237.gravityParm.samples[6];
    c.gravityParm.samples[7i] = _e237.gravityParm.samples[7];
    let _e513 = (*request).axis;
    param_15 = _e513.xyz;
    let _e515 = spawnAxis_u0028_vf3_u003b((&param_15));
    axis_1 = _e515;
    let _e517 = (*request).origin;
    base_1 = _e517.xyz;
    let _e520 = c.emitMode;
    if (_e520 != 0u) {
        let _e522 = (*ordinal_2);
        let _e526 = (*request).count;
        pathFraction = ((f32(_e522) + 0.5f) / f32(_e526));
        let _e530 = (*request).origin;
        let _e533 = (*request).end;
        let _e535 = pathFraction;
        base_1 = mix(_e530.xyz, _e533.xyz, vec3(_e535));
    }
    scatter = vec3<f32>(0f, 0f, 0f);
    let _e539 = c.scatterShape;
    if (_e539 == 1u) {
        let _e542 = (*request).seed;
        param_16 = _e542;
        let _e543 = (*ordinal_2);
        param_17 = _e543;
        param_18 = 1u;
        let _e544 = spawnSigned_u0028_u1_u003b_u1_u003b_u1_u003b((&param_16), (&param_17), (&param_18));
        let _e546 = (*request).seed;
        param_19 = _e546;
        let _e547 = (*ordinal_2);
        param_20 = _e547;
        param_21 = 2u;
        let _e548 = spawnSigned_u0028_u1_u003b_u1_u003b_u1_u003b((&param_19), (&param_20), (&param_21));
        let _e550 = (*request).seed;
        param_22 = _e550;
        let _e551 = (*ordinal_2);
        param_23 = _e551;
        param_24 = 3u;
        let _e552 = spawnSigned_u0028_u1_u003b_u1_u003b_u1_u003b((&param_22), (&param_23), (&param_24));
        let _e555 = c.scatterMagnitude;
        scatter = (vec3<f32>(_e544, _e548, _e552) * _e555);
    } else {
        let _e558 = c.scatterShape;
        if (_e558 == 2u) {
            let _e561 = (*request).seed;
            param_25 = _e561;
            let _e562 = (*ordinal_2);
            param_26 = _e562;
            param_27 = 4u;
            let _e563 = spawnSigned_u0028_u1_u003b_u1_u003b_u1_u003b((&param_25), (&param_26), (&param_27));
            z = _e563;
            let _e565 = (*request).seed;
            param_28 = _e565;
            let _e566 = (*ordinal_2);
            param_29 = _e566;
            param_30 = 5u;
            let _e567 = spawn01_u0028_u1_u003b_u1_u003b_u1_u003b((&param_28), (&param_29), (&param_30));
            phi = (_e567 * 6.2831855f);
            let _e569 = z;
            let _e570 = z;
            radial = sqrt(max(0f, (1f - (_e569 * _e570))));
            let _e576 = (*request).seed;
            param_31 = _e576;
            let _e577 = (*ordinal_2);
            param_32 = _e577;
            param_33 = 6u;
            let _e578 = spawn01_u0028_u1_u003b_u1_u003b_u1_u003b((&param_31), (&param_32), (&param_33));
            let _e581 = c.scatterMagnitude;
            radius = (pow(_e578, 0.33333334f) * _e581);
            let _e583 = radial;
            let _e584 = phi;
            let _e587 = radial;
            let _e588 = phi;
            let _e591 = z;
            let _e593 = radius;
            scatter = (vec3<f32>((_e583 * cos(_e584)), (_e587 * sin(_e588)), _e591) * _e593);
        } else {
            let _e596 = c.scatterShape;
            if (_e596 == 3u) {
                let _e599 = axis_1[2u];
                if (abs(_e599) < 0.999f) {
                    let _e602 = axis_1;
                    local_6 = cross(_e602, vec3<f32>(0f, 0f, 1f));
                } else {
                    let _e604 = axis_1;
                    local_6 = cross(_e604, vec3<f32>(0f, 1f, 0f));
                }
                let _e606 = local_6;
                tangent = normalize(_e606);
                let _e608 = axis_1;
                let _e609 = tangent;
                bitangent = cross(_e608, _e609);
                let _e612 = (*request).seed;
                param_34 = _e612;
                let _e613 = (*ordinal_2);
                param_35 = _e613;
                param_36 = 7u;
                let _e614 = spawn01_u0028_u1_u003b_u1_u003b_u1_u003b((&param_34), (&param_35), (&param_36));
                let _e617 = c.scatterMagnitude;
                radius_1 = (sqrt(_e614) * _e617);
                let _e620 = (*request).seed;
                param_37 = _e620;
                let _e621 = (*ordinal_2);
                param_38 = _e621;
                param_39 = 8u;
                let _e622 = spawn01_u0028_u1_u003b_u1_u003b_u1_u003b((&param_37), (&param_38), (&param_39));
                phi_1 = (_e622 * 6.2831855f);
                let _e624 = tangent;
                let _e625 = phi_1;
                let _e628 = bitangent;
                let _e629 = phi_1;
                let _e633 = radius_1;
                scatter = (((_e624 * cos(_e625)) + (_e628 * sin(_e629))) * _e633);
            }
        }
    }
    let _e636 = c.axialSpeed;
    let _e638 = (*request).seed;
    param_40 = _e638;
    let _e639 = (*ordinal_2);
    param_41 = _e639;
    param_42 = 9u;
    let _e640 = spawnSigned_u0028_u1_u003b_u1_u003b_u1_u003b((&param_40), (&param_41), (&param_42));
    let _e642 = c.speedJitter;
    speed = (_e636 + (_e640 * _e642));
    let _e646 = c.velocityShape;
    if (_e646 == 0u) {
        let _e648 = axis_1;
        let _e649 = speed;
        velocity = (_e648 * _e649);
    } else {
        let _e652 = c.velocityShape;
        if (_e652 == 1u) {
            let _e654 = axis_1;
            let _e655 = speed;
            let _e658 = (*request).seed;
            param_43 = _e658;
            let _e659 = (*ordinal_2);
            param_44 = _e659;
            param_45 = 10u;
            let _e660 = spawnSigned_u0028_u1_u003b_u1_u003b_u1_u003b((&param_43), (&param_44), (&param_45));
            let _e662 = (*request).seed;
            param_46 = _e662;
            let _e663 = (*ordinal_2);
            param_47 = _e663;
            param_48 = 11u;
            let _e664 = spawnSigned_u0028_u1_u003b_u1_u003b_u1_u003b((&param_46), (&param_47), (&param_48));
            let _e666 = (*request).seed;
            param_49 = _e666;
            let _e667 = (*ordinal_2);
            param_50 = _e667;
            param_51 = 12u;
            let _e668 = spawnSigned_u0028_u1_u003b_u1_u003b_u1_u003b((&param_49), (&param_50), (&param_51));
            let _e671 = c.cubeJitter;
            velocity = ((_e654 * _e655) + (vec3<f32>(_e660, _e664, _e668) * _e671));
        } else {
            let _e675 = c.velocityShape;
            if (_e675 == 2u) {
                let _e678 = axis_1[2u];
                if (abs(_e678) < 0.999f) {
                    let _e681 = axis_1;
                    local_7 = cross(_e681, vec3<f32>(0f, 0f, 1f));
                } else {
                    let _e683 = axis_1;
                    local_7 = cross(_e683, vec3<f32>(0f, 1f, 0f));
                }
                let _e685 = local_7;
                tangent_1 = normalize(_e685);
                let _e687 = axis_1;
                let _e688 = tangent_1;
                bitangent_1 = cross(_e687, _e688);
                let _e691 = c.coneHalfAngle;
                cosHalf = cos(_e691);
                let _e693 = cosHalf;
                let _e695 = (*request).seed;
                param_52 = _e695;
                let _e696 = (*ordinal_2);
                param_53 = _e696;
                param_54 = 13u;
                let _e697 = spawn01_u0028_u1_u003b_u1_u003b_u1_u003b((&param_52), (&param_53), (&param_54));
                z_1 = mix(_e693, 1f, _e697);
                let _e699 = z_1;
                let _e700 = z_1;
                radial_1 = sqrt(max(0f, (1f - (_e699 * _e700))));
                let _e706 = (*request).seed;
                param_55 = _e706;
                let _e707 = (*ordinal_2);
                param_56 = _e707;
                param_57 = 14u;
                let _e708 = spawn01_u0028_u1_u003b_u1_u003b_u1_u003b((&param_55), (&param_56), (&param_57));
                phi_2 = (_e708 * 6.2831855f);
                let _e710 = axis_1;
                let _e711 = z_1;
                let _e713 = tangent_1;
                let _e714 = radial_1;
                let _e715 = phi_2;
                let _e720 = bitangent_1;
                let _e721 = radial_1;
                let _e722 = phi_2;
                let _e727 = speed;
                velocity = ((((_e710 * _e711) + (_e713 * (_e714 * cos(_e715)))) + (_e720 * (_e721 * sin(_e722)))) * _e727);
            } else {
                let _e730 = (*request).seed;
                param_58 = _e730;
                let _e731 = (*ordinal_2);
                param_59 = _e731;
                param_60 = 15u;
                let _e732 = spawnSigned_u0028_u1_u003b_u1_u003b_u1_u003b((&param_58), (&param_59), (&param_60));
                let _e734 = (*request).seed;
                param_61 = _e734;
                let _e735 = (*ordinal_2);
                param_62 = _e735;
                param_63 = 16u;
                let _e736 = spawnSigned_u0028_u1_u003b_u1_u003b_u1_u003b((&param_61), (&param_62), (&param_63));
                let _e738 = (*request).seed;
                param_64 = _e738;
                let _e739 = (*ordinal_2);
                param_65 = _e739;
                param_66 = 17u;
                let _e740 = spawnSigned_u0028_u1_u003b_u1_u003b_u1_u003b((&param_64), (&param_65), (&param_66));
                let _e743 = c.cubeJitter;
                velocity = (vec3<f32>(_e732, _e736, _e740) * _e743);
            }
        }
    }
    let _e746 = c.velocityBias;
    let _e749 = (*request).seed;
    param_67 = _e749;
    let _e750 = (*ordinal_2);
    param_68 = _e750;
    param_69 = 18u;
    let _e751 = spawnSigned_u0028_u1_u003b_u1_u003b_u1_u003b((&param_67), (&param_68), (&param_69));
    let _e753 = (*request).seed;
    param_70 = _e753;
    let _e754 = (*ordinal_2);
    param_71 = _e754;
    param_72 = 19u;
    let _e755 = spawnSigned_u0028_u1_u003b_u1_u003b_u1_u003b((&param_70), (&param_71), (&param_72));
    let _e757 = (*request).seed;
    param_73 = _e757;
    let _e758 = (*ordinal_2);
    param_74 = _e758;
    param_75 = 20u;
    let _e759 = spawnSigned_u0028_u1_u003b_u1_u003b_u1_u003b((&param_73), (&param_74), (&param_75));
    let _e762 = c.velocityBiasJitter;
    let _e766 = velocity;
    velocity = (_e766 + (_e746.xyz + (vec3<f32>(_e751, _e755, _e759) * _e762.xyz)));
    let _e769 = c.lifetimeMean;
    let _e771 = (*request).seed;
    param_76 = _e771;
    let _e772 = (*ordinal_2);
    param_77 = _e772;
    param_78 = 21u;
    let _e773 = spawnSigned_u0028_u1_u003b_u1_u003b_u1_u003b((&param_76), (&param_77), (&param_78));
    let _e775 = c.lifetimeJitter;
    lifetime_1 = max(0.001f, (_e769 + (_e773 * _e775)));
    let _e779 = deadParticle_u0028_();
    p_8 = _e779;
    let _e780 = base_1;
    let _e781 = scatter;
    p_8.pos = (_e780 + _e781);
    let _e784 = velocity;
    p_8.vel = _e784;
    let _e786 = lifetime_1;
    p_8.lifetimeInv = (1f / _e786);
    let _e790 = (*request).classHandle;
    p_8.classHandle = _e790;
    let _e793 = c.paletteCount;
    if (_e793 > 1i) {
        let _e796 = (*request).seed;
        let _e797 = (*ordinal_2);
        param_79 = ((_e796 ^ _e797) ^ 2654435769u);
        let _e800 = spawnHash_u0028_u1_u003b((&param_79));
        let _e802 = c.paletteCount;
        local_8 = (_e800 % bitcast<u32>(_e802));
    } else {
        local_8 = 0u;
    }
    let _e805 = local_8;
    p_8.paletteIndex = _e805;
    let _e808 = (*request).seed;
    param_80 = _e808;
    let _e809 = (*ordinal_2);
    param_81 = _e809;
    param_82 = 22u;
    let _e810 = spawnSigned_u0028_u1_u003b_u1_u003b_u1_u003b((&param_80), (&param_81), (&param_82));
    let _e812 = c.sizeJitter;
    p_8.sizeJitterPick = (_e810 * _e812);
    let _e816 = (*request).profileHandle;
    p_8.pad1_ = _e816;
    let _e819 = (*request).stageIndex;
    p_8.pad2_ = _e819;
    let _e822 = (*request).seed;
    let _e823 = (*ordinal_2);
    p_8.pad3_ = (_e822 ^ _e823);
    let _e827 = (*request).stageFlags;
    p_8.pad4_ = _e827;
    let _e830 = (*request).colorTint;
    p_8.pad5_ = pack4x8unorm(clamp(_e830, vec4<f32>(0f, 0f, 0f, 0f), vec4<f32>(1f, 1f, 1f, 1f)));
    let _e834 = p_8;
    return _e834;
}

fn main_1() {
    var eventIndex_1: u32;
    var event_1: ParticleChildEvent;
    var stage_3: AtmosphereEffectStage;
    var requested: u32;
    var cursor: u32;
    var local_9: u32;
    var ordinal_3: u32;
    var request_1: ParticleSpawnRequest;
    var child: Particle;
    var param_83: ParticleSpawnRequest;
    var param_84: u32;
    var requestIndex: u32;
    var ordinal_4: u32;
    var request_2: ParticleSpawnRequest;
    var param_85: ParticleSpawnRequest;
    var param_86: u32;
    var idx: u32;
    var p_9: Particle;
    var c_1: ParticleClassGPU;
    var gScale: f32;
    var param_87: ParticleParm;
    var local_10: f32;
    var param_88: ParticleParm;
    var param_89: f32;
    var param_90: f32;
    var dragV: f32;
    var param_91: ParticleParm;
    var local_11: f32;
    var param_92: ParticleParm;
    var param_93: f32;
    var param_94: f32;
    var previousAge_2: f32;
    var died_2: bool;
    var collided_2: bool;
    var param_95: Particle;
    var param_96: Particle;
    var param_97: Particle;
    var param_98: f32;
    var param_99: f32;
    var param_100: bool;
    var param_101: bool;
    var phi_1902_: bool;
    var phi_1909_: bool;
    var phi_1921_: bool;
    var phi_2258_: bool;
    var phi_2267_: bool;
    var phi_2676_: bool;

    if override_type_6_ {
        let _e154 = gl_GlobalInvocationID_1[0u];
        if (_e154 == 0u) {
            let _e157 = atomicLoad((&unnamed_3.eventCount));
            let _e159 = unnamed_3.eventCapacity;
            unnamed_3.dispatchX = min(_e157, _e159);
            unnamed_3.dispatchY = 1u;
            unnamed_3.dispatchZ = 1u;
        }
        return;
    }
    if override_type_6_1 {
        let _e165 = gl_WorkGroupID_1[0u];
        eventIndex_1 = _e165;
        let _e166 = eventIndex_1;
        let _e168 = atomicLoad((&unnamed_3.eventCount));
        let _e170 = unnamed_3.eventCapacity;
        if (_e166 >= min(_e168, _e170)) {
            return;
        }
        let _e173 = eventIndex_1;
        let _e176 = unnamed_3.childEvents[_e173];
        event_1.origin = _e176.origin;
        event_1.velocity = _e176.velocity;
        event_1.colorTint = _e176.colorTint;
        event_1.profileHandle = _e176.profileHandle;
        event_1.stageIndex = _e176.stageIndex;
        event_1.count = _e176.count;
        event_1.seed = _e176.seed;
        let _e192 = event_1.profileHandle;
        let _e193 = (_e192 == 0u);
        phi_1902_ = _e193;
        if !(_e193) {
            let _e196 = event_1.profileHandle;
            phi_1902_ = (_e196 > 64u);
        }
        let _e199 = phi_1902_;
        phi_1909_ = _e199;
        if !(_e199) {
            let _e202 = event_1.stageIndex;
            phi_1909_ = (_e202 >= 8u);
        }
        let _e205 = phi_1909_;
        phi_1921_ = _e205;
        if !(_e205) {
            let _e208 = event_1.stageIndex;
            let _e210 = event_1.profileHandle;
            let _e215 = unnamed_2.atmosphereProfiles[(_e210 - 1u)].stageCount;
            phi_1921_ = (_e208 >= _e215);
        }
        let _e218 = phi_1921_;
        if _e218 {
            return;
        }
        let _e220 = event_1.profileHandle;
        let _e223 = event_1.stageIndex;
        let _e228 = unnamed_2.atmosphereProfiles[(_e220 - 1u)].stages[_e223];
        stage_3.trigger = _e228.trigger;
        stage_3.particleClass = _e228.particleClass;
        stage_3.parentStage = _e228.parentStage;
        stage_3.flags = _e228.flags;
        stage_3.maxParticles = _e228.maxParticles;
        stage_3.burstCount = _e228.burstCount;
        stage_3.spawnRate = _e228.spawnRate;
        stage_3.delay = _e228.delay;
        stage_3.duration = _e228.duration;
        stage_3.lodNear = _e228.lodNear;
        stage_3.lodFar = _e228.lodFar;
        stage_3.boundsRadius = _e228.boundsRadius;
        stage_3.intensityScale = _e228.intensityScale;
        stage_3.reserved0_ = _e228.reserved0_;
        stage_3.reserved1_ = _e228.reserved1_;
        stage_3.reserved2_ = _e228.reserved2_;
        let _e262 = gl_LocalInvocationID_1[0u];
        if (_e262 == 0u) {
            let _e265 = event_1.count;
            requested = min(_e265, 64u);
            let _e268 = requested;
            let _e269 = atomicAdd((&unnamed_3.particleCursor), _e268);
            cursor = _e269;
            let _e271 = unnamed_3.eventBaseSlot;
            let _e272 = cursor;
            let _e275 = unnamed.poolSize;
            childFirstSlot = ((_e271 + _e272) % max(_e275, 1u));
            let _e278 = cursor;
            let _e280 = unnamed_3.particleBudget;
            if (_e278 < _e280) {
                let _e282 = requested;
                let _e284 = unnamed_3.particleBudget;
                let _e285 = cursor;
                local_9 = min(_e282, (_e284 - _e285));
            } else {
                local_9 = 0u;
            }
            let _e288 = local_9;
            childAcceptedCount = _e288;
            let _e289 = childAcceptedCount;
            let _e290 = requested;
            if (_e289 < _e290) {
                let _e293 = requested;
                let _e294 = childAcceptedCount;
                let _e296 = atomicAdd((&unnamed_3.droppedParticles), (_e293 - _e294));
            }
        }
        workgroupBarrier();
        let _e298 = gl_LocalInvocationID_1[0u];
        ordinal_3 = _e298;
        let _e299 = ordinal_3;
        let _e300 = childAcceptedCount;
        if (_e299 < _e300) {
            let _e303 = event_1.origin;
            request_1.origin = _e303;
            let _e306 = event_1.velocity;
            request_1.axis = _e306;
            let _e309 = event_1.origin;
            request_1.end = _e309;
            let _e312 = event_1.colorTint;
            request_1.colorTint = _e312;
            let _e315 = stage_3.particleClass;
            request_1.classHandle = _e315;
            let _e317 = childAcceptedCount;
            request_1.count = _e317;
            let _e319 = childFirstSlot;
            request_1.firstSlot = _e319;
            let _e322 = event_1.seed;
            request_1.seed = _e322;
            let _e325 = event_1.profileHandle;
            request_1.profileHandle = _e325;
            let _e328 = event_1.stageIndex;
            request_1.stageIndex = _e328;
            let _e331 = stage_3.flags;
            request_1.stageFlags = _e331;
            request_1.reserved = 0u;
            let _e334 = request_1;
            param_83 = _e334;
            let _e335 = ordinal_3;
            param_84 = _e335;
            let _e336 = buildSpawnParticle_u0028_struct_u002d_ParticleSpawnRequest_u002d_vf4_u002d_vf4_u002d_vf4_u002d_vf4_u002d_u1_u002d_u1_u002d_u1_u002d_u1_u002d_u1_u002d_u1_u002d_u1_u002d_u11_u003b_u1_u003b((&param_83), (&param_84));
            child = _e336;
            let _e338 = stage_3.flags;
            if ((_e338 & 2u) != 0u) {
                let _e342 = event_1.velocity;
                let _e345 = child.vel;
                child.vel = (_e345 + _e342.xyz);
            }
            let _e348 = childFirstSlot;
            let _e349 = ordinal_3;
            let _e352 = unnamed.poolSize;
            let _e354 = child;
            unnamed_4.writeParticles[((_e348 + _e349) % _e352)].pos = _e354.pos;
            unnamed_4.writeParticles[((_e348 + _e349) % _e352)].age = _e354.age;
            unnamed_4.writeParticles[((_e348 + _e349) % _e352)].vel = _e354.vel;
            unnamed_4.writeParticles[((_e348 + _e349) % _e352)].lifetimeInv = _e354.lifetimeInv;
            unnamed_4.writeParticles[((_e348 + _e349) % _e352)].classHandle = _e354.classHandle;
            unnamed_4.writeParticles[((_e348 + _e349) % _e352)].paletteIndex = _e354.paletteIndex;
            unnamed_4.writeParticles[((_e348 + _e349) % _e352)].sizeJitterPick = _e354.sizeJitterPick;
            unnamed_4.writeParticles[((_e348 + _e349) % _e352)].pad1_ = _e354.pad1_;
            unnamed_4.writeParticles[((_e348 + _e349) % _e352)].pad2_ = _e354.pad2_;
            unnamed_4.writeParticles[((_e348 + _e349) % _e352)].pad3_ = _e354.pad3_;
            unnamed_4.writeParticles[((_e348 + _e349) % _e352)].pad4_ = _e354.pad4_;
            unnamed_4.writeParticles[((_e348 + _e349) % _e352)].pad5_ = _e354.pad5_;
        }
        return;
    }
    if override_type_6_2 {
        let _e382 = gl_WorkGroupID_1[0u];
        requestIndex = _e382;
        let _e384 = gl_WorkGroupID_1[1u];
        let _e387 = gl_LocalInvocationID_1[0u];
        ordinal_4 = ((_e384 * 64u) + _e387);
        let _e389 = requestIndex;
        let _e392 = unnamed_5.spawnRequests[_e389];
        request_2.origin = _e392.origin;
        request_2.axis = _e392.axis;
        request_2.end = _e392.end;
        request_2.colorTint = _e392.colorTint;
        request_2.classHandle = _e392.classHandle;
        request_2.count = _e392.count;
        request_2.firstSlot = _e392.firstSlot;
        request_2.seed = _e392.seed;
        request_2.profileHandle = _e392.profileHandle;
        request_2.stageIndex = _e392.stageIndex;
        request_2.stageFlags = _e392.stageFlags;
        request_2.reserved = _e392.reserved;
        let _e417 = ordinal_4;
        let _e419 = request_2.count;
        if (_e417 < _e419) {
            let _e422 = request_2.firstSlot;
            let _e423 = ordinal_4;
            let _e426 = unnamed.poolSize;
            let _e428 = request_2;
            param_85 = _e428;
            let _e429 = ordinal_4;
            param_86 = _e429;
            let _e430 = buildSpawnParticle_u0028_struct_u002d_ParticleSpawnRequest_u002d_vf4_u002d_vf4_u002d_vf4_u002d_vf4_u002d_u1_u002d_u1_u002d_u1_u002d_u1_u002d_u1_u002d_u1_u002d_u1_u002d_u11_u003b_u1_u003b((&param_85), (&param_86));
            unnamed_6.readParticles[((_e422 + _e423) % _e426)].pos = _e430.pos;
            unnamed_6.readParticles[((_e422 + _e423) % _e426)].age = _e430.age;
            unnamed_6.readParticles[((_e422 + _e423) % _e426)].vel = _e430.vel;
            unnamed_6.readParticles[((_e422 + _e423) % _e426)].lifetimeInv = _e430.lifetimeInv;
            unnamed_6.readParticles[((_e422 + _e423) % _e426)].classHandle = _e430.classHandle;
            unnamed_6.readParticles[((_e422 + _e423) % _e426)].paletteIndex = _e430.paletteIndex;
            unnamed_6.readParticles[((_e422 + _e423) % _e426)].sizeJitterPick = _e430.sizeJitterPick;
            unnamed_6.readParticles[((_e422 + _e423) % _e426)].pad1_ = _e430.pad1_;
            unnamed_6.readParticles[((_e422 + _e423) % _e426)].pad2_ = _e430.pad2_;
            unnamed_6.readParticles[((_e422 + _e423) % _e426)].pad3_ = _e430.pad3_;
            unnamed_6.readParticles[((_e422 + _e423) % _e426)].pad4_ = _e430.pad4_;
            unnamed_6.readParticles[((_e422 + _e423) % _e426)].pad5_ = _e430.pad5_;
        }
        return;
    }
    let _e458 = gl_GlobalInvocationID_1[0u];
    idx = _e458;
    let _e459 = idx;
    let _e461 = unnamed.poolSize;
    if (_e459 >= _e461) {
        return;
    }
    let _e463 = idx;
    let _e466 = unnamed_6.readParticles[_e463];
    p_9.pos = _e466.pos;
    p_9.age = _e466.age;
    p_9.vel = _e466.vel;
    p_9.lifetimeInv = _e466.lifetimeInv;
    p_9.classHandle = _e466.classHandle;
    p_9.paletteIndex = _e466.paletteIndex;
    p_9.sizeJitterPick = _e466.sizeJitterPick;
    p_9.pad1_ = _e466.pad1_;
    p_9.pad2_ = _e466.pad2_;
    p_9.pad3_ = _e466.pad3_;
    p_9.pad4_ = _e466.pad4_;
    p_9.pad5_ = _e466.pad5_;
    let _e492 = p_9.classHandle;
    let _e493 = (_e492 == 0u);
    phi_2258_ = _e493;
    if !(_e493) {
        let _e496 = p_9.age;
        phi_2258_ = (_e496 >= 1f);
    }
    let _e499 = phi_2258_;
    phi_2267_ = _e499;
    if !(_e499) {
        let _e502 = p_9.classHandle;
        let _e504 = unnamed.numClasses;
        phi_2267_ = (_e502 > _e504);
    }
    let _e507 = phi_2267_;
    if _e507 {
        let _e508 = idx;
        let _e509 = deadParticle_u0028_();
        unnamed_4.writeParticles[_e508].pos = _e509.pos;
        unnamed_4.writeParticles[_e508].age = _e509.age;
        unnamed_4.writeParticles[_e508].vel = _e509.vel;
        unnamed_4.writeParticles[_e508].lifetimeInv = _e509.lifetimeInv;
        unnamed_4.writeParticles[_e508].classHandle = _e509.classHandle;
        unnamed_4.writeParticles[_e508].paletteIndex = _e509.paletteIndex;
        unnamed_4.writeParticles[_e508].sizeJitterPick = _e509.sizeJitterPick;
        unnamed_4.writeParticles[_e508].pad1_ = _e509.pad1_;
        unnamed_4.writeParticles[_e508].pad2_ = _e509.pad2_;
        unnamed_4.writeParticles[_e508].pad3_ = _e509.pad3_;
        unnamed_4.writeParticles[_e508].pad4_ = _e509.pad4_;
        unnamed_4.writeParticles[_e508].pad5_ = _e509.pad5_;
        return;
    }
    let _e537 = p_9.classHandle;
    let _e541 = unnamed_1.classes[(_e537 - 1u)];
    c_1.shader = _e541.shader;
    c_1.renderFlags = _e541.renderFlags;
    c_1.emitMode = _e541.emitMode;
    c_1.scatterShape = _e541.scatterShape;
    c_1.scatterMagnitude = _e541.scatterMagnitude;
    c_1.velocityShape = _e541.velocityShape;
    c_1.axialSpeed = _e541.axialSpeed;
    c_1.cubeJitter = _e541.cubeJitter;
    c_1.coneHalfAngle = _e541.coneHalfAngle;
    c_1.lifetimeMean = _e541.lifetimeMean;
    c_1.lifetimeJitter = _e541.lifetimeJitter;
    c_1.paletteCount = _e541.paletteCount;
    c_1.colorPalette[0i] = _e541.colorPalette[0];
    c_1.colorPalette[1i] = _e541.colorPalette[1];
    c_1.colorPalette[2i] = _e541.colorPalette[2];
    c_1.colorPalette[3i] = _e541.colorPalette[3];
    c_1.colorPalette[4i] = _e541.colorPalette[4];
    c_1.colorPalette[5i] = _e541.colorPalette[5];
    c_1.colorPalette[6i] = _e541.colorPalette[6];
    c_1.colorPalette[7i] = _e541.colorPalette[7];
    c_1.colorPalette[8i] = _e541.colorPalette[8];
    c_1.colorPalette[9i] = _e541.colorPalette[9];
    c_1.colorPalette[10i] = _e541.colorPalette[10];
    c_1.colorPalette[11i] = _e541.colorPalette[11];
    c_1.colorPalette[12i] = _e541.colorPalette[12];
    c_1.colorPalette[13i] = _e541.colorPalette[13];
    c_1.colorPalette[14i] = _e541.colorPalette[14];
    c_1.colorPalette[15i] = _e541.colorPalette[15];
    c_1.colorEndMult = _e541.colorEndMult;
    c_1.sizeStart = _e541.sizeStart;
    c_1.sizeEnd = _e541.sizeEnd;
    c_1.gravityScale = _e541.gravityScale;
    c_1.drag = _e541.drag;
    c_1.shaderBlendIsAdditive = _e541.shaderBlendIsAdditive;
    c_1.pad1_ = _e541.pad1_;
    c_1.pad2_ = _e541.pad2_;
    c_1.pad3_ = _e541.pad3_;
    c_1.velocityBias = _e541.velocityBias;
    c_1.velocityBiasJitter = _e541.velocityBiasJitter;
    c_1.speedJitter = _e541.speedJitter;
    c_1.sizeJitter = _e541.sizeJitter;
    c_1.colorDomain = _e541.colorDomain;
    c_1.pad5_ = _e541.pad5_;
    c_1.frameSlots[0i] = _e541.frameSlots[0];
    c_1.frameSlots[1i] = _e541.frameSlots[1];
    c_1.frameSlots[2i] = _e541.frameSlots[2];
    c_1.frameSlots[3i] = _e541.frameSlots[3];
    c_1.frameSlots[4i] = _e541.frameSlots[4];
    c_1.frameSlots[5i] = _e541.frameSlots[5];
    c_1.frameSlots[6i] = _e541.frameSlots[6];
    c_1.frameSlots[7i] = _e541.frameSlots[7];
    c_1.frameSlots[8i] = _e541.frameSlots[8];
    c_1.frameSlots[9i] = _e541.frameSlots[9];
    c_1.frameSlots[10i] = _e541.frameSlots[10];
    c_1.frameSlots[11i] = _e541.frameSlots[11];
    c_1.frameSlots[12i] = _e541.frameSlots[12];
    c_1.frameSlots[13i] = _e541.frameSlots[13];
    c_1.frameSlots[14i] = _e541.frameSlots[14];
    c_1.frameSlots[15i] = _e541.frameSlots[15];
    c_1.frameCount = _e541.frameCount;
    c_1.frameBlend = _e541.frameBlend;
    c_1.framePad0_ = _e541.framePad0_;
    c_1.framePad1_ = _e541.framePad1_;
    c_1.sizeParm.calc = _e541.sizeParm.calc;
    c_1.sizeParm.hasCurve = _e541.sizeParm.hasCurve;
    c_1.sizeParm.val0_ = _e541.sizeParm.val0_;
    c_1.sizeParm.val1_ = _e541.sizeParm.val1_;
    c_1.sizeParm.variance = _e541.sizeParm.variance;
    c_1.sizeParm.parmPad0_ = _e541.sizeParm.parmPad0_;
    c_1.sizeParm.parmPad1_ = _e541.sizeParm.parmPad1_;
    c_1.sizeParm.parmPad2_ = _e541.sizeParm.parmPad2_;
    c_1.sizeParm.samples[0i] = _e541.sizeParm.samples[0];
    c_1.sizeParm.samples[1i] = _e541.sizeParm.samples[1];
    c_1.sizeParm.samples[2i] = _e541.sizeParm.samples[2];
    c_1.sizeParm.samples[3i] = _e541.sizeParm.samples[3];
    c_1.sizeParm.samples[4i] = _e541.sizeParm.samples[4];
    c_1.sizeParm.samples[5i] = _e541.sizeParm.samples[5];
    c_1.sizeParm.samples[6i] = _e541.sizeParm.samples[6];
    c_1.sizeParm.samples[7i] = _e541.sizeParm.samples[7];
    c_1.alphaParm.calc = _e541.alphaParm.calc;
    c_1.alphaParm.hasCurve = _e541.alphaParm.hasCurve;
    c_1.alphaParm.val0_ = _e541.alphaParm.val0_;
    c_1.alphaParm.val1_ = _e541.alphaParm.val1_;
    c_1.alphaParm.variance = _e541.alphaParm.variance;
    c_1.alphaParm.parmPad0_ = _e541.alphaParm.parmPad0_;
    c_1.alphaParm.parmPad1_ = _e541.alphaParm.parmPad1_;
    c_1.alphaParm.parmPad2_ = _e541.alphaParm.parmPad2_;
    c_1.alphaParm.samples[0i] = _e541.alphaParm.samples[0];
    c_1.alphaParm.samples[1i] = _e541.alphaParm.samples[1];
    c_1.alphaParm.samples[2i] = _e541.alphaParm.samples[2];
    c_1.alphaParm.samples[3i] = _e541.alphaParm.samples[3];
    c_1.alphaParm.samples[4i] = _e541.alphaParm.samples[4];
    c_1.alphaParm.samples[5i] = _e541.alphaParm.samples[5];
    c_1.alphaParm.samples[6i] = _e541.alphaParm.samples[6];
    c_1.alphaParm.samples[7i] = _e541.alphaParm.samples[7];
    c_1.dragParm.calc = _e541.dragParm.calc;
    c_1.dragParm.hasCurve = _e541.dragParm.hasCurve;
    c_1.dragParm.val0_ = _e541.dragParm.val0_;
    c_1.dragParm.val1_ = _e541.dragParm.val1_;
    c_1.dragParm.variance = _e541.dragParm.variance;
    c_1.dragParm.parmPad0_ = _e541.dragParm.parmPad0_;
    c_1.dragParm.parmPad1_ = _e541.dragParm.parmPad1_;
    c_1.dragParm.parmPad2_ = _e541.dragParm.parmPad2_;
    c_1.dragParm.samples[0i] = _e541.dragParm.samples[0];
    c_1.dragParm.samples[1i] = _e541.dragParm.samples[1];
    c_1.dragParm.samples[2i] = _e541.dragParm.samples[2];
    c_1.dragParm.samples[3i] = _e541.dragParm.samples[3];
    c_1.dragParm.samples[4i] = _e541.dragParm.samples[4];
    c_1.dragParm.samples[5i] = _e541.dragParm.samples[5];
    c_1.dragParm.samples[6i] = _e541.dragParm.samples[6];
    c_1.dragParm.samples[7i] = _e541.dragParm.samples[7];
    c_1.gravityParm.calc = _e541.gravityParm.calc;
    c_1.gravityParm.hasCurve = _e541.gravityParm.hasCurve;
    c_1.gravityParm.val0_ = _e541.gravityParm.val0_;
    c_1.gravityParm.val1_ = _e541.gravityParm.val1_;
    c_1.gravityParm.variance = _e541.gravityParm.variance;
    c_1.gravityParm.parmPad0_ = _e541.gravityParm.parmPad0_;
    c_1.gravityParm.parmPad1_ = _e541.gravityParm.parmPad1_;
    c_1.gravityParm.parmPad2_ = _e541.gravityParm.parmPad2_;
    c_1.gravityParm.samples[0i] = _e541.gravityParm.samples[0];
    c_1.gravityParm.samples[1i] = _e541.gravityParm.samples[1];
    c_1.gravityParm.samples[2i] = _e541.gravityParm.samples[2];
    c_1.gravityParm.samples[3i] = _e541.gravityParm.samples[3];
    c_1.gravityParm.samples[4i] = _e541.gravityParm.samples[4];
    c_1.gravityParm.samples[5i] = _e541.gravityParm.samples[5];
    c_1.gravityParm.samples[6i] = _e541.gravityParm.samples[6];
    c_1.gravityParm.samples[7i] = _e541.gravityParm.samples[7];
    let _e817 = c_1.gravityParm;
    param_87 = _e817;
    let _e818 = parmIsUnset_u0028_struct_u002d_ParticleParm_u002d_i1_u002d_i1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u005b_8_u005d_1_u003b((&param_87));
    if _e818 {
        let _e820 = c_1.gravityScale;
        local_10 = _e820;
    } else {
        let _e822 = c_1.gravityParm;
        param_88 = _e822;
        let _e824 = p_9.age;
        param_89 = _e824;
        let _e826 = p_9.sizeJitterPick;
        param_90 = _e826;
        let _e827 = parmEval_u0028_struct_u002d_ParticleParm_u002d_i1_u002d_i1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u005b_8_u005d_1_u003b_f1_u003b_f1_u003b((&param_88), (&param_89), (&param_90));
        local_10 = _e827;
    }
    let _e828 = local_10;
    gScale = _e828;
    let _e830 = c_1.dragParm;
    param_91 = _e830;
    let _e831 = parmIsUnset_u0028_struct_u002d_ParticleParm_u002d_i1_u002d_i1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u005b_8_u005d_1_u003b((&param_91));
    if _e831 {
        let _e833 = c_1.drag;
        local_11 = _e833;
    } else {
        let _e835 = c_1.dragParm;
        param_92 = _e835;
        let _e837 = p_9.age;
        param_93 = _e837;
        let _e839 = p_9.sizeJitterPick;
        param_94 = _e839;
        let _e840 = parmEval_u0028_struct_u002d_ParticleParm_u002d_i1_u002d_i1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u005b_8_u005d_1_u003b_f1_u003b_f1_u003b((&param_92), (&param_93), (&param_94));
        local_11 = _e840;
    }
    let _e841 = local_11;
    dragV = _e841;
    let _e843 = p_9.age;
    previousAge_2 = _e843;
    let _e844 = gScale;
    let _e847 = unnamed.dt;
    let _e851 = p_9.vel[2u];
    p_9.vel[2u] = (_e851 + ((-800f * _e844) * _e847));
    let _e855 = dragV;
    let _e858 = unnamed.dt;
    let _e862 = p_9.vel;
    p_9.vel = (_e862 * exp((-(_e855) * _e858)));
    let _e866 = p_9.vel;
    let _e868 = unnamed.dt;
    let _e871 = p_9.pos;
    p_9.pos = (_e871 + (_e866 * _e868));
    let _e875 = unnamed.dt;
    let _e877 = p_9.lifetimeInv;
    let _e880 = p_9.age;
    p_9.age = (_e880 + (_e875 * _e877));
    let _e884 = p_9.age;
    died_2 = (_e884 >= 1f);
    let _e886 = p_9;
    param_95 = _e886;
    let _e887 = particleHasCollisionChild_u0028_struct_u002d_Particle_u002d_vf3_u002d_f1_u002d_vf3_u002d_f1_u002d_u1_u002d_u1_u002d_f1_u002d_u1_u002d_u1_u002d_u1_u002d_u1_u002d_u11_u003b((&param_95));
    phi_2676_ = _e887;
    if _e887 {
        let _e888 = p_9;
        param_96 = _e888;
        let _e889 = particleCollided_u0028_struct_u002d_Particle_u002d_vf3_u002d_f1_u002d_vf3_u002d_f1_u002d_u1_u002d_u1_u002d_f1_u002d_u1_u002d_u1_u002d_u1_u002d_u1_u002d_u11_u003b((&param_96));
        phi_2676_ = _e889;
    }
    let _e891 = phi_2676_;
    collided_2 = _e891;
    let _e892 = died_2;
    let _e893 = collided_2;
    let _e895 = p_9;
    param_97 = _e895;
    let _e896 = previousAge_2;
    param_98 = _e896;
    let _e898 = p_9.age;
    param_99 = _e898;
    let _e899 = collided_2;
    param_100 = _e899;
    param_101 = (_e892 || _e893);
    appendChildEvents_u0028_struct_u002d_Particle_u002d_vf3_u002d_f1_u002d_vf3_u002d_f1_u002d_u1_u002d_u1_u002d_f1_u002d_u1_u002d_u1_u002d_u1_u002d_u1_u002d_u11_u003b_f1_u003b_f1_u003b_b1_u003b_b1_u003b((&param_97), (&param_98), (&param_99), (&param_100), (&param_101));
    let _e900 = died_2;
    let _e901 = collided_2;
    if (_e900 || _e901) {
        let _e903 = idx;
        let _e904 = deadParticle_u0028_();
        unnamed_4.writeParticles[_e903].pos = _e904.pos;
        unnamed_4.writeParticles[_e903].age = _e904.age;
        unnamed_4.writeParticles[_e903].vel = _e904.vel;
        unnamed_4.writeParticles[_e903].lifetimeInv = _e904.lifetimeInv;
        unnamed_4.writeParticles[_e903].classHandle = _e904.classHandle;
        unnamed_4.writeParticles[_e903].paletteIndex = _e904.paletteIndex;
        unnamed_4.writeParticles[_e903].sizeJitterPick = _e904.sizeJitterPick;
        unnamed_4.writeParticles[_e903].pad1_ = _e904.pad1_;
        unnamed_4.writeParticles[_e903].pad2_ = _e904.pad2_;
        unnamed_4.writeParticles[_e903].pad3_ = _e904.pad3_;
        unnamed_4.writeParticles[_e903].pad4_ = _e904.pad4_;
        unnamed_4.writeParticles[_e903].pad5_ = _e904.pad5_;
        return;
    }
    let _e931 = idx;
    let _e932 = p_9;
    unnamed_4.writeParticles[_e931].pos = _e932.pos;
    unnamed_4.writeParticles[_e931].age = _e932.age;
    unnamed_4.writeParticles[_e931].vel = _e932.vel;
    unnamed_4.writeParticles[_e931].lifetimeInv = _e932.lifetimeInv;
    unnamed_4.writeParticles[_e931].classHandle = _e932.classHandle;
    unnamed_4.writeParticles[_e931].paletteIndex = _e932.paletteIndex;
    unnamed_4.writeParticles[_e931].sizeJitterPick = _e932.sizeJitterPick;
    unnamed_4.writeParticles[_e931].pad1_ = _e932.pad1_;
    unnamed_4.writeParticles[_e931].pad2_ = _e932.pad2_;
    unnamed_4.writeParticles[_e931].pad3_ = _e932.pad3_;
    unnamed_4.writeParticles[_e931].pad4_ = _e932.pad4_;
    unnamed_4.writeParticles[_e931].pad5_ = _e932.pad5_;
    return;
}

@compute @workgroup_size(64, 1, 1)
fn main(@builtin(global_invocation_id) gl_GlobalInvocationID: vec3<u32>, @builtin(workgroup_id) gl_WorkGroupID: vec3<u32>, @builtin(local_invocation_id) gl_LocalInvocationID: vec3<u32>) {
    gl_GlobalInvocationID_1 = gl_GlobalInvocationID;
    gl_WorkGroupID_1 = gl_WorkGroupID;
    gl_LocalInvocationID_1 = gl_LocalInvocationID;
    main_1();
}
